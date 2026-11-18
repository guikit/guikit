#include "guikit/panels/card.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

namespace {

struct CardElevatedPayload {
    Color surfaceBg{};
};

void RenderElevatedCard(const Node* node, ICanvas& canvas, const UiTheme& /*theme*/) {
    const auto* payload = node ? node->As<CardElevatedPayload>() : nullptr;
    if (!payload) return;

    const Rect& rect = node->computedRect;
    const float r = node->cornerRadius;

    // 1. Soft two-layer ambient drop shadow
    canvas.DrawRoundRect(
        Rect(rect.x - 2.0f, rect.y + 6.0f, rect.w + 4.0f, rect.h + 6.0f),
        r + 2.0f,
        Color(0, 0, 0, 44),
        Color::Transparent(),
        0.0f
    );
    canvas.DrawRoundRect(
        Rect(rect.x, rect.y + 2.0f, rect.w, rect.h + 2.0f),
        r + 1.0f,
        Color(0, 0, 0, 62),
        Color::Transparent(),
        0.0f
    );

    // 2. Elevated surface fill over shadow (border is drawn after children by RenderNodeRecursive)
    if (payload->surfaceBg.a > 0) {
        if (r > 0.0f) {
            canvas.DrawRoundRect(rect, r, payload->surfaceBg, Color::Transparent(), 0.0f);
        } else {
            canvas.DrawRect(rect, payload->surfaceBg);
        }
    }
}

} // namespace

ContainerScope Card::Open(
    Context& ctx,
    const CardOptions& opts,
    keel::SourceLocation loc
) {
    const Id idOuter = ctx.MakeChildId(loc);

    // Outer styled surface frame
    Node* outerNode = ctx.CreateNode(NodeType::Card, idOuter, opts.disabled);
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
    const Color defaultBg = opts.elevated ? pal.surfaceElevated : pal.surface;
    const Color defaultBorder = opts.elevated ? pal.borderHover : pal.border;
    const Color resolvedBg = opts.background.IsBackgroundEnabled(true)
        ? theme.ResolveColor(opts.background, false, false, activeDisabled, defaultBg)
        : Color::Transparent();

    if (opts.elevated) {
        outerNode->backgroundColor = Color::Transparent();
        outerNode->SetPayload(ctx.AllocPayload<CardElevatedPayload>(CardElevatedPayload{.surfaceBg = resolvedBg}));
        outerNode->renderFn = &RenderElevatedCard;
    } else {
        outerNode->backgroundColor = resolvedBg;
    }

    outerNode->borderWidth = opts.border.width;
    outerNode->borderColor = (opts.border.width > 0.0f)
        ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, defaultBorder)
        : Color::Transparent();
    outerNode->visualHash = detail::HashArgs(opts.background, opts.border, opts.elevated);

    ctx.FinishContainer(outerNode);
    return ContainerScope(&ctx, outerNode);
}

} // namespace guikit
