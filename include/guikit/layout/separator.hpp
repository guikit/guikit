#pragma once

#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

class Context;
class ICanvas;

/// Autonomous immediate-mode Separator layout primitive and line renderer.
class Separator {
public:
    /// Creates a Separator layout node that adapts to the current container orientation
    /// (horizontal line in vertical containers, vertical line in horizontal containers),
    /// or uses `lineOrientationOverride` (`Orientation::Horizontal` = horizontal rule,
    /// `Orientation::Vertical` = vertical rule) when explicitly specified.
    static void Run(
        Context& ctx,
        keel::f32 extentRatio = 0.0f,
        Color colorOverride = Color::Transparent(),
        keel::SourceLocation loc = keel::SourceLocation::current(),
        keel::Optional<Orientation> lineOrientationOverride = keel::kNullOpt
    );

    /// Renders a crisp, half-pixel-aligned separator line between `p0` and `p1`.
    static void RenderLine(
        ICanvas& canvas,
        Vec2 p0,
        Vec2 p1,
        Color color,
        keel::f32 thickness = 1.0f
    ) noexcept;

    /// Renders a centered separator line inside `bounds` for the given `orientation`.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        Color color,
        const UiTheme& theme,
        Orientation orientation = Orientation::Horizontal,
        keel::f32 insetRatio = 0.20f
    ) noexcept;
};

} // namespace guikit
