#include "guikit/widgets/select.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/small_array.h"
#include "keel/string.h"

namespace guikit {

namespace {

ControlSpec EffectiveSelectControl(const SelectOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& m = theme.GetSelect(size);
    ControlSpec c{};
    c.width = opts.width;
    c.height = opts.height;
    c.defaultWidthMode = SizeMode::Stretch;
    c.defaultHeightMode = SizeMode::Fit;
    c.minWidth = opts.minWidth;
    c.defaultLayout = ElementLayout::Stacked;
    c.labelGap = m.labelGap;
    c.descGap = m.descGap;
    c.inlineGap = m.paddingX;
    return c;
}

Vec2 MeasureSelectBox(const SelectOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& m = theme.GetSelect(size);
    const ControlSpec ctrl = EffectiveSelectControl(opts, size, theme);
    const bool hasExplicitDpW = ctrl.width.IsFixed();
    float boxW = hasExplicitDpW ? theme.Scale(ctrl.width.Dp()) : m.defaultWidth;
    if (ctrl.minWidth.has_value()) {
        boxW = keel::max(theme.Scale(*ctrl.minWidth), boxW);
    } else if (!hasExplicitDpW) {
        boxW = keel::max(m.minWidth, boxW);
    } else {
        boxW = keel::max(m.defaultHeight, boxW);
    }
    return {boxW, m.defaultHeight};
}

keel::usize SnapToUtf8CodepointBoundary(keel::StringRef text, keel::usize idx) noexcept {
    idx = keel::min(idx, text.size());
    while (idx > 0 && idx < text.size() && (static_cast<keel::u8>(text[idx]) & 0xC0) == 0x80) {
        --idx;
    }
    return idx;
}

bool FindCaseInsensitiveSubstring(
    keel::StringRef haystack,
    keel::StringRef needle,
    keel::usize& outBytePos,
    keel::usize& outByteLen
) noexcept {
    return TextUtil::FindCaseInsensitive(haystack, needle, outBytePos, outByteLen);
}

bool ContainsCaseInsensitive(keel::StringRef haystack, keel::StringRef needle) noexcept {
    keel::usize pos = 0;
    keel::usize len = 0;
    return FindCaseInsensitiveSubstring(haystack, needle, pos, len);
}

bool MatchesSelectFilter(const SelectItem& item, keel::StringRef query) noexcept {
    if (query.empty()) {
        return true;
    }
    if (ContainsCaseInsensitive(item.text.text, query)) {
        return true;
    }
    if (!item.prefix.text.empty() && ContainsCaseInsensitive(item.prefix.text.text, query)) {
        return true;
    }
    if (!item.suffix.text.empty() && ContainsCaseInsensitive(item.suffix.text.text, query)) {
        return true;
    }
    return false;
}

struct SelectBoxRects {
    Rect boxRect{};
    Rect textRect{};
    Rect clearRect{};
    Rect chevronRect{};
};

SelectBoxRects ResolveSelectBoxRects(
    Rect bounds,
    const UiAffix& activePrefix,
    const UiAffix& activeSuffix,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    bool showClearBtn
) noexcept {
    const SelectMetrics& m = theme.GetSelect(size);
    const Rect boxRect = bounds;

    float leftX = boxRect.x + m.paddingX;
    if (activePrefix.HasContentOrReserve()) {
        const float pFont = theme.ResolveFontSize(activePrefix.text, TextRole::LabelMedium, size);
        const float pW = TextUtil::MeasureAffixWidth(activePrefix, pFont, m.iconSize, 4.0f, canvas);
        leftX += pW + 6.0f;
    }

    const float btnSize = m.iconSize + 4.0f;
    float rightX = boxRect.x + boxRect.w - m.paddingX - m.iconSize;
    const Rect chevronRect(rightX - 2.0f, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
    rightX -= 6.0f;

    Rect clearRect{};
    if (showClearBtn) {
        rightX -= btnSize;
        clearRect = Rect(rightX, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        rightX -= 4.0f;
    }

    if (activeSuffix.HasContentOrReserve()) {
        const float sFont = theme.ResolveFontSize(activeSuffix.text, TextRole::LabelMedium, size);
        const float sW = TextUtil::MeasureAffixWidth(activeSuffix, sFont, m.iconSize, 4.0f, canvas);
        rightX -= sW + 6.0f;
    }

    const float availW = keel::max(0.0f, rightX - leftX);
    return SelectBoxRects{
        .boxRect = boxRect,
        .textRect = Rect(leftX, boxRect.y, availW, boxRect.h),
        .clearRect = clearRect,
        .chevronRect = chevronRect
    };
}

} // namespace

Rect Select::GetBoxRect(
    Rect bounds,
    const SelectOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const Vec2 boxDims = Select::Measure(opts, size, canvas, theme, bounds.w);
    const float alignFactor = (opts.align == Align::End) ? 1.0f : ((opts.align == Align::Center) ? 0.5f : 0.0f);
    float ctrlW = keel::min(bounds.w, boxDims.x);
    if (opts.width.ResolveMode(SizeMode::Stretch) == SizeMode::Stretch) {
        ctrlW = bounds.w;
    }
    const float ctrlX = bounds.x + (bounds.w - ctrlW) * alignFactor;
    const float ctrlH = keel::min(boxDims.y, bounds.h);
    const float ctrlY = bounds.y + (bounds.h - ctrlH) * 0.5f;
    return Rect(ctrlX, ctrlY, ctrlW, ctrlH);
}

Vec2 Select::Measure(
    const SelectOptions& opts,
    UiSize size,
    ICanvas* /*canvas*/,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const Vec2 boxDims = MeasureSelectBox(opts, size, theme);
    const ControlSpec ctrl = EffectiveSelectControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, theme);
}

Vec2 Select::Measure(
    const SelectOptions& opts,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme, availableWidth);
}

Response Select::Run(
    Context& ctx,
    keel::usize* selectedIndex,
    keel::i32* selectedIntIndex,
    keel::Optional<keel::StringSink> selectedString,
    keel::StringRef* selectedStringView,
    keel::ArrayRef<keel::StringRef> items,
    const SelectOptions& opts,
    keel::SourceLocation loc
) {
    keel::SmallArray<SelectItem, 32> richItems;
    richItems.reserve(items.size());
    for (keel::StringRef sv : items) {
        richItems.push_back(SelectItem{.text = sv});
    }
    return Run(ctx, selectedIndex, selectedIntIndex, selectedString, selectedStringView, richItems, opts, loc);
}

Response Select::Run(
    Context& ctx,
    keel::usize* selectedIndex,
    keel::i32* selectedIntIndex,
    keel::Optional<keel::StringSink> selectedString,
    keel::StringRef* selectedStringView,
    keel::ArrayRef<SelectItem> items,
    const SelectOptions& opts,
    keel::SourceLocation loc
) {
    const ZoneScope zoneScope = ctx.Zone({.theme = opts.theme, .scale = opts.scale});

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool isEditable = opts.editable;
    const bool isSearchable = opts.searchable || isEditable;

    const bool wasOpen = ctx.IsPopupOpen(id);
    const bool wasFocused = ctx.IsFocused(id) && !effectiveDisabled;

    ctx.RegisterFocusable(id, effectiveDisabled);

    Response res = ctx.Interact(id);
    const bool mouseJustPressed = res.hovered && ctx.IsMouseClicked() && !effectiveDisabled;

    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    } else {
        if (res.clicked || (isSearchable && mouseJustPressed)) {
            ctx.RequestFocus(id);
        }
    }

    // Determine currently selected index from pointers
    auto findMatchingIndex = [&](keel::StringRef sv) -> keel::usize {
        for (keel::usize i = 0; i < items.size(); ++i) {
            if (items[i].text.text == sv) {
                return i;
            }
        }
        return keel::usize(-1);
    };

    keel::usize currIndex = keel::usize(-1);
    if (selectedIndex != nullptr) {
        if (*selectedIndex < items.size()) currIndex = *selectedIndex;
    } else if (selectedIntIndex != nullptr) {
        if (*selectedIntIndex >= 0 && static_cast<keel::usize>(*selectedIntIndex) < items.size()) {
            currIndex = static_cast<keel::usize>(*selectedIntIndex);
        }
    } else if (selectedString.has_value()) {
        currIndex = findMatchingIndex(selectedString->as_ref());
    } else if (selectedStringView != nullptr) {
        currIndex = findMatchingIndex(*selectedStringView);
    }

    const UiTheme theme = ctx.GetTheme();
    const SelectMetrics& m = theme.GetSelect(activeSize);
    const ScrollbarMetrics& sm = theme.GetScrollbar(activeSize);
    ICanvas* canvas = ctx.GetCanvas();

    SelectOptions optsCopy = opts;
    const SizeMode effWidthMode = Control::ResolveWidthMode(ctx, opts.width, SizeMode::Stretch);
    if (!optsCopy.width.IsFixed()) {
        optsCopy.width = effWidthMode;
    }

    auto measureRowContentW = [&](const UiText& mainText, const UiAffix& pfx, const UiAffix& sfx) -> float {
        const float mainFont = theme.ResolveFontSize(mainText, TextRole::LabelMedium, activeSize);
        float w = TextUtil::MeasureUiTextWidth(mainText, mainFont, canvas);
        if (pfx.HasContentOrReserve()) {
            const float pFont = theme.ResolveFontSize(pfx.text, TextRole::LabelMedium, activeSize);
            w += TextUtil::MeasureAffixWidth(pfx, pFont, m.iconSize, 4.0f, canvas) + 6.0f;
        }
        if (sfx.HasContentOrReserve()) {
            const float sFont = theme.ResolveFontSize(sfx.text, TextRole::LabelMedium, activeSize);
            w += TextUtil::MeasureAffixWidth(sfx, sFont, m.iconSize, 4.0f, canvas) + 6.0f;
        }
        return w;
    };

    float maxRowW = 0.0f;
    for (const SelectItem& it : items) {
        const UiAffix& effPfx = it.prefix.HasContentOrReserve() ? it.prefix : optsCopy.prefix;
        const UiAffix& effSfx = it.suffix.HasContentOrReserve() ? it.suffix : optsCopy.suffix;
        maxRowW = keel::max(maxRowW, measureRowContentW(it.text, effPfx, effSfx));
        // Also ensure popup row width (with item's own prefix & suffix) fits
        maxRowW = keel::max(maxRowW, measureRowContentW(it.text, it.prefix, it.suffix));
    }
    if (optsCopy.placeholder.HasContentOrReserve()) {
        maxRowW = keel::max(maxRowW, measureRowContentW(optsCopy.placeholder, optsCopy.prefix, optsCopy.suffix));
    }

    const bool exceedsVisibleItems = optsCopy.maxVisibleItems.has_value()
        && *optsCopy.maxVisibleItems > 0
        && items.size() > static_cast<keel::usize>(*optsCopy.maxVisibleItems);

    const float clearExtraW = optsCopy.clearable ? (m.iconSize + 8.0f) : 0.0f;
    float intrinsicW = maxRowW + m.paddingX * 2.0f + m.iconSize + 8.0f + clearExtraW + (exceedsVisibleItems ? sm.gutter : 0.0f);
    if (optsCopy.minWidth.has_value()) {
        intrinsicW = keel::max(theme.Scale(*optsCopy.minWidth), intrinsicW);
    } else {
        intrinsicW = keel::max(m.minWidth, intrinsicW);
    }
    if (optsCopy.width.IsFixed()) {
        intrinsicW = theme.Scale(optsCopy.width.Dp());
    }

