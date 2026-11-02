#include "../demo_context.hpp"
#include <array>
#include <format>

namespace guikit::demo {

namespace {

struct BreadcrumbDemoState {
    size_t crumbIdx{4};
    size_t separatorIdx{0};
    guikit::BreadcrumbSeparator separator{guikit::BreadcrumbSeparator::Chevron};
    int maxVisibleItems{0}; // 0 = show all, 2..5 = collapse middle items into "..."
    bool showCrumbIcons{true};
    bool showCrumbBadges{true};
    bool showSurfaceBox{false};
};

BreadcrumbDemoState s_bc;

constexpr std::string_view SeparatorEnumName(guikit::BreadcrumbSeparator s) noexcept {
    switch (s) {
        case guikit::BreadcrumbSeparator::Chevron: return "Chevron";
        case guikit::BreadcrumbSeparator::Slash:   return "Slash";
        case guikit::BreadcrumbSeparator::Arrow:   return "Arrow";
        case guikit::BreadcrumbSeparator::Dot:     return "Dot";
    }
    return "Chevron";
}

} // namespace

void RenderBreadcrumbInspector(guikit::Context& ui, DemoContext& ctx) {
    if (auto el = ui.Element({.label = "Control Size"})) {
        if (ui.StepSlider(ctx.controlsSize, {.showTicks = true, .showLabels = true, .includeNone = true}).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Semantic Variant"})) {
        if (ui.StepSlider(ctx.btnVariant, {.showTicks = true, .showLabels = true}).changed) {
            ui.InvalidateAll();
        }
    }

    ui.Size(guikit::UiSize::Xs).Heading({
        .label = "Breadcrumb Options",
        .divider = true
    });

    static constexpr std::array<std::string_view, 4> kSepNames = {
        "Chevron", "Slash", "Arrow", "Dot"
    };
    if (auto el = ui.Element({.label = "Separator Style"})) {
        if (ui.Segmented(s_bc.separatorIdx, kSepNames).changed) {
            s_bc.separator = static_cast<guikit::BreadcrumbSeparator>(s_bc.separatorIdx);
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({
        .label = "Max Visible Items",
        .description = "0 = show all; >= 2 collapses middle items into '...'"
    })) {
        if (ui.Slider(s_bc.maxVisibleItems, 0, 5, {
            .valueFormat = (s_bc.maxVisibleItems < 2) ? "All" : "%d items"
        }).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({.label = "Show Item Icons"})) {
        if (ui.Switch(s_bc.showCrumbIcons).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Show Item Badges"})) {
        if (ui.Switch(s_bc.showCrumbBadges).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Surface Well Frame"})) {
        if (ui.Switch(s_bc.showSurfaceBox).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Disabled State"})) {
        if (ui.Switch(ctx.elemDisabled).changed) ui.InvalidateAll();
    }
}

void RenderBreadcrumbStage(guikit::Context& ui, DemoContext& ctx) {
    const guikit::StateColor activeCol = guikit::StateColor::FromVariant(ctx.btnVariant);
    const uint32_t maxVis = (s_bc.maxVisibleItems >= 2) ? static_cast<uint32_t>(s_bc.maxVisibleItems) : 0u;

    const std::array<guikit::BreadcrumbItem, 5> crumbs = {{
        {.label = "Workspace",  .icon = s_bc.showCrumbIcons ? guikit::Icon::Folder()   : guikit::Icon{}},
        {.label = "Clusters",   .icon = s_bc.showCrumbIcons ? guikit::Icon::Settings() : guikit::Icon{}, .badge = s_bc.showCrumbBadges ? "EU" : ""},
        {.label = "Production", .icon = s_bc.showCrumbIcons ? guikit::Icon::Flash()    : guikit::Icon{}},
        {.label = "Telemetry",  .icon = s_bc.showCrumbIcons ? guikit::Icon::Search()   : guikit::Icon{}, .badge = s_bc.showCrumbBadges ? "v2.4" : ""},
        {.label = "LiveStream", .icon = s_bc.showCrumbIcons ? guikit::Icon::Play()     : guikit::Icon{}}
    }};

    ui.Size(ctx.controlsSize).Breadcrumb(s_bc.crumbIdx, crumbs, {
        .separator = s_bc.separator,
        .maxVisibleItems = maxVis,
        .color = activeCol,
        .background = s_bc.showSurfaceBox ? guikit::StateColor::Surface() : guikit::StateColor::None(),
        .border = s_bc.showSurfaceBox ? guikit::Border{.width = 1.0f, .radius = 8.0f} : guikit::Border::None(),
        .disabled = ctx.elemDisabled
    });
}

std::string GenerateBreadcrumbCode(const DemoContext& ctx) {
    const std::string sizePrefix = FormatUiSizePrefix(ctx.controlsSize);
    const uint32_t maxVis = (s_bc.maxVisibleItems >= 2) ? static_cast<uint32_t>(s_bc.maxVisibleItems) : 0u;

    return std::format(
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
        SeparatorEnumName(s_bc.separator),
        maxVis,
        VariantName(ctx.btnVariant),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
