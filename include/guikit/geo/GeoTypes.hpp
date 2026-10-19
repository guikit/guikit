#pragma once

#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>
#include "guikit/core/Types.hpp"
#include "guikit/geo/MapGrid.hpp"
#include "guikit/geo/ChartDataSource.hpp"

namespace guikit::geo {

/// Standard OGC / RFC 7946 vector geometry classification.
enum class GeometryType : uint8_t {
    Point = 0,
    MultiPoint,
    LineString,
    MultiLineString,
    Polygon,
    MultiPolygon,
    GeometryCollection
};

/// Returns human-readable OGC geometry type name.
[[nodiscard]] constexpr std::string_view GeometryTypeToString(GeometryType type) noexcept {
    switch (type) {
        case GeometryType::Point:              return "Point";
        case GeometryType::MultiPoint:         return "MultiPoint";
        case GeometryType::LineString:         return "LineString";
        case GeometryType::MultiLineString:    return "MultiLineString";
        case GeometryType::Polygon:            return "Polygon";
        case GeometryType::MultiPolygon:       return "MultiPolygon";
        case GeometryType::GeometryCollection: return "GeometryCollection";
    }
    return "Unknown";
}

/// Returns true if the geometry type is primarily polygonal.
[[nodiscard]] constexpr bool IsPolygonalType(GeometryType type) noexcept {
    return type == GeometryType::Polygon || type == GeometryType::MultiPolygon;
}

/// Returns true if the geometry type is primarily linear.
[[nodiscard]] constexpr bool IsLinearType(GeometryType type) noexcept {
    return type == GeometryType::LineString || type == GeometryType::MultiLineString;
}

/// Returns true if the geometry type is primarily point-based.
[[nodiscard]] constexpr bool IsPointType(GeometryType type) noexcept {
    return type == GeometryType::Point || type == GeometryType::MultiPoint;
}

/// Semantic IHO S-57 marine chart object classification (determines Z-order, SCAMIN & symbology).
enum class ChartFeatureClass : uint8_t {
    CoverageMask = 0,       ///< M_COVR (coverage boundary only, no opaque fill)
    DepthArea,              ///< DEPARE, DRGARE (bathymetric polygons shaded by safety depth)
    SeabedOrMarineArea,     ///< SEAARE, SBDARE, HRBARE, PRCARE, ACHARE, TSEZNE, RESARE, CTNARE
    FairwayArea,            ///< FAIRWY, DWRTPT (navigable channels & fairways)
    LandArea,               ///< LNDARE, LNDRGN, SLOGRD (continental & island land masses)
    BuiltUpOrStructure,     ///< BUAARE, BUISGL, BRIDGE, BERTHS, SLCONS, PONTON, HRBFAC, ROADWY, RIVERS
    Generic,                ///< General user GeoJSON / GPKG vector feature
    DepthContour,           ///< DEPCNT (isobath contour lines)
    Coastline,              ///< COALNE (shoreline boundary)
    SubmarineCablePipe,     ///< CBLSUB, PIPSOL (submarine cables & pipelines)
    NavigationLine,         ///< NAVLNE, RECTRC, RESLINE, STSLIN (leading lines, recommended tracks)
    Sounding,               ///< SOUNDG (spot depth soundings in meters)
    UnderwaterHazard,       ///< UWTROC, OBSTRN, WRECKS, DMPGRD (rocks, wrecks, obstructions)
    BuoyOrBeacon,           ///< BOYLAT, BOYCAR, BOYSPP, BCNCAR, BCNSPP, LIGHTS, LNDMRK, DISMAR
    TextLabel               ///< TEXT, SBDARE_PT (cartographic place names & seabed nature symbols)
};

/// Returns human-readable marine chart class label.
[[nodiscard]] constexpr std::string_view ChartFeatureClassToString(ChartFeatureClass cls) noexcept {
    switch (cls) {
        case ChartFeatureClass::CoverageMask:       return "Coverage Mask (M_COVR)";
        case ChartFeatureClass::DepthArea:          return "Depth Area (DEPARE)";
        case ChartFeatureClass::SeabedOrMarineArea: return "Marine / Restricted Area";
        case ChartFeatureClass::FairwayArea:        return "Fairway / Channel (FAIRWY)";
        case ChartFeatureClass::LandArea:           return "Land Area (LNDARE)";
        case ChartFeatureClass::BuiltUpOrStructure: return "Port Structure / Berth / Bridge";
        case ChartFeatureClass::Generic:            return "Vector GIS Feature";
        case ChartFeatureClass::DepthContour:       return "Depth Contour (DEPCNT)";
        case ChartFeatureClass::Coastline:          return "Coastline (COALNE)";
        case ChartFeatureClass::SubmarineCablePipe: return "Submarine Cable / Pipeline";
        case ChartFeatureClass::NavigationLine:     return "Navigation Track / Leading Line";
        case ChartFeatureClass::Sounding:           return "Spot Sounding (SOUNDG)";
        case ChartFeatureClass::UnderwaterHazard:   return "Underwater Rock / Hazard (UWTROC)";
        case ChartFeatureClass::BuoyOrBeacon:       return "IALA Buoy / Beacon / Light";
        case ChartFeatureClass::TextLabel:          return "Cartographic Label (TEXT)";
    }
    return "Vector Feature";
}

/// Single geographic triangle produced by ear-clipping triangulation of a polygon ring + holes.
struct GeoTriangle {
    guikit::GeoCoord a{};
    guikit::GeoCoord b{};
    guikit::GeoCoord c{};
};

/// Polygon with an exterior ring, optional interior hole rings, pre-projected Mercator rings, and LODs.
struct GeoPolygon {
    std::vector<guikit::GeoCoord> exteriorRing{};
    std::vector<std::vector<guikit::GeoCoord>> interiorRings{};
    std::vector<GeoTriangle> triangles{};

