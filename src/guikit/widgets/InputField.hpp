#pragma once

#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include "guikit/core/Response.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for InputField widget.
struct InputFieldOptions {
    std::string_view label{};                      ///< Primary caption text
    std::string_view description{};                ///< Optional helper text below input box
    std::string_view placeholder{};                ///< Placeholder text when empty (e.g. "Enter value...")
    std::string_view prefix{};                     ///< Symbol/text inside field on left (e.g. "$", "±")
    std::string_view suffix{};                     ///< Unit/symbol inside field on right (e.g. "%", "mm", "MB/s")
    std::string_view format{"%.2f"};               ///< Format string for floating-point values
    UiVariant variant{UiVariant::Default};         ///< Accent/border variant (Default, Warning, Danger, etc.)
    Placement labelPlacement{Placement::Stacked};  ///< Placement of label (Stacked by default for form inputs)
    Icon prefixIcon{};                             ///< Optional vector icon inside field on left
    Icon suffixIcon{};                             ///< Optional vector icon inside field on right
    std::optional<CursorBlinkMode> cursorMode{std::nullopt}; ///< Cursor blink override (inherits context config if nullopt)
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
    bool disabled{false};                          ///< Interaction suppression
    bool readOnly{false};                          ///< Non-editable display mode
};

/// Standardized layout and geometry metrics for an InputField widget.
struct InputFieldMetrics {
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
};

/// Returns standardized metrics for InputField according to UiSize.
[[nodiscard]] constexpr InputFieldMetrics GetInputFieldMetrics(UiSize size) noexcept {
    switch (size) {
        case UiSize::None:
        case UiSize::Md: return { 36.0f, 6.0f, 12.0f, 14.0f, 13.0f, 12.0f, 4.0f, 3.0f, 1.0f, 16.0f };
        case UiSize::Xs: return { 24.0f, 4.0f,  8.0f, 11.0f, 10.0f,  9.0f, 3.0f, 2.0f, 1.0f, 12.0f };
        case UiSize::Sm: return { 28.0f, 5.0f, 10.0f, 13.0f, 12.0f, 11.0f, 3.0f, 2.0f, 1.0f, 14.0f };
        case UiSize::Lg: return { 44.0f, 8.0f, 16.0f, 16.0f, 15.0f, 13.0f, 5.0f, 3.0f, 1.5f, 18.0f };
        case UiSize::Xl: return { 52.0f, 10.0f, 20.0f, 18.0f, 16.0f, 14.0f, 6.0f, 4.0f, 1.5f, 22.0f };
    }
    return { 36.0f, 6.0f, 12.0f, 14.0f, 13.0f, 12.0f, 4.0f, 3.0f, 1.0f, 16.0f };
}

/// Autonomous InputField widget logic (execution, measuring, and rendering).
class InputField {
public:
    /// Primary immediate mode entry point for InputField with label and editable value pointers.
    static Response Run(
        Context& ctx,
        std::string_view label,
        std::string* stringValue,
        float* floatValue,
        double* doubleValue,
        int* intValue,
        std::string_view displayFallback,
        const InputFieldOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on content, label, suffix, and placement.
    static Vec2 Measure(
        std::string_view label,
        std::string_view displayText,
        const InputFieldOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Renders the input frame, label, prefix, suffix/unit, text/placeholder, and cursor.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        std::string_view label,
        std::string_view displayText,
        const InputFieldOptions& opts,
        UiSize size,
        bool hovered = false,
        bool focused = false,
        size_t cursor = 0,
        bool disabled = false,
        bool cursorVisible = true,
        float cursorWidth = 1.5f,
        ThemeMode theme = ThemeMode::Dark
    );
};

} // namespace guikit
