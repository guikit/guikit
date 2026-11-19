#include "guikit/widgets/breadcrumb.hpp"
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/math.h"
#include "keel/small_array.h"

namespace guikit {

namespace {

struct BreadcrumbPersistentState {
    keel::i32 highlightedPopupRow{-1};
    float popupScrollY{0.0f};
};

struct BreadcrumbToken {
    keel::usize itemIndex{0};
    bool isEllipsis{false};
};

ControlSpec EffectiveBreadcrumbControl(const BreadcrumbOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& sm = theme.GetSelect(size);
    return Control::FromLayout(
        opts.layout,
        SizeMode::Fit,
        SizeMode::Fit,
        ElementLayout::Stacked,
        sm.labelGap,
        sm.descGap,
        sm.paddingX
    );
}

void BuildBreadcrumbTokens(
    keel::ArrayRef<BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    keel::SmallArray<BreadcrumbToken, 16>& outTokens
) {
    outTokens.clear();
    if (items.empty()) return;

    const bool shouldCollapse = (opts.maxVisibleItems >= 2) &&
                                (items.size() > static_cast<keel::usize>(opts.maxVisibleItems));

    if (!shouldCollapse) {
        outTokens.reserve(items.size());
        for (keel::usize i = 0; i < items.size(); ++i) {
            outTokens.push_back(BreadcrumbToken{.itemIndex = i, .isEllipsis = false});
        }
        return;
    }

    const keel::usize trailingCount = static_cast<keel::usize>(opts.maxVisibleItems - 1);
    outTokens.reserve(1 + 1 + trailingCount);
    outTokens.push_back(BreadcrumbToken{.itemIndex = 0, .isEllipsis = false});
    outTokens.push_back(BreadcrumbToken{.itemIndex = 1, .isEllipsis = true});

    const keel::usize startTrailing = items.size() - trailingCount;
    for (keel::usize i = startTrailing; i < items.size(); ++i) {
        outTokens.push_back(BreadcrumbToken{.itemIndex = i, .isEllipsis = false});
    }
}

float MeasureTokenNaturalWidth(
    keel::ArrayRef<BreadcrumbItem> items,
    const BreadcrumbToken& token,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const float padX = keel::max(6.0f, cm.paddingX * 0.65f);

    if (token.isEllipsis) {
        const float dotsW = TextUtil::MeasureWidth("...", fontSize, canvas);
        return keel::max(26.0f, dotsW + padX * 1.8f);
    }

    if (token.itemIndex >= items.size()) {
        return padX * 2.0f;
    }

    const BreadcrumbItem& item = items[token.itemIndex];
    const float iconGap = 5.0f;
    const float badgeGap = 5.0f;
    const float badgeFontSize = fontSize * 0.82f;

    float contentW = 0.0f;
    if (item.icon.IsValid()) {
        contentW += cm.iconSize;
    }
    if (!item.label.empty()) {
        if (contentW > 0.0f) contentW += iconGap;
        contentW += TextUtil::MeasureWidth(item.label, fontSize, canvas);
    }
    if (!item.badge.empty()) {
        if (contentW > 0.0f) contentW += badgeGap;
        contentW += TextUtil::MeasureWidth(item.badge, badgeFontSize, canvas) + 10.0f;
    }

    return keel::max(cm.defaultHeight * 0.75f, contentW + padX * 2.0f);
}

float MeasureSeparatorSlotWidth(UiSize size, const UiTheme& theme) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    return keel::max(14.0f, cm.iconSize * 1.05f);
}

Vec2 MeasureBreadcrumbNaturalBox(
    keel::ArrayRef<BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    keel::SmallArray<BreadcrumbToken, 16> tokens;
    BuildBreadcrumbTokens(items, opts, tokens);

    const float sepW = MeasureSeparatorSlotWidth(size, theme);
    float totalW = 0.0f;
    for (keel::usize i = 0; i < tokens.size(); ++i) {
        if (i > 0) {
            totalW += sepW;
        }
        totalW += MeasureTokenNaturalWidth(items, tokens[i], size, canvas, theme);
    }

    if (opts.layout.width.IsFixed()) {
        totalW = opts.layout.width.Dp();
    }

    const float boxH = opts.layout.height.IsFixed()
        ? opts.layout.height.Dp()
        : cm.defaultHeight;

    return {keel::max(24.0f, totalW), boxH};
}

void ComputeBreadcrumbLayout(
    Rect innerRect,
    keel::ArrayRef<BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::SmallArray<BreadcrumbToken, 16>& outTokens,
    keel::SmallArray<Rect, 16>& outTokenRects,
    keel::SmallArray<Rect, 16>& outSepRects
) {
    BuildBreadcrumbTokens(items, opts, outTokens);
    outTokenRects.clear();
    outSepRects.clear();
    if (outTokens.empty()) return;

    outTokenRects.reserve(outTokens.size());
    outSepRects.reserve(outTokens.size() > 1 ? outTokens.size() - 1 : 0);

    const float sepW = MeasureSeparatorSlotWidth(size, theme);
    keel::SmallArray<float, 16> widths(outTokens.size(), 0.0f);
    float totalW = 0.0f;
    for (keel::usize i = 0; i < outTokens.size(); ++i) {
        if (i > 0) totalW += sepW;
        widths[i] = MeasureTokenNaturalWidth(items, outTokens[i], size, canvas, theme);
        totalW += widths[i];
    }

    // Responsive auto-collapse when available innerRect width is narrower than natural trail width
    bool hasEllipsis = false;
    for (const BreadcrumbToken& t : outTokens) {
        if (t.isEllipsis) {
            hasEllipsis = true;
            break;
        }
    }
    if (!hasEllipsis && outTokens.size() >= 3 && innerRect.w > 1.0f && totalW > innerRect.w + 0.5f) {
        BreadcrumbOptions collapsedOpts = opts;
        for (keel::u32 maxVis = static_cast<keel::u32>(outTokens.size() - 1); maxVis >= 2; --maxVis) {
            collapsedOpts.maxVisibleItems = maxVis;
            BuildBreadcrumbTokens(items, collapsedOpts, outTokens);
            widths.assign(outTokens.size(), 0.0f);
            totalW = 0.0f;
            for (keel::usize i = 0; i < outTokens.size(); ++i) {
                if (i > 0) totalW += sepW;
                widths[i] = MeasureTokenNaturalWidth(items, outTokens[i], size, canvas, theme);
                totalW += widths[i];
            }
            if (totalW <= innerRect.w + 0.5f) {
                break;
            }
        }
    }

    float effSepW = sepW;
    if (innerRect.w > 1.0f && totalW > innerRect.w + 0.5f && !outTokens.empty()) {
        const float totalSepW = (outTokens.size() > 1) ? (static_cast<float>(outTokens.size() - 1) * sepW) : 0.0f;
        const float sumTokW = keel::max(1.0f, totalW - totalSepW);
        if (innerRect.w > totalSepW + static_cast<float>(outTokens.size()) * 18.0f) {
            const float availTokW = innerRect.w - totalSepW;
            const float ratio = availTokW / sumTokW;
            for (float& w : widths) {
                w *= ratio;
            }
        } else {
            const float ratio = innerRect.w / keel::max(1.0f, totalW);
            effSepW *= ratio;
            for (float& w : widths) {
                w *= ratio;
            }
        }
        totalW = innerRect.w;
    }

    float curX = innerRect.x;
    for (keel::usize i = 0; i < outTokens.size(); ++i) {
        if (i > 0) {
            outSepRects.emplace_back(curX, innerRect.y, effSepW, innerRect.h);
            curX += effSepW;
        }
        outTokenRects.emplace_back(curX, innerRect.y, widths[i], innerRect.h);
        curX += widths[i];
    }
}

void DrawSeparatorGlyph(
    ICanvas& canvas,
    Rect sepRect,
    BreadcrumbSeparator separator,
    UiSize size,
    const UiTheme& theme,
    Color color
) {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const float iconSz = keel::round(cm.iconSize * 0.78f);
    const Vec2 center = sepRect.Center();

    switch (separator) {
        case BreadcrumbSeparator::Chevron: {
            const Rect iconR(center.x - iconSz * 0.5f, center.y - iconSz * 0.5f, iconSz, iconSz);
            Icon::ChevronRight().Draw(canvas, iconR, color);
            break;
        }
        case BreadcrumbSeparator::Arrow: {
            const Rect iconR(center.x - iconSz * 0.5f, center.y - iconSz * 0.5f, iconSz, iconSz);
            Icon::ArrowRight().Draw(canvas, iconR, color);
            break;
        }
        case BreadcrumbSeparator::Slash: {
            canvas.DrawTextBox("/", sepRect, fontSize, color, 0.5f, 0.5f);
            break;
        }
        case BreadcrumbSeparator::Dot: {
            const float r = 2.2f;
            canvas.DrawRoundRect(Rect(center.x - r, center.y - r, r * 2.0f, r * 2.0f), r, color);
            break;
        }
    }
}

void RenderBreadcrumbPopup(
    ICanvas& canvas,
    Rect popupRect,
    keel::ArrayRef<BreadcrumbItem> collapsedItems,
    keel::i32 activeRow,
    keel::i32 hoveredRow,
    const BreadcrumbOptions& opts,
    UiSize size,
    const UiTheme& theme,
    float scrollY
) {
    if (popupRect.w <= 0.0f || popupRect.h <= 0.0f || collapsedItems.empty()) {
        return;
    }

    const SelectMetrics& sm = theme.GetSelect(size);
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const auto& pal = theme.palette;

    // Elevated dropdown card with subtle shadow and border
    canvas.DrawRoundRect(Rect(popupRect.x, popupRect.y + 2.0f, popupRect.w, popupRect.h), sm.cornerRadius + 1.0f, Color(0, 0, 0, 56));
    canvas.DrawRoundRect(popupRect, sm.cornerRadius, pal.surfaceElevated, pal.border, 1.0f);

    const float viewportH = keel::max(0.0f, popupRect.h - 8.0f);
    const float totalContentH = static_cast<float>(collapsedItems.size()) * sm.itemHeight;
    const float maxScrollY = keel::max(0.0f, totalContentH - viewportH);
    const float clampedScrollY = keel::clamp(scrollY, 0.0f, maxScrollY);
    const float itemW = keel::max(0.0f, popupRect.w - 8.0f);
    const Rect itemsClipRect(popupRect.x + 1.0f, popupRect.y + 4.0f, keel::max(0.0f, popupRect.w - 2.0f), viewportH);

    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const float badgeFontSize = fontSize * 0.82f;
    const float iconGap = 6.0f;
    const float badgeGap = 8.0f;

    canvas.PushClip(itemsClipRect);

    for (keel::usize i = 0; i < collapsedItems.size(); ++i) {
        const BreadcrumbItem& item = collapsedItems[i];
        const Rect itemRect(
            popupRect.x + 4.0f,
            popupRect.y + 4.0f + static_cast<float>(i) * sm.itemHeight - clampedScrollY,
            itemW,
            sm.itemHeight
        );
        if (itemRect.Bottom() <= itemsClipRect.y || itemRect.y >= itemsClipRect.Bottom()) {
            continue;
        }

        const bool isSelected = (static_cast<keel::i32>(i) == activeRow);
        const bool isHovered = (static_cast<keel::i32>(i) == hoveredRow);
        const Color accentColor = opts.style.color.HasAny()
            ? opts.style.color.Resolve(isHovered, false, false, pal.accent)
            : pal.accent;

        if (isSelected) {
            const Color selFill = accentColor.WithAlphaF(isHovered ? 0.22f : 0.14f);
            canvas.DrawRoundRect(itemRect, 4.0f, selFill);
        } else if (isHovered) {
            canvas.DrawRoundRect(itemRect, 4.0f, pal.surfaceHover);
        }

        const Color fgColor = isSelected
            ? accentColor
            : (isHovered ? pal.textPrimary : pal.textSecondary);

        float leftX = itemRect.x + sm.paddingX;
        float rightX = itemRect.Right() - sm.paddingX;
        const float centerY = itemRect.y + itemRect.h * 0.5f;

        if (isSelected) {
            const float checkSz = cm.iconSize;
            rightX -= checkSz;
            const Rect checkRect(rightX, centerY - checkSz * 0.5f, checkSz, checkSz);
            Icon::Check().Draw(canvas, checkRect, accentColor);
            rightX -= 6.0f;
        }

        if (!item.badge.empty()) {
            const float badgeW = canvas.MeasureText(item.badge, badgeFontSize).x + 10.0f;
            const float badgeH = badgeFontSize + 5.0f;
            rightX -= badgeW;
            const Rect badgeR(rightX, centerY - badgeH * 0.5f, badgeW, badgeH);
            const Color badgeBg = isSelected ? accentColor.WithAlphaF(0.25f) : pal.surfaceHover;
            const Color badgeFg = isSelected ? accentColor : pal.textSecondary;
            canvas.DrawRoundRect(badgeR, 4.0f, badgeBg);
            canvas.DrawTextBox(item.badge, badgeR, badgeFontSize, badgeFg, 0.5f, 0.5f);
            rightX -= badgeGap;
        }

        if (item.icon.IsValid()) {
            const Rect iconR(leftX, centerY - cm.iconSize * 0.5f, cm.iconSize, cm.iconSize);
            item.icon.Draw(canvas, iconR, isSelected ? accentColor : fgColor);
            leftX += cm.iconSize + iconGap;
        }

        if (!item.label.empty() && rightX > leftX) {
            const Rect textR(leftX, itemRect.y, rightX - leftX, itemRect.h);
            canvas.DrawTextBox(item.label, textR, fontSize, isSelected ? accentColor : pal.textPrimary, 0.0f, 0.5f);
        }
    }

    canvas.PopClip();
}

} // namespace

