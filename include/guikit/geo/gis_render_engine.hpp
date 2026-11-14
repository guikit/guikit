#pragma once

#include "guikit/geo/chart_data_source.hpp"
#include "guikit/geo/geo_types.hpp"
#include "guikit/panels/map_view.hpp"
#include "keel/ref.h"
#include "keel/smart_ptr.h"

namespace guikit::geo {

/// Marine chart color palette modes (S-52 ECDIS standard).
enum class ChartPalette : keel::u8 {
    MarineDay = 0,      ///< Bright white deep water, vivid cyan/blue shallow bands, warm sand land
    MarineDusk = 1,     ///< Twilight bridge palette with muted bronze land and deep slate water
    MarineNight = 2,    ///< Dark bridge night-vision palette
    ClassicGis = 3      ///< Layer-assigned GIS primary colors
};

using GisColorPalette = ChartPalette;

[[nodiscard]] constexpr keel::StringRef ChartPaletteName(ChartPalette p) noexcept {
    switch (p) {
        case ChartPalette::MarineDay:   return "S-52 Marine Day (Bright)";
        case ChartPalette::MarineDusk:  return "S-52 Marine Dusk (Twilight)";
        case ChartPalette::MarineNight: return "S-52 Marine Night (Bridge Dark)";
        case ChartPalette::ClassicGis:  return "Classic Vector GIS";
    }
    return "S-52 Marine Day";
}

[[nodiscard]] constexpr keel::StringRef GisColorPaletteName(ChartPalette p) noexcept {
    return ChartPaletteName(p);
}

/// Per-frame rendering telemetry for the GIS viewport.
struct GisRenderStats {
    keel::usize visibleLayers{0};
    keel::usize totalFeatures{0};
    keel::usize renderedFeatures{0};
    keel::usize culledFeatures{0};
    keel::usize renderedVertices{0};
    keel::usize renderedTriangles{0};
    keel::usize cachedTilesHit{0};
    keel::usize cachedTilesRendered{0};
    keel::usize cachedTilesPending{0};
    keel::usize declutteredLabels{0};
    keel::usize residentFeatureCount{0};          ///< Number of paged features currently holding resident geometry/properties in RAM
    keel::usize residentFeatureBytes{0};          ///< Total dynamic heap bytes currently held by paged features in RAM
    keel::usize pagedInFeaturesThisFrame{0};      ///< Features fetched on-demand from backing store during this frame
    keel::usize evictedFeaturesTotal{0};          ///< Cumulative features evicted from RAM by the LRU memory manager
    keel::u64 hoveredFeatureUid{0};
    keel::u64 clickedFeatureUid{0};
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
    bool preferRussianLabels{true};          ///< Prefer label_ru over label_en when preferredLanguage is empty
    keel::StringRef preferredLanguage{};     ///< Optional ISO 639-1 language code for localized labels (e.g. "ru", "en", "de", "fr")
    float safetyContourM{2.0f};              ///< Safety depth contour threshold in meters (default 2.0m)
    float shallowContourM{1.0f};             ///< Shallow water danger threshold in meters (default 1.0m)
    float deepContourM{10.0f};               ///< Deep water threshold in meters (default 10.0m)

    // --- Configurable Tile Engine & Asynchronous Background Streaming ---
    keel::u32 tileSizePx{256};                ///< Offscreen vector/raster tile resolution in pixels (128, 256, or 512)
    float tileRenderBudgetMs{8.5f};          ///< Maximum milliseconds spent rasterizing new vector tiles per frame
    keel::u32 maxCompositeTiles{48};          ///< Maximum cached Tier-2 composite screen tiles
    keel::u32 maxLayerTiles{48};              ///< Maximum global cached Tier-1 pixel-bearing layer tiles
    float featureCacheBudgetMb{64.0f};       ///< Maximum RAM budget (in MB) for resident Out-of-Core feature payloads
    bool asyncBackgroundStreaming{true};     ///< Stream spatial windows, geometries, and tiles on background worker threads
    bool showLoadingTilePlaceholders{true};  ///< Render animated cartographic placeholder overlay on tiles loading in background
    bool showCautionAndRestrictedZones{true};///< Render regulatory/caution marine polygons (CTNARE, RESARE, TSEZNE, MIPARE)

