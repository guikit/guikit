#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;
struct UiTheme;

/// Preferred edge alignment of a floating `Popover` relative to its trigger `anchorRect`.
enum class PopoverPlacement : keel::u8 {
    Auto = 0,     ///< Chooses BottomStart/BottomEnd based on trigger horizontal position, flipping above if clipped
    BottomStart,  ///< Below anchor, left edges aligned
    BottomEnd,    ///< Below anchor, right edges aligned
    TopStart,     ///< Above anchor, left edges aligned
    TopEnd        ///< Above anchor, right edges aligned
};

/// Configuration options for a non-modal out-of-flow `Popover` portal anchored to `anchorRect`.
/// Popover does not render a surface card or style children; place a `ui.Card(...)`, `ui.Box(...)`, or widget inside it.
struct PopoverOptions {
    Rect anchorRect{};                                  ///< Trigger bounds in current coordinates
    PopoverPlacement placement{PopoverPlacement::Auto}; ///< Preferred placement relative to anchorRect
    float offset{6.0f};                                 ///< Gap in logical dp between anchorRect and popover bounds
    keel::Optional<Rect> viewport{keel::kNullOpt};      ///< Explicit confining bounds (nullopt = active popup viewport)
    bool closeOnClickOutside{true};                     ///< Dismiss when clicking outside both popover content and anchorRect
    bool closeOnEscape{true};                           ///< Dismiss when pressing Escape (if no child dropdown popup is open)
    bool consumeOutsideClick{false};                    ///< If true, consumes the mouse click that closes the popover
};

/// RAII scope guard returned by `ui.Popover(...)`.
class [[nodiscard]] PopoverScope : public ContainerScope {
public:
    Response response{};
    bool closedByUser{false}; ///< True on the frame the popover was dismissed by outside click or Escape

    PopoverScope() noexcept : ContainerScope(nullptr, nullptr) {}

    PopoverScope(
        Context* ctx,
        Node* node,
        bool* openPtr,
        Response res,
        bool closed = false
    ) noexcept
        : ContainerScope(ctx, node)
        , response(res)
        , closedByUser(closed)
        , m_openPtr(openPtr)
        , m_activePopover(node != nullptr) {}

    ~PopoverScope() noexcept;

    PopoverScope(const PopoverScope&) = delete;
    PopoverScope& operator=(const PopoverScope&) = delete;

    PopoverScope(PopoverScope&& other) noexcept
        : ContainerScope(keel::move(other))
        , response(other.response)
        , closedByUser(other.closedByUser)
        , m_openPtr(other.m_openPtr)
        , m_activePopover(other.m_activePopover) {
        other.m_openPtr = nullptr;
        other.m_activePopover = false;
    }

    PopoverScope& operator=(PopoverScope&& other) noexcept {
        if (this != &other) {
            EndPopoverIfNeeded();
            ContainerScope::operator=(keel::move(other));
            response = other.response;
            closedByUser = other.closedByUser;
            m_openPtr = other.m_openPtr;
            m_activePopover = other.m_activePopover;
            other.m_openPtr = nullptr;
            other.m_activePopover = false;
        }
        return *this;
    }

    /// Returns the portal interaction response snapshot for this popover frame.
    [[nodiscard]] const Response& GetResponse() const noexcept { return response; }

    /// Imperatively closes the popover from inside its scope (e.g. when a confirmation action is clicked).
    void Close() noexcept;

private:
    void EndPopoverIfNeeded() noexcept;

    bool* m_openPtr{nullptr};
    bool m_activePopover{false};
};

/// Out-of-flow non-modal floating portal anchored to a trigger rectangle.
class Popover {
public:
    [[nodiscard]] static PopoverScope Open(
        Context& ctx,
        bool& open,
        const PopoverOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        const Node* node,
        float availableWidth,
        ICanvas* canvas,
        const UiTheme& theme
    );

    static void Arrange(
        Node* node,
        const Rect& availableRect,
        const UiTheme& theme
    );

    /// Computes final popover bounding rect anchored to `opts.anchorRect` inside the confining viewport.
    [[nodiscard]] static Rect ResolvePopoverRect(
        const Node* node,
        float resolvedWidth,
        float resolvedHeight,
        const Rect& fallbackViewport
    ) noexcept;
};

} // namespace guikit
