#include "guikit/panels/popover.hpp"
#include <cmath>
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

struct PopoverPayload {
    PopoverOptions opts{};
    Rect viewportRect{};
};

struct PopoverPersistState {
    bool wasOpen{false};
};

} // namespace

PopoverScope::~PopoverScope() noexcept {
    EndPopoverIfNeeded();
}

void PopoverScope::Close() noexcept {
    if (m_openPtr) {
        *m_openPtr = false;
    }
    if (m_ctx) {
        m_ctx->InvalidateAll();
    }
}

void PopoverScope::EndPopoverIfNeeded() noexcept {
    if (m_activePopover && m_ctx) {
        const bool stillOpen = m_openPtr ? *m_openPtr : false;
        if (!stillOpen && m_node && m_node->parent) {
            m_node->parent->RemoveChild(m_node);
            m_ctx->InvalidateAll();
        }
        m_activePopover = false;
    }
}

PopoverScope Popover::Open(
    Context& ctx,
    bool& open,
    const PopoverOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    auto& state = ctx.GetOrCreateState<PopoverPersistState>(id);

    if (!open) {
        state.wasOpen = false;
        return PopoverScope(&ctx, nullptr, &open, {}, false);
    }

    const bool justOpened = !state.wasOpen;
    const Rect viewportRect = opts.viewport.has_value()
        ? *opts.viewport
        : ctx.GetPopupViewportRect();
    const Rect prevPopoverRect = ctx.GetPrevUnclippedBounds(id);

    Response res{};
    res.id = id;
    res.rect = prevPopoverRect;
    res.ctx = &ctx;

    const bool childPopupOpen = ctx.GetOpenPopupId().IsValid();

    {
        auto hitScope = ctx.ScopedOverlayHitCheck(id);
        const Vec2 mouse = ctx.GetMousePos();
        res.hovered = prevPopoverRect.w > 0.0f && prevPopoverRect.Contains(mouse);

        if (!justOpened && opts.closeOnClickOutside && ctx.IsMouseClicked()) {
            const bool insidePopover = prevPopoverRect.w > 0.0f && prevPopoverRect.h > 0.0f && prevPopoverRect.Contains(mouse);
            const bool insideAnchor = opts.anchorRect.w > 0.0f && opts.anchorRect.h > 0.0f && opts.anchorRect.Contains(mouse);
            const bool insideChildPopup = childPopupOpen && ctx.GetOpenPopupRect().Contains(mouse);

            if (!insidePopover && !insideAnchor && !insideChildPopup) {
                open = false;
                state.wasOpen = false;
                res.changed = true;
                if (opts.consumeOutsideClick) {
                    ctx.ConsumeMouseClick();
                }
                ctx.InvalidateAll();
                return PopoverScope(&ctx, nullptr, &open, res, true);
            }
        }
    }

    if (opts.closeOnEscape && !childPopupOpen && ctx.ConsumeKey(Key::Escape)) {
        open = false;
        state.wasOpen = false;
        res.changed = true;
        ctx.InvalidateAll();
        return PopoverScope(&ctx, nullptr, &open, res, true);
    }

    state.wasOpen = true;

    Node* node = ctx.CreateNode(NodeType::Popover, id);
    node->direction = FlexDirection::Column;
    node->widthMode = SizeMode::Fit;
    node->heightMode = SizeMode::Fit;
    node->gap = 0.0f;
    node->padding = Padding::All(0.0f);
    node->clipChildren = false;
    node->uiSize = UiSize::Md;

    auto* payload = ctx.AllocPayload<PopoverPayload>(PopoverPayload{
        .opts = opts,
        .viewportRect = viewportRect
    });
    node->SetPayload(payload);
    node->measureFn = &Popover::Measure;
    node->visualHash = detail::HashArgs(
        opts.anchorRect.x,
        opts.anchorRect.y,
        opts.anchorRect.w,
        opts.anchorRect.h,
        static_cast<keel::u8>(opts.placement),
        opts.offset,
        viewportRect.x,
        viewportRect.y,
        viewportRect.w,
        viewportRect.h
    );

    ctx.FinishContainer(node);
    return PopoverScope(&ctx, node, &open, res, false);
}

