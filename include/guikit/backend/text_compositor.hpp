#pragma once

#include "guikit/backend/i_canvas.hpp"
#include "keel/array.h"
#include "keel/core.h"

namespace guikit {

/// Backend-agnostic text mask filtering and gamma-correct alpha / LCD subpixel compositor.
///
/// Designed to be shared across all GuiKit text rasterizers:
/// - Level 1: ThorVG outline masks (1x grayscale / 3x horizontal LCD)
/// - Level 2: FreeType hinted glyph atlas (`FT_PIXEL_MODE_GRAY` / `FT_PIXEL_MODE_LCD`)
/// - Level 3: HarfBuzz shaped runs over FreeType glyphs
class TextCompositor {
public:
    /// Resolves `FontSmoothing::Auto` based on physical canvas scale (`< 1.5` -> `SubpixelRGB`, `>= 1.5` -> `Grayscale`).
    [[nodiscard]] static constexpr FontSmoothing ResolveEffectiveSmoothing(
        FontSmoothing requested,
        float canvasScale
    ) noexcept {
        if (requested != FontSmoothing::Auto) {
            return requested;
        }
        return (canvasScale < 1.5f) ? FontSmoothing::SubpixelRGB : FontSmoothing::Grayscale;
    }

    /// Converts 8-bit sRGB channel `[0..255]` to 12-bit linear light `[0..4095]`.
    [[nodiscard]] static keel::u16 SrgbToLinear12(keel::u8 srgb) noexcept;

    /// Converts 12-bit linear light `[0..4095]` back to 8-bit sRGB `[0..255]`.
    [[nodiscard]] static keel::u8 Linear12ToSrgb(keel::u16 lin12) noexcept;

    /// Builds a monotonic 256-entry coverage shaping LUT incorporating stroke contrast and small-size stem darkening.
    /// Guarantees `outLut[0] == 0` and `outLut[255] == 255`.
    static void BuildCoverageLut(
        keel::StaticArray<keel::u8, 256>& outLut,
        float contrast,
        float stemDarkening,
        float scaledFontSizePx
    ) noexcept;

    /// Applies a 5-tap balanced FreeType-style LCD FIR filter (`[8, 77, 86, 77, 8] / 256`)
    /// across `pixelWidth * 3` horizontal coverage samples to eliminate color fringing while preserving energy.
    static void FilterSubpixel3xRow(
        const keel::u8* srcSubpixels,
        keel::u8* dstRgb,
        keel::u32 pixelWidth
    ) noexcept;

    /// Composites an 8-bit grayscale coverage mask into an `ABGR8888_Premul` target surface.
    static void BlitGrayscaleMask(
        keel::u32* dstBuffer,
        keel::u32 dstW,
        keel::u32 dstH,
        keel::u32 dstStride,
        const keel::u8* mask,
        keel::u32 maskW,
        keel::u32 maskH,
        keel::u32 maskStride,
        keel::i32 dstX,
        keel::i32 dstY,
        Rect clipPx,
        Color color,
        const TextRenderSettings& settings,
        float scaledFontSizePx
    ) noexcept;

    /// Composites a 3-byte-per-pixel RGB subpixel coverage mask into an `ABGR8888_Premul` target surface.
    /// When `bgrOrder` is true, subpixels are swizzled `(c2, c1, c0)` for BGR display panels.
    /// Gracefully falls back to scalar alpha `SrcOver` on non-opaque destination pixels (`dstA < 255`).
    static void BlitSubpixelMask(
        keel::u32* dstBuffer,
        keel::u32 dstW,
        keel::u32 dstH,
        keel::u32 dstStride,
        const keel::u8* maskRgb,
        keel::u32 maskW,
        keel::u32 maskH,
        keel::u32 maskStrideBytes,
        keel::i32 dstX,
        keel::i32 dstY,
        Rect clipPx,
        Color color,
        const TextRenderSettings& settings,
        float scaledFontSizePx,
        bool bgrOrder = false
    ) noexcept;
};

} // namespace guikit
