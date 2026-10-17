#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <string>
#include <string_view>
#include <vector>
#include "guikit/core/Types.hpp"

namespace guikit {

/// Coordinate Reference System (CRS) / Map Projection model.
enum class MapCrs : uint8_t {
    EPSG3857_WebMercator = 0,   ///< Spherical Web Mercator (OSM, OpenSeaMap, Google/Mapbox standard)
    EPSG3395_WorldMercator,     ///< WGS84 Ellipsoidal Mercator (True conformal nautical chart projection)
    EPSG4326_PlateCarree,       ///< Equirectangular WGS84 Geographic Lon/Lat grid
    EPSG32636_Utm36N,           ///< WGS84 / UTM Zone 36N (30°E..36°E, CM 33°E — Black Sea / Sevastopol)
    EPSG20906_Gsk2011Zone6,     ///< GSK-2011 / Gauss-Kruger Zone 6 (CM 33°E — RF State Geodetic Standard)
    EPSG28406_Pulkovo42Zone6,   ///< Pulkovo 1942 (SK-42) / Gauss-Kruger Zone 6 (CM 33°E — Krassowsky 1940 + GOST)
    EPSG3995_ArcticPolar        ///< WGS84 / Arctic Polar Stereographic (Standard parallel 71°N — IHO High-Latitude)
};

/// Display formatting mode for geographic and geodetic planar coordinates.
enum class GeoCoordFormat : uint8_t {
    NauticalDMM = 0,            ///< Degrees & Decimal Minutes: 54° 10.92' N, 007° 53.20' E (1' lat = 1 NM)
    DecimalDegrees,             ///< Signed/cardinal decimal degrees: 54.1820° N, 007.8867° E
    DMS,                        ///< Degrees, Minutes, Seconds: 54° 10' 55" N, 007° 53' 12" E
    UTM_Meters,                 ///< WGS84 UTM Zone + Easting/Northing in meters (e.g. 36N E 691425 m, N 4938121 m)
    GaussKruger_Meters          ///< GOST Gauss-Kruger Zone + X (North) / Y (East) in meters (GSK-2011 / SK-42)
};

/// Reference ellipsoid parameters (semi-major axis a and flattening f).
struct EllipsoidSpec {
    double a{6378137.0};
    double f{1.0 / 298.257223563};

    [[nodiscard]] static constexpr EllipsoidSpec Wgs84() noexcept {
        return {6378137.0, 1.0 / 298.257223563};
    }
    [[nodiscard]] static constexpr EllipsoidSpec Gsk2011() noexcept {
        return {6378136.5, 1.0 / 298.2564151};
    }
    [[nodiscard]] static constexpr EllipsoidSpec Krassowsky1940() noexcept {
        return {6378245.0, 1.0 / 298.3};
    }
    [[nodiscard]] static constexpr EllipsoidSpec Pz90_11() noexcept {
        return {6378136.0, 1.0 / 298.25784};
    }
};

/// 7-parameter Helmert datum transformation to WGS84 according to GOST 32453-2017.
struct Helmert7Spec {
    double dx{0.0};             ///< X translation in meters (to WGS84)
    double dy{0.0};             ///< Y translation in meters (to WGS84)
    double dz{0.0};             ///< Z translation in meters (to WGS84)
    double wxArcSec{0.0};       ///< X rotation in arc-seconds
    double wyArcSec{0.0};       ///< Y rotation in arc-seconds
    double wzArcSec{0.0};       ///< Z rotation in arc-seconds
    double scalePpm{0.0};       ///< Scale difference in parts per million (10^-6)
    bool isIdentity{true};

    [[nodiscard]] static constexpr Helmert7Spec Identity() noexcept {
        return {};
    }
    /// GOST 32453-2017: Pulkovo 1942 (SK-42) -> WGS84 / PZ-90.11
    [[nodiscard]] static constexpr Helmert7Spec Sk42ToWgs84() noexcept {
        return {+23.57, -140.95, -79.80, 0.0, -0.35, -0.79, -0.22, false};
    }
    /// GOST 32453-2017: SK-95 -> WGS84 / PZ-90.11
    [[nodiscard]] static constexpr Helmert7Spec Sk95ToWgs84() noexcept {
        return {+24.47, -130.89, -81.56, 0.0, 0.0, -0.13, -0.22, false};
    }
    /// GOST 32453-2017: PZ-90.11 -> WGS84
    [[nodiscard]] static constexpr Helmert7Spec Pz90ToWgs84() noexcept {
        return {-0.013, +0.106, +0.022, +0.0023, -0.00354, +0.00421, -0.008, false};
    }
};

/// Parametric Transverse Mercator / Gauss-Kruger configuration.
struct TransverseMercatorSpec {
    EllipsoidSpec ellipsoid{EllipsoidSpec::Wgs84()};
    Helmert7Spec datumToWgs84{Helmert7Spec::Identity()};
    double centralMeridianDeg{33.0};
    double scaleFactor{0.9996};
    double falseEasting{500000.0};
    double falseNorthing{0.0};
};

/// Geographic coordinate in degrees (WGS84).
struct GeoCoord {
    double lat{0.0};            ///< Latitude in degrees [-90..+90] (North positive)
    double lon{0.0};            ///< Longitude in degrees [-180..+180] (East positive)
};

/// Geographic bounding box defined by North-West and South-East corners.
struct GeoBounds {
    GeoCoord nw{};              ///< North-West corner (max lat, min lon)
    GeoCoord se{};              ///< South-East corner (min lat, max lon)

    [[nodiscard]] constexpr double WidthLon() const noexcept { return se.lon - nw.lon; }
    [[nodiscard]] constexpr double HeightLat() const noexcept { return nw.lat - se.lat; }
};

/// Slippy Map tile pyramid address [z/x/y].
struct OsmTileId {
    int z{0};                   ///< Zoom level [0..19]
    int x{0};                   ///< Tile column [0..2^z - 1] (West -> East)
    int y{0};                   ///< Tile row [0..2^z - 1] (North -> South)

    [[nodiscard]] constexpr bool operator==(const OsmTileId& o) const noexcept {
        return z == o.z && x == o.x && y == o.y;
    }
};

/// Resolved geometry and geographic bounds of a single visible tile in Scene world space.
struct OsmTileCell {
    OsmTileId id{};
    Rect worldRect{};           ///< Bounding rectangle in Scene world coordinates
    GeoBounds geoBounds{};      ///< Geographic NW/SE bounds of this tile
};

/// Single meridian (constant longitude) or parallel (constant latitude) in the nautical graticule.
struct GraticuleLine {
    double degrees{0.0};        ///< Latitude or longitude in degrees
    float worldPos{0.0f};       ///< X (for meridian) or Y (for parallel) in Scene world coordinates
    int stepIndex{0};           ///< Signed integer multiple of stepDeg from 0° (for pan-invariant ruler zebra bars)
    bool isMajor{false};        ///< True if whole degree or major 5x subdivision
    bool isZero{false};         ///< True for Equator (0° lat) or Prime Meridian (0° lon)
};

/// Computed physical scale bar specification (Metric or Nautical).
struct MapScaleBar {
    double distanceValue{0.0};  ///< Numeric distance value (in meters or nautical miles)
    float screenPixels{80.0f};  ///< Exact bar length in screen pixels at current view center
    char label[32]{};           ///< Formatted label (e.g. "500 m", "2 km", "5 NM", "2 cab")
};

/// Universal callback for resolving a tile image source (file path, URI, or cached PNG/SVG asset).
/// `layerIndex == 0` is the base map (e.g. OSM), `layerIndex == 1` is an overlay (e.g. OpenSeaMap seamark).
using TileSourceCallback = std::string_view (*)(OsmTileId tile, int layerIndex, void* userData);

/// Configuration for map projection, tile pyramid, and nautical graticule calculation.
struct MapGridConfig {
    MapCrs crs{MapCrs::EPSG3857_WebMercator};
    float worldSize{2048.0f};   ///< Scene world extent (0..worldSize in X and Y)
    int baseZoom{3};            ///< Tile zoom level corresponding to sceneZoom == 1.0f (2048 / 256 = 8 = 2^3)
    int minTileZoom{1};
    int maxTileZoom{18};
    float zoomBias{0.0f};       ///< Optional LOD offset applied before selecting integer tile level z
    TileSourceCallback tileSource{nullptr};
    void* tileSourceUserData{nullptr};
};

/// Complete per-frame calculated state of the map view, visible tile pyramid, and nautical graticule.
struct MapViewState {
    MapCrs crs{MapCrs::EPSG3857_WebMercator};
    float continuousZoom{3.0f}; ///< Smooth fractional zoom level: baseZoom + log2(sceneZoom)
    int tileZoom{3};            ///< Active discrete tile pyramid level z
    int totalTilesPerAxis{8};   ///< 2^z tiles along each axis
    int minTileX{0};
    int maxTileX{0};
    int minTileY{0};
    int maxTileY{0};
    int visibleTileCount{0};

