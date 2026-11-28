#include "guikit/core/main_window.hpp"

#include "guikit/backend/null_canvas.hpp"
#include "guikit/layout/column.hpp"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/math.h"
#include "keel/time.h"

namespace guikit {

namespace {

PlatformRegistration g_defaultPlatform{};

} // namespace

void RegisterDefaultPlatform(PlatformRegistration reg) noexcept {
    g_defaultPlatform = reg;
}

PlatformRegistration GetDefaultPlatform() noexcept {
    return g_defaultPlatform;
}

MainWindow::MainWindow(
    MainWindowConfig config,
    keel::UniquePtr<IPlatformWindow> window,
    keel::UniquePtr<ICanvas> canvas,
    GraphicsApi graphicsApi
)
    : m_config(keel::move(config))
    , m_window(keel::move(window))
    , m_canvas(keel::move(canvas))
    , m_graphicsApi(graphicsApi)
    , m_hasExplicitWindow(m_window != nullptr)
    , m_shellTheme(m_config.initialTheme.has_value()
                   ? *m_config.initialTheme
                   : (m_config.initialThemePreset.has_value()
                      ? UiTheme::FromPreset(*m_config.initialThemePreset)
                      : (m_config.initialThemeMode == ThemeMode::Light
                         ? UiTheme::Light()
                         : UiTheme::Dark())))
    , m_shellLightMode(m_shellTheme.mode == ThemeMode::Light)
    , m_shellFloating(m_config.initialTheme.has_value()
                      ? (m_shellTheme.shellStyle == ShellStyle::Floating)
                      : (m_config.initialShellStyle == ShellStyle::Floating))
    , m_reactiveMode(m_config.reactiveMode)
    , m_gpuShadersEnabled(m_config.gpuShaders) {
    if (!m_config.initialTheme.has_value()) {
        m_shellTheme.SetShellStyle(m_config.initialShellStyle);
    }
    m_ui.SetDensity(m_config.initialDensity);
    m_ui.SetOpenUrlCallback([this](keel::StringRef url) { OpenUrl(url); });
    SetShellScale(m_config.initialScale);
}

MainWindow::MainWindow(
    MainWindowConfig config,
    keel::UniquePtr<IPlatformWindow> window,
    PlatformCanvasFactory canvasFactory,
    GraphicsApi graphicsApi
)
    : MainWindow(keel::move(config), keel::move(window), keel::UniquePtr<ICanvas>{}, graphicsApi) {
    m_canvasFactory = keel::move(canvasFactory);
}

MainWindow::~MainWindow() {
    Shutdown();
}

bool MainWindow::Initialize() {
    if (m_initialized) {
        return true;
    }

    PlatformRegistration reg = GetDefaultPlatform();
    if (!m_window) {
        if (reg.createWindow == nullptr) {
            keel::eprintln("guikit::MainWindow::Initialize() failed: no default IPlatformWindow registered or provided.");
            return false;
        }
        m_window = reg.createWindow();
        if (!m_window) {
            keel::eprintln("guikit::MainWindow::Initialize() failed: default platform window factory returned null.");
            return false;
        }
        if (!m_hasExplicitWindow) {
            m_graphicsApi = reg.graphicsApi;
            if (!m_canvas && !m_canvasFactory && reg.createCanvas != nullptr) {
                auto createFn = reg.createCanvas;
                m_canvasFactory = [createFn](IPlatformWindow& w) -> keel::UniquePtr<ICanvas> {
                    return createFn(w);
                };
            }
        }
    }

    m_ui.SetTheme(m_shellTheme);
    m_ui.SetClearColor(m_shellTheme.palette.background);
    m_ui.SetIdleHeartbeatMs(m_config.idleHeartbeatMs);

    WindowHostCallbacks hostCallbacks{
        .onCloseRequested = [this]() -> bool {
            if (OnCloseRequested()) {
                m_running = false;
                return true;
            }
            return false;
        },
        .onResize = [this](keel::i32 w, keel::i32 h) {
            OnResize(w, h);
        },
        .onLiveResizeRender = [this]() {
            RenderFrame();
        },
        .onHitTest = [this](Vec2 logicalPos) -> WindowHitRegion {
            const keel::f32 zoom = (m_ui.GetRootZoom() > 0.01f) ? m_ui.GetRootZoom() : 1.0f;
            return m_ui.ResolveWindowHitRegion({logicalPos.x / zoom, logicalPos.y / zoom});
        }
    };

    PlatformWindowDesc desc{};
    desc.title = m_config.title;
    desc.width = m_config.width;
    desc.height = m_config.height;
    desc.minWidth = m_config.minWidth;
    desc.minHeight = m_config.minHeight;
    desc.resizable = m_config.resizable;
    desc.maximized = m_config.maximized;
    desc.fullscreen = m_config.fullscreen;
    desc.borderless = m_config.borderless;
    desc.customTitleBar = m_config.customTitleBar;
    desc.highDpi = m_config.highDpi;
    desc.vsync = m_config.vsync;
    desc.gpuShaders = m_config.gpuShaders;
    desc.debugShortcuts = m_config.debugShortcuts;
    desc.graphicsApi = m_graphicsApi;

    if (!m_window->Create(desc, m_ui, keel::move(hostCallbacks))) {
        return false;
    }

    m_ui.SetWindowHostService(WindowHostService{
        .setTitle = [this](keel::StringRef title) { SetTitle(title); },
        .getTitle = [this]() -> keel::String { return keel::String::from(m_config.title.as_ref()); },
        .minimize = [this]() { Minimize(); },
        .maximize = [this]() { Maximize(); },
        .restore = [this]() { Restore(); },
        .requestClose = [this]() {
            if (m_window) {
                m_window->RequestClose();
            } else if (OnCloseRequested()) {
                m_running = false;
            }
        },
        .isMaximized = [this]() -> bool { return IsMaximized(); },
        .isFocused = [this]() -> bool { return IsFocused(); },
        .supportsCustomTitleBar = [this]() -> bool { return SupportsCustomTitleBar(); },
        .isCustomTitleBar = [this]() -> bool { return IsCustomTitleBar(); },
        .setCustomTitleBar = [this](bool enabled) -> bool { return SetCustomTitleBar(enabled); },
        .isResizable = [this]() -> bool { return m_config.resizable; },
        .startSystemMove = [this]() {
            if (m_window) {
                m_window->StartSystemMove();
            }
        },
        .startSystemResize = [this](WindowHitRegion edge) {
            if (m_window) {
                m_window->StartSystemResize(edge);
            }
        },
        .supportsFileDialogs = [this]() -> bool { return m_window && m_window->SupportsFileDialogs(); },
        .showFileDialog = [this](const FileDialogOptions& opts) -> FileDialogResult {
            return ShowFileDialog(opts);
        },
        .showFileDialogAsync = [this](const FileDialogOptions& opts, keel::Function<void(FileDialogResult)> onDone) {
            ShowFileDialogAsync(opts, keel::move(onDone));
        },
        .showMessageBox = [this](const MessageBoxOptions& opts) -> MessageBoxChoice {
            return ShowMessageBox(opts);
        }
    });

    if (!m_window->MakeContextCurrent()) {
        m_window->Destroy();
        return false;
    }

    if (!m_canvas) {
        if (m_canvasFactory) {
            m_canvas = m_canvasFactory(*m_window);
        } else if (m_config.canvasFactory != nullptr) {
            m_canvas = m_config.canvasFactory(true);
        } else if (auto defaultCanvas = m_window->CreateDefaultCanvas()) {
            m_canvas = keel::move(defaultCanvas);
        } else {
            m_canvas = CreateVectorCanvas(true);
        }
    }
    if (!m_canvas) {
        m_canvas = keel::make_unique<NullCanvas>();
    }

    m_window->AttachCanvas(*m_canvas);
    m_gpuShadersEnabled = m_config.gpuShaders;
    m_window->SetUnderlayShadersEnabled(m_gpuShadersEnabled);
    const bool effectiveGpuShaders = m_gpuShadersEnabled &&
        (m_window->SupportsUnderlayShaders() || m_canvas->SupportsGpuShaders());
    m_canvas->SetGpuShadersEnabled(effectiveGpuShaders || m_gpuShadersEnabled);

    if (!m_canvas->UsesCpuFramebuffer()) {
        // Double/triple-buffered GPU swapchains do not preserve backbuffer contents across SwapBuffers()
        m_ui.SetDirtyRectEnabled(false);
    }

    m_canvas->SetTextRenderSettings(m_config.textRender);

    if (m_config.followSystemTheme &&
        !m_config.initialTheme.has_value() &&
        !m_config.initialThemePreset.has_value()) {
        if (const keel::Optional<ThemeMode> sysTheme = QuerySystemTheme(); sysTheme.has_value()) {
            SetShellLightMode(*sysTheme == ThemeMode::Light);
        }
    }

    m_initialized = true;
    m_running = true;
    OnInit();

    // Synchronize m_shellTheme if OnInit() customized m_ui.GetTheme() directly
    const UiTheme& postInitTheme = m_ui.GetTheme();
    if (postInitTheme.preset != m_shellTheme.preset ||
        postInitTheme.mode != m_shellTheme.mode ||
        postInitTheme.shellStyle != m_shellTheme.shellStyle ||
        postInitTheme.palette.background != m_shellTheme.palette.background ||
        postInitTheme.palette.surface != m_shellTheme.palette.surface ||
        postInitTheme.palette.accent != m_shellTheme.palette.accent) {
        m_shellTheme = postInitTheme;
        m_shellLightMode = (m_shellTheme.mode == ThemeMode::Light);
        m_shellFloating = (m_shellTheme.shellStyle == ShellStyle::Floating);
    }
    SyncWindowFrameAppearance();
    return true;
}

void MainWindow::Shutdown() noexcept {
    if (!m_initialized) {
        return;
    }
    m_running = false;
    OnShutdown();
    m_ui.SetWindowHostService(WindowHostService{});
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

bool MainWindow::StepFrame() {
    if (!m_initialized || !m_running || !m_window) {
        return false;
    }
    const bool minimized = m_window->IsMinimized();
    const bool effectiveReactive = m_reactiveMode || minimized;
    const keel::i32 sleepTimeoutMs = m_ui.GetRecommendedSleepTimeoutMs();
    const keel::i32 effectiveTimeout = minimized
        ? ((sleepTimeoutMs < 0) ? sleepTimeoutMs : keel::max(sleepTimeoutMs, 250))
        : sleepTimeoutMs;

    if (!m_window->PumpEvents(m_ui, effectiveReactive, effectiveTimeout)) {
        m_running = false;
        return false;
    }
    if (!m_running) {
        return false;
    }
    if (m_window->ShouldRenderFrame()) {
        RenderFrame();
    }
    return m_running;
}

keel::i32 MainWindow::Run(RenderCallback renderCallback) {
    m_renderCallback = keel::move(renderCallback);
    return Run();
}

keel::i32 MainWindow::Run() {
    if (!Initialize()) {
        return 1;
    }

    if (m_window->UsesMainLoopCallback()) {
        m_window->RunMainLoopCallback([this]() -> bool {
            if (!StepFrame()) {
                Shutdown();
                return false;
            }
            return true;
        });
        return 0;
    }

    while (StepFrame()) {
    }

    Shutdown();
    return 0;
}

void MainWindow::OnRender(Context& ui) {
    if (m_renderCallback) {
        m_renderCallback(ui);
    }
}

void MainWindow::PrepareFrameTarget() {
    if (!m_window || !m_canvas) {
        return;
    }
    (void)m_window->MakeContextCurrent();
    m_window->BeginFrameTarget(*m_canvas, m_ui);

    if (!m_canvas->UsesCpuFramebuffer()) {
        return;
    }

    const keel::i32 w = m_window->GetFramebufferWidth();
    const keel::i32 h = m_window->GetFramebufferHeight();
    if (w <= 0 || h <= 0) {
        return;
    }
    const auto uw = keel::as<keel::u32>(w);
    const auto uh = keel::as<keel::u32>(h);
    if (uw != m_fbWidth || uh != m_fbHeight || m_cpuFramebuffer.size() != keel::as<keel::usize>(uw) * uh) {
        m_fbWidth = uw;
        m_fbHeight = uh;
        m_cpuFramebuffer.resize(keel::as<keel::usize>(uw) * uh, 0u);
        m_ui.InvalidateAll();
    }
    m_canvas->SetTarget(m_cpuFramebuffer.data(), m_fbWidth, m_fbHeight, m_fbWidth);
}

void MainWindow::PresentFrame(bool forcePresent) {
    if (!m_window || !m_canvas) {
        return;
    }
    if (!forcePresent && !m_ui.HasDirtyDamage() && !m_canvas->HasActiveGpuPasses()) {
        return;
    }

    m_metrics.MarkGpuStart();
    if (m_canvas->UsesCpuFramebuffer()) {
        if (!m_cpuFramebuffer.empty() && m_fbWidth > 0 && m_fbHeight > 0) {
            m_window->PresentCpuFramebuffer(
                m_cpuFramebuffer.data(),
                m_fbWidth,
                m_fbHeight,
                m_fbWidth,
                m_canvas->GetPixelFormat(),
                m_ui.GetDirtyRects(),
                m_ui.IsFullRepaint(),
                *m_canvas
            );
        }
        m_metrics.MarkGpuDone();
    } else {
        m_window->PresentGpuFrame(*m_canvas);
        m_metrics.MarkGpuDone(m_canvas->GetLastGpuFrameTimeMs());
    }
    m_metrics.MarkFramePresented();
}

void MainWindow::PaceFrame(keel::f64 elapsedMs, bool presented) noexcept {
#ifndef __EMSCRIPTEN__
    if (IsVsyncEnabled()) {
        return;
    }
    const keel::f64 targetFps = keel::max(1.0, keel::as<keel::f64>(m_config.targetFps));
    const keel::f64 targetMs = 1000.0 / targetFps;
    if (elapsedMs + 1.0 < targetMs) {
        const keel::f64 sleepMs = presented
            ? (targetMs - elapsedMs)
            : keel::min(4.0, targetMs - elapsedMs);
        if (sleepMs > 0.5) {
            keel::sleep_us(keel::as<keel::u64>((sleepMs - 0.5) * 1000.0));
        }
    }
#else
    (void)elapsedMs;
    (void)presented;
#endif
}

void MainWindow::RenderFrame() {
    if (m_inRenderFrame || !m_window || !m_canvas) {
        return;
    }
    const keel::i32 winW = m_window->GetFramebufferWidth();
    const keel::i32 winH = m_window->GetFramebufferHeight();
    if (winW <= 0 || winH <= 0) {
        return;
    }

    m_inRenderFrame = true;
    struct FrameGuard {
        bool& flag;
        ~FrameGuard() { flag = false; }
    } frameGuard{m_inRenderFrame};

    PrepareFrameTarget();

    const keel::u64 frameWorkStartNs = keel::monotonic_ns();
    m_metrics.BeginFrame();

    m_ui.BeginFrame({keel::as<keel::f32>(winW), keel::as<keel::f32>(winH)}, GetDpiScale(), GetShellScale());
    {
        const ShellStyle activeShellStyle = m_shellFloating ? ShellStyle::Floating : ShellStyle::Docked;
        if (m_shellTheme.GetShellStyle() != activeShellStyle) {
            m_shellTheme.SetShellStyle(activeShellStyle);
            m_ui.SetTheme(m_shellTheme);
        }

        if (m_config.autoShellScope) {
            if (auto mainCol = guikit::Column(m_ui, {})) {
                OnRender(m_ui);
            }
        } else {
            OnRender(m_ui);
        }

        if (m_config.scaleShortcuts) {
            if (m_ui.ConsumeAction(KeyAction::ZoomIn)) {
                StepShellScale(+1);
            } else if (m_ui.ConsumeAction(KeyAction::ZoomOut)) {
                StepShellScale(-1);
            } else if (m_ui.ConsumeAction(KeyAction::ZoomReset)) {
                SetShellScale(1.0f);
            }
        }
    }
    if (!IsDirtyRectAvailable() && m_ui.IsDirtyRectEnabled()) {
        m_ui.SetDirtyRectEnabled(false);
    }
    m_ui.EndFrame(*m_canvas);
    m_window->SetCursor(m_ui.GetCursor());
    {
        const keel::f32 pxPerDp = m_ui.GetPixelsPerDp();
        const Rect imeLogical = m_ui.GetImeTargetRect();
        const Rect imePx(
            imeLogical.x * pxPerDp,
            imeLogical.y * pxPerDp,
            imeLogical.w * pxPerDp,
            imeLogical.h * pxPerDp
        );
        m_window->SetTextInputActive(m_ui.WantsTextInput(), imePx);
    }
    m_metrics.MarkCpuDone();

    const bool shouldPresent = m_ui.HasDirtyDamage() || !m_reactiveMode;
    if (shouldPresent) {
        PresentFrame(!m_reactiveMode);
        if (IsVsyncEnabled() && !m_canvas->UsesCpuFramebuffer() && !m_window->IsLiveResizeFrame()) {
            m_window->WaitForVBlank();
        }
    }

    if (shouldPresent && !IsVsyncEnabled() && !m_reactiveMode) {
        const keel::u64 frameEndNs = keel::monotonic_ns();
        const keel::f64 frameElapsedMs = (frameEndNs > frameWorkStartNs)
            ? (keel::as<keel::f64>(frameEndNs - frameWorkStartNs) * 1e-6)
            : 0.0;
        PaceFrame(frameElapsedMs, true);
    }

    m_metrics.EndFrame(keel::max(1.0f, m_config.targetFps));

    if (!shouldPresent && m_ui.NeedsRepaint()) {
        const keel::u64 frameEndNs = keel::monotonic_ns();
        const keel::f64 frameElapsedMs = (frameEndNs > frameWorkStartNs)
            ? (keel::as<keel::f64>(frameEndNs - frameWorkStartNs) * 1e-6)
            : 0.0;
        PaceFrame(frameElapsedMs, false);
    }
}

void MainWindow::WakeUp() noexcept {
    if (m_window) {
        m_window->WakeUp();
    }
}

void MainWindow::OpenUrl(keel::StringRef url) const noexcept {
    if (m_window) {
        m_window->OpenUrl(url);
    }
}

ICanvas& MainWindow::GetCanvas() noexcept {
    if (!m_canvas) {
        m_canvas = keel::make_unique<NullCanvas>();
    }
    return *m_canvas;
}

const ICanvas& MainWindow::GetCanvas() const noexcept {
    return const_cast<MainWindow*>(this)->GetCanvas();
}

keel::i32 MainWindow::GetWidth() const noexcept {
    return m_window ? m_window->GetFramebufferWidth() : m_config.width;
}

keel::i32 MainWindow::GetHeight() const noexcept {
    return m_window ? m_window->GetFramebufferHeight() : m_config.height;
}

keel::f32 MainWindow::GetDpiScale() const noexcept {
    return m_window ? m_window->GetDpiScale() : 1.0f;
}

keel::f32 MainWindow::GetDisplayPixelsPerMm() const noexcept {
    return m_window ? m_window->GetDisplayPixelsPerMm() : (96.0f / 25.4f);
}

bool MainWindow::IsLiveResizeFrame() const noexcept {
    return m_window && m_window->IsLiveResizeFrame();
}

bool MainWindow::IsMinimized() const noexcept {
    return m_window && m_window->IsMinimized();
}

bool MainWindow::IsVsyncEnabled() const noexcept {
    return m_window ? m_window->IsVsyncEnabled() : m_config.vsync;
}

void MainWindow::SetVsyncEnabled(bool enabled) noexcept {
    m_config.vsync = enabled;
    if (m_window) {
        m_window->SetVsyncEnabled(enabled);
    }
}

void MainWindow::SetReactiveMode(bool enabled) noexcept {
    m_reactiveMode = enabled;
    if (!m_reactiveMode) {
        m_ui.InvalidateAll();
    }
}

bool MainWindow::IsGpuShadersEnabled() const noexcept {
    return m_gpuShadersEnabled && IsGpuShadersAvailable();
}

bool MainWindow::IsGpuShadersAvailable() const noexcept {
    const bool underlay = m_window && m_window->SupportsUnderlayShaders();
    const bool direct = m_canvas && !m_canvas->UsesCpuFramebuffer() && m_canvas->SupportsGpuShaders();
    const bool cpuWithUnderlay = m_canvas && m_canvas->UsesCpuFramebuffer() && underlay;
    const bool canvasCap = m_canvas && m_canvas->SupportsGpuShaders();
    return underlay || direct || cpuWithUnderlay || canvasCap;
}

bool MainWindow::IsDirtyRectAvailable() const noexcept {
    return m_canvas && m_canvas->UsesCpuFramebuffer();
}

void MainWindow::SetGpuShadersEnabled(bool enabled) noexcept {
    m_config.gpuShaders = enabled;
    m_gpuShadersEnabled = enabled;
    if (m_window) {
        m_window->SetUnderlayShadersEnabled(enabled);
    }
    if (m_canvas) {
        m_canvas->SetGpuShadersEnabled(enabled);
    }
    m_ui.InvalidateAll();
    m_ui.RequestRepaint(2);
}

const TextRenderSettings& MainWindow::GetTextRenderSettings() const noexcept {
    if (m_canvas) {
        return m_canvas->GetTextRenderSettings();
    }
    return m_config.textRender;
}

void MainWindow::SetTextRenderSettings(const TextRenderSettings& settings) noexcept {
    m_config.textRender = settings;
    if (m_canvas) {
        m_canvas->SetTextRenderSettings(settings);
    }
    m_ui.InvalidateAll();
    m_ui.RequestRepaint(2);
}

void MainWindow::SyncWindowFrameAppearance() noexcept {
    if (!m_window) {
        return;
    }
    const WindowCornerStyle style = m_shellTheme.ResolveWindowCorners(m_windowCornerOverride);
    m_window->SetWindowCornerStyle(style);
    m_window->SetWindowBorderColor(m_shellTheme.palette.border);
}

void MainWindow::SetShellTheme(const UiTheme& theme) noexcept {
    m_shellTheme = theme;
    m_shellLightMode = (m_shellTheme.mode == ThemeMode::Light);
    m_shellFloating = (m_shellTheme.shellStyle == ShellStyle::Floating);
    m_ui.SetTheme(m_shellTheme);
    m_ui.SetClearColor(m_shellTheme.palette.background);
    SyncWindowFrameAppearance();
    m_ui.InvalidateAll();
}

void MainWindow::SetShellThemePreset(ThemePreset preset) noexcept {
    const ShellStyle prevShell = m_shellTheme.shellStyle;
    m_shellTheme = UiTheme::FromPreset(preset);
    m_shellTheme.SetShellStyle(prevShell);
    m_shellLightMode = (m_shellTheme.mode == ThemeMode::Light);
    m_ui.SetTheme(m_shellTheme);
    m_ui.SetClearColor(m_shellTheme.palette.background);
    SyncWindowFrameAppearance();
    m_ui.InvalidateAll();
}

keel::Optional<ThemeMode> MainWindow::QuerySystemTheme() const noexcept {
    if (m_window) {
        if (const auto mode = m_window->QuerySystemTheme(); mode.has_value()) {
            return mode;
        }
    }
    return QueryOsSystemTheme();
}

void MainWindow::SetShellLightMode(bool lightMode) noexcept {
    m_shellLightMode = lightMode;
    m_shellTheme.SetMode(m_shellLightMode ? ThemeMode::Light : ThemeMode::Dark);
    m_ui.SetTheme(m_shellTheme);
    m_ui.SetClearColor(m_shellTheme.palette.background);
    SyncWindowFrameAppearance();
    m_ui.InvalidateAll();
}

void MainWindow::SetShellFloating(bool floating) noexcept {
    m_shellFloating = floating;
    m_shellTheme.SetShellStyle(m_shellFloating ? ShellStyle::Floating : ShellStyle::Docked);
    m_ui.SetTheme(m_shellTheme);
    SyncWindowFrameAppearance();
    m_ui.InvalidateAll();
}

void MainWindow::SetShellScale(keel::f32 scale) noexcept {
    keel::usize bestIdx = 0;
    keel::f32 bestDiff = keel::Limits<keel::f32>::kMax;
    for (keel::usize i = 0; i < kScaleValues.size(); ++i) {
        const keel::f32 d = keel::abs(kScaleValues[i] - scale);
        if (d < bestDiff) {
            bestDiff = d;
            bestIdx = i;
        }
    }
    SetShellScaleIndex(bestIdx);
}

void MainWindow::SetShellScaleIndex(keel::usize index) noexcept {
    m_shellScaleIndex = keel::min(index, kScaleValues.size() - 1);
    m_shellScale = kScaleValues[m_shellScaleIndex];
    m_ui.InvalidateAll();
}

void MainWindow::StepShellScale(keel::i32 steps) noexcept {
    const keel::i64 maxIdx = keel::as<keel::i64>(kScaleValues.size()) - 1;
    const keel::i64 target = keel::clamp(keel::as<keel::i64>(m_shellScaleIndex) + steps, keel::i64{0}, maxIdx);
    SetShellScaleIndex(keel::as<keel::usize>(target));
}

void MainWindow::SetTitle(keel::StringRef title) noexcept {
    m_config.title = keel::FixedString<128>(title);
    if (m_window) {
        m_window->SetTitle(title);
    }
    m_ui.InvalidateAll();
}

void MainWindow::Minimize() noexcept {
    if (m_window) {
        m_window->Minimize();
    }
}

void MainWindow::Maximize() noexcept {
    if (m_window) {
        m_window->Maximize();
        m_ui.InvalidateAll();
    }
}

void MainWindow::Restore() noexcept {
    if (m_window) {
        m_window->Restore();
        m_ui.InvalidateAll();
    }
}

bool MainWindow::IsMaximized() const noexcept {
    return m_window ? m_window->IsMaximized() : false;
}

bool MainWindow::IsFocused() const noexcept {
    return m_window ? m_window->IsFocused() : true;
}

bool MainWindow::SupportsCustomTitleBar() const noexcept {
    return m_window ? m_window->SupportsCustomTitleBar() : false;
}

bool MainWindow::IsCustomTitleBar() const noexcept {
    return m_window ? m_window->IsCustomTitleBar() : m_config.customTitleBar;
}

bool MainWindow::SetCustomTitleBar(bool enabled) noexcept {
    if (!m_window) {
        m_config.customTitleBar = enabled;
        return true;
    }
    if (!m_window->SetCustomTitleBar(enabled)) {
        return false;
    }
    m_config.customTitleBar = m_window->IsCustomTitleBar();
    SyncWindowFrameAppearance();
    m_ui.InvalidateAll();
    m_ui.RequestRepaint(2);
    return true;
}

void MainWindow::SetWindowCornerStyle(WindowCornerStyle style) noexcept {
    m_windowCornerOverride = (style == WindowCornerStyle::Auto)
        ? keel::Optional<WindowCornerStyle>(keel::kNullOpt)
        : keel::Optional<WindowCornerStyle>(style);
    SyncWindowFrameAppearance();
}

WindowCornerStyle MainWindow::GetWindowCornerStyle() const noexcept {
    return m_windowCornerOverride.value_or(m_shellTheme.shell.windowCorners);
}

FileDialogResult MainWindow::ShowFileDialog(const FileDialogOptions& opts) {
    if (!m_window) {
        return FileDialogResult{.accepted = false, .supported = false, .paths = {}};
    }
    FileDialogResult res = m_window->ShowFileDialog(opts);
    m_ui.ResetMouseButtonsAfterModalDialog();
    return res;
}

void MainWindow::ShowFileDialogAsync(
    const FileDialogOptions& opts,
    keel::Function<void(FileDialogResult)> onDone
) {
    if (!m_window) {
        if (onDone) {
            onDone(FileDialogResult{.accepted = false, .supported = false, .paths = {}});
        }
        return;
    }
    m_window->ShowFileDialogAsync(opts, keel::move(onDone));
}

MessageBoxChoice MainWindow::ShowMessageBox(const MessageBoxOptions& opts) {
    if (!m_window) {
        return MessageBoxChoice::Cancel;
    }
    const MessageBoxChoice choice = m_window->ShowMessageBox(opts);
    m_ui.ResetMouseButtonsAfterModalDialog();
    return choice;
}

keel::i32 RunWindows(keel::ArrayRef<MainWindow*> windows) {
    if (windows.empty()) {
        return 0;
    }
    for (MainWindow* win : windows) {
        if (win && !win->Initialize()) {
            for (MainWindow* started : windows) {
                if (started && started->IsRunning()) {
                    started->Shutdown();
                }
            }
            return 1;
        }
    }

    bool anyAlive = true;
    while (anyAlive) {
        anyAlive = false;
        for (MainWindow* win : windows) {
            if (!win || !win->IsRunning()) {
                continue;
            }
            if (win->StepFrame()) {
                anyAlive = true;
            } else {
                win->Shutdown();
            }
        }
    }

    for (MainWindow* win : windows) {
        if (win) {
            win->Shutdown();
        }
    }
    return 0;
}

} // namespace guikit
