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

/// Clean configuration options for Switch widget.
struct SwitchOptions {
    std::string_view label{};                      ///< Primary caption text (left/off label when Placement::Both)
    std::string_view trailingLabel{};              ///< Secondary caption text (right/on label when Placement::Both)
    std::string_view description{};                ///< Optional explanation text below label
    UiVariant variant{UiVariant::Primary};          ///< Accent fill color when switched ON
    Placement labelPlacement{Placement::Trailing}; ///< Placement of label relative to the switch track
    bool symmetric{false};                         ///< Neutral/symmetric track style (highlights active label)
    bool disabled{false};                          ///< Interaction suppression
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
};
static_assert(sizeof(SwitchOptions) <= 64, "SwitchOptions must stay compact");

/// Autonomous Switch widget logic (execution, measuring, and rendering).
class Switch {
public:
    /// Primary immediate mode entry point for Switch widget with state pointer and options.
    static Response Run(
        Context& ctx,
        bool* value,
        bool checkedState,
        const SwitchOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on track size, gap, and label text.
    static Vec2 Measure(
        const SwitchOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    );
    static Vec2 Measure(
        const SwitchOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Renders the switch track, knob, and optional label text.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        bool checked,
        const SwitchOptions& opts,
        UiSize size,
        bool hovered,
        bool pressed,
        bool disabled,
        const UiTheme& theme
    );
};

} // namespace guikit
