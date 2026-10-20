#include "guikit/panels/SplitView.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/Solver.hpp"

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

    float minR = std::clamp(opts.minRatio, 0.0f, 1.0f);
    float maxR = std::clamp(opts.maxRatio, minR, 1.0f);

    if (availSpan > 1.0f) {
        const float minFirstPx = std::max(0.0f, opts.minFirstSize) * scale;
        const float minSecondPx = std::max(0.0f, opts.minSecondSize) * scale;
        const float reqMinR = std::min(0.5f, minFirstPx / availSpan);
        const float reqMaxR = std::max(0.5f, 1.0f - (minSecondPx / availSpan));
        minR = std::max(minR, reqMinR);
        maxR = std::min(maxR, reqMaxR);
        if (minR > maxR) {
            minR = maxR = 0.5f;
        }
    }

    return std::clamp(ratio, minR, maxR);
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
    const UiTheme& theme
) noexcept {
    if (!node) return {0.0f, 0.0f};
    const auto* payload = node->As<SplitViewPayload>();
    const SplitViewOptions opts = payload ? payload->opts : SplitViewOptions{};
    const bool isHoriz = (opts.orientation == Orientation::Horizontal);
    const float sashPx = std::max(2.0f, std::round(theme.Scale(opts.sashSize)));
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
        totalH += std::max(firstSz.y, secondSz.y);
    } else {
        totalW += std::max(firstSz.x, secondSz.x);
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
        std::max(0.0f, node->computedRect.w - node->padding.Horizontal() - bw * 2.0f),
        std::max(0.0f, node->computedRect.h - node->padding.Vertical() - bw * 2.0f)
    );

    const bool isHoriz = (opts.orientation == Orientation::Horizontal);
    const float s = (theme.scale > 0.0f) ? theme.scale : 1.0f;
    const float sashPx = std::max(2.0f, std::round(theme.Scale(opts.sashSize)));
    const float availSpan = std::max(0.0f, (isHoriz ? innerRect.w : innerRect.h) - sashPx);

    const float rawRatio = payload ? payload->ratio : opts.defaultRatio;
    const float r = ClampSplitRatio(rawRatio, availSpan, s, opts);

    Rect firstRect{};
    Rect sashRect{};
    Rect secondRect{};

    if (isHoriz) {
        const float effSashW = std::min(sashPx, innerRect.w);
        const float firstW = std::round(availSpan * r);
        const float secondW = std::max(0.0f, availSpan - firstW);
        firstRect = Rect(innerRect.x, innerRect.y, firstW, innerRect.h);
        sashRect = Rect(innerRect.x + firstW, innerRect.y, effSashW, innerRect.h);
        secondRect = Rect(sashRect.Right(), innerRect.y, secondW, innerRect.h);
    } else {
        const float effSashH = std::min(sashPx, innerRect.h);
        const float firstH = std::round(availSpan * r);
        const float secondH = std::max(0.0f, availSpan - firstH);
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
    const float s = (theme.scale > 0.0f) ? theme.scale : 1.0f;
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
        const float lineX = std::round(sashRect.Center().x);
        canvas.DrawRect(Rect(lineX, sashRect.y, 1.0f, sashRect.h), lineCol);
    } else {
        const float lineY = std::round(sashRect.Center().y);
        canvas.DrawRect(Rect(sashRect.x, lineY, sashRect.w, 1.0f), lineCol);
    }

    // Center grip pill (3x24 for Horizontal or 24x3 for Vertical)
    if (payload->opts.showGrip) {
        const float gripW = isHoriz ? std::max(2.0f, std::round(3.0f * s)) : std::round(24.0f * s);
        const float gripH = isHoriz ? std::round(24.0f * s) : std::max(2.0f, std::round(3.0f * s));
        if (sashRect.w >= (isHoriz ? 2.0f : gripW) && sashRect.h >= (isHoriz ? gripH : 2.0f)) {
            const Rect gripRect(
                std::round(sashRect.Center().x - gripW * 0.5f),
                std::round(sashRect.Center().y - gripH * 0.5f),
                gripW,
                gripH
            );
            const Color gripCol = active ? primaryCol : pal.textMuted.WithAlphaF(0.65f);
            const float gripRadius = std::min(gripW, gripH) * 0.5f;
            canvas.DrawRoundRect(gripRect, gripRadius, gripCol, Color::Transparent(), 0.0f);
        }
    }
}

} // namespace

BoxScope SplitViewScope::First(
    const BoxOptions& opts,
    std::source_location loc
) {
    if (!m_ctx) {
        return BoxScope(nullptr, nullptr);
    }
    return Box::Open(*m_ctx, opts, loc);
}

BoxScope SplitViewScope::Second(
    const BoxOptions& opts,
    std::source_location loc
) {
    if (!m_ctx) {
        return BoxScope(nullptr, nullptr);
    }
    return Box::Open(*m_ctx, opts, loc);
}

