#include "guikit/panels/Carousel.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Solver.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/panels/ScrollArea.hpp"

namespace guikit {

ContainerScope Carousel::Open(
    Context& ctx,
    const CarouselOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());

    UiTheme* pushedTheme = nullptr;
    if (opts.theme != nullptr || opts.scale.has_value()) {
        UiTheme scopedTheme = opts.theme ? *opts.theme : ctx.GetTheme();
        if (opts.scale.has_value() && *opts.scale > 0.0f) {
            scopedTheme = scopedTheme.Scaled(*opts.scale);
        }
        pushedTheme = ctx.PushTheme(scopedTheme);
    }

    const CarouselMetrics& cm = ctx.GetTheme().GetCarousel(activeSize);

    Node* node = ctx.CreateNode(NodeType::Carousel, id);
    node->theme = pushedTheme;
    node->direction = FlexDirection::Row;
    node->align = opts.align;
    node->justify = opts.justify;
    node->clipChildren = true;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fit;
    node->gap = opts.gap.value_or(ctx.GetTheme().GetGap(activeSize));
    node->padding = ctx.GetTheme().ResolvePadding(opts.padding);
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    node->measureFn = &Carousel::Measure;
    node->arrangeFn = &Carousel::Arrange;
    node->postRenderFn = &Carousel::RenderArrows;

    auto* cData = ctx.AllocPayload<CarouselData>();
    cData->opts = opts;
    node->payload = cData;

    auto& state = ctx.GetOrCreateState<ScrollState>(id);

    // Handle interactive arrow button clicks from previous frame
    const Id leftArrowId = GetCarouselLeftArrowId(id);
    const Id rightArrowId = GetCarouselRightArrowId(id);
    Response leftRes = ctx.Interact(leftArrowId);
    Response rightRes = ctx.Interact(rightArrowId);

    cData->leftArrowHovered = leftRes.hovered;
    cData->leftArrowPressed = leftRes.pressed;
    cData->rightArrowHovered = rightRes.hovered;
    cData->rightArrowPressed = rightRes.pressed;

    const float step = cm.scrollStep;

    if (leftRes.clicked && !activeDisabled) {
        state.offset.x = std::max(0.0f, state.offset.x - step);
        ctx.RequestRepaint(3);
    }
    if (rightRes.clicked && !activeDisabled) {
        state.offset.x += step;
        ctx.RequestRepaint(3);
    }

    // Handle horizontal mouse wheel scroll
    if (opts.scrollWheel && !activeDisabled) {
        Rect prevBounds = ctx.GetPrevFrameBounds(id);
        if (prevBounds.w > 0.0f && prevBounds.h > 0.0f && prevBounds.Contains(ctx.GetMousePos())) {
            Vec2 wheel = ctx.GetMouseWheel();
            const float wheelDelta = (wheel.x != 0.0f) ? wheel.x : wheel.y;
            if (wheelDelta != 0.0f) {
                state.offset.x -= wheelDelta * (cm.scrollStep * 0.35f);
                if (state.offset.x < 0.0f) state.offset.x = 0.0f;
                ctx.RequestRepaint(3);
            }
        }
    }

    node->scrollOffset = state.offset;

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);
    return ContainerScope(&ctx, node);
}

Vec2 Carousel::Measure(const Node* node, float /*availableWidth*/, ICanvas* /*canvas*/, const UiTheme& /*theme*/) {
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
    if (node->widthMode == SizeMode::Fit) {
        return size;
    } else if (node->widthMode == SizeMode::Fixed) {
        return Vec2{node->fixedWidth, size.y};
    } else {
        return Vec2{0.0f, size.y};
    }
}

Vec2 Carousel::Measure(const Node* node, float availableWidth, ICanvas* canvas) {
    return Measure(node, availableWidth, canvas, UiTheme::Dark());
}

void Carousel::Arrange(Node* node, const Rect& availableRect, const UiTheme& theme) {
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
            LayoutSolver::Arrange(child, child->computedRect, theme);
            curX += childW + node->gap;
        }
        return;
    }

    // 3. Overflow detected: compute arrow dimensions from theme tokens
    const CarouselMetrics& cm = theme.GetCarousel(node->uiSize);
    const float arrowMargin = cm.margin;
    const float arrowBtnH = std::max(0.0f, std::min(fullH - cm.margin * 2.0f, cm.arrowButtonSize));
    const float arrowBtnW = arrowBtnH; // Clean square button
    const float arrowGap = cm.gap;
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
        LayoutSolver::Arrange(child, child->computedRect, theme);
        curX += childW + node->gap;
    }
}

void Carousel::Arrange(Node* node, const Rect& availableRect) {
    Arrange(node, availableRect, UiTheme::Dark());
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
    const CarouselMetrics& cm = theme.GetCarousel(size);
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
            canvas.DrawRoundRect(rect, cm.cornerRadius, bg);
        }

        const float isz = std::min(rect.w, cm.iconSize);
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

Rect Carousel::GetContentClipRect(const Node* node) {
    if (!node) return Rect();
    const auto* cData = node->As<CarouselData>();
    return cData ? cData->viewportRect : node->computedRect;
}

void Carousel::Update(Context& ctx, Node* node) {
    if (!node || !node->id.IsValid()) return;
    auto& state = ctx.GetOrCreateState<ScrollState>(node->id);
    state.offset.x = node->scrollOffset.x;
}

void Carousel::ScrollToChild(Context& ctx, Node* carouselNode, const Node* targetNode) {
    if (!carouselNode || !targetNode || !carouselNode->id.IsValid()) return;
    auto& state = ctx.GetOrCreateState<ScrollState>(carouselNode->id);
    const float targetLeft = targetNode->computedRect.x;
    const float targetRight = targetNode->computedRect.Right();
    const auto* cData = carouselNode->As<CarouselData>();
    const float viewLeft = cData ? cData->viewportRect.x : carouselNode->computedRect.x;
    const float viewRight = cData ? cData->viewportRect.Right() : carouselNode->computedRect.Right();

    if (targetLeft < viewLeft) {
        state.offset.x -= (viewLeft - targetLeft);
    } else if (targetRight > viewRight) {
        state.offset.x += (targetRight - viewRight);
    }
    if (state.offset.x < 0.0f) state.offset.x = 0.0f;
    carouselNode->scrollOffset = state.offset;
}

void Carousel::CollectBounds(
    const Node* node,
    std::unordered_map<Id, Rect>& outBounds,
    Rect parentClip
) {
    if (!node || node->type != NodeType::Carousel) return;
    const auto* cData = node->As<CarouselData>();
    if (!cData) return;

    if (cData->showLeftArrow && cData->leftArrowRect.w > 0.0f) {
        const Rect clippedLeft = cData->leftArrowRect.Intersect(parentClip);
        if (clippedLeft.w > 0.0f && clippedLeft.h > 0.0f) {
            outBounds[GetCarouselLeftArrowId(node->id)] = clippedLeft;
        }
    }
    if (cData->showRightArrow && cData->rightArrowRect.w > 0.0f) {
        const Rect clippedRight = cData->rightArrowRect.Intersect(parentClip);
        if (clippedRight.w > 0.0f && clippedRight.h > 0.0f) {
            outBounds[GetCarouselRightArrowId(node->id)] = clippedRight;
        }
    }
}

} // namespace guikit
