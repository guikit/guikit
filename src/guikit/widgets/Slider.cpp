#include "guikit/widgets/Slider.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/ElementComposer.hpp"
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

void FormatSliderValue(
    char* valBuf,
    size_t bufSize,
    double value,
    bool isInt,
    std::string_view format,
    std::string_view suffix
) noexcept {
    if (!format.empty()) {
        std::string fmt(format);
        std::snprintf(valBuf, bufSize, fmt.c_str(), value);
    } else if (isInt) {
        std::snprintf(valBuf, bufSize, "%lld", static_cast<long long>(std::round(value)));
    } else {
        if (std::abs(value - std::round(value)) < 0.0001) {
            std::snprintf(valBuf, bufSize, "%.0f", value);
        } else {
            std::snprintf(valBuf, bufSize, "%.1f", value);
        }
    }
    if (!suffix.empty()) {
        const size_t len = std::strlen(valBuf);
        if (len < bufSize) {
            std::snprintf(valBuf + len, bufSize - len, " %.*s",
                          static_cast<int>(suffix.size()), suffix.data());
        }
    }
}

struct SliderControlMetrics {
    float leftTextW{0.0f};
    float leftTextFont{12.0f};
    float rightTextW{0.0f};
    float rightTextFont{12.0f};
    float valW{0.0f};
    float valReserveW{0.0f};
    float valLineH{0.0f};
    float valFont{12.0f};
    float leftGutterW{0.0f};
    float rightGutterW{0.0f};
    float topValH{0.0f};
    float bottomValH{0.0f};
    float trackRowH{0.0f};
    float extraLabelsH{0.0f};
    Vec2 intrinsicControlSize{};
};

SliderControlMetrics ComputeSliderControlMetrics(
    const UiText& textLeft,
    const UiText& textRight,
    const UiText& valueText,
    std::string_view formattedVal,
    bool showValue,
    Placement valuePlacement,
    StretchMode stretch,
    std::optional<float> width,
    float extraLabelsH,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const SliderMetrics& m = theme.GetSlider(size);
    const float innerGap = 8.0f;
    SliderControlMetrics cm{};
    cm.trackRowH = std::max(m.defaultHeight, m.thumbSize);
    cm.extraLabelsH = extraLabelsH;

    if (!textLeft.empty()) {
        cm.leftTextFont = theme.ResolveFontSize(textLeft, TextRole::LabelMedium, size);
        cm.leftTextW = canvas
            ? canvas->MeasureText(textLeft.text, cm.leftTextFont).x
            : static_cast<float>(textLeft.length()) * (cm.leftTextFont * kDefaultCharAdvanceRatio);
    }

    if (!textRight.empty()) {
        cm.rightTextFont = theme.ResolveFontSize(textRight, TextRole::LabelMedium, size);
        cm.rightTextW = canvas
            ? canvas->MeasureText(textRight.text, cm.rightTextFont).x
            : static_cast<float>(textRight.length()) * (cm.rightTextFont * kDefaultCharAdvanceRatio);
    }

    const std::string_view effectiveValStr = !valueText.empty() ? valueText.text : formattedVal;
    if (showValue) {
        cm.valFont = theme.ResolveFontSize(valueText, TextRole::LabelMedium, size);
        cm.valLineH = canvas ? canvas->GetLineHeight(cm.valFont) : theme.GetLineHeight(cm.valFont);
        if (!effectiveValStr.empty()) {
            cm.valW = canvas
                ? canvas->MeasureText(effectiveValStr, cm.valFont).x
                : static_cast<float>(effectiveValStr.length()) * (cm.valFont * kDefaultCharAdvanceRatio);
        }
        cm.valReserveW = std::max(cm.valW, 38.0f);
    }

    if (!textLeft.empty()) {
        cm.leftGutterW += cm.leftTextW + innerGap;
    }
    if (showValue && valuePlacement == Placement::Leading) {
        cm.leftGutterW += cm.valReserveW + innerGap;
    }

    if (!textRight.empty()) {
        cm.rightGutterW += innerGap + cm.rightTextW;
    }
    if (showValue && valuePlacement == Placement::Trailing) {
        cm.rightGutterW += innerGap + cm.valReserveW;
    }

    if (showValue && (valuePlacement == Placement::Top || valuePlacement == Placement::Thumb)) {
        cm.topValH = cm.valLineH + 2.0f;
    }
    if (showValue && valuePlacement == Placement::Bottom) {
        cm.bottomValH = 2.0f + cm.valLineH;
    }

    const float trackW = (stretch != StretchMode::Fill && width.has_value()) ? *width : m.defaultWidth;
    cm.intrinsicControlSize = {
        cm.leftGutterW + trackW + m.thumbSize + cm.rightGutterW,
        cm.topValH + cm.trackRowH + cm.extraLabelsH + cm.bottomValH
    };
    return cm;
}

