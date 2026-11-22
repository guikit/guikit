#include "guikit/backend/text_compositor.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

// DirectWrite / Skia calibrated text blending gamma (1.8 instead of raw 2.4).
// Raw 2.4 starves dark-on-light anti-aliased strokes into faint hairlines while
// over-bloating light-on-dark bold text due to emissive display halation.
struct TextGammaLuts {
    keel::StaticArray<keel::u16, 256> toLinear12{};
    keel::StaticArray<keel::u8, 4096> toSrgb8{};

    TextGammaLuts() noexcept {
        static constexpr keel::f32 kTextGamma = 1.8f;
        static constexpr keel::f32 kInvTextGamma = 1.0f / kTextGamma;
        for (keel::u32 i = 0; i < 256u; ++i) {
            const keel::f32 u = keel::as<keel::f32>(i) / 255.0f;
            const keel::f32 lin = keel::pow(u, kTextGamma);
            toLinear12[i] = keel::as<keel::u16>(keel::clamp(keel::round_i32(lin * 4095.0f), 0, 4095));
        }
        for (keel::u32 i = 0; i < 4096u; ++i) {
            const keel::f32 lin = keel::as<keel::f32>(i) / 4095.0f;
            const keel::f32 enc = keel::pow(lin, kInvTextGamma);
            toSrgb8[i] = keel::as<keel::u8>(keel::clamp(keel::round_i32(enc * 255.0f), 0, 255));
        }
        toLinear12[0] = 0u;
        toLinear12[255] = 4095u;
        toSrgb8[0] = 0u;
        toSrgb8[4095] = 255u;
    }
};

struct SrgbLuts {
    keel::StaticArray<keel::u16, 256> toLinear12{};
    keel::StaticArray<keel::u8, 4096> toSrgb8{};

    SrgbLuts() noexcept {
        for (keel::u32 i = 0; i < 256u; ++i) {
            const keel::f32 u = keel::as<keel::f32>(i) / 255.0f;
            const keel::f32 lin = (u <= 0.04045f)
                ? (u / 12.92f)
                : keel::pow((u + 0.055f) / 1.055f, 2.4f);
            toLinear12[i] = keel::as<keel::u16>(keel::clamp(keel::round_i32(lin * 4095.0f), 0, 4095));
        }
        for (keel::u32 i = 0; i < 4096u; ++i) {
            const keel::f32 lin = keel::as<keel::f32>(i) / 4095.0f;
            const keel::f32 srgb = (lin <= 0.0031308f)
                ? (lin * 12.92f)
                : (1.055f * keel::pow(lin, 1.0f / 2.4f) - 0.055f);
            toSrgb8[i] = keel::as<keel::u8>(keel::clamp(keel::round_i32(srgb * 255.0f), 0, 255));
        }
        toLinear12[0] = 0u;
        toLinear12[255] = 4095u;
        toSrgb8[0] = 0u;
        toSrgb8[4095] = 255u;
    }
};

const TextGammaLuts& GetTextGammaLuts() noexcept {
    static const TextGammaLuts kLuts{};
    return kLuts;
}

const SrgbLuts& GetSrgbLuts() noexcept {
    static const SrgbLuts kLuts{};
    return kLuts;
}

[[nodiscard]] constexpr keel::u32 ApproxLuma8(keel::u32 r, keel::u32 g, keel::u32 b) noexcept {
    return (54u * r + 183u * g + 19u * b) >> 8u;
}

