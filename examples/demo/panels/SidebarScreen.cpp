#include "../DemoContext.hpp"
#include <algorithm>
#include <format>

namespace guikit::demo {

namespace {
struct SidebarState {
    int menuIdx = 0;
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
};
SidebarState s_state;

constexpr std::string_view ModeEnumName(guikit::SidebarMode m) noexcept {
    switch (m) {
        case guikit::SidebarMode::Compact:   return "Compact";
        case guikit::SidebarMode::Collapsed: return "Collapsed";
        default:                             return "Expanded";
    }
}
} // namespace

void RenderSidebarInspector(guikit::Context& ui, DemoContext& ctx) {
    if (auto el = ui.Element({.label = "Sidebar Mode", .layout = guikit::ElementLayout::Stacked})) {
        if (ui.Segmented(s_state.mode, {"Expanded", "Compact", "Collapsed"}).changed) {
            ui.InvalidateAll();
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
    if (auto el = ui.Element({.label = "Show Trailing Badges"})) {
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
    if (auto demoWorkspace = ui.Box({
        .gap = 12.0f,
        .widthMode = guikit::SizeMode::Stretch,
        .heightMode = guikit::SizeMode::Stretch
    })) {
        if (auto sb = ui.Sidebar(s_state.mode, {
            .side = s_state.side,
            .width = s_state.autoWidth ? std::nullopt : std::optional<float>(s_state.fixedWidth),
            .minWidth = 140.0f,
            .maxWidth = 360.0f,
            .resizable = s_state.resizable,
            .allowCompact = s_state.allowCompact,
            .allowCollapse = s_state.allowCollapse,
            .scrollable = true
        })) {
            const int prevIdx = s_state.menuIdx;
            if (auto menu = ui.Size(ctx.controlsSize).SidebarMenu({
                .style = s_state.menuStyle,
                .variant = ctx.btnVariant
            })) {
                if (s_state.showSections) {
                    menu.Section("WORKSPACE");
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
                ui.InvalidateAll();
            }
        }

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
                .description = ui.Format("Mode: {}  |  Drag the sidebar border or click the edge button to toggle", ModeEnumName(s_state.mode)),
                .divider = true
            });
            ui.Text("Remaining workspace automatically stretches around docked Left/Right Sidebars. Dragging the inner border past the minimum threshold snaps into Compact (icons-only) or Collapsed (0px) mode.", {
                .variant = guikit::UiVariant::Secondary
            });
        }
    }
}

std::string GenerateSidebarCode(const DemoContext& ctx) {
    return std::format(
        "if (auto sb = ui.Sidebar(sidebarMode, {{\n"
        "    .side = guikit::SidebarSide::{},\n"
        "    .width = {},\n"
        "    .minWidth = 140.0f,\n"
        "    .maxWidth = 360.0f,\n"
        "    .resizable = {},\n"
        "    .allowCompact = {},\n"
        "    .allowCollapse = {},\n"
        "    .scrollable = true\n"
        "}})) {{\n"
        "    if (auto menu = ui.Size(guikit::UiSize::{}).SidebarMenu({{\n"
        "        .style = guikit::SidebarMenuStyle::{},\n"
        "        .variant = guikit::UiVariant::{}\n"
        "    }})) {{\n"
        "        menu.Section(\"WORKSPACE\");\n"
        "        menu.Item(selected, 0, {{.text = \"Overview\", .icon = guikit::Icon::Folder(){}}});\n"
        "        menu.Item(selected, 1, {{.text = \"Analytics\", .icon = guikit::Icon::Search(){}}});\n"
        "    }}\n"
        "}}",
        s_state.side == guikit::SidebarSide::Right ? "Right" : "Left",
        s_state.autoWidth ? "std::nullopt" : std::format("{:.1f}f", s_state.fixedWidth),
        s_state.resizable ? "true" : "false",
        s_state.allowCompact ? "true" : "false",
        s_state.allowCollapse ? "true" : "false",
        SizeName(ctx.controlsSize),
        s_state.menuStyle == guikit::SidebarMenuStyle::Indicator ? "Indicator" : "Pill",
        VariantName(ctx.btnVariant),
        s_state.showBadges ? ", .badge = \"3\"" : "",
        s_state.showBadges ? ", .badge = \"New\"" : ""
    );
}

} // namespace guikit::demo
