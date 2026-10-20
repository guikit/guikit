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
#include "guikit/backend/SdlGlShaderEngine.hpp"
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
        case NavCategory::ElemLevelMeter:    RenderLevelMeterInspector(ui, ctx); break;
        case NavCategory::ElemKnob:          RenderKnobInspector(ui, ctx); break;
        case NavCategory::ElemSpinBox:       RenderSpinBoxInspector(ui, ctx); break;
        case NavCategory::ElemInputField:    RenderInputInspector(ui, ctx); break;
        case NavCategory::ElemTagInput:      RenderTagInputInspector(ui, ctx); break;
        case NavCategory::ElemSelect:        RenderSelectInspector(ui, ctx); break;
        case NavCategory::ElemSegmented:     RenderSegmentedInspector(ui, ctx); break;
        case NavCategory::ElemColorPicker:   RenderColorPickerInspector(ui, ctx); break;
        case NavCategory::ElemDatePicker:    RenderDatePickerInspector(ui, ctx); break;
        case NavCategory::ElemSwitch:        RenderSwitchInspector(ui, ctx); break;
        case NavCategory::ElemCheckbox:      RenderCheckboxInspector(ui, ctx); break;
        case NavCategory::ElemRadio:         RenderRadioInspector(ui, ctx); break;
        case NavCategory::ElemImage:         RenderImageInspector(ui, ctx); break;

        // 2. PANELS & NAV
        case NavCategory::PanelCard:         RenderCardInspector(ui, ctx); break;
        case NavCategory::PanelSplitView:    RenderSplitViewInspector(ui, ctx); break;
        case NavCategory::PanelTabBar:       RenderTabBarInspector(ui, ctx); break;
        case NavCategory::PanelNavigation:   RenderNavigationInspector(ui, ctx); break;
        case NavCategory::PanelToolbar:      RenderToolbarInspector(ui, ctx); break;
        case NavCategory::PanelSidebar:      RenderSidebarInspector(ui, ctx); break;
        case NavCategory::PanelTreeView:     RenderTreeViewInspector(ui, ctx); break;
        case NavCategory::PanelTable:        RenderTableInspector(ui, ctx); break;
        case NavCategory::PanelOverlays:     RenderOverlaysInspector(ui, ctx); break;

        // 3. CONTENT & MEDIA
        case NavCategory::ContentMarkdown:   RenderMarkdownInspector(ui, ctx); break;
        case NavCategory::ContentCodeEditor: RenderCodeEditorInspector(ui, ctx); break;

        // 4. 2D SCENES
        case NavCategory::SceneSchematic2D:   RenderSchematicInspector(ui, ctx); break;
        case NavCategory::SceneOsmMap:        RenderOsmMapInspector(ui, ctx); break;
        case NavCategory::SceneSignalView:    RenderSignalInspector(ui, ctx); break;
        case NavCategory::SceneSonarView:     RenderSonarInspector(ui, ctx); break;
        case NavCategory::SceneWaterfallView: RenderWaterfallInspector(ui, ctx); break;
        case NavCategory::SceneImageView:     RenderImageViewInspector(ui, ctx); break;
        case NavCategory::SceneAttitudeView:  RenderAttitudeInspector(ui, ctx); break;
        case NavCategory::SceneCarDashboard:  RenderCarDashboardInspector(ui, ctx); break;
        case NavCategory::SceneBpmnEditor:    RenderBpmnInspector(ui, ctx); break;
    }
}

void DispatchSandboxStage(guikit::Context& ui, NavCategory cat, DemoContext& ctx) {
    switch (cat) {
        // 1. ELEMENTS
        case NavCategory::ElemHeading:       RenderHeadingStage(ui, ctx); break;
        case NavCategory::ElemButton:        RenderButtonStage(ui, ctx); break;
        case NavCategory::ElemSlider:        RenderSliderStage(ui, ctx); break;
        case NavCategory::ElemLevelMeter:    RenderLevelMeterStage(ui, ctx); break;
        case NavCategory::ElemKnob:          RenderKnobStage(ui, ctx); break;
        case NavCategory::ElemSpinBox:       RenderSpinBoxStage(ui, ctx); break;
        case NavCategory::ElemInputField:    RenderInputStage(ui, ctx); break;
        case NavCategory::ElemTagInput:      RenderTagInputStage(ui, ctx); break;
        case NavCategory::ElemSelect:        RenderSelectStage(ui, ctx); break;
        case NavCategory::ElemSegmented:     RenderSegmentedStage(ui, ctx); break;
        case NavCategory::ElemColorPicker:   RenderColorPickerStage(ui, ctx); break;
        case NavCategory::ElemDatePicker:    RenderDatePickerStage(ui, ctx); break;
        case NavCategory::ElemSwitch:        RenderSwitchStage(ui, ctx); break;
        case NavCategory::ElemCheckbox:      RenderCheckboxStage(ui, ctx); break;
        case NavCategory::ElemRadio:         RenderRadioStage(ui, ctx); break;
        case NavCategory::ElemImage:         RenderImageStage(ui, ctx); break;

        // 2. PANELS & NAV
        case NavCategory::PanelCard:         RenderCardStage(ui, ctx); break;
        case NavCategory::PanelSplitView:    RenderSplitViewStage(ui, ctx); break;
        case NavCategory::PanelTabBar:       RenderTabBarStage(ui, ctx); break;
        case NavCategory::PanelNavigation:   RenderNavigationStage(ui, ctx); break;
        case NavCategory::PanelToolbar:      RenderToolbarStage(ui, ctx); break;
        case NavCategory::PanelSidebar:      RenderSidebarStage(ui, ctx); break;
        case NavCategory::PanelTreeView:     RenderTreeViewStage(ui, ctx); break;
        case NavCategory::PanelTable:        RenderTableStage(ui, ctx); break;
        case NavCategory::PanelOverlays:     RenderOverlaysStage(ui, ctx); break;

        // 3. CONTENT & MEDIA
        case NavCategory::ContentMarkdown:   RenderMarkdownStage(ui, ctx); break;
        case NavCategory::ContentCodeEditor: RenderCodeEditorStage(ui, ctx); break;

        // 4. 2D SCENES
        case NavCategory::SceneSchematic2D:   RenderSchematicStage(ui, ctx); break;
        case NavCategory::SceneOsmMap:        RenderOsmMapStage(ui, ctx); break;
        case NavCategory::SceneSignalView:    RenderSignalStage(ui, ctx); break;
        case NavCategory::SceneSonarView:     RenderSonarStage(ui, ctx); break;
        case NavCategory::SceneWaterfallView: RenderWaterfallStage(ui, ctx); break;
        case NavCategory::SceneImageView:     RenderImageViewStage(ui, ctx); break;
        case NavCategory::SceneAttitudeView:  RenderAttitudeStage(ui, ctx); break;
        case NavCategory::SceneCarDashboard:  RenderCarDashboardStage(ui, ctx); break;
        case NavCategory::SceneBpmnEditor:    RenderBpmnStage(ui, ctx); break;

        default: break;
    }
}