[[nodiscard]] inline keel::u32 BlendPremulSrcOverScalar(keel::u32 dst, Color src, keel::u32 effAlpha) noexcept {
    if (effAlpha == 0u) return dst;
    if (effAlpha >= 255u) {
        return 0xFF000000u | (keel::as<keel::u32>(src.b) << 16) |
               (keel::as<keel::u32>(src.g) << 8) | keel::as<keel::u32>(src.r);
    }

    const keel::u32 invA = 255u - effAlpha;
    const keel::u32 dstA = (dst >> 24) & 0xFFu;
    if (dstA == 0u) {
        const keel::u32 sr = (keel::as<keel::u32>(src.r) * effAlpha + 127u) / 255u;
        const keel::u32 sg = (keel::as<keel::u32>(src.g) * effAlpha + 127u) / 255u;
        const keel::u32 sb = (keel::as<keel::u32>(src.b) * effAlpha + 127u) / 255u;
        return (effAlpha << 24) | (sb << 16) | (sg << 8) | sr;
    }

    const keel::u32 dr = dst & 0xFFu;
    const keel::u32 dg = (dst >> 8) & 0xFFu;
    const keel::u32 db = (dst >> 16) & 0xFFu;
    const keel::u32 outR = keel::min<keel::u32>(255u, (keel::as<keel::u32>(src.r) * effAlpha + dr * invA + 127u) / 255u);
    const keel::u32 outG = keel::min<keel::u32>(255u, (keel::as<keel::u32>(src.g) * effAlpha + dg * invA + 127u) / 255u);
    const keel::u32 outB = keel::min<keel::u32>(255u, (keel::as<keel::u32>(src.b) * effAlpha + db * invA + 127u) / 255u);
    const keel::u32 outA = keel::min<keel::u32>(255u, effAlpha + (dstA * invA + 127u) / 255u);
    return (outA << 24) | (outB << 16) | (outG << 8) | outR;
}

[[nodiscard]] inline bool ComputeClampedBlitRect(
    keel::u32 dstW,
    keel::u32 dstH,
    keel::u32 maskW,
    keel::u32 maskH,
    keel::i32 dstX,
    keel::i32 dstY,
    Rect clipPx,
    keel::i32& outClipX0,
    keel::i32& outClipY0,
    keel::i32& outClipX1,
    keel::i32& outClipY1
) noexcept {
    const keel::i32 x0 = dstX;
    const keel::i32 y0 = dstY;
    const keel::i32 x1 = x0 + keel::as<keel::i32>(maskW);
    const keel::i32 y1 = y0 + keel::as<keel::i32>(maskH);

    keel::i32 cx0 = keel::max(0, x0);
    keel::i32 cy0 = keel::max(0, y0);
    keel::i32 cx1 = keel::min(keel::as<keel::i32>(dstW), x1);
    keel::i32 cy1 = keel::min(keel::as<keel::i32>(dstH), y1);

    if (clipPx.w > 0.0f && clipPx.h > 0.0f) {
        cx0 = keel::max(cx0, keel::floor_i32(clipPx.x));
        cy0 = keel::max(cy0, keel::floor_i32(clipPx.y));
        cx1 = keel::min(cx1, keel::ceil_i32(clipPx.Right()));
        cy1 = keel::min(cy1, keel::ceil_i32(clipPx.Bottom()));
    }

    if (cx0 >= cx1 || cy0 >= cy1) {
        return false;
    }

    outClipX0 = cx0;
    outClipY0 = cy0;
    outClipX1 = cx1;
    outClipY1 = cy1;
    return true;
}

} // namespace

keel::u16 TextCompositor::SrgbToLinear12(keel::u8 srgb) noexcept {
    return GetSrgbLuts().toLinear12[srgb];
}

keel::u8 TextCompositor::Linear12ToSrgb(keel::u16 lin12) noexcept {
    const keel::u32 clamped = keel::min<keel::u32>(lin12, 4095u);
    return GetSrgbLuts().toSrgb8[clamped];
}

