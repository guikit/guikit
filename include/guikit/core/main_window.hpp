#pragma once

#include "guikit/backend/i_platform_window.hpp"
#include "guikit/backend/vector_backend.hpp"
#include "guikit/context.hpp"
#include "guikit/core/frame_metrics.hpp"
#include "keel/array.h"
#include "keel/core.h"

namespace guikit {

/// Configuration options for initializing a `guikit::MainWindow` application host.
struct MainWindowConfig {
    keel::FixedString<128> title{"GuiKit Application"};
    keel::i32 width = 1380;
    keel::i32 height = 800;
    keel::i32 minWidth = 320;
    keel::i32 minHeight = 240;
    bool resizable = true;
    bool maximized = false;
    bool fullscreen = false;
    bool borderless = false;
    bool highDpi = true;
    bool vsync = true;
    bool reactiveMode = true;
    bool gpuShaders = true;
    bool debugShortcuts = true; ///< Enables Ctrl+I / Ctrl+Shift+C (Inspect) and Ctrl+B (Bounds)
    bool scaleShortcuts = true; ///< Enables Ctrl+Plus / Ctrl+Minus / Ctrl+0 UI shell scaling
    bool autoShellScope = true; ///< Wraps OnRender() in a root Column with shellTheme (shell zoom is always frame-wide)
    keel::i32 idleHeartbeatMs = 500;  ///< Periodic wakeup interval in reactive mode (-1 to disable)
    keel::f32 targetFps = 60.0f;      ///< Target frame rate for telemetry budget and non-VSync frame pacing
    keel::f32 initialScale = 1.0f;    ///< Initial shell UI scale, snapped to the nearest `MainWindow::kScaleValues` preset
    ThemeMode initialThemeMode = ThemeMode::Dark;
    keel::Optional<ThemePreset> initialThemePreset = keel::kNullOpt; ///< Optional curated theme preset (overrides `initialThemeMode`)
    bool followSystemTheme = false; ///< If true (and `initialTheme` / `initialThemePreset` are unset), adopts OS dark/light theme on startup
    keel::f32 initialDensity = 1.0f; ///< Initial layout gap/padding density multiplier (1.0f = standard)
    ShellStyle initialShellStyle = ShellStyle::Docked;
    keel::Optional<UiTheme> initialTheme = keel::kNullOpt; ///< Optional complete initial theme override
    TextRenderSettings textRender{};                       ///< Initial text anti-aliasing, hinting, gamma & contrast settings
    VectorCanvasFactory canvasFactory = nullptr;           ///< Optional per-window custom `ICanvas` factory override
};

/// Platform-agnostic window and application loop host for GuiKit applications.
/// Lives in `guikit::core` with zero OS, windowing, or widget dependencies.
/// Composes a single `IPlatformWindow` + `ICanvas` directly (supports both single-window `Run()`
/// and multi-window cooperative `Initialize()` / `StepFrame()` / `Shutdown()` hosting).
class MainWindow {
public:
    using RenderCallback = keel::Function<void(Context& ui)>;

    explicit MainWindow(
        MainWindowConfig config = {},
        keel::UniquePtr<IPlatformWindow> window = nullptr,
        keel::UniquePtr<ICanvas> canvas = nullptr,
        GraphicsApi graphicsApi = GraphicsApi::None
    );

    MainWindow(
        MainWindowConfig config,
        keel::UniquePtr<IPlatformWindow> window,
        PlatformCanvasFactory canvasFactory,
        GraphicsApi graphicsApi = GraphicsApi::OpenGLES3
    );

    virtual ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    /// Initializes the OS window, graphics context, canvas, and calls `OnInit()`.
    /// Enables cooperative multi-window loops via `StepFrame()` + `Shutdown()`.
    bool Initialize();

    /// Pumps OS events and renders one frame if needed. Returns `false` when the window has closed.
    bool StepFrame();

    /// Releases the canvas, framebuffer, and OS window after calling `OnShutdown()`.
    void Shutdown() noexcept;

    /// Initializes the window and enters the main application loop until closed.
    keel::i32 Run();

    /// Convenience overload that runs the main loop using the provided per-frame `renderCallback`.
    keel::i32 Run(RenderCallback renderCallback);

    /// Sets the per-frame render callback (useful before `Initialize()` / `StepFrame()`).
    void SetRenderCallback(RenderCallback renderCallback) noexcept {
        m_renderCallback = keel::move(renderCallback);
    }

    /// Requests the window loop to terminate cleanly at the end of the current step.
    void Close() noexcept { m_running = false; }

    /// Returns true while the window is initialized and running.
    [[nodiscard]] bool IsRunning() const noexcept { return m_initialized && m_running; }

    /// Thread-safe wakeup signal: interrupts reactive idle sleep immediately so a new frame is rendered.
    void WakeUp() noexcept;

    /// Opens an external URL in the user's default system browser via the active `IPlatformWindow`.
    void OpenUrl(keel::StringRef url) const noexcept;

    // =========================================================================
    // Subsystems & State Accessors
    // =========================================================================

    [[nodiscard]] Context& GetUi() noexcept { return m_ui; }
    [[nodiscard]] const Context& GetUi() const noexcept { return m_ui; }

    [[nodiscard]] IPlatformWindow* GetPlatformWindow() noexcept { return m_window.get(); }
    [[nodiscard]] const IPlatformWindow* GetPlatformWindow() const noexcept { return m_window.get(); }

