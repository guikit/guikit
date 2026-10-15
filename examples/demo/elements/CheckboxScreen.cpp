#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct CheckboxState {
    bool state = true;
    bool indet = false;
    guikit::ElementLayout layout = guikit::ElementLayout::Inline;
    guikit::InlineOrder inlineOrder = guikit::InlineOrder::ControlFirst;
    guikit::Align elementAlign = guikit::Align::Start;
    size_t stretchIdx = 0; // Compact
    bool showLabel = true;
    bool showDesc = true;
    guikit::UiVariant descVariant = guikit::UiVariant::Muted;
    bool disabled = false;
};
CheckboxState s_state;
} // namespace

void RenderCheckboxInspector(guikit::Context& ui, DemoContext& ctx) {
    if (ui.StepSlider(ctx.controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();

    ctx.RenderVariantPicker(ui);

    if (ui.Switch(s_state.showLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
    ctx.RenderDescVariantPicker(ui, s_state.descVariant, s_state.showDesc);

    ctx.RenderElementLayoutControls(ui, s_state.showLabel || s_state.showDesc, s_state.layout, s_state.inlineOrder, s_state.elementAlign, s_state.stretchIdx);
    if (ui.Switch(s_state.indet, {.label = "Indeterminate Tri-State"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.disabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
}

void RenderCheckboxStage(guikit::Context& ui, DemoContext& ctx) {
    ui.Checkbox(s_state.state, {
        .label = s_state.showLabel ? "A checkbox" : "",
        .description = {s_state.showDesc ? "This is a checkbox" : "", s_state.descVariant},
        .variant = ctx.btnVariant,
        .layout = s_state.layout,
        .inlineOrder = s_state.inlineOrder,
        .elementAlign = s_state.elementAlign,
        .stretch = static_cast<guikit::StretchMode>(s_state.stretchIdx),
        .disabled = s_state.disabled,
        .indeterminate = s_state.indet,
        .size = ctx.controlsSize
    });
}

std::string GenerateCheckboxCode(const DemoContext& ctx) {
    return std::format(
        "ui.Checkbox(checkState, {{\n"
        "    .label = \"{}\",\n"
        "    .description = {},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .disabled = {},\n"
        "    .indeterminate = {},\n"
        "    .size = guikit::UiSize::{}\n"
        "}});",
        s_state.showLabel ? "A checkbox" : "",
        FormatDesc(s_state.showDesc, "This is a checkbox", s_state.descVariant),
        VariantName(ctx.btnVariant),
        ElementLayoutEnumName(s_state.layout),
        InlineOrderEnumName(s_state.inlineOrder),
        LabelAlignEnumName(s_state.elementAlign),
        StretchModeEnumName(static_cast<guikit::StretchMode>(s_state.stretchIdx)),
        s_state.disabled ? "true" : "false",
        s_state.indet ? "true" : "false",
        SizeName(ctx.controlsSize)
    );
}

} // namespace guikit::demo
