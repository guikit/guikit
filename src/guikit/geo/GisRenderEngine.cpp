#include "guikit/geo/GisRenderEngine.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/backend/VectorBackend.hpp"
#include "guikit/geo/PolygonTriangulator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <list>
#include <memory>
#include <unordered_map>

namespace guikit {

namespace {

[[nodiscard]] Color ApplyOpacity(Color c, float opacity) noexcept {
    const float a = std::clamp(static_cast<float>(c.a) * std::clamp(opacity, 0.0f, 1.0f), 0.0f, 255.0f);
    return Color::Rgba(c.r, c.g, c.b, static_cast<uint8_t>(a));
}

struct BathymetryPalette {
    Color deepWater;        ///< Depth > deepContour
    Color mediumDeepWater;  ///< safetyContour < Depth <= deepContour
    Color mediumShallow;    ///< shallowContour < Depth <= safetyContour
    Color veryShallow;      ///< 0 <= Depth <= shallowContour
    Color intertidal;       ///< Depth < 0 (drying heights)
    Color landFill;         ///< LNDARE fill
    Color landStroke;       ///< COALNE stroke
    Color urbanFill;        ///< BUISGL / HRBFAC fill
    Color contourNormal;    ///< Normal DEPCNT
    Color contourSafety;    ///< Safety DEPCNT
    Color soundingShallow;  ///< Sounding <= safetyContour
    Color soundingDeep;     ///< Sounding > safetyContour
    Color soundingHalo;     ///< Contrast halo behind sounding digits
};

[[nodiscard]] BathymetryPalette GetPaletteColors(GisColorPalette p) noexcept {
    switch (p) {
        case GisColorPalette::NavionicsDay:
            return {
                .deepWater       = Color::Rgba(212, 238, 255, 255),
                .mediumDeepWater = Color::Rgba(168, 220, 252, 255),
                .mediumShallow   = Color::Rgba(112, 196, 245, 255),
                .veryShallow     = Color::Rgba( 58, 172, 235, 255),
                .intertidal      = Color::Rgba(158, 208, 168, 255),
                .landFill        = Color::Rgba(234, 218, 174, 255),
                .landStroke      = Color::Rgba( 88,  72,  46, 255),
                .urbanFill       = Color::Rgba(206, 188, 148, 255),
                .contourNormal   = Color::Rgba( 52, 132, 190, 210),
                .contourSafety   = Color::Rgba( 14,  78, 140, 250),
                .soundingShallow = Color::Rgba( 12,  52,  98, 255),
                .soundingDeep    = Color::Rgba( 48,  92, 134, 245),
                .soundingHalo    = Color::Rgba(255, 255, 255, 200)
            };
        case GisColorPalette::NavionicsDusk:
            return {
                .deepWater       = Color::Rgba( 18,  38,  64, 255),
                .mediumDeepWater = Color::Rgba( 24,  54,  88, 255),
                .mediumShallow   = Color::Rgba( 32,  76, 118, 255),
                .veryShallow     = Color::Rgba( 42, 102, 152, 255),
                .intertidal      = Color::Rgba( 44,  88,  76, 255),
                .landFill        = Color::Rgba( 92,  78,  54, 255),
                .landStroke      = Color::Rgba(168, 146, 106, 245),
                .urbanFill       = Color::Rgba(116,  98,  72, 255),
                .contourNormal   = Color::Rgba( 72, 148, 204, 195),
                .contourSafety   = Color::Rgba(125, 211, 252, 245),
                .soundingShallow = Color::Rgba(186, 230, 253, 255),
                .soundingDeep    = Color::Rgba(125, 178, 218, 240),
                .soundingHalo    = Color::Rgba( 10,  22,  38, 210)
            };
        case GisColorPalette::NavionicsNight:
            return {
                .deepWater       = Color::Rgba(  8,  13,  22, 255),
                .mediumDeepWater = Color::Rgba( 12,  22,  36, 255),
                .mediumShallow   = Color::Rgba( 18,  34,  54, 255),
                .veryShallow     = Color::Rgba( 26,  50,  76, 255),
                .intertidal      = Color::Rgba( 24,  44,  38, 255),
                .landFill        = Color::Rgba( 38,  32,  26, 255),
                .landStroke      = Color::Rgba(124,  96,  64, 235),
                .urbanFill       = Color::Rgba( 54,  44,  36, 255),
                .contourNormal   = Color::Rgba( 46,  98, 138, 185),
                .contourSafety   = Color::Rgba( 56, 189, 248, 235),
                .soundingShallow = Color::Rgba(125, 211, 252, 245),
                .soundingDeep    = Color::Rgba( 82, 136, 178, 225),
                .soundingHalo    = Color::Rgba(  5,   8,  14, 210)
            };
        case GisColorPalette::ClassicGis:
            return {
                .deepWater       = Color::Rgba( 24,  48,  78, 255),
                .mediumDeepWater = Color::Rgba( 32,  66, 104, 255),
                .mediumShallow   = Color::Rgba( 44,  88, 134, 255),
                .veryShallow     = Color::Rgba( 58, 114, 168, 255),
                .intertidal      = Color::Rgba( 64, 118,  96, 255),
                .landFill        = Color::Rgba( 56,  72,  58, 255),
                .landStroke      = Color::Rgba(134, 168, 138, 245),
                .urbanFill       = Color::Rgba( 82,  92,  88, 255),
                .contourNormal   = Color::Rgba( 96, 165, 250, 190),
                .contourSafety   = Color::Rgba( 56, 189, 248, 245),
                .soundingShallow = Color::Rgba(226, 232, 240, 250),
                .soundingDeep    = Color::Rgba(148, 163, 184, 235),
                .soundingHalo    = Color::Rgba( 15,  23,  42, 210)
            };
    }
    return GetPaletteColors(GisColorPalette::NavionicsDay);
}

[[nodiscard]] Color ResolveDepthAreaFill(
    const GeoFeature& feat,
    const BathymetryPalette& pal,
    const GisRenderSettings& settings
) noexcept {
    const double dMin = feat.depthMinMeters.value_or(feat.contourDepthMeters.value_or(5.0));
    if (dMin < 0.0) {
        return pal.intertidal;
    }
    if (dMin <= static_cast<double>(settings.shallowContourMeters)) {
        return pal.veryShallow;
    }
    if (dMin <= static_cast<double>(settings.safetyContourMeters)) {
        return pal.mediumShallow;
    }
    if (dMin <= static_cast<double>(settings.deepContourMeters)) {
        return pal.mediumDeepWater;
    }
    return pal.deepWater;
}

[[nodiscard]] float PointToSegmentDistanceSq(Vec2 p, Vec2 a, Vec2 b) noexcept {
    const float vx = b.x - a.x;
    const float vy = b.y - a.y;
    const float wx = p.x - a.x;
    const float wy = p.y - a.y;
    const float c1 = wx * vx + wy * vy;
    if (c1 <= 0.0f) {
        return wx * wx + wy * wy;
    }
    const float c2 = vx * vx + vy * vy;
    if (c2 <= c1) {
        const float dx = p.x - b.x;
        const float dy = p.y - b.y;
        return dx * dx + dy * dy;
    }
    const float t = c1 / c2;
    const float projX = a.x + t * vx;
    const float projY = a.y + t * vy;
    const float dx = p.x - projX;
    const float dy = p.y - projY;
    return dx * dx + dy * dy;
}

// ============================================================================
// Two-Tier GIS Cache:
//   Tier 1: Projected Normalized [0..1] Coordinates & Precomputed World-Space
//           Edge Lengths per Feature (eliminates thousands of transcendental
//           std::sin/std::tan/std::log/std::hypot calls per frame).
//   Tier 2: Rasterized 256x256 Vector Tile Cache via Offscreen ICanvas
//           (rasterizes static polygons & polylines once per visible [z/x/y]
//           slippy tile, then blits cached 32-bit pixels in <0.2ms on pan).
// ============================================================================
struct ProjectedRing {
    std::vector<Vec2> normPts;
    std::vector<float> segLenNorm; ///< Euclidean length in normalized [0..1] space of segment i -> i+1
};

struct ProjectedPolygon {
    ProjectedRing exterior;
    std::vector<ProjectedRing> holes;
    std::vector<Vec2> cachedTrianglesNorm; ///< Only used if fallback triangle rendering is needed
};

struct ProjectedLine {
    std::vector<Vec2> normPts;
    std::vector<float> segLenNorm;
};

struct ProjectedFeature {
    uint64_t featureId{0};
    Rect normBounds{};
    std::vector<Vec2> pointsNorm;
    std::vector<ProjectedLine> lines;
    std::vector<ProjectedPolygon> polygons;
};

class GisVectorTileCache {
public:
    static constexpr uint32_t kTileSizePx = 256;
    static constexpr size_t kMaxCachedTiles = 256;

