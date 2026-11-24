#pragma once

#include "guikit/widgets/slider.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/style_painter.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/format.h"
#include "keel/math.h"
#include "keel/static_array.h"

namespace guikit::detail::slider_impl {

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
) noexcept;

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
) noexcept;

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
) noexcept;

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
) noexcept;

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
) noexcept;

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
) noexcept;

Color ResolveSliderTrackColor(const StateColor& rawColor, bool disabled, const UiTheme& theme) noexcept;
Color ResolveSliderActiveThumbColor(const StateColor& rawColor, const UiTheme& theme) noexcept;

} // namespace guikit::detail::slider_impl
