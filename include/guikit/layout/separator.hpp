#pragma once

#include <source_location>
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Autonomous immediate-mode Separator layout primitive and line renderer.
class Separator {
public:
    /// Creates a Separator layout node that adapts to the current container orientation
    /// (horizontal line in vertical containers, vertical line in horizontal containers).
    static void Run(
        Context& ctx,
        float extentRatio = 0.0f,
        Color colorOverride = Color::Transparent(),
        std::source_location loc = std::source_location::current()
    );

    /// Renders a crisp, half-pixel-aligned separator line between `p0` and `p1`.
    static void RenderLine(
        ICanvas& canvas,
        Vec2 p0,
        Vec2 p1,
        Color color,
        float thickness = 1.0f
    ) noexcept;

    /// Renders a centered separator line inside `bounds` for the given `orientation`.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        Color color,
        const UiTheme& theme,
        Orientation orientation = Orientation::Horizontal,
        float insetRatio = 0.20f
    ) noexcept;
};

} // namespace guikit
