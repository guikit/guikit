#include "demo_window.hpp"
#include "preview_viewport.hpp"
#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace guikit::demo {

namespace {

void ReleaseInactiveSceneBuffers(
    NavCategory leavingCat,
    keel::i32 leavingExt,
    const keel::Array<DemoExtensionScreen>& extensions
) noexcept {
    bool releasedHeavyBuffers = false;
    if (leavingExt >= 0 && static_cast<keel::usize>(leavingExt) < extensions.size()) {
        if (extensions[static_cast<keel::usize>(leavingExt)].releaseBuffers != nullptr) {
            extensions[static_cast<keel::usize>(leavingExt)].releaseBuffers();
            releasedHeavyBuffers = true;
        }
    } else if (leavingCat == NavCategory::SceneImageView) {
        ReleaseImageViewDemoBuffers();
        releasedHeavyBuffers = true;
    }
    if (releasedHeavyBuffers) {
#if defined(__GLIBC__)
        ::malloc_trim(0);
#endif
    }
}

} // namespace

DemoWindow::DemoWindow()
    : guikit::MainWindow({
          .title = "GuiKit Vector GUI Demo (C++20 Immediate Mode + ThorVG)",
          .width = 1380,
          .height = 800,
          .minWidth = 320,
          .minHeight = 240,
          .idleHeartbeatMs = 500
      }) {}

void DemoWindow::RegisterExtension(DemoExtensionScreen ext) {
    m_extensions.push_back(ext);
}

void DemoWindow::OnInit() {
    m_demoCtx.displayPxPerMm = GetDisplayPixelsPerMm();
    m_demoCtx.shellTheme = GetShellTheme();
    m_demoCtx.shellLightMode = IsShellLightMode();
    m_demoCtx.shellFloating = IsShellFloating();
}

