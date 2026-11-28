#include "slider_internal.hpp"
#include <cstring>

namespace guikit {

using namespace detail::slider_impl;

// ============================================================================
// RangeSlider (Two-Thumb Continuous / Stepped Interval Slider)
// ============================================================================

namespace {

enum class RangeDragMode : keel::u8 {
    None = 0,
    LowThumb,
    HighThumb,
    IntervalSpan
};

struct RangeSliderState {
    RangeDragMode dragMode{RangeDragMode::None};
    keel::u8 activeThumb{1}; // 0 = low thumb, 1 = high thumb
    keel::f64 dragStartLow{0.0};
    keel::f64 dragStartHigh{0.0};
    keel::f32 dragStartTravel{0.0f}; // pointer position along the track travel (SliderTrackLayout::Travel)
};

struct RangeSliderData {
    RangeSliderOptions opts;
    keel::f64 lowValue{0.0};
    keel::f64 highValue{1.0};
    keel::f64 min{0.0};
    keel::f64 max{1.0};
    keel::u8 activeThumb{1};
    keel::u8 hoveredMask{0};
    keel::u8 activeMask{0};
};

RangeDragMode ClassifyRangeTarget(
    keel::f32 pos,
    keel::f32 lowPos,
    keel::f32 highPos,
    keel::f32 thumbSize,
    bool shiftHeld
) noexcept {
    if (shiftHeld) {
        return RangeDragMode::IntervalSpan;
    }
    const keel::f32 hitRadius = thumbSize * 0.675f;
    const keel::f32 distLow = keel::abs(pos - lowPos);
    const keel::f32 distHigh = keel::abs(pos - highPos);

    // Direct thumb hits always take priority over interval-span dragging
    if (distLow <= hitRadius || distHigh <= hitRadius) {
        if (distLow < distHigh || (distLow == distHigh && pos <= lowPos)) {
            return RangeDragMode::LowThumb;
        }
        return RangeDragMode::HighThumb;
    }

    const bool insideIntervalBar = (pos > lowPos + hitRadius) &&
                                   (pos < highPos - hitRadius) &&
                                   ((highPos - lowPos) > thumbSize * 1.8f);
    if (insideIntervalBar) {
        return RangeDragMode::IntervalSpan;
    }
    if (distLow < distHigh || (distLow == distHigh && pos <= lowPos)) {
        return RangeDragMode::LowThumb;
    }
    return RangeDragMode::HighThumb;
}

keel::u8 DragModeToThumbMask(RangeDragMode mode) noexcept {
    switch (mode) {
        case RangeDragMode::LowThumb:     return 0x1u;
        case RangeDragMode::HighThumb:    return 0x2u;
        case RangeDragMode::IntervalSpan: return 0x3u;
        case RangeDragMode::None:         return 0x0u;
    }
    return 0x0u;
}


void FormatRangeSliderPair(
    char* outBuf,
    keel::usize bufSize,
    keel::f64 lowVal,
    keel::f64 highVal,
    bool isInt,
    keel::StringRef format
) noexcept {
    keel::i32 specCount = 0;
    keel::i32 braceCount = 0;
    for (keel::usize i = 0; i < format.size(); ++i) {
        if (format[i] == '%') {
            if (i + 1 < format.size() && format[i + 1] == '%') {
                ++i;
                continue;
            }
            ++specCount;
        } else if (format[i] == '{') {
            if (i + 1 < format.size() && format[i + 1] == '{') {
                ++i;
                continue;
            }
            ++braceCount;
        }
    }

    if (braceCount >= 2 || specCount >= 2) {
        if (isInt) {
            keel::format_buf(
                outBuf,
                bufSize,
                format,
                keel::round_i64(lowVal),
                keel::round_i64(highVal)
            );
        } else {
            keel::format_buf(outBuf, bufSize, format, lowVal, highVal);
        }
        return;
    }

    char lowBuf[48]{};
    char highBuf[48]{};
    FormatSliderValue(lowBuf, sizeof(lowBuf), lowVal, isInt, format);
    FormatSliderValue(highBuf, sizeof(highBuf), highVal, isInt, format);
    keel::format_buf(outBuf, bufSize, "{} .. {}", keel::StringRef(lowBuf), keel::StringRef(highBuf));
}

} // namespace

Response detail::RangeSlider::Run(
    Context& ctx,
    keel::f64* lowDouble,
    keel::f64* highDouble,
    keel::f32* lowFloat,
    keel::f32* highFloat,
    keel::i64* lowInt,
    keel::i64* highInt,
    keel::f64 min,
    keel::f64 max,
    const RangeSliderOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme& activeTheme = ctx.GetTheme();

    const RangeSliderOptions& optsCopy = opts;

    const SliderMetrics& m = activeTheme.GetSlider(activeSize);
    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);
    const bool isInt = (lowInt != nullptr || highInt != nullptr);
    const keel::f64 effectiveStep = (optsCopy.step.has_value() && *optsCopy.step > 0.0) ? *optsCopy.step : (isInt ? 1.0 : 0.0);
    const keel::f64 minSpan = keel::clamp(keel::max(0.0, optsCopy.minSpan), 0.0, rangeMax - rangeMin);

