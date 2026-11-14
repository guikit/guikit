#pragma once

#include <cstddef>
#include <cstdint>
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "keel/static_array.h"
#include "keel/string.h"

namespace guikit {

class Context;
class ICanvas;

/// Individual item definition for Select dropdown widget.
struct SelectItem {
    UiText text{};           ///< Primary item label text (default role: LabelMedium)
    UiAffix prefix{};        ///< Optional leading icon and/or text on the left of this item
    UiAffix suffix{};        ///< Optional trailing text and/or icon on the right of this item
    bool disabled{false};    ///< Whether this item is non-selectable and dimmed
};

/// Clean configuration options for Select, ComboBox, and Autocomplete control widgets.
struct SelectOptions {
    UiText placeholder{"Select..."};               ///< Placeholder text when no item is selected or input is empty
    UiText emptyText{"No matching items"};         ///< Text displayed inside popup when search filter yields no matches
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    UiAffix prefix{};                              ///< Optional default leading icon/text inside closed field
    UiAffix suffix{};                              ///< Optional default trailing text/icon inside closed field
    keel::Optional<uint32_t> maxVisibleItems{keel::kNullOpt}; ///< Optional limit on visible items count before scrolling
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    Align align{Align::Start};                     ///< Standalone horizontal control alignment (Start, Center, End)
    keel::Optional<float> width{keel::kNullOpt};      ///< Select box width in dp (or legacy ratio 0.0..1.0 in Fill)
    keel::Optional<float> widthRatio{keel::kNullOpt}; ///< Normalized fill ratio 0.0..1.0 of bounds in Fill mode
    keel::Optional<float> height{keel::kNullOpt};     ///< Explicit select box height in points (nullopt = inherit defaultHeight from UiSize)
    keel::Optional<float> minWidth{keel::kNullOpt};   ///< Minimum select box width constraint in points
    StateColor background{StateColor::Surface()};  ///< Select box surface fill color (Surface/Default = on, None = off, or custom StateColor)
    Border border{};                               ///< Select box border stroke & corner radius configuration
    bool searchable{false};                        ///< Enable live keyboard search/filtering of dropdown items (ComboBox mode)
    bool editable{false};                          ///< Enable free-form text input with dropdown suggestions (Autocomplete mode)
    bool clearable{false};                         ///< Show interactive clear (X) button when a selection or text is present
    keel::Optional<CursorBlinkMode> cursorMode{keel::kNullOpt}; ///< Optional override for text cursor blink mode in searchable/editable modes
    bool disabled{false};                          ///< Interaction suppression
    const UiTheme* theme{nullptr};                 ///< Optional scoped theme override
    keel::Optional<float> scale{keel::kNullOpt};      ///< Optional scoped scale factor override
};

/// Visual chip data stored inside frame arena for MultiSelect nodes.
struct SelectChipVisual {
    UiText text{};
    UiAffix prefix{};
    size_t itemIndex{0};
    bool removeHovered{false};
};

/// Internal runtime state allocated in frame arena for Select / ComboBox / Autocomplete / MultiSelect nodes.
struct SelectData {
    static constexpr size_t kMaxChips = 16;

    SelectOptions opts{};
    UiText displayText{};
    size_t cursor{0};
    size_t selStart{0};
    size_t selEnd{0};
    float scrollX{0.0f};
    float cursorWidth{1.5f};
    bool isEditing{false};
    bool cursorVisible{false};
    bool showClearBtn{false};
    bool clearHovered{false};
    bool chevronHovered{false};
    bool isMulti{false};
    keel::StaticArray<SelectChipVisual, kMaxChips> chips{};
    size_t chipCount{0};
    size_t totalSelectedCount{0};
};

/// Autonomous Select (dropdown combo box) widget logic.
class Select {
public:
    /// Primary immediate mode entry point for Select with rich SelectItem elements.
    static Response Run(
        Context& ctx,
        size_t* selectedIndex,
        int* selectedIntIndex,
        keel::Optional<keel::StringSink> selectedString,
        keel::StringRef* selectedStringView,
        keel::ArrayRef<SelectItem> items,
        const SelectOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Convenience immediate mode entry point for Select with plain StringRef items.
    static Response Run(
        Context& ctx,
        size_t* selectedIndex,
        int* selectedIntIndex,
        keel::Optional<keel::StringSink> selectedString,
        keel::StringRef* selectedStringView,
        keel::ArrayRef<keel::StringRef> items,
        const SelectOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Multi-selection immediate mode entry point with plain StringRef items.
    [[nodiscard]] static Response RunMulti(
        Context& ctx,
        IndexListSink selectedIndices,
        keel::ArrayRef<keel::StringRef> items,
        const SelectOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Multi-selection immediate mode entry point with rich SelectItem elements.
    [[nodiscard]] static Response RunMulti(
        Context& ctx,
        IndexListSink selectedIndices,
        keel::ArrayRef<SelectItem> items,
        const SelectOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions based on content, label, and placement.
    static Vec2 Measure(
        const SelectOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const SelectOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    /// Computes the exact bounding box for the closed select field within overall bounds.
    static Rect GetBoxRect(
        Rect bounds,
        const SelectOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        const UiTheme& theme = UiTheme::Dark()
    );

    /// Renders the closed select frame with active text, prefix/suffix affixes, clear button, and chevron.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        UiText currentText,
        const SelectOptions& opts,
        UiSize size,
        bool hovered = false,
        bool open = false,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark(),
        bool focused = false,
        bool isEditing = false,
        size_t cursor = 0,
        bool cursorVisible = false,
        float cursorWidth = 1.5f,
        size_t selStart = 0,
        size_t selEnd = 0,
        float scrollX = 0.0f,
        bool showClearBtn = false,
        bool clearHovered = false,
        bool chevronHovered = false
    );

    /// Renders the floating popup menu containing rich SelectItem dropdown items.
    static void RenderPopup(
        ICanvas& canvas,
        Rect popupRect,
        keel::ArrayRef<SelectItem> items,
        size_t selectedIndex,
        int hoveredIndex,
        const SelectOptions& opts,
        UiSize size,
        const UiTheme& theme = UiTheme::Dark(),
        float scrollY = 0.0f,
        bool scrollbarHovered = false,
        bool scrollbarActive = false,
        keel::StringRef highlightQuery = {}
    );

    /// Convenience overload rendering popup menu from plain StringRef items.
    static void RenderPopup(
        ICanvas& canvas,
        Rect popupRect,
        keel::ArrayRef<keel::StringRef> items,
        size_t selectedIndex,
        int hoveredIndex,
        const SelectOptions& opts,
        UiSize size,
        const UiTheme& theme = UiTheme::Dark(),
        float scrollY = 0.0f,
        bool scrollbarHovered = false,
        bool scrollbarActive = false,
        keel::StringRef highlightQuery = {}
    );
};

} // namespace guikit
