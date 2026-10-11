#include "guikit/panels/Carousel.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/layout/Solver.hpp"
#include "guikit/layout/Node.hpp"

namespace guikit {

ContainerScope Carousel::Open(
    Context& ctx,
    const CarouselOptions& opts,
    std::source_location loc
) {
    return ctx.Carousel(opts, loc);
}

Vec2 Carousel::Measure(const Node* node, ICanvas* /*canvas*/, const UiTheme& /*theme*/) {
    if (!node) return {0.0f, 0.0f};

    float totalWidth = node->padding.Horizontal();
    float maxHeight = 0.0f;
    uint32_t activeChildren = 0;

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
        const float ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
        totalWidth += cw;
        maxHeight = std::max(maxHeight, ch);
        ++activeChildren;
    }

    if (activeChildren > 1) {
        totalWidth += static_cast<float>(activeChildren - 1) * node->gap;
    }
    const Vec2 size = {totalWidth, maxHeight + node->padding.Vertical()};
    const_cast<Node*>(node)->scrollContentSize = size;
    return size;
}

Vec2 Carousel::Measure(const Node* node, ICanvas* canvas) {
    return Measure(node, canvas, UiTheme::Dark());
}

void Carousel::Arrange(Node* node, const Rect& availableRect) {
    if (!node) return;

    float w = availableRect.w;
    float h = availableRect.h;

    if (node->parent == nullptr && !node->isSpacer) {
        if (node->widthMode == SizeMode::Fit) {
            w = node->intrinsicSize.x;
        } else if (node->widthMode == SizeMode::Fixed) {
            w = node->fixedWidth;
        }

        if (node->heightMode == SizeMode::Fit) {
            h = (node->childCount == 0) ? node->intrinsicSize.y : node->intrinsicSize.y;
        } else if (node->heightMode == SizeMode::Fixed) {
            h = node->fixedHeight;
        }
    } else {
        if (node->heightMode == SizeMode::Fixed) {
            h = node->fixedHeight;
        } else if (node->heightMode == SizeMode::Fit) {
            h = node->intrinsicSize.y;
        }
    }

    node->computedRect = Rect(availableRect.x, availableRect.y, w, h);
    if (node->childCount == 0) return;

    const float startX = node->computedRect.x + node->padding.left;
    const float startY = node->computedRect.y + node->padding.top;
    const float fullW = std::max(0.0f, node->computedRect.w - node->padding.Horizontal());
    const float fullH = std::max(0.0f, node->computedRect.h - node->padding.Vertical());

    // 1. Calculate total width of all children
    float totalChildrenW = 0.0f;
    uint32_t activeChildren = 0;
    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
        totalChildrenW += cw;
        ++activeChildren;
    }
    if (activeChildren > 1) {
        totalChildrenW += static_cast<float>(activeChildren - 1) * node->gap;
    }
    node->scrollContentSize = {totalChildrenW, fullH};

    // 2. Check if overflow occurs
    auto* cData = node->As<CarouselData>();
    if (totalChildrenW <= fullW) {
        // Fits within available width without scrolling
        if (cData) {
            cData->showLeftArrow = false;
            cData->showRightArrow = false;
            cData->leftArrowRect = Rect(0.0f, 0.0f, 0.0f, 0.0f);
            cData->rightArrowRect = Rect(0.0f, 0.0f, 0.0f, 0.0f);
            cData->viewportRect = Rect(startX, startY, fullW, fullH);
        }
        node->scrollOffset.x = 0.0f;

        // Arrange children in a standard row
        float curX = startX;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            float childW = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
            float childH = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight :
                           (child->heightMode == SizeMode::Stretch ? fullH : child->intrinsicSize.y);
            float childY = startY;
            if (node->align == Alignment::Center) {
                childY += (fullH - childH) * 0.5f;
            } else if (node->align == Alignment::End) {
                childY += (fullH - childH);
            }
            child->computedRect = Rect(curX, childY, childW, childH);
            LayoutSolver::Arrange(child, child->computedRect);
            curX += childW + node->gap;
        }
        return;
    }

    // 3. Overflow detected: compute arrow dimensions
    const float arrowMargin = 4.0f;
    const float arrowBtnH = std::min(fullH - 6.0f, 26.0f);
    const float arrowBtnW = arrowBtnH; // Clean square pill button
    const float arrowGap = 6.0f;
    const float arrowY = startY + (fullH - arrowBtnH) * 0.5f;
    const float btnGutter = arrowMargin + arrowBtnW + arrowGap;

    // Dynamic gutters: only allocate gutter space when an arrow is active
    bool canScrollLeft = (node->scrollOffset.x > 1.0f);
    float leftGutter = canScrollLeft ? btnGutter : 0.0f;
    bool canScrollRight = (node->scrollOffset.x < (totalChildrenW - (fullW - leftGutter)) - 1.0f);
    float rightGutter = canScrollRight ? btnGutter : 0.0f;

    float viewportX = startX + leftGutter;
    float viewportW = std::max(0.0f, fullW - leftGutter - rightGutter);
    float maxScrollX = std::max(0.0f, totalChildrenW - viewportW);

    // Auto-scroll to active tab if requested
    if (cData && cData->scrollToActiveTab) {
        float relX = 0.0f;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
            if (child->active) {
                if (relX < node->scrollOffset.x) {
                    node->scrollOffset.x = relX;
                } else if (relX + cw > node->scrollOffset.x + viewportW) {
                    node->scrollOffset.x = (relX + cw) - viewportW;
                }
                break;
            }
            relX += cw + node->gap;
        }

        // Recompute gutters after auto-scroll updated scrollOffset
        canScrollLeft = (node->scrollOffset.x > 1.0f);
        leftGutter = canScrollLeft ? btnGutter : 0.0f;
        canScrollRight = (node->scrollOffset.x < (totalChildrenW - (fullW - leftGutter)) - 1.0f);
        rightGutter = canScrollRight ? btnGutter : 0.0f;

        viewportX = startX + leftGutter;
        viewportW = std::max(0.0f, fullW - leftGutter - rightGutter);
        maxScrollX = std::max(0.0f, totalChildrenW - viewportW);
    }

    // Clamp scroll offset
    node->scrollOffset.x = std::clamp(node->scrollOffset.x, 0.0f, maxScrollX);

    if (cData) {
        cData->showLeftArrow = canScrollLeft;
        cData->showRightArrow = canScrollRight;

        cData->leftArrowRect = canScrollLeft
            ? Rect(startX + arrowMargin, arrowY, arrowBtnW, arrowBtnH)
            : Rect(0.0f, 0.0f, 0.0f, 0.0f);
        cData->rightArrowRect = canScrollRight
            ? Rect(startX + fullW - arrowBtnW - arrowMargin, arrowY, arrowBtnW, arrowBtnH)
            : Rect(0.0f, 0.0f, 0.0f, 0.0f);
        cData->viewportRect = Rect(viewportX, startY, viewportW, fullH);
    }

    // Arrange children within the scrollable viewport
    float curX = viewportX - node->scrollOffset.x;
    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        float childW = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
        float childH = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight :
                       (child->heightMode == SizeMode::Stretch ? fullH : child->intrinsicSize.y);
        float childY = startY;
        if (node->align == Alignment::Center) {
            childY += (fullH - childH) * 0.5f;
        } else if (node->align == Alignment::End) {
            childY += (fullH - childH);
        }
        child->computedRect = Rect(curX, childY, childW, childH);
        LayoutSolver::Arrange(child, child->computedRect);
        curX += childW + node->gap;
    }
}

void Carousel::RenderArrows(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node) return;
    RenderArrows(canvas, const_cast<Node*>(node), node->uiSize, theme);
}

void Carousel::RenderArrows(
    ICanvas& canvas,
    Node* node,
    UiSize size,
    const UiTheme& theme
) {
    if (!node) return;
    const auto& pal = theme.palette;
    const ComponentMetrics& m = theme.GetMetrics(size);
    const VariantColors& ghost = theme.GetVariant(UiVariant::Ghost);

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

    auto* cData = node->As<CarouselData>();
    if (!cData) return;

    if (cData->showLeftArrow) {
        drawArrow(cData->leftArrowRect, cData->leftArrowHovered, cData->leftArrowPressed, true);
    }
    if (cData->showRightArrow) {
        drawArrow(cData->rightArrowRect, cData->rightArrowHovered, cData->rightArrowPressed, false);
    }

    // 3. Continuous baseline separator across entire bar width in Underline style
    if (cData->showBaselineSeparator) {
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
