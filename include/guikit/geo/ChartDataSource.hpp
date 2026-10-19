#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "guikit/core/Types.hpp"
#include "guikit/geo/MapGrid.hpp"

namespace guikit::geo {

/// Slippy Map [z/x/y] tile coordinate with configurable pixel resolution (128, 256, or 512).
struct SlippyTileKey {
    int z{0};
    int x{0};
    int y{0};
    uint32_t tileSizePx{256}; ///< Configurable tile dimension: 128, 256, or 512

    [[nodiscard]] constexpr bool operator==(const SlippyTileKey& o) const noexcept {
        return z == o.z && x == o.x && y == o.y && tileSizePx == o.tileSizePx;
    }
};

/// Hash functor for `SlippyTileKey`.
struct SlippyTileKeyHash {
    [[nodiscard]] size_t operator()(const SlippyTileKey& k) const noexcept {
        uint64_t h = static_cast<uint64_t>(static_cast<uint32_t>(k.z)) * 0x9E3779B97F4A7C15ULL;
        h ^= static_cast<uint64_t>(static_cast<uint32_t>(k.x)) * 0xBF58476D1CE4E5B9ULL + (h << 6) + (h >> 2);
        h ^= static_cast<uint64_t>(static_cast<uint32_t>(k.y)) * 0x94D049BB133111EBULL + (h << 6) + (h >> 2);
        h ^= static_cast<uint64_t>(k.tileSizePx) * 0xFF51AFD7ED558CCDULL + (h << 6) + (h >> 2);
        return static_cast<size_t>(h);
    }
};

/// Decoded 32-bit RGBA8888 raster tile buffer scaled to `key.tileSizePx` (128, 256, or 512).
struct RasterTileImage {
    SlippyTileKey key{};
    uint32_t width{0};
    uint32_t height{0};
    std::vector<uint32_t> rgbaPixels{}; ///< Straight RGBA8888 32-bit pixels (`width * height`)

    [[nodiscard]] bool IsValid() const noexcept {
        return width > 0 && height > 0 &&
               rgbaPixels.size() == static_cast<size_t>(width) * static_cast<size_t>(height);
    }
};

/// Abstract asynchronous-ready Raster Tile Provider interface (e.g. OpenStreetMap XYZ over HTTP,
/// OpenSeaMap, Satellite imagery, local tile directory, or MBTiles/GeoPackage raster pyramid).
/// All blocking network/disk `FetchTileBlocking` calls are executed on background worker threads, never on the UI thread.
class IRasterTileProvider {
public:
    virtual ~IRasterTileProvider() = default;

    /// Unique machine identifier for cache keying (e.g. "osm_standard").
    [[nodiscard]] virtual std::string_view GetSourceId() const noexcept = 0;

    /// Human-readable display name for UI selection (e.g. "OpenStreetMap Standard").
    [[nodiscard]] virtual std::string_view GetDisplayName() const noexcept = 0;

    [[nodiscard]] virtual int GetMinZoom() const noexcept { return 0; }
    [[nodiscard]] virtual int GetMaxZoom() const noexcept { return 19; }

    /// Fast O(1) non-blocking lookup in the provider's in-RAM LRU cache. Never touches disk or network.
    [[nodiscard]] virtual std::shared_ptr<const RasterTileImage> TryGetCachedTile(
        const SlippyTileKey& key
    ) const noexcept = 0;

    /// Fetches and decodes a `[z/x/y]` raster tile scaled to `key.tileSizePx` (128, 256, or 512).
    /// Called on background worker threads in `AsyncChartStreamer`.
    [[nodiscard]] virtual std::shared_ptr<const RasterTileImage> FetchTileBlocking(
        const SlippyTileKey& key
    ) = 0;

    /// Clears any in-RAM tile caches held by this provider.
    virtual void ClearCache() noexcept {}
};

/// Creates a platform-independent fallback cartographic raster tile provider (128, 256, or 512 px).
[[nodiscard]] std::shared_ptr<IRasterTileProvider> CreateCartographicBasemapTileProvider();

/// Creates a cross-platform OpenStreetMap (XYZ) raster tile provider with RAM + disk cache,
/// HTTP/HTTPS fetching via `guikit::net::HttpClient`, and LodePNG decoding/scaling.
[[nodiscard]] std::shared_ptr<IRasterTileProvider> CreateOsmRasterTileProvider(
    std::string cacheDir = "cache/osm_tiles"
);

/// Cross-platform PNG decoder and bilinear/box resampler to RGBA8888 (using LodePNG).
[[nodiscard]] bool DecodePngAndScaleToRgba(
    std::span<const uint8_t> pngBytes,
    uint32_t targetSizePx,
    std::vector<uint32_t>& outRgba
);

} // namespace guikit::geo

namespace guikit {
using namespace geo;
} // namespace guikit
