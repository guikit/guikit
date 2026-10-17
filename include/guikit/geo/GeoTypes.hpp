#pragma once

#include "guikit/core/Types.hpp"
#include "guikit/geo/MapGrid.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace guikit {

/// 2D geographic coordinate in WGS84 degrees with optional depth/elevation (Z in meters, positive down for depth).
struct GeoPoint {
    double lon{0.0}; ///< Longitude in degrees [-180..+180]
    double lat{0.0}; ///< Latitude in degrees [-90..+90]
    double z{0.0};   ///< Optional depth/elevation in meters
    bool hasZ{false};

    [[nodiscard]] constexpr GeoCoord ToGeoCoord() const noexcept {
        return GeoCoord{lat, lon};
    }

    static constexpr GeoPoint FromGeoCoord(GeoCoord c, double depth = 0.0, bool withZ = false) noexcept {
        return GeoPoint{c.lon, c.lat, depth, withZ};
    }
};

/// Axis-aligned geographic bounding box in WGS84 degrees.
struct GeoBBox {
    double minLon{ 180.0};
    double minLat{  90.0};
    double maxLon{-180.0};
    double maxLat{ -90.0};

    [[nodiscard]] constexpr bool IsValid() const noexcept {
        return minLon <= maxLon && minLat <= maxLat;
    }

    constexpr void Expand(double lon, double lat) noexcept {
        if (lon < minLon) minLon = lon;
        if (lon > maxLon) maxLon = lon;
        if (lat < minLat) minLat = lat;
        if (lat > maxLat) maxLat = lat;
    }

    constexpr void Expand(const GeoPoint& pt) noexcept {
        Expand(pt.lon, pt.lat);
    }

    constexpr void Expand(const GeoBBox& other) noexcept {
        if (!other.IsValid()) return;
        Expand(other.minLon, other.minLat);
        Expand(other.maxLon, other.maxLat);
    }

    [[nodiscard]] constexpr bool Intersects(const GeoBBox& other) const noexcept {
        if (!IsValid() || !other.IsValid()) return false;
        return !(maxLon < other.minLon || minLon > other.maxLon ||
                 maxLat < other.minLat || minLat > other.maxLat);
    }

    [[nodiscard]] constexpr bool Contains(double lon, double lat) const noexcept {
        return IsValid() && lon >= minLon && lon <= maxLon && lat >= minLat && lat <= maxLat;
    }

    [[nodiscard]] constexpr GeoCoord Center() const noexcept {
        if (!IsValid()) return {0.0, 0.0};
        return {(minLat + maxLat) * 0.5, (minLon + maxLon) * 0.5};
    }

    [[nodiscard]] constexpr double SpanLon() const noexcept {
        return IsValid() ? (maxLon - minLon) : 0.0;
    }

    [[nodiscard]] constexpr double SpanLat() const noexcept {
        return IsValid() ? (maxLat - minLat) : 0.0;
    }
};

/// Vector geometry primitive types (OGC Simple Features / GeoJSON).
enum class GeometryType : uint8_t {
    Point = 0,
    MultiPoint,
    LineString,
    MultiLineString,
    Polygon,
    MultiPolygon
};

[[nodiscard]] constexpr std::string_view GeometryTypeName(GeometryType t) noexcept {
    switch (t) {
        case GeometryType::Point:           return "Point";
        case GeometryType::MultiPoint:      return "MultiPoint";
        case GeometryType::LineString:      return "LineString";
        case GeometryType::MultiLineString: return "MultiLineString";
        case GeometryType::Polygon:         return "Polygon";
        case GeometryType::MultiPolygon:    return "MultiPolygon";
    }
    return "Unknown";
}

/// Maritime S-57 / Navionics / general GIS feature classification.
enum class GisFeatureClass : uint8_t {
    Generic = 0,
    LandArea,       ///< LNDARE - Land mass polygon
    Coastline,      ///< COALNE / SLCONS - Shoreline / construction
    DepthArea,      ///< DEPARE / DRGARE - Bathymetric depth zone polygon
    DepthContour,   ///< DEPCNT - Bathymetric isobath line
    Sounding,       ///< SOUNDG - Spot depth sounding (meters)
    Seamark,        ///< BOY*, BCN*, LNDMRK - Buoy, beacon, navigational aid
    Light,          ///< LIGHTS - Lighthouse / sector light
    UnderwaterHazard,///< UWTROC, WRECKS, OBSTRN - Rock, wreck, obstruction
    RestrictedArea, ///< RESARE, ACHARE,FSHZNE - Restricted / anchorage / Caution area
    Fairway,        ///< FAIRWY, DWRTCL, NAVLNE - Navigation channel / route / track
    CablePipeline,  ///< CBLSUB, PIPSOL - Submarine cable or pipeline
    UrbanBuilding,  ///< BUISGL, BUAARE - Building / Harbor facility
    RoadRailway     ///< ROADWY, RAILWY - Land transport
};

