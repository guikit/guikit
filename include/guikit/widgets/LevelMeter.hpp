#pragma once

#include <optional>
#include <span>
#include <source_location>
#include <string_view>
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;

struct LevelMeterOptions {
    Orientation orientation{Orientation::Horizontal}; ///< Horizontal or Vertical meter bar(s)
    uint32_t segments{24};                            ///< 0 = smooth continuous bar; >0 = discrete LED cells (e.g. 16, 24, 32)
    bool multiZoneColor{true};                        ///< 3-zone coloring (Success/Green -> Warning/Amber -> Danger/Red)
    float warningRatio{0.70f};                        ///< Normalized threshold [0..1] for Warning zone
    float dangerRatio{0.88f};                         ///< Normalized threshold [0..1] for Danger zone
    std::optional<double> peakHoldValue{std::nullopt};///< Explicit peak-hold marker value (if nullopt and autoPeakHold is true, tracked automatically)
    bool autoPeakHold{true};                          ///< Automatically track & decay peak-hold marker per widget ID
    float peakHoldDecayPerSec{0.15f};                 ///< Normalized decay rate per second for auto peak-hold after hold time
    bool showClipLed{true};                           ///< Show dedicated overload/clip indicator LED cap at max end (click meter to reset clip/peak latch)
    bool showScale{true};                             ///< Render calibrated dB/numeric scale ticks & labels alongside track
    bool showValue{true};                             ///< Display formatted numeric readout tag
    UiText valueFormat{"%.1f dB"};                    ///< Format string for numeric value (e.g. "%.1f dB", "%.0f %%")
    std::span<const std::string_view> channelLabels{};///< Optional per-channel labels (e.g. {"L", "R"} or {"CH1", "CH2"})
    std::optional<float> barThickness{std::nullopt};  ///< Thickness of each channel bar in logical dp (nullopt = auto from UiSize)
    StateColor color{};                               ///< Base zone color override (defaults to Success when multiZoneColor is true, or Primary when false)
    StretchMode stretch{StretchMode::Fill};
    Align align{Align::Start};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};
    StateColor background{};
    Border border{};
    UiText tooltip{};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
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
        std::source_location loc = std::source_location::current()
    );

    /// Multi-channel (e.g. Stereo L/R or N-channel rack) LevelMeter.
    static Response Run(
        Context& ctx,
        std::span<const float> channelValues,
        float min = -60.0f,
        float max = 0.0f,
        const LevelMeterOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Vec2 Measure(
        size_t channelCount,
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