void DemoWindow::OnResize(keel::i32 /*width*/, keel::i32 /*height*/) {
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

    if (m_selectedCategory != m_prevRenderedCategory || m_selectedExtension != m_prevRenderedExtension) {
        ReleaseInactiveSceneBuffers(m_prevRenderedCategory, m_prevRenderedExtension, m_extensions);
        m_prevRenderedCategory = m_selectedCategory;
        m_prevRenderedExtension = m_selectedExtension;
    }
    if (!m_demoCtx.previewShowCode && m_cachedCodeSnippet.capacity() > 0) {
        m_cachedCodeSnippet = {};
    }
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
        const keel::i32 prevExt = m_selectedExtension;
        if (auto menu = ui.SidebarMenu({.size = guikit::UiSize::Sm})) {
            constexpr keel::i32 kBaseCount = static_cast<keel::i32>(NavCategory::SceneImageView) + 1;
            keel::i32 activeIdx = (m_selectedExtension >= 0)
                ? (kBaseCount + m_selectedExtension)
                : static_cast<keel::i32>(m_selectedCategory);

            menu.Section("STRUCTURE");
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemHeading),       {.text = "Heading",      .icon = guikit::Icon::Edit()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemElement),       {.text = "Element",      .icon = guikit::Icon::Settings()});

            menu.Section("CONTROLS");
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemButton),        {.text = "Button",       .icon = guikit::Icon::Play()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemSlider),        {.text = "Slider",       .icon = guikit::Icon::Settings()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemLevelMeter),    {.text = "LevelMeter",   .icon = guikit::Icon::Play()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemKnob),          {.text = "Knob",         .icon = guikit::Icon::Refresh()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemSpinBox),       {.text = "SpinBox",      .icon = guikit::Icon::Plus()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemInputField),    {.text = "Input",        .icon = guikit::Icon::Edit()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemTagInput),      {.text = "TagInput",     .icon = guikit::Icon::Edit()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemSelect),        {.text = "Select",       .icon = guikit::Icon::Plus()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemSegmented),     {.text = "Segmented",    .icon = guikit::Icon::Settings()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemColorPicker),   {.text = "ColorPicker",  .icon = guikit::Icon::Eye()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemDatePicker),    {.text = "DatePicker",   .icon = guikit::Icon::Edit()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemSwitch),        {.text = "Switch",       .icon = guikit::Icon::Refresh()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemCheckbox),      {.text = "Checkbox",     .icon = guikit::Icon::Check()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemRadio),         {.text = "Radio",        .icon = guikit::Icon::Check()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ElemImage),         {.text = "Image",        .icon = guikit::Icon::Eye()});

            menu.Section("CONTAINERS & SHELL");
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::PanelCard),         {.text = "Card",         .icon = guikit::Icon::Copy()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::PanelSplitView),    {.text = "SplitView",    .icon = guikit::Icon::Copy()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::PanelSidebar),      {.text = "Sidebar",      .icon = guikit::Icon::Folder()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::PanelOverlays),     {.text = "Overlays",     .icon = guikit::Icon::Flash()});

            menu.Section("NAVIGATION");
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::PanelTabBar),       {.text = "TabBar",       .icon = guikit::Icon::Folder()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::PanelBreadcrumb),   {.text = "Breadcrumb",   .icon = guikit::Icon::Folder()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::PanelPagination),   {.text = "Pagination",   .icon = guikit::Icon::ArrowRight()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::PanelToolbar),      {.text = "Toolbar",      .icon = guikit::Icon::Settings()});

            menu.Section("DATA");
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::PanelTreeView),     {.text = "TreeView",     .icon = guikit::Icon::Folder()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::PanelTable),        {.text = "Table / Grid", .icon = guikit::Icon::Copy()});

            menu.Section("CONTENT");
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ContentMarkdown),   {.text = "Markdown",     .icon = guikit::Icon::Edit()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::ContentCodeEditor), {.text = "Code Editor",  .icon = guikit::Icon::Edit()});

            menu.Section("2D SCENES");
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::SceneSchematic2D),  {.text = "Schematic 2D", .icon = guikit::Icon::Search()});
            menu.Item(activeIdx, static_cast<keel::i32>(NavCategory::SceneImageView),    {.text = "Image View",   .icon = guikit::Icon::Eye()});

            if (!m_extensions.empty()) {
                menu.Section("MODULES");
                for (keel::usize ei = 0; ei < m_extensions.size(); ++ei) {
                    const auto& ext = m_extensions[ei];
                    menu.Item(activeIdx, kBaseCount + static_cast<keel::i32>(ei), {
                        .text = ext.navLabel,
                        .icon = ext.icon
                    });
                }
            }

            if (activeIdx >= kBaseCount) {
                m_selectedExtension = activeIdx - kBaseCount;
            } else {
                m_selectedExtension = -1;
                m_selectedCategory = static_cast<NavCategory>(activeIdx);
            }
        }
        if (m_selectedCategory != prevCat || m_selectedExtension != prevExt) {
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
        ui.Heading({
            .label = "INSPECTOR",
            .level = guikit::HeadingLevel::H6,
            .separator = guikit::HeadingSeparator::Bottom
        });

        if (m_selectedExtension >= 0 && static_cast<keel::usize>(m_selectedExtension) < m_extensions.size()) {
            if (auto fn = m_extensions[static_cast<keel::usize>(m_selectedExtension)].renderInspector) {
                fn(ui, m_demoCtx);
            }
        } else if (IsControlCategory(m_selectedCategory)) {
            if (auto elemAcc = ui.Accordion({
                .label = "ELEMENT",
                .defaultOpen = false,
                .size = guikit::UiSize::Sm
            })) {
                m_demoCtx.RenderElementSection(ui);
            }

            if (auto elemLayoutAcc = ui.Accordion({
                .label = "ELEMENT LAYOUT",
                .defaultOpen = false,
                .size = guikit::UiSize::Sm
            })) {
                m_demoCtx.RenderElementLayoutSection(ui, m_selectedCategory);
            }

            const bool hasCtrlLayout = DemoContext::HasCtrlLayoutCategory(m_selectedCategory);
            bool collapsedOpen = false;
            guikit::AccordionOptions ctrlLayoutAccOpts{
                .label = "CONTROL LAYOUT",
                .defaultOpen = false,
                .size = guikit::UiSize::Sm,
                .disabled = !hasCtrlLayout
            };
            if (!hasCtrlLayout) {
                ctrlLayoutAccOpts.badge = {"Fit", guikit::UiVariant::Secondary};
            }
            if (auto ctrlLayoutAcc = ui.Accordion(
                hasCtrlLayout ? m_demoCtx.ctrlLayoutOpen : collapsedOpen,
                ctrlLayoutAccOpts
            )) {
                m_demoCtx.RenderControlLayoutSection(ui, m_selectedCategory);
            }

            if (auto ctrlStyleAcc = ui.Accordion({
                .label = "CONTROL STYLE",
                .defaultOpen = false,
                .size = guikit::UiSize::Sm
            })) {
                m_demoCtx.RenderControlStyleSection(ui, m_selectedCategory);
            }

            if (auto specAcc = ui.Accordion({
                .label = "CONTROL SPECIFIC",
                .badge = {CategoryControlName(m_selectedCategory), guikit::UiVariant::Secondary},
                .defaultOpen = true,
                .size = guikit::UiSize::Sm
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

        if (ui.Select(m_demoCtx.previewFormFactorIdx, kPreviewFormFactorNames, {.size = guikit::UiSize::Sm, .width = 96.0f}).changed) {
            if (m_demoCtx.previewFormFactorIdx == 1) {
                m_demoCtx.previewLandscape = true;  // Tablet defaults to Landscape
                if (m_demoCtx.previewZoomModeIdx == static_cast<keel::usize>(PreviewZoomMode::Zoom100)) {
                    m_demoCtx.previewZoomModeIdx = static_cast<keel::usize>(PreviewZoomMode::Fit);
                }
            } else if (m_demoCtx.previewFormFactorIdx == 2) {
                m_demoCtx.previewLandscape = false; // Phone defaults to Portrait
                if (m_demoCtx.previewZoomModeIdx == static_cast<keel::usize>(PreviewZoomMode::Zoom100)) {
                    m_demoCtx.previewZoomModeIdx = static_cast<keel::usize>(PreviewZoomMode::Fit);
                }
            }
            ui.InvalidateAll();
        }

        static constexpr keel::StaticArray<keel::StringRef, 2> kOrientationVariants = {"Port", "Land"};
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

        const keel::i32 effZoomPct = keel::round_i32(m_demoCtx.previewEffectiveZoom * 100.0f);
        guikit::SelectOptions zoomSelectOpts{
            .tooltip = "Preview Zoom / Scale (Fit, Real 1:1 physical mm, or Ctrl + MouseWheel)",
            .size = guikit::UiSize::Sm,
            .width = 116.0f
        };
        if (m_demoCtx.previewZoomModeIdx == static_cast<keel::usize>(PreviewZoomMode::Fit) ||
            m_demoCtx.previewZoomModeIdx == static_cast<keel::usize>(PreviewZoomMode::Real1To1)) {
            zoomSelectOpts.suffix.text = {ui.Format("{}%", effZoomPct), guikit::StateColor::Muted()};
        }

        bool zoomChanged = false;
        if (m_demoCtx.previewZoomModeIdx >= kPreviewZoomPresetNames.size()) {
            const keel::StringRef customLabel = ui.AllocString(ui.Format("{}%", effZoomPct));
            const keel::StaticArray<keel::StringRef, 9> zoomItemsWithCustom = {
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
            zoomChanged = ui.Select(m_demoCtx.previewZoomModeIdx, zoomItemsWithCustom, zoomSelectOpts).changed;
        } else {
            zoomChanged = ui.Select(m_demoCtx.previewZoomModeIdx, kPreviewZoomPresetNames, zoomSelectOpts).changed;
        }
        if (zoomChanged) {
            ui.InvalidateAll();
        }

        if (ui.Switch(m_demoCtx.previewLightMode, {
            .textLeft = "Dark",
            .textRight = "Light",
            .symmetric = true,
            .size = guikit::UiSize::Md
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

        if (m_selectedExtension >= 0 || IsSandboxCategory(m_selectedCategory)) {
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
        .allowCollapse = true
    })) {
        if (auto codeTb = ui.Toolbar({
            .style = guikit::ToolbarStyle::Header,
            .size = guikit::UiSize::Sm
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

        if (!IsLiveResizeFrame() || m_cachedCodeSnippet.empty() ||
            m_cachedCodeCategory != m_selectedCategory || m_cachedCodeExtension != m_selectedExtension) {
            if (m_selectedExtension >= 0 && static_cast<keel::usize>(m_selectedExtension) < m_extensions.size()) {
                auto fn = m_extensions[static_cast<keel::usize>(m_selectedExtension)].generateCode;
                m_cachedCodeSnippet = fn ? fn(m_demoCtx) : keel::String{};
            } else {
                m_cachedCodeSnippet = DispatchCodeGenerator(m_selectedCategory, m_demoCtx);
            }
            m_cachedCodeCategory = m_selectedCategory;
            m_cachedCodeExtension = m_selectedExtension;
        }
        ui.TextArea(m_cachedCodeSnippet, {
            .autoHeight = false,
            .readOnly = true,
            .showLineNumbers = true,
            .showCopyButton = true,
            .monospace = true,
            .allowZoom = true,
            .syntax = guikit::SyntaxConfig::Cpp(),
            .height = guikit::SizeMode::Stretch,
            .background = guikit::StateColor{},
            .border = {.radius = 0.0f}
        });
    }
}

void DemoWindow::RenderWorkspace(guikit::Context& ui) {
    if (auto workspaceRow = ui.Row({.height = guikit::SizeMode::Stretch})) {
        RenderCatalogSidebar(ui);
        RenderInspectorSidebar(ui);

        if (auto contentCol = ui.Column({
            .width = guikit::SizeMode::Stretch,
            .height = guikit::SizeMode::Stretch
        })) {
            RenderPreviewToolbar(ui);

            const bool extActive = (m_selectedExtension >= 0 && static_cast<keel::usize>(m_selectedExtension) < m_extensions.size());
            if (extActive || IsSandboxCategory(m_selectedCategory)) {
                const bool isFullStage = extActive ? true : IsFullStageCategory(m_selectedCategory);
                const bool isShellScreen = !extActive && (m_selectedCategory == NavCategory::PanelSidebar);
                const keel::StringRef title = extActive
                    ? m_extensions[static_cast<keel::usize>(m_selectedExtension)].title
                    : CategoryDisplayTitle(m_selectedCategory);
                RenderPreviewViewport(ui, m_demoCtx, isFullStage, isShellScreen, title, [&]() {
                    if (extActive) {
                        if (auto fn = m_extensions[static_cast<keel::usize>(m_selectedExtension)].renderStage) {
                            fn(ui, m_demoCtx);
                        }
                    } else {
                        DispatchSandboxStage(ui, m_selectedCategory, m_demoCtx);
                    }
                });
                RenderCodePanel(ui);
            }
        }
    }
}

void DemoWindow::RenderBottomStatusBar(guikit::Context& ui) {
    if (auto sb = ui.StatusBar()) {
        const bool isGpuScene = (m_selectedExtension >= 0 &&
                                 static_cast<keel::usize>(m_selectedExtension) < m_extensions.size() &&
                                 m_extensions[static_cast<keel::usize>(m_selectedExtension)].isGpuScene);
        RenderTelemetryStatus(sb, isGpuScene);

        sb.Spacer();

        static const keel::StaticArray<guikit::CommandPaletteItem, 15> kGlobalCommands = {{
            {.label = "Go to Element Form-Field Wrapper",      .category = "Structure",  .description = "5 ElementLayout modes & composite slots", .icon = guikit::Icon::Settings()},
            {.label = "Go to LevelMeter / Segmented VU Meter", .category = "Controls",   .description = "LED dB meters, peak-hold & clip LED", .icon = guikit::Icon::Play()},
            {.label = "Go to TagInput Editable Chips",         .category = "Controls",   .description = "Editable tag pills & delimiter split",.icon = guikit::Icon::Edit()},
            {.label = "Go to Select / MultiSelect / ComboBox", .category = "Controls",   .description = "Dropdown, live filter & tag chips",   .icon = guikit::Icon::Plus()},
            {.label = "Go to DatePicker & TimePicker",         .category = "Controls",   .description = "Calendar grid & time stepper",        .icon = guikit::Icon::Edit()},
            {.label = "Go to SpinBox Numeric Stepper",         .category = "Controls",   .description = "Inline, Detached & Split steppers",   .icon = guikit::Icon::Plus()},
            {.label = "Go to ColorPicker HSV/HEX",             .category = "Controls",   .description = "2D SV canvas, hue & alpha sliders",   .icon = guikit::Icon::Eye()},
            {.label = "Go to SplitView Resizable Panes",       .category = "Containers", .description = "Horizontal & Vertical splitters",     .icon = guikit::Icon::Copy()},
            {.label = "Go to Breadcrumb Path Navigation",      .category = "Navigation", .description = "Hierarchical path trail & collapse",  .icon = guikit::Icon::Folder()},
            {.label = "Go to Pagination Control",              .category = "Navigation", .description = "Page numbers & smart ellipsis jump",  .icon = guikit::Icon::ArrowRight()},
            {.label = "Go to Overlays (Modal / Toast / Cmd)",  .category = "Containers", .description = "Modals, context menus & palettes",    .icon = guikit::Icon::Flash()},
            {.label = "Go to Table / Virtualized Grid",        .category = "Data",       .description = "100,000 rows virtualized data grid",  .icon = guikit::Icon::Copy()},
            {.label = "Toggle Preview Dark / Light Theme",     .category = "Theme",      .description = "Switch stage palette mode",           .icon = guikit::Icon::Refresh()},
            {.label = "Toggle Generated C++20 Code Panel",     .category = "View",       .description = "Show or hide bottom code generator",  .icon = guikit::Icon::Eye()},
            {.label = "Toggle Docked / Floating Island Shell", .category = "View",       .description = "Switch between flush IDE & floating island frame", .icon = guikit::Icon::Copy()}
        }};

        const auto cmdResp = RenderCommandPalette(sb, kGlobalCommands);

        if (m_selectedExtension >= 0 || IsSandboxCategory(m_selectedCategory)) {
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
                case 0:  m_selectedExtension = -1; m_selectedCategory = NavCategory::ElemElement; break;
                case 1:  m_selectedExtension = -1; m_selectedCategory = NavCategory::ElemLevelMeter; break;
                case 2:  m_selectedExtension = -1; m_selectedCategory = NavCategory::ElemTagInput; break;
                case 3:  m_selectedExtension = -1; m_selectedCategory = NavCategory::ElemSelect; break;
                case 4:  m_selectedExtension = -1; m_selectedCategory = NavCategory::ElemDatePicker; break;
                case 5:  m_selectedExtension = -1; m_selectedCategory = NavCategory::ElemSpinBox; break;
                case 6:  m_selectedExtension = -1; m_selectedCategory = NavCategory::ElemColorPicker; break;
                case 7:  m_selectedExtension = -1; m_selectedCategory = NavCategory::PanelSplitView; break;
                case 8:  m_selectedExtension = -1; m_selectedCategory = NavCategory::PanelBreadcrumb; break;
                case 9:  m_selectedExtension = -1; m_selectedCategory = NavCategory::PanelPagination; break;
                case 10: m_selectedExtension = -1; m_selectedCategory = NavCategory::PanelOverlays; break;
                case 11: m_selectedExtension = -1; m_selectedCategory = NavCategory::PanelTable; break;
                case 12:
                    m_demoCtx.previewLightMode = !m_demoCtx.previewLightMode;
                    m_demoCtx.previewDarkMode = !m_demoCtx.previewLightMode;
                    m_demoCtx.previewTheme.SetMode(m_demoCtx.previewDarkMode
                        ? guikit::ThemeMode::Dark
                        : guikit::ThemeMode::Light);
                    break;
                case 13:
                    m_demoCtx.previewShowCode = !m_demoCtx.previewShowCode;
                    break;
                case 14:
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
    static constexpr keel::StaticArray<InspectorFn, 31> kInspectors = {
        // 1. ELEMENTS
        &RenderHeadingInspector,
        &RenderElementInspector,
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
        &RenderImageViewInspector,
    };
    static_assert(kInspectors.size() == static_cast<keel::usize>(NavCategory::SceneImageView) + 1);

    const keel::usize idx = static_cast<keel::usize>(cat);
    if (idx < kInspectors.size()) {
        kInspectors[idx](ui, ctx);
    }
}

void DemoWindow::DispatchSandboxStage(guikit::Context& ui, NavCategory cat, DemoContext& ctx) {
    using StageFn = void (*)(guikit::Context&, DemoContext&);
    static constexpr keel::StaticArray<StageFn, 31> kStages = {
        // 1. ELEMENTS
        &RenderHeadingStage,
        &RenderElementStage,
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
        &RenderImageViewStage,
    };
    static_assert(kStages.size() == static_cast<keel::usize>(NavCategory::SceneImageView) + 1);

    const keel::usize idx = static_cast<keel::usize>(cat);
    if (idx < kStages.size()) {
        kStages[idx](ui, ctx);
    }
}

keel::String DemoWindow::DispatchCodeGenerator(NavCategory cat, const DemoContext& ctx) {
    using CodeGenFn = keel::String (*)(const DemoContext&);
    static constexpr keel::StaticArray<CodeGenFn, 31> kGenerators = {
        // 1. ELEMENTS
        &GenerateHeadingCode,
        &GenerateElementCode,
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
        &GenerateImageViewCode,
    };
    static_assert(kGenerators.size() == static_cast<keel::usize>(NavCategory::SceneImageView) + 1);

    const keel::usize idx = static_cast<keel::usize>(cat);
    if (idx < kGenerators.size()) {
        return kGenerators[idx](ctx);
    }
    return {};
}

} // namespace guikit::demo
