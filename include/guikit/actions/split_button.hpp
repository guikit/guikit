#pragma once

#include "guikit/actions/button.hpp"
#include "guikit/overlays/context_menu.hpp"
#include "keel/core.h"
#include "keel/ref.h"

namespace guikit {

class Context;

/// Extended response returned by standalone `SplitButton` as well as `Toolbar::SplitButton` and `SplitToggle`.
struct SplitButtonResponse : public Response {
    keel::i32 selectedIndex{-1};  ///< Selected item index (or clicked root menu item index)
    keel::i32 clickedId{-1};      ///< `ContextMenuItem::id` at any submenu depth (-1 if unset/none)
    bool toggled{false};          ///< True when the primary action/toggle zone was triggered this frame
    bool selectionChanged{false}; ///< True when a dropdown menu item was activated this frame
    bool isOpen{false};           ///< True while the dropdown menu is open
};

/// Dual-zone action button: left zone triggers the primary action, right chevron zone opens a dropdown menu.
namespace detail {

class SplitButton {
public:
    /// Declarative array-based SplitButton returning primary click + menu selection state.
    static SplitButtonResponse Run(
        Context& ctx,
        keel::ArrayRef<ContextMenuItem> items,
        ButtonOptions opts,
        ContextMenuOptions menuOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Immediate-mode RAII `MenuScope` SplitButton: sets `primaryClicked` when the left zone is activated.
    [[nodiscard]] static MenuScope Open(
        Context& ctx,
        bool& primaryClicked,
        ButtonOptions opts,
        ContextMenuOptions menuOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace detail

/// Renders a standalone SplitButton backed by a declarative `ContextMenuItem` array.
/// Check `res.clicked` (or `res.toggled`) for the primary action and `res.selectionChanged` / `res.selectedIndex` for menu items.
inline SplitButtonResponse SplitButton(
    Context& ui,
    keel::ArrayRef<ContextMenuItem> items,
    ButtonOptions opts,
    ContextMenuOptions menuOpts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::SplitButton::Run(ui, items, keel::move(opts), keel::move(menuOpts), loc);
}

/// Renders a standalone SplitButton with an immediate-mode RAII `MenuScope`.
/// Sets `primaryClicked = true` when the primary left zone is activated; enters the `if` body while the dropdown menu is open.
[[nodiscard]] inline MenuScope SplitButton(
    Context& ui,
    bool& primaryClicked,
    ButtonOptions opts,
    ContextMenuOptions menuOpts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::SplitButton::Open(ui, primaryClicked, keel::move(opts), keel::move(menuOpts), loc);
}

} // namespace guikit
