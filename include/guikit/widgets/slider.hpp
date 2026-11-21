#pragma once

#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for continuous numeric Slider widget.
///
/// `orientation = Orientation::Vertical` turns the slider into a fader: min at the bottom, max at the top.
/// The main axis is then the height: `height` defaults to `SizeMode::Stretch` to fill available height,
/// `AxisSize::Fixed(dp)` sets an explicit height or width in logical dp.
/// Companion texts and the value move along the main axis: `textRight` (max end) and a Trailing/Top value
/// above the track, a Leading/Bottom value and `textLeft` (min end) below it, a Thumb value right of the thumb.
struct SliderOptions {
    Orientation orientation{Orientation::Horizontal}; ///< Horizontal (min at left) or Vertical (min at bottom); Both = Horizontal
    UiText textLeft{};                                ///< Optional companion text inside control on the left of the track (below it when Vertical)
    UiText textRight{};                               ///< Optional companion text inside control on the right of the track (above it when Vertical)
    UiText valueFormat{};                             ///< Format string and style for value (e.g. "%.1f %%", {"%.1f dB", StateColor::Info()}, or static text; empty text = default numeric format)
    keel::Optional<keel::f64> step{keel::kNullOpt};      ///< Step quantization (defaults to 1.0 for integers, continuous for float)
    bool showValue{true};                             ///< Whether to display current formatted value text inside control
    Placement valuePlacement{Placement::Trailing};    ///< Value position relative to track: Leading, Trailing, Top, Bottom, Thumb
    UiText tooltip{};                                 ///< Optional floating tooltip text on hover
    bool disabled{false};                             ///< Interaction suppression
    UiSize size{UiSize::Md};                          ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                                 ///< Horizontal sizing policy (`Auto` -> `Stretch` when horizontal, `Fit` when vertical)
    AxisSize height{};                                ///< Vertical sizing policy (`Auto` -> `Fit` when horizontal, `Stretch` when vertical)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};   ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};   ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};  ///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};  ///< Optional maximum height in logical dp
    StateColor color{};                               ///< Per-state semantic accent color for active track and thumb
};

/// Clean configuration options for discrete N-position StepSlider widget.
///
/// `orientation = Orientation::Vertical`: first step at the bottom, last at the top; step labels in a column
/// right of the ticks. Sizing and companion texts follow the vertical `SliderOptions` rules.
struct StepSliderOptions {
    Orientation orientation{Orientation::Horizontal}; ///< Horizontal (first step at left) or Vertical (first step at bottom); Both = Horizontal
    UiText textLeft{};                                ///< Optional companion text inside control on the left of the track (below it when Vertical)
    UiText textRight{};                               ///< Optional companion text inside control on the right of the track (above it when Vertical)
    keel::ArrayRef<keel::StringRef> stepLabels{};     ///< Optional text labels for each discrete step
    bool showTicks{true};                             ///< Whether to render tick marks at each step
    bool showLabels{true};                            ///< Whether to render stepLabels below ticks (right of them when Vertical)
    bool showValue{false};                            ///< Whether to display currently selected label/value inside control
    Placement valuePlacement{Placement::Trailing};    ///< Value position relative to track: Leading, Trailing, Top, Bottom, Thumb
    UiText valueText{};                               ///< Optional custom display text/style for current value
    bool includeNone{false};                          ///< For UiSize overload: if true, range includes UiSize::None (None..Xl)
    UiText tooltip{};                                 ///< Optional floating tooltip text on hover
    bool disabled{false};                             ///< Interaction suppression
    UiSize size{UiSize::Md};                          ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                                 ///< Horizontal sizing policy (`Auto` -> `Stretch` when horizontal, `Fit` when vertical)
    AxisSize height{};                                ///< Vertical sizing policy (`Auto` -> `Fit` when horizontal, `Stretch` when vertical)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};   ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};   ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};  ///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};  ///< Optional maximum height in logical dp
    StateColor color{};                               ///< Per-state semantic accent color for active track and thumb
};