    const ControlSpec ctrl = EffectiveSelectControl(optsCopy, activeSize, theme);
    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    Vec2 fallbackDims = Select::Measure(optsCopy, activeSize, canvas, theme);
    if (effWidthMode == SizeMode::Fit && !optsCopy.width.IsFixed()) {
        fallbackDims.x = intrinsicW;
    }
    const Rect boxRect = (prevBounds.w > 0.0f && prevBounds.h > 0.0f)
        ? prevBounds
        : Rect({0.0f, 0.0f}, fallbackDims);

    struct SelectPopupState {
        float scrollY{0.0f};
        bool draggingScrollbar{false};
        float dragStartMouseY{0.0f};
        float dragStartScrollY{0.0f};
        bool scrollbarHovered{false};
        bool scrollbarActive{false};
        keel::SmallString<64> filterText{};
        float scrollX{0.0f};
        double lastClickTime{-10.0};
        Vec2 lastClickPos{-1000.0f, -1000.0f};
        keel::i32 clickCount{0};
        bool draggingWord{false};
        keel::usize wordDragStart{0};
        keel::usize wordDragEnd{0};
        bool actionBtnDown{false};
        bool filterActive{false};
        keel::i32 highlightedFilteredIdx{-1};
    };
    auto& popupState = ctx.GetOrCreateState<SelectPopupState>(id);
    if (popupState.actionBtnDown) {
        res.clicked = false;
    }
    if (!res.pressed && !mouseJustPressed) {
        popupState.actionBtnDown = false;
    }

    // If popup was closed externally (e.g. Escape or outside click), reset transient search filter
    if (!wasOpen && !isEditable && !popupState.filterText.empty() && !ctx.HasActiveTextEdit(id)) {
        popupState.filterText.clear();
        popupState.scrollX = 0.0f;
        popupState.highlightedFilteredIdx = -1;
    }

    UiAffix activePrefix = opts.prefix;
    UiAffix activeSuffix = opts.suffix;
    if (currIndex < items.size()) {
        if (items[currIndex].prefix.HasContentOrReserve()) activePrefix = items[currIndex].prefix;
        if (items[currIndex].suffix.HasContentOrReserve()) activeSuffix = items[currIndex].suffix;
    }

    auto hasValueToClear = [&]() -> bool {
        if (!opts.clearable || effectiveDisabled) return false;
        if (isEditable) {
            if (selectedString.has_value() && !selectedString->empty()) return true;
            if (selectedStringView != nullptr && !selectedStringView->empty()) return true;
            return !popupState.filterText.empty();
        }
        if (wasOpen && opts.searchable && !popupState.filterText.empty()) return true;
        if (currIndex < items.size()) return true;
        if (selectedString.has_value() && !selectedString->empty()) return true;
        if (selectedStringView != nullptr && !selectedStringView->empty()) return true;
        return false;
    };

