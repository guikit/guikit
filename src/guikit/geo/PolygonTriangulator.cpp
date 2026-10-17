#include "guikit/geo/PolygonTriangulator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace guikit {

namespace {

[[nodiscard]] constexpr bool NearlyEqual(const GeoPoint& a, const GeoPoint& b, double eps = 1e-11) noexcept {
    const double dLon = a.lon - b.lon;
    const double dLat = a.lat - b.lat;
    return (dLon >= -eps && dLon <= eps) && (dLat >= -eps && dLat <= eps);
}

[[nodiscard]] constexpr double Cross2D(const GeoPoint& a, const GeoPoint& b, const GeoPoint& c) noexcept {
    return (b.lon - a.lon) * (c.lat - a.lat) - (b.lat - a.lat) * (c.lon - a.lon);
}

[[nodiscard]] constexpr bool PointInTriangle2D(
    const GeoPoint& p,
    const GeoPoint& a,
    const GeoPoint& b,
    const GeoPoint& c
) noexcept {
    // Assumes (a, b, c) is counter-clockwise
    const double c1 = Cross2D(a, b, p);
    const double c2 = Cross2D(b, c, p);
    const double c3 = Cross2D(c, a, p);
    constexpr double kEps = -1e-13;
    return c1 >= kEps && c2 >= kEps && c3 >= kEps;
}

[[nodiscard]] constexpr bool SegmentsIntersectStrict(
    const GeoPoint& a,
    const GeoPoint& b,
    const GeoPoint& c,
    const GeoPoint& d
) noexcept {
    if (NearlyEqual(a, c) || NearlyEqual(a, d) || NearlyEqual(b, c) || NearlyEqual(b, d)) {
        return false;
    }
    const double d1 = Cross2D(c, d, a);
    const double d2 = Cross2D(c, d, b);
    const double d3 = Cross2D(a, b, c);
    const double d4 = Cross2D(a, b, d);
    return ((d1 > 0.0 && d2 < 0.0) || (d1 < 0.0 && d2 > 0.0)) &&
           ((d3 > 0.0 && d4 < 0.0) || (d3 < 0.0 && d4 > 0.0));
}

/// Cleans duplicate closing vertex and collinear/duplicate adjacent vertices.
[[nodiscard]] std::vector<GeoPoint> CleanRing(std::span<const GeoPoint> ring, bool wantCounterClockwise) {
    std::vector<GeoPoint> out;
    if (ring.size() < 3) return out;
    out.reserve(ring.size());

    for (const auto& pt : ring) {
        if (out.empty() || !NearlyEqual(out.back(), pt)) {
            out.push_back(pt);
        }
    }
    if (out.size() >= 2 && NearlyEqual(out.front(), out.back())) {
        out.pop_back();
    }
    if (out.size() < 3) {
        out.clear();
        return out;
    }

    const double area = PolygonTriangulator::SignedArea(out);
    if ((wantCounterClockwise && area < 0.0) || (!wantCounterClockwise && area > 0.0)) {
        std::reverse(out.begin(), out.end());
    }
    return out;
}

/// Splices a clockwise hole ring into a counter-clockwise outer ring using a horizontal ray-cast bridge.
void SpliceHoleIntoOuterRing(std::vector<GeoPoint>& outer, const std::vector<GeoPoint>& hole) {
    if (outer.size() < 3 || hole.size() < 3) return;

    // 1. Find the rightmost vertex M of the hole (maximum longitude)
    size_t holeMaxIdx = 0;
    for (size_t i = 1; i < hole.size(); ++i) {
        if (hole[i].lon > hole[holeMaxIdx].lon ||
            (hole[i].lon == hole[holeMaxIdx].lon && hole[i].lat < hole[holeMaxIdx].lat)) {
            holeMaxIdx = i;
        }
    }
    const GeoPoint M = hole[holeMaxIdx];

    // 2. Cast ray from M in +lon direction to intersect edges of `outer`
    double bestIntersectLon = std::numeric_limits<double>::infinity();
    size_t bridgeOuterIdx = 0;
    bool foundIntersection = false;

    const size_t nOuter = outer.size();
    for (size_t i = 0; i < nOuter; ++i) {
        const GeoPoint& a = outer[i];
        const GeoPoint& b = outer[(i + 1) % nOuter];

        // Edge must span M.lat vertically
        if ((a.lat > M.lat && b.lat > M.lat) || (a.lat < M.lat && b.lat < M.lat)) {
            continue;
        }
        if (std::abs(b.lat - a.lat) < 1e-14) {
            continue;
        }

        const double t = (M.lat - a.lat) / (b.lat - a.lat);
        if (t < -1e-9 || t > 1.0 + 1e-9) {
            continue;
        }
        const double xInt = a.lon + t * (b.lon - a.lon);
        if (xInt >= M.lon - 1e-11 && xInt < bestIntersectLon) {
            bestIntersectLon = xInt;
            // Pick the endpoint of edge (a, b) with larger longitude
            bridgeOuterIdx = (a.lon >= b.lon) ? i : ((i + 1) % nOuter);
            foundIntersection = true;
        }
    }

    if (!foundIntersection) {
        // Fallback: closest outer vertex to M
        double bestDistSq = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < nOuter; ++i) {
            const double dx = outer[i].lon - M.lon;
            const double dy = outer[i].lat - M.lat;
            const double d2 = dx * dx + dy * dy;
            if (d2 < bestDistSq) {
                bestDistSq = d2;
                bridgeOuterIdx = i;
            }
        }
    } else {
        // Check if any reflex vertex of outer lies inside triangle (M, I, P) and pick the one with minimum angle
        const GeoPoint P = outer[bridgeOuterIdx];
        const GeoPoint I{bestIntersectLon, M.lat, 0.0, false};
        GeoPoint triA = M;
        GeoPoint triB = I;
        GeoPoint triC = P;
        if (Cross2D(triA, triB, triC) < 0.0) {
            std::swap(triB, triC);
        }

        double bestDist = std::numeric_limits<double>::infinity();
        for (size_t i = 0; i < nOuter; ++i) {
            if (i == bridgeOuterIdx) continue;
            const GeoPoint& v = outer[i];
            if (v.lon < M.lon) continue;
            if (PointInTriangle2D(v, triA, triB, triC)) {
                const double dist = std::hypot(v.lon - M.lon, v.lat - M.lat);
                if (dist < bestDist) {
                    bestDist = dist;
                    bridgeOuterIdx = i;
                }
            }
        }
    }

