#include "../DemoContext.hpp"
#include <algorithm>
#include <format>

namespace guikit::demo {

namespace {
struct SidebarState {
    int activityIdx = 0;
    int menuIdx = 0;
    bool showActivityBar = true;
    guikit::ActivityBarStyle activityStyle = guikit::ActivityBarStyle::Indicator;
    guikit::SidebarMode mode = guikit::SidebarMode::Expanded;
    guikit::SidebarMenuStyle menuStyle = guikit::SidebarMenuStyle::Pill;
    guikit::SidebarSide side = guikit::SidebarSide::Left;
    bool resizable = true;
    bool allowCompact = true;
    bool allowCollapse = true;
    bool showBadges = true;
    bool showSections = true;
    bool autoWidth = false;
    float fixedWidth = 210.0f;

    // BottomPanel & StatusBar Workbench controls
    bool showBottomPanel = true;
    int bottomTabIdx = 0;
    bool showStatusBar = true;
    guikit::StatusBarStyle statusBarStyle = guikit::StatusBarStyle::Surface;
};
SidebarState s_state;

constexpr std::string_view ModeEnumName(guikit::SidebarMode m) noexcept {
    switch (m) {
        case guikit::SidebarMode::Compact:   return "Compact";
        case guikit::SidebarMode::Collapsed: return "Collapsed";
        default:                             return "Expanded";
    }
}

constexpr std::string_view StatusStyleName(guikit::StatusBarStyle s) noexcept {
    switch (s) {
        case guikit::StatusBarStyle::Header: return "Header";
        case guikit::StatusBarStyle::Accent: return "Accent";
        default:                             return "Surface";
    }
}
} // namespace

void RenderSidebarInspector(guikit::Context& ui, DemoContext& ctx) {
    if (auto el = ui.Element({
        .label = "ActivityBar (VS Code Rail)",
        .description = "Primary tool switcher docked flush to Sidebar"
    })) {
        if (ui.Switch(s_state.showActivityBar).changed) ui.InvalidateAll();
    }

    if (s_state.showActivityBar) {
        static constexpr std::array<std::string_view, 2> abStyleNames = {"Indicator", "Pill"};
        size_t abStyleIdx = (s_state.activityStyle == guikit::ActivityBarStyle::Pill) ? 1 : 0;
        if (auto el = ui.Element({.label = "ActivityBar Style"})) {
            if (ui.StepSlider(abStyleIdx, abStyleNames, {.showTicks = true, .showLabels = true}).changed) {
                s_state.activityStyle = (abStyleIdx == 1) ? guikit::ActivityBarStyle::Pill : guikit::ActivityBarStyle::Indicator;
                ui.InvalidateAll();
            }
        }
    }

    if (auto el = ui.Element({.label = "Sidebar Mode", .layout = guikit::ElementLayout::Stacked})) {
        if (ui.Segmented(s_state.mode, {"Expanded", "Compact", "Collapsed"}).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({
        .label = "BottomPanel (Resizable Dock)",
        .description = "Drag top border (Sash) to resize or collapse"
    })) {
        if (ui.Switch(s_state.showBottomPanel).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({
        .label = "StatusBar (Bottom Strip)",
        .description = "Compact telemetry & action strip at window bottom"
    })) {
        if (ui.Switch(s_state.showStatusBar).changed) ui.InvalidateAll();
    }

    if (s_state.showStatusBar) {
        if (auto el = ui.Element({.label = "StatusBar Style", .layout = guikit::ElementLayout::Stacked})) {
            if (ui.Segmented(s_state.statusBarStyle, {"Surface", "Header", "Accent"}).changed) {
                ui.InvalidateAll();
            }
        }
    }

    if (auto el = ui.Element({.label = "Menu Size"})) {
        if (ui.StepSlider(ctx.controlsSize, {.showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
    }
    ctx.RenderVariantPicker(ui);

    static constexpr std::array<std::string_view, 2> sbMenuStyleNames = {"Pill", "Indicator"};
    size_t sbStyleIdx = (s_state.menuStyle == guikit::SidebarMenuStyle::Indicator) ? 1 : 0;
    if (auto el = ui.Element({.label = "Menu Style"})) {
        if (ui.StepSlider(sbStyleIdx, sbMenuStyleNames, {.showTicks = true, .showLabels = true}).changed) {
            s_state.menuStyle = (sbStyleIdx == 1) ? guikit::SidebarMenuStyle::Indicator : guikit::SidebarMenuStyle::Pill;
            ui.InvalidateAll();
        }
    }

    static constexpr std::array<std::string_view, 2> sbSideNames = {"Left", "Right"};
    size_t sbSideIdx = (s_state.side == guikit::SidebarSide::Right) ? 1 : 0;
    if (auto el = ui.Element({.label = "Dock Side"})) {
        if (ui.StepSlider(sbSideIdx, sbSideNames, {.showTicks = true, .showLabels = true}).changed) {
            s_state.side = (sbSideIdx == 1) ? guikit::SidebarSide::Right : guikit::SidebarSide::Left;
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({
        .label = "Mouse Resizable (Sash)",
        .description = "Drag inner border to resize or snap"
    })) {
        if (ui.Switch(s_state.resizable).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({
        .label = "Allow Compact (Icons)",
        .description = "Snap/toggle to icons-only rail"
    })) {
        if (ui.Switch(s_state.allowCompact).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({
        .label = "Allow Collapse (0px)",
        .description = "Snap/toggle to completely minimized"
    })) {
        if (ui.Switch(s_state.allowCollapse).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Section Headers"})) {
        if (ui.Switch(s_state.showSections).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Badges (Corner / Trailing)"})) {
        if (ui.Switch(s_state.showBadges).changed) ui.InvalidateAll();
    }
    const bool notExpanded = (s_state.mode != guikit::SidebarMode::Expanded);
    if (auto el = ui.Element({.label = "Auto-Fit Width", .disabled = notExpanded})) {
        if (ui.Switch(s_state.autoWidth, {.disabled = notExpanded}).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Default Width", .disabled = notExpanded || s_state.autoWidth})) {
        if (ui.Slider(s_state.fixedWidth, 140.0f, 320.0f, {.valueFormat = "%.0f px", .disabled = notExpanded || s_state.autoWidth}).changed) {
            ui.InvalidateAll();
        }
    }
}

void RenderSidebarStage(guikit::Context& ui, DemoContext& ctx) {
    if (auto workbenchShell = ui.Column({
        .gap = 0.0f,
        .widthMode = guikit::SizeMode::Stretch,
        .heightMode = guikit::SizeMode::Stretch
    })) {
        if (auto demoWorkspace = ui.Box({
            .gap = 8.0f,
            .widthMode = guikit::SizeMode::Stretch,
            .heightMode = guikit::SizeMode::Stretch
        })) {
            // 1. Optional VS Code style ActivityBar docked on the outer edge, linked to s_state.mode
            if (s_state.showActivityBar) {
                if (auto bar = ui.Size(ctx.controlsSize).ActivityBar(s_state.mode, {
                    .side = s_state.side,
                    .style = s_state.activityStyle,
                    .color = ctx.ResolveControlColor()
                })) {
                    if (bar.Item(s_state.activityIdx, 0, {
                        .icon = guikit::Icon::Folder(),
                        .label = "Explorer",
                        .tooltip = "Explorer (Click active icon to toggle Sidebar)"
                    }).changed) {
                        s_state.menuIdx = 0;
                    }
                    if (bar.Item(s_state.activityIdx, 1, {
                        .icon = guikit::Icon::Search(),
                        .label = "Analytics",
                        .tooltip = "Search & Real-Time Analytics",
                        .badge = s_state.showBadges ? "2" : ""
                    }).changed) {
                        s_state.menuIdx = 1;
                    }
                    if (bar.Item(s_state.activityIdx, 2, {
                        .icon = guikit::Icon::Play(),
                        .label = "Deployments",
                        .tooltip = "Deployments & CI/CD Pipelines",
                        .badge = s_state.showBadges ? "3" : ""
                    }).changed) {
                        s_state.menuIdx = 2;
                    }

                    bar.Spacer();
                    bar.Separator();

                    const guikit::Response gearBtn = bar.Action({
                        .icon = guikit::Icon::Settings(),
                        .label = "Settings",
                        .tooltip = "Settings & Quick Actions"
                    });
                    const auto gearMenu = ui.ContextMenu(gearBtn, {
                        {.label = "Command Palette...", .shortcut = "Ctrl+Shift+P", .icon = guikit::Icon::Search()},
                        {.label = "Settings",           .shortcut = "Ctrl+,",       .icon = guikit::Icon::Settings(), .separatorAfter = true},
                        {.label = "Toggle Sidebar",     .shortcut = "Ctrl+B",       .icon = guikit::Icon::ChevronLeft()},
                        {.label = "Toggle BottomPanel", .shortcut = "Ctrl+J",       .icon = guikit::Icon::ChevronUp()}
                    }, {
                        .trigger = guikit::ContextMenuTrigger::LeftClick,
                        .minWidth = 220.0f
                    });
                    if (gearMenu.clickedIndex == 1) {
                        s_state.menuIdx = 3;
                        if (s_state.mode == guikit::SidebarMode::Collapsed) {
                            s_state.mode = guikit::SidebarMode::Expanded;
                        }
                        ui.InvalidateAll();
                    } else if (gearMenu.clickedIndex == 2) {
                        s_state.mode = (s_state.mode == guikit::SidebarMode::Collapsed)
                            ? guikit::SidebarMode::Expanded
                            : guikit::SidebarMode::Collapsed;
                        ui.InvalidateAll();
                    } else if (gearMenu.clickedIndex == 3) {
                        s_state.showBottomPanel = !s_state.showBottomPanel;
                        ui.InvalidateAll();
                    }
                }
            }

            // 2. Primary Sidebar docked flush next to ActivityBar (or on outer edge when standalone)
            if (auto sb = ui.Sidebar(s_state.mode, {
                .side = s_state.side,
                .width = s_state.autoWidth ? std::nullopt : std::optional<float>(s_state.fixedWidth),
                .resizable = s_state.resizable,
                .allowCompact = s_state.allowCompact,
                .allowCollapse = s_state.allowCollapse,
                .scrollable = true
            })) {
                const int prevIdx = s_state.menuIdx;
                if (auto menu = ui.Size(ctx.controlsSize).SidebarMenu({
                    .style = s_state.menuStyle,
                    .color = ctx.ResolveControlColor()
                })) {
                    static constexpr std::array<std::string_view, 3> actHeaders = {
                        "EXPLORER", "ANALYTICS", "PIPELINES"
                    };
                    if (s_state.showSections) {
                        menu.Section(s_state.showActivityBar
                            ? actHeaders[static_cast<size_t>(std::clamp(s_state.activityIdx, 0, 2))]
                            : std::string_view("WORKSPACE"));
                    }
                    menu.Item(s_state.menuIdx, 0, {
                        .text = "Overview",
                        .icon = guikit::Icon::Folder(),
                        .badge = s_state.showBadges ? "3" : ""
                    });
                    menu.Item(s_state.menuIdx, 1, {
                        .text = "Analytics",
                        .icon = guikit::Icon::Search(),
                        .badge = s_state.showBadges ? "New" : ""
                    });
                    menu.Item(s_state.menuIdx, 2, {
                        .text = "Deployments",
                        .icon = guikit::Icon::Play()
                    });
                    if (s_state.showSections) {
                        menu.Section("SYSTEM");
                    }
                    menu.Item(s_state.menuIdx, 3, {
                        .text = "Settings",
                        .icon = guikit::Icon::Settings()
                    });
                }
                if (s_state.menuIdx != prevIdx) {
                    if (s_state.menuIdx <= 2) {
                        s_state.activityIdx = s_state.menuIdx;
                    }
                    ui.InvalidateAll();
                }
            }

            // 3. Center Main Pane
            if (auto mainPane = ui.Card({
                .gap = 8.0f,
                .padding = guikit::Padding::All(16.0f),
                .widthMode = guikit::SizeMode::Stretch,
                .heightMode = guikit::SizeMode::Stretch
            })) {
                static constexpr std::array<std::string_view, 4> pageTitles = {
                    "Overview Dashboard", "Real-Time Analytics", "Active Deployments", "System Settings"
                };
                ui.Size(guikit::UiSize::Sm).Heading({
                    .label = pageTitles[static_cast<size_t>(std::clamp(s_state.menuIdx, 0, 3))],
                    .description = ui.Format(
                        "Sidebar: {}  |  BottomPanel: {}",
                        ModeEnumName(s_state.mode),
                        s_state.showBottomPanel ? "Expanded" : "Collapsed"
                    ),
                    .divider = true
                });
                ui.Text("Workbench composition combining ActivityBar, Sidebar, BottomPanel (top-edge resizable sash), and StatusBar.", {
                    .variant = guikit::UiVariant::Secondary
                });
            }

            // 4. Resizable & Collapsible BottomPanel (docked below Main Pane)
            if (auto bp = ui.BottomPanel(s_state.showBottomPanel, {
                .height = 145.0f,
                .resizable = s_state.resizable,
                .allowCollapse = s_state.allowCollapse,
                .border = {.radius = 6.0f}
            })) {
                if (auto bpTb = ui.Size(guikit::UiSize::Sm).Toolbar({
                    .style = guikit::ToolbarStyle::Header
                })) {
                    bpTb.Item(s_state.bottomTabIdx, 0, {.text = "Output", .icon = guikit::Icon::Play()});
                    bpTb.Item(s_state.bottomTabIdx, 1, {
                        .text = "Diagnostics",
                        .icon = guikit::Icon::Warning(),
                        .badge = s_state.showBadges ? "2" : ""
                    });
                    bpTb.Item(s_state.bottomTabIdx, 2, {.text = "Cross-Section 2D", .icon = guikit::Icon::Eye()});
                    bpTb.Spacer();
                    ui.Text(ui.Format("H: {:.0f}px", bp.Height()), {
                        .role = guikit::TextRole::LabelSmall,
                        .variant = guikit::UiVariant::Muted
                    });
                    if (bpTb.Action({
                        .text = "Close",
                        .tooltip = "Collapse BottomPanel",
                        .icon = guikit::Icon::Close(),
                        .square = true
                    }).clicked) {
                        s_state.showBottomPanel = false;
                        ui.InvalidateAll();
                    }
                }

                if (auto bpBody = ui.Box({
                    .gap = 6.0f,
                    .widthMode = guikit::SizeMode::Stretch,
                    .heightMode = guikit::SizeMode::Stretch,
                    .padding = guikit::Padding::All(10.0f)
                })) {
                    static constexpr std::array<std::string_view, 3> tabContents = {
                        "[CUDA Solver] Completed 2D acoustic ray trace (0.99 km, depth 24.5 m) in 14.2 ms.",
                        "[Warn] 2 boundary reflections attenuated below -65 dB threshold.",
                        "[Slice 2D] Drag the top border of this BottomPanel up/down to resize the cross-section viewport."
                    };
                    ui.Text(tabContents[static_cast<size_t>(std::clamp(s_state.bottomTabIdx, 0, 2))], {
                        .role = guikit::TextRole::BodySmall,
                        .variant = guikit::UiVariant::Secondary
                    });
                }
            }
        }

        // 5. Optional Bottom StatusBar spanning the full workbench width
        if (s_state.showStatusBar) {
            if (auto sb = ui.Size(ctx.controlsSize).StatusBar({
                .style = s_state.statusBarStyle
            })) {
                sb.Item({
                    .text = "Workbench Ready",
                    .dotColor = ctx.previewTheme.GetVariant(guikit::UiVariant::Success).bg,
                    .color = guikit::StateColor::Success(),
                    .badge = s_state.showBadges ? "GPU" : ""
                });
                sb.Separator();
                sb.Item({
                    .text = ui.Format("Sidebar: {}", ModeEnumName(s_state.mode)),
                    .icon = guikit::Icon::Folder()
                });
                sb.Spacer();
                if (sb.Action({
                    .text = s_state.showBottomPanel ? "Hide Panel" : "Show Panel",
                    .icon = s_state.showBottomPanel ? guikit::Icon::ChevronDown() : guikit::Icon::ChevronUp(),
                    .tooltip = "Toggle BottomPanel (Ctrl+J)",
                    .active = s_state.showBottomPanel
                }).clicked) {
                    s_state.showBottomPanel = !s_state.showBottomPanel;
                    ui.InvalidateAll();
                }
            }
        }
    }
}

std::string GenerateSidebarCode(const DemoContext& ctx) {
    const std::string sideStr = (s_state.side == guikit::SidebarSide::Right) ? "Right" : "Left";
    std::string actCode;
    if (s_state.showActivityBar) {
        actCode = std::format(
            "if (auto bar = ui.Size(guikit::UiSize::{}).ActivityBar(sidebarMode, {{\n"
            "    .side = guikit::SidebarSide::{},\n"
            "    .style = guikit::ActivityBarStyle::{},\n"
            "    .color = guikit::StateColor::{}()\n"
            "}})) {{\n"
            "    bar.Item(activeTool, 0, {{.icon = guikit::Icon::Folder(), .tooltip = \"Explorer\"}});\n"
            "    bar.Item(activeTool, 1, {{.icon = guikit::Icon::Search(), .tooltip = \"Analytics\"{}}});\n"
            "    bar.Spacer();\n"
            "    bar.Separator();\n"
            "    auto gear = bar.Action({{.icon = guikit::Icon::Settings(), .tooltip = \"Settings\"}});\n"
            "}}\n\n",
            SizeName(ctx.controlsSize),
            sideStr,
            s_state.activityStyle == guikit::ActivityBarStyle::Pill ? "Pill" : "Indicator",
            VariantName(ctx.btnVariant),
            s_state.showBadges ? ", .badge = \"2\"" : ""
        );
    }

    std::string bpCode;
    if (s_state.showBottomPanel) {
        bpCode = std::format(
            "\n\nif (auto bp = ui.BottomPanel(showBottomPanel, {{\n"
            "    .height = 180.0f,\n"
            "    .resizable = {},\n"
            "    .allowCollapse = {}\n"
            "}})) {{\n"
            "    if (auto tb = ui.Toolbar({{.style = guikit::ToolbarStyle::Header}})) {{\n"
            "        tb.Item(activeTab, 0, {{.text = \"Output\", .icon = guikit::Icon::Play()}});\n"
            "        tb.Spacer();\n"
            "        if (tb.Action({{.icon = guikit::Icon::Close(), .square = true}}).clicked) showBottomPanel = false;\n"
            "    }}\n"
            "    // Bottom panel content (charts, logs, 2D slice view)\n"
            "}}",
            s_state.resizable ? "true" : "false",
            s_state.allowCollapse ? "true" : "false"
        );
    }

    std::string statusCode;
    if (s_state.showStatusBar) {
        statusCode = std::format(
            "\n\nif (auto sb = ui.StatusBar({{.style = guikit::StatusBarStyle::{}}})) {{\n"
            "    sb.Item({{.text = \"Ready\", .dotColor = theme.variants.success.base}});\n"
            "    sb.Spacer();\n"
            "    if (sb.Action({{.text = \"Bottom Panel\", .active = showBottomPanel}}).clicked) {{\n"
            "        showBottomPanel = !showBottomPanel;\n"
            "    }}\n"
            "}}",
            StatusStyleName(s_state.statusBarStyle)
        );
    }

    return actCode + std::format(
        "if (auto sb = ui.Sidebar(sidebarMode, {{\n"
        "    .side = guikit::SidebarSide::{},\n"
        "    .width = {},\n"
        "    .resizable = {},\n"
        "    .allowCompact = {},\n"
        "    .allowCollapse = {},\n"
        "    .scrollable = true\n"
        "}})) {{\n"
        "    if (auto menu = ui.Size(guikit::UiSize::{}).SidebarMenu({{\n"
        "        .style = guikit::SidebarMenuStyle::{},\n"
        "        .color = guikit::StateColor::{}()\n"
        "    }})) {{\n"
        "        menu.Section(\"WORKSPACE\");\n"
        "        menu.Item(selected, 0, {{.text = \"Overview\", .icon = guikit::Icon::Folder(){}}});\n"
        "        menu.Item(selected, 1, {{.text = \"Analytics\", .icon = guikit::Icon::Search(){}}});\n"
        "    }}\n"
        "}}",
        sideStr,
        s_state.autoWidth ? "std::nullopt" : std::format("{:.1f}f", s_state.fixedWidth),
        s_state.resizable ? "true" : "false",
        s_state.allowCompact ? "true" : "false",
        s_state.allowCollapse ? "true" : "false",
        SizeName(ctx.controlsSize),
        s_state.menuStyle == guikit::SidebarMenuStyle::Indicator ? "Indicator" : "Pill",
        VariantName(ctx.btnVariant),
        s_state.showBadges ? ", .badge = \"3\"" : "",
        s_state.showBadges ? ", .badge = \"New\"" : ""
    ) + bpCode + statusCode;
}

} // namespace guikit::demo
