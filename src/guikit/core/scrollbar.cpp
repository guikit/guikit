#include "guikit/core/scrollbar.hpp"
#include "keel/math.h"

#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"

namespace guikit {

ScrollbarGeometry Scrollbar::ComputeVertical(
    Rect viewportRect,
    keel::f32 contentHeight,
    keel::f32 scrollOffset,
    const ScrollbarMetrics& metrics
) noexcept {
    ScrollbarGeometry geom{};
    const keel::f32 viewportH = keel::max(0.0f, viewportRect.h);
    const keel::f32 maxScroll = keel::max(0.0f, contentHeight - viewportH);
    geom.maxScroll = maxScroll;

    if (maxScroll <= 0.5f || contentHeight <= 0.0f || viewportH <= 8.0f || viewportRect.w <= 8.0f) {
        return geom;
    }

    geom.visible = true;
    const keel::f32 trackW = keel::max(3.0f, metrics.trackWidth);
    const keel::f32 margin = keel::max(2.0f, metrics.margin);
    const keel::f32 trackX = keel::round(viewportRect.Right() - trackW - margin);
    const keel::f32 trackY = viewportRect.y + margin;
    const keel::f32 trackH = keel::max(8.0f, viewportH - margin * 2.0f);
    const keel::f32 minThumb = keel::min(trackH, keel::max(16.0f, metrics.minThumbLength));
    const keel::f32 thumbH = keel::clamp(
        keel::round(trackH * (viewportH / contentHeight)),
        minThumb,
        trackH
    );
    const keel::f32 travel = keel::max(0.0f, trackH - thumbH);
    const keel::f32 ratio = keel::clamp(scrollOffset / maxScroll, 0.0f, 1.0f);
    const keel::f32 thumbY = keel::round(trackY + travel * ratio);

    geom.travel = travel;
    geom.trackRect = Rect(trackX, trackY, trackW, trackH);
    geom.thumbRect = Rect(trackX, thumbY, trackW, thumbH);

    const keel::f32 hitW = keel::max(metrics.gutter, trackW + margin * 2.0f);
    const keel::f32 hitX = viewportRect.Right() - hitW;
    geom.hitTrackRect = Rect(hitX, viewportRect.y, hitW, viewportH);
    geom.hitThumbRect = Rect(hitX, thumbY - 2.0f, hitW, thumbH + 4.0f);
    return geom;
}

ScrollbarGeometry Scrollbar::UpdateVertical(
    Context& ctx,
    Id activeOwnerId,
    Rect viewportRect,
    keel::f32 contentHeight,
    keel::f32& ioScrollOffset,
    ScrollbarState& ioState,
    const ScrollbarMetrics& metrics,
    keel::Optional<Rect> wheelHoverRect
) noexcept {
    const keel::f32 viewportH = keel::max(0.0f, viewportRect.h);
    const keel::f32 maxScroll = keel::max(0.0f, contentHeight - viewportH);
    ioScrollOffset = keel::clamp(ioScrollOffset, 0.0f, maxScroll);

    ScrollbarGeometry geom = ComputeVertical(viewportRect, contentHeight, ioScrollOffset, metrics);
    if (!geom.visible) {
        if (ioState.dragging && ctx.GetActiveId() == activeOwnerId) {
            ctx.SetActiveId(Id::Null());
        }
        ioState.dragging = false;
        ioState.hovered = false;
        return geom;
    }

    const Vec2 mouse = ctx.GetMousePos();
    const Rect wheelBounds = wheelHoverRect.value_or(viewportRect);
    const keel::f32 prevScroll = ioScrollOffset;
    const bool prevHovered = ioState.hovered;
    const bool prevDragging = ioState.dragging;

    if (wheelBounds.Contains(mouse)) {
        const Vec2 wheel = ctx.GetMouseWheel();
        if (wheel.y != 0.0f || wheel.x != 0.0f) {
            ioScrollOffset = keel::clamp(ioScrollOffset - wheel.y * metrics.scrollStep, 0.0f, maxScroll);
            ctx.ConsumeMouseWheel();
            geom = ComputeVertical(viewportRect, contentHeight, ioScrollOffset, metrics);
        }
    }

    const bool overThumb = geom.hitThumbRect.Contains(mouse);
    const bool overTrack = geom.hitTrackRect.Contains(mouse);
    ioState.hovered = overThumb || overTrack;

    if (ioState.dragging) {
        if (ctx.IsMouseDown()) {
            if (geom.travel > 0.5f) {
                const keel::f32 deltaY = mouse.y - ioState.dragStartMouse;
                ioScrollOffset = keel::clamp(
                    ioState.dragStartScroll + (deltaY / geom.travel) * maxScroll,
                    0.0f,
                    maxScroll
                );
                geom = ComputeVertical(viewportRect, contentHeight, ioScrollOffset, metrics);
            }
        } else {
            ioState.dragging = false;
            if (ctx.GetActiveId() == activeOwnerId) {
                ctx.SetActiveId(Id::Null());
            }
            ctx.ConsumeMouseClick();
        }
    } else if (ctx.IsMouseClicked() && overTrack) {
        if (overThumb) {
            ioState.dragging = true;
            ioState.dragStartMouse = mouse.y;
            ioState.dragStartScroll = ioScrollOffset;
            if (activeOwnerId.IsValid()) {
                ctx.SetActiveId(activeOwnerId);
            }
        } else {
            const keel::f32 pageStep = keel::max(24.0f, viewportH * 0.8f);
            if (mouse.y < geom.thumbRect.y) {
                ioScrollOffset = keel::max(0.0f, ioScrollOffset - pageStep);
            } else if (mouse.y > geom.thumbRect.Bottom()) {
                ioScrollOffset = keel::min(maxScroll, ioScrollOffset + pageStep);
            }
            geom = ComputeVertical(viewportRect, contentHeight, ioScrollOffset, metrics);
        }
        ctx.ConsumeMouseClick();
    }

    if (keel::abs(ioScrollOffset - prevScroll) > 0.01f ||
        ioState.hovered != prevHovered ||
        ioState.dragging != prevDragging) {
        ctx.InvalidateRect(wheelBounds);
        ctx.RequestRepaint(2);
    }

    return geom;
}

void Scrollbar::DrawVertical(
    ICanvas& canvas,
    const ScrollbarGeometry& geom,
    bool hovered,
    bool dragging,
    const ScrollbarMetrics& metrics,
    const ThemePalette& pal,
    Color accentColor
) {
    if (!geom.visible || geom.trackRect.h <= 0.0f || geom.thumbRect.h <= 0.0f) {
        return;
    }

    if (hovered || dragging) {
        canvas.DrawRoundRect(
            geom.trackRect,
            metrics.cornerRadius,
            pal.surfaceHover.WithAlphaF(0.45f),
            Color::Transparent(),
            0.0f
        );
    }

    const Color thumbCol = dragging
        ? accentColor
        : (hovered
            ? pal.textSecondary.WithAlphaF(0.85f)
            : pal.textMuted.WithAlphaF(0.50f));

    canvas.DrawRoundRect(
        geom.thumbRect,
        metrics.cornerRadius,
        thumbCol,
        Color::Transparent(),
        0.0f
    );
}

} // namespace guikit
