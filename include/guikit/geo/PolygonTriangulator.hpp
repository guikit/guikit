#pragma once

#include "guikit/geo/GeoTypes.hpp"

#include <span>
#include <vector>

namespace guikit {

/// 2D Polygon Triangulator (Ear-Clipping with Hole Bridge Merging) and Point-in-Polygon hit testing.
class PolygonTriangulator {
public:
    /// Triangulates a simple or concave exterior ring into a flat list of triangle vertices (size = 3 * numTriangles).
    [[nodiscard]] static std::vector<GeoPoint> TriangulateRing(std::span<const GeoPoint> ring);

    /// Triangulates a polygon with an exterior ring and optional interior holes into a flat list of triangle vertices.
    [[nodiscard]] static std::vector<GeoPoint> Triangulate(const GeoPolygonRings& poly);

    /// Ray-casting point-in-ring test in (lon, lat) space.
    [[nodiscard]] static bool PointInRing(double lon, double lat, std::span<const GeoPoint> ring) noexcept;

    /// Point-in-polygon test accounting for exterior boundary and interior holes.
    [[nodiscard]] static bool PointInPolygon(double lon, double lat, const GeoPolygonRings& poly) noexcept;

    /// Signed area of a ring in (lon, lat) degrees^2 (> 0 for counter-clockwise, < 0 for clockwise).
    [[nodiscard]] static double SignedArea(std::span<const GeoPoint> ring) noexcept;
};

} // namespace guikit