    [[nodiscard]] static GisVectorTileCache& Instance() {
        static GisVectorTileCache s_instance;
        return s_instance;
    }

    /// Computes a fast 64-bit signature of the current layer set, visibility, opacity, CRS, and render settings.
    [[nodiscard]] static uint64_t ComputeStyleSignature(
        std::span<const GeoLayer> layers,
        const GisRenderSettings& settings,
        MapCrs crs
    ) noexcept {
        uint64_t h = 14695981039346656037ULL;
        auto mix = [&](uint64_t v) noexcept {
            h ^= v;
            h *= 1099511628211ULL;
        };

        mix(static_cast<uint64_t>(crs));
        mix(static_cast<uint64_t>(settings.palette));
        uint32_t scBits = 0, shBits = 0, dpBits = 0;
        std::memcpy(&scBits, &settings.safetyContourMeters, sizeof(float));
        std::memcpy(&shBits, &settings.shallowContourMeters, sizeof(float));
        std::memcpy(&dpBits, &settings.deepContourMeters, sizeof(float));
        mix(scBits);
        mix(shBits);
        mix(dpBits);

        const uint64_t flags =
            (settings.showFilledPolygons     ? 1ULL : 0ULL) |
            (settings.showPolygonOutlines    ? 2ULL : 0ULL) |
            (settings.showDepthContours      ? 4ULL : 0ULL) |
            (settings.highlightSafetyContour ? 8ULL : 0ULL);
        mix(flags);
        mix(layers.size());

        for (const auto& l : layers) {
            mix(l.id);
            mix(l.visible ? 1ULL : 0ULL);
            mix(static_cast<uint64_t>(std::round(l.opacity * 255.0f)));
            mix(static_cast<uint64_t>(l.zOrder));
            mix(l.features.size());
            if (!l.features.empty()) {
                mix(l.features.front().id);
                mix(l.features.back().id);
            }
        }
        return h;
    }

