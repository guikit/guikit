#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"

namespace guikit {

class Context;
class ICanvas;

namespace detail {
struct MenuTreeFrameSession;
} // namespace detail

/// Single entry inside a declarative `ContextMenu` (or `MenuBar`) popup.
struct ContextMenuItem {
    UiText label{};
    UiText shortcut{};                         ///< Right-aligned keyboard shortcut hint (e.g. "Ctrl+C")
    Icon icon{};                               ///< Optional leading icon
    UiVariant variant{UiVariant::Default};     ///< Semantic variant (e.g. UiVariant::Danger for "Delete")
    bool separatorAfter{false};                ///< Draw a 1px horizontal divider below this item
    bool checked{false};                       ///< Checkmark / radio dot state for toggleable items
    bool hasCheck{false};                      ///< Show/reserve leading checkmark or radio slot
    bool isRadio{false};                       ///< Draw a filled radio dot instead of a checkmark when `checked`
    bool disabled{false};
    bool isSeparatorOnly{false};               ///< Pure horizontal divider line
    bool isSectionHeader{false};               ///< Non-interactive group heading inside the menu
    bool keepOpenOnClick{false};               ///< Keep menu open after activating this item (useful for toggles)
    keel::i32 id{-1};                          ///< Optional command ID returned via `ContextMenuResponse::clickedId`
    keel::ArrayRef<ContextMenuItem> children{};///< Optional cascading submenu entries

    [[nodiscard]] static constexpr ContextMenuItem Separator() noexcept {
        ContextMenuItem item{};
        item.isSeparatorOnly = true;
        return item;
    }

    [[nodiscard]] static constexpr ContextMenuItem Section(UiText title) noexcept {
        ContextMenuItem item{};
        item.label = title;
        item.isSectionHeader = true;
        return item;
    }
};

/// Ergonomic options for items added through `MenuScope::Item`, `Checkbox`, `Radio`, or `SubMenu`.
struct MenuItemOptions {
    UiText shortcut{};                     ///< Right-aligned keyboard shortcut hint (e.g. "Ctrl+S")
    Icon icon{};                           ///< Optional leading icon
    UiVariant variant{UiVariant::Default}; ///< Semantic variant (e.g. UiVariant::Danger)
    bool checked{false};                   ///< Checked state (set automatically by Checkbox/Radio)
    bool hasCheck{false};                  ///< Reserve leading check/radio slot
    bool isRadio{false};                   ///< Render filled radio dot instead of checkmark when `checked`
    bool disabled{false};                  ///< Dimmed non-interactive entry
    bool keepOpen{false};                  ///< Keep menu open after clicking this entry
};

/// Configuration options for a `ContextMenu` / dropdown menu popup.
struct ContextMenuOptions {
    UiText title{};                                     ///< Optional header title rendered above items with a subtle divider
    Rect anchorRect{};                                  ///< Anchor bounds (e.g. button or MenuBar rect) for dropdown placement
    keel::Optional<Vec2> anchorPos{keel::kNullOpt};     ///< Explicit anchor coordinate (e.g. cursor position on RightClick)
    keel::Optional<keel::f32> minWidth{188.0f};
    keel::Optional<keel::f32> width{keel::kNullOpt};
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};    ///< Optional max height before internal wheel scrolling kicks in
    StateColor background{};
    Border border{.width = 1.0f, .radius = 8.0f};
    UiSize size{UiSize::Md};
    bool disabled{false};
    bool autoFocusFirst{false};                         ///< Automatically highlight first enabled item when freshly opened
};

/// Extended response returned by `ui.ContextMenu(...)`.
struct ContextMenuResponse : public Response {
    keel::i32 clickedIndex{-1};         ///< Root-level index of the activated item (-1 if none or submenu leaf)
    keel::i32 clickedId{-1};            ///< `ContextMenuItem::id` of the activated item at any depth (-1 if unset/none)
    bool isOpen{false};                 ///< True while the menu popup is open
    keel::i32 horizontalNavRequest{0};  ///< -1 (Left) or +1 (Right) menu-bar switch request from keyboard navigation
};

