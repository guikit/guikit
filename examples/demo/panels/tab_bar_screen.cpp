#include "../demo_context.hpp"

namespace guikit::demo {

namespace {
struct TabBarState {
    keel::i32 idx = 0;
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

void RenderTabBarInspector(guikit::Context& ui, DemoContext& ctx) {
    if (auto el = ui.Element({.label = "TabBar Size"})) {
        if (ui.StepSlider(ctx.controlsSize, {.showTicks = true, .showLabels = true, .includeNone = true}).changed) {
            ui.InvalidateAll();
        }
    }
    static constexpr keel::StaticArray<keel::StringRef, 4> styleNames = {"Underline", "Pill", "Card", "Folder"};
    if (auto el = ui.Element({.label = "TabBar Style"})) {
        if (ui.Segmented(s_state.style, styleNames).changed) {
            ui.InvalidateAll();
        }
    }
    if (auto el = ui.Element({.label = "Container Background Fill"})) {
        if (ui.Switch(s_state.hasBg).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Container Corner Radius", .disabled = !s_state.hasBg})) {
        if (ui.Slider(s_state.radius, 0.0f, 24.0f, {.valueFormat = "%.0f px", .disabled = !s_state.hasBg}).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Container Internal Padding", .disabled = !s_state.hasBg})) {
        if (ui.Slider(s_state.padding, 0.0f, 12.0f, {.valueFormat = "%.0f px", .disabled = !s_state.hasBg}).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Container Border Width", .disabled = !s_state.hasBg})) {
        if (ui.Slider(s_state.borderWidth, 0.0f, 4.0f, {.valueFormat = "%.1f px", .disabled = !s_state.hasBg}).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Tab Spacing (Gap)"})) {
        if (ui.Slider(s_state.gap, 0.0f, 24.0f, {.valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Baseline Separator"})) {
        if (ui.Switch(s_state.showSep).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Full Width Stretch"})) {
        if (ui.Switch(s_state.fullWidth).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Scrollable Ribbon"})) {
        if (ui.Switch(s_state.scrollable).changed) ui.InvalidateAll();
    }
}

void RenderTabBarStage(guikit::Context& ui, DemoContext& ctx) {
    static const keel::StaticArray<guikit::TabItem, 3> demoTabs = {{
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
        .size = ctx.controlsSize,
        .background = s_state.hasBg ? guikit::StateColor{th.palette.surfaceElevated} : guikit::StateColor::None(),
        .border = {
            .width = s_state.hasBg ? s_state.borderWidth : 0.0f,
            .radius = s_state.hasBg ? s_state.radius : 0.0f,
            .color = s_state.hasBg ? guikit::StateColor{th.palette.borderSubtle} : guikit::StateColor{}
        }
    });
}

keel::String GenerateTabBarCode(const DemoContext& ctx) {
    static constexpr keel::StaticArray<keel::StringRef, 4> kStyleCodeNames = {"Underline", "Pill", "Card", "Folder"};
    const keel::usize styleIdx = keel::min<keel::usize>(static_cast<keel::usize>(s_state.style), 3);
    return keel::format(
        "ui.TabBar(activeTab, tabItems, {{\n"
        "    .style = guikit::TabBarStyle::{},\n"
        "    .showSeparator = {},\n"
        "    .fullWidth = {},\n"
        "    .scrollable = {},\n"
        "    .gap = {:.1f}f,\n"
        "    .padding = guikit::Padding::All({:.1f}f){},\n"
        "    .background = {},\n"
        "    .border = {{.width = {:.1f}f, .radius = {:.1f}f, .color = {}}}\n"
        "}});",
        kStyleCodeNames[styleIdx],
        s_state.showSep ? "true" : "false",
        s_state.fullWidth ? "true" : "false",
        s_state.scrollable ? "true" : "false",
        s_state.gap,
        s_state.hasBg ? s_state.padding : 0.0f,
        FormatUiSizePrefix(ctx.controlsSize),
        s_state.hasBg ? "guikit::StateColor{theme.palette.surfaceElevated}" : "guikit::StateColor::None()",
        s_state.hasBg ? s_state.borderWidth : 0.0f,
        s_state.hasBg ? s_state.radius : 0.0f,
        s_state.hasBg ? "guikit::StateColor{theme.palette.borderSubtle}" : "guikit::StateColor{}"
    );
}

} // namespace guikit::demo
