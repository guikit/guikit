#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct SliderDemoState {
    size_t modeIdx = 0; // 0 = Slider, 1 = StepSlider, 2 = RangeSlider, 3 = ProgressBar
    float val = 65.0f;
    size_t stepIdx = 2;
    float rangeLow = 22.0f;
    float rangeHigh = 78.0f;
    float rangeMinSpan = 10.0f;
    bool quantizeStep = false;

    uint32_t progressSegments = 20;
    bool progressSegmented = false;
    bool progressMultiZone = true;
    bool progressShowPeak = true;
    float progressPeakVal = 86.0f;
    float progressBarHeight = 10.0f;

    bool showTicks = true;
    bool showLabels = true;
    bool showValue = true;
    guikit::Placement valuePlacement = guikit::Placement::Trailing;
    std::string textLeft = "";
    std::string textRight = "";
};
SliderDemoState s_state;
} // namespace

void RenderSliderInspector(guikit::Context& ui, DemoContext&) {
    static constexpr std::array<std::string_view, 4> modeNames = {"Slider", "Step", "Range", "Progress"};
    if (auto el = ui.Element({
        .label = "Track Control Mode",
        .description = "Slider / StepSlider / RangeSlider / ProgressBar"
    })) {
        if (ui.StepSlider(s_state.modeIdx, modeNames, {
            .showTicks = true,
            .showLabels = true
        }).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Show Value Tag"})) {
        if (ui.Switch(s_state.showValue).changed) ui.InvalidateAll();
    }
    static constexpr std::array<std::string_view, 5> valPlaceNames = {"Lead", "Trail", "Top", "Btm", "Thumb"};
    size_t valPlaceIdx = static_cast<size_t>(s_state.valuePlacement);
    if (auto el = ui.Element({.label = "Value Placement", .disabled = !s_state.showValue})) {
        if (ui.StepSlider(valPlaceIdx, valPlaceNames, {
            .showTicks = true,
            .showLabels = true,
            .disabled = !s_state.showValue
        }).changed) {
            s_state.valuePlacement = static_cast<guikit::Placement>(valPlaceIdx);
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Text Left (Min)"})) {
        if (ui.InputField(s_state.textLeft, {
            .placeholder = "e.g. 0% / Min",
            .clearable = true
        }).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Text Right (Max)"})) {
        if (ui.InputField(s_state.textRight, {
            .placeholder = "e.g. 100% / Max",
            .clearable = true
        }).changed) ui.InvalidateAll();
    }

    if (s_state.modeIdx == 0 || s_state.modeIdx == 2) {
        if (auto el = ui.Element({.label = "Quantize Step (5.0%)"})) {
            if (ui.Switch(s_state.quantizeStep).changed) ui.InvalidateAll();
        }
    }

    if (s_state.modeIdx == 1) {
        if (auto el = ui.Element({.label = "Show Step Ticks"})) {
            if (ui.Switch(s_state.showTicks).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Show Step Labels"})) {
            if (ui.Switch(s_state.showLabels).changed) ui.InvalidateAll();
        }
    } else if (s_state.modeIdx == 2) {
        if (auto el = ui.Element({.label = "Minimum Span"})) {
            if (ui.Slider(s_state.rangeMinSpan, 0.0f, 40.0f, {
                .valueFormat = "%.0f %%",
                .step = 5.0f
            }).changed) {
                ui.InvalidateAll();
            }
        }
    } else if (s_state.modeIdx == 3) {
        if (auto el = ui.Element({.label = "Simulated Signal / Progress"})) {
            if (ui.Slider(s_state.val, 0.0f, 100.0f, {
                .valueFormat = "%.1f %%"
            }).changed) {
                if (s_state.val > s_state.progressPeakVal) {
                    s_state.progressPeakVal = s_state.val;
                }
                ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({
            .label = "Segmented LED / VU Meter",
            .description = "Discrete LED cells vs Smooth bar"
        })) {
            if (ui.Switch(s_state.progressSegmented).changed) ui.InvalidateAll();
        }
        if (s_state.progressSegmented) {
            if (auto el = ui.Element({.label = "LED Segment Count"})) {
                if (ui.Slider(s_state.progressSegments, 8u, 36u, {
                    .valueFormat = "%u segs"
                }).changed) ui.InvalidateAll();
            }
        }
        if (auto el = ui.Element({.label = "Bar Thickness"})) {
            if (ui.Slider(s_state.progressBarHeight, 4.0f, 20.0f, {
                .valueFormat = "%.0f px",
                .step = 1.0f
            }).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "3-Zone Colors (Green/Warn/Danger)"})) {
            if (ui.Switch(s_state.progressMultiZone).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Show Peak-Hold Marker"})) {
            if (ui.Switch(s_state.progressShowPeak).changed) ui.InvalidateAll();
        }
        if (s_state.progressShowPeak) {
            if (auto el = ui.Element({.label = "Peak-Hold Level"})) {
                if (ui.Slider(s_state.progressPeakVal, 0.0f, 100.0f, {
                    .valueFormat = "%.1f %%"
                }).changed) ui.InvalidateAll();
            }
        }
    }
}

void RenderSliderStage(guikit::Context& ui, DemoContext& ctx) {
    const std::optional<double> stepOpt = s_state.quantizeStep ? std::optional<double>(5.0) : std::nullopt;
    if (auto el = ui.Element({
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
        .size = ctx.controlsSize,
        .disabled = ctx.elemDisabled
    })) {
        if (s_state.modeIdx == 0) {
            ui.Slider(s_state.val, 0.0f, 100.0f, {
                .textLeft = s_state.textLeft,
                .textRight = s_state.textRight,
                .valueFormat = "%.1f %%",
                .step = stepOpt,
                .showValue = s_state.showValue,
                .valuePlacement = s_state.valuePlacement,
                .color = ctx.GetControlColor(),
                .size = ctx.controlsSize,
                .stretch = ctx.GetElementStretch(),
                .width = ctx.GetElementWidth(),
                .height = ctx.GetElementHeight(),
                .background = ctx.GetControlBackground(NavCategory::ElemSlider),
                .border = ctx.GetCtrlBorder(),
                .disabled = ctx.elemDisabled
            });
        } else if (s_state.modeIdx == 1) {
            static constexpr std::array<std::string_view, 5> stepItems = {"Off", "Low", "Mid", "High", "Max"};
            ui.StepSlider(s_state.stepIdx, stepItems, {
                .textLeft = s_state.textLeft,
                .textRight = s_state.textRight,
                .showTicks = s_state.showTicks,
                .showLabels = s_state.showLabels,
                .showValue = s_state.showValue,
                .valuePlacement = s_state.valuePlacement,
                .color = ctx.GetControlColor(),
                .size = ctx.controlsSize,
                .stretch = ctx.GetElementStretch(),
                .width = ctx.GetElementWidth(),
                .height = ctx.GetElementHeight(),
                .background = ctx.GetControlBackground(NavCategory::ElemSlider),
                .border = ctx.GetCtrlBorder(),
                .disabled = ctx.elemDisabled
            });
        } else if (s_state.modeIdx == 2) {
            ui.RangeSlider(s_state.rangeLow, s_state.rangeHigh, 0.0f, 100.0f, {
                .textLeft = s_state.textLeft,
                .textRight = s_state.textRight,
                .valueFormat = "%.0f %%",
                .step = stepOpt,
                .minSpan = static_cast<double>(s_state.rangeMinSpan),
                .showValue = s_state.showValue,
                .valuePlacement = s_state.valuePlacement,
                .color = ctx.GetControlColor(),
                .size = ctx.controlsSize,
                .stretch = ctx.GetElementStretch(),
                .width = ctx.GetElementWidth(),
                .height = ctx.GetElementHeight(),
                .background = ctx.GetControlBackground(NavCategory::ElemSlider),
                .border = ctx.GetCtrlBorder(),
                .disabled = ctx.elemDisabled
            });
        } else {
            ui.ProgressBar(s_state.val, 0.0, 100.0, {
                .textLeft = s_state.textLeft,
                .textRight = s_state.textRight,
                .valueFormat = "%.1f %%",
                .peakHoldValue = s_state.progressShowPeak ? std::optional<double>(s_state.progressPeakVal) : std::nullopt,
                .segments = s_state.progressSegmented ? s_state.progressSegments : 0u,
                .trackHeight = s_state.progressBarHeight,
                .showValue = s_state.showValue,
                .valuePlacement = s_state.valuePlacement,
                .color = ctx.GetControlColor(),
                .size = ctx.controlsSize,
                .stretch = ctx.GetElementStretch(),
                .width = ctx.GetElementWidth(),
                .height = ctx.GetElementHeight(),
                .multiZoneColor = s_state.progressMultiZone,
                .background = ctx.GetControlBackground(NavCategory::ElemSlider),
                .border = ctx.GetCtrlBorder(),
                .disabled = ctx.elemDisabled
            });
        }
    }
}

std::string GenerateSliderCode(const DemoContext& ctx) {
    if (s_state.modeIdx == 0) {
        return std::format(
            "if (auto el = ui.Element({{\n"
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
            "    .size = guikit::UiSize::{},\n"
            "    .disabled = {}\n"
            "}})) {{\n"
            "    ui.Slider(gainValue, 0.0f, 100.0f, {{\n"
            "        .textLeft = \"{}\",\n"
            "        .textRight = \"{}\",\n"
            "        .valueFormat = \"%.1f %%\",\n"
            "        .step = {},\n"
            "        .showValue = {},\n"
            "        .valuePlacement = guikit::Placement::{},\n"
            "        .color = {},\n"
            "        .size = guikit::UiSize::{},\n"
            "        .stretch = guikit::StretchMode::{},\n"
            "        .width = {},\n"
            "        .height = {},\n"
            "        .background = {},\n"
            "        .border = {},\n"
            "        .disabled = {}\n"
            "    }});\n"
            "}}",
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
            SizeName(ctx.controlsSize),
            ctx.elemDisabled ? "true" : "false",
            s_state.textLeft,
            s_state.textRight,
            s_state.quantizeStep ? "5.0" : "std::nullopt",
            s_state.showValue ? "true" : "false",
            PlacementName(s_state.valuePlacement),
            ctx.FormatControlColorCode(),
            SizeName(ctx.controlsSize),
            StretchModeEnumName(ctx.GetElementStretch()),
            ctx.FormatElementWidthCode(),
            ctx.FormatElementHeightCode(),
            ctx.FormatControlBackgroundCode(NavCategory::ElemSlider),
            ctx.FormatCtrlBorderCode(),
            ctx.elemDisabled ? "true" : "false"
        );
    }
    if (s_state.modeIdx == 1) {
        return std::format(
            "static constexpr std::array<std::string_view, 5> stepItems = {{\"Off\", \"Low\", \"Mid\", \"High\", \"Max\"}};\n"
            "if (auto el = ui.Element({{\n"
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
            "    .size = guikit::UiSize::{},\n"
            "    .disabled = {}\n"
            "}})) {{\n"
            "    ui.StepSlider(presetIndex, stepItems, {{\n"
            "        .textLeft = \"{}\",\n"
            "        .textRight = \"{}\",\n"
            "        .showTicks = {},\n"
            "        .showLabels = {},\n"
            "        .showValue = {},\n"
            "        .valuePlacement = guikit::Placement::{},\n"
            "        .color = {},\n"
            "        .size = guikit::UiSize::{},\n"
            "        .stretch = guikit::StretchMode::{},\n"
            "        .width = {},\n"
            "        .height = {},\n"
            "        .background = {},\n"
            "        .border = {},\n"
            "        .disabled = {}\n"
            "    }});\n"
            "}}",
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
            SizeName(ctx.controlsSize),
            ctx.elemDisabled ? "true" : "false",
            s_state.textLeft,
            s_state.textRight,
            s_state.showTicks ? "true" : "false",
            s_state.showLabels ? "true" : "false",
            s_state.showValue ? "true" : "false",
            PlacementName(s_state.valuePlacement),
            ctx.FormatControlColorCode(),
            SizeName(ctx.controlsSize),
            StretchModeEnumName(ctx.GetElementStretch()),
            ctx.FormatElementWidthCode(),
            ctx.FormatElementHeightCode(),
            ctx.FormatControlBackgroundCode(NavCategory::ElemSlider),
            ctx.FormatCtrlBorderCode(),
            ctx.elemDisabled ? "true" : "false"
        );
    }
    if (s_state.modeIdx == 2) {
        return std::format(
            "if (auto el = ui.Element({{\n"
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
            "    .size = guikit::UiSize::{},\n"
            "    .disabled = {}\n"
            "}})) {{\n"
            "    ui.RangeSlider(bandLow, bandHigh, 0.0f, 100.0f, {{\n"
            "        .textLeft = \"{}\",\n"
            "        .textRight = \"{}\",\n"
            "        .valueFormat = \"%.0f %%\",\n"
            "        .step = {},\n"
            "        .minSpan = {:.1f},\n"
            "        .showValue = {},\n"
            "        .valuePlacement = guikit::Placement::{},\n"
            "        .color = {},\n"
            "        .size = guikit::UiSize::{},\n"
            "        .stretch = guikit::StretchMode::{},\n"
            "        .width = {},\n"
            "        .height = {},\n"
            "        .background = {},\n"
            "        .border = {},\n"
            "        .disabled = {}\n"
            "    }});\n"
            "}}",
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
            SizeName(ctx.controlsSize),
            ctx.elemDisabled ? "true" : "false",
            s_state.textLeft,
            s_state.textRight,
            s_state.quantizeStep ? "5.0" : "std::nullopt",
            s_state.rangeMinSpan,
            s_state.showValue ? "true" : "false",
            PlacementName(s_state.valuePlacement),
            ctx.FormatControlColorCode(),
            SizeName(ctx.controlsSize),
            StretchModeEnumName(ctx.GetElementStretch()),
            ctx.FormatElementWidthCode(),
            ctx.FormatElementHeightCode(),
            ctx.FormatControlBackgroundCode(NavCategory::ElemSlider),
            ctx.FormatCtrlBorderCode(),
            ctx.elemDisabled ? "true" : "false"
        );
    }
    return std::format(
        "if (auto el = ui.Element({{\n"
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
        "    .size = guikit::UiSize::{},\n"
        "    .disabled = {}\n"
        "}})) {{\n"
        "    ui.ProgressBar(signalLevel, 0.0, 100.0, {{\n"
        "        .textLeft = \"{}\",\n"
        "        .textRight = \"{}\",\n"
        "        .valueFormat = \"%.1f %%\",\n"
        "        .peakHoldValue = {},\n"
        "        .segments = {},\n"
        "        .trackHeight = {:.1f}f,\n"
        "        .showValue = {},\n"
        "        .valuePlacement = guikit::Placement::{},\n"
        "        .color = {},\n"
        "        .size = guikit::UiSize::{},\n"
        "        .stretch = guikit::StretchMode::{},\n"
        "        .width = {},\n"
        "        .height = {},\n"
        "        .multiZoneColor = {},\n"
        "        .background = {},\n"
        "        .border = {},\n"
        "        .disabled = {}\n"
        "    }});\n"
        "}}",
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
        SizeName(ctx.controlsSize),
        ctx.elemDisabled ? "true" : "false",
        s_state.textLeft,
        s_state.textRight,
        s_state.progressShowPeak ? std::format("{:.1f}", s_state.progressPeakVal) : "std::nullopt",
        s_state.progressSegmented ? s_state.progressSegments : 0u,
        s_state.progressBarHeight,
        s_state.showValue ? "true" : "false",
        PlacementName(s_state.valuePlacement),
        ctx.FormatControlColorCode(),
        SizeName(ctx.controlsSize),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        s_state.progressMultiZone ? "true" : "false",
        ctx.FormatControlBackgroundCode(NavCategory::ElemSlider),
        ctx.FormatCtrlBorderCode(),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
