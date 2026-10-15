#include "../DemoContext.hpp"
#include <format>

namespace guikit::demo {

namespace {
struct TabBarState {
    int idx = 0;
    guikit::TabBarStyle style = guikit::TabBarStyle::Underline;
    float gap = 4.0f;
    bool showSep = true;
    bool fullWidth = true;
    bool scrollable = false;
    bool hasBg = true;
    float radius = 8.0f;
    float padding = 3.0f;
    float borderWidth = 1.0f;
};
TabBarState s_state;
} // namespace

void RenderTabBarInspector(guikit::Context& ui, DemoContext&) {
    static constexpr std::array<std::string_view, 2> styleNames = {"Underline", "Pill"};
    size_t styleIdx = (s_state.style == guikit::TabBarStyle::Pill) ? 1 : 0;
    if (ui.StepSlider(styleIdx, styleNames, {.label = "TabBar Style", .showTicks = true, .showLabels = true}).changed) {
        s_state.style = (styleIdx == 1) ? guikit::TabBarStyle::Pill : guikit::TabBarStyle::Underline;
        ui.InvalidateAll();
    }
    if (ui.Switch(s_state.hasBg, {.label = "Container Background Fill"}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.radius, 0.0f, 24.0f, {.label = "Container Corner Radius", .valueFormat = "%.0f px", .disabled = !s_state.hasBg}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.padding, 0.0f, 12.0f, {.label = "Container Internal Padding", .valueFormat = "%.0f px", .disabled = !s_state.hasBg}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.borderWidth, 0.0f, 4.0f, {.label = "Container Border Width", .valueFormat = "%.1f px", .disabled = !s_state.hasBg}).changed) ui.InvalidateAll();
    if (ui.Slider(s_state.gap, 0.0f, 24.0f, {.label = "Tab Spacing (Gap)", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.showSep, {.label = "Baseline Separator"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.fullWidth, {.label = "Full Width Stretch"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_state.scrollable, {.label = "Scrollable Ribbon"}).changed) ui.InvalidateAll();
}

void RenderTabBarStage(guikit::Context& ui, DemoContext& ctx) {
    static const std::array<guikit::TabItem, 3> demoTabs = {{
        {.label = "First", .icon = guikit::Icon::Settings()},
        {.label = "Second", .icon = guikit::Icon::Eye()},
        {.label = "Third", .icon = guikit::Icon::Play()}
    }};
    auto& th = ctx.previewTheme;
    ui.TabBar(s_state.idx, demoTabs, {
        .style = s_state.style,
        .showSeparator = s_state.showSep,
        .fullWidth = s_state.fullWidth,
        .scrollable = s_state.scrollable,
        .gap = s_state.gap,
        .padding = s_state.hasBg ? guikit::Padding::All(s_state.padding) : guikit::Padding{},
        .borderRadius = s_state.hasBg ? s_state.radius : 0.0f,
        .backgroundColor = s_state.hasBg ? th.palette.surfaceElevated : guikit::Color::Transparent(),
        .borderColor = s_state.hasBg ? th.palette.borderSubtle : guikit::Color::Transparent(),
        .borderWidth = s_state.hasBg ? s_state.borderWidth : 0.0f
    });
}

std::string GenerateTabBarCode(const DemoContext& ctx) {
    return std::format(
        "ui.TabBar(activeTab, tabItems, {{\n"
        "    .style = guikit::TabBarStyle::{},\n"
        "    .showSeparator = {},\n"
        "    .fullWidth = {},\n"
        "    .scrollable = {},\n"
        "    .gap = {:.1f}f,\n"
        "    .padding = guikit::Padding::All({:.1f}f),\n"
        "    .borderRadius = {:.1f}f,\n"
        "    .backgroundColor = {},\n"
        "    .borderColor = {},\n"
        "    .borderWidth = {:.1f}f,\n"
        "    .activeColor = guikit::Color::Transparent(),\n"
        "    .size = guikit::UiSize::{},\n"
        "    .theme = nullptr,\n"
        "    .scale = std::nullopt\n"
        "}});",
        s_state.style == guikit::TabBarStyle::Pill ? "Pill" : "Underline",
        s_state.showSep ? "true" : "false",
        s_state.fullWidth ? "true" : "false",
        s_state.scrollable ? "true" : "false",
        s_state.gap,
        s_state.hasBg ? s_state.padding : 0.0f,
        s_state.hasBg ? s_state.radius : 0.0f,
        s_state.hasBg ? "theme.palette.surfaceElevated" : "guikit::Color::Transparent()",
        s_state.hasBg ? "theme.palette.borderSubtle" : "guikit::Color::Transparent()",
        s_state.hasBg ? s_state.borderWidth : 0.0f,
        SizeName(ctx.controlsSize)
    );
}

} // namespace guikit::demo
