#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct ColorPickerDemoState {
    guikit::Color color{59, 130, 246, 235};
    bool showHex = true;
    bool showAlpha = true;
    bool showSwatches = true;
    bool inlinePanel = false;
};
ColorPickerDemoState s_state;
} // namespace

void RenderColorPickerInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({
        .label = "Inline Picker Panel",
        .description = "Embed full SV + Hue/Alpha panel inline vs Popover button"
    })) {
        if (ui.Switch(s_state.inlinePanel).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({
        .label = "Show #HEX Readout",
        .description = "Show formatted hex code inside closed swatch button",
        .disabled = s_state.inlinePanel
    })) {
        if (ui.Switch(s_state.showHex, {.disabled = s_state.inlinePanel}).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({
        .label = "Enable Alpha Channel",
        .description = "Include opacity slider and #RRGGBBAA format"
    })) {
        if (ui.Switch(s_state.showAlpha).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({
        .label = "Show Preset Swatches",
        .description = "Display quick-pick color palette row"
    })) {
        if (ui.Switch(s_state.showSwatches).changed) ui.InvalidateAll();
    }
}

void RenderColorPickerStage(guikit::Context& ui, DemoContext& ctx) {
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
        ui.Size(ctx.controlsSize).ColorPicker(s_state.color, {
            .stretch = ctx.GetElementStretch(),
            .width = ctx.GetElementWidth(),
            .height = ctx.GetElementHeight(),
            .background = ctx.GetControlBackground(NavCategory::ElemColorPicker),
            .border = ctx.GetCtrlBorder(),
            .showHex = s_state.showHex,
            .showAlpha = s_state.showAlpha,
            .showSwatches = s_state.showSwatches,
            .inlinePanel = s_state.inlinePanel,
            .disabled = ctx.elemDisabled
        });
    }
}

std::string GenerateColorPickerCode(const DemoContext& ctx) {
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
        "    ui.{}ColorPicker(traceColor, {{\n"
        "        .stretch = guikit::StretchMode::{},\n"
        "        .width = {},\n"
        "        .height = {},\n"
        "        .background = {},\n"
        "        .border = {},\n"
        "        .showHex = {},\n"
        "        .showAlpha = {},\n"
        "        .showSwatches = {},\n"
        "        .inlinePanel = {},\n"
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
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemColorPicker),
        ctx.FormatCtrlBorderCode(),
        s_state.showHex ? "true" : "false",
        s_state.showAlpha ? "true" : "false",
        s_state.showSwatches ? "true" : "false",
        s_state.inlinePanel ? "true" : "false",
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
