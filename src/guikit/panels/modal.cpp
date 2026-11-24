#include "guikit/panels/modal.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/solver.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

struct ModalPayload {
    ModalOptions opts{};
    Rect backdropRect{};
    ICanvas* canvas{nullptr};
    Rect* contentRectOut{nullptr};
};

struct ModalPersistState {
    Rect contentRect{};
};

keel::f32 ResolveChildAlignX(Alignment align, keel::f32 availX, keel::f32 availW, keel::f32 childW) noexcept {
    switch (align) {
        case Alignment::Center: return availX + (availW - childW) * 0.5f;
        case Alignment::End:    return availX + (availW - childW);
        case Alignment::Start:
        case Alignment::Stretch:
        default:                return availX;
    }
}

keel::f32 ResolveGroupAlignY(Alignment verticalAlign, keel::f32 availY, keel::f32 availH, keel::f32 totalH) noexcept {
    switch (verticalAlign) {
        case Alignment::Center: return availY + (availH - totalH) * 0.5f;
        case Alignment::End:    return availY + (availH - totalH);
        case Alignment::Start:
        case Alignment::Stretch:
        default:                return availY;
    }
}

} // namespace

ModalScope::~ModalScope() noexcept {
    EndModalIfNeeded();
}

void ModalScope::Close() noexcept {
    if (m_openPtr) {
        *m_openPtr = false;
    }
    if (m_ctx) {
        m_ctx->InvalidateAll();
    }
}

void ModalScope::EndModalIfNeeded() noexcept {
    if (m_activeModal && m_ctx) {
        const bool stillOpen = m_openPtr ? *m_openPtr : false;
        const Id modalId = m_node ? m_node->id : Id::Null();
        if (!stillOpen && m_node && m_node->parent) {
            m_node->parent->RemoveChild(m_node);
        }
        m_ctx->EndModalScope(stillOpen, modalId);
        m_activeModal = false;
    }
}

ModalScope detail::Modal::Open(
    Context& ctx,
    bool& open,
    const ModalOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    if (!open) {
        if (ctx.HasPrevFrameModal(id)) {
            ctx.ClearModalBlock(id);
            ctx.InvalidateAll();
        }
        return ModalScope{};
    }

    Rect backdropRect = ctx.GetPopupViewportRect();
    if (opts.confineToParent) {
        const Rect parentRect = ctx.GetParentPrevFrameBounds();
        if (parentRect.w > 10.0f && parentRect.h > 10.0f) {
            backdropRect = parentRect;
        }
    }

    auto& state = ctx.GetOrCreateState<ModalPersistState>(id);

    Response res{};
    res.id = id;
    res.rect = state.contentRect;
    res.ctx = &ctx;

    const Vec2 mouse = ctx.GetMousePos();
    const bool popupOpen = ctx.GetOpenPopupId().IsValid();
    const bool isTopModal = ctx.IsTopModal(id);
    const bool hasPopoverAbove = ctx.HasPrevFramePopoverOver(id);

    if (opts.closeOnBackdrop && !popupOpen && isTopModal) {
        auto hitScope = ctx.ScopedOverlayHitCheck(id, OverlayLayer::Portal);
        const bool hasPrevContent = (state.contentRect.w > 0.5f && state.contentRect.h > 0.5f);
        const bool occluded = ctx.IsOverlayOccluded(id, mouse, OverlayLayer::Portal);
        if (hasPrevContent && !occluded && ctx.IsMouseClicked() &&
            backdropRect.Contains(mouse) && !state.contentRect.Contains(mouse)) {
            open = false;
            res.changed = true;
            ctx.ClearModalBlock(id);
            ctx.ConsumeMouseClick();
            ctx.InvalidateAll();
            return ModalScope(&ctx, nullptr, &open, res, true);
        }
    }

    if (opts.closeOnEscape && !popupOpen && isTopModal && !hasPopoverAbove && ctx.ConsumeKey(Key::Escape)) {
        open = false;
        res.changed = true;
        ctx.ClearModalBlock(id);
        ctx.InvalidateAll();
        return ModalScope(&ctx, nullptr, &open, res, true);
    }

    Node* node = ctx.CreateNode(NodeType::Modal, id);
    node->direction = FlexDirection::Column;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Stretch;
    node->gap = 0.0f;
    node->padding = opts.padding;
    node->align = opts.align;
    node->clipChildren = true;
    node->uiSize = UiSize::Md;

    auto* payload = ctx.AllocPayload<ModalPayload>(ModalPayload{
        .opts = opts,
        .backdropRect = backdropRect,
        .canvas = ctx.GetCanvas(),
        .contentRectOut = &state.contentRect
    });
    node->SetPayload(payload);
    static constexpr PanelHooks kModalHooks{
        .contentClip = [](const Node* n, const UiTheme&) -> Rect { return detail::Modal::GetBackdropRect(n); }
    };
    node->measureFn = &detail::Modal::Measure;
    node->arrangeFn = &detail::Modal::Arrange;
    node->panelHooks = &kModalHooks;
    node->renderFn = &detail::Modal::Render;
    node->visualHash = detail::HashArgs(
        backdropRect.x,
        backdropRect.y,
        backdropRect.w,
        backdropRect.h,
        opts.backdropOpacity,
        opts.backdropColor.value_or(Color::Transparent()).ToUint32(),
        opts.padding.left,
        opts.padding.top,
        opts.padding.right,
        opts.padding.bottom,
        keel::as<keel::u8>(opts.align),
        keel::as<keel::u8>(opts.verticalAlign)
    );

    ctx.BeginModalScope(id, backdropRect);
    ctx.FinishContainer(node);

    return ModalScope(&ctx, node, &open, res, false);
}

