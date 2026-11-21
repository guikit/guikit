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

/// Configuration options for the out-of-flow `Modal` overlay portal (backdrop, input/focus isolation, placement).
/// Modal does not render a surface card or style children; place a `ui.Card(...)`, `ui.Box(...)`, or widget inside it.
struct ModalOptions {
    keel::f32 backdropOpacity{0.55f};                        ///< Dimmed backdrop alpha (0.0..1.0) when `backdropColor` is unset
    keel::Optional<Color> backdropColor{keel::kNullOpt}; ///< Explicit backdrop fill override
    bool closeOnBackdrop{true};                          ///< Dismiss modal when clicking backdrop outside modal content
    bool closeOnEscape{true};                            ///< Dismiss modal when pressing Escape
    bool confineToParent{false};                         ///< Anchor and clip backdrop inside current parent container instead of full viewport
    Padding padding{};                                   ///< Explicit inset between backdrop bounds and modal content
    Alignment align{Alignment::Center};                  ///< Horizontal placement of child content within padded backdrop bounds
    Alignment verticalAlign{Alignment::Center};          ///< Vertical placement of child content within padded backdrop bounds
};

/// RAII scope guard returned by `ui.Modal(...)`.
class [[nodiscard]] ModalScope : public ContainerScope {
public:
    Response response{};
    bool closedByUser{false}; ///< True on the frame the modal was dismissed via backdrop click or Escape

    ModalScope() noexcept : ContainerScope(nullptr, nullptr) {}

    ModalScope(
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
        , m_activeModal(node != nullptr) {}

    ~ModalScope() noexcept;

    ModalScope(const ModalScope&) = delete;
    ModalScope& operator=(const ModalScope&) = delete;

    ModalScope(ModalScope&& other) noexcept
        : ContainerScope(keel::move(other))
        , response(other.response)
        , closedByUser(other.closedByUser)
        , m_openPtr(other.m_openPtr)
        , m_activeModal(other.m_activeModal) {
        other.m_openPtr = nullptr;
        other.m_activeModal = false;
    }

    ModalScope& operator=(ModalScope&& other) noexcept {
        if (this != &other) {
            EndModalIfNeeded();
            ContainerScope::operator=(keel::move(other));
            response = other.response;
            closedByUser = other.closedByUser;
            m_openPtr = other.m_openPtr;
            m_activeModal = other.m_activeModal;
            other.m_openPtr = nullptr;
            other.m_activeModal = false;
        }
        return *this;
    }

    /// Returns the portal interaction response snapshot for this modal frame.
    [[nodiscard]] const Response& GetResponse() const noexcept { return response; }

    /// Imperatively closes the modal from inside its scope (e.g. when a Confirm/Cancel button is clicked).
    void Close() noexcept;

private:
    void EndModalIfNeeded() noexcept;

    bool* m_openPtr{nullptr};
    bool m_activeModal{false};
};

/// Out-of-flow modal overlay host with backdrop dimming and focus/input isolation.
class Modal {
public:
    [[nodiscard]] static ModalScope Open(
        Context& ctx,
        bool& open,
        const ModalOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    static Vec2 Measure(
        const Node* node,
        keel::f32 availableWidth,
        ICanvas* canvas,
        const UiTheme& theme
    );

    static void Arrange(
        Node* node,
        const Rect& availableRect,
        const UiTheme& theme
    );

    static void Render(
        const Node* node,
        ICanvas& canvas,
        const UiTheme& theme
    );

    /// Returns the backdrop rectangle in the modal's local coordinate space.
    [[nodiscard]] static Rect GetBackdropRect(const Node* node) noexcept;
};

} // namespace guikit