    GeoCoord center{};          ///< Geographic coordinates at the center of the viewport
    GeoCoord cursor{};          ///< Geographic coordinates directly under the mouse cursor
    GeoBounds visibleBounds{};  ///< Geographic NW/SE bounds of the visible viewport

    double metersPerPixel{1.0}; ///< Ground resolution at center latitude (m / screen px)
    double graticuleStepArcMin{60.0}; ///< Active nautical graticule step in arc-minutes (1' = 1 NM)
    MapScaleBar metricScale{};  ///< Metric scale bar (m / km)
    MapScaleBar nauticalScale{};///< Nautical scale bar (NM / cables, 1 NM = 1852 m)

    std::vector<OsmTileCell> visibleTiles{};
    std::vector<GraticuleLine> meridians{}; ///< Visible vertical longitude lines
    std::vector<GraticuleLine> parallels{}; ///< Visible horizontal latitude lines
};

/// Pure mathematical engine for Web Mercator / WGS84 / UTM / Gauss-Kruger / Polar Stereographic projections,
/// GOST 32453-2017 Helmert datum transformations, OSM [z/x/y] tile grids, and OpenSeaMap nautical graticules.
class MapGrid {
public:
    static constexpr double kEarthRadiusMeters = 6378137.0;
    static constexpr double kEarthCircumferenceMeters = 2.0 * std::numbers::pi * kEarthRadiusMeters; // 40075016.6856 m
    static constexpr double kWgs84Eccentricity = 0.08181919084262149; // WGS84 ellipsoid eccentricity e
    static constexpr double kMetersPerNauticalMile = 1852.0;          // Exact international nautical mile (1' latitude)
    static constexpr double kMaxMercatorLat = 85.05112877980659;      // atan(sinh(pi)) * 180 / pi

    /// Returns true if the CRS is a standard axis-aligned cylindrical projection (3857, 3395, 4326).
    [[nodiscard]] static constexpr bool IsCylindricalCrs(MapCrs crs) noexcept {
        return crs == MapCrs::EPSG3857_WebMercator ||
               crs == MapCrs::EPSG3395_WorldMercator ||
               crs == MapCrs::EPSG4326_PlateCarree;
    }

    /// Returns a concise human-readable EPSG & projection label for UI telemetry.
    [[nodiscard]] static constexpr std::string_view CrsDisplayName(MapCrs crs) noexcept {
        switch (crs) {
            case MapCrs::EPSG3857_WebMercator:    return "EPSG:3857 (Web Mercator)";
            case MapCrs::EPSG3395_WorldMercator:  return "EPSG:3395 (WGS84 World Mercator)";
            case MapCrs::EPSG4326_PlateCarree:    return "EPSG:4326 (WGS84 Equirectangular)";
            case MapCrs::EPSG32636_Utm36N:        return "EPSG:32636 (WGS84 / UTM Zone 36N)";
            case MapCrs::EPSG20906_Gsk2011Zone6:  return "EPSG:20906 (GSK-2011 / GK Zone 6)";
            case MapCrs::EPSG28406_Pulkovo42Zone6:return "EPSG:28406 (Pulkovo 1942 / SK-42 Z6)";
            case MapCrs::EPSG3995_ArcticPolar:    return "EPSG:3995 (Arctic Polar Stereo 71°N)";
        }
        return "EPSG:3857 (Web Mercator)";
    }

    /// Clamps latitude to the valid domain of the specified CRS.
    [[nodiscard]] static constexpr double ClampLat(double lat, MapCrs crs = MapCrs::EPSG3857_WebMercator) noexcept {
        if (crs == MapCrs::EPSG3995_ArcticPolar) {
            return std::clamp(lat, -30.0, 89.9999);
        }
        const bool isMerc = (crs == MapCrs::EPSG3857_WebMercator || crs == MapCrs::EPSG3395_WorldMercator);
        const double limit = isMerc ? kMaxMercatorLat : 89.9999;
        return std::clamp(lat, -limit, limit);
    }

    /// Wraps or clamps longitude to [-180..+180].
    [[nodiscard]] static constexpr double ClampLon(double lon) noexcept {
        return std::clamp(lon, -180.0, 180.0);
    }

    /// Applies a 7-parameter GOST 32453-2017 Helmert transformation from source ellipsoid to WGS84 (or inverse if `inverse == true`).
    [[nodiscard]] static GeoCoord TransformDatumHelmert(
        GeoCoord geo,
        const EllipsoidSpec& srcEllipsoid,
        const EllipsoidSpec& dstEllipsoid,
        const Helmert7Spec& toWgs84,
        bool inverse = false
    ) noexcept {
        if (toWgs84.isIdentity) {
            return geo;
        }

        constexpr double kDegToRad = std::numbers::pi / 180.0;
        constexpr double kRadToDeg = 180.0 / std::numbers::pi;
        constexpr double kArcSecToRad = std::numbers::pi / (180.0 * 3600.0);

        const double phi = std::clamp(geo.lat, -89.999999, 89.999999) * kDegToRad;
        const double lam = geo.lon * kDegToRad;
        const double sinPhi = std::sin(phi);
        const double cosPhi = std::cos(phi);
        const double sinLam = std::sin(lam);
        const double cosLam = std::cos(lam);

        const double e1Sq = 2.0 * srcEllipsoid.f - srcEllipsoid.f * srcEllipsoid.f;
        const double n1 = srcEllipsoid.a / std::sqrt(1.0 - e1Sq * sinPhi * sinPhi);
        const double x1 = n1 * cosPhi * cosLam;
        const double y1 = n1 * cosPhi * sinLam;
        const double z1 = n1 * (1.0 - e1Sq) * sinPhi;

        const double s = (inverse ? -1.0 : 1.0);
        const double dx = s * toWgs84.dx;
        const double dy = s * toWgs84.dy;
        const double dz = s * toWgs84.dz;
        const double wx = s * toWgs84.wxArcSec * kArcSecToRad;
        const double wy = s * toWgs84.wyArcSec * kArcSecToRad;
        const double wz = s * toWgs84.wzArcSec * kArcSecToRad;
        const double m  = 1.0 + s * toWgs84.scalePpm * 1e-6;

        // GOST 32453-2017 Eq. (13) coordinate frame rotation + translation
        const double x2 = m * ( x1 + wz * y1 - wy * z1) + dx;
        const double y2 = m * (-wz * x1 + y1 + wx * z1) + dy;
        const double z2 = m * ( wy * x1 - wx * y1 + z1) + dz;

        const double e2Sq = 2.0 * dstEllipsoid.f - dstEllipsoid.f * dstEllipsoid.f;
        const double b2 = dstEllipsoid.a * (1.0 - dstEllipsoid.f);
        const double ep2Sq = (dstEllipsoid.a * dstEllipsoid.a - b2 * b2) / (b2 * b2);
        const double p = std::hypot(x2, y2);
        if (p < 1e-9) {
            return {z2 >= 0.0 ? 90.0 : -90.0, geo.lon};
        }

        double theta = std::atan2(z2 * dstEllipsoid.a, p * b2);
        double phi2 = 0.0;
        for (int i = 0; i < 2; ++i) {
            const double sinT = std::sin(theta);
            const double cosT = std::cos(theta);
            phi2 = std::atan2(
                z2 + ep2Sq * b2 * sinT * sinT * sinT,
                p - e2Sq * dstEllipsoid.a * cosT * cosT * cosT
            );
            theta = std::atan((1.0 - dstEllipsoid.f) * std::tan(phi2));
        }
        const double lam2 = std::atan2(y2, x2);

        return {
            std::clamp(phi2 * kRadToDeg, -89.999999, 89.999999),
            ClampLon(lam2 * kRadToDeg)
        };
    }

