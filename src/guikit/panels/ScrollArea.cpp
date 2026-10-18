#include "guikit/panels/ScrollArea.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

ContainerScope ScrollArea::Open(
    Context& ctx,
    const ScrollAreaOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    Node* node = ctx.CreateNode(NodeType::ScrollArea, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = (opts.orientation == Orientation::Horizontal) ? FlexDirection::Row : FlexDirection::Column;
    node->clipChildren = true;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Stretch;
    node->flexGrow = 1.0f;
    node->gap = opts.gap.has_value() ? ctx.GetTheme().Scale(*opts.gap) : 0.0f;
    node->padding = ctx.GetTheme().ResolvePadding(opts.padding);
    node->scrollOrientation = opts.orientation;
    node->scrollbarPolicy = opts.scrollbar;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;
    node->postRenderFn = &ScrollArea::RenderScrollbar;

    // Retrieve persistent scroll state
    auto& state = ctx.GetOrCreateState<ScrollState>(id);

    // Handle immediate mouse wheel scroll if hovered on previous frame bounds
    Rect prevBounds = ctx.GetPrevFrameBounds(id);
    if (prevBounds.w > 0.0f && prevBounds.h > 0.0f && !node->disabled) {
        Vec2 wheel = ctx.GetMouseWheel();
        if (prevBounds.Contains(ctx.GetMousePos()) && (wheel.x != 0.0f || wheel.y != 0.0f) && !ctx.GetKeyModifiers().ctrl) {
            const ScrollbarMetrics& m = ctx.GetTheme().GetScrollbar(activeSize);
            if (opts.orientation == Orientation::Horizontal ||
                (opts.orientation == Orientation::Both && ctx.GetKeyModifiers().shift && wheel.x == 0.0f)) {
                const float delta = (wheel.x != 0.0f) ? wheel.x : wheel.y;
                state.offset.x -= delta * m.scrollStep;
            } else {
                state.offset.y -= wheel.y * m.scrollStep;
                state.offset.x -= wheel.x * m.scrollStep;
            }
            if (state.offset.y < 0.0f) state.offset.y = 0.0f;
            if (state.offset.x < 0.0f) state.offset.x = 0.0f;
        }
    }

    node->scrollOffset = state.offset;

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);
    return ContainerScope(&ctx, node);
}

void ScrollArea::RenderAxisScrollbar(
    ICanvas& canvas,
    Rect viewportRect,
    float contentLength,
    float scrollOffset,
    bool isHorizontal,
    bool hovered,
    bool active,
    UiSize size,
    const UiTheme& theme,
    float cornerReserve
) {
    const ScrollbarMetrics& m = theme.GetScrollbar(size);
    const float viewLen = isHorizontal ? viewportRect.w : viewportRect.h;
    if (viewLen <= 10.0f || contentLength <= 0.0f) return;

    const float trackLen = std::max(0.0f, viewLen - 2.0f * m.margin - cornerReserve);
    if (trackLen <= 4.0f) return;

    const float effContent = std::max(viewLen, contentLength);
    const float maxScroll = effContent - viewLen;
    const float thumbLen = std::clamp((viewLen / effContent) * trackLen, m.minThumbLength, trackLen);
    const float travel = std::max(0.0f, trackLen - thumbLen);
    const float ratio = (maxScroll > 0.0f) ? std::clamp(scrollOffset / maxScroll, 0.0f, 1.0f) : 0.0f;

    Rect trackRect{};
    Rect thumbRect{};
    if (isHorizontal) {
        const float trackX = viewportRect.x + m.margin;
        const float trackY = viewportRect.Bottom() - m.trackWidth - m.margin;
        const float thumbX = trackX + ratio * travel;
        trackRect = Rect(trackX, trackY, trackLen, m.trackWidth);
        thumbRect = Rect(thumbX, trackY, thumbLen, m.trackWidth);
    } else {
        const float trackX = viewportRect.Right() - m.trackWidth - m.margin;
        const float trackY = viewportRect.y + m.margin;
        const float thumbY = trackY + ratio * travel;
        trackRect = Rect(trackX, trackY, m.trackWidth, trackLen);
        thumbRect = Rect(trackX, thumbY, m.trackWidth, thumbLen);
    }

    const auto& pal = theme.palette;
    canvas.DrawRoundRect(trackRect, m.cornerRadius, pal.borderSubtle);

    Color thumbCol = pal.textMuted;
    if (active) {
        thumbCol = pal.accent;
    } else if (hovered) {
        thumbCol = pal.textSecondary;
    }
    canvas.DrawRoundRect(thumbRect, m.cornerRadius, thumbCol);
}

