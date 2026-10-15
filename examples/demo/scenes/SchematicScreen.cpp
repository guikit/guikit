#include "../DemoContext.hpp"
#include "SchematicPrimitives.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct SchematicState {
    guikit::SceneBackground bg = guikit::SceneBackground::Solid;
    static constexpr std::array<std::string_view, 4> bgNames = {
        "Solid", "Checker", "Dots", "Lines"
    };
    bool selectableItems = true;
    bool showControls = true;
    bool showScrollbars = true;
    float radius = 8.0f;
    float gridStep = 32.0f;
};
SchematicState s_state;
} // namespace

void RenderSchematicInspector(guikit::Context& ui, DemoContext&) {
    size_t bgIdx = static_cast<size_t>(s_state.bg);
    if (ui.StepSlider(bgIdx, SchematicState::bgNames, {.label = "Canvas Backdrop", .showTicks = true, .showLabels = true}).changed) {
        s_state.bg = static_cast<guikit::SceneBackground>(bgIdx);
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.selectableItems, {.label = "Interactive Item Selection"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showControls, {.label = "Floating Zoom & Inspector HUD"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showScrollbars, {.label = "2D Scrollbars (H + V)"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.radius, 0.0f, 20.0f, {.label = "Viewport Radius", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.gridStep, 12.0f, 64.0f, {.label = "Grid Step", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
}

void RenderSchematicStage(guikit::Context& ui, DemoContext& ctx) {
    const auto schemPal = ctx.previewLightMode
        ? SchematicPalette::VintagePaper()
        : SchematicPalette::DarkCad();
    if (auto scene = ui.Scene({
        .worldSize = guikit::Vec2{2200.0f, 920.0f},
        .background = s_state.bg,
        .gridStep = s_state.gridStep,
        .heightMode = guikit::SizeMode::Stretch,
        .selectableItems = s_state.selectableItems,
        .showScrollbars = s_state.showScrollbars,
        .showControls = s_state.showControls,
        .borderRadius = s_state.radius
    })) {
        RenderQuad303Schematic(scene, schemPal);
    }
}

std::string GenerateSchematicCode(const DemoContext& ctx) {
    return std::format(
        "if (auto scene = ui.Scene({{\n"
        "    .worldSize = guikit::Vec2{{2200.0f, 920.0f}},\n"
        "    .background = guikit::SceneBackground::{},\n"
        "    .gridStep = {:.1f}f,\n"
        "    .heightMode = guikit::SizeMode::Stretch,\n"
        "    .selectableItems = {},\n"
        "    .showScrollbars = {},\n"
        "    .showControls = {},\n"
        "    .borderRadius = {:.1f}f\n"
        "}})) {{\n"
        "    // Interactive BS 3939 Vector Schematic Primitives via scene.Item(...)\n"
        "    const auto pal = guikit::demo::SchematicPalette::{}();\n"
        "    guikit::demo::RenderQuad303Schematic(scene, pal);\n"
        "}}",
        SchematicState::bgNames[static_cast<size_t>(s_state.bg)],
        s_state.gridStep,
        s_state.selectableItems ? "true" : "false",
        s_state.showScrollbars ? "true" : "false",
        s_state.showControls ? "true" : "false",
        s_state.radius,
        ctx.previewLightMode ? "VintagePaper" : "DarkCad"
    );
}

} // namespace guikit::demo
