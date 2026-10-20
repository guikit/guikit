#include "guikit/widgets/TagInput.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

struct TagChipView {
    std::string_view text{};
    UiVariant variant{UiVariant::Default};
    Icon icon{};
};

struct TagInputPersistState {
    std::string editBuffer{};
    float scrollOffsetX{0.0f};
    int hoveredChipRemoveIdx{-1};
    bool clearHovered{false};
    std::vector<Rect> chipRemoveRects{};
    Rect clearBtnRect{};
};

struct TagInputNodeData {
    TagInputOptions opts{};
    std::span<const TagChipView> chips{};
    std::string_view editBuffer{};
    size_t cursor{0};
    bool cursorVisible{false};
    float cursorWidth{1.5f};
    int hoveredChipRemoveIdx{-1};
    bool clearHovered{false};
    float scrollOffsetX{0.0f};
    TagInputPersistState* persistState{nullptr};
};

ControlSpec EffectiveTagInputControl(const TagInputOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const InputFieldMetrics& m = theme.GetInputField(size);
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

std::string_view TrimWhitespace(std::string_view s) noexcept {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start])) != 0) {
        ++start;
    }
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1])) != 0) {
        --end;
    }
    return s.substr(start, end - start);
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) {
            return false;
        }
    }
    return true;
}

std::string FormatCounterText(size_t count, uint32_t maxTags) {
    char buf[48];
    if (maxTags > 0) {
        std::snprintf(buf, sizeof(buf), "%zu/%u", count, maxTags);
    } else {
        std::snprintf(buf, sizeof(buf), "%zu", count);
    }
    return std::string(buf);
}

