#pragma once

#include "guikit/backend/i_platform_window.hpp"
#include "guikit/backend/i_window_backend.hpp"
#include "keel/array.h"
#include "keel/function.h"
#include "keel/smart_ptr.h"

namespace guikit {

/// Factory callback that instantiates an `ICanvas` bound to an already-created `IPlatformWindow`
/// (e.g. constructing a `GlEs3Canvas` after the native GL/EGL context is current).
using PlatformCanvasFactory = keel::Function<keel::UniquePtr<ICanvas>(IPlatformWindow& window)>;

/// Reusable `IWindowBackend` adapter composing any `IPlatformWindow` (X11, Wayland, Win32, Headless)
/// with any `ICanvas` renderer (`GlEs3Canvas`, `ThorCanvas`, `NullCanvas`, or custom).
///
/// Eliminates duplication of canvas lifecycle, CPU framebuffer management, frame pacing,
/// telemetry timing, and `MainWindowConfig` bridging across platform window backends.
class PlatformWindowBackend : public IWindowBackend {
public:
    PlatformWindowBackend(
        keel::UniquePtr<IPlatformWindow> window,
        PlatformCanvasFactory canvasFactory = {},
        GraphicsApi graphicsApi = GraphicsApi::OpenGLES3
    ) noexcept;

    PlatformWindowBackend(
        keel::UniquePtr<IPlatformWindow> window,
        keel::UniquePtr<ICanvas> canvas,
        GraphicsApi graphicsApi = GraphicsApi::None
    ) noexcept;

    ~PlatformWindowBackend() override;

    bool Initialize(const MainWindowConfig& config, Context& ui, WindowHostCallbacks callbacks) override;
    void Shutdown() noexcept override;

    bool PumpEvents(Context& ui, bool reactiveMode, keel::i32 sleepTimeoutMs) override;
    void WakeUp() noexcept override;
    [[nodiscard]] bool ShouldRenderFrame() const noexcept override;

    [[nodiscard]] bool UsesMainLoopCallback() const noexcept override;
    void RunMainLoopCallback(keel::Function<bool()> stepFn) override;

    [[nodiscard]] ICanvas& GetCanvas() noexcept override;
    [[nodiscard]] const ICanvas& GetCanvas() const noexcept override;

    void PrepareFrameTarget(Context& ui) override;
    void PresentFrame(const Context& ui, FrameMetrics& metrics, bool forcePresent) override;
    void PaceFrame(keel::f64 elapsedMs, bool presented) noexcept override;

    [[nodiscard]] keel::i32 GetWidth() const noexcept override;
    [[nodiscard]] keel::i32 GetHeight() const noexcept override;
    [[nodiscard]] keel::f32 GetDpiScale() const noexcept override;
    [[nodiscard]] keel::f32 GetDisplayPixelsPerMm() const noexcept override;
    [[nodiscard]] bool IsMinimized() const noexcept override;
    [[nodiscard]] bool IsLiveResizeFrame() const noexcept override;

    [[nodiscard]] bool IsVsyncEnabled() const noexcept override;
    void SetVsyncEnabled(bool enabled) noexcept override;

    [[nodiscard]] bool IsGpuShadersEnabled() const noexcept override;
    [[nodiscard]] bool IsGpuShadersAvailable() const noexcept override;
    void SetGpuShadersEnabled(bool enabled) noexcept override;

    void UpdateSystemCursor(CursorType cursor) noexcept override;
    void SetTextInputActive(bool active, Rect caretRectPx) noexcept override;
    void OpenUrl(keel::StringRef url) const noexcept override;
    [[nodiscard]] keel::Optional<ThemeMode> QuerySystemTheme() const noexcept override;

    [[nodiscard]] IPlatformWindow* GetPlatformWindow() noexcept { return m_window.get(); }
    [[nodiscard]] const IPlatformWindow* GetPlatformWindow() const noexcept { return m_window.get(); }

private:
    keel::UniquePtr<IPlatformWindow> m_window;
    PlatformCanvasFactory m_canvasFactory;
    keel::UniquePtr<ICanvas> m_canvas;
    GraphicsApi m_graphicsApi{GraphicsApi::OpenGLES3};

    keel::Array<keel::u32> m_cpuFramebuffer;
    keel::u32 m_fbWidth{0};
    keel::u32 m_fbHeight{0};

    keel::f32 m_targetFps{60.0f};
    bool m_gpuShadersEnabled{true};
    bool m_initialized{false};
};

} // namespace guikit