/// Clean configuration options for two-thumb continuous/stepped RangeSlider widget.
///
/// `orientation = Orientation::Vertical`: low thumb below, high thumb above. Sizing and companion texts
/// follow the vertical `SliderOptions` rules; a Thumb value sits right of the interval middle.
struct RangeSliderOptions {
    Orientation orientation{Orientation::Horizontal}; ///< Horizontal (min at left) or Vertical (min at bottom); Both = Horizontal
    UiText textLeft{};                                ///< Optional companion text inside control on the left of the track (below it when Vertical)
    UiText textRight{};                               ///< Optional companion text inside control on the right of the track (above it when Vertical)
    UiText valueFormat{};                             ///< Format string for range values (e.g. "%.0f .. %.0f Hz" or "%.1f %%")
    keel::Optional<keel::f64> step{keel::kNullOpt};      ///< Step quantization (defaults to 1.0 for integers, continuous for float)
    keel::f64 minSpan{0.0};                              ///< Minimum separation between low and high values
    bool showValue{true};                             ///< Whether to display current formatted range text inside control
    Placement valuePlacement{Placement::Trailing};    ///< Value position relative to track: Leading, Trailing, Top, Bottom, Thumb
    UiText tooltip{};                                 ///< Optional floating tooltip text on hover
    bool disabled{false};                             ///< Interaction suppression
    UiSize size{UiSize::Md};                          ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                                 ///< Horizontal sizing policy (`Auto` -> `Stretch` when horizontal, `Fit` when vertical)
    AxisSize height{};                                ///< Vertical sizing policy (`Auto` -> `Fit` when horizontal, `Stretch` when vertical)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};   ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};   ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};  ///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};  ///< Optional maximum height in logical dp
    StateColor color{};                               ///< Per-state semantic accent color for active interval and thumbs
};

/// Clean configuration options for continuous ProgressBar & Segmented LED / VU LevelMeter widget.
struct ProgressBarOptions {
    Orientation orientation{Orientation::Horizontal}; ///< Horizontal (min at left) or Vertical (min at bottom)
    UiText textLeft{};                                ///< Optional companion text inside control on the left of the track (below when Vertical)
    UiText textRight{};                               ///< Optional companion text inside control on the right of the track (above when Vertical)
    UiText valueFormat{};                             ///< Format string for value (e.g. "%.0f %%", "%.1f dB"; empty = default numeric format)
    keel::Optional<keel::f64> peakHoldValue{keel::kNullOpt}; ///< Optional peak-hold marker value on the track (for VU/dB level meters)
    keel::u32 segments{0};                            ///< 0 = smooth continuous bar; >0 = segmented LED/VU level meter (e.g. 20 segments)
    keel::f32 warningRatio{0.70f};                        ///< Normalized threshold [0..1] for Warning zone when multiZoneColor is true
    keel::f32 dangerRatio{0.90f};                         ///< Normalized threshold [0..1] for Danger zone when multiZoneColor is true
    keel::Optional<keel::f32> trackHeight{keel::kNullOpt};///< Optional bar thickness override in points
    bool showValue{true};                             ///< Whether to display current formatted value text inside control
    bool indeterminate{false};                        ///< Animated indeterminate shuttle bar when progress duration is unknown
    Placement valuePlacement{Placement::Trailing};    ///< Value position relative to track: Leading, Trailing, Top, Bottom, Thumb
    bool multiZoneColor{false};                       ///< Color bar/segments across 3 zones (Variant/Success -> Warning -> Danger)
    UiText tooltip{};                                 ///< Optional floating tooltip text on hover
    bool disabled{false};                             ///< Dimmed disabled state
    UiSize size{UiSize::Md};                          ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                                 ///< Horizontal sizing policy (`Auto` -> `Stretch` when horizontal, `Fit` when vertical)
    AxisSize height{};                                ///< Vertical sizing policy (`Auto` -> `Fit` when horizontal, `Stretch` when vertical)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};   ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};   ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};  ///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};  ///< Optional maximum height in logical dp
    StateColor color{};                               ///< Per-state semantic accent color for active bar (or base zone when multiZoneColor is true)
};

