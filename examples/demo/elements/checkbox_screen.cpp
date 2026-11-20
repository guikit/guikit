#include "../demo_context.hpp"

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
    ctx.RunElementStage(ui, [&]() {
        guikit::CheckboxOptions opts{};
        opts.indeterminate = s_state.indet;
        opts.disabled = ctx.elemDisabled;
        opts.size = ctx.controlsSize;
        opts.color = ctx.GetControlColor();
        ui.Checkbox(s_state.state, opts);
    });
}

keel::String GenerateCheckboxCode(const DemoContext& ctx) {
    const bool hasCustomSize = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md);
    const keel::FixedString<64> sizeLine = hasCustomSize
        ? keel::format_fixed<64>("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};
    const keel::String ctrl = keel::format(
        "ui.Checkbox(checkState, {{\n"
        "    .indeterminate = {},\n"
        "    .disabled = {},\n"
        "{}"
        "    .color = {}\n"
        "}});",
        s_state.indet ? "true" : "false",
        ctx.elemDisabled ? "true" : "false",
        sizeLine,
        ctx.FormatControlColorCode()
    );
    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
