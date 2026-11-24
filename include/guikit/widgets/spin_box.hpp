#pragma once

#include "guikit/context.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Visual layout style for SpinBox decrement/increment ('-' / '+') buttons.
enum class SpinBoxStepStyle : keel::u8 {
    None,       ///< No stepper buttons (keyboard Up/Down or mouse wheel only)
    Inline,     ///< Compact '-' and '+' buttons integrated inside the right side of the input well
    Detached,   ///< Two square (H x H) '-' and '+' buttons placed outside to the right of the input field
    Split       ///< Square (H x H) '-' button on the left and '+' button on the right of the input field
};

/// Clean configuration options for numeric SpinBox widget.
struct SpinBoxOptions {
    UiAffix prefix{};                                          ///< Leading icon and/or symbol/text inside field on left (e.g. "$", "X:")
    UiAffix suffix{};                                          ///< Trailing unit/symbol/text and/or icon inside field on right (e.g. "MHz", "mm", "%")
    UiText valueFormat{"%.2f"};                                ///< Format string for numeric values and/or candidate width variants
    keel::Optional<CursorBlinkMode> cursorMode{keel::kNullOpt}; ///< Cursor blink override when editing text
    keel::Optional<keel::f64> min{keel::kNullOpt};                ///< Optional minimum numeric value constraint
    keel::Optional<keel::f64> max{keel::kNullOpt};                ///< Optional maximum numeric value constraint
    keel::Optional<keel::f64> step{keel::kNullOpt};               ///< Step increment for +/- buttons, Up/Down keys, and mouse wheel (defaults to 1.0)
    SpinBoxStepStyle stepStyle{SpinBoxStepStyle::Inline};      ///< Layout style for '-' and '+' stepper buttons
    Icon minusIcon{Icon::Minus()};                             ///< Icon rendered inside the decrement ('-') stepper button
    Icon plusIcon{Icon::Plus()};                               ///< Icon rendered inside the increment ('+') stepper button
    bool stepButtons{true};                                    ///< Shows interactive '-' and '+' stepper buttons (false forces SpinBoxStepStyle::None)
    bool showFillBar{false};                                   ///< Renders an accent progress bar along the bottom edge when min & max are set
    bool readOnly{false};                                      ///< Non-editable display mode
    UiText tooltip{};                                          ///< Optional floating tooltip shown on hover
    bool disabled{false};                                      ///< Interaction suppression
    UiSize size{UiSize::Md};                                   ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                                          ///< Horizontal sizing policy (`Auto` -> `Stretch`, `Fit`, or explicit `dp`)
    AxisSize height{};                                         ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or explicit `dp`)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};            ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};            ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};           ///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};           ///< Optional maximum height in logical dp
    StateColor color{};                                        ///< Per-state semantic accent color override
    StateColor background{};                                   ///< Optional background fill override (`{}` = theme default, `StateColor::None()` = transparent)
    Border border{};                                           ///< Optional border width, color, and radius override
};

/// Internal runtime state allocated in frame arena for SpinBox nodes.
struct SpinBoxData {
    SpinBoxOptions opts{};
    keel::StringRef displayText{};
    keel::f32 fillRatio{0.0f};
    keel::usize cursor{0};
    keel::usize selStart{0};
    keel::usize selEnd{0};
    keel::f32 scrollX{0.0f};
    keel::f32 cursorWidth{1.5f};
    bool cursorVisible{true};
    bool showStepBtns{true};
    bool stepMinusHovered{false};
    bool stepPlusHovered{false};
    bool stepMinusPressed{false};
    bool stepPlusPressed{false};
    bool stepMinusDisabled{false};
    bool stepPlusDisabled{false};
};

