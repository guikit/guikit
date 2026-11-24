#include "guikit/surfaces/carousel.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/solver.hpp"
#include "guikit/layout/scroll_area.hpp"
#include "keel/math.h"

namespace guikit {

ContainerScope detail::Carousel::Open(
    Context& ctx,
    const CarouselOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    Node* node = ctx.CreateNode(NodeType::Carousel, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const keel::f32 density = node->GetDensity();
    const UiSize activeSize = opts.size;
    const bool activeDisabled = node->disabled;
    node->direction = FlexDirection::Row;
    node->align = opts.align;
    node->justify = opts.justify;
    node->clipChildren = true;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fit;
    node->gap = theme.ResolveGap(opts.gap, activeSize, density);
    node->padding = theme.ResolvePadding(opts.padding, UiSize::None, density);
    node->uiSize = activeSize;

    static constexpr PanelHooks kCarouselHooks{
        .contentClip = [](const Node* n, const UiTheme&) -> Rect { return detail::Carousel::GetContentClipRect(n); },
        .update = &detail::Carousel::Update,
        .scrollToChild = &detail::Carousel::ScrollToChild,
        .collectBounds = &detail::Carousel::CollectBounds
    };
    node->measureFn = &detail::Carousel::Measure;
    node->arrangeFn = &detail::Carousel::Arrange;
    node->panelHooks = &kCarouselHooks;
    node->postRenderFn = &detail::Carousel::RenderArrows;

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

    ctx.FinishContainer(node);
    return ContainerScope(&ctx, node);
}

void detail::Carousel::BeginScrollInteraction(
    Context& ctx,
    Node* node,
    CarouselData* cData,
    bool scrollWheel,
    bool disabled
) {
    if (!node || !cData || !node->id.IsValid()) return;
    if (!node->scroll) {
        node->scroll = ctx.AllocPayload<ScrollData>();
        node->scroll->orientation = Orientation::Horizontal;
        node->scroll->solverManaged = false;
    }
    const Id id = node->id;
    const CarouselMetrics& cm = ctx.GetTheme().GetCarousel(node->uiSize);
    auto& state = ctx.GetOrCreatePersistentState<ScrollState>(id);
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
            const keel::f32 wheelDelta = (wheel.x != 0.0f) ? wheel.x : wheel.y;
            if (wheelDelta != 0.0f) {
                const keel::f32 baseX = state.animatingX ? state.targetOffsetX : state.offset.x;
                state.targetOffsetX = keel::max(0.0f, baseX - wheelDelta * (cm.scrollStep * 0.45f));
                state.animatingX = true;
                state.lastAnimTimeSec = ctx.GetCurrentTimeSec();
                cData->targetOffsetX = state.targetOffsetX;
                cData->animatingX = true;
                ctx.ConsumeMouseWheel();
                ctx.RequestRepaint(3);
            }
        }
    }

    if (node->scroll) {
        node->scroll->offset = state.offset;
    }
}

Vec2 detail::Carousel::Measure(const Node* node, keel::f32 /*availableWidth*/, ICanvas* /*canvas*/, const UiTheme& /*theme*/) {
    if (!node) return {0.0f, 0.0f};

    keel::f32 totalWidth = node->padding.Horizontal();
    keel::f32 maxHeight = 0.0f;
    keel::u32 activeChildren = 0;

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
        const keel::f32 ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
        totalWidth += cw;
        maxHeight = keel::max(maxHeight, ch);
        ++activeChildren;
    }

    if (activeChildren > 1) {
        totalWidth += keel::as<keel::f32>(activeChildren - 1) * node->gap;
    }
    const Vec2 size = {totalWidth, maxHeight + node->padding.Vertical()};
    if (node->scroll) {
        node->scroll->contentSize = size;
    }
    if (node->widthMode == SizeMode::Fit) {
        return size;
    } else if (node->widthMode == SizeMode::Fixed) {
        return Vec2{node->fixedWidth, size.y};
    } else {
        return Vec2{0.0f, size.y};
    }
}

Vec2 detail::Carousel::Measure(const Node* node, keel::f32 availableWidth, ICanvas* canvas) {
    return Measure(node, availableWidth, canvas, UiTheme::Dark());
}

