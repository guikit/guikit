#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>
#include "guikit/backend/ICanvas.hpp"

namespace guikit {

/// Factory callback signature for creating a 2D vector canvas instance.
using VectorCanvasFactory = std::unique_ptr<ICanvas>(*)(bool loadDefaultFonts);

/// Callback signature for decoding an encoded raster image (PNG / JPEG / WebP) from memory into 32-bit RGBA8888 pixels.
using ImageDecoderFn = bool(*)(
    std::span<const uint8_t> encodedBytes,
    uint32_t& outWidth,
    uint32_t& outHeight,
    std::vector<uint32_t>& outRgbaPixels
);

/// Registers the default `ICanvas` factory used by `CreateVectorCanvas()` and `CreateOffscreenVectorCanvas()`.
void RegisterDefaultVectorCanvasFactory(VectorCanvasFactory factory) noexcept;

/// Returns true if a default vector canvas factory is currently registered.
[[nodiscard]] bool HasDefaultVectorCanvasFactory() noexcept;

/// Registers the default backend-agnostic image decoder callback.
void RegisterDefaultImageDecoder(ImageDecoderFn decoder) noexcept;

/// Creates a primary UI vector canvas instance using the registered default factory (or returns `nullptr` if none is registered).
[[nodiscard]] std::unique_ptr<ICanvas> CreateVectorCanvas(bool loadDefaultFonts = true);

/// Creates a lightweight offscreen vector canvas instance (without scanning disk for default UI fonts),
/// optimized for vector tile rasterization and headless offscreen surfaces.
[[nodiscard]] std::unique_ptr<ICanvas> CreateOffscreenVectorCanvas();

/// Backend-agnostic image decoder: decodes an encoded raster image (PNG / JPEG / WebP) from memory
/// into 32-bit RGBA8888 pixels (`(A << 24) | (B << 16) | (G << 8) | R`).
/// Thread-safe and safe to invoke from background worker threads.
[[nodiscard]] bool DecodeImageFromMemory(
    std::span<const uint8_t> encodedBytes,
    uint32_t& outWidth,
    uint32_t& outHeight,
    std::vector<uint32_t>& outRgbaPixels
);

} // namespace guikit
