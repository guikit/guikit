#pragma once

#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for ColorPicker control widget.
struct ColorPickerOptions {
    UiText tooltip{};                                ///< Optional floating tooltip shown on hover
    AxisSize width{};                                ///< Horizontal sizing policy (`Auto` -> `Stretch`, `Fit` in `Toolbar`, or `Fixed(dp)`)
    AxisSize height{};                               ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or `Fixed(dp)`)
    keel::Optional<float> minWidth{keel::kNullOpt};  ///< Minimum control width constraint in points
    StateColor background{StateColor::Surface()};  ///< Control well surface fill color
    Border border{};                               ///< Control well border stroke & corner radius
    bool showHex{true};                            ///< Display formatted #RRGGBB / #RRGGBBAA text inside the closed swatch button
    bool showAlpha{true};                          ///< Enable alpha channel slider and #RRGGBBAA hex representation
    bool showSwatches{true};                       ///< Display quick preset color swatches inside the picker panel/popover
    bool inlinePanel{false};                       ///< Render the full interactive SV + Hue/Alpha picker inline instead of a closed button + popover
    keel::ArrayRef<Color> swatches{};              ///< Optional custom color swatches (preferred name; empty = fallback to presets or default palette)
    keel::ArrayRef<Color> presets{};               ///< Legacy alias for swatches (empty = default 10-color palette)
    UiSize size{UiSize::Md};                       ///< Component size scale
    bool disabled{false};                          ///< Interaction suppression
};

/// Internal runtime state stored per-widget ID for ColorPicker HSV continuity and drag tracking.
struct ColorPickerState {
    float h{210.0f};        ///< Hue in degrees [0..360)
    float s{0.75f};         ///< Saturation [0..1]
    float v{0.90f};         ///< Value / Brightness [0..1]
    Color lastSyncedColor{0, 0, 0, 0};
    bool initialized{false};
    keel::u8 dragTarget{0};  ///< 0 = none, 1 = SV square, 2 = Hue bar, 3 = Alpha bar
    keel::i32 hoveredSwatch{-1};
};

/// Internal runtime payload allocated in frame arena for ColorPicker nodes.
struct ColorPickerData {
    ColorPickerOptions opts{};
    Color value{59, 130, 246, 255};
    float h{210.0f};
    float s{0.75f};
    float v{0.90f};
    keel::StringRef hexText{};
    keel::i32 hoveredSwatch{-1};
    bool isOpen{false};
};

/// Autonomous ColorPicker widget logic (execution, HSV conversion, measuring, and rendering).
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
    static void RgbToHsv(Color c, float& outH, float& outS, float& outV) noexcept;

    /// Converts HSV (h in [0..360], s in [0..1], v in [0..1]) and alpha [0..255] to Color.
    [[nodiscard]] static Color HsvToRgb(float h, float s, float v, keel::u8 a = 255) noexcept;

    /// Measures intrinsic dimensions of the closed swatch button or inline picker panel.
    static Vec2 Measure(
        const ColorPickerOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        const ColorPickerOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr,
        float availableWidth = 0.0f
    );

    /// Renders the closed ColorPicker button (or inline panel when opts.inlinePanel is true).
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        Color value,
        float h,
        float s,
        float v,
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
        float h,
        float s,
        float v,
        keel::StringRef hexText,
        keel::i32 hoveredSwatch,
        const ColorPickerOptions& opts,
        UiSize size,
        bool isFloatingPopup,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