Vec2 Breadcrumb::Measure(
    keel::ArrayRef<BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const Vec2 boxDims = MeasureBreadcrumbNaturalBox(items, opts, size, canvas, theme);
    const ControlSpec ctrl = EffectiveBreadcrumbControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, theme);
}

Response Breadcrumb::Run(
    Context& ctx,
    keel::usize& activeIndex,
    keel::ArrayRef<keel::StringRef> items,
    const BreadcrumbOptions& opts,
    keel::SourceLocation loc
) {
    BreadcrumbItem* richItems = ctx.GetArena().create_array<BreadcrumbItem>(items.size());
    for (keel::usize i = 0; i < items.size(); ++i) {
        richItems[i].label = ctx.String(items[i]);
    }
    return Run(ctx, activeIndex, keel::ArrayRef<BreadcrumbItem>(richItems, items.size()), opts, loc);
}

Response Breadcrumb::Run(
    Context& ctx,
    keel::i32& activeIndex,
    keel::ArrayRef<keel::StringRef> items,
    const BreadcrumbOptions& opts,
    keel::SourceLocation loc
) {
    keel::usize idx = (activeIndex >= 0) ? static_cast<keel::usize>(activeIndex) : 0;
    Response res = Run(ctx, idx, items, opts, loc);
    if (res.changed) {
        activeIndex = static_cast<keel::i32>(idx);
    }
    return res;
}

