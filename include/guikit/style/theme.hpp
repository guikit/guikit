#pragma once

#include "guikit/style/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

class IStylePainter;

/// Complete styling theme for the GuiKit engine.
/// Flat, contiguous in memory, and fast to look up with zero dynamic allocation.
struct UiTheme {
    ThemeMode mode{ThemeMode::Dark};
    ThemePreset preset{ThemePreset::Dark};
    ShellStyle shellStyle{ShellStyle::Docked};
    ChromeStyle chromeStyle{ChromeStyle::Modern};
    FillStyle fillStyle{FillStyle::Solid};
    SyntaxPreset syntaxPreset{SyntaxPreset::Auto};
    keel::f32 lineHeightMultiplier{kDefaultLineHeightMultiplier};
    keel::f32 focusRingWidth{2.0f};
    keel::f32 disabledOpacity{0.50f};
    keel::f32 borderScale{1.0f};
    keel::f32 animationScale{1.0f};
    bool shadowsEnabled{true};

    // 0. Application shell framing tokens (resolved from shellStyle)
    ShellMetrics shell{};

    // 0b. Elevation shadow tokens per Elevation level (None, Sm, Md, Lg)
    keel::StaticArray<ShadowStyle, 4> shadows{};

    // 0c. 3D bevel color palette & declarative structural chrome matrix (zero hardcoded colors)
    BevelPalette bevel{};
    ChromeTokens chrome{};
    ScrollbarStyleTokens scrollbarStyle{};

    // 0d. Optional custom look-and-feel painter (nullptr = built-in DefaultStylePainter)
    const IStylePainter* stylePainter{nullptr};

    // 1. Semantic color palette
    ThemePalette palette{};

    // 1b. Syntax highlighting color palette for code editors & viewers
    SyntaxPalette syntax{};

    // 2. State colors for semantic variants (UiVariant::Default .. Muted, 9 variants)
    keel::StaticArray<VariantColors, 9> variants{};

    // 3. Component metrics per UiSize (None, Xs, Sm, Md, Lg, Xl, 6 sizes)
    keel::StaticArray<ComponentMetrics, 6> metrics{};

    // 4. Typography metrics per TextRole (DisplayLarge .. CodeSmall, 18 roles)
    keel::StaticArray<TypographyMetrics, 18> typography{};

    // 5. Scrollbar metrics per UiSize (6 sizes)
    keel::StaticArray<ScrollbarMetrics, 6> scrollbars{};

    // 5b. Carousel navigation metrics per UiSize (6 sizes)
    keel::StaticArray<CarouselMetrics, 6> carousels{};

    // 6. Spacing scale (gaps) per UiSize (6 sizes)
    keel::StaticArray<keel::f32, 6> spacing{};

    // 7. Corner radius scale per UiSize (6 sizes)
    keel::StaticArray<keel::f32, 6> radius{};

    // 8. Specific widget metrics per UiSize (6 sizes each), plus section HeadingMetrics per HeadingLevel (H1..H6)
    keel::StaticArray<SwitchMetrics, 6>     switches{};
    keel::StaticArray<CheckboxMetrics, 6>   checkboxes{};
    keel::StaticArray<InputFieldMetrics, 6> inputs{};
    keel::StaticArray<SelectMetrics, 6>     selects{};
    keel::StaticArray<HeadingMetrics, 6>    headings{};
    keel::StaticArray<SliderMetrics, 6>     sliders{};

    // --- Fast Inline Accessors ---

    [[nodiscard]] constexpr keel::f32 SizeFactor(UiSize size) const noexcept {
        return size.ResolveFactor(metrics[UiSize::Md.TableIndex()].defaultHeight);
    }

    [[nodiscard]] constexpr keel::f32 ScaleBySize(keel::f32 mdValue, UiSize size) const noexcept {
        if (size.IsPreset()) {
            const keel::f32 mdH = metrics[UiSize::Md.TableIndex()].defaultHeight;
            const keel::f32 curH = metrics[size.TableIndex()].defaultHeight;
            return (mdH > 0.0f && curH > 0.0f) ? (mdValue * (curH / mdH)) : mdValue;
        }
        return mdValue * SizeFactor(size);
    }

    [[nodiscard]] constexpr ComponentMetrics GetMetrics(UiSize size) const noexcept {
        if (size.IsPreset()) {
            return metrics[size.TableIndex()];
        }
        ComponentMetrics m = metrics[UiSize::Md.TableIndex()];
        const keel::f32 f = SizeFactor(size);
        m.defaultHeight *= f;
        m.fontSize *= f;
        m.companionFontSize *= f;
        m.descFontSize *= f;
        m.paddingX *= f;
        m.paddingY *= f;
        m.cornerRadius *= f;
        m.iconSize *= f;
        m.minWidth *= f;
        m.defaultWidth *= f;
        m.badgeFontSize *= f;
        m.itemGap *= f;
        m.badgeGap *= f;
        m.lineGap *= f;
        return m;
    }
    [[nodiscard]] constexpr ComponentMetrics& MutableMetrics(UiSize size) noexcept {
        return metrics[size.TableIndex()];
    }

    [[nodiscard]] constexpr const VariantColors& GetVariant(UiVariant variant) const noexcept {
        const keel::usize idx = keel::as<keel::usize>(variant);
        return (idx < variants.size()) ? variants[idx] : variants[keel::as<keel::usize>(UiVariant::Default)];
    }
    [[nodiscard]] constexpr VariantColors& GetVariant(UiVariant variant) noexcept {
        const keel::usize idx = keel::as<keel::usize>(variant);
        return (idx < variants.size()) ? variants[idx] : variants[keel::as<keel::usize>(UiVariant::Default)];
    }

