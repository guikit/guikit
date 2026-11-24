#include "guikit/overlays/toast.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/math.h"
#include "keel/small_array.h"

namespace guikit {

namespace {

inline constexpr Id kToastStoreId{0x544F'4153'545F'5354ULL}; // "TOAST_ST"

struct ToastStore {
    keel::Array<ToastEntry> entries;
    ToastHostOptions defaultHostOpts{};
    keel::u64 nextId{0};
    keel::u64 lastVisitedFrame{keel::Limits<keel::u64>::kMax};
};

void ToastEndFrameInvoke(Context& ctx) {
    auto* store = ctx.FindMutableState<ToastStore>(kToastStoreId);
    if (!store || store->entries.empty()) {
        return;
    }
    if (store->lastVisitedFrame != ctx.GetFrameIndex()) {
        detail::Toast::RunHost(ctx, store->defaultHostOpts);
    }
}

bool ToastHasActiveContent(const Context& ctx) noexcept {
    const auto* store = ctx.FindState<ToastStore>(kToastStoreId);
    return store != nullptr && !store->entries.empty();
}

bool ToastNeedsRepaint(const Context& ctx) noexcept {
    const auto* store = ctx.FindState<ToastStore>(kToastStoreId);
    if (!store) return false;
    for (const auto& t : store->entries) {
        if (t.duration > 0.0f) {
            return true;
        }
    }
    return false;
}

void EnsureToastHookRegistered(Context& ctx) {
    static const keel::u8 kToastHookKey = 0;
    ctx.RegisterEndFrameHook(EndFrameHook{
        .key = &kToastHookKey,
        .invoke = &ToastEndFrameInvoke,
        .hasActiveContent = &ToastHasActiveContent,
        .needsRepaint = &ToastNeedsRepaint
    });
}

} // namespace

keel::u64 detail::Toast::Show(Context& ctx, ToastOptions opts) {
    EnsureToastHookRegistered(ctx);
    auto& store = ctx.GetOrCreatePersistentState<ToastStore>(kToastStoreId);
    const keel::u64 id = ++store.nextId;
    ToastEntry entry{};
    entry.id = id;
    entry.title = keel::String::from(opts.title.text);
    entry.message = keel::String::from(opts.message.text);
    entry.titleWrap = opts.title.wrap;
    entry.messageWrap = opts.message.wrap;
    entry.variant = opts.variant;
    entry.icon = opts.icon;
    if (!opts.icon.GetPath().empty()) {
        entry.iconPath = keel::String::from(opts.icon.GetPath());
    }
    entry.duration = opts.duration;
    entry.showProgress = opts.showProgress;
    entry.closable = opts.closable;
    entry.elapsedSec = 0.0;
    entry.lastTickSec = ctx.GetCurrentTimeSec();
    store.entries.push_back(keel::move(entry));
    ctx.InvalidateAll();
    return id;
}

void detail::Toast::Dismiss(Context& ctx, keel::u64 id) noexcept {
    auto* store = ctx.FindMutableState<ToastStore>(kToastStoreId);
    if (!store) return;
    for (keel::usize i = 0; i < store->entries.size(); ++i) {
        if (store->entries[i].id == id) {
            store->entries.remove_at(i);
            ctx.InvalidateAll();
            return;
        }
    }
}

void detail::Toast::Clear(Context& ctx) noexcept {
    auto* store = ctx.FindMutableState<ToastStore>(kToastStoreId);
    if (store && !store->entries.empty()) {
        store->entries.clear();
        ctx.InvalidateAll();
    }
}

keel::usize detail::Toast::GetActiveCount(const Context& ctx) noexcept {
    const auto* store = ctx.FindState<ToastStore>(kToastStoreId);
    return store ? store->entries.size() : 0;
}

bool detail::Toast::HasHostVisited(const Context& ctx) noexcept {
    const auto* store = ctx.FindState<ToastStore>(kToastStoreId);
    return store != nullptr && store->lastVisitedFrame == ctx.GetFrameIndex();
}

void detail::Toast::SetHostOptions(Context& ctx, const ToastHostOptions& opts) noexcept {
    EnsureToastHookRegistered(ctx);
    auto& store = ctx.GetOrCreatePersistentState<ToastStore>(kToastStoreId);
    store.defaultHostOpts = opts;
}

ToastHostOptions detail::Toast::GetHostOptions(const Context& ctx) noexcept {
    const auto* store = ctx.FindState<ToastStore>(kToastStoreId);
    return store ? store->defaultHostOpts : ToastHostOptions{};
}

Icon detail::Toast::ResolveDefaultIcon(UiVariant variant, Icon explicitIcon) noexcept {
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

void detail::Toast::RunHost(
    Context& ctx,
    const ToastHostOptions& opts
) {
    EnsureToastHookRegistered(ctx);
    auto* store = ctx.FindMutableState<ToastStore>(kToastStoreId);
    if (!store) {
        return;
    }
    store->lastVisitedFrame = ctx.GetFrameIndex();
    auto& entries = store->entries;
    if (entries.empty()) {
        return;
    }

    const UiTheme theme = ctx.GetTheme();

    constexpr keel::f32 s = 1.0f;
    Rect hostBounds = ctx.GetPopupViewportRect();
    if (opts.confineToParent) {
        const Rect parentBounds = ctx.GetParentPrevFrameBounds();
        if (parentBounds.w > 20.0f && parentBounds.h > 20.0f) {
            hostBounds = parentBounds;
        }
    }

    ICanvas* canvas = ctx.GetCanvas();
    const keel::f32 cardW = keel::min(keel::max(160.0f, hostBounds.w - 24.0f * s), opts.width);
    const keel::f32 gap = opts.gap;
    const keel::f32 margin = opts.margin;
    const keel::f32 padX = 14.0f * s;
    const keel::f32 padY = 12.0f * s;
    const keel::f32 iconBoxSz = 26.0f * s;
    const keel::f32 closeBtnSz = 20.0f * s;
    const keel::f32 titleFont = theme.ResolveFontSize(UiText{}, TextRole::LabelMedium, UiSize::Md);
    const keel::f32 msgFont = theme.ResolveFontSize(UiText{}, TextRole::BodySmall, UiSize::Sm);
    const keel::f32 titleLineH = canvas ? canvas->GetLineHeight(titleFont) : (titleFont * 1.3f);
    const keel::f32 msgLineH = canvas ? canvas->GetLineHeight(msgFont) : (msgFont * 1.35f);

    const keel::f64 nowSec = ctx.GetCurrentTime();
    const Vec2 mouse = ctx.GetMousePos();

    // Determine visible slice (newest toasts up to maxVisible)
    const keel::usize maxVis = keel::max<keel::usize>(1, opts.maxVisible);
    const keel::usize startIdx = (entries.size() > maxVis) ? (entries.size() - maxVis) : 0;

    struct LayoutToast {
        keel::usize index{0};
        Rect cardRect{};
        Rect closeRect{};
        keel::f32 textW{0.0f};
        keel::f32 msgH{0.0f};
    };
    keel::SmallArray<LayoutToast, 8> layoutList;
    layoutList.reserve(entries.size() - startIdx);

    for (keel::usize i = startIdx; i < entries.size(); ++i) {
        const auto& e = entries[i];
        const keel::f32 rightReserve = e.closable ? (closeBtnSz + 8.0f * s) : 0.0f;
        const keel::f32 textW = keel::max(40.0f, cardW - padX * 2.0f - (iconBoxSz + 10.0f * s) - rightReserve);

        keel::f32 contentH = 0.0f;
        keel::f32 msgH = 0.0f;
        if (!e.title.empty()) {
            contentH += titleLineH;
        }
        if (!e.message.empty()) {
            if (!e.title.empty()) contentH += 3.0f * s;
            UiText msgText(e.message.as_ref());
            msgText.wrap = e.messageWrap;
            msgH = TextUtil::MeasureUiTextHeight(msgText, textW, msgFont, msgLineH, canvas, &theme);
            contentH += keel::max(msgLineH, msgH);
        }
        contentH = keel::max(contentH, iconBoxSz);
        const keel::f32 cardH = keel::round(padY * 2.0f + contentH + (e.showProgress && e.duration > 0.0f ? 4.0f * s : 0.0f));

        LayoutToast lt{};
        lt.index = i;
        lt.cardRect = Rect(0.0f, 0.0f, keel::round(cardW), cardH);
        lt.textW = textW;
        lt.msgH = keel::max(msgLineH, msgH);
        layoutList.push_back(lt);
    }

    const bool isBottom = (opts.placement == ToastPlacement::BottomRight ||
                           opts.placement == ToastPlacement::BottomLeft ||
                           opts.placement == ToastPlacement::BottomCenter);
    const bool isRight = (opts.placement == ToastPlacement::BottomRight ||
                          opts.placement == ToastPlacement::TopRight);
    const bool isCenter = (opts.placement == ToastPlacement::TopCenter ||
                           opts.placement == ToastPlacement::BottomCenter);

    keel::f32 curY = isBottom ? (hostBounds.Bottom() - margin) : (hostBounds.y + margin);
    // Stack newest closest to anchor edge
    for (keel::usize k = layoutList.size(); k > 0; --k) {
        auto& lt = layoutList[k - 1];
        keel::f32 x = hostBounds.x + margin;
        if (isRight) {
            x = hostBounds.Right() - margin - lt.cardRect.w;
        } else if (isCenter) {
            x = hostBounds.x + (hostBounds.w - lt.cardRect.w) * 0.5f;
        }
        keel::f32 y = curY;
        if (isBottom) {
            y = curY - lt.cardRect.h;
            curY = y - gap;
        } else {
            curY = y + lt.cardRect.h + gap;
        }
        lt.cardRect.x = keel::round(x);
        lt.cardRect.y = keel::round(y);
        lt.closeRect = Rect(
            lt.cardRect.Right() - padX * 0.75f - closeBtnSz,
            lt.cardRect.y + padY * 0.75f,
            closeBtnSz,
            closeBtnSz
        );
    }

    // Update timers, hover states, and close clicks
    keel::u64 dismissId = 0;
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
    for (keel::usize i = 0; i < entries.size(); ++i) {
        auto& e = entries[i];
        const keel::f64 dt = keel::max(0.0, nowSec - e.lastTickSec);
        e.lastTickSec = nowSec;
        if (e.duration > 0.0f) {
            if (!(opts.pauseOnHover && e.hovered)) {
                e.elapsedSec += dt;
            }
            anyAnimated = true;
        }
    }

    // Build snapshot for overlay draw pass before erasing dismissed/expired entries
    struct DrawToastData {
        keel::StringRef title{};
        keel::StringRef message{};
        TextWrap titleWrap{};
        TextWrap messageWrap{};
        UiVariant variant{UiVariant::Info};
        Icon icon{};
        keel::f32 duration{3.5f};
        keel::f64 elapsedSec{0.0};
        bool closable{true};
        bool showProgress{true};
        bool hovered{false};
        bool closeHovered{false};
        Rect cardRect{};
        Rect closeRect{};
        keel::f32 textW{0.0f};
        keel::f32 msgH{0.0f};
    };
    keel::SmallArray<DrawToastData, 8> drawList;
    drawList.reserve(layoutList.size());
    for (const auto& lt : layoutList) {
        if (lt.index >= entries.size()) continue;
        const auto& ent = entries[lt.index];
        const bool dismissed = (dismissId != 0 && ent.id == dismissId);
        const bool expired = (ent.duration > 0.0f && ent.elapsedSec >= keel::as<keel::f64>(ent.duration));
        if (dismissed || expired) continue;

        Icon safeIcon = ent.icon;
        if (!ent.iconPath.empty()) {
            safeIcon = Icon::File(ent.iconPath.as_ref());
        }
        drawList.push_back(DrawToastData{
            .title = ctx.AllocString(ent.title.as_ref()),
            .message = ctx.AllocString(ent.message.as_ref()),
            .titleWrap = ent.titleWrap,
            .messageWrap = ent.messageWrap,
            .variant = ent.variant,
            .icon = ctx.AllocIcon(safeIcon),
            .duration = ent.duration,
            .elapsedSec = ent.elapsedSec,
            .closable = ent.closable,
            .showProgress = ent.showProgress,
            .hovered = ent.hovered,
            .closeHovered = ent.closeHovered,
            .cardRect = lt.cardRect,
            .closeRect = lt.closeRect,
            .textW = lt.textW,
            .msgH = lt.msgH
        });
    }

    for (keel::usize i = 0; i < entries.size(); ) {
        const bool dismissed = (dismissId != 0 && entries[i].id == dismissId);
        const bool expired = (entries[i].duration > 0.0f && entries[i].elapsedSec >= keel::as<keel::f64>(entries[i].duration));
        if (dismissed || expired) {
            entries.remove_at(i);
            ctx.InvalidateAll();
        } else {
            ++i;
        }
    }

    if (anyAnimated) {
        ctx.InvalidateAll();
        ctx.RequestRepaint(2);
    }

    if (drawList.empty()) {
        return;
    }

    ctx.RegisterOverlay([drawList = keel::move(drawList), theme, padX, padY, iconBoxSz, closeBtnSz, titleFont, msgFont, titleLineH, msgLineH](ICanvas& c) {
        const auto& pal = theme.palette;
        const keel::f32 radius = theme.ResolveRadius(keel::kNullOpt, UiSize::Lg);

        for (const auto& dt : drawList) {
            const auto& e = dt;
            const Rect& r = dt.cardRect;

            Color accentCol = pal.accent;
            if (e.variant != UiVariant::Default && e.variant != UiVariant::Ghost && e.variant != UiVariant::Secondary) {
                accentCol = theme.GetVariant(e.variant).bg;
            }

            const Color bgCol = pal.surfaceElevated.WithAlpha(248u);
            const Color borderCol = e.hovered ? accentCol.WithAlphaF(0.65f) : pal.borderHover;
            theme.Painter().DrawSurface(c, theme, SurfacePaintParams{
                .rect = r,
                .role = SurfaceRole::Popup,
                .radius = radius,
                .bg = bgCol,
                .border = borderCol,
                .borderWidth = 1.0f,
                .elevation = Elevation::Lg,
                .shadowScale = s
            });

            c.PushClip(r);

            // Leading semantic accent stripe (3px) on left edge
            c.DrawRect(Rect(r.x, r.y, 3.5f * s, r.h), accentCol);

            // Leading icon badge
            const Icon effIcon = detail::Toast::ResolveDefaultIcon(e.variant, e.icon);
            const Rect badgeRect(r.x + padX, r.y + padY, iconBoxSz, iconBoxSz);
            c.DrawRoundRect(
                badgeRect,
                7.0f * s,
                accentCol.WithAlphaF(theme.SoftTintAlpha()),
                accentCol.WithAlphaF(0.32f),
                1.0f
            );
            const keel::f32 icSz = 14.0f * s;
            effIcon.Draw(
                c,
                Rect(badgeRect.x + (iconBoxSz - icSz) * 0.5f, badgeRect.y + (iconBoxSz - icSz) * 0.5f, icSz, icSz),
                accentCol
            );

            // Text stack
            const keel::f32 textX = badgeRect.Right() + 10.0f * s;
            keel::f32 textY = r.y + padY;
            if (!e.title.empty()) {
                UiText titleTxt(e.title);
                titleTxt.wrap = e.titleWrap;
                titleTxt.wrap.maxLines = 1;
                TextUtil::DrawUiText(
                    c,
                    titleTxt,
                    Rect(textX, textY, dt.textW, titleLineH),
                    titleFont,
                    titleLineH,
                    pal.textPrimary,
                    0.0f,
                    0.5f,
                    &theme
                );
                textY += titleLineH + 3.0f * s;
            }
            if (!e.message.empty()) {
                UiText msgTxt(e.message);
                msgTxt.wrap = e.messageWrap;
                TextUtil::DrawUiText(
                    c,
                    msgTxt,
                    Rect(textX, textY, dt.textW, dt.msgH),
                    msgFont,
                    msgLineH,
                    pal.textMuted,
                    0.0f,
                    0.0f,
                    &theme
                );
            }

            // Close button [X]
            if (e.closable) {
                if (e.closeHovered) {
                    c.DrawRoundRect(dt.closeRect, 5.0f * s, pal.surfaceHover, Color::Transparent(), 0.0f);
                }
                const keel::f32 xSz = 11.0f * s;
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
                const keel::f32 ratio = keel::clamp(
                    1.0f - keel::as<keel::f32>(e.elapsedSec / keel::as<keel::f64>(e.duration)),
                    0.0f,
                    1.0f
                );
                const keel::f32 barH = 3.0f * s;
                c.DrawRect(Rect(r.x, r.Bottom() - barH, r.w, barH), pal.borderSubtle.WithAlphaF(0.35f));
                c.DrawRect(Rect(r.x, r.Bottom() - barH, r.w * ratio, barH), accentCol);
            }

            c.PopClip();
        }
    }, OverlayLayer::Notification);
}

} // namespace guikit
