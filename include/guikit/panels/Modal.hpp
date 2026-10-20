#pragma once

#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Configuration options for the Modal dialog container.
struct ModalOptions {
    UiText title{};                                ///< Dialog header title
    UiText subtitle{};                             ///< Optional secondary description in header
    Icon icon{};                                   ///< Optional leading header icon (auto-resolved from variant if empty and variant != Default)
    UiVariant variant{UiVariant::Default};         ///< Semantic accent variant for header badge and accent bar
    float width{440.0f};                           ///< Dialog box width in logical pixels
    std::optional<float> height{std::nullopt};     ///< Explicit dialog height in logical pixels (nullopt = fit content)
    float maxHeightRatio{0.88f};                   ///< Maximum height as fraction of backdrop height
    std::optional<float> gap{std::nullopt};        ///< Vertical gap between body child widgets
    std::optional<Padding> padding{std::nullopt};  ///< Body padding (nullopt = standard spacious dialog padding)
    StateColor background{};                       ///< Optional dialog surface background color override
    Border border{.width = 1.0f, .radius = 12.0f}; ///< Dialog border width, corner radius, and color
    float backdropOpacity{0.55f};                  ///< Dimmed backdrop alpha (0.0..1.0)
    bool closeOnBackdrop{true};                    ///< Dismiss modal when clicking outside the dialog box
    bool closeOnEscape{true};                      ///< Dismiss modal when pressing Escape
    bool showCloseButton{true};                    ///< Render [X] close button in top-right header
    bool showHeaderDivider{true};                  ///< Render 1px divider below header when title/subtitle is present
    bool confineToParent{false};                   ///< Center and clip backdrop inside current parent container instead of full viewport
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

/// RAII scope guard returned by `ui.Modal(...)`.
class [[nodiscard]] ModalScope : public ContainerScope {
public:
    Response response{};
    bool closedByUser{false}; ///< True on the frame the modal was dismissed via [X], backdrop click, or Escape

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
        : ContainerScope(std::move(other))
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
            ContainerScope::operator=(std::move(other));
            response = other.response;
            closedByUser = other.closedByUser;
            m_openPtr = other.m_openPtr;
            m_activeModal = other.m_activeModal;
            other.m_openPtr = nullptr;
            other.m_activeModal = false;
        }
        return *this;
    }

    /// Imperatively closes the modal from inside its scope (e.g. when a Confirm/Cancel button is clicked).
    void Close() noexcept;

private:
    void EndModalIfNeeded() noexcept;

    bool* m_openPtr{nullptr};
    bool m_activeModal{false};
};

/// Out-of-flow floating Modal dialog container with backdrop dimming and focus/input isolation.
class Modal {
public:
    [[nodiscard]] static ModalScope Open(
        Context& ctx,
        bool& open,
        const ModalOptions& opts = {},
        std::source_location loc = std::source_location::current()
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

    static void Render(
        const Node* node,
        ICanvas& canvas,
        const UiTheme& theme
    );

    [[nodiscard]] static float ComputeHeaderHeight(
        const ModalOptions& opts,
        UiSize size,
        const UiTheme& theme,
        ICanvas* canvas = nullptr
    ) noexcept;

    [[nodiscard]] static Rect ResolveDialogRect(
        const Node* node,
        float resolvedHeight,
        const Rect& fallbackBackdrop
    ) noexcept;

    [[nodiscard]] static Rect GetBodyClipRect(const Node* node) noexcept;
};

} // namespace guikit