void ScrollArea::UpdateAxisScrollbar(
    Context& ctx,
    Id ownerId,
    Rect viewportRect,
    float contentLength,
    float& ioScrollOffset,
    bool& ioDragging,
    float& ioDragStartOffset,
    Vec2& ioDragStartMouse,
    bool& outHovered,
    bool& outActive,
    bool isHorizontal,
    UiSize size,
    const UiTheme& theme,
    float cornerReserve
) {
    outHovered = false;
    outActive = false;

    const float prevOffset = ioScrollOffset;
    const ScrollbarMetrics& m = theme.GetScrollbar(size);
    const float viewLen = isHorizontal ? viewportRect.w : viewportRect.h;
    const float maxScroll = std::max(0.0f, contentLength - viewLen);
    ioScrollOffset = std::clamp(ioScrollOffset, 0.0f, maxScroll);

    if (viewLen <= 10.0f || maxScroll <= 0.0f) {
        ioDragging = false;
        return;
    }

    const float trackLen = std::max(0.0f, viewLen - 2.0f * m.margin - cornerReserve);
    const float thumbLen = std::clamp((viewLen / contentLength) * trackLen, m.minThumbLength, trackLen);
    const float travel = std::max(0.0f, trackLen - thumbLen);
    const float ratio = (maxScroll > 0.0f) ? (ioScrollOffset / maxScroll) : 0.0f;

    Rect trackRect{};
    Rect thumbRect{};
    Rect hitTrackRect{};
    Rect hitThumbRect{};
    const float hitExpand = m.margin + 2.0f;
    if (isHorizontal) {
        const float trackX = viewportRect.x + m.margin;
        const float trackY = viewportRect.Bottom() - m.trackWidth - m.margin;
        const float thumbX = trackX + ratio * travel;
        trackRect = Rect(trackX, trackY, trackLen, m.trackWidth);
        thumbRect = Rect(thumbX, trackY, thumbLen, m.trackWidth);
        hitTrackRect = Rect(trackX, trackY - hitExpand, trackLen, m.trackWidth + hitExpand * 2.0f);
        hitThumbRect = Rect(thumbX, trackY - hitExpand, thumbLen, m.trackWidth + hitExpand * 2.0f);
    } else {
        const float trackX = viewportRect.Right() - m.trackWidth - m.margin;
        const float trackY = viewportRect.y + m.margin;
        const float thumbY = trackY + ratio * travel;
        trackRect = Rect(trackX, trackY, m.trackWidth, trackLen);
        thumbRect = Rect(trackX, thumbY, m.trackWidth, thumbLen);
        hitTrackRect = Rect(trackX - hitExpand, trackY, m.trackWidth + hitExpand * 2.0f, trackLen);
        hitThumbRect = Rect(trackX - hitExpand, thumbY, m.trackWidth + hitExpand * 2.0f, thumbLen);
    }

    const Vec2 mousePos = ctx.GetMousePos();
    const bool mouseClicked = ctx.IsMouseClicked();
    const bool mouseDown = ctx.IsMouseDown();

    const bool hoverThumb = hitThumbRect.Contains(mousePos);
    const bool hoverTrack = hitTrackRect.Contains(mousePos);
    outHovered = hoverThumb || hoverTrack;

    if (hoverThumb && mouseClicked) {
        ioDragging = true;
        ioDragStartMouse = mousePos;
        ioDragStartOffset = ioScrollOffset;
        ctx.SetActiveId(ownerId);
    } else if (hoverTrack && mouseClicked && !hoverThumb) {
        const float mouseCoord = isHorizontal ? mousePos.x : mousePos.y;
        const float thumbCoord = isHorizontal ? thumbRect.x : thumbRect.y;
        if (mouseCoord < thumbCoord) {
            ioScrollOffset = std::max(0.0f, ioScrollOffset - viewLen * 0.8f);
        } else {
            ioScrollOffset = std::min(maxScroll, ioScrollOffset + viewLen * 0.8f);
        }
    }

    if (ioDragging) {
        outActive = true;
        if (mouseDown) {
            const float delta = isHorizontal ? (mousePos.x - ioDragStartMouse.x) : (mousePos.y - ioDragStartMouse.y);
            if (travel > 0.0f) {
                ioScrollOffset = std::clamp(ioDragStartOffset + (delta / travel) * maxScroll, 0.0f, maxScroll);
            }
        } else {
            ioDragging = false;
            if (ctx.GetActiveId() == ownerId) {
                ctx.SetActiveId(Id::Null());
            }
        }
    }

    if (ioDragging || std::abs(ioScrollOffset - prevOffset) > 0.01f) {
        ctx.RequestRepaint(2);
    }
}

