#include "../demo_context.hpp"

namespace guikit::demo {

namespace {
struct SliderDemoState {
    keel::usize modeIdx = 0; // 0 = Slider, 1 = StepSlider, 2 = RangeSlider, 3 = ProgressBar
    float val = 65.0f;
    keel::usize stepIdx = 2;
    float rangeLow = 22.0f;
    float rangeHigh = 78.0f;
    float rangeMinSpan = 10.0f;
    bool quantizeStep = false;

    keel::u32 progressSegments = 20;
    bool progressSegmented = false;
    bool progressMultiZone = true;
    bool progressShowPeak = true;
    float progressPeakVal = 86.0f;
    float progressBarHeight = 10.0f;

    bool showTicks = true;
    bool showLabels = true;
    bool showValue = true;
    guikit::Orientation orientation = guikit::Orientation::Horizontal;
    bool compactVertical = false;
    guikit::Placement valuePlacement = guikit::Placement::Trailing;
    keel::SmallString<64> textLeft{""};
    keel::SmallString<64> textRight{""};
};
SliderDemoState s_state;

constexpr keel::StringRef OrientationEnumName(guikit::Orientation o) noexcept {
    return (o == guikit::Orientation::Vertical) ? "Vertical" : "Horizontal";
}
} // namespace

void RenderSliderInspector(guikit::Context& ui, DemoContext&) {
    static constexpr keel::StaticArray<keel::StringRef, 4> modeNames = {"Slider", "Step", "Range", "Progress"};
    if (auto el = ui.Element({
        .label = "Track Control Mode",
        .description = "Slider / StepSlider / RangeSlider / ProgressBar"
    })) {
        if (ui.Segmented(s_state.modeIdx, modeNames).changed) {
            ui.InvalidateAll();
        }
    }

    keel::i32 orientIdx = (s_state.orientation == guikit::Orientation::Vertical) ? 1 : 0;
    static constexpr keel::StaticArray<keel::StringRef, 2> orientNames = {"Horizontal", "Vertical (Fader)"};
    if (auto el = ui.Element({.label = "Orientation"})) {
        if (ui.Segmented(orientIdx, orientNames).changed) {
            s_state.orientation = (orientIdx == 1)
                ? guikit::Orientation::Vertical
                : guikit::Orientation::Horizontal;
            ui.InvalidateAll();
        }
    }

    if (s_state.orientation == guikit::Orientation::Vertical) {
        if (auto el = ui.Element({
            .label = "Compact Vertical Height (Fit)",
            .description = "Pass .height = SizeMode::Fit instead of stretching"
        })) {
            if (ui.Switch(s_state.compactVertical).changed) {
                ui.InvalidateAll();
            }
        }
    }

    if (auto el = ui.Element({.label = "Show Value Tag"})) {
        if (ui.Switch(s_state.showValue).changed) ui.InvalidateAll();
    }
    static constexpr keel::StaticArray<keel::StringRef, 5> valPlaceNames = {
        "Leading", "Trailing", "Top", "Bottom", "Thumb"
    };
    keel::usize valPlaceIdx = static_cast<keel::usize>(s_state.valuePlacement);
    if (auto el = ui.Element({.label = "Value Placement", .disabled = !s_state.showValue})) {
        if (ui.Select(valPlaceIdx, valPlaceNames, {
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
                .valueFormat = "%.0f dp",
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
    const keel::Optional<double> stepOpt = s_state.quantizeStep ? keel::Optional<double>(5.0) : keel::kNullOpt;
    const guikit::AxisSize wSize = ctx.ResolveCtrlWidth();
    const guikit::AxisSize hSize =
        (s_state.orientation == guikit::Orientation::Vertical && s_state.compactVertical && ctx.ResolveCtrlHeight().IsAuto())
            ? guikit::AxisSize(guikit::SizeMode::Fit)
            : ctx.ResolveCtrlHeight();

    ctx.RunElementStage(ui, [&]() {
        if (s_state.modeIdx == 0) {
            guikit::SliderOptions opts{};
            opts.orientation = s_state.orientation;
            opts.textLeft = s_state.textLeft;
            opts.textRight = s_state.textRight;
            opts.valueFormat = "%.1f %%";
            opts.step = stepOpt;
            opts.showValue = s_state.showValue;
            opts.valuePlacement = s_state.valuePlacement;
            opts.disabled = ctx.elemDisabled;
            opts.size = ctx.controlsSize;
            opts.width = wSize;
            opts.height = hSize;
            opts.color = ctx.GetControlColor();
            ui.Slider(s_state.val, 0.0f, 100.0f, opts);
        } else if (s_state.modeIdx == 1) {
            static constexpr keel::StaticArray<keel::StringRef, 5> stepItems = {"Off", "Low", "Mid", "High", "Max"};
            guikit::StepSliderOptions opts{};
            opts.orientation = s_state.orientation;
            opts.textLeft = s_state.textLeft;
            opts.textRight = s_state.textRight;
            opts.showTicks = s_state.showTicks;
            opts.showLabels = s_state.showLabels;
            opts.showValue = s_state.showValue;
            opts.valuePlacement = s_state.valuePlacement;
            opts.disabled = ctx.elemDisabled;
            opts.size = ctx.controlsSize;
            opts.width = wSize;
            opts.height = hSize;
            opts.color = ctx.GetControlColor();
            ui.StepSlider(s_state.stepIdx, stepItems, opts);
        } else if (s_state.modeIdx == 2) {
            guikit::RangeSliderOptions opts{};
            opts.orientation = s_state.orientation;
            opts.textLeft = s_state.textLeft;
            opts.textRight = s_state.textRight;
            opts.valueFormat = "%.0f %%";
            opts.step = stepOpt;
            opts.minSpan = static_cast<double>(s_state.rangeMinSpan);
            opts.showValue = s_state.showValue;
            opts.valuePlacement = s_state.valuePlacement;
            opts.disabled = ctx.elemDisabled;
            opts.size = ctx.controlsSize;
            opts.width = wSize;
            opts.height = hSize;
            opts.color = ctx.GetControlColor();
            ui.RangeSlider(s_state.rangeLow, s_state.rangeHigh, 0.0f, 100.0f, opts);
        } else {
            guikit::ProgressBarOptions opts{};
            opts.orientation = s_state.orientation;
            opts.textLeft = s_state.textLeft;
            opts.textRight = s_state.textRight;
            opts.valueFormat = "%.1f %%";
            opts.peakHoldValue = s_state.progressShowPeak
                ? keel::Optional<double>(s_state.progressPeakVal)
                : keel::kNullOpt;
            opts.segments = s_state.progressSegmented ? s_state.progressSegments : 0u;
            opts.trackHeight = s_state.progressBarHeight;
            opts.showValue = s_state.showValue;
            opts.valuePlacement = s_state.valuePlacement;
            opts.multiZoneColor = s_state.progressMultiZone;
            opts.disabled = ctx.elemDisabled;
            opts.size = ctx.controlsSize;
            opts.width = wSize;
            opts.height = hSize;
            opts.color = ctx.GetControlColor();
            ui.ProgressBar(s_state.val, 0.0, 100.0, opts);
        }
    });
}

keel::String GenerateSliderCode(const DemoContext& ctx) {
    const guikit::AxisSize ctrlH =
        (s_state.orientation == guikit::Orientation::Vertical && s_state.compactVertical && ctx.ResolveCtrlHeight().IsAuto())
            ? guikit::AxisSize(guikit::SizeMode::Fit)
            : ctx.ResolveCtrlHeight();
    const keel::FixedString<48> heightCode = DemoContext::FormatAxisSizeCode(ctrlH);
    const bool hasCustomSize = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md);
    const keel::FixedString<64> sizeLine = hasCustomSize
        ? keel::format_fixed<64>("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};

    if (s_state.modeIdx == 0) {
        const keel::String ctrl = keel::format(
            "ui.Slider(gainValue, 0.0f, 100.0f, {{\n"
            "    .orientation = guikit::Orientation::{},\n"
            "    .textLeft = \"{}\",\n"
            "    .textRight = \"{}\",\n"
            "    .valueFormat = \"%.1f %%\",\n"
            "    .step = {},\n"
            "    .showValue = {},\n"
            "    .valuePlacement = guikit::Placement::{},\n"
            "    .disabled = {},\n"
            "{}"
            "    .width = {},\n"
            "    .height = {},\n"
            "    .color = {}\n"
            "}});",
            OrientationEnumName(s_state.orientation),
            s_state.textLeft,
            s_state.textRight,
            s_state.quantizeStep ? "5.0" : "keel::kNullOpt",
            s_state.showValue ? "true" : "false",
            PlacementName(s_state.valuePlacement),
            ctx.elemDisabled ? "true" : "false",
            sizeLine,
            ctx.FormatCtrlWidthCode(),
            heightCode,
            ctx.FormatControlColorCode()
        );
        return ctx.WrapInElementCode(ctrl);
    }
    if (s_state.modeIdx == 1) {
        const keel::String ctrl = keel::format(
            "ui.StepSlider(presetIndex, stepItems, {{\n"
            "    .orientation = guikit::Orientation::{},\n"
            "    .textLeft = \"{}\",\n"
            "    .textRight = \"{}\",\n"
            "    .showTicks = {},\n"
            "    .showLabels = {},\n"
            "    .showValue = {},\n"
            "    .valuePlacement = guikit::Placement::{},\n"
            "    .disabled = {},\n"
            "{}"
            "    .width = {},\n"
            "    .height = {},\n"
            "    .color = {}\n"
            "}});",
            OrientationEnumName(s_state.orientation),
            s_state.textLeft,
            s_state.textRight,
            s_state.showTicks ? "true" : "false",
            s_state.showLabels ? "true" : "false",
            s_state.showValue ? "true" : "false",
            PlacementName(s_state.valuePlacement),
            ctx.elemDisabled ? "true" : "false",
            sizeLine,
            ctx.FormatCtrlWidthCode(),
            heightCode,
            ctx.FormatControlColorCode()
        );
        keel::String out = keel::String::from(
            "static constexpr keel::StaticArray<keel::StringRef, 5> stepItems = "
            "{\"Off\", \"Low\", \"Mid\", \"High\", \"Max\"};\n"
        );
        out.append(ctx.WrapInElementCode(ctrl));
        return out;
    }
    if (s_state.modeIdx == 2) {
        const keel::String ctrl = keel::format(
            "ui.RangeSlider(bandLow, bandHigh, 0.0f, 100.0f, {{\n"
            "    .orientation = guikit::Orientation::{},\n"
            "    .textLeft = \"{}\",\n"
            "    .textRight = \"{}\",\n"
            "    .valueFormat = \"%.0f %%\",\n"
            "    .step = {},\n"
            "    .minSpan = {:.1f},\n"
            "    .showValue = {},\n"
            "    .valuePlacement = guikit::Placement::{},\n"
            "    .disabled = {},\n"
            "{}"
            "    .width = {},\n"
            "    .height = {},\n"
            "    .color = {}\n"
            "}});",
            OrientationEnumName(s_state.orientation),
            s_state.textLeft,
            s_state.textRight,
            s_state.quantizeStep ? "5.0" : "keel::kNullOpt",
            s_state.rangeMinSpan,
            s_state.showValue ? "true" : "false",
            PlacementName(s_state.valuePlacement),
            ctx.elemDisabled ? "true" : "false",
            sizeLine,
            ctx.FormatCtrlWidthCode(),
            heightCode,
            ctx.FormatControlColorCode()
        );
        return ctx.WrapInElementCode(ctrl);
    }

    const keel::String ctrl = keel::format(
        "ui.ProgressBar(signalLevel, 0.0, 100.0, {{\n"
        "    .orientation = guikit::Orientation::{},\n"
        "    .textLeft = \"{}\",\n"
        "    .textRight = \"{}\",\n"
        "    .valueFormat = \"%.1f %%\",\n"
        "    .peakHoldValue = {},\n"
        "    .segments = {},\n"
        "    .trackHeight = {:.1f}f,\n"
        "    .showValue = {},\n"
        "    .valuePlacement = guikit::Placement::{},\n"
        "    .multiZoneColor = {},\n"
        "    .disabled = {},\n"
        "{}"
        "    .width = {},\n"
        "    .height = {},\n"
        "    .color = {}\n"
        "}});",
        OrientationEnumName(s_state.orientation),
        s_state.textLeft,
        s_state.textRight,
        s_state.progressShowPeak
            ? keel::format_fixed<32>("{:.1f}", s_state.progressPeakVal)
            : keel::FixedString<32>("keel::kNullOpt"),
        s_state.progressSegmented ? s_state.progressSegments : 0u,
        s_state.progressBarHeight,
        s_state.showValue ? "true" : "false",
        PlacementName(s_state.valuePlacement),
        s_state.progressMultiZone ? "true" : "false",
        ctx.elemDisabled ? "true" : "false",
        sizeLine,
        ctx.FormatCtrlWidthCode(),
        heightCode,
        ctx.FormatControlColorCode()
    );
    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
