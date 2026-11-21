#pragma once

#include "guikit/backend/i_canvas.hpp"
#include "guikit/backend/vector_backend.hpp"
#include "guikit/core/frame_metrics.hpp"
#include "guikit/core/theme.hpp"
#include "keel/function.h"
#include "keel/smart_ptr.h"
#include "keel/string.h"

namespace guikit {

class Context;

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
    keel::f32 targetFps = 60.0f;    ///< Target frame rate for telemetry budget and non-VSync frame pacing
    keel::f32 initialScale = 1.0f;  ///< Initial shell UI scale, snapped to the nearest `MainWindow::kScaleValues` preset
    ThemeMode initialThemeMode = ThemeMode::Dark;
    keel::f32 initialDensity = 1.0f; ///< Initial layout gap/padding density multiplier (1.0f = standard)
    ShellStyle initialShellStyle = ShellStyle::Docked;
    keel::Optional<UiTheme> initialTheme = keel::kNullOpt; ///< Optional complete initial theme override
    TextRenderSettings textRender{};                       ///< Initial text anti-aliasing, hinting, gamma & contrast settings
    VectorCanvasFactory canvasFactory = nullptr; ///< Optional per-window custom `ICanvas` factory override
};

/// Callbacks invoked by an `IWindowBackend` into its hosting `MainWindow`.
struct WindowHostCallbacks {
    keel::Function<bool()> onCloseRequested;
    keel::Function<void(keel::i32 width, keel::i32 height)> onResize;
    keel::Function<void()> onLiveResizeRender;
};

/// Abstract platform windowing and presentation backend for `guikit::MainWindow`.
/// Decouples `MainWindow` (in `guikit::core`) from concrete OS/GPU libraries (SDL3, Win32, GLFW, WebGL, Vulkan, etc.).
class IWindowBackend {
public:
    virtual ~IWindowBackend() = default;

    /// Creates the platform window, graphics context, and binds clipboard callbacks to `ui`.
    virtual bool Initialize(const MainWindowConfig& config, Context& ui, WindowHostCallbacks callbacks) = 0;

    /// Releases all OS window, texture, cursor, and GPU resources.
    virtual void Shutdown() noexcept = 0;

    /// Polls or sleeps for platform events and dispatches input into `ui`.
    /// Returns `false` if the main loop should exit.
    virtual bool PumpEvents(Context& ui, bool reactiveMode, keel::i32 sleepTimeoutMs) = 0;

    /// Thread-safe wakeup signal that interrupts `PumpEvents()` idle sleep immediately when background work arrives.
    virtual void WakeUp() noexcept {}

    /// Returns `true` if the platform requires an inverted callback main loop (e.g. WebAssembly / Emscripten).
    [[nodiscard]] virtual bool UsesMainLoopCallback() const noexcept { return false; }

    /// Starts an inverted platform main loop, invoking `stepFn()` each frame until it returns `false`.
    virtual void RunMainLoopCallback(keel::Function<bool()> stepFn) { (void)stepFn; }

    /// Returns the active 2D rendering canvas owned by this window backend.
    [[nodiscard]] virtual ICanvas& GetCanvas() noexcept = 0;
    [[nodiscard]] virtual const ICanvas& GetCanvas() const noexcept = 0;

    /// Ensures the offscreen/streaming frame target matches the current window dimensions before `ui.BeginFrame()`.
    virtual void PrepareFrameTarget(Context& ui) = 0;

    /// Uploads dirty regions (or full frame), renders GPU shader underlays, and presents the swapchain.
    virtual void PresentFrame(const Context& ui, FrameMetrics& metrics, bool forcePresent) = 0;

    /// Sleeps briefly when frame pacing is needed (e.g. non-reactive mode with VSync disabled).
    virtual void PaceFrame(keel::f64 elapsedMs, bool presented) noexcept {
        (void)elapsedMs;
        (void)presented;
    }

    [[nodiscard]] virtual keel::i32 GetWidth() const noexcept = 0;
    [[nodiscard]] virtual keel::i32 GetHeight() const noexcept = 0;
    [[nodiscard]] virtual keel::f32 GetDpiScale() const noexcept { return 1.0f; }
    [[nodiscard]] virtual keel::f32 GetDisplayPixelsPerMm() const noexcept = 0;
    [[nodiscard]] virtual bool IsMinimized() const noexcept = 0;
    [[nodiscard]] virtual bool ShouldRenderFrame() const noexcept { return !IsMinimized(); }
    [[nodiscard]] virtual bool IsLiveResizeFrame() const noexcept = 0;

    [[nodiscard]] virtual bool IsVsyncEnabled() const noexcept = 0;
    virtual void SetVsyncEnabled(bool enabled) noexcept = 0;

    [[nodiscard]] virtual bool IsGpuShadersEnabled() const noexcept = 0;
    [[nodiscard]] virtual bool IsGpuShadersAvailable() const noexcept = 0;
    virtual void SetGpuShadersEnabled(bool enabled) noexcept = 0;

    virtual void UpdateSystemCursor(CursorType cursor) noexcept = 0;
    virtual void OpenUrl(keel::StringRef url) const noexcept = 0;
};

using WindowBackendFactory = keel::UniquePtr<IWindowBackend>(*)();

/// Registers the default platform `IWindowBackend` factory used when `MainWindow` is constructed without an explicit backend.
void RegisterDefaultWindowBackendFactory(WindowBackendFactory factory) noexcept;

/// Instantiates the registered default `IWindowBackend` (or returns `nullptr` if no window backend is registered).
[[nodiscard]] keel::UniquePtr<IWindowBackend> CreateDefaultWindowBackend();

} // namespace guikit
