#include "guikit/layout/grid.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

GridScope Grid::Open(
    Context& ctx,
    const GridOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    ZoneScope zone = ctx.Zone({.theme = opts.theme, .scale = opts.scale, .disabled = opts.disabled});

    const UiTheme& theme = ctx.GetTheme();
    Node* node = ctx.CreateNode(NodeType::Grid, id);
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Grid;
    node->gridColumns = static_cast<keel::u8>(keel::clamp(opts.columns, 1, 24));
    node->minColWidth = theme.Scale(opts.minColWidth);
    node->gap = theme.ResolveGap(opts.gap, activeSize);
    node->runGap = opts.runGap.has_value() ? theme.Scale(*opts.runGap) : node->gap;
    node->padding = theme.ResolvePadding(opts.padding);
    node->align = opts.align;
    node->justify = opts.justify;
    node->widthMode = opts.width.ResolveMode(SizeMode::Stretch);
    node->fixedWidth = opts.width.IsFixed() ? theme.Scale(opts.width.Dp()) : 0.0f;
    node->heightMode = opts.height.ResolveMode(SizeMode::Fit);
    node->fixedHeight = opts.height.IsFixed() ? theme.Scale(opts.height.Dp()) : 0.0f;
    if (opts.minWidth.has_value() && *opts.minWidth > 0.0f) {
        node->minWidth = theme.Scale(*opts.minWidth);
    }
    if (opts.maxWidth.has_value() && *opts.maxWidth > 0.0f) {
        node->maxWidth = theme.Scale(*opts.maxWidth);
    }
    if (opts.minHeight.has_value() && *opts.minHeight > 0.0f) {
        node->minHeight = theme.Scale(*opts.minHeight);
    }
    if (opts.maxHeight.has_value() && *opts.maxHeight > 0.0f) {
        node->maxHeight = theme.Scale(*opts.maxHeight);
    }
    node->flexGrow = opts.flexGrow;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    ctx.PushContainer(node, keel::move(zone));
    return GridScope(&ctx, node);
}

ContainerScope GridScope::Cell(keel::i32 span, CellOptions opts, keel::SourceLocation loc) {
    if (!m_ctx) return ContainerScope(nullptr, nullptr);

    const Id id = m_ctx->MakeChildId(loc);
    const UiSize activeSize = m_ctx->GetCurrentSize();
    const bool activeDisabled = opts.disabled || m_ctx->IsDisabled();
    ZoneScope zone = m_ctx->Zone({.theme = opts.theme, .scale = opts.scale, .disabled = opts.disabled});

    const UiTheme& theme = m_ctx->GetTheme();
    const keel::i32 maxCols = (m_node && m_node->gridColumns > 0) ? static_cast<keel::i32>(m_node->gridColumns) : 12;
    Node* node = m_ctx->CreateNode(NodeType::Cell, id);
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Column;
    node->colSpan = static_cast<keel::u8>(keel::clamp(span, 1, maxCols));
    node->minWidth = theme.Scale(opts.minWidth);
    if (opts.maxWidth.has_value() && *opts.maxWidth > 0.0f) {
        node->maxWidth = theme.Scale(*opts.maxWidth);
    }
    node->clipChildren = opts.clipChildren;
    node->gap = theme.ResolveGap(opts.gap, activeSize);
    node->padding = theme.ResolvePadding(opts.padding);
    node->align = opts.align;
    node->justify = opts.justify;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = opts.height.ResolveMode(SizeMode::Fit);
    node->fixedHeight = opts.height.IsFixed() ? theme.Scale(opts.height.Dp()) : 0.0f;
    if (opts.minHeight.has_value() && *opts.minHeight > 0.0f) {
        node->minHeight = theme.Scale(*opts.minHeight);
    }
    if (opts.maxHeight.has_value() && *opts.maxHeight > 0.0f) {
        node->maxHeight = theme.Scale(*opts.maxHeight);
    }
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    m_ctx->PushContainer(node, keel::move(zone));
    return ContainerScope(m_ctx, node);
}

ContainerScope GridScope::Cell(CellOptions opts, keel::SourceLocation loc) {
    const keel::i32 maxCols = (m_node && m_node->gridColumns > 0) ? static_cast<keel::i32>(m_node->gridColumns) : 12;
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
