#pragma once

#include "guikit/style/theme.hpp"
#include "guikit/style/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"
#include "keel/math.h"

namespace guikit {

class ICanvas;

/// Semantic role of an interactive control frame passed to `IStylePainter::DrawFrame`.
/// (Container/panel/popup surfaces use `DrawSurfaceBg` / `DrawSurfaceFrame` with `SurfaceRole`.)
enum class FrameKind : keel::u8 {
    Input = 0,       ///< Editable text field / select box (sunken in Classic3D, bottom-accent in Fluent)
    Button,          ///< Push button (raised, sunken when pressed/active)
    ToolbarButton,   ///< Flat ghost toolbar / chrome button (thin bevel on hover/active in Classic3D)
    Badge,           ///< Tag / heading badge / status cell
    DropdownButton,  ///< Embedded field trigger button (Select chevron, DatePicker, SpinBox steppers)
    Keycap           ///< Keyboard shortcut keycap pill (raised 1px 3D key in Classic3D, subtle bordered pill otherwise)
};

/// Chrome-driven layout & geometry metrics returned by `IStylePainter::GetChromeMetrics`.
struct ChromeMetrics {
    keel::f32  buttonPressOffset{0.0f};      ///< Pressed/active button content shift in dp
    keel::f32  frameBevelInset{0.0f};        ///< Inner frame/bevel inset in dp (e.g. 2.0f for Bevel3D)
    keel::f32  tabGapOverride{-1.0f};        ///< Explicit TabBar gap in dp (< 0 uses theme default)
    keel::f32  sliderTrackHeight{0.0f};      ///< Fixed slider track height in dp (<= 0 uses widget default)
    keel::f32  rangeSliderTrackHeight{0.0f}; ///< Fixed range slider track height in dp (<= 0 uses widget default)
    keel::f32  progressBarMinHeight{0.0f};   ///< Minimum progress bar thickness in dp
    keel::f32  progressBarHeightBonus{0.0f}; ///< Extra progress bar thickness in dp (e.g. segmented 3D well)
    BadgeShape badgeShape{BadgeShape::Pill}; ///< Badge/tag corner rounding policy
    bool       solidSelectionHighlight{false}; ///< True when selected rows use solid accent fill requiring accentText foreground
    bool       recessedRegions{false};         ///< True when dialog/overlay inputs & lists render as inset sunken wells (Bevel3D)
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
    bool        explicitBorder{false};
    Elevation   elevation{Elevation::None};
    keel::f32   shadowScale{1.0f};
    const Gradient* bgGradient{nullptr};     ///< Optional explicit per-surface background gradient (arena/stack lifetime)
    const Gradient* borderGradient{nullptr}; ///< Optional explicit per-surface border gradient (arena/stack lifetime)
};

/// Parameters describing a generic control or surface frame for `IStylePainter::DrawFrame`.
struct FramePaintParams {
    Rect      rect{};
    keel::f32 radius{0.0f};
    bool      explicitRadius{false};
    Color     bg{Color::Transparent()};
    Color     border{Color::Transparent()};
    keel::f32 borderWidth{0.0f};
    bool      explicitBorder{false};
    bool      hovered{false};
    bool      pressed{false};
    bool      active{false};
    bool      focused{false};
    bool      disabled{false};
    Elevation elevation{Elevation::None};
    keel::f32 shadowScale{1.0f};
    Color     accent{Color::Transparent()}; ///< Optional semantic accent color (used e.g. by Fluent BottomAccent focus bar)
    bool      hasStatusAccent{false};       ///< True when field has an active semantic status (Success/Warning/Danger/Info)
    const Gradient* bgGradient{nullptr};     ///< Optional explicit per-control background gradient
    const Gradient* borderGradient{nullptr}; ///< Optional explicit per-control border gradient
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

/// Unified parameters for `IStylePainter::DrawSliderThumb`.
struct SliderThumbPaintParams {
    Vec2      center{};
    keel::f32 thumbSize{13.5f};
    bool      vertical{false};
    Color     accent{Color::Transparent()}; ///< Semantic active/accent color for this slider (falls back to `palette.accent`)
    bool      hovered{false};
    bool      active{false};
    bool      focused{false};
    bool      disabled{false};
    keel::f32 cornerRadius{0.0f};
};

/// Direction of a dockable panel collapse/expand edge toggle arrow (`BottomPanel` / `Sidebar`).
enum class PanelToggleDirection : keel::u8 {
    Up = 0,
    Down,
    Left,
    Right
};

/// Parameters describing an edge collapse/expand toggle button for `IStylePainter::DrawPanelToggle`.
struct PanelTogglePaintParams {
    Rect                 rect{};
    PanelToggleDirection direction{PanelToggleDirection::Down};
    bool                 hovered{false};
    bool                 pressed{false};
    bool                 highlighted{false};
    keel::f32            scale{1.0f};
};

/// Parameters describing a single Table column header cell for `IStylePainter::DrawTableHeaderCell`.
struct TableHeaderCellPaintParams {
    Rect rect{};
    bool hovered{false};
    bool pressed{false};
    bool sorted{false};
    bool sortable{false};
    bool dividerActive{false};
    bool showRightDivider{true};
    bool isFrozenSeam{false};
};

/// Semantic kind of a CSD window control button rendered by `IStylePainter::DrawWindowControlButton`.
enum class WindowControlKind : keel::u8 {
    Minimize = 0,
    Maximize,
    Close
};

/// Chrome-driven layout & color metrics for `guikit::HeaderBar` returned by `IStylePainter::GetHeaderBarMetrics`.
struct HeaderBarChromeMetrics {
    Color     titleColor{};
    Color     subtitleColor{};
    keel::f32 defaultHeightDelta{0.0f}; ///< Optional height adjustment in dp (e.g. -6.0f for compact Classic3D title bar)
    keel::f32 controlBtnWidth{38.0f};
    keel::f32 controlBtnHeight{0.0f};   ///< <= 0 means barHeight - 6.0f
    keel::f32 controlGap{6.0f};
    keel::f32 closeExtraGap{0.0f};      ///< Extra separation before Close button (e.g. Win98 `[ _ ][ □ ]  [ × ]`)
    keel::f32 rightEdgePadding{4.0f};
    bool      accentCaption{false};     ///< True when header renders a solid/gradient accent title bar (e.g. Bevel3D)
};

/// Parameters describing a `HeaderBar` background & bottom border for `IStylePainter::DrawHeaderBar`.
struct HeaderBarPaintParams {
    Rect      rect{};
    Color     bg{Color::Transparent()};
    bool      explicitBg{false};
    Color     borderColor{Color::Transparent()};
    keel::f32 borderWidth{1.0f};
    bool      drawBottomBorder{true};
    bool      focused{true};
};

/// Parameters describing a CSD window control button for `IStylePainter::DrawWindowControlButton`.
struct WindowControlPaintParams {
    Rect              rect{};
    WindowControlKind kind{WindowControlKind::Minimize};
    bool              maximized{false};
    bool              focused{true};
    bool              macStyle{false};
    bool              hovered{false};
    bool              pressed{false};
    bool              disabled{false};
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

