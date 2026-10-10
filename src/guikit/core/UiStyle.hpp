#pragma once

#include <algorithm>
#include <cstdint>
#include "guikit/core/Tokens.hpp"
#include "guikit/layout/LayoutTypes.hpp"

namespace guikit {

/// Density presets defining the compactness and touch targets of the interface.
enum class Density : uint8_t {
    Standard,  ///< Standard desktop productivity scale
    Compact,   ///< High-density engineering data / dense tabular workflows
    Touch      ///< Touch-friendly enlarged targets and generous spacing
};

/// Configurable spacing scale mapping UiSize tokens to physical point values.
struct SpacingScale {
    float none{0.0f};
    float xs{4.0f};
    float sm{8.0f};
    float md{12.0f};
    float lg{16.0f};
    float xl{24.0f};

    [[nodiscard]] constexpr float Get(UiSize size) const noexcept {
        switch (size) {
            case UiSize::None: return none;
            case UiSize::Xs:   return xs;
            case UiSize::Sm:   return sm;
            case UiSize::Md:   return md;
            case UiSize::Lg:   return lg;
            case UiSize::Xl:   return xl;
        }
        return md;
    }
};

/// Configurable corner radius scale mapping UiSize tokens to physical point values.
struct RadiusScale {
    float none{0.0f};
    float xs{2.0f};
    float sm{4.0f};
    float md{6.0f};
    float lg{8.0f};
    float xl{12.0f};

    [[nodiscard]] constexpr float Get(UiSize size) const noexcept {
        switch (size) {
            case UiSize::None: return none;
            case UiSize::Xs:   return xs;
            case UiSize::Sm:   return sm;
            case UiSize::Md:   return md;
            case UiSize::Lg:   return lg;
            case UiSize::Xl:   return xl;
        }
        return md;
    }
};

/// Application-level style engine resolving tokens and density modes to physical metrics.
class UiStyle {
public:
    constexpr UiStyle() noexcept = default;

    /// Preset for standard desktop density.
    [[nodiscard]] static constexpr UiStyle Standard() noexcept {
        UiStyle s;
        s.m_density = Density::Standard;
        s.m_spacing = {0.0f, 4.0f, 8.0f, 12.0f, 16.0f, 24.0f};
        s.m_radius  = {0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 12.0f};
        return s;
    }

    /// Preset for high-density engineering workstations.
    [[nodiscard]] static constexpr UiStyle Compact() noexcept {
        UiStyle s;
        s.m_density = Density::Compact;
        s.m_spacing = {0.0f, 2.0f, 4.0f, 8.0f, 12.0f, 16.0f};
        s.m_radius  = {0.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f};
        return s;
    }

    /// Preset for touch panels and kiosks.
    [[nodiscard]] static constexpr UiStyle Touch() noexcept {
        UiStyle s;
        s.m_density = Density::Touch;
        s.m_spacing = {0.0f, 6.0f, 12.0f, 16.0f, 24.0f, 32.0f};
        s.m_radius  = {0.0f, 4.0f, 6.0f, 8.0f, 12.0f, 16.0f};
        return s;
    }

    /// Returns the active density mode.
    [[nodiscard]] constexpr Density GetDensity() const noexcept { return m_density; }

    /// Sets the active density mode, reconfiguring spacing and radius defaults.
    constexpr void SetDensity(Density d) noexcept {
        *this = (d == Density::Compact) ? Compact() : (d == Density::Touch ? Touch() : Standard());
    }

    /// Resolves spacing for a given UiSize token.
    [[nodiscard]] constexpr float GetGap(UiSize size) const noexcept {
        return m_spacing.Get(size);
    }

    /// Resolves corner radius for a given UiSize token.
    [[nodiscard]] constexpr float GetRadius(UiSize size) const noexcept {
        return m_radius.Get(size);
    }

    /// Resolves default container / card padding for a given UiSize token.
    [[nodiscard]] constexpr Padding GetPadding(UiSize size) const noexcept {
        if (size == UiSize::None) return Padding{};
        const float val = (size == UiSize::Xs) ? m_spacing.sm :
                          (size == UiSize::Sm) ? m_spacing.md :
                          (size == UiSize::Md) ? m_spacing.lg :
                          (size == UiSize::Lg) ? m_spacing.xl : (m_spacing.xl * 1.5f);
        return Padding::All(val);
    }

    /// Mutable access to spacing scale.
    [[nodiscard]] constexpr SpacingScale& Spacing() noexcept { return m_spacing; }
    [[nodiscard]] constexpr const SpacingScale& Spacing() const noexcept { return m_spacing; }

    /// Mutable access to radius scale.
    [[nodiscard]] constexpr RadiusScale& Radius() noexcept { return m_radius; }
    [[nodiscard]] constexpr const RadiusScale& Radius() const noexcept { return m_radius; }

    /// Returns density-adapted component metrics for buttons, inputs, and controls.
    [[nodiscard]] constexpr ComponentMetrics GetComponentMetrics(UiSize size) const noexcept {
        if (m_density == Density::Compact) {
            switch (size) {
                case UiSize::None: return {  0.0f,  0.0f,  0.0f,  0.0f, 0.0f,  0.0f };
                case UiSize::Xs:   return { 20.0f, 10.0f,  6.0f,  2.0f, 2.0f, 10.0f };
                case UiSize::Sm:   return { 26.0f, 12.0f, 10.0f,  4.0f, 3.0f, 13.0f };
                case UiSize::Md:   return { 32.0f, 13.0f, 12.0f,  5.0f, 4.0f, 15.0f };
                case UiSize::Lg:   return { 40.0f, 15.0f, 16.0f,  8.0f, 6.0f, 18.0f };
                case UiSize::Xl:   return { 48.0f, 18.0f, 20.0f, 10.0f, 8.0f, 22.0f };
            }
        } else if (m_density == Density::Touch) {
            switch (size) {
                case UiSize::None: return {  0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  0.0f };
                case UiSize::Xs:   return { 28.0f, 12.0f, 10.0f,  4.0f,  4.0f, 14.0f };
                case UiSize::Sm:   return { 36.0f, 14.0f, 14.0f,  6.0f,  6.0f, 16.0f };
                case UiSize::Md:   return { 48.0f, 18.0f, 20.0f, 10.0f,  8.0f, 22.0f };
                case UiSize::Lg:   return { 56.0f, 20.0f, 24.0f, 12.0f, 10.0f, 26.0f };
                case UiSize::Xl:   return { 64.0f, 24.0f, 28.0f, 14.0f, 12.0f, 30.0f };
            }
        }
        return guikit::GetComponentMetrics(size);
    }

private:
    Density m_density{Density::Standard};
    SpacingScale m_spacing{0.0f, 4.0f, 8.0f, 12.0f, 16.0f, 24.0f};
    RadiusScale m_radius{0.0f, 2.0f, 4.0f, 6.0f, 8.0f, 12.0f};
};

} // namespace guikit