void RenderSliderCompanionTexts(
    ICanvas& canvas,
    const Rect& controlRect,
    float trackRegionX,
    float trackRegionW,
    float trackRowY,
    float thumbCenterX,
    const SliderControlMetrics& cm,
    const UiText& textLeft,
    const UiText& textRight,
    const UiText& valueText,
    std::string_view formattedVal,
    bool showValue,
    Placement valuePlacement,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const float innerGap = 8.0f;
    const std::string_view effectiveValStr = !valueText.empty() ? valueText.text : formattedVal;
    const Color valCol = theme.ResolveTextColor(valueText, UiVariant::Secondary, disabled);

    // Left gutter: [textLeft] [Leading value]
    float leftX = controlRect.x;
    if (!textLeft.empty()) {
        const Color leftCol = theme.ResolveTextColor(textLeft, UiVariant::Secondary, disabled);
        canvas.DrawTextBox(
            textLeft.text,
            Rect(leftX, trackRowY, cm.leftTextW, cm.trackRowH),
            cm.leftTextFont,
            leftCol,
            0.0f,
            0.5f
        );
        leftX += cm.leftTextW + innerGap;
    }
    if (showValue && valuePlacement == Placement::Leading && !effectiveValStr.empty()) {
        canvas.DrawTextBox(
            effectiveValStr,
            Rect(leftX, trackRowY, cm.valReserveW, cm.trackRowH),
            cm.valFont,
            valCol,
            0.0f,
            0.5f
        );
    }

    // Right gutter: [textRight] [Trailing value]
    float rightX = trackRegionX + trackRegionW;
    if (!textRight.empty()) {
        rightX += innerGap;
        const Color rightCol = theme.ResolveTextColor(textRight, UiVariant::Secondary, disabled);
        canvas.DrawTextBox(
            textRight.text,
            Rect(rightX, trackRowY, cm.rightTextW, cm.trackRowH),
            cm.rightTextFont,
            rightCol,
            0.0f,
            0.5f
        );
        rightX += cm.rightTextW;
    }
    if (showValue && valuePlacement == Placement::Trailing && !effectiveValStr.empty()) {
        rightX += innerGap;
        canvas.DrawTextBox(
            effectiveValStr,
            Rect(rightX, trackRowY, cm.valReserveW, cm.trackRowH),
            cm.valFont,
            valCol,
            1.0f,
            0.5f
        );
    }

    // Top / Thumb / Bottom value
    if (showValue && !effectiveValStr.empty()) {
        if (valuePlacement == Placement::Top) {
            canvas.DrawTextBox(
                effectiveValStr,
                Rect(trackRegionX, controlRect.y, trackRegionW, cm.valLineH),
                cm.valFont,
                valCol,
                0.5f,
                0.5f
            );
        } else if (valuePlacement == Placement::Thumb) {
            const float valDrawW = std::max(cm.valW, 4.0f);
            const float thumbValX = std::clamp(
                thumbCenterX - valDrawW * 0.5f,
                trackRegionX,
                std::max(trackRegionX, trackRegionX + trackRegionW - valDrawW)
            );
            canvas.DrawTextBox(
                effectiveValStr,
                Rect(thumbValX, controlRect.y, valDrawW, cm.valLineH),
                cm.valFont,
                valCol,
                0.5f,
                0.5f
            );
        } else if (valuePlacement == Placement::Bottom) {
            const float bottomY = trackRowY + cm.trackRowH + cm.extraLabelsH + 2.0f;
            canvas.DrawTextBox(
                effectiveValStr,
                Rect(trackRegionX, bottomY, trackRegionW, cm.valLineH),
                cm.valFont,
                valCol,
                0.5f,
                0.5f
            );
        }
    }
}

