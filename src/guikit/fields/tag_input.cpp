#include "guikit/fields/tag_input.hpp"
#include <cstdio>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/style/field_frame.hpp"
#include "guikit/style/style_painter.hpp"
#include "keel/core.h"
#include "keel/small_array.h"
#include "keel/string.h"
#include "keel/utf8.h"

namespace guikit {

namespace {

struct TagChipView {
    keel::StringRef text{};
    UiVariant variant{UiVariant::Default};
    Icon icon{};
};

struct TagInputPersistState {
    keel::SmallString<64> editBuffer{};
    keel::f32 scrollOffsetX{0.0f};
    keel::i32 hoveredChipRemoveIdx{-1};
    bool clearHovered{false};
    keel::SmallArray<Rect, 16> chipRemoveRects{};
    Rect clearBtnRect{};
};

struct TagInputNodeData {
    TagInputOptions opts{};
    keel::ArrayRef<TagChipView> chips{};
    keel::StringRef editBuffer{};
    keel::usize cursor{0};
    bool cursorVisible{false};
    keel::f32 cursorWidth{1.5f};
    keel::i32 hoveredChipRemoveIdx{-1};
    bool clearHovered{false};
    keel::f32 scrollOffsetX{0.0f};
    TagInputPersistState* persistState{nullptr};
};

ControlSpec EffectiveTagInputControl(const TagInputOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const InputFieldMetrics& m = theme.GetInputField(size);
    return Control::FromOptions(
        opts,
        SizeMode::Stretch,
        SizeMode::Fit,
        ElementLayout::Stacked,
        m.labelGap,
        m.descGap,
        m.paddingX
    );
}

keel::StringRef TrimWhitespace(keel::StringRef s) noexcept {
    keel::usize start = 0;
    while (start < s.size() && keel::is_ascii_space(s[start])) {
        ++start;
    }
    keel::usize end = s.size();
    while (end > start && keel::is_ascii_space(s[end - 1])) {
        --end;
    }
    return s.substr(start, end - start);
}

bool EqualsIgnoreCase(keel::StringRef a, keel::StringRef b) noexcept {
    keel::usize ia = 0;
    keel::usize ib = 0;
    while (ia < a.size() && ib < b.size()) {
        const keel::u32 ca = keel::utf8_to_lower(keel::utf8_decode_next(a, ia));
        const keel::u32 cb = keel::utf8_to_lower(keel::utf8_decode_next(b, ib));
        if (ca != cb) {
            return false;
        }
    }
    return ia == a.size() && ib == b.size();
}

keel::FixedString<48> FormatCounterText(keel::usize count, keel::u32 maxTags) noexcept {
    char buf[48];
    if (maxTags > 0) {
        std::snprintf(buf, sizeof(buf), "%zu/%u", count, maxTags);
    } else {
        std::snprintf(buf, sizeof(buf), "%zu", count);
    }
    return keel::FixedString<48>(buf);
}

keel::f32 MeasureChipWidth(
    const TagChipView& chip,
    keel::f32 chipFont,
    keel::f32 iconSz,
    bool showRemove,
    ICanvas* canvas
) noexcept {
    const keel::f32 textW = TextUtil::MeasureWidth(chip.text, chipFont, canvas);
    keel::f32 w = 6.0f + textW;
    if (chip.icon.IsValid()) {
        w += iconSz + 4.0f;
    }
    if (showRemove) {
        const keel::f32 removeBtnSz = keel::max(12.0f, iconSz + 2.0f);
        w += 4.0f + removeBtnSz + 4.0f;
    } else {
        w += 6.0f;
    }
    return w;
}

struct TagLayoutResult {
    Rect innerRect{};
    Rect clearBtnRect{};
    Rect counterRect{};
    keel::FixedString<48> counterStr{};
    keel::SmallArray<Rect, 16> chipRects{};
    keel::SmallArray<Rect, 16> chipRemoveRects{};
    keel::usize visibleChipCount{0};
    keel::usize overflowCount{0};
    Rect overflowRect{};
    keel::f32 inlineTextX{0.0f};
    keel::f32 resolvedScrollX{0.0f};
};

TagLayoutResult ComputeTagLayout(
    Rect boxRect,
    keel::ArrayRef<TagChipView> chips,
    keel::StringRef editBuffer,
    keel::usize cursor,
    const TagInputOptions& opts,
    UiSize size,
    bool focused,
    bool disabled,
    keel::f32 currentScrollX,
    ICanvas* canvas,
    const UiTheme& theme
) {
    TagLayoutResult out{};
    const InputFieldMetrics& m = theme.GetInputField(size);
    const keel::f32 chipFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
    const keel::f32 inputFont = theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size);
    const keel::f32 chipIconSz = keel::max(10.0f, m.iconSize - 2.0f);
    const keel::f32 removeBtnSz = keel::max(12.0f, chipIconSz + 2.0f);
    const bool showRemove = !opts.readOnly && !disabled;

    keel::f32 leftX = boxRect.x + m.paddingX;
    if (opts.prefix.HasContentOrReserve()) {
        const keel::f32 pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const keel::f32 pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, canvas);
        const keel::f32 pGap = (opts.prefix.HasIcon() && !opts.prefix.HasText()) ? 6.0f : 4.0f;
        leftX += pW + pGap;
    }

