#include "guikit/core/scrollbar.hpp"
#include "keel/math.h"

#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/style_painter.hpp"

namespace guikit {

ScrollbarGeometry Scrollbar::ComputeAxis(
    Rect viewportRect,
    keel::f32 contentLength,
    keel::f32 scrollOffset,
    bool isHorizontal,
    const ScrollbarMetrics& metrics,
    const ScrollbarStyleTokens& style,
    keel::f32 cornerReserve,
    bool alwaysVisible
) noexcept {
    ScrollbarGeometry geom{};
    const keel::f32 viewLen = keel::max(0.0f, isHorizontal ? viewportRect.w : viewportRect.h);
    const keel::f32 crossLen = keel::max(0.0f, isHorizontal ? viewportRect.h : viewportRect.w);
    const keel::f32 maxScroll = keel::max(0.0f, contentLength - viewLen);
    geom.maxScroll = maxScroll;

    if ((!alwaysVisible && maxScroll <= 0.5f) || contentLength <= 0.0f || viewLen <= 8.0f || crossLen <= 8.0f) {
        return geom;
    }

    const keel::f32 trackW = keel::max(3.0f, metrics.trackWidth);
    const keel::f32 margin = keel::max(0.0f, metrics.margin);
    const keel::f32 barLen = keel::max(0.0f, viewLen - margin * 2.0f - cornerReserve);
    if (barLen <= 4.0f) {
        return geom;
    }

    geom.visible = true;

    const keel::f32 btnLen = (style.buttonSize > 0.0f) ? style.buttonSize : trackW;
    const bool fitButtons = style.showButtons && (barLen >= btnLen * 2.0f + 12.0f);
    geom.hasButtons = fitButtons;

    const keel::f32 trackOffset = fitButtons ? btnLen : 0.0f;
    const keel::f32 trackLen = keel::max(4.0f, barLen - (fitButtons ? btnLen * 2.0f : 0.0f));
    const keel::f32 effContent = keel::max(viewLen, contentLength);
    const keel::f32 minThumb = keel::min(trackLen, keel::max(12.0f, metrics.minThumbLength));
    const keel::f32 thumbLen = keel::clamp(
        keel::round(trackLen * (viewLen / effContent)),
        minThumb,
        trackLen
    );
    const keel::f32 travel = keel::max(0.0f, trackLen - thumbLen);
    const keel::f32 ratio = (maxScroll > 0.0f) ? keel::clamp(scrollOffset / maxScroll, 0.0f, 1.0f) : 0.0f;
    const keel::f32 thumbOff = keel::round(travel * ratio);

    geom.travel = travel;

    const keel::f32 hitThick = keel::max(metrics.gutter, trackW + margin * 2.0f);
    if (isHorizontal) {
        const keel::f32 barX = keel::floor(viewportRect.x + margin);
        const keel::f32 barY = keel::floor(viewportRect.Bottom() - trackW - margin);
        geom.barRect = Rect(barX, barY, barLen, trackW);
        if (fitButtons) {
            geom.startButtonRect = Rect(barX, barY, btnLen, trackW);
            geom.endButtonRect   = Rect(barX + barLen - btnLen, barY, btnLen, trackW);
        }
        geom.trackRect = Rect(barX + trackOffset, barY, trackLen, trackW);
        geom.thumbRect = Rect(geom.trackRect.x + thumbOff, barY, thumbLen, trackW);

        const keel::f32 hitY = viewportRect.Bottom() - hitThick;
        geom.hitTrackRect = Rect(geom.barRect.x, hitY, geom.barRect.w, hitThick);
        geom.hitThumbRect = Rect(geom.thumbRect.x - 1.0f, hitY, geom.thumbRect.w + 2.0f, hitThick);
    } else {
        const keel::f32 barX = keel::floor(viewportRect.Right() - trackW - margin);
        const keel::f32 barY = keel::floor(viewportRect.y + margin);
        geom.barRect = Rect(barX, barY, trackW, barLen);
        if (fitButtons) {
            geom.startButtonRect = Rect(barX, barY, trackW, btnLen);
            geom.endButtonRect   = Rect(barX, barY + barLen - btnLen, trackW, btnLen);
        }
        geom.trackRect = Rect(barX, barY + trackOffset, trackW, trackLen);
        geom.thumbRect = Rect(barX, geom.trackRect.y + thumbOff, trackW, thumbLen);

        const keel::f32 hitX = viewportRect.Right() - hitThick;
        geom.hitTrackRect = Rect(hitX, geom.barRect.y, hitThick, geom.barRect.h);
        geom.hitThumbRect = Rect(hitX, geom.thumbRect.y - 1.0f, hitThick, geom.thumbRect.h + 2.0f);
    }

    return geom;
}

ScrollbarGeometry Scrollbar::ComputeVertical(
    Rect viewportRect,
    keel::f32 contentHeight,
    keel::f32 scrollOffset,
    const ScrollbarMetrics& metrics,
    const ScrollbarStyleTokens& style,
    keel::f32 cornerReserve
) noexcept {
    return ComputeAxis(viewportRect, contentHeight, scrollOffset, false, metrics, style, cornerReserve);
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
    const ScrollbarStyleTokens& style = ctx.GetTheme().scrollbarStyle;
    const keel::f32 viewportH = keel::max(0.0f, viewportRect.h);
    const keel::f32 maxScroll = keel::max(0.0f, contentHeight - viewportH);
    ioScrollOffset = keel::clamp(ioScrollOffset, 0.0f, maxScroll);

    ScrollbarGeometry geom = ComputeVertical(viewportRect, contentHeight, ioScrollOffset, metrics, style);
    if (!geom.visible) {
        if (ioState.dragging && ctx.GetActiveId() == activeOwnerId) {
            ctx.SetActiveId(Id::Null());
        }
        ioState.dragging = false;
        ioState.hovered = false;
        ioState.hoveredPart = ScrollbarPart::None;
        ioState.pressedPart = ScrollbarPart::None;
        return geom;
    }

    const Vec2 mouse = ctx.GetMousePos();
    const Rect wheelBounds = wheelHoverRect.value_or(viewportRect);
    const keel::f32 prevScroll = ioScrollOffset;
    const bool prevHovered = ioState.hovered;
    const bool prevDragging = ioState.dragging;
    const ScrollbarPart prevHovPart = ioState.hoveredPart;
    const ScrollbarPart prevPressPart = ioState.pressedPart;

    if (wheelBounds.Contains(mouse)) {
        const Vec2 wheel = ctx.GetMouseWheel();
        if (wheel.y != 0.0f || wheel.x != 0.0f) {
            ioScrollOffset = keel::clamp(ioScrollOffset - wheel.y * metrics.scrollStep, 0.0f, maxScroll);
            ctx.ConsumeMouseWheel();
            geom = ComputeVertical(viewportRect, contentHeight, ioScrollOffset, metrics, style);
        }
    }

    const bool overStart = geom.hasButtons && geom.startButtonRect.Contains(mouse);
    const bool overEnd   = geom.hasButtons && geom.endButtonRect.Contains(mouse);
    const bool overThumb = !overStart && !overEnd && geom.hitThumbRect.Contains(mouse);
    const bool overTrack = geom.hitTrackRect.Contains(mouse);
    ioState.hovered = overStart || overEnd || overThumb || overTrack;

    if (overStart)      ioState.hoveredPart = ScrollbarPart::StartButton;
    else if (overEnd)   ioState.hoveredPart = ScrollbarPart::EndButton;
    else if (overThumb) ioState.hoveredPart = ScrollbarPart::Thumb;
    else if (overTrack) ioState.hoveredPart = ScrollbarPart::Track;
    else                ioState.hoveredPart = ScrollbarPart::None;

    const keel::f64 now = ctx.GetCurrentTime();
    if (ioState.dragging) {
        if (ctx.IsMouseDown()) {
            ioState.pressedPart = ScrollbarPart::Thumb;
            if (geom.travel > 0.5f) {
                const keel::f32 deltaY = mouse.y - ioState.dragStartMouse;
                ioScrollOffset = keel::clamp(
                    ioState.dragStartScroll + (deltaY / geom.travel) * maxScroll,
                    0.0f,
                    maxScroll
                );
                geom = ComputeVertical(viewportRect, contentHeight, ioScrollOffset, metrics, style);
            }
        } else {
            ioState.dragging = false;
            ioState.pressedPart = ScrollbarPart::None;
            if (ctx.GetActiveId() == activeOwnerId) {
                ctx.SetActiveId(Id::Null());
            }
            ctx.ConsumeMouseClick();
        }
    } else if (ioState.pressedPart == ScrollbarPart::StartButton || ioState.pressedPart == ScrollbarPart::EndButton) {
        if (ctx.IsMouseDown()) {
            const bool stillOver = (ioState.pressedPart == ScrollbarPart::StartButton) ? overStart : overEnd;
            if (stillOver) {
                ctx.RequestRepaint(1);
                if (now >= ioState.nextRepeatSec) {
                    const keel::f32 step = (ioState.pressedPart == ScrollbarPart::StartButton) ? -metrics.scrollStep : metrics.scrollStep;
                    ioScrollOffset = keel::clamp(ioScrollOffset + step, 0.0f, maxScroll);
                    ioState.nextRepeatSec = now + 0.05;
                    geom = ComputeVertical(viewportRect, contentHeight, ioScrollOffset, metrics, style);
                    ctx.RequestRepaint(2);
                }
            }
        } else {
            ioState.pressedPart = ScrollbarPart::None;
            if (ctx.GetActiveId() == activeOwnerId) {
                ctx.SetActiveId(Id::Null());
            }
        }
    } else if (ctx.IsMouseClicked() && overTrack) {
        if (overStart) {
            ioScrollOffset = keel::max(0.0f, ioScrollOffset - metrics.scrollStep);
            ioState.pressedPart = ScrollbarPart::StartButton;
            ioState.nextRepeatSec = now + 0.30;
            if (activeOwnerId.IsValid()) ctx.SetActiveId(activeOwnerId);
            geom = ComputeVertical(viewportRect, contentHeight, ioScrollOffset, metrics, style);
        } else if (overEnd) {
            ioScrollOffset = keel::min(maxScroll, ioScrollOffset + metrics.scrollStep);
            ioState.pressedPart = ScrollbarPart::EndButton;
            ioState.nextRepeatSec = now + 0.30;
            if (activeOwnerId.IsValid()) ctx.SetActiveId(activeOwnerId);
            geom = ComputeVertical(viewportRect, contentHeight, ioScrollOffset, metrics, style);
        } else if (overThumb) {
            ioState.dragging = true;
            ioState.pressedPart = ScrollbarPart::Thumb;
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
            geom = ComputeVertical(viewportRect, contentHeight, ioScrollOffset, metrics, style);
        }
        ctx.ConsumeMouseClick();
    }

    if (keel::abs(ioScrollOffset - prevScroll) > 0.01f ||
        ioState.hovered != prevHovered ||
        ioState.dragging != prevDragging ||
        ioState.hoveredPart != prevHovPart ||
        ioState.pressedPart != prevPressPart) {
        ctx.InvalidateRect(wheelBounds);
        ctx.RequestRepaint(2);
    }

    return geom;
}

void Scrollbar::DrawAxis(
    ICanvas& canvas,
    const ScrollbarGeometry& geom,
    bool isHorizontal,
    bool hovered,
    bool dragging,
    ScrollbarPart hoveredPart,
    ScrollbarPart pressedPart,
    const ScrollbarMetrics& metrics,
    const UiTheme& theme,
    Color accentColor
) {
    if (!geom.visible || geom.trackRect.w <= 0.0f || geom.trackRect.h <= 0.0f) {
        return;
    }

    theme.Painter().DrawScrollbar(canvas, theme, ScrollbarPaintParams{
        .barRect         = geom.barRect,
        .trackRect       = geom.trackRect,
        .thumbRect       = geom.thumbRect,
        .startButtonRect = geom.startButtonRect,
        .endButtonRect   = geom.endButtonRect,
        .horizontal      = isHorizontal,
        .hasButtons      = geom.hasButtons,
        .hovered         = hovered,
        .thumbHovered    = (hoveredPart == ScrollbarPart::Thumb),
        .dragging        = dragging,
        .startHovered    = (hoveredPart == ScrollbarPart::StartButton),
        .startPressed    = (pressedPart == ScrollbarPart::StartButton),
        .endHovered      = (hoveredPart == ScrollbarPart::EndButton),
        .endPressed      = (pressedPart == ScrollbarPart::EndButton),
        .cornerRadius    = metrics.cornerRadius,
        .accentColor     = accentColor
    });
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

void Scrollbar::DrawVertical(
    ICanvas& canvas,
    const ScrollbarGeometry& geom,
    const ScrollbarState& state,
    const ScrollbarMetrics& metrics,
    const UiTheme& theme,
    Color accentColor
) {
    DrawAxis(
        canvas,
        geom,
        false,
        state.hovered,
        state.dragging,
        state.hoveredPart,
        state.pressedPart,
        metrics,
        theme,
        accentColor
    );
}

} // namespace guikit
