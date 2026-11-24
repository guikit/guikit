#pragma once

#include "guikit/style/theme.hpp"
#include "guikit/style/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

class ICanvas;

/// Semantic role of a framed UI surface passed to `IStylePainter::DrawFrame`.
enum class FrameKind : keel::u8 {
    Surface = 0,     ///< Flat card / container backdrop
    Elevated,        ///< Raised dialog / elevated card
    Popup,           ///< Context menu / dropdown popup panel
    Input,           ///< Editable text field / select box (sunken in 3D)
    Button,          ///< Push button (raised, sunken when pressed/active)
    ToolbarButton,   ///< Flat ghost toolbar / chrome button (thin 3D on hover/active)
    Badge,           ///< Tag / heading badge / status cell
    DropdownButton   ///< Right-hand combo-box dropdown button inside Select
};

/// Structural role of a container border overlay passed to `IStylePainter::DrawContainerBorder`.
enum class ContainerFrameRole : keel::u8 {
    Generic = 0,
    CardFlat,
    CardElevated,
    Sidebar,
    Toolbar,
    StatusBar,
    BottomPanel
};

/// Tab bar chrome style passed to `IStylePainter::DrawTabChrome` without pulling widget headers into core.
enum class TabChromeStyle : keel::u8 {
    Underline = 0,
    Card,
    Pill,
    Folder
};

/// Parameters describing a generic control or surface frame for `IStylePainter::DrawFrame`.
struct FramePaintParams {
    Rect      rect{};
    keel::f32 radius{0.0f};
    bool      explicitRadius{false};
    Color     bg{Color::Transparent()};
    Color     border{Color::Transparent()};
    keel::f32 borderWidth{0.0f};
    bool      hovered{false};
    bool      pressed{false};
    bool      active{false};
    bool      focused{false};
    bool      disabled{false};
    Elevation elevation{Elevation::None};
    keel::f32 shadowScale{1.0f};
};

/// Parameters describing a scrollbar's layout and interactive state for `IStylePainter::DrawScrollbar`.
struct ScrollbarPaintParams {
    Rect      barRect{};
    Rect      trackRect{};
    Rect      thumbRect{};
    Rect      startButtonRect{};
    Rect      endButtonRect{};
    bool      horizontal{false};
    bool      hasButtons{false};
    bool      hovered{false};
    bool      thumbHovered{false};
    bool      dragging{false};
    bool      startHovered{false};
    bool      startPressed{false};
    bool      endHovered{false};
    bool      endPressed{false};
    keel::f32 cornerRadius{0.0f};
    Color     accentColor{Color::Transparent()};
};

/// Pluggable look-and-feel style painter interface (analogous to Qt QStyle / Flutter ThemeExtension).
///
/// Widgets delegate surface frames, bevels, dividers, tabs, sliders, progress bars, switches,
/// checkboxes, menus, and badges to `theme.Painter()`. Applications can install a custom subclass
/// of `DefaultStylePainter` on any `UiTheme` (globally or inside a `ui.Zone`) to customize
/// individual control families without modifying GuiKit core.
class IStylePainter {
public:
    virtual ~IStylePainter() noexcept = default;

    /// Draws elevation shadow, fill background, border/3D bevel, and focus ring for a control surface.
    virtual void DrawFrame(
        ICanvas& canvas,
        const UiTheme& theme,
        FrameKind kind,
        const FramePaintParams& p
    ) const = 0;

    /// Draws a horizontal or vertical divider / separator line (flat hairline or 2px etched 3D groove).
    virtual void DrawDivider(
        ICanvas& canvas,
        const UiTheme& theme,
        Vec2 p0,
        Vec2 p1,
        Color fallbackColor,
        keel::f32 thickness = 1.0f
    ) const = 0;

    /// Draws a container's outer border overlay after children are rendered.
    virtual void DrawContainerBorder(
        ICanvas& canvas,
        const UiTheme& theme,
        ContainerFrameRole role,
        Rect rect,
        keel::f32 radius,
        Color borderColor,
        keel::f32 borderWidth,
        bool sidebarRight = false,
        bool isVerticalToolbar = false
    ) const = 0;

    /// Draws control label text with theme-appropriate disabled treatment (opacity or 3D embossed).
    virtual void DrawControlText(
        ICanvas& canvas,
        const UiTheme& theme,
        keel::StringRef text,
        Rect rect,
        keel::f32 fontSize,
        Color color,
        bool disabled,
        keel::f32 alignX = 0.5f,
        keel::f32 alignY = 0.5f
    ) const = 0;

