#include "../DemoContext.hpp"
#include "guikit/geo/GeoJsonIO.hpp"
#include "guikit/geo/GisRenderEngine.hpp"
#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace guikit::demo {

namespace {

struct GisScreenState {
    bool initialized{false};
    std::string loadedAssetPath{"assets/geo/demo_bay.geojson"};
    std::vector<guikit::GeoLayer> layers;
    guikit::GeoBBox chartBounds{};
    size_t totalFeatures{0};
    size_t totalVertices{0};

    guikit::GeoCoord initialCenter{44.6020, 33.4950};
    float initialTileZoom{13.4f};

    guikit::MapCrs crs{guikit::MapCrs::EPSG3857_WebMercator};
    guikit::GeoCoordFormat coordFormat{guikit::GeoCoordFormat::NauticalDMM};
    guikit::GisRenderConfig renderConfig{};

    float worldSize{2048.0f};
    int baseZoom{3};
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
    uint64_t selectedFeatureUid{0};

    std::optional<guikit::GeoCoord> pendingCenterGeo{};
    std::optional<float> pendingTileZoom{};
    bool requestResetView{false};

    static constexpr std::array<std::string_view, 4> paletteNames = {
        "S-52 Marine Day (Bright)",
        "S-52 Marine Dusk (Twilight)",
        "S-52 Marine Night (Bridge Dark)",
        "Classic Vector GIS"
    };

    static constexpr std::array<std::string_view, 7> crsNames = {
        "EPSG:3857 Web Mercator (WGS84)",
        "EPSG:3395 World Mercator (Ellipsoid)",
        "EPSG:4326 Plate Carrée (Equirect)",
        "EPSG:32636 UTM Zone 36N (33°E)",
        "EPSG:20906 GSK-2011 Zone 6 (GOST)",
        "EPSG:28406 Pulkovo-42 / SK-42 Z6",
        "EPSG:3995 Arctic Polar Stereo"
    };

    static constexpr std::array<std::string_view, 5> coordFormatNames = {
        "Nautical DMM (DD° MM.mm')",
        "Decimal Degrees (DD.dddd°)",
        "DMS (DD° MM' SS.s\")",
        "UTM Metric (Zone E / N m)",
        "GOST Gauss-Krüger (X / Y m)"
    };

    static constexpr std::array<std::string_view, 3> tileSizeNames = {
        "128 px (Ultra-Fast Low Latency)",
        "256 px (Standard Slippy Tile)",
        "512 px (High-Density Retina)"
    };