    /// Returns the solid semantic hue color for `variant` even when `fillStyle` is `Soft` or `Outline`.
    [[nodiscard]] constexpr Color GetSemanticAccent(UiVariant variant) const noexcept {
        const VariantColors& vc = GetVariant(variant);
        if (fillStyle == FillStyle::Outline || fillStyle == FillStyle::Soft) {
            if (variant == UiVariant::Primary) return palette.accent;
            if (vc.border.a > 0u) return vc.border.WithAlpha(255u);
            return vc.text.WithAlpha(255u);
        }
        return vc.bg;
    }

    /// Resolves a solid semantic accent color for indicators, arcs, meters, and slider tracks
    /// even when `fillStyle` is `Soft` or `Outline` (where `VariantColors::bg` is tinted/transparent).
    [[nodiscard]] constexpr Color ResolveSemanticAccent(
        const StateColor& sc,
        UiVariant defaultVariant = UiVariant::Primary,
        bool isHovered = false,
        bool isActive = false,
        bool isDisabled = false
    ) const noexcept {
        auto applyStates = [this, isHovered, isActive, isDisabled](Color base, keel::u8 alphaScale = 255u) constexpr noexcept -> Color {
            Color c = base.ScaleAlpha(alphaScale);
            if (isDisabled) {
                return c.WithAlphaF(disabledOpacity);
            }
            if (isActive) {
                return (mode == ThemeMode::Dark) ? c.Lighten(0.10f) : c.Darken(0.10f);
            }
            if (isHovered) {
                return (mode == ThemeMode::Dark) ? c.Lighten(0.06f) : c.Darken(0.06f);
            }
            return c;
        };
        if (!sc.HasAny()) {
            return applyStates(GetSemanticAccent(defaultVariant));
        }
        switch (sc.token) {
            case StateColorToken::Primary:   return applyStates(GetSemanticAccent(UiVariant::Primary), sc.alphaScale);
            case StateColorToken::Secondary: return applyStates(GetSemanticAccent(UiVariant::Secondary), sc.alphaScale);
            case StateColorToken::Success:   return applyStates(GetSemanticAccent(UiVariant::Success), sc.alphaScale);
            case StateColorToken::Warning:   return applyStates(GetSemanticAccent(UiVariant::Warning), sc.alphaScale);
            case StateColorToken::Danger:    return applyStates(GetSemanticAccent(UiVariant::Danger), sc.alphaScale);
            case StateColorToken::Info:      return applyStates(GetSemanticAccent(UiVariant::Info), sc.alphaScale);
            case StateColorToken::Ghost:     return applyStates(palette.borderHover, sc.alphaScale);
            case StateColorToken::Muted:     return applyStates(palette.textMuted, sc.alphaScale);
            default: break;
        }
        const Color base = GetSemanticAccent(defaultVariant);
        const Color resolved = ResolveColor(
            sc,
            isHovered && !isDisabled,
            isActive && !isDisabled,
            isDisabled,
            base,
            applyStates(base),
            applyStates(base),
            base.WithAlphaF(disabledOpacity)
        );
        if (resolved.a == 0u && sc.normal.has_value() && sc.normal->a == 0u) {
            return applyStates(palette.borderHover, sc.alphaScale);
        }
        return resolved;
    }

    /// Returns theme-appropriate translucent tint alpha for soft/ghost semantic fills (slightly higher on dark surfaces).
    [[nodiscard]] constexpr keel::f32 SoftTintAlpha(bool hovered = false, bool active = false) const noexcept {
        const bool isDark = (mode == ThemeMode::Dark);
        if (active)  return isDark ? 0.28f : 0.20f;
        if (hovered) return isDark ? 0.22f : 0.14f;
        return isDark ? 0.18f : 0.12f;
    }

    /// Returns a readable semantic foreground color (WCAG AA >= 4.5:1) against `surfaceRef`.
    [[nodiscard]] Color EnsureReadableInk(Color seed, Color surfaceRef) const noexcept {
        const bool isDark = (mode == ThemeMode::Dark);
        Color c = seed.WithAlpha(255u);
        for (keel::i32 step = 0; step < 5; ++step) {
            if (Color::ContrastRatio(c, surfaceRef) >= 4.5f) return c;
            c = isDark ? c.Lighten(0.10f) : c.Darken(0.10f);
        }
        return Color::BestTextColor(surfaceRef, Color::Hex(0x0F172AFF), Color::Hex(0xFFFFFFFF));
    }

    [[nodiscard]] constexpr Color GetTextColor(UiVariant variant) const noexcept {
        switch (variant) {
            case UiVariant::Default:   return palette.textPrimary;
            case UiVariant::Secondary:
            case UiVariant::Muted:     return palette.textSecondary;
            case UiVariant::Ghost:     return palette.textMuted;
            default:                   return GetSemanticAccent(variant);
        }
    }

    [[nodiscard]] constexpr const TypographyMetrics& GetTypography(TextRole role) const noexcept {
        const keel::usize idx = keel::as<keel::usize>(role);
        return (idx < typography.size()) ? typography[idx] : typography[keel::as<keel::usize>(TextRole::BodyMedium)];
    }
    [[nodiscard]] constexpr TypographyMetrics& GetTypography(TextRole role) noexcept {
        const keel::usize idx = keel::as<keel::usize>(role);
        return (idx < typography.size()) ? typography[idx] : typography[keel::as<keel::usize>(TextRole::BodyMedium)];
    }

