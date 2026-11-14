#include "guikit/layout/grid.hpp"
#include <algorithm>
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

GridScope Grid::Open(
    Context& ctx,
    const GridOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const UiTheme& theme = ctx.GetTheme();
    Node* node = ctx.CreateNode(NodeType::Grid, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Grid;
    node->gridColumns = static_cast<uint8_t>(std::clamp(opts.columns, 1, 24));
    node->minColWidth = theme.Scale(opts.minColWidth);
    node->gap = theme.ResolveGap(opts.gap, activeSize);
    node->runGap = opts.runGap.has_value() ? theme.Scale(*opts.runGap) : node->gap;
    node->padding = theme.ResolvePadding(opts.padding);
    node->align = opts.align;
    node->justify = opts.justify;
    node->widthMode = opts.widthMode;
    node->heightMode = opts.heightMode;
    if (opts.width.has_value() && *opts.width > 0.0f) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = theme.Scale(*opts.width);
    }
    if (opts.height.has_value() && *opts.height > 0.0f) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(*opts.height);
    }
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

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);
    return GridScope(&ctx, node);
}

ContainerScope GridScope::Cell(int span, CellOptions opts, std::source_location loc) {
    if (!m_ctx) return ContainerScope(nullptr, nullptr);

    const Id id = m_ctx->MakeChildId(loc);
    const UiSize activeSize = m_ctx->GetCurrentSize();
    const bool activeDisabled = opts.disabled || m_ctx->IsDisabled();
    const float activeScale = opts.scale.value_or(m_ctx->GetCurrentScale());
    UiTheme* pushedTheme = m_ctx->PushScopedTheme(opts.theme, opts.scale);

    const UiTheme& theme = m_ctx->GetTheme();
    const int maxCols = (m_node && m_node->gridColumns > 0) ? static_cast<int>(m_node->gridColumns) : 12;
    Node* node = m_ctx->CreateNode(NodeType::Cell, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Column;
    node->colSpan = static_cast<uint8_t>(std::clamp(span, 1, maxCols));
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
    node->heightMode = opts.heightMode;
    if (opts.height.has_value() && *opts.height > 0.0f) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(*opts.height);
    }
    if (opts.minHeight.has_value() && *opts.minHeight > 0.0f) {
        node->minHeight = theme.Scale(*opts.minHeight);
    }
    if (opts.maxHeight.has_value() && *opts.maxHeight > 0.0f) {
        node->maxHeight = theme.Scale(*opts.maxHeight);
    }
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    m_ctx->PushScale(activeScale);
    m_ctx->PushSize(activeSize);
    m_ctx->PushDisabled(activeDisabled);
    m_ctx->PushContainer(node);
    return ContainerScope(m_ctx, node);
}

ContainerScope GridScope::Cell(CellOptions opts, std::source_location loc) {
    const int maxCols = (m_node && m_node->gridColumns > 0) ? static_cast<int>(m_node->gridColumns) : 12;
    return Cell(maxCols, opts, loc);
}

ContainerScope GridScope::Cell12(CellOptions opts, std::source_location loc) {
    return Cell(12, opts, loc);
}

ContainerScope GridScope::Cell8(CellOptions opts, std::source_location loc) {
    return Cell(8, opts, loc);
}

ContainerScope GridScope::Cell6(CellOptions opts, std::source_location loc) {
    return Cell(6, opts, loc);
}

ContainerScope GridScope::Cell4(CellOptions opts, std::source_location loc) {
    return Cell(4, opts, loc);
}

ContainerScope GridScope::Cell3(CellOptions opts, std::source_location loc) {
    return Cell(3, opts, loc);
}

ContainerScope GridScope::Cell2(CellOptions opts, std::source_location loc) {
    return Cell(2, opts, loc);
}

ContainerScope GridScope::Cell1(CellOptions opts, std::source_location loc) {
    return Cell(1, opts, loc);
}

} // namespace guikit
