#include "guikit/widgets/Select.hpp"
#include <algorithm>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

ControlSpec EffectiveSelectControl(const SelectOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& m = theme.GetSelect(size);
    ControlSpec c{};
    c.width = opts.width;
    c.height = opts.height;
    c.minWidth = opts.minWidth;
    c.align = opts.align;
    c.background = opts.background;
    c.border = opts.border;
    c.defaultLayout = ElementLayout::Stacked;
    c.defaultInlineOrder = InlineOrder::LabelFirst;
    c.labelGap = m.labelGap;
    c.descGap = m.descGap;
    c.inlineGap = m.paddingX;
    c.stretchable = true;
    c.stretchHeightWhenStandalone = false;
    c.hasIntrinsicBox = true;
    return c;
}

Vec2 MeasureSelectBox(const SelectOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& m = theme.GetSelect(size);
    const ControlSpec ctrl = EffectiveSelectControl(opts, size, theme);
    float boxW = (opts.stretch != StretchMode::Fill && ctrl.width.has_value()) ? theme.Scale(*ctrl.width) : m.defaultWidth;
    if (ctrl.minWidth.has_value()) {
        boxW = std::max(theme.Scale(*ctrl.minWidth), boxW);
    } else {
        boxW = std::max(m.minWidth, boxW);
    }
    return {boxW, m.defaultHeight};
}

size_t SnapToUtf8CodepointBoundary(std::string_view text, size_t idx) noexcept {
    idx = std::min(idx, text.size());
    while (idx > 0 && idx < text.size() && (static_cast<unsigned char>(text[idx]) & 0xC0) == 0x80) {
        --idx;
    }
    return idx;
}

uint32_t DecodeUtf8Codepoint(std::string_view sv, size_t& ioIdx) noexcept {
    if (ioIdx >= sv.size()) return 0;
    const unsigned char b0 = static_cast<unsigned char>(sv[ioIdx]);
    if (b0 < 0x80) {
        ++ioIdx;
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0 && ioIdx + 1 < sv.size()) {
        const uint32_t cp = ((b0 & 0x1Fu) << 6) | (static_cast<unsigned char>(sv[ioIdx + 1]) & 0x3Fu);
        ioIdx += 2;
        return cp;
    }
    if ((b0 & 0xF0) == 0xE0 && ioIdx + 2 < sv.size()) {
        const uint32_t cp = ((b0 & 0x0Fu) << 12) |
                            ((static_cast<unsigned char>(sv[ioIdx + 1]) & 0x3Fu) << 6) |
                            (static_cast<unsigned char>(sv[ioIdx + 2]) & 0x3Fu);
        ioIdx += 3;
        return cp;
    }
    if ((b0 & 0xF8) == 0xF0 && ioIdx + 3 < sv.size()) {
        const uint32_t cp = ((b0 & 0x07u) << 18) |
                            ((static_cast<unsigned char>(sv[ioIdx + 1]) & 0x3Fu) << 12) |
                            ((static_cast<unsigned char>(sv[ioIdx + 2]) & 0x3Fu) << 6) |
                            (static_cast<unsigned char>(sv[ioIdx + 3]) & 0x3Fu);
        ioIdx += 4;
        return cp;
    }
    ++ioIdx;
    return b0;
}

uint32_t ToLowerCodepoint(uint32_t cp) noexcept {
    if (cp >= 'A' && cp <= 'Z') {
        return cp + ('a' - 'A');
    }
    if (cp >= 0x0410 && cp <= 0x042F) { // Cyrillic А..Я -> а..я
        return cp + 0x20;
    }
    if (cp == 0x0401) { // Ё -> ё
        return 0x0451;
    }
    return cp;
}

bool FindCaseInsensitiveSubstring(
    std::string_view haystack,
    std::string_view needle,
    size_t& outBytePos,
    size_t& outByteLen
) noexcept {
    outBytePos = 0;
    outByteLen = 0;
    if (needle.empty()) {
        return true;
    }
    if (haystack.empty()) {
        return false;
    }

    size_t hStart = 0;
    while (hStart < haystack.size()) {
        const size_t matchStart = hStart;
        size_t hCursor = hStart;
        size_t nCursor = 0;
        bool matched = true;
        while (nCursor < needle.size()) {
            if (hCursor >= haystack.size()) {
                matched = false;
                break;
            }
            const uint32_t hCp = ToLowerCodepoint(DecodeUtf8Codepoint(haystack, hCursor));
            const uint32_t nCp = ToLowerCodepoint(DecodeUtf8Codepoint(needle, nCursor));
            if (hCp != nCp) {
                matched = false;
                break;
            }
        }
        if (matched) {
            outBytePos = matchStart;
            outByteLen = hCursor - matchStart;
            return true;
        }
        DecodeUtf8Codepoint(haystack, hStart);
    }
    return false;
}

bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needle) noexcept {
    size_t pos = 0;
    size_t len = 0;
    return FindCaseInsensitiveSubstring(haystack, needle, pos, len);
}

bool MatchesSelectFilter(const SelectItem& item, std::string_view query) noexcept {
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

    const float availW = std::max(0.0f, rightX - leftX);
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
    float ctrlW = std::min(bounds.w, boxDims.x);
    if (opts.stretch == StretchMode::Fill) {
        const float ratio = std::clamp(opts.width.value_or(1.0f), 0.0f, 1.0f);
        ctrlW = bounds.w * ratio;
    }
    const float ctrlX = bounds.x + (bounds.w - ctrlW) * alignFactor;
    const float ctrlH = std::min(boxDims.y, bounds.h);
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
    return Control::Measure(boxDims, ctrl, opts.stretch, size, theme);
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
    size_t* selectedIndex,
    int* selectedIntIndex,
    std::string* selectedString,
    std::string_view* selectedStringView,
    std::span<const std::string_view> items,
    const SelectOptions& opts,
    std::source_location loc
) {
    std::vector<SelectItem> richItems;
    richItems.reserve(items.size());
    for (std::string_view sv : items) {
        richItems.push_back(SelectItem{.text = sv});
    }
    return Run(ctx, selectedIndex, selectedIntIndex, selectedString, selectedStringView, richItems, opts, loc);
}

