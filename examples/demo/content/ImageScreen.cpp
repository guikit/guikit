#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct ImageState {
    size_t sourceIdx = 0;
    static constexpr std::array<std::string_view, 2> sources = {
        "assets/images/architecture.svg",
        "assets/images/circuit.svg"
    };
    static constexpr std::array<std::string_view, 2> sourceNames = {
        "Architecture Diagram",
        "Circuit Schematic"
    };
    guikit::ObjectFit fit = guikit::ObjectFit::Contain;
    static constexpr std::array<std::string_view, 5> fitModeNames = {
        "Contain", "Cover", "Fill", "ScaleDown", "Original"
    };
    size_t stretchIdx = 2; // Fill
    float width = 360.0f;
    float fillPct = 85.0f;
    float height = 180.0f;
    bool autoHeight = true;
    float radius = 8.0f;
    float borderWidth = 1.0f;
    bool checkerboard = true;
    bool showLabel = true;
    bool showDesc = true;
    guikit::ElementLayout layout = guikit::ElementLayout::Stacked;
    guikit::Align elementAlign = guikit::Align::Center;
};
ImageState s_state;
} // namespace

void RenderImageInspector(guikit::Context& ui, DemoContext&) {
    if (ui.Select(s_state.sourceIdx, ImageState::sourceNames, {.label = "Vector SVG Asset"}).changed) ui.InvalidateAll();
    size_t fitIdx = static_cast<size_t>(s_state.fit);
    if (ui.StepSlider(fitIdx, ImageState::fitModeNames, {.label = "ObjectFit Policy", .showTicks = true, .showLabels = true}).changed) {
        s_state.fit = static_cast<guikit::ObjectFit>(fitIdx);
        ui.InvalidateAll();
    }
    if (ui.StepSlider(s_state.stretchIdx, kStretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
    if (s_state.stretchIdx == 2) {
        if (ui.Slider(s_state.fillPct, 20.0f, 100.0f, {.label = "Fill Width", .valueFormat = "%.0f %%", .step = 5.0f}).changed) ui.InvalidateAll();
    } else {
        if (ui.Slider(s_state.width, 120.0f, 560.0f, {.label = "Explicit Width", .valueFormat = "%.0f px", .step = 10.0f}).changed) ui.InvalidateAll();
    }
    if (ui.Switch(s_state.autoHeight, {.label = "Auto Aspect-Ratio Height"}).changed) ui.InvalidateAll();
    if (!s_state.autoHeight) {
        if (ui.Slider(s_state.height, 80.0f, 320.0f, {.label = "Explicit Height", .valueFormat = "%.0f px", .step = 10.0f}).changed) ui.InvalidateAll();
    }
    if (ui.Slider(s_state.radius, 0.0f, 24.0f, {.label = "Corner Radius", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.borderWidth, 0.0f, 4.0f, {.label = "Border Width", .valueFormat = "%.1f px"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.checkerboard, {.label = "Checkerboard Backdrop"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
}

void RenderImageStage(guikit::Context& ui, DemoContext& ctx) {
    ui.Image(ImageState::sources[s_state.sourceIdx % ImageState::sources.size()], {
        .label = s_state.showLabel ? "A vector image" : "",
        .description = s_state.showDesc ? "This is a vector image" : "",
        .fit = s_state.fit,
        .layout = s_state.layout,
        .elementAlign = s_state.elementAlign,
        .stretch = static_cast<guikit::StretchMode>(s_state.stretchIdx),
        .width = (s_state.stretchIdx == 2) ? std::optional<float>(s_state.fillPct) : std::optional<float>(s_state.width),
        .height = s_state.autoHeight ? std::nullopt : std::optional<float>(s_state.height),
        .borderRadius = s_state.radius,
        .borderWidth = s_state.borderWidth,
        .checkerboard = s_state.checkerboard,
        .size = ctx.controlsSize
    });
}

std::string GenerateImageCode(const DemoContext& ctx) {
    return std::format(
        "ui.Image(\"{}\", {{\n"
        "    .label = \"{}\",\n"
        "    .description = \"{}\",\n"
        "    .fit = guikit::ObjectFit::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .width = {:.1f}f,\n"
        "    .height = {},\n"
        "    .borderRadius = {:.1f}f,\n"
        "    .borderWidth = {:.1f}f,\n"
        "    .checkerboard = {},\n"
        "    .size = guikit::UiSize::{}\n"
        "}});",
        ImageState::sources[s_state.sourceIdx % ImageState::sources.size()],
        s_state.showLabel ? "A vector image" : "",
        s_state.showDesc ? "This is a vector image" : "",
        ImageState::fitModeNames[static_cast<size_t>(s_state.fit)],
        StretchModeEnumName(static_cast<guikit::StretchMode>(s_state.stretchIdx)),
        (s_state.stretchIdx == 2) ? s_state.fillPct : s_state.width,
        s_state.autoHeight ? "std::nullopt" : std::format("{:.1f}f", s_state.height),
        s_state.radius,
        s_state.borderWidth,
        s_state.checkerboard ? "true" : "false",
        SizeName(ctx.controlsSize)
    );
}

} // namespace guikit::demo
