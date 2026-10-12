#pragma once

#include <optional>
#include <source_location>
#include <unordered_map>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Id.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Configuration options for Carousel horizontal scroll container.
struct CarouselOptions {
    std::optional<float> gap{std::nullopt};        ///< Gap between items in the ribbon (nullopt = from theme)
    std::optional<Padding> padding{std::nullopt};   ///< Viewport content padding (nullopt = zero)
    Alignment align{Alignment::Center};             ///< Vertical alignment of items
    Justify justify{Justify::Start};                ///< Horizontal item distribution
    bool scrollWheel{true};                         ///< Enable horizontal mouse wheel scrolling
    std::optional<UiSize> size{std::nullopt};       ///< Size token determining arrow sizes and baseline height
    bool disabled{false};                           ///< Interaction suppression
    const UiTheme* theme{nullptr};                  ///< Optional scoped theme override
    std::optional<float> scale{std::nullopt};       ///< Optional scoped scale factor override
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
}

/// Autonomous Carousel container logic (measuring, arrangement, and navigation overlay).
class Carousel {
public:
    /// Primary immediate mode entry point for Carousel container.
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const CarouselOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions for a Carousel container ribbon.
    static Vec2 Measure(const Node* node, float availableWidth, ICanvas* canvas, const UiTheme& theme);
    static Vec2 Measure(const Node* node, float availableWidth = 0.0f, ICanvas* canvas = nullptr);

    /// Custom flex row arrangement with dynamic gutters and scroll viewport.
    static void Arrange(Node* node, const Rect& availableRect, const UiTheme& theme);
    static void Arrange(Node* node, const Rect& availableRect);

    /// Returns the active visible viewport rectangle for children clipping.
    [[nodiscard]] static Rect GetContentClipRect(const Node* node);

    /// Synchronizes persistent scroll state with arrangement results.
    static void Update(Context& ctx, Node* node);

    /// Adjusts scroll offset so the target child node is visible within the viewport.
    static void ScrollToChild(Context& ctx, Node* carouselNode, const Node* targetNode);

    /// Registers interactive arrow bounds into the bounds map for instantaneous hit testing.
    static void CollectBounds(
        const Node* node,
        std::unordered_map<Id, Rect>& outBounds,
        Rect parentClip
    );

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
