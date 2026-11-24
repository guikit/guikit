#pragma once

#include "guikit/context.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/style/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

struct UiTheme;
class Context;
struct Node;

/// Horizontal docking edge for a Sidebar container.
enum class SidebarSide : keel::u8 {
    Left,   ///< Docks to the left side of the parent container (stacked left-to-right in call order)
    Right   ///< Docks to the right side of the parent container (stacked right-to-left in call order)
};

/// Display and sizing state of a Sidebar container.
enum class SidebarMode : keel::u8 {
    Expanded,   ///< Full width mode (explicit/user-resized width or Fit to content)
    Compact,    ///< Compact icon-only rail mode (child SidebarMenu automatically renders icons only)
    Collapsed   ///< Completely minimized mode (0px layout width with interactive edge handle)
};

/// Options for configuring a dockable vertical Sidebar surface container.
struct SidebarOptions {
    SidebarSide side{SidebarSide::Left};                ///< Docking side (Left by default, or Right)
    SidebarMode defaultMode{SidebarMode::Expanded};     ///< Initial mode when managed by Context
    keel::Optional<keel::f32> width{keel::kNullOpt};           ///< Explicit sidebar width in Expanded mode (nullopt = Fit to content)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};        ///< Optional minimum width in logical dp (nullopt = auto from theme metrics)
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};        ///< Optional maximum width in logical dp (nullopt = auto from parent width * maxRatio)
    keel::f32 maxRatio{0.65f};                              ///< Maximum fraction of parent container width the sidebar can occupy (0.15 .. 0.90)
    bool resizable{false};                              ///< Allow dragging the inner border (sash) to resize or snap-collapse
    bool allowCompact{false};                           ///< Support Compact (icons-only rail) mode via drag-snap or toggle button
    bool allowCollapse{false};                          ///< Support Collapsed (completely minimized 0px) mode via drag-snap or toggle
    bool showToggleButton{false};                       ///< Optional floating edge tab on border (false by default; Sash line handles resize/collapse)
    bool scrollable{true};                              ///< Automatically scroll vertically when content overflows
    VisibilityPolicy scrollbar{VisibilityPolicy::Auto};
    keel::Optional<keel::f32> gap{keel::kNullOpt};             ///< Gap between child elements inside sidebar (nullopt = from theme)
    keel::Optional<Padding> padding{keel::kNullOpt};       ///< Sidebar internal padding (nullopt = auto from theme)
    StateColor background{};
    Border border{.width = 1.0f};
    bool clipChildren{true};
    UiSize size{UiSize::Md};
    bool disabled{false};
};

/// Persistent state stored per Sidebar in Context.
struct SidebarPersistState {
    bool initialized{false};
    SidebarMode mode{SidebarMode::Expanded};
    keel::f32 customWidth{0.0f};
    keel::f32 lastExpandedWidth{220.0f};
    bool draggingSash{false};
    keel::f32 dragStartMouseX{0.0f};
    keel::f32 dragStartWidth{0.0f};
    keel::f32 dragStartCustomWidth{0.0f};
    SidebarMode dragStartMode{SidebarMode::Expanded};
    bool sashHovered{false};
    bool toggleHovered{false};
    bool togglePressed{false};
};

/// RAII scope returned by `ui.Sidebar(...)`.
/// Evaluates to `true` (`operator bool()`) when in `Expanded` or `Compact` mode,
/// and `false` when in `Collapsed` mode (skipping child execution when minimized to 0px).
class [[nodiscard]] SidebarScope : public ContainerScope {
public:
    Response response{};

    SidebarScope() noexcept : ContainerScope(nullptr, nullptr) {}
    SidebarScope(Context* ctx, Node* node, Response res, SidebarMode mode, keel::f32 width) noexcept
        : ContainerScope(ctx, node), response(res), m_mode(mode), m_width(width) {}

    SidebarScope(SidebarScope&& other) noexcept
        : ContainerScope(keel::move(other))
        , response(other.response)
        , m_mode(other.m_mode)
        , m_width(other.m_width) {
        other.m_mode = SidebarMode::Collapsed;
    }

    SidebarScope& operator=(SidebarScope&& other) noexcept {
        if (this != &other) {
            ContainerScope::operator=(keel::move(other));
            response = other.response;
            m_mode = other.m_mode;
            m_width = other.m_width;
            other.m_mode = SidebarMode::Collapsed;
        }
        return *this;
    }

    /// Evaluates to true when the sidebar is Expanded or Compact; false when Collapsed (0px).
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return m_node != nullptr && m_mode != SidebarMode::Collapsed;
    }

    [[nodiscard]] constexpr SidebarMode Mode() const noexcept { return m_mode; }
    [[nodiscard]] constexpr bool IsExpanded() const noexcept { return m_mode == SidebarMode::Expanded; }
    [[nodiscard]] constexpr bool IsCompact() const noexcept { return m_mode == SidebarMode::Compact; }
    [[nodiscard]] constexpr bool IsCollapsed() const noexcept { return m_mode == SidebarMode::Collapsed; }
    [[nodiscard]] constexpr keel::f32 Width() const noexcept { return m_width; }
    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    SidebarMode m_mode{SidebarMode::Expanded};
    keel::f32 m_width{0.0f};
};

/// Autonomous immediate mode dockable Sidebar panel container entry point.
namespace detail {

class Sidebar {
public:
    [[nodiscard]] static SidebarScope Open(
        Context& ctx,
        const SidebarOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Open(ctx, nullptr, opts, loc);
    }

    [[nodiscard]] static SidebarScope Open(
        Context& ctx,
        SidebarMode* externalMode,
        const SidebarOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace detail

[[nodiscard]] inline SidebarScope Sidebar(
    Context& ui,
    SidebarOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::Sidebar::Open(ui, nullptr, opts, loc);
}

[[nodiscard]] inline SidebarScope Sidebar(
    Context& ui,
    SidebarMode& mode,
    SidebarOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::Sidebar::Open(ui, &mode, opts, loc);
}

[[nodiscard]] inline SidebarScope Sidebar(
    Context& ui,
    bool& open,
    SidebarOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    SidebarMode mode = open ? SidebarMode::Expanded : SidebarMode::Collapsed;
    SidebarScope scope = detail::Sidebar::Open(ui, &mode, opts, loc);
    open = (mode != SidebarMode::Collapsed);
    return scope;
}

} // namespace guikit
