#include "guikit/layout/column.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

ContainerScope Column::Open(
    Context& ctx,
    const ColumnOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    UiTheme* zoneTheme = ctx.PushZone({.theme = opts.theme, .scale = opts.scale, .disabled = opts.disabled});

    Node* node = ctx.CreateNode(NodeType::Column, id);
    const UiTheme& theme = ctx.GetTheme();
    node->theme = zoneTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Column;
    node->explicitGap = opts.gap.has_value() ? 1 : 0;
    node->explicitPadding = opts.padding.has_value() ? 1 : 0;
    node->gap = theme.ResolveGap(opts.gap, activeSize);
    node->padding = theme.ResolvePadding(opts.padding);
    node->align = opts.align;
    node->justify = opts.justify;
    node->widthMode = opts.width.ResolveMode(SizeMode::Stretch);
    node->fixedWidth = opts.width.IsFixed() ? theme.Scale(opts.width.Dp()) : 0.0f;
    node->heightMode = opts.height.ResolveMode(SizeMode::Stretch);
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

    ctx.PushContainerInZone(node);
    return ContainerScope(&ctx, node);
}

} // namespace guikit
