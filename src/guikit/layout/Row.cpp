#include "guikit/layout/Row.hpp"
#include "guikit/Context.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

ContainerScope Row::Open(
    Context& ctx,
    const RowOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();

    Node* node = ctx.CreateNode(NodeType::Row, id);
    node->direction = FlexDirection::Row;
    node->gap = opts.gap.has_value() ? ctx.GetTheme().GetGap(*opts.gap) : ctx.GetTheme().GetGap(activeSize);
    node->padding = ctx.GetTheme().ResolvePadding(opts.padding);
    node->align = opts.align;
    node->justify = opts.justify;
    node->heightMode = opts.heightMode;
    node->widthMode = SizeMode::Stretch;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);
    return ContainerScope(&ctx, node);
}

} // namespace guikit
