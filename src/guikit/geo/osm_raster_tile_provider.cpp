#include "guikit/backend/vector_backend.hpp"
#include "guikit/geo/chart_data_source.hpp"
#include "guikit/geo/gis_render_engine.hpp"

#include <atomic>
#if defined(__GLIBC__)
#include <malloc.h>
#endif
#include <mutex>
#include "keel/bits.h"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/fs.h"
#include "keel/http.h"
#include "keel/lru_cache.h"
#include "keel/math.h"
#include "keel/smart_ptr.h"
#include "keel/time.h"

namespace guikit::geo {

bool DecodePngAndScaleToRgba(
    keel::ArrayRef<keel::u8> pngBytes,
    keel::u32 targetSizePx,
    keel::Array<keel::u32>& outRgba
) {
    if (pngBytes.size() < 8 || targetSizePx == 0 || targetSizePx > 4096) {
        return false;
    }

    keel::u32 srcW = 0;
    keel::u32 srcH = 0;
    keel::Array<keel::u32> srcRgba;
    keel::usize srcPx = 0;
    if (!guikit::DecodeImageFromMemory(pngBytes, srcW, srcH, srcRgba) ||
        srcW == 0 || srcH == 0 ||
        !keel::checked_mul<keel::usize>(srcW, srcH, srcPx) ||
        srcRgba.size() != srcPx) {
        return false;
    }

    // Fast path: 1:1 native tile resolution (e.g. 256x256 -> 256x256)
    if (srcW == targetSizePx && srcH == targetSizePx) {
        outRgba = keel::move(srcRgba);
        return true;
    }

    keel::usize targetPx = 0;
    if (!keel::checked_mul<keel::usize>(targetSizePx, targetSizePx, targetPx)) {
        return false;
    }
    outRgba.resize(targetPx);

    // Fast 2x2 box filter path when downscaling by exact 2x (e.g. 256x256 -> 128x128)
    if (srcW == targetSizePx * 2u && srcH == targetSizePx * 2u) {
        for (keel::u32 dy = 0; dy < targetSizePx; ++dy) {
            const keel::u32 sy0 = dy * 2u;
            const keel::u32 sy1 = sy0 + 1u;
            const keel::u32* row0 = srcRgba.data() + static_cast<keel::usize>(sy0) * srcW;
            const keel::u32* row1 = srcRgba.data() + static_cast<keel::usize>(sy1) * srcW;
            keel::u32* dstRow = outRgba.data() + static_cast<keel::usize>(dy) * targetSizePx;

            for (keel::u32 dx = 0; dx < targetSizePx; ++dx) {
                const keel::u32 sx0 = dx * 2u;
                const keel::u32 sx1 = sx0 + 1u;

                const keel::u32 p00 = row0[sx0];
                const keel::u32 p10 = row0[sx1];
                const keel::u32 p01 = row1[sx0];
                const keel::u32 p11 = row1[sx1];

                const keel::u32 r = (((p00 >>  0u) & 0xFFu) + ((p10 >>  0u) & 0xFFu) +
                                    ((p01 >>  0u) & 0xFFu) + ((p11 >>  0u) & 0xFFu) + 2u) >> 2u;
                const keel::u32 g = (((p00 >>  8u) & 0xFFu) + ((p10 >>  8u) & 0xFFu) +
                                    ((p01 >>  8u) & 0xFFu) + ((p11 >>  8u) & 0xFFu) + 2u) >> 2u;
                const keel::u32 b = (((p00 >> 16u) & 0xFFu) + ((p10 >> 16u) & 0xFFu) +
                                    ((p01 >> 16u) & 0xFFu) + ((p11 >> 16u) & 0xFFu) + 2u) >> 2u;
                const keel::u32 a = (((p00 >> 24u) & 0xFFu) + ((p10 >> 24u) & 0xFFu) +
                                    ((p01 >> 24u) & 0xFFu) + ((p11 >> 24u) & 0xFFu) + 2u) >> 2u;

                dstRow[dx] = (a << 24u) | (b << 16u) | (g << 8u) | r;
            }
        }
        return true;
    }

    // General bilinear resampling (e.g. 256x256 -> 512x512 or arbitrary dimensions)
    const float scaleX = static_cast<float>(srcW) / static_cast<float>(targetSizePx);
    const float scaleY = static_cast<float>(srcH) / static_cast<float>(targetSizePx);

    for (keel::u32 dy = 0; dy < targetSizePx; ++dy) {
        const float srcYf = (static_cast<float>(dy) + 0.5f) * scaleY - 0.5f;
        const keel::i32 y0 = keel::clamp(keel::floor_i32(srcYf), 0, static_cast<keel::i32>(srcH) - 1);
        const keel::i32 y1 = keel::clamp(y0 + 1, 0, static_cast<keel::i32>(srcH) - 1);
        const float fy = keel::clamp(srcYf - static_cast<float>(y0), 0.0f, 1.0f);
        const float invFy = 1.0f - fy;

        const keel::u32* row0 = srcRgba.data() + static_cast<keel::usize>(y0) * srcW;
        const keel::u32* row1 = srcRgba.data() + static_cast<keel::usize>(y1) * srcW;
        keel::u32* dstRow = outRgba.data() + static_cast<keel::usize>(dy) * targetSizePx;

        for (keel::u32 dx = 0; dx < targetSizePx; ++dx) {
            const float srcXf = (static_cast<float>(dx) + 0.5f) * scaleX - 0.5f;
            const keel::i32 x0 = keel::clamp(keel::floor_i32(srcXf), 0, static_cast<keel::i32>(srcW) - 1);
            const keel::i32 x1 = keel::clamp(x0 + 1, 0, static_cast<keel::i32>(srcW) - 1);
            const float fx = keel::clamp(srcXf - static_cast<float>(x0), 0.0f, 1.0f);
            const float invFx = 1.0f - fx;

            const float w00 = invFx * invFy;
            const float w10 = fx * invFy;
            const float w01 = invFx * fy;
            const float w11 = fx * fy;

            const keel::u32 p00 = row0[x0];
            const keel::u32 p10 = row0[x1];
            const keel::u32 p01 = row1[x0];
            const keel::u32 p11 = row1[x1];

            const auto sampleChannel = [&](keel::u32 shift) noexcept -> keel::u32 {
                const float val = w00 * static_cast<float>((p00 >> shift) & 0xFFu) +
                                  w10 * static_cast<float>((p10 >> shift) & 0xFFu) +
                                  w01 * static_cast<float>((p01 >> shift) & 0xFFu) +
                                  w11 * static_cast<float>((p11 >> shift) & 0xFFu);
                return static_cast<keel::u32>(keel::sat_cast<keel::u8>(keel::round_i32(val)));
            };

            const keel::u32 r = sampleChannel(0u);
            const keel::u32 g = sampleChannel(8u);
            const keel::u32 b = sampleChannel(16u);
            const keel::u32 a = sampleChannel(24u);

            dstRow[dx] = (a << 24u) | (b << 16u) | (g << 8u) | r;
        }
    }

    return true;
}

namespace {

class OsmRasterTileProvider final : public IRasterTileProvider {
public:
    explicit OsmRasterTileProvider(keel::StringRef cacheDir)
        : m_cacheDir(keel::String::from(cacheDir.empty() ? keel::StringRef("cache/osm_tiles") : cacheDir))
        , m_httpClient("Hackionics-MarineGIS/1.0 (+https://openstreetmap.org)") {}

