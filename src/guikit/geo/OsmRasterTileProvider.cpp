#include "guikit/backend/VectorBackend.hpp"
#include "guikit/geo/ChartDataSource.hpp"
#include "guikit/geo/GisRenderEngine.hpp"
#include "guikit/net/NetClient.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace guikit::geo {

bool DecodePngAndScaleToRgba(
    std::span<const uint8_t> pngBytes,
    uint32_t targetSizePx,
    std::vector<uint32_t>& outRgba
) {
    if (pngBytes.size() < 8 || targetSizePx == 0 || targetSizePx > 4096) {
        return false;
    }

    uint32_t srcW = 0;
    uint32_t srcH = 0;
    std::vector<uint32_t> srcRgba;
    if (!guikit::DecodeImageFromMemory(pngBytes, srcW, srcH, srcRgba) ||
        srcW == 0 || srcH == 0 || srcRgba.size() != static_cast<size_t>(srcW) * srcH) {
        return false;
    }

    // Fast path: 1:1 native tile resolution (e.g. 256x256 -> 256x256)
    if (srcW == targetSizePx && srcH == targetSizePx) {
        outRgba = std::move(srcRgba);
        return true;
    }

    outRgba.resize(static_cast<size_t>(targetSizePx) * targetSizePx);

    // Fast 2x2 box filter path when downscaling by exact 2x (e.g. 256x256 -> 128x128)
    if (srcW == targetSizePx * 2u && srcH == targetSizePx * 2u) {
        for (uint32_t dy = 0; dy < targetSizePx; ++dy) {
            const uint32_t sy0 = dy * 2u;
            const uint32_t sy1 = sy0 + 1u;
            const uint32_t* row0 = srcRgba.data() + static_cast<size_t>(sy0) * srcW;
            const uint32_t* row1 = srcRgba.data() + static_cast<size_t>(sy1) * srcW;
            uint32_t* dstRow = outRgba.data() + static_cast<size_t>(dy) * targetSizePx;

            for (uint32_t dx = 0; dx < targetSizePx; ++dx) {
                const uint32_t sx0 = dx * 2u;
                const uint32_t sx1 = sx0 + 1u;

                const uint32_t p00 = row0[sx0];
                const uint32_t p10 = row0[sx1];
                const uint32_t p01 = row1[sx0];
                const uint32_t p11 = row1[sx1];

                const uint32_t r = (((p00 >>  0u) & 0xFFu) + ((p10 >>  0u) & 0xFFu) +
                                    ((p01 >>  0u) & 0xFFu) + ((p11 >>  0u) & 0xFFu) + 2u) >> 2u;
                const uint32_t g = (((p00 >>  8u) & 0xFFu) + ((p10 >>  8u) & 0xFFu) +
                                    ((p01 >>  8u) & 0xFFu) + ((p11 >>  8u) & 0xFFu) + 2u) >> 2u;
                const uint32_t b = (((p00 >> 16u) & 0xFFu) + ((p10 >> 16u) & 0xFFu) +
                                    ((p01 >> 16u) & 0xFFu) + ((p11 >> 16u) & 0xFFu) + 2u) >> 2u;
                const uint32_t a = (((p00 >> 24u) & 0xFFu) + ((p10 >> 24u) & 0xFFu) +
                                    ((p01 >> 24u) & 0xFFu) + ((p11 >> 24u) & 0xFFu) + 2u) >> 2u;

                dstRow[dx] = (a << 24u) | (b << 16u) | (g << 8u) | r;
            }
        }
        return true;
    }

    // General bilinear resampling (e.g. 256x256 -> 512x512 or arbitrary dimensions)
    const float scaleX = static_cast<float>(srcW) / static_cast<float>(targetSizePx);
    const float scaleY = static_cast<float>(srcH) / static_cast<float>(targetSizePx);

    for (uint32_t dy = 0; dy < targetSizePx; ++dy) {
        const float srcYf = (static_cast<float>(dy) + 0.5f) * scaleY - 0.5f;
        const int y0 = std::clamp(static_cast<int>(std::floor(srcYf)), 0, static_cast<int>(srcH) - 1);
        const int y1 = std::clamp(y0 + 1, 0, static_cast<int>(srcH) - 1);
        const float fy = std::clamp(srcYf - static_cast<float>(y0), 0.0f, 1.0f);
        const float invFy = 1.0f - fy;

        const uint32_t* row0 = srcRgba.data() + static_cast<size_t>(y0) * srcW;
        const uint32_t* row1 = srcRgba.data() + static_cast<size_t>(y1) * srcW;
        uint32_t* dstRow = outRgba.data() + static_cast<size_t>(dy) * targetSizePx;

        for (uint32_t dx = 0; dx < targetSizePx; ++dx) {
            const float srcXf = (static_cast<float>(dx) + 0.5f) * scaleX - 0.5f;
            const int x0 = std::clamp(static_cast<int>(std::floor(srcXf)), 0, static_cast<int>(srcW) - 1);
            const int x1 = std::clamp(x0 + 1, 0, static_cast<int>(srcW) - 1);
            const float fx = std::clamp(srcXf - static_cast<float>(x0), 0.0f, 1.0f);
            const float invFx = 1.0f - fx;

            const float w00 = invFx * invFy;
            const float w10 = fx * invFy;
            const float w01 = invFx * fy;
            const float w11 = fx * fy;

            const uint32_t p00 = row0[x0];
            const uint32_t p10 = row0[x1];
            const uint32_t p01 = row1[x0];
            const uint32_t p11 = row1[x1];

            const auto sampleChannel = [&](uint32_t shift) noexcept -> uint32_t {
                const float val = w00 * static_cast<float>((p00 >> shift) & 0xFFu) +
                                  w10 * static_cast<float>((p10 >> shift) & 0xFFu) +
                                  w01 * static_cast<float>((p01 >> shift) & 0xFFu) +
                                  w11 * static_cast<float>((p11 >> shift) & 0xFFu);
                return static_cast<uint32_t>(std::clamp(static_cast<int>(val + 0.5f), 0, 255));
            };

            const uint32_t r = sampleChannel(0u);
            const uint32_t g = sampleChannel(8u);
            const uint32_t b = sampleChannel(16u);
            const uint32_t a = sampleChannel(24u);

            dstRow[dx] = (a << 24u) | (b << 16u) | (g << 8u) | r;
        }
    }

    return true;
}

