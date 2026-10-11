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
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());

    UiTheme* pushedTheme = nullptr;
    if (opts.theme != nullptr || opts.scale.has_value()) {
        UiTheme scopedTheme = opts.theme ? *opts.theme : ctx.GetTheme();
        if (opts.scale.has_value() && *opts.scale > 0.0f) {
            scopedTheme = scopedTheme.Scaled(*opts.scale);
        }
        pushedTheme = ctx.PushTheme(scopedTheme);
    }

    Node* node = ctx.CreateNode(NodeType::Row, id);
    node->theme = pushedTheme;
    node->direction = FlexDirection::Row;
    node->gap = opts.gap.has_value() ? ctx.GetTheme().GetGap(*opts.gap) : ctx.GetTheme().GetGap(activeSize);
    node->padding = ctx.GetTheme().ResolvePadding(opts.padding);
    node->align = opts.align;
    node->justify = opts.justify;
    node->heightMode = opts.heightMode;
    node->widthMode = SizeMode::Stretch;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);
    return ContainerScope(&ctx, node);
}

} // namespace guikit
