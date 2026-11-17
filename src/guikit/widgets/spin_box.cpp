#include "guikit/widgets/spin_box.hpp"
#include <cmath>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/format.h"
#include "keel/math.h"
#include "keel/number.h"
#include "keel/string.h"

namespace guikit {

namespace {

ControlSpec EffectiveSpinBoxControl(const SpinBoxOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const InputFieldMetrics& m = theme.GetInputField(size);
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

SpinBoxStepStyle ResolveEffectiveStepStyle(const SpinBoxOptions& opts) noexcept {
    if (!opts.stepButtons || opts.readOnly) {
        return SpinBoxStepStyle::None;
    }
    return opts.stepStyle;
}

struct InlineStepMetrics {
    float btnSize{0.0f};
    float btnGap{0.0f};
    float padX{0.0f};
    float sectionWidth{0.0f};
};

InlineStepMetrics ComputeInlineStepMetrics(const InputFieldMetrics& m, const UiTheme& theme) noexcept {
    InlineStepMetrics sm{};
    sm.btnSize = m.iconSize + theme.Scale(4.0f);
    sm.btnGap = theme.Scale(2.0f);
    sm.padX = keel::max(theme.Scale(6.0f), std::round(m.paddingX * 0.55f));
    sm.sectionWidth = sm.padX * 2.0f + sm.btnSize * 2.0f + sm.btnGap;
    return sm;
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
    const SizeMode effWMode = opts.width.ResolveMode(SizeMode::Stretch);
    const SpinBoxStepStyle effStyle = ResolveEffectiveStepStyle(opts);
    const float boxH = ctrl.height.IsFixed() ? theme.Scale(ctrl.height.Dp()) : m.defaultHeight;
    const float extGap = theme.Scale(4.0f);
    const float extBtnsW = (effStyle == SpinBoxStepStyle::Detached || effStyle == SpinBoxStepStyle::Split)
        ? (boxH + extGap) * 2.0f
        : 0.0f;

    const bool hasExplicitDpW = ctrl.width.IsFixed();
    float boxW = m.defaultWidth + extBtnsW;
    if (hasExplicitDpW) {
        boxW = theme.Scale(ctrl.width.Dp());
    } else if (effWMode != SizeMode::Stretch && hasVariants) {
            const float inputFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelMedium, size);
            float contentW = 0.0f;
            for (keel::StringRef v : opts.valueFormat.variants) {
                if (!v.empty()) {
                    contentW = keel::max(contentW, TextUtil::MeasureWidth(v, inputFont, canvas));
                }
            }

            const float affixInnerGap = theme.Scale(4.0f);
            float totalW = m.paddingX * 2.0f + contentW + affixInnerGap;
            if (opts.prefix.HasContentOrReserve()) {
                const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
                const float pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, affixInnerGap, canvas);
                const float pGap = theme.Scale((opts.prefix.HasIcon() && !opts.prefix.HasText()) ? 6.0f : 4.0f);
                totalW += pW + pGap;
            }
            if (opts.suffix.HasContentOrReserve()) {
                const float sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
                const float sW = TextUtil::MeasureAffixWidth(opts.suffix, sFont, m.iconSize, affixInnerGap, canvas);
                totalW += sW + theme.Scale(6.0f);
            }
            if (effStyle == SpinBoxStepStyle::Inline) {
                totalW += ComputeInlineStepMetrics(m, theme).sectionWidth;
            } else if (effStyle == SpinBoxStepStyle::Detached || effStyle == SpinBoxStepStyle::Split) {
                totalW += extBtnsW;
            }
            boxW = totalW;
    }

    if (ctrl.minWidth.has_value()) {
        boxW = keel::max(theme.Scale(*ctrl.minWidth), boxW);
    } else if (hasExplicitDpW || (hasVariants && effWMode != SizeMode::Stretch)) {
        boxW = keel::max(boxH + extBtnsW, boxW);
    } else {
        boxW = keel::max(m.minWidth + extBtnsW, boxW);
    }
    return {boxW, boxH};
}

struct SpinBoxState {
    float scrollX{0.0f};
    double lastClickTime{-10.0};
    Vec2 lastClickPos{-1000.0f, -1000.0f};
    keel::i32 clickCount{0};
    bool draggingWord{false};
    keel::usize wordDragStart{0};
    keel::usize wordDragEnd{0};
    bool actionBtnDown{false};
    bool minusBtnDown{false};
    bool plusBtnDown{false};
    bool wasFocusedOnPress{false};
    double repeatStartTime{0.0};
    double lastRepeatTime{0.0};
};

struct SpinBoxRects {
    Rect boxRect{};
    Rect prefixRect{};
    Rect textRect{};
    Rect suffixRect{};
    Rect stepMinusRect{};
    Rect stepPlusRect{};
    float stepSepX{0.0f};
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
    const SpinBoxStepStyle effStyle = showStepBtns ? ResolveEffectiveStepStyle(opts) : SpinBoxStepStyle::None;
    const float extGap = theme.Scale(4.0f);
    const float affixInnerGap = theme.Scale(4.0f);