    // Pre-projected normalized Web Mercator [0..1]x[0..1] coordinates & Douglas-Peucker LOD
    std::vector<guikit::Vec2> normExterior{};
    std::vector<guikit::Vec2> normExteriorLod{};
    std::vector<std::vector<guikit::Vec2>> normInterior{};
    guikit::Rect normBounds{};

    guikit::GeoBounds bounds{};
    bool hasValidBounds{false};

    [[nodiscard]] size_t VertexCount() const noexcept {
        size_t total = exteriorRing.size();
        for (const auto& hole : interiorRings) {
            total += hole.size();
        }
        return total;
    }
};

/// Thread-safe, immutable decoded geometry payload (`std::shared_ptr<const DecodedGeometryPayload>`).
/// Shared across background tile rasterizer threads and active window snapshots without deep-copying or data races.
struct DecodedGeometryPayload {
    int64_t featureId{0};
    GeometryType type{GeometryType::MultiPolygon};
    std::vector<guikit::GeoCoord> points{};
    std::vector<std::vector<guikit::GeoCoord>> lines{};
    std::vector<GeoPolygon> polygons{};
    std::vector<guikit::Vec2> normPoints{};
    std::vector<std::vector<guikit::Vec2>> normLines{};
    std::vector<std::vector<guikit::Vec2>> normLinesLod{};
    guikit::GeoBounds bounds{};
    guikit::Rect normBounds{};
    guikit::GeoCoord centroid{};
    guikit::Vec2 normCentroid{};
    uint32_t cachedVertexCount{0};
    uint32_t cachedPolygonCount{0};
    uint32_t cachedHoleCount{0};
    uint32_t residentHeapBytes{0};
};

/// Strongly-typed feature attribute value (GeoJSON property or GeoPackage SQLite column).
struct AttributeValue {
    using Variant = std::variant<std::monostate, bool, int64_t, double, std::string>;
    Variant value{std::monostate{}};

