#pragma once

#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for Radio widget.
struct RadioOptions {
    UiText tooltip{};     ///< Optional floating tooltip text on hover
    bool disabled{false};     ///< Interaction suppression
    UiSize size{UiSize::Md};  ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    StateColor color{};       ///< Per-state semantic accent color override
};
static_assert(sizeof(RadioOptions) <= 384, "RadioOptions must stay compact");

/// Autonomous Radio widget logic (execution, measuring, and rendering).
namespace detail {

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
        keel::f32 availableWidth = 0.0f
    );
    static Vec2 Measure(
        const RadioOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        keel::f32 availableWidth = 0.0f
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

} // namespace detail

template <typename T, typename U = T>
    requires requires(const T& a, const U& b) { static_cast<bool>(a == b); }
Response Radio(
    Context& ui,
    T& currentValue,
    const U& optionValue,
    RadioOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    const bool isSelected = static_cast<bool>(currentValue == optionValue);
    bool state = isSelected;
    Response r = detail::Radio::Run(ui, &state, isSelected, opts, loc);
    if (r.clicked && !isSelected) {
        currentValue = static_cast<T>(optionValue);
        r.changed = true;
    } else {
        r.changed = false;
    }
    return r;
}

inline Response Radio(
    Context& ui,
    bool& value,
    RadioOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::Radio::Run(ui, &value, value, opts, loc);
}

inline Response Radio(
    Context& ui,
    const bool& checked,
    RadioOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::Radio::Run(ui, nullptr, checked, opts, loc);
}

Response Radio(Context& ui, const bool*, RadioOptions = {}, keel::SourceLocation = keel::SourceLocation::current()) = delete;

template <typename T, typename U = T>
    requires requires(const T& a, const U& b) { static_cast<bool>(a == b); }
Response RadioButton(
    Context& ui,
    T& currentValue,
    const U& optionValue,
    RadioOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return guikit::Radio(ui, currentValue, optionValue, keel::move(opts), loc);
}

inline Response RadioButton(
    Context& ui,
    bool& checked,
    RadioOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return guikit::Radio(ui, checked, keel::move(opts), loc);
}

} // namespace guikit