namespace {

class OsmRasterTileProvider final : public IRasterTileProvider {
public:
    explicit OsmRasterTileProvider(std::string cacheDir)
        : m_cacheDir(std::move(cacheDir))
        , m_httpClient("Hackionics-MarineGIS/1.0 (+https://openstreetmap.org)") {
        if (m_cacheDir.empty()) {
            m_cacheDir = "cache/osm_tiles";
        }
    }

    [[nodiscard]] std::string_view GetSourceId() const noexcept override {
        return "osm_standard";
    }

    [[nodiscard]] std::string_view GetDisplayName() const noexcept override {
        return "OpenStreetMap Standard";
    }

    [[nodiscard]] int GetMinZoom() const noexcept override { return 0; }
    [[nodiscard]] int GetMaxZoom() const noexcept override { return 19; }

    [[nodiscard]] std::shared_ptr<const RasterTileImage> TryGetCachedTile(
        const SlippyTileKey& key
    ) const noexcept override {
        const SlippyTileKey normKey = NormalizeKey(key);
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_ramCache.find(normKey);
        if (it == m_ramCache.end() || !it->second.image) {
            return nullptr;
        }

        // If this entry was synthesized while offline, expire it once the 15s circuit breaker window elapses
        if (it->second.isFallback) {
            const int64_t nowMs = NowSteadyMs();
            if ((nowMs - it->second.createdAtMs) >= kOfflineRetryIntervalMs) {
                return nullptr;
            }
        }

        it->second.lastAccess = m_accessCounter.fetch_add(1u, std::memory_order_relaxed);
        return it->second.image;
    }

