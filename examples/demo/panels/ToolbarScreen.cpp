#include "../DemoContext.hpp"
#include <array>
#include <format>

namespace guikit::demo {

namespace {
struct ToolbarDemoState {
    guikit::ToolbarStyle style = guikit::ToolbarStyle::Surface;
    bool showVerticalPalette = true;
    bool showFloatingHud = true;
    bool showSearchBox = true;
    bool showBadges = true;
    bool squareIcons = false;
    int activeTool = 0;
    int zoomPresetIdx = 1;
    bool snapToGrid = true;
    bool showCrosshair = true;
    bool liveCapture = false;
    std::string filterText{};
    std::string lastCommand{"Ready"};
};
ToolbarDemoState s_tb;

constexpr std::string_view StyleEnumName(guikit::ToolbarStyle s) noexcept {
    switch (s) {
        case guikit::ToolbarStyle::Header:   return "Header";
        case guikit::ToolbarStyle::Floating: return "Floating";
        default:                             return "Surface";
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

    if (auto el = ui.Element({.label = "Toolbar Size"})) {
        if (ui.StepSlider(ctx.controlsSize, {.showTicks = true, .showLabels = true, .includeNone = true}).changed) {
            ui.InvalidateAll();
        }
    }
    ctx.RenderVariantPicker(ui);

    if (auto el = ui.Element({
        .label = "Vertical Tool Palette",
        .description = "Orientation::Vertical docked on left"
    })) {
        if (ui.Switch(s_tb.showVerticalPalette).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({
        .label = "Floating HUD Island",
        .description = "ToolbarStyle::Floating pill bar"
    })) {
        if (ui.Switch(s_tb.showFloatingHud).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Inline Search Input"})) {
        if (ui.Switch(s_tb.showSearchBox).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Icon-Only Square Tools"})) {
        if (ui.Switch(s_tb.squareIcons).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Action Badges"})) {
        if (ui.Switch(s_tb.showBadges).changed) ui.InvalidateAll();
    }
}

void RenderToolbarStage(guikit::Context& ui, DemoContext& ctx) {
    // 1. Primary Horizontal Command Toolbar
    if (auto tb = ui.Size(ctx.controlsSize).Toolbar({
        .style = s_tb.style
    })) {
        // Mutually exclusive tool group via tb.Item(...)
        tb.Item(s_tb.activeTool, 0, {
            .text = "Select",
            .tooltip = "Select Tool (V)",
            .icon = guikit::Icon::Check(),
            .color = ctx.ResolveControlColor(),
            .square = s_tb.squareIcons
        });
        tb.Item(s_tb.activeTool, 1, {
            .text = "Pan",
            .tooltip = "Pan Viewport (H)",
            .icon = guikit::Icon::Folder(),
            .color = ctx.ResolveControlColor(),
            .square = s_tb.squareIcons
        });
        tb.Item(s_tb.activeTool, 2, {
            .text = "Measure",
            .tooltip = "Ruler & Harmonic Cursor (M)",
            .icon = guikit::Icon::Search(),
            .color = ctx.ResolveControlColor(),
            .square = s_tb.squareIcons
        });

        tb.Separator();

        // Stateful boolean toggles via tb.Toggle(...)
        tb.Toggle(s_tb.snapToGrid, {
            .text = "Snap",
            .tooltip = "Toggle Grid Snapping (Ctrl+G)",
            .icon = guikit::Icon::Settings(),
            .color = guikit::StateColor::Info(),
            .square = s_tb.squareIcons
        });
        tb.Toggle(s_tb.liveCapture, {
            .text = s_tb.liveCapture ? "Recording" : "Capture",
            .tooltip = "Toggle Live Stream Capture",
            .icon = s_tb.liveCapture ? guikit::Icon::Pause() : guikit::Icon::Play(),
            .color = s_tb.liveCapture ? guikit::StateColor::Success() : guikit::StateColor::Warning(),
            .square = s_tb.squareIcons,
            .badge = (s_tb.showBadges && s_tb.liveCapture) ? "REC" : ""
        });

        tb.Separator();

        // Dropdown menu button via tb.Dropdown(...)
        const auto exportMenu = tb.Dropdown({
            .text = "Export",
            .tooltip = "Export Snapshot or Telemetry",
            .badge = s_tb.showBadges ? "3" : ""
        }, {
            {.label = "Export Vector SVG...", .shortcut = "Ctrl+E",       .icon = guikit::Icon::Folder()},
            {.label = "Export Raw IQ (Float32)", .shortcut = "Ctrl+Shift+E", .icon = guikit::Icon::Copy(), .separatorAfter = true},
            {.label = "Copy Clipboard Summary", .shortcut = "Ctrl+C",     .icon = guikit::Icon::Check()}
        });
        if (exportMenu.clicked) {
            static constexpr std::array<std::string_view, 3> kExportNames = {
                "Exported Vector SVG", "Exported Raw IQ", "Copied Summary"
            };
            if (exportMenu.clickedIndex >= 0 && exportMenu.clickedIndex < 3) {
                s_tb.lastCommand = std::string(kExportNames[static_cast<size_t>(exportMenu.clickedIndex)]);
                ui.ShowToast({
                    .title = s_tb.lastCommand,
                    .message = "Triggered from Toolbar::Dropdown menu",
                    .variant = guikit::UiVariant::Success,
                    .duration = 2.5f
                });
            }
        }

        if (s_tb.style != guikit::ToolbarStyle::Floating) {
            tb.Spacer();
        }

        if (s_tb.showSearchBox) {
            ui.InputField(s_tb.filterText, {
                .placeholder = "Quick filter...",
                .prefix = guikit::Icon::Search(),
                .width = 160.0f,
                .clearable = true
            });
        }
    }

    // 2. Main Workspace Row with Vertical Tool Palette + Central Canvas + Floating HUD Island
    if (auto bodyRow = ui.Row({
        .gap = 12.0f,
        .heightMode = guikit::SizeMode::Stretch
    })) {
        if (s_tb.showVerticalPalette) {
            if (auto vtb = ui.Size(ctx.controlsSize).Toolbar({
                .style = guikit::ToolbarStyle::Surface,
                .orientation = guikit::Orientation::Vertical
            })) {
                vtb.Item(s_tb.activeTool, 0, {
                    .text = "Select Tool",
                    .icon = guikit::Icon::Check(),
                    .color = ctx.ResolveControlColor(),
                    .square = true
                });
                vtb.Item(s_tb.activeTool, 1, {
                    .text = "Pan Viewport",
                    .icon = guikit::Icon::Folder(),
                    .color = ctx.ResolveControlColor(),
                    .square = true
                });
                vtb.Item(s_tb.activeTool, 2, {
                    .text = "Measure Cursor",
                    .icon = guikit::Icon::Search(),
                    .color = ctx.ResolveControlColor(),
                    .square = true
                });
                vtb.Separator();
                vtb.Toggle(s_tb.showCrosshair, {
                    .text = "Crosshair Overlay",
                    .icon = guikit::Icon::Eye(),
                    .color = guikit::StateColor::Info(),
                    .square = true
                });
                vtb.Spacer();
                if (vtb.Action({
                    .text = "Reset View",
                    .icon = guikit::Icon::Refresh(),
                    .square = true
                }).clicked) {
                    s_tb.activeTool = 0;
                    s_tb.zoomPresetIdx = 1;
                    s_tb.lastCommand = "Reset View";
                }
            }
        }

        if (auto canvasCard = ui.Card({
            .gap = 12.0f,
            .padding = guikit::Padding::All(16.0f),
            .widthMode = guikit::SizeMode::Stretch,
            .heightMode = guikit::SizeMode::Stretch
        })) {
            static constexpr std::array<std::string_view, 3> kToolNames = {"Select", "Pan", "Measure"};
            ui.Size(guikit::UiSize::Sm).Heading({
                .label = "Interactive Toolbar Workspace",
                .description = ui.Format(
                    "Active Tool: {}  |  Snap: {}  |  Capture: {}  |  Last Action: {}",
                    kToolNames[static_cast<size_t>(std::clamp(s_tb.activeTool, 0, 2))],
                    s_tb.snapToGrid ? "ON" : "OFF",
                    s_tb.liveCapture ? "RECORDING" : "IDLE",
                    s_tb.lastCommand
                ),
                .divider = true
            });

            ui.Text("Toolbar supports Horizontal and Vertical orientations, Surface / Header / Floating island styles, exclusive Item() groups, stateful Toggle() switches, and Dropdown() popup menus.", {
                .variant = guikit::UiVariant::Secondary
            });

            if (s_tb.showFloatingHud) {
                if (auto centerRow = ui.Row({
                    .justify = guikit::Justify::Center
                })) {
                    if (auto hud = ui.Size(guikit::UiSize::Sm).Toolbar({
                        .style = guikit::ToolbarStyle::Floating
                    })) {
                        hud.Item(s_tb.zoomPresetIdx, 0, {.text = "50%"});
                        hud.Item(s_tb.zoomPresetIdx, 1, {.text = "100%"});
                        hud.Item(s_tb.zoomPresetIdx, 2, {.text = "200%"});
                        hud.Separator();
                        hud.Toggle(s_tb.showCrosshair, {
                            .text = "Crosshair",
                            .icon = guikit::Icon::Eye(),
                            .color = guikit::StateColor::Info()
                        });
                    }
                }
            }
        }
    }
}

std::string GenerateToolbarCode(const DemoContext& ctx) {
    return std::format(
        "if (auto tb = ui.{}Toolbar({{\n"
        "    .style = guikit::ToolbarStyle::{}\n"
        "}})) {{\n"
        "    // Exclusive tool group\n"
        "    tb.Item(activeTool, 0, {{.text = \"Select\",  .icon = guikit::Icon::Check(),  .color = guikit::StateColor::{}()}});\n"
        "    tb.Item(activeTool, 1, {{.text = \"Pan\",     .icon = guikit::Icon::Folder(), .color = guikit::StateColor::{}()}});\n"
        "    tb.Item(activeTool, 2, {{.text = \"Measure\", .icon = guikit::Icon::Search(), .color = guikit::StateColor::{}()}});\n"
        "    tb.Separator();\n\n"
        "    // Stateful boolean toggles\n"
        "    tb.Toggle(snapToGrid,  {{.text = \"Snap\",    .icon = guikit::Icon::Settings(), .color = guikit::StateColor::Info()}});\n"
        "    tb.Toggle(liveCapture, {{.text = \"Capture\", .icon = guikit::Icon::Play(),     .color = guikit::StateColor::Success()}});\n"
        "    tb.Separator();\n\n"
        "    // Dropdown menu button\n"
        "    auto menu = tb.Dropdown({{.text = \"Export\"{}}}, {{\n"
        "        {{.label = \"Export Vector SVG...\", .shortcut = \"Ctrl+E\", .icon = guikit::Icon::Download()}},\n"
        "        {{.label = \"Copy Summary\",         .shortcut = \"Ctrl+C\", .icon = guikit::Icon::Check()}}\n"
        "    }});\n"
        "}}",
        FormatUiSizePrefix(ctx.controlsSize),
        StyleEnumName(s_tb.style),
        VariantName(ctx.btnVariant),
        VariantName(ctx.btnVariant),
        VariantName(ctx.btnVariant),
        s_tb.showBadges ? ", .badge = \"3\"" : ""
    );
}

} // namespace guikit::demo
