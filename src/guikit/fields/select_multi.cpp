#include "select_internal.hpp"

namespace guikit {

using namespace detail::select_impl;

namespace {

template <typename Pal>
constexpr Color ResolvePrimaryColor(const Pal& pal) noexcept {
    if constexpr (requires { pal.primary; }) {
        return pal.primary;
    } else {
        return pal.accent;
    }
}

struct MultiChipBox {
    keel::usize selectedVecIdx{0};
    keel::usize itemIdx{0};
    Rect chipRect{};
    Rect removeRect{};
};

struct MultiChipLayout {
    keel::SmallArray<MultiChipBox, 8> visibleChips{};
    keel::usize totalValidCount{0};
    keel::usize overflowCount{0};
    Rect overflowRect{};
    keel::f32 afterChipsX{0.0f};
};

MultiChipLayout ComputeMultiChipLayout(
    Rect textRect,
    keel::f32 boxH,
    keel::ArrayRef<keel::usize> selectedIndices,
    keel::ArrayRef<SelectItem> items,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 reserveInputW
) {
    MultiChipLayout layout{};
    layout.afterChipsX = textRect.x;

    struct ValidEntry {
        keel::usize selIdx{0};
        keel::usize itemIdx{0};
    };
    keel::SmallArray<ValidEntry, 16> validEntries;
    validEntries.reserve(selectedIndices.size());
    for (keel::usize si = 0; si < selectedIndices.size(); ++si) {
        if (selectedIndices[si] < items.size()) {
            validEntries.push_back(ValidEntry{si, selectedIndices[si]});
        }
    }
    layout.totalValidCount = validEntries.size();
    if (validEntries.empty() || textRect.w <= 0.0f) {
        return layout;
    }

    const SelectMetrics& m = theme.GetSelect(size);
    const keel::f32 chipH = keel::max(14.0f, boxH - 8.0f);
    const keel::f32 chipY = textRect.y + (textRect.h - chipH) * 0.5f;
    const keel::f32 chipFont = keel::max(10.0f, theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, size));
    const keel::f32 chipIconSz = keel::max(10.0f, m.iconSize - 2.0f);
    const keel::f32 removeIconSz = keel::max(8.0f, m.iconSize - 4.0f);
    const keel::f32 removeBtnW = removeIconSz + 6.0f;
    const keel::f32 chipGap = 4.0f;

    const keel::f32 availW = keel::max(24.0f, textRect.w - reserveInputW);

    keel::SmallArray<keel::f32, 16> chipWidths(validEntries.size(), 0.0f);
    for (keel::usize i = 0; i < validEntries.size(); ++i) {
        const SelectItem& it = items[validEntries[i].itemIdx];
        const keel::f32 textW = TextUtil::MeasureUiTextWidth(it.text, chipFont, canvas);
        const keel::f32 iconW = (!it.prefix.icon.IsEmpty()) ? (chipIconSz + 4.0f) : 0.0f;
        chipWidths[i] = 6.0f + iconW + textW + 4.0f + removeBtnW;
    }

    const keel::usize maxCap = keel::min(validEntries.size(), SelectData::kMaxChips);
    keel::f32 totalAllW = 0.0f;
    for (keel::usize i = 0; i < validEntries.size(); ++i) {
        if (i > 0) totalAllW += chipGap;
        totalAllW += chipWidths[i];
    }

    keel::usize fitCount = 0;
    keel::f32 badgeW = 0.0f;
    if (validEntries.size() <= SelectData::kMaxChips && totalAllW <= availW) {
        fitCount = validEntries.size();
        layout.overflowCount = 0;
    } else {
        for (keel::usize count = maxCap; count > 0; --count) {
            const keel::usize rem = validEntries.size() - count;
            if (rem == 0) continue;
            char badgeBuf[32]{};
            const keel::StringRef badgeStr = keel::format_buf(badgeBuf, "+%zu", rem);
            const keel::f32 bw = TextUtil::MeasureWidth(badgeStr, chipFont, canvas) + 12.0f;
            keel::f32 sumW = 0.0f;
            for (keel::usize j = 0; j < count; ++j) {
                if (j > 0) sumW += chipGap;
                sumW += chipWidths[j];
            }
            if (count > 0) sumW += chipGap;
            sumW += bw;
            if (sumW <= availW) {
                fitCount = count;
                layout.overflowCount = rem;
                badgeW = bw;
                break;
            }
        }
        if (fitCount == 0) {
            if (validEntries.size() == 1 && availW >= 32.0f) {
                fitCount = 1;
                chipWidths[0] = availW;
                layout.overflowCount = 0;
            } else {
                layout.overflowCount = validEntries.size();
                char badgeBuf[32]{};
                const keel::StringRef badgeStr = keel::format_buf(badgeBuf, "+%zu", layout.overflowCount);
                badgeW = keel::min(availW, TextUtil::MeasureWidth(badgeStr, chipFont, canvas) + 12.0f);
            }
        }
    }

    keel::f32 curX = textRect.x;
    layout.visibleChips.reserve(fitCount);
    for (keel::usize i = 0; i < fitCount; ++i) {
        const keel::f32 w = chipWidths[i];
        const Rect cRect(curX, chipY, w, chipH);
        const Rect rRect(curX + w - removeBtnW, chipY, removeBtnW, chipH);
        layout.visibleChips.push_back(MultiChipBox{
            .selectedVecIdx = validEntries[i].selIdx,
            .itemIdx = validEntries[i].itemIdx,
            .chipRect = cRect,
            .removeRect = rRect
        });
        curX += w + chipGap;
    }

    if (layout.overflowCount > 0) {
        layout.overflowRect = Rect(curX, chipY, badgeW, chipH);
        curX += badgeW + chipGap;
    }

    layout.afterChipsX = curX;
    return layout;
}

void RenderMultiSelectNode(
    ICanvas& canvas,
    Rect bounds,
    const SelectData& data,
    UiSize size,
    bool hovered,
    bool open,
    bool disabled,
    const UiTheme& theme,
    bool focused
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const SelectOptions& opts = data.opts;
    const SelectMetrics& m = theme.GetSelect(size);
    const Rect boxRect = bounds;

    const auto& pal = theme.palette;
    const Color primaryCol = ResolvePrimaryColor(pal);

    Color placeholderCol = theme.ResolveTextColor(opts.placeholder, UiVariant::Muted, disabled);
    Color chevronCol = open ? FieldFrame::ResolveAccent(theme, opts.status, disabled) : ((data.chevronHovered && !disabled) ? pal.textPrimary : pal.textSecondary);
    Color unitCol = disabled ? pal.textSecondary.WithAlphaF(theme.disabledOpacity) : pal.textSecondary;
    if (disabled) {
        chevronCol = chevronCol.WithAlphaF(theme.disabledOpacity);
    }

    FieldFrame::Draw(canvas, theme, FieldFrameParams{
        .rect = boxRect,
        .style = opts.style,
        .status = opts.status,
        .background = opts.background,
        .border = opts.border,
        .defaultRadius = m.cornerRadius,
        .defaultStrokeWidth = m.strokeWidth,
        .hovered = hovered,
        .focused = focused,
        .active = open,
        .disabled = disabled
    });

    // Left prefix
    keel::f32 leftX = boxRect.x + m.paddingX;
    if (opts.prefix.HasContentOrReserve()) {
        const keel::f32 pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const Color pTextCol = theme.ResolveTextColor(opts.prefix.text, UiVariant::Secondary, disabled, hovered, open || focused);
        const Color pIconCol = opts.prefix.text.color.HasAny() ? pTextCol : chevronCol;
        const keel::f32 pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, &canvas);
        TextUtil::DrawAffix(canvas, opts.prefix, Rect(leftX, boxRect.y, pW, boxRect.h), pFont, m.iconSize, pTextCol, pIconCol, 4.0f);
        leftX += pW + 6.0f;
    }