    keel::f32 rightX = boxRect.x + boxRect.w - m.paddingX;

    const bool showClear = opts.clearable && !chips.empty() && !opts.readOnly && !disabled;
    if (showClear) {
        const keel::f32 btnSize = m.iconSize + 4.0f;
        rightX -= btnSize;
        out.clearBtnRect = Rect(rightX, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        rightX -= 4.0f;
    }

    const bool showCounter = opts.showCounter || opts.maxTags > 0;
    if (showCounter) {
        out.counterStr = FormatCounterText(chips.size(), opts.maxTags);
        const keel::f32 counterTextW = TextUtil::MeasureWidth(out.counterStr, chipFont, canvas);
        const keel::f32 badgeW = counterTextW + 10.0f;
        const keel::f32 badgeH = keel::max(16.0f, keel::min(boxRect.h - 8.0f, chipFont + 6.0f));
        rightX -= badgeW;
        out.counterRect = Rect(rightX, boxRect.y + (boxRect.h - badgeH) * 0.5f, badgeW, badgeH);
        rightX -= 6.0f;
    }

    const keel::f32 availW = keel::max(0.0f, rightX - leftX);
    out.innerRect = Rect(leftX, boxRect.y, availW, boxRect.h);

    const keel::f32 chipH = keel::max(16.0f, boxRect.h - 8.0f);
    const keel::f32 chipY = boxRect.y + (boxRect.h - chipH) * 0.5f;
    const keel::f32 chipGap = 4.0f;

    const keel::usize nChips = chips.size();
    out.chipRects.assign(nChips, Rect{});
    out.chipRemoveRects.assign(nChips, Rect{});

    keel::SmallArray<keel::f32, 16> chipWidths(nChips, 0.0f);
    keel::f32 totalChipsW = 0.0f;
    for (keel::usize i = 0; i < nChips; ++i) {
        chipWidths[i] = MeasureChipWidth(chips[i], chipFont, chipIconSz, showRemove, canvas);
        totalChipsW += chipWidths[i];
        if (i + 1 < nChips) {
            totalChipsW += chipGap;
        }
    }

    if (focused || !editBuffer.empty()) {
        out.visibleChipCount = chips.size();
        out.overflowCount = 0;

        keel::f32 relX = 0.0f;
        for (keel::usize i = 0; i < nChips; ++i) {
            relX += chipWidths[i] + chipGap;
        }
        const keel::f32 textStartRelX = relX;
        const keel::usize safeCursor = keel::min(cursor, editBuffer.size());
        const keel::f32 cursorOffset = TextUtil::MeasureWidth(editBuffer.substr(0, safeCursor), inputFont, canvas);
        const keel::f32 fullEditW = keel::max(
            TextUtil::MeasureWidth(editBuffer, inputFont, canvas) + 12.0f,
            (chips.empty() && editBuffer.empty()) ? TextUtil::MeasureWidth(opts.placeholder.text, inputFont, canvas) : 28.0f
        );
        const keel::f32 totalContentW = textStartRelX + fullEditW;
        const keel::f32 cursorRelX = textStartRelX + cursorOffset;

        keel::f32 scrollX = focused ? currentScrollX : 0.0f;
        if (focused && availW > 0.0f) {
            const keel::f32 visibleW = keel::max(0.0f, availW - 4.0f);
            if (cursorRelX + 4.0f > scrollX + visibleW) {
                scrollX = (cursorRelX + 4.0f) - visibleW;
            } else if (cursorRelX < scrollX) {
                scrollX = keel::max(0.0f, cursorRelX - 16.0f);
            }
            const keel::f32 maxScroll = keel::max(0.0f, totalContentW - visibleW);
            scrollX = keel::clamp(scrollX, 0.0f, maxScroll);
        } else {
            scrollX = 0.0f;
        }
        out.resolvedScrollX = scrollX;

        keel::f32 curX = out.innerRect.x - scrollX;
        for (keel::usize i = 0; i < nChips; ++i) {
            const keel::f32 cw = chipWidths[i];
            const Rect cRect(curX, chipY, cw, chipH);
            out.chipRects[i] = cRect;
            if (showRemove) {
                out.chipRemoveRects[i] = Rect(
                    cRect.Right() - 4.0f - removeBtnSz,
                    cRect.y + (cRect.h - removeBtnSz) * 0.5f,
                    removeBtnSz,
                    removeBtnSz
                );
            }
            curX += cw + chipGap;
        }
        out.inlineTextX = curX;
    } else {
        out.resolvedScrollX = 0.0f;
        if (totalChipsW <= availW || chips.empty()) {
            out.visibleChipCount = chips.size();
            out.overflowCount = 0;
            keel::f32 curX = out.innerRect.x;
            for (keel::usize i = 0; i < nChips; ++i) {
                const keel::f32 cw = chipWidths[i];
                const Rect cRect(curX, chipY, cw, chipH);
                out.chipRects[i] = cRect;
                if (showRemove) {
                    out.chipRemoveRects[i] = Rect(
                        cRect.Right() - 4.0f - removeBtnSz,
                        cRect.y + (cRect.h - removeBtnSz) * 0.5f,
                        removeBtnSz,
                        removeBtnSz
                    );
                }
                curX += cw + chipGap;
            }
            out.inlineTextX = curX;
        } else {
            keel::usize fitCount = 0;
            keel::f32 usedW = 0.0f;
            for (keel::usize i = 0; i < chips.size(); ++i) {
                const keel::usize remaining = chips.size() - (i + 1);
                char ovBuf[32];
                std::snprintf(ovBuf, sizeof(ovBuf), "+%zu", remaining);
                const keel::f32 ovBadgeW = (remaining > 0)
                    ? (TextUtil::MeasureWidth(ovBuf, chipFont, canvas) + 12.0f + chipGap)
                    : 0.0f;
                const keel::f32 nextW = usedW + (i > 0 ? chipGap : 0.0f) + chipWidths[i];
                if (nextW + ovBadgeW <= availW) {
                    usedW = nextW;
                    fitCount = i + 1;
                } else {
                    break;
                }
            }
            out.visibleChipCount = fitCount;
            out.overflowCount = chips.size() - fitCount;

            keel::f32 curX = out.innerRect.x;
            for (keel::usize i = 0; i < fitCount; ++i) {
                const keel::f32 cw = chipWidths[i];
                const Rect cRect(curX, chipY, cw, chipH);
                out.chipRects[i] = cRect;
                if (showRemove) {
                    out.chipRemoveRects[i] = Rect(
                        cRect.Right() - 4.0f - removeBtnSz,
                        cRect.y + (cRect.h - removeBtnSz) * 0.5f,
                        removeBtnSz,
                        removeBtnSz
                    );
                }
                curX += cw + chipGap;
            }
            if (out.overflowCount > 0) {
                char ovBuf[32];
                std::snprintf(ovBuf, sizeof(ovBuf), "+%zu", out.overflowCount);
                const keel::f32 ovW = TextUtil::MeasureWidth(ovBuf, chipFont, canvas) + 12.0f;
                out.overflowRect = Rect(curX, chipY, ovW, chipH);
                curX += ovW + chipGap;
            }
            out.inlineTextX = curX;
        }
    }

    return out;
}

Color ResolveChipAccentColor(
    UiVariant chipVariant,
    UiVariant defaultVariant,
    UiVariant statusOverride,
    const UiTheme& theme
) noexcept {
    const UiVariant effectiveVar = (chipVariant != UiVariant::Default)
        ? chipVariant
        : ((statusOverride != UiVariant::Default) ? statusOverride : defaultVariant);
    switch (effectiveVar) {
        case UiVariant::Default:
        case UiVariant::Secondary:
            return theme.palette.textSecondary;
        case UiVariant::Ghost:
        case UiVariant::Muted:
            return theme.palette.textMuted;
        default:
            return theme.GetVariant(effectiveVar).bg;
    }
}

template <typename SinkT, typename GetChipFn, typename PushTokenFn>
Response RunTagInputImpl(
    Context& ctx,
    SinkT tags,
    const TagInputOptions& opts,
    keel::SourceLocation loc,
    GetChipFn&& getChip,
    PushTokenFn&& pushToken
) {
    const Id id = ctx.MakeChildId(loc);

    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();

    ctx.RegisterFocusable(id, effectiveDisabled || opts.readOnly);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    }

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    TagInputOptions optsCopy = opts;
    if (optsCopy.width.IsAuto()) {
        optsCopy.width = Control::ResolveWidthMode(ctx, opts.width, SizeMode::Stretch);
    }
    optsCopy.placeholder = ctx.AllocText(opts.placeholder);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    optsCopy.prefix = ctx.AllocAffix(opts.prefix);

