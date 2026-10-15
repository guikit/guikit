#include <SDL3/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include "guikit/GuiKit.hpp"
#include "guikit/backend/ThorCanvas.hpp"
#include "guikit/core/FrameMetrics.hpp"
#include "DemoContext.hpp"

namespace {

using namespace guikit::demo;

void DispatchInspector(guikit::Context& ui, NavCategory cat, DemoContext& ctx) {
    switch (cat) {
        // 1. ELEMENTS
        case NavCategory::ElemHeading:       RenderHeadingInspector(ui, ctx); break;
        case NavCategory::ElemButton:        RenderButtonInspector(ui, ctx); break;
        case NavCategory::ElemSlider:        RenderSliderInspector(ui, ctx); break;
        case NavCategory::ElemInputField:    RenderInputInspector(ui, ctx); break;
        case NavCategory::ElemSelect:        RenderSelectInspector(ui, ctx); break;
        case NavCategory::ElemSwitch:        RenderSwitchInspector(ui, ctx); break;
        case NavCategory::ElemCheckbox:      RenderCheckboxInspector(ui, ctx); break;

        // 2. PANELS & NAV
        case NavCategory::PanelCard:         RenderCardInspector(ui, ctx); break;
        case NavCategory::PanelTabBar:       RenderTabBarInspector(ui, ctx); break;
        case NavCategory::PanelSidebar:      RenderSidebarInspector(ui, ctx); break;

        // 3. CONTENT & MEDIA
        case NavCategory::ContentImage:      RenderImageInspector(ui, ctx); break;
        case NavCategory::ContentMarkdown:   RenderMarkdownInspector(ui, ctx); break;
        case NavCategory::ContentCodeEditor: RenderCodeEditorInspector(ui, ctx); break;

        // 4. 2D SCENES
        case NavCategory::SceneSchematic2D:  RenderSchematicInspector(ui, ctx); break;
        case NavCategory::SceneOsmMap:       RenderOsmMapInspector(ui, ctx); break;

        // 5. ASSEMBLY
        case NavCategory::AsmDashboard:      RenderDashboardInspector(ui, ctx); break;
        case NavCategory::AsmFormControls:   RenderFormControlsInspector(ui, ctx); break;
        case NavCategory::AsmFlexRibbon:     RenderFlexRibbonInspector(ui, ctx); break;
        case NavCategory::AsmDiagnostics:    RenderDiagnosticsInspector(ui, ctx); break;
        case NavCategory::AsmThemeStudio:    RenderThemeStudioInspector(ui, ctx); break;
    }
}

void DispatchSandboxStage(guikit::Context& ui, NavCategory cat, DemoContext& ctx) {
    switch (cat) {
        // 1. ELEMENTS
        case NavCategory::ElemHeading:       RenderHeadingStage(ui, ctx); break;
        case NavCategory::ElemButton:        RenderButtonStage(ui, ctx); break;
        case NavCategory::ElemSlider:        RenderSliderStage(ui, ctx); break;
        case NavCategory::ElemInputField:    RenderInputStage(ui, ctx); break;
        case NavCategory::ElemSelect:        RenderSelectStage(ui, ctx); break;
        case NavCategory::ElemSwitch:        RenderSwitchStage(ui, ctx); break;
        case NavCategory::ElemCheckbox:      RenderCheckboxStage(ui, ctx); break;

        // 2. PANELS & NAV
        case NavCategory::PanelCard:         RenderCardStage(ui, ctx); break;
        case NavCategory::PanelTabBar:       RenderTabBarStage(ui, ctx); break;
        case NavCategory::PanelSidebar:      RenderSidebarStage(ui, ctx); break;

        // 3. CONTENT & MEDIA
        case NavCategory::ContentImage:      RenderImageStage(ui, ctx); break;
        case NavCategory::ContentMarkdown:   RenderMarkdownStage(ui, ctx); break;
        case NavCategory::ContentCodeEditor: RenderCodeEditorStage(ui, ctx); break;

        // 4. 2D SCENES
        case NavCategory::SceneSchematic2D:  RenderSchematicStage(ui, ctx); break;
        case NavCategory::SceneOsmMap:       RenderOsmMapStage(ui, ctx); break;

        default: break;
    }
}

std::string DispatchCodeGenerator(NavCategory cat, const DemoContext& ctx) {
    switch (cat) {
        // 1. ELEMENTS
        case NavCategory::ElemHeading:       return GenerateHeadingCode(ctx);
        case NavCategory::ElemButton:        return GenerateButtonCode(ctx);
        case NavCategory::ElemSlider:        return GenerateSliderCode(ctx);
        case NavCategory::ElemInputField:    return GenerateInputCode(ctx);
        case NavCategory::ElemSelect:        return GenerateSelectCode(ctx);
        case NavCategory::ElemSwitch:        return GenerateSwitchCode(ctx);
        case NavCategory::ElemCheckbox:      return GenerateCheckboxCode(ctx);

        // 2. PANELS & NAV
        case NavCategory::PanelCard:         return GenerateCardCode(ctx);
        case NavCategory::PanelTabBar:       return GenerateTabBarCode(ctx);
        case NavCategory::PanelSidebar:      return GenerateSidebarCode(ctx);

        // 3. CONTENT & MEDIA
        case NavCategory::ContentImage:      return GenerateImageCode(ctx);
        case NavCategory::ContentMarkdown:   return GenerateMarkdownCode(ctx);
        case NavCategory::ContentCodeEditor: return GenerateCodeEditorCode(ctx);

        // 4. 2D SCENES
        case NavCategory::SceneSchematic2D:  return GenerateSchematicCode(ctx);
        case NavCategory::SceneOsmMap:       return GenerateOsmMapCode(ctx);

        default:                             return {};
    }
}

void DispatchAssemblyScreen(guikit::Context& ui, NavCategory cat, DemoContext& ctx) {
    switch (cat) {
        case NavCategory::AsmDashboard:    RenderDashboardScreen(ui, ctx); break;
        case NavCategory::AsmFormControls: RenderFormControlsScreen(ui, ctx); break;
        case NavCategory::AsmFlexRibbon:   RenderFlexRibbonScreen(ui, ctx); break;
        case NavCategory::AsmDiagnostics:  RenderDiagnosticsScreen(ui, ctx); break;
        case NavCategory::AsmThemeStudio:  RenderThemeStudioScreen(ui, ctx); break;
        default: break;
    }
}

} // namespace

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "Failed to initialize SDL3: " << SDL_GetError() << std::endl;
        return 1;
    }

    int winWidth = 1120;
    int winHeight = 740;
