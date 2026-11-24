#pragma once

#include "guikit/backend/i_canvas.hpp"
#include "guikit/backend/system_theme.hpp"
#include "keel/array.h"
#include "keel/function.h"
#include "keel/smart_ptr.h"
#include "keel/string.h"

namespace guikit {

class Context;

/// Graphics context API requested from a native platform window surface.
enum class GraphicsApi : keel::u8 {
    None = 0,       ///< Headless, SDL renderer, or CPU framebuffer presentation (no raw GL context required)
    OpenGLES3 = 1,  ///< OpenGL ES 3.0 / WebGL 2.0 context (EGL on Linux/X11/Wayland, WebGL2 on Emscripten)
    OpenGL3 = 2     ///< Desktop OpenGL 3.3+ Core profile context (WGL on Win32)
};

/// Callbacks invoked by an `IPlatformWindow` into its hosting `MainWindow`.
struct WindowHostCallbacks {
    keel::Function<bool()> onCloseRequested;
    keel::Function<void(keel::i32 width, keel::i32 height)> onResize;
    keel::Function<void()> onLiveResizeRender;
};

/// Platform-neutral window creation descriptor passed to `IPlatformWindow::Create(...)`.
struct PlatformWindowDesc {
    keel::FixedString<128> title{"GuiKit Application"};
    keel::i32 width{1380};
    keel::i32 height{800};
    keel::i32 minWidth{320};
    keel::i32 minHeight{240};
    bool resizable{true};
    bool maximized{false};
    bool fullscreen{false};
    bool borderless{false};
    bool highDpi{true};
    bool vsync{true};
    bool gpuShaders{true};
    bool debugShortcuts{true};
    GraphicsApi graphicsApi{GraphicsApi::OpenGLES3};
};

/// Function pointer signature for resolving OpenGL / OpenGL ES entry points from a live context.
using GlProcAddressFn = void*(*)(const char* procName);

/// Abstract OS window + native graphics surface contract (SDL3, Win32, X11, Wayland, Emscripten, Headless).
///
/// Single unified platform windowing abstraction across the entire GuiKit ecosystem:
/// decouples OS window lifecycle, input/IME/file-drop dispatch, and swapchain/framebuffer presentation
/// from the 2D rendering canvas (`ICanvas`, e.g. `GlEs3Canvas`, `ThorCanvas`, `SdlCanvas`, `NullCanvas`).
class IPlatformWindow {
public:
    virtual ~IPlatformWindow() = default;

    /// Creates the OS window (and optional GL/ES3 or renderer context) and binds platform callbacks (e.g. clipboard) to `ui`.
    virtual bool Create(const PlatformWindowDesc& desc, Context& ui, WindowHostCallbacks callbacks) = 0;

    /// Releases the native window, graphics context, cursors, and OS handles.
    virtual void Destroy() noexcept = 0;

    /// Polls or sleeps (up to `sleepTimeoutMs`, `-1` for indefinite) for OS events and feeds them into `ui`.
    /// Returns `false` when the window has been closed or the main loop should exit.
    virtual bool PumpEvents(Context& ui, bool reactiveMode, keel::i32 sleepTimeoutMs) = 0;

    /// Thread-safe wakeup that interrupts an idle wait in `PumpEvents()`.
    virtual void WakeUp() noexcept {}

    /// Returns `true` if the host platform requires an inverted callback main loop (e.g. WebAssembly).
    [[nodiscard]] virtual bool UsesMainLoopCallback() const noexcept { return false; }

    /// Starts an inverted main loop, calling `stepFn()` per frame until it returns `false`.
    virtual void RunMainLoopCallback(keel::Function<bool()> stepFn) { (void)stepFn; }

    /// Makes the window's native GL/ES3 context current on the calling thread (no-op for `GraphicsApi::None`).
    virtual bool MakeContextCurrent() noexcept { return true; }

    /// Presents the rendered GPU backbuffer (EGL/WGL swapBuffers; no-op for headless/CPU surfaces).
    virtual void SwapBuffers() noexcept {}

    /// Returns the GL/ES3 procedure loader for the active context, or `nullptr` if no GL context is active.
    [[nodiscard]] virtual GlProcAddressFn GetGlProcAddressFn() const noexcept { return nullptr; }

    /// Optional factory hook allowing a platform window to instantiate its preferred default `ICanvas`
    /// once the native window/renderer is created (e.g. `SdlPlatformWindow` -> `ThorCanvas` or `SdlCanvas`).
    [[nodiscard]] virtual keel::UniquePtr<ICanvas> CreateDefaultCanvas() { return nullptr; }

