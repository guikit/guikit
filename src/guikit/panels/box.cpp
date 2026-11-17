#include "guikit/panels/box.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

ContainerScope Box::Open(
    Context& ctx,
    const BoxOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    UiTheme* zoneTheme = ctx.PushZone({.theme = opts.theme, .scale = opts.scale, .disabled = opts.disabled});

    const UiTheme& theme = ctx.GetTheme();
    Node* node = ctx.CreateNode(NodeType::Box, id);
    node->theme = zoneTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Column;
    node->explicitGap = opts.gap.has_value() ? 1 : 0;
    node->explicitPadding = opts.padding.has_value() ? 1 : 0;
    node->gap = theme.ResolveGap(opts.gap, activeSize);
    node->padding = theme.ResolvePadding(opts.padding);
    const auto& pal = theme.palette;
    node->backgroundColor = opts.background.IsBackgroundEnabled(false)
        ? theme.ResolveColor(opts.background, false, false, activeDisabled, pal.surface)
        : Color::Transparent();
    const float bw = (opts.border.width > 0.0f) ? opts.border.width : (opts.border.color.HasAny() ? 1.0f : 0.0f);
    node->borderWidth = theme.Scale(bw);
    node->borderColor = (bw > 0.0f) ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, pal.border) : Color::Transparent();
    node->explicitRadius = opts.border.radius.has_value() ? 1 : 0;
    node->cornerRadius = opts.border.radius.has_value() ? theme.Scale(*opts.border.radius) : 0.0f;
    node->clipChildren = opts.clipChildren;
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
    node->visualHash = detail::HashArgs(opts.background, opts.border);

    ctx.PushContainerInZone(node);
    return ContainerScope(&ctx, node);
}

} // namespace guikit
