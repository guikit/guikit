#pragma once

#include "guikit/context.hpp"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/core/types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for ColorPicker control widget.
struct ColorPickerOptions {
    bool showHex{true};              ///< Display formatted #RRGGBB / #RRGGBBAA text inside the closed swatch button
    bool showAlpha{true};            ///< Enable alpha channel slider and #RRGGBBAA hex representation
    bool showSwatches{true};         ///< Display quick preset color swatches inside the picker panel/popover
    bool inlinePanel{false};         ///< Render the full interactive SV + Hue/Alpha picker inline instead of a closed button + popover
    keel::ArrayRef<Color> swatches{};///< Optional custom color swatches (preferred name; empty = fallback to presets or default palette)
    keel::ArrayRef<Color> presets{}; ///< Legacy alias for swatches (empty = default 10-color palette)
    UiText tooltip{};                ///< Optional floating tooltip shown on hover
    bool disabled{false};                            ///< Interaction suppression
    UiSize size{UiSize::Md};                         ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                                ///< Horizontal sizing policy (`Auto` -> `Stretch`, `Fit`, or explicit `dp`)
    AxisSize height{};                               ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or explicit `dp`)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};  ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};  ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt}; ///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt}; ///< Optional maximum height in logical dp
    StateColor color{};                              ///< Per-state semantic tint/accent color override
    StateColor background{};                         ///< Optional well background override (`{}` = theme default, `StateColor::None()` = transparent)
    Border border{};                                 ///< Optional border width, color, and radius override
};

/// Internal runtime state stored per-widget ID for ColorPicker HSV continuity and drag tracking.
struct ColorPickerState {
    keel::f32 h{210.0f};        ///< Hue in degrees [0..360)
    keel::f32 s{0.75f};         ///< Saturation [0..1]
    keel::f32 v{0.90f};         ///< Value / Brightness [0..1]
    Color lastSyncedColor{0, 0, 0, 0};
    bool initialized{false};
    keel::u8 dragTarget{0};  ///< 0 = none, 1 = SV square, 2 = Hue bar, 3 = Alpha bar
    keel::i32 hoveredSwatch{-1};
};

/// Internal runtime payload allocated in frame arena for ColorPicker nodes.
struct ColorPickerData {
    ColorPickerOptions opts{};
    Color value{59, 130, 246, 255};
    keel::f32 h{210.0f};
    keel::f32 s{0.75f};
    keel::f32 v{0.90f};
    keel::StringRef hexText{};
    keel::i32 hoveredSwatch{-1};
    bool isOpen{false};
};

/// Autonomous ColorPicker widget logic (execution, HSV conversion, measuring, and rendering).
namespace detail {

class ColorPicker {
public:
    /// Primary immediate mode entry point for ColorPicker bound to a Color value.
    static Response Run(
        Context& ctx,
        Color& value,
        const ColorPickerOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Converts RGB [0..255] to HSV (h in [0..360], s in [0..1], v in [0..1]).
    static void RgbToHsv(Color c, keel::f32& outH, keel::f32& outS, keel::f32& outV) noexcept;

    /// Converts HSV (h in [0..360], s in [0..1], v in [0..1]) and alpha [0..255] to Color.
    [[nodiscard]] static Color HsvToRgb(keel::f32 h, keel::f32 s, keel::f32 v, keel::u8 a = 255) noexcept;

    /// Measures intrinsic dimensions of the closed swatch button or inline picker panel.
    static Vec2 Measure(
        const ColorPickerOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        keel::f32 availableWidth = 0.0f
    );
    static Vec2 Measure(
        const ColorPickerOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        keel::f32 availableWidth = 0.0f
    );

    /// Renders the closed ColorPicker button (or inline panel when opts.inlinePanel is true).
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        Color value,
        keel::f32 h,
        keel::f32 s,
        keel::f32 v,
        keel::StringRef hexText,
        keel::i32 hoveredSwatch,
        const ColorPickerOptions& opts,
        UiSize size,
        bool hovered = false,
        bool open = false,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark(),
        bool focused = false
    );

    /// Renders the interactive SV square + Hue/Alpha sliders + Hex readout + preset swatches inside `panelRect`.
    static void RenderPanel(
        ICanvas& canvas,
        Rect panelRect,
        Color value,
        keel::f32 h,
        keel::f32 s,
        keel::f32 v,
        keel::StringRef hexText,
        keel::i32 hoveredSwatch,
        const ColorPickerOptions& opts,
        UiSize size,
        bool isFloatingPopup,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace detail

inline Response ColorPicker(
    Context& ui,
    Color& value,
    ColorPickerOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::ColorPicker::Run(ui, value, opts, loc);
}

} // namespace guikit
