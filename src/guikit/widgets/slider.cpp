#include "guikit/widgets/slider.hpp"
#include <cstring>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/style_painter.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/format.h"
#include "keel/math.h"

namespace guikit {

namespace {

struct SliderData {
    SliderOptions opts;
    keel::f64 value{0.0};
    keel::f64 min{0.0};
    keel::f64 max{1.0};
    bool isInt{false};
    keel::StringRef formattedValue{};
};

struct StepSliderData {
    StepSliderOptions opts;
    keel::usize index{0};
    keel::usize totalSteps{1};
};

void FormatSliderValue(
    char* valBuf,
    keel::usize bufSize,
    keel::f64 value,
    bool isInt,
    keel::StringRef format
) noexcept {
    if (!format.empty()) {
        if (isInt) {
            keel::format_buf(valBuf, bufSize, format, keel::round_i64(value));
        } else {
            keel::format_buf(valBuf, bufSize, format, value);
        }
    } else if (isInt) {
        keel::format_buf(valBuf, bufSize, "{}", keel::round_i64(value));
    } else if (keel::abs(value - keel::round(value)) < 0.0001) {
        keel::format_buf(valBuf, bufSize, "{:.0f}", value);
    } else {
        keel::format_buf(valBuf, bufSize, "{:.1f}", value);
    }
}

struct SliderControlMetrics {
    keel::f32 leftTextW{0.0f};
    keel::f32 leftTextFont{12.0f};
    keel::f32 rightTextW{0.0f};
    keel::f32 rightTextFont{12.0f};
    keel::f32 valW{0.0f};
    keel::f32 valReserveW{0.0f};
    keel::f32 valLineH{0.0f};
    keel::f32 valFont{12.0f};
    keel::f32 leftGutterW{0.0f};
    keel::f32 rightGutterW{0.0f};
    keel::f32 topValH{0.0f};
    keel::f32 bottomValH{0.0f};
    keel::f32 trackRowH{0.0f};
    keel::f32 extraLabelsH{0.0f};
    // Vertical orientation: gutters along the main (Y) axis and the track column width
    keel::f32 leftTextLineH{0.0f};
    keel::f32 rightTextLineH{0.0f};
    keel::f32 endGutterH{0.0f};    ///< Above the track (max end): textRight, Trailing/Top value
    keel::f32 startGutterH{0.0f};  ///< Below the track (min end): Leading/Bottom value, textLeft
    keel::f32 crossW{0.0f};        ///< Track + thumb column width
    keel::f32 extraLabelsW{0.0f};  ///< StepSlider labels column right of the track
    keel::f32 thumbValW{0.0f};     ///< Thumb value readout right of the track (and labels)
    Vec2 intrinsicControlSize{};
};

constexpr keel::f32 kSliderInnerGap = 8.0f;
constexpr keel::f32 kSliderVerticalTextGap = 4.0f;

[[nodiscard]] constexpr bool IsVerticalSlider(Orientation orientation) noexcept {
    return orientation == Orientation::Vertical;
}

SliderControlMetrics ComputeSliderControlMetrics(
    const UiText& textLeft,
    const UiText& textRight,
    const UiText& valueStyle,
    keel::StringRef effectiveValStr,
    bool showValue,
    Placement valuePlacement,
    AxisSize width,
    keel::f32 extraLabelsBand,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::ArrayRef<keel::StringRef> extraValCandidates = {},
    bool hasNumericRange = false,
    keel::f64 rangeMin = 0.0,
    keel::f64 rangeMax = 1.0,
    bool isInt = false,
    Orientation orientation = Orientation::Horizontal
) noexcept {
    const SliderMetrics& m = theme.GetSlider(size);
    const keel::f32 innerGap = kSliderInnerGap;
    SliderControlMetrics cm{};
    cm.trackRowH = keel::max(m.defaultHeight, m.thumbSize);
    if (IsVerticalSlider(orientation)) {
        cm.extraLabelsW = extraLabelsBand;   // labels column right of the track
    } else {
        cm.extraLabelsH = extraLabelsBand;   // labels row below the track
    }

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
            auto measureNumericFmt = [&](keel::StringRef fmt) {
                char buf[64];
                FormatSliderValue(buf, sizeof(buf), rangeMin, isInt, fmt);
                cm.valReserveW = keel::max(cm.valReserveW, TextUtil::MeasureWidth(keel::StringRef(buf), cm.valFont, canvas));

                FormatSliderValue(buf, sizeof(buf), rangeMax, isInt, fmt);
                cm.valReserveW = keel::max(cm.valReserveW, TextUtil::MeasureWidth(keel::StringRef(buf), cm.valFont, canvas));

                const keel::f64 sampleVal = (rangeMax > rangeMin + 0.1)
                    ? (rangeMax - 0.1)
                    : (rangeMin + (rangeMax - rangeMin) * 0.88);
                FormatSliderValue(buf, sizeof(buf), sampleVal, isInt, fmt);
                cm.valReserveW = keel::max(cm.valReserveW, TextUtil::MeasureWidth(keel::StringRef(buf), cm.valFont, canvas));
            };

            measureNumericFmt(valueStyle.text);
            for (keel::StringRef var : valueStyle.variants) {
                if (!var.empty()) {
                    measureNumericFmt(var);
                }
            }
        } else {
            cm.valReserveW = keel::max(cm.valReserveW, TextUtil::MeasureWidth(valueStyle, cm.valFont, canvas));
            for (keel::StringRef cand : extraValCandidates) {
                if (!cand.empty()) {
                    cm.valReserveW = keel::max(cm.valReserveW, TextUtil::MeasureWidth(cand, cm.valFont, canvas));
                }
            }
        }
    }

    if (IsVerticalSlider(orientation)) {
        // Fader: track column + optional Thumb readout; texts and value stacked above / below the track
        const auto lineH = [&](keel::f32 font) { return canvas ? canvas->GetLineHeight(font) : theme.GetLineHeight(font); };
        cm.crossW = keel::max(m.defaultHeight, m.thumbSize);
        if (showValue && valuePlacement == Placement::Thumb) {
            cm.thumbValW = innerGap + cm.valReserveW;
        }
        if (hasRightText) {
            cm.rightTextLineH = lineH(cm.rightTextFont);
            cm.endGutterH += cm.rightTextLineH + kSliderVerticalTextGap;
        }
        if (showValue && (valuePlacement == Placement::Trailing || valuePlacement == Placement::Top)) {
            cm.endGutterH += cm.valLineH + 2.0f;
        }
        if (showValue && (valuePlacement == Placement::Leading || valuePlacement == Placement::Bottom)) {
            cm.startGutterH += 2.0f + cm.valLineH;
        }
        if (hasLeftText) {
            cm.leftTextLineH = lineH(cm.leftTextFont);
            cm.startGutterH += kSliderVerticalTextGap + cm.leftTextLineH;
        }
        const bool stackedValue = showValue && valuePlacement != Placement::Thumb;
        cm.intrinsicControlSize = {
            keel::max(keel::max(cm.crossW + cm.extraLabelsW + cm.thumbValW, stackedValue ? cm.valReserveW : 0.0f),
                      keel::max(cm.leftTextW, cm.rightTextW)),
            cm.endGutterH + m.defaultWidth + m.thumbSize + cm.startGutterH
        };
        return cm;
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

    const keel::f32 trackW = width.IsFixed() ? width.Dp() : m.defaultWidth;
    cm.intrinsicControlSize = {
        cm.leftGutterW + trackW + m.thumbSize + cm.rightGutterW,
        cm.topValH + cm.trackRowH + cm.extraLabelsH + cm.bottomValH
    };
    return cm;
}

/// Track geometry along the main axis, shared by `Slider::Run` (pointer -> value) and `Slider::Render`.
struct SliderTrackLayout {
    bool vertical{false};
    Rect region{};      ///< Track + thumb travel region (companion texts and value gutters excluded)
    keel::f32 start{0.0f};  ///< Thumb center coordinate along the main axis at value == min
    keel::f32 length{0.0f}; ///< Thumb center travel from min to max along the main axis (>= 0)
    keel::f32 cross{0.0f};  ///< Track centerline coordinate on the cross axis

