#include "../demo_context.hpp"
#include <algorithm>
#include <format>

namespace guikit::demo {

namespace {

struct PaginationDemoState {
    size_t currentPage{4};
    int totalPages{20};
    int siblingCount{1};
    bool showFirstLast{true};
    bool showPrevNext{true};
    bool showPageInfo{true};
    bool interactiveEllipsis{false};
    bool showSurfaceBox{false};
};

PaginationDemoState s_pg;

} // namespace

void RenderPaginationInspector(guikit::Context& ui, DemoContext& ctx) {
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
        .label = "Pagination Options",
        .separator = guikit::HeadingSeparator::Bottom
    });

    if (auto el = ui.Element({.label = "Total Pages"})) {
        if (ui.Slider(s_pg.totalPages, 3, 30, {.valueFormat = "%d pages"}).changed) {
            if (s_pg.currentPage >= static_cast<size_t>(s_pg.totalPages)) {
                s_pg.currentPage = static_cast<size_t>(s_pg.totalPages - 1);
            }
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({
        .label = "Sibling Count",
        .description = "Neighbors shown on each side of active page"
    })) {
        if (ui.Slider(s_pg.siblingCount, 0, 3, {.valueFormat = "%d"}).changed) {
            ui.InvalidateAll();
        }
    }

    if (auto el = ui.Element({
        .label = "Fast-Jump Ellipsis (...)",
        .description = "Off = static separator (MUI/Shadcn); On = fast-jump button (Ant Design)"
    })) {
        if (ui.Switch(s_pg.interactiveEllipsis).changed) ui.InvalidateAll();
    }

    if (auto el = ui.Element({.label = "First / Last Buttons (« »)"})) {
        if (ui.Switch(s_pg.showFirstLast).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Prev / Next Buttons (‹ ›)"})) {
        if (ui.Switch(s_pg.showPrevNext).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Page Info Summary"})) {
        if (ui.Switch(s_pg.showPageInfo).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Surface Well Frame"})) {
        if (ui.Switch(s_pg.showSurfaceBox).changed) ui.InvalidateAll();
    }
    if (auto el = ui.Element({.label = "Disabled State"})) {
        if (ui.Switch(ctx.elemDisabled).changed) ui.InvalidateAll();
    }
}

void RenderPaginationStage(guikit::Context& ui, DemoContext& ctx) {
    const guikit::StateColor activeCol = guikit::StateColor::FromVariant(ctx.btnVariant);
    const size_t totalPages = static_cast<size_t>(std::max(1, s_pg.totalPages));
    if (s_pg.currentPage >= totalPages) {
        s_pg.currentPage = totalPages - 1;
    }

    ui.Size(ctx.controlsSize).Pagination(s_pg.currentPage, totalPages, {
        .siblingCount = static_cast<uint32_t>(std::max(0, s_pg.siblingCount)),
        .showFirstLast = s_pg.showFirstLast,
        .showPrevNext = s_pg.showPrevNext,
        .showPageInfo = s_pg.showPageInfo,
        .interactiveEllipsis = s_pg.interactiveEllipsis,
        .color = activeCol,
        .background = s_pg.showSurfaceBox ? guikit::StateColor::Surface() : guikit::StateColor::None(),
        .border = s_pg.showSurfaceBox ? guikit::Border{.width = 1.0f, .radius = 8.0f} : guikit::Border::None(),
        .disabled = ctx.elemDisabled
    });
}

std::string GeneratePaginationCode(const DemoContext& ctx) {
    const std::string sizePrefix = FormatUiSizePrefix(ctx.controlsSize);

    return std::format(
        "ui.{}Pagination(currentPage, {}, {{\n"
        "    .siblingCount = {},\n"
        "    .showFirstLast = {},\n"
        "    .showPrevNext = {},\n"
        "    .showPageInfo = {},\n"
        "    .interactiveEllipsis = {},\n"
        "    .color = guikit::StateColor::{}(),\n"
        "    .disabled = {}\n"
        "}});",
        sizePrefix,
        s_pg.totalPages,
        s_pg.siblingCount,
        s_pg.showFirstLast ? "true" : "false",
        s_pg.showPrevNext ? "true" : "false",
        s_pg.showPageInfo ? "true" : "false",
        s_pg.interactiveEllipsis ? "true" : "false",
        VariantName(ctx.btnVariant),
        ctx.elemDisabled ? "true" : "false"
    );
}

} // namespace guikit::demo
