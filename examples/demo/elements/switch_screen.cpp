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
        opts.symmetric = s_state.symmetric;
        opts.disabled = ctx.elemDisabled;
        opts.size = ctx.controlsSize;
        opts.color = ctx.GetControlColor();
        ui.Switch(s_state.state, opts);
    });
}

keel::String GenerateSwitchCode(const DemoContext& ctx) {
    const bool hasCustomSize = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md);
    const keel::FixedString<64> sizeLine = hasCustomSize
        ? keel::format_fixed<64>("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};
    const keel::String ctrl = keel::format(
        "ui.Switch(switchState, {{\n"
        "    .textLeft = \"{}\",\n"
        "    .textRight = \"{}\",\n"
        "    .symmetric = {},\n"
        "    .disabled = {},\n"
        "{}"
        "    .color = {}\n"
        "}});",
        s_state.textLeft,
        s_state.textRight,
        s_state.symmetric ? "true" : "false",
        ctx.elemDisabled ? "true" : "false",
        sizeLine,
        ctx.FormatControlColorCode()
    );
    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
