#include "guikit/widgets/Breadcrumb.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

struct BreadcrumbPersistentState {
    int highlightedPopupRow{-1};
    float popupScrollY{0.0f};
};

struct BreadcrumbToken {
    size_t itemIndex{0};
    bool isEllipsis{false};
};

ControlSpec EffectiveBreadcrumbControl(const BreadcrumbOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& sm = theme.GetSelect(size);
    ControlSpec c{};
    c.width = opts.width;
    c.height = opts.height;
    c.align = opts.align;
    c.background = opts.background;
    c.border = opts.border;
    c.defaultLayout = ElementLayout::Stacked;
    c.defaultInlineOrder = InlineOrder::LabelFirst;
    c.labelGap = sm.labelGap;
    c.descGap = sm.descGap;
    c.inlineGap = sm.paddingX;
    c.stretchable = true;
    c.stretchHeightWhenStandalone = false;
    c.hasIntrinsicBox = false;
    return c;
}

void BuildBreadcrumbTokens(
    std::span<const BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    std::vector<BreadcrumbToken>& outTokens
) {
    outTokens.clear();
    if (items.empty()) return;

    const bool shouldCollapse = (opts.maxVisibleItems >= 2) &&
                                (items.size() > static_cast<size_t>(opts.maxVisibleItems));

    if (!shouldCollapse) {
        outTokens.reserve(items.size());
        for (size_t i = 0; i < items.size(); ++i) {
            outTokens.push_back(BreadcrumbToken{.itemIndex = i, .isEllipsis = false});
        }
        return;
    }

    const size_t trailingCount = static_cast<size_t>(opts.maxVisibleItems - 1);
    outTokens.reserve(1 + 1 + trailingCount);
    outTokens.push_back(BreadcrumbToken{.itemIndex = 0, .isEllipsis = false});
    outTokens.push_back(BreadcrumbToken{.itemIndex = 1, .isEllipsis = true});

    const size_t startTrailing = items.size() - trailingCount;
    for (size_t i = startTrailing; i < items.size(); ++i) {
        outTokens.push_back(BreadcrumbToken{.itemIndex = i, .isEllipsis = false});
    }
}

float MeasureTokenNaturalWidth(
    std::span<const BreadcrumbItem> items,
    const BreadcrumbToken& token,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const float padX = std::max(theme.Scale(6.0f), cm.paddingX * 0.65f);

    if (token.isEllipsis) {
        const float dotsW = TextUtil::MeasureWidth("...", fontSize, canvas);
        return std::max(theme.Scale(26.0f), dotsW + padX * 1.8f);
    }

    if (token.itemIndex >= items.size()) {
        return padX * 2.0f;
    }

    const BreadcrumbItem& item = items[token.itemIndex];
    const float iconGap = theme.Scale(5.0f);
    const float badgeGap = theme.Scale(5.0f);
    const float badgeFontSize = fontSize * 0.82f;

    float contentW = 0.0f;
    if (item.icon.IsValid()) {
        contentW += cm.iconSize;
    }
    if (!item.label.empty()) {
        if (contentW > 0.0f) contentW += iconGap;
        contentW += TextUtil::MeasureWidth(item.label.View(), fontSize, canvas);
    }
    if (!item.badge.empty()) {
        if (contentW > 0.0f) contentW += badgeGap;
        contentW += TextUtil::MeasureWidth(item.badge.View(), badgeFontSize, canvas) + theme.Scale(10.0f);
    }

    return std::max(cm.defaultHeight * 0.75f, contentW + padX * 2.0f);
}

float MeasureSeparatorSlotWidth(UiSize size, const UiTheme& theme) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    return std::max(theme.Scale(14.0f), cm.iconSize * 1.05f);
}

