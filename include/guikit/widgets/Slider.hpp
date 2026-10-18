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

/// Clean configuration options for continuous numeric Slider widget.
struct SliderOptions {
    UiText textLeft{};                             ///< Optional companion text inside control on the left of the track
    UiText textRight{};                            ///< Optional companion text inside control on the right of the track
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    UiText valueFormat{};                          ///< Format string and style for value (e.g. "%.1f %%", {"%.1f dB", StateColor::Info()}, or static text; empty text = default numeric format)
    std::optional<double> step{std::nullopt};      ///< Step quantization (defaults to 1.0 for integers, continuous for float)
    Align align{Align::Start};                     ///< Standalone control alignment (Start, Center, End)
    bool showValue{true};                          ///< Whether to display current formatted value text inside control
    Placement valuePlacement{Placement::Trailing}; ///< Value position relative to track: Leading, Trailing, Top, Bottom, Thumb
    StateColor color{};                            ///< Per-state accent fill color for active track and thumb
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
    std::optional<float> scale{std::nullopt};      ///< Local scale override
    const UiTheme* theme{nullptr};                 ///< Scoped theme override
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Control width: in points for Compact/Space (nullopt = from theme), or % of bounds (0..100) for Fill (nullopt = 100%)
    std::optional<float> height{std::nullopt};     ///< Explicit control height in points (nullopt = intrinsic height from UiSize)
    Background background{false};                  ///< Control well backdrop configuration (false, true, Color, or StateColor)
    Border border{};                               ///< Control well border stroke & corner radius configuration
    bool disabled{false};                          ///< Interaction suppression
};

/// Clean configuration options for discrete N-position StepSlider widget.
struct StepSliderOptions {
    UiText textLeft{};                             ///< Optional companion text inside control on the left of the track
    UiText textRight{};                            ///< Optional companion text inside control on the right of the track
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    std::span<const std::string_view> stepLabels{};///< Optional text labels for each discrete step
    Align align{Align::Start};                     ///< Standalone control alignment (Start, Center, End)
    bool showTicks{true};                          ///< Whether to render tick marks at each step
    bool showLabels{true};                         ///< Whether to render stepLabels below ticks
    bool showValue{false};                         ///< Whether to display currently selected label/value inside control
    Placement valuePlacement{Placement::Trailing}; ///< Value position relative to track: Leading, Trailing, Top, Bottom, Thumb
    UiText valueText{};                            ///< Optional custom display text/style for current value
    bool includeNone{false};                       ///< For UiSize overload: if true, range includes UiSize::None (None..Xl)
    StateColor color{};                            ///< Per-state accent fill color for active track and thumb
    std::optional<UiSize> size{std::nullopt};      ///< Local size override
    std::optional<float> scale{std::nullopt};      ///< Local scale override
    const UiTheme* theme{nullptr};                 ///< Scoped theme override
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Control width: in points for Compact/Space (nullopt = from theme), or % of bounds (0..100) for Fill (nullopt = 100%)
    std::optional<float> height{std::nullopt};     ///< Explicit control height in points (nullopt = intrinsic height from UiSize)
    Background background{false};                  ///< Control well backdrop configuration (false, true, Color, or StateColor)
    Border border{};                               ///< Control well border stroke & corner radius configuration
    bool disabled{false};                          ///< Interaction suppression
};

/// Clean configuration options for two-thumb continuous/stepped RangeSlider widget.
struct RangeSliderOptions {
    UiText textLeft{};                             ///< Optional companion text inside control on the left of the track
    UiText textRight{};                            ///< Optional companion text inside control on the right of the track
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    UiText valueFormat{};                          ///< Format string for range values (e.g. "%.0f .. %.0f Hz" or "%.1f %%")
    std::optional<double> step{std::nullopt};      ///< Step quantization (defaults to 1.0 for integers, continuous for float)
    double minSpan{0.0};                           ///< Minimum separation between low and high values
    Align align{Align::Start};                     ///< Standalone control alignment (Start, Center, End)
    bool showValue{true};                          ///< Whether to display current formatted range text inside control
    Placement valuePlacement{Placement::Trailing}; ///< Value position relative to track: Leading, Trailing, Top, Bottom, Thumb
    StateColor color{};                            ///< Per-state accent fill color for active interval and thumbs
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
    std::optional<float> scale{std::nullopt};      ///< Local scale override
    const UiTheme* theme{nullptr};                 ///< Scoped theme override
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Control width: in points for Compact/Space (nullopt = from theme), or % of bounds (0..100) for Fill (nullopt = 100%)
    std::optional<float> height{std::nullopt};     ///< Explicit control height in points (nullopt = intrinsic height from UiSize)
    Background background{false};                  ///< Control well backdrop configuration (false, true, Color, or StateColor)
    Border border{};                               ///< Control well border stroke & corner radius configuration
    bool disabled{false};                          ///< Interaction suppression
};

