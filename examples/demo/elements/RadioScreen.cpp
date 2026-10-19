#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct RadioState {
    bool groupDemo = true;
    int selectedIdx = 0;
    bool singleChecked = true;
};
RadioState s_state;
} // namespace

void RenderRadioInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({
        .label = "3-Option Radio Group",
        .description = "Preview mutually exclusive group vs single Radio"
    })) {
        if (ui.Switch(s_state.groupDemo).changed) {
            ui.InvalidateAll();
        }
    }
    if (s_state.groupDemo) {
        static constexpr std::array<std::string_view, 3> optNames = {"Opt A", "Opt B", "Opt C"};
        size_t idx = static_cast<size_t>(std::clamp(s_state.selectedIdx, 0, 2));
        if (auto el = ui.Element({.label = "Active Selection"})) {
            if (ui.StepSlider(idx, optNames, {.showTicks = true, .showLabels = true}).changed) {
                s_state.selectedIdx = static_cast<int>(idx);
                ui.InvalidateAll();
            }
        }
    } else {
        if (auto el = ui.Element({.label = "Selected State"})) {
            if (ui.Switch(s_state.singleChecked).changed) {
                ui.InvalidateAll();
            }
        }
    }
}

void RenderRadioStage(guikit::Context& ui, DemoContext& ctx) {
    const guikit::RadioOptions ctrlOpts{
        .color = ctx.GetControlColor(),
        .stretch = ctx.GetElementStretch(),
        .width = ctx.GetElementWidth(),
        .height = ctx.GetElementHeight(),
        .background = ctx.GetControlBackground(NavCategory::ElemRadio),
        .border = ctx.GetCtrlBorder(),
        .disabled = ctx.elemDisabled
    };
    if (s_state.groupDemo) {
        if (auto col = ui.Column({.gap = 10.0f})) {
            if (auto el = ui.Size(ctx.elemSize).Element({
                .label = guikit::UiText{ctx.elemLabel.empty() ? "Option A — Automatic Gain (AGC)" : ctx.elemLabel, ctx.elemLabelVariant},
                .description = ctx.GetElementDescription(),
                .icon = ctx.GetElementIcon(),
                .badge = ctx.GetElementBadge(),
                .layout = ctx.elemLayout,
                .inlineOrder = ctx.elemInlineOrder,
                .align = ctx.elemAlign,
                .stretch = ctx.GetElementStretch(),
                .background = ctx.elemBackground,
                .border = ctx.GetElemBorder(),
                .disabled = ctx.elemDisabled
            })) {
                ui.Size(ctx.controlsSize).Radio(s_state.selectedIdx, 0, ctrlOpts);
            }
            if (auto el = ui.Size(ctx.elemSize).Element({
                .label = guikit::UiText{"Option B — Manual Fixed Gain", ctx.elemLabelVariant},
                .description = ctx.GetElementDescription(),
                .icon = ctx.GetElementIcon(),
                .badge = ctx.GetElementBadge(),
                .layout = ctx.elemLayout,
                .inlineOrder = ctx.elemInlineOrder,
                .align = ctx.elemAlign,
                .stretch = ctx.GetElementStretch(),
                .background = ctx.elemBackground,
                .border = ctx.GetElemBorder(),
                .disabled = ctx.elemDisabled
            })) {
                ui.Size(ctx.controlsSize).Radio(s_state.selectedIdx, 1, ctrlOpts);
            }
            if (auto el = ui.Size(ctx.elemSize).Element({
                .label = guikit::UiText{"Option C — External Hardware Trigger", ctx.elemLabelVariant},
                .description = ctx.GetElementDescription(),
                .icon = ctx.GetElementIcon(),
                .badge = ctx.GetElementBadge(),
                .layout = ctx.elemLayout,
                .inlineOrder = ctx.elemInlineOrder,
                .align = ctx.elemAlign,
                .stretch = ctx.GetElementStretch(),
                .background = ctx.elemBackground,
                .border = ctx.GetElemBorder(),
                .disabled = ctx.elemDisabled
            })) {
                ui.Size(ctx.controlsSize).Radio(s_state.selectedIdx, 2, ctrlOpts);
            }
        }
    } else {
        if (auto el = ui.Size(ctx.elemSize).Element({
            .label = ctx.GetElementLabel(),
            .description = ctx.GetElementDescription(),
            .icon = ctx.GetElementIcon(),
            .badge = ctx.GetElementBadge(),
            .layout = ctx.elemLayout,
            .inlineOrder = ctx.elemInlineOrder,
            .align = ctx.elemAlign,
            .stretch = ctx.GetElementStretch(),
            .background = ctx.elemBackground,
            .border = ctx.GetElemBorder(),
            .disabled = ctx.elemDisabled
        })) {
            ui.Size(ctx.controlsSize).Radio(s_state.singleChecked, ctrlOpts);
        }
    }
}

