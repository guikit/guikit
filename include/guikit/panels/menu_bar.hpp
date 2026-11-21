#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/panels/context_menu.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Visual surface style for a desktop `MenuBar`.
enum class MenuBarStyle : keel::u8 {
    Header = 0, ///< Flush edge-to-edge top strip with 1px bottom divider (classic IDE/desktop window bar)
    Surface,    ///< Rounded surface bar with 1px border (for embedding inside panels/cards)
    Compact     ///< Ultra-compact flush strip (tighter vertical height)
};

/// Configuration options for `ui.MenuBar(...)`.
struct MenuBarOptions {
    MenuBarStyle style{MenuBarStyle::Header};
    UiSize size{UiSize::Sm};
    keel::Optional<keel::f32> gap{2.0f};
    keel::Optional<Padding> padding{keel::kNullOpt};
    StateColor background{};
    Border border{};
    bool disabled{false};
};

/// RAII scope returned by `ui.MenuBar(...)`.
/// Hosts top-level menus with hover-scrubbing, keyboard Left/Right switching, `Spacer`, and `Separator`.
class [[nodiscard]] MenuBarScope : public ContainerScope {
public:
    Response response{};

    MenuBarScope() noexcept : ContainerScope(nullptr, nullptr) {}
    ~MenuBarScope();

    MenuBarScope(const MenuBarScope&) = delete;
    MenuBarScope& operator=(const MenuBarScope&) = delete;

    MenuBarScope(MenuBarScope&& other) noexcept;
    MenuBarScope& operator=(MenuBarScope&& other) noexcept;

    /// Adds a top-level menu trigger and returns an RAII `MenuScope` (`if (auto file = mb.Menu("File")) { ... }`).
    [[nodiscard]] MenuScope Menu(
        UiText label,
        ContextMenuOptions menuOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Adds a top-level menu trigger backed by a declarative `ContextMenuItem` array (supporting `children` submenus).
    ContextMenuResponse Menu(
        UiText label,
        keel::ArrayRef<ContextMenuItem> items,
        ContextMenuOptions menuOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Adds an elastic spacer pushing subsequent widgets/badges to the trailing edge of the MenuBar.
    void Spacer(keel::SourceLocation loc = keel::SourceLocation::current());

    /// Adds a subtle vertical divider line between menu groups or trailing widgets.
    void Separator(keel::f32 heightRatio = 0.55f, keel::SourceLocation loc = keel::SourceLocation::current());

    /// Returns true if any menu in this MenuBar is currently expanded.
    [[nodiscard]] bool IsAnyMenuOpen() const noexcept;

    /// Imperatively closes any currently open menu in this MenuBar.
    void CloseMenu() noexcept;

private:
    friend class MenuBar;

    MenuBarScope(Context* ctx, Node* node, Id barId, const MenuBarOptions& opts, Response res) noexcept;
    void FinishBarIfNeeded() noexcept;

    struct TriggerResult {
        Id popupId{Id::Null()};
        Rect triggerRect{};
        bool isOpen{false};
    };
    TriggerResult RunMenuTrigger(UiText label, bool itemDisabled, keel::SourceLocation loc);

    Id m_barId{Id::Null()};
    MenuBarOptions m_opts{};
    keel::i32 m_menuCount{0};
    keel::i32 m_horizNavRequest{0};
    keel::i32 m_activeMenuIdx{-1};
    bool m_hadOpenMenuAtStart{false};
    bool m_triggerClicked{false};
    bool m_openedScopeThisFrame{false};
    bool m_openSlotClosedByChild{false};
    bool m_activeBar{false};
};

/// Autonomous desktop MenuBar container.
class MenuBar {
public:
    [[nodiscard]] static MenuBarScope Open(
        Context& ctx,
        MenuBarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace guikit
