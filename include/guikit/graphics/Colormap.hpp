#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include "guikit/core/Types.hpp"

namespace guikit {

class ICanvas;

/// Unified scientific, radiographic, spectral, and tactical color palettes across GuiKit.
enum class Colormap : uint8_t {
    Grayscale = 0,      ///< Linear monochrome black -> white
    Bone,               ///< Medical / radiographic cool steel-blue tinted monochrome
    Amber,              ///< Warm radiographic / golden sepia monochrome
    Inferno,            ///< Perceptually uniform black -> purple -> crimson -> orange -> bright yellow
    Magma,              ///< Perceptually uniform black -> indigo -> magenta -> warm cream
    Viridis,            ///< Perceptually uniform deep navy -> teal -> emerald -> yellow
    Plasma,             ///< Perceptually uniform deep blue -> violet -> coral -> bright yellow
    Cividis,            ///< Colorblind-safe perceptually uniform navy -> slate -> olive -> yellow
    Turbo,              ///< Smooth high-contrast spectral rainbow (Navy -> Cyan -> Green -> Yellow -> Red -> White)
    Jet,                ///< Classic acoustic / thermal rainbow (Navy -> Cyan -> Green -> Yellow -> Crimson)
    CyanHot,            ///< Spectral repair palette: deep navy -> cyan -> electric blue -> orange -> white
    Hot,                ///< Thermal blackbody radiation: black -> crimson -> orange -> yellow -> white
    SonarGold,          ///< High-frequency imaging sonar: abyssal black -> bronze -> copper -> bright gold
    SonarBlue,          ///< Hydroacoustic BTR console: abyssal black -> deep blue -> electric cyan -> ice white
    PhosphorGreen,      ///< Tactical radar/sonar CRT phosphor: dark green -> emerald -> neon green -> white-green
    NightRed,           ///< Bridge night-watch monochrome red: black -> maroon -> crimson -> bright scarlet
    Count
};

/// Packs an RGBA `Color` into a 32-bit `ABGR8888` pixel (`(a << 24) | (b << 16) | (g << 8) | r`).
[[nodiscard]] constexpr uint32_t PackAbgr(Color c) noexcept {
    return (static_cast<uint32_t>(c.a) << 24u) |
           (static_cast<uint32_t>(c.b) << 16u) |
           (static_cast<uint32_t>(c.g) << 8u)  |
           static_cast<uint32_t>(c.r);
}

/// Unpacks a 32-bit `ABGR8888` pixel into a `Color`.
[[nodiscard]] constexpr Color UnpackAbgr(uint32_t abgr) noexcept {
    return Color(
        static_cast<uint8_t>(abgr & 0xFFu),
        static_cast<uint8_t>((abgr >> 8u) & 0xFFu),
        static_cast<uint8_t>((abgr >> 16u) & 0xFFu),
        static_cast<uint8_t>((abgr >> 24u) & 0xFFu)
    );
}

/// Evaluates the specified `Colormap` at normalized position `t` in `[0.0, 1.0]`.
[[nodiscard]] Color SampleColormap(Colormap map, float t, bool invert = false) noexcept;

/// Evaluates the specified `Colormap` at normalized position `t` in `[0.0, 1.0]` as packed `ABGR8888`.
[[nodiscard]] uint32_t SampleColormapAbgr(Colormap map, float t, bool invert = false) noexcept;

/// Returns a precomputed 256-entry `Color` lookup table for the specified `Colormap` (zero frame allocation).
[[nodiscard]] const std::array<Color, 256>& GetColormapLut(Colormap map, bool invert = false) noexcept;

/// Returns a precomputed 256-entry `ABGR8888` lookup table for the specified `Colormap` (zero frame allocation).
[[nodiscard]] const std::array<uint32_t, 256>& GetColormapLutAbgr(Colormap map, bool invert = false) noexcept;

/// Copies the precomputed 256-entry `ABGR8888` lookup table into a destination array (e.g. GPU shader descriptor).
void CopyColormapLutAbgr(Colormap map, uint32_t outAbgr[256], bool invert = false) noexcept;

/// Returns the human-readable display name of a `Colormap`.
[[nodiscard]] std::string_view GetColormapName(Colormap map) noexcept;

/// Returns a static span of all `Colormap` display names in enum order (`0 .. Count-1`).
[[nodiscard]] std::span<const std::string_view> GetAllColormapNames() noexcept;

/// Draws a smooth colorbar strip (horizontal or vertical) for the specified `Colormap`.
void DrawColormapBar(
    ICanvas& canvas,
    Rect rect,
    Colormap map,
    bool vertical = false,
    bool invert = false,
    float cornerRadius = 0.0f,
    Color borderColor = Color::Transparent()
);

} // namespace guikit
