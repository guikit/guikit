#include "guikit/layout/row.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

ContainerScope detail::Row::Open(
    Context& ctx,
    const RowOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    Node* node = ctx.CreateNode(NodeType::Row, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const keel::f32 density = node->GetDensity();
    node->direction = FlexDirection::Row;
    node->explicitGap = opts.gap.has_value() ? 1 : 0;
    node->explicitPadding = opts.padding.has_value() ? 1 : 0;
    node->isShell = opts.shell;
    if (!opts.gap.has_value() && node->parent != nullptr && node->parent->type == NodeType::Toolbar) {
        const keel::f32 parentDensity = node->parent->GetDensity();
        const keel::f32 relDensity = (parentDensity > 0.0f) ? (density / parentDensity) : density;
        node->gap = node->parent->gap * relDensity;
    } else {
        node->gap = theme.ResolveGap(opts.gap, UiSize::Md, density);
    }
    node->padding = theme.ResolvePadding(opts.padding, UiSize::None, density);
    node->align = opts.align;
    node->justify = opts.justify;
    const bool inHorizontalParent = (node->parent != nullptr) &&
        (node->parent->type == NodeType::Toolbar ||
         (opts.equalWidths && node->parent->direction == FlexDirection::Row));
    node->widthMode = opts.width.ResolveMode(inHorizontalParent ? SizeMode::Fit : SizeMode::Stretch);
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
    node->equalWidths = opts.equalWidths;
    node->uiSize = UiSize::Md;

    ctx.FinishContainer(node);
    return ContainerScope(&ctx, node);
}

} // namespace guikit
