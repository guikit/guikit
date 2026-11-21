#include "guikit/backend/platform_window_backend.hpp"

#include "guikit/backend/null_canvas.hpp"
#include "guikit/context.hpp"
#include "keel/math.h"
#include "keel/time.h"

namespace guikit {

PlatformWindowBackend::PlatformWindowBackend(
    keel::UniquePtr<IPlatformWindow> window,
    PlatformCanvasFactory canvasFactory,
    GraphicsApi graphicsApi
) noexcept
    : m_window(keel::move(window))
    , m_canvasFactory(keel::move(canvasFactory))
    , m_graphicsApi(graphicsApi) {
}

PlatformWindowBackend::PlatformWindowBackend(
    keel::UniquePtr<IPlatformWindow> window,
    keel::UniquePtr<ICanvas> canvas,
    GraphicsApi graphicsApi
) noexcept
    : m_window(keel::move(window))
    , m_canvas(keel::move(canvas))
    , m_graphicsApi(graphicsApi) {
}

PlatformWindowBackend::~PlatformWindowBackend() {
    Shutdown();
}

bool PlatformWindowBackend::Initialize(
    const MainWindowConfig& config,
    Context& ui,
    WindowHostCallbacks callbacks
) {
    if (!m_window) {
        return false;
    }

    m_targetFps = (config.targetFps > 1.0f) ? config.targetFps : 60.0f;
    m_gpuShadersEnabled = config.gpuShaders;

    PlatformWindowDesc desc{};
    desc.title = config.title;
    desc.width = config.width;
    desc.height = config.height;
    desc.minWidth = config.minWidth;
    desc.minHeight = config.minHeight;
    desc.resizable = config.resizable;
    desc.maximized = config.maximized;
    desc.fullscreen = config.fullscreen;
    desc.borderless = config.borderless;
    desc.highDpi = config.highDpi;
    desc.vsync = config.vsync;
    desc.graphicsApi = m_graphicsApi;

    if (!m_window->Create(desc, ui, keel::move(callbacks))) {
        return false;
    }

    if (!m_window->MakeContextCurrent()) {
        m_window->Destroy();
        return false;
    }

    if (!m_canvas) {
        if (m_canvasFactory) {
            m_canvas = m_canvasFactory(*m_window);
        } else if (config.canvasFactory != nullptr) {
            m_canvas = config.canvasFactory(true);
        } else {
            m_canvas = CreateVectorCanvas(true);
        }
    }
    if (!m_canvas) {
        m_canvas = keel::make_unique<NullCanvas>();
    }

    m_canvas->SetGpuShadersEnabled(m_gpuShadersEnabled);
    if (!m_canvas->UsesCpuFramebuffer()) {
        // Double/triple-buffered GPU swapchains do not preserve backbuffer contents across SwapBuffers()
        ui.SetDirtyRectEnabled(false);
    }
    m_initialized = true;
    return true;
}

void PlatformWindowBackend::Shutdown() noexcept {
    if (!m_initialized) {
        return;
    }
    m_initialized = false;
    m_cpuFramebuffer.clear();
    m_fbWidth = 0;
    m_fbHeight = 0;
    if (m_window) {
        (void)m_window->MakeContextCurrent();
    }
    m_canvas.reset();
    if (m_window) {
        m_window->Destroy();
    }
}

bool PlatformWindowBackend::PumpEvents(Context& ui, bool reactiveMode, keel::i32 sleepTimeoutMs) {
    if (!m_window) {
        return false;
    }
    const bool minimized = m_window->IsMinimized();
    const bool effectiveReactive = reactiveMode || minimized;
    const keel::i32 effectiveTimeout = minimized
        ? ((sleepTimeoutMs < 0) ? sleepTimeoutMs : keel::max(sleepTimeoutMs, 250))
        : sleepTimeoutMs;
    return m_window->PumpEvents(ui, effectiveReactive, effectiveTimeout);
}

void PlatformWindowBackend::WakeUp() noexcept {
    if (m_window) {
        m_window->WakeUp();
    }
}

bool PlatformWindowBackend::ShouldRenderFrame() const noexcept {
    return m_initialized && m_window && !m_window->IsMinimized();
}

bool PlatformWindowBackend::UsesMainLoopCallback() const noexcept {
    return m_window ? m_window->UsesMainLoopCallback() : false;
}

void PlatformWindowBackend::RunMainLoopCallback(keel::Function<bool()> stepFn) {
    if (m_window) {
        m_window->RunMainLoopCallback(keel::move(stepFn));
    }
}

ICanvas& PlatformWindowBackend::GetCanvas() noexcept {
    if (!m_canvas) {
        m_canvas = keel::make_unique<NullCanvas>();
    }
    return *m_canvas;
}

const ICanvas& PlatformWindowBackend::GetCanvas() const noexcept {
    return const_cast<PlatformWindowBackend*>(this)->GetCanvas();
}

void PlatformWindowBackend::PrepareFrameTarget(Context& /*ui*/) {
    if (!m_window || !m_canvas) {
        return;
    }
    (void)m_window->MakeContextCurrent();
    if (!m_canvas->UsesCpuFramebuffer()) {
        return;
    }

    const keel::i32 w = m_window->GetFramebufferWidth();
    const keel::i32 h = m_window->GetFramebufferHeight();
    if (w <= 0 || h <= 0) {
        return;
    }
    const auto uw = static_cast<keel::u32>(w);
    const auto uh = static_cast<keel::u32>(h);
    if (uw != m_fbWidth || uh != m_fbHeight || m_cpuFramebuffer.size() != static_cast<keel::usize>(uw) * uh) {
        m_fbWidth = uw;
        m_fbHeight = uh;
        m_cpuFramebuffer.resize(static_cast<keel::usize>(uw) * uh, 0u);
    }
    m_canvas->SetTarget(m_cpuFramebuffer.data(), m_fbWidth, m_fbHeight, m_fbWidth);
}

void PlatformWindowBackend::PresentFrame(const Context& ui, FrameMetrics& metrics, bool forcePresent) {
    if (!m_window || !m_canvas) {
        return;
    }
    if (!forcePresent && !ui.HasDirtyDamage() && !m_canvas->HasActiveGpuPasses()) {
        return;
    }

    metrics.MarkGpuStart();
    if (m_canvas->UsesCpuFramebuffer()) {
        if (!m_cpuFramebuffer.empty() && m_fbWidth > 0 && m_fbHeight > 0) {
            const keel::Optional<Rect> dirty = ui.IsFullRepaint()
                ? keel::Optional<Rect>{keel::kNullOpt}
                : keel::Optional<Rect>{ui.GetPhysicalDirtyRect()};
            m_window->PresentCpuFramebuffer(
                m_cpuFramebuffer.data(),
                m_fbWidth,
                m_fbHeight,
                m_fbWidth,
                m_canvas->GetPixelFormat(),
                dirty
            );
        }
    } else {
        m_window->SwapBuffers();
    }
    metrics.MarkGpuDone();
    metrics.MarkFramePresented();
}

void PlatformWindowBackend::PaceFrame(double elapsedMs, bool presented) noexcept {
    if (!presented && IsVsyncEnabled()) {
        return;
    }
    if (IsVsyncEnabled()) {
        return;
    }
    const double targetMs = 1000.0 / static_cast<double>(m_targetFps);
    const double remainingMs = targetMs - elapsedMs;
    if (remainingMs > 1.0) {
        keel::sleep_us(static_cast<keel::u64>((remainingMs - 0.5) * 1000.0));
    }
}

keel::i32 PlatformWindowBackend::GetWidth() const noexcept {
    return m_window ? m_window->GetFramebufferWidth() : 0;
}

keel::i32 PlatformWindowBackend::GetHeight() const noexcept {
    return m_window ? m_window->GetFramebufferHeight() : 0;
}

float PlatformWindowBackend::GetDpiScale() const noexcept {
    return m_window ? m_window->GetDpiScale() : 1.0f;
}

float PlatformWindowBackend::GetDisplayPixelsPerMm() const noexcept {
    return m_window ? m_window->GetDisplayPixelsPerMm() : (96.0f / 25.4f);
}

bool PlatformWindowBackend::IsMinimized() const noexcept {
    return m_window ? m_window->IsMinimized() : false;
}

bool PlatformWindowBackend::IsLiveResizeFrame() const noexcept {
    return m_window ? m_window->IsLiveResizeFrame() : false;
}

bool PlatformWindowBackend::IsVsyncEnabled() const noexcept {
    return m_window ? m_window->IsVsyncEnabled() : true;
}

void PlatformWindowBackend::SetVsyncEnabled(bool enabled) noexcept {
    if (m_window) {
        m_window->SetVsyncEnabled(enabled);
    }
}

bool PlatformWindowBackend::IsGpuShadersEnabled() const noexcept {
    return m_gpuShadersEnabled && IsGpuShadersAvailable();
}

bool PlatformWindowBackend::IsGpuShadersAvailable() const noexcept {
    return m_canvas ? m_canvas->SupportsGpuShaders() : false;
}

void PlatformWindowBackend::SetGpuShadersEnabled(bool enabled) noexcept {
    m_gpuShadersEnabled = enabled;
    if (m_canvas) {
        m_canvas->SetGpuShadersEnabled(enabled);
    }
}

void PlatformWindowBackend::UpdateSystemCursor(CursorType cursor) noexcept {
    if (m_window) {
        m_window->SetCursor(cursor);
    }
}

void PlatformWindowBackend::OpenUrl(keel::StringRef url) const noexcept {
    if (m_window) {
        m_window->OpenUrl(url);
    }
}

} // namespace guikit
