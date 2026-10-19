#include "guikit/geo/PolygonTriangulator.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace guikit::geo {

namespace {

constexpr double kEps = 1e-11;

[[nodiscard]] bool NearlyEqual(guikit::GeoCoord a, guikit::GeoCoord b) noexcept {
    return std::abs(a.lon - b.lon) <= kEps && std::abs(a.lat - b.lat) <= kEps;
}

/// 2D cross product (b - a) x (c - a) in (lon, lat) space.
/// > 0 if (a, b, c) is a left turn (CCW), < 0 if right turn (CW).
[[nodiscard]] double Cross2D(guikit::GeoCoord a, guikit::GeoCoord b, guikit::GeoCoord c) noexcept {
    return (b.lon - a.lon) * (c.lat - a.lat) - (b.lat - a.lat) * (c.lon - a.lon);
}

/// Returns true if point p lies inside or on the boundary of CCW triangle (a, b, c).
[[nodiscard]] bool PointInTriangleCCW(
    guikit::GeoCoord p,
    guikit::GeoCoord a,
    guikit::GeoCoord b,
    guikit::GeoCoord c
) noexcept {
    const double c1 = Cross2D(a, b, p);
    const double c2 = Cross2D(b, c, p);
    const double c3 = Cross2D(c, a, p);
    return (c1 >= -kEps) && (c2 >= -kEps) && (c3 >= -kEps);
}

/// Cleans a polygon ring by removing duplicate consecutive points and closing duplicate vertex.
[[nodiscard]] std::vector<guikit::GeoCoord> CleanRing(const std::vector<guikit::GeoCoord>& raw) {
    std::vector<guikit::GeoCoord> cleaned;
    cleaned.reserve(raw.size());
    for (const auto& pt : raw) {
        if (cleaned.empty() || !NearlyEqual(cleaned.back(), pt)) {
            cleaned.push_back(pt);
        }
    }
    while (cleaned.size() >= 2 && NearlyEqual(cleaned.front(), cleaned.back())) {
        cleaned.pop_back();
    }
    return cleaned;
}

/// Splices a CW hole ring into the CCW outer contour using Eberly's horizontal ray bridge method.
void BridgeHoleIntoContour(
    std::vector<guikit::GeoCoord>& contour,
    const std::vector<guikit::GeoCoord>& hole
) {
    if (contour.size() < 3 || hole.size() < 3) {
        return;
    }

    // 1. Find rightmost vertex M of the hole (maximum longitude)
    size_t holeMaxIdx = 0;
    for (size_t i = 1; i < hole.size(); ++i) {
        if (hole[i].lon > hole[holeMaxIdx].lon ||
            (std::abs(hole[i].lon - hole[holeMaxIdx].lon) <= kEps && hole[i].lat > hole[holeMaxIdx].lat)) {
            holeMaxIdx = i;
        }
    }
    const guikit::GeoCoord M = hole[holeMaxIdx];

    // 2. Cast ray M + t * (1, 0) (eastward along constant latitude M.lat) against all contour edges
    double bestIntersectLon = std::numeric_limits<double>::infinity();
    size_t bestEdgeEndpointIdx = std::numeric_limits<size_t>::max();
    guikit::GeoCoord I{};

    const size_t n = contour.size();
    for (size_t i = 0; i < n; ++i) {
        const guikit::GeoCoord v0 = contour[i];
        const guikit::GeoCoord v1 = contour[(i + 1) % n];

        // Check if horizontal line y = M.lat intersects segment (v0, v1)
        const bool straddles = (v0.lat > M.lat) != (v1.lat > M.lat);
        if (!straddles) {
            // Handle horizontal or vertex-touching case
            if (std::abs(v0.lat - M.lat) <= kEps && v0.lon >= M.lon - kEps) {
                if (v0.lon < bestIntersectLon) {
                    bestIntersectLon = v0.lon;
                    I = v0;
                    bestEdgeEndpointIdx = i;
                }
            }
            continue;
        }

        // Only consider edges where the contour goes downward/upward to the right of M
        const double t = (M.lat - v0.lat) / (v1.lat - v0.lat);
        const double xIntersect = v0.lon + t * (v1.lon - v0.lon);
        if (xIntersect >= M.lon - kEps && xIntersect < bestIntersectLon) {
            bestIntersectLon = xIntersect;
            I = {M.lat, xIntersect};
            // Pick the endpoint of the intersected edge with greater longitude
            bestEdgeEndpointIdx = (v0.lon >= v1.lon) ? i : ((i + 1) % n);
        }
    }

    // Fallback: if ray missed due to numerical edge cases, connect to closest contour vertex
    if (bestEdgeEndpointIdx == std::numeric_limits<size_t>::max()) {
        double minDistSq = std::numeric_limits<double>::infinity();
        bestEdgeEndpointIdx = 0;
        for (size_t i = 0; i < n; ++i) {
            const double dx = contour[i].lon - M.lon;
            const double dy = contour[i].lat - M.lat;
            const double d2 = dx * dx + dy * dy;
            if (d2 < minDistSq) {
                minDistSq = d2;
                bestEdgeEndpointIdx = i;
            }
        }
    } else {
        // 3. Check if any reflex contour vertex lies inside triangle (M, I, P)
        const guikit::GeoCoord P = contour[bestEdgeEndpointIdx];
        if (!NearlyEqual(I, P)) {
            guikit::GeoCoord triA = M;
            guikit::GeoCoord triB = I;
            guikit::GeoCoord triC = P;
            if (Cross2D(triA, triB, triC) < 0.0) {
                std::swap(triB, triC);
            }

            double minAngleCos = -2.0; // We want smallest angle with +X axis (i.e. largest cos)
            double minDistSq = std::numeric_limits<double>::infinity();
            size_t visibleIdx = bestEdgeEndpointIdx;

            for (size_t i = 0; i < n; ++i) {
                if (i == bestEdgeEndpointIdx) continue;
                const guikit::GeoCoord prev = contour[(i + n - 1) % n];
                const guikit::GeoCoord curr = contour[i];
                const guikit::GeoCoord next = contour[(i + 1) % n];

                // Reflex vertex check in CCW contour
                if (Cross2D(prev, curr, next) <= kEps) {
                    if (PointInTriangleCCW(curr, triA, triB, triC)) {
                        const double dx = curr.lon - M.lon;
                        const double dy = curr.lat - M.lat;
                        const double distSq = dx * dx + dy * dy;
                        if (distSq > kEps) {
                            const double angleCos = dx / std::sqrt(distSq);
                            if (angleCos > minAngleCos + kEps ||
                                (std::abs(angleCos - minAngleCos) <= kEps && distSq < minDistSq)) {
                                minAngleCos = angleCos;
                                minDistSq = distSq;
                                visibleIdx = i;
                            }
                        }
                    }
                }
            }
            bestEdgeEndpointIdx = visibleIdx;
        }
    }

    // 4. Splice hole ring into contour at `bestEdgeEndpointIdx` via a bridge edge:
    // contour[0 .. bestEdgeEndpointIdx] -> hole[holeMaxIdx .. holeMaxIdx] -> contour[bestEdgeEndpointIdx .. n-1]
    std::vector<guikit::GeoCoord> merged;
    merged.reserve(contour.size() + hole.size() + 2);

    for (size_t i = 0; i <= bestEdgeEndpointIdx; ++i) {
        merged.push_back(contour[i]);
    }
    for (size_t i = 0; i < hole.size(); ++i) {
        merged.push_back(hole[(holeMaxIdx + i) % hole.size()]);
    }
    merged.push_back(hole[holeMaxIdx]);
    for (size_t i = bestEdgeEndpointIdx; i < contour.size(); ++i) {
        merged.push_back(contour[i]);
    }

    contour = std::move(merged);
}

} // namespace

