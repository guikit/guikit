#include "keel/math.h"
#include "../demo_context.hpp"
#include "guikit/geo/geo_json_io.hpp"
#include "guikit/geo/gis_render_engine.hpp"

namespace guikit::demo {

namespace {

struct GisScreenState {
    bool initialized{false};
    keel::StringRef loadedAssetPath{"assets/geo/demo_bay.geojson"};
    keel::Array<guikit::GeoLayer> layers;
    guikit::GeoBBox chartBounds{};
    keel::usize totalFeatures{0};
    keel::usize totalVertices{0};

    guikit::GeoCoord initialCenter{44.6020, 33.4950};
    float initialTileZoom{13.4f};

    guikit::MapCrs crs{guikit::MapCrs::EPSG3857_WebMercator};
    guikit::GeoCoordFormat coordFormat{guikit::GeoCoordFormat::NauticalDMM};
    guikit::GisRenderConfig renderConfig{};

    float worldSize{2048.0f};
    keel::i32 baseZoom{3};
    bool showTileGrid{false};
    bool showSubTileQuadtree{false};
    bool showNauticalGraticule{true};
    bool showBorderRulers{true};
    bool showScaleBarOverlay{true};
    bool showControls{true};
    bool showScrollbars{true};
    float radius{8.0f};

    guikit::MapViewState lastView{};
    guikit::GisRenderStats lastStats{};
    keel::u64 selectedFeatureUid{0};

    keel::Optional<guikit::GeoCoord> pendingCenterGeo{};
    keel::Optional<float> pendingTileZoom{};
    bool requestResetView{false};

    static constexpr keel::StaticArray<keel::StringRef, 4> paletteNames = {
        "S-52 Marine Day (Bright)",
        "S-52 Marine Dusk (Twilight)",
        "S-52 Marine Night (Bridge Dark)",
        "Classic Vector GIS"
    };

    static constexpr keel::StaticArray<keel::StringRef, 7> crsNames = {
        "EPSG:3857 Web Mercator (WGS84)",
        "EPSG:3395 World Mercator (Ellipsoid)",
        "EPSG:4326 Plate Carrée (Equirect)",
        "EPSG:32636 UTM Zone 36N (33°E)",
        "EPSG:20906 GSK-2011 Zone 6 (GOST)",
        "EPSG:28406 Pulkovo-42 / SK-42 Z6",
        "EPSG:3995 Arctic Polar Stereo"
    };

    static constexpr keel::StaticArray<keel::StringRef, 5> coordFormatNames = {
        "Nautical DMM (DD° MM.mm')",
        "Decimal Degrees (DD.dddd°)",
        "DMS (DD° MM' SS.s\")",
        "UTM Metric (Zone E / N m)",
        "GOST Gauss-Krüger (X / Y m)"
    };

    static constexpr keel::StaticArray<keel::StringRef, 3> tileSizeNames = {
        "128 px",
        "256 px",
        "512 px"
    };

