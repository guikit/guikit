#pragma once

#include "guikit/context.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/control.hpp"
#include <initializer_list>
#include "guikit/core/response.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"

namespace guikit {

class Context;
class ICanvas;

/// Visual presentation style for TabBar.
enum class TabBarStyle : keel::u8 {
    Underline,  ///< Classic desktop/IDE style: transparent tab background with an accent bottom indicator line
    Pill,       ///< Segmented capsule style: active tab has a rounded background fill
    Card,       ///< Outlined card tabs docked to baseline (open bottom on active tab)
    Folder      ///< Browser/editor folder tabs with angled/tapered shoulders and top accent line
};

/// Definition of an individual tab item.
struct TabItem {
    UiText label{};
    Icon icon{};
    UiText badge{};
    UiText tooltip{};
    UiVariant variant{UiVariant::Default};
    bool closable{false};   ///< Show an interactive close ('X') button on the trailing side of the tab
    bool disabled{false};
};

/// Response returned by TabBar::Run, extending Response with closed-tab notification.
struct TabBarResponse : Response {
    keel::Optional<keel::usize> closedIndex{keel::kNullOpt}; ///< Index of the tab whose close ('X') button was clicked this frame
};

/// Configuration options for TabBar widget.
struct TabBarOptions {
    TabBarStyle style{TabBarStyle::Underline};
    bool showSeparator{true};   ///< In Underline/Card/Folder styles: draws a 1px baseline line under tabs
    bool fullWidth{false};      ///< Stretch tabs equally to fill available container width
    bool scrollable{true};      ///< Enable horizontal scrolling with arrows when tabs overflow
    bool disabled{false};       ///< Disable interaction across the entire TabBar
    keel::Optional<keel::f32> gap{keel::kNullOpt};   ///< Gap between consecutive tab items (nullopt = from theme)
    keel::Optional<Padding> padding{keel::kNullOpt};   ///< Container padding around tabs (nullopt = auto based on style)
    UiSize size{UiSize::Md};                     ///< Component size scale
    UiVariant variant{UiVariant::Default};       ///< Default semantic accent variant for active tab indicator / pill (overridden by `TabItem::variant`)
    StateColor color{};                          ///< Optional custom accent color override for active tab indicator / pill
    StateColor background{};                     ///< Optional background fill color for the tab bar
    Border border{};                             ///< Optional border configuration (color, width, radius)
};

/// Compact action button options for trailing or leading controls inside `TabBarScope`.
struct TabBarActionOptions {
    UiText text{};
    UiText tooltip{};
    Icon icon{};
    UiVariant variant{UiVariant::Default};
    ButtonStyle style{ButtonStyle::Ghost};
    bool square{false};
    bool active{false};
    bool disabled{false};
    keel::Optional<UiSize> size{keel::kNullOpt}; ///< Optional size override (nullopt = inherit TabBarOptions::size)
};

/// Internal runtime state allocated in frame arena for TabItem nodes.
struct TabItemData {
    TabItem item{};
    TabBarOptions barOpts{};
    bool closeHovered{false};
};

class TabBarScope;

/// Autonomous TabBar widget logic (execution, measuring, and rendering).
namespace detail {

class TabBar {
public:
    /// Opens an RAII TabBar header scope supporting a scrollable tab strip (`Items()`) plus leading/trailing controls (`Action()`, widgets).
    [[nodiscard]] static TabBarScope Open(
        Context& ctx,
        const TabBarOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Primary immediate mode entry point for TabBar widget.
    static TabBarResponse Run(
        Context& ctx,
        keel::i32& activeIndex,
        keel::ArrayRef<TabItem> tabs,
        const TabBarOptions& opts,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Renders the scrollable tab strip inside an open `TabBarScope` (baseline is owned by the outer bar).
    static TabBarResponse RunStrip(
        Context& ctx,
        keel::i32& activeIndex,
        keel::ArrayRef<TabItem> tabs,
        const TabBarOptions& opts,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Renders a compact trailing/leading action button inside an open `TabBarScope`.
    static Response ActionButton(
        Context& ctx,
        const TabBarActionOptions& opts,
        const TabBarOptions& barOpts,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions of a single tab item.
    static Vec2 MeasureTab(
        const TabItem& item,
        const TabBarOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    );
    static Vec2 MeasureTab(
        const TabItem& item,
        const TabBarOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Renders a single tab item according to style, selection, hover, and disabled state.
    static void RenderTab(
        ICanvas& canvas,
        Rect bounds,
        const TabItem& item,
        const TabBarOptions& opts,
        UiSize size,
        bool active,
        bool hovered,
        bool pressed,
        bool disabled,
        const UiTheme& theme = UiTheme::Dark(),
        bool focused = false,
        bool closeHovered = false
    );
};

} // namespace detail

/// RAII scope returned by `guikit::TabBar(ui, opts)` for composing a full-width tab header
/// with a unified baseline, scrollable tab strip (`Items()`), and leading/trailing controls (`Action()`, widgets).
class [[nodiscard]] TabBarScope : public ContainerScope {
public:
    Response response{};

    TabBarScope() noexcept : ContainerScope(nullptr, nullptr) {}
    TabBarScope(Context* ctx, Node* node, Response res, const TabBarOptions& opts) noexcept
        : ContainerScope(ctx, node), response(res), m_opts(opts) {}

    /// Renders the interactive tab strip inside this TabBar header, stretching to keep trailing controls pinned right.
    template <IndexLike IndexT>
        requires (!keel::IsConst<IndexT>)
    TabBarResponse Items(
        IndexT& activeIndex,
        keel::ArrayRef<TabItem> tabs,
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if (!m_ctx) return TabBarResponse{};
        WidgetExecGuard guard(*m_ctx);
        keel::i32 idx = keel::as<keel::i32>(activeIndex);
        TabBarResponse r = detail::TabBar::RunStrip(*m_ctx, idx, tabs, m_opts, loc);
        if (r.changed && idx >= 0) {
            activeIndex = detail::FromIndexLike<IndexT>(idx);
        }
        return r;
    }

    /// Adds a compact trailing/leading action button (inheriting `TabBarOptions::size` and defaulting to `ButtonStyle::Ghost`).
    Response Action(
        TabBarActionOptions opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        if (!m_ctx) return Response{};
        return detail::TabBar::ActionButton(*m_ctx, opts, m_opts, loc);
    }

    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    TabBarOptions m_opts{};
};

[[nodiscard]] inline TabBarScope TabBar(
    Context& ui,
    TabBarOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::TabBar::Open(ui, opts, loc);
}

template <IndexLike IndexT>
    requires (!keel::IsConst<IndexT>)
TabBarResponse TabBar(
    Context& ui,
    IndexT& activeIndex,
    keel::ArrayRef<TabItem> tabs,
    TabBarOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    keel::i32 idx = keel::as<keel::i32>(activeIndex);
    TabBarResponse r = detail::TabBar::Run(ui, idx, tabs, opts, loc);
    if (r.changed && idx >= 0) activeIndex = detail::FromIndexLike<IndexT>(idx);
    return r;
}

} // namespace guikit
