#include "guikit/geo/geo_types.hpp"
#include "guikit/geo/polygon_triangulator.hpp"
#include <cmath>
#include <mutex>
#include "keel/core.h"
#include "keel/format.h"
#include "keel/lru_cache.h"
#include "keel/number.h"
#include "keel/static_array.h"

namespace guikit::geo {

namespace {

[[nodiscard]] constexpr char LowerChar(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

[[nodiscard]] constexpr char UpperChar(char c) noexcept {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - ('a' - 'A')) : c;
}

[[nodiscard]] bool EqualsIgnoreCaseAscii(keel::StringRef a, keel::StringRef b) noexcept {
    if (a.size() != b.size()) return false;
    for (keel::usize i = 0; i < a.size(); ++i) {
        if (LowerChar(a[i]) != LowerChar(b[i])) return false;
    }
    return true;
}

[[nodiscard]] keel::String ToLowerAscii(keel::StringRef sv) {
    keel::String out = keel::String::from(sv);
    for (char& c : out) {
        c = LowerChar(c);
    }
    return out;
}

[[nodiscard]] keel::String ToUpperAscii(keel::StringRef sv) {
    keel::String out = keel::String::from(sv);
    for (char& c : out) {
        c = UpperChar(c);
    }
    return out;
}

[[nodiscard]] bool ContainsCaseInsensitive(keel::StringRef haystack, keel::StringRef needleLower) noexcept {
    if (needleLower.empty()) return true;
    if (haystack.size() < needleLower.size()) return false;
    const keel::usize limit = haystack.size() - needleLower.size();
    for (keel::usize i = 0; i <= limit; ++i) {
        bool match = true;
        for (keel::usize j = 0; j < needleLower.size(); ++j) {
            if (LowerChar(haystack[i + j]) != needleLower[j]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

void ExpandNormBounds(guikit::Rect& r, bool& init, guikit::Vec2 p) noexcept {
    if (!init) {
        r = guikit::Rect(p.x, p.y, 0.0f, 0.0f);
        init = true;
    } else {
        const float minX = keel::min(r.x, p.x);
        const float minY = keel::min(r.y, p.y);
        const float maxX = keel::max(r.Right(), p.x);
        const float maxY = keel::max(r.Bottom(), p.y);
        r = guikit::Rect(minX, minY, maxX - minX, maxY - minY);
    }
}

void UnionNormBounds(guikit::Rect& r, bool& init, guikit::Rect other) noexcept {
    ExpandNormBounds(r, init, other.Pos());
    ExpandNormBounds(r, init, {other.Right(), other.Bottom()});
}

// Fast radial + perpendicular vertex decimation for overview LOD rings
void BuildLodRing(
    keel::ArrayRef<guikit::Vec2> src,
    guikit::Rect normBox,
    bool closed,
    keel::Array<guikit::Vec2>& outLod
) {
    outLod.clear();
    if (src.size() <= 20) {
        return;
    }
    const float span = keel::max(normBox.w, normBox.h);
    const float tol = keel::max(1e-7f, span * 0.015f);
    const float tolSq = tol * tol;

    outLod.reserve(keel::min<keel::usize>(src.size(), 64));
    outLod.push_back(src.front());
    guikit::Vec2 lastKept = src.front();

    for (keel::usize i = 1; i + 1 < src.size(); ++i) {
        const float dx = src[i].x - lastKept.x;
        const float dy = src[i].y - lastKept.y;
        if (dx * dx + dy * dy >= tolSq) {
            outLod.push_back(src[i]);
            lastKept = src[i];
        }
    }
    outLod.push_back(src.back());
    if (closed && outLod.size() < 3) {
        outLod.clear();
    }
}

} // namespace

keel::String AttributeValue::ToString() const {
    switch (m_kind) {
        case Kind::Null:   return keel::String::from("null");
        case Kind::Bool:   return keel::String::from(m_bool ? "true" : "false");
        case Kind::Int:    return keel::format("%lld", static_cast<long long>(m_int));
        case Kind::Double: return keel::format("%.6g", m_double);
        case Kind::String: return m_str.clone();
    }
    return {};
}

keel::StringRef AttributeValue::TypeLabel() const noexcept {
    switch (m_kind) {
        case Kind::Null:   return "null";
        case Kind::Bool:   return "bool";
        case Kind::Int:    return "int";
        case Kind::Double: return "float";
        case Kind::String: return "str";
    }
    return "unknown";
}

bool AttributeValue::MatchesSearch(keel::StringRef queryLower) const {
    if (queryLower.empty()) return true;
    if (m_kind == Kind::String) {
        return ContainsCaseInsensitive(m_str.as_ref(), queryLower);
    }
    const keel::String tmp = ToString();
    return ContainsCaseInsensitive(tmp.as_ref(), queryLower);
}

GeoFeature GeoFeature::clone() const {
    GeoFeature copy{};
    copy.uid = uid;
    copy.id = id.clone();
    copy.displayId = displayId.clone();
    copy.name = name.clone();
    copy.type = type;
    copy.featureCode = featureCode.clone();
    copy.chartClass = chartClass;
    copy.hasLight = hasLight;
    copy.depthMeters = depthMeters;
    copy.minDepthMeters = minDepthMeters;
    copy.maxDepthMeters = maxDepthMeters;
    copy.lightOrColor = lightOrColor.clone();
    copy.labelRu = labelRu.clone();
    copy.labelEn = labelEn.clone();
    copy.sqliteRowId = sqliteRowId;
    copy.srsId = srsId;
    copy.cachedVertexCount = cachedVertexCount;
    copy.cachedPolygonCount = cachedPolygonCount;
    copy.cachedHoleCount = cachedHoleCount;
    copy.pager = pager;
    copy.residentHeapBytes = residentHeapBytes;
    copy.lastAccessTick = lastAccessTick;
    copy.geometryLoaded = geometryLoaded;
    copy.propertiesLoaded = propertiesLoaded;
    copy.points = points.clone();
    copy.lines = lines.clone();
    copy.polygons = polygons.clone();
    copy.normPoints = normPoints.clone();
    copy.normLines = normLines.clone();
    copy.normLinesLod = normLinesLod.clone();
    copy.normBounds = normBounds;
    copy.normCentroid = normCentroid;
    copy.bounds = bounds;
    copy.centroid = centroid;
    copy.hasValidBounds = hasValidBounds;
    copy.sharedGeometry = sharedGeometry;
    copy.properties = properties.clone();
    return copy;
}

GeoLayer GeoLayer::clone() const {
    GeoLayer copy{};
    copy.uid = uid;
    copy.name = name.clone();
    copy.sourcePath = sourcePath.clone();
    copy.format = format.clone();
    copy.tableName = tableName.clone();
    copy.srsId = srsId;
    copy.visible = visible;
    copy.style = style;
    copy.pager = pager;
    copy.features = features.clone();
    copy.spatialIndex = spatialIndex.clone();
    copy.bounds = bounds;
    copy.normBounds = normBounds;
    copy.hasValidBounds = hasValidBounds;
    copy.pointFeatureCount = pointFeatureCount;
    copy.lineFeatureCount = lineFeatureCount;
    copy.polygonFeatureCount = polygonFeatureCount;
    copy.totalVertices = totalVertices;
    copy.totalTriangles = totalTriangles;
    copy.estimatedTotalFeatures = estimatedTotalFeatures;
    copy.windowGeneration = windowGeneration;
    copy.isWindowStreamed = isWindowStreamed;
    copy.dominantChartClass = dominantChartClass;
    copy.chartClassMask = chartClassMask;
    copy.BindFeaturePagerPointers();
    return copy;
}

keel::Arc<const DecodedGeometryPayload> IFeaturePager::GetOrLoadGeometryPayload(
    keel::i64 featureId,
    keel::i32 defaultSrsId,
    keel::u64 accessTick
) const {
    if (auto cached = TryGetCachedGeometryPayload(featureId, accessTick)) {
        return cached;
    }
    GeoFeature tempFeat;
    tempFeat.sqliteRowId = featureId;
    tempFeat.srsId = defaultSrsId;
    tempFeat.geometryLoaded = false;
    if (!LoadFeatureGeometry(featureId, defaultSrsId, tempFeat)) {
        return {};
    }
    return tempFeat.BuildDecodedGeometryPayload();
}

void GeoFeature::ApplyDecodedGeometryPayload(const DecodedGeometryPayload& payload) const {
    auto& mutableSelf = const_cast<GeoFeature&>(*this);
    mutableSelf.type = payload.type;
    if (!payload.points.empty()) {
        points = payload.points.clone();
        normPoints = payload.normPoints.clone();
    }
    lines = payload.lines.clone();
    normLines = payload.normLines.clone();
    normLinesLod = payload.normLinesLod.clone();
    polygons = payload.polygons.clone();
    if (payload.bounds.nw.lat != 0.0 || payload.bounds.se.lat != 0.0 ||
        payload.bounds.nw.lon != 0.0 || payload.bounds.se.lon != 0.0) {
        mutableSelf.bounds = payload.bounds;
        mutableSelf.normBounds = payload.normBounds;
        mutableSelf.centroid = payload.centroid;
        mutableSelf.normCentroid = payload.normCentroid;
        mutableSelf.hasValidBounds = true;
    }
    if (payload.cachedVertexCount > 0) mutableSelf.cachedVertexCount = payload.cachedVertexCount;
    if (payload.cachedPolygonCount > 0) mutableSelf.cachedPolygonCount = payload.cachedPolygonCount;
    if (payload.cachedHoleCount > 0) mutableSelf.cachedHoleCount = payload.cachedHoleCount;
    geometryLoaded = true;
    (void)RefreshResidentHeapBytes();
}

keel::Arc<const DecodedGeometryPayload> GeoFeature::BuildDecodedGeometryPayload() const {
    auto payload = keel::make_arc<DecodedGeometryPayload>();
    payload->featureId = sqliteRowId;
    payload->type = type;
    payload->points = points.clone();
    payload->lines = lines.clone();
    payload->polygons = polygons.clone();
    payload->normPoints = normPoints.clone();
    payload->normLines = normLines.clone();
    payload->normLinesLod = normLinesLod.clone();
    payload->bounds = bounds;
    payload->normBounds = normBounds;
    payload->centroid = centroid;
    payload->normCentroid = normCentroid;
    payload->cachedVertexCount = cachedVertexCount;
    payload->cachedPolygonCount = cachedPolygonCount;
    payload->cachedHoleCount = cachedHoleCount;
    payload->residentHeapBytes = static_cast<keel::u32>(keel::min<keel::usize>(RefreshResidentHeapBytes(), ~keel::u32{0}));
    return payload;
}

bool GeoFeature::TryEnsureGeometryLoadedNonBlocking(keel::u64 accessTick) const noexcept {
    if (accessTick > 0) {
        lastAccessTick = accessTick;
    }
    if (geometryLoaded) {
        return true;
    }
    if (!pager || sqliteRowId <= 0) {
        geometryLoaded = true;
        return true;
    }
    if (sharedGeometry) {
        try {
            ApplyDecodedGeometryPayload(*sharedGeometry);
            return true;
        } catch (...) {
            return false;
        }
    }
    if (auto cached = pager->TryGetCachedGeometryPayload(sqliteRowId, accessTick)) {
        try {
            sharedGeometry = keel::move(cached);
            ApplyDecodedGeometryPayload(*sharedGeometry);
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

bool GeoFeature::EnsureGeometryLoaded(keel::u64 accessTick) const {
    if (TryEnsureGeometryLoadedNonBlocking(accessTick)) {
        return true;
    }
    if (auto payload = pager->GetOrLoadGeometryPayload(sqliteRowId, srsId, accessTick)) {
        sharedGeometry = keel::move(payload);
        ApplyDecodedGeometryPayload(*sharedGeometry);
        return true;
    }
    auto& mutableSelf = const_cast<GeoFeature&>(*this);
    const bool loaded = pager->LoadFeatureGeometry(sqliteRowId, srsId, mutableSelf);
    geometryLoaded = loaded;
    if (loaded) {
        sharedGeometry = BuildDecodedGeometryPayload();
    }
    (void)RefreshResidentHeapBytes();
    return loaded;
}

bool GeoFeature::EnsurePropertiesLoaded(keel::u64 accessTick) const {
    if (accessTick > 0) {
        lastAccessTick = accessTick;
    }
    if (propertiesLoaded) {
        return true;
    }
    if (!pager || sqliteRowId <= 0) {
        propertiesLoaded = true;
        return true;
    }
    auto& mutableSelf = const_cast<GeoFeature&>(*this);
    const bool loaded = pager->LoadFeatureProperties(sqliteRowId, mutableSelf);
    propertiesLoaded = loaded;
    (void)RefreshResidentHeapBytes();
    return loaded;
}

keel::usize GeoFeature::RefreshResidentHeapBytes() const noexcept {
    keel::usize bytes = 0;
    for (const auto& line : lines) {
        bytes += line.capacity() * sizeof(guikit::GeoCoord);
    }
    for (const auto& nLine : normLines) {
        bytes += nLine.capacity() * sizeof(guikit::Vec2);
    }
    for (const auto& lodLine : normLinesLod) {
        bytes += lodLine.capacity() * sizeof(guikit::Vec2);
    }
    for (const auto& poly : polygons) {
        bytes += sizeof(GeoPolygon);
        bytes += poly.exteriorRing.capacity() * sizeof(guikit::GeoCoord);
        bytes += poly.normExterior.capacity() * sizeof(guikit::Vec2);
        bytes += poly.normExteriorLod.capacity() * sizeof(guikit::Vec2);
        bytes += poly.triangles.capacity() * sizeof(GeoTriangle);
        for (const auto& hole : poly.interiorRings) {
            bytes += hole.capacity() * sizeof(guikit::GeoCoord);
        }
        for (const auto& nHole : poly.normInterior) {
            bytes += nHole.capacity() * sizeof(guikit::Vec2);
        }
    }
    for (const auto& [k, v] : properties) {
        bytes += sizeof(GeoProperty) + k.capacity() + v.string_capacity();
    }
    residentHeapBytes = static_cast<keel::u32>(keel::min<keel::usize>(bytes, ~keel::u32{0}));
    return bytes;
}

keel::usize GeoFeature::UnloadGeometryPayload() const noexcept {
    if (!pager || sqliteRowId <= 0 || (!geometryLoaded && !sharedGeometry) ||
        (polygons.empty() && lines.empty() && !sharedGeometry)) {
        return 0;
    }
    const keel::usize before = residentHeapBytes;
    polygons.clear();
    polygons.shrink_to_fit();
    lines.clear();
    lines.shrink_to_fit();
    normLines.clear();
    normLines.shrink_to_fit();
    normLinesLod.clear();
    normLinesLod.shrink_to_fit();
    sharedGeometry.reset();
    geometryLoaded = false;
    const keel::usize after = RefreshResidentHeapBytes();
    return (before > after) ? (before - after) : 0;
}

keel::usize GeoFeature::UnloadPropertiesPayload() const noexcept {
    if (!pager || sqliteRowId <= 0 || properties.empty()) {
        propertiesLoaded = false;
        return 0;
    }
    const keel::usize before = residentHeapBytes;
    properties.clear();
    properties.shrink_to_fit();
    propertiesLoaded = false;
    const keel::usize after = RefreshResidentHeapBytes();
    return (before > after) ? (before - after) : 0;
}

void GeoFeature::RecomputeNormalizedEnvelopeFromBounds() noexcept {
    if (!hasValidBounds) {
        normBounds = {};
        normCentroid = {};
        return;
    }
    const guikit::Vec2 nwNorm = guikit::MapGrid::GeoToNormalized(bounds.nw, guikit::MapCrs::EPSG3857_WebMercator);
    const guikit::Vec2 seNorm = guikit::MapGrid::GeoToNormalized(bounds.se, guikit::MapCrs::EPSG3857_WebMercator);
    const float minX = keel::min(nwNorm.x, seNorm.x);
    const float maxX = keel::max(nwNorm.x, seNorm.x);
    const float minY = keel::min(nwNorm.y, seNorm.y);
    const float maxY = keel::max(nwNorm.y, seNorm.y);
    normBounds = guikit::Rect(minX, minY, maxX - minX, maxY - minY);
    normCentroid = guikit::MapGrid::GeoToNormalized(centroid, guikit::MapCrs::EPSG3857_WebMercator);
}

void GeoFeature::RecomputeBoundsAndCentroid() {
    if (!geometryLoaded && points.empty() && lines.empty() && polygons.empty() && hasValidBounds) {
        RecomputeNormalizedEnvelopeFromBounds();
        return;
    }

    hasValidBounds = false;
    bounds = {};
    normBounds = {};
    bool normInit = false;

    double sumLat = 0.0;
    double sumLon = 0.0;
    keel::usize count = 0;
    keel::usize holeCount = 0;

    normPoints.resize(points.size());
    for (keel::usize i = 0; i < points.size(); ++i) {
        const guikit::GeoCoord pt = points[i];
        SrsTransform::ExpandBounds(bounds, hasValidBounds, pt);
        const guikit::Vec2 np = guikit::MapGrid::GeoToNormalized(pt, guikit::MapCrs::EPSG3857_WebMercator);
        normPoints[i] = np;
        ExpandNormBounds(normBounds, normInit, np);
        sumLat += pt.lat;
        sumLon += pt.lon;
        ++count;
    }

    normLines.resize(lines.size());
    normLinesLod.resize(lines.size());
    for (keel::usize li = 0; li < lines.size(); ++li) {
        const auto& line = lines[li];
        auto& nLine = normLines[li];
        nLine.resize(line.size());
        guikit::Rect lineNormBox{};
        bool lineBoxInit = false;
        for (keel::usize pi = 0; pi < line.size(); ++pi) {
            const guikit::GeoCoord pt = line[pi];
            SrsTransform::ExpandBounds(bounds, hasValidBounds, pt);
            const guikit::Vec2 np = guikit::MapGrid::GeoToNormalized(pt, guikit::MapCrs::EPSG3857_WebMercator);
            nLine[pi] = np;
            ExpandNormBounds(lineNormBox, lineBoxInit, np);
            sumLat += pt.lat;
            sumLon += pt.lon;
            ++count;
        }
        if (lineBoxInit) {
            UnionNormBounds(normBounds, normInit, lineNormBox);
            BuildLodRing(nLine, lineNormBox, /*closed=*/false, normLinesLod[li]);
        }
    }

    for (auto& poly : polygons) {
        poly.hasValidBounds = false;
        poly.bounds = {};
        poly.normBounds = {};
        bool polyNormInit = false;

        poly.normExterior.resize(poly.exteriorRing.size());
        for (keel::usize pi = 0; pi < poly.exteriorRing.size(); ++pi) {
            const guikit::GeoCoord pt = poly.exteriorRing[pi];
            SrsTransform::ExpandBounds(poly.bounds, poly.hasValidBounds, pt);
            SrsTransform::ExpandBounds(bounds, hasValidBounds, pt);
            const guikit::Vec2 np = guikit::MapGrid::GeoToNormalized(pt, guikit::MapCrs::EPSG3857_WebMercator);
            poly.normExterior[pi] = np;
            ExpandNormBounds(poly.normBounds, polyNormInit, np);
            sumLat += pt.lat;
            sumLon += pt.lon;
            ++count;
        }

        holeCount += poly.interiorRings.size();
        poly.normInterior.resize(poly.interiorRings.size());
        for (keel::usize hi = 0; hi < poly.interiorRings.size(); ++hi) {
            const auto& hole = poly.interiorRings[hi];
            auto& nHole = poly.normInterior[hi];
            nHole.resize(hole.size());
            for (keel::usize pi = 0; pi < hole.size(); ++pi) {
                const guikit::GeoCoord pt = hole[pi];
                SrsTransform::ExpandBounds(poly.bounds, poly.hasValidBounds, pt);
                SrsTransform::ExpandBounds(bounds, hasValidBounds, pt);
                const guikit::Vec2 np = guikit::MapGrid::GeoToNormalized(pt, guikit::MapCrs::EPSG3857_WebMercator);
                nHole[pi] = np;
                ExpandNormBounds(poly.normBounds, polyNormInit, np);
                sumLat += pt.lat;
                sumLon += pt.lon;
                ++count;
            }
        }

        if (polyNormInit) {
            UnionNormBounds(normBounds, normInit, poly.normBounds);
            BuildLodRing(poly.normExterior, poly.normBounds, /*closed=*/true, poly.normExteriorLod);
        }
    }

    cachedVertexCount = static_cast<keel::u32>(count);
    cachedPolygonCount = static_cast<keel::u32>(polygons.size());
    cachedHoleCount = static_cast<keel::u32>(holeCount);

    if (count > 0) {
        centroid = {sumLat / static_cast<double>(count), sumLon / static_cast<double>(count)};
    } else if (hasValidBounds) {
        centroid = SrsTransform::BoundsCenter(bounds);
    }
    normCentroid = guikit::MapGrid::GeoToNormalized(centroid, guikit::MapCrs::EPSG3857_WebMercator);
    (void)RefreshResidentHeapBytes();
}

void GeoFeature::ResolveDisplayName(keel::usize fallbackIndex, keel::StringRef layerHint) {
    // Single pass over properties to extract marine metadata & display name without O(K*M) allocations
    keel::String fallbackTitle;
    keel::String fallbackFeatureType;

    auto extractFloatAttr = [](const AttributeValue& v, float& ioTarget) noexcept {
        if (!std::isnan(ioTarget)) return;
        if (v.is_double() || v.is_int()) {
            ioTarget = static_cast<float>(v.as_double());
        } else if (v.is_string()) {
            if (const auto res = keel::parse_f32(v.as_string()); res.is_ok()) {
                ioTarget = res.value();
            }
        }
    };

    for (const auto& [k, v] : properties) {
        if (v.is_null()) continue;
        const keel::StringRef keyRef = k.as_ref();

        if (EqualsIgnoreCaseAscii(keyRef, "objl") || EqualsIgnoreCaseAscii(keyRef, "feature_code") ||
            EqualsIgnoreCaseAscii(keyRef, "acronym") || EqualsIgnoreCaseAscii(keyRef, "s57_class")) {
            if (featureCode.empty()) {
                const keel::String s = v.ToString();
                featureCode = ToUpperAscii(s.as_ref());
            }
        } else if (EqualsIgnoreCaseAscii(keyRef, "valsou") || EqualsIgnoreCaseAscii(keyRef, "depth") ||
                   EqualsIgnoreCaseAscii(keyRef, "depth_m") || EqualsIgnoreCaseAscii(keyRef, "valdco") ||
                   EqualsIgnoreCaseAscii(keyRef, "elevat") || EqualsIgnoreCaseAscii(keyRef, "height")) {
            extractFloatAttr(v, depthMeters);
        } else if (EqualsIgnoreCaseAscii(keyRef, "drval1") || EqualsIgnoreCaseAscii(keyRef, "min_depth_m") ||
                   EqualsIgnoreCaseAscii(keyRef, "depth_min") || EqualsIgnoreCaseAscii(keyRef, "min_depth")) {
            extractFloatAttr(v, minDepthMeters);
        } else if (EqualsIgnoreCaseAscii(keyRef, "drval2") || EqualsIgnoreCaseAscii(keyRef, "max_depth_m") ||
                   EqualsIgnoreCaseAscii(keyRef, "depth_max") || EqualsIgnoreCaseAscii(keyRef, "max_depth")) {
            extractFloatAttr(v, maxDepthMeters);
        } else if (EqualsIgnoreCaseAscii(keyRef, "colour") || EqualsIgnoreCaseAscii(keyRef, "color")) {
            if (lightOrColor.empty()) lightOrColor = v.ToString();
        } else if (EqualsIgnoreCaseAscii(keyRef, "litchr") || EqualsIgnoreCaseAscii(keyRef, "siggrp") ||
                   EqualsIgnoreCaseAscii(keyRef, "sigper") || EqualsIgnoreCaseAscii(keyRef, "valnmr") ||
                   EqualsIgnoreCaseAscii(keyRef, "seamark:light:character")) {
            keel::String s = v.ToString();
            if (!s.empty()) {
                hasLight = true;
                if (lightOrColor.empty()) lightOrColor = keel::move(s);
            }
        } else if (EqualsIgnoreCaseAscii(keyRef, "nobjnm") || EqualsIgnoreCaseAscii(keyRef, "name_ru") ||
                   EqualsIgnoreCaseAscii(keyRef, "label_ru") || EqualsIgnoreCaseAscii(keyRef, "buoy_name_ru")) {
            if (labelRu.empty()) labelRu = v.ToString();
        } else if (EqualsIgnoreCaseAscii(keyRef, "objnam") || EqualsIgnoreCaseAscii(keyRef, "name") ||
                   EqualsIgnoreCaseAscii(keyRef, "name_en") || EqualsIgnoreCaseAscii(keyRef, "label_en") ||
                   EqualsIgnoreCaseAscii(keyRef, "buoy_name_en")) {
            if (labelEn.empty()) labelEn = v.ToString();
        } else if (EqualsIgnoreCaseAscii(keyRef, "title") || EqualsIgnoreCaseAscii(keyRef, "label") ||
                   EqualsIgnoreCaseAscii(keyRef, "seamark:name") || EqualsIgnoreCaseAscii(keyRef, "port_name") ||
                   EqualsIgnoreCaseAscii(keyRef, "zone_name") || EqualsIgnoreCaseAscii(keyRef, "route_name") ||
                   EqualsIgnoreCaseAscii(keyRef, "natsur") || EqualsIgnoreCaseAscii(keyRef, "inform")) {
            if (fallbackTitle.empty()) fallbackTitle = v.ToString();
        } else if (EqualsIgnoreCaseAscii(keyRef, "feature_type") || EqualsIgnoreCaseAscii(keyRef, "layer")) {
            if (fallbackFeatureType.empty()) fallbackFeatureType = v.ToString();
        }
    }

    if (featureCode.empty() && !fallbackFeatureType.empty() && fallbackFeatureType.size() == 6) {
        featureCode = ToUpperAscii(fallbackFeatureType.as_ref());
    }
    if (featureCode.empty() && layerHint.size() == 6) {
        featureCode = ToUpperAscii(layerHint);
    }
    if (featureCode.as_ref() == "LIGHTS" || featureCode.as_ref() == "LITFLT" || featureCode.as_ref() == "LITVES") {
        hasLight = true;
    }

    if (chartClass == ChartFeatureClass::Generic) {
        chartClass = SrsTransform::ClassifyFeatureCode(
            featureCode.as_ref(),
            type,
            !fallbackFeatureType.empty() ? fallbackFeatureType.as_ref() : layerHint
        );
    }

    if (!name.empty()) {
        return;
    }

    if (!labelRu.empty()) {
        name = labelRu.clone();
        return;
    }
    if (!labelEn.empty()) {
        name = labelEn.clone();
        return;
    }
    if (!fallbackTitle.empty()) {
        name = keel::move(fallbackTitle);
        return;
    }
    if (chartClass == ChartFeatureClass::Sounding && !std::isnan(depthMeters)) {
        name = keel::format("%.1f m", static_cast<double>(depthMeters));
        return;
    }
    if ((chartClass == ChartFeatureClass::DepthContour || featureCode.as_ref() == "LNDELV") && !std::isnan(depthMeters)) {
        name = keel::format("%.4g m", static_cast<double>(depthMeters));
        return;
    }
    if (chartClass == ChartFeatureClass::DepthArea && (!std::isnan(minDepthMeters) || !std::isnan(maxDepthMeters))) {
        name = keel::format(
            "DEPARE %.2g..%.2g m",
            std::isnan(minDepthMeters) ? 0.0 : static_cast<double>(minDepthMeters),
            std::isnan(maxDepthMeters) ? 0.0 : static_cast<double>(maxDepthMeters)
        );
        return;
    }
    if (!lightOrColor.empty() && !featureCode.empty()) {
        name = keel::format("%s (%s)", featureCode, lightOrColor);
        return;
    }
    if (!featureCode.empty()) {
        if (!displayId.empty()) {
            name = keel::format("%s #%s", featureCode, displayId);
        } else {
            name = keel::format("%s #%zu", featureCode, fallbackIndex + 1);
        }
        return;
    }
    if (!fallbackFeatureType.empty()) {
        name = keel::move(fallbackFeatureType);
        return;
    }

    if (!displayId.empty()) {
        name = keel::format("%s #%s", GeometryTypeToString(type), displayId);
    } else {
        name = keel::format("%s #%zu", GeometryTypeToString(type), fallbackIndex + 1);
    }
}

void GeoFeature::SetProperty(keel::StringRef key, AttributeValue val) {
    for (auto& [k, v] : properties) {
        if (k.as_ref() == key) {
            v = keel::move(val);
            return;
        }
    }
    properties.push_back(GeoProperty{keel::String::from(key), keel::move(val)});
}

void GeoFeature::SetProperty(keel::String&& key, AttributeValue val) {
    for (auto& [k, v] : properties) {
        if (k.as_ref() == key.as_ref()) {
            v = keel::move(val);
            return;
        }
    }
    properties.push_back(GeoProperty{keel::move(key), keel::move(val)});
}

keel::String GeoFeature::GetPropertyString(keel::StringRef key) const {
    if (!propertiesLoaded && pager && sqliteRowId > 0) {
        (void)EnsurePropertiesLoaded();
    }
    for (const auto& [k, v] : properties) {
        if (EqualsIgnoreCaseAscii(k.as_ref(), key) && !v.is_null()) {
            return v.ToString();
        }
    }
    return {};
}

float GeoFeature::GetPropertyFloat(keel::StringRef key) const noexcept {
    if (!propertiesLoaded && pager && sqliteRowId > 0) {
        (void)EnsurePropertiesLoaded();
    }
    for (const auto& [k, v] : properties) {
        if (EqualsIgnoreCaseAscii(k.as_ref(), key) && !v.is_null()) {
            if (v.is_double() || v.is_int()) return static_cast<float>(v.as_double());
        }
    }
    return NAN;
}

keel::usize GeoFeature::VertexCount() const noexcept {
    keel::usize total = points.size();
    for (const auto& line : lines) {
        total += line.size();
    }
    for (const auto& poly : polygons) {
        total += poly.VertexCount();
    }
    if (total == 0 && cachedVertexCount > 0) {
        return static_cast<keel::usize>(cachedVertexCount);
    }
    return total;
}

keel::usize GeoFeature::TriangleCount() const noexcept {
    keel::usize total = 0;
    for (const auto& poly : polygons) {
        total += poly.triangles.size();
    }
    return total;
}

keel::usize GeoFeature::HoleCount() const noexcept {
    keel::usize total = 0;
    for (const auto& poly : polygons) {
        total += poly.interiorRings.size();
    }
    if (total == 0 && !geometryLoaded && cachedHoleCount > 0) {
        return static_cast<keel::usize>(cachedHoleCount);
    }
    return total;
}

bool GeoFeature::MatchesSearch(keel::StringRef query) const {
    if (query.empty()) return true;
    const keel::String qLower = ToLowerAscii(query);
    const keel::StringRef qRef = qLower.as_ref();

    if (ContainsCaseInsensitive(name.as_ref(), qRef)) return true;
    if (ContainsCaseInsensitive(featureCode.as_ref(), qRef)) return true;
    if (ContainsCaseInsensitive(labelRu.as_ref(), qRef)) return true;
    if (ContainsCaseInsensitive(labelEn.as_ref(), qRef)) return true;
    if (ContainsCaseInsensitive(lightOrColor.as_ref(), qRef)) return true;
    if (ContainsCaseInsensitive(displayId.as_ref(), qRef)) return true;
    if (ContainsCaseInsensitive(id.as_ref(), qRef)) return true;
    if (ContainsCaseInsensitive(GeometryTypeToString(type), qRef)) return true;

    for (const auto& [k, v] : properties) {
        if (ContainsCaseInsensitive(k.as_ref(), qRef) || v.MatchesSearch(qRef)) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Feature Spatial Index Builder (keel::RTree)
// ============================================================================

namespace {

void BuildFeatureSpatialIndex(keel::RTree<keel::u32, float>& index, keel::ArrayRef<GeoFeature> features) {
    index.clear();
    if (features.empty()) return;

    keel::ArenaTemp scratch = keel::get_scratch();
    auto* items = scratch.arena().alloc_array<keel::RTreeItem<keel::u32, float>>(features.size());
    keel::usize count = 0;
    for (keel::u32 i = 0; i < static_cast<keel::u32>(features.size()); ++i) {
        const GeoFeature& f = features[i];
        if (!f.hasValidBounds) continue;
        const guikit::Rect& b = f.normBounds;
        items[count++] = keel::RTreeItem<keel::u32, float>{
            .id = i,
            .box = keel::RTreeBox<float>::from_xywh(b.x, b.y, b.w, b.h).inflated_min_size(1e-7f)
        };
    }
    if (count > 0) {
        index.build(keel::ArrayRef<keel::RTreeItem<keel::u32, float>>(items, count));
    }
}

} // namespace

void GeoLayer::FinalizeLayer(keel::u64 layerUid, bool forceTriangulateAll) {
    uid = layerUid;
    const bool preserveStreamMetadata = isWindowStreamed && hasValidBounds;
    const guikit::GeoBounds savedBounds = bounds;
    const guikit::Rect savedNormBounds = normBounds;
    const keel::usize savedPointCount = pointFeatureCount;
    const keel::usize savedLineCount = lineFeatureCount;
    const keel::usize savedPolyCount = polygonFeatureCount;
    const ChartFeatureClass savedDominantClass = dominantChartClass;
    const keel::u32 savedClassMask = chartClassMask;

    if (!preserveStreamMetadata) {
        bounds = {};
        normBounds = {};
        hasValidBounds = false;
    }
    bool normInit = preserveStreamMetadata && (normBounds.w > 0.0f || normBounds.h > 0.0f);
    pointFeatureCount = 0;
    lineFeatureCount = 0;
    polygonFeatureCount = 0;
    totalVertices = 0;
    totalTriangles = 0;

    if (preserveStreamMetadata && !forceTriangulateAll) {
        const bool allPrebound = features.empty() ||
            (features.front().sqliteRowId > 0 &&
             features.front().pager != nullptr &&
             features.front().uid == ((layerUid << 32) | static_cast<keel::u64>(static_cast<keel::u32>(features.front().sqliteRowId))) &&
             !features.front().id.empty() &&
             !features.front().name.empty() &&
             features.back().uid == ((layerUid << 32) | static_cast<keel::u64>(static_cast<keel::u32>(features.back().sqliteRowId))));
        if (allPrebound) {
            bounds = savedBounds;
            normBounds = savedNormBounds;
            hasValidBounds = true;
            if (!features.empty()) {
                for (const GeoFeature& f : features) {
                    chartClassMask |= (1u << static_cast<keel::u8>(f.chartClass));
                    if (f.HasPolygons()) {
                        ++polygonFeatureCount;
                    } else if (f.HasLines()) {
                        ++lineFeatureCount;
                    } else {
                        ++pointFeatureCount;
                    }
                    totalVertices += keel::max<keel::usize>(1u, f.VertexCount());
                }
                if (dominantChartClass == ChartFeatureClass::Generic) {
                    dominantChartClass = features.front().chartClass;
                }
            } else {
                polygonFeatureCount = savedPolyCount;
                lineFeatureCount = savedLineCount;
                pointFeatureCount = savedPointCount;
            }
            dominantChartClass = (dominantChartClass != ChartFeatureClass::Generic) ? dominantChartClass : savedDominantClass;
            chartClassMask |= savedClassMask;
            ++windowGeneration;
            BuildFeatureSpatialIndex(spatialIndex, features);
            return;
        }
    }

    BindFeaturePagerPointers();

    const bool doTriangulate = forceTriangulateAll || (features.size() <= 512 && !pager);
    const keel::StringRef layerHint = !tableName.empty() ? tableName.as_ref() : name.as_ref();
    keel::StaticArray<keel::usize, 16> classCounts{};
    keel::u32 activeClassMask = 0;

    for (keel::usize i = 0; i < features.size(); ++i) {
        GeoFeature& feat = features[i];
        const keel::u64 lowId = (isWindowStreamed && feat.sqliteRowId > 0)
            ? static_cast<keel::u64>(static_cast<keel::u32>(feat.sqliteRowId))
            : static_cast<keel::u64>(i + 1);
        const keel::u64 expectedUid = (layerUid << 32) | lowId;
        if (!isWindowStreamed || feat.sqliteRowId <= 0 || feat.uid != expectedUid || feat.id.empty()) {
            feat.uid = expectedUid;
            if (isWindowStreamed && feat.sqliteRowId > 0) {
                char idBuf[40];
                feat.id.assign(keel::format_buf(idBuf, "L%llu_R%lld",
                                                static_cast<unsigned long long>(layerUid),
                                                static_cast<long long>(feat.sqliteRowId)));
            } else {
                char idBuf[40];
                feat.id.assign(keel::format_buf(idBuf, "L%llu_F%zu",
                                                static_cast<unsigned long long>(layerUid), i));
            }
        }
        if (feat.displayId.empty()) {
            char dispBuf[24];
            feat.displayId.assign(keel::format_buf(dispBuf, "%llu", static_cast<unsigned long long>(lowId)));
        }

        if (doTriangulate && feat.geometryLoaded) {
            for (GeoPolygon& poly : feat.polygons) {
                if (poly.triangles.empty() && poly.exteriorRing.size() >= 3) {
                    PolygonTriangulator::TriangulatePolygon(poly);
                }
            }
        }

        if (!feat.hasValidBounds || (feat.geometryLoaded && !feat.polygons.empty() && feat.polygons[0].normExterior.empty()) ||
            (feat.geometryLoaded && !feat.lines.empty() && feat.normLines.empty()) ||
            (feat.geometryLoaded && !feat.points.empty() && feat.normPoints.empty())) {
            feat.RecomputeBoundsAndCentroid();
        }
        if (feat.name.empty()) {
            feat.ResolveDisplayName(i, layerHint);
        }

        const keel::u8 clsIdx = static_cast<keel::u8>(feat.chartClass);
        if (clsIdx < classCounts.size()) {
            ++classCounts[clsIdx];
            activeClassMask |= (1u << clsIdx);
        }

        if (feat.hasValidBounds && !preserveStreamMetadata) {
            SrsTransform::UnionBounds(bounds, hasValidBounds, feat.bounds);
            UnionNormBounds(normBounds, normInit, feat.normBounds);
        }

        if (feat.HasPolygons()) {
            ++polygonFeatureCount;
        } else if (feat.HasLines()) {
            ++lineFeatureCount;
        } else if (!feat.points.empty() || IsPointType(feat.type)) {
            ++pointFeatureCount;
        }

        totalVertices += feat.VertexCount();
        totalTriangles += feat.TriangleCount();
    }

    if (activeClassMask != 0u) {
        chartClassMask = preserveStreamMetadata ? (savedClassMask | activeClassMask) : activeClassMask;
        keel::usize bestCount = 0;
        ChartFeatureClass bestClass = ChartFeatureClass::Generic;
        for (keel::usize c = 1; c < classCounts.size(); ++c) {
            if (classCounts[c] > bestCount) {
                bestCount = classCounts[c];
                bestClass = static_cast<ChartFeatureClass>(c);
            }
        }
        dominantChartClass = bestClass;
    } else if (preserveStreamMetadata && savedClassMask != 0u) {
        chartClassMask = savedClassMask;
        dominantChartClass = savedDominantClass;
    } else {
        dominantChartClass = SrsTransform::ClassifyFeatureCode({}, GeometryType::Point, layerHint);
        chartClassMask = (dominantChartClass != ChartFeatureClass::Generic)
            ? (1u << static_cast<keel::u8>(dominantChartClass))
            : 0u;
    }

    if (preserveStreamMetadata) {
        bounds = savedBounds;
        normBounds = savedNormBounds;
        hasValidBounds = true;
        if (polygonFeatureCount == 0 && savedPolyCount > 0 && lineFeatureCount == 0 && pointFeatureCount == 0) {
            polygonFeatureCount = savedPolyCount;
        }
        if (lineFeatureCount == 0 && savedLineCount > 0 && polygonFeatureCount == 0 && pointFeatureCount == 0) {
            lineFeatureCount = savedLineCount;
        }
        if (pointFeatureCount == 0 && savedPointCount > 0 && polygonFeatureCount == 0 && lineFeatureCount == 0) {
            pointFeatureCount = savedPointCount;
        }
    }

    ++windowGeneration;
    BuildFeatureSpatialIndex(spatialIndex, features);
}

bool GeoLayer::ApplyWindowSnapshot(SpatialWindowSnapshot&& snapshot) noexcept {
    if (!snapshot.changed) {
        return false;
    }
    const keel::usize savedPointCount = pointFeatureCount;
    const keel::usize savedLineCount = lineFeatureCount;
    const keel::usize savedPolyCount = polygonFeatureCount;
    const ChartFeatureClass savedDominantClass = dominantChartClass;
    const keel::u32 savedClassMask = chartClassMask;

    features = keel::move(snapshot.features);
    spatialIndex = keel::move(snapshot.spatialIndex);
    pointFeatureCount = snapshot.pointFeatureCount;
    lineFeatureCount = snapshot.lineFeatureCount;
    polygonFeatureCount = snapshot.polygonFeatureCount;
    totalVertices = snapshot.totalVertices;
    totalTriangles = 0;

    if (polygonFeatureCount == 0 && savedPolyCount > 0 && lineFeatureCount == 0 && pointFeatureCount == 0) {
        polygonFeatureCount = savedPolyCount;
    }
    if (lineFeatureCount == 0 && savedLineCount > 0 && polygonFeatureCount == 0 && pointFeatureCount == 0) {
        lineFeatureCount = savedLineCount;
    }
    if (pointFeatureCount == 0 && savedPointCount > 0 && polygonFeatureCount == 0 && lineFeatureCount == 0) {
        pointFeatureCount = savedPointCount;
    }

    if (snapshot.dominantChartClass != ChartFeatureClass::Generic) {
        dominantChartClass = snapshot.dominantChartClass;
    } else if (savedDominantClass != ChartFeatureClass::Generic) {
        dominantChartClass = savedDominantClass;
    }
    chartClassMask = savedClassMask | snapshot.chartClassMask;
    ++windowGeneration;
    return true;
}

keel::usize GeoLayer::EvictAllPagedPayloads() const noexcept {
    keel::usize freed = 0;
    for (const GeoFeature& feat : features) {
        freed += feat.UnloadGeometryPayload();
        freed += feat.UnloadPropertiesPayload();
    }
    if (pager) {
        freed += pager->EvictChunksToBudget(0, ~keel::u64{0});
        pager->CloseIdleConnection();
    }
    return freed;
}

guikit::GeoCoord SrsTransform::ToWgs84(double x, double y, keel::i32 srsId) noexcept {
    constexpr double kPi = guikit::MapGrid::kPi;

    // 1. WGS84 / UTM Northern Hemisphere (EPSG:32601 .. EPSG:32660)
    if (srsId >= 32601 && srsId <= 32660) {
        const keel::i32 zone = srsId - 32600;
        const guikit::TransverseMercatorSpec spec{
            .ellipsoid = guikit::EllipsoidSpec::Wgs84(),
            .datumToWgs84 = guikit::Helmert7Spec::Identity(),
            .centralMeridianDeg = static_cast<double>(zone) * 6.0 - 183.0,
            .scaleFactor = 0.9996,
            .falseEasting = 500000.0,
            .falseNorthing = 0.0
        };
        return guikit::MapGrid::TransverseMercatorInverse(x, y, spec);
    }

    // 2. WGS84 / UTM Southern Hemisphere (EPSG:32701 .. EPSG:32760)
    if (srsId >= 32701 && srsId <= 32760) {
        const keel::i32 zone = srsId - 32700;
        const guikit::TransverseMercatorSpec spec{
            .ellipsoid = guikit::EllipsoidSpec::Wgs84(),
            .datumToWgs84 = guikit::Helmert7Spec::Identity(),
            .centralMeridianDeg = static_cast<double>(zone) * 6.0 - 183.0,
            .scaleFactor = 0.9996,
            .falseEasting = 500000.0,
            .falseNorthing = 10000000.0
        };
        return guikit::MapGrid::TransverseMercatorInverse(x, y, spec);
    }

    // 3. GSK-2011 / Gauss-Kruger Zones 1..60 (EPSG:20901 .. EPSG:20960)
    if (srsId >= 20901 && srsId <= 20960) {
        const keel::i32 zone = srsId - 20900;
        const double fe = (x >= 1000000.0) ? (static_cast<double>(zone) * 1000000.0 + 500000.0) : 500000.0;
        const guikit::TransverseMercatorSpec spec{
            .ellipsoid = guikit::EllipsoidSpec::Gsk2011(),
            .datumToWgs84 = guikit::Helmert7Spec::Identity(),
            .centralMeridianDeg = static_cast<double>(zone) * 6.0 - 3.0,
            .scaleFactor = 1.0,
            .falseEasting = fe,
            .falseNorthing = 0.0
        };
        return guikit::MapGrid::TransverseMercatorInverse(x, y, spec);
    }

    // 4. Pulkovo 1942 (SK-42) / Gauss-Kruger Zones 1..32 (EPSG:28401..28432 with zone prefix, EPSG:28461..28492 without)
    if ((srsId >= 28401 && srsId <= 28432) || (srsId >= 28461 && srsId <= 28492)) {
        const keel::i32 zone = (srsId >= 28461) ? (srsId - 28460) : (srsId - 28400);
        const double fe = (srsId < 28461 && x >= 1000000.0)
            ? (static_cast<double>(zone) * 1000000.0 + 500000.0)
            : 500000.0;
        const guikit::TransverseMercatorSpec spec{
            .ellipsoid = guikit::EllipsoidSpec::Krassowsky1940(),
            .datumToWgs84 = guikit::Helmert7Spec::Sk42ToWgs84(),
            .centralMeridianDeg = static_cast<double>(zone) * 6.0 - 3.0,
            .scaleFactor = 1.0,
            .falseEasting = fe,
            .falseNorthing = 0.0
        };
        return guikit::MapGrid::TransverseMercatorInverse(x, y, spec);
    }

    // 5. Pulkovo 1995 (SK-95) / Gauss-Kruger Zones 1..32 (EPSG:20001..20032 with zone prefix, EPSG:20061..20092 without)
    if ((srsId >= 20001 && srsId <= 20032) || (srsId >= 20061 && srsId <= 20092)) {
        const keel::i32 zone = (srsId >= 20061) ? (srsId - 20060) : (srsId - 20000);
        const double fe = (srsId < 20061 && x >= 1000000.0)
            ? (static_cast<double>(zone) * 1000000.0 + 500000.0)
            : 500000.0;
        const guikit::TransverseMercatorSpec spec{
            .ellipsoid = guikit::EllipsoidSpec::Krassowsky1940(),
            .datumToWgs84 = guikit::Helmert7Spec::Sk95ToWgs84(),
            .centralMeridianDeg = static_cast<double>(zone) * 6.0 - 3.0,
            .scaleFactor = 1.0,
            .falseEasting = fe,
            .falseNorthing = 0.0
        };
        return guikit::MapGrid::TransverseMercatorInverse(x, y, spec);
    }

    // 6. Polar Stereographic: EPSG:3995 (Arctic 71°N) & EPSG:3031 (Antarctic 71°S)
    if (srsId == 3995) {
        return guikit::MapGrid::PolarStereographicInverse(x, y, /*northPole=*/true);
    }
    if (srsId == 3031) {
        return guikit::MapGrid::PolarStereographicInverse(x, y, /*northPole=*/false);
    }

    // 7. Geographic Datums: Pulkovo 1942 (EPSG:4284), SK-95 (EPSG:4200), PZ-90.11 (EPSG:7679), GSK-2011 (EPSG:7683)
    if (srsId == 4284) {
        return guikit::MapGrid::TransformDatumHelmert(
            {y, x}, guikit::EllipsoidSpec::Krassowsky1940(), guikit::EllipsoidSpec::Wgs84(),
            guikit::Helmert7Spec::Sk42ToWgs84(), /*inverse=*/false
        );
    }
    if (srsId == 4200) {
        return guikit::MapGrid::TransformDatumHelmert(
            {y, x}, guikit::EllipsoidSpec::Krassowsky1940(), guikit::EllipsoidSpec::Wgs84(),
            guikit::Helmert7Spec::Sk95ToWgs84(), /*inverse=*/false
        );
    }
    if (srsId == 7679) {
        return guikit::MapGrid::TransformDatumHelmert(
            {y, x}, guikit::EllipsoidSpec::Pz90_11(), guikit::EllipsoidSpec::Wgs84(),
            guikit::Helmert7Spec::Pz90ToWgs84(), /*inverse=*/false
        );
    }

    const bool looksLikeMeters = (std::abs(x) > 180.0001 || std::abs(y) > 90.0001);

    if (srsId == 3857 || srsId == 900913 || ((srsId == 4326 || srsId == 7683) && looksLikeMeters)) {
        constexpr double R = guikit::MapGrid::kEarthRadiusMeters;
        const double lon = (x / R) * (180.0 / kPi);
        const double lat = (2.0 * std::atan(std::exp(y / R)) - 0.5 * kPi) * (180.0 / kPi);
        return {
            guikit::MapGrid::ClampLat(lat, guikit::MapCrs::EPSG4326_PlateCarree),
            guikit::MapGrid::ClampLon(lon)
        };
    }

    if (srsId == 3395) {
        constexpr double R = guikit::MapGrid::kEarthRadiusMeters;
        constexpr double e = guikit::MapGrid::kWgs84Eccentricity;
        const double lon = (x / R) * (180.0 / kPi);
        const double ts = std::exp(-y / R);
        double phi = 0.5 * kPi - 2.0 * std::atan(ts);
        for (keel::i32 iter = 0; iter < 6; ++iter) {
            const double esin = e * std::sin(phi);
            const double con = std::pow((1.0 - esin) / (1.0 + esin), 0.5 * e);
            phi = 0.5 * kPi - 2.0 * std::atan(ts * con);
        }
        const double lat = phi * (180.0 / kPi);
        return {
            guikit::MapGrid::ClampLat(lat, guikit::MapCrs::EPSG4326_PlateCarree),
            guikit::MapGrid::ClampLon(lon)
        };
    }

    return {
        guikit::MapGrid::ClampLat(y, guikit::MapCrs::EPSG4326_PlateCarree),
        guikit::MapGrid::ClampLon(x)
    };
}

guikit::GeoPlanar SrsTransform::FromWgs84(guikit::GeoCoord coord, keel::i32 srsId) noexcept {
    constexpr double kPi = guikit::MapGrid::kPi;

    if (srsId == 4326 || srsId == 7683) {
        return {
            guikit::MapGrid::ClampLon(coord.lon),
            guikit::MapGrid::ClampLat(coord.lat, guikit::MapCrs::EPSG4326_PlateCarree)
        };
    }
    if (srsId == 3857 || srsId == 900913) {
        return Wgs84ToWebMercatorMeters(coord);
    }
    if (srsId == 3395) {
        constexpr double R = guikit::MapGrid::kEarthRadiusMeters;
        constexpr double e = guikit::MapGrid::kWgs84Eccentricity;
        const double lonRad = guikit::MapGrid::ClampLon(coord.lon) * (kPi / 180.0);
        const double phi = guikit::MapGrid::ClampLat(coord.lat, guikit::MapCrs::EPSG3395_WorldMercator) * (kPi / 180.0);
        const double eSin = e * std::sin(phi);
        const double cf = std::pow((1.0 - eSin) / (1.0 + eSin), 0.5 * e);
        return {
            R * lonRad,
            R * std::log(std::tan(0.25 * kPi + 0.5 * phi) * cf)
        };
    }
    if (srsId >= 32601 && srsId <= 32660) {
        const keel::i32 zone = srsId - 32600;
        const guikit::TransverseMercatorSpec spec{
            .ellipsoid = guikit::EllipsoidSpec::Wgs84(),
            .datumToWgs84 = guikit::Helmert7Spec::Identity(),
            .centralMeridianDeg = static_cast<double>(zone) * 6.0 - 183.0,
            .scaleFactor = 0.9996,
            .falseEasting = 500000.0,
            .falseNorthing = 0.0
        };
        return guikit::MapGrid::TransverseMercatorForward(coord, spec);
    }
    if (srsId >= 32701 && srsId <= 32760) {
        const keel::i32 zone = srsId - 32700;
        const guikit::TransverseMercatorSpec spec{
            .ellipsoid = guikit::EllipsoidSpec::Wgs84(),
            .datumToWgs84 = guikit::Helmert7Spec::Identity(),
            .centralMeridianDeg = static_cast<double>(zone) * 6.0 - 183.0,
            .scaleFactor = 0.9996,
            .falseEasting = 500000.0,
            .falseNorthing = 10000000.0
        };
        return guikit::MapGrid::TransverseMercatorForward(coord, spec);
    }
    if (srsId >= 20901 && srsId <= 20960) {
        const keel::i32 zone = srsId - 20900;
        const guikit::TransverseMercatorSpec spec{
            .ellipsoid = guikit::EllipsoidSpec::Gsk2011(),
            .datumToWgs84 = guikit::Helmert7Spec::Identity(),
            .centralMeridianDeg = static_cast<double>(zone) * 6.0 - 3.0,
            .scaleFactor = 1.0,
            .falseEasting = static_cast<double>(zone) * 1000000.0 + 500000.0,
            .falseNorthing = 0.0
        };
        return guikit::MapGrid::TransverseMercatorForward(coord, spec);
    }
    if ((srsId >= 28401 && srsId <= 28432) || (srsId >= 28461 && srsId <= 28492)) {
        const keel::i32 zone = (srsId >= 28461) ? (srsId - 28460) : (srsId - 28400);
        const double fe = (srsId >= 28461) ? 500000.0 : (static_cast<double>(zone) * 1000000.0 + 500000.0);
        const guikit::TransverseMercatorSpec spec{
            .ellipsoid = guikit::EllipsoidSpec::Krassowsky1940(),
            .datumToWgs84 = guikit::Helmert7Spec::Sk42ToWgs84(),
            .centralMeridianDeg = static_cast<double>(zone) * 6.0 - 3.0,
            .scaleFactor = 1.0,
            .falseEasting = fe,
            .falseNorthing = 0.0
        };
        return guikit::MapGrid::TransverseMercatorForward(coord, spec);
    }
    if ((srsId >= 20001 && srsId <= 20032) || (srsId >= 20061 && srsId <= 20092)) {
        const keel::i32 zone = (srsId >= 20061) ? (srsId - 20060) : (srsId - 20000);
        const double fe = (srsId >= 20061) ? 500000.0 : (static_cast<double>(zone) * 1000000.0 + 500000.0);
        const guikit::TransverseMercatorSpec spec{
            .ellipsoid = guikit::EllipsoidSpec::Krassowsky1940(),
            .datumToWgs84 = guikit::Helmert7Spec::Sk95ToWgs84(),
            .centralMeridianDeg = static_cast<double>(zone) * 6.0 - 3.0,
            .scaleFactor = 1.0,
            .falseEasting = fe,
            .falseNorthing = 0.0
        };
        return guikit::MapGrid::TransverseMercatorForward(coord, spec);
    }
    if (srsId == 3995) {
        return guikit::MapGrid::PolarStereographicForward(coord, /*northPole=*/true);
    }
    if (srsId == 3031) {
        return guikit::MapGrid::PolarStereographicForward(coord, /*northPole=*/false);
    }
    if (srsId == 4284) {
        const guikit::GeoCoord sk42 = guikit::MapGrid::TransformDatumHelmert(
            coord, guikit::EllipsoidSpec::Wgs84(), guikit::EllipsoidSpec::Krassowsky1940(),
            guikit::Helmert7Spec::Sk42ToWgs84(), /*inverse=*/true
        );
        return {sk42.lon, sk42.lat};
    }
    if (srsId == 4200) {
        const guikit::GeoCoord sk95 = guikit::MapGrid::TransformDatumHelmert(
            coord, guikit::EllipsoidSpec::Wgs84(), guikit::EllipsoidSpec::Krassowsky1940(),
            guikit::Helmert7Spec::Sk95ToWgs84(), /*inverse=*/true
        );
        return {sk95.lon, sk95.lat};
    }
    if (srsId == 7679) {
        const guikit::GeoCoord pz90 = guikit::MapGrid::TransformDatumHelmert(
            coord, guikit::EllipsoidSpec::Wgs84(), guikit::EllipsoidSpec::Pz90_11(),
            guikit::Helmert7Spec::Pz90ToWgs84(), /*inverse=*/true
        );
        return {pz90.lon, pz90.lat};
    }
    return Wgs84ToWebMercatorMeters(coord);
}

guikit::GeoPlanar SrsTransform::Wgs84ToWebMercatorMeters(guikit::GeoCoord coord) noexcept {
    constexpr double kPi = guikit::MapGrid::kPi;
    constexpr double R = guikit::MapGrid::kEarthRadiusMeters;
    const double lonRad = guikit::MapGrid::ClampLon(coord.lon) * (kPi / 180.0);
    const double latRad = guikit::MapGrid::ClampLat(coord.lat, guikit::MapCrs::EPSG3857_WebMercator) * (kPi / 180.0);
    const double x = R * lonRad;
    const double y = R * std::log(std::tan(0.25 * kPi + 0.5 * latRad));
    return {x, y};
}

void SrsTransform::ExpandBounds(guikit::GeoBounds& bounds, bool& initialized, guikit::GeoCoord pt) noexcept {
    if (!initialized) {
        bounds.nw.lat = pt.lat;
        bounds.nw.lon = pt.lon;
        bounds.se.lat = pt.lat;
        bounds.se.lon = pt.lon;
        initialized = true;
    } else {
        bounds.nw.lat = keel::max(bounds.nw.lat, pt.lat);
        bounds.nw.lon = keel::min(bounds.nw.lon, pt.lon);
        bounds.se.lat = keel::min(bounds.se.lat, pt.lat);
        bounds.se.lon = keel::max(bounds.se.lon, pt.lon);
    }
}

void SrsTransform::UnionBounds(guikit::GeoBounds& bounds, bool& initialized, const guikit::GeoBounds& other) noexcept {
    ExpandBounds(bounds, initialized, other.nw);
    ExpandBounds(bounds, initialized, other.se);
}

bool SrsTransform::BoundsIntersect(const guikit::GeoBounds& a, const guikit::GeoBounds& b) noexcept {
    const double aMinLon = keel::min(a.nw.lon, a.se.lon);
    const double aMaxLon = keel::max(a.nw.lon, a.se.lon);
    const double aMinLat = keel::min(a.se.lat, a.nw.lat);
    const double aMaxLat = keel::max(a.se.lat, a.nw.lat);

    const double bMinLon = keel::min(b.nw.lon, b.se.lon);
    const double bMaxLon = keel::max(b.nw.lon, b.se.lon);
    const double bMinLat = keel::min(b.se.lat, b.nw.lat);
    const double bMaxLat = keel::max(b.se.lat, b.nw.lat);

    if (aMaxLon < bMinLon || aMinLon > bMaxLon) return false;
    if (aMaxLat < bMinLat || aMinLat > bMaxLat) return false;
    return true;
}

guikit::GeoCoord SrsTransform::BoundsCenter(const guikit::GeoBounds& bounds) noexcept {
    return {
        0.5 * (bounds.nw.lat + bounds.se.lat),
        0.5 * (bounds.nw.lon + bounds.se.lon)
    };
}

float SrsTransform::EstimateFitTileZoom(const guikit::GeoBounds& bounds) noexcept {
    const double dLon = keel::max(1e-5, std::abs(bounds.se.lon - bounds.nw.lon));
    const double dLat = keel::max(1e-5, std::abs(bounds.nw.lat - bounds.se.lat));
    const double maxSpanDeg = keel::max(dLon, dLat * 1.45);

    if (maxSpanDeg <= 0.005) {
        return 14.5f;
    }
    const double z = std::log2(360.0 / maxSpanDeg) - 0.35;
    return static_cast<float>(keel::clamp(z, 1.5, 16.5));
}

guikit::Color SrsTransform::DefaultLayerColor(keel::usize paletteIndex) noexcept {
    static constexpr keel::StaticArray<guikit::Color, 8> kPalette = {{
        guikit::Color(56, 189, 248, 255),   // Sky Cyan
        guikit::Color(16, 185, 129, 255),   // Emerald Green
        guikit::Color(245, 158, 11, 255),   // Amber Gold
        guikit::Color(232, 121, 249, 255),  // Chart Magenta
        guikit::Color(99, 102, 241, 255),   // Indigo
        guikit::Color(244, 63, 94, 255),    // Coral Rose
        guikit::Color(45, 212, 191, 255),   // Nautical Teal
        guikit::Color(168, 85, 247, 255)    // Royal Purple
    }};
    return kPalette[paletteIndex % kPalette.size()];
}

ChartFeatureClass SrsTransform::ClassifyFeatureCode(
    keel::StringRef code,
    GeometryType geomType,
    keel::StringRef layerHint
) noexcept {
    const keel::StringRef effectiveCode = !code.empty() ? code : layerHint;
    if (!effectiveCode.empty()) {
        if (EqualsIgnoreCaseAscii(effectiveCode, "M_COVR") || EqualsIgnoreCaseAscii(effectiveCode, "M_NPUB") ||
            EqualsIgnoreCaseAscii(effectiveCode, "M_NSYS") || EqualsIgnoreCaseAscii(effectiveCode, "M_QUAL")) {
            return ChartFeatureClass::CoverageMask;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "DEPARE") || EqualsIgnoreCaseAscii(effectiveCode, "DRGARE")) {
            return IsLinearType(geomType) ? ChartFeatureClass::DepthContour : ChartFeatureClass::DepthArea;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "DEPCNT")) {
            return ChartFeatureClass::DepthContour;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "SOUNDG")) {
            return ChartFeatureClass::Sounding;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "LNDARE") || EqualsIgnoreCaseAscii(effectiveCode, "LNDRGN") ||
            EqualsIgnoreCaseAscii(effectiveCode, "SLOGRD") || EqualsIgnoreCaseAscii(effectiveCode, "LNDELV") ||
            EqualsIgnoreCaseAscii(effectiveCode, "VEGATN")) {
            return ChartFeatureClass::LandArea;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "COALNE") || EqualsIgnoreCaseAscii(effectiveCode, "SLOTOP")) {
            return ChartFeatureClass::Coastline;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "FAIRWY") || EqualsIgnoreCaseAscii(effectiveCode, "DWRTPT") ||
            EqualsIgnoreCaseAscii(effectiveCode, "TSSLPT") || EqualsIgnoreCaseAscii(effectiveCode, "TSSCRS") ||
            EqualsIgnoreCaseAscii(effectiveCode, "TWRTPT") || EqualsIgnoreCaseAscii(effectiveCode, "SUBTLN")) {
            return ChartFeatureClass::FairwayArea;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "SEAARE") || EqualsIgnoreCaseAscii(effectiveCode, "SBDARE")) {
            return IsPointType(geomType) ? ChartFeatureClass::TextLabel : ChartFeatureClass::SeabedOrMarineArea;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "HRBARE") || EqualsIgnoreCaseAscii(effectiveCode, "PRCARE") ||
            EqualsIgnoreCaseAscii(effectiveCode, "ACHARE") || EqualsIgnoreCaseAscii(effectiveCode, "ACHBRT") ||
            EqualsIgnoreCaseAscii(effectiveCode, "TSEZNE") || EqualsIgnoreCaseAscii(effectiveCode, "RESARE") ||
            EqualsIgnoreCaseAscii(effectiveCode, "CTNARE") || EqualsIgnoreCaseAscii(effectiveCode, "MIPARE") ||
            EqualsIgnoreCaseAscii(effectiveCode, "SPLARE") || EqualsIgnoreCaseAscii(effectiveCode, "FSHZNE") ||
            EqualsIgnoreCaseAscii(effectiveCode, "FSHGRD") || EqualsIgnoreCaseAscii(effectiveCode, "TESARE") ||
            EqualsIgnoreCaseAscii(effectiveCode, "EXEZNE") || EqualsIgnoreCaseAscii(effectiveCode, "CONZNE") ||
            EqualsIgnoreCaseAscii(effectiveCode, "LAKARE") || EqualsIgnoreCaseAscii(effectiveCode, "OSPARE") ||
            EqualsIgnoreCaseAscii(effectiveCode, "ICNARE") || EqualsIgnoreCaseAscii(effectiveCode, "MARCUL") ||
            EqualsIgnoreCaseAscii(effectiveCode, "SWPARE")) {
            return ChartFeatureClass::SeabedOrMarineArea;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "BUAARE") || EqualsIgnoreCaseAscii(effectiveCode, "BUISGL") ||
            EqualsIgnoreCaseAscii(effectiveCode, "BRIDGE") || EqualsIgnoreCaseAscii(effectiveCode, "BERTHS") ||
            EqualsIgnoreCaseAscii(effectiveCode, "SLCONS") || EqualsIgnoreCaseAscii(effectiveCode, "PONTON") ||
            EqualsIgnoreCaseAscii(effectiveCode, "HRBFAC") || EqualsIgnoreCaseAscii(effectiveCode, "ROADWY") ||
            EqualsIgnoreCaseAscii(effectiveCode, "RIVERS") || EqualsIgnoreCaseAscii(effectiveCode, "CANALS") ||
            EqualsIgnoreCaseAscii(effectiveCode, "DYKCON") || EqualsIgnoreCaseAscii(effectiveCode, "DAMCON") ||
            EqualsIgnoreCaseAscii(effectiveCode, "PILPNT") || EqualsIgnoreCaseAscii(effectiveCode, "FSHFAC") ||
            EqualsIgnoreCaseAscii(effectiveCode, "FLODOC") || EqualsIgnoreCaseAscii(effectiveCode, "DRYDOC") ||
            EqualsIgnoreCaseAscii(effectiveCode, "HULKES") || EqualsIgnoreCaseAscii(effectiveCode, "CRANES") ||
            EqualsIgnoreCaseAscii(effectiveCode, "GATCON") || EqualsIgnoreCaseAscii(effectiveCode, "LOKBSN") ||
            EqualsIgnoreCaseAscii(effectiveCode, "FORSTC") || EqualsIgnoreCaseAscii(effectiveCode, "SILTNK") ||
            EqualsIgnoreCaseAscii(effectiveCode, "RUNWAY") || EqualsIgnoreCaseAscii(effectiveCode, "AIRARE") ||
            EqualsIgnoreCaseAscii(effectiveCode, "RAILWY") || EqualsIgnoreCaseAscii(effectiveCode, "TUNNEL") ||
            EqualsIgnoreCaseAscii(effectiveCode, "CAUSWY") || EqualsIgnoreCaseAscii(effectiveCode, "PRDARE") ||
            EqualsIgnoreCaseAscii(effectiveCode, "SMCFRD")) {
            return ChartFeatureClass::BuiltUpOrStructure;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "MORFAC")) {
            return IsPointType(geomType) ? ChartFeatureClass::BuoyOrBeacon : ChartFeatureClass::BuiltUpOrStructure;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "CBLSUB") || EqualsIgnoreCaseAscii(effectiveCode, "PIPSOL") ||
            EqualsIgnoreCaseAscii(effectiveCode, "CBLARE") || EqualsIgnoreCaseAscii(effectiveCode, "PIPARE") ||
            EqualsIgnoreCaseAscii(effectiveCode, "CBLOHD") || EqualsIgnoreCaseAscii(effectiveCode, "PIPOHD")) {
            return ChartFeatureClass::SubmarineCablePipe;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "NAVLNE") || EqualsIgnoreCaseAscii(effectiveCode, "RECTRC") ||
            EqualsIgnoreCaseAscii(effectiveCode, "RESLINE") || EqualsIgnoreCaseAscii(effectiveCode, "STSLIN") ||
            EqualsIgnoreCaseAscii(effectiveCode, "RCRTCL") || EqualsIgnoreCaseAscii(effectiveCode, "FERYRT") ||
            EqualsIgnoreCaseAscii(effectiveCode, "TSSBND") || EqualsIgnoreCaseAscii(effectiveCode, "TSSRON") ||
            EqualsIgnoreCaseAscii(effectiveCode, "MAGVAR") || EqualsIgnoreCaseAscii(effectiveCode, "CURENT")) {
            return ChartFeatureClass::NavigationLine;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "UWTROC") || EqualsIgnoreCaseAscii(effectiveCode, "OBSTRN") ||
            EqualsIgnoreCaseAscii(effectiveCode, "WRECKS") || EqualsIgnoreCaseAscii(effectiveCode, "DMPGRD") ||
            EqualsIgnoreCaseAscii(effectiveCode, "WATTUR") || EqualsIgnoreCaseAscii(effectiveCode, "WEDKLP")) {
            return ChartFeatureClass::UnderwaterHazard;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "BOYLAT") || EqualsIgnoreCaseAscii(effectiveCode, "BOYCAR") ||
            EqualsIgnoreCaseAscii(effectiveCode, "BOYSPP") || EqualsIgnoreCaseAscii(effectiveCode, "BOYSAW") ||
            EqualsIgnoreCaseAscii(effectiveCode, "BOYISD") || EqualsIgnoreCaseAscii(effectiveCode, "BOYINB") ||
            EqualsIgnoreCaseAscii(effectiveCode, "BCNCAR") || EqualsIgnoreCaseAscii(effectiveCode, "BCNLAT") ||
            EqualsIgnoreCaseAscii(effectiveCode, "BCNSPP") || EqualsIgnoreCaseAscii(effectiveCode, "BCNSAW") ||
            EqualsIgnoreCaseAscii(effectiveCode, "BCNISD") || EqualsIgnoreCaseAscii(effectiveCode, "LIGHTS") ||
            EqualsIgnoreCaseAscii(effectiveCode, "LNDMRK") || EqualsIgnoreCaseAscii(effectiveCode, "DAYMAR") ||
            EqualsIgnoreCaseAscii(effectiveCode, "TOPMAR") || EqualsIgnoreCaseAscii(effectiveCode, "PILBOP") ||
            EqualsIgnoreCaseAscii(effectiveCode, "SISTAT") || EqualsIgnoreCaseAscii(effectiveCode, "SISTAW") ||
            EqualsIgnoreCaseAscii(effectiveCode, "OFSPLF") || EqualsIgnoreCaseAscii(effectiveCode, "DISMAR") ||
            EqualsIgnoreCaseAscii(effectiveCode, "FOGSIG") || EqualsIgnoreCaseAscii(effectiveCode, "RTPBCN") ||
            EqualsIgnoreCaseAscii(effectiveCode, "RADSTA") || EqualsIgnoreCaseAscii(effectiveCode, "RDOSTA") ||
            EqualsIgnoreCaseAscii(effectiveCode, "RSCSTA") || EqualsIgnoreCaseAscii(effectiveCode, "LITFLT") ||
            EqualsIgnoreCaseAscii(effectiveCode, "LITVES")) {
            return ChartFeatureClass::BuoyOrBeacon;
        }
        if (EqualsIgnoreCaseAscii(effectiveCode, "TEXT") || EqualsIgnoreCaseAscii(effectiveCode, "$TEXTS")) {
            return ChartFeatureClass::TextLabel;
        }
    }

    if (!layerHint.empty()) {
        if (EqualsIgnoreCaseAscii(layerHint, "depth_areas")) return ChartFeatureClass::DepthArea;
        if (EqualsIgnoreCaseAscii(layerHint, "depth_contours")) return ChartFeatureClass::DepthContour;
        if (EqualsIgnoreCaseAscii(layerHint, "soundings")) return ChartFeatureClass::Sounding;
        if (EqualsIgnoreCaseAscii(layerHint, "land_areas")) return ChartFeatureClass::LandArea;
        if (EqualsIgnoreCaseAscii(layerHint, "coastline")) return ChartFeatureClass::Coastline;
        if (EqualsIgnoreCaseAscii(layerHint, "navigation_lines")) return ChartFeatureClass::NavigationLine;
        if (EqualsIgnoreCaseAscii(layerHint, "marine_areas")) return ChartFeatureClass::SeabedOrMarineArea;
    }

    return ChartFeatureClass::Generic;
}

