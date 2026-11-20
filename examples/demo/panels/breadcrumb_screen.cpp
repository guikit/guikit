#include "../demo_context.hpp"

namespace guikit::demo {

namespace {

struct BreadcrumbDemoState {
    keel::usize crumbIdx{4};
    keel::usize separatorIdx{0};
    guikit::BreadcrumbSeparator separator{guikit::BreadcrumbSeparator::Chevron};
    keel::i32 maxVisibleItems{0}; // 0 = show all, 2..5 = collapse middle items into "..."
    bool showCrumbIcons{true};
    bool showCrumbBadges{true};
    bool showSurfaceBox{false};
};

BreadcrumbDemoState s_bc;

constexpr keel::StringRef SeparatorEnumName(guikit::BreadcrumbSeparator s) noexcept {
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
        if (ui.Select(ctx.btnVariant, kUiVariantNames).changed) {
            ui.InvalidateAll();
        }
    }

    ui.Heading({
        .label = "Breadcrumb Options",
        .level = guikit::HeadingLevel::H6,
        .separator = guikit::HeadingSeparator::Bottom
    });

    static constexpr keel::StaticArray<keel::StringRef, 4> kSepNames = {
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
    const keel::u32 maxVis = (s_bc.maxVisibleItems >= 2) ? static_cast<keel::u32>(s_bc.maxVisibleItems) : 0u;

    const keel::StaticArray<guikit::BreadcrumbItem, 5> crumbs = {{
        {.label = "Workspace",  .icon = s_bc.showCrumbIcons ? guikit::Icon::Folder()   : guikit::Icon{}},
        {.label = "Clusters",   .icon = s_bc.showCrumbIcons ? guikit::Icon::Settings() : guikit::Icon{}, .badge = s_bc.showCrumbBadges ? "EU" : ""},
        {.label = "Production", .icon = s_bc.showCrumbIcons ? guikit::Icon::Flash()    : guikit::Icon{}},
        {.label = "Telemetry",  .icon = s_bc.showCrumbIcons ? guikit::Icon::Search()   : guikit::Icon{}, .badge = s_bc.showCrumbBadges ? "v2.4" : ""},
        {.label = "LiveStream", .icon = s_bc.showCrumbIcons ? guikit::Icon::Play()     : guikit::Icon{}}
    }};

    auto renderTrail = [&]() {
        ui.Breadcrumb(s_bc.crumbIdx, crumbs, {
            .separator = s_bc.separator,
            .maxVisibleItems = maxVis,
            .disabled = ctx.elemDisabled,
            .size = ctx.controlsSize,
            .color = activeCol
        });
    };

    if (s_bc.showSurfaceBox) {
        if (auto well = ui.Box({
            .width = guikit::SizeMode::Fit,
            .padding = guikit::Padding::Symmetric(10.0f, 6.0f),
            .background = guikit::StateColor::Surface(),
            .border = {.width = 1.0f, .radius = 8.0f}
        })) {
            renderTrail();
        }
    } else {
        renderTrail();
    }
}

keel::String GenerateBreadcrumbCode(const DemoContext& ctx) {
    const keel::u32 maxVis = (s_bc.maxVisibleItems >= 2) ? static_cast<keel::u32>(s_bc.maxVisibleItems) : 0u;
    const bool hasCustomSize = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md);
    const keel::FixedString<64> sizeLineIndented = hasCustomSize
        ? keel::format_fixed<64>("        .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};
    const keel::FixedString<64> sizeLineTop = hasCustomSize
        ? keel::format_fixed<64>("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};
    const keel::FixedString<64> colorCode = keel::format_fixed<64>("guikit::StateColor::{}()", VariantName(ctx.btnVariant));

    const keel::String crumbsDecl = keel::String::from(
        "static const keel::StaticArray<guikit::BreadcrumbItem, 5> crumbs = {{\n"
        "    {.label = \"Workspace\",  .icon = guikit::Icon::Folder()},\n"
        "    {.label = \"Clusters\",   .icon = guikit::Icon::Settings(), .badge = \"EU\"},\n"
        "    {.label = \"Production\", .icon = guikit::Icon::Flash()},\n"
        "    {.label = \"Telemetry\",  .icon = guikit::Icon::Search(),   .badge = \"v2.4\"},\n"
        "    {.label = \"LiveStream\", .icon = guikit::Icon::Play()}\n"
        "}};\n"
    );

    if (s_bc.showSurfaceBox) {
        return keel::format(
            "{}"
            "if (auto well = ui.Box({{\n"
            "    .width = guikit::SizeMode::Fit,\n"
            "    .padding = guikit::Padding::Symmetric(10.0f, 6.0f),\n"
            "    .background = guikit::StateColor::Surface(),\n"
            "    .border = {{.width = 1.0f, .radius = 8.0f}}\n"
            "}})) {{\n"
            "    ui.Breadcrumb(activeCrumb, crumbs, {{\n"
            "        .separator = guikit::BreadcrumbSeparator::{},\n"
            "        .maxVisibleItems = {},\n"
            "        .disabled = {},\n"
            "{}"
            "        .color = {}\n"
            "    }});\n"
            "}}",
            crumbsDecl,
            SeparatorEnumName(s_bc.separator),
            maxVis,
            ctx.elemDisabled ? "true" : "false",
            sizeLineIndented,
            colorCode
        );
    }

    return keel::format(
        "{}"
        "ui.Breadcrumb(activeCrumb, crumbs, {{\n"
        "    .separator = guikit::BreadcrumbSeparator::{},\n"
        "    .maxVisibleItems = {},\n"
        "    .disabled = {},\n"
        "{}"
        "    .color = {}\n"
        "}});",
        crumbsDecl,
        SeparatorEnumName(s_bc.separator),
        maxVis,
        ctx.elemDisabled ? "true" : "false",
        sizeLineTop,
        colorCode
    );
}

} // namespace guikit::demo
