#include "guikit/core/Theme.hpp"
#include <cmath>

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
        ScrollbarMetrics{  4.0f, 16.0f, 2.0f, 24.0f, 2.0f,  8.0f }, // Xs
        ScrollbarMetrics{  6.0f, 20.0f, 3.0f, 32.0f, 3.0f, 12.0f }, // Sm
        ScrollbarMetrics{  8.0f, 28.0f, 4.0f, 40.0f, 3.0f, 14.0f }, // Md
        ScrollbarMetrics{ 10.0f, 36.0f, 5.0f, 48.0f, 4.0f, 18.0f }, // Lg
        ScrollbarMetrics{ 12.0f, 44.0f, 6.0f, 56.0f, 4.0f, 20.0f }  // Xl
    }};
}

void PopulateCarousels(UiTheme& theme) {
    theme.carousels = {{
        CarouselMetrics{ 26.0f, 14.0f,  6.0f, 4.0f, 6.0f, 36.0f, 110.0f }, // None
        CarouselMetrics{ 18.0f, 10.0f,  3.0f, 2.0f, 4.0f, 24.0f,  60.0f }, // Xs
        CarouselMetrics{ 22.0f, 12.0f,  4.0f, 3.0f, 5.0f, 30.0f,  80.0f }, // Sm
        CarouselMetrics{ 26.0f, 14.0f,  6.0f, 4.0f, 6.0f, 36.0f, 110.0f }, // Md
        CarouselMetrics{ 32.0f, 16.0f,  8.0f, 5.0f, 8.0f, 45.0f, 140.0f }, // Lg
        CarouselMetrics{ 40.0f, 20.0f, 10.0f, 6.0f, 10.0f, 56.0f, 180.0f }  // Xl
    }};
}