    /// Resolves font size for a semantic TextRole at a given widget UiSize.
    [[nodiscard]] constexpr keel::f32 ResolveFontSize(
        TextRole role,
        UiSize widgetSize = UiSize::Md
    ) const noexcept {
        if (widgetSize == UiSize::Md || widgetSize.IsNone()) {
            return GetTypography(role).fontSize;
        }
        const ComponentMetrics cm = GetMetrics(widgetSize);
        switch (role) {
            case TextRole::LabelLarge:
            case TextRole::BodyMedium:
                return cm.fontSize;
            case TextRole::LabelMedium:
            case TextRole::BodySmall:
                return cm.companionFontSize;
            case TextRole::LabelSmall:
                return cm.descFontSize;
            default:
                return GetTypography(role).fontSize * (widgetSize.IsCustom() ? SizeFactor(widgetSize) : 1.0f);
        }
    }

    [[nodiscard]] constexpr keel::f32 GetFontSize(
        TextRole role,
        UiSize widgetSize = UiSize::Md
    ) const noexcept {
        return ResolveFontSize(role, widgetSize);
    }

    /// Resolves font size for a UiText: checks explicit px, preset UiSize, or TextRole, falling back to defaultRole at widgetSize.
    [[nodiscard]] constexpr keel::f32 ResolveFontSize(
        const UiText& text,
        TextRole defaultRole,
        UiSize widgetSize = UiSize::Md
    ) const noexcept {
        if (text.size.px > 0.0f) {
            return text.size.px;
        }
        const UiSize effectiveSize = !text.size.preset.IsNone() ? text.size.preset : widgetSize;
        if (text.size.role.has_value()) {
            const keel::f32 f = effectiveSize.IsCustom() ? SizeFactor(effectiveSize) : 1.0f;
            return GetTypography(*text.size.role).fontSize * f;
        }
        return ResolveFontSize(defaultRole, effectiveSize);
    }

    /// Resolves font size for a UiText with an explicit fallback pixel size.
    [[nodiscard]] constexpr keel::f32 ResolveFontSize(
        const UiText& text,
        keel::f32 fallbackFontSize
    ) const noexcept {
        if (text.size.px > 0.0f) {
            return text.size.px;
        }
        if (text.size.role.has_value()) {
            return GetTypography(*text.size.role).fontSize;
        }
        if (text.size.preset != UiSize::None) {
            return GetMetrics(text.size.preset).companionFontSize;
        }
        return fallbackFontSize;
    }

    /// Applies `surfaceTint` (tonal mix onto this theme's opaque `surface` palette) and `alphaScale` (post-resolve opacity) to a resolved `StateColor`.
    [[nodiscard]] constexpr StateColor ApplyStateColorModifiers(
        StateColor res,
        keel::u8 surfaceTint,
        keel::u8 alphaScale
    ) const noexcept {
        if (!res.HasAny() || res.IsNone()) {
            return res;
        }
        if (surfaceTint < 255 && res.token != StateColorToken::Surface && res.token != StateColorToken::Ghost) {
            const Color opaqueSurface = palette.surface;
            const Color opaqueSurfaceHover = Color::Mix255(
                palette.surface,
                palette.surfaceHover.WithAlpha(255),
                palette.surfaceHover.a
            ).WithAlpha(palette.surface.a);
            const Color opaqueSurfaceActive = Color::Mix255(
                palette.surface,
                palette.surfaceActive.WithAlpha(255),
                palette.surfaceActive.a
            ).WithAlpha(palette.surface.a);

            auto tintOver = [](Color surf, Color accent, keel::u8 tint) constexpr noexcept -> Color {
                const keel::u32 effW = (keel::as<keel::u32>(tint) * keel::as<keel::u32>(accent.a) + 127u) / 255u;
                return Color::Mix255(surf, accent, keel::as<keel::u8>(effW)).WithAlpha(surf.a);
            };

            const Color baseNormal = res.normal.value_or(opaqueSurface);
            const Color baseHover = res.hover.value_or(StateColor::BrightenColor(baseNormal));
            const Color baseActive = res.active.value_or(StateColor::DarkenColor(baseNormal));

            res.normal = tintOver(opaqueSurface, baseNormal, surfaceTint);
            res.hover = tintOver(opaqueSurfaceHover, baseHover, surfaceTint);
            res.active = tintOver(opaqueSurfaceActive, baseActive, surfaceTint);
            res.disabled = res.normal->WithAlphaF(disabledOpacity);
        }
        if (alphaScale < 255) {
            if (res.normal.has_value())   res.normal   = res.normal->ScaleAlpha(alphaScale);
            if (res.hover.has_value())    res.hover    = res.hover->ScaleAlpha(alphaScale);
            if (res.active.has_value())   res.active   = res.active->ScaleAlpha(alphaScale);
            if (res.disabled.has_value()) res.disabled = res.disabled->ScaleAlpha(alphaScale);
        }
        res.surfaceTint = 255;
        res.alphaScale = 255;
        return res;
    }