    keel::f64 curLow = (lowDouble != nullptr) ? *lowDouble :
                    (lowFloat != nullptr)  ? keel::as<keel::f64>(*lowFloat) :
                    (lowInt != nullptr)    ? keel::as<keel::f64>(*lowInt) : rangeMin;
    keel::f64 curHigh = (highDouble != nullptr) ? *highDouble :
                     (highFloat != nullptr)  ? keel::as<keel::f64>(*highFloat) :
                     (highInt != nullptr)    ? keel::as<keel::f64>(*highInt) : rangeMax;

    curLow = keel::clamp(curLow, rangeMin, rangeMax);
    curHigh = keel::clamp(curHigh, rangeMin, rangeMax);
    if (curHigh < curLow + minSpan) {
        curHigh = keel::min(rangeMax, curLow + minSpan);
        curLow = keel::max(rangeMin, curHigh - minSpan);
    }

    char initValBuf[96];
    FormatRangeSliderPair(initValBuf, sizeof(initValBuf), curLow, curHigh, isInt, optsCopy.valueFormat.text);
    char maxSpanBuf[96];
    FormatRangeSliderPair(maxSpanBuf, sizeof(maxSpanBuf), rangeMin, rangeMax, isInt, optsCopy.valueFormat.text);
    const keel::StringRef maxSpanView(maxSpanBuf);

    Rect prevBounds = ctx.GetPrevFrameBounds(id);
    auto& state = ctx.GetOrCreateState<RangeSliderState>(id);

    ctx.RegisterFocusable(id, effectiveDisabled);

