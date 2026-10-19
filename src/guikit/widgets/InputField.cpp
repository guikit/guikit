#include "guikit/widgets/InputField.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

ControlSpec EffectiveInputControl(const InputFieldOptions& opts, UiSize size, const UiTheme& theme) noexcept {
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

Vec2 MeasureInputBox(
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const InputFieldMetrics& m = theme.GetInputField(size);
    const ControlSpec ctrl = EffectiveInputControl(opts, size, theme);
    const bool hasVariants = !opts.valueFormat.variants.empty();

    float boxW = m.defaultWidth;
    if (opts.stretch != StretchMode::Fill) {
        if (ctrl.width.has_value()) {
            boxW = theme.Scale(*ctrl.width);
        } else if (hasVariants) {
            const float inputFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelMedium, size);
            float contentW = 0.0f;
            for (std::string_view v : opts.valueFormat.variants) {
                if (!v.empty()) {
                    contentW = std::max(contentW, TextUtil::MeasureWidth(v, inputFont, canvas));
                }
            }

            float totalW = m.paddingX * 2.0f + contentW + 4.0f;
            if (opts.prefix.HasContentOrReserve()) {
                const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
                const float pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, canvas);
                const float pGap = (opts.prefix.HasIcon() && !opts.prefix.HasText()) ? 6.0f : 4.0f;
                totalW += pW + pGap;
            }
            if (opts.suffix.HasContentOrReserve()) {
                const float sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
                const float sW = TextUtil::MeasureAffixWidth(opts.suffix, sFont, m.iconSize, 4.0f, canvas);
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
    }

    if (ctrl.minWidth.has_value()) {
        boxW = std::max(theme.Scale(*ctrl.minWidth), boxW);
    } else if (hasVariants && opts.stretch != StretchMode::Fill && !ctrl.width.has_value()) {
        boxW = std::max(m.defaultHeight, boxW);
    } else {
        boxW = std::max(m.minWidth, boxW);
    }
    return {boxW, m.defaultHeight};
}

struct InputFieldState {
    float scrollX{0.0f};
    double lastClickTime{-10.0};
    Vec2 lastClickPos{-1000.0f, -1000.0f};
    int clickCount{0};
    bool draggingWord{false};
    size_t wordDragStart{0};
    size_t wordDragEnd{0};
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

    float leftX = boxRect.x + m.paddingX;
    if (opts.prefix.HasContentOrReserve()) {
        const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const float pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, canvas);
        const float pGap = (opts.prefix.HasIcon() && !opts.prefix.HasText()) ? 6.0f : 4.0f;
        leftX += pW + pGap;
    }

    float rightX = boxRect.x + boxRect.w - m.paddingX;

    if (opts.suffix.HasContentOrReserve()) {
        const float sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
        const float sW = TextUtil::MeasureAffixWidth(opts.suffix, sFont, m.iconSize, 4.0f, canvas);
        rightX -= sW + 6.0f;
    }

    Rect revealRect{};
    if (showRevealBtn) {
        const float btnSize = m.iconSize + 4.0f;
        rightX -= btnSize;
        revealRect = Rect(rightX, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        rightX -= 4.0f;
    }

    Rect clearRect{};
    if (showClearBtn) {
        const float btnSize = m.iconSize + 4.0f;
        rightX -= btnSize;
        clearRect = Rect(rightX, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        rightX -= 4.0f;
    }

    const float availW = std::max(0.0f, rightX - leftX);
    return InputBoxRects{
        .boxRect = boxRect,
        .textRect = Rect(leftX, boxRect.y, availW, boxRect.h),
        .clearRect = clearRect,
        .revealRect = revealRect
    };
}

size_t SnapToUtf8CodepointBoundary(std::string_view text, size_t idx) noexcept {
    idx = std::min(idx, text.size());
    while (idx > 0 && idx < text.size() && (static_cast<unsigned char>(text[idx]) & 0xC0) == 0x80) {
        --idx;
    }
    return idx;
}

} // namespace

Response InputField::Run(
    Context& ctx,
    std::string* stringValue,
    std::string_view displayFallback,
    const InputFieldOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
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
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);
    optsCopy.placeholder = ctx.AllocText(opts.placeholder);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    optsCopy.prefix = ctx.AllocAffix(opts.prefix);
    optsCopy.suffix = ctx.AllocAffix(opts.suffix);
    optsCopy.valueFormat = ctx.AllocText(opts.valueFormat);

    std::string_view valStr = (stringValue != nullptr) ? std::string_view(*stringValue) : displayFallback;

    auto& state = ctx.GetOrCreateState<InputFieldState>(id);
    if (!res.pressed && !mouseJustPressed) {
        state.actionBtnDown = false;
    }

    const UiTheme& theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();
    const Rect refBounds = (res.rect.w > 0.0f && res.rect.h > 0.0f)
        ? res.rect
        : Rect({0.0f, 0.0f}, InputField::Measure(valStr, optsCopy, activeSize, canvas, theme));

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
            if (stringValue != nullptr) {
                stringValue->clear();
                valStr = *stringValue;
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

    size_t cursor = 0;
    size_t selAnchor = 0;
    std::string_view displayText = valStr;

    if (isFocused || ctx.HasActiveTextEdit(id)) {
        std::string_view currentEditText = ctx.EnsureTextEdit(id, valStr, /*multiline=*/false, opts.readOnly);
        showClearBtn = opts.clearable && !currentEditText.empty() && !effectiveDisabled && !opts.readOnly;
        rects = ResolveInputBoxRects(refBounds, optsCopy, activeSize, canvas, theme, showClearBtn, showRevealBtn);
        clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);

        const Rect textRect = rects.textRect;
        const float inputFont = theme.ResolveFontSize(optsCopy.valueFormat, TextRole::LabelMedium, activeSize);

        const bool isMasked = opts.password && !state.passwordRevealed;
        std::string maskedEditBuf;
        std::string_view hitTestText = currentEditText;
        if (isMasked && !currentEditText.empty()) {
            maskedEditBuf.assign(currentEditText.size(), '*');
            hitTestText = maskedEditBuf;
        }

        if ((mouseJustPressed || res.pressed) && !state.actionBtnDown) {
            const float localX = (mousePos.x - textRect.x) + state.scrollX;
            size_t hitIdx = TextUtil::HitTestIndex(hitTestText, localX, inputFont, canvas);
            hitIdx = SnapToUtf8CodepointBoundary(currentEditText, hitIdx);

            if (mouseJustPressed) {
                const double now = ctx.GetCurrentTime();
                const float dist = (mousePos - state.lastClickPos).Length();
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
                        const size_t anchor = ctx.GetTextEditAnchor(id);
                        ctx.SetTextEditCursor(id, hitIdx, anchor);
                    } else {
                        ctx.SetTextEditCursor(id, hitIdx, hitIdx);
                    }
                } else if (state.clickCount == 2) {
                    size_t wStart = 0;
                    size_t wEnd = 0;
                    TextUtil::SelectWordAt(hitTestText, hitIdx, wStart, wEnd);
                    wStart = SnapToUtf8CodepointBoundary(currentEditText, wStart);
                    wEnd = SnapToUtf8CodepointBoundary(currentEditText, wEnd);
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
                    size_t wStart = 0;
                    size_t wEnd = 0;
                    TextUtil::SelectWordAt(hitTestText, hitIdx, wStart, wEnd);
                    wStart = SnapToUtf8CodepointBoundary(currentEditText, wStart);
                    wEnd = SnapToUtf8CodepointBoundary(currentEditText, wEnd);
                    if (hitIdx < state.wordDragStart) {
                        ctx.SetTextEditCursor(id, wStart, state.wordDragEnd);
                    } else {
                        ctx.SetTextEditCursor(id, wEnd, state.wordDragStart);
                    }
                } else {
                    const size_t anchor = ctx.GetTextEditAnchor(id);
                    ctx.SetTextEditCursor(id, hitIdx, anchor);
                }
            }
        }

        bool changed = false;
        bool submitted = false;
        displayText = ctx.GetOrStartTextEdit(id, valStr, cursor, selAnchor, changed, submitted, /*multiline=*/false, opts.readOnly);

        if (changed || submitted) {
            state.lastClickTime = -10.0;
            state.clickCount = 0;
        }

        if (changed && !opts.readOnly) {
            if (stringValue != nullptr) {
                *stringValue = displayText;
            }
            res.changed = true;
        }

        if (submitted) {
            res.submitted = true;
        }

        showClearBtn = opts.clearable && !displayText.empty() && !effectiveDisabled && !opts.readOnly;
        rects = ResolveInputBoxRects(refBounds, optsCopy, activeSize, canvas, theme, showClearBtn, showRevealBtn);
        clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);

        if (rects.textRect.w > 0.0f && isFocused) {
            std::string maskedDisplayBuf;
            std::string_view measureText = displayText;
            if (opts.password && !state.passwordRevealed && !displayText.empty()) {
                maskedDisplayBuf.assign(displayText.size(), '*');
                measureText = maskedDisplayBuf;
            }
            const size_t safeCursor = std::min(cursor, measureText.size());
            const float cursorX = TextUtil::MeasureWidth(measureText.substr(0, safeCursor), inputFont, canvas);
            const float fullTextW = TextUtil::MeasureWidth(measureText, inputFont, canvas);
            const float visibleW = std::max(0.0f, rects.textRect.w - 2.0f);

            if (cursorX < state.scrollX) {
                state.scrollX = cursorX;
            } else if (cursorX > state.scrollX + visibleW) {
                state.scrollX = cursorX - visibleW;
            }
            const float maxScroll = std::max(0.0f, fullTextW - visibleW);
            state.scrollX = std::clamp(state.scrollX, 0.0f, maxScroll);
        } else if (!isFocused) {
            state.scrollX = 0.0f;
        }
    } else {
        state.scrollX = 0.0f;
    }

    const size_t selStart = std::min(cursor, selAnchor);
    const size_t selEnd = std::max(cursor, selAnchor);

    std::string maskedFinalBuf;
    std::string_view renderedText = displayText;
    if (opts.password && !state.passwordRevealed && !displayText.empty()) {
        maskedFinalBuf.assign(displayText.size(), '*');
        renderedText = maskedFinalBuf;
    }

    const ControlSpec ctrl = EffectiveInputControl(optsCopy, activeSize, theme);
    Node* node = ctx.CreateNode(NodeType::InputField, id);

    auto* data = ctx.AllocPayload<InputFieldData>();
    data->opts = optsCopy;
    data->displayText = ctx.AllocString(renderedText);
    node->text = data->displayText.empty() ? optsCopy.placeholder.text : data->displayText;
    data->cursor = cursor;
    data->selStart = selStart;
    data->selEnd = selEnd;
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
        static_cast<uint8_t>(optsCopy.align),
        static_cast<uint8_t>(optsCopy.stretch),
        optsCopy.background,
        optsCopy.border,
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.minWidth.value_or(0.0f),
        optsCopy.readOnly,
        optsCopy.password,
        optsCopy.passwordReveal,
        optsCopy.clearable,
        data->displayText,
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
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<InputFieldData>();
        return InputField::Measure(
            d ? d->displayText : std::string_view{},
            d ? d->opts : InputFieldOptions{},
            n->uiSize,
            canvas,
            theme,
            availableWidth
        );
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<InputFieldData>();
        InputField::Render(
            canvas,
            n->computedRect,
            d ? d->displayText : std::string_view{},
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
            d ? d->passwordRevealed : false
        );
    };

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

