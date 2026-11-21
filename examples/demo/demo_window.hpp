#pragma once

#include "demo_context.hpp"
#include "guikit/guikit.hpp"
#include "keel/keel.h"

namespace guikit::demo {

/// Optional showcase screen registered by external GuiKit ecosystem modules (geo, signal, cockpit, diagram).
struct DemoExtensionScreen {
    keel::StringRef navLabel{};
    keel::StringRef title{};
    guikit::Icon icon{};
    bool isGpuScene{false};
    void (*renderInspector)(guikit::Context&, DemoContext&){nullptr};
    void (*renderStage)(guikit::Context&, DemoContext&){nullptr};
    keel::String (*generateCode)(const DemoContext&){nullptr};
    void (*releaseBuffers)() noexcept{nullptr};
};

/// Interactive showcase workbench window for the GuiKit C++20 vector GUI library.
class DemoWindow : public guikit::MainWindow {
public:
    DemoWindow();
    ~DemoWindow() override = default;

    /// Registers an additional domain module showcase screen under the "2D SCENES" catalog section.
    void RegisterExtension(DemoExtensionScreen ext);

protected:
    void OnInit() override;
    void OnResize(keel::i32 width, keel::i32 height) override;
    void OnRender(guikit::Context& ui) override;

private:
    void RenderTopToolbar(guikit::Context& ui);
    void RenderCatalogSidebar(guikit::Context& ui);
    void RenderInspectorSidebar(guikit::Context& ui);
    void RenderPreviewToolbar(guikit::Context& ui);
    void RenderCodePanel(guikit::Context& ui);
    void RenderWorkspace(guikit::Context& ui);
    void RenderBottomStatusBar(guikit::Context& ui);

    static void DispatchInspector(guikit::Context& ui, NavCategory cat, DemoContext& ctx);
    static void DispatchSandboxStage(guikit::Context& ui, NavCategory cat, DemoContext& ctx);
    static keel::String DispatchCodeGenerator(NavCategory cat, const DemoContext& ctx);

    guikit::SidebarMode m_navSidebarMode = guikit::SidebarMode::Expanded;
    guikit::SidebarMode m_inspectorSidebarMode = guikit::SidebarMode::Expanded;
    NavCategory m_selectedCategory = NavCategory::ElemHeading;
    keel::i32 m_selectedExtension = -1;
    NavCategory m_prevRenderedCategory = NavCategory::ElemHeading;
    keel::i32 m_prevRenderedExtension = -1;
    NavCategory m_cachedCodeCategory = NavCategory::ElemHeading;
    keel::i32 m_cachedCodeExtension = -1;
    keel::String m_cachedCodeSnippet;
    keel::Array<DemoExtensionScreen> m_extensions;
    DemoContext m_demoCtx;
};

} // namespace guikit::demo