void PopulateComponentMetrics(UiTheme& theme) {
    theme.switches = {{
        SwitchMetrics{ {36.0f, 20.0f}, 16.0f, 2.0f, 10.0f, 3.0f }, // None
        SwitchMetrics{ {24.0f, 14.0f}, 10.0f, 2.0f,  6.0f, 2.0f }, // Xs
        SwitchMetrics{ {28.0f, 16.0f}, 12.0f, 2.0f,  8.0f, 2.0f }, // Sm
        SwitchMetrics{ {36.0f, 20.0f}, 16.0f, 2.0f, 10.0f, 3.0f }, // Md
        SwitchMetrics{ {44.0f, 24.0f}, 20.0f, 2.0f, 12.0f, 3.0f }, // Lg
        SwitchMetrics{ {52.0f, 28.0f}, 24.0f, 2.0f, 14.0f, 4.0f }  // Xl
    }};

    theme.checkboxes = {{
        CheckboxMetrics{ 18.0f, 4.0f, 10.0f, 3.0f, 2.0f }, // None
        CheckboxMetrics{ 12.0f, 2.5f,  6.0f, 2.0f, 1.5f }, // Xs
        CheckboxMetrics{ 14.0f, 3.0f,  8.0f, 2.0f, 1.8f }, // Sm
        CheckboxMetrics{ 18.0f, 4.0f, 10.0f, 3.0f, 2.0f }, // Md
        CheckboxMetrics{ 22.0f, 5.0f, 12.0f, 3.0f, 2.4f }, // Lg
        CheckboxMetrics{ 26.0f, 6.0f, 14.0f, 4.0f, 2.8f }  // Xl
    }};

    theme.inputs = {{
        InputFieldMetrics{ 36.0f,  6.0f, 12.0f, 4.0f, 3.0f, 1.0f, 16.0f }, // None
        InputFieldMetrics{ 24.0f,  4.0f,  8.0f, 3.0f, 2.0f, 1.0f, 12.0f }, // Xs
        InputFieldMetrics{ 28.0f,  5.0f, 10.0f, 3.0f, 2.0f, 1.0f, 14.0f }, // Sm
        InputFieldMetrics{ 36.0f,  6.0f, 12.0f, 4.0f, 3.0f, 1.0f, 16.0f }, // Md
        InputFieldMetrics{ 44.0f,  8.0f, 16.0f, 5.0f, 3.0f, 1.5f, 18.0f }, // Lg
        InputFieldMetrics{ 52.0f, 10.0f, 20.0f, 6.0f, 4.0f, 1.5f, 22.0f }  // Xl
    }};

    theme.selects = {{
        SelectMetrics{ 36.0f,  6.0f, 12.0f, 4.0f, 3.0f, 1.0f, 16.0f, 32.0f }, // None
        SelectMetrics{ 24.0f,  4.0f,  8.0f, 3.0f, 2.0f, 1.0f, 12.0f, 22.0f }, // Xs
        SelectMetrics{ 28.0f,  5.0f, 10.0f, 3.0f, 2.0f, 1.0f, 14.0f, 26.0f }, // Sm
        SelectMetrics{ 36.0f,  6.0f, 12.0f, 4.0f, 3.0f, 1.0f, 16.0f, 32.0f }, // Md
        SelectMetrics{ 44.0f,  8.0f, 16.0f, 5.0f, 3.0f, 1.5f, 18.0f, 38.0f }, // Lg
        SelectMetrics{ 52.0f, 10.0f, 20.0f, 6.0f, 4.0f, 1.5f, 22.0f, 44.0f }  // Xl
    }};

    theme.headings = {{
        HeadingMetrics{ 16.0f, 13.0f, 11.0f, 16.0f, 6.0f, 8.0f, 2.0f }, // None
        HeadingMetrics{ 13.0f, 10.0f,  9.0f, 12.0f, 4.0f, 6.0f, 1.5f }, // Xs
        HeadingMetrics{ 14.0f, 11.0f, 10.0f, 14.0f, 5.0f, 6.0f, 2.0f }, // Sm
        HeadingMetrics{ 16.0f, 13.0f, 11.0f, 16.0f, 6.0f, 8.0f, 2.0f }, // Md
        HeadingMetrics{ 18.0f, 14.0f, 12.0f, 18.0f, 8.0f, 8.0f, 3.0f }, // Lg
        HeadingMetrics{ 22.0f, 15.0f, 13.0f, 22.0f, 8.0f, 10.0f, 4.0f }  // Xl
    }};

    theme.sliders = {{
        SliderMetrics{ 26.0f, 5.0f, 18.0f, 9.0f, 4.0f, 5.0f, 5.0f, 3.0f }, // None
        SliderMetrics{ 18.0f, 3.0f, 12.0f, 6.0f, 3.0f, 3.0f, 3.0f, 2.0f }, // Xs
        SliderMetrics{ 22.0f, 4.0f, 14.0f, 7.0f, 3.5f, 4.0f, 4.0f, 2.0f }, // Sm
        SliderMetrics{ 26.0f, 5.0f, 18.0f, 9.0f, 4.0f, 5.0f, 5.0f, 3.0f }, // Md
        SliderMetrics{ 32.0f, 6.0f, 22.0f, 11.0f, 5.0f, 6.0f, 6.0f, 3.0f }, // Lg
        SliderMetrics{ 38.0f, 8.0f, 26.0f, 13.0f, 6.0f, 8.0f, 8.0f, 4.0f }  // Xl
    }};
}

