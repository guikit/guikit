#include "../DemoContext.hpp"

namespace guikit::demo {

namespace {
struct FormControlsState {
    int counter = 0;
    bool showTelemetry = false;
    bool formMode = true;
    float speedDrop = 50.0f;
    std::string sampleId = "Specimen-2026-X";
    size_t baudIndex = 1;
    std::string profileMode = "Standard Performance";
    float masterGain = 72.5f;
    int channelCount = 4;
    size_t filterPreset = 1;
    static constexpr std::array<std::string_view, 4> filterLabels = {"Bypass", "Butterworth", "Chebyshev", "Bessel"};
};
FormControlsState s_state;
} // namespace

void RenderFormControlsInspector(guikit::Context& ui, DemoContext& ctx) {
    ctx.RenderAssemblyCommonInspector(ui);
    if (auto el = ui.Element({.label = "Stacked Labels Mode"})) {
        if (ui.Switch(s_state.formMode).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Telemetry Banner"})) {
        if (ui.Switch(s_state.showTelemetry).changed) ui.InvalidateAll();
    }
}

void RenderFormControlsScreen(guikit::Context& ui, DemoContext& ctx) {
    if (auto card = ui.Card(ctx.MakeCard())) {
        ui.Heading({
            .label = "Interactive Reactive State",
            .description = "Pure immediate mode with zero external widget IDs and instant response.",
            .size = ctx.headingSize
        });

        if (auto actionRow = ui.Wrap({.gap = ctx.rowGap, .runGap = ctx.colGap, .align = guikit::Alignment::Center})) {
            if (ui.Button({.text = "Increment (+1)", .icon = guikit::Icon::Plus(), .color = guikit::StateColor::Success()})) {
                ++s_state.counter;
            }
            if (ui.Button({.text = "Decrement (-1)", .icon = guikit::Icon::Minus(), .color = guikit::StateColor::Warning()})) {
                --s_state.counter;
            }
            if (ui.Button({.text = "Reset", .icon = guikit::Icon::Refresh(), .color = guikit::StateColor::Danger()})) {
                s_state.counter = 0;
            }

            ui.Text(ui.Format("Value: {}", s_state.counter), {.role = guikit::TextRole::HeadlineMedium, .variant = guikit::UiVariant::Info});
        }

        if (auto togglesRow = ui.Wrap({.gap = ctx.rowGap, .runGap = ctx.colGap, .align = guikit::Alignment::Center})) {
            if (auto el = ui.Element({.label = "Show Telemetry"})) {
                ui.Checkbox(s_state.showTelemetry, {.color = guikit::StateColor::Info()});
            }
            if (auto el = ui.Element({
                .label = "Form Mode",
                .description = "Toggle stacked vs default label layout",
                .layout = guikit::ElementLayout::Stacked
            })) {
                if (ui.Switch(s_state.formMode).changed) {
                    ui.InvalidateAll();
                }
            }
            if (auto el = ui.Element({.label = "Auto-Scale", .disabled = true})) {
                ui.Checkbox(true, {.disabled = true});
            }
        }

        if (s_state.showTelemetry) {
            ui.Heading({
                .label = {"Telemetry Stream", guikit::StateColor::Success()},
                .description = "1000 samples/sec | Jitter < 0.2ms | Hardware loop nominal",
                .icon = guikit::Icon::Check(),
                .size = ctx.headingSize
            });
        }

        const guikit::ElementLayout activeLayout = s_state.formMode ? guikit::ElementLayout::Stacked : guikit::ElementLayout::Top;

        if (auto fieldsGrid = ui.Grid(ctx.MakeGrid())) {
            if (auto c1 = fieldsGrid.Cell3()) {
                if (auto el = ui.Element({
                    .label = "Speed Drop dF/dt",
                    .description = "Automatic stop of testing on load drop",
                    .layout = activeLayout
                })) {
                    ui.InputField(s_state.speedDrop, {
                        .suffix = "%",
                        .valueFormat = "%.0f"
                    });
                }
            }
            if (auto c2 = fieldsGrid.Cell3()) {
                if (auto el = ui.Element({
                    .label = "Sample Identifier",
                    .description = "Unique test specimen code",
                    .layout = activeLayout
                })) {
                    ui.InputField(s_state.sampleId, {
                        .placeholder = "Enter sample code..."
                    });
                }
            }
            if (auto c3 = fieldsGrid.Cell3()) {
                if (auto el = ui.Element({
                    .label = "Baud Rate",
                    .description = "Serial communication bus rate",
                    .layout = activeLayout
                })) {
                    ui.Select(s_state.baudIndex, {"9600 bps", "19200 bps", "38400 bps", "115200 bps"});
                }
            }
            if (auto c4 = fieldsGrid.Cell3()) {
                if (auto el = ui.Element({
                    .label = "Operation Profile",
                    .description = "Active runtime sensor & compute profile",
                    .layout = activeLayout
                })) {
                    ui.Select(s_state.profileMode, {"Eco Saver", "Standard Performance", "Industrial High-Speed", "Extreme Calibration"});
                }
            }
        }

        ui.Heading({
            .label = "Precision Continuous & Stepped Sliders",
            .description = "Immediate-mode numeric sliders with wheel scrubbing, ticks, and step presets",
            .icon = guikit::Icon::Settings(),
            .size = ctx.headingSize
        });

        if (auto sliderGrid = ui.Grid(ctx.MakeGrid())) {
            if (auto c1 = sliderGrid.Cell4()) {
                if (auto el = ui.Element({.label = "Master Analog Gain"})) {
                    ui.Slider(s_state.masterGain, 0.0f, 100.0f, {
                        .valueFormat = "%.1f dB"
                    });
                }
            }
            if (auto c2 = sliderGrid.Cell4()) {
                if (auto el = ui.Element({.label = "ADC Active Channels"})) {
                    ui.Slider(s_state.channelCount, 1, 8, {
                        .valueFormat = "%d ch",
                        .step = 1.0f
                    });
                }
            }
            if (auto c3 = sliderGrid.Cell4()) {
                if (auto el = ui.Element({.label = "DSP Filter Mode"})) {
                    ui.StepSlider(s_state.filterPreset, FormControlsState::filterLabels, {
                        .showTicks = true,
                        .showLabels = true
                    });
                }
            }
        }
    }
}

} // namespace guikit::demo
