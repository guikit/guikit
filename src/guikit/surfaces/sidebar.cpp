#include "guikit/surfaces/sidebar.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/graphics/icon.hpp"
#include "guikit/layout/scroll_area.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

Rect ComputeToggleRect(
    const Rect& bounds,
    SidebarSide side,
    SidebarMode /*mode*/,
    keel::u32 sameSideIndex,
    keel::f32 scale
) noexcept {
    const keel::f32 edgeX = (side == SidebarSide::Left) ? bounds.Right() : bounds.x;
    const keel::f32 toggleW = 13.0f * scale;
    const keel::f32 toggleH = 28.0f * scale;
    const keel::f32 baseCenterY = bounds.Center().y + keel::as<keel::f32>(sameSideIndex) * (toggleH + 6.0f * scale);
    const keel::f32 minY = bounds.y + 4.0f * scale;
    const keel::f32 maxY = keel::max(minY, bounds.Bottom() - toggleH - 4.0f * scale);
    const keel::f32 toggleY = keel::clamp(baseCenterY - toggleH * 0.5f, minY, maxY);

    const keel::f32 toggleX = (side == SidebarSide::Left)
        ? (edgeX - 1.0f * scale)
        : (edgeX - toggleW + 1.0f * scale);
    return Rect(toggleX, toggleY, toggleW, toggleH);
}

