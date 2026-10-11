#pragma once

#include <cstdint>
#include <string_view>
#include "guikit/core/Types.hpp"

namespace guikit {

/// Scale for density, typography, spacing, and component dimensions.
enum class UiSize : uint8_t {
    None = 0, ///< Zero / unpadded / no gap / square corners
    Xs,       ///< Extra Small (compact toolbars, inline badges, dense data tables)
    Sm,       ///< Small (secondary panels, compact forms)
    Md,       ///< Medium (standard default controls across the engine)
    Lg,       ///< Large (prominent action bars, touch-friendly interfaces)
    Xl        ///< Extra Large (hero actions, emergency stops, kiosk displays)
};

/// Semantic visual variants for colors, states, and emphasis across all widgets.
enum class UiVariant : uint8_t {
    Default,    ///< Neutral base styling
    Primary,    ///< Primary brand/call-to-action color
    Secondary,  ///< Outline / subtle secondary action
    Success,    ///< Positive / confirmation / success (green)
    Warning,    ///< Caution / alert (amber)
    Danger,     ///< Destructive / emergency / error (red)
    Info,       ///< Informational / accent (blue/cyan)
    Ghost,      ///< Transparent background with hover highlighting
    Muted       ///< Subdued / secondary styling (slate-400)
};

/// Universal relative placement for companion elements (icons, labels, badges, adornments).
enum class Placement : uint8_t {
    Leading,    ///< Placed before primary element (left / start)
    Trailing,   ///< Placed after primary element (right / end)
    Top,        ///< Placed above primary element
    Bottom,     ///< Placed below primary element
    Stacked     ///< Form standard layout: Label on top, primary control in middle, Description on bottom
};

/// 6x3 Typographic Role Matrix (Material 3 scale extended for desktop/engineering).
enum class TextRole : uint8_t {
    // Display (hero metrics, large gauges, KPI counters)
    DisplayLarge,   ///< 40px / LH 48px
    DisplayMedium,  ///< 32px / LH 40px
    DisplaySmall,   ///< 28px / LH 36px

    // Headline (primary window and screen titles)
    HeadlineLarge,  ///< 24px / LH 32px
    HeadlineMedium, ///< 22px / LH 28px
    HeadlineSmall,  ///< 20px / LH 26px

    // Title (section headers, card titles, modal titles)
    TitleLarge,     ///< 18px / LH 24px
    TitleMedium,    ///< 16px / LH 22px
    TitleSmall,     ///< 15px / LH 20px

    // Body (standard content, forms, table values)
    BodyLarge,      ///< 16px / LH 24px
    BodyMedium,     ///< 14px / LH 20px (Default)
    BodySmall,      ///< 13px / LH 18px (Dense)

    // Label (control captions, badges, toolbars, hints)
    LabelLarge,     ///< 14px / LH 18px
    LabelMedium,    ///< 12px / LH 16px
    LabelSmall,     ///< 11px / LH 14px