void TextCompositor::BuildCoverageLut(
    keel::StaticArray<keel::u8, 256>& outLut,
    keel::f32 contrast,
    keel::f32 stemDarkening,
    keel::f32 scaledFontSizePx
) noexcept {
    const keel::f32 clampedContrast = keel::clamp(contrast, 0.5f, 2.0f);
    const keel::f32 clampedDarken = keel::clamp(stemDarkening, 0.0f, 0.5f);
    // Taper stem darkening smoothly to 0 on large display typography (> 28 px)
    const keel::f32 sizeFactor = (scaledFontSizePx > 0.0f)
        ? keel::clamp((28.0f - scaledFontSizePx) / 16.0f, 0.0f, 1.0f)
        : 1.0f;
    const keel::f32 effDarken = clampedDarken * sizeFactor;
    const bool identity = (keel::abs(clampedContrast - 1.0f) < 0.002f) && (effDarken < 0.002f);

    if (identity) {
        for (keel::u32 i = 0; i < 256u; ++i) {
            outLut[i] = keel::as<keel::u8>(i);
        }
        return;
    }

    const keel::f32 invContrast = 1.0f / clampedContrast;
    outLut[0] = 0u;
    for (keel::u32 i = 1; i < 255u; ++i) {
        const keel::f32 c = keel::as<keel::f32>(i) / 255.0f;
        keel::f32 shaped = (keel::abs(invContrast - 1.0f) > 0.002f) ? keel::pow(c, invContrast) : c;
        if (effDarken > 0.0f) {
            // Smooth monotonic mid-tone lift peaking on thin 1px anti-aliased strokes
            shaped = shaped + effDarken * shaped * (1.0f - shaped) * (1.5f - 0.5f * shaped);
        }
        outLut[i] = keel::as<keel::u8>(keel::clamp(keel::round_i32(shaped * 255.0f), 0, 255));
    }
    outLut[255] = 255u;
}

void TextCompositor::FilterSubpixel3xRow(
    const keel::u8* srcSubpixels,
    keel::u8* dstRgb,
    keel::u32 pixelWidth
) noexcept {
    if (!srcSubpixels || !dstRgb || pixelWidth == 0u) {
        return;
    }
    const keel::i32 totalSub = keel::as<keel::i32>(pixelWidth * 3u);
    auto sample = [&](keel::i32 idx) noexcept -> keel::u32 {
        return (idx >= 0 && idx < totalSub) ? keel::as<keel::u32>(srcSubpixels[idx]) : 0u;
    };

    // FreeType FT_LCD_FILTER_DEFAULT 5-tap balanced FIR weights: [8, 77, 86, 77, 8] (sum = 256)
    for (keel::i32 i = 0; i < totalSub; ++i) {
        const keel::u32 acc =
            8u * sample(i - 2) +
            77u * sample(i - 1) +
            86u * sample(i) +
            77u * sample(i + 1) +
            8u * sample(i + 2);
        dstRgb[i] = keel::as<keel::u8>((acc + 128u) >> 8);
    }
}