    void EnsureLoaded() {
        if (initialized) return;
        initialized = true;

        auto res = guikit::GeoJsonIO::LoadFromFile(loadedAssetPath, /*splitByThematicClass=*/true);
        if (res.ok && !res.layers.empty()) {
            layers = std::move(res.layers);
            chartBounds = res.totalBounds;
            totalFeatures = res.totalFeatures;
            totalVertices = res.totalVertices;

            if (chartBounds.IsValid()) {
                initialCenter = chartBounds.Center();
                const double spanLon = std::max(0.005, chartBounds.SpanLon());
                const double spanLat = std::max(0.005, chartBounds.SpanLat());
                const double maxSpan = std::max(spanLon, spanLat * 1.35);
                initialTileZoom = std::clamp(
                    static_cast<float>(std::log2(360.0 / (maxSpan * 1.35))),
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
        if (ui.Size(guikit::UiSize::Sm).Button({
            .text = "Fit Chart Bounds",
            .icon = guikit::Icon::Search(),
            .color = guikit::StateColor::Primary(),
            .stretch = guikit::StretchMode::Fill
        }).clicked) {
            s_state.pendingCenterGeo = s_state.initialCenter;
            s_state.pendingTileZoom = s_state.initialTileZoom;
            ui.InvalidateAll();
        }
        if (ui.Size(guikit::UiSize::Sm).Button({
            .text = "Clear Sel",
            .color = guikit::StateColor::Secondary(),
            .disabled = (s_state.selectedFeatureUid == 0)
        }).clicked) {
            s_state.selectedFeatureUid = 0;
            ui.InvalidateAll();
        }
    }

    // 2. S-52 / ENC Palette & Bathymetry Safety Thresholds
    size_t palIdx = static_cast<size_t>(s_state.renderConfig.chartPalette);
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
    size_t crsIdx = static_cast<size_t>(s_state.crs);
    if (auto el = ui.Element({.label = "Projection (CRS)"})) {
        if (ui.Select(crsIdx, GisScreenState::crsNames).changed) {
            s_state.crs = static_cast<guikit::MapCrs>(crsIdx);
            ui.InvalidateAll();
        }
    }

    size_t fmtIdx = static_cast<size_t>(s_state.coordFormat);
    if (auto el = ui.Element({.label = "Coordinate Format"})) {
        if (ui.Select(fmtIdx, GisScreenState::coordFormatNames).changed) {
            s_state.coordFormat = static_cast<guikit::GeoCoordFormat>(fmtIdx);
            ui.InvalidateAll();
        }
    }

    size_t tileSizeIdx = (s_state.renderConfig.tileSizePx <= 128u) ? 0u
                       : ((s_state.renderConfig.tileSizePx >= 512u) ? 2u : 1u);
    if (auto el = ui.Element({.label = "Tile Resolution [z/x/y]"})) {
        if (ui.Select(tileSizeIdx, GisScreenState::tileSizeNames).changed) {
            s_state.renderConfig.tileSizePx = (tileSizeIdx == 0u) ? 128u : ((tileSizeIdx == 2u) ? 512u : 256u);
            ui.InvalidateAll();
        }
    }

    // 4. S-52 Vector Display & Tile Engine Filters
    ui.Size(guikit::UiSize::Xs).Heading({.label = "S-52 & TILE ENGINE FILTERS", .divider = true});
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
    ui.Size(guikit::UiSize::Xs).Heading({.label = "CARTOGRAPHIC OVERLAYS", .divider = true});
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
        ui.Size(guikit::UiSize::Xs).Heading({.label = "THEMATIC ENC LAYERS", .divider = true});
        for (auto& layer : s_state.layers) {
            const std::string lbl = std::format("{} ({})", layer.name, layer.features.size());
            if (auto el = ui.Size(guikit::UiSize::Sm).Element({.label = lbl})) {
                if (ui.Size(guikit::UiSize::Sm).Checkbox(layer.visible).changed) {
                    ui.InvalidateAll();
                }
            }
        }
    }

    // 7. Selected / Hovered Feature Inspector Card
    const guikit::GeoLayer* inspectLayer = nullptr;
    const guikit::GeoFeature* inspectFeat = guikit::FindFeatureByUid(
        s_state.layers, s_state.selectedFeatureUid, &inspectLayer
    );
    bool isHoverInspect = false;
    if (!inspectFeat && s_state.lastStats.hoveredFeatureUid != 0) {
        inspectFeat = guikit::FindFeatureByUid(
            s_state.layers, s_state.lastStats.hoveredFeatureUid, &inspectLayer
        );
        isHoverInspect = (inspectFeat != nullptr);
    }

    if (inspectFeat != nullptr) {
        ui.Size(guikit::UiSize::Xs).Heading({
            .label = isHoverInspect ? "HOVERED FEATURE" : "SELECTED FEATURE",
            .divider = true
        });
        if (auto featCard = ui.Card({
            .gap = 3.0f,
            .padding = guikit::Padding::All(8.0f),
            .borderRadius = 6.0f
        })) {
            ui.Text(
                ui.Format("[{}] {}",
                          inspectFeat->featureCode.empty() ? "FEAT" : inspectFeat->featureCode,
                          inspectFeat->name.empty() ? guikit::ChartFeatureClassToString(inspectFeat->chartClass) : inspectFeat->name),
                {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Primary}
            );
            ui.Text(
                ui.Format("Geometry: {} ({} vtx)",
                          guikit::GeometryTypeToString(inspectFeat->type),
                          inspectFeat->TotalVertexCount()),
                {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Info}
            );
            if (!std::isnan(inspectFeat->minDepthMeters) || !std::isnan(inspectFeat->maxDepthMeters)) {
                ui.Text(
                    ui.Format("Depth Range: {:.1f}m .. {:.1f}m",
                              std::isnan(inspectFeat->minDepthMeters) ? 0.0f : inspectFeat->minDepthMeters,
                              std::isnan(inspectFeat->maxDepthMeters) ? 0.0f : inspectFeat->maxDepthMeters),
                    {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Success}
                );
            } else if (!std::isnan(inspectFeat->depthMeters)) {
                ui.Text(
                    ui.Format("Depth: {:.1f} m", inspectFeat->depthMeters),
                    {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Success}
                );
            }
            size_t shownProps = 0;
            for (const auto& [key, value] : inspectFeat->properties) {
                if (key == "tile" || key == "file_type" || key == "tag" ||
                    key == "scale_index" || key == "scale_grid") {
                    continue;
                }
                ui.Text(
                    ui.Format("{}: {}", key, value.ToString()),
                    {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Secondary}
                );
                if (++shownProps >= 5) break;
            }
        }
    }

    // 8. Cartographic & Asset Telemetry Card
    if (auto telCard = ui.Card({
        .gap = 4.0f,
        .padding = guikit::Padding::All(8.0f),
        .borderRadius = 6.0f
    })) {
        ui.Text(
            ui.Format("Dataset: {} ({} layers, {} features)",
                      s_state.loadedAssetPath,
                      s_state.layers.size(),
                      s_state.totalFeatures),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Primary}
        );
        ui.Text(
            ui.Format("Zoom: z={} ({:.2f} m/px, {} tiles @ {}px)",
                      s_state.lastView.tileZoom,
                      s_state.lastView.metersPerPixel,
                      s_state.lastView.visibleTiles.size(),
                      s_state.renderConfig.tileSizePx),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Info}
        );
        ui.Text(
            ui.Format("Tile Cache: {} hit / {} new / {} pending",
                      s_state.lastStats.cachedTilesHit,
                      s_state.lastStats.cachedTilesRendered,
                      s_state.lastStats.cachedTilesPending),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Secondary}
        );
        ui.Text(
            ui.Format("Cursor: {}",
                      guikit::MapGrid::FormatCoord(s_state.lastView.cursor, s_state.coordFormat, s_state.crs)),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Success}
        );
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
        .heightMode = guikit::SizeMode::Stretch,
        .selectableItems = true,
        .showScrollbars = s_state.showScrollbars,
        .showControls = s_state.showControls,
        .borderRadius = s_state.radius
    })) {
        s_state.lastStats = guikit::GisRenderEngine::RenderLayers(
            map,
            s_state.layers,
            s_state.renderConfig,
            s_state.selectedFeatureUid,
            ctx.previewLightMode
        );
        if (s_state.lastStats.cachedTilesPending > 0) {
            ui.InvalidateAll();
        }
        s_state.lastView = map.GetViewState();
    }

    s_state.pendingCenterGeo.reset();
    s_state.pendingTileZoom.reset();
    s_state.requestResetView = false;
}