    AttributeValue() = default;
    AttributeValue(std::nullptr_t) : value(std::monostate{}) {}
    AttributeValue(bool b) : value(b) {}
    AttributeValue(int64_t i) : value(i) {}
    AttributeValue(int i) : value(static_cast<int64_t>(i)) {}
    AttributeValue(double d) : value(d) {}
    AttributeValue(std::string s) : value(std::move(s)) {}
    AttributeValue(std::string_view sv) : value(std::string(sv)) {}
    AttributeValue(const char* s) : value(std::string(s ? s : "")) {}

    [[nodiscard]] bool IsNull() const noexcept {
        return std::holds_alternative<std::monostate>(value);
    }

    [[nodiscard]] std::string ToString() const;
    [[nodiscard]] std::string_view TypeLabel() const noexcept;
    [[nodiscard]] bool MatchesSearch(std::string_view queryLower) const;
};

struct GeoFeature;
struct GeoLayer;
struct SpatialWindowRequest;
struct SpatialWindowSnapshot;

/// Abstract Out-of-Core Feature Pager / Vector Data Source (local SQLite `.gpkg` or remote HTTP/REST/MVT server).
class IFeaturePager {
public:
    virtual ~IFeaturePager() = default;

    /// Fetches and parses the geometry payload (polygons/lines/Mercator LODs) for `sqliteRowId` into `ioFeature`.
    virtual bool LoadFeatureGeometry(int64_t sqliteRowId, int defaultSrsId, GeoFeature& ioFeature) const = 0;

    /// Fetches all non-null SQLite attribute columns for `sqliteRowId` into `ioFeature.properties`.
    virtual bool LoadFeatureProperties(int64_t sqliteRowId, GeoFeature& ioFeature) const = 0;

    /// Thread-safe: returns a shared, immutable `DecodedGeometryPayload` for `featureId`, decoding it from
    /// the backing source (SQLite or HTTP) on the calling worker thread if not yet in cache.
    [[nodiscard]] virtual std::shared_ptr<const DecodedGeometryPayload> GetOrLoadGeometryPayload(
        int64_t featureId,
        int defaultSrsId,
        uint64_t accessTick = 0
    ) const;

    /// Non-blocking O(1) RAM lookup: returns the cached `DecodedGeometryPayload` if already decoded in RAM,
    /// or `nullptr` without touching disk/network.
    [[nodiscard]] virtual std::shared_ptr<const DecodedGeometryPayload> TryGetCachedGeometryPayload(
        int64_t featureId,
        uint64_t accessTick = 0
    ) const noexcept {
        (void)featureId;
        (void)accessTick;
        return nullptr;
    }

    /// Returns true when this layer streams features dynamically by spatial viewport chunks (for multi-gigabyte datasets).
    [[nodiscard]] virtual bool IsWindowStreamed() const noexcept { return false; }

    /// Fast O(1) non-blocking check (< 50 ns, never touches SQLite/network): returns true if the camera viewport
    /// `(geoViewBounds, tileZoom, scaleFiltering)` has moved outside the currently loaded safe window or changed scale band.
    [[nodiscard]] virtual bool NeedsSpatialWindowUpdate(
        const GeoLayer& layer,
        const guikit::GeoBounds& geoViewBounds,
        float tileZoom,
        bool scaleFiltering
    ) const noexcept {
        (void)layer;
        (void)geoViewBounds;
        (void)tileZoom;
        (void)scaleFiltering;
        return false;
    }

    /// Builds a complete, self-contained `SpatialWindowSnapshot` (`features` + pre-built `LayerSpatialIndex` R-Tree)
    /// on a background worker thread without mutating any live `GeoLayer` on the UI thread.
    /// If `activeGeneration` no longer matches `req.requestGeneration`, aborts early and returns `nullptr`.
    [[nodiscard]] virtual std::shared_ptr<SpatialWindowSnapshot> BuildSpatialWindowSnapshot(
        const SpatialWindowRequest& req,
        const std::atomic<uint64_t>* activeGeneration = nullptr
    ) const {
        (void)req;
        (void)activeGeneration;
        return nullptr;
    }