void TextCompositor::BlitGrayscaleMask(
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
    keel::f32 scaledFontSizePx
) noexcept {
    if (!dstBuffer || !mask || dstW == 0u || dstH == 0u || maskW == 0u || maskH == 0u || color.a == 0u) {
        return;
    }

    keel::i32 cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0;
    if (!ComputeClampedBlitRect(dstW, dstH, maskW, maskH, dstX, dstY, clipPx, cx0, cy0, cx1, cy1)) {
        return;
    }

    const bool useGamma = settings.gammaCorrectBlending;
    keel::StaticArray<keel::u8, 256> covLutDarkOnLight{};
    keel::StaticArray<keel::u8, 256> covLutLightOnDark{};
    if (useGamma) {
        // Polarity-aware stem darkening (DirectWrite / CoreText / Skia style):
        // dark-on-light needs stronger ink weight to resist bright-pixel halation;
        // light-on-dark already receives lift from 1.8 gamma blending.
        const keel::f32 darkBoost = keel::clamp(settings.stemDarkening * 1.85f + 0.14f, 0.0f, 0.50f);
        const keel::f32 lightBoost = keel::clamp(settings.stemDarkening * 0.25f, 0.0f, 0.20f);
        BuildCoverageLut(covLutDarkOnLight, settings.contrast, darkBoost, scaledFontSizePx);
        BuildCoverageLut(covLutLightOnDark, settings.contrast, lightBoost, scaledFontSizePx);
    } else {
        BuildCoverageLut(covLutDarkOnLight, settings.contrast, settings.stemDarkening, scaledFontSizePx);
        covLutLightOnDark = covLutDarkOnLight;
    }

    const TextGammaLuts& luts = GetTextGammaLuts();
    const keel::u32 srcLinR = luts.toLinear12[color.r];
    const keel::u32 srcLinG = luts.toLinear12[color.g];
    const keel::u32 srcLinB = luts.toLinear12[color.b];
    const keel::u32 srcLuma = ApproxLuma8(color.r, color.g, color.b);
    const keel::u32 srcOpaquePx = 0xFF000000u | (keel::as<keel::u32>(color.b) << 16) |
                                  (keel::as<keel::u32>(color.g) << 8) | keel::as<keel::u32>(color.r);
    const keel::u32 colorA = color.a;

    const keel::i32 spanW = cx1 - cx0;
    const keel::i32 srcOffX = cx0 - dstX;

    for (keel::i32 y = cy0; y < cy1; ++y) {
        const keel::u8* srcRow = mask + keel::as<keel::usize>(y - dstY) * maskStride + srcOffX;
        keel::u32* dstRow = dstBuffer + keel::as<keel::usize>(y) * dstStride + cx0;
        for (keel::i32 x = 0; x < spanW; ++x) {
            const keel::u8 rawCov = srcRow[x];
            if (rawCov == 0u) continue;

            const keel::u32 dstPx = dstRow[x];
            const keel::u32 dstA = (dstPx >> 24) & 0xFFu;
            const keel::u32 dr = dstPx & 0xFFu;
            const keel::u32 dg = (dstPx >> 8) & 0xFFu;
            const keel::u32 db = (dstPx >> 16) & 0xFFu;
            const bool isLightOnDark = (dstA == 255u) ? (srcLuma > ApproxLuma8(dr, dg, db)) : (srcLuma >= 128u);
            const keel::u32 shaped = isLightOnDark ? covLutLightOnDark[rawCov] : covLutDarkOnLight[rawCov];
            const keel::u32 effA = (colorA == 255u) ? shaped : ((shaped * colorA + 127u) / 255u);
            if (effA == 0u) continue;

            if (effA == 255u) {
                dstRow[x] = srcOpaquePx;
                continue;
            }

            if (dstA < 255u) {
                dstRow[x] = BlendPremulSrcOverScalar(dstPx, color, effA);
                continue;
            }

            const keel::u32 invA = 255u - effA;

            if (useGamma) {
                const keel::u32 outLinR = (keel::as<keel::u32>(luts.toLinear12[dr]) * invA + srcLinR * effA + 127u) / 255u;
                const keel::u32 outLinG = (keel::as<keel::u32>(luts.toLinear12[dg]) * invA + srcLinG * effA + 127u) / 255u;
                const keel::u32 outLinB = (keel::as<keel::u32>(luts.toLinear12[db]) * invA + srcLinB * effA + 127u) / 255u;
                const keel::u32 outR = luts.toSrgb8[outLinR];
                const keel::u32 outG = luts.toSrgb8[outLinG];
                const keel::u32 outB = luts.toSrgb8[outLinB];
                dstRow[x] = 0xFF000000u | (outB << 16) | (outG << 8) | outR;
            } else {
                const keel::u32 outR = (dr * invA + keel::as<keel::u32>(color.r) * effA + 127u) / 255u;
                const keel::u32 outG = (dg * invA + keel::as<keel::u32>(color.g) * effA + 127u) / 255u;
                const keel::u32 outB = (db * invA + keel::as<keel::u32>(color.b) * effA + 127u) / 255u;
                dstRow[x] = 0xFF000000u | (outB << 16) | (outG << 8) | outR;
            }
        }
    }
}