Vec2 Popover::Measure(
    const Node* node,
    float availableWidth,
    ICanvas* /*canvas*/,
    const UiTheme& /*theme*/
) {
    float maxVpW = availableWidth > 0.0f ? availableWidth : 480.0f;
    if (const auto* payload = node->As<PopoverPayload>()) {
        if (payload->viewportRect.w > 0.0f) {
            maxVpW = payload->viewportRect.w;
        }
    }

    float totalH = 0.0f;
    float maxW = 0.0f;

    for (const Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        if (child->IsOutOfFlow()) continue;
        const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
        const float ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
        totalH += ch;
        maxW = keel::max(maxW, cw);
    }

    const float w = (maxVpW > 0.0f) ? keel::min(maxW, maxVpW) : maxW;
    return {w, totalH};
}

void Popover::Arrange(
    Node* /*node*/,
    const Rect& /*availableRect*/,
    const UiTheme& /*theme*/
) {
    // Arranged in Solver::ArrangeNode for NodeType::Popover
}

Rect Popover::ResolvePopoverRect(
    const Node* node,
    float resolvedWidth,
    float resolvedHeight,
    const Rect& fallbackViewport
) noexcept {
    if (!node) return fallbackViewport;

    Rect vp = fallbackViewport;
    PopoverOptions opts{};
    if (const auto* payload = node->As<PopoverPayload>()) {
        opts = payload->opts;
        if (payload->viewportRect.w > 1.0f && payload->viewportRect.h > 1.0f) {
            vp = payload->viewportRect;
        }
    }

    const float margin = 8.0f;
    const float maxVpW = keel::max(0.0f, vp.w - margin * 2.0f);
    const float maxVpH = keel::max(0.0f, vp.h - margin * 2.0f);
    const float w = keel::clamp(resolvedWidth, 0.0f, (vp.w > margin * 2.0f) ? maxVpW : vp.w);
    const float h = keel::clamp(resolvedHeight, 0.0f, (vp.h > margin * 2.0f) ? maxVpH : vp.h);

    const Rect& anchor = opts.anchorRect;
    if (anchor.w <= 0.0f && anchor.h <= 0.0f) {
        const float cx = std::round(vp.x + (vp.w - w) * 0.5f);
        const float cy = std::round(vp.y + (vp.h - h) * 0.5f);
        return Rect(cx, cy, std::round(w), std::round(h));
    }

    PopoverPlacement resolvedPlacement = opts.placement;
    if (resolvedPlacement == PopoverPlacement::Auto) {
        resolvedPlacement = (anchor.Center().x > vp.Center().x)
            ? PopoverPlacement::BottomEnd
            : PopoverPlacement::BottomStart;
    }

    const float gap = keel::max(0.0f, opts.offset);
    float x = anchor.x;
    if (resolvedPlacement == PopoverPlacement::BottomEnd || resolvedPlacement == PopoverPlacement::TopEnd) {
        x = anchor.Right() - w;
    }

    const float belowY = anchor.Bottom() + gap;
    const float aboveY = anchor.y - gap - h;
    float y = belowY;

    if (resolvedPlacement == PopoverPlacement::TopStart || resolvedPlacement == PopoverPlacement::TopEnd) {
        y = aboveY;
        if (y < vp.y + margin && belowY + h <= vp.Bottom() - margin) {
            y = belowY;
        }
    } else {
        if (y + h > vp.Bottom() - margin) {
            if (aboveY >= vp.y + margin) {
                y = aboveY;
            } else {
                const float spaceBelow = vp.Bottom() - anchor.Bottom();
                const float spaceAbove = anchor.y - vp.y;
                y = (spaceAbove > spaceBelow) ? aboveY : belowY;
            }
        }
    }

    x = keel::clamp(x, vp.x + margin, keel::max(vp.x + margin, vp.Right() - margin - w));
    y = keel::clamp(y, vp.y + margin, keel::max(vp.y + margin, vp.Bottom() - margin - h));
    return Rect(std::round(x), std::round(y), std::round(w), std::round(h));
}

} // namespace guikit
