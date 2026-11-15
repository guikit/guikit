#include "guikit/core/main_window.hpp"

#include <cmath>
#include "keel/core.h"
#include "keel/format.h"
#include "keel/math.h"
#include "keel/time.h"

namespace guikit {

namespace {

WindowBackendFactory g_defaultWindowBackendFactory = nullptr;

} // namespace

void RegisterDefaultWindowBackendFactory(WindowBackendFactory factory) noexcept {
    g_defaultWindowBackendFactory = factory;
}

keel::UniquePtr<IWindowBackend> CreateDefaultWindowBackend() {
    if (g_defaultWindowBackendFactory != nullptr) {
        return g_defaultWindowBackendFactory();
    }
    return nullptr;
}

MainWindow::MainWindow(MainWindowConfig config, keel::UniquePtr<IWindowBackend> backend)
    : m_config(keel::move(config))
    , m_backend(backend ? keel::move(backend) : CreateDefaultWindowBackend())
    , m_shellTheme(m_config.initialTheme.has_value()
                   ? *m_config.initialTheme
                   : (m_config.initialThemeMode == ThemeMode::Light
                      ? UiTheme::Light(m_config.initialDensity)
                      : UiTheme::Dark(m_config.initialDensity)))
    , m_shellLightMode(m_shellTheme.mode == ThemeMode::Light)
    , m_shellFloating(m_config.initialTheme.has_value()
                      ? (m_shellTheme.shellStyle == ShellStyle::Floating)
                      : (m_config.initialShellStyle == ShellStyle::Floating))
    , m_reactiveMode(m_config.reactiveMode) {
    if (!m_config.initialTheme.has_value()) {
        m_shellTheme.SetShellStyle(m_config.initialShellStyle);
    }
    m_ui.SetOpenUrlCallback([this](keel::StringRef url) { OpenUrl(url); });
    SetShellScale(m_config.initialScale);
    for (keel::usize i = 0; i < kHeartbeatMsValues.size(); ++i) {
        if (kHeartbeatMsValues[i] == m_config.idleHeartbeatMs) {
            m_heartbeatIndex = i;
            break;
        }
    }
}

MainWindow::~MainWindow() {
    if (m_backend) {
        m_backend->Shutdown();
    }
}

keel::i32 MainWindow::Run(RenderCallback renderCallback) {
    m_renderCallback = keel::move(renderCallback);
    return Run();
}

keel::i32 MainWindow::Run() {
    if (!m_backend) {
        m_backend = CreateDefaultWindowBackend();
        if (!m_backend) {
            keel::eprintln("guikit::MainWindow::Run() failed: no IWindowBackend registered or provided.");
            return 1;
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
        }
    };

    if (!m_backend->Initialize(m_config, m_ui, keel::move(hostCallbacks))) {
        return 1;
    }

    m_running = true;
    OnInit();

    // Synchronize m_shellTheme if OnInit() customized m_ui.GetTheme() directly
    const UiTheme& postInitTheme = m_ui.GetTheme();
    if (postInitTheme.mode != m_shellTheme.mode ||
        postInitTheme.density != m_shellTheme.density ||
        postInitTheme.shellStyle != m_shellTheme.shellStyle ||
        postInitTheme.palette.background != m_shellTheme.palette.background ||
        postInitTheme.palette.surface != m_shellTheme.palette.surface ||
        postInitTheme.palette.accent != m_shellTheme.palette.accent) {
        m_shellTheme = postInitTheme;
        m_shellLightMode = (m_shellTheme.mode == ThemeMode::Light);
        m_shellFloating = (m_shellTheme.shellStyle == ShellStyle::Floating);
    }

    if (m_backend->UsesMainLoopCallback()) {
        m_backend->RunMainLoopCallback([this]() -> bool {
            if (!m_running) {
                OnShutdown();
                m_backend->Shutdown();
                return false;
            }
            const keel::i32 timeoutMs = m_ui.GetRecommendedSleepTimeoutMs();
            if (!m_backend->PumpEvents(m_ui, m_reactiveMode, timeoutMs)) {
                m_running = false;
                OnShutdown();
                m_backend->Shutdown();
                return false;
            }
            if (m_backend->ShouldRenderFrame()) {
                RenderFrame();
            }
            return m_running;
        });
        return 0;
    }

    while (m_running) {
        const keel::i32 timeoutMs = m_ui.GetRecommendedSleepTimeoutMs();
        if (!m_backend->PumpEvents(m_ui, m_reactiveMode, timeoutMs)) {
            m_running = false;
            break;
        }
        if (!m_running) {
            break;
        }
        if (m_backend->ShouldRenderFrame()) {
            RenderFrame();
        }
    }

    OnShutdown();
    m_backend->Shutdown();
    return 0;
}

void MainWindow::OnRender(Context& ui) {
    if (m_renderCallback) {
        m_renderCallback(ui);
    }
}

void MainWindow::RenderFrame() {
    if (m_inRenderFrame || !m_backend) {
        return;
    }
    const keel::i32 winW = m_backend->GetWidth();
    const keel::i32 winH = m_backend->GetHeight();
    if (winW <= 0 || winH <= 0) {
        return;
    }

    m_inRenderFrame = true;
    struct FrameGuard {
        bool& flag;
        ~FrameGuard() { flag = false; }
    } frameGuard{m_inRenderFrame};

    m_backend->PrepareFrameTarget(m_ui);

    const keel::u64 frameWorkStartNs = keel::monotonic_ns();
    m_metrics.BeginFrame();

    // One dp -> pixel factor for the whole frame: OS display scale x user shell zoom. The canvas applies
    // it uniformly, so the shell zoom behaves exactly like a different monitor DPI and nothing in the
    // tree needs to be scaled by hand.
    m_ui.BeginFrame({static_cast<float>(winW), static_cast<float>(winH)}, GetDpiScale() * GetShellScale());
    {
        const ShellStyle activeShellStyle = m_shellFloating ? ShellStyle::Floating : ShellStyle::Docked;
        if (m_shellTheme.GetShellStyle() != activeShellStyle) {
            m_shellTheme.SetShellStyle(activeShellStyle);
            m_ui.SetTheme(m_shellTheme);
        }

        if (m_config.autoShellScope) {
            if (auto mainCol = m_ui.Column({.theme = &m_shellTheme})) {
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
    m_ui.EndFrame(m_backend->GetCanvas());
    m_backend->UpdateSystemCursor(m_ui.GetCursor());
    m_metrics.MarkCpuDone();

    const bool shouldPresent = m_ui.HasDirtyDamage() || !m_reactiveMode;
    if (shouldPresent) {
        m_backend->PresentFrame(m_ui, m_metrics, !m_reactiveMode);
    }

    if (shouldPresent && !m_backend->IsVsyncEnabled() && !m_reactiveMode) {
        const keel::u64 frameEndNs = keel::monotonic_ns();
        const double frameElapsedMs = (frameEndNs > frameWorkStartNs)
            ? (static_cast<double>(frameEndNs - frameWorkStartNs) * 1e-6)
            : 0.0;
        m_backend->PaceFrame(frameElapsedMs, true);
    }

    m_metrics.EndFrame(keel::max(1.0f, m_config.targetFps));

    if (!shouldPresent && m_ui.NeedsRepaint()) {
        const keel::u64 frameEndNs = keel::monotonic_ns();
        const double frameElapsedMs = (frameEndNs > frameWorkStartNs)
            ? (static_cast<double>(frameEndNs - frameWorkStartNs) * 1e-6)
            : 0.0;
        m_backend->PaceFrame(frameElapsedMs, false);
    }
}

void MainWindow::WakeUp() noexcept {
    if (m_backend) {
        m_backend->WakeUp();
    }
}

void MainWindow::OpenUrl(keel::StringRef url) const noexcept {
    if (m_backend) {
        m_backend->OpenUrl(url);
    }
}

keel::i32 MainWindow::GetWidth() const noexcept {
    return m_backend ? m_backend->GetWidth() : m_config.width;
}

keel::i32 MainWindow::GetHeight() const noexcept {
    return m_backend ? m_backend->GetHeight() : m_config.height;
}

float MainWindow::GetDpiScale() const noexcept {
    return m_backend ? m_backend->GetDpiScale() : 1.0f;
}

float MainWindow::GetDisplayPixelsPerMm() const noexcept {
    return m_backend ? m_backend->GetDisplayPixelsPerMm() : (96.0f / 25.4f);
}

bool MainWindow::IsLiveResizeFrame() const noexcept {
    return m_backend && m_backend->IsLiveResizeFrame();
}

bool MainWindow::IsMinimized() const noexcept {
    return m_backend && m_backend->IsMinimized();
}

bool MainWindow::IsVsyncEnabled() const noexcept {
    return m_backend ? m_backend->IsVsyncEnabled() : m_config.vsync;
}

void MainWindow::SetVsyncEnabled(bool enabled) noexcept {
    m_config.vsync = enabled;
    if (m_backend) {
        m_backend->SetVsyncEnabled(enabled);
    }
}

void MainWindow::SetReactiveMode(bool enabled) noexcept {
    m_reactiveMode = enabled;
    if (!m_reactiveMode) {
        m_ui.InvalidateAll();
    }
}

bool MainWindow::IsGpuShadersEnabled() const noexcept {
    return m_backend ? m_backend->IsGpuShadersEnabled() : m_config.gpuShaders;
}

bool MainWindow::IsGpuShadersAvailable() const noexcept {
    return m_backend && m_backend->IsGpuShadersAvailable();
}

void MainWindow::SetGpuShadersEnabled(bool enabled) noexcept {
    m_config.gpuShaders = enabled;
    if (m_backend) {
        m_backend->SetGpuShadersEnabled(enabled);
    }
    m_ui.InvalidateAll();
    m_ui.RequestRepaint(2);
}

void MainWindow::SetShellTheme(const UiTheme& theme) noexcept {
    m_shellTheme = theme;
    m_shellLightMode = (m_shellTheme.mode == ThemeMode::Light);
    m_shellFloating = (m_shellTheme.shellStyle == ShellStyle::Floating);
    m_ui.SetTheme(m_shellTheme);
    m_ui.SetClearColor(m_shellTheme.palette.background);
    m_ui.InvalidateAll();
}

void MainWindow::SetShellLightMode(bool lightMode) noexcept {
    m_shellLightMode = lightMode;
    m_shellTheme.SetMode(m_shellLightMode ? ThemeMode::Light : ThemeMode::Dark);
    m_ui.SetTheme(m_shellTheme);
    m_ui.SetClearColor(m_shellTheme.palette.background);
    m_ui.InvalidateAll();
}

void MainWindow::SetShellFloating(bool floating) noexcept {
    m_shellFloating = floating;
    m_shellTheme.SetShellStyle(m_shellFloating ? ShellStyle::Floating : ShellStyle::Docked);
    m_ui.SetTheme(m_shellTheme);
    m_ui.InvalidateAll();
}

void MainWindow::SetShellScale(float scale) noexcept {
    keel::usize bestIdx = 0;
    float bestDiff = keel::Limits<float>::kMax;
    for (keel::usize i = 0; i < kScaleValues.size(); ++i) {
        const float d = std::abs(kScaleValues[i] - scale);
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
    const keel::i64 maxIdx = static_cast<keel::i64>(kScaleValues.size()) - 1;
    const keel::i64 target = keel::clamp(static_cast<keel::i64>(m_shellScaleIndex) + steps, keel::i64{0}, maxIdx);
    SetShellScaleIndex(static_cast<keel::usize>(target));
}

void MainWindow::SetCommandPaletteOpen(bool open) noexcept {
    if (m_commandPaletteOpen != open) {
        m_commandPaletteOpen = open;
        m_ui.InvalidateAll();
    }
}

// =========================================================================
// Composable Shell UI Helpers
// =========================================================================

bool MainWindow::RenderSidebarCycleButton(
    ToolbarScope& tb,
    SidebarMode& mode,
    keel::StringRef label,
    keel::StringRef sidebarTitle,
    const Icon& icon
) {
    const StateColor btnColor = (mode == SidebarMode::Expanded)
        ? StateColor::Primary()
        : (mode == SidebarMode::Compact)
            ? StateColor::Info()
            : StateColor::Ghost();

    const keel::StringRef tooltip = (mode == SidebarMode::Expanded)
        ? m_ui.Format("{}: Expanded (Click for Compact Icons, Right-click to Collapse)", sidebarTitle)
        : (mode == SidebarMode::Compact)
            ? m_ui.Format("{}: Compact Icons (Click to Collapse, Right-click to Expand)", sidebarTitle)
            : m_ui.Format("{}: Collapsed (Click to Expand)", sidebarTitle);

    const auto btn = tb.ToolButton({
        .text = label,
        .tooltip = tooltip,
        .icon = icon,
        .color = btnColor,
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
        m_ui.InvalidateAll();
        return true;
    }
    if (btn.rightClicked) {
        mode = (mode == SidebarMode::Collapsed)
            ? SidebarMode::Expanded
            : SidebarMode::Collapsed;
        m_ui.InvalidateAll();
        return true;
    }
    return false;
}

bool MainWindow::RenderSidebarToggle(
    ToolbarScope& tb,
    SidebarMode& mode,
    keel::StringRef label,
    keel::StringRef sidebarTitle,
    const Icon& icon
) {
    bool visible = (mode != SidebarMode::Collapsed);
    const keel::StringRef tooltip = visible
        ? m_ui.Format("{}: Expanded (Click to Collapse)", sidebarTitle)
        : m_ui.Format("{}: Collapsed (Click to Expand)", sidebarTitle);

    if (tb.Toggle(visible, {
        .text = label,
        .tooltip = tooltip,
        .icon = icon,
        .color = StateColor::Primary()
    }).changed) {
        mode = visible ? SidebarMode::Expanded : SidebarMode::Collapsed;
        m_ui.InvalidateAll();
        return true;
    }
    return false;
}

void MainWindow::RenderThemeControls(ToolbarScope& tb) {
    (void)tb;
    if (m_ui.Size(UiSize::Sm).Select(m_shellScaleIndex, kScaleLabels, {.width = 92.0f}).changed) {
        SetShellScaleIndex(m_shellScaleIndex);
    }

    if (m_ui.Size(UiSize::Md).Switch(m_shellLightMode, {
        .textLeft = "Dark",
        .textRight = "Light",
        .symmetric = true
    }).changed) {
        SetShellLightMode(m_shellLightMode);
    }

    if (m_ui.Size(UiSize::Md).Switch(m_shellFloating, {
        .textLeft = "Dock",
        .textRight = "Float",
        .symmetric = true
    }).changed) {
        SetShellFloating(m_shellFloating);
    }
}

void MainWindow::RenderDebugControls(ToolbarScope& tb) {
    if (auto debugGroup = m_ui.Row({.equalWidths = true})) {
        bool boundsActive = m_ui.IsDebugBounds();
        if (tb.Toggle(boundsActive, {
            .text = "Bounds",
            .icon = boundsActive ? Icon::Eye() : Icon::EyeOff(),
            .color = StateColor::Info()
        }).changed) {
            m_ui.SetDebugBounds(boundsActive);
        }

        bool inspectActive = m_ui.IsInspectMode();
        if (tb.Toggle(inspectActive, {
            .text = "Inspect",
            .icon = Icon::Search(),
            .color = StateColor::Warning()
        }).changed) {
            m_ui.SetInspectMode(inspectActive);
        }
    }
}

void MainWindow::RenderEngineControls(ToolbarScope& tb) {
    if (auto engineGroup = m_ui.Row({.equalWidths = true})) {
        bool gpuEnabled = IsGpuShadersEnabled();
        if (tb.Toggle(gpuEnabled, {
            .text = "GPU",
            .tooltip = "Toggle Hardware GPU Shader Acceleration",
            .icon = Icon::Flash(),
            .color = StateColor::Success(),
            .disabled = !IsGpuShadersAvailable()
        }).changed) {
            SetGpuShadersEnabled(gpuEnabled);
        }

        bool vsync = IsVsyncEnabled();
        if (tb.Toggle(vsync, {
            .text = "VSync",
            .tooltip = "Toggle Vertical Sync",
            .color = StateColor::Info()
        }).changed) {
            SetVsyncEnabled(vsync);
        }

        if (tb.Toggle(m_reactiveMode, {
            .text = "React",
            .tooltip = "Toggle Event-Driven Reactive Sleep Loop",
            .color = StateColor::Primary()
        }).changed) {
            SetReactiveMode(m_reactiveMode);
        }

        bool dirtyActive = m_ui.IsDirtyRectEnabled();
        if (tb.Toggle(dirtyActive, {
            .text = "Dirty",
            .tooltip = "Toggle Disjoint Dirty-Rect Partial Rasterization",
            .color = StateColor::Success()
        }).changed) {
            m_ui.SetDirtyRectEnabled(dirtyActive);
            if (!dirtyActive) {
                m_ui.InvalidateAll();
            }
        }

        bool liveResizeActive = m_ui.IsLiveResizeEnabled();
        if (tb.Toggle(liveResizeActive, {
            .text = "Live",
            .tooltip = liveResizeActive
                ? "Live Window Resize: Smooth 60 FPS reflow while dragging window border"
                : "Deferred Window Resize: Reflow after window resize step completes",
            .color = StateColor::Info()
        }).changed) {
            m_ui.SetLiveResize(liveResizeActive);
        }
    }

    if (tb.Action({
        .text = {kHeartbeatLabels[m_heartbeatIndex], kHeartbeatLabels},
        .icon = Icon::Refresh(),
        .disabled = !m_reactiveMode
    })) {
        m_heartbeatIndex = (m_heartbeatIndex + 1) % kHeartbeatLabels.size();
        m_ui.SetIdleHeartbeatMs(kHeartbeatMsValues[m_heartbeatIndex]);
    }
}

void MainWindow::RenderTelemetryStatus(StatusBarScope& sb, bool gpuSceneActive) {
    if (IsGpuShadersAvailable()) {
        sb.Item({
            .text = {
                gpuSceneActive ? "GPU Shaders: Active" : "GPU Shaders: Ready",
                {"GPU Shaders: Active", "GPU Shaders: Ready"}
            },
            .dotColor = gpuSceneActive
                ? m_shellTheme.GetVariant(UiVariant::Success).bg
                : m_shellTheme.GetVariant(UiVariant::Info).bg,
            .color = gpuSceneActive ? StateColor::Success() : StateColor::Info()
        });
    } else {
        sb.Item({
            .text = "GPU Shaders: CPU Fallback",
            .dotColor = m_shellTheme.GetVariant(UiVariant::Warning).bg,
            .color = StateColor::Warning()
        });
    }

    sb.Separator();
    sb.Item({
        .text = {m_metrics.GetFpsString(), {"0 FPS (Idle)", "999 FPS"}},
        .color = StateColor::Secondary()
    });
    sb.Separator();
    sb.Item({
        .text = {m_metrics.GetCpuString(), {"CPU: 99.99 ms (100.0%)"}},
        .color = StateColor::Secondary()
    });
    sb.Separator();
    sb.Item({
        .text = {m_metrics.GetGpuString(), {"GPU: 99.99 ms (100% budget)"}},
        .color = StateColor::Secondary()
    });
    sb.Separator();
    sb.Item({
        .text = {m_metrics.GetRamString(), {"RAM: 9999.9 MB"}},
        .color = StateColor::Secondary()
    });
}

CommandPaletteResponse MainWindow::RenderCommandPalette(
    StatusBarScope& sb,
    keel::ArrayRef<CommandPaletteItem> items,
    UiText placeholder
) {
    if (m_ui.ConsumeShortcut(Key::K, true)) {
        m_commandPaletteOpen = !m_commandPaletteOpen;
        m_ui.InvalidateAll();
    }
    if (sb.Action({
        .text = "Command Palette",
        .icon = Icon::Search(),
        .tooltip = "Open Global Command Palette (Ctrl+K)",
        .active = m_commandPaletteOpen
    }).clicked) {
        m_commandPaletteOpen = !m_commandPaletteOpen;
        m_ui.InvalidateAll();
    }

    return m_ui.CommandPalette(m_commandPaletteOpen, items, {
        .placeholder = placeholder
    });
}

} // namespace guikit