    const ControlSpec ctrl = EffectiveSliderControl(optsCopy, activeSize, activeTheme);
    Response res = ctx.Interact(id);
    keel::u8 hoveredMask = 0x0u;
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        state.dragMode = RangeDragMode::None;
    } else {
        const SliderControlMetrics cm = ComputeSliderControlMetrics(
            optsCopy.textLeft, optsCopy.textRight, optsCopy.valueFormat, initValBuf,
            optsCopy.showValue, optsCopy.valuePlacement, ctrl.width,
            0.0f, activeSize, ctx.GetCanvas(), activeTheme,
            keel::ArrayRef<keel::StringRef>(&maxSpanView, 1), false, 0.0, 1.0, false, optsCopy.orientation
        );
        const Rect ctrlRect = prevBounds;
        const SliderTrackLayout layout =
            ComputeSliderTrackLayout(ctrlRect, cm, m, IsVerticalSlider(optsCopy.orientation));
        const keel::f32 trackLen = keel::max(1.0f, layout.length);

        // Thumb and pointer positions along the travel from the min end (X when horizontal,
        // distance up from the bottom when vertical)
        const keel::f64 range = keel::max(1e-9, rangeMax - rangeMin);
        const keel::f32 lowRatio = keel::as<keel::f32>(keel::clamp((curLow - rangeMin) / range, 0.0, 1.0));
        const keel::f32 highRatio = keel::as<keel::f32>(keel::clamp((curHigh - rangeMin) / range, 0.0, 1.0));
        const keel::f32 lowPos = lowRatio * layout.length;
        const keel::f32 highPos = highRatio * layout.length;
        const keel::f32 pointerPos = layout.Travel(ctx.GetMousePos());
        const RangeDragMode hoverTarget = ClassifyRangeTarget(
            pointerPos, lowPos, highPos, m.thumbSize, ctx.GetKeyModifiers().shift
        );

        if (res.clicked || (res.pressed && state.dragMode == RangeDragMode::None)) {
            state.dragMode = hoverTarget;
            if (state.dragMode == RangeDragMode::LowThumb) {
                state.activeThumb = 0;
            } else if (state.dragMode == RangeDragMode::HighThumb) {
                state.activeThumb = 1;
            }
            state.dragStartLow = curLow;
            state.dragStartHigh = curHigh;
            state.dragStartTravel = pointerPos;
            ctx.SetActiveId(id);
            ctx.RequestFocus(id);
        }

        if (state.dragMode != RangeDragMode::None) {
            hoveredMask = DragModeToThumbMask(state.dragMode);
        } else if (res.hovered) {
            hoveredMask = DragModeToThumbMask(hoverTarget);
        }

        if (state.dragMode != RangeDragMode::None) {
            if (!ctx.IsMouseDown()) {
                state.dragMode = RangeDragMode::None;
                if (ctx.GetActiveId() == id) {
                    ctx.SetActiveId(Id::Null());
                }
            } else {
                res.pressed = true;
                const keel::f32 pos = layout.Travel(ctx.GetMousePos());
                const keel::f32 t = layout.RatioAt(ctx.GetMousePos());
                auto quantize = [&](keel::f64 v) -> keel::f64 {
                    if (effectiveStep > 0.0) {
                        v = rangeMin + keel::round((v - rangeMin) / effectiveStep) * effectiveStep;
                    }
                    return keel::clamp(v, rangeMin, rangeMax);
                };

                if (state.dragMode == RangeDragMode::LowThumb) {
                    keel::f64 rawLow = quantize(rangeMin + t * (rangeMax - rangeMin));
                    rawLow = keel::min(rawLow, curHigh - minSpan);
                    rawLow = keel::clamp(rawLow, rangeMin, rangeMax);
                    if (rawLow != curLow) {
                        curLow = rawLow;
                        res.changed = true;
                        ctx.RequestRepaint(3);
                    }
                } else if (state.dragMode == RangeDragMode::HighThumb) {
                    keel::f64 rawHigh = quantize(rangeMin + t * (rangeMax - rangeMin));
                    rawHigh = keel::max(rawHigh, curLow + minSpan);
                    rawHigh = keel::clamp(rawHigh, rangeMin, rangeMax);
                    if (rawHigh != curHigh) {
                        curHigh = rawHigh;
                        res.changed = true;
                        ctx.RequestRepaint(3);
                    }
                } else if (state.dragMode == RangeDragMode::IntervalSpan) {
                    const keel::f64 span = state.dragStartHigh - state.dragStartLow;
                    const keel::f64 deltaVal = ((pos - state.dragStartTravel) / trackLen) * (rangeMax - rangeMin);
                    keel::f64 newLow = state.dragStartLow + deltaVal;
                    if (newLow < rangeMin) newLow = rangeMin;
                    if (newLow + span > rangeMax) newLow = rangeMax - span;
                    newLow = quantize(newLow);
                    keel::f64 newHigh = keel::clamp(newLow + span, rangeMin, rangeMax);
                    if (effectiveStep > 0.0) {
                        newHigh = quantize(newHigh);
                    }
                    if (newLow != curLow || newHigh != curHigh) {
                        curLow = newLow;
                        curHigh = newHigh;
                        res.changed = true;
                        ctx.RequestRepaint(3);
                    }
                }
            }
        }

        // Keyboard arrow adjustment when focused
        if (ctx.IsFocused(id) && state.dragMode == RangeDragMode::None) {
            const keel::f64 stepDelta = (effectiveStep > 0.0) ? effectiveStep : (rangeMax - rangeMin) * 0.05;
            keel::f64 dir = 0.0;
            if (ctx.ConsumeKey(Key::Right) || ctx.ConsumeKey(Key::Up)) dir = 1.0;
            else if (ctx.ConsumeKey(Key::Left) || ctx.ConsumeKey(Key::Down)) dir = -1.0;

            if (dir != 0.0) {
                if (state.activeThumb == 0) {
                    const keel::f64 nextLow = keel::clamp(curLow + dir * stepDelta, rangeMin, curHigh - minSpan);
                    if (nextLow != curLow) {
                        curLow = nextLow;
                        res.changed = true;
                        ctx.RequestRepaint(3);
                    }
                } else {
                    const keel::f64 nextHigh = keel::clamp(curHigh + dir * stepDelta, curLow + minSpan, rangeMax);
                    if (nextHigh != curHigh) {
                        curHigh = nextHigh;
                        res.changed = true;
                        ctx.RequestRepaint(3);
                    }
                }
            }
        }
    }

    if (res.hovered && !optsCopy.tooltip.empty()) {
        ctx.SetTooltip(optsCopy.tooltip, prevBounds);
    }

    if (lowDouble != nullptr)  *lowDouble = curLow;
    if (highDouble != nullptr) *highDouble = curHigh;
    if (lowFloat != nullptr)   *lowFloat = keel::as<keel::f32>(curLow);
    if (highFloat != nullptr)  *highFloat = keel::as<keel::f32>(curHigh);
    if (lowInt != nullptr)     *lowInt = keel::round_i64(curLow);
    if (highInt != nullptr)    *highInt = keel::round_i64(curHigh);

    Node* node = ctx.CreateNode(NodeType::RangeSlider, id);
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed || (state.dragMode != RangeDragMode::None);
    node->focused = ctx.IsFocusVisible(id);

    const keel::u8 activeMask = DragModeToThumbMask(state.dragMode);
    if (activeMask != 0x0u) {
        hoveredMask = activeMask;
    }

    auto* data = ctx.AllocPayload<RangeSliderData>();
    data->opts = optsCopy;
    data->opts.textLeft = ctx.AllocText(optsCopy.textLeft);
    data->opts.textRight = ctx.AllocText(optsCopy.textRight);
    data->opts.tooltip = ctx.AllocText(optsCopy.tooltip);
    data->opts.valueFormat = ctx.AllocText(optsCopy.valueFormat);
    node->text = ctx.AllocString(initValBuf);
    data->lowValue = curLow;
    data->highValue = curHigh;
    data->min = rangeMin;
    data->max = rangeMax;
    data->activeThumb = state.activeThumb;
    data->hoveredMask = hoveredMask;
    data->activeMask = activeMask;
    node->SetPayload(data);

    const keel::u64 lowBits = keel::bit_cast<keel::u64>(curLow);
    const keel::u64 highBits = keel::bit_cast<keel::u64>(curHigh);
    node->visualHash = detail::HashArgs(
        lowBits,
        highBits,
        state.activeThumb,
        hoveredMask,
        activeMask,
        keel::as<keel::u8>(data->opts.orientation),
        data->opts.color,
        keel::as<keel::u8>(data->opts.valuePlacement),
        data->opts.width,
        data->opts.height,
        data->opts.minWidth.value_or(0.0f),
        data->opts.maxWidth.value_or(0.0f),
        data->opts.minHeight.value_or(0.0f),
        data->opts.maxHeight.value_or(0.0f),
        data->opts.showValue,
        data->opts.textLeft,
        data->opts.textRight,
        data->opts.valueFormat
    );

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<RangeSliderData>();
        return detail::RangeSlider::Measure(
            d ? d->opts : RangeSliderOptions{},
            d ? d->lowValue : 0.0,
            d ? d->highValue : 1.0,
            d ? d->min : 0.0,
            d ? d->max : 1.0,
            n->uiSize, canvas, theme, availableWidth
        );
    };

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<RangeSliderData>();
        if (!d) return;
        detail::RangeSlider::Render(
            canvas, n->computedRect, d->lowValue, d->highValue, d->min, d->max,
            d->opts, n->uiSize, n->hovered, n->pressed, n->disabled, theme, n->focused,
            d->activeThumb, d->hoveredMask, d->activeMask
        );
    };

    Control::SetupNode(ctx, node, ctrl);
    return res;
}