ElementSpec MakeSliderElementSpec(
    const SliderOptions& opts,
    UiSize size,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const SliderMetrics& m = theme.GetSlider(size);
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = opts.width,
        .widgetSize = size,
        .labelGap = m.labelGap,
        .descGap = m.descGap,
        .inlineGap = 12.0f,
        .stretchableControl = true,
        .stretchHeightWhenStandalone = false,
        .disabled = disabled
    };
}

ElementSpec MakeStepSliderElementSpec(
    const StepSliderOptions& opts,
    UiSize size,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const SliderMetrics& m = theme.GetSlider(size);
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = opts.width,
        .widgetSize = size,
        .labelGap = m.labelGap,
        .descGap = m.descGap,
        .inlineGap = 12.0f,
        .stretchableControl = true,
        .stretchHeightWhenStandalone = false,
        .disabled = disabled
    };
}

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
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const UiTheme& activeTheme = ctx.GetTheme();

    const SliderMetrics& m = activeTheme.GetSlider(activeSize);
    const double rangeMin = std::min(min, max);
    const double rangeMax = std::max(min, max);

    double currentVal = (valDouble != nullptr) ? *valDouble :
                        (valFloat != nullptr)  ? static_cast<double>(*valFloat) :
                        (valInt != nullptr)    ? static_cast<double>(*valInt) : rangeMin;

    currentVal = std::clamp(currentVal, rangeMin, rangeMax);

    const double effectiveStep = (opts.step.has_value() && *opts.step > 0.0) ? *opts.step :
                                 (valInt != nullptr) ? 1.0 : 0.0;

    char initValBuf[64];
    FormatSliderValue(initValBuf, sizeof(initValBuf), currentVal, valInt != nullptr, opts.format, opts.suffix);

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
        const SliderControlMetrics cm = ComputeSliderControlMetrics(
            opts.textLeft, opts.textRight, opts.valueText, initValBuf,
            opts.showValue, opts.valuePlacement, opts.stretch, opts.width,
            0.0f, activeSize, nullptr, activeTheme
        );
        const ElementSpec spec = MakeSliderElementSpec(opts, activeSize, effectiveDisabled, activeTheme);
        const ElementGeometry geom = ElementComposer::Resolve(prevBounds, cm.intrinsicControlSize, spec, nullptr, activeTheme);
        const float trackRegionX = geom.controlRect.x + cm.leftGutterW;
        const float trackRegionW = std::max(m.thumbSize, geom.controlRect.w - cm.leftGutterW - cm.rightGutterW);
        const float trackStartX = trackRegionX + halfThumb;
        const float trackWidth = std::max(1.0f, trackRegionW - m.thumbSize);

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
    FormatSliderValue(valBuf, sizeof(valBuf), currentVal, valInt != nullptr, opts.format, opts.suffix);
    const std::string_view formattedStr = ctx.AllocString(valBuf);

    Node* node = ctx.CreateNode(NodeType::Slider, id);
    node->theme = pushedTheme;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed || state.dragging;
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    auto* data = ctx.AllocPayload<SliderData>();
    data->opts = opts;
    data->opts.label = ctx.AllocText(opts.label);
    data->opts.textLeft = ctx.AllocText(opts.textLeft);
    data->opts.textRight = ctx.AllocText(opts.textRight);
    data->opts.description = ctx.AllocText(opts.description);
    data->opts.valueText = ctx.AllocText(opts.valueText);
    node->text = data->opts.label.text;
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
        static_cast<uint8_t>(opts.stretch),
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        static_cast<uint8_t>(opts.valuePlacement),
        opts.width.value_or(0.0f),
        opts.showValue,
        opts.format,
        opts.suffix,
        data->opts.label,
        data->opts.textLeft,
        data->opts.textRight,
        data->opts.description,
        data->opts.valueText,
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
    if (pushedTheme) {
        ctx.PopTheme();
    }
    return res;
}

