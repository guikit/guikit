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

/// Clean configuration options for Radio widget.
struct RadioOptions {
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    StateColor color{};                            ///< Per-state accent fill color when selected
    Align align{Align::Start};                     ///< Standalone control alignment (Start, Center, End)
    StretchMode stretch{StretchMode::Compact};     ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Control width: in points for Compact/Space (nullopt = auto), or % of bounds (0..100) for Fill (nullopt = 100%)
    std::optional<float> height{std::nullopt};     ///< Explicit control height in points (nullopt = auto from UiSize)
    StateColor background{};                       ///< Per-state control well backdrop color (Default/None = off, Surface/Primary/etc. = on)
    Border border{};                               ///< Control well border stroke & corner radius configuration
    bool disabled{false};                          ///< Interaction suppression
};
static_assert(sizeof(RadioOptions) <= 384, "RadioOptions must stay compact");

/// Autonomous Radio widget logic (execution, measuring, and rendering).
class Radio {
public:
    /// Primary immediate mode entry point for Radio widget with state pointer and options.
    static Response Run(
        Context& ctx,
        bool* value,
        bool selectedState,
        const RadioOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on radio indicator size and control spec.
    static Vec2 Measure(
        const RadioOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const RadioOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    /// Renders the circular radio ring and inner selection indicator dot.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        bool selected,
        const RadioOptions& opts,
        UiSize size,
        bool hovered,
        bool pressed,
        bool disabled,
        const UiTheme& theme,
        bool focused = false
    );
};

} // namespace guikit