/// RAII scope guard for building declarative-in-code menus and arbitrary-depth cascading submenus.
/// Supports `Item`, `Checkbox`, `Radio`, `SubMenu`, `Section`, and `Separator`.
class [[nodiscard]] MenuScope {
public:
    MenuScope() noexcept = default;
    ~MenuScope() noexcept;

    MenuScope(const MenuScope&) = delete;
    MenuScope& operator=(const MenuScope&) = delete;

    MenuScope(MenuScope&& other) noexcept;
    MenuScope& operator=(MenuScope&& other) noexcept;

    /// Evaluates to true when this menu panel (or submenu) is currently open.
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return m_session != nullptr && m_active;
    }

    /// Adds a clickable command entry. Returns a `Response` (`if (menu.Item("Save", {.shortcut = "Ctrl+S"}))`).
    Response Item(
        UiText label,
        const MenuItemOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Adds a toggleable checkbox entry bound to `value`, flipping `value` on click.
    Response Checkbox(
        UiText label,
        bool& value,
        MenuItemOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Adds a mutually exclusive radio entry bound to `currentValue`.
    template <typename T>
    Response Radio(
        UiText label,
        T& currentValue,
        T optionValue,
        MenuItemOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        opts.hasCheck = true;
        opts.isRadio = true;
        opts.checked = (currentValue == optionValue);
        Response r = Item(label, opts, loc);
        if (r.clicked && currentValue != optionValue) {
            currentValue = optionValue;
            r.changed = true;
        }
        return r;
    }

    /// Adds a cascading submenu entry (`▸`). Returns a truthy child `MenuScope` only while expanded.
    [[nodiscard]] MenuScope SubMenu(
        UiText label,
        const MenuItemOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Adds a non-interactive section heading inside the menu.
    void Section(
        UiText label,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Adds a 1px horizontal divider line (duplicate/leading/trailing dividers are automatically collapsed).
    void Separator(keel::SourceLocation loc = keel::SourceLocation::current());

    /// Imperatively closes the entire menu tree.
    void Close() noexcept;

    /// Returns -1 (Left) or +1 (Right) if the user requested switching to an adjacent MenuBar menu via keyboard.
    [[nodiscard]] keel::i32 GetHorizontalNavRequest() const noexcept;

    /// True if any item in this menu tree was activated on the current frame.
    [[nodiscard]] bool AnyActivated() const noexcept;

private:
    friend class ContextMenu;
    friend class MenuBar;
    friend class MenuBarScope;

    MenuScope(detail::MenuTreeFrameSession* session, keel::u32 panelIdx, bool isRoot, bool active) noexcept
        : m_session(session), m_panelIdx(panelIdx), m_isRoot(isRoot), m_active(active) {}

    void FinalizeIfNeeded() noexcept;

    detail::MenuTreeFrameSession* m_session{nullptr};
    keel::u32 m_panelIdx{0};
    bool m_isRoot{false};
    bool m_active{false};
};

/// Floating ContextMenu / Cascading Dropdown Menu component.
class ContextMenu {
public:
    /// Opens an immediate-mode RAII `MenuScope` supporting `Item`, `Checkbox`, `Radio`, `SubMenu`, `Section`, `Separator`.
    [[nodiscard]] static MenuScope Open(
        Context& ctx,
        bool& open,
        const ContextMenuOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Opens an RAII `MenuScope` with an explicit root ID (used internally by `MenuBar`).
    [[nodiscard]] static MenuScope OpenWithId(
        Context& ctx,
        Id rootId,
        bool& open,
        const ContextMenuOptions& opts = {},
        keel::i32* outHorizontalNav = nullptr
    );

    /// Resets cached open/hover state for a menu root ID when closed externally (e.g. MenuBar hover-scrub switch).
    static void ResetState(Context& ctx, Id rootId) noexcept;

    /// Declarative array-based ContextMenu (supports recursive `ContextMenuItem::children` submenus & `Section`).
    static ContextMenuResponse Run(
        Context& ctx,
        bool& open,
        keel::ArrayRef<ContextMenuItem> items,
        const ContextMenuOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace guikit
