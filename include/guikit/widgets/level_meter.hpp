#pragma once

#include "guikit/context.hpp"
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

struct LevelMeterOptions {
    Orientation orientation{Orientation::Horizontal};  ///< Horizontal or Vertical meter bar(s)
    keel::u32 segments{24};                            ///< 0 = smooth continuous bar; >0 = discrete LED cells (e.g. 16, 24, 32)
    bool multiZoneColor{true};                         ///< 3-zone coloring (Success/Green -> Warning/Amber -> Danger/Red)
    keel::f32 warningRatio{0.70f};                         ///< Normalized threshold [0..1] for Warning zone
    keel::f32 dangerRatio{0.88f};                          ///< Normalized threshold [0..1] for Danger zone
    keel::Optional<keel::f64> peakHoldValue{keel::kNullOpt}; ///< Explicit peak-hold marker value (if nullopt and autoPeakHold is true, tracked automatically)
    bool autoPeakHold{true};                           ///< Automatically track & decay peak-hold marker per widget ID
    keel::f32 peakHoldDecayPerSec{0.15f};                  ///< Normalized decay rate per second for auto peak-hold after hold time
    bool showClipLed{true};                            ///< Show dedicated overload/clip indicator LED cap at max end (click meter to reset clip/peak latch)
    bool showScale{true};                              ///< Render calibrated dB/numeric scale ticks & labels alongside track
    bool showValue{true};                              ///< Display formatted numeric readout tag
    UiText valueFormat{"%.1f dB"};                     ///< Format string for numeric value (e.g. "%.1f dB", "%.0f %%")
    keel::ArrayRef<keel::StringRef> channelLabels{};   ///< Optional per-channel labels (e.g. {"L", "R"} or {"CH1", "CH2"})
    keel::Optional<keel::f32> barThickness{keel::kNullOpt};///< Thickness of each channel bar in logical dp (nullopt = auto from UiSize)
    UiText tooltip{};                                  ///< Optional floating tooltip text on hover
    bool disabled{false};                            ///< Interaction suppression
    UiSize size{UiSize::Md};                         ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                                ///< Horizontal sizing policy (`Auto` -> `Stretch` when horizontal, `Fit` when vertical)
    AxisSize height{};                               ///< Vertical sizing policy (`Auto` -> `Fit` when horizontal, `Stretch` when vertical)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};  ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};  ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt}; ///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt}; ///< Optional maximum height in logical dp
    StateColor color{};                              ///< Base zone semantic color override
};

namespace detail {

class LevelMeter {
public:
    /// Single-channel LevelMeter.
    static Response Run(
        Context& ctx,
        keel::f64 value,
        keel::f64 min = -60.0,
        keel::f64 max = 0.0,
        const LevelMeterOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Multi-channel (e.g. Stereo L/R or N-channel rack) LevelMeter.
    static Response Run(
        Context& ctx,
        keel::ArrayRef<keel::f32> channelValues,
        keel::f32 min = -60.0f,
        keel::f32 max = 0.0f,
        const LevelMeterOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        keel::usize channelCount,
        keel::f64 min,
        keel::f64 max,
        const LevelMeterOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        keel::f32 availableWidth = 0.0f
    );

    static void Render(const Node* node, ICanvas& canvas, const UiTheme& theme);
};

} // namespace detail

inline Response LevelMeter(
    Context& ui,
    keel::f64 value,
    keel::f64 min = -60.0,
    keel::f64 max = 0.0,
    LevelMeterOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::LevelMeter::Run(ui, value, min, max, opts, loc);
}

inline Response LevelMeter(
    Context& ui,
    keel::ArrayRef<keel::f32> channelValues,
    keel::f32 min = -60.0f,
    keel::f32 max = 0.0f,
    LevelMeterOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::LevelMeter::Run(ui, channelValues, min, max, opts, loc);
}

} // namespace guikit
