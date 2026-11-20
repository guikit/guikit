#include "../demo_context.hpp"

namespace guikit::demo {

namespace {
struct RadioState {
    bool groupDemo = true;
    keel::i32 selectedIdx = 0;
    bool singleChecked = true;
};
RadioState s_state;
} // namespace

void RenderRadioInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({
        .label = "3-Option Radio Group",
        .description = "Preview mutually exclusive group vs single Radio"
    })) {
        if (ui.Switch(s_state.groupDemo).changed) {
            ui.InvalidateAll();
        }
    }
    if (s_state.groupDemo) {
        static constexpr keel::StaticArray<keel::StringRef, 3> optNames = {"Option A", "Option B", "Option C"};
        keel::usize idx = static_cast<keel::usize>(keel::clamp(s_state.selectedIdx, 0, 2));
        if (auto el = ui.Element({.label = "Active Selection"})) {
            if (ui.Segmented(idx, optNames).changed) {
                s_state.selectedIdx = static_cast<keel::i32>(idx);
                ui.InvalidateAll();
            }
        }
    } else {
        if (auto el = ui.Element({.label = "Selected State"})) {
            if (ui.Switch(s_state.singleChecked).changed) {
                ui.InvalidateAll();
            }
        }
    }
}

void RenderRadioStage(guikit::Context& ui, DemoContext& ctx) {
    guikit::RadioOptions ctrlOpts{};
    ctrlOpts.disabled = ctx.elemDisabled;
    ctrlOpts.size = ctx.controlsSize;
    ctrlOpts.color = ctx.GetControlColor();

    if (s_state.groupDemo) {
        if (auto col = ui.Column({.gap = 14.0f})) {
            const guikit::UiText lblA{
                ctx.elemLabel.empty()
                    ? keel::StringRef("Option A — Automatic Gain (AGC)")
                    : ctx.elemLabel.as_ref(),
                ctx.elemLabelVariant
            };
            if (auto el = ui.Element(ctx.MakeElementOptions(lblA))) {
                ui.Radio(s_state.selectedIdx, 0, ctrlOpts);
            }
            if (auto el = ui.Element(ctx.MakeElementOptions(
                guikit::UiText{"Option B — Manual Fixed Gain", ctx.elemLabelVariant}
            ))) {
                ui.Radio(s_state.selectedIdx, 1, ctrlOpts);
            }
            if (auto el = ui.Element(ctx.MakeElementOptions(
                guikit::UiText{"Option C — External Hardware Trigger", ctx.elemLabelVariant}
            ))) {
                ui.Radio(s_state.selectedIdx, 2, ctrlOpts);
            }
        }
    } else {
        ctx.RunElementStage(ui, [&]() {
            ui.Radio(s_state.singleChecked, ctrlOpts);
        });
    }
}

keel::String GenerateRadioCode(const DemoContext& ctx) {
    const keel::StringRef callHead = s_state.groupDemo
        ? "Radio(selectedMode, 0"
        : "Radio(radioSelected";
    const bool hasCustomSize = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md);
    const keel::FixedString<64> sizeLine = hasCustomSize
        ? keel::format_fixed<64>("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};
    const keel::String ctrl = keel::format(
        "ui.{}, {{\n"
        "    .disabled = {},\n"
        "{}"
        "    .color = {}\n"
        "}});",
        callHead,
        ctx.elemDisabled ? "true" : "false",
        sizeLine,
        ctx.FormatControlColorCode()
    );
    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
