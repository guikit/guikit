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
    if (ui.StepSlider(s_state.modeIdx, modeNames, {
        .label = "Track Control Mode",
        .description = "Slider / StepSlider / RangeSlider / ProgressBar",
        .showTicks = true,
        .showLabels = true
    }).changed) {
        ui.InvalidateAll();
    }

    if (ui.Switch(s_state.showValue, {.label = "Show Value Tag"}).changed) ui.InvalidateAll();
    static constexpr std::array<std::string_view, 5> valPlaceNames = {"Lead", "Trail", "Top", "Btm", "Thumb"};
    size_t valPlaceIdx = static_cast<size_t>(s_state.valuePlacement);
    if (ui.StepSlider(valPlaceIdx, valPlaceNames, {
        .label = "Value Placement",
        .showTicks = true,
        .showLabels = true,
        .disabled = !s_state.showValue
    }).changed) {
        s_state.valuePlacement = static_cast<guikit::Placement>(valPlaceIdx);
        ui.InvalidateAll();
    }
    if (ui.InputField(s_state.textLeft, {
        .label = "Text Left (Min)",
        .placeholder = "e.g. 0% / Min",
        .clearable = true
    }).changed) ui.InvalidateAll();
    if (ui.InputField(s_state.textRight, {
        .label = "Text Right (Max)",
        .placeholder = "e.g. 100% / Max",
        .clearable = true
    }).changed) ui.InvalidateAll();

    if (s_state.modeIdx == 0 || s_state.modeIdx == 2) {
        if (ui.Switch(s_state.quantizeStep, {.label = "Quantize Step (5.0%)"}).changed) ui.InvalidateAll();
    }

    if (s_state.modeIdx == 1) {
        if (ui.Switch(s_state.showTicks, {.label = "Show Step Ticks"}).changed) ui.InvalidateAll();
        if (ui.Switch(s_state.showLabels, {.label = "Show Step Labels"}).changed) ui.InvalidateAll();
    } else if (s_state.modeIdx == 2) {
        if (ui.Slider(s_state.rangeMinSpan, 0.0f, 40.0f, {
            .label = "Minimum Span",
            .valueFormat = "%.0f %%",
            .step = 5.0f
        }).changed) {
            ui.InvalidateAll();
        }
    } else if (s_state.modeIdx == 3) {
        if (ui.Slider(s_state.val, 0.0f, 100.0f, {
            .label = "Simulated Signal / Progress",
            .valueFormat = "%.1f %%"
        }).changed) {
            if (s_state.val > s_state.progressPeakVal) {
                s_state.progressPeakVal = s_state.val;
            }
            ui.InvalidateAll();
        }
        if (ui.Switch(s_state.progressSegmented, {
            .label = "Segmented LED / VU Meter",
            .description = "Discrete LED cells vs Smooth bar"
        }).changed) ui.InvalidateAll();
        if (s_state.progressSegmented) {
            if (ui.Slider(s_state.progressSegments, 8u, 36u, {
                .label = "LED Segment Count",
                .valueFormat = "%u segs"
            }).changed) ui.InvalidateAll();
        }
        if (ui.Slider(s_state.progressBarHeight, 4.0f, 20.0f, {
            .label = "Bar Thickness",
            .valueFormat = "%.0f px",
            .step = 1.0f
        }).changed) ui.InvalidateAll();
        if (ui.Switch(s_state.progressMultiZone, {
            .label = "3-Zone Colors (Green/Warn/Danger)"
        }).changed) ui.InvalidateAll();
        if (ui.Switch(s_state.progressShowPeak, {
            .label = "Show Peak-Hold Marker"
        }).changed) ui.InvalidateAll();
        if (s_state.progressShowPeak) {
            if (ui.Slider(s_state.progressPeakVal, 0.0f, 100.0f, {
                .label = "Peak-Hold Level",
                .valueFormat = "%.1f %%"
            }).changed) ui.InvalidateAll();
        }
    }
}