std::string GenerateOsmMapCode(const DemoContext&) {
    static constexpr std::array<std::string_view, 7> crsEnumNames = {
        "EPSG3857_WebMercator",
        "EPSG3395_WorldMercator",
        "EPSG4326_PlateCarree",
        "EPSG32636_Utm36N",
        "EPSG20906_Gsk2011Zone6",
        "EPSG28406_Pulkovo42Zone6",
        "EPSG3995_ArcticPolar"
    };
    static constexpr std::array<std::string_view, 5> fmtEnumNames = {
        "NauticalDMM",
        "DecimalDegrees",
        "DMS",
        "UTM_Meters",
        "GaussKruger_Meters"
    };
    static constexpr std::array<std::string_view, 4> palEnumNames = {
        "MarineDay",
        "MarineDusk",
        "MarineNight",
        "ClassicGis"
    };

    return std::format(
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
        crsEnumNames[static_cast<size_t>(s_state.crs)],
        fmtEnumNames[static_cast<size_t>(s_state.coordFormat)],
        s_state.initialTileZoom,
        s_state.showNauticalGraticule ? "true" : "false",
        s_state.showBorderRulers ? "true" : "false",
        s_state.showScaleBarOverlay ? "true" : "false",
        s_state.showTileGrid ? "true" : "false",
        palEnumNames[static_cast<size_t>(s_state.renderConfig.chartPalette)],
        s_state.renderConfig.safetyContourM,
        s_state.renderConfig.tileSizePx,
        s_state.renderConfig.showLoadingTilePlaceholders ? "true" : "false",
        s_state.renderConfig.showOsmBasemap ? "true" : "false",
        s_state.renderConfig.showDepthContours ? "true" : "false",
        s_state.renderConfig.showSoundings ? "true" : "false",
        s_state.renderConfig.showFeatureCallouts ? "true" : "false"
    );
}

} // namespace guikit::demo
