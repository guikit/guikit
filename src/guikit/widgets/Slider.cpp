#include "guikit/widgets/Slider.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

namespace {

struct SliderData {
    SliderOptions opts;
    double value{0.0};
    double min{0.0};
    double max{1.0};
    std::string_view formattedValue{};
};

struct StepSliderData {
    StepSliderOptions opts;
    size_t index{0};
    size_t totalSteps{1};
};

} // namespace

// ============================================================================
// Slider (Continuous / Stepped Numeric Slider)
// ============================================================================

Response Slider::Run(
    Context& ctx,
    double* valDouble,
    float* valFloat,
    int64_t* valInt,
    double min,
    double max,
    const SliderOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());

    UiTheme* pushedTheme = nullptr;
    if (opts.theme != nullptr || opts.scale.has_value()) {
        UiTheme scopedTheme = opts.theme ? *opts.theme : ctx.GetTheme();
        if (opts.scale.has_value() && *opts.scale > 0.0f) {
            scopedTheme = scopedTheme.Scaled(*opts.scale);
        }
        pushedTheme = ctx.PushTheme(scopedTheme);
    }

    const SliderMetrics& m = ctx.GetTheme().GetSlider(activeSize);
    const double rangeMin = std::min(min, max);
    const double rangeMax = std::max(min, max);

    double currentVal = (valDouble != nullptr) ? *valDouble :
                        (valFloat != nullptr)  ? static_cast<double>(*valFloat) :
                        (valInt != nullptr)    ? static_cast<double>(*valInt) : rangeMin;

    currentVal = std::clamp(currentVal, rangeMin, rangeMax);

    const double effectiveStep = (opts.step.has_value() && *opts.step > 0.0) ? *opts.step :
                                 (valInt != nullptr) ? 1.0 : 0.0;

    Rect prevBounds = ctx.GetPrevFrameBounds(id);
    auto& state = ctx.GetOrCreateState<SliderState>(id);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        state.dragging = false;
    } else {
        const float halfThumb = m.thumbSize * 0.5f;
        const float trackStartX = prevBounds.x + halfThumb;
        const float trackWidth = std::max(1.0f, prevBounds.w - m.thumbSize);

        if (res.clicked || (res.pressed && !state.dragging)) {
            state.dragging = true;
            ctx.SetActiveId(id);
        }

        if (state.dragging) {
            if (!ctx.IsMouseDown()) {
                state.dragging = false;
                if (ctx.GetActiveId() == id) {
                    ctx.SetActiveId(Id::Null());
                }
            } else {
                res.pressed = true;
                const float mouseX = ctx.GetMousePos().x;
                const float t = std::clamp((mouseX - trackStartX) / trackWidth, 0.0f, 1.0f);
                double rawVal = rangeMin + t * (rangeMax - rangeMin);

                if (effectiveStep > 0.0) {
                    rawVal = rangeMin + std::round((rawVal - rangeMin) / effectiveStep) * effectiveStep;
                }
                rawVal = std::clamp(rawVal, rangeMin, rangeMax);

                if (rawVal != currentVal) {
                    currentVal = rawVal;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            }
        }

        // Mouse wheel adjustment
        if (res.hovered && !state.dragging) {
            const Vec2 wheel = ctx.GetMouseWheel();
            const float wheelDelta = (wheel.x != 0.0f) ? wheel.x : wheel.y;
            if (wheelDelta != 0.0f) {
                const double stepDelta = (effectiveStep > 0.0) ? effectiveStep : (rangeMax - rangeMin) * 0.05;
                double rawVal = currentVal + wheelDelta * stepDelta;
                rawVal = std::clamp(rawVal, rangeMin, rangeMax);
                if (rawVal != currentVal) {
                    currentVal = rawVal;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            }
        }
    }

    // Write back updated value
    if (valDouble != nullptr) *valDouble = currentVal;
    if (valFloat != nullptr)  *valFloat = static_cast<float>(currentVal);
    if (valInt != nullptr)    *valInt = static_cast<int64_t>(std::round(currentVal));

    // Format current value string
    char valBuf[64];
    if (!opts.format.empty()) {
        std::string fmt(opts.format);
        std::snprintf(valBuf, sizeof(valBuf), fmt.c_str(), currentVal);
    } else if (valInt != nullptr) {
        std::snprintf(valBuf, sizeof(valBuf), "%lld", static_cast<long long>(std::round(currentVal)));
    } else {
        if (std::abs(currentVal - std::round(currentVal)) < 0.0001) {
            std::snprintf(valBuf, sizeof(valBuf), "%.0f", currentVal);
        } else {
            std::snprintf(valBuf, sizeof(valBuf), "%.1f", currentVal);
        }
    }

    if (!opts.suffix.empty()) {
        const size_t len = std::strlen(valBuf);
        std::snprintf(valBuf + len, sizeof(valBuf) - len, " %.*s",
                      static_cast<int>(opts.suffix.size()), opts.suffix.data());
    }

    const std::string_view formattedStr = ctx.AllocString(valBuf);

    Node* node = ctx.CreateNode(NodeType::Slider, id);
    node->theme = pushedTheme;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed || state.dragging;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fit;

    auto* data = ctx.AllocPayload<SliderData>();
    data->opts = opts;
    data->opts.label = ctx.AllocString(opts.label);
    if (!opts.description.empty()) {
        data->opts.description = ctx.AllocString(opts.description);
    }
    data->value = currentVal;
    data->min = rangeMin;
    data->max = rangeMax;
    data->formattedValue = formattedStr;
    node->payload = data;

    uint64_t valBits = 0;
    std::memcpy(&valBits, &currentVal, sizeof(double));
    node->visualHash = detail::HashArgs(
        valBits,
        opts.variant,
        opts.showValue,
        opts.format,
        opts.suffix,
        data->opts.label,
        data->opts.description,
        data->formattedValue
    );

    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<SliderData>();
        return Slider::Measure(d ? d->opts : SliderOptions{},
                               d ? d->value : 0.0,
                               d ? d->min : 0.0,
                               d ? d->max : 1.0,
                               n->uiSize, canvas, theme);
    };

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<SliderData>();
        if (!d) return;
        Slider::Render(canvas, n->computedRect, d->value, d->min, d->max,
                       d->opts, n->uiSize, n->hovered, n->pressed, n->disabled, theme);
    };

    ctx.AdaptChildToContainer(node);
    return res;
}

