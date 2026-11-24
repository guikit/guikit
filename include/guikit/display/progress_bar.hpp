#pragma once

#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/types.hpp"
#include "guikit/style/theme.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

class Context;
class ICanvas;

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

/// Autonomous ProgressBar & Segmented LED / VU LevelMeter widget logic.
namespace detail {

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

} // namespace detail

inline Response ProgressBar(
    Context& ui,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
    ProgressBarOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::ProgressBar::Run(ui, value, min, max, opts, loc);
}

inline Response ProgressBar(
    Context& ui,
    keel::f64 percent0to100,
    ProgressBarOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::ProgressBar::Run(ui, percent0to100, 0.0, 100.0, opts, loc);
}

} // namespace guikit
