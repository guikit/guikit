#pragma once

#include "guikit/context.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "guikit/style/tokens.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

class Context;
struct Node;

/// Placement of Minimize / Maximize / Close window control buttons in a `HeaderBar`.
enum class WindowControlsPlacement : keel::u8 {
    Auto = 0, ///< Right on Windows/Linux, Left (traffic lights) on macOS, hidden on WebAssembly
    Right,    ///< Windows / Linux standard right-edge caption buttons
    Left,     ///< macOS-style left-edge circular window buttons
    None      ///< Do not render window control buttons
};

/// Configuration options for `guikit::HeaderBar` (Client-Side Decorations / custom window title bar).
struct HeaderBarOptions {
    UiText title{};                                       ///< Window title text (falls back to host title when empty & `showWindowTitle`)
    UiText subtitle{};                                    ///< Optional secondary subtitle / active project path
    Icon icon{};                                          ///< Optional leading application icon
    bool showWindowTitle{true};                           ///< Display host window title if `title` is empty and `bindToWindow` is true
    bool centerTitle{false};                              ///< Center title across the full bar width regardless of left/right widget widths
    bool showWindowControls{true};                        ///< Render Minimize / Maximize / Close buttons
    WindowControlsPlacement controlsPlacement{WindowControlsPlacement::Auto};
    bool bindToWindow{true};                              ///< Route actions to `Context`'s `WindowHostService` and register OS CSD drag regions
    bool allowDrag{true};                                 ///< Allow dragging the window by free space in the header bar
    bool allowDoubleClickMaximize{true};                  ///< Double-clicking free header bar space toggles maximize/restore
    keel::Optional<bool> maximizedOverride{keel::kNullOpt}; ///< Explicit maximized state override (useful for managed frames/previews)
    keel::Optional<bool> focusedOverride{keel::kNullOpt};   ///< Explicit focused state override (useful for managed frames/previews)
    keel::Optional<keel::f32> height{keel::kNullOpt};     ///< Explicit bar height in logical dp (nullopt = theme size ~36–40 dp)
    keel::Optional<keel::f32> gap{keel::kNullOpt};        ///< Horizontal gap between child widgets
    keel::Optional<Padding> padding{keel::kNullOpt};      ///< Internal horizontal/vertical padding
    StateColor background{};
    Border border{.width = 1.0f};                         ///< Bottom 1px hairline divider by default
    UiSize size{UiSize::Md};
    bool disabled{false};
};

/// RAII scope guard returned by `guikit::HeaderBar(ui, opts)`.
class [[nodiscard]] HeaderBarScope : public ContainerScope {
public:
    Response response{};
    bool minimizeClicked{false};
    bool maximizeClicked{false};
    bool closeClicked{false};

    HeaderBarScope() noexcept : ContainerScope(nullptr, nullptr) {}

    HeaderBarScope(
        Context* ctx,
        Node* node,
        Response res,
        const HeaderBarOptions& opts,
        WindowControlsPlacement resolvedPlacement,
        bool maximized,
        bool focused,
        bool minClicked,
        bool maxClicked,
        bool clsClicked,
        Id minBtnId,
        Id maxBtnId,
        Id clsBtnId,
        bool minHovered,
        bool minPressed,
        bool maxHovered,
        bool maxPressed,
        bool clsHovered,
        bool clsPressed
    ) noexcept
        : ContainerScope(ctx, node)
        , response(res)
        , minimizeClicked(minClicked)
        , maximizeClicked(maxClicked)
        , closeClicked(clsClicked)
        , m_opts(opts)
        , m_placement(resolvedPlacement)
        , m_maximized(maximized)
        , m_focused(focused)
        , m_minBtnId(minBtnId)
        , m_maxBtnId(maxBtnId)
        , m_clsBtnId(clsBtnId)
        , m_minHovered(minHovered)
        , m_minPressed(minPressed)
        , m_maxHovered(maxHovered)
        , m_maxPressed(maxPressed)
        , m_clsHovered(clsHovered)
        , m_clsPressed(clsPressed)
        , m_activeHeader(node != nullptr) {}

    ~HeaderBarScope() noexcept {
        CloseHeaderBar();
    }

    HeaderBarScope(const HeaderBarScope&) = delete;
    HeaderBarScope& operator=(const HeaderBarScope&) = delete;

