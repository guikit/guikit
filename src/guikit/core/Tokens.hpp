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

/// Metrics associated with a TextRole.
struct TypographyMetrics {
    float fontSize;
    float lineHeight;
    bool isMonospace;
};

/// Returns standardized typography metrics for a given TextRole.
[[nodiscard]] constexpr TypographyMetrics GetTypographyMetrics(TextRole role) noexcept {
    switch (role) {
        case TextRole::DisplayLarge:   return { 40.0f, 48.0f, false };
        case TextRole::DisplayMedium:  return { 32.0f, 40.0f, false };
        case TextRole::DisplaySmall:   return { 28.0f, 36.0f, false };

        case TextRole::HeadlineLarge:  return { 24.0f, 32.0f, false };
        case TextRole::HeadlineMedium: return { 22.0f, 28.0f, false };
        case TextRole::HeadlineSmall:  return { 20.0f, 26.0f, false };

        case TextRole::TitleLarge:     return { 18.0f, 24.0f, false };
        case TextRole::TitleMedium:    return { 16.0f, 22.0f, false };
        case TextRole::TitleSmall:     return { 15.0f, 20.0f, false };

        case TextRole::BodyLarge:      return { 16.0f, 24.0f, false };
        case TextRole::BodyMedium:     return { 14.0f, 20.0f, false };
        case TextRole::BodySmall:      return { 13.0f, 18.0f, false };

        case TextRole::LabelLarge:     return { 14.0f, 18.0f, false };
        case TextRole::LabelMedium:    return { 12.0f, 16.0f, false };
        case TextRole::LabelSmall:     return { 11.0f, 14.0f, false };

        case TextRole::CodeLarge:      return { 15.0f, 20.0f, true  };
        case TextRole::CodeMedium:     return { 13.0f, 18.0f, true  };
        case TextRole::CodeSmall:      return { 11.0f, 15.0f, true  };
    }
    return { 14.0f, 20.0f, false };
}

/// Semantic gap spacing tokens.
enum class Gap : uint8_t {
    None = 0,
    Xs   = 4,
    Sm   = 8,
    Md   = 12,
    Lg   = 16,
    Xl   = 24
};

[[nodiscard]] constexpr float ToPoints(Gap gap) noexcept {
    return static_cast<float>(static_cast<uint8_t>(gap));
}

/// Semantic corner radius tokens.
enum class Radius : uint8_t {
    None = 0,
    Xs   = 2,
    Sm   = 4,
    Md   = 6,
    Lg   = 8,
    Xl   = 12,
    Full = 99
};

[[nodiscard]] constexpr float ToPoints(Radius r) noexcept {
    return static_cast<float>(static_cast<uint8_t>(r));
}

/// Standardized component metrics associated with each UiSize.
struct ComponentMetrics {
    float defaultHeight;
    float fontSize;
    float paddingX;
    float paddingY;
    float cornerRadius;
    float iconSize;
};

/// Returns standardized component metrics for a given UiSize.
[[nodiscard]] constexpr ComponentMetrics GetComponentMetrics(UiSize size) noexcept {
    switch (size) {
        case UiSize::None: return {  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f };
        case UiSize::Xs:   return { 24.0f, 11.0f,  8.0f,  3.0f,  3.0f, 12.0f };
        case UiSize::Sm:   return { 32.0f, 13.0f, 12.0f,  5.0f,  4.0f, 15.0f };
        case UiSize::Md:   return { 40.0f, 15.0f, 16.0f,  8.0f,  6.0f, 18.0f };
        case UiSize::Lg:   return { 48.0f, 18.0f, 20.0f, 10.0f,  8.0f, 22.0f };
        case UiSize::Xl:   return { 56.0f, 22.0f, 24.0f, 12.0f, 10.0f, 26.0f };
    }
    return { 40.0f, 15.0f, 16.0f, 8.0f, 6.0f, 18.0f };
}

/// Standardized metrics for scrollbars derived from UiSize tokens.
struct ScrollbarMetrics {
    float trackWidth;      ///< Thickness of the scrollbar track and thumb in points
    float minThumbLength;  ///< Minimum thumb size preventing it from becoming unclickable
    float cornerRadius;    ///< Rounding radius of the thumb pill
    float scrollStep;      ///< Pixels / points scrolled per mouse wheel tick
    float margin;          ///< Margin between track and viewport edge / content
    float gutter;          ///< Total reserved gutter (trackWidth + 2 * margin)
};