void ApplyDensity(UiTheme& theme, Density d) {
    theme.density = d;
    if (d == Density::Compact) {
        theme.spacing = {0.0f, 2.0f, 4.0f, 8.0f, 12.0f, 16.0f};
        theme.radius  = {0.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};
        theme.metrics = {{
            ComponentMetrics{  0.0f, 13.0f, 11.0f, 10.0f,  0.0f,  0.0f, 0.0f,  0.0f }, // None
            ComponentMetrics{ 20.0f, 10.0f,  9.0f,  8.0f,  6.0f,  2.0f, 2.0f, 10.0f }, // Xs
            ComponentMetrics{ 26.0f, 12.0f, 10.0f,  9.0f, 10.0f,  4.0f, 3.0f, 13.0f }, // Sm
            ComponentMetrics{ 32.0f, 13.0f, 11.0f, 10.0f, 12.0f,  5.0f, 4.0f, 15.0f }, // Md
            ComponentMetrics{ 40.0f, 15.0f, 13.0f, 11.0f, 16.0f,  8.0f, 6.0f, 18.0f }, // Lg
            ComponentMetrics{ 48.0f, 18.0f, 15.0f, 13.0f, 20.0f, 10.0f, 8.0f, 22.0f }  // Xl
        }};
        theme.carousels = {{
            CarouselMetrics{ 24.0f, 13.0f, 4.0f, 3.0f, 5.0f, 32.0f,  95.0f }, // None
            CarouselMetrics{ 16.0f,  9.0f, 2.0f, 2.0f, 3.0f, 21.0f,  50.0f }, // Xs
            CarouselMetrics{ 20.0f, 11.0f, 3.0f, 2.0f, 4.0f, 26.0f,  70.0f }, // Sm
            CarouselMetrics{ 24.0f, 13.0f, 4.0f, 3.0f, 5.0f, 32.0f,  95.0f }, // Md
            CarouselMetrics{ 30.0f, 15.0f, 6.0f, 4.0f, 6.0f, 40.0f, 120.0f }, // Lg
            CarouselMetrics{ 36.0f, 18.0f, 8.0f, 5.0f, 8.0f, 49.0f, 150.0f }  // Xl
        }};
        theme.sliders = {{
            SliderMetrics{ 22.0f, 4.0f, 16.0f, 8.0f, 3.5f, 4.0f, 4.0f, 2.0f }, // None
            SliderMetrics{ 16.0f, 2.5f, 10.0f, 5.0f, 2.5f, 2.0f, 2.0f, 2.0f }, // Xs
            SliderMetrics{ 20.0f, 3.0f, 12.0f, 6.0f, 3.0f, 3.0f, 3.0f, 2.0f }, // Sm
            SliderMetrics{ 22.0f, 4.0f, 16.0f, 8.0f, 3.5f, 4.0f, 4.0f, 2.0f }, // Md
            SliderMetrics{ 28.0f, 5.0f, 20.0f, 10.0f, 4.0f, 5.0f, 5.0f, 3.0f }, // Lg
            SliderMetrics{ 34.0f, 6.0f, 24.0f, 12.0f, 5.0f, 6.0f, 6.0f, 3.0f }  // Xl
        }};
    } else if (d == Density::Touch) {
        theme.spacing = {0.0f, 6.0f, 12.0f, 16.0f, 24.0f, 32.0f};
        theme.radius  = {0.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f};
        theme.metrics = {{
            ComponentMetrics{  0.0f, 16.0f, 14.0f, 12.0f,  0.0f,  0.0f,  0.0f,  0.0f }, // None
            ComponentMetrics{ 28.0f, 12.0f, 10.0f,  9.0f, 10.0f,  4.0f,  4.0f, 14.0f }, // Xs
            ComponentMetrics{ 36.0f, 14.0f, 12.0f, 11.0f, 14.0f,  6.0f,  6.0f, 16.0f }, // Sm
            ComponentMetrics{ 48.0f, 16.0f, 14.0f, 12.0f, 20.0f, 10.0f,  8.0f, 22.0f }, // Md
            ComponentMetrics{ 56.0f, 18.0f, 16.0f, 14.0f, 24.0f, 12.0f, 10.0f, 26.0f }, // Lg
            ComponentMetrics{ 64.0f, 22.0f, 18.0f, 16.0f, 28.0f, 14.0f, 12.0f, 30.0f }  // Xl
        }};
        theme.carousels = {{
            CarouselMetrics{ 36.0f, 18.0f,  8.0f, 5.0f,  8.0f, 49.0f, 130.0f }, // None
            CarouselMetrics{ 22.0f, 12.0f,  4.0f, 3.0f,  5.0f, 30.0f,  70.0f }, // Xs
            CarouselMetrics{ 28.0f, 15.0f,  6.0f, 4.0f,  6.0f, 38.0f,  90.0f }, // Sm
            CarouselMetrics{ 36.0f, 18.0f,  8.0f, 5.0f,  8.0f, 49.0f, 130.0f }, // Md
            CarouselMetrics{ 42.0f, 22.0f, 10.0f, 6.0f, 10.0f, 58.0f, 160.0f }, // Lg
            CarouselMetrics{ 48.0f, 26.0f, 12.0f, 8.0f, 12.0f, 68.0f, 200.0f }  // Xl
        }};
        theme.sliders = {{
            SliderMetrics{ 36.0f, 6.0f, 26.0f, 13.0f, 6.0f, 6.0f, 6.0f, 4.0f }, // None
            SliderMetrics{ 24.0f, 4.0f, 16.0f,  8.0f, 4.0f, 4.0f, 4.0f, 3.0f }, // Xs
            SliderMetrics{ 30.0f, 5.0f, 20.0f, 10.0f, 5.0f, 5.0f, 5.0f, 3.0f }, // Sm
            SliderMetrics{ 36.0f, 6.0f, 26.0f, 13.0f, 6.0f, 6.0f, 6.0f, 4.0f }, // Md
            SliderMetrics{ 44.0f, 8.0f, 32.0f, 16.0f, 7.0f, 8.0f, 8.0f, 4.0f }, // Lg
            SliderMetrics{ 52.0f, 10.0f, 38.0f, 19.0f, 8.0f, 10.0f, 10.0f, 5.0f } // Xl
        }};
    } else { // Density::Standard
        theme.spacing = {0.0f, 4.0f, 8.0f, 12.0f, 16.0f, 24.0f};
        theme.radius  = {0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 12.0f};
        theme.metrics = {{
            ComponentMetrics{  0.0f, 14.0f, 12.0f, 11.0f,  0.0f,  0.0f,  0.0f,  0.0f }, // None
            ComponentMetrics{ 24.0f, 11.0f, 10.0f,  9.0f,  8.0f,  3.0f,  3.0f, 12.0f }, // Xs
            ComponentMetrics{ 32.0f, 13.0f, 11.0f, 10.0f, 12.0f,  5.0f,  4.0f, 15.0f }, // Sm
            ComponentMetrics{ 40.0f, 14.0f, 12.0f, 11.0f, 16.0f,  8.0f,  6.0f, 18.0f }, // Md
            ComponentMetrics{ 48.0f, 16.0f, 14.0f, 12.0f, 20.0f, 10.0f,  8.0f, 22.0f }, // Lg
            ComponentMetrics{ 56.0f, 18.0f, 16.0f, 14.0f, 24.0f, 12.0f, 10.0f, 26.0f }  // Xl
        }};
        theme.carousels = {{
            CarouselMetrics{ 26.0f, 14.0f,  6.0f, 4.0f,  6.0f, 36.0f, 110.0f }, // None
            CarouselMetrics{ 18.0f, 10.0f,  3.0f, 2.0f,  4.0f, 24.0f,  60.0f }, // Xs
            CarouselMetrics{ 22.0f, 12.0f,  4.0f, 3.0f,  5.0f, 30.0f,  80.0f }, // Sm
            CarouselMetrics{ 26.0f, 14.0f,  6.0f, 4.0f,  6.0f, 36.0f, 110.0f }, // Md
            CarouselMetrics{ 32.0f, 16.0f,  8.0f, 5.0f,  8.0f, 45.0f, 140.0f }, // Lg
            CarouselMetrics{ 40.0f, 20.0f, 10.0f, 6.0f, 10.0f, 56.0f, 180.0f }  // Xl
        }};
        theme.sliders = {{
            SliderMetrics{ 26.0f, 5.0f, 18.0f, 9.0f, 4.0f, 5.0f, 5.0f, 3.0f }, // None
            SliderMetrics{ 18.0f, 3.0f, 12.0f, 6.0f, 3.0f, 3.0f, 3.0f, 2.0f }, // Xs
            SliderMetrics{ 22.0f, 4.0f, 14.0f, 7.0f, 3.5f, 4.0f, 4.0f, 2.0f }, // Sm
            SliderMetrics{ 26.0f, 5.0f, 18.0f, 9.0f, 4.0f, 5.0f, 5.0f, 3.0f }, // Md
            SliderMetrics{ 32.0f, 6.0f, 22.0f, 11.0f, 5.0f, 6.0f, 6.0f, 3.0f }, // Lg
            SliderMetrics{ 38.0f, 8.0f, 26.0f, 13.0f, 6.0f, 8.0f, 8.0f, 4.0f }  // Xl
        }};
    }
}

} // namespace