    auto& st = ctx.GetOrCreatePersistentState<TagInputPersistState>(id);
    const UiTheme& theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();

    auto buildTempViews = [&]() {
        keel::SmallArray<TagChipView, 16> views;
        const keel::usize n = tags.size();
        views.reserve(n);
        for (keel::usize i = 0; i < n; ++i) {
            views.push_back(getChip(i, optsCopy.tagVariant));
        }
        return views;
    };

    const Rect prevBounds = (res.rect.w > 0.0f && res.rect.h > 0.0f)
        ? res.rect
        : Rect({0.0f, 0.0f}, detail::TagInput::Measure({}, optsCopy, activeSize, canvas, theme));

    if (st.chipRemoveRects.size() != tags.size()) {
        auto tempViews = buildTempViews();
        auto initLayout = ComputeTagLayout(
            prevBounds,
            tempViews,
            st.editBuffer.as_ref(),
            st.editBuffer.size(),
            optsCopy,
            activeSize,
            ctx.IsFocused(id) && !effectiveDisabled,
            effectiveDisabled,
            st.scrollOffsetX,
            canvas,
            theme
        );
        st.chipRemoveRects = keel::move(initLayout.chipRemoveRects);
        st.clearBtnRect = initLayout.clearBtnRect;
    }

    const Vec2 mousePos = ctx.GetMousePos();
    st.hoveredChipRemoveIdx = -1;
    st.clearHovered = false;