    /// Thumb center for normalized ratio `t` in [0, 1] (min at the left / bottom).
    [[nodiscard]] Vec2 ThumbCenter(keel::f32 t) const noexcept {
        return vertical ? Vec2{cross, start - t * length} : Vec2{start + t * length, cross};
    }

    /// Signed distance of `p` along the travel from the min end (grows towards max in both orientations).
    [[nodiscard]] keel::f32 Travel(Vec2 p) const noexcept {
        return vertical ? (start - p.y) : (p.x - start);
    }

    /// Normalized ratio [0, 1] under pointer position `p`.
    [[nodiscard]] keel::f32 RatioAt(Vec2 p) const noexcept {
        return keel::clamp(Travel(p) / keel::max(1.0f, length), 0.0f, 1.0f);
    }
};

SliderTrackLayout ComputeSliderTrackLayout(
    const Rect& ctrlRect,
    const SliderControlMetrics& cm,
    const SliderMetrics& m,
    bool vertical
) noexcept {
    const keel::f32 halfThumb = m.thumbSize * 0.5f;
    SliderTrackLayout l{};
    l.vertical = vertical;
    if (vertical) {
        const keel::f32 contentW = cm.crossW + cm.extraLabelsW + cm.thumbValW;
        const keel::f32 offsetX = keel::max(0.0f, (ctrlRect.w - contentW) * 0.5f);
        l.region = Rect(
            ctrlRect.x + offsetX,
            ctrlRect.y + cm.endGutterH,
            cm.crossW,
            keel::max(m.thumbSize, ctrlRect.h - cm.endGutterH - cm.startGutterH)
        );
        l.start = l.region.y + l.region.h - halfThumb;
        l.length = keel::max(0.0f, l.region.h - m.thumbSize);
        l.cross = l.region.x + cm.crossW * 0.5f;
        return l;
    }
    const keel::f32 contentOffsetY = keel::max(0.0f, (ctrlRect.h - cm.intrinsicControlSize.y) * 0.5f);
    l.region = Rect(
        ctrlRect.x + cm.leftGutterW,
        ctrlRect.y + contentOffsetY + cm.topValH,
        keel::max(m.thumbSize, ctrlRect.w - cm.leftGutterW - cm.rightGutterW),
        cm.trackRowH
    );
    l.start = l.region.x + halfThumb;
    l.length = keel::max(0.0f, l.region.w - m.thumbSize);
    l.cross = l.region.y + cm.trackRowH * 0.5f;
    return l;
}

/// Companion texts and value readout of a vertical (fader) Slider: stacked above / below the track,
/// or right of the thumb for `Placement::Thumb`.
void RenderSliderCompanionTextsVertical(
    ICanvas& canvas,
    const Rect& controlRect,
    const SliderTrackLayout& layout,
    Vec2 thumbCenter,
    keel::f32 thumbSize,
    const SliderControlMetrics& cm,
    const UiText& textLeft,
    const UiText& textRight,
    const UiText& valueStyle,
    keel::StringRef effectiveValStr,
    bool showValue,
    Placement valuePlacement,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const Color valCol = theme.ResolveTextColor(valueStyle, UiVariant::Secondary, disabled);
    const bool hasValue = showValue && !effectiveValStr.empty();

    // Above the track (max end): [textRight] [Trailing / Top value]
    keel::f32 y = controlRect.y;
    if (!textRight.empty() || !textRight.variants.empty()) {
        if (!textRight.empty()) {
            const Color col = theme.ResolveTextColor(textRight, UiVariant::Secondary, disabled);
            canvas.DrawTextBox(textRight.text, Rect(controlRect.x, y, controlRect.w, cm.rightTextLineH),
                               cm.rightTextFont, col, 0.5f, 0.5f);
        }
        y += cm.rightTextLineH + kSliderVerticalTextGap;
    }
    if (hasValue && (valuePlacement == Placement::Trailing || valuePlacement == Placement::Top)) {
        canvas.DrawTextBox(effectiveValStr, Rect(controlRect.x, y, controlRect.w, cm.valLineH),
                           cm.valFont, valCol, 0.5f, 0.5f);
    }

    // Below the track (min end): [Leading / Bottom value] [textLeft]
    y = layout.region.y + layout.region.h;
    if (showValue && (valuePlacement == Placement::Leading || valuePlacement == Placement::Bottom)) {
        y += 2.0f;
        if (hasValue) {
            canvas.DrawTextBox(effectiveValStr, Rect(controlRect.x, y, controlRect.w, cm.valLineH),
                               cm.valFont, valCol, 0.5f, 0.5f);
        }
        y += cm.valLineH;
    }
    if (!textLeft.empty()) {
        y += kSliderVerticalTextGap;
        const Color col = theme.ResolveTextColor(textLeft, UiVariant::Secondary, disabled);
        canvas.DrawTextBox(textLeft.text, Rect(controlRect.x, y, controlRect.w, cm.leftTextLineH),
                           cm.leftTextFont, col, 0.5f, 0.5f);
    }

    // Thumb value: right of the track (and labels column), following the thumb
    if (hasValue && valuePlacement == Placement::Thumb) {
        const keel::f32 x = keel::max(thumbCenter.x + thumbSize * 0.5f, layout.region.x + layout.region.w) +
                        cm.extraLabelsW + kSliderInnerGap;
        canvas.DrawTextBox(effectiveValStr, Rect(x, thumbCenter.y - cm.valLineH * 0.5f, cm.valReserveW, cm.valLineH),
                           cm.valFont, valCol, 0.0f, 0.5f);
    }
}

void RenderSliderCompanionTexts(
    ICanvas& canvas,
    const Rect& controlRect,
    keel::f32 trackRegionX,
    keel::f32 trackRegionW,
    keel::f32 trackRowY,
    keel::f32 thumbCenterX,
    const SliderControlMetrics& cm,
    const UiText& textLeft,
    const UiText& textRight,
    const UiText& valueStyle,
    keel::StringRef effectiveValStr,
    bool showValue,
    Placement valuePlacement,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const keel::f32 innerGap = kSliderInnerGap;
    const Color valCol = theme.ResolveTextColor(valueStyle, UiVariant::Secondary, disabled);

    // Left gutter: [textLeft] [Leading value]
    keel::f32 leftX = controlRect.x;
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
    keel::f32 rightX = trackRegionX + trackRegionW;
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
        const keel::f32 topY = trackRowY - cm.topValH;
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
            const keel::f32 valDrawW = keel::max(cm.valW, 4.0f);
            const keel::f32 thumbValX = keel::clamp(
                thumbCenterX - valDrawW * 0.5f,
                trackRegionX,
                keel::max(trackRegionX, trackRegionX + trackRegionW - valDrawW)
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
            const keel::f32 bottomY = trackRowY + cm.trackRowH + cm.extraLabelsH + 2.0f;
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
    const bool isVert = IsVerticalSlider(opts.orientation);
    return Control::FromOptions(
        opts,
        isVert ? SizeMode::Fit : SizeMode::Stretch,
        isVert ? SizeMode::Stretch : SizeMode::Fit,
        ElementLayout::Stacked,
        m.labelGap,
        m.descGap,
        12.0f
    );
}

/// StepSlider labels band across the track: a row below it (height) when horizontal, a column right of
/// it (widest label) when vertical; 0 without labels.
keel::f32 StepSliderLabelsBand(
    const StepSliderOptions& opts,
    keel::usize totalSteps,
    const SliderMetrics& m,
    keel::f32 tickFont,
    ICanvas* canvas
) noexcept {
    if (!opts.showLabels || opts.stepLabels.empty() || totalSteps <= 1) {
        return 0.0f;
    }
    if (!IsVerticalSlider(opts.orientation)) {
        return m.tickGap + tickFont * 1.3f;
    }
    keel::f32 widest = 0.0f;
    const keel::usize n = keel::min(totalSteps, opts.stepLabels.size());
    for (keel::usize i = 0; i < n; ++i) {
        widest = keel::max(widest, TextUtil::MeasureWidth(opts.stepLabels[i], tickFont, canvas));
    }
    return m.tickGap + widest;
}

Color ResolveSliderTrackColor(const StateColor& rawColor, bool disabled, const UiTheme& theme) noexcept {
    const StateColor color = theme.ResolveStateColor(rawColor);
    const auto& pal = theme.palette;
    const Color primaryHue = theme.GetSemanticAccent(UiVariant::Primary);
    if (!color.HasAny()) {
        return disabled ? primaryHue.WithAlphaF(theme.disabledOpacity) : primaryHue;
    }
    if (rawColor.token == StateColorToken::Primary) {
        return disabled ? primaryHue.WithAlphaF(theme.disabledOpacity) : primaryHue;
    }
    if (rawColor.token == StateColorToken::Success) {
        const Color c = theme.GetSemanticAccent(UiVariant::Success);
        return disabled ? c.WithAlphaF(theme.disabledOpacity) : c;
    }
    if (rawColor.token == StateColorToken::Warning) {
        const Color c = theme.GetSemanticAccent(UiVariant::Warning);
        return disabled ? c.WithAlphaF(theme.disabledOpacity) : c;
    }
    if (rawColor.token == StateColorToken::Danger) {
        const Color c = theme.GetSemanticAccent(UiVariant::Danger);
        return disabled ? c.WithAlphaF(theme.disabledOpacity) : c;
    }
    if (rawColor.token == StateColorToken::Info) {
        const Color c = theme.GetSemanticAccent(UiVariant::Info);
        return disabled ? c.WithAlphaF(theme.disabledOpacity) : c;
    }
    if (color.normal.has_value() && color.normal->a == 0) {
        return disabled ? pal.borderHover.WithAlphaF(theme.disabledOpacity) : pal.borderHover;
    }
    const Color base = color.normal.value_or(primaryHue);
    return disabled ? color.disabled.value_or(base.WithAlphaF(theme.disabledOpacity)) : base;
}

Color ResolveSliderActiveThumbColor(const StateColor& rawColor, const UiTheme& theme) noexcept {
    const StateColor color = theme.ResolveStateColor(rawColor);
    const Color primaryActive = theme.GetSemanticAccent(UiVariant::Primary).Darken(0.08f);
    if (!color.HasAny() || rawColor.token == StateColorToken::Primary) {
        return primaryActive;
    }
    if (color.normal.has_value() && color.normal->a == 0) {
        return theme.GetVariant(UiVariant::Ghost).bgActive;
    }
    return color.active.value_or(color.normal.has_value() ? StateColor::DarkenColor(*color.normal) : primaryActive);
}

} // namespace

// ============================================================================
// Slider (Continuous / Stepped Numeric Slider)
// ============================================================================

Response Slider::Run(
    Context& ctx,
    keel::f64* valDouble,
    keel::f32* valFloat,
    keel::i64* valInt,
    keel::f64 min,
    keel::f64 max,
    const SliderOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme& activeTheme = ctx.GetTheme();

    const SliderOptions& optsCopy = opts;

    const SliderMetrics& m = activeTheme.GetSlider(activeSize);
    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);