void ScrollArea::RenderScrollbar(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node) return;
    const bool allowV = (node->scrollOrientation == Orientation::Vertical || node->scrollOrientation == Orientation::Both);
    const bool allowH = (node->scrollOrientation == Orientation::Horizontal || node->scrollOrientation == Orientation::Both);

    const bool showV = allowV && (
        node->scrollbarPolicy == VisibilityPolicy::Always ||
        (node->scrollbarPolicy == VisibilityPolicy::Auto && node->scrollContentSize.y > node->computedRect.h + 0.5f)
    );
    const bool showH = allowH && (
        node->scrollbarPolicy == VisibilityPolicy::Always ||
        (node->scrollbarPolicy == VisibilityPolicy::Auto && node->scrollContentSize.x > node->computedRect.w + 0.5f)
    );

    const ScrollbarMetrics& m = theme.GetScrollbar(node->uiSize);
    const float cornerReserve = (showV && showH) ? (m.trackWidth + m.margin) : 0.0f;

    if (showV) {
        RenderAxisScrollbar(
            canvas,
            node->computedRect,
            node->scrollContentSize.y,
            node->scrollOffset.y,
            false,
            node->scrollbarHovered,
            node->scrollbarActive,
            node->uiSize,
            theme,
            cornerReserve
        );
    }
    if (showH) {
        RenderAxisScrollbar(
            canvas,
            node->computedRect,
            node->scrollContentSize.x,
            node->scrollOffset.x,
            true,
            node->scrollbarHoveredX,
            node->scrollbarActiveX,
            node->uiSize,
            theme,
            cornerReserve
        );
    }
}

Rect ScrollArea::GetContentClipRect(const Node* node, const UiTheme& theme) {
    if (!node) return Rect();
    const ScrollbarMetrics& sm = theme.GetScrollbar(node->uiSize);
    const bool allowV = (node->scrollOrientation == Orientation::Vertical || node->scrollOrientation == Orientation::Both);
    const bool allowH = (node->scrollOrientation == Orientation::Horizontal || node->scrollOrientation == Orientation::Both);

    const bool showV = allowV && (
        node->scrollbarPolicy == VisibilityPolicy::Always ||
        (node->scrollbarPolicy == VisibilityPolicy::Auto && node->scrollContentSize.y > node->computedRect.h + 0.5f)
    );
    const bool showH = allowH && (
        node->scrollbarPolicy == VisibilityPolicy::Always ||
        (node->scrollbarPolicy == VisibilityPolicy::Auto && node->scrollContentSize.x > node->computedRect.w + 0.5f)
    );

    const float gutterX = showV ? sm.gutter : 0.0f;
    const float gutterY = showH ? sm.gutter : 0.0f;
    return Rect(
        node->computedRect.x + node->padding.left,
        node->computedRect.y + node->padding.top,
        std::max(0.0f, node->computedRect.w - node->padding.Horizontal() - gutterX),
        std::max(0.0f, node->computedRect.h - node->padding.Vertical() - gutterY)
    );
}