    [[nodiscard]] keel::StringRef GetSourceId() const noexcept override {
        return "osm_standard";
    }

    [[nodiscard]] keel::StringRef GetDisplayName() const noexcept override {
        return "OpenStreetMap Standard";
    }

    [[nodiscard]] keel::i32 GetMinZoom() const noexcept override { return 0; }
    [[nodiscard]] keel::i32 GetMaxZoom() const noexcept override { return 19; }

    [[nodiscard]] keel::Arc<const RasterTileImage> TryGetCachedTile(
        const SlippyTileKey& key
    ) const noexcept override {
        const SlippyTileKey normKey = NormalizeKey(key);
        std::lock_guard<std::mutex> lk(m_mutex);
        CacheEntry* entry = m_ramCache.get(normKey);
        if (!entry || !entry->image) {
            return {};
        }

        // If this entry was synthesized while offline, expire it once the 15s circuit breaker window elapses
        if (entry->isFallback) {
            const keel::i64 nowMs = NowSteadyMs();
            if ((nowMs - entry->createdAtMs) >= kOfflineRetryIntervalMs) {
                return {};
            }
        }

        return entry->image;
    }

    [[nodiscard]] keel::Arc<const RasterTileImage> FetchTileBlocking(
        const SlippyTileKey& key
    ) override {
        const SlippyTileKey normKey = NormalizeKey(key);

        if (auto cached = TryGetCachedTile(normKey)) {
            return cached;
        }

        // 1. Try persistent disk cache or cross-platform HTTPS fetch via keel::HttpClient
        keel::Array<keel::u8> pngBytes = LoadOrFetchOsmPng(normKey.z, normKey.x, normKey.y);

        auto tileImg = keel::make_arc<RasterTileImage>();
        tileImg->key = normKey;
        tileImg->width = normKey.tileSizePx;
        tileImg->height = normKey.tileSizePx;

        bool decoded = false;
        if (!pngBytes.empty()) {
            decoded = DecodePngAndScaleToRgba(pngBytes, normKey.tileSizePx, tileImg->rgbaPixels);
        }

        // 2. Clean cartographic fallback if offline or in headless test environment without internet
        keel::usize expectedPx = 0;
        const bool isFallback =
            !decoded ||
            !keel::checked_mul<keel::usize>(normKey.tileSizePx, normKey.tileSizePx, expectedPx) ||
            tileImg->rgbaPixels.size() != expectedPx;
        if (isFallback) {
            SynthesizeCartographicFallbackTile(normKey, tileImg->rgbaPixels);
        }

        keel::Arc<const RasterTileImage> constImg = keel::move(tileImg);
        const keel::i64 nowMs = NowSteadyMs();
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_ramCache.put(normKey, CacheEntry{
                .image = constImg,
                .createdAtMs = nowMs,
                .isFallback = isFallback
            });
        }

