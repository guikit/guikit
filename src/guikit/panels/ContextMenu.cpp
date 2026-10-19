#include "guikit/panels/ContextMenu.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

struct ContextMenuPersistState {
    Vec2 anchorPos{0.0f, 0.0f};
    int keyboardIndex{-1};
    bool wasExternalOpen{false};
};

} // namespace

ContextMenuResponse ContextMenu::Run(
    Context& ctx,
    bool* externalOpen,
    std::optional<Vec2> explicitAnchorPos,
    Rect triggerRect,
    std::span<const ContextMenuItem> items,
    const ContextMenuOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const UiTheme theme = ctx.GetTheme();
    if (pushedTheme != nullptr) {
        ctx.PopTheme();
    }

    auto& state = ctx.GetOrCreateState<ContextMenuPersistState>(id);
    const bool wasPopupOpen = ctx.IsPopupOpen(id);

    ContextMenuResponse res{};
    res.id = id;
    res.rect = triggerRect;
    res.ctx = &ctx;
    res.clickedIndex = -1;

    if (effectiveDisabled || items.empty()) {
        if (wasPopupOpen) {
            ctx.ClosePopup();
        }
        if (externalOpen) {
            *externalOpen = false;
        }
        state.wasExternalOpen = false;
        res.isOpen = false;
        return res;
    }

    // Measure menu geometry
    ICanvas* canvas = ctx.GetCanvas();
    const float s = (theme.scale > 0.0f) ? theme.scale : activeScale;
    const float rowH = std::round(30.0f * s);
    const float sepH = std::round(9.0f * s);
    const float padX = std::round(10.0f * s);
    const float padY = std::round(6.0f * s);
    const float iconSlotW = std::round(22.0f * s);
    const float shortcutGap = std::round(24.0f * s);
    const float labelFont = theme.ResolveFontSize(UiText{}, TextRole::LabelMedium, activeSize);
    const float shortcutFont = theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, activeSize);

    bool anyLeadingSlot = false;
    for (const auto& item : items) {
        if (!item.isSeparatorOnly && (item.icon.IsValid() || item.hasCheck || item.checked)) {
            anyLeadingSlot = true;
            break;
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
        maxContentW = std::max(maxContentW, leadW + lw + sw);
    }

    float menuW = maxContentW + padX * 2.0f + 8.0f * s;
    if (opts.minWidth.has_value()) {
        menuW = std::max(menuW, theme.Scale(*opts.minWidth));
    }
    if (opts.width.has_value() && *opts.width > 0.0f) {
        menuW = theme.Scale(*opts.width);
    }
    const float menuH = totalH;

    const Vec2 vp = ctx.GetViewportSize() / ((ctx.GetDpiScale() > 0.0f) ? ctx.GetDpiScale() : 1.0f);
    auto clampMenuRect = [&](Vec2 anchor, bool fromTriggerBox) -> Rect {
        float x = anchor.x;
        float y = anchor.y;
        if (vp.x > 0.0f && x + menuW > vp.x - 6.0f) {
            if (fromTriggerBox && triggerRect.w > 0.0f) {
                x = triggerRect.Right() - menuW;
            } else {
                x = anchor.x - menuW;
            }
            x = std::clamp(x, 6.0f, std::max(6.0f, vp.x - menuW - 6.0f));
        }
        if (vp.y > 0.0f && y + menuH > vp.y - 6.0f) {
            if (fromTriggerBox && triggerRect.h > 0.0f && triggerRect.y - menuH - 4.0f >= 6.0f) {
                y = triggerRect.y - menuH - 4.0f;
            } else {
                y = std::clamp(vp.y - menuH - 6.0f, 6.0f, std::max(6.0f, vp.y - 6.0f));
            }
        }
        return Rect(std::round(x), std::round(y), std::round(menuW), std::round(menuH));
    };

    const Vec2 mouse = ctx.GetMousePos();
    bool isOpen = wasPopupOpen;

    // Synchronize if externalOpen was closed by BeginFrame outside click
    if (externalOpen != nullptr) {
        if (*externalOpen && !wasPopupOpen && !state.wasExternalOpen) {
            state.anchorPos = explicitAnchorPos.value_or(
                (triggerRect.w > 0.0f || triggerRect.h > 0.0f)
                    ? Vec2{triggerRect.x, triggerRect.Bottom() + 4.0f}
                    : mouse
            );
            const Rect popupRect = clampMenuRect(state.anchorPos, explicitAnchorPos == std::nullopt && triggerRect.h > 0.0f);
            ctx.OpenPopup(id, popupRect);
            state.keyboardIndex = -1;
            isOpen = true;
        } else if (!*externalOpen && wasPopupOpen) {
            ctx.ClosePopup();
            isOpen = false;
        } else if (*externalOpen && !wasPopupOpen && state.wasExternalOpen) {
            // Popup was dismissed by clicking outside in BeginFrame
            *externalOpen = false;
            isOpen = false;
        }
    }

    // Check trigger interaction
    const bool overTrigger = (triggerRect.w > 0.0f && triggerRect.h > 0.0f) && triggerRect.Contains(mouse);
    const bool occludedByOtherPopup = ctx.GetOpenPopupId().IsValid() && ctx.GetOpenPopupId() != id && ctx.GetOpenPopupRect().Contains(mouse);
    const bool occludedByModal = ctx.IsModalBlocking(mouse);

    if (!occludedByOtherPopup && !occludedByModal) {
        if (opts.trigger == ContextMenuTrigger::RightClick && overTrigger && ctx.IsRightMouseClicked()) {
            state.anchorPos = explicitAnchorPos.value_or(mouse);
            const Rect popupRect = clampMenuRect(state.anchorPos, false);
            ctx.OpenPopup(id, popupRect);
            state.keyboardIndex = -1;
            isOpen = true;
            if (externalOpen) *externalOpen = true;
            ctx.ConsumeMouseClick();
            ctx.InvalidateAll();
        } else if (opts.trigger == ContextMenuTrigger::LeftClick && overTrigger && ctx.IsMouseClicked()) {
            if (wasPopupOpen) {
                ctx.ClosePopup();
                isOpen = false;
                if (externalOpen) *externalOpen = false;
            } else {
                state.anchorPos = explicitAnchorPos.value_or(Vec2{triggerRect.x, triggerRect.Bottom() + 4.0f});
                const Rect popupRect = clampMenuRect(state.anchorPos, !explicitAnchorPos.has_value());
                ctx.OpenPopup(id, popupRect);
                state.keyboardIndex = -1;
                isOpen = true;
                if (externalOpen) *externalOpen = true;
            }
            ctx.ConsumeMouseClick();
            ctx.InvalidateAll();
        }
    }

    if (!isOpen) {
        state.wasExternalOpen = (externalOpen != nullptr) ? *externalOpen : false;
        res.isOpen = false;
        return res;
    }

    const Rect popupRect = clampMenuRect(
        state.anchorPos,
        opts.trigger == ContextMenuTrigger::LeftClick && !explicitAnchorPos.has_value()
    );
    ctx.OpenPopup(id, popupRect);
    res.rect = popupRect;

    // Build item rects & process hover / click / keyboard navigation
    struct RowEntry {
        Rect rect{};
        size_t itemIndex{0};
        bool hovered{false};
        bool separatorBelow{false};
        float sepY{0.0f};
    };
    std::vector<RowEntry> rows;
    rows.reserve(items.size());

    float curY = popupRect.y + padY;
    int hoveredIdx = -1;

    for (size_t i = 0; i < items.size(); ++i) {
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
            hoveredIdx = static_cast<int>(i);
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
    auto stepSelectable = [&](int dir) {
        const int n = static_cast<int>(items.size());
        if (n <= 0) return;
        int cur = state.keyboardIndex;
        for (int step = 0; step < n; ++step) {
            cur = (cur < 0)
                ? (dir > 0 ? 0 : n - 1)
                : ((cur + dir + n) % n);
            if (!items[static_cast<size_t>(cur)].isSeparatorOnly &&
                !items[static_cast<size_t>(cur)].disabled) {
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
        isOpen = false;
        if (externalOpen) *externalOpen = false;
        state.wasExternalOpen = false;
        res.isOpen = false;
        ctx.InvalidateAll();
        return res;
    } else if (ctx.ConsumeKey(Key::Enter) || ctx.ConsumeKey(Key::Space)) {
        if (state.keyboardIndex >= 0 && static_cast<size_t>(state.keyboardIndex) < items.size()) {
            const auto& activeItem = items[static_cast<size_t>(state.keyboardIndex)];
            if (!activeItem.isSeparatorOnly && !activeItem.disabled) {
                res.clickedIndex = state.keyboardIndex;
                res.clicked = true;
                res.changed = true;
                ctx.ClosePopup();
                isOpen = false;
                if (externalOpen) *externalOpen = false;
                state.wasExternalOpen = false;
                res.isOpen = false;
                ctx.InvalidateAll();
                return res;
            }
        }
    }

    if (hoveredIdx < 0 && state.keyboardIndex >= 0) {
        hoveredIdx = state.keyboardIndex;
        for (auto& r : rows) {
            if (static_cast<int>(r.itemIndex) == hoveredIdx) {
                r.hovered = true;
            }
        }
    }

    // Mouse click handling when already open
    if (wasPopupOpen && (ctx.IsMouseClicked() || ctx.IsRightMouseClicked())) {
        if (popupRect.Contains(mouse)) {
            if (ctx.IsMouseClicked() && hoveredIdx >= 0 && static_cast<size_t>(hoveredIdx) < items.size()) {
                const auto& clickedItem = items[static_cast<size_t>(hoveredIdx)];
                if (!clickedItem.isSeparatorOnly && !clickedItem.disabled) {
                    res.clickedIndex = hoveredIdx;
                    res.clicked = true;
                    res.changed = true;
                    ctx.ClosePopup();
                    isOpen = false;
                    if (externalOpen) *externalOpen = false;
                }
            }
            ctx.ConsumeMouseClick();
            ctx.InvalidateAll();
        } else if (ctx.IsRightMouseClicked() && overTrigger && opts.trigger == ContextMenuTrigger::RightClick) {
            // Reposition context menu to new right-click coordinates inside trigger
            state.anchorPos = explicitAnchorPos.value_or(mouse);
            const Rect newRect = clampMenuRect(state.anchorPos, false);
            ctx.OpenPopup(id, newRect);
            ctx.ConsumeMouseClick();
            ctx.InvalidateAll();
        } else {
            ctx.ClosePopup();
            isOpen = false;
            if (externalOpen) *externalOpen = false;
            ctx.ConsumeMouseClick();
            ctx.InvalidateAll();
        }
    }

    state.wasExternalOpen = isOpen && (externalOpen != nullptr ? *externalOpen : true);
    res.isOpen = isOpen;

    if (!isOpen) {
        return res;
    }

    // Copy items into a snapshot vector for the deferred overlay draw pass
    struct OverlayItemData {
        std::string label;
        std::string shortcut;
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
    std::vector<OverlayItemData> drawItems;
    drawItems.reserve(rows.size());
    for (const auto& r : rows) {
        const auto& src = items[r.itemIndex];
        drawItems.push_back(OverlayItemData{
            .label = std::string(src.label.text),
            .shortcut = std::string(src.shortcut.text),
            .icon = src.icon,
            .variant = src.variant,
            .checked = src.checked,
            .hasCheck = src.hasCheck,
            .disabled = src.disabled,
            .isSeparatorOnly = src.isSeparatorOnly,
            .rect = r.rect,
            .hovered = r.hovered,
            .separatorBelow = r.separatorBelow,
            .sepY = r.sepY
        });
    }

    const float radius = opts.borderRadius.has_value()
        ? theme.Scale(*opts.borderRadius)
        : theme.ResolveRadius(std::nullopt, activeSize);

    ctx.RegisterOverlay([popupRect, radius, s, padX, iconSlotW, anyLeadingSlot, labelFont, shortcutFont, theme, drawItems = std::move(drawItems)](ICanvas& c) {
        const auto& pal = theme.palette;

        // Soft drop shadow
        c.DrawRoundRect(
            Rect(popupRect.x - 1.0f * s, popupRect.y + 4.0f * s, popupRect.w + 2.0f * s, popupRect.h + 4.0f * s),
            radius + 2.0f * s,
            Color(0, 0, 0, 65),
            Color::Transparent(),
            0.0f
        );

        const Color surfaceBg = (theme.mode == ThemeMode::Dark)
            ? Color(19, 26, 41, 250)
            : Color(255, 255, 255, 252);
        c.DrawRoundRect(popupRect, radius, surfaceBg, pal.borderHover, 1.0f);

        c.PushClip(popupRect);
        for (const auto& it : drawItems) {
            if (!it.isSeparatorOnly) {
                const bool isDanger = (it.variant == UiVariant::Danger);
                const Color dangerCol = theme.GetVariant(UiVariant::Danger).bg;

                if (it.hovered && !it.disabled) {
                    const Color hovBg = isDanger
                        ? dangerCol.WithAlphaF(theme.mode == ThemeMode::Dark ? 0.22f : 0.14f)
                        : pal.surfaceHover;
                    c.DrawRoundRect(it.rect, std::max(4.0f * s, radius - 2.0f * s), hovBg, Color::Transparent(), 0.0f);
                }

                Color fg = isDanger ? dangerCol : pal.textPrimary;
                Color mutedFg = pal.textMuted;
                if (it.disabled) {
                    fg = fg.WithAlphaF(0.4f);
                    mutedFg = mutedFg.WithAlphaF(0.4f);
                }

                float textX = it.rect.x + padX - 2.0f * s;
                if (anyLeadingSlot) {
                    const float icSz = 14.0f * s;
                    const Rect icRect(
                        textX + (iconSlotW - icSz) * 0.5f - 2.0f * s,
                        it.rect.y + (it.rect.h - icSz) * 0.5f,
                        icSz,
                        icSz
                    );
                    if (it.checked) {
                        Icon::Check().Draw(c, icRect, isDanger ? dangerCol : pal.accent);
                    } else if (it.icon.IsValid()) {
                        it.icon.Draw(c, icRect, isDanger ? fg : (it.hovered ? fg : mutedFg));
                    }
                    textX += iconSlotW;
                }

                const float rightPad = it.rect.Right() - padX + 2.0f * s;
                float availW = std::max(10.0f, rightPad - textX);

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
                    availW = std::max(10.0f, rightPad - sw - 12.0f * s - textX);
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
                    Rect(popupRect.x + 6.0f * s, it.sepY, std::max(0.0f, popupRect.w - 12.0f * s), 1.0f),
                    pal.borderSubtle
                );
            }
        }
        c.PopClip();
    });

    return res;
}

} // namespace guikit
