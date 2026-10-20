#include "guikit/panels/Modal.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

struct ModalPayload {
    ModalOptions opts{};
    Rect backdropRect{};
    float headerHeight{0.0f};
    float bodyPadTop{0.0f};
    bool closeHovered{false};
    bool closePressed{false};
};

struct ModalPersistState {
    bool closeArmed{false};
};

Icon ResolveModalIcon(UiVariant variant, Icon explicitIcon) noexcept {
    if (explicitIcon.IsValid()) {
        return explicitIcon;
    }
    switch (variant) {
        case UiVariant::Info:    return Icon::Info();
        case UiVariant::Success: return Icon::Check();
        case UiVariant::Warning: return Icon::Warning();
        case UiVariant::Danger:  return Icon::Error();
        default:                 return Icon{};
    }
}

Rect ComputeCloseButtonRect(const Rect& dialogRect, float scale) noexcept {
    const float btnSize = 26.0f * scale;
    const float margin = 12.0f * scale;
    return Rect(
        dialogRect.Right() - margin - btnSize,
        dialogRect.y + margin,
        btnSize,
        btnSize
    );
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
        m_ctx->EndModalScope(m_openPtr ? *m_openPtr : false);
        m_activeModal = false;
    }
}

float Modal::ComputeHeaderHeight(
    const ModalOptions& opts,
    UiSize size,
    const UiTheme& theme,
    ICanvas* canvas
) noexcept {
    const bool hasTitle = !opts.title.empty();
    const bool hasSub = !opts.subtitle.empty();
    const Icon effIcon = ResolveModalIcon(opts.variant, opts.icon);
    if (!hasTitle && !hasSub && !effIcon.IsValid() && !opts.showCloseButton) {
        return 0.0f;
    }

    const float s = (theme.scale > 0.0f) ? theme.scale : 1.0f;
    const float titleFont = theme.ResolveFontSize(opts.title, TextRole::TitleMedium, size);
    const float subFont = theme.ResolveFontSize(opts.subtitle, TextRole::BodySmall, size);
    const float titleLineH = canvas ? canvas->GetLineHeight(titleFont) : (titleFont * 1.3f);
    const float subLineH = canvas ? canvas->GetLineHeight(subFont) : (subFont * 1.3f);

    float textStackH = 0.0f;
    if (hasTitle) {
        textStackH += titleLineH;
    }
    if (hasSub) {
        if (hasTitle) textStackH += 3.0f * s;
        textStackH += subLineH;
    }

    const float minHeaderContentH = 26.0f * s;
    const float contentH = std::max(minHeaderContentH, textStackH);
    const float topPad = 14.0f * s;
    const float bottomPad = 12.0f * s;
    return topPad + contentH + bottomPad;
}

