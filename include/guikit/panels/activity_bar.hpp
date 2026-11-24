#pragma once

#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "guikit/panels/sidebar.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Visual highlight style for active ActivityBar items.
enum class ActivityBarStyle : keel::u8 {
    Indicator,  ///< VS Code style vertical accent bar on the outer rail edge + tinted icon
    Pill        ///< Full rounded square capsule highlight behind the active icon
};

/// Definition of an individual icon item or action button inside an ActivityBar.
struct ActivityBarItem {
    Icon icon{};                                        ///< Primary icon displayed centered in the rail cell
    UiText label{};                                     ///< Item name (used as fallback monogram and default tooltip)
    UiText tooltip{};                                   ///< Explicit floating tooltip override (defaults to label)
    UiText badge{};                                     ///< Optional corner badge counter/status (e.g. "3", "9+", "!")
    StateColor color{};                                 ///< Optional per-item accent color override
    bool active{false};                                 ///< Whether this item is currently active/selected
    bool disabled{false};
};

/// Configuration options for a dockable vertical ActivityBar rail container.
struct ActivityBarOptions {
    SidebarSide side{SidebarSide::Left};                ///< Docking edge (Left or Right)
    ActivityBarStyle style{ActivityBarStyle::Indicator}; ///< Active item indicator style
    StateColor color{};                                 ///< Default accent color for active items and badges (Default = theme accent)
    SidebarMode* linkedSidebarMode{nullptr};            ///< Optional companion Sidebar mode for VS Code click-to-toggle/expand behavior
    keel::Optional<bool> flushWithSidebar{keel::kNullOpt}; ///< Dock adjacent same-side Sidebar flush (nullopt = auto from theme.shellStyle)
    keel::Optional<keel::f32> width{keel::kNullOpt};           ///< Optional explicit rail width (nullopt = auto-fit to square icon cells)
    keel::Optional<keel::f32> gap{4.0f};                     ///< Vertical spacing between consecutive items
    keel::Optional<Padding> padding{keel::kNullOpt};       ///< Internal rail padding (nullopt = compact symmetric padding)
    StateColor background{};
    Border border{.width = 1.0f};
    UiSize size{UiSize::Md};
    bool disabled{false};
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
        keel::SourceLocation loc = keel::SourceLocation::current()
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
        keel::SourceLocation loc = keel::SourceLocation::current()
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
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Item(item, loc);
    }

    /// Adds a subtle horizontal separator line across the ActivityBar.
    void Separator(keel::SourceLocation loc = keel::SourceLocation::current());

    /// Adds an elastic vertical spacer pushing subsequent items (e.g. Settings/Account) to the bottom.
    void Spacer(keel::f32 flex = 1.0f, keel::SourceLocation loc = keel::SourceLocation::current());

    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    void InvalidateContext() noexcept;
    ActivityBarOptions m_opts{};
};

/// Autonomous immediate-mode ActivityBar panel & item logic.
namespace detail {

class ActivityBar {
public:
    /// Opens an immediate-mode RAII scope for building an ActivityBar with Item(), Action(), Spacer(), and Separator().
    [[nodiscard]] static ActivityBarScope Open(
        Context& ctx,
        const ActivityBarOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Convenience overload binding a companion SidebarMode directly for VS Code toggle behavior.
    [[nodiscard]] static ActivityBarScope Open(
        Context& ctx,
        SidebarMode& linkedSidebarMode,
        ActivityBarOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        opts.linkedSidebarMode = &linkedSidebarMode;
        return Open(ctx, opts, loc);
    }

    /// Declarative span entry point for rendering a list of ActivityBarItems and updating `activeIndex`.
    static Response Run(
        Context& ctx,
        keel::i32& activeIndex,
        keel::ArrayRef<ActivityBarItem> items,
        const ActivityBarOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Renders a single standalone ActivityBarItem inside the current container.
    static Response Item(
        Context& ctx,
        const ActivityBarItem& item,
        const ActivityBarOptions& barOpts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
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

} // namespace detail

[[nodiscard]] inline ActivityBarScope ActivityBar(
    Context& ui,
    ActivityBarOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::ActivityBar::Open(ui, opts, loc);
}

[[nodiscard]] inline ActivityBarScope ActivityBar(
    Context& ui,
    SidebarMode& linkedSidebarMode,
    ActivityBarOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::ActivityBar::Open(ui, linkedSidebarMode, keel::move(opts), loc);
}

template <IndexLike IndexT>
    requires (!keel::IsConst<IndexT>)
Response ActivityBar(
    Context& ui,
    IndexT& activeIndex,
    keel::ArrayRef<ActivityBarItem> items,
    ActivityBarOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    if constexpr (keel::IsSame<keel::Decay<IndexT>, keel::i32>) {
        return detail::ActivityBar::Run(ui, activeIndex, items, opts, loc);
    } else {
        keel::i32 idx = keel::as<keel::i32>(activeIndex);
        Response r = detail::ActivityBar::Run(ui, idx, items, opts, loc);
        if (r.changed && idx >= 0) {
            activeIndex = detail::FromIndexLike<IndexT>(idx);
        }
        return r;
    }
}

} // namespace guikit
