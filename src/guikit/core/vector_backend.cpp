#include "guikit/backend/vector_backend.hpp"

#include <atomic>

namespace guikit {

namespace {

std::atomic<VectorCanvasFactory> g_defaultCanvasFactory{nullptr};
std::atomic<ImageDecoderFn> g_defaultImageDecoder{nullptr};

} // namespace

void RegisterDefaultVectorCanvasFactory(VectorCanvasFactory factory) noexcept {
    g_defaultCanvasFactory.store(factory, std::memory_order_release);
}

bool HasDefaultVectorCanvasFactory() noexcept {
    return g_defaultCanvasFactory.load(std::memory_order_acquire) != nullptr;
}

void RegisterDefaultImageDecoder(ImageDecoderFn decoder) noexcept {
    g_defaultImageDecoder.store(decoder, std::memory_order_release);
}

std::unique_ptr<ICanvas> CreateVectorCanvas(bool loadDefaultFonts) {
    const VectorCanvasFactory factory = g_defaultCanvasFactory.load(std::memory_order_acquire);
    if (factory != nullptr) {
        return factory(loadDefaultFonts);
    }
    return nullptr;
}

std::unique_ptr<ICanvas> CreateOffscreenVectorCanvas() {
    return CreateVectorCanvas(/*loadDefaultFonts=*/false);
}

bool DecodeImageFromMemory(
    std::span<const uint8_t> encodedBytes,
    uint32_t& outWidth,
    uint32_t& outHeight,
    std::vector<uint32_t>& outRgbaPixels
) {
    outWidth = 0;
    outHeight = 0;
    const ImageDecoderFn decoder = g_defaultImageDecoder.load(std::memory_order_acquire);
    if (decoder == nullptr) {
        return false;
    }
    return decoder(encodedBytes, outWidth, outHeight, outRgbaPixels);
}

} // namespace guikit
