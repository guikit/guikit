#include "guikit/panels/Sidebar.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/graphics/Icon.hpp"
#include "guikit/panels/ScrollArea.hpp"

namespace guikit {

namespace {

Rect ComputeToggleRect(
    const Rect& bounds,
    SidebarSide side,
    SidebarMode /*mode*/,
    uint32_t sameSideIndex,
    float scale
) noexcept {
    const float edgeX = (side == SidebarSide::Left) ? bounds.Right() : bounds.x;
    const float toggleW = 13.0f * scale;
    const float toggleH = 28.0f * scale;
    const float baseCenterY = bounds.Center().y + static_cast<float>(sameSideIndex) * (toggleH + 6.0f * scale);
    const float minY = bounds.y + 4.0f * scale;
    const float maxY = std::max(minY, bounds.Bottom() - toggleH - 4.0f * scale);
    const float toggleY = std::clamp(baseCenterY - toggleH * 0.5f, minY, maxY);

    const float toggleX = (side == SidebarSide::Left)
        ? (edgeX - 1.0f * scale)
        : (edgeX - toggleW + 1.0f * scale);
    return Rect(toggleX, toggleY, toggleW, toggleH);
}

Rect ComputeSashHitRect(
    const Rect& bounds,
    SidebarSide side,
    SidebarMode /*mode*/,
    float scale
) noexcept {
    const float edgeX = (side == SidebarSide::Left) ? bounds.Right() : bounds.x;
    const float x = (side == SidebarSide::Left)
        ? (edgeX - 1.5f * scale)
        : (edgeX - 6.5f * scale);
    return Rect(x, bounds.y, 8.0f * scale, bounds.h);
}

SidebarMode NextSidebarMode(
    SidebarMode cur,
    bool allowCompact,
    bool allowCollapse
) noexcept {
    if (cur == SidebarMode::Expanded) {
        if (allowCompact) return SidebarMode::Compact;
        if (allowCollapse) return SidebarMode::Collapsed;
        return SidebarMode::Expanded;
    }
    if (cur == SidebarMode::Compact) {
        if (allowCollapse) return SidebarMode::Collapsed;
        return SidebarMode::Expanded;
    }
    return SidebarMode::Expanded;
}

SidebarMode PrevSidebarMode(
    SidebarMode cur,
    bool allowCompact,
    bool allowCollapse
) noexcept {
    if (cur == SidebarMode::Expanded) {
        if (allowCollapse) return SidebarMode::Collapsed;
        if (allowCompact) return SidebarMode::Compact;
        return SidebarMode::Expanded;
    }
    if (cur == SidebarMode::Compact) {
        return SidebarMode::Expanded;
    }
    if (allowCompact) return SidebarMode::Compact;
    return SidebarMode::Expanded;
}

} // namespace

SidebarScope Sidebar::Open(
    Context& ctx,
    SidebarMode* externalMode,
    const SidebarOptions& opts,
    std::source_location loc
) {
    uint32_t sameSideIndex = 0;
    if (const Node* parent = ctx.GetCurrentContainer()) {
        const bool isRight = (opts.side == SidebarSide::Right);
        for (const Node* c = parent->firstChild; c != nullptr; c = c->nextSibling) {
            if (c->type == NodeType::Sidebar && c->sidebarRight == isRight && !c->sidebarFlush) {
                ++sameSideIndex;
            }
        }
    }

    const Id id = ctx.MakeChildId(loc);
    const Id sashId(detail::HashArgs(id.value, uint64_t(0x53425F53415348ull)));
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const UiTheme& theme = ctx.GetTheme();
    auto& state = ctx.GetOrCreateState<SidebarPersistState>(id);
    if (!state.initialized) {
        state.initialized = true;
        state.mode = opts.defaultMode;
        if (opts.width.has_value() && *opts.width > 0.0f) {
            state.lastExpandedWidth = *opts.width;
        } else if (opts.minWidth > 0.0f) {
            state.lastExpandedWidth = opts.minWidth;
        }
    }
    const Rect prevBounds = ctx.GetPrevUnclippedBounds(id);
    const bool hasPrevGeometry = (prevBounds.h > 1.0f);
    if (hasPrevGeometry && state.mode == SidebarMode::Expanded && prevBounds.w > 1.0f && !state.draggingSash && !state.togglePressed) {
        state.lastExpandedWidth = prevBounds.w / activeScale;
    }
    if (externalMode != nullptr) {
        state.mode = *externalMode;
    }

    const bool showToggle = opts.showToggleButton;
    const bool hasInteractiveEdge = !activeDisabled && hasPrevGeometry &&
        (opts.resizable || showToggle || opts.allowCompact || opts.allowCollapse || state.mode == SidebarMode::Collapsed);

    Response res{};
    res.id = id;
    res.rect = prevBounds;
    res.ctx = &ctx;

    bool modeChanged = false;
    bool widthChanged = false;
    const bool prevSashHovered = state.sashHovered;
    const bool prevToggleHovered = state.toggleHovered;
    const bool prevDraggingSash = state.draggingSash;
    const bool prevTogglePressed = state.togglePressed;
    state.sashHovered = false;
    state.toggleHovered = false;

    if (hasInteractiveEdge) {
        const Vec2 mouse = ctx.GetMousePos();
        const bool occludedByPopup = ctx.GetOpenPopupId().IsValid() && ctx.GetOpenPopupRect().Contains(mouse);
        const bool otherActive = ctx.GetActiveId().IsValid() && ctx.GetActiveId() != sashId;

        const Rect toggleRect = ComputeToggleRect(prevBounds, opts.side, state.mode, sameSideIndex, activeScale);
        const Rect sashRect = ComputeSashHitRect(prevBounds, opts.side, state.mode, activeScale);
        const bool canToggle = showToggle;
        const bool canSash = opts.resizable || (state.mode == SidebarMode::Collapsed && opts.allowCollapse);

        if (!occludedByPopup && !otherActive) {
            const bool onToggle = canToggle && toggleRect.Contains(mouse);
            const bool onSash = canSash && !onToggle && sashRect.Contains(mouse);
            state.toggleHovered = onToggle;
            state.sashHovered = onSash;

            if (onToggle && !state.draggingSash) {
                const SidebarMode nextM = NextSidebarMode(state.mode, opts.allowCompact, opts.allowCollapse);
                std::string_view tip = "Toggle Sidebar";
                if (nextM == SidebarMode::Compact) {
                    tip = "Compact Rail (Icons Only)";
                } else if (nextM == SidebarMode::Collapsed) {
                    tip = "Minimize Sidebar";
                } else if (nextM == SidebarMode::Expanded) {
                    tip = "Expand Sidebar";
                }
                ctx.SetTooltip(tip, toggleRect);
            } else if (onSash && state.mode == SidebarMode::Collapsed && !state.draggingSash) {
                ctx.SetTooltip("Drag or click edge to expand sidebar", sashRect);
            }

            if (onToggle && ctx.IsRightMouseClicked()) {
                const SidebarMode prevM = PrevSidebarMode(state.mode, opts.allowCompact, opts.allowCollapse);
                if (prevM != state.mode) {
                    state.mode = prevM;
                    modeChanged = true;
                }
                ctx.ConsumeMouseClick();
            } else if (onToggle && ctx.IsMouseClicked()) {
                state.togglePressed = true;
                state.dragStartMouseX = mouse.x;
                state.dragStartMode = state.mode;
                state.dragStartWidth = (state.mode == SidebarMode::Collapsed) ? 0.0f : (prevBounds.w / activeScale);
                state.dragStartCustomWidth = state.customWidth;
                ctx.SetActiveId(sashId);
                ctx.ConsumeMouseClick();
            } else if (onSash && ctx.IsMouseDoubleClicked()) {
                if (state.mode == SidebarMode::Expanded) {
                    if (state.customWidth > 0.0f) {
                        state.customWidth = 0.0f;
                        widthChanged = true;
                    } else if (opts.allowCompact) {
                        state.mode = SidebarMode::Compact;
                        modeChanged = true;
                    } else if (opts.allowCollapse) {
                        state.mode = SidebarMode::Collapsed;
                        modeChanged = true;
                    }
                } else {
                    state.mode = SidebarMode::Expanded;
                    modeChanged = true;
                }
                state.draggingSash = false;
                state.togglePressed = false;
                if (ctx.GetActiveId() == sashId) {
                    ctx.SetActiveId(Id::Null());
                }
                ctx.ConsumeMouseClick();
            } else if (onSash && ctx.IsMouseClicked()) {
                if (state.mode == SidebarMode::Collapsed) {
                    // When collapsed (0px), pressing the edge allows either a quick click to expand OR dragging out
                    state.togglePressed = true;
                } else {
                    state.draggingSash = true;
                }
                state.dragStartMouseX = mouse.x;
                state.dragStartMode = state.mode;
                state.dragStartWidth = (state.mode == SidebarMode::Collapsed) ? 0.0f : (prevBounds.w / activeScale);
                state.dragStartCustomWidth = state.customWidth;
                ctx.SetActiveId(sashId);
                ctx.ConsumeMouseClick();
            }
        }

        if (state.togglePressed) {
            if (ctx.IsMouseDown()) {
                res.pressed = true;
                const float openDeltaX = (opts.side == SidebarSide::Left)
                    ? (mouse.x - state.dragStartMouseX)
                    : (state.dragStartMouseX - mouse.x);
                const float dragDeltaX = (state.dragStartMode == SidebarMode::Collapsed)
                    ? openDeltaX
                    : std::abs(mouse.x - state.dragStartMouseX);
                const float dragThreshold = (state.dragStartMode == SidebarMode::Collapsed)
                    ? (12.0f * activeScale)
                    : (4.0f * activeScale);
                if (opts.resizable && dragDeltaX > dragThreshold) {
                    state.draggingSash = true;
                    state.togglePressed = false;
                }
            } else {
                state.togglePressed = false;
                if (ctx.GetActiveId() == sashId) {
                    ctx.SetActiveId(Id::Null());
                }
                const bool hitTarget = (canToggle && toggleRect.Contains(mouse)) ||
                    (state.dragStartMode == SidebarMode::Collapsed &&
                     (sashRect.Contains(mouse) || std::abs(mouse.x - state.dragStartMouseX) <= 12.0f * activeScale));
                if (hitTarget) {
                    const SidebarMode nextM = NextSidebarMode(state.mode, opts.allowCompact, opts.allowCollapse);
                    if (nextM != state.mode) {
                        state.mode = nextM;
                        modeChanged = true;
                    }
                    res.clicked = true;
                }
                ctx.ConsumeMouseClick();
            }
        }

        if (state.draggingSash) {
            if (ctx.IsMouseDown()) {
                res.pressed = true;
                const float deltaX = (opts.side == SidebarSide::Left)
                    ? (mouse.x - state.dragStartMouseX)
                    : (state.dragStartMouseX - mouse.x);
                const float rawLogicalW = state.dragStartWidth + (deltaX / activeScale);

                const float baseW = opts.width.value_or(state.lastExpandedWidth > 0.0f ? state.lastExpandedWidth : 180.0f);
                const float effMinW = (opts.minWidth > 0.0f) ? opts.minWidth : std::min(baseW, 136.0f);
                const float effMaxW = std::max(effMinW, opts.maxWidth);
                const float compactW = 54.0f;
                const float compactSnapW = (effMinW + compactW) * 0.5f;
                const float collapseSnapW = (state.dragStartMode == SidebarMode::Collapsed)
                    ? 12.0f
                    : (opts.allowCompact ? 24.0f : (effMinW * 0.45f));

                if (opts.allowCollapse && rawLogicalW < collapseSnapW) {
                    if (state.mode != SidebarMode::Collapsed) {
                        state.mode = SidebarMode::Collapsed;
                        if (state.dragStartMode == SidebarMode::Expanded) {
                            state.customWidth = state.dragStartCustomWidth;
                            state.lastExpandedWidth = state.dragStartWidth;
                        }
                        modeChanged = true;
                    }
                } else if (opts.allowCompact && rawLogicalW < compactSnapW) {
                    if (state.mode != SidebarMode::Compact) {
                        state.mode = SidebarMode::Compact;
                        if (state.dragStartMode == SidebarMode::Expanded) {
                            state.customWidth = state.dragStartCustomWidth;
                            state.lastExpandedWidth = state.dragStartWidth;
                        }
                        modeChanged = true;
                    }
                } else {
                    if (state.mode != SidebarMode::Expanded) {
                        state.mode = SidebarMode::Expanded;
                        modeChanged = true;
                    }
                    const float clampedW = std::clamp(rawLogicalW, effMinW, effMaxW);
                    if (std::abs(state.customWidth - clampedW) > 0.25f) {
                        state.customWidth = clampedW;
                        state.lastExpandedWidth = clampedW;
                        widthChanged = true;
                    }
                }
            } else {
                state.draggingSash = false;
                if (ctx.GetActiveId() == sashId) {
                    ctx.SetActiveId(Id::Null());
                }
                ctx.ConsumeMouseClick();
            }
        }
    } else {
        state.draggingSash = false;
        state.togglePressed = false;
        if (ctx.GetActiveId() == sashId) {
            ctx.SetActiveId(Id::Null());
        }
    }

    if (externalMode != nullptr) {
        *externalMode = state.mode;
    }

    if (modeChanged || widthChanged) {
        res.changed = true;
        ctx.InvalidateAll();
    } else if (state.sashHovered != prevSashHovered ||
               state.toggleHovered != prevToggleHovered ||
               state.draggingSash != prevDraggingSash ||
               state.togglePressed != prevTogglePressed ||
               state.draggingSash ||
               state.togglePressed) {
        ctx.InvalidateAll();
    }

    Node* node = ctx.CreateNode(NodeType::Sidebar, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Column;
    node->sidebarRight = (opts.side == SidebarSide::Right);
    node->sidebarCompact = (state.mode == SidebarMode::Compact);
    node->sidebarCollapsed = (state.mode == SidebarMode::Collapsed);
    node->sidebarScrollable = opts.scrollable && !node->sidebarCollapsed;
    node->gap = theme.ResolveGap(opts.gap, activeSize);
    node->heightMode = SizeMode::Stretch;
    node->cornerRadius = theme.ResolveRadius(opts.borderRadius, activeSize);
    node->clipChildren = opts.clipChildren;
    node->scrollOrientation = Orientation::Vertical;
    node->scrollbarPolicy = opts.scrollbar;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    const auto& pal = theme.palette;
    float reportedLogicalW = 0.0f;

    if (node->sidebarCollapsed) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = 0.0f;
        node->minWidth = 0.0f;
        node->padding = Padding{};
        node->backgroundColor = Color::Transparent();
        node->borderColor = Color::Transparent();
        node->borderWidth = 0.0f;
        reportedLogicalW = 0.0f;
    } else if (node->sidebarCompact) {
        node->widthMode = SizeMode::Fit;
        node->fixedWidth = 0.0f;
        node->minWidth = 0.0f;
        node->padding = opts.padding.has_value()
            ? theme.ResolvePadding(opts.padding, activeSize)
            : theme.GetPadding(UiSize::Sm);
        node->backgroundColor = (opts.backgroundColor.a > 0) ? opts.backgroundColor : pal.surface;
        node->borderColor = (opts.borderColor.a > 0) ? opts.borderColor : pal.border;
        node->borderWidth = theme.Scale(opts.borderWidth);
        reportedLogicalW = (prevBounds.w > 0.0f) ? (prevBounds.w / activeScale) : 52.0f;
    } else {
        const float effMinW = (opts.minWidth > 0.0f) ? opts.minWidth : (opts.resizable ? 136.0f : 0.0f);
        node->minWidth = theme.Scale(effMinW);
        if (state.customWidth > 0.0f) {
            node->widthMode = SizeMode::Fixed;
            node->fixedWidth = theme.Scale(state.customWidth);
            reportedLogicalW = state.customWidth;
        } else if (opts.width.has_value()) {
            node->widthMode = SizeMode::Fixed;
            node->fixedWidth = theme.Scale(*opts.width);
            reportedLogicalW = *opts.width;
        } else {
            node->widthMode = SizeMode::Fit;
            reportedLogicalW = state.lastExpandedWidth;
        }
        node->padding = theme.ResolvePadding(opts.padding, activeSize);
        node->backgroundColor = (opts.backgroundColor.a > 0) ? opts.backgroundColor : pal.surface;
        node->borderColor = (opts.borderColor.a > 0) ? opts.borderColor : pal.border;
        node->borderWidth = theme.Scale(opts.borderWidth);
    }

    node->visualHash = detail::HashArgs(
        static_cast<uint8_t>(state.mode),
        state.customWidth,
        state.sashHovered,
        state.draggingSash,
        state.toggleHovered,
        state.togglePressed
    );

    if (node->sidebarScrollable) {
        node->postRenderFn = &ScrollArea::RenderScrollbar;

        auto& scrollState = ctx.GetOrCreateState<ScrollState>(id);
        if (prevBounds.w > 0.0f && prevBounds.h > 0.0f && !node->disabled) {
            const Vec2 wheel = ctx.GetMouseWheel();
            if (prevBounds.Contains(ctx.GetMousePos()) && wheel.y != 0.0f) {
                const ScrollbarMetrics& sm = ctx.GetTheme().GetScrollbar(activeSize);
                scrollState.offset.y -= wheel.y * sm.scrollStep;
                if (scrollState.offset.y < 0.0f) scrollState.offset.y = 0.0f;
            }
        }
        node->scrollOffset = scrollState.offset;
    }

    const bool drawToggle = showToggle;
    if (!activeDisabled && (opts.resizable || drawToggle || (state.mode == SidebarMode::Collapsed && opts.allowCollapse))) {
        const SidebarSide side = opts.side;
        const SidebarMode curMode = state.mode;
        const bool sashHov = state.sashHovered;
        const bool sashDrag = state.draggingSash;
        const bool togHov = state.toggleHovered;
        const bool togPress = state.togglePressed;
        const bool allowComp = opts.allowCompact;
        const bool allowColl = opts.allowCollapse;
        const UiTheme themeSnap = theme;

        ctx.RegisterOverlay([node, side, curMode, sashHov, sashDrag, togHov, togPress, drawToggle, allowComp, allowColl, sameSideIndex, activeScale, themeSnap](ICanvas& canvas) {
            if (!node) return;
            const Rect r = node->computedRect;
            if (r.h <= 4.0f) return;

            const auto& p = themeSnap.palette;
            const float curEdgeX = (side == SidebarSide::Left) ? r.Right() : r.x;

            // 1. Draw interactive splitter (sash) highlight when hovered or actively dragged
            if (sashHov || sashDrag) {
                const float glowW = 6.0f * activeScale;
                const float lineW = (sashDrag ? 2.5f : 2.0f) * activeScale;
                const Rect glowRect(curEdgeX - glowW * 0.5f, r.y + 2.0f, glowW, std::max(0.0f, r.h - 4.0f));
                const Rect lineRect(curEdgeX - lineW * 0.5f, r.y + 2.0f, lineW, std::max(0.0f, r.h - 4.0f));
                canvas.DrawRect(glowRect, p.accent.WithAlphaF(sashDrag ? 0.25f : 0.14f));
                canvas.DrawRoundRect(lineRect, lineW * 0.5f, sashDrag ? p.accent : p.accent.WithAlphaF(0.85f));

                if (!drawToggle) {
                    const float gripW = 4.0f * activeScale;
                    const float gripH = 24.0f * activeScale;
                    const Rect gripRect(curEdgeX - gripW * 0.5f, r.Center().y - gripH * 0.5f, gripW, gripH);
                    canvas.DrawRoundRect(gripRect, gripW * 0.5f, p.accent);
                }
            }

            // 2. Draw edge mode-toggle pill button
            if (drawToggle) {
                const Rect btnRect = ComputeToggleRect(r, side, curMode, sameSideIndex, activeScale);
                const bool highlighted = togHov || togPress || sashHov || sashDrag || (curMode == SidebarMode::Collapsed);
                const float alphaMul = highlighted ? 1.0f : 0.80f;

                Color bg = togPress ? p.surfaceActive : (togHov ? p.surfaceHover : p.surfaceElevated);
                Color border = (togHov || togPress || sashDrag) ? p.accent : p.border;
                bg = bg.WithAlphaF(alphaMul);
                border = border.WithAlphaF(alphaMul);

                canvas.DrawRoundRect(btnRect, 5.0f * activeScale, bg, border, 1.0f * activeScale);

                const SidebarMode nextM = NextSidebarMode(curMode, allowComp, allowColl);
                const bool shrinksTowardWall = (nextM == SidebarMode::Compact || nextM == SidebarMode::Collapsed) &&
                    (curMode != SidebarMode::Collapsed);
                const bool pointLeft = (side == SidebarSide::Left) ? shrinksTowardWall : !shrinksTowardWall;
                const Icon chevron = pointLeft ? Icon::ChevronLeft() : Icon::ChevronRight();

                const float iconSz = 10.0f * activeScale;
                const Rect iconRect(
                    btnRect.Center().x - iconSz * 0.5f,
                    btnRect.Center().y - iconSz * 0.5f,
                    iconSz,
                    iconSz
                );
                Color iconCol = (togHov || togPress) ? p.accent : p.textSecondary;
                iconCol = iconCol.WithAlphaF(alphaMul);
                chevron.Draw(canvas, iconRect, iconCol);
            }
        });
    }

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);
    return SidebarScope(&ctx, node, res, state.mode, reportedLogicalW);
}

} // namespace guikit