    /// Resolves a semantic `StateColor` token against this theme's `variants` and `palette` (controls, surfaces, borders, badges),
    /// applying any `surfaceTint` (tonal surface mix) and `alphaScale` (post-resolve opacity multiplier).
    [[nodiscard]] constexpr StateColor ResolveStateColor(const StateColor& sc) const noexcept {
        auto fromVar = [this](UiVariant v, StateColorToken tok) constexpr noexcept -> StateColor {
            const VariantColors& vc = GetVariant(v);
            return StateColor{
                .normal   = vc.bg,
                .hover    = vc.bgHover,
                .active   = vc.bgActive,
                .disabled = vc.bg.WithAlphaF(disabledOpacity),
                .token    = tok
            };
        };
        StateColor res = sc;
        switch (sc.token) {
            case StateColorToken::Custom:        break;
            case StateColorToken::Primary:       res = fromVar(UiVariant::Primary, sc.token); break;
            case StateColorToken::Secondary:     res = fromVar(UiVariant::Secondary, sc.token); break;
            case StateColorToken::Success:       res = fromVar(UiVariant::Success, sc.token); break;
            case StateColorToken::Warning:       res = fromVar(UiVariant::Warning, sc.token); break;
            case StateColorToken::Danger:        res = fromVar(UiVariant::Danger, sc.token); break;
            case StateColorToken::Info:          res = fromVar(UiVariant::Info, sc.token); break;
            case StateColorToken::Ghost: {
                const VariantColors& vc = GetVariant(UiVariant::Ghost);
                res = StateColor{
                    .normal   = vc.bg,
                    .hover    = vc.bgHover,
                    .active   = vc.bgActive,
                    .disabled = Color::Transparent(),
                    .token    = sc.token
                };
                break;
            }
            case StateColorToken::Muted:
            case StateColorToken::TextMuted:
                res = StateColor{
                    .normal   = palette.textMuted,
                    .hover    = palette.textSecondary,
                    .active   = palette.textMuted,
                    .disabled = palette.textDisabled,
                    .token    = sc.token
                };
                break;
            case StateColorToken::TextPrimary:
                res = StateColor{
                    .normal   = palette.textPrimary,
                    .hover    = palette.textPrimary,
                    .active   = palette.textPrimary,
                    .disabled = palette.textDisabled,
                    .token    = sc.token
                };
                break;
            case StateColorToken::TextSecondary:
                res = StateColor{
                    .normal   = palette.textSecondary,
                    .hover    = palette.textPrimary,
                    .active   = palette.textSecondary,
                    .disabled = palette.textDisabled,
                    .token    = sc.token
                };
                break;
            case StateColorToken::Surface: {
                const Color opaqueHover = Color::Mix255(
                    palette.surface,
                    palette.surfaceHover.WithAlpha(255),
                    palette.surfaceHover.a
                ).WithAlpha(palette.surface.a);
                const Color opaqueActive = Color::Mix255(
                    palette.surface,
                    palette.surfaceActive.WithAlpha(255),
                    palette.surfaceActive.a
                ).WithAlpha(palette.surface.a);
                res = StateColor{
                    .normal   = palette.surface,
                    .hover    = opaqueHover,
                    .active   = opaqueActive,
                    .disabled = palette.surface.WithAlphaF(disabledOpacity),
                    .token    = sc.token
                };
                break;
            }
        }
        StateColor out = ApplyStateColorModifiers(res, sc.surfaceTint, sc.alphaScale);
        out.gradientAngleDeg = sc.gradientAngleDeg;
        out.gradientFlags = sc.gradientFlags;
        return out;
    }

    /// Resolves a semantic `StateColor` token specifically for foreground text against this theme's Light/Dark palette.
    [[nodiscard]] constexpr StateColor ResolveTextStateColor(const StateColor& sc) const noexcept {
        StateColor res = sc;
        switch (sc.token) {
            case StateColorToken::Custom:
                break;
            case StateColorToken::TextPrimary:
                res = StateColor{
                    .normal   = palette.textPrimary,
                    .hover    = palette.textPrimary,
                    .active   = palette.textPrimary,
                    .disabled = palette.textDisabled,
                    .token    = sc.token
                };
                break;
            case StateColorToken::Secondary:
            case StateColorToken::Muted:
            case StateColorToken::TextSecondary:
                res = StateColor{
                    .normal   = palette.textSecondary,
                    .hover    = palette.textPrimary,
                    .active   = palette.textSecondary,
                    .disabled = palette.textDisabled,
                    .token    = sc.token
                };
                break;
            case StateColorToken::Ghost:
            case StateColorToken::TextMuted:
                res = StateColor{
                    .normal   = palette.textMuted,
                    .hover    = palette.textSecondary,
                    .active   = palette.textMuted,
                    .disabled = palette.textDisabled,
                    .token    = sc.token
                };
                break;
            default:
                return ResolveStateColor(sc);
        }
        StateColor out = ApplyStateColorModifiers(res, sc.surfaceTint, sc.alphaScale);
        out.gradientAngleDeg = sc.gradientAngleDeg;
        out.gradientFlags = sc.gradientFlags;
        return out;
    }

    /// Resolves a control/surface/border `StateColor` across interaction states using theme-aware semantic tokens.
    [[nodiscard]] constexpr Color ResolveColor(
        const StateColor& sc,
        bool isHovered,
        bool isActive,
        bool isDisabled,
        Color fallbackNormal,
        Color fallbackHover,
        Color fallbackActive,
        Color fallbackDisabled
    ) const noexcept {
        return ResolveStateColor(sc).Resolve(
            isHovered,
            isActive,
            isDisabled,
            fallbackNormal,
            fallbackHover,
            fallbackActive,
            fallbackDisabled
        );
    }

