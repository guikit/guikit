#pragma once

#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Character filter mode for single-line InputField editing.
enum class InputFilter : uint8_t {
    Any = 0,  ///< Accept all characters (default)
    Integer,  ///< Accept optional leading +/- and ASCII digits [0..9]
    Decimal,  ///< Accept optional leading +/-, digits, decimal '.', and optional 'e'/'E' exponent
    Hex       ///< Accept optional leading '#' or '0x'/'0X' prefix and hexadecimal digits [0-9a-fA-F]
};

/// Clean configuration options for text InputField widget.
struct InputFieldOptions {
    UiText placeholder{};                          ///< Placeholder text when empty (e.g. "Enter value...")
    UiText tooltip{};                              ///< Optional floating tooltip shown on hover
    UiAffix prefix{};                              ///< Leading icon and/or symbol/text inside field on left (e.g. "$", Icon::Search(), {Icon::Search(), "https://"})
    UiAffix suffix{};                              ///< Trailing symbol/text and/or icon inside field on right (e.g. ":443", {"UTF-8", Icon::Check()})
    UiText valueFormat{};                          ///< Optional text styling and/or candidate width variants
    Align align{Align::Start};                     ///< Standalone control alignment (Start, Center, End)
    std::optional<CursorBlinkMode> cursorMode{std::nullopt}; ///< Cursor blink override (inherits context config if nullopt)
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Input box width in dp (or legacy ratio 0.0..1.0 in Fill)
    std::optional<float> widthRatio{std::nullopt}; ///< Normalized fill ratio 0.0..1.0 of bounds in Fill mode
    std::optional<float> height{std::nullopt};     ///< Explicit input box height in points (nullopt = inherit defaultHeight from UiSize)
    std::optional<float> minWidth{std::nullopt};   ///< Minimum input box width constraint in points
    StateColor color{};                            ///< Optional focus ring & caret accent color override (e.g. StateColor::Danger() for validation error)
    StateColor background{StateColor::Surface()};  ///< Input box surface fill color (Surface/Default = on, None = off, or custom StateColor)
    Border border{};                               ///< Input box border stroke & corner radius configuration
    size_t maxLength{0};                           ///< Maximum UTF-8 codepoint count allowed (0 = unlimited)
    InputFilter filter{InputFilter::Any};          ///< Optional live character filter (Integer, Decimal, Hex)
    bool numericOnly{false};                       ///< Convenience alias for filter = InputFilter::Decimal when filter == InputFilter::Any
    bool selectAllOnFocus{false};                  ///< Automatically select the entire text buffer when the field gains focus
    bool password{false};                          ///< Masks input characters with '*'
    bool passwordReveal{true};                     ///< Shows an interactive eye/eye-off toggle button when password is true
    bool clearable{false};                         ///< Shows an interactive clear ('X') button when non-empty and editable
    bool disabled{false};                          ///< Interaction suppression
    bool readOnly{false};                          ///< Non-editable display mode
    const UiTheme* theme{nullptr};                 ///< Optional scoped theme override
    std::optional<float> scale{std::nullopt};      ///< Optional scoped scale factor override
};

/// Internal runtime state allocated in frame arena for InputField nodes.
struct InputFieldData {
    InputFieldOptions opts{};
    std::string_view displayText{};
    size_t cursor{0};
    size_t selStart{0};
    size_t selEnd{0};
    float scrollX{0.0f};
    float cursorWidth{1.5f};
    bool cursorVisible{true};
    bool showClearBtn{false};
    bool clearHovered{false};
    bool showRevealBtn{false};
    bool revealHovered{false};
    bool passwordRevealed{false};
};

/// Autonomous text InputField widget logic (execution, measuring, and rendering).
class InputField {
public:
    /// Primary immediate mode entry point for InputField with editable string pointer or display fallback.
    static Response Run(
        Context& ctx,
        std::string* stringValue,
        std::string_view displayFallback,
        const InputFieldOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on content, prefix, suffix, and sizing mode.
    static Vec2 Measure(
        std::string_view displayText,
        const InputFieldOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        std::string_view displayText,
        const InputFieldOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    /// Renders the input frame, prefix, suffix, text/placeholder, selection highlight, and cursor.
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
        const UiTheme& theme = UiTheme::Dark(),
        size_t selStart = 0,
        size_t selEnd = 0,
        float scrollX = 0.0f,
        bool showClearBtn = false,
        bool clearHovered = false,
        bool showRevealBtn = false,
        bool revealHovered = false,
        bool passwordRevealed = false
    );
};

} // namespace guikit