    // Code (monospace telemetry, registers, hex dumps, log traces)
    CodeLarge,      ///< 15px / LH 20px
    CodeMedium,     ///< 13px / LH 18px
    CodeSmall       ///< 11px / LH 15px
};

/// Typographic metrics ratio representing line-height to font-size for standard UI text.
inline constexpr float kDefaultLineHeightMultiplier = 1.35f;

/// Typographic metrics ratio representing average character advance to font-size for proportional text fallback.
inline constexpr float kDefaultCharAdvanceRatio = 0.58f;

/// Typographic metrics ratio representing average character advance to font-size for monospace text fallback.
inline constexpr float kMonospaceCharAdvanceRatio = 0.62f;

/// Metrics associated with a TextRole.
struct TypographyMetrics {
    float fontSize;
    float lineHeight;
    bool isMonospace;
};

/// Standardized component metrics associated with each UiSize.
struct ComponentMetrics {
    float defaultHeight;
    float fontSize;
    float paddingX;
    float paddingY;
    float cornerRadius;
    float iconSize;
};

/// Standardized metrics for scrollbars derived from UiSize tokens.
struct ScrollbarMetrics {
    float trackWidth;      ///< Thickness of the scrollbar track and thumb in points
    float minThumbLength;  ///< Minimum thumb size preventing it from becoming unclickable
    float cornerRadius;    ///< Rounding radius of the thumb pill
    float scrollStep;      ///< Pixels / points scrolled per mouse wheel tick
    float margin;          ///< Margin between track and viewport edge / content
    float gutter;          ///< Total reserved gutter (trackWidth + 2 * margin)
};

/// Standardized metrics for Switch widget according to UiSize.
struct SwitchMetrics {
    Vec2 trackSize;
    float knobDiameter;
    float knobPadding;
    float gap;
    float fontSize;
    float descFontSize;
    float descGap;
};

/// Standardized metrics for Checkbox widget according to UiSize.
struct CheckboxMetrics {
    float boxSize;
    float cornerRadius;
    float gap;
    float fontSize;
    float descFontSize;
    float descGap;
    float strokeWidth;
};

/// Standardized metrics for InputField widget according to UiSize.
struct InputFieldMetrics {
    float defaultHeight;
    float cornerRadius;
    float paddingX;
    float fontSize;
    float labelFontSize;
    float descFontSize;
    float labelGap;
    float descGap;
    float strokeWidth;
    float iconSize;
};

/// Standardized metrics for Select widget according to UiSize.
struct SelectMetrics {
    float defaultHeight;
    float cornerRadius;
    float paddingX;
    float fontSize;
    float labelFontSize;
    float descFontSize;
    float labelGap;
    float descGap;
    float strokeWidth;
    float iconSize;
    float itemHeight;
};

/// Standardized metrics for Heading widget according to UiSize.
struct HeadingMetrics {
    float titleFontSize;
    float subtitleFontSize;
    float badgeFontSize;
    float iconSize;
    float gap;
    float badgeGap;
    float lineGap;
};

/// Density scale for component metrics and touch targets.
enum class Density : uint8_t {
    Standard,  ///< Standard desktop productivity scale
    Compact,   ///< High-density engineering data / dense tabular workflows
    Touch      ///< Touch-friendly enlarged targets and generous spacing
};

/// Universal color theme modes.
enum class ThemeMode : uint8_t {
    Dark,
    Light
};

/// Color palette definition for an entire theme.
struct ThemePalette {
    // 1. Surface and viewport backgrounds
    Color background;       ///< Main viewport clear color (#0B0F19 dark / #F1F5F9 light)
    Color surface;          ///< Card / container fill (#1E293B dark / #FFFFFF light)
    Color surfaceHover;     ///< Hovered container / list item fill (#33415580 dark / #F1F5F9FF light)
    Color surfaceActive;    ///< Active / pressed item fill (#334155FF dark / #E2E8F0FF light)
    Color surfaceElevated;  ///< Dropdowns, floating popups, toolbars (#1E293B dark / #FFFFFF light)

    // 2. Borders and dividers
    Color border;           ///< Card / container border (#334155 dark / #E2E8F0 light)
    Color borderHover;      ///< Hovered control / card border (#64748B dark / #94A3B8 light)
    Color borderSubtle;     ///< Subtle separators and dividers (#33415580 dark / #E2E8F0FF light)
    Color focusRing;        ///< Focus outline glow (#38BDF8FF Sky-400 / #0284C7FF Sky-600)

    // 3. Text hierarchy
    Color textPrimary;      ///< High-contrast primary text (#F8FAFC dark / #0F172A light)
    Color textSecondary;    ///< Medium-contrast muted text (#94A3B8 dark / #64748B light)
    Color textMuted;        ///< Low-contrast subtle text (#64748B dark / #94A3B8 light)
    Color textDisabled;     ///< Disabled interaction text (#64748B80 dark / #94A3B880 light)

    // 4. Text input fields
    Color inputBackground;  ///< Textbox / input slot fill (#0F172A dark / #F8FAFC light)
    Color inputBorder;      ///< Textbox idle border (#334155 dark / #CBD5E1 light)
    Color caret;            ///< Text cursor caret (#38BDF8FF dark / #0284C7FF light)

    // 5. Interactive controls (Switch, Checkbox, Slider, Radio)
    Color controlTrack;        ///< Idle track fill of switch / checkbox (#1E293B dark / #E2E8F0 light)
    Color controlTrackHover;   ///< Hovered track fill (#334155 dark / #CBD5E1 light)
    Color controlKnob;         ///< Idle switch thumb / checkbox uncheck (#94A3B8 dark / #F8FAFC light)
    Color controlKnobHover;    ///< Hovered switch thumb (#E2E8F0 dark / #FFFFFF light)
    Color controlKnobActive;   ///< Checked thumb / active marker (#FFFFFF dark / #FFFFFF light)

    // 6. Accent brand
    Color accent;           ///< Primary brand / highlight color (#38BDF8 Sky-400 / #0284C7 Sky-600)
    Color accentHover;      ///< Primary brand hover color (#7DD3FC Sky-300 / #0369A1 Sky-700)
    Color accentText;       ///< Foreground text on accent fill (#0F172A dark / #FFFFFF light)
};

/// Color set for rendering a widget variant across its interaction states.
struct VariantColors {
    Color bg;
    Color bgHover;
    Color bgActive;
    Color text;
    Color border;
    float borderWidth{0.0f};
};

} // namespace guikit
