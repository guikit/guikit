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
        const float defaultTrackW = opts.width.value_or(m.defaultWidth);
        float trackStartX = prevBounds.x + halfThumb;
        float trackWidth = std::max(1.0f, prevBounds.w - m.thumbSize);

        if (opts.layout == FormLayout::Inline) {
            float textW = 0.0f;
            if (!opts.label.empty()) {
                textW += static_cast<float>(opts.label.size()) * (m.labelFontSize * 0.55f);
            }
            if (opts.showValue) {
                textW += (textW > 0.0f ? 8.0f : 0.0f) + 50.0f;
            }
            if (!opts.description.empty()) {
                textW = std::max(textW, static_cast<float>(opts.description.size()) * (m.labelFontSize * 0.5f));
            }
            const float gap = textW > 0.0f ? 12.0f : 0.0f;
            const float availTrackW = std::max(1.0f, prevBounds.w - textW - gap - m.thumbSize);
            if (opts.stretch == StretchMode::Fill) {
                trackWidth = opts.width.has_value() ? std::min(availTrackW, *opts.width) : availTrackW;
            } else {
                trackWidth = std::min(defaultTrackW, availTrackW);
            }
            if (opts.placement == ElementOrder::Leading) {
                if (opts.stretch == StretchMode::Between) {
                    trackStartX = prevBounds.x + prevBounds.w - m.thumbSize - trackWidth + halfThumb;
                } else {
                    trackStartX = prevBounds.x + textW + gap + halfThumb;
                }
            } else {
                trackStartX = prevBounds.x + halfThumb;
            }
        } else {
            if (opts.stretch == StretchMode::Fill) {
                if (opts.width.has_value()) {
                    trackWidth = *opts.width;
                } else {
                    trackWidth = std::max(1.0f, prevBounds.w - m.thumbSize);
                }
                trackStartX = prevBounds.x + halfThumb;
            } else if (opts.stretch == StretchMode::Between || opts.placement == ElementOrder::Trailing) {
                trackWidth = std::min(defaultTrackW, std::max(1.0f, prevBounds.w - m.thumbSize));
                trackStartX = prevBounds.x + prevBounds.w - m.thumbSize - trackWidth + halfThumb;
            } else { // Compact
                trackWidth = std::min(defaultTrackW, std::max(1.0f, prevBounds.w - m.thumbSize));
                trackStartX = prevBounds.x + halfThumb;
            }
        }

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
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
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
        static_cast<uint8_t>(opts.stretch),
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.placement),
        static_cast<uint8_t>(opts.labelAlign),
        opts.width.value_or(0.0f),
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
    ICanvas* canvas,
    const UiTheme& theme
) {
    const SliderMetrics& m = theme.GetSlider(size);
    const float trackH = std::max(m.defaultHeight, m.thumbSize);
    const bool hasHeader = !opts.label.empty() || opts.showValue;
    const bool hasDesc = !opts.description.empty();
    const float headerH = hasHeader ? (m.labelFontSize * 1.3f) : 0.0f;
    const float descH = hasDesc ? (m.descGap + m.labelFontSize * 1.1f) : 0.0f;
    const float defaultTrackW = opts.width.value_or(m.defaultWidth) + m.thumbSize;

    if (opts.layout == FormLayout::Stacked) {
        float totalH = trackH;
        if (hasHeader) totalH += headerH + m.labelGap;
        if (hasDesc) totalH += descH;
        return Vec2{defaultTrackW, totalH};
    } else if (opts.layout == FormLayout::Top) {
        return Vec2{defaultTrackW, trackH + m.labelGap + headerH + descH};
    } else if (opts.layout == FormLayout::Bottom) {
        return Vec2{defaultTrackW, trackH + m.descGap + headerH + descH};
    } else { // FormLayout::Inline
        float textW = 0.0f;
        if (!opts.label.empty()) {
            if (canvas) {
                textW += canvas->MeasureText(opts.label, m.labelFontSize).x;
            } else {
                textW += static_cast<float>(opts.label.size()) * (m.labelFontSize * 0.55f);
            }
        }
        if (opts.showValue) {
            textW += (textW > 0.0f ? 8.0f : 0.0f) + 50.0f;
        }
        if (hasDesc) {
            float dW = 0.0f;
            if (canvas) {
                dW = canvas->MeasureText(opts.description, m.labelFontSize * 0.9f).x;
            } else {
                dW = static_cast<float>(opts.description.size()) * (m.labelFontSize * 0.5f);
            }
            textW = std::max(textW, dW);
        }
        const float gap = textW > 0.0f ? 12.0f : 0.0f;
        const float totalW = defaultTrackW + textW + gap;
        const float textH = headerH + (hasDesc ? descH : 0.0f);
        const float totalH = std::max(trackH, textH);
        return Vec2{totalW, totalH};
    }
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

    const bool hasHeader = !opts.label.empty() || opts.showValue;
    const bool hasDesc = !opts.description.empty();
    const float headerH = hasHeader ? (m.labelFontSize * 1.3f) : 0.0f;
    const float descH = hasDesc ? (m.labelFontSize * 0.9f * 1.2f) : 0.0f;
    const float trackRowH = std::max(m.defaultHeight, m.thumbSize);
    const float halfThumb = m.thumbSize * 0.5f;
    const float defaultTrackW = opts.width.value_or(m.defaultWidth);

    Rect headerRect;
    Rect descRect;
    float trackStartX = bounds.x + halfThumb;
    float trackWidth = 0.0f;
    float centerY = bounds.y + trackRowH * 0.5f;

    if (opts.layout == FormLayout::Stacked) {
        float curY = bounds.y;
        if (hasHeader) {
            headerRect = Rect(bounds.x, curY, bounds.w, headerH);
            curY += headerH + m.labelGap;
        }
        centerY = curY + trackRowH * 0.5f;
        curY += trackRowH + m.descGap;
        if (hasDesc) {
            descRect = Rect(bounds.x, curY, bounds.w, descH);
        }

        const float availTrackW = std::max(0.0f, bounds.w - m.thumbSize);
        if (opts.stretch == StretchMode::Fill) {
            trackWidth = opts.width.has_value() ? std::min(availTrackW, *opts.width) : availTrackW;
            trackStartX = bounds.x + halfThumb;
        } else if (opts.stretch == StretchMode::Between || opts.placement == ElementOrder::Trailing) {
            trackWidth = std::min(defaultTrackW, availTrackW);
            trackStartX = bounds.x + bounds.w - m.thumbSize - trackWidth + halfThumb;
        } else { // Compact
            trackWidth = std::min(defaultTrackW, availTrackW);
            trackStartX = bounds.x + halfThumb;
        }
    } else if (opts.layout == FormLayout::Top) {
        float curY = bounds.y;
        if (hasHeader) {
            headerRect = Rect(bounds.x, curY, bounds.w, headerH);
            curY += headerH + (hasDesc ? m.descGap : 0.0f);
        }
        if (hasDesc) {
            descRect = Rect(bounds.x, curY, bounds.w, descH);
            curY += descH;
        }
        curY += m.labelGap;
        centerY = curY + trackRowH * 0.5f;

        const float availTrackW = std::max(0.0f, bounds.w - m.thumbSize);
        if (opts.stretch == StretchMode::Fill) {
            trackWidth = opts.width.has_value() ? std::min(availTrackW, *opts.width) : availTrackW;
            trackStartX = bounds.x + halfThumb;
        } else if (opts.stretch == StretchMode::Between || opts.placement == ElementOrder::Trailing) {
            trackWidth = std::min(defaultTrackW, availTrackW);
            trackStartX = bounds.x + bounds.w - m.thumbSize - trackWidth + halfThumb;
        } else { // Compact
            trackWidth = std::min(defaultTrackW, availTrackW);
            trackStartX = bounds.x + halfThumb;
        }
    } else if (opts.layout == FormLayout::Bottom) {
        float curY = bounds.y;
        centerY = curY + trackRowH * 0.5f;
        curY += trackRowH + m.descGap;
        if (hasHeader) {
            headerRect = Rect(bounds.x, curY, bounds.w, headerH);
            curY += headerH + (hasDesc ? m.descGap : 0.0f);
        }
        if (hasDesc) {
            descRect = Rect(bounds.x, curY, bounds.w, descH);
        }

        const float availTrackW = std::max(0.0f, bounds.w - m.thumbSize);
        if (opts.stretch == StretchMode::Fill) {
            trackWidth = opts.width.has_value() ? std::min(availTrackW, *opts.width) : availTrackW;
            trackStartX = bounds.x + halfThumb;
        } else if (opts.stretch == StretchMode::Between || opts.placement == ElementOrder::Trailing) {
            trackWidth = std::min(defaultTrackW, availTrackW);
            trackStartX = bounds.x + bounds.w - m.thumbSize - trackWidth + halfThumb;
        } else { // Compact
            trackWidth = std::min(defaultTrackW, availTrackW);
            trackStartX = bounds.x + halfThumb;
        }
    } else { // FormLayout::Inline
        centerY = bounds.y + bounds.h * 0.5f;
        float textW = 0.0f;
        if (!opts.label.empty()) {
            textW += canvas.MeasureText(opts.label, m.labelFontSize).x;
        }
        if (opts.showValue) {
            textW += (textW > 0.0f ? 8.0f : 0.0f) + canvas.MeasureText(valBuf, m.labelFontSize).x;
        }
        if (hasDesc) {
            textW = std::max(textW, canvas.MeasureText(opts.description, m.labelFontSize * 0.9f).x);
        }

        const float gap = textW > 0.0f ? 12.0f : 0.0f;
        const float availTrackW = std::max(0.0f, bounds.w - textW - gap - m.thumbSize);
        if (opts.stretch == StretchMode::Fill) {
            trackWidth = opts.width.has_value() ? std::min(availTrackW, *opts.width) : availTrackW;
        } else {
            trackWidth = std::min(defaultTrackW, availTrackW);
        }

        Rect textRect;
        if (opts.placement == ElementOrder::Leading) {
            // Text left, track right
            textRect = Rect(bounds.x, bounds.y, textW, bounds.h);
            if (opts.stretch == StretchMode::Between) {
                trackStartX = bounds.x + bounds.w - m.thumbSize - trackWidth + halfThumb;
            } else {
                trackStartX = bounds.x + textW + gap + halfThumb;
            }
        } else {
            // Track left, text right
            trackStartX = bounds.x + halfThumb;
            float textX = bounds.x + trackWidth + m.thumbSize + gap;
            if (opts.stretch == StretchMode::Between) {
                textX = std::max(textX, bounds.x + bounds.w - textW);
            }
            textRect = Rect(textX, bounds.y, textW, bounds.h);
        }

        if (hasHeader && hasDesc) {
            const float totalTextH = headerH + m.descGap + descH;
            const float startTextY = bounds.y + (bounds.h - totalTextH) * 0.5f;
            headerRect = Rect(textRect.x, startTextY, textRect.w, headerH);
            descRect = Rect(textRect.x, startTextY + headerH + m.descGap, textRect.w, descH);
        } else if (hasHeader) {
            headerRect = Rect(textRect.x, bounds.y + (bounds.h - headerH) * 0.5f, textRect.w, headerH);
        } else if (hasDesc) {
            descRect = Rect(textRect.x, bounds.y + (bounds.h - descH) * 0.5f, textRect.w, descH);
        }
    }

    const float alignFactor = (opts.labelAlign == Align::End) ? 1.0f : ((opts.labelAlign == Align::Center) ? 0.5f : 0.0f);
    const Color labelCol = disabled ? pal.textDisabled : pal.textPrimary;
    const Color descCol = disabled ? pal.textDisabled : pal.textSecondary;

    if (hasHeader && headerRect.w > 0.0f && headerRect.h > 0.0f) {
        if (opts.showValue) {
            const Vec2 valDims = canvas.MeasureText(valBuf, m.labelFontSize);
            const Color valCol = disabled ? pal.textDisabled : pal.textSecondary;
            float valX = headerRect.Right() - valDims.x;
            Rect lblRect = Rect(headerRect.x, headerRect.y, std::max(0.0f, headerRect.w - valDims.x - 8.0f), headerRect.h);

            if (opts.placement == ElementOrder::Trailing && opts.layout != FormLayout::Inline) {
                valX = headerRect.x;
                lblRect = Rect(headerRect.x + valDims.x + 8.0f, headerRect.y, std::max(0.0f, headerRect.w - valDims.x - 8.0f), headerRect.h);
            }

            canvas.DrawText(valBuf, Vec2(valX, headerRect.y + (headerRect.h - m.labelFontSize) * 0.5f), m.labelFontSize, valCol);
            if (!opts.label.empty() && lblRect.w > 0.0f) {
                canvas.DrawTextBox(opts.label, lblRect, m.labelFontSize, labelCol, alignFactor, 0.5f);
            }
        } else if (!opts.label.empty()) {
            canvas.DrawTextBox(opts.label, headerRect, m.labelFontSize, labelCol, alignFactor, 0.5f);
        }
    }

    if (hasDesc && descRect.w > 0.0f && descRect.h > 0.0f) {
        canvas.DrawTextBox(opts.description, descRect, m.labelFontSize * 0.9f, descCol, alignFactor, 0.5f);
    }

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
        Color baseFill = (opts.variant == UiVariant::Ghost) ? pal.borderHover : variantCol.bg;
        Color fillCol = disabled ? baseFill.WithAlphaF(0.4f) : baseFill;
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
        const float defaultTrackW = opts.width.value_or(m.defaultWidth);
        float trackStartX = prevBounds.x + halfThumb;
        float trackWidth = std::max(1.0f, prevBounds.w - m.thumbSize);

        if (opts.layout == FormLayout::Inline) {
            float textW = 0.0f;
            if (!opts.label.empty()) {
                textW += static_cast<float>(opts.label.size()) * (m.labelFontSize * 0.55f);
            }
            if (opts.showValue) {
                textW += (textW > 0.0f ? 8.0f : 0.0f) + 50.0f;
            }
            if (!opts.description.empty()) {
                textW = std::max(textW, static_cast<float>(opts.description.size()) * (m.labelFontSize * 0.5f));
            }
            const float gap = textW > 0.0f ? 12.0f : 0.0f;
            const float availTrackW = std::max(1.0f, prevBounds.w - textW - gap - m.thumbSize);
            if (opts.stretch == StretchMode::Fill) {
                trackWidth = opts.width.has_value() ? std::min(availTrackW, *opts.width) : availTrackW;
            } else {
                trackWidth = std::min(defaultTrackW, availTrackW);
            }
            if (opts.placement == ElementOrder::Leading) {
                if (opts.stretch == StretchMode::Between) {
                    trackStartX = prevBounds.x + prevBounds.w - m.thumbSize - trackWidth + halfThumb;
                } else {
                    trackStartX = prevBounds.x + textW + gap + halfThumb;
                }
            } else {
                trackStartX = prevBounds.x + halfThumb;
            }
        } else {
            if (opts.stretch == StretchMode::Fill) {
                if (opts.width.has_value()) {
                    trackWidth = *opts.width;
                } else {
                    trackWidth = std::max(1.0f, prevBounds.w - m.thumbSize);
                }
                trackStartX = prevBounds.x + halfThumb;
            } else if (opts.stretch == StretchMode::Between || opts.placement == ElementOrder::Trailing) {
                trackWidth = std::min(defaultTrackW, std::max(1.0f, prevBounds.w - m.thumbSize));
                trackStartX = prevBounds.x + prevBounds.w - m.thumbSize - trackWidth + halfThumb;
            } else { // Compact
                trackWidth = std::min(defaultTrackW, std::max(1.0f, prevBounds.w - m.thumbSize));
                trackStartX = prevBounds.x + halfThumb;
            }
        }

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
    data->opts.label = ctx.AllocString(opts.label);
    if (!opts.description.empty()) {
        data->opts.description = ctx.AllocString(opts.description);
    }
    if (!opts.valueText.empty()) {
        data->opts.valueText = ctx.AllocString(opts.valueText);
    }
    data->index = curIdx;
    data->totalSteps = totalSteps;
    node->payload = data;

    node->visualHash = detail::HashArgs(
        curIdx,
        totalSteps,
        opts.variant,
        static_cast<uint8_t>(opts.stretch),
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.placement),
        static_cast<uint8_t>(opts.labelAlign),
        opts.width.value_or(0.0f),
        opts.showTicks,
        opts.showLabels,
        opts.showValue,
        data->opts.label,
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
    return res;
}