std::string GenerateRadioCode(const DemoContext& ctx) {
    if (s_state.groupDemo) {
        return std::format(
            "if (auto el = ui.{}Element({{\n"
            "    .label = {},\n"
            "    .description = {},\n"
            "    .icon = {},\n"
            "    .badge = {},\n"
            "    .layout = guikit::ElementLayout::{},\n"
            "    .inlineOrder = guikit::InlineOrder::{},\n"
            "    .align = guikit::Align::{},\n"
            "    .stretch = guikit::StretchMode::{},\n"
            "    .background = {},\n"
            "    .border = {},\n"
            "    .disabled = {}\n"
            "}})) {{\n"
            "    ui.{}Radio(selectedMode, 0, {{\n"
            "        .color = {},\n"
            "        .stretch = guikit::StretchMode::{},\n"
            "        .width = {},\n"
            "        .height = {},\n"
            "        .background = {},\n"
            "        .border = {},\n"
            "        .disabled = {}\n"
            "    }});\n"
            "}}",
            FormatUiSizePrefix(ctx.elemSize),
            ctx.FormatElementLabelCode(),
            ctx.FormatElementDescCode(),
            ctx.FormatElementIconCode(),
            ctx.FormatElementBadgeCode(),
            ElementLayoutEnumName(ctx.elemLayout),
            InlineOrderEnumName(ctx.elemInlineOrder),
            LabelAlignEnumName(ctx.elemAlign),
            StretchModeEnumName(ctx.GetElementStretch()),
            ctx.FormatElemBackgroundCode(),
            ctx.FormatElemBorderCode(),
            ctx.elemDisabled ? "true" : "false",
            FormatUiSizePrefix(ctx.controlsSize),
            ctx.FormatControlColorCode(),
            StretchModeEnumName(ctx.GetElementStretch()),
            ctx.FormatElementWidthCode(),
            ctx.FormatElementHeightCode(),
            ctx.FormatControlBackgroundCode(NavCategory::ElemRadio),
            ctx.FormatCtrlBorderCode(),
            ctx.elemDisabled ? "true" : "false"
        );
    }
    return std::format(
        "if (auto el = ui.{}Element({{\n"
        "    .label = {},\n"
        "    .description = {},\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .align = guikit::Align::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .background = {},\n"
        "    .border = {},\n"
        "    .disabled = {}\n"
        "}})) {{\n"
        "    ui.{}Radio(radioSelected, {{\n"
        "        .color = {},\n"
        "        .stretch = guikit::StretchMode::{},\n"
        "        .width = {},\n"
        "        .height = {},\n"
        "        .background = {},\n"
        "        .border = {},\n"
        "        .disabled = {}\n"
        "    }});\n"
        "}}",
        FormatUiSizePrefix(ctx.elemSize),
        ctx.FormatElementLabelCode(),
        ctx.FormatElementDescCode(),
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElemBackgroundCode(),
        ctx.FormatElemBorderCode(),
        ctx.elemDisabled ? "true" : "false",
        FormatUiSizePrefix(ctx.controlsSize),
        ctx.FormatControlColorCode(),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemRadio),
        ctx.FormatCtrlBorderCode(),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
