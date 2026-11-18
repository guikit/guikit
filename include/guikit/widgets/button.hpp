#pragma once

#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for a Button widget.
struct ButtonOptions {
    UiText text{};                                  ///< Text inside the button capsule
    UiText badge{};                                 ///< Optional pill badge inside the button capsule
    Icon icon{};                                    ///< Icon inside the button capsule
    Placement iconPlacement{Placement::Leading};
    Align align{Align::Center};                     ///< Content horizontal alignment inside button capsule (Start, Center, End)
    bool active{false};
    bool loading{false};                            ///< Renders an animated vector spinner and suppresses click activation
    bool square{false};                             ///< Force square (defaultHeight x defaultHeight) button geometry
    keel::Optional<Color> iconColor{keel::kNullOpt}; ///< Optional custom icon tint override
    UiText tooltip{};                               ///< Optional floating tooltip text on hover
    bool disabled{false};
    ControlLayout layout{};                         ///< Sizing policy (`Fit` default) and min/max constraints
    SurfaceStyle style{};                           ///< Control size, semantic color, background surface, and border
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
        keel::SourceLocation loc = keel::SourceLocation::current()
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
        bool focused = false,
        float loadingPhase = 0.0f
    );
};

} // namespace guikit