SplitViewScope SplitView::Open(
    Context& ctx,
    const SplitViewOptions& opts,
    std::source_location loc
) {
    return Open(ctx, static_cast<float*>(nullptr), opts, loc);
}

SplitViewScope SplitView::Open(
    Context& ctx,
    float& ratio,
    const SplitViewOptions& opts,
    std::source_location loc
) {
    return Open(ctx, &ratio, opts, loc);
}

SplitViewScope SplitView::Open(
    Context& ctx,
    float* externalRatio,
    const SplitViewOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const Id idSash(detail::HashArgs(id.value, uint64_t(0x53504C545F535348ull)));

    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    const float safeScale = (activeScale > 0.0f) ? activeScale : 1.0f;
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const UiTheme& theme = ctx.GetTheme();

    auto& st = ctx.GetOrCreateState<SplitViewPersistState>(id);
    if (!st.initialized) {
        st.initialized = true;
        st.ratio = std::clamp(opts.defaultRatio, 0.0f, 1.0f);
    }
    if (externalRatio != nullptr) {
        st.ratio = std::clamp(*externalRatio, 0.0f, 1.0f);
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
        ctx.BeginOverlayHitCheck(idSash);
        const Vec2 mouse = ctx.GetMousePos();
        const bool occludedByPopup = ctx.GetOpenPopupId().IsValid() && ctx.GetOpenPopupRect().Contains(mouse);
        const bool occludedByModal = ctx.IsModalBlocking(mouse);
        const bool otherActive = ctx.GetActiveId().IsValid() && ctx.GetActiveId() != idSash;

        const Rect hitRect = ExpandSashHitRect(st.prevSashRect, opts.orientation, 2.0f * safeScale);

        if (!occludedByPopup && !occludedByModal && !otherActive) {
            if (hitRect.Contains(mouse)) {
                st.sashHovered = true;
                res.hovered = true;

                if (ctx.IsMouseDoubleClicked()) {
                    const float resetTarget = std::clamp(
                        opts.defaultRatio,
                        std::clamp(opts.minRatio, 0.0f, 1.0f),
                        std::clamp(opts.maxRatio, 0.0f, 1.0f)
                    );
                    if (std::abs(st.ratio - resetTarget) > 1e-5f) {
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
                const float sashPx = std::max(2.0f, std::round(theme.Scale(opts.sashSize)));
                const float containerSpan = isHoriz ? st.prevContainerRect.w : st.prevContainerRect.h;
                const float availSpan = std::max(1.0f, containerSpan - sashPx);
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

                if (std::abs(newRatio - st.ratio) > 1e-4f) {
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

        ctx.EndOverlayHitCheck();
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

    Node* outerNode = ctx.CreateNode(NodeType::SplitView, id);
    outerNode->theme = pushedTheme;
    outerNode->debugBoundsOverride = opts.debugBounds;
    outerNode->inspectModeOverride = opts.inspectMode;
    outerNode->direction = isHoriz ? FlexDirection::Row : FlexDirection::Column;
    outerNode->widthMode = opts.width.has_value() ? SizeMode::Fixed : opts.widthMode;
    outerNode->heightMode = opts.height.has_value() ? SizeMode::Fixed : opts.heightMode;
    if (opts.width.has_value() && *opts.width > 0.0f) {
        outerNode->fixedWidth = theme.Scale(*opts.width);
    }
    if (opts.height.has_value() && *opts.height > 0.0f) {
        outerNode->fixedHeight = theme.Scale(*opts.height);
    }
    outerNode->padding = theme.ResolvePadding(opts.padding, activeSize);

    const auto& pal = theme.palette;
    outerNode->backgroundColor = opts.background.IsBackgroundEnabled(false)
        ? (opts.background.HasAny() && !opts.background.IsSurface()
            ? opts.background.Resolve(false, false, activeDisabled, pal.surface)
            : pal.surface)
        : Color::Transparent();
    const float bw = (opts.border.width > 0.0f) ? opts.border.width : (opts.border.color.HasAny() ? 1.0f : 0.0f);
    outerNode->borderWidth = theme.Scale(bw);
    outerNode->borderColor = (bw > 0.0f)
        ? opts.border.color.Resolve(false, false, activeDisabled, pal.border)
        : Color::Transparent();
    outerNode->cornerRadius = opts.border.radius.has_value() ? theme.Scale(*opts.border.radius) : 0.0f;
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
        static_cast<uint8_t>(opts.orientation),
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
                ExpandSashHitRect(payload->sashRect, orient, 2.0f * safeScale)
            );
        });
    }

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(outerNode);

    return SplitViewScope(&ctx, outerNode, res, st.ratio, st.dragging);
}

} // namespace guikit
