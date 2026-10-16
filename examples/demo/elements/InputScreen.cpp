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
    float cornerRadius = 6.0f;
};
InputState s_state;
} // namespace

void RenderInputInspector(guikit::Context& ui, DemoContext&) {
    if (ui.Switch(s_state.numericMode, {
        .label = "Numeric SpinBox Mode",
        .description = "Numeric InputField with +/- Stepper vs Text"
    }).changed) {
        ui.InvalidateAll();
    }

    if (s_state.numericMode) {
        if (ui.Switch(s_state.stepButtons, {.label = "Show Stepper Buttons (- / +)"}).changed) ui.InvalidateAll();
        if (ui.Slider(s_state.numStep, 0.5f, 25.0f, {.label = "Step Increment", .valueFormat = "%.1f", .step = 0.5f}).changed) ui.InvalidateAll();
        if (ui.InputField(s_state.numSuffix, {.label = "Unit Suffix", .placeholder = "e.g. MHz / dB / %", .clearable = true}).changed) ui.InvalidateAll();
    } else {
        if (ui.InputField(s_state.placeholder, {
            .label = "Placeholder",
            .placeholder = "No placeholder...",
            .clearable = true
        }).changed) ui.InvalidateAll();
        if (ui.InputField(s_state.prefix, {
            .label = "Prefix Text",
            .placeholder = "e.g. https://",
            .clearable = true
        }).changed) ui.InvalidateAll();
        if (ui.InputField(s_state.suffix, {
            .label = "Suffix Text",
            .placeholder = "e.g. :443",
            .clearable = true
        }).changed) ui.InvalidateAll();
        if (ui.Switch(s_state.clearable, {.label = "Clearable Button ('X')"}).changed) ui.InvalidateAll();
        if (ui.Switch(s_state.password, {.label = "Password Mask & Reveal"}).changed) ui.InvalidateAll();
    }

    if (ui.Slider(s_state.cornerRadius, 0.0f, 24.0f, {.label = "Border Radius", .valueFormat = "%.0f px"}).changed) {
        ui.InvalidateAll();
    }

    static constexpr std::array<std::string_view, 3> cursorModes = {"Blinking", "Solid", "Hidden"};
    size_t curIdx = (s_state.cursorMode == guikit::CursorBlinkMode::Solid) ? 1 : ((s_state.cursorMode == guikit::CursorBlinkMode::Hidden) ? 2 : 0);
    if (ui.StepSlider(curIdx, cursorModes, {.label = "Cursor Blink Mode", .showTicks = true, .showLabels = true}).changed) {
        s_state.cursorMode = (curIdx == 1) ? guikit::CursorBlinkMode::Solid : ((curIdx == 2) ? guikit::CursorBlinkMode::Hidden : guikit::CursorBlinkMode::Blinking);
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.readOnly, {.label = "Read Only"}).changed) ui.InvalidateAll();
}

void RenderInputStage(guikit::Context& ui, DemoContext& ctx) {
    if (s_state.numericMode) {
        ui.InputField(s_state.numVal, {
            .label = ctx.elemLabel,
            .description = ctx.GetElementDescription(),
            .icon = ctx.GetElementIcon(),
            .badge = ctx.GetElementBadge(),
            .suffix = s_state.numSuffix,
            .valueFormat = {"%.1f", {"0.0", "500.0"}},
            .variant = ctx.btnVariant,
            .layout = ctx.elemLayout,
            .inlineOrder = ctx.elemInlineOrder,
            .elementAlign = ctx.elemAlign,
            .cursorMode = s_state.cursorMode,
            .size = ctx.controlsSize,
            .borderRadius = s_state.cornerRadius,
            .stretch = ctx.GetElementStretch(),
            .width = ctx.GetElementWidth(),
            .min = static_cast<double>(s_state.numMin),
            .max = static_cast<double>(s_state.numMax),
            .step = static_cast<double>(s_state.numStep),
            .divider = ctx.elemDivider,
            .dividerStyle = ctx.elemDividerStyle,
            .stepButtons = s_state.stepButtons,
            .disabled = ctx.elemDisabled,
            .readOnly = s_state.readOnly
        });
    } else {
        ui.InputField(s_state.text, {
            .label = ctx.elemLabel,
            .description = ctx.GetElementDescription(),
            .placeholder = s_state.placeholder,
            .icon = ctx.GetElementIcon(),
            .badge = ctx.GetElementBadge(),
            .prefix = s_state.prefix,
            .suffix = s_state.suffix,
            .valueFormat = {{"guikit.dev", "api.guikit.dev"}},
            .variant = ctx.btnVariant,
            .layout = ctx.elemLayout,
            .inlineOrder = ctx.elemInlineOrder,
            .elementAlign = ctx.elemAlign,
            .cursorMode = s_state.cursorMode,
            .size = ctx.controlsSize,
            .borderRadius = s_state.cornerRadius,
            .stretch = ctx.GetElementStretch(),
            .width = ctx.GetElementWidth(),
            .divider = ctx.elemDivider,
            .dividerStyle = ctx.elemDividerStyle,
            .password = s_state.password,
            .clearable = s_state.clearable,
            .disabled = ctx.elemDisabled,
            .readOnly = s_state.readOnly
        });
    }
}

