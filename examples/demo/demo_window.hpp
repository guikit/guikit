#pragma once

#include <string>
#include "demo_context.hpp"
#include "guikit/guikit.hpp"

namespace guikit::demo {

/// Interactive showcase workbench window for the GuiKit C++20 vector GUI library.
class DemoWindow final : public guikit::MainWindow {
public:
    DemoWindow();
    ~DemoWindow() override = default;

protected:
    void OnInit() override;
    void OnResize(int width, int height) override;
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
    static std::string DispatchCodeGenerator(NavCategory cat, const DemoContext& ctx);

    guikit::SidebarMode m_navSidebarMode = guikit::SidebarMode::Expanded;
    guikit::SidebarMode m_inspectorSidebarMode = guikit::SidebarMode::Expanded;
    NavCategory m_selectedCategory = NavCategory::ElemHeading;
    NavCategory m_cachedCodeCategory = NavCategory::ElemHeading;
    std::string m_cachedCodeSnippet;
    DemoContext m_demoCtx;
};

} // namespace guikit::demo
