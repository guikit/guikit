#include "guikit/widgets/SpinBox.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

ControlSpec EffectiveSpinBoxControl(const SpinBoxOptions& opts, UiSize size, const UiTheme& theme) noexcept {
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

Vec2 MeasureSpinBox(
    const SpinBoxOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const InputFieldMetrics& m = theme.GetInputField(size);
    const ControlSpec ctrl = EffectiveSpinBoxControl(opts, size, theme);
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
            if (opts.stepButtons && !opts.readOnly) {
                totalW += (m.iconSize + 4.0f) * 2.0f + 10.0f;
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

struct SpinBoxState {
    float scrollX{0.0f};
    double lastClickTime{-10.0};
    Vec2 lastClickPos{-1000.0f, -1000.0f};
    int clickCount{0};
    bool draggingWord{false};
    size_t wordDragStart{0};
    size_t wordDragEnd{0};
    bool actionBtnDown{false};
    bool wasFocusedOnPress{false};
    bool scrubbing{false};
    Vec2 scrubStartPos{};
    double scrubStartValue{0.0};
};

struct SpinBoxRects {
    Rect boxRect{};
    Rect textRect{};
    Rect stepMinusRect{};
    Rect stepPlusRect{};
};

SpinBoxRects ResolveSpinBoxRects(
    Rect bounds,
    const SpinBoxOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    bool showStepBtns
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

    const float availW = std::max(0.0f, rightX - leftX);
    return SpinBoxRects{
        .boxRect = boxRect,
        .textRect = Rect(leftX, boxRect.y, availW, boxRect.h),
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

double ClampNumericValue(double val, const SpinBoxOptions& opts) noexcept {
    if (opts.min.has_value()) {
        val = std::max(val, *opts.min);
    }
    if (opts.max.has_value()) {
        val = std::min(val, *opts.max);
    }
    return val;
}

double ReadNumericValue(float* floatValue, double* doubleValue, int* intValue) noexcept {
    if (floatValue != nullptr) return static_cast<double>(*floatValue);
    if (doubleValue != nullptr) return *doubleValue;
    if (intValue != nullptr) return static_cast<double>(*intValue);
    return 0.0;
}

void WriteNumericValue(
    double nextVal,
    float* floatValue,
    double* doubleValue,
    int* intValue,
    const SpinBoxOptions& opts
) noexcept {
    const double clamped = ClampNumericValue(nextVal, opts);
    if (floatValue != nullptr) {
        *floatValue = static_cast<float>(clamped);
    } else if (doubleValue != nullptr) {
        *doubleValue = clamped;
    } else if (intValue != nullptr) {
        *intValue = static_cast<int>(std::llround(clamped));
    }
}

void FormatNumericValue(
    char* valBuf,
    size_t bufSize,
    float* floatValue,
    double* doubleValue,
    int* intValue,
    const SpinBoxOptions& opts
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

Response SpinBox::Run(
    Context& ctx,
    float* floatValue,
    double* doubleValue,
    int* intValue,
    const SpinBoxOptions& opts,
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

    SpinBoxOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    optsCopy.prefix = ctx.AllocAffix(opts.prefix);
    optsCopy.suffix = ctx.AllocAffix(opts.suffix);
    optsCopy.valueFormat = ctx.AllocText(opts.valueFormat);

    if (opts.min.has_value() || opts.max.has_value()) {
        WriteNumericValue(ReadNumericValue(floatValue, doubleValue, intValue), floatValue, doubleValue, intValue, opts);
    }

    char valBuf[64]{};
    FormatNumericValue(valBuf, sizeof(valBuf), floatValue, doubleValue, intValue, optsCopy);
    std::string_view valStr = valBuf;

    auto& state = ctx.GetOrCreateState<SpinBoxState>(id);
    if (!res.pressed && !mouseJustPressed) {
        state.actionBtnDown = false;
        state.scrubbing = false;
    }

    const UiTheme& theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();
    const Rect refBounds = (res.rect.w > 0.0f && res.rect.h > 0.0f)
        ? res.rect
        : Rect({0.0f, 0.0f}, SpinBox::Measure(valStr, optsCopy, activeSize, canvas, theme));

    const bool showStepBtns = opts.stepButtons && !opts.readOnly && !effectiveDisabled;
    SpinBoxRects rects = ResolveSpinBoxRects(refBounds, optsCopy, activeSize, canvas, theme, showStepBtns);

    const Vec2 mousePos = ctx.GetMousePos();
    const bool stepMinusHovered = showStepBtns && res.hovered && rects.stepMinusRect.Contains(mousePos);
    const bool stepPlusHovered = showStepBtns && res.hovered && rects.stepPlusRect.Contains(mousePos);

    double stepClickDelta = 0.0;
    const double stepVal = opts.step.value_or(1.0);
    if (mouseJustPressed) {
        state.wasFocusedOnPress = wasFocused;
        state.scrubbing = false;
        state.scrubStartPos = mousePos;
        state.scrubStartValue = ReadNumericValue(floatValue, doubleValue, intValue);

        if (stepMinusHovered) {
            state.actionBtnDown = true;
            stepClickDelta = -stepVal;
        } else if (stepPlusHovered) {
            state.actionBtnDown = true;
            stepClickDelta = stepVal;
        }
    }

    // Handle drag-scrub when dragging horizontally before entering active text selection
    if (opts.dragScrub && !opts.readOnly && !effectiveDisabled && res.pressed && !state.actionBtnDown && !state.wasFocusedOnPress) {
        const float dx = mousePos.x - state.scrubStartPos.x;
        if (state.scrubbing || std::abs(dx) > 5.0f) {
            state.scrubbing = true;
            const auto mods = ctx.GetKeyModifiers();
            const double mult = mods.shift ? 0.1 : (mods.ctrl ? 10.0 : 1.0);
            const double effectiveStep = stepVal * mult;
            const double stepsMoved = (intValue != nullptr)
                ? std::round(static_cast<double>(dx) / 6.0)
                : (static_cast<double>(dx) / 6.0);
            double nextVal = state.scrubStartValue + stepsMoved * effectiveStep;
            if (opts.step.has_value() && !mods.shift) {
                nextVal = std::round((nextVal - state.scrubStartValue) / effectiveStep) * effectiveStep + state.scrubStartValue;
            }
            const double prevVal = ReadNumericValue(floatValue, doubleValue, intValue);
            WriteNumericValue(nextVal, floatValue, doubleValue, intValue, opts);
            const double updatedVal = ReadNumericValue(floatValue, doubleValue, intValue);
            if (std::abs(updatedVal - prevVal) > 1e-7) {
                FormatNumericValue(valBuf, sizeof(valBuf), floatValue, doubleValue, intValue, optsCopy);
                valStr = valBuf;
                ctx.SetTextEditBuffer(id, valStr);
                res.changed = true;
            }
        }
    }

    // Handle Up/Down key, mouse wheel, and stepper button stepping
    if (!opts.readOnly && !effectiveDisabled && (isFocused || res.hovered || stepClickDelta != 0.0)) {
        double stepDelta = stepClickDelta;
        if (isFocused) {
            if (ctx.ConsumeKey(Key::Up)) {
                stepDelta += stepVal;
            }
            if (ctx.ConsumeKey(Key::Down)) {
                stepDelta -= stepVal;
            }
        }
        if ((isFocused || res.hovered) && std::abs(ctx.GetMouseWheel().y) > 0.001f) {
            stepDelta += (ctx.GetMouseWheel().y > 0.0f ? stepVal : -stepVal);
            ctx.ConsumeMouseWheel();
        }

        if (stepDelta != 0.0) {
            const double curVal = ReadNumericValue(floatValue, doubleValue, intValue);
            WriteNumericValue(curVal + stepDelta, floatValue, doubleValue, intValue, opts);
            FormatNumericValue(valBuf, sizeof(valBuf), floatValue, doubleValue, intValue, optsCopy);
            valStr = valBuf;
            ctx.SetTextEditBuffer(id, valStr);
            res.changed = true;
        }
    }

    size_t cursor = 0;
    size_t selAnchor = 0;
    std::string_view displayText = valStr;

    if (isFocused || ctx.HasActiveTextEdit(id)) {
        std::string_view currentEditText = ctx.EnsureTextEdit(id, valStr, /*multiline=*/false, opts.readOnly);
        const Rect textRect = rects.textRect;
        const float inputFont = theme.ResolveFontSize(optsCopy.valueFormat, TextRole::LabelMedium, activeSize);

        if ((mouseJustPressed || res.pressed) && !state.actionBtnDown && !state.scrubbing) {
            const float localX = (mousePos.x - textRect.x) + state.scrollX;
            size_t hitIdx = TextUtil::HitTestIndex(currentEditText, localX, inputFont, canvas);
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
                    TextUtil::SelectWordAt(currentEditText, hitIdx, wStart, wEnd);
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
                    TextUtil::SelectWordAt(currentEditText, hitIdx, wStart, wEnd);
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
            if (floatValue != nullptr) {
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
            if (!opts.readOnly) {
                FormatNumericValue(valBuf, sizeof(valBuf), floatValue, doubleValue, intValue, optsCopy);
                valStr = valBuf;
                ctx.SetTextEditBuffer(id, valStr);
                displayText = valStr;
                cursor = displayText.size();
                selAnchor = cursor;
            }
        }

        if (rects.textRect.w > 0.0f && isFocused) {
            const size_t safeCursor = std::min(cursor, displayText.size());
            const float cursorX = TextUtil::MeasureWidth(displayText.substr(0, safeCursor), inputFont, canvas);
            const float fullTextW = TextUtil::MeasureWidth(displayText, inputFont, canvas);
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

    bool stepMinusDisabled = false;
    bool stepPlusDisabled = false;
    const double curNum = ReadNumericValue(floatValue, doubleValue, intValue);
    if (showStepBtns) {
        if (opts.min.has_value() && curNum <= *opts.min + 1e-6) stepMinusDisabled = true;
        if (opts.max.has_value() && curNum >= *opts.max - 1e-6) stepPlusDisabled = true;
    }

    float fillRatio = 0.0f;
    if (opts.showFillBar && opts.min.has_value() && opts.max.has_value() && (*opts.max > *opts.min)) {
        fillRatio = static_cast<float>(std::clamp((curNum - *opts.min) / (*opts.max - *opts.min), 0.0, 1.0));
    }

    const ControlSpec ctrl = EffectiveSpinBoxControl(optsCopy, activeSize, theme);
    Node* node = ctx.CreateNode(NodeType::SpinBox, id);

    auto* data = ctx.AllocPayload<SpinBoxData>();
    data->opts = optsCopy;
    data->displayText = ctx.AllocString(displayText);
    node->text = data->displayText;
    data->fillRatio = fillRatio;
    data->cursor = cursor;
    data->selStart = selStart;
    data->selEnd = selEnd;
    data->scrollX = state.scrollX;
    data->cursorVisible = isFocused && !opts.readOnly && !state.scrubbing && ctx.IsCursorVisible(opts.cursorMode);
    data->cursorWidth = ctx.GetCursorConfig().width;
    data->showStepBtns = showStepBtns;
    data->stepMinusHovered = stepMinusHovered;
    data->stepPlusHovered = stepPlusHovered;
    data->stepMinusDisabled = stepMinusDisabled;
    data->stepPlusDisabled = stepPlusDisabled;
    data->scrubbing = state.scrubbing;
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        static_cast<uint8_t>(optsCopy.align),
        static_cast<uint8_t>(optsCopy.stretch),
        optsCopy.color,
        optsCopy.background,
        optsCopy.border,
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.minWidth.value_or(0.0f),
        optsCopy.readOnly,
        optsCopy.stepButtons,
        optsCopy.showFillBar,
        data->displayText,
        data->fillRatio,
        data->cursor,
        data->selStart,
        data->selEnd,
        data->scrollX,
        data->showStepBtns,
        data->stepMinusHovered,
        data->stepPlusHovered,
        data->stepMinusDisabled,
        data->stepPlusDisabled,
        data->scrubbing,
        optsCopy.prefix,
        optsCopy.suffix,
        optsCopy.valueFormat
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<SpinBoxData>();
        return SpinBox::Measure(
            d ? d->displayText : std::string_view{},
            d ? d->opts : SpinBoxOptions{},
            n->uiSize,
            canvas,
            theme,
            availableWidth
        );
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<SpinBoxData>();
        SpinBox::Render(
            canvas,
            n->computedRect,
            d ? d->displayText : std::string_view{},
            d ? d->opts : SpinBoxOptions{},
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
            d ? d->showStepBtns : false,
            d ? d->stepMinusHovered : false,
            d ? d->stepPlusHovered : false,
            d ? d->stepMinusDisabled : false,
            d ? d->stepPlusDisabled : false,
            d ? d->fillRatio : 0.0f,
            d ? d->scrubbing : false
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

Vec2 SpinBox::Measure(
    std::string_view /*displayText*/,
    const SpinBoxOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const Vec2 boxDims = MeasureSpinBox(opts, size, canvas, theme);
    const ControlSpec ctrl = EffectiveSpinBoxControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, opts.stretch, size, theme);
}

Vec2 SpinBox::Measure(
    std::string_view displayText,
    const SpinBoxOptions& opts,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(displayText, opts, size, canvas, defaultTheme, availableWidth);
}

void SpinBox::Render(
    ICanvas& canvas,
    Rect bounds,
    std::string_view displayText,
    const SpinBoxOptions& opts,
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
    bool showStepBtns,
    bool stepMinusHovered,
    bool stepPlusHovered,
    bool stepMinusDisabled,
    bool stepPlusDisabled,
    float fillRatio,
    bool scrubbing
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const InputFieldMetrics& m = theme.GetInputField(size);
    const ControlSpec ctrl = EffectiveSpinBoxControl(opts, size, theme);
    const Rect boxRect = bounds;

    const auto& pal = theme.palette;
    Color boxBg = ctrl.HasBackground(true)
        ? ((hovered && !disabled) ? pal.surface : pal.inputBackground)
        : Color::Transparent();
    Color boxBorder;
    float strokeW = (ctrl.border.width > 0.0f) ? theme.Scale(ctrl.border.width) : m.strokeWidth;

    const VariantColors& primaryVar = theme.GetVariant(UiVariant::Primary);
    const Color accentCol = opts.color.HasAny()
        ? opts.color.Resolve(hovered && !disabled, (focused || scrubbing) && !disabled, disabled, primaryVar.bg, primaryVar.bgHover, primaryVar.bgActive, primaryVar.bg)
        : primaryVar.bg;

    if ((focused || scrubbing) && !disabled) {
        boxBorder = opts.color.HasAny() ? accentCol : pal.focusRing;
        if (ctrl.border.width > 0.0f || m.strokeWidth > 0.0f) strokeW += 0.5f;
    } else if (hovered && !disabled) {
        boxBorder = pal.borderHover;
    } else {
        boxBorder = pal.inputBorder;
    }

    Color textCol = opts.valueFormat.color.HasAny()
        ? theme.ResolveTextColor(opts.valueFormat, UiVariant::Default, disabled, hovered, focused)
        : (disabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary);
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

    // Optional fill bar along the bottom inside the SpinBox
    if (opts.showFillBar && fillRatio > 0.001f) {
        const float barH = std::max(2.0f, theme.Scale(3.0f));
        const float padX = std::max(2.0f, radius * 0.35f);
        const float maxBarW = std::max(0.0f, boxRect.w - padX * 2.0f);
        const float barW = maxBarW * std::clamp(fillRatio, 0.0f, 1.0f);
        if (barW > 1.0f) {
            const Rect barRect(boxRect.x + padX, boxRect.Bottom() - barH - 1.5f, barW, barH);
            const Color barCol = disabled ? accentCol.WithAlphaF(0.35f) : accentCol.WithAlphaF(0.85f);
            canvas.DrawRoundRect(barRect, barH * 0.5f, barCol, Color::Transparent(), 0.0f);
        }
    }

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
            : (stepPlusHovered ? (opts.color.HasAny() ? accentCol : pal.textPrimary) : unitCol);
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
            : (stepMinusHovered ? (opts.color.HasAny() ? accentCol : pal.textPrimary) : unitCol);
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
        const Color sTextCol = theme.ResolveTextColor(opts.suffix.text, UiVariant::Secondary, disabled, hovered, focused);
        const Color sIconCol = opts.suffix.text.color.HasAny() ? sTextCol : unitCol;
        const float sW = TextUtil::MeasureAffixWidth(opts.suffix, sFont, m.iconSize, 4.0f, &canvas);
        rightX -= sW;
        TextUtil::DrawAffix(canvas, opts.suffix, Rect(rightX, boxRect.y, sW, boxRect.h), sFont, m.iconSize, sTextCol, sIconCol, 4.0f);
        rightX -= 6.0f;
    }

    const float availW = std::max(0.0f, rightX - leftX);
    const Rect textRect(leftX, boxRect.y, availW, boxRect.h);
    canvas.PushClip(textRect);

    if (focused && !disabled && !scrubbing && selStart != selEnd && !displayText.empty()) {
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

    if (!displayText.empty()) {
        if (focused || scrollX > 0.0f) {
            const float fullW = std::max(textRect.w, canvas.MeasureText(displayText, inputFont).x + 8.0f);
            const Rect scrolledRect(textRect.x - scrollX, textRect.y, fullW, textRect.h);
            canvas.DrawTextBox(displayText, scrolledRect, inputFont, textCol, 0.0f, 0.5f);
        } else {
            canvas.DrawTextBox(displayText, textRect, inputFont, textCol, 0.0f, 0.5f);
        }
    }

    if (focused && !disabled && !opts.readOnly && !scrubbing && cursorVisible) {
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
