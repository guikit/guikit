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
                ui.Heading({.label = "Primary Stop Conditions", .description = "Sensory tripwire thresholds & limits", .size = ctx.headingSize});
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
                            ui.InputField(s_state.maxTravel, {.suffix = "mm", .valueFormat = "%.1f"});
                        }
                    }
                    if (auto c = grid1.Cell6()) {
                        if (auto el = ui.Element({.label = "Max Velocity"})) {
                            ui.InputField(s_state.maxVelocity, {.suffix = "mm/s", .valueFormat = "%.1f"});
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
                ui.Heading({.label = {"Emergency Interlocks", guikit::StateColor::Danger()}, .description = "Hardware line supervision & failsafes", .size = ctx.headingSize});
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
                            ui.InputField(s_state.rampDown, {.suffix = "ms", .valueFormat = "%.0f"});
                        }
                    }
                }
            }
        }

        // Right Column
        if (auto colRight = dashboardGrid.Cell6({.gap = ctx.colGap})) {
            if (auto card3 = ui.Card(ctx.MakeCard())) {
                ui.Heading({.label = "Secondary Criteria & PID", .description = "Dynamic closed-loop feedback parameters", .size = ctx.headingSize});
                if (auto grid3 = ui.Grid(ctx.MakeGrid())) {
                    if (auto c = grid3.Cell6()) {
                        if (auto el = ui.Element({.label = "Force Drop Limit"})) {
                            ui.InputField(s_state.forceLimit, {.suffix = "%", .valueFormat = "%.1f"});
                        }
                    }
                    if (auto c = grid3.Cell6()) {
                        if (auto el = ui.Element({.label = "Yield Ratio"})) {
                            ui.InputField(s_state.yieldRatio, {.suffix = "%", .valueFormat = "%.2f"});
                        }
                    }
                    if (auto c = grid3.Cell4()) {
                        if (auto el = ui.Element({.label = "P-Gain"})) {
                            ui.InputField(s_state.pGain, {.valueFormat = "%.2f"});
                        }
                    }
                    if (auto c = grid3.Cell4()) {
                        if (auto el = ui.Element({.label = "I-Gain"})) {
                            ui.InputField(s_state.iGain, {.valueFormat = "%.3f"});
                        }
                    }
                    if (auto c = grid3.Cell4()) {
                        if (auto el = ui.Element({.label = "D-Gain"})) {
                            ui.InputField(s_state.dGain, {.valueFormat = "%.3f"});
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
        ui.Heading({
            .label = {"Supervisor Hardware Loop Diagnostics", guikit::StateColor::Success()},
            .description = "Live bus performance telemetry and continuous line supervision",
            .icon = guikit::Icon::Check(),
            .size = ctx.headingSize
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
