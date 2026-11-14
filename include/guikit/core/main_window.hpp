#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include "guikit/context.hpp"
#include "guikit/backend/i_window_backend.hpp"
#include "guikit/core/frame_metrics.hpp"

namespace guikit {

/// Platform-agnostic base window and application loop host for GuiKit applications.
/// Lives in `guikit::core` with zero OS, windowing, or GPU dependencies.
/// Delegates low-level windowing and swapchain presentation to an interchangeable `IWindowBackend`.
class MainWindow {
public:
    using RenderCallback = std::function<void(Context& ui)>;

    explicit MainWindow(MainWindowConfig config = {}, std::unique_ptr<IWindowBackend> backend = nullptr);
    virtual ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    /// Initializes the window backend and enters the main application loop.
    /// Invokes `OnInit()`, then `OnRender(ui)` each frame, and `OnShutdown()` before exit.
    int Run();

    /// Convenience overload that runs the main loop using the provided per-frame `renderCallback`.
    int Run(RenderCallback renderCallback);

    /// Requests the main loop to terminate cleanly at the end of the current step.
    void Close() noexcept { m_running = false; }

    /// Thread-safe wakeup signal: interrupts reactive idle sleep immediately so a new frame is rendered.
    /// Safe to call from any background thread (e.g. hardware acquisition or compute pipeline callbacks).
    void WakeUp() noexcept;

    /// Opens an external URL in the user's default system browser via the active `IWindowBackend`.
    void OpenUrl(std::string_view url) const noexcept;

    // =========================================================================
    // Subsystems & State Accessors
    // =========================================================================

    [[nodiscard]] Context& GetUi() noexcept { return m_ui; }
    [[nodiscard]] const Context& GetUi() const noexcept { return m_ui; }

    [[nodiscard]] IWindowBackend* GetBackend() noexcept { return m_backend.get(); }
    [[nodiscard]] const IWindowBackend* GetBackend() const noexcept { return m_backend.get(); }

    [[nodiscard]] ICanvas& GetCanvas() noexcept { return m_backend->GetCanvas(); }
    [[nodiscard]] const ICanvas& GetCanvas() const noexcept { return m_backend->GetCanvas(); }

    [[nodiscard]] const FrameMetrics& GetMetrics() const noexcept { return m_metrics; }

    [[nodiscard]] int GetWidth() const noexcept;
    [[nodiscard]] int GetHeight() const noexcept;
    [[nodiscard]] float GetDpiScale() const noexcept;
    [[nodiscard]] float GetDisplayPixelsPerMm() const noexcept;
    [[nodiscard]] bool IsLiveResizeFrame() const noexcept;
    [[nodiscard]] bool IsMinimized() const noexcept;

    // =========================================================================
    // Engine & Shell Runtime Controls
    // =========================================================================

    [[nodiscard]] bool IsVsyncEnabled() const noexcept;
    void SetVsyncEnabled(bool enabled) noexcept;

    [[nodiscard]] bool IsReactiveMode() const noexcept { return m_reactiveMode; }
    void SetReactiveMode(bool enabled) noexcept;

    [[nodiscard]] bool IsGpuShadersEnabled() const noexcept;
    [[nodiscard]] bool IsGpuShadersAvailable() const noexcept;
    void SetGpuShadersEnabled(bool enabled) noexcept;

    [[nodiscard]] UiTheme& GetShellTheme() noexcept { return m_shellTheme; }
    [[nodiscard]] const UiTheme& GetShellTheme() const noexcept { return m_shellTheme; }
    void SetShellTheme(const UiTheme& theme) noexcept;

    [[nodiscard]] bool IsShellLightMode() const noexcept { return m_shellLightMode; }
    void SetShellLightMode(bool lightMode) noexcept;

    [[nodiscard]] bool IsShellFloating() const noexcept { return m_shellFloating; }
    void SetShellFloating(bool floating) noexcept;

    [[nodiscard]] float GetShellScale() const noexcept { return m_shellScale; }
    void SetShellScale(float scale) noexcept;
    [[nodiscard]] size_t GetShellScaleIndex() const noexcept { return m_shellScaleIndex; }
    void SetShellScaleIndex(size_t index) noexcept;

