#include "guikit/style/theme.hpp"
#include "guikit/style/style_painter.hpp"
#include "keel/format.h"
#include "keel/math.h"
#include "keel/number.h"

namespace guikit {

namespace {

void PopulateBevelPalette(UiTheme& theme, bool exactWin98 = false) noexcept {
    if (exactWin98 && theme.preset == ThemePreset::Windows98) {
        theme.bevel = BevelPalette{
            .highlightOuter = Color::Hex(0xFFFFFFFF),
            .highlightInner = Color::Hex(0xDFDFDFFF),
            .shadowInner    = Color::Hex(0x808080FF),
            .shadowOuter    = Color::Hex(0x000000FF)
        };
        return;
    }
    const Color base = theme.palette.surface.WithAlpha(255u);
    if (theme.mode == ThemeMode::Dark) {
        theme.bevel = BevelPalette{
            .highlightOuter = base.Lighten(0.28f),
            .highlightInner = base.Lighten(0.13f),
            .shadowInner    = theme.palette.background.Darken(0.22f),
            .shadowOuter    = Color::Hex(0x000000EB)
        };
    } else {
        theme.bevel = BevelPalette{
            .highlightOuter = base.Lighten(0.38f),
            .highlightInner = base.Lighten(0.14f),
            .shadowInner    = base.Darken(0.28f),
            .shadowOuter    = base.Darken(0.58f)
        };
    }
}

void ApplyScrollbarMetricsForChrome(UiTheme& theme, ScrollbarChromeStyle chromeStyle) noexcept {
    if (chromeStyle == ScrollbarChromeStyle::Modern) {
        theme.scrollbars = {{
            ScrollbarMetrics{ 0.0f,  0.0f, 0.0f,  0.0f, 0.0f,  0.0f }, // None
            ScrollbarMetrics{ 3.0f, 12.0f, 1.5f, 18.0f, 1.5f,  6.0f }, // Xs
            ScrollbarMetrics{ 4.5f, 15.0f, 2.25f, 24.0f, 2.25f, 9.0f }, // Sm
            ScrollbarMetrics{ 6.0f, 21.0f, 3.0f, 30.0f, 2.25f, 10.5f }, // Md
            ScrollbarMetrics{ 7.5f, 27.0f, 3.75f, 36.0f, 3.0f, 13.5f }, // Lg
            ScrollbarMetrics{ 9.0f, 33.0f, 4.5f, 42.0f, 3.0f, 15.0f }  // Xl
        }};
        return;
    }

    theme.scrollbars = {{
        ScrollbarMetrics{  0.0f,  0.0f, 0.0f,  0.0f, 0.0f,  0.0f }, // None
        ScrollbarMetrics{ 12.0f, 16.0f, 0.0f, 18.0f, 0.0f, 12.0f }, // Xs
        ScrollbarMetrics{ 14.0f, 18.0f, 0.0f, 24.0f, 0.0f, 14.0f }, // Sm
        ScrollbarMetrics{ 16.0f, 20.0f, 0.0f, 30.0f, 0.0f, 16.0f }, // Md
        ScrollbarMetrics{ 18.0f, 24.0f, 0.0f, 36.0f, 0.0f, 18.0f }, // Lg
        ScrollbarMetrics{ 20.0f, 28.0f, 0.0f, 42.0f, 0.0f, 20.0f }  // Xl
    }};
}

void SyncScrollbar(UiTheme& theme) noexcept {
    theme.scrollbarStyle = ScrollbarStyleTokens::ForChrome(theme.chrome.scrollbar);
    ApplyScrollbarMetricsForChrome(theme, theme.chrome.scrollbar);
}

void ResetDefaultRadii(UiTheme& theme) noexcept {
    theme.radius = {0.0f, 1.5f, 3.0f, 4.5f, 6.0f, 9.0f};
    constexpr keel::StaticArray<keel::f32, 6> kCompR   = {0.0f, 2.25f, 3.0f, 4.5f, 6.0f, 7.5f};
    constexpr keel::StaticArray<keel::f32, 6> kCheckR  = {3.0f, 2.0f, 2.25f, 3.0f, 3.75f, 4.5f};
    constexpr keel::StaticArray<keel::f32, 6> kInputR  = {4.5f, 3.0f, 3.75f, 4.5f, 6.0f, 7.5f};
    constexpr keel::StaticArray<keel::f32, 6> kSliderR = {6.75f, 4.5f, 5.25f, 6.75f, 8.25f, 9.75f};
    for (keel::usize i = 0u; i < 6u; ++i) {
        theme.metrics[i].cornerRadius    = kCompR[i];
        theme.checkboxes[i].cornerRadius = kCheckR[i];
        theme.inputs[i].cornerRadius     = kInputR[i];
        theme.selects[i].cornerRadius    = kInputR[i];
        theme.sliders[i].cornerRadius    = kSliderR[i];
    }
    if (theme.shellStyle == ShellStyle::Floating) {
        theme.shell.panelRadius = theme.GetRadius(UiSize::Md);
    }
}

void PopulateChromeTokens(UiTheme& theme, ChromeStyle style) noexcept {
    theme.chromeStyle = style;
    theme.chrome = ChromeTokens::FromStyle(style);
    const bool zeroRadius = (theme.GetRadius(UiSize::Md) <= 0.01f);
    if (style == ChromeStyle::Modern && zeroRadius) {
        theme.chrome.badgeShape = BadgeShape::Sharp;
        theme.chrome.sliderThumb = SliderThumbShape::Square;
    }
    SyncScrollbar(theme);
}

[[nodiscard]] VariantColors MakeSemanticVariant(Color base, ThemeMode mode, keel::f32 borderWidth = 0.0f) noexcept {
    const Color hover = (mode == ThemeMode::Dark) ? base.Lighten(0.08f) : base.Lighten(0.07f);
    const Color active = (mode == ThemeMode::Dark) ? base.Darken(0.08f) : base.Darken(0.09f);
    const Color ink = Color::BestTextColor(base, Color::Hex(0x0F172AFF), Color::Hex(0xFFFFFFFF));
    return VariantColors{base, hover, active, ink, Color::Hex(0x00000000), borderWidth};
}

void PopulateShadows(UiTheme& theme, ThemeMode mode, keel::f32 intensity = 1.0f) noexcept {
    const keel::f32 s = keel::clamp(intensity, 0.0f, 2.5f);
    theme.shadowsEnabled = (s > 0.001f);
    auto alpha = [s](keel::u8 baseAlpha) noexcept -> keel::u8 {
        return keel::as<keel::u8>(keel::clamp(keel::round_i32(keel::as<keel::f32>(baseAlpha) * s), 0, 255));
    };
    if (mode == ThemeMode::Light) {
        theme.shadows = {{
            ShadowStyle{0.0f,  {0.0f, 0.0f}, Color(0, 0, 0, 0)},
            ShadowStyle{10.0f, {0.0f, 3.0f}, Color(15, 23, 42, alpha(38))},
            ShadowStyle{14.0f, {0.0f, 4.0f}, Color(15, 23, 42, alpha(54))},
            ShadowStyle{20.0f, {0.0f, 7.0f}, Color(15, 23, 42, alpha(72))}
        }};
    } else {
        theme.shadows = {{
            ShadowStyle{0.0f,  {0.0f, 0.0f}, Color(0, 0, 0, 0)},
            ShadowStyle{12.0f, {0.0f, 4.0f}, Color(0, 0, 0, alpha(96))},
            ShadowStyle{16.0f, {0.0f, 5.0f}, Color(0, 0, 0, alpha(112))},
            ShadowStyle{22.0f, {0.0f, 8.0f}, Color(0, 0, 0, alpha(148))}
        }};
    }
}

void PopulatePalette(UiTheme& theme, ThemeMode mode) noexcept {
    if (mode == ThemeMode::Light) {
        theme.palette = {
            .background        = Color::Hex(0xF1F5F9FF), // Slate-100
            .surface           = Color::Hex(0xFFFFFFFF), // Pure white
            .surfaceHover      = Color::Hex(0xF1F5F9FF), // Slate-100
            .surfaceActive     = Color::Hex(0xE2E8F0FF), // Slate-200
            .surfaceElevated   = Color::Hex(0xFFFFFFFF), // Pure white
            .border            = Color::Hex(0xE2E8F0FF), // Slate-200
            .borderHover       = Color::Hex(0x94A3B8FF), // Slate-400
            .borderSubtle      = Color::Hex(0xE2E8F0FF), // Slate-200
            .focusRing         = Color::Hex(0x0369A1FF), // Sky-700 (WCAG AA 5.9:1 on white)
            .textPrimary       = Color::Hex(0x0F172AFF), // Slate-900
            .textSecondary     = Color::Hex(0x64748BFF), // Slate-500
            .textMuted         = Color::Hex(0x94A3B8FF), // Slate-400
            .textDisabled      = Color::Hex(0x94A3B880), // Slate-400 / 50%
            .inputBackground   = Color::Hex(0xF8FAFCFF), // Slate-50
            .inputBorder       = Color::Hex(0xCBD5E1FF), // Slate-300
            .caret             = Color::Hex(0x0369A1FF), // Sky-700
            .controlTrack      = Color::Hex(0xE2E8F0FF), // Slate-200
            .controlTrackHover = Color::Hex(0xCBD5E1FF), // Slate-300
            .controlKnob       = Color::Hex(0xF8FAFCFF), // Slate-50
            .controlKnobHover  = Color::Hex(0xFFFFFFFF), // White
            .controlKnobActive = Color::Hex(0xFFFFFFFF), // White
            .accent            = Color::Hex(0x0369A1FF), // Sky-700 (WCAG AA 5.9:1 with white text)
            .accentHover       = Color::Hex(0x075985FF), // Sky-800
            .accentText        = Color::Hex(0xFFFFFFFF)  // White
        };
    } else {
        theme.palette = {
            .background        = Color::Hex(0x0B0F19FF), // Slate-950
            .surface           = Color::Hex(0x1E293BFF), // Slate-800
            .surfaceHover      = Color::Hex(0x33415580), // Slate-700 / 50%
            .surfaceActive     = Color::Hex(0x334155FF), // Slate-700
            .surfaceElevated   = Color::Hex(0x1E293BFF), // Slate-800
            .border            = Color::Hex(0x334155FF), // Slate-700
            .borderHover       = Color::Hex(0x64748BFF), // Slate-500
            .borderSubtle      = Color::Hex(0x33415580), // Slate-700 / 50%
            .focusRing         = Color::Hex(0x38BDF8FF), // Sky-400
            .textPrimary       = Color::Hex(0xF8FAFCFF), // Slate-50
            .textSecondary     = Color::Hex(0x94A3B8FF), // Slate-400
            .textMuted         = Color::Hex(0x64748BFF), // Slate-500
            .textDisabled      = Color::Hex(0x64748B80), // Slate-500 / 50%
            .inputBackground   = Color::Hex(0x0F172AFF), // Slate-900
            .inputBorder       = Color::Hex(0x334155FF), // Slate-700
            .caret             = Color::Hex(0x38BDF8FF), // Sky-400
            .controlTrack      = Color::Hex(0x1E293BFF), // Slate-800
            .controlTrackHover = Color::Hex(0x334155FF), // Slate-700
            .controlKnob       = Color::Hex(0x94A3B8FF), // Slate-400
            .controlKnobHover  = Color::Hex(0xE2E8F0FF), // Slate-200
            .controlKnobActive = Color::Hex(0xFFFFFFFF), // White
            .accent            = Color::Hex(0x38BDF8FF), // Sky-400
            .accentHover       = Color::Hex(0x7DD3FCFF), // Sky-300
            .accentText        = Color::Hex(0x0F172AFF)  // Slate-900
        };
    }
}

void PopulateSyntaxPalette(UiTheme& theme, ThemeMode mode) noexcept {
    if (mode == ThemeMode::Light) {
        theme.syntax = {
            .keyword       = Color::Hex(0x7E22CEFF), // Purple-700
            .type          = Color::Hex(0x0284C7FF), // Sky-600
            .function      = Color::Hex(0x2563EBFF), // Blue-600
            .string        = Color::Hex(0x059669FF), // Emerald-600
            .number        = Color::Hex(0xD97706FF), // Amber-600
            .comment       = Color::Hex(0x64748BFF), // Slate-500
            .preprocessor  = Color::Hex(0xDB2777FF), // Pink-600
            .punctuation   = Color::Hex(0x475569FF), // Slate-600
            .identifier    = Color::Hex(0x0F172AFF), // Slate-900
            .lineHighlight = Color::Hex(0xE2E8F070), // Subtle Slate-200
            .selectionBg   = Color::Hex(0x0284C733)  // Sky-600 / 20%
        };
    } else {
        theme.syntax = {
            .keyword       = Color::Hex(0xC084FCFF), // Purple-400
            .type          = Color::Hex(0x38BDF8FF), // Sky-400
            .function      = Color::Hex(0x60A5FAFF), // Blue-400
            .string        = Color::Hex(0x34D399FF), // Emerald-400
            .number        = Color::Hex(0xFBBF24FF), // Amber-400
            .comment       = Color::Hex(0x64748BFF), // Slate-500
            .preprocessor  = Color::Hex(0xF472B6FF), // Pink-400
            .punctuation   = Color::Hex(0x94A3B8FF), // Slate-400
            .identifier    = Color::Hex(0xF8FAFCFF), // Slate-50
            .lineHighlight = Color::Hex(0x1E293B90), // Subtle Slate-800
            .selectionBg   = Color::Hex(0x38BDF840)  // Sky-400 / 25%
        };
    }
}

void PopulateVariants(UiTheme& theme, ThemeMode mode) noexcept {
    if (mode == ThemeMode::Light) {
        theme.variants = {{
            VariantColors{ Color::Hex(0xE2E8F0FF), Color::Hex(0xCBD5E1FF), Color::Hex(0x94A3B8FF), Color::Hex(0x0F172AFF), Color::Hex(0xCBD5E1FF), 1.0f },
            VariantColors{ Color::Hex(0x2563EBFF), Color::Hex(0x3B82F6FF), Color::Hex(0x1D4ED8FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            VariantColors{ Color::Hex(0xE2E8F0FF), Color::Hex(0xCBD5E1FF), Color::Hex(0x94A3B8FF), Color::Hex(0x0F172AFF), Color::Hex(0xCBD5E1FF), 1.0f },
            VariantColors{ Color::Hex(0x059669FF), Color::Hex(0x10B981FF), Color::Hex(0x047857FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            VariantColors{ Color::Hex(0xD97706FF), Color::Hex(0xF59E0BFF), Color::Hex(0xB45309FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            VariantColors{ Color::Hex(0xDC2626FF), Color::Hex(0xEF4444FF), Color::Hex(0xB91C1CFF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            VariantColors{ Color::Hex(0x0891B2FF), Color::Hex(0x06B6D4FF), Color::Hex(0x0E7490FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            VariantColors{ Color::Hex(0x00000000), Color::Hex(0x0F172A0D), Color::Hex(0x0F172A1A), Color::Hex(0x334155FF), Color::Hex(0x00000000), 0.0f },
            VariantColors{ Color::Hex(0x00000000), Color::Hex(0x00000000), Color::Hex(0x00000000), Color::Hex(0x64748BFF), Color::Hex(0x00000000), 0.0f }
        }};
    } else {
        theme.variants = {{
            VariantColors{ Color::Hex(0x334155FF), Color::Hex(0x475569FF), Color::Hex(0x1E293BFF), Color::Hex(0xF8FAFCFF), Color::Hex(0x64748B40), 1.0f },
            VariantColors{ Color::Hex(0x3B82F6FF), Color::Hex(0x60A5FAFF), Color::Hex(0x2563EBFF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            VariantColors{ Color::Hex(0x334155FF), Color::Hex(0x475569FF), Color::Hex(0x1E293BFF), Color::Hex(0xF8FAFCFF), Color::Hex(0x64748B40), 1.0f },
            VariantColors{ Color::Hex(0x10B981FF), Color::Hex(0x34D399FF), Color::Hex(0x059669FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            VariantColors{ Color::Hex(0xF59E0BFF), Color::Hex(0xFBBF24FF), Color::Hex(0xD97706FF), Color::Hex(0x0F172AFF), Color::Hex(0x00000000), 0.0f },
            VariantColors{ Color::Hex(0xEF4444FF), Color::Hex(0xF87171FF), Color::Hex(0xDC2626FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            VariantColors{ Color::Hex(0x06B6D4FF), Color::Hex(0x22D3EEFF), Color::Hex(0x0891B2FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            VariantColors{ Color::Hex(0x00000000), Color::Hex(0xFFFFFF18), Color::Hex(0xFFFFFF28), Color::Hex(0xF1F5F9FF), Color::Hex(0x00000000), 0.0f },
            VariantColors{ Color::Hex(0x00000000), Color::Hex(0x00000000), Color::Hex(0x00000000), Color::Hex(0x94A3B8FF), Color::Hex(0x00000000), 0.0f }
        }};
    }
}

void PopulateTypography(UiTheme& theme) noexcept {
    theme.typography = {{
        TypographyMetrics{ 40.0f, 48.0f, false }, // DisplayLarge
        TypographyMetrics{ 32.0f, 40.0f, false }, // DisplayMedium
        TypographyMetrics{ 28.0f, 36.0f, false }, // DisplaySmall
        TypographyMetrics{ 24.0f, 32.0f, false }, // HeadlineLarge
        TypographyMetrics{ 22.0f, 28.0f, false }, // HeadlineMedium
        TypographyMetrics{ 20.0f, 26.0f, false }, // HeadlineSmall
        TypographyMetrics{ 18.0f, 24.0f, false }, // TitleLarge
        TypographyMetrics{ 16.0f, 22.0f, false }, // TitleMedium
        TypographyMetrics{ 15.0f, 20.0f, false }, // TitleSmall
        TypographyMetrics{ 16.0f, 24.0f, false }, // BodyLarge
        TypographyMetrics{ 14.0f, 20.0f, false }, // BodyMedium
        TypographyMetrics{ 13.0f, 18.0f, false }, // BodySmall
        TypographyMetrics{ 14.0f, 18.0f, false }, // LabelLarge
        TypographyMetrics{ 12.0f, 16.0f, false }, // LabelMedium
        TypographyMetrics{ 11.0f, 14.0f, false }, // LabelSmall
        TypographyMetrics{ 15.0f, 20.0f, true  }, // CodeLarge
        TypographyMetrics{ 13.0f, 18.0f, true  }, // CodeMedium
        TypographyMetrics{ 11.0f, 15.0f, true  }  // CodeSmall
    }};
}

void PopulateScrollbars(UiTheme& theme) noexcept {
    theme.scrollbars = {{
        ScrollbarMetrics{ 0.0f,  0.0f, 0.0f,  0.0f, 0.0f,  0.0f }, // None
        ScrollbarMetrics{ 3.0f, 12.0f, 1.5f, 18.0f, 1.5f,  6.0f }, // Xs
        ScrollbarMetrics{ 4.5f, 15.0f, 2.25f, 24.0f, 2.25f, 9.0f }, // Sm
        ScrollbarMetrics{ 6.0f, 21.0f, 3.0f, 30.0f, 2.25f, 10.5f }, // Md
        ScrollbarMetrics{ 7.5f, 27.0f, 3.75f, 36.0f, 3.0f, 13.5f }, // Lg
        ScrollbarMetrics{ 9.0f, 33.0f, 4.5f, 42.0f, 3.0f, 15.0f }  // Xl
    }};
}

void PopulateCarousels(UiTheme& theme) noexcept {
    theme.carousels = {{
        CarouselMetrics{ 19.5f, 10.5f,  4.5f, 3.0f, 4.5f, 27.0f, 82.5f }, // None
        CarouselMetrics{ 13.5f, 7.5f,  2.25f, 1.5f, 3.0f, 18.0f,  45.0f }, // Xs
        CarouselMetrics{ 16.5f, 9.0f,  3.0f, 2.25f, 3.75f, 22.5f,  60.0f }, // Sm
        CarouselMetrics{ 19.5f, 10.5f,  4.5f, 3.0f, 4.5f, 27.0f, 82.5f }, // Md
        CarouselMetrics{ 24.0f, 12.0f,  6.0f, 3.75f, 6.0f, 33.75f, 105.0f }, // Lg
        CarouselMetrics{ 30.0f, 15.0f, 7.5f, 4.5f, 7.5f, 42.0f, 135.0f }  // Xl
    }};
}

void PopulateComponentMetrics(UiTheme& theme) noexcept {
    theme.switches = {{
        SwitchMetrics{ {27.0f, 15.0f}, 12.0f, 1.5f, 7.5f,  4.5f,  4.5f }, // None
        SwitchMetrics{ {18.0f, 10.5f}, 7.5f, 1.5f,  4.5f,  3.0f,  3.0f }, // Xs
        SwitchMetrics{ {21.0f, 12.0f}, 9.0f, 1.5f,  6.0f,  3.75f,  3.75f }, // Sm
        SwitchMetrics{ {27.0f, 15.0f}, 12.0f, 1.5f, 7.5f,  4.5f,  4.5f }, // Md
        SwitchMetrics{ {33.0f, 18.0f}, 15.0f, 1.5f, 9.0f,  6.0f,  6.0f }, // Lg
        SwitchMetrics{ {39.0f, 21.0f}, 18.0f, 1.5f, 10.5f, 7.5f, 7.5f }  // Xl
    }};

    theme.checkboxes = {{
        CheckboxMetrics{ 13.5f, 3.0f, 7.5f,  4.5f,  4.5f, 1.5f }, // None
        CheckboxMetrics{ 9.0f, 2.0f,  4.5f,  3.0f,  3.0f, 1.25f }, // Xs
        CheckboxMetrics{ 10.5f, 2.25f,  6.0f,  3.75f,  3.75f, 1.25f }, // Sm
        CheckboxMetrics{ 13.5f, 3.0f, 7.5f,  4.5f,  4.5f, 1.5f }, // Md
        CheckboxMetrics{ 16.5f, 3.75f, 9.0f,  6.0f,  6.0f, 1.75f }, // Lg
        CheckboxMetrics{ 19.5f, 4.5f, 10.5f, 7.5f, 7.5f, 2.0f }  // Xl
    }};

    theme.inputs = {{
        InputFieldMetrics{ 27.0f,  4.5f, 9.0f,  4.5f,  4.5f, 1.0f, 12.0f }, // None
        InputFieldMetrics{ 18.0f,  3.0f,  6.0f,  3.0f,  3.0f, 1.0f, 9.0f }, // Xs
        InputFieldMetrics{ 21.0f,  3.75f, 7.5f,  3.75f,  3.75f, 1.0f, 10.5f }, // Sm
        InputFieldMetrics{ 27.0f,  4.5f, 9.0f,  4.5f,  4.5f, 1.0f, 12.0f }, // Md
        InputFieldMetrics{ 33.0f,  6.0f, 12.0f,  6.0f,  6.0f, 1.5f, 13.5f }, // Lg
        InputFieldMetrics{ 39.0f,  7.5f, 15.0f,  7.5f,  7.5f, 1.5f, 16.5f }  // Xl
    }};

    theme.selects = {{
        SelectMetrics{ 27.0f,  4.5f, 9.0f,  4.5f,  4.5f, 1.0f, 12.0f, 24.0f }, // None
        SelectMetrics{ 18.0f,  3.0f,  6.0f,  3.0f,  3.0f, 1.0f, 9.0f, 16.5f }, // Xs
        SelectMetrics{ 21.0f,  3.75f, 7.5f,  3.75f,  3.75f, 1.0f, 10.5f, 19.5f }, // Sm
        SelectMetrics{ 27.0f,  4.5f, 9.0f,  4.5f,  4.5f, 1.0f, 12.0f, 24.0f }, // Md
        SelectMetrics{ 33.0f,  6.0f, 12.0f,  6.0f,  6.0f, 1.5f, 13.5f, 28.5f }, // Lg
        SelectMetrics{ 39.0f,  7.5f, 15.0f,  7.5f,  7.5f, 1.5f, 16.5f, 33.0f }  // Xl
    }};

    theme.headings = {{
        HeadingMetrics{ 24.0f, 16.0f, 13.0f, 18.0f, 7.5f, 7.5f, 3.75f }, // H1
        HeadingMetrics{ 20.0f, 14.0f, 12.0f, 15.0f,  6.0f,  6.75f, 3.0f }, // H2
        HeadingMetrics{ 18.0f, 13.5f, 11.5f, 13.5f,  5.25f,  6.0f, 2.75f }, // H3
        HeadingMetrics{ 16.0f, 13.0f, 11.0f, 12.0f,  4.5f,  6.0f, 2.25f }, // H4
        HeadingMetrics{ 14.0f, 11.5f, 10.0f, 10.5f,  3.75f,  4.5f, 2.0f }, // H5
        HeadingMetrics{ 12.0f, 10.5f,  9.0f, 9.0f,  3.0f,  4.5f, 1.5f }  // H6
    }};

    theme.sliders = {{
        SliderMetrics{ 19.5f, 3.75f, 13.5f, 6.75f, 3.0f,  4.5f, 3.75f,  4.5f }, // None
        SliderMetrics{ 13.5f, 2.25f, 9.0f, 4.5f, 2.25f,  3.0f, 2.25f,  3.0f }, // Xs
        SliderMetrics{ 16.5f, 3.0f, 10.5f, 5.25f, 2.75f,  3.75f, 3.0f,  3.75f }, // Sm
        SliderMetrics{ 19.5f, 3.75f, 13.5f, 6.75f, 3.0f,  4.5f, 3.75f,  4.5f }, // Md
        SliderMetrics{ 24.0f, 4.5f, 16.5f, 8.25f, 3.75f, 6.0f, 4.5f,  6.0f }, // Lg
        SliderMetrics{ 28.5f, 6.0f, 19.5f, 9.75f, 4.5f, 7.5f, 6.0f, 7.5f }  // Xl
    }};
}

void PopulateBaseMetrics(UiTheme& theme) noexcept {
    theme.spacing = {0.0f, 3.0f, 6.0f, 9.0f, 12.0f, 18.0f};
    theme.radius  = {0.0f, 1.5f, 3.0f, 4.5f, 6.0f, 9.0f};
    theme.metrics = {{
        ComponentMetrics{  0.0f, 14.0f, 12.0f, 11.0f,  0.0f,  0.0f,  0.0f,  0.0f, 48.0f, 60.0f, 11.0f, 4.5f,  6.0f, 2.25f }, // None
        ComponentMetrics{ 18.0f, 11.0f, 10.0f,  9.0f,  6.0f,  2.25f, 2.25f, 9.0f, 48.0f, 60.0f,  9.0f, 3.0f,  4.5f, 1.5f }, // Xs
        ComponentMetrics{ 24.0f, 13.0f, 11.0f, 10.0f,  9.0f,  3.75f, 3.0f, 11.25f, 48.0f, 60.0f, 10.0f, 3.75f, 4.5f, 2.0f }, // Sm
        ComponentMetrics{ 30.0f, 14.0f, 12.0f, 11.0f, 12.0f,  6.0f,  4.5f, 13.5f, 48.0f, 60.0f, 11.0f, 4.5f,  6.0f, 2.25f }, // Md
        ComponentMetrics{ 36.0f, 16.0f, 14.0f, 12.0f, 15.0f,  7.5f,  6.0f, 16.5f, 48.0f, 60.0f, 12.0f, 6.0f,  6.0f, 2.75f }, // Lg
        ComponentMetrics{ 42.0f, 18.0f, 16.0f, 14.0f, 18.0f,  9.0f,  7.5f, 19.5f, 48.0f, 60.0f, 13.0f, 6.0f,  7.5f, 3.0f }  // Xl
    }};
    theme.SetShellStyle(theme.shellStyle);
}

void PopulateAllMetrics(UiTheme& theme) noexcept {
    theme.focusRingWidth = 2.0f;
    theme.disabledOpacity = 0.50f;
    theme.borderScale = 1.0f;
    theme.animationScale = 1.0f;
    PopulateTypography(theme);
    PopulateScrollbars(theme);
    PopulateCarousels(theme);
    PopulateComponentMetrics(theme);
    PopulateBaseMetrics(theme);
}

void ApplyCuratedPresetColors(UiTheme& theme, ThemePreset p) noexcept {
    theme.preset = p;
    theme.chromeStyle = ChromeStyle::Modern;
    theme.fillStyle = FillStyle::Solid;
    switch (p) {
        case ThemePreset::Dark:
            theme.mode = ThemeMode::Dark;
            PopulatePalette(theme, ThemeMode::Dark);
            PopulateSyntaxPalette(theme, ThemeMode::Dark);
            PopulateVariants(theme, ThemeMode::Dark);
            PopulateShadows(theme, ThemeMode::Dark, 1.0f);
            break;

        case ThemePreset::Light:
            theme.mode = ThemeMode::Light;
            PopulatePalette(theme, ThemeMode::Light);
            PopulateSyntaxPalette(theme, ThemeMode::Light);
            PopulateVariants(theme, ThemeMode::Light);
            PopulateShadows(theme, ThemeMode::Light, 1.0f);
            break;

        case ThemePreset::Nord:
            theme.mode = ThemeMode::Dark;
            theme.palette = {
                .background        = Color::Hex(0x242933FF), // Polar Night deep
                .surface           = Color::Hex(0x2E3440FF), // nord0
                .surfaceHover      = Color::Hex(0x3B4252CC), // nord1
                .surfaceActive     = Color::Hex(0x434C5EFF), // nord2
                .surfaceElevated   = Color::Hex(0x3B4252FF), // nord1
                .border            = Color::Hex(0x4C566AFF), // nord3
                .borderHover       = Color::Hex(0x616E88FF),
                .borderSubtle      = Color::Hex(0x434C5E99),
                .focusRing         = Color::Hex(0x88C0D0FF), // nord8 Frost
                .textPrimary       = Color::Hex(0xECEFF4FF), // nord6
                .textSecondary     = Color::Hex(0xD8DEE9FF), // nord4
                .textMuted         = Color::Hex(0x8F9BB0FF),
                .textDisabled      = Color::Hex(0x8F9BB080),
                .inputBackground   = Color::Hex(0x242933FF),
                .inputBorder       = Color::Hex(0x4C566AFF),
                .caret             = Color::Hex(0x88C0D0FF),
                .controlTrack      = Color::Hex(0x3B4252FF),
                .controlTrackHover = Color::Hex(0x4C566AFF),
                .controlKnob       = Color::Hex(0xD8DEE9FF),
                .controlKnobHover  = Color::Hex(0xECEFF4FF),
                .controlKnobActive = Color::Hex(0xECEFF4FF),
                .accent            = Color::Hex(0x88C0D0FF),
                .accentHover       = Color::Hex(0x8FBCBBFF),
                .accentText        = Color::Hex(0x242933FF)
            };
            PopulateVariants(theme, ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Default)]   = { Color::Hex(0x3B4252FF), Color::Hex(0x434C5EFF), Color::Hex(0x4C566AFF), Color::Hex(0xECEFF4FF), Color::Hex(0x4C566AFF), 1.0f };
            theme.variants[keel::as<keel::usize>(UiVariant::Secondary)] = theme.variants[keel::as<keel::usize>(UiVariant::Default)];
            theme.variants[keel::as<keel::usize>(UiVariant::Primary)]   = MakeSemanticVariant(Color::Hex(0x5E81ACFF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Success)]   = MakeSemanticVariant(Color::Hex(0xA3BE8CFF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Warning)]   = MakeSemanticVariant(Color::Hex(0xEBCB8BFF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Danger)]    = MakeSemanticVariant(Color::Hex(0xBF616AFF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Info)]      = MakeSemanticVariant(Color::Hex(0x88C0D0FF), ThemeMode::Dark);
            theme.syntax = SyntaxPalette::OneDark();
            theme.syntax.editorBg = Color::Hex(0x242933FF);
            theme.syntax.gutterBg = Color::Hex(0x1E222BFF);
            PopulateShadows(theme, ThemeMode::Dark, 1.0f);
            break;

        case ThemePreset::Dracula:
            theme.mode = ThemeMode::Dark;
            theme.palette = {
                .background        = Color::Hex(0x1E1F29FF),
                .surface           = Color::Hex(0x282A36FF),
                .surfaceHover      = Color::Hex(0x343746CC),
                .surfaceActive     = Color::Hex(0x44475AFF),
                .surfaceElevated   = Color::Hex(0x303342FF),
                .border            = Color::Hex(0x44475AFF),
                .borderHover       = Color::Hex(0x6272A4FF),
                .borderSubtle      = Color::Hex(0x44475A99),
                .focusRing         = Color::Hex(0xBD93F9FF),
                .textPrimary       = Color::Hex(0xF8F8F2FF),
                .textSecondary     = Color::Hex(0xB6C2E2FF),
                .textMuted         = Color::Hex(0x6272A4FF),
                .textDisabled      = Color::Hex(0x6272A480),
                .inputBackground   = Color::Hex(0x21222CFF),
                .inputBorder       = Color::Hex(0x44475AFF),
                .caret             = Color::Hex(0xFF79C6FF),
                .controlTrack      = Color::Hex(0x383A4BFF),
                .controlTrackHover = Color::Hex(0x44475AFF),
                .controlKnob       = Color::Hex(0xB6C2E2FF),
                .controlKnobHover  = Color::Hex(0xF8F8F2FF),
                .controlKnobActive = Color::Hex(0xF8F8F2FF),
                .accent            = Color::Hex(0xBD93F9FF),
                .accentHover       = Color::Hex(0xD0B0FFFF),
                .accentText        = Color::Hex(0x21222CFF)
            };
            PopulateVariants(theme, ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Default)]   = { Color::Hex(0x383A4BFF), Color::Hex(0x44475AFF), Color::Hex(0x282A36FF), Color::Hex(0xF8F8F2FF), Color::Hex(0x6272A466), 1.0f };
            theme.variants[keel::as<keel::usize>(UiVariant::Secondary)] = theme.variants[keel::as<keel::usize>(UiVariant::Default)];
            theme.variants[keel::as<keel::usize>(UiVariant::Primary)]   = MakeSemanticVariant(Color::Hex(0xBD93F9FF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Success)]   = MakeSemanticVariant(Color::Hex(0x50FA7BFF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Warning)]   = MakeSemanticVariant(Color::Hex(0xFFB86CFF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Danger)]    = MakeSemanticVariant(Color::Hex(0xFF5555FF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Info)]      = MakeSemanticVariant(Color::Hex(0x8BE9FDFF), ThemeMode::Dark);
            theme.syntax = SyntaxPalette::Dracula();
            PopulateShadows(theme, ThemeMode::Dark, 1.1f);
            break;

        case ThemePreset::CatppuccinMocha:
            theme.mode = ThemeMode::Dark;
            theme.palette = {
                .background        = Color::Hex(0x181825FF), // Mantle
                .surface           = Color::Hex(0x1E1E2EFF), // Base
                .surfaceHover      = Color::Hex(0x313244CC), // Surface0
                .surfaceActive     = Color::Hex(0x45475AFF), // Surface1
                .surfaceElevated   = Color::Hex(0x262637FF),
                .border            = Color::Hex(0x313244FF), // Surface0
                .borderHover       = Color::Hex(0x585B70FF), // Surface2
                .borderSubtle      = Color::Hex(0x31324499),
                .focusRing         = Color::Hex(0xCBA6F7FF), // Mauve
                .textPrimary       = Color::Hex(0xCDD6F4FF), // Text
                .textSecondary     = Color::Hex(0xA6ADC8FF), // Subtext0
                .textMuted         = Color::Hex(0x6C7086FF), // Overlay0
                .textDisabled      = Color::Hex(0x6C708680),
                .inputBackground   = Color::Hex(0x11111BFF), // Crust
                .inputBorder       = Color::Hex(0x45475AFF), // Surface1
                .caret             = Color::Hex(0xF5E0DCFF), // Rosewater
                .controlTrack      = Color::Hex(0x313244FF),
                .controlTrackHover = Color::Hex(0x45475AFF),
                .controlKnob       = Color::Hex(0xBAC2DEFF),
                .controlKnobHover  = Color::Hex(0xCDD6F4FF),
                .controlKnobActive = Color::Hex(0xFFFFFFFF),
                .accent            = Color::Hex(0xCBA6F7FF), // Mauve
                .accentHover       = Color::Hex(0xDFC3FFFF),
                .accentText        = Color::Hex(0x11111BFF)
            };
            PopulateVariants(theme, ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Default)]   = { Color::Hex(0x313244FF), Color::Hex(0x45475AFF), Color::Hex(0x1E1E2EFF), Color::Hex(0xCDD6F4FF), Color::Hex(0x585B7066), 1.0f };
            theme.variants[keel::as<keel::usize>(UiVariant::Secondary)] = theme.variants[keel::as<keel::usize>(UiVariant::Default)];
            theme.variants[keel::as<keel::usize>(UiVariant::Primary)]   = MakeSemanticVariant(Color::Hex(0x89B4FAFF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Success)]   = MakeSemanticVariant(Color::Hex(0xA6E3A1FF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Warning)]   = MakeSemanticVariant(Color::Hex(0xF9E2AFFF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Danger)]    = MakeSemanticVariant(Color::Hex(0xF38BA8FF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Info)]      = MakeSemanticVariant(Color::Hex(0x89DCEBFF), ThemeMode::Dark);
            PopulateSyntaxPalette(theme, ThemeMode::Dark);
            theme.syntax.keyword  = Color::Hex(0xCBA6F7FF);
            theme.syntax.type     = Color::Hex(0xF9E2AFFF);
            theme.syntax.function = Color::Hex(0x89B4FAFF);
            theme.syntax.string   = Color::Hex(0xA6E3A1FF);
            theme.syntax.number   = Color::Hex(0xFAB387FF);
            theme.syntax.comment  = Color::Hex(0x6C7086FF);
            theme.syntax.editorBg = Color::Hex(0x1E1E2EFF);
            theme.syntax.gutterBg = Color::Hex(0x181825FF);
            PopulateShadows(theme, ThemeMode::Dark, 1.0f);
            break;

        case ThemePreset::CatppuccinLatte:
            theme.mode = ThemeMode::Light;
            theme.palette = {
                .background        = Color::Hex(0xEFF1F5FF), // Base
                .surface           = Color::Hex(0xFFFFFFFF),
                .surfaceHover      = Color::Hex(0xE6E9EFFF), // Mantle
                .surfaceActive     = Color::Hex(0xDCE0E8FF), // Crust
                .surfaceElevated   = Color::Hex(0xFFFFFFFF),
                .border            = Color::Hex(0xCCD0DAFF), // Surface0
                .borderHover       = Color::Hex(0x9CA0B0FF), // Overlay0
                .borderSubtle      = Color::Hex(0xDCE0E8FF),
                .focusRing         = Color::Hex(0x8839EFFF), // Mauve
                .textPrimary       = Color::Hex(0x4C4F69FF), // Text
                .textSecondary     = Color::Hex(0x6C6F85FF), // Subtext0
                .textMuted         = Color::Hex(0x8C8FA1FF), // Overlay1
                .textDisabled      = Color::Hex(0x8C8FA180),
                .inputBackground   = Color::Hex(0xF6F7FAFF),
                .inputBorder       = Color::Hex(0xBCC0CCFF), // Surface1
                .caret             = Color::Hex(0x8839EFFF),
                .controlTrack      = Color::Hex(0xDCE0E8FF),
                .controlTrackHover = Color::Hex(0xCCD0DAFF),
                .controlKnob       = Color::Hex(0xFFFFFFFF),
                .controlKnobHover  = Color::Hex(0xFFFFFFFF),
                .controlKnobActive = Color::Hex(0xFFFFFFFF),
                .accent            = Color::Hex(0x8839EFFF), // Mauve
                .accentHover       = Color::Hex(0x7228D6FF),
                .accentText        = Color::Hex(0xFFFFFFFF)
            };
            PopulateVariants(theme, ThemeMode::Light);
            theme.variants[keel::as<keel::usize>(UiVariant::Default)]   = { Color::Hex(0xE6E9EFFF), Color::Hex(0xDCE0E8FF), Color::Hex(0xCCD0DAFF), Color::Hex(0x4C4F69FF), Color::Hex(0xCCD0DAFF), 1.0f };
            theme.variants[keel::as<keel::usize>(UiVariant::Secondary)] = theme.variants[keel::as<keel::usize>(UiVariant::Default)];
            theme.variants[keel::as<keel::usize>(UiVariant::Primary)]   = MakeSemanticVariant(Color::Hex(0x1E66F5FF), ThemeMode::Light);
            theme.variants[keel::as<keel::usize>(UiVariant::Success)]   = MakeSemanticVariant(Color::Hex(0x40A02BFF), ThemeMode::Light);
            theme.variants[keel::as<keel::usize>(UiVariant::Warning)]   = MakeSemanticVariant(Color::Hex(0xDF8E1DFF), ThemeMode::Light);
            theme.variants[keel::as<keel::usize>(UiVariant::Danger)]    = MakeSemanticVariant(Color::Hex(0xD20F39FF), ThemeMode::Light);
            theme.variants[keel::as<keel::usize>(UiVariant::Info)]      = MakeSemanticVariant(Color::Hex(0x04A5E5FF), ThemeMode::Light);
            theme.syntax = SyntaxPalette::GitHubLight();
            PopulateShadows(theme, ThemeMode::Light, 0.9f);
            break;

        case ThemePreset::Industrial:
            theme.mode = ThemeMode::Dark;
            theme.palette = {
                .background        = Color::Hex(0x0B0C0FFF), // Obsidian
                .surface           = Color::Hex(0x15171CFF), // Panel steel
                .surfaceHover      = Color::Hex(0x232730CC),
                .surfaceActive     = Color::Hex(0x2D323EFF),
                .surfaceElevated   = Color::Hex(0x1B1E25FF),
                .border            = Color::Hex(0x2E3340FF),
                .borderHover       = Color::Hex(0xF59E0BFF), // Amber highlight
                .borderSubtle      = Color::Hex(0x232730B0),
                .focusRing         = Color::Hex(0xF59E0BFF), // Amber-500
                .textPrimary       = Color::Hex(0xF3F4F6FF),
                .textSecondary     = Color::Hex(0x9CA3AFFF),
                .textMuted         = Color::Hex(0x6B7280FF),
                .textDisabled      = Color::Hex(0x6B728080),
                .inputBackground   = Color::Hex(0x0E1014FF),
                .inputBorder       = Color::Hex(0x374151FF),
                .caret             = Color::Hex(0xF59E0BFF),
                .controlTrack      = Color::Hex(0x1F242DFF),
                .controlTrackHover = Color::Hex(0x2E3442FF),
                .controlKnob       = Color::Hex(0xD1D5DBFF),
                .controlKnobHover  = Color::Hex(0xF9FAFBFF),
                .controlKnobActive = Color::Hex(0xFFFFFFFF),
                .accent            = Color::Hex(0xF59E0BFF), // Industrial Amber
                .accentHover       = Color::Hex(0xFBBF24FF),
                .accentText        = Color::Hex(0x0B0C0FFF)
            };
            PopulateVariants(theme, ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Default)]   = { Color::Hex(0x232730FF), Color::Hex(0x2F3542FF), Color::Hex(0x15171CFF), Color::Hex(0xF3F4F6FF), Color::Hex(0x4B556380), 1.0f };
            theme.variants[keel::as<keel::usize>(UiVariant::Secondary)] = theme.variants[keel::as<keel::usize>(UiVariant::Default)];
            theme.variants[keel::as<keel::usize>(UiVariant::Primary)]   = MakeSemanticVariant(Color::Hex(0xF59E0BFF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Success)]   = MakeSemanticVariant(Color::Hex(0x22C55EFF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Warning)]   = MakeSemanticVariant(Color::Hex(0xFB923CFF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Danger)]    = MakeSemanticVariant(Color::Hex(0xEF4444FF), ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Info)]      = MakeSemanticVariant(Color::Hex(0x38BDF8FF), ThemeMode::Dark);
            theme.syntax = SyntaxPalette::Monokai();
            theme.syntax.editorBg = Color::Hex(0x0E1014FF);
            theme.syntax.gutterBg = Color::Hex(0x0B0C0FFF);
            PopulateShadows(theme, ThemeMode::Dark, 0.75f);
            break;

        case ThemePreset::IndustrialLight:
            theme.mode = ThemeMode::Light;
            theme.palette = {
                .background        = Color::Hex(0xE6EAEFFF), // Cool machined aluminum
                .surface           = Color::Hex(0xF6F8FAFF), // Crisp instrument panel
                .surfaceHover      = Color::Hex(0xDDE3EAFF),
                .surfaceActive     = Color::Hex(0xCFD7E1FF),
                .surfaceElevated   = Color::Hex(0xFFFFFFFF),
                .border            = Color::Hex(0xB8C2CEFF), // Steel bezel border
                .borderHover       = Color::Hex(0xB45309FF), // Industrial Amber-700 highlight
                .borderSubtle      = Color::Hex(0xCFD7E1FF),
                .focusRing         = Color::Hex(0xB45309FF), // Amber-700
                .textPrimary       = Color::Hex(0x111827FF), // Deep charcoal ink
                .textSecondary     = Color::Hex(0x374151FF),
                .textMuted         = Color::Hex(0x6B7280FF),
                .textDisabled      = Color::Hex(0x6B728080),
                .inputBackground   = Color::Hex(0xFFFFFFFF),
                .inputBorder       = Color::Hex(0x9CA3AFFF),
                .caret             = Color::Hex(0xB45309FF),
                .controlTrack      = Color::Hex(0xCFD7E1FF),
                .controlTrackHover = Color::Hex(0xB8C2CEFF),
                .controlKnob       = Color::Hex(0xFFFFFFFF),
                .controlKnobHover  = Color::Hex(0xFFFFFFFF),
                .controlKnobActive = Color::Hex(0xFFFFFFFF),
                .accent            = Color::Hex(0xB45309FF), // Industrial Amber-700 (WCAG AA 5.2:1 on white)
                .accentHover       = Color::Hex(0x92400EFF), // Amber-800
                .accentText        = Color::Hex(0xFFFFFFFF)
            };
            PopulateVariants(theme, ThemeMode::Light);
            theme.variants[keel::as<keel::usize>(UiVariant::Default)]   = { Color::Hex(0xE2E7EEFF), Color::Hex(0xD3DBE5FF), Color::Hex(0xC2CCD8FF), Color::Hex(0x111827FF), Color::Hex(0x9CA3AFFF), 1.0f };
            theme.variants[keel::as<keel::usize>(UiVariant::Secondary)] = theme.variants[keel::as<keel::usize>(UiVariant::Default)];
            theme.variants[keel::as<keel::usize>(UiVariant::Primary)]   = MakeSemanticVariant(Color::Hex(0xB45309FF), ThemeMode::Light);
            theme.variants[keel::as<keel::usize>(UiVariant::Success)]   = MakeSemanticVariant(Color::Hex(0x15803DFF), ThemeMode::Light);
            theme.variants[keel::as<keel::usize>(UiVariant::Warning)]   = MakeSemanticVariant(Color::Hex(0xD97706FF), ThemeMode::Light);
            theme.variants[keel::as<keel::usize>(UiVariant::Danger)]    = MakeSemanticVariant(Color::Hex(0xDC2626FF), ThemeMode::Light);
            theme.variants[keel::as<keel::usize>(UiVariant::Info)]      = MakeSemanticVariant(Color::Hex(0x0369A1FF), ThemeMode::Light);
            theme.syntax = SyntaxPalette::MatlabClassic();
            theme.syntax.editorBg = Color::Hex(0xF6F8FAFF);
            theme.syntax.gutterBg = Color::Hex(0xE6EAEFFF);
            PopulateShadows(theme, ThemeMode::Light, 0.70f);
            break;

        case ThemePreset::HighContrastDark:
            theme.mode = ThemeMode::Dark;
            theme.palette = {
                .background        = Color::Hex(0x000000FF),
                .surface           = Color::Hex(0x0A0A0EFF),
                .surfaceHover      = Color::Hex(0x1F2937FF),
                .surfaceActive     = Color::Hex(0x374151FF),
                .surfaceElevated   = Color::Hex(0x111118FF),
                .border            = Color::Hex(0xD1D5DBFF),
                .borderHover       = Color::Hex(0xFFFFFFFF),
                .borderSubtle      = Color::Hex(0x9CA3AFFF),
                .focusRing         = Color::Hex(0xFDE047FF), // Vivid Yellow focus ring
                .textPrimary       = Color::Hex(0xFFFFFFFF),
                .textSecondary     = Color::Hex(0xE5E7EBFF),
                .textMuted         = Color::Hex(0xD1D5DBFF),
                .textDisabled      = Color::Hex(0x9CA3AFB0),
                .inputBackground   = Color::Hex(0x000000FF),
                .inputBorder       = Color::Hex(0xFFFFFFFF),
                .caret             = Color::Hex(0xFDE047FF),
                .controlTrack      = Color::Hex(0x1F2937FF),
                .controlTrackHover = Color::Hex(0x374151FF),
                .controlKnob       = Color::Hex(0xFFFFFFFF),
                .controlKnobHover  = Color::Hex(0xFDE047FF),
                .controlKnobActive = Color::Hex(0xFFFFFFFF),
                .accent            = Color::Hex(0x60A5FAFF),
                .accentHover       = Color::Hex(0x93C5FDFF),
                .accentText        = Color::Hex(0x000000FF)
            };
            PopulateVariants(theme, ThemeMode::Dark);
            theme.variants[keel::as<keel::usize>(UiVariant::Default)]   = { Color::Hex(0x111118FF), Color::Hex(0x27272AFF), Color::Hex(0x3F3F46FF), Color::Hex(0xFFFFFFFF), Color::Hex(0xFFFFFFFF), 1.5f };
            theme.variants[keel::as<keel::usize>(UiVariant::Secondary)] = theme.variants[keel::as<keel::usize>(UiVariant::Default)];
            theme.variants[keel::as<keel::usize>(UiVariant::Primary)]   = { Color::Hex(0x60A5FAFF), Color::Hex(0x93C5FDFF), Color::Hex(0x3B82F6FF), Color::Hex(0x000000FF), Color::Hex(0xFFFFFFFF), 1.5f };
            theme.variants[keel::as<keel::usize>(UiVariant::Success)]   = { Color::Hex(0x4ADE80FF), Color::Hex(0x86EFACFF), Color::Hex(0x22C55EFF), Color::Hex(0x000000FF), Color::Hex(0xFFFFFFFF), 1.5f };
            theme.variants[keel::as<keel::usize>(UiVariant::Warning)]   = { Color::Hex(0xFACC15FF), Color::Hex(0xFDE047FF), Color::Hex(0xEAB308FF), Color::Hex(0x000000FF), Color::Hex(0xFFFFFFFF), 1.5f };
            theme.variants[keel::as<keel::usize>(UiVariant::Danger)]    = { Color::Hex(0xF87171FF), Color::Hex(0xFCA5A5FF), Color::Hex(0xEF4444FF), Color::Hex(0x000000FF), Color::Hex(0xFFFFFFFF), 1.5f };
            theme.variants[keel::as<keel::usize>(UiVariant::Info)]      = { Color::Hex(0x38BDF8FF), Color::Hex(0x7DD3FCFF), Color::Hex(0x0EA5E9FF), Color::Hex(0x000000FF), Color::Hex(0xFFFFFFFF), 1.5f };
            PopulateSyntaxPalette(theme, ThemeMode::Dark);
            theme.syntax.editorBg = Color::Hex(0x000000FF);
            theme.syntax.gutterBg = Color::Hex(0x0A0A0EFF);
            PopulateShadows(theme, ThemeMode::Dark, 0.0f);
            theme.focusRingWidth = 3.0f;
            break;

        case ThemePreset::HighContrastLight:
            theme.mode = ThemeMode::Light;
            theme.palette = {
                .background        = Color::Hex(0xFFFFFFFF),
                .surface           = Color::Hex(0xFFFFFFFF),
                .surfaceHover      = Color::Hex(0xE5E7EBFF),
                .surfaceActive     = Color::Hex(0xD1D5DBFF),
                .surfaceElevated   = Color::Hex(0xFFFFFFFF),
                .border            = Color::Hex(0x111827FF),
                .borderHover       = Color::Hex(0x000000FF),
                .borderSubtle      = Color::Hex(0x374151FF),
                .focusRing         = Color::Hex(0x1D4ED8FF),
                .textPrimary       = Color::Hex(0x000000FF),
                .textSecondary     = Color::Hex(0x1F2937FF),
                .textMuted         = Color::Hex(0x374151FF),
                .textDisabled      = Color::Hex(0x4B5563B0),
                .inputBackground   = Color::Hex(0xFFFFFFFF),
                .inputBorder       = Color::Hex(0x000000FF),
                .caret             = Color::Hex(0x000000FF),
                .controlTrack      = Color::Hex(0xD1D5DBFF),
                .controlTrackHover = Color::Hex(0x9CA3AFFF),
                .controlKnob       = Color::Hex(0x000000FF),
                .controlKnobHover  = Color::Hex(0x000000FF),
                .controlKnobActive = Color::Hex(0xFFFFFFFF),
                .accent            = Color::Hex(0x1D4ED8FF),
                .accentHover       = Color::Hex(0x1E40AFFF),
                .accentText        = Color::Hex(0xFFFFFFFF)
            };
            PopulateVariants(theme, ThemeMode::Light);
            theme.variants[keel::as<keel::usize>(UiVariant::Default)]   = { Color::Hex(0xF3F4F6FF), Color::Hex(0xE5E7EBFF), Color::Hex(0xD1D5DBFF), Color::Hex(0x000000FF), Color::Hex(0x000000FF), 1.5f };
            theme.variants[keel::as<keel::usize>(UiVariant::Secondary)] = theme.variants[keel::as<keel::usize>(UiVariant::Default)];
            theme.variants[keel::as<keel::usize>(UiVariant::Primary)]   = { Color::Hex(0x1D4ED8FF), Color::Hex(0x1E40AFFF), Color::Hex(0x1E3A8AFF), Color::Hex(0xFFFFFFFF), Color::Hex(0x000000FF), 1.5f };
            theme.variants[keel::as<keel::usize>(UiVariant::Success)]   = { Color::Hex(0x15803DFF), Color::Hex(0x166534FF), Color::Hex(0x14532DFF), Color::Hex(0xFFFFFFFF), Color::Hex(0x000000FF), 1.5f };
            theme.variants[keel::as<keel::usize>(UiVariant::Warning)]   = { Color::Hex(0xB45309FF), Color::Hex(0x92400EFF), Color::Hex(0x78350FFF), Color::Hex(0xFFFFFFFF), Color::Hex(0x000000FF), 1.5f };
            theme.variants[keel::as<keel::usize>(UiVariant::Danger)]    = { Color::Hex(0xB91C1CFF), Color::Hex(0x991B1BFF), Color::Hex(0x7F1D1DFF), Color::Hex(0xFFFFFFFF), Color::Hex(0x000000FF), 1.5f };
            theme.variants[keel::as<keel::usize>(UiVariant::Info)]      = { Color::Hex(0x0369A1FF), Color::Hex(0x075985FF), Color::Hex(0x0C4A6EFF), Color::Hex(0xFFFFFFFF), Color::Hex(0x000000FF), 1.5f };
            theme.syntax = SyntaxPalette::GitHubLight();
            PopulateShadows(theme, ThemeMode::Light, 0.0f);
            theme.focusRingWidth = 3.0f;
            break;

        case ThemePreset::Windows98:
            theme.mode = ThemeMode::Light;
            theme.chromeStyle = ChromeStyle::Classic3D;
            theme.fillStyle = FillStyle::Solid;
            theme.palette = {
                .background        = Color::Hex(0xC0C0C0FF), // Classic Win98 COLOR_3DFACE desktop gray
                .surface           = Color::Hex(0xC8C8C8FF), // Classic Win98 dialog & panel gray
                .surfaceHover      = Color::Hex(0xD4D4D4FF),
                .surfaceActive     = Color::Hex(0xB0B0B0FF),
                .surfaceElevated   = Color::Hex(0xC0C0C0FF), // Classic Win98 popup menu & raised window face
                .border            = Color::Hex(0x808080FF), // Classic Win98 COLOR_3DSHADOW
                .borderHover       = Color::Hex(0x000080FF), // Win98 Navy highlight border
                .borderSubtle      = Color::Hex(0x989898FF), // Classic etched separator
                .focusRing         = Color::Hex(0x000080FF), // Iconic Win98 Navy focus outline
                .textPrimary       = Color::Hex(0x000000FF), // Pure black ink (11.5:1 on #C0C0C0)
                .textSecondary     = Color::Hex(0x222222FF),
                .textMuted         = Color::Hex(0x4F4F4FFF),
                .textDisabled      = Color::Hex(0x787878B8), // Classic embossed gray disabled text
                .inputBackground   = Color::Hex(0xFFFFFFFF), // Classic sunken white text field
                .inputBorder       = Color::Hex(0x404040FF), // Classic Win98 COLOR_3DDKSHADOW
                .caret             = Color::Hex(0x000000FF),
                .controlTrack      = Color::Hex(0xB0B0B0FF), // Sunken scrollbar / slider trough
                .controlTrackHover = Color::Hex(0xA4A4A4FF),
                .controlKnob       = Color::Hex(0xC0C0C0FF), // Classic raised silver thumb
                .controlKnobHover  = Color::Hex(0xD0D0D0FF),
                .controlKnobActive = Color::Hex(0xB0B0B0FF),
                .accent            = Color::Hex(0x000080FF), // Iconic Win98 Active Titlebar Navy (16.0:1 with white)
                .accentHover       = Color::Hex(0x0000A8FF), // Rich Win98 hover blue
                .accentText        = Color::Hex(0xFFFFFFFF)
            };
            PopulateVariants(theme, ThemeMode::Light);
            theme.variants[keel::as<keel::usize>(UiVariant::Default)]   = { Color::Hex(0xC0C0C0FF), Color::Hex(0xCCCCCCFF), Color::Hex(0xB0B0B0FF), Color::Hex(0x000000FF), Color::Hex(0x404040FF), 1.0f };
            theme.variants[keel::as<keel::usize>(UiVariant::Secondary)] = theme.variants[keel::as<keel::usize>(UiVariant::Default)];
            theme.variants[keel::as<keel::usize>(UiVariant::Primary)]   = { Color::Hex(0x000080FF), Color::Hex(0x0000A8FF), Color::Hex(0x000060FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x000040FF), 1.0f };
            theme.variants[keel::as<keel::usize>(UiVariant::Success)]   = { Color::Hex(0x006B38FF), Color::Hex(0x008044FF), Color::Hex(0x00522AFF), Color::Hex(0xFFFFFFFF), Color::Hex(0x003D20FF), 1.0f };
            theme.variants[keel::as<keel::usize>(UiVariant::Warning)]   = { Color::Hex(0x8A5300FF), Color::Hex(0xA36200FF), Color::Hex(0x6E4200FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x4D2E00FF), 1.0f };
            theme.variants[keel::as<keel::usize>(UiVariant::Danger)]    = { Color::Hex(0xA80000FF), Color::Hex(0xC40000FF), Color::Hex(0x850000FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x5C0000FF), 1.0f };
            theme.variants[keel::as<keel::usize>(UiVariant::Info)]      = { Color::Hex(0x006666FF), Color::Hex(0x007A7AFF), Color::Hex(0x004F4FFF), Color::Hex(0xFFFFFFFF), Color::Hex(0x003B3BFF), 1.0f };
            theme.syntax = SyntaxPalette::MatlabClassic();
            theme.syntax.editorBg      = Color::Hex(0xFFFFFFFF);
            theme.syntax.gutterBg      = Color::Hex(0xC0C0C0FF);
            theme.syntax.lineHighlight = Color::Hex(0x00008018);
            theme.syntax.selectionBg   = Color::Hex(0x00008040);
            PopulateShadows(theme, ThemeMode::Light, 0.0f);
            break;
    }
    PopulateBevelPalette(theme, /*exactWin98=*/(p == ThemePreset::Windows98));
    PopulateChromeTokens(theme, theme.chromeStyle);
}

// ==============================================================================
// Compact Zero-Dependency JSON Helpers for ThemeSpec
// ==============================================================================

[[nodiscard]] keel::FixedString<16> FormatHexColor(Color c) noexcept {
    if (c.a == 255u) {
        return keel::format_fixed<16>("#{:02X}{:02X}{:02X}", c.r, c.g, c.b);
    }
    return keel::format_fixed<16>("#{:02X}{:02X}{:02X}{:02X}", c.r, c.g, c.b, c.a);
}

[[nodiscard]] keel::i32 HexDigit(char ch) noexcept {
    if (ch >= '0' && ch <= '9') return keel::as<keel::i32>(ch - '0');
    if (ch >= 'a' && ch <= 'f') return 10 + keel::as<keel::i32>(ch - 'a');
    if (ch >= 'A' && ch <= 'F') return 10 + keel::as<keel::i32>(ch - 'A');
    return -1;
}

[[nodiscard]] keel::Optional<Color> ParseHexColor(keel::StringRef s) noexcept {
    if (!s.empty() && s[0] == '#') {
        s = s.substr(1u);
    }
    if (s.size() != 6u && s.size() != 8u) {
        return keel::kNullOpt;
    }
    auto byteAt = [&](keel::usize i) noexcept -> keel::i32 {
        const keel::i32 hi = HexDigit(s[i]);
        const keel::i32 lo = HexDigit(s[i + 1u]);
        return (hi >= 0 && lo >= 0) ? ((hi << 4) | lo) : -1;
    };
    const keel::i32 r = byteAt(0u);
    const keel::i32 g = byteAt(2u);
    const keel::i32 b = byteAt(4u);
    const keel::i32 a = (s.size() == 8u) ? byteAt(6u) : 255;
    if (r < 0 || g < 0 || b < 0 || a < 0) return keel::kNullOpt;
    return Color(keel::as<keel::u8>(r), keel::as<keel::u8>(g), keel::as<keel::u8>(b), keel::as<keel::u8>(a));
}

[[nodiscard]] keel::StringRef PresetJsonKey(ThemePreset p) noexcept {
    switch (p) {
        case ThemePreset::Dark:              return "Dark";
        case ThemePreset::Light:             return "Light";
        case ThemePreset::Nord:              return "Nord";
        case ThemePreset::Dracula:           return "Dracula";
        case ThemePreset::CatppuccinMocha:   return "CatppuccinMocha";
        case ThemePreset::CatppuccinLatte:   return "CatppuccinLatte";
        case ThemePreset::Industrial:        return "Industrial";
        case ThemePreset::IndustrialLight:   return "IndustrialLight";
        case ThemePreset::HighContrastDark:  return "HighContrastDark";
        case ThemePreset::HighContrastLight: return "HighContrastLight";
        case ThemePreset::Windows98:         return "Windows98";
    }
    return "Dark";
}

[[nodiscard]] keel::Optional<ThemePreset> ParsePresetKey(keel::StringRef s) noexcept {
    for (keel::usize i = 0u; i < kThemePresetCount; ++i) {
        const auto p = keel::from_underlying<ThemePreset>(i);
        if (s == PresetJsonKey(p) || s == ThemePresetName(p)) {
            return p;
        }
    }
    return keel::kNullOpt;
}

[[nodiscard]] keel::StringRef ChromeStyleJsonKey(ChromeStyle s) noexcept {
    switch (s) {
        case ChromeStyle::Modern:    return "Modern";
        case ChromeStyle::Fluent:    return "Fluent";
        case ChromeStyle::Classic3D: return "Classic3D";
        case ChromeStyle::Brutalist: return "Brutalist";
    }
    return "Modern";
}

[[nodiscard]] keel::Optional<ChromeStyle> ParseChromeStyleKey(keel::StringRef s) noexcept {
    for (keel::usize i = 0u; i < kChromeStyleCount; ++i) {
        const auto st = keel::from_underlying<ChromeStyle>(i);
        if (s == ChromeStyleJsonKey(st) || s == ChromeStyleName(st)) {
            return st;
        }
    }
    return keel::kNullOpt;
}

[[nodiscard]] keel::StringRef FillStyleJsonKey(FillStyle s) noexcept {
    switch (s) {
        case FillStyle::Solid:   return "Solid";
        case FillStyle::Soft:    return "Soft";
        case FillStyle::Outline: return "Outline";
    }
    return "Solid";
}

[[nodiscard]] keel::Optional<FillStyle> ParseFillStyleKey(keel::StringRef s) noexcept {
    for (keel::usize i = 0u; i < kFillStyleCount; ++i) {
        const auto st = keel::from_underlying<FillStyle>(i);
        if (s == FillStyleJsonKey(st) || s == FillStyleName(st)) {
            return st;
        }
    }
    return keel::kNullOpt;
}

[[nodiscard]] keel::StringRef SyntaxPresetJsonKey(SyntaxPreset p) noexcept {
    switch (p) {
        case SyntaxPreset::Auto:          return "Auto";
        case SyntaxPreset::VsCodeDark:    return "VsCodeDark";
        case SyntaxPreset::Monokai:       return "Monokai";
        case SyntaxPreset::Dracula:       return "Dracula";
        case SyntaxPreset::OneDark:       return "OneDark";
        case SyntaxPreset::Solarized:     return "Solarized";
        case SyntaxPreset::GitHubLight:   return "GitHubLight";
        case SyntaxPreset::MatlabClassic: return "MatlabClassic";
    }
    return "Auto";
}

[[nodiscard]] keel::Optional<SyntaxPreset> ParseSyntaxPresetKey(keel::StringRef s) noexcept {
    for (keel::usize i = 0u; i < kSyntaxPresetCount; ++i) {
        const auto p = keel::from_underlying<SyntaxPreset>(i);
        if (s == SyntaxPresetJsonKey(p) || s == SyntaxPresetName(p)) {
            return p;
        }
    }
    return keel::kNullOpt;
}

void SkipWs(keel::StringRef s, keel::usize& pos) noexcept {
    while (pos < s.size()) {
        const char c = s[pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++pos;
        else break;
    }
}

[[nodiscard]] bool ParseJsonString(keel::StringRef s, keel::usize& pos, keel::StringRef& out) noexcept {
    SkipWs(s, pos);
    if (pos >= s.size() || s[pos] != '"') return false;
    ++pos;
    const keel::usize start = pos;
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\' && pos + 1u < s.size()) pos += 2u;
        else ++pos;
    }
    if (pos >= s.size()) return false;
    out = s.substr(start, pos - start);
    ++pos; // consume closing '"'
    return true;
}

[[nodiscard]] bool ParseJsonNumber(keel::StringRef s, keel::usize& pos, keel::f32& out) noexcept {
    SkipWs(s, pos);
    const keel::usize start = pos;
    while (pos < s.size()) {
        const char c = s[pos];
        if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') ++pos;
        else break;
    }
    if (pos == start) return false;
    const auto parsed = keel::parse_f64(s.substr(start, pos - start));
    if (!parsed.is_ok()) return false;
    out = keel::as<keel::f32>(parsed.value());
    return true;
}

void SkipJsonValue(keel::StringRef s, keel::usize& pos) noexcept {
    SkipWs(s, pos);
    if (pos >= s.size()) return;
    if (s[pos] == '"') {
        keel::StringRef dummy;
        (void)ParseJsonString(s, pos, dummy);
        return;
    }
    if (s[pos] == '{' || s[pos] == '[') {
        const char openCh = s[pos];
        const char closeCh = (openCh == '{') ? '}' : ']';
        keel::i32 depth = 1;
        ++pos;
        while (pos < s.size() && depth > 0) {
            if (s[pos] == '"') {
                keel::StringRef dummy;
                if (!ParseJsonString(s, pos, dummy)) break;
            } else {
                if (s[pos] == openCh) ++depth;
                else if (s[pos] == closeCh) --depth;
                ++pos;
            }
        }
        return;
    }
    while (pos < s.size() && s[pos] != ',' && s[pos] != '}' && s[pos] != ']' &&
           s[pos] != ' ' && s[pos] != '\t' && s[pos] != '\r' && s[pos] != '\n') {
        ++pos;
    }
}

} // namespace

// ==============================================================================
// ThemeSpec JSON Serialization
// ==============================================================================

keel::String ThemeSpec::ToJson() const {
    keel::String out;
    out.reserve(512u);
    out.append("{\n");
    out.append(keel::format("  \"base\": \"{}\",\n", PresetJsonKey(base)));
    if (mode.has_value()) {
        out.append(keel::format("  \"mode\": \"{}\",\n", (*mode == ThemeMode::Light) ? "Light" : "Dark"));
    }
    if (chrome.has_value()) {
        out.append(keel::format("  \"chrome\": \"{}\",\n", ChromeStyleJsonKey(*chrome)));
    }
    if (fillStyle.has_value()) {
        out.append(keel::format("  \"fillStyle\": \"{}\",\n", FillStyleJsonKey(*fillStyle)));
    }
    if (syntaxPreset != SyntaxPreset::Auto) {
        out.append(keel::format("  \"syntaxPreset\": \"{}\",\n", SyntaxPresetJsonKey(syntaxPreset)));
    }
    if (accent.has_value()) {
        out.append(keel::format("  \"accent\": \"{}\",\n", FormatHexColor(*accent)));
    }
    if (neutral.has_value()) {
        out.append(keel::format("  \"neutral\": \"{}\",\n", FormatHexColor(*neutral)));
    }
    if (background.has_value()) {
        out.append(keel::format("  \"background\": \"{}\",\n", FormatHexColor(*background)));
    }
    if (surface.has_value()) {
        out.append(keel::format("  \"surface\": \"{}\",\n", FormatHexColor(*surface)));
    }
    if (border.has_value()) {
        out.append(keel::format("  \"border\": \"{}\",\n", FormatHexColor(*border)));
    }
    if (textPrimary.has_value()) {
        out.append(keel::format("  \"textPrimary\": \"{}\",\n", FormatHexColor(*textPrimary)));
    }
    if (success.has_value()) {
        out.append(keel::format("  \"success\": \"{}\",\n", FormatHexColor(*success)));
    }
    if (warning.has_value()) {
        out.append(keel::format("  \"warning\": \"{}\",\n", FormatHexColor(*warning)));
    }
    if (danger.has_value()) {
        out.append(keel::format("  \"danger\": \"{}\",\n", FormatHexColor(*danger)));
    }
    if (info.has_value()) {
        out.append(keel::format("  \"info\": \"{}\",\n", FormatHexColor(*info)));
    }
    if (scrollbarWidth.has_value()) {
        out.append(keel::format("  \"scrollbarWidth\": {:.2f},\n", *scrollbarWidth));
    }
    out.append(keel::format("  \"surfaceContrast\": {:.2f},\n", surfaceContrast));
    out.append(keel::format("  \"radiusScale\": {:.2f},\n", radiusScale));
    out.append(keel::format("  \"spacingScale\": {:.2f},\n", spacingScale));
    out.append(keel::format("  \"fontScale\": {:.2f},\n", fontScale));
    out.append(keel::format("  \"lineHeightMultiplier\": {:.2f},\n", lineHeightMultiplier));
    out.append(keel::format("  \"borderScale\": {:.2f},\n", borderScale));
    out.append(keel::format("  \"shadowIntensity\": {:.2f},\n", shadowIntensity));
    out.append(keel::format("  \"focusRingWidth\": {:.2f},\n", focusRingWidth));
    out.append(keel::format("  \"disabledOpacity\": {:.2f},\n", disabledOpacity));
    out.append(keel::format("  \"animationScale\": {:.2f},\n", animationScale));
    out.append(keel::format("  \"shellStyle\": \"{}\"\n", (shellStyle == ShellStyle::Floating) ? "Floating" : "Docked"));
    out.append("}");
    return out;
}

keel::Optional<ThemeSpec> ThemeSpec::FromJson(keel::StringRef json) noexcept {
    keel::usize pos = 0u;
    SkipWs(json, pos);
    if (pos >= json.size() || json[pos] != '{') {
        return keel::kNullOpt;
    }
    ++pos;

    ThemeSpec spec{};
    while (pos < json.size()) {
        SkipWs(json, pos);
        if (pos < json.size() && json[pos] == '}') {
            ++pos;
            return spec;
        }
        keel::StringRef key;
        if (!ParseJsonString(json, pos, key)) {
            return keel::kNullOpt;
        }
        SkipWs(json, pos);
        if (pos >= json.size() || json[pos] != ':') {
            return keel::kNullOpt;
        }
        ++pos;
        SkipWs(json, pos);

        if (key == "base") {
            keel::StringRef val;
            if (!ParseJsonString(json, pos, val)) return keel::kNullOpt;
            if (const auto p = ParsePresetKey(val); p.has_value()) spec.base = *p;
        } else if (key == "mode") {
            keel::StringRef val;
            if (!ParseJsonString(json, pos, val)) return keel::kNullOpt;
            if (val == "Light" || val == "light") spec.mode = ThemeMode::Light;
            else if (val == "Dark" || val == "dark") spec.mode = ThemeMode::Dark;
        } else if (key == "chrome") {
            keel::StringRef val;
            if (!ParseJsonString(json, pos, val)) return keel::kNullOpt;
            if (const auto st = ParseChromeStyleKey(val); st.has_value()) spec.chrome = *st;
        } else if (key == "fillStyle") {
            keel::StringRef val;
            if (!ParseJsonString(json, pos, val)) return keel::kNullOpt;
            if (const auto st = ParseFillStyleKey(val); st.has_value()) spec.fillStyle = *st;
        } else if (key == "syntaxPreset") {
            keel::StringRef val;
            if (!ParseJsonString(json, pos, val)) return keel::kNullOpt;
            if (const auto sp = ParseSyntaxPresetKey(val); sp.has_value()) spec.syntaxPreset = *sp;
        } else if (key == "scrollbarWidth") {
            keel::f32 num = 0.0f;
            if (!ParseJsonNumber(json, pos, num)) return keel::kNullOpt;
            spec.scrollbarWidth = keel::clamp(num, 4.0f, 28.0f);
        } else if (key == "shellStyle") {
            keel::StringRef val;
            if (!ParseJsonString(json, pos, val)) return keel::kNullOpt;
            spec.shellStyle = (val == "Floating" || val == "floating") ? ShellStyle::Floating : ShellStyle::Docked;
        } else if (key == "accent" || key == "neutral" || key == "background" ||
                   key == "surface" || key == "border" || key == "textPrimary" ||
                   key == "success" || key == "warning" || key == "danger" || key == "info") {
            keel::StringRef val;
            if (!ParseJsonString(json, pos, val)) return keel::kNullOpt;
            const auto col = ParseHexColor(val);
            if (!col.has_value()) return keel::kNullOpt;
            if (key == "accent") spec.accent = *col;
            else if (key == "neutral") spec.neutral = *col;
            else if (key == "background") spec.background = *col;
            else if (key == "surface") spec.surface = *col;
            else if (key == "border") spec.border = *col;
            else if (key == "textPrimary") spec.textPrimary = *col;
            else if (key == "success") spec.success = *col;
            else if (key == "warning") spec.warning = *col;
            else if (key == "danger") spec.danger = *col;
            else if (key == "info") spec.info = *col;
        } else if (key == "surfaceContrast" || key == "radiusScale" || key == "spacingScale" ||
                   key == "fontScale" || key == "lineHeightMultiplier" || key == "borderScale" ||
                   key == "shadowIntensity" || key == "focusRingWidth" || key == "disabledOpacity" ||
                   key == "animationScale") {
            keel::f32 num = 1.0f;
            if (!ParseJsonNumber(json, pos, num)) return keel::kNullOpt;
            if (key == "surfaceContrast") spec.surfaceContrast = keel::clamp(num, 0.5f, 1.8f);
            else if (key == "radiusScale") spec.radiusScale = keel::clamp(num, 0.0f, 3.0f);
            else if (key == "spacingScale") spec.spacingScale = keel::clamp(num, 0.5f, 2.0f);
            else if (key == "fontScale") spec.fontScale = keel::clamp(num, 0.7f, 2.0f);
            else if (key == "lineHeightMultiplier") spec.lineHeightMultiplier = keel::clamp(num, 1.05f, 2.0f);
            else if (key == "borderScale") spec.borderScale = keel::clamp(num, 0.0f, 3.0f);
            else if (key == "shadowIntensity") spec.shadowIntensity = keel::clamp(num, 0.0f, 2.5f);
            else if (key == "focusRingWidth") spec.focusRingWidth = keel::clamp(num, 1.0f, 5.0f);
            else if (key == "disabledOpacity") spec.disabledOpacity = keel::clamp(num, 0.15f, 0.85f);
            else if (key == "animationScale") spec.animationScale = keel::clamp(num, 0.0f, 2.0f);
        } else {
            SkipJsonValue(json, pos);
        }

        SkipWs(json, pos);
        if (pos < json.size() && json[pos] == ',') {
            ++pos;
        }
    }
    return keel::kNullOpt;
}

// ==============================================================================
// UiTheme Mutators
// ==============================================================================

const IStylePainter& UiTheme::Painter() const noexcept {
    return (stylePainter != nullptr) ? *stylePainter : GetDefaultStylePainter();
}

void UiTheme::SetChromeTokens(const ChromeTokens& tokens) noexcept {
    chrome = tokens;
    SyncScrollbar(*this);
}

void UiTheme::SetScrollbarStyle(const ScrollbarStyleTokens& style) noexcept {
    const bool chromeChanged = (style.chrome != scrollbarStyle.chrome);
    scrollbarStyle = style;
    if (chromeChanged) {
        ApplyScrollbarMetricsForChrome(*this, style.chrome);
    }
}

void UiTheme::SetScrollbarWidth(keel::f32 mdWidth) noexcept {
    const keel::f32 targetMd = keel::clamp(mdWidth, 4.0f, 28.0f);
    const keel::f32 baseMd = keel::max(1.0f, scrollbars[keel::as<keel::usize>(UiSize::Preset::Md)].trackWidth);
    const keel::f32 scale = targetMd / baseMd;
    for (keel::usize i = 1u; i < scrollbars.size(); ++i) {
        scrollbars[i].trackWidth = keel::max(3.0f, keel::round(scrollbars[i].trackWidth * scale));
        scrollbars[i].gutter = scrollbars[i].trackWidth + scrollbars[i].margin * 2.0f;
    }
}

void UiTheme::SetMode(ThemeMode m) noexcept {
    mode = m;
    preset = (m == ThemeMode::Light) ? ThemePreset::Light : ThemePreset::Dark;
    PopulatePalette(*this, m);
    PopulateSyntaxPalette(*this, m);
    PopulateVariants(*this, m);
    PopulateShadows(*this, m, shadowsEnabled ? 1.0f : 0.0f);
    PopulateBevelPalette(*this);
}

void UiTheme::SetPreset(ThemePreset p) noexcept {
    ApplyCuratedPresetColors(*this, p);
}

void UiTheme::SetShellStyle(ShellStyle style) noexcept {
    shellStyle = style;
    if (style == ShellStyle::Docked) {
        shell.windowPadding    = Padding::All(0.0f);
        shell.panelGap         = 0.0f;
        shell.panelRadius      = 0.0f;
        shell.panelBorderWidth = 1.0f * borderScale;
    } else {
        const keel::f32 gap = GetGap(UiSize::Sm);
        shell.windowPadding    = Padding::All(gap);
        shell.panelGap         = gap;
        shell.panelRadius      = GetRadius(UiSize::Md);
        shell.panelBorderWidth = 1.0f * borderScale;
    }
}

void UiTheme::SetAccent(Color accent) noexcept {
    const Color solid = accent.WithAlpha(255u);
    palette.accent      = solid;
    palette.accentHover = (mode == ThemeMode::Dark) ? solid.Lighten(0.10f) : solid.Darken(0.08f);
    palette.accentText  = Color::BestTextColor(solid, Color::Hex(0x0F172AFF), Color::Hex(0xFFFFFFFF));
    palette.focusRing   = (mode == ThemeMode::Dark) ? solid.Lighten(0.06f) : solid;
    palette.caret       = palette.focusRing;

    const keel::f32 prevBw = variants[keel::as<keel::usize>(UiVariant::Primary)].borderWidth;
    const Color prevBorder = variants[keel::as<keel::usize>(UiVariant::Primary)].border;
    VariantColors prim = MakeSemanticVariant(solid, mode, prevBw);
    if (prevBw > 0.0f) prim.border = prevBorder;
    variants[keel::as<keel::usize>(UiVariant::Primary)] = prim;
}

void UiTheme::SetSemanticColor(UiVariant variant, Color baseColor) noexcept {
    const keel::usize idx = keel::as<keel::usize>(variant);
    if (idx >= variants.size()) return;
    if (variant == UiVariant::Primary) {
        SetAccent(baseColor);
        return;
    }
    const keel::f32 prevBw = variants[idx].borderWidth;
    const Color prevBorder = variants[idx].border;
    VariantColors v = MakeSemanticVariant(baseColor.WithAlpha(255u), mode, prevBw);
    if (prevBw > 0.0f) v.border = prevBorder;
    variants[idx] = v;
}

void UiTheme::SetNeutralTint(Color tint, keel::f32 strength) noexcept {
    const keel::f32 s = keel::clamp(strength, 0.0f, 0.65f);
    if (s <= 0.001f) return;
    const Color solidTint = tint.WithAlpha(255u);
    auto blendPreserveAlpha = [solidTint](Color c, keel::f32 t) noexcept -> Color {
        return Color::Lerp(c, solidTint, t).WithAlpha(c.a);
    };
    palette.background        = blendPreserveAlpha(palette.background, s * 0.45f);
    palette.surface           = blendPreserveAlpha(palette.surface, s * 0.55f);
    palette.surfaceHover      = blendPreserveAlpha(palette.surfaceHover, s * 0.60f);
    palette.surfaceActive     = blendPreserveAlpha(palette.surfaceActive, s * 0.65f);
    palette.surfaceElevated   = blendPreserveAlpha(palette.surfaceElevated, s * 0.55f);
    palette.border            = blendPreserveAlpha(palette.border, s * 0.70f);
    palette.borderHover       = blendPreserveAlpha(palette.borderHover, s * 0.65f);
    palette.borderSubtle      = blendPreserveAlpha(palette.borderSubtle, s * 0.65f);
    palette.inputBackground   = blendPreserveAlpha(palette.inputBackground, s * 0.45f);
    palette.inputBorder       = blendPreserveAlpha(palette.inputBorder, s * 0.70f);
    palette.controlTrack      = blendPreserveAlpha(palette.controlTrack, s * 0.60f);
    palette.controlTrackHover = blendPreserveAlpha(palette.controlTrackHover, s * 0.65f);

    for (const UiVariant v : {UiVariant::Default, UiVariant::Secondary}) {
        auto& vc = variants[keel::as<keel::usize>(v)];
        vc.bg       = blendPreserveAlpha(vc.bg, s * 0.60f);
        vc.bgHover  = blendPreserveAlpha(vc.bgHover, s * 0.60f);
        vc.bgActive = blendPreserveAlpha(vc.bgActive, s * 0.60f);
        vc.border   = blendPreserveAlpha(vc.border, s * 0.70f);
    }
}

void UiTheme::SetRadiusScale(keel::f32 scale) noexcept {
    const keel::f32 s = keel::clamp(scale, 0.0f, 3.0f);
    for (keel::usize i = 0u; i < radius.size(); ++i) {
        radius[i] *= s;
    }
    for (keel::usize i = 0u; i < metrics.size(); ++i) {
        metrics[i].cornerRadius *= s;
        checkboxes[i].cornerRadius *= s;
        inputs[i].cornerRadius *= s;
        selects[i].cornerRadius *= s;
        sliders[i].cornerRadius *= s;
        scrollbars[i].cornerRadius *= s;
    }
    if (shellStyle == ShellStyle::Floating) {
        shell.panelRadius = GetRadius(UiSize::Md);
    }
    const bool zeroRadius = (GetRadius(UiSize::Md) <= 0.01f);
    if (chrome.frame == FrameTreatment::Flat) {
        chrome.badgeShape = zeroRadius ? BadgeShape::Sharp : BadgeShape::Pill;
        if (zeroRadius && chrome.sliderThumb == SliderThumbShape::Circle) {
            chrome.sliderThumb = SliderThumbShape::Square;
        } else if (!zeroRadius && chrome.sliderThumb == SliderThumbShape::Square) {
            chrome.sliderThumb = SliderThumbShape::Circle;
        }
    }
}

void UiTheme::SetSpacingScale(keel::f32 scale) noexcept {
    const keel::f32 s = keel::clamp(scale, 0.5f, 2.0f);
    for (keel::usize i = 0u; i < spacing.size(); ++i) {
        spacing[i] *= s;
    }
    for (keel::usize i = 0u; i < metrics.size(); ++i) {
        metrics[i].paddingX *= s;
        metrics[i].paddingY *= s;
        metrics[i].itemGap *= s;
        metrics[i].badgeGap *= s;
        metrics[i].lineGap *= s;
        inputs[i].paddingX *= s;
        selects[i].paddingX *= s;
    }
    SetShellStyle(shellStyle);
}

void UiTheme::SetFontScale(keel::f32 scale) noexcept {
    const keel::f32 s = keel::clamp(scale, 0.7f, 2.0f);
    for (keel::usize i = 0u; i < typography.size(); ++i) {
        typography[i].fontSize *= s;
        typography[i].lineHeight *= s;
    }
    for (keel::usize i = 0u; i < metrics.size(); ++i) {
        metrics[i].fontSize *= s;
        metrics[i].companionFontSize *= s;
        metrics[i].descFontSize *= s;
        metrics[i].badgeFontSize *= s;
        headings[i].titleFontSize *= s;
        headings[i].subtitleFontSize *= s;
        headings[i].badgeFontSize *= s;
    }
}

void UiTheme::SetBorderScale(keel::f32 scale) noexcept {
    const keel::f32 s = keel::clamp(scale, 0.0f, 3.0f);
    const keel::f32 ratio = (borderScale > 1.0e-4f) ? (s / borderScale) : s;
    borderScale = s;
    for (keel::usize i = 0u; i < variants.size(); ++i) {
        variants[i].borderWidth *= ratio;
    }
    for (keel::usize i = 0u; i < inputs.size(); ++i) {
        inputs[i].strokeWidth *= ratio;
        selects[i].strokeWidth *= ratio;
    }
    shell.panelBorderWidth = 1.0f * s;
}

void UiTheme::SetShadowIntensity(keel::f32 intensity) noexcept {
    PopulateShadows(*this, mode, intensity);
}

void UiTheme::SetChromeStyle(ChromeStyle style) noexcept {
    chromeStyle = style;
    const keel::f32 bwScale = keel::max(borderScale, 0.5f);

    if (style == ChromeStyle::Classic3D) {
        SetRadiusScale(0.0f);
        PopulateShadows(*this, mode, 0.0f);
        shell.panelBorderWidth = 1.0f * bwScale;
    } else if (style == ChromeStyle::Brutalist) {
        SetRadiusScale(0.0f);
        const Color hardCol = (mode == ThemeMode::Dark)
            ? Color::Hex(0x000000D8)
            : palette.textPrimary.WithAlpha(220u);
        shadowsEnabled = true;
        shadows = {{
            ShadowStyle{0.0f, {0.0f, 0.0f}, Color(0, 0, 0, 0)},
            ShadowStyle{0.0f, {2.0f, 2.0f}, hardCol},
            ShadowStyle{0.0f, {3.0f, 3.0f}, hardCol},
            ShadowStyle{0.0f, {5.0f, 5.0f}, hardCol}
        }};
        shell.panelBorderWidth = 1.5f * bwScale;
    } else {
        // Modern / Fluent
        if (GetRadius(UiSize::Md) <= 0.01f) {
            ResetDefaultRadii(*this);
        }
        if (style == ChromeStyle::Fluent) {
            ResetDefaultRadii(*this);
            SetRadiusScale(0.90f);
        }
        PopulateShadows(*this, mode, 1.0f);
        shell.panelBorderWidth = 1.0f * bwScale;
    }

    PopulateBevelPalette(*this, /*exactWin98=*/(preset == ThemePreset::Windows98 && style == ChromeStyle::Classic3D));
    PopulateChromeTokens(*this, style);
    SetFillStyle(fillStyle);
}

void UiTheme::SetFillStyle(FillStyle style) noexcept {
    fillStyle = style;
    const bool isDark = (mode == ThemeMode::Dark);
    const keel::f32 bwScale = keel::max(borderScale, 0.5f);
    const bool isBrutalist = (chrome.frame == FrameTreatment::HardShadow);
    const bool isBevel = (chrome.frame == FrameTreatment::Bevel3D);
    const Color boldInk = isDark ? palette.textPrimary : Color::Hex(0x0F172AFF);

    auto ensureReadableInk = [](Color ink, Color bg, bool dark) noexcept -> Color {
        Color candidate = ink.WithAlpha(255u);
        for (keel::i32 step = 0; step < 5; ++step) {
            if (Color::ContrastRatio(candidate, bg) >= 4.5f) {
                return candidate;
            }
            candidate = dark ? candidate.Lighten(0.10f) : candidate.Darken(0.10f);
        }
        return Color::BestTextColor(bg, Color::Hex(0x0F172AFF), Color::Hex(0xFFFFFFFF));
    };

    auto extractSeedColor = [](const VariantColors& vc) noexcept -> Color {
        if (vc.bg.a >= 200u) return vc.bg.WithAlpha(255u);
        if (vc.border.a >= 120u) return vc.border.WithAlpha(255u);
        return vc.text.WithAlpha(255u);
    };

    const keel::StaticArray<UiVariant, 5> kSemantics = {{
        UiVariant::Primary,
        UiVariant::Success,
        UiVariant::Warning,
        UiVariant::Danger,
        UiVariant::Info
    }};

    if (style == FillStyle::Solid) {
        const keel::f32 semBw = isBrutalist ? (1.75f * bwScale) : (isBevel ? (1.0f * bwScale) : 0.0f);
        for (const UiVariant v : kSemantics) {
            auto& vc = variants[keel::as<keel::usize>(v)];
            const Color seed = extractSeedColor(vc);
            vc = MakeSemanticVariant(seed, mode, semBw);
            if (isBrutalist) {
                vc.border = boldInk;
            } else if (isBevel) {
                vc.border = seed.Darken(0.28f);
            }
        }
        for (const UiVariant v : {UiVariant::Default, UiVariant::Secondary}) {
            auto& vc = variants[keel::as<keel::usize>(v)];
            vc.bg          = palette.surface;
            vc.bgHover     = palette.surfaceHover.WithAlpha(255u);
            vc.bgActive    = palette.surfaceActive.WithAlpha(255u);
            vc.text        = palette.textPrimary;
            vc.border      = isBrutalist ? boldInk : palette.border;
            vc.borderWidth = isBrutalist ? (1.75f * bwScale) : (1.0f * bwScale);
        }
        return;
    }

    if (style == FillStyle::Soft) {
        const keel::f32 softBw = isBrutalist ? (1.75f * bwScale) : (1.0f * bwScale);
        for (const UiVariant v : kSemantics) {
            auto& vc = variants[keel::as<keel::usize>(v)];
            const Color seed = extractSeedColor(vc);
            const Color bg       = Color::Lerp(palette.surface, seed, isDark ? 0.22f : 0.15f).WithAlpha(255u);
            const Color bgHover  = Color::Lerp(palette.surface, seed, isDark ? 0.32f : 0.23f).WithAlpha(255u);
            const Color bgActive = Color::Lerp(palette.surface, seed, isDark ? 0.42f : 0.31f).WithAlpha(255u);
            const Color ink      = ensureReadableInk(isDark ? seed.Lighten(0.18f) : seed.Darken(0.16f), bg, isDark);
            const Color bdr      = isBrutalist ? boldInk : Color::Lerp(palette.border, seed, isDark ? 0.45f : 0.40f).WithAlpha(255u);
            vc = VariantColors{bg, bgHover, bgActive, ink, bdr, softBw};
        }
        for (const UiVariant v : {UiVariant::Default, UiVariant::Secondary}) {
            auto& vc = variants[keel::as<keel::usize>(v)];
            vc.bg          = Color::Lerp(palette.surface, palette.surfaceHover.WithAlpha(255u), 0.35f);
            vc.bgHover     = Color::Lerp(palette.surface, palette.surfaceActive.WithAlpha(255u), 0.65f);
            vc.bgActive    = palette.surfaceActive.WithAlpha(255u);
            vc.text        = palette.textPrimary;
            vc.border      = isBrutalist ? boldInk : palette.border;
            vc.borderWidth = softBw;
        }
        return;
    }

    // FillStyle::Outline
    const keel::f32 outBw = isBrutalist ? (1.75f * bwScale) : (1.25f * bwScale);
    for (const UiVariant v : kSemantics) {
        auto& vc = variants[keel::as<keel::usize>(v)];
        const Color seed = extractSeedColor(vc);
        const Color ink = ensureReadableInk(isDark ? seed.Lighten(0.12f) : seed.Darken(0.12f), palette.surface, isDark);
        vc = VariantColors{
            Color::Transparent(),
            seed.WithAlphaF(isDark ? 0.18f : 0.12f),
            seed.WithAlphaF(isDark ? 0.28f : 0.20f),
            ink,
            seed.WithAlphaF(0.85f),
            outBw
        };
    }
    for (const UiVariant v : {UiVariant::Default, UiVariant::Secondary}) {
        auto& vc = variants[keel::as<keel::usize>(v)];
        vc.bg          = Color::Transparent();
        vc.bgHover     = palette.surfaceHover;
        vc.bgActive    = palette.surfaceActive;
        vc.text        = palette.textPrimary;
        vc.border      = isBrutalist ? boldInk : palette.borderHover;
        vc.borderWidth = isBrutalist ? (1.75f * bwScale) : (1.15f * bwScale);
    }
}

void UiTheme::SetSurfaceContrast(keel::f32 contrast) noexcept {
    const keel::f32 c = keel::clamp(contrast, 0.5f, 1.8f);
    if (keel::abs(c - 1.0f) <= 1.0e-3f) return;

    const Color bg = palette.background;
    const Color ink = palette.textPrimary;
    const bool isDark = (mode == ThemeMode::Dark);

    auto scaleAwayFrom = [](Color color, Color anchor, keel::f32 factor) noexcept -> Color {
        const keel::f32 r = keel::clamp(keel::as<keel::f32>(anchor.r) + (keel::as<keel::f32>(color.r) - keel::as<keel::f32>(anchor.r)) * factor, 0.0f, 255.0f);
        const keel::f32 g = keel::clamp(keel::as<keel::f32>(anchor.g) + (keel::as<keel::f32>(color.g) - keel::as<keel::f32>(anchor.g)) * factor, 0.0f, 255.0f);
        const keel::f32 b = keel::clamp(keel::as<keel::f32>(anchor.b) + (keel::as<keel::f32>(color.b) - keel::as<keel::f32>(anchor.b)) * factor, 0.0f, 255.0f);
        return Color(keel::as<keel::u8>(keel::round_i32(r)), keel::as<keel::u8>(keel::round_i32(g)), keel::as<keel::u8>(keel::round_i32(b)), color.a);
    };

    palette.surface           = scaleAwayFrom(palette.surface, bg, c);
    palette.surfaceHover      = scaleAwayFrom(palette.surfaceHover, bg, c);
    palette.surfaceActive     = scaleAwayFrom(palette.surfaceActive, bg, c);
    palette.surfaceElevated   = scaleAwayFrom(palette.surfaceElevated, bg, c);
    palette.border            = scaleAwayFrom(palette.border, bg, c);
    palette.borderHover       = scaleAwayFrom(palette.borderHover, bg, c);
    palette.borderSubtle      = scaleAwayFrom(palette.borderSubtle, bg, c);
    palette.inputBackground   = scaleAwayFrom(palette.inputBackground, bg, c);
    palette.inputBorder       = scaleAwayFrom(palette.inputBorder, bg, c);
    palette.controlTrack      = scaleAwayFrom(palette.controlTrack, bg, c);
    palette.controlTrackHover = scaleAwayFrom(palette.controlTrackHover, bg, c);

    // Subtle shift of secondary/muted text legibility with contrast
    if (c > 1.0f) {
        const keel::f32 boost = keel::clamp((c - 1.0f) * 0.35f, 0.0f, 0.35f);
        palette.textSecondary = Color::Lerp(palette.textSecondary, ink, boost);
        palette.textMuted     = Color::Lerp(palette.textMuted, palette.textSecondary, boost);
    } else {
        const keel::f32 soften = keel::clamp((1.0f - c) * 0.25f, 0.0f, 0.25f);
        palette.textSecondary = Color::Lerp(palette.textSecondary, bg, soften);
        palette.textMuted     = Color::Lerp(palette.textMuted, bg, soften * 0.8f);
    }

    // Keep Default/Secondary buttons consistent with updated surfaces
    for (const UiVariant v : {UiVariant::Default, UiVariant::Secondary}) {
        auto& vc = variants[keel::as<keel::usize>(v)];
        vc.bg       = scaleAwayFrom(vc.bg, bg, c);
        vc.bgHover  = scaleAwayFrom(vc.bgHover, bg, c);
        vc.bgActive = scaleAwayFrom(vc.bgActive, bg, c);
        vc.border   = scaleAwayFrom(vc.border, bg, c);
    }
    (void)isDark;
}

void UiTheme::SetSyntaxPreset(SyntaxPreset sp) noexcept {
    syntaxPreset = sp;
    switch (sp) {
        case SyntaxPreset::Auto:          break;
        case SyntaxPreset::VsCodeDark:    syntax = SyntaxPalette::VsCodeDark(); break;
        case SyntaxPreset::Monokai:       syntax = SyntaxPalette::Monokai(); break;
        case SyntaxPreset::Dracula:       syntax = SyntaxPalette::Dracula(); break;
        case SyntaxPreset::OneDark:       syntax = SyntaxPalette::OneDark(); break;
        case SyntaxPreset::Solarized:     syntax = SyntaxPalette::Solarized(); break;
        case SyntaxPreset::GitHubLight:   syntax = SyntaxPalette::GitHubLight(); break;
        case SyntaxPreset::MatlabClassic: syntax = SyntaxPalette::MatlabClassic(); break;
    }
}

// ==============================================================================
// Curated Factories, Builder & Lerp
// ==============================================================================

UiTheme UiTheme::FromPreset(ThemePreset p) noexcept {
    UiTheme theme;
    PopulateAllMetrics(theme);
    ApplyCuratedPresetColors(theme, p);
    if (p == ThemePreset::Industrial || p == ThemePreset::IndustrialLight) {
        theme.SetRadiusScale(0.55f);
    } else if (p == ThemePreset::HighContrastDark || p == ThemePreset::HighContrastLight) {
        theme.SetRadiusScale(0.45f);
        theme.SetBorderScale(1.5f);
    } else if (p == ThemePreset::Windows98) {
        theme.SetRadiusScale(0.0f);
    }
    return theme;
}

UiTheme UiTheme::Dark() noexcept              { return FromPreset(ThemePreset::Dark); }
UiTheme UiTheme::Light() noexcept             { return FromPreset(ThemePreset::Light); }
UiTheme UiTheme::Nord() noexcept              { return FromPreset(ThemePreset::Nord); }
UiTheme UiTheme::Dracula() noexcept           { return FromPreset(ThemePreset::Dracula); }
UiTheme UiTheme::CatppuccinMocha() noexcept   { return FromPreset(ThemePreset::CatppuccinMocha); }
UiTheme UiTheme::CatppuccinLatte() noexcept   { return FromPreset(ThemePreset::CatppuccinLatte); }
UiTheme UiTheme::Industrial() noexcept        { return FromPreset(ThemePreset::Industrial); }
UiTheme UiTheme::IndustrialLight() noexcept   { return FromPreset(ThemePreset::IndustrialLight); }
UiTheme UiTheme::HighContrastDark() noexcept  { return FromPreset(ThemePreset::HighContrastDark); }
UiTheme UiTheme::HighContrastLight() noexcept { return FromPreset(ThemePreset::HighContrastLight); }
UiTheme UiTheme::Windows98() noexcept         { return FromPreset(ThemePreset::Windows98); }

UiTheme UiTheme::Build(const ThemeSpec& spec) noexcept {
    UiTheme theme = FromPreset(spec.base);
    if (spec.mode.has_value() && *spec.mode != theme.mode) {
        theme.SetMode(*spec.mode);
    }
    if (spec.neutral.has_value()) {
        theme.SetNeutralTint(*spec.neutral, 0.30f);
    }
    if (keel::abs(spec.surfaceContrast - 1.0f) > 1.0e-4f) {
        theme.SetSurfaceContrast(spec.surfaceContrast);
    }
    if (spec.background.has_value()) {
        theme.palette.background = spec.background->WithAlpha(255u);
    }
    if (spec.surface.has_value()) {
        const Color s = spec.surface->WithAlpha(255u);
        theme.palette.surface = s;
        theme.palette.surfaceElevated = (theme.mode == ThemeMode::Dark) ? s.Lighten(0.03f) : s;
        theme.palette.surfaceHover = (theme.mode == ThemeMode::Dark) ? s.Lighten(0.08f).WithAlpha(180u) : s.Darken(0.05f);
        theme.palette.surfaceActive = (theme.mode == ThemeMode::Dark) ? s.Lighten(0.12f) : s.Darken(0.10f);
    }
    if (spec.border.has_value()) {
        const Color b = spec.border->WithAlpha(255u);
        theme.palette.border = b;
        theme.palette.inputBorder = b;
        theme.palette.borderHover = (theme.mode == ThemeMode::Dark) ? b.Lighten(0.14f) : b.Darken(0.14f);
        theme.palette.borderSubtle = b.WithAlpha(140u);
    }
    if (spec.textPrimary.has_value()) {
        const Color t = spec.textPrimary->WithAlpha(255u);
        theme.palette.textPrimary = t;
        theme.palette.textSecondary = Color::Lerp(t, theme.palette.background, 0.32f);
        theme.palette.textMuted = Color::Lerp(t, theme.palette.background, 0.52f);
        theme.palette.textDisabled = theme.palette.textMuted.WithAlpha(128u);
    }
    if (spec.accent.has_value()) {
        theme.SetAccent(*spec.accent);
    }
    if (spec.success.has_value()) theme.SetSemanticColor(UiVariant::Success, *spec.success);
    if (spec.warning.has_value()) theme.SetSemanticColor(UiVariant::Warning, *spec.warning);
    if (spec.danger.has_value())  theme.SetSemanticColor(UiVariant::Danger, *spec.danger);
    if (spec.info.has_value())    theme.SetSemanticColor(UiVariant::Info, *spec.info);

    const bool customPaletteColors = spec.neutral.has_value()
        || spec.background.has_value()
        || spec.surface.has_value()
        || keel::abs(spec.surfaceContrast - 1.0f) > 1.0e-4f;

    if (spec.chrome.has_value()) {
        theme.SetChromeStyle(*spec.chrome);
    }
    if (spec.fillStyle.has_value()) {
        theme.SetFillStyle(*spec.fillStyle);
    }
    if (customPaletteColors) {
        PopulateBevelPalette(theme, /*exactWin98=*/false);
    }
    if (spec.syntaxPreset != SyntaxPreset::Auto) {
        theme.SetSyntaxPreset(spec.syntaxPreset);
    }
    if (keel::abs(spec.radiusScale - 1.0f) > 1.0e-4f) {
        if (theme.GetRadius(UiSize::Md) <= 0.01f && spec.radiusScale > 0.0f) {
            ResetDefaultRadii(theme);
        }
        theme.SetRadiusScale(spec.radiusScale);
    }
    if (keel::abs(spec.spacingScale - 1.0f) > 1.0e-4f) {
        theme.SetSpacingScale(spec.spacingScale);
    }
    if (keel::abs(spec.fontScale - 1.0f) > 1.0e-4f) {
        theme.SetFontScale(spec.fontScale);
    }
    if (keel::abs(spec.lineHeightMultiplier - kDefaultLineHeightMultiplier) > 1.0e-4f) {
        const keel::f32 lhRatio = keel::clamp(spec.lineHeightMultiplier, 1.05f, 2.0f) / kDefaultLineHeightMultiplier;
        theme.lineHeightMultiplier = keel::clamp(spec.lineHeightMultiplier, 1.05f, 2.0f);
        for (keel::usize i = 0u; i < theme.typography.size(); ++i) {
            theme.typography[i].lineHeight *= lhRatio;
        }
    }
    if (keel::abs(spec.borderScale - 1.0f) > 1.0e-4f) {
        theme.SetBorderScale(spec.borderScale);
    }
    if (keel::abs(spec.shadowIntensity - 1.0f) > 1.0e-4f) {
        theme.SetShadowIntensity(spec.shadowIntensity);
    } else if (theme.chromeStyle == ChromeStyle::Classic3D) {
        theme.SetShadowIntensity(0.0f);
    }
    if (keel::abs(spec.focusRingWidth - 2.0f) > 1.0e-4f) {
        theme.focusRingWidth = keel::clamp(spec.focusRingWidth, 1.0f, 5.0f);
    }
    theme.disabledOpacity = keel::clamp(spec.disabledOpacity, 0.15f, 0.85f);
    theme.animationScale = keel::clamp(spec.animationScale, 0.0f, 2.0f);
    if (spec.scrollbarWidth.has_value()) {
        theme.SetScrollbarWidth(*spec.scrollbarWidth);
    }
    theme.SetShellStyle(spec.shellStyle);
    return theme;
}

UiTheme UiTheme::Lerp(const UiTheme& a, const UiTheme& b, keel::f32 t) noexcept {
    const keel::f32 f = keel::clamp(t, 0.0f, 1.0f);
    if (f <= 0.0f) return a;
    if (f >= 1.0f) return b;

    auto lerpF = [f](keel::f32 x, keel::f32 y) noexcept -> keel::f32 { return x + f * (y - x); };
    auto lerpC = [f](Color x, Color y) noexcept -> Color { return Color::Lerp(x, y, f); };

    UiTheme out = (f < 0.5f) ? a : b;
    out.lineHeightMultiplier = lerpF(a.lineHeightMultiplier, b.lineHeightMultiplier);
    out.focusRingWidth = lerpF(a.focusRingWidth, b.focusRingWidth);
    out.disabledOpacity = lerpF(a.disabledOpacity, b.disabledOpacity);
    out.borderScale = lerpF(a.borderScale, b.borderScale);
    out.animationScale = lerpF(a.animationScale, b.animationScale);
    out.shadowsEnabled = a.shadowsEnabled || b.shadowsEnabled;

    // Bevel palette & structural offsets
    out.bevel.highlightOuter = lerpC(a.bevel.highlightOuter, b.bevel.highlightOuter);
    out.bevel.highlightInner = lerpC(a.bevel.highlightInner, b.bevel.highlightInner);
    out.bevel.shadowInner    = lerpC(a.bevel.shadowInner, b.bevel.shadowInner);
    out.bevel.shadowOuter    = lerpC(a.bevel.shadowOuter, b.bevel.shadowOuter);
    out.chrome.buttonPressOffset = lerpF(a.chrome.buttonPressOffset, b.chrome.buttonPressOffset);

    // Palette
    out.palette.background        = lerpC(a.palette.background, b.palette.background);
    out.palette.surface           = lerpC(a.palette.surface, b.palette.surface);
    out.palette.surfaceHover      = lerpC(a.palette.surfaceHover, b.palette.surfaceHover);
    out.palette.surfaceActive     = lerpC(a.palette.surfaceActive, b.palette.surfaceActive);
    out.palette.surfaceElevated   = lerpC(a.palette.surfaceElevated, b.palette.surfaceElevated);
    out.palette.border            = lerpC(a.palette.border, b.palette.border);
    out.palette.borderHover       = lerpC(a.palette.borderHover, b.palette.borderHover);
    out.palette.borderSubtle      = lerpC(a.palette.borderSubtle, b.palette.borderSubtle);
    out.palette.focusRing         = lerpC(a.palette.focusRing, b.palette.focusRing);
    out.palette.textPrimary       = lerpC(a.palette.textPrimary, b.palette.textPrimary);
    out.palette.textSecondary     = lerpC(a.palette.textSecondary, b.palette.textSecondary);
    out.palette.textMuted         = lerpC(a.palette.textMuted, b.palette.textMuted);
    out.palette.textDisabled      = lerpC(a.palette.textDisabled, b.palette.textDisabled);
    out.palette.inputBackground   = lerpC(a.palette.inputBackground, b.palette.inputBackground);
    out.palette.inputBorder       = lerpC(a.palette.inputBorder, b.palette.inputBorder);
    out.palette.caret             = lerpC(a.palette.caret, b.palette.caret);
    out.palette.controlTrack      = lerpC(a.palette.controlTrack, b.palette.controlTrack);
    out.palette.controlTrackHover = lerpC(a.palette.controlTrackHover, b.palette.controlTrackHover);
    out.palette.controlKnob       = lerpC(a.palette.controlKnob, b.palette.controlKnob);
    out.palette.controlKnobHover  = lerpC(a.palette.controlKnobHover, b.palette.controlKnobHover);
    out.palette.controlKnobActive = lerpC(a.palette.controlKnobActive, b.palette.controlKnobActive);
    out.palette.accent            = lerpC(a.palette.accent, b.palette.accent);
    out.palette.accentHover       = lerpC(a.palette.accentHover, b.palette.accentHover);
    out.palette.accentText        = lerpC(a.palette.accentText, b.palette.accentText);

    // Syntax
    out.syntax.keyword       = lerpC(a.syntax.keyword, b.syntax.keyword);
    out.syntax.type          = lerpC(a.syntax.type, b.syntax.type);
    out.syntax.function      = lerpC(a.syntax.function, b.syntax.function);
    out.syntax.string        = lerpC(a.syntax.string, b.syntax.string);
    out.syntax.number        = lerpC(a.syntax.number, b.syntax.number);
    out.syntax.comment       = lerpC(a.syntax.comment, b.syntax.comment);
    out.syntax.preprocessor  = lerpC(a.syntax.preprocessor, b.syntax.preprocessor);
    out.syntax.punctuation   = lerpC(a.syntax.punctuation, b.syntax.punctuation);
    out.syntax.identifier    = lerpC(a.syntax.identifier, b.syntax.identifier);
    out.syntax.lineHighlight = lerpC(a.syntax.lineHighlight, b.syntax.lineHighlight);
    out.syntax.selectionBg   = lerpC(a.syntax.selectionBg, b.syntax.selectionBg);
    out.syntax.editorBg      = lerpC(a.syntax.editorBg, b.syntax.editorBg);
    out.syntax.gutterBg      = lerpC(a.syntax.gutterBg, b.syntax.gutterBg);

    // Variants
    for (keel::usize i = 0u; i < out.variants.size(); ++i) {
        out.variants[i].bg          = lerpC(a.variants[i].bg, b.variants[i].bg);
        out.variants[i].bgHover     = lerpC(a.variants[i].bgHover, b.variants[i].bgHover);
        out.variants[i].bgActive    = lerpC(a.variants[i].bgActive, b.variants[i].bgActive);
        out.variants[i].text        = lerpC(a.variants[i].text, b.variants[i].text);
        out.variants[i].border      = lerpC(a.variants[i].border, b.variants[i].border);
        out.variants[i].borderWidth = lerpF(a.variants[i].borderWidth, b.variants[i].borderWidth);
    }

    // Shadows, spacing, radii
    for (keel::usize i = 0u; i < out.shadows.size(); ++i) {
        out.shadows[i].blur     = lerpF(a.shadows[i].blur, b.shadows[i].blur);
        out.shadows[i].offset.x = lerpF(a.shadows[i].offset.x, b.shadows[i].offset.x);
        out.shadows[i].offset.y = lerpF(a.shadows[i].offset.y, b.shadows[i].offset.y);
        out.shadows[i].color    = lerpC(a.shadows[i].color, b.shadows[i].color);
    }
    for (keel::usize i = 0u; i < out.radius.size(); ++i) {
        out.radius[i]  = lerpF(a.radius[i], b.radius[i]);
        out.spacing[i] = lerpF(a.spacing[i], b.spacing[i]);
        out.metrics[i].cornerRadius = lerpF(a.metrics[i].cornerRadius, b.metrics[i].cornerRadius);
        out.inputs[i].cornerRadius  = lerpF(a.inputs[i].cornerRadius, b.inputs[i].cornerRadius);
        out.selects[i].cornerRadius = lerpF(a.selects[i].cornerRadius, b.selects[i].cornerRadius);
        out.checkboxes[i].cornerRadius = lerpF(a.checkboxes[i].cornerRadius, b.checkboxes[i].cornerRadius);
        out.sliders[i].cornerRadius = lerpF(a.sliders[i].cornerRadius, b.sliders[i].cornerRadius);
    }

    return out;
}

} // namespace guikit
