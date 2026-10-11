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

    Node* node = ctx.CreateNode(NodeType::Box, id);
    node->theme = pushedTheme;
    node->direction = FlexDirection::Column;
    node->gap = opts.gap.has_value() ? ctx.GetTheme().GetGap(*opts.gap) : ctx.GetTheme().GetGap(activeSize);
    node->padding = ctx.GetTheme().ResolvePadding(opts.padding);
    node->backgroundColor = opts.backgroundColor;
    node->borderColor = opts.borderColor;
    node->borderWidth = opts.borderWidth;
    node->cornerRadius = opts.radius.has_value() ? ctx.GetTheme().GetRadius(*opts.radius) : 0.0f;
    node->clipChildren = opts.clipChildren;
    node->widthMode = opts.widthMode;
    node->heightMode = opts.heightMode;

    if (opts.fixedWidth > 0.0f) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = opts.fixedWidth;
    }
    if (opts.fixedHeight > 0.0f) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = opts.fixedHeight;
    }
    if (opts.size.has_value() && opts.fixedHeight <= 0.0f) {
        node->fixedHeight = ctx.GetTheme().GetMetrics(*opts.size).defaultHeight;
        node->heightMode = SizeMode::Fixed;
    }

    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);
    return ContainerScope(&ctx, node);
}

} // namespace guikit
