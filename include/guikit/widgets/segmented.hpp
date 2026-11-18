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
    AxisSize width{};                              ///< Horizontal sizing policy (`Auto` -> `Stretch`, `Fit` in `Toolbar`, or `Fixed(dp)`)
    AxisSize height{};                             ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or `Fixed(dp)`)
    keel::Optional<float> minWidth{keel::kNullOpt};   ///< Minimum control width constraint in points
    StateColor background{StateColor::Surface()};  ///< Outer capsule/track surface fill color
    Border border{};                               ///< Outer capsule/track border stroke & corner radius
    UiSize size{UiSize::Md};                       ///< Component size scale
    bool equalWidth{true};                         ///< Give all segments equal width (true) or size each segment to its content (false)
    bool showDividers{true};                       ///< Render subtle vertical hairlines between unselected adjacent segments
    bool disabled{false};                          ///< Interaction suppression
};

/// Internal runtime state allocated in frame arena for Segmented nodes.
struct SegmentedData {
    SegmentedOptions opts{};
    keel::ArrayRef<SegmentedItem> items{};
    keel::usize selectedIndex{keel::usize(-1)};
    keel::u32 selectedMask{0};
    bool multiSelect{false};
    keel::i32 hoveredSegment{-1};
    keel::i32 pressedSegment{-1};
};

/// Autonomous Segmented control widget logic (execution, measuring, and rendering).
class Segmented {
public:
    /// Primary immediate mode entry point for single-select Segmented control with rich SegmentedItem elements.
    static Response Run(
        Context& ctx,
        keel::usize* selectedIndex,
        keel::i32* selectedIntIndex,
        keel::u32* selectedMask,
        keel::ArrayRef<SegmentedItem> items,
        const SegmentedOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Convenience immediate mode entry point for Segmented control with plain StringRef items.
    static Response Run(
        Context& ctx,
        keel::usize* selectedIndex,
        keel::i32* selectedIntIndex,
        keel::u32* selectedMask,
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
        keel::usize selectedIndex,
        keel::u32 selectedMask,
        bool multiSelect,
        keel::i32 hoveredSegment,
        keel::i32 pressedSegment,
        const SegmentedOptions& opts,
        UiSize size,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark(),
        bool focused = false
    );
};

} // namespace guikit
