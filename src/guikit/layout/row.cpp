#include "guikit/layout/row.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

ContainerScope Row::Open(
    Context& ctx,
    const RowOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    UiTheme* zoneTheme = ctx.PushZone({.theme = opts.theme, .scale = opts.scale, .disabled = opts.disabled});

    Node* node = ctx.CreateNode(NodeType::Row, id);
    const UiTheme& theme = ctx.GetTheme();
    node->theme = zoneTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Row;
    node->explicitGap = opts.gap.has_value() ? 1 : 0;
    node->explicitPadding = opts.padding.has_value() ? 1 : 0;
    if (!opts.gap.has_value() && node->parent != nullptr && node->parent->type == NodeType::Toolbar) {
        node->gap = node->parent->gap;
    } else {
        node->gap = theme.ResolveGap(opts.gap, activeSize);
    }
    node->padding = theme.ResolvePadding(opts.padding);
    node->align = opts.align;
    node->justify = opts.justify;
    const bool inHorizontalParent = (node->parent != nullptr) &&
        (node->parent->type == NodeType::Toolbar ||
         (opts.equalWidths && node->parent->direction == FlexDirection::Row));
    node->widthMode = opts.width.ResolveMode(inHorizontalParent ? SizeMode::Fit : SizeMode::Stretch);
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
    node->equalWidths = opts.equalWidths;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    ctx.PushContainerInZone(node);
    return ContainerScope(&ctx, node);
}

} // namespace guikit