void detail::Carousel::Arrange(Node* node, const Rect& availableRect, const UiTheme& theme) {
    if (!node) return;

    keel::f32 w = availableRect.w;
    keel::f32 h = availableRect.h;

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

    const keel::f32 startX = node->computedRect.x + node->padding.left;
    const keel::f32 startY = node->computedRect.y + node->padding.top;
    const keel::f32 fullW = keel::max(0.0f, node->computedRect.w - node->padding.Horizontal());
    const keel::f32 fullH = keel::max(0.0f, node->computedRect.h - node->padding.Vertical());

    // 1. Calculate total width of all children
    keel::f32 totalChildrenW = 0.0f;
    keel::u32 activeChildren = 0;
    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        if (child->IsOutOfFlow()) continue;
        const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
        totalChildrenW += cw;
        ++activeChildren;
    }
    if (activeChildren > 1) {
        totalChildrenW += keel::as<keel::f32>(activeChildren - 1) * node->gap;
    }
    if (node->scroll) {
        node->scroll->contentSize = {totalChildrenW, fullH};
    }

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
        if (node->scroll) {
            node->scroll->offset.x = 0.0f;
        }

        // Arrange children in a standard row
        keel::f32 curX = startX;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->IsOutOfFlow()) continue;
            keel::f32 childW = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
            keel::f32 childH = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight :
                           (child->heightMode == SizeMode::Stretch ? fullH : child->intrinsicSize.y);
            keel::f32 childY = startY;
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
    const keel::f32 arrowMargin = cm.margin;
    const keel::f32 arrowBtnH = keel::max(0.0f, keel::min(fullH - cm.margin * 2.0f, cm.arrowButtonSize));
    const keel::f32 arrowBtnW = arrowBtnH; // Clean square button
    const keel::f32 arrowGap = cm.gap;
    const keel::f32 arrowY = startY + (fullH - arrowBtnH) * 0.5f;
    const keel::f32 btnGutter = arrowMargin + arrowBtnW + arrowGap;
    const keel::f32 maxScrollX = keel::max(0.0f, totalChildrenW - fullW);

    // Resolve item-boundary stepping when a navigation arrow was clicked
    if (cData && cData->stepDirection != 0) {
        const keel::i8 dir = cData->stepDirection;
        cData->stepDirection = 0;

        const keel::f32 baseX = keel::clamp(cData->baseTargetX, 0.0f, maxScrollX);
        const keel::f32 visLeft = baseX + ((baseX > 1.0f) ? btnGutter : 0.0f);
        const keel::f32 visRight = baseX + fullW - ((baseX < maxScrollX - 1.0f) ? btnGutter : 0.0f);
        const keel::f32 midViewportW = keel::max(1.0f, fullW - 2.0f * btnGutter);
        const keel::f32 minAdvance = keel::min(cm.scrollStep * 0.5f, midViewportW * 0.5f);
        keel::f32 nextTarget = baseX;

        if (dir > 0) {
            keel::f32 rX = -1.0f;
            keel::f32 rRight = -1.0f;
            keel::f32 relX = 0.0f;
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->IsOutOfFlow()) continue;
                const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
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
                const keel::f32 minOffsetForR = rRight - (fullW - btnGutter);
                const keel::f32 maxLeadOffset = keel::max(baseX + 2.0f, rX - btnGutter);
                const keel::f32 desiredMin = keel::min(maxLeadOffset, keel::max(baseX + minAdvance, minOffsetForR));
                bool foundSnap = false;
                relX = 0.0f;
                for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                    if (child->IsOutOfFlow()) continue;
                    const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                    if (relX > rX + 0.5f) break;
                    const bool isSep = (child->type == NodeType::Separator || cw <= 4.0f);
                    if (!isSep) {
                        const keel::f32 cand = relX - btnGutter;
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
            keel::f32 lX = -1.0f;
            keel::f32 lRight = -1.0f;
            keel::f32 relX = 0.0f;
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->IsOutOfFlow()) continue;
                const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
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
                const keel::f32 minOffsetKeepLVisible = lRight - (fullW - btnGutter);
                const keel::f32 maxOffsetForL = lX - btnGutter;
                const keel::f32 effMinOffset = keel::min(maxOffsetForL, keel::max(minOffsetKeepLVisible, baseX - midViewportW));
                bool foundSnap = false;
                relX = 0.0f;
                for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                    if (child->IsOutOfFlow()) continue;
                    const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                    if (relX > lX + 0.5f) break;
                    const bool isSep = (child->type == NodeType::Separator || cw <= 4.0f);
                    if (!isSep) {
                        const keel::f32 cand = relX - btnGutter;
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

        cData->targetOffsetX = keel::clamp(nextTarget, 0.0f, maxScrollX);
        cData->animatingX = true;
    }

    keel::f32 scrollX = node->scroll ? node->scroll->offset.x : 0.0f;

    // Auto-scroll to active tab if requested
    if (cData && cData->scrollToActiveTab) {
        cData->scrollToActiveTab = false;
        const keel::f32 baseX = cData->animatingX ? cData->targetOffsetX : scrollX;
        const keel::f32 visLeft = baseX + ((baseX > 1.0f) ? btnGutter : 0.0f);
        const keel::f32 visRight = baseX + fullW - ((baseX < maxScrollX - 1.0f) ? btnGutter : 0.0f);
        keel::f32 relX = 0.0f;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->IsOutOfFlow()) continue;
            const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
            if (child->active) {
                keel::f32 newTarget = baseX;
                if (relX < visLeft) {
                    newTarget = (relX + cw <= fullW - btnGutter || relX <= btnGutter + 8.0f)
                        ? 0.0f
                        : (relX - btnGutter);
                } else if (relX + cw > visRight) {
                    newTarget = (totalChildrenW - relX <= fullW - btnGutter)
                        ? maxScrollX
                        : ((relX + cw) - (fullW - btnGutter));
                }
                newTarget = keel::clamp(newTarget, 0.0f, maxScrollX);
                if (keel::abs(newTarget - scrollX) > 0.5f) {
                    cData->targetOffsetX = newTarget;
                    cData->animatingX = true;
                }
                break;
            }
            relX += cw + node->gap;
        }
    }

    // Clamp scroll offset
    scrollX = keel::clamp(scrollX, 0.0f, maxScrollX);
    if (node->scroll) {
        node->scroll->offset.x = scrollX;
    }

    // Dynamic gutters: only allocate gutter space when an arrow is active
    const bool canScrollLeft = (scrollX > 1.0f);
    const keel::f32 leftGutter = canScrollLeft ? btnGutter : 0.0f;
    const bool canScrollRight = (scrollX < maxScrollX - 1.0f);
    const keel::f32 rightGutter = canScrollRight ? btnGutter : 0.0f;

    const keel::f32 viewportX = startX + leftGutter;
    const keel::f32 viewportW = keel::max(0.0f, fullW - leftGutter - rightGutter);

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
    keel::f32 curX = startX - scrollX;
    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        if (child->IsOutOfFlow()) continue;
        keel::f32 childW = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
        keel::f32 childH = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight :
                       (child->heightMode == SizeMode::Stretch ? fullH : child->intrinsicSize.y);
        keel::f32 childY = startY;
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

