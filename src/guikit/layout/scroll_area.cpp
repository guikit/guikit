#include "guikit/layout/scroll_area.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/scrollbar.hpp"
#include "guikit/style/style_painter.hpp"
#include "keel/math.h"

namespace guikit {

const PanelHooks* detail::ScrollArea::Hooks() noexcept {
    static constexpr PanelHooks kHooks{
        .contentClip = &detail::ScrollArea::GetContentClipRect,
        .update = &detail::ScrollArea::Update,
        .scrollToChild = &detail::ScrollArea::ScrollToChild
    };
    return &kHooks;
}

ContainerScope detail::ScrollArea::Open(
    Context& ctx,
    const ScrollAreaOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    Node* node = ctx.CreateNode(NodeType::ScrollArea, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const keel::f32 density = node->GetDensity();
    const bool activeDisabled = node->disabled;
    const UiSize activeSize = opts.size;

    node->direction = (opts.orientation == Orientation::Horizontal) ? FlexDirection::Row : FlexDirection::Column;
    node->align = opts.align;
    node->justify = opts.justify;
    node->clipChildren = true;
    node->widthMode = opts.width.ResolveMode(SizeMode::Stretch);
    node->fixedWidth = opts.width.IsFixed() ? opts.width.Dp() : 0.0f;
    node->heightMode = opts.height.ResolveMode(SizeMode::Stretch);
    node->fixedHeight = opts.height.IsFixed() ? opts.height.Dp() : 0.0f;
    node->flexGrow = Control::ResolveFlexGrow(node->parent, opts.width, opts.height, 1.0f);
    node->scroll = ctx.AllocPayload<ScrollData>();
    node->scroll->orientation = opts.orientation;
    node->scroll->policy = opts.scrollbar;
    node->scroll->solverManaged = true;
    if (!opts.height.IsFixed() && opts.maxHeight.has_value() && *opts.maxHeight > 0.0f) {
        node->scroll->maxHeight = *opts.maxHeight;
        if (node->heightMode == SizeMode::Stretch) {
            node->heightMode = SizeMode::Fit;
        }
    }
    const auto& pal = theme.palette;
    node->gap = (opts.gap.has_value() ? *opts.gap : 0.0f) * density;
    node->padding = theme.ResolvePadding(opts.padding, UiSize::None, density);
    node->backgroundColor = opts.background.IsBackgroundEnabled(false)
        ? theme.ResolveColor(opts.background, false, false, activeDisabled, pal.surface)
        : Color::Transparent();
    const keel::f32 bw = (opts.border.width > 0.0f) ? opts.border.width : (opts.border.color.HasAny() ? 1.0f : 0.0f);
    node->borderWidth = bw;
    node->borderColor = (bw > 0.0f) ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, pal.border) : Color::Transparent();
    node->cornerRadius = opts.border.radius.has_value() ? *opts.border.radius : 0.0f;
    node->uiSize = activeSize;
    node->visualHash = detail::HashArgs(opts.background, opts.border);
    node->postRenderFn = &detail::ScrollArea::RenderScrollbar;
    node->panelHooks = detail::ScrollArea::Hooks();

    // Retrieve persistent scroll state (mouse wheel is handled bottom-up in ScrollArea::Update)
    auto& state = ctx.GetOrCreatePersistentState<ScrollState>(id);
    node->scroll->offset = state.offset;

    ctx.FinishContainer(node);
    return ContainerScope(&ctx, node);
}

void detail::ScrollArea::RenderAxisScrollbar(
    ICanvas& canvas,
    Rect viewportRect,
    keel::f32 contentLength,
    keel::f32 scrollOffset,
    bool isHorizontal,
    bool hovered,
    bool active,
    UiSize size,
    const UiTheme& theme,
    keel::f32 cornerReserve,
    keel::u8 hoveredPart,
    keel::u8 pressedPart
) {
    const ScrollbarMetrics& m = theme.GetScrollbar(size);
    const ScrollbarGeometry geom = Scrollbar::ComputeAxis(
        viewportRect,
        contentLength,
        scrollOffset,
        isHorizontal,
        m,
        theme.scrollbarStyle,
        cornerReserve,
        true
    );
    if (!geom.visible) return;

    const ScrollbarPart hPart = static_cast<ScrollbarPart>(hoveredPart);
    const ScrollbarPart pPart = static_cast<ScrollbarPart>(pressedPart);
    Scrollbar::DrawAxis(canvas, geom, isHorizontal, hovered, active, hPart, pPart, m, theme);
}

void detail::ScrollArea::UpdateAxisScrollbar(
    Context& ctx,
    Id ownerId,
    Rect viewportRect,
    keel::f32 contentLength,
    keel::f32& ioScrollOffset,
    bool& ioDragging,
    keel::f32& ioDragStartOffset,
    Vec2& ioDragStartMouse,
    bool& outHovered,
    bool& outActive,
    bool isHorizontal,
    UiSize size,
    const UiTheme& theme,
    keel::f32 cornerReserve,
    keel::u8* outHoveredPart,
    keel::u8* ioPressedButton,
    keel::f64* ioNextRepeatSec
) {
    outHovered = false;
    outActive = false;
    if (outHoveredPart) *outHoveredPart = static_cast<keel::u8>(ScrollbarPart::None);

    const keel::f32 prevOffset = ioScrollOffset;
    const ScrollbarMetrics& m = theme.GetScrollbar(size);
    const keel::f32 viewLen = isHorizontal ? viewportRect.w : viewportRect.h;
    const keel::f32 maxScroll = keel::max(0.0f, contentLength - viewLen);
    ioScrollOffset = keel::clamp(ioScrollOffset, 0.0f, maxScroll);

    if (viewLen <= 10.0f || maxScroll <= 0.0f) {
        ioDragging = false;
        if (ioPressedButton) *ioPressedButton = static_cast<keel::u8>(ScrollbarPart::None);
        return;
    }

    const ScrollbarGeometry geom = Scrollbar::ComputeAxis(
        viewportRect,
        contentLength,
        ioScrollOffset,
        isHorizontal,
        m,
        theme.scrollbarStyle,
        cornerReserve
    );
    if (!geom.visible) {
        ioDragging = false;
        if (ioPressedButton) *ioPressedButton = static_cast<keel::u8>(ScrollbarPart::None);
        return;
    }

    const Vec2 mousePos = ctx.GetMousePos();
    const bool occluded = ctx.IsMouseOccluded(ownerId, mousePos);
    const bool mouseClicked = !occluded && ctx.IsMouseClicked();
    const bool mouseDown = ctx.IsMouseDown();

    const bool hoverStart = !occluded && geom.hasButtons && geom.startButtonRect.Contains(mousePos);
    const bool hoverEnd   = !occluded && geom.hasButtons && geom.endButtonRect.Contains(mousePos);
    const bool hoverThumb = !occluded && !hoverStart && !hoverEnd && geom.hitThumbRect.Contains(mousePos);
    const bool hoverTrack = !occluded && geom.hitTrackRect.Contains(mousePos);
    outHovered = hoverStart || hoverEnd || hoverThumb || hoverTrack;

    ScrollbarPart hovPart = ScrollbarPart::None;
    if (hoverStart)      hovPart = ScrollbarPart::StartButton;
    else if (hoverEnd)   hovPart = ScrollbarPart::EndButton;
    else if (hoverThumb) hovPart = ScrollbarPart::Thumb;
    else if (hoverTrack) hovPart = ScrollbarPart::Track;
    if (outHoveredPart) *outHoveredPart = static_cast<keel::u8>(hovPart);

    const keel::f64 now = ctx.GetCurrentTime();
    keel::u8 pressedBtn = ioPressedButton ? *ioPressedButton : static_cast<keel::u8>(ScrollbarPart::None);

    if (ioDragging) {
        outActive = true;
        if (mouseDown) {
            const keel::f32 delta = isHorizontal ? (mousePos.x - ioDragStartMouse.x) : (mousePos.y - ioDragStartMouse.y);
            if (geom.travel > 0.0f) {
                ioScrollOffset = keel::clamp(ioDragStartOffset + (delta / geom.travel) * maxScroll, 0.0f, maxScroll);
            }
        } else {
            ioDragging = false;
            if (ctx.GetActiveId() == ownerId) {
                ctx.SetActiveId(Id::Null());
            }
        }
    } else if (pressedBtn == static_cast<keel::u8>(ScrollbarPart::StartButton) ||
               pressedBtn == static_cast<keel::u8>(ScrollbarPart::EndButton)) {
        if (mouseDown) {
            outActive = true;
            const bool isStart = (pressedBtn == static_cast<keel::u8>(ScrollbarPart::StartButton));
            const bool stillOver = isStart ? hoverStart : hoverEnd;
            if (stillOver) {
                ctx.RequestRepaint(1);
                if (ioNextRepeatSec && now >= *ioNextRepeatSec) {
                    const keel::f32 step = isStart ? -m.scrollStep : m.scrollStep;
                    ioScrollOffset = keel::clamp(ioScrollOffset + step, 0.0f, maxScroll);
                    *ioNextRepeatSec = now + 0.05;
                    ctx.RequestRepaint(2);
                }
            }
        } else {
            pressedBtn = static_cast<keel::u8>(ScrollbarPart::None);
            if (ctx.GetActiveId() == ownerId) {
                ctx.SetActiveId(Id::Null());
            }
        }
    } else if (mouseClicked && outHovered) {
        if (hoverStart) {
            ioScrollOffset = keel::max(0.0f, ioScrollOffset - m.scrollStep);
            pressedBtn = static_cast<keel::u8>(ScrollbarPart::StartButton);
            outActive = true;
            if (ioNextRepeatSec) *ioNextRepeatSec = now + 0.30;
            ctx.SetActiveId(ownerId);
        } else if (hoverEnd) {
            ioScrollOffset = keel::min(maxScroll, ioScrollOffset + m.scrollStep);
            pressedBtn = static_cast<keel::u8>(ScrollbarPart::EndButton);
            outActive = true;
            if (ioNextRepeatSec) *ioNextRepeatSec = now + 0.30;
            ctx.SetActiveId(ownerId);
        } else if (hoverThumb) {
            ioDragging = true;
            outActive = true;
            ioDragStartMouse = mousePos;
            ioDragStartOffset = ioScrollOffset;
            ctx.SetActiveId(ownerId);
        } else if (hoverTrack) {
            const keel::f32 mouseCoord = isHorizontal ? mousePos.x : mousePos.y;
            const keel::f32 thumbCoord = isHorizontal ? geom.thumbRect.x : geom.thumbRect.y;
            if (mouseCoord < thumbCoord) {
                ioScrollOffset = keel::max(0.0f, ioScrollOffset - viewLen * 0.8f);
            } else {
                ioScrollOffset = keel::min(maxScroll, ioScrollOffset + viewLen * 0.8f);
            }
        }
    }

    if (ioPressedButton) *ioPressedButton = pressedBtn;

    if (ioDragging || pressedBtn != 0u || keel::abs(ioScrollOffset - prevOffset) > 0.01f) {
        ctx.RequestRepaint(2);
    }
}

void detail::ScrollArea::RenderScrollbar(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node || !node->scroll) return;
    const ScrollData& sd = *node->scroll;
    const bool allowV = (sd.orientation == Orientation::Vertical || sd.orientation == Orientation::Both);
    const bool allowH = (sd.orientation == Orientation::Horizontal || sd.orientation == Orientation::Both);