    /// Ensures all visible features have their normalized [0..1] coordinates projected for the active CRS.
    void EnsureProjectedGeometry(
        std::span<const GeoLayer> layers,
        MapCrs crs
    ) {
        uint64_t geomSig = 14695981039346656037ULL;
        geomSig = (geomSig ^ static_cast<uint64_t>(crs)) * 1099511628211ULL;
        geomSig = (geomSig ^ layers.size()) * 1099511628211ULL;
        for (const auto& l : layers) {
            geomSig = (geomSig ^ l.id) * 1099511628211ULL;
            geomSig = (geomSig ^ l.features.size()) * 1099511628211ULL;
            if (!l.features.empty()) {
                geomSig = (geomSig ^ reinterpret_cast< uintptr_t >(&l.features.front())) * 1099511628211ULL;
            }
        }

        if (geomSig == m_projectedSignature && m_projectedCrs == crs) {
            return;
        }

        m_projectedSignature = geomSig;
        m_projectedCrs = crs;
        m_projectedFeatures.clear();

        auto projectRing = [&](std::span<const GeoPoint> src) -> ProjectedRing {
            ProjectedRing pr;
            pr.normPts.reserve(src.size());
            pr.segLenNorm.reserve(src.size());
            for (const auto& gp : src) {
                pr.normPts.push_back(MapGrid::GeoToNormalized(gp.ToGeoCoord(), crs));
            }
            for (size_t i = 0; i < pr.normPts.size(); ++i) {
                const Vec2& p0 = pr.normPts[i];
                const Vec2& p1 = pr.normPts[(i + 1) % pr.normPts.size()];
                pr.segLenNorm.push_back(std::hypot(p1.x - p0.x, p1.y - p0.y));
            }
            return pr;
        };

        for (const auto& layer : layers) {
            for (const auto& feat : layer.features) {
                ProjectedFeature pf;
                pf.featureId = feat.id;

                float minX =  1e9f, minY =  1e9f;
                float maxX = -1e9f, maxY = -1e9f;
                auto expandBounds = [&](Vec2 p) noexcept {
                    if (p.x < minX) minX = p.x;
                    if (p.x > maxX) maxX = p.x;
                    if (p.y < minY) minY = p.y;
                    if (p.y > maxY) maxY = p.y;
                };

                pf.pointsNorm.reserve(feat.points.size());
                for (const auto& gp : feat.points) {
                    const Vec2 np = MapGrid::GeoToNormalized(gp.ToGeoCoord(), crs);
                    pf.pointsNorm.push_back(np);
                    expandBounds(np);
                }

                pf.lines.reserve(feat.lines.size());
                for (const auto& line : feat.lines) {
                    ProjectedLine pl;
                    pl.normPts.reserve(line.size());
                    pl.segLenNorm.reserve(line.size());
                    for (const auto& gp : line) {
                        const Vec2 np = MapGrid::GeoToNormalized(gp.ToGeoCoord(), crs);
                        pl.normPts.push_back(np);
                        expandBounds(np);
                    }
                    for (size_t i = 0; i + 1 < pl.normPts.size(); ++i) {
                        pl.segLenNorm.push_back(std::hypot(
                            pl.normPts[i + 1].x - pl.normPts[i].x,
                            pl.normPts[i + 1].y - pl.normPts[i].y
                        ));
                    }
                    pf.lines.push_back(std::move(pl));
                }

                pf.polygons.reserve(feat.polygons.size());
                for (const auto& poly : feat.polygons) {
                    ProjectedPolygon pp;
                    pp.exterior = projectRing(poly.exterior);
                    for (const Vec2& np : pp.exterior.normPts) {
                        expandBounds(np);
                    }
                    pp.holes.reserve(poly.holes.size());
                    for (const auto& h : poly.holes) {
                        pp.holes.push_back(projectRing(h));
                    }
                    pp.cachedTrianglesNorm.reserve(poly.cachedTriangles.size());
                    for (const auto& tp : poly.cachedTriangles) {
                        pp.cachedTrianglesNorm.push_back(MapGrid::GeoToNormalized(tp.ToGeoCoord(), crs));
                    }
                    pf.polygons.push_back(std::move(pp));
                }

                if (minX <= maxX && minY <= maxY) {
                    pf.normBounds = Rect(minX, minY, std::max(1e-7f, maxX - minX), std::max(1e-7f, maxY - minY));
                }
                const uint64_t key = (static_cast<uint64_t>(layer.id) << 32) | (feat.id & 0xFFFFFFFFULL);
                m_projectedFeatures.emplace(key, std::move(pf));
            }
        }
    }

    [[nodiscard]] const ProjectedFeature* FindProjected(uint32_t layerId, uint64_t featureId) const noexcept {
        const uint64_t key = (static_cast<uint64_t>(layerId) << 32) | (featureId & 0xFFFFFFFFULL);
        const auto it = m_projectedFeatures.find(key);
        return (it != m_projectedFeatures.end()) ? &it->second : nullptr;
    }

    /// Retrieves or rasterizes a 256x256 ABGR8888 vector tile for `[z/x/y]`.
    [[nodiscard]] const std::vector<uint32_t>& GetOrRasterizeTile(
        OsmTileId tileId,
        std::span<const GeoLayer* const> sortedLayers,
        const GisRenderSettings& settings,
        const BathymetryPalette& pal,
        uint64_t styleSignature
    ) {
        if (styleSignature != m_tileStyleSignature) {
            m_tileStyleSignature = styleSignature;
            m_tileMap.clear();
            m_lruOrder.clear();
        }

        const TileKey key{tileId.z, tileId.x, tileId.y};
        if (auto it = m_tileMap.find(key); it != m_tileMap.end()) {
            m_lruOrder.splice(m_lruOrder.begin(), m_lruOrder, it->second.lruIt);
            return it->second.pixels;
        }

        if (m_tileMap.size() >= kMaxCachedTiles && !m_lruOrder.empty()) {
            const TileKey oldest = m_lruOrder.back();
            m_lruOrder.pop_back();
            m_tileMap.erase(oldest);
        }

        m_lruOrder.push_front(key);
        TileEntry entry;
        entry.lruIt = m_lruOrder.begin();
        entry.pixels.resize(kTileSizePx * kTileSizePx, 0u);

        RasterizeTileIntoBuffer(tileId, sortedLayers, settings, pal, entry.pixels);

        auto [insIt, _] = m_tileMap.emplace(key, std::move(entry));
        return insIt->second.pixels;
    }

private:
    struct TileKey {
        int z{0};
        int x{0};
        int y{0};
        bool operator==(const TileKey& o) const noexcept {
            return z == o.z && x == o.x && y == o.y;
        }
    };

    struct TileKeyHash {
        size_t operator()(const TileKey& k) const noexcept {
            uint64_t h = static_cast<uint64_t>(static_cast<uint32_t>(k.z));
            h = (h * 0x9e3779b97f4a7c15ULL) ^ static_cast<uint64_t>(static_cast<uint32_t>(k.x));
            h = (h * 0xbf58476d1ce4e5b9ULL) ^ static_cast<uint64_t>(static_cast<uint32_t>(k.y));
            return static_cast<size_t>(h);
        }
    };

    struct TileEntry {
        std::vector<uint32_t> pixels;
        std::list<TileKey>::iterator lruIt;
    };

    GisVectorTileCache() {
        m_tileCanvas = CreateOffscreenVectorCanvas();
    }

    ~GisVectorTileCache() = default;

