#include "slider_internal.hpp"
#include <cstring>

namespace guikit {

using namespace detail::slider_impl;

// ============================================================================
// HybridSlider (Discrete Leading Stops + Continuous Numeric Range)
// ============================================================================

namespace {

struct HybridSliderData {
    HybridSliderOptions opts;
    keel::usize modeIndex{0};
    keel::usize stopCount{1};
    keel::f64 value{0.0};
    keel::f64 min{0.0};
    keel::f64 max{1.0};
    bool isInt{false};
};

[[nodiscard]] keel::StringRef ResolveStopLabel(
    keel::ArrayRef<keel::StringRef> stops,
    keel::usize idx
) noexcept {
    if (idx < stops.size() && !stops[idx].empty()) {
        return stops[idx];
    }
    return (idx == 0u) ? keel::StringRef("Off") : keel::StringRef("Auto");
}

[[nodiscard]] bool UseInlineBottomRow(const HybridSliderOptions& opts) noexcept {
    return !IsVerticalSlider(opts.orientation) &&
           opts.showStopLabels &&
           opts.valuePlacement == Placement::Bottom;
}

[[nodiscard]] bool UseInlineVerticalColumn(const HybridSliderOptions& opts) noexcept {
    return IsVerticalSlider(opts.orientation) &&
           opts.showStopLabels &&
           opts.valuePlacement == Placement::Bottom;
}

[[nodiscard]] keel::f32 MeasureMaxNumericWidth(
    const HybridSliderOptions& opts,
    keel::f64 rangeMin,
    keel::f64 rangeMax,
    bool isInt,
    keel::f32 font,
    ICanvas* canvas
) noexcept {
    char buf[64];
    keel::f32 w = 0.0f;
    FormatSliderValue(buf, sizeof(buf), rangeMin, isInt, opts.valueFormat.text);
    w = keel::max(w, TextUtil::MeasureWidth(keel::StringRef(buf), font, canvas));
    FormatSliderValue(buf, sizeof(buf), rangeMax, isInt, opts.valueFormat.text);
    w = keel::max(w, TextUtil::MeasureWidth(keel::StringRef(buf), font, canvas));
    const keel::f64 sampleVal = (rangeMax > rangeMin + 0.1)
        ? (rangeMax - 0.1)
        : (rangeMin + (rangeMax - rangeMin) * 0.88);
    FormatSliderValue(buf, sizeof(buf), sampleVal, isInt, opts.valueFormat.text);
    w = keel::max(w, TextUtil::MeasureWidth(keel::StringRef(buf), font, canvas));
    return w;
}

[[nodiscard]] keel::f32 HybridSliderLabelsBand(
    const HybridSliderOptions& opts,
    keel::usize stopCount,
    keel::f64 rangeMin,
    keel::f64 rangeMax,
    bool isInt,
    const SliderMetrics& m,
    keel::f32 tickFont,
    keel::f32 valFont,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    if (!opts.showStopLabels || stopCount == 0u) {
        return 0.0f;
    }
    if (!IsVerticalSlider(opts.orientation)) {
        const keel::f32 tickH = tickFont * 1.3f;
        const keel::f32 valH = (opts.showValue && UseInlineBottomRow(opts))
            ? (canvas ? canvas->GetLineHeight(valFont) : theme.GetLineHeight(valFont))
            : 0.0f;
        return m.tickGap + keel::max(tickH, valH);
    }
    keel::f32 widest = 0.0f;
    for (keel::usize i = 0; i < stopCount; ++i) {
        widest = keel::max(widest, TextUtil::MeasureWidth(ResolveStopLabel(opts.stops, i), tickFont, canvas));
    }
    if (opts.showValue && UseInlineVerticalColumn(opts)) {
        widest = keel::max(widest, MeasureMaxNumericWidth(opts, rangeMin, rangeMax, isInt, valFont, canvas));
    }
    return m.tickGap + widest;
}

struct HybridTrackZones {
    keel::usize stopCount{1};
    keel::f32 stopStep{28.0f};
    keel::f32 boundaryGap{22.0f};
    keel::f32 dividerPos{10.0f};
    keel::f32 contStartPos{22.0f};
    keel::f32 contLen{1.0f};