    [[nodiscard]] constexpr Color ResolveColor(
        const StateColor& sc,
        bool isHovered,
        bool isActive,
        bool isDisabled,
        Color fallback
    ) const noexcept {
        return ResolveStateColor(sc).Resolve(isHovered, isActive, isDisabled, fallback);
    }

    /// Derives a palette-harmonized 2-stop gradient from a seed accent color using HSL hue shift
    /// while guaranteeing WCAG AA (>= 4.5:1) text contrast across both stops.
    [[nodiscard]] inline Gradient DeriveAccentGradient(Color seed, keel::f32 angleDeg = 135.0f) const noexcept {
        const Color darkInk = Color::Hex(0x0F172AFF);
        const Color lightInk = Color::Hex(0xFFFFFFFF);
        const bool prefersWhiteInk = Color::ContrastRatio(lightInk, seed) >= Color::ContrastRatio(darkInk, seed);
        const Color targetInk = prefersWhiteInk ? lightInk : darkInk;
        auto ensureStopContrast = [targetInk, prefersWhiteInk](Color c) noexcept -> Color {
            for (keel::i32 i = 0; i < 5 && Color::ContrastRatio(targetInk, c) < 4.5f; ++i) {
                c = prefersWhiteInk ? c.ShiftLightness(-0.04f) : c.ShiftLightness(0.04f);
            }
            return c;
        };
        const Color c0 = ensureStopContrast(seed.ShiftHue(-10.0f, 0.05f, prefersWhiteInk ? 0.00f : 0.05f));
        const Color c1 = ensureStopContrast(seed.ShiftHue(24.0f, 0.07f, prefersWhiteInk ? -0.05f : 0.01f));
        return Gradient::Linear(c0, c1, angleDeg);
    }

    /// Resolves a per-widget `StateColor` gradient (explicit or semantic auto-gradient) across interaction states.
    /// Returns `kNullOpt` when `sc.HasGradient()` is false.
    [[nodiscard]] inline keel::Optional<Gradient> ResolveGradient(
        const StateColor& sc,
        bool isHovered,
        bool isActive,
        bool isDisabled
    ) const noexcept {
        if (!sc.HasGradient()) {
            return keel::kNullOpt;
        }
        const keel::f32 angle = keel::as<keel::f32>(sc.gradientAngleDeg);
        auto mixTint = [this, &sc](Color c) noexcept -> Color {
            if (sc.surfaceTint < 255u) {
                const keel::f32 t = keel::as<keel::f32>(sc.surfaceTint) / 255.0f;
                c = Color::Lerp(palette.surface, c, t).WithAlpha(c.a);
            }
            return c.ScaleAlpha(sc.alphaScale);
        };

        Gradient g{};
        if (sc.IsAutoGradient() || !sc.HasExplicitGradientStops()) {
            Color seed = palette.accent;
            if (sc.token == StateColorToken::Primary)        seed = GetSemanticAccent(UiVariant::Primary);
            else if (sc.token == StateColorToken::Secondary) seed = GetSemanticAccent(UiVariant::Secondary);
            else if (sc.token == StateColorToken::Success)   seed = GetSemanticAccent(UiVariant::Success);
            else if (sc.token == StateColorToken::Warning)   seed = GetSemanticAccent(UiVariant::Warning);
            else if (sc.token == StateColorToken::Danger)    seed = GetSemanticAccent(UiVariant::Danger);
            else if (sc.token == StateColorToken::Info)      seed = GetSemanticAccent(UiVariant::Info);
            else if (sc.token == StateColorToken::Surface)   seed = palette.surface;
            else if (sc.normal.has_value())                  seed = *sc.normal;

            g = DeriveAccentGradient(seed, angle);
            g.stops[0].color = mixTint(g.stops[0].color);
            g.stops[1].color = mixTint(g.stops[1].color);
        } else {
            const Color c0 = mixTint(sc.GradientStartColor());
            const Color cEnd = mixTint(sc.GradientEndColor());
            const keel::Optional<Color> midOpt = sc.GradientMidColor();
            if (sc.IsRadialGradient()) {
                g = midOpt.has_value()
                    ? Gradient::Radial3(c0, mixTint(*midOpt), cEnd)
                    : Gradient::Radial(c0, cEnd);
            } else {
                g = midOpt.has_value()
                    ? Gradient::Linear3(c0, mixTint(*midOpt), cEnd, angle)
                    : Gradient::Linear(c0, cEnd, angle);
            }
        }
        return g.AdjustedForState(isHovered, isActive, isDisabled, disabledOpacity);
    }

    /// Resolves text color for a UiText: uses explicit StateColor if set, or defaultVariant across interaction states.
    [[nodiscard]] constexpr Color ResolveTextColor(
        const UiText& text,
        UiVariant defaultVariant,
        bool disabled = false,
        bool hovered = false,
        bool active = false
    ) const noexcept {
        const Color fallback = GetTextColor(defaultVariant);
        return ResolveTextStateColor(text.color).Resolve(hovered, active, disabled, fallback);
    }

    /// Resolves text color for a UiText with an explicit fallback Color across interaction states.
    [[nodiscard]] constexpr Color ResolveTextColor(
        const UiText& text,
        Color fallbackColor,
        bool disabled = false,
        bool hovered = false,
        bool active = false
    ) const noexcept {
        return ResolveTextStateColor(text.color).Resolve(hovered, active, disabled, fallbackColor);
    }

