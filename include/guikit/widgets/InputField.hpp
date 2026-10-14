#pragma once

#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for InputField widget.
struct InputFieldOptions {
    UiText label{};                                ///< Primary caption text
    UiText description{};                          ///< Optional helper text below input box
    std::string_view placeholder{};                ///< Placeholder text when empty (e.g. "Enter value...")
    std::string_view prefix{};                     ///< Symbol/text inside field on left (e.g. "$", "±")
    std::string_view suffix{};                     ///< Unit/symbol inside field on right (e.g. "%", "mm", "MB/s")
    std::string_view format{"%.2f"};               ///< Format string for floating-point values
    UiVariant variant{UiVariant::Default};         ///< Accent/border variant (Default, Warning, Danger, etc.)
    ElementLayout layout{ElementLayout::Stacked};    ///< Layout geometry: Stacked, Top, Bottom, Inline
    InlineOrder inlineOrder{InlineOrder::LabelFirst}; ///< Sequencing order in Inline layout: LabelFirst, ControlFirst
    Align elementAlign{Align::Start};                ///< Element composition and label alignment (Start, Center, End)
    Icon prefixIcon{};                             ///< Optional vector icon inside field on left
    Icon suffixIcon{};                             ///< Optional vector icon inside field on right
    std::optional<CursorBlinkMode> cursorMode{std::nullopt}; ///< Cursor blink override (inherits context config if nullopt)
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
    std::optional<float> radius{std::nullopt};     ///< Local corner radius override (nullopt = inherit from size metrics)
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Input box width: in points for Compact/Space (nullopt = from theme), or % of bounds (0..100) for Fill (nullopt = 100%)
    std::optional<float> minWidth{std::nullopt};   ///< Minimum input box width constraint in points
    bool disabled{false};                          ///< Interaction suppression
    bool readOnly{false};                          ///< Non-editable display mode
};

/// Internal runtime state allocated in frame arena for InputField nodes.
struct InputFieldData {
    InputFieldOptions opts{};
    std::string_view displayText{};
    size_t cursor{0};
    float cursorWidth{1.5f};
    bool cursorVisible{true};
};

/// Autonomous InputField widget logic (execution, measuring, and rendering).
class InputField {
public:
    /// Primary immediate mode entry point for InputField with editable value pointers.
    static Response Run(
        Context& ctx,
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
        std::string_view displayText,
        const InputFieldOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    );
    static Vec2 Measure(
        std::string_view displayText,
        const InputFieldOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Renders the input frame, label, prefix, suffix/unit, text/placeholder, and cursor.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        std::string_view displayText,
        const InputFieldOptions& opts,
        UiSize size,
        bool hovered = false,
        bool focused = false,
        size_t cursor = 0,
        bool disabled = false,
        bool cursorVisible = true,
        float cursorWidth = 1.5f,
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