    [[nodiscard]] keel::f32 StopPos(keel::usize i) const noexcept {
        return keel::as<keel::f32>(i) * stopStep;
    }

    [[nodiscard]] keel::f32 ValuePos(keel::f64 value, keel::f64 rangeMin, keel::f64 rangeMax) const noexcept {
        const keel::f32 t = (rangeMax > rangeMin)
            ? keel::clamp(keel::as<keel::f32>((value - rangeMin) / (rangeMax - rangeMin)), 0.0f, 1.0f)
            : 0.0f;
        return contStartPos + t * contLen;
    }

    [[nodiscard]] keel::f32 ThumbPos(keel::usize mode, keel::f64 value, keel::f64 rangeMin, keel::f64 rangeMax) const noexcept {
        if (mode < stopCount) {
            return StopPos(mode);
        }
        return ValuePos(value, rangeMin, rangeMax);
    }
};

[[nodiscard]] HybridTrackZones ComputeHybridTrackZones(
    const HybridSliderOptions& opts,
    keel::usize stopCount,
    const SliderTrackLayout& layout,
    const SliderMetrics& m,
    keel::f32 tickFont,
    ICanvas* canvas
) noexcept {
    HybridTrackZones z{};
    z.stopCount = keel::max(keel::usize{1}, stopCount);
    const bool vertical = layout.vertical;

    if (vertical) {
        z.stopStep = keel::max(m.thumbSize + 10.0f, tickFont * 1.4f + 6.0f);
        z.boundaryGap = keel::max(m.thumbSize + 10.0f, tickFont * 1.4f + 8.0f);
    } else {
        keel::f32 maxStopW = 0.0f;
        keel::f32 lastStopW = 0.0f;
        for (keel::usize i = 0; i < z.stopCount; ++i) {
            const keel::f32 w = TextUtil::MeasureWidth(ResolveStopLabel(opts.stops, i), tickFont, canvas);
            maxStopW = keel::max(maxStopW, w);
            if (i + 1u == z.stopCount) {
                lastStopW = w;
            }
        }
        z.stopStep = keel::max(m.thumbSize + 10.0f, maxStopW + 10.0f);
        z.boundaryGap = keel::max(m.thumbSize + 8.0f, lastStopW * 0.5f + m.thumbSize * 0.5f + 8.0f);
    }

    const keel::f32 rawDiscrete = keel::as<keel::f32>(z.stopCount - 1u) * z.stopStep + z.boundaryGap;
    const keel::f32 maxDiscrete = layout.length * 0.45f;
    if (rawDiscrete > maxDiscrete && rawDiscrete > 1.0f) {
        const keel::f32 scale = maxDiscrete / rawDiscrete;
        z.stopStep *= scale;
        z.boundaryGap *= scale;
    }

    const keel::f32 lastStopPos = keel::as<keel::f32>(z.stopCount - 1u) * z.stopStep;
    z.contStartPos = lastStopPos + z.boundaryGap;
    z.dividerPos = lastStopPos + z.boundaryGap * 0.46f;
    z.contLen = keel::max(1.0f, layout.length - z.contStartPos);
    return z;
}

[[nodiscard]] keel::f64 QuantizeContinuous(
    keel::f64 rawVal,
    keel::f64 rangeMin,
    keel::f64 rangeMax,
    keel::f64 effectiveStep,
    bool isInt
) noexcept {
    keel::f64 v = rawVal;
    if (effectiveStep > 0.0) {
        v = rangeMin + keel::round((v - rangeMin) / effectiveStep) * effectiveStep;
    }
    v = keel::clamp(v, rangeMin, rangeMax);
    if (isInt) {
        v = keel::as<keel::f64>(keel::round_i64(v));
    }
    return v;
}

[[nodiscard]] keel::usize NearestDiscreteStop(keel::f32 travel, const HybridTrackZones& z) noexcept {
    if (z.stopCount <= 1u || z.stopStep <= 1e-4f) {
        return 0u;
    }
    const keel::f32 idxF = travel / z.stopStep;
    const keel::i64 rounded = keel::round_i64(idxF);
    return keel::clamp(
        keel::as<keel::usize>(keel::max(keel::i64{0}, rounded)),
        keel::usize{0},
        z.stopCount - 1u
    );
}

[[nodiscard]] SliderControlMetrics ComputeHybridMetrics(
    const HybridSliderOptions& opts,
    keel::usize modeIndex,
    keel::usize stopCount,
    keel::f64 value,
    keel::f64 rangeMin,
    keel::f64 rangeMax,
    bool isInt,
    const ControlSpec& ctrl,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    char* outValBuf,
    keel::usize outValBufSize,
    keel::StringRef& outCompanionValStr
) noexcept {
    const SliderMetrics& m = theme.GetSlider(size);
    const keel::f32 tickFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
    const bool inlineBottom = UseInlineBottomRow(opts);
    const bool inlineVertical = UseInlineVerticalColumn(opts);
    const keel::f32 valFont = theme.ResolveFontSize(
        opts.valueFormat,
        (inlineBottom || inlineVertical) ? TextRole::LabelSmall : TextRole::LabelMedium,
        size
    );
    const bool isManual = (modeIndex >= stopCount);

    FormatSliderValue(outValBuf, outValBufSize, value, isInt, opts.valueFormat.text);
    const keel::StringRef numericStr(outValBuf);
    const keel::StringRef stopStr = isManual ? keel::StringRef{} : ResolveStopLabel(opts.stops, modeIndex);

    const bool delegatedShowValue = opts.showValue && !inlineBottom && !inlineVertical;

    if (isManual) {
        outCompanionValStr = numericStr;
    } else if (!opts.showStopLabels) {
        outCompanionValStr = stopStr;
    } else {
        outCompanionValStr = keel::StringRef{};
    }

    const keel::f32 labelsBand = HybridSliderLabelsBand(
        opts, stopCount, rangeMin, rangeMax, isInt, m, tickFont, valFont, canvas, theme
    );

    keel::StaticArray<keel::StringRef, 1> defaultStopArr = {{"Off"}};
    const keel::ArrayRef<keel::StringRef> candidates = opts.stops.empty()
        ? defaultStopArr.as_ref()
        : opts.stops;

    return ComputeSliderControlMetrics(
        opts.textLeft,
        opts.textRight,
        opts.valueFormat,
        outCompanionValStr,
        delegatedShowValue,
        opts.valuePlacement,
        ctrl.width,
        labelsBand,
        size,
        canvas,
        theme,
        (!opts.showStopLabels) ? candidates : keel::ArrayRef<keel::StringRef>{},
        true,
        rangeMin,
        rangeMax,
        isInt,
        opts.orientation
    );
}

} // namespace

Response detail::HybridSlider::Run(
    Context& ctx,
    keel::usize* modeIndex,
    keel::usize stopCount,
    keel::f64* valDouble,
    keel::f32* valFloat,
    keel::i64* valInt,
    keel::f64 min,
    keel::f64 max,
    const HybridSliderOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme& activeTheme = ctx.GetTheme();
    const SliderMetrics& m = activeTheme.GetSlider(activeSize);
    const keel::f32 tickFont = activeTheme.ResolveFontSize(TextRole::LabelSmall, activeSize);

    const keel::usize effStopCount = keel::max(keel::usize{1}, stopCount);
    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);
    const bool isInt = (valInt != nullptr);

