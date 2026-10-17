#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct ButtonState {
    std::string text = "Click me";
    bool hasIcon = true;
    guikit::Placement iconPlacement = guikit::Placement::Leading;
    guikit::Align controlAlign = guikit::Align::Center;
    bool square = false;
    int clicks = 0;
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

    static constexpr std::array<std::string_view, 3> alignNames = {"Start (Left)", "Center", "End (Right)"};
    size_t alignIdx = (s_state.controlAlign == guikit::Align::Start) ? 0 : ((s_state.controlAlign == guikit::Align::End) ? 2 : 1);
    if (ui.StepSlider(alignIdx, alignNames, {.label = "Content Alignment", .showTicks = true, .showLabels = true}).changed) {
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
    if (ui.Switch(s_state.square, {.label = "Square ToolButton"}) .changed) ui.InvalidateAll();
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
        .stretch = ctx.GetElementStretch(),
        .background = ctx.elemBackground,
        .borderWidth = ctx.elemBorderWidth,
        .borderRadius = ctx.GetElemBorderRadius(),
        .control = ctx.GetControlSpec(NavCategory::ElemButton, s_state.controlAlign),
        .disabled = ctx.elemDisabled,
        .square = s_state.square,
        .size = ctx.controlsSize
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
        "    .stretch = guikit::StretchMode::{},\n"
        "    .background = {},\n"
        "    .borderWidth = {:.1f}f,\n"
        "    .borderRadius = {},\n"
        "    .control = {},\n"
        "    .disabled = {},\n"
        "    .square = {},\n"
        "    .size = guikit::UiSize::{}\n"
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
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.elemBackground ? "true" : "false",
        ctx.elemBorderWidth,
        ctx.FormatElemBorderRadiusCode(),
        ctx.FormatControlSpecCode(NavCategory::ElemButton, s_state.controlAlign),
        ctx.elemDisabled ? "true" : "false",
        s_state.square ? "true" : "false",
        SizeName(ctx.controlsSize)
    );
}

} // namespace guikit::demo