    // Right elements (chevron + clear button + suffix)
    const keel::f32 btnSize = m.iconSize + 4.0f;
    const Icon chevron = open ? Icon::ChevronUp() : Icon::ChevronDown();
    keel::f32 rightX = FieldFrame::DrawTrigger(
        canvas, theme, boxRect, chevron,
        m.paddingX, m.iconSize, m.cornerRadius, opts.border.radius.has_value(),
        data.chevronHovered, false, open, disabled, opts.searchable, chevronCol
    );

    if (data.showClearBtn) {
        rightX -= btnSize;
        const Rect clearBtnRect(rightX, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        if (data.clearHovered && !disabled) {
            canvas.DrawRoundRect(clearBtnRect, 4.0f, pal.surfaceHover, Color::Transparent(), 0.0f);
        }
        const keel::f32 clearIconSz = keel::max(8.0f, m.iconSize - 2.0f);
        const Rect icRect(
            rightX + (btnSize - clearIconSz) * 0.5f,
            boxRect.y + (boxRect.h - clearIconSz) * 0.5f,
            clearIconSz,
            clearIconSz
        );
        const Color icCol = (data.clearHovered && !disabled) ? pal.textPrimary : unitCol;
        Icon::Close().Draw(canvas, icRect, icCol);
        rightX -= 4.0f;
    }

    if (opts.suffix.HasContentOrReserve()) {
        const keel::f32 sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
        const Color sTextCol = theme.ResolveTextColor(opts.suffix.text, UiVariant::Muted, disabled, hovered, open || focused);
        const Color sIconCol = opts.suffix.text.color.HasAny() ? sTextCol : unitCol;
        const keel::f32 sW = TextUtil::MeasureAffixWidth(opts.suffix, sFont, m.iconSize, 4.0f, &canvas);
        rightX -= sW;
        TextUtil::DrawAffix(canvas, opts.suffix, Rect(rightX, boxRect.y, sW, boxRect.h), sFont, m.iconSize, sTextCol, sIconCol, 4.0f);
        rightX -= 6.0f;
    }

    const keel::f32 availW = keel::max(0.0f, rightX - leftX);
    const Rect textRect(leftX, boxRect.y, availW, boxRect.h);
    canvas.PushClip(textRect);

    const keel::f32 chipH = keel::max(14.0f, boxRect.h - 8.0f);
    const keel::f32 chipY = boxRect.y + (boxRect.h - chipH) * 0.5f;
    const keel::f32 chipFont = keel::max(10.0f, theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, size));
    const keel::f32 chipIconSz = keel::max(10.0f, m.iconSize - 2.0f);
    const keel::f32 removeIconSz = keel::max(8.0f, m.iconSize - 4.0f);
    const keel::f32 removeBtnW = removeIconSz + 6.0f;
    const keel::f32 chipGap = 4.0f;

    const Color chipBg = disabled ? primaryCol.WithAlphaF(0.08f) : primaryCol.WithAlphaF(0.16f);
    const Color chipBorder = disabled ? primaryCol.WithAlphaF(0.20f) : primaryCol.WithAlphaF(0.40f);
    const Color chipTextCol = disabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary;

    keel::f32 curX = textRect.x;
    for (keel::usize i = 0; i < data.chipCount && i < SelectData::kMaxChips; ++i) {
        const SelectChipVisual& chip = data.chips[i];
        const keel::f32 textW = TextUtil::MeasureUiTextWidth(chip.text, chipFont, &canvas);
        const keel::f32 iconW = (!chip.prefix.icon.IsEmpty()) ? (chipIconSz + 4.0f) : 0.0f;
        keel::f32 w = 6.0f + iconW + textW + 4.0f + removeBtnW;
        if (data.chipCount == 1 && data.totalSelectedCount == 1 && w > availW) {
            w = availW;
        }

        const Rect cRect(curX, chipY, w, chipH);
        canvas.DrawRoundRect(cRect, 4.0f, chipBg, chipBorder, 1.0f);

        keel::f32 innerX = curX + 6.0f;
        if (!chip.prefix.icon.IsEmpty()) {
            const Rect icRect(innerX, chipY + (chipH - chipIconSz) * 0.5f, chipIconSz, chipIconSz);
            chip.prefix.icon.Draw(canvas, icRect, disabled ? primaryCol.WithAlphaF(0.40f) : primaryCol);
            innerX += chipIconSz + 4.0f;
        }

        const keel::f32 maxTxtW = keel::max(0.0f, (curX + w - removeBtnW - 2.0f) - innerX);
        const Rect chipTxtRect(innerX, chipY, maxTxtW, chipH);
        TextUtil::DrawUiText(canvas, chip.text, chipTxtRect, chipFont, chipH, chipTextCol, 0.0f, 0.5f, &theme);

        // Draw chip remove (X) button
        const Rect remBtnRect(curX + w - removeBtnW, chipY, removeBtnW, chipH);
        if (chip.removeHovered && !disabled) {
            canvas.DrawRoundRect(remBtnRect, 3.0f, primaryCol.WithAlphaF(0.28f), Color::Transparent(), 0.0f);
        }
        const Rect remIcRect(
            remBtnRect.x + (remBtnRect.w - removeIconSz) * 0.5f,
            chipY + (chipH - removeIconSz) * 0.5f,
            removeIconSz,
            removeIconSz
        );
        const Color remIcCol = (chip.removeHovered && !disabled)
            ? pal.textPrimary
            : (disabled ? pal.textSecondary.WithAlphaF(0.40f) : pal.textSecondary);
        Icon::Close().Draw(canvas, remIcRect, remIcCol);

        curX += w + chipGap;
    }

    if (data.totalSelectedCount > data.chipCount) {
        const keel::usize overflowN = data.totalSelectedCount - data.chipCount;
        char badgeBuf[32]{};
        const keel::StringRef badgeStr = keel::format_buf(badgeBuf, "+%zu", overflowN);
        const keel::f32 bw = keel::min(keel::max(0.0f, textRect.Right() - curX), TextUtil::MeasureWidth(badgeStr, chipFont, &canvas) + 12.0f);
        if (bw > 8.0f) {
            const Rect bRect(curX, chipY, bw, chipH);
            canvas.DrawRoundRect(bRect, 4.0f, primaryCol.WithAlphaF(0.22f), chipBorder, 1.0f);
            canvas.DrawTextBox(badgeStr, bRect, chipFont, chipTextCol, 0.5f, 0.5f);
            curX += bw + chipGap;
        }
    }