    keel::usize curMode = (modeIndex != nullptr) ? keel::min(*modeIndex, effStopCount) : 0u;
    keel::f64 curVal = (valDouble != nullptr) ? *valDouble :
                       (valFloat != nullptr)  ? keel::as<keel::f64>(*valFloat) :
                       (valInt != nullptr)    ? keel::as<keel::f64>(*valInt) : rangeMin;
    curVal = keel::clamp(curVal, rangeMin, rangeMax);

    const keel::f64 effectiveStep = (opts.step.has_value() && *opts.step > 0.0)
        ? *opts.step
        : (isInt ? 1.0 : 0.0);

    Rect prevBounds = ctx.GetPrevFrameBounds(id);
    auto& state = ctx.GetOrCreateState<SliderState>(id);

    ctx.RegisterFocusable(id, effectiveDisabled);

    const ControlSpec ctrl = EffectiveSliderControl(opts, activeSize, activeTheme);
    Response res = ctx.Interact(id);

    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        state.dragging = false;
    } else {
        char initValBuf[64];
        keel::StringRef companionStr{};
        ICanvas* measureCanvas = ctx.GetCanvas();
        const SliderControlMetrics cm = ComputeHybridMetrics(
            opts, curMode, effStopCount, curVal, rangeMin, rangeMax, isInt,
            ctrl, activeSize, measureCanvas, activeTheme,
            initValBuf, sizeof(initValBuf), companionStr
        );
        const SliderTrackLayout layout =
            ComputeSliderTrackLayout(prevBounds, cm, m, IsVerticalSlider(opts.orientation));
        const HybridTrackZones zones =
            ComputeHybridTrackZones(opts, effStopCount, layout, m, tickFont, measureCanvas);

        const bool justStarted = res.clicked || (res.pressed && !state.dragging);
        if (justStarted) {
            state.dragging = true;
            state.dragStartVal = curVal;
            state.dragStartMouse = ctx.GetMousePos();
            ctx.SetActiveId(id);
            ctx.RequestFocus(id);
        }

        if (state.dragging) {
            if (!ctx.IsMouseDown()) {
                state.dragging = false;
                if (ctx.GetActiveId() == id) {
                    ctx.SetActiveId(Id::Null());
                }
            } else {
                res.pressed = true;
                const keel::f32 travel = layout.Travel(ctx.GetMousePos());
                const keel::f32 lastStopPos = zones.StopPos(effStopCount - 1u);

                keel::usize nextMode = curMode;
                keel::f64 nextVal = curVal;

                if (justStarted) {
                    // Direct click: jump straight to clicked zone without hysteresis barrier
                    if (travel < zones.dividerPos) {
                        nextMode = NearestDiscreteStop(travel, zones);
                    } else {
                        nextMode = effStopCount;
                        const keel::f32 t = keel::clamp((travel - zones.contStartPos) / zones.contLen, 0.0f, 1.0f);
                        const keel::f64 raw = rangeMin + keel::as<keel::f64>(t) * (rangeMax - rangeMin);
                        nextVal = QuantizeContinuous(raw, rangeMin, rangeMax, effectiveStep, isInt);
                    }
                } else if (curMode >= effStopCount) {
                    // Dragging while in continuous range: hold at `min` until pointer crosses detent barrier
                    const keel::f32 snapBackThreshold = lastStopPos + zones.boundaryGap * 0.25f;
                    if (travel < snapBackThreshold) {
                        nextMode = NearestDiscreteStop(travel, zones);
                    } else {
                        const keel::f32 t = keel::clamp((travel - zones.contStartPos) / zones.contLen, 0.0f, 1.0f);
                        const keel::f64 raw = rangeMin + keel::as<keel::f64>(t) * (rangeMax - rangeMin);
                        nextVal = QuantizeContinuous(raw, rangeMin, rangeMax, effectiveStep, isInt);
                    }
                } else {
                    // Dragging while on a discrete stop: require crossing breakout threshold to enter continuous range
                    const keel::f32 breakoutThreshold = lastStopPos + zones.boundaryGap * 0.65f;
                    if (travel >= breakoutThreshold) {
                        nextMode = effStopCount;
                        const keel::f32 t = keel::clamp((travel - zones.contStartPos) / zones.contLen, 0.0f, 1.0f);
                        const keel::f64 raw = rangeMin + keel::as<keel::f64>(t) * (rangeMax - rangeMin);
                        nextVal = QuantizeContinuous(raw, rangeMin, rangeMax, effectiveStep, isInt);
                    } else {
                        nextMode = NearestDiscreteStop(travel, zones);
                    }
                }

                const bool modeChanged = (nextMode != curMode);
                const bool valChanged = (nextMode >= effStopCount) && (keel::abs(nextVal - curVal) > 1e-9);
                if (modeChanged || valChanged) {
                    curMode = nextMode;
                    if (nextMode >= effStopCount) {
                        curVal = nextVal;
                    }
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            }
        }

        const keel::f64 deltaStep = (effectiveStep > 0.0)
            ? effectiveStep
            : (rangeMax - rangeMin) * 0.05;

        auto stepForward = [&]() {
            if (curMode < effStopCount) {
                ++curMode;
                if (curMode >= effStopCount) {
                    curVal = QuantizeContinuous(rangeMin, rangeMin, rangeMax, effectiveStep, isInt);
                }
                res.changed = true;
                ctx.RequestRepaint(3);
            } else {
                const keel::f64 nextVal = QuantizeContinuous(curVal + deltaStep, rangeMin, rangeMax, effectiveStep, isInt);
                if (keel::abs(nextVal - curVal) > 1e-9) {
                    curVal = nextVal;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            }
        };

        auto stepBackward = [&]() {
            if (curMode >= effStopCount) {
                if (curVal <= rangeMin + 1e-9) {
                    curMode = effStopCount - 1u;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                } else {
                    const keel::f64 nextVal = QuantizeContinuous(curVal - deltaStep, rangeMin, rangeMax, effectiveStep, isInt);
                    if (keel::abs(nextVal - curVal) > 1e-9) {
                        curVal = nextVal;
                        res.changed = true;
                        ctx.RequestRepaint(3);
                    }
                }
            } else if (curMode > 0u) {
                --curMode;
                res.changed = true;
                ctx.RequestRepaint(3);
            }
        };

        // Mouse wheel adjustment
        if (res.hovered && !state.dragging) {
            const Vec2 wheel = ctx.GetMouseWheel();
            const keel::f32 wheelDelta = (wheel.x != 0.0f) ? wheel.x : wheel.y;
            if (wheelDelta > 0.0f) {
                stepForward();
            } else if (wheelDelta < 0.0f) {
                stepBackward();
            }
        }

        // Keyboard adjustment when focused
        if (ctx.IsFocused(id) && !state.dragging) {
            if (ctx.ConsumeKey(Key::Right) || ctx.ConsumeKey(Key::Up)) {
                stepForward();
            } else if (ctx.ConsumeKey(Key::Left) || ctx.ConsumeKey(Key::Down)) {
                stepBackward();
            } else if (ctx.ConsumeKey(Key::Home)) {
                if (curMode != 0u) {
                    curMode = 0u;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            } else if (ctx.ConsumeKey(Key::End)) {
                const keel::f64 endVal = QuantizeContinuous(rangeMax, rangeMin, rangeMax, effectiveStep, isInt);
                if (curMode != effStopCount || keel::abs(endVal - curVal) > 1e-9) {
                    curMode = effStopCount;
                    curVal = endVal;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            }
        }
    }

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, prevBounds);
    }

    if (modeIndex != nullptr) *modeIndex = curMode;
    if (curMode >= effStopCount) {
        if (valDouble != nullptr) *valDouble = curVal;
        if (valFloat != nullptr)  *valFloat = keel::as<keel::f32>(curVal);
        if (valInt != nullptr)    *valInt = keel::round_i64(curVal);
    }

    Node* node = ctx.CreateNode(NodeType::HybridSlider, id);
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed || state.dragging;
    node->focused = ctx.IsFocusVisible(id);

    auto* data = ctx.AllocPayload<HybridSliderData>();
    data->opts = opts;
    data->opts.textLeft = ctx.AllocText(opts.textLeft);
    data->opts.textRight = ctx.AllocText(opts.textRight);
    data->opts.valueFormat = ctx.AllocText(opts.valueFormat);
    data->opts.tooltip = ctx.AllocText(opts.tooltip);

    // Copy discrete stop labels into frame arena so caller temporaries never dangle
    auto* arenaStops = ctx.GetArena().alloc_array<keel::StringRef>(effStopCount);
    for (keel::usize i = 0; i < effStopCount; ++i) {
        arenaStops[i] = ctx.AllocString(ResolveStopLabel(opts.stops, i));
    }
    data->opts.stops = keel::ArrayRef<keel::StringRef>(arenaStops, effStopCount);

    data->modeIndex = curMode;
    data->stopCount = effStopCount;
    data->value = curVal;
    data->min = rangeMin;
    data->max = rangeMax;
    data->isInt = isInt;
    node->SetPayload(data);

    char finalValBuf[64];
    FormatSliderValue(finalValBuf, sizeof(finalValBuf), curVal, isInt, data->opts.valueFormat.text);
    node->text = (curMode < effStopCount)
        ? data->opts.stops[curMode]
        : ctx.AllocString(keel::StringRef(finalValBuf));

    keel::u64 stopsHash = effStopCount;
    for (keel::usize i = 0; i < effStopCount; ++i) {
        stopsHash = detail::HashArgs(stopsHash, data->opts.stops[i]);
    }
    node->visualHash = detail::HashArgs(
        curMode,
        stopsHash,
        curVal,
        rangeMin,
        rangeMax,
        isInt,
        keel::as<keel::u8>(data->opts.orientation),
        data->opts.color,
        keel::as<keel::u8>(data->opts.valuePlacement),
        data->opts.width,
        data->opts.height,
        data->opts.minWidth.value_or(0.0f),
        data->opts.maxWidth.value_or(0.0f),
        data->opts.minHeight.value_or(0.0f),
        data->opts.maxHeight.value_or(0.0f),
        data->opts.showTicks,
        data->opts.showStopLabels,
        data->opts.showValue,
        data->opts.textLeft,
        data->opts.textRight,
        data->opts.valueFormat
    );

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<HybridSliderData>();
        if (!d) return Vec2{};
        return detail::HybridSlider::Measure(
            d->opts, d->modeIndex, d->stopCount, d->value, d->min, d->max, d->isInt,
            n->uiSize, canvas, theme, availableWidth
        );
    };

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<HybridSliderData>();
        if (!d) return;
        detail::HybridSlider::Render(
            canvas, n->computedRect, d->modeIndex, d->stopCount, d->value, d->min, d->max, d->isInt,
            d->opts, n->uiSize, n->hovered, n->pressed, n->disabled, theme, n->focused
        );
    };

    Control::SetupNode(ctx, node, ctrl);
    return res;
}