float MeasureChipWidth(
    const TagChipView& chip,
    float chipFont,
    float iconSz,
    bool showRemove,
    ICanvas* canvas
) noexcept {
    const float textW = TextUtil::MeasureWidth(chip.text, chipFont, canvas);
    float w = 6.0f + textW;
    if (chip.icon.IsValid()) {
        w += iconSz + 4.0f;
    }
    if (showRemove) {
        const float removeBtnSz = std::max(12.0f, iconSz + 2.0f);
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
    std::string counterStr{};
    std::vector<Rect> chipRects{};
    std::vector<Rect> chipRemoveRects{};
    size_t visibleChipCount{0};
    size_t overflowCount{0};
    Rect overflowRect{};
    float inlineTextX{0.0f};
    float resolvedScrollX{0.0f};
};

TagLayoutResult ComputeTagLayout(
    Rect boxRect,
    std::span<const TagChipView> chips,
    std::string_view editBuffer,
    size_t cursor,
    const TagInputOptions& opts,
    UiSize size,
    bool focused,
    bool disabled,
    float currentScrollX,
    ICanvas* canvas,
    const UiTheme& theme
) {
    TagLayoutResult out{};
    const InputFieldMetrics& m = theme.GetInputField(size);
    const float chipFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
    const float inputFont = theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size);
    const float chipIconSz = std::max(10.0f, m.iconSize - 2.0f);
    const float removeBtnSz = std::max(12.0f, chipIconSz + 2.0f);
    const bool showRemove = !opts.readOnly && !disabled;

    float leftX = boxRect.x + m.paddingX;
    if (opts.prefix.HasContentOrReserve()) {
        const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const float pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, canvas);
        const float pGap = (opts.prefix.HasIcon() && !opts.prefix.HasText()) ? 6.0f : 4.0f;
        leftX += pW + pGap;
    }

    float rightX = boxRect.x + boxRect.w - m.paddingX;

    const bool showClear = opts.clearable && !chips.empty() && !opts.readOnly && !disabled;
    if (showClear) {
        const float btnSize = m.iconSize + 4.0f;
        rightX -= btnSize;
        out.clearBtnRect = Rect(rightX, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        rightX -= 4.0f;
    }

    const bool showCounter = opts.showCounter || opts.maxTags > 0;
    if (showCounter) {
        out.counterStr = FormatCounterText(chips.size(), opts.maxTags);
        const float counterTextW = TextUtil::MeasureWidth(out.counterStr, chipFont, canvas);
        const float badgeW = counterTextW + 10.0f;
        const float badgeH = std::max(16.0f, std::min(boxRect.h - 8.0f, chipFont + 6.0f));
        rightX -= badgeW;
        out.counterRect = Rect(rightX, boxRect.y + (boxRect.h - badgeH) * 0.5f, badgeW, badgeH);
        rightX -= 6.0f;
    }

    const float availW = std::max(0.0f, rightX - leftX);
    out.innerRect = Rect(leftX, boxRect.y, availW, boxRect.h);

    const float chipH = std::max(16.0f, boxRect.h - 8.0f);
    const float chipY = boxRect.y + (boxRect.h - chipH) * 0.5f;
    const float chipGap = 4.0f;

    out.chipRects.assign(chips.size(), Rect{});
    out.chipRemoveRects.assign(chips.size(), Rect{});

    std::vector<float> chipWidths(chips.size(), 0.0f);
    float totalChipsW = 0.0f;
    for (size_t i = 0; i < chips.size(); ++i) {
        chipWidths[i] = MeasureChipWidth(chips[i], chipFont, chipIconSz, showRemove, canvas);
        totalChipsW += chipWidths[i];
        if (i + 1 < chips.size()) {
            totalChipsW += chipGap;
        }
    }

    if (focused || !editBuffer.empty()) {
        out.visibleChipCount = chips.size();
        out.overflowCount = 0;

        float relX = 0.0f;
        for (size_t i = 0; i < chips.size(); ++i) {
            relX += chipWidths[i] + chipGap;
        }
        const float textStartRelX = relX;
        const size_t safeCursor = std::min(cursor, editBuffer.size());
        const float cursorOffset = TextUtil::MeasureWidth(editBuffer.substr(0, safeCursor), inputFont, canvas);
        const float fullEditW = std::max(
            TextUtil::MeasureWidth(editBuffer, inputFont, canvas) + 12.0f,
            (chips.empty() && editBuffer.empty()) ? TextUtil::MeasureWidth(opts.placeholder.text, inputFont, canvas) : 28.0f
        );
        const float totalContentW = textStartRelX + fullEditW;
        const float cursorRelX = textStartRelX + cursorOffset;

        float scrollX = focused ? currentScrollX : 0.0f;
        if (focused && availW > 0.0f) {
            const float visibleW = std::max(0.0f, availW - 4.0f);
            if (cursorRelX + 4.0f > scrollX + visibleW) {
                scrollX = (cursorRelX + 4.0f) - visibleW;
            } else if (cursorRelX < scrollX) {
                scrollX = std::max(0.0f, cursorRelX - 16.0f);
            }
            const float maxScroll = std::max(0.0f, totalContentW - visibleW);
            scrollX = std::clamp(scrollX, 0.0f, maxScroll);
        } else {
            scrollX = 0.0f;
        }
        out.resolvedScrollX = scrollX;

        float curX = out.innerRect.x - scrollX;
        for (size_t i = 0; i < chips.size(); ++i) {
            const float cw = chipWidths[i];
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
            float curX = out.innerRect.x;
            for (size_t i = 0; i < chips.size(); ++i) {
                const float cw = chipWidths[i];
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
            size_t fitCount = 0;
            float usedW = 0.0f;
            for (size_t i = 0; i < chips.size(); ++i) {
                const size_t remaining = chips.size() - (i + 1);
                char ovBuf[32];
                std::snprintf(ovBuf, sizeof(ovBuf), "+%zu", remaining);
                const float ovBadgeW = (remaining > 0)
                    ? (TextUtil::MeasureWidth(ovBuf, chipFont, canvas) + 12.0f + chipGap)
                    : 0.0f;
                const float nextW = usedW + (i > 0 ? chipGap : 0.0f) + chipWidths[i];
                if (nextW + ovBadgeW <= availW) {
                    usedW = nextW;
                    fitCount = i + 1;
                } else {
                    break;
                }
            }
            out.visibleChipCount = fitCount;
            out.overflowCount = chips.size() - fitCount;

            float curX = out.innerRect.x;
            for (size_t i = 0; i < fitCount; ++i) {
                const float cw = chipWidths[i];
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
                const float ovW = TextUtil::MeasureWidth(ovBuf, chipFont, canvas) + 12.0f;
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
    const StateColor& colorOverride,
    bool hovered,
    bool focused,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const UiVariant effectiveVar = (chipVariant != UiVariant::Default) ? chipVariant : defaultVariant;
    Color baseAccent;
    switch (effectiveVar) {
        case UiVariant::Default:
        case UiVariant::Secondary:
            baseAccent = theme.palette.textSecondary;
            break;
        case UiVariant::Ghost:
        case UiVariant::Muted:
            baseAccent = theme.palette.textMuted;
            break;
        default:
            baseAccent = theme.GetVariant(effectiveVar).bg;
            break;
    }
    if (chipVariant == UiVariant::Default && colorOverride.HasAny()) {
        baseAccent = colorOverride.Resolve(hovered, focused, disabled, baseAccent);
    }
    return baseAccent;
}

template <typename TagT, typename GetTextFn, typename MakeTagFn>
Response RunTagInputImpl(
    Context& ctx,
    std::vector<TagT>& tags,
    const TagInputOptions& opts,
    std::source_location loc,
    GetTextFn&& getText,
    MakeTagFn&& makeTag
) {
    struct ThemeScopeGuard {
        Context& c;
        bool pushed{false};
        ~ThemeScopeGuard() noexcept {
            if (pushed) c.PopTheme();
        }
    };

    const Id id = ctx.MakeChildId(loc);
    UiTheme* scopedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    ThemeScopeGuard themeGuard{ctx, scopedTheme != nullptr};

    const UiSize activeSize = ctx.GetCurrentSize();
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
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);
    optsCopy.placeholder = ctx.AllocText(opts.placeholder);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    optsCopy.prefix = ctx.AllocAffix(opts.prefix);

    auto& st = ctx.GetOrCreateState<TagInputPersistState>(id);
    const UiTheme& theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();

    auto buildTempViews = [&]() {
        std::vector<TagChipView> views;
        views.reserve(tags.size());
        for (const auto& t : tags) {
            if constexpr (std::is_same_v<TagT, TagItem>) {
                views.push_back(TagChipView{
                    .text = t.text,
                    .variant = (t.variant != UiVariant::Default) ? t.variant : optsCopy.tagVariant,
                    .icon = t.icon
                });
            } else {
                views.push_back(TagChipView{
                    .text = getText(t),
                    .variant = optsCopy.tagVariant,
                    .icon = {}
                });
            }
        }
        return views;
    };

    const Rect prevBounds = (res.rect.w > 0.0f && res.rect.h > 0.0f)
        ? res.rect
        : Rect({0.0f, 0.0f}, TagInput::Measure({}, optsCopy, activeSize, canvas, theme));

    if (st.chipRemoveRects.size() != tags.size()) {
        auto tempViews = buildTempViews();
        const auto initLayout = ComputeTagLayout(
            prevBounds,
            tempViews,
            st.editBuffer,
            st.editBuffer.size(),
            optsCopy,
            activeSize,
            ctx.IsFocused(id) && !effectiveDisabled,
            effectiveDisabled,
            st.scrollOffsetX,
            canvas,
            theme
        );
        st.chipRemoveRects = initLayout.chipRemoveRects;
        st.clearBtnRect = initLayout.clearBtnRect;
    }

    const Vec2 mousePos = ctx.GetMousePos();
    st.hoveredChipRemoveIdx = -1;
    st.clearHovered = false;

    if (res.hovered && !optsCopy.readOnly && !effectiveDisabled) {
        if (optsCopy.clearable && !tags.empty() && st.clearBtnRect.w > 0.0f && st.clearBtnRect.Contains(mousePos)) {
            st.clearHovered = true;
        } else {
            for (size_t i = 0; i < st.chipRemoveRects.size() && i < tags.size(); ++i) {
                if (st.chipRemoveRects[i].w > 0.0f && st.chipRemoveRects[i].Contains(mousePos)) {
                    st.hoveredChipRemoveIdx = static_cast<int>(i);
                    break;
                }
            }
        }
    }

    const bool mouseJustPressed = res.hovered && ctx.IsMouseClicked() && !effectiveDisabled;
    if ((mouseJustPressed || res.clicked) && !effectiveDisabled) {
        if (!optsCopy.readOnly) {
            if (mouseJustPressed && st.hoveredChipRemoveIdx >= 0 &&
                static_cast<size_t>(st.hoveredChipRemoveIdx) < tags.size()) {
                tags.erase(tags.begin() + st.hoveredChipRemoveIdx);
                st.hoveredChipRemoveIdx = -1;
                res.changed = true;
                ctx.RequestFocus(id);
                (void)ctx.EnsureTextEdit(id, st.editBuffer, /*multiline=*/false, /*readOnly=*/false);
            } else if (mouseJustPressed && st.clearHovered && optsCopy.clearable && !tags.empty()) {
                tags.clear();
                st.editBuffer.clear();
                st.scrollOffsetX = 0.0f;
                st.clearHovered = false;
                st.hoveredChipRemoveIdx = -1;
                ctx.SetTextEditBuffer(id, "");
                res.changed = true;
                ctx.RequestFocus(id);
                (void)ctx.EnsureTextEdit(id, st.editBuffer, /*multiline=*/false, /*readOnly=*/false);
            } else {
                ctx.RequestFocus(id);
                (void)ctx.EnsureTextEdit(id, st.editBuffer, /*multiline=*/false, /*readOnly=*/false);
            }
        } else {
            ctx.RequestFocus(id);
        }
    }

    bool isFocused = ctx.IsFocused(id) && !effectiveDisabled;
    res.focused = isFocused;

    size_t cursor = st.editBuffer.size();
    size_t selAnchor = cursor;

    auto tryCommitToken = [&](std::string_view raw) -> bool {
        const std::string_view token = TrimWhitespace(raw);
        if (token.empty()) {
            return false;
        }
        if (optsCopy.maxTags > 0 && tags.size() >= optsCopy.maxTags) {
            return false;
        }
        if (!optsCopy.allowDuplicates) {
            for (const auto& existing : tags) {
                if (EqualsIgnoreCase(getText(existing), token)) {
                    return false;
                }
            }
        }
        tags.push_back(makeTag(token));
        return true;
    };

    if ((isFocused || ctx.HasActiveTextEdit(id)) && !optsCopy.readOnly && !effectiveDisabled) {
        bool textChanged = false;
        bool textSubmitted = false;
        const std::string_view liveBuf = ctx.GetOrStartTextEdit(
            id,
            st.editBuffer,
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

            size_t lastDelimPos = std::string::npos;
            for (size_t i = 0; i < st.editBuffer.size(); ++i) {
                if (isDelimiter(st.editBuffer[i])) {
                    lastDelimPos = i;
                }
            }

            if (lastDelimPos != std::string::npos) {
                size_t tokenStart = 0;
                bool anyAdded = false;
                for (size_t i = 0; i <= lastDelimPos; ++i) {
                    if (isDelimiter(st.editBuffer[i])) {
                        const std::string_view seg(st.editBuffer.data() + tokenStart, i - tokenStart);
                        if (tryCommitToken(seg)) {
                            anyAdded = true;
                        }
                        tokenStart = i + 1;
                    }
                }
                const std::string tail = std::string(TrimWhitespace(
                    std::string_view(st.editBuffer).substr(lastDelimPos + 1)
                ));
                st.editBuffer = tail;
                ctx.SetTextEditBuffer(id, st.editBuffer);
                cursor = st.editBuffer.size();
                selAnchor = cursor;
                ctx.SetTextEditCursor(id, cursor, selAnchor);
                if (anyAdded) {
                    res.changed = true;
                }
            }
        }

        if (textSubmitted || ctx.ConsumeKey(Key::Enter)) {
            if (tryCommitToken(st.editBuffer)) {
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
                tags.pop_back();
                res.changed = true;
            }
        }
    }

    const size_t chipCount = tags.size();
    TagChipView* arenaChips = nullptr;
    if (chipCount > 0) {
        arenaChips = static_cast<TagChipView*>(
            ctx.GetArena().AllocateRaw(sizeof(TagChipView) * chipCount, alignof(TagChipView))
        );
        for (size_t i = 0; i < chipCount; ++i) {
            if constexpr (std::is_same_v<TagT, TagItem>) {
                Icon ic = tags[i].icon;
                if (!ic.GetPath().empty()) {
                    ic = Icon::File(ctx.AllocString(ic.GetPath()));
                }
                arenaChips[i] = TagChipView{
                    .text = ctx.AllocString(tags[i].text),
                    .variant = (tags[i].variant != UiVariant::Default) ? tags[i].variant : optsCopy.tagVariant,
                    .icon = ic
                };
            } else {
                arenaChips[i] = TagChipView{
                    .text = ctx.AllocString(getText(tags[i])),
                    .variant = optsCopy.tagVariant,
                    .icon = {}
                };
            }
        }
    }

    const std::span<const TagChipView> chipSpan(arenaChips, chipCount);
    const TagLayoutResult layoutNow = ComputeTagLayout(
        prevBounds,
        chipSpan,
        st.editBuffer,
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
    st.chipRemoveRects = layoutNow.chipRemoveRects;
    st.clearBtnRect = layoutNow.clearBtnRect;

    const ControlSpec ctrl = EffectiveTagInputControl(optsCopy, activeSize, theme);
    Node* node = ctx.CreateNode(NodeType::TagInput, id);
    node->theme = scopedTheme;
    node->debugBoundsOverride = optsCopy.debugBounds;
    node->inspectModeOverride = optsCopy.inspectMode;

    auto* data = ctx.AllocPayload<TagInputNodeData>();
    data->opts = optsCopy;
    data->chips = chipSpan;
    data->editBuffer = ctx.AllocString(st.editBuffer);
    data->cursor = cursor;
    data->cursorVisible = isFocused && !optsCopy.readOnly && ctx.IsCursorVisible();
    data->cursorWidth = ctx.GetCursorConfig().width;
    data->hoveredChipRemoveIdx = st.hoveredChipRemoveIdx;
    data->clearHovered = st.clearHovered;
    data->scrollOffsetX = st.scrollOffsetX;
    data->persistState = &st;
    node->SetPayload(data);

    uint64_t chipsHash = detail::HashArgs(chipCount);
    for (size_t i = 0; i < chipCount; ++i) {
        chipsHash = detail::HashArgs(
            chipsHash,
            chipSpan[i].text,
            static_cast<uint8_t>(chipSpan[i].variant),
            static_cast<uint16_t>(chipSpan[i].icon.GetId()),
            chipSpan[i].icon.GetPath()
        );
    }

    node->visualHash = detail::HashArgs(
        chipsHash,
        static_cast<uint8_t>(optsCopy.tagVariant),
        static_cast<uint8_t>(optsCopy.align),
        static_cast<uint8_t>(optsCopy.stretch),
        optsCopy.maxTags,
        optsCopy.clearable,
        optsCopy.showCounter,
        optsCopy.readOnly,
        optsCopy.color,
        optsCopy.background,
        optsCopy.border,
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.minWidth.value_or(0.0f),
        optsCopy.placeholder,
        optsCopy.prefix,
        data->editBuffer,
        data->cursor,
        data->cursorVisible,
        data->hoveredChipRemoveIdx,
        data->clearHovered,
        data->scrollOffsetX
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* c, const UiTheme& t) noexcept -> Vec2 {
        const auto* d = n->As<TagInputNodeData>();
        if (!d) {
            return TagInput::Measure({}, TagInputOptions{}, n->uiSize, c, t, availableWidth);
        }
        const InputFieldMetrics& m = t.GetInputField(n->uiSize);
        const ControlSpec spec = EffectiveTagInputControl(d->opts, n->uiSize, t);
        float boxW = m.defaultWidth;
        if (d->opts.stretch != StretchMode::Fill) {
            if (spec.width.has_value()) {
                boxW = t.Scale(*spec.width);
            } else {
                const float chipFont = t.ResolveFontSize(TextRole::LabelSmall, n->uiSize);
                const float chipIconSz = std::max(10.0f, m.iconSize - 2.0f);
                const bool showRemove = !d->opts.readOnly && !n->disabled;
                float contentW = m.paddingX * 2.0f;
                for (size_t i = 0; i < d->chips.size(); ++i) {
                    contentW += MeasureChipWidth(d->chips[i], chipFont, chipIconSz, showRemove, c) + 4.0f;
                }
                if (!d->opts.readOnly) {
                    const float inputFont = t.ResolveFontSize(d->opts.placeholder, TextRole::LabelMedium, n->uiSize);
                    const float phW = d->chips.empty()
                        ? TextUtil::MeasureWidth(d->opts.placeholder.text, inputFont, c)
                        : 56.0f;
                    contentW += std::max(48.0f, phW + 8.0f);
                }
                if (d->opts.prefix.HasContentOrReserve()) {
                    const float pFont = t.ResolveFontSize(d->opts.prefix.text, TextRole::LabelMedium, n->uiSize);
                    contentW += TextUtil::MeasureAffixWidth(d->opts.prefix, pFont, m.iconSize, 4.0f, c) + 6.0f;
                }
                if (d->opts.clearable && !d->opts.readOnly) {
                    contentW += m.iconSize + 8.0f;
                }
                if (d->opts.showCounter || d->opts.maxTags > 0) {
                    contentW += 36.0f;
                }
                boxW = std::max(m.defaultWidth, contentW);
            }
        }
        if (spec.minWidth.has_value()) {
            boxW = std::max(t.Scale(*spec.minWidth), boxW);
        } else {
            boxW = std::max(m.minWidth, boxW);
        }
        return Control::Measure({boxW, m.defaultHeight}, spec, d->opts.stretch, n->uiSize, t);
    };

    node->renderFn = &TagInput::Render;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->focused = isFocused;
    node->pressed = res.pressed;
    node->widthMode = (optsCopy.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);
    ctx.AdaptChildToContainer(node);

    return res;
}

} // namespace

Response TagInput::Run(
    Context& ctx,
    std::vector<std::string>& tags,
    const TagInputOptions& opts,
    std::source_location loc
) {
    return RunTagInputImpl(
        ctx,
        tags,
        opts,
        loc,
        [](const std::string& s) noexcept -> std::string_view { return s; },
        [](std::string_view tok) -> std::string { return std::string(tok); }
    );
}

Response TagInput::Run(
    Context& ctx,
    std::vector<TagItem>& tags,
    const TagInputOptions& opts,
    std::source_location loc
) {
    return RunTagInputImpl(
        ctx,
        tags,
        opts,
        loc,
        [](const TagItem& item) noexcept -> std::string_view { return item.text; },
        [&opts](std::string_view tok) -> TagItem {
            return TagItem{
                .text = std::string(tok),
                .variant = opts.tagVariant,
                .icon = {}
            };
        }
    );
}

Vec2 TagInput::Measure(
    std::span<const std::string> tags,
    const TagInputOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const InputFieldMetrics& m = theme.GetInputField(size);
    const ControlSpec ctrl = EffectiveTagInputControl(opts, size, theme);

    float boxW = m.defaultWidth;
    if (opts.stretch != StretchMode::Fill) {
        if (ctrl.width.has_value()) {
            boxW = theme.Scale(*ctrl.width);
        } else {
            const float chipFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
            const float chipIconSz = std::max(10.0f, m.iconSize - 2.0f);
            const bool showRemove = !opts.readOnly && !opts.disabled;
            float contentW = m.paddingX * 2.0f;
            for (const auto& tag : tags) {
                const TagChipView v{.text = tag, .variant = opts.tagVariant};
                contentW += MeasureChipWidth(v, chipFont, chipIconSz, showRemove, canvas) + 4.0f;
            }
            if (!opts.readOnly) {
                const float inputFont = theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size);
                const float phW = tags.empty()
                    ? TextUtil::MeasureWidth(opts.placeholder.text, inputFont, canvas)
                    : 56.0f;
                contentW += std::max(48.0f, phW + 8.0f);
            }
            if (opts.prefix.HasContentOrReserve()) {
                const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
                contentW += TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, canvas) + 6.0f;
            }
            if (opts.clearable && !opts.readOnly) {
                contentW += m.iconSize + 8.0f;
            }
            if (opts.showCounter || opts.maxTags > 0) {
                contentW += 36.0f;
            }
            boxW = std::max(m.defaultWidth, contentW);
        }
    }

    if (ctrl.minWidth.has_value()) {
        boxW = std::max(theme.Scale(*ctrl.minWidth), boxW);
    } else {
        boxW = std::max(m.minWidth, boxW);
    }

    return Control::Measure({boxW, m.defaultHeight}, ctrl, opts.stretch, size, theme);
}

void TagInput::Render(const Node* node, ICanvas& canvas, const UiTheme& theme) {
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
    const ControlSpec ctrl = EffectiveTagInputControl(opts, size, theme);
    const auto& pal = theme.palette;

    Color boxBg = ctrl.HasBackground(true)
        ? ((hovered && !disabled) ? pal.surface : pal.inputBackground)
        : Color::Transparent();
    Color boxBorder;
    float strokeW = (ctrl.border.width > 0.0f) ? theme.Scale(ctrl.border.width) : m.strokeWidth;

    if (focused && !disabled) {
        boxBorder = opts.color.HasAny()
            ? opts.color.Resolve(hovered, focused, disabled, pal.focusRing)
            : pal.focusRing;
        if (ctrl.border.width > 0.0f || m.strokeWidth > 0.0f) {
            strokeW += 0.5f;
        }
    } else if (hovered && !disabled) {
        boxBorder = pal.borderHover;
    } else {
        boxBorder = pal.inputBorder;
    }

    Color textCol = disabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary;
    Color placeholderCol = theme.ResolveTextColor(opts.placeholder, UiVariant::Secondary, disabled, hovered, focused);
    Color unitCol = disabled ? pal.textSecondary.WithAlphaF(0.40f) : pal.textSecondary;

    if (disabled) {
        boxBg = boxBg.WithAlphaF(0.40f);
        boxBorder = boxBorder.WithAlphaF(0.40f);
    }

    if (ctrl.HasBackground(true) && ctrl.background.HasAny() && !ctrl.background.IsSurface()) {
        boxBg = ctrl.background.Resolve(hovered, focused, disabled, boxBg);
    }
    if (ctrl.border.color.HasAny()) {
        boxBorder = ctrl.border.color.Resolve(hovered, focused, disabled, boxBorder);
    }

    const float radius = ctrl.border.radius.has_value() ? theme.Scale(*ctrl.border.radius) : m.cornerRadius;
    canvas.DrawRoundRect(boxRect, radius, boxBg, boxBorder, strokeW);

    if (opts.prefix.HasContentOrReserve()) {
        const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const Color pTextCol = theme.ResolveTextColor(opts.prefix.text, UiVariant::Secondary, disabled, hovered, focused);
        const Color pIconCol = opts.prefix.text.color.HasAny() ? pTextCol : unitCol;
        const float pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, &canvas);
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

    const std::span<const TagChipView> chips = data ? data->chips : std::span<const TagChipView>{};
    const std::string_view editBuf = data ? data->editBuffer : std::string_view{};
    const size_t cursor = data ? data->cursor : 0;
    const float scrollIn = data ? data->scrollOffsetX : 0.0f;

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
        data->persistState->chipRemoveRects = layout.chipRemoveRects;
        data->persistState->clearBtnRect = layout.clearBtnRect;
    }

    const float chipFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
    const float inputFont = theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size);
    const float chipIconSz = std::max(10.0f, m.iconSize - 2.0f);

    if (layout.counterRect.w > 0.0f) {
        const bool atLimit = (opts.maxTags > 0 && chips.size() >= opts.maxTags);
        const Color badgeAccent = atLimit ? theme.GetVariant(UiVariant::Warning).bg : pal.textSecondary;
        const Color badgeBg = atLimit ? badgeAccent.WithAlphaF(0.18f) : pal.surfaceHover.WithAlphaF(disabled ? 0.35f : 0.85f);
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
        const float clearIconSz = std::max(8.0f, m.iconSize - 2.0f);
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
    const int hoveredRemoveIdx = data ? data->hoveredChipRemoveIdx : -1;

    for (size_t i = 0; i < layout.visibleChipCount && i < chips.size(); ++i) {
        const Rect cRect = layout.chipRects[i];
        if (cRect.Right() < layout.innerRect.x || cRect.x > layout.innerRect.Right()) {
            continue;
        }

        const TagChipView& chip = chips[i];
        const UiVariant effectiveVar = (chip.variant != UiVariant::Default) ? chip.variant : opts.tagVariant;
        const Color accent = ResolveChipAccentColor(
            chip.variant,
            opts.tagVariant,
            opts.color,
            hovered,
            focused,
            disabled,
            theme
        );

        const Color chipBg = accent.WithAlphaF(disabled ? 0.08f : 0.16f);
        const Color chipBorder = accent.WithAlphaF(disabled ? 0.20f : 0.42f);
        Color chipTextCol = (effectiveVar == UiVariant::Default || effectiveVar == UiVariant::Secondary)
            ? pal.textPrimary
            : ((effectiveVar == UiVariant::Ghost || effectiveVar == UiVariant::Muted) ? pal.textSecondary : accent);
        if (disabled) {
            chipTextCol = chipTextCol.WithAlphaF(0.45f);
        }

        canvas.DrawRoundRect(cRect, 4.0f, chipBg, chipBorder, 1.0f);

        float cx = cRect.x + 6.0f;
        if (chip.icon.IsValid()) {
            const Rect icRect(cx, cRect.y + (cRect.h - chipIconSz) * 0.5f, chipIconSz, chipIconSz);
            chip.icon.Draw(canvas, icRect, chipTextCol);
            cx += chipIconSz + 4.0f;
        }

        const float textRight = showRemove ? layout.chipRemoveRects[i].x - 2.0f : cRect.Right() - 6.0f;
        const Rect chipTextRect(cx, cRect.y, std::max(0.0f, textRight - cx), cRect.h);
        canvas.DrawTextBox(chip.text, chipTextRect, chipFont, chipTextCol, 0.0f, 0.5f);

        if (showRemove) {
            const Rect remRect = layout.chipRemoveRects[i];
            const bool remHovered = (hoveredRemoveIdx == static_cast<int>(i));
            if (remHovered) {
                canvas.DrawRoundRect(remRect, 3.0f, accent.WithAlphaF(0.28f), Color::Transparent(), 0.0f);
            }
            const float xIconSz = std::max(7.0f, chipIconSz - 2.0f);
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
        const float textAvailW = std::max(64.0f, layout.innerRect.Right() - layout.inlineTextX);
        const Rect inlineRect(layout.inlineTextX, boxRect.y, textAvailW, boxRect.h);

        if (!editBuf.empty()) {
            const float drawW = std::max(inlineRect.w, canvas.MeasureText(editBuf, inputFont).x + 8.0f);
            canvas.DrawTextBox(editBuf, Rect(inlineRect.x, inlineRect.y, drawW, inlineRect.h), inputFont, textCol, 0.0f, 0.5f);
        } else if (chips.empty() && !opts.placeholder.empty()) {
            canvas.DrawTextBox(opts.placeholder.text, inlineRect, inputFont, placeholderCol, 0.0f, 0.5f);
        }

        if (focused && !disabled && data && data->cursorVisible) {
            const size_t safeCursor = std::min(cursor, editBuf.size());
            const float subW = canvas.MeasureText(editBuf.substr(0, safeCursor), inputFont).x;
            const float caretX = inlineRect.x + subW;
            const float caretH = inputFont * 1.15f;
            const float caretY = boxRect.y + (boxRect.h - caretH) * 0.5f;
            canvas.DrawLine({caretX, caretY}, {caretX, caretY + caretH}, pal.caret, data->cursorWidth);
        }
    } else if (chips.empty() && !opts.placeholder.empty()) {
        canvas.DrawTextBox(opts.placeholder.text, layout.innerRect, inputFont, placeholderCol, 0.0f, 0.5f);
    }

    canvas.PopClip();
}

} // namespace guikit