    /// Returns chrome-driven geometry & sizing metrics for the active theme.
    [[nodiscard]] virtual ChromeMetrics GetChromeMetrics(
        const UiTheme& theme
    ) const noexcept = 0;

    /// Returns horizontal/vertical content press-shift in dp for buttons/triggers.
    [[nodiscard]] keel::f32 GetButtonContentOffset(
        const UiTheme& theme,
        bool pressed,
        bool active,
        bool disabled
    ) const noexcept {
        return (!disabled && (pressed || active)) ? GetChromeMetrics(theme).buttonPressOffset : 0.0f;
    }

    /// Returns resolved horizontal gap between consecutive `TabBar` items.
    [[nodiscard]] keel::f32 GetTabGap(
        const UiTheme& theme,
        keel::f32 fallbackGap
    ) const noexcept {
        const keel::f32 overrideGap = GetChromeMetrics(theme).tabGapOverride;
        return (overrideGap >= 0.0f) ? overrideGap : fallbackGap;
    }

    /// Returns effective slider track thickness in dp for the active chrome.
    [[nodiscard]] keel::f32 GetSliderTrackHeight(
        const UiTheme& theme,
        keel::f32 defaultHeight,
        bool isRange = false
    ) const noexcept {
        if (theme.sliderStyle.trackHeight > 0.0f) {
            return defaultHeight;
        }
        const ChromeMetrics m = GetChromeMetrics(theme);
        const keel::f32 fixed = isRange ? m.rangeSliderTrackHeight : m.sliderTrackHeight;
        return (fixed > 0.0f) ? fixed : defaultHeight;
    }

