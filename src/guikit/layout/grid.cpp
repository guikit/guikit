#include "guikit/layout/grid.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

GridScope detail::Grid::Open(
    Context& ctx,
    const GridOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    Node* node = ctx.CreateNode(NodeType::Grid, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const keel::f32 density = node->GetDensity();
    node->direction = FlexDirection::Grid;
    auto* gridData = ctx.AllocPayload<GridData>(GridData{
        .columns = keel::as<keel::u8>(keel::clamp(opts.columns, 1, 24)),
        .minColWidth = opts.minColWidth
    });
    node->SetPayload(gridData);
    node->gap = theme.ResolveGap(opts.gap, UiSize::Md, density);
    node->runGap = opts.runGap.has_value() ? (*opts.runGap * density) : node->gap;
    node->padding = theme.ResolvePadding(opts.padding, UiSize::None, density);
    node->align = opts.align;
    node->justify = opts.justify;
    node->widthMode = opts.width.ResolveMode(SizeMode::Stretch);
    node->fixedWidth = opts.width.IsFixed() ? opts.width.Dp() : 0.0f;
    node->heightMode = opts.height.ResolveMode(SizeMode::Fit);
    node->fixedHeight = opts.height.IsFixed() ? opts.height.Dp() : 0.0f;
    if (opts.minWidth.has_value() && *opts.minWidth > 0.0f) {
        node->minWidth = *opts.minWidth;
    }
    if (opts.maxWidth.has_value() && *opts.maxWidth > 0.0f) {
        node->maxWidth = *opts.maxWidth;
    }
    if (opts.minHeight.has_value() && *opts.minHeight > 0.0f) {
        node->minHeight = *opts.minHeight;
    }
    if (opts.maxHeight.has_value() && *opts.maxHeight > 0.0f) {
        node->maxHeight = *opts.maxHeight;
    }
    node->flexGrow = Control::ResolveFlexGrow(node->parent, opts.width, opts.height, opts.flexGrow);
    node->uiSize = UiSize::Md;

    ctx.FinishContainer(node);
    return GridScope(&ctx, node);
}

ContainerScope GridScope::Cell(keel::i32 span, CellOptions opts, keel::SourceLocation loc) {
    if (!m_ctx) return ContainerScope(nullptr, nullptr);

    const Id id = m_ctx->MakeChildId(loc);
    const keel::i32 maxCols = m_node ? keel::as<keel::i32>(m_node->GetGridColumns()) : 12;
    Node* node = m_ctx->CreateNode(NodeType::Cell, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(m_ctx->GetTheme());
    const keel::f32 density = node->GetDensity();
    node->direction = FlexDirection::Column;
    node->colSpan = keel::as<keel::u16>(keel::clamp(span, 1, maxCols));
    if (opts.minWidth.has_value() && *opts.minWidth > 0.0f) {
        node->minWidth = *opts.minWidth;
    }
    if (opts.maxWidth.has_value() && *opts.maxWidth > 0.0f) {
        node->maxWidth = *opts.maxWidth;
    }
    node->clipChildren = opts.clipChildren;
    node->gap = theme.ResolveGap(opts.gap, UiSize::Md, density);
    node->padding = theme.ResolvePadding(opts.padding, UiSize::None, density);
    node->align = opts.align;
    node->justify = opts.justify;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = opts.height.ResolveMode(SizeMode::Fit);
    node->fixedHeight = opts.height.IsFixed() ? opts.height.Dp() : 0.0f;
    if (opts.minHeight.has_value() && *opts.minHeight > 0.0f) {
        node->minHeight = *opts.minHeight;
    }
    if (opts.maxHeight.has_value() && *opts.maxHeight > 0.0f) {
        node->maxHeight = *opts.maxHeight;
    }
    node->uiSize = UiSize::Md;

    m_ctx->FinishContainer(node);
    return ContainerScope(m_ctx, node);
}

ContainerScope GridScope::Cell(CellOptions opts, keel::SourceLocation loc) {
    const keel::i32 maxCols = m_node ? keel::as<keel::i32>(m_node->GetGridColumns()) : 12;
    return Cell(maxCols, opts, loc);
}

ContainerScope GridScope::Cell12(CellOptions opts, keel::SourceLocation loc) {
    return Cell(12, opts, loc);
}

ContainerScope GridScope::Cell8(CellOptions opts, keel::SourceLocation loc) {
    return Cell(8, opts, loc);
}

ContainerScope GridScope::Cell6(CellOptions opts, keel::SourceLocation loc) {
    return Cell(6, opts, loc);
}

ContainerScope GridScope::Cell4(CellOptions opts, keel::SourceLocation loc) {
    return Cell(4, opts, loc);
}

ContainerScope GridScope::Cell3(CellOptions opts, keel::SourceLocation loc) {
    return Cell(3, opts, loc);
}

ContainerScope GridScope::Cell2(CellOptions opts, keel::SourceLocation loc) {
    return Cell(2, opts, loc);
}

ContainerScope GridScope::Cell1(CellOptions opts, keel::SourceLocation loc) {
    return Cell(1, opts, loc);
}

} // namespace guikit
