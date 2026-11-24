#pragma once

#include "guikit/context.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

struct UiTheme;
class Context;
struct Node;

/// Display and sizing state of a BottomPanel container.
enum class BottomPanelMode : keel::u8 {
    Expanded,   ///< Open mode (explicit/user-resized height or Fit to content)
    Collapsed   ///< Minimized mode (0px layout height with interactive top edge / toggle handle)
};

/// Options for configuring a dockable bottom panel surface container.
struct BottomPanelOptions {
    BottomPanelMode defaultMode{BottomPanelMode::Expanded}; ///< Initial mode when managed by Context
    keel::Optional<keel::f32> height{220.0f};                    ///< Explicit panel height in Expanded mode (nullopt = Fit to content)
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};           ///< Optional minimum height in logical dp (nullopt = auto from theme metrics)
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};           ///< Optional maximum height in logical dp (nullopt = auto from parent height * maxRatio)
    keel::f32 maxRatio{0.75f};                                  ///< Maximum fraction of parent container height the panel can occupy (0.15 .. 0.95)
    bool resizable{true};                                   ///< Allow dragging the top border (sash) up/down to resize or snap-collapse
    bool allowCollapse{true};                               ///< Support Collapsed (completely minimized 0px) mode via drag-snap or toggle
    bool showToggleButton{false};                           ///< Optional floating edge pill button on the top border
    bool scrollable{false};                                 ///< Automatically scroll vertically when content overflows
    VisibilityPolicy scrollbar{VisibilityPolicy::Auto};
    keel::Optional<keel::f32> gap{0.0f};                         ///< Gap between direct children (0px by default so inner Toolbar docks flush)
    keel::Optional<Padding> padding{Padding::All(0.0f)};     ///< Internal padding (0px by default so inner Toolbar/views fill edge-to-edge)
    StateColor background{};
    Border border{.width = 1.0f};
    bool clipChildren{true};
    UiSize size{UiSize::Md};
    bool disabled{false};
};

/// Persistent state stored per BottomPanel in Context.
struct BottomPanelPersistState {
    bool initialized{false};
    BottomPanelMode mode{BottomPanelMode::Expanded};
    keel::f32 customHeight{0.0f};
    keel::f32 lastExpandedHeight{220.0f};
    bool draggingSash{false};
    keel::f32 dragStartMouseY{0.0f};
    keel::f32 dragStartHeight{0.0f};
    keel::f32 dragStartCustomHeight{0.0f};
    BottomPanelMode dragStartMode{BottomPanelMode::Expanded};
    bool sashHovered{false};
    bool toggleHovered{false};
    bool togglePressed{false};
};

/// RAII scope returned by `ui.BottomPanel(...)`.
/// Evaluates to `true` (`operator bool()`) when in `Expanded` mode,
/// and `false` when in `Collapsed` mode (skipping child execution when minimized to 0px).
class [[nodiscard]] BottomPanelScope : public ContainerScope {
public:
    Response response{};

    BottomPanelScope() noexcept : ContainerScope(nullptr, nullptr) {}
    BottomPanelScope(Context* ctx, Node* node, Response res, BottomPanelMode mode, keel::f32 height) noexcept
        : ContainerScope(ctx, node), response(res), m_mode(mode), m_height(height) {}

    BottomPanelScope(BottomPanelScope&& other) noexcept
        : ContainerScope(keel::move(other))
        , response(other.response)
        , m_mode(other.m_mode)
        , m_height(other.m_height) {
        other.m_mode = BottomPanelMode::Collapsed;
    }

    BottomPanelScope& operator=(BottomPanelScope&& other) noexcept {
        if (this != &other) {
            ContainerScope::operator=(keel::move(other));
            response = other.response;
            m_mode = other.m_mode;
            m_height = other.m_height;
            other.m_mode = BottomPanelMode::Collapsed;
        }
        return *this;
    }

    /// Evaluates to true when the bottom panel is Expanded; false when Collapsed (0px).
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return m_node != nullptr && m_mode != BottomPanelMode::Collapsed;
    }

    [[nodiscard]] constexpr BottomPanelMode Mode() const noexcept { return m_mode; }
    [[nodiscard]] constexpr bool IsExpanded() const noexcept { return m_mode == BottomPanelMode::Expanded; }
    [[nodiscard]] constexpr bool IsCollapsed() const noexcept { return m_mode == BottomPanelMode::Collapsed; }
    [[nodiscard]] constexpr keel::f32 Height() const noexcept { return m_height; }
    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    BottomPanelMode m_mode{BottomPanelMode::Expanded};
    keel::f32 m_height{0.0f};
};

/// Autonomous immediate mode dockable BottomPanel container entry point.
namespace detail {

class BottomPanel {
public:
    [[nodiscard]] static BottomPanelScope Open(
        Context& ctx,
        const BottomPanelOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Open(ctx, static_cast<BottomPanelMode*>(nullptr), opts, loc);
    }

    [[nodiscard]] static BottomPanelScope Open(
        Context& ctx,
        bool* open,
        const BottomPanelOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] static BottomPanelScope Open(
        Context& ctx,
        BottomPanelMode* externalMode,
        const BottomPanelOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace detail

[[nodiscard]] inline BottomPanelScope BottomPanel(
    Context& ui,
    BottomPanelOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::BottomPanel::Open(ui, opts, loc);
}

[[nodiscard]] inline BottomPanelScope BottomPanel(
    Context& ui,
    BottomPanelMode& mode,
    BottomPanelOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::BottomPanel::Open(ui, &mode, opts, loc);
}

[[nodiscard]] inline BottomPanelScope BottomPanel(
    Context& ui,
    BottomPanelMode* mode,
    BottomPanelOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::BottomPanel::Open(ui, mode, opts, loc);
}

[[nodiscard]] inline BottomPanelScope BottomPanel(
    Context& ui,
    bool& open,
    BottomPanelOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::BottomPanel::Open(ui, &open, opts, loc);
}

[[nodiscard]] inline BottomPanelScope BottomPanel(
    Context& ui,
    bool* open,
    BottomPanelOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::BottomPanel::Open(ui, open, opts, loc);
}

} // namespace guikit
