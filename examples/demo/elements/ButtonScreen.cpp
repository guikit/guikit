#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct ButtonState {
    std::string text = "Click me";
    bool hasIcon = true;
    guikit::Placement iconPlacement = guikit::Placement::Leading;
    guikit::Align controlAlign = guikit::Align::Center;
    int clicks = 0;
    float cornerRadius = 6.0f;
};
ButtonState s_state;
} // namespace

void RenderButtonInspector(guikit::Context& ui, DemoContext&) {
    if (ui.InputField(s_state.text, {
        .label = "Capsule Text",
        .placeholder = "Icon-only...",
        .clearable = true
    }).changed) {
        s_state.clicks = 0;
        ui.InvalidateAll();
    }
    if (ui.Slider(s_state.cornerRadius, 0.0f, 24.0f, {.label = "Border Radius", .valueFormat = "%.0f px"}).changed) {
        ui.InvalidateAll();
    }

    static constexpr std::array<std::string_view, 3> alignNames = {"Start (Left)", "Center", "End (Right)"};
    size_t alignIdx = (s_state.controlAlign == guikit::Align::Start) ? 0 : ((s_state.controlAlign == guikit::Align::End) ? 2 : 1);
    if (ui.StepSlider(alignIdx, alignNames, {.label = "Control Alignment", .showTicks = true, .showLabels = true}).changed) {
        s_state.controlAlign = (alignIdx == 0) ? guikit::Align::Start : ((alignIdx == 2) ? guikit::Align::End : guikit::Align::Center);
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.hasIcon, {.label = "Show Capsule Icon"}).changed) ui.InvalidateAll();
    static constexpr std::array<std::string_view, 4> placeNames = {"Leading", "Trailing", "Top", "Bottom"};
    size_t placeIdx = (s_state.iconPlacement == guikit::Placement::Trailing) ? 1 : ((s_state.iconPlacement == guikit::Placement::Top) ? 2 : ((s_state.iconPlacement == guikit::Placement::Bottom) ? 3 : 0));
    if (ui.StepSlider(placeIdx, placeNames, {.label = "Icon Placement", .showTicks = true, .showLabels = true, .disabled = !s_state.hasIcon}).changed) {
        s_state.iconPlacement = (placeIdx == 1) ? guikit::Placement::Trailing : ((placeIdx == 2) ? guikit::Placement::Top : ((placeIdx == 3) ? guikit::Placement::Bottom : guikit::Placement::Leading));
        ui.InvalidateAll();
    }
}

void RenderButtonStage(guikit::Context& ui, DemoContext& ctx) {
    std::string btnText = (s_state.clicks > 0 && !s_state.text.empty())
        ? std::format("{} ({}x)", s_state.text, s_state.clicks)
        : s_state.text;
    if (ui.Button({
        .label = ctx.elemLabel,
        .text = btnText,
        .description = ctx.GetElementDescription(),
        .icon = s_state.hasIcon ? guikit::Icon::Play() : guikit::Icon{},
        .labelIcon = ctx.GetElementIcon(),
        .badge = ctx.GetElementBadge(),
        .variant = ctx.btnVariant,
        .iconPlacement = s_state.iconPlacement,
        .layout = ctx.elemLayout,
        .inlineOrder = ctx.elemInlineOrder,
        .elementAlign = ctx.elemAlign,
        .controlAlign = s_state.controlAlign,
        .stretch = ctx.GetElementStretch(),
        .width = ctx.GetElementWidth(),
        .divider = ctx.elemDivider,
        .dividerStyle = ctx.elemDividerStyle,
        .disabled = ctx.elemDisabled,
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
        "    .labelIcon = {},\n"
        "    .badge = {},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .iconPlacement = guikit::Placement::{},\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .controlAlign = guikit::Align::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .width = {},\n"
        "    .divider = {},\n"
        "    .dividerStyle = guikit::ElementDivider::{},\n"
        "    .disabled = {},\n"
        "    .active = false,\n"
        "    .size = guikit::UiSize::{},\n"
        "    .borderRadius = {:.1f}f\n"
        "}})) {{\n"
        "    // On click handler\n"
        "}}",
        ctx.elemLabel,
        s_state.text,
        ctx.FormatElementDescCode(),
        s_state.hasIcon ? "guikit::Icon::Play()" : "{}",
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        VariantName(ctx.btnVariant),
        PlacementName(s_state.iconPlacement),
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        AlignName(s_state.controlAlign),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.elemDivider ? "true" : "false",
        ctx.FormatDividerStyleCode(),
        ctx.elemDisabled ? "true" : "false",
        SizeName(ctx.controlsSize),
        s_state.cornerRadius
    );
}

} // namespace guikit::demo