    /// Forward Krüger 4th-order (O(e^8)) Conformal-Latitude Transverse Mercator:
    /// Converts WGS84 (lat, lon) into metric (Easting, Northing) according to `spec`.
    [[nodiscard]] static std::pair<double, double> TransverseMercatorForward(
        GeoCoord wgs84,
        const TransverseMercatorSpec& spec
    ) noexcept {
        constexpr double kDegToRad = std::numbers::pi / 180.0;
        const GeoCoord localGeo = TransformDatumHelmert(
            wgs84, EllipsoidSpec::Wgs84(), spec.ellipsoid, spec.datumToWgs84, /*inverse=*/true
        );

        const double f = spec.ellipsoid.f;
        const double n = f / (2.0 - f);
        const double n2 = n * n;
        const double n3 = n2 * n;
        const double n4 = n2 * n2;
        const double e = std::sqrt(2.0 * f - f * f);
        const double A = (spec.ellipsoid.a / (1.0 + n)) * (1.0 + 0.25 * n2 + (1.0 / 64.0) * n4);

        const double alpha1 = (1.0 / 2.0) * n - (2.0 / 3.0) * n2 + (5.0 / 16.0) * n3 + (41.0 / 180.0) * n4;
        const double alpha2 = (13.0 / 48.0) * n2 - (3.0 / 5.0) * n3 + (557.0 / 1440.0) * n4;
        const double alpha3 = (61.0 / 240.0) * n3 - (103.0 / 140.0) * n4;
        const double alpha4 = (49561.0 / 161280.0) * n4;

        const double phi = std::clamp(localGeo.lat, -89.9999, 89.9999) * kDegToRad;
        double dLonDeg = localGeo.lon - spec.centralMeridianDeg;
        while (dLonDeg > 180.0) dLonDeg -= 360.0;
        while (dLonDeg < -180.0) dLonDeg += 360.0;
        const double dLam = std::clamp(dLonDeg, -75.0, 75.0) * kDegToRad;

        const double sinPhi = std::sin(phi);
        const double Q = std::asinh(std::tan(phi)) - e * std::atanh(e * sinPhi);
        const double tanChi = std::sinh(Q);
        const double cosChi = 1.0 / std::hypot(1.0, tanChi);

        const double xiP = std::atan2(tanChi, std::cos(dLam));
        const double etaP = std::atanh(std::clamp(cosChi * std::sin(dLam), -0.9999999, 0.9999999));

        const double xi = xiP
            + alpha1 * std::sin(2.0 * xiP) * std::cosh(2.0 * etaP)
            + alpha2 * std::sin(4.0 * xiP) * std::cosh(4.0 * etaP)
            + alpha3 * std::sin(6.0 * xiP) * std::cosh(6.0 * etaP)
            + alpha4 * std::sin(8.0 * xiP) * std::cosh(8.0 * etaP);

        const double eta = etaP
            + alpha1 * std::cos(2.0 * xiP) * std::sinh(2.0 * etaP)
            + alpha2 * std::cos(4.0 * xiP) * std::sinh(4.0 * etaP)
            + alpha3 * std::cos(6.0 * xiP) * std::sinh(6.0 * etaP)
            + alpha4 * std::cos(8.0 * xiP) * std::sinh(8.0 * etaP);

        const double easting  = spec.falseEasting  + spec.scaleFactor * A * eta;
        const double northing = spec.falseNorthing + spec.scaleFactor * A * xi;
        return {easting, northing};
    }

    /// Inverse Krüger 4th-order (O(e^8)) Conformal-Latitude Transverse Mercator:
    /// Converts metric (Easting, Northing) back into WGS84 (lat, lon) degrees.
    [[nodiscard]] static GeoCoord TransverseMercatorInverse(
        double easting,
        double northing,
        const TransverseMercatorSpec& spec
    ) noexcept {
        constexpr double kRadToDeg = 180.0 / std::numbers::pi;
        const double f = spec.ellipsoid.f;
        const double n = f / (2.0 - f);
        const double n2 = n * n;
        const double n3 = n2 * n;
        const double n4 = n2 * n2;
        const double eSq = 2.0 * f - f * f;
        const double e = std::sqrt(eSq);
        const double A = (spec.ellipsoid.a / (1.0 + n)) * (1.0 + 0.25 * n2 + (1.0 / 64.0) * n4);

        const double beta1 = (1.0 / 2.0) * n - (2.0 / 3.0) * n2 + (37.0 / 96.0) * n3 - (1.0 / 360.0) * n4;
        const double beta2 = (1.0 / 48.0) * n2 + (1.0 / 15.0) * n3 - (437.0 / 1440.0) * n4;
        const double beta3 = (17.0 / 480.0) * n3 - (37.0 / 840.0) * n4;
        const double beta4 = (4397.0 / 161280.0) * n4;

        const double invScaleA = 1.0 / (spec.scaleFactor * A);
        const double xi  = std::clamp((northing - spec.falseNorthing) * invScaleA, -1.56, 1.56);
        const double eta = std::clamp((easting  - spec.falseEasting)  * invScaleA, -2.20, 2.20);

        const double xiP = xi
            - beta1 * std::sin(2.0 * xi) * std::cosh(2.0 * eta)
            - beta2 * std::sin(4.0 * xi) * std::cosh(4.0 * eta)
            - beta3 * std::sin(6.0 * xi) * std::cosh(6.0 * eta)
            - beta4 * std::sin(8.0 * xi) * std::cosh(8.0 * eta);

        const double etaP = eta
            - beta1 * std::cos(2.0 * xi) * std::sinh(2.0 * eta)
            - beta2 * std::cos(4.0 * xi) * std::sinh(4.0 * eta)
            - beta3 * std::cos(6.0 * xi) * std::sinh(6.0 * eta)
            - beta4 * std::cos(8.0 * xi) * std::sinh(8.0 * eta);

        const double sinhEtaP = std::sinh(etaP);
        const double coshEtaP = std::cosh(etaP);
        const double sinChi = std::clamp(std::sin(xiP) / coshEtaP, -0.999999999, 0.999999999);
        const double dLam = std::atan2(sinhEtaP, std::cos(xiP));

        // Karney's quadratic Newton iteration for geodetic tangent tau = tan(phi)
        const double Q = std::atanh(sinChi);
        const double sinhQ = std::sinh(Q);
        double tau = sinhQ;
        for (int iter = 0; iter < 5; ++iter) {
            const double tauHyp = std::hypot(1.0, tau);
            const double sigma = std::sinh(e * std::atanh(e * tau / tauHyp));
            const double tauP = tau * std::hypot(1.0, sigma) - sigma * tauHyp;
            const double dTau = ((sinhQ - tauP) / std::hypot(1.0, tauP)) *
                                ((1.0 + (1.0 - eSq) * tau * tau) / ((1.0 - eSq) * tauHyp));
            tau += dTau;
            if (std::abs(dTau) < 1e-13) break;
        }

        double lonDeg = spec.centralMeridianDeg + dLam * kRadToDeg;
        while (lonDeg > 180.0) lonDeg -= 360.0;
        while (lonDeg < -180.0) lonDeg += 360.0;

        const GeoCoord localGeo{
            std::clamp(std::atan(tau) * kRadToDeg, -89.999999, 89.999999),
            ClampLon(lonDeg)
        };
        return TransformDatumHelmert(
            localGeo, spec.ellipsoid, EllipsoidSpec::Wgs84(), spec.datumToWgs84, /*inverse=*/false
        );
    }