    if (res.hovered && !optsCopy.readOnly && !effectiveDisabled) {
        if (optsCopy.clearable && !tags.empty() && st.clearBtnRect.w > 0.0f && st.clearBtnRect.Contains(mousePos)) {
            st.clearHovered = true;
        } else {
            for (keel::usize i = 0; i < st.chipRemoveRects.size() && i < tags.size(); ++i) {
                if (st.chipRemoveRects[i].w > 0.0f &&
                    st.chipRemoveRects[i].Contains(mousePos)) {
                    st.hoveredChipRemoveIdx = keel::as<keel::i32>(i);
                    break;
                }
            }
        }
    }

    const bool mouseJustPressed = res.hovered && ctx.IsMouseClicked() && !effectiveDisabled;
    if ((mouseJustPressed || res.clicked) && !effectiveDisabled) {
        if (!optsCopy.readOnly) {
            if (mouseJustPressed && st.hoveredChipRemoveIdx >= 0 &&
                keel::as<keel::usize>(st.hoveredChipRemoveIdx) < tags.size()) {
                tags.erase_at(keel::as<keel::usize>(st.hoveredChipRemoveIdx));
                st.hoveredChipRemoveIdx = -1;
                res.changed = true;
                ctx.RequestFocus(id);
                (void)ctx.EnsureTextEdit(id, st.editBuffer.as_ref(), /*multiline=*/false, /*readOnly=*/false);
            } else if (mouseJustPressed && st.clearHovered && optsCopy.clearable && !tags.empty()) {
                tags.clear();
                st.editBuffer.clear();
                st.scrollOffsetX = 0.0f;
                st.clearHovered = false;
                st.hoveredChipRemoveIdx = -1;
                ctx.SetTextEditBuffer(id, "");
                res.changed = true;
                ctx.RequestFocus(id);
                (void)ctx.EnsureTextEdit(id, st.editBuffer.as_ref(), /*multiline=*/false, /*readOnly=*/false);
            } else {
                ctx.RequestFocus(id);
                (void)ctx.EnsureTextEdit(id, st.editBuffer.as_ref(), /*multiline=*/false, /*readOnly=*/false);
            }
        } else {
            ctx.RequestFocus(id);
        }
    }

    bool isFocused = ctx.IsFocused(id) && !effectiveDisabled;
    res.focused = isFocused;

    keel::usize cursor = st.editBuffer.size();
    keel::usize selAnchor = cursor;

    auto tryCommitToken = [&](keel::StringRef raw) -> bool {
        const keel::StringRef token = TrimWhitespace(raw);
        if (token.empty()) {
            return false;
        }
        if (optsCopy.maxTags > 0 && tags.size() >= optsCopy.maxTags) {
            return false;
        }
        if (!optsCopy.allowDuplicates) {
            const keel::usize existingCount = tags.size();
            for (keel::usize i = 0; i < existingCount; ++i) {
                if (EqualsIgnoreCase(getChip(i, optsCopy.tagVariant).text, token)) {
                    return false;
                }
            }
        }
        const keel::usize beforeCount = tags.size();
        pushToken(token);
        return tags.size() > beforeCount;
    };

    if ((isFocused || ctx.HasActiveTextEdit(id)) && !optsCopy.readOnly && !effectiveDisabled) {
        bool textChanged = false;
        bool textSubmitted = false;
        const keel::StringRef liveBuf = ctx.GetOrStartTextEdit(
            id,
            st.editBuffer.as_ref(),
            cursor,
            selAnchor,
            textChanged,
            textSubmitted,
            /*multiline=*/false,
            /*readOnly=*/false
        );
        st.editBuffer.assign(liveBuf);

        if (optsCopy.splitOnComma || optsCopy.splitOnSpace) {
            auto isDelimiter = [&](char ch) noexcept {
                if (optsCopy.splitOnComma && (ch == ',' || ch == ';')) return true;
                if (optsCopy.splitOnSpace && ch == ' ') return true;
                return false;
            };

            keel::usize lastDelimPos = keel::StringRef::npos;
            for (keel::usize i = 0; i < st.editBuffer.size(); ++i) {
                if (isDelimiter(st.editBuffer[i])) {
                    lastDelimPos = i;
                }
            }

            if (lastDelimPos != keel::StringRef::npos) {
                keel::usize tokenStart = 0;
                bool anyAdded = false;
                for (keel::usize i = 0; i <= lastDelimPos; ++i) {
                    if (isDelimiter(st.editBuffer[i])) {
                        const keel::StringRef seg(st.editBuffer.data() + tokenStart, i - tokenStart);
                        if (tryCommitToken(seg)) {
                            anyAdded = true;
                        }
                        tokenStart = i + 1;
                    }
                }
                keel::SmallString<64> tail(TrimWhitespace(
                    st.editBuffer.as_ref().substr(lastDelimPos + 1)
                ));
                st.editBuffer = keel::move(tail);
                ctx.SetTextEditBuffer(id, st.editBuffer.as_ref());
                cursor = st.editBuffer.size();
                selAnchor = cursor;
                ctx.SetTextEditCursor(id, cursor, selAnchor);
                if (anyAdded) {
                    res.changed = true;
                }
            }
        }

        if (textSubmitted || ctx.ConsumeKey(Key::Enter)) {
            if (tryCommitToken(st.editBuffer.as_ref())) {
                res.changed = true;
                res.submitted = true;
            }
            st.editBuffer.clear();
            ctx.SetTextEditBuffer(id, "");
            ctx.RequestFocus(id);
            isFocused = true;
            res.focused = true;
            (void)ctx.EnsureTextEdit(id, "", /*multiline=*/false, /*readOnly=*/false);
            cursor = 0;
            selAnchor = 0;
            ctx.SetTextEditCursor(id, 0, 0);
        }

        if (st.editBuffer.empty() && !textChanged && ctx.ConsumeKey(Key::Backspace)) {
            if (!tags.empty()) {
                tags.erase_at(tags.size() - 1);
                res.changed = true;
            }
        }
    }

