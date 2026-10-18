#include "guikit/layout/Grid.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/core/Node.hpp"

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
    node->direction = FlexDirection::Grid;
    node->gridColumns = static_cast<uint8_t>(opts.columns);
    node->minColWidth = theme.Scale(opts.minColWidth);
    node->gap = theme.ResolveGap(opts.gap, activeSize);
    node->runGap = opts.runGap.has_value() ? theme.Scale(*opts.runGap) : node->gap;
    node->padding = theme.ResolvePadding(opts.padding);
    node->align = opts.align;
    node->justify = opts.justify;
    node->widthMode = opts.widthMode;
    node->heightMode = opts.heightMode;
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
    Node* node = m_ctx->CreateNode(NodeType::Cell, id);
    node->theme = pushedTheme;
    node->direction = FlexDirection::Column;
    node->colSpan = std::clamp(span, 1, 12);
    node->minWidth = theme.Scale(opts.minWidth);
    node->clipChildren = opts.clipChildren;
    node->gap = theme.ResolveGap(opts.gap, activeSize);
    node->padding = theme.ResolvePadding(opts.padding);
    node->align = opts.align;
    node->justify = opts.justify;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = opts.heightMode;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    m_ctx->PushScale(activeScale);
    m_ctx->PushSize(activeSize);
    m_ctx->PushDisabled(activeDisabled);
    m_ctx->PushContainer(node);
    return ContainerScope(m_ctx, node);
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