void RenderSliderStage(guikit::Context& ui, DemoContext& ctx) {
    const std::optional<double> stepOpt = s_state.quantizeStep ? std::optional<double>(5.0) : std::nullopt;
    if (s_state.modeIdx == 0) {
        ui.Slider(s_state.val, 0.0f, 100.0f, {
            .label = ctx.elemLabel,
            .textLeft = s_state.textLeft,
            .textRight = s_state.textRight,
            .description = ctx.GetElementDescription(),
            .icon = ctx.GetElementIcon(),
            .badge = ctx.GetElementBadge(),
            .valueFormat = "%.1f %%",
            .step = stepOpt,
            .layout = ctx.elemLayout,
            .inlineOrder = ctx.elemInlineOrder,
            .elementAlign = ctx.elemAlign,
            .showValue = s_state.showValue,
            .valuePlacement = s_state.valuePlacement,
            .variant = ctx.btnVariant,
            .size = ctx.controlsSize,
            .stretch = ctx.GetElementStretch(),
            .background = ctx.elemBackground,
            .borderWidth = ctx.elemBorderWidth,
            .borderRadius = ctx.GetElemBorderRadius(),
            .control = ctx.GetControlSpec(NavCategory::ElemSlider),
            .disabled = ctx.elemDisabled
        });
    } else if (s_state.modeIdx == 1) {
        static constexpr std::array<std::string_view, 5> stepItems = {"Off", "Low", "Mid", "High", "Max"};
        ui.StepSlider(s_state.stepIdx, stepItems, {
            .label = ctx.elemLabel,
            .textLeft = s_state.textLeft,
            .textRight = s_state.textRight,
            .description = ctx.GetElementDescription(),
            .icon = ctx.GetElementIcon(),
            .badge = ctx.GetElementBadge(),
            .layout = ctx.elemLayout,
            .inlineOrder = ctx.elemInlineOrder,
            .elementAlign = ctx.elemAlign,
            .showTicks = s_state.showTicks,
            .showLabels = s_state.showLabels,
            .showValue = s_state.showValue,
            .valuePlacement = s_state.valuePlacement,
            .variant = ctx.btnVariant,
            .size = ctx.controlsSize,
            .stretch = ctx.GetElementStretch(),
            .background = ctx.elemBackground,
            .borderWidth = ctx.elemBorderWidth,
            .borderRadius = ctx.GetElemBorderRadius(),
            .control = ctx.GetControlSpec(NavCategory::ElemSlider),
            .disabled = ctx.elemDisabled
        });
    } else if (s_state.modeIdx == 2) {
        ui.RangeSlider(s_state.rangeLow, s_state.rangeHigh, 0.0f, 100.0f, {
            .label = ctx.elemLabel,
            .textLeft = s_state.textLeft,
            .textRight = s_state.textRight,
            .description = ctx.GetElementDescription(),
            .icon = ctx.GetElementIcon(),
            .badge = ctx.GetElementBadge(),
            .valueFormat = "%.0f %%",
            .step = stepOpt,
            .minSpan = static_cast<double>(s_state.rangeMinSpan),
            .layout = ctx.elemLayout,
            .inlineOrder = ctx.elemInlineOrder,
            .elementAlign = ctx.elemAlign,
            .showValue = s_state.showValue,
            .valuePlacement = s_state.valuePlacement,
            .variant = ctx.btnVariant,
            .size = ctx.controlsSize,
            .stretch = ctx.GetElementStretch(),
            .background = ctx.elemBackground,
            .borderWidth = ctx.elemBorderWidth,
            .borderRadius = ctx.GetElemBorderRadius(),
            .control = ctx.GetControlSpec(NavCategory::ElemSlider),
            .disabled = ctx.elemDisabled
        });
    } else {
        ui.ProgressBar(s_state.val, 0.0, 100.0, {
            .label = ctx.elemLabel,
            .textLeft = s_state.textLeft,
            .textRight = s_state.textRight,
            .description = ctx.GetElementDescription(),
            .icon = ctx.GetElementIcon(),
            .badge = ctx.GetElementBadge(),
            .valueFormat = "%.1f %%",
            .peakHoldValue = s_state.progressShowPeak ? std::optional<double>(s_state.progressPeakVal) : std::nullopt,
            .segments = s_state.progressSegmented ? s_state.progressSegments : 0u,
            .trackHeight = s_state.progressBarHeight,
            .layout = ctx.elemLayout,
            .inlineOrder = ctx.elemInlineOrder,
            .elementAlign = ctx.elemAlign,
            .showValue = s_state.showValue,
            .valuePlacement = s_state.valuePlacement,
            .variant = ctx.btnVariant,
            .size = ctx.controlsSize,
            .stretch = ctx.GetElementStretch(),
            .multiZoneColor = s_state.progressMultiZone,
            .background = ctx.elemBackground,
            .borderWidth = ctx.elemBorderWidth,
            .borderRadius = ctx.GetElemBorderRadius(),
            .control = ctx.GetControlSpec(NavCategory::ElemSlider),
            .disabled = ctx.elemDisabled
        });
    }
}

