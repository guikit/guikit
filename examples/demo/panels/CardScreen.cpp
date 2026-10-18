#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

void RenderCardInspector(guikit::Context& ui, DemoContext& ctx) {
    if (ui.StepSlider(ctx.controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
    ctx.RenderVariantPicker(ui);
    if (ui.Slider(ctx.cardRadius, 0.0f, 24.0f, {.label = "Corner Radius", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Slider(ctx.cardPadding, 0.0f, 32.0f, {.label = "Internal Padding", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Slider(ctx.cardGap, 0.0f, 24.0f, {.label = "Content Spacing (Gap)", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Slider(ctx.cardBorderWidth, 0.0f, 4.0f, {.label = "Border Width", .valueFormat = "%.1f px"}).changed) ui.InvalidateAll();
}

void RenderCardStage(guikit::Context& ui, DemoContext& ctx) {
    if (auto c = ui.Card({
        .gap = ctx.cardGap,
        .padding = guikit::Padding(ctx.cardPadding),
        .borderWidth = ctx.cardBorderWidth,
        .borderRadius = ctx.cardRadius,
        .size = ctx.controlsSize
    })) {
        ui.Heading({
            .title = "Card Surface Component",
            .subtitle = "Configurable padding, radius and spacing tokens",
            .icon = guikit::Icon::Copy(),
            .size = ctx.controlsSize,
            .divider = true
        });
        if (ui.Button({
            .text = "Inner Action",
            .icon = guikit::Icon::Play(),
            .color = ctx.GetControlColor(),
            .size = ctx.controlsSize
        })) {}
    }
}

std::string GenerateCardCode(const DemoContext& ctx) {
    return std::format(
        "if (auto card = ui.Card({{\n"
        "    .gap = {:.1f}f,\n"
        "    .padding = guikit::Padding({:.1f}f),\n"
        "    .borderWidth = {:.1f}f,\n"
        "    .borderRadius = {:.1f}f,\n"
        "    .clipChildren = true,\n"
        "    .widthMode = guikit::SizeMode::Stretch,\n"
        "    .heightMode = guikit::SizeMode::Fit,\n"
        "    .size = guikit::UiSize::{}\n"
        "}})) {{\n"
        "    ui.Heading({{\n"
        "        .title = \"Card Surface Component\",\n"
        "        .subtitle = \"Configurable padding, radius and spacing tokens\",\n"
        "        .icon = guikit::Icon::Copy(),\n"
        "        .size = guikit::UiSize::{},\n"
        "        .divider = true\n"
        "    }});\n"
        "    if (ui.Button({{\n"
        "        .text = \"Inner Action\",\n"
        "        .icon = guikit::Icon::Play(),\n"
        "        .color = {},\n"
        "        .size = guikit::UiSize::{}\n"
        "    }})) {{\n"
        "        // On click handler\n"
        "    }}\n"
        "}}",
        ctx.cardGap,
        ctx.cardPadding,
        ctx.cardBorderWidth,
        ctx.cardRadius,
        SizeName(ctx.controlsSize),
        SizeName(ctx.controlsSize),
        ctx.FormatControlColorCode(),
        SizeName(ctx.controlsSize)
    );
}

} // namespace guikit::demo
