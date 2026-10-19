#pragma once

#include <optional>
#include <span>
#include <source_location>
#include <string_view>
#include "guikit/core/Control.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Clean configuration options for ColorPicker control widget.
struct ColorPickerOptions {
    UiText tooltip{};                              ///< Optional floating tooltip shown on hover
    StretchMode stretch{StretchMode::Fill};        ///< Sizing behavior: Compact, Space, Fill
    Align align{Align::Start};                     ///< Standalone horizontal control alignment (Start, Center, End)
    std::optional<float> width{std::nullopt};      ///< Control width: in points for Compact/Space (nullopt = auto), or ratio of bounds (0.0..1.0) for Fill (nullopt = 1.0)
    std::optional<float> height{std::nullopt};     ///< Explicit control height in points (nullopt = inherit from UiSize)
    std::optional<float> minWidth{std::nullopt};   ///< Minimum control width constraint in points
    StateColor background{StateColor::Surface()};  ///< Control well surface fill color
    Border border{};                               ///< Control well border stroke & corner radius
    bool showHex{true};                            ///< Display formatted #RRGGBB / #RRGGBBAA text inside the closed swatch button
    bool showAlpha{true};                          ///< Enable alpha channel slider and #RRGGBBAA hex representation
    bool showSwatches{true};                       ///< Display quick preset color swatches inside the picker panel/popover
    bool inlinePanel{false};                       ///< Render the full interactive SV + Hue/Alpha picker inline instead of a closed button + popover
    std::span<const Color> presets{};              ///< Optional custom preset color swatches (empty = default 10-color palette)
    bool disabled{false};                          ///< Interaction suppression
};

/// Internal runtime state stored per-widget ID for ColorPicker HSV continuity and drag tracking.
struct ColorPickerState {
    float h{210.0f};        ///< Hue in degrees [0..360)
    float s{0.75f};         ///< Saturation [0..1]
    float v{0.90f};         ///< Value / Brightness [0..1]
    Color lastSyncedColor{0, 0, 0, 0};
    bool initialized{false};
    uint8_t dragTarget{0};  ///< 0 = none, 1 = SV square, 2 = Hue bar, 3 = Alpha bar
    int hoveredSwatch{-1};
};

/// Internal runtime payload allocated in frame arena for ColorPicker nodes.
struct ColorPickerData {
    ColorPickerOptions opts{};
    Color value{59, 130, 246, 255};
    float h{210.0f};
    float s{0.75f};
    float v{0.90f};
    std::string_view hexText{};
    int hoveredSwatch{-1};
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
        std::source_location loc = std::source_location::current()
    );

    /// Converts RGB [0..255] to HSV (h in [0..360], s in [0..1], v in [0..1]).
    static void RgbToHsv(Color c, float& outH, float& outS, float& outV) noexcept;

    /// Converts HSV (h in [0..360], s in [0..1], v in [0..1]) and alpha [0..255] to Color.
    [[nodiscard]] static Color HsvToRgb(float h, float s, float v, uint8_t a = 255) noexcept;

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
        std::string_view hexText,
        int hoveredSwatch,
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
        std::string_view hexText,
        int hoveredSwatch,
        const ColorPickerOptions& opts,
        UiSize size,
        bool isFloatingPopup,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
