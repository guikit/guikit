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
    EPSG4326_PlateCarree        ///< Equirectangular WGS84 Geographic Lon/Lat grid
};

/// Display formatting mode for geographic coordinates.
enum class GeoCoordFormat : uint8_t {
    NauticalDMM = 0,            ///< Degrees & Decimal Minutes: 54° 10.92' N, 007° 53.20' E (1' lat = 1 NM)
    DecimalDegrees,             ///< Signed/cardinal decimal degrees: 54.1820° N, 007.8867° E
    DMS                         ///< Degrees, Minutes, Seconds: 54° 10' 55" N, 007° 53' 12" E
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

/// Pure mathematical engine for Web Mercator / WGS84 projections, OSM [z/x/y] tile grids, and OpenSeaMap nautical graticules.
class MapGrid {
public:
    static constexpr double kEarthRadiusMeters = 6378137.0;
    static constexpr double kEarthCircumferenceMeters = 2.0 * std::numbers::pi * kEarthRadiusMeters; // 40075016.6856 m
    static constexpr double kWgs84Eccentricity = 0.08181919084262149; // WGS84 ellipsoid eccentricity e
    static constexpr double kMetersPerNauticalMile = 1852.0;          // Exact international nautical mile (1' latitude)
    static constexpr double kMaxMercatorLat = 85.05112877980659;      // atan(sinh(pi)) * 180 / pi

    /// Clamps latitude to the valid domain of the specified CRS.
    [[nodiscard]] static constexpr double ClampLat(double lat, MapCrs crs = MapCrs::EPSG3857_WebMercator) noexcept {
        const double limit = (crs == MapCrs::EPSG4326_PlateCarree) ? 89.9999 : kMaxMercatorLat;
        return std::clamp(lat, -limit, limit);
    }

    /// Wraps or clamps longitude to [-180..+180].
    [[nodiscard]] static constexpr double ClampLon(double lon) noexcept {
        return std::clamp(lon, -180.0, 180.0);
    }

    /// Projects geographic coordinates (lat, lon) to normalized [0..1]x[0..1] map coordinates.
    /// Origin (0,0) is Top-Left (North-West: +maxLat, -180°), (1,1) is Bottom-Right (South-East: -maxLat, +180°).
    [[nodiscard]] static Vec2 GeoToNormalized(GeoCoord geo, MapCrs crs = MapCrs::EPSG3857_WebMercator) noexcept {
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

        return {
            static_cast<float>(std::clamp(x, 0.0, 1.0)),
            static_cast<float>(std::clamp(y, 0.0, 1.0))
        };
    }

    /// Unprojects normalized [0..1]x[0..1] map coordinates back to geographic (lat, lon) in degrees.
    [[nodiscard]] static GeoCoord NormalizedToGeo(Vec2 norm, MapCrs crs = MapCrs::EPSG3857_WebMercator) noexcept {
        const double nx = std::clamp(static_cast<double>(norm.x), 0.0, 1.0);
        const double ny = std::clamp(static_cast<double>(norm.y), 0.0, 1.0);
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

    /// Returns the geographic NW and SE bounds of tile [z/x/y].
    [[nodiscard]] static GeoBounds TileToGeoBounds(OsmTileId tile, MapCrs crs = MapCrs::EPSG3857_WebMercator) noexcept {
        const int clampedZ = std::clamp(tile.z, 0, 22);
        const float invN = 1.0f / static_cast<float>(1 << clampedZ);
        const GeoCoord nw = NormalizedToGeo({static_cast<float>(tile.x) * invN, static_cast<float>(tile.y) * invN}, crs);
        const GeoCoord se = NormalizedToGeo({static_cast<float>(tile.x + 1) * invN, static_cast<float>(tile.y + 1) * invN}, crs);
        return {nw, se};
    }

    /// Computes the Scene zoom factor required to achieve a target continuous tile zoom level.
    [[nodiscard]] static float TileZoomToSceneZoom(float targetTileZoom, int baseZoom = 3) noexcept {
        return std::pow(2.0f, targetTileZoom - static_cast<float>(baseZoom));
    }

    /// Computes ground resolution in meters per screen pixel at a given latitude and continuous tile zoom.
    /// At standard 256px tile size, zoom 0 has 256 pixels across 40,075,016.686 meters at the equator.
    [[nodiscard]] static double MetersPerPixel(double latDeg, float continuousZoom, float screenTileSize = 256.0f) noexcept {
        const double cosLat = std::cos(ClampLat(latDeg) * (std::numbers::pi / 180.0));
        const double worldPixels = static_cast<double>(screenTileSize) * std::pow(2.0, static_cast<double>(continuousZoom));
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
        out.visibleBounds.nw = WorldToGeo({vx0, vy0}, safeWorld, cfg.crs);
        out.visibleBounds.se = WorldToGeo({vx1, vy1}, safeWorld, cfg.crs);

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
        out.metersPerPixel = MetersPerPixel(out.center.lat, static_cast<float>(effContinuousZ), baseTileWorld);
        out.metricScale = ComputeMetricScaleBar(out.metersPerPixel, 120.0f);
        out.nauticalScale = ComputeNauticalScaleBar(out.metersPerPixel, 120.0f);

        // 5. OpenSeaMap Nautical Graticule (meridians & parallels in degrees and nautical arc-minutes)
        // Use un-clamped visibleWorldRect.w so panning near map boundaries never alters the graticule step!
        const double unClampedLonSpan = (visibleWorldRect.w > 0.0f)
            ? (static_cast<double>(visibleWorldRect.w) / static_cast<double>(safeWorld)) * 360.0
            : (360.0 / static_cast<double>(safeZoom));
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
                const float wx = GeoToWorld({0.0, lon}, safeWorld, cfg.crs).x;
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
                const double maxL = (cfg.crs == MapCrs::EPSG4326_PlateCarree) ? 89.9 : kMaxMercatorLat;
                if (lat < -maxL || lat > maxL) continue;
                const float wy = GeoToWorld({lat, 0.0}, safeWorld, cfg.crs).y;
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

    /// Formats a full (lat, lon) pair string.
    [[nodiscard]] static std::string FormatCoord(GeoCoord c, GeoCoordFormat fmt = GeoCoordFormat::NauticalDMM) {
        return FormatAxis(c.lat, true, fmt) + ", " + FormatAxis(c.lon, false, fmt);
    }
};

} // namespace guikit
