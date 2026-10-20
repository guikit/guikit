#pragma once

#include <array>
#include <optional>
#include <span>
#include <source_location>
#include <string>
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
    std::optional<uint32_t> maxVisibleItems{std::nullopt}; ///< Optional limit on visible items count before scrolling
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    Align align{Align::Start};                     ///< Standalone horizontal control alignment (Start, Center, End)
    std::optional<float> width{std::nullopt};      ///< Select box width: in points for Compact/Space (nullopt = auto from items), or ratio of bounds (0.0..1.0) for Fill (nullopt = 1.0)
    std::optional<float> height{std::nullopt};     ///< Explicit select box height in points (nullopt = inherit defaultHeight from UiSize)
    std::optional<float> minWidth{std::nullopt};   ///< Minimum select box width constraint in points
    StateColor background{StateColor::Surface()};  ///< Select box surface fill color (Surface/Default = on, None = off, or custom StateColor)
    Border border{};                               ///< Select box border stroke & corner radius configuration
    bool searchable{false};                        ///< Enable live keyboard search/filtering of dropdown items (ComboBox mode)
    bool editable{false};                          ///< Enable free-form text input with dropdown suggestions (Autocomplete mode)
    bool clearable{false};                         ///< Show interactive clear (X) button when a selection or text is present
    std::optional<CursorBlinkMode> cursorMode{std::nullopt}; ///< Optional override for text cursor blink mode in searchable/editable modes
    bool disabled{false};                          ///< Interaction suppression
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
    std::array<SelectChipVisual, kMaxChips> chips{};
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
        std::string* selectedString,
        std::string_view* selectedStringView,
        std::span<const SelectItem> items,
        const SelectOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Convenience immediate mode entry point for Select with plain string_view items.
    static Response Run(
        Context& ctx,
        size_t* selectedIndex,
        int* selectedIntIndex,
        std::string* selectedString,
        std::string_view* selectedStringView,
        std::span<const std::string_view> items,
        const SelectOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Multi-selection immediate mode entry point with plain string_view items.
    [[nodiscard]] static Response RunMulti(
        Context& ctx,
        std::vector<size_t>& selectedIndices,
        std::span<const std::string_view> items,
        const SelectOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Multi-selection immediate mode entry point with rich SelectItem elements.
    [[nodiscard]] static Response RunMulti(
        Context& ctx,
        std::vector<size_t>& selectedIndices,
        std::span<const SelectItem> items,
        const SelectOptions& opts = {},
        std::source_location loc = std::source_location::current()
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
        std::span<const SelectItem> items,
        size_t selectedIndex,
        int hoveredIndex,
        const SelectOptions& opts,
        UiSize size,
        const UiTheme& theme = UiTheme::Dark(),
        float scrollY = 0.0f,
        bool scrollbarHovered = false,
        bool scrollbarActive = false,
        std::string_view highlightQuery = {}
    );

    /// Convenience overload rendering popup menu from plain string_view items.
    static void RenderPopup(
        ICanvas& canvas,
        Rect popupRect,
        std::span<const std::string_view> items,
        size_t selectedIndex,
        int hoveredIndex,
        const SelectOptions& opts,
        UiSize size,
        const UiTheme& theme = UiTheme::Dark(),
        float scrollY = 0.0f,
        bool scrollbarHovered = false,
        bool scrollbarActive = false,
        std::string_view highlightQuery = {}
    );
};

} // namespace guikit