    /// Forward WGS84 Polar Stereographic (Variant B, standard parallel +-71°):
    /// `northPole == true` -> EPSG:3995 (Arctic, 71°N, CM 0°E); `false` -> EPSG:3031 (Antarctic, 71°S, CM 0°E).
    [[nodiscard]] static std::pair<double, double> PolarStereographicForward(
        GeoCoord wgs84,
        bool northPole = true
    ) noexcept {
        constexpr double kDegToRad = std::numbers::pi / 180.0;
        constexpr double a = kEarthRadiusMeters;
        constexpr double e = kWgs84Eccentricity;
        constexpr double eSq = e * e;
        constexpr double phiC = 71.0 * kDegToRad;

        const double sinPhiC = std::sin(phiC);
        const double mC = std::cos(phiC) / std::sqrt(1.0 - eSq * sinPhiC * sinPhiC);
        const double tC = std::tan(0.25 * std::numbers::pi - 0.5 * phiC) /
                          std::pow((1.0 - e * sinPhiC) / (1.0 + e * sinPhiC), 0.5 * e);

        const double sign = northPole ? 1.0 : -1.0;
        const double phi = std::clamp(sign * wgs84.lat, -30.0, 89.999999) * kDegToRad;
        const double lam = sign * wgs84.lon * kDegToRad;

        const double sinPhi = std::sin(phi);
        const double t = std::tan(0.25 * std::numbers::pi - 0.5 * phi) /
                         std::pow((1.0 - e * sinPhi) / (1.0 + e * sinPhi), 0.5 * e);
        const double rho = (a * mC * t) / tC;

        const double x = sign * rho * std::sin(lam);
        const double y = -sign * rho * std::cos(lam);
        return {x, y};
    }

    /// Inverse WGS84 Polar Stereographic (Variant B, EPSG:3995 / EPSG:3031):
    /// Converts metric (x, y) back to WGS84 (lat, lon) degrees.
    [[nodiscard]] static GeoCoord PolarStereographicInverse(
        double x,
        double y,
        bool northPole = true
    ) noexcept {
        constexpr double kDegToRad = std::numbers::pi / 180.0;
        constexpr double kRadToDeg = 180.0 / std::numbers::pi;
        constexpr double a = kEarthRadiusMeters;
        constexpr double e = kWgs84Eccentricity;
        constexpr double eSq = e * e;
        constexpr double phiC = 71.0 * kDegToRad;

        const double sinPhiC = std::sin(phiC);
        const double mC = std::cos(phiC) / std::sqrt(1.0 - eSq * sinPhiC * sinPhiC);
        const double tC = std::tan(0.25 * std::numbers::pi - 0.5 * phiC) /
                          std::pow((1.0 - e * sinPhiC) / (1.0 + e * sinPhiC), 0.5 * e);

        const double sign = northPole ? 1.0 : -1.0;
        const double sx = sign * x;
        const double sy = -sign * y;
        const double rho = std::hypot(sx, sy);
        if (rho < 1e-9) {
            return {sign * 90.0, 0.0};
        }

        const double t = (rho * tC) / (a * mC);
        double phi = 0.5 * std::numbers::pi - 2.0 * std::atan(t);
        for (int iter = 0; iter < 6; ++iter) {
            const double eSin = e * std::sin(phi);
            const double cf = std::pow((1.0 - eSin) / (1.0 + eSin), 0.5 * e);
            phi = 0.5 * std::numbers::pi - 2.0 * std::atan(t * cf);
        }

        const double lat = sign * (phi * kRadToDeg);
        const double lon = sign * (std::atan2(sx, sy) * kRadToDeg);
        return {std::clamp(lat, -89.999999, 89.999999), ClampLon(lon)};
    }

    /// Returns the TransverseMercatorSpec for a given MapCrs preset.
    [[nodiscard]] static constexpr TransverseMercatorSpec GetTransverseSpecForMapCrs(MapCrs crs) noexcept {
        if (crs == MapCrs::EPSG20906_Gsk2011Zone6) {
            return {
                .ellipsoid = EllipsoidSpec::Gsk2011(),
                .datumToWgs84 = Helmert7Spec::Identity(),
                .centralMeridianDeg = 33.0,
                .scaleFactor = 1.0,
                .falseEasting = 6500000.0,
                .falseNorthing = 0.0
            };
        }
        if (crs == MapCrs::EPSG28406_Pulkovo42Zone6) {
            return {
                .ellipsoid = EllipsoidSpec::Krassowsky1940(),
                .datumToWgs84 = Helmert7Spec::Sk42ToWgs84(),
                .centralMeridianDeg = 33.0,
                .scaleFactor = 1.0,
                .falseEasting = 6500000.0,
                .falseNorthing = 0.0
            };
        }
        // Default: EPSG:32636 (WGS84 / UTM Zone 36N, CM = 33°E)
        return {
            .ellipsoid = EllipsoidSpec::Wgs84(),
            .datumToWgs84 = Helmert7Spec::Identity(),
            .centralMeridianDeg = 33.0,
            .scaleFactor = 0.9996,
            .falseEasting = 500000.0,
            .falseNorthing = 0.0
        };
    }

