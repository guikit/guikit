#pragma once

#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/string.h"

namespace guikit {

class Context;
class ICanvas;

/// Character filter mode for single-line InputField editing.
enum class InputFilter : keel::u8 {
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
    keel::Optional<CursorBlinkMode> cursorMode{keel::kNullOpt}; ///< Cursor blink override (inherits context config if nullopt)
    AxisSize width{};                              ///< Horizontal sizing policy (`Auto` -> `Stretch`, `Fit` in `Toolbar`, or `Fixed(dp)`)
    AxisSize height{};                             ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or `Fixed(dp)`)
    keel::Optional<float> minWidth{keel::kNullOpt};   ///< Minimum input box width constraint in points
    StateColor color{};                            ///< Optional focus ring & caret accent color override (e.g. StateColor::Danger() for validation error)
    StateColor background{StateColor::Surface()};  ///< Input box surface fill color (Surface/Default = on, None = off, or custom StateColor)
    Border border{};                               ///< Input box border stroke & corner radius configuration
    keel::usize maxLength{0};                           ///< Maximum UTF-8 codepoint count allowed (0 = unlimited)
    InputFilter filter{InputFilter::Any};          ///< Optional live character filter (Integer, Decimal, Hex)
    bool numericOnly{false};                       ///< Convenience alias for filter = InputFilter::Decimal when filter == InputFilter::Any
    bool selectAllOnFocus{false};                  ///< Automatically select the entire text buffer when the field gains focus
    bool password{false};                          ///< Masks input characters with '*'
    bool passwordReveal{true};                     ///< Shows an interactive eye/eye-off toggle button when password is true
    bool clearable{false};                         ///< Shows an interactive clear ('X') button when non-empty and editable
    UiSize size{UiSize::Md};                       ///< Component size scale
    bool disabled{false};                          ///< Interaction suppression
    bool readOnly{false};                          ///< Non-editable display mode
};

/// Internal runtime state allocated in frame arena for InputField nodes.
struct InputFieldData {
    InputFieldOptions opts{};
    keel::StringRef displayText{};
    keel::usize cursor{0};
    keel::usize selStart{0};
    keel::usize selEnd{0};
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
    /// Primary immediate mode entry point for InputField with editable StringSink or display fallback.
    static Response Run(
        Context& ctx,
        keel::Optional<keel::StringSink> stringValue,
        keel::StringRef displayFallback,
        const InputFieldOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions based on content, prefix, suffix, and sizing mode.
    static Vec2 Measure(
        keel::StringRef displayText,
        const InputFieldOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        keel::StringRef displayText,
        const InputFieldOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    /// Renders the input frame, prefix, suffix, text/placeholder, selection highlight, and cursor.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        keel::StringRef displayText,
        const InputFieldOptions& opts,
        UiSize size,
        bool hovered = false,
        bool focused = false,
        keel::usize cursor = 0,
        bool disabled = false,
        bool cursorVisible = true,
        float cursorWidth = 1.5f,
        const UiTheme& theme = UiTheme::Dark(),
        keel::usize selStart = 0,
        keel::usize selEnd = 0,
        float scrollX = 0.0f,
        bool showClearBtn = false,
        bool clearHovered = false,
        bool showRevealBtn = false,
        bool revealHovered = false,
        bool passwordRevealed = false
    );
};

} // namespace guikit