Vec2 InputField::Measure(
    std::string_view /*displayText*/,
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const Vec2 boxDims = MeasureInputBox(opts, size, canvas, theme);
    const ControlSpec ctrl = EffectiveInputControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, opts.stretch, size, theme);
}

Vec2 InputField::Measure(
    std::string_view displayText,
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(displayText, opts, size, canvas, defaultTheme, availableWidth);
}

void InputField::Render(
    ICanvas& canvas,
    Rect bounds,
    std::string_view displayText,
    const InputFieldOptions& opts,
    UiSize size,
    bool hovered,
    bool focused,
    size_t cursor,
    bool disabled,
    bool cursorVisible,
    float cursorWidth,
    const UiTheme& theme,
    size_t selStart,
    size_t selEnd,
    float scrollX,
    bool showClearBtn,
    bool clearHovered,
    bool showRevealBtn,
    bool revealHovered,
    bool passwordRevealed
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const InputFieldMetrics& m = theme.GetInputField(size);
    const ControlSpec ctrl = EffectiveInputControl(opts, size, theme);
    const Rect boxRect = bounds;

    const auto& pal = theme.palette;
    Color boxBg = ctrl.HasBackground(true)
        ? ((hovered && !disabled) ? pal.surface : pal.inputBackground)
        : Color::Transparent();
    Color boxBorder;
    float strokeW = (ctrl.border.width > 0.0f) ? theme.Scale(ctrl.border.width) : m.strokeWidth;

    if (focused && !disabled) {
        boxBorder = pal.focusRing;
        if (ctrl.border.width > 0.0f || m.strokeWidth > 0.0f) strokeW += 0.5f;
    } else if (hovered && !disabled) {
        boxBorder = pal.borderHover;
    } else {
        boxBorder = pal.inputBorder;
    }

    Color textCol = opts.valueFormat.color.HasAny()
        ? theme.ResolveTextColor(opts.valueFormat, UiVariant::Default, disabled, hovered, focused)
        : (disabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary);
    Color placeholderCol = theme.ResolveTextColor(opts.placeholder, UiVariant::Secondary, disabled, hovered, focused);
    Color unitCol = pal.textSecondary;

    if (disabled) {
        boxBg = boxBg.WithAlphaF(0.40f);
        boxBorder = boxBorder.WithAlphaF(0.40f);
        unitCol = unitCol.WithAlphaF(0.40f);
    }

    if (ctrl.HasBackground(true) && ctrl.background.HasAny() && !ctrl.background.IsSurface()) {
        boxBg = ctrl.background.Resolve(hovered, focused, disabled, boxBg);
    }
    if (ctrl.border.color.HasAny()) {
        boxBorder = ctrl.border.color.Resolve(hovered, focused, disabled, boxBorder);
    }

    const float radius = ctrl.border.radius.has_value() ? theme.Scale(*ctrl.border.radius) : m.cornerRadius;
    canvas.DrawRoundRect(boxRect, radius, boxBg, boxBorder, strokeW);

    const float inputFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelMedium, size);

    float leftX = boxRect.x + m.paddingX;
    if (opts.prefix.HasContentOrReserve()) {
        const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const Color pTextCol = theme.ResolveTextColor(opts.prefix.text, UiVariant::Secondary, disabled, hovered, focused);
        const Color pIconCol = opts.prefix.text.color.HasAny() ? pTextCol : unitCol;
        const float pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, &canvas);
        TextUtil::DrawAffix(canvas, opts.prefix, Rect(leftX, boxRect.y, pW, boxRect.h), pFont, m.iconSize, pTextCol, pIconCol, 4.0f);
        const float pGap = (opts.prefix.HasIcon() && !opts.prefix.HasText()) ? 6.0f : 4.0f;
        leftX += pW + pGap;
    }

    float rightX = boxRect.x + boxRect.w - m.paddingX;

    if (opts.suffix.HasContentOrReserve()) {
        const float sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
        const Color sTextCol = theme.ResolveTextColor(opts.suffix.text, UiVariant::Secondary, disabled, hovered, focused);
        const Color sIconCol = opts.suffix.text.color.HasAny() ? sTextCol : unitCol;
        const float sW = TextUtil::MeasureAffixWidth(opts.suffix, sFont, m.iconSize, 4.0f, &canvas);
        rightX -= sW;
        TextUtil::DrawAffix(canvas, opts.suffix, Rect(rightX, boxRect.y, sW, boxRect.h), sFont, m.iconSize, sTextCol, sIconCol, 4.0f);
        rightX -= 6.0f;
    }

    if (showRevealBtn) {
        const float btnSize = m.iconSize + 4.0f;
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
        const float btnSize = m.iconSize + 4.0f;
        rightX -= btnSize;
        const Rect btnRect(rightX, boxRect.y + (boxRect.h - btnSize) * 0.5f, btnSize, btnSize);
        if (clearHovered && !disabled) {
            canvas.DrawRoundRect(btnRect, 4.0f, pal.surfaceHover, Color::Transparent(), 0.0f);
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

    const float availW = std::max(0.0f, rightX - leftX);
    const Rect textRect(leftX, boxRect.y, availW, boxRect.h);
    canvas.PushClip(textRect);

    if (focused && !disabled && selStart != selEnd && !displayText.empty()) {
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

    if (displayText.empty() && !opts.placeholder.empty() && !focused) {
        const float phFont = !opts.placeholder.size.IsAuto()
            ? theme.ResolveFontSize(opts.placeholder, TextRole::LabelMedium, size)
            : inputFont;
        canvas.DrawTextBox(opts.placeholder.text, textRect, phFont, placeholderCol, 0.0f, 0.5f);
    } else if (!displayText.empty()) {
        if (focused || scrollX > 0.0f) {
            const float fullW = std::max(textRect.w, canvas.MeasureText(displayText, inputFont).x + 8.0f);
            const Rect scrolledRect(textRect.x - scrollX, textRect.y, fullW, textRect.h);
            canvas.DrawTextBox(displayText, scrolledRect, inputFont, textCol, 0.0f, 0.5f);
        } else {
            canvas.DrawTextBox(displayText, textRect, inputFont, textCol, 0.0f, 0.5f);
        }
    }

    if (focused && !disabled && !opts.readOnly && cursorVisible) {
        const size_t safeCursor = std::min(cursor, displayText.size());
        const std::string_view sub = displayText.substr(0, safeCursor);
        const float subW = canvas.MeasureText(sub, inputFont).x;
        const float caretX = textRect.x - scrollX + subW;
        const float caretH = inputFont * 1.15f;
        const float caretY = boxRect.y + (boxRect.h - caretH) * 0.5f;
        canvas.DrawLine({caretX, caretY}, {caretX, caretY + caretH}, pal.caret, cursorWidth);
    }
    canvas.PopClip();
}

} // namespace guikit