    void RasterizeTileIntoBuffer(
        OsmTileId tileId,
        std::span<const GeoLayer* const> sortedLayers,
        const GisRenderSettings& settings,
        const BathymetryPalette& pal,
        std::vector<uint32_t>& outPixels
    ) {
        const Color waterBg = GisRenderEngine::GetWaterBackgroundColor(settings.palette);
        const uint32_t rawWater = waterBg.ToUint32();
        std::fill(outPixels.begin(), outPixels.end(), rawWater);

        if (!m_tileCanvas) return;

        m_tileCanvas->SetTarget(outPixels.data(), kTileSizePx, kTileSizePx, kTileSizePx);
        m_tileCanvas->Begin(kTileSizePx, kTileSizePx, 1.0f);

        const float n = static_cast<float>(1 << std::clamp(tileId.z, 0, 22));
        const float tileNormX0 = static_cast<float>(tileId.x) / n;
        const float tileNormY0 = static_cast<float>(tileId.y) / n;
        const float tileNormW  = 1.0f / n;
        const float normToTilePx = static_cast<float>(kTileSizePx) * n;

        // Expand tile bounds slightly by 8px margin so strokes along tile borders don't clip
        const float marginNorm = 8.0f / normToTilePx;
        const Rect tileNormRect(
            tileNormX0 - marginNorm,
            tileNormY0 - marginNorm,
            tileNormW + marginNorm * 2.0f,
            tileNormW + marginNorm * 2.0f
        );

        auto toTilePx = [&](Vec2 normPt) noexcept -> Vec2 {
            return {
                (normPt.x - tileNormX0) * normToTilePx,
                (normPt.y - tileNormY0) * normToTilePx
            };
        };

        auto appendDecimatedRing = [&](VectorPath& outPath, const ProjectedRing& ring) {
            if (ring.normPts.size() < 3) return;
            const Vec2 firstPx = toTilePx(ring.normPts.front());
            outPath.MoveTo(firstPx);
            Vec2 lastPx = firstPx;
            for (size_t i = 1; i < ring.normPts.size(); ++i) {
                const Vec2 ptPx = toTilePx(ring.normPts[i]);
                if (i + 1 < ring.normPts.size() &&
                    std::abs(ptPx.x - lastPx.x) < 0.6f &&
                    std::abs(ptPx.y - lastPx.y) < 0.6f) {
                    continue;
                }
                outPath.LineTo(ptPx);
                lastPx = ptPx;
            }
            outPath.Close();
        };

        for (const GeoLayer* layerPtr : sortedLayers) {
            const GeoLayer& layer = *layerPtr;
            if (!layer.visible || layer.opacity <= 0.01f) continue;

            for (const GeoFeature& feat : layer.features) {
                if (feat.polygons.empty() && feat.lines.empty()) continue;

                const ProjectedFeature* pf = FindProjected(layer.id, feat.id);
                if (!pf || !pf->normBounds.Intersects(tileNormRect)) {
                    continue;
                }

                // Resolve style
                Color fillCol = ApplyOpacity(layer.defaultStyle.fillColor, layer.opacity);
                Color strokeCol = ApplyOpacity(layer.defaultStyle.strokeColor, layer.opacity);
                float strokeW = layer.defaultStyle.strokeWidthPx;
                bool dashed = layer.defaultStyle.dashed;
                bool renderFill = settings.showFilledPolygons && layer.defaultStyle.filled;
                bool renderStroke = settings.showPolygonOutlines && layer.defaultStyle.stroked;

                switch (feat.featureClass) {
                    case GisFeatureClass::DepthArea:
                        fillCol = ApplyOpacity(ResolveDepthAreaFill(feat, pal, settings), layer.opacity);
                        strokeCol = Color::Transparent();
                        renderStroke = false;
                        break;
                    case GisFeatureClass::LandArea:
                        fillCol = ApplyOpacity(pal.landFill, layer.opacity);
                        strokeCol = ApplyOpacity(pal.landStroke, layer.opacity);
                        strokeW = 1.4f;
                        break;
                    case GisFeatureClass::UrbanBuilding:
                        fillCol = ApplyOpacity(pal.urbanFill, layer.opacity);
                        strokeCol = ApplyOpacity(pal.landStroke, layer.opacity);
                        strokeW = 1.1f;
                        break;
                    case GisFeatureClass::Coastline:
                        renderFill = false;
                        renderStroke = true;
                        strokeCol = ApplyOpacity(pal.landStroke, layer.opacity);
                        strokeW = 2.0f;
                        break;
                    case GisFeatureClass::DepthContour: {
                        if (!settings.showDepthContours) continue;
                        renderFill = false;
                        renderStroke = true;
                        const double d = feat.contourDepthMeters.value_or(10.0);
                        const bool isSafety = settings.highlightSafetyContour &&
                                              (d <= static_cast<double>(settings.safetyContourMeters) + 1e-3);
                        strokeCol = ApplyOpacity(isSafety ? pal.contourSafety : pal.contourNormal, layer.opacity);
                        strokeW = isSafety ? 2.1f : 1.15f;
                        break;
                    }
                    default:
                        break;
                }

                // 1. Polygons (batched through backend-agnostic ICanvas::DrawPath)
                for (const ProjectedPolygon& poly : pf->polygons) {
                    if (poly.exterior.normPts.size() < 3) continue;
                    if (!renderFill && (!renderStroke || strokeCol.a == 0)) continue;

                    m_scratchPath.Clear();
                    appendDecimatedRing(m_scratchPath, poly.exterior);
                    for (const auto& hole : poly.holes) {
                        appendDecimatedRing(m_scratchPath, hole);
                    }

                    PathPaint paint{};
                    paint.fillColor = fillCol;
                    paint.strokeColor = strokeCol;
                    paint.strokeWidthPx = strokeW;
                    paint.fillRule = poly.holes.empty() ? FillRule::NonZero : FillRule::EvenOdd;
                    paint.renderFill = (renderFill && fillCol.a > 0);
                    paint.renderStroke = (renderStroke && strokeCol.a > 0 && strokeW > 0.0f);
                    paint.dashed = dashed;
                    paint.dashOnPx = 5.0f;
                    paint.dashOffPx = 4.0f;

                    m_tileCanvas->DrawPath(m_scratchPath, paint);
                }

                // 2. Polylines (DEPCNT, COALNE, FAIRWY, CBLSUB, ROADWY)
                for (const ProjectedLine& line : pf->lines) {
                    if (line.normPts.size() < 2 || strokeCol.a == 0) continue;

                    m_scratchPath.Clear();
                    const Vec2 firstPx = toTilePx(line.normPts.front());
                    m_scratchPath.MoveTo(firstPx);
                    Vec2 lastPx = firstPx;
                    int segCount = 0;

                    for (size_t i = 1; i < line.normPts.size(); ++i) {
                        const Vec2 ptPx = toTilePx(line.normPts[i]);
                        if (i + 1 < line.normPts.size() &&
                            std::abs(ptPx.x - lastPx.x) < 0.6f &&
                            std::abs(ptPx.y - lastPx.y) < 0.6f) {
                            continue;
                        }
                        m_scratchPath.LineTo(ptPx);
                        lastPx = ptPx;
                        ++segCount;
                    }

                    if (segCount > 0) {
                        PathPaint paint{};
                        paint.strokeColor = strokeCol;
                        paint.strokeWidthPx = strokeW;
                        paint.renderFill = false;
                        paint.renderStroke = true;
                        paint.dashed = dashed;
                        paint.dashOnPx = 6.0f;
                        paint.dashOffPx = 4.0f;
                        m_tileCanvas->DrawPath(m_scratchPath, paint);
                    }
                }
            }
        }

        m_tileCanvas->End();
    }

