#pragma once

#include "guikit/context.hpp"
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
    UiText textLeft{};    ///< Optional companion text inside control to the left of the switch track (off state)
    UiText textRight{};   ///< Optional companion text inside control to the right of the switch track (on state)
    bool symmetric{false};///< Neutral/symmetric track style (highlights active side label)
    UiText tooltip{};     ///< Optional floating tooltip text on hover
    bool disabled{false};     ///< Interaction suppression
    UiSize size{UiSize::Md};  ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    StateColor color{};       ///< Per-state semantic accent color override
};
static_assert(sizeof(SwitchOptions) <= 512, "SwitchOptions must stay compact");

/// Autonomous Switch widget logic (execution, measuring, and rendering).
namespace detail {

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
        keel::f32 availableWidth = 0.0f
    );
    static Vec2 Measure(
        const SwitchOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        keel::f32 availableWidth = 0.0f
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

} // namespace detail

inline Response Switch(
    Context& ui,
    bool& value,
    SwitchOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::Switch::Run(ui, &value, value, opts, loc);
}

inline Response Switch(
    Context& ui,
    const bool& checked,
    SwitchOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::Switch::Run(ui, nullptr, checked, opts, loc);
}

Response Switch(Context& ui, const bool*, SwitchOptions = {}, keel::SourceLocation = keel::SourceLocation::current()) = delete;

} // namespace guikit
