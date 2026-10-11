#include <SDL3/SDL.h>
#include <array>
#include <iostream>
#include <string>
#include <vector>
#include "guikit/GuiKit.hpp"
#include "guikit/backend/ThorCanvas.hpp"
#include "guikit/core/FrameMetrics.hpp"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialize SDL3: " << SDL_GetError() << std::endl;
        return 1;
    }

    int winWidth = 1024;
    int winHeight = 720;

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    const char* winTitle = "GuiKit Vector GUI Demo (C++20 Immediate Mode + ThorVG)";

    if (!SDL_CreateWindowAndRenderer(winTitle, winWidth, winHeight,
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY,
                                     &window, &renderer)) {
        std::cerr << "Failed to create SDL window & renderer: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_SetRenderVSync(renderer, 1);
    SDL_StartTextInput(window);

    guikit::Context ui;
    guikit::ThorCanvas canvas;

    std::vector<uint32_t> pixelBuffer(winWidth * winHeight, 0);
    canvas.SetTarget(pixelBuffer.data(), winWidth, winHeight, winWidth);

    SDL_Texture* screenTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        winWidth,
        winHeight
    );

    int counter = 0;
    int activeTabIndex = 0;
    bool showSidebar = true;
    int sidebarTabIndex = 0;
    bool running = true;
    bool vsyncEnabled = true;
    bool reactiveMode = true;
    bool darkMode = true;
    bool showTelemetry = false;
    bool formMode = true;
    float speedDrop = 50.0f;
    std::string sampleId = "Specimen-2026-X";
    size_t baudIndex = 1;
    std::string profileMode = "Standard Performance";
    size_t heartbeatIndex = 2; // Default: 500 ms (2 Hz)
    const std::vector<int> heartbeatMsValues = {-1, 250, 500, 1000, 2000};
    const std::vector<std::string_view> heartbeatLabels = {
        "Heartbeat: Off",
        "Heartbeat: 4 Hz",
        "Heartbeat: 2 Hz",
        "Heartbeat: 1 Hz",
        "Heartbeat: 0.5 Hz"
    };
    size_t scaleIndex = 0; // Default: 100%
    const std::vector<float> scaleValues = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
    const std::vector<std::string_view> scaleLabels = {
        "100%",
        "125%",
        "150%",
        "175%",
        "200%"
    };
    bool enableSensors = true;
    float maxTravel = 150.0f;
    float maxVelocity = 25.0f;
    size_t sensorChannel = 0;
    bool hwEstop = true;
    size_t interlockMode = 1;
    float rampDown = 250.0f;
    float forceLimit = 85.0f;
    float yieldRatio = 0.2f;
    float pGain = 1.25f;
    float iGain = 0.05f;
    float dGain = 0.01f;
    bool diagLogging = true;
    guikit::TabBarStyle tabStyle = guikit::TabBarStyle::Underline;
    guikit::FrameMetrics metrics;

    static const guikit::UiTheme inspectorTheme = guikit::UiTheme::Dark(guikit::Density::Standard);
    guikit::UiTheme appTheme = guikit::UiTheme::Dark(guikit::Density::Standard);

    ui.SetThemeMode(darkMode ? guikit::ThemeMode::Dark : guikit::ThemeMode::Light);
    ui.SetClearColor(appTheme.palette.background);
    ui.SetIdleHeartbeatMs(heartbeatMsValues[heartbeatIndex]);

    auto handleEvent = [&](const SDL_Event& event) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                winWidth = event.window.data1;
                winHeight = event.window.data2;
                pixelBuffer.resize(winWidth * winHeight, 0);
                canvas.SetTarget(pixelBuffer.data(), winWidth, winHeight, winWidth);
                if (screenTexture) {
                    SDL_DestroyTexture(screenTexture);
                }
                screenTexture = SDL_CreateTexture(
                    renderer,
                    SDL_PIXELFORMAT_ABGR8888,
                    SDL_TEXTUREACCESS_STREAMING,
                    winWidth,
                    winHeight
                );
                ui.RequestRepaint(3);
                break;
            }
            case SDL_EVENT_MOUSE_MOTION:
                ui.OnMouseMove({event.motion.x, event.motion.y});
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                ui.OnMouseDown(event.button.button);
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                ui.OnMouseUp(event.button.button);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                ui.OnMouseWheel(event.wheel.x, event.wheel.y);
                break;
            case SDL_EVENT_TEXT_INPUT:
                ui.OnTextInput(event.text.text);
                break;
            case SDL_EVENT_KEY_DOWN: {
                const bool ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
                const bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
                if ((ctrl && shift && event.key.key == SDLK_C) || (ctrl && event.key.key == SDLK_I)) {
                    ui.SetInspectMode(!ui.IsInspectMode());
                    break;
                }
                if (ctrl && event.key.key == SDLK_B) {
                    ui.SetDebugBounds(!ui.IsDebugBounds());
                    break;
                }

                guikit::Key key = guikit::Key::None;
                switch (event.key.key) {
                    case SDLK_BACKSPACE: key = guikit::Key::Backspace; break;
                    case SDLK_DELETE:    key = guikit::Key::Delete; break;
                    case SDLK_RETURN:    key = guikit::Key::Enter; break;
                    case SDLK_ESCAPE:    key = guikit::Key::Escape; break;
                    case SDLK_TAB:       key = guikit::Key::Tab; break;
                    case SDLK_LEFT:      key = guikit::Key::Left; break;
                    case SDLK_RIGHT:     key = guikit::Key::Right; break;
                    case SDLK_UP:        key = guikit::Key::Up; break;
                    case SDLK_DOWN:      key = guikit::Key::Down; break;
                    case SDLK_HOME:      key = guikit::Key::Home; break;
                    case SDLK_END:       key = guikit::Key::End; break;
                    default: break;
                }
                if (key != guikit::Key::None) {
                    ui.OnKeyDown(key);
                }
                break;
            }
            default:
                break;
        }
    };

    while (running) {
        if (reactiveMode && !ui.NeedsRepaint()) {
            const int timeoutMs = ui.GetRecommendedSleepTimeoutMs();
            SDL_Event event;
            if (SDL_WaitEventTimeout(&event, timeoutMs)) {
                handleEvent(event);
                while (SDL_PollEvent(&event)) {
                    handleEvent(event);
                }
            } else {
                // Timeout expired with no events.
                // If timeoutMs <= 0 (disabled periodic heartbeat / deep sleep), continue waiting.
                // If timeoutMs > 0, the periodic heartbeat fired: proceed to render 1 frame to refresh metrics.
                if (timeoutMs <= 0 && !ui.NeedsRepaint()) {
                    continue;
                }
            }
        } else {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                handleEvent(event);
            }
        }

        if (!running) {
            break;
        }

        const auto frameWorkStart = std::chrono::steady_clock::now();
        metrics.BeginFrame();

        // --- Begin Immediate Mode GuiKit Frame ---
        ui.BeginFrame({static_cast<float>(winWidth), static_cast<float>(winHeight)}, 1.0f);
        {
            if (auto mainCol = ui.Column()) {
                // Header Bar
                if (auto header = ui.Row({.align = guikit::Alignment::Center})) {
                    ui.Text("GUIKIT GUI", {.role = guikit::TextRole::TitleLarge, .variant = guikit::UiVariant::Info});
                    ui.Text("| Autonomous Vector GUI Engine", {.role = guikit::TextRole::LabelMedium, .variant = guikit::UiVariant::Muted});
                    ui.Spacer(); // Push build badge to right
                    ui.Text("C++20 Immediate + ThorVG", {.role = guikit::TextRole::LabelMedium, .variant = guikit::UiVariant::Muted});
                }

                // Engine HUD Toolbar
                if (auto tb = ui.Toolbar()) {
                    // Density Preset Toggle
                    const char* densityLabel = (appTheme.density == guikit::Density::Standard) ? "Density: Standard" :
                                              ((appTheme.density == guikit::Density::Compact) ? "Density: Compact" : "Density: Touch");
                    if (tb.ToolButton({
                        .label = densityLabel,
                        .icon = guikit::Icon::Settings(),
                        .labelVariants = {"Density: Standard", "Density: Compact", "Density: Touch"},
                        .active = true
                    })) {
                        if (appTheme.density == guikit::Density::Standard) {
                            appTheme.density = guikit::Density::Compact;
                            appTheme.metrics = (darkMode ? guikit::UiTheme::Dark(guikit::Density::Compact) : guikit::UiTheme::Light(guikit::Density::Compact)).metrics;
                        } else if (appTheme.density == guikit::Density::Compact) {
                            appTheme.density = guikit::Density::Touch;
                            appTheme.metrics = (darkMode ? guikit::UiTheme::Dark(guikit::Density::Touch) : guikit::UiTheme::Light(guikit::Density::Touch)).metrics;
                        } else {
                            appTheme.density = guikit::Density::Standard;
                            appTheme.metrics = (darkMode ? guikit::UiTheme::Dark(guikit::Density::Standard) : guikit::UiTheme::Light(guikit::Density::Standard)).metrics;
                        }
                        ui.InvalidateAll();
                    }

                    tb.Separator();

                    // Scale Preset Combo
                    if (auto scaleBox = ui.Box({.fixedWidth = 92.0f})) {
                        if (ui.Select(scaleIndex, scaleLabels, {.size = guikit::UiSize::Sm}).changed) {
                            ui.InvalidateAll();
                        }
                    }

                    tb.Separator();

                    // Theme Mode Toggle
                    if (tb.ToolButton({
                        .label = darkMode ? "Theme: Dark" : "Theme: Light",
                        .icon = darkMode ? guikit::Icon::Eye() : guikit::Icon::EyeOff(),
                        .labelVariants = {"Theme: Dark", "Theme: Light"},
                        .active = darkMode
                    })) {
                        darkMode = !darkMode;
                        const auto defTheme = darkMode ? guikit::UiTheme::Dark(appTheme.density) : guikit::UiTheme::Light(appTheme.density);
                        appTheme.palette = defTheme.palette;
                        appTheme.variants = defTheme.variants;
                        ui.SetClearColor(appTheme.palette.background);
                        ui.InvalidateAll();
                    }

                    tb.Separator();

                    // Sidebar Inspector Toggle
                    if (tb.ToolButton({
                        .label = showSidebar ? "Sidebar: ON" : "Sidebar: OFF",
                        .icon = guikit::Icon::Settings(),
                        .variant = showSidebar ? guikit::UiVariant::Primary : guikit::UiVariant::Ghost,
                        .labelVariants = {"Sidebar: ON", "Sidebar: OFF"},
                        .active = showSidebar
                    })) {
                        showSidebar = !showSidebar;
                        ui.InvalidateAll();
                    }

                    tb.Separator();

                    // VSync Toggle
                    if (tb.ToolButton({
                        .label = vsyncEnabled ? "VSync: ON" : "VSync: OFF",
                        .icon = guikit::Icon::Refresh(),
                        .variant = vsyncEnabled ? guikit::UiVariant::Success : guikit::UiVariant::Ghost,
                        .labelVariants = {"VSync: ON", "VSync: OFF"},
                        .active = vsyncEnabled
                    })) {
                        vsyncEnabled = !vsyncEnabled;
                        SDL_SetRenderVSync(renderer, vsyncEnabled ? 1 : 0);
                    }

                    // Reactive Mode Toggle
                    if (tb.ToolButton({
                        .label = reactiveMode ? "Reactive: ON" : "Reactive: OFF",
                        .icon = reactiveMode ? guikit::Icon::Play() : guikit::Icon::Pause(),
                        .variant = reactiveMode ? guikit::UiVariant::Primary : guikit::UiVariant::Ghost,
                        .labelVariants = {"Reactive: ON", "Reactive: OFF"},
                        .active = reactiveMode
                    })) {
                        reactiveMode = !reactiveMode;
                        if (!reactiveMode) {
                            ui.InvalidateAll();
                        }
                    }

                    // Dirty Rect Toggle
                    const bool dirtyActive = ui.IsDirtyRectEnabled();
                    if (tb.ToolButton({
                        .label = dirtyActive ? "Dirty: ON" : "Dirty: OFF",
                        .icon = guikit::Icon::Settings(),
                        .variant = dirtyActive ? guikit::UiVariant::Info : guikit::UiVariant::Ghost,
                        .labelVariants = {"Dirty: ON", "Dirty: OFF"},
                        .active = dirtyActive
                    })) {
                        const bool nextDirty = !dirtyActive;
                        ui.SetDirtyRectEnabled(nextDirty);
                        if (!nextDirty) {
                            ui.InvalidateAll();
                        }
                    }

                    tb.Separator();

                    // Debug Bounds Toggle
                    const bool boundsActive = ui.IsDebugBounds();
                    if (tb.ToolButton({
                        .label = boundsActive ? "Bounds: ON" : "Bounds: OFF",
                        .icon = boundsActive ? guikit::Icon::Eye() : guikit::Icon::EyeOff(),
                        .variant = boundsActive ? guikit::UiVariant::Info : guikit::UiVariant::Ghost,
                        .labelVariants = {"Bounds: ON", "Bounds: OFF"},
                        .active = boundsActive
                    })) {
                        ui.SetDebugBounds(!boundsActive);
                    }

                    // Inspect Mode Toggle (Chrome DevTools Inspect element)
                    const bool inspectActive = ui.IsInspectMode();
                    if (tb.ToolButton({
                        .label = inspectActive ? "Inspect: ON" : "Inspect: OFF",
                        .icon = guikit::Icon::Search(),
                        .variant = inspectActive ? guikit::UiVariant::Warning : guikit::UiVariant::Ghost,
                        .labelVariants = {"Inspect: ON", "Inspect: OFF"},
                        .active = inspectActive
                    })) {
                        ui.SetInspectMode(!inspectActive);
                    }

                    // Idle Heartbeat Presets Cycle
                    if (tb.ToolButton({
                        .label = heartbeatLabels[heartbeatIndex],
                        .icon = guikit::Icon::Plus(),
                        .labelVariants = heartbeatLabels,
                        .disabled = !reactiveMode
                    })) {
                        heartbeatIndex = (heartbeatIndex + 1) % heartbeatLabels.size();
                        ui.SetIdleHeartbeatMs(heartbeatMsValues[heartbeatIndex]);
                    }

                    tb.Spacer();

                    // TabBar Style Toggle
                    if (tb.ToolButton({
                        .label = tabStyle == guikit::TabBarStyle::Underline ? "Tabs: Underline" : "Tabs: Pill",
                        .icon = guikit::Icon::Folder(),
                        .labelVariants = {"Tabs: Underline", "Tabs: Pill"}
                    })) {
                        tabStyle = (tabStyle == guikit::TabBarStyle::Underline)
                            ? guikit::TabBarStyle::Pill
                            : guikit::TabBarStyle::Underline;
                    }
                }

                // Workspace Row containing Sidebar (left) and Content Area (right)
                if (auto workspaceRow = ui.Row({.gap = guikit::UiSize::Md, .heightMode = guikit::SizeMode::Stretch})) {
                    // Collapsible Style Inspector Sidebar
                    if (showSidebar) {
                        if (auto sidebarBox = ui.Box({
                            .heightMode = guikit::SizeMode::Stretch,
                            .fixedWidth = 300.0f,
                            .theme = &inspectorTheme,
                            .scale = 1.0f
                        })) {
                            if (auto sidebarCard = ui.Card({.gap = guikit::UiSize::Sm, .heightMode = guikit::SizeMode::Stretch})) {
                                ui.Heading({
                                    .title = "Style Inspector",
                                    .subtitle = "Live Token Tuning",
                                    .icon = guikit::Icon::Settings()
                                });

                                static const std::array<guikit::TabItem, 2> sidebarTabs = {{
                                    {.label = "Geometry", .icon = guikit::Icon::Settings()},
                                    {.label = "Colors", .icon = guikit::Icon::Eye()}
                                }};
                                ui.TabBar(sidebarTabIndex, sidebarTabs, {
                                    .style = guikit::TabBarStyle::Pill,
                                    .fullWidth = true
                                });

                                if (auto sidebarScroll = ui.ScrollArea({.gap = guikit::UiSize::Sm})) {
                                    auto& th = appTheme;
                                    if (sidebarTabIndex == 0) {
                                        // === GEOMETRY TAB ===
                                        ui.Text("Density Scale", {.role = guikit::TextRole::LabelLarge, .variant = guikit::UiVariant::Info});
                                        if (auto row = ui.Row({.gap = guikit::UiSize::Xs})) {
                                            if (ui.Button({
                                                .label = "Standard",
                                                .variant = (appTheme.density == guikit::Density::Standard) ? guikit::UiVariant::Primary : guikit::UiVariant::Default,
                                                .stretch = true
                                            })) {
                                                appTheme.density = guikit::Density::Standard;
                                                appTheme.metrics = (darkMode ? guikit::UiTheme::Dark(guikit::Density::Standard) : guikit::UiTheme::Light(guikit::Density::Standard)).metrics;
                                                ui.InvalidateAll();
                                            }
                                            if (ui.Button({
                                                .label = "Compact",
                                                .variant = (appTheme.density == guikit::Density::Compact) ? guikit::UiVariant::Primary : guikit::UiVariant::Default,
                                                .stretch = true
                                            })) {
                                                appTheme.density = guikit::Density::Compact;
                                                appTheme.metrics = (darkMode ? guikit::UiTheme::Dark(guikit::Density::Compact) : guikit::UiTheme::Light(guikit::Density::Compact)).metrics;
                                                ui.InvalidateAll();
                                            }
                                            if (ui.Button({
                                                .label = "Touch",
                                                .variant = (appTheme.density == guikit::Density::Touch) ? guikit::UiVariant::Primary : guikit::UiVariant::Default,
                                                .stretch = true
                                            })) {
                                                appTheme.density = guikit::Density::Touch;
                                                appTheme.metrics = (darkMode ? guikit::UiTheme::Dark(guikit::Density::Touch) : guikit::UiTheme::Light(guikit::Density::Touch)).metrics;
                                                ui.InvalidateAll();
                                            }
                                        }

                                        ui.Text("Corner Radius", {.role = guikit::TextRole::LabelLarge, .variant = guikit::UiVariant::Info});
                                        if (auto row = ui.Row({.gap = guikit::UiSize::Xs})) {
                                            if (ui.Button({.label = "Sharp (0px)", .stretch = true})) {
                                                th.radius = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
                                                ui.InvalidateAll();
                                            }
                                            if (ui.Button({.label = "Default (6px)", .stretch = true})) {
                                                th.radius = {0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 12.0f};
                                                ui.InvalidateAll();
                                            }
                                            if (ui.Button({.label = "Pill (14px)", .stretch = true})) {
                                                th.radius = {0.0f, 4.0f, 8.0f, 14.0f, 20.0f, 28.0f};
                                                ui.InvalidateAll();
                                            }
                                        }
                                        if (auto grid = ui.Grid()) {
                                            if (auto c1 = grid.Cell4()) {
                                                if (ui.InputField(th.radius[2], {.label = "Radius Sm", .suffix = "px", .format = "%.0f"}).changed) {
                                                    ui.InvalidateAll();
                                                }
                                            }
                                            if (auto c2 = grid.Cell4()) {
                                                if (ui.InputField(th.radius[3], {.label = "Radius Md", .suffix = "px", .format = "%.0f"}).changed) {
                                                    ui.InvalidateAll();
                                                }
                                            }
                                            if (auto c3 = grid.Cell4()) {
                                                if (ui.InputField(th.radius[4], {.label = "Radius Lg", .suffix = "px", .format = "%.0f"}).changed) {
                                                    ui.InvalidateAll();
                                                }
                                            }
                                        }

                                        ui.Text("Spacing & Gaps", {.role = guikit::TextRole::LabelLarge, .variant = guikit::UiVariant::Info});
                                        if (auto row = ui.Row({.gap = guikit::UiSize::Xs})) {
                                            if (ui.Button({.label = "Tight", .stretch = true})) {
                                                th.spacing = {0.0f, 2.0f, 4.0f, 8.0f, 12.0f, 16.0f};
                                                ui.InvalidateAll();
                                            }
                                            if (ui.Button({.label = "Normal", .stretch = true})) {
                                                th.spacing = {0.0f, 4.0f, 8.0f, 12.0f, 16.0f, 24.0f};
                                                ui.InvalidateAll();
                                            }
                                            if (ui.Button({.label = "Spacious", .stretch = true})) {
                                                th.spacing = {0.0f, 6.0f, 12.0f, 18.0f, 26.0f, 36.0f};
                                                ui.InvalidateAll();
                                            }
                                        }
                                        if (auto grid = ui.Grid()) {
                                            if (auto c1 = grid.Cell4()) {
                                                if (ui.InputField(th.spacing[2], {.label = "Gap Sm", .suffix = "px", .format = "%.0f"}).changed) {
                                                    ui.InvalidateAll();
                                                }
                                            }
                                            if (auto c2 = grid.Cell4()) {
                                                if (ui.InputField(th.spacing[3], {.label = "Gap Md", .suffix = "px", .format = "%.0f"}).changed) {
                                                    ui.InvalidateAll();
                                                }
                                            }
                                            if (auto c3 = grid.Cell4()) {
                                                if (ui.InputField(th.spacing[4], {.label = "Gap Lg", .suffix = "px", .format = "%.0f"}).changed) {
                                                    ui.InvalidateAll();
                                                }
                                            }
                                        }

                                        ui.Text("Control Heights", {.role = guikit::TextRole::LabelLarge, .variant = guikit::UiVariant::Info});
                                        if (auto grid = ui.Grid()) {
                                            if (auto c1 = grid.Cell4()) {
                                                if (ui.InputField(th.GetMetrics(guikit::UiSize::Sm).defaultHeight, {.label = "Height Sm", .suffix = "px", .format = "%.0f"}).changed) {
                                                    ui.InvalidateAll();
                                                }
                                            }
                                            if (auto c2 = grid.Cell4()) {
                                                if (ui.InputField(th.GetMetrics(guikit::UiSize::Md).defaultHeight, {.label = "Height Md", .suffix = "px", .format = "%.0f"}).changed) {
                                                    ui.InvalidateAll();
                                                }
                                            }
                                            if (auto c3 = grid.Cell4()) {
                                                if (ui.InputField(th.GetMetrics(guikit::UiSize::Lg).defaultHeight, {.label = "Height Lg", .suffix = "px", .format = "%.0f"}).changed) {
                                                    ui.InvalidateAll();
                                                }
                                            }
                                        }

                                        ui.Text("Typography Line Height", {.role = guikit::TextRole::LabelLarge, .variant = guikit::UiVariant::Info});
                                        if (ui.InputField(th.lineHeightMultiplier, {.label = "Line Height Ratio", .format = "%.2f"}).changed) {
                                            ui.InvalidateAll();
                                        }

                                        if (ui.Button({
                                            .label = "Reset Geometry Defaults",
                                            .icon = guikit::Icon::Refresh(),
                                            .variant = guikit::UiVariant::Secondary,
                                            .stretch = true
                                        })) {
                                            const auto defTheme = darkMode ? guikit::UiTheme::Dark(guikit::Density::Standard) : guikit::UiTheme::Light(guikit::Density::Standard);
                                            th.spacing = defTheme.spacing;
                                            th.radius = defTheme.radius;
                                            th.metrics = defTheme.metrics;
                                            th.density = guikit::Density::Standard;
                                            th.lineHeightMultiplier = guikit::kDefaultLineHeightMultiplier;
                                            ui.InvalidateAll();
                                        }
                                    } else {
                                        // === COLORS TAB ===
                                        if (ui.Switch(darkMode, {
                                            .label = "Dark Mode",
                                            .description = "Switch between Dark and Light palette"
                                        }).changed) {
                                            const auto defTheme = darkMode ? guikit::UiTheme::Dark(appTheme.density) : guikit::UiTheme::Light(appTheme.density);
                                            appTheme.palette = defTheme.palette;
                                            appTheme.variants = defTheme.variants;
                                            ui.SetClearColor(appTheme.palette.background);
                                            ui.InvalidateAll();
                                        }

                                        ui.Text("Brand Accent", {.role = guikit::TextRole::LabelLarge, .variant = guikit::UiVariant::Info});
                                        auto setAccent = [&](guikit::Color accent, guikit::Color hover, guikit::Color active) {
                                            th.palette.accent = accent;
                                            th.palette.accentHover = hover;
                                            th.palette.focusRing = accent;
                                            th.palette.caret = accent;
                                            auto& prim = th.GetVariant(guikit::UiVariant::Primary);
                                            prim.bg = accent;
                                            prim.bgHover = hover;
                                            prim.bgActive = active;
                                            ui.InvalidateAll();
                                        };

                                        if (auto grid = ui.Grid()) {
                                            if (auto c = grid.Cell4()) {
                                                if (ui.Button({.label = "Sky Blue", .variant = guikit::UiVariant::Primary, .stretch = true})) {
                                                    setAccent(guikit::Color::Hex(0x38BDF8FF), guikit::Color::Hex(0x60A5FAFF), guikit::Color::Hex(0x2563EBFF));
                                                }
                                            }
                                            if (auto c = grid.Cell4()) {
                                                if (ui.Button({.label = "Emerald", .variant = guikit::UiVariant::Success, .stretch = true})) {
                                                    setAccent(guikit::Color::Hex(0x10B981FF), guikit::Color::Hex(0x34D399FF), guikit::Color::Hex(0x059669FF));
                                                }
                                            }
                                            if (auto c = grid.Cell4()) {
                                                if (ui.Button({.label = "Violet", .stretch = true})) {
                                                    setAccent(guikit::Color::Hex(0x8B5CF6FF), guikit::Color::Hex(0xA78BFAFF), guikit::Color::Hex(0x7C3AEDFF));
                                                }
                                            }
                                            if (auto c = grid.Cell4()) {
                                                if (ui.Button({.label = "Amber", .variant = guikit::UiVariant::Warning, .stretch = true})) {
                                                    setAccent(guikit::Color::Hex(0xF59E0BFF), guikit::Color::Hex(0xFBBF24FF), guikit::Color::Hex(0xD97706FF));
                                                }
                                            }
                                            if (auto c = grid.Cell4()) {
                                                if (ui.Button({.label = "Rose", .variant = guikit::UiVariant::Danger, .stretch = true})) {
                                                    setAccent(guikit::Color::Hex(0xF43F5EFF), guikit::Color::Hex(0xFB7185FF), guikit::Color::Hex(0xE11D48FF));
                                                }
                                            }
                                            if (auto c = grid.Cell4()) {
                                                if (ui.Button({.label = "Cyan", .variant = guikit::UiVariant::Info, .stretch = true})) {
                                                    setAccent(guikit::Color::Hex(0x06B6D4FF), guikit::Color::Hex(0x22D3EEFF), guikit::Color::Hex(0x0891B2FF));
                                                }
                                            }
                                        }

                                        ui.Text("Dark Surface Tones", {.role = guikit::TextRole::LabelLarge, .variant = guikit::UiVariant::Info});
                                        auto setSurfaceTone = [&](guikit::Color bg, guikit::Color surf, guikit::Color elev, guikit::Color bdr) {
                                            th.palette.background = bg;
                                            th.palette.surface = surf;
                                            th.palette.surfaceElevated = elev;
                                            th.palette.border = bdr;
                                            th.palette.inputBackground = bg;
                                            ui.SetClearColor(bg);
                                            ui.InvalidateAll();
                                        };

                                        if (auto grid = ui.Grid()) {
                                            if (auto c = grid.Cell6()) {
                                                if (ui.Button({.label = "Slate (Default)", .stretch = true})) {
                                                    setSurfaceTone(guikit::Color::Hex(0x0B0F19FF), guikit::Color::Hex(0x1E293BFF), guikit::Color::Hex(0x1E293BFF), guikit::Color::Hex(0x334155FF));
                                                }
                                            }
                                            if (auto c = grid.Cell6()) {
                                                if (ui.Button({.label = "Zinc Gray", .stretch = true})) {
                                                    setSurfaceTone(guikit::Color::Hex(0x18181BFF), guikit::Color::Hex(0x27272AFF), guikit::Color::Hex(0x27272AFF), guikit::Color::Hex(0x3F3F46FF));
                                                }
                                            }
                                            if (auto c = grid.Cell6()) {
                                                if (ui.Button({.label = "OLED Black", .stretch = true})) {
                                                    setSurfaceTone(guikit::Color::Hex(0x000000FF), guikit::Color::Hex(0x121212FF), guikit::Color::Hex(0x181818FF), guikit::Color::Hex(0x27272AFF));
                                                }
                                            }
                                            if (auto c = grid.Cell6()) {
                                                if (ui.Button({.label = "Deep Navy", .stretch = true})) {
                                                    setSurfaceTone(guikit::Color::Hex(0x0A0F1DFF), guikit::Color::Hex(0x151E32FF), guikit::Color::Hex(0x1C2844FF), guikit::Color::Hex(0x263554FF));
                                                }
                                            }
                                        }

                                        ui.Text("Border Contrast", {.role = guikit::TextRole::LabelLarge, .variant = guikit::UiVariant::Info});
                                        if (auto row = ui.Row({.gap = guikit::UiSize::Xs})) {
                                            if (ui.Button({.label = "Subtle", .stretch = true})) {
                                                th.palette.border = guikit::Color::Hex(0x33415560);
                                                ui.InvalidateAll();
                                            }
                                            if (ui.Button({.label = "Default", .stretch = true})) {
                                                th.palette.border = guikit::Color::Hex(0x334155FF);
                                                ui.InvalidateAll();
                                            }
                                            if (ui.Button({.label = "High", .stretch = true})) {
                                                th.palette.border = guikit::Color::Hex(0x64748BFF);
                                                ui.InvalidateAll();
                                            }
                                        }

                                        if (ui.Button({
                                            .label = "Reset Color Defaults",
                                            .icon = guikit::Icon::Refresh(),
                                            .variant = guikit::UiVariant::Secondary,
                                            .stretch = true
                                        })) {
                                            const auto defTheme = darkMode ? guikit::UiTheme::Dark(appTheme.density) : guikit::UiTheme::Light(appTheme.density);
                                            th.palette = defTheme.palette;
                                            th.variants = defTheme.variants;
                                            ui.SetClearColor(th.palette.background);
                                            ui.InvalidateAll();
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Content Column (Right side workspace)
                    if (auto contentCol = ui.Column({
                        .gap = guikit::UiSize::Sm,
                        .heightMode = guikit::SizeMode::Stretch,
                        .theme = &appTheme,
                        .scale = scaleValues[scaleIndex]
                    })) {
                        // First-class Vector TabBar with Carousel auto-scrolling
                        static const std::array<guikit::TabItem, 7> navTabs = {{
                            {.label = "Overview", .icon = guikit::Icon::Eye()},
                            {.label = "Flexbox Demo", .icon = guikit::Icon::Settings()},
                            {.label = "Industrial Dashboard", .icon = guikit::Icon::Folder(), .badge = "Live"},
                            {.label = "Telemetry & Logs", .icon = guikit::Icon::Play()},
                            {.label = "Network Diagnostics", .icon = guikit::Icon::Plus()},
                            {.label = "Calibration Matrix", .icon = guikit::Icon::Settings()},
                            {.label = "System Audit", .icon = guikit::Icon::Trash()}
                        }};
                        ui.TabBar(activeTabIndex, navTabs, {.style = tabStyle});

                        if (activeTabIndex == 2) {
                            // Industrial Dashboard: responsive 2-column layout inside a dedicated ScrollArea
                            if (auto scroll = ui.ScrollArea()) {
                                if (auto dashboardGrid = ui.Grid()) {
                                    // Left Column: wraps to full width when narrow
                                    if (auto colLeft = dashboardGrid.Cell6()) {
                                        if (auto card1 = ui.Card()) {
                                            ui.Heading({.title = "Primary Stop Conditions", .subtitle = "Sensory tripwire thresholds & limits"});
                                            if (auto grid1 = ui.Grid()) {
                                                if (auto c = grid1.Cell12()) {
                                                    ui.Switch(enableSensors, {
                                                        .label = "Enable Sensor Drop Limits",
                                                        .description = "Immediate stop on threshold breach"
                                                    });
                                                }
                                                if (auto c = grid1.Cell6()) {
                                                    ui.InputField(maxTravel, {.label = "Max Travel", .suffix = "mm", .format = "%.1f"});
                                                }
                                                if (auto c = grid1.Cell6()) {
                                                    ui.InputField(maxVelocity, {.label = "Max Velocity", .suffix = "mm/s", .format = "%.1f"});
                                                }
                                                if (auto c = grid1.Cell12()) {
                                                    ui.Select(sensorChannel, {
                                                        "Channel A (Load Cell 50kN)",
                                                        "Channel B (Optical Extensometer)",
                                                        "Channel C (Laser Interferometer)"
                                                    }, {
                                                        .label = "Primary Sensor Channel",
                                                        .description = "Active physical transducer bus",
                                                        .labelPlacement = guikit::Placement::Stacked
                                                    });
                                                }
                                            }
                                        }

                                        if (auto card2 = ui.Card()) {
                                            ui.Heading({.title = "Emergency Interlocks", .subtitle = "Hardware line supervision & failsafes", .variant = guikit::UiVariant::Danger});
                                            if (auto grid2 = ui.Grid()) {
                                                if (auto c = grid2.Cell12()) {
                                                    ui.Switch(hwEstop, {
                                                        .label = "Hardware E-Stop Line",
                                                        .description = "Continuous 24V loop monitoring",
                                                        .variant = guikit::UiVariant::Danger
                                                    });
                                                }
                                                if (auto c = grid2.Cell6()) {
                                                    ui.Select(interlockMode, {
                                                        "Immediate Cutoff",
                                                        "Controlled Ramp-down",
                                                        "Hold Position"
                                                    }, {
                                                        .label = "Interlock Action"
                                                    });
                                                }
                                                if (auto c = grid2.Cell6()) {
                                                    ui.InputField(rampDown, {.label = "Ramp Duration", .suffix = "ms", .format = "%.0f"});
                                                }
                                            }
                                        }
                                    }

                                    // Right Column: wraps to full width when narrow
                                    if (auto colRight = dashboardGrid.Cell6()) {
                                        if (auto card3 = ui.Card()) {
                                            ui.Heading({.title = "Secondary Criteria & PID", .subtitle = "Dynamic closed-loop feedback parameters"});
                                            if (auto grid3 = ui.Grid()) {
                                                if (auto c = grid3.Cell6()) {
                                                    ui.InputField(forceLimit, {.label = "Force Drop Limit", .suffix = "%", .format = "%.1f"});
                                                }
                                                if (auto c = grid3.Cell6()) {
                                                    ui.InputField(yieldRatio, {.label = "Yield Ratio", .suffix = "%", .format = "%.2f"});
                                                }
                                                if (auto c = grid3.Cell4()) {
                                                    ui.InputField(pGain, {.label = "P-Gain", .format = "%.2f"});
                                                }
                                                if (auto c = grid3.Cell4()) {
                                                    ui.InputField(iGain, {.label = "I-Gain", .format = "%.3f"});
                                                }
                                                if (auto c = grid3.Cell4()) {
                                                    ui.InputField(dGain, {.label = "D-Gain", .format = "%.3f"});
                                                }
                                                if (auto c = grid3.Cell12()) {
                                                    ui.Switch(diagLogging, {
                                                        .label = "Log High-Speed Telemetry",
                                                        .description = "Record 10 kHz buffer to internal flash"
                                                    });
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        } else if (activeTabIndex == 0) {
                            // Overview Tab: Interactive Counter & Form
                            if (auto card = ui.Card()) {
                                ui.Heading({.title = "Interactive Reactive State", .subtitle = "Pure immediate mode with zero external widget IDs and instant response."});

                                if (auto actionRow = ui.Wrap({.align = guikit::Alignment::Center})) {
                                    if (ui.Button({.label = "Increment (+1)", .icon = guikit::Icon::Plus(), .variant = guikit::UiVariant::Success})) {
                                        ++counter;
                                    }
                                    if (ui.Button({.label = "Decrement (-1)", .icon = guikit::Icon::Minus(), .variant = guikit::UiVariant::Warning})) {
                                        --counter;
                                    }
                                    if (ui.Button({.label = "Reset", .icon = guikit::Icon::Refresh(), .variant = guikit::UiVariant::Danger})) {
                                        counter = 0;
                                    }

                                    ui.Text(ui.Format("Value: {}", counter), {.role = guikit::TextRole::HeadlineMedium, .variant = guikit::UiVariant::Info});
                                }

                                if (auto togglesRow = ui.Wrap({.gap = guikit::UiSize::Lg, .align = guikit::Alignment::Center})) {
                                    ui.Checkbox(showTelemetry, {.label = "Show Telemetry", .variant = guikit::UiVariant::Info});
                                    if (ui.Switch(formMode, {
                                        .label = "Form Mode",
                                        .description = "Toggle stacked vs default label layout",
                                        .labelPlacement = guikit::Placement::Stacked
                                        }).changed) {
                                        ui.InvalidateAll();
                                    }
                                    ui.Checkbox(true, {.label = "Auto-Scale", .disabled = true});
                                }

                                if (showTelemetry) {
                                    ui.Heading({
                                        .title = "Telemetry Stream",
                                        .subtitle = "1000 samples/sec | Jitter < 0.2ms | Hardware loop nominal",
                                        .icon = guikit::Icon::Check(),
                                        .badge = "ACTIVE",
                                        .variant = guikit::UiVariant::Success
                                    });
                                }

                                const guikit::Placement activePlacement = formMode ? guikit::Placement::Stacked : guikit::Placement::Leading;

                                if (auto fieldsGrid = ui.Grid()) {
                                    if (auto c1 = fieldsGrid.Cell3()) {
                                        ui.InputField(speedDrop, {
                                            .label = "Speed Drop dF/dt",
                                            .description = "Automatic stop of testing on load drop",
                                            .suffix = "%",
                                            .format = "%.0f",
                                            .labelPlacement = activePlacement
                                        });
                                    }
                                    if (auto c2 = fieldsGrid.Cell3()) {
                                        ui.InputField(sampleId, {
                                            .label = "Sample Identifier",
                                            .description = "Unique test specimen code",
                                            .placeholder = "Enter sample code...",
                                            .labelPlacement = activePlacement
                                        });
                                    }
                                    if (auto c3 = fieldsGrid.Cell3()) {
                                        ui.Select(baudIndex, {"9600 bps", "19200 bps", "38400 bps", "115200 bps"}, {
                                            .label = "Baud Rate",
                                            .description = "Serial communication bus rate",
                                            .labelPlacement = activePlacement
                                        });
                                    }
                                    if (auto c4 = fieldsGrid.Cell3()) {
                                        ui.Select(profileMode, {"Eco Saver", "Standard Performance", "Industrial High-Speed", "Extreme Calibration"}, {
                                            .label = "Operation Profile",
                                            .description = "Active runtime sensor & compute profile",
                                            .labelPlacement = activePlacement
                                        });
                                    }
                                }
                            }
                        } else if (activeTabIndex == 1) {
                            // Flexbox Demo Tab: Equal Stretch & Alignment
                            if (auto card = ui.Card()) {
                                ui.Heading({.title = "Dynamic 3-Way Equal Stretch Layout", .subtitle = "Each button automatically receives exactly 33.3% available width via stretch = true."});

                                if (auto stretchRow = ui.Row()) {
                                    ui.Button({.label = "Col 1 (Stretch)", .stretch = true});
                                    ui.Button({.label = "Col 2 (Stretch)", .stretch = true});
                                    ui.Button({.label = "Col 3 (Stretch)", .stretch = true});
                                }

                                ui.Heading({.title = "Horizontal Carousel Ribbon", .subtitle = "Autonomous scroll container with navigation arrows and mouse wheel support."});
                                if (auto ribbon = ui.Carousel({.gap = guikit::UiSize::Sm})) {
                                    for (int k = 1; k <= 10; ++k) {
                                        ui.Button({
                                            .label = ui.Format("Module #{:02d}", k),
                                            .variant = (k % 3 == 0) ? guikit::UiVariant::Primary : guikit::UiVariant::Default
                                        });
                                    }
                                }
                            }
                        } else {
                            // Diagnostics & Audit Tabs
                            if (auto card = ui.Card()) {
                                ui.Heading({
                                    .title = navTabs[activeTabIndex].label,
                                    .subtitle = "Operational module telemetry stream and live system diagnostics.",
                                    .icon = navTabs[activeTabIndex].icon
                                });
                                if (auto grid = ui.Grid()) {
                                    if (auto c1 = grid.Cell6()) {
                                        ui.Text(ui.Format("Channel Status: ONLINE | Active node: [{}]", navTabs[activeTabIndex].label),
                                                {.role = guikit::TextRole::BodyMedium, .variant = guikit::UiVariant::Success});
                                    }
                                    if (auto c2 = grid.Cell6()) {
                                        ui.Text("Latency: 0.18 ms | Packet Drop: 0.00% | CRC: OK",
                                                {.role = guikit::TextRole::CodeMedium, .variant = guikit::UiVariant::Info});
                                    }
                                }
                            }
                        }

                        // Vertical Spacer pushing the footer down (only for non-stretching tabs)
                        if (activeTabIndex != 2) {
                            ui.Spacer();
                        }
                    }
                }

                // Sticky Footer Bar
                if (auto footer = ui.Row({.align = guikit::Alignment::Center})) {
                    ui.Text(metrics.GetStatusString(), {.role = guikit::TextRole::LabelMedium, .variant = guikit::UiVariant::Muted});
                    ui.Spacer();
                    if (ui.Button({.label = "Exit", .icon = guikit::Icon::Close(), .variant = guikit::UiVariant::Ghost})) {
                        running = false;
                    }
                }
            }
        }
        ui.EndFrame(canvas);
        metrics.MarkCpuDone();

        const bool shouldPresent = ui.HasDirtyDamage() || !reactiveMode;
        if (shouldPresent) {
            metrics.MarkGpuStart();
            if (ui.HasDirtyDamage()) {
                if (ui.IsFullRepaint()) {
                    SDL_UpdateTexture(screenTexture, nullptr, pixelBuffer.data(), winWidth * sizeof(uint32_t));
                } else {
                    const guikit::Rect r = ui.GetPhysicalDirtyRect();
                    const SDL_Rect sdlR = {
                        static_cast<int>(r.x),
                        static_cast<int>(r.y),
                        static_cast<int>(r.w),
                        static_cast<int>(r.h)
                    };
                    const uint32_t* src = pixelBuffer.data() + sdlR.y * winWidth + sdlR.x;
                    SDL_UpdateTexture(screenTexture, &sdlR, src, winWidth * sizeof(uint32_t));
                }
            }
            SDL_RenderClear(renderer);
            SDL_RenderTexture(renderer, screenTexture, nullptr, nullptr);
            metrics.MarkGpuDone();

            SDL_RenderPresent(renderer);
            metrics.MarkFramePresented();
        }

        // Frame rate limiter and pacing:
        // Ensures smooth 60 FPS pacing and prevents unconstrained spinning when presenting is skipped or VSync is off
        const auto frameEnd = std::chrono::steady_clock::now();
        const double frameElapsedMs = std::chrono::duration<double, std::milli>(frameEnd - frameWorkStart).count();
        const double targetFrameMs = 1000.0 / 60.0; // 16.67 ms (60 FPS)
        if (frameElapsedMs < targetFrameMs) {
            const double remainingMs = targetFrameMs - frameElapsedMs;
            if (!reactiveMode && !vsyncEnabled) {
                SDL_Delay(static_cast<uint32_t>(remainingMs));
            } else if (!shouldPresent) {
                if (remainingMs >= 1.0) {
                    SDL_Delay(static_cast<uint32_t>(remainingMs));
                }
            }
        }

        metrics.EndFrame(60.0f);
    }

    if (screenTexture) {
        SDL_DestroyTexture(screenTexture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