    /// Synchronizes `ioLayer.features` and `ioLayer.spatialIndex` with the active camera viewport
    /// `(geoViewBounds, normViewBox)` and `tileZoom`.
    /// Returns true if `ioLayer.features` changed (new spatial chunks were streamed in or scale band switched).
    virtual bool SyncSpatialWindow(
        GeoLayer& ioLayer,
        const guikit::GeoBounds& geoViewBounds,
        const guikit::Rect& normViewBox,
        float tileZoom,
        bool scaleFiltering,
        uint64_t frameTick,
        std::vector<guikit::Rect>* outDirtiedNormBoxes = nullptr) const {
        (void)ioLayer;
        (void)geoViewBounds;
        (void)normViewBox;
        (void)tileZoom;
        (void)scaleFiltering;
        (void)frameTick;
        (void)outDirtiedNormBoxes;
        return false;
    }

    /// Returns current resident heap bytes held by this pager's internal spatial chunk & geometry caches.
    [[nodiscard]] virtual size_t GetCachedChunkBytes() const noexcept { return 0; }

    /// Returns total number of features currently held in this pager's internal spatial chunk cache.
    [[nodiscard]] virtual size_t GetCachedFeatureCount() const noexcept { return 0; }

    /// Evicts least-recently-used spatial chunks from the pager's internal cache to stay within `maxBytes`.
    virtual size_t EvictChunksToBudget(size_t maxBytes, uint64_t protectedTick) const noexcept {
        (void)maxBytes;
        (void)protectedTick;
        return 0;
    }

    /// Releases any cached SQLite database connection / prepared statements when idle.
    virtual void CloseIdleConnection() const noexcept {}
};

/// Single geographic vector feature with geometry, pre-projected Mercator cache, and marine semantics.
struct GeoFeature {
    uint64_t uid{0};
    std::string id{};           ///< Scene-unique interactive ID (e.g. "L1_F0")
    std::string displayId{};    ///< Original feature ID or FID from GeoJSON / GPKG
    std::string name{};         ///< Resolved human-readable label
    GeometryType type{GeometryType::Point};

    // Semantic marine chart metadata (extracted once at load/finalization time)
    std::string featureCode{};  ///< Uppercase S-57 / OGC feature code (e.g. "DEPARE", "SOUNDG", "BOYLAT")
    ChartFeatureClass chartClass{ChartFeatureClass::Generic};
    bool hasLight{false};       ///< True when feature is a light ("LIGHTS") or carries S-57 LITCHR/SIGGRP/VALNMR
    float depthMeters{std::numeric_limits<float>::quiet_NaN()};
    float minDepthMeters{std::numeric_limits<float>::quiet_NaN()};
    float maxDepthMeters{std::numeric_limits<float>::quiet_NaN()};
    std::string lightOrColor{}; ///< IHO S-57 COLOUR or IALA light descriptor
    std::string labelRu{};
    std::string labelEn{};

    // Out-of-Core SQLite Spatial Paging & LRU Eviction state
    int64_t sqliteRowId{0};                         ///< > 0 when backed by an on-disk SQLite .gpkg row
    int32_t srsId{4326};                            ///< Source SRS ID for on-demand geometry decoding
    uint32_t cachedVertexCount{0};                  ///< Preserved total vertex count even when geometry is evicted
    uint32_t cachedPolygonCount{0};                 ///< Preserved polygon count even when geometry is evicted
    uint32_t cachedHoleCount{0};                    ///< Preserved interior hole count even when geometry is evicted
    const IFeaturePager* pager{nullptr};            ///< Non-owning pointer to parent GeoLayer's pager
    mutable uint32_t residentHeapBytes{0};          ///< Dynamic heap footprint of currently loaded rings & properties
    mutable uint64_t lastAccessTick{0};             ///< Frame/access tick for LRU geometry & property eviction
    mutable bool geometryLoaded{true};              ///< False when polygon/line rings are unloaded from RAM
    mutable bool propertiesLoaded{true};            ///< False when full attribute dictionary is unloaded from RAM