    /// Draws a slider / step-slider track groove and optional active fill segment.
    virtual void DrawSliderTrack(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect trackRect,
        Rect fillRect,
        bool vertical,
        bool hasFill,
        Color trackBg,
        Color trackBorder,
        Color fillColor,
        keel::f32 cornerRadius,
        bool explicitColor
    ) const = 0;

    /// Draws a slider / step-slider thumb knob (circular, square, or pentagonal 3D pointer).
    virtual void DrawSliderThumb(
        ICanvas& canvas,
        const UiTheme& theme,
        Vec2 center,
        keel::f32 thumbSize,
        bool vertical,
        Color bg,
        Color border,
        keel::f32 borderWidth,
        bool hovered,
        bool active,
        bool focused,
        bool disabled,
        keel::f32 cornerRadius
    ) const = 0;

    /// Draws a continuous, indeterminate, or classic 3D segmented progress bar.
    virtual void DrawProgressBar(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect trackRect,
        keel::f32 ratio,
        bool vertical,
        bool indeterminate,
        keel::f32 animPhase,
        Color trackBg,
        Color trackBorder,
        Color fillColor,
        keel::f32 cornerRadius,
        bool disabled
    ) const = 0;

    /// Draws a toggle switch track and sliding thumb knob.
    virtual void DrawSwitch(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect trackRect,
        keel::f32 knobDiameter,
        keel::f32 knobPadding,
        Color trackFill,
        Color trackStroke,
        keel::f32 strokeWidth,
        Color knobFill,
        bool checked,
        bool focused,
        bool disabled,
        keel::f32 themeRadius
    ) const = 0;

    /// Draws a checkbox box and returns the resolved checkmark/indeterminate ink color.
    [[nodiscard]] virtual Color DrawCheckboxBox(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect boxRect,
        keel::f32 cornerRadius,
        Color fill,
        Color stroke,
        keel::f32 strokeWidth,
        Color defaultMarkColor,
        bool pressed,
        bool focused,
        bool disabled
    ) const = 0;

    /// Draws the baseline separator along a `TabBar`.
    virtual void DrawTabBarBaseline(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect barRect
    ) const = 0;

    /// Draws the background chrome of a single `TabBar` item and returns vertical content shift.
    [[nodiscard]] virtual keel::f32 DrawTabChrome(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect bounds,
        TabChromeStyle style,
        bool active,
        bool hovered,
        bool focused,
        bool disabled,
        bool showSeparator,
        Color accentCol,
        Color pillActiveCol,
        keel::f32 radius
    ) const = 0;

    /// Draws a `MenuBar` top-level trigger background/border, writing resolved text color & content shift.
    virtual void DrawMenuBarTrigger(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect rect,
        keel::f32 radius,
        bool hovered,
        bool open,
        bool disabled,
        Color& outTextColor,
        Vec2& outTextShift
    ) const = 0;

    /// Draws a popup menu row highlight (`ContextMenu` / `Select` popup).
    /// Returns true when the row uses solid accent highlight requiring `palette.accentText` foreground.
    [[nodiscard]] virtual bool DrawMenuItemHighlight(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect itemRect,
        keel::f32 itemRadius,
        bool hovered,
        bool selected,
        bool subExpanded,
        bool isDanger,
        bool disabled,
        Color dangerColor
    ) const = 0;

    /// Draws a compact badge / counter pill (respecting `chrome.badgeShape` and `chrome.frame`).
    virtual void DrawBadge(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect rect,
        Color bg,
        Color border,
        keel::f32 borderWidth,
        keel::f32 fallbackRadius
    ) const = 0;

    /// Draws a vertical or horizontal scrollbar (track, thumb, optional grip, and optional stepper buttons).
    virtual void DrawScrollbar(
        ICanvas& canvas,
        const UiTheme& theme,
        const ScrollbarPaintParams& p
    ) const = 0;

    /// Draws the bottom-right corner filler square where vertical and horizontal scrollbars intersect.
    virtual void DrawScrollbarCorner(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect cornerRect
    ) const = 0;
};

/// Built-in default look-and-feel painter supporting all `ChromeStyle`, `ChromeTokens`, and `FillStyle` modes
/// (`Modern`, `Fluent`, `Classic3D`, `Brutalist` across any `ThemePalette`).
/// Subclass this painter to override only specific control primitives while keeping default behavior.
class DefaultStylePainter : public IStylePainter {
public:
    ~DefaultStylePainter() noexcept override = default;