    keel::f64 currentVal = (valDouble != nullptr) ? *valDouble :
                        (valFloat != nullptr)  ? keel::as<keel::f64>(*valFloat) :
                        (valInt != nullptr)    ? keel::as<keel::f64>(*valInt) : rangeMin;

    currentVal = keel::clamp(currentVal, rangeMin, rangeMax);

    const keel::f64 effectiveStep = (optsCopy.step.has_value() && *optsCopy.step > 0.0) ? *optsCopy.step :
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
        const SliderControlMetrics cm = ComputeSliderControlMetrics(
            optsCopy.textLeft, optsCopy.textRight, optsCopy.valueFormat, initValBuf,
            optsCopy.showValue, optsCopy.valuePlacement, ctrl.width,
            0.0f, activeSize, nullptr, activeTheme,
            {}, true, rangeMin, rangeMax, valInt != nullptr, optsCopy.orientation
        );
        const Rect ctrlRect = prevBounds;
        const SliderTrackLayout layout =
            ComputeSliderTrackLayout(ctrlRect, cm, m, IsVerticalSlider(optsCopy.orientation));

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
                const keel::f32 t = layout.RatioAt(ctx.GetMousePos());
                keel::f64 rawVal = rangeMin + t * (rangeMax - rangeMin);

                if (effectiveStep > 0.0) {
                    rawVal = rangeMin + keel::round((rawVal - rangeMin) / effectiveStep) * effectiveStep;
                }
                rawVal = keel::clamp(rawVal, rangeMin, rangeMax);

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
            const keel::f32 wheelDelta = (wheel.x != 0.0f) ? wheel.x : wheel.y;
            if (wheelDelta != 0.0f) {
                const keel::f64 stepDelta = (effectiveStep > 0.0) ? effectiveStep : (rangeMax - rangeMin) * 0.05;
                keel::f64 rawVal = currentVal + wheelDelta * stepDelta;
                rawVal = keel::clamp(rawVal, rangeMin, rangeMax);
                if (rawVal != currentVal) {
                    currentVal = rawVal;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            }
        }

        // Keyboard arrow adjustment when focused
        if (ctx.IsFocused(id) && !state.dragging) {
            const keel::f64 stepDelta = (effectiveStep > 0.0) ? effectiveStep : (rangeMax - rangeMin) * 0.05;
            if (ctx.ConsumeKey(Key::Right) || ctx.ConsumeKey(Key::Up)) {
                const keel::f64 rawVal = keel::clamp(currentVal + stepDelta, rangeMin, rangeMax);
                if (rawVal != currentVal) {
                    currentVal = rawVal;
                    res.changed = true;
                    ctx.RequestRepaint(3);
                }
            } else if (ctx.ConsumeKey(Key::Left) || ctx.ConsumeKey(Key::Down)) {
                const keel::f64 rawVal = keel::clamp(currentVal - stepDelta, rangeMin, rangeMax);
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
    if (valFloat != nullptr)  *valFloat = keel::as<keel::f32>(currentVal);
    if (valInt != nullptr)    *valInt = keel::round_i64(currentVal);

    // Format current value string
    char valBuf[64];
    FormatSliderValue(valBuf, sizeof(valBuf), currentVal, valInt != nullptr, optsCopy.valueFormat.text);
    const keel::StringRef formattedStr = ctx.AllocString(valBuf);

    Node* node = ctx.CreateNode(NodeType::Slider, id);
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed || state.dragging;
    node->focused = ctx.IsFocusVisible(id);

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

    const keel::u64 valBits = keel::bit_cast<keel::u64>(currentVal);
    node->visualHash = detail::HashArgs(
        valBits,
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
        data->opts.valueFormat,
        data->formattedValue
    );

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
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

    Control::SetupNode(ctx, node, ctrl);
    return res;
}

Vec2 Slider::Measure(
    const SliderOptions& opts,
    keel::f64 value,
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
                           keel::abs(*opts.step - keel::round(*opts.step)) < 0.0001 &&
                           keel::abs(rangeMin - keel::round(rangeMin)) < 0.0001;
    char valBuf[64];
    FormatSliderValue(valBuf, sizeof(valBuf), value, isIntStep, opts.valueFormat.text);
    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
        opts.showValue, opts.valuePlacement, ctrl.width,
        0.0f, size, canvas, theme,
        {}, true, rangeMin, rangeMax, isIntStep, opts.orientation
    );
    return Control::Measure(cm.intrinsicControlSize, ctrl, theme);
}

Vec2 Slider::Measure(
    const SliderOptions& opts,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
    UiSize size,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, value, min, max, size, canvas, defaultTheme, availableWidth);
}

