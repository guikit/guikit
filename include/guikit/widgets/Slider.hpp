#pragma once

#include <optional>
#include <span>
#include <source_location>
#include <string_view>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for continuous numeric Slider widget.
struct SliderOptions {
    std::string_view label{};                      ///< Primary caption text above or beside track
    std::string_view description{};                ///< Optional explanation text below slider
    std::string_view suffix{};                     ///< Optional unit/suffix displayed beside value (e.g. "px", "%")
    std::string_view format{};                     ///< Format string for value (defaults to "%d" for int, "%.1f" for float)
    std::optional<double> step{std::nullopt};      ///< Step quantization (defaults to 1.0 for integers, continuous for float)
    Placement labelPlacement{Placement::Stacked};  ///< Label placement (Stacked = header row, Leading = side-by-side)
    bool showValue{true};                          ///< Whether to display current formatted value text
    UiVariant variant{UiVariant::Primary};         ///< Accent fill color for active track and thumb
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
    std::optional<float> scale{std::nullopt};      ///< Local scale override
    const UiTheme* theme{nullptr};                 ///< Scoped theme override
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Between (space-between), Fill
    std::optional<float> width{std::nullopt};      ///< Explicit track width in points (nullopt = from theme)
    bool disabled{false};                          ///< Interaction suppression
};

/// Clean configuration options for discrete N-position StepSlider widget.
struct StepSliderOptions {
    std::string_view label{};                      ///< Primary caption text above track
    std::string_view description{};                ///< Optional explanation text below slider
    std::span<const std::string_view> stepLabels{};///< Optional text labels for each discrete step
    Placement labelPlacement{Placement::Stacked};  ///< Label placement
    bool showTicks{true};                          ///< Whether to render tick marks at each step
    bool showLabels{true};                         ///< Whether to render stepLabels below ticks
    bool showValue{false};                         ///< Whether to display currently selected label/index in header
    std::string_view valueText{};                  ///< Optional custom display string for current value in header
    bool includeNone{false};                       ///< For UiSize overload: if true, range includes UiSize::None (None..Xl)
    UiVariant variant{UiVariant::Primary};         ///< Accent fill color for active track and thumb
    std::optional<UiSize> size{std::nullopt};      ///< Local size override
    std::optional<float> scale{std::nullopt};      ///< Local scale override
    const UiTheme* theme{nullptr};                 ///< Scoped theme override
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Between (space-between), Fill
    std::optional<float> width{std::nullopt};      ///< Explicit track width in points (nullopt = from theme)
    bool disabled{false};                          ///< Interaction suppression
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
        const UiTheme& theme
    );
    static Vec2 Measure(
        const SliderOptions& opts,
        double value,
        double min,
        double max,
        UiSize size,
        ICanvas* canvas = nullptr
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
        const UiTheme& theme
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
        const UiTheme& theme
    );
    static Vec2 Measure(
        const StepSliderOptions& opts,
        size_t index,
        size_t totalSteps,
        UiSize size,
        ICanvas* canvas = nullptr
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
        const UiTheme& theme
    );
};

} // namespace guikit
