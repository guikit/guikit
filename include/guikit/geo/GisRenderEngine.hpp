#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include "guikit/panels/MapView.hpp"
#include "guikit/geo/ChartDataSource.hpp"
#include "guikit/geo/GeoTypes.hpp"

namespace guikit::geo {

/// Marine chart color palette modes (S-52 ECDIS standard).
enum class ChartPalette : uint8_t {
    MarineDay = 0,      ///< Bright white deep water, vivid cyan/blue shallow bands, warm sand land
    MarineDusk = 1,     ///< Twilight bridge palette with muted bronze land and deep slate water
    MarineNight = 2,    ///< Dark bridge night-vision palette
    ClassicGis = 3      ///< Layer-assigned GIS primary colors
};

using GisColorPalette = ChartPalette;

[[nodiscard]] constexpr std::string_view ChartPaletteName(ChartPalette p) noexcept {
    switch (p) {
        case ChartPalette::MarineDay:   return "S-52 Marine Day (Bright)";
        case ChartPalette::MarineDusk:  return "S-52 Marine Dusk (Twilight)";
        case ChartPalette::MarineNight: return "S-52 Marine Night (Bridge Dark)";
        case ChartPalette::ClassicGis:  return "Classic Vector GIS";
    }
    return "S-52 Marine Day";
}

[[nodiscard]] constexpr std::string_view GisColorPaletteName(ChartPalette p) noexcept {
    return ChartPaletteName(p);
}

/// Per-frame rendering telemetry for the GIS viewport.
struct GisRenderStats {
    size_t visibleLayers{0};
    size_t totalFeatures{0};
    size_t renderedFeatures{0};
    size_t culledFeatures{0};
    size_t renderedVertices{0};
    size_t renderedTriangles{0};
    size_t cachedTilesHit{0};
    size_t cachedTilesRendered{0};
    size_t cachedTilesPending{0};
    size_t declutteredLabels{0};
    size_t residentFeatureCount{0};          ///< Number of paged features currently holding resident geometry/properties in RAM
    size_t residentFeatureBytes{0};          ///< Total dynamic heap bytes currently held by paged features in RAM
    size_t pagedInFeaturesThisFrame{0};      ///< Features fetched on-demand from backing store during this frame
    size_t evictedFeaturesTotal{0};          ///< Cumulative features evicted from RAM by the LRU memory manager
    uint64_t hoveredFeatureUid{0};
    uint64_t clickedFeatureUid{0};
};

/// Global rendering options for vector layers and S-57 / S-52 marine chart symbology.
struct GisRenderConfig {
    bool frustumCulling{true};               ///< Cull features via R-Tree spatial index outside visible viewport
    bool lodSimplification{true};            ///< Use multi-zoom LOD rings and skip sub-pixel segments
    float lodPixelTolerance{1.25f};          ///< Minimum screen-space pixel distance between consecutive vertices
    bool enableFeaturePicking{true};         ///< Enable interactive hover hit-testing and click selection highlight on map features
    bool showVertexDotsOnSelect{true};       ///< Render vertex control handles on the currently selected feature
    bool showFeatureCallouts{true};          ///< Render labels, soundings, and seamark callouts
    bool useVectorTileCache{true};           ///< Rasterize static polygons/lines into [z/x/y] offscreen ICanvas vector tiles
    bool scaleFiltering{true};               ///< Progressive detail disclosure by zoom level and SCAMIN scale_index
    bool showSoundings{true};                ///< Render SOUNDG depth soundings with subscript tenths
    bool showDepthContours{true};            ///< Render DEPCNT isobaths and inline contour labels
    bool showCoverageBounds{false};          ///< Render M_COVR coverage outlines
    bool preferRussianLabels{true};          ///< Prefer label_ru over label_en when both exist
    float safetyContourM{2.0f};              ///< Safety depth contour threshold in meters (default 2.0m)
    float shallowContourM{1.0f};             ///< Shallow water danger threshold in meters (default 1.0m)
    float deepContourM{10.0f};               ///< Deep water threshold in meters (default 10.0m)

    // --- Configurable Tile Engine & Asynchronous Background Streaming ---
    uint32_t tileSizePx{256};                ///< Offscreen vector/raster tile resolution in pixels (128, 256, or 512)
    float tileRenderBudgetMs{8.5f};          ///< Maximum milliseconds spent rasterizing new vector tiles per frame
    uint32_t maxCompositeTiles{640};         ///< Maximum cached Tier-2 composite screen tiles
    uint32_t maxLayerTiles{512};             ///< Maximum cached Tier-1 per-layer sparse tiles
    float featureCacheBudgetMb{512.0f};      ///< Maximum RAM budget (in MB) for resident Out-of-Core feature payloads
    bool asyncBackgroundStreaming{true};     ///< Stream spatial windows, geometries, and tiles on background worker threads
    bool showLoadingTilePlaceholders{true};  ///< Render animated cartographic placeholder overlay on tiles loading in background
    bool showCautionAndRestrictedZones{true};///< Render regulatory/caution marine polygons (CTNARE, RESARE, TSEZNE, MIPARE)

