#include "guikit/surfaces/bottom_panel.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/graphics/icon.hpp"
#include "guikit/layout/scroll_area.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

Rect ComputeTopToggleRect(
    const Rect& bounds,
    keel::f32 scale
) noexcept {
    const keel::f32 edgeY = bounds.y;
    const keel::f32 toggleW = 28.0f * scale;
    const keel::f32 toggleH = 13.0f * scale;
    const keel::f32 minX = bounds.x + 4.0f * scale;
    const keel::f32 maxX = keel::max(minX, bounds.Right() - toggleW - 4.0f * scale);
    const keel::f32 toggleX = keel::clamp(bounds.Center().x - toggleW * 0.5f, minX, maxX);
    const keel::f32 toggleY = edgeY - toggleH + 1.0f * scale;
    return Rect(toggleX, toggleY, toggleW, toggleH);
}

Rect ComputeTopSashHitRect(
    const Rect& bounds,
    BottomPanelMode mode,
    keel::f32 scale
) noexcept {
    const keel::f32 edgeY = bounds.y;
    if (mode == BottomPanelMode::Collapsed) {
        return Rect(bounds.x, edgeY - 8.0f * scale, bounds.w, 10.0f * scale);
    }
    return Rect(bounds.x, edgeY - 6.0f * scale, bounds.w, 10.0f * scale);
}

} // namespace

BottomPanelScope detail::BottomPanel::Open(
    Context& ctx,
    bool* open,
    const BottomPanelOptions& opts,
    keel::SourceLocation loc
) {
    if (open == nullptr) {
        return Open(ctx, static_cast<BottomPanelMode*>(nullptr), opts, loc);
    }
    BottomPanelMode mode = *open ? BottomPanelMode::Expanded : BottomPanelMode::Collapsed;
    BottomPanelScope scope = Open(ctx, &mode, opts, loc);
    *open = (mode != BottomPanelMode::Collapsed);
    return scope;
}

