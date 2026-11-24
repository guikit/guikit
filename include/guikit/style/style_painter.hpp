#pragma once

#include "guikit/style/theme.hpp"
#include "guikit/style/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

class ICanvas;

/// Semantic role of a framed interactive control passed to `IStylePainter::DrawFrame`.
enum class FrameKind : keel::u8 {
    Surface = 0,     ///< Flat card / container backdrop
    Elevated,        ///< Raised dialog / elevated card
    Popup,           ///< Context menu / dropdown popup panel
    Input,           ///< Editable text field / select box (sunken in Classic3D, bottom-accent in Fluent)
    Button,          ///< Push button (raised, sunken when pressed/active)
    ToolbarButton,   ///< Flat ghost toolbar / chrome button (thin bevel on hover/active in Classic3D)
    Badge,           ///< Tag / heading badge / status cell
    DropdownButton   ///< Embedded field trigger button (Select chevron, DatePicker, SpinBox steppers)
};

/// Tab bar chrome style passed to `IStylePainter::DrawTabChrome` without pulling action headers into style/.
enum class TabChromeStyle : keel::u8 {
    Underline = 0,
    Card,
    Pill,
    Folder
};

/// Parameters describing a container/overlay/view surface rendered via `DrawSurfaceBg` / `DrawSurfaceFrame`.
struct SurfacePaintParams {
    Rect        rect{};
    SurfaceRole role{SurfaceRole::Flat};
    DockSeam    dockSeam{DockSeam::None};
    keel::f32   radius{0.0f};
    bool        explicitRadius{false};
    Color       bg{Color::Transparent()};
    Color       border{Color::Transparent()};
    keel::f32   borderWidth{0.0f};
    Elevation   elevation{Elevation::None};
    keel::f32   shadowScale{1.0f};
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
    Color     accent{Color::Transparent()}; ///< Optional semantic accent color (used e.g. by Fluent BottomAccent focus bar)
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
/// All 8 element classes delegate visual physics (shadows, fills, borders, 3D bevels, directional 1px lighting,
/// bottom-accent fields, selection bars, dividers, tabs, toggles, sliders, progress bars, badges, scrollbars)
/// to `theme.Painter()`, reading strictly from `theme.palette`, `theme.bevel`, and `theme.chrome`.
class IStylePainter {
public:
    virtual ~IStylePainter() noexcept = default;

    /// Draws the background & elevation shadow of a container/overlay/view surface (before children).
    virtual void DrawSurfaceBg(
        ICanvas& canvas,
        const UiTheme& theme,
        const SurfacePaintParams& p
    ) const = 0;

    /// Draws the outer border / 3D bevel / directional lighting / dock seam of a surface (after children).
    virtual void DrawSurfaceFrame(
        ICanvas& canvas,
        const UiTheme& theme,
        const SurfacePaintParams& p
    ) const = 0;

    /// Convenience helper drawing a self-contained surface (`DrawSurfaceBg` followed by `DrawSurfaceFrame`).
    void DrawSurface(
        ICanvas& canvas,
        const UiTheme& theme,
        const SurfacePaintParams& p
    ) const {
        DrawSurfaceBg(canvas, theme, p);
        DrawSurfaceFrame(canvas, theme, p);
    }

    /// Draws elevation shadow, fill background, border/bevel/lighting, and focus indicator for a control frame.
    virtual void DrawFrame(
        ICanvas& canvas,
        const UiTheme& theme,
        FrameKind kind,
        const FramePaintParams& p
    ) const = 0;

    /// Returns horizontal/vertical content press-shift in dp for buttons/triggers (0dp in Modern/Fluent, 1dp in Classic3D/Brutalist).
    [[nodiscard]] virtual keel::f32 GetButtonContentOffset(
        const UiTheme& theme,
        bool pressed,
        bool active,
        bool disabled
    ) const noexcept = 0;

