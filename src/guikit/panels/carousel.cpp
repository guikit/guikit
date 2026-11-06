#include "guikit/panels/carousel.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/solver.hpp"
#include "guikit/core/node.hpp"
#include "guikit/panels/scroll_area.hpp"

namespace guikit {

ContainerScope Carousel::Open(
    Context& ctx,
    const CarouselOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const CarouselMetrics& cm = ctx.GetTheme().GetCarousel(activeSize);

    Node* node = ctx.CreateNode(NodeType::Carousel, id);
    node->theme = pushedTheme;
    node->direction = FlexDirection::Row;
    node->align = opts.align;
    node->justify = opts.justify;
    node->clipChildren = true;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fit;
    node->gap = ctx.GetTheme().ResolveGap(opts.gap, activeSize);
    node->padding = ctx.GetTheme().ResolvePadding(opts.padding);
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    node->measureFn = &Carousel::Measure;
    node->arrangeFn = &Carousel::Arrange;
    node->postRenderFn = &Carousel::RenderArrows;

    auto* cData = ctx.AllocPayload<CarouselData>();
    cData->opts = opts;
    node->SetPayload(cData);

    BeginScrollInteraction(ctx, node, cData, opts.scrollWheel, activeDisabled);
    node->visualHash = detail::HashArgs(
        cData->leftArrowHovered,
        cData->leftArrowPressed,
        cData->rightArrowHovered,
        cData->rightArrowPressed
    );

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);
    return ContainerScope(&ctx, node);
}

