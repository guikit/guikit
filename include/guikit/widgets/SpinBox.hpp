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

/// Clean configuration options for numeric SpinBox widget.
struct SpinBoxOptions {
    UiText tooltip{};                              ///< Optional floating tooltip shown on hover
    UiAffix prefix{};                              ///< Leading icon and/or symbol/text inside field on left (e.g. "$", "X:")
    UiAffix suffix{};                              ///< Trailing unit/symbol/text and/or icon inside field on right (e.g. "MHz", "mm", "%")
    UiText valueFormat{"%.2f"};                    ///< Format string for numeric values and/or candidate width variants
    Align align{Align::Start};                     ///< Standalone control alignment (Start, Center, End)
    std::optional<CursorBlinkMode> cursorMode{std::nullopt}; ///< Cursor blink override when editing text
    StateColor color{};                            ///< Per-state accent color for active stepper buttons and optional progress fill bar
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    std::optional<float> width{std::nullopt};      ///< SpinBox width: in points for Compact/Space (nullopt = from theme), or ratio of bounds (0.0..1.0) for Fill (nullopt = 1.0)
    std::optional<float> height{std::nullopt};     ///< Explicit SpinBox height in points (nullopt = inherit defaultHeight from UiSize)
    std::optional<float> minWidth{std::nullopt};   ///< Minimum SpinBox width constraint in points
    StateColor background{StateColor::Surface()};  ///< SpinBox surface fill color (Surface/Default = on, None = off, or custom StateColor)
    Border border{};                               ///< SpinBox border stroke & corner radius configuration
    std::optional<double> min{std::nullopt};       ///< Optional minimum numeric value constraint
    std::optional<double> max{std::nullopt};       ///< Optional maximum numeric value constraint
    std::optional<double> step{std::nullopt};      ///< Step increment for +/- buttons, Up/Down keys, mouse wheel, and drag-scrub (defaults to 1.0)
    bool stepButtons{true};                        ///< Shows interactive '-' and '+' stepper buttons inside SpinBox
    bool dragScrub{true};                          ///< Allows horizontal mouse drag on the value area to scrub the number
    bool showFillBar{false};                       ///< Renders an accent progress bar along the bottom edge when min & max are set
    bool disabled{false};                          ///< Interaction suppression
    bool readOnly{false};                          ///< Non-editable display mode
};

/// Internal runtime state allocated in frame arena for SpinBox nodes.
struct SpinBoxData {
    SpinBoxOptions opts{};
    std::string_view displayText{};
    float fillRatio{0.0f};
    size_t cursor{0};
    size_t selStart{0};
    size_t selEnd{0};
    float scrollX{0.0f};
    float cursorWidth{1.5f};
    bool cursorVisible{true};
    bool showStepBtns{true};
    bool stepMinusHovered{false};
    bool stepPlusHovered{false};
    bool stepMinusDisabled{false};
    bool stepPlusDisabled{false};
    bool scrubbing{false};
};

/// Autonomous SpinBox widget logic (execution, measuring, and rendering).
class SpinBox {
public:
    /// Primary immediate mode entry point for SpinBox with editable numeric value pointers.
    static Response Run(
        Context& ctx,
        float* floatValue,
        double* doubleValue,
        int* intValue,
        const SpinBoxOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on content, prefix, suffix, stepper buttons, and sizing mode.
    static Vec2 Measure(
        std::string_view displayText,
        const SpinBoxOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        std::string_view displayText,
        const SpinBoxOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    /// Renders the spinbox frame, optional fill bar, prefix, suffix/unit, stepper buttons, text, selection, and cursor.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        std::string_view displayText,
        const SpinBoxOptions& opts,
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
        bool showStepBtns = true,
        bool stepMinusHovered = false,
        bool stepPlusHovered = false,
        bool stepMinusDisabled = false,
        bool stepPlusDisabled = false,
        float fillRatio = 0.0f,
        bool scrubbing = false
    );
};

} // namespace guikit