    mutable std::vector<guikit::GeoCoord> points{};
    mutable std::vector<std::vector<guikit::GeoCoord>> lines{};
    mutable std::vector<GeoPolygon> polygons{};

    // Pre-projected normalized Web Mercator [0..1]x[0..1] coordinates & LODs
    mutable std::vector<guikit::Vec2> normPoints{};
    mutable std::vector<std::vector<guikit::Vec2>> normLines{};
    mutable std::vector<std::vector<guikit::Vec2>> normLinesLod{};
    guikit::Rect normBounds{};
    guikit::Vec2 normCentroid{};

    guikit::GeoBounds bounds{};
    guikit::GeoCoord centroid{};
    bool hasValidBounds{false};

    mutable std::shared_ptr<const DecodedGeometryPayload> sharedGeometry{};
    mutable std::vector<std::pair<std::string, AttributeValue>> properties{};

    [[nodiscard]] bool HasDepth() const noexcept { return !std::isnan(depthMeters); }
    [[nodiscard]] bool HasDepthRange() const noexcept { return !std::isnan(minDepthMeters) || !std::isnan(maxDepthMeters); }

    /// Returns true if this feature has polygonal geometry (whether currently resident in RAM or paged out on disk).
    [[nodiscard]] bool HasPolygons() const noexcept {
        return !polygons.empty() || (sharedGeometry && !sharedGeometry->polygons.empty()) ||
               cachedPolygonCount > 0 || IsPolygonalType(type);
    }

    /// Returns true if this feature has linear geometry (whether currently resident in RAM or paged out on disk).
    [[nodiscard]] bool HasLines() const noexcept {
        return !lines.empty() || (sharedGeometry && !sharedGeometry->lines.empty()) || IsLinearType(type);
    }

    /// Number of polygons in this feature (works even when geometry payload is currently paged out).
    [[nodiscard]] size_t PolygonCount() const noexcept {
        if (!polygons.empty()) return polygons.size();
        if (sharedGeometry && !sharedGeometry->polygons.empty()) return sharedGeometry->polygons.size();
        return static_cast<size_t>(cachedPolygonCount);
    }

    /// Ensures the vector geometry payload (`polygons`, `lines`, `normLines`, `normExterior`) is resident in RAM.
    /// Returns true if a page-in from SQLite actually occurred on this call.
    bool EnsureGeometryLoaded(uint64_t accessTick = 0) const;

    /// Non-blocking O(1) check: if geometry is already loaded or cached in the pager's RAM cache,
    /// attaches it without touching disk/network and returns true. Never blocks on SQLite/HTTP!
    bool TryEnsureGeometryLoadedNonBlocking(uint64_t accessTick = 0) const noexcept;

    /// Copies or moves decoded geometry fields from a `DecodedGeometryPayload` into this feature.
    void ApplyDecodedGeometryPayload(const DecodedGeometryPayload& payload) const;

    /// Builds an immutable `DecodedGeometryPayload` from this feature's currently resident geometry.
    [[nodiscard]] std::shared_ptr<const DecodedGeometryPayload> BuildDecodedGeometryPayload() const;

    /// Ensures the full SQLite attribute dictionary (`properties`) is resident in RAM.
    /// Returns true if a page-in from SQLite actually occurred on this call.
    bool EnsurePropertiesLoaded(uint64_t accessTick = 0) const;

