#include "../demo_context.hpp"
#include <format>

namespace guikit::demo {

void RenderHeadingInspector(guikit::Context& ui, DemoContext& ctx) {
    static constexpr std::array<std::string_view, 7> levelNames = {"Auto", "H1", "H2", "H3", "H4", "H5", "H6"};
    if (auto el = ui.Element({.label = "Heading Level"})) {
        if (ui.StepSlider(ctx.elemHeadingLevel, levelNames, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            ui.InvalidateAll();
        }
    }

    if (ctx.elemStretchIdx == 2) {
        if (auto el = ui.Element({.label = "Explicit Width"})) {
            if (ui.Slider(ctx.elemFillRatio, 0.10f, 1.00f, {.valueFormat = "%.2f", .step = 0.05f}).changed) ui.InvalidateAll();
        }
    } else {
        if (auto el = ui.Element({.label = "Explicit Width"})) {
            if (ui.Slider(ctx.elemWidth, 0.0f, 400.0f, {.valueFormat = {(ctx.elemWidth > 0.0f) ? "%.0f px" : "Auto", {"Auto", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
        }
    }

    static constexpr std::array<std::string_view, 3> sepStyles = {"None", "Bottom", "Trailing"};
    if (auto el = ui.Element({.label = "Separator"})) {
        if (ui.StepSlider(ctx.elemSeparator, sepStyles, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            ui.InvalidateAll();
        }
    }
    const bool sepDisabled = (ctx.elemSeparator == guikit::HeadingSeparator::None);
    if (auto el = ui.Element({.label = "Separator Color", .disabled = sepDisabled})) {
        if (ui.StepSlider(ctx.elemSeparatorVariant, {
            .showTicks = true,
            .showLabels = true,
            .disabled = sepDisabled
        }).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Separator Width", .disabled = sepDisabled})) {
        if (ui.Slider(ctx.elemSeparatorWidth, 0.5f, 4.0f, {
            .valueFormat = "%.1f px",
            .step = 0.5f,
            .disabled = sepDisabled
        }).changed) {
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
        .level = ctx.elemHeadingLevel,
        .layout = ctx.elemLayout,
        .inlineOrder = ctx.elemInlineOrder,
        .align = ctx.elemAlign,
        .stretch = ctx.GetElementStretch(),
        .width = ctx.GetElementWidth(),
        .background = ctx.elemBackground,
        .border = ctx.GetElemBorder(),
        .separator = ctx.GetHeadingDivider(),
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
        "    .level = {},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .align = guikit::Align::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .width = {},\n"
        "    .background = {},\n"
        "    .border = {},\n"
        "    .separator = {},\n"
        "    .disabled = {}\n"
        "}});",
        FormatUiSizePrefix(ctx.elemSize),
        ctx.FormatElementLabelCode(),
        ctx.FormatElementDescCode(),
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        ctx.FormatHeadingLevelCode(),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElemBackgroundCode(),
        ctx.FormatElemBorderCode(),
        ctx.FormatSeparatorCode(),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
