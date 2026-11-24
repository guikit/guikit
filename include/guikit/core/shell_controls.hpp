#pragma once

#include "guikit/core/main_window.hpp"
#include "guikit/panels/command_palette.hpp"
#include "guikit/panels/sidebar.hpp"
#include "guikit/panels/status_bar.hpp"
#include "guikit/panels/toolbar.hpp"

namespace guikit {

/// Composable opt-in shell-chrome controls (theme/scale/debug/engine popovers, telemetry status, command palette).
/// Decoupled from `MainWindow` so headless and custom-chrome apps pay zero widget/panel coupling cost.
class ShellControls {
public:
    ShellControls() noexcept = default;
    explicit ShellControls(keel::i32 initialIdleHeartbeatMs) noexcept;

    [[nodiscard]] bool IsCommandPaletteOpen() const noexcept { return m_commandPaletteOpen; }
    void SetCommandPaletteOpen(MainWindow& win, bool open) noexcept;

    /// Renders a 3-state sidebar cycle button (`Expanded -> Compact -> Collapsed` on LMB, toggle collapse on RMB).
    bool RenderSidebarCycleButton(
        MainWindow& win,
        ToolbarScope& tb,
        SidebarMode& mode,
        keel::StringRef label = "Nav",
        keel::StringRef sidebarTitle = "Navigation Sidebar",
        const Icon& icon = Icon::Folder()
    );

    /// Renders a 2-state sidebar collapse/expand toggle button (`Expanded <-> Collapsed`).
    bool RenderSidebarToggle(
        MainWindow& win,
        ToolbarScope& tb,
        SidebarMode& mode,
        keel::StringRef label = "Inspector",
        keel::StringRef sidebarTitle = "Inspector Sidebar",
        const Icon& icon = Icon::Settings()
    );

    /// Renders the UI scale selector (`75%..200%`), theme preset selector, and `Dock/Float` shell switch.
    void RenderThemeControls(MainWindow& win, ToolbarScope& tb);

    /// Renders the `Bounds` (`Ctrl+B`) and `Inspect` (`Ctrl+I`) debug overlay toggles.
    void RenderDebugControls(MainWindow& win, ToolbarScope& tb);

    /// Renders the compact `Engine` popover trigger and its floating pipeline settings menu.
    void RenderEngineControls(MainWindow& win, ToolbarScope& tb);

    /// Renders the GPU shader status indicator and non-jumping `FPS | CPU | GPU | RAM` telemetry segments.
    void RenderTelemetryStatus(MainWindow& win, StatusBarScope& sb, bool gpuSceneActive = false);

    /// Handles `Ctrl+K`, renders the `"Command Palette"` action button in `sb`, and renders the modal palette overlay.
    CommandPaletteResponse RenderCommandPalette(
        MainWindow& win,
        StatusBarScope& sb,
        keel::ArrayRef<CommandPaletteItem> items,
        UiText placeholder = "Jump to screen or run command..."
    );

private:
    keel::usize m_heartbeatIndex{2};
    bool m_enginePopoverOpen{false};
    bool m_commandPaletteOpen{false};
};

} // namespace guikit
