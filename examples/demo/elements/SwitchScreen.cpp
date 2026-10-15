#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct SwitchState {
    bool state = true;
    bool sideText = false;
    guikit::ElementLayout layout = guikit::ElementLayout::Inline;
    guikit::InlineOrder inlineOrder = guikit::InlineOrder::ControlFirst;
    guikit::Align elementAlign = guikit::Align::Start;
    size_t stretchIdx = 0; // Compact
    bool symmetric = false;
    bool showLabel = true;
    bool showDesc = true;
    guikit::UiVariant descVariant = guikit::UiVariant::Muted;
    bool disabled = false;
};
SwitchState s_state;
} // namespace

void RenderSwitchInspector(guikit::Context& ui, DemoContext& ctx) {
    if (ui.StepSlider(ctx.controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();

    ctx.RenderVariantPicker(ui);

    if (ui.Switch(s_state.showLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
    ctx.RenderDescVariantPicker(ui, s_state.descVariant, s_state.showDesc);

    ctx.RenderElementLayoutControls(ui, s_state.showLabel || s_state.showDesc, s_state.layout, s_state.inlineOrder, s_state.elementAlign, s_state.stretchIdx);
    if (ui.Switch(s_state.sideText, {.label = "Side Text (Off / On)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.symmetric, {.label = "Symmetric (Neutral Track)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.disabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
}

void RenderSwitchStage(guikit::Context& ui, DemoContext& ctx) {
    ui.Switch(s_state.state, {
        .label = s_state.showLabel ? "A switch" : "",
        .textLeft = s_state.sideText ? "Off" : "",
        .textRight = s_state.sideText ? "On" : "",
        .description = {s_state.showDesc ? "This is a switch" : "", s_state.descVariant},
        .variant = ctx.btnVariant,
        .layout = s_state.layout,
        .inlineOrder = s_state.inlineOrder,
        .elementAlign = s_state.elementAlign,
        .symmetric = s_state.symmetric,
        .stretch = static_cast<guikit::StretchMode>(s_state.stretchIdx),
        .disabled = s_state.disabled,
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
        "    .variant = guikit::UiVariant::{},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .symmetric = {},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .disabled = {},\n"
        "    .size = guikit::UiSize::{}\n"
        "}});",
        s_state.showLabel ? "A switch" : "",
        s_state.sideText ? "Off" : "",
        s_state.sideText ? "On" : "",
        FormatDesc(s_state.showDesc, "This is a switch", s_state.descVariant),
        VariantName(ctx.btnVariant),
        ElementLayoutEnumName(s_state.layout),
        InlineOrderEnumName(s_state.inlineOrder),
        LabelAlignEnumName(s_state.elementAlign),
        s_state.symmetric ? "true" : "false",
        StretchModeEnumName(static_cast<guikit::StretchMode>(s_state.stretchIdx)),
        s_state.disabled ? "true" : "false",
        SizeName(ctx.controlsSize)
    );
}

} // namespace guikit::demo