    [[nodiscard]] ICanvas& GetCanvas() noexcept;
    [[nodiscard]] const ICanvas& GetCanvas() const noexcept;

    [[nodiscard]] const FrameMetrics& GetMetrics() const noexcept { return m_metrics; }
    [[nodiscard]] const MainWindowConfig& GetConfig() const noexcept { return m_config; }

    [[nodiscard]] keel::i32 GetWidth() const noexcept;
    [[nodiscard]] keel::i32 GetHeight() const noexcept;
    [[nodiscard]] keel::f32 GetDpiScale() const noexcept;
    [[nodiscard]] keel::f32 GetDisplayPixelsPerMm() const noexcept;
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

    /// Returns true when the active canvas owns a persistent CPU framebuffer safe for partial dirty-rect redraws.
    [[nodiscard]] bool IsDirtyRectAvailable() const noexcept;

    [[nodiscard]] const TextRenderSettings& GetTextRenderSettings() const noexcept;
    void SetTextRenderSettings(const TextRenderSettings& settings) noexcept;

    [[nodiscard]] UiTheme& GetShellTheme() noexcept { return m_shellTheme; }
    [[nodiscard]] const UiTheme& GetShellTheme() const noexcept { return m_shellTheme; }
    void SetShellTheme(const UiTheme& theme) noexcept;

    [[nodiscard]] ThemePreset GetShellThemePreset() const noexcept { return m_shellTheme.preset; }
    void SetShellThemePreset(ThemePreset preset) noexcept;

    [[nodiscard]] keel::Optional<ThemeMode> QuerySystemTheme() const noexcept;

    [[nodiscard]] bool IsShellLightMode() const noexcept { return m_shellLightMode; }
    void SetShellLightMode(bool lightMode) noexcept;

    [[nodiscard]] bool IsShellFloating() const noexcept { return m_shellFloating; }
    void SetShellFloating(bool floating) noexcept;

    /// Shell UI scale is always one of `kScaleValues`.
    [[nodiscard]] keel::f32 GetShellScale() const noexcept { return m_shellScale; }
    /// Snaps `scale` to the nearest `kScaleValues` preset.
    void SetShellScale(keel::f32 scale) noexcept;
    [[nodiscard]] keel::usize GetShellScaleIndex() const noexcept { return m_shellScaleIndex; }
    void SetShellScaleIndex(keel::usize index) noexcept;
    /// Moves `steps` presets up (positive) or down (negative), clamped to the preset range.
    void StepShellScale(keel::i32 steps) noexcept;

    static constexpr keel::StaticArray<keel::f32, 7> kScaleValues = {{0.75f, 0.9f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f}};
    static constexpr keel::StaticArray<keel::StringRef, 7> kScaleLabels = {{
        "75%",
        "90%",
        "100%",
        "125%",
        "150%",
        "175%",
        "200%"
    }};

    static constexpr keel::StaticArray<keel::i32, 5> kHeartbeatMsValues = {{-1, 250, 500, 1000, 2000}};
    static constexpr keel::StaticArray<keel::StringRef, 5> kHeartbeatLabels = {{
        "Heartbeat: Off",
        "Heartbeat: 4 Hz",
        "Heartbeat: 2 Hz",
        "Heartbeat: 1 Hz",
        "Heartbeat: 0.5 Hz"
    }};

protected:
    /// Called once after the `IPlatformWindow`, `ICanvas`, and `Context` have been initialized.
    virtual void OnInit() {}

    /// Called every frame to construct the immediate-mode UI tree.
    virtual void OnRender(Context& ui);

    /// Called when the window pixel dimensions change.
    virtual void OnResize(keel::i32 width, keel::i32 height) {
        (void)width;
        (void)height;
    }

    /// Called when the user or OS requests the window to close.
    /// Return `true` to allow closing, or `false` to veto and keep the window open.
    virtual bool OnCloseRequested() { return true; }

    /// Called once before the window and GPU resources are destroyed.
    virtual void OnShutdown() {}

private:
    void PrepareFrameTarget();
    void PresentFrame(bool forcePresent);
    void PaceFrame(keel::f64 elapsedMs, bool presented) noexcept;
    void RenderFrame();

    MainWindowConfig m_config;
    keel::UniquePtr<IPlatformWindow> m_window;
    PlatformCanvasFactory m_canvasFactory;
    keel::UniquePtr<ICanvas> m_canvas;
    GraphicsApi m_graphicsApi{GraphicsApi::None};
    bool m_hasExplicitWindow{false};
    RenderCallback m_renderCallback;

    Context m_ui;
    FrameMetrics m_metrics;

    keel::Array<keel::u32> m_cpuFramebuffer;
    keel::u32 m_fbWidth{0};
    keel::u32 m_fbHeight{0};

    UiTheme m_shellTheme = UiTheme::Dark();
    bool m_shellLightMode = false;
    bool m_shellFloating = false;
    keel::f32 m_shellScale = 1.0f;
    keel::usize m_shellScaleIndex = 2; // kScaleValues[2] == 1.0f

    bool m_initialized = false;
    bool m_running = false;
    bool m_reactiveMode = true;
    bool m_gpuShadersEnabled = true;
    bool m_inRenderFrame = false;
};

/// Cooperative multi-window event & render loop: initializes all provided `MainWindow` instances
/// and steps them until every window has closed.
keel::i32 RunWindows(keel::ArrayRef<MainWindow*> windows);

} // namespace guikit