    void DrawFrame(ICanvas& canvas, const UiTheme& theme, FrameKind kind, const FramePaintParams& p) const override;
    void DrawDivider(ICanvas& canvas, const UiTheme& theme, Vec2 p0, Vec2 p1, Color fallbackColor, keel::f32 thickness = 1.0f) const override;
    void DrawContainerBorder(ICanvas& canvas, const UiTheme& theme, ContainerFrameRole role, Rect rect, keel::f32 radius, Color borderColor, keel::f32 borderWidth, bool sidebarRight = false, bool isVerticalToolbar = false) const override;
    void DrawControlText(ICanvas& canvas, const UiTheme& theme, keel::StringRef text, Rect rect, keel::f32 fontSize, Color color, bool disabled, keel::f32 alignX = 0.5f, keel::f32 alignY = 0.5f) const override;
    void DrawSliderTrack(ICanvas& canvas, const UiTheme& theme, Rect trackRect, Rect fillRect, bool vertical, bool hasFill, Color trackBg, Color trackBorder, Color fillColor, keel::f32 cornerRadius, bool explicitColor) const override;
    void DrawSliderThumb(ICanvas& canvas, const UiTheme& theme, Vec2 center, keel::f32 thumbSize, bool vertical, Color bg, Color border, keel::f32 borderWidth, bool hovered, bool active, bool focused, bool disabled, keel::f32 cornerRadius) const override;
    void DrawProgressBar(ICanvas& canvas, const UiTheme& theme, Rect trackRect, keel::f32 ratio, bool vertical, bool indeterminate, keel::f32 animPhase, Color trackBg, Color trackBorder, Color fillColor, keel::f32 cornerRadius, bool disabled) const override;
    void DrawSwitch(ICanvas& canvas, const UiTheme& theme, Rect trackRect, keel::f32 knobDiameter, keel::f32 knobPadding, Color trackFill, Color trackStroke, keel::f32 strokeWidth, Color knobFill, bool checked, bool focused, bool disabled, keel::f32 themeRadius) const override;
    [[nodiscard]] Color DrawCheckboxBox(ICanvas& canvas, const UiTheme& theme, Rect boxRect, keel::f32 cornerRadius, Color fill, Color stroke, keel::f32 strokeWidth, Color defaultMarkColor, bool pressed, bool focused, bool disabled) const override;
    void DrawTabBarBaseline(ICanvas& canvas, const UiTheme& theme, Rect barRect) const override;
    [[nodiscard]] keel::f32 DrawTabChrome(ICanvas& canvas, const UiTheme& theme, Rect bounds, TabChromeStyle style, bool active, bool hovered, bool focused, bool disabled, bool showSeparator, Color accentCol, Color pillActiveCol, keel::f32 radius) const override;
    void DrawMenuBarTrigger(ICanvas& canvas, const UiTheme& theme, Rect rect, keel::f32 radius, bool hovered, bool open, bool disabled, Color& outTextColor, Vec2& outTextShift) const override;
    [[nodiscard]] bool DrawMenuItemHighlight(ICanvas& canvas, const UiTheme& theme, Rect itemRect, keel::f32 itemRadius, bool hovered, bool selected, bool subExpanded, bool isDanger, bool disabled, Color dangerColor) const override;
    void DrawBadge(ICanvas& canvas, const UiTheme& theme, Rect rect, Color bg, Color border, keel::f32 borderWidth, keel::f32 fallbackRadius) const override;
    void DrawScrollbar(ICanvas& canvas, const UiTheme& theme, const ScrollbarPaintParams& p) const override;
    void DrawScrollbarCorner(ICanvas& canvas, const UiTheme& theme, Rect cornerRect) const override;

    // Reusable low-level 3D bevel drawing helpers available to custom painters
    static void DrawBevelFrame(ICanvas& canvas, Rect r, const BevelPalette& c, bool sunken, bool thin = false);
    static void DrawBevelEtchedFrame(ICanvas& canvas, Rect r, const BevelPalette& c);
    static void DrawBevelDividerH(ICanvas& canvas, keel::f32 x0, keel::f32 x1, keel::f32 y, const BevelPalette& c);
    static void DrawBevelDividerV(ICanvas& canvas, keel::f32 x, keel::f32 y0, keel::f32 y1, const BevelPalette& c);
    static void DrawBevelSliderThumb(ICanvas& canvas, Vec2 center, bool vertical, Color fill, const BevelPalette& c, Color focusRing, bool focused);
};

/// Returns the immutable singleton instance of `DefaultStylePainter`.
[[nodiscard]] const IStylePainter& GetDefaultStylePainter() noexcept;

} // namespace guikit
