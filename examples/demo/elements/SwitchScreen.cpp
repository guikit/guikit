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
    if (ui.InputField(s_state.textLeft, {
        .label = "Text Left (Off)",
        .placeholder = "e.g. Off",
        .clearable = true
    }).changed) ui.InvalidateAll();
    if (ui.InputField(s_state.textRight, {
        .label = "Text Right (On)",
        .placeholder = "e.g. On",
        .clearable = true
    }).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.symmetric, {.label = "Symmetric (Neutral Track)"}).changed) ui.InvalidateAll();
}

void RenderSwitchStage(guikit::Context& ui, DemoContext& ctx) {
    ui.Switch(s_state.state, {
        .label = ctx.elemLabel,
        .textLeft = s_state.textLeft,
        .textRight = s_state.textRight,
        .description = ctx.GetElementDescription(),
        .icon = ctx.GetElementIcon(),
        .badge = ctx.GetElementBadge(),
        .variant = ctx.btnVariant,
        .layout = ctx.elemLayout,
        .inlineOrder = ctx.elemInlineOrder,
        .elementAlign = ctx.elemAlign,
        .symmetric = s_state.symmetric,
        .stretch = ctx.GetElementStretch(),
        .width = ctx.GetElementWidth(),
        .divider = ctx.elemDivider,
        .dividerStyle = ctx.elemDividerStyle,
        .disabled = ctx.elemDisabled,
        .size = ctx.controlsSize
    });
}

std::string GenerateSwitchCode(const DemoContext& ctx) {
    return std::format(
        "ui.Switch(switchState, {{\n"
        "    .label = \"{}\",\n"
        "    .textLeft = \"{}\",\n"
        "    .textRight = \"{}\",\n"
        "    .description = {},\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .symmetric = {},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .width = {},\n"
        "    .divider = {},\n"
        "    .dividerStyle = guikit::ElementDivider::{},\n"
        "    .disabled = {},\n"
        "    .size = guikit::UiSize::{}\n"
        "}});",
        ctx.elemLabel,
        s_state.textLeft,
        s_state.textRight,
        ctx.FormatElementDescCode(),
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        VariantName(ctx.btnVariant),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        s_state.symmetric ? "true" : "false",
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.elemDivider ? "true" : "false",
        ctx.FormatDividerStyleCode(),
        ctx.elemDisabled ? "true" : "false",
        SizeName(ctx.controlsSize)
    );
}

} // namespace guikit::demo