Response Select::Run(
    Context& ctx,
    size_t* selectedIndex,
    int* selectedIntIndex,
    std::string* selectedString,
    std::string_view* selectedStringView,
    std::span<const SelectItem> items,
    const SelectOptions& opts,
    std::source_location loc
) {
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
    auto findMatchingIndex = [&](std::string_view sv) -> size_t {
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i].text.text == sv) {
                return i;
            }
        }
        return size_t(-1);
    };

    size_t currIndex = size_t(-1);
    if (selectedIndex != nullptr) {
        if (*selectedIndex < items.size()) currIndex = *selectedIndex;
    } else if (selectedIntIndex != nullptr) {
        if (*selectedIntIndex >= 0 && static_cast<size_t>(*selectedIntIndex) < items.size()) {
            currIndex = static_cast<size_t>(*selectedIntIndex);
        }
    } else if (selectedString != nullptr) {
        currIndex = findMatchingIndex(*selectedString);
    } else if (selectedStringView != nullptr) {
        currIndex = findMatchingIndex(*selectedStringView);
    }

    const UiTheme theme = ctx.GetTheme();
    const SelectMetrics& m = theme.GetSelect(activeSize);
    const ScrollbarMetrics& sm = theme.GetScrollbar(activeSize);
    ICanvas* canvas = ctx.GetCanvas();

    SelectOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

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
        maxRowW = std::max(maxRowW, measureRowContentW(it.text, effPfx, effSfx));
        // Also ensure popup row width (with item's own prefix & suffix) fits
        maxRowW = std::max(maxRowW, measureRowContentW(it.text, it.prefix, it.suffix));
    }
    if (optsCopy.placeholder.HasContentOrReserve()) {
        maxRowW = std::max(maxRowW, measureRowContentW(optsCopy.placeholder, optsCopy.prefix, optsCopy.suffix));
    }

    const bool exceedsVisibleItems = optsCopy.maxVisibleItems.has_value()
        && *optsCopy.maxVisibleItems > 0
        && items.size() > static_cast<size_t>(*optsCopy.maxVisibleItems);

    const float clearExtraW = optsCopy.clearable ? (m.iconSize + 8.0f) : 0.0f;
    float intrinsicW = maxRowW + m.paddingX * 2.0f + m.iconSize + 8.0f + clearExtraW + (exceedsVisibleItems ? sm.gutter : 0.0f);
    if (optsCopy.minWidth.has_value()) {
        intrinsicW = std::max(theme.Scale(*optsCopy.minWidth), intrinsicW);
    } else {
        intrinsicW = std::max(m.minWidth, intrinsicW);
    }
    if (optsCopy.stretch != StretchMode::Fill && optsCopy.width.has_value()) {
        intrinsicW = theme.Scale(*optsCopy.width);
    }

    const float unscaledIntrinsicW = (theme.scale > 0.0f) ? (intrinsicW / theme.scale) : intrinsicW;
    if (optsCopy.stretch != StretchMode::Fill && !optsCopy.width.has_value()) {
        optsCopy.width = unscaledIntrinsicW;
    }

    const ControlSpec ctrl = EffectiveSelectControl(optsCopy, activeSize, theme);
    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    const Rect boxRect = (prevBounds.w > 0.0f && prevBounds.h > 0.0f)
        ? prevBounds
        : Rect({0.0f, 0.0f}, Select::Measure(optsCopy, activeSize, canvas, theme));

    struct SelectPopupState {
        float scrollY{0.0f};
        bool draggingScrollbar{false};
        float dragStartMouseY{0.0f};
        float dragStartScrollY{0.0f};
        bool scrollbarHovered{false};
        bool scrollbarActive{false};
        std::string filterText{};
        float scrollX{0.0f};
        double lastClickTime{-10.0};
        Vec2 lastClickPos{-1000.0f, -1000.0f};
        int clickCount{0};
        bool draggingWord{false};
        size_t wordDragStart{0};
        size_t wordDragEnd{0};
        bool actionBtnDown{false};
        bool filterActive{false};
        int highlightedFilteredIdx{-1};
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
            if (selectedString != nullptr && !selectedString->empty()) return true;
            if (selectedStringView != nullptr && !selectedStringView->empty()) return true;
            return !popupState.filterText.empty();
        }
        if (wasOpen && opts.searchable && !popupState.filterText.empty()) return true;
        if (currIndex < items.size()) return true;
        if (selectedString != nullptr && !selectedString->empty()) return true;
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
            || (selectedIndex != nullptr && *selectedIndex != size_t(-1))
            || (selectedIntIndex != nullptr && *selectedIntIndex != -1)
            || (selectedString != nullptr && !selectedString->empty())
            || (selectedStringView != nullptr && !selectedStringView->empty());

        currIndex = size_t(-1);
        if (selectedIndex != nullptr) *selectedIndex = size_t(-1);
        if (selectedIntIndex != nullptr) *selectedIntIndex = -1;
        if (selectedString != nullptr) selectedString->clear();
        if (selectedStringView != nullptr) *selectedStringView = std::string_view{};
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

    size_t cursor = 0;
    size_t selAnchor = 0;
    bool editChanged = false;
    bool editSubmitted = false;
    bool isEditingText = false;
    std::string_view editDisplayText{};

    if (!effectiveDisabled && isSearchable) {
        const bool shouldRunTextEdit = isEditable
            ? (isFocused || ctx.HasActiveTextEdit(id))
            : (wasOpen || isFocused || ctx.HasActiveTextEdit(id));

        if (shouldRunTextEdit) {
            std::string_view initText = isEditable
                ? (selectedString != nullptr ? std::string_view(*selectedString)
                   : (selectedStringView != nullptr ? *selectedStringView
                      : (currIndex < items.size() ? items[currIndex].text.text : std::string_view(popupState.filterText))))
                : std::string_view(popupState.filterText);

            std::string_view currentEditText = ctx.EnsureTextEdit(id, initText, /*multiline=*/false, /*readOnly=*/false);
            showClearBtn = opts.clearable && (!currentEditText.empty() || (!isEditable && currIndex < items.size()));
            rects = ResolveSelectBoxRects(boxRect, activePrefix, activeSuffix, activeSize, canvas, theme, showClearBtn);
            clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);
            chevronHovered = res.hovered && rects.chevronRect.Contains(mousePos);

            const float inputFont = theme.ResolveFontSize(optsCopy.placeholder, TextRole::LabelMedium, activeSize);
            const bool allowMouseCaretHit = isEditable || (wasOpen && !currentEditText.empty());

            if ((mouseJustPressed || res.pressed) && !popupState.actionBtnDown && !clearHovered && !chevronHovered && allowMouseCaretHit) {
                const float localX = (mousePos.x - rects.textRect.x) + popupState.scrollX;
                size_t hitIdx = TextUtil::HitTestIndex(currentEditText, localX, inputFont, canvas);
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
                            const size_t anchor = ctx.GetTextEditAnchor(id);
                            ctx.SetTextEditCursor(id, hitIdx, anchor);
                        } else {
                            ctx.SetTextEditCursor(id, hitIdx, hitIdx);
                        }
                    } else if (popupState.clickCount == 2) {
                        size_t wStart = 0;
                        size_t wEnd = 0;
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
                        size_t wStart = 0;
                        size_t wEnd = 0;
                        TextUtil::SelectWordAt(currentEditText, hitIdx, wStart, wEnd);
                        wStart = SnapToUtf8CodepointBoundary(currentEditText, wStart);
                        wEnd = SnapToUtf8CodepointBoundary(currentEditText, wEnd);
                        if (hitIdx < popupState.wordDragStart) {
                            ctx.SetTextEditCursor(id, wStart, popupState.wordDragEnd);
                        } else {
                            ctx.SetTextEditCursor(id, wEnd, popupState.wordDragStart);
                        }
                    } else {
                        const size_t anchor = ctx.GetTextEditAnchor(id);
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
                    if (selectedString != nullptr) {
                        *selectedString = std::string(editDisplayText);
                    } else {
                        popupState.filterText = std::string(editDisplayText);
                    }
                    currIndex = findMatchingIndex(editDisplayText);
                    if (selectedIndex != nullptr) *selectedIndex = currIndex;
                    if (selectedIntIndex != nullptr) *selectedIntIndex = (currIndex < items.size()) ? static_cast<int>(currIndex) : -1;
                    if (selectedStringView != nullptr) *selectedStringView = (currIndex < items.size()) ? items[currIndex].text.text : std::string_view{};
                    popupState.filterActive = true;
                    popupState.highlightedFilteredIdx = 0;
                    res.changed = true;
                }
            } else {
                popupState.filterText = std::string(editDisplayText);
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
                std::string_view curVal = (selectedString != nullptr) ? std::string_view(*selectedString) : editDisplayText;
                popupState.filterActive = (currIndex == size_t(-1) && !curVal.empty());
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
    std::string_view activeQuery{};
    if (isOpen && isSearchable) {
        if (isEditable) {
            if (popupState.filterActive) {
                activeQuery = (selectedString != nullptr) ? std::string_view(*selectedString) : editDisplayText;
            }
        } else {
            activeQuery = popupState.filterText;
        }
    }

    std::vector<size_t> filteredIndices;
    filteredIndices.reserve(items.size());
    int currFilteredPos = -1;
    for (size_t i = 0; i < items.size(); ++i) {
        if (MatchesSelectFilter(items[i], activeQuery)) {
            if (i == currIndex) {
                currFilteredPos = static_cast<int>(filteredIndices.size());
            }
            filteredIndices.push_back(i);
        }
    }

    if (isOpen && isSearchable) {
        if (popupState.highlightedFilteredIdx < 0 ||
            static_cast<size_t>(popupState.highlightedFilteredIdx) >= filteredIndices.size()) {
            if (currFilteredPos >= 0) {
                popupState.highlightedFilteredIdx = currFilteredPos;
            } else if (!activeQuery.empty() && !filteredIndices.empty()) {
                popupState.highlightedFilteredIdx = -1;
                for (size_t fi = 0; fi < filteredIndices.size(); ++fi) {
                    if (!items[filteredIndices[fi]].disabled) {
                        popupState.highlightedFilteredIdx = static_cast<int>(fi);
                        break;
                    }
                }
            } else {
                popupState.highlightedFilteredIdx = -1;
            }
        }
    }

    const size_t effectiveRowCount = filteredIndices.empty()
        ? (isSearchable ? 1 : 0)
        : filteredIndices.size();
    const float totalContentH = static_cast<float>(effectiveRowCount) * m.itemHeight;

    auto scrollItemIntoView = [&](size_t filteredPos, float viewportH, float maxScrollY) {
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
        popupState.scrollY = std::clamp(popupState.scrollY, 0.0f, maxScrollY);
    };

    auto computePopupRect = [&](size_t rowCount) -> Rect {
        const float popupW = boxRect.w > 0.0f ? boxRect.w : intrinsicW;
        const size_t clampedRows = std::max<size_t>(1, rowCount);
        const size_t visibleCount = (opts.maxVisibleItems.has_value() && *opts.maxVisibleItems > 0)
            ? std::min(clampedRows, static_cast<size_t>(*opts.maxVisibleItems))
            : clampedRows;
        const float contentH = static_cast<float>(visibleCount) * m.itemHeight + 8.0f;
        float popupH = contentH;
        float popupX = boxRect.x;
        float popupY = boxRect.y + boxRect.h + 4.0f;

        const Vec2 vp = ctx.GetViewportSize();
        if (vp.y > 0.0f) {
            const float availDown = vp.y - popupY - 8.0f;
            const float availUp = boxRect.y - 4.0f - 8.0f;
            if (popupY + popupH > vp.y && availUp > availDown) {
                popupY = std::max(8.0f, boxRect.y - 4.0f - popupH);
                popupH = std::min(popupH, boxRect.y - 4.0f - popupY);
            } else if (popupY + popupH > vp.y) {
                popupH = std::max(m.itemHeight + 8.0f, availDown);
            }
        }
        if (vp.x > 0.0f) {
            popupX = std::clamp(popupX, 4.0f, std::max(4.0f, vp.x - popupW - 4.0f));
        }
        return Rect(popupX, popupY, popupW, popupH);
    };

    if (isOpen && effectiveRowCount > 0) {
        const Rect popupRect = computePopupRect(effectiveRowCount);
        ctx.OpenPopup(id, popupRect);
        if (!wasOpen) {
            const float viewportH = std::max(0.0f, popupRect.h - 8.0f);
            const float maxScrollY = std::max(0.0f, totalContentH - viewportH);
            popupState.scrollY = std::clamp(popupState.scrollY, 0.0f, maxScrollY);
            if (currFilteredPos >= 0) {
                scrollItemIntoView(static_cast<size_t>(currFilteredPos), viewportH, maxScrollY);
            } else {
                popupState.scrollY = 0.0f;
            }
        }
    } else if (isOpen && effectiveRowCount == 0) {
        ctx.ClosePopup();
        isOpen = false;
    }

    auto applySelection = [&](size_t newIdx) {
        if (newIdx >= items.size()) return;
        const bool strDiffers = (selectedString != nullptr && *selectedString != items[newIdx].text.text) ||
                                (selectedStringView != nullptr && *selectedStringView != items[newIdx].text.text);
        if (currIndex != newIdx || strDiffers) {
            res.changed = true;
            currIndex = newIdx;
            if (selectedIndex != nullptr) *selectedIndex = newIdx;
            if (selectedIntIndex != nullptr) *selectedIntIndex = static_cast<int>(newIdx);
            if (selectedString != nullptr) *selectedString = std::string(items[newIdx].text.text);
            if (selectedStringView != nullptr) *selectedStringView = items[newIdx].text.text;
        }
        if (isEditable && selectedString != nullptr) {
            editDisplayText = *selectedString;
            ctx.SetTextEditBuffer(id, *selectedString);
            cursor = selectedString->size();
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
    int hoveredItem = -1;
    if (isOpen && wasOpen && !effectiveDisabled) {
        ctx.BeginOverlayHitCheck(id);
        const Rect popupRect = ctx.GetOpenPopupRect();
        const Vec2 mouse = ctx.GetMousePos();
        const float viewportH = std::max(0.0f, popupRect.h - 8.0f);
        const float maxScrollY = std::max(0.0f, totalContentH - viewportH);
        const bool hasScrollbar = (maxScrollY > 0.5f);

        popupState.scrollY = std::clamp(popupState.scrollY, 0.0f, maxScrollY);

        // 1. Mouse wheel scrolling inside popup
        if (hasScrollbar && popupRect.Contains(mouse)) {
            const Vec2 wheel = ctx.GetPopupMouseWheel(id);
            if (wheel.y != 0.0f || wheel.x != 0.0f) {
                popupState.scrollY = std::clamp(popupState.scrollY - wheel.y * sm.scrollStep, 0.0f, maxScrollY);
                ctx.ConsumeMouseWheel();
            }
        }

        // 2. Scrollbar thumb drag & track page-jump interaction
        popupState.scrollbarHovered = false;
        if (hasScrollbar && popupRect.h > 10.0f) {
            const float trackX = popupRect.Right() - sm.trackWidth - sm.margin;
            const float trackY = popupRect.y + sm.margin;
            const float trackH = std::max(0.0f, popupRect.h - 2.0f * sm.margin);
            const float thumbH = std::clamp((viewportH / std::max(1.0f, totalContentH)) * trackH, sm.minThumbLength, trackH);
            const float travelH = std::max(0.0f, trackH - thumbH);
            const float scrollRatio = (maxScrollY > 0.0f) ? std::clamp(popupState.scrollY / maxScrollY, 0.0f, 1.0f) : 0.0f;
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
                        popupState.scrollY = std::max(0.0f, popupState.scrollY - viewportH * 0.8f);
                    } else if (mouse.y > thumbRect.Bottom()) {
                        popupState.scrollY = std::min(maxScrollY, popupState.scrollY + viewportH * 0.8f);
                    }
                }
                ctx.ConsumeMouseClick();
            }

            if (popupState.draggingScrollbar) {
                if (ctx.IsMouseDown()) {
                    if (travelH > 0.0f) {
                        const float deltaY = mouse.y - popupState.dragStartMouseY;
                        popupState.scrollY = std::clamp(
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
        const float itemW = std::max(0.0f, popupRect.w - 8.0f - (hasScrollbar ? sm.gutter : 0.0f));
        const Rect itemsViewport(popupRect.x + 4.0f, popupRect.y + 4.0f, itemW, viewportH);

        if (!popupState.draggingScrollbar && !popupState.scrollbarHovered && itemsViewport.Contains(mouse)) {
            for (size_t fi = 0; fi < filteredIndices.size(); ++fi) {
                const size_t origIdx = filteredIndices[fi];
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
                        hoveredItem = static_cast<int>(fi);
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
                    static_cast<size_t>(popupState.highlightedFilteredIdx) < filteredIndices.size()) {
                    const size_t chosen = filteredIndices[static_cast<size_t>(popupState.highlightedFilteredIdx)];
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
                        const size_t startFi = (currFilteredPos < 0) ? 0 : static_cast<size_t>(currFilteredPos + 1);
                        for (size_t fi = startFi; fi < filteredIndices.size(); ++fi) {
                            const size_t origIdx = filteredIndices[fi];
                            if (!items[origIdx].disabled) {
                                applySelection(origIdx);
                                scrollItemIntoView(fi, viewportH, maxScrollY);
                                break;
                            }
                        }
                    } else {
                        const size_t startFi = (popupState.highlightedFilteredIdx < 0)
                            ? 0
                            : static_cast<size_t>(popupState.highlightedFilteredIdx + 1);
                        for (size_t fi = startFi; fi < filteredIndices.size(); ++fi) {
                            if (!items[filteredIndices[fi]].disabled) {
                                popupState.highlightedFilteredIdx = static_cast<int>(fi);
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
                            for (size_t fi = static_cast<size_t>(currFilteredPos); fi > 0; --fi) {
                                const size_t candidateFi = fi - 1;
                                const size_t origIdx = filteredIndices[candidateFi];
                                if (!items[origIdx].disabled) {
                                    applySelection(origIdx);
                                    scrollItemIntoView(candidateFi, viewportH, maxScrollY);
                                    break;
                                }
                            }
                        }
                    } else {
                        if (popupState.highlightedFilteredIdx > 0) {
                            for (size_t fi = static_cast<size_t>(popupState.highlightedFilteredIdx); fi > 0; --fi) {
                                const size_t candidateFi = fi - 1;
                                if (!items[filteredIndices[candidateFi]].disabled) {
                                    popupState.highlightedFilteredIdx = static_cast<int>(candidateFi);
                                    scrollItemIntoView(candidateFi, viewportH, maxScrollY);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
        ctx.EndOverlayHitCheck();
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
            if (selectedString != nullptr) {
                currentDisplayText = *selectedString;
            } else if (!editDisplayText.empty()) {
                currentDisplayText = editDisplayText;
            }
        } else {
            currentDisplayText = popupState.filterText;
        }
    } else {
        if (currIndex < items.size()) {
            currentDisplayText = items[currIndex].text;
        } else if (selectedString != nullptr && !selectedString->empty()) {
            currentDisplayText = *selectedString;
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
        const std::string_view measureText = currentDisplayText.text;
        const size_t safeCursor = std::min(cursor, measureText.size());
        const float cursorX = TextUtil::MeasureWidth(measureText.substr(0, safeCursor), inputFont, canvas);
        const float fullTextW = TextUtil::MeasureWidth(measureText, inputFont, canvas);
        const float visibleW = std::max(0.0f, rects.textRect.w - 2.0f);

        if (cursorX < popupState.scrollX) {
            popupState.scrollX = cursorX;
        } else if (cursorX > popupState.scrollX + visibleW) {
            popupState.scrollX = cursorX - visibleW;
        }
        const float maxScroll = std::max(0.0f, fullTextW - visibleW);
        popupState.scrollX = std::clamp(popupState.scrollX, 0.0f, maxScroll);
    } else {
        popupState.scrollX = 0.0f;
    }

    // Register overlay render callback if popup is open
    if (isOpen) {
        const Rect currentPopupRect = ctx.GetOpenPopupRect();
        const float popupScrollY = popupState.scrollY;
        const bool sbHovered = popupState.scrollbarHovered;
        const bool sbActive = popupState.scrollbarActive;
        const int effectiveHoveredIdx = (hoveredItem >= 0)
            ? hoveredItem
            : (isSearchable ? popupState.highlightedFilteredIdx : -1);

        size_t filteredSelectedIndex = size_t(-1);
        std::vector<SelectItem> itemsVec;
        itemsVec.reserve(filteredIndices.size());
        for (size_t fi = 0; fi < filteredIndices.size(); ++fi) {
            const size_t origIdx = filteredIndices[fi];
            if (origIdx == currIndex) {
                filteredSelectedIndex = fi;
            }
            SelectItem copy = items[origIdx];
            copy.text = ctx.AllocText(copy.text);
            copy.prefix = ctx.AllocAffix(copy.prefix);
            copy.suffix = ctx.AllocAffix(copy.suffix);
            itemsVec.push_back(copy);
        }

        const std::string highlightQueryStr(activeQuery);
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
            itemsVec.size(),
            detail::HashStringView(highlightQueryStr)
        ));

        SelectOptions popupOpts = opts;
        popupOpts.emptyText = ctx.AllocText(opts.emptyText);

        ctx.RegisterOverlay([currentPopupRect, itemsVec = std::move(itemsVec), filteredSelectedIndex, effectiveHoveredIdx, popupOpts, activeSize, theme, popupScrollY, sbHovered, sbActive, highlightQueryStr](ICanvas& canvas) {
            Select::RenderPopup(
                canvas,
                currentPopupRect,
                std::span<const SelectItem>(itemsVec),
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

    // When ComboBox popup is open and filterText is empty, show selected item text (or placeholder) as ghost placeholder
    UiText ghostPlaceholder = optsCopy.placeholder;
    if (opts.searchable && !isEditable && isOpen && currentDisplayText.empty() && currIndex < items.size()) {
        ghostPlaceholder = items[currIndex].text;
    }

    optsCopy.placeholder = ctx.AllocText(ghostPlaceholder);
    optsCopy.emptyText = ctx.AllocText(optsCopy.emptyText);
    optsCopy.tooltip = ctx.AllocText(optsCopy.tooltip);
    optsCopy.prefix = ctx.AllocAffix(activePrefix);
    optsCopy.suffix = ctx.AllocAffix(activeSuffix);

    const size_t selStart = std::min(cursor, selAnchor);
    const size_t selEnd = std::max(cursor, selAnchor);
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
    data->showClearBtn = showClearBtn;
    data->clearHovered = clearHovered;
    data->chevronHovered = chevronHovered;

    node->text = data->displayText.empty() ? optsCopy.placeholder.text : data->displayText.text;
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        static_cast<uint8_t>(optsCopy.stretch),
        static_cast<uint8_t>(optsCopy.align),
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
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
        return Select::Measure(d ? d->opts : SelectOptions{}, n->uiSize, canvas, theme, availableWidth);
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

    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);
    ctx.AdaptChildToContainer(node);

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
    size_t cursor,
    bool cursorVisible,
    float cursorWidth,
    size_t selStart,
    size_t selEnd,
    float scrollX,
    bool showClearBtn,
    bool clearHovered,
    bool chevronHovered
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const SelectMetrics& m = theme.GetSelect(size);
    const ControlSpec ctrl = EffectiveSelectControl(opts, size, theme);
    const Rect boxRect = bounds;

    // Determine colors
    const auto& pal = theme.palette;
    Color boxBg = ctrl.HasBackground(true)
        ? ((hovered && !disabled) ? pal.surface : pal.inputBackground)
        : Color::Transparent();
    Color boxBorder;
    float strokeW = (ctrl.border.width > 0.0f) ? theme.Scale(ctrl.border.width) : m.strokeWidth;

    if ((open || focused) && !disabled) {
        boxBorder = pal.focusRing;
        if (ctrl.border.width > 0.0f || m.strokeWidth > 0.0f) strokeW += 0.5f;
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

    if (ctrl.HasBackground(true) && ctrl.background.HasAny() && !ctrl.background.IsSurface()) {
        boxBg = ctrl.background.Resolve(hovered, open || focused, disabled, boxBg);
    }
    if (ctrl.border.color.HasAny()) {
        boxBorder = ctrl.border.color.Resolve(hovered, open || focused, disabled, boxBorder);
    }

    // Draw box background and border
    const float radius = ctrl.border.radius.has_value() ? theme.Scale(*ctrl.border.radius) : m.cornerRadius;
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
        const float clearIconSz = std::max(8.0f, m.iconSize - 2.0f);
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
    const float availW = std::max(0.0f, rightX - leftX);
    const Rect textRect(leftX, boxRect.y, availW, boxRect.h);
    canvas.PushClip(textRect);

    if (isEditing) {
        const std::string_view displayText = currentText.text;
        const float inputFont = theme.ResolveFontSize(currentText, TextRole::LabelMedium, size);
        const Color textCol = currentText.color.HasAny()
            ? theme.ResolveTextColor(currentText, UiVariant::Default, disabled, hovered, open || focused)
            : (disabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary);

        if (!disabled && selStart != selEnd && !displayText.empty()) {
            const size_t s0 = std::min({selStart, selEnd, displayText.size()});
            const size_t s1 = std::min(std::max(selStart, selEnd), displayText.size());
            if (s1 > s0) {
                const float x0 = textRect.x - scrollX + canvas.MeasureText(displayText.substr(0, s0), inputFont).x;
                const float x1 = textRect.x - scrollX + canvas.MeasureText(displayText.substr(0, s1), inputFont).x;
                const float selH = inputFont * 1.25f;
                const float selY = boxRect.y + (boxRect.h - selH) * 0.5f;
                const Rect selRect(x0, selY, std::max(1.0f, x1 - x0), selH);
                canvas.DrawRoundRect(selRect, 2.0f, pal.accent.WithAlpha(88), Color::Transparent(), 0.0f);
            }
        }

        if (displayText.empty() && !opts.placeholder.empty()) {
            const float phFont = !opts.placeholder.size.IsAuto()
                ? theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size)
                : inputFont;
            canvas.DrawTextBox(opts.placeholder.text, textRect, phFont, placeholderCol, 0.0f, 0.5f);
        } else if (!displayText.empty()) {
            const float fullW = std::max(textRect.w, canvas.MeasureText(displayText, inputFont).x + 8.0f);
            const Rect scrolledRect(textRect.x - scrollX, textRect.y, fullW, textRect.h);
            canvas.DrawTextBox(displayText, scrolledRect, inputFont, textCol, 0.0f, 0.5f);
        }

        if (!disabled && cursorVisible) {
            const size_t safeCursor = std::min(cursor, displayText.size());
            const std::string_view sub = displayText.substr(0, safeCursor);
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
        TextUtil::DrawUiText(canvas, currentText, textRect, selFont, boxRect.h, mainCol, 0.0f, 0.5f);
    }

    canvas.PopClip();
}

void Select::RenderPopup(
    ICanvas& canvas,
    Rect popupRect,
    std::span<const std::string_view> items,
    size_t selectedIndex,
    int hoveredIndex,
    const SelectOptions& opts,
    UiSize size,
    const UiTheme& theme,
    float scrollY,
    bool scrollbarHovered,
    bool scrollbarActive,
    std::string_view highlightQuery
) {
    std::vector<SelectItem> richItems;
    richItems.reserve(items.size());
    for (std::string_view sv : items) {
        richItems.push_back(SelectItem{.text = sv});
    }
    RenderPopup(canvas, popupRect, std::span<const SelectItem>(richItems), selectedIndex, hoveredIndex, opts, size, theme, scrollY, scrollbarHovered, scrollbarActive, highlightQuery);
}

void Select::RenderPopup(
    ICanvas& canvas,
    Rect popupRect,
    std::span<const SelectItem> items,
    size_t selectedIndex,
    int hoveredIndex,
    const SelectOptions& opts,
    UiSize size,
    const UiTheme& theme,
    float scrollY,
    bool scrollbarHovered,
    bool scrollbarActive,
    std::string_view highlightQuery
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
        const Rect emptyRect(popupRect.x + m.paddingX, popupRect.y + 4.0f, std::max(0.0f, popupRect.w - m.paddingX * 2.0f), std::max(0.0f, popupRect.h - 8.0f));
        canvas.PushClip(emptyRect);
        const UiText emptyTxt = opts.emptyText.empty() ? UiText{"No matching items"} : opts.emptyText;
        const float emptyFont = theme.ResolveFontSize(emptyTxt, TextRole::LabelMedium, size);
        const Color emptyCol = theme.ResolveTextColor(emptyTxt, UiVariant::Muted, false);
        TextUtil::DrawUiText(canvas, emptyTxt, emptyRect, emptyFont, emptyRect.h, emptyCol, 0.0f, 0.5f);
        canvas.PopClip();
        return;
    }

    const float viewportH = std::max(0.0f, popupRect.h - 8.0f);
    const float totalContentH = static_cast<float>(items.size()) * m.itemHeight;
    const float maxScrollY = std::max(0.0f, totalContentH - viewportH);
    const bool hasScrollbar = (maxScrollY > 0.5f);
    const float clampedScrollY = std::clamp(scrollY, 0.0f, maxScrollY);
    const float itemW = std::max(0.0f, popupRect.w - 8.0f - (hasScrollbar ? sm.gutter : 0.0f));
    const Rect itemsClipRect(popupRect.x + 1.0f, popupRect.y + 4.0f, std::max(0.0f, popupRect.w - 2.0f), viewportH);

    canvas.PushClip(itemsClipRect);

    for (size_t i = 0; i < items.size(); ++i) {
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
        const bool isHovered = (static_cast<int>(i) == hoveredIndex) && !itemDisabled;

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
        const Rect itemTextRect(textLeftX, itemRect.y, std::max(0.0f, textRightX - textLeftX), itemRect.h);

        // Highlight matched substring when search query is active
        if (!highlightQuery.empty() && !item.text.empty()) {
            size_t matchPos = 0;
            size_t matchLen = 0;
            if (FindCaseInsensitiveSubstring(item.text.text, highlightQuery, matchPos, matchLen) && matchLen > 0) {
                const float x0 = textLeftX + canvas.MeasureText(item.text.text.substr(0, matchPos), selFont).x;
                const float x1 = textLeftX + canvas.MeasureText(item.text.text.substr(0, matchPos + matchLen), selFont).x;
                const float hlH = selFont * 1.25f;
                const float hlY = itemRect.y + (itemRect.h - hlH) * 0.5f;
                const Rect hlRect = Rect(x0 - 1.0f, hlY, std::max(2.0f, (x1 - x0) + 2.0f), hlH).Intersect(itemTextRect);
                if (hlRect.w > 0.0f && hlRect.h > 0.0f) {
                    canvas.DrawRoundRect(hlRect, 2.0f, pal.accent.WithAlpha(68), Color::Transparent(), 0.0f);
                }
            }
        }

        TextUtil::DrawUiText(canvas, item.text, itemTextRect, selFont, itemRect.h, itemTextCol, 0.0f, 0.5f);
    }

    canvas.PopClip();

    // Render vertical scrollbar when items exceed visible popup height
    if (hasScrollbar && popupRect.h > 10.0f) {
        canvas.PushClip(popupRect);
        const float trackX = popupRect.Right() - sm.trackWidth - sm.margin;
        const float trackY = popupRect.y + sm.margin;
        const float trackH = std::max(0.0f, popupRect.h - 2.0f * sm.margin);
        const float thumbH = std::clamp((viewportH / std::max(1.0f, totalContentH)) * trackH, sm.minThumbLength, trackH);
        const float travelH = std::max(0.0f, trackH - thumbH);
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

} // namespace guikit
