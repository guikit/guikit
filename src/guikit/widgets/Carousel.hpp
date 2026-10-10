#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/Id.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;
class ContainerScope;
using CarouselScope = ContainerScope;
struct Node;

/// Configuration options for Carousel horizontal scroll container.
struct CarouselOptions {
    std::optional<UiSize> size{std::nullopt};       ///< Size token determining arrow sizes and baseline height
    std::optional<UiSize> gap{std::nullopt};        ///< Gap between items in the ribbon (nullopt = inherit size)
    std::optional<UiSize> padding{std::nullopt};    ///< Internal padding token (nullopt = zero)
    bool scrollWheel{true};                         ///< Enable horizontal mouse wheel scrolling
    bool disabled{false};                           ///< Interaction suppression
};

/// Deterministic 64-bit ID for Carousel left navigation arrow button.
[[nodiscard]] inline constexpr Id GetCarouselLeftArrowId(Id carouselId) noexcept {
    return Id(detail::HashInt64(0xFF01, carouselId.value));
}

/// Deterministic 64-bit ID for Carousel right navigation arrow button.
[[nodiscard]] inline constexpr Id GetCarouselRightArrowId(Id carouselId) noexcept {
    return Id(detail::HashInt64(0xFF02, carouselId.value));
};

/// Autonomous Carousel widget logic (execution, measuring, and rendering).
class Carousel {
public:
    /// Primary immediate mode entry point for Carousel container.
    static CarouselScope Open(
        Context& ctx,
        const CarouselOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Renders navigation arrow buttons for an overflowing Carousel container.
    static void RenderArrows(
        ICanvas& canvas,
        Node* node,
        UiSize size,
        ThemeMode theme
    );
};

} // namespace guikit