Vec2 Slider::Measure(
    const SliderOptions& opts,
    double value,
    double /*min*/,
    double /*max*/,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    char valBuf[64];
    FormatSliderValue(valBuf, sizeof(valBuf), value, false, opts.format, opts.suffix);
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueText, valBuf,
        opts.showValue, opts.valuePlacement, opts.stretch, opts.width,
        0.0f, size, canvas, theme
    );
    const ElementSpec spec = MakeSliderElementSpec(opts, size, opts.disabled, theme);
    return ElementComposer::Measure(cm.intrinsicControlSize, spec, canvas, theme);
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

    char valBuf[64];
    FormatSliderValue(valBuf, sizeof(valBuf), value, false, opts.format, opts.suffix);

    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueText, valBuf,
        opts.showValue, opts.valuePlacement, opts.stretch, opts.width,
        0.0f, size, &canvas, theme
    );
    const ElementSpec spec = MakeSliderElementSpec(opts, size, disabled, theme);
    const ElementGeometry geom = ElementComposer::Resolve(bounds, cm.intrinsicControlSize, spec, &canvas, theme);
    ElementComposer::RenderText(canvas, geom, spec, theme);

    const float halfThumb = m.thumbSize * 0.5f;
    const float trackRegionX = geom.controlRect.x + cm.leftGutterW;
    const float trackRegionW = std::max(m.thumbSize, geom.controlRect.w - cm.leftGutterW - cm.rightGutterW);
    const float trackStartX = trackRegionX + halfThumb;
    const float trackWidth = std::max(0.0f, trackRegionW - m.thumbSize);
    const float trackRowY = geom.controlRect.y + cm.topValH;
    const float centerY = trackRowY + cm.trackRowH * 0.5f;

    const double range = max - min;
    const float ratio = (range > 0.0) ? static_cast<float>(std::clamp((value - min) / range, 0.0, 1.0)) : 0.0f;
    const float fillW = ratio * trackWidth;
    const float thumbCenterX = trackStartX + fillW;

    // 0. Draw companion textLeft / textRight and value readout
    RenderSliderCompanionTexts(
        canvas, geom.controlRect, trackRegionX, trackRegionW, trackRowY, thumbCenterX,
        cm, opts.textLeft, opts.textRight, opts.valueText, valBuf,
        opts.showValue, opts.valuePlacement, disabled, theme
    );

    // 1. Draw inactive background track
    const Rect trackBgRect(trackStartX, centerY - m.trackHeight * 0.5f, trackWidth, m.trackHeight);
    Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
    Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
    canvas.DrawRoundRect(trackBgRect, m.trackHeight * 0.5f, trackBg, trackBorder, 1.0f);

    // 2. Draw active filled track segment
    if (fillW > 0.0f) {
        const Rect fillRect(trackStartX, centerY - m.trackHeight * 0.5f, fillW, m.trackHeight);
        Color baseFill = (opts.variant == UiVariant::Ghost) ? pal.borderHover : variantCol.bg;
        Color fillCol = disabled ? baseFill.WithAlphaF(0.4f) : baseFill;
        canvas.DrawRoundRect(fillRect, m.trackHeight * 0.5f, fillCol);
    }

    // 3. Draw thumb knob
    const Rect thumbRect(thumbCenterX - halfThumb, centerY - halfThumb, m.thumbSize, m.thumbSize);
    Color thumbBg = disabled ? pal.surface.WithAlphaF(0.5f) :
                    active   ? variantCol.bgActive :
                    hovered  ? pal.surface : pal.surface;
    Color thumbBorder = disabled ? pal.border.WithAlphaF(0.4f) :
                        (active || hovered) ? pal.accent : pal.borderHover;
    const float thumbBorderW = (active || hovered) ? 2.5f : 2.0f;
    canvas.DrawRoundRect(thumbRect, halfThumb, thumbBg, thumbBorder, thumbBorderW);
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
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const UiTheme& activeTheme = ctx.GetTheme();

    const SliderMetrics& m = activeTheme.GetSlider(activeSize);

    if (totalSteps == 0) {
        totalSteps = opts.stepLabels.empty() ? 1 : opts.stepLabels.size();
    }

    size_t curIdx = (indexSizeT != nullptr) ? *indexSizeT :
                    (indexInt != nullptr)   ? static_cast<size_t>(std::max(0, *indexInt)) : 0;
    curIdx = std::min(curIdx, totalSteps - 1);

    const std::string_view initStepStr = (curIdx < opts.stepLabels.size()) ? opts.stepLabels[curIdx] : std::string_view{};
    const float extraLabelsH = (opts.showLabels && !opts.stepLabels.empty() && totalSteps > 1) ? (m.tickGap + m.tickFontSize * 1.3f) : 0.0f;

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
        const SliderControlMetrics cm = ComputeSliderControlMetrics(
            opts.textLeft, opts.textRight, opts.valueText, initStepStr,
            opts.showValue, opts.valuePlacement, opts.stretch, opts.width,
            extraLabelsH, activeSize, nullptr, activeTheme
        );
        const ElementSpec spec = MakeStepSliderElementSpec(opts, activeSize, effectiveDisabled, activeTheme);
        const ElementGeometry geom = ElementComposer::Resolve(prevBounds, cm.intrinsicControlSize, spec, nullptr, activeTheme);
        const float trackRegionX = geom.controlRect.x + cm.leftGutterW;
        const float trackRegionW = std::max(m.thumbSize, geom.controlRect.w - cm.leftGutterW - cm.rightGutterW);
        const float trackStartX = trackRegionX + halfThumb;
        const float trackWidth = std::max(1.0f, trackRegionW - m.thumbSize);

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
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    auto* data = ctx.AllocPayload<StepSliderData>();
    data->opts = opts;
    data->opts.label = ctx.AllocText(opts.label);
    data->opts.textLeft = ctx.AllocText(opts.textLeft);
    data->opts.textRight = ctx.AllocText(opts.textRight);
    data->opts.description = ctx.AllocText(opts.description);
    data->opts.valueText = ctx.AllocText(opts.valueText);
    node->text = data->opts.label.text;
    data->index = curIdx;
    data->totalSteps = totalSteps;
    node->payload = data;

    node->visualHash = detail::HashArgs(
        curIdx,
        totalSteps,
        opts.variant,
        static_cast<uint8_t>(opts.stretch),
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        static_cast<uint8_t>(opts.valuePlacement),
        opts.width.value_or(0.0f),
        opts.showTicks,
        opts.showLabels,
        opts.showValue,
        data->opts.label,
        data->opts.textLeft,
        data->opts.textRight,
        data->opts.description,
        data->opts.valueText
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
    if (pushedTheme) {
        ctx.PopTheme();
    }
    return res;
}