const GeoFeature* FindFeatureByUid(
    keel::ArrayRef<GeoLayer> layers,
    keel::u64 uid,
    const GeoLayer** outLayer
) noexcept {
    if (uid == 0) return nullptr;
    const keel::u32 targetLayerUid = static_cast<keel::u32>(uid >> 32);
    const keel::u32 oneBasedIdx = static_cast<keel::u32>(uid & 0xFFFFFFFFu);

    for (const GeoLayer& lyr : layers) {
        if (lyr.uid == targetLayerUid) {
            if (oneBasedIdx > 0 && oneBasedIdx <= lyr.features.size()) {
                const GeoFeature& cand = lyr.features[oneBasedIdx - 1];
                if (cand.uid == uid) {
                    if (outLayer) *outLayer = &lyr;
                    return &cand;
                }
            }
            for (const GeoFeature& f : lyr.features) {
                if (f.uid == uid) {
                    if (outLayer) *outLayer = &lyr;
                    return &f;
                }
            }
        }
    }
    for (const GeoLayer& lyr : layers) {
        for (const GeoFeature& f : lyr.features) {
            if (f.uid == uid) {
                if (outLayer) *outLayer = &lyr;
                return &f;
            }
        }
    }
    return nullptr;
}

// ============================================================================
// Platform-Independent Fallback Cartographic Basemap Tile Provider
// ============================================================================

