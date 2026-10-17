#pragma once

#include <cstdint>
#include <memory>
#include "guikit/backend/ICanvas.hpp"

namespace guikit {

/// Supported 2D vector rasterization engine backends.
enum class VectorBackendType : uint8_t {
    Auto   = 0, ///< Automatically select the default compiled vector backend
    ThorVG = 1  ///< ThorVG software vector rasterizer (SIMD scanline RLE)
};

/// Creates a primary UI vector canvas instance for the specified backend.
[[nodiscard]] std::unique_ptr<ICanvas> CreateVectorCanvas(
    VectorBackendType type = VectorBackendType::Auto,
    bool loadDefaultFonts = true
);

/// Creates a lightweight offscreen vector canvas instance (without scanning disk for default UI fonts),
/// optimized for vector tile rasterization and headless offscreen surfaces.
[[nodiscard]] std::unique_ptr<ICanvas> CreateOffscreenVectorCanvas(
    VectorBackendType type = VectorBackendType::Auto
);

} // namespace guikit