double PolygonTriangulator::SignedAreaLonLat(const std::vector<guikit::GeoCoord>& ring) noexcept {
    if (ring.size() < 3) return 0.0;
    double area2 = 0.0;
    const size_t n = ring.size();
    for (size_t i = 0; i < n; ++i) {
        const auto& p0 = ring[i];
        const auto& p1 = ring[(i + 1) % n];
        area2 += (p0.lon * p1.lat - p1.lon * p0.lat);
    }
    return 0.5 * area2;
}

void PolygonTriangulator::TriangulatePolygon(GeoPolygon& poly) {
    poly.triangles.clear();
    poly.hasValidBounds = false;

    for (const auto& pt : poly.exteriorRing) {
        SrsTransform::ExpandBounds(poly.bounds, poly.hasValidBounds, pt);
    }
    for (const auto& hole : poly.interiorRings) {
        for (const auto& pt : hole) {
            SrsTransform::ExpandBounds(poly.bounds, poly.hasValidBounds, pt);
        }
    }

    std::vector<guikit::GeoCoord> contour = CleanRing(poly.exteriorRing);
    if (contour.size() < 3) {
        return;
    }

    // Ensure exterior contour is Counter-Clockwise (CCW)
    if (SignedAreaLonLat(contour) < 0.0) {
        std::reverse(contour.begin(), contour.end());
    }

    // Prepare and bridge interior hole rings (each oriented Clockwise)
    if (!poly.interiorRings.empty()) {
        struct PreparedHole {
            std::vector<guikit::GeoCoord> ring;
            double maxLon{-std::numeric_limits<double>::infinity()};
        };
        std::vector<PreparedHole> holes;
        holes.reserve(poly.interiorRings.size());

        for (const auto& rawHole : poly.interiorRings) {
            auto cleaned = CleanRing(rawHole);
            if (cleaned.size() < 3) continue;
            if (SignedAreaLonLat(cleaned) > 0.0) {
                std::reverse(cleaned.begin(), cleaned.end());
            }
            double maxLon = cleaned[0].lon;
            for (const auto& pt : cleaned) {
                maxLon = std::max(maxLon, pt.lon);
            }
            holes.push_back({std::move(cleaned), maxLon});
        }

        // Sort holes from rightmost to leftmost as required by Eberly's algorithm
        std::sort(holes.begin(), holes.end(), [](const PreparedHole& a, const PreparedHole& b) {
            return a.maxLon > b.maxLon;
        });

        for (const auto& h : holes) {
            BridgeHoleIntoContour(contour, h.ring);
        }
    }

    // Ear-Clipping on the single merged simple polygon `contour`
    const size_t totalVerts = contour.size();
    if (totalVerts < 3) return;

    poly.triangles.reserve(totalVerts - 2);
    std::vector<uint32_t> indices(totalVerts);
    std::iota(indices.begin(), indices.end(), 0u);

    auto isEar = [&](size_t idxInList, bool strictPointCheck) -> bool {
        const size_t m = indices.size();
        const guikit::GeoCoord a = contour[indices[(idxInList + m - 1) % m]];
        const guikit::GeoCoord b = contour[indices[idxInList]];
        const guikit::GeoCoord c = contour[indices[(idxInList + 1) % m]];

        // Must be a convex vertex (left turn in CCW contour)
        const double cross = Cross2D(a, b, c);
        if (cross <= kEps) {
            return false;
        }

        if (!strictPointCheck) {
            return true;
        }

        // No other vertex of the remaining polygon may lie strictly inside triangle (a, b, c)
        for (size_t j = 0; j < m; ++j) {
            if (j == idxInList || j == (idxInList + m - 1) % m || j == (idxInList + 1) % m) {
                continue;
            }
            const guikit::GeoCoord p = contour[indices[j]];
            if (NearlyEqual(p, a) || NearlyEqual(p, b) || NearlyEqual(p, c)) {
                continue;
            }
            if (PointInTriangleCCW(p, a, b, c)) {
                return false;
            }
        }
        return true;
    };

    size_t guardCounter = 0;
    const size_t maxIterations = totalVerts * totalVerts + 32;

    while (indices.size() > 3 && guardCounter++ < maxIterations) {
        const size_t m = indices.size();
        bool clipped = false;

        // Pass 1: Strict ear test
        for (size_t i = 0; i < m; ++i) {
            if (isEar(i, /*strictPointCheck=*/true)) {
                const guikit::GeoCoord a = contour[indices[(i + m - 1) % m]];
                const guikit::GeoCoord b = contour[indices[i]];
                const guikit::GeoCoord c = contour[indices[(i + 1) % m]];
                poly.triangles.push_back({a, b, c});
                indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(i));
                clipped = true;
                break;
            }
        }
        if (clipped) continue;

        // Pass 2: Relaxed convex ear test (for self-touching / bridge-degenerate vertices)
        for (size_t i = 0; i < m; ++i) {
            if (isEar(i, /*strictPointCheck=*/false)) {
                const guikit::GeoCoord a = contour[indices[(i + m - 1) % m]];
                const guikit::GeoCoord b = contour[indices[i]];
                const guikit::GeoCoord c = contour[indices[(i + 1) % m]];
                poly.triangles.push_back({a, b, c});
                indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(i));
                clipped = true;
                break;
            }
        }
        if (clipped) continue;

        // Pass 3: Collinear / degenerate vertex removal to guarantee termination
        const guikit::GeoCoord a = contour[indices[m - 1]];
        const guikit::GeoCoord b = contour[indices[0]];
        const guikit::GeoCoord c = contour[indices[1]];
        if (std::abs(Cross2D(a, b, c)) > kEps) {
            poly.triangles.push_back({a, b, c});
        }
        indices.erase(indices.begin());
    }

    if (indices.size() == 3) {
        const guikit::GeoCoord a = contour[indices[0]];
        const guikit::GeoCoord b = contour[indices[1]];
        const guikit::GeoCoord c = contour[indices[2]];
        if (std::abs(Cross2D(a, b, c)) > kEps) {
            poly.triangles.push_back({a, b, c});
        }
    }
}

} // namespace guikit::geo
