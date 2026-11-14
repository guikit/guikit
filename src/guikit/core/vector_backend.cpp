#include "guikit/backend/vector_backend.hpp"

#if defined(_MSC_VER) && !defined(__clang__)
extern "C" void _ReadWriteBarrier();
#pragma intrinsic(_ReadWriteBarrier)
#endif

namespace guikit {

namespace {

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

bool DecodeImageFromMemory(
    keel::ArrayRef<uint8_t> encodedBytes,
    uint32_t& outWidth,
    uint32_t& outHeight,
    keel::Array<uint32_t>& outRgbaPixels
) {
    outWidth = 0;
    outHeight = 0;
    const ImageDecoderFn decoder = AtomicLoadPtr(g_defaultImageDecoder);
    if (decoder == nullptr) {
        return false;
    }
    return decoder(encodedBytes, outWidth, outHeight, outRgbaPixels);
}

} // namespace guikit
