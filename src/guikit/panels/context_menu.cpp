#include "guikit/panels/context_menu.hpp"
#include <cmath>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/small_array.h"

namespace guikit {

namespace {

struct ContextMenuPersistState {
    Vec2 anchorPos{0.0f, 0.0f};
    keel::Optional<Vec2> lastCallerAnchorPos{keel::kNullOpt};
    Rect anchorRect{0.0f, 0.0f, 0.0f, 0.0f};
    keel::i32 keyboardIndex{-1};
    bool wasOpen{false};
};

} // namespace

ContextMenuResponse ContextMenu::Run(
    Context& ctx,
    bool& open,
    keel::ArrayRef<ContextMenuItem> items,
    const ContextMenuOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme theme = ctx.GetTheme();

    auto& state = ctx.GetOrCreateState<ContextMenuPersistState>(id);
    Rect anchorRect = opts.anchorRect;
    if (anchorRect.w > 0.0f || anchorRect.h > 0.0f) {
        state.anchorRect = anchorRect;
    } else if (!opts.anchorPos.has_value() && (state.anchorRect.w > 0.0f || state.anchorRect.h > 0.0f)) {
        anchorRect = state.anchorRect;
    }
    const bool wasPopupOpen = ctx.IsPopupOpen(id);

    ContextMenuResponse res{};
    res.id = id;
    res.rect = anchorRect;
    res.ctx = &ctx;
    res.clickedIndex = -1;

    if (effectiveDisabled || items.empty()) {
        if (wasPopupOpen) {
            ctx.ClosePopup();
        }
        open = false;
        state.wasOpen = false;
        state.lastCallerAnchorPos.reset();
        res.isOpen = false;
        return res;
    }

    if (!open) {
        if (wasPopupOpen) {
            ctx.ClosePopup();
        }
        state.wasOpen = false;
        state.lastCallerAnchorPos.reset();
        res.isOpen = false;
        return res;
    }

    if (!wasPopupOpen && state.wasOpen) {
        // Popup was dismissed by clicking outside in BeginFrame
        open = false;
        state.wasOpen = false;
        state.lastCallerAnchorPos.reset();
        res.isOpen = false;
        return res;
    }

    // Measure menu geometry
    ICanvas* canvas = ctx.GetCanvas();
    const float s = 1.0f;
    const ComponentMetrics& cm = theme.GetMetrics(activeSize);
    const float rowH = std::round(keel::max(32.0f * s, cm.defaultHeight * 0.9f));
    const float sepH = std::round(9.0f * s);
    const float padX = std::round(12.0f * s);
    const float padY = std::round(6.0f * s);
    const float icSz = cm.iconSize;
    const float iconSlotW = std::round(icSz + 8.0f * s);
    const float shortcutGap = std::round(24.0f * s);
    const float labelFont = theme.ResolveFontSize(UiText{}, TextRole::LabelLarge, activeSize);
    const float shortcutFont = theme.ResolveFontSize(UiText{}, TextRole::LabelMedium, activeSize);

    bool anyLeadingSlot = false;
    bool anyTrailingCheck = false;
    for (const auto& item : items) {
        if (!item.isSeparatorOnly && (item.icon.IsValid() || item.hasCheck || item.checked)) {
            anyLeadingSlot = true;
            if (item.icon.IsValid() && (item.hasCheck || item.checked)) {
                anyTrailingCheck = true;
            }
        }
    }

    float maxContentW = 0.0f;
    float totalH = padY * 2.0f;
    for (const auto& item : items) {
        if (item.isSeparatorOnly) {
            totalH += sepH;
            continue;
        }
        totalH += rowH;
        if (item.separatorAfter) {
            totalH += sepH;
        }
        const float lw = TextUtil::MeasureUiTextWidth(item.label, labelFont, canvas);
        const float sw = item.shortcut.empty()
            ? 0.0f
            : (shortcutGap + TextUtil::MeasureUiTextWidth(item.shortcut, shortcutFont, canvas));
        const float leadW = anyLeadingSlot ? iconSlotW : 0.0f;
        const float trailW = anyTrailingCheck ? iconSlotW : 0.0f;
        maxContentW = keel::max(maxContentW, leadW + lw + sw + trailW);
    }

    float menuW = maxContentW + padX * 2.0f + 8.0f * s;
    if (opts.minWidth.has_value()) {
        menuW = keel::max(menuW, *opts.minWidth);
    }
    if (opts.width.has_value() && *opts.width > 0.0f) {
        menuW = *opts.width;
    }
    const float menuH = totalH;

    const Rect vp = ctx.GetPopupViewportRect();
    auto clampMenuRect = [&](Vec2 anchor, bool fromAnchorRect) -> Rect {
        float x = anchor.x;
        float y = anchor.y;
        if (vp.w > 0.0f && x + menuW > vp.Right() - 6.0f) {
            if (fromAnchorRect && anchorRect.w > 0.0f) {
                x = anchorRect.Right() - menuW;
            } else {
                x = anchor.x - menuW;
            }
        }
        if (vp.w > 0.0f) {
            x = keel::clamp(x, vp.x + 6.0f, keel::max(vp.x + 6.0f, vp.Right() - menuW - 6.0f));
        }
        if (vp.h > 0.0f && y + menuH > vp.Bottom() - 6.0f) {
            if (fromAnchorRect && anchorRect.h > 0.0f && anchorRect.y - menuH - 4.0f >= vp.y + 6.0f) {
                y = anchorRect.y - menuH - 4.0f;
            } else {
                y = vp.Bottom() - menuH - 6.0f;
            }
        }
        if (vp.h > 0.0f) {
            y = keel::clamp(y, vp.y + 6.0f, keel::max(vp.y + 6.0f, vp.Bottom() - menuH - 6.0f));
        }
        return Rect(std::round(x), std::round(y), std::round(menuW), std::round(menuH));
    };

    const Vec2 mouse = ctx.GetMousePos();
    const bool fromAnchorRect = !opts.anchorPos.has_value() && anchorRect.h > 0.0f;
    bool callerMovedAnchor = false;

    if (!wasPopupOpen) {
        // Fresh open: latch anchor coordinates for the lifetime of this popup
        state.anchorPos = opts.anchorPos.value_or(
            (anchorRect.w > 0.0f || anchorRect.h > 0.0f)
                ? Vec2{anchorRect.x, anchorRect.Bottom() + 4.0f}
                : mouse
        );
        state.lastCallerAnchorPos = opts.anchorPos;
        state.keyboardIndex = -1;
    } else if (opts.anchorPos.has_value() &&
               state.lastCallerAnchorPos.has_value() &&
               *opts.anchorPos != *state.lastCallerAnchorPos) {
        // Caller explicitly moved anchorPos (e.g. re-triggered RightClick at a new coordinate)
        state.anchorPos = *opts.anchorPos;
        state.lastCallerAnchorPos = opts.anchorPos;
        state.keyboardIndex = -1;
        callerMovedAnchor = true;
    }

    const Rect popupRect = clampMenuRect(state.anchorPos, fromAnchorRect);
    ctx.OpenPopup(id, popupRect, opts.anchorRect);
    bool isOpen = true;
    res.rect = popupRect;

    // Build item rects & process hover / click / keyboard navigation
    struct RowEntry {
        Rect rect{};
        keel::usize itemIndex{0};
        bool hovered{false};
        bool separatorBelow{false};
        float sepY{0.0f};
    };
    keel::SmallArray<RowEntry, 16> rows;
    rows.reserve(items.size());

    float curY = popupRect.y + padY;
    keel::i32 hoveredIdx = -1;

    for (keel::usize i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        if (item.isSeparatorOnly) {
            RowEntry sepRow{};
            sepRow.itemIndex = i;
            sepRow.separatorBelow = true;
            sepRow.sepY = std::round(curY + sepH * 0.5f);
            rows.push_back(sepRow);
            curY += sepH;
            continue;
        }

        const Rect itemRect(popupRect.x + 4.0f * s, curY, popupRect.w - 8.0f * s, rowH);
        const bool isHov = !item.disabled && itemRect.Contains(mouse);
        if (isHov) {
            hoveredIdx = static_cast<keel::i32>(i);
            state.keyboardIndex = hoveredIdx;
        }

        RowEntry rEntry{};
        rEntry.rect = itemRect;
        rEntry.itemIndex = i;
        rEntry.hovered = isHov;
        curY += rowH;
        if (item.separatorAfter) {
            rEntry.separatorBelow = true;
            rEntry.sepY = std::round(curY + sepH * 0.5f);
            curY += sepH;
        }
        rows.push_back(rEntry);
    }

    // Keyboard navigation (Up / Down / Enter / Escape)
    auto stepSelectable = [&](keel::i32 dir) {
        const keel::i32 n = static_cast<keel::i32>(items.size());
        if (n <= 0) return;
        keel::i32 cur = state.keyboardIndex;
        for (keel::i32 step = 0; step < n; ++step) {
            cur = (cur < 0)
                ? (dir > 0 ? 0 : n - 1)
                : ((cur + dir + n) % n);
            if (!items[static_cast<keel::usize>(cur)].isSeparatorOnly &&
                !items[static_cast<keel::usize>(cur)].disabled) {
                state.keyboardIndex = cur;
                ctx.InvalidateAll();
                break;
            }
        }
    };

    if (ctx.ConsumeKey(Key::Down)) {
        stepSelectable(1);
    } else if (ctx.ConsumeKey(Key::Up)) {
        stepSelectable(-1);
    } else if (ctx.ConsumeKey(Key::Escape)) {
        ctx.ClosePopup();
        open = false;
        state.wasOpen = false;
        state.lastCallerAnchorPos.reset();
        res.isOpen = false;
        ctx.InvalidateAll();
        return res;
    } else if (ctx.ConsumeKey(Key::Enter) || ctx.ConsumeKey(Key::Space)) {
        if (state.keyboardIndex >= 0 && static_cast<keel::usize>(state.keyboardIndex) < items.size()) {
            const auto& activeItem = items[static_cast<keel::usize>(state.keyboardIndex)];
            if (!activeItem.isSeparatorOnly && !activeItem.disabled) {
                res.clickedIndex = state.keyboardIndex;
                res.clicked = true;
                res.changed = true;
                ctx.ClosePopup();
                open = false;
                state.wasOpen = false;
                state.lastCallerAnchorPos.reset();
                res.isOpen = false;
                ctx.InvalidateAll();
                return res;
            }
        }
    }

    if (hoveredIdx < 0 && state.keyboardIndex >= 0) {
        hoveredIdx = state.keyboardIndex;
        for (auto& r : rows) {
            if (static_cast<keel::i32>(r.itemIndex) == hoveredIdx) {
                r.hovered = true;
            }
        }
    }

    // Mouse click handling when already open
    {
        auto hitScope = ctx.ScopedOverlayHitCheck(wasPopupOpen ? id : Id::Null());
        if (wasPopupOpen && !callerMovedAnchor && (ctx.IsMouseClicked() || ctx.IsRightMouseClicked())) {
            const bool insidePopup = popupRect.Contains(mouse);
            const bool insideAnchor = (opts.anchorRect.w > 0.0f && opts.anchorRect.h > 0.0f) &&
                                      opts.anchorRect.Contains(mouse);
            if (insidePopup) {
                if (ctx.IsMouseClicked() && hoveredIdx >= 0 && static_cast<keel::usize>(hoveredIdx) < items.size()) {
                    const auto& clickedItem = items[static_cast<keel::usize>(hoveredIdx)];
                    if (!clickedItem.isSeparatorOnly && !clickedItem.disabled) {
                        res.clickedIndex = hoveredIdx;
                        res.clicked = true;
                        res.changed = true;
                        ctx.ClosePopup();
                        isOpen = false;
                        open = false;
                    }
                }
                ctx.ConsumeMouseClick();
                ctx.InvalidateAll();
            } else if (ctx.IsMouseClicked() && insideAnchor) {
                // Allow the anchor button's own click handler to toggle `open = false` cleanly on release.
            } else {
                ctx.ClosePopup();
                isOpen = false;
                open = false;
                ctx.ConsumeMouseClick();
                ctx.InvalidateAll();
            }
        } else if (callerMovedAnchor && (ctx.IsMouseClicked() || ctx.IsRightMouseClicked())) {
            ctx.ConsumeMouseClick();
            ctx.InvalidateAll();
        }
    }

    state.wasOpen = isOpen;
    if (!isOpen) {
        state.lastCallerAnchorPos.reset();
    }
    res.isOpen = isOpen;

    if (!isOpen) {
        return res;
    }

    // Copy items into an arena snapshot slice for the deferred overlay draw pass
    struct OverlayItemData {
        keel::StringRef label;
        keel::StringRef shortcut;
        Icon icon;
        UiVariant variant;
        bool checked;
        bool hasCheck;
        bool disabled;
        bool isSeparatorOnly;
        Rect rect;
        bool hovered;
        bool separatorBelow;
        float sepY;
    };
    OverlayItemData* arenaDrawItems = ctx.GetArena().create_array<OverlayItemData>(rows.size());
    for (keel::usize i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        const auto& src = items[r.itemIndex];
        arenaDrawItems[i] = OverlayItemData{
            .label = ctx.AllocString(src.label.text),
            .shortcut = ctx.AllocString(src.shortcut.text),
            .icon = ctx.AllocIcon(src.icon),
            .variant = src.variant,
            .checked = src.checked,
            .hasCheck = src.hasCheck,
            .disabled = src.disabled,
            .isSeparatorOnly = src.isSeparatorOnly,
            .rect = r.rect,
            .hovered = r.hovered,
            .separatorBelow = r.separatorBelow,
            .sepY = r.sepY
        };
    }
    const keel::ArrayRef<OverlayItemData> drawItems(arenaDrawItems, rows.size());

    const float radius = opts.border.radius.has_value()
        ? *opts.border.radius
        : theme.ResolveRadius(keel::kNullOpt, activeSize);

    ctx.SetOpenPopupVisualHash(detail::HashArgs(
        popupRect.x,
        popupRect.y,
        popupRect.w,
        popupRect.h,
        hoveredIdx,
        state.keyboardIndex,
        items.size(),
        activeSize
    ));

    ctx.RegisterOverlay([popupRect, radius, s, padX, icSz, iconSlotW, anyLeadingSlot, anyTrailingCheck, labelFont, shortcutFont, theme, opts, drawItems](ICanvas& c) {
        const auto& pal = theme.palette;

        // Soft drop shadow
        c.DrawRoundRect(
            Rect(popupRect.x - 1.0f * s, popupRect.y + 4.0f * s, popupRect.w + 2.0f * s, popupRect.h + 4.0f * s),
            radius + 2.0f * s,
            Color(0, 0, 0, 65),
            Color::Transparent(),
            0.0f
        );

        const Color defaultSurfaceBg = (theme.mode == ThemeMode::Dark)
            ? Color(19, 26, 41, 250)
            : Color(255, 255, 255, 252);
        const Color surfaceBg = opts.background.IsBackgroundEnabled(true)
            ? (!opts.background.HasAny()
                ? defaultSurfaceBg
                : (opts.background.IsSurface()
                    ? defaultSurfaceBg.ScaleAlpha(opts.background.alphaScale)
                    : theme.ResolveColor(opts.background, false, false, false, defaultSurfaceBg)))
            : Color::Transparent();
        const float borderW = (opts.border.width > 0.0f) ? opts.border.width : 0.0f;
        const Color borderCol = (borderW > 0.0f)
            ? theme.ResolveColor(opts.border.color, false, false, false, pal.borderHover)
            : Color::Transparent();
        c.DrawRoundRect(popupRect, radius, surfaceBg, borderCol, borderW);

        c.PushClip(popupRect);
        for (const auto& it : drawItems) {
            if (!it.isSeparatorOnly) {
                const bool isDanger = (it.variant == UiVariant::Danger);
                const Color dangerCol = theme.GetVariant(UiVariant::Danger).bg;
                const bool iconWithCheck = it.icon.IsValid() && (it.hasCheck || it.checked);

                if (it.hovered && !it.disabled) {
                    const Color hovBg = isDanger
                        ? dangerCol.WithAlphaF(theme.mode == ThemeMode::Dark ? 0.22f : 0.14f)
                        : (it.checked && iconWithCheck ? pal.surfaceActive : pal.surfaceHover);
                    c.DrawRoundRect(it.rect, keel::max(4.0f * s, radius - 2.0f * s), hovBg, Color::Transparent(), 0.0f);
                } else if (it.checked && iconWithCheck) {
                    c.DrawRoundRect(it.rect, keel::max(4.0f * s, radius - 2.0f * s), pal.surfaceHover, Color::Transparent(), 0.0f);
                }

                Color fg = isDanger ? dangerCol : (it.checked && iconWithCheck ? pal.accent : pal.textPrimary);
                Color mutedFg = pal.textSecondary;
                if (it.disabled) {
                    fg = fg.WithAlphaF(0.4f);
                    mutedFg = mutedFg.WithAlphaF(0.4f);
                }

                float textX = it.rect.x + padX - 2.0f * s;
                if (anyLeadingSlot) {
                    const Rect icRect(
                        textX + (iconSlotW - icSz) * 0.5f - 2.0f * s,
                        it.rect.y + (it.rect.h - icSz) * 0.5f,
                        icSz,
                        icSz
                    );
                    if (it.icon.IsValid()) {
                        const Color icCol = isDanger ? fg : (it.checked ? pal.accent : (it.hovered ? fg : mutedFg));
                        it.icon.Draw(c, icRect, icCol);
                    } else if (it.checked) {
                        Icon::Check().Draw(c, icRect, isDanger ? dangerCol : pal.accent);
                    }
                    textX += iconSlotW;
                }

                float rightPad = it.rect.Right() - padX + 2.0f * s;
                if (anyTrailingCheck) {
                    if (it.checked && iconWithCheck) {
                        const Rect chkRect(
                            rightPad - icSz,
                            it.rect.y + (it.rect.h - icSz) * 0.5f,
                            icSz,
                            icSz
                        );
                        Icon::Check().Draw(c, chkRect, isDanger ? dangerCol : pal.accent);
                    }
                    rightPad -= iconSlotW;
                }
                float availW = keel::max(10.0f, rightPad - textX);

                if (!it.shortcut.empty()) {
                    const float sw = c.MeasureText(it.shortcut, shortcutFont).x;
                    c.DrawTextBox(
                        it.shortcut,
                        Rect(rightPad - sw - 2.0f * s, it.rect.y, sw + 4.0f * s, it.rect.h),
                        shortcutFont,
                        mutedFg,
                        1.0f,
                        0.5f
                    );
                    availW = keel::max(10.0f, rightPad - sw - 12.0f * s - textX);
                }

                c.DrawTextBox(
                    it.label,
                    Rect(textX, it.rect.y, availW, it.rect.h),
                    labelFont,
                    fg,
                    0.0f,
                    0.5f
                );
            }

            if (it.separatorBelow) {
                c.DrawRect(
                    Rect(popupRect.x + 6.0f * s, it.sepY, keel::max(0.0f, popupRect.w - 12.0f * s), 1.0f),
                    pal.borderSubtle
                );
            }
        }
        c.PopClip();
    });

    return res;
}

} // namespace guikit
