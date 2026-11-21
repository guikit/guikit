#include "guikit/core/main_window.hpp"

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
                      ? UiTheme::Light()
                      : UiTheme::Dark()))
    , m_shellLightMode(m_shellTheme.mode == ThemeMode::Light)
    , m_shellFloating(m_config.initialTheme.has_value()
                      ? (m_shellTheme.shellStyle == ShellStyle::Floating)
                      : (m_config.initialShellStyle == ShellStyle::Floating))
    , m_reactiveMode(m_config.reactiveMode) {
    if (!m_config.initialTheme.has_value()) {
        m_shellTheme.SetShellStyle(m_config.initialShellStyle);
    }
    m_ui.SetDensity(m_config.initialDensity);
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
    m_backend->GetCanvas().SetTextRenderSettings(m_config.textRender);

    m_running = true;
    OnInit();

    // Synchronize m_shellTheme if OnInit() customized m_ui.GetTheme() directly
    const UiTheme& postInitTheme = m_ui.GetTheme();
    if (postInitTheme.mode != m_shellTheme.mode ||
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

    // One dp -> pixel factor for the whole frame: OS display scale x root zoom (the shell zoom). The canvas applies
    // it uniformly, so the shell zoom behaves exactly like a different monitor DPI and nothing in the
    // tree needs to be scaled by hand.
    m_ui.BeginFrame({keel::as<keel::f32>(winW), keel::as<keel::f32>(winH)}, GetDpiScale(), GetShellScale());
    {
        const ShellStyle activeShellStyle = m_shellFloating ? ShellStyle::Floating : ShellStyle::Docked;
        if (m_shellTheme.GetShellStyle() != activeShellStyle) {
            m_shellTheme.SetShellStyle(activeShellStyle);
            m_ui.SetTheme(m_shellTheme);
        }

        if (m_config.autoShellScope) {
            if (auto mainCol = m_ui.Column({})) {
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
        const keel::f64 frameElapsedMs = (frameEndNs > frameWorkStartNs)
            ? (keel::as<keel::f64>(frameEndNs - frameWorkStartNs) * 1e-6)
            : 0.0;
        m_backend->PaceFrame(frameElapsedMs, true);
    }

    m_metrics.EndFrame(keel::max(1.0f, m_config.targetFps));

    if (!shouldPresent && m_ui.NeedsRepaint()) {
        const keel::u64 frameEndNs = keel::monotonic_ns();
        const keel::f64 frameElapsedMs = (frameEndNs > frameWorkStartNs)
            ? (keel::as<keel::f64>(frameEndNs - frameWorkStartNs) * 1e-6)
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

keel::f32 MainWindow::GetDpiScale() const noexcept {
    return m_backend ? m_backend->GetDpiScale() : 1.0f;
}

keel::f32 MainWindow::GetDisplayPixelsPerMm() const noexcept {
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

const TextRenderSettings& MainWindow::GetTextRenderSettings() const noexcept {
    if (m_backend) {
        return m_backend->GetCanvas().GetTextRenderSettings();
    }
    return m_config.textRender;
}

void MainWindow::SetTextRenderSettings(const TextRenderSettings& settings) noexcept {
    m_config.textRender = settings;
    if (m_backend) {
        m_backend->GetCanvas().SetTextRenderSettings(settings);
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
    if (m_ui.Select(m_shellScaleIndex, kScaleLabels, {.size = UiSize::Sm, .width = 92.0f}).changed) {
        SetShellScaleIndex(m_shellScaleIndex);
    }

    if (m_ui.Switch(m_shellLightMode, {
        .textLeft = "Dark",
        .textRight = "Light",
        .symmetric = true
    }).changed) {
        SetShellLightMode(m_shellLightMode);
    }

    if (m_ui.Switch(m_shellFloating, {
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
    static constexpr keel::StaticArray<keel::StringRef, 5> kHeartbeatShortLabels = {{
        "Off",
        "4 Hz (250 ms)",
        "2 Hz (500 ms)",
        "1 Hz (1000 ms)",
        "0.5 Hz (2000 ms)"
    }};

    const Response engineBtn = tb.Toggle(m_enginePopoverOpen, {
        .text = "Engine",
        .tooltip = "Engine & Render Pipeline Settings",
        .icon = Icon::ChevronDown(),
        .iconPlacement = Placement::Trailing
    });

    if (auto pop = m_ui.Popover(m_enginePopoverOpen, {
        .anchorRect = engineBtn.rect
    })) {
        if (auto card = m_ui.Card({
            .width = 296.0f,
            .gap = 8.0f,
            .padding = Padding::All(14.0f),
            .elevated = true
        })) {
            m_ui.Heading({
                .label = "Engine Pipeline",
                .level = HeadingLevel::H5,
                .separator = HeadingSeparator::Bottom
            });

            bool gpuEnabled = IsGpuShadersEnabled();
            if (auto el = m_ui.Element({
                .label = "GPU Shaders",
                .description = "Hardware shader underlays",
                .layout = ElementLayout::Leading,
                .disabled = !IsGpuShadersAvailable()
            })) {
                if (m_ui.Switch(gpuEnabled, {.size = UiSize::Sm})) {
                    SetGpuShadersEnabled(gpuEnabled);
                }
            }

            bool vsync = IsVsyncEnabled();
            if (auto el = m_ui.Element({
                .label = "VSync",
                .description = "Synchronize swap interval",
                .layout = ElementLayout::Leading
            })) {
                if (m_ui.Switch(vsync, {.size = UiSize::Sm})) {
                    SetVsyncEnabled(vsync);
                }
            }

            if (auto el = m_ui.Element({
                .label = "Reactive Loop",
                .description = "Idle sleep at 0% CPU",
                .layout = ElementLayout::Leading
            })) {
                if (m_ui.Switch(m_reactiveMode, {.size = UiSize::Sm})) {
                    SetReactiveMode(m_reactiveMode);
                }
            }

            bool dirtyActive = m_ui.IsDirtyRectEnabled();
            if (auto el = m_ui.Element({
                .label = "Dirty-Rect Raster",
                .description = "Disjoint partial redraws",
                .layout = ElementLayout::Leading
            })) {
                if (m_ui.Switch(dirtyActive, {.size = UiSize::Sm})) {
                    m_ui.SetDirtyRectEnabled(dirtyActive);
                    if (!dirtyActive) {
                        m_ui.InvalidateAll();
                    }
                }
            }

            bool liveResizeActive = m_ui.IsLiveResizeEnabled();
            if (auto el = m_ui.Element({
                .label = "Live Window Resize",
                .description = "Reflow while dragging edge",
                .layout = ElementLayout::Leading
            })) {
                if (m_ui.Switch(liveResizeActive, {.size = UiSize::Sm})) {
                    m_ui.SetLiveResize(liveResizeActive);
                }
            }

            m_ui.Separator();

            if (auto el = m_ui.Element({
                .label = "Idle Heartbeat",
                .layout = ElementLayout::Leading,
                .disabled = !m_reactiveMode
            })) {
                keel::usize hbIdx = m_heartbeatIndex;
                if (m_ui.Select(hbIdx, kHeartbeatShortLabels, {.size = UiSize::Sm, .width = 128.0f})) {
                    m_heartbeatIndex = hbIdx % kHeartbeatMsValues.size();
                    m_ui.SetIdleHeartbeatMs(kHeartbeatMsValues[m_heartbeatIndex]);
                }
            }

            m_ui.Separator();

            m_ui.Heading({
                .label = "Text Rendering",
                .badge = {m_backend ? m_backend->GetCanvas().GetTextEngineName() : keel::StringRef{"Default"}, StateColor::Primary()},
                .level = HeadingLevel::H5
            });

            static constexpr keel::StaticArray<keel::StringRef, 4> kSmoothingLabels = {{
                "Auto",
                "Grayscale",
                "Subpixel RGB",
                "Subpixel BGR"
            }};
            static constexpr keel::StaticArray<keel::StringRef, 3> kHintingLabels = {{
                "None",
                "Light",
                "Normal"
            }};

            TextRenderSettings textCfg = GetTextRenderSettings();
            const bool supportsHinting = m_backend && m_backend->GetCanvas().SupportsFontHinting();
            bool textChanged = false;

            if (auto el = m_ui.Element({
                .label = "Smoothing",
                .layout = ElementLayout::Leading
            })) {
                keel::usize smoothIdx = keel::as<keel::usize>(textCfg.smoothing);
                if (m_ui.Select(smoothIdx, kSmoothingLabels, {.size = UiSize::Sm, .width = 128.0f})) {
                    textCfg.smoothing = keel::from_underlying<FontSmoothing>(smoothIdx % kSmoothingLabels.size());
                    textChanged = true;
                }
            }

            if (auto el = m_ui.Element({
                .label = "Hinting",
                .layout = ElementLayout::Leading,
                .disabled = !supportsHinting
            })) {
                keel::usize hintIdx = keel::as<keel::usize>(textCfg.hinting);
                if (m_ui.Select(hintIdx, kHintingLabels, {.disabled = !supportsHinting, .size = UiSize::Sm, .width = 128.0f})) {
                    textCfg.hinting = keel::from_underlying<FontHinting>(hintIdx % kHintingLabels.size());
                    textChanged = true;
                }
            }

            if (auto el = m_ui.Element({
                .label = "Gamma Blending",
                .description = "Linear sRGB text compositing",
                .layout = ElementLayout::Leading
            })) {
                if (m_ui.Switch(textCfg.gammaCorrectBlending, {.size = UiSize::Sm})) {
                    textChanged = true;
                }
            }

            if (auto el = m_ui.Element({
                .label = "Contrast",
                .layout = ElementLayout::Leading
            })) {
                if (m_ui.Slider(textCfg.contrast, 0.6f, 1.8f, {
                    .valueFormat = "%.2f",
                    .size = UiSize::Sm,
                    .width = 136.0f
                })) {
                    textChanged = true;
                }
            }

            if (auto el = m_ui.Element({
                .label = "Stem Darkening",
                .layout = ElementLayout::Leading
            })) {
                if (m_ui.Slider(textCfg.stemDarkening, 0.0f, 0.45f, {
                    .valueFormat = "%.2f",
                    .size = UiSize::Sm,
                    .width = 136.0f
                })) {
                    textChanged = true;
                }
            }

            if (textChanged) {
                SetTextRenderSettings(textCfg);
            }
        }
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