    const bool showV = allowV && (
        sd.policy == VisibilityPolicy::Always ||
        (sd.policy == VisibilityPolicy::Auto && sd.contentSize.y > node->computedRect.h + 0.5f)
    );
    const bool showH = allowH && (
        sd.policy == VisibilityPolicy::Always ||
        (sd.policy == VisibilityPolicy::Auto && sd.contentSize.x > node->computedRect.w + 0.5f)
    );

    const ScrollbarMetrics& m = theme.GetScrollbar(node->uiSize);
    const keel::f32 cornerReserve = (showV && showH) ? (m.trackWidth + m.margin) : 0.0f;

    if (showV) {
        RenderAxisScrollbar(
            canvas,
            node->computedRect,
            sd.contentSize.y,
            sd.offset.y,
            false,
            sd.hoveredY,
            sd.activeY,
            node->uiSize,
            theme,
            cornerReserve,
            sd.partHoveredY,
            sd.partPressedY
        );
    }
    if (showH) {
        RenderAxisScrollbar(
            canvas,
            node->computedRect,
            sd.contentSize.x,
            sd.offset.x,
            true,
            sd.hoveredX,
            sd.activeX,
            node->uiSize,
            theme,
            cornerReserve,
            sd.partHoveredX,
            sd.partPressedX
        );
    }
    if (showV && showH) {
        const Rect cornerRect(
            keel::floor(node->computedRect.Right() - m.trackWidth - m.margin),
            keel::floor(node->computedRect.Bottom() - m.trackWidth - m.margin),
            m.trackWidth,
            m.trackWidth
        );
        theme.Painter().DrawScrollbarCorner(canvas, theme, cornerRect);
    }
}