    /// Projects geographic coordinates (lat, lon) to double-precision normalized [0..1]x[0..1] map coordinates.
    [[nodiscard]] static std::pair<double, double> GeoToNormalizedDouble(
        GeoCoord geo,
        MapCrs crs = MapCrs::EPSG3857_WebMercator
    ) noexcept {
        if (crs == MapCrs::EPSG32636_Utm36N ||
            crs == MapCrs::EPSG20906_Gsk2011Zone6 ||
            crs == MapCrs::EPSG28406_Pulkovo42Zone6) {
            const TransverseMercatorSpec spec = GetTransverseSpecForMapCrs(crs);
            const auto [easting, northing] = TransverseMercatorForward(geo, spec);
            const double u = 0.5 + (easting - spec.falseEasting) / kEarthCircumferenceMeters;
            const double v = 0.5 - (northing - spec.falseNorthing) / kEarthCircumferenceMeters;
            return {std::clamp(u, 0.0, 1.0), std::clamp(v, 0.0, 1.0)};
        }
        if (crs == MapCrs::EPSG3995_ArcticPolar) {
            const auto [px, py] = PolarStereographicForward(geo, /*northPole=*/true);
            const double u = 0.5 + px / kEarthCircumferenceMeters;
            const double v = 0.5 - py / kEarthCircumferenceMeters;
            return {std::clamp(u, 0.0, 1.0), std::clamp(v, 0.0, 1.0)};
        }

        const double lon = ClampLon(geo.lon);
        const double lat = ClampLat(geo.lat, crs);
        const double x = (lon + 180.0) / 360.0;

        double y = 0.5;
        if (crs == MapCrs::EPSG4326_PlateCarree) {
            y = (90.0 - lat) / 180.0;
        } else if (crs == MapCrs::EPSG3395_WorldMercator) {
            const double phi = lat * (std::numbers::pi / 180.0);
            const double eSin = kWgs84Eccentricity * std::sin(phi);
            const double conformalFactor = std::pow((1.0 - eSin) / (1.0 + eSin), 0.5 * kWgs84Eccentricity);
            const double mercY = std::log(std::tan(0.25 * std::numbers::pi + 0.5 * phi) * conformalFactor);
            y = (1.0 - mercY / std::numbers::pi) * 0.5;
        } else {
            const double phi = lat * (std::numbers::pi / 180.0);
            const double mercY = std::log(std::tan(0.25 * std::numbers::pi + 0.5 * phi));
            y = (1.0 - mercY / std::numbers::pi) * 0.5;
        }

        return {std::clamp(x, 0.0, 1.0), std::clamp(y, 0.0, 1.0)};
    }

    /// Projects geographic coordinates (lat, lon) to normalized [0..1]x[0..1] map coordinates.
    /// Origin (0,0) is Top-Left (North-West), (1,1) is Bottom-Right (South-East).
    [[nodiscard]] static Vec2 GeoToNormalized(GeoCoord geo, MapCrs crs = MapCrs::EPSG3857_WebMercator) noexcept {
        const auto [u, v] = GeoToNormalizedDouble(geo, crs);
        return {static_cast<float>(u), static_cast<float>(v)};
    }

    /// Unprojects normalized [0..1]x[0..1] map coordinates back to geographic (lat, lon) in degrees.
    [[nodiscard]] static GeoCoord NormalizedToGeo(Vec2 norm, MapCrs crs = MapCrs::EPSG3857_WebMercator) noexcept {
        const double nx = std::clamp(static_cast<double>(norm.x), 0.0, 1.0);
        const double ny = std::clamp(static_cast<double>(norm.y), 0.0, 1.0);

        if (crs == MapCrs::EPSG32636_Utm36N ||
            crs == MapCrs::EPSG20906_Gsk2011Zone6 ||
            crs == MapCrs::EPSG28406_Pulkovo42Zone6) {
            const TransverseMercatorSpec spec = GetTransverseSpecForMapCrs(crs);
            const double easting  = spec.falseEasting  + (nx - 0.5) * kEarthCircumferenceMeters;
            const double northing = spec.falseNorthing + (0.5 - ny) * kEarthCircumferenceMeters;
            return TransverseMercatorInverse(easting, northing, spec);
        }
        if (crs == MapCrs::EPSG3995_ArcticPolar) {
            const double px = (nx - 0.5) * kEarthCircumferenceMeters;
            const double py = (0.5 - ny) * kEarthCircumferenceMeters;
            return PolarStereographicInverse(px, py, /*northPole=*/true);
        }

        const double lon = nx * 360.0 - 180.0;

        double lat = 0.0;
        if (crs == MapCrs::EPSG4326_PlateCarree) {
            lat = 90.0 - ny * 180.0;
        } else if (crs == MapCrs::EPSG3395_WorldMercator) {
            const double mercY = std::numbers::pi * (1.0 - 2.0 * ny);
            const double t = std::exp(-mercY);
            double phi = 0.5 * std::numbers::pi - 2.0 * std::atan(t);
            for (int iter = 0; iter < 6; ++iter) {
                const double eSin = kWgs84Eccentricity * std::sin(phi);
                const double cf = std::pow((1.0 - eSin) / (1.0 + eSin), 0.5 * kWgs84Eccentricity);
                phi = 0.5 * std::numbers::pi - 2.0 * std::atan(t * cf);
            }
            lat = phi * (180.0 / std::numbers::pi);
        } else {
            const double n = std::numbers::pi * (1.0 - 2.0 * ny);
            lat = std::atan(std::sinh(n)) * (180.0 / std::numbers::pi);
        }

        return {ClampLat(lat, crs), ClampLon(lon)};
    }

    /// Converts geographic coordinates (lat, lon) to Scene World Coordinates (0..worldSize).
    [[nodiscard]] static Vec2 GeoToWorld(GeoCoord geo, float worldSize, MapCrs crs = MapCrs::EPSG3857_WebMercator) noexcept {
        const Vec2 n = GeoToNormalized(geo, crs);
        return {n.x * worldSize, n.y * worldSize};
    }

    /// Converts Scene World Coordinates (0..worldSize) to geographic coordinates (lat, lon).
    [[nodiscard]] static GeoCoord WorldToGeo(Vec2 worldPos, float worldSize, MapCrs crs = MapCrs::EPSG3857_WebMercator) noexcept {
        const float inv = (worldSize > 0.0f) ? (1.0f / worldSize) : 1.0f;
        return NormalizedToGeo({worldPos.x * inv, worldPos.y * inv}, crs);
    }

    /// Converts a geographic Coordinate into fractional slippy tile coordinates at zoom level z.
    [[nodiscard]] static Vec2 GeoToTileFraction(GeoCoord geo, int z, MapCrs crs = MapCrs::EPSG3857_WebMercator) noexcept {
        const int clampedZ = std::clamp(z, 0, 22);
        const float n = static_cast<float>(1 << clampedZ);
        const Vec2 norm = GeoToNormalized(geo, crs);
        return {norm.x * n, norm.y * n};
    }

    /// Converts a geographic coordinate into integer [z/x/y] tile indices.
    [[nodiscard]] static OsmTileId GeoToTileId(GeoCoord geo, int z, MapCrs crs = MapCrs::EPSG3857_WebMercator) noexcept {
        const int clampedZ = std::clamp(z, 0, 22);
        const int n = 1 << clampedZ;
        const Vec2 f = GeoToTileFraction(geo, clampedZ, crs);
        const int tx = std::clamp(static_cast<int>(std::floor(f.x)), 0, n - 1);
        const int ty = std::clamp(static_cast<int>(std::floor(f.y)), 0, n - 1);
        return {clampedZ, tx, ty};
    }

    /// Returns the Scene world-space rectangle occupied by tile [z/x/y].
    [[nodiscard]] static Rect TileToWorldRect(OsmTileId tile, float worldSize) noexcept {
        const int clampedZ = std::clamp(tile.z, 0, 22);
        const float n = static_cast<float>(1 << clampedZ);
        const float tileWorldW = worldSize / n;
        return Rect(
            static_cast<float>(tile.x) * tileWorldW,
            static_cast<float>(tile.y) * tileWorldW,
            tileWorldW,
            tileWorldW
        );
    }

