#include "guikit/fields/input_field.hpp"
#include <cstring>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/style/field_frame.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/core.h"
#include "keel/utf8.h"

namespace guikit {

namespace {

ControlSpec EffectiveInputControl(const InputFieldOptions& opts, UiSize size, const UiTheme& theme) noexcept {
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

Vec2 MeasureInputBox(
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const InputFieldMetrics& m = theme.GetInputField(size);
    const ControlSpec ctrl = EffectiveInputControl(opts, size, theme);
    const bool hasVariants = !opts.valueFormat.variants.empty();

    const SizeMode effWMode = opts.width.ResolveMode(SizeMode::Stretch);
    const bool hasExplicitDpW = ctrl.width.IsFixed();
    keel::f32 boxW = m.defaultWidth;
    if (hasExplicitDpW) {
        boxW = ctrl.width.Dp();
    } else if (effWMode != SizeMode::Stretch && hasVariants) {
            const keel::f32 inputFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelMedium, size);
            keel::f32 contentW = 0.0f;
            for (keel::StringRef v : opts.valueFormat.variants) {
                if (!v.empty()) {
                    contentW = keel::max(contentW, TextUtil::MeasureWidth(v, inputFont, canvas));
                }
            }

            keel::f32 totalW = m.paddingX * 2.0f + contentW + 4.0f;
            if (opts.prefix.HasContentOrReserve()) {
                const keel::f32 pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
                const keel::f32 pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, canvas);
                const keel::f32 pGap = (opts.prefix.HasIcon() && !opts.prefix.HasText()) ? 6.0f : 4.0f;
                totalW += pW + pGap;
            }
            if (opts.suffix.HasContentOrReserve()) {
                const keel::f32 sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
                const keel::f32 sW = TextUtil::MeasureAffixWidth(opts.suffix, sFont, m.iconSize, 4.0f, canvas);
                totalW += sW + 6.0f;
            }
            if (opts.password && opts.passwordReveal) {
                totalW += m.iconSize + 8.0f;
            }
            if (opts.clearable && !opts.readOnly) {
                totalW += m.iconSize + 8.0f;
            }
            boxW = totalW;
    }

    if (ctrl.minWidth.has_value()) {
        boxW = keel::max(*ctrl.minWidth, boxW);
    } else if (hasExplicitDpW || (hasVariants && effWMode != SizeMode::Stretch)) {
        boxW = keel::max(m.defaultHeight, boxW);
    } else {
        boxW = keel::max(m.minWidth, boxW);
    }
    return {boxW, m.defaultHeight};
}

struct InputFieldState {
    keel::f32 scrollX{0.0f};
    keel::f64 lastClickTime{-10.0};
    Vec2 lastClickPos{-1000.0f, -1000.0f};
    keel::i32 clickCount{0};
    bool draggingWord{false};
    keel::usize wordDragStart{0};
    keel::usize wordDragEnd{0};
    bool passwordRevealed{false};
    bool actionBtnDown{false};
};

struct InputBoxRects {
    Rect boxRect{};
    Rect textRect{};
    Rect clearRect{};
    Rect revealRect{};
};

InputBoxRects ResolveInputBoxRects(
    Rect bounds,
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    bool showClearBtn,
    bool showRevealBtn
) noexcept {
    const InputFieldMetrics& m = theme.GetInputField(size);
    const Rect boxRect = bounds;

    keel::f32 leftX = boxRect.x + m.paddingX;
    if (opts.prefix.HasContentOrReserve()) {
        const keel::f32 pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const keel::f32 pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, canvas);
        const keel::f32 pGap = (opts.prefix.HasIcon() && !opts.prefix.HasText()) ? 6.0f : 4.0f;
        leftX += pW + pGap;
    }

    keel::f32 rightX = boxRect.x + boxRect.w - m.paddingX;

    if (opts.suffix.HasContentOrReserve()) {
        const keel::f32 sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
        const keel::f32 sW = TextUtil::MeasureAffixWidth(opts.suffix, sFont, m.iconSize, 4.0f, canvas);
        rightX -= sW + 6.0f;
    }