    [[nodiscard]] bool IsCommandPaletteOpen() const noexcept { return m_commandPaletteOpen; }
    void SetCommandPaletteOpen(bool open) noexcept;

    // =========================================================================
    // Composable Shell UI Helpers (Opt-In Toolbar / StatusBar Building Blocks)
    // =========================================================================

    /// Renders a 3-state sidebar cycle button (`Expanded -> Compact -> Collapsed` on LMB, toggle collapse on RMB).
    bool RenderSidebarCycleButton(
        ToolbarScope& tb,
        SidebarMode& mode,
        std::string_view label = "Nav",
        std::string_view sidebarTitle = "Navigation Sidebar",
        const Icon& icon = Icon::Folder()
    );

    /// Renders a 2-state sidebar collapse/expand toggle button (`Expanded <-> Collapsed`).
    bool RenderSidebarToggle(
        ToolbarScope& tb,
        SidebarMode& mode,
        std::string_view label = "Inspector",
        std::string_view sidebarTitle = "Inspector Sidebar",
        const Icon& icon = Icon::Settings()
    );

    /// Renders the UI scale selector (`100%..200%`), `Dark/Light` theme switch, and `Dock/Float` shell switch.
    void RenderThemeControls(ToolbarScope& tb);

    /// Renders the `Bounds` (`Ctrl+B`) and `Inspect` (`Ctrl+I`) debug overlay toggles.
    void RenderDebugControls(ToolbarScope& tb);

    /// Renders the engine pipeline toggles (`GPU`, `VSync`, `React`, `Dirty`, `Live`) and `Heartbeat` cycler.
    void RenderEngineControls(ToolbarScope& tb);

    /// Renders the GPU shader status indicator (`Ready` / `Active` / `CPU Fallback`) and non-jumping
    /// `FPS | CPU | GPU | RAM` telemetry segments inside an active `StatusBarScope`.
    void RenderTelemetryStatus(StatusBarScope& sb, bool gpuSceneActive = false);

    /// Handles the `Ctrl+K` global shortcut, renders the `"Command Palette"` action button in `sb`,
    /// and renders the modal `CommandPalette` overlay.
    CommandPaletteResponse RenderCommandPalette(
        StatusBarScope& sb,
        std::span<const CommandPaletteItem> items,
        UiText placeholder = "Jump to screen or run command..."
    );

    static constexpr std::array<float, 5> kScaleValues = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
    static constexpr std::array<std::string_view, 5> kScaleLabels = {
        "100%",
        "125%",
        "150%",
        "175%",
        "200%"
    };

    static constexpr std::array<int, 5> kHeartbeatMsValues = {-1, 250, 500, 1000, 2000};
    static constexpr std::array<std::string_view, 5> kHeartbeatLabels = {
        "Heartbeat: Off",
        "Heartbeat: 4 Hz",
        "Heartbeat: 2 Hz",
        "Heartbeat: 1 Hz",
        "Heartbeat: 0.5 Hz"
    };

protected:
    /// Called once after the `IWindowBackend` and `guikit::Context` have been initialized.
    virtual void OnInit() {}

    /// Called every frame to construct the immediate-mode UI tree.
    virtual void OnRender(Context& ui);

    /// Called when the window pixel dimensions change.
    virtual void OnResize(int width, int height) {
        (void)width;
        (void)height;
    }

    /// Called when the user or OS requests the window to close.
    /// Return `true` to allow closing, or `false` to veto and keep the window open (e.g., to show an unsaved-changes modal).
    virtual bool OnCloseRequested() { return true; }

    /// Called once before the window and GPU resources are destroyed.
    virtual void OnShutdown() {}

private:
    void RenderFrame();

    MainWindowConfig m_config;
    std::unique_ptr<IWindowBackend> m_backend;
    RenderCallback m_renderCallback;

    Context m_ui;
    FrameMetrics m_metrics;

    UiTheme m_shellTheme = UiTheme::Dark(Density::Standard);
    bool m_shellLightMode = false;
    bool m_shellFloating = false;
    float m_shellScale = 1.0f;
    size_t m_shellScaleIndex = 0;
    size_t m_heartbeatIndex = 2;
    bool m_commandPaletteOpen = false;

    bool m_running = false;
    bool m_reactiveMode = true;
    bool m_inRenderFrame = false;
};

} // namespace guikit