Vec2 Slider::Measure(
    const SliderOptions& opts,
    double /*value*/,
    double /*min*/,
    double /*max*/,
    UiSize size,
    ICanvas* /*canvas*/,
    const UiTheme& theme
) {
    const SliderMetrics& m = theme.GetSlider(size);
    float totalH = std::max(m.defaultHeight, m.thumbSize);

    const bool hasHeader = !opts.label.empty() || opts.showValue;
    if (hasHeader) {
        totalH += m.labelFontSize * 1.3f + m.labelGap;
    }
    if (!opts.description.empty()) {
        totalH += m.descGap + m.labelFontSize * 1.1f;
    }

    return Vec2{100.0f, totalH};
}

Vec2 Slider::Measure(
    const SliderOptions& opts,
    double value,
    double min,
    double max,
    UiSize size,
    ICanvas* canvas
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, value, min, max, size, canvas, defaultTheme);
}

void Slider::Render(
    ICanvas& canvas,
    Rect bounds,
    double value,
    double min,
    double max,
    const SliderOptions& opts,
    UiSize size,
    bool hovered,
    bool active,
    bool disabled,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const SliderMetrics& m = theme.GetSlider(size);
    const auto& pal = theme.palette;
    const VariantColors& variantCol = theme.GetVariant(opts.variant);

    float curY = bounds.y;
    const bool hasHeader = !opts.label.empty() || opts.showValue;

    if (hasHeader) {
        const float headerH = m.labelFontSize * 1.3f;
        if (!opts.label.empty()) {
            Color labelCol = disabled ? pal.textDisabled : pal.textPrimary;
            canvas.DrawText(opts.label, Vec2(bounds.x, curY), m.labelFontSize, labelCol);
        }

        if (opts.showValue) {
            char valBuf[64];
            if (!opts.format.empty()) {
                std::string fmt(opts.format);
                std::snprintf(valBuf, sizeof(valBuf), fmt.c_str(), value);
            } else {
                if (std::abs(value - std::round(value)) < 0.0001) {
                    std::snprintf(valBuf, sizeof(valBuf), "%.0f", value);
                } else {
                    std::snprintf(valBuf, sizeof(valBuf), "%.1f", value);
                }
            }
            if (!opts.suffix.empty()) {
                const size_t len = std::strlen(valBuf);
                std::snprintf(valBuf + len, sizeof(valBuf) - len, " %.*s",
                              static_cast<int>(opts.suffix.size()), opts.suffix.data());
            }

            const Vec2 valDims = canvas.MeasureText(valBuf, m.labelFontSize);
            const Color valCol = disabled ? pal.textDisabled : pal.textSecondary;
            canvas.DrawText(valBuf, Vec2(bounds.Right() - valDims.x, curY), m.labelFontSize, valCol);
        }
        curY += headerH + m.labelGap;
    }

    const float trackRowH = std::max(m.defaultHeight, m.thumbSize);
    const float centerY = curY + trackRowH * 0.5f;
    const float halfThumb = m.thumbSize * 0.5f;
    const float trackStartX = bounds.x + halfThumb;
    const float trackWidth = std::max(0.0f, bounds.w - m.thumbSize);

    const double range = max - min;
    const float ratio = (range > 0.0) ? static_cast<float>(std::clamp((value - min) / range, 0.0, 1.0)) : 0.0f;

    // 1. Draw inactive background track
    const Rect trackBgRect(trackStartX, centerY - m.trackHeight * 0.5f, trackWidth, m.trackHeight);
    Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
    Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
    canvas.DrawRoundRect(trackBgRect, m.trackHeight * 0.5f, trackBg, trackBorder, 1.0f);

    // 2. Draw active filled track segment
    const float fillW = ratio * trackWidth;
    if (fillW > 0.0f) {
        const Rect fillRect(trackStartX, centerY - m.trackHeight * 0.5f, fillW, m.trackHeight);
        Color fillCol = disabled ? variantCol.bg.WithAlphaF(0.4f) : variantCol.bg;
        canvas.DrawRoundRect(fillRect, m.trackHeight * 0.5f, fillCol);
    }

    // 3. Draw thumb knob
    const float thumbCenterX = trackStartX + fillW;
    const Rect thumbRect(thumbCenterX - halfThumb, centerY - halfThumb, m.thumbSize, m.thumbSize);
    Color thumbBg = disabled ? pal.surface.WithAlphaF(0.5f) :
                    active   ? variantCol.bgActive :
                    hovered  ? pal.surface : pal.surface;
    Color thumbBorder = disabled ? pal.border.WithAlphaF(0.4f) :
                        (active || hovered) ? pal.accent : pal.borderHover;
    const float thumbBorderW = (active || hovered) ? 2.5f : 2.0f;
    canvas.DrawRoundRect(thumbRect, halfThumb, thumbBg, thumbBorder, thumbBorderW);

    // 4. Description text
    if (!opts.description.empty()) {
        curY += trackRowH + m.descGap;
        Color descCol = disabled ? pal.textDisabled : pal.textSecondary;
        canvas.DrawText(opts.description, Vec2(bounds.x, curY), m.labelFontSize * 0.9f, descCol);
    }
}

