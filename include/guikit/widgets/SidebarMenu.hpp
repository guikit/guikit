#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <source_location>
#include <string_view>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Visual presentation style for SidebarMenu items.
enum class SidebarMenuStyle : uint8_t {
    Pill,       ///< Full rounded capsule highlight for the active item
    Indicator   ///< Subtle tinted background with a vertical accent bar on the left edge
};

/// Definition of an individual navigation item in a SidebarMenu.
struct SidebarMenuItem {
    UiText text{};                                      ///< Item label text (default role: LabelLarge)
    Icon icon{};                                        ///< Optional leading icon
    UiText badge{};                                     ///< Optional trailing badge/counter (default role: LabelSmall)
    UiText section{};                                   ///< Optional section heading above this item (used in span overload)
    UiText tooltip{};                                   ///< Optional floating tooltip (defaults to item.text when menu is collapsed)
    StateColor color{};                                 ///< Optional per-item highlight color override (empty = inherit menu color)
    bool active{false};                                 ///< Whether this item is currently selected/active
    bool disabled{false};
};

/// Configuration options for SidebarMenu widget.
struct SidebarMenuOptions {
    SidebarMenuStyle style{SidebarMenuStyle::Pill};
    bool collapsed{false};                          ///< Compact icon-only rail mode (hides text, badges, and section titles)
    StateColor color{};                             ///< Default highlight color for active items (Default = theme Primary)
    std::optional<float> gap{4.0f};                 ///< Vertical gap between consecutive menu items
    float sectionGap{8.0f};                         ///< Extra top spacing before a non-first Section header
    std::optional<float> width{std::nullopt};       ///< Optional explicit menu width (nullopt = Fit/Stretch to container)
    std::optional<float> borderRadius{std::nullopt}; ///< Item corner rounding radius override
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
};

/// Internal runtime payload allocated in frame arena for SidebarItem leaf nodes.
struct SidebarItemData {
    SidebarMenuItem item{};
    SidebarMenuOptions menuOpts{};
};

/// RAII scope for building a SidebarMenu declaratively with sections, items, separators, and spacers.
class [[nodiscard]] SidebarMenuScope : public ContainerScope {
public:
    SidebarMenuScope(Context* ctx, Node* node, const SidebarMenuOptions& opts) noexcept
        : ContainerScope(ctx, node), m_opts(opts) {}

    /// Adds a section header (with automatic top spacing when preceded by other items).
    void Section(
        UiText title,
        bool divider = true,
        std::source_location loc = std::source_location::current()
    );

    /// Adds a clickable navigation item and returns its interaction Response.
    Response Item(
        const SidebarMenuItem& item,
        std::source_location loc = std::source_location::current()
    );

    /// Adds a navigation item bound to a state variable (enum, int, size_t, etc.), updating it on click.
    template <typename T>
    Response Item(
        T& selectedValue,
        T itemValue,
        SidebarMenuItem item,
        std::source_location loc = std::source_location::current()
    ) {
        item.active = (selectedValue == itemValue);
        Response res = Item(item, loc);
        if (res.clicked && selectedValue != itemValue) {
            selectedValue = itemValue;
            res.changed = true;
            InvalidateContext();
        }
        return res;
    }

    /// Adds a horizontal separator line between menu groups.
    void Separator(std::source_location loc = std::source_location::current());

    /// Adds a vertical expanding spacer pushing subsequent items to the bottom of the menu.
    void Spacer(float flex = 1.0f, std::source_location loc = std::source_location::current());

private:
    void InvalidateContext() noexcept;
    SidebarMenuOptions m_opts{};
};

/// Autonomous SidebarMenu widget logic (scope opening, span execution, item measuring, and rendering).
class SidebarMenu {
public:
    /// Opens an immediate-mode RAII scope for adding Section(), Item(), Separator(), and Spacer().
    [[nodiscard]] static SidebarMenuScope Open(
        Context& ctx,
        const SidebarMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Primary immediate-mode span entry point (renders all items and updates activeIndex on selection).
    static Response Run(
        Context& ctx,
        int& activeIndex,
        std::span<const SidebarMenuItem> items,
        const SidebarMenuOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Renders a single standalone SidebarMenuItem inside the current container.
    static Response Item(
        Context& ctx,
        const SidebarMenuItem& item,
        const SidebarMenuOptions& menuOpts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions of a single sidebar menu item.
    static Vec2 MeasureItem(
        const SidebarMenuItem& item,
        const SidebarMenuOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    );
    static Vec2 MeasureItem(
        const SidebarMenuItem& item,
        const SidebarMenuOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Renders a single sidebar menu item according to style, active selection, hover, and disabled state.
    static void RenderItem(
        ICanvas& canvas,
        Rect bounds,
        const SidebarMenuItem& item,
        const SidebarMenuOptions& opts,
        UiSize size,
        bool hovered,
        bool pressed,
        bool disabled,
        const UiTheme& theme = UiTheme::Dark(),
        bool focused = false
    );
};

} // namespace guikit