    /// Draws a horizontal or vertical divider / separator line (hairline, etched 3D groove, or bold stroke).
    virtual void DrawDivider(
        ICanvas& canvas,
        const UiTheme& theme,
        Vec2 p0,
        Vec2 p1,
        Color fallbackColor,
        keel::f32 thickness = 1.0f
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

    /// Draws a continuous, indeterminate, or segmented progress bar.
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

    /// Draws a radio circle and returns the resolved inner dot ink color.
    [[nodiscard]] virtual Color DrawRadioCircle(
        ICanvas& canvas,
        const UiTheme& theme,
        Vec2 center,
        keel::f32 radius,
        Color fill,
        Color stroke,
        keel::f32 strokeWidth,
        Color defaultDotColor,
        bool checked,
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

    /// Draws a row highlight for menus, dropdown lists, command palettes, sidebars, tables, and trees.
    /// Returns true when the row uses solid accent highlight requiring `palette.accentText` foreground.
    [[nodiscard]] virtual bool DrawSelectionHighlight(
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

    void DrawSurfaceBg(ICanvas& canvas, const UiTheme& theme, const SurfacePaintParams& p) const override;
    void DrawSurfaceFrame(ICanvas& canvas, const UiTheme& theme, const SurfacePaintParams& p) const override;
    void DrawFrame(ICanvas& canvas, const UiTheme& theme, FrameKind kind, const FramePaintParams& p) const override;
    [[nodiscard]] keel::f32 GetButtonContentOffset(const UiTheme& theme, bool pressed, bool active, bool disabled) const noexcept override;
    void DrawDivider(ICanvas& canvas, const UiTheme& theme, Vec2 p0, Vec2 p1, Color fallbackColor, keel::f32 thickness = 1.0f) const override;
    void DrawControlText(ICanvas& canvas, const UiTheme& theme, keel::StringRef text, Rect rect, keel::f32 fontSize, Color color, bool disabled, keel::f32 alignX = 0.5f, keel::f32 alignY = 0.5f) const override;
    void DrawSliderTrack(ICanvas& canvas, const UiTheme& theme, Rect trackRect, Rect fillRect, bool vertical, bool hasFill, Color trackBg, Color trackBorder, Color fillColor, keel::f32 cornerRadius, bool explicitColor) const override;
    void DrawSliderThumb(ICanvas& canvas, const UiTheme& theme, Vec2 center, keel::f32 thumbSize, bool vertical, Color bg, Color border, keel::f32 borderWidth, bool hovered, bool active, bool focused, bool disabled, keel::f32 cornerRadius) const override;
    void DrawProgressBar(ICanvas& canvas, const UiTheme& theme, Rect trackRect, keel::f32 ratio, bool vertical, bool indeterminate, keel::f32 animPhase, Color trackBg, Color trackBorder, Color fillColor, keel::f32 cornerRadius, bool disabled) const override;
    void DrawSwitch(ICanvas& canvas, const UiTheme& theme, Rect trackRect, keel::f32 knobDiameter, keel::f32 knobPadding, Color trackFill, Color trackStroke, keel::f32 strokeWidth, Color knobFill, bool checked, bool focused, bool disabled, keel::f32 themeRadius) const override;
    [[nodiscard]] Color DrawCheckboxBox(ICanvas& canvas, const UiTheme& theme, Rect boxRect, keel::f32 cornerRadius, Color fill, Color stroke, keel::f32 strokeWidth, Color defaultMarkColor, bool pressed, bool focused, bool disabled) const override;
    [[nodiscard]] Color DrawRadioCircle(ICanvas& canvas, const UiTheme& theme, Vec2 center, keel::f32 radius, Color fill, Color stroke, keel::f32 strokeWidth, Color defaultDotColor, bool checked, bool pressed, bool focused, bool disabled) const override;
    void DrawTabBarBaseline(ICanvas& canvas, const UiTheme& theme, Rect barRect) const override;
    [[nodiscard]] keel::f32 DrawTabChrome(ICanvas& canvas, const UiTheme& theme, Rect bounds, TabChromeStyle style, bool active, bool hovered, bool focused, bool disabled, bool showSeparator, Color accentCol, Color pillActiveCol, keel::f32 radius) const override;
    void DrawMenuBarTrigger(ICanvas& canvas, const UiTheme& theme, Rect rect, keel::f32 radius, bool hovered, bool open, bool disabled, Color& outTextColor, Vec2& outTextShift) const override;
    [[nodiscard]] bool DrawSelectionHighlight(ICanvas& canvas, const UiTheme& theme, Rect itemRect, keel::f32 itemRadius, bool hovered, bool selected, bool subExpanded, bool isDanger, bool disabled, Color dangerColor) const override;
    void DrawBadge(ICanvas& canvas, const UiTheme& theme, Rect rect, Color bg, Color border, keel::f32 borderWidth, keel::f32 fallbackRadius) const override;
    void DrawScrollbar(ICanvas& canvas, const UiTheme& theme, const ScrollbarPaintParams& p) const override;
    void DrawScrollbarCorner(ICanvas& canvas, const UiTheme& theme, Rect cornerRect) const override;

    // Reusable low-level 3D bevel helpers exposed to subclasses
    static void DrawBevelFrame(ICanvas& canvas, Rect r, const BevelPalette& c, bool sunken = false, bool thin = false);
    static void DrawBevelEtchedFrame(ICanvas& canvas, Rect r, const BevelPalette& c);
    static void DrawBevelDividerH(ICanvas& canvas, keel::f32 x0, keel::f32 x1, keel::f32 y, const BevelPalette& c);
    static void DrawBevelDividerV(ICanvas& canvas, keel::f32 x, keel::f32 y0, keel::f32 y1, const BevelPalette& c);
    static void DrawBevelSliderThumb(ICanvas& canvas, Vec2 center, bool vertical, Color fill, const BevelPalette& c, Color focusRing, bool focused);
};

/// Returns the process-wide default `DefaultStylePainter` singleton.
[[nodiscard]] const IStylePainter& GetDefaultStylePainter() noexcept;

} // namespace guikit
