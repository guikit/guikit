#include "guikit/core/MainWindow.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>

namespace guikit {

namespace {

WindowBackendFactory g_defaultWindowBackendFactory = nullptr;

} // namespace

void RegisterDefaultWindowBackendFactory(WindowBackendFactory factory) noexcept {
    g_defaultWindowBackendFactory = factory;
}

std::unique_ptr<IWindowBackend> CreateDefaultWindowBackend() {
    if (g_defaultWindowBackendFactory != nullptr) {
        return g_defaultWindowBackendFactory();
    }
    return nullptr;
}

MainWindow::MainWindow(MainWindowConfig config, std::unique_ptr<IWindowBackend> backend)
    : m_config(std::move(config))
    , m_backend(backend ? std::move(backend) : CreateDefaultWindowBackend())
    , m_shellTheme(m_config.initialThemeMode == ThemeMode::Light
                   ? UiTheme::Light(Density::Standard)
                   : UiTheme::Dark(Density::Standard))
    , m_shellLightMode(m_config.initialThemeMode == ThemeMode::Light)
    , m_shellFloating(m_config.initialShellStyle == ShellStyle::Floating)
    , m_reactiveMode(m_config.reactiveMode) {
    m_shellTheme.SetShellStyle(m_config.initialShellStyle);
    for (size_t i = 0; i < kHeartbeatMsValues.size(); ++i) {
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

int MainWindow::Run(RenderCallback renderCallback) {
    m_renderCallback = std::move(renderCallback);
    return Run();
}

int MainWindow::Run() {
    if (!m_backend) {
        m_backend = CreateDefaultWindowBackend();
        if (!m_backend) {
            std::cerr << "guikit::MainWindow::Run() failed: no IWindowBackend registered or provided.\n";
            return 1;
        }
    }

    m_ui.SetTheme(m_shellTheme);
    m_ui.SetClearColor(m_shellTheme.palette.background);
    m_ui.SetIdleHeartbeatMs(m_config.idleHeartbeatMs);

    const WindowHostCallbacks hostCallbacks{
        .onCloseRequested = [this]() -> bool {
            if (OnCloseRequested()) {
                m_running = false;
                return true;
            }
            return false;
        },
        .onResize = [this](int w, int h) {
            OnResize(w, h);
        },
        .onLiveResizeRender = [this]() {
            RenderFrame();
        }
    };

    if (!m_backend->Initialize(m_config, m_ui, hostCallbacks)) {
        return 1;
    }

    m_running = true;
    OnInit();

    if (m_backend->UsesMainLoopCallback()) {
        m_backend->RunMainLoopCallback([this]() -> bool {
            if (!m_running) {
                OnShutdown();
                m_backend->Shutdown();
                return false;
            }
            const int timeoutMs = m_ui.GetRecommendedSleepTimeoutMs();
            if (!m_backend->PumpEvents(m_ui, m_reactiveMode, timeoutMs)) {
                m_running = false;
                OnShutdown();
                m_backend->Shutdown();
                return false;
            }
            if (!m_backend->IsMinimized()) {
                RenderFrame();
            }
            return m_running;
        });
        return 0;
    }

    while (m_running) {
        const int timeoutMs = m_ui.GetRecommendedSleepTimeoutMs();
        if (!m_backend->PumpEvents(m_ui, m_reactiveMode, timeoutMs)) {
            m_running = false;
            break;
        }
        if (!m_running) {
            break;
        }
        if (!m_backend->IsMinimized()) {
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
    const int winW = m_backend->GetWidth();
    const int winH = m_backend->GetHeight();
    if (winW <= 0 || winH <= 0) {
        return;
    }

    m_inRenderFrame = true;
    struct FrameGuard {
        bool& flag;
        ~FrameGuard() { flag = false; }
    } frameGuard{m_inRenderFrame};

    m_backend->PrepareFrameTarget(m_ui);

    const auto frameWorkStart = std::chrono::steady_clock::now();
    m_metrics.BeginFrame();

    m_ui.BeginFrame({static_cast<float>(winW), static_cast<float>(winH)}, 1.0f);
    {
        const ShellStyle activeShellStyle = m_shellFloating ? ShellStyle::Floating : ShellStyle::Docked;
        if (m_shellTheme.GetShellStyle() != activeShellStyle) {
            m_shellTheme.SetShellStyle(activeShellStyle);
            m_ui.SetTheme(m_shellTheme);
        }

        if (m_config.autoShellScope) {
            if (auto mainCol = m_ui.Column({
                .theme = &m_shellTheme,
                .scale = GetShellScale()
            })) {
                OnRender(m_ui);
            }
        } else {
            OnRender(m_ui);
        }
    }
    m_ui.EndFrame(m_backend->GetCanvas());
    m_backend->UpdateSystemCursor(m_ui.GetCursor());
    m_metrics.MarkCpuDone();

    const bool shouldPresent = m_ui.HasDirtyDamage() || !m_reactiveMode;
    if (shouldPresent) {
        m_backend->PresentFrame(m_ui, m_metrics, !m_reactiveMode);
    }

    if (!shouldPresent || (!m_backend->IsVsyncEnabled() && !m_reactiveMode)) {
        const auto frameEnd = std::chrono::steady_clock::now();
        const double frameElapsedMs = std::chrono::duration<double, std::milli>(frameEnd - frameWorkStart).count();
        m_backend->PaceFrame(frameElapsedMs, shouldPresent);
    }

    m_metrics.EndFrame(60.0f);
}

void MainWindow::OpenUrl(std::string_view url) const noexcept {
    if (m_backend) {
        m_backend->OpenUrl(url);
    }
}

int MainWindow::GetWidth() const noexcept {
    return m_backend ? m_backend->GetWidth() : m_config.width;
}

int MainWindow::GetHeight() const noexcept {
    return m_backend ? m_backend->GetHeight() : m_config.height;
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

void MainWindow::SetShellScaleIndex(size_t index) noexcept {
    m_shellScaleIndex = std::min(index, kScaleValues.size() - 1);
    m_ui.InvalidateAll();
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
    std::string_view label,
    std::string_view sidebarTitle,
    const Icon& icon
) {
    const StateColor btnColor = (mode == SidebarMode::Expanded)
        ? StateColor::Primary()
        : (mode == SidebarMode::Compact)
            ? StateColor::Info()
            : StateColor::Ghost();

    const std::string_view tooltip = (mode == SidebarMode::Expanded)
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
    std::string_view label,
    std::string_view sidebarTitle,
    const Icon& icon
) {
    bool visible = (mode != SidebarMode::Collapsed);
    const std::string_view tooltip = visible
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
        m_ui.InvalidateAll();
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
    std::span<const CommandPaletteItem> items,
    std::string_view placeholder
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