void UiTheme::SetDensity(Density d) noexcept {
    ApplyDensity(*this, d);
}

UiTheme UiTheme::Dark(Density d) noexcept {
    UiTheme theme;
    theme.mode = ThemeMode::Dark;
    PopulatePalette(theme, ThemeMode::Dark);
    PopulateVariants(theme, ThemeMode::Dark);
    PopulateTypography(theme);
    PopulateScrollbars(theme);
    PopulateCarousels(theme);
    PopulateComponentMetrics(theme);
    ApplyDensity(theme, d);
    return theme;
}

UiTheme UiTheme::Light(Density d) noexcept {
    UiTheme theme;
    theme.mode = ThemeMode::Light;
    PopulatePalette(theme, ThemeMode::Light);
    PopulateVariants(theme, ThemeMode::Light);
    PopulateTypography(theme);
    PopulateScrollbars(theme);
    PopulateCarousels(theme);
    PopulateComponentMetrics(theme);
    ApplyDensity(theme, d);
    return theme;
}

UiTheme UiTheme::Scaled(float factor) const noexcept {
    UiTheme copy = *this;
    if (factor <= 0.0f || std::abs(factor - 1.0f) < 0.0001f) {
        return copy;
    }
    copy.scale *= factor;
    for (auto& s : copy.spacing) s *= factor;
    for (auto& r : copy.radius) r *= factor;
    for (auto& m : copy.metrics) {
        m.defaultHeight *= factor;
        m.fontSize *= factor;
        m.companionFontSize *= factor;
        m.descFontSize *= factor;
        m.paddingX *= factor;
        m.paddingY *= factor;
        m.cornerRadius *= factor;
        m.iconSize *= factor;
        m.minWidth *= factor;
        m.defaultWidth *= factor;
    }
    for (auto& t : copy.typography) {
        t.fontSize *= factor;
        t.lineHeight *= factor;
    }
    for (auto& sb : copy.scrollbars) {
        sb.trackWidth *= factor;
        sb.minThumbLength *= factor;
        sb.cornerRadius *= factor;
        sb.scrollStep *= factor;
        sb.margin *= factor;
        sb.gutter *= factor;
    }
    for (auto& cm : copy.carousels) {
        cm.arrowButtonSize *= factor;
        cm.iconSize *= factor;
        cm.cornerRadius *= factor;
        cm.margin *= factor;
        cm.gap *= factor;
        cm.gutter *= factor;
        cm.scrollStep *= factor;
    }
    for (auto& sw : copy.switches) {
        sw.trackSize.x *= factor;
        sw.trackSize.y *= factor;
        sw.knobDiameter *= factor;
        sw.knobPadding *= factor;
        sw.gap *= factor;
        sw.descGap *= factor;
    }
    for (auto& cb : copy.checkboxes) {
        cb.boxSize *= factor;
        cb.cornerRadius *= factor;
        cb.gap *= factor;
        cb.descGap *= factor;
        cb.strokeWidth *= factor;
    }
    for (auto& in : copy.inputs) {
        in.defaultHeight *= factor;
        in.cornerRadius *= factor;
        in.paddingX *= factor;
        in.labelGap *= factor;
        in.descGap *= factor;
        in.strokeWidth *= factor;
        in.iconSize *= factor;
        in.defaultWidth *= factor;
        in.minWidth *= factor;
    }
    for (auto& sl : copy.selects) {
        sl.defaultHeight *= factor;
        sl.cornerRadius *= factor;
        sl.paddingX *= factor;
        sl.labelGap *= factor;
        sl.descGap *= factor;
        sl.strokeWidth *= factor;
        sl.iconSize *= factor;
        sl.itemHeight *= factor;
        sl.defaultWidth *= factor;
        sl.minWidth *= factor;
    }
    for (auto& hd : copy.headings) {
        hd.titleFontSize *= factor;
        hd.subtitleFontSize *= factor;
        hd.badgeFontSize *= factor;
        hd.iconSize *= factor;
        hd.gap *= factor;
        hd.badgeGap *= factor;
        hd.lineGap *= factor;
    }
    for (auto& sl : copy.sliders) {
        sl.defaultHeight *= factor;
        sl.trackHeight *= factor;
        sl.thumbSize *= factor;
        sl.cornerRadius *= factor;
        sl.tickSize *= factor;
        sl.labelGap *= factor;
        sl.tickGap *= factor;
        sl.descGap *= factor;
        sl.defaultWidth *= factor;
    }
    for (auto& v : copy.variants) {
        v.borderWidth *= factor;
    }
    return copy;
}

} // namespace guikit
