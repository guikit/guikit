#include "guikit/panels/split_view.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/solver.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

struct SplitViewPersistState {
    bool initialized{false};
    float ratio{0.5f};
    bool dragging{false};
    bool sashHovered{false};
    Vec2 dragStartMouse{0.0f, 0.0f};
    float dragStartRatio{0.5f};
    Rect prevContainerRect{0.0f, 0.0f, 0.0f, 0.0f};
    Rect prevSashRect{0.0f, 0.0f, 0.0f, 0.0f};
};

struct SplitViewPayload {
    SplitViewOptions opts{};
    SplitViewPersistState* state{nullptr};
    ICanvas* canvas{nullptr};
    Rect sashRect{0.0f, 0.0f, 0.0f, 0.0f};
    float ratio{0.5f};
    bool sashHovered{false};
    bool dragging{false};
};

float ClampSplitRatio(
    float ratio,
    float availSpan,
    float scale,
    const SplitViewOptions& opts
) noexcept {
    if (opts.collapsible) {
        if (ratio <= 0.001f) return 0.0f;
        if (ratio >= 0.999f) return 1.0f;
    }

    float minR = keel::clamp(opts.minRatio, 0.0f, 1.0f);
    float maxR = keel::clamp(opts.maxRatio, minR, 1.0f);

    if (availSpan > 1.0f) {
        const float minFirstPx = keel::max(0.0f, opts.minFirstSize) * scale;
        const float minSecondPx = keel::max(0.0f, opts.minSecondSize) * scale;
        const float reqMinR = keel::min(0.5f, minFirstPx / availSpan);
        const float reqMaxR = keel::max(0.5f, 1.0f - (minSecondPx / availSpan));
        minR = keel::max(minR, reqMinR);
        maxR = keel::min(maxR, reqMaxR);
        if (minR > maxR) {
            minR = maxR = 0.5f;
        }
    }

    return keel::clamp(ratio, minR, maxR);
}

Rect ExpandSashHitRect(const Rect& sashRect, Orientation orientation, float expandPx) noexcept {
    if (sashRect.w <= 0.0f || sashRect.h <= 0.0f) {
        return sashRect;
    }
    if (orientation == Orientation::Horizontal) {
        return Rect(sashRect.x - expandPx, sashRect.y, sashRect.w + expandPx * 2.0f, sashRect.h);
    }
    return Rect(sashRect.x, sashRect.y - expandPx, sashRect.w, sashRect.h + expandPx * 2.0f);
}

Vec2 MeasureSplitView(
    const Node* node,
    float /*availableWidth*/,
    ICanvas* /*canvas*/,
    const UiTheme& /*theme*/
) noexcept {
    if (!node) return {0.0f, 0.0f};
    const auto* payload = node->As<SplitViewPayload>();
    const SplitViewOptions opts = payload ? payload->opts : SplitViewOptions{};
    const bool isHoriz = (opts.orientation == Orientation::Horizontal);
    const float sashPx = keel::max(2.0f, keel::round(opts.sashSize));
    const float bw = node->borderWidth;

    const Node* firstChild = node->firstChild;
    const Node* secondChild = firstChild ? firstChild->nextSibling : nullptr;

    Vec2 firstSz{0.0f, 0.0f};
    if (firstChild) {
        firstSz.x = (firstChild->widthMode == SizeMode::Fixed) ? firstChild->fixedWidth : firstChild->intrinsicSize.x;
        firstSz.y = (firstChild->heightMode == SizeMode::Fixed) ? firstChild->fixedHeight : firstChild->intrinsicSize.y;
    }

    Vec2 secondSz{0.0f, 0.0f};
    if (secondChild) {
        secondSz.x = (secondChild->widthMode == SizeMode::Fixed) ? secondChild->fixedWidth : secondChild->intrinsicSize.x;
        secondSz.y = (secondChild->heightMode == SizeMode::Fixed) ? secondChild->fixedHeight : secondChild->intrinsicSize.y;
    }

    float totalW = node->padding.Horizontal() + bw * 2.0f;
    float totalH = node->padding.Vertical() + bw * 2.0f;

    if (isHoriz) {
        totalW += firstSz.x + sashPx + secondSz.x;
        totalH += keel::max(firstSz.y, secondSz.y);
    } else {
        totalW += keel::max(firstSz.x, secondSz.x);
        totalH += firstSz.y + sashPx + secondSz.y;
    }

    if (node->widthMode == SizeMode::Fixed && node->fixedWidth > 0.0f) {
        totalW = node->fixedWidth;
    }
    if (node->heightMode == SizeMode::Fixed && node->fixedHeight > 0.0f) {
        totalH = node->fixedHeight;
    }
    return {totalW, totalH};
}

