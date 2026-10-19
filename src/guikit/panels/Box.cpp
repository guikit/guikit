#include "guikit/panels/Box.hpp"
#include "guikit/Context.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

ContainerScope Box::Open(
    Context& ctx,
    const BoxOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const UiTheme& theme = ctx.GetTheme();
    Node* node = ctx.CreateNode(NodeType::Box, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Column;
    node->gap = theme.ResolveGap(opts.gap, activeSize);
    node->padding = theme.ResolvePadding(opts.padding);
    const auto& pal = theme.palette;
    node->backgroundColor = opts.background.IsBackgroundEnabled(false)
        ? (opts.background.HasAny() && !opts.background.IsSurface()
            ? opts.background.Resolve(false, false, activeDisabled, pal.surface)
            : pal.surface)
        : Color::Transparent();
    const float bw = (opts.border.width > 0.0f) ? opts.border.width : (opts.border.color.HasAny() ? 1.0f : 0.0f);
    node->borderWidth = theme.Scale(bw);
    node->borderColor = (bw > 0.0f) ? opts.border.color.Resolve(false, false, activeDisabled, pal.border) : Color::Transparent();
    node->cornerRadius = opts.border.radius.has_value() ? theme.Scale(*opts.border.radius) : 0.0f;
    node->clipChildren = opts.clipChildren;
    node->widthMode = opts.widthMode;
    node->heightMode = opts.heightMode;

    if (opts.fixedWidth > 0.0f) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = theme.Scale(opts.fixedWidth);
    }
    if (opts.fixedHeight > 0.0f) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(opts.fixedHeight);
    }

    node->uiSize = activeSize;
    node->disabled = activeDisabled;
    node->visualHash = detail::HashArgs(opts.background, opts.border);

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);
    return ContainerScope(&ctx, node);
}

} // namespace guikit
