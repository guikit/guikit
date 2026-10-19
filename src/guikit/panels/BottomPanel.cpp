#include "guikit/panels/BottomPanel.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/graphics/Icon.hpp"
#include "guikit/panels/ScrollArea.hpp"

namespace guikit {

namespace {

Rect ComputeTopToggleRect(
    const Rect& bounds,
    float scale
) noexcept {
    const float edgeY = bounds.y;
    const float toggleW = 28.0f * scale;
    const float toggleH = 13.0f * scale;
    const float minX = bounds.x + 4.0f * scale;
    const float maxX = std::max(minX, bounds.Right() - toggleW - 4.0f * scale);
    const float toggleX = std::clamp(bounds.Center().x - toggleW * 0.5f, minX, maxX);
    const float toggleY = edgeY - toggleH + 1.0f * scale;
    return Rect(toggleX, toggleY, toggleW, toggleH);
}

Rect ComputeTopSashHitRect(
    const Rect& bounds,
    BottomPanelMode mode,
    float scale
) noexcept {
    const float edgeY = bounds.y;
    if (mode == BottomPanelMode::Collapsed) {
        return Rect(bounds.x, edgeY - 8.0f * scale, bounds.w, 10.0f * scale);
    }
    return Rect(bounds.x, edgeY - 4.0f * scale, bounds.w, 8.0f * scale);
}

} // namespace

BottomPanelScope BottomPanel::Open(
    Context& ctx,
    bool* open,
    const BottomPanelOptions& opts,
    std::source_location loc
) {
    if (open == nullptr) {
        return Open(ctx, static_cast<BottomPanelMode*>(nullptr), opts, loc);
    }
    BottomPanelMode mode = *open ? BottomPanelMode::Expanded : BottomPanelMode::Collapsed;
    BottomPanelScope scope = Open(ctx, &mode, opts, loc);
    *open = (mode != BottomPanelMode::Collapsed);
    return scope;
}

BottomPanelScope BottomPanel::Open(
    Context& ctx,
    BottomPanelMode* externalMode,
    const BottomPanelOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const Id sashId(detail::HashArgs(id.value, uint64_t(0x42505F53415348ull)));
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const UiTheme& theme = ctx.GetTheme();
    const float safeScale = (activeScale > 0.0f) ? activeScale : 1.0f;
    const ComponentMetrics& m = theme.GetMetrics(activeSize);
    const float metricMinH = (m.defaultHeight / safeScale) * 2.5f;
    const Rect parentBounds = ctx.GetParentPrevFrameBounds();
    const float clampedRatio = std::clamp(opts.maxRatio, 0.15f, 0.95f);

    auto& state = ctx.GetOrCreateState<BottomPanelPersistState>(id);
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

    const float baseH = opts.height.value_or(state.lastExpandedHeight > 0.0f ? state.lastExpandedHeight : 200.0f);
    const float effMinH = (opts.minHeight.has_value() && *opts.minHeight > 0.0f)
        ? *opts.minHeight
        : (opts.resizable ? std::min(baseH, metricMinH) : 0.0f);
    const float parentMaxH = (parentBounds.h > 1.0f)
        ? std::max(effMinH, (parentBounds.h / safeScale) * clampedRatio)
        : std::max(effMinH, 560.0f);
    const float effMaxH = (opts.maxHeight.has_value() && *opts.maxHeight > 0.0f)
        ? std::max(effMinH, (parentBounds.h > 1.0f) ? std::min(*opts.maxHeight, parentMaxH) : *opts.maxHeight)
        : parentMaxH;

    const Rect prevBounds = ctx.GetPrevUnclippedBounds(id);
    const bool hasPrevGeometry = (prevBounds.w > 1.0f);
    if (hasPrevGeometry && state.mode == BottomPanelMode::Expanded && prevBounds.h > 1.0f && !state.draggingSash && !state.togglePressed) {
        state.lastExpandedHeight = prevBounds.h / safeScale;
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
        const Vec2 mouse = ctx.GetMousePos();
        const bool occludedByPopup = ctx.GetOpenPopupId().IsValid() && ctx.GetOpenPopupRect().Contains(mouse);
        const bool otherActive = ctx.GetActiveId().IsValid() && ctx.GetActiveId() != sashId;

        const Rect toggleRect = ComputeTopToggleRect(prevBounds, activeScale);
        const Rect sashRect = ComputeTopSashHitRect(prevBounds, state.mode, activeScale);
        const bool canToggle = showToggle;
        const bool canSash = opts.resizable || (state.mode == BottomPanelMode::Collapsed && opts.allowCollapse);

        if (!occludedByPopup && !otherActive) {
            const bool onToggle = canToggle && toggleRect.Contains(mouse);
            const bool onSash = canSash && !onToggle && sashRect.Contains(mouse);
            state.toggleHovered = onToggle;
            state.sashHovered = onSash;

            if (onToggle && !state.draggingSash) {
                const std::string_view tip = (state.mode == BottomPanelMode::Collapsed)
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
                const float dragDeltaY = (state.dragStartMode == BottomPanelMode::Collapsed)
                    ? (state.dragStartMouseY - mouse.y)
                    : std::abs(mouse.y - state.dragStartMouseY);
                const float dragThreshold = (state.dragStartMode == BottomPanelMode::Collapsed)
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
                     (sashRect.Contains(mouse) || std::abs(mouse.y - state.dragStartMouseY) <= 12.0f * activeScale));
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
                const float deltaY = state.dragStartMouseY - mouse.y;
                const float rawLogicalH = state.dragStartHeight + (deltaY / safeScale);

                const float collapseSnapH = (state.dragStartMode == BottomPanelMode::Collapsed)
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
                    const float clampedH = std::clamp(rawLogicalH, effMinH, effMaxH);
                    if (std::abs(state.customHeight - clampedH) > 0.25f) {
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

    Node* node = ctx.CreateNode(NodeType::BottomPanel, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Column;
    node->sidebarCollapsed = (state.mode == BottomPanelMode::Collapsed);
    node->sidebarScrollable = opts.scrollable && !node->sidebarCollapsed;
    node->gap = theme.ResolveGap(opts.gap, activeSize);
    node->widthMode = SizeMode::Stretch;
    node->cornerRadius = theme.ResolveRadius(opts.border.radius, activeSize);
    node->clipChildren = opts.clipChildren;
    node->scrollOrientation = Orientation::Vertical;
    node->scrollbarPolicy = opts.scrollbar;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    const auto& pal = theme.palette;
    float reportedLogicalH = 0.0f;

    if (node->sidebarCollapsed) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = 0.0f;
        node->minHeight = 0.0f;
        node->padding = Padding{};
        node->backgroundColor = Color::Transparent();
        node->borderColor = Color::Transparent();
        node->borderWidth = 0.0f;
        reportedLogicalH = 0.0f;
    } else {
        node->minHeight = theme.Scale(effMinH);
        if (state.customHeight > 0.0f) {
            const float effH = (parentBounds.h > 1.0f)
                ? std::clamp(state.customHeight, effMinH, effMaxH)
                : state.customHeight;
            node->heightMode = SizeMode::Fixed;
            node->fixedHeight = theme.Scale(effH);
            reportedLogicalH = effH;
        } else if (opts.height.has_value()) {
            const float effH = (parentBounds.h > 1.0f)
                ? std::clamp(*opts.height, effMinH, effMaxH)
                : *opts.height;
            node->heightMode = SizeMode::Fixed;
            node->fixedHeight = theme.Scale(effH);
            reportedLogicalH = effH;
        } else {
            node->heightMode = SizeMode::Fit;
            reportedLogicalH = state.lastExpandedHeight;
        }
        node->padding = theme.ResolvePadding(opts.padding, activeSize);
        node->backgroundColor = opts.background.IsBackgroundEnabled(true)
            ? (opts.background.HasAny() && !opts.background.IsSurface()
                ? opts.background.Resolve(false, false, activeDisabled, pal.surface)
                : pal.surface)
            : Color::Transparent();
        node->borderWidth = theme.Scale(opts.border.width);
        node->borderColor = (opts.border.width > 0.0f)
            ? opts.border.color.Resolve(false, false, activeDisabled, pal.border)
            : Color::Transparent();
    }

    node->visualHash = detail::HashArgs(
        static_cast<uint8_t>(state.mode),
        state.customHeight,
        state.sashHovered,
        state.draggingSash,
        state.toggleHovered,
        state.togglePressed,
        opts.background,
        opts.border
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
    if (!activeDisabled && (opts.resizable || drawToggle || (state.mode == BottomPanelMode::Collapsed && opts.allowCollapse))) {
        const BottomPanelMode curMode = state.mode;
        const bool sashHov = state.sashHovered;
        const bool sashDrag = state.draggingSash;
        const bool togHov = state.toggleHovered;
        const bool togPress = state.togglePressed;
        const UiTheme themeSnap = theme;

        ctx.RegisterOverlay([node, curMode, sashHov, sashDrag, togHov, togPress, drawToggle, activeScale, themeSnap](ICanvas& canvas) {
            if (!node) return;
            const Rect r = node->computedRect;
            if (r.w <= 4.0f) return;

            const auto& p = themeSnap.palette;
            const float curEdgeY = r.y;

            // 1. Draw interactive top splitter (sash) highlight when hovered or actively dragged
            if (sashHov || sashDrag) {
                const float cornerInset = std::max(2.0f * activeScale, node->cornerRadius);
                const float glowH = 6.0f * activeScale;
                const float lineH = (sashDrag ? 2.5f : 2.0f) * activeScale;
                const float spanW = std::max(0.0f, r.w - cornerInset * 2.0f);
                const Rect glowRect(r.x + cornerInset, curEdgeY - glowH * 0.5f, spanW, glowH);
                const Rect lineRect(r.x + cornerInset, curEdgeY - lineH * 0.5f, spanW, lineH);
                canvas.DrawRect(glowRect, p.accent.WithAlphaF(sashDrag ? 0.25f : 0.14f));
                canvas.DrawRoundRect(lineRect, lineH * 0.5f, sashDrag ? p.accent : p.accent.WithAlphaF(0.85f));

                if (!drawToggle) {
                    const float gripW = 24.0f * activeScale;
                    const float gripH = 4.0f * activeScale;
                    const Rect gripRect(r.Center().x - gripW * 0.5f, curEdgeY - gripH * 0.5f, gripW, gripH);
                    canvas.DrawRoundRect(gripRect, gripH * 0.5f, p.accent);
                }
            } else if (curMode == BottomPanelMode::Collapsed && !drawToggle) {
                const float gripW = 28.0f * activeScale;
                const float gripH = 3.0f * activeScale;
                const Rect gripRect(r.Center().x - gripW * 0.5f, curEdgeY - gripH, gripW, gripH);
                canvas.DrawRoundRect(gripRect, gripH * 0.5f, p.border.WithAlphaF(0.85f));
            }

            // 2. Draw top edge mode-toggle pill button
            if (drawToggle) {
                const Rect btnRect = ComputeTopToggleRect(r, activeScale);
                const bool highlighted = togHov || togPress || sashHov || sashDrag || (curMode == BottomPanelMode::Collapsed);
                const float alphaMul = highlighted ? 1.0f : 0.80f;

                Color bg = togPress ? p.surfaceActive : (togHov ? p.surfaceHover : p.surfaceElevated);
                Color border = (togHov || togPress || sashDrag) ? p.accent : p.border;
                bg = bg.WithAlphaF(alphaMul);
                border = border.WithAlphaF(alphaMul);

                canvas.DrawRoundRect(btnRect, 5.0f * activeScale, bg, border, 1.0f * activeScale);

                const Icon chevron = (curMode == BottomPanelMode::Collapsed)
                    ? Icon::ChevronUp()
                    : Icon::ChevronDown();

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
    return BottomPanelScope(&ctx, node, res, state.mode, reportedLogicalH);
}

} // namespace guikit