    [[nodiscard]] constexpr ScrollbarMetrics GetScrollbar(UiSize size) const noexcept {
        if (size.IsPreset()) {
            return scrollbars[size.TableIndex()];
        }
        ScrollbarMetrics m = scrollbars[UiSize::Md.TableIndex()];
        const keel::f32 f = SizeFactor(size);
        m.trackWidth *= f;
        m.minThumbLength *= f;
        m.cornerRadius *= f;
        m.scrollStep *= f;
        m.margin *= f;
        m.gutter *= f;
        return m;
    }
    [[nodiscard]] constexpr ScrollbarMetrics& MutableScrollbar(UiSize size) noexcept {
        return scrollbars[size.TableIndex()];
    }

    [[nodiscard]] constexpr CarouselMetrics GetCarousel(UiSize size) const noexcept {
        if (size.IsPreset()) {
            return carousels[size.TableIndex()];
        }
        CarouselMetrics m = carousels[UiSize::Md.TableIndex()];
        const keel::f32 f = SizeFactor(size);
        m.arrowButtonSize *= f;
        m.iconSize *= f;
        m.cornerRadius *= f;
        m.margin *= f;
        m.gap *= f;
        m.gutter *= f;
        m.scrollStep *= f;
        return m;
    }
    [[nodiscard]] constexpr CarouselMetrics& MutableCarousel(UiSize size) noexcept {
        return carousels[size.TableIndex()];
    }

    [[nodiscard]] constexpr keel::f32 GetGap(UiSize size) const noexcept {
        if (size.IsPreset()) {
            return spacing[size.TableIndex()];
        }
        return spacing[UiSize::Md.TableIndex()] * SizeFactor(size);
    }

    /// Resolves an optional explicit gap or falls back to the theme spacing token for fallbackSize, scaled by container density.
    [[nodiscard]] constexpr keel::f32 ResolveGap(
        keel::Optional<keel::f32> explicitGap,
        UiSize fallbackSize = UiSize::Md,
        keel::f32 density = 1.0f
    ) const noexcept {
        const keel::f32 base = explicitGap.has_value() ? *explicitGap : GetGap(fallbackSize);
        return base * density;
    }

    [[nodiscard]] constexpr keel::f32 GetRadius(UiSize size) const noexcept {
        if (size.IsPreset()) {
            return radius[size.TableIndex()];
        }
        return radius[UiSize::Md.TableIndex()] * SizeFactor(size);
    }

    /// Resolves an optional explicit corner radius or falls back to the theme radius token for fallbackSize.
    [[nodiscard]] constexpr keel::f32 ResolveRadius(keel::Optional<keel::f32> explicitRadius, UiSize fallbackSize = UiSize::Md) const noexcept {
        return explicitRadius.has_value() ? *explicitRadius : GetRadius(fallbackSize);
    }

    [[nodiscard]] constexpr Padding GetPadding(UiSize size) const noexcept {
        if (size.IsNone()) return Padding{};
        const keel::f32 val = (size == UiSize::Xs) ? spacing[UiSize::Sm.TableIndex()] :
                          (size == UiSize::Sm) ? spacing[UiSize::Md.TableIndex()] :
                          (size == UiSize::Md) ? spacing[UiSize::Lg.TableIndex()] :
                          (size == UiSize::Lg) ? spacing[UiSize::Xl.TableIndex()] :
                          (size == UiSize::Xl) ? (spacing[UiSize::Xl.TableIndex()] * 1.5f) :
                          (spacing[UiSize::Lg.TableIndex()] * SizeFactor(size));
        return Padding::All(val);
    }

    /// Resolves an optional padding (design token or explicit insets), scaled by container density.
    [[nodiscard]] constexpr Padding ResolvePadding(
        keel::Optional<Padding> p,
        UiSize fallbackSize = UiSize::None,
        keel::f32 density = 1.0f
    ) const noexcept {
        Padding result{};
        if (p.has_value()) {
            result = p->IsToken() ? GetPadding(p->GetToken()) : *p;
        } else if (!fallbackSize.IsNone()) {
            result = GetPadding(fallbackSize);
        }
        return result.ScaledBy(density);
    }

    [[nodiscard]] constexpr SwitchMetrics GetSwitch(UiSize size) const noexcept {
        if (size.IsPreset()) {
            return switches[size.TableIndex()];
        }
        SwitchMetrics m = switches[UiSize::Md.TableIndex()];
        const keel::f32 f = SizeFactor(size);
        m.trackSize.x *= f;
        m.trackSize.y *= f;
        m.knobDiameter *= f;
        m.knobPadding *= f;
        m.gap *= f;
        m.labelGap *= f;
        m.descGap *= f;
        return m;
    }
    [[nodiscard]] constexpr SwitchMetrics& MutableSwitch(UiSize size) noexcept {
        return switches[size.TableIndex()];
    }

    [[nodiscard]] constexpr CheckboxMetrics GetCheckbox(UiSize size) const noexcept {
        if (size.IsPreset()) {
            return checkboxes[size.TableIndex()];
        }
        CheckboxMetrics m = checkboxes[UiSize::Md.TableIndex()];
        const keel::f32 f = SizeFactor(size);
        m.boxSize *= f;
        m.cornerRadius *= f;
        m.gap *= f;
        m.labelGap *= f;
        m.descGap *= f;
        m.strokeWidth *= f;
        return m;
    }
    [[nodiscard]] constexpr CheckboxMetrics& MutableCheckbox(UiSize size) noexcept {
        return checkboxes[size.TableIndex()];
    }