Vec2 detail::Modal::Measure(
    const Node* node,
    keel::f32 /*availableWidth*/,
    ICanvas* /*canvas*/,
    const UiTheme& /*theme*/
) {
    keel::f32 totalH = node->padding.Vertical();
    keel::f32 maxW = 0.0f;

    for (const Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        if (child->IsOutOfFlow()) continue;
        const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
        const keel::f32 ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
        totalH += ch;
        maxW = keel::max(maxW, cw);
    }

    return {maxW + node->padding.Horizontal(), totalH};
}

void detail::Modal::Arrange(
    Node* node,
    const Rect& availableRect,
    const UiTheme& theme
) {
    if (!node) return;
    const auto* payload = node->As<ModalPayload>();
    const Rect backdrop = (payload && payload->backdropRect.w > 1.0f && payload->backdropRect.h > 1.0f)
        ? payload->backdropRect
        : availableRect;
    node->computedRect = backdrop;

    const Rect contentArea(
        backdrop.x + node->padding.left,
        backdrop.y + node->padding.top,
        keel::max(0.0f, backdrop.w - node->padding.Horizontal()),
        keel::max(0.0f, backdrop.h - node->padding.Vertical())
    );
    const Alignment alignX = payload ? payload->opts.align : Alignment::Center;
    const Alignment alignY = payload ? payload->opts.verticalAlign : Alignment::Center;
    ICanvas* canvas = payload ? payload->canvas : nullptr;

    struct ChildLayout {
        Node* child{nullptr};
        keel::f32 w{0.0f};
        keel::f32 h{0.0f};
    };
    keel::SmallArray<ChildLayout, 8> items;
    keel::f32 totalH = 0.0f;
    keel::u32 flowCount = 0;

    for (Node* c = node->firstChild; c != nullptr; c = c->nextSibling) {
        if (!c->IsOutOfFlow()) {
            ++flowCount;
        }
    }

    for (Node* c = node->firstChild; c != nullptr; c = c->nextSibling) {
        if (c->IsOutOfFlow()) continue;

        keel::f32 cw = 0.0f;
        if (c->widthMode == SizeMode::Fixed && c->fixedWidth > 0.0f) {
            cw = c->fixedWidth;
        } else if (alignX == Alignment::Stretch) {
            cw = contentArea.w;
        } else {
            cw = keel::min(contentArea.w, c->intrinsicSize.x);
        }
        if (c->minWidth > 0.0f) {
            cw = keel::max(cw, c->minWidth);
        }
        if (c->maxWidth > 0.0f) {
            cw = keel::min(cw, c->maxWidth);
        }
        cw = keel::min(cw, contentArea.w);

        const UiTheme& childTheme = c->GetTheme(theme);
        keel::f32 ch = 0.0f;
        if (c->heightMode == SizeMode::Fixed && c->fixedHeight > 0.0f) {
            ch = c->fixedHeight;
        } else if (flowCount == 1 && (c->heightMode == SizeMode::Stretch || alignY == Alignment::Stretch)) {
            ch = contentArea.h;
        } else {
            ch = Solver::ResolveHeight(c, cw, canvas, childTheme);
        }
        if (c->minHeight > 0.0f) {
            ch = keel::max(ch, c->minHeight);
        }
        if (c->maxHeight > 0.0f) {
            ch = keel::min(ch, c->maxHeight);
        }
        ch = keel::min(ch, contentArea.h);

        totalH += ch;
        items.push_back(ChildLayout{c, cw, ch});
    }

    keel::f32 curY = keel::round(ResolveGroupAlignY(alignY, contentArea.y, contentArea.h, totalH));
    Rect contentUnion{};
    bool hasUnion = false;

    for (const auto& item : items) {
        const keel::f32 cx = keel::round(ResolveChildAlignX(alignX, contentArea.x, contentArea.w, item.w));
        const Rect childRect(cx, curY, keel::round(item.w), keel::round(item.h));
        Solver::Arrange(item.child, childRect, theme, canvas);
        contentUnion = hasUnion ? contentUnion.Union(childRect) : childRect;
        hasUnion = true;
        curY += item.h;
    }

    if (payload && payload->contentRectOut) {
        *(payload->contentRectOut) = contentUnion;
    }
}

Rect detail::Modal::GetBackdropRect(const Node* node) noexcept {
    if (!node) return Rect{};
    if (const auto* payload = node->As<ModalPayload>()) {
        return payload->backdropRect;
    }
    return node->computedRect;
}

void detail::Modal::Render(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& /*theme*/
) {
    const auto* payload = node->As<ModalPayload>();
    if (!payload) return;

    const ModalOptions& opts = payload->opts;
    const Rect& backdropRect = payload->backdropRect;
    if (backdropRect.w <= 0.0f || backdropRect.h <= 0.0f) {
        return;
    }

    if (opts.backdropColor.has_value()) {
        if (opts.backdropColor->a > 0) {
            canvas.DrawRect(backdropRect, *opts.backdropColor);
        }
    } else {
        const keel::f32 alpha = keel::clamp(opts.backdropOpacity, 0.0f, 0.95f);
        if (alpha > 0.001f) {
            canvas.DrawRect(
                backdropRect,
                Color(4, 7, 14, keel::as<keel::u8>(keel::clamp(keel::round_i32(alpha * 255.0f), 0, 255)))
            );
        }
    }
}

} // namespace guikit