    bool showClearBtn = hasValueToClear();
    SelectBoxRects rects = ResolveSelectBoxRects(boxRect, activePrefix, activeSuffix, activeSize, canvas, theme, showClearBtn);
    const Vec2 mousePos = ctx.GetMousePos();
    bool clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);
    bool chevronHovered = res.hovered && rects.chevronRect.Contains(mousePos);

    if (mouseJustPressed && clearHovered) {
        popupState.actionBtnDown = true;
        popupState.scrollX = 0.0f;
        popupState.filterText.clear();
        popupState.filterActive = false;
        popupState.highlightedFilteredIdx = -1;

        const bool hadSelection = (currIndex < items.size())
            || (selectedIndex != nullptr && *selectedIndex != keel::usize(-1))
            || (selectedIntIndex != nullptr && *selectedIntIndex != -1)
            || (selectedString.has_value() && !selectedString->empty())
            || (selectedStringView != nullptr && !selectedStringView->empty());

        currIndex = keel::usize(-1);
        if (selectedIndex != nullptr) *selectedIndex = keel::usize(-1);
        if (selectedIntIndex != nullptr) *selectedIntIndex = -1;
        if (selectedString.has_value()) selectedString->clear();
        if (selectedStringView != nullptr) *selectedStringView = keel::StringRef{};
        if (ctx.HasActiveTextEdit(id)) {
            ctx.SetTextEditBuffer(id, "");
        }
        activePrefix = opts.prefix;
        activeSuffix = opts.suffix;
        showClearBtn = false;
        clearHovered = false;
        rects = ResolveSelectBoxRects(boxRect, activePrefix, activeSuffix, activeSize, canvas, theme, showClearBtn);
        res.clicked = false;
        if (hadSelection) {
            res.changed = true;
        }
    }

    const bool isFocused = ctx.IsFocused(id) && !effectiveDisabled;
    res.focused = isFocused;

    keel::usize cursor = 0;
    keel::usize selAnchor = 0;
    bool editChanged = false;
    bool editSubmitted = false;
    bool isEditingText = false;
    keel::StringRef editDisplayText{};

    if (!effectiveDisabled && isSearchable) {
        const bool shouldRunTextEdit = isEditable
            ? (isFocused || ctx.HasActiveTextEdit(id))
            : (wasOpen || isFocused || ctx.HasActiveTextEdit(id));

        if (shouldRunTextEdit) {
            keel::StringRef initText = isEditable
                ? (selectedString.has_value() ? selectedString->as_ref()
                   : (selectedStringView != nullptr ? *selectedStringView
                      : (currIndex < items.size() ? items[currIndex].text.text : popupState.filterText.as_ref())))
                : popupState.filterText.as_ref();

            keel::StringRef currentEditText = ctx.EnsureTextEdit(id, initText, /*multiline=*/false, /*readOnly=*/false);
            showClearBtn = opts.clearable && (!currentEditText.empty() || (!isEditable && currIndex < items.size()));
            rects = ResolveSelectBoxRects(boxRect, activePrefix, activeSuffix, activeSize, canvas, theme, showClearBtn);
            clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);
            chevronHovered = res.hovered && rects.chevronRect.Contains(mousePos);

            const float inputFont = theme.ResolveFontSize(optsCopy.placeholder, TextRole::LabelMedium, activeSize);
            const bool allowMouseCaretHit = isEditable || (wasOpen && !currentEditText.empty());

            if ((mouseJustPressed || res.pressed) && !popupState.actionBtnDown && !clearHovered && !chevronHovered && allowMouseCaretHit) {
                const float localX = (mousePos.x - rects.textRect.x) + popupState.scrollX;
                keel::usize hitIdx = TextUtil::HitTestIndex(currentEditText, localX, inputFont, canvas);
                hitIdx = SnapToUtf8CodepointBoundary(currentEditText, hitIdx);

                if (mouseJustPressed) {
                    const double now = ctx.GetCurrentTime();
                    const float dist = (mousePos - popupState.lastClickPos).Length();
                    if (wasFocused && (now - popupState.lastClickTime) <= 0.35 && dist <= 6.0f) {
                        popupState.clickCount += 1;
                    } else {
                        popupState.clickCount = 1;
                    }
                    popupState.lastClickTime = now;
                    popupState.lastClickPos = mousePos;

                    if (popupState.clickCount == 1) {
                        popupState.draggingWord = false;
                        if (ctx.GetKeyModifiers().shift && wasFocused) {
                            const keel::usize anchor = ctx.GetTextEditAnchor(id);
                            ctx.SetTextEditCursor(id, hitIdx, anchor);
                        } else {
                            ctx.SetTextEditCursor(id, hitIdx, hitIdx);
                        }
                    } else if (popupState.clickCount == 2) {
                        keel::usize wStart = 0;
                        keel::usize wEnd = 0;
                        TextUtil::SelectWordAt(currentEditText, hitIdx, wStart, wEnd);
                        wStart = SnapToUtf8CodepointBoundary(currentEditText, wStart);
                        wEnd = SnapToUtf8CodepointBoundary(currentEditText, wEnd);
                        popupState.draggingWord = true;
                        popupState.wordDragStart = wStart;
                        popupState.wordDragEnd = wEnd;
                        ctx.SetTextEditCursor(id, wEnd, wStart);
                    } else {
                        popupState.draggingWord = false;
                        ctx.SetTextEditCursor(id, currentEditText.size(), 0);
                    }
                } else if (res.pressed) {
                    if (popupState.clickCount >= 3) {
                        ctx.SetTextEditCursor(id, currentEditText.size(), 0);
                    } else if (popupState.draggingWord) {
                        keel::usize wStart = 0;
                        keel::usize wEnd = 0;
                        TextUtil::SelectWordAt(currentEditText, hitIdx, wStart, wEnd);
                        wStart = SnapToUtf8CodepointBoundary(currentEditText, wStart);
                        wEnd = SnapToUtf8CodepointBoundary(currentEditText, wEnd);
                        if (hitIdx < popupState.wordDragStart) {
                            ctx.SetTextEditCursor(id, wStart, popupState.wordDragEnd);
                        } else {
                            ctx.SetTextEditCursor(id, wEnd, popupState.wordDragStart);
                        }
                    } else {
                        const keel::usize anchor = ctx.GetTextEditAnchor(id);
                        ctx.SetTextEditCursor(id, hitIdx, anchor);
                    }
                }
            }

            editDisplayText = ctx.GetOrStartTextEdit(
                id,
                initText,
                cursor,
                selAnchor,
                editChanged,
                editSubmitted,
                /*multiline=*/false,
                /*readOnly=*/false
            );

            if (editChanged || editSubmitted) {
                popupState.lastClickTime = -10.0;
                popupState.clickCount = 0;
            }

            if (isEditable) {
                isEditingText = isFocused || wasOpen;
                if (editChanged) {
                    if (selectedString.has_value()) {
                        const keel::StringRef prevVal = ctx.AllocString(initText);
                        selectedString->assign(editDisplayText);
                        const keel::StringRef stored = selectedString->as_ref();
                        if (stored != editDisplayText) {
                            cursor = keel::min(cursor, stored.size());
                            selAnchor = keel::min(selAnchor, stored.size());
                            ctx.SetTextEditBuffer(id, stored);
                            ctx.SetTextEditCursor(id, cursor, selAnchor);
                            editDisplayText = stored;
                        }
                        res.changed = (editDisplayText != prevVal);
                    } else {
                        popupState.filterText.assign(editDisplayText);
                        res.changed = true;
                    }
                    currIndex = findMatchingIndex(editDisplayText);
                    if (selectedIndex != nullptr) *selectedIndex = currIndex;
                    if (selectedIntIndex != nullptr) *selectedIntIndex = (currIndex < items.size()) ? static_cast<keel::i32>(currIndex) : -1;
                    if (selectedStringView != nullptr) *selectedStringView = (currIndex < items.size()) ? items[currIndex].text.text : keel::StringRef{};
                    popupState.filterActive = true;
                    popupState.highlightedFilteredIdx = 0;
                }
            } else {
                popupState.filterText.assign(editDisplayText);
                if (editChanged) {
                    popupState.highlightedFilteredIdx = 0;
                }
            }
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
                if (!isSearchable) {
                    if (ctx.ConsumeKey(Key::Space) || ctx.ConsumeKey(Key::Enter) || ctx.ConsumeKey(Key::Down)) {
                        wantOpen = true;
                        res.clicked = true;
                    }
                } else if (!isEditable) {
                    if (ctx.ConsumeKey(Key::Down) || (popupState.filterText.empty() && ctx.ConsumeKey(Key::Space))) {
                        wantOpen = true;
                        res.clicked = true;
                    } else if (editChanged && !popupState.filterText.empty()) {
                        wantOpen = true;
                    } else if (editSubmitted) {
                        wantOpen = true;
                        editSubmitted = false;
                        ctx.RequestFocus(id);
                    }
                } else {
                    if (ctx.ConsumeKey(Key::Down) || editChanged) {
                        wantOpen = true;
                    } else if (editSubmitted) {
                        res.submitted = true;
                    }
                }
            }
        } else {
            // Popup was already open
            if (res.clicked && !popupState.actionBtnDown) {
                if (!isSearchable) {
                    wantClose = true;
                } else if (!isEditable) {
                    if (chevronHovered || popupState.filterText.empty()) {
                        wantClose = true;
                    }
                } else {
                    if (chevronHovered) {
                        wantClose = true;
                    }
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
        popupState.filterActive = false;
        if (opts.searchable && !isEditable) {
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
        if (opts.searchable && !isEditable) {
            if (!editChanged) {
                popupState.filterText.clear();
                popupState.scrollX = 0.0f;
                ctx.SetTextEditBuffer(id, "");
                editDisplayText = "";
                cursor = 0;
                selAnchor = 0;
            }
            popupState.highlightedFilteredIdx = -1;
        } else if (isEditable) {
            if (!editChanged) {
                keel::StringRef curVal = selectedString.has_value() ? selectedString->as_ref() : editDisplayText;
                popupState.filterActive = (currIndex == keel::usize(-1) && !curVal.empty());
                ctx.EnsureTextEdit(id, curVal, /*multiline=*/false, /*readOnly=*/false);
            }
            popupState.highlightedFilteredIdx = -1;
        }
    }

    if (opts.searchable && !isEditable) {
        isEditingText = isOpen;
    } else if (isEditable) {
        isEditingText = ctx.IsFocused(id) || isOpen;
    }

    // Build filtered item indices
    keel::StringRef activeQuery{};
    if (isOpen && isSearchable) {
        if (isEditable) {
            if (popupState.filterActive) {
                activeQuery = selectedString.has_value() ? selectedString->as_ref() : editDisplayText;
            }
        } else {
            activeQuery = popupState.filterText.as_ref();
        }
    }

    keel::SmallArray<keel::usize, 64> filteredIndices;
    filteredIndices.reserve(items.size());
    keel::i32 currFilteredPos = -1;
    for (keel::usize i = 0; i < items.size(); ++i) {
        if (MatchesSelectFilter(items[i], activeQuery)) {
            if (i == currIndex) {
                currFilteredPos = static_cast<keel::i32>(filteredIndices.size());
            }
            filteredIndices.push_back(i);
        }
    }

    if (isOpen && isSearchable) {
        if (popupState.highlightedFilteredIdx < 0 ||
            popupState.highlightedFilteredIdx >= static_cast<keel::i32>(filteredIndices.size())) {
            if (currFilteredPos >= 0) {
                popupState.highlightedFilteredIdx = currFilteredPos;
            } else if (!activeQuery.empty() && !filteredIndices.empty()) {
                popupState.highlightedFilteredIdx = -1;
                for (keel::usize fi = 0; fi < filteredIndices.size(); ++fi) {
                    if (!items[filteredIndices[fi]].disabled) {
                        popupState.highlightedFilteredIdx = static_cast<keel::i32>(fi);
                        break;
                    }
                }
            } else {
                popupState.highlightedFilteredIdx = -1;
            }
        }
    }

    const keel::usize effectiveRowCount = filteredIndices.empty()
        ? (isSearchable ? 1 : 0)
        : filteredIndices.size();
    const float totalContentH = static_cast<float>(effectiveRowCount) * m.itemHeight;

    auto scrollItemIntoView = [&](keel::usize filteredPos, float viewportH, float maxScrollY) {
        if (filteredPos >= filteredIndices.size() || maxScrollY <= 0.0f || viewportH <= 0.0f) {
            return;
        }
        const float itemTop = static_cast<float>(filteredPos) * m.itemHeight;
        const float itemBottom = itemTop + m.itemHeight;
        if (itemTop < popupState.scrollY) {
            popupState.scrollY = itemTop;
        } else if (itemBottom > popupState.scrollY + viewportH) {
            popupState.scrollY = itemBottom - viewportH;
        }
        popupState.scrollY = keel::clamp(popupState.scrollY, 0.0f, maxScrollY);
    };

    auto computePopupRect = [&](keel::usize rowCount) -> Rect {
        const float popupW = boxRect.w > 0.0f ? boxRect.w : intrinsicW;
        const keel::usize clampedRows = keel::max<keel::usize>(1, rowCount);
        const keel::usize visibleCount = (opts.maxVisibleItems.has_value() && *opts.maxVisibleItems > 0)
            ? keel::min(clampedRows, static_cast<keel::usize>(*opts.maxVisibleItems))
            : clampedRows;
        const float contentH = static_cast<float>(visibleCount) * m.itemHeight + 8.0f;
        float popupH = contentH;
        float popupX = boxRect.x;
        float popupY = boxRect.y + boxRect.h + 4.0f;

        const Rect vp = ctx.GetPopupViewportRect();
        if (vp.h > 0.0f) {
            const float availDown = vp.Bottom() - popupY - 6.0f;
            const float availUp = boxRect.y - 4.0f - (vp.y + 6.0f);
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
            const float viewportH = keel::max(0.0f, popupRect.h - 8.0f);
            const float maxScrollY = keel::max(0.0f, totalContentH - viewportH);
            popupState.scrollY = keel::clamp(popupState.scrollY, 0.0f, maxScrollY);
            if (currFilteredPos >= 0) {
                scrollItemIntoView(static_cast<keel::usize>(currFilteredPos), viewportH, maxScrollY);
            } else {
                popupState.scrollY = 0.0f;
            }
        }
    } else if (isOpen && effectiveRowCount == 0) {
        ctx.ClosePopup();
        isOpen = false;
    }

    auto applySelection = [&](keel::usize newIdx) {
        if (newIdx >= items.size()) return;
        const bool strDiffers = (selectedString.has_value() && selectedString->as_ref() != items[newIdx].text.text) ||
                                (selectedStringView != nullptr && *selectedStringView != items[newIdx].text.text);
        if (currIndex != newIdx || strDiffers) {
            res.changed = true;
            currIndex = newIdx;
            if (selectedIndex != nullptr) *selectedIndex = newIdx;
            if (selectedIntIndex != nullptr) *selectedIntIndex = static_cast<keel::i32>(newIdx);
            if (selectedString.has_value()) selectedString->assign(items[newIdx].text.text);
            if (selectedStringView != nullptr) *selectedStringView = items[newIdx].text.text;
        }
        if (isEditable && selectedString.has_value()) {
            editDisplayText = selectedString->as_ref();
            ctx.SetTextEditBuffer(id, editDisplayText);
            cursor = editDisplayText.size();
            selAnchor = cursor;
            popupState.filterActive = false;
        } else if (opts.searchable) {
            popupState.filterText.clear();
            editDisplayText = "";
            ctx.SetTextEditBuffer(id, "");
            cursor = 0;
            selAnchor = 0;
        }
    };

    // Process open popup interaction, scrolling, and keyboard navigation
    keel::i32 hoveredItem = -1;
    if (isOpen && wasOpen && !effectiveDisabled) {
        auto hitScope = ctx.ScopedOverlayHitCheck(id);
        const Rect popupRect = ctx.GetOpenPopupRect();
        const Vec2 mouse = ctx.GetMousePos();
        const float viewportH = keel::max(0.0f, popupRect.h - 8.0f);
        const float maxScrollY = keel::max(0.0f, totalContentH - viewportH);
        const bool hasScrollbar = (maxScrollY > 0.5f);

        popupState.scrollY = keel::clamp(popupState.scrollY, 0.0f, maxScrollY);

        // 1. Mouse wheel scrolling inside popup
        if (hasScrollbar && popupRect.Contains(mouse)) {
            const Vec2 wheel = ctx.GetMouseWheel();
            if (wheel.y != 0.0f || wheel.x != 0.0f) {
                popupState.scrollY = keel::clamp(popupState.scrollY - wheel.y * sm.scrollStep, 0.0f, maxScrollY);
                ctx.ConsumeMouseWheel();
            }
        }

        // 2. Scrollbar thumb drag & track page-jump interaction
        popupState.scrollbarHovered = false;
        if (hasScrollbar && popupRect.h > 10.0f) {
            const float trackX = popupRect.Right() - sm.trackWidth - sm.margin;
            const float trackY = popupRect.y + sm.margin;
            const float trackH = keel::max(0.0f, popupRect.h - 2.0f * sm.margin);
            const float thumbH = keel::clamp((viewportH / keel::max(1.0f, totalContentH)) * trackH, sm.minThumbLength, trackH);
            const float travelH = keel::max(0.0f, trackH - thumbH);
            const float scrollRatio = (maxScrollY > 0.0f) ? keel::clamp(popupState.scrollY / maxScrollY, 0.0f, 1.0f) : 0.0f;
            const float thumbY = trackY + scrollRatio * travelH;

            const Rect trackRect(trackX, trackY, sm.trackWidth, trackH);
            const Rect thumbRect(trackX, thumbY, sm.trackWidth, thumbH);
            const Rect gutterRect(popupRect.Right() - sm.gutter, popupRect.y, sm.gutter, popupRect.h);

            const bool overThumb = thumbRect.Contains(mouse);
            const bool overTrack = trackRect.Contains(mouse) || gutterRect.Contains(mouse);
            popupState.scrollbarHovered = overThumb || overTrack;

            if (ctx.IsMouseClicked() && overTrack) {
                if (overThumb) {
                    popupState.draggingScrollbar = true;
                    popupState.dragStartMouseY = mouse.y;
                    popupState.dragStartScrollY = popupState.scrollY;
                    ctx.SetActiveId(id);
                } else {
                    if (mouse.y < thumbRect.y) {
                        popupState.scrollY = keel::max(0.0f, popupState.scrollY - viewportH * 0.8f);
                    } else if (mouse.y > thumbRect.Bottom()) {
                        popupState.scrollY = keel::min(maxScrollY, popupState.scrollY + viewportH * 0.8f);
                    }
                }
                ctx.ConsumeMouseClick();
            }

            if (popupState.draggingScrollbar) {
                if (ctx.IsMouseDown()) {
                    if (travelH > 0.0f) {
                        const float deltaY = mouse.y - popupState.dragStartMouseY;
                        popupState.scrollY = keel::clamp(
                            popupState.dragStartScrollY + (deltaY / travelH) * maxScrollY,
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

        // 3. Check hover and click on dropdown items (suppressed when dragging or hovering scrollbar)
        const float itemW = keel::max(0.0f, popupRect.w - 8.0f - (hasScrollbar ? sm.gutter : 0.0f));
        const Rect itemsViewport(popupRect.x + 4.0f, popupRect.y + 4.0f, itemW, viewportH);

        if (!popupState.draggingScrollbar && !popupState.scrollbarHovered && itemsViewport.Contains(mouse)) {
            for (keel::usize fi = 0; fi < filteredIndices.size(); ++fi) {
                const keel::usize origIdx = filteredIndices[fi];
                const Rect itemRect(
                    popupRect.x + 4.0f,
                    popupRect.y + 4.0f + static_cast<float>(fi) * m.itemHeight - popupState.scrollY,
                    itemW,
                    m.itemHeight
                );
                if (itemRect.Bottom() <= itemsViewport.y || itemRect.y >= itemsViewport.Bottom()) {
                    continue;
                }
                if (itemRect.Contains(mouse)) {
                    if (!items[origIdx].disabled) {
                        hoveredItem = static_cast<keel::i32>(fi);
                        popupState.highlightedFilteredIdx = hoveredItem;
                    }
                    if (ctx.IsMouseReleased()) {
                        if (!items[origIdx].disabled) {
                            applySelection(origIdx);
                            ctx.RequestFocus(id);
                            ctx.ClosePopup();
                            isOpen = false;
                        }
                        ctx.ConsumeMouseClick();
                        break;
                    }
                }
            }
        }

        // 4. Keyboard navigation when popup is open (skipping disabled items + auto scroll-into-view)
        if (isOpen) {
            if (ctx.ConsumeKey(Key::Escape)) {
                ctx.ClosePopup();
                isOpen = false;
                popupState.draggingScrollbar = false;
                popupState.filterActive = false;
                if (opts.searchable && !isEditable) {
                    popupState.filterText.clear();
                    ctx.SetTextEditBuffer(id, "");
                }
                ctx.RequestFocus(id);
            } else if (editSubmitted || ctx.ConsumeKey(Key::Enter)) {
                if (isSearchable &&
                    popupState.highlightedFilteredIdx >= 0 &&
                    popupState.highlightedFilteredIdx < static_cast<keel::i32>(filteredIndices.size())) {
                    const keel::usize chosen = filteredIndices[static_cast<keel::usize>(popupState.highlightedFilteredIdx)];
                    if (!items[chosen].disabled) {
                        applySelection(chosen);
                    }
                }
                res.submitted = true;
                ctx.ClosePopup();
                isOpen = false;
                popupState.draggingScrollbar = false;
                ctx.RequestFocus(id);
            } else if (ctx.ConsumeKey(Key::Down)) {
                if (!filteredIndices.empty()) {
                    if (!isSearchable) {
                        const keel::usize startFi = (currFilteredPos < 0) ? 0 : static_cast<keel::usize>(currFilteredPos + 1);
                        for (keel::usize fi = startFi; fi < filteredIndices.size(); ++fi) {
                            const keel::usize origIdx = filteredIndices[fi];
                            if (!items[origIdx].disabled) {
                                applySelection(origIdx);
                                scrollItemIntoView(fi, viewportH, maxScrollY);
                                break;
                            }
                        }
                    } else {
                        const keel::usize startFi = (popupState.highlightedFilteredIdx < 0)
                            ? 0
                            : static_cast<keel::usize>(popupState.highlightedFilteredIdx + 1);
                        for (keel::usize fi = startFi; fi < filteredIndices.size(); ++fi) {
                            if (!items[filteredIndices[fi]].disabled) {
                                popupState.highlightedFilteredIdx = static_cast<keel::i32>(fi);
                                scrollItemIntoView(fi, viewportH, maxScrollY);
                                break;
                            }
                        }
                    }
                }
            } else if (ctx.ConsumeKey(Key::Up)) {
                if (!filteredIndices.empty()) {
                    if (!isSearchable) {
                        if (currFilteredPos > 0) {
                            for (keel::usize fi = static_cast<keel::usize>(currFilteredPos); fi > 0; --fi) {
                                const keel::usize candidateFi = fi - 1;
                                const keel::usize origIdx = filteredIndices[candidateFi];
                                if (!items[origIdx].disabled) {
                                    applySelection(origIdx);
                                    scrollItemIntoView(candidateFi, viewportH, maxScrollY);
                                    break;
                                }
                            }
                        }
                    } else {
                        if (popupState.highlightedFilteredIdx > 0) {
                            for (keel::usize fi = static_cast<keel::usize>(popupState.highlightedFilteredIdx); fi > 0; --fi) {
                                const keel::usize candidateFi = fi - 1;
                                if (!items[filteredIndices[candidateFi]].disabled) {
                                    popupState.highlightedFilteredIdx = static_cast<keel::i32>(candidateFi);
                                    scrollItemIntoView(candidateFi, viewportH, maxScrollY);
                                    break;
                                }
                            }
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

    // Recompute active affixes and display text after selection/edit updates
    activePrefix = opts.prefix;
    activeSuffix = opts.suffix;
    if (currIndex < items.size()) {
        if (items[currIndex].prefix.HasContentOrReserve()) {
            activePrefix = items[currIndex].prefix;
        }
        if (items[currIndex].suffix.HasContentOrReserve()) {
            activeSuffix = items[currIndex].suffix;
        }
    }

    UiText currentDisplayText{};
    if (isEditingText) {
        if (isEditable) {
            if (selectedString.has_value()) {
                currentDisplayText = selectedString->as_ref();
            } else if (!editDisplayText.empty()) {
                currentDisplayText = editDisplayText;
            }
        } else {
            currentDisplayText = popupState.filterText.as_ref();
        }
    } else {
        if (currIndex < items.size()) {
            currentDisplayText = items[currIndex].text;
        } else if (selectedString.has_value() && !selectedString->empty()) {
            currentDisplayText = selectedString->as_ref();
        } else if (selectedStringView != nullptr && !selectedStringView->empty()) {
            currentDisplayText = *selectedStringView;
        } else {
            currentDisplayText = opts.placeholder;
        }
    }

    showClearBtn = hasValueToClear();
    rects = ResolveSelectBoxRects(boxRect, activePrefix, activeSuffix, activeSize, canvas, theme, showClearBtn);
    clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);
    chevronHovered = res.hovered && rects.chevronRect.Contains(mousePos);

    if (isEditingText && rects.textRect.w > 0.0f) {
        const float inputFont = theme.ResolveFontSize(currentDisplayText, TextRole::LabelMedium, activeSize);
        const keel::StringRef measureText = currentDisplayText.text;
        const keel::usize safeCursor = keel::min(cursor, measureText.size());
        const float cursorX = TextUtil::MeasureWidth(measureText.substr(0, safeCursor), inputFont, canvas);
        const float fullTextW = TextUtil::MeasureWidth(measureText, inputFont, canvas);
        const float visibleW = keel::max(0.0f, rects.textRect.w - 2.0f);

        if (cursorX < popupState.scrollX) {
            popupState.scrollX = cursorX;
        } else if (cursorX > popupState.scrollX + visibleW) {
            popupState.scrollX = cursorX - visibleW;
        }
        const float maxScroll = keel::max(0.0f, fullTextW - visibleW);
        popupState.scrollX = keel::clamp(popupState.scrollX, 0.0f, maxScroll);
    } else {
        popupState.scrollX = 0.0f;
    }

    // Register overlay render callback if popup is open
    if (isOpen) {
        const Rect currentPopupRect = ctx.GetOpenPopupRect();
        const float popupScrollY = popupState.scrollY;
        const bool sbHovered = popupState.scrollbarHovered;
        const bool sbActive = popupState.scrollbarActive;
        const keel::i32 effectiveHoveredIdx = (hoveredItem >= 0)
            ? hoveredItem
            : (isSearchable ? popupState.highlightedFilteredIdx : -1);

        keel::usize filteredSelectedIndex = keel::usize(-1);
        SelectItem* arenaPopupItems = ctx.GetArena().create_array<SelectItem>(filteredIndices.size());
        for (keel::usize fi = 0; fi < filteredIndices.size(); ++fi) {
            const keel::usize origIdx = filteredIndices[fi];
            if (origIdx == currIndex) {
                filteredSelectedIndex = fi;
            }
            SelectItem copy = items[origIdx];
            copy.text = ctx.AllocText(copy.text);
            copy.prefix = ctx.AllocAffix(copy.prefix);
            copy.suffix = ctx.AllocAffix(copy.suffix);
            arenaPopupItems[fi] = copy;
        }
        const keel::ArrayRef<SelectItem> popupItemsSpan(arenaPopupItems, filteredIndices.size());

        const keel::StringRef highlightQueryStr = ctx.AllocString(activeQuery);
        ctx.SetOpenPopupVisualHash(detail::HashArgs(
            currentPopupRect.x,
            currentPopupRect.y,
            currentPopupRect.w,
            currentPopupRect.h,
            filteredSelectedIndex,
            effectiveHoveredIdx,
            popupScrollY,
            sbHovered,
            sbActive,
            popupItemsSpan.size(),
            detail::HashStringView(highlightQueryStr)
        ));

        SelectOptions popupOpts = opts;
        popupOpts.emptyText = ctx.AllocText(opts.emptyText);

        ctx.RegisterOverlay([currentPopupRect, popupItemsSpan, filteredSelectedIndex, effectiveHoveredIdx, popupOpts, activeSize, theme, popupScrollY, sbHovered, sbActive, highlightQueryStr](ICanvas& canvas) {
            Select::RenderPopup(
                canvas,
                currentPopupRect,
                popupItemsSpan,
                filteredSelectedIndex,
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
    UiText ghostPlaceholder = optsCopy.placeholder;
    if (opts.searchable && !isEditable && isOpen && currentDisplayText.empty() && currIndex < items.size()) {
        ghostPlaceholder = items[currIndex].text;
    }

    optsCopy.placeholder = ctx.AllocText(ghostPlaceholder);
    optsCopy.emptyText = ctx.AllocText(optsCopy.emptyText);
    optsCopy.tooltip = ctx.AllocText(optsCopy.tooltip);
    optsCopy.prefix = ctx.AllocAffix(activePrefix);
    optsCopy.suffix = ctx.AllocAffix(activeSuffix);

    const keel::usize selStart = keel::min(cursor, selAnchor);
    const keel::usize selEnd = keel::max(cursor, selAnchor);
    const bool finalFocused = isOpen || ctx.IsFocused(id);

    auto* data = ctx.AllocPayload<SelectData>();
    data->opts = optsCopy;
    data->displayText = ctx.AllocText(currentDisplayText);
    data->isEditing = isEditingText;
    data->cursor = cursor;
    data->selStart = selStart;
    data->selEnd = selEnd;
    data->scrollX = popupState.scrollX;
    data->cursorVisible = isEditingText && finalFocused && !effectiveDisabled && ctx.IsCursorVisible(optsCopy.cursorMode);
    data->cursorWidth = ctx.GetCursorConfig().width;
    data->naturalWidth = (effWidthMode == SizeMode::Fit && !optsCopy.width.IsFixed())
        ? intrinsicW
        : 0.0f;
    data->showClearBtn = showClearBtn;
    data->clearHovered = clearHovered;
    data->chevronHovered = chevronHovered;

    node->text = data->displayText.empty() ? optsCopy.placeholder.text : data->displayText.text;
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        static_cast<keel::u8>(optsCopy.align),
        optsCopy.width,
        optsCopy.height,
        optsCopy.minWidth.value_or(0.0f),
        optsCopy.searchable,
        optsCopy.editable,
        optsCopy.clearable,
        data->displayText,
        data->isEditing,
        data->cursor,
        data->selStart,
        data->selEnd,
        data->scrollX,
        data->showClearBtn,
        data->clearHovered,
        data->chevronHovered,
        optsCopy.placeholder,
        optsCopy.prefix,
        optsCopy.suffix,
        optsCopy.background,
        optsCopy.border
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<SelectData>();
        Vec2 sz = Select::Measure(d ? d->opts : SelectOptions{}, n->uiSize, canvas, theme, availableWidth);
        if (d && d->naturalWidth > 0.0f && !d->opts.width.IsFixed()) {
            sz.x = d->naturalWidth;
        }
        return sz;
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<SelectData>();
        Select::Render(
            canvas,
            n->computedRect,
            d ? d->displayText : UiText{},
            d ? d->opts : SelectOptions{},
            n->uiSize,
            n->hovered,
            n->active,
            n->disabled,
            theme,
            n->focused,
            d ? d->isEditing : false,
            d ? d->cursor : 0,
            d ? d->cursorVisible : false,
            d ? d->cursorWidth : 1.5f,
            d ? d->selStart : 0,
            d ? d->selEnd : 0,
            d ? d->scrollX : 0.0f,
            d ? d->showClearBtn : false,
            d ? d->clearHovered : false,
            d ? d->chevronHovered : false
        );
    };
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->active = isOpen;
    node->focused = finalFocused;
    res.focused = finalFocused;

    Control::SetupNode(ctx, node, ctrl);

    return res;
}

void Select::Render(
    ICanvas& canvas,
    Rect bounds,
    UiText currentText,
    const SelectOptions& opts,
    UiSize size,
    bool hovered,
    bool open,
    bool disabled,
    const UiTheme& theme,
    bool focused,
    bool isEditing,
    keel::usize cursor,
    bool cursorVisible,
    float cursorWidth,
    keel::usize selStart,
    keel::usize selEnd,
    float scrollX,
    bool showClearBtn,
    bool clearHovered,
    bool chevronHovered
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const SelectMetrics& m = theme.GetSelect(size);
    const Rect boxRect = bounds;

    // Determine colors
    const auto& pal = theme.palette;
    const bool hasBg = opts.background.IsBackgroundEnabled(true);
    Color boxBg = hasBg
        ? ((hovered && !disabled) ? pal.surface : pal.inputBackground)
        : Color::Transparent();
    Color boxBorder;
    float strokeW = (opts.border.width > 0.0f) ? theme.Scale(opts.border.width) : m.strokeWidth;

    if ((open || focused) && !disabled) {
        boxBorder = pal.focusRing;
        if (opts.border.width > 0.0f || m.strokeWidth > 0.0f) strokeW += 0.5f;
    } else if (hovered && !disabled) {
        boxBorder = pal.borderHover;
    } else {
        boxBorder = pal.inputBorder;
    }

    Color placeholderCol = theme.ResolveTextColor(opts.placeholder, UiVariant::Secondary, disabled);
    Color chevronCol = open ? pal.accent : ((chevronHovered && !disabled) ? pal.textPrimary : pal.textSecondary);
    Color unitCol = pal.textSecondary;

    if (disabled) {
        boxBg = boxBg.WithAlphaF(0.40f);
        boxBorder = boxBorder.WithAlphaF(0.40f);
        chevronCol = chevronCol.WithAlphaF(0.40f);
        unitCol = unitCol.WithAlphaF(0.40f);
    }

    if (hasBg && opts.background.HasAny()) {
        if (!opts.background.IsSurface()) {
            boxBg = theme.ResolveColor(opts.background, hovered, open || focused, disabled, boxBg);
        } else if (opts.background.HasAlphaScale()) {
            boxBg = boxBg.ScaleAlpha(opts.background.alphaScale);
        }
    }
    if (opts.border.color.HasAny()) {
        boxBorder = theme.ResolveColor(opts.border.color, hovered, open || focused, disabled, boxBorder);
    }

    // Draw box background and border
    const float radius = opts.border.radius.has_value() ? theme.Scale(*opts.border.radius) : m.cornerRadius;
    canvas.DrawRoundRect(boxRect, radius, boxBg, boxBorder, strokeW);

    // Left elements (prefix icon and/or text)
    float leftX = boxRect.x + m.paddingX;
    if (opts.prefix.HasContentOrReserve()) {
        const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const Color pTextCol = theme.ResolveTextColor(opts.prefix.text, UiVariant::Secondary, disabled, hovered, open || focused);
        const Color pIconCol = opts.prefix.text.color.HasAny() ? pTextCol : chevronCol;
        const float pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, &canvas);
        TextUtil::DrawAffix(canvas, opts.prefix, Rect(leftX, boxRect.y, pW, boxRect.h), pFont, m.iconSize, pTextCol, pIconCol, 4.0f);
        leftX += pW + 6.0f;
    }

    // Right elements (chevron indicator + optional clear button + optional suffix)
    const float btnSize = m.iconSize + 4.0f;
    float rightX = boxRect.x + boxRect.w - m.paddingX - m.iconSize;
    if (chevronHovered && !disabled && (opts.searchable || opts.editable)) {
        const Rect chevBtnRect(rightX - 2.0f, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        canvas.DrawRoundRect(chevBtnRect, 4.0f, pal.surfaceHover, Color::Transparent(), 0.0f);
    }
    const Rect chevronRect(rightX, boxRect.y + (boxRect.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
    const Icon chevron = open ? Icon::ChevronUp() : Icon::ChevronDown();
    chevron.Draw(canvas, chevronRect, chevronCol);
    rightX -= 6.0f;

    if (showClearBtn) {
        rightX -= btnSize;
        const Rect clearBtnRect(rightX, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        if (clearHovered && !disabled) {
            canvas.DrawRoundRect(clearBtnRect, 4.0f, pal.surfaceHover, Color::Transparent(), 0.0f);
        }
        const float clearIconSz = keel::max(8.0f, m.iconSize - 2.0f);
        const Rect icRect(
            rightX + (btnSize - clearIconSz) * 0.5f,
            boxRect.y + (boxRect.h - clearIconSz) * 0.5f,
            clearIconSz,
            clearIconSz
        );
        const Color icCol = (clearHovered && !disabled) ? pal.textPrimary : unitCol;
        Icon::Close().Draw(canvas, icRect, icCol);
        rightX -= 4.0f;
    }

    if (opts.suffix.HasContentOrReserve()) {
        const float sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
        const Color sTextCol = theme.ResolveTextColor(opts.suffix.text, UiVariant::Muted, disabled, hovered, open || focused);
        const Color sIconCol = opts.suffix.text.color.HasAny() ? sTextCol : unitCol;
        const float sW = TextUtil::MeasureAffixWidth(opts.suffix, sFont, m.iconSize, 4.0f, &canvas);
        rightX -= sW;
        TextUtil::DrawAffix(canvas, opts.suffix, Rect(rightX, boxRect.y, sW, boxRect.h), sFont, m.iconSize, sTextCol, sIconCol, 4.0f);
        rightX -= 6.0f;
    }

    // Center display text / placeholder / editable input
    const float availW = keel::max(0.0f, rightX - leftX);
    const Rect textRect(leftX, boxRect.y, availW, boxRect.h);
    canvas.PushClip(textRect);

    if (isEditing) {
        const keel::StringRef displayText = currentText.text;
        const float inputFont = theme.ResolveFontSize(currentText, TextRole::LabelMedium, size);
        const Color textCol = currentText.color.HasAny()
            ? theme.ResolveTextColor(currentText, UiVariant::Default, disabled, hovered, open || focused)
            : (disabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary);

        if (!disabled && selStart != selEnd && !displayText.empty()) {
            const keel::usize s0 = keel::min(keel::min(selStart, selEnd), displayText.size());
            const keel::usize s1 = keel::min(keel::max(selStart, selEnd), displayText.size());
            if (s1 > s0) {
                const float x0 = textRect.x - scrollX + canvas.MeasureText(displayText.substr(0, s0), inputFont).x;
                const float x1 = textRect.x - scrollX + canvas.MeasureText(displayText.substr(0, s1), inputFont).x;
                const float selH = inputFont * 1.25f;
                const float selY = boxRect.y + (boxRect.h - selH) * 0.5f;
                const Rect selRect(x0, selY, keel::max(1.0f, x1 - x0), selH);
                canvas.DrawRoundRect(selRect, 2.0f, pal.accent.WithAlpha(88), Color::Transparent(), 0.0f);
            }
        }

        if (displayText.empty() && !opts.placeholder.empty()) {
            const float phFont = !opts.placeholder.size.IsAuto()
                ? theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size)
                : inputFont;
            canvas.DrawTextBox(opts.placeholder.text, textRect, phFont, placeholderCol, 0.0f, 0.5f);
        } else if (!displayText.empty()) {
            const float fullW = keel::max(textRect.w, canvas.MeasureText(displayText, inputFont).x + 8.0f);
            const Rect scrolledRect(textRect.x - scrollX, textRect.y, fullW, textRect.h);
            canvas.DrawTextBox(displayText, scrolledRect, inputFont, textCol, 0.0f, 0.5f);
        }

        if (!disabled && cursorVisible) {
            const keel::usize safeCursor = keel::min(cursor, displayText.size());
            const keel::StringRef sub = displayText.substr(0, safeCursor);
            const float subW = canvas.MeasureText(sub, inputFont).x;
            const float caretX = textRect.x - scrollX + subW;
            const float caretH = inputFont * 1.15f;
            const float caretY = boxRect.y + (boxRect.h - caretH) * 0.5f;
            canvas.DrawLine({caretX, caretY}, {caretX, caretY + caretH}, pal.caret, cursorWidth);
        }
    } else {
        const bool isPlaceholder = currentText.empty() || (!currentText.color.HasAny() && currentText.text == opts.placeholder.text);
        const float selFont = theme.ResolveFontSize(currentText, TextRole::LabelMedium, size);
        const Color mainCol = isPlaceholder
            ? placeholderCol
            : theme.ResolveTextColor(currentText, UiVariant::Default, disabled, hovered, open || focused);
        const float alignX = (opts.align == Align::End) ? 1.0f : ((opts.align == Align::Center) ? 0.5f : 0.0f);
        TextUtil::DrawUiText(canvas, currentText, textRect, selFont, boxRect.h, mainCol, alignX, 0.5f, &theme);
    }

    canvas.PopClip();
}

void Select::RenderPopup(
    ICanvas& canvas,
    Rect popupRect,
    keel::ArrayRef<keel::StringRef> items,
    keel::usize selectedIndex,
    keel::i32 hoveredIndex,
    const SelectOptions& opts,
    UiSize size,
    const UiTheme& theme,
    float scrollY,
    bool scrollbarHovered,
    bool scrollbarActive,
    keel::StringRef highlightQuery
) {
    keel::SmallArray<SelectItem, 32> richItems;
    richItems.reserve(items.size());
    for (keel::StringRef sv : items) {
        richItems.push_back(SelectItem{.text = sv});
    }
    RenderPopup(canvas, popupRect, richItems, selectedIndex, hoveredIndex, opts, size, theme, scrollY, scrollbarHovered, scrollbarActive, highlightQuery);
}

void Select::RenderPopup(
    ICanvas& canvas,
    Rect popupRect,
    keel::ArrayRef<SelectItem> items,
    keel::usize selectedIndex,
    keel::i32 hoveredIndex,
    const SelectOptions& opts,
    UiSize size,
    const UiTheme& theme,
    float scrollY,
    bool scrollbarHovered,
    bool scrollbarActive,
    keel::StringRef highlightQuery
) {
    const bool isSearchable = opts.searchable || opts.editable;
    if (popupRect.w <= 0.0f || popupRect.h <= 0.0f || (items.empty() && !isSearchable)) {
        return;
    }

    const SelectMetrics& m = theme.GetSelect(size);
    const ScrollbarMetrics& sm = theme.GetScrollbar(size);
    const auto& pal = theme.palette;

    // Dropdown container styling: card with border and subtle shadow
    const Color popupBg = pal.surfaceElevated;
    const Color popupBorder = pal.border;
    const Color shadowCol = Color(0, 0, 0, 50);

    // Outer subtle drop shadow
    canvas.DrawRoundRect(Rect(popupRect.x, popupRect.y + 2.0f, popupRect.w, popupRect.h), m.cornerRadius + 1.0f, shadowCol);

    // Popup background and border
    canvas.DrawRoundRect(popupRect, m.cornerRadius, popupBg, popupBorder, 1.0f);

    if (items.empty()) {
        const Rect emptyRect(popupRect.x + m.paddingX, popupRect.y + 4.0f, keel::max(0.0f, popupRect.w - m.paddingX * 2.0f), keel::max(0.0f, popupRect.h - 8.0f));
        canvas.PushClip(emptyRect);
        const UiText emptyTxt = opts.emptyText.empty() ? UiText{"No matching items"} : opts.emptyText;
        const float emptyFont = theme.ResolveFontSize(emptyTxt, TextRole::LabelMedium, size);
        const Color emptyCol = theme.ResolveTextColor(emptyTxt, UiVariant::Muted, false);
        TextUtil::DrawUiText(canvas, emptyTxt, emptyRect, emptyFont, emptyRect.h, emptyCol, 0.0f, 0.5f, &theme);
        canvas.PopClip();
        return;
    }

    const float viewportH = keel::max(0.0f, popupRect.h - 8.0f);
    const float totalContentH = static_cast<float>(items.size()) * m.itemHeight;
    const float maxScrollY = keel::max(0.0f, totalContentH - viewportH);
    const bool hasScrollbar = (maxScrollY > 0.5f);
    const float clampedScrollY = keel::clamp(scrollY, 0.0f, maxScrollY);
    const float itemW = keel::max(0.0f, popupRect.w - 8.0f - (hasScrollbar ? sm.gutter : 0.0f));
    const Rect itemsClipRect(popupRect.x + 1.0f, popupRect.y + 4.0f, keel::max(0.0f, popupRect.w - 2.0f), viewportH);

    canvas.PushClip(itemsClipRect);

    for (keel::usize i = 0; i < items.size(); ++i) {
        const SelectItem& item = items[i];
        const Rect itemRect(
            popupRect.x + 4.0f,
            popupRect.y + 4.0f + static_cast<float>(i) * m.itemHeight - clampedScrollY,
            itemW,
            m.itemHeight
        );

        // Cull items completely outside the visible scroll viewport
        if (itemRect.Bottom() <= itemsClipRect.y || itemRect.y >= itemsClipRect.Bottom()) {
            continue;
        }

        const bool itemDisabled = item.disabled;
        const bool isSelected = (i == selectedIndex);
        const bool isHovered = (static_cast<keel::i32>(i) == hoveredIndex) && !itemDisabled;

        if (isSelected) {
            const Color itemBg = isHovered ? pal.surfaceActive : pal.surfaceHover;
            canvas.DrawRoundRect(itemRect, 4.0f, itemBg);
        } else if (isHovered) {
            canvas.DrawRoundRect(itemRect, 4.0f, pal.surfaceHover);
        }

        // Colors for item text and checkmark
        Color itemTextCol = item.text.color.HasAny()
            ? theme.ResolveTextColor(item.text, UiVariant::Default, itemDisabled, isHovered, isSelected)
            : (isSelected ? (itemDisabled ? pal.accent.WithAlphaF(0.40f) : pal.accent)
                          : (itemDisabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary));

        // Right side: checkmark (if selected) + item suffix
        float textRightX = itemRect.x + itemRect.w - m.paddingX;
        if (isSelected) {
            const float checkX = textRightX - m.iconSize;
            const float checkY = itemRect.y + (itemRect.h - m.iconSize) * 0.5f;
            const Rect checkRect(checkX, checkY, m.iconSize, m.iconSize);
            Icon::Check().Draw(canvas, checkRect, itemTextCol);
            textRightX = checkX - 6.0f;
        }

        if (item.suffix.HasContentOrReserve()) {
            const float sFont = theme.ResolveFontSize(item.suffix.text, TextRole::LabelMedium, size);
            const Color sTextCol = theme.ResolveTextColor(item.suffix.text, UiVariant::Muted, itemDisabled);
            const float sW = TextUtil::MeasureAffixWidth(item.suffix, sFont, m.iconSize, 4.0f, &canvas);
            textRightX -= sW;
            TextUtil::DrawAffix(canvas, item.suffix, Rect(textRightX, itemRect.y, sW, itemRect.h), sFont, m.iconSize, sTextCol, sTextCol, 4.0f);
            textRightX -= 6.0f;
        }

        // Left side: item prefix + main text
        float textLeftX = itemRect.x + m.paddingX;
        if (item.prefix.HasContentOrReserve()) {
            const float pFont = theme.ResolveFontSize(item.prefix.text, TextRole::LabelMedium, size);
            const Color pTextCol = theme.ResolveTextColor(item.prefix.text, UiVariant::Secondary, itemDisabled);
            const float pW = TextUtil::MeasureAffixWidth(item.prefix, pFont, m.iconSize, 4.0f, &canvas);
            TextUtil::DrawAffix(canvas, item.prefix, Rect(textLeftX, itemRect.y, pW, itemRect.h), pFont, m.iconSize, pTextCol, pTextCol, 4.0f);
            textLeftX += pW + 6.0f;
        }

        const float selFont = theme.ResolveFontSize(item.text, TextRole::LabelMedium, size);
        const Rect itemTextRect(textLeftX, itemRect.y, keel::max(0.0f, textRightX - textLeftX), itemRect.h);

        // Highlight matched substring when search query is active
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
                    pal.accent.WithAlpha(68),
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

    // Render vertical scrollbar when items exceed visible popup height
    if (hasScrollbar && popupRect.h > 10.0f) {
        canvas.PushClip(popupRect);
        const float trackX = popupRect.Right() - sm.trackWidth - sm.margin;
        const float trackY = popupRect.y + sm.margin;
        const float trackH = keel::max(0.0f, popupRect.h - 2.0f * sm.margin);
        const float thumbH = keel::clamp((viewportH / keel::max(1.0f, totalContentH)) * trackH, sm.minThumbLength, trackH);
        const float travelH = keel::max(0.0f, trackH - thumbH);
        const float scrollRatio = (maxScrollY > 0.0f) ? (clampedScrollY / maxScrollY) : 0.0f;
        const float thumbY = trackY + scrollRatio * travelH;

        const Rect trackRect(trackX, trackY, sm.trackWidth, trackH);
        const Rect thumbRect(trackX, thumbY, sm.trackWidth, thumbH);

        canvas.DrawRoundRect(trackRect, sm.cornerRadius, pal.borderSubtle);

        Color thumbCol = pal.textMuted;
        if (scrollbarActive) {
            thumbCol = pal.accent;
        } else if (scrollbarHovered) {
            thumbCol = pal.textSecondary;
        }
        canvas.DrawRoundRect(thumbRect, sm.cornerRadius, thumbCol);
        canvas.PopClip();
    }
}

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
    float afterChipsX{0.0f};
};

MultiChipLayout ComputeMultiChipLayout(
    Rect textRect,
    float boxH,
    keel::ArrayRef<keel::usize> selectedIndices,
    keel::ArrayRef<SelectItem> items,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float reserveInputW
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
    const float chipH = keel::max(14.0f, boxH - 8.0f);
    const float chipY = textRect.y + (textRect.h - chipH) * 0.5f;
    const float chipFont = keel::max(10.0f, theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, size));
    const float chipIconSz = keel::max(10.0f, m.iconSize - 2.0f);
    const float removeIconSz = keel::max(8.0f, m.iconSize - 4.0f);
    const float removeBtnW = removeIconSz + 6.0f;
    const float chipGap = 4.0f;

    const float availW = keel::max(24.0f, textRect.w - reserveInputW);

    keel::SmallArray<float, 16> chipWidths(validEntries.size(), 0.0f);
    for (keel::usize i = 0; i < validEntries.size(); ++i) {
        const SelectItem& it = items[validEntries[i].itemIdx];
        const float textW = TextUtil::MeasureUiTextWidth(it.text, chipFont, canvas);
        const float iconW = (!it.prefix.icon.IsEmpty()) ? (chipIconSz + 4.0f) : 0.0f;
        chipWidths[i] = 6.0f + iconW + textW + 4.0f + removeBtnW;
    }

    const keel::usize maxCap = keel::min(validEntries.size(), SelectData::kMaxChips);
    float totalAllW = 0.0f;
    for (keel::usize i = 0; i < validEntries.size(); ++i) {
        if (i > 0) totalAllW += chipGap;
        totalAllW += chipWidths[i];
    }

    keel::usize fitCount = 0;
    float badgeW = 0.0f;
    if (validEntries.size() <= SelectData::kMaxChips && totalAllW <= availW) {
        fitCount = validEntries.size();
        layout.overflowCount = 0;
    } else {
        for (keel::usize count = maxCap; count > 0; --count) {
            const keel::usize rem = validEntries.size() - count;
            if (rem == 0) continue;
            char badgeBuf[32]{};
            const keel::StringRef badgeStr = keel::format_buf(badgeBuf, "+%zu", rem);
            const float bw = TextUtil::MeasureWidth(badgeStr, chipFont, canvas) + 12.0f;
            float sumW = 0.0f;
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

    float curX = textRect.x;
    layout.visibleChips.reserve(fitCount);
    for (keel::usize i = 0; i < fitCount; ++i) {
        const float w = chipWidths[i];
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

    const bool hasBg = opts.background.IsBackgroundEnabled(true);
    Color boxBg = hasBg
        ? ((hovered && !disabled) ? pal.surface : pal.inputBackground)
        : Color::Transparent();
    Color boxBorder;
    float strokeW = (opts.border.width > 0.0f) ? theme.Scale(opts.border.width) : m.strokeWidth;

    if ((open || focused) && !disabled) {
        boxBorder = pal.focusRing;
        if (opts.border.width > 0.0f || m.strokeWidth > 0.0f) strokeW += 0.5f;
    } else if (hovered && !disabled) {
        boxBorder = pal.borderHover;
    } else {
        boxBorder = pal.inputBorder;
    }

    Color placeholderCol = theme.ResolveTextColor(opts.placeholder, UiVariant::Muted, disabled);
    Color chevronCol = open ? primaryCol : ((data.chevronHovered && !disabled) ? pal.textPrimary : pal.textSecondary);
    Color unitCol = pal.textSecondary;

    if (disabled) {
        boxBg = boxBg.WithAlphaF(0.40f);
        boxBorder = boxBorder.WithAlphaF(0.40f);
        chevronCol = chevronCol.WithAlphaF(0.40f);
        unitCol = unitCol.WithAlphaF(0.40f);
    }

    if (hasBg && opts.background.HasAny()) {
        if (!opts.background.IsSurface()) {
            boxBg = theme.ResolveColor(opts.background, hovered, open || focused, disabled, boxBg);
        } else if (opts.background.HasAlphaScale()) {
            boxBg = boxBg.ScaleAlpha(opts.background.alphaScale);
        }
    }
    if (opts.border.color.HasAny()) {
        boxBorder = theme.ResolveColor(opts.border.color, hovered, open || focused, disabled, boxBorder);
    }

    const float radius = opts.border.radius.has_value() ? theme.Scale(*opts.border.radius) : m.cornerRadius;
    canvas.DrawRoundRect(boxRect, radius, boxBg, boxBorder, strokeW);

    // Left prefix
    float leftX = boxRect.x + m.paddingX;
    if (opts.prefix.HasContentOrReserve()) {
        const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const Color pTextCol = theme.ResolveTextColor(opts.prefix.text, UiVariant::Secondary, disabled, hovered, open || focused);
        const Color pIconCol = opts.prefix.text.color.HasAny() ? pTextCol : chevronCol;
        const float pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, &canvas);
        TextUtil::DrawAffix(canvas, opts.prefix, Rect(leftX, boxRect.y, pW, boxRect.h), pFont, m.iconSize, pTextCol, pIconCol, 4.0f);
        leftX += pW + 6.0f;
    }

    // Right elements (chevron + clear button + suffix)
    const float btnSize = m.iconSize + 4.0f;
    float rightX = boxRect.x + boxRect.w - m.paddingX - m.iconSize;
    if (data.chevronHovered && !disabled && opts.searchable) {
        const Rect chevBtnRect(rightX - 2.0f, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        canvas.DrawRoundRect(chevBtnRect, 4.0f, pal.surfaceHover, Color::Transparent(), 0.0f);
    }
    const Rect chevronRect(rightX, boxRect.y + (boxRect.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
    const Icon chevron = open ? Icon::ChevronUp() : Icon::ChevronDown();
    chevron.Draw(canvas, chevronRect, chevronCol);
    rightX -= 6.0f;

    if (data.showClearBtn) {
        rightX -= btnSize;
        const Rect clearBtnRect(rightX, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        if (data.clearHovered && !disabled) {
            canvas.DrawRoundRect(clearBtnRect, 4.0f, pal.surfaceHover, Color::Transparent(), 0.0f);
        }
        const float clearIconSz = keel::max(8.0f, m.iconSize - 2.0f);
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
        const float sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
        const Color sTextCol = theme.ResolveTextColor(opts.suffix.text, UiVariant::Muted, disabled, hovered, open || focused);
        const Color sIconCol = opts.suffix.text.color.HasAny() ? sTextCol : unitCol;
        const float sW = TextUtil::MeasureAffixWidth(opts.suffix, sFont, m.iconSize, 4.0f, &canvas);
        rightX -= sW;
        TextUtil::DrawAffix(canvas, opts.suffix, Rect(rightX, boxRect.y, sW, boxRect.h), sFont, m.iconSize, sTextCol, sIconCol, 4.0f);
        rightX -= 6.0f;
    }

    const float availW = keel::max(0.0f, rightX - leftX);
    const Rect textRect(leftX, boxRect.y, availW, boxRect.h);
    canvas.PushClip(textRect);

    const float chipH = keel::max(14.0f, boxRect.h - 8.0f);
    const float chipY = boxRect.y + (boxRect.h - chipH) * 0.5f;
    const float chipFont = keel::max(10.0f, theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, size));
    const float chipIconSz = keel::max(10.0f, m.iconSize - 2.0f);
    const float removeIconSz = keel::max(8.0f, m.iconSize - 4.0f);
    const float removeBtnW = removeIconSz + 6.0f;
    const float chipGap = 4.0f;

    const Color chipBg = disabled ? primaryCol.WithAlphaF(0.08f) : primaryCol.WithAlphaF(0.16f);
    const Color chipBorder = disabled ? primaryCol.WithAlphaF(0.20f) : primaryCol.WithAlphaF(0.40f);
    const Color chipTextCol = disabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary;

    float curX = textRect.x;
    for (keel::usize i = 0; i < data.chipCount && i < SelectData::kMaxChips; ++i) {
        const SelectChipVisual& chip = data.chips[i];
        const float textW = TextUtil::MeasureUiTextWidth(chip.text, chipFont, &canvas);
        const float iconW = (!chip.prefix.icon.IsEmpty()) ? (chipIconSz + 4.0f) : 0.0f;
        float w = 6.0f + iconW + textW + 4.0f + removeBtnW;
        if (data.chipCount == 1 && data.totalSelectedCount == 1 && w > availW) {
            w = availW;
        }

        const Rect cRect(curX, chipY, w, chipH);
        canvas.DrawRoundRect(cRect, 4.0f, chipBg, chipBorder, 1.0f);

        float innerX = curX + 6.0f;
        if (!chip.prefix.icon.IsEmpty()) {
            const Rect icRect(innerX, chipY + (chipH - chipIconSz) * 0.5f, chipIconSz, chipIconSz);
            chip.prefix.icon.Draw(canvas, icRect, disabled ? primaryCol.WithAlphaF(0.40f) : primaryCol);
            innerX += chipIconSz + 4.0f;
        }

        const float maxTxtW = keel::max(0.0f, (curX + w - removeBtnW - 2.0f) - innerX);
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
        const float bw = keel::min(keel::max(0.0f, textRect.Right() - curX), TextUtil::MeasureWidth(badgeStr, chipFont, &canvas) + 12.0f);
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
        const float inputFont = theme.ResolveFontSize(data.displayText, TextRole::LabelMedium, size);
        const Color textCol = disabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary;
        const Rect editRect(curX, boxRect.y, keel::max(0.0f, textRect.Right() - curX), boxRect.h);

        if (displayText.empty() && data.totalSelectedCount == 0 && !opts.placeholder.empty()) {
            const float phFont = !opts.placeholder.size.IsAuto()
                ? theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size)
                : inputFont;
            canvas.DrawTextBox(opts.placeholder.text, editRect, phFont, placeholderCol, 0.0f, 0.5f);
        } else if (!displayText.empty()) {
            const float fullW = keel::max(editRect.w, canvas.MeasureText(displayText, inputFont).x + 8.0f);
            const Rect scrolledRect(editRect.x - data.scrollX, editRect.y, fullW, editRect.h);
            canvas.DrawTextBox(displayText, scrolledRect, inputFont, textCol, 0.0f, 0.5f);
        }

        if (!disabled && data.cursorVisible) {
            const keel::usize safeCursor = keel::min(data.cursor, displayText.size());
            const keel::StringRef sub = displayText.substr(0, safeCursor);
            const float subW = canvas.MeasureText(sub, inputFont).x;
            const float caretX = editRect.x - data.scrollX + subW;
            const float caretH = inputFont * 1.15f;
            const float caretY = boxRect.y + (boxRect.h - caretH) * 0.5f;
            canvas.DrawLine({caretX, caretY}, {caretX, caretY + caretH}, pal.caret, data.cursorWidth);
        }
    } else if (data.totalSelectedCount == 0 && !opts.placeholder.empty()) {
        const float phFont = theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size);
        const float alignX = (opts.align == Align::End) ? 1.0f : ((opts.align == Align::Center) ? 0.5f : 0.0f);
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
    float scrollY,
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

    const Color popupBg = pal.surfaceElevated;
    const Color popupBorder = pal.border;
    const Color shadowCol = Color(0, 0, 0, 50);

    canvas.DrawRoundRect(Rect(popupRect.x, popupRect.y + 2.0f, popupRect.w, popupRect.h), m.cornerRadius + 1.0f, shadowCol);
    canvas.DrawRoundRect(popupRect, m.cornerRadius, popupBg, popupBorder, 1.0f);

    if (items.empty()) {
        const Rect emptyRect(popupRect.x + m.paddingX, popupRect.y + 4.0f, keel::max(0.0f, popupRect.w - m.paddingX * 2.0f), keel::max(0.0f, popupRect.h - 8.0f));
        canvas.PushClip(emptyRect);
        const UiText emptyTxt = opts.emptyText.empty() ? UiText{"No matching items"} : opts.emptyText;
        const float emptyFont = theme.ResolveFontSize(emptyTxt, TextRole::LabelMedium, size);
        const Color emptyCol = theme.ResolveTextColor(emptyTxt, UiVariant::Muted, false);
        TextUtil::DrawUiText(canvas, emptyTxt, emptyRect, emptyFont, emptyRect.h, emptyCol, 0.0f, 0.5f, &theme);
        canvas.PopClip();
        return;
    }

    const float viewportH = keel::max(0.0f, popupRect.h - 8.0f);
    const float totalContentH = static_cast<float>(items.size()) * m.itemHeight;
    const float maxScrollY = keel::max(0.0f, totalContentH - viewportH);
    const bool hasScrollbar = (maxScrollY > 0.5f);
    const float clampedScrollY = keel::clamp(scrollY, 0.0f, maxScrollY);
    const float itemW = keel::max(0.0f, popupRect.w - 8.0f - (hasScrollbar ? sm.gutter : 0.0f));
    const Rect itemsClipRect(popupRect.x + 1.0f, popupRect.y + 4.0f, keel::max(0.0f, popupRect.w - 2.0f), viewportH);

    canvas.PushClip(itemsClipRect);

    const float cbSize = 14.0f;

    for (keel::usize i = 0; i < items.size(); ++i) {
        const SelectItem& item = items[i];
        const Rect itemRect(
            popupRect.x + 4.0f,
            popupRect.y + 4.0f + static_cast<float>(i) * m.itemHeight - clampedScrollY,
            itemW,
            m.itemHeight
        );

        if (itemRect.Bottom() <= itemsClipRect.y || itemRect.y >= itemsClipRect.Bottom()) {
            continue;
        }

        const bool itemDisabled = item.disabled;
        const bool isSelected = (i < itemSelectedFlags.size()) && (itemSelectedFlags[i] != 0);
        const bool isHovered = (static_cast<keel::i32>(i) == hoveredIndex) && !itemDisabled;

        if (isHovered) {
            canvas.DrawRoundRect(itemRect, 4.0f, isSelected ? pal.surfaceActive : pal.surfaceHover);
        } else if (isSelected) {
            canvas.DrawRoundRect(itemRect, 4.0f, primaryCol.WithAlphaF(0.08f));
        }

        Color itemTextCol = item.text.color.HasAny()
            ? theme.ResolveTextColor(item.text, UiVariant::Default, itemDisabled, isHovered, isSelected)
            : (itemDisabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary);

        // Right side: item suffix
        float textRightX = itemRect.x + itemRect.w - m.paddingX;
        if (item.suffix.HasContentOrReserve()) {
            const float sFont = theme.ResolveFontSize(item.suffix.text, TextRole::LabelMedium, size);
            const Color sTextCol = theme.ResolveTextColor(item.suffix.text, UiVariant::Muted, itemDisabled);
            const float sW = TextUtil::MeasureAffixWidth(item.suffix, sFont, m.iconSize, 4.0f, &canvas);
            textRightX -= sW;
            TextUtil::DrawAffix(canvas, item.suffix, Rect(textRightX, itemRect.y, sW, itemRect.h), sFont, m.iconSize, sTextCol, sTextCol, 4.0f);
            textRightX -= 6.0f;
        }

        // Left side: 14x14 rounded checkbox + optional prefix + main text
        float textLeftX = itemRect.x + m.paddingX;
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
            const float pFont = theme.ResolveFontSize(item.prefix.text, TextRole::LabelMedium, size);
            const Color pTextCol = theme.ResolveTextColor(item.prefix.text, UiVariant::Secondary, itemDisabled);
            const float pW = TextUtil::MeasureAffixWidth(item.prefix, pFont, m.iconSize, 4.0f, &canvas);
            TextUtil::DrawAffix(canvas, item.prefix, Rect(textLeftX, itemRect.y, pW, itemRect.h), pFont, m.iconSize, pTextCol, pTextCol, 4.0f);
            textLeftX += pW + 6.0f;
        }

        const float selFont = theme.ResolveFontSize(item.text, TextRole::LabelMedium, size);
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
        const float trackX = popupRect.Right() - sm.trackWidth - sm.margin;
        const float trackY = popupRect.y + sm.margin;
        const float trackH = keel::max(0.0f, popupRect.h - 2.0f * sm.margin);
        const float thumbH = keel::clamp((viewportH / keel::max(1.0f, totalContentH)) * trackH, sm.minThumbLength, trackH);
        const float travelH = keel::max(0.0f, trackH - thumbH);
        const float scrollRatio = (maxScrollY > 0.0f) ? (clampedScrollY / maxScrollY) : 0.0f;
        const float thumbY = trackY + scrollRatio * travelH;

        const Rect trackRect(trackX, trackY, sm.trackWidth, trackH);
        const Rect thumbRect(trackX, thumbY, sm.trackWidth, thumbH);

        canvas.DrawRoundRect(trackRect, sm.cornerRadius, pal.borderSubtle);

        Color thumbCol = pal.textMuted;
        if (scrollbarActive) {
            thumbCol = primaryCol;
        } else if (scrollbarHovered) {
            thumbCol = pal.textSecondary;
        }
        canvas.DrawRoundRect(thumbRect, sm.cornerRadius, thumbCol);
        canvas.PopClip();
    }
}

} // namespace

Response Select::RunMulti(
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

Response Select::RunMulti(
    Context& ctx,
    IndexListSink selectedIndices,
    keel::ArrayRef<SelectItem> items,
    const SelectOptions& opts,
    keel::SourceLocation loc
) {
    const ZoneScope zoneScope = ctx.Zone({.theme = opts.theme, .scale = opts.scale});

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
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
    if (!optsCopy.width.IsFixed()) {
        optsCopy.width = effWidthMode;
    }

    auto measureRowContentW = [&](const UiText& mainText, const UiAffix& pfx, const UiAffix& sfx) -> float {
        const float mainFont = theme.ResolveFontSize(mainText, TextRole::LabelMedium, activeSize);
        float w = TextUtil::MeasureUiTextWidth(mainText, mainFont, canvas);
        if (pfx.HasContentOrReserve()) {
            const float pFont = theme.ResolveFontSize(pfx.text, TextRole::LabelMedium, activeSize);
            w += TextUtil::MeasureAffixWidth(pfx, pFont, m.iconSize, 4.0f, canvas) + 6.0f;
        }
        if (sfx.HasContentOrReserve()) {
            const float sFont = theme.ResolveFontSize(sfx.text, TextRole::LabelMedium, activeSize);
            w += TextUtil::MeasureAffixWidth(sfx, sFont, m.iconSize, 4.0f, canvas) + 6.0f;
        }
        return w;
    };

    float maxRowW = 0.0f;
    for (const SelectItem& it : items) {
        maxRowW = keel::max(maxRowW, measureRowContentW(it.text, it.prefix, it.suffix) + 22.0f);
    }
    if (optsCopy.placeholder.HasContentOrReserve()) {
        maxRowW = keel::max(maxRowW, measureRowContentW(optsCopy.placeholder, optsCopy.prefix, optsCopy.suffix));
    }

    const bool exceedsVisibleItems = optsCopy.maxVisibleItems.has_value()
        && *optsCopy.maxVisibleItems > 0
        && items.size() > static_cast<keel::usize>(*optsCopy.maxVisibleItems);

    const float clearExtraW = optsCopy.clearable ? (m.iconSize + 8.0f) : 0.0f;
    float intrinsicW = maxRowW + m.paddingX * 2.0f + m.iconSize + 8.0f + clearExtraW + (exceedsVisibleItems ? sm.gutter : 0.0f);
    if (optsCopy.minWidth.has_value()) {
        intrinsicW = keel::max(theme.Scale(*optsCopy.minWidth), intrinsicW);
    } else {
        intrinsicW = keel::max(m.minWidth, intrinsicW);
    }
    if (optsCopy.width.IsFixed()) {
        intrinsicW = theme.Scale(optsCopy.width.Dp());
    }

    const ControlSpec ctrl = EffectiveSelectControl(optsCopy, activeSize, theme);
    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    Vec2 fallbackDims = Select::Measure(optsCopy, activeSize, canvas, theme);
    if (effWidthMode == SizeMode::Fit && !optsCopy.width.IsFixed()) {
        fallbackDims.x = intrinsicW;
    }
    const Rect boxRect = (prevBounds.w > 0.0f && prevBounds.h > 0.0f)
        ? prevBounds
        : Rect({0.0f, 0.0f}, fallbackDims);

    struct MultiSelectPopupState {
        float scrollY{0.0f};
        bool draggingScrollbar{false};
        float dragStartMouseY{0.0f};
        float dragStartScrollY{0.0f};
        bool scrollbarHovered{false};
        bool scrollbarActive{false};
        keel::SmallString<64> filterText{};
        float scrollX{0.0f};
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

    const float inputFont = theme.ResolveFontSize(optsCopy.placeholder, TextRole::LabelMedium, activeSize);
    const float reserveW = (wasOpen && isSearchable && !popupState.filterText.empty())
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
                hoveredChipRemove = static_cast<keel::i32>(ci);
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
            popupState.highlightedFilteredIdx >= static_cast<keel::i32>(filteredIndices.size())) {
            popupState.highlightedFilteredIdx = -1;
            for (keel::usize fi = 0; fi < filteredIndices.size(); ++fi) {
                if (!items[filteredIndices[fi]].disabled) {
                    popupState.highlightedFilteredIdx = static_cast<keel::i32>(fi);
                    break;
                }
            }
        }
    }

    const keel::usize effectiveRowCount = filteredIndices.empty()
        ? (isSearchable ? 1 : 0)
        : filteredIndices.size();
    const float totalContentH = static_cast<float>(effectiveRowCount) * m.itemHeight;

    auto scrollItemIntoView = [&](keel::usize filteredPos, float viewportH, float maxScrollY) {
        if (filteredPos >= filteredIndices.size() || maxScrollY <= 0.0f || viewportH <= 0.0f) {
            return;
        }
        const float itemTop = static_cast<float>(filteredPos) * m.itemHeight;
        const float itemBottom = itemTop + m.itemHeight;
        if (itemTop < popupState.scrollY) {
            popupState.scrollY = itemTop;
        } else if (itemBottom > popupState.scrollY + viewportH) {
            popupState.scrollY = itemBottom - viewportH;
        }
        popupState.scrollY = keel::clamp(popupState.scrollY, 0.0f, maxScrollY);
    };

    auto computePopupRect = [&](keel::usize rowCount) -> Rect {
        const float popupW = boxRect.w > 0.0f ? boxRect.w : intrinsicW;
        const keel::usize clampedRows = keel::max<keel::usize>(1, rowCount);
        const keel::usize visibleCount = (opts.maxVisibleItems.has_value() && *opts.maxVisibleItems > 0)
            ? keel::min(clampedRows, static_cast<keel::usize>(*opts.maxVisibleItems))
            : clampedRows;
        const float contentH = static_cast<float>(visibleCount) * m.itemHeight + 8.0f;
        float popupH = contentH;
        float popupX = boxRect.x;
        float popupY = boxRect.y + boxRect.h + 4.0f;

        const Rect vp = ctx.GetPopupViewportRect();
        if (vp.h > 0.0f) {
            const float availDown = vp.Bottom() - popupY - 6.0f;
            const float availUp = boxRect.y - 4.0f - (vp.y + 6.0f);
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
            const float viewportH = keel::max(0.0f, popupRect.h - 8.0f);
            const float maxScrollY = keel::max(0.0f, totalContentH - viewportH);
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
        const float viewportH = keel::max(0.0f, popupRect.h - 8.0f);
        const float maxScrollY = keel::max(0.0f, totalContentH - viewportH);
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
            const float trackX = popupRect.Right() - sm.trackWidth - sm.margin;
            const float trackY = popupRect.y + sm.margin;
            const float trackH = keel::max(0.0f, popupRect.h - 2.0f * sm.margin);
            const float thumbH = keel::clamp((viewportH / keel::max(1.0f, totalContentH)) * trackH, sm.minThumbLength, trackH);
            const float travelH = keel::max(0.0f, trackH - thumbH);
            const float scrollRatio = (maxScrollY > 0.0f) ? keel::clamp(popupState.scrollY / maxScrollY, 0.0f, 1.0f) : 0.0f;
            const float thumbY = trackY + scrollRatio * travelH;

            const Rect trackRect(trackX, trackY, sm.trackWidth, trackH);
            const Rect thumbRect(trackX, thumbY, sm.trackWidth, thumbH);
            const Rect gutterRect(popupRect.Right() - sm.gutter, popupRect.y, sm.gutter, popupRect.h);

            const bool overThumb = thumbRect.Contains(mouse);
            const bool overTrack = trackRect.Contains(mouse) || gutterRect.Contains(mouse);
            popupState.scrollbarHovered = overThumb || overTrack;

            if (ctx.IsMouseClicked() && overTrack) {
                if (overThumb) {
                    popupState.draggingScrollbar = true;
                    popupState.dragStartMouseY = mouse.y;
                    popupState.dragStartScrollY = popupState.scrollY;
                    ctx.SetActiveId(id);
                } else {
                    if (mouse.y < thumbRect.y) {
                        popupState.scrollY = keel::max(0.0f, popupState.scrollY - viewportH * 0.8f);
                    } else if (mouse.y > thumbRect.Bottom()) {
                        popupState.scrollY = keel::min(maxScrollY, popupState.scrollY + viewportH * 0.8f);
                    }
                }
                ctx.ConsumeMouseClick();
            }

            if (popupState.draggingScrollbar) {
                if (ctx.IsMouseDown()) {
                    if (travelH > 0.0f) {
                        const float deltaY = mouse.y - popupState.dragStartMouseY;
                        popupState.scrollY = keel::clamp(
                            popupState.dragStartScrollY + (deltaY / travelH) * maxScrollY,
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

        const float itemW = keel::max(0.0f, popupRect.w - 8.0f - (hasScrollbar ? sm.gutter : 0.0f));
        const Rect itemsViewport(popupRect.x + 4.0f, popupRect.y + 4.0f, itemW, viewportH);

        if (!popupState.draggingScrollbar && !popupState.scrollbarHovered && itemsViewport.Contains(mouse)) {
            for (keel::usize fi = 0; fi < filteredIndices.size(); ++fi) {
                const keel::usize origIdx = filteredIndices[fi];
                const Rect itemRect(
                    popupRect.x + 4.0f,
                    popupRect.y + 4.0f + static_cast<float>(fi) * m.itemHeight - popupState.scrollY,
                    itemW,
                    m.itemHeight
                );
                if (itemRect.Bottom() <= itemsViewport.y || itemRect.y >= itemsViewport.Bottom()) {
                    continue;
                }
                if (itemRect.Contains(mouse)) {
                    if (!items[origIdx].disabled) {
                        hoveredItem = static_cast<keel::i32>(fi);
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
                    popupState.highlightedFilteredIdx < static_cast<keel::i32>(filteredIndices.size())) {
                    const keel::usize chosen = filteredIndices[static_cast<keel::usize>(popupState.highlightedFilteredIdx)];
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
                        : static_cast<keel::usize>(popupState.highlightedFilteredIdx + 1);
                    for (keel::usize fi = startFi; fi < filteredIndices.size(); ++fi) {
                        if (!items[filteredIndices[fi]].disabled) {
                            popupState.highlightedFilteredIdx = static_cast<keel::i32>(fi);
                            scrollItemIntoView(fi, viewportH, maxScrollY);
                            break;
                        }
                    }
                }
            } else if (ctx.ConsumeKey(Key::Up)) {
                if (!filteredIndices.empty() && popupState.highlightedFilteredIdx > 0) {
                    for (keel::usize fi = static_cast<keel::usize>(popupState.highlightedFilteredIdx); fi > 0; --fi) {
                        const keel::usize candidateFi = fi - 1;
                        if (!items[filteredIndices[candidateFi]].disabled) {
                            popupState.highlightedFilteredIdx = static_cast<keel::i32>(candidateFi);
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

    const float finalReserveW = (isOpen && isSearchable && !popupState.filterText.empty())
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
        const float popupScrollY = popupState.scrollY;
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
            .removeHovered = (static_cast<keel::i32>(i) == hoveredChipRemove)
        };
        chipsHash = detail::HashArgs(chipsHash, cb.itemIdx, data->chips[i].removeHovered);
    }

    node->text = optsCopy.placeholder.text;
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        static_cast<keel::u8>(optsCopy.align),
        optsCopy.width,
        optsCopy.height,
        optsCopy.minWidth.value_or(0.0f),
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
        optsCopy.background,
        optsCopy.border
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<SelectData>();
        Vec2 sz = Select::Measure(d ? d->opts : SelectOptions{}, n->uiSize, canvas, theme, availableWidth);
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
    node->focused = finalFocused;
    res.focused = finalFocused;

    Control::SetupNode(ctx, node, ctrl);

    return res;
}

} // namespace guikit