// ============================================================================
// StepSlider (Discrete N-Position Step Slider)
// ============================================================================

Response StepSlider::Run(
    Context& ctx,
    size_t* indexSizeT,
    int* indexInt,
    size_t totalSteps,
    const StepSliderOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());

    UiTheme* pushedTheme = nullptr;
    if (opts.theme != nullptr || opts.scale.has_value()) {
        UiTheme scopedTheme = opts.theme ? *opts.theme : ctx.GetTheme();
        if (opts.scale.has_value() && *opts.scale > 0.0f) {
            scopedTheme = scopedTheme.Scaled(*opts.scale);
        }
        pushedTheme = ctx.PushTheme(scopedTheme);
    }

    const SliderMetrics& m = ctx.GetTheme().GetSlider(activeSize);

    if (totalSteps == 0) {
        totalSteps = opts.stepLabels.empty() ? 1 : opts.stepLabels.size();
    }

    size_t curIdx = (indexSizeT != nullptr) ? *indexSizeT :
                    (indexInt != nullptr)   ? static_cast<size_t>(std::max(0, *indexInt)) : 0;
    curIdx = std::min(curIdx, totalSteps - 1);

    Rect prevBounds = ctx.GetPrevFrameBounds(id);
    auto& state = ctx.GetOrCreateState<SliderState>(id);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        state.dragging = false;
    } else {
        const float halfThumb = m.thumbSize * 0.5f;
        const float trackStartX = prevBounds.x + halfThumb;
        const float trackWidth = std::max(1.0f, prevBounds.w - m.thumbSize);

        if (res.clicked || (res.pressed && !state.dragging)) {
            state.dragging = true;
            ctx.SetActiveId(id);
        }

        if (state.dragging) {
            if (!ctx.IsMouseDown()) {
                state.dragging = false;
                if (ctx.GetActiveId() == id) {
                    ctx.SetActiveId(Id::Null());
                }
            } else {
                res.pressed = true;
                const float mouseX = ctx.GetMousePos().x;
                const float t = std::clamp((mouseX - trackStartX) / trackWidth, 0.0f, 1.0f);
                const size_t newIdx = (totalSteps > 1)
                    ? std::clamp(static_cast<size_t>(std::round(t * static_cast<float>(totalSteps - 1))), size_t{0}, totalSteps - 1)
                    : 0;

                if (newIdx != curIdx) {
                    curIdx = newIdx;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            }
        }

        // Mouse wheel adjustment
        if (res.hovered && !state.dragging) {
            const Vec2 wheel = ctx.GetMouseWheel();
            const float wheelDelta = (wheel.x != 0.0f) ? wheel.x : wheel.y;
            if (wheelDelta != 0.0f) {
                if (wheelDelta > 0.0f && curIdx + 1 < totalSteps) {
                    curIdx++;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                } else if (wheelDelta < 0.0f && curIdx > 0) {
                    curIdx--;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            }
        }
    }

    if (indexSizeT != nullptr) *indexSizeT = curIdx;
    if (indexInt != nullptr)   *indexInt = static_cast<int>(curIdx);

    Node* node = ctx.CreateNode(NodeType::StepSlider, id);
    node->theme = pushedTheme;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed || state.dragging;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fit;

    auto* data = ctx.AllocPayload<StepSliderData>();
    data->opts = opts;
    data->opts.label = ctx.AllocString(opts.label);
    if (!opts.description.empty()) {
        data->opts.description = ctx.AllocString(opts.description);
    }
    data->index = curIdx;
    data->totalSteps = totalSteps;
    node->payload = data;

    node->visualHash = detail::HashArgs(
        curIdx,
        totalSteps,
        opts.variant,
        opts.showTicks,
        opts.showLabels,
        opts.showValue,
        data->opts.label,
        data->opts.description
    );

    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<StepSliderData>();
        return StepSlider::Measure(d ? d->opts : StepSliderOptions{},
                                   d ? d->index : 0,
                                   d ? d->totalSteps : 1,
                                   n->uiSize, canvas, theme);
    };

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<StepSliderData>();
        if (!d) return;
        StepSlider::Render(canvas, n->computedRect, d->index, d->totalSteps,
                           d->opts, n->uiSize, n->hovered, n->pressed, n->disabled, theme);
    };

    ctx.AdaptChildToContainer(node);
    return res;
}