ModalScope Modal::Open(
    Context& ctx,
    bool& open,
    const ModalOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    if (!open) {
        if (ctx.GetPrevFrameModalId() == id) {
            ctx.ClearModalBlock();
        }
        return ModalScope{};
    }

    const Id closeBtnId(detail::HashArgs(id.value, uint64_t(0x4D4F44414C5F58ull)));
    const UiSize activeSize = ctx.GetCurrentSize();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());

    const Vec2 vp = ctx.GetViewportSize();
    const float dpi = (ctx.GetDpiScale() > 0.0f) ? ctx.GetDpiScale() : 1.0f;
    const Vec2 logicalVp = vp / dpi;
    Rect backdropRect(0.0f, 0.0f, logicalVp.x, logicalVp.y);
    if (opts.confineToParent) {
        const Rect parentRect = ctx.GetParentPrevFrameBounds();
        if (parentRect.w > 10.0f && parentRect.h > 10.0f) {
            backdropRect = parentRect;
        }
    }

    const Rect prevDialogRect = ctx.GetPrevUnclippedBounds(id);
    const bool hasPrevDialog = (prevDialogRect.w > 1.0f && prevDialogRect.h > 1.0f);
    auto& state = ctx.GetOrCreateState<ModalPersistState>(id);

    Response res{};
    res.id = id;
    res.rect = prevDialogRect;
    res.ctx = &ctx;

    const Vec2 mouse = ctx.GetMousePos();
    const bool popupOpen = ctx.GetOpenPopupId().IsValid();
    bool closeHovered = false;
    bool closePressed = false;

    if (hasPrevDialog && !popupOpen) {
        ctx.BeginOverlayHitCheck(id);
        const Rect closeRect = ComputeCloseButtonRect(prevDialogRect, activeScale);
        if (opts.showCloseButton && closeRect.Contains(mouse)) {
            closeHovered = true;
            ctx.SetTooltip("Close (Esc)", closeRect);
            if (ctx.IsMouseClicked()) {
                state.closeArmed = true;
                ctx.SetActiveId(closeBtnId);
                ctx.ConsumeMouseClick();
            }
        } else if (opts.closeOnBackdrop && ctx.IsMouseClicked() &&
                   backdropRect.Contains(mouse) && !prevDialogRect.Contains(mouse)) {
            open = false;
            state.closeArmed = false;
            res.changed = true;
            ctx.ClearModalBlock();
            ctx.ConsumeMouseClick();
            ctx.InvalidateAll();
            ctx.EndOverlayHitCheck();
            return ModalScope(&ctx, nullptr, &open, res, true);
        }

        if (state.closeArmed) {
            if (ctx.IsMouseDown()) {
                closePressed = closeRect.Contains(mouse);
            } else {
                state.closeArmed = false;
                if (ctx.GetActiveId() == closeBtnId) {
                    ctx.SetActiveId(Id::Null());
                }
                if (closeRect.Contains(mouse)) {
                    open = false;
                    res.changed = true;
                    res.clicked = true;
                    ctx.ClearModalBlock();
                    ctx.ConsumeMouseClick();
                    ctx.InvalidateAll();
                    ctx.EndOverlayHitCheck();
                    return ModalScope(&ctx, nullptr, &open, res, true);
                }
                ctx.ConsumeMouseClick();
            }
        }
        ctx.EndOverlayHitCheck();
    } else {
        state.closeArmed = false;
    }

    if (opts.closeOnEscape && !popupOpen && ctx.ConsumeKey(Key::Escape)) {
        open = false;
        state.closeArmed = false;
        res.changed = true;
        ctx.ClearModalBlock();
        ctx.InvalidateAll();
        return ModalScope(&ctx, nullptr, &open, res, true);
    }

    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const UiTheme& theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();

    ModalOptions allocatedOpts = opts;
    allocatedOpts.title = ctx.AllocText(opts.title);
    allocatedOpts.subtitle = ctx.AllocText(opts.subtitle);
    if (!opts.icon.GetPath().empty()) {
        allocatedOpts.icon = Icon::File(ctx.AllocString(opts.icon.GetPath()));
    }

    const float headerH = ComputeHeaderHeight(allocatedOpts, activeSize, theme, canvas);
    const float defaultPad = theme.Scale(18.0f);
    const Padding bodyPad = opts.padding.has_value()
        ? theme.ResolvePadding(*opts.padding, activeSize)
        : Padding::All(defaultPad);

    Node* node = ctx.CreateNode(NodeType::Modal, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Column;
    node->widthMode = SizeMode::Fixed;
    node->fixedWidth = theme.Scale(std::max(180.0f, opts.width));
    if (opts.height.has_value() && *opts.height > 0.0f) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(*opts.height);
    } else {
        node->heightMode = SizeMode::Fit;
    }
    node->gap = opts.gap.has_value() ? theme.Scale(*opts.gap) : theme.Scale(12.0f);
    node->padding = Padding(
        bodyPad.left,
        headerH + bodyPad.top,
        bodyPad.right,
        bodyPad.bottom
    );
    node->cornerRadius = opts.borderRadius.has_value()
        ? theme.Scale(*opts.borderRadius)
        : theme.ResolveRadius(std::nullopt, activeSize);
    node->clipChildren = true;
    node->uiSize = activeSize;
    node->disabled = ctx.IsDisabled();
    node->hovered = closeHovered;
    node->pressed = closePressed;

    auto* payload = ctx.AllocPayload<ModalPayload>(ModalPayload{
        .opts = allocatedOpts,
        .backdropRect = backdropRect,
        .headerHeight = headerH,
        .bodyPadTop = bodyPad.top,
        .closeHovered = closeHovered,
        .closePressed = closePressed
    });
    node->SetPayload(payload);
    node->measureFn = &Modal::Measure;
    node->renderFn = &Modal::Render;
    node->visualHash = detail::HashArgs(
        allocatedOpts.title.text,
        allocatedOpts.subtitle.text,
        static_cast<uint32_t>(allocatedOpts.variant),
        allocatedOpts.width,
        allocatedOpts.backdropOpacity,
        closeHovered,
        closePressed
    );

    ctx.PushContainer(node);
    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(false);
    ctx.BeginModalScope(id, backdropRect);

    return ModalScope(&ctx, node, &open, res, false);
}

