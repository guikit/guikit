#pragma once

#include <array>
#include <cstdint>
#include "guikit/core/Tokens.hpp"
#include "guikit/layout/LayoutTypes.hpp"

namespace guikit {

/// Complete styling theme for the GuiKit engine.
/// Flat, contiguous in memory, and fast to look up with zero dynamic allocation.
struct UiTheme {
    ThemeMode mode{ThemeMode::Dark};
    Density density{Density::Standard};

    // 1. Semantic color palette
    ThemePalette palette{};

    // 2. State colors for semantic variants (UiVariant::Default .. Muted, 9 variants)
    std::array<VariantColors, 9> variants{};

    // 3. Component metrics per UiSize (None, Xs, Sm, Md, Lg, Xl, 6 sizes)
    std::array<ComponentMetrics, 6> metrics{};

    // 4. Typography metrics per TextRole (DisplayLarge .. CodeSmall, 18 roles)
    std::array<TypographyMetrics, 18> typography{};

    // 5. Scrollbar metrics per UiSize (6 sizes)
    std::array<ScrollbarMetrics, 6> scrollbars{};

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

    [[nodiscard]] constexpr const TypographyMetrics& GetTypography(TextRole role) const noexcept {
        const size_t idx = static_cast<size_t>(role);
        return (idx < typography.size()) ? typography[idx] : typography[static_cast<size_t>(TextRole::BodyMedium)];
    }
    [[nodiscard]] constexpr TypographyMetrics& GetTypography(TextRole role) noexcept {
        const size_t idx = static_cast<size_t>(role);
        return (idx < typography.size()) ? typography[idx] : typography[static_cast<size_t>(TextRole::BodyMedium)];
    }

    [[nodiscard]] constexpr const ScrollbarMetrics& GetScrollbar(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < scrollbars.size()) ? scrollbars[idx] : scrollbars[static_cast<size_t>(UiSize::Md)];
    }

    [[nodiscard]] constexpr float GetGap(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < spacing.size()) ? spacing[idx] : spacing[static_cast<size_t>(UiSize::Md)];
    }

    [[nodiscard]] constexpr float GetRadius(UiSize size) const noexcept {
        const size_t idx = static_cast<size_t>(size);
        return (idx < radius.size()) ? radius[idx] : radius[static_cast<size_t>(UiSize::Md)];
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
    [[nodiscard]] constexpr const ComponentMetrics& GetComponentMetrics(UiSize size) const noexcept { return GetMetrics(size); }

    // --- Standard Theme Factories ---
    [[nodiscard]] static UiTheme Dark(Density d = Density::Standard) noexcept;
    [[nodiscard]] static UiTheme Light(Density d = Density::Standard) noexcept;
    [[nodiscard]] static UiTheme Standard() noexcept { return Dark(Density::Standard); }
    [[nodiscard]] static UiTheme Compact() noexcept { return Dark(Density::Compact); }
    [[nodiscard]] static UiTheme Touch() noexcept { return Dark(Density::Touch); }

    /// Updates metrics, spacing, and radius to the requested density scale.
    void SetDensity(Density d) noexcept;
};

/// Backward-compatible alias for UiTheme.
using UiStyle = UiTheme;

} // namespace guikit