void Slider::Render(
    ICanvas& canvas,
    Rect bounds,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
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
    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);
    const bool isIntStep = opts.step.has_value() && *opts.step >= 1.0 &&
                           keel::abs(*opts.step - keel::round(*opts.step)) < 0.0001 &&
                           keel::abs(rangeMin - keel::round(rangeMin)) < 0.0001;

    char valBuf[64];
    FormatSliderValue(valBuf, sizeof(valBuf), value, isIntStep, opts.valueFormat.text);

    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
        opts.showValue, opts.valuePlacement, ctrl.width,
        0.0f, size, &canvas, theme,
        {}, true, rangeMin, rangeMax, isIntStep, opts.orientation
    );

    const Rect ctrlRect = bounds;
    const bool vertical = IsVerticalSlider(opts.orientation);
    const SliderTrackLayout layout = ComputeSliderTrackLayout(ctrlRect, cm, m, vertical);

    const keel::f64 range = max - min;
    const keel::f32 ratio = (range > 0.0) ? keel::as<keel::f32>(keel::clamp((value - min) / range, 0.0, 1.0)) : 0.0f;
    const Vec2 thumbCenter = layout.ThumbCenter(ratio);
    const keel::f32 fillLen = ratio * layout.length;

    // 0. Draw companion textLeft / textRight and value readout
    if (vertical) {
        RenderSliderCompanionTextsVertical(
            canvas, ctrlRect, layout, thumbCenter, m.thumbSize,
            cm, opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
            opts.showValue, opts.valuePlacement, disabled, theme
        );
    } else {
        RenderSliderCompanionTexts(
            canvas, ctrlRect, layout.region.x, layout.region.w, layout.region.y, thumbCenter.x,
            cm, opts.textLeft, opts.textRight, opts.valueFormat, valBuf,
            opts.showValue, opts.valuePlacement, disabled, theme
        );
    }

    // 1 & 2. Draw track groove and optional active fill via active style painter
    const keel::f32 effTrackH = (theme.controlStyle.frameChrome == FrameChrome::Bevel3D) ? 4.0f : m.trackHeight;
    const keel::f32 halfTrack = effTrackH * 0.5f;
    const Rect trackBgRect = vertical
        ? Rect(keel::floor(layout.cross - halfTrack), layout.start - layout.length, effTrackH, layout.length)
        : Rect(layout.start, keel::floor(layout.cross - halfTrack), layout.length, effTrackH);
    const Rect fillRect = vertical
        ? Rect(trackBgRect.x, thumbCenter.y, effTrackH, fillLen)
        : Rect(layout.start, trackBgRect.y, fillLen, effTrackH);
    const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
    const Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
    const Color fillCol = ResolveSliderTrackColor(opts.color, disabled, theme);
    theme.Painter().DrawSliderTrack(
        canvas, theme, trackBgRect, fillRect, vertical, fillLen > 0.0f,
        trackBg, trackBorder, fillCol, m.cornerRadius, opts.color.HasAny()
    );

    // 3. Draw thumb knob via active style painter
    const Color thumbBg = disabled ? pal.surface.WithAlphaF(0.5f) :
                          active   ? ResolveSliderActiveThumbColor(opts.color, theme) : pal.surface;
    const Color thumbBorder = disabled ? pal.border.WithAlphaF(0.4f) :
                              focused  ? pal.focusRing :
                              (active || hovered) ? pal.accent : pal.borderHover;
    const keel::f32 thumbBorderW = (active || hovered || focused) ? 2.5f : 2.0f;
    theme.Painter().DrawSliderThumb(
        canvas, theme, thumbCenter, m.thumbSize, vertical,
        thumbBg, thumbBorder, thumbBorderW, hovered, active, focused, disabled, m.cornerRadius
    );
}

// ============================================================================
// StepSlider (Discrete N-Position Step Slider)
// ============================================================================

Response StepSlider::Run(
    Context& ctx,
    keel::usize* indexSizeT,
    keel::i32* indexInt,
    keel::usize totalSteps,
    const StepSliderOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme& activeTheme = ctx.GetTheme();

    const StepSliderOptions& optsCopy = opts;

    const SliderMetrics& m = activeTheme.GetSlider(activeSize);

    if (totalSteps == 0) {
        totalSteps = optsCopy.stepLabels.empty() ? 1 : optsCopy.stepLabels.size();
    }

    keel::usize curIdx = (indexSizeT != nullptr) ? *indexSizeT :
                    (indexInt != nullptr)   ? keel::as<keel::usize>(keel::max(0, *indexInt)) : 0;
    curIdx = keel::min(curIdx, totalSteps - 1);

    const keel::StringRef initStepStr = !optsCopy.valueText.empty()
        ? optsCopy.valueText.text
        : ((curIdx < optsCopy.stepLabels.size()) ? optsCopy.stepLabels[curIdx] : keel::StringRef{});
    const keel::f32 tickFont = activeTheme.ResolveFontSize(TextRole::LabelSmall, activeSize);
    const keel::f32 labelsBand = StepSliderLabelsBand(optsCopy, totalSteps, m, tickFont, nullptr);

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
        const SliderControlMetrics cm = ComputeSliderControlMetrics(
            optsCopy.textLeft, optsCopy.textRight, optsCopy.valueText, initStepStr,
            optsCopy.showValue, optsCopy.valuePlacement, ctrl.width,
            labelsBand, activeSize, nullptr, activeTheme,
            optsCopy.stepLabels, false, 0.0, 1.0, false, optsCopy.orientation
        );
        const Rect ctrlRect = prevBounds;
        const SliderTrackLayout layout =
            ComputeSliderTrackLayout(ctrlRect, cm, m, IsVerticalSlider(optsCopy.orientation));

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
                const keel::f32 t = layout.RatioAt(ctx.GetMousePos());
                const keel::usize newIdx = (totalSteps > 1)
                    ? keel::clamp(
                          keel::as<keel::usize>(keel::round_u64(t * keel::as<keel::f32>(totalSteps - 1))),
                          keel::usize{0},
                          totalSteps - 1
                      )
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
            const keel::f32 wheelDelta = (wheel.x != 0.0f) ? wheel.x : wheel.y;
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
    if (indexInt != nullptr)   *indexInt = keel::as<keel::i32>(curIdx);

    Node* node = ctx.CreateNode(NodeType::StepSlider, id);
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed || state.dragging;
    node->focused = ctx.IsFocusVisible(id);

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
        data->opts.showLabels,
        data->opts.showValue,
        data->opts.textLeft,
        data->opts.textRight,
        data->opts.valueText
    );

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
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

    Control::SetupNode(ctx, node, ctrl);
    return res;
}

Vec2 StepSlider::Measure(
    const StepSliderOptions& opts,
    keel::usize index,
    keel::usize totalSteps,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 /*availableWidth*/
) {
    if (totalSteps == 0) {
        totalSteps = opts.stepLabels.empty() ? 1 : opts.stepLabels.size();
    }
    const SliderMetrics& m = theme.GetSlider(size);
    const keel::f32 tickFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
    const keel::f32 labelsBand = StepSliderLabelsBand(opts, totalSteps, m, tickFont, canvas);
    const keel::StringRef stepStr = !opts.valueText.empty()
        ? opts.valueText.text
        : ((index < opts.stepLabels.size()) ? opts.stepLabels[index] : keel::StringRef{});
    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueText, stepStr,
        opts.showValue, opts.valuePlacement, ctrl.width,
        labelsBand, size, canvas, theme,
        opts.stepLabels, false, 0.0, 1.0, false, opts.orientation
    );
    return Control::Measure(cm.intrinsicControlSize, ctrl, theme);
}

Vec2 StepSlider::Measure(
    const StepSliderOptions& opts,
    keel::usize index,
    keel::usize totalSteps,
    UiSize size,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, index, totalSteps, size, canvas, defaultTheme, availableWidth);
}

