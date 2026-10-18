#include "guikit/widgets/Slider.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

struct SliderData {
    SliderOptions opts;
    double value{0.0};
    double min{0.0};
    double max{1.0};
    bool isInt{false};
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
    std::string_view format
) noexcept {
    if (!format.empty()) {
        char convSpec = '\0';
        bool isLongLong = false;
        bool isLong = false;
        for (size_t i = 0; i < format.size(); ++i) {
            if (format[i] == '%') {
                if (i + 1 < format.size() && format[i + 1] == '%') {
                    ++i; // skip escaped %%
                    continue;
                }
                for (size_t j = i + 1; j < format.size(); ++j) {
                    const char c = format[j];
                    if (c == 'l') {
                        if (isLong) isLongLong = true;
                        else isLong = true;
                    } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                        convSpec = c;
                        break;
                    }
                }
                break;
            }
        }

        std::string fmt(format);
        if (convSpec == 'd' || convSpec == 'i' || convSpec == 'u' ||
            convSpec == 'x' || convSpec == 'X' || convSpec == 'o') {
            const long long intVal = static_cast<long long>(std::round(value));
            if (isLongLong) {
                std::snprintf(valBuf, bufSize, fmt.c_str(), intVal);
            } else if (isLong) {
                std::snprintf(valBuf, bufSize, fmt.c_str(), static_cast<long>(intVal));
            } else {
                std::snprintf(valBuf, bufSize, fmt.c_str(), static_cast<int>(intVal));
            }
        } else if (convSpec != '\0') {
            std::snprintf(valBuf, bufSize, fmt.c_str(), value);
        } else {
            std::snprintf(valBuf, bufSize, fmt.c_str());
        }
    } else if (isInt) {
        std::snprintf(valBuf, bufSize, "%lld", static_cast<long long>(std::round(value)));
    } else {
        if (std::abs(value - std::round(value)) < 0.0001) {
            std::snprintf(valBuf, bufSize, "%.0f", value);
        } else {
            std::snprintf(valBuf, bufSize, "%.1f", value);
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
    const UiText& valueStyle,
    std::string_view effectiveValStr,
    bool showValue,
    Placement valuePlacement,
    StretchMode stretch,
    std::optional<float> width,
    float extraLabelsH,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    std::span<const std::string_view> extraValCandidates = {},
    bool hasNumericRange = false,
    double rangeMin = 0.0,
    double rangeMax = 1.0,
    bool isInt = false
) noexcept {
    const SliderMetrics& m = theme.GetSlider(size);
    const float innerGap = 8.0f;
    SliderControlMetrics cm{};
    cm.trackRowH = std::max(m.defaultHeight, m.thumbSize);
    cm.extraLabelsH = extraLabelsH;

    const bool hasLeftText = !textLeft.empty() || !textLeft.variants.empty();
    const bool hasRightText = !textRight.empty() || !textRight.variants.empty();

    if (hasLeftText) {
        cm.leftTextFont = theme.ResolveFontSize(textLeft, TextRole::LabelMedium, size);
        cm.leftTextW = TextUtil::MeasureWidth(textLeft, cm.leftTextFont, canvas);
    }

    if (hasRightText) {
        cm.rightTextFont = theme.ResolveFontSize(textRight, TextRole::LabelMedium, size);
        cm.rightTextW = TextUtil::MeasureWidth(textRight, cm.rightTextFont, canvas);
    }

    if (showValue) {
        cm.valFont = theme.ResolveFontSize(valueStyle, TextRole::LabelMedium, size);
        cm.valLineH = canvas ? canvas->GetLineHeight(cm.valFont) : theme.GetLineHeight(cm.valFont);
        cm.valW = TextUtil::MeasureWidth(effectiveValStr, cm.valFont, canvas);
        cm.valReserveW = cm.valW;

        if (hasNumericRange) {
            auto measureNumericFmt = [&](std::string_view fmt) {
                char buf[64];
                FormatSliderValue(buf, sizeof(buf), rangeMin, isInt, fmt);
                cm.valReserveW = std::max(cm.valReserveW, TextUtil::MeasureWidth(std::string_view(buf), cm.valFont, canvas));

                FormatSliderValue(buf, sizeof(buf), rangeMax, isInt, fmt);
                cm.valReserveW = std::max(cm.valReserveW, TextUtil::MeasureWidth(std::string_view(buf), cm.valFont, canvas));

                const double sampleVal = (rangeMax > rangeMin + 0.1)
                    ? (rangeMax - 0.1)
                    : (rangeMin + (rangeMax - rangeMin) * 0.88);
                FormatSliderValue(buf, sizeof(buf), sampleVal, isInt, fmt);
                cm.valReserveW = std::max(cm.valReserveW, TextUtil::MeasureWidth(std::string_view(buf), cm.valFont, canvas));
            };

            measureNumericFmt(valueStyle.text);
            for (std::string_view var : valueStyle.variants) {
                if (!var.empty()) {
                    measureNumericFmt(var);
                }
            }
        } else {
            cm.valReserveW = std::max(cm.valReserveW, TextUtil::MeasureWidth(valueStyle, cm.valFont, canvas));
            for (std::string_view cand : extraValCandidates) {
                if (!cand.empty()) {
                    cm.valReserveW = std::max(cm.valReserveW, TextUtil::MeasureWidth(cand, cm.valFont, canvas));
                }
            }
        }
    }

    if (hasLeftText) {
        cm.leftGutterW += cm.leftTextW + innerGap;
    }
    if (showValue && valuePlacement == Placement::Leading) {
        cm.leftGutterW += cm.valReserveW + innerGap;
    }

    if (hasRightText) {
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

    const float trackW = (stretch != StretchMode::Fill && width.has_value()) ? theme.Scale(*width) : m.defaultWidth;
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
    const UiText& valueStyle,
    std::string_view effectiveValStr,
    bool showValue,
    Placement valuePlacement,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const float innerGap = 8.0f;
    const Color valCol = theme.ResolveTextColor(valueStyle, UiVariant::Secondary, disabled);

    // Left gutter: [textLeft] [Leading value]
    float leftX = controlRect.x;
    if (!textLeft.empty() || !textLeft.variants.empty()) {
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
        }
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
    if (!textRight.empty() || !textRight.variants.empty()) {
        rightX += innerGap;
        if (!textRight.empty()) {
            const Color rightCol = theme.ResolveTextColor(textRight, UiVariant::Secondary, disabled);
            canvas.DrawTextBox(
                textRight.text,
                Rect(rightX, trackRowY, cm.rightTextW, cm.trackRowH),
                cm.rightTextFont,
                rightCol,
                0.0f,
                0.5f
            );
        }
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
        const float topY = trackRowY - cm.topValH;
        if (valuePlacement == Placement::Top) {
            canvas.DrawTextBox(
                effectiveValStr,
                Rect(trackRegionX, topY, trackRegionW, cm.valLineH),
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
                Rect(thumbValX, topY, valDrawW, cm.valLineH),
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

template <typename Opts>
ControlSpec EffectiveSliderControl(const Opts& opts, UiSize size, const UiTheme& theme) noexcept {
    const SliderMetrics& m = theme.GetSlider(size);
    ControlSpec c{};
    c.width = opts.width;
    c.height = opts.height;
    c.align = opts.align;
    c.background = opts.background;
    c.border = opts.border;
    c.defaultLayout = ElementLayout::Stacked;
    c.defaultInlineOrder = InlineOrder::LabelFirst;
    c.labelGap = m.labelGap;
    c.descGap = m.descGap;
    c.inlineGap = 12.0f;
    c.stretchable = true;
    c.stretchHeightWhenStandalone = false;
    c.hasIntrinsicBox = false;
    return c;
}

Color ResolveSliderTrackColor(const StateColor& color, bool disabled, const UiTheme& theme) noexcept {
    const auto& pal = theme.palette;
    const VariantColors& primary = theme.GetVariant(UiVariant::Primary);
    if (!color.HasAny()) {
        return disabled ? primary.bg.WithAlphaF(0.4f) : primary.bg;
    }
    if (color.normal.has_value() && color.normal->a == 0) {
        return disabled ? pal.borderHover.WithAlphaF(0.4f) : pal.borderHover;
    }
    const Color base = color.normal.value_or(primary.bg);
    return disabled ? color.disabled.value_or(base.WithAlphaF(0.4f)) : base;
}

Color ResolveSliderActiveThumbColor(const StateColor& color, const UiTheme& theme) noexcept {
    const VariantColors& primary = theme.GetVariant(UiVariant::Primary);
    if (!color.HasAny()) {
        return primary.bgActive;
    }
    if (color.normal.has_value() && color.normal->a == 0) {
        return theme.GetVariant(UiVariant::Ghost).bgActive;
    }
    return color.active.value_or(color.normal.has_value() ? StateColor::DarkenColor(*color.normal) : primary.bgActive);
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

    SliderOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    const SliderMetrics& m = activeTheme.GetSlider(activeSize);
    const double rangeMin = std::min(min, max);
    const double rangeMax = std::max(min, max);

    double currentVal = (valDouble != nullptr) ? *valDouble :
                        (valFloat != nullptr)  ? static_cast<double>(*valFloat) :
                        (valInt != nullptr)    ? static_cast<double>(*valInt) : rangeMin;

    currentVal = std::clamp(currentVal, rangeMin, rangeMax);

    const double effectiveStep = (optsCopy.step.has_value() && *optsCopy.step > 0.0) ? *optsCopy.step :
                                 (valInt != nullptr) ? 1.0 : 0.0;

    char initValBuf[64];
    FormatSliderValue(initValBuf, sizeof(initValBuf), currentVal, valInt != nullptr, optsCopy.valueFormat.text);

    Rect prevBounds = ctx.GetPrevFrameBounds(id);
    auto& state = ctx.GetOrCreateState<SliderState>(id);

    ctx.RegisterFocusable(id, effectiveDisabled);

    const ControlSpec ctrl = EffectiveSliderControl(optsCopy, activeSize, activeTheme);
    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        state.dragging = false;
    } else {
        const float halfThumb = m.thumbSize * 0.5f;
        const SliderControlMetrics cm = ComputeSliderControlMetrics(
            optsCopy.textLeft, optsCopy.textRight, optsCopy.valueFormat, initValBuf,
            optsCopy.showValue, optsCopy.valuePlacement, optsCopy.stretch, ctrl.width,
            0.0f, activeSize, nullptr, activeTheme,
            {}, true, rangeMin, rangeMax, valInt != nullptr
        );
        const Rect ctrlRect = Control::ResolveInnerRect(prevBounds, ctrl, activeSize, activeTheme);
        const float trackRegionX = ctrlRect.x + cm.leftGutterW;
        const float trackRegionW = std::max(m.thumbSize, ctrlRect.w - cm.leftGutterW - cm.rightGutterW);
        const float trackStartX = trackRegionX + halfThumb;
        const float trackWidth = std::max(1.0f, trackRegionW - m.thumbSize);

        if (res.clicked || (res.pressed && !state.dragging)) {
            state.dragging = true;
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

        // Keyboard arrow adjustment when focused
        if (ctx.IsFocused(id) && !state.dragging) {
            const double stepDelta = (effectiveStep > 0.0) ? effectiveStep : (rangeMax - rangeMin) * 0.05;
            if (ctx.ConsumeKey(Key::Right) || ctx.ConsumeKey(Key::Up)) {
                const double rawVal = std::clamp(currentVal + stepDelta, rangeMin, rangeMax);
                if (rawVal != currentVal) {
                    currentVal = rawVal;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            } else if (ctx.ConsumeKey(Key::Left) || ctx.ConsumeKey(Key::Down)) {
                const double rawVal = std::clamp(currentVal - stepDelta, rangeMin, rangeMax);
                if (rawVal != currentVal) {
                    currentVal = rawVal;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            }
        }
    }

    if (res.hovered && !optsCopy.tooltip.empty()) {
        ctx.SetTooltip(optsCopy.tooltip, prevBounds);
    }

    // Write back updated value
    if (valDouble != nullptr) *valDouble = currentVal;
    if (valFloat != nullptr)  *valFloat = static_cast<float>(currentVal);
    if (valInt != nullptr)    *valInt = static_cast<int64_t>(std::round(currentVal));

    // Format current value string
    char valBuf[64];
    FormatSliderValue(valBuf, sizeof(valBuf), currentVal, valInt != nullptr, optsCopy.valueFormat.text);
    const std::string_view formattedStr = ctx.AllocString(valBuf);

    Node* node = ctx.CreateNode(NodeType::Slider, id);
    node->theme = pushedTheme;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed || state.dragging;
    node->focused = ctx.IsFocused(id);
    node->widthMode = (optsCopy.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    auto* data = ctx.AllocPayload<SliderData>();
    data->opts = optsCopy;
    data->opts.textLeft = ctx.AllocText(optsCopy.textLeft);
    data->opts.textRight = ctx.AllocText(optsCopy.textRight);
    data->opts.tooltip = ctx.AllocText(optsCopy.tooltip);
    data->opts.valueFormat = ctx.AllocText(optsCopy.valueFormat);
    node->text = formattedStr;
    data->value = currentVal;
    data->min = rangeMin;
    data->max = rangeMax;
    data->isInt = (valInt != nullptr);
    data->formattedValue = formattedStr;
    node->SetPayload(data);

    uint64_t valBits = 0;
    std::memcpy(&valBits, &currentVal, sizeof(double));
    node->visualHash = detail::HashArgs(
        valBits,
        data->opts.color,
        static_cast<uint8_t>(data->opts.stretch),
        static_cast<uint8_t>(data->opts.align),
        static_cast<uint8_t>(data->opts.valuePlacement),
        data->opts.background,
        data->opts.border,
        data->opts.width.value_or(0.0f),
        data->opts.height.value_or(0.0f),
        data->opts.showValue,
        data->opts.textLeft,
        data->opts.textRight,
        data->opts.valueFormat,
        data->formattedValue
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<SliderData>();
        return Slider::Measure(d ? d->opts : SliderOptions{},
                               d ? d->value : 0.0,
                               d ? d->min : 0.0,
                               d ? d->max : 1.0,
                               n->uiSize, canvas, theme, availableWidth);
    };

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<SliderData>();
        if (!d) return;
        Slider::Render(canvas, n->computedRect, d->value, d->min, d->max,
                       d->opts, n->uiSize, n->hovered, n->pressed, n->disabled, theme, n->focused);
    };

    Control::SetupNode(ctx, node, ctrl, data->opts.stretch, data->opts.align);
    ctx.AdaptChildToContainer(node);
    if (pushedTheme) {
        ctx.PopTheme();
    }
    return res;
}

Vec2 Slider::Measure(
    const SliderOptions& opts,
    double value,
    double min,
    double max,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const double rangeMin = std::min(min, max);
    const double rangeMax = std::max(min, max);
    const bool isIntStep = opts.step.has_value() && *opts.step >= 1.0 &&
                           std::abs(*opts.step - std::round(*opts.step)) < 0.0001 &&
                           std::abs(rangeMin - std::round(rangeMin)) < 0.0001;
    char valBuf[64];
    FormatSliderValue(valBuf, sizeof(valBuf), value, isIntStep, opts.valueFormat.text);
    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
        opts.showValue, opts.valuePlacement, opts.stretch, ctrl.width,
        0.0f, size, canvas, theme,
        {}, true, rangeMin, rangeMax, isIntStep
    );
    return Control::Measure(cm.intrinsicControlSize, ctrl, opts.stretch, size, theme);
}

Vec2 Slider::Measure(
    const SliderOptions& opts,
    double value,
    double min,
    double max,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, value, min, max, size, canvas, defaultTheme, availableWidth);
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
    const UiTheme& theme,
    bool focused
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const SliderMetrics& m = theme.GetSlider(size);
    const auto& pal = theme.palette;
    const double rangeMin = std::min(min, max);
    const double rangeMax = std::max(min, max);
    const bool isIntStep = opts.step.has_value() && *opts.step >= 1.0 &&
                           std::abs(*opts.step - std::round(*opts.step)) < 0.0001 &&
                           std::abs(rangeMin - std::round(rangeMin)) < 0.0001;

    char valBuf[64];
    FormatSliderValue(valBuf, sizeof(valBuf), value, isIntStep, opts.valueFormat.text);

    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
        opts.showValue, opts.valuePlacement, opts.stretch, ctrl.width,
        0.0f, size, &canvas, theme,
        {}, true, rangeMin, rangeMax, isIntStep
    );
    Control::RenderFrame(canvas, bounds, ctrl, size, disabled, theme, hovered, active || focused);

    const Rect ctrlRect = Control::ResolveInnerRect(bounds, ctrl, size, theme);
    const float halfThumb = m.thumbSize * 0.5f;
    const float trackRegionX = ctrlRect.x + cm.leftGutterW;
    const float trackRegionW = std::max(m.thumbSize, ctrlRect.w - cm.leftGutterW - cm.rightGutterW);
    const float trackStartX = trackRegionX + halfThumb;
    const float trackWidth = std::max(0.0f, trackRegionW - m.thumbSize);
    const float contentOffsetY = std::max(0.0f, (ctrlRect.h - cm.intrinsicControlSize.y) * 0.5f);
    const float trackRowY = ctrlRect.y + contentOffsetY + cm.topValH;
    const float centerY = trackRowY + cm.trackRowH * 0.5f;

    const double range = max - min;
    const float ratio = (range > 0.0) ? static_cast<float>(std::clamp((value - min) / range, 0.0, 1.0)) : 0.0f;
    const float fillW = ratio * trackWidth;
    const float thumbCenterX = trackStartX + fillW;

    // 0. Draw companion textLeft / textRight and value readout
    RenderSliderCompanionTexts(
        canvas, ctrlRect, trackRegionX, trackRegionW, trackRowY, thumbCenterX,
        cm, opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
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
        Color fillCol = ResolveSliderTrackColor(opts.color, disabled, theme);
        canvas.DrawRoundRect(fillRect, m.trackHeight * 0.5f, fillCol);
    }

    // 3. Draw thumb knob
    const Rect thumbRect(thumbCenterX - halfThumb, centerY - halfThumb, m.thumbSize, m.thumbSize);
    Color thumbBg = disabled ? pal.surface.WithAlphaF(0.5f) :
                    active   ? ResolveSliderActiveThumbColor(opts.color, theme) :
                    hovered  ? pal.surface : pal.surface;
    Color thumbBorder = disabled ? pal.border.WithAlphaF(0.4f) :
                        focused  ? pal.focusRing :
                        (active || hovered) ? pal.accent : pal.borderHover;
    const float thumbBorderW = (active || hovered || focused) ? 2.5f : 2.0f;
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

    StepSliderOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    const SliderMetrics& m = activeTheme.GetSlider(activeSize);

    if (totalSteps == 0) {
        totalSteps = optsCopy.stepLabels.empty() ? 1 : optsCopy.stepLabels.size();
    }

    size_t curIdx = (indexSizeT != nullptr) ? *indexSizeT :
                    (indexInt != nullptr)   ? static_cast<size_t>(std::max(0, *indexInt)) : 0;
    curIdx = std::min(curIdx, totalSteps - 1);

    const std::string_view initStepStr = !optsCopy.valueText.empty()
        ? optsCopy.valueText.text
        : ((curIdx < optsCopy.stepLabels.size()) ? optsCopy.stepLabels[curIdx] : std::string_view{});
    const float tickFont = activeTheme.ResolveFontSize(TextRole::LabelSmall, activeSize);
    const float extraLabelsH = (optsCopy.showLabels && !optsCopy.stepLabels.empty() && totalSteps > 1) ? (m.tickGap + tickFont * 1.3f) : 0.0f;

    Rect prevBounds = ctx.GetPrevFrameBounds(id);
    auto& state = ctx.GetOrCreateState<SliderState>(id);

    ctx.RegisterFocusable(id, effectiveDisabled);

    const ControlSpec ctrl = EffectiveSliderControl(optsCopy, activeSize, activeTheme);
    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        state.dragging = false;
    } else {
        const float halfThumb = m.thumbSize * 0.5f;
        const SliderControlMetrics cm = ComputeSliderControlMetrics(
            optsCopy.textLeft, optsCopy.textRight, optsCopy.valueText, initStepStr,
            optsCopy.showValue, optsCopy.valuePlacement, optsCopy.stretch, ctrl.width,
            extraLabelsH, activeSize, nullptr, activeTheme,
            optsCopy.stepLabels
        );
        const Rect ctrlRect = Control::ResolveInnerRect(prevBounds, ctrl, activeSize, activeTheme);
        const float trackRegionX = ctrlRect.x + cm.leftGutterW;
        const float trackRegionW = std::max(m.thumbSize, ctrlRect.w - cm.leftGutterW - cm.rightGutterW);
        const float trackStartX = trackRegionX + halfThumb;
        const float trackWidth = std::max(1.0f, trackRegionW - m.thumbSize);

        if (res.clicked || (res.pressed && !state.dragging)) {
            state.dragging = true;
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

        // Keyboard arrow adjustment when focused
        if (ctx.IsFocused(id) && !state.dragging) {
            if (ctx.ConsumeKey(Key::Right) || ctx.ConsumeKey(Key::Up)) {
                if (curIdx + 1 < totalSteps) {
                    curIdx++;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            } else if (ctx.ConsumeKey(Key::Left) || ctx.ConsumeKey(Key::Down)) {
                if (curIdx > 0) {
                    curIdx--;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            }
        }
    }

    if (res.hovered && !optsCopy.tooltip.empty()) {
        ctx.SetTooltip(optsCopy.tooltip, prevBounds);
    }

    if (indexSizeT != nullptr) *indexSizeT = curIdx;
    if (indexInt != nullptr)   *indexInt = static_cast<int>(curIdx);

    Node* node = ctx.CreateNode(NodeType::StepSlider, id);
    node->theme = pushedTheme;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed || state.dragging;
    node->focused = ctx.IsFocused(id);
    node->widthMode = (optsCopy.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    auto* data = ctx.AllocPayload<StepSliderData>();
    data->opts = optsCopy;
    data->opts.textLeft = ctx.AllocText(optsCopy.textLeft);
    data->opts.textRight = ctx.AllocText(optsCopy.textRight);
    data->opts.tooltip = ctx.AllocText(optsCopy.tooltip);
    data->opts.valueText = ctx.AllocText(optsCopy.valueText);
    node->text = initStepStr;
    data->index = curIdx;
    data->totalSteps = totalSteps;
    node->SetPayload(data);

    node->visualHash = detail::HashArgs(
        curIdx,
        totalSteps,
        data->opts.color,
        static_cast<uint8_t>(data->opts.stretch),
        static_cast<uint8_t>(data->opts.align),
        static_cast<uint8_t>(data->opts.valuePlacement),
        data->opts.background,
        data->opts.border,
        data->opts.width.value_or(0.0f),
        data->opts.height.value_or(0.0f),
        data->opts.showTicks,
        data->opts.showLabels,
        data->opts.showValue,
        data->opts.textLeft,
        data->opts.textRight,
        data->opts.valueText
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<StepSliderData>();
        return StepSlider::Measure(d ? d->opts : StepSliderOptions{},
                                   d ? d->index : 0,
                                   d ? d->totalSteps : 1,
                                   n->uiSize, canvas, theme, availableWidth);
    };

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<StepSliderData>();
        if (!d) return;
        StepSlider::Render(canvas, n->computedRect, d->index, d->totalSteps,
                           d->opts, n->uiSize, n->hovered, n->pressed, n->disabled, theme, n->focused);
    };

    Control::SetupNode(ctx, node, ctrl, data->opts.stretch, data->opts.align);
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
    const UiTheme& theme,
    float /*availableWidth*/
) {
    if (totalSteps == 0) {
        totalSteps = opts.stepLabels.empty() ? 1 : opts.stepLabels.size();
    }
    const SliderMetrics& m = theme.GetSlider(size);
    const float tickFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
    const float extraLabelsH = (opts.showLabels && !opts.stepLabels.empty() && totalSteps > 1) ? (m.tickGap + tickFont * 1.3f) : 0.0f;
    const std::string_view stepStr = !opts.valueText.empty()
        ? opts.valueText.text
        : ((index < opts.stepLabels.size()) ? opts.stepLabels[index] : std::string_view{});
    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueText, stepStr,
        opts.showValue, opts.valuePlacement, opts.stretch, ctrl.width,
        extraLabelsH, size, canvas, theme,
        opts.stepLabels
    );
    return Control::Measure(cm.intrinsicControlSize, ctrl, opts.stretch, size, theme);
}

Vec2 StepSlider::Measure(
    const StepSliderOptions& opts,
    size_t index,
    size_t totalSteps,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, index, totalSteps, size, canvas, defaultTheme, availableWidth);
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
    const UiTheme& theme,
    bool focused
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const SliderMetrics& m = theme.GetSlider(size);
    const float tickFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
    const auto& pal = theme.palette;

    const std::string_view stepStr = !opts.valueText.empty()
        ? opts.valueText.text
        : ((index < opts.stepLabels.size()) ? opts.stepLabels[index] : std::string_view{});
    const float extraLabelsH = (opts.showLabels && !opts.stepLabels.empty() && totalSteps > 1) ? (m.tickGap + tickFont * 1.3f) : 0.0f;

    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueText, stepStr,
        opts.showValue, opts.valuePlacement, opts.stretch, ctrl.width,
        extraLabelsH, size, &canvas, theme,
        opts.stepLabels
    );
    Control::RenderFrame(canvas, bounds, ctrl, size, disabled, theme, hovered, active || focused);

    const Rect ctrlRect = Control::ResolveInnerRect(bounds, ctrl, size, theme);
    const float halfThumb = m.thumbSize * 0.5f;
    const float trackRegionX = ctrlRect.x + cm.leftGutterW;
    const float trackRegionW = std::max(m.thumbSize, ctrlRect.w - cm.leftGutterW - cm.rightGutterW);
    const float trackStartX = trackRegionX + halfThumb;
    const float trackWidth = std::max(0.0f, trackRegionW - m.thumbSize);
    const float contentOffsetY = std::max(0.0f, (ctrlRect.h - cm.intrinsicControlSize.y) * 0.5f);
    const float trackRowY = ctrlRect.y + contentOffsetY + cm.topValH;
    const float centerY = trackRowY + cm.trackRowH * 0.5f;

    const float ratio = (totalSteps > 1) ? static_cast<float>(index) / static_cast<float>(totalSteps - 1) : 0.0f;
    const float fillW = ratio * trackWidth;
    const float thumbCenterX = trackStartX + fillW;

    // 0. Draw companion textLeft / textRight and value readout
    RenderSliderCompanionTexts(
        canvas, ctrlRect, trackRegionX, trackRegionW, trackRowY, thumbCenterX,
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
        Color fillCol = ResolveSliderTrackColor(opts.color, disabled, theme);
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
                    active   ? ResolveSliderActiveThumbColor(opts.color, theme) :
                    hovered  ? pal.surface : pal.surface;
    Color thumbBorder = disabled ? pal.border.WithAlphaF(0.4f) :
                        focused  ? pal.focusRing :
                        (active || hovered) ? pal.accent : pal.borderHover;
    const float thumbBorderW = (active || hovered || focused) ? 2.5f : 2.0f;
    canvas.DrawRoundRect(thumbRect, halfThumb, thumbBg, thumbBorder, thumbBorderW);

    // 5. Draw step labels below track
    if (opts.showLabels && !opts.stepLabels.empty() && totalSteps > 1) {
        const float labelsY = centerY + halfThumb + m.tickGap;
        for (size_t i = 0; i < std::min(totalSteps, opts.stepLabels.size()); ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(totalSteps - 1);
            const float tickX = trackStartX + t * trackWidth;
            const std::string_view lbl = opts.stepLabels[i];
            const Vec2 dims = canvas.MeasureText(lbl, tickFont);
            float lblX = tickX - dims.x * 0.5f;
            lblX = std::clamp(lblX, trackRegionX, std::max(trackRegionX, trackRegionX + trackRegionW - dims.x));
            Color lblCol = (i == index) ? pal.accent : (disabled ? pal.textDisabled : pal.textSecondary);
            canvas.DrawText(lbl, Vec2(lblX, labelsY), tickFont, lblCol);
        }
    }
}

// ============================================================================
// RangeSlider (Two-Thumb Continuous / Stepped Interval Slider)
// ============================================================================

namespace {

enum class RangeDragMode : uint8_t {
    None = 0,
    LowThumb,
    HighThumb,
    IntervalSpan
};

struct RangeSliderState {
    RangeDragMode dragMode{RangeDragMode::None};
    uint8_t activeThumb{1}; // 0 = low thumb, 1 = high thumb
    double dragStartLow{0.0};
    double dragStartHigh{0.0};
    float dragStartMouseX{0.0f};
};

struct RangeSliderData {
    RangeSliderOptions opts;
    double lowValue{0.0};
    double highValue{1.0};
    double min{0.0};
    double max{1.0};
    uint8_t activeThumb{1};
};

struct ProgressBarData {
    ProgressBarOptions opts;
    double value{0.0};
    double min{0.0};
    double max{100.0};
};

void FormatRangeSliderPair(
    char* outBuf,
    size_t bufSize,
    double lowVal,
    double highVal,
    bool isInt,
    std::string_view format
) noexcept {
    int specCount = 0;
    size_t secondSpecPos = std::string_view::npos;
    for (size_t i = 0; i < format.size(); ++i) {
        if (format[i] == '%') {
            if (i + 1 < format.size() && format[i + 1] == '%') {
                ++i;
                continue;
            }
            ++specCount;
            if (specCount == 2) {
                secondSpecPos = i;
            }
        }
    }

    if (specCount >= 2 && secondSpecPos != std::string_view::npos) {
        const std::string_view part1 = format.substr(0, secondSpecPos);
        const std::string_view part2 = format.substr(secondSpecPos);
        char lowBuf[64];
        char highBuf[64];
        FormatSliderValue(lowBuf, sizeof(lowBuf), lowVal, isInt, part1);
        FormatSliderValue(highBuf, sizeof(highBuf), highVal, isInt, part2);
        std::snprintf(outBuf, bufSize, "%s%s", lowBuf, highBuf);
        return;
    }

    char lowBuf[48];
    char highBuf[48];
    FormatSliderValue(lowBuf, sizeof(lowBuf), lowVal, isInt, format);
    FormatSliderValue(highBuf, sizeof(highBuf), highVal, isInt, format);
    std::snprintf(outBuf, bufSize, "%s .. %s", lowBuf, highBuf);
}

} // namespace

Response RangeSlider::Run(
    Context& ctx,
    double* lowDouble,
    double* highDouble,
    float* lowFloat,
    float* highFloat,
    int64_t* lowInt,
    int64_t* highInt,
    double min,
    double max,
    const RangeSliderOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const UiTheme& activeTheme = ctx.GetTheme();

    RangeSliderOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    const SliderMetrics& m = activeTheme.GetSlider(activeSize);
    const double rangeMin = std::min(min, max);
    const double rangeMax = std::max(min, max);
    const bool isInt = (lowInt != nullptr || highInt != nullptr);
    const double effectiveStep = (optsCopy.step.has_value() && *optsCopy.step > 0.0) ? *optsCopy.step : (isInt ? 1.0 : 0.0);
    const double minSpan = std::clamp(std::max(0.0, optsCopy.minSpan), 0.0, rangeMax - rangeMin);

    double curLow = (lowDouble != nullptr) ? *lowDouble :
                    (lowFloat != nullptr)  ? static_cast<double>(*lowFloat) :
                    (lowInt != nullptr)    ? static_cast<double>(*lowInt) : rangeMin;
    double curHigh = (highDouble != nullptr) ? *highDouble :
                     (highFloat != nullptr)  ? static_cast<double>(*highFloat) :
                     (highInt != nullptr)    ? static_cast<double>(*highInt) : rangeMax;

    curLow = std::clamp(curLow, rangeMin, rangeMax);
    curHigh = std::clamp(curHigh, rangeMin, rangeMax);
    if (curHigh < curLow + minSpan) {
        curHigh = std::min(rangeMax, curLow + minSpan);
        curLow = std::max(rangeMin, curHigh - minSpan);
    }

    char initValBuf[96];
    FormatRangeSliderPair(initValBuf, sizeof(initValBuf), curLow, curHigh, isInt, optsCopy.valueFormat.text);
    char maxSpanBuf[96];
    FormatRangeSliderPair(maxSpanBuf, sizeof(maxSpanBuf), rangeMin, rangeMax, isInt, optsCopy.valueFormat.text);
    const std::string_view maxSpanView(maxSpanBuf);

    Rect prevBounds = ctx.GetPrevFrameBounds(id);
    auto& state = ctx.GetOrCreateState<RangeSliderState>(id);

    ctx.RegisterFocusable(id, effectiveDisabled);

    const ControlSpec ctrl = EffectiveSliderControl(optsCopy, activeSize, activeTheme);
    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        state.dragMode = RangeDragMode::None;
    } else {
        const float halfThumb = m.thumbSize * 0.5f;
        const SliderControlMetrics cm = ComputeSliderControlMetrics(
            optsCopy.textLeft, optsCopy.textRight, optsCopy.valueFormat, initValBuf,
            optsCopy.showValue, optsCopy.valuePlacement, optsCopy.stretch, ctrl.width,
            0.0f, activeSize, nullptr, activeTheme,
            std::span<const std::string_view>(&maxSpanView, 1)
        );
        const Rect ctrlRect = Control::ResolveInnerRect(prevBounds, ctrl, activeSize, activeTheme);
        const float trackRegionX = ctrlRect.x + cm.leftGutterW;
        const float trackRegionW = std::max(m.thumbSize, ctrlRect.w - cm.leftGutterW - cm.rightGutterW);
        const float trackStartX = trackRegionX + halfThumb;
        const float trackWidth = std::max(1.0f, trackRegionW - m.thumbSize);

        const double range = std::max(1e-9, rangeMax - rangeMin);
        const float lowRatio = static_cast<float>(std::clamp((curLow - rangeMin) / range, 0.0, 1.0));
        const float highRatio = static_cast<float>(std::clamp((curHigh - rangeMin) / range, 0.0, 1.0));
        const float lowThumbX = trackStartX + lowRatio * trackWidth;
        const float highThumbX = trackStartX + highRatio * trackWidth;

        if (res.clicked || (res.pressed && state.dragMode == RangeDragMode::None)) {
            const float mouseX = ctx.GetMousePos().x;
            const float distLow = std::abs(mouseX - lowThumbX);
            const float distHigh = std::abs(mouseX - highThumbX);
            const bool insideIntervalBar = (mouseX > lowThumbX + halfThumb * 1.15f) &&
                                           (mouseX < highThumbX - halfThumb * 1.15f) &&
                                           ((highThumbX - lowThumbX) > m.thumbSize * 1.8f);

            if (insideIntervalBar || ctx.GetKeyModifiers().shift) {
                state.dragMode = RangeDragMode::IntervalSpan;
            } else if (distLow < distHigh || (distLow == distHigh && mouseX < lowThumbX)) {
                state.dragMode = RangeDragMode::LowThumb;
                state.activeThumb = 0;
            } else {
                state.dragMode = RangeDragMode::HighThumb;
                state.activeThumb = 1;
            }
            state.dragStartLow = curLow;
            state.dragStartHigh = curHigh;
            state.dragStartMouseX = mouseX;
            ctx.SetActiveId(id);
            ctx.RequestFocus(id);
        }

        if (state.dragMode != RangeDragMode::None) {
            if (!ctx.IsMouseDown()) {
                state.dragMode = RangeDragMode::None;
                if (ctx.GetActiveId() == id) {
                    ctx.SetActiveId(Id::Null());
                }
            } else {
                res.pressed = true;
                const float mouseX = ctx.GetMousePos().x;
                auto quantize = [&](double v) -> double {
                    if (effectiveStep > 0.0) {
                        v = rangeMin + std::round((v - rangeMin) / effectiveStep) * effectiveStep;
                    }
                    return std::clamp(v, rangeMin, rangeMax);
                };

                if (state.dragMode == RangeDragMode::LowThumb) {
                    const float t = std::clamp((mouseX - trackStartX) / trackWidth, 0.0f, 1.0f);
                    double rawLow = quantize(rangeMin + t * (rangeMax - rangeMin));
                    rawLow = std::min(rawLow, curHigh - minSpan);
                    rawLow = std::clamp(rawLow, rangeMin, rangeMax);
                    if (rawLow != curLow) {
                        curLow = rawLow;
                        res.changed = true;
                        ctx.RequestRepaint(3);
                    }
                } else if (state.dragMode == RangeDragMode::HighThumb) {
                    const float t = std::clamp((mouseX - trackStartX) / trackWidth, 0.0f, 1.0f);
                    double rawHigh = quantize(rangeMin + t * (rangeMax - rangeMin));
                    rawHigh = std::max(rawHigh, curLow + minSpan);
                    rawHigh = std::clamp(rawHigh, rangeMin, rangeMax);
                    if (rawHigh != curHigh) {
                        curHigh = rawHigh;
                        res.changed = true;
                        ctx.RequestRepaint(3);
                    }
                } else if (state.dragMode == RangeDragMode::IntervalSpan) {
                    const double span = state.dragStartHigh - state.dragStartLow;
                    const double deltaVal = ((mouseX - state.dragStartMouseX) / trackWidth) * (rangeMax - rangeMin);
                    double newLow = state.dragStartLow + deltaVal;
                    if (newLow < rangeMin) newLow = rangeMin;
                    if (newLow + span > rangeMax) newLow = rangeMax - span;
                    newLow = quantize(newLow);
                    double newHigh = std::clamp(newLow + span, rangeMin, rangeMax);
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
            const double stepDelta = (effectiveStep > 0.0) ? effectiveStep : (rangeMax - rangeMin) * 0.05;
            double dir = 0.0;
            if (ctx.ConsumeKey(Key::Right) || ctx.ConsumeKey(Key::Up)) dir = 1.0;
            else if (ctx.ConsumeKey(Key::Left) || ctx.ConsumeKey(Key::Down)) dir = -1.0;

            if (dir != 0.0) {
                if (state.activeThumb == 0) {
                    const double nextLow = std::clamp(curLow + dir * stepDelta, rangeMin, curHigh - minSpan);
                    if (nextLow != curLow) {
                        curLow = nextLow;
                        res.changed = true;
                        ctx.RequestRepaint(3);
                    }
                } else {
                    const double nextHigh = std::clamp(curHigh + dir * stepDelta, curLow + minSpan, rangeMax);
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
    if (lowFloat != nullptr)   *lowFloat = static_cast<float>(curLow);
    if (highFloat != nullptr)  *highFloat = static_cast<float>(curHigh);
    if (lowInt != nullptr)     *lowInt = static_cast<int64_t>(std::round(curLow));
    if (highInt != nullptr)    *highInt = static_cast<int64_t>(std::round(curHigh));

    Node* node = ctx.CreateNode(NodeType::RangeSlider, id);
    node->theme = pushedTheme;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed || (state.dragMode != RangeDragMode::None);
    node->focused = ctx.IsFocused(id);
    node->widthMode = (optsCopy.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

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
    node->SetPayload(data);

    uint64_t lowBits = 0, highBits = 0;
    std::memcpy(&lowBits, &curLow, sizeof(double));
    std::memcpy(&highBits, &curHigh, sizeof(double));
    node->visualHash = detail::HashArgs(
        lowBits,
        highBits,
        state.activeThumb,
        data->opts.color,
        static_cast<uint8_t>(data->opts.stretch),
        static_cast<uint8_t>(data->opts.align),
        static_cast<uint8_t>(data->opts.valuePlacement),
        data->opts.background,
        data->opts.border,
        data->opts.width.value_or(0.0f),
        data->opts.height.value_or(0.0f),
        data->opts.showValue,
        data->opts.textLeft,
        data->opts.textRight,
        data->opts.valueFormat
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<RangeSliderData>();
        return RangeSlider::Measure(
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
        RangeSlider::Render(
            canvas, n->computedRect, d->lowValue, d->highValue, d->min, d->max,
            d->opts, n->uiSize, n->hovered, n->pressed, n->disabled, theme, n->focused, d->activeThumb
        );
    };

    Control::SetupNode(ctx, node, ctrl, data->opts.stretch, data->opts.align);
    ctx.AdaptChildToContainer(node);
    if (pushedTheme) {
        ctx.PopTheme();
    }
    return res;
}

Vec2 RangeSlider::Measure(
    const RangeSliderOptions& opts,
    double lowValue,
    double highValue,
    double min,
    double max,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const double rangeMin = std::min(min, max);
    const double rangeMax = std::max(min, max);
    const bool isIntStep = opts.step.has_value() && *opts.step >= 1.0 &&
                           std::abs(*opts.step - std::round(*opts.step)) < 0.0001;
    char valBuf[96];
    FormatRangeSliderPair(valBuf, sizeof(valBuf), lowValue, highValue, isIntStep, opts.valueFormat.text);
    char maxSpanBuf[96];
    FormatRangeSliderPair(maxSpanBuf, sizeof(maxSpanBuf), rangeMin, rangeMax, isIntStep, opts.valueFormat.text);
    const std::string_view maxSpanView(maxSpanBuf);

    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
        opts.showValue, opts.valuePlacement, opts.stretch, ctrl.width,
        0.0f, size, canvas, theme,
        std::span<const std::string_view>(&maxSpanView, 1)
    );
    return Control::Measure(cm.intrinsicControlSize, ctrl, opts.stretch, size, theme);
}

Vec2 RangeSlider::Measure(
    const RangeSliderOptions& opts,
    double lowValue,
    double highValue,
    double min,
    double max,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, lowValue, highValue, min, max, size, canvas, defaultTheme, availableWidth);
}

void RangeSlider::Render(
    ICanvas& canvas,
    Rect bounds,
    double lowValue,
    double highValue,
    double min,
    double max,
    const RangeSliderOptions& opts,
    UiSize size,
    bool hovered,
    bool active,
    bool disabled,
    const UiTheme& theme,
    bool focused,
    uint8_t activeThumb
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const SliderMetrics& m = theme.GetSlider(size);
    const auto& pal = theme.palette;
    const double rangeMin = std::min(min, max);
    const double rangeMax = std::max(min, max);
    const bool isIntStep = opts.step.has_value() && *opts.step >= 1.0 &&
                           std::abs(*opts.step - std::round(*opts.step)) < 0.0001;

    char valBuf[96];
    FormatRangeSliderPair(valBuf, sizeof(valBuf), lowValue, highValue, isIntStep, opts.valueFormat.text);
    char maxSpanBuf[96];
    FormatRangeSliderPair(maxSpanBuf, sizeof(maxSpanBuf), rangeMin, rangeMax, isIntStep, opts.valueFormat.text);
    const std::string_view maxSpanView(maxSpanBuf);

    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
        opts.showValue, opts.valuePlacement, opts.stretch, ctrl.width,
        0.0f, size, &canvas, theme,
        std::span<const std::string_view>(&maxSpanView, 1)
    );
    Control::RenderFrame(canvas, bounds, ctrl, size, disabled, theme, hovered, active || focused);

    const Rect ctrlRect = Control::ResolveInnerRect(bounds, ctrl, size, theme);
    const float halfThumb = m.thumbSize * 0.5f;
    const float trackRegionX = ctrlRect.x + cm.leftGutterW;
    const float trackRegionW = std::max(m.thumbSize, ctrlRect.w - cm.leftGutterW - cm.rightGutterW);
    const float trackStartX = trackRegionX + halfThumb;
    const float trackWidth = std::max(0.0f, trackRegionW - m.thumbSize);
    const float contentOffsetY = std::max(0.0f, (ctrlRect.h - cm.intrinsicControlSize.y) * 0.5f);
    const float trackRowY = ctrlRect.y + contentOffsetY + cm.topValH;
    const float centerY = trackRowY + cm.trackRowH * 0.5f;

    const double range = rangeMax - rangeMin;
    const float lowRatio = (range > 0.0) ? static_cast<float>(std::clamp((lowValue - rangeMin) / range, 0.0, 1.0)) : 0.0f;
    const float highRatio = (range > 0.0) ? static_cast<float>(std::clamp((highValue - rangeMin) / range, 0.0, 1.0)) : 1.0f;
    const float lowThumbX = trackStartX + std::min(lowRatio, highRatio) * trackWidth;
    const float highThumbX = trackStartX + std::max(lowRatio, highRatio) * trackWidth;
    const float midCenterX = (lowThumbX + highThumbX) * 0.5f;

    // 0. Draw companion textLeft / textRight and value readout
    RenderSliderCompanionTexts(
        canvas, ctrlRect, trackRegionX, trackRegionW, trackRowY, midCenterX,
        cm, opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
        opts.showValue, opts.valuePlacement, disabled, theme
    );

    // 1. Draw inactive background track
    const Rect trackBgRect(trackStartX, centerY - m.trackHeight * 0.5f, trackWidth, m.trackHeight);
    const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
    const Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
    canvas.DrawRoundRect(trackBgRect, m.trackHeight * 0.5f, trackBg, trackBorder, 1.0f);

    // 2. Draw active filled interval [lowThumbX .. highThumbX]
    const float fillW = std::max(0.0f, highThumbX - lowThumbX);
    if (fillW > 0.0f) {
        const Rect fillRect(lowThumbX, centerY - m.trackHeight * 0.5f, fillW, m.trackHeight);
        const Color fillCol = ResolveSliderTrackColor(opts.color, disabled, theme);
        canvas.DrawRoundRect(fillRect, m.trackHeight * 0.5f, fillCol);
    }

    // 3. Draw low and high thumb knobs
    auto drawThumb = [&](float thumbX, bool isFocusedThumb) {
        const Rect thumbRect(thumbX - halfThumb, centerY - halfThumb, m.thumbSize, m.thumbSize);
        const Color thumbBg = disabled ? pal.surface.WithAlphaF(0.5f) :
                              (active && isFocusedThumb) ? ResolveSliderActiveThumbColor(opts.color, theme) : pal.surface;
        const Color thumbBorder = disabled ? pal.border.WithAlphaF(0.4f) :
                                  (focused && isFocusedThumb) ? pal.focusRing :
                                  (active || hovered) ? pal.accent : pal.borderHover;
        const float thumbBorderW = (active || hovered || (focused && isFocusedThumb)) ? 2.5f : 2.0f;
        canvas.DrawRoundRect(thumbRect, halfThumb, thumbBg, thumbBorder, thumbBorderW);
    };

    drawThumb(lowThumbX, activeThumb == 0);
    drawThumb(highThumbX, activeThumb == 1);
}

// ============================================================================
// ProgressBar (Continuous Progress Bar & Segmented LED / VU LevelMeter)
// ============================================================================

Response ProgressBar::Run(
    Context& ctx,
    double value,
    double min,
    double max,
    const ProgressBarOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const UiTheme& activeTheme = ctx.GetTheme();

    ProgressBarOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    const double rangeMin = std::min(min, max);
    const double rangeMax = std::max(min, max);
    const double clampedVal = std::clamp(value, rangeMin, rangeMax);

    Response res = ctx.Interact(id);
    res.clicked = false;
    res.pressed = false;
    if (effectiveDisabled) {
        res.hovered = false;
    } else if (res.hovered && !optsCopy.tooltip.empty()) {
        ctx.SetTooltip(optsCopy.tooltip, ctx.GetPrevFrameBounds(id));
    }

    const ControlSpec ctrl = EffectiveSliderControl(optsCopy, activeSize, activeTheme);
    Node* node = ctx.CreateNode(NodeType::ProgressBar, id);
    node->theme = pushedTheme;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->widthMode = (optsCopy.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    auto* data = ctx.AllocPayload<ProgressBarData>();
    data->opts = optsCopy;
    data->opts.textLeft = ctx.AllocText(optsCopy.textLeft);
    data->opts.textRight = ctx.AllocText(optsCopy.textRight);
    data->opts.tooltip = ctx.AllocText(optsCopy.tooltip);
    data->opts.valueFormat = ctx.AllocText(optsCopy.valueFormat);
    data->value = clampedVal;
    data->min = rangeMin;
    data->max = rangeMax;
    node->SetPayload(data);

    uint64_t valBits = 0;
    std::memcpy(&valBits, &clampedVal, sizeof(double));
    const double peakVal = data->opts.peakHoldValue.value_or(-1e9);
    uint64_t peakBits = 0;
    std::memcpy(&peakBits, &peakVal, sizeof(double));

    node->visualHash = detail::HashArgs(
        valBits,
        peakBits,
        data->opts.segments,
        data->opts.multiZoneColor,
        data->opts.warningRatio,
        data->opts.dangerRatio,
        data->opts.trackHeight.value_or(0.0f),
        data->opts.color,
        static_cast<uint8_t>(data->opts.stretch),
        static_cast<uint8_t>(data->opts.align),
        static_cast<uint8_t>(data->opts.valuePlacement),
        data->opts.background,
        data->opts.border,
        data->opts.width.value_or(0.0f),
        data->opts.height.value_or(0.0f),
        data->opts.showValue,
        data->opts.textLeft,
        data->opts.textRight,
        data->opts.valueFormat
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<ProgressBarData>();
        return ProgressBar::Measure(
            d ? d->opts : ProgressBarOptions{},
            d ? d->value : 0.0,
            d ? d->min : 0.0,
            d ? d->max : 100.0,
            n->uiSize, canvas, theme, availableWidth
        );
    };

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<ProgressBarData>();
        if (!d) return;
        ProgressBar::Render(
            canvas, n->computedRect, d->value, d->min, d->max,
            d->opts, n->uiSize, n->hovered, n->disabled, theme
        );
    };

    Control::SetupNode(ctx, node, ctrl, data->opts.stretch, data->opts.align);
    ctx.AdaptChildToContainer(node);
    if (pushedTheme) {
        ctx.PopTheme();
    }
    return res;
}

Vec2 ProgressBar::Measure(
    const ProgressBarOptions& opts,
    double value,
    double min,
    double max,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const double rangeMin = std::min(min, max);
    const double rangeMax = std::max(min, max);
    const std::string_view fmt = opts.valueFormat.text.empty() ? "%.0f %%" : opts.valueFormat.text;
    char valBuf[64];
    FormatSliderValue(valBuf, sizeof(valBuf), value, false, fmt);

    UiText fmtText = opts.valueFormat;
    if (fmtText.text.empty()) {
        fmtText.text = fmt;
    }

    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, fmtText, valBuf,
        opts.showValue, opts.valuePlacement, opts.stretch, ctrl.width,
        0.0f, size, canvas, theme,
        {}, true, rangeMin, rangeMax, false
    );
    if (opts.trackHeight.has_value()) {
        const float minRowH = theme.Scale(*opts.trackHeight) + 4.0f;
        if (minRowH > cm.trackRowH) {
            cm.intrinsicControlSize.y += (minRowH - cm.trackRowH);
            cm.trackRowH = minRowH;
        }
    }
    return Control::Measure(cm.intrinsicControlSize, ctrl, opts.stretch, size, theme);
}

Vec2 ProgressBar::Measure(
    const ProgressBarOptions& opts,
    double value,
    double min,
    double max,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, value, min, max, size, canvas, defaultTheme, availableWidth);
}

void ProgressBar::Render(
    ICanvas& canvas,
    Rect bounds,
    double value,
    double min,
    double max,
    const ProgressBarOptions& opts,
    UiSize size,
    bool hovered,
    bool disabled,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const SliderMetrics& m = theme.GetSlider(size);
    const auto& pal = theme.palette;
    const double rangeMin = std::min(min, max);
    const double rangeMax = std::max(min, max);
    const std::string_view fmt = opts.valueFormat.text.empty() ? "%.0f %%" : opts.valueFormat.text;

    char valBuf[64];
    FormatSliderValue(valBuf, sizeof(valBuf), value, false, fmt);

    UiText fmtText = opts.valueFormat;
    if (fmtText.text.empty()) {
        fmtText.text = fmt;
    }

    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, fmtText, valBuf,
        opts.showValue, opts.valuePlacement, opts.stretch, ctrl.width,
        0.0f, size, &canvas, theme,
        {}, true, rangeMin, rangeMax, false
    );
    const float barH = opts.trackHeight.has_value()
        ? theme.Scale(*opts.trackHeight)
        : std::max(m.trackHeight * 1.55f, 8.0f);
    if (barH + 4.0f > cm.trackRowH) {
        cm.intrinsicControlSize.y += ((barH + 4.0f) - cm.trackRowH);
        cm.trackRowH = barH + 4.0f;
    }

    Control::RenderFrame(canvas, bounds, ctrl, size, disabled, theme, hovered, false);
    const Rect ctrlRect = Control::ResolveInnerRect(bounds, ctrl, size, theme);
    const float trackRegionX = ctrlRect.x + cm.leftGutterW;
    const float trackRegionW = std::max(12.0f, ctrlRect.w - cm.leftGutterW - cm.rightGutterW);
    const float contentOffsetY = std::max(0.0f, (ctrlRect.h - cm.intrinsicControlSize.y) * 0.5f);
    const float trackRowY = ctrlRect.y + contentOffsetY + cm.topValH;
    const float centerY = trackRowY + cm.trackRowH * 0.5f;

    const double range = rangeMax - rangeMin;
    const float ratio = (range > 0.0) ? static_cast<float>(std::clamp((value - rangeMin) / range, 0.0, 1.0)) : 0.0f;
    const float fillW = ratio * trackRegionW;
    const float tipX = trackRegionX + fillW;

    // 0. Draw companion textLeft / textRight and value readout
    RenderSliderCompanionTexts(
        canvas, ctrlRect, trackRegionX, trackRegionW, trackRowY, tipX,
        cm, opts.textLeft, opts.textRight, fmtText, valBuf,
        opts.showValue, opts.valuePlacement, disabled, theme
    );

    auto resolveZoneColor = [&](float normPos) -> Color {
        if (!opts.multiZoneColor) {
            return ResolveSliderTrackColor(opts.color, disabled, theme);
        }
        Color col;
        if (normPos >= opts.dangerRatio) {
            col = theme.GetVariant(UiVariant::Danger).bg;
        } else if (normPos >= opts.warningRatio) {
            col = theme.GetVariant(UiVariant::Warning).bg;
        } else if (!opts.color.HasAny() || opts.color == StateColor::Primary()) {
            col = theme.GetVariant(UiVariant::Success).bg;
        } else if (opts.color.normal.has_value() && opts.color.normal->a == 0) {
            col = pal.borderHover;
        } else {
            col = opts.color.normal.value_or(theme.GetVariant(UiVariant::Success).bg);
        }
        return disabled ? col.WithAlphaF(0.4f) : col;
    };

    const float barY = centerY - barH * 0.5f;

    if (opts.segments > 1) {
        // Segmented LED / VU Level Meter
        const uint32_t segCount = std::min<uint32_t>(opts.segments, 64u);
        const float gap = (segCount > 24) ? 1.5f : 2.5f;
        const float segW = std::max(2.0f, (trackRegionW - gap * static_cast<float>(segCount - 1)) / static_cast<float>(segCount));
        const float segRadius = std::min(2.5f, barH * 0.25f);
        const Color unlitBg = disabled ? pal.surfaceActive.WithAlphaF(0.35f) : pal.surfaceActive;
        const Color unlitBorder = disabled ? pal.border.WithAlphaF(0.25f) : pal.border.WithAlphaF(0.65f);

        int peakSegIdx = -1;
        if (opts.peakHoldValue.has_value() && range > 0.0) {
            const float peakRatio = static_cast<float>(std::clamp((*opts.peakHoldValue - rangeMin) / range, 0.0, 1.0));
            if (peakRatio > 0.001f) {
                peakSegIdx = std::clamp(static_cast<int>( std::ceil(peakRatio * static_cast<float>(segCount)) ) - 1, 0, static_cast<int>(segCount) - 1);
            }
        }

        for (uint32_t i = 0; i < segCount; ++i) {
            const float segX = trackRegionX + static_cast<float>(i) * (segW + gap);
            const float segThreshold = static_cast<float>(i + 1) / static_cast<float>(segCount);
            const float segMidRatio = (static_cast<float>(i) + 0.5f) / static_cast<float>(segCount);
            const bool isLit = (ratio >= segThreshold - (0.5f / static_cast<float>(segCount))) && (ratio > 0.001f);
            const bool isPeakSeg = (static_cast<int>(i) == peakSegIdx);

            const Rect segRect(segX, barY, segW, barH);
            if (isLit || isPeakSeg) {
                const Color segCol = resolveZoneColor(segMidRatio);
                canvas.DrawRoundRect(segRect, segRadius, segCol);
            } else {
                canvas.DrawRoundRect(segRect, segRadius, unlitBg, unlitBorder, 1.0f);
            }
        }
    } else {
        // Smooth continuous progress bar
        const Rect trackBgRect(trackRegionX, barY, trackRegionW, barH);
        const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
        const Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
        canvas.DrawRoundRect(trackBgRect, barH * 0.5f, trackBg, trackBorder, 1.0f);

        if (fillW > 0.5f) {
            const Rect fillRect(trackRegionX, barY, fillW, barH);
            const Color fillCol = resolveZoneColor(ratio);
            canvas.DrawRoundRect(fillRect, barH * 0.5f, fillCol);
        }

        // Optional peak-hold marker tick on smooth bar
        if (opts.peakHoldValue.has_value() && range > 0.0) {
            const float peakRatio = static_cast<float>(std::clamp((*opts.peakHoldValue - rangeMin) / range, 0.0, 1.0));
            if (peakRatio > 0.001f) {
                const float peakX = trackRegionX + peakRatio * trackRegionW;
                const Color peakCol = resolveZoneColor(peakRatio);
                canvas.DrawRoundRect(Rect(peakX - 1.5f, barY - 2.0f, 3.0f, barH + 4.0f), 1.5f, peakCol);
            }
        }
    }
}

} // namespace guikit