/// Autonomous SpinBox widget logic (execution, measuring, and rendering).
namespace detail {

class SpinBox {
public:
    /// Primary immediate mode entry point for SpinBox with editable numeric value pointers.
    static Response Run(
        Context& ctx,
        keel::f32* floatValue,
        keel::f64* doubleValue,
        keel::i64* intValue,
        const SpinBoxOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions based on content, prefix, suffix, stepper buttons, and sizing mode.
    static Vec2 Measure(
        keel::StringRef displayText,
        const SpinBoxOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        keel::f32 availableWidth = 0.0f
    );
    static Vec2 Measure(
        keel::StringRef displayText,
        const SpinBoxOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        keel::f32 availableWidth = 0.0f
    );

    /// Renders the spinbox frame, optional fill bar, prefix, suffix/unit, stepper buttons, text, selection, and cursor.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        keel::StringRef displayText,
        const SpinBoxOptions& opts,
        UiSize size,
        bool hovered = false,
        bool focused = false,
        keel::usize cursor = 0,
        bool disabled = false,
        bool cursorVisible = true,
        keel::f32 cursorWidth = 1.5f,
        const UiTheme& theme = UiTheme::Dark(),
        keel::usize selStart = 0,
        keel::usize selEnd = 0,
        keel::f32 scrollX = 0.0f,
        bool showStepBtns = true,
        bool stepMinusHovered = false,
        bool stepPlusHovered = false,
        bool stepMinusDisabled = false,
        bool stepPlusDisabled = false,
        keel::f32 fillRatio = 0.0f,
        bool stepMinusPressed = false,
        bool stepPlusPressed = false
    );
};

} // namespace detail

template <SpinBoxNumeric T>
Response SpinBox(
    Context& ui,
    T& value,
    SpinBoxOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    using Decayed = keel::Decay<T>;
    if constexpr (keel::IsSame<Decayed, keel::f32>) {
        return detail::SpinBox::Run(ui, &value, nullptr, nullptr, opts, loc);
    } else if constexpr (keel::IsSame<Decayed, keel::f64>) {
        return detail::SpinBox::Run(ui, nullptr, &value, nullptr, opts, loc);
    } else if constexpr (keel::IsFloatingPoint<Decayed>) {
        keel::f64 tmp = keel::as<keel::f64>(value);
        Response r = detail::SpinBox::Run(ui, nullptr, &tmp, nullptr, opts, loc);
        if (r.changed) {
            value = keel::as<Decayed>(tmp);
        }
        return r;
    } else if constexpr (keel::IsSame<Decayed, keel::i64>) {
        return detail::SpinBox::Run(ui, nullptr, nullptr, &value, opts, loc);
    } else {
        constexpr keel::i64 kInt64Max = keel::Limits<keel::i64>::kMax;
        constexpr keel::i64 kInt64Min = keel::Limits<keel::i64>::kMin;
        constexpr keel::i64 kTypeMin = keel::IsUnsigned<Decayed>
            ? keel::i64{0}
            : ((sizeof(Decayed) >= sizeof(keel::i64)) ? kInt64Min : keel::as<keel::i64>(keel::Limits<Decayed>::kMin));
        constexpr keel::i64 kTypeMax = keel::IsUnsigned<Decayed>
            ? ((sizeof(Decayed) >= sizeof(keel::u64))
                   ? kInt64Max
                   : keel::as<keel::i64>(keel::Limits<Decayed>::kMax))
            : ((sizeof(Decayed) >= sizeof(keel::i64))
                   ? kInt64Max
                   : keel::as<keel::i64>(keel::Limits<Decayed>::kMax));
        if constexpr (keel::IsUnsigned<Decayed>) {
            if (!opts.min.has_value()) {
                opts.min = 0.0;
            } else {
                opts.min = keel::max(0.0, *opts.min);
            }
        }
        keel::i64 tmp = (keel::IsUnsigned<Decayed> && keel::as<keel::u64>(value) > keel::as<keel::u64>(kTypeMax))
            ? kTypeMax
            : keel::as<keel::i64>(value);
        Response r = detail::SpinBox::Run(ui, nullptr, nullptr, &tmp, opts, loc);
        if (r.changed) {
            value = keel::as<Decayed>(keel::clamp(tmp, kTypeMin, kTypeMax));
        }
        return r;
    }
}

template <SpinBoxNumeric T>
Response SpinBox(
    Context& ui,
    T& value,
    keel::f64 minVal,
    keel::f64 maxVal,
    keel::f64 stepVal = 1.0,
    SpinBoxOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    if (!opts.min.has_value()) opts.min = minVal;
    if (!opts.max.has_value()) opts.max = maxVal;
    if (!opts.step.has_value()) opts.step = stepVal;
    return guikit::SpinBox(ui, value, keel::move(opts), loc);
}

} // namespace guikit
