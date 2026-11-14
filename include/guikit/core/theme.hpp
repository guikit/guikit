#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"

namespace guikit {

/// Complete styling theme for the GuiKit engine.
/// Flat, contiguous in memory, and fast to look up with zero dynamic allocation.
struct UiTheme {
    ThemeMode mode{ThemeMode::Dark};
    Density density{Density::Standard};
    ShellStyle shellStyle{ShellStyle::Docked};
    float lineHeightMultiplier{kDefaultLineHeightMultiplier};
    float scale{1.0f};

    // 0. Application shell framing tokens (resolved from shellStyle + density + scale)
    ShellMetrics shell{};

    // 1. Semantic color palette
    ThemePalette palette{};

    // 1b. Syntax highlighting color palette for code editors & viewers
    SyntaxPalette syntax{};

    // 2. State colors for semantic variants (UiVariant::Default .. Muted, 9 variants)
    std::array<VariantColors, 9> variants{};

    // 3. Component metrics per UiSize (None, Xs, Sm, Md, Lg, Xl, 6 sizes)
    std::array<ComponentMetrics, 6> metrics{};

    // 4. Typography metrics per TextRole (DisplayLarge .. CodeSmall, 18 roles)
    std::array<TypographyMetrics, 18> typography{};

    // 5. Scrollbar metrics per UiSize (6 sizes)
    std::array<ScrollbarMetrics, 6> scrollbars{};

    // 5b. Carousel navigation metrics per UiSize (6 sizes)
    std::array<CarouselMetrics, 6> carousels{};

    // 6. Spacing scale (gaps) per UiSize (6 sizes)
    std::array<float, 6> spacing{};

    // 7. Corner radius scale per UiSize (6 sizes)
    std::array<float, 6> radius{};

    // 8. Specific widget metrics per UiSize (6 sizes each)
    std::array<SwitchMetrics, 6>     switches{};
    std::array<CheckboxMetrics, 6>   checkboxes{};
    std::array<InputFieldMetrics, 6> inputs{};
    std::array<SelectMetrics, 6>     selects{};
    std::array<HeadingMetrics, 6>    headings{};
    std::array<SliderMetrics, 6>     sliders{};

    // --- Fast Inline Accessors ---

