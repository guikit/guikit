#pragma once

#include "id.hpp"
#include "types.hpp"

namespace guikit {

class Context;

/// Unified interaction trigger selector for widgets and containers.
enum class InteractionTrigger : keel::u8 {
    Click = 0,   ///< Left click (or keyboard activation on focused controls)
    RightClick,  ///< Right mouse button click
    DoubleClick  ///< Double-click with primary mouse button
};

/// Represents interaction feedback and computed layout bounds for any widget.
struct Response {
    Id id{Id::Null()};
    Rect rect{};

    // Mouse and pointer interaction states
    bool clicked{false};       ///< Left button clicked during this frame
    bool rightClicked{false};  ///< Right button clicked during this frame
    bool doubleClicked{false}; ///< Double clicked
    bool hovered{false};       ///< Pointer is hovering over widget bounds
    bool pressed{false};       ///< Active / pointer held down during this frame
    bool released{false};      ///< Pointer released inside the widget bounds
    bool focused{false};       ///< Widget has keyboard / input focus
    bool changed{false};       ///< Value changed (inputs, sliders, toggles)
    bool submitted{false};     ///< Value submitted / Enter pressed (inputs)

    Context* ctx{nullptr};     ///< Pointer to context for imperative actions

    /// Implicit conversion to bool for idiomatic `if (ui.Button(...))` usage.
    [[nodiscard]] constexpr operator bool() const noexcept {
        return clicked || changed || submitted;
    }

    /// Returns true if the specified interaction trigger fired on this frame.
    [[nodiscard]] constexpr bool IsTriggered(InteractionTrigger trigger) const noexcept {
        switch (trigger) {
            case InteractionTrigger::Click:       return clicked;
            case InteractionTrigger::RightClick:  return rightClicked;
            case InteractionTrigger::DoubleClick: return doubleClicked;
        }
        return false;
    }

    /// Imperatively requests keyboard focus for this widget.
    void Focus();

    /// Imperatively requests the enclosing ScrollArea to bring this widget into visible view.
    void ScrollTo();
};

/// Type alias for backward compatibility.
using WidgetResponse = Response;

} // namespace guikit
