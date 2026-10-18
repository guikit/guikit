#pragma once

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

/// Clean configuration options for Select (dropdown combo box) control widget.
struct SelectOptions {
    UiText placeholder{"Select..."};               ///< Placeholder text when no item is selected or items list is empty
    UiText tooltip{};                              ///< Optional floating tooltip text on hover
    UiAffix prefix{};                              ///< Optional default leading icon/text inside closed field
    UiAffix suffix{};                              ///< Optional default trailing text/icon inside closed field
    std::optional<uint32_t> maxVisibleItems{std::nullopt}; ///< Optional limit on visible items count before scrolling
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    Align align{Align::Start};                     ///< Standalone horizontal control alignment (Start, Center, End)
    std::optional<float> width{std::nullopt};      ///< Select box width: in points for Compact/Space (nullopt = auto from items), or % of bounds (0..100) for Fill (nullopt = 100%)
    std::optional<float> height{std::nullopt};     ///< Explicit select box height in points (nullopt = inherit defaultHeight from UiSize)
    std::optional<float> minWidth{std::nullopt};   ///< Minimum select box width constraint in points
    StateColor background{StateColor::Surface()};  ///< Select box surface fill color (Surface/Default = on, None = off, or custom StateColor)
    Border border{};                               ///< Select box border stroke & corner radius configuration
    bool disabled{false};                          ///< Interaction suppression
};

/// Internal runtime state allocated in frame arena for Select nodes.
struct SelectData {
    SelectOptions opts{};
    UiText displayText{};
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

    /// Renders the closed select frame with active text, prefix/suffix affixes, and chevron.
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
        bool focused = false
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
        bool scrollbarActive = false
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
        bool scrollbarActive = false
    );
};

} // namespace guikit
