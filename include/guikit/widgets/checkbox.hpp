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

/// Clean configuration options for Checkbox widget.
struct CheckboxOptions {
    bool indeterminate{false}; ///< Tri-state / partial check (horizontal dash)
    UiText tooltip{};          ///< Optional floating tooltip text on hover
    bool disabled{false};      ///< Interaction suppression
    UiSize size{UiSize::Md};   ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    StateColor color{};        ///< Per-state semantic accent color override
};
static_assert(sizeof(CheckboxOptions) <= 384, "CheckboxOptions must stay compact");

/// Autonomous Checkbox widget logic (execution, measuring, and rendering).
class Checkbox {
public:
    /// Primary immediate mode entry point for Checkbox widget with state pointer and options.
    static Response Run(
        Context& ctx,
        bool* value,
        bool checkedState,
        const CheckboxOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions based on box size, gap, and label text.
    static Vec2 Measure(
        const CheckboxOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const CheckboxOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
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
        const UiTheme& theme,
        bool focused = false
    );
};

} // namespace guikit