    HeaderBarScope(HeaderBarScope&& other) noexcept
        : ContainerScope(keel::move(other))
        , response(other.response)
        , minimizeClicked(other.minimizeClicked)
        , maximizeClicked(other.maximizeClicked)
        , closeClicked(other.closeClicked)
        , m_opts(other.m_opts)
        , m_placement(other.m_placement)
        , m_maximized(other.m_maximized)
        , m_focused(other.m_focused)
        , m_minBtnId(other.m_minBtnId)
        , m_maxBtnId(other.m_maxBtnId)
        , m_clsBtnId(other.m_clsBtnId)
        , m_minHovered(other.m_minHovered)
        , m_minPressed(other.m_minPressed)
        , m_maxHovered(other.m_maxHovered)
        , m_maxPressed(other.m_maxPressed)
        , m_clsHovered(other.m_clsHovered)
        , m_clsPressed(other.m_clsPressed)
        , m_activeHeader(other.m_activeHeader) {
        other.m_activeHeader = false;
    }

    HeaderBarScope& operator=(HeaderBarScope&& other) noexcept {
        if (this != &other) {
            CloseHeaderBar();
            ContainerScope::operator=(keel::move(other));
            response = other.response;
            minimizeClicked = other.minimizeClicked;
            maximizeClicked = other.maximizeClicked;
            closeClicked = other.closeClicked;
            m_opts = other.m_opts;
            m_placement = other.m_placement;
            m_maximized = other.m_maximized;
            m_focused = other.m_focused;
            m_minBtnId = other.m_minBtnId;
            m_maxBtnId = other.m_maxBtnId;
            m_clsBtnId = other.m_clsBtnId;
            m_minHovered = other.m_minHovered;
            m_minPressed = other.m_minPressed;
            m_maxHovered = other.m_maxHovered;
            m_maxPressed = other.m_maxPressed;
            m_clsHovered = other.m_clsHovered;
            m_clsPressed = other.m_clsPressed;
            m_activeHeader = other.m_activeHeader;
            other.m_activeHeader = false;
        }
        return *this;
    }

    /// Inserts an elastic horizontal spacer pushing subsequent items toward the right edge.
    void Spacer(keel::SourceLocation loc = keel::SourceLocation::current());

    /// Inserts a fixed horizontal gap in logical dp.
    void Gap(keel::f32 sizeDp = 8.0f, keel::SourceLocation loc = keel::SourceLocation::current());

    /// Renders a subtle vertical hairline divider inside the header bar.
    void Separator(keel::f32 heightRatio = 0.52f, keel::SourceLocation loc = keel::SourceLocation::current());

    [[nodiscard]] constexpr bool IsMaximized() const noexcept { return m_maximized; }
    [[nodiscard]] constexpr bool IsWindowFocused() const noexcept { return m_focused; }
    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    void CloseHeaderBar() noexcept;

    HeaderBarOptions m_opts{};
    WindowControlsPlacement m_placement{WindowControlsPlacement::Right};
    bool m_maximized{false};
    bool m_focused{true};
    Id m_minBtnId{};
    Id m_maxBtnId{};
    Id m_clsBtnId{};
    bool m_minHovered{false};
    bool m_minPressed{false};
    bool m_maxHovered{false};
    bool m_maxPressed{false};
    bool m_clsHovered{false};
    bool m_clsPressed{false};
    bool m_activeHeader{false};
};

namespace detail {

class HeaderBar {
public:
    [[nodiscard]] static HeaderBarScope Open(
        Context& ctx,
        const HeaderBarOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static void EmitTrailingControls(
        Context& ctx,
        const HeaderBarOptions& opts,
        WindowControlsPlacement placement,
        bool maximized,
        bool focused,
        Id minBtnId,
        Id maxBtnId,
        Id clsBtnId,
        bool minHovered,
        bool minPressed,
        bool maxHovered,
        bool maxPressed,
        bool clsHovered,
        bool clsPressed
    );
};

} // namespace detail

/// Opens a custom Client-Side Decoration (CSD) window header bar container.
[[nodiscard]] inline HeaderBarScope HeaderBar(
    Context& ui,
    HeaderBarOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::HeaderBar::Open(ui, opts, loc);
}

/// Registers an arbitrary rectangle (in current coordinate space) as a draggable window caption region.
inline void WindowDragArea(Context& ui, Rect localRect) {
    ui.RegisterWindowDragRect(localRect);
}

/// Registers a laid-out widget/container node as a draggable window caption region.
inline void WindowDragArea(Context& ui, Id nodeId) {
    ui.RegisterWindowDragNode(nodeId);
}

} // namespace guikit