void Carousel::BeginScrollInteraction(
    Context& ctx,
    Node* node,
    CarouselData* cData,
    bool scrollWheel,
    bool disabled
) {
    if (!node || !cData || !node->id.IsValid()) return;
    const Id id = node->id;
    const CarouselMetrics& cm = ctx.GetTheme().GetCarousel(node->uiSize);
    auto& state = ctx.GetOrCreateState<ScrollState>(id);
    if (!state.animatingX) {
        state.targetOffsetX = state.offset.x;
    }

    const Id leftArrowId = GetCarouselLeftArrowId(id);
    const Id rightArrowId = GetCarouselRightArrowId(id);
    const Response leftRes = ctx.Interact(leftArrowId);
    const Response rightRes = ctx.Interact(rightArrowId);

    cData->leftArrowHovered = leftRes.hovered;
    cData->leftArrowPressed = leftRes.pressed;
    cData->rightArrowHovered = rightRes.hovered;
    cData->rightArrowPressed = rightRes.pressed;
    cData->baseTargetX = state.animatingX ? state.targetOffsetX : state.offset.x;
    cData->targetOffsetX = cData->baseTargetX;
    cData->animatingX = state.animatingX;

    if (leftRes.clicked && !disabled) {
        cData->stepDirection = -1;
        state.lastAnimTimeSec = ctx.GetCurrentTimeSec();
        ctx.RequestRepaint(3);
    }
    if (rightRes.clicked && !disabled) {
        cData->stepDirection = 1;
        state.lastAnimTimeSec = ctx.GetCurrentTimeSec();
        ctx.RequestRepaint(3);
    }

    if (scrollWheel && !disabled) {
        const Rect prevBounds = ctx.GetPrevFrameBounds(id);
        if (prevBounds.w > 0.0f && prevBounds.h > 0.0f && ctx.IsRectHovered(id, prevBounds)) {
            const Vec2 wheel = ctx.GetMouseWheel();
            const float wheelDelta = (wheel.x != 0.0f) ? wheel.x : wheel.y;
            if (wheelDelta != 0.0f) {
                const float baseX = state.animatingX ? state.targetOffsetX : state.offset.x;
                state.targetOffsetX = std::max(0.0f, baseX - wheelDelta * (cm.scrollStep * 0.45f));
                state.animatingX = true;
                state.lastAnimTimeSec = ctx.GetCurrentTimeSec();
                cData->targetOffsetX = state.targetOffsetX;
                cData->animatingX = true;
                ctx.ConsumeMouseWheel();
                ctx.RequestRepaint(3);
            }
        }
    }

    node->scrollOffset = state.offset;
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
        if (child->type == NodeType::Modal) continue;
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
            cData->targetOffsetX = 0.0f;
            cData->animatingX = false;
            cData->stepDirection = 0;
        }
        node->scrollOffset.x = 0.0f;

        // Arrange children in a standard row
        float curX = startX;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Modal) continue;
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
            Solver::Arrange(child, child->computedRect, theme);
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
    const float maxScrollX = std::max(0.0f, totalChildrenW - fullW);

    // Resolve item-boundary stepping when a navigation arrow was clicked
    if (cData && cData->stepDirection != 0) {
        const int8_t dir = cData->stepDirection;
        cData->stepDirection = 0;

        const float baseX = std::clamp(cData->baseTargetX, 0.0f, maxScrollX);
        const float visLeft = baseX + ((baseX > 1.0f) ? btnGutter : 0.0f);
        const float visRight = baseX + fullW - ((baseX < maxScrollX - 1.0f) ? btnGutter : 0.0f);
        const float midViewportW = std::max(1.0f, fullW - 2.0f * btnGutter);
        const float minAdvance = std::min(cm.scrollStep * 0.5f, midViewportW * 0.5f);
        float nextTarget = baseX;

        if (dir > 0) {
            float rX = -1.0f;
            float rRight = -1.0f;
            float relX = 0.0f;
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type == NodeType::Modal) continue;
                const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                const bool isSep = (child->type == NodeType::Separator || cw <= 4.0f);
                if (!isSep && (relX + cw > visRight + 1.0f)) {
                    rX = relX;
                    rRight = relX + cw;
                    break;
                }
                relX += cw + node->gap;
            }

            if (rX < 0.0f || (totalChildrenW - rX <= fullW - btnGutter + 1.0f)) {
                nextTarget = maxScrollX;
            } else {
                const float minOffsetForR = rRight - (fullW - btnGutter);
                const float maxLeadOffset = std::max(baseX + 2.0f, rX - btnGutter);
                const float desiredMin = std::min(maxLeadOffset, std::max(baseX + minAdvance, minOffsetForR));
                bool foundSnap = false;
                relX = 0.0f;
                for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                    if (child->type == NodeType::Modal) continue;
                    const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                    if (relX > rX + 0.5f) break;
                    const bool isSep = (child->type == NodeType::Separator || cw <= 4.0f);
                    if (!isSep) {
                        const float cand = relX - btnGutter;
                        if (cand >= desiredMin - 0.5f) {
                            nextTarget = cand;
                            foundSnap = true;
                            break;
                        }
                    }
                    relX += cw + node->gap;
                }
                if (!foundSnap) {
                    nextTarget = maxLeadOffset;
                }
                if (maxScrollX - nextTarget <= btnGutter + 8.0f) {
                    nextTarget = maxScrollX;
                }
            }
        } else {
            float lX = -1.0f;
            float lRight = -1.0f;
            float relX = 0.0f;
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type == NodeType::Modal) continue;
                const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                const bool isSep = (child->type == NodeType::Separator || cw <= 4.0f);
                if (!isSep && (relX < visLeft - 1.0f)) {
                    lX = relX;
                    lRight = relX + cw;
                }
                relX += cw + node->gap;
            }

            if (lX < 0.0f || (lRight <= fullW - btnGutter + 1.0f)) {
                nextTarget = 0.0f;
            } else {
                const float minOffsetKeepLVisible = lRight - (fullW - btnGutter);
                const float maxOffsetForL = lX - btnGutter;
                const float effMinOffset = std::min(maxOffsetForL, std::max(minOffsetKeepLVisible, baseX - midViewportW));
                bool foundSnap = false;
                relX = 0.0f;
                for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                    if (child->type == NodeType::Modal) continue;
                    const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                    if (relX > lX + 0.5f) break;
                    const bool isSep = (child->type == NodeType::Separator || cw <= 4.0f);
                    if (!isSep) {
                        const float cand = relX - btnGutter;
                        if (cand >= effMinOffset - 0.5f && cand <= maxOffsetForL + 0.5f) {
                            nextTarget = cand;
                            foundSnap = true;
                            break;
                        }
                    }
                    relX += cw + node->gap;
                }
                if (!foundSnap) {
                    nextTarget = maxOffsetForL;
                }
                if (nextTarget <= btnGutter + 8.0f) {
                    nextTarget = 0.0f;
                }
            }
        }

        cData->targetOffsetX = std::clamp(nextTarget, 0.0f, maxScrollX);
        cData->animatingX = true;
    }

    // Auto-scroll to active tab if requested
    if (cData && cData->scrollToActiveTab) {
        cData->scrollToActiveTab = false;
        const float baseX = cData->animatingX ? cData->targetOffsetX : node->scrollOffset.x;
        const float visLeft = baseX + ((baseX > 1.0f) ? btnGutter : 0.0f);
        const float visRight = baseX + fullW - ((baseX < maxScrollX - 1.0f) ? btnGutter : 0.0f);
        float relX = 0.0f;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Modal) continue;
            const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
            if (child->active) {
                float newTarget = baseX;
                if (relX < visLeft) {
                    newTarget = (relX + cw <= fullW - btnGutter || relX <= btnGutter + 8.0f)
                        ? 0.0f
                        : (relX - btnGutter);
                } else if (relX + cw > visRight) {
                    newTarget = (totalChildrenW - relX <= fullW - btnGutter)
                        ? maxScrollX
                        : ((relX + cw) - (fullW - btnGutter));
                }
                newTarget = std::clamp(newTarget, 0.0f, maxScrollX);
                if (std::abs(newTarget - node->scrollOffset.x) > 0.5f) {
                    cData->targetOffsetX = newTarget;
                    cData->animatingX = true;
                }
                break;
            }
            relX += cw + node->gap;
        }
    }

    // Clamp scroll offset
    node->scrollOffset.x = std::clamp(node->scrollOffset.x, 0.0f, maxScrollX);

    // Dynamic gutters: only allocate gutter space when an arrow is active
    const bool canScrollLeft = (node->scrollOffset.x > 1.0f);
    const float leftGutter = canScrollLeft ? btnGutter : 0.0f;
    const bool canScrollRight = (node->scrollOffset.x < maxScrollX - 1.0f);
    const float rightGutter = canScrollRight ? btnGutter : 0.0f;

    const float viewportX = startX + leftGutter;
    const float viewportW = std::max(0.0f, fullW - leftGutter - rightGutter);

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

    // Arrange children smoothly relative to startX so toggling leftGutter never causes a position jump
    float curX = startX - node->scrollOffset.x;
    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        if (child->type == NodeType::Modal) continue;
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
        Solver::Arrange(child, child->computedRect, theme);
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

    const float sepInsetY = std::max(4.0f, std::round(node->computedRect.h * 0.18f));
    const float sepY1 = node->computedRect.y + sepInsetY;
    const float sepY2 = node->computedRect.Bottom() - sepInsetY;
    const Color sepCol = pal.borderSubtle.WithAlphaF(0.65f);

    if (cData->showLeftArrow) {
        drawArrow(cData->leftArrowRect, cData->leftArrowHovered, cData->leftArrowPressed, true);
        if (sepY2 > sepY1) {
            const float sepX = std::floor(cData->viewportRect.x) + 0.5f;
            canvas.DrawLine(Vec2(sepX, sepY1), Vec2(sepX, sepY2), sepCol, 1.0f);
        }
    }
    if (cData->showRightArrow) {
        if (sepY2 > sepY1) {
            const float sepX = std::floor(cData->viewportRect.Right()) - 0.5f;
            canvas.DrawLine(Vec2(sepX, sepY1), Vec2(sepX, sepY2), sepCol, 1.0f);
        }
        drawArrow(cData->rightArrowRect, cData->rightArrowHovered, cData->rightArrowPressed, false);
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
    auto* cData = node->As<CarouselData>();
    const float fullW = std::max(0.0f, node->computedRect.w - node->padding.Horizontal());
    const float maxScrollX = std::max(0.0f, node->scrollContentSize.x - fullW);

    if (maxScrollX <= 0.5f) {
        state.offset.x = 0.0f;
        state.targetOffsetX = 0.0f;
        state.animatingX = false;
        if (cData) cData->animatingX = false;
        node->scrollOffset.x = 0.0f;
        return;
    }

    if (cData && cData->animatingX) {
        state.targetOffsetX = std::clamp(cData->targetOffsetX, 0.0f, maxScrollX);
        state.animatingX = true;
    } else if (!state.animatingX) {
        state.offset.x = std::clamp(node->scrollOffset.x, 0.0f, maxScrollX);
        state.targetOffsetX = state.offset.x;
    }

    if (state.animatingX) {
        state.targetOffsetX = std::clamp(state.targetOffsetX, 0.0f, maxScrollX);
        const double now = ctx.GetCurrentTimeSec();
        float dt = static_cast<float>(now - state.lastAnimTimeSec);
        state.lastAnimTimeSec = now;
        if (dt <= 0.0001f || dt > 0.25f) {
            dt = 0.016f;
        }
        const float diff = state.targetOffsetX - state.offset.x;
        if (std::abs(diff) <= 0.75f) {
            state.offset.x = state.targetOffsetX;
            state.animatingX = false;
            if (cData) cData->animatingX = false;
            ctx.RequestRepaint(2);
        } else {
            const float alpha = 1.0f - std::exp(-20.0f * dt);
            const float stepMove = diff * alpha;
            const float minMove = (diff > 0.0f) ? std::min(diff, 0.75f) : std::max(diff, -0.75f);
            state.offset.x += (std::abs(stepMove) < std::abs(minMove)) ? minMove : stepMove;
            state.offset.x = std::clamp(state.offset.x, 0.0f, maxScrollX);
            if (cData) cData->animatingX = true;
            ctx.RequestRepaint(2);
        }
        node->scrollOffset.x = state.offset.x;
    } else {
        state.offset.x = node->scrollOffset.x;
        state.targetOffsetX = state.offset.x;
    }
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
    state.targetOffsetX = state.offset.x;
    state.animatingX = false;
    carouselNode->scrollOffset = state.offset;
}

void Carousel::CollectBounds(
    const Node* node,
    keel::HashMap<Id, Rect>& outBounds,
    Rect parentClip
) {
    if (!node || (node->type != NodeType::Carousel && node->type != NodeType::Toolbar)) return;
    const auto* cData = node->As<CarouselData>();
    if (!cData) return;

    if (cData->showLeftArrow && cData->leftArrowRect.w > 0.0f) {
        const Rect clippedLeft = cData->leftArrowRect.Intersect(parentClip);
        if (clippedLeft.w > 0.0f && clippedLeft.h > 0.0f) {
            outBounds.put(GetCarouselLeftArrowId(node->id), clippedLeft);
        }
    }
    if (cData->showRightArrow && cData->rightArrowRect.w > 0.0f) {
        const Rect clippedRight = cData->rightArrowRect.Intersect(parentClip);
        if (clippedRight.w > 0.0f && clippedRight.h > 0.0f) {
            outBounds.put(GetCarouselRightArrowId(node->id), clippedRight);
        }
    }
}

} // namespace guikit