void StepSlider::Render(
    ICanvas& canvas,
    Rect bounds,
    keel::usize index,
    keel::usize totalSteps,
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
    const keel::f32 tickFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
    const auto& pal = theme.palette;

    const keel::StringRef stepStr = !opts.valueText.empty()
        ? opts.valueText.text
        : ((index < opts.stepLabels.size()) ? opts.stepLabels[index] : keel::StringRef{});
    const keel::f32 labelsBand = StepSliderLabelsBand(opts, totalSteps, m, tickFont, &canvas);

    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    const SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, opts.valueText, stepStr,
        opts.showValue, opts.valuePlacement, ctrl.width,
        labelsBand, size, &canvas, theme,
        opts.stepLabels, false, 0.0, 1.0, false, opts.orientation
    );

    const Rect ctrlRect = bounds;
    const bool vertical = IsVerticalSlider(opts.orientation);
    const SliderTrackLayout layout = ComputeSliderTrackLayout(ctrlRect, cm, m, vertical);
    const keel::f32 halfThumb = m.thumbSize * 0.5f;

    const keel::f32 ratio = (totalSteps > 1) ? keel::as<keel::f32>(index) / keel::as<keel::f32>(totalSteps - 1) : 0.0f;
    const Vec2 thumbCenter = layout.ThumbCenter(ratio);
    const keel::f32 fillLen = ratio * layout.length;
    auto stepRatio = [&](keel::usize i) { return keel::as<keel::f32>(i) / keel::as<keel::f32>(totalSteps - 1); };

    // 0. Draw companion textLeft / textRight and value readout
    if (vertical) {
        RenderSliderCompanionTextsVertical(
            canvas, ctrlRect, layout, thumbCenter, m.thumbSize,
            cm, opts.textLeft, opts.textRight, opts.valueText, stepStr,
            opts.showValue, opts.valuePlacement, disabled, theme
        );
    } else {
        RenderSliderCompanionTexts(
            canvas, ctrlRect, layout.region.x, layout.region.w, layout.region.y, thumbCenter.x,
            cm, opts.textLeft, opts.textRight, opts.valueText, stepStr,
            opts.showValue, opts.valuePlacement, disabled, theme
        );
    }

    // 1 & 2. Draw track groove and optional active fill via active style painter
    const bool isPointer3D = (theme.controlStyle.sliderThumb == SliderThumbShape::Pointer3D);
    const keel::f32 effTrackH = (theme.controlStyle.frameChrome == FrameChrome::Bevel3D) ? 4.0f : m.trackHeight;
    const keel::f32 effHalfTrack = effTrackH * 0.5f;
    const Rect trackBgRect = vertical
        ? Rect(keel::floor(layout.cross - effHalfTrack), layout.start - layout.length, effTrackH, layout.length)
        : Rect(layout.start, keel::floor(layout.cross - effHalfTrack), layout.length, effTrackH);
    const Rect fillRect = vertical
        ? Rect(trackBgRect.x, thumbCenter.y, effTrackH, fillLen)
        : Rect(layout.start, trackBgRect.y, fillLen, effTrackH);
    const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
    const Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
    const Color fillCol = ResolveSliderTrackColor(opts.color, disabled, theme);
    theme.Painter().DrawSliderTrack(
        canvas, theme, trackBgRect, fillRect, vertical, fillLen > 0.0f,
        trackBg, trackBorder, fillCol, m.cornerRadius, opts.color.HasAny()
    );

    // 3. Draw tick marks on track
    if (opts.showTicks && totalSteps > 1) {
        if (isPointer3D) {
            const Color tickCol = disabled ? pal.textDisabled : pal.border;
            for (keel::usize i = 0; i < totalSteps; ++i) {
                const Vec2 tick = layout.ThumbCenter(stepRatio(i));
                if (vertical) {
                    canvas.DrawRect(Rect(keel::floor(layout.cross + 10.0f), keel::floor(tick.y), 3.0f, 1.0f), tickCol);
                } else {
                    canvas.DrawRect(Rect(keel::floor(tick.x), keel::floor(layout.cross + 10.0f), 1.0f, 3.0f), tickCol);
                }
            }
        } else {
            const keel::f32 tickR = m.tickSize * 0.5f;
            const bool sharpTick = (m.cornerRadius <= 0.01f);
            for (keel::usize i = 0; i < totalSteps; ++i) {
                const Vec2 tick = layout.ThumbCenter(stepRatio(i));
                const Rect tickRect(tick.x - tickR, tick.y - tickR, m.tickSize, m.tickSize);
                Color tickCol = (i <= index) ? pal.surface : pal.textDisabled;
                if (disabled) tickCol = tickCol.WithAlphaF(0.4f);
                if (sharpTick) {
                    canvas.DrawRect(tickRect, tickCol);
                } else {
                    canvas.DrawRoundRect(tickRect, tickR, tickCol);
                }
            }
        }
    }

    // 4. Draw thumb knob via active style painter
    const Color thumbBg = disabled ? pal.surface.WithAlphaF(0.5f) :
                          active   ? ResolveSliderActiveThumbColor(opts.color, theme) : pal.surface;
    const Color thumbBorder = disabled ? pal.border.WithAlphaF(0.4f) :
                              focused  ? pal.focusRing :
                              (active || hovered) ? pal.accent : pal.borderHover;
    const keel::f32 thumbBorderW = (active || hovered || focused) ? 2.5f : 2.0f;
    theme.Painter().DrawSliderThumb(
        canvas, theme, thumbCenter, m.thumbSize, vertical,
        thumbBg, thumbBorder, thumbBorderW, hovered, active, focused, disabled, m.cornerRadius
    );

    // 5. Draw step labels (with collision culling so labels never overlap): a row below the track
    //    when horizontal, a column right of the ticks when vertical
    if (opts.showLabels && !opts.stepLabels.empty() && totalSteps > 1) {
        const keel::usize nLabels = keel::min(totalSteps, opts.stepLabels.size());
        const keel::f32 minGap = 4.0f;
        auto labelColor = [&](keel::usize i) {
            return (i == index) ? pal.accent : (disabled ? pal.textDisabled : pal.textSecondary);
        };

        if (vertical) {
            // Bottom-up from the first step; first and last labels always drawn
            const keel::f32 labelsX = layout.region.x + layout.region.w + m.tickGap;
            const keel::f32 regionTop = layout.region.y;
            const keel::f32 regionBottom = layout.region.y + layout.region.h;
            auto labelTop = [&](keel::usize i, keel::f32 h) {
                return keel::clamp(layout.ThumbCenter(stepRatio(i)).y - h * 0.5f, regionTop,
                                  keel::max(regionTop, regionBottom - h));
            };
            const keel::f32 lastH = canvas.MeasureText(opts.stepLabels[nLabels - 1], tickFont).y;
            const keel::f32 lastBottom = labelTop(nLabels - 1, lastH) + lastH;
            keel::f32 prevTop = 1e9f;
            for (keel::usize i = 0; i < nLabels; ++i) {
                const keel::StringRef lbl = opts.stepLabels[i];
                const keel::f32 h = canvas.MeasureText(lbl, tickFont).y;
                const keel::f32 top = labelTop(i, h);
                const bool isFirst = (i == 0);
                const bool isLast = (i + 1 == nLabels);
                if (!isFirst && !isLast) {
                    if (top + h + minGap > prevTop || top < lastBottom + minGap) {
                        continue;
                    }
                } else if (isLast && i > 0 && top + h + minGap > prevTop) {
                    continue;
                }
                canvas.DrawText(lbl, Vec2(labelsX, top), tickFont, labelColor(i));
                prevTop = top;
            }
        } else {
            const keel::f32 trackRegionX = layout.region.x;
            const keel::f32 trackRegionW = layout.region.w;
            const keel::f32 labelsY = layout.cross + halfThumb + m.tickGap;

            keel::f32 lastLabelLeft = trackRegionX + trackRegionW;
            if (nLabels > 1) {
                const Vec2 lastDims = canvas.MeasureText(opts.stepLabels[nLabels - 1], tickFont);
                const keel::f32 lastTickX = layout.start + layout.length;
                lastLabelLeft = keel::clamp(
                    lastTickX - lastDims.x * 0.5f,
                    trackRegionX,
                    keel::max(trackRegionX, trackRegionX + trackRegionW - lastDims.x)
                );
            }

            keel::f32 prevRight = -1e9f;
            for (keel::usize i = 0; i < nLabels; ++i) {
                const keel::f32 tickX = layout.ThumbCenter(stepRatio(i)).x;
                const keel::StringRef lbl = opts.stepLabels[i];
                const Vec2 dims = canvas.MeasureText(lbl, tickFont);
                keel::f32 lblX = tickX - dims.x * 0.5f;
                lblX = keel::clamp(lblX, trackRegionX, keel::max(trackRegionX, trackRegionX + trackRegionW - dims.x));

                const bool isFirst = (i == 0);
                const bool isLast = (i + 1 == nLabels);
                if (!isFirst && !isLast) {
                    if (lblX < prevRight + minGap || lblX + dims.x + minGap > lastLabelLeft) {
                        continue;
                    }
                } else if (isLast && i > 0 && lblX < prevRight + minGap) {
                    continue;
                }

                canvas.DrawText(lbl, Vec2(lblX, labelsY), tickFont, labelColor(i));
                prevRight = lblX + dims.x;
            }
        }
    }
}

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
};