Vec2 MeasureBreadcrumbNaturalBox(
    std::span<const BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    std::vector<BreadcrumbToken> tokens;
    BuildBreadcrumbTokens(items, opts, tokens);

    const float sepW = MeasureSeparatorSlotWidth(size, theme);
    float totalW = 0.0f;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) {
            totalW += sepW;
        }
        totalW += MeasureTokenNaturalWidth(items, tokens[i], size, canvas, theme);
    }

    if (opts.stretch != StretchMode::Fill && opts.width.has_value() && *opts.width > 0.0f) {
        totalW = theme.Scale(*opts.width);
    }

    const float boxH = (opts.height.has_value() && *opts.height > 0.0f)
        ? theme.Scale(*opts.height)
        : cm.defaultHeight;

    return {std::max(theme.Scale(24.0f), totalW), boxH};
}

void ComputeBreadcrumbLayout(
    Rect innerRect,
    std::span<const BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    std::vector<BreadcrumbToken>& outTokens,
    std::vector<Rect>& outTokenRects,
    std::vector<Rect>& outSepRects
) {
    BuildBreadcrumbTokens(items, opts, outTokens);
    outTokenRects.clear();
    outSepRects.clear();
    if (outTokens.empty()) return;

    outTokenRects.reserve(outTokens.size());
    outSepRects.reserve(outTokens.size() > 1 ? outTokens.size() - 1 : 0);

    const float sepW = MeasureSeparatorSlotWidth(size, theme);
    std::vector<float> widths(outTokens.size(), 0.0f);
    float totalW = 0.0f;
    for (size_t i = 0; i < outTokens.size(); ++i) {
        if (i > 0) totalW += sepW;
        widths[i] = MeasureTokenNaturalWidth(items, outTokens[i], size, canvas, theme);
        totalW += widths[i];
    }

    // Responsive auto-collapse when available innerRect width is narrower than natural trail width
    const bool hasEllipsis = std::any_of(outTokens.begin(), outTokens.end(), [](const BreadcrumbToken& t) { return t.isEllipsis; });
    if (!hasEllipsis && outTokens.size() >= 3 && innerRect.w > 1.0f && totalW > innerRect.w + 0.5f) {
        BreadcrumbOptions collapsedOpts = opts;
        for (uint32_t maxVis = static_cast<uint32_t>(outTokens.size() - 1); maxVis >= 2; --maxVis) {
            collapsedOpts.maxVisibleItems = maxVis;
            BuildBreadcrumbTokens(items, collapsedOpts, outTokens);
            widths.assign(outTokens.size(), 0.0f);
            totalW = 0.0f;
            for (size_t i = 0; i < outTokens.size(); ++i) {
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
        const float sumTokW = std::max(1.0f, totalW - totalSepW);
        if (innerRect.w > totalSepW + static_cast<float>(outTokens.size()) * theme.Scale(18.0f)) {
            const float availTokW = innerRect.w - totalSepW;
            const float ratio = availTokW / sumTokW;
            for (float& w : widths) {
                w *= ratio;
            }
        } else {
            const float ratio = innerRect.w / std::max(1.0f, totalW);
            effSepW *= ratio;
            for (float& w : widths) {
                w *= ratio;
            }
        }
        totalW = innerRect.w;
    }

    float startX = innerRect.x;
    if (opts.stretch == StretchMode::Fill && innerRect.w > totalW) {
        if (opts.align == Align::Center) {
            startX += (innerRect.w - totalW) * 0.5f;
        } else if (opts.align == Align::End) {
            startX += (innerRect.w - totalW);
        }
    }

    float curX = startX;
    for (size_t i = 0; i < outTokens.size(); ++i) {
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
    const float iconSz = std::round(cm.iconSize * 0.78f);
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
            const float r = std::max(1.8f, theme.Scale(2.2f));
            canvas.DrawRoundRect(Rect(center.x - r, center.y - r, r * 2.0f, r * 2.0f), r, color);
            break;
        }
    }
}

void RenderBreadcrumbPopup(
    ICanvas& canvas,
    Rect popupRect,
    std::span<const BreadcrumbItem> collapsedItems,
    int activeRow,
    int hoveredRow,
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

    const float viewportH = std::max(0.0f, popupRect.h - 8.0f);
    const float totalContentH = static_cast<float>(collapsedItems.size()) * sm.itemHeight;
    const float maxScrollY = std::max(0.0f, totalContentH - viewportH);
    const float clampedScrollY = std::clamp(scrollY, 0.0f, maxScrollY);
    const float itemW = std::max(0.0f, popupRect.w - 8.0f);
    const Rect itemsClipRect(popupRect.x + 1.0f, popupRect.y + 4.0f, std::max(0.0f, popupRect.w - 2.0f), viewportH);

    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const float badgeFontSize = fontSize * 0.82f;
    const float iconGap = theme.Scale(6.0f);
    const float badgeGap = theme.Scale(8.0f);

    canvas.PushClip(itemsClipRect);

    for (size_t i = 0; i < collapsedItems.size(); ++i) {
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

        const bool isSelected = (static_cast<int>(i) == activeRow);
        const bool isHovered = (static_cast<int>(i) == hoveredRow);
        const Color accentColor = opts.color.HasAny()
            ? opts.color.Resolve(isHovered, false, false, pal.accent)
            : pal.accent;

        if (isSelected) {
            const Color selFill = accentColor.WithAlphaF(isHovered ? 0.22f : 0.14f);
            canvas.DrawRoundRect(itemRect, theme.Scale(4.0f), selFill);
        } else if (isHovered) {
            canvas.DrawRoundRect(itemRect, theme.Scale(4.0f), pal.surfaceHover);
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
            rightX -= theme.Scale(6.0f);
        }

        if (!item.badge.empty()) {
            const float badgeW = canvas.MeasureText(item.badge.View(), badgeFontSize).x + theme.Scale(10.0f);
            const float badgeH = badgeFontSize + theme.Scale(5.0f);
            rightX -= badgeW;
            const Rect badgeR(rightX, centerY - badgeH * 0.5f, badgeW, badgeH);
            const Color badgeBg = isSelected ? accentColor.WithAlphaF(0.25f) : pal.surfaceHover;
            const Color badgeFg = isSelected ? accentColor : pal.textSecondary;
            canvas.DrawRoundRect(badgeR, theme.Scale(4.0f), badgeBg);
            canvas.DrawTextBox(item.badge.View(), badgeR, badgeFontSize, badgeFg, 0.5f, 0.5f);
            rightX -= badgeGap;
        }

        if (item.icon.IsValid()) {
            const Rect iconR(leftX, centerY - cm.iconSize * 0.5f, cm.iconSize, cm.iconSize);
            item.icon.Draw(canvas, iconR, isSelected ? accentColor : fgColor);
            leftX += cm.iconSize + iconGap;
        }

        if (!item.label.empty() && rightX > leftX) {
            const Rect textR(leftX, itemRect.y, rightX - leftX, itemRect.h);
            canvas.DrawTextBox(item.label.View(), textR, fontSize, isSelected ? accentColor : pal.textPrimary, 0.0f, 0.5f);
        }
    }

    canvas.PopClip();
}

} // namespace