    const keel::usize chipCount = tags.size();
    TagChipView* arenaChips = nullptr;
    if (chipCount > 0) {
        arenaChips = ctx.GetArena().alloc_array<TagChipView>(chipCount);
        for (keel::usize i = 0; i < chipCount; ++i) {
            TagChipView cv = getChip(i, optsCopy.tagVariant);
            if (!cv.icon.GetPath().empty()) {
                cv.icon = Icon::File(ctx.AllocString(cv.icon.GetPath()));
            }
            cv.text = ctx.AllocString(cv.text);
            arenaChips[i] = cv;
        }
    }

    const keel::ArrayRef<TagChipView> chipSpan(arenaChips, chipCount);
    TagLayoutResult layoutNow = ComputeTagLayout(
        prevBounds,
        chipSpan,
        st.editBuffer.as_ref(),
        cursor,
        optsCopy,
        activeSize,
        isFocused,
        effectiveDisabled,
        st.scrollOffsetX,
        canvas,
        theme
    );
    st.scrollOffsetX = layoutNow.resolvedScrollX;
    st.chipRemoveRects = keel::move(layoutNow.chipRemoveRects);
    st.clearBtnRect = layoutNow.clearBtnRect;

    const ControlSpec ctrl = EffectiveTagInputControl(optsCopy, activeSize, theme);
    Node* node = ctx.CreateNode(NodeType::TagInput, id);

    auto* data = ctx.AllocPayload<TagInputNodeData>();
    data->opts = optsCopy;
    data->chips = chipSpan;
    data->editBuffer = ctx.AllocString(st.editBuffer.as_ref());
    data->cursor = cursor;
    data->cursorVisible = isFocused && !optsCopy.readOnly && ctx.IsCursorVisible();
    data->cursorWidth = ctx.GetCursorConfig().width;
    data->hoveredChipRemoveIdx = st.hoveredChipRemoveIdx;
    data->clearHovered = st.clearHovered;
    data->scrollOffsetX = st.scrollOffsetX;
    data->persistState = &st;
    node->SetPayload(data);

    keel::u64 chipsHash = detail::HashArgs(chipCount);
    for (keel::usize i = 0; i < chipCount; ++i) {
        chipsHash = detail::HashArgs(
            chipsHash,
            chipSpan[i].text,
            keel::as<keel::u8>(chipSpan[i].variant),
            keel::as<keel::u16>(chipSpan[i].icon.GetId()),
            chipSpan[i].icon.GetPath()
        );
    }