Rect ComputeSashHitRect(
    const Rect& bounds,
    SidebarSide side,
    SidebarMode mode,
    keel::f32 scale
) noexcept {
    const keel::f32 edgeX = (side == SidebarSide::Left) ? bounds.Right() : bounds.x;
    if (mode == SidebarMode::Collapsed) {
        const keel::f32 x = (side == SidebarSide::Left)
            ? (edgeX - 2.0f * scale)
            : (edgeX - 8.0f * scale);
        return Rect(x, bounds.y, 10.0f * scale, bounds.h);
    }
    if (side == SidebarSide::Left) {
        return Rect(edgeX - 1.5f * scale, bounds.y, 8.5f * scale, bounds.h);
    }
    return Rect(edgeX - 6.5f * scale, bounds.y, 10.0f * scale, bounds.h);
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

SidebarScope detail::Sidebar::Open(
    Context& ctx,
    SidebarMode* externalMode,
    const SidebarOptions& opts,
    keel::SourceLocation loc
) {
    keel::u32 sameSideIndex = 0;
    bool hasFlushSibling = false;
    const Node* parent = ctx.GetCurrentContainer();
    if (parent != nullptr) {
        const bool isRight = (opts.side == SidebarSide::Right);
        for (const Node* c = parent->firstChild; c != nullptr; c = c->nextSibling) {
            if (c->type == NodeType::Sidebar && c->IsDockedRight() == isRight) {
                if (c->IsFlushDock()) {
                    hasFlushSibling = true;
                } else {
                    ++sameSideIndex;
                }
            }
        }
    }

    const Id id = ctx.MakeChildId(loc);
    const Id sashId(detail::HashArgs(id.value, keel::u64(0x53425F53415348ull)));

    const UiSize activeSize = opts.size;
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const keel::f32 activeScale = 1.0f;

    const UiTheme& theme = ctx.GetTheme();
    const keel::f32 safeScale = 1.0f;
    const ComponentMetrics& m = theme.GetMetrics(activeSize);
    const keel::f32 metricMinW = m.defaultHeight * 3.8f;
    const Rect parentBounds = ctx.GetParentPrevFrameBounds();
    const keel::f32 clampedRatio = keel::clamp(opts.maxRatio, 0.15f, 0.90f);

    auto& state = ctx.GetOrCreatePersistentState<SidebarPersistState>(id);
    if (!state.initialized) {
        state.initialized = true;
        state.mode = opts.defaultMode;
        if (opts.width.has_value() && *opts.width > 0.0f) {
            state.lastExpandedWidth = *opts.width;
        } else if (opts.minWidth.has_value() && *opts.minWidth > 0.0f) {
            state.lastExpandedWidth = *opts.minWidth;
        } else {
            state.lastExpandedWidth = metricMinW;
        }
    }

    const keel::f32 baseW = opts.width.value_or(state.lastExpandedWidth > 0.0f ? state.lastExpandedWidth : 180.0f);
    const keel::f32 effMinW = (opts.minWidth.has_value() && *opts.minWidth > 0.0f)
        ? *opts.minWidth
        : (opts.resizable ? keel::min(baseW, metricMinW) : 0.0f);
    const keel::f32 parentMaxW = (parentBounds.w > 1.0f)
        ? keel::max(effMinW, (parentBounds.w / safeScale) * clampedRatio)
        : keel::max(effMinW, 560.0f);
    const keel::f32 effMaxW = (opts.maxWidth.has_value() && *opts.maxWidth > 0.0f)
        ? keel::max(effMinW, (parentBounds.w > 1.0f) ? keel::min(*opts.maxWidth, parentMaxW) : *opts.maxWidth)
        : parentMaxW;

    const Rect prevBounds = ctx.GetPrevUnclippedBounds(id);
    const bool hasPrevGeometry = (prevBounds.h > 1.0f);
    if (hasPrevGeometry && state.mode == SidebarMode::Expanded && prevBounds.w > 1.0f && !state.draggingSash && !state.togglePressed) {
        state.lastExpandedWidth = prevBounds.w / safeScale;
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
        auto hitScope = ctx.ScopedOverlayHitCheck(sashId, OverlayLayer::Panel);
        const Vec2 mouse = ctx.GetMousePos();
        const bool occluded = ctx.IsOverlayOccluded(sashId, mouse, OverlayLayer::Panel);
        const bool otherActive = ctx.GetActiveId().IsValid() && ctx.GetActiveId() != sashId;

        const Rect toggleRect = ComputeToggleRect(prevBounds, opts.side, state.mode, sameSideIndex, activeScale);
        const Rect sashRect = ComputeSashHitRect(prevBounds, opts.side, state.mode, activeScale);
        const bool canToggle = showToggle;
        const bool canSash = opts.resizable || (state.mode == SidebarMode::Collapsed && opts.allowCollapse);

        if (!occluded && !otherActive) {
            const bool onToggle = canToggle && toggleRect.Contains(mouse);
            const bool onSash = canSash && !onToggle && sashRect.Contains(mouse);
            state.toggleHovered = onToggle;
            state.sashHovered = onSash;

            if (onToggle && !state.draggingSash) {
                const SidebarMode nextM = NextSidebarMode(state.mode, opts.allowCompact, opts.allowCollapse);
                keel::StringRef tip = "Toggle Sidebar";
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
                const keel::f32 openDeltaX = (opts.side == SidebarSide::Left)
                    ? (mouse.x - state.dragStartMouseX)
                    : (state.dragStartMouseX - mouse.x);
                const keel::f32 dragDeltaX = (state.dragStartMode == SidebarMode::Collapsed)
                    ? openDeltaX
                    : keel::abs(mouse.x - state.dragStartMouseX);
                const keel::f32 dragThreshold = (state.dragStartMode == SidebarMode::Collapsed)
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
                     (sashRect.Contains(mouse) || keel::abs(mouse.x - state.dragStartMouseX) <= 12.0f * activeScale));
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
                const keel::f32 deltaX = (opts.side == SidebarSide::Left)
                    ? (mouse.x - state.dragStartMouseX)
                    : (state.dragStartMouseX - mouse.x);
                const keel::f32 rawLogicalW = state.dragStartWidth + (deltaX / safeScale);

                const keel::f32 compactW = (m.defaultHeight / safeScale) * 1.5f;
                const keel::f32 compactSnapW = (effMinW + compactW) * 0.5f;
                const keel::f32 collapseSnapW = (state.dragStartMode == SidebarMode::Collapsed)
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
                    const keel::f32 clampedW = keel::clamp(rawLogicalW, effMinW, effMaxW);
                    if (keel::abs(state.customWidth - clampedW) > 0.25f) {
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
        if (state.sashHovered || state.draggingSash) {
            ctx.SetCursor(CursorType::ResizeEW);
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

    Node* node = ctx.CreateNode(NodeType::Sidebar, id, opts.disabled);
    const keel::f32 density = node->GetDensity();
    node->direction = FlexDirection::Column;
    const bool isCollapsed = (state.mode == SidebarMode::Collapsed);
    const bool isCompact = (state.mode == SidebarMode::Compact);
    node->dockFlags =
        ((opts.side == SidebarSide::Right) ? dock_flags::kRight : dock_flags::kNone) |
        (isCompact ? dock_flags::kCompact : dock_flags::kNone) |
        (isCollapsed ? dock_flags::kCollapsed : dock_flags::kNone);
    const bool isScrollable = opts.scrollable && !isCollapsed;
    node->gap = theme.ResolveGap(opts.gap, activeSize, density);
    node->heightMode = SizeMode::Stretch;
    node->cornerRadius = opts.border.radius.has_value()
        ? *opts.border.radius
        : (hasFlushSibling ? 0.0f : theme.shell.panelRadius);
    node->clipChildren = opts.clipChildren;
    node->uiSize = activeSize;

    const auto& pal = theme.palette;
    const Color resolvedBg = opts.background.IsBackgroundEnabled(true)
        ? theme.ResolveColor(opts.background, false, false, activeDisabled, pal.surface)
        : Color::Transparent();
    const Color resolvedBorderCol = (opts.border.width > 0.0f)
        ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, pal.border)
        : Color::Transparent();
    const keel::f32 resolvedBorderW = (opts.border.width == 1.0f) ? theme.shell.panelBorderWidth : opts.border.width;
    keel::f32 reportedLogicalW = 0.0f;

    if (isCollapsed) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = 0.0f;
        node->minWidth = 0.0f;
        node->padding = Padding{};
        node->backgroundColor = Color::Transparent();
        node->borderColor = Color::Transparent();
        node->borderWidth = 0.0f;
        reportedLogicalW = 0.0f;
    } else if (isCompact) {
        node->widthMode = SizeMode::Fit;
        node->fixedWidth = 0.0f;
        node->minWidth = 0.0f;
        node->padding = opts.padding.has_value()
            ? theme.ResolvePadding(opts.padding, activeSize, density)
            : theme.GetPadding(UiSize::Sm).ScaledBy(density);
        node->backgroundColor = resolvedBg;
        node->borderColor = resolvedBorderCol;
        node->borderWidth = resolvedBorderW;
        reportedLogicalW = (prevBounds.w > 0.0f) ? prevBounds.w : 52.0f;
    } else {
        node->minWidth = effMinW;
        if (state.customWidth > 0.0f) {
            const keel::f32 effW = (parentBounds.w > 1.0f)
                ? keel::clamp(state.customWidth, effMinW, effMaxW)
                : state.customWidth;
            node->widthMode = SizeMode::Fixed;
            node->fixedWidth = effW;
            reportedLogicalW = effW;
        } else if (opts.width.has_value()) {
            const keel::f32 effW = (parentBounds.w > 1.0f)
                ? keel::clamp(*opts.width, effMinW, effMaxW)
                : *opts.width;
            node->widthMode = SizeMode::Fixed;
            node->fixedWidth = effW;
            reportedLogicalW = effW;
        } else {
            node->widthMode = SizeMode::Fit;
            reportedLogicalW = state.lastExpandedWidth;
        }
        node->padding = theme.ResolvePadding(opts.padding, activeSize, density);
        node->backgroundColor = resolvedBg;
        node->borderColor = resolvedBorderCol;
        node->borderWidth = resolvedBorderW;
    }

    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(state.mode),
        state.customWidth,
        state.sashHovered,
        state.draggingSash,
        state.toggleHovered,
        state.togglePressed,
        opts.background,
        opts.border
    );

    ctx.FinishContainer(node);

    if (isScrollable) {
        node->scroll = ctx.AllocPayload<ScrollData>();
        node->scroll->orientation = Orientation::Vertical;
        node->scroll->policy = opts.scrollbar;
        node->scroll->solverManaged = true;
        node->postRenderFn = &detail::ScrollArea::RenderScrollbar;
        node->panelHooks = detail::ScrollArea::Hooks();

        auto& scrollState = ctx.GetOrCreatePersistentState<ScrollState>(id);
        node->scroll->offset = scrollState.offset;
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
        const bool canSash = opts.resizable || (curMode == SidebarMode::Collapsed && allowColl);
        const UiTheme themeSnap = theme;

        ctx.RegisterOverlayHitBuilder([&ctx, node, sashId, side, curMode, drawToggle, canSash, sameSideIndex, activeScale]() {
            if (!node) return;
            const Rect r = node->computedRect;
            if (r.h <= 4.0f) return;
            if (drawToggle) {
                ctx.RegisterOverlayHitRegion(sashId, ComputeToggleRect(r, side, curMode, sameSideIndex, activeScale), OverlayLayer::Panel);
            }
            if (canSash) {
                ctx.RegisterOverlayHitRegion(sashId, ComputeSashHitRect(r, side, curMode, activeScale), OverlayLayer::Panel);
            }
        });

        ctx.RegisterOverlay([node, side, curMode, sashHov, sashDrag, togHov, togPress, drawToggle, allowComp, allowColl, sameSideIndex, activeScale, themeSnap](ICanvas& canvas) {
            if (!node) return;
            const Rect r = node->computedRect;
            if (r.h <= 4.0f) return;

            const auto& p = themeSnap.palette;
            const keel::f32 curEdgeX = (side == SidebarSide::Left) ? r.Right() : r.x;

            // 1. Draw interactive splitter (sash) highlight when hovered or actively dragged
            if (sashHov || sashDrag) {
                const keel::f32 cornerInset = keel::max(2.0f * activeScale, node->cornerRadius);
                const keel::f32 glowW = 6.0f * activeScale;
                const keel::f32 lineW = (sashDrag ? 2.5f : 2.0f) * activeScale;
                const keel::f32 spanH = keel::max(0.0f, r.h - cornerInset * 2.0f);
                const Rect glowRect(curEdgeX - glowW * 0.5f, r.y + cornerInset, glowW, spanH);
                const Rect lineRect(curEdgeX - lineW * 0.5f, r.y + cornerInset, lineW, spanH);
                canvas.DrawRect(glowRect, p.accent.WithAlphaF(sashDrag ? 0.25f : 0.14f));
                canvas.DrawRoundRect(lineRect, lineW * 0.5f, sashDrag ? p.accent : p.accent.WithAlphaF(0.85f));

                if (!drawToggle) {
                    const keel::f32 gripW = 4.0f * activeScale;
                    const keel::f32 gripH = 24.0f * activeScale;
                    const Rect gripRect(curEdgeX - gripW * 0.5f, r.Center().y - gripH * 0.5f, gripW, gripH);
                    canvas.DrawRoundRect(gripRect, gripW * 0.5f, p.accent);
                }
            }

            // 2. Draw edge mode-toggle pill button
            if (drawToggle) {
                const Rect btnRect = ComputeToggleRect(r, side, curMode, sameSideIndex, activeScale);
                const bool highlighted = togHov || togPress || sashHov || sashDrag || (curMode == SidebarMode::Collapsed);
                const keel::f32 alphaMul = highlighted ? 1.0f : 0.80f;

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

                const keel::f32 iconSz = 10.0f * activeScale;
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
        }, OverlayLayer::Panel);
    }

    return SidebarScope(&ctx, node, res, state.mode, reportedLogicalW);
}

} // namespace guikit
