#pragma once

#include <optional>
#include "guikit/core/Id.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Persistent interaction state for a vertical or horizontal scrollbar.
struct ScrollbarState {
    bool dragging{false};
    bool hovered{false};
    float dragStartScroll{0.0f};
    float dragStartMouse{0.0f};
};

/// Computed geometry for a scrollbar track, thumb, and interactive hit zones.
struct ScrollbarGeometry {
    bool visible{false};
    float maxScroll{0.0f};
    float travel{0.0f};
    Rect trackRect{};
    Rect thumbRect{};
    Rect hitTrackRect{};
    Rect hitThumbRect{};
};

/// Deterministic scrollbar layout, interaction, and rendering primitive for scrollable widgets, panels, and popups.
class Scrollbar {
public:
    /// Computes vertical scrollbar geometry inside `viewportRect` for `contentHeight` and `scrollOffset`.
    [[nodiscard]] static ScrollbarGeometry ComputeVertical(
        Rect viewportRect,
        float contentHeight,
        float scrollOffset,
        const ScrollbarMetrics& metrics
    ) noexcept;

    /// Clamps `ioScrollOffset`, processes mouse wheel (when hovering `wheelHoverRect` or `viewportRect`),
    /// thumb dragging, and track page-jump clicks, and returns the updated `ScrollbarGeometry`.
    static ScrollbarGeometry UpdateVertical(
        Context& ctx,
        Id activeOwnerId,
        Rect viewportRect,
        float contentHeight,
        float& ioScrollOffset,
        ScrollbarState& ioState,
        const ScrollbarMetrics& metrics,
        std::optional<Rect> wheelHoverRect = std::nullopt
    ) noexcept;

    /// Renders a vertical scrollbar track and thumb into `canvas` according to `geom` and interaction state.
    static void DrawVertical(
        ICanvas& canvas,
        const ScrollbarGeometry& geom,
        bool hovered,
        bool dragging,
        const ScrollbarMetrics& metrics,
        const ThemePalette& pal,
        Color accentColor
    );
};

} // namespace guikit