Vec2 detail::HybridSlider::Measure(
    const HybridSliderOptions& opts,
    keel::usize modeIndex,
    keel::usize stopCount,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
    bool isInt,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 /*availableWidth*/
) {
    const keel::usize effStopCount = keel::max(keel::usize{1}, stopCount);
    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);
    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    char valBuf[64];
    keel::StringRef companionStr{};
    const SliderControlMetrics cm = ComputeHybridMetrics(
        opts, keel::min(modeIndex, effStopCount), effStopCount,
        keel::clamp(value, rangeMin, rangeMax), rangeMin, rangeMax, isInt,
        ctrl, size, canvas, theme, valBuf, sizeof(valBuf), companionStr
    );
    return Control::Measure(cm.intrinsicControlSize, ctrl, theme);
}

Vec2 detail::HybridSlider::Measure(
    const HybridSliderOptions& opts,
    keel::usize modeIndex,
    keel::usize stopCount,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
    bool isInt,
    UiSize size,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, modeIndex, stopCount, value, min, max, isInt, size, canvas, defaultTheme, availableWidth);
}

void detail::HybridSlider::Render(
    ICanvas& canvas,
    Rect bounds,
    keel::usize modeIndex,
    keel::usize stopCount,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
    bool isInt,
    const HybridSliderOptions& opts,
    UiSize size,
    bool hovered,
    bool active,
    bool disabled,
    const UiTheme& theme,
    bool focused
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const keel::usize effStopCount = keel::max(keel::usize{1}, stopCount);
    const keel::usize curMode = keel::min(modeIndex, effStopCount);
    const bool isManual = (curMode >= effStopCount);
    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);
    const keel::f64 curVal = keel::clamp(value, rangeMin, rangeMax);

    const SliderMetrics& m = theme.GetSlider(size);
    const keel::f32 tickFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
    const auto& pal = theme.palette;

    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    char valBuf[64];
    keel::StringRef companionStr{};
    const SliderControlMetrics cm = ComputeHybridMetrics(
        opts, curMode, effStopCount, curVal, rangeMin, rangeMax, isInt,
        ctrl, size, &canvas, theme, valBuf, sizeof(valBuf), companionStr
    );

    const Rect ctrlRect = bounds;
    const bool vertical = IsVerticalSlider(opts.orientation);
    const SliderTrackLayout layout = ComputeSliderTrackLayout(ctrlRect, cm, m, vertical);
    const HybridTrackZones zones = ComputeHybridTrackZones(opts, effStopCount, layout, m, tickFont, &canvas);
    const keel::f32 halfThumb = m.thumbSize * 0.5f;

    const keel::f32 thumbTravel = zones.ThumbPos(curMode, curVal, rangeMin, rangeMax);
    const keel::f32 thumbRatio = (layout.length > 1e-4f) ? (thumbTravel / layout.length) : 0.0f;
    const Vec2 thumbCenter = layout.ThumbCenter(thumbRatio);

    const bool inlineBottom = UseInlineBottomRow(opts);
    const bool inlineVertical = UseInlineVerticalColumn(opts);
    const bool delegatedShowValue = opts.showValue && !inlineBottom && !inlineVertical;

    // 0. Companion texts & non-inline value readout
    if (vertical) {
        RenderSliderCompanionTextsVertical(
            canvas, ctrlRect, layout, thumbCenter, m.thumbSize,
            cm, opts.textLeft, opts.textRight, opts.valueFormat, companionStr,
            delegatedShowValue, opts.valuePlacement, disabled, theme
        );
    } else {
        RenderSliderCompanionTexts(
            canvas, ctrlRect, layout.region.x, layout.region.w, layout.region.y, thumbCenter.x,
            cm, opts.textLeft, opts.textRight, opts.valueFormat, companionStr,
            delegatedShowValue, opts.valuePlacement, disabled, theme
        );
    }

    // 1. Single continuous full-length track groove + active fill via active style painter
    const keel::f32 effTrackH = theme.Painter().GetSliderTrackHeight(theme, m.trackHeight, /*isRange=*/false);
    const keel::f32 effHalfTrack = effTrackH * 0.5f;
    const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(theme.disabledOpacity) : pal.surfaceActive;
    const Color trackBorder = disabled ? pal.border.WithAlphaF(theme.disabledOpacity) : pal.border;
    const Color fillCol = ResolveSliderTrackColor(opts.color, disabled, theme);

    const Rect fullTrackRect = vertical
        ? Rect(keel::floor(layout.cross - effHalfTrack), layout.start - layout.length, effTrackH, layout.length)
        : Rect(layout.start, keel::floor(layout.cross - effHalfTrack), layout.length, effTrackH);
    const bool hasFill = (curMode > 0u) && (thumbTravel > 0.0f);
    const Rect fillRect = vertical
        ? Rect(fullTrackRect.x, thumbCenter.y, effTrackH, hasFill ? thumbTravel : 0.0f)
        : Rect(layout.start, fullTrackRect.y, hasFill ? thumbTravel : 0.0f, effTrackH);
    theme.Painter().DrawSliderTrack(
        canvas, theme, fullTrackRect, fillRect, vertical, hasFill,
        trackBg, trackBorder, fillCol, m.cornerRadius, opts.color.HasAny()
    );

    // 2. Discrete stop tick marks + subtle boundary tick at continuous `min` right on the track
    if (opts.showTicks) {
        for (keel::usize i = 0; i < effStopCount; ++i) {
            const keel::f32 stopRatio = (layout.length > 1e-4f) ? (zones.StopPos(i) / layout.length) : 0.0f;
            const Vec2 tickCenter = layout.ThumbCenter(stopRatio);
            const bool insideFill = hasFill && (zones.StopPos(i) <= thumbTravel + 0.5f);
            Color tickCol = insideFill ? pal.surface : pal.textDisabled;
            if (disabled) tickCol = tickCol.WithAlphaF(theme.disabledOpacity);
            theme.Painter().DrawSliderTick(
                canvas, theme, tickCenter, layout.cross, vertical,
                m.tickSize, m.thumbSize, tickCol, disabled
            );
        }

        // Subtle boundary tick marking the start of the continuous `[min .. max]` range
        const keel::f32 minRatio = (layout.length > 1e-4f) ? (zones.contStartPos / layout.length) : 0.0f;
        const Vec2 minTickCenter = layout.ThumbCenter(minRatio);
        const bool minInsideFill = hasFill && (zones.contStartPos <= thumbTravel + 0.5f);
        Color minTickCol = minInsideFill ? pal.surface.WithAlphaF(0.85f) : pal.textDisabled.WithAlphaF(0.75f);
        if (disabled) minTickCol = minTickCol.WithAlphaF(theme.disabledOpacity);
        theme.Painter().DrawSliderTick(
            canvas, theme, minTickCenter, layout.cross, vertical,
            keel::max(2.5f, m.tickSize * 0.75f), m.thumbSize, minTickCol, disabled
        );
    }

    // 3. Thumb knob via active style painter (muted neutral on stop 0 "Off", semantic accent otherwise)
    Color thumbAccent = active ? ResolveSliderActiveThumbColor(opts.color, theme) : fillCol;
    if (!isManual && curMode == 0u) {
        thumbAccent = disabled ? pal.textDisabled : pal.textSecondary;
    }
    theme.Painter().DrawSliderThumb(canvas, theme, SliderThumbPaintParams{
        .center       = thumbCenter,
        .thumbSize    = m.thumbSize,
        .vertical     = vertical,
        .accent       = thumbAccent,
        .hovered      = hovered,
        .active       = active,
        .focused      = focused,
        .disabled     = disabled,
        .cornerRadius = m.cornerRadius
    });

    // 4. Discrete stop labels + inline bottom-right (or vertical top-right) manual value readout
    if (opts.showStopLabels) {
        const keel::f32 minGap = 6.0f;
        auto stopLabelColor = [&](keel::usize i) {
            if (disabled) return pal.textDisabled;
            if (i == curMode) {
                return (i == 0u) ? pal.textPrimary : pal.accent;
            }
            return pal.textSecondary;
        };

        if (vertical) {
            const keel::f32 labelsX = layout.region.x + layout.region.w + m.tickGap;
            const keel::f32 regionTop = layout.region.y;
            const keel::f32 regionBottom = layout.region.y + layout.region.h;
            keel::f32 prevTop = 1e9f;
            for (keel::usize i = 0; i < effStopCount; ++i) {
                const keel::StringRef lbl = ResolveStopLabel(opts.stops, i);
                const keel::f32 h = canvas.MeasureText(lbl, tickFont).y;
                const keel::f32 tickY = layout.start - zones.StopPos(i);
                const keel::f32 top = keel::clamp(tickY - h * 0.5f, regionTop, keel::max(regionTop, regionBottom - h));
                if (i > 0u && top + h + 4.0f > prevTop) continue;
                canvas.DrawText(lbl, Vec2(labelsX, top), tickFont, stopLabelColor(i));
                prevTop = top;
            }
            if (inlineVertical && opts.showValue && isManual) {
                const keel::f32 valFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelSmall, size);
                const keel::f32 valH = canvas.GetLineHeight(valFont);
                const Color valCol = theme.ResolveTextColor(opts.valueFormat, UiVariant::Secondary, disabled);
                if (regionTop + valH + 4.0f <= prevTop) {
                    canvas.DrawText(keel::StringRef(valBuf), Vec2(labelsX, regionTop), valFont, valCol);
                }
            }
        } else {
            const keel::f32 trackRegionX = layout.region.x;
            const keel::f32 trackRegionW = layout.region.w;
            const keel::f32 labelsY = layout.cross + halfThumb + m.tickGap;
            keel::f32 prevRight = -1e9f;

            for (keel::usize i = 0; i < effStopCount; ++i) {
                const keel::StringRef lbl = ResolveStopLabel(opts.stops, i);
                const Vec2 dims = canvas.MeasureText(lbl, tickFont);
                const keel::f32 tickX = layout.start + zones.StopPos(i);
                keel::f32 lblX = tickX - dims.x * 0.5f;
                lblX = keel::clamp(lblX, trackRegionX, keel::max(trackRegionX, trackRegionX + trackRegionW - dims.x));
                if (i > 0u && lblX < prevRight + minGap) continue;
                canvas.DrawText(lbl, Vec2(lblX, labelsY), tickFont, stopLabelColor(i));
                prevRight = lblX + dims.x;
            }

            if (inlineBottom && opts.showValue && isManual) {
                const keel::f32 valFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelSmall, size);
                const Vec2 valDims = canvas.MeasureText(keel::StringRef(valBuf), valFont);
                const keel::f32 valX = trackRegionX + trackRegionW - valDims.x;
                if (valX >= prevRight + minGap) {
                    const Color valCol = theme.ResolveTextColor(opts.valueFormat, UiVariant::Secondary, disabled);
                    canvas.DrawText(keel::StringRef(valBuf), Vec2(valX, labelsY), valFont, valCol);
                }
            }
        }
    }
}

} // namespace guikit
