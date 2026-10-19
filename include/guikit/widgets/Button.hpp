#pragma once

#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for a Button widget.
struct ButtonOptions {
    UiText text{};                                 ///< Text inside the button capsule
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    Icon icon{};                                   ///< Icon inside the button capsule
    StateColor color{};                            ///< Per-state capsule fill color (normal, hover, active, disabled)
    Placement iconPlacement{Placement::Leading};
    Align align{Align::Center};                    ///< Content horizontal alignment inside button capsule (Start, Center, End)
    StretchMode stretch{StretchMode::Compact};     ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Capsule width: in points for Compact/Space (nullopt = auto), or ratio of bounds (0.0..1.0) for Fill (nullopt = 1.0)
    std::optional<float> height{std::nullopt};     ///< Explicit capsule height in points (nullopt = inherit defaultHeight from UiSize)
    std::optional<float> minWidth{std::nullopt};   ///< Minimum capsule width constraint in points
    StateColor background{StateColor::Surface()};  ///< Capsule surface fill configuration (Surface/Default = on, None = off, or custom StateColor)
    Border border{};                               ///< Capsule border stroke & corner radius configuration
    bool disabled{false};
    bool active{false};
    bool square{false};                            ///< Force square (defaultHeight x defaultHeight) button geometry
    std::optional<Color> iconColor{std::nullopt};  ///< Optional custom icon tint override
};

/// Type alias for button response.
using ButtonResponse = Response;

/// Autonomous Button widget logic (execution, measuring, and rendering).
class Button {
public:
    /// Primary immediate mode entry point for Button widget.
    static Response Run(
        Context& ctx,
        const ButtonOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on content (icon + label + padding for current UiSize).
    static Vec2 Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas, const UiTheme& theme, float availableWidth = 0.0f);
    static Vec2 Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas = nullptr, float availableWidth = 0.0f);

    /// Renders the button into its allocated bounds according to interaction and disabled state.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        const ButtonOptions& opts,
        UiSize size,
        bool hovered,
        bool pressed,
        bool disabled,
        const UiTheme& theme,
        bool focused = false
    );
};

} // namespace guikit
