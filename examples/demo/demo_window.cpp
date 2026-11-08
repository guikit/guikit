#include "demo_window.hpp"
#include "preview_viewport.hpp"

namespace guikit::demo {

DemoWindow::DemoWindow()
    : guikit::MainWindow({
          .title = "GuiKit Vector GUI Demo (C++20 Immediate Mode + ThorVG)",
          .width = 1380,
          .height = 800,
          .minWidth = 320,
          .minHeight = 240,
          .idleHeartbeatMs = 500
      }) {}

void DemoWindow::OnInit() {
    m_demoCtx.displayPxPerMm = GetDisplayPixelsPerMm();
    m_demoCtx.shellTheme = GetShellTheme();
    m_demoCtx.shellLightMode = IsShellLightMode();
    m_demoCtx.shellFloating = IsShellFloating();
}

void DemoWindow::OnResize(int /*width*/, int /*height*/) {
    m_demoCtx.displayPxPerMm = GetDisplayPixelsPerMm();
}

void DemoWindow::OnRender(guikit::Context& ui) {
    m_demoCtx.displayPxPerMm = GetDisplayPixelsPerMm();
    m_demoCtx.shellTheme = GetShellTheme();
    m_demoCtx.shellLightMode = IsShellLightMode();
    m_demoCtx.shellFloating = IsShellFloating();

    RenderTopToolbar(ui);
    RenderWorkspace(ui);
    RenderBottomStatusBar(ui);
}

void DemoWindow::RenderTopToolbar(guikit::Context& ui) {
    if (auto tb = ui.Toolbar()) {
        RenderSidebarCycleButton(tb, m_navSidebarMode, "Nav", "Navigation Sidebar", guikit::Icon::Folder());
        RenderSidebarToggle(tb, m_inspectorSidebarMode, "Inspector", "Inspector Sidebar", guikit::Icon::Settings());

        tb.Separator();

        RenderThemeControls(tb);
        RenderDebugControls(tb);

        tb.Separator();

        RenderEngineControls(tb);

        tb.Spacer();
        if (ui.Text("https://github.com/guikit/", {.role = guikit::TextRole::TitleMedium, .variant = guikit::UiVariant::Info})) {
            OpenUrl("https://github.com/guikit/");
        }
    }
}

void DemoWindow::RenderCatalogSidebar(guikit::Context& ui) {
    if (auto railSidebar = ui.Sidebar(m_navSidebarMode, {
        .side = guikit::SidebarSide::Left,
        .maxRatio = 0.35f,
        .resizable = true,
        .allowCompact = true,
        .allowCollapse = true
    })) {
        const NavCategory prevCat = m_selectedCategory;
        if (auto menu = ui.Size(guikit::UiSize::Sm).SidebarMenu()) {
            menu.Section("ELEMENTS");
            menu.Item(m_selectedCategory, NavCategory::ElemHeading,       {.text = "Heading",      .icon = guikit::Icon::Edit()});
            menu.Item(m_selectedCategory, NavCategory::ElemButton,        {.text = "Button",       .icon = guikit::Icon::Play()});
            menu.Item(m_selectedCategory, NavCategory::ElemSlider,        {.text = "Slider",       .icon = guikit::Icon::Settings()});
            menu.Item(m_selectedCategory, NavCategory::ElemLevelMeter,    {.text = "LevelMeter",   .icon = guikit::Icon::Play()});
            menu.Item(m_selectedCategory, NavCategory::ElemKnob,          {.text = "Knob",         .icon = guikit::Icon::Refresh()});
            menu.Item(m_selectedCategory, NavCategory::ElemSpinBox,       {.text = "SpinBox",      .icon = guikit::Icon::Plus()});
            menu.Item(m_selectedCategory, NavCategory::ElemInputField,    {.text = "Input",        .icon = guikit::Icon::Edit()});
            menu.Item(m_selectedCategory, NavCategory::ElemTagInput,      {.text = "TagInput",     .icon = guikit::Icon::Edit()});
            menu.Item(m_selectedCategory, NavCategory::ElemSelect,        {.text = "Select",       .icon = guikit::Icon::Plus()});
            menu.Item(m_selectedCategory, NavCategory::ElemSegmented,     {.text = "Segmented",    .icon = guikit::Icon::Settings()});
            menu.Item(m_selectedCategory, NavCategory::ElemColorPicker,   {.text = "ColorPicker",  .icon = guikit::Icon::Eye()});
            menu.Item(m_selectedCategory, NavCategory::ElemDatePicker,    {.text = "DatePicker",   .icon = guikit::Icon::Edit()});
            menu.Item(m_selectedCategory, NavCategory::ElemSwitch,        {.text = "Switch",       .icon = guikit::Icon::Refresh()});
            menu.Item(m_selectedCategory, NavCategory::ElemCheckbox,      {.text = "Checkbox",     .icon = guikit::Icon::Check()});
            menu.Item(m_selectedCategory, NavCategory::ElemRadio,         {.text = "Radio",        .icon = guikit::Icon::Check()});
            menu.Item(m_selectedCategory, NavCategory::ElemImage,         {.text = "Image",        .icon = guikit::Icon::Eye()});

            menu.Section("PANELS & NAV");
            menu.Item(m_selectedCategory, NavCategory::PanelCard,         {.text = "Card",         .icon = guikit::Icon::Copy()});
            menu.Item(m_selectedCategory, NavCategory::PanelSplitView,    {.text = "SplitView",    .icon = guikit::Icon::Copy()});
            menu.Item(m_selectedCategory, NavCategory::PanelTabBar,       {.text = "TabBar",       .icon = guikit::Icon::Folder()});
            menu.Item(m_selectedCategory, NavCategory::PanelBreadcrumb,   {.text = "Breadcrumb",   .icon = guikit::Icon::Folder()});
            menu.Item(m_selectedCategory, NavCategory::PanelPagination,   {.text = "Pagination",   .icon = guikit::Icon::ArrowRight()});
            menu.Item(m_selectedCategory, NavCategory::PanelToolbar,      {.text = "Toolbar",      .icon = guikit::Icon::Settings()});
            menu.Item(m_selectedCategory, NavCategory::PanelSidebar,      {.text = "Sidebar",      .icon = guikit::Icon::Folder()});
            menu.Item(m_selectedCategory, NavCategory::PanelTreeView,     {.text = "TreeView",     .icon = guikit::Icon::Folder()});
            menu.Item(m_selectedCategory, NavCategory::PanelTable,        {.text = "Table / Grid", .icon = guikit::Icon::Copy()});
            menu.Item(m_selectedCategory, NavCategory::PanelOverlays,     {.text = "Overlays",     .icon = guikit::Icon::Flash()});

            menu.Section("CONTENT & MEDIA");
            menu.Item(m_selectedCategory, NavCategory::ContentMarkdown,   {.text = "Markdown",     .icon = guikit::Icon::Edit()});
            menu.Item(m_selectedCategory, NavCategory::ContentCodeEditor, {.text = "Code Editor",  .icon = guikit::Icon::Edit()});

            menu.Section("2D SCENES");
            menu.Item(m_selectedCategory, NavCategory::SceneSchematic2D,   {.text = "Schematic 2D", .icon = guikit::Icon::Search()});
            menu.Item(m_selectedCategory, NavCategory::SceneOsmMap,        {.text = "GIS",          .icon = guikit::Icon::Search()});
            menu.Item(m_selectedCategory, NavCategory::SceneSignalView,    {.text = "Signal View",  .icon = guikit::Icon::Play()});
            menu.Item(m_selectedCategory, NavCategory::SceneSonarView,     {.text = "Sonar Radar",  .icon = guikit::Icon::Search()});
            menu.Item(m_selectedCategory, NavCategory::SceneWaterfallView, {.text = "Waterfall",    .icon = guikit::Icon::Play()});
            menu.Item(m_selectedCategory, NavCategory::SceneImageView,     {.text = "Image View",   .icon = guikit::Icon::Eye()});
            menu.Item(m_selectedCategory, NavCategory::SceneAttitudeView,  {.text = "Attitude PFD", .icon = guikit::Icon::Play()});
            menu.Item(m_selectedCategory, NavCategory::SceneCarDashboard,  {.text = "Car Cluster",  .icon = guikit::Icon::Play()});
            menu.Item(m_selectedCategory, NavCategory::SceneBpmnEditor,    {.text = "BPMN Modeler", .icon = guikit::Icon::Edit()});
        }
        if (m_selectedCategory != prevCat) {
            ui.InvalidateAll();
        }
    }
}

void DemoWindow::RenderInspectorSidebar(guikit::Context& ui) {
    if (auto inspectorSidebar = ui.Sidebar(m_inspectorSidebarMode, {
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
            .separator = guikit::HeadingSeparator::Bottom
        });

        if (IsElementCategory(m_selectedCategory)) {
            if (auto elemAcc = ui.Size(guikit::UiSize::Sm).Accordion({
                .label = "ELEMENT",
                .defaultOpen = false
            })) {
                m_demoCtx.RenderElementInspector(ui, m_selectedCategory);
            }

            if (m_selectedCategory == NavCategory::ElemHeading) {
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
                    m_demoCtx.RenderControlInspector(ui, m_selectedCategory);
                }
            }

            if (auto specAcc = ui.Size(guikit::UiSize::Sm).Accordion({
                .label = "SPECIFIC",
                .defaultOpen = true
            })) {
                DispatchInspector(ui, m_selectedCategory, m_demoCtx);
            }
        } else {
            DispatchInspector(ui, m_selectedCategory, m_demoCtx);
        }
    }
}