void TextCompositor::BlitSubpixelMask(
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
    keel::f32 scaledFontSizePx,
    bool bgrOrder
) noexcept {
    if (!dstBuffer || !maskRgb || dstW == 0u || dstH == 0u || maskW == 0u || maskH == 0u || color.a == 0u) {
        return;
    }

    keel::i32 cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0;
    if (!ComputeClampedBlitRect(dstW, dstH, maskW, maskH, dstX, dstY, clipPx, cx0, cy0, cx1, cy1)) {
        return;
    }

    const bool useGamma = settings.gammaCorrectBlending;
    keel::StaticArray<keel::u8, 256> covLutDarkOnLight{};
    keel::StaticArray<keel::u8, 256> covLutLightOnDark{};
    if (useGamma) {
        const keel::f32 darkBoost = keel::clamp(settings.stemDarkening * 1.85f + 0.14f, 0.0f, 0.50f);
        const keel::f32 lightBoost = keel::clamp(settings.stemDarkening * 0.25f, 0.0f, 0.20f);
        BuildCoverageLut(covLutDarkOnLight, settings.contrast, darkBoost, scaledFontSizePx);
        BuildCoverageLut(covLutLightOnDark, settings.contrast, lightBoost, scaledFontSizePx);
    } else {
        BuildCoverageLut(covLutDarkOnLight, settings.contrast, settings.stemDarkening, scaledFontSizePx);
        covLutLightOnDark = covLutDarkOnLight;
    }

    const TextGammaLuts& luts = GetTextGammaLuts();
    const keel::u32 srcLinR = luts.toLinear12[color.r];
    const keel::u32 srcLinG = luts.toLinear12[color.g];
    const keel::u32 srcLinB = luts.toLinear12[color.b];
    const keel::u32 srcLuma = ApproxLuma8(color.r, color.g, color.b);
    const keel::u32 srcOpaquePx = 0xFF000000u | (keel::as<keel::u32>(color.b) << 16) |
                                  (keel::as<keel::u32>(color.g) << 8) | keel::as<keel::u32>(color.r);
    const keel::u32 colorA = color.a;

    const keel::i32 spanW = cx1 - cx0;
    const keel::i32 srcOffX = cx0 - dstX;
    const keel::usize idxR = bgrOrder ? 2u : 0u;
    const keel::usize idxG = 1u;
    const keel::usize idxB = bgrOrder ? 0u : 2u;

    // Damp LCD subpixel chroma separation on saturated foreground colors (e.g. red/cyan/orange/blue
    // readouts & syntax tokens) so R/G/B coverage never creates asymmetrical luma/hue fringing.
    const keel::u32 maxC = keel::max(keel::as<keel::u32>(color.r), keel::max(keel::as<keel::u32>(color.g), keel::as<keel::u32>(color.b)));
    const keel::u32 minC = keel::min(keel::as<keel::u32>(color.r), keel::min(keel::as<keel::u32>(color.g), keel::as<keel::u32>(color.b)));
    const keel::f32 sat = (maxC > 0u) ? (keel::as<keel::f32>(maxC - minC) / keel::as<keel::f32>(maxC)) : 0.0f;
    const keel::f32 satKeep = 1.0f - 0.85f * sat;
    const keel::i32 keepLightQ8 = keel::clamp(keel::round_i32(satKeep * 256.0f), 32, 256);
    // Light-on-dark gamma lifts low subpixel tails more strongly; soften chroma gently on dark backdrops.
    const keel::i32 keepDarkQ8 = keel::clamp(keel::round_i32(satKeep * 0.72f * 256.0f), 24, 256);

    for (keel::i32 y = cy0; y < cy1; ++y) {
        const keel::u8* srcRow = maskRgb + keel::as<keel::usize>(y - dstY) * maskStrideBytes + keel::as<keel::usize>(srcOffX) * 3u;
        keel::u32* dstRow = dstBuffer + keel::as<keel::usize>(y) * dstStride + cx0;
        for (keel::i32 x = 0; x < spanW; ++x) {
            const keel::u8* sub = srcRow + keel::as<keel::usize>(x) * 3u;
            const keel::u8 rawR = sub[idxR];
            const keel::u8 rawG = sub[idxG];
            const keel::u8 rawB = sub[idxB];
            if ((rawR | rawG | rawB) == 0u) continue;

            const keel::u32 dstPx = dstRow[x];
            const keel::u32 dstA = (dstPx >> 24) & 0xFFu;
            const keel::u32 dr = dstPx & 0xFFu;
            const keel::u32 dg = (dstPx >> 8) & 0xFFu;
            const keel::u32 db = (dstPx >> 16) & 0xFFu;
            const bool isLightOnDark = (dstA == 255u) ? (srcLuma > ApproxLuma8(dr, dg, db)) : (srcLuma >= 128u);
            const auto& activeLut = isLightOnDark ? covLutLightOnDark : covLutDarkOnLight;

            keel::i32 shR = keel::as<keel::i32>(activeLut[rawR]);
            keel::i32 shG = keel::as<keel::i32>(activeLut[rawG]);
            keel::i32 shB = keel::as<keel::i32>(activeLut[rawB]);

            const keel::i32 keepQ8 = isLightOnDark ? keepDarkQ8 : keepLightQ8;
            if (keepQ8 < 256) {
                const keel::i32 avg = (shR + shG + shB + 1) / 3;
                shR = avg + (((shR - avg) * keepQ8) >> 8);
                shG = avg + (((shG - avg) * keepQ8) >> 8);
                shB = avg + (((shB - avg) * keepQ8) >> 8);
            }

            const keel::u32 uShR = keel::as<keel::u32>(keel::clamp(shR, 0, 255));
            const keel::u32 uShG = keel::as<keel::u32>(keel::clamp(shG, 0, 255));
            const keel::u32 uShB = keel::as<keel::u32>(keel::clamp(shB, 0, 255));

            const keel::u32 covR = (colorA == 255u) ? uShR : ((uShR * colorA + 127u) / 255u);
            const keel::u32 covG = (colorA == 255u) ? uShG : ((uShG * colorA + 127u) / 255u);
            const keel::u32 covB = (colorA == 255u) ? uShB : ((uShB * colorA + 127u) / 255u);

            if ((covR | covG | covB) == 0u) continue;
            if ((covR & covG & covB) == 255u) {
                dstRow[x] = srcOpaquePx;
                continue;
            }

            if (dstA < 255u) {
                // Non-opaque destination surface: gracefully fall back to scalar coverage SrcOver
                const keel::u32 scalarCov = (covR + covG + covB + 1u) / 3u;
                dstRow[x] = BlendPremulSrcOverScalar(dstPx, color, scalarCov);
                continue;
            }

            if (useGamma) {
                const keel::u32 outLinR = (keel::as<keel::u32>(luts.toLinear12[dr]) * (255u - covR) + srcLinR * covR + 127u) / 255u;
                const keel::u32 outLinG = (keel::as<keel::u32>(luts.toLinear12[dg]) * (255u - covG) + srcLinG * covG + 127u) / 255u;
                const keel::u32 outLinB = (keel::as<keel::u32>(luts.toLinear12[db]) * (255u - covB) + srcLinB * covB + 127u) / 255u;
                const keel::u32 outR = luts.toSrgb8[outLinR];
                const keel::u32 outG = luts.toSrgb8[outLinG];
                const keel::u32 outB = luts.toSrgb8[outLinB];
                dstRow[x] = 0xFF000000u | (outB << 16) | (outG << 8) | outR;
            } else {
                const keel::u32 outR = (dr * (255u - covR) + keel::as<keel::u32>(color.r) * covR + 127u) / 255u;
                const keel::u32 outG = (dg * (255u - covG) + keel::as<keel::u32>(color.g) * covG + 127u) / 255u;
                const keel::u32 outB = (db * (255u - covB) + keel::as<keel::u32>(color.b) * covB + 127u) / 255u;
                dstRow[x] = 0xFF000000u | (outB << 16) | (outG << 8) | outR;
            }
        }
    }
}

} // namespace guikit