    Rect revealRect{};
    if (showRevealBtn) {
        const keel::f32 btnSize = m.iconSize + 4.0f;
        rightX -= btnSize;
        revealRect = Rect(rightX, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        rightX -= 4.0f;
    }

    Rect clearRect{};
    if (showClearBtn) {
        const keel::f32 btnSize = m.iconSize + 4.0f;
        rightX -= btnSize;
        clearRect = Rect(rightX, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        rightX -= 4.0f;
    }

    const keel::f32 availW = keel::max(0.0f, rightX - leftX);
    return InputBoxRects{
        .boxRect = boxRect,
        .textRect = Rect(leftX, boxRect.y, availW, boxRect.h),
        .clearRect = clearRect,
        .revealRect = revealRect
    };
}

keel::usize SnapToUtf8CodepointBoundary(keel::StringRef text, keel::usize idx) noexcept {
    idx = keel::min(idx, text.size());
    while (idx > 0 && idx < text.size() && (keel::as<keel::u8>(text[idx]) & 0xC0) == 0x80) {
        --idx;
    }
    return idx;
}

keel::StringRef AllocPasswordMask(Context& ctx, keel::StringRef text) noexcept {
    const keel::usize cpCount = keel::utf8_codepoint_count(text);
    if (cpCount == 0) {
        return {};
    }
    char* buf = ctx.GetArena().alloc_array<char>(cpCount);
    if (!buf) {
        return {};
    }
    keel::mem_set(buf, keel::as<keel::u8>('*'), cpCount);
    return {buf, cpCount};
}

keel::usize CodepointOffsetToByteIndex(keel::StringRef text, keel::usize cpIndex) noexcept {
    keel::usize byteIdx = 0;
    for (keel::usize i = 0; i < cpIndex && byteIdx < text.size(); ++i) {
        byteIdx = keel::utf8_next_boundary(text, byteIdx);
    }
    return byteIdx;
}

keel::usize ByteIndexToCodepointOffset(keel::StringRef text, keel::usize byteIndex) noexcept {
    const keel::usize clamped = keel::min(byteIndex, text.size());
    return keel::utf8_codepoint_count(text.substr(0, clamped));
}

keel::StringRef SanitizeInputText(
    Context& ctx,
    keel::StringRef raw,
    InputFilter filter,
    keel::usize maxLength
) noexcept {
    if (raw.empty()) return raw;

    keel::StringRef filtered = raw;
    if (filter != InputFilter::Any) {
        char* out = ctx.GetArena().alloc_array<char>(raw.size());
        keel::usize outLen = 0;
        bool hasDecimal = false;
        bool hasExp = false;
        bool afterExp = false;

        for (keel::usize i = 0; i < raw.size(); ++i) {
            const char ch = raw[i];
            if (filter == InputFilter::Integer) {
                if ((ch == '-' || ch == '+') && outLen == 0) {
                    out[outLen++] = ch;
                } else if (ch >= '0' && ch <= '9') {
                    out[outLen++] = ch;
                }
            } else if (filter == InputFilter::Decimal) {
                if ((ch == '-' || ch == '+') && (outLen == 0 || afterExp)) {
                    out[outLen++] = ch;
                    afterExp = false;
                } else if (ch >= '0' && ch <= '9') {
                    out[outLen++] = ch;
                    afterExp = false;
                } else if (ch == '.' && !hasDecimal && !hasExp) {
                    hasDecimal = true;
                    out[outLen++] = ch;
                    afterExp = false;
                } else if ((ch == 'e' || ch == 'E') && !hasExp && outLen > 0 &&
                           out[outLen - 1] != '-' && out[outLen - 1] != '+') {
                    hasExp = true;
                    afterExp = true;
                    out[outLen++] = ch;
                }
            } else if (filter == InputFilter::Hex) {
                if (ch == '#' && outLen == 0) {
                    out[outLen++] = ch;
                } else if ((ch == 'x' || ch == 'X') && outLen == 1 && out[0] == '0') {
                    out[outLen++] = ch;
                } else if ((ch >= '0' && ch <= '9') ||
                           (ch >= 'a' && ch <= 'f') ||
                           (ch >= 'A' && ch <= 'F')) {
                    out[outLen++] = ch;
                }
            }
        }
        filtered = {out, outLen};
    }

    if (maxLength > 0) {
        const keel::usize cutByte = CodepointOffsetToByteIndex(filtered, maxLength);
        if (cutByte < filtered.size()) {
            filtered = filtered.substr(0, cutByte);
        }
    }
    return filtered;
}

} // namespace

Response detail::InputField::Run(
    Context& ctx,
    keel::Optional<keel::StringSink> stringValue,
    keel::StringRef displayFallback,
    const InputFieldOptions& opts,
    keel::SourceLocation loc
) {

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool wasFocused = ctx.IsFocused(id) && !effectiveDisabled;

    ctx.RegisterFocusable(id, effectiveDisabled || opts.readOnly);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    }