void ScrollArea::Update(Context& ctx, Node* node) {
    if (!node || !node->id.IsValid()) return;

    auto& state = ctx.GetOrCreateState<ScrollState>(node->id);
    if (node->disabled) {
        state.draggingY = false;
        state.draggingX = false;
        node->scrollbarHovered = false;
        node->scrollbarActive = false;
        node->scrollbarHoveredX = false;
        node->scrollbarActiveX = false;
        return;
    }

    const UiTheme* resolvedTheme = &ctx.GetTheme();
    for (const Node* curr = node; curr != nullptr; curr = curr->parent) {
        if (curr->theme != nullptr) {
            resolvedTheme = curr->theme;
            break;
        }
    }

    const bool allowV = (node->scrollOrientation == Orientation::Vertical || node->scrollOrientation == Orientation::Both);
    const bool allowH = (node->scrollOrientation == Orientation::Horizontal || node->scrollOrientation == Orientation::Both);
    const bool showV = allowV && (
        node->scrollbarPolicy == VisibilityPolicy::Always ||
        (node->scrollbarPolicy == VisibilityPolicy::Auto && node->scrollContentSize.y > node->computedRect.h + 0.5f)
    );
    const bool showH = allowH && (
        node->scrollbarPolicy == VisibilityPolicy::Always ||
        (node->scrollbarPolicy == VisibilityPolicy::Auto && node->scrollContentSize.x > node->computedRect.w + 0.5f)
    );
    const ScrollbarMetrics& m = resolvedTheme->GetScrollbar(node->uiSize);
    const float cornerReserve = (showV && showH) ? (m.trackWidth + m.margin) : 0.0f;

    bool hovY = false, actY = false;
    if (showV) {
        UpdateAxisScrollbar(
            ctx,
            node->id,
            node->computedRect,
            node->scrollContentSize.y,
            state.offset.y,
            state.draggingY,
            state.dragStartOffset,
            state.dragStartMouse,
            hovY,
            actY,
            false,
            node->uiSize,
            *resolvedTheme,
            cornerReserve
        );
    } else if (allowV) {
        const float maxScrollY = std::max(0.0f, node->scrollContentSize.y - node->computedRect.h);
        state.offset.y = std::clamp(state.offset.y, 0.0f, maxScrollY);
        state.draggingY = false;
    } else {
        state.offset.y = 0.0f;
        state.draggingY = false;
    }
    node->scrollbarHovered = hovY;
    node->scrollbarActive = actY;

    bool hovX = false, actX = false;
    if (showH) {
        UpdateAxisScrollbar(
            ctx,
            node->id,
            node->computedRect,
            node->scrollContentSize.x,
            state.offset.x,
            state.draggingX,
            state.dragStartOffsetX,
            state.dragStartMouse,
            hovX,
            actX,
            true,
            node->uiSize,
            *resolvedTheme,
            cornerReserve
        );
    } else if (allowH) {
        const float maxScrollX = std::max(0.0f, node->scrollContentSize.x - node->computedRect.w);
        state.offset.x = std::clamp(state.offset.x, 0.0f, maxScrollX);
        state.draggingX = false;
    } else {
        state.offset.x = 0.0f;
        state.draggingX = false;
    }
    node->scrollbarHoveredX = hovX;
    node->scrollbarActiveX = actX;

    node->scrollOffset = state.offset;
}

void ScrollArea::ScrollToChild(Context& ctx, Node* scrollNode, const Node* targetNode) {
    if (!scrollNode || !targetNode || !scrollNode->id.IsValid()) return;
    auto& state = ctx.GetOrCreateState<ScrollState>(scrollNode->id);
    const float targetTop = targetNode->computedRect.y;
    const float targetBottom = targetNode->computedRect.Bottom();
    const float viewTop = scrollNode->computedRect.y + scrollNode->padding.top;
    const float viewBottom = scrollNode->computedRect.Bottom() - scrollNode->padding.bottom;

    if (targetTop < viewTop) {
        state.offset.y -= (viewTop - targetTop);
    } else if (targetBottom > viewBottom) {
        state.offset.y += (targetBottom - viewBottom);
    }
    if (state.offset.y < 0.0f) state.offset.y = 0.0f;
    scrollNode->scrollOffset = state.offset;
}

} // namespace guikit