    // 3. Build combined ring:
    // outer[0 .. bridgeOuterIdx] -> hole[holeMaxIdx .. end, 0 .. holeMaxIdx] -> outer[bridgeOuterIdx .. end]
    std::vector<GeoPoint> merged;
    merged.reserve(outer.size() + hole.size() + 2);

    for (size_t i = 0; i <= bridgeOuterIdx; ++i) {
        merged.push_back(outer[i]);
    }
    for (size_t k = 0; k < hole.size(); ++k) {
        merged.push_back(hole[(holeMaxIdx + k) % hole.size()]);
    }
    merged.push_back(hole[holeMaxIdx]);
    merged.push_back(outer[bridgeOuterIdx]);
    for (size_t i = bridgeOuterIdx + 1; i < outer.size(); ++i) {
        merged.push_back(outer[i]);
    }

    outer = std::move(merged);
}

} // namespace

double PolygonTriangulator::SignedArea(std::span<const GeoPoint> ring) noexcept {
    if (ring.size() < 3) return 0.0;
    double sum = 0.0;
    const size_t n = ring.size();
    for (size_t i = 0; i < n; ++i) {
        const GeoPoint& p0 = ring[i];
        const GeoPoint& p1 = ring[(i + 1) % n];
        sum += (p0.lon * p1.lat - p1.lon * p0.lat);
    }
    return 0.5 * sum;
}

