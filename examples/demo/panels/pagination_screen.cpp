#include "../demo_context.hpp"

namespace guikit::demo {

namespace {

struct PaginationDemoState {
    keel::usize currentPage{4};
    keel::i32 totalPages{20};
    keel::i32 siblingCount{1};
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
        if (ui.Select(ctx.btnVariant, kUiVariantNames).changed) {
            ui.InvalidateAll();
        }
    }

    ui.Heading({
        .label = "Pagination Options",
        .level = guikit::HeadingLevel::H6,
        .separator = guikit::HeadingSeparator::Bottom
    });

    if (auto el = ui.Element({.label = "Total Pages"})) {
        if (ui.Slider(s_pg.totalPages, 3, 30, {.valueFormat = "%d pages"}).changed) {
            if (s_pg.currentPage >= static_cast<keel::usize>(s_pg.totalPages)) {
                s_pg.currentPage = static_cast<keel::usize>(s_pg.totalPages - 1);
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
    const keel::usize totalPages = static_cast<keel::usize>(keel::max(1, s_pg.totalPages));
    if (s_pg.currentPage >= totalPages) {
        s_pg.currentPage = totalPages - 1;
    }

    auto renderPager = [&]() {
        ui.Pagination(s_pg.currentPage, totalPages, {
            .siblingCount = static_cast<keel::u32>(keel::max(0, s_pg.siblingCount)),
            .showFirstLast = s_pg.showFirstLast,
            .showPrevNext = s_pg.showPrevNext,
            .showPageInfo = s_pg.showPageInfo,
            .interactiveEllipsis = s_pg.interactiveEllipsis,
            .disabled = ctx.elemDisabled,
            .size = ctx.controlsSize,
            .color = activeCol
        });
    };

    if (s_pg.showSurfaceBox) {
        if (auto well = ui.Box({
            .width = guikit::SizeMode::Fit,
            .padding = guikit::Padding::Symmetric(10.0f, 6.0f),
            .background = guikit::StateColor::Surface(),
            .border = {.width = 1.0f, .radius = 8.0f}
        })) {
            renderPager();
        }
    } else {
        renderPager();
    }
}

keel::String GeneratePaginationCode(const DemoContext& ctx) {
    const bool hasCustomSize = (ctx.controlsSize != guikit::UiSize::None && ctx.controlsSize != guikit::UiSize::Md);
    const keel::FixedString<64> sizeLineIndented = hasCustomSize
        ? keel::format_fixed<64>("        .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};
    const keel::FixedString<64> sizeLineTop = hasCustomSize
        ? keel::format_fixed<64>("    .size = guikit::UiSize::{},\n", SizeName(ctx.controlsSize))
        : keel::FixedString<64>{};
    const keel::FixedString<64> colorCode = keel::format_fixed<64>("guikit::StateColor::{}()", VariantName(ctx.btnVariant));

    if (s_pg.showSurfaceBox) {
        return keel::format(
            "if (auto well = ui.Box({{\n"
            "    .width = guikit::SizeMode::Fit,\n"
            "    .padding = guikit::Padding::Symmetric(10.0f, 6.0f),\n"
            "    .background = guikit::StateColor::Surface(),\n"
            "    .border = {{.width = 1.0f, .radius = 8.0f}}\n"
            "}})) {{\n"
            "    ui.Pagination(currentPage, {}, {{\n"
            "        .siblingCount = {},\n"
            "        .showFirstLast = {},\n"
            "        .showPrevNext = {},\n"
            "        .showPageInfo = {},\n"
            "        .interactiveEllipsis = {},\n"
            "        .disabled = {},\n"
            "{}"
            "        .color = {}\n"
            "    }});\n"
            "}}",
            s_pg.totalPages,
            s_pg.siblingCount,
            s_pg.showFirstLast ? "true" : "false",
            s_pg.showPrevNext ? "true" : "false",
            s_pg.showPageInfo ? "true" : "false",
            s_pg.interactiveEllipsis ? "true" : "false",
            ctx.elemDisabled ? "true" : "false",
            sizeLineIndented,
            colorCode
        );
    }

    return keel::format(
        "ui.Pagination(currentPage, {}, {{\n"
        "    .siblingCount = {},\n"
        "    .showFirstLast = {},\n"
        "    .showPrevNext = {},\n"
        "    .showPageInfo = {},\n"
        "    .interactiveEllipsis = {},\n"
        "    .disabled = {},\n"
        "{}"
        "    .color = {}\n"
        "}});",
        s_pg.totalPages,
        s_pg.siblingCount,
        s_pg.showFirstLast ? "true" : "false",
        s_pg.showPrevNext ? "true" : "false",
        s_pg.showPageInfo ? "true" : "false",
        s_pg.interactiveEllipsis ? "true" : "false",
        ctx.elemDisabled ? "true" : "false",
        sizeLineTop,
        colorCode
    );
}

} // namespace guikit::demo