    /// Returns the geographic NW and SE bounds of tile [z/x/y] (evaluating all 4 corners + center for rotated/polar CRS).
    [[nodiscard]] static GeoBounds TileToGeoBounds(OsmTileId tile, MapCrs crs = MapCrs::EPSG3857_WebMercator) noexcept {
        const int clampedZ = std::clamp(tile.z, 0, 22);
        const float invN = 1.0f / static_cast<float>(1 << clampedZ);
        const float x0 = static_cast<float>(tile.x) * invN;
        const float y0 = static_cast<float>(tile.y) * invN;
        const float x1 = static_cast<float>(tile.x + 1) * invN;
        const float y1 = static_cast<float>(tile.y + 1) * invN;

        const GeoCoord c00 = NormalizedToGeo({x0, y0}, crs);
        const GeoCoord c11 = NormalizedToGeo({x1, y1}, crs);
        if (IsCylindricalCrs(crs)) {
            return {c00, c11};
        }
        const GeoCoord c10 = NormalizedToGeo({x1, y0}, crs);
        const GeoCoord c01 = NormalizedToGeo({x0, y1}, crs);
        const GeoCoord cMid = NormalizedToGeo({0.5f * (x0 + x1), 0.5f * (y0 + y1)}, crs);
        const double maxLat = std::max({c00.lat, c10.lat, c01.lat, c11.lat, cMid.lat});
        const double minLat = std::min({c00.lat, c10.lat, c01.lat, c11.lat, cMid.lat});
        const double minLon = std::min({c00.lon, c10.lon, c01.lon, c11.lon, cMid.lon});
        const double maxLon = std::max({c00.lon, c10.lon, c01.lon, c11.lon, cMid.lon});
        return {{maxLat, minLon}, {minLat, maxLon}};
    }

    /// Computes the Scene zoom factor required to achieve a target continuous tile zoom level.
    [[nodiscard]] static float TileZoomToSceneZoom(float targetTileZoom, int baseZoom = 3) noexcept {
        return std::pow(2.0f, targetTileZoom - static_cast<float>(baseZoom));
    }

    /// Computes ground resolution in meters per screen pixel at a given latitude and continuous tile zoom.
    /// At standard 256px tile size, zoom 0 has 256 pixels across 40,075,016.686 meters at the equator (or planar extent).
    [[nodiscard]] static double MetersPerPixel(
        double latDeg,
        float continuousZoom,
        float screenTileSize = 256.0f,
        MapCrs crs = MapCrs::EPSG3857_WebMercator
    ) noexcept {
        const double worldPixels = static_cast<double>(screenTileSize) * std::pow(2.0, static_cast<double>(continuousZoom));
        if (!IsCylindricalCrs(crs)) {
            return std::max(0.0001, kEarthCircumferenceMeters / worldPixels);
        }
        const double cosLat = std::cos(ClampLat(latDeg) * (std::numbers::pi / 180.0));
        return std::max(0.0001, (kEarthCircumferenceMeters * cosLat) / worldPixels);
    }

    /// Selects an optimal nautical graticule spacing in arc-minutes (1' = 1 Nautical Mile)
    /// so that grid lines are spaced roughly `targetPixelSpacing` pixels apart on screen.
    [[nodiscard]] static double SelectNauticalGraticuleStepArcMin(
        double visibleLonSpanDeg,
        float viewportWidthPx,
        float targetPixelSpacing = 110.0f
    ) noexcept {
        static constexpr std::array<double, 16> kStepsArcMin = {
            1800.0, // 30°
            1200.0, // 20°
            600.0,  // 10°
            300.0,  // 5°
            120.0,  // 2°
            60.0,   // 1° (60 NM)
            30.0,   // 30' (30 NM)
            15.0,   // 15' (15 NM)
            10.0,   // 10' (10 NM)
            5.0,    // 5' (5 NM)
            2.0,    // 2' (2 NM)
            1.0,    // 1' (1 Nautical Mile)
            0.5,    // 0.5' (5 cables)
            0.2,    // 0.2' (2 cables)
            0.1,    // 0.1' (1 cable = 185.2 m)
            0.05    // 0.05'
        };
        if (viewportWidthPx <= 1.0f || visibleLonSpanDeg <= 0.0) {
            return 60.0;
        }
        const double desiredDeg = visibleLonSpanDeg * (static_cast<double>(targetPixelSpacing) / static_cast<double>(viewportWidthPx));
        const double desiredArcMin = desiredDeg * 60.0;

        double best = kStepsArcMin.front();
        double bestDiff = std::abs(best - desiredArcMin);
        for (double step : kStepsArcMin) {
            const double diff = std::abs(step - desiredArcMin);
            if (diff < bestDiff) {
                bestDiff = diff;
                best = step;
            }
        }
        return best;
    }

    /// Computes a clean 1-2-5 physical scale bar in meters/kilometers.
    [[nodiscard]] static MapScaleBar ComputeMetricScaleBar(double metersPerPixel, float maxBarPixels = 120.0f) noexcept {
        MapScaleBar bar{};
        if (metersPerPixel <= 0.0 || maxBarPixels <= 10.0f) return bar;

        const double maxMeters = metersPerPixel * static_cast<double>(maxBarPixels);
        const double exp10 = std::pow(10.0, std::floor(std::log10(std::max(1.0, maxMeters))));
        double niceMeters = exp10;
        if (5.0 * exp10 <= maxMeters) {
            niceMeters = 5.0 * exp10;
        } else if (2.0 * exp10 <= maxMeters) {
            niceMeters = 2.0 * exp10;
        }

        bar.distanceValue = niceMeters;
        bar.screenPixels = static_cast<float>(niceMeters / metersPerPixel);
        if (niceMeters >= 1000.0) {
            std::snprintf(bar.label, sizeof(bar.label), "%.0f km", niceMeters / 1000.0);
        } else {
            std::snprintf(bar.label, sizeof(bar.label), "%.0f m", niceMeters);
        }
        return bar;
    }

    /// Computes a clean 1-2-5 nautical scale bar in Nautical Miles (NM) or Cables (cab, 0.1 NM = 185.2 m).
    [[nodiscard]] static MapScaleBar ComputeNauticalScaleBar(double metersPerPixel, float maxBarPixels = 120.0f) noexcept {
        MapScaleBar bar{};
        if (metersPerPixel <= 0.0 || maxBarPixels <= 10.0f) return bar;

        const double nmPerPixel = metersPerPixel / kMetersPerNauticalMile;
        const double maxNm = nmPerPixel * static_cast<double>(maxBarPixels);

        if (maxNm >= 1.0) {
            const double exp10 = std::pow(10.0, std::floor(std::log10(maxNm)));
            double niceNm = exp10;
            if (5.0 * exp10 <= maxNm) {
                niceNm = 5.0 * exp10;
            } else if (2.0 * exp10 <= maxNm) {
                niceNm = 2.0 * exp10;
            }
            bar.distanceValue = niceNm;
            bar.screenPixels = static_cast<float>((niceNm * kMetersPerNauticalMile) / metersPerPixel);
            std::snprintf(bar.label, sizeof(bar.label), "%.0f NM", niceNm);
        } else {
            // Sub-mile nautical scale in cables (1 NM = 10 cables = 1852 m)
            const double maxCables = std::max(0.1, maxNm * 10.0);
            double niceCables = 0.5;
            if (maxCables >= 5.0) niceCables = 5.0;
            else if (maxCables >= 2.0) niceCables = 2.0;
            else if (maxCables >= 1.0) niceCables = 1.0;

            bar.distanceValue = niceCables * 0.1;
            bar.screenPixels = static_cast<float>((bar.distanceValue * kMetersPerNauticalMile) / metersPerPixel);
            std::snprintf(bar.label, sizeof(bar.label), "%.1f NM (%.0f cab)", bar.distanceValue, niceCables);
        }
        return bar;
    }