    Rect boxRect = bounds;
    Rect prefixRect{};
    Rect suffixRect{};
    Rect stepMinusRect{};
    Rect stepPlusRect{};
    float stepSepX = 0.0f;

    if (effStyle == SpinBoxStepStyle::Detached) {
        const float btnH = bounds.h;
        const float btnW = keel::min(btnH, keel::max(0.0f, (bounds.w - extGap * 2.0f) * 0.30f));
        stepPlusRect = Rect(bounds.Right() - btnW, bounds.y, btnW, btnH);
        stepMinusRect = Rect(stepPlusRect.x - extGap - btnW, bounds.y, btnW, btnH);
        boxRect = Rect(bounds.x, bounds.y, keel::max(0.0f, stepMinusRect.x - extGap - bounds.x), bounds.h);
    } else if (effStyle == SpinBoxStepStyle::Split) {
        const float btnH = bounds.h;
        const float btnW = keel::min(btnH, keel::max(0.0f, (bounds.w - extGap * 2.0f) * 0.30f));
        stepMinusRect = Rect(bounds.x, bounds.y, btnW, btnH);
        stepPlusRect = Rect(bounds.Right() - btnW, bounds.y, btnW, btnH);
        const float boxX = stepMinusRect.Right() + extGap;
        boxRect = Rect(boxX, bounds.y, keel::max(0.0f, stepPlusRect.x - extGap - boxX), bounds.h);
    }

    float leftX = boxRect.x + m.paddingX;
    if (opts.prefix.HasContentOrReserve()) {
        const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const float pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, affixInnerGap, canvas);
        prefixRect = Rect(leftX, boxRect.y, pW, boxRect.h);
        const float pGap = theme.Scale((opts.prefix.HasIcon() && !opts.prefix.HasText()) ? 6.0f : 4.0f);
        leftX += pW + pGap;
    }

    float rightX = boxRect.Right() - m.paddingX;
    if (effStyle == SpinBoxStepStyle::Inline) {
        const InlineStepMetrics sm = ComputeInlineStepMetrics(m, theme);
        const float btnY = boxRect.y + (boxRect.h - sm.btnSize) * 0.5f;
        const float plusX = boxRect.Right() - sm.padX - sm.btnSize;
        const float minusX = plusX - sm.btnGap - sm.btnSize;
        stepPlusRect = Rect(plusX, btnY, sm.btnSize, sm.btnSize);
        stepMinusRect = Rect(minusX, btnY, sm.btnSize, sm.btnSize);
        stepSepX = minusX - sm.padX;
        rightX = stepSepX - m.paddingX;
    }

    if (opts.suffix.HasContentOrReserve()) {
        const float sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
        const float sW = TextUtil::MeasureAffixWidth(opts.suffix, sFont, m.iconSize, affixInnerGap, canvas);
        rightX -= sW;
        suffixRect = Rect(rightX, boxRect.y, sW, boxRect.h);
        rightX -= theme.Scale(6.0f);
    }

    const float availW = keel::max(0.0f, rightX - leftX);
    return SpinBoxRects{
        .boxRect = boxRect,
        .prefixRect = prefixRect,
        .textRect = Rect(leftX, boxRect.y, availW, boxRect.h),
        .suffixRect = suffixRect,
        .stepMinusRect = stepMinusRect,
        .stepPlusRect = stepPlusRect,
        .stepSepX = stepSepX
    };
}

keel::usize SnapToUtf8CodepointBoundary(keel::StringRef text, keel::usize idx) noexcept {
    idx = keel::min(idx, text.size());
    while (idx > 0 && idx < text.size() && (static_cast<keel::u8>(text[idx]) & 0xC0) == 0x80) {
        --idx;
    }
    return idx;
}

