#pragma once

#include "guikit/context.hpp"
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
    UiSize size{UiSize::Md};                        ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                               ///< Horizontal sizing policy (`Auto` -> `Fit`, `Stretch`, or explicit `dp`)
    AxisSize height{};                              ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or explicit `dp`)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt}; ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt}; ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};///< Optional maximum height in logical dp
    StateColor color{};                             ///< Per-state semantic fill/accent color override
    StateColor background{};                        ///< Optional background fill override (`{}` = theme/variant default, `StateColor::None()` = transparent)
    Border border{};                                ///< Optional border width, color, and radius override
};

/// Type alias for button response.
using ButtonResponse = Response;

/// Autonomous Button widget logic (execution, measuring, and rendering).
namespace detail {

class Button {
public:
    /// Primary immediate mode entry point for Button widget.
    static Response Run(
        Context& ctx,
        const ButtonOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions based on content (icon + label + padding for current UiSize).
    static Vec2 Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas, const UiTheme& theme, keel::f32 availableWidth = 0.0f);
    static Vec2 Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas = nullptr, keel::f32 availableWidth = 0.0f);

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
        keel::f32 loadingPhase = 0.0f
    );
};

} // namespace detail

inline Response Button(
    Context& ui,
    ButtonOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::Button::Run(ui, opts, loc);
}

} // namespace guikit