    /// Computes the full visible tile grid, nautical graticule, geographic bounds, and scale bars for the current Scene view.
    [[nodiscard]] static MapViewState ComputeView(
        Rect visibleWorldRect,
        Rect viewportScreenRect,
        float sceneZoom,
        Vec2 mouseWorldPos,
        const MapGridConfig& cfg = {}
    ) {
        MapViewState out{};
        out.crs = cfg.crs;

        const float safeWorld = std::max(1.0f, cfg.worldSize);
        const float safeZoom = std::max(0.0001f, sceneZoom);

        // 1. Continuous and integer tile pyramid zoom level
        out.continuousZoom = static_cast<float>(cfg.baseZoom) + std::log2(safeZoom) + cfg.zoomBias;
        out.tileZoom = std::clamp(
            static_cast<int>(std::floor(out.continuousZoom + 0.5f)),
            cfg.minTileZoom,
            cfg.maxTileZoom
        );
        out.totalTilesPerAxis = 1 << out.tileZoom;

        // 2. Clamp visible world rect to [0..worldSize]
        const float vx0 = std::clamp(visibleWorldRect.x, 0.0f, safeWorld);
        const float vy0 = std::clamp(visibleWorldRect.y, 0.0f, safeWorld);
        const float vx1 = std::clamp(visibleWorldRect.Right(), 0.0f, safeWorld);
        const float vy1 = std::clamp(visibleWorldRect.Bottom(), 0.0f, safeWorld);

        const Vec2 worldCenter{
            visibleWorldRect.x + visibleWorldRect.w * 0.5f,
            visibleWorldRect.y + visibleWorldRect.h * 0.5f
        };
        out.center = WorldToGeo(worldCenter, safeWorld, cfg.crs);
        out.cursor = WorldToGeo(mouseWorldPos, safeWorld, cfg.crs);
        if (IsCylindricalCrs(cfg.crs)) {
            out.visibleBounds.nw = WorldToGeo({vx0, vy0}, safeWorld, cfg.crs);
            out.visibleBounds.se = WorldToGeo({vx1, vy1}, safeWorld, cfg.crs);
        } else {
            const GeoCoord c00 = WorldToGeo({vx0, vy0}, safeWorld, cfg.crs);
            const GeoCoord c10 = WorldToGeo({vx1, vy0}, safeWorld, cfg.crs);
            const GeoCoord c01 = WorldToGeo({vx0, vy1}, safeWorld, cfg.crs);
            const GeoCoord c11 = WorldToGeo({vx1, vy1}, safeWorld, cfg.crs);
            out.visibleBounds.nw.lat = std::max({c00.lat, c10.lat, c01.lat, c11.lat, out.center.lat});
            out.visibleBounds.se.lat = std::min({c00.lat, c10.lat, c01.lat, c11.lat, out.center.lat});
            out.visibleBounds.nw.lon = std::min({c00.lon, c10.lon, c01.lon, c11.lon, out.center.lon});
            out.visibleBounds.se.lon = std::max({c00.lon, c10.lon, c01.lon, c11.lon, out.center.lon});
        }

        // 3. Visible [z/x/y] tile range & frustum culling
        const float nF = static_cast<float>(out.totalTilesPerAxis);
        const float tileWorldW = safeWorld / nF;

        out.minTileX = std::clamp(static_cast<int>(std::floor(vx0 / tileWorldW)), 0, out.totalTilesPerAxis - 1);
        out.maxTileX = std::clamp(static_cast<int>(std::floor( std::max(vx0, vx1 - 1e-4f) / tileWorldW)), 0, out.totalTilesPerAxis - 1);
        out.minTileY = std::clamp(static_cast<int>(std::floor(vy0 / tileWorldW)), 0, out.totalTilesPerAxis - 1);
        out.maxTileY = std::clamp(static_cast<int>(std::floor( std::max(vy0, vy1 - 1e-4f) / tileWorldW)), 0, out.totalTilesPerAxis - 1);

        const int countX = std::max(0, out.maxTileX - out.minTileX + 1);
        const int countY = std::max(0, out.maxTileY - out.minTileY + 1);
        out.visibleTileCount = countX * countY;
        out.visibleTiles.reserve(static_cast<size_t>(out.visibleTileCount));

        for (int ty = out.minTileY; ty <= out.maxTileY; ++ty) {
            for (int tx = out.minTileX; tx <= out.maxTileX; ++tx) {
                const OsmTileId id{out.tileZoom, tx, ty};
                out.visibleTiles.push_back(OsmTileCell{
                    .id = id,
                    .worldRect = TileToWorldRect(id, safeWorld),
                    .geoBounds = TileToGeoBounds(id, cfg.crs)
                });
            }
        }

        // 4. Physical ground resolution & Metric / Nautical scale bars
        // Screen size of a base tile is (safeWorld / 2^baseZoom) * sceneZoom
        const float baseTileWorld = safeWorld / static_cast<float>(1 << cfg.baseZoom);
        const double effContinuousZ = static_cast<double>(cfg.baseZoom) + std::log2(static_cast<double>(safeZoom));
        out.metersPerPixel = MetersPerPixel(out.center.lat, static_cast<float>(effContinuousZ), baseTileWorld, cfg.crs);
        out.metricScale = ComputeMetricScaleBar(out.metersPerPixel, 120.0f);
        out.nauticalScale = ComputeNauticalScaleBar(out.metersPerPixel, 120.0f);

        // 5. OpenSeaMap Nautical Graticule (meridians & parallels in degrees and nautical arc-minutes)
        // Use un-clamped visibleWorldRect.w so panning near map boundaries never alters the graticule step!
        const double unClampedLonSpan = IsCylindricalCrs(cfg.crs)
            ? ((visibleWorldRect.w > 0.0f)
                ? (static_cast<double>(visibleWorldRect.w) / static_cast<double>(safeWorld)) * 360.0
                : (360.0 / static_cast<double>(safeZoom)))
            : std::max(0.0001, out.visibleBounds.WidthLon());
        out.graticuleStepArcMin = SelectNauticalGraticuleStepArcMin(
            std::max(0.0001, unClampedLonSpan),
            std::max(120.0f, viewportScreenRect.w),
            115.0f
        );
        const double stepDeg = out.graticuleStepArcMin / 60.0;

        if (stepDeg > 0.0) {
            // Meridians (West -> East), padded by 1 step so border ruler zebra bars clip smoothly at edges
            const int iLonStart = static_cast<int>(std::floor(out.visibleBounds.nw.lon / stepDeg)) - 1;
            const int iLonEnd   = static_cast<int>(std::ceil(out.visibleBounds.se.lon / stepDeg)) + 1;
            for (int idx = iLonStart; idx <= iLonEnd; ++idx) {
                const double lon = static_cast<double>(idx) * stepDeg;
                if (lon < -180.0 - 1e-6 || lon > 180.0 + 1e-6) continue;
                const float wx = GeoToWorld({out.center.lat, lon}, safeWorld, cfg.crs).x;
                const bool isZero = (idx == 0);
                const bool isMajor = isZero || (idx % 5 == 0) || (std::abs(std::fmod(lon, 1.0)) < 1e-5 && out.graticuleStepArcMin < 60.0);
                out.meridians.push_back(GraticuleLine{
                    .degrees = lon,
                    .worldPos = wx,
                    .stepIndex = idx,
                    .isMajor = isMajor,
                    .isZero = isZero
                });
            }

            // Parallels (South -> North), padded by 1 step so border ruler zebra bars clip smoothly at edges
            const int iLatStart = static_cast<int>(std::floor(out.visibleBounds.se.lat / stepDeg)) - 1;
            const int iLatEnd   = static_cast<int>(std::ceil(out.visibleBounds.nw.lat / stepDeg)) + 1;
            for (int idx = iLatStart; idx <= iLatEnd; ++idx) {
                const double lat = static_cast<double>(idx) * stepDeg;
                const double maxL = IsCylindricalCrs(cfg.crs)
                    ? ((cfg.crs == MapCrs::EPSG4326_PlateCarree) ? 89.9 : kMaxMercatorLat)
                    : 89.9;
                if (lat < -maxL || lat > maxL) continue;
                const float wy = GeoToWorld({lat, out.center.lon}, safeWorld, cfg.crs).y;
                const bool isZero = (idx == 0);
                const bool isMajor = isZero || (idx % 5 == 0) || (std::abs(std::fmod(lat, 1.0)) < 1e-5 && out.graticuleStepArcMin < 60.0);
                out.parallels.push_back(GraticuleLine{
                    .degrees = lat,
                    .worldPos = wy,
                    .stepIndex = idx,
                    .isMajor = isMajor,
                    .isZero = isZero
                });
            }
        }

        return out;
    }