        return constImg;
    }

    void ClearCache() noexcept override {
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_ramCache.clear();
        }
#if defined(__GLIBC__)
        ::malloc_trim(0);
#endif
    }

private:
    static constexpr keel::i64 kOfflineRetryIntervalMs = 15000;

    struct CacheEntry {
        keel::Arc<const RasterTileImage> image{};
        keel::i64 createdAtMs{0};
        bool isFallback{false};
    };

    [[nodiscard]] static keel::i64 NowSteadyMs() noexcept {
        return static_cast<keel::i64>(keel::monotonic_ms()) + 1;
    }

    [[nodiscard]] static SlippyTileKey NormalizeKey(const SlippyTileKey& k) noexcept {
        const keel::i32 z = keel::clamp(k.z, 0, 19);
        const keel::i32 n = 1 << z;
        const keel::i32 x = ((k.x % n) + n) % n;
        const keel::i32 y = keel::clamp(k.y, 0, n - 1);
        const keel::u32 sz = (k.tileSizePx <= 160u) ? 128u : ((k.tileSizePx >= 384u) ? 512u : 256u);
        return SlippyTileKey{.z = z, .x = x, .y = y, .tileSizePx = sz};
    }

    [[nodiscard]] keel::Array<keel::u8> LoadOrFetchOsmPng(keel::i32 z, keel::i32 x, keel::i32 y) const {
        const auto dirPath = keel::format("%s/%d/%d", m_cacheDir, z, x);
        const auto diskPath = keel::format("%s/%d.png", dirPath.as_ref(), y);
        const keel::StringRef diskPathRef = diskPath.as_ref();

        if (keel::fs_exists(diskPathRef)) {
            if (auto fileRes = keel::fs_read_file(diskPathRef); fileRes.is_ok()) {
                auto& buf = fileRes.value();
                if (buf.size() > 32 && buf.size() < 4 * 1024 * 1024 &&
                    buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') {
                    return keel::move(buf);
                }
            }
        }

        // Circuit breaker: if network failed in the last 15 seconds, avoid blocking worker threads
        const keel::i64 nowMs = NowSteadyMs();
        const keel::i64 lastFailMs = m_lastNetworkFailureMs.load(std::memory_order_relaxed);
        if (lastFailMs > 0 && (nowMs - lastFailMs) < kOfflineRetryIntervalMs) {
            return keel::Array<keel::u8>{};
        }

        char urlBuf[128];
        const auto urlRef = keel::format_buf(urlBuf, "https://tile.openstreetmap.org/%d/%d/%d.png", z, x, y);
        keel::HttpRequest req{};
        req.url = keel::String::from(urlRef);
        req.method = keel::HttpMethod::Get;
        req.user_agent = keel::String::from("Hackionics-MarineGIS/1.0 (+https://openstreetmap.org)");
        req.connect_timeout_ms = 2000;
        req.read_timeout_ms = 2500;
        req.max_response_bytes = 4u * 1024u * 1024u;

        keel::HttpResponse resp = m_httpClient.execute(req);
        if (resp.succeeded() && resp.body.size() >= 32 &&
            resp.body[0] == 0x89 && resp.body[1] == 'P' && resp.body[2] == 'N' && resp.body[3] == 'G') {
            (void)keel::fs_create_dirs(dirPath.as_ref());

            // Atomic write via temporary file + rename so concurrent readers never observe partial PNGs
            const keel::u64 tmpId = m_accessCounter.fetch_add(1u, std::memory_order_relaxed);
            const auto tmpPath = keel::format("%s/%d.tmp.%llu.png", dirPath.as_ref(), y, tmpId);
            const keel::StringRef tmpPathRef = tmpPath.as_ref();
            if (!keel::fs_write_file(tmpPathRef, resp.body)) {
                if (keel::fs_rename(tmpPathRef, diskPathRef)) {
                    (void)keel::fs_remove(tmpPathRef);
                }
            } else {
                (void)keel::fs_remove(tmpPathRef);
            }
            return keel::move(resp.body);
        }

        m_lastNetworkFailureMs.store(nowMs, std::memory_order_relaxed);
        return keel::Array<keel::u8>{};
    }

