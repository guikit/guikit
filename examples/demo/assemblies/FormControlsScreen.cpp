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
    if (ui.Switch(s_state.formMode, {.label = "Stacked Labels Mode"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showTelemetry, {.label = "Show Telemetry Banner"}).changed) ui.InvalidateAll();
}

void RenderFormControlsScreen(guikit::Context& ui, DemoContext& ctx) {
    if (auto card = ui.Card(ctx.MakeCard())) {
        ui.Heading({
            .title = "Interactive Reactive State",
            .subtitle = "Pure immediate mode with zero external widget IDs and instant response.",
            .size = ctx.headingSize,
            .wordWrap = ctx.headingWordWrap
        });

        if (auto actionRow = ui.Wrap({.gap = ctx.rowGap, .runGap = ctx.colGap, .align = guikit::Alignment::Center})) {
            if (ui.Button({.label = "Increment (+1)", .icon = guikit::Icon::Plus(), .color = guikit::StateColor::Success()})) {
                ++s_state.counter;
            }
            if (ui.Button({.label = "Decrement (-1)", .icon = guikit::Icon::Minus(), .color = guikit::StateColor::Warning()})) {
                --s_state.counter;
            }
            if (ui.Button({.label = "Reset", .icon = guikit::Icon::Refresh(), .color = guikit::StateColor::Danger()})) {
                s_state.counter = 0;
            }

            ui.Text(ui.Format("Value: {}", s_state.counter), {.role = guikit::TextRole::HeadlineMedium, .variant = guikit::UiVariant::Info});
        }

        if (auto togglesRow = ui.Wrap({.gap = ctx.rowGap, .runGap = ctx.colGap, .align = guikit::Alignment::Center})) {
            ui.Checkbox(s_state.showTelemetry, {.label = "Show Telemetry", .color = guikit::StateColor::Info()});
            if (ui.Switch(s_state.formMode, {
                .label = "Form Mode",
                .description = "Toggle stacked vs default label layout",
                .layout = guikit::ElementLayout::Stacked
            }).changed) {
                ui.InvalidateAll();
            }
            ui.Checkbox(true, {.label = "Auto-Scale", .disabled = true});
        }

        if (s_state.showTelemetry) {
            ui.Heading({
                .title = {"Telemetry Stream", guikit::StateColor::Success()},
                .subtitle = "1000 samples/sec | Jitter < 0.2ms | Hardware loop nominal",
                .icon = guikit::Icon::Check(),
                .badge = {"ACTIVE", guikit::StateColor::Success()},
                .size = ctx.headingSize,
                .wordWrap = ctx.headingWordWrap
            });
        }

        const guikit::ElementLayout activeLayout = s_state.formMode ? guikit::ElementLayout::Stacked : guikit::ElementLayout::Top;

        if (auto fieldsGrid = ui.Grid(ctx.MakeGrid())) {
            if (auto c1 = fieldsGrid.Cell3()) {
                ui.InputField(s_state.speedDrop, {
                    .label = "Speed Drop dF/dt",
                    .description = "Automatic stop of testing on load drop",
                    .suffix = "%",
                    .valueFormat = "%.0f",
                    .layout = activeLayout
                });
            }
            if (auto c2 = fieldsGrid.Cell3()) {
                ui.InputField(s_state.sampleId, {
                    .label = "Sample Identifier",
                    .description = "Unique test specimen code",
                    .placeholder = "Enter sample code...",
                    .layout = activeLayout
                });
            }
            if (auto c3 = fieldsGrid.Cell3()) {
                ui.Select(s_state.baudIndex, {"9600 bps", "19200 bps", "38400 bps", "115200 bps"}, {
                    .label = "Baud Rate",
                    .description = "Serial communication bus rate",
                    .layout = activeLayout
                });
            }
            if (auto c4 = fieldsGrid.Cell3()) {
                ui.Select(s_state.profileMode, {"Eco Saver", "Standard Performance", "Industrial High-Speed", "Extreme Calibration"}, {
                    .label = "Operation Profile",
                    .description = "Active runtime sensor & compute profile",
                    .layout = activeLayout
                });
            }
        }

        ui.Heading({
            .title = "Precision Continuous & Stepped Sliders",
            .subtitle = "Immediate-mode numeric sliders with wheel scrubbing, ticks, and step presets",
            .icon = guikit::Icon::Settings(),
            .size = ctx.headingSize,
            .wordWrap = ctx.headingWordWrap
        });

        if (auto sliderGrid = ui.Grid(ctx.MakeGrid())) {
            if (auto c1 = sliderGrid.Cell4()) {
                ui.Slider(s_state.masterGain, 0.0f, 100.0f, {
                    .label = "Master Analog Gain",
                    .valueFormat = "%.1f dB"
                });
            }
            if (auto c2 = sliderGrid.Cell4()) {
                ui.Slider(s_state.channelCount, 1, 8, {
                    .label = "ADC Active Channels",
                    .valueFormat = "%d ch",
                    .step = 1.0f
                });
            }
            if (auto c3 = sliderGrid.Cell4()) {
                ui.StepSlider(s_state.filterPreset, FormControlsState::filterLabels, {
                    .label = "DSP Filter Mode",
                    .showTicks = true,
                    .showLabels = true
                });
            }
        }
    }
}

} // namespace guikit::demo
