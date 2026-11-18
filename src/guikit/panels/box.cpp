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
    Node* node = ctx.CreateNode(NodeType::Box, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const float density = node->GetDensity();
    const bool activeDisabled = node->disabled;
    node->direction = FlexDirection::Column;
    node->explicitGap = opts.gap.has_value() ? 1 : 0;
    node->explicitPadding = opts.padding.has_value() ? 1 : 0;
    node->gap = theme.ResolveGap(opts.gap, UiSize::Md, density);
    node->padding = theme.ResolvePadding(opts.padding, UiSize::None, density);
    const auto& pal = theme.palette;
    node->backgroundColor = opts.background.IsBackgroundEnabled(false)
        ? theme.ResolveColor(opts.background, false, false, activeDisabled, pal.surface)
        : Color::Transparent();
    const float bw = (opts.border.width > 0.0f) ? opts.border.width : (opts.border.color.HasAny() ? 1.0f : 0.0f);
    node->borderWidth = bw;
    node->borderColor = (bw > 0.0f) ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, pal.border) : Color::Transparent();
    node->explicitRadius = opts.border.radius.has_value() ? 1 : 0;
    node->cornerRadius = opts.border.radius.has_value() ? *opts.border.radius : 0.0f;
    node->clipChildren = opts.clipChildren;
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
    node->flexGrow = opts.flexGrow;

    node->uiSize = UiSize::Md;
    node->visualHash = detail::HashArgs(opts.background, opts.border);

    ctx.FinishContainer(node);
    return ContainerScope(&ctx, node);
}

} // namespace guikit
