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

} // namespace guikit
