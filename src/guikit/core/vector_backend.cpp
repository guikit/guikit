#include "guikit/backend/vector_backend.hpp"

#include "keel/bits.h"
#include "keel/math.h"
#include "keel/memory.h"
#if defined(GUIKIT_HAS_KEEL_IMAGEFILE) && GUIKIT_HAS_KEEL_IMAGEFILE
#include "keel/imagefile.h"
#endif

#if defined(_MSC_VER) && !defined(__clang__)
extern "C" void _ReadWriteBarrier();
#pragma intrinsic(_ReadWriteBarrier)
#endif

namespace guikit {

namespace {

static constexpr keel::u32 kMaxDecodedImageDim = 4096u;

VectorCanvasFactory g_defaultCanvasFactory = nullptr;
ImageDecoderFn g_defaultImageDecoder = nullptr;

template <typename FnPtr>
inline void AtomicStorePtr(FnPtr& slot, FnPtr val) noexcept {
#if defined(_MSC_VER) && !defined(__clang__)
    _ReadWriteBarrier();
    *static_cast<volatile FnPtr*>(&slot) = val;
    _ReadWriteBarrier();
#else
    __atomic_store_n(&slot, val, __ATOMIC_RELEASE);
#endif
}

template <typename FnPtr>
[[nodiscard]] inline FnPtr AtomicLoadPtr(const FnPtr& slot) noexcept {
#if defined(_MSC_VER) && !defined(__clang__)
    const FnPtr v = *static_cast<const volatile FnPtr*>(&slot);
    _ReadWriteBarrier();
    return v;
#else
    return __atomic_load_n(&slot, __ATOMIC_ACQUIRE);
#endif
}

#if defined(GUIKIT_HAS_KEEL_IMAGEFILE) && GUIKIT_HAS_KEEL_IMAGEFILE
[[nodiscard]] inline keel::u32 PackRgba8(keel::u8 r, keel::u8 g, keel::u8 b, keel::u8 a) noexcept {
    return keel::as<keel::u32>(r) |
           (keel::as<keel::u32>(g) << 8) |
           (keel::as<keel::u32>(b) << 16) |
           (keel::as<keel::u32>(a) << 24);
}

[[nodiscard]] inline keel::u8 ReadSampleAsU8(
    const keel::u8* base,
    keel::usize sampleIdx,
    keel::imagefile::SampleType st
) noexcept {
    using keel::imagefile::SampleType;
    switch (st) {
        case SampleType::U8:
            return base[sampleIdx];
        case SampleType::U16: {
            keel::u16 v = 0u;
            keel::mem_copy(&v, base + sampleIdx * 2u, sizeof(v));
            return keel::as<keel::u8>(v >> 8);
        }
        case SampleType::U32: {
            keel::u32 v = 0u;
            keel::mem_copy(&v, base + sampleIdx * 4u, sizeof(v));
            return keel::as<keel::u8>(v >> 24);
        }
        case SampleType::I8: {
            keel::i8 v = 0;
            keel::mem_copy(&v, base + sampleIdx, sizeof(v));
            return keel::saturate<keel::u8>(keel::as<keel::i32>(v) + 128);
        }
        case SampleType::I16: {
            keel::i16 v = 0;
            keel::mem_copy(&v, base + sampleIdx * 2u, sizeof(v));
            return keel::saturate<keel::u8>((keel::as<keel::i32>(v) + 32768) >> 8);
        }
        case SampleType::I32: {
            keel::i32 v = 0;
            keel::mem_copy(&v, base + sampleIdx * 4u, sizeof(v));
            return keel::saturate<keel::u8>((keel::as<keel::i64>(v) + 2147483648LL) >> 24);
        }
        case SampleType::F32: {
            keel::f32 v = 0.0f;
            keel::mem_copy(&v, base + sampleIdx * 4u, sizeof(v));
            return keel::saturate<keel::u8>(keel::round_i32(keel::clamp(v, 0.0f, 1.0f) * 255.0f));
        }
        case SampleType::F64: {
            keel::f64 v = 0.0;
            keel::mem_copy(&v, base + sampleIdx * 8u, sizeof(v));
            return keel::saturate<keel::u8>(keel::round_i32(keel::clamp(v, 0.0, 1.0) * 255.0));
        }
    }
    return 0u;
}

[[nodiscard]] bool DecodeViaKeelImagefile(
    keel::ArrayRef<keel::u8> encodedBytes,
    keel::u32& outWidth,
    keel::u32& outHeight,
    keel::Array<keel::u32>& outRgbaPixels
) {
    keel::imagefile::ImageDecodeOptions opts{};
    opts.max_width = kMaxDecodedImageDim;
    opts.max_height = kMaxDecodedImageDim;
    opts.expand_indexed = true;

    auto res = keel::imagefile::decode_image(encodedBytes, opts);
    if (!res) {
        return false;
    }

    const keel::imagefile::Image& img = *res;
    if (img.empty() || img.width == 0u || img.height == 0u ||
        img.width > kMaxDecodedImageDim || img.height > kMaxDecodedImageDim ||
        img.channels == 0u) {
        return false;
    }

    keel::usize totalPx = 0u;
    if (!keel::checked_mul(keel::as<keel::usize>(img.width), keel::as<keel::usize>(img.height), totalPx)) {
        return false;
    }

    const keel::usize sampleBytes = keel::imagefile::sample_byte_size(img.sample_type);
    const keel::usize neededBytes = totalPx * keel::as<keel::usize>(img.channels) * sampleBytes;
    if (img.pixels.size() < neededBytes) {
        return false;
    }

    outRgbaPixels.resize(totalPx);
    const keel::u8* src = img.pixels.data();
    keel::u32* dst = outRgbaPixels.data();

    if (img.sample_type == keel::imagefile::SampleType::U8) {
        if (img.channels == 4u) {
#if !defined(__BYTE_ORDER__) || (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
            keel::mem_copy(dst, src, totalPx * sizeof(keel::u32));
#else
            for (keel::usize i = 0u; i < totalPx; ++i) {
                dst[i] = keel::read_le32(src + i * 4u);
            }
#endif
        } else if (img.channels == 3u) {
            for (keel::usize i = 0u; i < totalPx; ++i) {
                const keel::usize off = i * 3u;
                dst[i] = PackRgba8(src[off], src[off + 1u], src[off + 2u], 255u);
            }
        } else if (img.channels == 2u) {
            for (keel::usize i = 0u; i < totalPx; ++i) {
                const keel::u8 g = src[i * 2u];
                const keel::u8 a = src[i * 2u + 1u];
                dst[i] = PackRgba8(g, g, g, a);
            }
        } else if (img.channels == 1u) {
            for (keel::usize i = 0u; i < totalPx; ++i) {
                const keel::u8 g = src[i];
                dst[i] = PackRgba8(g, g, g, 255u);
            }
        } else {
            const keel::usize ch = keel::as<keel::usize>(img.channels);
            for (keel::usize i = 0u; i < totalPx; ++i) {
                const keel::usize off = i * ch;
                dst[i] = PackRgba8(src[off], src[off + 1u], src[off + 2u], src[off + 3u]);
            }
        }
    } else {
        const keel::usize ch = keel::as<keel::usize>(img.channels);
        for (keel::usize i = 0u; i < totalPx; ++i) {
            const keel::usize baseIdx = i * ch;
            if (ch == 1u) {
                const keel::u8 g = ReadSampleAsU8(src, baseIdx, img.sample_type);
                dst[i] = PackRgba8(g, g, g, 255u);
            } else if (ch == 2u) {
                const keel::u8 g = ReadSampleAsU8(src, baseIdx, img.sample_type);
                const keel::u8 a = ReadSampleAsU8(src, baseIdx + 1u, img.sample_type);
                dst[i] = PackRgba8(g, g, g, a);
            } else if (ch == 3u) {
                const keel::u8 r = ReadSampleAsU8(src, baseIdx, img.sample_type);
                const keel::u8 g = ReadSampleAsU8(src, baseIdx + 1u, img.sample_type);
                const keel::u8 b = ReadSampleAsU8(src, baseIdx + 2u, img.sample_type);
                dst[i] = PackRgba8(r, g, b, 255u);
            } else {
                const keel::u8 r = ReadSampleAsU8(src, baseIdx, img.sample_type);
                const keel::u8 g = ReadSampleAsU8(src, baseIdx + 1u, img.sample_type);
                const keel::u8 b = ReadSampleAsU8(src, baseIdx + 2u, img.sample_type);
                const keel::u8 a = ReadSampleAsU8(src, baseIdx + 3u, img.sample_type);
                dst[i] = PackRgba8(r, g, b, a);
            }
        }
    }

    outWidth = img.width;
    outHeight = img.height;
    return true;
}
#endif

} // namespace

void RegisterDefaultVectorCanvasFactory(VectorCanvasFactory factory) noexcept {
    AtomicStorePtr(g_defaultCanvasFactory, factory);
}

bool HasDefaultVectorCanvasFactory() noexcept {
    return AtomicLoadPtr(g_defaultCanvasFactory) != nullptr;
}

void RegisterDefaultImageDecoder(ImageDecoderFn decoder) noexcept {
    AtomicStorePtr(g_defaultImageDecoder, decoder);
}

keel::UniquePtr<ICanvas> CreateVectorCanvas(bool loadDefaultFonts) {
    const VectorCanvasFactory factory = AtomicLoadPtr(g_defaultCanvasFactory);
    if (factory != nullptr) {
        return factory(loadDefaultFonts);
    }
    return nullptr;
}

keel::UniquePtr<ICanvas> CreateOffscreenVectorCanvas() {
    return CreateVectorCanvas(/*loadDefaultFonts=*/false);
}

bool InspectImageFromMemory(
    keel::ArrayRef<keel::u8> encodedBytes,
    keel::u32& outWidth,
    keel::u32& outHeight
) noexcept {
    outWidth = 0u;
    outHeight = 0u;
    if (encodedBytes.size() < 4u) {
        return false;
    }
#if defined(GUIKIT_HAS_KEEL_IMAGEFILE) && GUIKIT_HAS_KEEL_IMAGEFILE
    if (auto info = keel::imagefile::peek_image(encodedBytes)) {
        if (info->width > 0u && info->height > 0u &&
            info->width <= kMaxDecodedImageDim && info->height <= kMaxDecodedImageDim) {
            outWidth = info->width;
            outHeight = info->height;
            return true;
        }
    }
#endif
    return false;
}

bool DecodeImageFromMemory(
    keel::ArrayRef<keel::u8> encodedBytes,
    keel::u32& outWidth,
    keel::u32& outHeight,
    keel::Array<keel::u32>& outRgbaPixels
) {
    outWidth = 0u;
    outHeight = 0u;
    if (encodedBytes.size() < 4u) {
        return false;
    }
#if defined(GUIKIT_HAS_KEEL_IMAGEFILE) && GUIKIT_HAS_KEEL_IMAGEFILE
    if (DecodeViaKeelImagefile(encodedBytes, outWidth, outHeight, outRgbaPixels)) {
        return true;
    }
#endif
    const ImageDecoderFn decoder = AtomicLoadPtr(g_defaultImageDecoder);
    if (decoder == nullptr) {
        return false;
    }
    return decoder(encodedBytes, outWidth, outHeight, outRgbaPixels);
}

void ICanvas::DrawArc(
    Vec2 center,
    keel::f32 radius,
    keel::f32 startAngleRad,
    keel::f32 endAngleRad,
    Color color,
    keel::f32 strokeWidth,
    bool roundCaps
) {
    if (color.a == 0u || radius <= 0.25f || strokeWidth <= 0.1f) {
        return;
    }
    keel::f32 a0 = startAngleRad;
    keel::f32 a1 = endAngleRad;
    if (a1 < a0) {
        keel::swap(a0, a1);
    }
    const keel::f32 span = a1 - a0;
    if (span < 1e-4f) {
        return;
    }

    const keel::f32 halfT = strokeWidth * 0.5f;
    const keel::f32 rOuter = radius + halfT;
    const keel::f32 rInner = keel::max(0.25f, radius - halfT);

    if (span >= keel::kTauF - 1e-4f) {
        DrawRoundRect(
            Rect(center.x - rOuter, center.y - rOuter, rOuter * 2.0f, rOuter * 2.0f),
            rOuter,
            Color(0, 0, 0, 0),
            color,
            strokeWidth
        );
        return;
    }

    // Split circular arc into <= 90 deg exact cubic Bezier segments with optional integrated semicircular caps
    const keel::i32 nSegs = keel::clamp(keel::ceil_i32(span / (keel::kPiF * 0.5f)), 1, 8);
    const keel::f32 dTheta = span / keel::as<keel::f32>(nSegs);
    const keel::f32 kArc = (4.0f / 3.0f) * keel::tan(dTheta * 0.25f);
    static constexpr keel::f32 kQuadCap = 0.55228475f;

    VectorPath path;
    path.Reserve(keel::as<keel::usize>(nSegs * 2 + 8), keel::as<keel::usize>(nSegs * 6 + 18));

    const Vec2 uStart(keel::cos(a0), keel::sin(a0));
    const Vec2 tStart(-uStart.y, uStart.x);
    const Vec2 uEnd(keel::cos(a1), keel::sin(a1));
    const Vec2 tEnd(-uEnd.y, uEnd.x);

    path.MoveTo(center + uStart * rOuter);
    for (keel::i32 i = 0; i < nSegs; ++i) {
        const keel::f32 ang0 = a0 + dTheta * keel::as<keel::f32>(i);
        const keel::f32 ang1 = a0 + dTheta * keel::as<keel::f32>(i + 1);
        const Vec2 u0(keel::cos(ang0), keel::sin(ang0));
        const Vec2 u1(keel::cos(ang1), keel::sin(ang1));
        const Vec2 t0(-u0.y, u0.x);
        const Vec2 t1(-u1.y, u1.x);
        const Vec2 p0 = center + u0 * rOuter;
        const Vec2 p3 = center + u1 * rOuter;
        path.CubicTo(p0 + t0 * (kArc * rOuter), p3 - t1 * (kArc * rOuter), p3);
    }

    const keel::f32 capR = (rOuter - rInner) * 0.5f;
    const keel::f32 capMidR = (rOuter + rInner) * 0.5f;
    if (roundCaps && capR > 0.1f) {
        const Vec2 capC = center + uEnd * capMidR;
        const Vec2 o1 = capC + uEnd * capR;
        const Vec2 apex1 = capC + tEnd * capR;
        const Vec2 i1 = capC - uEnd * capR;
        path.CubicTo(o1 + tEnd * (kQuadCap * capR), apex1 + uEnd * (kQuadCap * capR), apex1);
        path.CubicTo(apex1 - uEnd * (kQuadCap * capR), i1 + tEnd * (kQuadCap * capR), i1);
    } else {
        path.LineTo(center + uEnd * rInner);
    }

    for (keel::i32 i = nSegs - 1; i >= 0; --i) {
        const keel::f32 ang0 = a0 + dTheta * keel::as<keel::f32>(i);
        const keel::f32 ang1 = a0 + dTheta * keel::as<keel::f32>(i + 1);
        const Vec2 u0(keel::cos(ang0), keel::sin(ang0));
        const Vec2 u1(keel::cos(ang1), keel::sin(ang1));
        const Vec2 t0(-u0.y, u0.x);
        const Vec2 t1(-u1.y, u1.x);
        const Vec2 p0 = center + u1 * rInner;
        const Vec2 p3 = center + u0 * rInner;
        path.CubicTo(p0 - t1 * (kArc * rInner), p3 + t0 * (kArc * rInner), p3);
    }

    if (roundCaps && capR > 0.1f) {
        const Vec2 capC = center + uStart * capMidR;
        const Vec2 i0 = capC - uStart * capR;
        const Vec2 apex0 = capC - tStart * capR;
        const Vec2 o0 = capC + uStart * capR;
        path.CubicTo(i0 - tStart * (kQuadCap * capR), apex0 - uStart * (kQuadCap * capR), apex0);
        path.CubicTo(apex0 + uStart * (kQuadCap * capR), o0 - tStart * (kQuadCap * capR), o0);
    }
    path.Close();

    PathPaint paint{};
    paint.fillColor = color;
    paint.renderFill = true;
    DrawPath(path, paint);
}

void ICanvas::DrawShadow(
    Rect rect,
    keel::f32 cornerRadius,
    keel::f32 blurRadius,
    Vec2 offset,
    Color color,
    keel::f32 spread
) {
    if (color.a == 0u || rect.w <= 0.0f || rect.h <= 0.0f) {
        return;
    }
    const Rect base(
        rect.x + offset.x - spread,
        rect.y + offset.y - spread,
        rect.w + spread * 2.0f,
        rect.h + spread * 2.0f
    );
    if (base.w <= 0.0f || base.h <= 0.0f) {
        return;
    }
    const keel::f32 blur = keel::max(0.0f, blurRadius);
    if (blur < 1.0f) {
        DrawRoundRect(base, keel::max(0.0f, cornerRadius + spread), color);
        return;
    }

    // Soft 3-layer concentric fallback for software/vector canvases without a Gaussian shadow shader
    const keel::f32 baseAlpha = keel::as<keel::f32>(color.a) / 255.0f;
    static constexpr keel::f32 kExpands[3] = {0.55f, 0.28f, 0.08f};
    static constexpr keel::f32 kWeights[3] = {0.22f, 0.34f, 0.44f};
    for (keel::usize i = 0u; i < 3u; ++i) {
        const keel::f32 pad = blur * kExpands[i];
        const keel::u8 a = keel::as<keel::u8>(keel::clamp(keel::round_i32(baseAlpha * kWeights[i] * 255.0f), 0, 255));
        if (a == 0u) continue;
        const Rect layer(base.x - pad, base.y - pad, base.w + pad * 2.0f, base.h + pad * 2.0f);
        DrawRoundRect(layer, keel::max(0.0f, cornerRadius + spread + pad * 0.5f), Color(color.r, color.g, color.b, a));
    }
}

void ICanvas::DrawGlassRect(Rect rect, keel::f32 radius, const GlassParams& params) {
    if (rect.w <= 0.0f || rect.h <= 0.0f) {
        return;
    }
    const keel::f32 r = keel::clamp(radius, 0.0f, keel::min(rect.w, rect.h) * 0.5f);
    auto boostFrostAlpha = [&params](Color c) noexcept -> Color {
        if (c.a == 0u) return c;
        const keel::f32 baseA = keel::as<keel::f32>(c.a) / 255.0f;
        const keel::f32 frostBoost = keel::clamp(params.frost, 0.0f, 1.0f) * 0.42f;
        const keel::f32 effA = keel::clamp(baseA + (1.0f - baseA) * frostBoost, 0.0f, 0.94f);
        c.a = keel::as<keel::u8>(keel::round_i32(effA * 255.0f));
        return c;
    };

    const Color fillStart = boostFrostAlpha(params.tint);
    if (params.tintEnd.a > 0u) {
        const Color fillEnd = boostFrostAlpha(params.tintEnd);
        const Gradient fillGrad = params.tintRadial
            ? Gradient::Radial(fillStart, fillEnd)
            : Gradient::Linear(fillStart, fillEnd, params.tintAngleDeg);
        if (params.borderColorEnd.a > 0u && params.borderWidth > 0.0f) {
            const Gradient bdrGrad = Gradient::Linear(params.borderColor, params.borderColorEnd, params.tintAngleDeg);
            DrawGradientRoundRect(rect, r, fillGrad, params.borderColor, params.borderWidth, &bdrGrad);
        } else {
            DrawGradientRoundRect(rect, r, fillGrad, params.borderColor, params.borderWidth, nullptr);
        }
    } else if (params.borderColorEnd.a > 0u && params.borderWidth > 0.0f) {
        const Gradient solidFill = Gradient::Linear(fillStart, fillStart, params.tintAngleDeg);
        const Gradient bdrGrad = Gradient::Linear(params.borderColor, params.borderColorEnd, params.tintAngleDeg);
        DrawGradientRoundRect(rect, r, solidFill, params.borderColor, params.borderWidth, &bdrGrad);
    } else {
        DrawRoundRect(rect, r, fillStart, params.borderColor, params.borderWidth);
    }

    if (params.rimLight > 0.01f && rect.w > 8.0f && rect.h > 4.0f) {
        const keel::f32 inset = keel::clamp(r * 0.65f, 1.5f, rect.w * 0.25f);
        const keel::f32 x0 = rect.x + inset;
        const keel::f32 x1 = rect.Right() - inset;
        if (x1 > x0) {
            const Color rim = params.rimColor.WithAlphaF(keel::clamp(params.rimLight * 0.55f, 0.0f, 0.85f));
            DrawLine(Vec2(x0, rect.y + 0.75f), Vec2(x1, rect.y + 0.75f), rim, 1.0f);
        }
    }
}

void ICanvas::DrawGradientRoundRect(
    Rect rect,
    keel::f32 radius,
    const Gradient& fill,
    Color stroke,
    keel::f32 strokeWidth,
    const Gradient* strokeGradient
) {
    if (rect.w <= 0.0f || rect.h <= 0.0f) {
        return;
    }
    const Color avgFill = fill.IsValid() ? fill.AverageColor() : Color::Transparent();
    const Color avgStroke = (strokeGradient && strokeGradient->IsValid())
        ? strokeGradient->AverageColor()
        : stroke;
    DrawRoundRect(rect, radius, avgFill, avgStroke, strokeWidth);
}

} // namespace guikit
