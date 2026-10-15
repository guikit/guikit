#include "../DemoContext.hpp"
#include <algorithm>
#include <format>

namespace guikit::demo {

namespace {
struct SidebarState {
    int menuIdx = 0;
    guikit::SidebarMenuStyle menuStyle = guikit::SidebarMenuStyle::Pill;
    guikit::SidebarSide side = guikit::SidebarSide::Left;
    bool collapsed = false;
    bool showBadges = true;
    bool showSections = true;
    bool autoWidth = true;
    float fixedWidth = 200.0f;
};
SidebarState s_state;
} // namespace

void RenderSidebarInspector(guikit::Context& ui, DemoContext& ctx) {
    if (ui.StepSlider(ctx.controlsSize, {.label = "Menu Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
    ctx.RenderVariantPicker(ui);

    static constexpr std::array<std::string_view, 2> sbMenuStyleNames = {"Pill", "Indicator"};
    size_t sbStyleIdx = (s_state.menuStyle == guikit::SidebarMenuStyle::Indicator) ? 1 : 0;
    if (ui.StepSlider(sbStyleIdx, sbMenuStyleNames, {.label = "Menu Style", .showTicks = true, .showLabels = true}).changed) {
        s_state.menuStyle = (sbStyleIdx == 1) ? guikit::SidebarMenuStyle::Indicator : guikit::SidebarMenuStyle::Pill;
        ui.InvalidateAll();
    }

    static constexpr std::array<std::string_view, 2> sbSideNames = {"Left", "Right"};
    size_t sbSideIdx = (s_state.side == guikit::SidebarSide::Right) ? 1 : 0;
    if (ui.StepSlider(sbSideIdx, sbSideNames, {.label = "Dock Side", .showTicks = true, .showLabels = true}).changed) {
        s_state.side = (sbSideIdx == 1) ? guikit::SidebarSide::Right : guikit::SidebarSide::Left;
        ui.InvalidateAll();
    }

    if (ui.Switch(s_state.collapsed, {.label = "Collapsed Rail (Icons Only)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showSections, {.label = "Show Section Headers"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showBadges, {.label = "Show Trailing Badges"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.autoWidth, {.label = "Auto-Fit Width", .disabled = s_state.collapsed}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.fixedWidth, 120.0f, 300.0f, {.label = "Explicit Width", .valueFormat = "%.0f px", .disabled = s_state.collapsed || s_state.autoWidth}).changed) ui.InvalidateAll();
}

void RenderSidebarStage(guikit::Context& ui, DemoContext& ctx) {
    if (auto demoWorkspace = ui.Box({
        .gap = 12.0f,
        .widthMode = guikit::SizeMode::Stretch,
        .heightMode = guikit::SizeMode::Stretch
    })) {
        if (auto sb = ui.Sidebar({
            .side = s_state.side,
            .width = (s_state.collapsed || s_state.autoWidth) ? std::nullopt : std::optional<float>(s_state.fixedWidth),
            .scrollable = true
        })) {
            const int prevIdx = s_state.menuIdx;
            if (auto menu = ui.SidebarMenu({
                .style = s_state.menuStyle,
                .collapsed = s_state.collapsed,
                .variant = ctx.btnVariant,
                .size = ctx.controlsSize
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
            .gap = 6.0f,
            .padding = guikit::Padding::All(16.0f),
            .widthMode = guikit::SizeMode::Stretch,
            .heightMode = guikit::SizeMode::Stretch
        })) {
            static constexpr std::array<std::string_view, 4> pageTitles = {
                "Overview Dashboard", "Real-Time Analytics", "Active Deployments", "System Settings"
            };
            ui.Heading({
                .title = pageTitles[static_cast<size_t>(std::clamp(s_state.menuIdx, 0, 3))],
                .subtitle = "Docked Sidebar + SidebarMenu container arrangement",
                .size = guikit::UiSize::Sm,
                .divider = true
            });
            ui.Text("Remaining workspace automatically stretches around docked Left/Right Sidebars.", {
                .variant = guikit::UiVariant::Secondary
            });
        }
    }
}

std::string GenerateSidebarCode(const DemoContext& ctx) {
    return std::format(
        "if (auto sb = ui.Sidebar({{\n"
        "    .side = guikit::SidebarSide::{},\n"
        "    .width = {},\n"
        "    .scrollable = true\n"
        "}})) {{\n"
        "    if (auto menu = ui.SidebarMenu({{\n"
        "        .style = guikit::SidebarMenuStyle::{},\n"
        "        .collapsed = {},\n"
        "        .variant = guikit::UiVariant::{},\n"
        "        .size = guikit::UiSize::{}\n"
        "    }})) {{\n"
        "        menu.Section(\"WORKSPACE\");\n"
        "        menu.Item(selected, 0, {{.text = \"Overview\", .icon = guikit::Icon::Folder(){}}});\n"
        "        menu.Item(selected, 1, {{.text = \"Analytics\", .icon = guikit::Icon::Search(){}}});\n"
        "    }}\n"
        "}}",
        s_state.side == guikit::SidebarSide::Right ? "Right" : "Left",
        (s_state.collapsed || s_state.autoWidth) ? "std::nullopt" : std::format("{:.1f}f", s_state.fixedWidth),
        s_state.menuStyle == guikit::SidebarMenuStyle::Indicator ? "Indicator" : "Pill",
        s_state.collapsed ? "true" : "false",
        VariantName(ctx.btnVariant),
        SizeName(ctx.controlsSize),
        s_state.showBadges ? ", .badge = \"3\"" : "",
        s_state.showBadges ? ", .badge = \"New\"" : ""
    );
}

} // namespace guikit::demo
