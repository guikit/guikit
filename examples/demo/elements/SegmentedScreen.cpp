#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct SegmentedDemoState {
    size_t selectedIdx = 1;
    uint32_t selectedMask = 0b0101u;
    bool multiSelect = false;
    bool showIcons = true;
    bool showText = true;
    guikit::Placement iconPlacement = guikit::Placement::Leading;
    bool equalWidth = true;
    bool showDividers = true;
    bool disableLastSegment = false;
};
SegmentedDemoState s_state;
} // namespace

void RenderSegmentedInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({
        .label = "Multi-Toggle Mask Mode",
        .description = "Toggle multiple segments independently (ui.SegmentedMask)"
    })) {
        if (ui.Switch(s_state.multiSelect).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Show Segment Icons"})) {
        if (ui.Switch(s_state.showIcons).changed) {
            if (!s_state.showIcons && !s_state.showText) s_state.showText = true;
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Show Segment Labels"})) {
        if (ui.Switch(s_state.showText).changed) {
            if (!s_state.showText && !s_state.showIcons) s_state.showIcons = true;
            ui.InvalidateAll();
        }
    }

    static constexpr std::array<std::string_view, 4> placeNames = {"Leading", "Trailing", "Top", "Bottom"};
    size_t placeIdx = static_cast<size_t>(s_state.iconPlacement);
    if (placeIdx > 3) placeIdx = 0;
    if (auto el = ui.Element({
        .label = "Icon Placement",
        .disabled = !(s_state.showIcons && s_state.showText)
    })) {
        if (ui.StepSlider(placeIdx, placeNames, {
            .showTicks = true,
            .showLabels = true,
            .disabled = !(s_state.showIcons && s_state.showText)
        }).changed) {
            s_state.iconPlacement = static_cast<guikit::Placement>(placeIdx);
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({
        .label = "Equal Segment Widths",
        .description = "Uniform segment width vs natural content width"
    })) {
        if (ui.Switch(s_state.equalWidth).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Show Track Dividers"})) {
        if (ui.Switch(s_state.showDividers).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Disable Last Segment"})) {
        if (ui.Switch(s_state.disableLastSegment).changed) ui.InvalidateAll();
    }
}

void RenderSegmentedStage(guikit::Context& ui, DemoContext& ctx) {
    const std::array<guikit::SegmentedItem, 4> items = {{
        {
            .text = s_state.showText ? "Sine" : "",
            .icon = s_state.showIcons ? guikit::Icon::Refresh() : guikit::Icon{},
            .iconPlacement = s_state.iconPlacement,
            .tooltip = "Pure sinusoidal carrier"
        },
        {
            .text = s_state.showText ? "Square" : "",
            .icon = s_state.showIcons ? guikit::Icon::Settings() : guikit::Icon{},
            .iconPlacement = s_state.iconPlacement,
            .tooltip = "50% duty cycle pulse train"
        },
        {
            .text = s_state.showText ? "Sawtooth" : "",
            .icon = s_state.showIcons ? guikit::Icon::Play() : guikit::Icon{},
            .iconPlacement = s_state.iconPlacement,
            .tooltip = "Linear ramp waveform"
        },
        {
            .text = s_state.showText ? "Noise" : "",
            .icon = s_state.showIcons ? guikit::Icon::Warning() : guikit::Icon{},
            .iconPlacement = s_state.iconPlacement,
            .tooltip = "Wideband AWGN generator",
            .disabled = s_state.disableLastSegment
        }
    }};

    const guikit::SegmentedOptions segOpts{
        .color = (ctx.btnVariant == guikit::UiVariant::Default) ? guikit::StateColor{} : ctx.GetControlColor(),
        .stretch = ctx.GetElementStretch(),
        .width = ctx.GetElementWidth(),
        .height = ctx.GetElementHeight(),
        .background = ctx.GetControlBackground(NavCategory::ElemSegmented),
        .border = ctx.GetCtrlBorder(),
        .equalWidth = s_state.equalWidth,
        .showDividers = s_state.showDividers,
        .disabled = ctx.elemDisabled
    };

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
        if (s_state.multiSelect) {
            ui.Size(ctx.controlsSize).SegmentedMask(s_state.selectedMask, items, segOpts);
        } else {
            ui.Size(ctx.controlsSize).Segmented(s_state.selectedIdx, items, segOpts);
        }
    }
}

std::string GenerateSegmentedCode(const DemoContext& ctx) {
    const std::string fnName = s_state.multiSelect ? "SegmentedMask" : "Segmented";
    const std::string varName = s_state.multiSelect ? "activeMask" : "selectedMode";
    const std::string colorCode = (ctx.btnVariant == guikit::UiVariant::Default)
        ? "guikit::StateColor::Default()"
        : ctx.FormatControlColorCode();

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
        "    ui.{}{}({}, {{\n"
        "        {{.text = \"Sine\", .icon = guikit::Icon::Refresh()}},\n"
        "        {{.text = \"Square\", .icon = guikit::Icon::Settings()}},\n"
        "        {{.text = \"Sawtooth\", .icon = guikit::Icon::Play()}},\n"
        "        {{.text = \"Noise\", .icon = guikit::Icon::Warning(), .disabled = {}}}\n"
        "    }}, {{\n"
        "        .color = {},\n"
        "        .stretch = guikit::StretchMode::{},\n"
        "        .width = {},\n"
        "        .height = {},\n"
        "        .background = {},\n"
        "        .border = {},\n"
        "        .equalWidth = {},\n"
        "        .showDividers = {},\n"
        "        .disabled = {}\n"
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
        fnName,
        varName,
        s_state.disableLastSegment ? "true" : "false",
        colorCode,
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemSegmented),
        ctx.FormatCtrlBorderCode(),
        s_state.equalWidth ? "true" : "false",
        s_state.showDividers ? "true" : "false",
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
