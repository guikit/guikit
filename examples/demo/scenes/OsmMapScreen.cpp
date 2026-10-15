#include "../DemoContext.hpp"
#include "OsmMapPrimitives.hpp"
#include <format>
#include <optional>

namespace guikit::demo {

namespace {
struct OsmMapState {
    OsmMapRenderOptions opts{};
    guikit::MapViewState lastView{};
    bool showControls = true;
    bool showScrollbars = true;
    bool selectableItems = true;
    float radius = 8.0f;
    bool cameraInitialized = false;
    std::optional<guikit::GeoCoord> pendingCenterGeo{};
    std::optional<float> pendingSceneZoom{};
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
            s_state.pendingSceneZoom = guikit::MapGrid::TileZoomToSceneZoom(wp.defaultTileZoom, s_state.opts.baseZoom);
        } else {
            s_state.pendingCenterGeo = guikit::GeoCoord{20.0, 0.0};
            s_state.pendingSceneZoom = guikit::MapGrid::TileZoomToSceneZoom(2.0f, s_state.opts.baseZoom);
        }
        ui.InvalidateAll();
    }

    size_t crsIdx = static_cast<size_t>(s_state.opts.crs);
    if (ui.StepSlider(crsIdx, OsmMapState::crsShortNames, {
        .label = "Projection (CRS)",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        const guikit::GeoCoord keepCenter = s_state.lastView.center;
        s_state.opts.crs = static_cast<guikit::MapCrs>(crsIdx);
        s_state.pendingCenterGeo = keepCenter;
        ui.InvalidateAll();
    }

    size_t fmtIdx = static_cast<size_t>(s_state.opts.coordFormat);
    if (ui.StepSlider(fmtIdx, OsmMapState::coordFormatNames, {
        .label = "Coordinate Format",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        s_state.opts.coordFormat = static_cast<guikit::GeoCoordFormat>(fmtIdx);
        ui.InvalidateAll();
    }

    if (ui.Switch(s_state.opts.showTileGrid, {.label = "OSM Tile Grid [z/x/y]"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.opts.showSubTileQuadtree, {.label = "2x2 Sub-Tile Quadtree", .disabled = !s_state.opts.showTileGrid}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.opts.showNauticalGraticule, {.label = "Nautical Graticule (OpenSeaMap)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.opts.showBorderRulers, {.label = "Border Lat/Lon Rulers (1' = 1 NM)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.opts.showSeamarksAndCourses, {.label = "IALA Seamarks & Rhumb Courses"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.opts.showScaleBarOverlay, {.label = "Dual Scale Bar (NM + km)"}).changed) ui.InvalidateAll();
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
                      guikit::MapGrid::FormatCoord(s_state.lastView.center, s_state.opts.coordFormat)),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Secondary}
        );
        const auto curTile = guikit::MapGrid::GeoToTileId(s_state.lastView.cursor, s_state.lastView.tileZoom, s_state.opts.crs);
        ui.Text(
            ui.Format("Cursor: {} [/{}/{}/{}]",
                      guikit::MapGrid::FormatCoord(s_state.lastView.cursor, s_state.opts.coordFormat),
                      curTile.z,
                      curTile.x,
                      curTile.y),
            {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Success}
        );
    }
}

void RenderOsmMapStage(guikit::Context& ui, DemoContext& ctx) {
    s_state.opts.lightMode = ctx.previewLightMode;
    if (auto scene = ui.Scene({
        .worldSize = guikit::Vec2{s_state.opts.worldSize, s_state.opts.worldSize},
        .background = guikit::SceneBackground::Solid,
        .heightMode = guikit::SizeMode::Stretch,
        .minZoom = 0.25f,
        .maxZoom = 4096.0f,
        .selectableItems = s_state.selectableItems,
        .showScrollbars = s_state.showScrollbars,
        .showControls = s_state.showControls,
        .borderRadius = s_state.radius
    })) {
        if (!s_state.cameraInitialized) {
            s_state.cameraInitialized = true;
            const auto& wp = kMaritimeWaypoints[0];
            const guikit::Vec2 helgolandWorld = guikit::MapGrid::GeoToWorld(wp.coord, s_state.opts.worldSize, s_state.opts.crs);
            const float initSceneZoom = guikit::MapGrid::TileZoomToSceneZoom(7.0f, s_state.opts.baseZoom);
            scene.CenterOn(helgolandWorld, initSceneZoom);
        } else if (s_state.pendingCenterGeo.has_value()) {
            const guikit::Vec2 targetWorld = guikit::MapGrid::GeoToWorld(*s_state.pendingCenterGeo, s_state.opts.worldSize, s_state.opts.crs);
            scene.CenterOn(targetWorld, s_state.pendingSceneZoom);
            s_state.pendingCenterGeo.reset();
            s_state.pendingSceneZoom.reset();
        }

        s_state.lastView = RenderOsmSlippyMap(scene, s_state.opts);
    }
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
        "guikit::MapGridConfig mapCfg{{\n"
        "    .worldSize = 2048.0f,\n"
        "    .baseZoom = 3,\n"
        "    .crs = guikit::MapCrs::{}\n"
        "}};\n"
        "if (auto scene = ui.Scene({{\n"
        "    .worldSize = {{mapCfg.worldSize, mapCfg.worldSize}},\n"
        "    .minZoom = 0.25f, .maxZoom = 4096.0f,\n"
        "    .showScrollbars = {}, .showControls = {}\n"
        "}})) {{\n"
        "    const auto view = guikit::MapGrid::ComputeView(\n"
        "        scene.GetVisibleWorldRect(), scene.GetZoom(),\n"
        "        scene.GetWorldMousePos(), scene.IsHovered(), mapCfg\n"
        "    );\n"
        "    // Optional TileSourceCallback for PNG/JPG from SQLite / Disk / HTTP:\n"
        "    for (const auto& cell : view.visibleTiles) {{\n"
        "        if (auto uri = tileSource ? tileSource(cell.id, 0) : std::nullopt) {{\n"
        "            scene.DrawImage(*uri, cell.worldRect);\n"
        "        }}\n"
        "    }}\n"
        "    // Nautical Graticule (1' = 1 NM) + Coord Format: guikit::GeoCoordFormat::{}\n"
        "}}",
        crsEnumNames[static_cast<size_t>(s_state.opts.crs)],
        s_state.showScrollbars ? "true" : "false",
        s_state.showControls ? "true" : "false",
        fmtEnumNames[static_cast<size_t>(s_state.opts.coordFormat)]
    );
}

} // namespace guikit::demo