    static void SynthesizeCartographicFallbackTile(
        const SlippyTileKey& key,
        keel::Array<keel::u32>& outRgba
    ) {
        const keel::u32 sz = key.tileSizePx;
        keel::usize numPx = 0;
        if (!keel::checked_mul<keel::usize>(sz, sz, numPx)) {
            outRgba.clear();
            return;
        }
        outRgba.resize(numPx);

        const bool evenChecker = ((key.x + key.y) & 1) == 0;
        const keel::u8 baseR = evenChecker ? 236 : 231;
        const keel::u8 baseG = evenChecker ? 233 : 228;
        const keel::u8 baseB = evenChecker ? 222 : 216;
        const keel::u32 subStep = keel::max(16u, sz / 4u);

        for (keel::u32 py = 0; py < sz; ++py) {
            for (keel::u32 px = 0; px < sz; ++px) {
                keel::u8 r = baseR;
                keel::u8 g = baseG;
                keel::u8 b = baseB;

                if (px == 0 || py == 0) {
                    r = 188; g = 198; b = 208;
                } else if ((px % subStep) == 0 || (py % subStep) == 0) {
                    r = 216; g = 214; b = 204;
                }
                const keel::usize idx = static_cast<keel::usize>(py) * sz + px;
                outRgba[idx] = (static_cast<keel::u32>(255u) << 24u) |
                               (static_cast<keel::u32>(b) << 16u) |
                               (static_cast<keel::u32>(g) << 8u) |
                               static_cast<keel::u32>(r);
            }
        }
    }

    keel::String m_cacheDir;
    keel::HttpClient m_httpClient;
    mutable std::mutex m_mutex{};
    mutable keel::LruCache<SlippyTileKey, CacheEntry, 32, SlippyTileKeyHash> m_ramCache{};
    mutable std::atomic<keel::u64> m_accessCounter{1};
    mutable std::atomic<keel::i64> m_lastNetworkFailureMs{0};
};

} // namespace

keel::Arc<IRasterTileProvider> CreateOsmRasterTileProvider(keel::StringRef cacheDir) {
    keel::Arc<IRasterTileProvider> provider = keel::make_arc<OsmRasterTileProvider>(cacheDir);
    if (!GisRenderEngine::GetDefaultBasemapProvider()) {
        GisRenderEngine::SetDefaultBasemapProvider(provider);
    }
    return provider;
}

} // namespace guikit::geo