    /// Updates the LRU access timestamp without triggering I/O.
    void TouchAccess(uint64_t accessTick) const noexcept {
        if (accessTick > 0) {
            lastAccessTick = accessTick;
        }
    }

    /// Evicts heavy polygon/line vertex buffers from RAM (only if backed by an `IFeaturePager` with `sqliteRowId > 0`).
    /// Returns the number of heap bytes freed.
    size_t UnloadGeometryPayload() const noexcept;

    /// Evicts the attribute dictionary from RAM (only if backed by an `IFeaturePager` with `sqliteRowId > 0`).
    /// Returns the number of heap bytes freed.
    size_t UnloadPropertiesPayload() const noexcept;

    /// Computes and updates `residentHeapBytes` for the currently loaded vertex rings and properties.
    size_t RefreshResidentHeapBytes() const noexcept;

    /// Recomputes bounding box, centroid, Web Mercator pre-projection, and LODs.
    void RecomputeBoundsAndCentroid();

    /// Projects only the already-known `bounds` (`nw`, `se`) and `centroid` into `normBounds` and `normCentroid`
    /// without requiring vertex arrays to be loaded in RAM.
    void RecomputeNormalizedEnvelopeFromBounds() noexcept;

    /// Extracts marine chart metadata (`featureCode`, `chartClass`, depths, labels) and resolves `name`.
    void ResolveDisplayName(size_t fallbackIndex, std::string_view layerHint = {});

    /// Sets or updates a property key-value pair.
    void SetProperty(std::string key, AttributeValue val);

    /// Finds a property by case-insensitive key and returns its string representation, or empty if absent.
    [[nodiscard]] std::string GetPropertyString(std::string_view key) const;

    /// Finds a numeric property by case-insensitive key, or returns NaN if absent.
    [[nodiscard]] float GetPropertyFloat(std::string_view key) const noexcept;

    /// Total number of vertices across all points, lines, and polygon rings.
    [[nodiscard]] size_t VertexCount() const noexcept;
    [[nodiscard]] size_t TotalVertexCount() const noexcept { return VertexCount(); }

    /// Total number of triangulated fill triangles across all polygons.
    [[nodiscard]] size_t TriangleCount() const noexcept;

    /// Total number of interior hole rings across all polygons.
    [[nodiscard]] size_t HoleCount() const noexcept;

    /// Case-insensitive full-text search across feature name, ID, feature code, and properties.
    [[nodiscard]] bool MatchesSearch(std::string_view query) const;
};

/// Flat Sort-Tile-Recursive (STR) R-Tree node over normalized Web Mercator [0..1]x[0..1] bounding boxes.
struct RTreeNode {
    guikit::Rect bounds{};
    uint32_t firstChild{0};
    uint16_t count{0};
    bool isLeaf{true};
};

/// High-speed 2D spatial index (Flat STR R-Tree) for O(log N) frustum culling, tile queries, and hit-testing.
class LayerSpatialIndex {
public:
    void Clear() noexcept {
        m_nodes.clear();
        m_leafFeatureIndices.clear();
        m_rootIndex = 0;
    }

    [[nodiscard]] bool Empty() const noexcept { return m_nodes.empty(); }

    /// Builds a flat packed STR R-Tree over the features of a layer.
    void Build(const std::vector<GeoFeature>& features);

