#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct CheckboxState {
    bool state = true;
    bool indet = false;
    bool radio = false;
    bool radioGroupDemo = false;
    int selectedRadioIdx = 0;
};
CheckboxState s_state;
} // namespace

void RenderCheckboxInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({
        .label = "Radio Button Style",
        .description = "Circular radio indicator vs Square checkbox"
    })) {
        if (ui.Switch(s_state.radio).changed) {
            ui.InvalidateAll();
        }
    }
    if (s_state.radio) {
        if (auto el = ui.Element({
            .label = "Preview 3-Option Radio Group",
            .description = "Test ui.RadioButton(activeVal, optionVal)"
        })) {
            if (ui.Switch(s_state.radioGroupDemo).changed) {
                ui.InvalidateAll();
            }
        }
    } else {
        if (auto el = ui.Element({.label = "Indeterminate Tri-State"})) {
            if (ui.Switch(s_state.indet).changed) {
                ui.InvalidateAll();
            }
        }
    }
}

void RenderCheckboxStage(guikit::Context& ui, DemoContext& ctx) {
    const guikit::CheckboxOptions ctrlOpts{
        .color = ctx.GetControlColor(),
        .stretch = ctx.GetElementStretch(),
        .width = ctx.GetElementWidth(),
        .height = ctx.GetElementHeight(),
        .background = ctx.GetControlBackground(NavCategory::ElemCheckbox),
        .border = ctx.GetCtrlBorder(),
        .disabled = ctx.elemDisabled
    };
    if (s_state.radio && s_state.radioGroupDemo) {
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
                ui.Size(ctx.controlsSize).RadioButton(s_state.selectedRadioIdx, 0, ctrlOpts);
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
                ui.Size(ctx.controlsSize).RadioButton(s_state.selectedRadioIdx, 1, ctrlOpts);
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
                ui.Size(ctx.controlsSize).RadioButton(s_state.selectedRadioIdx, 2, ctrlOpts);
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
            ui.Size(ctx.controlsSize).Checkbox(s_state.state, {
                .color = ctx.GetControlColor(),
                .stretch = ctx.GetElementStretch(),
                .width = ctx.GetElementWidth(),
                .height = ctx.GetElementHeight(),
                .background = ctx.GetControlBackground(NavCategory::ElemCheckbox),
                .border = ctx.GetCtrlBorder(),
                .disabled = ctx.elemDisabled,
                .indeterminate = s_state.indet,
                .radio = s_state.radio
            });
        }
    }
}

std::string GenerateCheckboxCode(const DemoContext& ctx) {
    if (s_state.radio && s_state.radioGroupDemo) {
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
            "    ui.{}RadioButton(selectedMode, 0, {{\n"
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
            ctx.FormatControlBackgroundCode(NavCategory::ElemCheckbox),
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
        "    ui.{}Checkbox(checkState, {{\n"
        "        .color = {},\n"
        "        .stretch = guikit::StretchMode::{},\n"
        "        .width = {},\n"
        "        .height = {},\n"
        "        .background = {},\n"
        "        .border = {},\n"
        "        .disabled = {},\n"
        "        .indeterminate = {},\n"
        "        .radio = {}\n"
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
        ctx.FormatControlBackgroundCode(NavCategory::ElemCheckbox),
        ctx.FormatCtrlBorderCode(),
        ctx.elemDisabled ? "true" : "false",
        s_state.indet ? "true" : "false",
        s_state.radio ? "true" : "false"
    );
}

} // namespace guikit::demo