    /// Formats a single latitude or longitude value according to the selected maritime/cartographic format.
    [[nodiscard]] static std::string FormatAxis(double deg, bool isLat, GeoCoordFormat fmt = GeoCoordFormat::NauticalDMM) {
        const char hemi = isLat ? (deg >= 0.0 ? 'N' : 'S') : (deg >= 0.0 ? 'E' : 'W');
        const double absDeg = std::abs(deg);
        char buf[48]{};

        if (fmt == GeoCoordFormat::DecimalDegrees) {
            std::snprintf(buf, sizeof(buf), isLat ? "%07.4f° %c" : "%08.4f° %c", absDeg, hemi);
        } else if (fmt == GeoCoordFormat::DMS) {
            int d = static_cast<int>( std::floor(absDeg) );
            double remMin = (absDeg - static_cast<double>(d)) * 60.0;
            int m = static_cast<int>( std::floor(remMin) );
            double s = (remMin - static_cast<double>(m)) * 60.0;
            if (s >= 59.95) { s = 0.0; ++m; }
            if (m >= 60) { m = 0; ++d; }
            std::snprintf(buf, sizeof(buf), isLat ? "%02d° %02d' %04.1f\" %c" : "%03d° %02d' %04.1f\" %c", d, m, s, hemi);
        } else {
            // Standard OpenSeaMap / Marine GPS Degrees & Decimal Minutes (DD° MM.mm')
            int d = static_cast<int>( std::floor(absDeg) );
            double m = (absDeg - static_cast<double>(d)) * 60.0;
            if (m >= 59.995) { m = 0.0; ++d; }
            std::snprintf(buf, sizeof(buf), isLat ? "%02d° %05.2f' %c" : "%03d° %05.2f' %c", d, m, hemi);
        }
        return std::string(buf);
    }

    /// Formats a short graticule ruler label (e.g. "54° 15.0' N") for border ticks.
    [[nodiscard]] static std::string FormatGraticuleTick(double deg, bool isLat, double stepArcMin) {
        const char hemi = isLat ? (deg >= 0.0 ? 'N' : 'S') : (deg >= 0.0 ? 'E' : 'W');
        const double absDeg = std::abs(deg);
        char buf[32]{};
        if (stepArcMin >= 60.0) {
            std::snprintf(buf, sizeof(buf), "%.0f° %c", absDeg, hemi);
        } else if (stepArcMin >= 1.0) {
            int d = static_cast<int>(std::floor(absDeg + 1e-7));
            double m = (absDeg - static_cast<double>(d)) * 60.0;
            if (m >= 59.95) { m = 0.0; ++d; }
            std::snprintf(buf, sizeof(buf), "%d° %02.0f' %c", d, m, hemi);
        } else {
            int d = static_cast<int>(std::floor(absDeg + 1e-7));
            double m = (absDeg - static_cast<double>(d)) * 60.0;
            if (m >= 59.995) { m = 0.0; ++d; }
            std::snprintf(buf, sizeof(buf), "%d° %04.1f' %c", d, m, hemi);
        }
        return std::string(buf);
    }

    /// Formats a full (lat, lon) or planar metric (UTM / GOST Gauss-Kruger) coordinate string.
    [[nodiscard]] static std::string FormatCoord(
        GeoCoord c,
        GeoCoordFormat fmt = GeoCoordFormat::NauticalDMM,
        MapCrs activeCrs = MapCrs::EPSG3857_WebMercator
    ) {
        if (fmt == GeoCoordFormat::UTM_Meters) {
            const int zone = (activeCrs == MapCrs::EPSG32636_Utm36N)
                ? 36
                : std::clamp(static_cast<int>(std::floor((ClampLon(c.lon) + 180.0) / 6.0)) + 1, 1, 60);
            const bool north = (c.lat >= 0.0);
            const TransverseMercatorSpec spec{
                .ellipsoid = EllipsoidSpec::Wgs84(),
                .datumToWgs84 = Helmert7Spec::Identity(),
                .centralMeridianDeg = static_cast<double>(zone * 6 - 183),
                .scaleFactor = 0.9996,
                .falseEasting = 500000.0,
                .falseNorthing = north ? 0.0 : 10000000.0
            };
            const auto [easting, northing] = TransverseMercatorForward(c, spec);
            char buf[80]{};
            std::snprintf(buf, sizeof(buf), "UTM %02d%c  E %.1f m, N %.1f m",
                zone, north ? 'N' : 'S', easting, northing);
            return std::string(buf);
        }
        if (fmt == GeoCoordFormat::GaussKruger_Meters) {
            double posLon = ClampLon(c.lon);
            if (posLon < 0.0) posLon += 360.0;
            const int zone = (activeCrs == MapCrs::EPSG20906_Gsk2011Zone6 || activeCrs == MapCrs::EPSG28406_Pulkovo42Zone6)
                ? 6
                : std::clamp(static_cast<int>(std::floor(posLon / 6.0)) + 1, 1, 60);
            const bool useSk42 = (activeCrs == MapCrs::EPSG28406_Pulkovo42Zone6);
            const TransverseMercatorSpec spec{
                .ellipsoid = useSk42 ? EllipsoidSpec::Krassowsky1940() : EllipsoidSpec::Gsk2011(),
                .datumToWgs84 = useSk42 ? Helmert7Spec::Sk42ToWgs84() : Helmert7Spec::Identity(),
                .centralMeridianDeg = static_cast<double>(zone * 6 - 3),
                .scaleFactor = 1.0,
                .falseEasting = static_cast<double>(zone) * 1000000.0 + 500000.0,
                .falseNorthing = 0.0
            };
            const auto [yEast, xNorth] = TransverseMercatorForward(c, spec);
            char buf[96]{};
            std::snprintf(buf, sizeof(buf), "%s Z%d  X %.1f m, Y %.1f m",
                useSk42 ? "SK-42" : "GSK-2011", zone, xNorth, yEast);
            return std::string(buf);
        }
        return FormatAxis(c.lat, true, fmt) + ", " + FormatAxis(c.lon, false, fmt);
    }
};

} // namespace guikit
