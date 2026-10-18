#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Id.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Persistent scroll state for ScrollArea and 2D viewports.
struct ScrollState {
    Vec2 offset{0.0f, 0.0f};
    bool draggingY{false};
    bool draggingX{false};
    float dragStartOffset{0.0f};
    float dragStartOffsetX{0.0f};
    Vec2 dragStartMouse{0.0f, 0.0f};
};

/// Options for configuring a ScrollArea viewport.
struct ScrollAreaOptions {
    Orientation orientation{Orientation::Vertical};
    VisibilityPolicy scrollbar{VisibilityPolicy::Auto};
    std::optional<float> gap{std::nullopt};        ///< Gap between consecutive scroll items (nullopt = zero)
    std::optional<Padding> padding{std::nullopt};   ///< Viewport content padding (nullopt = zero)
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

/// ScrollArea container logic and shared 1D/2D scrollbar primitives.
class ScrollArea {
public:
    /// Primary immediate mode entry point for ScrollArea container.
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const ScrollAreaOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Returns the active visible viewport rectangle (computed bounds minus padding and gutter).
    [[nodiscard]] static Rect GetContentClipRect(const Node* node, const UiTheme& theme);

    /// Updates scrollbar hover, dragging, and paging interactions for this frame.
    static void Update(Context& ctx, Node* node);

    /// Adjusts scroll offset so the target child node is visible within the viewport.
    static void ScrollToChild(Context& ctx, Node* scrollNode, const Node* targetNode);

    /// Renders scrollbar thumb and track after children (postRenderFn signature).
    static void RenderScrollbar(
        const Node* node,
        ICanvas& canvas,
        const UiTheme& theme = UiTheme::Dark()
    );

    /// Shared primitive: renders a vertical or horizontal scrollbar along the edge of `viewportRect`.
    static void RenderAxisScrollbar(
        ICanvas& canvas,
        Rect viewportRect,
        float contentLength,
        float scrollOffset,
        bool isHorizontal,
        bool hovered,
        bool active,
        UiSize size,
        const UiTheme& theme,
        float cornerReserve = 0.0f
    );

    /// Shared primitive: updates interaction (hover, thumb drag, track page jump) for a single scrollbar axis.
    static void UpdateAxisScrollbar(
        Context& ctx,
        Id ownerId,
        Rect viewportRect,
        float contentLength,
        float& ioScrollOffset,
        bool& ioDragging,
        float& ioDragStartOffset,
        Vec2& ioDragStartMouse,
        bool& outHovered,
        bool& outActive,
        bool isHorizontal,
        UiSize size,
        const UiTheme& theme,
        float cornerReserve = 0.0f
    );
};

} // namespace guikit