Rect detail::ScrollArea::GetContentClipRect(const Node* node, const UiTheme& theme) {
    if (!node) return Rect();
    const ScrollbarMetrics& sm = theme.GetScrollbar(node->uiSize);
    bool showV = false;
    bool showH = false;
    if (const ScrollData* sd = node->scroll) {
        const bool allowV = (sd->orientation == Orientation::Vertical || sd->orientation == Orientation::Both);
        const bool allowH = (sd->orientation == Orientation::Horizontal || sd->orientation == Orientation::Both);

        showV = allowV && (
            sd->policy == VisibilityPolicy::Always ||
            (sd->policy == VisibilityPolicy::Auto && sd->contentSize.y > node->computedRect.h + 0.5f)
        );
        showH = allowH && (
            sd->policy == VisibilityPolicy::Always ||
            (sd->policy == VisibilityPolicy::Auto && sd->contentSize.x > node->computedRect.w + 0.5f)
        );
    }

    const keel::f32 gutterX = showV ? sm.gutter : 0.0f;
    const keel::f32 gutterY = showH ? sm.gutter : 0.0f;
    return Rect(
        node->computedRect.x + node->padding.left,
        node->computedRect.y + node->padding.top,
        keel::max(0.0f, node->computedRect.w - node->padding.Horizontal() - gutterX),
        keel::max(0.0f, node->computedRect.h - node->padding.Vertical() - gutterY)
    );
}