Vec2 StepSlider::Measure(
    const StepSliderOptions& opts,
    size_t /*index*/,
    size_t /*totalSteps*/,
    UiSize size,
    ICanvas* /*canvas*/,
    const UiTheme& theme
) {
    const SliderMetrics& m = theme.GetSlider(size);
    float totalH = std::max(m.defaultHeight, m.thumbSize);

    const bool hasHeader = !opts.label.empty() || opts.showValue;
    if (hasHeader) {
        totalH += m.labelFontSize * 1.3f + m.labelGap;
    }
    if (opts.showLabels && !opts.stepLabels.empty()) {
        totalH += m.tickGap + m.tickFontSize * 1.3f;
    }
    if (!opts.description.empty()) {
        totalH += m.descGap + m.labelFontSize * 1.1f;
    }

    return Vec2{100.0f, totalH};
}

Vec2 StepSlider::Measure(
    const StepSliderOptions& opts,
    size_t index,
    size_t totalSteps,
    UiSize size,
    ICanvas* canvas
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, index, totalSteps, size, canvas, defaultTheme);
}

void StepSlider::Render(
    ICanvas& canvas,
    Rect bounds,
    size_t index,
    size_t totalSteps,
    const StepSliderOptions& opts,
    UiSize size,
    bool hovered,
    bool active,
    bool disabled,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const SliderMetrics& m = theme.GetSlider(size);
    const auto& pal = theme.palette;
    const VariantColors& variantCol = theme.GetVariant(opts.variant);

    float curY = bounds.y;
    const bool hasHeader = !opts.label.empty() || opts.showValue;

    if (hasHeader) {
        const float headerH = m.labelFontSize * 1.3f;
        if (!opts.label.empty()) {
            Color labelCol = disabled ? pal.textDisabled : pal.textPrimary;
            canvas.DrawText(opts.label, Vec2(bounds.x, curY), m.labelFontSize, labelCol);
        }

        if (opts.showValue && index < opts.stepLabels.size()) {
            std::string_view valStr = opts.stepLabels[index];
            const Vec2 valDims = canvas.MeasureText(valStr, m.labelFontSize);
            const Color valCol = disabled ? pal.textDisabled : pal.accent;
            canvas.DrawText(valStr, Vec2(bounds.Right() - valDims.x, curY), m.labelFontSize, valCol);
        }
        curY += headerH + m.labelGap;
    }

    const float trackRowH = std::max(m.defaultHeight, m.thumbSize);
    const float centerY = curY + trackRowH * 0.5f;
    const float halfThumb = m.thumbSize * 0.5f;
    const float trackStartX = bounds.x + halfThumb;
    const float trackWidth = std::max(0.0f, bounds.w - m.thumbSize);

    const float ratio = (totalSteps > 1) ? static_cast<float>(index) / static_cast<float>(totalSteps - 1) : 0.0f;

    // 1. Draw inactive background track
    const Rect trackBgRect(trackStartX, centerY - m.trackHeight * 0.5f, trackWidth, m.trackHeight);
    Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
    Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
    canvas.DrawRoundRect(trackBgRect, m.trackHeight * 0.5f, trackBg, trackBorder, 1.0f);

    // 2. Draw active filled track segment
    const float fillW = ratio * trackWidth;
    if (fillW > 0.0f) {
        const Rect fillRect(trackStartX, centerY - m.trackHeight * 0.5f, fillW, m.trackHeight);
        Color fillCol = disabled ? variantCol.bg.WithAlphaF(0.4f) : variantCol.bg;
        canvas.DrawRoundRect(fillRect, m.trackHeight * 0.5f, fillCol);
    }

    // 3. Draw tick dots on track
    if (opts.showTicks && totalSteps > 1) {
        const float tickR = m.tickSize * 0.5f;
        for (size_t i = 0; i < totalSteps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(totalSteps - 1);
            const float tickX = trackStartX + t * trackWidth;
            const Rect tickRect(tickX - tickR, centerY - tickR, m.tickSize, m.tickSize);
            Color tickCol = (i <= index) ? pal.surface : pal.textDisabled;
            if (disabled) tickCol = tickCol.WithAlphaF(0.4f);
            canvas.DrawRoundRect(tickRect, tickR, tickCol);
        }
    }

    // 4. Draw thumb knob
    const float thumbCenterX = trackStartX + fillW;
    const Rect thumbRect(thumbCenterX - halfThumb, centerY - halfThumb, m.thumbSize, m.thumbSize);
    Color thumbBg = disabled ? pal.surface.WithAlphaF(0.5f) :
                    active   ? variantCol.bgActive :
                    hovered  ? pal.surface : pal.surface;
    Color thumbBorder = disabled ? pal.border.WithAlphaF(0.4f) :
                        (active || hovered) ? pal.accent : pal.borderHover;
    const float thumbBorderW = (active || hovered) ? 2.5f : 2.0f;
    canvas.DrawRoundRect(thumbRect, halfThumb, thumbBg, thumbBorder, thumbBorderW);

    // 5. Draw step labels below track
    if (opts.showLabels && !opts.stepLabels.empty() && totalSteps > 1) {
        const float labelsY = centerY + halfThumb + m.tickGap;
        for (size_t i = 0; i < std::min(totalSteps, opts.stepLabels.size()); ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(totalSteps - 1);
            const float tickX = trackStartX + t * trackWidth;
            const std::string_view lbl = opts.stepLabels[i];
            const Vec2 dims = canvas.MeasureText(lbl, m.tickFontSize);
            float lblX = tickX - dims.x * 0.5f;
            lblX = std::clamp(lblX, bounds.x, bounds.Right() - dims.x);
            Color lblCol = (i == index) ? pal.accent : (disabled ? pal.textDisabled : pal.textSecondary);
            canvas.DrawText(lbl, Vec2(lblX, labelsY), m.tickFontSize, lblCol);
        }
    }

    // 6. Description text
    if (!opts.description.empty()) {
        const float extraH = (opts.showLabels && !opts.stepLabels.empty()) ? (m.tickGap + m.tickFontSize * 1.3f) : 0.0f;
        curY += trackRowH + extraH + m.descGap;
        Color descCol = disabled ? pal.textDisabled : pal.textSecondary;
        canvas.DrawText(opts.description, Vec2(bounds.x, curY), m.labelFontSize * 0.9f, descCol);
    }
}

} // namespace guikit
