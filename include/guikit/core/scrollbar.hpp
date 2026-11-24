#pragma once

#include "guikit/core/id.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

class Context;
class ICanvas;

/// Interactive sub-part of a scrollbar for hover & press visual feedback.
enum class ScrollbarPart : keel::u8 {
    None = 0,
    StartButton = 1, ///< Up (vertical) or Left (horizontal) stepper button
    EndButton   = 2, ///< Down (vertical) or Right (horizontal) stepper button
    Thumb       = 3, ///< Draggable thumb knob
    Track       = 4  ///< Track trough between buttons
};

/// Persistent interaction state for a vertical or horizontal scrollbar.
struct ScrollbarState {
    bool dragging{false};
    bool hovered{false};
    ScrollbarPart hoveredPart{ScrollbarPart::None};
    ScrollbarPart pressedPart{ScrollbarPart::None};
    keel::f32 dragStartScroll{0.0f};
    keel::f32 dragStartMouse{0.0f};
    keel::f64 nextRepeatSec{0.0};
};

/// Computed geometry for a scrollbar track, thumb, optional stepper buttons, and hit zones.
struct ScrollbarGeometry {
    bool visible{false};
    bool hasButtons{false};
    keel::f32 maxScroll{0.0f};
    keel::f32 travel{0.0f};
    Rect barRect{};
    Rect startButtonRect{};
    Rect endButtonRect{};
    Rect trackRect{};
    Rect thumbRect{};
    Rect hitTrackRect{};
    Rect hitThumbRect{};
};

/// Deterministic scrollbar layout, interaction, and rendering primitive for scrollable widgets, panels, and popups.
class Scrollbar {
public:
    /// Computes vertical or horizontal scrollbar geometry inside `viewportRect` respecting `style` buttons and `cornerReserve`.
    [[nodiscard]] static ScrollbarGeometry ComputeAxis(
        Rect viewportRect,
        keel::f32 contentLength,
        keel::f32 scrollOffset,
        bool isHorizontal,
        const ScrollbarMetrics& metrics,
        const ScrollbarStyleTokens& style,
        keel::f32 cornerReserve = 0.0f,
        bool alwaysVisible = false
    ) noexcept;

    /// Computes vertical scrollbar geometry inside `viewportRect` for `contentHeight` and `scrollOffset`.
    [[nodiscard]] static ScrollbarGeometry ComputeVertical(
        Rect viewportRect,
        keel::f32 contentHeight,
        keel::f32 scrollOffset,
        const ScrollbarMetrics& metrics,
        const ScrollbarStyleTokens& style = ScrollbarStyleTokens::Modern(),
        keel::f32 cornerReserve = 0.0f
    ) noexcept;

    /// Clamps `ioScrollOffset`, processes mouse wheel (when hovering `wheelHoverRect` or `viewportRect`),
    /// stepper buttons, thumb dragging, and track page-jump clicks, and returns the updated `ScrollbarGeometry`.
    static ScrollbarGeometry UpdateVertical(
        Context& ctx,
        Id activeOwnerId,
        Rect viewportRect,
        keel::f32 contentHeight,
        keel::f32& ioScrollOffset,
        ScrollbarState& ioState,
        const ScrollbarMetrics& metrics,
        keel::Optional<Rect> wheelHoverRect = keel::kNullOpt
    ) noexcept;

    /// Renders a vertical or horizontal scrollbar through `theme.Painter().DrawScrollbar(...)`.
    static void DrawAxis(
        ICanvas& canvas,
        const ScrollbarGeometry& geom,
        bool isHorizontal,
        bool hovered,
        bool dragging,
        ScrollbarPart hoveredPart,
        ScrollbarPart pressedPart,
        const ScrollbarMetrics& metrics,
        const UiTheme& theme,
        Color accentColor = Color::Transparent()
    );

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

    /// Theme-aware overload rendering vertical scrollbar via `theme.Painter().DrawScrollbar(...)`.
    static void DrawVertical(
        ICanvas& canvas,
        const ScrollbarGeometry& geom,
        const ScrollbarState& state,
        const ScrollbarMetrics& metrics,
        const UiTheme& theme,
        Color accentColor = Color::Transparent()
    );
};

} // namespace guikit