    const bool mouseJustPressed = res.hovered && ctx.IsMouseClicked() && !effectiveDisabled;
    if ((mouseJustPressed || res.clicked) && !effectiveDisabled) {
        ctx.RequestFocus(id);
    }

    const bool isFocused = ctx.IsFocused(id) && !effectiveDisabled;
    res.focused = isFocused;

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    InputFieldOptions optsCopy = opts;
    if (optsCopy.width.IsAuto()) {
        optsCopy.width = Control::ResolveWidthMode(ctx, opts.width, SizeMode::Stretch);
    }
    optsCopy.placeholder = ctx.AllocText(opts.placeholder);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    optsCopy.prefix = ctx.AllocAffix(opts.prefix);
    optsCopy.suffix = ctx.AllocAffix(opts.suffix);
    optsCopy.valueFormat = ctx.AllocText(opts.valueFormat);

    keel::StringRef valStr = stringValue.has_value() ? stringValue->as_ref() : displayFallback;

    auto& state = ctx.GetOrCreatePersistentState<InputFieldState>(id);
    if (!res.pressed && !mouseJustPressed) {
        state.actionBtnDown = false;
    }

    const UiTheme& theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();
    const Rect refBounds = (res.rect.w > 0.0f && res.rect.h > 0.0f)
        ? res.rect
        : Rect({0.0f, 0.0f}, detail::InputField::Measure(valStr, optsCopy, activeSize, canvas, theme));

    const bool showRevealBtn = opts.password && opts.passwordReveal && !effectiveDisabled;
    bool showClearBtn = opts.clearable && !valStr.empty() && !effectiveDisabled && !opts.readOnly;
    InputBoxRects rects = ResolveInputBoxRects(refBounds, optsCopy, activeSize, canvas, theme, showClearBtn, showRevealBtn);