    [[nodiscard]] std::shared_ptr<const RasterTileImage> FetchTileBlocking(
        const SlippyTileKey& key
    ) override {
        const SlippyTileKey normKey = NormalizeKey(key);

        if (auto cached = TryGetCachedTile(normKey)) {
            return cached;
        }

        // 1. Try persistent disk cache or cross-platform HTTPS fetch via guikit::net::HttpClient
        std::vector<uint8_t> pngBytes = LoadOrFetchOsmPng(normKey.z, normKey.x, normKey.y);

        auto tileImg = std::make_shared<RasterTileImage>();
        tileImg->key = normKey;
        tileImg->width = normKey.tileSizePx;
        tileImg->height = normKey.tileSizePx;

        bool decoded = false;
        if (!pngBytes.empty()) {
            decoded = DecodePngAndScaleToRgba(pngBytes, normKey.tileSizePx, tileImg->rgbaPixels);
        }

        // 2. Clean cartographic fallback if offline or in headless test environment without internet
        const bool isFallback =
            !decoded || tileImg->rgbaPixels.size() != static_cast<size_t>(normKey.tileSizePx) * normKey.tileSizePx;
        if (isFallback) {
            SynthesizeCartographicFallbackTile(normKey, tileImg->rgbaPixels);
        }

        const int64_t nowMs = NowSteadyMs();
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            if (m_ramCache.size() >= 512 && m_ramCache.find(normKey) == m_ramCache.end()) {
                uint64_t oldestTick = std::numeric_limits<uint64_t>::max();
                SlippyTileKey oldestKey{};
                bool foundOldest = false;
                for (const auto& [k, v] : m_ramCache) {
                    if (v.lastAccess < oldestTick) {
                        oldestTick = v.lastAccess;
                        oldestKey = k;
                        foundOldest = true;
                    }
                }
                if (foundOldest) {
                    m_ramCache.erase(oldestKey);
                }
            }
            m_ramCache[normKey] = CacheEntry{
                .image = tileImg,
                .lastAccess = m_accessCounter.fetch_add(1u, std::memory_order_relaxed),
                .createdAtMs = nowMs,
                .isFallback = isFallback
            };
        }

        return tileImg;
    }

    void ClearCache() noexcept override {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_ramCache.clear();
    }

private:
    static constexpr int64_t kOfflineRetryIntervalMs = 15000;

    struct CacheEntry {
        std::shared_ptr<const RasterTileImage> image{};
        uint64_t lastAccess{0};
        int64_t createdAtMs{0};
        bool isFallback{false};
    };