void ArrangeSplitView(
    Node* node,
    const Rect& availableRect,
    const UiTheme& theme
) noexcept {
    if (!node) return;
    auto* payload = node->As<SplitViewPayload>();
    const SplitViewOptions opts = payload ? payload->opts : SplitViewOptions{};

    float w = availableRect.w;
    float h = availableRect.h;
    if (node->widthMode == SizeMode::Fixed && node->fixedWidth > 0.0f) {
        w = node->fixedWidth;
    } else if (node->widthMode == SizeMode::Fit && node->intrinsicSize.x > 0.0f) {
        w = node->intrinsicSize.x;
    }
    if (node->heightMode == SizeMode::Fixed && node->fixedHeight > 0.0f) {
        h = node->fixedHeight;
    } else if (node->heightMode == SizeMode::Fit && node->intrinsicSize.y > 0.0f) {
        h = node->intrinsicSize.y;
    }

    node->computedRect = Rect(availableRect.x, availableRect.y, w, h);

    const float bw = node->borderWidth;
    const Rect innerRect(
        node->computedRect.x + node->padding.left + bw,
        node->computedRect.y + node->padding.top + bw,
        keel::max(0.0f, node->computedRect.w - node->padding.Horizontal() - bw * 2.0f),
        keel::max(0.0f, node->computedRect.h - node->padding.Vertical() - bw * 2.0f)
    );

    const bool isHoriz = (opts.orientation == Orientation::Horizontal);
    const float s = 1.0f;
    const float sashPx = keel::max(2.0f, keel::round(opts.sashSize));
    const float availSpan = keel::max(0.0f, (isHoriz ? innerRect.w : innerRect.h) - sashPx);

    const float rawRatio = payload ? payload->ratio : opts.defaultRatio;
    const float r = ClampSplitRatio(rawRatio, availSpan, s, opts);

    Rect firstRect{};
    Rect sashRect{};
    Rect secondRect{};

    if (isHoriz) {
        const float effSashW = keel::min(sashPx, innerRect.w);
        const float firstW = keel::round(availSpan * r);
        const float secondW = keel::max(0.0f, availSpan - firstW);
        firstRect = Rect(innerRect.x, innerRect.y, firstW, innerRect.h);
        sashRect = Rect(innerRect.x + firstW, innerRect.y, effSashW, innerRect.h);
        secondRect = Rect(sashRect.Right(), innerRect.y, secondW, innerRect.h);
    } else {
        const float effSashH = keel::min(sashPx, innerRect.h);
        const float firstH = keel::round(availSpan * r);
        const float secondH = keel::max(0.0f, availSpan - firstH);
        firstRect = Rect(innerRect.x, innerRect.y, innerRect.w, firstH);
        sashRect = Rect(innerRect.x, innerRect.y + firstH, innerRect.w, effSashH);
        secondRect = Rect(innerRect.x, sashRect.Bottom(), innerRect.w, secondH);
    }

    if (payload) {
        payload->sashRect = sashRect;
        if (payload->state) {
            payload->state->prevContainerRect = innerRect;
            payload->state->prevSashRect = sashRect;
        }
    }

    Node* firstChild = node->firstChild;
    Node* secondChild = firstChild ? firstChild->nextSibling : nullptr;
    ICanvas* canvas = payload ? payload->canvas : nullptr;

    if (firstChild) {
        firstChild->computedRect = firstRect;
        Solver::Arrange(firstChild, firstRect, theme, canvas);
    }
    if (secondChild) {
        secondChild->computedRect = secondRect;
        Solver::Arrange(secondChild, secondRect, theme, canvas);
    }
}

