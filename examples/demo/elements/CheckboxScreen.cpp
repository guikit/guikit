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
    if (ui.Switch(s_state.indet, {.label = "Indeterminate Tri-State"}).changed) ui.InvalidateAll();
}

void RenderCheckboxStage(guikit::Context& ui, DemoContext& ctx) {
    ui.Checkbox(s_state.state, {
        .label = ctx.elemLabel,
        .description = ctx.GetElementDescription(),
        .icon = ctx.GetElementIcon(),
        .badge = ctx.GetElementBadge(),
        .variant = ctx.btnVariant,
        .layout = ctx.elemLayout,
        .inlineOrder = ctx.elemInlineOrder,
        .elementAlign = ctx.elemAlign,
        .stretch = ctx.GetElementStretch(),
        .width = ctx.GetElementWidth(),
        .divider = ctx.elemDivider,
        .dividerStyle = ctx.elemDividerStyle,
        .disabled = ctx.elemDisabled,
        .indeterminate = s_state.indet,
        .size = ctx.controlsSize
    });
}

std::string GenerateCheckboxCode(const DemoContext& ctx) {
    return std::format(
        "ui.Checkbox(checkState, {{\n"
        "    .label = \"{}\",\n"
        "    .description = {},\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .width = {},\n"
        "    .divider = {},\n"
        "    .dividerStyle = guikit::ElementDivider::{},\n"
        "    .disabled = {},\n"
        "    .indeterminate = {},\n"
        "    .size = guikit::UiSize::{}\n"
        "}});",
        ctx.elemLabel,
        ctx.FormatElementDescCode(),
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        VariantName(ctx.btnVariant),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.elemDivider ? "true" : "false",
        ctx.FormatDividerStyleCode(),
        ctx.elemDisabled ? "true" : "false",
        s_state.indet ? "true" : "false",
        SizeName(ctx.controlsSize)
    );
}

} // namespace guikit::demo
