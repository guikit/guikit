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
    if (ui.Switch(s_state.radio, {
        .label = "Radio Button Style",
        .description = "Circular radio indicator vs Square checkbox"
    }).changed) {
        ui.InvalidateAll();
    }
    if (s_state.radio) {
        if (ui.Switch(s_state.radioGroupDemo, {
            .label = "Preview 3-Option Radio Group",
            .description = "Test ui.RadioButton(activeVal, optionVal)"
        }).changed) {
            ui.InvalidateAll();
        }
    } else {
        if (ui.Switch(s_state.indet, {.label = "Indeterminate Tri-State"}).changed) {
            ui.InvalidateAll();
        }
    }
}

void RenderCheckboxStage(guikit::Context& ui, DemoContext& ctx) {
    if (s_state.radio && s_state.radioGroupDemo) {
        if (auto col = ui.Column({.gap = 10.0f})) {
            ui.RadioButton(s_state.selectedRadioIdx, 0, {
                .label = guikit::UiText{ctx.elemLabel.empty() ? "Option A — Automatic Gain (AGC)" : ctx.elemLabel, ctx.elemLabelVariant},
                .description = ctx.GetElementDescription(),
                .icon = ctx.GetElementIcon(),
                .badge = ctx.GetElementBadge(),
                .variant = ctx.btnVariant,
                .layout = ctx.elemLayout,
                .inlineOrder = ctx.elemInlineOrder,
                .elementAlign = ctx.elemAlign,
                .stretch = ctx.GetElementStretch(),
                .background = ctx.elemBackground,
                .borderWidth = ctx.elemBorderWidth,
                .borderRadius = ctx.GetElemBorderRadius(),
                .borderColor = ctx.GetElemBorderColor(),
                .control = ctx.GetControlSpec(NavCategory::ElemCheckbox),
                .disabled = ctx.elemDisabled,
                .size = ctx.controlsSize
            });
            ui.RadioButton(s_state.selectedRadioIdx, 1, {
                .label = guikit::UiText{"Option B — Manual Fixed Gain", ctx.elemLabelVariant},
                .description = ctx.GetElementDescription(),
                .icon = ctx.GetElementIcon(),
                .badge = ctx.GetElementBadge(),
                .variant = ctx.btnVariant,
                .layout = ctx.elemLayout,
                .inlineOrder = ctx.elemInlineOrder,
                .elementAlign = ctx.elemAlign,
                .stretch = ctx.GetElementStretch(),
                .background = ctx.elemBackground,
                .borderWidth = ctx.elemBorderWidth,
                .borderRadius = ctx.GetElemBorderRadius(),
                .borderColor = ctx.GetElemBorderColor(),
                .control = ctx.GetControlSpec(NavCategory::ElemCheckbox),
                .disabled = ctx.elemDisabled,
                .size = ctx.controlsSize
            });
            ui.RadioButton(s_state.selectedRadioIdx, 2, {
                .label = guikit::UiText{"Option C — External Hardware Trigger", ctx.elemLabelVariant},
                .description = ctx.GetElementDescription(),
                .icon = ctx.GetElementIcon(),
                .badge = ctx.GetElementBadge(),
                .variant = ctx.btnVariant,
                .layout = ctx.elemLayout,
                .inlineOrder = ctx.elemInlineOrder,
                .elementAlign = ctx.elemAlign,
                .stretch = ctx.GetElementStretch(),
                .background = ctx.elemBackground,
                .borderWidth = ctx.elemBorderWidth,
                .borderRadius = ctx.GetElemBorderRadius(),
                .borderColor = ctx.GetElemBorderColor(),
                .control = ctx.GetControlSpec(NavCategory::ElemCheckbox),
                .disabled = ctx.elemDisabled,
                .size = ctx.controlsSize
            });
        }
    } else {
        ui.Checkbox(s_state.state, {
            .label = ctx.GetElementLabel(),
            .description = ctx.GetElementDescription(),
            .icon = ctx.GetElementIcon(),
            .badge = ctx.GetElementBadge(),
            .variant = ctx.btnVariant,
            .layout = ctx.elemLayout,
            .inlineOrder = ctx.elemInlineOrder,
            .elementAlign = ctx.elemAlign,
            .stretch = ctx.GetElementStretch(),
            .background = ctx.elemBackground,
            .borderWidth = ctx.elemBorderWidth,
            .borderRadius = ctx.GetElemBorderRadius(),
            .borderColor = ctx.GetElemBorderColor(),
            .control = ctx.GetControlSpec(NavCategory::ElemCheckbox),
            .disabled = ctx.elemDisabled,
            .indeterminate = s_state.indet,
            .radio = s_state.radio,
            .size = ctx.controlsSize
        });
    }
}

std::string GenerateCheckboxCode(const DemoContext& ctx) {
    if (s_state.radio && s_state.radioGroupDemo) {
        return std::format(
            "ui.RadioButton(selectedMode, 0, {{\n"
            "    .label = {},\n"
            "    .description = {},\n"
            "    .icon = {},\n"
            "    .badge = {},\n"
            "    .variant = guikit::UiVariant::{},\n"
            "    .layout = guikit::ElementLayout::{},\n"
            "    .inlineOrder = guikit::InlineOrder::{},\n"
            "    .elementAlign = guikit::Align::{},\n"
            "    .stretch = guikit::StretchMode::{},\n"
            "    .background = {},\n"
            "    .borderWidth = {:.1f}f,\n"
            "    .borderRadius = {},\n"
            "    .borderColor = {},\n"
            "    .control = {},\n"
            "    .disabled = {},\n"
            "    .size = guikit::UiSize::{}\n"
            "}});",
            ctx.FormatElementLabelCode(),
            ctx.FormatElementDescCode(),
            ctx.FormatElementIconCode(),
            ctx.FormatElementBadgeCode(),
            VariantName(ctx.btnVariant),
            ElementLayoutEnumName(ctx.elemLayout),
            InlineOrderEnumName(ctx.elemInlineOrder),
            LabelAlignEnumName(ctx.elemAlign),
            StretchModeEnumName(ctx.GetElementStretch()),
            ctx.elemBackground ? "true" : "false",
            ctx.elemBorderWidth,
            ctx.FormatElemBorderRadiusCode(),
            ctx.FormatElemBorderColorCode(),
            ctx.FormatControlSpecCode(NavCategory::ElemCheckbox),
            ctx.elemDisabled ? "true" : "false",
            SizeName(ctx.controlsSize)
        );
    }
    return std::format(
        "ui.Checkbox(checkState, {{\n"
        "    .label = {},\n"
        "    .description = {},\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .background = {},\n"
        "    .borderWidth = {:.1f}f,\n"
        "    .borderRadius = {},\n"
        "    .borderColor = {},\n"
        "    .control = {},\n"
        "    .disabled = {},\n"
        "    .indeterminate = {},\n"
        "    .radio = {},\n"
        "    .size = guikit::UiSize::{}\n"
        "}});",
        ctx.FormatElementLabelCode(),
        ctx.FormatElementDescCode(),
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        VariantName(ctx.btnVariant),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.elemBackground ? "true" : "false",
        ctx.elemBorderWidth,
        ctx.FormatElemBorderRadiusCode(),
        ctx.FormatElemBorderColorCode(),
        ctx.FormatControlSpecCode(NavCategory::ElemCheckbox),
        ctx.elemDisabled ? "true" : "false",
        s_state.indet ? "true" : "false",
        s_state.radio ? "true" : "false",
        SizeName(ctx.controlsSize)
    );
}

} // namespace guikit::demo
