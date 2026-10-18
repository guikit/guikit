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
    if (ui.Switch(s_state.hwEstop, {.label = "Hardware E-Stop Line", .color = guikit::StateColor::Danger()}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.enableSensors, {.label = "Sensor Drop Limits"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.diagLogging, {.label = "High-Speed Flash Logging"}).changed) ui.InvalidateAll();
}

void RenderDashboardScreen(guikit::Context& ui, DemoContext& ctx) {
    // Industrial Dashboard (responsive 2-column layout)
    if (auto dashboardGrid = ui.Grid(ctx.MakeGrid())) {
        // Left Column
        if (auto colLeft = dashboardGrid.Cell6({.gap = ctx.colGap})) {
            if (auto card1 = ui.Card(ctx.MakeCard())) {
                ui.Heading({.title = "Primary Stop Conditions", .subtitle = "Sensory tripwire thresholds & limits", .size = ctx.headingSize, .wordWrap = ctx.headingWordWrap});
                if (auto grid1 = ui.Grid(ctx.MakeGrid())) {
                    if (auto c = grid1.Cell12()) {
                        ui.Switch(s_state.enableSensors, {
                            .label = "Enable Sensor Drop Limits",
                            .description = "Immediate stop on threshold breach"
                        });
                    }
                    if (auto c = grid1.Cell6()) {
                        ui.InputField(s_state.maxTravel, {.label = "Max Travel", .suffix = "mm", .valueFormat = "%.1f"});
                    }
                    if (auto c = grid1.Cell6()) {
                        ui.InputField(s_state.maxVelocity, {.label = "Max Velocity", .suffix = "mm/s", .valueFormat = "%.1f"});
                    }
                    if (auto c = grid1.Cell12()) {
                        ui.Select(s_state.sensorChannel, {
                            "Channel A (Load Cell 50kN)",
                            "Channel B (Optical Extensometer)",
                            "Channel C (Laser Interferometer)"
                        }, {
                            .label = "Primary Sensor Channel",
                            .description = "Active physical transducer bus",
                            .layout = guikit::ElementLayout::Stacked
                        });
                    }
                }
            }

            if (auto card2 = ui.Card(ctx.MakeCard())) {
                ui.Heading({.title = {"Emergency Interlocks", guikit::StateColor::Danger()}, .subtitle = "Hardware line supervision & failsafes", .size = ctx.headingSize, .wordWrap = ctx.headingWordWrap});
                if (auto grid2 = ui.Grid(ctx.MakeGrid())) {
                    if (auto c = grid2.Cell12()) {
                        ui.Switch(s_state.hwEstop, {
                            .label = "Hardware E-Stop Line",
                            .description = "Continuous 24V loop monitoring",
                            .color = guikit::StateColor::Danger()
                        });
                    }
                    if (auto c = grid2.Cell6()) {
                        ui.Select(s_state.interlockMode, {
                            "Immediate Cutoff",
                            "Controlled Ramp-down",
                            "Hold Position"
                        }, {
                            .label = "Interlock Action"
                        });
                    }
                    if (auto c = grid2.Cell6()) {
                        ui.InputField(s_state.rampDown, {.label = "Ramp Duration", .suffix = "ms", .valueFormat = "%.0f"});
                    }
                }
            }
        }

        // Right Column
        if (auto colRight = dashboardGrid.Cell6({.gap = ctx.colGap})) {
            if (auto card3 = ui.Card(ctx.MakeCard())) {
                ui.Heading({.title = "Secondary Criteria & PID", .subtitle = "Dynamic closed-loop feedback parameters", .size = ctx.headingSize, .wordWrap = ctx.headingWordWrap});
                if (auto grid3 = ui.Grid(ctx.MakeGrid())) {
                    if (auto c = grid3.Cell6()) {
                        ui.InputField(s_state.forceLimit, {.label = "Force Drop Limit", .suffix = "%", .valueFormat = "%.1f"});
                    }
                    if (auto c = grid3.Cell6()) {
                        ui.InputField(s_state.yieldRatio, {.label = "Yield Ratio", .suffix = "%", .valueFormat = "%.2f"});
                    }
                    if (auto c = grid3.Cell4()) {
                        ui.InputField(s_state.pGain, {.label = "P-Gain", .valueFormat = "%.2f"});
                    }
                    if (auto c = grid3.Cell4()) {
                        ui.InputField(s_state.iGain, {.label = "I-Gain", .valueFormat = "%.3f"});
                    }
                    if (auto c = grid3.Cell4()) {
                        ui.InputField(s_state.dGain, {.label = "D-Gain", .valueFormat = "%.3f"});
                    }
                    if (auto c = grid3.Cell12()) {
                        ui.Switch(s_state.diagLogging, {
                            .label = "Log High-Speed Telemetry",
                            .description = "Record 10 kHz buffer to internal flash"
                        });
                    }
                }
            }
        }
    }

    if (auto cardAudit = ui.Card(ctx.MakeCard())) {
        ui.Heading({
            .title = {"Supervisor Hardware Loop Diagnostics", guikit::StateColor::Success()},
            .subtitle = "Live bus performance telemetry and continuous line supervision",
            .icon = guikit::Icon::Check(),
            .size = ctx.headingSize,
            .wordWrap = ctx.headingWordWrap
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