Response Breadcrumb::Run(
    Context& ctx,
    keel::i32& activeIndex,
    keel::ArrayRef<BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    keel::SourceLocation loc
) {
    keel::usize idx = (activeIndex >= 0) ? static_cast<keel::usize>(activeIndex) : 0;
    Response res = Run(ctx, idx, items, opts, loc);
    if (res.changed) {
        activeIndex = static_cast<keel::i32>(idx);
    }
    return res;
}

Response Breadcrumb::Run(
    Context& ctx,
    keel::usize& activeIndex,
    keel::ArrayRef<BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.style.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();

    auto& state = ctx.GetOrCreatePersistentState<BreadcrumbPersistentState>(id);
    const bool wasOpen = ctx.IsPopupOpen(id);

    BreadcrumbOptions optsCopy = opts;

    ctx.RegisterFocusable(id, effectiveDisabled);
    Response res = ctx.Interact(id);

    keel::usize currIndex = items.empty() ? 0 : keel::min(activeIndex, items.size() - 1);

    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    const Rect innerBounds = prevBounds;

    keel::SmallArray<BreadcrumbToken, 16> tokens;
    keel::SmallArray<Rect, 16> tokenRects;
    keel::SmallArray<Rect, 16> sepRects;
    ComputeBreadcrumbLayout(innerBounds, items, optsCopy, activeSize, canvas, theme, tokens, tokenRects, sepRects);

    keel::i32 ellipsisTokenIdx = -1;
    keel::SmallArray<keel::usize, 16> collapsedIndices;
    for (keel::usize i = 0; i < tokens.size(); ++i) {
        if (tokens[i].isEllipsis) {
            ellipsisTokenIdx = static_cast<keel::i32>(i);
            const keel::usize startCollapsed = (i > 0) ? (tokens[i - 1].itemIndex + 1) : 0;
            const keel::usize endCollapsed = (i + 1 < tokens.size()) ? tokens[i + 1].itemIndex : items.size();
            for (keel::usize idx = startCollapsed; idx < endCollapsed && idx < items.size(); ++idx) {
                collapsedIndices.push_back(idx);
            }
            break;
        }
    }

    keel::i32 hoveredToken = -1;
    keel::i32 pressedToken = -1;
    bool isOpen = wasOpen;
    bool wantOpen = false;
    bool wantClose = false;

    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
        if (wasOpen) {
            wantClose = true;
        }
    } else {
        if (wasOpen && (ellipsisTokenIdx < 0 || collapsedIndices.empty())) {
            wantClose = true;
        }

        const Vec2 mouse = ctx.GetMousePos();
        if (res.hovered || res.pressed || res.clicked) {
            for (keel::usize i = 0; i < tokenRects.size(); ++i) {
                if (tokenRects[i].Contains(mouse)) {
                    hoveredToken = static_cast<keel::i32>(i);
                    if (res.pressed) {
                        pressedToken = static_cast<keel::i32>(i);
                    }
                    break;
                }
            }
        }

        if (res.clicked) {
            ctx.RequestFocus(id);
            if (hoveredToken >= 0 && static_cast<keel::usize>(hoveredToken) < tokens.size()) {
                const BreadcrumbToken& tok = tokens[static_cast<keel::usize>(hoveredToken)];
                if (tok.isEllipsis) {
                    if (wasOpen) {
                        wantClose = true;
                    } else if (!collapsedIndices.empty()) {
                        wantOpen = true;
                    }
                    ctx.RequestRepaint(3);
                } else if (tok.itemIndex < items.size()) {
                    if (wasOpen) {
                        wantClose = true;
                    }
                    if (currIndex != tok.itemIndex) {
                        currIndex = tok.itemIndex;
                        activeIndex = currIndex;
                        res.changed = true;
                    }
                    ctx.RequestRepaint(3);
                }
            } else if (wasOpen) {
                wantClose = true;
            }
        }
    }

    if (wantClose) {
        ctx.ClosePopup();
        isOpen = false;
        state.highlightedPopupRow = -1;
    } else if (wantOpen && !collapsedIndices.empty()) {
        isOpen = true;
        state.popupScrollY = 0.0f;
        state.highlightedPopupRow = 0;
        for (keel::usize r = 0; r < collapsedIndices.size(); ++r) {
            if (collapsedIndices[r] == currIndex) {
                state.highlightedPopupRow = static_cast<keel::i32>(r);
                break;
            }
        }
    }

    const SelectMetrics& sm = theme.GetSelect(activeSize);
    const ComponentMetrics& cm = theme.GetMetrics(activeSize);
    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, activeSize);
    const float badgeFontSize = fontSize * 0.82f;
    const float iconGap = 6.0f;
    const float badgeGap = 8.0f;

    if (isOpen && !collapsedIndices.empty()) {
        float maxRowW = 140.0f;
        for (keel::usize origIdx : collapsedIndices) {
            const BreadcrumbItem& item = items[origIdx];
            float w = sm.paddingX * 2.0f;
            if (item.icon.IsValid()) {
                w += cm.iconSize + iconGap;
            }
            if (!item.label.empty()) {
                w += TextUtil::MeasureWidth(item.label, fontSize, canvas);
            }
            if (!item.badge.empty()) {
                w += badgeGap + TextUtil::MeasureWidth(item.badge, badgeFontSize, canvas) + 10.0f;
            }
            if (origIdx == currIndex) {
                w += cm.iconSize + 8.0f;
            }
            maxRowW = keel::max(maxRowW, w);
        }

        const float popupW = maxRowW + 8.0f;
        const keel::usize visibleRows = keel::min<keel::usize>(collapsedIndices.size(), 8);
        float popupH = static_cast<float>(visibleRows) * sm.itemHeight + 8.0f;
        const Rect anchorRect = (ellipsisTokenIdx >= 0 && static_cast<keel::usize>(ellipsisTokenIdx) < tokenRects.size())
            ? tokenRects[static_cast<keel::usize>(ellipsisTokenIdx)]
            : prevBounds;
        float popupX = anchorRect.x;
        float popupY = anchorRect.Bottom() + 4.0f;

        const Rect vp = ctx.GetPopupViewportRect();
        if (vp.h > 0.0f) {
            const float availDown = vp.Bottom() - popupY - 6.0f;
            const float availUp = anchorRect.y - 4.0f - (vp.y + 6.0f);
            if (popupY + popupH > vp.Bottom() - 6.0f && availUp > availDown) {
                popupY = keel::max(vp.y + 6.0f, anchorRect.y - 4.0f - popupH);
                popupH = keel::clamp(anchorRect.y - 4.0f - popupY, keel::min(sm.itemHeight + 8.0f, vp.h - 8.0f), popupH);
            } else if (popupY + popupH > vp.Bottom() - 6.0f) {
                popupH = keel::max(keel::min(sm.itemHeight + 8.0f, vp.h - 8.0f), availDown);
                popupY = keel::clamp(popupY, vp.y + 4.0f, keel::max(vp.y + 4.0f, vp.Bottom() - popupH - 4.0f));
            }
        }
        if (vp.w > 0.0f) {
            popupX = keel::clamp(popupX, vp.x + 4.0f, keel::max(vp.x + 4.0f, vp.Right() - popupW - 4.0f));
        }

        ctx.OpenPopup(id, Rect(popupX, popupY, popupW, popupH));
    } else if (isOpen) {
        ctx.ClosePopup();
        isOpen = false;
    }

    keel::i32 hoveredPopupRow = -1;
    if (isOpen && wasOpen && !effectiveDisabled && !collapsedIndices.empty()) {
        auto hitScope = ctx.ScopedOverlayHitCheck(id);
        const Rect popupRect = ctx.GetOpenPopupRect();
        const Vec2 mouse = ctx.GetMousePos();
        const float viewportH = keel::max(0.0f, popupRect.h - 8.0f);
        const float totalContentH = static_cast<float>(collapsedIndices.size()) * sm.itemHeight;
        const float maxScrollY = keel::max(0.0f, totalContentH - viewportH);
        state.popupScrollY = keel::clamp(state.popupScrollY, 0.0f, maxScrollY);

        if (maxScrollY > 0.5f && popupRect.Contains(mouse)) {
            const Vec2 wheel = ctx.GetMouseWheel();
            if (wheel.y != 0.0f || wheel.x != 0.0f) {
                const ScrollbarMetrics& sbm = theme.GetScrollbar(activeSize);
                state.popupScrollY = keel::clamp(state.popupScrollY - wheel.y * sbm.scrollStep, 0.0f, maxScrollY);
                ctx.ConsumeMouseWheel();
            }
        }

        const float itemW = keel::max(0.0f, popupRect.w - 8.0f);
        const Rect itemsViewport(popupRect.x + 4.0f, popupRect.y + 4.0f, itemW, viewportH);
        if (itemsViewport.Contains(mouse)) {
            for (keel::usize r = 0; r < collapsedIndices.size(); ++r) {
                const Rect itemRect(
                    popupRect.x + 4.0f,
                    popupRect.y + 4.0f + static_cast<float>(r) * sm.itemHeight - state.popupScrollY,
                    itemW,
                    sm.itemHeight
                );
                if (itemRect.Bottom() <= itemsViewport.y || itemRect.y >= itemsViewport.Bottom()) {
                    continue;
                }
                if (itemRect.Contains(mouse)) {
                    hoveredPopupRow = static_cast<keel::i32>(r);
                    state.highlightedPopupRow = hoveredPopupRow;
                    if (ctx.IsMouseReleased()) {
                        const keel::usize chosenIdx = collapsedIndices[r];
                        if (currIndex != chosenIdx) {
                            currIndex = chosenIdx;
                            activeIndex = currIndex;
                            res.changed = true;
                        }
                        ctx.RequestFocus(id);
                        ctx.ClosePopup();
                        isOpen = false;
                        ctx.ConsumeMouseClick();
                        ctx.RequestRepaint(3);
                        break;
                    }
                }
            }
        }

        if (isOpen) {
            if (ctx.ConsumeKey(Key::Escape)) {
                ctx.ClosePopup();
                isOpen = false;
                ctx.RequestFocus(id);
            } else if (ctx.ConsumeKey(Key::Enter) || ctx.ConsumeKey(Key::Space)) {
                if (state.highlightedPopupRow >= 0 &&
                    static_cast<keel::usize>(state.highlightedPopupRow) < collapsedIndices.size()) {
                    const keel::usize chosenIdx = collapsedIndices[static_cast<keel::usize>(state.highlightedPopupRow)];
                    if (currIndex != chosenIdx) {
                        currIndex = chosenIdx;
                        activeIndex = currIndex;
                        res.changed = true;
                    }
                }
                ctx.ClosePopup();
                isOpen = false;
                ctx.RequestFocus(id);
                ctx.RequestRepaint(3);
            } else if (ctx.ConsumeKey(Key::Down)) {
                const keel::i32 nextRow = keel::clamp(
                    state.highlightedPopupRow + 1,
                    0,
                    static_cast<keel::i32>(collapsedIndices.size()) - 1
                );
                state.highlightedPopupRow = nextRow;
            } else if (ctx.ConsumeKey(Key::Up)) {
                const keel::i32 prevRow = keel::clamp(
                    (state.highlightedPopupRow <= 0) ? 0 : (state.highlightedPopupRow - 1),
                    0,
                    static_cast<keel::i32>(collapsedIndices.size()) - 1
                );
                state.highlightedPopupRow = prevRow;
            }
        }
    } else if (!effectiveDisabled && ctx.IsFocused(id) && !items.empty()) {
        // Keyboard navigation along the trail when dropdown is closed
        if (ctx.ConsumeKey(Key::Right) || ctx.ConsumeKey(Key::Down)) {
            if (currIndex + 1 < items.size()) {
                ++currIndex;
                activeIndex = currIndex;
                res.changed = true;
            }
        } else if (ctx.ConsumeKey(Key::Left) || ctx.ConsumeKey(Key::Up)) {
            if (currIndex > 0) {
                --currIndex;
                activeIndex = currIndex;
                res.changed = true;
            }
        } else if (ctx.ConsumeKey(Key::Home)) {
            if (currIndex != 0) {
                currIndex = 0;
                activeIndex = 0;
                res.changed = true;
            }
        } else if (ctx.ConsumeKey(Key::End)) {
            if (currIndex != items.size() - 1) {
                currIndex = items.size() - 1;
                activeIndex = currIndex;
                res.changed = true;
            }
        }
    }

    if (res.hovered && !isOpen && !optsCopy.tooltip.empty()) {
        ctx.SetTooltip(optsCopy.tooltip, prevBounds);
    }

    const ControlSpec ctrl = EffectiveBreadcrumbControl(optsCopy, activeSize, theme);

    Node* node = ctx.CreateNode(NodeType::Breadcrumb, id);

    optsCopy.tooltip = ctx.AllocText(optsCopy.tooltip);
    BreadcrumbItem* arenaItems = ctx.GetArena().create_array<BreadcrumbItem>(items.size());
    keel::u64 itemsHash = 0;
    for (keel::usize i = 0; i < items.size(); ++i) {
        arenaItems[i] = items[i];
        arenaItems[i].label = ctx.String(items[i].label);
        arenaItems[i].badge = ctx.String(items[i].badge);
        if (!arenaItems[i].icon.GetPath().empty()) {
            arenaItems[i].icon = Icon::File(ctx.AllocString(arenaItems[i].icon.GetPath()));
        }
        itemsHash = detail::HashArgs(
            itemsHash,
            arenaItems[i].label,
            arenaItems[i].icon,
            arenaItems[i].badge
        );
    }

    if (isOpen && !collapsedIndices.empty()) {
        const Rect currentPopupRect = ctx.GetOpenPopupRect();
        const float popupScrollY = state.popupScrollY;
        const keel::i32 effectiveHoveredRow = (hoveredPopupRow >= 0) ? hoveredPopupRow : state.highlightedPopupRow;
        keel::i32 activePopupRow = -1;
        BreadcrumbItem* popupItems = ctx.GetArena().create_array<BreadcrumbItem>(collapsedIndices.size());
        for (keel::usize r = 0; r < collapsedIndices.size(); ++r) {
            const keel::usize origIdx = collapsedIndices[r];
            if (origIdx == currIndex) {
                activePopupRow = static_cast<keel::i32>(r);
            }
            popupItems[r] = arenaItems[origIdx];
        }

        ctx.SetOpenPopupVisualHash(detail::HashArgs(
            currentPopupRect.x,
            currentPopupRect.y,
            currentPopupRect.w,
            currentPopupRect.h,
            activePopupRow,
            effectiveHoveredRow,
            popupScrollY,
            collapsedIndices.size(),
            itemsHash
        ));

        BreadcrumbOptions popupOpts = optsCopy;
        const keel::ArrayRef<BreadcrumbItem> constPopupItems(popupItems, collapsedIndices.size());
        ctx.RegisterOverlay([currentPopupRect, constPopupItems, activePopupRow, effectiveHoveredRow, popupOpts, activeSize, theme, popupScrollY](ICanvas& c) {
            RenderBreadcrumbPopup(
                c,
                currentPopupRect,
                constPopupItems,
                activePopupRow,
                effectiveHoveredRow,
                popupOpts,
                activeSize,
                theme,
                popupScrollY
            );
        });
    }

    auto* data = ctx.AllocPayload<BreadcrumbData>();
    data->opts = optsCopy;
    data->items = keel::ArrayRef<BreadcrumbItem>(arenaItems, items.size());
    data->activeIndex = currIndex;
    data->popupOpen = isOpen;
    data->hoveredToken = hoveredToken;
    data->pressedToken = pressedToken;

    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        itemsHash,
        currIndex,
        isOpen,
        hoveredToken,
        pressedToken,
        static_cast<keel::u8>(optsCopy.separator),
        optsCopy.maxVisibleItems,
        optsCopy.layout.width,
        optsCopy.layout.height,
        optsCopy.layout.minWidth.value_or(0.0f),
        optsCopy.layout.maxWidth.value_or(0.0f),
        optsCopy.layout.minHeight.value_or(0.0f),
        optsCopy.layout.maxHeight.value_or(0.0f),
        optsCopy.style.color
    );

    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* c, const UiTheme& t) noexcept -> Vec2 {
        const auto* d = n->As<BreadcrumbData>();
        if (!d) return {0.0f, 0.0f};
        return Breadcrumb::Measure(d->items, d->opts, n->uiSize, c, t);
    };
    node->renderFn = &Breadcrumb::Render;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->active = res.pressed;
    node->focused = ctx.IsFocused(id);

    Control::SetupNode(ctx, node, ctrl);


    return res;
}