Vec2 Breadcrumb::Measure(
    std::span<const BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const Vec2 boxDims = MeasureBreadcrumbNaturalBox(items, opts, size, canvas, theme);
    const ControlSpec ctrl = EffectiveBreadcrumbControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, opts.stretch, size, theme);
}

Response Breadcrumb::Run(
    Context& ctx,
    size_t& activeIndex,
    std::span<const std::string_view> items,
    const BreadcrumbOptions& opts,
    std::source_location loc
) {
    BreadcrumbItem* richItems = items.empty()
        ? nullptr
        : static_cast<BreadcrumbItem*>(ctx.GetArena().AllocateRaw(sizeof(BreadcrumbItem) * items.size(), alignof(BreadcrumbItem)));
    for (size_t i = 0; i < items.size(); ++i) {
        ::new (&richItems[i]) BreadcrumbItem{};
        richItems[i].label = ctx.String(items[i]);
    }
    return Run(ctx, activeIndex, std::span<const BreadcrumbItem>(richItems, items.size()), opts, loc);
}

Response Breadcrumb::Run(
    Context& ctx,
    int& activeIndex,
    std::span<const std::string_view> items,
    const BreadcrumbOptions& opts,
    std::source_location loc
) {
    size_t idx = (activeIndex >= 0) ? static_cast<size_t>(activeIndex) : 0;
    Response res = Run(ctx, idx, items, opts, loc);
    if (res.changed) {
        activeIndex = static_cast<int>(idx);
    }
    return res;
}