    void EnsureLoaded() {
        if (initialized) return;
        initialized = true;

        auto res = guikit::GeoJsonIO::LoadFromFile(loadedAssetPath, /*splitByThematicClass=*/true);
        if (res.ok && !res.layers.empty()) {
            layers = keel::move(res.layers);
            chartBounds = res.totalBounds;
            totalFeatures = res.totalFeatures;
            totalVertices = res.totalVertices;

            if (chartBounds.IsValid()) {
                initialCenter = chartBounds.Center();
                const double spanLon = keel::max(0.005, chartBounds.SpanLon());
                const double spanLat = keel::max(0.005, chartBounds.SpanLat());
                const double maxSpan = keel::max(spanLon, spanLat * 1.35);
                initialTileZoom = keel::clamp(
                    static_cast<float>(keel::log2(360.0 / (maxSpan * 1.35))),
                    2.0f,
                    16.5f
                );
            }
        }
    }
};

GisScreenState s_state;

} // namespace

void RenderOsmMapInspector(guikit::Context& ui, DemoContext&) {
    s_state.EnsureLoaded();

    // 1. Camera & Dataset Header Actions
    if (auto row = ui.Row({.gap = 6.0f})) {
        if (ui.Button({
            .text = "Fit Chart Bounds",
            .icon = guikit::Icon::Search(),
            .layout = {.width = guikit::SizeMode::Stretch},
            .style = {.size = guikit::UiSize::Sm, .color = guikit::StateColor::Primary()}
        }).clicked) {
            s_state.pendingCenterGeo = s_state.initialCenter;
            s_state.pendingTileZoom = s_state.initialTileZoom;
            ui.InvalidateAll();
        }
        if (ui.Button({
            .text = "Clear Sel",
            .disabled = (s_state.selectedFeatureUid == 0),
            .style = {.size = guikit::UiSize::Sm, .color = guikit::StateColor::Secondary()}
        }).clicked) {
            s_state.selectedFeatureUid = 0;
            ui.InvalidateAll();
        }
    }

    // 2. S-52 / ENC Palette & Bathymetry Safety Thresholds
    keel::usize palIdx = static_cast<keel::usize>(s_state.renderConfig.chartPalette);
    if (auto el = ui.Element({.label = "Chart Color Palette (S-52 / ENC)"})) {
        if (ui.Select(palIdx, GisScreenState::paletteNames).changed) {
            s_state.renderConfig.chartPalette = static_cast<guikit::ChartPalette>(palIdx);
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Safety Contour Depth (m)"})) {
        if (ui.Slider(s_state.renderConfig.safetyContourM, 1.0f, 20.0f, {
            .valueFormat = "%.1f m",
            .step = 0.5
        }).changed) {
            ui.InvalidateAll();
        }
    }

    // 3. Projection (CRS), Coordinate Readout Format & Tile Size
    keel::usize crsIdx = static_cast<keel::usize>(s_state.crs);
    if (auto el = ui.Element({.label = "Projection (CRS)"})) {
        if (ui.Select(crsIdx, GisScreenState::crsNames).changed) {
            s_state.crs = static_cast<guikit::MapCrs>(crsIdx);
            ui.InvalidateAll();
        }
    }

    keel::usize fmtIdx = static_cast<keel::usize>(s_state.coordFormat);
    if (auto el = ui.Element({.label = "Coordinate Format"})) {
        if (ui.Select(fmtIdx, GisScreenState::coordFormatNames).changed) {
            s_state.coordFormat = static_cast<guikit::GeoCoordFormat>(fmtIdx);
            ui.InvalidateAll();
        }
    }

    keel::usize tileSizeIdx = (s_state.renderConfig.tileSizePx <= 128u) ? 0u
                       : ((s_state.renderConfig.tileSizePx >= 512u) ? 2u : 1u);
    if (auto el = ui.Element({.label = "Tile Resolution [z/x/y]"})) {
        if (ui.StepSlider(tileSizeIdx, GisScreenState::tileSizeNames, {.showTicks = true, .showLabels = true}).changed) {
            s_state.renderConfig.tileSizePx = (tileSizeIdx == 0u) ? 128u : ((tileSizeIdx == 2u) ? 512u : 256u);
            ui.InvalidateAll();
        }
    }

    // 4. S-52 Vector Display & Tile Engine Filters
    ui.Heading({.label = "S-52 & TILE ENGINE FILTERS", .level = guikit::HeadingLevel::H6, .separator = guikit::HeadingSeparator::Bottom});
    if (auto el = ui.Element({.label = "Vector Tile Cache [z/x/y]"})) {
        if (ui.Switch(s_state.renderConfig.useVectorTileCache).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Loading Tile Placeholders"})) {
        if (ui.Switch(s_state.renderConfig.showLoadingTilePlaceholders).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Raster Basemap Underlay"})) {
        if (ui.Switch(s_state.renderConfig.showOsmBasemap).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Depth Contours (DEPCNT)"})) {
        if (ui.Switch(s_state.renderConfig.showDepthContours).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Spot Soundings (SOUNDG)"})) {
        if (ui.Switch(s_state.renderConfig.showSoundings).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Cartographic Labels & Callouts"})) {
        if (ui.Switch(s_state.renderConfig.showFeatureCallouts).changed) ui.InvalidateAll();
    }

    // 5. Cartographic Grid & HUD Overlays
    ui.Heading({.label = "CARTOGRAPHIC OVERLAYS", .level = guikit::HeadingLevel::H6, .separator = guikit::HeadingSeparator::Bottom});
    if (auto el = ui.Element({.label = "Nautical Graticule (1' = 1 NM)"})) {
        if (ui.Switch(s_state.showNauticalGraticule).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Border Lat/Lon Zebra Rulers"})) {
        if (ui.Switch(s_state.showBorderRulers).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Dual Scale Bar (NM + km)"})) {
        if (ui.Switch(s_state.showScaleBarOverlay).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Vector Tile Grid [z/x/y]"})) {
        if (ui.Switch(s_state.showTileGrid).changed) ui.InvalidateAll();
    }

    // 6. Thematic Layers from demo_bay.geojson
    if (!s_state.layers.empty()) {
        ui.Heading({.label = "THEMATIC ENC LAYERS", .level = guikit::HeadingLevel::H6, .separator = guikit::HeadingSeparator::Bottom});
        for (auto& layer : s_state.layers) {
            if (auto el = ui.Element({
                .label = ui.Format("{} ({})", layer.name.as_ref(), layer.features.size()),
                .size = guikit::UiSize::Sm
            })) {
                if (ui.Switch(layer.visible, {.style = {.size = guikit::UiSize::Sm}}).changed) {
                    ui.InvalidateAll();
                }
            }
        }
    }
}

void RenderOsmMapStage(guikit::Context& ui, DemoContext& ctx) {
    s_state.EnsureLoaded();

    if (auto map = ui.MapView({
        .crs = s_state.crs,
        .coordFormat = s_state.coordFormat,
        .worldSize = s_state.worldSize,
        .baseZoom = s_state.baseZoom,
        .initialCenter = s_state.initialCenter,
        .initialTileZoom = s_state.initialTileZoom,
        .requestCenterGeo = s_state.pendingCenterGeo,
        .requestTileZoom = s_state.pendingTileZoom,
        .requestResetView = s_state.requestResetView,
        .showTileGrid = s_state.showTileGrid,
        .showSubTileQuadtree = s_state.showSubTileQuadtree,
        .showNauticalGraticule = s_state.showNauticalGraticule,
        .showBorderRulers = s_state.showBorderRulers,
        .showScaleBarOverlay = s_state.showScaleBarOverlay,
        .height = guikit::SizeMode::Stretch,
        .selectableItems = true,
        .showScrollbars = s_state.showScrollbars,
        .showControls = s_state.showControls,
        .border = {.width = 1.0f, .radius = s_state.radius}
    })) {
        s_state.lastStats = guikit::GisRenderEngine::RenderLayers(
            map,
            s_state.layers,
            s_state.renderConfig,
            s_state.selectedFeatureUid,
            ctx.previewLightMode
        );
        if (s_state.lastStats.cachedTilesPending > 0 || s_state.lastStats.cachedTilesRendered > 0) {
            ui.InvalidateAll();
        }
        s_state.lastView = map.GetViewState();
    }

    s_state.pendingCenterGeo.reset();
    s_state.pendingTileZoom.reset();
    s_state.requestResetView = false;
}

keel::String GenerateOsmMapCode(const DemoContext&) {
    static constexpr keel::StaticArray<keel::StringRef, 7> crsEnumNames = {
        "EPSG3857_WebMercator",
        "EPSG3395_WorldMercator",
        "EPSG4326_PlateCarree",
        "EPSG32636_Utm36N",
        "EPSG20906_Gsk2011Zone6",
        "EPSG28406_Pulkovo42Zone6",
        "EPSG3995_ArcticPolar"
    };
    static constexpr keel::StaticArray<keel::StringRef, 5> fmtEnumNames = {
        "NauticalDMM",
        "DecimalDegrees",
        "DMS",
        "UTM_Meters",
        "GaussKruger_Meters"
    };
    static constexpr keel::StaticArray<keel::StringRef, 4> palEnumNames = {
        "MarineDay",
        "MarineDusk",
        "MarineNight",
        "ClassicGis"
    };

    return keel::format(
        "// Load embedded or disk S-57 / ENC GeoJSON chart (Native + WebAssembly):\n"
        "auto chart = guikit::GeoJsonIO::LoadFromFile(\"assets/geo/demo_bay.geojson\", true);\n\n"
        "if (auto map = ui.MapView({{\n"
        "    .crs = guikit::MapCrs::{},\n"
        "    .coordFormat = guikit::GeoCoordFormat::{},\n"
        "    .initialCenter = chart.totalBounds.Center(),\n"
        "    .initialTileZoom = {:.1f}f,\n"
        "    .showNauticalGraticule = {},\n"
        "    .showBorderRulers = {},\n"
        "    .showScaleBarOverlay = {},\n"
        "    .showTileGrid = {}\n"
        "}})) {{\n"
        "    guikit::GisRenderEngine::RenderLayers(map, chart.layers, {{\n"
        "        .chartPalette = guikit::ChartPalette::{},\n"
        "        .safetyContourM = {:.1f}f,\n"
        "        .tileSizePx = {},\n"
        "        .showLoadingTilePlaceholders = {},\n"
        "        .showOsmBasemap = {},\n"
        "        .showDepthContours = {},\n"
        "        .showSoundings = {},\n"
        "        .showFeatureCallouts = {}\n"
        "    }}, selectedFeatureUid);\n"
        "}}",
        crsEnumNames[static_cast<keel::usize>(s_state.crs)],
        fmtEnumNames[static_cast<keel::usize>(s_state.coordFormat)],
        s_state.initialTileZoom,
        s_state.showNauticalGraticule ? "true" : "false",
        s_state.showBorderRulers ? "true" : "false",
        s_state.showScaleBarOverlay ? "true" : "false",
        s_state.showTileGrid ? "true" : "false",
        palEnumNames[static_cast<keel::usize>(s_state.renderConfig.chartPalette)],
        s_state.renderConfig.safetyContourM,
        s_state.renderConfig.tileSizePx,
        s_state.renderConfig.showLoadingTilePlaceholders ? "true" : "false",
        s_state.renderConfig.showOsmBasemap ? "true" : "false",
        s_state.renderConfig.showDepthContours ? "true" : "false",
        s_state.renderConfig.showSoundings ? "true" : "false",
        s_state.renderConfig.showFeatureCallouts ? "true" : "false"
    );
}

void ReleaseOsmMapDemoBuffers() noexcept {
    guikit::GisRenderEngine::TrimInactiveCaches();
    s_state.layers = {};
    s_state.initialized = false;
}

} // namespace guikit::demo