struct ProgressBarData {
    ProgressBarOptions opts;
    keel::f64 value{0.0};
    keel::f64 min{0.0};
    keel::f64 max{100.0};
    keel::f32 animPhase{0.0f};
};

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

Response RangeSlider::Run(
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
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        state.dragMode = RangeDragMode::None;
    } else {
        const keel::f32 halfThumb = m.thumbSize * 0.5f;
        const SliderControlMetrics cm = ComputeSliderControlMetrics(
            optsCopy.textLeft, optsCopy.textRight, optsCopy.valueFormat, initValBuf,
            optsCopy.showValue, optsCopy.valuePlacement, ctrl.width,
            0.0f, activeSize, nullptr, activeTheme,
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

        if (res.clicked || (res.pressed && state.dragMode == RangeDragMode::None)) {
            const keel::f32 pos = layout.Travel(ctx.GetMousePos());
            const keel::f32 distLow = keel::abs(pos - lowPos);
            const keel::f32 distHigh = keel::abs(pos - highPos);
            const bool insideIntervalBar = (pos > lowPos + halfThumb * 1.15f) &&
                                           (pos < highPos - halfThumb * 1.15f) &&
                                           ((highPos - lowPos) > m.thumbSize * 1.8f);

            if (insideIntervalBar || ctx.GetKeyModifiers().shift) {
                state.dragMode = RangeDragMode::IntervalSpan;
            } else if (distLow < distHigh || (distLow == distHigh && pos < lowPos)) {
                state.dragMode = RangeDragMode::LowThumb;
                state.activeThumb = 0;
            } else {
                state.dragMode = RangeDragMode::HighThumb;
                state.activeThumb = 1;
            }
            state.dragStartLow = curLow;
            state.dragStartHigh = curHigh;
            state.dragStartTravel = pos;
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

    const keel::u64 lowBits = keel::bit_cast<keel::u64>(curLow);
    const keel::u64 highBits = keel::bit_cast<keel::u64>(curHigh);
    node->visualHash = detail::HashArgs(
        lowBits,
        highBits,
        state.activeThumb,
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

    Control::SetupNode(ctx, node, ctrl);
    return res;
}

Vec2 RangeSlider::Measure(
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

Vec2 RangeSlider::Measure(
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

void RangeSlider::Render(
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
    keel::u8 activeThumb
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const SliderMetrics& m = theme.GetSlider(size);
    const auto& pal = theme.palette;
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
    const bool isBevel = (theme.controlStyle.frameChrome == FrameChrome::Bevel3D);
    const keel::f32 effTrackH = isBevel ? 6.0f : m.trackHeight;
    const keel::f32 halfTrack = effTrackH * 0.5f;
    const Rect trackBgRect = vertical
        ? Rect(keel::floor(layout.cross - halfTrack), layout.start - layout.length, effTrackH, layout.length)
        : Rect(layout.start, keel::floor(layout.cross - halfTrack), layout.length, effTrackH);
    const keel::f32 fillLen = keel::max(0.0f, keel::abs(highRatio - lowRatio) * layout.length);
    const Rect fillRect = vertical
        ? Rect(trackBgRect.x, highThumb.y, effTrackH, fillLen)
        : Rect(lowThumb.x, trackBgRect.y, fillLen, effTrackH);
    const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
    const Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
    const Color fillCol = ResolveSliderTrackColor(opts.color, disabled, theme);
    theme.Painter().DrawSliderTrack(
        canvas, theme, trackBgRect, fillRect, vertical, fillLen > 0.0f,
        trackBg, trackBorder, fillCol, m.cornerRadius, true
    );

    // 3. Draw low and high thumb knobs via active style painter
    auto drawThumb = [&](Vec2 c, bool isFocusedThumb) {
        const bool thumbActive = active && isFocusedThumb;
        const bool thumbFocused = focused && isFocusedThumb;
        const Color thumbBg = disabled ? pal.surface.WithAlphaF(0.5f) :
                              thumbActive ? ResolveSliderActiveThumbColor(opts.color, theme) : pal.surface;
        const Color thumbBorder = disabled ? pal.border.WithAlphaF(0.4f) :
                                  thumbFocused ? pal.focusRing :
                                  (active || hovered) ? pal.accent : pal.borderHover;
        const keel::f32 thumbBorderW = (active || hovered || thumbFocused) ? 2.5f : 2.0f;
        theme.Painter().DrawSliderThumb(
            canvas, theme, c, m.thumbSize, vertical,
            thumbBg, thumbBorder, thumbBorderW, hovered, thumbActive, thumbFocused, disabled, m.cornerRadius
        );
    };

    drawThumb(lowThumb, activeThumb == 0);
    drawThumb(highThumb, activeThumb == 1);
}

// ============================================================================
// ProgressBar (Continuous Progress Bar & Segmented LED / VU LevelMeter)
// ============================================================================

Response ProgressBar::Run(
    Context& ctx,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
    const ProgressBarOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme& activeTheme = ctx.GetTheme();

    const ProgressBarOptions& optsCopy = opts;

    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);
    const keel::f64 clampedVal = keel::clamp(value, rangeMin, rangeMax);

    Response res = ctx.Interact(id);
    res.clicked = false;
    res.pressed = false;
    if (effectiveDisabled) {
        res.hovered = false;
    } else if (res.hovered && !optsCopy.tooltip.empty()) {
        ctx.SetTooltip(optsCopy.tooltip, ctx.GetPrevFrameBounds(id));
    }

    keel::f32 animPhase = 0.0f;
    if (optsCopy.indeterminate && !effectiveDisabled) {
        const keel::f32 animScale = keel::max(0.0f, activeTheme.animationScale);
        if (animScale > 0.001f) {
            animPhase = keel::as<keel::f32>(keel::fmod(keel::max(0.0, ctx.GetCurrentTime()) * 0.85 * keel::as<keel::f64>(animScale), 1.0));
            ctx.RequestRepaint(2);
        } else {
            animPhase = 0.35f;
        }
    }

    const ControlSpec ctrl = EffectiveSliderControl(optsCopy, activeSize, activeTheme);
    Node* node = ctx.CreateNode(NodeType::ProgressBar, id);
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;

    auto* data = ctx.AllocPayload<ProgressBarData>();
    data->opts = optsCopy;
    data->opts.textLeft = ctx.AllocText(optsCopy.textLeft);
    data->opts.textRight = ctx.AllocText(optsCopy.textRight);
    data->opts.tooltip = ctx.AllocText(optsCopy.tooltip);
    data->opts.valueFormat = ctx.AllocText(optsCopy.valueFormat);
    data->value = clampedVal;
    data->min = rangeMin;
    data->max = rangeMax;
    data->animPhase = animPhase;
    node->SetPayload(data);

    const keel::u64 valBits = keel::bit_cast<keel::u64>(clampedVal);
    const keel::f64 peakVal = data->opts.peakHoldValue.value_or(-1e9);
    const keel::u64 peakBits = keel::bit_cast<keel::u64>(peakVal);

    node->visualHash = detail::HashArgs(
        valBits,
        peakBits,
        keel::as<keel::u8>(data->opts.orientation),
        data->opts.indeterminate,
        animPhase,
        data->opts.segments,
        data->opts.multiZoneColor,
        data->opts.warningRatio,
        data->opts.dangerRatio,
        data->opts.trackHeight.value_or(0.0f),
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
            d->opts, n->uiSize, n->hovered, n->disabled, theme, d->animPhase
        );
    };

    Control::SetupNode(ctx, node, ctrl);
    return res;
}

Vec2 ProgressBar::Measure(
    const ProgressBarOptions& opts,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 /*availableWidth*/
) {
    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);
    const bool hasStaticLabel = !opts.valueFormat.text.empty() && opts.valueFormat.text.find('%') == keel::StringRef::npos;
    const bool effShowVal = opts.showValue && (!opts.indeterminate || hasStaticLabel);
    const keel::StringRef fmt = opts.valueFormat.text.empty() ? "%.0f %%" : opts.valueFormat.text;
    char valBuf[64]{};
    if (effShowVal) {
        FormatSliderValue(valBuf, sizeof(valBuf), value, false, fmt);
    }

    UiText fmtText = opts.valueFormat;
    if (fmtText.text.empty()) {
        fmtText.text = fmt;
    }

    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, fmtText, valBuf,
        effShowVal, opts.valuePlacement, ctrl.width,
        0.0f, size, canvas, theme,
        {}, !opts.indeterminate, rangeMin, rangeMax, false, opts.orientation
    );
    if (opts.trackHeight.has_value()) {
        const keel::f32 minThick = *opts.trackHeight + 4.0f;
        if (IsVerticalSlider(opts.orientation)) {
            if (minThick > cm.crossW) {
                cm.intrinsicControlSize.x += (minThick - cm.crossW);
                cm.crossW = minThick;
            }
        } else {
            if (minThick > cm.trackRowH) {
                cm.intrinsicControlSize.y += (minThick - cm.trackRowH);
                cm.trackRowH = minThick;
            }
        }
    }
    return Control::Measure(cm.intrinsicControlSize, ctrl, theme);
}

Vec2 ProgressBar::Measure(
    const ProgressBarOptions& opts,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
    UiSize size,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, value, min, max, size, canvas, defaultTheme, availableWidth);
}