void RenderSplitView(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) noexcept {
    if (!node) return;
    const auto* payload = node->As<SplitViewPayload>();
    if (!payload) return;

    // If leaf (0 children), draw background/border here; otherwise Context::RenderNodeRecursive draws container bg/border
    if (node->IsLeaf()) {
        if (node->backgroundColor.a > 0 || (node->borderColor.a > 0 && node->borderWidth > 0.0f)) {
            canvas.DrawRoundRect(
                node->computedRect,
                node->cornerRadius,
                node->backgroundColor,
                node->borderColor,
                node->borderWidth
            );
        }
    }

    const Rect sashRect = payload->sashRect;
    if (sashRect.w <= 0.0f || sashRect.h <= 0.0f) {
        return;
    }

    const auto& pal = theme.palette;
    const float s = 1.0f;
    const bool isHoriz = (payload->opts.orientation == Orientation::Horizontal);
    const bool active = payload->sashHovered || payload->dragging;
    const Color primaryCol = theme.GetVariant(UiVariant::Primary).bg;

    // Highlighted sash track on hover/drag
    if (active) {
        canvas.DrawRect(sashRect, primaryCol.WithAlphaF(payload->dragging ? 0.32f : 0.25f));
    } else {
        canvas.DrawRect(sashRect, pal.surfaceHover.WithAlphaF(0.35f));
    }

    // Center divider line (subtle line or highlighted primary line)
    const Color lineCol = active ? primaryCol.WithAlphaF(0.85f) : pal.borderSubtle;
    if (isHoriz) {
        const float lineX = keel::round(sashRect.Center().x);
        canvas.DrawRect(Rect(lineX, sashRect.y, 1.0f, sashRect.h), lineCol);
    } else {
        const float lineY = keel::round(sashRect.Center().y);
        canvas.DrawRect(Rect(sashRect.x, lineY, sashRect.w, 1.0f), lineCol);
    }

    // Center grip pill (3x24 for Horizontal or 24x3 for Vertical)
    if (payload->opts.showGrip) {
        const float gripW = isHoriz ? keel::max(2.0f, keel::round(3.0f * s)) : keel::round(24.0f * s);
        const float gripH = isHoriz ? keel::round(24.0f * s) : keel::max(2.0f, keel::round(3.0f * s));
        if (sashRect.w >= (isHoriz ? 2.0f : gripW) && sashRect.h >= (isHoriz ? gripH : 2.0f)) {
            const Rect gripRect(
                keel::round(sashRect.Center().x - gripW * 0.5f),
                keel::round(sashRect.Center().y - gripH * 0.5f),
                gripW,
                gripH
            );
            const Color gripCol = active ? primaryCol : pal.textMuted.WithAlphaF(0.65f);
            const float gripRadius = keel::min(gripW, gripH) * 0.5f;
            canvas.DrawRoundRect(gripRect, gripRadius, gripCol, Color::Transparent(), 0.0f);
        }
    }
}

} // namespace

BoxScope SplitViewScope::First(
    const BoxOptions& opts,
    keel::SourceLocation loc
) {
    if (!m_ctx) {
        return BoxScope(nullptr, nullptr);
    }
    return Box::Open(*m_ctx, opts, loc);
}

BoxScope SplitViewScope::Second(
    const BoxOptions& opts,
    keel::SourceLocation loc
) {
    if (!m_ctx) {
        return BoxScope(nullptr, nullptr);
    }
    return Box::Open(*m_ctx, opts, loc);
}

SplitViewScope SplitView::Open(
    Context& ctx,
    const SplitViewOptions& opts,
    keel::SourceLocation loc
) {
    return Open(ctx, static_cast<float*>(nullptr), opts, loc);
}

SplitViewScope SplitView::Open(
    Context& ctx,
    float& ratio,
    const SplitViewOptions& opts,
    keel::SourceLocation loc
) {
    return Open(ctx, &ratio, opts, loc);
}