    /// Appends indices of all features whose normalized Mercator bounds intersect `queryBox`.
    void QueryBox(guikit::Rect queryBox, std::vector<uint32_t>& outFeatureIndices) const;

private:
    std::vector<RTreeNode> m_nodes{};
    std::vector<uint32_t> m_leafFeatureIndices{};
    uint32_t m_rootIndex{0};
};

/// Background spatial window request posted by the UI thread to `AsyncChartStreamer`.
struct SpatialWindowRequest {
    uint64_t layerUid{0};
    int srsId{4326};
    guikit::GeoBounds layerBounds{};
    bool hasLayerBounds{false};
    ChartFeatureClass dominantClass{ChartFeatureClass::Generic};
    uint32_t chartClassMask{0};
    size_t savedPointCount{0};
    size_t savedLineCount{0};
    size_t savedPolygonCount{0};
    guikit::GeoBounds viewBounds{};
    guikit::Rect normViewBox{};
    float tileZoom{10.0f};
    bool scaleFiltering{true};
    uint64_t frameTick{0};
    uint64_t requestGeneration{0};
};

/// Self-contained spatial window snapshot built off-thread by `IFeaturePager::BuildSpatialWindowSnapshot`.
struct SpatialWindowSnapshot {
    uint64_t layerUid{0};
    uint64_t requestGeneration{0};
    std::vector<GeoFeature> features{};
    LayerSpatialIndex spatialIndex{};
    std::vector<guikit::Rect> dirtiedNormBoxes{};
    size_t pointFeatureCount{0};
    size_t lineFeatureCount{0};
    size_t polygonFeatureCount{0};
    size_t totalVertices{0};
    ChartFeatureClass dominantChartClass{ChartFeatureClass::Generic};
    uint32_t chartClassMask{0};
    size_t newlyPagedFeatures{0};
    bool changed{false};
};

/// Visual styling parameters for rendering a vector GIS layer.
struct LayerStyle {
    guikit::Color primaryColor{56, 189, 248, 255};
    float fillOpacity{1.0f};
    float strokeOpacity{0.92f};
    float strokeWidthPx{2.0f};
    float pointRadiusPx{6.5f};
    bool renderFill{true};
    bool renderStroke{true};
    bool showLabels{true};
    bool renderAsSeamark{false};
    bool useCustomColor{false};
};

/// Vector GIS layer loaded from a GeoJSON file or a GeoPackage (.gpkg) feature table.
struct GeoLayer {
    uint64_t uid{0};
    std::string name{"Untitled Layer"};
    std::string sourcePath{};
    std::string format{"GeoJSON"};  ///< "GeoJSON" or "GeoPackage"
    std::string tableName{};        ///< SQLite feature table name (for .gpkg)
    int srsId{4326};                ///< Original spatial reference system ID (e.g. 4326, 3857, 3395)
    bool visible{true};
    LayerStyle style{};

    std::shared_ptr<IFeaturePager> pager{};
    std::vector<GeoFeature> features{};
    LayerSpatialIndex spatialIndex{};
    guikit::GeoBounds bounds{};
    guikit::Rect normBounds{};
    bool hasValidBounds{false};

    // Cached summary telemetry & semantic classification
    size_t pointFeatureCount{0};
    size_t lineFeatureCount{0};
    size_t polygonFeatureCount{0};
    size_t totalVertices{0};
    size_t totalTriangles{0};
    size_t estimatedTotalFeatures{0}; ///< Total rows in backing SQLite table (even when window-streamed)
    uint64_t windowGeneration{0};     ///< Incremented whenever active window features are rebuilt
    bool isWindowStreamed{false};     ///< True when features are dynamically streamed per viewport window
    ChartFeatureClass dominantChartClass{ChartFeatureClass::Generic};
    uint32_t chartClassMask{0};       ///< Bitmask of (1u << static_cast<uint8_t>(cls)) present in this layer

    [[nodiscard]] bool HasChartClass(ChartFeatureClass cls) const noexcept {
        return (chartClassMask & (1u << static_cast<uint8_t>(cls))) != 0u;
    }

    [[nodiscard]] bool IsExclusivelyChartClasses(uint32_t allowedMask) const noexcept {
        return chartClassMask != 0u && (chartClassMask & ~allowedMask) == 0u;
    }

    /// Assigns deterministic IDs, pre-projects Mercator LODs, builds the R-Tree spatial index, and computes telemetry.
    void FinalizeLayer(uint64_t layerUid, bool forceTriangulateAll = false);