    // Search filter text or placeholder when empty
    const keel::StringRef displayText = data.displayText.text;
    if (data.isEditing) {
        const keel::f32 inputFont = theme.ResolveFontSize(data.displayText, TextRole::LabelMedium, size);
        const Color textCol = disabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary;
        const Rect editRect(curX, boxRect.y, keel::max(0.0f, textRect.Right() - curX), boxRect.h);

        if (displayText.empty() && data.totalSelectedCount == 0 && !opts.placeholder.empty()) {
            const keel::f32 phFont = !opts.placeholder.size.IsAuto()
                ? theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size)
                : inputFont;
            canvas.DrawTextBox(opts.placeholder.text, editRect, phFont, placeholderCol, 0.0f, 0.5f);
        } else if (!displayText.empty()) {
            const keel::f32 fullW = keel::max(editRect.w, canvas.MeasureText(displayText, inputFont).x + 8.0f);
            const Rect scrolledRect(editRect.x - data.scrollX, editRect.y, fullW, editRect.h);
            canvas.DrawTextBox(displayText, scrolledRect, inputFont, textCol, 0.0f, 0.5f);
        }

        if (!disabled && data.cursorVisible) {
            const keel::usize safeCursor = keel::min(data.cursor, displayText.size());
            const keel::StringRef sub = displayText.substr(0, safeCursor);
            const keel::f32 subW = canvas.MeasureText(sub, inputFont).x;
            const keel::f32 caretX = editRect.x - data.scrollX + subW;
            const keel::f32 caretH = inputFont * 1.15f;
            const keel::f32 caretY = boxRect.y + (boxRect.h - caretH) * 0.5f;
            canvas.DrawLine({caretX, caretY}, {caretX, caretY + caretH}, pal.caret, data.cursorWidth);
        }
    } else if (data.totalSelectedCount == 0 && !opts.placeholder.empty()) {
        const keel::f32 phFont = theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size);
        const keel::f32 alignX = (opts.align == Align::End) ? 1.0f : ((opts.align == Align::Center) ? 0.5f : 0.0f);
        TextUtil::DrawUiText(canvas, opts.placeholder, textRect, phFont, boxRect.h, placeholderCol, alignX, 0.5f, &theme);
    }

    canvas.PopClip();
}

void RenderMultiPopup(
    ICanvas& canvas,
    Rect popupRect,
    keel::ArrayRef<SelectItem> items,
    keel::ArrayRef<keel::u8> itemSelectedFlags,
    keel::i32 hoveredIndex,
    const SelectOptions& opts,
    UiSize size,
    const UiTheme& theme,
    keel::f32 scrollY,
    bool scrollbarHovered,
    bool scrollbarActive,
    keel::StringRef highlightQuery
) {
    if (popupRect.w <= 0.0f || popupRect.h <= 0.0f || (items.empty() && !opts.searchable)) {
        return;
    }

    const SelectMetrics& m = theme.GetSelect(size);
    const ScrollbarMetrics& sm = theme.GetScrollbar(size);
    const auto& pal = theme.palette;
    const Color primaryCol = ResolvePrimaryColor(pal);

    theme.Painter().DrawDropdownSurface(canvas, theme, popupRect, m.cornerRadius);

    if (items.empty()) {
        const Rect emptyRect(popupRect.x + m.paddingX, popupRect.y + 4.0f, keel::max(0.0f, popupRect.w - m.paddingX * 2.0f), keel::max(0.0f, popupRect.h - 8.0f));
        canvas.PushClip(emptyRect);
        const UiText emptyTxt = opts.emptyText.empty() ? UiText{"No matching items"} : opts.emptyText;
        const keel::f32 emptyFont = theme.ResolveFontSize(emptyTxt, TextRole::LabelMedium, size);
        const Color emptyCol = theme.ResolveTextColor(emptyTxt, UiVariant::Muted, false);
        TextUtil::DrawUiText(canvas, emptyTxt, emptyRect, emptyFont, emptyRect.h, emptyCol, 0.0f, 0.5f, &theme);
        canvas.PopClip();
        return;
    }

    const keel::f32 viewportH = keel::max(0.0f, popupRect.h - 8.0f);
    const keel::f32 totalContentH = keel::as<keel::f32>(items.size()) * m.itemHeight;
    const keel::f32 maxScrollY = keel::max(0.0f, totalContentH - viewportH);
    const bool hasScrollbar = (maxScrollY > 0.5f);
    const keel::f32 clampedScrollY = keel::clamp(scrollY, 0.0f, maxScrollY);
    const keel::f32 itemW = keel::max(0.0f, popupRect.w - 8.0f - (hasScrollbar ? sm.gutter : 0.0f));
    const Rect itemsClipRect(popupRect.x + 1.0f, popupRect.y + 4.0f, keel::max(0.0f, popupRect.w - 2.0f), viewportH);

    canvas.PushClip(itemsClipRect);

    const keel::f32 cbSize = 14.0f;

    for (keel::usize i = 0; i < items.size(); ++i) {
        const SelectItem& item = items[i];
        const Rect itemRect(
            popupRect.x + 4.0f,
            popupRect.y + 4.0f + keel::as<keel::f32>(i) * m.itemHeight - clampedScrollY,
            itemW,
            m.itemHeight
        );

        if (itemRect.Bottom() <= itemsClipRect.y || itemRect.y >= itemsClipRect.Bottom()) {
            continue;
        }

        const bool itemDisabled = item.disabled;
        const bool isSelected = (i < itemSelectedFlags.size()) && (itemSelectedFlags[i] != 0);
        const bool isHovered = (keel::as<keel::i32>(i) == hoveredIndex) && !itemDisabled;

        if (isHovered) {
            canvas.DrawRoundRect(itemRect, 4.0f, isSelected ? pal.surfaceActive : pal.surfaceHover);
        } else if (isSelected) {
            canvas.DrawRoundRect(itemRect, 4.0f, primaryCol.WithAlphaF(0.08f));
        }

        Color itemTextCol = item.text.color.HasAny()
            ? theme.ResolveTextColor(item.text, UiVariant::Default, itemDisabled, isHovered, isSelected)
            : (itemDisabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary);

        // Right side: item suffix
        keel::f32 textRightX = itemRect.x + itemRect.w - m.paddingX;
        if (item.suffix.HasContentOrReserve()) {
            const keel::f32 sFont = theme.ResolveFontSize(item.suffix.text, TextRole::LabelMedium, size);
            const Color sTextCol = theme.ResolveTextColor(item.suffix.text, UiVariant::Muted, itemDisabled);
            const keel::f32 sW = TextUtil::MeasureAffixWidth(item.suffix, sFont, m.iconSize, 4.0f, &canvas);
            textRightX -= sW;
            TextUtil::DrawAffix(canvas, item.suffix, Rect(textRightX, itemRect.y, sW, itemRect.h), sFont, m.iconSize, sTextCol, sTextCol, 4.0f);
            textRightX -= 6.0f;
        }

        // Left side: 14x14 rounded checkbox + optional prefix + main text
        keel::f32 textLeftX = itemRect.x + m.paddingX;
        const Rect cbRect(textLeftX, itemRect.y + (itemRect.h - cbSize) * 0.5f, cbSize, cbSize);
        if (isSelected) {
            const Color cbFill = itemDisabled ? primaryCol.WithAlphaF(0.40f) : primaryCol;
            canvas.DrawRoundRect(cbRect, 3.5f, cbFill, cbFill, 1.0f);
            const Rect checkRect(cbRect.x + 2.0f, cbRect.y + 2.0f, cbSize - 4.0f, cbSize - 4.0f);
            Icon::Check().Draw(canvas, checkRect, Color(255, 255, 255, 255));
        } else {
            const Color cbBorder = isHovered ? pal.borderHover : pal.border;
            canvas.DrawRoundRect(cbRect, 3.5f, pal.surface, cbBorder, 1.0f);
        }
        textLeftX += cbSize + 8.0f;

        if (item.prefix.HasContentOrReserve()) {
            const keel::f32 pFont = theme.ResolveFontSize(item.prefix.text, TextRole::LabelMedium, size);
            const Color pTextCol = theme.ResolveTextColor(item.prefix.text, UiVariant::Secondary, itemDisabled);
            const keel::f32 pW = TextUtil::MeasureAffixWidth(item.prefix, pFont, m.iconSize, 4.0f, &canvas);
            TextUtil::DrawAffix(canvas, item.prefix, Rect(textLeftX, itemRect.y, pW, itemRect.h), pFont, m.iconSize, pTextCol, pTextCol, 4.0f);
            textLeftX += pW + 6.0f;
        }

        const keel::f32 selFont = theme.ResolveFontSize(item.text, TextRole::LabelMedium, size);
        const Rect itemTextRect(textLeftX, itemRect.y, keel::max(0.0f, textRightX - textLeftX), itemRect.h);

        if (!highlightQuery.empty() && !item.text.empty()) {
            keel::usize matchPos = 0;
            keel::usize matchLen = 0;
            if (TextUtil::FindCaseInsensitive(item.text.text, highlightQuery, matchPos, matchLen) && matchLen > 0) {
                TextUtil::DrawHighlightedTextBox(
                    canvas,
                    item.text.text,
                    itemTextRect,
                    selFont,
                    itemTextCol,
                    matchPos,
                    matchLen,
                    primaryCol.WithAlpha(68),
                    Color::Transparent(),
                    2.0f,
                    0.5f
                );
                continue;
            }
        }

        TextUtil::DrawUiText(canvas, item.text, itemTextRect, selFont, itemRect.h, itemTextCol, 0.0f, 0.5f, &theme);
    }

    canvas.PopClip();

    if (hasScrollbar && popupRect.h > 10.0f) {
        canvas.PushClip(popupRect);
        const ScrollbarGeometry sbGeom = Scrollbar::ComputeAxis(
            popupRect,
            popupRect.h + maxScrollY,
            clampedScrollY,
            false,
            sm,
            theme.scrollbarStyle
        );
        if (sbGeom.visible) {
            Scrollbar::DrawAxis(
                canvas,
                sbGeom,
                false,
                scrollbarHovered,
                scrollbarActive,
                scrollbarHovered ? ScrollbarPart::Thumb : ScrollbarPart::None,
                scrollbarActive ? ScrollbarPart::Thumb : ScrollbarPart::None,
                sm,
                theme,
                primaryCol
            );
        }
        canvas.PopClip();
    }
}

} // namespace