Vec2 detail::RangeSlider::Measure(
    const RangeSliderOptions& opts,
    keel::f64 lowValue,
    keel::f64 highValue,
    keel::f64 min,
    keel::f64 max,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 /*availableWidth*/
) {
    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);
    const bool isIntStep = opts.step.has_value() && *opts.step >= 1.0 &&
                           keel::abs(*opts.step - keel::round(*opts.step)) < 0.0001;
    char valBuf[96];
    FormatRangeSliderPair(valBuf, sizeof(valBuf), lowValue, highValue, isIntStep, opts.valueFormat.text);
    char maxSpanBuf[96];
    FormatRangeSliderPair(maxSpanBuf, sizeof(maxSpanBuf), rangeMin, rangeMax, isIntStep, opts.valueFormat.text);
    const keel::StringRef maxSpanView(maxSpanBuf);

    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
        opts.showValue, opts.valuePlacement, ctrl.width,
        0.0f, size, canvas, theme,
        keel::ArrayRef<keel::StringRef>(&maxSpanView, 1), false, 0.0, 1.0, false, opts.orientation
    );
    return Control::Measure(cm.intrinsicControlSize, ctrl, theme);
}

Vec2 detail::RangeSlider::Measure(
    const RangeSliderOptions& opts,
    keel::f64 lowValue,
    keel::f64 highValue,
    keel::f64 min,
    keel::f64 max,
    UiSize size,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, lowValue, highValue, min, max, size, canvas, defaultTheme, availableWidth);
}

