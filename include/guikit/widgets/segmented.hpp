#pragma once

#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "keel/string.h"

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
    keel::Optional<float> width{keel::kNullOpt};      ///< Control width in dp (or legacy ratio 0.0..1.0 in Fill)
    keel::Optional<float> widthRatio{keel::kNullOpt}; ///< Normalized fill ratio 0.0..1.0 of bounds in Fill mode
    keel::Optional<float> height{keel::kNullOpt};     ///< Explicit control height in points (nullopt = inherit defaultHeight from UiSize)
    keel::Optional<float> minWidth{keel::kNullOpt};   ///< Minimum control width constraint in points
    StateColor background{StateColor::Surface()};  ///< Outer capsule/track surface fill color
    Border border{};                               ///< Outer capsule/track border stroke & corner radius
    bool equalWidth{true};                         ///< Give all segments equal width (true) or size each segment to its content (false)
    bool showDividers{true};                       ///< Render subtle vertical hairlines between unselected adjacent segments
    bool disabled{false};                          ///< Interaction suppression
    const UiTheme* theme{nullptr};                 ///< Optional scoped theme override
    keel::Optional<float> scale{keel::kNullOpt};      ///< Optional scoped scale factor override
};

/// Internal runtime state allocated in frame arena for Segmented nodes.
struct SegmentedData {
    SegmentedOptions opts{};
    keel::ArrayRef<SegmentedItem> items{};
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
        keel::ArrayRef<SegmentedItem> items,
        const SegmentedOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Convenience immediate mode entry point for Segmented control with plain StringRef items.
    static Response Run(
        Context& ctx,
        size_t* selectedIndex,
        int* selectedIntIndex,
        uint32_t* selectedMask,
        keel::ArrayRef<keel::StringRef> items,
        const SegmentedOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions of the segmented control based on segment items, icons, and sizing mode.
    static Vec2 Measure(
        keel::ArrayRef<SegmentedItem> items,
        const SegmentedOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        keel::ArrayRef<SegmentedItem> items,
        const SegmentedOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    /// Renders the segmented control track, dividers, active segment pills, icons, and labels.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        keel::ArrayRef<SegmentedItem> items,
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