Vec2 Modal::Measure(
    const Node* node,
    float /*availableWidth*/,
    ICanvas* /*canvas*/,
    const UiTheme& /*theme*/
) {
    float totalH = node->padding.Vertical();
    float maxW = 0.0f;
    uint32_t activeChildren = 0;

    for (const Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        if (child->type == NodeType::Modal) continue;
        const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
        const float ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
        totalH += ch;
        maxW = std::max(maxW, cw);
        ++activeChildren;
    }
    if (activeChildren > 1) {
        totalH += static_cast<float>(activeChildren - 1) * node->gap;
    }

    const float w = (node->widthMode == SizeMode::Fixed && node->fixedWidth > 0.0f)
        ? node->fixedWidth
        : (maxW + node->padding.Horizontal());
    const float h = (node->heightMode == SizeMode::Fixed && node->fixedHeight > 0.0f)
        ? node->fixedHeight
        : totalH;
    return {w, h};
}

void Modal::Arrange(
    Node* /*node*/,
    const Rect& /*availableRect*/,
    const UiTheme& /*theme*/
) {
    // Arranged in Solver::ArrangeNode for NodeType::Modal
}

Rect Modal::ResolveDialogRect(
    const Node* node,
    float resolvedHeight,
    const Rect& fallbackBackdrop
) noexcept {
    if (!node) return fallbackBackdrop;
    Rect backdrop = fallbackBackdrop;
    float maxRatio = 0.88f;
    if (const auto* payload = node->As<ModalPayload>()) {
        if (payload->backdropRect.w > 1.0f && payload->backdropRect.h > 1.0f) {
            backdrop = payload->backdropRect;
        }
        maxRatio = std::clamp(payload->opts.maxHeightRatio, 0.25f, 0.98f);
    }
    const float w = std::min(std::max(120.0f, backdrop.w - 24.0f), node->fixedWidth);
    const float maxH = std::max(80.0f, backdrop.h * maxRatio);
    const float h = std::min(maxH, std::max(60.0f, resolvedHeight));
    const float x = std::round(backdrop.x + (backdrop.w - w) * 0.5f);
    const float y = std::round(backdrop.y + (backdrop.h - h) * 0.46f);
    return Rect(x, y, w, h);
}

Rect Modal::GetBodyClipRect(const Node* node) noexcept {
    if (!node) return Rect{};
    float headerH = 0.0f;
    if (const auto* payload = node->As<ModalPayload>()) {
        headerH = payload->headerHeight;
    }
    const Rect& r = node->computedRect;
    return Rect(
        r.x + 1.0f,
        r.y + headerH,
        std::max(0.0f, r.w - 2.0f),
        std::max(0.0f, r.h - headerH - 1.0f)
    );
}

