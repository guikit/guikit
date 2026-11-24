#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/id.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"
#include "keel/hash_map.h"
#include "keel/optional.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Configuration options for Carousel horizontal scroll container.
struct CarouselOptions {
    keel::Optional<keel::f32> gap{keel::kNullOpt};        ///< Gap between items in the ribbon (nullopt = from theme)
    keel::Optional<Padding> padding{keel::kNullOpt};   ///< Viewport content padding (nullopt = zero)
    Alignment align{Alignment::Center};             ///< Vertical alignment of items
    Justify justify{Justify::Start};                ///< Horizontal item distribution
    bool scrollWheel{true};                         ///< Enable horizontal mouse wheel scrolling
    UiSize size{UiSize::Md};                        ///< Navigation arrow & scrollbar size
    bool disabled{false};                           ///< Interaction suppression
};

/// Internal runtime state allocated in frame arena for Carousel container nodes.
struct CarouselData {
    CarouselOptions opts{};
    Rect leftArrowRect{0.0f, 0.0f, 0.0f, 0.0f};
    Rect rightArrowRect{0.0f, 0.0f, 0.0f, 0.0f};
    Rect viewportRect{0.0f, 0.0f, 0.0f, 0.0f};
    keel::f32 targetOffsetX{0.0f};
    keel::f32 baseTargetX{0.0f};
    keel::i8 stepDirection{0};
    bool animatingX{false};
    bool showLeftArrow{false};
    bool showRightArrow{false};
    bool leftArrowHovered{false};
    bool rightArrowHovered{false};
    bool leftArrowPressed{false};
    bool rightArrowPressed{false};
    bool scrollToActiveTab{false};
    bool autoCompact{false};
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
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions for a Carousel container ribbon.
    static Vec2 Measure(const Node* node, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme);
    static Vec2 Measure(const Node* node, keel::f32 availableWidth = 0.0f, ICanvas* canvas = nullptr);

    /// Custom flex row arrangement with dynamic gutters and scroll viewport.
    static void Arrange(Node* node, const Rect& availableRect, const UiTheme& theme);
    static void Arrange(Node* node, const Rect& availableRect);

    /// Returns the active visible viewport rectangle for children clipping.
    [[nodiscard]] static Rect GetContentClipRect(const Node* node);

    /// Shared arrow & wheel scroll interaction setup for Carousel and horizontal Toolbar nodes.
    static void BeginScrollInteraction(
        Context& ctx,
        Node* node,
        CarouselData* cData,
        bool scrollWheel,
        bool disabled
    );

    /// Synchronizes persistent scroll state with arrangement results.
    static void Update(Context& ctx, Node* node);

    /// Adjusts scroll offset so the target child node is visible within the viewport.
    static void ScrollToChild(Context& ctx, Node* carouselNode, const Node* targetNode);

    /// Registers interactive arrow bounds via the generic `PanelHooks::RecordBoundsFn` callback.
    static void CollectBounds(
        Node* node,
        void (*recordBounds)(void*, Id, Rect),
        void* user,
        const Rect& parentClip,
        const Transform2D& space
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
