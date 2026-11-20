#include "../demo_context.hpp"

namespace guikit::demo {

namespace {
struct KnobDemoState {
    float val = 65.0f;
    float bipolarVal = 18.0f;
    bool bipolar = false;
    bool quantizeStep = false;
    bool showArc = true;
    bool showTicks = true;
    keel::u32 tickCount = 11;
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
}

void RenderKnobStage(guikit::Context& ui, DemoContext& ctx) {
    const keel::Optional<double> stepOpt = s_state.quantizeStep ? keel::Optional<double>(5.0) : keel::kNullOpt;

    if (s_state.rackDemo) {
        if (auto card = ui.Card({
            .gap = 16.0f,
            .padding = guikit::Padding::All(18.0f),
            .border = {.width = 1.0f, .radius = 10.0f}
        })) {
            ui.Heading({
                .label = "PARAMETRIC CHANNEL STRIP — VFO / EQ",
                .description = "Drag vertically/horizontally (Shift = fine, Wheel = step, Double-Click = reset)",
                .badge = {"DSP", guikit::StateColor::Primary()},
                .level = guikit::HeadingLevel::H5,
                .separator = guikit::HeadingSeparator::Bottom
            });
            if (auto row = ui.Row({
                .gap = 24.0f,
                .align = guikit::Alignment::Center,
                .justify = guikit::Justify::SpaceBetween
            })) {
                if (auto el = ui.Element({
                    .label = "Input Gain",
                    .description = "Preamp trim",
                    .layout = guikit::ElementLayout::Top,
                    .align = guikit::Align::Center,
                    .width = guikit::SizeMode::Fit,
                    .size = ctx.elemSize,
                    .disabled = ctx.elemDisabled
                })) {
                    ui.Knob(s_state.rackGainDb, -24.0f, 24.0f, {
                        .valueFormat = "%+.1f dB",
                        .step = 0.5,
                        .defaultValue = 0.0,
                        .tickCount = s_state.tickCount,
                        .showValue = s_state.showValue,
                        .showTicks = s_state.showTicks,
                        .showArc = s_state.showArc,
                        .bipolar = true,
                        .valuePlacement = s_state.valuePlacement,
                        .diameter = ctx.ResolveKnobDiameter(),
                        .disabled = ctx.elemDisabled,
                        .size = ctx.controlsSize,
                        .color = guikit::StateColor::Primary()
                    });
                }
                if (auto el = ui.Element({
                    .label = "Cutoff Freq",
                    .description = "Low-pass filter",
                    .layout = guikit::ElementLayout::Top,
                    .align = guikit::Align::Center,
                    .width = guikit::SizeMode::Fit,
                    .size = ctx.elemSize,
                    .disabled = ctx.elemDisabled
                })) {
                    ui.Knob(s_state.rackFreqHz, 80.0f, 8000.0f, {
                        .valueFormat = "%.0f Hz",
                        .step = 20.0,
                        .defaultValue = 1000.0,
                        .tickCount = s_state.tickCount,
                        .showValue = s_state.showValue,
                        .showTicks = s_state.showTicks,
                        .showArc = s_state.showArc,
                        .bipolar = false,
                        .valuePlacement = s_state.valuePlacement,
                        .diameter = ctx.ResolveKnobDiameter(),
                        .disabled = ctx.elemDisabled,
                        .size = ctx.controlsSize,
                        .color = guikit::StateColor::Info()
                    });
                }
                if (auto el = ui.Element({
                    .label = "Resonance (Q)",
                    .description = "Filter peak",
                    .layout = guikit::ElementLayout::Top,
                    .align = guikit::Align::Center,
                    .width = guikit::SizeMode::Fit,
                    .size = ctx.elemSize,
                    .disabled = ctx.elemDisabled
                })) {
                    ui.Knob(s_state.rackResQ, 0.5f, 12.0f, {
                        .valueFormat = "%.1f Q",
                        .step = 0.1,
                        .defaultValue = 1.0,
                        .tickCount = s_state.tickCount,
                        .showValue = s_state.showValue,
                        .showTicks = s_state.showTicks,
                        .showArc = s_state.showArc,
                        .bipolar = false,
                        .valuePlacement = s_state.valuePlacement,
                        .diameter = ctx.ResolveKnobDiameter(),
                        .disabled = ctx.elemDisabled,
                        .size = ctx.controlsSize,
                        .color = guikit::StateColor::Success()
                    });
                }
                if (auto el = ui.Element({
                    .label = "Azimuth Pan",
                    .description = "L / R balance",
                    .layout = guikit::ElementLayout::Top,
                    .align = guikit::Align::Center,
                    .width = guikit::SizeMode::Fit,
                    .size = ctx.elemSize,
                    .disabled = ctx.elemDisabled
                })) {
                    ui.Knob(s_state.rackPan, -50.0f, 50.0f, {
                        .valueFormat = "%+.0f %%",
                        .step = 1.0,
                        .defaultValue = 0.0,
                        .tickCount = s_state.tickCount,
                        .showValue = s_state.showValue,
                        .showTicks = s_state.showTicks,
                        .showArc = s_state.showArc,
                        .bipolar = true,
                        .valuePlacement = s_state.valuePlacement,
                        .diameter = ctx.ResolveKnobDiameter(),
                        .disabled = ctx.elemDisabled,
                        .size = ctx.controlsSize,
                        .color = guikit::StateColor::Warning()
                    });
                }
            }
        }
        return;
    }

    ctx.RunElementStage(ui, [&]() {
        guikit::KnobOptions opts{};
        opts.step = stepOpt;
        opts.tickCount = s_state.tickCount;
        opts.showValue = s_state.showValue;
        opts.showTicks = s_state.showTicks;
        opts.showArc = s_state.showArc;
        opts.bipolar = s_state.bipolar;
        opts.valuePlacement = s_state.valuePlacement;
        opts.diameter = ctx.ResolveKnobDiameter();
        opts.disabled = ctx.elemDisabled;
        opts.size = ctx.controlsSize;
        opts.color = ctx.GetControlColor();

        if (s_state.bipolar) {
            opts.valueFormat = "%+.1f %%";
            opts.defaultValue = 0.0;
            ui.Knob(s_state.bipolarVal, -50.0f, 50.0f, opts);
        } else {
            opts.valueFormat = "%.1f %%";
            opts.defaultValue = 50.0;
            ui.Knob(s_state.val, 0.0f, 100.0f, opts);
        }
    });
}

keel::String GenerateKnobCode(const DemoContext& ctx) {
    const bool hasCustomSize = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md);
    const keel::FixedString<64> sizeLine = hasCustomSize
        ? keel::format_fixed<64>("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};
    const keel::String ctrl = keel::format(
        "ui.Knob(rotaryGain, {}, {}, {{\n"
        "    .valueFormat = \"{}\",\n"
        "    .step = {},\n"
        "    .defaultValue = {},\n"
        "    .tickCount = {},\n"
        "    .showValue = {},\n"
        "    .showTicks = {},\n"
        "    .showArc = {},\n"
        "    .bipolar = {},\n"
        "    .valuePlacement = guikit::Placement::{},\n"
        "    .diameter = {},\n"
        "    .disabled = {},\n"
        "{}"
        "    .color = {}\n"
        "}});",
        s_state.bipolar ? "-50.0f" : "0.0f",
        s_state.bipolar ? "50.0f" : "100.0f",
        s_state.bipolar ? "%+.1f %%" : "%.1f %%",
        s_state.quantizeStep ? "5.0" : "keel::kNullOpt",
        s_state.bipolar ? "0.0" : "50.0",
        s_state.tickCount,
        s_state.showValue ? "true" : "false",
        s_state.showTicks ? "true" : "false",
        s_state.showArc ? "true" : "false",
        s_state.bipolar ? "true" : "false",
        PlacementName(s_state.valuePlacement),
        ctx.FormatKnobDiameterCode(),
        ctx.elemDisabled ? "true" : "false",
        sizeLine,
        ctx.FormatControlColorCode()
    );
    return ctx.WrapInElementCode(ctrl);
}

} // namespace guikit::demo