std::string GenerateSliderCode(const DemoContext& ctx) {
    if (s_state.modeIdx == 0) {
        return std::format(
            "ui.Slider(gainValue, 0.0f, 100.0f, {{\n"
            "    .label = \"{}\",\n"
            "    .textLeft = \"{}\",\n"
            "    .textRight = \"{}\",\n"
            "    .description = {},\n"
            "    .icon = {},\n"
            "    .badge = {},\n"
            "    .valueFormat = \"%.1f %%\",\n"
            "    .step = {},\n"
            "    .layout = guikit::ElementLayout::{},\n"
            "    .inlineOrder = guikit::InlineOrder::{},\n"
            "    .elementAlign = guikit::Align::{},\n"
            "    .showValue = {},\n"
            "    .valuePlacement = guikit::Placement::{},\n"
            "    .variant = guikit::UiVariant::{},\n"
            "    .size = guikit::UiSize::{},\n"
            "    .stretch = guikit::StretchMode::{},\n"
            "    .background = {},\n"
            "    .borderWidth = {:.1f}f,\n"
            "    .borderRadius = {},\n"
            "    .control = {},\n"
            "    .disabled = {}\n"
            "}});",
            ctx.elemLabel,
            s_state.textLeft,
            s_state.textRight,
            ctx.FormatElementDescCode(),
            ctx.FormatElementIconCode(),
            ctx.FormatElementBadgeCode(),
            s_state.quantizeStep ? "5.0" : "std::nullopt",
            ElementLayoutEnumName(ctx.elemLayout),
            InlineOrderEnumName(ctx.elemInlineOrder),
            LabelAlignEnumName(ctx.elemAlign),
            s_state.showValue ? "true" : "false",
            PlacementName(s_state.valuePlacement),
            VariantName(ctx.btnVariant),
            SizeName(ctx.controlsSize),
            StretchModeEnumName(ctx.GetElementStretch()),
            ctx.elemBackground ? "true" : "false",
            ctx.elemBorderWidth,
            ctx.FormatElemBorderRadiusCode(),
            ctx.FormatControlSpecCode(NavCategory::ElemSlider),
            ctx.elemDisabled ? "true" : "false"
        );
    }
    if (s_state.modeIdx == 1) {
        return std::format(
            "static constexpr std::array<std::string_view, 5> stepItems = {{\"Off\", \"Low\", \"Mid\", \"High\", \"Max\"}};\n"
            "ui.StepSlider(presetIndex, stepItems, {{\n"
            "    .label = \"{}\",\n"
            "    .textLeft = \"{}\",\n"
            "    .textRight = \"{}\",\n"
            "    .description = {},\n"
            "    .icon = {},\n"
            "    .badge = {},\n"
            "    .layout = guikit::ElementLayout::{},\n"
            "    .inlineOrder = guikit::InlineOrder::{},\n"
            "    .elementAlign = guikit::Align::{},\n"
            "    .showTicks = {},\n"
            "    .showLabels = {},\n"
            "    .showValue = {},\n"
            "    .valuePlacement = guikit::Placement::{},\n"
            "    .variant = guikit::UiVariant::{},\n"
            "    .size = guikit::UiSize::{},\n"
            "    .stretch = guikit::StretchMode::{},\n"
            "    .background = {},\n"
            "    .borderWidth = {:.1f}f,\n"
            "    .borderRadius = {},\n"
            "    .control = {},\n"
            "    .disabled = {}\n"
            "}});",
            ctx.elemLabel,
            s_state.textLeft,
            s_state.textRight,
            ctx.FormatElementDescCode(),
            ctx.FormatElementIconCode(),
            ctx.FormatElementBadgeCode(),
            ElementLayoutEnumName(ctx.elemLayout),
            InlineOrderEnumName(ctx.elemInlineOrder),
            LabelAlignEnumName(ctx.elemAlign),
            s_state.showTicks ? "true" : "false",
            s_state.showLabels ? "true" : "false",
            s_state.showValue ? "true" : "false",
            PlacementName(s_state.valuePlacement),
            VariantName(ctx.btnVariant),
            SizeName(ctx.controlsSize),
            StretchModeEnumName(ctx.GetElementStretch()),
            ctx.elemBackground ? "true" : "false",
            ctx.elemBorderWidth,
            ctx.FormatElemBorderRadiusCode(),
            ctx.FormatControlSpecCode(NavCategory::ElemSlider),
            ctx.elemDisabled ? "true" : "false"
        );
    }
    if (s_state.modeIdx == 2) {
        return std::format(
            "ui.RangeSlider(bandLow, bandHigh, 0.0f, 100.0f, {{\n"
            "    .label = \"{}\",\n"
            "    .textLeft = \"{}\",\n"
            "    .textRight = \"{}\",\n"
            "    .description = {},\n"
            "    .icon = {},\n"
            "    .badge = {},\n"
            "    .valueFormat = \"%.0f %%\",\n"
            "    .step = {},\n"
            "    .minSpan = {:.1f},\n"
            "    .layout = guikit::ElementLayout::{},\n"
            "    .inlineOrder = guikit::InlineOrder::{},\n"
            "    .elementAlign = guikit::Align::{},\n"
            "    .showValue = {},\n"
            "    .valuePlacement = guikit::Placement::{},\n"
            "    .variant = guikit::UiVariant::{},\n"
            "    .size = guikit::UiSize::{},\n"
            "    .stretch = guikit::StretchMode::{},\n"
            "    .background = {},\n"
            "    .borderWidth = {:.1f}f,\n"
            "    .borderRadius = {},\n"
            "    .control = {},\n"
            "    .disabled = {}\n"
            "}});",
            ctx.elemLabel,
            s_state.textLeft,
            s_state.textRight,
            ctx.FormatElementDescCode(),
            ctx.FormatElementIconCode(),
            ctx.FormatElementBadgeCode(),
            s_state.quantizeStep ? "5.0" : "std::nullopt",
            s_state.rangeMinSpan,
            ElementLayoutEnumName(ctx.elemLayout),
            InlineOrderEnumName(ctx.elemInlineOrder),
            LabelAlignEnumName(ctx.elemAlign),
            s_state.showValue ? "true" : "false",
            PlacementName(s_state.valuePlacement),
            VariantName(ctx.btnVariant),
            SizeName(ctx.controlsSize),
            StretchModeEnumName(ctx.GetElementStretch()),
            ctx.elemBackground ? "true" : "false",
            ctx.elemBorderWidth,
            ctx.FormatElemBorderRadiusCode(),
            ctx.FormatControlSpecCode(NavCategory::ElemSlider),
            ctx.elemDisabled ? "true" : "false"
        );
    }
    return std::format(
        "ui.ProgressBar(signalLevel, 0.0, 100.0, {{\n"
        "    .label = \"{}\",\n"
        "    .textLeft = \"{}\",\n"
        "    .textRight = \"{}\",\n"
        "    .description = {},\n"
        "    .icon = {},\n"
        "    .badge = {},\n"
        "    .valueFormat = \"%.1f %%\",\n"
        "    .peakHoldValue = {},\n"
        "    .segments = {},\n"
        "    .trackHeight = {:.1f}f,\n"
        "    .layout = guikit::ElementLayout::{},\n"
        "    .inlineOrder = guikit::InlineOrder::{},\n"
        "    .elementAlign = guikit::Align::{},\n"
        "    .showValue = {},\n"
        "    .valuePlacement = guikit::Placement::{},\n"
        "    .variant = guikit::UiVariant::{},\n"
        "    .size = guikit::UiSize::{},\n"
        "    .stretch = guikit::StretchMode::{},\n"
        "    .multiZoneColor = {},\n"
        "    .background = {},\n"
        "    .borderWidth = {:.1f}f,\n"
        "    .borderRadius = {},\n"
        "    .control = {},\n"
        "    .disabled = {}\n"
        "}});",
        ctx.elemLabel,
        s_state.textLeft,
        s_state.textRight,
        ctx.FormatElementDescCode(),
        ctx.FormatElementIconCode(),
        ctx.FormatElementBadgeCode(),
        s_state.progressShowPeak ? std::format("{:.1f}", s_state.progressPeakVal) : "std::nullopt",
        s_state.progressSegmented ? s_state.progressSegments : 0u,
        s_state.progressBarHeight,
        ElementLayoutEnumName(ctx.elemLayout),
        InlineOrderEnumName(ctx.elemInlineOrder),
        LabelAlignEnumName(ctx.elemAlign),
        s_state.showValue ? "true" : "false",
        PlacementName(s_state.valuePlacement),
        VariantName(ctx.btnVariant),
        SizeName(ctx.controlsSize),
        StretchModeEnumName(ctx.GetElementStretch()),
        s_state.progressMultiZone ? "true" : "false",
        ctx.elemBackground ? "true" : "false",
        ctx.elemBorderWidth,
        ctx.FormatElemBorderRadiusCode(),
        ctx.FormatControlSpecCode(NavCategory::ElemSlider),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