BottomPanelScope detail::BottomPanel::Open(
    Context& ctx,
    BottomPanelMode* externalMode,
    const BottomPanelOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const Id sashId(detail::HashArgs(id.value, keel::u64(0x42505F53415348ull)));

    Node* node = ctx.CreateNode(NodeType::BottomPanel, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const UiSize activeSize = opts.size;
    const bool activeDisabled = node->disabled;
    const keel::f32 activeScale = 1.0f;
    const ComponentMetrics& m = theme.GetMetrics(activeSize);
    const keel::f32 metricMinH = m.defaultHeight * 2.5f;
    const Rect parentBounds = ctx.GetParentPrevFrameBounds();
    const keel::f32 clampedRatio = keel::clamp(opts.maxRatio, 0.15f, 0.95f);

    auto& state = ctx.GetOrCreatePersistentState<BottomPanelPersistState>(id);
    if (!state.initialized) {
        state.initialized = true;
        state.mode = opts.defaultMode;
        if (opts.height.has_value() && *opts.height > 0.0f) {
            state.lastExpandedHeight = *opts.height;
        } else if (opts.minHeight.has_value() && *opts.minHeight > 0.0f) {
            state.lastExpandedHeight = *opts.minHeight;
        } else {
            state.lastExpandedHeight = metricMinH;
        }
    }

    const keel::f32 baseH = opts.height.value_or(state.lastExpandedHeight > 0.0f ? state.lastExpandedHeight : 200.0f);
    const keel::f32 effMinH = (opts.minHeight.has_value() && *opts.minHeight > 0.0f)
        ? *opts.minHeight
        : (opts.resizable ? keel::min(baseH, metricMinH) : 0.0f);
    const keel::f32 parentMaxH = (parentBounds.h > 1.0f)
        ? keel::max(effMinH, parentBounds.h * clampedRatio)
        : keel::max(effMinH, 560.0f);
    const keel::f32 effMaxH = (opts.maxHeight.has_value() && *opts.maxHeight > 0.0f)
        ? keel::max(effMinH, (parentBounds.h > 1.0f) ? keel::min(*opts.maxHeight, parentMaxH) : *opts.maxHeight)
        : parentMaxH;

    const Rect prevBounds = ctx.GetPrevUnclippedBounds(id);
    const bool hasPrevGeometry = (prevBounds.w > 1.0f);
    if (hasPrevGeometry && state.mode == BottomPanelMode::Expanded && prevBounds.h > 1.0f && !state.draggingSash && !state.togglePressed) {
        state.lastExpandedHeight = prevBounds.h;
    }
    if (externalMode != nullptr) {
        state.mode = *externalMode;
    }

    const bool showToggle = opts.showToggleButton;
    const bool hasInteractiveEdge = !activeDisabled && hasPrevGeometry &&
        (opts.resizable || showToggle || opts.allowCollapse || state.mode == BottomPanelMode::Collapsed);

    Response res{};
    res.id = id;
    res.rect = prevBounds;
    res.ctx = &ctx;

    bool modeChanged = false;
    bool heightChanged = false;
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

        const Rect toggleRect = ComputeTopToggleRect(prevBounds, activeScale);
        const Rect sashRect = ComputeTopSashHitRect(prevBounds, state.mode, activeScale);
        const bool canToggle = showToggle;
        const bool canSash = opts.resizable || (state.mode == BottomPanelMode::Collapsed && opts.allowCollapse);

        if (!occluded && !otherActive) {
            const bool onToggle = canToggle && toggleRect.Contains(mouse);
            const bool onSash = canSash && !onToggle && sashRect.Contains(mouse);
            state.toggleHovered = onToggle;
            state.sashHovered = onSash;

            if (onToggle && !state.draggingSash) {
                const keel::StringRef tip = (state.mode == BottomPanelMode::Collapsed)
                    ? "Expand Bottom Panel"
                    : "Minimize Bottom Panel";
                ctx.SetTooltip(tip, toggleRect);
            } else if (onSash && state.mode == BottomPanelMode::Collapsed && !state.draggingSash) {
                ctx.SetTooltip("Drag or click top edge to expand panel", sashRect);
            }

            if (onToggle && ctx.IsMouseClicked()) {
                state.togglePressed = true;
                state.dragStartMouseY = mouse.y;
                state.dragStartMode = state.mode;
                state.dragStartHeight = (state.mode == BottomPanelMode::Collapsed) ? 0.0f : (prevBounds.h / activeScale);
                state.dragStartCustomHeight = state.customHeight;
                ctx.SetActiveId(sashId);
                ctx.ConsumeMouseClick();
            } else if (onSash && ctx.IsMouseDoubleClicked()) {
                if (state.mode == BottomPanelMode::Expanded) {
                    if (state.customHeight > 0.0f) {
                        state.customHeight = 0.0f;
                        heightChanged = true;
                    } else if (opts.allowCollapse) {
                        state.mode = BottomPanelMode::Collapsed;
                        modeChanged = true;
                    }
                } else {
                    state.mode = BottomPanelMode::Expanded;
                    modeChanged = true;
                }
                state.draggingSash = false;
                state.togglePressed = false;
                if (ctx.GetActiveId() == sashId) {
                    ctx.SetActiveId(Id::Null());
                }
                ctx.ConsumeMouseClick();
            } else if (onSash && ctx.IsMouseClicked()) {
                if (state.mode == BottomPanelMode::Collapsed) {
                    state.togglePressed = true;
                } else {
                    state.draggingSash = true;
                }
                state.dragStartMouseY = mouse.y;
                state.dragStartMode = state.mode;
                state.dragStartHeight = (state.mode == BottomPanelMode::Collapsed) ? 0.0f : (prevBounds.h / activeScale);
                state.dragStartCustomHeight = state.customHeight;
                ctx.SetActiveId(sashId);
                ctx.ConsumeMouseClick();
            }
        }

        if (state.togglePressed) {
            if (ctx.IsMouseDown()) {
                res.pressed = true;
                const keel::f32 dragDeltaY = (state.dragStartMode == BottomPanelMode::Collapsed)
                    ? (state.dragStartMouseY - mouse.y)
                    : keel::abs(mouse.y - state.dragStartMouseY);
                const keel::f32 dragThreshold = (state.dragStartMode == BottomPanelMode::Collapsed)
                    ? (12.0f * activeScale)
                    : (4.0f * activeScale);
                if (opts.resizable && dragDeltaY > dragThreshold) {
                    state.draggingSash = true;
                    state.togglePressed = false;
                }
            } else {
                state.togglePressed = false;
                if (ctx.GetActiveId() == sashId) {
                    ctx.SetActiveId(Id::Null());
                }
                const bool hitTarget = (canToggle && toggleRect.Contains(mouse)) ||
                    (state.dragStartMode == BottomPanelMode::Collapsed &&
                     (sashRect.Contains(mouse) || keel::abs(mouse.y - state.dragStartMouseY) <= 12.0f * activeScale));
                if (hitTarget) {
                    const BottomPanelMode nextM = (state.mode == BottomPanelMode::Expanded && opts.allowCollapse)
                        ? BottomPanelMode::Collapsed
                        : BottomPanelMode::Expanded;
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
                // Moving mouse UP (smaller mouse.y) increases the BottomPanel height
                const keel::f32 deltaY = state.dragStartMouseY - mouse.y;
                const keel::f32 rawLogicalH = state.dragStartHeight + deltaY;

                const keel::f32 collapseSnapH = (state.dragStartMode == BottomPanelMode::Collapsed)
                    ? 12.0f
                    : (effMinH * 0.45f);

                if (opts.allowCollapse && rawLogicalH < collapseSnapH) {
                    if (state.mode != BottomPanelMode::Collapsed) {
                        state.mode = BottomPanelMode::Collapsed;
                        if (state.dragStartMode == BottomPanelMode::Expanded) {
                            state.customHeight = state.dragStartCustomHeight;
                            state.lastExpandedHeight = state.dragStartHeight;
                        }
                        modeChanged = true;
                    }
                } else {
                    if (state.mode != BottomPanelMode::Expanded) {
                        state.mode = BottomPanelMode::Expanded;
                        modeChanged = true;
                    }
                    const keel::f32 clampedH = keel::clamp(rawLogicalH, effMinH, effMaxH);
                    if (keel::abs(state.customHeight - clampedH) > 0.25f) {
                        state.customHeight = clampedH;
                        state.lastExpandedHeight = clampedH;
                        heightChanged = true;
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
            ctx.SetCursor(CursorType::ResizeNS);
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

    if (modeChanged || heightChanged) {
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

    const keel::f32 density = node->GetDensity();
    node->direction = FlexDirection::Column;
    const bool isCollapsed = (state.mode == BottomPanelMode::Collapsed);
    node->dockFlags = isCollapsed ? dock_flags::kCollapsed : dock_flags::kNone;
    const bool isScrollable = opts.scrollable && !isCollapsed;
    node->gap = theme.ResolveGap(opts.gap, activeSize, density);
    node->widthMode = SizeMode::Stretch;
    node->SetSurfaceRole(SurfaceRole::DockEdge);
    node->SetDockSeam(DockSeam::Top);
    node->cornerRadius = opts.border.radius.has_value()
        ? *opts.border.radius
        : theme.shell.panelRadius;
    node->clipChildren = opts.clipChildren;
    node->uiSize = activeSize;

    const auto& pal = theme.palette;
    keel::f32 reportedLogicalH = 0.0f;

    if (isCollapsed) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = 0.0f;
        node->minHeight = 0.0f;
        node->padding = Padding{};
        node->backgroundColor = Color::Transparent();
        node->borderColor = Color::Transparent();
        node->borderWidth = 0.0f;
        reportedLogicalH = 0.0f;
    } else {
        node->minHeight = effMinH;
        if (state.customHeight > 0.0f) {
            const keel::f32 effH = (parentBounds.h > 1.0f)
                ? keel::clamp(state.customHeight, effMinH, effMaxH)
                : state.customHeight;
            node->heightMode = SizeMode::Fixed;
            node->fixedHeight = effH;
            reportedLogicalH = effH;
        } else if (opts.height.has_value()) {
            const keel::f32 effH = (parentBounds.h > 1.0f)
                ? keel::clamp(*opts.height, effMinH, effMaxH)
                : *opts.height;
            node->heightMode = SizeMode::Fixed;
            node->fixedHeight = effH;
            reportedLogicalH = effH;
        } else {
            node->heightMode = SizeMode::Fit;
            reportedLogicalH = state.lastExpandedHeight;
        }
        node->padding = theme.ResolvePadding(opts.padding, activeSize, density);
        node->backgroundColor = opts.background.IsBackgroundEnabled(true)
            ? theme.ResolveColor(opts.background, false, false, activeDisabled, pal.surface)
            : Color::Transparent();
        node->borderWidth = (opts.border.width == 1.0f) ? theme.shell.panelBorderWidth : opts.border.width;
        node->borderColor = (opts.border.width > 0.0f)
            ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, pal.border)
            : Color::Transparent();
    }

    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(state.mode),
        state.customHeight,
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
    if (!activeDisabled && (opts.resizable || drawToggle || (state.mode == BottomPanelMode::Collapsed && opts.allowCollapse))) {
        const BottomPanelMode curMode = state.mode;
        const bool sashHov = state.sashHovered;
        const bool sashDrag = state.draggingSash;
        const bool togHov = state.toggleHovered;
        const bool togPress = state.togglePressed;
        const bool canSash = opts.resizable || (curMode == BottomPanelMode::Collapsed && opts.allowCollapse);
        const UiTheme themeSnap = theme;

        ctx.RegisterOverlayHitBuilder([&ctx, node, sashId, curMode, drawToggle, canSash, activeScale]() {
            if (!node) return;
            const Rect r = node->computedRect;
            if (r.w <= 4.0f) return;
            if (drawToggle) {
                ctx.RegisterOverlayHitRegion(sashId, ComputeTopToggleRect(r, activeScale), OverlayLayer::Panel);
            }
            if (canSash) {
                ctx.RegisterOverlayHitRegion(sashId, ComputeTopSashHitRect(r, curMode, activeScale), OverlayLayer::Panel);
            }
        });

        ctx.RegisterOverlay([node, curMode, sashHov, sashDrag, togHov, togPress, drawToggle, activeScale, themeSnap](ICanvas& canvas) {
            if (!node) return;
            const Rect r = node->computedRect;
            if (r.w <= 4.0f) return;

            const auto& p = themeSnap.palette;
            const keel::f32 curEdgeY = r.y;

            // 1. Draw interactive top splitter (sash) highlight when hovered or actively dragged
            if (sashHov || sashDrag) {
                const keel::f32 cornerInset = keel::max(2.0f * activeScale, node->cornerRadius);
                const keel::f32 glowH = 6.0f * activeScale;
                const keel::f32 lineH = (sashDrag ? 2.5f : 2.0f) * activeScale;
                const keel::f32 spanW = keel::max(0.0f, r.w - cornerInset * 2.0f);
                const Rect glowRect(r.x + cornerInset, curEdgeY - glowH * 0.5f, spanW, glowH);
                const Rect lineRect(r.x + cornerInset, curEdgeY - lineH * 0.5f, spanW, lineH);
                canvas.DrawRect(glowRect, p.accent.WithAlphaF(sashDrag ? 0.25f : 0.14f));
                canvas.DrawRoundRect(lineRect, lineH * 0.5f, sashDrag ? p.accent : p.accent.WithAlphaF(0.85f));

                if (!drawToggle) {
                    const keel::f32 gripW = 24.0f * activeScale;
                    const keel::f32 gripH = 4.0f * activeScale;
                    const Rect gripRect(r.Center().x - gripW * 0.5f, curEdgeY - gripH * 0.5f, gripW, gripH);
                    canvas.DrawRoundRect(gripRect, gripH * 0.5f, p.accent);
                }
            } else if (curMode == BottomPanelMode::Collapsed && !drawToggle) {
                const keel::f32 gripW = 28.0f * activeScale;
                const keel::f32 gripH = 3.0f * activeScale;
                const Rect gripRect(r.Center().x - gripW * 0.5f, curEdgeY - gripH, gripW, gripH);
                canvas.DrawRoundRect(gripRect, gripH * 0.5f, p.border.WithAlphaF(0.85f));
            }

            // 2. Draw top edge mode-toggle pill button
            if (drawToggle) {
                const Rect btnRect = ComputeTopToggleRect(r, activeScale);
                const bool highlighted = togHov || togPress || sashHov || sashDrag || (curMode == BottomPanelMode::Collapsed);
                const keel::f32 alphaMul = highlighted ? 1.0f : 0.80f;

                Color bg = togPress ? p.surfaceActive : (togHov ? p.surfaceHover : p.surfaceElevated);
                Color border = (togHov || togPress || sashDrag) ? p.accent : p.border;
                bg = bg.WithAlphaF(alphaMul);
                border = border.WithAlphaF(alphaMul);

                canvas.DrawRoundRect(btnRect, 5.0f * activeScale, bg, border, 1.0f * activeScale);

                const Icon chevron = (curMode == BottomPanelMode::Collapsed)
                    ? Icon::ChevronUp()
                    : Icon::ChevronDown();

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

    return BottomPanelScope(&ctx, node, res, state.mode, reportedLogicalH);
}

} // namespace guikit
