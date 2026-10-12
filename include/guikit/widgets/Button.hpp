#pragma once

#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for a Button widget.
struct ButtonOptions {
    std::string_view label{};
    Icon icon{};
    UiVariant variant{UiVariant::Secondary};
    Placement iconPlacement{Placement::Leading};
    Align align{Align::Center};   ///< Content horizontal alignment (Start = left, Center, End = right)
    LabelVariants labelVariants{};
    bool stretch{false};           ///< Expand to fill available horizontal space in flex containers
    bool disabled{false};
    bool active{false};
    std::optional<UiSize> size{std::nullopt}; ///< Local size override (nullopt = inherit container size)
    std::optional<float> radius{std::nullopt}; ///< Local corner radius override (nullopt = inherit from size metrics)
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
    static Vec2 Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas, const UiTheme& theme);
    static Vec2 Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas = nullptr);

    /// Renders the button into its allocated bounds according to interaction and disabled state.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        const ButtonOptions& opts,
        UiSize size,
        bool hovered,
        bool pressed,
        bool disabled,
        const UiTheme& theme
    );
};

} // namespace guikit