double ClampNumericValue(double val, const SpinBoxOptions& opts) noexcept {
    if (opts.min.has_value()) {
        val = keel::max(val, *opts.min);
    }
    if (opts.max.has_value()) {
        val = keel::min(val, *opts.max);
    }
    return val;
}

double ReadNumericValue(float* floatValue, double* doubleValue, keel::i64* intValue) noexcept {
    if (floatValue != nullptr) return static_cast<double>(*floatValue);
    if (doubleValue != nullptr) return *doubleValue;
    if (intValue != nullptr) return static_cast<double>(*intValue);
    return 0.0;
}

void WriteNumericValue(
    double nextVal,
    float* floatValue,
    double* doubleValue,
    keel::i64* intValue,
    const SpinBoxOptions& opts
) noexcept {
    const double clamped = ClampNumericValue(nextVal, opts);
    if (floatValue != nullptr) {
        *floatValue = static_cast<float>(clamped);
    } else if (doubleValue != nullptr) {
        *doubleValue = clamped;
    } else if (intValue != nullptr) {
        *intValue = keel::round_i64(clamped);
    }
}

keel::StringRef ExtractNumericPrefix(keel::StringRef text, bool allowFloat) noexcept {
    keel::usize start = 0;
    while (start < text.size() && (text[start] == ' ' || text[start] == '\t')) {
        ++start;
    }
    keel::usize i = start;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
        ++i;
    }
    bool hasDigits = false;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
        hasDigits = true;
        ++i;
    }
    if (allowFloat) {
        if (i < text.size() && text[i] == '.') {
            ++i;
            while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
                hasDigits = true;
                ++i;
            }
        }
        if (hasDigits && i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
            keel::usize expPos = i + 1;
            if (expPos < text.size() && (text[expPos] == '+' || text[expPos] == '-')) {
                ++expPos;
            }
            if (expPos < text.size() && text[expPos] >= '0' && text[expPos] <= '9') {
                i = expPos + 1;
                while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
                    ++i;
                }
            }
        }
    }
    return hasDigits ? text.substr(start, i - start) : keel::StringRef{};
}

void FormatNumericValue(
    char* valBuf,
    keel::usize bufSize,
    float* floatValue,
    double* doubleValue,
    keel::i64* intValue,
    const SpinBoxOptions& opts
) noexcept {
    const keel::StringRef rawFmt = opts.valueFormat.text;
    if (floatValue != nullptr || doubleValue != nullptr) {
        const double dv = (floatValue != nullptr) ? static_cast<double>(*floatValue) : *doubleValue;
        keel::format_buf(valBuf, bufSize, rawFmt.empty() ? "{:.2f}" : rawFmt, dv);
    } else if (intValue != nullptr) {
        const keel::StringRef fmt = (rawFmt.empty() || rawFmt == "%.2f") ? "{}" : rawFmt;
        keel::format_buf(valBuf, bufSize, fmt, *intValue);
    }
}

} // namespace

