#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Configuration options for an Accordion collapsible section panel.
struct AccordionOptions {
    UiText label{};                                 ///< Primary header title text
    UiText description{};                           ///< Optional secondary subtitle below header title
    Icon icon{};                                    ///< Optional vector icon before header title
    UiText badge{};                                 ///< Optional pill badge at the trailing side of the header
    UiText tooltip{};                               ///< Optional hover tooltip text for the header
    bool defaultOpen{true};                         ///< Initial open state when state is managed by Context
    Placement chevronPlacement{Placement::Leading}; ///< Chevron position: Leading (left of label/icon) or Trailing (far right)
    std::optional<float> gap{std::nullopt};         ///< Vertical gap between children inside expanded body (nullopt = from theme)
    std::optional<Padding> padding{std::nullopt};   ///< Padding around expanded body children (nullopt = default top/bottom spacing)
    StateColor background{};                        ///< Per-state header backdrop color (Default = subtle surface bar, None = transparent, Surface/Primary/... = custom)
    Border border{};                                ///< Header border stroke & corner radius
    std::optional<UiSize> size{std::nullopt};       ///< Header size token (nullopt = inherit container size)
    bool disabled{false};                           ///< Whether the accordion header and its children are disabled
    const UiTheme* theme{nullptr};                  ///< Optional scoped theme override
    std::optional<float> scale{std::nullopt};       ///< Optional local UI scale factor override
    std::optional<bool> debugBounds{std::nullopt};  ///< Scoped debug bounding box overlay override
    std::optional<bool> inspectMode{std::nullopt};  ///< Scoped interactive DevTools inspector override
};

/// Persistent per-accordion open/collapsed state stored in Context.
struct AccordionState {
    bool open{true};
};

/// Internal per-frame arena payload for `NodeType::AccordionHeader`.
struct AccordionHeaderData {
    AccordionOptions opts{};
    bool open{true};
};

/// RAII scope returned by `ui.Accordion(...)`.
/// Evaluates to `true` (`operator bool()`) ONLY when the accordion section is expanded (`open == true`),
/// skipping execution of all child widgets inside the `if` body when collapsed.
class [[nodiscard]] AccordionScope : public ContainerScope {
public:
    Response response{};

    AccordionScope() noexcept : ContainerScope(nullptr, nullptr), m_open(false) {}
    AccordionScope(Context* ctx, Node* bodyNode, Response res, bool isOpen) noexcept
        : ContainerScope(ctx, bodyNode), response(res), m_open(isOpen) {}

    AccordionScope(AccordionScope&& other) noexcept
        : ContainerScope(std::move(other)), response(other.response), m_open(other.m_open) {
        other.m_open = false;
    }

    AccordionScope& operator=(AccordionScope&& other) noexcept {
        if (this != &other) {
            ContainerScope::operator=(std::move(other));
            response = other.response;
            m_open = other.m_open;
            other.m_open = false;
        }
        return *this;
    }

    /// Evaluates to true ONLY when the accordion section is open, skipping body execution when collapsed.
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return m_open && m_node != nullptr;
    }

    [[nodiscard]] constexpr bool IsOpen() const noexcept { return m_open; }
    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    bool m_open{false};
};

/// Autonomous immediate-mode Accordion collapsible section container.
class Accordion {
public:
    [[nodiscard]] static AccordionScope Open(
        Context& ctx,
        const AccordionOptions& opts = {},
        std::source_location loc = std::source_location::current()
    ) {
        return Open(ctx, nullptr, opts, loc);
    }

    [[nodiscard]] static AccordionScope Open(
        Context& ctx,
        bool* externalOpen,
        const AccordionOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] static Vec2 MeasureHeader(
        const AccordionOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept;

    static void RenderHeader(
        ICanvas& canvas,
        Rect bounds,
        const AccordionOptions& opts,
        UiSize size,
        bool open,
        bool hovered,
        bool pressed,
        bool disabled,
        const UiTheme& theme
    ) noexcept;
};

} // namespace guikit