SplitViewScope SplitView::Open(
    Context& ctx,
    float* externalRatio,
    const SplitViewOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const Id idSash(detail::HashArgs(id.value, keel::u64(0x53504C545F535348ull)));

    const UiSize activeSize = UiSize::Md;
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float safeScale = 1.0f;
    const UiTheme& theme = ctx.GetTheme();

    auto& st = ctx.GetOrCreatePersistentState<SplitViewPersistState>(id);
    if (!st.initialized) {
        st.initialized = true;
        st.ratio = keel::clamp(opts.defaultRatio, 0.0f, 1.0f);
    }
    if (externalRatio != nullptr) {
        st.ratio = keel::clamp(*externalRatio, 0.0f, 1.0f);
    }

    Response res{};
    res.id = id;
    res.rect = ctx.GetPrevUnclippedBounds(id);
    res.ctx = &ctx;

    const bool prevHovered = st.sashHovered;
    const bool prevDragging = st.dragging;
    st.sashHovered = false;

    const bool isHoriz = (opts.orientation == Orientation::Horizontal);
    const bool hasPrevSash = (st.prevSashRect.w > 0.0f && st.prevSashRect.h > 0.0f);

    if (!activeDisabled && hasPrevSash) {
        auto hitScope = ctx.ScopedOverlayHitCheck(idSash);
        const Vec2 mouse = ctx.GetMousePos();
        const bool occludedByPopup = ctx.GetOpenPopupId().IsValid() && ctx.GetOpenPopupRect().Contains(mouse);
        const bool occludedByModal = ctx.IsModalBlocking(mouse);
        const bool occludedByPopover = ctx.IsPopoverBlocking(mouse);
        const bool otherActive = ctx.GetActiveId().IsValid() && ctx.GetActiveId() != idSash;

        const Rect hitRect = ExpandSashHitRect(st.prevSashRect, opts.orientation, 2.0f * safeScale);

        if (!occludedByPopup && !occludedByModal && !occludedByPopover && !otherActive) {
            if (hitRect.Contains(mouse)) {
                st.sashHovered = true;
                res.hovered = true;

                if (ctx.IsMouseDoubleClicked()) {
                    const float resetTarget = keel::clamp(
                        opts.defaultRatio,
                        keel::clamp(opts.minRatio, 0.0f, 1.0f),
                        keel::clamp(opts.maxRatio, 0.0f, 1.0f)
                    );
                    if (keel::abs(st.ratio - resetTarget) > 1e-5f) {
                        st.ratio = resetTarget;
                        res.changed = true;
                    }
                    res.doubleClicked = true;
                    st.dragging = false;
                    if (ctx.GetActiveId() == idSash) {
                        ctx.SetActiveId(Id::Null());
                    }
                    ctx.ConsumeMouseClick();
                    ctx.InvalidateAll();
                } else if (ctx.IsMouseClicked()) {
                    st.dragging = true;
                    st.dragStartMouse = mouse;
                    st.dragStartRatio = st.ratio;
                    res.clicked = true;
                    ctx.SetActiveId(idSash);
                    ctx.ConsumeMouseClick();
                    ctx.InvalidateAll();
                }
            }
        }

        if (st.dragging) {
            if (ctx.IsMouseDown()) {
                res.pressed = true;
                const float sashPx = keel::max(2.0f, keel::round(opts.sashSize));
                const float containerSpan = isHoriz ? st.prevContainerRect.w : st.prevContainerRect.h;
                const float availSpan = keel::max(1.0f, containerSpan - sashPx);
                const float delta = isHoriz
                    ? (mouse.x - st.dragStartMouse.x)
                    : (mouse.y - st.dragStartMouse.y);

                const float rawRatio = st.dragStartRatio + (delta / availSpan);
                const float rawFirstPx = rawRatio * availSpan;
                const float rawSecondPx = (1.0f - rawRatio) * availSpan;
                const float distFromStart = isHoriz
                    ? (mouse.x - st.prevContainerRect.x)
                    : (mouse.y - st.prevContainerRect.y);
                const float distFromEnd = isHoriz
                    ? (st.prevContainerRect.Right() - mouse.x)
                    : (st.prevContainerRect.Bottom() - mouse.y);
                const float snapThresholdPx = 16.0f * safeScale;

                float newRatio = rawRatio;
                if (opts.collapsible && (distFromStart <= snapThresholdPx || rawFirstPx <= snapThresholdPx)) {
                    newRatio = 0.0f;
                } else if (opts.collapsible && (distFromEnd <= snapThresholdPx || rawSecondPx <= snapThresholdPx)) {
                    newRatio = 1.0f;
                } else {
                    newRatio = ClampSplitRatio(rawRatio, availSpan, safeScale, opts);
                }

                if (keel::abs(newRatio - st.ratio) > 1e-4f) {
                    st.ratio = newRatio;
                    res.changed = true;
                    ctx.InvalidateAll();
                }
            } else {
                st.dragging = false;
                if (ctx.GetActiveId() == idSash) {
                    ctx.SetActiveId(Id::Null());
                }
                ctx.ConsumeMouseClick();
                ctx.InvalidateAll();
            }
        }

        if (st.sashHovered || st.dragging) {
            ctx.SetCursor(isHoriz ? CursorType::ResizeEW : CursorType::ResizeNS);
        }
    } else {
        st.dragging = false;
        if (ctx.GetActiveId() == idSash) {
            ctx.SetActiveId(Id::Null());
        }
    }

    if (externalRatio != nullptr) {
        *externalRatio = st.ratio;
    }

    if (st.sashHovered != prevHovered || st.dragging != prevDragging) {
        ctx.InvalidateAll();
    }

    Node* outerNode = ctx.CreateNode(NodeType::SplitView, id, opts.disabled);
    const float density = outerNode->GetDensity();
    outerNode->direction = isHoriz ? FlexDirection::Row : FlexDirection::Column;
    outerNode->widthMode = opts.width.ResolveMode(SizeMode::Stretch);
    outerNode->fixedWidth = opts.width.IsFixed() ? opts.width.Dp() : 0.0f;
    outerNode->heightMode = opts.height.ResolveMode(SizeMode::Stretch);
    outerNode->fixedHeight = opts.height.IsFixed() ? opts.height.Dp() : 0.0f;
    outerNode->flexGrow = Control::ResolveFlexGrow(outerNode->parent, opts.width, opts.height, 1.0f);
    outerNode->padding = theme.ResolvePadding(opts.padding, UiSize::None, density);

    const auto& pal = theme.palette;
    outerNode->backgroundColor = opts.background.IsBackgroundEnabled(false)
        ? theme.ResolveColor(opts.background, false, false, activeDisabled, pal.surface)
        : Color::Transparent();
    const float bw = (opts.border.width > 0.0f) ? opts.border.width : (opts.border.color.HasAny() ? 1.0f : 0.0f);
    outerNode->borderWidth = bw;
    outerNode->borderColor = (bw > 0.0f)
        ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, pal.border)
        : Color::Transparent();
    outerNode->cornerRadius = opts.border.radius.has_value() ? *opts.border.radius : 0.0f;
    outerNode->clipChildren = opts.clipChildren;
    outerNode->uiSize = activeSize;
    outerNode->disabled = activeDisabled;
    outerNode->hovered = st.sashHovered;
    outerNode->pressed = st.dragging;

    auto* payload = ctx.AllocPayload<SplitViewPayload>(SplitViewPayload{
        .opts = opts,
        .state = &st,
        .canvas = ctx.GetCanvas(),
        .sashRect = st.prevSashRect,
        .ratio = st.ratio,
        .sashHovered = st.sashHovered,
        .dragging = st.dragging
    });
    outerNode->SetPayload(payload);
    outerNode->measureFn = &MeasureSplitView;
    outerNode->arrangeFn = &ArrangeSplitView;
    outerNode->renderFn = &RenderSplitView;
    outerNode->visualHash = detail::HashArgs(
        st.ratio,
        st.sashHovered,
        st.dragging,
        static_cast<keel::u8>(opts.orientation),
        opts.sashSize,
        opts.showGrip,
        opts.collapsible,
        opts.background,
        opts.border
    );

    if (!activeDisabled) {
        const Orientation orient = opts.orientation;
        ctx.RegisterOverlayHitBuilder([&ctx, payload, idSash, orient, safeScale]() {
            if (!payload || payload->sashRect.w <= 0.0f || payload->sashRect.h <= 0.0f) return;
            ctx.RegisterOverlayHitRegion(
                idSash,
                ExpandSashHitRect(payload->sashRect, orient, 2.0f * safeScale),
                OverlayLayer::Panel
            );
        });
    }

    ctx.FinishContainer(outerNode);

    return SplitViewScope(&ctx, outerNode, res, st.ratio, st.dragging);
}

} // namespace guikit
