#pragma once

#include <vector>
#include "guikit/geo/GeoTypes.hpp"

namespace guikit::geo {

/// Robust 2D Polygon Triangulator using Eberly's Hole-Bridging + Ear-Clipping algorithm.
/// Supports convex and concave polygons with arbitrary interior hole rings.
class PolygonTriangulator {
public:
    /// Triangulates a single polygon (exterior ring + interior hole rings) into `poly.triangles`
    /// and updates `poly.bounds`.
    static void TriangulatePolygon(GeoPolygon& poly);

    /// Computes signed area of a closed ring in (lon, lat) coordinates.
    /// Positive = Counter-Clockwise (CCW), Negative = Clockwise (CW).
    [[nodiscard]] static double SignedAreaLonLat(const std::vector<guikit::GeoCoord>& ring) noexcept;
};

} // namespace guikit::geo

namespace guikit {
using namespace geo;
} // namespace guikit
