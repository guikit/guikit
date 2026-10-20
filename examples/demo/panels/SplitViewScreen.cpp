#include "../DemoContext.hpp"
#include "guikit/panels/SplitView.hpp"
#include <array>
#include <format>
#include <string>
#include <string_view>

namespace guikit::demo {

namespace {

struct SplitViewDemoState {
    size_t layoutModeIdx = 0;      // 0 = Nested 3-Pane IDE, 1 = Single 2-Pane Split
    size_t orientationIdx = 0;     // 0 = Horizontal, 1 = Vertical (for Single 2-Pane or Primary split)
    float primaryRatio = 0.28f;    // Left Explorer | Right Workspace
    float secondaryRatio = 0.62f;  // Top Viewport / Bottom Telemetry Log
    float sashSize = 6.0f;
    float minRatio = 0.12f;
    float maxRatio = 0.88f;
    float minPaneSize = 48.0f;
    bool showGrip = true;
    bool collapsible = true;
    bool showOuterBorder = true;
    size_t selectedChannelIdx = 0;
    bool liveStreamActive = true;
};

SplitViewDemoState s_split;

constexpr std::array<std::string_view, 2> kLayoutModes = {
    "3-Pane Nested", "2-Pane Single"
};

constexpr std::array<std::string_view, 2> kOrientations = {
    "Horizontal", "Vertical"
};

constexpr std::array<std::string_view, 5> kExplorerNodes = {
    "CH-01  Radar Array North (X-Band)",
    "CH-02  Sonar Towed Hydrophone",
    "CH-03  Optical Gimbal IR-HD",
    "CH-04  AIS / ADS-B Transponder",
    "CH-05  GNSS RTK Reference"
};

} // namespace

void RenderSplitViewInspector(guikit::Context& ui, DemoContext& ctx) {
    ui.Heading({
        .label = "SplitView Configuration",
        .divider = true
    });

    if (auto el = ui.Element({.label = "Demo Topology"})) {
        if (ui.Segmented(s_split.layoutModeIdx, kLayoutModes).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Primary Orientation"})) {
        if (ui.Segmented(s_split.orientationIdx, kOrientations).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Primary Ratio"})) {
        if (ui.Slider(s_split.primaryRatio, 0.0f, 1.0f, {.valueFormat = "%.2f", .step = 0.01f}).changed) {
            ui.InvalidateAll();
        }
    }

    if (s_split.layoutModeIdx == 0) {
        if (auto el = ui.Element({.label = "Secondary Ratio"})) {
            if (ui.Slider(s_split.secondaryRatio, 0.0f, 1.0f, {.valueFormat = "%.2f", .step = 0.01f}).changed) {
                ui.InvalidateAll();
            }
        }
    }

    ui.Heading({
        .label = "Sash & Constraints",
        .divider = true
    });

    if (auto el = ui.Element({.label = "Sash Thickness"})) {
        if (ui.Slider(s_split.sashSize, 4.0f, 14.0f, {.valueFormat = "%.0f dp", .step = 1.0f}).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Ratio Bounds"})) {
        if (ui.RangeSlider(s_split.minRatio, s_split.maxRatio, 0.05f, 0.95f, {.valueFormat = "%.2f", .step = 0.05f}).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Min Pane Size"})) {
        if (ui.Slider(s_split.minPaneSize, 24.0f, 140.0f, {.valueFormat = "%.0f dp", .step = 4.0f}).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Show Sash Grip Pill"})) {
        if (ui.Switch(s_split.showGrip).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Collapsible Edge Snap"})) {
        if (ui.Switch(s_split.collapsible).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Show Outer Frame"})) {
        if (ui.Switch(s_split.showOuterBorder).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto btnRow = ui.Row({.gap = 8.0f})) {
        if (ui.Button({
            .text = "Reset Ratios",
            .icon = guikit::Icon::Refresh(),
            .color = guikit::StateColor::Secondary(),
            .stretch = guikit::StretchMode::Fill
        }).clicked) {
            s_split.primaryRatio = 0.28f;
            s_split.secondaryRatio = 0.62f;
            ui.InvalidateAll();
        }
    }
    (void)ctx;
}

void RenderSplitViewStage(guikit::Context& ui, DemoContext& ctx) {
    (void)ctx;
    const auto& pal = ui.GetTheme().palette;
    const guikit::Orientation primaryOrient = (s_split.orientationIdx == 0)
        ? guikit::Orientation::Horizontal
        : guikit::Orientation::Vertical;
    const guikit::Orientation secondaryOrient = (primaryOrient == guikit::Orientation::Horizontal)
        ? guikit::Orientation::Vertical
        : guikit::Orientation::Horizontal;

    if (auto stageCol = ui.Column({
        .gap = 10.0f,
        .widthMode = guikit::SizeMode::Stretch,
        .heightMode = guikit::SizeMode::Stretch
    })) {
        // Top status readout bar
        if (auto infoBar = ui.Box({
            .widthMode = guikit::SizeMode::Stretch,
            .heightMode = guikit::SizeMode::Fit,
            .padding = guikit::Padding::Symmetric(12.0f, 8.0f),
            .background = {pal.surface},
            .border = {.width = 1.0f, .radius = 8.0f, .color = {pal.borderSubtle}}
        })) {
            ui.Heading({
                .label = (s_split.layoutModeIdx == 0)
                    ? "Nested 3-Pane SplitView Workspace"
                    : "Single 2-Pane SplitView Container",
                .description = "Drag the splitter sash to resize panes, drag near an edge to snap-collapse, or double-click a sash to reset to defaultRatio.",
                .icon = guikit::Icon::Folder(),
                .badge = {
                    ui.Format("Primary {:.0f}% | Secondary {:.0f}%",
                              s_split.primaryRatio * 100.0f,
                              s_split.secondaryRatio * 100.0f).View(),
                    guikit::StateColor::Primary()
                }
            });
        }

        const guikit::SplitViewOptions primaryOpts{
            .orientation = primaryOrient,
            .defaultRatio = 0.28f,
            .minRatio = s_split.minRatio,
            .maxRatio = s_split.maxRatio,
            .minFirstSize = s_split.minPaneSize,
            .minSecondSize = s_split.minPaneSize,
            .sashSize = s_split.sashSize,
            .showGrip = s_split.showGrip,
            .collapsible = s_split.collapsible,
            .widthMode = guikit::SizeMode::Stretch,
            .heightMode = guikit::SizeMode::Stretch,
            .background = {pal.surface},
            .border = s_split.showOuterBorder
                ? guikit::Border{.width = 1.0f, .radius = 8.0f, .color = {pal.borderSubtle}}
                : guikit::Border::None(),
            .clipChildren = true
        };

        if (auto outerSplit = guikit::SplitView::Open(ui, s_split.primaryRatio, primaryOpts)) {
            // =================================================================
            // FIRST PANE: Explorer / Channel Hierarchy
            // =================================================================
            if (auto pane1 = outerSplit.First({
                .gap = 8.0f,
                .widthMode = guikit::SizeMode::Stretch,
                .heightMode = guikit::SizeMode::Stretch,
                .padding = guikit::Padding::All(12.0f),
                .background = {pal.surface},
                .clipChildren = true
            })) {
                ui.Heading({
                    .label = " Pane 1: Sensor Explorer",
                    .icon = guikit::Icon::Folder(),
                    .badge = {ui.Format("{:.0f}%", outerSplit.Ratio() * 100.0f).View(), guikit::StateColor::Info()},
                    .divider = true
                });

                if (auto listScroll = ui.ScrollArea({
                    .gap = 6.0f
                })) {
                    for (size_t i = 0; i < kExplorerNodes.size(); ++i) {
                        const bool isSelected = (s_split.selectedChannelIdx == i);
                        if (ui.Button({
                            .text = kExplorerNodes[i],
                            .icon = isSelected ? guikit::Icon::Check() : guikit::Icon::Play(),
                            .color = isSelected ? guikit::StateColor::Primary() : guikit::StateColor::Ghost(),
                            .align = guikit::Align::Start,
                            .stretch = guikit::StretchMode::Fill
                        }).clicked) {
                            s_split.selectedChannelIdx = i;
                        }
                    }
                }

                if (auto el = ui.Element({.label = "Live Acquisition"})) {
                    ui.Switch(s_split.liveStreamActive);
                }
            }

            // =================================================================
            // SECOND PANE: Either Nested Vertical SplitView or Single Details Pane
            // =================================================================
            if (auto pane2 = outerSplit.Second({
                .widthMode = guikit::SizeMode::Stretch,
                .heightMode = guikit::SizeMode::Stretch,
                .clipChildren = true
            })) {
                if (s_split.layoutModeIdx == 0) {
                    const guikit::SplitViewOptions nestedOpts{
                        .orientation = secondaryOrient,
                        .defaultRatio = 0.62f,
                        .minRatio = s_split.minRatio,
                        .maxRatio = s_split.maxRatio,
                        .minFirstSize = s_split.minPaneSize,
                        .minSecondSize = s_split.minPaneSize,
                        .sashSize = s_split.sashSize,
                        .showGrip = s_split.showGrip,
                        .collapsible = s_split.collapsible,
                        .widthMode = guikit::SizeMode::Stretch,
                        .heightMode = guikit::SizeMode::Stretch,
                        .clipChildren = true
                    };

                    if (auto innerSplit = guikit::SplitView::Open(ui, s_split.secondaryRatio, nestedOpts)) {
                        // Top / First Nested Pane: Active Telemetry Viewport
                        if (auto topPane = innerSplit.First({
                            .gap = 10.0f,
                            .widthMode = guikit::SizeMode::Stretch,
                            .heightMode = guikit::SizeMode::Stretch,
                            .padding = guikit::Padding::All(14.0f),
                            .background = {pal.background.WithAlphaF(0.65f)},
                            .clipChildren = true
                        })) {
                            ui.Heading({
                                .label = "Pane 2A: Active Signal Viewport",
                                .description = kExplorerNodes[s_split.selectedChannelIdx],
                                .icon = guikit::Icon::Flash(),
                                .badge = {ui.Format("{:.0f}%", innerSplit.Ratio() * 100.0f).View(), guikit::StateColor::Success()},
                                .divider = true
                            });

                            if (auto metricsGrid = ui.Grid({.columns = 12, .minColWidth = 140.0f, .gap = 10.0f})) {
                                if (auto c1 = metricsGrid.Cell(4)) {
                                    if (auto card = ui.Card({.gap = 4.0f, .padding = guikit::Padding::All(10.0f)})) {
                                        ui.Text("Carrier SNR", {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Muted});
                                        ui.Text("34.8 dB", {.role = guikit::TextRole::TitleMedium, .variant = guikit::UiVariant::Success});
                                        ui.ProgressBar(82.0, {.color = guikit::StateColor::Success()});
                                    }
                                }
                                if (auto c2 = metricsGrid.Cell(4)) {
                                    if (auto card = ui.Card({.gap = 4.0f, .padding = guikit::Padding::All(10.0f)})) {
                                        ui.Text("Packet Throughput", {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Muted});
                                        ui.Text("1.42 Gbps", {.role = guikit::TextRole::TitleMedium, .variant = guikit::UiVariant::Info});
                                        ui.ProgressBar(64.0, {.color = guikit::StateColor::Info()});
                                    }
                                }
                                if (auto c3 = metricsGrid.Cell(4)) {
                                    if (auto card = ui.Card({.gap = 4.0f, .padding = guikit::Padding::All(10.0f)})) {
                                        ui.Text("Thermal Load", {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Muted});
                                        ui.Text("51.2 C", {.role = guikit::TextRole::TitleMedium, .variant = guikit::UiVariant::Warning});
                                        ui.ProgressBar(48.0, {.color = guikit::StateColor::Warning()});
                                    }
                                }
                            }
                        }

                        // Bottom / Second Nested Pane: Telemetry Event Console
                        if (auto bottomPane = innerSplit.Second({
                            .gap = 6.0f,
                            .widthMode = guikit::SizeMode::Stretch,
                            .heightMode = guikit::SizeMode::Stretch,
                            .padding = guikit::Padding::All(12.0f),
                            .background = {pal.surface},
                            .clipChildren = true
                        })) {
                            ui.Heading({
                                .label = "Pane 2B: Telemetry Event Console",
                                .icon = guikit::Icon::Info(),
                                .badge = {ui.Format("{:.0f}%", (1.0f - innerSplit.Ratio()) * 100.0f).View(), guikit::StateColor::Secondary()},
                                .divider = true
                            });

                            if (auto logScroll = ui.ScrollArea({
                                .gap = 4.0f
                            })) {
                                ui.Text("[14:32:01.104] INFO   DSP ring buffer locked at 48.0 kHz (0 drops)", {.role = guikit::TextRole::CodeSmall, .variant = guikit::UiVariant::Success});
                                ui.Text("[14:32:01.418] INFO   Selected channel bound to hardware DMA slot #2", {.role = guikit::TextRole::CodeSmall, .variant = guikit::UiVariant::Info});
                                ui.Text("[14:32:02.005] WARN   Side-lobe suppression threshold adjusted to -42 dB", {.role = guikit::TextRole::CodeSmall, .variant = guikit::UiVariant::Warning});
                                ui.Text("[14:32:02.890] TRACE  SplitView sash geometry synchronized across frames", {.role = guikit::TextRole::CodeSmall, .variant = guikit::UiVariant::Muted});
                            }
                        }
                    }
                } else {
                    // Single 2-Pane mode: Second pane content directly
                    if (auto singleRight = ui.Box({
                        .gap = 10.0f,
                        .widthMode = guikit::SizeMode::Stretch,
                        .heightMode = guikit::SizeMode::Stretch,
                        .padding = guikit::Padding::All(16.0f),
                        .background = {pal.background.WithAlphaF(0.65f)},
                        .clipChildren = true
                    })) {
                        ui.Heading({
                            .label = "Pane 2: Inspector & Telemetry Details",
                            .description = kExplorerNodes[s_split.selectedChannelIdx],
                            .icon = guikit::Icon::Settings(),
                            .badge = {ui.Format("{:.0f}%", (1.0f - outerSplit.Ratio()) * 100.0f).View(), guikit::StateColor::Primary()},
                            .divider = true
                        });

                        ui.Text(
                            "In Single 2-Pane mode, SplitView divides its available rect into First() and Second() panes separated by an interactive sash.",
                            {.role = guikit::TextRole::BodyMedium, .variant = guikit::UiVariant::Muted}
                        );
                    }
                }
            }
        }
    }
}

std::string GenerateSplitViewCode(const DemoContext& ctx) {
    (void)ctx;
    const std::string_view orientName = kOrientations[s_split.orientationIdx];
    const std::string_view nestedOrientName = (s_split.orientationIdx == 0) ? "Vertical" : "Horizontal";

    return std::format(
        "// Resizable 2-pane & nested 3-pane SplitView container\n"
        "if (auto split = ui.SplitView(primaryRatio, {{\n"
        "    .orientation = guikit::Orientation::{},\n"
        "    .defaultRatio = 0.28f,\n"
        "    .minRatio = {:.2f}f,\n"
        "    .maxRatio = {:.2f}f,\n"
        "    .minFirstSize = {:.0f}f,\n"
        "    .minSecondSize = {:.0f}f,\n"
        "    .sashSize = {:.0f}f,\n"
        "    .showGrip = {},\n"
        "    .collapsible = {}\n"
        "}})) {{\n"
        "    if (auto leftPane = split.First({{.padding = guikit::Padding::All(12.0f)}})) {{\n"
        "        ui.Heading({{.label = \"Sensor Explorer\", .divider = true}});\n"
        "    }}\n"
        "    if (auto rightPane = split.Second()) {{\n"
        "        if (auto inner = ui.SplitView(secondaryRatio, {{\n"
        "            .orientation = guikit::Orientation::{},\n"
        "            .defaultRatio = 0.62f,\n"
        "            .sashSize = {:.0f}f\n"
        "        }})) {{\n"
        "            if (auto top = inner.First())    {{ /* Viewport */ }}\n"
        "            if (auto bottom = inner.Second()) {{ /* Telemetry Log */ }}\n"
        "        }}\n"
        "    }}\n"
        "}}",
        orientName,
        s_split.minRatio,
        s_split.maxRatio,
        s_split.minPaneSize,
        s_split.minPaneSize,
        s_split.sashSize,
        s_split.showGrip ? "true" : "false",
        s_split.collapsible ? "true" : "false",
        nestedOrientName,
        s_split.sashSize
    );
}

} // namespace guikit::demo