    [[nodiscard]] constexpr InputFieldMetrics GetInputField(UiSize size) const noexcept {
        if (size.IsPreset()) {
            return inputs[size.TableIndex()];
        }
        InputFieldMetrics m = inputs[UiSize::Md.TableIndex()];
        const keel::f32 f = SizeFactor(size);
        m.defaultHeight *= f;
        m.cornerRadius *= f;
        m.paddingX *= f;
        m.labelGap *= f;
        m.descGap *= f;
        m.strokeWidth *= f;
        m.iconSize *= f;
        m.defaultWidth *= f;
        m.minWidth *= f;
        return m;
    }
    [[nodiscard]] constexpr InputFieldMetrics& MutableInputField(UiSize size) noexcept {
        return inputs[size.TableIndex()];
    }

    [[nodiscard]] constexpr SelectMetrics GetSelect(UiSize size) const noexcept {
        if (size.IsPreset()) {
            return selects[size.TableIndex()];
        }
        SelectMetrics m = selects[UiSize::Md.TableIndex()];
        const keel::f32 f = SizeFactor(size);
        m.defaultHeight *= f;
        m.cornerRadius *= f;
        m.paddingX *= f;
        m.labelGap *= f;
        m.descGap *= f;
        m.strokeWidth *= f;
        m.iconSize *= f;
        m.defaultWidth *= f;
        m.minWidth *= f;
        m.itemHeight *= f;
        return m;
    }
    [[nodiscard]] constexpr SelectMetrics& MutableSelect(UiSize size) noexcept {
        return selects[size.TableIndex()];
    }

    [[nodiscard]] constexpr const HeadingMetrics& GetHeading(HeadingLevel level) const noexcept {
        const keel::usize idx = keel::as<keel::usize>(level);
        return (idx < headings.size()) ? headings[idx] : headings[keel::as<keel::usize>(HeadingLevel::H3)];
    }
    [[nodiscard]] constexpr HeadingMetrics& GetHeading(HeadingLevel level) noexcept {
        const keel::usize idx = keel::as<keel::usize>(level);
        return (idx < headings.size()) ? headings[idx] : headings[keel::as<keel::usize>(HeadingLevel::H3)];
    }

    [[nodiscard]] constexpr SliderMetrics GetSlider(UiSize size) const noexcept {
        if (size.IsPreset()) {
            return sliders[size.TableIndex()];
        }
        SliderMetrics m = sliders[UiSize::Md.TableIndex()];
        const keel::f32 f = SizeFactor(size);
        m.defaultHeight *= f;
        m.trackHeight *= f;
        m.thumbSize *= f;
        m.cornerRadius *= f;
        m.tickSize *= f;
        m.labelGap *= f;
        m.tickGap *= f;
        m.descGap *= f;
        m.defaultWidth *= f;
        return m;
    }
    [[nodiscard]] constexpr SliderMetrics& MutableSlider(UiSize size) noexcept {
        return sliders[size.TableIndex()];
    }

    struct SpacingProxy {
        keel::f32& none;
        keel::f32& xs;
        keel::f32& sm;
        keel::f32& md;
        keel::f32& lg;
        keel::f32& xl;

        explicit SpacingProxy(keel::StaticArray<keel::f32, 6>& a) noexcept
            : none(a[0]), xs(a[1]), sm(a[2]), md(a[3]), lg(a[4]), xl(a[5]) {}
    };

    struct ConstSpacingProxy {
        const keel::f32 none;
        const keel::f32 xs;
        const keel::f32 sm;
        const keel::f32 md;
        const keel::f32 lg;
        const keel::f32 xl;

        explicit ConstSpacingProxy(const keel::StaticArray<keel::f32, 6>& a) noexcept
            : none(a[0]), xs(a[1]), sm(a[2]), md(a[3]), lg(a[4]), xl(a[5]) {}
    };

    [[nodiscard]] SpacingProxy Spacing() noexcept { return SpacingProxy(spacing); }
    [[nodiscard]] ConstSpacingProxy Spacing() const noexcept { return ConstSpacingProxy(spacing); }

    [[nodiscard]] constexpr ShellStyle GetShellStyle() const noexcept { return shellStyle; }
    [[nodiscard]] constexpr ComponentMetrics GetComponentMetrics(UiSize size) const noexcept { return GetMetrics(size); }
    [[nodiscard]] constexpr keel::f32 GetLineHeight(keel::f32 fontSize) const noexcept { return fontSize * lineHeightMultiplier; }

    /// Returns resolved elevation shadow parameters (empty/invisible when `shadowsEnabled == false` or `Elevation::None`).
    [[nodiscard]] constexpr ShadowStyle GetShadow(
        Elevation elev,
        keel::f32 geomScale = 1.0f
    ) const noexcept {
        if (!shadowsEnabled || elev == Elevation::None) {
            return ShadowStyle{};
        }
        const keel::usize idx = keel::as<keel::usize>(elev);
        const ShadowStyle s = (idx < shadows.size()) ? shadows[idx] : shadows[2];
        return (geomScale == 1.0f) ? s : s.Scaled(geomScale);
    }