void detail::Carousel::Arrange(Node* node, const Rect& availableRect) {
    Arrange(node, availableRect, UiTheme::Dark());
}

void detail::Carousel::RenderArrows(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node) return;
    RenderArrows(canvas, const_cast<Node*>(node), node->uiSize, theme);
}

void detail::Carousel::RenderArrows(
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

        const keel::f32 isz = keel::min(rect.w, cm.iconSize);
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

    const keel::f32 sepInsetY = keel::max(4.0f, keel::round(node->computedRect.h * 0.18f));
    const keel::f32 sepY1 = node->computedRect.y + sepInsetY;
    const keel::f32 sepY2 = node->computedRect.Bottom() - sepInsetY;
    const Color sepCol = pal.borderSubtle.WithAlphaF(0.65f);

    if (cData->showLeftArrow) {
        drawArrow(cData->leftArrowRect, cData->leftArrowHovered, cData->leftArrowPressed, true);
        if (sepY2 > sepY1) {
            const keel::f32 sepX = keel::floor(cData->viewportRect.x) + 0.5f;
            canvas.DrawLine(Vec2(sepX, sepY1), Vec2(sepX, sepY2), sepCol, 1.0f);
        }
    }
    if (cData->showRightArrow) {
        if (sepY2 > sepY1) {
            const keel::f32 sepX = keel::floor(cData->viewportRect.Right()) - 0.5f;
            canvas.DrawLine(Vec2(sepX, sepY1), Vec2(sepX, sepY2), sepCol, 1.0f);
        }
        drawArrow(cData->rightArrowRect, cData->rightArrowHovered, cData->rightArrowPressed, false);
    }
}

Rect detail::Carousel::GetContentClipRect(const Node* node) {
    if (!node) return Rect();
    const auto* cData = node->As<CarouselData>();
    return cData ? cData->viewportRect : node->computedRect;
}