Response SpinBox::Run(
    Context& ctx,
    float* floatValue,
    double* doubleValue,
    keel::i64* intValue,
    const SpinBoxOptions& opts,
    keel::SourceLocation loc
) {
    const ZoneScope zoneScope = ctx.Zone({.theme = opts.theme, .scale = opts.scale});

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
    const SizeMode effW = Control::ResolveWidthMode(ctx, opts.width, SizeMode::Stretch);
    if (!optsCopy.width.IsFixed()) {
        optsCopy.width = effW;
    }
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    optsCopy.prefix = ctx.AllocAffix(opts.prefix);
    optsCopy.suffix = ctx.AllocAffix(opts.suffix);
    optsCopy.valueFormat = ctx.AllocText(opts.valueFormat);
    optsCopy.minusIcon = ctx.AllocIcon(opts.minusIcon);
    optsCopy.plusIcon = ctx.AllocIcon(opts.plusIcon);

    if (opts.min.has_value() || opts.max.has_value()) {
        WriteNumericValue(ReadNumericValue(floatValue, doubleValue, intValue), floatValue, doubleValue, intValue, opts);
    }

    char valBuf[64]{};
    FormatNumericValue(valBuf, sizeof(valBuf), floatValue, doubleValue, intValue, optsCopy);
    keel::StringRef valStr = valBuf;

    auto& state = ctx.GetOrCreateState<SpinBoxState>(id);
    if (!res.pressed && !mouseJustPressed) {
        state.actionBtnDown = false;
        state.minusBtnDown = false;
        state.plusBtnDown = false;
    }

    const UiTheme& theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();
    const Rect refBounds = (res.rect.w > 0.0f && res.rect.h > 0.0f)
        ? res.rect
        : Rect({0.0f, 0.0f}, SpinBox::Measure(valStr, optsCopy, activeSize, canvas, theme));

    const SpinBoxStepStyle effStepStyle = ResolveEffectiveStepStyle(optsCopy);
    const bool showStepBtns = (effStepStyle != SpinBoxStepStyle::None);
    SpinBoxRects rects = ResolveSpinBoxRects(refBounds, optsCopy, activeSize, canvas, theme, showStepBtns);

    const Vec2 mousePos = ctx.GetMousePos();
    const bool stepMinusHovered = showStepBtns && !effectiveDisabled && (res.hovered || res.pressed) && rects.stepMinusRect.Contains(mousePos);
    const bool stepPlusHovered = showStepBtns && !effectiveDisabled && (res.hovered || res.pressed) && rects.stepPlusRect.Contains(mousePos);

    double stepClickDelta = 0.0;
    const double stepVal = opts.step.value_or(1.0);
    const double nowSec = ctx.GetCurrentTime();
    if (mouseJustPressed) {
        state.wasFocusedOnPress = wasFocused;
        state.minusBtnDown = false;
        state.plusBtnDown = false;

        if (stepMinusHovered) {
            state.actionBtnDown = true;
            state.minusBtnDown = true;
            state.repeatStartTime = nowSec;
            state.lastRepeatTime = nowSec;
            stepClickDelta = -stepVal;
            ctx.RequestRepaint(2);
        } else if (stepPlusHovered) {
            state.actionBtnDown = true;
            state.plusBtnDown = true;
            state.repeatStartTime = nowSec;
            state.lastRepeatTime = nowSec;
            stepClickDelta = stepVal;
            ctx.RequestRepaint(2);
        }
    } else if (res.pressed && state.actionBtnDown && !opts.readOnly && !effectiveDisabled) {
        ctx.RequestRepaint(2);
        const bool activeBtnHovered = (state.minusBtnDown && stepMinusHovered) || (state.plusBtnDown && stepPlusHovered);
        if (activeBtnHovered) {
            constexpr double kRepeatInitialDelaySec = 0.40;
            constexpr double kRepeatIntervalSec = 0.065;
            constexpr double kFastRepeatDelaySec = 1.60;
            constexpr double kFastRepeatIntervalSec = 0.032;
            const double heldSec = nowSec - state.repeatStartTime;
            if (heldSec >= kRepeatInitialDelaySec) {
                const double interval = (heldSec >= kFastRepeatDelaySec) ? kFastRepeatIntervalSec : kRepeatIntervalSec;
                if ((nowSec - state.lastRepeatTime) >= interval) {
                    state.lastRepeatTime = nowSec;
                    stepClickDelta = state.minusBtnDown ? -stepVal : stepVal;
                }
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

    keel::usize cursor = 0;
    keel::usize selAnchor = 0;
    keel::StringRef displayText = valStr;

    if (isFocused || ctx.HasActiveTextEdit(id)) {
        keel::StringRef currentEditText = ctx.EnsureTextEdit(id, valStr, /*multiline=*/false, opts.readOnly);
        const Rect textRect = rects.textRect;
        const float inputFont = theme.ResolveFontSize(optsCopy.valueFormat, TextRole::LabelMedium, activeSize);

        if ((mouseJustPressed || res.pressed) && !state.actionBtnDown) {
            const float localX = (mousePos.x - textRect.x) + state.scrollX;
            keel::usize hitIdx = TextUtil::HitTestIndex(currentEditText, localX, inputFont, canvas);
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
                        const keel::usize anchor = ctx.GetTextEditAnchor(id);
                        ctx.SetTextEditCursor(id, hitIdx, anchor);
                    } else {
                        ctx.SetTextEditCursor(id, hitIdx, hitIdx);
                    }
                } else if (state.clickCount == 2) {
                    keel::usize wStart = 0;
                    keel::usize wEnd = 0;
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
                    keel::usize wStart = 0;
                    keel::usize wEnd = 0;
                    TextUtil::SelectWordAt(currentEditText, hitIdx, wStart, wEnd);
                    wStart = SnapToUtf8CodepointBoundary(currentEditText, wStart);
                    wEnd = SnapToUtf8CodepointBoundary(currentEditText, wEnd);
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
                if (const auto f = keel::parse_f32(ExtractNumericPrefix(displayText, true))) {
                    *floatValue = static_cast<float>(ClampNumericValue(static_cast<double>(*f), opts));
                }
            } else if (doubleValue != nullptr) {
                if (const auto d = keel::parse_f64(ExtractNumericPrefix(displayText, true))) {
                    *doubleValue = ClampNumericValue(*d, opts);
                }
            } else if (intValue != nullptr) {
                if (const auto i = keel::parse_i64(ExtractNumericPrefix(displayText, false), 10)) {
                    const double clamped = ClampNumericValue(static_cast<double>(*i), opts);
                    *intValue = keel::round_i64(clamped);
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
            const keel::usize safeCursor = keel::min(cursor, displayText.size());
            const float cursorX = TextUtil::MeasureWidth(displayText.substr(0, safeCursor), inputFont, canvas);
            const float fullTextW = TextUtil::MeasureWidth(displayText, inputFont, canvas);
            const float visibleW = keel::max(0.0f, rects.textRect.w - 2.0f);

            if (cursorX < state.scrollX) {
                state.scrollX = cursorX;
            } else if (cursorX > state.scrollX + visibleW) {
                state.scrollX = cursorX - visibleW;
            }
            const float maxScroll = keel::max(0.0f, fullTextW - visibleW);
            state.scrollX = keel::clamp(state.scrollX, 0.0f, maxScroll);
        } else if (!isFocused) {
            state.scrollX = 0.0f;
        }
    } else {
        state.scrollX = 0.0f;
    }

    const keel::usize selStart = keel::min(cursor, selAnchor);
    const keel::usize selEnd = keel::max(cursor, selAnchor);

    bool stepMinusDisabled = false;
    bool stepPlusDisabled = false;
    const double curNum = ReadNumericValue(floatValue, doubleValue, intValue);
    if (showStepBtns) {
        if (opts.min.has_value() && curNum <= *opts.min + 1e-6) stepMinusDisabled = true;
        if (opts.max.has_value() && curNum >= *opts.max - 1e-6) stepPlusDisabled = true;
    }

    float fillRatio = 0.0f;
    if (opts.showFillBar && opts.min.has_value() && opts.max.has_value() && (*opts.max > *opts.min)) {
        fillRatio = static_cast<float>(keel::clamp((curNum - *opts.min) / (*opts.max - *opts.min), 0.0, 1.0));
    }

    const ControlSpec ctrl = EffectiveSpinBoxControl(optsCopy, activeSize, theme);
    Node* node = ctx.CreateNode(NodeType::SpinBox, id);
    node->theme = zoneScope.GetTheme();

    auto* data = ctx.AllocPayload<SpinBoxData>();
    data->opts = optsCopy;
    data->displayText = ctx.AllocString(displayText);
    node->text = data->displayText;
    data->fillRatio = fillRatio;
    data->cursor = cursor;
    data->selStart = selStart;
    data->selEnd = selEnd;
    data->scrollX = state.scrollX;
    data->cursorVisible = isFocused && !opts.readOnly && ctx.IsCursorVisible(opts.cursorMode);
    data->cursorWidth = ctx.GetCursorConfig().width;
    data->showStepBtns = showStepBtns;
    data->stepMinusHovered = stepMinusHovered;
    data->stepPlusHovered = stepPlusHovered;
    data->stepMinusPressed = state.minusBtnDown && res.pressed;
    data->stepPlusPressed = state.plusBtnDown && res.pressed;
    data->stepMinusDisabled = stepMinusDisabled;
    data->stepPlusDisabled = stepPlusDisabled;
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        static_cast<keel::u8>(optsCopy.stepStyle),
        optsCopy.color,
        optsCopy.background,
        optsCopy.border,
        optsCopy.width,
        optsCopy.height,
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
        data->stepMinusPressed,
        data->stepPlusPressed,
        data->stepMinusDisabled,
        data->stepPlusDisabled,
        static_cast<keel::u8>(optsCopy.minusIcon.GetType()),
        static_cast<keel::u8>(optsCopy.minusIcon.GetId()),
        optsCopy.minusIcon.GetPath(),
        static_cast<keel::u8>(optsCopy.plusIcon.GetType()),
        static_cast<keel::u8>(optsCopy.plusIcon.GetId()),
        optsCopy.plusIcon.GetPath(),
        optsCopy.prefix,
        optsCopy.suffix,
        optsCopy.valueFormat
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<SpinBoxData>();
        return SpinBox::Measure(
            d ? d->displayText : keel::StringRef{},
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
            d ? d->displayText : keel::StringRef{},
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
            d ? d->stepMinusPressed : false,
            d ? d->stepPlusPressed : false
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

Vec2 SpinBox::Measure(
    keel::StringRef /*displayText*/,
    const SpinBoxOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const Vec2 boxDims = MeasureSpinBox(opts, size, canvas, theme);
    const ControlSpec ctrl = EffectiveSpinBoxControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, theme);
}

Vec2 SpinBox::Measure(
    keel::StringRef displayText,
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
    keel::StringRef displayText,
    const SpinBoxOptions& opts,
    UiSize size,
    bool hovered,
    bool focused,
    keel::usize cursor,
    bool disabled,
    bool cursorVisible,
    float cursorWidth,
    const UiTheme& theme,
    keel::usize selStart,
    keel::usize selEnd,
    float scrollX,
    bool showStepBtns,
    bool stepMinusHovered,
    bool stepPlusHovered,
    bool stepMinusDisabled,
    bool stepPlusDisabled,
    float fillRatio,
    bool stepMinusPressed,
    bool stepPlusPressed
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const InputFieldMetrics& m = theme.GetInputField(size);
    const SpinBoxStepStyle effStyle = showStepBtns ? ResolveEffectiveStepStyle(opts) : SpinBoxStepStyle::None;
    const SpinBoxRects rects = ResolveSpinBoxRects(bounds, opts, size, &canvas, theme, showStepBtns);
    const Rect boxRect = rects.boxRect;
    const bool isExternalStepBtns = (effStyle == SpinBoxStepStyle::Detached || effStyle == SpinBoxStepStyle::Split);
    const bool inputHovered = hovered && (!isExternalStepBtns || (!stepMinusHovered && !stepPlusHovered));

    const auto& pal = theme.palette;
    const bool hasBg = opts.background.IsBackgroundEnabled(true);
    Color boxBg = hasBg
        ? ((inputHovered && !disabled) ? pal.surface : pal.inputBackground)
        : Color::Transparent();
    Color boxBorder;
    float strokeW = (opts.border.width > 0.0f) ? theme.Scale(opts.border.width) : m.strokeWidth;

    const VariantColors& primaryVar = theme.GetVariant(UiVariant::Primary);
    const Color accentCol = opts.color.HasAny()
        ? theme.ResolveColor(opts.color, hovered && !disabled, focused && !disabled, disabled, primaryVar.bg, primaryVar.bgHover, primaryVar.bgActive, primaryVar.bg)
        : primaryVar.bg;

    if (focused && !disabled) {
        boxBorder = opts.color.HasAny() ? accentCol : pal.focusRing;
        if (opts.border.width > 0.0f || m.strokeWidth > 0.0f) strokeW += 0.5f;
    } else if (inputHovered && !disabled) {
        boxBorder = pal.borderHover;
    } else {
        boxBorder = pal.inputBorder;
    }

    Color textCol = opts.valueFormat.color.HasAny()
        ? theme.ResolveTextColor(opts.valueFormat, UiVariant::Default, disabled, inputHovered, focused)
        : (disabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary);
    Color unitCol = pal.textSecondary;

    if (disabled) {
        boxBg = boxBg.WithAlphaF(0.40f);
        boxBorder = boxBorder.WithAlphaF(0.40f);
        unitCol = unitCol.WithAlphaF(0.40f);
    }

    if (hasBg && opts.background.HasAny()) {
        if (!opts.background.IsSurface()) {
            boxBg = theme.ResolveColor(opts.background, inputHovered, focused, disabled, boxBg);
        } else if (opts.background.HasAlphaScale()) {
            boxBg = boxBg.ScaleAlpha(opts.background.alphaScale);
        }
    }
    if (opts.border.color.HasAny()) {
        boxBorder = theme.ResolveColor(opts.border.color, inputHovered, focused, disabled, boxBorder);
    }

    const float radius = opts.border.radius.has_value() ? theme.Scale(*opts.border.radius) : m.cornerRadius;
    canvas.DrawRoundRect(boxRect, radius, boxBg, boxBorder, strokeW);

    // Optional fill bar along the bottom inside the SpinBox input field
    if (opts.showFillBar && fillRatio > 0.001f) {
        const float barH = keel::max(2.0f, theme.Scale(3.0f));
        const float padX = keel::max(2.0f, radius * 0.35f);
        const float maxBarW = keel::max(0.0f, boxRect.w - padX * 2.0f);
        const float barW = maxBarW * keel::clamp(fillRatio, 0.0f, 1.0f);
        if (barW > 1.0f) {
            const Rect barRect(boxRect.x + padX, boxRect.Bottom() - barH - 1.5f, barW, barH);
            const Color barCol = disabled ? accentCol.WithAlphaF(0.35f) : accentCol.WithAlphaF(0.85f);
            canvas.DrawRoundRect(barRect, barH * 0.5f, barCol, Color::Transparent(), 0.0f);
        }
    }

    const Icon effMinusIcon = opts.minusIcon.IsValid() ? opts.minusIcon : Icon::Minus();
    const Icon effPlusIcon = opts.plusIcon.IsValid() ? opts.plusIcon : Icon::Plus();

    // External square (H x H) stepper buttons for Detached / Split styles
    if (isExternalStepBtns) {
        const float baseStrokeW = (opts.border.width > 0.0f) ? theme.Scale(opts.border.width) : m.strokeWidth;
        const float iconSz = keel::max(8.0f, m.iconSize);
        auto drawSquareStepBtn = [&](const Rect& btnRect, const Icon& ic, bool btnHov, bool btnPress, bool btnDis) {
            if (btnRect.w <= 0.0f || btnRect.h <= 0.0f) return;
            const bool effDis = disabled || btnDis;
            Color btnBg = hasBg ? pal.inputBackground : Color::Transparent();
            if (hasBg && opts.background.HasAny() && !opts.background.IsSurface()) {
                btnBg = theme.ResolveColor(opts.background, btnHov && !effDis, btnPress && !effDis, effDis, btnBg);
            } else {
                if (!effDis) {
                    if (btnPress) {
                        btnBg = accentCol.WithAlphaF(0.28f);
                    } else if (btnHov) {
                        btnBg = pal.surfaceHover;
                    }
                }
                if (hasBg && opts.background.IsSurface() && opts.background.HasAlphaScale()) {
                    btnBg = btnBg.ScaleAlpha(opts.background.alphaScale);
                }
            }

            Color btnBorder = pal.inputBorder;
            if (!effDis) {
                if (btnPress) {
                    btnBorder = accentCol;
                } else if (btnHov) {
                    btnBorder = opts.color.HasAny() ? accentCol.WithAlphaF(0.75f) : pal.borderHover;
                }
            }
            if (opts.border.color.HasAny()) {
                btnBorder = theme.ResolveColor(opts.border.color, btnHov && !effDis, btnPress && !effDis, effDis, btnBorder);
            }
            if (effDis) {
                btnBg = btnBg.WithAlphaF(0.40f);
                btnBorder = btnBorder.WithAlphaF(0.40f);
            }

            canvas.DrawRoundRect(btnRect, radius, btnBg, btnBorder, baseStrokeW);
            const Rect icRect(
                btnRect.x + (btnRect.w - iconSz) * 0.5f,
                btnRect.y + (btnRect.h - iconSz) * 0.5f,
                iconSz,
                iconSz
            );
            const Color icCol = effDis
                ? unitCol.WithAlphaF(0.35f)
                : ((btnHov || btnPress) ? (opts.color.HasAny() ? accentCol : pal.textPrimary) : pal.textPrimary);
            ic.Draw(canvas, icRect, icCol);
        };

        drawSquareStepBtn(rects.stepMinusRect, effMinusIcon, stepMinusHovered, stepMinusPressed, stepMinusDisabled);
        drawSquareStepBtn(rects.stepPlusRect, effPlusIcon, stepPlusHovered, stepPlusPressed, stepPlusDisabled);
    }

    const float inputFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelMedium, size);
    const float affixInnerGap = theme.Scale(4.0f);

    if (opts.prefix.HasContentOrReserve() && rects.prefixRect.w > 0.0f) {
        const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const Color pTextCol = theme.ResolveTextColor(opts.prefix.text, UiVariant::Secondary, disabled, inputHovered, focused);
        const Color pIconCol = opts.prefix.text.color.HasAny() ? pTextCol : unitCol;
        TextUtil::DrawAffix(canvas, opts.prefix, rects.prefixRect, pFont, m.iconSize, pTextCol, pIconCol, affixInnerGap);
    }

    if (effStyle == SpinBoxStepStyle::Inline) {
        const float iconSz = keel::max(theme.Scale(10.0f), m.iconSize);
        const float btnRadius = theme.Scale(4.0f);
        const float sepInsetY = theme.Scale(5.0f);

        auto drawInlineStepBtn = [&](const Rect& btnRect, const Icon& ic, bool btnHov, bool btnPress, bool btnDis) {
            if (btnRect.w <= 0.0f || btnRect.h <= 0.0f) return;
            const bool effDis = disabled || btnDis;
            Color btnBg = pal.surfaceHover.WithAlphaF(effDis ? 0.18f : 0.45f);
            Color btnBorder = pal.borderSubtle.WithAlphaF(effDis ? 0.25f : 0.65f);
            if (!effDis) {
                if (btnPress) {
                    btnBg = accentCol.WithAlphaF(0.28f);
                    btnBorder = accentCol;
                } else if (btnHov) {
                    btnBg = pal.surfaceHover;
                    btnBorder = opts.color.HasAny() ? accentCol.WithAlphaF(0.75f) : pal.borderHover;
                }
            }
            canvas.DrawRoundRect(btnRect, btnRadius, btnBg, btnBorder, 1.0f);
            const Rect icRect(
                btnRect.x + (btnRect.w - iconSz) * 0.5f,
                btnRect.y + (btnRect.h - iconSz) * 0.5f,
                iconSz,
                iconSz
            );
            const Color icCol = effDis
                ? unitCol.WithAlphaF(0.35f)
                : ((btnHov || btnPress) ? (opts.color.HasAny() ? accentCol : pal.textPrimary) : pal.textPrimary);
            ic.Draw(canvas, icRect, icCol);
        };

        drawInlineStepBtn(rects.stepPlusRect, effPlusIcon, stepPlusHovered, stepPlusPressed, stepPlusDisabled);
        drawInlineStepBtn(rects.stepMinusRect, effMinusIcon, stepMinusHovered, stepMinusPressed, stepMinusDisabled);

        // Subtle vertical separator before stepper buttons
        canvas.DrawLine(
            {rects.stepSepX, boxRect.y + sepInsetY},
            {rects.stepSepX, boxRect.Bottom() - sepInsetY},
            disabled ? pal.border.WithAlphaF(0.25f) : pal.border.WithAlphaF(0.65f),
            1.0f
        );
    }

    if (opts.suffix.HasContentOrReserve() && rects.suffixRect.w > 0.0f) {
        const float sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
        const Color sTextCol = theme.ResolveTextColor(opts.suffix.text, UiVariant::Secondary, disabled, hovered, focused);
        const Color sIconCol = opts.suffix.text.color.HasAny() ? sTextCol : unitCol;
        TextUtil::DrawAffix(canvas, opts.suffix, rects.suffixRect, sFont, m.iconSize, sTextCol, sIconCol, affixInnerGap);
    }

    const Rect textRect = rects.textRect;
    canvas.PushClip(textRect);

    if (focused && !disabled && selStart != selEnd && !displayText.empty()) {
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

    if (!displayText.empty()) {
        if (focused || scrollX > 0.0f) {
            const float fullW = keel::max(textRect.w, canvas.MeasureText(displayText, inputFont).x + 8.0f);
            const Rect scrolledRect(textRect.x - scrollX, textRect.y, fullW, textRect.h);
            canvas.DrawTextBox(displayText, scrolledRect, inputFont, textCol, 0.0f, 0.5f);
        } else {
            canvas.DrawTextBox(displayText, textRect, inputFont, textCol, 0.0f, 0.5f);
        }
    }

    if (focused && !disabled && !opts.readOnly && cursorVisible) {
        const keel::usize safeCursor = keel::min(cursor, displayText.size());
        const keel::StringRef sub = displayText.substr(0, safeCursor);
        const float subW = canvas.MeasureText(sub, inputFont).x;
        const float caretX = textRect.x - scrollX + subW;
        const float caretH = inputFont * 1.15f;
        const float caretY = boxRect.y + (boxRect.h - caretH) * 0.5f;
        canvas.DrawLine({caretX, caretY}, {caretX, caretY + caretH}, pal.caret, cursorWidth);
    }
    canvas.PopClip();
}

} // namespace guikit