void detail::ScrollArea::Update(Context& ctx, Node* node) {
    if (!node || !node->id.IsValid() || !node->scroll) return;
    ScrollData& sd = *node->scroll;

    auto& state = ctx.GetOrCreatePersistentState<ScrollState>(node->id);
    if (node->disabled) {
        state.draggingY = false;
        state.draggingX = false;
        state.pressedButtonY = 0u;
        state.pressedButtonX = 0u;
        sd.hoveredY = false;
        sd.activeY = false;
        sd.hoveredX = false;
        sd.activeX = false;
        sd.partHoveredY = 0u;
        sd.partPressedY = 0u;
        sd.partHoveredX = 0u;
        sd.partPressedX = 0u;
        return;
    }

    const UiTheme* resolvedTheme = &node->GetTheme(ctx.GetTheme());

    const bool allowV = (sd.orientation == Orientation::Vertical || sd.orientation == Orientation::Both);
    const bool allowH = (sd.orientation == Orientation::Horizontal || sd.orientation == Orientation::Both);
    const bool showV = allowV && (
        sd.policy == VisibilityPolicy::Always ||
        (sd.policy == VisibilityPolicy::Auto && sd.contentSize.y > node->computedRect.h + 0.5f)
    );
    const bool showH = allowH && (
        sd.policy == VisibilityPolicy::Always ||
        (sd.policy == VisibilityPolicy::Auto && sd.contentSize.x > node->computedRect.w + 0.5f)
    );
    const ScrollbarMetrics& m = resolvedTheme->GetScrollbar(node->uiSize);
    const keel::f32 cornerReserve = (showV && showH) ? (m.trackWidth + m.margin) : 0.0f;

    if (!node->disabled && node->computedRect.w > 0.0f && node->computedRect.h > 0.0f) {
        const Vec2 wheel = ctx.GetMouseWheel();
        const Vec2 mousePos = ctx.GetMousePos();
        if ((wheel.x != 0.0f || wheel.y != 0.0f) &&
            node->computedRect.Contains(mousePos) &&
            !ctx.IsMouseOccluded(node->id, mousePos) &&
            !ctx.GetKeyModifiers().ctrl) {
            const keel::f32 maxScrollY = allowV ? keel::max(0.0f, sd.contentSize.y - node->computedRect.h) : 0.0f;
            const keel::f32 maxScrollX = allowH ? keel::max(0.0f, sd.contentSize.x - node->computedRect.w) : 0.0f;
            const Vec2 prevOffset = state.offset;

            if (sd.orientation == Orientation::Horizontal ||
                (sd.orientation == Orientation::Both && ctx.GetKeyModifiers().shift && wheel.x == 0.0f)) {
                const keel::f32 delta = (wheel.x != 0.0f) ? wheel.x : wheel.y;
                if (allowH) {
                    state.offset.x = keel::clamp(state.offset.x - delta * m.scrollStep, 0.0f, maxScrollX);
                }
            } else {
                if (allowV) {
                    state.offset.y = keel::clamp(state.offset.y - wheel.y * m.scrollStep, 0.0f, maxScrollY);
                }
                if (allowH) {
                    state.offset.x = keel::clamp(state.offset.x - wheel.x * m.scrollStep, 0.0f, maxScrollX);
                }
            }

            if (keel::abs(state.offset.x - prevOffset.x) > 0.01f ||
                keel::abs(state.offset.y - prevOffset.y) > 0.01f) {
                ctx.ConsumeMouseWheel();
                ctx.RequestRepaint(2);
            }
        }
    }

    bool hovY = false, actY = false;
    keel::u8 hovPartY = 0u;
    if (showV) {
        UpdateAxisScrollbar(
            ctx,
            node->id,
            node->computedRect,
            sd.contentSize.y,
            state.offset.y,
            state.draggingY,
            state.dragStartOffset,
            state.dragStartMouse,
            hovY,
            actY,
            false,
            node->uiSize,
            *resolvedTheme,
            cornerReserve,
            &hovPartY,
            &state.pressedButtonY,
            &state.nextRepeatSecY
        );
    } else if (allowV) {
        const keel::f32 maxScrollY = keel::max(0.0f, sd.contentSize.y - node->computedRect.h);
        state.offset.y = keel::clamp(state.offset.y, 0.0f, maxScrollY);
        state.draggingY = false;
        state.pressedButtonY = 0u;
    } else {
        state.offset.y = 0.0f;
        state.draggingY = false;
        state.pressedButtonY = 0u;
    }
    sd.hoveredY = hovY;
    sd.activeY = actY;
    sd.partHoveredY = hovPartY;
    sd.partPressedY = state.draggingY
        ? static_cast<keel::u8>(ScrollbarPart::Thumb)
        : state.pressedButtonY;

    bool hovX = false, actX = false;
    keel::u8 hovPartX = 0u;
    if (showH) {
        UpdateAxisScrollbar(
            ctx,
            node->id,
            node->computedRect,
            sd.contentSize.x,
            state.offset.x,
            state.draggingX,
            state.dragStartOffsetX,
            state.dragStartMouse,
            hovX,
            actX,
            true,
            node->uiSize,
            *resolvedTheme,
            cornerReserve,
            &hovPartX,
            &state.pressedButtonX,
            &state.nextRepeatSecX
        );
    } else if (allowH) {
        const keel::f32 maxScrollX = keel::max(0.0f, sd.contentSize.x - node->computedRect.w);
        state.offset.x = keel::clamp(state.offset.x, 0.0f, maxScrollX);
        state.draggingX = false;
        state.pressedButtonX = 0u;
    } else {
        state.offset.x = 0.0f;
        state.draggingX = false;
        state.pressedButtonX = 0u;
    }
    sd.hoveredX = hovX;
    sd.activeX = actX;
    sd.partHoveredX = hovPartX;
    sd.partPressedX = state.draggingX
        ? static_cast<keel::u8>(ScrollbarPart::Thumb)
        : state.pressedButtonX;

    sd.offset = state.offset;
}

void detail::ScrollArea::ScrollToChild(Context& ctx, Node* scrollNode, const Node* targetNode) {
    if (!scrollNode || !targetNode || !scrollNode->id.IsValid() || !scrollNode->scroll) return;
    auto& state = ctx.GetOrCreatePersistentState<ScrollState>(scrollNode->id);
    const keel::f32 targetTop = targetNode->computedRect.y;
    const keel::f32 targetBottom = targetNode->computedRect.Bottom();
    const keel::f32 viewTop = scrollNode->computedRect.y + scrollNode->padding.top;
    const keel::f32 viewBottom = scrollNode->computedRect.Bottom() - scrollNode->padding.bottom;

    if (targetTop < viewTop) {
        state.offset.y -= (viewTop - targetTop);
    } else if (targetBottom > viewBottom) {
        state.offset.y += (targetBottom - viewBottom);
    }
    if (state.offset.y < 0.0f) state.offset.y = 0.0f;
    scrollNode->scroll->offset = state.offset;
}

} // namespace guikit