void DemoWindow::RenderPreviewToolbar(guikit::Context& ui) {
    if (auto previewTb = ui.Toolbar()) {
        ui.Text("PREVIEW", {.role = guikit::TextRole::LabelSmall, .variant = guikit::UiVariant::Muted});
        previewTb.Separator();

        if (ui.Size(guikit::UiSize::Sm).Select(m_demoCtx.previewFormFactorIdx, kPreviewFormFactorNames, {.width = 96.0f}).changed) {
            if (m_demoCtx.previewFormFactorIdx == 1) {
                m_demoCtx.previewLandscape = true;  // Tablet defaults to Landscape
                if (m_demoCtx.previewZoomModeIdx == static_cast<size_t>(PreviewZoomMode::Zoom100)) {
                    m_demoCtx.previewZoomModeIdx = static_cast<size_t>(PreviewZoomMode::Fit);
                }
            } else if (m_demoCtx.previewFormFactorIdx == 2) {
                m_demoCtx.previewLandscape = false; // Phone defaults to Portrait
                if (m_demoCtx.previewZoomModeIdx == static_cast<size_t>(PreviewZoomMode::Zoom100)) {
                    m_demoCtx.previewZoomModeIdx = static_cast<size_t>(PreviewZoomMode::Fit);
                }
            }
            ui.InvalidateAll();
        }

        static constexpr std::array<std::string_view, 2> kOrientationVariants = {"Port", "Land"};
        const bool canRotateDevice = (m_demoCtx.previewFormFactorIdx == 1 || m_demoCtx.previewFormFactorIdx == 2);
        if (previewTb.Toggle(m_demoCtx.previewLandscape, {
            .text = {m_demoCtx.previewLandscape ? "Land" : "Port", kOrientationVariants},
            .tooltip = "Rotate Device Orientation (Portrait / Landscape)",
            .icon = guikit::Icon::Refresh(),
            .color = guikit::StateColor::Info(),
            .disabled = !canRotateDevice
        }).changed) {
            ui.InvalidateAll();
        }

        previewTb.Separator();

        const int effZoomPct = static_cast<int>(std::lround(m_demoCtx.previewEffectiveZoom * 100.0f));
        guikit::SelectOptions zoomSelectOpts{
            .tooltip = "Preview Zoom / Scale (Fit, Real 1:1 physical mm, or Ctrl + MouseWheel)",
            .width = 116.0f
        };
        if (m_demoCtx.previewZoomModeIdx == static_cast<size_t>(PreviewZoomMode::Fit) ||
            m_demoCtx.previewZoomModeIdx == static_cast<size_t>(PreviewZoomMode::Real1To1)) {
            zoomSelectOpts.suffix.text = {ui.Format("{}%", effZoomPct), guikit::StateColor::Muted()};
        }

        bool zoomChanged = false;
        if (m_demoCtx.previewZoomModeIdx >= kPreviewZoomPresetNames.size()) {
            const std::string_view customLabel = ui.AllocString(ui.Format("{}%", effZoomPct));
            const std::array<std::string_view, 9> zoomItemsWithCustom = {
                kPreviewZoomPresetNames[0],
                kPreviewZoomPresetNames[1],
                kPreviewZoomPresetNames[2],
                kPreviewZoomPresetNames[3],
                kPreviewZoomPresetNames[4],
                kPreviewZoomPresetNames[5],
                kPreviewZoomPresetNames[6],
                kPreviewZoomPresetNames[7],
                customLabel
            };
            zoomChanged = ui.Size(guikit::UiSize::Sm).Select(m_demoCtx.previewZoomModeIdx, zoomItemsWithCustom, zoomSelectOpts).changed;
        } else {
            zoomChanged = ui.Size(guikit::UiSize::Sm).Select(m_demoCtx.previewZoomModeIdx, kPreviewZoomPresetNames, zoomSelectOpts).changed;
        }
        if (zoomChanged) {
            ui.InvalidateAll();
        }

        if (ui.Size(guikit::UiSize::Md).Switch(m_demoCtx.previewLightMode, {
            .textLeft = "Dark",
            .textRight = "Light",
            .symmetric = true
        }).changed) {
            m_demoCtx.previewDarkMode = !m_demoCtx.previewLightMode;
            m_demoCtx.previewTheme.SetMode(m_demoCtx.previewDarkMode
                ? guikit::ThemeMode::Dark
                : guikit::ThemeMode::Light);
            ui.InvalidateAll();
        }

        previewTb.Toggle(m_demoCtx.previewBounds, {
            .text = "Bounds",
            .icon = m_demoCtx.previewBounds ? guikit::Icon::Eye() : guikit::Icon::EyeOff(),
            .color = guikit::StateColor::Info()
        });

        previewTb.Toggle(m_demoCtx.previewInspect, {
            .text = "Inspect",
            .icon = guikit::Icon::Search(),
            .color = guikit::StateColor::Warning()
        });

        if (IsSandboxCategory(m_selectedCategory)) {
            if (previewTb.Toggle(m_demoCtx.previewShowCode, {
                .text = "Code",
                .icon = m_demoCtx.previewShowCode ? guikit::Icon::Eye() : guikit::Icon::EyeOff(),
                .color = guikit::StateColor::Primary()
            }).changed) {
                ui.RequestRepaint(3);
            }
        }
    }
}

