#pragma once

#include "guikit/context.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"

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
    keel::Optional<keel::f32> gap{keel::kNullOpt};         ///< Vertical gap between children inside expanded body (nullopt = from theme)
    keel::Optional<Padding> padding{keel::kNullOpt};   ///< Padding around expanded body children (nullopt = default top/bottom spacing)
    StateColor background{};                        ///< Per-state header backdrop color (Default = subtle surface bar, None = transparent, Surface/Primary/... = custom)
    Border border{};                                ///< Header border stroke & corner radius
    UiSize size{UiSize::Md};                        ///< Header height and typography size
    bool disabled{false};                           ///< Whether the accordion header and its children are disabled
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
        : ContainerScope(keel::move(other)), response(other.response), m_open(other.m_open) {
        other.m_open = false;
    }

    AccordionScope& operator=(AccordionScope&& other) noexcept {
        if (this != &other) {
            ContainerScope::operator=(keel::move(other));
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
namespace detail {

class Accordion {
public:
    [[nodiscard]] static AccordionScope Open(
        Context& ctx,
        const AccordionOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    ) {
        return Open(ctx, nullptr, opts, loc);
    }

    [[nodiscard]] static AccordionScope Open(
        Context& ctx,
        bool* externalOpen,
        const AccordionOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
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

} // namespace detail

[[nodiscard]] inline AccordionScope Accordion(
    Context& ui,
    AccordionOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::Accordion::Open(ui, nullptr, opts, loc);
}

[[nodiscard]] inline AccordionScope Accordion(
    Context& ui,
    keel::StringRef label,
    AccordionOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    opts.label.text = label;
    return detail::Accordion::Open(ui, nullptr, opts, loc);
}

[[nodiscard]] inline AccordionScope Accordion(
    Context& ui,
    bool& open,
    AccordionOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::Accordion::Open(ui, &open, opts, loc);
}

template <typename T, typename U>
    requires (!keel::IsSame<keel::Decay<T>, bool> && requires(T& a, const U& b) {
        static_cast<bool>(a == b);
        a = static_cast<T>(b);
    })
[[nodiscard]] AccordionScope Accordion(
    Context& ui,
    T& activeValue,
    const U& sectionValue,
    AccordionOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    bool isOpen = static_cast<bool>(activeValue == sectionValue);
    auto scope = detail::Accordion::Open(ui, &isOpen, opts, loc);
    if (scope.response.changed && isOpen) {
        activeValue = static_cast<T>(sectionValue);
    }
    return scope;
}

} // namespace guikit
