#include "../DemoContext.hpp"
#include "guikit/widgets/Breadcrumb.hpp"
#include "guikit/widgets/Pagination.hpp"
#include <array>
#include <format>

namespace guikit::demo {

namespace {

struct NavigationDemoState {
    size_t widgetModeIdx{0}; // 0 = Breadcrumb, 1 = Pagination, 2 = Both

    // Breadcrumb configuration
    size_t crumbIdx{4};
    guikit::BreadcrumbSeparator separator{guikit::BreadcrumbSeparator::Chevron};
    int maxVisibleItems{0}; // 0 = show all, 2..5 = collapse middle items into "..."
    bool showCrumbIcons{true};
    bool showCrumbBadges{true};
    bool showSurfaceBox{false};

    // Pagination configuration
    size_t currentPage{2};
    int totalPages{12};
    int siblingCount{1};
    bool showFirstLast{true};
    bool showPrevNext{true};
    bool showPageInfo{true};
};

NavigationDemoState s_nav;

constexpr std::string_view SeparatorEnumName(guikit::BreadcrumbSeparator sep) noexcept {
    switch (sep) {
        case guikit::BreadcrumbSeparator::Chevron: return "Chevron";
        case guikit::BreadcrumbSeparator::Slash:   return "Slash";
        case guikit::BreadcrumbSeparator::Arrow:   return "Arrow";
        case guikit::BreadcrumbSeparator::Dot:     return "Dot";
    }
    return "Chevron";
}

} // namespace

void RenderNavigationInspector(guikit::Context& ui, DemoContext& ctx) {
    static constexpr std::array<std::string_view, 3> kWidgetModes = {"Breadcrumb", "Pagination", "Both"};
    if (auto el = ui.Element({.label = "Displayed Control", .layout = guikit::ElementLayout::Stacked})) {
        if (ui.Segmented(s_nav.widgetModeIdx, kWidgetModes).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Control Size"})) {
        if (ui.StepSlider(ctx.controlsSize, {.showTicks = true, .showLabels = true, .includeNone = true}).changed) {
            ui.InvalidateAll();
        }
    }
    ctx.RenderVariantPicker(ui);

    const bool showBc = (s_nav.widgetModeIdx == 0 || s_nav.widgetModeIdx == 2);
    const bool showPg = (s_nav.widgetModeIdx == 1 || s_nav.widgetModeIdx == 2);

    if (showBc) {
        ui.Size(guikit::UiSize::Xs).Heading({
            .label = "Breadcrumb Options",
            .divider = true
        });

        static constexpr std::array<std::string_view, 4> kSepLabels = {"Chevron", "Slash", "Arrow", "Dot"};
        size_t sepIdx = static_cast<size_t>(s_nav.separator);
        if (auto el = ui.Element({.label = "Separator Style", .layout = guikit::ElementLayout::Stacked})) {
            if (ui.Segmented(sepIdx, kSepLabels).changed) {
                s_nav.separator = static_cast<guikit::BreadcrumbSeparator>(sepIdx);
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({
            .label = "Max Visible Items",
            .description = "0 = show all; >= 2 collapses middle items into '...'"
        })) {
            if (ui.Slider(s_nav.maxVisibleItems, 0, 5, {
                .valueFormat = (s_nav.maxVisibleItems == 0)
                    ? guikit::UiText{"All", {"All", "%d items"}}
                    : guikit::UiText{"%d items", {"All", "%d items"}}
            }).changed) {
                if (s_nav.maxVisibleItems == 1) s_nav.maxVisibleItems = 2;
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "Show Item Icons"})) {
            if (ui.Switch(s_nav.showCrumbIcons).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Show Item Badges"})) {
            if (ui.Switch(s_nav.showCrumbBadges).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Surface Well Frame"})) {
            if (ui.Switch(s_nav.showSurfaceBox).changed) ui.InvalidateAll();
        }
    }

    if (showPg) {
        ui.Size(guikit::UiSize::Xs).Heading({
            .label = "Pagination Options",
            .divider = true
        });

        if (auto el = ui.Element({.label = "Total Pages"})) {
            if (ui.Slider(s_nav.totalPages, 3, 30, {.valueFormat = "%d pages"}).changed) {
                if (s_nav.currentPage >= static_cast<size_t>(s_nav.totalPages)) {
                    s_nav.currentPage = static_cast<size_t>(s_nav.totalPages - 1);
                }
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({
            .label = "Sibling Count",
            .description = "Neighbors shown on each side of active page"
        })) {
            if (ui.Slider(s_nav.siblingCount, 0, 3, {.valueFormat = "%d"}).changed) {
                ui.InvalidateAll();
            }
        }

        if (auto el = ui.Element({.label = "First / Last Buttons (« »)"})) {
            if (ui.Switch(s_nav.showFirstLast).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Prev / Next Buttons (‹ ›)"})) {
            if (ui.Switch(s_nav.showPrevNext).changed) ui.InvalidateAll();
        }
        if (auto el = ui.Element({.label = "Page Info Summary"})) {
            if (ui.Switch(s_nav.showPageInfo).changed) ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Disabled State"})) {
        if (ui.Switch(ctx.elemDisabled).changed) ui.InvalidateAll();
    }
}

void RenderNavigationStage(guikit::Context& ui, DemoContext& ctx) {
    const guikit::StateColor activeCol = guikit::StateColor::FromVariant(ctx.btnVariant);
    const uint32_t maxVis = (s_nav.maxVisibleItems >= 2) ? static_cast<uint32_t>(s_nav.maxVisibleItems) : 0u;
    const bool showBc = (s_nav.widgetModeIdx == 0 || s_nav.widgetModeIdx == 2);
    const bool showPg = (s_nav.widgetModeIdx == 1 || s_nav.widgetModeIdx == 2);

    if (auto col = ui.Column({
        .gap = 24.0f,
        .align = guikit::Alignment::Center,
        .widthMode = guikit::SizeMode::Fit
    })) {
        if (showBc) {
            std::array<guikit::BreadcrumbItem, 5> crumbs = {{
                {.label = "Workspace",  .icon = s_nav.showCrumbIcons ? guikit::Icon::Folder()   : guikit::Icon{}},
                {.label = "Clusters",   .icon = s_nav.showCrumbIcons ? guikit::Icon::Settings() : guikit::Icon{}, .badge = s_nav.showCrumbBadges ? "EU" : ""},
                {.label = "Production", .icon = s_nav.showCrumbIcons ? guikit::Icon::Flash()    : guikit::Icon{}},
                {.label = "Telemetry",  .icon = s_nav.showCrumbIcons ? guikit::Icon::Search()   : guikit::Icon{}, .badge = s_nav.showCrumbBadges ? "v2.4" : ""},
                {.label = "LiveStream", .icon = s_nav.showCrumbIcons ? guikit::Icon::Play()     : guikit::Icon{}}
            }};

            auto sz = ui.Size(ctx.controlsSize);
            guikit::Breadcrumb::Run(ui, s_nav.crumbIdx, std::span<const guikit::BreadcrumbItem>(crumbs), {
                .separator = s_nav.separator,
                .maxVisibleItems = maxVis,
                .color = activeCol,
                .background = s_nav.showSurfaceBox ? guikit::StateColor::Surface() : guikit::StateColor::None(),
                .border = s_nav.showSurfaceBox ? guikit::Border{.width = 1.0f, .radius = 8.0f} : guikit::Border::None(),
                .disabled = ctx.elemDisabled
            });
        }

        if (showPg) {
            const size_t totalPages = static_cast<size_t>(std::max(1, s_nav.totalPages));
            if (s_nav.currentPage >= totalPages) {
                s_nav.currentPage = totalPages - 1;
            }

            ui.Size(ctx.controlsSize).Pagination(s_nav.currentPage, totalPages, {
                .siblingCount = static_cast<uint32_t>(std::max(0, s_nav.siblingCount)),
                .showFirstLast = s_nav.showFirstLast,
                .showPrevNext = s_nav.showPrevNext,
                .showPageInfo = s_nav.showPageInfo,
                .color = activeCol,
                .disabled = ctx.elemDisabled
            });
        }
    }
}

std::string GenerateNavigationCode(const DemoContext& ctx) {
    const std::string sizePrefix = FormatUiSizePrefix(ctx.controlsSize);
    const uint32_t maxVis = (s_nav.maxVisibleItems >= 2) ? static_cast<uint32_t>(s_nav.maxVisibleItems) : 0u;
    const bool showBc = (s_nav.widgetModeIdx == 0 || s_nav.widgetModeIdx == 2);
    const bool showPg = (s_nav.widgetModeIdx == 1 || s_nav.widgetModeIdx == 2);

    std::string bcCode;
    if (showBc) {
        bcCode = std::format(
            "static const std::array<guikit::BreadcrumbItem, 5> crumbs = {{{{\n"
            "    {{.label = \"Workspace\",  .icon = guikit::Icon::Folder()}},\n"
            "    {{.label = \"Clusters\",   .icon = guikit::Icon::Settings(), .badge = \"EU\"}},\n"
            "    {{.label = \"Production\", .icon = guikit::Icon::Flash()}},\n"
            "    {{.label = \"Telemetry\",  .icon = guikit::Icon::Search(),   .badge = \"v2.4\"}},\n"
            "    {{.label = \"LiveStream\", .icon = guikit::Icon::Play()}}\n"
            "}}}};\n"
            "ui.{}Breadcrumb(activeCrumb, crumbs, {{\n"
            "    .separator = guikit::BreadcrumbSeparator::{},\n"
            "    .maxVisibleItems = {},\n"
            "    .color = guikit::StateColor::{}(),\n"
            "    .disabled = {}\n"
            "}});",
            sizePrefix,
            SeparatorEnumName(s_nav.separator),
            maxVis,
            VariantName(ctx.btnVariant),
            ctx.elemDisabled ? "true" : "false"
        );
    }

    std::string pgCode;
    if (showPg) {
        pgCode = std::format(
            "ui.{}Pagination(currentPage, {}, {{\n"
            "    .siblingCount = {},\n"
            "    .showFirstLast = {},\n"
            "    .showPrevNext = {},\n"
            "    .showPageInfo = {},\n"
            "    .color = guikit::StateColor::{}(),\n"
            "    .disabled = {}\n"
            "}});",
            sizePrefix,
            s_nav.totalPages,
            s_nav.siblingCount,
            s_nav.showFirstLast ? "true" : "false",
            s_nav.showPrevNext ? "true" : "false",
            s_nav.showPageInfo ? "true" : "false",
            VariantName(ctx.btnVariant),
            ctx.elemDisabled ? "true" : "false"
        );
    }

    if (showBc && showPg) {
        return bcCode + "\n\n" + pgCode;
    }
    return showBc ? bcCode : pgCode;
}

} // namespace guikit::demo
