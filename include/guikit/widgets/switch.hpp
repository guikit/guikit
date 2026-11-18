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

/// Clean configuration options for Switch widget.
struct SwitchOptions {
    UiText textLeft{};                             ///< Optional companion text inside control to the left of the switch track (off state)
    UiText textRight{};                            ///< Optional companion text inside control to the right of the switch track (on state)
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    StateColor color{};                            ///< Per-state accent fill color when switched ON
    UiSize size{UiSize::Md};                       ///< Component size scale
    bool symmetric{false};                         ///< Neutral/symmetric track style (highlights active side label)
    bool disabled{false};                          ///< Interaction suppression
};
static_assert(sizeof(SwitchOptions) <= 512, "SwitchOptions must stay compact");

/// Autonomous Switch widget logic (execution, measuring, and rendering).
class Switch {
public:
    /// Primary immediate mode entry point for Switch widget with state pointer and options.
    static Response Run(
        Context& ctx,
        bool* value,
        bool checkedState,
        const SwitchOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions based on track size, gap, and label text.
    static Vec2 Measure(
        const SwitchOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const SwitchOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
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
        const UiTheme& theme,
        bool focused = false
    );
};

} // namespace guikit