void detail::RangeSlider::Render(
    ICanvas& canvas,
    Rect bounds,
    keel::f64 lowValue,
    keel::f64 highValue,
    keel::f64 min,
    keel::f64 max,
    const RangeSliderOptions& opts,
    UiSize size,
    bool hovered,
    bool active,
    bool disabled,
    const UiTheme& theme,
    bool focused,
    keel::u8 activeThumb,
    keel::u8 hoveredMask,
    keel::u8 activeMask
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const SliderMetrics& m = theme.GetSlider(size);
    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);
    const bool isIntStep = opts.step.has_value() && *opts.step >= 1.0 &&
                           keel::abs(*opts.step - keel::round(*opts.step)) < 0.0001;

    char valBuf[96];
    FormatRangeSliderPair(valBuf, sizeof(valBuf), lowValue, highValue, isIntStep, opts.valueFormat.text);
    char maxSpanBuf[96];
    FormatRangeSliderPair(maxSpanBuf, sizeof(maxSpanBuf), rangeMin, rangeMax, isIntStep, opts.valueFormat.text);
    const keel::StringRef maxSpanView(maxSpanBuf);

    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
        opts.showValue, opts.valuePlacement, ctrl.width,
        0.0f, size, &canvas, theme,
        keel::ArrayRef<keel::StringRef>(&maxSpanView, 1), false, 0.0, 1.0, false, opts.orientation
    );

    const Rect ctrlRect = bounds;
    const bool vertical = IsVerticalSlider(opts.orientation);
    const SliderTrackLayout layout = ComputeSliderTrackLayout(ctrlRect, cm, m, vertical);

    const keel::f64 range = rangeMax - rangeMin;
    const keel::f32 lowRatio = (range > 0.0) ? keel::as<keel::f32>(keel::clamp((lowValue - rangeMin) / range, 0.0, 1.0)) : 0.0f;
    const keel::f32 highRatio = (range > 0.0) ? keel::as<keel::f32>(keel::clamp((highValue - rangeMin) / range, 0.0, 1.0)) : 1.0f;
    const Vec2 lowThumb = layout.ThumbCenter(keel::min(lowRatio, highRatio));
    const Vec2 highThumb = layout.ThumbCenter(keel::max(lowRatio, highRatio));
    const Vec2 midCenter{(lowThumb.x + highThumb.x) * 0.5f, (lowThumb.y + highThumb.y) * 0.5f};

    // 0. Draw companion textLeft / textRight and value readout
    if (vertical) {
        RenderSliderCompanionTextsVertical(
            canvas, ctrlRect, layout, midCenter, m.thumbSize,
            cm, opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
            opts.showValue, opts.valuePlacement, disabled, theme
        );
    } else {
        RenderSliderCompanionTexts(
            canvas, ctrlRect, layout.region.x, layout.region.w, layout.region.y, midCenter.x,
            cm, opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
            opts.showValue, opts.valuePlacement, disabled, theme
        );
    }

    // 1 & 2. Draw track groove and active interval fill via active style painter
    const Color fillCol = ResolveSliderTrackColor(opts.color, disabled, theme);
    RenderSliderTrack(
        canvas, layout, m, lowRatio * layout.length, highRatio * layout.length, true,
        fillCol, true, disabled, theme
    );

    // 3. Draw low and high thumb knobs via active style painter
    const keel::u8 effActiveMask = (activeMask != 0x0u)
        ? activeMask
        : (active ? keel::as<keel::u8>(activeThumb == 0 ? 0x1u : 0x2u) : 0x0u);
    auto drawThumb = [&](Vec2 c, keel::u8 thumbBit, bool isFocusedThumb) {
        const bool thumbActive = active && ((effActiveMask & thumbBit) != 0u);
        const bool thumbHovered = hovered && ((hoveredMask & thumbBit) != 0u);
        const bool thumbFocused = focused && isFocusedThumb;
        const Color thumbAccent = thumbActive ? ResolveSliderActiveThumbColor(opts.color, theme) : fillCol;
        theme.Painter().DrawSliderThumb(canvas, theme, SliderThumbPaintParams{
            .center       = c,
            .thumbSize    = m.thumbSize,
            .vertical     = vertical,
            .accent       = thumbAccent,
            .hovered      = thumbHovered,
            .active       = thumbActive,
            .focused      = thumbFocused,
            .disabled     = disabled,
            .cornerRadius = m.cornerRadius
        });
    };

    drawThumb(lowThumb, 0x1u, activeThumb == 0);
    drawThumb(highThumb, 0x2u, activeThumb == 1);
}


} // namespace guikit