namespace {

class CartographicFallbackTileProvider final : public IRasterTileProvider {
public:
    [[nodiscard]] keel::StringRef GetSourceId() const noexcept override {
        return "carto_fallback";
    }

    [[nodiscard]] keel::StringRef GetDisplayName() const noexcept override {
        return "Cartographic Basemap Grid";
    }

    [[nodiscard]] keel::Arc<const RasterTileImage> TryGetCachedTile(
        const SlippyTileKey& key
    ) const noexcept override {
        const SlippyTileKey normKey = NormalizeKey(key);
        std::lock_guard<std::mutex> lk(m_mutex);
        auto* cached = m_cache.get(normKey);
        return cached ? *cached : keel::Arc<const RasterTileImage>{};
    }

    [[nodiscard]] keel::Arc<const RasterTileImage> FetchTileBlocking(
        const SlippyTileKey& key
    ) override {
        const SlippyTileKey normKey = NormalizeKey(key);
        if (auto cached = TryGetCachedTile(normKey)) {
            return cached;
        }

        auto tileImg = keel::make_arc<RasterTileImage>();
        tileImg->key = normKey;
        tileImg->width = normKey.tileSizePx;
        tileImg->height = normKey.tileSizePx;

        const keel::u32 sz = normKey.tileSizePx;
        tileImg->rgbaPixels.resize(static_cast<keel::usize>(sz) * sz);

        const bool evenChecker = ((normKey.x + normKey.y) & 1) == 0;
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
                tileImg->rgbaPixels[static_cast<keel::usize>(py) * sz + px] =
                    (static_cast<keel::u32>(255u) << 24u) |
                    (static_cast<keel::u32>(b) << 16u) |
                    (static_cast<keel::u32>(g) << 8u) |
                    static_cast<keel::u32>(r);
            }
        }

        keel::Arc<const RasterTileImage> constImg = keel::move(tileImg);
        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_cache.put(normKey, constImg);
        }
        return constImg;
    }

    void ClearCache() noexcept override {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_cache.clear();
    }

private:
    [[nodiscard]] static SlippyTileKey NormalizeKey(const SlippyTileKey& k) noexcept {
        const keel::i32 z = keel::clamp(k.z, 0, 19);
        const keel::i32 n = 1 << z;
        const keel::i32 x = ((k.x % n) + n) % n;
        const keel::i32 y = keel::clamp(k.y, 0, n - 1);
        const keel::u32 sz = (k.tileSizePx <= 160u) ? 128u : ((k.tileSizePx >= 384u) ? 512u : 256u);
        return SlippyTileKey{.z = z, .x = x, .y = y, .tileSizePx = sz};
    }

    mutable std::mutex m_mutex{};
    mutable keel::LruCache<SlippyTileKey, keel::Arc<const RasterTileImage>, 256, SlippyTileKeyHash> m_cache{};
};

} // namespace

keel::Arc<IRasterTileProvider> CreateCartographicBasemapTileProvider() {
    return keel::make_arc<CartographicFallbackTileProvider>();
}

} // namespace guikit::geo