[[nodiscard]] constexpr std::string_view GisFeatureClassName(GisFeatureClass c) noexcept {
    switch (c) {
        case GisFeatureClass::Generic:          return "Generic Feature";
        case GisFeatureClass::LandArea:         return "Land Area (LNDARE)";
        case GisFeatureClass::Coastline:        return "Coastline (COALNE)";
        case GisFeatureClass::DepthArea:        return "Depth Area (DEPARE)";
        case GisFeatureClass::DepthContour:     return "Depth Contour (DEPCNT)";
        case GisFeatureClass::Sounding:         return "Spot Sounding (SOUNDG)";
        case GisFeatureClass::Seamark:          return "Seamark / Buoy (IALA)";
        case GisFeatureClass::Light:            return "Navigational Light (LIGHTS)";
        case GisFeatureClass::UnderwaterHazard: return "Underwater Hazard (UWTROC/WRECKS)";
        case GisFeatureClass::RestrictedArea:   return "Restricted / Anchorage Area";
        case GisFeatureClass::Fairway:          return "Fairway / Navigation Channel";
        case GisFeatureClass::CablePipeline:    return "Submarine Cable / Pipeline";
        case GisFeatureClass::UrbanBuilding:    return "Harbor Facility / Building";
        case GisFeatureClass::RoadRailway:      return "Road / Infrastructure";
    }
    return "Unknown";
}

/// Key-value attribute property attached to a GIS feature.
struct GeoAttribute {
    std::string key;
    std::string value;
};

/// Single polygon with one exterior ring and zero or more interior hole rings.
struct GeoPolygonRings {
    std::vector<GeoPoint> exterior;
    std::vector<std::vector<GeoPoint>> holes;

    /// Pre-triangulated triangle indices into a flattened vertex array, or triplets of points.
    /// Each 3 consecutive GeoPoints in `CachedTriangles` form one filled triangle.
    std::vector<GeoPoint> cachedTriangles;
};

/// Visual style override or layer-derived styling for a feature.
struct GisFeatureStyle {
    Color fillColor{56, 189, 248, 70};
    Color strokeColor{56, 189, 248, 220};
    float strokeWidthPx{1.6f};
    float pointRadiusPx{5.5f};
    bool dashed{false};
    bool filled{true};
    bool stroked{true};
};

/// A single GIS vector feature with geometry, bounding box, S-57/GIS attributes, and triangulated cache.
struct GeoFeature {
    uint64_t id{0};
    std::string featureCode; ///< e.g. "DEPARE", "DEPCNT", "SOUNDG", "BOYLAT"
    std::string name;        ///< Display label (OBJNAM / name / title)
    GeometryType geometryType{GeometryType::Point};
    GisFeatureClass featureClass{GisFeatureClass::Generic};

    // Geometry storage:
    // - Point / MultiPoint: `points`
    // - LineString / MultiLineString: `lines`
    // - Polygon / MultiPolygon: `polygons`
    std::vector<GeoPoint> points;
    std::vector<std::vector<GeoPoint>> lines;
    std::vector<GeoPolygonRings> polygons;

    // Hydrographic / Bathymetric numeric attributes (parsed for fast S-52 / Navionics color shading)
    std::optional<double> depthMinMeters;   ///< DRVAL1 (minimum depth of depth area)
    std::optional<double> depthMaxMeters;   ///< DRVAL2 (maximum depth of depth area)
    std::optional<double> contourDepthMeters; ///< VALDCO / VALSOU / depth value

    std::vector<GeoAttribute> attributes;
    GeoBBox bbox{};

    [[nodiscard]] std::string_view GetAttribute(std::string_view key) const noexcept;
    void SetAttribute(std::string key, std::string value);
    void RecomputeBBox() noexcept;
    [[nodiscard]] size_t TotalVertexCount() const noexcept;
};

/// A named thematic GIS vector layer containing a collection of features.
struct GeoLayer {
    uint32_t id{0};
    std::string name;
    std::string sourcePath;
    bool visible{true};
    bool locked{false};
    float opacity{1.0f};
    int zOrder{0}; ///< Lower renders first (background polygons), higher renders on top (points/text)

    GisFeatureClass dominantClass{GisFeatureClass::Generic};
    GisFeatureStyle defaultStyle{};
    std::vector<GeoFeature> features;
    GeoBBox bbox{};

    void RecomputeBBox() noexcept;
    [[nodiscard]] size_t TotalVertexCount() const noexcept;
};

/// Classifies a feature based on S-57 acronym (`OBJL` / `layer` / `type`) or GeoJSON properties.
[[nodiscard]] GisFeatureClass ClassifyFeatureFromAttributes(const GeoFeature& feature) noexcept;

/// Computes default z-order priority for a feature class (0 = Land/Depth polygons, 100 = Soundings/Seamarks).
[[nodiscard]] int DefaultZOrderForClass(GisFeatureClass cls) noexcept;

} // namespace guikit