    // --- Multi-Source Raster Basemap Overlay (e.g. Bathymetry over OpenStreetMap) ---
    bool showOsmBasemap{false};              ///< Overlay vector marine charts / bathymetry on top of raster [z/x/y] tiles
    bool hideLandOverBasemap{true};          ///< Suppress opaque S-57 LNDARE fill when basemap is active so basemap land/streets show through
    float basemapOpacity{1.0f};              ///< Opacity of the underlying raster basemap [0..1]
    float bathymetryOverBasemapOpacity{0.78f}; ///< Opacity of depth area (DEPARE) fills when overlaid on a raster basemap
    keel::Arc<IRasterTileProvider> customBasemapProvider{}; ///< Optional custom raster tile provider (defaults to registered default or Cartographic Basemap)

    // --- Configurable Cartographic Labeling Policy ---
    bool showAreaFeatureLabels{true};        ///< Render text callouts on polygonal marine areas (SEAARE, SBDARE, etc.)
    bool hideMultiLineNotesOnMap{true};      ///< Suppress multi-sentence INFORM/TXTDSC cautionary notes (containing '|') from map surface
    keel::u32 maxMapLabelChars{48};           ///< Maximum characters displayed for a map callout label (0 = unlimited)
    bool truncateLongLabels{true};           ///< Truncate labels exceeding maxMapLabelChars with "..." instead of dropping them

    keel::i32 activeStyleLayerIdx{-1};         ///< Layer index currently open in the Style editor (-1 = none)
    ChartPalette chartPalette{ChartPalette::MarineDay};
};

using GisRenderSettings = GisRenderConfig;

/// High-performance vector GIS & S-52 marine chart rendering engine over `guikit::MapViewScope`.
class GisRenderEngine {
public:
    using BreadcrumbCallback = void(*)(const char* stage, keel::i32 z, keel::i32 x, keel::i32 y) noexcept;

    /// Renders all visible `GeoLayer`s into `map`, using the R-Tree spatial index, `[z/x/y]` offscreen
    /// ICanvas vector tile cache, S-52 depth shading, screen-space label decluttering, and O(log N) hit-testing.
    static GisRenderStats RenderLayers(
        MapViewScope& map,
        keel::ArrayRef<GeoLayer> layers,
        const GisRenderConfig& cfg,
        keel::u64& ioSelectedFeatureUid,
        bool lightMode = true
    );

    /// Enforces the Out-of-Core LRU memory budget across `layers`, evicting the least-recently-used
    /// feature geometries and attribute dictionaries whose `lastAccessTick < protectedTick` until
    /// resident heap usage is within `maxResidentBytes`. Returns the number of evicted features.
    static keel::usize EnforceFeatureRamBudget(
        keel::ArrayRef<GeoLayer> layers,
        keel::usize maxResidentBytes,
        keel::u64 protectedTick = ~keel::u64{0},
        keel::usize* outResidentFeatures = nullptr,
        keel::usize* outResidentBytes = nullptr
    );

    /// Explicitly clears the offscreen vector tile cache (e.g., when layers are reloaded or modified).
    static void InvalidateTileCache();

    /// Releases offscreen tile, declutter, basemap, and worker-canvas caches when all MapViews become inactive.
    static void TrimInactiveCaches() noexcept;

    /// Waits until all background spatial window and tile rasterization worker threads are idle (for deterministic tests).
    static bool WaitForBackgroundStreamingIdle(keel::u32 timeoutMs = 5000);

    /// Computes the background water color for the active palette.
    [[nodiscard]] static Color GetWaterBackgroundColor(ChartPalette palette) noexcept;

    /// Registers or retrieves the default raster basemap tile provider used when `cfg.showOsmBasemap` is true
    /// and `cfg.customBasemapProvider` is null. If none is registered, falls back to `CreateCartographicBasemapTileProvider()`.
    static void SetDefaultBasemapProvider(keel::Arc<IRasterTileProvider> provider);
    [[nodiscard]] static keel::Arc<IRasterTileProvider> GetDefaultBasemapProvider();

    /// Registers an optional diagnostic breadcrumb callback invoked at key rendering stages.
    static void SetBreadcrumbCallback(BreadcrumbCallback cb) noexcept;
};

} // namespace guikit::geo

namespace guikit {
using geo::ChartPalette;
using geo::ChartPaletteName;
using geo::GisColorPalette;
using geo::GisColorPaletteName;
using geo::GisRenderConfig;
using geo::GisRenderEngine;
using geo::GisRenderSettings;
using geo::GisRenderStats;
} // namespace guikit
