#include "guikit/layout/Wrap.hpp"
#include "guikit/Context.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

ContainerScope Wrap::Open(
    Context& ctx,
    const WrapOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();

    Node* node = ctx.CreateNode(NodeType::Wrap, id);
    node->direction = FlexDirection::Wrap;
    node->gap = opts.gap.has_value() ? ctx.GetTheme().GetGap(*opts.gap) : ctx.GetTheme().GetGap(activeSize);
    node->runGap = opts.runGap.has_value() ? ctx.GetTheme().GetGap(*opts.runGap) : ctx.GetTheme().GetGap(activeSize);
    node->padding = ctx.GetTheme().ResolvePadding(opts.padding);
    node->align = opts.align;
    node->justify = opts.justify;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = opts.heightMode;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);
    return ContainerScope(&ctx, node);
}

} // namespace guikit
