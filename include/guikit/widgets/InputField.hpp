#pragma once

#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for InputField widget.
struct InputFieldOptions {
    UiText placeholder{};                          ///< Placeholder text when empty (e.g. "Enter value...")
    UiText tooltip{};                              ///< Optional floating tooltip shown on hover
    UiAffix prefix{};                              ///< Leading icon and/or symbol/text inside field on left (e.g. "$", Icon::Search(), {Icon::Search(), "https://"})
    UiAffix suffix{};                              ///< Trailing unit/symbol/text and/or icon inside field on right (e.g. "%", "mm", {"MB/s", Icon::Check()})
    UiText valueFormat{"%.2f"};                    ///< Format string for numeric values and/or candidate width variants
    Align align{Align::Start};                     ///< Standalone control alignment (Start, Center, End)
    std::optional<CursorBlinkMode> cursorMode{std::nullopt}; ///< Cursor blink override (inherits context config if nullopt)
    std::optional<UiSize> size{std::nullopt};      ///< Local size override (nullopt = inherit container size)
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< Input box width: in points for Compact/Space (nullopt = from theme), or % of bounds (0..100) for Fill (nullopt = 100%)
    std::optional<float> height{std::nullopt};     ///< Explicit input box height in points (nullopt = inherit defaultHeight from UiSize)
    std::optional<float> minWidth{std::nullopt};   ///< Minimum input box width constraint in points
    StateColor background{StateColor::Surface()};  ///< Input box surface fill color (Surface/Default = on, None = off, or custom StateColor)
    Border border{};                               ///< Input box border stroke & corner radius configuration
    std::optional<double> min{std::nullopt};       ///< Optional minimum numeric value constraint
    std::optional<double> max{std::nullopt};       ///< Optional maximum numeric value constraint
    std::optional<double> step{std::nullopt};      ///< Optional step increment for Up/Down keys, mouse wheel, and stepper buttons
    bool password{false};                          ///< Masks input characters with '*'
    bool passwordReveal{true};                     ///< Shows an interactive eye/eye-off toggle button when password is true
    bool clearable{false};                         ///< Shows an interactive clear ('X') button when non-empty and editable
    bool stepButtons{false};                       ///< Shows interactive '-' and '+' stepper buttons inside numeric InputField
    bool disabled{false};                          ///< Interaction suppression
    bool readOnly{false};                          ///< Non-editable display mode
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
    bool showStepBtns{false};
    bool stepMinusHovered{false};
    bool stepPlusHovered{false};
    bool stepMinusDisabled{false};
    bool stepPlusDisabled{false};
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

    /// Renders the input frame, label, prefix, suffix/unit, text/placeholder, selection highlight, and cursor.
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
        bool passwordRevealed = false,
        bool showStepBtns = false,
        bool stepMinusHovered = false,
        bool stepPlusHovered = false,
        bool stepMinusDisabled = false,
        bool stepPlusDisabled = false
    );
};

} // namespace guikit