/// Returns standardized scrollbar metrics for a given UiSize token.
[[nodiscard]] constexpr ScrollbarMetrics GetScrollbarMetrics(UiSize size) noexcept {
    switch (size) {
        case UiSize::None: return {  0.0f,  0.0f,  0.0f,  0.0f, 0.0f,  0.0f };
        case UiSize::Xs:   return {  4.0f, 16.0f,  2.0f, 24.0f, 2.0f,  8.0f };
        case UiSize::Sm:   return {  6.0f, 20.0f,  3.0f, 32.0f, 3.0f, 12.0f };
        case UiSize::Md:   return {  8.0f, 28.0f,  4.0f, 40.0f, 3.0f, 14.0f };
        case UiSize::Lg:   return { 10.0f, 36.0f,  5.0f, 48.0f, 4.0f, 18.0f };
        case UiSize::Xl:   return { 12.0f, 44.0f,  6.0f, 56.0f, 4.0f, 20.0f };
    }
    return { 8.0f, 28.0f, 4.0f, 40.0f, 3.0f, 14.0f };
}

/// Universal color theme modes.
enum class ThemeMode : uint8_t {
    Dark,
    Light
};

/// Color palette definition for an entire theme.
struct ThemePalette {
    Color background;       ///< Main viewport clear color (#0B0F19 dark / #F1F5F9 light)
    Color surface;          ///< Card / container fill (#1E293B dark / #FFFFFF light)
    Color surfaceElevated;  ///< Dropdowns, floating popups, toolbars (#1E293B dark / #FFFFFF light)
    Color border;           ///< Card / container border (#334155 dark / #E2E8F0 light)
    Color borderSubtle;     ///< Subtle separators and dividers (#33415580 dark / #E2E8F0FF light)
    Color textPrimary;      ///< High-contrast primary text (#F8FAFC dark / #0F172A light)
    Color textSecondary;    ///< Medium-contrast muted text (#94A3B8 dark / #64748B light)
    Color inputBackground;  ///< Textbox / input slot fill (#0F172A dark / #F8FAFC light)
    Color inputBorder;      ///< Textbox idle border (#334155 dark / #CBD5E1 light)
    Color accent;           ///< Primary brand / highlight color (#38BDF8 Sky-400 / #0284C7 Sky-600)
};

/// Resolves complete color palette tokens for a theme mode.
[[nodiscard]] constexpr ThemePalette GetThemePalette(ThemeMode mode) noexcept {
    if (mode == ThemeMode::Light) {
        return {
            .background       = Color::Hex(0xF1F5F9FF), // Slate-100
            .surface          = Color::Hex(0xFFFFFFFF), // Pure white
            .surfaceElevated  = Color::Hex(0xFFFFFFFF), // Pure white
            .border           = Color::Hex(0xE2E8F0FF), // Slate-200
            .borderSubtle     = Color::Hex(0xE2E8F0FF), // Slate-200
            .textPrimary      = Color::Hex(0x0F172AFF), // Slate-900 (crisp dark text)
            .textSecondary    = Color::Hex(0x64748BFF), // Slate-500 (muted text)
            .inputBackground  = Color::Hex(0xF8FAFCFF), // Slate-50
            .inputBorder      = Color::Hex(0xCBD5E1FF), // Slate-300
            .accent           = Color::Hex(0x0284C7FF)  // Sky-600
        };
    }
    // Dark mode (default)
    return {
        .background       = Color::Hex(0x0B0F19FF), // Slate-950
        .surface          = Color::Hex(0x1E293BFF), // Slate-800
        .surfaceElevated  = Color::Hex(0x1E293BFF), // Slate-800
        .border           = Color::Hex(0x334155FF), // Slate-700
        .borderSubtle     = Color::Hex(0x33415580), // Slate-700 / 50%
        .textPrimary      = Color::Hex(0xF8FAFCFF), // Slate-50 (crisp white text)
        .textSecondary    = Color::Hex(0x94A3B8FF), // Slate-400 (muted text)
        .inputBackground  = Color::Hex(0x0F172AFF), // Slate-900
        .inputBorder      = Color::Hex(0x334155FF), // Slate-700
        .accent           = Color::Hex(0x38BDF8FF)  // Sky-400
    };
}

/// Color set for rendering a widget variant across its interaction states.
struct VariantColors {
    Color bg;
    Color bgHover;
    Color bgActive;
    Color text;
    Color border;
    float borderWidth{0.0f};
};

