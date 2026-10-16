#include "guikit/widgets/InputField.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Element.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

Vec2 MeasureInputBox(
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const InputFieldMetrics& m = theme.GetInputField(size);
    const bool hasVariants = !opts.valueFormat.variants.empty();

    float boxW = m.defaultWidth;
    if (opts.stretch != StretchMode::Fill) {
        if (opts.width.has_value()) {
            boxW = theme.Scale(*opts.width);
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
            if (opts.stepButtons && !opts.readOnly) {
                totalW += (m.iconSize + 4.0f) * 2.0f + 10.0f;
            }
            boxW = totalW;
        }
    }

    if (opts.minWidth.has_value()) {
        boxW = std::max(theme.Scale(*opts.minWidth), boxW);
    } else if (hasVariants && opts.stretch != StretchMode::Fill && !opts.width.has_value()) {
        boxW = std::max(m.defaultHeight, boxW);
    } else {
        boxW = std::max(m.minWidth, boxW);
    }
    return {boxW, m.defaultHeight};
}

ElementSpec MakeInputFieldElementSpec(
    const InputFieldOptions& opts,
    UiSize size,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const InputFieldMetrics& m = theme.GetInputField(size);
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .icon = opts.icon,
        .badge = opts.badge,
        .variant = opts.variant,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = opts.width,
        .widgetSize = size,
        .labelGap = m.labelGap,
        .descGap = m.descGap,
        .inlineGap = m.paddingX,
        .divider = opts.divider,
        .dividerStyle = opts.dividerStyle,
        .stretchableControl = true,
        .stretchHeightWhenStandalone = false,
        .disabled = disabled
    };
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
    Rect stepMinusRect{};
    Rect stepPlusRect{};
};

InputBoxRects ResolveInputBoxRects(
    Rect bounds,
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    bool showClearBtn,
    bool showRevealBtn,
    bool showStepBtns = false
) noexcept {
    const InputFieldMetrics& m = theme.GetInputField(size);
    const Vec2 boxDims = MeasureInputBox(opts, size, canvas, theme);
    const ElementSpec spec = MakeInputFieldElementSpec(opts, size, opts.disabled, theme);
    const ElementGeometry geom = ElementComposer::Resolve(bounds, boxDims, spec, canvas, theme);
    const Rect boxRect = geom.controlRect;

    float leftX = boxRect.x + m.paddingX;
    if (opts.prefix.HasContentOrReserve()) {
        const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const float pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, canvas);
        const float pGap = (opts.prefix.HasIcon() && !opts.prefix.HasText()) ? 6.0f : 4.0f;
        leftX += pW + pGap;
    }

    float rightX = boxRect.x + boxRect.w - m.paddingX;

    Rect stepMinusRect{};
    Rect stepPlusRect{};
    if (showStepBtns) {
        const float btnSize = m.iconSize + 4.0f;
        const float btnY = boxRect.y + (boxRect.h - btnSize) * 0.5f;
        rightX -= btnSize;
        stepPlusRect = Rect(rightX, btnY, btnSize, btnSize);
        rightX -= (2.0f + btnSize);
        stepMinusRect = Rect(rightX, btnY, btnSize, btnSize);
        rightX -= 6.0f;
    }

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
        .revealRect = revealRect,
        .stepMinusRect = stepMinusRect,
        .stepPlusRect = stepPlusRect
    };
}

size_t SnapToUtf8CodepointBoundary(std::string_view text, size_t idx) noexcept {
    idx = std::min(idx, text.size());
    while (idx > 0 && idx < text.size() && (static_cast<unsigned char>(text[idx]) & 0xC0) == 0x80) {
        --idx;
    }
    return idx;
}

double ClampNumericValue(double val, const InputFieldOptions& opts) noexcept {
    if (opts.min.has_value()) {
        val = std::max(val, *opts.min);
    }
    if (opts.max.has_value()) {
        val = std::min(val, *opts.max);
    }
    return val;
}

