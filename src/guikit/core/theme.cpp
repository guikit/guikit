#include "guikit/core/theme.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

void PopulatePalette(UiTheme& theme, ThemeMode mode) {
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
            .focusRing         = Color::Hex(0x0284C7FF), // Sky-600
            .textPrimary       = Color::Hex(0x0F172AFF), // Slate-900
            .textSecondary     = Color::Hex(0x64748BFF), // Slate-500
            .textMuted         = Color::Hex(0x94A3B8FF), // Slate-400
            .textDisabled      = Color::Hex(0x94A3B880), // Slate-400 / 50%
            .inputBackground   = Color::Hex(0xF8FAFCFF), // Slate-50
            .inputBorder       = Color::Hex(0xCBD5E1FF), // Slate-300
            .caret             = Color::Hex(0x0284C7FF), // Sky-600
            .controlTrack      = Color::Hex(0xE2E8F0FF), // Slate-200
            .controlTrackHover = Color::Hex(0xCBD5E1FF), // Slate-300
            .controlKnob       = Color::Hex(0xF8FAFCFF), // Slate-50
            .controlKnobHover  = Color::Hex(0xFFFFFFFF), // White
            .controlKnobActive = Color::Hex(0xFFFFFFFF), // White
            .accent            = Color::Hex(0x0284C7FF), // Sky-600
            .accentHover       = Color::Hex(0x0369A1FF), // Sky-700
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

void PopulateSyntaxPalette(UiTheme& theme, ThemeMode mode) {
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

void PopulateVariants(UiTheme& theme, ThemeMode mode) {
    if (mode == ThemeMode::Light) {
        theme.variants = {{
            // Default
            VariantColors{ Color::Hex(0xE2E8F0FF), Color::Hex(0xCBD5E1FF), Color::Hex(0x94A3B8FF), Color::Hex(0x0F172AFF), Color::Hex(0xCBD5E1FF), 1.0f },
            // Primary
            VariantColors{ Color::Hex(0x2563EBFF), Color::Hex(0x3B82F6FF), Color::Hex(0x1D4ED8FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            // Secondary
            VariantColors{ Color::Hex(0xE2E8F0FF), Color::Hex(0xCBD5E1FF), Color::Hex(0x94A3B8FF), Color::Hex(0x0F172AFF), Color::Hex(0xCBD5E1FF), 1.0f },
            // Success
            VariantColors{ Color::Hex(0x059669FF), Color::Hex(0x10B981FF), Color::Hex(0x047857FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            // Warning
            VariantColors{ Color::Hex(0xD97706FF), Color::Hex(0xF59E0BFF), Color::Hex(0xB45309FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            // Danger
            VariantColors{ Color::Hex(0xDC2626FF), Color::Hex(0xEF4444FF), Color::Hex(0xB91C1CFF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            // Info
            VariantColors{ Color::Hex(0x0891B2FF), Color::Hex(0x06B6D4FF), Color::Hex(0x0E7490FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            // Ghost
            VariantColors{ Color::Hex(0x00000000), Color::Hex(0x0F172A0D), Color::Hex(0x0F172A1A), Color::Hex(0x334155FF), Color::Hex(0x00000000), 0.0f },
            // Muted
            VariantColors{ Color::Hex(0x00000000), Color::Hex(0x00000000), Color::Hex(0x00000000), Color::Hex(0x64748BFF), Color::Hex(0x00000000), 0.0f }
        }};
    } else {
        theme.variants = {{
            // Default
            VariantColors{ Color::Hex(0x334155FF), Color::Hex(0x475569FF), Color::Hex(0x1E293BFF), Color::Hex(0xF8FAFCFF), Color::Hex(0x64748B40), 1.0f },
            // Primary
            VariantColors{ Color::Hex(0x3B82F6FF), Color::Hex(0x60A5FAFF), Color::Hex(0x2563EBFF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            // Secondary
            VariantColors{ Color::Hex(0x334155FF), Color::Hex(0x475569FF), Color::Hex(0x1E293BFF), Color::Hex(0xF8FAFCFF), Color::Hex(0x64748B40), 1.0f },
            // Success
            VariantColors{ Color::Hex(0x10B981FF), Color::Hex(0x34D399FF), Color::Hex(0x059669FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            // Warning
            VariantColors{ Color::Hex(0xF59E0BFF), Color::Hex(0xFBBF24FF), Color::Hex(0xD97706FF), Color::Hex(0x0F172AFF), Color::Hex(0x00000000), 0.0f },
            // Danger
            VariantColors{ Color::Hex(0xEF4444FF), Color::Hex(0xF87171FF), Color::Hex(0xDC2626FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            // Info
            VariantColors{ Color::Hex(0x06B6D4FF), Color::Hex(0x22D3EEFF), Color::Hex(0x0891B2FF), Color::Hex(0xFFFFFFFF), Color::Hex(0x00000000), 0.0f },
            // Ghost
            VariantColors{ Color::Hex(0x00000000), Color::Hex(0xFFFFFF18), Color::Hex(0xFFFFFF28), Color::Hex(0xF1F5F9FF), Color::Hex(0x00000000), 0.0f },
            // Muted
            VariantColors{ Color::Hex(0x00000000), Color::Hex(0x00000000), Color::Hex(0x00000000), Color::Hex(0x94A3B8FF), Color::Hex(0x00000000), 0.0f }
        }};
    }
}

void PopulateTypography(UiTheme& theme) {
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

void PopulateScrollbars(UiTheme& theme) {
    theme.scrollbars = {{
        ScrollbarMetrics{  0.0f,  0.0f, 0.0f,  0.0f, 0.0f,  0.0f }, // None
        ScrollbarMetrics{  3.0f, 12.0f, 1.5f, 18.0f, 1.5f,  6.0f }, // Xs
        ScrollbarMetrics{  4.5f, 15.0f, 2.25f, 24.0f, 2.25f, 9.0f }, // Sm
        ScrollbarMetrics{  6.0f, 21.0f, 3.0f, 30.0f, 2.25f, 10.5f }, // Md
        ScrollbarMetrics{ 7.5f, 27.0f, 3.75f, 36.0f, 3.0f, 13.5f }, // Lg
        ScrollbarMetrics{ 9.0f, 33.0f, 4.5f, 42.0f, 3.0f, 15.0f }  // Xl
    }};
}

void PopulateCarousels(UiTheme& theme) {
    theme.carousels = {{
        CarouselMetrics{ 19.5f, 10.5f,  4.5f, 3.0f, 4.5f, 27.0f, 82.5f }, // None
        CarouselMetrics{ 13.5f, 7.5f,  2.25f, 1.5f, 3.0f, 18.0f,  45.0f }, // Xs
        CarouselMetrics{ 16.5f, 9.0f,  3.0f, 2.25f, 3.75f, 22.5f,  60.0f }, // Sm
        CarouselMetrics{ 19.5f, 10.5f,  4.5f, 3.0f, 4.5f, 27.0f, 82.5f }, // Md
        CarouselMetrics{ 24.0f, 12.0f,  6.0f, 3.75f, 6.0f, 33.75f, 105.0f }, // Lg
        CarouselMetrics{ 30.0f, 15.0f, 7.5f, 4.5f, 7.5f, 42.0f, 135.0f }  // Xl
    }};
}

void PopulateComponentMetrics(UiTheme& theme) {
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
        InputFieldMetrics{ 39.0f, 7.5f, 15.0f, 7.5f, 7.5f, 1.5f, 16.5f }  // Xl
    }};

    theme.selects = {{
        SelectMetrics{ 27.0f,  4.5f, 9.0f,  4.5f,  4.5f, 1.0f, 12.0f, 24.0f }, // None
        SelectMetrics{ 18.0f,  3.0f,  6.0f,  3.0f,  3.0f, 1.0f, 9.0f, 16.5f }, // Xs
        SelectMetrics{ 21.0f,  3.75f, 7.5f,  3.75f,  3.75f, 1.0f, 10.5f, 19.5f }, // Sm
        SelectMetrics{ 27.0f,  4.5f, 9.0f,  4.5f,  4.5f, 1.0f, 12.0f, 24.0f }, // Md
        SelectMetrics{ 33.0f,  6.0f, 12.0f,  6.0f,  6.0f, 1.5f, 13.5f, 28.5f }, // Lg
        SelectMetrics{ 39.0f, 7.5f, 15.0f, 7.5f, 7.5f, 1.5f, 16.5f, 33.0f }  // Xl
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

void PopulateBaseMetrics(UiTheme& theme) {
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

} // namespace

void UiTheme::SetMode(ThemeMode m) noexcept {
    mode = m;
    PopulatePalette(*this, m);
    PopulateSyntaxPalette(*this, m);
    PopulateVariants(*this, m);
}

void UiTheme::SetShellStyle(ShellStyle style) noexcept {
    shellStyle = style;
    if (style == ShellStyle::Docked) {
        shell.windowPadding    = Padding::All(0.0f);
        shell.panelGap         = 0.0f;
        shell.panelRadius      = 0.0f;
        shell.panelBorderWidth = 1.0f;
    } else {
        const keel::f32 gap = GetGap(UiSize::Sm);
        shell.windowPadding    = Padding::All(gap);
        shell.panelGap         = gap;
        shell.panelRadius      = GetRadius(UiSize::Md);
        shell.panelBorderWidth = 1.0f;
    }
}

UiTheme UiTheme::Dark() noexcept {
    UiTheme theme;
    theme.mode = ThemeMode::Dark;
    PopulatePalette(theme, ThemeMode::Dark);
    PopulateSyntaxPalette(theme, ThemeMode::Dark);
    PopulateVariants(theme, ThemeMode::Dark);
    PopulateTypography(theme);
    PopulateScrollbars(theme);
    PopulateCarousels(theme);
    PopulateComponentMetrics(theme);
    PopulateBaseMetrics(theme);
    return theme;
}

UiTheme UiTheme::Light() noexcept {
    UiTheme theme;
    theme.mode = ThemeMode::Light;
    PopulatePalette(theme, ThemeMode::Light);
    PopulateSyntaxPalette(theme, ThemeMode::Light);
    PopulateVariants(theme, ThemeMode::Light);
    PopulateTypography(theme);
    PopulateScrollbars(theme);
    PopulateCarousels(theme);
    PopulateComponentMetrics(theme);
    PopulateBaseMetrics(theme);
    return theme;
}

} // namespace guikit