    [[nodiscard]] static int64_t NowSteadyMs() noexcept {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    [[nodiscard]] static SlippyTileKey NormalizeKey(const SlippyTileKey& k) noexcept {
        const int z = std::clamp(k.z, 0, 19);
        const int n = 1 << z;
        const int x = ((k.x % n) + n) % n;
        const int y = std::clamp(k.y, 0, n - 1);
        const uint32_t sz = (k.tileSizePx <= 160u) ? 128u : ((k.tileSizePx >= 384u) ? 512u : 256u);
        return SlippyTileKey{.z = z, .x = x, .y = y, .tileSizePx = sz};
    }

    [[nodiscard]] std::vector<uint8_t> LoadOrFetchOsmPng(int z, int x, int y) const {
        const std::filesystem::path diskPath =
            std::filesystem::path(m_cacheDir) / std::to_string(z) / std::to_string(x) / (std::to_string(y) + ".png");

        std::error_code ec;
        if (std::filesystem::exists(diskPath, ec)) {
            std::ifstream in(diskPath, std::ios::binary | std::ios::ate);
            if (in) {
                const auto fileSize = in.tellg();
                if (fileSize > 32 && fileSize < 4 * 1024 * 1024) {
                    std::vector<uint8_t> buf(static_cast<size_t>(fileSize));
                    in.seekg(0, std::ios::beg);
                    if (in.read(reinterpret_cast<char*>(buf.data()), fileSize)) {
                        if (buf.size() >= 8 && buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') {
                            return buf;
                        }
                    }
                }
            }
        }

        // Circuit breaker: if network failed in the last 15 seconds, avoid blocking worker threads
        const int64_t nowMs = NowSteadyMs();
        const int64_t lastFailMs = m_lastNetworkFailureMs.load(std::memory_order_relaxed);
        if (lastFailMs > 0 && (nowMs - lastFailMs) < kOfflineRetryIntervalMs) {
            return {};
        }

        const std::string url = std::format("https://tile.openstreetmap.org/{}/{}/{}.png", z, x, y);
        guikit::net::HttpRequest req{};
        req.url = url;
        req.method = guikit::net::HttpMethod::Get;
        req.userAgent = "Hackionics-MarineGIS/1.0 (+https://openstreetmap.org)";
        req.connectTimeoutMs = 2000;
        req.readTimeoutMs = 2500;
        req.maxResponseBytes = 4u * 1024u * 1024u;

        const guikit::net::HttpResponse resp = m_httpClient.Execute(req);
        if (resp.Succeeded() && resp.body.size() >= 32 &&
            resp.body[0] == 0x89 && resp.body[1] == 'P' && resp.body[2] == 'N' && resp.body[3] == 'G') {
            std::filesystem::create_directories(diskPath.parent_path(), ec);

            // Atomic write via temporary file + rename so concurrent readers never observe partial PNGs
            const uint64_t tmpId = m_accessCounter.fetch_add(1u, std::memory_order_relaxed);
            const std::filesystem::path tmpPath =
                diskPath.parent_path() / std::format("{}.tmp.{}.png", y, tmpId);
            {
                std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
                if (out) {
                    out.write(
                        reinterpret_cast<const char*>(resp.body.data()),
                        static_cast<std::streamsize>(resp.body.size())
                    );
                    out.flush();
                }
            }
            std::filesystem::rename(tmpPath, diskPath, ec);
            if (ec) {
                std::filesystem::remove(tmpPath, ec);
            }
            return resp.body;
        }

        m_lastNetworkFailureMs.store(nowMs, std::memory_order_relaxed);
        return {};
    }

    static void SynthesizeCartographicFallbackTile(
        const SlippyTileKey& key,
        std::vector<uint32_t>& outRgba
    ) {
        const uint32_t sz = key.tileSizePx;
        outRgba.resize(static_cast<size_t>(sz) * sz);

        const bool evenChecker = ((key.x + key.y) & 1) == 0;
        const uint8_t baseR = evenChecker ? 236 : 231;
        const uint8_t baseG = evenChecker ? 233 : 228;
        const uint8_t baseB = evenChecker ? 222 : 216;
        const uint32_t subStep = std::max(16u, sz / 4u);

        for (uint32_t py = 0; py < sz; ++py) {
            for (uint32_t px = 0; px < sz; ++px) {
                uint8_t r = baseR;
                uint8_t g = baseG;
                uint8_t b = baseB;

                if (px == 0 || py == 0) {
                    r = 188; g = 198; b = 208;
                } else if ((px % subStep) == 0 || (py % subStep) == 0) {
                    r = 216; g = 214; b = 204;
                }
                const size_t idx = static_cast<size_t>(py) * sz + px;
                outRgba[idx] = (static_cast<uint32_t>(255u) << 24u) |
                               (static_cast<uint32_t>(b) << 16u) |
                               (static_cast<uint32_t>(g) << 8u) |
                               static_cast<uint32_t>(r);
            }
        }
    }

    std::string m_cacheDir;
    guikit::net::HttpClient m_httpClient;
    mutable std::mutex m_mutex{};
    mutable std::unordered_map<SlippyTileKey, CacheEntry, SlippyTileKeyHash> m_ramCache{};
    mutable std::atomic<uint64_t> m_accessCounter{1};
    mutable std::atomic<int64_t> m_lastNetworkFailureMs{0};
};

} // namespace

std::shared_ptr<IRasterTileProvider> CreateOsmRasterTileProvider(std::string cacheDir) {
    auto provider = std::make_shared<OsmRasterTileProvider>(std::move(cacheDir));
    if (!GisRenderEngine::GetDefaultBasemapProvider()) {
        GisRenderEngine::SetDefaultBasemapProvider(provider);
    }
    return provider;
}

} // namespace guikit::geo
