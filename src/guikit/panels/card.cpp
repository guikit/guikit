#include "guikit/panels/card.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

ContainerScope Card::Open(
    Context& ctx,
    const CardOptions& opts,
    std::source_location loc
) {
    const Id idOuter = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    // Outer styled surface frame
    const UiTheme& theme = ctx.GetTheme();
    Node* outerNode = ctx.CreateNode(NodeType::Card, idOuter);
    outerNode->theme = pushedTheme;
    outerNode->debugBoundsOverride = opts.debugBounds;
    outerNode->inspectModeOverride = opts.inspectMode;
    outerNode->direction = FlexDirection::Column;
    outerNode->gap = theme.ResolveGap(opts.gap, activeSize);
    outerNode->minWidth = theme.Scale(opts.minWidth);
    outerNode->widthMode = opts.widthMode;
    outerNode->heightMode = opts.heightMode;
    if (opts.width.has_value() && *opts.width > 0.0f) {
        outerNode->widthMode = SizeMode::Fixed;
        outerNode->fixedWidth = theme.Scale(*opts.width);
    }
    if (opts.height.has_value() && *opts.height > 0.0f) {
        outerNode->heightMode = SizeMode::Fixed;
        outerNode->fixedHeight = theme.Scale(*opts.height);
    }
    if (opts.maxWidth.has_value() && *opts.maxWidth > 0.0f) {
        outerNode->maxWidth = theme.Scale(*opts.maxWidth);
    }
    if (opts.minHeight.has_value() && *opts.minHeight > 0.0f) {
        outerNode->minHeight = theme.Scale(*opts.minHeight);
    }
    if (opts.maxHeight.has_value() && *opts.maxHeight > 0.0f) {
        outerNode->maxHeight = theme.Scale(*opts.maxHeight);
    }
    outerNode->flexGrow = opts.flexGrow;
    outerNode->padding = theme.ResolvePadding(opts.padding, activeSize);
    outerNode->explicitRadius = opts.border.radius.has_value() ? 1 : 0;
    outerNode->cornerRadius = theme.ResolveRadius(opts.border.radius, activeSize);
    outerNode->clipChildren = opts.clipChildren;
    outerNode->uiSize = activeSize;
    outerNode->disabled = activeDisabled;

    const auto& pal = theme.palette;
    outerNode->backgroundColor = opts.background.IsBackgroundEnabled(true)
        ? (opts.background.HasAny() && !opts.background.IsSurface()
            ? theme.ResolveColor(opts.background, false, false, activeDisabled, pal.surface)
            : pal.surface)
        : Color::Transparent();
    outerNode->borderWidth = theme.Scale(opts.border.width);
    outerNode->borderColor = (opts.border.width > 0.0f)
        ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, pal.border)
        : Color::Transparent();
    outerNode->visualHash = detail::HashArgs(opts.background, opts.border);

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(outerNode);
    return ContainerScope(&ctx, outerNode);
}

} // namespace guikit