void detail::Carousel::Update(Context& ctx, Node* node) {
    if (!node || !node->id.IsValid() || !node->scroll) return;
    ScrollData& sd = *node->scroll;
    auto& state = ctx.GetOrCreatePersistentState<ScrollState>(node->id);
    auto* cData = node->As<CarouselData>();
    const keel::f32 fullW = keel::max(0.0f, node->computedRect.w - node->padding.Horizontal());
    const keel::f32 maxScrollX = keel::max(0.0f, sd.contentSize.x - fullW);

    if (maxScrollX <= 0.5f) {
        state.offset.x = 0.0f;
        state.targetOffsetX = 0.0f;
        state.animatingX = false;
        if (cData) cData->animatingX = false;
        sd.offset.x = 0.0f;
        return;
    }

    if (cData && cData->animatingX) {
        state.targetOffsetX = keel::clamp(cData->targetOffsetX, 0.0f, maxScrollX);
        state.animatingX = true;
    } else if (!state.animatingX) {
        state.offset.x = keel::clamp(sd.offset.x, 0.0f, maxScrollX);
        state.targetOffsetX = state.offset.x;
    }

    if (state.animatingX) {
        state.targetOffsetX = keel::clamp(state.targetOffsetX, 0.0f, maxScrollX);
        const keel::f64 now = ctx.GetCurrentTimeSec();
        keel::f32 dt = keel::as<keel::f32>(now - state.lastAnimTimeSec);
        state.lastAnimTimeSec = now;
        if (dt <= 0.0001f || dt > 0.25f) {
            dt = 0.016f;
        }
        const keel::f32 diff = state.targetOffsetX - state.offset.x;
        if (keel::abs(diff) <= 0.75f) {
            state.offset.x = state.targetOffsetX;
            state.animatingX = false;
            if (cData) cData->animatingX = false;
            ctx.RequestRepaint(2);
        } else {
            const keel::f32 alpha = 1.0f - keel::exp(-20.0f * dt);
            const keel::f32 stepMove = diff * alpha;
            const keel::f32 minMove = (diff > 0.0f) ? keel::min(diff, 0.75f) : keel::max(diff, -0.75f);
            state.offset.x += (keel::abs(stepMove) < keel::abs(minMove)) ? minMove : stepMove;
            state.offset.x = keel::clamp(state.offset.x, 0.0f, maxScrollX);
            if (cData) cData->animatingX = true;
            ctx.RequestRepaint(2);
        }
        sd.offset.x = state.offset.x;
    } else {
        state.offset.x = sd.offset.x;
        state.targetOffsetX = state.offset.x;
    }
}

void detail::Carousel::ScrollToChild(Context& ctx, Node* carouselNode, const Node* targetNode) {
    if (!carouselNode || !targetNode || !carouselNode->id.IsValid()) return;
    auto& state = ctx.GetOrCreatePersistentState<ScrollState>(carouselNode->id);
    const keel::f32 targetLeft = targetNode->computedRect.x;
    const keel::f32 targetRight = targetNode->computedRect.Right();
    const auto* cData = carouselNode->As<CarouselData>();
    const keel::f32 viewLeft = cData ? cData->viewportRect.x : carouselNode->computedRect.x;
    const keel::f32 viewRight = cData ? cData->viewportRect.Right() : carouselNode->computedRect.Right();

    if (targetLeft < viewLeft) {
        state.offset.x -= (viewLeft - targetLeft);
    } else if (targetRight > viewRight) {
        state.offset.x += (targetRight - viewRight);
    }
    if (state.offset.x < 0.0f) state.offset.x = 0.0f;
    state.targetOffsetX = state.offset.x;
    state.animatingX = false;
    if (carouselNode->scroll) {
        carouselNode->scroll->offset = state.offset;
    }
}

void detail::Carousel::CollectBounds(
    Node* node,
    void (*recordBounds)(void*, Id, Rect),
    void* user,
    const Rect& parentClip,
    const Transform2D& space
) {
    if (!node || !recordBounds || (node->type != NodeType::Carousel && node->type != NodeType::Toolbar)) return;
    const auto* cData = node->As<CarouselData>();
    if (!cData) return;

    if (cData->showLeftArrow && cData->leftArrowRect.w > 0.0f) {
        const Rect clippedLeft = space.Apply(cData->leftArrowRect).Intersect(parentClip);
        if (clippedLeft.w > 0.0f && clippedLeft.h > 0.0f) {
            recordBounds(user, GetCarouselLeftArrowId(node->id), clippedLeft);
        }
    }
    if (cData->showRightArrow && cData->rightArrowRect.w > 0.0f) {
        const Rect clippedRight = space.Apply(cData->rightArrowRect).Intersect(parentClip);
        if (clippedRight.w > 0.0f && clippedRight.h > 0.0f) {
            recordBounds(user, GetCarouselRightArrowId(node->id), clippedRight);
        }
    }
}

} // namespace guikit
