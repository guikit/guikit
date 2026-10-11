#pragma once

#include <optional>
#include <span>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for Select (dropdown combo box) widget.
struct SelectOptions {
    std::string_view label{};                      ///< Primary caption text
    std::string_view description{};                ///< Optional helper text below select box
    std::string_view placeholder{"Select..."};     ///< Placeholder text when no item is selected or items list is empty
    UiVariant variant{UiVariant::Default};         ///< Accent/border variant (Default, Warning, Danger, etc.)
    Placement labelPlacement{Placement::Stacked};  ///< Placement of label (Stacked by default for form inputs)
    Icon prefixIcon{};                             ///< Optional vector icon inside field on left
    std::optional<uint32_t> maxVisibleItems{std::nullopt}; ///< Optional limit on visible items count before scrolling
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
    bool disabled{false};                          ///< Interaction suppression
};

/// Internal runtime state allocated in frame arena for Select nodes.
struct SelectData {
    SelectOptions opts{};
    std::string_view displayText{};
};

/// Autonomous Select (dropdown combo box) widget logic.
class Select {
public:
    /// Primary immediate mode entry point for Select with index and string pointers.
    static Response Run(
        Context& ctx,
        std::string_view label,
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
        std::string_view label,
        const SelectOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    );
    static Vec2 Measure(
        std::string_view label,
        const SelectOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Computes the exact bounding box for the closed select field within overall bounds.
    static Rect GetBoxRect(
        Rect bounds,
        std::string_view label,
        const SelectOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        const UiTheme& theme = UiTheme::Dark()
    );

    /// Renders the closed select frame with active text, prefix icon, and chevron.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        std::string_view label,
        std::string_view currentText,
        const SelectOptions& opts,
        UiSize size,
        bool hovered = false,
        bool open = false,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark()
    );

    /// Renders the floating popup menu containing dropdown items.
    static void RenderPopup(
        ICanvas& canvas,
        Rect popupRect,
        std::span<const std::string_view> items,
        size_t selectedIndex,
        int hoveredIndex,
        const SelectOptions& opts,
        UiSize size,
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
