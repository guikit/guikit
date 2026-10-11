#include "guikit/containers/ScrollArea.hpp"
#include <algorithm>
#include "guikit/backend/ICanvas.hpp"
#include "guikit/layout/Node.hpp"

namespace guikit {

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
