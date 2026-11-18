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
    UiText text{};                                 ///< Text inside the button capsule
    UiText badge{};                                ///< Optional pill badge inside the button capsule
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    Icon icon{};                                   ///< Icon inside the button capsule
    StateColor color{};                            ///< Per-state capsule fill color (normal, hover, active, disabled)
    Placement iconPlacement{Placement::Leading};
    Align align{Align::Center};                    ///< Content horizontal alignment inside button capsule (Start, Center, End)
    AxisSize width{};                              ///< Horizontal sizing policy (`Auto` -> `Fit`, `Stretch`, or `Fixed(dp)`)
    AxisSize height{};                             ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or `Fixed(dp)`)
    keel::Optional<float> minWidth{keel::kNullOpt};   ///< Minimum capsule width constraint in points
    StateColor background{StateColor::Surface()};  ///< Capsule surface fill configuration (Surface/Default = on, None = off, or custom StateColor)
    Border border{};                               ///< Capsule border stroke & corner radius configuration
    bool disabled{false};
    bool active{false};
    bool loading{false};                           ///< Renders an animated vector spinner and suppresses click activation
    bool square{false};                            ///< Force square (defaultHeight x defaultHeight) button geometry
    UiSize size{UiSize::Md};                       ///< Component size scale (preset, dp target height, or factor)
    keel::Optional<Color> iconColor{keel::kNullOpt};  ///< Optional custom icon tint override
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
