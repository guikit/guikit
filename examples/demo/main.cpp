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

    int winWidth = 1120;
    int winHeight = 740;

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
    size_t btnStretchIdx = 0; // Compact
    float btnWidth = 0.0f;    // 0 = auto
    bool btnDisabled = false;
    int btnClicks = 0;
    float btnCornerRadius = 6.0f;
    bool btnShowLabel = false;
    bool btnShowDesc = false;
    guikit::WidgetLayout btnLayout = guikit::WidgetLayout::Stacked;
    guikit::ContentOrder btnOrder = guikit::ContentOrder::Leading;
    guikit::Align btnLabelAlign = guikit::Align::Start;

    float testSliderVal = 65.0f;
    size_t testStepIdx = 2;
    bool sliderContinuous = true;
    size_t sliderStretchIdx = 2; // Fill
    float sliderWidth = 0.0f;
    bool sliderShowLabel = true;
    bool sliderShowDesc = false;
    bool sliderShowTicks = true;
    bool sliderShowLabels = true;
    bool sliderShowValue = true;
    bool sliderDisabled = false;
    guikit::WidgetLayout sliderLayout = guikit::WidgetLayout::Stacked;
    guikit::ContentOrder sliderOrder = guikit::ContentOrder::Leading;
    guikit::Align sliderLabelAlign = guikit::Align::Start;

    std::string testInputText = "guikit.dev";
    guikit::CursorBlinkMode inputCursorMode = guikit::CursorBlinkMode::Blinking;
    bool inputReadOnly = false;
    bool inputDisabled = false;
    bool inputHasPrefix = true;
    bool inputHasSuffix = false;
    float inputCornerRadius = 6.0f;
    bool inputShowLabel = true;
    bool inputShowDesc = false;
    guikit::WidgetLayout inputLayout = guikit::WidgetLayout::Stacked;
    guikit::ContentOrder inputOrder = guikit::ContentOrder::Leading;
    guikit::Align inputLabelAlign = guikit::Align::Start;
    size_t inputStretchIdx = 2; // Fill
    float inputWidth = 0.0f;

    size_t testSelectIdx = 0;
    static const std::vector<std::string_view> selectItems = {"Option Alpha", "Option Beta", "Option Gamma", "Option Delta", "Option Epsilon"};
    bool selectDisabled = false;
    size_t selectStretchIdx = 2; // Fill
    float selectWidth = 0.0f;
    uint32_t selectMaxItems = 4;
    float selectCornerRadius = 6.0f;
    bool selectShowLabel = true;
    bool selectShowDesc = true;
    guikit::WidgetLayout selectLayout = guikit::WidgetLayout::Stacked;
    guikit::ContentOrder selectOrder = guikit::ContentOrder::Leading;
    guikit::Align selectLabelAlign = guikit::Align::Start;
    bool selectHasPrefix = false;

    bool testSwitchState = true;
    bool testDarkLightSwitch = false;
    guikit::WidgetLayout switchLayout = guikit::WidgetLayout::Inline;
    guikit::ContentOrder switchOrder = guikit::ContentOrder::Trailing;
    guikit::Align switchLabelAlign = guikit::Align::Start;
    size_t switchStretchIdx = 0; // Compact
    bool switchSymmetric = false;
    bool switchShowLabel = true;
    bool switchShowDesc = true;
    bool switchDisabled = false;

    bool testCheckboxState = true;
    bool testCheckboxIndet = false;
    guikit::WidgetLayout checkboxLayout = guikit::WidgetLayout::Inline;
    guikit::ContentOrder checkboxOrder = guikit::ContentOrder::Trailing;
    guikit::Align checkboxLabelAlign = guikit::Align::Start;
    size_t checkboxStretchIdx = 0; // Compact
    bool checkboxShowLabel = true;
    bool checkboxShowDesc = true;
    bool checkboxDisabled = false;

    static constexpr std::array<std::string_view, 3> stretchModeNames = {"Compact", "Between", "Fill"};
    auto stretchModeEnumName = [](guikit::StretchMode m) -> std::string_view {
        switch (m) {
            case guikit::StretchMode::Compact: return "Compact";
            case guikit::StretchMode::Between: return "Between";
            default: return "Fill";
        }
    };

    static constexpr std::array<std::string_view, 4> widgetLayoutNames = {"Stacked", "Top", "Bottom", "Inline"};
    static constexpr std::array<std::string_view, 2> contentOrderNames = {"Leading (Text 1st)", "Trailing (Ctrl 1st)"};
    static constexpr std::array<std::string_view, 3> labelAlignNames = {"Start (Left)", "Center", "End (Right)"};

    auto widgetLayoutEnumName = [](guikit::WidgetLayout l) -> std::string_view {
        switch (l) {
            case guikit::WidgetLayout::Stacked: return "Stacked";
            case guikit::WidgetLayout::Top:     return "Top";
            case guikit::WidgetLayout::Bottom:  return "Bottom";
            case guikit::WidgetLayout::Inline:  return "Inline";
        }
        return "Stacked";
    };

    auto contentOrderEnumName = [](guikit::ContentOrder o) -> std::string_view {
        switch (o) {
            case guikit::ContentOrder::Leading:  return "Leading";
            case guikit::ContentOrder::Trailing: return "Trailing";
        }
        return "Leading";
    };

    auto labelAlignEnumName = [](guikit::Align a) -> std::string_view {
        switch (a) {
            case guikit::Align::Start: return "Start";
            case guikit::Align::Center: return "Center";
            case guikit::Align::End: return "End";
        }
        return "Start";
    };

    static constexpr std::array<std::pair<guikit::UiVariant, std::string_view>, 8> allVariants = {{
        {guikit::UiVariant::Default, "Default"},
        {guikit::UiVariant::Primary, "Primary"},
        {guikit::UiVariant::Secondary, "Secondary"},
        {guikit::UiVariant::Success, "Success"},
        {guikit::UiVariant::Warning, "Warning"},
        {guikit::UiVariant::Danger, "Danger"},
        {guikit::UiVariant::Info, "Info"},
        {guikit::UiVariant::Ghost, "Ghost"}
    }};

    auto variantName = [](guikit::UiVariant v) -> std::string_view {
        switch (v) {
            case guikit::UiVariant::Primary:   return "Primary";
            case guikit::UiVariant::Secondary: return "Secondary";
            case guikit::UiVariant::Success:   return "Success";
            case guikit::UiVariant::Warning:   return "Warning";
            case guikit::UiVariant::Danger:    return "Danger";
            case guikit::UiVariant::Info:      return "Info";
            case guikit::UiVariant::Ghost:     return "Ghost";
            default:                           return "Default";
        }
    };

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
    guikit::UiTheme previewTheme = darkMode ? guikit::UiTheme::Dark(guikit::Density::Standard) : guikit::UiTheme::Light(guikit::Density::Standard);

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
                // Engine HUD Toolbar:
                // Sidebar | Scale Light | VSync React Dirty Heartbeat | Bounds Inspect | guikit
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
                        .symmetric = true,
                        .size = guikit::UiSize::Md
                    }).changed) {
                        darkMode = !lightMode;
                        const auto defTheme = darkMode ? guikit::UiTheme::Dark(previewTheme.density) : guikit::UiTheme::Light(previewTheme.density);
                        previewTheme.palette = defTheme.palette;
                        previewTheme.variants = defTheme.variants;
                        previewTheme.mode = darkMode ? guikit::ThemeMode::Dark : guikit::ThemeMode::Light;
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

                    tb.Spacer();
                    if (ui.Text("https://github.com/guikit/", {.role = guikit::TextRole::TitleMedium, .variant = guikit::UiVariant::Info})) {
                        SDL_OpenURL("https://github.com/guikit/");
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
                            if (auto railCard = ui.Card({
                                .gap = 4.0f,
                                .padding = guikit::Padding::All(8.0f),
                                .radius = 8.0f,
                                .heightMode = guikit::SizeMode::Stretch
                            })) {
                                if (auto railScroll = ui.ScrollArea({.gap = 4.0f})) {
                                    auto makeCatBtn = [&](NavCategory cat, std::string_view label, guikit::Icon icon) {
                                        const bool isSel = (selectedCategory == cat);
                                        if (ui.Button({
                                            .label = label,
                                            .icon = icon,
                                            .variant = isSel ? guikit::UiVariant::Primary : guikit::UiVariant::Ghost,
                                            .iconPlacement = guikit::Placement::Leading,
                                            .align = guikit::Align::Start,
                                            .stretch = guikit::StretchMode::Fill,
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
                            .fixedWidth = 340.0f,
                            .theme = &inspectorTheme,
                            .scale = 1.0f
                        })) {
                            if (auto inspectorCard = ui.Card({
                                .gap = 6.0f,
                                .padding = guikit::Padding::All(12.0f),
                                .radius = 8.0f,
                                .heightMode = guikit::SizeMode::Stretch
                            })) {
                                if (auto inspectorScroll = ui.ScrollArea({.gap = 8.0f})) {
                                    ui.Heading({
                                        .title = "INSPECTOR",
                                        .size = guikit::UiSize::Xs,
                                        .divider = true
                                    });

                                    auto& th = previewTheme;
                                    auto renderVariantPicker = [&]() {
                                        if (ui.StepSlider(btnVariant, {
                                            .label = "Semantic Variant",
                                            .showTicks = true,
                                            .showLabels = true,
                                            .showValue = true
                                        }).changed) {
                                            ui.InvalidateAll();
                                        }
                                    };

                                    if (isElementCategory(selectedCategory)) {
                                        switch (selectedCategory) {
                                            case NavCategory::ElemButton: {
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
                                                if (ui.Slider(btnCornerRadius, 0.0f, 24.0f, {.label = "Corner Radius", .format = "%.0f px"}).changed) {
                                                    ui.InvalidateAll();
                                                }

                                                renderVariantPicker();

                                                if (ui.Switch(btnShowLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(btnShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                const bool btnHasExternal = btnShowLabel || btnShowDesc;

                                                size_t btnLayoutIdx = static_cast<size_t>(btnLayout);
                                                if (ui.StepSlider(btnLayoutIdx, widgetLayoutNames, {.label = "Widget Layout", .showTicks = true, .showLabels = true, .disabled = !btnHasExternal}).changed) {
                                                    btnLayout = static_cast<guikit::WidgetLayout>(btnLayoutIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t btnOrderIdx = static_cast<size_t>(btnOrder);
                                                if (ui.StepSlider(btnOrderIdx, contentOrderNames, {.label = "Content Order", .showTicks = true, .showLabels = true, .disabled = !btnHasExternal}).changed) {
                                                    btnOrder = static_cast<guikit::ContentOrder>(btnOrderIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t btnAlignIdx = static_cast<size_t>(btnLabelAlign);
                                                if (ui.StepSlider(btnAlignIdx, labelAlignNames, {.label = "Label Alignment", .showTicks = true, .showLabels = true, .disabled = !btnHasExternal}).changed) {
                                                    btnLabelAlign = static_cast<guikit::Align>(btnAlignIdx);
                                                    ui.InvalidateAll();
                                                }

                                                if (ui.StepSlider(btnStretchIdx, stretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
                                                if (ui.Slider(btnWidth, 0.0f, 400.0f, {.label = "Explicit Width", .format = (btnWidth > 0.0f) ? "%.0f px" : "Auto", .step = 10.0}).changed) ui.InvalidateAll();
                                                static constexpr std::array<std::string_view, 3> alignNames = {"Start (Left)", "Center", "End (Right)"};
                                                size_t alignIdx = (btnAlign == guikit::Align::Start) ? 0 : ((btnAlign == guikit::Align::End) ? 2 : 1);
                                                if (ui.StepSlider(alignIdx, alignNames, {.label = "Content Alignment", .showTicks = true, .showLabels = true}).changed) {
                                                    btnAlign = (alignIdx == 0) ? guikit::Align::Start : ((alignIdx == 2) ? guikit::Align::End : guikit::Align::Center);
                                                    ui.InvalidateAll();
                                                }
                                                if (ui.Switch(btnHasIcon, {.label = "Show Icon"}).changed) ui.InvalidateAll();
                                                static constexpr std::array<std::string_view, 4> placeNames = {"Leading", "Trailing", "Top", "Bottom"};
                                                size_t placeIdx = (btnIconPlacement == guikit::Placement::Trailing) ? 1 : ((btnIconPlacement == guikit::Placement::Top) ? 2 : ((btnIconPlacement == guikit::Placement::Bottom) ? 3 : 0));
                                                if (ui.StepSlider(placeIdx, placeNames, {.label = "Icon Placement", .showTicks = true, .showLabels = true, .disabled = !btnHasIcon}).changed) {
                                                    btnIconPlacement = (placeIdx == 1) ? guikit::Placement::Trailing : ((placeIdx == 2) ? guikit::Placement::Top : ((placeIdx == 3) ? guikit::Placement::Bottom : guikit::Placement::Leading));
                                                    ui.InvalidateAll();
                                                }
                                                if (ui.Switch(btnDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemSlider: {
                                                if (ui.Switch(sliderContinuous, {.label = "Continuous Mode", .description = "Continuous vs Quantized StepSlider"}).changed) ui.InvalidateAll();
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();

                                                renderVariantPicker();

                                                if (ui.StepSlider(sliderStretchIdx, stretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
                                                if (ui.Slider(sliderWidth, 0.0f, 400.0f, {.label = "Explicit Track Width", .format = (sliderWidth > 0.0f) ? "%.0f px" : "Default", .step = 10.0}).changed) ui.InvalidateAll();
                                                if (ui.Switch(sliderShowLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(sliderShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                const bool sliderHasExternal = sliderShowLabel || sliderShowDesc;

                                                size_t slLayoutIdx = static_cast<size_t>(sliderLayout);
                                                if (ui.StepSlider(slLayoutIdx, widgetLayoutNames, {.label = "Widget Layout", .showTicks = true, .showLabels = true, .disabled = !sliderHasExternal}).changed) {
                                                    sliderLayout = static_cast<guikit::WidgetLayout>(slLayoutIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t slOrderIdx = static_cast<size_t>(sliderOrder);
                                                if (ui.StepSlider(slOrderIdx, contentOrderNames, {.label = "Content Order", .showTicks = true, .showLabels = true, .disabled = !sliderHasExternal}).changed) {
                                                    sliderOrder = static_cast<guikit::ContentOrder>(slOrderIdx);
                                                    ui.InvalidateAll();
                                                }
 
                                                size_t slAlignIdx = static_cast<size_t>(sliderLabelAlign);
                                                if (ui.StepSlider(slAlignIdx, labelAlignNames, {.label = "Label Alignment", .showTicks = true, .showLabels = true, .disabled = !sliderHasExternal}).changed) {
                                                    sliderLabelAlign = static_cast<guikit::Align>(slAlignIdx);
                                                    ui.InvalidateAll();
                                                }
                                                if (ui.Switch(sliderShowValue, {.label = "Show Value Tag"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(sliderShowTicks, {.label = "Show Step Ticks", .disabled = sliderContinuous}).changed) ui.InvalidateAll();
                                                if (ui.Switch(sliderShowLabels, {.label = "Show Step Labels", .disabled = sliderContinuous}).changed) ui.InvalidateAll();
                                                if (ui.Switch(sliderDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemInputField: {
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
                                                if (ui.Slider(inputCornerRadius, 0.0f, 24.0f, {.label = "Corner Radius", .format = "%.0f px"}).changed) {
                                                    ui.InvalidateAll();
                                                }

                                                renderVariantPicker();

                                                if (ui.StepSlider(inputStretchIdx, stretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
                                                if (ui.Slider(inputWidth, 0.0f, 400.0f, {.label = "Explicit Width", .format = (inputWidth > 0.0f) ? "%.0f px" : "Default", .step = 10.0}).changed) ui.InvalidateAll();
                                                if (ui.Switch(inputShowLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(inputShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                const bool inHasExternal = inputShowLabel || inputShowDesc;

                                                size_t inLayoutIdx = static_cast<size_t>(inputLayout);
                                                if (ui.StepSlider(inLayoutIdx, widgetLayoutNames, {.label = "Widget Layout", .showTicks = true, .showLabels = true, .disabled = !inHasExternal}).changed) {
                                                    inputLayout = static_cast<guikit::WidgetLayout>(inLayoutIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t inOrderIdx = static_cast<size_t>(inputOrder);
                                                if (ui.StepSlider(inOrderIdx, contentOrderNames, {.label = "Content Order", .showTicks = true, .showLabels = true, .disabled = !inHasExternal}).changed) {
                                                    inputOrder = static_cast<guikit::ContentOrder>(inOrderIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t inAlignIdx = static_cast<size_t>(inputLabelAlign);
                                                if (ui.StepSlider(inAlignIdx, labelAlignNames, {.label = "Label Alignment", .showTicks = true, .showLabels = true, .disabled = !inHasExternal}).changed) {
                                                    inputLabelAlign = static_cast<guikit::Align>(inAlignIdx);
                                                    ui.InvalidateAll();
                                                }

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
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
                                                if (ui.Slider(selectCornerRadius, 0.0f, 24.0f, {.label = "Corner Radius", .format = "%.0f px"}).changed) {
                                                    ui.InvalidateAll();
                                                }
                                                float maxItemsFloat = static_cast<float>(selectMaxItems);
                                                if (ui.Slider(maxItemsFloat, 2.0f, 8.0f, {.label = "Max Visible Menu Items", .format = "%.0f items", .step = 1.0}).changed) {
                                                    selectMaxItems = static_cast<uint32_t>(maxItemsFloat);
                                                    ui.InvalidateAll();
                                                }

                                                renderVariantPicker();

                                                if (ui.StepSlider(selectStretchIdx, stretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
                                                if (ui.Slider(selectWidth, 0.0f, 400.0f, {.label = "Explicit Width", .format = (selectWidth > 0.0f) ? "%.0f px" : "Auto", .step = 10.0}).changed) ui.InvalidateAll();
                                                if (ui.Switch(selectShowLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(selectShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                const bool selHasExternal = selectShowLabel || selectShowDesc;

                                                size_t selLayoutIdx = static_cast<size_t>(selectLayout);
                                                if (ui.StepSlider(selLayoutIdx, widgetLayoutNames, {.label = "Widget Layout", .showTicks = true, .showLabels = true, .disabled = !selHasExternal}).changed) {
                                                    selectLayout = static_cast<guikit::WidgetLayout>(selLayoutIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t selOrderIdx = static_cast<size_t>(selectOrder);
                                                if (ui.StepSlider(selOrderIdx, contentOrderNames, {.label = "Content Order", .showTicks = true, .showLabels = true, .disabled = !selHasExternal}).changed) {
                                                    selectOrder = static_cast<guikit::ContentOrder>(selOrderIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t selAlignIdx = static_cast<size_t>(selectLabelAlign);
                                                if (ui.StepSlider(selAlignIdx, labelAlignNames, {.label = "Label Alignment", .showTicks = true, .showLabels = true, .disabled = !selHasExternal}).changed) {
                                                    selectLabelAlign = static_cast<guikit::Align>(selAlignIdx);
                                                    ui.InvalidateAll();
                                                }

                                                if (ui.Switch(selectHasPrefix, {.label = "Prefix Icon"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(selectDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemSwitch: {
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();

                                                renderVariantPicker();

                                                if (ui.StepSlider(switchStretchIdx, stretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
                                                if (ui.Switch(switchShowLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(switchShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                const bool switchHasExternal = switchShowLabel || switchShowDesc;

                                                size_t swLayoutIdx = static_cast<size_t>(switchLayout);
                                                if (ui.StepSlider(swLayoutIdx, widgetLayoutNames, {.label = "Widget Layout", .showTicks = true, .showLabels = true, .disabled = !switchHasExternal}).changed) {
                                                    switchLayout = static_cast<guikit::WidgetLayout>(swLayoutIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t swOrderIdx = static_cast<size_t>(switchOrder);
                                                if (ui.StepSlider(swOrderIdx, contentOrderNames, {.label = "Content Order", .showTicks = true, .showLabels = true, .disabled = !switchHasExternal}).changed) {
                                                    switchOrder = static_cast<guikit::ContentOrder>(swOrderIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t swAlignIdx = static_cast<size_t>(switchLabelAlign);
                                                if (ui.StepSlider(swAlignIdx, labelAlignNames, {.label = "Label Alignment", .showTicks = true, .showLabels = true, .disabled = !switchHasExternal}).changed) {
                                                    switchLabelAlign = static_cast<guikit::Align>(swAlignIdx);
                                                    ui.InvalidateAll();
                                                }

                                                if (ui.Switch(testDarkLightSwitch, {.label = "Dual Labels (Dark / Light)"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(switchSymmetric, {.label = "Symmetric (Neutral Track)"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(switchDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemCheckbox: {
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();

                                                renderVariantPicker();

                                                if (ui.StepSlider(checkboxStretchIdx, stretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
                                                if (ui.Switch(checkboxShowLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(checkboxShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                const bool checkHasExternal = checkboxShowLabel || checkboxShowDesc;

                                                size_t cbLayoutIdx = static_cast<size_t>(checkboxLayout);
                                                if (ui.StepSlider(cbLayoutIdx, widgetLayoutNames, {.label = "Widget Layout", .showTicks = true, .showLabels = true, .disabled = !checkHasExternal}).changed) {
                                                    checkboxLayout = static_cast<guikit::WidgetLayout>(cbLayoutIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t cbOrderIdx = static_cast<size_t>(checkboxOrder);
                                                if (ui.StepSlider(cbOrderIdx, contentOrderNames, {.label = "Content Order", .showTicks = true, .showLabels = true, .disabled = !checkHasExternal}).changed) {
                                                    checkboxOrder = static_cast<guikit::ContentOrder>(cbOrderIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t cbAlignIdx = static_cast<size_t>(checkboxLabelAlign);
                                                if (ui.StepSlider(cbAlignIdx, labelAlignNames, {.label = "Label Alignment", .showTicks = true, .showLabels = true, .disabled = !checkHasExternal}).changed) {
                                                    checkboxLabelAlign = static_cast<guikit::Align>(cbAlignIdx);
                                                    ui.InvalidateAll();
                                                }

                                                if (ui.Switch(testCheckboxIndet, {.label = "Indeterminate Tri-State"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(checkboxDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemTabBar: {
                                                static constexpr std::array<std::string_view, 2> styleNames = {"Underline", "Pill"};
                                                size_t styleIdx = (tabStyle == guikit::TabBarStyle::Pill) ? 1 : 0;
                                                if (ui.StepSlider(styleIdx, styleNames, {.label = "TabBar Style", .showTicks = true, .showLabels = true}).changed) {
                                                    tabStyle = (styleIdx == 1) ? guikit::TabBarStyle::Pill : guikit::TabBarStyle::Underline;
                                                    ui.InvalidateAll();
                                                }
                                                if (ui.Switch(testTabHasBg, {.label = "Container Background Fill"}).changed) ui.InvalidateAll();
                                                if (ui.Slider(testTabRadius, 0.0f, 24.0f, {.label = "Container Corner Radius", .format = "%.0f px", .disabled = !testTabHasBg}).changed) ui.InvalidateAll();
                                                if (ui.Slider(testTabPadding, 0.0f, 12.0f, {.label = "Container Internal Padding", .format = "%.0f px", .disabled = !testTabHasBg}).changed) ui.InvalidateAll();
                                                if (ui.Slider(testTabBorderWidth, 0.0f, 4.0f, {.label = "Container Border Width", .format = "%.1f px", .disabled = !testTabHasBg}).changed) ui.InvalidateAll();
                                                if (ui.Slider(tabGap, 0.0f, 24.0f, {.label = "Tab Spacing (Gap)", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(testTabShowSep, {.label = "Baseline Separator"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(testTabFullWidth, {.label = "Full Width Stretch"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(testTabScrollable, {.label = "Scrollable Ribbon"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemContainers: {
                                                if (ui.Slider(cardRadius, 0.0f, 24.0f, {.label = "Corner Radius", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Slider(cardPadding, 0.0f, 32.0f, {.label = "Internal Padding", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Slider(cardGap, 0.0f, 24.0f, {.label = "Content Spacing (Gap)", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Slider(cardBorderWidth, 0.0f, 4.0f, {.label = "Border Width", .format = "%.1f px"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemTypography: {
                                                if (ui.StepSlider(headingSize, {.label = "Heading Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
                                                if (ui.Switch(headingDivider, {.label = "Show Divider Line"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(headingWordWrap, {.label = "Subtitle Word Wrap"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            default: break;
                                        }
                                    } else {
                                        // Assembly Inspector (Scene & Environment)
                                        if (ui.Switch(darkMode, {
                                            .label = "Preview Dark Mode",
                                            .description = "Switch preview between Dark and Light palette"
                                        }).changed) {
                                            lightMode = !darkMode;
                                            const auto defTheme = darkMode ? guikit::UiTheme::Dark(previewTheme.density) : guikit::UiTheme::Light(previewTheme.density);
                                            previewTheme.palette = defTheme.palette;
                                            previewTheme.variants = defTheme.variants;
                                            previewTheme.mode = darkMode ? guikit::ThemeMode::Dark : guikit::ThemeMode::Light;
                                            ui.InvalidateAll();
                                        }

                                        static constexpr std::array<std::string_view, 3> asmDensityLabels = {"Compact", "Standard", "Touch"};
                                        size_t dIdx = (previewTheme.density == guikit::Density::Compact) ? 0 : ((previewTheme.density == guikit::Density::Touch) ? 2 : 1);
                                        if (ui.StepSlider(dIdx, asmDensityLabels, {.label = "Layout Density", .showTicks = true, .showLabels = true}).changed) {
                                            previewTheme.density = (dIdx == 0) ? guikit::Density::Compact : ((dIdx == 2) ? guikit::Density::Touch : guikit::Density::Standard);
                                            previewTheme.SetDensity(previewTheme.density);
                                            ui.InvalidateAll();
                                        }

                                        if (ui.Slider(gridColGap, 0.0f, 32.0f, {.label = "Horizontal Gap (Columns)", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                        if (ui.Slider(gridRowGap, 0.0f, 32.0f, {.label = "Vertical Gap (Rows)", .format = "%.0f px"}).changed) ui.InvalidateAll();

                                        if (ui.Slider(cardRadius, 0.0f, 24.0f, {.label = "Card Corner Radius", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                        if (ui.Slider(cardPadding, 4.0f, 32.0f, {.label = "Card Padding", .format = "%.0f px"}).changed) ui.InvalidateAll();
                                        if (ui.Slider(cardBorderWidth, 0.0f, 4.0f, {.label = "Card Border Width", .format = "%.1f px"}).changed) ui.InvalidateAll();

                                        if (selectedCategory == NavCategory::AsmDashboard) {
                                            if (ui.Switch(hwEstop, {.label = "Hardware E-Stop Line", .variant = guikit::UiVariant::Danger}).changed) ui.InvalidateAll();
                                            if (ui.Switch(enableSensors, {.label = "Sensor Drop Limits"}).changed) ui.InvalidateAll();
                                            if (ui.Switch(diagLogging, {.label = "High-Speed Flash Logging"}).changed) ui.InvalidateAll();
                                        } else if (selectedCategory == NavCategory::AsmFormControls) {
                                            if (ui.Switch(formMode, {.label = "Stacked Labels Mode"}).changed) ui.InvalidateAll();
                                            if (ui.Switch(showTelemetry, {.label = "Show Telemetry Banner"}).changed) ui.InvalidateAll();
                                        } else if (selectedCategory == NavCategory::AsmFlexRibbon) {
                                            if (ui.Slider(carouselGap, 0.0f, 24.0f, {.label = "Ribbon Items Gap", .format = "%.0f px"}).changed) ui.InvalidateAll();
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
                        .backgroundColor = shellTheme.palette.background,
                        .borderColor = shellTheme.palette.borderSubtle,
                        .borderWidth = 1.0f,
                        .radius = 8.0f,
                        .clipChildren = true,
                        .theme = &shellTheme,
                        .scale = scaleValues[scaleIndex]
                    })) {
                        if (auto contentCol = ui.Column({
                            .gap = colGap,
                            .widthMode = guikit::SizeMode::Stretch,
                            .heightMode = guikit::SizeMode::Stretch
                        })) {
                            auto& th = previewTheme;
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
                                // ELEMENTS MODE: Component Sandbox (Hero 50% + C++ Code 50%)
                                // -------------------------------------------------------------
                                if (auto heroCard = ui.Card({
                                    .gap = 12.0f,
                                    .padding = guikit::Padding::All(14.0f),
                                    .radius = 8.0f,
                                    .heightMode = guikit::SizeMode::Stretch,
                                    .theme = &previewTheme
                                })) {
                                    // Hero Showcase Stage
                                    if (auto stageBox = ui.Box({
                                        .widthMode = guikit::SizeMode::Stretch,
                                        .heightMode = guikit::SizeMode::Stretch,
                                        .padding = guikit::Padding::All(24.0f),
                                        .backgroundColor = th.palette.surfaceElevated,
                                        .borderColor = th.palette.borderSubtle,
                                        .borderWidth = 1.0f,
                                        .radius = 8.0f,
                                        .theme = &previewTheme
                                    })) {
                                        if (auto centerRow = ui.Row({
                                            .align = guikit::Alignment::Center,
                                            .justify = guikit::Justify::Center,
                                            .heightMode = guikit::SizeMode::Stretch,
                                            .theme = &previewTheme
                                        })) {
                                            switch (selectedCategory) {
                                                case NavCategory::ElemButton: {
                                                    std::string btnText = btnClicks > 0 ? std::format("Clicked {}x", btnClicks) : "Interactive Button";
                                                    if (ui.Button({
                                                        .label = btnShowLabel ? "System Action" : "",
                                                        .text = btnText,
                                                        .description = btnShowDesc ? "Commit payload and trigger immediate canary rollout" : "",
                                                        .icon = btnHasIcon ? guikit::Icon::Play() : guikit::Icon{},
                                                        .variant = btnVariant,
                                                        .iconPlacement = btnIconPlacement,
                                                        .layout = btnLayout,
                                                        .order = btnOrder,
                                                        .labelAlign = btnLabelAlign,
                                                        .align = btnAlign,
                                                        .stretch = static_cast<guikit::StretchMode>(btnStretchIdx),
                                                        .width = (btnWidth > 0.0f) ? std::optional<float>(btnWidth) : std::nullopt,
                                                        .disabled = btnDisabled,
                                                        .size = controlsSize,
                                                        .radius = btnCornerRadius
                                                    })) {
                                                        ++btnClicks;
                                                        ui.InvalidateAll();
                                                    }
                                                    break;
                                                }

                                                case NavCategory::ElemSlider: {
                                                    if (sliderContinuous) {
                                                        ui.Slider(testSliderVal, 0.0f, 100.0f, {
                                                            .label = sliderShowLabel ? "Analog Master Gain" : "",
                                                            .description = sliderShowDesc ? "Adjust internal preamp attenuation" : "",
                                                            .format = "%.1f %%",
                                                            .layout = sliderLayout,
                                                            .order = sliderOrder,
                                                            .labelAlign = sliderLabelAlign,
                                                            .showValue = sliderShowValue,
                                                            .variant = btnVariant,
                                                            .size = controlsSize,
                                                            .stretch = static_cast<guikit::StretchMode>(sliderStretchIdx),
                                                            .width = (sliderWidth > 0.0f) ? std::optional<float>(sliderWidth) : std::nullopt,
                                                            .disabled = sliderDisabled
                                                        });
                                                    } else {
                                                        static constexpr std::array<std::string_view, 5> stepItems = {"Off", "Low", "Mid", "High", "Max"};
                                                        ui.StepSlider(testStepIdx, stepItems, {
                                                            .label = sliderShowLabel ? "DSP Filter Mode" : "",
                                                            .description = sliderShowDesc ? "Select active FIR profile" : "",
                                                            .layout = sliderLayout,
                                                            .order = sliderOrder,
                                                            .labelAlign = sliderLabelAlign,
                                                            .showTicks = sliderShowTicks,
                                                            .showLabels = sliderShowLabels,
                                                            .showValue = sliderShowValue,
                                                            .variant = btnVariant,
                                                            .size = controlsSize,
                                                            .stretch = static_cast<guikit::StretchMode>(sliderStretchIdx),
                                                            .width = (sliderWidth > 0.0f) ? std::optional<float>(sliderWidth) : std::nullopt,
                                                            .disabled = sliderDisabled
                                                        });
                                                    }
                                                    break;
                                                }

                                                case NavCategory::ElemInputField: {
                                                    ui.InputField(testInputText, {
                                                        .label = inputShowLabel ? "Server Endpoint" : "",
                                                        .description = inputShowDesc ? "Configured host address" : "",
                                                        .placeholder = "Enter domain...",
                                                        .prefix = inputHasPrefix ? "https://" : "",
                                                        .suffix = inputHasSuffix ? ":443" : "",
                                                        .variant = btnVariant,
                                                        .layout = inputLayout,
                                                        .order = inputOrder,
                                                        .labelAlign = inputLabelAlign,
                                                        .cursorMode = inputCursorMode,
                                                        .size = controlsSize,
                                                        .radius = inputCornerRadius,
                                                        .stretch = static_cast<guikit::StretchMode>(inputStretchIdx),
                                                        .width = (inputWidth > 0.0f) ? std::optional<float>(inputWidth) : std::nullopt,
                                                        .disabled = inputDisabled,
                                                        .readOnly = inputReadOnly
                                                    });
                                                    break;
                                                }

                                                case NavCategory::ElemSelect: {
                                                    ui.Select(testSelectIdx, selectItems, {
                                                        .label = selectShowLabel ? "Active Preset" : "",
                                                        .description = selectShowDesc ? "Dropdown selection" : "",
                                                        .variant = btnVariant,
                                                        .layout = selectLayout,
                                                        .order = selectOrder,
                                                        .labelAlign = selectLabelAlign,
                                                        .prefixIcon = selectHasPrefix ? guikit::Icon::Settings() : guikit::Icon{},
                                                        .maxVisibleItems = selectMaxItems,
                                                        .size = controlsSize,
                                                        .radius = selectCornerRadius,
                                                        .stretch = static_cast<guikit::StretchMode>(selectStretchIdx),
                                                        .width = (selectWidth > 0.0f) ? std::optional<float>(selectWidth) : std::nullopt,
                                                        .disabled = selectDisabled
                                                    });
                                                    break;
                                                }

                                                case NavCategory::ElemSwitch: {
                                                    if (auto swRow = ui.Row({.gap = 20.0f, .align = guikit::Alignment::Center})) {
                                                        ui.Switch(testSwitchState, {
                                                            .label = switchShowLabel ? (testDarkLightSwitch ? "Dark" : "Hardware E-Stop") : "",
                                                            .trailingLabel = (switchShowLabel && testDarkLightSwitch) ? "Light" : "",
                                                            .description = switchShowDesc ? "Safety interlocking system" : "",
                                                            .variant = btnVariant,
                                                            .layout = switchLayout,
                                                            .order = switchOrder,
                                                            .labelAlign = switchLabelAlign,
                                                            .symmetric = switchSymmetric,
                                                            .stretch = static_cast<guikit::StretchMode>(switchStretchIdx),
                                                            .disabled = switchDisabled,
                                                            .size = controlsSize
                                                        });
                                                    }
                                                    break;
                                                }

                                                case NavCategory::ElemCheckbox: {
                                                    if (auto chRow = ui.Row({.gap = 20.0f, .align = guikit::Alignment::Center})) {
                                                        ui.Checkbox(testCheckboxState, {
                                                            .label = checkboxShowLabel ? "Telemetry Logging" : "",
                                                            .description = checkboxShowDesc ? "Save trace to buffer" : "",
                                                            .variant = btnVariant,
                                                            .layout = checkboxLayout,
                                                            .order = checkboxOrder,
                                                            .labelAlign = checkboxLabelAlign,
                                                            .stretch = static_cast<guikit::StretchMode>(checkboxStretchIdx),
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

                                // C++20 Immediate Mode Code Generator Card
                                if (auto codeCard = ui.Card({
                                    .gap = 8.0f,
                                    .padding = guikit::Padding::All(12.0f),
                                    .radius = 8.0f,
                                    .heightMode = guikit::SizeMode::Stretch,
                                    .theme = &shellTheme
                                })) {
                                    auto placementName = [](guikit::Placement p) -> std::string_view {
                                        switch (p) {
                                            case guikit::Placement::Leading:  return "Leading";
                                            case guikit::Placement::Trailing: return "Trailing";
                                            case guikit::Placement::Top:      return "Top";
                                            case guikit::Placement::Bottom:   return "Bottom";
                                        }
                                        return "Trailing";
                                    };

                                    auto sizeName = [](guikit::UiSize s) -> std::string_view {
                                        switch (s) {
                                            case guikit::UiSize::Xs: return "Xs";
                                            case guikit::UiSize::Sm: return "Sm";
                                            case guikit::UiSize::Md: return "Md";
                                            case guikit::UiSize::Lg: return "Lg";
                                            case guikit::UiSize::Xl: return "Xl";
                                            default: return "Md";
                                        }
                                    };

                                    auto alignName = [](guikit::Align a) -> std::string_view {
                                        switch (a) {
                                            case guikit::Align::Start:  return "Start";
                                            case guikit::Align::Center: return "Center";
                                            case guikit::Align::End:    return "End";
                                        }
                                        return "Center";
                                    };

                                    auto cursorModeName = [](guikit::CursorBlinkMode m) -> std::string_view {
                                        switch (m) {
                                            case guikit::CursorBlinkMode::Blinking: return "Blinking";
                                            case guikit::CursorBlinkMode::Solid:    return "Solid";
                                            case guikit::CursorBlinkMode::Hidden:   return "Hidden";
                                        }
                                        return "Blinking";
                                    };

                                    std::string snippet;
                                     switch (selectedCategory) {
                                        case NavCategory::ElemButton: {
                                            snippet = std::format(
                                                "if (ui.Button({{\n"
                                                "    .label = \"{}\",\n"
                                                "    .text = \"{}\",\n"
                                                "    .description = \"{}\",\n"
                                                "    .icon = {},\n"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .iconPlacement = guikit::Placement::{},\n"
                                                "    .layout = guikit::WidgetLayout::{},\n"
                                                "    .order = guikit::ContentOrder::{},\n"
                                                "    .labelAlign = guikit::Align::{},\n"
                                                "    .align = guikit::Align::{},\n"
                                                "    .labelVariants = {{}},\n"
                                                "    .stretch = guikit::StretchMode::{},\n"
                                                "    .width = {},\n"
                                                "    .disabled = {},\n"
                                                "    .active = false,\n"
                                                "    .size = guikit::UiSize::{},\n"
                                                "    .radius = {:.1f}f\n"
                                                "}})) {{\n"
                                                "    // On click handler\n"
                                                "}}",
                                                btnShowLabel ? "System Action" : "",
                                                btnClicks > 0 ? std::format("Clicked {}x", btnClicks) : "Interactive Button",
                                                btnShowDesc ? "Commit payload and trigger immediate canary rollout" : "",
                                                btnHasIcon ? "guikit::Icon::Play()" : "{}",
                                                variantName(btnVariant),
                                                placementName(btnIconPlacement),
                                                widgetLayoutEnumName(btnLayout),
                                                contentOrderEnumName(btnOrder),
                                                labelAlignEnumName(btnLabelAlign),
                                                alignName(btnAlign),
                                                stretchModeEnumName(static_cast<guikit::StretchMode>(btnStretchIdx)),
                                                btnWidth > 0.0f ? std::format("{:.1f}f", btnWidth) : "std::nullopt",
                                                btnDisabled ? "true" : "false",
                                                sizeName(controlsSize),
                                                btnCornerRadius
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemSlider: {
                                            if (sliderContinuous) {
                                                snippet = std::format(
                                                    "ui.Slider(gainValue, 0.0f, 100.0f, {{\n"
                                                    "    .label = \"{}\",\n"
                                                    "    .description = \"{}\",\n"
                                                    "    .suffix = \"\",\n"
                                                    "    .format = \"%.1f %%\",\n"
                                                    "    .step = std::nullopt,\n"
                                                    "    .layout = guikit::WidgetLayout::{},\n"
                                                    "    .order = guikit::ContentOrder::{},\n"
                                                    "    .showValue = {},\n"
                                                    "    .variant = guikit::UiVariant::{},\n"
                                                    "    .size = guikit::UiSize::{},\n"
                                                    "    .stretch = guikit::StretchMode::{},\n"
                                                    "    .width = {},\n"
                                                    "    .scale = std::nullopt,\n"
                                                    "    .theme = nullptr,\n"
                                                    "    .disabled = {}\n"
                                                    "}});",
                                                    sliderShowLabel ? "Analog Master Gain" : "",
                                                    sliderShowDesc ? "Adjust internal preamp attenuation" : "",
                                                    widgetLayoutEnumName(sliderLayout),
                                                    contentOrderEnumName(sliderOrder),
                                                    sliderShowValue ? "true" : "false",
                                                    variantName(btnVariant),
                                                    sizeName(controlsSize),
                                                    stretchModeEnumName(static_cast<guikit::StretchMode>(sliderStretchIdx)),
                                                    sliderWidth > 0.0f ? std::format("{:.1f}f", sliderWidth) : "std::nullopt",
                                                    sliderDisabled ? "true" : "false"
                                                );
                                            } else {
                                                snippet = std::format(
                                                    "static constexpr std::array<std::string_view, 5> stepItems = {{\"Off\", \"Low\", \"Mid\", \"High\", \"Max\"}};\n"
                                                    "ui.StepSlider(presetIndex, stepItems, {{\n"
                                                    "    .label = \"{}\",\n"
                                                    "    .description = \"{}\",\n"
                                                    "    .stepLabels = {{}},\n"
                                                    "    .layout = guikit::WidgetLayout::{},\n"
                                                    "    .order = guikit::ContentOrder::{},\n"
                                                    "    .showTicks = {},\n"
                                                    "    .showLabels = {},\n"
                                                    "    .showValue = {},\n"
                                                    "    .valueText = \"\",\n"
                                                    "    .includeNone = false,\n"
                                                    "    .variant = guikit::UiVariant::{},\n"
                                                    "    .size = guikit::UiSize::{},\n"
                                                    "    .stretch = guikit::StretchMode::{},\n"
                                                    "    .width = {},\n"
                                                    "    .scale = std::nullopt,\n"
                                                    "    .theme = nullptr,\n"
                                                    "    .disabled = {}\n"
                                                    "}});",
                                                    sliderShowLabel ? "DSP Filter Mode" : "",
                                                    sliderShowDesc ? "Select active FIR profile" : "",
                                                    widgetLayoutEnumName(sliderLayout),
                                                    contentOrderEnumName(sliderOrder),
                                                    sliderShowTicks ? "true" : "false",
                                                    sliderShowLabels ? "true" : "false",
                                                    sliderShowValue ? "true" : "false",
                                                    variantName(btnVariant),
                                                    sizeName(controlsSize),
                                                    stretchModeEnumName(static_cast<guikit::StretchMode>(sliderStretchIdx)),
                                                    sliderWidth > 0.0f ? std::format("{:.1f}f", sliderWidth) : "std::nullopt",
                                                    sliderDisabled ? "true" : "false"
                                                );
                                            }
                                            break;
                                        }

                                        case NavCategory::ElemInputField: {
                                            snippet = std::format(
                                                "ui.InputField(serverEndpoint, {{\n"
                                                "    .label = \"{}\",\n"
                                                "    .description = \"{}\",\n"
                                                "    .placeholder = \"Enter domain...\",\n"
                                                "    .prefix = \"{}\",\n"
                                                "    .suffix = \"{}\",\n"
                                                "    .format = \"%.2f\",\n"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .layout = guikit::WidgetLayout::{},\n"
                                                "    .order = guikit::ContentOrder::{},\n"
                                                "    .prefixIcon = {},\n"
                                                "    .suffixIcon = {},\n"
                                                "    .cursorMode = guikit::CursorBlinkMode::{},\n"
                                                "    .size = guikit::UiSize::{},\n"
                                                "    .radius = {:.1f}f,\n"
                                                "    .stretch = guikit::StretchMode::{},\n"
                                                "    .width = {},\n"
                                                "    .disabled = {},\n"
                                                "    .readOnly = {}\n"
                                                "}});",
                                                inputShowLabel ? "Server Endpoint" : "",
                                                inputShowDesc ? "Configured host address" : "",
                                                inputHasPrefix ? "https://" : "",
                                                inputHasSuffix ? ":443" : "",
                                                variantName(btnVariant),
                                                widgetLayoutEnumName(inputLayout),
                                                contentOrderEnumName(inputOrder),
                                                "{}",
                                                "{}",
                                                cursorModeName(inputCursorMode),
                                                sizeName(controlsSize),
                                                inputCornerRadius,
                                                stretchModeEnumName(static_cast<guikit::StretchMode>(inputStretchIdx)),
                                                inputWidth > 0.0f ? std::format("{:.1f}f", inputWidth) : "std::nullopt",
                                                inputDisabled ? "true" : "false",
                                                inputReadOnly ? "true" : "false"
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemSelect: {
                                            snippet = std::format(
                                                "ui.Select(presetIndex, itemsList, {{\n"
                                                "    .label = \"{}\",\n"
                                                "    .description = \"{}\",\n"
                                                "    .placeholder = \"Select...\",\n"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .layout = guikit::WidgetLayout::{},\n"
                                                "    .order = guikit::ContentOrder::{},\n"
                                                "    .prefixIcon = {},\n"
                                                "    .maxVisibleItems = {},\n"
                                                "    .size = guikit::UiSize::{},\n"
                                                "    .radius = {:.1f}f,\n"
                                                "    .stretch = guikit::StretchMode::{},\n"
                                                "    .width = {},\n"
                                                "    .disabled = {}\n"
                                                "}});",
                                                selectShowLabel ? "Active Preset" : "",
                                                selectShowDesc ? "Dropdown selection" : "",
                                                variantName(btnVariant),
                                                widgetLayoutEnumName(selectLayout),
                                                contentOrderEnumName(selectOrder),
                                                selectHasPrefix ? "guikit::Icon::Settings()" : "{}",
                                                selectMaxItems,
                                                sizeName(controlsSize),
                                                selectCornerRadius,
                                                stretchModeEnumName(static_cast<guikit::StretchMode>(selectStretchIdx)),
                                                selectWidth > 0.0f ? std::format("{:.1f}f", selectWidth) : "std::nullopt",
                                                selectDisabled ? "true" : "false"
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemSwitch: {
                                            snippet = std::format(
                                                "ui.Switch(switchState, {{\n"
                                                "    .label = \"{}\",\n"
                                                "    .trailingLabel = \"{}\",\n"
                                                "    .description = \"{}\",\n"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .layout = guikit::WidgetLayout::{},\n"
                                                "    .order = guikit::ContentOrder::{},\n"
                                                "    .stretch = guikit::StretchMode::{},\n"
                                                "    .symmetric = {},\n"
                                                "    .disabled = {},\n"
                                                "    .size = guikit::UiSize::{}\n"
                                                "}});",
                                                switchShowLabel ? (testDarkLightSwitch ? "Dark" : "Hardware E-Stop") : "",
                                                (switchShowLabel && testDarkLightSwitch) ? "Light" : "",
                                                switchShowDesc ? "Safety interlocking system" : "",
                                                variantName(btnVariant),
                                                widgetLayoutEnumName(switchLayout),
                                                contentOrderEnumName(switchOrder),
                                                stretchModeEnumName(static_cast<guikit::StretchMode>(switchStretchIdx)),
                                                switchSymmetric ? "true" : "false",
                                                switchDisabled ? "true" : "false",
                                                sizeName(controlsSize)
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemCheckbox: {
                                            snippet = std::format(
                                                "ui.Checkbox(checkState, {{\n"
                                                "    .label = \"{}\",\n"
                                                "    .description = \"{}\",\n"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .layout = guikit::WidgetLayout::{},\n"
                                                "    .order = guikit::ContentOrder::{},\n"
                                                "    .stretch = guikit::StretchMode::{},\n"
                                                "    .disabled = {},\n"
                                                "    .indeterminate = {},\n"
                                                "    .size = guikit::UiSize::{}\n"
                                                "}});",
                                                checkboxShowLabel ? "Telemetry Logging" : "",
                                                checkboxShowDesc ? "Save trace to buffer" : "",
                                                variantName(btnVariant),
                                                widgetLayoutEnumName(checkboxLayout),
                                                contentOrderEnumName(checkboxOrder),
                                                stretchModeEnumName(static_cast<guikit::StretchMode>(checkboxStretchIdx)),
                                                checkboxDisabled ? "true" : "false",
                                                testCheckboxIndet ? "true" : "false",
                                                sizeName(controlsSize)
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
                                                "    .backgroundColor = {},\n"
                                                "    .borderColor = {},\n"
                                                "    .borderWidth = {:.1f}f,\n"
                                                "    .activeColor = guikit::Color::Transparent(),\n"
                                                "    .size = guikit::UiSize::{},\n"
                                                "    .theme = nullptr,\n"
                                                "    .scale = std::nullopt\n"
                                                "}});",
                                                tabStyle == guikit::TabBarStyle::Pill ? "Pill" : "Underline",
                                                testTabShowSep ? "true" : "false",
                                                testTabFullWidth ? "true" : "false",
                                                testTabScrollable ? "true" : "false",
                                                tabGap,
                                                testTabHasBg ? testTabPadding : 0.0f,
                                                testTabHasBg ? testTabRadius : 0.0f,
                                                testTabHasBg ? "theme.palette.surfaceElevated" : "guikit::Color::Transparent()",
                                                testTabHasBg ? "theme.palette.borderSubtle" : "guikit::Color::Transparent()",
                                                testTabHasBg ? testTabBorderWidth : 0.0f,
                                                sizeName(controlsSize)
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemContainers: {
                                            snippet = std::format(
                                                "if (auto card = ui.Card({{\n"
                                                "    .variant = guikit::UiVariant::Default,\n"
                                                "    .minWidth = 0.0f,\n"
                                                "    .gap = {:.1f}f,\n"
                                                "    .padding = guikit::Padding({:.1f}f),\n"
                                                "    .backgroundColor = guikit::Color::Transparent(),\n"
                                                "    .borderColor = guikit::Color::Transparent(),\n"
                                                "    .borderWidth = {:.1f}f,\n"
                                                "    .radius = {:.1f}f,\n"
                                                "    .clipChildren = true,\n"
                                                "    .widthMode = guikit::SizeMode::Stretch,\n"
                                                "    .heightMode = guikit::SizeMode::Fit,\n"
                                                "    .size = guikit::UiSize::{},\n"
                                                "    .disabled = false,\n"
                                                "    .theme = nullptr,\n"
                                                "    .scale = std::nullopt\n"
                                                "}})) {{\n"
                                                "    ui.Text(\"Card Surface Component\");\n"
                                                "}}",
                                                cardGap,
                                                cardPadding,
                                                cardBorderWidth,
                                                cardRadius,
                                                sizeName(controlsSize)
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemTypography: {
                                            snippet = std::format(
                                                "ui.Heading({{\n"
                                                "    .title = \"Typographic Heading Sample\",\n"
                                                "    .subtitle = \"Dynamic typography scale test for multi-line layout hierarchy\",\n"
                                                "    .icon = guikit::Icon::Edit(),\n"
                                                "    .badge = \"\",\n"
                                                "    .variant = guikit::UiVariant::Default,\n"
                                                "    .size = guikit::UiSize::{},\n"
                                                "    .divider = {},\n"
                                                "    .dividerStyle = guikit::HeadingDivider::Bottom,\n"
                                                "    .wordWrap = {}\n"
                                                "}});",
                                                sizeName(headingSize),
                                                headingDivider ? "true" : "false",
                                                headingWordWrap ? "true" : "false"
                                            );
                                            break;
                                        }

                                        default: break;
                                    }

                                    ui.TextArea(snippet, {
                                        .autoHeight = false,
                                        .readOnly = true,
                                        .showLineNumbers = true,
                                        .monospace = true,
                                        .syntaxHighlighter = &guikit::TextArea::CppHighlighter,
                                        .heightMode = guikit::SizeMode::Stretch
                                    });
                                }
                            } else {
                                // -------------------------------------------------------------
                                // ASSEMBLY MODE: Real-World Screens & Compositions
                                // -------------------------------------------------------------
                                if (auto contentScroll = ui.ScrollArea({.gap = 12.0f, .theme = &previewTheme})) {
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
                                                                .layout = guikit::WidgetLayout::Stacked
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
                                                    .layout = guikit::WidgetLayout::Stacked
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

                                            const guikit::WidgetLayout activeLayout = formMode ? guikit::WidgetLayout::Stacked : guikit::WidgetLayout::Top;

                                            if (auto fieldsGrid = ui.Grid(makeGrid())) {
                                                if (auto c1 = fieldsGrid.Cell3()) {
                                                    ui.InputField(speedDrop, {
                                                        .label = "Speed Drop dF/dt",
                                                        .description = "Automatic stop of testing on load drop",
                                                        .suffix = "%",
                                                        .format = "%.0f",
                                                        .layout = activeLayout
                                                    });
                                                }
                                                if (auto c2 = fieldsGrid.Cell3()) {
                                                    ui.InputField(sampleId, {
                                                        .label = "Sample Identifier",
                                                        .description = "Unique test specimen code",
                                                        .placeholder = "Enter sample code...",
                                                        .layout = activeLayout
                                                    });
                                                }
                                                if (auto c3 = fieldsGrid.Cell3()) {
                                                    ui.Select(baudIndex, {"9600 bps", "19200 bps", "38400 bps", "115200 bps"}, {
                                                        .label = "Baud Rate",
                                                        .description = "Serial communication bus rate",
                                                        .layout = activeLayout
                                                    });
                                                }
                                                if (auto c4 = fieldsGrid.Cell3()) {
                                                    ui.Select(profileMode, {"Eco Saver", "Standard Performance", "Industrial High-Speed", "Extreme Calibration"}, {
                                                        .label = "Operation Profile",
                                                        .description = "Active runtime sensor & compute profile",
                                                        .layout = activeLayout
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
                                            ui.Heading({.title = "Dynamic 3-Way Equal Stretch Layout", .subtitle = "Each button automatically receives exactly 33.3% available width via stretch = StretchMode::Fill.", .size = headingSize, .wordWrap = headingWordWrap});

                                            if (auto stretchRow = ui.Row({.gap = rowGap})) {
                                                ui.Button({.label = "Col 1 (Stretch)", .stretch = guikit::StretchMode::Fill});
                                                ui.Button({.label = "Col 2 (Stretch)", .stretch = guikit::StretchMode::Fill});
                                                ui.Button({.label = "Col 3 (Stretch)", .stretch = guikit::StretchMode::Fill});
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
