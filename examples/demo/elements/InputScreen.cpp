#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct InputState {
    bool numericMode = false;
    std::string text = "guikit.dev";
    float numVal = 142.5f;
    float numMin = 0.0f;
    float numMax = 500.0f;
    float numStep = 2.5f;
    bool stepButtons = true;
    std::string numSuffix = "MHz";
    std::string placeholder = "Enter domain...";
    std::string prefix = "https://";
    std::string suffix = ":443";
    guikit::CursorBlinkMode cursorMode = guikit::CursorBlinkMode::Blinking;
    bool password = false;
    bool clearable = true;
    bool readOnly = false;
};
InputState s_state;
} // namespace

void RenderInputInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({
        .label = "Numeric SpinBox Mode",
        .description = "Numeric InputField with +/- Stepper vs Text"
    })) {
        if (ui.Switch(s_state.numericMode).changed) {
            ui.InvalidateAll();
        }
    }

    if (s_state.numericMode) {
        if (auto el = ui.Element({.label = "Show Stepper Buttons (- / +)"})) {
            if (ui.Switch(s_state.stepButtons).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Step Increment"})) {
            if (ui.Slider(s_state.numStep, 0.5f, 25.0f, {.valueFormat = "%.1f", .step = 0.5f}).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Unit Suffix"})) {
            if (ui.InputField(s_state.numSuffix, {.placeholder = "e.g. MHz / dB / %", .clearable = true}).changed) ui.InvalidateAll();
        }
    } else {
        if (auto el = ui.Element({.label = "Placeholder"})) {
            if (ui.InputField(s_state.placeholder, {
                .placeholder = "No placeholder...",
                .clearable = true
            }).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Prefix Text"})) {
            if (ui.InputField(s_state.prefix, {
                .placeholder = "e.g. https://",
                .clearable = true
            }).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Suffix Text"})) {
            if (ui.InputField(s_state.suffix, {
                .placeholder = "e.g. :443",
                .clearable = true
            }).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Clearable Button ('X')"})) {
            if (ui.Switch(s_state.clearable).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Password Mask & Reveal"})) {
            if (ui.Switch(s_state.password).changed) ui.InvalidateAll();
        }
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

void RenderInputStage(guikit::Context& ui, DemoContext& ctx) {
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
        if (s_state.numericMode) {
            ui.Size(ctx.controlsSize).InputField(s_state.numVal, {
                .suffix = s_state.numSuffix,
                .valueFormat = {"%.1f", {"0.0", "500.0"}},
                .cursorMode = s_state.cursorMode,
                .stretch = ctx.GetElementStretch(),
                .width = ctx.GetElementWidth(),
                .height = ctx.GetElementHeight(),
                .background = ctx.GetControlBackground(NavCategory::ElemInputField),
                .border = ctx.GetCtrlBorder(),
                .min = static_cast<double>(s_state.numMin),
                .max = static_cast<double>(s_state.numMax),
                .step = static_cast<double>(s_state.numStep),
                .stepButtons = s_state.stepButtons,
                .disabled = ctx.elemDisabled,
                .readOnly = s_state.readOnly
            });
        } else {
            ui.Size(ctx.controlsSize).InputField(s_state.text, {
                .placeholder = s_state.placeholder,
                .prefix = s_state.prefix,
                .suffix = s_state.suffix,
                .valueFormat = {{"guikit.dev", "api.guikit.dev"}},
                .cursorMode = s_state.cursorMode,
                .stretch = ctx.GetElementStretch(),
                .width = ctx.GetElementWidth(),
                .height = ctx.GetElementHeight(),
                .background = ctx.GetControlBackground(NavCategory::ElemInputField),
                .border = ctx.GetCtrlBorder(),
                .password = s_state.password,
                .clearable = s_state.clearable,
                .disabled = ctx.elemDisabled,
                .readOnly = s_state.readOnly
            });
        }
    }
}

std::string GenerateInputCode(const DemoContext& ctx) {
    if (s_state.numericMode) {
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
            "    ui.{}InputField(centerFreqMhz, {{\n"
            "        .suffix = \"{}\",\n"
            "        .valueFormat = \"%.1f\",\n"
            "        .cursorMode = guikit::CursorBlinkMode::{},\n"
            "        .stretch = guikit::StretchMode::{},\n"
            "        .width = {},\n"
            "        .height = {},\n"
            "        .background = {},\n"
            "        .border = {},\n"
            "        .min = {:.1f},\n"
            "        .max = {:.1f},\n"
            "        .step = {:.1f},\n"
            "        .stepButtons = {},\n"
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
            s_state.numSuffix,
            CursorModeName(s_state.cursorMode),
            StretchModeEnumName(ctx.GetElementStretch()),
            ctx.FormatElementWidthCode(),
            ctx.FormatElementHeightCode(),
            ctx.FormatControlBackgroundCode(NavCategory::ElemInputField),
            ctx.FormatCtrlBorderCode(),
            s_state.numMin,
            s_state.numMax,
            s_state.numStep,
            s_state.stepButtons ? "true" : "false",
            ctx.elemDisabled ? "true" : "false",
            s_state.readOnly ? "true" : "false"
        );
    }
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
        "    ui.{}InputField(serverEndpoint, {{\n"
        "        .placeholder = \"{}\",\n"
        "        .prefix = \"{}\",\n"
        "        .suffix = \"{}\",\n"
        "        .valueFormat = {{{{\"guikit.dev\", \"api.guikit.dev\"}}}},\n"
        "        .cursorMode = guikit::CursorBlinkMode::{},\n"
        "        .stretch = guikit::StretchMode::{},\n"
        "        .width = {},\n"
        "        .height = {},\n"
        "        .background = {},\n"
        "        .border = {},\n"
        "        .password = {},\n"
        "        .clearable = {},\n"
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
        s_state.placeholder,
        s_state.prefix,
        s_state.suffix,
        CursorModeName(s_state.cursorMode),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemInputField),
        ctx.FormatCtrlBorderCode(),
        s_state.password ? "true" : "false",
        s_state.clearable ? "true" : "false",
        ctx.elemDisabled ? "true" : "false",
        s_state.readOnly ? "true" : "false"
    );
}

} // namespace guikit::demo
