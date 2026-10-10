#include "guikit/widgets/Carousel.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/layout/Node.hpp"

namespace guikit {

CarouselScope Carousel::Open(
    Context& ctx,
    const CarouselOptions& opts,
    std::source_location loc
) {
    return ctx.Carousel(opts, loc);
}

void Carousel::RenderArrows(
    ICanvas& canvas,
    Node* node,
    UiSize size,
    ThemeMode theme
) {
    if (!node) return;
    const auto pal = GetThemePalette(theme);
    const ComponentMetrics m = GetComponentMetrics(size);
    const VariantColors ghost = GetVariantColors(UiVariant::Ghost, theme);

    // Refined ghost chevron navigation buttons
    auto drawArrow = [&](const Rect& rect, bool hovered, bool pressed, bool isLeft) {
        if (rect.w <= 0.0f || rect.h <= 0.0f) return;

        Color bg = Color::Transparent();
        Color iconCol = pal.textSecondary;

        if (pressed) {
            bg = ghost.bgActive;
            iconCol = pal.accent;
        } else if (hovered) {
            bg = ghost.bgHover;
            iconCol = ghost.text;
        }

        if (bg.a > 0) {
            canvas.DrawRoundRect(rect, m.cornerRadius, bg);
        }

        const float isz = std::clamp(m.iconSize * 0.85f, 13.0f, 15.0f);
        const Rect iconBounds(
            rect.x + (rect.w - isz) * 0.5f,
            rect.y + (rect.h - isz) * 0.5f,
            isz, isz
        );

        Icon arrowIcon = isLeft ? Icon::ChevronLeft() : Icon::ChevronRight();
        arrowIcon.Draw(canvas, iconBounds, iconCol);
    };

    if (node->showLeftArrow) {
        drawArrow(node->leftArrowRect, node->leftArrowHovered, node->leftArrowPressed, true);
    }
    if (node->showRightArrow) {
        drawArrow(node->rightArrowRect, node->rightArrowHovered, node->rightArrowPressed, false);
    }

    // 3. Continuous baseline separator across entire bar width in Underline style
    if (node->tabBarOpts.showSeparator && node->tabBarOpts.style == TabBarStyle::Underline) {
        const float sepY = node->computedRect.Bottom() - 0.5f;
        canvas.DrawLine(
            Vec2(node->computedRect.x, sepY),
            Vec2(node->computedRect.Right(), sepY),
            pal.borderSubtle,
            1.0f
        );
    }
}

} // namespace guikit