/// Resolves colors for a semantic variant across light/dark themes.
[[nodiscard]] constexpr VariantColors GetVariantColors(UiVariant variant, ThemeMode theme = ThemeMode::Dark) noexcept {
    if (theme == ThemeMode::Light) {
        switch (variant) {
            case UiVariant::Primary:
                return {
                    Color::Hex(0x2563EBFF), // Blue-600
                    Color::Hex(0x3B82F6FF), // Blue-500
                    Color::Hex(0x1D4ED8FF), // Blue-700
                    Color::Hex(0xFFFFFFFF),
                    Color::Hex(0x00000000),
                    0.0f
                };
            case UiVariant::Success:
                return {
                    Color::Hex(0x059669FF), // Emerald-600
                    Color::Hex(0x10B981FF), // Emerald-500
                    Color::Hex(0x047857FF), // Emerald-700
                    Color::Hex(0xFFFFFFFF),
                    Color::Hex(0x00000000),
                    0.0f
                };
            case UiVariant::Warning:
                return {
                    Color::Hex(0xD97706FF), // Amber-600
                    Color::Hex(0xF59E0BFF), // Amber-500
                    Color::Hex(0xB45309FF), // Amber-700
                    Color::Hex(0xFFFFFFFF),
                    Color::Hex(0x00000000),
                    0.0f
                };
            case UiVariant::Danger:
                return {
                    Color::Hex(0xDC2626FF), // Red-600
                    Color::Hex(0xEF4444FF), // Red-500
                    Color::Hex(0xB91C1CFF), // Red-700
                    Color::Hex(0xFFFFFFFF),
                    Color::Hex(0x00000000),
                    0.0f
                };
            case UiVariant::Info:
                return {
                    Color::Hex(0x0891B2FF), // Cyan-600
                    Color::Hex(0x06B6D4FF), // Cyan-500
                    Color::Hex(0x0E7490FF), // Cyan-700
                    Color::Hex(0xFFFFFFFF),
                    Color::Hex(0x00000000),
                    0.0f
                };
            case UiVariant::Ghost:
                return {
                    Color::Hex(0x00000000),
                    Color::Hex(0x0F172A0D), // 5% black hover
                    Color::Hex(0x0F172A1A), // 10% black active
                    Color::Hex(0x334155FF), // Slate-700
                    Color::Hex(0x00000000),
                    0.0f
                };
            case UiVariant::Muted:
                return {
                    Color::Hex(0x00000000),
                    Color::Hex(0x00000000),
                    Color::Hex(0x00000000),
                    Color::Hex(0x64748BFF), // Slate-500
                    Color::Hex(0x00000000),
                    0.0f
                };
            case UiVariant::Default:
            case UiVariant::Secondary:
            default:
                return {
                    Color::Hex(0xE2E8F0FF), // Slate-200
                    Color::Hex(0xCBD5E1FF), // Slate-300
                    Color::Hex(0x94A3B8FF), // Slate-400
                    Color::Hex(0x0F172AFF), // Slate-900
                    Color::Hex(0xCBD5E1FF), // Slate-300
                    1.0f
                };
        }
    }

    // ThemeMode::Dark (default)
    switch (variant) {
        case UiVariant::Primary:
            return {
                Color::Hex(0x3B82F6FF), // Blue-500
                Color::Hex(0x60A5FAFF), // Blue-400
                Color::Hex(0x2563EBFF), // Blue-600
                Color::Hex(0xFFFFFFFF),
                Color::Hex(0x00000000),
                0.0f
            };
        case UiVariant::Success:
            return {
                Color::Hex(0x10B981FF), // Emerald-500
                Color::Hex(0x34D399FF), // Emerald-400
                Color::Hex(0x059669FF), // Emerald-600
                Color::Hex(0xFFFFFFFF),
                Color::Hex(0x00000000),
                0.0f
            };
        case UiVariant::Warning:
            return {
                Color::Hex(0xF59E0BFF), // Amber-500
                Color::Hex(0xFBBF24FF), // Amber-400
                Color::Hex(0xD97706FF), // Amber-600
                Color::Hex(0x0F172AFF), // Slate-900
                Color::Hex(0x00000000),
                0.0f
            };
        case UiVariant::Danger:
            return {
                Color::Hex(0xEF4444FF), // Red-500
                Color::Hex(0xF87171FF), // Red-400
                Color::Hex(0xDC2626FF), // Red-600
                Color::Hex(0xFFFFFFFF),
                Color::Hex(0x00000000),
                0.0f
            };
        case UiVariant::Info:
            return {
                Color::Hex(0x06B6D4FF), // Cyan-500
                Color::Hex(0x22D3EEFF), // Cyan-400
                Color::Hex(0x0891B2FF), // Cyan-600
                Color::Hex(0xFFFFFFFF),
                Color::Hex(0x00000000),
                0.0f
            };
        case UiVariant::Ghost:
            return {
                Color::Hex(0x00000000), // Transparent
                Color::Hex(0xFFFFFF18), // 10% white
                Color::Hex(0xFFFFFF28), // 16% white
                Color::Hex(0xF1F5F9FF), // Slate-100
                Color::Hex(0x00000000),
                0.0f
            };
        case UiVariant::Muted:
            return {
                Color::Hex(0x00000000),
                Color::Hex(0x00000000),
                Color::Hex(0x00000000),
                Color::Hex(0x94A3B8FF), // Slate-400
                Color::Hex(0x00000000),
                0.0f
            };
        case UiVariant::Default:
        case UiVariant::Secondary:
        default:
            return {
                Color::Hex(0x334155FF), // Slate-700
                Color::Hex(0x475569FF), // Slate-600
                Color::Hex(0x1E293BFF), // Slate-800
                Color::Hex(0xF8FAFCFF), // Slate-50
                Color::Hex(0x64748B40), // Slate-500 / 25%
                1.0f
            };
    }
}

} // namespace guikit