std::string GenerateInputCode(const DemoContext& ctx) {
    if (s_state.numericMode) {
        return std::format(
            "ui.InputField(centerFreqMhz, {{\n"
            "    .label = \"{}\",\n"
            "    .description = {},\n"
            "    .icon = {},\n"
            "    .badge = {},\n"
            "    .suffix = \"{}\",\n"
            "    .valueFormat = \"%.1f\",\n"
            "    .variant = guikit::UiVariant::{},\n"
            "    .layout = guikit::ElementLayout::{},\n"
            "    .inlineOrder = guikit::InlineOrder::{},\n"
            "    .elementAlign = guikit::Align::{},\n"
            "    .cursorMode = guikit::CursorBlinkMode::{},\n"
            "    .size = guikit::UiSize::{},\n"
            "    .borderRadius = {:.1f}f,\n"
            "    .stretch = guikit::StretchMode::{},\n"
            "    .width = {},\n"
            "    .min = {:.1f},\n"
            "    .max = {:.1f},\n"
            "    .step = {:.1f},\n"
            "    .divider = {},\n"
            "    .dividerStyle = guikit::ElementDivider::{},\n"
            "    .stepButtons = {},\n"
            "    .disabled = {},\n"
            "    .readOnly = {}\n"
            "}});",
            ctx.elemLabel,
            ctx.FormatElementDescCode(),
            ctx.FormatElementIconCode(),
            ctx.FormatElementBadgeCode(),
            s_state.numSuffix,
            VariantName(ctx.btnVariant),
            ElementLayoutEnumName(ctx.elemLayout),
            InlineOrderEnumName(ctx.elemInlineOrder),
            LabelAlignEnumName(ctx.elemAlign),
            CursorModeName(s_state.cursorMode),
            SizeName(ctx.controlsSize),
            s_state.cornerRadius,
            StretchModeEnumName(ctx.GetElementStretch()),
            ctx.FormatElementWidthCode(),
            s_state.numMin,
            s_state.numMax,
            s_state.numStep,
            ctx.elemDivider ? "true" : "false",
            ctx.FormatDividerStyleCode(),
            s_state.stepButtons ? "true" : "false",
            ctx.elemDisabled ? "true" : "false",
            s_state.readOnly ? "true" : "false"
        );
    }
    return std::format(
        "ui.InputField(serverEndpoint, {{\n"
        "    .label = \"{}\",\n"
        "    .description = {},\n"
        "    .placeholder = \"{}\",\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .prefix = \"{}\",\n"
        "    .suffix = \"{}\",\n"
        "    .valueFormat = {{{{\"guikit.dev\", \"api.guikit.dev\"}}}},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .cursorMode = guikit::CursorBlinkMode::{},\n"
        "    .size = guikit::UiSize::{},\n"
        "    .borderRadius = {:.1f}f,\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .width = {},\n"
        "    .divider = {},\n"
        "    .dividerStyle = guikit::ElementDivider::{},\n"
        "    .password = {},\n"
        "    .clearable = {},\n"
        "    .disabled = {},\n"
        "    .readOnly = {}\n"
        "}});",
        ctx.elemLabel,
        ctx.FormatElementDescCode(),
        s_state.placeholder,
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        s_state.prefix,
        s_state.suffix,
        VariantName(ctx.btnVariant),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        CursorModeName(s_state.cursorMode),
        SizeName(ctx.controlsSize),
        s_state.cornerRadius,
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.elemDivider ? "true" : "false",
        ctx.FormatDividerStyleCode(),
        s_state.password ? "true" : "false",
        s_state.clearable ? "true" : "false",
        ctx.elemDisabled ? "true" : "false",
        s_state.readOnly ? "true" : "false"
    );
}

} // namespace guikit::demo