Vec2 StepSlider::Measure(
    const StepSliderOptions& opts,
    size_t index,
    size_t totalSteps,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    if (totalSteps == 0) {
        totalSteps = opts.stepLabels.empty() ? 1 : opts.stepLabels.size();
    }
    const SliderMetrics& m = theme.GetSlider(size);
    const float extraLabelsH = (opts.showLabels && !opts.stepLabels.empty() && totalSteps > 1) ? (m.tickGap + m.tickFontSize * 1.3f) : 0.0f;
    const std::string_view stepStr = (index < opts.stepLabels.size()) ? opts.stepLabels[index] : std::string_view{};
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueText, stepStr,
        opts.showValue, opts.valuePlacement, opts.stretch, opts.width,
        extraLabelsH, size, canvas, theme
    );
    const ElementSpec spec = MakeStepSliderElementSpec(opts, size, opts.disabled, theme);
    return ElementComposer::Measure(cm.intrinsicControlSize, spec, canvas, theme);
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

    const std::string_view stepStr = (index < opts.stepLabels.size()) ? opts.stepLabels[index] : std::string_view{};
    const float extraLabelsH = (opts.showLabels && !opts.stepLabels.empty() && totalSteps > 1) ? (m.tickGap + m.tickFontSize * 1.3f) : 0.0f;

    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueText, stepStr,
        opts.showValue, opts.valuePlacement, opts.stretch, opts.width,
        extraLabelsH, size, &canvas, theme
    );
    const ElementSpec spec = MakeStepSliderElementSpec(opts, size, disabled, theme);
    const ElementGeometry geom = ElementComposer::Resolve(bounds, cm.intrinsicControlSize, spec, &canvas, theme);
    ElementComposer::RenderText(canvas, geom, spec, theme);

    const float halfThumb = m.thumbSize * 0.5f;
    const float trackRegionX = geom.controlRect.x + cm.leftGutterW;
    const float trackRegionW = std::max(m.thumbSize, geom.controlRect.w - cm.leftGutterW - cm.rightGutterW);
    const float trackStartX = trackRegionX + halfThumb;
    const float trackWidth = std::max(0.0f, trackRegionW - m.thumbSize);
    const float trackRowY = geom.controlRect.y + cm.topValH;
    const float centerY = trackRowY + cm.trackRowH * 0.5f;

    const float ratio = (totalSteps > 1) ? static_cast<float>(index) / static_cast<float>(totalSteps - 1) : 0.0f;
    const float fillW = ratio * trackWidth;
    const float thumbCenterX = trackStartX + fillW;

    // 0. Draw companion textLeft / textRight and value readout
    RenderSliderCompanionTexts(
        canvas, geom.controlRect, trackRegionX, trackRegionW, trackRowY, thumbCenterX,
        cm, opts.textLeft, opts.textRight, opts.valueText, stepStr,
        opts.showValue, opts.valuePlacement, disabled, theme
    );

    // 1. Draw inactive background track
    const Rect trackBgRect(trackStartX, centerY - m.trackHeight * 0.5f, trackWidth, m.trackHeight);
    Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
    Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
    canvas.DrawRoundRect(trackBgRect, m.trackHeight * 0.5f, trackBg, trackBorder, 1.0f);

    // 2. Draw active filled track segment
    if (fillW > 0.0f) {
        const Rect fillRect(trackStartX, centerY - m.trackHeight * 0.5f, fillW, m.trackHeight);
        Color baseFill = (opts.variant == UiVariant::Ghost) ? pal.borderHover : variantCol.bg;
        Color fillCol = disabled ? baseFill.WithAlphaF(0.4f) : baseFill;
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
            lblX = std::clamp(lblX, trackRegionX, std::max(trackRegionX, trackRegionX + trackRegionW - dims.x));
            Color lblCol = (i == index) ? pal.accent : (disabled ? pal.textDisabled : pal.textSecondary);
            canvas.DrawText(lbl, Vec2(lblX, labelsY), m.tickFontSize, lblCol);
        }
    }
}

} // namespace guikit
