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
    guikit::GisRenderSettings renderSettings{};

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
    std::optional<uint64_t> selectedFeatureId{};
    std::optional<uint32_t> selectedLayerId{};
    guikit::Vec2 mouseDownPos{-10000.0f, -10000.0f};

    std::optional<guikit::GeoCoord> pendingCenterGeo{};
    std::optional<float> pendingTileZoom{};
    bool requestResetView{false};

    static constexpr std::array<std::string_view, 4> paletteNames = {
        "Navionics Day (S-52 Bright)",
        "Navionics Dusk (Twilight)",
        "Navionics Night (Bridge Dark)",
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

    [[nodiscard]] const guikit::GeoFeature* FindFeature(
        std::optional<uint32_t> layerId,
        std::optional<uint64_t> featureId
    ) const noexcept {
        if (!featureId.has_value()) return nullptr;
        for (const auto& layer : layers) {
            if (layerId.has_value() && layer.id != *layerId) continue;
            for (const auto& feat : layer.features) {
                if (feat.id == *featureId) {
                    return &feat;
                }
            }
        }
        return nullptr;
    }
};

GisScreenState s_state;

} // namespace

void RenderOsmMapInspector(guikit::Context& ui, DemoContext&) {
    s_state.EnsureLoaded();

    // 1. Camera & Dataset Header Actions
    if (auto row = ui.Row({.gap = 6.0f})) {
        if (ui.Button({
            .label = "Fit Chart Bounds",
            .icon = guikit::Icon::Search(),
            .variant = guikit::UiVariant::Primary,
            .stretch = guikit::StretchMode::Fill,
            .size = guikit::UiSize::Sm
        }).clicked) {
            s_state.pendingCenterGeo = s_state.initialCenter;
            s_state.pendingTileZoom = s_state.initialTileZoom;
            ui.InvalidateAll();
        }
        if (ui.Button({
            .label = "Clear Sel",
            .variant = guikit::UiVariant::Secondary,
            .disabled = !s_state.selectedFeatureId.has_value(),
            .size = guikit::UiSize::Sm
        }).clicked) {
            s_state.selectedFeatureId.reset();
            s_state.selectedLayerId.reset();
            ui.InvalidateAll();
        }
    }

    // 2. S-52 / Navionics Palette & Bathymetry Safety Thresholds
    size_t palIdx = static_cast<size_t>(s_state.renderSettings.palette);
    if (ui.Select(palIdx, GisScreenState::paletteNames, {
        .label = "Chart Color Palette (S-52 / Navionics)"
    }).changed) {
        s_state.renderSettings.palette = static_cast<guikit::GisColorPalette>(palIdx);
        ui.InvalidateAll();
    }

    if (ui.Slider(s_state.renderSettings.safetyContourMeters, 1.0f, 20.0f, {
        .label = "Safety Contour Depth (m)",
        .valueFormat = "%.1f m",
        .step = 0.5
    }).changed) {
        ui.InvalidateAll();
    }

    // 3. Projection (CRS) & Coordinate Readout Format
    size_t crsIdx = static_cast<size_t>(s_state.crs);
    if (ui.Select(crsIdx, GisScreenState::crsNames, {
        .label = "Projection (CRS)"
    }).changed) {
        s_state.crs = static_cast<guikit::MapCrs>(crsIdx);
        ui.InvalidateAll();
    }

    size_t fmtIdx = static_cast<size_t>(s_state.coordFormat);
    if (ui.Select(fmtIdx, GisScreenState::coordFormatNames, {
        .label = "Coordinate Format"
    }).changed) {
        s_state.coordFormat = static_cast<guikit::GeoCoordFormat>(fmtIdx);
        ui.InvalidateAll();
    }

    // 4. S-52 Vector Display Filters
    ui.Heading({.title = "S-52 DISPLAY FILTERS", .size = guikit::UiSize::Xs, .divider = true});
    if (ui.Switch(s_state.renderSettings.showFilledPolygons, {.label = "Filled Polygons (LNDARE / DEPARE)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.renderSettings.showPolygonOutlines, {.label = "Polygon Outlines (COALNE)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.renderSettings.showDepthContours, {.label = "Depth Contours (DEPCNT)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.renderSettings.highlightSafetyContour, {
        .label = "Highlight Safety Isobath",
        .disabled = !s_state.renderSettings.showDepthContours
    }).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.renderSettings.showSoundings, {.label = "Spot Soundings (SOUNDG)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.renderSettings.showSeamarks, {.label = "IALA Seamarks & Lights"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.renderSettings.showFeatureLabels, {.label = "Cartographic Labels"}).changed) ui.InvalidateAll();

    // 5. Cartographic Grid & HUD Overlays
    ui.Heading({.title = "CARTOGRAPHIC OVERLAYS", .size = guikit::UiSize::Xs, .divider = true});
    if (ui.Switch(s_state.showNauticalGraticule, {.label = "Nautical Graticule (1' = 1 NM)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showBorderRulers, {.label = "Border Lat/Lon Zebra Rulers"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showScaleBarOverlay, {.label = "Dual Scale Bar (NM + km)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showTileGrid, {.label = "Vector Tile Grid [z/x/y]"}).changed) ui.InvalidateAll();

    // 6. Thematic Layers from demo_bay.geojson
    if (!s_state.layers.empty()) {
        ui.Heading({.title = "THEMATIC ENC LAYERS", .size = guikit::UiSize::Xs, .divider = true});
        for (auto& layer : s_state.layers) {
            const std::string lbl = std::format("{} ({})", layer.name, layer.features.size());
            if (ui.Checkbox(layer.visible, {.label = lbl, .size = guikit::UiSize::Sm}).changed) {
                ui.InvalidateAll();
            }
        }
    }

    // 7. Selected / Hovered Feature Inspector Card
    const guikit::GeoFeature* inspectFeat = s_state.FindFeature(s_state.selectedLayerId, s_state.selectedFeatureId);
    bool isHoverInspect = false;
    if (!inspectFeat && s_state.lastStats.hoveredFeatureId.has_value()) {
        inspectFeat = s_state.FindFeature(s_state.lastStats.hoveredLayerId, s_state.lastStats.hoveredFeatureId);
        isHoverInspect = (inspectFeat != nullptr);
    }

    if (inspectFeat != nullptr) {
        ui.Heading({
            .title = isHoverInspect ? "HOVERED FEATURE" : "SELECTED FEATURE",
            .size = guikit::UiSize::Xs,
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
                          inspectFeat->name.empty() ? guikit::GisFeatureClassName(inspectFeat->featureClass) : inspectFeat->name),
                {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Primary}
            );
            ui.Text(
                ui.Format("Geometry: {} ({} vtx)",
                          guikit::GeometryTypeName(inspectFeat->geometryType),
                          inspectFeat->TotalVertexCount()),
                {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Info}
            );
            if (inspectFeat->depthMinMeters.has_value() || inspectFeat->depthMaxMeters.has_value()) {
                ui.Text(
                    ui.Format("Depth Range: {:.1f}m .. {:.1f}m",
                              inspectFeat->depthMinMeters.value_or(0.0),
                              inspectFeat->depthMaxMeters.value_or(0.0)),
                    {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Success}
                );
            } else if (inspectFeat->contourDepthMeters.has_value()) {
                ui.Text(
                    ui.Format("Depth: {:.1f} m", *inspectFeat->contourDepthMeters),
                    {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Success}
                );
            }
            size_t shownProps = 0;
            for (const auto& attr : inspectFeat->attributes) {
                if (attr.key == "tile" || attr.key == "file_type" || attr.key == "tag" ||
                    attr.key == "scale_index" || attr.key == "scale_grid") {
                    continue;
                }
                ui.Text(
                    ui.Format("{}: {}", attr.key, attr.value),
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
            ui.Format("Zoom: z={} ({:.2f} m/px, {} tiles)",
                      s_state.lastView.tileZoom,
                      s_state.lastView.metersPerPixel,
                      s_state.lastView.visibleTiles.size()),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Info}
        );
        ui.Text(
            ui.Format("Center: {}",
                      guikit::MapGrid::FormatCoord(s_state.lastView.center, s_state.coordFormat, s_state.crs)),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Secondary}
        );
        ui.Text(
            ui.Format("Cursor: {}",
                      guikit::MapGrid::FormatCoord(s_state.lastView.cursor, s_state.coordFormat, s_state.crs)),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Success}
        );
    }
}

void RenderOsmMapStage(guikit::Context& ui, DemoContext&) {
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
        .selectableItems = false,
        .showScrollbars = s_state.showScrollbars,
        .showControls = s_state.showControls,
        .borderRadius = s_state.radius
    })) {
        const guikit::Vec2 mousePos = ui.GetMousePos();
        if (ui.IsMouseClicked()) {
            s_state.mouseDownPos = mousePos;
        }
        const guikit::Rect vpRect = map.GetScene().GetViewportRect();
        const float dragDistSq =
            (mousePos.x - s_state.mouseDownPos.x) * (mousePos.x - s_state.mouseDownPos.x) +
            (mousePos.y - s_state.mouseDownPos.y) * (mousePos.y - s_state.mouseDownPos.y);
        const bool clickedInMap =
            ui.IsMouseReleased() && !map.GetScene().IsPanning() && vpRect.Contains(mousePos) && (dragDistSq <= 36.0f);

        s_state.lastStats = guikit::GisRenderEngine::RenderLayers(
            map,
            s_state.layers,
            s_state.renderSettings,
            s_state.selectedFeatureId,
            mousePos,
            clickedInMap
        );

        if (clickedInMap) {
            s_state.selectedFeatureId = s_state.lastStats.clickedFeatureId;
            s_state.selectedLayerId = s_state.lastStats.clickedLayerId;
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
        "NavionicsDay",
        "NavionicsDusk",
        "NavionicsNight",
        "ClassicGis"
    };

    return std::format(
        "// Load embedded or disk S-57 / Navionics GeoJSON chart (Native + WebAssembly):\n"
        "auto chart = guikit::GeoJsonIO::LoadFromFile(\"assets/geo/demo_bay.geojson\");\n\n"
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
        "        .palette = guikit::GisColorPalette::{},\n"
        "        .safetyContourMeters = {:.1f}f,\n"
        "        .showDepthContours = {},\n"
        "        .showSoundings = {},\n"
        "        .showSeamarks = {},\n"
        "        .showFeatureLabels = {}\n"
        "    }}, selectedFeatureId, ui.GetMousePos(), clickedInMap);\n"
        "}}",
        crsEnumNames[static_cast<size_t>(s_state.crs)],
        fmtEnumNames[static_cast<size_t>(s_state.coordFormat)],
        s_state.initialTileZoom,
        s_state.showNauticalGraticule ? "true" : "false",
        s_state.showBorderRulers ? "true" : "false",
        s_state.showScaleBarOverlay ? "true" : "false",
        s_state.showTileGrid ? "true" : "false",
        palEnumNames[static_cast<size_t>(s_state.renderSettings.palette)],
        s_state.renderSettings.safetyContourMeters,
        s_state.renderSettings.showDepthContours ? "true" : "false",
        s_state.renderSettings.showSoundings ? "true" : "false",
        s_state.renderSettings.showSeamarks ? "true" : "false",
        s_state.renderSettings.showFeatureLabels ? "true" : "false"
    );
}

} // namespace guikit::demo