Vec2 StepSlider::Measure(
    const StepSliderOptions& opts,
    size_t /*index*/,
    size_t /*totalSteps*/,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const SliderMetrics& m = theme.GetSlider(size);
    const float extraLabelsH = (opts.showLabels && !opts.stepLabels.empty()) ? (m.tickGap + m.tickFontSize * 1.3f) : 0.0f;
    const float trackH = std::max(m.defaultHeight, m.thumbSize) + extraLabelsH;
    const bool hasHeader = !opts.label.empty() || opts.showValue;
    const bool hasDesc = !opts.description.empty();
    const float headerH = hasHeader ? (m.labelFontSize * 1.3f) : 0.0f;
    const float descH = hasDesc ? (m.descGap + m.labelFontSize * 1.1f) : 0.0f;
    const float defaultTrackW = opts.width.value_or(m.defaultWidth) + m.thumbSize;

    if (opts.layout == FormLayout::Stacked) {
        float totalH = trackH;
        if (hasHeader) totalH += headerH + m.labelGap;
        if (hasDesc) totalH += descH;
        return Vec2{defaultTrackW, totalH};
    } else if (opts.layout == FormLayout::Top) {
        return Vec2{defaultTrackW, trackH + m.labelGap + headerH + descH};
    } else if (opts.layout == FormLayout::Bottom) {
        return Vec2{defaultTrackW, trackH + m.descGap + headerH + descH};
    } else { // FormLayout::Inline
        float textW = 0.0f;
        if (!opts.label.empty()) {
            if (canvas) {
                textW += canvas->MeasureText(opts.label, m.labelFontSize).x;
            } else {
                textW += static_cast<float>(opts.label.size()) * (m.labelFontSize * 0.55f);
            }
        }
        if (opts.showValue) {
            textW += (textW > 0.0f ? 8.0f : 0.0f) + 50.0f;
        }
        if (hasDesc) {
            float dW = 0.0f;
            if (canvas) {
                dW = canvas->MeasureText(opts.description, m.labelFontSize * 0.9f).x;
            } else {
                dW = static_cast<float>(opts.description.size()) * (m.labelFontSize * 0.5f);
            }
            textW = std::max(textW, dW);
        }
        const float gap = textW > 0.0f ? 12.0f : 0.0f;
        const float totalW = defaultTrackW + textW + gap;
        const float textH = headerH + (hasDesc ? descH : 0.0f);
        const float totalH = std::max(trackH, textH);
        return Vec2{totalW, totalH};
    }
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

    std::string_view valStr = !opts.valueText.empty() ? opts.valueText : (index < opts.stepLabels.size() ? opts.stepLabels[index] : "");

    const bool hasHeader = !opts.label.empty() || opts.showValue;
    const bool hasDesc = !opts.description.empty();
    const float headerH = hasHeader ? (m.labelFontSize * 1.3f) : 0.0f;
    const float descH = hasDesc ? (m.labelFontSize * 0.9f * 1.2f) : 0.0f;
    const float trackRowH = std::max(m.defaultHeight, m.thumbSize);
    const float extraLabelsH = (opts.showLabels && !opts.stepLabels.empty() && totalSteps > 1) ? (m.tickGap + m.tickFontSize * 1.3f) : 0.0f;
    const float controlH = trackRowH + extraLabelsH;
    const float halfThumb = m.thumbSize * 0.5f;
    const float defaultTrackW = opts.width.value_or(m.defaultWidth);

    Rect headerRect;
    Rect descRect;
    float trackStartX = bounds.x + halfThumb;
    float trackWidth = 0.0f;
    float centerY = bounds.y + trackRowH * 0.5f;

    if (opts.layout == FormLayout::Stacked) {
        float curY = bounds.y;
        if (hasHeader) {
            headerRect = Rect(bounds.x, curY, bounds.w, headerH);
            curY += headerH + m.labelGap;
        }
        centerY = curY + trackRowH * 0.5f;
        curY += controlH + m.descGap;
        if (hasDesc) {
            descRect = Rect(bounds.x, curY, bounds.w, descH);
        }

        const float availTrackW = std::max(0.0f, bounds.w - m.thumbSize);
        if (opts.stretch == StretchMode::Fill) {
            trackWidth = opts.width.has_value() ? std::min(availTrackW, *opts.width) : availTrackW;
            trackStartX = bounds.x + halfThumb;
        } else if (opts.stretch == StretchMode::Between || opts.placement == ElementOrder::Trailing) {
            trackWidth = std::min(defaultTrackW, availTrackW);
            trackStartX = bounds.x + bounds.w - m.thumbSize - trackWidth + halfThumb;
        } else { // Compact
            trackWidth = std::min(defaultTrackW, availTrackW);
            trackStartX = bounds.x + halfThumb;
        }
    } else if (opts.layout == FormLayout::Top) {
        float curY = bounds.y;
        if (hasHeader) {
            headerRect = Rect(bounds.x, curY, bounds.w, headerH);
            curY += headerH + (hasDesc ? m.descGap : 0.0f);
        }
        if (hasDesc) {
            descRect = Rect(bounds.x, curY, bounds.w, descH);
            curY += descH;
        }
        curY += m.labelGap;
        centerY = curY + trackRowH * 0.5f;

        const float availTrackW = std::max(0.0f, bounds.w - m.thumbSize);
        if (opts.stretch == StretchMode::Fill) {
            trackWidth = opts.width.has_value() ? std::min(availTrackW, *opts.width) : availTrackW;
            trackStartX = bounds.x + halfThumb;
        } else if (opts.stretch == StretchMode::Between || opts.placement == ElementOrder::Trailing) {
            trackWidth = std::min(defaultTrackW, availTrackW);
            trackStartX = bounds.x + bounds.w - m.thumbSize - trackWidth + halfThumb;
        } else { // Compact
            trackWidth = std::min(defaultTrackW, availTrackW);
            trackStartX = bounds.x + halfThumb;
        }
    } else if (opts.layout == FormLayout::Bottom) {
        float curY = bounds.y;
        centerY = curY + trackRowH * 0.5f;
        curY += controlH + m.descGap;
        if (hasHeader) {
            headerRect = Rect(bounds.x, curY, bounds.w, headerH);
            curY += headerH + (hasDesc ? m.descGap : 0.0f);
        }
        if (hasDesc) {
            descRect = Rect(bounds.x, curY, bounds.w, descH);
        }

        const float availTrackW = std::max(0.0f, bounds.w - m.thumbSize);
        if (opts.stretch == StretchMode::Fill) {
            trackWidth = opts.width.has_value() ? std::min(availTrackW, *opts.width) : availTrackW;
            trackStartX = bounds.x + halfThumb;
        } else if (opts.stretch == StretchMode::Between || opts.placement == ElementOrder::Trailing) {
            trackWidth = std::min(defaultTrackW, availTrackW);
            trackStartX = bounds.x + bounds.w - m.thumbSize - trackWidth + halfThumb;
        } else { // Compact
            trackWidth = std::min(defaultTrackW, availTrackW);
            trackStartX = bounds.x + halfThumb;
        }
    } else { // FormLayout::Inline
        centerY = bounds.y + (trackRowH * 0.5f);
        float textW = 0.0f;
        if (!opts.label.empty()) {
            textW += canvas.MeasureText(opts.label, m.labelFontSize).x;
        }
        if (opts.showValue && !valStr.empty()) {
            textW += (textW > 0.0f ? 8.0f : 0.0f) + canvas.MeasureText(valStr, m.labelFontSize).x;
        }
        if (hasDesc) {
            textW = std::max(textW, canvas.MeasureText(opts.description, m.labelFontSize * 0.9f).x);
        }

        const float gap = textW > 0.0f ? 12.0f : 0.0f;
        const float availTrackW = std::max(0.0f, bounds.w - textW - gap - m.thumbSize);
        if (opts.stretch == StretchMode::Fill) {
            trackWidth = opts.width.has_value() ? std::min(availTrackW, *opts.width) : availTrackW;
        } else {
            trackWidth = std::min(defaultTrackW, availTrackW);
        }

        Rect textRect;
        if (opts.placement == ElementOrder::Leading) {
            // Text left, track right
            textRect = Rect(bounds.x, bounds.y, textW, bounds.h);
            if (opts.stretch == StretchMode::Between) {
                trackStartX = bounds.x + bounds.w - m.thumbSize - trackWidth + halfThumb;
            } else {
                trackStartX = bounds.x + textW + gap + halfThumb;
            }
        } else {
            // Track left, text right
            trackStartX = bounds.x + halfThumb;
            float textX = bounds.x + trackWidth + m.thumbSize + gap;
            if (opts.stretch == StretchMode::Between) {
                textX = std::max(textX, bounds.x + bounds.w - textW);
            }
            textRect = Rect(textX, bounds.y, textW, bounds.h);
        }

        if (hasHeader && hasDesc) {
            const float totalTextH = headerH + m.descGap + descH;
            const float startTextY = bounds.y + (bounds.h - totalTextH) * 0.5f;
            headerRect = Rect(textRect.x, startTextY, textRect.w, headerH);
            descRect = Rect(textRect.x, startTextY + headerH + m.descGap, textRect.w, descH);
        } else if (hasHeader) {
            headerRect = Rect(textRect.x, bounds.y + (bounds.h - headerH) * 0.5f, textRect.w, headerH);
        } else if (hasDesc) {
            descRect = Rect(textRect.x, bounds.y + (bounds.h - descH) * 0.5f, textRect.w, descH);
        }
    }

    const float alignFactor = (opts.labelAlign == Align::End) ? 1.0f : ((opts.labelAlign == Align::Center) ? 0.5f : 0.0f);
    const Color labelCol = disabled ? pal.textDisabled : pal.textPrimary;
    const Color descCol = disabled ? pal.textDisabled : pal.textSecondary;

    if (hasHeader && headerRect.w > 0.0f && headerRect.h > 0.0f) {
        if (opts.showValue && !valStr.empty()) {
            const Vec2 valDims = canvas.MeasureText(valStr, m.labelFontSize);
            Color valCol = disabled ? pal.textDisabled : (opts.variant != UiVariant::Default ? (variantCol.bg.a > 0 ? variantCol.bg : pal.textPrimary) : pal.accent);
            float valX = headerRect.Right() - valDims.x;
            Rect lblRect = Rect(headerRect.x, headerRect.y, std::max(0.0f, headerRect.w - valDims.x - 8.0f), headerRect.h);

            if (opts.placement == ElementOrder::Trailing && opts.layout != FormLayout::Inline) {
                valX = headerRect.x;
                lblRect = Rect(headerRect.x + valDims.x + 8.0f, headerRect.y, std::max(0.0f, headerRect.w - valDims.x - 8.0f), headerRect.h);
            }

            canvas.DrawText(valStr, Vec2(valX, headerRect.y + (headerRect.h - m.labelFontSize) * 0.5f), m.labelFontSize, valCol);
            if (!opts.label.empty() && lblRect.w > 0.0f) {
                canvas.DrawTextBox(opts.label, lblRect, m.labelFontSize, labelCol, alignFactor, 0.5f);
            }
        } else if (!opts.label.empty()) {
            canvas.DrawTextBox(opts.label, headerRect, m.labelFontSize, labelCol, alignFactor, 0.5f);
        }
    }

    if (hasDesc && descRect.w > 0.0f && descRect.h > 0.0f) {
        canvas.DrawTextBox(opts.description, descRect, m.labelFontSize * 0.9f, descCol, alignFactor, 0.5f);
    }

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
}

} // namespace guikit
