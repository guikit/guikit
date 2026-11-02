#include "../demo_context.hpp"
#include <array>
#include <format>

namespace guikit::demo {

namespace {
struct ToolbarDemoState {
    guikit::ToolbarStyle style = guikit::ToolbarStyle::Surface;
    guikit::Orientation orientation = guikit::Orientation::Horizontal;
    guikit::ToolbarOverflow overflow = guikit::ToolbarOverflow::AutoCompact;
    bool showBadges = true;
    bool squareIcons = false;
    int activeTool = 0;
    bool snapToGrid = true;
    bool liveCapture = false;
};
ToolbarDemoState s_tb;

constexpr std::string_view StyleEnumName(guikit::ToolbarStyle s) noexcept {
    switch (s) {
        case guikit::ToolbarStyle::Header:   return "Header";
        case guikit::ToolbarStyle::Floating: return "Floating";
        default:                             return "Surface";
    }
}

constexpr std::string_view OverflowEnumName(guikit::ToolbarOverflow o) noexcept {
    switch (o) {
        case guikit::ToolbarOverflow::Scroll: return "Scroll";
        case guikit::ToolbarOverflow::Wrap:   return "Wrap";
        case guikit::ToolbarOverflow::Clip:   return "Clip";
        default:                              return "AutoCompact";
    }
}
} // namespace

void RenderToolbarInspector(guikit::Context& ui, DemoContext& ctx) {
    static constexpr std::array<std::string_view, 3> kStyleLabels = {"Surface", "Header", "Floating"};
    size_t styleIdx = (s_tb.style == guikit::ToolbarStyle::Header)
        ? 1
        : (s_tb.style == guikit::ToolbarStyle::Floating ? 2 : 0);
    if (auto el = ui.Element({.label = "Toolbar Style", .layout = guikit::ElementLayout::Stacked})) {
        if (ui.Segmented(styleIdx, kStyleLabels).changed) {
            s_tb.style = (styleIdx == 1)
                ? guikit::ToolbarStyle::Header
                : (styleIdx == 2 ? guikit::ToolbarStyle::Floating : guikit::ToolbarStyle::Surface);
            ui.InvalidateAll();
        }
    }

    static constexpr std::array<std::string_view, 2> kOrientLabels = {"Horizontal", "Vertical"};
    size_t orientIdx = (s_tb.orientation == guikit::Orientation::Vertical) ? 1 : 0;
    if (auto el = ui.Element({.label = "Orientation", .layout = guikit::ElementLayout::Stacked})) {
        if (ui.Segmented(orientIdx, kOrientLabels).changed) {
            s_tb.orientation = (orientIdx == 1)
                ? guikit::Orientation::Vertical
                : guikit::Orientation::Horizontal;
            ui.InvalidateAll();
        }
    }

    if (s_tb.orientation == guikit::Orientation::Horizontal) {
        static constexpr std::array<std::string_view, 4> kOverflowLabels = {"Compact", "Scroll", "Wrap", "Clip"};
        size_t overflowIdx = static_cast<size_t>(s_tb.overflow);
        if (auto el = ui.Element({.label = "Overflow Policy", .layout = guikit::ElementLayout::Stacked})) {
            if (ui.Segmented(overflowIdx, kOverflowLabels).changed) {
                s_tb.overflow = static_cast<guikit::ToolbarOverflow>(overflowIdx);
                ui.InvalidateAll();
            }
        }
    }

    if (auto el = ui.Element({.label = "Toolbar Size"})) {
        if (ui.StepSlider(ctx.controlsSize, {.showTicks = true, .showLabels = true, .includeNone = true}).changed) {
            ui.InvalidateAll();
        }
    }
    ctx.RenderVariantPicker(ui);

    if (auto el = ui.Element({.label = "Icon-Only Square Tools"})) {
        if (ui.Switch(s_tb.squareIcons).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Action Badges"})) {
        if (ui.Switch(s_tb.showBadges).changed) ui.InvalidateAll();
    }
}

void RenderToolbarStage(guikit::Context& ui, DemoContext& ctx) {
    const bool isVert = (s_tb.orientation == guikit::Orientation::Vertical);
    const bool isSq = s_tb.squareIcons || isVert;

    if (auto tb = ui.Size(ctx.controlsSize).Toolbar({
        .style = s_tb.style,
        .orientation = s_tb.orientation,
        .overflow = s_tb.overflow
    })) {
        tb.Item(s_tb.activeTool, 0, {
            .text = "Select",
            .tooltip = "Select Tool (V)",
            .icon = guikit::Icon::Check(),
            .color = ctx.ResolveControlColor(),
            .square = isSq
        });
        tb.Item(s_tb.activeTool, 1, {
            .text = "Pan",
            .tooltip = "Pan Viewport (H)",
            .icon = guikit::Icon::Folder(),
            .color = ctx.ResolveControlColor(),
            .square = isSq
        });
        tb.Item(s_tb.activeTool, 2, {
            .text = "Measure",
            .tooltip = "Measure Cursor (M)",
            .icon = guikit::Icon::Search(),
            .color = ctx.ResolveControlColor(),
            .square = isSq
        });

        tb.Separator();

        tb.Toggle(s_tb.snapToGrid, {
            .text = "Snap",
            .tooltip = "Toggle Grid Snapping",
            .icon = guikit::Icon::Settings(),
            .color = guikit::StateColor::Info(),
            .square = isSq
        });
        tb.Toggle(s_tb.liveCapture, {
            .text = s_tb.liveCapture ? "Recording" : "Capture",
            .tooltip = "Toggle Live Capture",
            .icon = s_tb.liveCapture ? guikit::Icon::Pause() : guikit::Icon::Play(),
            .color = s_tb.liveCapture ? guikit::StateColor::Success() : guikit::StateColor::Warning(),
            .square = isSq,
            .badge = (s_tb.showBadges && s_tb.liveCapture) ? "REC" : ""
        });

        tb.Separator();

        tb.Dropdown({
            .text = "Export",
            .tooltip = "Export Snapshot",
            .icon = guikit::Icon::Save(),
            .square = isSq,
            .badge = s_tb.showBadges ? "3" : ""
        }, {
            {.label = "Export Vector SVG...", .shortcut = "Ctrl+E",       .icon = guikit::Icon::Folder()},
            {.label = "Export Raw IQ (Float32)", .shortcut = "Ctrl+Shift+E", .icon = guikit::Icon::Copy(), .separatorAfter = true},
            {.label = "Copy Clipboard Summary", .shortcut = "Ctrl+C",     .icon = guikit::Icon::Check()}
        });
    }
}

std::string GenerateToolbarCode(const DemoContext& ctx) {
    return std::format(
        "if (auto tb = ui.{}Toolbar({{\n"
        "    .style = guikit::ToolbarStyle::{},\n"
        "    .orientation = guikit::Orientation::{},\n"
        "    .overflow = guikit::ToolbarOverflow::{}\n"
        "}})) {{\n"
        "    tb.Item(activeTool, 0, {{.text = \"Select\",  .icon = guikit::Icon::Check(),  .color = guikit::StateColor::{}(), .square = {}}});\n"
        "    tb.Item(activeTool, 1, {{.text = \"Pan\",     .icon = guikit::Icon::Folder(), .color = guikit::StateColor::{}(), .square = {}}});\n"
        "    tb.Item(activeTool, 2, {{.text = \"Measure\", .icon = guikit::Icon::Search(), .color = guikit::StateColor::{}(), .square = {}}});\n"
        "    tb.Separator();\n"
        "    tb.Toggle(snapToGrid,  {{.text = \"Snap\",    .icon = guikit::Icon::Settings(), .color = guikit::StateColor::Info(), .square = {}}});\n"
        "    tb.Toggle(liveCapture, {{.text = \"Capture\", .icon = guikit::Icon::Play(),     .color = guikit::StateColor::Success(), .square = {}}});\n"
        "    tb.Separator();\n"
        "    auto menu = tb.Dropdown({{.text = \"Export\"{}}}, {{\n"
        "        {{.label = \"Export Vector SVG...\", .shortcut = \"Ctrl+E\", .icon = guikit::Icon::Folder()}},\n"
        "        {{.label = \"Copy Summary\",         .shortcut = \"Ctrl+C\", .icon = guikit::Icon::Check()}}\n"
        "    }});\n"
        "}}",
        FormatUiSizePrefix(ctx.controlsSize),
        StyleEnumName(s_tb.style),
        s_tb.orientation == guikit::Orientation::Vertical ? "Vertical" : "Horizontal",
        OverflowEnumName(s_tb.overflow),
        VariantName(ctx.btnVariant),
        s_tb.squareIcons ? "true" : "false",
        VariantName(ctx.btnVariant),
        s_tb.squareIcons ? "true" : "false",
        VariantName(ctx.btnVariant),
        s_tb.squareIcons ? "true" : "false",
        s_tb.squareIcons ? "true" : "false",
        s_tb.squareIcons ? "true" : "false",
        s_tb.showBadges ? ", .badge = \"3\"" : ""
    );
}

} // namespace guikit::demo
