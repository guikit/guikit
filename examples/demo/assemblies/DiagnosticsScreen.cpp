#include "../DemoContext.hpp"

namespace guikit::demo {

void RenderDiagnosticsInspector(guikit::Context& ui, DemoContext& ctx) {
    ctx.RenderAssemblyCommonInspector(ui);
}

void RenderDiagnosticsScreen(guikit::Context& ui, DemoContext& ctx) {
    if (auto card = ui.Card(ctx.MakeCard())) {
        ui.Size(ctx.headingSize).Heading({
            .label = "System Diagnostics & Network Audit",
            .description = "Operational module telemetry stream and live system diagnostics.",
            .icon = guikit::Icon::Check()
        });
        if (auto grid = ui.Grid(ctx.MakeGrid())) {
            if (auto c1 = grid.Cell6()) {
                ui.Text("Channel Status: ONLINE | Active bus node: [SUPERVISOR]",
                        {.role = guikit::TextRole::BodyMedium, .variant = guikit::UiVariant::Success});
            }
            if (auto c2 = grid.Cell6()) {
                ui.Text("Latency: 0.18 ms | Packet Drop: 0.00% | CRC: OK",
                        {.role = guikit::TextRole::CodeMedium, .variant = guikit::UiVariant::Info});
            }
            if (auto c3 = grid.Cell6()) {
                ui.Text("Memory Arena High Watermark: 24.8 KB",
                        {.role = guikit::TextRole::CodeMedium, .variant = guikit::UiVariant::Secondary});
            }
            if (auto c4 = grid.Cell6()) {
                ui.Text("Frame Damage Rate: 0 full repaints / 99.8% partial",
                        {.role = guikit::TextRole::CodeMedium, .variant = guikit::UiVariant::Success});
            }
        }
    }
}

} // namespace guikit::demo