    /// Atomically swaps in a pre-built `SpatialWindowSnapshot` in O(1) time without rebuilding the R-Tree on the UI thread.
    bool ApplyWindowSnapshot(SpatialWindowSnapshot&& snapshot) noexcept;

    /// Rebinds `feat.pager = pager.get()` on all features in this layer.
    void BindFeaturePagerPointers() noexcept {
        const IFeaturePager* rawPager = pager.get();
        for (GeoFeature& f : features) {
            f.pager = rawPager;
            f.srsId = srsId;
        }
    }

    /// Evicts all paged-in geometries & properties from RAM back to SQLite (returning freed bytes).
    size_t EvictAllPagedPayloads() const noexcept;
};

/// Coordinate Reference System (SRS) conversion and spatial bounding box utilities.
class SrsTransform {
public:
    /// Converts (x, y) in source SRS into WGS84 geographic coordinates (lat, lon in degrees).
    /// Supports EPSG:4326, 3857/900913, 3395, 32601..32660 (UTM N), 32701..32760 (UTM S),
    /// 20901..20960 & 7683 (GSK-2011), 28401..28432 / 28461..28492 & 4284 (Pulkovo 1942 / SK-42),
    /// 20001..20032 / 20061..20092 & 4200 (SK-95), 7679 (PZ-90.11), 3995 (Arctic Polar), and 3031 (Antarctic Polar).
    [[nodiscard]] static guikit::GeoCoord ToWgs84(double x, double y, int srsId = 4326) noexcept;

    /// Converts WGS84 (lat, lon) into target SRS coordinates (x, y) for any supported EPSG code.
    [[nodiscard]] static std::pair<double, double> FromWgs84(guikit::GeoCoord coord, int srsId = 3857) noexcept;

    /// Converts WGS84 (lat, lon) to Web Mercator (EPSG:3857) meters (x, y).
    [[nodiscard]] static std::pair<double, double> Wgs84ToWebMercatorMeters(guikit::GeoCoord coord) noexcept;

    /// Expands `bounds` to include `pt`.
    static void ExpandBounds(guikit::GeoBounds& bounds, bool& initialized, guikit::GeoCoord pt) noexcept;

    /// Expands `bounds` to include `other`.
    static void UnionBounds(guikit::GeoBounds& bounds, bool& initialized, const guikit::GeoBounds& other) noexcept;

    /// Returns true if two geographic bounding boxes overlap.
    [[nodiscard]] static bool BoundsIntersect(const guikit::GeoBounds& a, const guikit::GeoBounds& b) noexcept;

    /// Computes the geographic center of a bounding box.
    [[nodiscard]] static guikit::GeoCoord BoundsCenter(const guikit::GeoBounds& bounds) noexcept;

    /// Computes an appropriate Slippy Map tile zoom level [1..18] to frame `bounds`.
    [[nodiscard]] static float EstimateFitTileZoom(const guikit::GeoBounds& bounds) noexcept;

    /// Returns a distinct, high-contrast cartographic layer color for a given palette index.
    [[nodiscard]] static guikit::Color DefaultLayerColor(size_t paletteIndex) noexcept;

    /// Classifies an S-57 / OGC feature code (or thematic layer hint) into a `ChartFeatureClass`.
    [[nodiscard]] static ChartFeatureClass ClassifyFeatureCode(
        std::string_view featureCode,
        GeometryType geomType,
        std::string_view layerOrTableHint = {}
    ) noexcept;
};

/// Fast O(1)-hinted lookup of a `GeoFeature` by its 64-bit `uid` across a span of `GeoLayer`s.
[[nodiscard]] const GeoFeature* FindFeatureByUid(
    std::span<const GeoLayer> layers,
    uint64_t uid,
    const GeoLayer** outLayer = nullptr
) noexcept;

} // namespace guikit::geo

namespace guikit {
using namespace geo;
} // namespace guikit
