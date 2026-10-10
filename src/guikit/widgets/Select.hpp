#pragma once

#include <optional>
#include <span>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>
#include "guikit/core/Response.hpp"
#include "guikit/core/Tokens.hpp"
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

/// Standardized layout and geometry metrics for a Select widget.
struct SelectMetrics {
    float defaultHeight;
    float cornerRadius;
    float paddingX;
    float fontSize;
    float labelFontSize;
    float descFontSize;
    float labelGap;
    float descGap;
    float strokeWidth;
    float iconSize;
    float itemHeight;
};

/// Returns standardized metrics for Select according to UiSize.
[[nodiscard]] constexpr SelectMetrics GetSelectMetrics(UiSize size) noexcept {
    switch (size) {
        case UiSize::None:
        case UiSize::Md: return { 36.0f, 6.0f, 12.0f, 14.0f, 13.0f, 12.0f, 4.0f, 3.0f, 1.0f, 16.0f, 32.0f };
        case UiSize::Xs: return { 24.0f, 4.0f,  8.0f, 11.0f, 10.0f,  9.0f, 3.0f, 2.0f, 1.0f, 12.0f, 22.0f };
        case UiSize::Sm: return { 28.0f, 5.0f, 10.0f, 13.0f, 12.0f, 11.0f, 3.0f, 2.0f, 1.0f, 14.0f, 26.0f };
        case UiSize::Lg: return { 44.0f, 8.0f, 16.0f, 16.0f, 15.0f, 13.0f, 5.0f, 3.0f, 1.5f, 18.0f, 38.0f };
        case UiSize::Xl: return { 52.0f, 10.0f, 20.0f, 18.0f, 16.0f, 14.0f, 6.0f, 4.0f, 1.5f, 22.0f, 44.0f };
    }
    return { 36.0f, 6.0f, 12.0f, 14.0f, 13.0f, 12.0f, 4.0f, 3.0f, 1.0f, 16.0f, 32.0f };
}

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
        ICanvas* canvas = nullptr
    );

    /// Computes the exact bounding box for the closed select field within overall bounds.
    static Rect GetBoxRect(
        Rect bounds,
        std::string_view label,
        const SelectOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
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
        ThemeMode theme = ThemeMode::Dark
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
        ThemeMode theme = ThemeMode::Dark
    );
};

} // namespace guikit