#ifdef __EMSCRIPTEN__
    {
        double cssW = 0.0, cssH = 0.0;
        if (emscripten_get_element_css_size("#canvas", &cssW, &cssH) == EMSCRIPTEN_RESULT_SUCCESS &&
            cssW >= 200.0 && cssH >= 200.0) {
            winWidth = static_cast<int>(cssW);
            winHeight = static_cast<int>(cssH);
        }
    }
    const SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE;
#else
    const SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#endif

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    const char* winTitle = "GuiKit Vector GUI Demo (C++20 Immediate Mode + ThorVG)";

    if (!SDL_CreateWindowAndRenderer(winTitle, winWidth, winHeight,
                                     windowFlags,
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

    bool showSidebar = true;
    bool running = true;
    bool vsyncEnabled = true;
    bool reactiveMode = true;

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

    NavCategory selectedCategory = NavCategory::ElemHeading;
    DemoContext demoCtx;
    guikit::FrameMetrics metrics;

    ui.SetTheme(demoCtx.shellTheme);
    ui.SetClearColor(demoCtx.shellTheme.palette.background);
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
            case SDL_EVENT_MOUSE_WHEEL: {
                const SDL_Keymod mod = SDL_GetModState();
                ui.SetKeyModifiers({
                    .ctrl  = (mod & SDL_KMOD_CTRL) != 0,
                    .shift = (mod & SDL_KMOD_SHIFT) != 0,
                    .alt   = (mod & SDL_KMOD_ALT) != 0
                });
                ui.OnMouseWheel(event.wheel.x, event.wheel.y);
                break;
            }
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
                    case SDLK_EQUALS:
                    case SDLK_PLUS:
                    case SDLK_KP_PLUS:   if (ctrl) key = guikit::Key::Plus; break;
                    case SDLK_MINUS:
                    case SDLK_KP_MINUS:  if (ctrl) key = guikit::Key::Minus; break;
                    case SDLK_0:
                    case SDLK_KP_0:      if (ctrl) key = guikit::Key::Zero; break;
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

    auto renderFrame = [&]() {
        const auto frameWorkStart = std::chrono::steady_clock::now();
        metrics.BeginFrame();

        const float shellScale = scaleValues[shellScaleIndex];
        const float previewScale = scaleValues[previewScaleIndex];

        ui.BeginFrame({static_cast<float>(winWidth), static_cast<float>(winHeight)}, 1.0f);
        {
            if (auto mainCol = ui.Column({.theme = &demoCtx.shellTheme, .scale = shellScale})) {
                // Engine HUD Toolbar
                if (auto tb = ui.Toolbar()) {
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

                    if (auto scaleBox = ui.Box({.fixedWidth = 92.0f})) {
                        if (ui.Select(shellScaleIndex, scaleLabels, {.size = guikit::UiSize::Sm}).changed) {
                            ui.InvalidateAll();
                        }
                    }

                    if (ui.Switch(demoCtx.shellLightMode, {
                        .textLeft = "Dark",
                        .textRight = "Light",
                        .symmetric = true,
                        .size = guikit::UiSize::Md
                    }).changed) {
                        demoCtx.shellTheme = demoCtx.shellLightMode
                            ? guikit::UiTheme::Light(demoCtx.shellTheme.density)
                            : guikit::UiTheme::Dark(demoCtx.shellTheme.density);
                        ui.SetTheme(demoCtx.shellTheme);
                        ui.SetClearColor(demoCtx.shellTheme.palette.background);
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
                    if (showSidebar) {
                        // 1. Level 1: 5-Section Catalog Rail
                        if (auto railSidebar = ui.Sidebar({
                            .side = guikit::SidebarSide::Left,
                            .minWidth = 156.0f
                        })) {
                            const NavCategory prevCat = selectedCategory;
                            if (auto menu = ui.SidebarMenu({.size = guikit::UiSize::Sm})) {
                                menu.Section("ELEMENTS");
                                menu.Item(selectedCategory, NavCategory::ElemHeading,       {.text = "Heading",      .icon = guikit::Icon::Edit()});
                                menu.Item(selectedCategory, NavCategory::ElemButton,        {.text = "Button",       .icon = guikit::Icon::Play()});
                                menu.Item(selectedCategory, NavCategory::ElemSlider,        {.text = "Slider",       .icon = guikit::Icon::Settings()});
                                menu.Item(selectedCategory, NavCategory::ElemInputField,    {.text = "Input",        .icon = guikit::Icon::Edit()});
                                menu.Item(selectedCategory, NavCategory::ElemSelect,        {.text = "Select",       .icon = guikit::Icon::Plus()});
                                menu.Item(selectedCategory, NavCategory::ElemSwitch,        {.text = "Switch",       .icon = guikit::Icon::Refresh()});
                                menu.Item(selectedCategory, NavCategory::ElemCheckbox,      {.text = "Checkbox",     .icon = guikit::Icon::Check()});

                                menu.Section("PANELS & NAV");
                                menu.Item(selectedCategory, NavCategory::PanelCard,         {.text = "Card",         .icon = guikit::Icon::Copy()});
                                menu.Item(selectedCategory, NavCategory::PanelTabBar,       {.text = "TabBar",       .icon = guikit::Icon::Folder()});
                                menu.Item(selectedCategory, NavCategory::PanelSidebar,      {.text = "Sidebar",      .icon = guikit::Icon::Folder()});

                                menu.Section("CONTENT & MEDIA");
                                menu.Item(selectedCategory, NavCategory::ContentImage,      {.text = "Image",        .icon = guikit::Icon::Eye()});
                                menu.Item(selectedCategory, NavCategory::ContentMarkdown,   {.text = "Markdown",     .icon = guikit::Icon::Edit()});
                                menu.Item(selectedCategory, NavCategory::ContentCodeEditor, {.text = "Code Editor",  .icon = guikit::Icon::Edit()});

                                menu.Section("2D SCENES");
                                menu.Item(selectedCategory, NavCategory::SceneSchematic2D,  {.text = "Schematic 2D", .icon = guikit::Icon::Search()});
                                menu.Item(selectedCategory, NavCategory::SceneOsmMap,       {.text = "OSM Map",      .icon = guikit::Icon::Search(), .badge = "GIS"});

                                menu.Section("ASSEMBLY");
                                menu.Item(selectedCategory, NavCategory::AsmDashboard,      {.text = "Dashboard",    .icon = guikit::Icon::Settings()});
                                menu.Item(selectedCategory, NavCategory::AsmFormControls,   {.text = "Form Demo",    .icon = guikit::Icon::Edit()});
                                menu.Item(selectedCategory, NavCategory::AsmFlexRibbon,     {.text = "Flex Ribbon",  .icon = guikit::Icon::Folder()});
                                menu.Item(selectedCategory, NavCategory::AsmDiagnostics,    {.text = "Diagnostics",  .icon = guikit::Icon::Eye()});
                                menu.Item(selectedCategory, NavCategory::AsmThemeStudio,    {.text = "Theme Studio", .icon = guikit::Icon::Refresh()});
                            }
                            if (selectedCategory != prevCat) {
                                ui.InvalidateAll();
                            }
                        }

                        // 2. Level 2: Inspector Panel
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
                            DispatchInspector(ui, selectedCategory, demoCtx);
                        }
                    }

                    // Content Canvas (Center / Right Workspace)
                    if (auto contentBox = ui.Box({
                        .widthMode = guikit::SizeMode::Stretch,
                        .heightMode = guikit::SizeMode::Stretch,
                        .padding = guikit::Padding::All(8.0f),
                        .backgroundColor = demoCtx.shellTheme.palette.background,
                        .borderColor = demoCtx.shellTheme.palette.borderSubtle,
                        .borderWidth = 1.0f,
                        .borderRadius = 8.0f,
                        .clipChildren = true
                    })) {
                        if (auto contentCol = ui.Column({
                            .gap = demoCtx.colGap,
                            .widthMode = guikit::SizeMode::Stretch,
                            .heightMode = guikit::SizeMode::Stretch
                        })) {
                            // Local Preview Toolbar
                            if (auto previewTb = ui.Toolbar()) {
                                ui.Text("PREVIEW", {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Muted});
                                previewTb.Separator();

                                if (auto pScaleBox = ui.Box({.fixedWidth = 92.0f})) {
                                    if (ui.Select(previewScaleIndex, scaleLabels, {.size = guikit::UiSize::Sm}).changed) {
                                        ui.InvalidateAll();
                                    }
                                }

                                if (ui.Switch(demoCtx.previewLightMode, {
                                    .textLeft = "Dark",
                                    .textRight = "Light",
                                    .symmetric = true,
                                    .size = guikit::UiSize::Md
                                }).changed) {
                                    demoCtx.previewDarkMode = !demoCtx.previewLightMode;
                                    const auto defTheme = demoCtx.previewDarkMode
                                        ? guikit::UiTheme::Dark(demoCtx.previewTheme.density)
                                        : guikit::UiTheme::Light(demoCtx.previewTheme.density);
                                    demoCtx.previewTheme.palette = defTheme.palette;
                                    demoCtx.previewTheme.variants = defTheme.variants;
                                    demoCtx.previewTheme.syntax = defTheme.syntax;
                                    demoCtx.previewTheme.mode = demoCtx.previewDarkMode ? guikit::ThemeMode::Dark : guikit::ThemeMode::Light;
                                    ui.InvalidateAll();
                                }

                                if (previewTb.ToolButton({
                                    .label = "Bounds",
                                    .icon = demoCtx.previewBounds ? guikit::Icon::Eye() : guikit::Icon::EyeOff(),
                                    .variant = demoCtx.previewBounds ? guikit::UiVariant::Info : guikit::UiVariant::Ghost,
                                    .active = demoCtx.previewBounds
                                })) {
                                    demoCtx.previewBounds = !demoCtx.previewBounds;
                                    ui.InvalidateAll();
                                }

                                if (previewTb.ToolButton({
                                    .label = "Inspect",
                                    .icon = guikit::Icon::Search(),
                                    .variant = demoCtx.previewInspect ? guikit::UiVariant::Warning : guikit::UiVariant::Ghost,
                                    .active = demoCtx.previewInspect
                                })) {
                                    demoCtx.previewInspect = !demoCtx.previewInspect;
                                    ui.InvalidateAll();
                                }
                            }

                            if (IsSandboxCategory(selectedCategory)) {
                                // SANDBOX MODE: Hero Stage (50%) + C++20 Code Generator (50%)
                                if (auto heroCard = ui.Card({
                                    .gap = 12.0f,
                                    .padding = guikit::Padding::All(14.0f),
                                    .borderRadius = 8.0f,
                                    .heightMode = guikit::SizeMode::Stretch,
                                    .theme = &demoCtx.previewTheme,
                                    .scale = previewScale,
                                    .debugBounds = demoCtx.previewBounds,
                                    .inspectMode = demoCtx.previewInspect
                                })) {
                                    const bool isFullStage = IsFullStageCategory(selectedCategory);
                                    if (auto stageBox = ui.Box({
                                        .widthMode = guikit::SizeMode::Stretch,
                                        .heightMode = guikit::SizeMode::Stretch,
                                        .padding = guikit::Padding::All(isFullStage ? 8.0f : 24.0f),
                                        .backgroundColor = demoCtx.previewTheme.palette.surfaceElevated,
                                        .borderColor = demoCtx.previewTheme.palette.borderSubtle,
                                        .borderWidth = 1.0f,
                                        .borderRadius = 8.0f
                                    })) {
                                        if (auto centerRow = ui.Row({
                                            .align = isFullStage ? guikit::Alignment::Stretch : guikit::Alignment::Center,
                                            .justify = guikit::Justify::Center,
                                            .heightMode = guikit::SizeMode::Stretch
                                        })) {
                                            DispatchSandboxStage(ui, selectedCategory, demoCtx);
                                        }
                                    }
                                }

                                if (auto codeCard = ui.Card({
                                    .gap = 8.0f,
                                    .padding = guikit::Padding::All(12.0f),
                                    .borderRadius = 8.0f,
                                    .heightMode = guikit::SizeMode::Stretch,
                                    .theme = &demoCtx.shellTheme
                                })) {
                                    std::string snippet = DispatchCodeGenerator(selectedCategory, demoCtx);
                                    ui.TextArea(snippet, {
                                        .autoHeight = false,
                                        .readOnly = true,
                                        .showLineNumbers = true,
                                        .showCopyButton = true,
                                        .monospace = true,
                                        .allowZoom = true,
                                        .syntax = guikit::SyntaxConfig::Cpp(),
                                        .heightMode = guikit::SizeMode::Stretch
                                    });
                                }
                            } else {
                                // ASSEMBLY MODE: Real-World Screens & Compositions
                                if (auto previewBox = ui.Box({
                                    .widthMode = guikit::SizeMode::Stretch,
                                    .heightMode = guikit::SizeMode::Stretch,
                                    .padding = guikit::Padding::All(8.0f),
                                    .backgroundColor = demoCtx.previewTheme.palette.background,
                                    .borderColor = demoCtx.previewTheme.palette.borderSubtle,
                                    .borderWidth = 1.0f,
                                    .borderRadius = 8.0f,
                                    .clipChildren = true,
                                    .theme = &demoCtx.previewTheme,
                                    .scale = previewScale,
                                    .debugBounds = demoCtx.previewBounds,
                                    .inspectMode = demoCtx.previewInspect
                                })) {
                                    if (auto contentScroll = ui.ScrollArea({.gap = 12.0f})) {
                                        DispatchAssemblyScreen(ui, selectedCategory, demoCtx);
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

#ifndef __EMSCRIPTEN__
        const auto frameEnd = std::chrono::steady_clock::now();
        const double frameElapsedMs = std::chrono::duration<double, std::milli>(frameEnd - frameWorkStart).count();
        const double targetFrameMs = 1000.0 / 60.0;
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
#else
        (void)frameWorkStart;
#endif

        metrics.EndFrame(60.0f);
    };

#ifdef __EMSCRIPTEN__
    auto lastRenderTime = std::chrono::steady_clock::now();
    auto emLoopStep = [&]() {
        if (!running) {
            emscripten_cancel_main_loop();
            return;
        }

        double cssW = 0.0, cssH = 0.0;
        if (emscripten_get_element_css_size("#canvas", &cssW, &cssH) == EMSCRIPTEN_RESULT_SUCCESS) {
            const int targetW = static_cast<int>(cssW);
            const int targetH = static_cast<int>(cssH);
            if (targetW >= 200 && targetH >= 200 && (targetW != winWidth || targetH != winHeight)) {
                SDL_SetWindowSize(window, targetW, targetH);
            }
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            handleEvent(event);
        }

        if (reactiveMode && !ui.NeedsRepaint()) {
            const int timeoutMs = ui.GetRecommendedSleepTimeoutMs();
            if (timeoutMs <= 0) {
                return;
            }
            const auto now = std::chrono::steady_clock::now();
            const double elapsedMs = std::chrono::duration<double, std::milli>(now - lastRenderTime).count();
            if (elapsedMs < static_cast<double>(timeoutMs)) {
                return;
            }
        }

        lastRenderTime = std::chrono::steady_clock::now();
        renderFrame();
    };

    emscripten_set_main_loop_arg(
        [](void* arg) {
            (*static_cast<decltype(emLoopStep)*>(arg))();
        },
        &emLoopStep,
        0,
        true
    );
#else
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

        renderFrame();
    }
#endif

    if (screenTexture) {
        SDL_DestroyTexture(screenTexture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
