#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct CheckboxState {
    bool state = true;
    bool indet = false;
};
CheckboxState s_state;
} // namespace

void RenderCheckboxInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({
        .label = "Checked State",
        .description = "Toggle checkbox checked boolean"
    })) {
        if (ui.Switch(s_state.state).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({
        .label = "Indeterminate Tri-State",
        .description = "Render horizontal dash inside checkbox"
    })) {
        if (ui.Switch(s_state.indet).changed) {
            ui.InvalidateAll();
        }
    }
}

void RenderCheckboxStage(guikit::Context& ui, DemoContext& ctx) {
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
            .indeterminate = s_state.indet
        });
    }
}

std::string GenerateCheckboxCode(const DemoContext& ctx) {
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
        "        .indeterminate = {}\n"
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
        s_state.indet ? "true" : "false"
    );
}

} // namespace guikit::demo
