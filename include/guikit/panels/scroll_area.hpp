#pragma once

#include "guikit/context.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/id.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;
struct PanelHooks;

/// Persistent scroll state for ScrollArea and 2D viewports.
struct ScrollState {
    Vec2 offset{0.0f, 0.0f};
    bool draggingY{false};
    bool draggingX{false};
    keel::f32 dragStartOffset{0.0f};
    keel::f32 dragStartOffsetX{0.0f};
    Vec2 dragStartMouse{0.0f, 0.0f};
    keel::f32 targetOffsetX{0.0f};
    bool animatingX{false};
    keel::f64 lastAnimTimeSec{0.0};
    keel::u8 pressedButtonY{0};
    keel::u8 pressedButtonX{0};
    keel::f64 nextRepeatSecY{0.0};
    keel::f64 nextRepeatSecX{0.0};
};

/// Options for configuring a ScrollArea viewport.
struct ScrollAreaOptions {
    Orientation orientation{Orientation::Vertical};
    VisibilityPolicy scrollbar{VisibilityPolicy::Auto};
    Alignment align{Alignment::Stretch};            ///< Cross-axis alignment inside scroll content rect
    Justify justify{Justify::Start};                ///< Main-axis justification when content is smaller than viewport
    keel::Optional<keel::f32> gap{keel::kNullOpt};        ///< Gap between consecutive scroll items (nullopt = zero)
    keel::Optional<Padding> padding{keel::kNullOpt};   ///< Viewport content padding (nullopt = zero)
    AxisSize width{SizeMode::Stretch};                ///< Horizontal sizing policy (`Stretch`, `Fit`, or fixed `dp`)
    AxisSize height{SizeMode::Stretch};               ///< Vertical sizing policy (`Stretch`, `Fit`, or fixed `dp`)
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};   ///< Maximum viewport height in dp (shrink-wraps content up to maxHeight)
    StateColor background{};
    Border border{};
    UiSize size{UiSize::Md};                          ///< Scrollbar thickness/gutter size
    bool disabled{false};
};

/// ScrollArea container logic and shared 1D/2D scrollbar primitives.
namespace detail {

class ScrollArea {
public:
    /// Primary immediate mode entry point for ScrollArea container.
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const ScrollAreaOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Returns the active visible viewport rectangle (computed bounds minus padding and gutter).
    [[nodiscard]] static Rect GetContentClipRect(const Node* node, const UiTheme& theme);

    /// Updates scrollbar hover, dragging, and paging interactions for this frame.
    static void Update(Context& ctx, Node* node);

    /// Adjusts scroll offset so the target child node is visible within the viewport.
    static void ScrollToChild(Context& ctx, Node* scrollNode, const Node* targetNode);

    /// Returns static panel lifecycle hooks for solver-managed scroll containers.
    [[nodiscard]] static const PanelHooks* Hooks() noexcept;

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
        keel::f32 contentLength,
        keel::f32 scrollOffset,
        bool isHorizontal,
        bool hovered,
        bool active,
        UiSize size,
        const UiTheme& theme,
        keel::f32 cornerReserve = 0.0f,
        keel::u8 hoveredPart = 0u,
        keel::u8 pressedPart = 0u
    );

    /// Shared primitive: updates interaction (stepper buttons, hover, thumb drag, track page jump) for a single scrollbar axis.
    static void UpdateAxisScrollbar(
        Context& ctx,
        Id ownerId,
        Rect viewportRect,
        keel::f32 contentLength,
        keel::f32& ioScrollOffset,
        bool& ioDragging,
        keel::f32& ioDragStartOffset,
        Vec2& ioDragStartMouse,
        bool& outHovered,
        bool& outActive,
        bool isHorizontal,
        UiSize size,
        const UiTheme& theme,
        keel::f32 cornerReserve = 0.0f,
        keel::u8* outHoveredPart = nullptr,
        keel::u8* ioPressedButton = nullptr,
        keel::f64* ioNextRepeatSec = nullptr
    );
};

} // namespace detail

[[nodiscard]] inline ContainerScope ScrollArea(
    Context& ui,
    ScrollAreaOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::ScrollArea::Open(ui, opts, loc);
}

/// Renders scrollbar thumb and track after children (postRenderFn signature).
inline void RenderScrollbar(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme = UiTheme::Dark()
) {
    detail::ScrollArea::RenderScrollbar(node, canvas, theme);
}

} // namespace guikit
