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
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();

    Node* node = ctx.CreateNode(NodeType::ScrollArea, id);
    node->direction = (opts.orientation == Orientation::Horizontal) ? FlexDirection::Row : FlexDirection::Column;
    node->clipChildren = true;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Stretch;
    node->flexGrow = 1.0f;
    node->gap = opts.gap.has_value() ? ctx.GetTheme().GetGap(*opts.gap) : 0.0f;
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
        if (prevBounds.Contains(ctx.GetMousePos()) && (wheel.x != 0.0f || wheel.y != 0.0f)) {
            const ScrollbarMetrics& m = ctx.GetTheme().GetScrollbar(activeSize);
            state.offset.y -= wheel.y * m.scrollStep;
            state.offset.x -= wheel.x * m.scrollStep;
            if (state.offset.y < 0.0f) state.offset.y = 0.0f;
            if (state.offset.x < 0.0f) state.offset.x = 0.0f;
        }
    }

    node->scrollOffset = state.offset;

    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);
    return ContainerScope(&ctx, node);
}

void ScrollArea::RenderScrollbar(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node) return;
    bool showScrollbar = false;
    if (node->scrollbarPolicy == VisibilityPolicy::Always) {
        showScrollbar = true;
    } else if (node->scrollbarPolicy == VisibilityPolicy::Auto) {
        showScrollbar = (node->scrollContentSize.y > node->computedRect.h);
    }

    if (showScrollbar && node->computedRect.h > 10.0f) {
        const ScrollbarMetrics& m = theme.GetScrollbar(node->uiSize);
        const float trackX = node->computedRect.Right() - m.trackWidth - m.margin;
        const float trackY = node->computedRect.y + m.margin;
        const float trackH = std::max(0.0f, node->computedRect.h - 2.0f * m.margin);

        const float contentH = std::max(node->computedRect.h, node->scrollContentSize.y);
        const float maxScrollY = contentH - node->computedRect.h;
        const float thumbH = std::clamp((node->computedRect.h / contentH) * trackH, m.minThumbLength, trackH);
        const float travelH = trackH - thumbH;
        const float scrollRatio = (maxScrollY > 0.0f) ? (node->scrollOffset.y / maxScrollY) : 0.0f;
        const float thumbY = trackY + scrollRatio * travelH;

        const Rect trackRect(trackX, trackY, m.trackWidth, trackH);
        const Rect thumbRect(trackX, thumbY, m.trackWidth, thumbH);

        const auto& pal = theme.palette;

        // Subtle track background
        const Color trackCol = pal.borderSubtle;
        canvas.DrawRoundRect(trackRect, m.cornerRadius, trackCol);

        // Thumb pill
        Color thumbCol = pal.textMuted;
        if (node->scrollbarActive) {
            thumbCol = pal.accent;
        } else if (node->scrollbarHovered) {
            thumbCol = pal.textSecondary;
        }

        canvas.DrawRoundRect(thumbRect, m.cornerRadius, thumbCol);
    }
}

} // namespace guikit