/// Clean configuration options for continuous ProgressBar & Segmented LED / VU LevelMeter widget.
struct ProgressBarOptions {
    UiText textLeft{};                             ///< Optional companion text inside control on the left of the track
    UiText textRight{};                            ///< Optional companion text inside control on the right of the track
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    UiText valueFormat{};                          ///< Format string for value (e.g. "%.0f %%", "%.1f dB"; empty = default numeric format)
    std::optional<double> peakHoldValue{std::nullopt}; ///< Optional peak-hold marker value on the track (for VU/dB level meters)
    uint32_t segments{0};                          ///< 0 = smooth continuous bar; >0 = segmented LED/VU level meter (e.g. 20 segments)
    float warningRatio{0.70f};                     ///< Normalized threshold [0..1] for Warning zone when multiZoneColor is true
    float dangerRatio{0.90f};                      ///< Normalized threshold [0..1] for Danger zone when multiZoneColor is true
    std::optional<float> trackHeight{std::nullopt};///< Optional bar thickness override in points
    Align align{Align::Start};                     ///< Standalone control alignment (Start, Center, End)
    bool showValue{true};                          ///< Whether to display current formatted value text inside control
    Placement valuePlacement{Placement::Trailing}; ///< Value position relative to track: Leading, Trailing, Top, Bottom, Thumb
    StateColor color{};                            ///< Per-state accent fill color for active bar (or base zone when multiZoneColor is true)
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
    std::optional<float> scale{std::nullopt};      ///< Local scale override
    const UiTheme* theme{nullptr};                 ///< Scoped theme override
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Control width: in points for Compact/Space (nullopt = from theme), or % of bounds (0..100) for Fill (nullopt = 100%)
    std::optional<float> height{std::nullopt};     ///< Explicit control height in points (nullopt = intrinsic height from UiSize)
    bool multiZoneColor{false};                    ///< Color bar/segments across 3 zones (Variant/Success -> Warning -> Danger)
    Background background{false};                  ///< Control well backdrop configuration (false, true, Color, or StateColor)
    Border border{};                               ///< Control well border stroke & corner radius configuration
    bool disabled{false};                          ///< Dimmed disabled state
};

/// Internal runtime interaction state stored per-widget ID.
struct SliderState {
    bool dragging{false};
    double dragStartVal{0.0};
    Vec2 dragStartMouse{};
};

/// Autonomous Slider widget logic.
class Slider {
public:
    static Response Run(
        Context& ctx,
        double* valDouble,
        float* valFloat,
        int64_t* valInt,
        double min,
        double max,
        const SliderOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Vec2 Measure(
        const SliderOptions& opts,
        double value,
        double min,
        double max,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const SliderOptions& opts,
        double value,
        double min,
        double max,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    static void Render(
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
        bool focused = false
    );
};

/// Autonomous discrete N-position StepSlider widget logic.
class StepSlider {
public:
    static Response Run(
        Context& ctx,
        size_t* indexSizeT,
        int* indexInt,
        size_t totalSteps,
        const StepSliderOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Vec2 Measure(
        const StepSliderOptions& opts,
        size_t index,
        size_t totalSteps,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const StepSliderOptions& opts,
        size_t index,
        size_t totalSteps,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    static void Render(
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
        bool focused = false
    );
};

/// Autonomous two-thumb RangeSlider ([low .. high] + draggable interval span) widget logic.
class RangeSlider {
public:
    static Response Run(
        Context& ctx,
        double* lowDouble,
        double* highDouble,
        float* lowFloat,
        float* highFloat,
        int64_t* lowInt,
        int64_t* highInt,
        double min,
        double max,
        const RangeSliderOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Vec2 Measure(
        const RangeSliderOptions& opts,
        double lowValue,
        double highValue,
        double min,
        double max,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const RangeSliderOptions& opts,
        double lowValue,
        double highValue,
        double min,
        double max,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    static void Render(
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
        bool focused = false,
        uint8_t activeThumb = 1
    );
};

/// Autonomous ProgressBar & Segmented LED / VU LevelMeter widget logic.
class ProgressBar {
public:
    static Response Run(
        Context& ctx,
        double value,
        double min,
        double max,
        const ProgressBarOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Vec2 Measure(
        const ProgressBarOptions& opts,
        double value,
        double min,
        double max,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const ProgressBarOptions& opts,
        double value,
        double min,
        double max,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    static void Render(
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
    );
};

} // namespace guikit