    // --- Multi-Source Raster Basemap Overlay (e.g. Bathymetry over OpenStreetMap) ---
    bool showOsmBasemap{false};              ///< Overlay vector marine charts / bathymetry on top of raster [z/x/y] tiles
    bool hideLandOverBasemap{true};          ///< Suppress opaque S-57 LNDARE fill when basemap is active so basemap land/streets show through
    float basemapOpacity{1.0f};              ///< Opacity of the underlying raster basemap [0..1]
    float bathymetryOverBasemapOpacity{0.78f}; ///< Opacity of depth area (DEPARE) fills when overlaid on a raster basemap
    std::shared_ptr<IRasterTileProvider> customBasemapProvider{}; ///< Optional custom raster tile provider (defaults to registered default or Cartographic Basemap)

    // --- Configurable Cartographic Labeling Policy ---
    bool showAreaFeatureLabels{true};        ///< Render text callouts on polygonal marine areas (SEAARE, SBDARE, etc.)
    bool hideMultiLineNotesOnMap{true};      ///< Suppress multi-sentence INFORM/TXTDSC cautionary notes (containing '|') from map surface
    uint32_t maxMapLabelChars{48};           ///< Maximum characters displayed for a map callout label (0 = unlimited)
    bool truncateLongLabels{true};           ///< Truncate labels exceeding maxMapLabelChars with "..." instead of dropping them

    int32_t activeStyleLayerIdx{-1};         ///< Layer index currently open in the Style editor (-1 = none)
    ChartPalette chartPalette{ChartPalette::MarineDay};
};

using GisRenderSettings = GisRenderConfig;

/// High-performance vector GIS & S-52 marine chart rendering engine over `guikit::MapViewScope`.
class GisRenderEngine {
public:
    using BreadcrumbCallback = void(*)(const char* stage, int z, int x, int y) noexcept;

    /// Renders all visible `GeoLayer`s into `map`, using the R-Tree spatial index, `[z/x/y]` offscreen
    /// ICanvas vector tile cache, S-52 depth shading, screen-space label decluttering, and O(log N) hit-testing.
    static GisRenderStats RenderLayers(
        MapViewScope& map,
        std::span<const GeoLayer> layers,
        const GisRenderConfig& cfg,
        uint64_t& ioSelectedFeatureUid,
        bool lightMode = true
    );

    /// Enforces the Out-of-Core LRU memory budget across `layers`, evicting the least-recently-used
    /// feature geometries and attribute dictionaries whose `lastAccessTick < protectedTick` until
    /// resident heap usage is within `maxResidentBytes`. Returns the number of evicted features.
    static size_t EnforceFeatureRamBudget(
        std::span<const GeoLayer> layers,
        size_t maxResidentBytes,
        uint64_t protectedTick = UINT64_MAX,
        size_t* outResidentFeatures = nullptr,
        size_t* outResidentBytes = nullptr
    );

    /// Explicitly clears the offscreen vector tile cache (e.g., when layers are reloaded or modified).
    static void InvalidateTileCache();

    /// Waits until all background spatial window and tile rasterization worker threads are idle (for deterministic tests).
    static bool WaitForBackgroundStreamingIdle(uint32_t timeoutMs = 5000);

    /// Computes the background water color for the active palette.
    [[nodiscard]] static Color GetWaterBackgroundColor(ChartPalette palette) noexcept;

    /// Registers or retrieves the default raster basemap tile provider used when `cfg.showOsmBasemap` is true
    /// and `cfg.customBasemapProvider` is null. If none is registered, falls back to `CreateCartographicBasemapTileProvider()`.
    static void SetDefaultBasemapProvider(std::shared_ptr<IRasterTileProvider> provider);
    [[nodiscard]] static std::shared_ptr<IRasterTileProvider> GetDefaultBasemapProvider();

    /// Registers an optional diagnostic breadcrumb callback invoked at key rendering stages.
    static void SetBreadcrumbCallback(BreadcrumbCallback cb) noexcept;
};

} // namespace guikit::geo

namespace guikit {
using namespace geo;
} // namespace guikit
