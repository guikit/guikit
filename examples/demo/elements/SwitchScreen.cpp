#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct SwitchState {
    bool state = true;
    std::string textLeft = "";
    std::string textRight = "";
    bool symmetric = false;
};
SwitchState s_state;
} // namespace

void RenderSwitchInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({.label = "Text Left (Off)"})) {
        if (ui.InputField(s_state.textLeft, {
            .placeholder = "e.g. Off",
            .clearable = true
        }).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Text Right (On)"})) {
        if (ui.InputField(s_state.textRight, {
            .placeholder = "e.g. On",
            .clearable = true
        }).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Symmetric (Neutral Track)"})) {
        if (ui.Switch(s_state.symmetric).changed) ui.InvalidateAll();
    }
}

void RenderSwitchStage(guikit::Context& ui, DemoContext& ctx) {
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
        ui.Size(ctx.controlsSize).Switch(s_state.state, {
            .textLeft = s_state.textLeft,
            .textRight = s_state.textRight,
            .color = ctx.GetControlColor(),
            .symmetric = s_state.symmetric,
            .stretch = ctx.GetElementStretch(),
            .width = ctx.GetElementWidth(),
            .height = ctx.GetElementHeight(),
            .background = ctx.GetControlBackground(NavCategory::ElemSwitch),
            .border = ctx.GetCtrlBorder(),
            .disabled = ctx.elemDisabled
        });
    }
}

std::string GenerateSwitchCode(const DemoContext& ctx) {
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
        "    ui.{}Switch(switchState, {{\n"
        "        .textLeft = \"{}\",\n"
        "        .textRight = \"{}\",\n"
        "        .color = {},\n"
        "        .symmetric = {},\n"
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
        s_state.textLeft,
        s_state.textRight,
        ctx.FormatControlColorCode(),
        s_state.symmetric ? "true" : "false",
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemSwitch),
        ctx.FormatCtrlBorderCode(),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
