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
    if (auto el = ui.Element({.label = "Capsule Text"})) {
        if (ui.InputField(s_state.text, {
            .placeholder = "Icon-only...",
            .clearable = true
        }).changed) {
            s_state.clicks = 0;
            ui.InvalidateAll();
        }
    }

    static constexpr std::array<std::string_view, 3> alignNames = {"Start (Left)", "Center", "End (Right)"};
    size_t alignIdx = (s_state.controlAlign == guikit::Align::Start) ? 0 : ((s_state.controlAlign == guikit::Align::End) ? 2 : 1);
    if (auto el = ui.Element({.label = "Content Alignment"})) {
        if (ui.StepSlider(alignIdx, alignNames, {.showTicks = true, .showLabels = true}).changed) {
            s_state.controlAlign = (alignIdx == 0) ? guikit::Align::Start : ((alignIdx == 2) ? guikit::Align::End : guikit::Align::Center);
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Show Capsule Icon"})) {
        if (ui.Switch(s_state.hasIcon).changed) ui.InvalidateAll();
    }
    static constexpr std::array<std::string_view, 4> placeNames = {"Leading", "Trailing", "Top", "Bottom"};
    size_t placeIdx = (s_state.iconPlacement == guikit::Placement::Trailing) ? 1 : ((s_state.iconPlacement == guikit::Placement::Top) ? 2 : ((s_state.iconPlacement == guikit::Placement::Bottom) ? 3 : 0));
    if (auto el = ui.Element({.label = "Icon Placement", .disabled = !s_state.hasIcon})) {
        if (ui.StepSlider(placeIdx, placeNames, {.showTicks = true, .showLabels = true, .disabled = !s_state.hasIcon}).changed) {
            s_state.iconPlacement = (placeIdx == 1) ? guikit::Placement::Trailing : ((placeIdx == 2) ? guikit::Placement::Top : ((placeIdx == 3) ? guikit::Placement::Bottom : guikit::Placement::Leading));
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Square ToolButton"})) {
        if (ui.Switch(s_state.square).changed) ui.InvalidateAll();
    }
}

void RenderButtonStage(guikit::Context& ui, DemoContext& ctx) {
    std::string btnText = (s_state.clicks > 0 && !s_state.text.empty())
        ? std::format("{} ({}x)", s_state.text, s_state.clicks)
        : s_state.text;
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
        if (ui.Size(ctx.controlsSize).Button({
            .text = btnText,
            .icon = s_state.hasIcon ? guikit::Icon::Play() : guikit::Icon{},
            .color = ctx.GetControlColor(),
            .iconPlacement = s_state.iconPlacement,
            .align = s_state.controlAlign,
            .stretch = ctx.GetElementStretch(),
            .width = ctx.GetElementWidth(),
            .height = ctx.GetElementHeight(),
            .background = ctx.GetControlBackground(NavCategory::ElemButton),
            .border = ctx.GetCtrlBorder(),
            .disabled = ctx.elemDisabled,
            .square = s_state.square
        })) {
            ++s_state.clicks;
            ui.InvalidateAll();
        }
    }
}

std::string GenerateButtonCode(const DemoContext& ctx) {
    return std::format(
        "if (auto el = ui.Size(guikit::UiSize::{}).Element({{\n"
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
        "    if (ui.Size(guikit::UiSize::{}).Button({{\n"
        "        .text = \"{}\",\n"
        "        .icon = {},\n"
        "        .color = {},\n"
        "        .iconPlacement = guikit::Placement::{},\n"
        "        .align = guikit::Align::{},\n"
        "        .stretch = guikit::StretchMode::{},\n"
        "        .width = {},\n"
        "        .height = {},\n"
        "        .background = {},\n"
        "        .border = {},\n"
        "        .disabled = {},\n"
        "        .square = {}\n"
        "    }})) {{\n"
        "        // On click handler\n"
        "    }}\n"
        "}}",
        SizeName(ctx.elemSize),
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
        SizeName(ctx.controlsSize),
        s_state.text,
        s_state.hasIcon ? "guikit::Icon::Play()" : "{}",
        ctx.FormatControlColorCode(),
        PlacementName(s_state.iconPlacement),
        AlignName(s_state.controlAlign),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemButton),
        ctx.FormatCtrlBorderCode(),
        ctx.elemDisabled ? "true" : "false",
        s_state.square ? "true" : "false"
    );
}

} // namespace guikit::demo
