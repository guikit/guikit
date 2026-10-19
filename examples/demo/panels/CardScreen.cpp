#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

void RenderCardInspector(guikit::Context& ui, DemoContext& ctx) {
    if (auto el = ui.Element({.label = "Card Size"})) {
        if (ui.StepSlider(ctx.controlsSize, {.showTicks = true, .showLabels = true, .includeNone = true}).changed) ui.InvalidateAll();
    }
    ctx.RenderVariantPicker(ui);
    if (auto el = ui.Element({.label = "Corner Radius"})) {
        if (ui.Slider(ctx.cardRadius, 0.0f, 24.0f, {.valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Internal Padding"})) {
        if (ui.Slider(ctx.cardPadding, 0.0f, 32.0f, {.valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Content Spacing (Gap)"})) {
        if (ui.Slider(ctx.cardGap, 0.0f, 24.0f, {.valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Border Width"})) {
        if (ui.Slider(ctx.cardBorderWidth, 0.0f, 4.0f, {.valueFormat = "%.1f px"}).changed) ui.InvalidateAll();
    }
}

void RenderCardStage(guikit::Context& ui, DemoContext& ctx) {
    if (auto c = ui.Size(ctx.controlsSize).Card({
        .gap = ctx.cardGap,
        .padding = guikit::Padding(ctx.cardPadding),
        .border = {.width = ctx.cardBorderWidth, .radius = ctx.cardRadius}
    })) {
        ui.Size(ctx.controlsSize).Heading({
            .label = "Card Surface Component",
            .description = "Configurable padding, radius and spacing tokens",
            .icon = guikit::Icon::Copy(),
            .divider = true
        });
        if (ui.Size(ctx.controlsSize).Button({
            .text = "Inner Action",
            .icon = guikit::Icon::Play(),
            .color = ctx.GetControlColor()
        })) {}
    }
}

std::string GenerateCardCode(const DemoContext& ctx) {
    const std::string szPrefix = FormatUiSizePrefix(ctx.controlsSize);
    return std::format(
        "if (auto card = ui.{}Card({{\n"
        "    .gap = {:.1f}f,\n"
        "    .padding = guikit::Padding({:.1f}f),\n"
        "    .border = {{.width = {:.1f}f, .radius = {:.1f}f}},\n"
        "    .clipChildren = true,\n"
        "    .widthMode = guikit::SizeMode::Stretch,\n"
        "    .heightMode = guikit::SizeMode::Fit\n"
        "}})) {{\n"
        "    ui.{}Heading({{\n"
        "        .label = \"Card Surface Component\",\n"
        "        .description = \"Configurable padding, radius and spacing tokens\",\n"
        "        .icon = guikit::Icon::Copy(),\n"
        "        .divider = true\n"
        "    }});\n"
        "    if (ui.{}Button({{\n"
        "        .text = \"Inner Action\",\n"
        "        .icon = guikit::Icon::Play(),\n"
        "        .color = {}\n"
        "    }})) {{\n"
        "        // On click handler\n"
        "    }}\n"
        "}}",
        szPrefix,
        ctx.cardGap,
        ctx.cardPadding,
        ctx.cardBorderWidth,
        ctx.cardRadius,
        szPrefix,
        szPrefix,
        ctx.FormatControlColorCode()
    );
}

} // namespace guikit::demo