    node->visualHash = detail::HashArgs(
        chipsHash,
        keel::as<keel::u8>(optsCopy.tagVariant),
        optsCopy.maxTags,
        optsCopy.clearable,
        optsCopy.showCounter,
        optsCopy.readOnly,
        keel::as<keel::u8>(optsCopy.style),
        keel::as<keel::u8>(optsCopy.status),
        optsCopy.background,
        optsCopy.border,
        optsCopy.width,
        optsCopy.height,
        optsCopy.minWidth.value_or(0.0f),
        optsCopy.maxWidth.value_or(0.0f),
        optsCopy.minHeight.value_or(0.0f),
        optsCopy.maxHeight.value_or(0.0f),
        optsCopy.placeholder,
        optsCopy.prefix,
        data->editBuffer,
        data->cursor,
        data->cursorVisible,
        data->hoveredChipRemoveIdx,
        data->clearHovered,
        data->scrollOffsetX
    );

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* c, const UiTheme& t) noexcept -> Vec2 {
        const auto* d = n->As<TagInputNodeData>();
        if (!d) {
            return detail::TagInput::Measure({}, TagInputOptions{}, n->uiSize, c, t, availableWidth);
        }
        const InputFieldMetrics& m = t.GetInputField(n->uiSize);
        const ControlSpec spec = EffectiveTagInputControl(d->opts, n->uiSize, t);
        keel::f32 boxW = m.defaultWidth;
        if (spec.width.IsFixed()) {
            boxW = spec.width.Dp();
        } else if (d->opts.width.ResolveMode(SizeMode::Stretch) != SizeMode::Stretch) {
            const keel::f32 chipFont = t.ResolveFontSize(TextRole::LabelSmall, n->uiSize);
            const keel::f32 chipIconSz = keel::max(10.0f, m.iconSize - 2.0f);
            const bool showRemove = !d->opts.readOnly && !n->disabled;
            keel::f32 contentW = m.paddingX * 2.0f;
            for (keel::usize i = 0; i < d->chips.size(); ++i) {
                contentW += MeasureChipWidth(d->chips[i], chipFont, chipIconSz, showRemove, c) + 4.0f;
            }
            if (!d->opts.readOnly) {
                const keel::f32 inputFont = t.ResolveFontSize(d->opts.placeholder, TextRole::LabelMedium, n->uiSize);
                const keel::f32 phW = d->chips.empty()
                    ? TextUtil::MeasureWidth(d->opts.placeholder.text, inputFont, c)
                    : 56.0f;
                contentW += keel::max(48.0f, phW + 8.0f);
            }
            if (d->opts.prefix.HasContentOrReserve()) {
                const keel::f32 pFont = t.ResolveFontSize(d->opts.prefix.text, TextRole::LabelMedium, n->uiSize);
                contentW += TextUtil::MeasureAffixWidth(d->opts.prefix, pFont, m.iconSize, 4.0f, c) + 6.0f;
            }
            if (d->opts.clearable && !d->opts.readOnly) {
                contentW += m.iconSize + 8.0f;
            }
            if (d->opts.showCounter || d->opts.maxTags > 0) {
                contentW += 36.0f;
            }
            boxW = keel::max(m.defaultWidth, contentW);
        }
        if (spec.minWidth.has_value()) {
            boxW = keel::max(*spec.minWidth, boxW);
        } else {
            boxW = keel::max(m.minWidth, boxW);
        }
        return Control::Measure({boxW, m.defaultHeight}, spec, t);
    };

    node->renderFn = &detail::TagInput::Render;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->focused = isFocused;
    node->pressed = res.pressed;

    Control::SetupNode(ctx, node, ctrl);

    return res;
}

} // namespace

Response detail::TagInput::Run(
    Context& ctx,
    StringTagListSink tags,
    const TagInputOptions& opts,
    keel::SourceLocation loc
) {
    return RunTagInputImpl(
        ctx,
        tags,
        opts,
        loc,
        [tags](keel::usize idx, UiVariant defaultVariant) noexcept -> TagChipView {
            return TagChipView{
                .text = tags.get(idx),
                .variant = defaultVariant,
                .icon = {}
            };
        },
        [tags](keel::StringRef tok) {
            tags.push_back(tok);
        }
    );
}

Response detail::TagInput::Run(
    Context& ctx,
    TagItemListSink tags,
    const TagInputOptions& opts,
    keel::SourceLocation loc
) {
    return RunTagInputImpl(
        ctx,
        tags,
        opts,
        loc,
        [tags](keel::usize idx, UiVariant defaultVariant) noexcept -> TagChipView {
            const TagItem& item = tags[idx];
            return TagChipView{
                .text = item.text.as_ref(),
                .variant = (item.variant != UiVariant::Default) ? item.variant : defaultVariant,
                .icon = item.icon
            };
        },
        [tags, &opts](keel::StringRef tok) {
            tags.push_back(TagItem{
                .text = keel::FixedString<64>(tok),
                .variant = opts.tagVariant,
                .icon = {}
            });
        }
    );
}

Vec2 detail::TagInput::Measure(
    keel::ArrayRef<keel::StringRef> tags,
    const TagInputOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 /*availableWidth*/
) {
    const InputFieldMetrics& m = theme.GetInputField(size);
    const ControlSpec ctrl = EffectiveTagInputControl(opts, size, theme);

    keel::f32 boxW = m.defaultWidth;
    if (ctrl.width.IsFixed()) {
        boxW = ctrl.width.Dp();
    } else if (opts.width.ResolveMode(SizeMode::Stretch) != SizeMode::Stretch) {
        const keel::f32 chipFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
        const keel::f32 chipIconSz = keel::max(10.0f, m.iconSize - 2.0f);
        const bool showRemove = !opts.readOnly && !opts.disabled;
        keel::f32 contentW = m.paddingX * 2.0f;
        for (keel::StringRef tag : tags) {
            const TagChipView v{.text = tag, .variant = opts.tagVariant};
            contentW += MeasureChipWidth(v, chipFont, chipIconSz, showRemove, canvas) + 4.0f;
        }
        if (!opts.readOnly) {
            const keel::f32 inputFont = theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size);
            const keel::f32 phW = tags.empty()
                ? TextUtil::MeasureWidth(opts.placeholder.text, inputFont, canvas)
                : 56.0f;
            contentW += keel::max(48.0f, phW + 8.0f);
        }
        if (opts.prefix.HasContentOrReserve()) {
            const keel::f32 pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
            contentW += TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, canvas) + 6.0f;
        }
        if (opts.clearable && !opts.readOnly) {
            contentW += m.iconSize + 8.0f;
        }
        if (opts.showCounter || opts.maxTags > 0) {
            contentW += 36.0f;
        }
        boxW = keel::max(m.defaultWidth, contentW);
    }

    if (ctrl.minWidth.has_value()) {
        boxW = keel::max(*ctrl.minWidth, boxW);
    } else {
        boxW = keel::max(m.minWidth, boxW);
    }

    return Control::Measure({boxW, m.defaultHeight}, ctrl, theme);
}

