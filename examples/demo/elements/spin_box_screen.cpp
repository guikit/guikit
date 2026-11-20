#include "../demo_context.hpp"

namespace guikit::demo {

namespace {
struct SpinBoxState {
    float val = 142.5f;
    float minVal = 0.0f;
    float maxVal = 500.0f;
    float step = 2.5f;
    guikit::SpinBoxStepStyle stepStyle = guikit::SpinBoxStepStyle::Detached;
    keel::i32 iconStyleIdx = 0; // 0 = Minus/Plus, 1 = Chevrons, 2 = Arrows
    bool showFillBar = true;
    keel::SmallString<64> prefix{""};
    keel::SmallString<64> suffix{"MHz"};
    guikit::CursorBlinkMode cursorMode = guikit::CursorBlinkMode::Blinking;
    bool readOnly = false;
};
SpinBoxState s_state;

constexpr keel::StringRef StepStyleEnumName(guikit::SpinBoxStepStyle style) noexcept {
    switch (style) {
        case guikit::SpinBoxStepStyle::None:     return "None";
        case guikit::SpinBoxStepStyle::Inline:   return "Inline";
        case guikit::SpinBoxStepStyle::Detached: return "Detached";
        case guikit::SpinBoxStepStyle::Split:    return "Split";
    }
    return "Inline";
}
} // namespace

void RenderSpinBoxInspector(guikit::Context& ui, DemoContext&) {
    static constexpr keel::StaticArray<keel::StringRef, 4> stepStyleLabels = {"None", "Inline", "Detached", "Split"};
    keel::i32 styleIdx = static_cast<keel::i32>(s_state.stepStyle);
    if (auto el = ui.Element({
        .label = "Stepper Style (- / +)",
        .description = "None, Inline inside box, Detached square buttons after box, or Split sides"
    })) {
        if (ui.Segmented(styleIdx, stepStyleLabels).changed) {
            s_state.stepStyle = static_cast<guikit::SpinBoxStepStyle>(keel::clamp(styleIdx, 0, 3));
            ui.InvalidateAll();
        }
    }
    static constexpr keel::StaticArray<keel::StringRef, 3> iconStyleLabels = {"Minus / Plus", "Chevrons", "Arrows"};
    if (auto el = ui.Element({
        .label = "Stepper Icons",
        .description = "Custom vector icons for decrement / increment buttons"
    })) {
        if (ui.Segmented(s_state.iconStyleIdx, iconStyleLabels).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({
        .label = "Progress Fill Bar",
        .description = "Render normalized [min, max] fill bar along bottom edge"
    })) {
        if (ui.Switch(s_state.showFillBar).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Step Increment"})) {
        if (ui.Slider(s_state.step, 0.5f, 25.0f, {.valueFormat = "%.1f", .step = 0.5f}).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Prefix Text"})) {
        if (ui.InputField(s_state.prefix, {.placeholder = "e.g. Fc =", .clearable = true}).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Unit Suffix"})) {
        if (ui.InputField(s_state.suffix, {.placeholder = "e.g. MHz / dB / %", .clearable = true}).changed) {
            ui.InvalidateAll();
        }
    }

    static constexpr keel::StaticArray<keel::StringRef, 3> cursorModes = {"Blinking", "Solid", "Hidden"};
    if (auto el = ui.Element({.label = "Cursor Blink Mode"})) {
        if (ui.Segmented(s_state.cursorMode, cursorModes).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Read Only"})) {
        if (ui.Switch(s_state.readOnly).changed) ui.InvalidateAll();
    }
}

void RenderSpinBoxStage(guikit::Context& ui, DemoContext& ctx) {
    const guikit::Icon minusIc = (s_state.iconStyleIdx == 1)
        ? guikit::Icon::ChevronDown()
        : ((s_state.iconStyleIdx == 2) ? guikit::Icon::ArrowLeft() : guikit::Icon::Minus());
    const guikit::Icon plusIc = (s_state.iconStyleIdx == 1)
        ? guikit::Icon::ChevronUp()
        : ((s_state.iconStyleIdx == 2) ? guikit::Icon::ArrowRight() : guikit::Icon::Plus());

    ctx.RunElementStage(ui, [&]() {
        static constexpr keel::StaticArray<keel::StringRef, 2> kValVariants = {{"0.0", "500.0"}};
        guikit::SpinBoxOptions opts{};
        opts.prefix = s_state.prefix;
        opts.suffix = s_state.suffix;
        opts.valueFormat = {"%.1f", kValVariants};
        opts.cursorMode = s_state.cursorMode;
        opts.min = static_cast<double>(s_state.minVal);
        opts.max = static_cast<double>(s_state.maxVal);
        opts.step = static_cast<double>(s_state.step);
        opts.stepStyle = s_state.stepStyle;
        opts.minusIcon = minusIc;
        opts.plusIcon = plusIc;
        opts.showFillBar = s_state.showFillBar;
        opts.readOnly = s_state.readOnly;
        opts.disabled = ctx.elemDisabled;
        opts.size = ctx.controlsSize;
        opts.width = ctx.ResolveCtrlWidth();
        opts.height = ctx.ResolveCtrlHeight();
        opts.color = ctx.GetControlColor();
        opts.background = ctx.GetControlBackground(NavCategory::ElemSpinBox);
        opts.border = ctx.GetCtrlBorder();
        ui.SpinBox(s_state.val, opts);
    });
}

keel::String GenerateSpinBoxCode(const DemoContext& ctx) {
    const keel::StringRef minusIconCode = (s_state.iconStyleIdx == 1)
        ? "guikit::Icon::ChevronDown()"
        : ((s_state.iconStyleIdx == 2) ? "guikit::Icon::ArrowLeft()" : "guikit::Icon::Minus()");
    const keel::StringRef plusIconCode = (s_state.iconStyleIdx == 1)
        ? "guikit::Icon::ChevronUp()"
        : ((s_state.iconStyleIdx == 2) ? "guikit::Icon::ArrowRight()" : "guikit::Icon::Plus()");
    const bool hasCustomSize = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md);
    const keel::FixedString<64> sizeLine = hasCustomSize
        ? keel::format_fixed<64>("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};

    const keel::String ctrl = keel::format(
        "ui.SpinBox(centerFreqMhz, {{\n"
        "    .prefix = \"{}\",\n"
        "    .suffix = \"{}\",\n"
        "    .valueFormat = \"%.1f\",\n"
        "    .cursorMode = guikit::CursorBlinkMode::{},\n"
        "    .min = {:.1f},\n"
        "    .max = {:.1f},\n"
        "    .step = {:.1f},\n"
        "    .stepStyle = guikit::SpinBoxStepStyle::{},\n"
        "    .minusIcon = {},\n"
        "    .plusIcon = {},\n"
        "    .showFillBar = {},\n"
        "    .readOnly = {},\n"
        "    .disabled = {},\n"
        "{}"
        "    .width = {},\n"
        "    .height = {},\n"
        "    .color = {},\n"
        "    .background = {},\n"
        "    .border = {}\n"
        "}});",
        s_state.prefix,
        s_state.suffix,
        CursorModeName(s_state.cursorMode),
        s_state.minVal,
        s_state.maxVal,
        s_state.step,
        StepStyleEnumName(s_state.stepStyle),
        minusIconCode,
        plusIconCode,
        s_state.showFillBar ? "true" : "false",
        s_state.readOnly ? "true" : "false",
        ctx.elemDisabled ? "true" : "false",
        sizeLine,
        ctx.FormatControlWidthCode(),
        ctx.FormatControlHeightCode(),
        ctx.FormatControlColorCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemSpinBox),
        ctx.FormatCtrlBorderCode()
    );
    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
