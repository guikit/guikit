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
    std::string_view label{};                      ///< Primary external caption text (or fallback capsule text if text & description are empty)
    std::string_view text{};                       ///< Text inside the button capsule
    std::string_view description{};                ///< Optional external helper text below/beside button
    Icon icon{};
    UiVariant variant{UiVariant::Secondary};
    Placement iconPlacement{Placement::Leading};
    ElementLayout layout{ElementLayout::Stacked};    ///< Layout geometry: Stacked, Top, Bottom, Inline
    InlineOrder inlineOrder{InlineOrder::LabelFirst}; ///< Sequencing order in Inline layout: LabelFirst, ControlFirst
    Align labelAlign{Align::Start};                ///< Label and description text alignment (Start, Center, End)
    Align align{Align::Center};   ///< Content horizontal alignment inside button capsule (Start = left, Center, End = right)
    LabelVariants labelVariants{};
    StretchMode stretch{StretchMode::Compact};     ///< Sizing behavior: Compact, Between (space-between), Fill
    std::optional<float> width{std::nullopt};      ///< Explicit capsule width in points (nullopt = auto from content)
    std::optional<float> minWidth{std::nullopt};   ///< Minimum capsule width constraint in points
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
