#include "guikit/panels/Card.hpp"
#include "guikit/Context.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

ContainerScope Card::Open(
    Context& ctx,
    const CardOptions& opts,
    std::source_location loc
) {
    const Id idOuter = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();

    // Outer styled surface frame
    Node* outerNode = ctx.CreateNode(NodeType::Card, idOuter);
    outerNode->direction = FlexDirection::Column;
    outerNode->gap = opts.gap.has_value() ? ctx.GetTheme().GetGap(*opts.gap) : ctx.GetTheme().GetGap(activeSize);
    outerNode->minWidth = opts.minWidth;
    outerNode->widthMode = opts.widthMode;
    outerNode->heightMode = opts.heightMode;
    outerNode->padding = ctx.GetTheme().ResolvePadding(opts.padding, activeSize);
    outerNode->cornerRadius = opts.radius.has_value() ? ctx.GetTheme().GetRadius(*opts.radius) : ctx.GetTheme().GetRadius(activeSize);
    outerNode->clipChildren = opts.clipChildren;
    outerNode->uiSize = activeSize;
    outerNode->disabled = activeDisabled;

    const auto& pal = ctx.GetTheme().palette;
    outerNode->backgroundColor = (opts.backgroundColor.a > 0) ? opts.backgroundColor : pal.surface;
    outerNode->borderColor = (opts.borderColor.a > 0) ? opts.borderColor : pal.border;
    outerNode->borderWidth = opts.borderWidth;

    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(outerNode);
    return ContainerScope(&ctx, outerNode);
}

} // namespace guikit
