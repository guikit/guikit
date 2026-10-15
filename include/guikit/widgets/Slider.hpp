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
    UiText label{};                                ///< External primary caption text above or beside control
    UiText textLeft{};                             ///< Optional companion text inside control on the left of the track
    UiText textRight{};                            ///< Optional companion text inside control on the right of the track
    UiText description{};                          ///< Optional external explanation text
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    Icon icon{};                                   ///< Optional leading icon in external Element label bar
    UiText badge{};                                ///< Optional trailing pill badge in external Element label bar
    UiText valueFormat{};                          ///< Format string and style for value (e.g. "%.1f %%", {"%.1f dB", UiVariant::Info}, or static text; empty text = default numeric format)
    std::optional<double> step{std::nullopt};      ///< Step quantization (defaults to 1.0 for integers, continuous for float)
    ElementLayout layout{ElementLayout::Stacked};    ///< Widget layout (Stacked, Inline, Top, Bottom)
    InlineOrder inlineOrder{InlineOrder::LabelFirst}; ///< Sequencing order in Inline layout (LabelFirst = text first, ControlFirst = control first)
    Align elementAlign{Align::Start};                ///< Element composition and label alignment (Start, Center, End)
    bool showValue{true};                          ///< Whether to display current formatted value text inside control
    Placement valuePlacement{Placement::Trailing}; ///< Value position relative to track: Leading, Trailing, Top, Bottom, Thumb
    UiVariant variant{UiVariant::Primary};         ///< Accent fill color for active track and thumb
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
    std::optional<float> scale{std::nullopt};      ///< Local scale override
    const UiTheme* theme{nullptr};                 ///< Scoped theme override
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Control width: in points for Compact/Space (nullopt = from theme), or % of bounds (0..100) for Fill (nullopt = 100%)
    bool divider{false};                           ///< Whether to render an Element divider line
    ElementDivider dividerStyle{ElementDivider::Bottom}; ///< Element divider placement style (Bottom or Trailing)
    bool disabled{false};                          ///< Interaction suppression
};

/// Clean configuration options for discrete N-position StepSlider widget.
struct StepSliderOptions {
    UiText label{};                                ///< External primary caption text above or beside control
    UiText textLeft{};                             ///< Optional companion text inside control on the left of the track
    UiText textRight{};                            ///< Optional companion text inside control on the right of the track
    UiText description{};                          ///< Optional external explanation text
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    Icon icon{};                                   ///< Optional leading icon in external Element label bar
    UiText badge{};                                ///< Optional trailing pill badge in external Element label bar
    std::span<const std::string_view> stepLabels{};///< Optional text labels for each discrete step
    ElementLayout layout{ElementLayout::Stacked};    ///< Widget layout (Stacked, Inline, Top, Bottom)
    InlineOrder inlineOrder{InlineOrder::LabelFirst}; ///< Sequencing order in Inline layout (LabelFirst = text first, ControlFirst = control first)
    Align elementAlign{Align::Start};                ///< Element composition and label alignment (Start, Center, End)
    bool showTicks{true};                          ///< Whether to render tick marks at each step
    bool showLabels{true};                         ///< Whether to render stepLabels below ticks
    bool showValue{false};                         ///< Whether to display currently selected label/value inside control
    Placement valuePlacement{Placement::Trailing}; ///< Value position relative to track: Leading, Trailing, Top, Bottom, Thumb
    UiText valueText{};                            ///< Optional custom display text/style for current value
    bool includeNone{false};                       ///< For UiSize overload: if true, range includes UiSize::None (None..Xl)
    UiVariant variant{UiVariant::Primary};         ///< Accent fill color for active track and thumb
    std::optional<UiSize> size{std::nullopt};      ///< Local size override
    std::optional<float> scale{std::nullopt};      ///< Local scale override
    const UiTheme* theme{nullptr};                 ///< Scoped theme override
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Control width: in points for Compact/Space (nullopt = from theme), or % of bounds (0..100) for Fill (nullopt = 100%)
    bool divider{false};                           ///< Whether to render an Element divider line
    ElementDivider dividerStyle{ElementDivider::Bottom}; ///< Element divider placement style (Bottom or Trailing)
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

} // namespace guikit