    /// Returns effective progress bar thickness in dp for the active chrome.
    [[nodiscard]] keel::f32 GetProgressBarThickness(
        const UiTheme& theme,
        keel::f32 defaultThickness
    ) const noexcept {
        const ChromeMetrics m = GetChromeMetrics(theme);
        if (m.progressBarMinHeight > 0.0f || m.progressBarHeightBonus > 0.0f) {
            return keel::max(defaultThickness + m.progressBarHeightBonus, m.progressBarMinHeight);
        }
        return defaultThickness;
    }

    /// Returns effective badge/tag corner radius in dp honoring `chrome.badgeShape`.
    [[nodiscard]] keel::f32 GetBadgeRadius(
        const UiTheme& theme,
        keel::f32 height,
        keel::f32 defaultRadius,
        bool pill
    ) const noexcept {
        const BadgeShape shape = GetChromeMetrics(theme).badgeShape;
        if (shape == BadgeShape::Sharp) return 0.0f;
        if (pill && shape == BadgeShape::Pill) return height * 0.5f;
        return defaultRadius;
    }

    /// Draws an embedded right-side field trigger button (e.g., Select/DatePicker/ColorPicker chevron) according to
    /// `chrome.fieldButton`, writing the resolved icon rect & color and returning the remaining `rightX` for content.
    [[nodiscard]] virtual keel::f32 DrawFieldButton(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect fieldRect,
        keel::f32 paddingX,
        keel::f32 iconSize,
        keel::f32 cornerRadius,
        bool explicitRadius,
        bool hovered,
        bool pressed,
        bool active,
        bool disabled,
        bool showHoverPill,
        Color fallbackIconCol,
        Rect& outIconRect,
        Color& outIconColor
    ) const = 0;

    /// Draws a floating dropdown list surface (e.g., Select/Autocomplete/MultiSelect popup) according to active chrome.
    virtual void DrawDropdownSurface(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect popupRect,
        keel::f32 cornerRadius
    ) const = 0;

    /// Draws a single discrete slider tick mark (below/beside track for `Pointer3D` scaled to `thumbSize`, centered dot otherwise).
    virtual void DrawSliderTick(
        ICanvas& canvas,
        const UiTheme& theme,
        Vec2 tickCenter,
        keel::f32 crossCoord,
        bool vertical,
        keel::f32 tickSize,
        keel::f32 thumbSize,
        Color color,
        bool disabled
    ) const = 0;

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

