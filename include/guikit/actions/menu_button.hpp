#pragma once

#include "guikit/actions/button.hpp"
#include "guikit/overlays/context_menu.hpp"
#include "keel/core.h"
#include "keel/ref.h"

namespace guikit {

class Context;

/// Action trigger button that owns an autonomous dropdown menu (`MenuScope` or `ContextMenuItem` array).
namespace detail {

class MenuButton {
public:
    /// Opens an immediate-mode RAII `MenuScope` anchored to the button trigger.
    [[nodiscard]] static MenuScope Open(
        Context& ctx,
        ButtonOptions opts,
        ContextMenuOptions menuOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Renders a declarative array-based dropdown menu anchored to the button trigger.
    static ContextMenuResponse Run(
        Context& ctx,
        keel::ArrayRef<ContextMenuItem> items,
        ButtonOptions opts,
        ContextMenuOptions menuOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace detail

/// Renders a push button that toggles an attached RAII `MenuScope` dropdown menu.
/// Open state is tracked autonomously by widget ID; no external `bool` is required.
[[nodiscard]] inline MenuScope MenuButton(
    Context& ui,
    ButtonOptions opts,
    ContextMenuOptions menuOpts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::MenuButton::Open(ui, keel::move(opts), keel::move(menuOpts), loc);
}

/// Renders a push button that toggles a declarative `ContextMenuItem` dropdown menu.
/// Open state is tracked autonomously by widget ID; no external `bool` is required.
inline ContextMenuResponse MenuButton(
    Context& ui,
    keel::ArrayRef<ContextMenuItem> items,
    ButtonOptions opts,
    ContextMenuOptions menuOpts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::MenuButton::Run(ui, items, keel::move(opts), keel::move(menuOpts), loc);
}

} // namespace guikit
