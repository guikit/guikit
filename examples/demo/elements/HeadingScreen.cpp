#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

void RenderHeadingInspector(guikit::Context& ui, DemoContext& ctx) {
    if (ui.StepSlider(ctx.elemLabelVariant, {
        .label = "Title Color",
        .showTicks = true,
        .showLabels = true,
        .showValue = true,
        .disabled = (ctx.elemLabel.empty() && !ctx.elemShowIcon)
    }).changed) {
        ui.InvalidateAll();
    }

    ctx.RenderDescVariantPicker(ui, ctx.elemDescVariant, !ctx.elemDesc.empty());
    if (ui.Switch(ctx.elemWordWrap, {.label = "Subtitle Word Wrap", .disabled = ctx.elemDesc.empty()}).changed) {
        ui.InvalidateAll();
    }

    if (ui.StepSlider(ctx.elemBadgeVariant, {
        .label = "Badge Color",
        .showTicks = true,
        .showLabels = true,
        .showValue = true,
        .disabled = ctx.elemBadge.empty()
    }).changed) {
        ui.InvalidateAll();
    }

    if (ctx.elemStretchIdx == 2) {
        if (ui.Slider(ctx.elemFillPct, 10.0f, 100.0f, {.label = "Explicit Width", .valueFormat = "%.0f %%", .step = 5.0f}).changed) ui.InvalidateAll();
    } else {
        if (ui.Slider(ctx.elemWidth, 0.0f, 400.0f, {.label = "Explicit Width", .valueFormat = {(ctx.elemWidth > 0.0f) ? "%.0f px" : "Auto", {"Auto", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
    }
    if (ui.Slider(ctx.elemHeight, 0.0f, 320.0f, {.label = "Explicit Height", .valueFormat = {(ctx.elemHeight > 0.0f) ? "%.0f px" : "Auto", {"Auto", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();

    if (ui.Switch(ctx.elemDivider, {.label = "Show Divider Line"}).changed) ui.InvalidateAll();
    static constexpr std::array<std::string_view, 2> divStyles = {"Bottom", "Trailing"};
    size_t divStyleIdx = (ctx.elemDividerStyle == guikit::ElementDivider::Trailing) ? 1 : 0;
    if (ui.StepSlider(divStyleIdx, divStyles, {
        .label = "Divider Placement",
        .showTicks = true,
        .showLabels = true,
        .disabled = !ctx.elemDivider
    }).changed) {
        ctx.elemDividerStyle = (divStyleIdx == 1) ? guikit::ElementDivider::Trailing : guikit::ElementDivider::Bottom;
        ui.InvalidateAll();
    }
}

void RenderHeadingStage(guikit::Context& ui, DemoContext& ctx) {
    ui.Heading({
        .title = ctx.GetElementLabel(),
        .subtitle = ctx.GetElementDescription(),
        .icon = ctx.GetElementIcon(),
        .badge = ctx.GetElementBadge(),
        .size = ctx.controlsSize,
        .layout = ctx.elemLayout,
        .inlineOrder = ctx.elemInlineOrder,
        .elementAlign = ctx.elemAlign,
        .stretch = ctx.GetElementStretch(),
        .width = ctx.GetElementWidth(),
        .height = ctx.GetElementHeight(),
        .background = ctx.elemBackground,
        .borderWidth = ctx.elemBorderWidth,
        .borderRadius = ctx.GetElemBorderRadius(),
        .borderColor = ctx.GetElemBorderColor(),
        .divider = ctx.elemDivider,
        .dividerStyle = ctx.elemDividerStyle,
        .wordWrap = ctx.elemWordWrap,
        .disabled = ctx.elemDisabled
    });
}

std::string GenerateHeadingCode(const DemoContext& ctx) {
    return std::format(
        "ui.Heading({{\n"
        "    .title = {},\n"
        "    .subtitle = {},\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .size = guikit::UiSize::{},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .width = {},\n"
        "    .height = {},\n"
        "    .background = {},\n"
        "    .borderWidth = {:.1f}f,\n"
        "    .borderRadius = {},\n"
        "    .borderColor = {},\n"
        "    .divider = {},\n"
        "    .dividerStyle = guikit::ElementDivider::{},\n"
        "    .wordWrap = {},\n"
        "    .disabled = {}\n"
        "}});",
        ctx.FormatElementLabelCode(),
        ctx.FormatElementDescCode(),
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        SizeName(ctx.controlsSize),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.elemBackground ? "true" : "false",
        ctx.elemBorderWidth,
        ctx.FormatElemBorderRadiusCode(),
        ctx.FormatElemBorderColorCode(),
        ctx.elemDivider ? "true" : "false",
        ctx.FormatDividerStyleCode(),
        ctx.elemWordWrap ? "true" : "false",
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
