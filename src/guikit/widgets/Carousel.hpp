#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/Id.hpp"
#include "guikit/core/Theme.hpp"
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

/// Internal runtime state allocated in frame arena for Carousel container nodes.
struct CarouselData {
    CarouselOptions opts{};
    Rect leftArrowRect{0.0f, 0.0f, 0.0f, 0.0f};
    Rect rightArrowRect{0.0f, 0.0f, 0.0f, 0.0f};
    Rect viewportRect{0.0f, 0.0f, 0.0f, 0.0f};
    bool showLeftArrow{false};
    bool showRightArrow{false};
    bool leftArrowHovered{false};
    bool rightArrowHovered{false};
    bool leftArrowPressed{false};
    bool rightArrowPressed{false};
    bool scrollToActiveTab{false};
    bool showBaselineSeparator{false};
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

    /// Measures intrinsic dimensions for a Carousel container ribbon.
    static Vec2 Measure(const Node* node, ICanvas* canvas, const UiTheme& theme);
    static Vec2 Measure(const Node* node, ICanvas* canvas = nullptr);

    /// Custom flex row arrangement with dynamic gutters and scroll viewport.
    static void Arrange(Node* node, const Rect& availableRect);

    /// Renders navigation arrow buttons for an overflowing Carousel container (RenderFunc signature).
    static void RenderArrows(
        const Node* node,
        ICanvas& canvas,
        const UiTheme& theme
    );

    /// Backwards-compatible overload.
    static void RenderArrows(
        ICanvas& canvas,
        Node* node,
        UiSize size,
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
