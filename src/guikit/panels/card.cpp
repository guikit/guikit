#include "guikit/panels/card.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

namespace {

struct CardElevatedPayload {
    Color surfaceBg{};
};

void RenderElevatedCard(const Node* node, ICanvas& canvas, const UiTheme& theme) {
    const auto* payload = node ? node->As<CardElevatedPayload>() : nullptr;
    if (!payload) return;

    const Rect& rect = node->computedRect;
    const keel::f32 r = node->cornerRadius;

    // 1. Theme-controlled elevation shadow
    if (const ShadowStyle sh = theme.GetShadow(Elevation::Md); sh.IsVisible()) {
        canvas.DrawShadow(rect, r, sh.blur, sh.offset, sh.color);
    }

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

ContainerScope detail::Card::Open(
    Context& ctx,
    const CardOptions& opts,
    keel::SourceLocation loc
) {
    const Id idOuter = ctx.MakeChildId(loc);

    // Outer styled surface frame
    Node* outerNode = ctx.CreateNode(NodeType::Card, idOuter, opts.disabled);
    const UiTheme& theme = outerNode->GetTheme(ctx.GetTheme());
    const keel::f32 density = outerNode->GetDensity();
    const bool activeDisabled = outerNode->disabled;
    outerNode->direction = FlexDirection::Column;
    outerNode->gap = theme.ResolveGap(opts.gap, UiSize::Md, density);
    if (opts.minWidth.has_value() && *opts.minWidth > 0.0f) {
        outerNode->minWidth = *opts.minWidth;
    }
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
    outerNode->flexGrow = Control::ResolveFlexGrow(outerNode->parent, opts.width, opts.height, opts.flexGrow);
    outerNode->padding = theme.ResolvePadding(opts.padding, UiSize::Md, density);
    outerNode->explicitRadius = opts.border.radius.has_value() ? 1 : 0;
    outerNode->cornerRadius = theme.ResolveRadius(opts.border.radius, UiSize::Md);
    outerNode->clipChildren = opts.clipChildren;
    outerNode->uiSize = UiSize::Md;

    const auto& pal = theme.palette;
    const Color defaultBg = opts.elevated ? pal.surfaceElevated : pal.surface;
    const Color defaultBorder = opts.elevated ? pal.borderHover : pal.border;
    const bool wantsBgHover = opts.background.hover.has_value();
    const bool wantsBorderHover = opts.border.width > 0.0f && opts.border.color.hover.has_value();
    bool isHovered = false;
    if (!activeDisabled && (wantsBgHover || wantsBorderHover)) {
        const Rect prevRect = ctx.GetPrevFrameBounds(idOuter);
        if (prevRect.w > 0.0f && prevRect.h > 0.0f) {
            isHovered = ctx.IsRectHovered(idOuter, prevRect);
        }
    }
    const Color resolvedBg = opts.background.IsBackgroundEnabled(true)
        ? theme.ResolveColor(opts.background, wantsBgHover && isHovered, false, activeDisabled, defaultBg)
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
        ? theme.ResolveColor(opts.border.color, wantsBorderHover && isHovered, false, activeDisabled, defaultBorder)
        : Color::Transparent();
    outerNode->visualHash = detail::HashArgs(opts.background, opts.border, opts.elevated, isHovered);

    ctx.FinishContainer(outerNode);
    return ContainerScope(&ctx, outerNode);
}

} // namespace guikit