void Modal::Render(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    const auto* payload = node->As<ModalPayload>();
    if (!payload) return;

    const ModalOptions& opts = payload->opts;
    const Rect& backdropRect = payload->backdropRect;
    const Rect& dialogRect = node->computedRect;
    const auto& pal = theme.palette;
    const float s = (theme.scale > 0.0f) ? theme.scale : 1.0f;

    // 1. Render translucent backdrop overlay
    const float alpha = std::clamp(opts.backdropOpacity, 0.0f, 0.95f);
    if (alpha > 0.001f && backdropRect.w > 0.0f && backdropRect.h > 0.0f) {
        canvas.DrawRect(backdropRect, Color(4, 7, 14, static_cast<uint8_t>(std::round(alpha * 255.0f))));
    }

    // 2. Render multi-layer ambient drop shadow behind the dialog box
    const float r = node->cornerRadius;
    canvas.DrawRoundRect(
        Rect(dialogRect.x - 2.0f * s, dialogRect.y + 8.0f * s, dialogRect.w + 4.0f * s, dialogRect.h + 8.0f * s),
        r + 3.0f * s,
        Color(0, 0, 0, 55),
        Color::Transparent(),
        0.0f
    );
    canvas.DrawRoundRect(
        Rect(dialogRect.x, dialogRect.y + 3.0f * s, dialogRect.w, dialogRect.h + 2.0f * s),
        r + 1.0f * s,
        Color(0, 0, 0, 75),
        Color::Transparent(),
        0.0f
    );

    // 3. Render elevated dialog surface & crisp border
    const Color surfaceBg = (theme.mode == ThemeMode::Dark)
        ? Color(19, 26, 41, 255)
        : Color(255, 255, 255, 255);
    Color borderCol = pal.borderHover;
    Color accentCol = pal.accent;
    bool hasSemanticVariant = (opts.variant != UiVariant::Default && opts.variant != UiVariant::Ghost && opts.variant != UiVariant::Secondary);
    if (hasSemanticVariant) {
        accentCol = theme.GetVariant(opts.variant).bg;
    }

    canvas.DrawRoundRect(dialogRect, r, surfaceBg, borderCol, 1.0f);

    // Optional top semantic accent bar clipped inside dialog top corners
    if (hasSemanticVariant) {
        canvas.PushClip(Rect(dialogRect.x, dialogRect.y, dialogRect.w, 4.0f * s));
        canvas.DrawRoundRect(
            Rect(dialogRect.x, dialogRect.y, dialogRect.w, std::max(r * 2.0f, 8.0f * s)),
            r,
            accentCol,
            Color::Transparent(),
            0.0f
        );
        canvas.PopClip();
    }

    // 4. Render header (icon badge, title, subtitle, close button, and divider)
    const float headerH = payload->headerHeight;
    if (headerH > 0.0f) {
        const float padX = node->padding.left;
        const float topPad = 14.0f * s;
        float contentX = dialogRect.x + padX;
        float availRight = dialogRect.Right() - node->padding.right;

        if (opts.showCloseButton) {
            const Rect closeRect = ComputeCloseButtonRect(dialogRect, s);
            availRight = closeRect.x - 8.0f * s;

            if (payload->closeHovered || payload->closePressed) {
                const Color closeBg = payload->closePressed
                    ? pal.surfaceActive
                    : pal.surfaceHover;
                canvas.DrawRoundRect(closeRect, 6.0f * s, closeBg, Color::Transparent(), 0.0f);
            }
            const float iconSz = 13.0f * s;
            const Rect iconBox(
                closeRect.x + (closeRect.w - iconSz) * 0.5f,
                closeRect.y + (closeRect.h - iconSz) * 0.5f,
                iconSz,
                iconSz
            );
            const Color closeIconCol = payload->closeHovered ? pal.textPrimary : pal.textMuted;
            Icon::Close().Draw(canvas, iconBox, closeIconCol);
        }

        const Icon effIcon = ResolveModalIcon(opts.variant, opts.icon);
        if (effIcon.IsValid()) {
            const float badgeSz = 32.0f * s;
            const Rect badgeRect(contentX, dialogRect.y + topPad, badgeSz, badgeSz);
            const Color badgeBg = accentCol.WithAlphaF(theme.mode == ThemeMode::Dark ? 0.18f : 0.14f);
            const Color badgeBorder = accentCol.WithAlphaF(0.35f);
            canvas.DrawRoundRect(badgeRect, 8.0f * s, badgeBg, badgeBorder, 1.0f);

            const float icSz = 16.0f * s;
            const Rect icRect(
                badgeRect.x + (badgeSz - icSz) * 0.5f,
                badgeRect.y + (badgeSz - icSz) * 0.5f,
                icSz,
                icSz
            );
            effIcon.Draw(canvas, icRect, accentCol);
            contentX += badgeSz + 12.0f * s;
        }

        const float textW = std::max(10.0f, availRight - contentX);
        float textY = dialogRect.y + topPad;

        if (!opts.title.empty()) {
            const float titleFont = theme.ResolveFontSize(opts.title, TextRole::TitleMedium, node->uiSize);
            const float titleLineH = canvas.GetLineHeight(titleFont);
            const Color titleCol = theme.ResolveTextColor(opts.title, pal.textPrimary);
            TextUtil::DrawUiText(
                canvas,
                opts.title,
                Rect(contentX, textY, textW, titleLineH),
                titleFont,
                titleLineH,
                titleCol,
                0.0f,
                0.5f
            );
            textY += titleLineH + 3.0f * s;
        }

        if (!opts.subtitle.empty()) {
            const float subFont = theme.ResolveFontSize(opts.subtitle, TextRole::BodySmall, node->uiSize);
            const float subLineH = canvas.GetLineHeight(subFont);
            const Color subCol = theme.ResolveTextColor(opts.subtitle, pal.textMuted);
            TextUtil::DrawUiText(
                canvas,
                opts.subtitle,
                Rect(contentX, textY, textW, subLineH),
                subFont,
                subLineH,
                subCol,
                0.0f,
                0.5f
            );
        }

        if (opts.showHeaderDivider) {
            const float divY = std::round(dialogRect.y + headerH) - 1.0f;
            canvas.DrawRect(
                Rect(dialogRect.x + 1.0f, divY, std::max(0.0f, dialogRect.w - 2.0f), 1.0f),
                pal.borderSubtle
            );
        }
    }
}

} // namespace guikit
