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
    UiText label{};                                ///< Primary external caption text (or fallback capsule text if text & description are empty)
    UiText text{};                                 ///< Text inside the button capsule
    UiText description{};                          ///< Optional external helper text below/beside button
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    Icon icon{};                                   ///< Icon inside the button capsule
    Icon labelIcon{};                              ///< Optional leading icon in external Element label bar
    UiText badge{};                                ///< Optional trailing pill badge in external Element label bar
    UiVariant variant{UiVariant::Secondary};
    Placement iconPlacement{Placement::Leading};
    ElementLayout layout{ElementLayout::Stacked};     ///< Layout geometry: Stacked, Top, Bottom, Inline
    InlineOrder inlineOrder{InlineOrder::LabelFirst}; ///< Sequencing order in Inline layout: LabelFirst, ControlFirst
    Align elementAlign{Align::Start};                 ///< Element composition and label alignment (Start, Center, End)
    Align controlAlign{Align::Center};                ///< Content horizontal alignment inside button capsule (Start, Center, End)
    StretchMode stretch{StretchMode::Compact};        ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Capsule width: in points for Compact/Space (nullopt = auto), or % of bounds (0..100) for Fill (nullopt = 100%)
    std::optional<float> height{std::nullopt};     ///< Explicit capsule height in points (nullopt = inherit defaultHeight from UiSize)
    std::optional<float> minWidth{std::nullopt};   ///< Minimum capsule width constraint in points
    bool background{false};                        ///< Whether to render a surface backdrop around the outer Element
    float borderWidth{0.0f};                       ///< Outer Element border thickness in points (0.0f = no border)
    std::optional<float> borderRadius{std::nullopt}; ///< Outer Element corner radius in points
    StateColor borderColor{};                      ///< Per-state outer Element border color (normal, hover, active, disabled)
    StateColor bgColor{};                          ///< Per-state outer Element background color (normal, hover, active, disabled)
    ControlSpec control{};                         ///< Inner Control slot configuration (width, height, minWidth, align, background, borderWidth, borderRadius, borderColor, bgColor)
    bool disabled{false};
    bool active{false};
    bool square{false};                            ///< Force square (defaultHeight x defaultHeight) button geometry
    std::optional<Color> iconColor{std::nullopt};  ///< Optional custom icon tint override
    std::optional<UiSize> size{std::nullopt}; ///< Local size override (nullopt = inherit container size)
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