void DemoWindow::RenderCodePanel(guikit::Context& ui) {
    if (auto codePanel = ui.BottomPanel(m_demoCtx.previewShowCode, {
        .height = 240.0f,
        .resizable = true,
        .allowCollapse = true,
        .theme = &GetShellTheme()
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
                .text = "Close",
                .tooltip = "Close Code Panel",
                .icon = guikit::Icon::Close(),
                .square = true
            }).clicked) {
                m_demoCtx.previewShowCode = false;
                ui.InvalidateAll();
            }
        }

        if (!IsLiveResizeFrame() || m_cachedCodeSnippet.empty() || m_cachedCodeCategory != m_selectedCategory) {
            m_cachedCodeSnippet = DispatchCodeGenerator(m_selectedCategory, m_demoCtx);
            m_cachedCodeCategory = m_selectedCategory;
        }
        ui.TextArea(m_cachedCodeSnippet, {
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

void DemoWindow::RenderWorkspace(guikit::Context& ui) {
    if (auto workspaceRow = ui.Row({.heightMode = guikit::SizeMode::Stretch})) {
        RenderCatalogSidebar(ui);
        RenderInspectorSidebar(ui);

        if (auto contentCol = ui.Column({
            .widthMode = guikit::SizeMode::Stretch,
            .heightMode = guikit::SizeMode::Stretch
        })) {
            RenderPreviewToolbar(ui);

            if (IsSandboxCategory(m_selectedCategory)) {
                RenderPreviewViewport(ui, m_demoCtx, m_selectedCategory, [&]() {
                    DispatchSandboxStage(ui, m_selectedCategory, m_demoCtx);
                });
                RenderCodePanel(ui);
            }
        }
    }
}

void DemoWindow::RenderBottomStatusBar(guikit::Context& ui) {
    if (auto sb = ui.StatusBar()) {
        const bool isGpuScene = (m_selectedCategory == NavCategory::SceneSonarView ||
                                 m_selectedCategory == NavCategory::SceneWaterfallView ||
                                 m_selectedCategory == NavCategory::SceneSignalView);
        RenderTelemetryStatus(sb, isGpuScene);

        sb.Spacer();

        static const std::array<guikit::CommandPaletteItem, 17> kGlobalCommands = {{
            {.label = "Go to LevelMeter / Segmented VU Meter", .category = "Elements",  .description = "LED dB meters, peak-hold & clip LED", .icon = guikit::Icon::Play()},
            {.label = "Go to TagInput Editable Chips",         .category = "Elements",  .description = "Editable tag pills & delimiter split",.icon = guikit::Icon::Edit()},
            {.label = "Go to Select / MultiSelect / ComboBox", .category = "Elements",  .description = "Dropdown, live filter & tag chips",   .icon = guikit::Icon::Plus()},
            {.label = "Go to DatePicker & TimePicker",         .category = "Elements",  .description = "Calendar grid & time stepper",        .icon = guikit::Icon::Edit()},
            {.label = "Go to SpinBox Numeric Stepper",         .category = "Elements",  .description = "Inline, Detached & Split steppers",   .icon = guikit::Icon::Plus()},
            {.label = "Go to ColorPicker HSV/HEX",             .category = "Elements",  .description = "2D SV canvas, hue & alpha sliders",   .icon = guikit::Icon::Eye()},
            {.label = "Go to SplitView Resizable Panes",       .category = "Panels",    .description = "Horizontal & Vertical splitters",     .icon = guikit::Icon::Copy()},
            {.label = "Go to Breadcrumb Path Navigation",      .category = "Panels",    .description = "Hierarchical path trail & collapse",  .icon = guikit::Icon::Folder()},
            {.label = "Go to Pagination Control",              .category = "Panels",    .description = "Page numbers & smart ellipsis jump",  .icon = guikit::Icon::ArrowRight()},
            {.label = "Go to Overlays (Modal / Toast / Cmd)",  .category = "Panels",    .description = "Modals, context menus & palettes",    .icon = guikit::Icon::Flash()},
            {.label = "Go to Table / Virtualized Grid",        .category = "Panels",    .description = "100,000 rows virtualized data grid",  .icon = guikit::Icon::Copy()},
            {.label = "Go to Signal View (DSP)",               .category = "2D Scenes", .description = "Waveform & spectrogram analyzer",     .icon = guikit::Icon::Play()},
            {.label = "Go to Sonar Radar (DDS)",               .category = "2D Scenes", .description = "Polar heatmap & target tracker",      .icon = guikit::Icon::Search()},
            {.label = "Go to BPMN 2.0 Modeler",                .category = "2D Scenes", .description = "Interactive workflow graph & XML",    .icon = guikit::Icon::Edit()},
            {.label = "Toggle Preview Dark / Light Theme",     .category = "Theme",     .description = "Switch stage palette mode",           .icon = guikit::Icon::Refresh()},
            {.label = "Toggle Generated C++20 Code Panel",     .category = "View",      .description = "Show or hide bottom code generator",  .icon = guikit::Icon::Eye()},
            {.label = "Toggle Docked / Floating Island Shell", .category = "View",      .description = "Switch between flush IDE & floating island frame", .icon = guikit::Icon::Copy()}
        }};

        const auto cmdResp = RenderCommandPalette(sb, kGlobalCommands);

        if (IsSandboxCategory(m_selectedCategory)) {
            if (sb.Action({
                .text = "C++ Code",
                .icon = m_demoCtx.previewShowCode ? guikit::Icon::Eye() : guikit::Icon::EyeOff(),
                .tooltip = "Toggle BottomPanel C++20 Code Generator",
                .active = m_demoCtx.previewShowCode
            }).clicked) {
                m_demoCtx.previewShowCode = !m_demoCtx.previewShowCode;
                ui.InvalidateAll();
            }
        }

        if (cmdResp.changed && cmdResp.selectedIndex >= 0) {
            switch (cmdResp.selectedIndex) {
                case 0:  m_selectedCategory = NavCategory::ElemLevelMeter; break;
                case 1:  m_selectedCategory = NavCategory::ElemTagInput; break;
                case 2:  m_selectedCategory = NavCategory::ElemSelect; break;
                case 3:  m_selectedCategory = NavCategory::ElemDatePicker; break;
                case 4:  m_selectedCategory = NavCategory::ElemSpinBox; break;
                case 5:  m_selectedCategory = NavCategory::ElemColorPicker; break;
                case 6:  m_selectedCategory = NavCategory::PanelSplitView; break;
                case 7:  m_selectedCategory = NavCategory::PanelBreadcrumb; break;
                case 8:  m_selectedCategory = NavCategory::PanelPagination; break;
                case 9:  m_selectedCategory = NavCategory::PanelOverlays; break;
                case 10: m_selectedCategory = NavCategory::PanelTable; break;
                case 11: m_selectedCategory = NavCategory::SceneSignalView; break;
                case 12: m_selectedCategory = NavCategory::SceneSonarView; break;
                case 13: m_selectedCategory = NavCategory::SceneBpmnEditor; break;
                case 14:
                    m_demoCtx.previewLightMode = !m_demoCtx.previewLightMode;
                    m_demoCtx.previewDarkMode = !m_demoCtx.previewLightMode;
                    m_demoCtx.previewTheme.SetMode(m_demoCtx.previewDarkMode
                        ? guikit::ThemeMode::Dark
                        : guikit::ThemeMode::Light);
                    break;
                case 15:
                    m_demoCtx.previewShowCode = !m_demoCtx.previewShowCode;
                    break;
                case 16:
                    SetShellFloating(!IsShellFloating());
                    break;
                default: break;
            }
            ui.InvalidateAll();
        }
    }
}

void DemoWindow::DispatchInspector(guikit::Context& ui, NavCategory cat, DemoContext& ctx) {
    using InspectorFn = void (*)(guikit::Context&, DemoContext&);
    static constexpr std::array<InspectorFn, 37> kInspectors = {
        // 1. ELEMENTS
        &RenderHeadingInspector,
        &RenderButtonInspector,
        &RenderSliderInspector,
        &RenderLevelMeterInspector,
        &RenderKnobInspector,
        &RenderSpinBoxInspector,
        &RenderInputInspector,
        &RenderTagInputInspector,
        &RenderSelectInspector,
        &RenderSegmentedInspector,
        &RenderColorPickerInspector,
        &RenderDatePickerInspector,
        &RenderSwitchInspector,
        &RenderCheckboxInspector,
        &RenderRadioInspector,
        &RenderImageInspector,

        // 2. PANELS & NAV
        &RenderCardInspector,
        &RenderSplitViewInspector,
        &RenderTabBarInspector,
        &RenderBreadcrumbInspector,
        &RenderPaginationInspector,
        &RenderToolbarInspector,
        &RenderSidebarInspector,
        &RenderTreeViewInspector,
        &RenderTableInspector,
        &RenderOverlaysInspector,

        // 3. CONTENT & MEDIA
        &RenderMarkdownInspector,
        &RenderCodeEditorInspector,

        // 4. 2D SCENES
        &RenderSchematicInspector,
        &RenderOsmMapInspector,
        &RenderSignalInspector,
        &RenderSonarInspector,
        &RenderWaterfallInspector,
        &RenderImageViewInspector,
        &RenderAttitudeInspector,
        &RenderCarDashboardInspector,
        &RenderBpmnInspector,
    };
    static_assert(kInspectors.size() == static_cast<size_t>(NavCategory::SceneBpmnEditor) + 1);

    const size_t idx = static_cast<size_t>(cat);
    if (idx < kInspectors.size()) {
        kInspectors[idx](ui, ctx);
    }
}

void DemoWindow::DispatchSandboxStage(guikit::Context& ui, NavCategory cat, DemoContext& ctx) {
    using StageFn = void (*)(guikit::Context&, DemoContext&);
    static constexpr std::array<StageFn, 37> kStages = {
        // 1. ELEMENTS
        &RenderHeadingStage,
        &RenderButtonStage,
        &RenderSliderStage,
        &RenderLevelMeterStage,
        &RenderKnobStage,
        &RenderSpinBoxStage,
        &RenderInputStage,
        &RenderTagInputStage,
        &RenderSelectStage,
        &RenderSegmentedStage,
        &RenderColorPickerStage,
        &RenderDatePickerStage,
        &RenderSwitchStage,
        &RenderCheckboxStage,
        &RenderRadioStage,
        &RenderImageStage,

        // 2. PANELS & NAV
        &RenderCardStage,
        &RenderSplitViewStage,
        &RenderTabBarStage,
        &RenderBreadcrumbStage,
        &RenderPaginationStage,
        &RenderToolbarStage,
        &RenderSidebarStage,
        &RenderTreeViewStage,
        &RenderTableStage,
        &RenderOverlaysStage,

        // 3. CONTENT & MEDIA
        &RenderMarkdownStage,
        &RenderCodeEditorStage,

        // 4. 2D SCENES
        &RenderSchematicStage,
        &RenderOsmMapStage,
        &RenderSignalStage,
        &RenderSonarStage,
        &RenderWaterfallStage,
        &RenderImageViewStage,
        &RenderAttitudeStage,
        &RenderCarDashboardStage,
        &RenderBpmnStage,
    };
    static_assert(kStages.size() == static_cast<size_t>(NavCategory::SceneBpmnEditor) + 1);

    const size_t idx = static_cast<size_t>(cat);
    if (idx < kStages.size()) {
        kStages[idx](ui, ctx);
    }
}

std::string DemoWindow::DispatchCodeGenerator(NavCategory cat, const DemoContext& ctx) {
    using CodeGenFn = std::string (*)(const DemoContext&);
    static constexpr std::array<CodeGenFn, 37> kGenerators = {
        // 1. ELEMENTS
        &GenerateHeadingCode,
        &GenerateButtonCode,
        &GenerateSliderCode,
        &GenerateLevelMeterCode,
        &GenerateKnobCode,
        &GenerateSpinBoxCode,
        &GenerateInputCode,
        &GenerateTagInputCode,
        &GenerateSelectCode,
        &GenerateSegmentedCode,
        &GenerateColorPickerCode,
        &GenerateDatePickerCode,
        &GenerateSwitchCode,
        &GenerateCheckboxCode,
        &GenerateRadioCode,
        &GenerateImageCode,

        // 2. PANELS & NAV
        &GenerateCardCode,
        &GenerateSplitViewCode,
        &GenerateTabBarCode,
        &GenerateBreadcrumbCode,
        &GeneratePaginationCode,
        &GenerateToolbarCode,
        &GenerateSidebarCode,
        &GenerateTreeViewCode,
        &GenerateTableCode,
        &GenerateOverlaysCode,

        // 3. CONTENT & MEDIA
        &GenerateMarkdownCode,
        &GenerateCodeEditorCode,

        // 4. 2D SCENES
        &GenerateSchematicCode,
        &GenerateOsmMapCode,
        &GenerateSignalCode,
        &GenerateSonarCode,
        &GenerateWaterfallCode,
        &GenerateImageViewCode,
        &GenerateAttitudeCode,
        &GenerateCarDashboardCode,
        &GenerateBpmnCode,
    };
    static_assert(kGenerators.size() == static_cast<size_t>(NavCategory::SceneBpmnEditor) + 1);

    const size_t idx = static_cast<size_t>(cat);
    if (idx < kGenerators.size()) {
        return kGenerators[idx](ctx);
    }
    return {};
}

} // namespace guikit::demo
