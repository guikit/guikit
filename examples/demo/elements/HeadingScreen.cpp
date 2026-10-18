#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

void RenderHeadingInspector(guikit::Context& ui, DemoContext& ctx) {
    if (ctx.elemStretchIdx == 2) {
        if (auto el = ui.Element({.label = "Explicit Width"})) {
            if (ui.Slider(ctx.elemFillPct, 10.0f, 100.0f, {.valueFormat = "%.0f %%", .step = 5.0f}).changed) ui.InvalidateAll();
        }
    } else {
        if (auto el = ui.Element({.label = "Explicit Width"})) {
            if (ui.Slider(ctx.elemWidth, 0.0f, 400.0f, {.valueFormat = {(ctx.elemWidth > 0.0f) ? "%.0f px" : "Auto", {"Auto", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Explicit Height"})) {
        if (ui.Slider(ctx.elemHeight, 0.0f, 320.0f, {.valueFormat = {(ctx.elemHeight > 0.0f) ? "%.0f px" : "Auto", {"Auto", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Show Divider Line"})) {
        if (ui.Switch(ctx.elemDivider).changed) ui.InvalidateAll();
    }
    static constexpr std::array<std::string_view, 2> divStyles = {"Bottom", "Trailing"};
    size_t divStyleIdx = (ctx.elemDividerStyle == guikit::HeadingDivider::Trailing) ? 1 : 0;
    if (auto el = ui.Element({.label = "Divider Placement", .disabled = !ctx.elemDivider})) {
        if (ui.StepSlider(divStyleIdx, divStyles, {
            .showTicks = true,
            .showLabels = true,
            .disabled = !ctx.elemDivider
        }).changed) {
            ctx.elemDividerStyle = (divStyleIdx == 1) ? guikit::HeadingDivider::Trailing : guikit::HeadingDivider::Bottom;
            ui.InvalidateAll();
        }
    }
}

void RenderHeadingStage(guikit::Context& ui, DemoContext& ctx) {
    ui.Size(ctx.elemSize).Heading({
        .label = ctx.GetElementLabel(),
        .description = ctx.GetElementDescription(),
        .icon = ctx.GetElementIcon(),
        .badge = ctx.GetElementBadge(),
        .layout = ctx.elemLayout,
        .inlineOrder = ctx.elemInlineOrder,
        .align = ctx.elemAlign,
        .stretch = ctx.GetElementStretch(),
        .width = ctx.GetElementWidth(),
        .height = ctx.GetElementHeight(),
        .background = ctx.elemBackground,
        .border = ctx.GetElemBorder(),
        .divider = ctx.elemDivider,
        .dividerStyle = ctx.elemDividerStyle,
        .disabled = ctx.elemDisabled
    });
}

std::string GenerateHeadingCode(const DemoContext& ctx) {
    return std::format(
        "ui.{}Heading({{\n"
        "    .label = {},\n"
        "    .description = {},\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .align = guikit::Align::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .width = {},\n"
        "    .height = {},\n"
        "    .background = {},\n"
        "    .border = {},\n"
        "    .divider = {},\n"
        "    .dividerStyle = guikit::HeadingDivider::{},\n"
        "    .disabled = {}\n"
        "}});",
        FormatUiSizePrefix(ctx.elemSize),
        ctx.FormatElementLabelCode(),
        ctx.FormatElementDescCode(),
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatElemBackgroundCode(),
        ctx.FormatElemBorderCode(),
        ctx.elemDivider ? "true" : "false",
        ctx.FormatDividerStyleCode(),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
