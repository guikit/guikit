#include "../DemoContext.hpp"
#include "OsmMapPrimitives.hpp"
#include <format>
#include <optional>

namespace guikit::demo {

namespace {
struct OsmMapState {
    guikit::MapCrs crs = guikit::MapCrs::EPSG3857_WebMercator;
    guikit::GeoCoordFormat coordFormat = guikit::GeoCoordFormat::NauticalDMM;
    float worldSize = 2048.0f;
    int baseZoom = 3;
    bool showTileGrid = true;
    bool showSubTileQuadtree = true;
    bool showNauticalGraticule = true;
    bool showBorderRulers = true;
    bool showSeamarksAndCourses = true;
    bool showScaleBarOverlay = true;
    bool showControls = true;
    bool showScrollbars = true;
    bool selectableItems = true;
    float radius = 8.0f;

    guikit::MapViewState lastView{};
    std::optional<guikit::GeoCoord> pendingCenterGeo{};
    std::optional<float> pendingTileZoom{};
    size_t regionPresetIdx = 0;

    static constexpr std::array<std::string_view, 6> regionPresetNames = {
        "Helgoland / German Bight (z=7)",
        "Kiel Canal / Elbe Approach (z=9)",
        "Dover Strait TSS (z=8)",
        "Greenwich Prime Meridian (z=8)",
        "Strait of Gibraltar (z=8)",
        "Global Overview (z=2)"
    };
    static constexpr std::array<std::string_view, 3> crsShortNames = {
        "EPSG:3857", "EPSG:3395", "EPSG:4326"
    };
    static constexpr std::array<std::string_view, 3> coordFormatNames = {
        "Nautical DMM", "Decimal", "DMS"
    };
};
OsmMapState s_state;
} // namespace

void RenderOsmMapInspector(guikit::Context& ui, DemoContext&) {
    if (ui.Select(s_state.regionPresetIdx, OsmMapState::regionPresetNames, {
        .label = "Chart Region / Camera Jump",
        .prefix = guikit::Icon::Search()
    }).changed) {
        if (s_state.regionPresetIdx < kMaritimeWaypoints.size()) {
            const auto& wp = kMaritimeWaypoints[s_state.regionPresetIdx];
            s_state.pendingCenterGeo = wp.coord;
            s_state.pendingTileZoom = wp.defaultTileZoom;
        } else {
            s_state.pendingCenterGeo = guikit::GeoCoord{20.0, 0.0};
            s_state.pendingTileZoom = 2.0f;
        }
        ui.InvalidateAll();
    }

    size_t crsIdx = static_cast<size_t>(s_state.crs);
    if (ui.StepSlider(crsIdx, OsmMapState::crsShortNames, {
        .label = "Projection (CRS)",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        s_state.crs = static_cast<guikit::MapCrs>(crsIdx);
        ui.InvalidateAll();
    }

    size_t fmtIdx = static_cast<size_t>(s_state.coordFormat);
    if (ui.StepSlider(fmtIdx, OsmMapState::coordFormatNames, {
        .label = "Coordinate Format",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        s_state.coordFormat = static_cast<guikit::GeoCoordFormat>(fmtIdx);
        ui.InvalidateAll();
    }

    if (ui.Switch(s_state.showTileGrid, {.label = "OSM Tile Grid [z/x/y]"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showSubTileQuadtree, {.label = "2x2 Sub-Tile Quadtree", .disabled = !s_state.showTileGrid}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showNauticalGraticule, {.label = "Nautical Graticule (OpenSeaMap)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showBorderRulers, {.label = "Border Lat/Lon Rulers (1' = 1 NM)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showSeamarksAndCourses, {.label = "IALA Seamarks & Rhumb Courses"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showScaleBarOverlay, {.label = "Dual Scale Bar (NM + km)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showControls, {.label = "Floating Zoom & Inspector HUD"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showScrollbars, {.label = "2D Scrollbars (H + V)"}).changed) ui.InvalidateAll();

    if (auto telCard = ui.Card({
        .gap = 4.0f,
        .padding = guikit::Padding::All(8.0f),
        .borderRadius = 6.0f
    })) {
        ui.Text(
            ui.Format("Tile Pyramid: z={} ({} visible tiles)",
                      s_state.lastView.tileZoom,
                      s_state.lastView.visibleTiles.size()),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Primary}
        );
        ui.Text(
            ui.Format("Graticule Step: {:.2f}' ({:.2f} NM)",
                      s_state.lastView.graticuleStepArcMin,
                      s_state.lastView.graticuleStepArcMin),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Info}
        );
        ui.Text(
            ui.Format("Center: {}",
                      guikit::MapGrid::FormatCoord(s_state.lastView.center, s_state.coordFormat)),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Secondary}
        );
        const auto curTile = guikit::MapGrid::GeoToTileId(s_state.lastView.cursor, s_state.lastView.tileZoom, s_state.crs);
        ui.Text(
            ui.Format("Cursor: {} [/{}/{}/{}]",
                      guikit::MapGrid::FormatCoord(s_state.lastView.cursor, s_state.coordFormat),
                      curTile.z,
                      curTile.x,
                      curTile.y),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Success}
        );
    }
}

void RenderOsmMapStage(guikit::Context& ui, DemoContext& ctx) {
    if (auto map = ui.MapView({
        .crs = s_state.crs,
        .coordFormat = s_state.coordFormat,
        .worldSize = s_state.worldSize,
        .baseZoom = s_state.baseZoom,
        .initialCenter = kMaritimeWaypoints[0].coord,
        .initialTileZoom = 7.0f,
        .requestCenterGeo = s_state.pendingCenterGeo,
        .requestTileZoom = s_state.pendingTileZoom,
        .showTileGrid = s_state.showTileGrid,
        .showSubTileQuadtree = s_state.showSubTileQuadtree,
        .showNauticalGraticule = s_state.showNauticalGraticule,
        .showBorderRulers = s_state.showBorderRulers,
        .showScaleBarOverlay = s_state.showScaleBarOverlay,
        .heightMode = guikit::SizeMode::Stretch,
        .selectableItems = s_state.selectableItems,
        .showScrollbars = s_state.showScrollbars,
        .showControls = s_state.showControls,
        .borderRadius = s_state.radius
    })) {
        if (s_state.showSeamarksAndCourses) {
            RenderDemoMaritimeOverlay(map, ctx.previewLightMode);
        }
        s_state.lastView = map.GetViewState();
    }

    s_state.pendingCenterGeo.reset();
    s_state.pendingTileZoom.reset();
}

std::string GenerateOsmMapCode(const DemoContext&) {
    static constexpr std::array<std::string_view, 3> crsEnumNames = {
        "EPSG3857_WebMercator",
        "EPSG3395_WorldMercator",
        "EPSG4326_PlateCarree"
    };
    static constexpr std::array<std::string_view, 3> fmtEnumNames = {
        "NauticalDMM",
        "DecimalDegrees",
        "DMS"
    };
    return std::format(
        "if (auto map = ui.MapView({{\n"
        "    .crs = guikit::MapCrs::{},\n"
        "    .coordFormat = guikit::GeoCoordFormat::{},\n"
        "    .initialCenter = {{54.1825, 7.8853}},\n"
        "    .initialTileZoom = 7.0f,\n"
        "    .showTileGrid = {},\n"
        "    .showSubTileQuadtree = {},\n"
        "    .showNauticalGraticule = {},\n"
        "    .showBorderRulers = {},\n"
        "    .showScaleBarOverlay = {},\n"
        "    .showScrollbars = {},\n"
        "    .showControls = {},\n"
        "    .tileSource = myTileResolver\n"
        "}})) {{\n"
        "    // Draw directly in WGS84 Geographic Coordinates (lat, lon):\n"
        "    map.DrawGeoLine({{53.8900, 9.1433}}, {{54.1825, 7.8853}}, routeColor, 1.8f);\n"
        "    map.DrawSeamark({{54.1825, 7.8853}}, {{\n"
        "        .id = \"WP-HELGOLAND\",\n"
        "        .title = \"Helgoland Light (North Sea)\",\n"
        "        .category = \"IALA Major Lighthouse\"\n"
        "    }});\n"
        "}}",
        crsEnumNames[static_cast<size_t>(s_state.crs)],
        fmtEnumNames[static_cast<size_t>(s_state.coordFormat)],
        s_state.showTileGrid ? "true" : "false",
        s_state.showSubTileQuadtree ? "true" : "false",
        s_state.showNauticalGraticule ? "true" : "false",
        s_state.showBorderRulers ? "true" : "false",
        s_state.showScaleBarOverlay ? "true" : "false",
        s_state.showScrollbars ? "true" : "false",
        s_state.showControls ? "true" : "false"
    );
}

} // namespace guikit::demo
