#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct KnobDemoState {
    float val = 65.0f;
    float bipolarVal = 18.0f;
    bool bipolar = false;
    bool quantizeStep = false;
    bool showArc = true;
    bool showTicks = true;
    uint32_t tickCount = 11;
    bool showValue = true;
    guikit::Placement valuePlacement = guikit::Placement::Bottom;
    bool rackDemo = false;

    // 4-Knob Rack Strip state
    float rackGainDb = 3.5f;
    float rackFreqHz = 1250.0f;
    float rackResQ = 2.4f;
    float rackPan = -15.0f;
};
KnobDemoState s_state;
} // namespace

void RenderKnobInspector(guikit::Context& ui, DemoContext&) {
    if (auto el = ui.Element({
        .label = "4-Knob Channel Strip Preview",
        .description = "Preview 4-knob studio rack strip vs single Knob"
    })) {
        if (ui.Switch(s_state.rackDemo).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({
        .label = "Bipolar Center-Zero Sweep",
        .description = "Fill arc from 12 o'clock center (-50..+50)"
    })) {
        if (ui.Switch(s_state.bipolar).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Show 270° Active Arc"})) {
        if (ui.Switch(s_state.showArc).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Show Radial Ticks"})) {
        if (ui.Switch(s_state.showTicks).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Radial Tick Count", .disabled = !s_state.showTicks})) {
        if (ui.Slider(s_state.tickCount, 5u, 21u, {
            .valueFormat = "%u ticks",
            .step = 2.0,
            .disabled = !s_state.showTicks
        }).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Quantize Step (5.0)"})) {
        if (ui.Switch(s_state.quantizeStep).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "Show Value Readout"})) {
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
}

void RenderKnobStage(guikit::Context& ui, DemoContext& ctx) {
    const std::optional<double> stepOpt = s_state.quantizeStep ? std::optional<double>(5.0) : std::nullopt;

    if (s_state.rackDemo) {
        if (auto card = ui.Card({.gap = 16.0f, .padding = guikit::Padding::All(18.0f), .borderRadius = 10.0f})) {
            ui.Size(guikit::UiSize::Sm).Heading({
                .label = "PARAMETRIC CHANNEL STRIP — VFO / EQ",
                .description = "Drag vertically/horizontally (Shift = fine, Wheel = step, Double-Click = reset)",
                .badge = {"DSP", guikit::StateColor::Primary()},
                .divider = true
            });
            if (auto row = ui.Row({.gap = 24.0f, .align = guikit::Alignment::Center, .justify = guikit::Justify::SpaceBetween})) {
                if (auto el = ui.Size(ctx.elemSize).Element({
                    .label = "Input Gain",
                    .description = "Preamp trim",
                    .layout = guikit::ElementLayout::Top,
                    .align = guikit::Align::Center,
                    .stretch = guikit::StretchMode::Compact,
                    .disabled = ctx.elemDisabled
                })) {
                    ui.Size(ctx.controlsSize).Knob(s_state.rackGainDb, -24.0f, 24.0f, {
                        .valueFormat = "%+.1f dB",
                        .step = 0.5,
                        .defaultValue = 0.0,
                        .tickCount = s_state.tickCount,
                        .showValue = s_state.showValue,
                        .showTicks = s_state.showTicks,
                        .showArc = s_state.showArc,
                        .bipolar = true,
                        .valuePlacement = s_state.valuePlacement,
                        .color = guikit::StateColor::Primary(),
                        .disabled = ctx.elemDisabled
                    });
                }
                if (auto el = ui.Size(ctx.elemSize).Element({
                    .label = "Cutoff Freq",
                    .description = "Low-pass filter",
                    .layout = guikit::ElementLayout::Top,
                    .align = guikit::Align::Center,
                    .stretch = guikit::StretchMode::Compact,
                    .disabled = ctx.elemDisabled
                })) {
                    ui.Size(ctx.controlsSize).Knob(s_state.rackFreqHz, 80.0f, 8000.0f, {
                        .valueFormat = "%.0f Hz",
                        .step = 20.0,
                        .defaultValue = 1000.0,
                        .tickCount = s_state.tickCount,
                        .showValue = s_state.showValue,
                        .showTicks = s_state.showTicks,
                        .showArc = s_state.showArc,
                        .bipolar = false,
                        .valuePlacement = s_state.valuePlacement,
                        .color = guikit::StateColor::Info(),
                        .disabled = ctx.elemDisabled
                    });
                }
                if (auto el = ui.Size(ctx.elemSize).Element({
                    .label = "Resonance (Q)",
                    .description = "Filter peak",
                    .layout = guikit::ElementLayout::Top,
                    .align = guikit::Align::Center,
                    .stretch = guikit::StretchMode::Compact,
                    .disabled = ctx.elemDisabled
                })) {
                    ui.Size(ctx.controlsSize).Knob(s_state.rackResQ, 0.5f, 12.0f, {
                        .valueFormat = "%.1f Q",
                        .step = 0.1,
                        .defaultValue = 1.0,
                        .tickCount = s_state.tickCount,
                        .showValue = s_state.showValue,
                        .showTicks = s_state.showTicks,
                        .showArc = s_state.showArc,
                        .bipolar = false,
                        .valuePlacement = s_state.valuePlacement,
                        .color = guikit::StateColor::Success(),
                        .disabled = ctx.elemDisabled
                    });
                }
                if (auto el = ui.Size(ctx.elemSize).Element({
                    .label = "Azimuth Pan",
                    .description = "L / R balance",
                    .layout = guikit::ElementLayout::Top,
                    .align = guikit::Align::Center,
                    .stretch = guikit::StretchMode::Compact,
                    .disabled = ctx.elemDisabled
                })) {
                    ui.Size(ctx.controlsSize).Knob(s_state.rackPan, -50.0f, 50.0f, {
                        .valueFormat = "%+.0f %%",
                        .step = 1.0,
                        .defaultValue = 0.0,
                        .tickCount = s_state.tickCount,
                        .showValue = s_state.showValue,
                        .showTicks = s_state.showTicks,
                        .showArc = s_state.showArc,
                        .bipolar = true,
                        .valuePlacement = s_state.valuePlacement,
                        .color = guikit::StateColor::Warning(),
                        .disabled = ctx.elemDisabled
                    });
                }
            }
        }
        return;
    }

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
        if (s_state.bipolar) {
            ui.Size(ctx.controlsSize).Knob(s_state.bipolarVal, -50.0f, 50.0f, {
                .valueFormat = "%+.1f %%",
                .step = stepOpt,
                .defaultValue = 0.0,
                .tickCount = s_state.tickCount,
                .showValue = s_state.showValue,
                .showTicks = s_state.showTicks,
                .showArc = s_state.showArc,
                .bipolar = true,
                .valuePlacement = s_state.valuePlacement,
                .color = ctx.GetControlColor(),
                .stretch = ctx.GetElementStretch(),
                .width = ctx.GetElementWidth(),
                .height = ctx.GetElementHeight(),
                .background = ctx.GetControlBackground(NavCategory::ElemKnob),
                .border = ctx.GetCtrlBorder(),
                .disabled = ctx.elemDisabled
            });
        } else {
            ui.Size(ctx.controlsSize).Knob(s_state.val, 0.0f, 100.0f, {
                .valueFormat = "%.1f %%",
                .step = stepOpt,
                .defaultValue = 50.0,
                .tickCount = s_state.tickCount,
                .showValue = s_state.showValue,
                .showTicks = s_state.showTicks,
                .showArc = s_state.showArc,
                .bipolar = false,
                .valuePlacement = s_state.valuePlacement,
                .color = ctx.GetControlColor(),
                .stretch = ctx.GetElementStretch(),
                .width = ctx.GetElementWidth(),
                .height = ctx.GetElementHeight(),
                .background = ctx.GetControlBackground(NavCategory::ElemKnob),
                .border = ctx.GetCtrlBorder(),
                .disabled = ctx.elemDisabled
            });
        }
    }
}

std::string GenerateKnobCode(const DemoContext& ctx) {
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
        "    ui.{}Knob(rotaryGain, {}, {}, {{\n"
        "        .valueFormat = \"{}\",\n"
        "        .step = {},\n"
        "        .defaultValue = {},\n"
        "        .tickCount = {},\n"
        "        .showValue = {},\n"
        "        .showTicks = {},\n"
        "        .showArc = {},\n"
        "        .bipolar = {},\n"
        "        .valuePlacement = guikit::Placement::{},\n"
        "        .color = {},\n"
        "        .stretch = guikit::StretchMode::{},\n"
        "        .width = {},\n"
        "        .height = {},\n"
        "        .background = {},\n"
        "        .border = {},\n"
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
        s_state.bipolar ? "-50.0f" : "0.0f",
        s_state.bipolar ? "50.0f" : "100.0f",
        s_state.bipolar ? "%+.1f %%" : "%.1f %%",
        s_state.quantizeStep ? "5.0" : "std::nullopt",
        s_state.bipolar ? "0.0" : "50.0",
        s_state.tickCount,
        s_state.showValue ? "true" : "false",
        s_state.showTicks ? "true" : "false",
        s_state.showArc ? "true" : "false",
        s_state.bipolar ? "true" : "false",
        PlacementName(s_state.valuePlacement),
        ctx.FormatControlColorCode(),
        StretchModeEnumName(ctx.GetElementStretch()),
        ctx.FormatElementWidthCode(),
        ctx.FormatElementHeightCode(),
        ctx.FormatControlBackgroundCode(NavCategory::ElemKnob),
        ctx.FormatCtrlBorderCode(),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
