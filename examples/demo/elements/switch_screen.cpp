#include "../demo_context.hpp"

namespace guikit::demo {

namespace {
struct SwitchState {
    bool state = true;
    keel::SmallString<64> textLeft{""};
    keel::SmallString<64> textRight{""};
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
    ctx.RunElementStage(ui, [&]() {
        guikit::SwitchOptions opts{};
        opts.textLeft = s_state.textLeft;
        opts.textRight = s_state.textRight;
        opts.color = ctx.GetControlColor();
        opts.size = ctx.controlsSize;
        opts.symmetric = s_state.symmetric;
        opts.disabled = ctx.elemDisabled;
        ui.Switch(s_state.state, opts);
    });
}

keel::String GenerateSwitchCode(const DemoContext& ctx) {
    const keel::String ctrl = keel::format(
        "ui.Switch(switchState, {{\n"
        "    .textLeft = \"{}\",\n"
        "    .textRight = \"{}\",\n"
        "    .color = {}{},\n"
        "    .symmetric = {},\n"
        "    .disabled = {}\n"
        "}});",
        s_state.textLeft,
        s_state.textRight,
        ctx.FormatControlColorCode(),
        FormatUiSizePrefix(ctx.controlsSize),
        s_state.symmetric ? "true" : "false",
        ctx.elemDisabled ? "true" : "false"
    );
    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
