#include "guikit/panels/card.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

ContainerScope Card::Open(
    Context& ctx,
    const CardOptions& opts,
    keel::SourceLocation loc
) {
    const Id idOuter = ctx.MakeChildId(loc);

    // Outer styled surface frame
    Node* outerNode = ctx.CreateNode(NodeType::Card, idOuter, opts.zone, opts.disabled);
    const UiTheme& theme = outerNode->GetTheme(ctx.GetTheme());
    const float density = outerNode->GetDensity();
    const bool activeDisabled = outerNode->disabled;
    outerNode->direction = FlexDirection::Column;
    outerNode->gap = theme.ResolveGap(opts.gap, UiSize::Md, density);
    outerNode->minWidth = opts.minWidth;
    outerNode->widthMode = opts.width.ResolveMode(SizeMode::Stretch);
    outerNode->fixedWidth = opts.width.IsFixed() ? opts.width.Dp() : 0.0f;
    outerNode->heightMode = opts.height.ResolveMode(SizeMode::Fit);
    outerNode->fixedHeight = opts.height.IsFixed() ? opts.height.Dp() : 0.0f;
    if (opts.maxWidth.has_value() && *opts.maxWidth > 0.0f) {
        outerNode->maxWidth = *opts.maxWidth;
    }
    if (opts.minHeight.has_value() && *opts.minHeight > 0.0f) {
        outerNode->minHeight = *opts.minHeight;
    }
    if (opts.maxHeight.has_value() && *opts.maxHeight > 0.0f) {
        outerNode->maxHeight = *opts.maxHeight;
    }
    outerNode->flexGrow = opts.flexGrow;
    outerNode->padding = theme.ResolvePadding(opts.padding, UiSize::Md, density);
    outerNode->explicitRadius = opts.border.radius.has_value() ? 1 : 0;
    outerNode->cornerRadius = theme.ResolveRadius(opts.border.radius, UiSize::Md);
    outerNode->clipChildren = opts.clipChildren;
    outerNode->uiSize = UiSize::Md;

    const auto& pal = theme.palette;
    outerNode->backgroundColor = opts.background.IsBackgroundEnabled(true)
        ? theme.ResolveColor(opts.background, false, false, activeDisabled, pal.surface)
        : Color::Transparent();
    outerNode->borderWidth = opts.border.width;
    outerNode->borderColor = (opts.border.width > 0.0f)
        ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, pal.border)
        : Color::Transparent();
    outerNode->visualHash = detail::HashArgs(opts.background, opts.border);

    ctx.FinishContainer(outerNode);
    return ContainerScope(&ctx, outerNode);
}

} // namespace guikit
