#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct InputState {
    std::string text = "guikit.dev";
    guikit::CursorBlinkMode cursorMode = guikit::CursorBlinkMode::Blinking;
    bool password = false;
    bool clearable = true;
    bool readOnly = false;
    bool disabled = false;
    bool hasPrefix = true;
    bool hasSuffix = false;
    float cornerRadius = 6.0f;
    bool showLabel = true;
    bool showDesc = true;
    guikit::UiVariant descVariant = guikit::UiVariant::Muted;
    guikit::ElementLayout layout = guikit::ElementLayout::Stacked;
    guikit::InlineOrder inlineOrder = guikit::InlineOrder::LabelFirst;
    guikit::Align elementAlign = guikit::Align::Start;
    size_t stretchIdx = 2; // Fill
    float width = 0.0f;
    float fillPct = 50.0f;
};
InputState s_state;
} // namespace

void RenderInputInspector(guikit::Context& ui, DemoContext& ctx) {
    if (ui.StepSlider(ctx.controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.cornerRadius, 0.0f, 24.0f, {.label = "Corner Radius", .valueFormat = "%.0f px"}).changed) {
        ui.InvalidateAll();
    }

    ctx.RenderVariantPicker(ui);

    if (ui.Switch(s_state.showLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
    ctx.RenderDescVariantPicker(ui, s_state.descVariant, s_state.showDesc);

    ctx.RenderElementLayoutControls(ui, s_state.showLabel || s_state.showDesc, s_state.layout, s_state.inlineOrder, s_state.elementAlign, s_state.stretchIdx);
    if (s_state.stretchIdx == 2) {
        if (ui.Slider(s_state.fillPct, 10.0f, 100.0f, {.label = "Explicit Width", .valueFormat = "%.0f %%", .step = 5.0f}).changed) ui.InvalidateAll();
    } else {
        if (ui.Slider(s_state.width, 0.0f, 400.0f, {.label = "Explicit Width", .valueFormat = {(s_state.width > 0.0f) ? "%.0f px" : "Default", {"Default", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
    }

    static constexpr std::array<std::string_view, 3> cursorModes = {"Blinking", "Solid", "Hidden"};
    size_t curIdx = (s_state.cursorMode == guikit::CursorBlinkMode::Solid) ? 1 : ((s_state.cursorMode == guikit::CursorBlinkMode::Hidden) ? 2 : 0);
    if (ui.StepSlider(curIdx, cursorModes, {.label = "Cursor Blink Mode", .showTicks = true, .showLabels = true}).changed) {
        s_state.cursorMode = (curIdx == 1) ? guikit::CursorBlinkMode::Solid : ((curIdx == 2) ? guikit::CursorBlinkMode::Hidden : guikit::CursorBlinkMode::Blinking);
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.hasPrefix, {.label = "Prefix (https://)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.hasSuffix, {.label = "Suffix (:443)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.clearable, {.label = "Clearable Button ('X')"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.password, {.label = "Password Mask & Reveal"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.readOnly, {.label = "Read Only"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.disabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
}

void RenderInputStage(guikit::Context& ui, DemoContext& ctx) {
    ui.InputField(s_state.text, {
        .label = s_state.showLabel ? "A text input" : "",
        .description = {s_state.showDesc ? "This is a text input" : "", s_state.descVariant},
        .placeholder = "Enter domain...",
        .prefix = s_state.hasPrefix ? "https://" : "",
        .suffix = s_state.hasSuffix ? ":443" : "",
        .valueFormat = {{"guikit.dev", "api.guikit.dev"}},
        .variant = ctx.btnVariant,
        .layout = s_state.layout,
        .inlineOrder = s_state.inlineOrder,
        .elementAlign = s_state.elementAlign,
        .cursorMode = s_state.cursorMode,
        .size = ctx.controlsSize,
        .borderRadius = s_state.cornerRadius,
        .stretch = static_cast<guikit::StretchMode>(s_state.stretchIdx),
        .width = (s_state.stretchIdx == 2) ? std::optional<float>(s_state.fillPct) : ((s_state.width > 0.0f) ? std::optional<float>(s_state.width) : std::nullopt),
        .password = s_state.password,
        .clearable = s_state.clearable,
        .disabled = s_state.disabled,
        .readOnly = s_state.readOnly
    });
}

std::string GenerateInputCode(const DemoContext& ctx) {
    return std::format(
        "ui.InputField(serverEndpoint, {{\n"
        "    .label = \"{}\",\n"
        "    .description = {},\n"
        "    .placeholder = \"Enter domain...\",\n"
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
        "    .password = {},\n"
        "    .clearable = {},\n"
        "    .disabled = {},\n"
        "    .readOnly = {}\n"
        "}});",
        s_state.showLabel ? "A text input" : "",
        FormatDesc(s_state.showDesc, "This is a text input", s_state.descVariant),
        s_state.hasPrefix ? "https://" : "",
        s_state.hasSuffix ? ":443" : "",
        VariantName(ctx.btnVariant),
        ElementLayoutEnumName(s_state.layout),
        InlineOrderEnumName(s_state.inlineOrder),
        LabelAlignEnumName(s_state.elementAlign),
        CursorModeName(s_state.cursorMode),
        SizeName(ctx.controlsSize),
        s_state.cornerRadius,
        StretchModeEnumName(static_cast<guikit::StretchMode>(s_state.stretchIdx)),
        (s_state.stretchIdx == 2) ? std::format("{:.1f}f", s_state.fillPct) : ((s_state.width > 0.0f) ? std::format("{:.1f}f", s_state.width) : "std::nullopt"),
        s_state.password ? "true" : "false",
        s_state.clearable ? "true" : "false",
        s_state.disabled ? "true" : "false",
        s_state.readOnly ? "true" : "false"
    );
}

} // namespace guikit::demo