    const Vec2 mousePos = ctx.GetMousePos();
    bool revealHovered = showRevealBtn && res.hovered && rects.revealRect.Contains(mousePos);
    bool clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);

    if (mouseJustPressed) {
        if (revealHovered) {
            state.passwordRevealed = !state.passwordRevealed;
            state.actionBtnDown = true;
        } else if (clearHovered) {
            state.actionBtnDown = true;
            state.scrollX = 0.0f;
            if (stringValue.has_value()) {
                stringValue->clear();
                valStr = stringValue->as_ref();
            } else {
                valStr = "";
            }
            ctx.SetTextEditBuffer(id, "");
            showClearBtn = false;
            clearHovered = false;
            rects = ResolveInputBoxRects(refBounds, optsCopy, activeSize, canvas, theme, showClearBtn, showRevealBtn);
            res.changed = true;
        }
    }

    keel::usize cursor = 0;
    keel::usize selAnchor = 0;
    keel::StringRef displayText = valStr;
    const InputFilter effFilter = (opts.filter != InputFilter::Any)
        ? opts.filter
        : (opts.numericOnly ? InputFilter::Decimal : InputFilter::Any);

    if (isFocused || ctx.HasActiveTextEdit(id)) {
        keel::StringRef currentEditText = ctx.EnsureTextEdit(id, valStr, /*multiline=*/false, opts.readOnly);
        const bool justGainedFocus = (!wasFocused && isFocused);
        if (opts.selectAllOnFocus && justGainedFocus && !state.actionBtnDown) {
            ctx.SetTextEditCursor(id, currentEditText.size(), 0);
        }

        showClearBtn = opts.clearable && !currentEditText.empty() && !effectiveDisabled && !opts.readOnly;
        rects = ResolveInputBoxRects(refBounds, optsCopy, activeSize, canvas, theme, showClearBtn, showRevealBtn);
        clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);

        const Rect textRect = rects.textRect;
        const keel::f32 inputFont = theme.ResolveFontSize(optsCopy.valueFormat, TextRole::LabelMedium, activeSize);

        const bool isMasked = opts.password && !state.passwordRevealed;
        const keel::StringRef hitTestText = (isMasked && !currentEditText.empty())
            ? AllocPasswordMask(ctx, currentEditText)
            : currentEditText;

        auto mapHitToByte = [&](keel::usize idx) noexcept -> keel::usize {
            return isMasked
                ? CodepointOffsetToByteIndex(currentEditText, idx)
                : SnapToUtf8CodepointBoundary(currentEditText, idx);
        };

        if ((mouseJustPressed || res.pressed) && !state.actionBtnDown &&
            !(opts.selectAllOnFocus && justGainedFocus && mouseJustPressed)) {
            const keel::f32 localX = (mousePos.x - textRect.x) + state.scrollX;
            const keel::usize rawHitIdx = TextUtil::HitTestIndex(hitTestText, localX, inputFont, canvas);
            const keel::usize hitIdx = mapHitToByte(rawHitIdx);

            if (mouseJustPressed) {
                const keel::f64 now = ctx.GetCurrentTime();
                const keel::f32 dist = (mousePos - state.lastClickPos).Length();
                if (wasFocused && (now - state.lastClickTime) <= 0.35 && dist <= 6.0f) {
                    state.clickCount += 1;
                } else {
                    state.clickCount = 1;
                }
                state.lastClickTime = now;
                state.lastClickPos = mousePos;

                if (state.clickCount == 1) {
                    state.draggingWord = false;
                    if (ctx.GetKeyModifiers().shift && wasFocused) {
                        const keel::usize anchor = ctx.GetTextEditAnchor(id);
                        ctx.SetTextEditCursor(id, hitIdx, anchor);
                    } else {
                        ctx.SetTextEditCursor(id, hitIdx, hitIdx);
                    }
                } else if (state.clickCount == 2) {
                    keel::usize wStart = 0;
                    keel::usize wEnd = 0;
                    TextUtil::SelectWordAt(hitTestText, rawHitIdx, wStart, wEnd);
                    wStart = mapHitToByte(wStart);
                    wEnd = mapHitToByte(wEnd);
                    state.draggingWord = true;
                    state.wordDragStart = wStart;
                    state.wordDragEnd = wEnd;
                    ctx.SetTextEditCursor(id, wEnd, wStart);
                } else {
                    state.draggingWord = false;
                    ctx.SetTextEditCursor(id, currentEditText.size(), 0);
                }
            } else if (res.pressed) {
                if (state.clickCount >= 3) {
                    ctx.SetTextEditCursor(id, currentEditText.size(), 0);
                } else if (state.draggingWord) {
                    keel::usize wStart = 0;
                    keel::usize wEnd = 0;
                    TextUtil::SelectWordAt(hitTestText, rawHitIdx, wStart, wEnd);
                    wStart = mapHitToByte(wStart);
                    wEnd = mapHitToByte(wEnd);
                    if (hitIdx < state.wordDragStart) {
                        ctx.SetTextEditCursor(id, wStart, state.wordDragEnd);
                    } else {
                        ctx.SetTextEditCursor(id, wEnd, state.wordDragStart);
                    }
                } else {
                    const keel::usize anchor = ctx.GetTextEditAnchor(id);
                    ctx.SetTextEditCursor(id, hitIdx, anchor);
                }
            }
        } else if (opts.selectAllOnFocus && justGainedFocus && mouseJustPressed && !state.actionBtnDown) {
            state.lastClickTime = ctx.GetCurrentTime();
            state.lastClickPos = mousePos;
            state.clickCount = 1;
            state.draggingWord = false;
        }

        bool changed = false;
        bool submitted = false;
        displayText = ctx.GetOrStartTextEdit(id, valStr, cursor, selAnchor, changed, submitted, /*multiline=*/false, opts.readOnly);

        if (changed && !opts.readOnly && (effFilter != InputFilter::Any || opts.maxLength > 0)) {
            const keel::StringRef sanitized = SanitizeInputText(ctx, displayText, effFilter, opts.maxLength);
            if (sanitized != displayText) {
                ctx.SetTextEditBuffer(id, sanitized);
                cursor = keel::min(cursor, sanitized.size());
                selAnchor = keel::min(selAnchor, sanitized.size());
                ctx.SetTextEditCursor(id, cursor, selAnchor);
                displayText = sanitized;
                changed = (displayText != valStr);
            }
        }

        if (changed || submitted) {
            state.lastClickTime = -10.0;
            state.clickCount = 0;
        }

        if (changed && !opts.readOnly) {
            if (stringValue.has_value()) {
                const keel::StringRef prevVal = ctx.AllocString(valStr);
                stringValue->assign(displayText);
                const keel::StringRef stored = stringValue->as_ref();
                if (stored != displayText) {
                    cursor = keel::min(cursor, stored.size());
                    selAnchor = keel::min(selAnchor, stored.size());
                    ctx.SetTextEditBuffer(id, stored);
                    ctx.SetTextEditCursor(id, cursor, selAnchor);
                    displayText = stored;
                }
                res.changed = (displayText != prevVal);
            } else {
                res.changed = true;
            }
        }

        if (submitted) {
            res.submitted = true;
        }

        showClearBtn = opts.clearable && !displayText.empty() && !effectiveDisabled && !opts.readOnly;
        rects = ResolveInputBoxRects(refBounds, optsCopy, activeSize, canvas, theme, showClearBtn, showRevealBtn);
        clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);

        if (rects.textRect.w > 0.0f && isFocused) {
            const bool maskAfterEdit = opts.password && !state.passwordRevealed;
            const keel::StringRef measureText = (maskAfterEdit && !displayText.empty())
                ? AllocPasswordMask(ctx, displayText)
                : displayText;
            const keel::usize mappedCursor = maskAfterEdit
                ? ByteIndexToCodepointOffset(displayText, cursor)
                : cursor;
            const keel::usize safeCursor = keel::min(mappedCursor, measureText.size());
            const keel::f32 cursorX = TextUtil::MeasureWidth(measureText.substr(0, safeCursor), inputFont, canvas);
            const keel::f32 fullTextW = TextUtil::MeasureWidth(measureText, inputFont, canvas);
            const keel::f32 visibleW = keel::max(0.0f, rects.textRect.w - 2.0f);

            if (cursorX < state.scrollX) {
                state.scrollX = cursorX;
            } else if (cursorX > state.scrollX + visibleW) {
                state.scrollX = cursorX - visibleW;
            }
            const keel::f32 maxScroll = keel::max(0.0f, fullTextW - visibleW);
            state.scrollX = keel::clamp(state.scrollX, 0.0f, maxScroll);

            if (!opts.readOnly) {
                const keel::f32 caretX = rects.textRect.x - state.scrollX + cursorX;
                const keel::f32 caretH = inputFont * 1.15f;
                const keel::f32 caretY = rects.boxRect.y + (rects.boxRect.h - caretH) * 0.5f;
                ctx.SetImeTargetRect(Rect(caretX, caretY, 2.0f, caretH));
            }
        } else if (!isFocused) {
            state.scrollX = 0.0f;
        }
    } else {
        state.scrollX = 0.0f;
    }

    const keel::usize selStart = keel::min(cursor, selAnchor);
    const keel::usize selEnd = keel::max(cursor, selAnchor);

    const bool maskRendered = opts.password && !state.passwordRevealed;
    const keel::StringRef renderedText = (maskRendered && !displayText.empty())
        ? AllocPasswordMask(ctx, displayText)
        : ctx.AllocString(displayText);
    const keel::usize renderCursor = maskRendered ? ByteIndexToCodepointOffset(displayText, cursor) : cursor;
    const keel::usize renderSelStart = maskRendered ? ByteIndexToCodepointOffset(displayText, selStart) : selStart;
    const keel::usize renderSelEnd = maskRendered ? ByteIndexToCodepointOffset(displayText, selEnd) : selEnd;

    const ControlSpec ctrl = EffectiveInputControl(optsCopy, activeSize, theme);
    Node* node = ctx.CreateNode(NodeType::InputField, id);

    auto* data = ctx.AllocPayload<InputFieldData>();
    data->opts = optsCopy;
    data->displayText = renderedText;
    if (isFocused && !opts.readOnly && !maskRendered && ctx.IsImeComposing()) {
        data->imePreedit = ctx.AllocString(ctx.GetImeComposition().text.as_ref());
    }
    node->text = data->displayText.empty() ? optsCopy.placeholder.text : data->displayText;
    data->cursor = renderCursor;
    data->selStart = renderSelStart;
    data->selEnd = renderSelEnd;
    data->scrollX = state.scrollX;
    data->cursorVisible = isFocused && !opts.readOnly && ctx.IsCursorVisible(opts.cursorMode);
    data->cursorWidth = ctx.GetCursorConfig().width;
    data->showClearBtn = showClearBtn;
    data->clearHovered = clearHovered;
    data->showRevealBtn = showRevealBtn;
    data->revealHovered = revealHovered;
    data->passwordRevealed = state.passwordRevealed;
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
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
        optsCopy.readOnly,
        optsCopy.password,
        optsCopy.passwordReveal,
        optsCopy.clearable,
        data->displayText,
        data->imePreedit,
        data->cursor,
        data->selStart,
        data->selEnd,
        data->scrollX,
        data->showClearBtn,
        data->clearHovered,
        data->showRevealBtn,
        data->revealHovered,
        data->passwordRevealed,
        optsCopy.prefix,
        optsCopy.suffix,
        optsCopy.placeholder,
        optsCopy.valueFormat
    );
    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<InputFieldData>();
        return detail::InputField::Measure(
            d ? d->displayText : keel::StringRef{},
            d ? d->opts : InputFieldOptions{},
            n->uiSize,
            canvas,
            theme,
            availableWidth
        );
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<InputFieldData>();
        detail::InputField::Render(
            canvas,
            n->computedRect,
            d ? d->displayText : keel::StringRef{},
            d ? d->opts : InputFieldOptions{},
            n->uiSize,
            n->hovered,
            n->focused,
            d ? d->cursor : 0,
            n->disabled,
            d ? d->cursorVisible : false,
            d ? d->cursorWidth : 1.5f,
            theme,
            d ? d->selStart : 0,
            d ? d->selEnd : 0,
            d ? d->scrollX : 0.0f,
            d ? d->showClearBtn : false,
            d ? d->clearHovered : false,
            d ? d->showRevealBtn : false,
            d ? d->revealHovered : false,
            d ? d->passwordRevealed : false,
            d ? d->imePreedit : keel::StringRef{}
        );
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->focused = isFocused;
    node->pressed = res.pressed;

    Control::SetupNode(ctx, node, ctrl);

    return res;
}

