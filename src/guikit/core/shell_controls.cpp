#include "guikit/core/shell_controls.hpp"

#include "guikit/core/element.hpp"
#include "guikit/layout/row.hpp"
#include "guikit/display/separator.hpp"
#include "guikit/surfaces/card.hpp"
#include "guikit/overlays/popover.hpp"
#include "guikit/display/heading.hpp"
#include "guikit/fields/select.hpp"
#include "guikit/controls/slider.hpp"
#include "guikit/controls/switch.hpp"

namespace guikit {

ShellControls::ShellControls(keel::i32 initialIdleHeartbeatMs) noexcept {
    for (keel::usize i = 0; i < MainWindow::kHeartbeatMsValues.size(); ++i) {
        if (MainWindow::kHeartbeatMsValues[i] == initialIdleHeartbeatMs) {
            m_heartbeatIndex = i;
            break;
        }
    }
}

void ShellControls::SetCommandPaletteOpen(MainWindow& win, bool open) noexcept {
    if (m_commandPaletteOpen != open) {
        m_commandPaletteOpen = open;
        win.GetUi().InvalidateAll();
    }
}

bool ShellControls::RenderSidebarCycleButton(
    MainWindow& win,
    ToolbarScope& tb,
    SidebarMode& mode,
    keel::StringRef label,
    keel::StringRef sidebarTitle,
    const Icon& icon
) {
    Context& ui = win.GetUi();
    const ButtonStyle btnStyle = (mode == SidebarMode::Collapsed)
        ? ButtonStyle::Ghost
        : ButtonStyle::Solid;
    const UiVariant btnVariant = (mode == SidebarMode::Expanded)
        ? UiVariant::Primary
        : (mode == SidebarMode::Compact)
            ? UiVariant::Info
            : UiVariant::Default;

    const keel::StringRef tooltip = (mode == SidebarMode::Expanded)
        ? ui.Format("{}: Expanded (Click for Compact Icons, Right-click to Collapse)", sidebarTitle)
        : (mode == SidebarMode::Compact)
            ? ui.Format("{}: Compact Icons (Click to Collapse, Right-click to Expand)", sidebarTitle)
            : ui.Format("{}: Collapsed (Click to Expand)", sidebarTitle);

    const auto btn = tb.ToolButton({
        .text = label,
        .tooltip = tooltip,
        .icon = icon,
        .style = btnStyle,
        .variant = btnVariant,
        .active = (mode != SidebarMode::Collapsed)
    });

    if (btn.clicked) {
        if (mode == SidebarMode::Expanded) {
            mode = SidebarMode::Compact;
        } else if (mode == SidebarMode::Compact) {
            mode = SidebarMode::Collapsed;
        } else {
            mode = SidebarMode::Expanded;
        }
        ui.InvalidateAll();
        return true;
    }
    if (btn.rightClicked) {
        mode = (mode == SidebarMode::Collapsed)
            ? SidebarMode::Expanded
            : SidebarMode::Collapsed;
        ui.InvalidateAll();
        return true;
    }
    return false;
}

bool ShellControls::RenderSidebarToggle(
    MainWindow& win,
    ToolbarScope& tb,
    SidebarMode& mode,
    keel::StringRef label,
    keel::StringRef sidebarTitle,
    const Icon& icon
) {
    Context& ui = win.GetUi();
    bool visible = (mode != SidebarMode::Collapsed);
    const keel::StringRef tooltip = visible
        ? ui.Format("{}: Expanded (Click to Collapse)", sidebarTitle)
        : ui.Format("{}: Collapsed (Click to Expand)", sidebarTitle);

    if (tb.Toggle(visible, {
        .text = label,
        .tooltip = tooltip,
        .icon = icon,
        .variant = UiVariant::Primary
    }).changed) {
        mode = visible ? SidebarMode::Expanded : SidebarMode::Collapsed;
        ui.InvalidateAll();
        return true;
    }
    return false;
}

void ShellControls::RenderThemeControls(MainWindow& win, ToolbarScope& tb) {
    (void)tb;
    Context& ui = win.GetUi();
    static constexpr keel::StaticArray<keel::StringRef, kThemePresetCount> kPresetLabels = {{
        ThemePresetName(ThemePreset::Dark),
        ThemePresetName(ThemePreset::Light),
        ThemePresetName(ThemePreset::Nord),
        ThemePresetName(ThemePreset::Dracula),
        ThemePresetName(ThemePreset::CatppuccinMocha),
        ThemePresetName(ThemePreset::CatppuccinLatte),
        ThemePresetName(ThemePreset::Industrial),
        ThemePresetName(ThemePreset::IndustrialLight),
        ThemePresetName(ThemePreset::HighContrastDark),
        ThemePresetName(ThemePreset::HighContrastLight),
        ThemePresetName(ThemePreset::Windows98)
    }};

    keel::usize scaleIdx = win.GetShellScaleIndex();
    if (guikit::Select(ui, scaleIdx, MainWindow::kScaleLabels, {.size = UiSize::Sm, .width = 96.0f}).changed) {
        win.SetShellScaleIndex(scaleIdx);
    }

    keel::usize presetIndex = keel::as<keel::usize>(win.GetShellThemePreset());
    if (presetIndex >= kThemePresetCount) {
        presetIndex = 0u;
    }
    if (guikit::Select(ui, presetIndex, kPresetLabels, {.size = UiSize::Sm, .width = 160.0f}).changed) {
        win.SetShellThemePreset(keel::from_underlying<ThemePreset>(presetIndex));
    }

    bool floating = win.IsShellFloating();
    if (guikit::Switch(ui, floating, {
        .textLeft = "Dock",
        .textRight = "Float",
        .symmetric = true
    }).changed) {
        win.SetShellFloating(floating);
    }
}

void ShellControls::RenderDebugControls(MainWindow& win, ToolbarScope& tb) {
    Context& ui = win.GetUi();
    if (auto debugGroup = guikit::Row(ui, {.equalWidths = true})) {
        bool boundsActive = ui.IsDebugBounds();
        if (tb.Toggle(boundsActive, {
            .text = "Bounds",
            .icon = boundsActive ? Icon::Eye() : Icon::EyeOff(),
            .variant = UiVariant::Info
        }).changed) {
            ui.SetDebugBounds(boundsActive);
        }

        bool inspectActive = ui.IsInspectMode();
        if (tb.Toggle(inspectActive, {
            .text = "Inspect",
            .icon = Icon::Search(),
            .variant = UiVariant::Warning
        }).changed) {
            ui.SetInspectMode(inspectActive);
        }
    }
}

void ShellControls::RenderEngineControls(MainWindow& win, ToolbarScope& tb) {
    Context& ui = win.GetUi();
    static constexpr keel::StaticArray<keel::StringRef, 5> kHeartbeatShortLabels = {{
        "Off",
        "4 Hz (250 ms)",
        "2 Hz (500 ms)",
        "1 Hz (1000 ms)",
        "0.5 Hz (2000 ms)"
    }};

    const Response engineBtn = tb.Toggle(m_enginePopoverOpen, {
        .text = "Engine",
        .tooltip = "Engine & Render Pipeline Settings",
        .icon = Icon::ChevronDown(),
        .iconPlacement = Placement::Trailing
    });

    if (auto pop = guikit::Popover(ui, m_enginePopoverOpen, {
        .anchorRect = engineBtn.rect
    })) {
        if (auto card = guikit::Card(ui, {
            .width = 296.0f,
            .gap = 8.0f,
            .padding = Padding::All(14.0f),
            .surface = SurfaceRole::Raised
        })) {
            guikit::Heading(ui, {
                .label = "Engine Pipeline",
                .level = HeadingLevel::H5,
                .separator = HeadingSeparator::Bottom
            });

            bool gpuEnabled = win.IsGpuShadersEnabled();
            if (auto el = guikit::Element(ui, {
                .label = "GPU Shaders",
                .description = "Hardware shader underlays",
                .layout = ElementLayout::Leading,
                .disabled = !win.IsGpuShadersAvailable()
            })) {
                if (guikit::Switch(ui, gpuEnabled, {.size = UiSize::Sm})) {
                    win.SetGpuShadersEnabled(gpuEnabled);
                }
            }

            bool vsync = win.IsVsyncEnabled();
            if (auto el = guikit::Element(ui, {
                .label = "VSync",
                .description = "Synchronize swap interval",
                .layout = ElementLayout::Leading
            })) {
                if (guikit::Switch(ui, vsync, {.size = UiSize::Sm})) {
                    win.SetVsyncEnabled(vsync);
                }
            }

            bool reactive = win.IsReactiveMode();
            if (auto el = guikit::Element(ui, {
                .label = "Reactive Loop",
                .description = "Idle sleep at 0% CPU",
                .layout = ElementLayout::Leading
            })) {
                if (guikit::Switch(ui, reactive, {.size = UiSize::Sm})) {
                    win.SetReactiveMode(reactive);
                }
            }

            bool dirtyActive = ui.IsDirtyRectEnabled();
            if (auto el = guikit::Element(ui, {
                .label = "Dirty-Rect Raster",
                .description = "Disjoint partial redraws",
                .layout = ElementLayout::Leading
            })) {
                if (guikit::Switch(ui, dirtyActive, {.size = UiSize::Sm})) {
                    ui.SetDirtyRectEnabled(dirtyActive);
                    if (!dirtyActive) {
                        ui.InvalidateAll();
                    }
                }
            }

            bool liveResizeActive = ui.IsLiveResizeEnabled();
            if (auto el = guikit::Element(ui, {
                .label = "Live Window Resize",
                .description = "Reflow while dragging edge",
                .layout = ElementLayout::Leading
            })) {
                if (guikit::Switch(ui, liveResizeActive, {.size = UiSize::Sm})) {
                    ui.SetLiveResize(liveResizeActive);
                }
            }

            guikit::Separator(ui);

            if (auto el = guikit::Element(ui, {
                .label = "Idle Heartbeat",
                .layout = ElementLayout::Leading,
                .disabled = !win.IsReactiveMode()
            })) {
                keel::usize hbIdx = m_heartbeatIndex;
                if (guikit::Select(ui, hbIdx, kHeartbeatShortLabels, {.size = UiSize::Sm, .width = 128.0f})) {
                    m_heartbeatIndex = hbIdx % MainWindow::kHeartbeatMsValues.size();
                    ui.SetIdleHeartbeatMs(MainWindow::kHeartbeatMsValues[m_heartbeatIndex]);
                }
            }

            guikit::Separator(ui);

            ICanvas& canvas = win.GetCanvas();
            guikit::Heading(ui, {
                .label = "Text Rendering",
                .badge = {canvas.GetTextEngineName(), StateColor::Primary()},
                .level = HeadingLevel::H5
            });

            static constexpr keel::StaticArray<keel::StringRef, 4> kSmoothingLabels = {{
                "Auto",
                "Grayscale",
                "Subpixel RGB",
                "Subpixel BGR"
            }};
            static constexpr keel::StaticArray<keel::StringRef, 4> kHintingLabels = {{
                "None",
                "Light",
                "Normal",
                "Auto"
            }};

            TextRenderSettings textCfg = win.GetTextRenderSettings();
            const bool supportsHinting = canvas.SupportsFontHinting();
            bool textChanged = false;

            if (auto el = guikit::Element(ui, {
                .label = "Smoothing",
                .layout = ElementLayout::Leading
            })) {
                keel::usize smoothIdx = keel::as<keel::usize>(textCfg.smoothing);
                if (guikit::Select(ui, smoothIdx, kSmoothingLabels, {.size = UiSize::Sm, .width = 128.0f})) {
                    textCfg.smoothing = keel::from_underlying<FontSmoothing>(smoothIdx % kSmoothingLabels.size());
                    textChanged = true;
                }
            }

            if (auto el = guikit::Element(ui, {
                .label = "Hinting",
                .layout = ElementLayout::Leading,
                .disabled = !supportsHinting
            })) {
                keel::usize hintIdx = keel::as<keel::usize>(textCfg.hinting);
                if (guikit::Select(ui, hintIdx, kHintingLabels, {.disabled = !supportsHinting, .size = UiSize::Sm, .width = 128.0f})) {
                    textCfg.hinting = keel::from_underlying<FontHinting>(hintIdx % kHintingLabels.size());
                    textChanged = true;
                }
            }

            if (auto el = guikit::Element(ui, {
                .label = "Gamma Blending",
                .description = "Linear sRGB text compositing",
                .layout = ElementLayout::Leading
            })) {
                if (guikit::Switch(ui, textCfg.gammaCorrectBlending, {.size = UiSize::Sm})) {
                    textChanged = true;
                }
            }

            if (auto el = guikit::Element(ui, {
                .label = "Contrast",
                .layout = ElementLayout::Leading
            })) {
                if (guikit::Slider(ui, textCfg.contrast, 0.6f, 1.8f, {
                    .valueFormat = "%.2f",
                    .size = UiSize::Sm,
                    .width = 136.0f
                })) {
                    textChanged = true;
                }
            }

            if (auto el = guikit::Element(ui, {
                .label = "Stem Darkening",
                .layout = ElementLayout::Leading
            })) {
                if (guikit::Slider(ui, textCfg.stemDarkening, 0.0f, 0.45f, {
                    .valueFormat = "%.2f",
                    .size = UiSize::Sm,
                    .width = 136.0f
                })) {
                    textChanged = true;
                }
            }

            if (textChanged) {
                win.SetTextRenderSettings(textCfg);
            }
        }
    }
}

void ShellControls::RenderTelemetryStatus(MainWindow& win, StatusBarScope& sb, bool gpuSceneActive) {
    const UiTheme& theme = win.GetShellTheme();
    const FrameMetrics& metrics = win.GetMetrics();
    if (win.IsGpuShadersAvailable()) {
        sb.Item({
            .text = {
                gpuSceneActive ? "GPU Shaders: Active" : "GPU Shaders: Ready",
                {"GPU Shaders: Active", "GPU Shaders: Ready"}
            },
            .dotColor = gpuSceneActive
                ? theme.GetVariant(UiVariant::Success).bg
                : theme.GetVariant(UiVariant::Info).bg,
            .color = gpuSceneActive ? StateColor::Success() : StateColor::Info()
        });
    } else {
        sb.Item({
            .text = "GPU Shaders: CPU Fallback",
            .dotColor = theme.GetVariant(UiVariant::Warning).bg,
            .color = StateColor::Warning()
        });
    }

    sb.Separator();
    sb.Item({
        .text = {metrics.GetFpsString(), {"0 FPS (Idle)", "999 FPS"}},
        .color = StateColor::Secondary()
    });
    sb.Separator();
    sb.Item({
        .text = {metrics.GetCpuString(), {"CPU: 99.99 ms (100.0%)"}},
        .color = StateColor::Secondary()
    });
    sb.Separator();
    sb.Item({
        .text = {metrics.GetGpuString(), {"GPU: 99.99 ms (100% budget)"}},
        .color = StateColor::Secondary()
    });
    sb.Separator();
    sb.Item({
        .text = {metrics.GetRamString(), {"RAM: 9999.9 MB"}},
        .color = StateColor::Secondary()
    });
}

CommandPaletteResponse ShellControls::RenderCommandPalette(
    MainWindow& win,
    StatusBarScope& sb,
    keel::ArrayRef<CommandPaletteItem> items,
    UiText placeholder
) {
    Context& ui = win.GetUi();
    if (ui.ConsumeShortcut(Key::K, true)) {
        m_commandPaletteOpen = !m_commandPaletteOpen;
        ui.InvalidateAll();
    }
    if (sb.Action({
        .text = "Command Palette",
        .icon = Icon::Search(),
        .tooltip = "Open Global Command Palette (Ctrl+K)",
        .active = m_commandPaletteOpen
    }).clicked) {
        m_commandPaletteOpen = true;
        ui.InvalidateAll();
    }
    return guikit::CommandPalette(ui, m_commandPaletteOpen, items, {.placeholder = placeholder});
}

} // namespace guikit