void ProgressBar::Render(
    ICanvas& canvas,
    Rect bounds,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
    const ProgressBarOptions& opts,
    UiSize size,
    bool hovered,
    bool disabled,
    const UiTheme& theme,
    keel::f32 animPhase
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const SliderMetrics& m = theme.GetSlider(size);
    const auto& pal = theme.palette;
    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);
    const bool vertical = IsVerticalSlider(opts.orientation);
    const bool hasStaticLabel = !opts.valueFormat.text.empty() && opts.valueFormat.text.find('%') == keel::StringRef::npos;
    const bool effShowVal = opts.showValue && (!opts.indeterminate || hasStaticLabel);
    const keel::StringRef fmt = opts.valueFormat.text.empty() ? "%.0f %%" : opts.valueFormat.text;

    char valBuf[64]{};
    if (effShowVal) {
        FormatSliderValue(valBuf, sizeof(valBuf), value, false, fmt);
    }

    UiText fmtText = opts.valueFormat;
    if (fmtText.text.empty()) {
        fmtText.text = fmt;
    }

    (void)hovered;
    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, fmtText, valBuf,
        effShowVal, opts.valuePlacement, ctrl.width,
        0.0f, size, &canvas, theme,
        {}, !opts.indeterminate, rangeMin, rangeMax, false, opts.orientation
    );
    const keel::f32 barThick = opts.trackHeight.has_value()
        ? *opts.trackHeight
        : keel::max(m.trackHeight * 1.55f, 8.0f);
    if (!vertical && barThick + 4.0f > cm.trackRowH) {
        cm.intrinsicControlSize.y += ((barThick + 4.0f) - cm.trackRowH);
        cm.trackRowH = barThick + 4.0f;
    } else if (vertical && barThick + 4.0f > cm.crossW) {
        cm.intrinsicControlSize.x += ((barThick + 4.0f) - cm.crossW);
        cm.crossW = barThick + 4.0f;
    }

    const Rect ctrlRect = bounds;

    const keel::f64 range = rangeMax - rangeMin;
    const keel::f32 ratio = opts.indeterminate
        ? 0.0f
        : ((range > 0.0) ? keel::as<keel::f32>(keel::clamp((value - rangeMin) / range, 0.0, 1.0)) : 0.0f);

    auto resolveZoneColor = [&](keel::f32 normPos) -> Color {
        if (!opts.multiZoneColor) {
            return ResolveSliderTrackColor(opts.color, disabled, theme);
        }
        Color col;
        if (normPos >= opts.dangerRatio) {
            col = theme.GetSemanticAccent(UiVariant::Danger);
        } else if (normPos >= opts.warningRatio) {
            col = theme.GetSemanticAccent(UiVariant::Warning);
        } else if (!opts.color.HasAny() || opts.color.token == StateColorToken::Primary) {
            col = theme.GetSemanticAccent(UiVariant::Success);
        } else if (opts.color.token == StateColorToken::Ghost || (opts.color.normal.has_value() && opts.color.normal->a == 0)) {
            col = pal.borderHover;
        } else {
            const StateColor resolved = theme.ResolveStateColor(opts.color);
            col = resolved.normal.value_or(theme.GetSemanticAccent(UiVariant::Success));
        }
        return disabled ? col.WithAlphaF(theme.disabledOpacity) : col;
    };

    if (vertical) {
        SliderTrackLayout layout = ComputeSliderTrackLayout(ctrlRect, cm, m, true);
        const keel::f32 trackTop = layout.region.y;
        const keel::f32 trackH = keel::max(12.0f, layout.region.h);
        const keel::f32 barWInt = keel::max(2.0f, keel::round(barThick));
        const keel::f32 barX = keel::round(layout.cross - barWInt * 0.5f);
        const keel::f32 fillH = ratio * trackH;
        const Vec2 tipCenter(layout.cross, trackTop + trackH - fillH);

        RenderSliderCompanionTextsVertical(
            canvas, ctrlRect, layout, tipCenter, barWInt,
            cm, opts.textLeft, opts.textRight, fmtText, valBuf,
            effShowVal, opts.valuePlacement, disabled, theme
        );

        if (opts.indeterminate) {
            const Rect trackBgRect(barX, trackTop, barWInt, trackH);
            const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
            const Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
            canvas.DrawRoundRect(trackBgRect, barWInt * 0.5f, trackBg, trackBorder, 1.0f);

            const keel::f32 shuttleH = keel::max(barWInt, trackH * 0.32f);
            const keel::f32 pingPong = 1.0f - keel::abs(keel::fmod(animPhase * 2.0f, 2.0f) - 1.0f);
            const keel::f32 smoothT = pingPong * pingPong * (3.0f - 2.0f * pingPong);
            const keel::f32 shuttleY = trackTop + (1.0f - smoothT) * keel::max(0.0f, trackH - shuttleH);
            canvas.DrawRoundRect(Rect(barX, shuttleY, barWInt, shuttleH), barWInt * 0.5f, ResolveSliderTrackColor(opts.color, disabled, theme));
            return;
        }

        if (opts.segments > 1) {
            const keel::u32 segCount = keel::min<keel::u32>(opts.segments, 64u);
            const keel::f32 gap = (segCount > 28) ? 1.0f : 2.0f;
            const keel::f32 totalGaps = gap * keel::as<keel::f32>(segCount - 1);
            const keel::f32 segH = keel::max(2.0f, keel::floor((keel::round(trackH) - totalGaps) / keel::as<keel::f32>(segCount)));
            const keel::f32 bottomY = keel::round(trackTop + trackH);
            const bool useRound = (keel::min(barWInt, segH) >= 8.0f);
            const Color unlitBg = disabled ? pal.surfaceActive.WithAlphaF(0.35f) : pal.surfaceActive;
            const Color unlitBorder = disabled ? pal.border.WithAlphaF(0.25f) : pal.border.WithAlphaF(0.65f);

            keel::i32 peakSegIdx = -1;
            if (opts.peakHoldValue.has_value() && range > 0.0) {
                const keel::f32 peakRatio = keel::as<keel::f32>(keel::clamp((*opts.peakHoldValue - rangeMin) / range, 0.0, 1.0));
                if (peakRatio > 0.001f) {
                    peakSegIdx = keel::clamp(
                        keel::ceil_i32(peakRatio * keel::as<keel::f32>(segCount)) - 1,
                        0,
                        keel::as<keel::i32>(segCount) - 1
                    );
                }
            }

            for (keel::u32 i = 0; i < segCount; ++i) {
                const keel::f32 segY = bottomY - keel::as<keel::f32>(i + 1) * segH - keel::as<keel::f32>(i) * gap;
                const keel::f32 segThreshold = keel::as<keel::f32>(i + 1) / keel::as<keel::f32>(segCount);
                const keel::f32 segMidRatio = (keel::as<keel::f32>(i) + 0.5f) / keel::as<keel::f32>(segCount);
                const bool isLit = (ratio >= segThreshold - (0.5f / keel::as<keel::f32>(segCount))) && (ratio > 0.001f);
                const bool isPeakSeg = (keel::as<keel::i32>(i) == peakSegIdx);
                const Rect segRect(barX, segY, barWInt, segH);
                if (isLit || isPeakSeg) {
                    const Color segCol = resolveZoneColor(segMidRatio);
                    if (useRound) canvas.DrawRoundRect(segRect, 1.0f, segCol);
                    else canvas.DrawRect(segRect, segCol);
                } else {
                    if (useRound) canvas.DrawRoundRect(segRect, 1.0f, unlitBg, unlitBorder, 1.0f);
                    else canvas.DrawRect(segRect, unlitBg);
                }
            }
        } else {
            const Rect trackBgRect(barX, trackTop, barWInt, trackH);
            const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
            const Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
            canvas.DrawRoundRect(trackBgRect, barWInt * 0.5f, trackBg, trackBorder, 1.0f);

            if (fillH > 0.5f) {
                const Rect fillRect(barX, trackTop + trackH - fillH, barWInt, fillH);
                canvas.DrawRoundRect(fillRect, barWInt * 0.5f, resolveZoneColor(ratio));
            }
            if (opts.peakHoldValue.has_value() && range > 0.0) {
                const keel::f32 peakRatio = keel::as<keel::f32>(keel::clamp((*opts.peakHoldValue - rangeMin) / range, 0.0, 1.0));
                if (peakRatio > 0.001f) {
                    const keel::f32 peakY = trackTop + trackH - peakRatio * trackH;
                    canvas.DrawRoundRect(Rect(barX - 2.0f, peakY - 1.5f, barWInt + 4.0f, 3.0f), 1.5f, resolveZoneColor(peakRatio));
                }
            }
        }
        return;
    }

    const keel::f32 trackRegionX = ctrlRect.x + cm.leftGutterW;
    const keel::f32 trackRegionW = keel::max(12.0f, ctrlRect.w - cm.leftGutterW - cm.rightGutterW);
    const keel::f32 contentOffsetY = keel::max(0.0f, (ctrlRect.h - cm.intrinsicControlSize.y) * 0.5f);
    const keel::f32 trackRowY = ctrlRect.y + contentOffsetY + cm.topValH;
    const keel::f32 centerY = trackRowY + cm.trackRowH * 0.5f;

    const keel::f32 fillW = ratio * trackRegionW;
    const keel::f32 tipX = trackRegionX + fillW;

    // 0. Draw companion textLeft / textRight and value readout
    RenderSliderCompanionTexts(
        canvas, ctrlRect, trackRegionX, trackRegionW, trackRowY, tipX,
        cm, opts.textLeft, opts.textRight, fmtText, valBuf,
        effShowVal, opts.valuePlacement, disabled, theme
    );

    const bool isSeg3D = (theme.controlStyle.progressStyle == ProgressFillStyle::Segmented3D);
    const bool sharpRect = isSeg3D || (m.cornerRadius <= 0.01f);
    const keel::f32 effBarThick = isSeg3D ? keel::max(barThick + 4.0f, 14.0f) : barThick;
    const keel::f32 barY = keel::round(centerY - effBarThick * 0.5f);
    const keel::f32 barHInt = keel::max(2.0f, keel::round(effBarThick));

    if (opts.indeterminate) {
        const Rect trackBgRect(trackRegionX, barY, trackRegionW, effBarThick);
        const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
        const Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
        theme.Painter().DrawProgressBar(
            canvas, theme, trackBgRect, 0.0f, false, true, animPhase,
            trackBg, trackBorder, ResolveSliderTrackColor(opts.color, disabled, theme), m.cornerRadius, disabled
        );
        return;
    }

    if (opts.segments > 1) {
        // Segmented LED / VU Level Meter (integer pixel-snapped cells & gaps)
        const keel::u32 segCount = keel::min<keel::u32>(opts.segments, 64u);
        const keel::f32 gap = (segCount > 28) ? 1.0f : 2.0f;
        const keel::f32 totalGaps = gap * keel::as<keel::f32>(segCount - 1);
        const keel::f32 segW = keel::max(2.0f, keel::floor((keel::round(trackRegionW) - totalGaps) / keel::as<keel::f32>(segCount)));
        const keel::f32 startX = keel::round(trackRegionX);
        const bool useRound = !sharpRect && (keel::min(segW, barHInt) >= 8.0f);
        const Color unlitBg = disabled ? pal.surfaceActive.WithAlphaF(0.35f) : pal.surfaceActive;
        const Color unlitBorder = disabled ? pal.border.WithAlphaF(0.25f) : pal.border.WithAlphaF(0.65f);

        keel::i32 peakSegIdx = -1;
        if (opts.peakHoldValue.has_value() && range > 0.0) {
            const keel::f32 peakRatio = keel::as<keel::f32>(keel::clamp((*opts.peakHoldValue - rangeMin) / range, 0.0, 1.0));
            if (peakRatio > 0.001f) {
                peakSegIdx = keel::clamp(
                    keel::ceil_i32(peakRatio * keel::as<keel::f32>(segCount)) - 1,
                    0,
                    keel::as<keel::i32>(segCount) - 1
                );
            }
        }

        for (keel::u32 i = 0; i < segCount; ++i) {
            const keel::f32 segX = startX + keel::as<keel::f32>(i) * (segW + gap);
            const keel::f32 segThreshold = keel::as<keel::f32>(i + 1) / keel::as<keel::f32>(segCount);
            const keel::f32 segMidRatio = (keel::as<keel::f32>(i) + 0.5f) / keel::as<keel::f32>(segCount);
            const bool isLit = (ratio >= segThreshold - (0.5f / keel::as<keel::f32>(segCount))) && (ratio > 0.001f);
            const bool isPeakSeg = (keel::as<keel::i32>(i) == peakSegIdx);

            const Rect segRect(segX, barY, segW, barHInt);
            if (isLit || isPeakSeg) {
                const Color segCol = resolveZoneColor(segMidRatio);
                if (useRound) {
                    canvas.DrawRoundRect(segRect, 1.0f, segCol);
                } else {
                    canvas.DrawRect(segRect, segCol);
                }
            } else {
                if (useRound) {
                    canvas.DrawRoundRect(segRect, 1.0f, unlitBg, unlitBorder, 1.0f);
                } else {
                    canvas.DrawRect(segRect, unlitBg);
                }
            }
        }
    } else {
        const Rect trackBgRect(trackRegionX, barY, trackRegionW, effBarThick);
        const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
        const Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
        theme.Painter().DrawProgressBar(
            canvas, theme, trackBgRect, ratio, false, false, animPhase,
            trackBg, trackBorder, resolveZoneColor(ratio), m.cornerRadius, disabled
        );

        // Optional peak-hold marker tick on smooth bar
        if (opts.peakHoldValue.has_value() && range > 0.0) {
            const keel::f32 peakRatio = keel::as<keel::f32>(keel::clamp((*opts.peakHoldValue - rangeMin) / range, 0.0, 1.0));
            if (peakRatio > 0.001f) {
                const keel::f32 peakX = trackRegionX + peakRatio * trackRegionW;
                const Color peakCol = resolveZoneColor(peakRatio);
                canvas.DrawRoundRect(Rect(peakX - 1.5f, barY - 2.0f, 3.0f, effBarThick + 4.0f), sharpRect ? 0.0f : 1.5f, peakCol);
            }
        }
    }
}

} // namespace guikit
