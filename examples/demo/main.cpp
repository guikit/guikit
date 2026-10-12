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
    bool lightMode = false;
    bool showTelemetry = false;
    bool formMode = true;
    float speedDrop = 50.0f;
    std::string sampleId = "Specimen-2026-X";
    size_t baudIndex = 1;
    std::string profileMode = "Standard Performance";
    float masterGain = 72.5f;
    int channelCount = 4;
    size_t filterPreset = 1;
    static constexpr std::array<std::string_view, 4> filterLabels = {"Bypass", "Butterworth", "Chebyshev", "Bessel"};
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
    guikit::UiSize controlsSize = guikit::UiSize::Md;
    float cardRadius = 8.0f;
    float cardPadding = 16.0f;
    float cardGap = 8.0f;
    float gridColGap = 12.0f;
    float gridRowGap = 10.0f;
    float colGap = 12.0f;
    float rowGap = 8.0f;
    float tabGap = 4.0f;
    float carouselGap = 8.0f;
    float scrollGap = 12.0f;
    guikit::UiSize headingSize = guikit::UiSize::Md;
    bool headingWordWrap = true;
    bool headingDivider = true;
    float cardBorderWidth = 1.0f;

    enum class NavCategory : uint8_t {
        // Elements (Atoms)
        ElemButton = 0,
        ElemSlider,
        ElemInputField,
        ElemSelect,
        ElemSwitch,
        ElemCheckbox,
        ElemTabBar,
        ElemContainers,
        ElemTypography,

        // Assembly (Compositions)
        AsmDashboard,
        AsmFormControls,
        AsmFlexRibbon,
        AsmDiagnostics,
        AsmThemeStudio
    };

    auto isElementCategory = [](NavCategory cat) {
        return cat <= NavCategory::ElemTypography;
    };

    NavCategory selectedCategory = NavCategory::ElemButton;

    guikit::UiVariant btnVariant = guikit::UiVariant::Primary;
    bool btnHasIcon = true;
    guikit::Placement btnIconPlacement = guikit::Placement::Leading;
    guikit::Align btnAlign = guikit::Align::Center;
    bool btnStretch = false;
    bool btnDisabled = false;
    int btnClicks = 0;

    float testSliderVal = 65.0f;
    size_t testStepIdx = 2;
    bool sliderContinuous = true;
    bool sliderShowTicks = true;
    bool sliderShowLabels = true;
    bool sliderShowValue = true;
    bool sliderDisabled = false;

    std::string testInputText = "guikit.dev";
    guikit::CursorBlinkMode inputCursorMode = guikit::CursorBlinkMode::Blinking;
    bool inputReadOnly = false;
    bool inputDisabled = false;
    bool inputHasPrefix = true;
    bool inputHasSuffix = false;

    size_t testSelectIdx = 0;
    static const std::vector<std::string_view> selectItems = {"Option Alpha", "Option Beta", "Option Gamma", "Option Delta", "Option Epsilon"};
    bool selectDisabled = false;
    bool selectStretch = true;
    uint32_t selectMaxItems = 4;

    bool testSwitchState = true;
    guikit::Placement switchLabelPlace = guikit::Placement::Trailing;
    bool switchSymmetric = false;
    bool switchShowDesc = true;
    bool switchDisabled = false;

    bool testCheckboxState = true;
    bool testCheckboxIndet = false;
    guikit::Placement checkboxLabelPlace = guikit::Placement::Trailing;
    bool checkboxShowDesc = true;
    bool checkboxDisabled = false;

    int testTabIdx = 0;
    static const std::array<guikit::TabItem, 3> inspectorDemoTabs = {{
        {.label = "First", .icon = guikit::Icon::Settings()},
        {.label = "Second", .icon = guikit::Icon::Eye()},
        {.label = "Third", .icon = guikit::Icon::Play()}
    }};
    bool testTabShowSep = true;
    bool testTabFullWidth = true;
    bool testTabScrollable = false;
    bool testTabHasBg = true;
    float testTabRadius = 8.0f;
    float testTabPadding = 3.0f;
    float testTabBorderWidth = 1.0f;

    guikit::FrameMetrics metrics;

    static const guikit::UiTheme shellTheme = guikit::UiTheme::Dark(guikit::Density::Standard);
    static const guikit::UiTheme& inspectorTheme = shellTheme;
    guikit::UiTheme appTheme = darkMode ? guikit::UiTheme::Dark(guikit::Density::Standard) : guikit::UiTheme::Light(guikit::Density::Standard);

    ui.SetTheme(shellTheme);
    ui.SetClearColor(shellTheme.palette.background);
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

                // Engine HUD Toolbar:
                // SIdebar | Scale Light | VSync React Dirty Heartbeat | Bounds Inspect
                if (auto tb = ui.Toolbar()) {
                    // 1. Sidebar Toggle
                    if (tb.ToolButton({
                        .label = "Sidebar",
                        .icon = guikit::Icon::Settings(),
                        .variant = showSidebar ? guikit::UiVariant::Primary : guikit::UiVariant::Ghost,
                        .active = showSidebar
                    })) {
                        showSidebar = !showSidebar;
                        ui.InvalidateAll();
                    }

                    tb.Separator();

                    // 2. Scale Preset Combo & Light Mode Switch
                    if (auto scaleBox = ui.Box({.fixedWidth = 92.0f})) {
                        if (ui.Select(scaleIndex, scaleLabels, {.size = guikit::UiSize::Sm}).changed) {
                            ui.InvalidateAll();
                        }
                    }

                    if (ui.Switch(lightMode, {
                        .label = "Dark",
                        .trailingLabel = "Light",
                        .labelPlacement = guikit::Placement::Both,
                        .symmetric = true,
                        .size = guikit::UiSize::Md
                    }).changed) {
                        darkMode = !lightMode;
                        const auto defTheme = darkMode ? guikit::UiTheme::Dark(appTheme.density) : guikit::UiTheme::Light(appTheme.density);
                        appTheme.palette = defTheme.palette;
                        appTheme.variants = defTheme.variants;
                        appTheme.mode = darkMode ? guikit::ThemeMode::Dark : guikit::ThemeMode::Light;
                        ui.InvalidateAll();
                    }

                    tb.Separator();

                    // 3. VSync, React, Dirty Switches & Heartbeat Cycle
                    if (ui.Switch(vsyncEnabled, {.label = "VSync", .size = guikit::UiSize::Md}).changed) {
                        SDL_SetRenderVSync(renderer, vsyncEnabled ? 1 : 0);
                    }

                    if (ui.Switch(reactiveMode, {.label = "React", .size = guikit::UiSize::Md}).changed) {
                        if (!reactiveMode) {
                            ui.InvalidateAll();
                        }
                    }

                    bool dirtyActive = ui.IsDirtyRectEnabled();
                    if (ui.Switch(dirtyActive, {.label = "Dirty", .size = guikit::UiSize::Md}).changed) {
                        ui.SetDirtyRectEnabled(dirtyActive);
                        if (!dirtyActive) {
                            ui.InvalidateAll();
                        }
                    }

                    if (tb.ToolButton({
                        .label = heartbeatLabels[heartbeatIndex],
                        .icon = guikit::Icon::Refresh(),
                        .labelVariants = heartbeatLabels,
                        .disabled = !reactiveMode
                    })) {
                        heartbeatIndex = (heartbeatIndex + 1) % heartbeatLabels.size();
                        ui.SetIdleHeartbeatMs(heartbeatMsValues[heartbeatIndex]);
                    }

                    tb.Separator();

                    // 4. Debug Bounds & Inspect Mode
                    const bool boundsActive = ui.IsDebugBounds();
                    if (tb.ToolButton({
                        .label = "Bounds",
                        .icon = boundsActive ? guikit::Icon::Eye() : guikit::Icon::EyeOff(),
                        .variant = boundsActive ? guikit::UiVariant::Info : guikit::UiVariant::Ghost,
                        .active = boundsActive
                    })) {
                        ui.SetDebugBounds(!boundsActive);
                    }

                    const bool inspectActive = ui.IsInspectMode();
                    if (tb.ToolButton({
                        .label = "Inspect",
                        .icon = guikit::Icon::Search(),
                        .variant = inspectActive ? guikit::UiVariant::Warning : guikit::UiVariant::Ghost,
                        .active = inspectActive
                    })) {
                        ui.SetInspectMode(!inspectActive);
                    }
                }

                // Workspace Row containing Sidebar (left) and Content Area (right)
                if (auto workspaceRow = ui.Row({.gap = 12.0f, .heightMode = guikit::SizeMode::Stretch})) {
                    // Collapsible Two-Level Inspector Sidebar
                    if (showSidebar) {
                        // 1. Level 1: Element Catalog & Assembly Rail (Left)
                        if (auto railBox = ui.Box({
                            .heightMode = guikit::SizeMode::Stretch,
                            .fixedWidth = 155.0f,
                            .theme = &inspectorTheme,
                            .scale = 1.0f
                        })) {
                            if (auto railCard = ui.Card({.gap = 4.0f, .padding = guikit::Padding::All(8.0f), .heightMode = guikit::SizeMode::Stretch})) {
                                if (auto railScroll = ui.ScrollArea({.gap = 4.0f})) {
                                    auto makeCatBtn = [&](NavCategory cat, std::string_view label, guikit::Icon icon) {
                                        const bool isSel = (selectedCategory == cat);
                                        if (ui.Button({
                                            .label = label,
                                            .icon = icon,
                                            .variant = isSel ? guikit::UiVariant::Primary : guikit::UiVariant::Ghost,
                                            .iconPlacement = guikit::Placement::Leading,
                                            .align = guikit::Align::Start,
                                            .stretch = true,
                                            .disabled = false,
                                            .active = isSel,
                                            .size = guikit::UiSize::Sm
                                        })) {
                                            selectedCategory = cat;
                                            ui.InvalidateAll();
                                        }
                                    };

                                    ui.Heading({
                                        .title = "ELEMENTS",
                                        .size = guikit::UiSize::Xs,
                                        .divider = true
                                    });
                                    makeCatBtn(NavCategory::ElemButton, "Button", guikit::Icon::Play());
                                    makeCatBtn(NavCategory::ElemSlider, "Slider", guikit::Icon::Settings());
                                    makeCatBtn(NavCategory::ElemInputField, "Input", guikit::Icon::Edit());
                                    makeCatBtn(NavCategory::ElemSelect, "Select", guikit::Icon::Plus());
                                    makeCatBtn(NavCategory::ElemSwitch, "Switch", guikit::Icon::Refresh());
                                    makeCatBtn(NavCategory::ElemCheckbox, "Checkbox", guikit::Icon::Check());
                                    makeCatBtn(NavCategory::ElemTabBar, "TabBar", guikit::Icon::Folder());
                                    makeCatBtn(NavCategory::ElemContainers, "Containers", guikit::Icon::Copy());
                                    makeCatBtn(NavCategory::ElemTypography, "Typography", guikit::Icon::Edit());

                                    if (auto spacerBox = ui.Box({.fixedHeight = 8.0f})) {}

                                    ui.Heading({
                                        .title = "ASSEMBLY",
                                        .size = guikit::UiSize::Xs,
                                        .divider = true
                                    });
                                    makeCatBtn(NavCategory::AsmDashboard, "Dashboard", guikit::Icon::Settings());
                                    makeCatBtn(NavCategory::AsmFormControls, "Form Demo", guikit::Icon::Edit());
                                    makeCatBtn(NavCategory::AsmFlexRibbon, "Flex Ribbon", guikit::Icon::Folder());
                                    makeCatBtn(NavCategory::AsmDiagnostics, "Diagnostics", guikit::Icon::Eye());
                                    makeCatBtn(NavCategory::AsmThemeStudio, "Theme Studio", guikit::Icon::Refresh());
                                }
                            }
                        }

                        // 2. Level 2: Inspector Panel (Right of Rail)
                        if (auto inspectorBox = ui.Box({
                            .heightMode = guikit::SizeMode::Stretch,
                            .fixedWidth = 300.0f,
                            .theme = &inspectorTheme,
                            .scale = 1.0f
                        })) {
                            if (auto inspectorCard = ui.Card({.gap = 6.0f, .padding = guikit::Padding::All(12.0f), .heightMode = guikit::SizeMode::Stretch})) {
                                if (auto inspectorScroll = ui.ScrollArea({.gap = 8.0f})) {
                                    auto& th = appTheme;
                                    if (isElementCategory(selectedCategory)) {
                                        switch (selectedCategory) {
                                            case NavCategory::ElemButton: {
                                                ui.Heading({.title = "Button Inspector", .subtitle = "Design System Component", .size = guikit::UiSize::Sm, .divider = true});

                                                ui.Heading({.title = "Geometry & Metrics", .size = guikit::UiSize::Sm});
                                                if (ui.StepSlider(controlsSize, {
                                                    .label = "Controls Size",
                                                    .showTicks = true,
                                                    .showLabels = true,
                                                    .showValue = true
                                                }).changed) {
                                                    ui.InvalidateAll();
                                                }
                                                auto& m = th.GetMetrics(controlsSize);
                                                if (ui.Slider(m.cornerRadius, 0.0f, 24.0f, {.label = "Corner Radius", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Slider(m.paddingX, 4.0f, 32.0f, {.label = "Horizontal Padding", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Slider(m.paddingY, 2.0f, 20.0f, {.label = "Vertical Padding", .format = "%.0f px"}).changed) ui.InvalidateAll();

                                                ui.Heading({.title = "Variant & Styling", .size = guikit::UiSize::Sm});
                                                static constexpr std::array<std::pair<guikit::UiVariant, std::string_view>, 8> variants = {{
                                                    {guikit::UiVariant::Default, "Default"},
                                                    {guikit::UiVariant::Primary, "Primary"},
                                                    {guikit::UiVariant::Secondary, "Secondary"},
                                                    {guikit::UiVariant::Success, "Success"},
                                                    {guikit::UiVariant::Warning, "Warning"},
                                                    {guikit::UiVariant::Danger, "Danger"},
                                                    {guikit::UiVariant::Info, "Info"},
                                                    {guikit::UiVariant::Ghost, "Ghost"}
                                                }};
                                                if (auto vGrid = ui.Grid()) {
                                                    for (const auto& [v, name] : variants) {
                                                        if (auto c = vGrid.Cell6()) {
                                                            const bool activeV = (btnVariant == v);
                                                            if (ui.Button({
                                                                .label = name,
                                                                .variant = v,
                                                                .stretch = true,
                                                                .active = activeV
                                                            })) {
                                                                btnVariant = v;
                                                                ui.InvalidateAll();
                                                            }
                                                        }
                                                    }
                                                }

                                                ui.Heading({.title = "Options & States", .size = guikit::UiSize::Sm});
                                                if (ui.Switch(btnStretch, {.label = "Stretch (Full Width)"}).changed) ui.InvalidateAll();
                                                static constexpr std::array<std::string_view, 3> alignNames = {"Start (Left)", "Center", "End (Right)"};
                                                size_t alignIdx = (btnAlign == guikit::Align::Start) ? 0 : ((btnAlign == guikit::Align::End) ? 2 : 1);
                                                if (ui.StepSlider(alignIdx, alignNames, {.label = "Content Alignment", .showTicks = true, .showLabels = true}).changed) {
                                                    btnAlign = (alignIdx == 0) ? guikit::Align::Start : ((alignIdx == 2) ? guikit::Align::End : guikit::Align::Center);
                                                    ui.InvalidateAll();
                                                }
                                                if (ui.Switch(btnHasIcon, {.label = "Show Icon"}).changed) ui.InvalidateAll();
                                                if (btnHasIcon) {
                                                    static constexpr std::array<std::string_view, 4> placeNames = {"Leading", "Trailing", "Top", "Bottom"};
                                                    size_t placeIdx = (btnIconPlacement == guikit::Placement::Trailing) ? 1 : ((btnIconPlacement == guikit::Placement::Top) ? 2 : ((btnIconPlacement == guikit::Placement::Bottom) ? 3 : 0));
                                                    if (ui.StepSlider(placeIdx, placeNames, {.label = "Icon Placement", .showTicks = true, .showLabels = true}).changed) {
                                                        btnIconPlacement = (placeIdx == 1) ? guikit::Placement::Trailing : ((placeIdx == 2) ? guikit::Placement::Top : ((placeIdx == 3) ? guikit::Placement::Bottom : guikit::Placement::Leading));
                                                        ui.InvalidateAll();
                                                    }
                                                }
                                                if (ui.Switch(btnDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemSlider: {
                                                ui.Heading({.title = "Slider Inspector", .subtitle = "Range & Quantized Controls", .size = guikit::UiSize::Sm, .divider = true});

                                                ui.Heading({.title = "Mode & Geometry", .size = guikit::UiSize::Sm});
                                                if (ui.Switch(sliderContinuous, {.label = "Continuous Mode", .description = "Continuous vs Quantized StepSlider"}).changed) ui.InvalidateAll();
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();

                                                ui.Heading({.title = "Display Options", .size = guikit::UiSize::Sm});
                                                if (ui.Switch(sliderShowValue, {.label = "Show Value Tag"}).changed) ui.InvalidateAll();
                                                if (!sliderContinuous) {
                                                    if (ui.Switch(sliderShowTicks, {.label = "Show Step Ticks"}).changed) ui.InvalidateAll();
                                                    if (ui.Switch(sliderShowLabels, {.label = "Show Step Labels"}).changed) ui.InvalidateAll();
                                                }
                                                if (ui.Switch(sliderDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemInputField: {
                                                ui.Heading({.title = "InputField Inspector", .subtitle = "Text & Value Editing", .size = guikit::UiSize::Sm, .divider = true});

                                                ui.Heading({.title = "Geometry & Styling", .size = guikit::UiSize::Sm});
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
                                                auto& m = th.GetMetrics(controlsSize);
                                                if (ui.Slider(m.cornerRadius, 0.0f, 24.0f, {.label = "Corner Radius", .format = "%.0f px"}).changed) ui.InvalidateAll();

                                                ui.Heading({.title = "Cursor & Flags", .size = guikit::UiSize::Sm});
                                                static constexpr std::array<std::string_view, 3> cursorModes = {"Blinking", "Solid", "Hidden"};
                                                size_t curIdx = (inputCursorMode == guikit::CursorBlinkMode::Solid) ? 1 : ((inputCursorMode == guikit::CursorBlinkMode::Hidden) ? 2 : 0);
                                                if (ui.StepSlider(curIdx, cursorModes, {.label = "Cursor Blink Mode", .showTicks = true, .showLabels = true}).changed) {
                                                    inputCursorMode = (curIdx == 1) ? guikit::CursorBlinkMode::Solid : ((curIdx == 2) ? guikit::CursorBlinkMode::Hidden : guikit::CursorBlinkMode::Blinking);
                                                    ui.InvalidateAll();
                                                }
                                                if (ui.Switch(inputHasPrefix, {.label = "Prefix (https://)"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(inputHasSuffix, {.label = "Suffix (:443)"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(inputReadOnly, {.label = "Read Only"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(inputDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemSelect: {
                                                ui.Heading({.title = "Select Inspector", .subtitle = "Dropdown Options Menu", .size = guikit::UiSize::Sm, .divider = true});

                                                ui.Heading({.title = "Geometry & Behavior", .size = guikit::UiSize::Sm});
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
                                                float maxItemsFloat = static_cast<float>(selectMaxItems);
                                                if (ui.Slider(maxItemsFloat, 2.0f, 8.0f, {.label = "Max Visible Menu Items", .format = "%.0f items", .step = 1.0}).changed) {
                                                    selectMaxItems = static_cast<uint32_t>(maxItemsFloat);
                                                    ui.InvalidateAll();
                                                }
                                                if (ui.Switch(selectDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemSwitch: {
                                                ui.Heading({.title = "Switch Inspector", .subtitle = "Two-State & Dual Toggles", .size = guikit::UiSize::Sm, .divider = true});

                                                ui.Heading({.title = "Geometry & Options", .size = guikit::UiSize::Sm});
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
                                                static constexpr std::array<std::string_view, 6> switchPlacements = {
                                                    "Trail", "Lead", "Top", "Btm", "Stack", "Both"
                                                };
                                                size_t spIdx = 0;
                                                switch (switchLabelPlace) {
                                                    case guikit::Placement::Trailing: spIdx = 0; break;
                                                    case guikit::Placement::Leading:  spIdx = 1; break;
                                                    case guikit::Placement::Top:      spIdx = 2; break;
                                                    case guikit::Placement::Bottom:   spIdx = 3; break;
                                                    case guikit::Placement::Stacked:  spIdx = 4; break;
                                                    case guikit::Placement::Both:     spIdx = 5; break;
                                                }
                                                if (ui.StepSlider(spIdx, switchPlacements, {.label = "Label Placement", .showTicks = true, .showLabels = true}).changed) {
                                                    switch (spIdx) {
                                                        case 0: switchLabelPlace = guikit::Placement::Trailing; break;
                                                        case 1: switchLabelPlace = guikit::Placement::Leading;  break;
                                                        case 2: switchLabelPlace = guikit::Placement::Top;      break;
                                                        case 3: switchLabelPlace = guikit::Placement::Bottom;   break;
                                                        case 4: switchLabelPlace = guikit::Placement::Stacked;  break;
                                                        case 5: switchLabelPlace = guikit::Placement::Both;     break;
                                                    }
                                                    ui.InvalidateAll();
                                                }
                                                if (ui.Switch(switchSymmetric, {.label = "Symmetric (Neutral Track)"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(switchShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(switchDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemCheckbox: {
                                                ui.Heading({.title = "Checkbox Inspector", .subtitle = "Multi-State Selection & Tri-State", .size = guikit::UiSize::Sm, .divider = true});

                                                ui.Heading({.title = "Geometry & Options", .size = guikit::UiSize::Sm});
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
                                                static constexpr std::array<std::string_view, 5> checkPlacements = {
                                                    "Trail", "Lead", "Top", "Btm", "Stack"
                                                };
                                                size_t cpIdx = 0;
                                                switch (checkboxLabelPlace) {
                                                    case guikit::Placement::Trailing: cpIdx = 0; break;
                                                    case guikit::Placement::Leading:  cpIdx = 1; break;
                                                    case guikit::Placement::Top:      cpIdx = 2; break;
                                                    case guikit::Placement::Bottom:   cpIdx = 3; break;
                                                    case guikit::Placement::Stacked:  cpIdx = 4; break;
                                                    default:                          cpIdx = 0; break;
                                                }
                                                if (ui.StepSlider(cpIdx, checkPlacements, {.label = "Label Placement", .showTicks = true, .showLabels = true}).changed) {
                                                    switch (cpIdx) {
                                                        case 0: checkboxLabelPlace = guikit::Placement::Trailing; break;
                                                        case 1: checkboxLabelPlace = guikit::Placement::Leading;  break;
                                                        case 2: checkboxLabelPlace = guikit::Placement::Top;      break;
                                                        case 3: checkboxLabelPlace = guikit::Placement::Bottom;   break;
                                                        case 4: checkboxLabelPlace = guikit::Placement::Stacked;  break;
                                                    }
                                                    ui.InvalidateAll();
                                                }
                                                if (ui.Switch(testCheckboxIndet, {.label = "Indeterminate Tri-State"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(checkboxShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(checkboxDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemTabBar: {
                                                ui.Heading({.title = "TabBar Inspector", .subtitle = "Tabs & Segmented Control", .size = guikit::UiSize::Sm, .divider = true});

                                                ui.Heading({.title = "Style & Container", .size = guikit::UiSize::Sm});
                                                static constexpr std::array<std::string_view, 2> styleNames = {"Underline", "Pill"};
                                                size_t styleIdx = (tabStyle == guikit::TabBarStyle::Pill) ? 1 : 0;
                                                if (ui.StepSlider(styleIdx, styleNames, {.label = "TabBar Style", .showTicks = true, .showLabels = true}).changed) {
                                                    tabStyle = (styleIdx == 1) ? guikit::TabBarStyle::Pill : guikit::TabBarStyle::Underline;
                                                    ui.InvalidateAll();
                                                }
                                                if (ui.Switch(testTabHasBg, {.label = "Container Background Fill"}).changed) ui.InvalidateAll();
                                                if (testTabHasBg) {
                                                    if (ui.Slider(testTabRadius, 0.0f, 24.0f, {.label = "Container Corner Radius", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                                    if (ui.Slider(testTabPadding, 0.0f, 12.0f, {.label = "Container Internal Padding", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                                    if (ui.Slider(testTabBorderWidth, 0.0f, 4.0f, {.label = "Container Border Width", .format = "%.1f px"}).changed) ui.InvalidateAll();
                                                }
                                                if (ui.Slider(tabGap, 0.0f, 24.0f, {.label = "Tab Spacing (Gap)", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(testTabShowSep, {.label = "Baseline Separator"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(testTabFullWidth, {.label = "Full Width Stretch"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(testTabScrollable, {.label = "Scrollable Ribbon"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemContainers: {
                                                ui.Heading({.title = "Containers Inspector", .subtitle = "Card, Box & Surfaces", .size = guikit::UiSize::Sm, .divider = true});

                                                ui.Heading({.title = "Card Geometry", .size = guikit::UiSize::Sm});
                                                if (ui.Slider(cardRadius, 0.0f, 24.0f, {.label = "Corner Radius", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Slider(cardPadding, 0.0f, 32.0f, {.label = "Internal Padding", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Slider(cardGap, 0.0f, 24.0f, {.label = "Content Spacing (Gap)", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Slider(cardBorderWidth, 0.0f, 4.0f, {.label = "Border Width", .format = "%.1f px"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemTypography: {
                                                ui.Heading({.title = "Typography Inspector", .subtitle = "Headings & Text Scales", .size = guikit::UiSize::Sm, .divider = true});

                                                ui.Heading({.title = "Typography Scale", .size = guikit::UiSize::Sm});
                                                if (ui.StepSlider(headingSize, {.label = "Heading Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
                                                if (ui.Switch(headingDivider, {.label = "Show Divider Line"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(headingWordWrap, {.label = "Subtitle Word Wrap"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            default: break;
                                        }

                                        // Common Reset Action for Elements
                                        if (ui.Button({
                                            .label = "Reset Element Parameters",
                                            .icon = guikit::Icon::Refresh(),
                                            .variant = guikit::UiVariant::Secondary,
                                            .stretch = true
                                        })) {
                                            controlsSize = guikit::UiSize::Md;
                                            btnVariant = guikit::UiVariant::Primary;
                                            btnHasIcon = true;
                                            btnIconPlacement = guikit::Placement::Leading;
                                            btnAlign = guikit::Align::Center;
                                            btnStretch = false;
                                            btnDisabled = false;
                                            cardRadius = 8.0f;
                                            cardGap = 8.0f;
                                            cardPadding = 16.0f;
                                            cardBorderWidth = 1.0f;
                                            tabGap = 4.0f;
                                            testTabRadius = 8.0f;
                                            testTabPadding = 3.0f;
                                            testTabBorderWidth = 1.0f;
                                            testTabHasBg = true;
                                            headingSize = guikit::UiSize::Md;
                                            headingWordWrap = true;
                                            headingDivider = true;
                                            switchLabelPlace = guikit::Placement::Trailing;
                                            switchSymmetric = false;
                                            switchShowDesc = true;
                                            switchDisabled = false;
                                            checkboxLabelPlace = guikit::Placement::Trailing;
                                            checkboxShowDesc = true;
                                            checkboxDisabled = false;
                                            testCheckboxIndet = false;
                                            ui.InvalidateAll();
                                        }
                                    } else {
                                        // Assembly Inspector (Scene & Environment)
                                        ui.Heading({
                                            .title = "Assembly Scene",
                                            .subtitle = "Global Layout & Environment",
                                            .size = guikit::UiSize::Sm,
                                            .divider = true
                                        });

                                        ui.Heading({.title = "Theme & Density", .size = guikit::UiSize::Sm});
                                        if (ui.Switch(darkMode, {
                                            .label = "Content Dark Mode",
                                            .description = "Switch Content preview between Dark and Light palette"
                                        }).changed) {
                                            lightMode = !darkMode;
                                            const auto defTheme = darkMode ? guikit::UiTheme::Dark(appTheme.density) : guikit::UiTheme::Light(appTheme.density);
                                            appTheme.palette = defTheme.palette;
                                            appTheme.variants = defTheme.variants;
                                            appTheme.mode = darkMode ? guikit::ThemeMode::Dark : guikit::ThemeMode::Light;
                                            ui.InvalidateAll();
                                        }

                                        static constexpr std::array<std::string_view, 3> asmDensityLabels = {"Compact", "Standard", "Touch"};
                                        size_t dIdx = (appTheme.density == guikit::Density::Compact) ? 0 : ((appTheme.density == guikit::Density::Touch) ? 2 : 1);
                                        if (ui.StepSlider(dIdx, asmDensityLabels, {.label = "Layout Density", .showTicks = true, .showLabels = true}).changed) {
                                            appTheme.density = (dIdx == 0) ? guikit::Density::Compact : ((dIdx == 2) ? guikit::Density::Touch : guikit::Density::Standard);
                                            appTheme.SetDensity(appTheme.density);
                                            ui.InvalidateAll();
                                        }

                                        ui.Heading({.title = "Grid Layout Spacing", .size = guikit::UiSize::Sm});
                                        if (ui.Slider(gridColGap, 0.0f, 32.0f, {.label = "Horizontal Gap (Columns)", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                        if (ui.Slider(gridRowGap, 0.0f, 32.0f, {.label = "Vertical Gap (Rows)", .format = "%.0f px"}).changed) ui.InvalidateAll();

                                        ui.Heading({.title = "Surfaces & Containers", .size = guikit::UiSize::Sm});
                                        if (ui.Slider(cardRadius, 0.0f, 24.0f, {.label = "Card Corner Radius", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                        if (ui.Slider(cardPadding, 4.0f, 32.0f, {.label = "Card Padding", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                        if (ui.Slider(cardBorderWidth, 0.0f, 4.0f, {.label = "Card Border Width", .format = "%.1f px"}).changed) ui.InvalidateAll();

                                        if (selectedCategory == NavCategory::AsmDashboard) {
                                            ui.Heading({.title = "Dashboard Overrides", .size = guikit::UiSize::Sm});
                                            if (ui.Switch(hwEstop, {.label = "Hardware E-Stop Line", .variant = guikit::UiVariant::Danger}).changed) ui.InvalidateAll();
                                            if (ui.Switch(enableSensors, {.label = "Sensor Drop Limits"}).changed) ui.InvalidateAll();
                                            if (ui.Switch(diagLogging, {.label = "High-Speed Flash Logging"}).changed) ui.InvalidateAll();
                                        } else if (selectedCategory == NavCategory::AsmFormControls) {
                                            ui.Heading({.title = "Form Overrides", .size = guikit::UiSize::Sm});
                                            if (ui.Switch(formMode, {.label = "Stacked Labels Mode"}).changed) ui.InvalidateAll();
                                            if (ui.Switch(showTelemetry, {.label = "Show Telemetry Banner"}).changed) ui.InvalidateAll();
                                        } else if (selectedCategory == NavCategory::AsmFlexRibbon) {
                                            ui.Heading({.title = "Ribbon Spacing", .size = guikit::UiSize::Sm});
                                            if (ui.Slider(carouselGap, 0.0f, 24.0f, {.label = "Ribbon Items Gap", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                        }

                                        // Common Reset Action for Assembly
                                        if (ui.Button({
                                            .label = "Reset Scene Parameters",
                                            .icon = guikit::Icon::Refresh(),
                                            .variant = guikit::UiVariant::Secondary,
                                            .stretch = true
                                        })) {
                                            gridColGap = 12.0f;
                                            gridRowGap = 10.0f;
                                            cardRadius = 8.0f;
                                            cardPadding = 16.0f;
                                            cardBorderWidth = 1.0f;
                                            carouselGap = 8.0f;
                                            appTheme.density = guikit::Density::Standard;
                                            appTheme.SetDensity(appTheme.density);
                                            ui.InvalidateAll();
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Content Canvas (Center / Right Workspace) with Scoped Theme & Canvas Frame
                    if (auto contentBox = ui.Box({
                        .widthMode = guikit::SizeMode::Stretch,
                        .heightMode = guikit::SizeMode::Stretch,
                        .padding = guikit::Padding::All(8.0f),
                        .backgroundColor = appTheme.palette.background,
                        .borderColor = shellTheme.palette.borderSubtle,
                        .borderWidth = 1.0f,
                        .radius = 8.0f,
                        .clipChildren = true,
                        .theme = &appTheme,
                        .scale = scaleValues[scaleIndex]
                    })) {
                        if (auto contentCol = ui.Column({
                            .gap = colGap,
                            .widthMode = guikit::SizeMode::Stretch,
                            .heightMode = guikit::SizeMode::Stretch,
                            .size = controlsSize
                        })) {
                            if (auto contentScroll = ui.ScrollArea({.gap = 12.0f})) {
                            auto& th = appTheme;
                            const auto makeCard = [&]() {
                                return guikit::CardOptions{
                                    .gap = cardGap,
                                    .padding = guikit::Padding(cardPadding),
                                    .radius = cardRadius
                                };
                            };
                            const auto makeGrid = [&]() {
                                return guikit::GridOptions{
                                    .gap = gridColGap,
                                    .runGap = gridRowGap
                                };
                            };

                            if (isElementCategory(selectedCategory)) {
                                // -------------------------------------------------------------
                                // ELEMENTS MODE: Component Sandbox (Hero + Matrices + C++ Code)
                                // -------------------------------------------------------------
                                if (auto heroCard = ui.Card({.gap = 12.0f, .padding = guikit::Padding::All(14.0f)})) {
                                    // Hero Showcase Stage
                                    if (auto stageBox = ui.Box({
                                        .padding = guikit::Padding::All(24.0f),
                                        .backgroundColor = th.palette.surfaceElevated,
                                        .borderColor = th.palette.borderSubtle,
                                        .borderWidth = 1.0f,
                                        .radius = 8.0f
                                    })) {
                                        if (auto centerRow = ui.Row({.align = guikit::Alignment::Center, .justify = guikit::Justify::Center})) {
                                            switch (selectedCategory) {
                                                case NavCategory::ElemButton: {
                                                    std::string btnText = btnClicks > 0 ? std::format("Clicked {}x", btnClicks) : "Interactive Button";
                                                    if (ui.Button({
                                                        .label = btnText,
                                                        .icon = btnHasIcon ? guikit::Icon::Play() : guikit::Icon{},
                                                        .variant = btnVariant,
                                                        .iconPlacement = btnIconPlacement,
                                                        .align = btnAlign,
                                                        .stretch = btnStretch,
                                                        .disabled = btnDisabled,
                                                        .size = controlsSize
                                                    })) {
                                                        ++btnClicks;
                                                        ui.InvalidateAll();
                                                    }
                                                    break;
                                                }

                                                case NavCategory::ElemSlider: {
                                                    if (sliderContinuous) {
                                                        ui.Slider(testSliderVal, 0.0f, 100.0f, {
                                                            .label = "Analog Master Gain",
                                                            .format = "%.1f %%",
                                                            .showValue = sliderShowValue,
                                                            .variant = btnVariant,
                                                            .size = controlsSize,
                                                            .disabled = sliderDisabled
                                                        });
                                                    } else {
                                                        static constexpr std::array<std::string_view, 5> stepItems = {"Off", "Low", "Mid", "High", "Max"};
                                                        ui.StepSlider(testStepIdx, stepItems, {
                                                            .label = "DSP Filter Mode",
                                                            .showTicks = sliderShowTicks,
                                                            .showLabels = sliderShowLabels,
                                                            .showValue = sliderShowValue,
                                                            .variant = btnVariant,
                                                            .size = controlsSize,
                                                            .disabled = sliderDisabled
                                                        });
                                                    }
                                                    break;
                                                }

                                                case NavCategory::ElemInputField: {
                                                    ui.InputField(testInputText, {
                                                        .label = "Server Endpoint",
                                                        .description = "Configured host address",
                                                        .placeholder = "Enter domain...",
                                                        .prefix = inputHasPrefix ? "https://" : "",
                                                        .suffix = inputHasSuffix ? ":443" : "",
                                                        .variant = btnVariant,
                                                        .cursorMode = inputCursorMode,
                                                        .size = controlsSize,
                                                        .disabled = inputDisabled,
                                                        .readOnly = inputReadOnly
                                                    });
                                                    break;
                                                }

                                                case NavCategory::ElemSelect: {
                                                    ui.Select(testSelectIdx, selectItems, {
                                                        .label = "Active Preset",
                                                        .description = "Dropdown selection",
                                                        .variant = btnVariant,
                                                        .maxVisibleItems = selectMaxItems,
                                                        .size = controlsSize,
                                                        .disabled = selectDisabled
                                                    });
                                                    break;
                                                }

                                                case NavCategory::ElemSwitch: {
                                                    if (auto swRow = ui.Row({.gap = 20.0f, .align = guikit::Alignment::Center})) {
                                                        ui.Switch(testSwitchState, {
                                                            .label = (switchLabelPlace == guikit::Placement::Both) ? "Dark" : "Hardware E-Stop",
                                                            .trailingLabel = (switchLabelPlace == guikit::Placement::Both) ? "Light" : "",
                                                            .description = switchShowDesc ? "Safety interlocking system" : "",
                                                            .variant = btnVariant,
                                                            .labelPlacement = switchLabelPlace,
                                                            .symmetric = switchSymmetric,
                                                            .disabled = switchDisabled,
                                                            .size = controlsSize
                                                        });
                                                    }
                                                    break;
                                                }

                                                case NavCategory::ElemCheckbox: {
                                                    if (auto chRow = ui.Row({.gap = 20.0f, .align = guikit::Alignment::Center})) {
                                                        ui.Checkbox(testCheckboxState, {
                                                            .label = "Telemetry Logging",
                                                            .description = checkboxShowDesc ? "Save trace to buffer" : "",
                                                            .variant = btnVariant,
                                                            .labelPlacement = checkboxLabelPlace,
                                                            .disabled = checkboxDisabled,
                                                            .indeterminate = testCheckboxIndet,
                                                            .size = controlsSize
                                                        });
                                                    }
                                                    break;
                                                }

                                                case NavCategory::ElemTabBar: {
                                                    ui.TabBar(testTabIdx, inspectorDemoTabs, {
                                                        .style = tabStyle,
                                                        .showSeparator = testTabShowSep,
                                                        .fullWidth = testTabFullWidth,
                                                        .scrollable = testTabScrollable,
                                                        .gap = tabGap,
                                                        .padding = testTabHasBg ? guikit::Padding::All(testTabPadding) : guikit::Padding{},
                                                        .radius = testTabHasBg ? testTabRadius : 0.0f,
                                                        .backgroundColor = testTabHasBg ? th.palette.surfaceElevated : guikit::Color::Transparent(),
                                                        .borderColor = testTabHasBg ? th.palette.borderSubtle : guikit::Color::Transparent(),
                                                        .borderWidth = testTabHasBg ? testTabBorderWidth : 0.0f
                                                    });
                                                    break;
                                                }

                                                case NavCategory::ElemContainers: {
                                                    if (auto c = ui.Card({.gap = cardGap, .padding = guikit::Padding(cardPadding), .borderWidth = cardBorderWidth, .radius = cardRadius})) {
                                                        ui.Text("Card Surface Component", {.role = guikit::TextRole::HeadlineSmall});
                                                        ui.Text("Configurable padding, radius and spacing tokens", {.variant = guikit::UiVariant::Secondary});
                                                        if (ui.Button({.label = "Inner Action", .variant = guikit::UiVariant::Primary, .size = controlsSize})) {}
                                                    }
                                                    break;
                                                }

                                                case NavCategory::ElemTypography: {
                                                    ui.Heading({
                                                        .title = "Typographic Heading Sample",
                                                        .subtitle = "Dynamic typography scale test for multi-line layout hierarchy",
                                                        .icon = guikit::Icon::Edit(),
                                                        .size = headingSize,
                                                        .divider = headingDivider,
                                                        .wordWrap = headingWordWrap
                                                    });
                                                    break;
                                                }

                                                default: break;
                                            }
                                        }
                                    }
                                }

                                // Design Token Scale Matrix (UiSize: Xs -> Xl)
                                if (auto scaleMatrixCard = ui.Card({.gap = 8.0f, .padding = guikit::Padding::All(12.0f)})) {
                                    switch (selectedCategory) {
                                        case NavCategory::ElemButton: {
                                            if (auto row = ui.Row({.gap = 8.0f, .align = guikit::Alignment::Center})) {
                                                ui.Button({.label = "Button Xs", .variant = btnVariant, .size = guikit::UiSize::Xs});
                                                ui.Button({.label = "Button Sm", .variant = btnVariant, .size = guikit::UiSize::Sm});
                                                ui.Button({.label = "Button Md", .variant = btnVariant, .size = guikit::UiSize::Md});
                                                ui.Button({.label = "Button Lg", .variant = btnVariant, .size = guikit::UiSize::Lg});
                                                ui.Button({.label = "Button Xl", .variant = btnVariant, .size = guikit::UiSize::Xl});
                                            }
                                            break;
                                        }
                                        case NavCategory::ElemSlider: {
                                            if (auto grid = ui.Grid({.gap = 12.0f, .runGap = 8.0f})) {
                                                if (auto c = grid.Cell4()) { ui.Slider(testSliderVal, 0.0f, 100.0f, {.label = "Size Xs", .size = guikit::UiSize::Xs}); }
                                                if (auto c = grid.Cell4()) { ui.Slider(testSliderVal, 0.0f, 100.0f, {.label = "Size Sm", .size = guikit::UiSize::Sm}); }
                                                if (auto c = grid.Cell4()) { ui.Slider(testSliderVal, 0.0f, 100.0f, {.label = "Size Md", .size = guikit::UiSize::Md}); }
                                                if (auto c = grid.Cell6()) { ui.Slider(testSliderVal, 0.0f, 100.0f, {.label = "Size Lg", .size = guikit::UiSize::Lg}); }
                                                if (auto c = grid.Cell6()) { ui.Slider(testSliderVal, 0.0f, 100.0f, {.label = "Size Xl", .size = guikit::UiSize::Xl}); }
                                            }
                                            break;
                                        }
                                        case NavCategory::ElemInputField: {
                                            if (auto grid = ui.Grid({.gap = 12.0f, .runGap = 8.0f})) {
                                                if (auto c = grid.Cell4()) { ui.InputField(testInputText, {.label = "Size Xs", .size = guikit::UiSize::Xs}); }
                                                if (auto c = grid.Cell4()) { ui.InputField(testInputText, {.label = "Size Sm", .size = guikit::UiSize::Sm}); }
                                                if (auto c = grid.Cell4()) { ui.InputField(testInputText, {.label = "Size Md", .size = guikit::UiSize::Md}); }
                                                if (auto c = grid.Cell6()) { ui.InputField(testInputText, {.label = "Size Lg", .size = guikit::UiSize::Lg}); }
                                                if (auto c = grid.Cell6()) { ui.InputField(testInputText, {.label = "Size Xl", .size = guikit::UiSize::Xl}); }
                                            }
                                            break;
                                        }
                                        case NavCategory::ElemSelect: {
                                            if (auto grid = ui.Grid({.gap = 12.0f, .runGap = 8.0f})) {
                                                if (auto c = grid.Cell4()) { ui.Select(testSelectIdx, selectItems, {.label = "Size Xs", .size = guikit::UiSize::Xs}); }
                                                if (auto c = grid.Cell4()) { ui.Select(testSelectIdx, selectItems, {.label = "Size Sm", .size = guikit::UiSize::Sm}); }
                                                if (auto c = grid.Cell4()) { ui.Select(testSelectIdx, selectItems, {.label = "Size Md", .size = guikit::UiSize::Md}); }
                                                if (auto c = grid.Cell6()) { ui.Select(testSelectIdx, selectItems, {.label = "Size Lg", .size = guikit::UiSize::Lg}); }
                                                if (auto c = grid.Cell6()) { ui.Select(testSelectIdx, selectItems, {.label = "Size Xl", .size = guikit::UiSize::Xl}); }
                                            }
                                            break;
                                        }
                                        case NavCategory::ElemSwitch: {
                                            if (auto row = ui.Row({.gap = 16.0f, .align = guikit::Alignment::Center})) {
                                                ui.Switch(testSwitchState, {.label = "Xs", .size = guikit::UiSize::Xs});
                                                ui.Switch(testSwitchState, {.label = "Sm", .size = guikit::UiSize::Sm});
                                                ui.Switch(testSwitchState, {.label = "Md", .size = guikit::UiSize::Md});
                                                ui.Switch(testSwitchState, {.label = "Lg", .size = guikit::UiSize::Lg});
                                                ui.Switch(testSwitchState, {.label = "Xl", .size = guikit::UiSize::Xl});
                                            }
                                            if (auto row = ui.Row({.gap = 20.0f, .align = guikit::Alignment::Center})) {
                                                ui.Switch(lightMode, {
                                                    .label = "Dark",
                                                    .trailingLabel = "Light",
                                                    .labelPlacement = guikit::Placement::Both,
                                                    .symmetric = true,
                                                    .size = guikit::UiSize::Md
                                                });
                                                ui.Switch(testSwitchState, {
                                                    .label = "Leading Label",
                                                    .labelPlacement = guikit::Placement::Leading,
                                                    .size = guikit::UiSize::Md
                                                });
                                                ui.Switch(testSwitchState, {
                                                    .label = "Stacked Style",
                                                    .description = "Placement::Stacked",
                                                    .labelPlacement = guikit::Placement::Stacked,
                                                    .size = guikit::UiSize::Md
                                                });
                                            }
                                            break;
                                        }
                                        case NavCategory::ElemCheckbox: {
                                            if (auto row = ui.Row({.gap = 16.0f, .align = guikit::Alignment::Center})) {
                                                ui.Checkbox(testCheckboxState, {.label = "Xs", .size = guikit::UiSize::Xs});
                                                ui.Checkbox(testCheckboxState, {.label = "Sm", .size = guikit::UiSize::Sm});
                                                ui.Checkbox(testCheckboxState, {.label = "Md", .size = guikit::UiSize::Md});
                                                ui.Checkbox(testCheckboxState, {.label = "Lg", .size = guikit::UiSize::Lg});
                                                ui.Checkbox(testCheckboxState, {.label = "Xl", .size = guikit::UiSize::Xl});
                                            }
                                            if (auto row = ui.Row({.gap = 20.0f, .align = guikit::Alignment::Center})) {
                                                ui.Checkbox(testCheckboxState, {
                                                    .label = "Tri-State Indeterminate",
                                                    .indeterminate = true,
                                                    .size = guikit::UiSize::Md
                                                });
                                                ui.Checkbox(testCheckboxState, {
                                                    .label = "Leading Label",
                                                    .labelPlacement = guikit::Placement::Leading,
                                                    .size = guikit::UiSize::Md
                                                });
                                            }
                                            break;
                                        }
                                        case NavCategory::ElemTabBar: {
                                            ui.TabBar(testTabIdx, inspectorDemoTabs, {.style = guikit::TabBarStyle::Underline, .fullWidth = true});
                                            ui.TabBar(testTabIdx, inspectorDemoTabs, {
                                                .style = guikit::TabBarStyle::Pill,
                                                .fullWidth = true,
                                                .padding = guikit::Padding::All(3.0f),
                                                .radius = 8.0f,
                                                .backgroundColor = th.palette.surfaceElevated
                                            });
                                            break;
                                        }
                                        case NavCategory::ElemContainers: {
                                            if (auto grid = ui.Grid({.gap = 12.0f, .runGap = 8.0f})) {
                                                if (auto c1 = grid.Cell4()) {
                                                    if (auto cd = ui.Card({.gap = 6.0f, .padding = guikit::Padding::All(10.0f)})) {
                                                        ui.Text("Surface Base", {.role = guikit::TextRole::TitleMedium});
                                                        ui.Text("Card container standard", {.variant = guikit::UiVariant::Secondary});
                                                    }
                                                }
                                                if (auto c2 = grid.Cell4()) {
                                                    if (auto cd = ui.Card({.gap = 6.0f, .padding = guikit::Padding::All(10.0f), .backgroundColor = th.palette.surfaceElevated, .borderColor = th.palette.accent, .borderWidth = 1.5f})) {
                                                        ui.Text("Surface Elevated", {.role = guikit::TextRole::TitleMedium, .variant = guikit::UiVariant::Primary});
                                                        ui.Text("Card with primary border", {.variant = guikit::UiVariant::Secondary});
                                                    }
                                                }
                                                if (auto c3 = grid.Cell4()) {
                                                    if (auto cd = ui.Card({.gap = 6.0f, .padding = guikit::Padding::All(10.0f), .backgroundColor = th.palette.surfaceActive})) {
                                                        ui.Text("Surface Subtle", {.role = guikit::TextRole::TitleMedium});
                                                        ui.Text("Subdued background card", {.variant = guikit::UiVariant::Secondary});
                                                    }
                                                }
                                            }
                                            break;
                                        }
                                        case NavCategory::ElemTypography: {
                                            ui.Text("Display Large Typography (40px)", {.role = guikit::TextRole::DisplayLarge});
                                            ui.Text("Headline Medium Typography (22px)", {.role = guikit::TextRole::HeadlineMedium});
                                            ui.Text("Title Large Component Header (18px)", {.role = guikit::TextRole::TitleLarge});
                                            ui.Text("Body Medium Standard Paragraph (14px) - Clean legible rendering with subpixel positioning.", {.role = guikit::TextRole::BodyMedium});
                                            ui.Text("Code Medium Monospace - void Run(Context& ctx, const Options& opts = {});", {.role = guikit::TextRole::CodeMedium});
                                            break;
                                        }
                                        default: break;
                                    }
                                }

                                // Semantic Variants Matrix (UiVariant)
                                if (auto varMatrixCard = ui.Card({.gap = 8.0f, .padding = guikit::Padding::All(12.0f)})) {
                                    if (selectedCategory == NavCategory::ElemButton) {
                                        if (auto row = ui.Wrap({.gap = 8.0f, .runGap = 8.0f, .align = guikit::Alignment::Center})) {
                                            ui.Button({.label = "Default", .variant = guikit::UiVariant::Default, .size = controlsSize});
                                            ui.Button({.label = "Primary", .variant = guikit::UiVariant::Primary, .size = controlsSize});
                                            ui.Button({.label = "Secondary", .variant = guikit::UiVariant::Secondary, .size = controlsSize});
                                            ui.Button({.label = "Success", .variant = guikit::UiVariant::Success, .size = controlsSize});
                                            ui.Button({.label = "Warning", .variant = guikit::UiVariant::Warning, .size = controlsSize});
                                            ui.Button({.label = "Danger", .variant = guikit::UiVariant::Danger, .size = controlsSize});
                                            ui.Button({.label = "Info", .variant = guikit::UiVariant::Info, .size = controlsSize});
                                            ui.Button({.label = "Ghost", .variant = guikit::UiVariant::Ghost, .size = controlsSize});
                                        }
                                    } else {
                                        if (auto grid = ui.Grid({.gap = 12.0f, .runGap = 8.0f})) {
                                            if (auto c = grid.Cell4()) { ui.Button({.label = "Default Variant", .variant = guikit::UiVariant::Default, .stretch = true, .size = controlsSize}); }
                                            if (auto c = grid.Cell4()) { ui.Button({.label = "Primary Variant", .variant = guikit::UiVariant::Primary, .stretch = true, .size = controlsSize}); }
                                            if (auto c = grid.Cell4()) { ui.Button({.label = "Secondary Variant", .variant = guikit::UiVariant::Secondary, .stretch = true, .size = controlsSize}); }
                                            if (auto c = grid.Cell4()) { ui.Button({.label = "Success Variant", .variant = guikit::UiVariant::Success, .stretch = true, .size = controlsSize}); }
                                            if (auto c = grid.Cell4()) { ui.Button({.label = "Warning Variant", .variant = guikit::UiVariant::Warning, .stretch = true, .size = controlsSize}); }
                                            if (auto c = grid.Cell4()) { ui.Button({.label = "Danger Variant", .variant = guikit::UiVariant::Danger, .stretch = true, .size = controlsSize}); }
                                        }
                                    }
                                }

                                // C++20 Immediate Mode Code Generator Card
                                if (auto codeCard = ui.Card({.gap = 8.0f, .padding = guikit::Padding::All(12.0f)})) {
                                    std::string snippet;
                                    switch (selectedCategory) {
                                        case NavCategory::ElemButton: {
                                            snippet = std::format(
                                                "ui.Button({{\n"
                                                "    .label = \"{}\",\n"
                                                "{}"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "{}"
                                                "{}"
                                                "{}"
                                                "{}"
                                                "    .size = guikit::UiSize::{}\n"
                                                "}});",
                                                btnClicks > 0 ? std::format("Clicked {}x", btnClicks) : "Interactive Button",
                                                btnHasIcon ? "    .icon = guikit::Icon::Play(),\n" : "",
                                                btnVariant == guikit::UiVariant::Primary ? "Primary" :
                                                (btnVariant == guikit::UiVariant::Secondary ? "Secondary" :
                                                (btnVariant == guikit::UiVariant::Success ? "Success" :
                                                (btnVariant == guikit::UiVariant::Warning ? "Warning" :
                                                (btnVariant == guikit::UiVariant::Danger ? "Danger" :
                                                (btnVariant == guikit::UiVariant::Info ? "Info" :
                                                (btnVariant == guikit::UiVariant::Ghost ? "Ghost" : "Default")))))),
                                                btnHasIcon ? std::format("    .iconPlacement = guikit::Placement::{},\n",
                                                    btnIconPlacement == guikit::Placement::Leading ? "Leading" :
                                                    (btnIconPlacement == guikit::Placement::Trailing ? "Trailing" :
                                                    (btnIconPlacement == guikit::Placement::Top ? "Top" : "Bottom"))) : "",
                                                btnAlign != guikit::Align::Center ? (btnAlign == guikit::Align::Start ? "    .align = guikit::Align::Start,\n" : "    .align = guikit::Align::End,\n") : "",
                                                btnStretch ? "    .stretch = true,\n" : "",
                                                btnDisabled ? "    .disabled = true,\n" : "",
                                                controlsSize == guikit::UiSize::Xs ? "Xs" :
                                                (controlsSize == guikit::UiSize::Sm ? "Sm" :
                                                (controlsSize == guikit::UiSize::Lg ? "Lg" :
                                                (controlsSize == guikit::UiSize::Xl ? "Xl" : "Md")))
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemSlider: {
                                            if (sliderContinuous) {
                                                snippet = std::format(
                                                    "ui.Slider(gainValue, 0.0f, 100.0f, {{\n"
                                                    "    .label = \"Analog Master Gain\",\n"
                                                    "    .format = \"%.1f %%\",\n"
                                                    "    .showValue = {},\n"
                                                    "    .variant = guikit::UiVariant::{},\n"
                                                    "    .size = guikit::UiSize::{}{}\n"
                                                    "}});",
                                                    sliderShowValue ? "true" : "false",
                                                    btnVariant == guikit::UiVariant::Primary ? "Primary" : "Default",
                                                    controlsSize == guikit::UiSize::Sm ? "Sm" : (controlsSize == guikit::UiSize::Lg ? "Lg" : "Md"),
                                                    sliderDisabled ? ",\n    .disabled = true" : ""
                                                );
                                            } else {
                                                snippet = std::format(
                                                    "ui.StepSlider(presetIndex, filterLabels, {{\n"
                                                    "    .label = \"DSP Filter Mode\",\n"
                                                    "    .showTicks = {},\n"
                                                    "    .showLabels = {},\n"
                                                    "    .showValue = {},\n"
                                                    "    .variant = guikit::UiVariant::{},\n"
                                                    "    .size = guikit::UiSize::{}{}\n"
                                                    "}});",
                                                    sliderShowTicks ? "true" : "false",
                                                    sliderShowLabels ? "true" : "false",
                                                    sliderShowValue ? "true" : "false",
                                                    btnVariant == guikit::UiVariant::Primary ? "Primary" : "Default",
                                                    controlsSize == guikit::UiSize::Sm ? "Sm" : (controlsSize == guikit::UiSize::Lg ? "Lg" : "Md"),
                                                    sliderDisabled ? ",\n    .disabled = true" : ""
                                                );
                                            }
                                            break;
                                        }

                                        case NavCategory::ElemInputField: {
                                            snippet = std::format(
                                                "ui.InputField(textValue, {{\n"
                                                "    .label = \"Server Endpoint\",\n"
                                                "    .description = \"Configured host address\",\n"
                                                "    .placeholder = \"Enter domain...\",\n"
                                                "{}"
                                                "{}"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .cursorMode = guikit::CursorBlinkMode::{},\n"
                                                "    .size = guikit::UiSize::{}{}{}\n"
                                                "}});",
                                                inputHasPrefix ? "    .prefix = \"https://\",\n" : "",
                                                inputHasSuffix ? "    .suffix = \":443\",\n" : "",
                                                btnVariant == guikit::UiVariant::Primary ? "Primary" : "Default",
                                                inputCursorMode == guikit::CursorBlinkMode::Solid ? "Solid" :
                                                (inputCursorMode == guikit::CursorBlinkMode::Hidden ? "Hidden" : "Blinking"),
                                                controlsSize == guikit::UiSize::Sm ? "Sm" : (controlsSize == guikit::UiSize::Lg ? "Lg" : "Md"),
                                                inputDisabled ? ",\n    .disabled = true" : "",
                                                inputReadOnly ? ",\n    .readOnly = true" : ""
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemSelect: {
                                            snippet = std::format(
                                                "ui.Select(selectedIndex, itemsList, {{\n"
                                                "    .label = \"Active Preset\",\n"
                                                "    .description = \"Dropdown selection\",\n"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .maxVisibleItems = {},\n"
                                                "    .size = guikit::UiSize::{}{}\n"
                                                "}});",
                                                btnVariant == guikit::UiVariant::Primary ? "Primary" : "Default",
                                                selectMaxItems,
                                                controlsSize == guikit::UiSize::Sm ? "Sm" : (controlsSize == guikit::UiSize::Lg ? "Lg" : "Md"),
                                                selectDisabled ? ",\n    .disabled = true" : ""
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemSwitch: {
                                            auto placementName = [](guikit::Placement p) -> std::string_view {
                                                switch (p) {
                                                    case guikit::Placement::Leading:  return "Leading";
                                                    case guikit::Placement::Trailing: return "Trailing";
                                                    case guikit::Placement::Top:      return "Top";
                                                    case guikit::Placement::Bottom:   return "Bottom";
                                                    case guikit::Placement::Stacked:  return "Stacked";
                                                    case guikit::Placement::Both:     return "Both";
                                                }
                                                return "Trailing";
                                            };
                                            snippet = std::format(
                                                "ui.Switch(switchState, {{\n"
                                                "    .label = \"{}\",\n"
                                                "{}"
                                                "{}"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .labelPlacement = guikit::Placement::{},\n"
                                                "{}"
                                                "    .size = guikit::UiSize::{}{}\n"
                                                "}});",
                                                (switchLabelPlace == guikit::Placement::Both) ? "Dark" : "Hardware E-Stop",
                                                (switchLabelPlace == guikit::Placement::Both) ? "    .trailingLabel = \"Light\",\n" : "",
                                                switchShowDesc ? "    .description = \"Safety interlocking system\",\n" : "",
                                                btnVariant == guikit::UiVariant::Primary ? "Primary" : "Default",
                                                placementName(switchLabelPlace),
                                                switchSymmetric ? "    .symmetric = true,\n" : "",
                                                controlsSize == guikit::UiSize::Sm ? "Sm" : (controlsSize == guikit::UiSize::Lg ? "Lg" : "Md"),
                                                switchDisabled ? ",\n    .disabled = true" : ""
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemCheckbox: {
                                            auto placementName = [](guikit::Placement p) -> std::string_view {
                                                switch (p) {
                                                    case guikit::Placement::Leading:  return "Leading";
                                                    case guikit::Placement::Trailing: return "Trailing";
                                                    case guikit::Placement::Top:      return "Top";
                                                    case guikit::Placement::Bottom:   return "Bottom";
                                                    case guikit::Placement::Stacked:  return "Stacked";
                                                    case guikit::Placement::Both:     return "Both";
                                                }
                                                return "Trailing";
                                            };
                                            snippet = std::format(
                                                "ui.Checkbox(checkState, {{\n"
                                                "    .label = \"Telemetry Logging\",\n"
                                                "{}"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .labelPlacement = guikit::Placement::{},\n"
                                                "{}"
                                                "    .size = guikit::UiSize::{}{}\n"
                                                "}});",
                                                checkboxShowDesc ? "    .description = \"Save trace to buffer\",\n" : "",
                                                btnVariant == guikit::UiVariant::Primary ? "Primary" : "Default",
                                                placementName(checkboxLabelPlace),
                                                testCheckboxIndet ? "    .indeterminate = true,\n" : "",
                                                controlsSize == guikit::UiSize::Sm ? "Sm" : (controlsSize == guikit::UiSize::Lg ? "Lg" : "Md"),
                                                checkboxDisabled ? ",\n    .disabled = true" : ""
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemTabBar: {
                                            snippet = std::format(
                                                "ui.TabBar(activeTab, tabItems, {{\n"
                                                "    .style = guikit::TabBarStyle::{},\n"
                                                "    .showSeparator = {},\n"
                                                "    .fullWidth = {},\n"
                                                "    .scrollable = {},\n"
                                                "    .gap = {:.1f}f,\n"
                                                "    .padding = guikit::Padding::All({:.1f}f),\n"
                                                "    .radius = {:.1f}f,\n"
                                                "    .backgroundColor = theme.palette.surfaceElevated,\n"
                                                "    .borderColor = theme.palette.borderSubtle,\n"
                                                "    .borderWidth = {:.1f}f\n"
                                                "}});",
                                                tabStyle == guikit::TabBarStyle::Pill ? "Pill" : "Underline",
                                                testTabShowSep ? "true" : "false",
                                                testTabFullWidth ? "true" : "false",
                                                testTabScrollable ? "true" : "false",
                                                tabGap,
                                                testTabHasBg ? testTabPadding : 0.0f,
                                                testTabHasBg ? testTabRadius : 0.0f,
                                                testTabHasBg ? testTabBorderWidth : 0.0f
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemContainers: {
                                            snippet = std::format(
                                                "if (auto card = ui.Card({{\n"
                                                "    .gap = {:.1f}f,\n"
                                                "    .padding = guikit::Padding({:.1f}f),\n"
                                                "    .borderWidth = {:.1f}f,\n"
                                                "    .radius = {:.1f}f\n"
                                                "}})) {{\n"
                                                "    ui.Text(\"Card Surface Component\");\n"
                                                "}}",
                                                cardGap, cardPadding, cardBorderWidth, cardRadius
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemTypography: {
                                            snippet = std::format(
                                                "ui.Heading({{\n"
                                                "    .title = \"Typographic Heading Sample\",\n"
                                                "    .subtitle = \"Dynamic typography scale test\",\n"
                                                "    .icon = guikit::Icon::Edit(),\n"
                                                "    .size = guikit::UiSize::{},\n"
                                                "    .divider = {},\n"
                                                "    .wordWrap = {}\n"
                                                "}});",
                                                headingSize == guikit::UiSize::Sm ? "Sm" : (headingSize == guikit::UiSize::Lg ? "Lg" : "Md"),
                                                headingDivider ? "true" : "false",
                                                headingWordWrap ? "true" : "false"
                                            );
                                            break;
                                        }

                                        default: break;
                                    }

                                    ui.TextArea(snippet, {
                                        .autoHeight = true,
                                        .readOnly = true,
                                        .showLineNumbers = true,
                                        .monospace = true,
                                        .syntaxHighlighter = &guikit::TextArea::CppHighlighter
                                    });
                                }
                            } else {
                                // -------------------------------------------------------------
                                // ASSEMBLY MODE: Real-World Screens & Compositions
                                // -------------------------------------------------------------
                                switch (selectedCategory) {
                                    case NavCategory::AsmDashboard: {
                                        // Industrial Dashboard (responsive 2-column layout)
                                        if (auto dashboardGrid = ui.Grid(makeGrid())) {
                                            // Left Column
                                            if (auto colLeft = dashboardGrid.Cell6({.gap = colGap})) {
                                                if (auto card1 = ui.Card(makeCard())) {
                                                    ui.Heading({.title = "Primary Stop Conditions", .subtitle = "Sensory tripwire thresholds & limits", .size = headingSize, .wordWrap = headingWordWrap});
                                                    if (auto grid1 = ui.Grid(makeGrid())) {
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

                                                if (auto card2 = ui.Card(makeCard())) {
                                                    ui.Heading({.title = "Emergency Interlocks", .subtitle = "Hardware line supervision & failsafes", .variant = guikit::UiVariant::Danger, .size = headingSize, .wordWrap = headingWordWrap});
                                                    if (auto grid2 = ui.Grid(makeGrid())) {
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

                                            // Right Column
                                            if (auto colRight = dashboardGrid.Cell6({.gap = colGap})) {
                                                if (auto card3 = ui.Card(makeCard())) {
                                                    ui.Heading({.title = "Secondary Criteria & PID", .subtitle = "Dynamic closed-loop feedback parameters", .size = headingSize, .wordWrap = headingWordWrap});
                                                    if (auto grid3 = ui.Grid(makeGrid())) {
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

                                        if (auto cardAudit = ui.Card(makeCard())) {
                                            ui.Heading({
                                                .title = "Supervisor Hardware Loop Diagnostics",
                                                .subtitle = "Live bus performance telemetry and continuous line supervision",
                                                .icon = guikit::Icon::Check(),
                                                .variant = guikit::UiVariant::Success,
                                                .size = headingSize,
                                                .wordWrap = headingWordWrap
                                            });
                                            if (auto g = ui.Grid(makeGrid())) {
                                                if (auto c1 = g.Cell4()) {
                                                    ui.Text("Safety Loop: CLOSED (Nominal)", {.role = guikit::TextRole::BodyMedium, .variant = guikit::UiVariant::Success});
                                                }
                                                if (auto c2 = g.Cell4()) {
                                                    ui.Text("Bus Jitter: < 0.12 ms", {.role = guikit::TextRole::CodeMedium, .variant = guikit::UiVariant::Info});
                                                }
                                                if (auto c3 = g.Cell4()) {
                                                    ui.Text("Supervisor Heartbeat: 1000 Hz", {.role = guikit::TextRole::CodeMedium});
                                                }
                                            }
                                        }
                                        break;
                                    }

                                    case NavCategory::AsmFormControls: {
                                        // Overview Tab: Interactive Counter & Form
                                        if (auto card = ui.Card(makeCard())) {
                                            ui.Heading({.title = "Interactive Reactive State", .subtitle = "Pure immediate mode with zero external widget IDs and instant response.", .size = headingSize, .wordWrap = headingWordWrap});

                                            if (auto actionRow = ui.Wrap({.gap = rowGap, .runGap = colGap, .align = guikit::Alignment::Center})) {
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

                                            if (auto togglesRow = ui.Wrap({.gap = rowGap, .runGap = colGap, .align = guikit::Alignment::Center})) {
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
                                                    .variant = guikit::UiVariant::Success,
                                                    .size = headingSize,
                                                    .wordWrap = headingWordWrap
                                                });
                                            }

                                            const guikit::Placement activePlacement = formMode ? guikit::Placement::Stacked : guikit::Placement::Leading;

                                            if (auto fieldsGrid = ui.Grid(makeGrid())) {
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

                                            ui.Heading({
                                                .title = "Precision Continuous & Stepped Sliders",
                                                .subtitle = "Immediate-mode numeric sliders with wheel scrubbing, ticks, and step presets",
                                                .icon = guikit::Icon::Settings(),
                                                .size = headingSize,
                                                .wordWrap = headingWordWrap
                                            });

                                            if (auto sliderGrid = ui.Grid(makeGrid())) {
                                                if (auto c1 = sliderGrid.Cell4()) {
                                                    ui.Slider(masterGain, 0.0f, 100.0f, {
                                                        .label = "Master Analog Gain",
                                                        .suffix = "dB",
                                                        .format = "%.1f"
                                                    });
                                                }
                                                if (auto c2 = sliderGrid.Cell4()) {
                                                    ui.Slider(channelCount, 1, 8, {
                                                        .label = "ADC Active Channels",
                                                        .suffix = "ch",
                                                        .step = 1.0f
                                                    });
                                                }
                                                if (auto c3 = sliderGrid.Cell4()) {
                                                    ui.StepSlider(filterPreset, filterLabels, {
                                                        .label = "DSP Filter Mode",
                                                        .showTicks = true,
                                                        .showLabels = true
                                                    });
                                                }
                                            }
                                        }
                                        break;
                                    }

                                    case NavCategory::AsmFlexRibbon: {
                                        // Flexbox Demo: Equal Stretch & Alignment
                                        if (auto card = ui.Card(makeCard())) {
                                            ui.Heading({.title = "Dynamic 3-Way Equal Stretch Layout", .subtitle = "Each button automatically receives exactly 33.3% available width via stretch = true.", .size = headingSize, .wordWrap = headingWordWrap});

                                            if (auto stretchRow = ui.Row({.gap = rowGap})) {
                                                ui.Button({.label = "Col 1 (Stretch)", .stretch = true});
                                                ui.Button({.label = "Col 2 (Stretch)", .stretch = true});
                                                ui.Button({.label = "Col 3 (Stretch)", .stretch = true});
                                            }

                                            ui.Heading({.title = "Horizontal Carousel Ribbon", .subtitle = "Autonomous scroll container with navigation arrows and mouse wheel support.", .size = headingSize, .wordWrap = headingWordWrap});
                                            if (auto ribbon = ui.Carousel({.gap = carouselGap})) {
                                                for (int k = 1; k <= 10; ++k) {
                                                    ui.Button({
                                                        .label = ui.Format("Module #{:02d}", k),
                                                        .variant = (k % 3 == 0) ? guikit::UiVariant::Primary : guikit::UiVariant::Default
                                                    });
                                                }
                                            }
                                        }
                                        break;
                                    }

                                    case NavCategory::AsmDiagnostics: {
                                        // Diagnostics & System Audit
                                        if (auto card = ui.Card(makeCard())) {
                                            ui.Heading({
                                                .title = "System Diagnostics & Network Audit",
                                                .subtitle = "Operational module telemetry stream and live system diagnostics.",
                                                .icon = guikit::Icon::Check(),
                                                .size = headingSize,
                                                .wordWrap = headingWordWrap
                                            });
                                            if (auto grid = ui.Grid(makeGrid())) {
                                                if (auto c1 = grid.Cell6()) {
                                                    ui.Text("Channel Status: ONLINE | Active bus node: [SUPERVISOR]",
                                                            {.role = guikit::TextRole::BodyMedium, .variant = guikit::UiVariant::Success});
                                                }
                                                if (auto c2 = grid.Cell6()) {
                                                    ui.Text("Latency: 0.18 ms | Packet Drop: 0.00% | CRC: OK",
                                                            {.role = guikit::TextRole::CodeMedium, .variant = guikit::UiVariant::Info});
                                                }
                                                if (auto c3 = grid.Cell6()) {
                                                    ui.Text("Memory Arena High Watermark: 24.8 KB",
                                                            {.role = guikit::TextRole::CodeMedium, .variant = guikit::UiVariant::Secondary});
                                                }
                                                if (auto c4 = grid.Cell6()) {
                                                    ui.Text("Frame Damage Rate: 0 full repaints / 99.8% partial",
                                                            {.role = guikit::TextRole::CodeMedium, .variant = guikit::UiVariant::Success});
                                                }
                                            }
                                        }
                                        break;
                                    }

                                    case NavCategory::AsmThemeStudio: {
                                        // Theme & Palette Studio
                                        if (auto card = ui.Card(makeCard())) {
                                            ui.Heading({
                                                .title = "Theme & Palette Studio",
                                                .subtitle = "Live token tweaking and contrast validation",
                                                .size = headingSize,
                                                .wordWrap = headingWordWrap
                                            });

                                            ui.Heading({.title = "Quick Accent Color Presets", .size = guikit::UiSize::Sm});
                                            if (auto accentRow = ui.Wrap({.gap = 8.0f, .runGap = 8.0f})) {
                                                auto makePreset = [&](std::string_view name, guikit::Color col) {
                                                    if (ui.Button({.label = name, .size = guikit::UiSize::Sm})) {
                                                        th.palette.accent = col;
                                                        th.GetVariant(guikit::UiVariant::Primary).bg = col;
                                                        th.palette.focusRing = col;
                                                        ui.InvalidateAll();
                                                    }
                                                };
                                                makePreset("Cyan", guikit::Color(6, 182, 212));
                                                makePreset("Blue", guikit::Color(59, 130, 246));
                                                makePreset("Purple", guikit::Color(168, 85, 247));
                                                makePreset("Emerald", guikit::Color(16, 185, 129));
                                                makePreset("Rose", guikit::Color(244, 63, 94));
                                                makePreset("Amber", guikit::Color(245, 158, 11));
                                            }

                                            ui.Heading({.title = "Base Palette Tokens", .size = guikit::UiSize::Sm});
                                            auto makeColorSliders = [&](std::string_view title, guikit::Color& c) {
                                                float r = c.r, g = c.g, b = c.b;
                                                bool changed = false;
                                                ui.Text(title, {.role = guikit::TextRole::LabelMedium});
                                                if (auto g_row = ui.Grid({.gap = 8.0f})) {
                                                    if (auto c1 = g_row.Cell4()) { if (ui.Slider(r, 0.0f, 255.0f, {.label = "R", .format = "%.0f"}).changed) changed = true; }
                                                    if (auto c2 = g_row.Cell4()) { if (ui.Slider(g, 0.0f, 255.0f, {.label = "G", .format = "%.0f"}).changed) changed = true; }
                                                    if (auto c3 = g_row.Cell4()) { if (ui.Slider(b, 0.0f, 255.0f, {.label = "B", .format = "%.0f"}).changed) changed = true; }
                                                }
                                                if (changed) {
                                                    c = guikit::Color(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), c.a);
                                                    ui.InvalidateAll();
                                                }
                                            };

                                            makeColorSliders("Primary Accent Fill", th.palette.accent);
                                            makeColorSliders("Surface Color", th.palette.surface);
                                            makeColorSliders("Background Color", th.palette.background);
                                        }
                                        break;
                                    }

                                    default: break;
                                }
                            }
                        }
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