Response detail::Select::RunMulti(
    Context& ctx,
    IndexListSink selectedIndices,
    keel::ArrayRef<keel::StringRef> items,
    const SelectOptions& opts,
    keel::SourceLocation loc
) {
    keel::SmallArray<SelectItem, 32> richItems;
    richItems.reserve(items.size());
    for (keel::StringRef sv : items) {
        richItems.push_back(SelectItem{.text = sv});
    }
    return RunMulti(ctx, selectedIndices, richItems, opts, loc);
}

Response detail::Select::RunMulti(
    Context& ctx,
    IndexListSink selectedIndices,
    keel::ArrayRef<SelectItem> items,
    const SelectOptions& opts,
    keel::SourceLocation loc
) {

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool isSearchable = opts.searchable;

    const bool wasOpen = ctx.IsPopupOpen(id);

    ctx.RegisterFocusable(id, effectiveDisabled);

    Response res = ctx.Interact(id);
    const bool mouseJustPressed = res.hovered && ctx.IsMouseClicked() && !effectiveDisabled;

    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    } else if (res.clicked || mouseJustPressed) {
        ctx.RequestFocus(id);
    }

    const UiTheme theme = ctx.GetTheme();
    const SelectMetrics& m = theme.GetSelect(activeSize);
    const ScrollbarMetrics& sm = theme.GetScrollbar(activeSize);
    ICanvas* canvas = ctx.GetCanvas();

    SelectOptions optsCopy = opts;
    const SizeMode effWidthMode = Control::ResolveWidthMode(ctx, opts.width, SizeMode::Stretch);
    if (optsCopy.width.IsAuto()) {
        optsCopy.width = effWidthMode;
    }

    auto measureRowContentW = [&](const UiText& mainText, const UiAffix& pfx, const UiAffix& sfx) -> keel::f32 {
        const keel::f32 mainFont = theme.ResolveFontSize(mainText, TextRole::LabelMedium, activeSize);
        keel::f32 w = TextUtil::MeasureUiTextWidth(mainText, mainFont, canvas);
        if (pfx.HasContentOrReserve()) {
            const keel::f32 pFont = theme.ResolveFontSize(pfx.text, TextRole::LabelMedium, activeSize);
            w += TextUtil::MeasureAffixWidth(pfx, pFont, m.iconSize, 4.0f, canvas) + 6.0f;
        }
        if (sfx.HasContentOrReserve()) {
            const keel::f32 sFont = theme.ResolveFontSize(sfx.text, TextRole::LabelMedium, activeSize);
            w += TextUtil::MeasureAffixWidth(sfx, sFont, m.iconSize, 4.0f, canvas) + 6.0f;
        }
        return w;
    };

    keel::f32 maxRowW = 0.0f;
    for (const SelectItem& it : items) {
        maxRowW = keel::max(maxRowW, measureRowContentW(it.text, it.prefix, it.suffix) + 22.0f);
    }
    if (optsCopy.placeholder.HasContentOrReserve()) {
        maxRowW = keel::max(maxRowW, measureRowContentW(optsCopy.placeholder, optsCopy.prefix, optsCopy.suffix));
    }

    const bool exceedsVisibleItems = optsCopy.maxVisibleItems.has_value()
        && *optsCopy.maxVisibleItems > 0
        && items.size() > keel::as<keel::usize>(*optsCopy.maxVisibleItems);

    const keel::f32 clearExtraW = optsCopy.clearable ? (m.iconSize + 8.0f) : 0.0f;
    keel::f32 intrinsicW = maxRowW + m.paddingX * 2.0f + m.iconSize + 8.0f + clearExtraW + (exceedsVisibleItems ? sm.gutter : 0.0f);
    if (optsCopy.minWidth.has_value()) {
        intrinsicW = keel::max(*optsCopy.minWidth, intrinsicW);
    } else {
        intrinsicW = keel::max(m.minWidth, intrinsicW);
    }
    if (optsCopy.maxWidth.has_value() && *optsCopy.maxWidth > 0.0f) {
        intrinsicW = keel::min(*optsCopy.maxWidth, intrinsicW);
    }
    if (optsCopy.width.IsFixed()) {
        intrinsicW = optsCopy.width.Dp();
    }

    const ControlSpec ctrl = EffectiveSelectControl(optsCopy, activeSize, theme);
    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    Vec2 fallbackDims = detail::Select::Measure(optsCopy, activeSize, canvas, theme);
    if (effWidthMode == SizeMode::Fit && !optsCopy.width.IsFixed()) {
        fallbackDims.x = intrinsicW;
    }
    const Rect boxRect = (prevBounds.w > 0.0f && prevBounds.h > 0.0f)
        ? prevBounds
        : Rect({0.0f, 0.0f}, fallbackDims);

    struct MultiSelectPopupState {
        keel::f32 scrollY{0.0f};
        bool draggingScrollbar{false};
        keel::f32 dragStartMouseY{0.0f};
        keel::f32 dragStartScrollY{0.0f};
        bool scrollbarHovered{false};
        bool scrollbarActive{false};
        keel::SmallString<64> filterText{};
        keel::f32 scrollX{0.0f};
        bool actionBtnDown{false};
        keel::i32 highlightedFilteredIdx{-1};
    };
    auto& popupState = ctx.GetOrCreateState<MultiSelectPopupState>(id);
    if (popupState.actionBtnDown) {
        res.clicked = false;
    }
    if (!res.pressed && !mouseJustPressed) {
        popupState.actionBtnDown = false;
    }

    if (!wasOpen && !popupState.filterText.empty() && !ctx.HasActiveTextEdit(id)) {
        popupState.filterText.clear();
        popupState.scrollX = 0.0f;
        popupState.highlightedFilteredIdx = -1;
    }

    auto hasValidSelections = [&]() -> bool {
        for (keel::usize idx : selectedIndices) {
            if (idx < items.size()) return true;
        }
        return false;
    };

    auto hasValueToClear = [&]() -> bool {
        if (!opts.clearable || effectiveDisabled) return false;
        if (hasValidSelections()) return true;
        if (wasOpen && isSearchable && !popupState.filterText.empty()) return true;
        return false;
    };

    bool showClearBtn = hasValueToClear();
    SelectBoxRects rects = ResolveSelectBoxRects(boxRect, opts.prefix, opts.suffix, activeSize, canvas, theme, showClearBtn);
    const Vec2 mousePos = ctx.GetMousePos();
    bool clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);
    bool chevronHovered = res.hovered && rects.chevronRect.Contains(mousePos);

    const keel::f32 inputFont = theme.ResolveFontSize(optsCopy.placeholder, TextRole::LabelMedium, activeSize);
    const keel::f32 reserveW = (wasOpen && isSearchable && !popupState.filterText.empty())
        ? keel::min(rects.textRect.w * 0.45f, TextUtil::MeasureWidth(popupState.filterText, inputFont, canvas) + 16.0f)
        : 0.0f;

    MultiChipLayout chipLayout = ComputeMultiChipLayout(
        rects.textRect,
        boxRect.h,
        selectedIndices.as_ref(),
        items,
        activeSize,
        canvas,
        theme,
        reserveW
    );

    keel::i32 hoveredChipRemove = -1;
    if (res.hovered && !effectiveDisabled) {
        for (keel::usize ci = 0; ci < chipLayout.visibleChips.size(); ++ci) {
            if (chipLayout.visibleChips[ci].removeRect.Contains(mousePos)) {
                hoveredChipRemove = keel::as<keel::i32>(ci);
                break;
            }
        }
    }

    // Handle individual chip remove (X) button click without toggling dropdown
    if (mouseJustPressed && hoveredChipRemove >= 0) {
        const keel::usize vecIdx = chipLayout.visibleChips[hoveredChipRemove].selectedVecIdx;
        if (vecIdx < selectedIndices.size()) {
            selectedIndices.erase_at(vecIdx);
            res.changed = true;
        }
        popupState.actionBtnDown = true;
        res.clicked = false;
        hoveredChipRemove = -1;
        showClearBtn = hasValueToClear();
        rects = ResolveSelectBoxRects(boxRect, opts.prefix, opts.suffix, activeSize, canvas, theme, showClearBtn);
        clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);
        chevronHovered = res.hovered && rects.chevronRect.Contains(mousePos);
        chipLayout = ComputeMultiChipLayout(rects.textRect, boxRect.h, selectedIndices.as_ref(), items, activeSize, canvas, theme, reserveW);
    } else if (mouseJustPressed && clearHovered) {
        popupState.actionBtnDown = true;
        popupState.scrollX = 0.0f;
        popupState.filterText.clear();
        popupState.highlightedFilteredIdx = -1;
        if (ctx.HasActiveTextEdit(id)) {
            ctx.SetTextEditBuffer(id, "");
        }
        if (!selectedIndices.empty()) {
            selectedIndices.clear();
            res.changed = true;
        }
        showClearBtn = false;
        clearHovered = false;
        rects = ResolveSelectBoxRects(boxRect, opts.prefix, opts.suffix, activeSize, canvas, theme, showClearBtn);
        chipLayout = ComputeMultiChipLayout(rects.textRect, boxRect.h, selectedIndices.as_ref(), items, activeSize, canvas, theme, 0.0f);
        res.clicked = false;
    }

    const bool isFocused = ctx.IsFocused(id) && !effectiveDisabled;
    res.focused = isFocused;

    // Backspace when filterText is empty removes the last selected chip
    if (!effectiveDisabled && (wasOpen || isFocused) && popupState.filterText.empty() && !selectedIndices.empty()) {
        if (ctx.ConsumeKey(Key::Backspace)) {
            selectedIndices.erase_at(selectedIndices.size() - 1);
            res.changed = true;
            showClearBtn = hasValueToClear();
            rects = ResolveSelectBoxRects(boxRect, opts.prefix, opts.suffix, activeSize, canvas, theme, showClearBtn);
            chipLayout = ComputeMultiChipLayout(rects.textRect, boxRect.h, selectedIndices.as_ref(), items, activeSize, canvas, theme, 0.0f);
        }
    }

    keel::usize cursor = 0;
    keel::usize selAnchor = 0;
    bool editChanged = false;
    bool editSubmitted = false;
    bool isEditingText = false;
    keel::StringRef editDisplayText{};

    if (!effectiveDisabled && isSearchable && (wasOpen || isFocused || ctx.HasActiveTextEdit(id))) {
        keel::StringRef currentEditText = ctx.EnsureTextEdit(id, popupState.filterText.as_ref(), /*multiline=*/false, /*readOnly=*/false);
        editDisplayText = ctx.GetOrStartTextEdit(
            id,
            currentEditText,
            cursor,
            selAnchor,
            editChanged,
            editSubmitted,
            /*multiline=*/false,
            /*readOnly=*/false
        );
        popupState.filterText.assign(editDisplayText);
        if (editChanged) {
            popupState.highlightedFilteredIdx = 0;
        }
    }

    bool isOpen = wasOpen;
    bool wantOpen = false;
    bool wantClose = false;

    if (!effectiveDisabled) {
        if (!wasOpen) {
            if (res.clicked && !popupState.actionBtnDown) {
                wantOpen = true;
            } else if (isFocused) {
                if (ctx.ConsumeKey(Key::Down) || (popupState.filterText.empty() && ctx.ConsumeKey(Key::Space))) {
                    wantOpen = true;
                    res.clicked = true;
                } else if (isSearchable && editChanged && !popupState.filterText.empty()) {
                    wantOpen = true;
                } else if (editSubmitted || (!isSearchable && ctx.ConsumeKey(Key::Enter))) {
                    wantOpen = true;
                    editSubmitted = false;
                    ctx.RequestFocus(id);
                }
            }
        } else {
            if (res.clicked && !popupState.actionBtnDown) {
                if (!isSearchable || chevronHovered || popupState.filterText.empty()) {
                    wantClose = true;
                }
            } else if (isSearchable && editChanged) {
                isOpen = true;
            }
        }
    }

    if (wantClose) {
        ctx.ClosePopup();
        isOpen = false;
        popupState.draggingScrollbar = false;
        popupState.scrollbarHovered = false;
        popupState.scrollbarActive = false;
        if (isSearchable) {
            popupState.filterText.clear();
            popupState.scrollX = 0.0f;
            ctx.SetTextEditBuffer(id, "");
        }
    } else if (wantOpen && (!items.empty() || isSearchable)) {
        isOpen = true;
        ctx.RequestFocus(id);
        popupState.draggingScrollbar = false;
        popupState.scrollbarHovered = false;
        popupState.scrollbarActive = false;
        if (isSearchable && !editChanged) {
            popupState.filterText.clear();
            popupState.scrollX = 0.0f;
            ctx.SetTextEditBuffer(id, "");
            editDisplayText = "";
            cursor = 0;
            selAnchor = 0;
        }
        popupState.highlightedFilteredIdx = 0;
    }

    if (isSearchable) {
        isEditingText = isOpen;
    }

    const keel::StringRef activeQuery = (isOpen && isSearchable)
        ? popupState.filterText.as_ref()
        : keel::StringRef{};

    keel::SmallArray<keel::usize, 64> filteredIndices;
    filteredIndices.reserve(items.size());
    for (keel::usize i = 0; i < items.size(); ++i) {
        if (MatchesSelectFilter(items[i], activeQuery)) {
            filteredIndices.push_back(i);
        }
    }

    if (isOpen) {
        if (popupState.highlightedFilteredIdx < 0 ||
            popupState.highlightedFilteredIdx >= keel::as<keel::i32>(filteredIndices.size())) {
            popupState.highlightedFilteredIdx = -1;
            for (keel::usize fi = 0; fi < filteredIndices.size(); ++fi) {
                if (!items[filteredIndices[fi]].disabled) {
                    popupState.highlightedFilteredIdx = keel::as<keel::i32>(fi);
                    break;
                }
            }
        }
    }

    const keel::usize effectiveRowCount = filteredIndices.empty()
        ? (isSearchable ? 1 : 0)
        : filteredIndices.size();
    const keel::f32 totalContentH = keel::as<keel::f32>(effectiveRowCount) * m.itemHeight;

    auto scrollItemIntoView = [&](keel::usize filteredPos, keel::f32 viewportH, keel::f32 maxScrollY) {
        if (filteredPos >= filteredIndices.size() || maxScrollY <= 0.0f || viewportH <= 0.0f) {
            return;
        }
        const keel::f32 itemTop = keel::as<keel::f32>(filteredPos) * m.itemHeight;
        const keel::f32 itemBottom = itemTop + m.itemHeight;
        if (itemTop < popupState.scrollY) {
            popupState.scrollY = itemTop;
        } else if (itemBottom > popupState.scrollY + viewportH) {
            popupState.scrollY = itemBottom - viewportH;
        }
        popupState.scrollY = keel::clamp(popupState.scrollY, 0.0f, maxScrollY);
    };

    auto computePopupRect = [&](keel::usize rowCount) -> Rect {
        const keel::f32 popupW = boxRect.w > 0.0f ? boxRect.w : intrinsicW;
        const keel::usize clampedRows = keel::max<keel::usize>(1, rowCount);
        const keel::usize visibleCount = (opts.maxVisibleItems.has_value() && *opts.maxVisibleItems > 0)
            ? keel::min(clampedRows, keel::as<keel::usize>(*opts.maxVisibleItems))
            : clampedRows;
        const keel::f32 contentH = keel::as<keel::f32>(visibleCount) * m.itemHeight + 8.0f;
        keel::f32 popupH = contentH;
        keel::f32 popupX = boxRect.x;
        keel::f32 popupY = boxRect.y + boxRect.h + 4.0f;

        const Rect vp = ctx.GetPopupViewportRect();
        if (vp.h > 0.0f) {
            const keel::f32 availDown = vp.Bottom() - popupY - 6.0f;
            const keel::f32 availUp = boxRect.y - 4.0f - (vp.y + 6.0f);
            if (popupY + popupH > vp.Bottom() - 6.0f && availUp > availDown) {
                popupY = keel::max(vp.y + 6.0f, boxRect.y - 4.0f - popupH);
                popupH = keel::clamp(boxRect.y - 4.0f - popupY, keel::min(m.itemHeight + 8.0f, vp.h - 8.0f), popupH);
            } else if (popupY + popupH > vp.Bottom() - 6.0f) {
                popupH = keel::max(keel::min(m.itemHeight + 8.0f, vp.h - 8.0f), availDown);
                popupY = keel::clamp(popupY, vp.y + 4.0f, keel::max(vp.y + 4.0f, vp.Bottom() - popupH - 4.0f));
            }
        }
        if (vp.w > 0.0f) {
            popupX = keel::clamp(popupX, vp.x + 4.0f, keel::max(vp.x + 4.0f, vp.Right() - popupW - 4.0f));
        }
        return Rect(popupX, popupY, popupW, popupH);
    };

    if (isOpen && effectiveRowCount > 0) {
        const Rect popupRect = computePopupRect(effectiveRowCount);
        ctx.OpenPopup(id, popupRect);
        if (!wasOpen) {
            const keel::f32 viewportH = keel::max(0.0f, popupRect.h - 8.0f);
            const keel::f32 maxScrollY = keel::max(0.0f, totalContentH - viewportH);
            popupState.scrollY = keel::clamp(popupState.scrollY, 0.0f, maxScrollY);
        }
    } else if (isOpen && effectiveRowCount == 0) {
        ctx.ClosePopup();
        isOpen = false;
    }

    auto toggleItemSelection = [&](keel::usize itemIdx) {
        if (itemIdx >= items.size() || items[itemIdx].disabled) return;
        for (keel::usize si = 0; si < selectedIndices.size(); ++si) {
            if (selectedIndices[si] == itemIdx) {
                selectedIndices.erase_at(si);
                res.changed = true;
                return;
            }
        }
        selectedIndices.push_back(itemIdx);
        res.changed = true;
    };

    keel::i32 hoveredItem = -1;
    if (isOpen && wasOpen && !effectiveDisabled) {
        auto hitScope = ctx.ScopedOverlayHitCheck(id);
        const Rect popupRect = ctx.GetOpenPopupRect();
        const Vec2 mouse = ctx.GetMousePos();
        const keel::f32 viewportH = keel::max(0.0f, popupRect.h - 8.0f);
        const keel::f32 maxScrollY = keel::max(0.0f, totalContentH - viewportH);
        const bool hasScrollbar = (maxScrollY > 0.5f);

        popupState.scrollY = keel::clamp(popupState.scrollY, 0.0f, maxScrollY);

        if (hasScrollbar && popupRect.Contains(mouse)) {
            const Vec2 wheel = ctx.GetMouseWheel();
            if (wheel.y != 0.0f || wheel.x != 0.0f) {
                popupState.scrollY = keel::clamp(popupState.scrollY - wheel.y * sm.scrollStep, 0.0f, maxScrollY);
                ctx.ConsumeMouseWheel();
            }
        }

        popupState.scrollbarHovered = false;
        if (hasScrollbar && popupRect.h > 10.0f) {
            const ScrollbarGeometry sbGeom = Scrollbar::ComputeAxis(
                popupRect,
                popupRect.h + maxScrollY,
                popupState.scrollY,
                false,
                sm,
                theme.scrollbarStyle
            );
            const bool overStart = sbGeom.hasButtons && sbGeom.startButtonRect.Contains(mouse);
            const bool overEnd   = sbGeom.hasButtons && sbGeom.endButtonRect.Contains(mouse);
            const bool overThumb = !overStart && !overEnd && sbGeom.hitThumbRect.Contains(mouse);
            const bool overTrack = sbGeom.hitTrackRect.Contains(mouse);
            popupState.scrollbarHovered = overStart || overEnd || overThumb || overTrack;

            if (ctx.IsMouseClicked() && popupState.scrollbarHovered) {
                if (overStart) {
                    popupState.scrollY = keel::max(0.0f, popupState.scrollY - sm.scrollStep);
                } else if (overEnd) {
                    popupState.scrollY = keel::min(maxScrollY, popupState.scrollY + sm.scrollStep);
                } else if (overThumb) {
                    popupState.draggingScrollbar = true;
                    popupState.dragStartMouseY = mouse.y;
                    popupState.dragStartScrollY = popupState.scrollY;
                    ctx.SetActiveId(id);
                } else {
                    if (mouse.y < sbGeom.thumbRect.y) {
                        popupState.scrollY = keel::max(0.0f, popupState.scrollY - viewportH * 0.8f);
                    } else if (mouse.y > sbGeom.thumbRect.Bottom()) {
                        popupState.scrollY = keel::min(maxScrollY, popupState.scrollY + viewportH * 0.8f);
                    }
                }
                ctx.ConsumeMouseClick();
            }

            if (popupState.draggingScrollbar) {
                if (ctx.IsMouseDown()) {
                    if (sbGeom.travel > 0.0f) {
                        const keel::f32 deltaY = mouse.y - popupState.dragStartMouseY;
                        popupState.scrollY = keel::clamp(
                            popupState.dragStartScrollY + (deltaY / sbGeom.travel) * maxScrollY,
                            0.0f,
                            maxScrollY
                        );
                    }
                } else {
                    popupState.draggingScrollbar = false;
                    if (ctx.GetActiveId() == id) {
                        ctx.SetActiveId(Id::Null());
                    }
                    ctx.ConsumeMouseClick();
                }
            }
        } else {
            popupState.draggingScrollbar = false;
        }
        popupState.scrollbarActive = popupState.draggingScrollbar;

        const keel::f32 itemW = keel::max(0.0f, popupRect.w - 8.0f - (hasScrollbar ? sm.gutter : 0.0f));
        const Rect itemsViewport(popupRect.x + 4.0f, popupRect.y + 4.0f, itemW, viewportH);

        if (!popupState.draggingScrollbar && !popupState.scrollbarHovered && itemsViewport.Contains(mouse)) {
            for (keel::usize fi = 0; fi < filteredIndices.size(); ++fi) {
                const keel::usize origIdx = filteredIndices[fi];
                const Rect itemRect(
                    popupRect.x + 4.0f,
                    popupRect.y + 4.0f + keel::as<keel::f32>(fi) * m.itemHeight - popupState.scrollY,
                    itemW,
                    m.itemHeight
                );
                if (itemRect.Bottom() <= itemsViewport.y || itemRect.y >= itemsViewport.Bottom()) {
                    continue;
                }
                if (itemRect.Contains(mouse)) {
                    if (!items[origIdx].disabled) {
                        hoveredItem = keel::as<keel::i32>(fi);
                        popupState.highlightedFilteredIdx = hoveredItem;
                    }
                    if (ctx.IsMouseReleased()) {
                        if (!items[origIdx].disabled) {
                            toggleItemSelection(origIdx);
                            ctx.RequestFocus(id);
                            // Keep popup open so multiple items can be toggled!
                        }
                        ctx.ConsumeMouseClick();
                        break;
                    }
                }
            }
        }

        if (isOpen) {
            if (ctx.ConsumeKey(Key::Escape)) {
                ctx.ClosePopup();
                isOpen = false;
                popupState.draggingScrollbar = false;
                if (isSearchable) {
                    popupState.filterText.clear();
                    ctx.SetTextEditBuffer(id, "");
                }
                ctx.RequestFocus(id);
            } else if (editSubmitted || ctx.ConsumeKey(Key::Enter) || (popupState.filterText.empty() && ctx.ConsumeKey(Key::Space))) {
                if (popupState.highlightedFilteredIdx >= 0 &&
                    popupState.highlightedFilteredIdx < keel::as<keel::i32>(filteredIndices.size())) {
                    const keel::usize chosen = filteredIndices[keel::as<keel::usize>(popupState.highlightedFilteredIdx)];
                    if (!items[chosen].disabled) {
                        toggleItemSelection(chosen);
                    }
                }
                ctx.RequestFocus(id);
                // Keep popup open on Enter/Space!
            } else if (ctx.ConsumeKey(Key::Down)) {
                if (!filteredIndices.empty()) {
                    const keel::usize startFi = (popupState.highlightedFilteredIdx < 0)
                        ? 0
                        : keel::as<keel::usize>(popupState.highlightedFilteredIdx + 1);
                    for (keel::usize fi = startFi; fi < filteredIndices.size(); ++fi) {
                        if (!items[filteredIndices[fi]].disabled) {
                            popupState.highlightedFilteredIdx = keel::as<keel::i32>(fi);
                            scrollItemIntoView(fi, viewportH, maxScrollY);
                            break;
                        }
                    }
                }
            } else if (ctx.ConsumeKey(Key::Up)) {
                if (!filteredIndices.empty() && popupState.highlightedFilteredIdx > 0) {
                    for (keel::usize fi = keel::as<keel::usize>(popupState.highlightedFilteredIdx); fi > 0; --fi) {
                        const keel::usize candidateFi = fi - 1;
                        if (!items[filteredIndices[candidateFi]].disabled) {
                            popupState.highlightedFilteredIdx = keel::as<keel::i32>(candidateFi);
                            scrollItemIntoView(candidateFi, viewportH, maxScrollY);
                            break;
                        }
                    }
                }
            }
        }
    } else if (!isOpen) {
        popupState.draggingScrollbar = false;
        popupState.scrollbarHovered = false;
        popupState.scrollbarActive = false;
    }

    // Recompute layout after any popup toggle changes
    showClearBtn = hasValueToClear();
    rects = ResolveSelectBoxRects(boxRect, opts.prefix, opts.suffix, activeSize, canvas, theme, showClearBtn);
    clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);
    chevronHovered = res.hovered && rects.chevronRect.Contains(mousePos);

    const keel::f32 finalReserveW = (isOpen && isSearchable && !popupState.filterText.empty())
        ? keel::min(rects.textRect.w * 0.45f, TextUtil::MeasureWidth(popupState.filterText.as_ref(), inputFont, canvas) + 16.0f)
        : 0.0f;
    chipLayout = ComputeMultiChipLayout(
        rects.textRect,
        boxRect.h,
        selectedIndices.as_ref(),
        items,
        activeSize,
        canvas,
        theme,
        finalReserveW
    );

    if (isOpen) {
        const Rect currentPopupRect = ctx.GetOpenPopupRect();
        const keel::f32 popupScrollY = popupState.scrollY;
        const bool sbHovered = popupState.scrollbarHovered;
        const bool sbActive = popupState.scrollbarActive;
        const keel::i32 effectiveHoveredIdx = (hoveredItem >= 0) ? hoveredItem : popupState.highlightedFilteredIdx;

        SelectItem* arenaPopupItems = ctx.GetArena().create_array<SelectItem>(filteredIndices.size());
        keel::u8* arenaSelectedFlags = ctx.GetArena().alloc_array<keel::u8>(filteredIndices.size());

        keel::u64 selectionHash = 0;
        for (keel::usize fi = 0; fi < filteredIndices.size(); ++fi) {
            const keel::usize origIdx = filteredIndices[fi];
            bool isSel = false;
            for (keel::usize selIdx : selectedIndices) {
                if (selIdx == origIdx) {
                    isSel = true;
                    break;
                }
            }
            arenaSelectedFlags[fi] = isSel ? 1 : 0;
            if (isSel) {
                selectionHash = detail::HashArgs(selectionHash, fi);
            }
            SelectItem copy = items[origIdx];
            copy.text = ctx.AllocText(copy.text);
            copy.prefix = ctx.AllocAffix(copy.prefix);
            copy.suffix = ctx.AllocAffix(copy.suffix);
            arenaPopupItems[fi] = copy;
        }
        const keel::ArrayRef<SelectItem> popupItemsSpan(arenaPopupItems, filteredIndices.size());
        const keel::ArrayRef<keel::u8> selectedFlagsSpan(arenaSelectedFlags, filteredIndices.size());

        const keel::StringRef highlightQueryStr = ctx.AllocString(activeQuery);
        ctx.SetOpenPopupVisualHash(detail::HashArgs(
            currentPopupRect.x,
            currentPopupRect.y,
            currentPopupRect.w,
            currentPopupRect.h,
            selectionHash,
            effectiveHoveredIdx,
            popupScrollY,
            sbHovered,
            sbActive,
            popupItemsSpan.size(),
            detail::HashStringView(highlightQueryStr)
        ));

        SelectOptions popupOpts = opts;
        popupOpts.emptyText = ctx.AllocText(opts.emptyText);

        ctx.RegisterOverlay([currentPopupRect, popupItemsSpan, selectedFlagsSpan, effectiveHoveredIdx, popupOpts, activeSize, theme, popupScrollY, sbHovered, sbActive, highlightQueryStr](ICanvas& canvas) {
            RenderMultiPopup(
                canvas,
                currentPopupRect,
                popupItemsSpan,
                selectedFlagsSpan,
                effectiveHoveredIdx,
                popupOpts,
                activeSize,
                theme,
                popupScrollY,
                sbHovered,
                sbActive,
                highlightQueryStr
            );
        });
    }

    if (res.hovered && !isOpen && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, prevBounds);
    }

    Node* node = ctx.CreateNode(NodeType::Select, id);

    optsCopy.placeholder = ctx.AllocText(optsCopy.placeholder);
    optsCopy.emptyText = ctx.AllocText(optsCopy.emptyText);
    optsCopy.tooltip = ctx.AllocText(optsCopy.tooltip);
    optsCopy.prefix = ctx.AllocAffix(opts.prefix);
    optsCopy.suffix = ctx.AllocAffix(opts.suffix);

    const bool finalFocused = isOpen || ctx.IsFocused(id);

    auto* data = ctx.AllocPayload<SelectData>();
    data->opts = optsCopy;
    data->displayText = ctx.AllocText(UiText{popupState.filterText.as_ref()});
    data->isEditing = isEditingText;
    data->cursor = cursor;
    data->selStart = keel::min(cursor, selAnchor);
    data->selEnd = keel::max(cursor, selAnchor);
    data->scrollX = popupState.scrollX;
    data->cursorVisible = isEditingText && finalFocused && !effectiveDisabled && ctx.IsCursorVisible(optsCopy.cursorMode);
    data->cursorWidth = ctx.GetCursorConfig().width;
    data->naturalWidth = (effWidthMode == SizeMode::Fit && !optsCopy.width.IsFixed())
        ? intrinsicW
        : 0.0f;
    data->showClearBtn = showClearBtn;
    data->clearHovered = clearHovered;
    data->chevronHovered = chevronHovered;
    data->isMulti = true;
    data->totalSelectedCount = chipLayout.totalValidCount;
    data->chipCount = keel::min(chipLayout.visibleChips.size(), SelectData::kMaxChips);

    keel::u64 chipsHash = detail::HashArgs(data->totalSelectedCount, data->chipCount);
    for (keel::usize i = 0; i < data->chipCount; ++i) {
        const MultiChipBox& cb = chipLayout.visibleChips[i];
        const SelectItem& it = items[cb.itemIdx];
        data->chips[i] = SelectChipVisual{
            .text = ctx.AllocText(it.text),
            .prefix = ctx.AllocAffix(it.prefix),
            .itemIndex = cb.itemIdx,
            .removeHovered = (keel::as<keel::i32>(i) == hoveredChipRemove)
        };
        chipsHash = detail::HashArgs(chipsHash, cb.itemIdx, data->chips[i].removeHovered);
    }

    node->text = optsCopy.placeholder.text;
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(optsCopy.align),
        optsCopy.width,
        optsCopy.height,
        optsCopy.minWidth.value_or(0.0f),
        optsCopy.maxWidth.value_or(0.0f),
        optsCopy.minHeight.value_or(0.0f),
        optsCopy.maxHeight.value_or(0.0f),
        optsCopy.searchable,
        optsCopy.clearable,
        data->displayText,
        data->isEditing,
        data->cursor,
        data->showClearBtn,
        data->clearHovered,
        data->chevronHovered,
        chipsHash,
        optsCopy.placeholder,
        optsCopy.prefix,
        optsCopy.suffix,
        keel::as<keel::u8>(optsCopy.style),
        keel::as<keel::u8>(optsCopy.status),
        optsCopy.background,
        optsCopy.border
    );
    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<SelectData>();
        Vec2 sz = detail::Select::Measure(d ? d->opts : SelectOptions{}, n->uiSize, canvas, theme, availableWidth);
        if (d && d->naturalWidth > 0.0f && !d->opts.width.IsFixed()) {
            sz.x = d->naturalWidth;
        }
        return sz;
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<SelectData>();
        if (!d) return;
        RenderMultiSelectNode(
            canvas,
            n->computedRect,
            *d,
            n->uiSize,
            n->hovered,
            n->active,
            n->disabled,
            theme,
            n->focused
        );
    };
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->active = isOpen;
    node->focused = isOpen || isEditingText || ctx.IsFocusVisible(id);
    res.focused = finalFocused;

    Control::SetupNode(ctx, node, ctrl);

    return res;
}

} // namespace guikit