    /// Draws a slider / step-slider / range-slider thumb knob according to `theme.chrome.sliderThumb`.
    virtual void DrawSliderThumb(
        ICanvas& canvas,
        const UiTheme& theme,
        const SliderThumbPaintParams& p
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

    /// Draws an interactive SplitView resize sash and optional grip handle according to active chrome.
    /// `verticalBar` is true for a vertical sash bar separating left/right panes (`Orientation::Horizontal`).
    virtual void DrawSplitSash(
        ICanvas& canvas,
        const UiTheme& theme,
        Rect sashRect,
        bool verticalBar,
        bool showGrip,
        bool hovered,
        bool dragging,
        bool disabled
    ) const = 0;

    /// Draws a dockable panel collapse/expand edge toggle button (`BottomPanel` / `Sidebar`) according to active chrome.
    virtual void DrawPanelToggle(
        ICanvas& canvas,
        const UiTheme& theme,
        const PanelTogglePaintParams& p
    ) const = 0;

    /// Draws a single Table column header cell (background, 3D bevel or flat divider) according to active chrome.
    virtual void DrawTableHeaderCell(
        ICanvas& canvas,
        const UiTheme& theme,
        const TableHeaderCellPaintParams& p
    ) const = 0;

    /// Returns chrome-driven geometry & foreground ink tokens for `guikit::HeaderBar`.
    [[nodiscard]] virtual HeaderBarChromeMetrics GetHeaderBarMetrics(
        const UiTheme& theme,
        bool focused
    ) const noexcept = 0;

    /// Draws the `HeaderBar` surface (classic 3D accent gradient bar, glass, luminous, or modern strip) and bottom seam.
    virtual void DrawHeaderBar(
        ICanvas& canvas,
        const UiTheme& theme,
        const HeaderBarPaintParams& p
    ) const = 0;

    /// Draws a CSD window control button (`Minimize`, `Maximize/Restore`, `Close`) in the active chrome style.
    virtual void DrawWindowControlButton(
        ICanvas& canvas,
        const UiTheme& theme,
        const WindowControlPaintParams& p
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
    [[nodiscard]] ChromeMetrics GetChromeMetrics(const UiTheme& theme) const noexcept override;
    [[nodiscard]] keel::f32 DrawFieldButton(ICanvas& canvas, const UiTheme& theme, Rect fieldRect, keel::f32 paddingX, keel::f32 iconSize, keel::f32 cornerRadius, bool explicitRadius, bool hovered, bool pressed, bool active, bool disabled, bool showHoverPill, Color fallbackIconCol, Rect& outIconRect, Color& outIconColor) const override;
    void DrawDropdownSurface(ICanvas& canvas, const UiTheme& theme, Rect popupRect, keel::f32 cornerRadius) const override;
    void DrawSliderTick(ICanvas& canvas, const UiTheme& theme, Vec2 tickCenter, keel::f32 crossCoord, bool vertical, keel::f32 tickSize, keel::f32 thumbSize, Color color, bool disabled) const override;
    void DrawDivider(ICanvas& canvas, const UiTheme& theme, Vec2 p0, Vec2 p1, Color fallbackColor, keel::f32 thickness = 1.0f) const override;
    void DrawControlText(ICanvas& canvas, const UiTheme& theme, keel::StringRef text, Rect rect, keel::f32 fontSize, Color color, bool disabled, keel::f32 alignX = 0.5f, keel::f32 alignY = 0.5f) const override;
    void DrawSliderTrack(ICanvas& canvas, const UiTheme& theme, Rect trackRect, Rect fillRect, bool vertical, bool hasFill, Color trackBg, Color trackBorder, Color fillColor, keel::f32 cornerRadius, bool explicitColor) const override;
    void DrawSliderThumb(ICanvas& canvas, const UiTheme& theme, const SliderThumbPaintParams& p) const override;
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
    void DrawSplitSash(ICanvas& canvas, const UiTheme& theme, Rect sashRect, bool verticalBar, bool showGrip, bool hovered, bool dragging, bool disabled) const override;
    void DrawPanelToggle(ICanvas& canvas, const UiTheme& theme, const PanelTogglePaintParams& p) const override;
    void DrawTableHeaderCell(ICanvas& canvas, const UiTheme& theme, const TableHeaderCellPaintParams& p) const override;
    [[nodiscard]] HeaderBarChromeMetrics GetHeaderBarMetrics(const UiTheme& theme, bool focused) const noexcept override;
    void DrawHeaderBar(ICanvas& canvas, const UiTheme& theme, const HeaderBarPaintParams& p) const override;
    void DrawWindowControlButton(ICanvas& canvas, const UiTheme& theme, const WindowControlPaintParams& p) const override;

    // Reusable low-level 3D bevel helpers exposed to subclasses
    static void DrawBevelFrame(ICanvas& canvas, Rect r, const BevelPalette& c, bool sunken = false, bool thin = false);
    static void DrawBevelEtchedFrame(ICanvas& canvas, Rect r, const BevelPalette& c);
    static void DrawBevelDividerH(ICanvas& canvas, keel::f32 x0, keel::f32 x1, keel::f32 y, const BevelPalette& c);
    static void DrawBevelDividerV(ICanvas& canvas, keel::f32 x, keel::f32 y0, keel::f32 y1, const BevelPalette& c);
    static void DrawBevelSliderThumb(ICanvas& canvas, Vec2 center, keel::f32 thumbSize, bool vertical, Color fill, const BevelPalette& c, Color focusRing, bool focused);
};

/// Returns the process-wide default `DefaultStylePainter` singleton.
[[nodiscard]] const IStylePainter& GetDefaultStylePainter() noexcept;

} // namespace guikit