    /// Optional hook called when an externally provided `ICanvas` is attached to this window
    /// (e.g. binding `SdlCanvas` to `SDL_Renderer`).
    virtual void AttachCanvas(ICanvas& /*canvas*/) noexcept {}

    /// Optional hook called before `ui.BeginFrame(...)` each frame (e.g. ticking shader caches or clearing renderer).
    virtual void BeginFrameTarget(ICanvas& /*canvas*/, Context& /*ui*/) {}

    /// Optional presentation hook for CPU-framebuffer canvases (`ICanvas::UsesCpuFramebuffer() == true`).
    virtual void PresentCpuFramebuffer(
        const keel::u32* /*pixels*/,
        keel::u32 /*width*/,
        keel::u32 /*height*/,
        keel::u32 /*stride*/,
        SurfacePixelFormat /*format*/,
        keel::ArrayRef<Rect> /*dirtyRects*/,
        bool /*fullRepaint*/,
        ICanvas& /*canvas*/
    ) noexcept {}

    /// Optional presentation hook for direct GPU canvases (`ICanvas::UsesCpuFramebuffer() == false`).
    /// Defaults to calling `SwapBuffers()`.
    virtual void PresentGpuFrame(ICanvas& /*canvas*/) noexcept {
        SwapBuffers();
    }

    /// Returns true if this window hosts a dedicated GPU shader underlay engine (e.g. SDL3 GL shader underlays).
    [[nodiscard]] virtual bool SupportsUnderlayShaders() const noexcept { return false; }

    /// Enables or disables this window's dedicated GPU shader underlay engine.
    virtual void SetUnderlayShadersEnabled(bool /*enabled*/) noexcept {}

    [[nodiscard]] virtual keel::i32 GetFramebufferWidth() const noexcept = 0;
    [[nodiscard]] virtual keel::i32 GetFramebufferHeight() const noexcept = 0;
    [[nodiscard]] virtual keel::f32 GetDpiScale() const noexcept { return 1.0f; }
    [[nodiscard]] virtual keel::f32 GetDisplayPixelsPerMm() const noexcept {
        return (96.0f * GetDpiScale()) / 25.4f;
    }
    [[nodiscard]] virtual bool IsMinimized() const noexcept { return false; }
    [[nodiscard]] virtual bool ShouldRenderFrame() const noexcept { return !IsMinimized(); }
    [[nodiscard]] virtual bool IsLiveResizeFrame() const noexcept { return false; }

    [[nodiscard]] virtual bool IsVsyncEnabled() const noexcept { return true; }
    virtual void SetVsyncEnabled(bool /*enabled*/) noexcept {}

    virtual void SetCursor(CursorType /*cursor*/) noexcept {}
    virtual void SetTextInputActive(bool /*active*/, Rect /*caretRectPx*/) noexcept {}
    virtual void OpenUrl(keel::StringRef /*url*/) const noexcept {}

    /// Queries the OS/browser preferred color scheme (`ThemeMode::Dark` or `ThemeMode::Light`),
    /// or `keel::kNullOpt` if unavailable or unknown.
    [[nodiscard]] virtual keel::Optional<ThemeMode> QuerySystemTheme() const noexcept { return QueryOsSystemTheme(); }

    /// Human-readable platform identifier (e.g. "SDL3", "Win32 + WGL", "X11 + EGL", "Headless").
    [[nodiscard]] virtual keel::StringRef GetPlatformName() const noexcept = 0;
};

/// Factory callback that instantiates an `ICanvas` bound to an already-created `IPlatformWindow`
/// (e.g. constructing a `GlEs3Canvas` after the native GL/EGL context is current).
using PlatformCanvasFactory = keel::Function<keel::UniquePtr<ICanvas>(IPlatformWindow& window)>;

/// Default platform registration descriptor populated by a windowing module (`guikit::sdl` or `guikit::native`).
struct PlatformRegistration {
    keel::UniquePtr<IPlatformWindow> (*createWindow)() = nullptr;
    keel::UniquePtr<ICanvas> (*createCanvas)(IPlatformWindow& window) = nullptr;
    GraphicsApi graphicsApi{GraphicsApi::None};
};

/// Registers the default platform window & canvas factory used by `MainWindow` when no explicit window is injected.
void RegisterDefaultPlatform(PlatformRegistration reg) noexcept;

/// Returns the currently registered default platform descriptor.
[[nodiscard]] PlatformRegistration GetDefaultPlatform() noexcept;

} // namespace guikit