void FormatNumericValue(
    char* valBuf,
    size_t bufSize,
    float* floatValue,
    double* doubleValue,
    int* intValue,
    const InputFieldOptions& opts
) noexcept {
    if (floatValue != nullptr) {
        const char* fmt = opts.valueFormat.empty() ? "%.2f" : opts.valueFormat.text.data();
        std::snprintf(valBuf, bufSize, fmt, *floatValue);
    } else if (doubleValue != nullptr) {
        const char* fmt = opts.valueFormat.empty() ? "%.2f" : opts.valueFormat.text.data();
        std::snprintf(valBuf, bufSize, fmt, *doubleValue);
    } else if (intValue != nullptr) {
        const bool customIntFmt = !opts.valueFormat.empty() &&
                                  opts.valueFormat.text != "%.2f" &&
                                  opts.valueFormat.text.find('f') == std::string_view::npos;
        std::snprintf(valBuf, bufSize, customIntFmt ? opts.valueFormat.text.data() : "%d", *intValue);
    }
}

} // namespace

Response InputField::Run(
    Context& ctx,
    std::string* stringValue,
    float* floatValue,
    double* doubleValue,
    int* intValue,
    std::string_view displayFallback,
    const InputFieldOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
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
    optsCopy.label = ctx.AllocText(opts.label);
    optsCopy.badge = ctx.AllocText(opts.badge);
    optsCopy.description = ctx.AllocText(opts.description);
    optsCopy.placeholder = ctx.AllocText(opts.placeholder);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    optsCopy.prefix = ctx.AllocAffix(opts.prefix);
    optsCopy.suffix = ctx.AllocAffix(opts.suffix);
    optsCopy.valueFormat = ctx.AllocText(opts.valueFormat);

    const bool isNumeric = (floatValue != nullptr || doubleValue != nullptr || intValue != nullptr);
    if (isNumeric && (opts.min.has_value() || opts.max.has_value())) {
        if (floatValue != nullptr) {
            *floatValue = static_cast<float>(ClampNumericValue(static_cast<double>(*floatValue), opts));
        } else if (doubleValue != nullptr) {
            *doubleValue = ClampNumericValue(*doubleValue, opts);
        } else if (intValue != nullptr) {
            *intValue = static_cast<int>( std::llround(ClampNumericValue(static_cast<double>(*intValue), opts)) );
        }
    }

    // Convert underlying data to formatted text
    char valBuf[64]{};
    std::string_view valStr;

    if (stringValue != nullptr) {
        valStr = *stringValue;
    } else if (isNumeric) {
        FormatNumericValue(valBuf, sizeof(valBuf), floatValue, doubleValue, intValue, optsCopy);
        valStr = valBuf;
    } else {
        valStr = displayFallback;
    }

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
    const bool showStepBtns = isNumeric && opts.stepButtons && !opts.readOnly && !effectiveDisabled;
    InputBoxRects rects = ResolveInputBoxRects(refBounds, optsCopy, activeSize, canvas, theme, showClearBtn, showRevealBtn, showStepBtns);

    const Vec2 mousePos = ctx.GetMousePos();
    bool revealHovered = showRevealBtn && res.hovered && rects.revealRect.Contains(mousePos);
    bool clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);
    bool stepMinusHovered = showStepBtns && res.hovered && rects.stepMinusRect.Contains(mousePos);
    bool stepPlusHovered = showStepBtns && res.hovered && rects.stepPlusRect.Contains(mousePos);

    double stepClickDelta = 0.0;
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
            rects = ResolveInputBoxRects(refBounds, optsCopy, activeSize, canvas, theme, showClearBtn, showRevealBtn, showStepBtns);
            res.changed = true;
        } else if (stepMinusHovered) {
            state.actionBtnDown = true;
            stepClickDelta = -opts.step.value_or(1.0);
        } else if (stepPlusHovered) {
            state.actionBtnDown = true;
            stepClickDelta = opts.step.value_or(1.0);
        }
    }

    // Handle numeric Up/Down key, mouse wheel, and stepper button stepping
    if (isNumeric && !opts.readOnly && !effectiveDisabled && (isFocused || stepClickDelta != 0.0)) {
        double stepDelta = stepClickDelta;
        const double stepVal = opts.step.value_or(1.0);
        if (isFocused) {
            if (ctx.ConsumeKey(Key::Up)) {
                stepDelta += stepVal;
            }
            if (ctx.ConsumeKey(Key::Down)) {
                stepDelta -= stepVal;
            }
            if (res.hovered && std::abs(ctx.GetMouseWheel().y) > 0.001f) {
                stepDelta += (ctx.GetMouseWheel().y > 0.0f ? stepVal : -stepVal);
                ctx.ConsumeMouseWheel();
            }
        }

        if (stepDelta != 0.0) {
            if (floatValue != nullptr) {
                const double nextVal = ClampNumericValue(static_cast<double>(*floatValue) + stepDelta, opts);
                *floatValue = static_cast<float>(nextVal);
            } else if (doubleValue != nullptr) {
                *doubleValue = ClampNumericValue(*doubleValue + stepDelta, opts);
            } else if (intValue != nullptr) {
                const double nextVal = ClampNumericValue(static_cast<double>(*intValue) + stepDelta, opts);
                *intValue = static_cast<int>(std::llround(nextVal));
            }
            FormatNumericValue(valBuf, sizeof(valBuf), floatValue, doubleValue, intValue, optsCopy);
            valStr = valBuf;
            ctx.SetTextEditBuffer(id, valStr);
            res.changed = true;
        }
    }

    // Active edit state integration with Context
    size_t cursor = 0;
    size_t selAnchor = 0;
    std::string_view displayText = valStr;

    if (isFocused || ctx.HasActiveTextEdit(id)) {
        std::string_view currentEditText = ctx.EnsureTextEdit(id, valStr, /*multiline=*/false, opts.readOnly);
        showClearBtn = opts.clearable && !currentEditText.empty() && !effectiveDisabled && !opts.readOnly;
        rects = ResolveInputBoxRects(refBounds, optsCopy, activeSize, canvas, theme, showClearBtn, showRevealBtn, showStepBtns);
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

        // Handle LMB click, double/triple click, Shift+click, and LMB drag selection
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
            } else if (floatValue != nullptr) {
                char* end = nullptr;
                std::string s(displayText);
                const float f = std::strtof(s.c_str(), &end);
                if (end != s.c_str()) {
                    *floatValue = static_cast<float>(ClampNumericValue(static_cast<double>(f), opts));
                }
            } else if (doubleValue != nullptr) {
                char* end = nullptr;
                std::string s(displayText);
                const double d = std::strtod(s.c_str(), &end);
                if (end != s.c_str()) {
                    *doubleValue = ClampNumericValue(d, opts);
                }
            } else if (intValue != nullptr) {
                char* end = nullptr;
                std::string s(displayText);
                const long i = std::strtol(s.c_str(), &end, 10);
                if (end != s.c_str()) {
                    const double clamped = ClampNumericValue(static_cast<double>(i), opts);
                    *intValue = static_cast<int>(std::llround(clamped));
                }
            }
            res.changed = true;
        }

        if (submitted) {
            res.submitted = true;
            if (isNumeric && !opts.readOnly) {
                FormatNumericValue(valBuf, sizeof(valBuf), floatValue, doubleValue, intValue, optsCopy);
                valStr = valBuf;
                ctx.SetTextEditBuffer(id, valStr);
                displayText = valStr;
                cursor = displayText.size();
                selAnchor = cursor;
            }
        }

        showClearBtn = opts.clearable && !displayText.empty() && !effectiveDisabled && !opts.readOnly;
        rects = ResolveInputBoxRects(refBounds, optsCopy, activeSize, canvas, theme, showClearBtn, showRevealBtn, showStepBtns);
        clearHovered = showClearBtn && res.hovered && rects.clearRect.Contains(mousePos);

        // Update horizontal scrollX to keep active cursor within visible textRect
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

    bool stepMinusDisabled = false;
    bool stepPlusDisabled = false;
    if (showStepBtns) {
        const double curNum = (floatValue != nullptr)  ? static_cast<double>(*floatValue) :
                              (doubleValue != nullptr) ? *doubleValue :
                              (intValue != nullptr)    ? static_cast<double>(*intValue) : 0.0;
        if (opts.min.has_value() && curNum <= *opts.min + 1e-6) stepMinusDisabled = true;
        if (opts.max.has_value() && curNum >= *opts.max - 1e-6) stepPlusDisabled = true;
    }

    Node* node = ctx.CreateNode(NodeType::InputField, id);
    node->text = optsCopy.label.text;

    auto* data = ctx.AllocPayload<InputFieldData>();
    data->opts = optsCopy;
    data->displayText = ctx.AllocString(renderedText);
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
    data->showStepBtns = showStepBtns;
    data->stepMinusHovered = stepMinusHovered;
    data->stepPlusHovered = stepPlusHovered;
    data->stepMinusDisabled = stepMinusDisabled;
    data->stepPlusDisabled = stepPlusDisabled;
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        opts.variant,
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        static_cast<uint8_t>(opts.stretch),
        static_cast<uint8_t>(opts.divider),
        static_cast<uint8_t>(opts.dividerStyle),
        opts.width.value_or(0.0f),
        opts.minWidth.value_or(0.0f),
        opts.readOnly,
        opts.password,
        opts.passwordReveal,
        opts.clearable,
        opts.stepButtons,
        opts.borderRadius.value_or(-1.0f),
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
        data->showStepBtns,
        data->stepMinusHovered,
        data->stepPlusHovered,
        data->stepMinusDisabled,
        data->stepPlusDisabled,
        optsCopy.prefix,
        optsCopy.suffix,
        optsCopy.placeholder,
        optsCopy.valueFormat,
        optsCopy.description,
        optsCopy.label,
        optsCopy.badge,
        optsCopy.icon
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
            d ? d->passwordRevealed : false,
            d ? d->showStepBtns : false,
            d ? d->stepMinusHovered : false,
            d ? d->stepPlusHovered : false,
            d ? d->stepMinusDisabled : false,
            d ? d->stepPlusDisabled : false
        );
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->focused = isFocused;
    node->pressed = res.pressed;

    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 InputField::Measure(
    std::string_view /*displayText*/,
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) {
    const Vec2 boxDims = MeasureInputBox(opts, size, canvas, theme);
    const ElementSpec spec = MakeInputFieldElementSpec(opts, size, opts.disabled, theme);
    return Element::Measure(boxDims, spec, canvas, theme, availableWidth);
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
    bool passwordRevealed,
    bool showStepBtns,
    bool stepMinusHovered,
    bool stepPlusHovered,
    bool stepMinusDisabled,
    bool stepPlusDisabled
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const InputFieldMetrics& m = theme.GetInputField(size);
    const Vec2 boxDims = MeasureInputBox(opts, size, &canvas, theme);
    const ElementSpec spec = MakeInputFieldElementSpec(opts, size, disabled, theme);
    const ElementGeometry geom = Element::Resolve(bounds, boxDims, spec, &canvas, theme);
    Element::RenderText(canvas, geom, spec, theme);
    const Rect boxRect = geom.controlRect;

    // Determine colors
    const auto& pal = theme.palette;
    Color boxBg = (hovered && !disabled) ? pal.surface : pal.inputBackground;
    Color boxBorder;
    float strokeW = m.strokeWidth;

    const VariantColors& v = theme.GetVariant(opts.variant);
    if (focused && !disabled) {
        boxBorder = (opts.variant == UiVariant::Default || opts.variant == UiVariant::Ghost) ? pal.focusRing : v.bg;
        strokeW += 0.5f;
    } else if (hovered && !disabled) {
        boxBorder = (opts.variant == UiVariant::Default || opts.variant == UiVariant::Ghost) ? pal.borderHover : v.bgHover;
    } else {
        if (opts.variant == UiVariant::Default) {
            boxBorder = pal.inputBorder;
        } else if (opts.variant == UiVariant::Ghost) {
            boxBorder = pal.border;
        } else {
            boxBorder = (v.border.a > 0) ? v.border : v.bg;
        }
    }

    Color textCol = opts.valueFormat.variant.has_value()
        ? theme.ResolveTextColor(opts.valueFormat, UiVariant::Default, disabled)
        : (disabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary);
    Color placeholderCol = theme.ResolveTextColor(opts.placeholder, UiVariant::Secondary, disabled);
    Color unitCol = pal.textSecondary;

    if (disabled) {
        boxBg = boxBg.WithAlphaF(0.40f);
        boxBorder = boxBorder.WithAlphaF(0.40f);
        unitCol = unitCol.WithAlphaF(0.40f);
    }

    // Draw box background and border
    const float radius = opts.borderRadius.has_value() ? theme.Scale(*opts.borderRadius) : m.cornerRadius;
    canvas.DrawRoundRect(boxRect, radius, boxBg, boxBorder, strokeW);

    const float inputFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelMedium, size);

    // Left elements (prefix icon and/or text)
    float leftX = boxRect.x + m.paddingX;
    if (opts.prefix.HasContentOrReserve()) {
        const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const Color pTextCol = theme.ResolveTextColor(opts.prefix.text, UiVariant::Secondary, disabled);
        const Color pIconCol = opts.prefix.text.variant.has_value() ? pTextCol : unitCol;
        const float pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, &canvas);
        TextUtil::DrawAffix(canvas, opts.prefix, Rect(leftX, boxRect.y, pW, boxRect.h), pFont, m.iconSize, pTextCol, pIconCol, 4.0f);
        const float pGap = (opts.prefix.HasIcon() && !opts.prefix.HasText()) ? 6.0f : 4.0f;
        leftX += pW + pGap;
    }

    // Right elements (stepper buttons, suffix text/icon, password reveal button, clear button)
    float rightX = boxRect.x + boxRect.w - m.paddingX;
    if (showStepBtns) {
        const float btnSize = m.iconSize + 4.0f;
        const float btnY = boxRect.y + (boxRect.h - btnSize) * 0.5f;
        const float iconSz = std::max(8.0f, m.iconSize - 2.0f);

        // '+' button on far right
        rightX -= btnSize;
        const Rect plusBtnRect(rightX, btnY, btnSize, btnSize);
        if (stepPlusHovered && !disabled && !stepPlusDisabled) {
            canvas.DrawRoundRect(plusBtnRect, 4.0f, pal.surfaceHover, Color::Transparent(), 0.0f);
        }
        const Rect plusIcRect(
            rightX + (btnSize - iconSz) * 0.5f,
            boxRect.y + (boxRect.h - iconSz) * 0.5f,
            iconSz,
            iconSz
        );
        const Color plusCol = (disabled || stepPlusDisabled)
            ? unitCol.WithAlphaF(0.35f)
            : (stepPlusHovered ? pal.textPrimary : unitCol);
        Icon::Plus().Draw(canvas, plusIcRect, plusCol);

        // '-' button to the left of '+'
        rightX -= (2.0f + btnSize);
        const Rect minusBtnRect(rightX, btnY, btnSize, btnSize);
        if (stepMinusHovered && !disabled && !stepMinusDisabled) {
            canvas.DrawRoundRect(minusBtnRect, 4.0f, pal.surfaceHover, Color::Transparent(), 0.0f);
        }
        const Rect minusIcRect(
            rightX + (btnSize - iconSz) * 0.5f,
            boxRect.y + (boxRect.h - iconSz) * 0.5f,
            iconSz,
            iconSz
        );
        const Color minusCol = (disabled || stepMinusDisabled)
            ? unitCol.WithAlphaF(0.35f)
            : (stepMinusHovered ? pal.textPrimary : unitCol);
        Icon::Minus().Draw(canvas, minusIcRect, minusCol);

        // Subtle vertical separator before stepper buttons
        const float sepX = rightX - 3.0f;
        canvas.DrawLine(
            {sepX, boxRect.y + 5.0f},
            {sepX, boxRect.Bottom() - 5.0f},
            disabled ? pal.border.WithAlphaF(0.25f) : pal.border.WithAlphaF(0.65f),
            1.0f
        );
        rightX -= 6.0f;
    }

    if (opts.suffix.HasContentOrReserve()) {
        const float sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
        const Color sTextCol = theme.ResolveTextColor(opts.suffix.text, UiVariant::Secondary, disabled);
        const Color sIconCol = opts.suffix.text.variant.has_value() ? sTextCol : unitCol;
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

    // Text / placeholder / selection / cursor in middle
    const float availW = std::max(0.0f, rightX - leftX);
    const Rect textRect(leftX, boxRect.y, availW, boxRect.h);
    canvas.PushClip(textRect);

    // Selection highlight
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
        const float phFont = opts.placeholder.role.has_value()
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