Response Breadcrumb::Run(
    Context& ctx,
    int& activeIndex,
    std::span<const BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    std::source_location loc
) {
    size_t idx = (activeIndex >= 0) ? static_cast<size_t>(activeIndex) : 0;
    Response res = Run(ctx, idx, items, opts, loc);
    if (res.changed) {
        activeIndex = static_cast<int>(idx);
    }
    return res;
}

Response Breadcrumb::Run(
    Context& ctx,
    size_t& activeIndex,
    std::span<const BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    std::source_location loc
) {
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();

    auto& state = ctx.GetOrCreateState<BreadcrumbPersistentState>(id);
    const bool wasOpen = ctx.IsPopupOpen(id);

    BreadcrumbOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    ctx.RegisterFocusable(id, effectiveDisabled);
    Response res = ctx.Interact(id);

    size_t currIndex = items.empty() ? 0 : std::min(activeIndex, items.size() - 1);

    const ControlSpec prevCtrl = EffectiveBreadcrumbControl(optsCopy, activeSize, theme);
    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    const Rect innerBounds = Control::ResolveInnerRect(prevBounds, prevCtrl, activeSize, theme);

    std::vector<BreadcrumbToken> tokens;
    std::vector<Rect> tokenRects;
    std::vector<Rect> sepRects;
    ComputeBreadcrumbLayout(innerBounds, items, optsCopy, activeSize, canvas, theme, tokens, tokenRects, sepRects);

    int ellipsisTokenIdx = -1;
    std::vector<size_t> collapsedIndices;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i].isEllipsis) {
            ellipsisTokenIdx = static_cast<int>(i);
            const size_t startCollapsed = (i > 0) ? (tokens[i - 1].itemIndex + 1) : 0;
            const size_t endCollapsed = (i + 1 < tokens.size()) ? tokens[i + 1].itemIndex : items.size();
            for (size_t idx = startCollapsed; idx < endCollapsed && idx < items.size(); ++idx) {
                collapsedIndices.push_back(idx);
            }
            break;
        }
    }

    int hoveredToken = -1;
    int pressedToken = -1;
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
            for (size_t i = 0; i < tokenRects.size(); ++i) {
                if (tokenRects[i].Contains(mouse)) {
                    hoveredToken = static_cast<int>(i);
                    if (res.pressed) {
                        pressedToken = static_cast<int>(i);
                    }
                    break;
                }
            }
        }

        if (res.clicked) {
            ctx.RequestFocus(id);
            if (hoveredToken >= 0 && static_cast<size_t>(hoveredToken) < tokens.size()) {
                const BreadcrumbToken& tok = tokens[static_cast<size_t>(hoveredToken)];
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
        for (size_t r = 0; r < collapsedIndices.size(); ++r) {
            if (collapsedIndices[r] == currIndex) {
                state.highlightedPopupRow = static_cast<int>(r);
                break;
            }
        }
    }

    const SelectMetrics& sm = theme.GetSelect(activeSize);
    const ComponentMetrics& cm = theme.GetMetrics(activeSize);
    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, activeSize);
    const float badgeFontSize = fontSize * 0.82f;
    const float iconGap = theme.Scale(6.0f);
    const float badgeGap = theme.Scale(8.0f);

    if (isOpen && !collapsedIndices.empty()) {
        float maxRowW = theme.Scale(140.0f);
        for (size_t origIdx : collapsedIndices) {
            const BreadcrumbItem& item = items[origIdx];
            float w = sm.paddingX * 2.0f;
            if (item.icon.IsValid()) {
                w += cm.iconSize + iconGap;
            }
            if (!item.label.empty()) {
                w += TextUtil::MeasureWidth(item.label.View(), fontSize, canvas);
            }
            if (!item.badge.empty()) {
                w += badgeGap + TextUtil::MeasureWidth(item.badge.View(), badgeFontSize, canvas) + theme.Scale(10.0f);
            }
            if (origIdx == currIndex) {
                w += cm.iconSize + theme.Scale(8.0f);
            }
            maxRowW = std::max(maxRowW, w);
        }

        const float popupW = maxRowW + theme.Scale(8.0f);
        const size_t visibleRows = std::min<size_t>(collapsedIndices.size(), 8);
        float popupH = static_cast<float>(visibleRows) * sm.itemHeight + 8.0f;
        const Rect anchorRect = (ellipsisTokenIdx >= 0 && static_cast<size_t>(ellipsisTokenIdx) < tokenRects.size())
            ? tokenRects[static_cast<size_t>(ellipsisTokenIdx)]
            : prevBounds;
        float popupX = anchorRect.x;
        float popupY = anchorRect.Bottom() + 4.0f;

        const Rect vp = ctx.GetPopupViewportRect();
        if (vp.h > 0.0f) {
            const float availDown = vp.Bottom() - popupY - 6.0f;
            const float availUp = anchorRect.y - 4.0f - (vp.y + 6.0f);
            if (popupY + popupH > vp.Bottom() - 6.0f && availUp > availDown) {
                popupY = std::max(vp.y + 6.0f, anchorRect.y - 4.0f - popupH);
                popupH = std::clamp(anchorRect.y - 4.0f - popupY, std::min(sm.itemHeight + 8.0f, vp.h - 8.0f), popupH);
            } else if (popupY + popupH > vp.Bottom() - 6.0f) {
                popupH = std::max(std::min(sm.itemHeight + 8.0f, vp.h - 8.0f), availDown);
                popupY = std::clamp(popupY, vp.y + 4.0f, std::max(vp.y + 4.0f, vp.Bottom() - popupH - 4.0f));
            }
        }
        if (vp.w > 0.0f) {
            popupX = std::clamp(popupX, vp.x + 4.0f, std::max(vp.x + 4.0f, vp.Right() - popupW - 4.0f));
        }

        ctx.OpenPopup(id, Rect(popupX, popupY, popupW, popupH));
    } else if (isOpen) {
        ctx.ClosePopup();
        isOpen = false;
    }

    int hoveredPopupRow = -1;
    if (isOpen && wasOpen && !effectiveDisabled && !collapsedIndices.empty()) {
        ctx.BeginOverlayHitCheck(id);
        const Rect popupRect = ctx.GetOpenPopupRect();
        const Vec2 mouse = ctx.GetMousePos();
        const float viewportH = std::max(0.0f, popupRect.h - 8.0f);
        const float totalContentH = static_cast<float>(collapsedIndices.size()) * sm.itemHeight;
        const float maxScrollY = std::max(0.0f, totalContentH - viewportH);
        state.popupScrollY = std::clamp(state.popupScrollY, 0.0f, maxScrollY);

        if (maxScrollY > 0.5f && popupRect.Contains(mouse)) {
            const Vec2 wheel = ctx.GetPopupMouseWheel(id);
            if (wheel.y != 0.0f || wheel.x != 0.0f) {
                const ScrollbarMetrics& sbm = theme.GetScrollbar(activeSize);
                state.popupScrollY = std::clamp(state.popupScrollY - wheel.y * sbm.scrollStep, 0.0f, maxScrollY);
                ctx.ConsumeMouseWheel();
            }
        }

        const float itemW = std::max(0.0f, popupRect.w - 8.0f);
        const Rect itemsViewport(popupRect.x + 4.0f, popupRect.y + 4.0f, itemW, viewportH);
        if (itemsViewport.Contains(mouse)) {
            for (size_t r = 0; r < collapsedIndices.size(); ++r) {
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
                    hoveredPopupRow = static_cast<int>(r);
                    state.highlightedPopupRow = hoveredPopupRow;
                    if (ctx.IsMouseReleased()) {
                        const size_t chosenIdx = collapsedIndices[r];
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
                    static_cast<size_t>(state.highlightedPopupRow) < collapsedIndices.size()) {
                    const size_t chosenIdx = collapsedIndices[static_cast<size_t>(state.highlightedPopupRow)];
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
                const int nextRow = std::clamp(
                    state.highlightedPopupRow + 1,
                    0,
                    static_cast<int>(collapsedIndices.size()) - 1
                );
                state.highlightedPopupRow = nextRow;
            } else if (ctx.ConsumeKey(Key::Up)) {
                const int prevRow = std::clamp(
                    (state.highlightedPopupRow <= 0) ? 0 : (state.highlightedPopupRow - 1),
                    0,
                    static_cast<int>(collapsedIndices.size()) - 1
                );
                state.highlightedPopupRow = prevRow;
            }
        }
        ctx.EndOverlayHitCheck();
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
        ctx.SetTooltip(optsCopy.tooltip.View(), prevBounds);
    }

    ControlSpec ctrl = EffectiveBreadcrumbControl(optsCopy, activeSize, theme);
    if (optsCopy.stretch != StretchMode::Fill && !ctrl.width.has_value()) {
        const Vec2 naturalDims = MeasureBreadcrumbNaturalBox(items, optsCopy, activeSize, canvas, theme);
        ctrl.width = (theme.scale > 0.0f) ? (naturalDims.x / theme.scale) : naturalDims.x;
    }

    Node* node = ctx.CreateNode(NodeType::Breadcrumb, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;

    optsCopy.tooltip = ctx.String(optsCopy.tooltip.View());
    BreadcrumbItem* arenaItems = items.empty()
        ? nullptr
        : static_cast<BreadcrumbItem*>(ctx.GetArena().AllocateRaw(sizeof(BreadcrumbItem) * items.size(), alignof(BreadcrumbItem)));
    uint64_t itemsHash = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        ::new (&arenaItems[i]) BreadcrumbItem(items[i]);
        arenaItems[i].label = ctx.String(items[i].label.View());
        arenaItems[i].badge = ctx.String(items[i].badge.View());
        if (!arenaItems[i].icon.GetPath().empty()) {
            arenaItems[i].icon = Icon::File(ctx.AllocString(arenaItems[i].icon.GetPath()));
        }
        itemsHash = detail::HashArgs(
            itemsHash,
            arenaItems[i].label.View(),
            arenaItems[i].icon,
            arenaItems[i].badge.View()
        );
    }

    if (isOpen && !collapsedIndices.empty()) {
        const Rect currentPopupRect = ctx.GetOpenPopupRect();
        const float popupScrollY = state.popupScrollY;
        const int effectiveHoveredRow = (hoveredPopupRow >= 0) ? hoveredPopupRow : state.highlightedPopupRow;
        int activePopupRow = -1;
        std::vector<BreadcrumbItem> popupItems;
        popupItems.reserve(collapsedIndices.size());
        for (size_t r = 0; r < collapsedIndices.size(); ++r) {
            const size_t origIdx = collapsedIndices[r];
            if (origIdx == currIndex) {
                activePopupRow = static_cast<int>(r);
            }
            popupItems.push_back(arenaItems[origIdx]);
        }

        ctx.SetOpenPopupVisualHash(detail::HashArgs(
            currentPopupRect.x,
            currentPopupRect.y,
            currentPopupRect.w,
            currentPopupRect.h,
            activePopupRow,
            effectiveHoveredRow,
            popupScrollY,
            popupItems.size(),
            itemsHash
        ));

        BreadcrumbOptions popupOpts = optsCopy;
        ctx.RegisterOverlay([currentPopupRect, popupItems = std::move(popupItems), activePopupRow, effectiveHoveredRow, popupOpts, activeSize, theme, popupScrollY](ICanvas& c) {
            RenderBreadcrumbPopup(
                c,
                currentPopupRect,
                std::span<const BreadcrumbItem>(popupItems),
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
    data->items = std::span<const BreadcrumbItem>(arenaItems, items.size());
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
        static_cast<uint8_t>(optsCopy.separator),
        optsCopy.maxVisibleItems,
        static_cast<uint8_t>(optsCopy.stretch),
        static_cast<uint8_t>(optsCopy.align),
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.color,
        optsCopy.background,
        optsCopy.border
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

    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);

    if (pushedTheme) {
        ctx.PopTheme();
    }

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

    const ControlSpec ctrl = EffectiveBreadcrumbControl(d->opts, size, theme);
    Control::RenderFrame(canvas, bounds, ctrl, size, disabled, theme, d->hoveredToken >= 0, d->pressedToken >= 0 || focused || d->popupOpen);
    const Rect innerRect = Control::ResolveInnerRect(bounds, ctrl, size, theme);

    if (d->items.empty()) return;

    std::vector<BreadcrumbToken> tokens;
    std::vector<Rect> tokenRects;
    std::vector<Rect> sepRects;
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
    const float pillRadius = std::max(theme.Scale(4.0f), cm.cornerRadius * 0.85f);
    const float pillInsetY = std::max(theme.Scale(2.0f), innerRect.h * 0.10f);

    for (size_t i = 0; i < tokens.size(); ++i) {
        const BreadcrumbToken& tok = tokens[i];
        const Rect tr = tokenRects[i];
        const Rect pillRect(tr.x, tr.y + pillInsetY, tr.w, std::max(0.0f, tr.h - pillInsetY * 2.0f));

        const bool isHovered = !disabled && (d->hoveredToken == static_cast<int>(i));
        const bool isPressed = !disabled && (d->pressedToken == static_cast<int>(i));
        const bool isActiveItem = !tok.isEllipsis && (tok.itemIndex == d->activeIndex);

        const Color accentColor = d->opts.color.HasAny()
            ? d->opts.color.Resolve(isHovered, isPressed, disabled, pal.accent)
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
            const Color activeFill = d->opts.color.HasAny()
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
                ? (d->opts.color.HasAny() ? accentColor : pal.textPrimary)
                : (isHovered ? pal.textPrimary : pal.textSecondary));

        const float iconGap = theme.Scale(5.0f);
        const float badgeGap = theme.Scale(5.0f);
        const float padX = std::min(std::max(theme.Scale(4.0f), cm.paddingX * 0.55f), tr.w * 0.14f);
        const float availW = std::max(0.0f, tr.w - padX * 2.0f);

        const bool hasIcon = item.icon.IsValid() && (availW >= cm.iconSize * 0.85f);
        float badgeW = 0.0f;
        if (!item.badge.empty()) {
            badgeW = canvas.MeasureText(item.badge.View(), badgeFontSize).x + theme.Scale(10.0f);
        }
        const bool showBadge = !item.badge.empty() &&
            (availW >= (hasIcon ? cm.iconSize + iconGap : 0.0f) + badgeW + (!item.label.empty() ? fontSize * 1.8f + badgeGap : 0.0f));

        const float fixedW = (hasIcon ? cm.iconSize : 0.0f) + (showBadge ? badgeW + badgeGap : 0.0f);
        const float availLabelW = std::max(0.0f, availW - fixedW - ((hasIcon && !item.label.empty()) ? iconGap : 0.0f));
        const bool showLabel = !item.label.empty() && (!hasIcon || availLabelW >= fontSize * 1.1f);

        std::string displayLabel;
        float labelW = 0.0f;
        if (showLabel) {
            displayLabel = std::string(item.label.View());
            labelW = canvas.MeasureText(displayLabel, fontSize).x;
            if (labelW > availLabelW + 0.5f && availLabelW > 0.0f) {
                displayLabel = TextUtil::TruncateWithEllipsis(item.label.View(), availLabelW, fontSize, &canvas);
                labelW = std::min(availLabelW, canvas.MeasureText(displayLabel, fontSize).x);
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

        float curX = tr.x + std::max(padX, (tr.w - contentW) * 0.5f);
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
            const float badgeH = badgeFontSize + theme.Scale(5.0f);
            const Rect badgeR(curX, centerY - badgeH * 0.5f, badgeW, badgeH);
            const Color badgeBg = isActiveItem ? accentColor.WithAlphaF(0.25f) : pal.surfaceHover;
            const Color badgeFg = isActiveItem ? accentColor : pal.textSecondary;
            canvas.DrawRoundRect(badgeR, theme.Scale(4.0f), badgeBg);
            canvas.DrawTextBox(item.badge.View(), badgeR, badgeFontSize, badgeFg, 0.5f, 0.5f);
        }
    }

    canvas.PopClip();
}

} // namespace guikit
