#include "guikit/GuiKit.hpp"

int main(int /*argc*/, char* /*argv*/[]) {
    bool autopilot = true;
    float thrustPct = 72.5f;
    int pulseCount = 0;

    guikit::MainWindow app({
        .title = "GuiKit Minimal Quickstart",
        .width = 720,
        .height = 480
    });

    return app.Run([&](guikit::Context& ui) {
        if (auto root = ui.Column({
            .padding = guikit::Padding::All(24.0f),
            .align = guikit::Alignment::Center,
            .justify = guikit::Justify::Center,
            .widthMode = guikit::SizeMode::Stretch,
            .heightMode = guikit::SizeMode::Stretch
        })) {
            if (auto card = ui.Card({
                .minWidth = 420.0f,
                .gap = 12.0f,
                .padding = guikit::Padding::All(20.0f),
                .widthMode = guikit::SizeMode::Fit
            })) {
                ui.Heading({
                    .label = "Mission Control",
                    .description = "GuiKit C++20 Immediate-Mode Vector UI",
                    .icon = guikit::Icon::Play(),
                    .divider = true
                });

                if (auto el = ui.Element({
                    .label = "Autopilot Loop",
                    .description = "Closed-loop attitude stabilization"
                })) {
                    ui.Switch(autopilot);
                }

                if (auto el = ui.Element({.label = "Main Thruster Output"})) {
                    ui.Slider(thrustPct, 0.0f, 100.0f, {.valueFormat = "%.1f %%"});
                }

                if (auto row = ui.Row({.gap = 8.0f, .align = guikit::Alignment::Center})) {
                    if (ui.Button({
                        .text = "Trigger Telemetry Pulse",
                        .icon = guikit::Icon::Flash(),
                        .color = guikit::StateColor::Primary()
                    })) {
                        ++pulseCount;
                    }
                    ui.Spacer();
                    ui.Text(ui.Format("Pulses: {}", pulseCount), {
                        .role = guikit::TextRole::CodeMedium,
                        .variant = guikit::UiVariant::Info
                    });
                }
            }
        }
    });
}