/// Internal runtime interaction state stored per-widget ID.
struct SliderState {
    bool dragging{false};
    keel::f64 dragStartVal{0.0};
    Vec2 dragStartMouse{};
};

/// Autonomous Slider widget logic.
class Slider {
public:
    static Response Run(
        Context& ctx,
        keel::f64* valDouble,
        keel::f32* valFloat,
        keel::i64* valInt,
        keel::f64 min,
        keel::f64 max,
        const SliderOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        const SliderOptions& opts,
        keel::f64 value,
        keel::f64 min,
        keel::f64 max,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        keel::f32 availableWidth = 0.0f
    );
    static Vec2 Measure(
        const SliderOptions& opts,
        keel::f64 value,
        keel::f64 min,
        keel::f64 max,
        UiSize size,
        ICanvas* canvas = nullptr,
        keel::f32 availableWidth = 0.0f
    );

    static void Render(
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
        bool focused = false
    );
};

/// Autonomous discrete N-position StepSlider widget logic.
class StepSlider {
public:
    static Response Run(
        Context& ctx,
        keel::usize* indexSizeT,
        keel::i32* indexInt,
        keel::usize totalSteps,
        const StepSliderOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        const StepSliderOptions& opts,
        keel::usize index,
        keel::usize totalSteps,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        keel::f32 availableWidth = 0.0f
    );
    static Vec2 Measure(
        const StepSliderOptions& opts,
        keel::usize index,
        keel::usize totalSteps,
        UiSize size,
        ICanvas* canvas = nullptr,
        keel::f32 availableWidth = 0.0f
    );

    static void Render(
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
        bool focused = false
    );
};

/// Autonomous two-thumb RangeSlider ([low .. high] + draggable interval span) widget logic.
class RangeSlider {
public:
    static Response Run(
        Context& ctx,
        keel::f64* lowDouble,
        keel::f64* highDouble,
        keel::f32* lowFloat,
        keel::f32* highFloat,
        keel::i64* lowInt,
        keel::i64* highInt,
        keel::f64 min,
        keel::f64 max,
        const RangeSliderOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        const RangeSliderOptions& opts,
        keel::f64 lowValue,
        keel::f64 highValue,
        keel::f64 min,
        keel::f64 max,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        keel::f32 availableWidth = 0.0f
    );
    static Vec2 Measure(
        const RangeSliderOptions& opts,
        keel::f64 lowValue,
        keel::f64 highValue,
        keel::f64 min,
        keel::f64 max,
        UiSize size,
        ICanvas* canvas = nullptr,
        keel::f32 availableWidth = 0.0f
    );

    static void Render(
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
        bool focused = false,
        keel::u8 activeThumb = 1
    );
};

/// Autonomous ProgressBar & Segmented LED / VU LevelMeter widget logic.
class ProgressBar {
public:
    static Response Run(
        Context& ctx,
        keel::f64 value,
        keel::f64 min,
        keel::f64 max,
        const ProgressBarOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        const ProgressBarOptions& opts,
        keel::f64 value,
        keel::f64 min,
        keel::f64 max,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        keel::f32 availableWidth = 0.0f
    );
    static Vec2 Measure(
        const ProgressBarOptions& opts,
        keel::f64 value,
        keel::f64 min,
        keel::f64 max,
        UiSize size,
        ICanvas* canvas = nullptr,
        keel::f32 availableWidth = 0.0f
    );

    static void Render(
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
        keel::f32 animPhase = 0.0f
    );
};

} // namespace guikit
