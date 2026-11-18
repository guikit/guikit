#pragma once

#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for Radio widget.
struct RadioOptions {
    UiText tooltip{};     ///< Optional floating tooltip text on hover
    bool disabled{false}; ///< Interaction suppression
    ControlStyle style{}; ///< Component size scale and accent color
};
static_assert(sizeof(RadioOptions) <= 384, "RadioOptions must stay compact");

/// Autonomous Radio widget logic (execution, measuring, and rendering).
class Radio {
public:
    /// Primary immediate mode entry point for Radio widget with state pointer and options.
    static Response Run(
        Context& ctx,
        bool* value,
        bool selectedState,
        const RadioOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions based on radio indicator size and control spec.
    static Vec2 Measure(
        const RadioOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const RadioOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    /// Renders the circular radio ring and inner selection indicator dot.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        bool selected,
        const RadioOptions& opts,
        UiSize size,
        bool hovered,
        bool pressed,
        bool disabled,
        const UiTheme& theme,
        bool focused = false
    );
};

} // namespace guikit
