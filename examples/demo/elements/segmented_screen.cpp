#include "../demo_context.hpp"

namespace guikit::demo {

namespace {
struct SegmentedDemoState {
    keel::usize selectedIdx = 1;
    keel::u32 selectedMask = 0b0101u;
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

    static constexpr keel::StaticArray<keel::StringRef, 4> placeNames = {"Leading", "Trailing", "Top", "Bottom"};
    keel::usize placeIdx = static_cast<keel::usize>(s_state.iconPlacement);
    if (placeIdx > 3) placeIdx = 0;
    if (auto el = ui.Element({
        .label = "Icon Placement",
        .disabled = !(s_state.showIcons && s_state.showText)
    })) {
        if (ui.Segmented(placeIdx, placeNames, {
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
    const keel::StaticArray<guikit::SegmentedItem, 4> items = {{
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

    guikit::SegmentedOptions segOpts{};
    segOpts.equalWidth = s_state.equalWidth;
    segOpts.showDividers = s_state.showDividers;
    segOpts.disabled = ctx.elemDisabled;
    segOpts.size = ctx.controlsSize;
    segOpts.width = ctx.ResolveCtrlWidth();
    segOpts.height = ctx.ResolveCtrlHeight();
    segOpts.color = (ctx.btnVariant == guikit::UiVariant::Default) ? guikit::StateColor{} : ctx.GetControlColor();
    segOpts.background = ctx.GetControlBackground(NavCategory::ElemSegmented);
    segOpts.border = ctx.GetCtrlBorder();

    ctx.RunElementStage(ui, [&]() {
        if (s_state.multiSelect) {
            ui.SegmentedMask(s_state.selectedMask, items, segOpts);
        } else {
            ui.Segmented(s_state.selectedIdx, items, segOpts);
        }
    });
}

keel::String GenerateSegmentedCode(const DemoContext& ctx) {
    const keel::StringRef fnName = s_state.multiSelect ? "SegmentedMask" : "Segmented";
    const keel::StringRef varName = s_state.multiSelect ? "activeMask" : "selectedMode";
    const keel::FixedString<64> colorCode = (ctx.btnVariant == guikit::UiVariant::Default)
        ? keel::FixedString<64>("guikit::StateColor::Default()")
        : ctx.FormatControlColorCode();
    const bool hasCustomSize = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md);
    const keel::FixedString<64> sizeLine = hasCustomSize
        ? keel::format_fixed<64>("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};

    const keel::String ctrl = keel::format(
        "ui.{}({}, {{\n"
        "    {{.text = \"Sine\", .icon = guikit::Icon::Refresh()}},\n"
        "    {{.text = \"Square\", .icon = guikit::Icon::Settings()}},\n"
        "    {{.text = \"Sawtooth\", .icon = guikit::Icon::Play()}},\n"
        "    {{.text = \"Noise\", .icon = guikit::Icon::Warning(), .disabled = {}}}\n"
        "}}, {{\n"
        "    .equalWidth = {},\n"
        "    .showDividers = {},\n"
        "    .disabled = {},\n"
        "{}"
        "    .width = {},\n"
        "    .height = {},\n"
        "    .color = {},\n"
        "    .background = {},\n"
        "    .border = {}\n"
        "}});",
        fnName,
        varName,
        s_state.disableLastSegment ? "true" : "false",
        s_state.equalWidth ? "true" : "false",
        s_state.showDividers ? "true" : "false",
        ctx.elemDisabled ? "true" : "false",
        sizeLine,
        ctx.FormatCtrlWidthCode(),
        ctx.FormatCtrlHeightCode(),
        colorCode,
        ctx.FormatControlBackgroundCode(NavCategory::ElemSegmented),
        ctx.FormatCtrlBorderCode()
    );
    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
