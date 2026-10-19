#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"
#include "guikit/panels/Sidebar.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Visual highlight style for active ActivityBar items.
enum class ActivityBarStyle : uint8_t {
    Indicator,  ///< VS Code style vertical accent bar on the outer rail edge + tinted icon
    Pill        ///< Full rounded square capsule highlight behind the active icon
};

/// Definition of an individual icon item or action button inside an ActivityBar.
struct ActivityBarItem {
    Icon icon{};                                        ///< Primary icon displayed centered in the rail cell
    UiText label{};                                     ///< Item name (used as fallback monogram and default tooltip)
    UiText tooltip{};                                   ///< Explicit floating tooltip override (defaults to label)
    UiText badge{};                                     ///< Optional corner badge counter/status (e.g. "3", "9+", "!")
    std::optional<UiVariant> variant{std::nullopt};     ///< Optional per-item accent variant override
    bool active{false};                                 ///< Whether this item is currently active/selected
    bool disabled{false};
};

/// Configuration options for a dockable vertical ActivityBar rail container.
struct ActivityBarOptions {
    SidebarSide side{SidebarSide::Left};                ///< Docking edge (Left or Right)
    ActivityBarStyle style{ActivityBarStyle::Indicator}; ///< Active item indicator style
    UiVariant variant{UiVariant::Primary};              ///< Default accent variant for active items and badges
    SidebarMode* linkedSidebarMode{nullptr};            ///< Optional companion Sidebar mode for VS Code click-to-toggle/expand behavior
    bool flushWithSidebar{true};                        ///< Dock adjacent same-side Sidebar flush (0px gap) against this rail
    std::optional<float> width{std::nullopt};           ///< Optional explicit rail width (nullopt = auto-fit to square icon cells)
    std::optional<float> gap{4.0f};                     ///< Vertical spacing between consecutive items
    std::optional<Padding> padding{std::nullopt};       ///< Internal rail padding (nullopt = compact symmetric padding)
    Color backgroundColor{Color::Transparent()};
    Color borderColor{Color::Transparent()};
    float borderWidth{1.0f};
    std::optional<float> borderRadius{std::nullopt};    ///< Rail corner rounding radius (nullopt = from theme)
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

/// Internal runtime payload allocated in frame arena for ActivityBar item nodes.
struct ActivityBarItemData {
    ActivityBarItem item{};
    ActivityBarOptions barOpts{};
};

/// RAII scope returned by `ui.ActivityBar(...)` for adding items, spacers, separators, and bottom actions.
class [[nodiscard]] ActivityBarScope : public ContainerScope {
public:
    Response response{};

    ActivityBarScope() noexcept : ContainerScope(nullptr, nullptr) {}
    ActivityBarScope(Context* ctx, Node* node, Response res, const ActivityBarOptions& opts) noexcept
        : ContainerScope(ctx, node), response(res), m_opts(opts) {}

    /// Adds a clickable activity bar item and returns its interaction Response.
    Response Item(
        const ActivityBarItem& item,
        std::source_location loc = std::source_location::current()
    );

    /// Adds a stateful activity bar item bound to `selectedValue` (enum, int, size_t).
    /// When `linkedSidebarMode` is configured on the ActivityBar:
    /// - Clicking an inactive item selects it AND expands the linked Sidebar if it was Collapsed.
    /// - Clicking the already-active item toggles the linked Sidebar between Expanded and Collapsed.
    template <typename T>
    Response Item(
        T& selectedValue,
        T itemValue,
        ActivityBarItem item,
        std::source_location loc = std::source_location::current()
    ) {
        const bool isSelected = (selectedValue == itemValue);
        const bool isSidebarOpen = (m_opts.linkedSidebarMode == nullptr) ||
            (*m_opts.linkedSidebarMode != SidebarMode::Collapsed);
        item.active = isSelected && isSidebarOpen;

        Response res = Item(item, loc);
        if (res.clicked) {
            if (!isSelected) {
                selectedValue = itemValue;
                if (m_opts.linkedSidebarMode != nullptr && *m_opts.linkedSidebarMode == SidebarMode::Collapsed) {
                    *m_opts.linkedSidebarMode = SidebarMode::Expanded;
                }
                res.changed = true;
                InvalidateContext();
            } else if (m_opts.linkedSidebarMode != nullptr) {
                *m_opts.linkedSidebarMode = isSidebarOpen
                    ? SidebarMode::Collapsed
                    : SidebarMode::Expanded;
                res.changed = true;
                InvalidateContext();
            }
        }
        return res;
    }

    /// Adds a stateless utility/action button (e.g. Settings or Account at the bottom of the rail)
    /// that does not modify the active view selection or linked Sidebar state.
    Response Action(
        ActivityBarItem item,
        std::source_location loc = std::source_location::current()
    ) {
        return Item(item, loc);
    }

    /// Adds a subtle horizontal separator line across the ActivityBar.
    void Separator(std::source_location loc = std::source_location::current());

    /// Adds an elastic vertical spacer pushing subsequent items (e.g. Settings/Account) to the bottom.
    void Spacer(float flex = 1.0f, std::source_location loc = std::source_location::current());

    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    void InvalidateContext() noexcept;
    ActivityBarOptions m_opts{};
};

/// Autonomous immediate-mode ActivityBar panel & item logic.
class ActivityBar {
public:
    /// Opens an immediate-mode RAII scope for building an ActivityBar with Item(), Action(), Spacer(), and Separator().
    [[nodiscard]] static ActivityBarScope Open(
        Context& ctx,
        const ActivityBarOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Convenience overload binding a companion SidebarMode directly for VS Code toggle behavior.
    [[nodiscard]] static ActivityBarScope Open(
        Context& ctx,
        SidebarMode& linkedSidebarMode,
        ActivityBarOptions opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        opts.linkedSidebarMode = &linkedSidebarMode;
        return Open(ctx, opts, loc);
    }

    /// Declarative span entry point for rendering a list of ActivityBarItems and updating `activeIndex`.
    static Response Run(
        Context& ctx,
        int& activeIndex,
        std::span<const ActivityBarItem> items,
        const ActivityBarOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Renders a single standalone ActivityBarItem inside the current container.
    static Response Item(
        Context& ctx,
        const ActivityBarItem& item,
        const ActivityBarOptions& barOpts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions of a single ActivityBar item cell.
    static Vec2 MeasureItem(
        const ActivityBarItem& item,
        const ActivityBarOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    );

    /// Renders a single ActivityBar item with icon, active indicator/pill, and corner badge.
    static void RenderItem(
        ICanvas& canvas,
        Rect bounds,
        const ActivityBarItem& item,
        const ActivityBarOptions& opts,
        UiSize size,
        bool hovered,
        bool pressed,
        bool disabled,
        const UiTheme& theme = UiTheme::Dark(),
        bool focused = false
    );
};

} // namespace guikit
