#include "../demo_context.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct SpinBoxState {
    float val = 142.5f;
    float minVal = 0.0f;
    float maxVal = 500.0f;
    float step = 2.5f;
    guikit::SpinBoxStepStyle stepStyle = guikit::SpinBoxStepStyle::Detached;
    int iconStyleIdx = 0; // 0 = Minus/Plus, 1 = Chevrons, 2 = Arrows
    bool showFillBar = true;
    std::string prefix = "";
    std::string suffix = "MHz";
    guikit::CursorBlinkMode cursorMode = guikit::CursorBlinkMode::Blinking;
    bool readOnly = false;
};
SpinBoxState s_state;

constexpr std::string_view StepStyleEnumName(guikit::SpinBoxStepStyle style) noexcept {
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
    static constexpr std::array<std::string_view, 4> stepStyleLabels = {"None", "Inline", "Detached", "Split"};
    int styleIdx = static_cast<int>(s_state.stepStyle);
    if (auto el = ui.Element({
        .label = "Stepper Style (- / +)",
        .description = "None, Inline inside box, Detached square buttons after box, or Split sides"
    })) {
        if (ui.Segmented(styleIdx, stepStyleLabels).changed) {
            s_state.stepStyle = static_cast<guikit::SpinBoxStepStyle>(std::clamp(styleIdx, 0, 3));
            ui.InvalidateAll();
        }
    }
    static constexpr std::array<std::string_view, 3> iconStyleLabels = {"Minus / Plus", "Chevrons", "Arrows"};
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
        if (ui.InputField(s_state.suffix, {.placeholder = "e.g. MHz / dB / %", .clearable = true}).changed) ui.InvalidateAll();
    }

    static constexpr std::array<std::string_view, 3> cursorModes = {"Blinking", "Solid", "Hidden"};
    size_t curIdx = (s_state.cursorMode == guikit::CursorBlinkMode::Solid) ? 1 : ((s_state.cursorMode == guikit::CursorBlinkMode::Hidden) ? 2 : 0);
    if (auto el = ui.Element({.label = "Cursor Blink Mode"})) {
        if (ui.StepSlider(curIdx, cursorModes, {.showTicks = true, .showLabels = true}).changed) {
            s_state.cursorMode = (curIdx == 1) ? guikit::CursorBlinkMode::Solid : ((curIdx == 2) ? guikit::CursorBlinkMode::Hidden : guikit::CursorBlinkMode::Blinking);
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

    if (auto el = ui.Size(ctx.elemSize).Element({
        .label = ctx.GetElementLabel(),
        .description = ctx.GetElementDescription(),
        .icon = ctx.GetElementIcon(),
        .badge = ctx.GetElementBadge(),
        .layout = ctx.elemLayout,
        .inlineOrder = ctx.elemInlineOrder,
        .align = ctx.elemAlign,
        .stretch = ctx.GetElementStretch(),
        .background = ctx.elemBackground,
        .border = ctx.GetElemBorder(),
        .disabled = ctx.elemDisabled
    })) {
        ui.Size(ctx.controlsSize).SpinBox(s_state.val, {
            .prefix = s_state.prefix,
            .suffix = s_state.suffix,
            .valueFormat = {"%.1f", {"0.0", "500.0"}},
            .cursorMode = s_state.cursorMode,
            .color = ctx.GetControlColor(),
            .stretch = ctx.GetElementStretch(),
            .width = ctx.GetElementWidth(),
            .height = ctx.GetElementHeight(),
            .background = ctx.GetControlBackground(NavCategory::ElemSpinBox),
            .border = ctx.GetCtrlBorder(),
            .min = static_cast<double>(s_state.minVal),
            .max = static_cast<double>(s_state.maxVal),
            .step = static_cast<double>(s_state.step),
            .stepStyle = s_state.stepStyle,
            .minusIcon = minusIc,
            .plusIcon = plusIc,
            .showFillBar = s_state.showFillBar,
            .disabled = ctx.elemDisabled,
            .readOnly = s_state.readOnly
        });
    }
}

std::string GenerateSpinBoxCode(const DemoContext& ctx) {
    const std::string_view minusIconCode = (s_state.iconStyleIdx == 1)
        ? "guikit::Icon::ChevronDown()"
        : ((s_state.iconStyleIdx == 2) ? "guikit::Icon::ArrowLeft()" : "guikit::Icon::Minus()");
    const std::string_view plusIconCode = (s_state.iconStyleIdx == 1)
        ? "guikit::Icon::ChevronUp()"
        : ((s_state.iconStyleIdx == 2) ? "guikit::Icon::ArrowRight()" : "guikit::Icon::Plus()");

    return std::format(
        "if (auto el = ui.{}Element({{\n"
        "    .label = {},\n"
        "    .description = {},\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .align = guikit::Align::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .background = {},\n"
        "    .border = {},\n"
        "    .disabled = {}\n"
        "}})) {{\n"
        "    ui.{}SpinBox(centerFreqMhz, {{\n"
        "        .prefix = \"{}\",\n"
        "        .suffix = \"{}\",\n"
        "        .valueFormat = \"%.1f\",\n"
        "        .cursorMode = guikit::CursorBlinkMode::{},\n"
        "        .color = {},\n"
        "        .stretch = guikit::StretchMode::{},\n"
        "        .width = {},\n"
        "        .height = {},\n"
        "        .background = {},\n"
        "        .border = {},\n"
        "        .min = {:.1f},\n"
        "        .max = {:.1f},\n"
        "        .step = {:.1f},\n"
        "        .stepStyle = guikit::SpinBoxStepStyle::{},\n"
        "        .minusIcon = {},\n"
        "        .plusIcon = {},\n"
        "        .showFillBar = {},\n"
        "        .disabled = {},\n"
        "        .readOnly = {}\n"
        "    }});\n"
        "}}",
        FormatUiSizePrefix(ctx.elemSize),
        ctx.FormatElementLabelCode(),
        ctx.FormatElementDescCode(),
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElemBackgroundCode(),
        ctx.FormatElemBorderCode(),
        ctx.elemDisabled ? "true" : "false",
        FormatUiSizePrefix(ctx.controlsSize),
        s_state.prefix,
        s_state.suffix,
        CursorModeName(s_state.cursorMode),
        ctx.FormatControlColorCode(),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemSpinBox),
        ctx.FormatCtrlBorderCode(),
        s_state.minVal,
        s_state.maxVal,
        s_state.step,
        StepStyleEnumName(s_state.stepStyle),
        minusIconCode,
        plusIconCode,
        s_state.showFillBar ? "true" : "false",
        ctx.elemDisabled ? "true" : "false",
        s_state.readOnly ? "true" : "false"
    );
}

} // namespace guikit::demo
