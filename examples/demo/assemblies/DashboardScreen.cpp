#include "../DemoContext.hpp"

namespace guikit::demo {

namespace {
struct DashboardState {
    bool enableSensors = true;
    float maxTravel = 150.0f;
    float maxVelocity = 25.0f;
    size_t sensorChannel = 0;
    bool hwEstop = true;
    size_t interlockMode = 1;
    float rampDown = 250.0f;
    float forceLimit = 85.0f;
    float yieldRatio = 0.2f;
    float pGain = 1.25f;
    float iGain = 0.05f;
    float dGain = 0.01f;
    bool diagLogging = true;
};
DashboardState s_state;
} // namespace

void RenderDashboardInspector(guikit::Context& ui, DemoContext& ctx) {
    ctx.RenderAssemblyCommonInspector(ui);
    if (auto el = ui.Element({.label = "Hardware E-Stop Line"})) {
        if (ui.Switch(s_state.hwEstop, {.color = guikit::StateColor::Danger()}).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Sensor Drop Limits"})) {
        if (ui.Switch(s_state.enableSensors).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "High-Speed Flash Logging"})) {
        if (ui.Switch(s_state.diagLogging).changed) ui.InvalidateAll();
    }
}

void RenderDashboardScreen(guikit::Context& ui, DemoContext& ctx) {
    // Industrial Dashboard (responsive 2-column layout)
    if (auto dashboardGrid = ui.Grid(ctx.MakeGrid())) {
        // Left Column
        if (auto colLeft = dashboardGrid.Cell6({.gap = ctx.colGap})) {
            if (auto card1 = ui.Card(ctx.MakeCard())) {
                ui.Size(ctx.headingSize).Heading({.label = "Primary Stop Conditions", .description = "Sensory tripwire thresholds & limits"});
                if (auto grid1 = ui.Grid(ctx.MakeGrid())) {
                    if (auto c = grid1.Cell12()) {
                        if (auto el = ui.Element({
                            .label = "Enable Sensor Drop Limits",
                            .description = "Immediate stop on threshold breach"
                        })) {
                            ui.Switch(s_state.enableSensors);
                        }
                    }
                    if (auto c = grid1.Cell6()) {
                        if (auto el = ui.Element({.label = "Max Travel"})) {
                            ui.SpinBox(s_state.maxTravel, {.suffix = "mm", .valueFormat = "%.1f", .min = 0.0, .max = 500.0, .step = 1.0});
                        }
                    }
                    if (auto c = grid1.Cell6()) {
                        if (auto el = ui.Element({.label = "Max Velocity"})) {
                            ui.SpinBox(s_state.maxVelocity, {.suffix = "mm/s", .valueFormat = "%.1f", .min = 0.0, .max = 200.0, .step = 0.5});
                        }
                    }
                    if (auto c = grid1.Cell12()) {
                        if (auto el = ui.Element({
                            .label = "Primary Sensor Channel",
                            .description = "Active physical transducer bus",
                            .layout = guikit::ElementLayout::Stacked
                        })) {
                            ui.Select(s_state.sensorChannel, {
                                "Channel A (Load Cell 50kN)",
                                "Channel B (Optical Extensometer)",
                                "Channel C (Laser Interferometer)"
                            });
                        }
                    }
                }
            }

            if (auto card2 = ui.Card(ctx.MakeCard())) {
                ui.Size(ctx.headingSize).Heading({.label = {"Emergency Interlocks", guikit::StateColor::Danger()}, .description = "Hardware line supervision & failsafes"});
                if (auto grid2 = ui.Grid(ctx.MakeGrid())) {
                    if (auto c = grid2.Cell12()) {
                        if (auto el = ui.Element({
                            .label = "Hardware E-Stop Line",
                            .description = "Continuous 24V loop monitoring"
                        })) {
                            ui.Switch(s_state.hwEstop, {
                                .color = guikit::StateColor::Danger()
                            });
                        }
                    }
                    if (auto c = grid2.Cell6()) {
                        if (auto el = ui.Element({.label = "Interlock Action"})) {
                            ui.Select(s_state.interlockMode, {
                                "Immediate Cutoff",
                                "Controlled Ramp-down",
                                "Hold Position"
                            });
                        }
                    }
                    if (auto c = grid2.Cell6()) {
                        if (auto el = ui.Element({.label = "Ramp Duration"})) {
                            ui.SpinBox(s_state.rampDown, {.suffix = "ms", .valueFormat = "%.0f", .min = 0.0, .max = 5000.0, .step = 10.0});
                        }
                    }
                }
            }
        }

        // Right Column
        if (auto colRight = dashboardGrid.Cell6({.gap = ctx.colGap})) {
            if (auto card3 = ui.Card(ctx.MakeCard())) {
                ui.Size(ctx.headingSize).Heading({.label = "Secondary Criteria & PID", .description = "Dynamic closed-loop feedback parameters"});
                if (auto grid3 = ui.Grid(ctx.MakeGrid())) {
                    if (auto c = grid3.Cell6()) {
                        if (auto el = ui.Element({.label = "Force Drop Limit"})) {
                            ui.SpinBox(s_state.forceLimit, {.suffix = "%", .valueFormat = "%.1f", .min = 0.0, .max = 100.0, .step = 1.0});
                        }
                    }
                    if (auto c = grid3.Cell6()) {
                        if (auto el = ui.Element({.label = "Yield Ratio"})) {
                            ui.SpinBox(s_state.yieldRatio, {.suffix = "%", .valueFormat = "%.2f", .min = 0.0, .max = 10.0, .step = 0.05});
                        }
                    }
                    if (auto c = grid3.Cell4()) {
                        if (auto el = ui.Element({.label = "P-Gain"})) {
                            ui.SpinBox(s_state.pGain, {.valueFormat = "%.2f", .min = 0.0, .max = 20.0, .step = 0.05});
                        }
                    }
                    if (auto c = grid3.Cell4()) {
                        if (auto el = ui.Element({.label = "I-Gain"})) {
                            ui.SpinBox(s_state.iGain, {.valueFormat = "%.3f", .min = 0.0, .max = 5.0, .step = 0.005});
                        }
                    }
                    if (auto c = grid3.Cell4()) {
                        if (auto el = ui.Element({.label = "D-Gain"})) {
                            ui.SpinBox(s_state.dGain, {.valueFormat = "%.3f", .min = 0.0, .max = 5.0, .step = 0.001});
                        }
                    }
                    if (auto c = grid3.Cell12()) {
                        if (auto el = ui.Element({
                            .label = "Log High-Speed Telemetry",
                            .description = "Record 10 kHz buffer to internal flash"
                        })) {
                            ui.Switch(s_state.diagLogging);
                        }
                    }
                }
            }
        }
    }

    if (auto cardAudit = ui.Card(ctx.MakeCard())) {
        ui.Size(ctx.headingSize).Heading({
            .label = {"Supervisor Hardware Loop Diagnostics", guikit::StateColor::Success()},
            .description = "Live bus performance telemetry and continuous line supervision",
            .icon = guikit::Icon::Check()
        });
        if (auto g = ui.Grid(ctx.MakeGrid())) {
            if (auto c1 = g.Cell4()) {
                ui.Text("Safety Loop: CLOSED (Nominal)", {.role = guikit::TextRole::BodyMedium, .variant = guikit::UiVariant::Success});
            }
            if (auto c2 = g.Cell4()) {
                ui.Text("Bus Jitter: < 0.12 ms", {.role = guikit::TextRole::CodeMedium, .variant = guikit::UiVariant::Info});
            }
            if (auto c3 = g.Cell4()) {
                ui.Text("Supervisor Heartbeat: 1000 Hz", {.role = guikit::TextRole::CodeMedium});
            }
        }
    }
}

} // namespace guikit::demo