    /// Returns the active look-and-feel style painter (custom `stylePainter` if set, otherwise `GetDefaultStylePainter()`).
    [[nodiscard]] const IStylePainter& Painter() const noexcept;

    /// Replaces the 4-tone 3D bevel palette (normally derived automatically from `ThemePalette`).
    void SetBevelPalette(const BevelPalette& b) noexcept { bevel = b; }

    /// Replaces individual structural chrome tokens without switching the active palette.
    void SetChromeTokens(const ChromeTokens& tokens) noexcept;

    /// Replaces the declarative `ScrollbarStyleTokens` for fine-grained scrollbar customization.
    void SetScrollbarStyle(const ScrollbarStyleTokens& style) noexcept;

    /// Scales scrollbar track widths and gutters proportionally around the specified `UiSize::Md` width in dp.
    void SetScrollbarWidth(keel::f32 mdWidth) noexcept;

    /// Installs a custom non-owning `IStylePainter` for this theme (pass `nullptr` to restore built-in default).
    void SetStylePainter(const IStylePainter* painter) noexcept { stylePainter = painter; }

    // --- Standard & Curated Theme Factories ---
    [[nodiscard]] static UiTheme Dark() noexcept;
    [[nodiscard]] static UiTheme Light() noexcept;
    [[nodiscard]] static UiTheme Nord() noexcept;
    [[nodiscard]] static UiTheme Dracula() noexcept;
    [[nodiscard]] static UiTheme CatppuccinMocha() noexcept;
    [[nodiscard]] static UiTheme CatppuccinLatte() noexcept;
    [[nodiscard]] static UiTheme Industrial() noexcept;
    [[nodiscard]] static UiTheme IndustrialLight() noexcept;
    [[nodiscard]] static UiTheme HighContrastDark() noexcept;
    [[nodiscard]] static UiTheme HighContrastLight() noexcept;
    [[nodiscard]] static UiTheme Windows98() noexcept;
    [[nodiscard]] static UiTheme Pulse() noexcept;
    [[nodiscard]] static UiTheme PulseLight() noexcept;
    [[nodiscard]] static UiTheme Standard() noexcept { return Dark(); }

    /// Constructs a curated theme from a `ThemePreset` enum value.
    [[nodiscard]] static UiTheme FromPreset(ThemePreset p) noexcept;

    /// Compiles a complete `UiTheme` from a declarative `ThemeSpec` recipe.
    [[nodiscard]] static UiTheme Build(const ThemeSpec& spec) noexcept;

    /// Smoothly interpolates colors, shadows, radii, and metrics between two themes (`t` in [0..1]).
    [[nodiscard]] static UiTheme Lerp(const UiTheme& a, const UiTheme& b, keel::f32 t) noexcept;

    /// Updates color palette, syntax palette, shadows, and semantic variants to Dark or Light mode while preserving shell style.
    void SetMode(ThemeMode m) noexcept;

    /// Applies a curated preset's palette, syntax palette, shadows, and semantic variants while preserving shell style.
    void SetPreset(ThemePreset p) noexcept;

    /// Updates application shell framing metrics (windowPadding, panelGap, panelRadius, panelBorderWidth) for Docked or Floating style.
    void SetShellStyle(ShellStyle style) noexcept;

    /// Applies a seed brand accent color, auto-deriving `accentHover`, WCAG-contrast `accentText`, `focusRing`, `caret`, and `variants[Primary]`.
    void SetAccent(Color accent) noexcept;

    /// Overrides a semantic variant (`Primary`, `Success`, `Warning`, `Danger`, `Info`) with auto-derived hover/active/contrast text.
    void SetSemanticColor(UiVariant variant, Color baseColor) noexcept;

    /// Blends a subtle neutral tint seed into backgrounds, surfaces, and borders.
    void SetNeutralTint(Color tint, keel::f32 strength = 0.25f) noexcept;

    /// Scales corner radii consistently across all tokens, widget metrics, and floating shell panels (`0.0` = sharp, `1.0` = default).
    void SetRadiusScale(keel::f32 scale) noexcept;

    /// Scales spacing gaps and paddings across layout tokens, component metrics, and floating shell chrome.
    void SetSpacingScale(keel::f32 scale) noexcept;

    /// Scales typography roles, component text sizes, and heading metrics globally.
    void SetFontScale(keel::f32 scale) noexcept;

    /// Scales border stroke thicknesses across variants, inputs, selects, and shell dividers.
    void SetBorderScale(keel::f32 scale) noexcept;

    /// Scales elevation shadow intensity (`0.0` = flat / disables shadows, `1.0` = default, `1.5` = deep).
    void SetShadowIntensity(keel::f32 intensity) noexcept;

    /// Switches structural visual language (`Modern`, `Fluent`, `Classic3D`, `Brutalist`) without altering the color palette.
    void SetChromeStyle(ChromeStyle style) noexcept;

    /// Transforms semantic & neutral control variants to match `Solid`, `Soft`, or `Outline` fill treatment.
    void SetFillStyle(FillStyle style) noexcept;

    /// Adjusts tonal separation between viewport background, cards, elevated surfaces, inputs, and borders (`0.5..1.8`).
    void SetSurfaceContrast(keel::f32 contrast) noexcept;

    /// Applies an independent curated syntax highlighting palette (`Auto` preserves the active theme preset's syntax colors).
    void SetSyntaxPreset(SyntaxPreset preset) noexcept;
};

/// Backward-compatible alias for UiTheme.
using UiStyle = UiTheme;

} // namespace guikit
