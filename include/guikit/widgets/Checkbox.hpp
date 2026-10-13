#pragma once

#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for Checkbox widget.
struct CheckboxOptions {
    std::string_view label{};                      ///< Primary caption text
    std::string_view description{};                ///< Optional explanation text below label
    UiVariant variant{UiVariant::Primary};          ///< Accent fill color when checked
    FormLayout layout{FormLayout::Inline};         ///< Layout geometry: Stacked, Top, Bottom, Inline
    ElementOrder placement{ElementOrder::Trailing};///< Sequencing order: Leading (Text first), Trailing (Control first)
    Align labelAlign{Align::Start};                ///< Label and description text alignment (Start, Center, End)
    StretchMode stretch{StretchMode::Compact};     ///< Sizing behavior: Compact, Between, Fill
    bool disabled{false};                          ///< Interaction suppression
    bool indeterminate{false};                     ///< Tri-state / partial check (horizontal dash)
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
};
static_assert(sizeof(CheckboxOptions) <= 48, "CheckboxOptions must stay compact");

/// Autonomous Checkbox widget logic (execution, measuring, and rendering).
class Checkbox {
public:
    /// Primary immediate mode entry point for Checkbox widget with state pointer and options.
    static Response Run(
        Context& ctx,
        bool* value,
        bool checkedState,
        const CheckboxOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on box size, gap, and label text.
    static Vec2 Measure(
        const CheckboxOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    );
    static Vec2 Measure(
        const CheckboxOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Renders the checkbox square, checkmark/dash, and optional label text.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        bool checked,
        const CheckboxOptions& opts,
        UiSize size,
        bool hovered,
        bool pressed,
        bool disabled,
        const UiTheme& theme
    );
};

} // namespace guikit
