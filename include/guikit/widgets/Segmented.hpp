#pragma once

#include <optional>
#include <span>
#include <source_location>
#include <string_view>
#include <vector>
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Individual segment item definition for Segmented control widget.
struct SegmentedItem {
    UiText text{};                                 ///< Segment label text (can be empty for icon-only segment)
    Icon icon{};                                   ///< Optional vector icon inside segment
    Placement iconPlacement{Placement::Leading};   ///< Icon position relative to text (Leading, Trailing, Top, Bottom)
    UiText tooltip{};                              ///< Optional per-segment tooltip on hover
    bool disabled{false};                          ///< Whether this individual segment is non-interactive and dimmed
};

/// Clean configuration options for Segmented control (button group / multi-segment switch) widget.
struct SegmentedOptions {
    UiText tooltip{};                              ///< Optional control-level floating tooltip on hover
    StateColor color{};                            ///< Active segment pill fill color (Default/None = elevated surface + accent border, or Primary/Success/etc.)
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    Align align{Align::Start};                     ///< Standalone horizontal control alignment (Start, Center, End)
    std::optional<float> width{std::nullopt};      ///< Control width: in points for Compact/Space (nullopt = auto from items), or ratio of bounds (0.0..1.0) for Fill (nullopt = 1.0)
    std::optional<float> height{std::nullopt};     ///< Explicit control height in points (nullopt = inherit defaultHeight from UiSize)
    std::optional<float> minWidth{std::nullopt};   ///< Minimum control width constraint in points
    StateColor background{StateColor::Surface()};  ///< Outer capsule/track surface fill color
    Border border{};                               ///< Outer capsule/track border stroke & corner radius
    bool equalWidth{true};                         ///< Give all segments equal width (true) or size each segment to its content (false)
    bool showDividers{true};                       ///< Render subtle vertical hairlines between unselected adjacent segments
    bool disabled{false};                          ///< Interaction suppression
};

/// Internal runtime state allocated in frame arena for Segmented nodes.
struct SegmentedData {
    SegmentedOptions opts{};
    std::span<const SegmentedItem> items{};
    size_t selectedIndex{size_t(-1)};
    uint32_t selectedMask{0};
    bool multiSelect{false};
    int hoveredSegment{-1};
    int pressedSegment{-1};
};

/// Autonomous Segmented control widget logic (execution, measuring, and rendering).
class Segmented {
public:
    /// Primary immediate mode entry point for single-select Segmented control with rich SegmentedItem elements.
    static Response Run(
        Context& ctx,
        size_t* selectedIndex,
        int* selectedIntIndex,
        uint32_t* selectedMask,
        std::span<const SegmentedItem> items,
        const SegmentedOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Convenience immediate mode entry point for Segmented control with plain string_view items.
    static Response Run(
        Context& ctx,
        size_t* selectedIndex,
        int* selectedIntIndex,
        uint32_t* selectedMask,
        std::span<const std::string_view> items,
        const SegmentedOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions of the segmented control based on segment items, icons, and sizing mode.
    static Vec2 Measure(
        std::span<const SegmentedItem> items,
        const SegmentedOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        std::span<const SegmentedItem> items,
        const SegmentedOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    /// Renders the segmented control track, dividers, active segment pills, icons, and labels.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        std::span<const SegmentedItem> items,
        size_t selectedIndex,
        uint32_t selectedMask,
        bool multiSelect,
        int hoveredSegment,
        int pressedSegment,
        const SegmentedOptions& opts,
        UiSize size,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark(),
        bool focused = false
    );
};

} // namespace guikit
