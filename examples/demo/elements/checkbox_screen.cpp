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
        opts.color = ctx.GetControlColor();
        opts.size = ctx.controlsSize;
        opts.disabled = ctx.elemDisabled;
        opts.indeterminate = s_state.indet;
        ui.Checkbox(s_state.state, opts);
    });
}

keel::String GenerateCheckboxCode(const DemoContext& ctx) {
    const keel::String ctrl = keel::format(
        "ui.Checkbox(checkState, {{\n"
        "    .color = {}{},\n"
        "    .disabled = {},\n"
        "    .indeterminate = {}\n"
        "}});",
        ctx.FormatControlColorCode(),
        FormatUiSizePrefix(ctx.controlsSize),
        ctx.elemDisabled ? "true" : "false",
        s_state.indet ? "true" : "false"
    );
    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
