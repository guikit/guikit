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

struct LevelMeterOptions {
    Orientation orientation{Orientation::Horizontal}; ///< Horizontal or Vertical meter bar(s)
    keel::u32 segments{24};                            ///< 0 = smooth continuous bar; >0 = discrete LED cells (e.g. 16, 24, 32)
    bool multiZoneColor{true};                        ///< 3-zone coloring (Success/Green -> Warning/Amber -> Danger/Red)
    float warningRatio{0.70f};                        ///< Normalized threshold [0..1] for Warning zone
    float dangerRatio{0.88f};                         ///< Normalized threshold [0..1] for Danger zone
    keel::Optional<double> peakHoldValue{keel::kNullOpt}; ///< Explicit peak-hold marker value (if nullopt and autoPeakHold is true, tracked automatically)
    bool autoPeakHold{true};                          ///< Automatically track & decay peak-hold marker per widget ID
    float peakHoldDecayPerSec{0.15f};                 ///< Normalized decay rate per second for auto peak-hold after hold time
    bool showClipLed{true};                           ///< Show dedicated overload/clip indicator LED cap at max end (click meter to reset clip/peak latch)
    bool showScale{true};                             ///< Render calibrated dB/numeric scale ticks & labels alongside track
    bool showValue{true};                             ///< Display formatted numeric readout tag
    UiText valueFormat{"%.1f dB"};                    ///< Format string for numeric value (e.g. "%.1f dB", "%.0f %%")
    keel::ArrayRef<keel::StringRef> channelLabels{};  ///< Optional per-channel labels (e.g. {"L", "R"} or {"CH1", "CH2"})
    keel::Optional<float> barThickness{keel::kNullOpt}; ///< Thickness of each channel bar in logical dp (nullopt = auto from UiSize)
    StateColor color{};                               ///< Base zone color override (defaults to Success when multiZoneColor is true, or Primary when false)
    AxisSize width{};                                 ///< Horizontal sizing policy (`Auto`: `Stretch` when Horizontal, `Fit` when Vertical; or `Fit`/`Stretch`/`Fixed(dp)`)
    AxisSize height{};                                ///< Vertical sizing policy (`Auto`: `Fit` when Horizontal, `Stretch` when Vertical; or `Fit`/`Stretch`/`Fixed(dp)`)
    UiText tooltip{};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    keel::Optional<float> scale{keel::kNullOpt};
    keel::Optional<bool> debugBounds{keel::kNullOpt};
    keel::Optional<bool> inspectMode{keel::kNullOpt};
};

class LevelMeter {
public:
    /// Single-channel LevelMeter.
    static Response Run(
        Context& ctx,
        double value,
        double min = -60.0,
        double max = 0.0,
        const LevelMeterOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Multi-channel (e.g. Stereo L/R or N-channel rack) LevelMeter.
    static Response Run(
        Context& ctx,
        keel::ArrayRef<float> channelValues,
        float min = -60.0f,
        float max = 0.0f,
        const LevelMeterOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        keel::usize channelCount,
        double min,
        double max,
        const LevelMeterOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );

    static void Render(const Node* node, ICanvas& canvas, const UiTheme& theme);
};

} // namespace guikit
