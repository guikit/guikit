#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/backend/vector_backend.hpp"
#include "guikit/core/frame_metrics.hpp"
#include "guikit/core/theme.hpp"

namespace guikit {

class Context;

/// Configuration options for initializing a `guikit::MainWindow` application host.
struct MainWindowConfig {
    std::string title = "GuiKit Application";
    int width = 1380;
    int height = 800;
    int minWidth = 320;
    int minHeight = 240;
    bool resizable = true;
    bool highDpi = true;
    bool vsync = true;
    bool reactiveMode = true;
    bool gpuShaders = true;
    bool debugShortcuts = true; ///< Enables Ctrl+I / Ctrl+Shift+C (Inspect) and Ctrl+B (Bounds)
    bool scaleShortcuts = true; ///< Enables Ctrl+Plus / Ctrl+Minus / Ctrl+0 UI shell scaling
    bool autoShellScope = true; ///< Wraps OnRender() in a root Column with shellTheme & shellScale
    int idleHeartbeatMs = 500;  ///< Periodic wakeup interval in reactive mode (-1 to disable)
    ThemeMode initialThemeMode = ThemeMode::Dark;
    ShellStyle initialShellStyle = ShellStyle::Docked;
    VectorCanvasFactory canvasFactory = nullptr; ///< Optional per-window custom `ICanvas` factory override
};

/// Callbacks invoked by an `IWindowBackend` into its hosting `MainWindow`.
struct WindowHostCallbacks {
    std::function<bool()> onCloseRequested;
    std::function<void(int width, int height)> onResize;
    std::function<void()> onLiveResizeRender;
};

/// Abstract platform windowing and presentation backend for `guikit::MainWindow`.
/// Decouples `MainWindow` (in `guikit::core`) from concrete OS/GPU libraries (SDL3, Win32, GLFW, WebGL, Vulkan, etc.).
class IWindowBackend {
public:
    virtual ~IWindowBackend() = default;

    /// Creates the platform window, graphics context, and binds clipboard callbacks to `ui`.
    virtual bool Initialize(const MainWindowConfig& config, Context& ui, const WindowHostCallbacks& callbacks) = 0;

    /// Releases all OS window, texture, cursor, and GPU resources.
    virtual void Shutdown() noexcept = 0;

    /// Polls or sleeps for platform events and dispatches input into `ui`.
    /// Returns `false` if the main loop should exit.
    virtual bool PumpEvents(Context& ui, bool reactiveMode, int sleepTimeoutMs) = 0;

    /// Returns `true` if the platform requires an inverted callback main loop (e.g. WebAssembly / Emscripten).
    [[nodiscard]] virtual bool UsesMainLoopCallback() const noexcept { return false; }

    /// Starts an inverted platform main loop, invoking `stepFn()` each frame until it returns `false`.
    virtual void RunMainLoopCallback(std::function<bool()> stepFn) { (void)stepFn; }

    /// Returns the active 2D rendering canvas owned by this window backend.
    [[nodiscard]] virtual ICanvas& GetCanvas() noexcept = 0;
    [[nodiscard]] virtual const ICanvas& GetCanvas() const noexcept = 0;

    /// Ensures the offscreen/streaming frame target matches the current window dimensions before `ui.BeginFrame()`.
    virtual void PrepareFrameTarget(Context& ui) = 0;

    /// Uploads dirty regions (or full frame), renders GPU shader underlays, and presents the swapchain.
    virtual void PresentFrame(const Context& ui, FrameMetrics& metrics, bool forcePresent) = 0;

    /// Sleeps briefly when frame pacing is needed (e.g. non-reactive mode with VSync disabled).
    virtual void PaceFrame(double elapsedMs, bool presented) noexcept {
        (void)elapsedMs;
        (void)presented;
    }

    [[nodiscard]] virtual int GetWidth() const noexcept = 0;
    [[nodiscard]] virtual int GetHeight() const noexcept = 0;
    [[nodiscard]] virtual float GetDisplayPixelsPerMm() const noexcept = 0;
    [[nodiscard]] virtual bool IsMinimized() const noexcept = 0;
    [[nodiscard]] virtual bool IsLiveResizeFrame() const noexcept = 0;

    [[nodiscard]] virtual bool IsVsyncEnabled() const noexcept = 0;
    virtual void SetVsyncEnabled(bool enabled) noexcept = 0;

    [[nodiscard]] virtual bool IsGpuShadersEnabled() const noexcept = 0;
    [[nodiscard]] virtual bool IsGpuShadersAvailable() const noexcept = 0;
    virtual void SetGpuShadersEnabled(bool enabled) noexcept = 0;

    virtual void UpdateSystemCursor(CursorType cursor) noexcept = 0;
    virtual void OpenUrl(std::string_view url) const noexcept = 0;
};

using WindowBackendFactory = std::unique_ptr<IWindowBackend>(*)();

/// Registers the default platform `IWindowBackend` factory used when `MainWindow` is constructed without an explicit backend.
void RegisterDefaultWindowBackendFactory(WindowBackendFactory factory) noexcept;

/// Instantiates the registered default `IWindowBackend` (or returns `nullptr` if no window backend is registered).
[[nodiscard]] std::unique_ptr<IWindowBackend> CreateDefaultWindowBackend();

} // namespace guikit
