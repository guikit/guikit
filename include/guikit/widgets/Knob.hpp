#pragma once

#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for rotary Knob / Dial widget.
struct KnobOptions {
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    UiText valueFormat{};                          ///< Format string for value (e.g. "%.1f dB", "%.0f %%"; empty = default numeric format)
    std::optional<double> step{std::nullopt};      ///< Step quantization (defaults to 1.0 for integers, continuous for float/double)
    std::optional<double> defaultValue{std::nullopt}; ///< Optional reset value on double-click
    uint32_t tickCount{11};                        ///< Number of radial tick marks when showTicks is true (0 = auto from step or 11)
    bool showValue{true};                          ///< Whether to display current formatted value text
    bool showTicks{true};                          ///< Render radial scale tick marks around the dial
    bool showArc{true};                            ///< Render outer track & active value arc around the rotor
    bool bipolar{false};                           ///< Sweep active arc from center/zero instead of minimum
    Placement valuePlacement{Placement::Bottom};   ///< Value position relative to dial: Bottom, Top, Thumb (inside center), Leading, Trailing
    StateColor color{};                            ///< Per-state accent fill color for active arc, pointer, and ticks
    StretchMode stretch{StretchMode::Compact};     ///< Sizing behavior: Compact, Space, Fill
    Align align{Align::Center};                    ///< Standalone control alignment (Start, Center, End)
    std::optional<float> width{std::nullopt};      ///< Control width: in points for Compact/Space (nullopt = auto from UiSize), or % for Fill
    std::optional<float> height{std::nullopt};     ///< Explicit control height in points (nullopt = auto from UiSize)
    StateColor background{};                       ///< Per-state control well backdrop color (Default/None = off, Surface/Primary/etc. = on)
    Border border{};                               ///< Control well border stroke & corner radius configuration
    bool disabled{false};                          ///< Interaction suppression
};

/// Internal runtime state stored per-widget ID for Knob drag interaction.
struct KnobState {
    bool dragging{false};
    double dragStartVal{0.0};
    Vec2 dragStartMouse{};
};

/// Internal runtime payload allocated in frame arena for Knob nodes.
struct KnobData {
    KnobOptions opts{};
    float normalizedValue{0.0f};
    float normalizedOrigin{0.0f};
    std::string_view valueText{};
    bool active{false};
};

/// Autonomous rotary Knob widget logic (execution, measuring, and rendering).
class Knob {
public:
    static Response Run(
        Context& ctx,
        double* doubleVal,
        float* floatVal,
        int* intVal,
        double minVal,
        double maxVal,
        const KnobOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    static Vec2 Measure(
        const KnobOptions& opts,
        std::string_view valueText,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const KnobOptions& opts,
        std::string_view valueText,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    static void Render(
        ICanvas& canvas,
        Rect bounds,
        float normalizedValue,
        float normalizedOrigin,
        std::string_view valueText,
        const KnobOptions& opts,
        UiSize size,
        bool hovered,
        bool active,
        bool disabled,
        const UiTheme& theme,
        bool focused = false
    );
};

} // namespace guikit
