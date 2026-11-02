#include "../demo_context.hpp"
#include "guikit/panels/split_view.hpp"
#include <array>
#include <format>
#include <string>
#include <string_view>

namespace guikit::demo {

namespace {

struct SplitViewDemoState {
    size_t layoutModeIdx = 1;      // 0 = 3-Pane Nested, 1 = 2-Pane Single (default to clean 2-Pane)
    size_t orientationIdx = 0;     // 0 = Horizontal, 1 = Vertical
    float primaryRatio = 0.35f;
    float secondaryRatio = 0.55f;
    float sashSize = 6.0f;
    float minRatio = 0.12f;
    float maxRatio = 0.88f;
    float minPaneSize = 48.0f;
    bool showGrip = true;
    bool collapsible = true;
    bool showOuterBorder = true;
};

SplitViewDemoState s_split;

constexpr std::array<std::string_view, 2> kLayoutModes = {
    "3-Pane Nested", "2-Pane Single"
};

constexpr std::array<std::string_view, 2> kOrientations = {
    "Horizontal", "Vertical"
};

void RenderPanePlaceholder(
    guikit::Context& ui,
    std::string_view title,
    float ratioPct,
    guikit::UiVariant badgeVariant
) {
    if (auto col = ui.Column({
        .gap = 6.0f,
        .align = guikit::Alignment::Center,
        .justify = guikit::Justify::Center,
        .widthMode = guikit::SizeMode::Stretch,
        .heightMode = guikit::SizeMode::Stretch
    })) {
        ui.Heading({
            .label = title,
            .badge = {ui.Format("{:.0f}%", ratioPct).View(), guikit::StateColor::FromVariant(badgeVariant)},
            .align = guikit::Align::Center,
            .stretch = guikit::StretchMode::Compact
        });
        ui.Text("Drag sash to resize • Double-click sash to reset", {
            .role = guikit::TextRole::LabelSmall,
            .variant = guikit::UiVariant::Muted
        });
    }
}

} // namespace

void RenderSplitViewInspector(guikit::Context& ui, DemoContext& ctx) {
    if (auto el = ui.Element({.label = "Topology", .layout = guikit::ElementLayout::Stacked})) {
        if (ui.Segmented(s_split.layoutModeIdx, kLayoutModes).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Orientation", .layout = guikit::ElementLayout::Stacked})) {
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

    if (ui.Button({
        .text = "Reset Ratios",
        .icon = guikit::Icon::Refresh(),
        .color = guikit::StateColor::Secondary(),
        .stretch = guikit::StretchMode::Fill
    }).clicked) {
        s_split.primaryRatio = 0.35f;
        s_split.secondaryRatio = 0.55f;
        ui.InvalidateAll();
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

    const guikit::SplitViewOptions primaryOpts{
        .orientation = primaryOrient,
        .defaultRatio = 0.35f,
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
        if (auto pane1 = outerSplit.First({
            .widthMode = guikit::SizeMode::Stretch,
            .heightMode = guikit::SizeMode::Stretch,
            .padding = guikit::Padding::All(16.0f),
            .background = {pal.surface},
            .clipChildren = true
        })) {
            RenderPanePlaceholder(ui, "Pane 1", outerSplit.Ratio() * 100.0f, guikit::UiVariant::Primary);
        }

        if (auto pane2 = outerSplit.Second({
            .widthMode = guikit::SizeMode::Stretch,
            .heightMode = guikit::SizeMode::Stretch,
            .clipChildren = true
        })) {
            if (s_split.layoutModeIdx == 0) {
                const guikit::SplitViewOptions nestedOpts{
                    .orientation = secondaryOrient,
                    .defaultRatio = 0.55f,
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
                    if (auto topPane = innerSplit.First({
                        .widthMode = guikit::SizeMode::Stretch,
                        .heightMode = guikit::SizeMode::Stretch,
                        .padding = guikit::Padding::All(16.0f),
                        .background = {pal.background.WithAlphaF(0.55f)},
                        .clipChildren = true
                    })) {
                        RenderPanePlaceholder(ui, "Pane 2A", innerSplit.Ratio() * 100.0f, guikit::UiVariant::Info);
                    }

                    if (auto bottomPane = innerSplit.Second({
                        .widthMode = guikit::SizeMode::Stretch,
                        .heightMode = guikit::SizeMode::Stretch,
                        .padding = guikit::Padding::All(16.0f),
                        .background = {pal.surface},
                        .clipChildren = true
                    })) {
                        RenderPanePlaceholder(ui, "Pane 2B", (1.0f - innerSplit.Ratio()) * 100.0f, guikit::UiVariant::Success);
                    }
                }
            } else {
                if (auto singleRight = ui.Box({
                    .widthMode = guikit::SizeMode::Stretch,
                    .heightMode = guikit::SizeMode::Stretch,
                    .padding = guikit::Padding::All(16.0f),
                    .background = {pal.background.WithAlphaF(0.55f)},
                    .clipChildren = true
                })) {
                    RenderPanePlaceholder(ui, "Pane 2", (1.0f - outerSplit.Ratio()) * 100.0f, guikit::UiVariant::Info);
                }
            }
        }
    }
}

std::string GenerateSplitViewCode(const DemoContext& ctx) {
    (void)ctx;
    const std::string_view orientName = kOrientations[s_split.orientationIdx];
    if (s_split.layoutModeIdx == 1) {
        return std::format(
            "if (auto split = ui.SplitView(ratio, {{\n"
            "    .orientation = guikit::Orientation::{},\n"
            "    .defaultRatio = 0.35f,\n"
            "    .minRatio = {:.2f}f,\n"
            "    .maxRatio = {:.2f}f,\n"
            "    .minFirstSize = {:.0f}f,\n"
            "    .minSecondSize = {:.0f}f,\n"
            "    .sashSize = {:.0f}f,\n"
            "    .showGrip = {},\n"
            "    .collapsible = {}\n"
            "}})) {{\n"
            "    if (auto first = split.First()) {{\n"
            "        // First pane content\n"
            "    }}\n"
            "    if (auto second = split.Second()) {{\n"
            "        // Second pane content\n"
            "    }}\n"
            "}}",
            orientName,
            s_split.minRatio,
            s_split.maxRatio,
            s_split.minPaneSize,
            s_split.minPaneSize,
            s_split.sashSize,
            s_split.showGrip ? "true" : "false",
            s_split.collapsible ? "true" : "false"
        );
    }

    const std::string_view nestedOrientName = (s_split.orientationIdx == 0) ? "Vertical" : "Horizontal";
    return std::format(
        "if (auto split = ui.SplitView(primaryRatio, {{\n"
        "    .orientation = guikit::Orientation::{},\n"
        "    .defaultRatio = 0.35f,\n"
        "    .sashSize = {:.0f}f,\n"
        "    .showGrip = {},\n"
        "    .collapsible = {}\n"
        "}})) {{\n"
        "    if (auto first = split.First()) {{ /* Pane 1 */ }}\n"
        "    if (auto second = split.Second()) {{\n"
        "        if (auto inner = ui.SplitView(secondaryRatio, {{\n"
        "            .orientation = guikit::Orientation::{},\n"
        "            .defaultRatio = 0.55f,\n"
        "            .sashSize = {:.0f}f\n"
        "        }})) {{\n"
        "            if (auto paneA = inner.First())  {{ /* Pane 2A */ }}\n"
        "            if (auto paneB = inner.Second()) {{ /* Pane 2B */ }}\n"
        "        }}\n"
        "    }}\n"
        "}}",
        orientName,
        s_split.sashSize,
        s_split.showGrip ? "true" : "false",
        s_split.collapsible ? "true" : "false",
        nestedOrientName,
        s_split.sashSize
    );
}

} // namespace guikit::demo
