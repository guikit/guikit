#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct ButtonState {
    bool hasIcon = true;
    guikit::Placement iconPlacement = guikit::Placement::Leading;
    guikit::Align controlAlign = guikit::Align::Center;
    size_t stretchIdx = 0; // Compact
    float width = 0.0f;    // 0 = auto (px in Compact/Space)
    float fillPct = 50.0f; // 50% default in Fill mode
    bool disabled = false;
    int clicks = 0;
    float cornerRadius = 6.0f;
    bool showLabel = true;
    bool showDesc = true;
    guikit::UiVariant descVariant = guikit::UiVariant::Muted;
    guikit::ElementLayout layout = guikit::ElementLayout::Stacked;
    guikit::InlineOrder inlineOrder = guikit::InlineOrder::LabelFirst;
    guikit::Align elementAlign = guikit::Align::Start;
};
ButtonState s_state;
} // namespace

void RenderButtonInspector(guikit::Context& ui, DemoContext& ctx) {
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
        if (ui.Slider(s_state.width, 0.0f, 400.0f, {.label = "Explicit Width", .valueFormat = {(s_state.width > 0.0f) ? "%.0f px" : "Auto", {"Auto", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
    }

    static constexpr std::array<std::string_view, 3> alignNames = {"Start (Left)", "Center", "End (Right)"};
    size_t alignIdx = (s_state.controlAlign == guikit::Align::Start) ? 0 : ((s_state.controlAlign == guikit::Align::End) ? 2 : 1);
    if (ui.StepSlider(alignIdx, alignNames, {.label = "Control Alignment", .showTicks = true, .showLabels = true}).changed) {
        s_state.controlAlign = (alignIdx == 0) ? guikit::Align::Start : ((alignIdx == 2) ? guikit::Align::End : guikit::Align::Center);
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.hasIcon, {.label = "Show Icon"}).changed) ui.InvalidateAll();
    static constexpr std::array<std::string_view, 4> placeNames = {"Leading", "Trailing", "Top", "Bottom"};
    size_t placeIdx = (s_state.iconPlacement == guikit::Placement::Trailing) ? 1 : ((s_state.iconPlacement == guikit::Placement::Top) ? 2 : ((s_state.iconPlacement == guikit::Placement::Bottom) ? 3 : 0));
    if (ui.StepSlider(placeIdx, placeNames, {.label = "Icon Placement", .showTicks = true, .showLabels = true, .disabled = !s_state.hasIcon}).changed) {
        s_state.iconPlacement = (placeIdx == 1) ? guikit::Placement::Trailing : ((placeIdx == 2) ? guikit::Placement::Top : ((placeIdx == 3) ? guikit::Placement::Bottom : guikit::Placement::Leading));
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.disabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
}

void RenderButtonStage(guikit::Context& ui, DemoContext& ctx) {
    std::string btnText = s_state.clicks > 0 ? std::format("Clicked {}x", s_state.clicks) : "Click me";
    if (ui.Button({
        .label = s_state.showLabel ? "A button" : "",
        .text = btnText,
        .description = {s_state.showDesc ? "This is a button" : "", s_state.descVariant},
        .icon = s_state.hasIcon ? guikit::Icon::Play() : guikit::Icon{},
        .variant = ctx.btnVariant,
        .iconPlacement = s_state.iconPlacement,
        .layout = s_state.layout,
        .inlineOrder = s_state.inlineOrder,
        .elementAlign = s_state.elementAlign,
        .controlAlign = s_state.controlAlign,
        .stretch = static_cast<guikit::StretchMode>(s_state.stretchIdx),
        .width = (s_state.stretchIdx == 2) ? std::optional<float>(s_state.fillPct) : ((s_state.width > 0.0f) ? std::optional<float>(s_state.width) : std::nullopt),
        .disabled = s_state.disabled,
        .size = ctx.controlsSize,
        .borderRadius = s_state.cornerRadius
    })) {
        ++s_state.clicks;
        ui.InvalidateAll();
    }
}

std::string GenerateButtonCode(const DemoContext& ctx) {
    return std::format(
        "if (ui.Button({{\n"
        "    .label = \"{}\",\n"
        "    .text = \"{}\",\n"
        "    .description = {},\n"
        "    .icon = {},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .iconPlacement = guikit::Placement::{},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .controlAlign = guikit::Align::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .width = {},\n"
        "    .disabled = {},\n"
        "    .active = false,\n"
        "    .size = guikit::UiSize::{},\n"
        "    .borderRadius = {:.1f}f\n"
        "}})) {{\n"
        "    // On click handler\n"
        "}}",
        s_state.showLabel ? "A button" : "",
        s_state.clicks > 0 ? std::format("Clicked {}x", s_state.clicks) : "Click me",
        FormatDesc(s_state.showDesc, "This is a button", s_state.descVariant),
        s_state.hasIcon ? "guikit::Icon::Play()" : "{}",
        VariantName(ctx.btnVariant),
        PlacementName(s_state.iconPlacement),
        ElementLayoutEnumName(s_state.layout),
        InlineOrderEnumName(s_state.inlineOrder),
        LabelAlignEnumName(s_state.elementAlign),
        AlignName(s_state.controlAlign),
        StretchModeEnumName(static_cast<guikit::StretchMode>(s_state.stretchIdx)),
        (s_state.stretchIdx == 2) ? std::format("{:.1f}f", s_state.fillPct) : ((s_state.width > 0.0f) ? std::format("{:.1f}f", s_state.width) : "std::nullopt"),
        s_state.disabled ? "true" : "false",
        SizeName(ctx.controlsSize),
        s_state.cornerRadius
    );
}

} // namespace guikit::demo