std::string DispatchCodeGenerator(NavCategory cat, const DemoContext& ctx) {
    switch (cat) {
        // 1. ELEMENTS
        case NavCategory::ElemHeading:       return GenerateHeadingCode(ctx);
        case NavCategory::ElemButton:        return GenerateButtonCode(ctx);
        case NavCategory::ElemSlider:        return GenerateSliderCode(ctx);
        case NavCategory::ElemLevelMeter:    return GenerateLevelMeterCode(ctx);
        case NavCategory::ElemKnob:          return GenerateKnobCode(ctx);
        case NavCategory::ElemSpinBox:       return GenerateSpinBoxCode(ctx);
        case NavCategory::ElemInputField:    return GenerateInputCode(ctx);
        case NavCategory::ElemTagInput:      return GenerateTagInputCode(ctx);
        case NavCategory::ElemSelect:        return GenerateSelectCode(ctx);
        case NavCategory::ElemSegmented:     return GenerateSegmentedCode(ctx);
        case NavCategory::ElemColorPicker:   return GenerateColorPickerCode(ctx);
        case NavCategory::ElemDatePicker:    return GenerateDatePickerCode(ctx);
        case NavCategory::ElemSwitch:        return GenerateSwitchCode(ctx);
        case NavCategory::ElemCheckbox:      return GenerateCheckboxCode(ctx);
        case NavCategory::ElemRadio:         return GenerateRadioCode(ctx);
        case NavCategory::ElemImage:         return GenerateImageCode(ctx);

        // 2. PANELS & NAV
        case NavCategory::PanelCard:         return GenerateCardCode(ctx);
        case NavCategory::PanelSplitView:    return GenerateSplitViewCode(ctx);
        case NavCategory::PanelTabBar:       return GenerateTabBarCode(ctx);
        case NavCategory::PanelNavigation:   return GenerateNavigationCode(ctx);
        case NavCategory::PanelToolbar:      return GenerateToolbarCode(ctx);
        case NavCategory::PanelSidebar:      return GenerateSidebarCode(ctx);
        case NavCategory::PanelTreeView:     return GenerateTreeViewCode(ctx);
        case NavCategory::PanelTable:        return GenerateTableCode(ctx);
        case NavCategory::PanelOverlays:     return GenerateOverlaysCode(ctx);

        // 3. CONTENT & MEDIA
        case NavCategory::ContentMarkdown:   return GenerateMarkdownCode(ctx);
        case NavCategory::ContentCodeEditor: return GenerateCodeEditorCode(ctx);

        // 4. 2D SCENES
        case NavCategory::SceneSchematic2D:   return GenerateSchematicCode(ctx);
        case NavCategory::SceneOsmMap:        return GenerateOsmMapCode(ctx);
        case NavCategory::SceneSignalView:    return GenerateSignalCode(ctx);
        case NavCategory::SceneSonarView:     return GenerateSonarCode(ctx);
        case NavCategory::SceneWaterfallView: return GenerateWaterfallCode(ctx);
        case NavCategory::SceneImageView:     return GenerateImageViewCode(ctx);
        case NavCategory::SceneAttitudeView:  return GenerateAttitudeCode(ctx);
        case NavCategory::SceneCarDashboard:  return GenerateCarDashboardCode(ctx);
        case NavCategory::SceneBpmnEditor:    return GenerateBpmnCode(ctx);

        default:                             return {};
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

    int winWidth = 1380;
    int winHeight = 800;
#ifdef __EMSCRIPTEN__
    auto getCanvasPhysicalSize = [](int& outW, int& outH) -> bool {
        double cssW = 0.0, cssH = 0.0;
        if (emscripten_get_element_css_size("#canvas", &cssW, &cssH) == EMSCRIPTEN_RESULT_SUCCESS &&
            cssW >= 200.0 && cssH >= 200.0) {
            double dpr = emscripten_get_device_pixel_ratio();
            if (dpr <= 0.0) {
                dpr = 1.0;
            }
            outW = static_cast<int>(std::lround(cssW * dpr));
            outH = static_cast<int>(std::lround(cssH * dpr));
            return true;
        }
        return false;
    };
    getCanvasPhysicalSize(winWidth, winHeight);
    const SDL_WindowFlags windowFlags = 0;
#else
    const SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#endif

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    const char* winTitle = "GuiKit Vector GUI Demo (C++20 Immediate Mode + ThorVG)";

#ifndef __EMSCRIPTEN__
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
#endif
    if (!SDL_CreateWindowAndRenderer(winTitle, winWidth, winHeight,
                                     windowFlags,
                                     &window, &renderer)) {
#ifndef __EMSCRIPTEN__
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, nullptr);
        if (!SDL_CreateWindowAndRenderer(winTitle, winWidth, winHeight,
                                         windowFlags,
                                         &window, &renderer)) {
            std::cerr << "Failed to create SDL window & renderer: " << SDL_GetError() << std::endl;
            SDL_Quit();
            return 1;
        }
#else
        std::cerr << "Failed to create SDL window & renderer: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
#endif
    }

    SDL_SetWindowMinimumSize(window, 320, 240);
#ifdef __EMSCRIPTEN__
    SDL_SetWindowSize(window, winWidth, winHeight);
#else
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
#endif
    SDL_SetRenderVSync(renderer, 1);
    SDL_StartTextInput(window);

    guikit::Context ui;
    guikit::ThorCanvas canvas;
    guikit::SdlGlShaderEngine gpuShaderEngine;
    bool gpuShadersEnabled = gpuShaderEngine.Initialize(renderer);
    canvas.SetGpuShadersEnabled(gpuShadersEnabled);

    int bufWidth = winWidth;
    int bufHeight = winHeight;
    std::vector<uint32_t> pixelBuffer(static_cast<size_t>(bufWidth) * static_cast<size_t>(bufHeight), 0);
    canvas.SetTarget(pixelBuffer.data(), bufWidth, bufHeight, bufWidth);

    SDL_Texture* screenTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        bufWidth,
        bufHeight
    );
    SDL_SetTextureScaleMode(screenTexture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(screenTexture, SDL_BLENDMODE_BLEND_PREMULTIPLIED);

    guikit::SidebarMode navSidebarMode = guikit::SidebarMode::Expanded;
    guikit::SidebarMode inspectorSidebarMode = guikit::SidebarMode::Expanded;
    bool running = true;
    bool vsyncEnabled = true;
    bool reactiveMode = true;
    bool inRenderFrame = false;
    bool isLiveResizeFrame = false;
    auto lastLiveResizeTime = std::chrono::steady_clock::now();
    std::string cachedCodeSnippet;
    NavCategory cachedCodeCategory = NavCategory::ElemHeading;

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
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                if (event.window.data1 > 0 && event.window.data2 > 0) {
                    winWidth = event.window.data1;
                    winHeight = event.window.data2;
                    ui.InvalidateAll();
                    ui.RequestRepaint(3);
                }
                break;
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
                    case SDLK_PAGEUP:    key = guikit::Key::PageUp; break;
                    case SDLK_PAGEDOWN:  key = guikit::Key::PageDown; break;
                    case SDLK_A:         if (ctrl) key = guikit::Key::A; break;
                    case SDLK_C:         if (ctrl) key = guikit::Key::C; break;
                    case SDLK_D:         if (ctrl) key = guikit::Key::D; break;
                    case SDLK_F:         if (ctrl) key = guikit::Key::F; break;
                    case SDLK_G:         if (ctrl) key = guikit::Key::G; break;
                    case SDLK_H:         if (ctrl) key = guikit::Key::H; break;
                    case SDLK_K:         if (ctrl) key = guikit::Key::K; break;
                    case SDLK_V:         if (ctrl) key = guikit::Key::V; break;
                    case SDLK_X:         if (ctrl) key = guikit::Key::X; break;
                    case SDLK_Y:         if (ctrl) key = guikit::Key::Y; break;
                    case SDLK_Z:         if (ctrl) key = guikit::Key::Z; break;
                    case SDLK_SLASH:
                    case SDLK_KP_DIVIDE: if (ctrl) key = guikit::Key::Slash; break;
                    case SDLK_EQUALS:
                    case SDLK_PLUS:
                    case SDLK_KP_PLUS:   if (ctrl) key = guikit::Key::Plus; break;
                    case SDLK_MINUS:
                    case SDLK_KP_MINUS:  if (ctrl) key = guikit::Key::Minus; break;
                    case SDLK_0:
                    case SDLK_KP_0:      if (ctrl) key = guikit::Key::Zero; break;
                    case SDLK_F3:        key = guikit::Key::F3; break;
                    case SDLK_F5:        key = guikit::Key::F5; break;
                    case SDLK_F9:        key = guikit::Key::F9; break;
                    case SDLK_F10:       key = guikit::Key::F10; break;
                    case SDLK_F11:       key = guikit::Key::F11; break;
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
        if (inRenderFrame || winWidth <= 0 || winHeight <= 0) {
            return;
        }
        inRenderFrame = true;
        struct FrameGuard {
            bool& flag;
            ~FrameGuard() { flag = false; }
        } frameGuard{inRenderFrame};

        if (winWidth != bufWidth || winHeight != bufHeight) {
            bufWidth = winWidth;
            bufHeight = winHeight;
            pixelBuffer.assign(static_cast<size_t>(bufWidth) * static_cast<size_t>(bufHeight), 0);
            canvas.SetTarget(pixelBuffer.data(), bufWidth, bufHeight, bufWidth);
            if (screenTexture) {
                SDL_DestroyTexture(screenTexture);
            }
            screenTexture = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_ABGR8888,
                SDL_TEXTUREACCESS_STREAMING,
                bufWidth,
                bufHeight
            );
            SDL_SetTextureScaleMode(screenTexture, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(screenTexture, SDL_BLENDMODE_BLEND_PREMULTIPLIED);
            ui.InvalidateAll();
        }

        const auto frameWorkStart = std::chrono::steady_clock::now();
        metrics.BeginFrame();

        const float shellScale = scaleValues[shellScaleIndex];
        const float previewScale = scaleValues[previewScaleIndex];

        ui.BeginFrame({static_cast<float>(winWidth), static_cast<float>(winHeight)}, 1.0f);
        {
            if (auto mainCol = ui.Column({.theme = &demoCtx.shellTheme, .scale = shellScale})) {
                // Engine HUD Toolbar
                if (auto tb = ui.Toolbar()) {
                    const guikit::UiVariant navVariant = (navSidebarMode == guikit::SidebarMode::Expanded)
                        ? guikit::UiVariant::Primary
                        : (navSidebarMode == guikit::SidebarMode::Compact)
                            ? guikit::UiVariant::Info
                            : guikit::UiVariant::Ghost;
                    const std::string_view navTooltip = (navSidebarMode == guikit::SidebarMode::Expanded)
                        ? "Navigation Sidebar: Expanded (Click for Compact Icons, Right-click to Collapse)"
                        : (navSidebarMode == guikit::SidebarMode::Compact)
                            ? "Navigation Sidebar: Compact Icons (Click to Collapse, Right-click to Expand)"
                            : "Navigation Sidebar: Collapsed (Click to Expand)";
                    const auto navBtn = tb.ToolButton({
                        .label = "Nav",
                        .tooltip = navTooltip,
                        .icon = guikit::Icon::Folder(),
                        .variant = navVariant,
                        .active = (navSidebarMode != guikit::SidebarMode::Collapsed)
                    });
                    if (navBtn.clicked) {
                        if (navSidebarMode == guikit::SidebarMode::Expanded) {
                            navSidebarMode = guikit::SidebarMode::Compact;
                        } else if (navSidebarMode == guikit::SidebarMode::Compact) {
                            navSidebarMode = guikit::SidebarMode::Collapsed;
                        } else {
                            navSidebarMode = guikit::SidebarMode::Expanded;
                        }
                        ui.InvalidateAll();
                    } else if (navBtn.rightClicked) {
                        navSidebarMode = (navSidebarMode == guikit::SidebarMode::Collapsed)
                            ? guikit::SidebarMode::Expanded
                            : guikit::SidebarMode::Collapsed;
                        ui.InvalidateAll();
                    }

                    bool inspectorVisible = (inspectorSidebarMode != guikit::SidebarMode::Collapsed);
                    if (tb.Toggle(inspectorVisible, {
                        .label = "Inspector",
                        .tooltip = inspectorVisible
                            ? "Inspector Sidebar: Expanded (Click to Collapse)"
                            : "Inspector Sidebar: Collapsed (Click to Expand)",
                        .icon = guikit::Icon::Settings(),
                        .variant = guikit::UiVariant::Primary
                    }).changed) {
                        inspectorSidebarMode = inspectorVisible
                            ? guikit::SidebarMode::Expanded
                            : guikit::SidebarMode::Collapsed;
                        ui.InvalidateAll();
                    }

                    tb.Separator();

                    if (ui.Size(guikit::UiSize::Sm).Select(shellScaleIndex, scaleLabels, {.width = 92.0f}).changed) {
                        ui.InvalidateAll();
                    }

                    if (ui.Size(guikit::UiSize::Md).Switch(demoCtx.shellLightMode, {
                        .textLeft = "Dark",
                        .textRight = "Light",
                        .symmetric = true
                    }).changed) {
                        demoCtx.shellTheme = demoCtx.shellLightMode
                            ? guikit::UiTheme::Light(demoCtx.shellTheme.density)
                            : guikit::UiTheme::Dark(demoCtx.shellTheme.density);
                        ui.SetTheme(demoCtx.shellTheme);
                        ui.SetClearColor(demoCtx.shellTheme.palette.background);
                        ui.InvalidateAll();
                    }

                    if (auto debugGroup = ui.Row({.equalWidths = true})) {
                        bool boundsActive = ui.IsDebugBounds();
                        if (tb.Toggle(boundsActive, {
                            .label = "Bounds",
                            .icon = boundsActive ? guikit::Icon::Eye() : guikit::Icon::EyeOff(),
                            .variant = guikit::UiVariant::Info
                        }).changed) {
                            ui.SetDebugBounds(boundsActive);
                        }

                        bool inspectActive = ui.IsInspectMode();
                        if (tb.Toggle(inspectActive, {
                            .label = "Inspect",
                            .icon = guikit::Icon::Search(),
                            .variant = guikit::UiVariant::Warning
                        }).changed) {
                            ui.SetInspectMode(inspectActive);
                        }
                    }

                    tb.Separator();

                    if (auto engineGroup = ui.Row({.equalWidths = true})) {
                        if (tb.Toggle(gpuShadersEnabled, {
                            .label = "GPU",
                            .tooltip = "Toggle OpenGL ES3 Shader Acceleration",
                            .icon = guikit::Icon::Flash(),
                            .variant = guikit::UiVariant::Success,
                            .disabled = !gpuShaderEngine.IsAvailable()
                        }).changed) {
                            canvas.SetGpuShadersEnabled(gpuShadersEnabled && gpuShaderEngine.IsAvailable());
                            ui.InvalidateAll();
                            ui.RequestRepaint(2);
                        }

                        if (tb.Toggle(vsyncEnabled, {
                            .label = "VSync",
                            .tooltip = "Toggle Vertical Sync",
                            .variant = guikit::UiVariant::Info
                        }).changed) {
                            SDL_SetRenderVSync(renderer, vsyncEnabled ? 1 : 0);
                        }

                        if (tb.Toggle(reactiveMode, {
                            .label = "React",
                            .tooltip = "Toggle Event-Driven Reactive Sleep Loop",
                            .variant = guikit::UiVariant::Primary
                        }).changed) {
                            if (!reactiveMode) {
                                ui.InvalidateAll();
                            }
                        }

                        bool dirtyActive = ui.IsDirtyRectEnabled();
                        if (tb.Toggle(dirtyActive, {
                            .label = "Dirty",
                            .tooltip = "Toggle Disjoint Dirty-Rect Partial Rasterization",
                            .variant = guikit::UiVariant::Success
                        }).changed) {
                            ui.SetDirtyRectEnabled(dirtyActive);
                            if (!dirtyActive) {
                                ui.InvalidateAll();
                            }
                        }

                        bool liveResizeActive = ui.IsLiveResizeEnabled();
                        if (tb.Toggle(liveResizeActive, {
                            .label = "Live",
                            .tooltip = liveResizeActive
                                ? "Live Window Resize: Smooth 60 FPS reflow while dragging window border"
                                : "Deferred Window Resize: Reflow after window resize step completes",
                            .variant = guikit::UiVariant::Info
                        }).changed) {
                            ui.SetLiveResize(liveResizeActive);
                        }
                    }

                    if (tb.Action({
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
                    // 1. Level 1: 5-Section Catalog Rail
                    if (auto railSidebar = ui.Sidebar(navSidebarMode, {
                        .side = guikit::SidebarSide::Left,
                        .maxRatio = 0.35f,
                        .resizable = true,
                        .allowCompact = true,
                        .allowCollapse = true
                    })) {
                        const NavCategory prevCat = selectedCategory;
                        if (auto menu = ui.Size(guikit::UiSize::Sm).SidebarMenu()) {
                            menu.Section("ELEMENTS");
                            menu.Item(selectedCategory, NavCategory::ElemHeading,       {.text = "Heading",      .icon = guikit::Icon::Edit()});
                            menu.Item(selectedCategory, NavCategory::ElemButton,        {.text = "Button",       .icon = guikit::Icon::Play()});
                            menu.Item(selectedCategory, NavCategory::ElemSlider,        {.text = "Slider",       .icon = guikit::Icon::Settings()});
                            menu.Item(selectedCategory, NavCategory::ElemLevelMeter,    {.text = "LevelMeter",   .icon = guikit::Icon::Play()});
                            menu.Item(selectedCategory, NavCategory::ElemKnob,          {.text = "Knob",         .icon = guikit::Icon::Refresh()});
                            menu.Item(selectedCategory, NavCategory::ElemSpinBox,       {.text = "SpinBox",      .icon = guikit::Icon::Plus()});
                            menu.Item(selectedCategory, NavCategory::ElemInputField,    {.text = "Input",        .icon = guikit::Icon::Edit()});
                            menu.Item(selectedCategory, NavCategory::ElemTagInput,      {.text = "TagInput",     .icon = guikit::Icon::Edit()});
                            menu.Item(selectedCategory, NavCategory::ElemSelect,        {.text = "Select",       .icon = guikit::Icon::Plus()});
                            menu.Item(selectedCategory, NavCategory::ElemSegmented,     {.text = "Segmented",    .icon = guikit::Icon::Settings()});
                            menu.Item(selectedCategory, NavCategory::ElemColorPicker,   {.text = "ColorPicker",  .icon = guikit::Icon::Eye()});
                            menu.Item(selectedCategory, NavCategory::ElemDatePicker,    {.text = "DatePicker",   .icon = guikit::Icon::Edit()});
                            menu.Item(selectedCategory, NavCategory::ElemSwitch,        {.text = "Switch",       .icon = guikit::Icon::Refresh()});
                            menu.Item(selectedCategory, NavCategory::ElemCheckbox,      {.text = "Checkbox",     .icon = guikit::Icon::Check()});
                            menu.Item(selectedCategory, NavCategory::ElemRadio,         {.text = "Radio",        .icon = guikit::Icon::Check()});
                            menu.Item(selectedCategory, NavCategory::ElemImage,         {.text = "Image",        .icon = guikit::Icon::Eye()});

                            menu.Section("PANELS & NAV");
                            menu.Item(selectedCategory, NavCategory::PanelCard,         {.text = "Card",         .icon = guikit::Icon::Copy()});
                            menu.Item(selectedCategory, NavCategory::PanelSplitView,    {.text = "SplitView",    .icon = guikit::Icon::Copy()});
                            menu.Item(selectedCategory, NavCategory::PanelTabBar,       {.text = "TabBar",       .icon = guikit::Icon::Folder()});
                            menu.Item(selectedCategory, NavCategory::PanelNavigation,   {.text = "Navigation",   .icon = guikit::Icon::Folder()});
                            menu.Item(selectedCategory, NavCategory::PanelToolbar,      {.text = "Toolbar",      .icon = guikit::Icon::Settings()});
                            menu.Item(selectedCategory, NavCategory::PanelSidebar,      {.text = "Sidebar",      .icon = guikit::Icon::Folder()});
                            menu.Item(selectedCategory, NavCategory::PanelTreeView,     {.text = "TreeView",     .icon = guikit::Icon::Folder()});
                            menu.Item(selectedCategory, NavCategory::PanelTable,        {.text = "Table / Grid", .icon = guikit::Icon::Copy(),   .badge = "100k"});
                            menu.Item(selectedCategory, NavCategory::PanelOverlays,     {.text = "Overlays",     .icon = guikit::Icon::Flash(),  .badge = "New"});

                            menu.Section("CONTENT & MEDIA");
                            menu.Item(selectedCategory, NavCategory::ContentMarkdown,   {.text = "Markdown",     .icon = guikit::Icon::Edit()});
                            menu.Item(selectedCategory, NavCategory::ContentCodeEditor, {.text = "Code Editor",  .icon = guikit::Icon::Edit()});

                            menu.Section("2D SCENES");
                            menu.Item(selectedCategory, NavCategory::SceneSchematic2D,   {.text = "Schematic 2D", .icon = guikit::Icon::Search()});
                            menu.Item(selectedCategory, NavCategory::SceneOsmMap,        {.text = "GIS",          .icon = guikit::Icon::Search(), .badge = "ENC"});
                            menu.Item(selectedCategory, NavCategory::SceneSignalView,    {.text = "Signal View",  .icon = guikit::Icon::Play(),   .badge = "DSP"});
                            menu.Item(selectedCategory, NavCategory::SceneSonarView,     {.text = "Sonar Radar",  .icon = guikit::Icon::Search(), .badge = "DDS"});
                            menu.Item(selectedCategory, NavCategory::SceneWaterfallView, {.text = "Waterfall",    .icon = guikit::Icon::Play(),   .badge = "DOA"});
                            menu.Item(selectedCategory, NavCategory::SceneImageView,     {.text = "Image View",   .icon = guikit::Icon::Eye(),    .badge = "16b/RGB"});
                            menu.Item(selectedCategory, NavCategory::SceneAttitudeView,  {.text = "Attitude PFD", .icon = guikit::Icon::Play(),   .badge = "ADI"});
                            menu.Item(selectedCategory, NavCategory::SceneCarDashboard,  {.text = "Car Cluster",  .icon = guikit::Icon::Play(),   .badge = "AUTO"});
                            menu.Item(selectedCategory, NavCategory::SceneBpmnEditor,    {.text = "BPMN Modeler", .icon = guikit::Icon::Edit(),   .badge = "2.0"});
                        }
                        if (selectedCategory != prevCat) {
                            ui.InvalidateAll();
                        }
                    }

                    // 2. Level 2: Inspector Panel (Resizable & Collapsible)
                    if (auto inspectorSidebar = ui.Sidebar(inspectorSidebarMode, {
                        .side = guikit::SidebarSide::Left,
                        .width = 320.0f,
                        .maxRatio = 0.45f,
                        .resizable = true,
                        .allowCollapse = true,
                        .gap = 8.0f,
                        .padding = guikit::Padding::All(12.0f)
                    })) {
                        ui.Size(guikit::UiSize::Xs).Heading({
                            .label = "INSPECTOR",
                            .divider = true
                        });

                        if (IsElementCategory(selectedCategory)) {
                            if (auto elemAcc = ui.Size(guikit::UiSize::Sm).Accordion({
                                .label = "ELEMENT",
                                .defaultOpen = false
                            })) {
                                demoCtx.RenderElementInspector(ui, selectedCategory);
                            }

                            if (selectedCategory == NavCategory::ElemHeading) {
                                bool ctrlOpen = false;
                                if (auto ctrlAcc = ui.Size(guikit::UiSize::Sm).Accordion(ctrlOpen, {
                                    .label = "CONTROL",
                                    .badge = {"N/A", guikit::StateColor::Muted()},
                                    .disabled = true
                                })) {
                                }
                            } else {
                                if (auto ctrlAcc = ui.Size(guikit::UiSize::Sm).Accordion({
                                    .label = "CONTROL",
                                    .defaultOpen = false
                                })) {
                                    demoCtx.RenderControlInspector(ui, selectedCategory);
                                }
                            }

                            if (auto specAcc = ui.Size(guikit::UiSize::Sm).Accordion({
                                .label = "SPECIFIC",
                                .defaultOpen = true
                            })) {
                                DispatchInspector(ui, selectedCategory, demoCtx);
                            }
                        } else {
                            DispatchInspector(ui, selectedCategory, demoCtx);
                        }
                    }

                    // Content Canvas (Center / Right Workspace)
                    if (auto contentCol = ui.Column({
                        .gap = demoCtx.colGap,
                        .widthMode = guikit::SizeMode::Stretch,
                        .heightMode = guikit::SizeMode::Stretch
                    })) {
                        // Local Preview Toolbar
                        if (auto previewTb = ui.Toolbar()) {
                            ui.Text("PREVIEW", {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Muted});
                            previewTb.Separator();

                            if (ui.Size(guikit::UiSize::Sm).Select(previewScaleIndex, scaleLabels, {.width = 92.0f}).changed) {
                                ui.InvalidateAll();
                            }

                            if (ui.Size(guikit::UiSize::Md).Switch(demoCtx.previewLightMode, {
                                .textLeft = "Dark",
                                .textRight = "Light",
                                .symmetric = true
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

                            previewTb.Toggle(demoCtx.previewBounds, {
                                .label = "Bounds",
                                .icon = demoCtx.previewBounds ? guikit::Icon::Eye() : guikit::Icon::EyeOff(),
                                .variant = guikit::UiVariant::Info
                            });

                            previewTb.Toggle(demoCtx.previewInspect, {
                                .label = "Inspect",
                                .icon = guikit::Icon::Search(),
                                .variant = guikit::UiVariant::Warning
                            });

                            if (IsSandboxCategory(selectedCategory)) {
                                if (previewTb.Toggle(demoCtx.previewShowCode, {
                                    .label = "Code",
                                    .icon = demoCtx.previewShowCode ? guikit::Icon::Eye() : guikit::Icon::EyeOff(),
                                    .variant = guikit::UiVariant::Primary
                                }).changed) {
                                    ui.RequestRepaint(3);
                                }
                            }
                        }

                        if (IsSandboxCategory(selectedCategory)) {
                            // SANDBOX MODE: Hero Stage + optional C++20 Code Generator
                            const bool isFullStage = IsFullStageCategory(selectedCategory);
                            if (auto stageBox = ui.Box({
                                .widthMode = guikit::SizeMode::Stretch,
                                .heightMode = guikit::SizeMode::Stretch,
                                .padding = guikit::Padding::All(isFullStage ? 8.0f : 24.0f),
                                .background = {demoCtx.previewTheme.palette.surface},
                                .border = {.width = 1.0f, .radius = 8.0f, .color = {demoCtx.previewTheme.palette.borderSubtle}},
                                .clipChildren = true,
                                .theme = &demoCtx.previewTheme,
                                .scale = previewScale,
                                .debugBounds = demoCtx.previewBounds,
                                .inspectMode = demoCtx.previewInspect
                            })) {
                                if (isFullStage) {
                                    DispatchSandboxStage(ui, selectedCategory, demoCtx);
                                } else if (auto centerRow = ui.Row({
                                    .align = guikit::Alignment::Center,
                                    .justify = guikit::Justify::Center,
                                    .heightMode = guikit::SizeMode::Stretch
                                })) {
                                    DispatchSandboxStage(ui, selectedCategory, demoCtx);
                                }
                            }

                            if (auto codePanel = ui.BottomPanel(demoCtx.previewShowCode, {
                                .height = 240.0f,
                                .resizable = true,
                                .allowCollapse = true,
                                .border = {.width = 1.0f, .radius = 8.0f},
                                .theme = &demoCtx.shellTheme
                            })) {
                                if (auto codeTb = ui.Size(guikit::UiSize::Sm).Toolbar({
                                    .style = guikit::ToolbarStyle::Header
                                })) {
                                    ui.Text("GENERATED C++20 CODE", {
                                        .role = guikit::TextRole::LabelSmall,
                                        .variant = guikit::UiVariant::Secondary
                                    });
                                    codeTb.Spacer();
                                    if (codeTb.Action({
                                        .label = "Close",
                                        .tooltip = "Close Code Panel",
                                        .icon = guikit::Icon::Close(),
                                        .square = true
                                    }).clicked) {
                                        demoCtx.previewShowCode = false;
                                        ui.InvalidateAll();
                                    }
                                }

                                if (!isLiveResizeFrame || cachedCodeSnippet.empty() || cachedCodeCategory != selectedCategory) {
                                    cachedCodeSnippet = DispatchCodeGenerator(selectedCategory, demoCtx);
                                    cachedCodeCategory = selectedCategory;
                                }
                                ui.TextArea(cachedCodeSnippet, {
                                    .autoHeight = false,
                                    .readOnly = true,
                                    .showLineNumbers = true,
                                    .showCopyButton = true,
                                    .monospace = true,
                                    .allowZoom = true,
                                    .syntax = guikit::SyntaxConfig::Cpp(),
                                    .heightMode = guikit::SizeMode::Stretch,
                                    .border = {.radius = 0.0f}
                                });
                            }
                        }
                    }
                }

                // Application StatusBar
                if (auto sb = ui.StatusBar({.border = {.width = 1.0f, .radius = 6.0f}})) {
                    const bool isGpuScene = (selectedCategory == NavCategory::SceneSonarView || selectedCategory == NavCategory::SceneSignalView);
                    if (gpuShaderEngine.IsAvailable()) {
                        sb.Item({
                            .text = isGpuScene ? "GPU Shaders: GLSL Underlay Active" : "GPU Shaders: GLSL Ready",
                            .dotColor = isGpuScene
                                ? demoCtx.shellTheme.GetVariant(guikit::UiVariant::Success).bg
                                : demoCtx.shellTheme.GetVariant(guikit::UiVariant::Info).bg,
                            .variant = isGpuScene ? guikit::UiVariant::Success : guikit::UiVariant::Info,
                            .badge = gpuShadersEnabled ? "GLSL" : "OFF"
                        });
                    } else {
                        sb.Item({
                            .text = "GPU Shaders: CPU Fallback",
                            .dotColor = demoCtx.shellTheme.GetVariant(guikit::UiVariant::Warning).bg,
                            .variant = guikit::UiVariant::Warning
                        });
                    }

                    sb.Separator();
                    sb.Item({
                        .text = metrics.GetStatusString(),
                        .variant = guikit::UiVariant::Secondary
                    });

                    sb.Spacer();

                    static bool globalPaletteOpen = false;
                    if (ui.ConsumeShortcut(guikit::Key::K, true)) {
                        globalPaletteOpen = !globalPaletteOpen;
                        ui.InvalidateAll();
                    }
                    if (sb.Action({
                        .text = "Command Palette",
                        .icon = guikit::Icon::Search(),
                        .tooltip = "Open Global Command Palette (Ctrl+K)",
                        .active = globalPaletteOpen
                    }).clicked) {
                        globalPaletteOpen = !globalPaletteOpen;
                        ui.InvalidateAll();
                    }

                    if (IsSandboxCategory(selectedCategory)) {
                        if (sb.Action({
                            .text = "C++ Code",
                            .icon = demoCtx.previewShowCode ? guikit::Icon::Eye() : guikit::Icon::EyeOff(),
                            .tooltip = "Toggle BottomPanel C++20 Code Generator",
                            .active = demoCtx.previewShowCode
                        }).clicked) {
                            demoCtx.previewShowCode = !demoCtx.previewShowCode;
                            ui.InvalidateAll();
                        }
                    }

                    static const std::vector<guikit::CommandPaletteItem> kGlobalCommands = {
                        {.label = "Go to LevelMeter / Segmented VU Meter", .category = "Elements",  .description = "LED dB meters, peak-hold & clip LED", .icon = guikit::Icon::Play()},
                        {.label = "Go to TagInput Editable Chips",         .category = "Elements",  .description = "Editable tag pills & delimiter split",.icon = guikit::Icon::Edit()},
                        {.label = "Go to Select / MultiSelect / ComboBox", .category = "Elements",  .description = "Dropdown, live filter & tag chips",   .icon = guikit::Icon::Plus()},
                        {.label = "Go to DatePicker & TimePicker",         .category = "Elements",  .description = "Calendar grid & time stepper",        .icon = guikit::Icon::Edit()},
                        {.label = "Go to SpinBox Numeric Stepper",         .category = "Elements",  .description = "Inline, Detached & Split steppers",   .icon = guikit::Icon::Plus()},
                        {.label = "Go to ColorPicker HSV/HEX",             .category = "Elements",  .description = "2D SV canvas, hue & alpha sliders",   .icon = guikit::Icon::Eye()},
                        {.label = "Go to SplitView Resizable Panes",       .category = "Panels",    .description = "Horizontal & Vertical splitters",     .icon = guikit::Icon::Copy()},
                        {.label = "Go to Navigation (Breadcrumb & Page)",  .category = "Panels",    .description = "Breadcrumbs & smart pagination",      .icon = guikit::Icon::Folder()},
                        {.label = "Go to Overlays (Modal / Toast / Cmd)",  .category = "Panels",    .description = "Modals, context menus & palettes",    .icon = guikit::Icon::Flash()},
                        {.label = "Go to Table / Virtualized Grid",        .category = "Panels",    .description = "100,000 rows virtualized data grid",  .icon = guikit::Icon::Copy()},
                        {.label = "Go to Signal View (DSP)",               .category = "2D Scenes", .description = "Waveform & spectrogram analyzer",     .icon = guikit::Icon::Play()},
                        {.label = "Go to Sonar Radar (DDS)",               .category = "2D Scenes", .description = "Polar heatmap & target tracker",      .icon = guikit::Icon::Search()},
                        {.label = "Go to BPMN 2.0 Modeler",                .category = "2D Scenes", .description = "Interactive workflow graph & XML",    .icon = guikit::Icon::Edit()},
                        {.label = "Toggle Preview Dark / Light Theme",     .category = "Theme",     .description = "Switch stage palette mode",           .icon = guikit::Icon::Refresh()},
                        {.label = "Toggle Generated C++20 Code Panel",     .category = "View",      .description = "Show or hide bottom code generator",  .icon = guikit::Icon::Eye()}
                    };
                    const auto cmdResp = ui.CommandPalette(globalPaletteOpen, kGlobalCommands, {
                        .placeholder = "Jump to screen or run command..."
                    });
                    if (cmdResp.changed && cmdResp.selectedIndex >= 0) {
                        switch (cmdResp.selectedIndex) {
                            case 0: selectedCategory = NavCategory::ElemLevelMeter; break;
                            case 1: selectedCategory = NavCategory::ElemTagInput; break;
                            case 2: selectedCategory = NavCategory::ElemSelect; break;
                            case 3: selectedCategory = NavCategory::ElemDatePicker; break;
                            case 4: selectedCategory = NavCategory::ElemSpinBox; break;
                            case 5: selectedCategory = NavCategory::ElemColorPicker; break;
                            case 6: selectedCategory = NavCategory::PanelSplitView; break;
                            case 7: selectedCategory = NavCategory::PanelNavigation; break;
                            case 8: selectedCategory = NavCategory::PanelOverlays; break;
                            case 9: selectedCategory = NavCategory::PanelTable; break;
                            case 10: selectedCategory = NavCategory::SceneSignalView; break;
                            case 11: selectedCategory = NavCategory::SceneSonarView; break;
                            case 12: selectedCategory = NavCategory::SceneBpmnEditor; break;
                            case 13:
                                demoCtx.previewLightMode = !demoCtx.previewLightMode;
                                demoCtx.previewDarkMode = !demoCtx.previewLightMode;
                                {
                                    const auto defTheme = demoCtx.previewDarkMode
                                        ? guikit::UiTheme::Dark(demoCtx.previewTheme.density)
                                        : guikit::UiTheme::Light(demoCtx.previewTheme.density);
                                    demoCtx.previewTheme.palette = defTheme.palette;
                                    demoCtx.previewTheme.variants = defTheme.variants;
                                    demoCtx.previewTheme.syntax = defTheme.syntax;
                                    demoCtx.previewTheme.mode = demoCtx.previewDarkMode ? guikit::ThemeMode::Dark : guikit::ThemeMode::Light;
                                }
                                break;
                            case 14:
                                demoCtx.previewShowCode = !demoCtx.previewShowCode;
                                break;
                            default: break;
                        }
                        ui.InvalidateAll();
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
                    const float dpiScale = ui.GetDpiScale();
                    for (const guikit::Rect& dr : ui.GetDirtyRects()) {
                        const int x1 = std::clamp(static_cast<int>(std::floor(dr.x * dpiScale)), 0, winWidth);
                        const int y1 = std::clamp(static_cast<int>(std::floor(dr.y * dpiScale)), 0, winHeight);
                        const int x2 = std::clamp(static_cast<int>(std::ceil(dr.Right() * dpiScale)), 0, winWidth);
                        const int y2 = std::clamp(static_cast<int>(std::ceil(dr.Bottom() * dpiScale)), 0, winHeight);
                        if (x2 > x1 && y2 > y1) {
                            const SDL_Rect sdlR = {x1, y1, x2 - x1, y2 - y1};
                            const uint32_t* src = pixelBuffer.data() + static_cast<size_t>(sdlR.y) * winWidth + sdlR.x;
                            SDL_UpdateTexture(screenTexture, &sdlR, src, winWidth * sizeof(uint32_t));
                        }
                    }
                }
            }
            SDL_RenderClear(renderer);
            if (canvas.HasActiveGpuPasses()) {
                gpuShaderEngine.RenderUnderlays(canvas, static_cast<uint32_t>(winWidth), static_cast<uint32_t>(winHeight));
            }
            SDL_RenderTexture(renderer, screenTexture, nullptr, nullptr);
            metrics.MarkGpuDone();

            SDL_RenderPresent(renderer);
            metrics.MarkFramePresented();
        }

#ifndef __EMSCRIPTEN__
        if (!shouldPresent || (!vsyncEnabled && !reactiveMode)) {
            const auto frameEnd = std::chrono::steady_clock::now();
            const double frameElapsedMs = std::chrono::duration<double, std::milli>(frameEnd - frameWorkStart).count();
            const double targetFrameMs = 1000.0 / 60.0;
            if (frameElapsedMs + 1.0 < targetFrameMs) {
                const double sleepMs = shouldPresent
                    ? (targetFrameMs - frameElapsedMs)
                    : std::min(4.0, targetFrameMs - frameElapsedMs);
                SDL_Delay(static_cast<uint32_t>(sleepMs));
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

        int targetW = winWidth;
        int targetH = winHeight;
        if (getCanvasPhysicalSize(targetW, targetH)) {
            if (targetW != winWidth || targetH != winHeight) {
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
    auto liveResizeWatch = [&](SDL_Event* event) -> bool {
        if (!running || inRenderFrame || !event || !ui.IsLiveResizeEnabled()) {
            return true;
        }
        if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            if (event->window.data1 > 0 && event->window.data2 > 0) {
                winWidth = event->window.data1;
                winHeight = event->window.data2;
                ui.InvalidateAll();
                ui.RequestRepaint(3);
            }
            return true;
        }
        // Only render on SDL_EVENT_WINDOW_EXPOSED (which fires on WM_PAINT / live-resize timer
        // AFTER DefWindowProc(WM_WINDOWPOSCHANGED) and WM_SIZE have updated the OpenGL backbuffer)
        // when the window pixel dimensions actually differ from the last rendered buffer.
        if (event->type == SDL_EVENT_WINDOW_EXPOSED &&
            (winWidth != bufWidth || winHeight != bufHeight)) {
            const auto now = std::chrono::steady_clock::now();
            const double elapsedMs = std::chrono::duration<double, std::milli>(now - lastLiveResizeTime).count();
            if (elapsedMs >= 16.0) {
                lastLiveResizeTime = now;
                isLiveResizeFrame = true;
                renderFrame();
                isLiveResizeFrame = false;
            }
        }
        return true;
    };
    SDL_EventFilter watchCallback = [](void* userdata, SDL_Event* event) -> bool {
        return (*static_cast<decltype(liveResizeWatch)*>(userdata))(event);
    };
    SDL_AddEventWatch(watchCallback, &liveResizeWatch);

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

    SDL_RemoveEventWatch(watchCallback, &liveResizeWatch);
#endif

    gpuShaderEngine.Shutdown();
    if (screenTexture) {
        SDL_DestroyTexture(screenTexture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
