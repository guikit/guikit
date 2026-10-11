#pragma once

#include <optional>
#include <source_location>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Persistent scroll state for ScrollArea and Carousel viewports.
struct ScrollState {
    Vec2 offset{0.0f, 0.0f};
    bool draggingY{false};
    bool draggingX{false};
    float dragStartOffset{0.0f};
    Vec2 dragStartMouse{0.0f, 0.0f};
};

/// Options for configuring a ScrollArea viewport.
struct ScrollAreaOptions {
    Orientation orientation{Orientation::Vertical};
    VisibilityPolicy scrollbar{VisibilityPolicy::Auto};
    std::optional<UiSize> gap{std::nullopt};        ///< Gap between consecutive scroll items (nullopt = zero)
    std::optional<Padding> padding{std::nullopt};   ///< Viewport content padding (nullopt = zero)
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

/// ScrollArea container logic and scrollbar rendering.
class ScrollArea {
public:
    /// Primary immediate mode entry point for ScrollArea container.
    static ContainerScope Open(
        Context& ctx,
        const ScrollAreaOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Renders scrollbar thumb and track after children (postRenderFn signature).
    static void RenderScrollbar(
        const Node* node,
        ICanvas& canvas,
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
