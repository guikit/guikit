#include "guikit/widgets/Toast.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

Icon Toast::ResolveDefaultIcon(UiVariant variant, Icon explicitIcon) noexcept {
    if (explicitIcon.IsValid()) {
        return explicitIcon;
    }
    switch (variant) {
        case UiVariant::Success: return Icon::Check();
        case UiVariant::Warning: return Icon::Warning();
        case UiVariant::Danger:  return Icon::Error();
        case UiVariant::Primary: return Icon::Flash();
        case UiVariant::Info:
        default:                 return Icon::Info();
    }
}

void Toast::RunHost(
    Context& ctx,
    const ToastHostOptions& opts
) {
    ctx.MarkToastHostVisited();
    auto& entries = ctx.GetToastEntries();
    if (entries.empty()) {
        return;
    }

    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const UiTheme theme = ctx.GetTheme();
    if (pushedTheme != nullptr) {
        ctx.PopTheme();
    }

    const float s = (theme.scale > 0.0f) ? theme.scale : activeScale;
    const Vec2 vp = ctx.GetViewportSize() / ((ctx.GetDpiScale() > 0.0f) ? ctx.GetDpiScale() : 1.0f);
    Rect hostBounds(0.0f, 0.0f, vp.x, vp.y);
    if (opts.confineToParent) {
        const Rect parentBounds = ctx.GetParentPrevFrameBounds();
        if (parentBounds.w > 20.0f && parentBounds.h > 20.0f) {
            hostBounds = parentBounds;
        }
    }

    ICanvas* canvas = ctx.GetCanvas();
    const float cardW = std::min(std::max(160.0f, hostBounds.w - 24.0f * s), theme.Scale(opts.width));
    const float gap = theme.Scale(opts.gap);
    const float margin = theme.Scale(opts.margin);
    const float padX = 14.0f * s;
    const float padY = 12.0f * s;
    const float iconBoxSz = 26.0f * s;
    const float closeBtnSz = 20.0f * s;
    const float titleFont = theme.ResolveFontSize(UiText{}, TextRole::LabelMedium, UiSize::Md);
    const float msgFont = theme.ResolveFontSize(UiText{}, TextRole::BodySmall, UiSize::Sm);
    const float titleLineH = canvas ? canvas->GetLineHeight(titleFont) : (titleFont * 1.3f);
    const float msgLineH = canvas ? canvas->GetLineHeight(msgFont) : (msgFont * 1.35f);

    const double nowSec = ctx.GetCurrentTime();
    const Vec2 mouse = ctx.GetMousePos();

    // Determine visible slice (newest toasts up to maxVisible)
    const size_t maxVis = std::max<size_t>(1, opts.maxVisible);
    const size_t startIdx = (entries.size() > maxVis) ? (entries.size() - maxVis) : 0;

    struct LayoutToast {
        size_t index{0};
        Rect cardRect{};
        Rect closeRect{};
        float textW{0.0f};
        float msgH{0.0f};
    };
    std::vector<LayoutToast> layoutList;
    layoutList.reserve(entries.size() - startIdx);

    for (size_t i = startIdx; i < entries.size(); ++i) {
        const auto& e = entries[i];
        const float rightReserve = e.closable ? (closeBtnSz + 8.0f * s) : 0.0f;
        const float textW = std::max(40.0f, cardW - padX * 2.0f - (iconBoxSz + 10.0f * s) - rightReserve);

        float contentH = 0.0f;
        float msgH = 0.0f;
        if (!e.title.empty()) {
            contentH += titleLineH;
        }
        if (!e.message.empty()) {
            if (!e.title.empty()) contentH += 3.0f * s;
            msgH = TextUtil::MeasureUiTextHeight(UiText(e.message), textW, msgFont, msgLineH, canvas);
            contentH += std::max(msgLineH, msgH);
        }
        contentH = std::max(contentH, iconBoxSz);
        const float cardH = std::round(padY * 2.0f + contentH + (e.showProgress && e.duration > 0.0f ? 4.0f * s : 0.0f));

        LayoutToast lt{};
        lt.index = i;
        lt.cardRect = Rect(0.0f, 0.0f, std::round(cardW), cardH);
        lt.textW = textW;
        lt.msgH = std::max(msgLineH, msgH);
        layoutList.push_back(lt);
    }

    const bool isBottom = (opts.placement == ToastPlacement::BottomRight ||
                           opts.placement == ToastPlacement::BottomLeft ||
                           opts.placement == ToastPlacement::BottomCenter);
    const bool isRight = (opts.placement == ToastPlacement::BottomRight ||
                          opts.placement == ToastPlacement::TopRight);
    const bool isCenter = (opts.placement == ToastPlacement::TopCenter ||
                           opts.placement == ToastPlacement::BottomCenter);

    float curY = isBottom ? (hostBounds.Bottom() - margin) : (hostBounds.y + margin);
    // Stack newest closest to anchor edge
    for (size_t k = layoutList.size(); k > 0; --k) {
        auto& lt = layoutList[k - 1];
        float x = hostBounds.x + margin;
        if (isRight) {
            x = hostBounds.Right() - margin - lt.cardRect.w;
        } else if (isCenter) {
            x = hostBounds.x + (hostBounds.w - lt.cardRect.w) * 0.5f;
        }
        float y = curY;
        if (isBottom) {
            y = curY - lt.cardRect.h;
            curY = y - gap;
        } else {
            curY = y + lt.cardRect.h + gap;
        }
        lt.cardRect.x = std::round(x);
        lt.cardRect.y = std::round(y);
        lt.closeRect = Rect(
            lt.cardRect.Right() - padX * 0.75f - closeBtnSz,
            lt.cardRect.y + padY * 0.75f,
            closeBtnSz,
            closeBtnSz
        );
    }

    // Update timers, hover states, and close clicks
    uint64_t dismissId = 0;
    bool anyAnimated = false;

    for (const auto& lt : layoutList) {
        auto& e = entries[lt.index];
        const bool wasHov = e.hovered;
        const bool wasCloseHov = e.closeHovered;
        e.hovered = lt.cardRect.Contains(mouse);
        e.closeHovered = e.closable && lt.closeRect.Contains(mouse);
        if (e.hovered != wasHov || e.closeHovered != wasCloseHov) {
            ctx.InvalidateAll();
        }

        if (e.hovered && ctx.IsMouseClicked()) {
            if (e.closeHovered) {
                dismissId = e.id;
            }
            ctx.ConsumeMouseClick();
        }
    }

    // Advance timers on all entries
    for (auto it = entries.begin(); it != entries.end(); ) {
        if (it->id == dismissId && dismissId != 0) {
            it = entries.erase(it);
            ctx.InvalidateAll();
            continue;
        }
        const double dt = std::max(0.0, nowSec - it->lastTickSec);
        it->lastTickSec = nowSec;
        if (it->duration > 0.0f) {
            if (!(opts.pauseOnHover && it->hovered)) {
                it->elapsedSec += dt;
            }
            anyAnimated = true;
            if (it->elapsedSec >= static_cast<double>(it->duration)) {
                it = entries.erase(it);
                ctx.InvalidateAll();
                continue;
            }
        }
        ++it;
    }

    if (anyAnimated) {
        ctx.InvalidateAll();
        ctx.RequestRepaint(2);
    }

    if (entries.empty()) {
        return;
    }

    // Rebuild snapshot for overlay draw pass after any erasures
    struct DrawToastData {
        ToastEntry entry;
        Rect cardRect;
        Rect closeRect;
        float textW;
        float msgH;
    };
    std::vector<DrawToastData> drawList;
    drawList.reserve(layoutList.size());
    for (const auto& lt : layoutList) {
        if (lt.index < entries.size() && entries[lt.index].id != dismissId) {
            drawList.push_back(DrawToastData{
                .entry = entries[lt.index],
                .cardRect = lt.cardRect,
                .closeRect = lt.closeRect,
                .textW = lt.textW,
                .msgH = lt.msgH
            });
        }
    }

    ctx.RegisterOverlay([drawList = std::move(drawList), theme, s, padX, padY, iconBoxSz, closeBtnSz, titleFont, msgFont, titleLineH, msgLineH](ICanvas& c) {
        const auto& pal = theme.palette;
        const float radius = 10.0f * s;

        for (const auto& dt : drawList) {
            const auto& e = dt.entry;
            const Rect& r = dt.cardRect;

            // Shadow
            c.DrawRoundRect(
                Rect(r.x - 1.0f * s, r.y + 4.0f * s, r.w + 2.0f * s, r.h + 4.0f * s),
                radius + 2.0f * s,
                Color(0, 0, 0, 60),
                Color::Transparent(),
                0.0f
            );

            Color accentCol = pal.accent;
            if (e.variant != UiVariant::Default && e.variant != UiVariant::Ghost && e.variant != UiVariant::Secondary) {
                accentCol = theme.GetVariant(e.variant).bg;
            }

            const Color bgCol = (theme.mode == ThemeMode::Dark)
                ? Color(19, 26, 41, 248)
                : Color(255, 255, 255, 250);
            const Color borderCol = e.hovered ? accentCol.WithAlphaF(0.65f) : pal.borderHover;
            c.DrawRoundRect(r, radius, bgCol, borderCol, 1.0f);

            c.PushClip(r);

            // Leading semantic accent stripe (3px) on left edge
            c.DrawRect(Rect(r.x, r.y, 3.5f * s, r.h), accentCol);

            // Leading icon badge
            const Icon effIcon = Toast::ResolveDefaultIcon(e.variant, e.icon);
            const Rect badgeRect(r.x + padX, r.y + padY, iconBoxSz, iconBoxSz);
            c.DrawRoundRect(
                badgeRect,
                7.0f * s,
                accentCol.WithAlphaF(theme.mode == ThemeMode::Dark ? 0.18f : 0.14f),
                accentCol.WithAlphaF(0.32f),
                1.0f
            );
            const float icSz = 14.0f * s;
            effIcon.Draw(
                c,
                Rect(badgeRect.x + (iconBoxSz - icSz) * 0.5f, badgeRect.y + (iconBoxSz - icSz) * 0.5f, icSz, icSz),
                accentCol
            );

            // Text stack
            const float textX = badgeRect.Right() + 10.0f * s;
            float textY = r.y + padY;
            if (!e.title.empty()) {
                c.DrawTextBox(
                    e.title,
                    Rect(textX, textY, dt.textW, titleLineH),
                    titleFont,
                    pal.textPrimary,
                    0.0f,
                    0.5f
                );
                textY += titleLineH + 3.0f * s;
            }
            if (!e.message.empty()) {
                TextUtil::DrawUiText(
                    c,
                    UiText(e.message),
                    Rect(textX, textY, dt.textW, dt.msgH),
                    msgFont,
                    msgLineH,
                    pal.textMuted,
                    0.0f,
                    0.0f
                );
            }

            // Close button [X]
            if (e.closable) {
                if (e.closeHovered) {
                    c.DrawRoundRect(dt.closeRect, 5.0f * s, pal.surfaceHover, Color::Transparent(), 0.0f);
                }
                const float xSz = 11.0f * s;
                Icon::Close().Draw(
                    c,
                    Rect(
                        dt.closeRect.x + (closeBtnSz - xSz) * 0.5f,
                        dt.closeRect.y + (closeBtnSz - xSz) * 0.5f,
                        xSz,
                        xSz
                    ),
                    e.closeHovered ? pal.textPrimary : pal.textMuted
                );
            }

            // Bottom countdown progress bar
            if (e.showProgress && e.duration > 0.0f) {
                const float ratio = std::clamp(
                    1.0f - static_cast<float>(e.elapsedSec / static_cast<double>(e.duration)),
                    0.0f,
                    1.0f
                );
                const float barH = 3.0f * s;
                c.DrawRect(Rect(r.x, r.Bottom() - barH, r.w, barH), pal.borderSubtle.WithAlphaF(0.35f));
                c.DrawRect(Rect(r.x, r.Bottom() - barH, r.w * ratio, barH), accentCol);
            }

            c.PopClip();
        }
    });
}

} // namespace guikit