void Breadcrumb::Render(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node) return;
    const auto* d = node->As<BreadcrumbData>();
    if (!d) return;

    const Rect bounds = node->computedRect;
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const UiSize size = node->uiSize;
    const bool disabled = node->disabled;
    const bool focused = node->focused;
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const auto& pal = theme.palette;

    const Rect innerRect = bounds;

    if (d->items.empty()) return;

    keel::SmallArray<BreadcrumbToken, 16> tokens;
    keel::SmallArray<Rect, 16> tokenRects;
    keel::SmallArray<Rect, 16> sepRects;
    ComputeBreadcrumbLayout(innerRect, d->items, d->opts, size, &canvas, theme, tokens, tokenRects, sepRects);

    canvas.PushClip(bounds);

    // 1. Render separators between tokens
    const Color sepColor = disabled ? pal.textDisabled : pal.textMuted;
    for (const Rect& sr : sepRects) {
        DrawSeparatorGlyph(canvas, sr, d->opts.separator, size, theme, sepColor);
    }

    // 2. Render tokens (items & interactive "..." dropdown trigger pill)
    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const float badgeFontSize = fontSize * 0.82f;
    const float pillRadius = keel::max(4.0f, cm.cornerRadius * 0.85f);
    const float pillInsetY = keel::max(2.0f, innerRect.h * 0.10f);

    for (keel::usize i = 0; i < tokens.size(); ++i) {
        const BreadcrumbToken& tok = tokens[i];
        const Rect tr = tokenRects[i];
        const Rect pillRect(tr.x, tr.y + pillInsetY, tr.w, keel::max(0.0f, tr.h - pillInsetY * 2.0f));

        const bool isHovered = !disabled && (d->hoveredToken == static_cast<keel::i32>(i));
        const bool isPressed = !disabled && (d->pressedToken == static_cast<keel::i32>(i));
        const bool isActiveItem = !tok.isEllipsis && (tok.itemIndex == d->activeIndex);

        const Color accentColor = d->opts.style.color.HasAny()
            ? d->opts.style.color.Resolve(isHovered, isPressed, disabled, pal.accent)
            : pal.accent;

        if (tok.isEllipsis) {
            const bool isOpenTrigger = d->popupOpen && !disabled;
            const Color pillBg = isOpenTrigger
                ? accentColor.WithAlphaF(0.20f)
                : (isPressed ? pal.surfaceActive : (isHovered ? pal.surfaceHover : pal.surfaceHover.WithAlphaF(0.55f)));
            const Color pillBorder = isOpenTrigger
                ? accentColor.WithAlphaF(0.65f)
                : (isHovered ? pal.borderHover : pal.borderSubtle);
            canvas.DrawRoundRect(pillRect, pillRadius, pillBg, pillBorder, 1.0f);

            const Color dotsCol = disabled
                ? pal.textDisabled
                : (isOpenTrigger ? accentColor : (isHovered ? pal.textPrimary : pal.textSecondary));
            canvas.DrawTextBox("...", pillRect, fontSize, dotsCol, 0.5f, 0.5f);
            continue;
        }

        if (tok.itemIndex >= d->items.size()) continue;
        const BreadcrumbItem& item = d->items[tok.itemIndex];

        // Pill background for active or hovered breadcrumb item
        if (isActiveItem) {
            const Color activeFill = d->opts.style.color.HasAny()
                ? accentColor.WithAlphaF(disabled ? 0.10f : (isHovered ? 0.24f : 0.16f))
                : pal.accent.WithAlphaF(disabled ? 0.10f : (isHovered ? 0.22f : 0.14f));
            const Color activeStroke = (focused && !disabled)
                ? pal.focusRing
                : accentColor.WithAlphaF(disabled ? 0.20f : 0.45f);
            canvas.DrawRoundRect(pillRect, pillRadius, activeFill, activeStroke, 1.0f);
        } else if (isHovered || isPressed) {
            const Color hovFill = isPressed ? pal.surfaceActive : pal.surfaceHover;
            canvas.DrawRoundRect(pillRect, pillRadius, hovFill);
        }

        Color fgColor = disabled
            ? pal.textDisabled
            : (isActiveItem
                ? (d->opts.style.color.HasAny() ? accentColor : pal.textPrimary)
                : (isHovered ? pal.textPrimary : pal.textSecondary));

        const float iconGap = 5.0f;
        const float badgeGap = 5.0f;
        const float padX = keel::min(keel::max(4.0f, cm.paddingX * 0.55f), tr.w * 0.14f);
        const float availW = keel::max(0.0f, tr.w - padX * 2.0f);

        const bool hasIcon = item.icon.IsValid() && (availW >= cm.iconSize * 0.85f);
        float badgeW = 0.0f;
        if (!item.badge.empty()) {
            badgeW = canvas.MeasureText(item.badge, badgeFontSize).x + 10.0f;
        }
        const bool showBadge = !item.badge.empty() &&
            (availW >= (hasIcon ? cm.iconSize + iconGap : 0.0f) + badgeW + (!item.label.empty() ? fontSize * 1.8f + badgeGap : 0.0f));

        const float fixedW = (hasIcon ? cm.iconSize : 0.0f) + (showBadge ? badgeW + badgeGap : 0.0f);
        const float availLabelW = keel::max(0.0f, availW - fixedW - ((hasIcon && !item.label.empty()) ? iconGap : 0.0f));
        const bool showLabel = !item.label.empty() && (!hasIcon || availLabelW >= fontSize * 1.1f);

        keel::StringRef displayLabel{};
        keel::FixedString<256> truncatedLabel;
        float labelW = 0.0f;
        if (showLabel) {
            displayLabel = item.label;
            labelW = canvas.MeasureText(displayLabel, fontSize).x;
            if (labelW > availLabelW + 0.5f && availLabelW > 0.0f) {
                truncatedLabel = TextUtil::TruncateWithEllipsis(displayLabel, availLabelW, fontSize, &canvas);
                displayLabel = truncatedLabel.as_ref();
                labelW = keel::min(availLabelW, canvas.MeasureText(displayLabel, fontSize).x);
            }
        }

        float contentW = 0.0f;
        if (hasIcon) {
            contentW += cm.iconSize;
        }
        if (showLabel) {
            if (contentW > 0.0f) contentW += iconGap;
            contentW += labelW;
        }
        if (showBadge) {
            if (contentW > 0.0f) contentW += badgeGap;
            contentW += badgeW;
        }

        float curX = tr.x + keel::max(padX, (tr.w - contentW) * 0.5f);
        const float centerY = tr.y + tr.h * 0.5f;

        if (hasIcon) {
            const Rect iconR(curX, centerY - cm.iconSize * 0.5f, cm.iconSize, cm.iconSize);
            item.icon.Draw(canvas, iconR, isActiveItem ? accentColor : fgColor);
            curX += cm.iconSize + (showLabel ? iconGap : 0.0f);
        }

        if (showLabel) {
            const Rect textR(curX, tr.y, labelW + 2.0f, tr.h);
            canvas.DrawTextBox(displayLabel, textR, fontSize, fgColor, 0.0f, 0.5f);
            curX += labelW;
        }

        if (showBadge) {
            if (hasIcon || showLabel) curX += badgeGap;
            const float badgeH = badgeFontSize + 5.0f;
            const Rect badgeR(curX, centerY - badgeH * 0.5f, badgeW, badgeH);
            const Color badgeBg = isActiveItem ? accentColor.WithAlphaF(0.25f) : pal.surfaceHover;
            const Color badgeFg = isActiveItem ? accentColor : pal.textSecondary;
            canvas.DrawRoundRect(badgeR, 4.0f, badgeBg);
            canvas.DrawTextBox(item.badge, badgeR, badgeFontSize, badgeFg, 0.5f, 0.5f);
        }
    }

    canvas.PopClip();
}

} // namespace guikit
