#include "guikit/layout/wrap.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

ContainerScope Wrap::Open(
    Context& ctx,
    const WrapOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    Node* node = ctx.CreateNode(NodeType::Wrap, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const keel::f32 density = node->GetDensity();
    node->direction = FlexDirection::Wrap;
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
    return ContainerScope(&ctx, node);
}

} // namespace guikit