Vec2 detail::InputField::Measure(
    keel::StringRef /*displayText*/,
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 /*availableWidth*/
) {
    const Vec2 boxDims = MeasureInputBox(opts, size, canvas, theme);
    const ControlSpec ctrl = EffectiveInputControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, theme);
}

Vec2 detail::InputField::Measure(
    keel::StringRef displayText,
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(displayText, opts, size, canvas, defaultTheme, availableWidth);
}

void detail::InputField::Render(
    ICanvas& canvas,
    Rect bounds,
    keel::StringRef displayText,
    const InputFieldOptions& opts,
    UiSize size,
    bool hovered,
    bool focused,
    keel::usize cursor,
    bool disabled,
    bool cursorVisible,
    keel::f32 cursorWidth,
    const UiTheme& theme,
    keel::usize selStart,
    keel::usize selEnd,
    keel::f32 scrollX,
    bool showClearBtn,
    bool clearHovered,
    bool showRevealBtn,
    bool revealHovered,
    bool passwordRevealed,
    keel::StringRef imePreedit
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const InputFieldMetrics& m = theme.GetInputField(size);
    const Rect boxRect = bounds;
    const auto& pal = theme.palette;

    Color textCol = opts.valueFormat.color.HasAny()
        ? theme.ResolveTextColor(opts.valueFormat, UiVariant::Default, disabled, hovered, focused)
        : (disabled ? pal.textPrimary.WithAlphaF(theme.disabledOpacity) : pal.textPrimary);
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

    const keel::f32 inputFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelMedium, size);

    keel::f32 leftX = boxRect.x + m.paddingX;
    if (opts.prefix.HasContentOrReserve()) {
        const keel::f32 pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const Color pTextCol = theme.ResolveTextColor(opts.prefix.text, UiVariant::Secondary, disabled, hovered, focused);
        const Color pIconCol = opts.prefix.text.color.HasAny() ? pTextCol : unitCol;
        const keel::f32 pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, &canvas);
        TextUtil::DrawAffix(canvas, opts.prefix, Rect(leftX, boxRect.y, pW, boxRect.h), pFont, m.iconSize, pTextCol, pIconCol, 4.0f);
        const keel::f32 pGap = (opts.prefix.HasIcon() && !opts.prefix.HasText()) ? 6.0f : 4.0f;
        leftX += pW + pGap;
    }

    keel::f32 rightX = boxRect.x + boxRect.w - m.paddingX;

    if (opts.suffix.HasContentOrReserve()) {
        const keel::f32 sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
        const Color sTextCol = theme.ResolveTextColor(opts.suffix.text, UiVariant::Secondary, disabled, hovered, focused);
        const Color sIconCol = opts.suffix.text.color.HasAny() ? sTextCol : unitCol;
        const keel::f32 sW = TextUtil::MeasureAffixWidth(opts.suffix, sFont, m.iconSize, 4.0f, &canvas);
        rightX -= sW;
        TextUtil::DrawAffix(canvas, opts.suffix, Rect(rightX, boxRect.y, sW, boxRect.h), sFont, m.iconSize, sTextCol, sIconCol, 4.0f);
        rightX -= 6.0f;
    }

    if (showRevealBtn) {
        const keel::f32 btnSize = m.iconSize + 4.0f;
        rightX -= btnSize;
        const Rect btnRect(rightX, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        if (revealHovered && !disabled) {
            canvas.DrawRoundRect(btnRect, 4.0f, pal.surfaceHover, Color::Transparent(), 0.0f);
        }
        const Rect icRect(rightX + 2.0f, boxRect.y + (boxRect.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
        const Color icCol = (revealHovered && !disabled) ? pal.textPrimary : unitCol;
        const Icon eyeIcon = passwordRevealed ? Icon::EyeOff() : Icon::Eye();
        eyeIcon.Draw(canvas, icRect, icCol);
        rightX -= 4.0f;
    }

    if (showClearBtn) {
        const keel::f32 btnSize = m.iconSize + 4.0f;
        rightX -= btnSize;
        const Rect btnRect(rightX, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        if (clearHovered && !disabled) {
            canvas.DrawRoundRect(btnRect, 4.0f, pal.surfaceHover, Color::Transparent(), 0.0f);
        }
        const keel::f32 clearIconSz = keel::max(8.0f, m.iconSize - 2.0f);
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

    const keel::f32 availW = keel::max(0.0f, rightX - leftX);
    const Rect textRect(leftX, boxRect.y, availW, boxRect.h);
    canvas.PushClip(textRect);

    if (focused && !disabled && selStart != selEnd && !displayText.empty()) {
        const keel::usize s0 = keel::min(keel::min(selStart, selEnd), displayText.size());
        const keel::usize s1 = keel::min(keel::max(selStart, selEnd), displayText.size());
        if (s1 > s0) {
            const keel::f32 x0 = textRect.x - scrollX + canvas.MeasureText(displayText.substr(0, s0), inputFont).x;
            const keel::f32 x1 = textRect.x - scrollX + canvas.MeasureText(displayText.substr(0, s1), inputFont).x;
            const keel::f32 selH = inputFont * 1.25f;
            const keel::f32 selY = boxRect.y + (boxRect.h - selH) * 0.5f;
            const Rect selRect(x0, selY, keel::max(1.0f, x1 - x0), selH);
            canvas.DrawRoundRect(selRect, 2.0f, pal.accent.WithAlpha(88), Color::Transparent(), 0.0f);
        }
    }

    if (displayText.empty() && imePreedit.empty() && !opts.placeholder.empty() && !focused) {
        const keel::f32 phFont = !opts.placeholder.size.IsAuto()
            ? theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size)
            : inputFont;
        canvas.DrawTextBox(opts.placeholder.text, textRect, phFont, placeholderCol, 0.0f, 0.5f);
    } else if (!imePreedit.empty() && focused && !disabled && !opts.readOnly) {
        const keel::usize safeCursor = keel::min(cursor, displayText.size());
        const keel::StringRef before = displayText.substr(0, safeCursor);
        const keel::StringRef after = displayText.substr(safeCursor);
        const keel::f32 beforeW = before.empty() ? 0.0f : canvas.MeasureText(before, inputFont).x;
        const keel::f32 preeditW = canvas.MeasureText(imePreedit, inputFont).x;
        const keel::f32 afterW = after.empty() ? 0.0f : canvas.MeasureText(after, inputFont).x;
        const keel::f32 baseX = textRect.x - scrollX;
        if (!before.empty()) {
            canvas.DrawTextBox(before, Rect(baseX, textRect.y, beforeW + 4.0f, textRect.h), inputFont, textCol, 0.0f, 0.5f);
        }
        const keel::f32 preX = baseX + beforeW;
        canvas.DrawTextBox(imePreedit, Rect(preX, textRect.y, preeditW + 4.0f, textRect.h), inputFont, textCol, 0.0f, 0.5f);
        const keel::f32 underlineY = boxRect.y + (boxRect.h + inputFont * 1.05f) * 0.5f;
        canvas.DrawLine({preX, underlineY}, {preX + preeditW, underlineY}, pal.accent, 1.5f);
        if (!after.empty()) {
            canvas.DrawTextBox(after, Rect(preX + preeditW, textRect.y, afterW + 4.0f, textRect.h), inputFont, textCol, 0.0f, 0.5f);
        }
    } else if (!displayText.empty()) {
        if (focused || scrollX > 0.0f) {
            const keel::f32 fullW = keel::max(textRect.w, canvas.MeasureText(displayText, inputFont).x + 8.0f);
            const Rect scrolledRect(textRect.x - scrollX, textRect.y, fullW, textRect.h);
            canvas.DrawTextBox(displayText, scrolledRect, inputFont, textCol, 0.0f, 0.5f);
        } else {
            canvas.DrawTextBox(displayText, textRect, inputFont, textCol, 0.0f, 0.5f);
        }
    }

    if (focused && !disabled && !opts.readOnly && cursorVisible) {
        const keel::usize safeCursor = keel::min(cursor, displayText.size());
        const keel::StringRef sub = displayText.substr(0, safeCursor);
        const keel::f32 subW = canvas.MeasureText(sub, inputFont).x +
            (imePreedit.empty() ? 0.0f : canvas.MeasureText(imePreedit, inputFont).x);
        const keel::f32 caretX = textRect.x - scrollX + subW;
        const keel::f32 caretH = inputFont * 1.15f;
        const keel::f32 caretY = boxRect.y + (boxRect.h - caretH) * 0.5f;
        canvas.DrawLine({caretX, caretY}, {caretX, caretY + caretH}, pal.caret, cursorWidth);
    }
    canvas.PopClip();
}

} // namespace guikit
