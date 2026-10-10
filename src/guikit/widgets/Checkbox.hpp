#pragma once

#include <optional>
#include <source_location>
#include <string_view>
#include "guikit/core/Response.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for Checkbox widget.
struct CheckboxOptions {
    std::string_view label{};                      ///< Primary caption text
    std::string_view description{};                ///< Optional explanation text below label
    UiVariant variant{UiVariant::Primary};          ///< Accent fill color when checked
    Placement labelPlacement{Placement::Trailing}; ///< Placement of label relative to the checkbox square
    bool disabled{false};                          ///< Interaction suppression
    bool indeterminate{false};                     ///< Tri-state / partial check (horizontal dash)
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
};
static_assert(sizeof(CheckboxOptions) <= 40, "CheckboxOptions must stay compact");

/// Standardized layout and geometry metrics for a Checkbox widget.
struct CheckboxMetrics {
    float boxSize;
    float cornerRadius;
    float gap;
    float fontSize;
    float descFontSize;
    float descGap;
    float strokeWidth;
};

/// Returns standardized metrics for Checkbox according to UiSize.
[[nodiscard]] constexpr CheckboxMetrics GetCheckboxMetrics(UiSize size) noexcept {
    switch (size) {
        case UiSize::None:
        case UiSize::Md: return { 18.0f, 4.0f, 10.0f, 14.0f, 12.0f, 3.0f, 2.0f };
        case UiSize::Xs: return { 12.0f, 2.5f,  6.0f, 11.0f,  9.0f, 2.0f, 1.5f };
        case UiSize::Sm: return { 14.0f, 3.0f,  8.0f, 13.0f, 11.0f, 2.0f, 1.8f };
        case UiSize::Lg: return { 22.0f, 5.0f, 12.0f, 16.0f, 13.0f, 3.0f, 2.4f };
        case UiSize::Xl: return { 26.0f, 6.0f, 14.0f, 18.0f, 14.0f, 4.0f, 2.8f };
    }
    return { 18.0f, 4.0f, 10.0f, 14.0f, 12.0f, 3.0f, 2.0f };
}

/// Autonomous Checkbox widget logic (execution, measuring, and rendering).
class Checkbox {
public:
    /// Primary immediate mode entry point for Checkbox widget with state pointer and label.
    static Response Run(
        Context& ctx,
        std::string_view label,
        bool* value,
        bool checkedState,
        const CheckboxOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on box size, gap, and label text.
    static Vec2 Measure(
        std::string_view label,
        const CheckboxOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Renders the checkbox square, checkmark/dash, and optional label text.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        std::string_view label,
        bool checked,
        const CheckboxOptions& opts,
        UiSize size,
        bool hovered = false,
        bool pressed = false,
        bool disabled = false,
        ThemeMode theme = ThemeMode::Dark
    );
};

} // namespace guikit