std::vector<GeoPoint> PolygonTriangulator::TriangulateRing(std::span<const GeoPoint> rawRing) {
    std::vector<GeoPoint> result;
    std::vector<GeoPoint> pts = CleanRing(rawRing, /*wantCounterClockwise=*/true);
    if (pts.size() < 3) {
        return result;
    }

    if (pts.size() == 3) {
        result.assign(pts.begin(), pts.end());
        return result;
    }

    // Safety cap for extremely dense rings: if > 1800 vertices, decimate slightly for interactive ear-clipping
    if (pts.size() > 1800) {
        std::vector<GeoPoint> decimated;
        const size_t step = (pts.size() + 1199) / 1200;
        decimated.reserve(pts.size() / step + 2);
        for (size_t i = 0; i < pts.size(); i += step) {
            decimated.push_back(pts[i]);
        }
        if (decimated.size() >= 3) {
            pts = std::move(decimated);
        }
    }

    std::vector<uint32_t> indices(pts.size());
    for (size_t i = 0; i < pts.size(); ++i) {
        indices[i] = static_cast<uint32_t>(i);
    }

    result.reserve((pts.size() - 2) * 3);

    size_t nv = indices.size();
    size_t guard = nv * 3 + 16;
    size_t v = nv - 1;

    auto isEar = [&](size_t uIdx, size_t vIdx, size_t wIdx, bool strictPointCheck) noexcept -> bool {
        const GeoPoint& a = pts[indices[uIdx]];
        const GeoPoint& b = pts[indices[vIdx]];
        const GeoPoint& c = pts[indices[wIdx]];

        const double cross = Cross2D(a, b, c);
        if (cross <= 1e-15) {
            return false;
        }

        if (!strictPointCheck) {
            return true;
        }

        for (size_t p = 0; p < nv; ++p) {
            if (p == uIdx || p == vIdx || p == wIdx) continue;
            const GeoPoint& pt = pts[indices[p]];
            if (NearlyEqual(pt, a) || NearlyEqual(pt, b) || NearlyEqual(pt, c)) {
                continue;
            }
            if (PointInTriangle2D(pt, a, b, c)) {
                return false;
            }
            // Also reject if diagonal (a, c) intersects any polygon edge (pt, nextPt)
            const GeoPoint& nextPt = pts[indices[(p + 1) % nv]];
            if (SegmentsIntersectStrict(a, c, pt, nextPt)) {
                return false;
            }
        }
        return true;
    };

    bool strictMode = true;
    while (nv > 2) {
        if (guard == 0) {
            if (strictMode) {
                // Relax point-in-triangle check for self-touching / degenerate S-57 rings
                strictMode = false;
                guard = nv * 2 + 8;
            } else {
                // Final fallback: clip any remaining triangle to finish cleanly without losing rest of polygon
                const size_t u = (v == 0) ? (nv - 1) : (v - 1);
                const size_t w = (v + 1) % nv;
                result.push_back(pts[indices[u]]);
                result.push_back(pts[indices[v]]);
                result.push_back(pts[indices[w]]);
                indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(v));
                --nv;
                if (v >= nv) v = 0;
                guard = nv * 2 + 8;
                continue;
            }
        }
        --guard;

        const size_t u = v;
        v = (u + 1) % nv;
        const size_t w = (v + 1) % nv;

        if (isEar(u, v, w, strictMode)) {
            result.push_back(pts[indices[u]]);
            result.push_back(pts[indices[v]]);
            result.push_back(pts[indices[w]]);
            indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(v));
            --nv;
            if (v >= nv) v = 0;
            guard = nv * 2 + 8;
        }
    }

    return result;
}

std::vector<GeoPoint> PolygonTriangulator::Triangulate(const GeoPolygonRings& poly) {
    if (poly.exterior.size() < 3) {
        return {};
    }
    if (poly.holes.empty()) {
        return TriangulateRing(poly.exterior);
    }

    std::vector<GeoPoint> outer = CleanRing(poly.exterior, /*wantCounterClockwise=*/true);
    if (outer.size() < 3) {
        return {};
    }

    // Clean holes (clockwise orientation) and sort them by maximum longitude descending
    struct CleanedHole {
        std::vector<GeoPoint> ring;
        double maxLon{-180.0};
    };
    std::vector<CleanedHole> holes;
    holes.reserve(poly.holes.size());

    for (const auto& h : poly.holes) {
        auto cleaned = CleanRing(h, /*wantCounterClockwise=*/false);
        if (cleaned.size() >= 3) {
            double maxLon = -1e9;
            for (const auto& pt : cleaned) {
                if (pt.lon > maxLon) maxLon = pt.lon;
            }
            holes.push_back(CleanedHole{std::move(cleaned), maxLon});
        }
    }

    std::sort(holes.begin(), holes.end(), [](const CleanedHole& a, const CleanedHole& b) {
        return a.maxLon > b.maxLon;
    });

    for (const auto& h : holes) {
        SpliceHoleIntoOuterRing(outer, h.ring);
    }

    return TriangulateRing(outer);
}

bool PolygonTriangulator::PointInRing(double lon, double lat, std::span<const GeoPoint> ring) noexcept {
    if (ring.size() < 3) return false;
    bool inside = false;
    const size_t n = ring.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const double xi = ring[i].lon, yi = ring[i].lat;
        const double xj = ring[j].lon, yj = ring[j].lat;

        const bool intersect = ((yi > lat) != (yj > lat)) &&
                               (lon < (xj - xi) * (lat - yi) / (yj - yi + 1e-20) + xi);
        if (intersect) {
            inside = !inside;
        }
    }
    return inside;
}

bool PolygonTriangulator::PointInPolygon(double lon, double lat, const GeoPolygonRings& poly) noexcept {
    if (!PointInRing(lon, lat, poly.exterior)) {
        return false;
    }
    for (const auto& hole : poly.holes) {
        if (PointInRing(lon, lat, hole)) {
            return false;
        }
    }
    return true;
}

} // namespace guikit