void detail::TagInput::Render(const Node* node, ICanvas& canvas, const UiTheme& theme) {
    if (!node) return;
    const Rect boxRect = node->computedRect;
    if (boxRect.w <= 0.0f || boxRect.h <= 0.0f) return;

    const auto* data = node->As<TagInputNodeData>();
    const TagInputOptions defaultOpts{};
    const TagInputOptions& opts = data ? data->opts : defaultOpts;
    const UiSize size = node->uiSize;
    const bool hovered = node->hovered;
    const bool focused = node->focused;
    const bool disabled = node->disabled;

    const InputFieldMetrics& m = theme.GetInputField(size);
    const auto& pal = theme.palette;

    Color textCol = disabled ? pal.textPrimary.WithAlphaF(theme.disabledOpacity) : pal.textPrimary;
    Color placeholderCol = theme.ResolveTextColor(opts.placeholder, UiVariant::Secondary, disabled, hovered, focused);
    Color unitCol = disabled ? pal.textSecondary.WithAlphaF(theme.disabledOpacity) : pal.textSecondary;

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
        .disabled = disabled
    });

    if (opts.prefix.HasContentOrReserve()) {
        const keel::f32 pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const Color pTextCol = theme.ResolveTextColor(opts.prefix.text, UiVariant::Secondary, disabled, hovered, focused);
        const Color pIconCol = opts.prefix.text.color.HasAny() ? pTextCol : unitCol;
        const keel::f32 pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, &canvas);
        TextUtil::DrawAffix(
            canvas,
            opts.prefix,
            Rect(boxRect.x + m.paddingX, boxRect.y, pW, boxRect.h),
            pFont,
            m.iconSize,
            pTextCol,
            pIconCol,
            4.0f
        );
    }

    const keel::ArrayRef<TagChipView> chips = data ? data->chips : keel::ArrayRef<TagChipView>{};
    const keel::StringRef editBuf = data ? data->editBuffer : keel::StringRef{};
    const keel::usize cursor = data ? data->cursor : 0;
    const keel::f32 scrollIn = data ? data->scrollOffsetX : 0.0f;

    const TagLayoutResult layout = ComputeTagLayout(
        boxRect,
        chips,
        editBuf,
        cursor,
        opts,
        size,
        focused,
        disabled,
        scrollIn,
        &canvas,
        theme
    );

    if (data && data->persistState) {
        data->persistState->scrollOffsetX = layout.resolvedScrollX;
        data->persistState->chipRemoveRects = layout.chipRemoveRects.clone();
        data->persistState->clearBtnRect = layout.clearBtnRect;
    }

    const keel::f32 chipFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
    const keel::f32 inputFont = theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size);
    const keel::f32 chipIconSz = keel::max(10.0f, m.iconSize - 2.0f);

    if (layout.counterRect.w > 0.0f) {
        const bool atLimit = (opts.maxTags > 0 && chips.size() >= opts.maxTags);
        const Color badgeAccent = atLimit ? theme.GetVariant(UiVariant::Warning).bg : pal.textSecondary;
        const Color badgeBg = atLimit ? badgeAccent.WithAlphaF(0.18f) : pal.surfaceHover.WithAlphaF(disabled ? theme.disabledOpacity : 0.85f);
        const Color badgeBorder = atLimit ? badgeAccent.WithAlphaF(0.45f) : pal.border.WithAlphaF(disabled ? 0.25f : 0.50f);
        const Color badgeText = atLimit ? badgeAccent : unitCol;
        canvas.DrawRoundRect(layout.counterRect, 4.0f, badgeBg, badgeBorder, 1.0f);
        canvas.DrawTextBox(layout.counterStr, layout.counterRect, chipFont, badgeText, 0.5f, 0.5f);
    }

    if (layout.clearBtnRect.w > 0.0f) {
        const bool clearHovered = data && data->clearHovered && !disabled;
        if (clearHovered) {
            canvas.DrawRoundRect(layout.clearBtnRect, 4.0f, pal.surfaceHover, Color::Transparent(), 0.0f);
        }
        const keel::f32 clearIconSz = keel::max(8.0f, m.iconSize - 2.0f);
        const Rect icRect(
            layout.clearBtnRect.x + (layout.clearBtnRect.w - clearIconSz) * 0.5f,
            layout.clearBtnRect.y + (layout.clearBtnRect.h - clearIconSz) * 0.5f,
            clearIconSz,
            clearIconSz
        );
        const Color icCol = clearHovered ? pal.textPrimary : unitCol;
        Icon::Close().Draw(canvas, icRect, icCol);
    }

    if (layout.innerRect.w <= 0.0f) {
        return;
    }

    canvas.PushClip(layout.innerRect);

    const bool showRemove = !opts.readOnly && !disabled;
    const keel::i32 hoveredRemoveIdx = data ? data->hoveredChipRemoveIdx : -1;

    for (keel::usize i = 0; i < layout.visibleChipCount && i < chips.size(); ++i) {
        const Rect cRect = layout.chipRects[i];
        if (cRect.Right() < layout.innerRect.x || cRect.x > layout.innerRect.Right()) {
            continue;
        }

        const TagChipView& chip = chips[i];
        const UiVariant effectiveVar = (chip.variant != UiVariant::Default) ? chip.variant : opts.tagVariant;
        const Color accent = ResolveChipAccentColor(
            chip.variant,
            opts.tagVariant,
            opts.status,
            theme
        );

        const Color chipBg = accent.WithAlphaF(disabled ? 0.08f : 0.16f);
        const Color chipBorder = accent.WithAlphaF(disabled ? 0.20f : 0.42f);
        Color chipTextCol = (effectiveVar == UiVariant::Default || effectiveVar == UiVariant::Secondary)
            ? pal.textPrimary
            : ((effectiveVar == UiVariant::Ghost || effectiveVar == UiVariant::Muted) ? pal.textSecondary : accent);
        if (disabled) {
            chipTextCol = chipTextCol.WithAlphaF(theme.disabledOpacity);
        }

        canvas.DrawRoundRect(cRect, 4.0f, chipBg, chipBorder, 1.0f);

        keel::f32 cx = cRect.x + 6.0f;
        if (chip.icon.IsValid()) {
            const Rect icRect(cx, cRect.y + (cRect.h - chipIconSz) * 0.5f, chipIconSz, chipIconSz);
            chip.icon.Draw(canvas, icRect, chipTextCol);
            cx += chipIconSz + 4.0f;
        }

        const keel::f32 textRight = showRemove ? layout.chipRemoveRects[i].x - 2.0f : cRect.Right() - 6.0f;
        const Rect chipTextRect(cx, cRect.y, keel::max(0.0f, textRight - cx), cRect.h);
        canvas.DrawTextBox(chip.text, chipTextRect, chipFont, chipTextCol, 0.0f, 0.5f);

        if (showRemove) {
            const Rect remRect = layout.chipRemoveRects[i];
            const bool remHovered = (hoveredRemoveIdx == keel::as<keel::i32>(i));
            if (remHovered) {
                canvas.DrawRoundRect(remRect, 3.0f, accent.WithAlphaF(0.28f), Color::Transparent(), 0.0f);
            }
            const keel::f32 xIconSz = keel::max(7.0f, chipIconSz - 2.0f);
            const Rect xRect(
                remRect.x + (remRect.w - xIconSz) * 0.5f,
                remRect.y + (remRect.h - xIconSz) * 0.5f,
                xIconSz,
                xIconSz
            );
            const Color xCol = remHovered ? pal.textPrimary : chipTextCol.WithAlphaF(0.80f);
            Icon::Close().Draw(canvas, xRect, xCol);
        }
    }

    if (!focused && layout.overflowCount > 0 && layout.overflowRect.w > 0.0f) {
        char ovBuf[32];
        std::snprintf(ovBuf, sizeof(ovBuf), "+%zu", layout.overflowCount);
        const Color ovBg = pal.surfaceHover.WithAlphaF(disabled ? 0.40f : 0.90f);
        const Color ovBorder = pal.border.WithAlphaF(disabled ? 0.25f : 0.55f);
        canvas.DrawRoundRect(layout.overflowRect, 4.0f, ovBg, ovBorder, 1.0f);
        canvas.DrawTextBox(ovBuf, layout.overflowRect, chipFont, unitCol, 0.5f, 0.5f);
    }

    if (!opts.readOnly) {
        const keel::f32 textAvailW = keel::max(64.0f, layout.innerRect.Right() - layout.inlineTextX);
        const Rect inlineRect(layout.inlineTextX, boxRect.y, textAvailW, boxRect.h);

        if (!editBuf.empty()) {
            const keel::f32 drawW = keel::max(inlineRect.w, canvas.MeasureText(editBuf, inputFont).x + 8.0f);
            canvas.DrawTextBox(editBuf, Rect(inlineRect.x, inlineRect.y, drawW, inlineRect.h), inputFont, textCol, 0.0f, 0.5f);
        } else if (chips.empty() && !opts.placeholder.empty()) {
            canvas.DrawTextBox(opts.placeholder.text, inlineRect, inputFont, placeholderCol, 0.0f, 0.5f);
        }

        if (focused && !disabled && data && data->cursorVisible) {
            const keel::usize safeCursor = keel::min(cursor, editBuf.size());
            const keel::f32 subW = canvas.MeasureText(editBuf.substr(0, safeCursor), inputFont).x;
            const keel::f32 caretX = inlineRect.x + subW;
            const keel::f32 caretH = inputFont * 1.15f;
            const keel::f32 caretY = boxRect.y + (boxRect.h - caretH) * 0.5f;
            canvas.DrawLine({caretX, caretY}, {caretX, caretY + caretH}, pal.caret, data->cursorWidth);
        }
    } else if (chips.empty() && !opts.placeholder.empty()) {
        canvas.DrawTextBox(opts.placeholder.text, layout.innerRect, inputFont, placeholderCol, 0.0f, 0.5f);
    }

    canvas.PopClip();
}

} // namespace guikit
