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
    bool shellLightMode = false;
    bool previewDarkMode = true;
    bool previewLightMode = false;
    bool previewBounds = false;
    bool previewInspect = false;
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
    size_t shellScaleIndex = 0;   // Default: 100%
    size_t previewScaleIndex = 0; // Default: 100%
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
        ElemSidebar,
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
    guikit::Align btnControlAlign = guikit::Align::Center;
    size_t btnStretchIdx = 0; // Compact
    float btnWidth = 0.0f;    // 0 = auto (px in Compact/Space)
    float btnFillPct = 50.0f; // 50% default in Fill mode
    bool btnDisabled = false;
    int btnClicks = 0;
    float btnCornerRadius = 6.0f;
    bool btnShowLabel = false;
    bool btnShowDesc = false;
    guikit::UiVariant btnDescVariant = guikit::UiVariant::Muted;
    guikit::ElementLayout btnLayout = guikit::ElementLayout::Stacked;
    guikit::InlineOrder btnInlineOrder = guikit::InlineOrder::LabelFirst;
    guikit::Align btnElementAlign = guikit::Align::Start;

    float testSliderVal = 65.0f;
    size_t testStepIdx = 2;
    bool sliderContinuous = true;
    size_t sliderStretchIdx = 2; // Fill
    float sliderWidth = 0.0f;
    float sliderFillPct = 50.0f;
    bool sliderShowLabel = true;
    bool sliderShowDesc = false;
    guikit::UiVariant sliderDescVariant = guikit::UiVariant::Muted;
    bool sliderShowTicks = true;
    bool sliderShowLabels = true;
    bool sliderShowValue = true;
    guikit::Placement sliderValuePlacement = guikit::Placement::Trailing;
    bool sliderShowRangeText = false;
    bool sliderDisabled = false;
    guikit::ElementLayout sliderLayout = guikit::ElementLayout::Stacked;
    guikit::InlineOrder sliderInlineOrder = guikit::InlineOrder::LabelFirst;
    guikit::Align sliderElementAlign = guikit::Align::Start;

    std::string testInputText = "guikit.dev";
    guikit::CursorBlinkMode inputCursorMode = guikit::CursorBlinkMode::Blinking;
    bool inputPassword = false;
    bool inputClearable = true;
    bool inputReadOnly = false;
    bool inputDisabled = false;
    bool inputHasPrefix = true;
    bool inputHasSuffix = false;
    float inputCornerRadius = 6.0f;
    bool inputShowLabel = true;
    bool inputShowDesc = false;
    guikit::UiVariant inputDescVariant = guikit::UiVariant::Muted;
    guikit::ElementLayout inputLayout = guikit::ElementLayout::Stacked;
    guikit::InlineOrder inputInlineOrder = guikit::InlineOrder::LabelFirst;
    guikit::Align inputElementAlign = guikit::Align::Start;
    size_t inputStretchIdx = 2; // Fill
    float inputWidth = 0.0f;
    float inputFillPct = 50.0f;

    size_t testSelectIdx = 0;
    static const std::vector<std::string_view> selectItems = {"Option Alpha", "Option Beta", "Option Gamma", "Option Delta", "Option Epsilon"};
    bool selectDisabled = false;
    size_t selectStretchIdx = 2; // Fill
    float selectWidth = 0.0f;
    float selectFillPct = 50.0f;
    uint32_t selectMaxItems = 4;
    float selectCornerRadius = 6.0f;
    bool selectShowLabel = true;
    bool selectShowDesc = true;
    guikit::UiVariant selectDescVariant = guikit::UiVariant::Muted;
    guikit::ElementLayout selectLayout = guikit::ElementLayout::Stacked;
    guikit::InlineOrder selectInlineOrder = guikit::InlineOrder::LabelFirst;
    guikit::Align selectElementAlign = guikit::Align::Start;
    bool selectHasPrefix = false;

    bool testSwitchState = true;
    bool testDarkLightSwitch = false;
    guikit::ElementLayout switchLayout = guikit::ElementLayout::Inline;
    guikit::InlineOrder switchInlineOrder = guikit::InlineOrder::ControlFirst;
    guikit::Align switchElementAlign = guikit::Align::Start;
    size_t switchStretchIdx = 0; // Compact
    bool switchSymmetric = false;
    bool switchShowLabel = true;
    bool switchShowDesc = true;
    guikit::UiVariant switchDescVariant = guikit::UiVariant::Muted;
    bool switchDisabled = false;

    bool testCheckboxState = true;
    bool testCheckboxIndet = false;
    guikit::ElementLayout checkboxLayout = guikit::ElementLayout::Inline;
    guikit::InlineOrder checkboxInlineOrder = guikit::InlineOrder::ControlFirst;
    guikit::Align checkboxElementAlign = guikit::Align::Start;
    size_t checkboxStretchIdx = 0; // Compact
    bool checkboxShowLabel = true;
    bool checkboxShowDesc = true;
    guikit::UiVariant checkboxDescVariant = guikit::UiVariant::Muted;
    bool checkboxDisabled = false;

    static constexpr std::array<std::string_view, 3> stretchModeNames = {"Compact", "Space", "Fill"};
    auto stretchModeEnumName = [](guikit::StretchMode m) -> std::string_view {
        switch (m) {
            case guikit::StretchMode::Compact: return "Compact";
            case guikit::StretchMode::Space: return "Space";
            default: return "Fill";
        }
    };

    static constexpr std::array<std::string_view, 4> elementLayoutNames = {"Stacked", "Top", "Bottom", "Inline"};
    static constexpr std::array<std::string_view, 2> inlineOrderNames = {"LabelFirst", "ControlFirst"};
    static constexpr std::array<std::string_view, 3> labelAlignNames = {"Start (Left)", "Center", "End (Right)"};

    auto elementLayoutEnumName = [](guikit::ElementLayout l) -> std::string_view {
        switch (l) {
            case guikit::ElementLayout::Stacked: return "Stacked";
            case guikit::ElementLayout::Top:     return "Top";
            case guikit::ElementLayout::Bottom:  return "Bottom";
            case guikit::ElementLayout::Inline:  return "Inline";
        }
        return "Stacked";
    };

    auto inlineOrderEnumName = [](guikit::InlineOrder o) -> std::string_view {
        switch (o) {
            case guikit::InlineOrder::LabelFirst:   return "LabelFirst";
            case guikit::InlineOrder::ControlFirst: return "ControlFirst";
        }
        return "LabelFirst";
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
            case guikit::UiVariant::Muted:     return "Muted";
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

    int testSidebarMenuIdx = 0;
    guikit::SidebarMenuStyle testSidebarMenuStyle = guikit::SidebarMenuStyle::Pill;
    guikit::SidebarSide testSidebarSide = guikit::SidebarSide::Left;
    bool testSidebarCollapsed = false;
    bool testSidebarShowBadges = true;
    bool testSidebarShowSections = true;
    bool testSidebarAutoWidth = true;
    float testSidebarFixedWidth = 200.0f;

    guikit::FrameMetrics metrics;

    guikit::UiTheme shellTheme = shellLightMode ? guikit::UiTheme::Light(guikit::Density::Standard) : guikit::UiTheme::Dark(guikit::Density::Standard);
    guikit::UiTheme previewTheme = previewDarkMode ? guikit::UiTheme::Dark(guikit::Density::Standard) : guikit::UiTheme::Light(guikit::Density::Standard);

    ui.SetTheme(shellTheme);
    ui.SetClearColor(shellTheme.palette.background);
    ui.SetIdleHeartbeatMs(heartbeatMsValues[heartbeatIndex]);
    ui.SetClipboardCallbacks(
        []() -> std::string {
            char* clip = SDL_GetClipboardText();
            if (!clip) return {};
            std::string res(clip);
            SDL_free(clip);
            return res;
        },
        [](std::string_view text) {
            std::string s(text);
            SDL_SetClipboardText(s.c_str());
        }
    );

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
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                const SDL_Keymod mod = SDL_GetModState();
                ui.SetKeyModifiers({
                    .ctrl  = (mod & SDL_KMOD_CTRL) != 0,
                    .shift = (mod & SDL_KMOD_SHIFT) != 0,
                    .alt   = (mod & SDL_KMOD_ALT) != 0
                });
                ui.OnMouseDown(event.button.button);
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_UP:
                ui.OnMouseUp(event.button.button);
                break;
            case SDL_EVENT_MOUSE_WHEEL:
                ui.OnMouseWheel(event.wheel.x, event.wheel.y);
                break;
            case SDL_EVENT_TEXT_INPUT: {
                const SDL_Keymod mod = SDL_GetModState();
                if ((mod & SDL_KMOD_CTRL) && !(mod & SDL_KMOD_ALT)) {
                    break;
                }
                ui.OnTextInput(event.text.text);
                break;
            }
            case SDL_EVENT_KEY_UP: {
                const bool ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
                const bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
                const bool alt = (event.key.mod & SDL_KMOD_ALT) != 0;
                ui.SetKeyModifiers({ctrl, shift, alt});
                break;
            }
            case SDL_EVENT_KEY_DOWN: {
                const bool ctrl = (event.key.mod & SDL_KMOD_CTRL) != 0;
                const bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
                const bool alt = (event.key.mod & SDL_KMOD_ALT) != 0;
                const guikit::KeyModifiers mods{ctrl, shift, alt};
                ui.SetKeyModifiers(mods);

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
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:  key = guikit::Key::Enter; break;
                    case SDLK_ESCAPE:    key = guikit::Key::Escape; break;
                    case SDLK_TAB:       key = guikit::Key::Tab; break;
                    case SDLK_SPACE:     key = guikit::Key::Space; break;
                    case SDLK_LEFT:      key = guikit::Key::Left; break;
                    case SDLK_RIGHT:     key = guikit::Key::Right; break;
                    case SDLK_UP:        key = guikit::Key::Up; break;
                    case SDLK_DOWN:      key = guikit::Key::Down; break;
                    case SDLK_HOME:      key = guikit::Key::Home; break;
                    case SDLK_END:       key = guikit::Key::End; break;
                    case SDLK_A:         if (ctrl) key = guikit::Key::A; break;
                    case SDLK_C:         if (ctrl) key = guikit::Key::C; break;
                    case SDLK_V:         if (ctrl) key = guikit::Key::V; break;
                    case SDLK_X:         if (ctrl) key = guikit::Key::X; break;
                    case SDLK_Z:         if (ctrl) key = guikit::Key::Z; break;
                    default: break;
                }
                if (key != guikit::Key::None) {
                    ui.OnKeyDown(key, mods);
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

        const float shellScale = scaleValues[shellScaleIndex];
        const float previewScale = scaleValues[previewScaleIndex];

        // --- Begin Immediate Mode GuiKit Frame ---
        ui.BeginFrame({static_cast<float>(winWidth), static_cast<float>(winHeight)}, 1.0f);
        {
            if (auto mainCol = ui.Column({.theme = &shellTheme, .scale = shellScale})) {
                // Engine HUD Toolbar:
                // Sidebar | Scale Dark/Light Bounds Inspect | VSync React Dirty Heartbeat | guikit
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

                    // 2. Shell Controls Group: Scale, Dark/Light, Bounds, Inspect (affects UI except Preview)
                    if (auto scaleBox = ui.Box({.fixedWidth = 92.0f})) {
                        if (ui.Select(shellScaleIndex, scaleLabels, {.size = guikit::UiSize::Sm}).changed) {
                            ui.InvalidateAll();
                        }
                    }

                    if (ui.Switch(shellLightMode, {
                        .textLeft = "Dark",
                        .textRight = "Light",
                        .symmetric = true,
                        .size = guikit::UiSize::Md
                    }).changed) {
                        shellTheme = shellLightMode ? guikit::UiTheme::Light(shellTheme.density) : guikit::UiTheme::Dark(shellTheme.density);
                        ui.SetTheme(shellTheme);
                        ui.SetClearColor(shellTheme.palette.background);
                        ui.InvalidateAll();
                    }

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
                        .label = {heartbeatLabels[heartbeatIndex], heartbeatLabels},
                        .icon = guikit::Icon::Refresh(),
                        .disabled = !reactiveMode
                    })) {
                        heartbeatIndex = (heartbeatIndex + 1) % heartbeatLabels.size();
                        ui.SetIdleHeartbeatMs(heartbeatMsValues[heartbeatIndex]);
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
                        // 1. Level 1: Element Catalog & Assembly Rail (Left, auto-fit width)
                        if (auto railSidebar = ui.Sidebar({
                            .side = guikit::SidebarSide::Left,
                            .minWidth = 152.0f
                        })) {
                            const NavCategory prevCat = selectedCategory;
                            if (auto menu = ui.SidebarMenu({.size = guikit::UiSize::Sm})) {
                                menu.Section("ELEMENTS");
                                menu.Item(selectedCategory, NavCategory::ElemButton,     {.text = "Button",     .icon = guikit::Icon::Play()});
                                menu.Item(selectedCategory, NavCategory::ElemSlider,     {.text = "Slider",     .icon = guikit::Icon::Settings()});
                                menu.Item(selectedCategory, NavCategory::ElemInputField, {.text = "Input",      .icon = guikit::Icon::Edit()});
                                menu.Item(selectedCategory, NavCategory::ElemSelect,     {.text = "Select",     .icon = guikit::Icon::Plus()});
                                menu.Item(selectedCategory, NavCategory::ElemSwitch,     {.text = "Switch",     .icon = guikit::Icon::Refresh()});
                                menu.Item(selectedCategory, NavCategory::ElemCheckbox,   {.text = "Checkbox",   .icon = guikit::Icon::Check()});
                                menu.Item(selectedCategory, NavCategory::ElemTabBar,     {.text = "TabBar",     .icon = guikit::Icon::Folder()});
                                menu.Item(selectedCategory, NavCategory::ElemSidebar,    {.text = "Sidebar",    .icon = guikit::Icon::Folder()});
                                menu.Item(selectedCategory, NavCategory::ElemContainers, {.text = "Containers", .icon = guikit::Icon::Copy()});
                                menu.Item(selectedCategory, NavCategory::ElemTypography, {.text = "Typography", .icon = guikit::Icon::Edit()});

                                menu.Section("ASSEMBLY");
                                menu.Item(selectedCategory, NavCategory::AsmDashboard,    {.text = "Dashboard",    .icon = guikit::Icon::Settings()});
                                menu.Item(selectedCategory, NavCategory::AsmFormControls, {.text = "Form Demo",    .icon = guikit::Icon::Edit()});
                                menu.Item(selectedCategory, NavCategory::AsmFlexRibbon,   {.text = "Flex Ribbon",  .icon = guikit::Icon::Folder()});
                                menu.Item(selectedCategory, NavCategory::AsmDiagnostics,  {.text = "Diagnostics",  .icon = guikit::Icon::Eye()});
                                menu.Item(selectedCategory, NavCategory::AsmThemeStudio,  {.text = "Theme Studio", .icon = guikit::Icon::Refresh()});
                            }
                            if (selectedCategory != prevCat) {
                                ui.InvalidateAll();
                            }
                        }

                        // 2. Level 2: Inspector Panel (Automatically docks right after the first Sidebar)
                        if (auto inspectorSidebar = ui.Sidebar({
                            .side = guikit::SidebarSide::Left,
                            .width = 340.0f,
                            .gap = 8.0f,
                            .padding = guikit::Padding::All(12.0f)
                        })) {
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

                                    auto renderDescVariantPicker = [&](guikit::UiVariant& descVar, bool enabled) {
                                        static constexpr std::array<std::string_view, 7> descVarShort = {"Mut", "Def", "Pri", "Suc", "Wrn", "Dgr", "Inf"};
                                        static constexpr std::array<std::string_view, 7> descVarFull = {"Muted", "Default", "Primary", "Success", "Warning", "Danger", "Info"};
                                        static constexpr std::array<guikit::UiVariant, 7> descVars = {
                                            guikit::UiVariant::Muted, guikit::UiVariant::Default, guikit::UiVariant::Primary,
                                            guikit::UiVariant::Success, guikit::UiVariant::Warning, guikit::UiVariant::Danger, guikit::UiVariant::Info
                                        };
                                        size_t descVarIdx = 0;
                                        for (size_t i = 0; i < descVars.size(); ++i) {
                                            if (descVars[i] == descVar) { descVarIdx = i; break; }
                                        }
                                        if (ui.StepSlider(descVarIdx, descVarShort, {
                                            .label = "Description Variant",
                                            .showTicks = true,
                                            .showLabels = true,
                                            .showValue = true,
                                            .valueText = {variantName(descVar), (descVar == guikit::UiVariant::Muted) ? guikit::UiVariant::Secondary : descVar, descVarFull},
                                            .variant = (descVar == guikit::UiVariant::Muted) ? guikit::UiVariant::Secondary : descVar,
                                            .disabled = !enabled
                                        }).changed) {
                                            descVar = descVars[descVarIdx];
                                            ui.InvalidateAll();
                                        }
                                    };

                                    if (isElementCategory(selectedCategory)) {
                                        switch (selectedCategory) {
                                            case NavCategory::ElemButton: {
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
                                                if (ui.Slider(btnCornerRadius, 0.0f, 24.0f, {.label = "Corner Radius", .valueFormat = "%.0f px"}).changed) {
                                                    ui.InvalidateAll();
                                                }

                                                renderVariantPicker();

                                                if (ui.Switch(btnShowLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(btnShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                renderDescVariantPicker(btnDescVariant, btnShowDesc);
                                                const bool btnHasExternal = btnShowLabel || btnShowDesc;

                                                size_t btnLayoutIdx = static_cast<size_t>(btnLayout);
                                                if (ui.StepSlider(btnLayoutIdx, elementLayoutNames, {.label = "Element Layout", .showTicks = true, .showLabels = true, .disabled = !btnHasExternal}).changed) {
                                                    btnLayout = static_cast<guikit::ElementLayout>(btnLayoutIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t btnOrderIdx = static_cast<size_t>(btnInlineOrder);
                                                if (ui.StepSlider(btnOrderIdx, inlineOrderNames, {.label = "Inline Order", .showTicks = true, .showLabels = true, .disabled = (!btnHasExternal || btnLayout != guikit::ElementLayout::Inline)}).changed) {
                                                    btnInlineOrder = static_cast<guikit::InlineOrder>(btnOrderIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t btnAlignIdx = static_cast<size_t>(btnElementAlign);
                                                if (ui.StepSlider(btnAlignIdx, labelAlignNames, {.label = "Element Alignment", .showTicks = true, .showLabels = true, .disabled = (!btnHasExternal && btnStretchIdx == 0)}).changed) {
                                                    btnElementAlign = static_cast<guikit::Align>(btnAlignIdx);
                                                    ui.InvalidateAll();
                                                }

                                                if (ui.StepSlider(btnStretchIdx, stretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
                                                if (btnStretchIdx == 2) {
                                                    if (ui.Slider(btnFillPct, 10.0f, 100.0f, {.label = "Explicit Width", .valueFormat = "%.0f %%", .step = 5.0f}).changed) ui.InvalidateAll();
                                                } else {
                                                    if (ui.Slider(btnWidth, 0.0f, 400.0f, {.label = "Explicit Width", .valueFormat = {(btnWidth > 0.0f) ? "%.0f px" : "Auto", {"Auto", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
                                                }
                                                static constexpr std::array<std::string_view, 3> alignNames = {"Start (Left)", "Center", "End (Right)"};
                                                size_t alignIdx = (btnControlAlign == guikit::Align::Start) ? 0 : ((btnControlAlign == guikit::Align::End) ? 2 : 1);
                                                if (ui.StepSlider(alignIdx, alignNames, {.label = "Control Alignment", .showTicks = true, .showLabels = true}).changed) {
                                                    btnControlAlign = (alignIdx == 0) ? guikit::Align::Start : ((alignIdx == 2) ? guikit::Align::End : guikit::Align::Center);
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

                                                if (ui.Switch(sliderShowLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(sliderShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                renderDescVariantPicker(sliderDescVariant, sliderShowDesc);
                                                const bool sliderHasExternal = sliderShowLabel || sliderShowDesc;

                                                size_t slLayoutIdx = static_cast<size_t>(sliderLayout);
                                                if (ui.StepSlider(slLayoutIdx, elementLayoutNames, {.label = "Element Layout", .showTicks = true, .showLabels = true, .disabled = !sliderHasExternal}).changed) {
                                                    sliderLayout = static_cast<guikit::ElementLayout>(slLayoutIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t slOrderIdx = static_cast<size_t>(sliderInlineOrder);
                                                if (ui.StepSlider(slOrderIdx, inlineOrderNames, {.label = "Inline Order", .showTicks = true, .showLabels = true, .disabled = (!sliderHasExternal || sliderLayout != guikit::ElementLayout::Inline)}).changed) {
                                                    sliderInlineOrder = static_cast<guikit::InlineOrder>(slOrderIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t slAlignIdx = static_cast<size_t>(sliderElementAlign);
                                                if (ui.StepSlider(slAlignIdx, labelAlignNames, {.label = "Element Alignment", .showTicks = true, .showLabels = true, .disabled = (!sliderHasExternal && sliderStretchIdx == 0)}).changed) {
                                                    sliderElementAlign = static_cast<guikit::Align>(slAlignIdx);
                                                    ui.InvalidateAll();
                                                }

                                                if (ui.StepSlider(sliderStretchIdx, stretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
                                                if (sliderStretchIdx == 2) {
                                                    if (ui.Slider(sliderFillPct, 10.0f, 100.0f, {.label = "Explicit Width", .valueFormat = "%.0f %%", .step = 5.0f}).changed) ui.InvalidateAll();
                                                } else {
                                                    if (ui.Slider(sliderWidth, 0.0f, 400.0f, {.label = "Explicit Width", .valueFormat = {(sliderWidth > 0.0f) ? "%.0f px" : "Default", {"Default", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
                                                }
                                                if (ui.Switch(sliderShowValue, {.label = "Show Value Tag"}).changed) ui.InvalidateAll();
                                                static constexpr std::array<std::string_view, 5> valPlaceNames = {"Lead", "Trail", "Top", "Btm", "Thumb"};
                                                size_t valPlaceIdx = static_cast<size_t>(sliderValuePlacement);
                                                if (ui.StepSlider(valPlaceIdx, valPlaceNames, {
                                                    .label = "Value Placement",
                                                    .showTicks = true,
                                                    .showLabels = true,
                                                    .disabled = !sliderShowValue
                                                }).changed) {
                                                    sliderValuePlacement = static_cast<guikit::Placement>(valPlaceIdx);
                                                    ui.InvalidateAll();
                                                }
                                                if (ui.Switch(sliderShowRangeText, {.label = "Show Range Text (Min / Max)"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(sliderShowTicks, {.label = "Show Step Ticks", .disabled = sliderContinuous}).changed) ui.InvalidateAll();
                                                if (ui.Switch(sliderShowLabels, {.label = "Show Step Labels", .disabled = sliderContinuous}).changed) ui.InvalidateAll();
                                                if (ui.Switch(sliderDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemInputField: {
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
                                                if (ui.Slider(inputCornerRadius, 0.0f, 24.0f, {.label = "Corner Radius", .valueFormat = "%.0f px"}).changed) {
                                                    ui.InvalidateAll();
                                                }

                                                renderVariantPicker();

                                                if (ui.Switch(inputShowLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(inputShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                renderDescVariantPicker(inputDescVariant, inputShowDesc);
                                                const bool inHasExternal = inputShowLabel || inputShowDesc;

                                                size_t inLayoutIdx = static_cast<size_t>(inputLayout);
                                                if (ui.StepSlider(inLayoutIdx, elementLayoutNames, {.label = "Element Layout", .showTicks = true, .showLabels = true, .disabled = !inHasExternal}).changed) {
                                                    inputLayout = static_cast<guikit::ElementLayout>(inLayoutIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t inOrderIdx = static_cast<size_t>(inputInlineOrder);
                                                if (ui.StepSlider(inOrderIdx, inlineOrderNames, {.label = "Inline Order", .showTicks = true, .showLabels = true, .disabled = (!inHasExternal || inputLayout != guikit::ElementLayout::Inline)}).changed) {
                                                    inputInlineOrder = static_cast<guikit::InlineOrder>(inOrderIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t inAlignIdx = static_cast<size_t>(inputElementAlign);
                                                if (ui.StepSlider(inAlignIdx, labelAlignNames, {.label = "Element Alignment", .showTicks = true, .showLabels = true, .disabled = (!inHasExternal && inputStretchIdx == 0)}).changed) {
                                                    inputElementAlign = static_cast<guikit::Align>(inAlignIdx);
                                                    ui.InvalidateAll();
                                                }

                                                if (ui.StepSlider(inputStretchIdx, stretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
                                                if (inputStretchIdx == 2) {
                                                    if (ui.Slider(inputFillPct, 10.0f, 100.0f, {.label = "Explicit Width", .valueFormat = "%.0f %%", .step = 5.0f}).changed) ui.InvalidateAll();
                                                } else {
                                                    if (ui.Slider(inputWidth, 0.0f, 400.0f, {.label = "Explicit Width", .valueFormat = {(inputWidth > 0.0f) ? "%.0f px" : "Default", {"Default", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
                                                }

                                                static constexpr std::array<std::string_view, 3> cursorModes = {"Blinking", "Solid", "Hidden"};
                                                size_t curIdx = (inputCursorMode == guikit::CursorBlinkMode::Solid) ? 1 : ((inputCursorMode == guikit::CursorBlinkMode::Hidden) ? 2 : 0);
                                                if (ui.StepSlider(curIdx, cursorModes, {.label = "Cursor Blink Mode", .showTicks = true, .showLabels = true}).changed) {
                                                    inputCursorMode = (curIdx == 1) ? guikit::CursorBlinkMode::Solid : ((curIdx == 2) ? guikit::CursorBlinkMode::Hidden : guikit::CursorBlinkMode::Blinking);
                                                    ui.InvalidateAll();
                                                }
                                                if (ui.Switch(inputHasPrefix, {.label = "Prefix (https://)"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(inputHasSuffix, {.label = "Suffix (:443)"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(inputClearable, {.label = "Clearable Button ('X')"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(inputPassword, {.label = "Password Mask & Reveal"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(inputReadOnly, {.label = "Read Only"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(inputDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemSelect: {
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
                                                if (ui.Slider(selectCornerRadius, 0.0f, 24.0f, {.label = "Corner Radius", .valueFormat = "%.0f px"}).changed) {
                                                    ui.InvalidateAll();
                                                }
                                                float maxItemsFloat = static_cast<float>(selectMaxItems);
                                                if (ui.Slider(maxItemsFloat, 2.0f, 8.0f, {.label = "Max Visible Menu Items", .valueFormat = "%.0f items", .step = 1.0}).changed) {
                                                    selectMaxItems = static_cast<uint32_t>(maxItemsFloat);
                                                    ui.InvalidateAll();
                                                }

                                                renderVariantPicker();

                                                if (ui.Switch(selectShowLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(selectShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                renderDescVariantPicker(selectDescVariant, selectShowDesc);
                                                const bool selHasExternal = selectShowLabel || selectShowDesc;

                                                size_t selLayoutIdx = static_cast<size_t>(selectLayout);
                                                if (ui.StepSlider(selLayoutIdx, elementLayoutNames, {.label = "Element Layout", .showTicks = true, .showLabels = true, .disabled = !selHasExternal}).changed) {
                                                    selectLayout = static_cast<guikit::ElementLayout>(selLayoutIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t selOrderIdx = static_cast<size_t>(selectInlineOrder);
                                                if (ui.StepSlider(selOrderIdx, inlineOrderNames, {.label = "Inline Order", .showTicks = true, .showLabels = true, .disabled = (!selHasExternal || selectLayout != guikit::ElementLayout::Inline)}).changed) {
                                                    selectInlineOrder = static_cast<guikit::InlineOrder>(selOrderIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t selAlignIdx = static_cast<size_t>(selectElementAlign);
                                                if (ui.StepSlider(selAlignIdx, labelAlignNames, {.label = "Element Alignment", .showTicks = true, .showLabels = true, .disabled = (!selHasExternal && selectStretchIdx == 0)}).changed) {
                                                    selectElementAlign = static_cast<guikit::Align>(selAlignIdx);
                                                    ui.InvalidateAll();
                                                }

                                                if (ui.StepSlider(selectStretchIdx, stretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
                                                if (selectStretchIdx == 2) {
                                                    if (ui.Slider(selectFillPct, 10.0f, 100.0f, {.label = "Explicit Width", .valueFormat = "%.0f %%", .step = 5.0f}).changed) ui.InvalidateAll();
                                                } else {
                                                    if (ui.Slider(selectWidth, 0.0f, 400.0f, {.label = "Explicit Width", .valueFormat = {(selectWidth > 0.0f) ? "%.0f px" : "Auto", {"Auto", "%.0f px"}}, .step = 10.0f}).changed) ui.InvalidateAll();
                                                }

                                                if (ui.Switch(selectHasPrefix, {.label = "Prefix Icon"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(selectDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemSwitch: {
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();

                                                renderVariantPicker();

                                                if (ui.Switch(switchShowLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(switchShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                renderDescVariantPicker(switchDescVariant, switchShowDesc);
                                                const bool switchHasExternal = switchShowLabel || switchShowDesc;

                                                size_t swLayoutIdx = static_cast<size_t>(switchLayout);
                                                if (ui.StepSlider(swLayoutIdx, elementLayoutNames, {.label = "Element Layout", .showTicks = true, .showLabels = true, .disabled = !switchHasExternal}).changed) {
                                                    switchLayout = static_cast<guikit::ElementLayout>(swLayoutIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t swOrderIdx = static_cast<size_t>(switchInlineOrder);
                                                if (ui.StepSlider(swOrderIdx, inlineOrderNames, {.label = "Inline Order", .showTicks = true, .showLabels = true, .disabled = (!switchHasExternal || switchLayout != guikit::ElementLayout::Inline)}).changed) {
                                                    switchInlineOrder = static_cast<guikit::InlineOrder>(swOrderIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t swAlignIdx = static_cast<size_t>(switchElementAlign);
                                                if (ui.StepSlider(swAlignIdx, labelAlignNames, {.label = "Element Alignment", .showTicks = true, .showLabels = true, .disabled = (!switchHasExternal && switchStretchIdx == 0)}).changed) {
                                                    switchElementAlign = static_cast<guikit::Align>(swAlignIdx);
                                                    ui.InvalidateAll();
                                                }

                                                if (ui.StepSlider(switchStretchIdx, stretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
                                                if (ui.Switch(testDarkLightSwitch, {.label = "Side Text (Off / On)"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(switchSymmetric, {.label = "Symmetric (Neutral Track)"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(switchDisabled, {.label = "Disabled State"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemCheckbox: {
                                                if (ui.StepSlider(controlsSize, {.label = "Controls Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();

                                                renderVariantPicker();

                                                if (ui.Switch(checkboxShowLabel, {.label = "Show Label"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(checkboxShowDesc, {.label = "Show Description"}).changed) ui.InvalidateAll();
                                                renderDescVariantPicker(checkboxDescVariant, checkboxShowDesc);
                                                const bool checkHasExternal = checkboxShowLabel || checkboxShowDesc;

                                                size_t cbLayoutIdx = static_cast<size_t>(checkboxLayout);
                                                if (ui.StepSlider(cbLayoutIdx, elementLayoutNames, {.label = "Element Layout", .showTicks = true, .showLabels = true, .disabled = !checkHasExternal}).changed) {
                                                    checkboxLayout = static_cast<guikit::ElementLayout>(cbLayoutIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t cbOrderIdx = static_cast<size_t>(checkboxInlineOrder);
                                                if (ui.StepSlider(cbOrderIdx, inlineOrderNames, {.label = "Inline Order", .showTicks = true, .showLabels = true, .disabled = (!checkHasExternal || checkboxLayout != guikit::ElementLayout::Inline)}).changed) {
                                                    checkboxInlineOrder = static_cast<guikit::InlineOrder>(cbOrderIdx);
                                                    ui.InvalidateAll();
                                                }

                                                size_t cbAlignIdx = static_cast<size_t>(checkboxElementAlign);
                                                if (ui.StepSlider(cbAlignIdx, labelAlignNames, {.label = "Element Alignment", .showTicks = true, .showLabels = true, .disabled = (!checkHasExternal && checkboxStretchIdx == 0)}).changed) {
                                                    checkboxElementAlign = static_cast<guikit::Align>(cbAlignIdx);
                                                    ui.InvalidateAll();
                                                }

                                                if (ui.StepSlider(checkboxStretchIdx, stretchModeNames, {.label = "Stretch Mode", .showTicks = true, .showLabels = true}).changed) ui.InvalidateAll();
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
                                                if (ui.Slider(testTabRadius, 0.0f, 24.0f, {.label = "Container Corner Radius", .valueFormat = "%.0f px", .disabled = !testTabHasBg}).changed) ui.InvalidateAll();
                                                if (ui.Slider(testTabPadding, 0.0f, 12.0f, {.label = "Container Internal Padding", .valueFormat = "%.0f px", .disabled = !testTabHasBg}).changed) ui.InvalidateAll();
                                                if (ui.Slider(testTabBorderWidth, 0.0f, 4.0f, {.label = "Container Border Width", .valueFormat = "%.1f px", .disabled = !testTabHasBg}).changed) ui.InvalidateAll();
                                                if (ui.Slider(tabGap, 0.0f, 24.0f, {.label = "Tab Spacing (Gap)", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(testTabShowSep, {.label = "Baseline Separator"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(testTabFullWidth, {.label = "Full Width Stretch"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(testTabScrollable, {.label = "Scrollable Ribbon"}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemSidebar: {
                                                if (ui.StepSlider(controlsSize, {.label = "Menu Size", .showTicks = true, .showLabels = true, .showValue = true}).changed) ui.InvalidateAll();
                                                renderVariantPicker();

                                                static constexpr std::array<std::string_view, 2> sbMenuStyleNames = {"Pill", "Indicator"};
                                                size_t sbStyleIdx = (testSidebarMenuStyle == guikit::SidebarMenuStyle::Indicator) ? 1 : 0;
                                                if (ui.StepSlider(sbStyleIdx, sbMenuStyleNames, {.label = "Menu Style", .showTicks = true, .showLabels = true}).changed) {
                                                    testSidebarMenuStyle = (sbStyleIdx == 1) ? guikit::SidebarMenuStyle::Indicator : guikit::SidebarMenuStyle::Pill;
                                                    ui.InvalidateAll();
                                                }

                                                static constexpr std::array<std::string_view, 2> sbSideNames = {"Left", "Right"};
                                                size_t sbSideIdx = (testSidebarSide == guikit::SidebarSide::Right) ? 1 : 0;
                                                if (ui.StepSlider(sbSideIdx, sbSideNames, {.label = "Dock Side", .showTicks = true, .showLabels = true}).changed) {
                                                    testSidebarSide = (sbSideIdx == 1) ? guikit::SidebarSide::Right : guikit::SidebarSide::Left;
                                                    ui.InvalidateAll();
                                                }

                                                if (ui.Switch(testSidebarCollapsed, {.label = "Collapsed Rail (Icons Only)"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(testSidebarShowSections, {.label = "Show Section Headers"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(testSidebarShowBadges, {.label = "Show Trailing Badges"}).changed) ui.InvalidateAll();
                                                if (ui.Switch(testSidebarAutoWidth, {.label = "Auto-Fit Width"}).changed) ui.InvalidateAll();
                                                if (ui.Slider(testSidebarFixedWidth, 120.0f, 300.0f, {.label = "Explicit Width", .valueFormat = "%.0f px", .disabled = testSidebarAutoWidth}).changed) ui.InvalidateAll();
                                                break;
                                            }

                                            case NavCategory::ElemContainers: {
                                                if (ui.Slider(cardRadius, 0.0f, 24.0f, {.label = "Corner Radius", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Slider(cardPadding, 0.0f, 32.0f, {.label = "Internal Padding", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Slider(cardGap, 0.0f, 24.0f, {.label = "Content Spacing (Gap)", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
                                                if (ui.Slider(cardBorderWidth, 0.0f, 4.0f, {.label = "Border Width", .valueFormat = "%.1f px"}).changed) ui.InvalidateAll();
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
                                        if (ui.Switch(previewDarkMode, {
                                            .label = "Preview Dark Mode",
                                            .description = "Switch preview between Dark and Light palette"
                                        }).changed) {
                                            previewLightMode = !previewDarkMode;
                                            const auto defTheme = previewDarkMode ? guikit::UiTheme::Dark(previewTheme.density) : guikit::UiTheme::Light(previewTheme.density);
                                            previewTheme.palette = defTheme.palette;
                                            previewTheme.variants = defTheme.variants;
                                            previewTheme.mode = previewDarkMode ? guikit::ThemeMode::Dark : guikit::ThemeMode::Light;
                                            ui.InvalidateAll();
                                        }

                                        static constexpr std::array<std::string_view, 3> asmDensityLabels = {"Compact", "Standard", "Touch"};
                                        size_t dIdx = (previewTheme.density == guikit::Density::Compact) ? 0 : ((previewTheme.density == guikit::Density::Touch) ? 2 : 1);
                                        if (ui.StepSlider(dIdx, asmDensityLabels, {.label = "Layout Density", .showTicks = true, .showLabels = true}).changed) {
                                            previewTheme.density = (dIdx == 0) ? guikit::Density::Compact : ((dIdx == 2) ? guikit::Density::Touch : guikit::Density::Standard);
                                            previewTheme.SetDensity(previewTheme.density);
                                            ui.InvalidateAll();
                                        }

                                        if (ui.Slider(gridColGap, 0.0f, 32.0f, {.label = "Horizontal Gap (Columns)", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
                                        if (ui.Slider(gridRowGap, 0.0f, 32.0f, {.label = "Vertical Gap (Rows)", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();

                                        if (ui.Slider(cardRadius, 0.0f, 24.0f, {.label = "Card Corner Radius", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
                                        if (ui.Slider(cardPadding, 4.0f, 32.0f, {.label = "Card Padding", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
                                        if (ui.Slider(cardBorderWidth, 0.0f, 4.0f, {.label = "Card Border Width", .valueFormat = "%.1f px"}).changed) ui.InvalidateAll();

                                        if (selectedCategory == NavCategory::AsmDashboard) {
                                            if (ui.Switch(hwEstop, {.label = "Hardware E-Stop Line", .variant = guikit::UiVariant::Danger}).changed) ui.InvalidateAll();
                                            if (ui.Switch(enableSensors, {.label = "Sensor Drop Limits"}).changed) ui.InvalidateAll();
                                            if (ui.Switch(diagLogging, {.label = "High-Speed Flash Logging"}).changed) ui.InvalidateAll();
                                        } else if (selectedCategory == NavCategory::AsmFormControls) {
                                            if (ui.Switch(formMode, {.label = "Stacked Labels Mode"}).changed) ui.InvalidateAll();
                                            if (ui.Switch(showTelemetry, {.label = "Show Telemetry Banner"}).changed) ui.InvalidateAll();
                                        } else if (selectedCategory == NavCategory::AsmFlexRibbon) {
                                            if (ui.Slider(carouselGap, 0.0f, 24.0f, {.label = "Ribbon Items Gap", .valueFormat = "%.0f px"}).changed) ui.InvalidateAll();
                                        }
                                    }
                        }
                    }

                    // Content Canvas (Center / Right Workspace)
                    if (auto contentBox = ui.Box({
                        .widthMode = guikit::SizeMode::Stretch,
                        .heightMode = guikit::SizeMode::Stretch,
                        .padding = guikit::Padding::All(8.0f),
                        .backgroundColor = shellTheme.palette.background,
                        .borderColor = shellTheme.palette.borderSubtle,
                        .borderWidth = 1.0f,
                        .radius = 8.0f,
                        .clipChildren = true
                    })) {
                        if (auto contentCol = ui.Column({
                            .gap = colGap,
                            .widthMode = guikit::SizeMode::Stretch,
                            .heightMode = guikit::SizeMode::Stretch
                        })) {
                            // Local Preview Toolbar: Scale | Dark/Light | Bounds | Inspect (affects ONLY the Preview)
                            if (auto previewTb = ui.Toolbar()) {
                                ui.Text("PREVIEW", {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Muted});
                                previewTb.Separator();

                                if (auto pScaleBox = ui.Box({.fixedWidth = 92.0f})) {
                                    if (ui.Select(previewScaleIndex, scaleLabels, {.size = guikit::UiSize::Sm}).changed) {
                                        ui.InvalidateAll();
                                    }
                                }

                                if (ui.Switch(previewLightMode, {
                                    .textLeft = "Dark",
                                    .textRight = "Light",
                                    .symmetric = true,
                                    .size = guikit::UiSize::Md
                                }).changed) {
                                    previewDarkMode = !previewLightMode;
                                    const auto defTheme = previewDarkMode ? guikit::UiTheme::Dark(previewTheme.density) : guikit::UiTheme::Light(previewTheme.density);
                                    previewTheme.palette = defTheme.palette;
                                    previewTheme.variants = defTheme.variants;
                                    previewTheme.mode = previewDarkMode ? guikit::ThemeMode::Dark : guikit::ThemeMode::Light;
                                    ui.InvalidateAll();
                                }

                                if (previewTb.ToolButton({
                                    .label = "Bounds",
                                    .icon = previewBounds ? guikit::Icon::Eye() : guikit::Icon::EyeOff(),
                                    .variant = previewBounds ? guikit::UiVariant::Info : guikit::UiVariant::Ghost,
                                    .active = previewBounds
                                })) {
                                    previewBounds = !previewBounds;
                                    ui.InvalidateAll();
                                }

                                if (previewTb.ToolButton({
                                    .label = "Inspect",
                                    .icon = guikit::Icon::Search(),
                                    .variant = previewInspect ? guikit::UiVariant::Warning : guikit::UiVariant::Ghost,
                                    .active = previewInspect
                                })) {
                                    previewInspect = !previewInspect;
                                    ui.InvalidateAll();
                                }
                            }

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
                                    .theme = &previewTheme,
                                    .scale = previewScale,
                                    .debugBounds = previewBounds,
                                    .inspectMode = previewInspect
                                })) {
                                    // Hero Showcase Stage
                                    if (auto stageBox = ui.Box({
                                        .widthMode = guikit::SizeMode::Stretch,
                                        .heightMode = guikit::SizeMode::Stretch,
                                        .padding = guikit::Padding::All(24.0f),
                                        .backgroundColor = th.palette.surfaceElevated,
                                        .borderColor = th.palette.borderSubtle,
                                        .borderWidth = 1.0f,
                                        .radius = 8.0f
                                    })) {
                                        if (auto centerRow = ui.Row({
                                            .align = guikit::Alignment::Center,
                                            .justify = guikit::Justify::Center,
                                            .heightMode = guikit::SizeMode::Stretch
                                        })) {
                                            switch (selectedCategory) {
                                                case NavCategory::ElemButton: {
                                                    std::string btnText = btnClicks > 0 ? std::format("Clicked {}x", btnClicks) : "Interactive Button";
                                                    if (ui.Button({
                                                        .label = btnShowLabel ? "System Action" : "",
                                                        .text = btnText,
                                                        .description = {btnShowDesc ? "Commit payload and trigger immediate canary rollout" : "", btnDescVariant},
                                                        .icon = btnHasIcon ? guikit::Icon::Play() : guikit::Icon{},
                                                        .variant = btnVariant,
                                                        .iconPlacement = btnIconPlacement,
                                                        .layout = btnLayout,
                                                        .inlineOrder = btnInlineOrder,
                                                        .elementAlign = btnElementAlign,
                                                        .controlAlign = btnControlAlign,
                                                        .stretch = static_cast<guikit::StretchMode>(btnStretchIdx),
                                                        .width = (btnStretchIdx == 2) ? std::optional<float>(btnFillPct) : ((btnWidth > 0.0f) ? std::optional<float>(btnWidth) : std::nullopt),
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
                                                            .textLeft = sliderShowRangeText ? "0%" : "",
                                                            .textRight = sliderShowRangeText ? "100%" : "",
                                                            .description = {sliderShowDesc ? "Adjust internal preamp attenuation" : "", sliderDescVariant},
                                                            .valueFormat = "%.1f %%",
                                                            .layout = sliderLayout,
                                                            .inlineOrder = sliderInlineOrder,
                                                            .elementAlign = sliderElementAlign,
                                                            .showValue = sliderShowValue,
                                                            .valuePlacement = sliderValuePlacement,
                                                            .variant = btnVariant,
                                                            .size = controlsSize,
                                                            .stretch = static_cast<guikit::StretchMode>(sliderStretchIdx),
                                                            .width = (sliderStretchIdx == 2) ? std::optional<float>(sliderFillPct) : ((sliderWidth > 0.0f) ? std::optional<float>(sliderWidth) : std::nullopt),
                                                            .disabled = sliderDisabled
                                                        });
                                                    } else {
                                                        static constexpr std::array<std::string_view, 5> stepItems = {"Off", "Low", "Mid", "High", "Max"};
                                                        ui.StepSlider(testStepIdx, stepItems, {
                                                            .label = sliderShowLabel ? "DSP Filter Mode" : "",
                                                            .textLeft = sliderShowRangeText ? "Min" : "",
                                                            .textRight = sliderShowRangeText ? "Max" : "",
                                                            .description = {sliderShowDesc ? "Select active FIR profile" : "", sliderDescVariant},
                                                            .layout = sliderLayout,
                                                            .inlineOrder = sliderInlineOrder,
                                                            .elementAlign = sliderElementAlign,
                                                            .showTicks = sliderShowTicks,
                                                            .showLabels = sliderShowLabels,
                                                            .showValue = sliderShowValue,
                                                            .valuePlacement = sliderValuePlacement,
                                                            .variant = btnVariant,
                                                            .size = controlsSize,
                                                            .stretch = static_cast<guikit::StretchMode>(sliderStretchIdx),
                                                            .width = (sliderStretchIdx == 2) ? std::optional<float>(sliderFillPct) : ((sliderWidth > 0.0f) ? std::optional<float>(sliderWidth) : std::nullopt),
                                                            .disabled = sliderDisabled
                                                        });
                                                    }
                                                    break;
                                                }

                                                case NavCategory::ElemInputField: {
                                                    ui.InputField(testInputText, {
                                                        .label = inputShowLabel ? "Server Endpoint" : "",
                                                        .description = {inputShowDesc ? "Configured host address" : "", inputDescVariant},
                                                        .placeholder = "Enter domain...",
                                                        .prefix = inputHasPrefix ? "https://" : "",
                                                        .suffix = inputHasSuffix ? ":443" : "",
                                                        .valueFormat = {{"guikit.dev", "api.guikit.dev"}},
                                                        .variant = btnVariant,
                                                        .layout = inputLayout,
                                                        .inlineOrder = inputInlineOrder,
                                                        .elementAlign = inputElementAlign,
                                                        .cursorMode = inputCursorMode,
                                                        .size = controlsSize,
                                                        .radius = inputCornerRadius,
                                                        .stretch = static_cast<guikit::StretchMode>(inputStretchIdx),
                                                        .width = (inputStretchIdx == 2) ? std::optional<float>(inputFillPct) : ((inputWidth > 0.0f) ? std::optional<float>(inputWidth) : std::nullopt),
                                                        .password = inputPassword,
                                                        .clearable = inputClearable,
                                                        .disabled = inputDisabled,
                                                        .readOnly = inputReadOnly
                                                    });
                                                    break;
                                                }

                                                case NavCategory::ElemSelect: {
                                                    ui.Select(testSelectIdx, selectItems, {
                                                        .label = selectShowLabel ? "Active Preset" : "",
                                                        .description = {selectShowDesc ? "Dropdown selection" : "", selectDescVariant},
                                                        .prefix = selectHasPrefix ? guikit::Icon::Settings() : guikit::Icon{},
                                                        .variant = btnVariant,
                                                        .layout = selectLayout,
                                                        .inlineOrder = selectInlineOrder,
                                                        .elementAlign = selectElementAlign,
                                                        .maxVisibleItems = selectMaxItems,
                                                        .size = controlsSize,
                                                        .radius = selectCornerRadius,
                                                        .stretch = static_cast<guikit::StretchMode>(selectStretchIdx),
                                                        .width = (selectStretchIdx == 2) ? std::optional<float>(selectFillPct) : ((selectWidth > 0.0f) ? std::optional<float>(selectWidth) : std::nullopt),
                                                        .disabled = selectDisabled
                                                    });
                                                    break;
                                                }

                                                case NavCategory::ElemSwitch: {
                                                    ui.Switch(testSwitchState, {
                                                        .label = switchShowLabel ? "Hardware E-Stop" : "",
                                                        .textLeft = testDarkLightSwitch ? "Off" : "",
                                                        .textRight = testDarkLightSwitch ? "On" : "",
                                                        .description = {switchShowDesc ? "Safety interlocking system" : "", switchDescVariant},
                                                        .variant = btnVariant,
                                                        .layout = switchLayout,
                                                        .inlineOrder = switchInlineOrder,
                                                        .elementAlign = switchElementAlign,
                                                        .symmetric = switchSymmetric,
                                                        .stretch = static_cast<guikit::StretchMode>(switchStretchIdx),
                                                        .disabled = switchDisabled,
                                                        .size = controlsSize
                                                    });
                                                    break;
                                                }

                                                case NavCategory::ElemCheckbox: {
                                                    ui.Checkbox(testCheckboxState, {
                                                        .label = checkboxShowLabel ? "Telemetry Logging" : "",
                                                        .description = {checkboxShowDesc ? "Save trace to buffer" : "", checkboxDescVariant},
                                                        .variant = btnVariant,
                                                        .layout = checkboxLayout,
                                                        .inlineOrder = checkboxInlineOrder,
                                                        .elementAlign = checkboxElementAlign,
                                                        .stretch = static_cast<guikit::StretchMode>(checkboxStretchIdx),
                                                        .disabled = checkboxDisabled,
                                                        .indeterminate = testCheckboxIndet,
                                                        .size = controlsSize
                                                    });
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

                                                case NavCategory::ElemSidebar: {
                                                    if (auto demoWorkspace = ui.Box({
                                                        .gap = 12.0f,
                                                        .widthMode = guikit::SizeMode::Stretch,
                                                        .heightMode = guikit::SizeMode::Stretch
                                                    })) {
                                                        if (auto sb = ui.Sidebar({
                                                            .side = testSidebarSide,
                                                            .width = testSidebarAutoWidth ? std::nullopt : std::optional<float>(testSidebarFixedWidth),
                                                            .scrollable = true
                                                        })) {
                                                            const int prevIdx = testSidebarMenuIdx;
                                                            if (auto menu = ui.SidebarMenu({
                                                                .style = testSidebarMenuStyle,
                                                                .collapsed = testSidebarCollapsed,
                                                                .variant = btnVariant,
                                                                .size = controlsSize
                                                            })) {
                                                                if (testSidebarShowSections) {
                                                                    menu.Section("WORKSPACE");
                                                                }
                                                                menu.Item(testSidebarMenuIdx, 0, {
                                                                    .text = "Overview",
                                                                    .icon = guikit::Icon::Folder(),
                                                                    .badge = testSidebarShowBadges ? "3" : ""
                                                                });
                                                                menu.Item(testSidebarMenuIdx, 1, {
                                                                    .text = "Analytics",
                                                                    .icon = guikit::Icon::Search(),
                                                                    .badge = testSidebarShowBadges ? "New" : ""
                                                                });
                                                                menu.Item(testSidebarMenuIdx, 2, {
                                                                    .text = "Deployments",
                                                                    .icon = guikit::Icon::Play()
                                                                });
                                                                if (testSidebarShowSections) {
                                                                    menu.Section("SYSTEM");
                                                                }
                                                                menu.Item(testSidebarMenuIdx, 3, {
                                                                    .text = "Settings",
                                                                    .icon = guikit::Icon::Settings()
                                                                });
                                                            }
                                                            if (testSidebarMenuIdx != prevIdx) {
                                                                ui.InvalidateAll();
                                                            }
                                                        }

                                                        if (auto mainPane = ui.Card({
                                                            .gap = 6.0f,
                                                            .padding = guikit::Padding::All(16.0f),
                                                            .widthMode = guikit::SizeMode::Stretch,
                                                            .heightMode = guikit::SizeMode::Stretch
                                                        })) {
                                                            static constexpr std::array<std::string_view, 4> pageTitles = {
                                                                "Overview Dashboard", "Real-Time Analytics", "Active Deployments", "System Settings"
                                                            };
                                                            ui.Heading({
                                                                .title = pageTitles[static_cast<size_t>(std::clamp(testSidebarMenuIdx, 0, 3))],
                                                                .subtitle = "Docked Sidebar + SidebarMenu container arrangement",
                                                                .size = guikit::UiSize::Sm,
                                                                .divider = true
                                                            });
                                                            ui.Text("Remaining workspace automatically stretches around docked Left/Right Sidebars.", {
                                                                .variant = guikit::UiVariant::Secondary
                                                            });
                                                        }
                                                    }
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
                                            case guikit::Placement::Thumb:    return "Thumb";
                                        }
                                        return "Trailing";
                                    };

                                    auto formatDesc = [&](bool show, std::string_view text, guikit::UiVariant v) -> std::string {
                                        if (!show) return "\"\"";
                                        if (v == guikit::UiVariant::Muted) return std::format("\"{}\"", text);
                                        return std::format("{{\"{}\", guikit::UiVariant::{}}}", text, variantName(v));
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
                                                "    .description = {},\n"
                                                "    .icon = {},\n"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .iconPlacement = guikit::Placement::{},\n"
                                                "    .layout = guikit::ElementLayout::{},\n"
                                                "    .inlineOrder = guikit::InlineOrder::{},\n"
                                                "    .elementAlign = guikit::Align::{},\n"
                                                "    .controlAlign = guikit::Align::{},\n"
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
                                                formatDesc(btnShowDesc, "Commit payload and trigger immediate canary rollout", btnDescVariant),
                                                btnHasIcon ? "guikit::Icon::Play()" : "{}",
                                                variantName(btnVariant),
                                                placementName(btnIconPlacement),
                                                elementLayoutEnumName(btnLayout),
                                                inlineOrderEnumName(btnInlineOrder),
                                                labelAlignEnumName(btnElementAlign),
                                                alignName(btnControlAlign),
                                                stretchModeEnumName(static_cast<guikit::StretchMode>(btnStretchIdx)),
                                                (btnStretchIdx == 2) ? std::format("{:.1f}f", btnFillPct) : ((btnWidth > 0.0f) ? std::format("{:.1f}f", btnWidth) : "std::nullopt"),
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
                                                    "    .textLeft = \"{}\",\n"
                                                    "    .textRight = \"{}\",\n"
                                                    "    .description = {},\n"
                                                    "    .valueFormat = \"%.1f %%\",\n"
                                                    "    .step = std::nullopt,\n"
                                                    "    .layout = guikit::ElementLayout::{},\n"
                                                    "    .inlineOrder = guikit::InlineOrder::{},\n"
                                                    "    .elementAlign = guikit::Align::{},\n"
                                                    "    .showValue = {},\n"
                                                    "    .valuePlacement = guikit::Placement::{},\n"
                                                    "    .variant = guikit::UiVariant::{},\n"
                                                    "    .size = guikit::UiSize::{},\n"
                                                    "    .stretch = guikit::StretchMode::{},\n"
                                                    "    .width = {},\n"
                                                    "    .scale = std::nullopt,\n"
                                                    "    .theme = nullptr,\n"
                                                    "    .disabled = {}\n"
                                                    "}});",
                                                    sliderShowLabel ? "Analog Master Gain" : "",
                                                    sliderShowRangeText ? "0%" : "",
                                                    sliderShowRangeText ? "100%" : "",
                                                    formatDesc(sliderShowDesc, "Adjust internal preamp attenuation", sliderDescVariant),
                                                    elementLayoutEnumName(sliderLayout),
                                                    inlineOrderEnumName(sliderInlineOrder),
                                                    labelAlignEnumName(sliderElementAlign),
                                                    sliderShowValue ? "true" : "false",
                                                    placementName(sliderValuePlacement),
                                                    variantName(btnVariant),
                                                    sizeName(controlsSize),
                                                    stretchModeEnumName(static_cast<guikit::StretchMode>(sliderStretchIdx)),
                                                    (sliderStretchIdx == 2) ? std::format("{:.1f}f", sliderFillPct) : ((sliderWidth > 0.0f) ? std::format("{:.1f}f", sliderWidth) : "std::nullopt"),
                                                    sliderDisabled ? "true" : "false"
                                                );
                                            } else {
                                                snippet = std::format(
                                                    "static constexpr std::array<std::string_view, 5> stepItems = {{\"Off\", \"Low\", \"Mid\", \"High\", \"Max\"}};\n"
                                                    "ui.StepSlider(presetIndex, stepItems, {{\n"
                                                    "    .label = \"{}\",\n"
                                                    "    .textLeft = \"{}\",\n"
                                                    "    .textRight = \"{}\",\n"
                                                    "    .description = {},\n"
                                                    "    .stepLabels = {{}},\n"
                                                    "    .layout = guikit::ElementLayout::{},\n"
                                                    "    .inlineOrder = guikit::InlineOrder::{},\n"
                                                    "    .elementAlign = guikit::Align::{},\n"
                                                    "    .showTicks = {},\n"
                                                    "    .showLabels = {},\n"
                                                    "    .showValue = {},\n"
                                                    "    .valuePlacement = guikit::Placement::{},\n"
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
                                                    sliderShowRangeText ? "Min" : "",
                                                    sliderShowRangeText ? "Max" : "",
                                                    formatDesc(sliderShowDesc, "Select active FIR profile", sliderDescVariant),
                                                    elementLayoutEnumName(sliderLayout),
                                                    inlineOrderEnumName(sliderInlineOrder),
                                                    labelAlignEnumName(sliderElementAlign),
                                                    sliderShowTicks ? "true" : "false",
                                                    sliderShowLabels ? "true" : "false",
                                                    sliderShowValue ? "true" : "false",
                                                    placementName(sliderValuePlacement),
                                                    variantName(btnVariant),
                                                    sizeName(controlsSize),
                                                    stretchModeEnumName(static_cast<guikit::StretchMode>(sliderStretchIdx)),
                                                    (sliderStretchIdx == 2) ? std::format("{:.1f}f", sliderFillPct) : ((sliderWidth > 0.0f) ? std::format("{:.1f}f", sliderWidth) : "std::nullopt"),
                                                    sliderDisabled ? "true" : "false"
                                                );
                                            }
                                            break;
                                        }

                                        case NavCategory::ElemInputField: {
                                            snippet = std::format(
                                                "ui.InputField(serverEndpoint, {{\n"
                                                "    .label = \"{}\",\n"
                                                "    .description = {},\n"
                                                "    .placeholder = \"Enter domain...\",\n"
                                                "    .prefix = \"{}\",\n"
                                                "    .suffix = \"{}\",\n"
                                                "    .valueFormat = {{{{\"guikit.dev\", \"api.guikit.dev\"}}}},\n"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .layout = guikit::ElementLayout::{},\n"
                                                "    .inlineOrder = guikit::InlineOrder::{},\n"
                                                "    .elementAlign = guikit::Align::{},\n"
                                                "    .cursorMode = guikit::CursorBlinkMode::{},\n"
                                                "    .size = guikit::UiSize::{},\n"
                                                "    .radius = {:.1f}f,\n"
                                                "    .stretch = guikit::StretchMode::{},\n"
                                                "    .width = {},\n"
                                                "    .password = {},\n"
                                                "    .clearable = {},\n"
                                                "    .disabled = {},\n"
                                                "    .readOnly = {}\n"
                                                "}});",
                                                inputShowLabel ? "Server Endpoint" : "",
                                                formatDesc(inputShowDesc, "Configured host address", inputDescVariant),
                                                inputHasPrefix ? "https://" : "",
                                                inputHasSuffix ? ":443" : "",
                                                variantName(btnVariant),
                                                elementLayoutEnumName(inputLayout),
                                                inlineOrderEnumName(inputInlineOrder),
                                                labelAlignEnumName(inputElementAlign),
                                                cursorModeName(inputCursorMode),
                                                sizeName(controlsSize),
                                                inputCornerRadius,
                                                stretchModeEnumName(static_cast<guikit::StretchMode>(inputStretchIdx)),
                                                (inputStretchIdx == 2) ? std::format("{:.1f}f", inputFillPct) : ((inputWidth > 0.0f) ? std::format("{:.1f}f", inputWidth) : "std::nullopt"),
                                                inputPassword ? "true" : "false",
                                                inputClearable ? "true" : "false",
                                                inputDisabled ? "true" : "false",
                                                inputReadOnly ? "true" : "false"
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemSelect: {
                                            snippet = std::format(
                                                "ui.Select(presetIndex, itemsList, {{\n"
                                                "    .label = \"{}\",\n"
                                                "    .description = {},\n"
                                                "    .placeholder = \"Select...\",\n"
                                                "    .prefix = {},\n"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .layout = guikit::ElementLayout::{},\n"
                                                "    .inlineOrder = guikit::InlineOrder::{},\n"
                                                "    .elementAlign = guikit::Align::{},\n"
                                                "    .maxVisibleItems = {},\n"
                                                "    .size = guikit::UiSize::{},\n"
                                                "    .radius = {:.1f}f,\n"
                                                "    .stretch = guikit::StretchMode::{},\n"
                                                "    .width = {},\n"
                                                "    .disabled = {}\n"
                                                "}});",
                                                selectShowLabel ? "Active Preset" : "",
                                                formatDesc(selectShowDesc, "Dropdown selection", selectDescVariant),
                                                selectHasPrefix ? "guikit::Icon::Settings()" : "{}",
                                                variantName(btnVariant),
                                                elementLayoutEnumName(selectLayout),
                                                inlineOrderEnumName(selectInlineOrder),
                                                labelAlignEnumName(selectElementAlign),
                                                selectMaxItems,
                                                sizeName(controlsSize),
                                                selectCornerRadius,
                                                stretchModeEnumName(static_cast<guikit::StretchMode>(selectStretchIdx)),
                                                (selectStretchIdx == 2) ? std::format("{:.1f}f", selectFillPct) : ((selectWidth > 0.0f) ? std::format("{:.1f}f", selectWidth) : "std::nullopt"),
                                                selectDisabled ? "true" : "false"
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemSwitch: {
                                            snippet = std::format(
                                                "ui.Switch(switchState, {{\n"
                                                "    .label = \"{}\",\n"
                                                "    .textLeft = \"{}\",\n"
                                                "    .textRight = \"{}\",\n"
                                                "    .description = {},\n"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .layout = guikit::ElementLayout::{},\n"
                                                "    .inlineOrder = guikit::InlineOrder::{},\n"
                                                "    .elementAlign = guikit::Align::{},\n"
                                                "    .symmetric = {},\n"
                                                "    .stretch = guikit::StretchMode::{},\n"
                                                "    .disabled = {},\n"
                                                "    .size = guikit::UiSize::{}\n"
                                                "}});",
                                                switchShowLabel ? "Hardware E-Stop" : "",
                                                testDarkLightSwitch ? "Off" : "",
                                                testDarkLightSwitch ? "On" : "",
                                                formatDesc(switchShowDesc, "Safety interlocking system", switchDescVariant),
                                                variantName(btnVariant),
                                                elementLayoutEnumName(switchLayout),
                                                inlineOrderEnumName(switchInlineOrder),
                                                labelAlignEnumName(switchElementAlign),
                                                switchSymmetric ? "true" : "false",
                                                stretchModeEnumName(static_cast<guikit::StretchMode>(switchStretchIdx)),
                                                switchDisabled ? "true" : "false",
                                                sizeName(controlsSize)
                                            );
                                            break;
                                        }

                                        case NavCategory::ElemCheckbox: {
                                            snippet = std::format(
                                                "ui.Checkbox(checkState, {{\n"
                                                "    .label = \"{}\",\n"
                                                "    .description = {},\n"
                                                "    .variant = guikit::UiVariant::{},\n"
                                                "    .layout = guikit::ElementLayout::{},\n"
                                                "    .inlineOrder = guikit::InlineOrder::{},\n"
                                                "    .elementAlign = guikit::Align::{},\n"
                                                "    .stretch = guikit::StretchMode::{},\n"
                                                "    .disabled = {},\n"
                                                "    .indeterminate = {},\n"
                                                "    .size = guikit::UiSize::{}\n"
                                                "}});",
                                                checkboxShowLabel ? "Telemetry Logging" : "",
                                                formatDesc(checkboxShowDesc, "Save trace to buffer", checkboxDescVariant),
                                                variantName(btnVariant),
                                                elementLayoutEnumName(checkboxLayout),
                                                inlineOrderEnumName(checkboxInlineOrder),
                                                labelAlignEnumName(checkboxElementAlign),
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

                                        case NavCategory::ElemSidebar: {
                                            snippet = std::format(
                                                "if (auto sb = ui.Sidebar({{\n"
                                                "    .side = guikit::SidebarSide::{},\n"
                                                "    .width = {},\n"
                                                "    .scrollable = true\n"
                                                "}})) {{\n"
                                                "    if (auto menu = ui.SidebarMenu({{\n"
                                                "        .style = guikit::SidebarMenuStyle::{},\n"
                                                "        .collapsed = {},\n"
                                                "        .variant = guikit::UiVariant::{},\n"
                                                "        .size = guikit::UiSize::{}\n"
                                                "    }})) {{\n"
                                                "        menu.Section(\"WORKSPACE\");\n"
                                                "        menu.Item(selected, 0, {{.text = \"Overview\", .icon = guikit::Icon::Folder(){}}});\n"
                                                "        menu.Item(selected, 1, {{.text = \"Analytics\", .icon = guikit::Icon::Search(){}}});\n"
                                                "    }}\n"
                                                "}}",
                                                testSidebarSide == guikit::SidebarSide::Right ? "Right" : "Left",
                                                testSidebarAutoWidth ? "std::nullopt" : std::format("{:.1f}f", testSidebarFixedWidth),
                                                testSidebarMenuStyle == guikit::SidebarMenuStyle::Indicator ? "Indicator" : "Pill",
                                                testSidebarCollapsed ? "true" : "false",
                                                variantName(btnVariant),
                                                sizeName(controlsSize),
                                                testSidebarShowBadges ? ", .badge = \"3\"" : "",
                                                testSidebarShowBadges ? ", .badge = \"New\"" : ""
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
                                if (auto previewBox = ui.Box({
                                    .widthMode = guikit::SizeMode::Stretch,
                                    .heightMode = guikit::SizeMode::Stretch,
                                    .padding = guikit::Padding::All(8.0f),
                                    .backgroundColor = previewTheme.palette.background,
                                    .borderColor = previewTheme.palette.borderSubtle,
                                    .borderWidth = 1.0f,
                                    .radius = 8.0f,
                                    .clipChildren = true,
                                    .theme = &previewTheme,
                                    .scale = previewScale,
                                    .debugBounds = previewBounds,
                                    .inspectMode = previewInspect
                                })) {
                                    if (auto contentScroll = ui.ScrollArea({.gap = 12.0f})) {
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
                                                            ui.InputField(maxTravel, {.label = "Max Travel", .suffix = "mm", .valueFormat = "%.1f"});
                                                        }
                                                        if (auto c = grid1.Cell6()) {
                                                            ui.InputField(maxVelocity, {.label = "Max Velocity", .suffix = "mm/s", .valueFormat = "%.1f"});
                                                        }
                                                        if (auto c = grid1.Cell12()) {
                                                            ui.Select(sensorChannel, {
                                                                "Channel A (Load Cell 50kN)",
                                                                "Channel B (Optical Extensometer)",
                                                                "Channel C (Laser Interferometer)"
                                                            }, {
                                                                .label = "Primary Sensor Channel",
                                                                .description = "Active physical transducer bus",
                                                                .layout = guikit::ElementLayout::Stacked
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
                                                            ui.InputField(rampDown, {.label = "Ramp Duration", .suffix = "ms", .valueFormat = "%.0f"});
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
                                                            ui.InputField(forceLimit, {.label = "Force Drop Limit", .suffix = "%", .valueFormat = "%.1f"});
                                                        }
                                                        if (auto c = grid3.Cell6()) {
                                                            ui.InputField(yieldRatio, {.label = "Yield Ratio", .suffix = "%", .valueFormat = "%.2f"});
                                                        }
                                                        if (auto c = grid3.Cell4()) {
                                                            ui.InputField(pGain, {.label = "P-Gain", .valueFormat = "%.2f"});
                                                        }
                                                        if (auto c = grid3.Cell4()) {
                                                            ui.InputField(iGain, {.label = "I-Gain", .valueFormat = "%.3f"});
                                                        }
                                                        if (auto c = grid3.Cell4()) {
                                                            ui.InputField(dGain, {.label = "D-Gain", .valueFormat = "%.3f"});
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
                                                    .layout = guikit::ElementLayout::Stacked
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

                                            const guikit::ElementLayout activeLayout = formMode ? guikit::ElementLayout::Stacked : guikit::ElementLayout::Top;

                                            if (auto fieldsGrid = ui.Grid(makeGrid())) {
                                                if (auto c1 = fieldsGrid.Cell3()) {
                                                    ui.InputField(speedDrop, {
                                                        .label = "Speed Drop dF/dt",
                                                        .description = "Automatic stop of testing on load drop",
                                                        .suffix = "%",
                                                        .valueFormat = "%.0f",
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
                                                        .valueFormat = "%.1f dB"
                                                    });
                                                }
                                                if (auto c2 = sliderGrid.Cell4()) {
                                                    ui.Slider(channelCount, 1, 8, {
                                                        .label = "ADC Active Channels",
                                                        .valueFormat = "%d ch",
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
                                                    if (auto c1 = g_row.Cell4()) { if (ui.Slider(r, 0.0f, 255.0f, {.label = "R", .valueFormat = "%.0f"}).changed) changed = true; }
                                                    if (auto c2 = g_row.Cell4()) { if (ui.Slider(g, 0.0f, 255.0f, {.label = "G", .valueFormat = "%.0f"}).changed) changed = true; }
                                                    if (auto c3 = g_row.Cell4()) { if (ui.Slider(b, 0.0f, 255.0f, {.label = "B", .valueFormat = "%.0f"}).changed) changed = true; }
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