    [[nodiscard]] constexpr const ComponentMetrics& GetMetrics(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < metrics.size()) ? metrics[idx] : metrics[static_cast<size_t>(UiSize::Md)];
    }
    [[nodiscard]] constexpr ComponentMetrics& GetMetrics(UiSize size) noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < metrics.size()) ? metrics[idx] : metrics[static_cast<size_t>(UiSize::Md)];
    }

    [[nodiscard]] constexpr const VariantColors& GetVariant(UiVariant variant) const noexcept {
        const size_t idx = static_cast<size_t>(variant);
        return (idx < variants.size()) ? variants[idx] : variants[static_cast<size_t>(UiVariant::Default)];
    }
    [[nodiscard]] constexpr VariantColors& GetVariant(UiVariant variant) noexcept {
        const size_t idx = static_cast<size_t>(variant);
        return (idx < variants.size()) ? variants[idx] : variants[static_cast<size_t>(UiVariant::Default)];
    }

    [[nodiscard]] constexpr Color GetTextColor(UiVariant variant) const noexcept {
        switch (variant) {
            case UiVariant::Default:   return palette.textPrimary;
            case UiVariant::Secondary:
            case UiVariant::Muted:     return palette.textSecondary;
            case UiVariant::Ghost:     return palette.textMuted;
            default:                   return GetVariant(variant).bg;
        }
    }

    [[nodiscard]] constexpr const TypographyMetrics& GetTypography(TextRole role) const noexcept {
        const size_t idx = static_cast<size_t>(role);
        return (idx < typography.size()) ? typography[idx] : typography[static_cast<size_t>(TextRole::BodyMedium)];
    }
    [[nodiscard]] constexpr TypographyMetrics& GetTypography(TextRole role) noexcept {
        const size_t idx = static_cast<size_t>(role);
        return (idx < typography.size()) ? typography[idx] : typography[static_cast<size_t>(TextRole::BodyMedium)];
    }

    /// Resolves font size for a semantic TextRole at a given widget UiSize.
    [[nodiscard]] constexpr float ResolveFontSize(
        TextRole role,
        UiSize widgetSize = UiSize::Md
    ) const noexcept {
        if ((widgetSize == UiSize::Md || widgetSize == UiSize::None) && density == Density::Standard) {
            return GetTypography(role).fontSize;
        }
        const ComponentMetrics& cm = GetMetrics(widgetSize);
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
                return GetTypography(role).fontSize;
        }
    }

    [[nodiscard]] constexpr float GetFontSize(
        TextRole role,
        UiSize widgetSize = UiSize::Md
    ) const noexcept {
        return ResolveFontSize(role, widgetSize);
    }

    /// Resolves font size for a UiText: checks explicit px, preset UiSize, or TextRole, falling back to defaultRole at widgetSize.
    [[nodiscard]] constexpr float ResolveFontSize(
        const UiText& text,
        TextRole defaultRole,
        UiSize widgetSize = UiSize::Md
    ) const noexcept {
        if (text.size.px > 0.0f) {
            return text.size.px;
        }
        const UiSize effectiveSize = (text.size.preset != UiSize::None) ? text.size.preset : widgetSize;
        if (text.size.role.has_value()) {
            return GetTypography(*text.size.role).fontSize;
        }
        return ResolveFontSize(defaultRole, effectiveSize);
    }

    /// Resolves font size for a UiText with an explicit fallback pixel size.
    [[nodiscard]] constexpr float ResolveFontSize(
        const UiText& text,
        float fallbackFontSize
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

    /// Resolves a semantic `StateColor` token against this theme's `variants` and `palette` (controls, surfaces, borders, badges).
    /// Custom `StateColor` values (`token == StateColorToken::Custom`) are returned unchanged.
    [[nodiscard]] constexpr StateColor ResolveStateColor(const StateColor& sc) const noexcept {
        auto fromVar = [this](UiVariant v, StateColorToken tok) constexpr noexcept -> StateColor {
            const VariantColors& vc = GetVariant(v);
            return StateColor{
                .normal   = vc.bg,
                .hover    = vc.bgHover,
                .active   = vc.bgActive,
                .disabled = vc.bg.WithAlphaF(0.40f),
                .token    = tok
            };
        };
        switch (sc.token) {
            case StateColorToken::Custom:        return sc;
            case StateColorToken::Primary:       return fromVar(UiVariant::Primary, sc.token);
            case StateColorToken::Secondary:     return fromVar(UiVariant::Secondary, sc.token);
            case StateColorToken::Success:       return fromVar(UiVariant::Success, sc.token);
            case StateColorToken::Warning:       return fromVar(UiVariant::Warning, sc.token);
            case StateColorToken::Danger:        return fromVar(UiVariant::Danger, sc.token);
            case StateColorToken::Info:          return fromVar(UiVariant::Info, sc.token);
            case StateColorToken::Ghost: {
                const VariantColors& vc = GetVariant(UiVariant::Ghost);
                return StateColor{
                    .normal   = vc.bg,
                    .hover    = vc.bgHover,
                    .active   = vc.bgActive,
                    .disabled = Color::Transparent(),
                    .token    = sc.token
                };
            }
            case StateColorToken::Muted:
            case StateColorToken::TextMuted:
                return StateColor{
                    .normal   = palette.textMuted,
                    .hover    = palette.textSecondary,
                    .active   = palette.textMuted,
                    .disabled = palette.textDisabled,
                    .token    = sc.token
                };
            case StateColorToken::TextPrimary:
                return StateColor{
                    .normal   = palette.textPrimary,
                    .hover    = palette.textPrimary,
                    .active   = palette.textPrimary,
                    .disabled = palette.textDisabled,
                    .token    = sc.token
                };
            case StateColorToken::TextSecondary:
                return StateColor{
                    .normal   = palette.textSecondary,
                    .hover    = palette.textPrimary,
                    .active   = palette.textSecondary,
                    .disabled = palette.textDisabled,
                    .token    = sc.token
                };
            case StateColorToken::Surface:
                return StateColor{
                    .normal   = palette.surface,
                    .hover    = palette.surfaceHover,
                    .active   = palette.surfaceActive,
                    .disabled = palette.surface.WithAlphaF(0.40f),
                    .token    = sc.token
                };
        }
        return sc;
    }

    /// Resolves a semantic `StateColor` token specifically for foreground text against this theme's Light/Dark palette.
    [[nodiscard]] constexpr StateColor ResolveTextStateColor(const StateColor& sc) const noexcept {
        switch (sc.token) {
            case StateColorToken::Custom:
                return sc;
            case StateColorToken::TextPrimary:
                return StateColor{
                    .normal   = palette.textPrimary,
                    .hover    = palette.textPrimary,
                    .active   = palette.textPrimary,
                    .disabled = palette.textDisabled,
                    .token    = sc.token
                };
            case StateColorToken::Secondary:
            case StateColorToken::Muted:
            case StateColorToken::TextSecondary:
                return StateColor{
                    .normal   = palette.textSecondary,
                    .hover    = palette.textPrimary,
                    .active   = palette.textSecondary,
                    .disabled = palette.textDisabled,
                    .token    = sc.token
                };
            case StateColorToken::Ghost:
            case StateColorToken::TextMuted:
                return StateColor{
                    .normal   = palette.textMuted,
                    .hover    = palette.textSecondary,
                    .active   = palette.textMuted,
                    .disabled = palette.textDisabled,
                    .token    = sc.token
                };
            default:
                return ResolveStateColor(sc);
        }
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

    [[nodiscard]] constexpr const ScrollbarMetrics& GetScrollbar(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < scrollbars.size()) ? scrollbars[idx] : scrollbars[static_cast<size_t>(UiSize::Md)];
    }
    [[nodiscard]] constexpr ScrollbarMetrics& GetScrollbar(UiSize size) noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < scrollbars.size()) ? scrollbars[idx] : scrollbars[static_cast<size_t>(UiSize::Md)];
    }

    [[nodiscard]] constexpr const CarouselMetrics& GetCarousel(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < carousels.size()) ? carousels[idx] : carousels[static_cast<size_t>(UiSize::Md)];
    }
    [[nodiscard]] constexpr CarouselMetrics& GetCarousel(UiSize size) noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < carousels.size()) ? carousels[idx] : carousels[static_cast<size_t>(UiSize::Md)];
    }

    /// Scales an explicit logical dimension (dp at 100% scale) by the active theme scale factor.
    [[nodiscard]] constexpr float Scale(float v) const noexcept {
        return v * scale;
    }

    [[nodiscard]] constexpr float GetGap(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < spacing.size()) ? spacing[idx] : spacing[static_cast<size_t>(UiSize::Md)];
    }

    /// Resolves an optional explicit gap (in base dp) or falls back to the theme spacing token for fallbackSize.
    [[nodiscard]] constexpr float ResolveGap(std::optional<float> explicitGap, UiSize fallbackSize = UiSize::Md) const noexcept {
        return explicitGap.has_value() ? (*explicitGap * scale) : GetGap(fallbackSize);
    }

    [[nodiscard]] constexpr float GetRadius(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < radius.size()) ? radius[idx] : radius[static_cast<size_t>(UiSize::Md)];
    }

    /// Resolves an optional explicit corner radius (in base dp) or falls back to the theme radius token for fallbackSize.
    [[nodiscard]] constexpr float ResolveRadius(std::optional<float> explicitRadius, UiSize fallbackSize = UiSize::Md) const noexcept {
        return explicitRadius.has_value() ? (*explicitRadius * scale) : GetRadius(fallbackSize);
    }

    [[nodiscard]] constexpr Padding GetPadding(UiSize size) const noexcept {
        if (size == UiSize::None) return Padding{};
        const float val = (size == UiSize::Xs) ? spacing[static_cast<size_t>(UiSize::Sm)] :
                          (size == UiSize::Sm) ? spacing[static_cast<size_t>(UiSize::Md)] :
                          (size == UiSize::Md) ? spacing[static_cast<size_t>(UiSize::Lg)] :
                          (size == UiSize::Lg) ? spacing[static_cast<size_t>(UiSize::Xl)] :
                          (spacing[static_cast<size_t>(UiSize::Xl)] * 1.5f);
        return Padding::All(val);
    }

    /// Resolves an optional padding (design token or explicit insets in base dp) into scaled layout coordinates.
    [[nodiscard]] constexpr Padding ResolvePadding(std::optional<Padding> p, UiSize fallbackSize = UiSize::None) const noexcept {
        if (p.has_value()) {
            if (p->IsToken()) {
                return GetPadding(p->GetToken());
            }
            return Padding(p->left * scale, p->top * scale, p->right * scale, p->bottom * scale);
        }
        if (fallbackSize != UiSize::None) {
            return GetPadding(fallbackSize);
        }
        return Padding{};
    }

    [[nodiscard]] constexpr const SwitchMetrics& GetSwitch(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < switches.size()) ? switches[idx] : switches[static_cast<size_t>(UiSize::Md)];
    }
    [[nodiscard]] constexpr SwitchMetrics& GetSwitch(UiSize size) noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < switches.size()) ? switches[idx] : switches[static_cast<size_t>(UiSize::Md)];
    }

    [[nodiscard]] constexpr const CheckboxMetrics& GetCheckbox(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < checkboxes.size()) ? checkboxes[idx] : checkboxes[static_cast<size_t>(UiSize::Md)];
    }
    [[nodiscard]] constexpr CheckboxMetrics& GetCheckbox(UiSize size) noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < checkboxes.size()) ? checkboxes[idx] : checkboxes[static_cast<size_t>(UiSize::Md)];
    }

    [[nodiscard]] constexpr const InputFieldMetrics& GetInputField(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < inputs.size()) ? inputs[idx] : inputs[static_cast<size_t>(UiSize::Md)];
    }
    [[nodiscard]] constexpr InputFieldMetrics& GetInputField(UiSize size) noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < inputs.size()) ? inputs[idx] : inputs[static_cast<size_t>(UiSize::Md)];
    }

    [[nodiscard]] constexpr const SelectMetrics& GetSelect(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < selects.size()) ? selects[idx] : selects[static_cast<size_t>(UiSize::Md)];
    }
    [[nodiscard]] constexpr SelectMetrics& GetSelect(UiSize size) noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < selects.size()) ? selects[idx] : selects[static_cast<size_t>(UiSize::Md)];
    }

    [[nodiscard]] constexpr const HeadingMetrics& GetHeading(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < headings.size()) ? headings[idx] : headings[static_cast<size_t>(UiSize::Md)];
    }
    [[nodiscard]] constexpr HeadingMetrics& GetHeading(UiSize size) noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < headings.size()) ? headings[idx] : headings[static_cast<size_t>(UiSize::Md)];
    }

    [[nodiscard]] constexpr const SliderMetrics& GetSlider(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < sliders.size()) ? sliders[idx] : sliders[static_cast<size_t>(UiSize::Md)];
    }
    [[nodiscard]] constexpr SliderMetrics& GetSlider(UiSize size) noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < sliders.size()) ? sliders[idx] : sliders[static_cast<size_t>(UiSize::Md)];
    }

    struct SpacingProxy {
        float& none;
        float& xs;
        float& sm;
        float& md;
        float& lg;
        float& xl;

        explicit SpacingProxy(std::array<float, 6>& a) noexcept
            : none(a[0]), xs(a[1]), sm(a[2]), md(a[3]), lg(a[4]), xl(a[5]) {}
    };

    struct ConstSpacingProxy {
        const float none;
        const float xs;
        const float sm;
        const float md;
        const float lg;
        const float xl;

        explicit ConstSpacingProxy(const std::array<float, 6>& a) noexcept
            : none(a[0]), xs(a[1]), sm(a[2]), md(a[3]), lg(a[4]), xl(a[5]) {}
    };

    [[nodiscard]] SpacingProxy Spacing() noexcept { return SpacingProxy(spacing); }
    [[nodiscard]] ConstSpacingProxy Spacing() const noexcept { return ConstSpacingProxy(spacing); }

    [[nodiscard]] constexpr Density GetDensity() const noexcept { return density; }
    [[nodiscard]] constexpr ShellStyle GetShellStyle() const noexcept { return shellStyle; }
    [[nodiscard]] constexpr const ComponentMetrics& GetComponentMetrics(UiSize size) const noexcept { return GetMetrics(size); }
    [[nodiscard]] constexpr float GetLineHeight(float fontSize) const noexcept { return fontSize * lineHeightMultiplier; }

    // --- Standard Theme Factories ---
    [[nodiscard]] static UiTheme Dark(Density d = Density::Standard) noexcept;
    [[nodiscard]] static UiTheme Light(Density d = Density::Standard) noexcept;
    [[nodiscard]] static UiTheme Standard() noexcept { return Dark(Density::Standard); }
    [[nodiscard]] static UiTheme Compact() noexcept { return Dark(Density::Compact); }
    [[nodiscard]] static UiTheme Touch() noexcept { return Dark(Density::Touch); }

    /// Updates color palette, syntax palette, and semantic variants to Dark or Light mode while preserving density, shell style, and scale.
    void SetMode(ThemeMode m) noexcept;

    /// Updates metrics, spacing, and radius to the requested density scale.
    void SetDensity(Density d) noexcept;

    /// Updates application shell framing metrics (windowPadding, panelGap, panelRadius, panelBorderWidth) for Docked or Floating style.
    void SetShellStyle(ShellStyle style) noexcept;

    /// Returns a new theme with all metrics, typography, spacing, and radius scaled by factor.
    [[nodiscard]] UiTheme Scaled(float factor) const noexcept;
};

/// Backward-compatible alias for UiTheme.
using UiStyle = UiTheme;

} // namespace guikit