    std::unique_ptr<ICanvas> m_tileCanvas;
    VectorPath m_scratchPath;
    uint64_t m_projectedSignature{0};
    MapCrs m_projectedCrs{MapCrs::EPSG3857_WebMercator};
    std::unordered_map<uint64_t, ProjectedFeature> m_projectedFeatures;

    uint64_t m_tileStyleSignature{0};
    std::list<TileKey> m_lruOrder;
    std::unordered_map<TileKey, TileEntry, TileKeyHash> m_tileMap;
};

} // namespace

Color GisRenderEngine::GetWaterBackgroundColor(GisColorPalette palette) noexcept {
    return GetPaletteColors(palette).deepWater;
}

GisRenderStats GisRenderEngine::RenderLayers(
    MapViewScope& map,
    std::span<const GeoLayer> layers,
    const GisRenderSettings& settings,
    std::optional<uint64_t> selectedFeatureId,
    Vec2 mouseScreenPos,
    bool mouseClickedInMap
) {
    GisRenderStats stats{};
    SceneScope& scene = map.GetScene();
    const MapViewState& view = map.GetViewState();
    const float z = std::max(0.0001f, scene.GetZoom());
    const float invZ = 1.0f / z;
    const Rect visWorld = scene.GetVisibleWorldRect();
    const float worldSize = (view.totalTilesPerAxis > 0 && !view.visibleTiles.empty())
        ? (view.visibleTiles.front().worldRect.w * static_cast<float>(view.totalTilesPerAxis))
        : 2048.0f;

    // 1. Paint water background across visible viewport
    const BathymetryPalette pal = GetPaletteColors(settings.palette);
    scene.DrawRect(visWorld, pal.deepWater);

    // Build visible GeoBBox for fast spatial frustum culling
    GeoBBox viewBBox{};
    viewBBox.Expand(view.visibleBounds.nw.lon, view.visibleBounds.nw.lat);
    viewBBox.Expand(view.visibleBounds.se.lon, view.visibleBounds.se.lat);
    const double padLon = std::max(0.01, viewBBox.SpanLon() * 0.15);
    const double padLat = std::max(0.01, viewBBox.SpanLat() * 0.15);
    viewBBox.minLon -= padLon;
    viewBBox.maxLon += padLon;
    viewBBox.minLat -= padLat;
    viewBBox.maxLat += padLat;

    // Sort visible layers by zOrder
    std::vector<const GeoLayer*> sortedLayers;
    sortedLayers.reserve(layers.size());
    for (const auto& layer : layers) {
        stats.featuresTotal += layer.features.size();
        if (layer.visible && layer.opacity > 0.01f) {
            sortedLayers.push_back(&layer);
        }
    }
    std::sort(sortedLayers.begin(), sortedLayers.end(), [](const GeoLayer* a, const GeoLayer* b) {
        return a->zOrder < b->zOrder;
    });
    stats.layersRendered = sortedLayers.size();

    // 2. Ensure Projected Normalized [0..1] Coordinates & Blit Cached 256x256 Vector Tiles!
    auto& tileCache = GisVectorTileCache::Instance();
    tileCache.EnsureProjectedGeometry(layers, view.crs);
    const uint64_t styleSig = GisVectorTileCache::ComputeStyleSignature(layers, settings, view.crs);

    for (const OsmTileCell& cell : view.visibleTiles) {
        const auto& tilePixels = tileCache.GetOrRasterizeTile(cell.id, sortedLayers, settings, pal, styleSig);
        scene.DrawPixels(
            tilePixels.data(),
            GisVectorTileCache::kTileSizePx,
            GisVectorTileCache::kTileSizePx,
            cell.worldRect
        );
    }

    const Vec2 mouseWorld = scene.GetMouseWorldPos();
    const GeoCoord mouseGeo = map.WorldToGeo(mouseWorld);
    const bool mouseInViewport = scene.GetViewportRect().Contains(mouseScreenPos);

    // Hit-test candidates: Points > Lines > Polygons (higher zOrder wins within same geometry class)
    float bestPointDistSq = (10.0f * invZ) * (10.0f * invZ);
    std::optional<std::pair<uint32_t, uint64_t>> bestPointHit;

    float bestLineDistSq = (6.5f * invZ) * (6.5f * invZ);
    std::optional<std::pair<uint32_t, uint64_t>> bestLineHit;

    std::optional<std::pair<uint32_t, uint64_t>> bestPolyHit;

    // Screen-space label collision avoidance grid (prevents overlapping soundings & contour labels)
    std::vector<Rect> placedLabelRectsWorld;
    placedLabelRectsWorld.reserve(192);
    auto canPlaceLabelWorld = [&](Rect candidateWorld) noexcept -> bool {
        if (placedLabelRectsWorld.size() >= 180) {
            return false;
        }
        for (const Rect& r : placedLabelRectsWorld) {
            if (r.Intersects(candidateWorld)) {
                return false;
            }
        }
        placedLabelRectsWorld.push_back(candidateWorld);
        return true;
    };

    const float decimationTolWorld = 1.25f * invZ;
    const float decimationTolNorm  = decimationTolWorld / std::max(1.0f, worldSize);
    const Rect visNormRect(
        visWorld.x / worldSize - 0.02f,
        visWorld.y / worldSize - 0.02f,
        visWorld.w / worldSize + 0.04f,
        visWorld.h / worldSize + 0.04f
    );

    // 3. Foreground Pass: Interactive Hit-Testing, Selection Highlights, Spot Soundings, Seamarks & Labels
    for (const GeoLayer* layerPtr : sortedLayers) {
        const GeoLayer& layer = *layerPtr;

        for (const GeoFeature& feat : layer.features) {
            if (feat.bbox.IsValid() && !feat.bbox.Intersects(viewBBox)) {
                ++stats.featuresCulled;
                continue;
            }
            if (feat.featureClass == GisFeatureClass::DepthContour && !settings.showDepthContours) {
                continue;
            }
            if (feat.featureClass == GisFeatureClass::Sounding && !settings.showSoundings) {
                continue;
            }
            if ((feat.featureClass == GisFeatureClass::Seamark || feat.featureClass == GisFeatureClass::Light) &&
                !settings.showSeamarks) {
                continue;
            }

            const ProjectedFeature* pf = tileCache.FindProjected(layer.id, feat.id);
            if (pf && !pf->normBounds.Intersects(visNormRect)) {
                ++stats.featuresCulled;
                continue;
            }

            ++stats.featuresRendered;
            const bool isSelected = selectedFeatureId.has_value() && (*selectedFeatureId == feat.id);

            // --- A. Polygon Hit-Testing, Selected Highlight & Labels ---
            if (!feat.polygons.empty()) {
                for (size_t polyIdx = 0; polyIdx < feat.polygons.size(); ++polyIdx) {
                    const auto& poly = feat.polygons[polyIdx];
                    if (poly.exterior.size() < 3) continue;

                    if (mouseInViewport && feat.bbox.Contains(mouseGeo.lon, mouseGeo.lat)) {
                        if (PolygonTriangulator::PointInPolygon(mouseGeo.lon, mouseGeo.lat, poly)) {
                            bestPolyHit = {layer.id, feat.id};
                        }
                    }

                    if (isSelected && pf && polyIdx < pf->polygons.size()) {
                        const auto& projRing = pf->polygons[polyIdx].exterior;
                        const Color selCol = Color::Rgba(56, 189, 248, 255);
                        const float selW = 3.0f * invZ;
                        float accumNorm = 0.0f;
                        Vec2 prevW{projRing.normPts.front().x * worldSize, projRing.normPts.front().y * worldSize};

                        for (size_t i = 0; i < projRing.normPts.size(); ++i) {
                            accumNorm += projRing.segLenNorm[i];
                            const size_t nextIdx = (i + 1) % projRing.normPts.size();
                            if (accumNorm >= decimationTolNorm || nextIdx == 0) {
                                const Vec2 nextW{projRing.normPts[nextIdx].x * worldSize, projRing.normPts[nextIdx].y * worldSize};
                                scene.DrawLine(prevW, nextW, selCol, selW);
                                prevW = nextW;
                                accumNorm = 0.0f;
                            }
                            if (settings.showVerticesOnSelect && (i % std::max<size_t>(1, projRing.normPts.size() / 48) == 0)) {
                                const Vec2 vW{projRing.normPts[i].x * worldSize, projRing.normPts[i].y * worldSize};
                                scene.DrawCircle(vW, 3.0f * invZ, Color::Rgba(255, 255, 255, 240), selCol, 1.5f * invZ);
                            }
                        }
                    }
                }

                if (settings.showFeatureLabels && !feat.name.empty() && feat.bbox.IsValid() &&
                    feat.featureClass != GisFeatureClass::DepthArea) {
                    const Vec2 centerW = map.GeoToWorld(feat.bbox.Center());
                    const float boxW = static_cast<float>(feat.name.size()) * 6.8f * invZ;
                    const float boxH = 14.0f * invZ;
                    const Rect lblRect(centerW.x - boxW * 0.5f, centerW.y - boxH * 0.5f, boxW, boxH);
                    if (visWorld.Intersects(lblRect) && canPlaceLabelWorld(lblRect)) {
                        const Color textCol = (settings.palette == GisColorPalette::NavionicsDay)
                            ? Color::Rgba(45, 36, 22, 235)
                            : Color::Rgba(226, 232, 240, 235);
                        scene.DrawText(
                            feat.name,
                            {lblRect.x, lblRect.y},
                            11.0f,
                            textCol,
                            FontStyle::Bold,
                            /*scaleWithZoom=*/false
                        );
                    }
                }
            }

            // --- B. LineString Hit-Testing, Selected Highlight & Contour Labels ---
            if (!feat.lines.empty() && pf) {
                for (size_t lineIdx = 0; lineIdx < pf->lines.size(); ++lineIdx) {
                    const auto& pl = pf->lines[lineIdx];
                    if (pl.normPts.size() < 2) continue;

                    const bool checkLineHit = mouseInViewport && feat.bbox.Contains(mouseGeo.lon, mouseGeo.lat);
                    float accumNorm = 0.0f;
                    Vec2 prevW{pl.normPts.front().x * worldSize, pl.normPts.front().y * worldSize};

                    for (size_t i = 0; i + 1 < pl.normPts.size(); ++i) {
                        accumNorm += pl.segLenNorm[i];
                        const bool isLast = (i + 2 == pl.normPts.size());
                        if (!isLast && accumNorm < decimationTolNorm) {
                            continue;
                        }
                        const Vec2 nextW{pl.normPts[i + 1].x * worldSize, pl.normPts[i + 1].y * worldSize};
                        accumNorm = 0.0f;

                        if (checkLineHit) {
                            const float dSq = PointToSegmentDistanceSq(mouseWorld, prevW, nextW);
                            if (dSq < bestLineDistSq) {
                                bestLineDistSq = dSq;
                                bestLineHit = {layer.id, feat.id};
                            }
                        }

                        if (isSelected) {
                            scene.DrawLine(prevW, nextW, Color::Rgba(56, 189, 248, 255), 3.2f * invZ);
                            if (settings.showVerticesOnSelect) {
                                scene.DrawCircle(prevW, 3.0f * invZ, Color::Rgba(255, 255, 255, 240), Color::Rgba(56, 189, 248, 255), 1.4f * invZ);
                            }
                        }
                        prevW = nextW;
                    }

                    // Contour inline depth label
                    if (settings.showFeatureLabels && feat.featureClass == GisFeatureClass::DepthContour &&
                        feat.contourDepthMeters.has_value() && pl.normPts.size() >= 3) {
                        const Vec2 midN = pl.normPts[pl.normPts.size() / 2];
                        const Vec2 midW{midN.x * worldSize, midN.y * worldSize};
                        if (visWorld.Contains(midW)) {
                            char dBuf[24]{};
                            const double dVal = *feat.contourDepthMeters;
                            std::snprintf(dBuf, sizeof(dBuf), (std::abs(dVal - std::round(dVal)) < 0.05) ? "%.0fm" : "%.1fm", dVal);
                            const Rect lblRect(midW.x - 14.0f * invZ, midW.y - 7.0f * invZ, 28.0f * invZ, 14.0f * invZ);
                            if (canPlaceLabelWorld(lblRect)) {
                                const bool isSafety = settings.highlightSafetyContour &&
                                                      (dVal <= static_cast<double>(settings.safetyContourMeters) + 1e-3);
                                const Color cCol = isSafety ? pal.contourSafety : pal.contourNormal;
                                scene.DrawText(dBuf, {lblRect.x, lblRect.y}, 9.5f, cCol, FontStyle::Bold, false);
                            }
                        }
                    }
                }
            }

            // --- C. Point / MultiPoint Rendering (SOUNDG, Seamarks, Lights, Hazards) ---
            for (size_t ptIdx = 0; ptIdx < feat.points.size(); ++ptIdx) {
                const GeoPoint& gp = feat.points[ptIdx];
                const Vec2 wPos = (pf && ptIdx < pf->pointsNorm.size())
                    ? Vec2{pf->pointsNorm[ptIdx].x * worldSize, pf->pointsNorm[ptIdx].y * worldSize}
                    : map.GeoToWorld(gp.ToGeoCoord());
                if (!visWorld.Contains(wPos)) continue;

                if (mouseInViewport) {
                    const float dx = mouseWorld.x - wPos.x;
                    const float dy = mouseWorld.y - wPos.y;
                    const float dSq = dx * dx + dy * dy;
                    if (dSq < bestPointDistSq) {
                        bestPointDistSq = dSq;
                        bestPointHit = {layer.id, feat.id};
                    }
                }

                if (feat.featureClass == GisFeatureClass::Sounding) {
                    // S-52 / Navionics Spot Sounding: whole meters + subscript tenths
                    const double depth = gp.hasZ ? gp.z : feat.contourDepthMeters.value_or(0.0);
                    const Rect sndRect(wPos.x - 15.0f * invZ, wPos.y - 8.0f * invZ, 30.0f * invZ, 16.0f * invZ);
                    if (!isSelected && !canPlaceLabelWorld(sndRect)) {
                        continue;
                    }

                    const bool isShallow = (depth <= static_cast<double>(settings.safetyContourMeters) + 1e-3);
                    const Color sndCol = isSelected
                        ? Color::Rgba(56, 189, 248, 255)
                        : ApplyOpacity(isShallow ? pal.soundingShallow : pal.soundingDeep, layer.opacity);

                    const double absD = std::abs(depth);
                    const int wholeMeters = static_cast<int>(std::floor(absD + 1e-4));
                    const int tenths = static_cast<int>(std::round((absD - static_cast<double>(wholeMeters)) * 10.0));

                    char mainBuf[16]{};
                    std::snprintf(mainBuf, sizeof(mainBuf), "%d", wholeMeters);

                    if (isSelected) {
                        scene.DrawCircle(wPos, 11.0f * invZ, Color::Rgba(56, 189, 248, 45), Color::Rgba(56, 189, 248, 240), 1.6f * invZ);
                    }

                    scene.DrawText(
                        mainBuf,
                        {wPos.x - 5.0f * invZ, wPos.y - 6.0f * invZ},
                        10.5f,
                        sndCol,
                        isShallow ? FontStyle::Bold : FontStyle::Regular,
                        /*scaleWithZoom=*/false
                    );

                    if (tenths > 0 && tenths < 10 && wholeMeters < 30) {
                        char subBuf[8]{};
                        std::snprintf(subBuf, sizeof(subBuf), "%d", tenths);
                        const float xOff = (wholeMeters >= 10) ? 7.5f : 2.5f;
                        scene.DrawText(
                            subBuf,
                            {wPos.x + xOff * invZ, wPos.y - 2.0f * invZ},
                            8.0f,
                            sndCol,
                            isShallow ? FontStyle::Bold : FontStyle::Regular,
                            /*scaleWithZoom=*/false
                        );
                    }
                    ++stats.pointsSubmitted;
                } else if (feat.featureClass == GisFeatureClass::Seamark || feat.featureClass == GisFeatureClass::Light) {
                    const float r = (isSelected ? 8.5f : 6.5f) * invZ;
                    Color markCol = (feat.featureClass == GisFeatureClass::Light)
                        ? Color::Rgba(250, 204, 21, 245)
                        : Color::Rgba(217, 70, 239, 240);

                    const std::string_view colAttr = feat.GetAttribute("COLOUR");
                    const std::string_view lightOrCol = feat.GetAttribute("light_or_color_en");
                    if (colAttr == "3" || colAttr == "red" || feat.featureCode == "BOYLAT_PORT" ||
                        lightOrCol.starts_with("R ") || lightOrCol == "R") {
                        markCol = Color::Rgba(239, 68, 68, 245);
                    } else if (colAttr == "4" || colAttr == "green" || feat.featureCode == "BOYLAT_STBD" ||
                               lightOrCol.starts_with("G ") || lightOrCol == "G") {
                        markCol = Color::Rgba(16, 185, 129, 245);
                    }
                    if (isSelected) {
                        markCol = Color::Rgba(56, 189, 248, 255);
                    }
                    markCol = ApplyOpacity(markCol, layer.opacity);

                    if (feat.featureClass == GisFeatureClass::Light || !feat.GetAttribute("SECT1").empty()) {
                        scene.DrawArc(wPos, r * 2.1f, -60.0f, 120.0f, Color::Rgba(250, 204, 21, 215), 2.0f * invZ, 14);
                    }

                    // IALA Diamond Buoy / Beacon symbol
                    scene.DrawTriangle(
                        {wPos.x, wPos.y - r * 1.3f},
                        {wPos.x - r * 0.85f, wPos.y},
                        {wPos.x + r * 0.85f, wPos.y},
                        markCol.WithAlpha(85),
                        markCol,
                        1.5f * invZ
                    );
                    scene.DrawTriangle(
                        {wPos.x, wPos.y + r * 1.3f},
                        {wPos.x - r * 0.85f, wPos.y},
                        {wPos.x + r * 0.85f, wPos.y},
                        markCol.WithAlpha(85),
                        markCol,
                        1.5f * invZ
                    );
                    scene.DrawCircle(wPos, 2.2f * invZ, Color::Rgba(255, 255, 255, 245));

                    if (settings.showFeatureLabels && !feat.name.empty()) {
                        const Rect lblRect(wPos.x + r * 1.3f, wPos.y - 7.0f * invZ, 90.0f * invZ, 14.0f * invZ);
                        if (isSelected || canPlaceLabelWorld(lblRect)) {
                            scene.DrawText(feat.name, {lblRect.x, lblRect.y}, 10.5f, markCol, FontStyle::Bold, false);
                        }
                    }
                    ++stats.pointsSubmitted;
                } else if (feat.featureClass == GisFeatureClass::UnderwaterHazard) {
                    const float r = (isSelected ? 7.5f : 5.5f) * invZ;
                    const Color hazCol = isSelected ? Color::Rgba(56, 189, 248, 255) : Color::Rgba(220, 38, 38, 240);
                    // S-52 Isolated Danger / Underwater Rock / Wreck symbol (+) inside dotted circle
                    scene.DrawCircle(wPos, r * 1.25f, Color::Rgba(239, 68, 68, 45), hazCol, 1.4f * invZ);
                    scene.DrawLine({wPos.x - r, wPos.y}, {wPos.x + r, wPos.y}, hazCol, 1.6f * invZ);
                    scene.DrawLine({wPos.x, wPos.y - r}, {wPos.x, wPos.y + r}, hazCol, 1.6f * invZ);
                    if (settings.showFeatureLabels && !feat.name.empty()) {
                        const Rect lblRect(wPos.x + r * 1.4f, wPos.y - 6.0f * invZ, 80.0f * invZ, 12.0f * invZ);
                        if (isSelected || canPlaceLabelWorld(lblRect)) {
                            scene.DrawText(feat.name, {lblRect.x, lblRect.y}, 10.0f, hazCol, FontStyle::Bold, false);
                        }
                    }
                    ++stats.pointsSubmitted;
                } else if (feat.featureCode == "TEXT") {
                    if (settings.showFeatureLabels && !feat.name.empty()) {
                        const float boxW = static_cast<float>(feat.name.size()) * 7.0f * invZ;
                        const Rect lblRect(wPos.x - boxW * 0.5f, wPos.y - 7.0f * invZ, boxW, 14.0f * invZ);
                        if (isSelected || canPlaceLabelWorld(lblRect)) {
                            const Color txtCol = isSelected
                                ? Color::Rgba(56, 189, 248, 255)
                                : ((settings.palette == GisColorPalette::NavionicsDay)
                                    ? Color::Rgba(30, 58, 95, 235)
                                    : Color::Rgba(226, 232, 240, 235));
                            scene.DrawText(feat.name, {lblRect.x, lblRect.y}, 11.0f, txtCol, FontStyle::Bold, false);
                        }
                    }
                    ++stats.pointsSubmitted;
                } else {
                    const float r = (isSelected ? 6.5f : layer.defaultStyle.pointRadiusPx) * invZ;
                    const Color ptFill = isSelected ? Color::Rgba(56, 189, 248, 245) : ApplyOpacity(layer.defaultStyle.fillColor, layer.opacity);
                    const Color ptStroke = isSelected ? Color::Rgba(255, 255, 255, 255) : ApplyOpacity(layer.defaultStyle.strokeColor, layer.opacity);
                    scene.DrawCircle(wPos, r, ptFill, ptStroke, 1.5f * invZ);
                    if (settings.showFeatureLabels && !feat.name.empty()) {
                        scene.DrawText(feat.name, {wPos.x + r + 4.0f * invZ, wPos.y - 6.0f * invZ}, 10.5f, ptFill, FontStyle::Bold, false);
                    }
                    ++stats.pointsSubmitted;
                }
            }
        }
    }

    // Resolve final hover & click winner (Points > Lines > Polygons)
    std::optional<std::pair<uint32_t, uint64_t>> winner = bestPointHit;
    if (!winner.has_value()) winner = bestLineHit;
    if (!winner.has_value()) winner = bestPolyHit;

    if (winner.has_value()) {
        stats.hoveredLayerId = winner->first;
        stats.hoveredFeatureId = winner->second;
        if (mouseClickedInMap) {
            stats.clickedLayerId = winner->first;
            stats.clickedFeatureId = winner->second;
        }
    }

    return stats;
}

} // namespace guikit
