#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

struct UiTheme;
class Context;
struct Node;

/// Horizontal docking edge for a Sidebar container.
enum class SidebarSide : uint8_t {
    Left,   ///< Docks to the left side of the parent container (stacked left-to-right in call order)
    Right   ///< Docks to the right side of the parent container (stacked right-to-left in call order)
};

/// Display and sizing state of a Sidebar container.
enum class SidebarMode : uint8_t {
    Expanded,   ///< Full width mode (explicit/user-resized width or Fit to content)
    Compact,    ///< Compact icon-only rail mode (child SidebarMenu automatically renders icons only)
    Collapsed   ///< Completely minimized mode (0px layout width with interactive edge handle)
};

/// Options for configuring a dockable vertical Sidebar surface container.
struct SidebarOptions {
    SidebarSide side{SidebarSide::Left};                ///< Docking side (Left by default, or Right)
    SidebarMode defaultMode{SidebarMode::Expanded};     ///< Initial mode when managed by Context
    std::optional<float> width{std::nullopt};           ///< Explicit sidebar width in Expanded mode (nullopt = Fit to content)
    std::optional<float> minWidth{std::nullopt};        ///< Optional minimum width in logical dp (nullopt = auto from theme metrics)
    std::optional<float> maxWidth{std::nullopt};        ///< Optional maximum width in logical dp (nullopt = auto from parent width * maxRatio)
    float maxRatio{0.65f};                              ///< Maximum fraction of parent container width the sidebar can occupy (0.15 .. 0.90)
    bool resizable{false};                              ///< Allow dragging the inner border (sash) to resize or snap-collapse
    bool allowCompact{false};                           ///< Support Compact (icons-only rail) mode via drag-snap or toggle button
    bool allowCollapse{false};                          ///< Support Collapsed (completely minimized 0px) mode via drag-snap or toggle
    bool showToggleButton{false};                       ///< Optional floating edge tab on border (false by default; Sash line handles resize/collapse)
    bool scrollable{true};                              ///< Automatically scroll vertically when content overflows
    VisibilityPolicy scrollbar{VisibilityPolicy::Auto};
    std::optional<float> gap{std::nullopt};             ///< Gap between child elements inside sidebar (nullopt = from theme)
    std::optional<Padding> padding{std::nullopt};       ///< Sidebar internal padding (nullopt = auto from theme)
    StateColor background{};
    Border border{.width = 1.0f};
    bool clipChildren{true};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

/// Persistent state stored per Sidebar in Context.
struct SidebarPersistState {
    bool initialized{false};
    SidebarMode mode{SidebarMode::Expanded};
    float customWidth{0.0f};
    float lastExpandedWidth{220.0f};
    bool draggingSash{false};
    float dragStartMouseX{0.0f};
    float dragStartWidth{0.0f};
    float dragStartCustomWidth{0.0f};
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
    SidebarScope(Context* ctx, Node* node, Response res, SidebarMode mode, float width) noexcept
        : ContainerScope(ctx, node), response(res), m_mode(mode), m_width(width) {}

    SidebarScope(SidebarScope&& other) noexcept
        : ContainerScope(std::move(other))
        , response(other.response)
        , m_mode(other.m_mode)
        , m_width(other.m_width) {
        other.m_mode = SidebarMode::Collapsed;
    }

    SidebarScope& operator=(SidebarScope&& other) noexcept {
        if (this != &other) {
            ContainerScope::operator=(std::move(other));
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
    [[nodiscard]] constexpr float Width() const noexcept { return m_width; }
    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    SidebarMode m_mode{SidebarMode::Expanded};
    float m_width{0.0f};
};

/// Autonomous immediate mode dockable Sidebar panel container entry point.
class Sidebar {
public:
    [[nodiscard]] static SidebarScope Open(
        Context& ctx,
        const SidebarOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Open(ctx, nullptr, opts, loc);
    }

    [[nodiscard]] static SidebarScope Open(
        Context& ctx,
        SidebarMode* externalMode,
        const SidebarOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
