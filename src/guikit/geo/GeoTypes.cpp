#include "guikit/geo/GeoTypes.hpp"
#include "guikit/geo/PolygonTriangulator.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <format>
#include <mutex>
#include <numbers>
#include <unordered_map>

namespace guikit::geo {

namespace {

[[nodiscard]] constexpr char LowerChar(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

[[nodiscard]] constexpr char UpperChar(char c) noexcept {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - ('a' - 'A')) : c;
}

[[nodiscard]] bool EqualsIgnoreCaseAscii(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (LowerChar(a[i]) != LowerChar(b[i])) return false;
    }
    return true;
}

[[nodiscard]] std::string ToLowerAscii(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) {
        c = LowerChar(c);
    }
    return out;
}

[[nodiscard]] std::string ToUpperAscii(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) {
        c = UpperChar(c);
    }
    return out;
}

[[nodiscard]] bool ContainsCaseInsensitive(std::string_view haystack, std::string_view needleLower) noexcept {
    if (needleLower.empty()) return true;
    if (haystack.size() < needleLower.size()) return false;
    const size_t limit = haystack.size() - needleLower.size();
    for (size_t i = 0; i <= limit; ++i) {
        bool match = true;
        for (size_t j = 0; j < needleLower.size(); ++j) {
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
        const float minX = std::min(r.x, p.x);
        const float minY = std::min(r.y, p.y);
        const float maxX = std::max(r.Right(), p.x);
        const float maxY = std::max(r.Bottom(), p.y);
        r = guikit::Rect(minX, minY, maxX - minX, maxY - minY);
    }
}

void UnionNormBounds(guikit::Rect& r, bool& init, guikit::Rect other) noexcept {
    ExpandNormBounds(r, init, other.Pos());
    ExpandNormBounds(r, init, {other.Right(), other.Bottom()});
}

// Fast radial + perpendicular vertex decimation for overview LOD rings
void BuildLodRing(
    const std::vector<guikit::Vec2>& src,
    guikit::Rect normBox,
    bool closed,
    std::vector<guikit::Vec2>& outLod
) {
    outLod.clear();
    if (src.size() <= 20) {
        return;
    }
    const float span = std::max(normBox.w, normBox.h);
    const float tol = std::max(1e-7f, span * 0.015f);
    const float tolSq = tol * tol;

    outLod.reserve(std::min<size_t>(src.size(), 64));
    outLod.push_back(src.front());
    guikit::Vec2 lastKept = src.front();

    for (size_t i = 1; i + 1 < src.size(); ++i) {
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

std::string AttributeValue::ToString() const {
    if (std::holds_alternative<std::monostate>(value)) {
        return "null";
    }
    if (const auto* b = std::get_if<bool>(&value)) {
        return *b ? "true" : "false";
    }
    if (const auto* i = std::get_if<int64_t>(&value)) {
        return std::to_string(*i);
    }
    if (const auto* d = std::get_if<double>(&value)) {
        char buf[64]{};
        std::snprintf(buf, sizeof(buf), "%.6g", *d);
        return std::string(buf);
    }
    if (const auto* s = std::get_if<std::string>(&value)) {
        return *s;
    }
    return {};
}

std::string_view AttributeValue::TypeLabel() const noexcept {
    if (std::holds_alternative<std::monostate>(value)) return "null";
    if (std::holds_alternative<bool>(value))           return "bool";
    if (std::holds_alternative<int64_t>(value))        return "int";
    if (std::holds_alternative<double>(value))         return "float";
    if (std::holds_alternative<std::string>(value))    return "str";
    return "unknown";
}

bool AttributeValue::MatchesSearch(std::string_view queryLower) const {
    if (queryLower.empty()) return true;
    if (const auto* s = std::get_if<std::string>(&value)) {
        return ContainsCaseInsensitive(*s, queryLower);
    }
    return ContainsCaseInsensitive(ToString(), queryLower);
}

std::shared_ptr<const DecodedGeometryPayload> IFeaturePager::GetOrLoadGeometryPayload(
    int64_t featureId,
    int defaultSrsId,
    uint64_t accessTick
) const {
    if (auto cached = TryGetCachedGeometryPayload(featureId, accessTick)) {
        return cached;
    }
    GeoFeature tempFeat;
    tempFeat.sqliteRowId = featureId;
    tempFeat.srsId = defaultSrsId;
    tempFeat.geometryLoaded = false;
    if (!LoadFeatureGeometry(featureId, defaultSrsId, tempFeat)) {
        return nullptr;
    }
    return tempFeat.BuildDecodedGeometryPayload();
}

void GeoFeature::ApplyDecodedGeometryPayload(const DecodedGeometryPayload& payload) const {
    auto& mutableSelf = const_cast<GeoFeature&>(*this);
    mutableSelf.type = payload.type;
    if (!payload.points.empty()) {
        points = payload.points;
        normPoints = payload.normPoints;
    }
    lines = payload.lines;
    normLines = payload.normLines;
    normLinesLod = payload.normLinesLod;
    polygons = payload.polygons;
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

std::shared_ptr<const DecodedGeometryPayload> GeoFeature::BuildDecodedGeometryPayload() const {
    auto payload = std::make_shared<DecodedGeometryPayload>();
    payload->featureId = sqliteRowId;
    payload->type = type;
    payload->points = points;
    payload->lines = lines;
    payload->polygons = polygons;
    payload->normPoints = normPoints;
    payload->normLines = normLines;
    payload->normLinesLod = normLinesLod;
    payload->bounds = bounds;
    payload->normBounds = normBounds;
    payload->centroid = centroid;
    payload->normCentroid = normCentroid;
    payload->cachedVertexCount = cachedVertexCount;
    payload->cachedPolygonCount = cachedPolygonCount;
    payload->cachedHoleCount = cachedHoleCount;
    payload->residentHeapBytes = static_cast<uint32_t>(
        std::min<size_t>(RefreshResidentHeapBytes(), std::numeric_limits<uint32_t>::max()));
    return payload;
}

bool GeoFeature::TryEnsureGeometryLoadedNonBlocking(uint64_t accessTick) const noexcept {
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
            sharedGeometry = std::move(cached);
            ApplyDecodedGeometryPayload(*sharedGeometry);
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

bool GeoFeature::EnsureGeometryLoaded(uint64_t accessTick) const {
    if (TryEnsureGeometryLoadedNonBlocking(accessTick)) {
        return true;
    }
    if (auto payload = pager->GetOrLoadGeometryPayload(sqliteRowId, srsId, accessTick)) {
        sharedGeometry = std::move(payload);
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

bool GeoFeature::EnsurePropertiesLoaded(uint64_t accessTick) const {
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

size_t GeoFeature::RefreshResidentHeapBytes() const noexcept {
    size_t bytes = 0;
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
        bytes += sizeof(std::pair<std::string, AttributeValue>) + k.capacity();
        if (const auto* s = std::get_if<std::string>(&v.value)) {
            bytes += s->capacity();
        }
    }
    residentHeapBytes = static_cast<uint32_t>(std::min<size_t>(bytes, std::numeric_limits<uint32_t>::max()));
    return bytes;
}

size_t GeoFeature::UnloadGeometryPayload() const noexcept {
    if (!pager || sqliteRowId <= 0 || (!geometryLoaded && !sharedGeometry) ||
        (polygons.empty() && lines.empty() && !sharedGeometry)) {
        return 0;
    }
    const size_t before = residentHeapBytes;
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
    const size_t after = RefreshResidentHeapBytes();
    return (before > after) ? (before - after) : 0;
}

size_t GeoFeature::UnloadPropertiesPayload() const noexcept {
    if (!pager || sqliteRowId <= 0 || properties.empty()) {
        propertiesLoaded = false;
        return 0;
    }
    const size_t before = residentHeapBytes;
    properties.clear();
    properties.shrink_to_fit();
    propertiesLoaded = false;
    const size_t after = RefreshResidentHeapBytes();
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
    const float minX = std::min(nwNorm.x, seNorm.x);
    const float maxX = std::max(nwNorm.x, seNorm.x);
    const float minY = std::min(nwNorm.y, seNorm.y);
    const float maxY = std::max(nwNorm.y, seNorm.y);
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
    size_t count = 0;
    size_t holeCount = 0;

    normPoints.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) {
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
    for (size_t li = 0; li < lines.size(); ++li) {
        const auto& line = lines[li];
        auto& nLine = normLines[li];
        nLine.resize(line.size());
        guikit::Rect lineNormBox{};
        bool lineBoxInit = false;
        for (size_t pi = 0; pi < line.size(); ++pi) {
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
        for (size_t pi = 0; pi < poly.exteriorRing.size(); ++pi) {
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
        for (size_t hi = 0; hi < poly.interiorRings.size(); ++hi) {
            const auto& hole = poly.interiorRings[hi];
            auto& nHole = poly.normInterior[hi];
            nHole.resize(hole.size());
            for (size_t pi = 0; pi < hole.size(); ++pi) {
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

    cachedVertexCount = static_cast<uint32_t>(count);
    cachedPolygonCount = static_cast<uint32_t>(polygons.size());
    cachedHoleCount = static_cast<uint32_t>(holeCount);

    if (count > 0) {
        centroid = {sumLat / static_cast<double>(count), sumLon / static_cast<double>(count)};
    } else if (hasValidBounds) {
        centroid = SrsTransform::BoundsCenter(bounds);
    }
    normCentroid = guikit::MapGrid::GeoToNormalized(centroid, guikit::MapCrs::EPSG3857_WebMercator);
    (void)RefreshResidentHeapBytes();
}

void GeoFeature::ResolveDisplayName(size_t fallbackIndex, std::string_view layerHint) {
    // Single pass over properties to extract marine metadata & display name without O(K*M) allocations
    std::string fallbackTitle;
    std::string fallbackFeatureType;

    for (const auto& [k, v] : properties) {
        if (v.IsNull()) continue;

        if (EqualsIgnoreCaseAscii(k, "objl") || EqualsIgnoreCaseAscii(k, "feature_code") ||
            EqualsIgnoreCaseAscii(k, "acronym") || EqualsIgnoreCaseAscii(k, "s57_class")) {
            if (featureCode.empty()) featureCode = ToUpperAscii(v.ToString());
        } else if (EqualsIgnoreCaseAscii(k, "valsou") || EqualsIgnoreCaseAscii(k, "depth") ||
                   EqualsIgnoreCaseAscii(k, "depth_m") || EqualsIgnoreCaseAscii(k, "valdco") ||
                   EqualsIgnoreCaseAscii(k, "elevat") || EqualsIgnoreCaseAscii(k, "height")) {
            if (std::isnan(depthMeters)) {
                if (const auto* d = std::get_if<double>(&v.value)) depthMeters = static_cast<float>(*d);
                else if (const auto* i = std::get_if<int64_t>(&v.value)) depthMeters = static_cast<float>(*i);
                else if (const auto* s = std::get_if<std::string>(&v.value)) {
                    float fVal = 0.0f;
                    const auto [ptr, ec] = std::from_chars(s->data(), s->data() + s->size(), fVal);
                    if (ec == std::errc{}) depthMeters = fVal;
                }
            }
        } else if (EqualsIgnoreCaseAscii(k, "drval1") || EqualsIgnoreCaseAscii(k, "min_depth_m") ||
                   EqualsIgnoreCaseAscii(k, "depth_min") || EqualsIgnoreCaseAscii(k, "min_depth")) {
            if (std::isnan(minDepthMeters)) {
                if (const auto* d = std::get_if<double>(&v.value)) minDepthMeters = static_cast<float>(*d);
                else if (const auto* i = std::get_if<int64_t>(&v.value)) minDepthMeters = static_cast<float>(*i);
                else if (const auto* s = std::get_if<std::string>(&v.value)) {
                    float fVal = 0.0f;
                    const auto [ptr, ec] = std::from_chars(s->data(), s->data() + s->size(), fVal);
                    if (ec == std::errc{}) minDepthMeters = fVal;
                }
            }
        } else if (EqualsIgnoreCaseAscii(k, "drval2") || EqualsIgnoreCaseAscii(k, "max_depth_m") ||
                   EqualsIgnoreCaseAscii(k, "depth_max") || EqualsIgnoreCaseAscii(k, "max_depth")) {
            if (std::isnan(maxDepthMeters)) {
                if (const auto* d = std::get_if<double>(&v.value)) maxDepthMeters = static_cast<float>(*d);
                else if (const auto* i = std::get_if<int64_t>(&v.value)) maxDepthMeters = static_cast<float>(*i);
                else if (const auto* s = std::get_if<std::string>(&v.value)) {
                    float fVal = 0.0f;
                    const auto [ptr, ec] = std::from_chars(s->data(), s->data() + s->size(), fVal);
                    if (ec == std::errc{}) maxDepthMeters = fVal;
                }
            }
        } else if (EqualsIgnoreCaseAscii(k, "colour") || EqualsIgnoreCaseAscii(k, "color")) {
            if (lightOrColor.empty()) lightOrColor = v.ToString();
        } else if (EqualsIgnoreCaseAscii(k, "litchr") || EqualsIgnoreCaseAscii(k, "siggrp") ||
                   EqualsIgnoreCaseAscii(k, "sigper") || EqualsIgnoreCaseAscii(k, "valnmr") ||
                   EqualsIgnoreCaseAscii(k, "seamark:light:character")) {
            const std::string s = v.ToString();
            if (!s.empty()) {
                hasLight = true;
                if (lightOrColor.empty()) lightOrColor = s;
            }
        } else if (EqualsIgnoreCaseAscii(k, "nobjnm") || EqualsIgnoreCaseAscii(k, "name_ru") ||
                   EqualsIgnoreCaseAscii(k, "label_ru") || EqualsIgnoreCaseAscii(k, "buoy_name_ru")) {
            if (labelRu.empty()) labelRu = v.ToString();
        } else if (EqualsIgnoreCaseAscii(k, "objnam") || EqualsIgnoreCaseAscii(k, "name") ||
                   EqualsIgnoreCaseAscii(k, "name_en") || EqualsIgnoreCaseAscii(k, "label_en") ||
                   EqualsIgnoreCaseAscii(k, "buoy_name_en")) {
            if (labelEn.empty()) labelEn = v.ToString();
        } else if (EqualsIgnoreCaseAscii(k, "title") || EqualsIgnoreCaseAscii(k, "label") ||
                   EqualsIgnoreCaseAscii(k, "seamark:name") || EqualsIgnoreCaseAscii(k, "port_name") ||
                   EqualsIgnoreCaseAscii(k, "zone_name") || EqualsIgnoreCaseAscii(k, "route_name") ||
                   EqualsIgnoreCaseAscii(k, "natsur") || EqualsIgnoreCaseAscii(k, "inform")) {
            if (fallbackTitle.empty()) fallbackTitle = v.ToString();
        } else if (EqualsIgnoreCaseAscii(k, "feature_type") || EqualsIgnoreCaseAscii(k, "layer")) {
            if (fallbackFeatureType.empty()) fallbackFeatureType = v.ToString();
        }
    }

    if (featureCode.empty() && !fallbackFeatureType.empty() && fallbackFeatureType.size() == 6) {
        featureCode = ToUpperAscii(fallbackFeatureType);
    }
    if (featureCode.empty() && layerHint.size() == 6) {
        featureCode = ToUpperAscii(layerHint);
    }
    if (featureCode == "LIGHTS" || featureCode == "LITFLT" || featureCode == "LITVES") {
        hasLight = true;
    }

    if (chartClass == ChartFeatureClass::Generic) {
        chartClass = SrsTransform::ClassifyFeatureCode(
            featureCode,
            type,
            !fallbackFeatureType.empty() ? std::string_view(fallbackFeatureType) : layerHint
        );
    }

    if (!name.empty()) {
        return;
    }

    if (!labelRu.empty()) {
        name = labelRu;
        return;
    }
    if (!labelEn.empty()) {
        name = labelEn;
        return;
    }
    if (!fallbackTitle.empty()) {
        name = fallbackTitle;
        return;
    }
    if (chartClass == ChartFeatureClass::Sounding && !std::isnan(depthMeters)) {
        char buf[32]{};
        std::snprintf(buf, sizeof(buf), "%.1f m", static_cast<double>(depthMeters));
        name = buf;
        return;
    }
    if ((chartClass == ChartFeatureClass::DepthContour || featureCode == "LNDELV") && !std::isnan(depthMeters)) {
        char buf[32]{};
        std::snprintf(buf, sizeof(buf), "%.4g m", static_cast<double>(depthMeters));
        name = buf;
        return;
    }
    if (chartClass == ChartFeatureClass::DepthArea && (!std::isnan(minDepthMeters) || !std::isnan(maxDepthMeters))) {
        char buf[48]{};
        std::snprintf(
            buf,
            sizeof(buf),
            "DEPARE %.2g..%.2g m",
            std::isnan(minDepthMeters) ? 0.0 : static_cast<double>(minDepthMeters),
            std::isnan(maxDepthMeters) ? 0.0 : static_cast<double>(maxDepthMeters)
        );
        name = buf;
        return;
    }
    if (!lightOrColor.empty() && !featureCode.empty()) {
        name = std::format("{} ({})", featureCode, lightOrColor);
        return;
    }
    if (!featureCode.empty()) {
        if (!displayId.empty()) {
            name = std::format("{} #{}", featureCode, displayId);
        } else {
            name = std::format("{} #{}", featureCode, fallbackIndex + 1);
        }
        return;
    }
    if (!fallbackFeatureType.empty()) {
        name = fallbackFeatureType;
        return;
    }

    if (!displayId.empty()) {
        name = std::format("{} #{}", GeometryTypeToString(type), displayId);
    } else {
        name = std::format("{} #{}", GeometryTypeToString(type), fallbackIndex + 1);
    }
}

void GeoFeature::SetProperty(std::string key, AttributeValue val) {
    for (auto& [k, v] : properties) {
        if (k == key) {
            v = std::move(val);
            return;
        }
    }
    properties.emplace_back(std::move(key), std::move(val));
}

std::string GeoFeature::GetPropertyString(std::string_view key) const {
    if (!propertiesLoaded && pager && sqliteRowId > 0) {
        (void)EnsurePropertiesLoaded();
    }
    for (const auto& [k, v] : properties) {
        if (EqualsIgnoreCaseAscii(k, key) && !v.IsNull()) {
            return v.ToString();
        }
    }
    return {};
}

float GeoFeature::GetPropertyFloat(std::string_view key) const noexcept {
    if (!propertiesLoaded && pager && sqliteRowId > 0) {
        (void)EnsurePropertiesLoaded();
    }
    for (const auto& [k, v] : properties) {
        if (EqualsIgnoreCaseAscii(k, key) && !v.IsNull()) {
            if (const auto* d = std::get_if<double>(&v.value)) return static_cast<float>(*d);
            if (const auto* i = std::get_if<int64_t>(&v.value)) return static_cast<float>(*i);
        }
    }
    return std::numeric_limits<float>::quiet_NaN();
}

size_t GeoFeature::VertexCount() const noexcept {
    size_t total = points.size();
    for (const auto& line : lines) {
        total += line.size();
    }
    for (const auto& poly : polygons) {
        total += poly.VertexCount();
    }
    if (total == 0 && cachedVertexCount > 0) {
        return static_cast<size_t>(cachedVertexCount);
    }
    return total;
}

size_t GeoFeature::TriangleCount() const noexcept {
    size_t total = 0;
    for (const auto& poly : polygons) {
        total += poly.triangles.size();
    }
    return total;
}

size_t GeoFeature::HoleCount() const noexcept {
    size_t total = 0;
    for (const auto& poly : polygons) {
        total += poly.interiorRings.size();
    }
    if (total == 0 && !geometryLoaded && cachedHoleCount > 0) {
        return static_cast<size_t>(cachedHoleCount);
    }
    return total;
}

bool GeoFeature::MatchesSearch(std::string_view query) const {
    if (query.empty()) return true;
    const std::string qLower = ToLowerAscii(query);

    if (ContainsCaseInsensitive(name, qLower)) return true;
    if (ContainsCaseInsensitive(featureCode, qLower)) return true;
    if (ContainsCaseInsensitive(labelRu, qLower)) return true;
    if (ContainsCaseInsensitive(labelEn, qLower)) return true;
    if (ContainsCaseInsensitive(lightOrColor, qLower)) return true;
    if (ContainsCaseInsensitive(displayId, qLower)) return true;
    if (ContainsCaseInsensitive(id, qLower)) return true;
    if (ContainsCaseInsensitive(GeometryTypeToString(type), qLower)) return true;

    for (const auto& [k, v] : properties) {
        if (ContainsCaseInsensitive(k, qLower) || v.MatchesSearch(qLower)) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// LayerSpatialIndex — Flat Sort-Tile-Recursive (STR) R-Tree
// ============================================================================

void LayerSpatialIndex::Build(const std::vector<GeoFeature>& features) {
    Clear();
    if (features.empty()) return;

    struct Item {
        uint32_t featIdx;
        guikit::Rect box;
        float cx;
        float cy;
    };

    std::vector<Item> items;
    items.reserve(features.size());
    for (uint32_t i = 0; i < static_cast<uint32_t>(features.size()); ++i) {
        const GeoFeature& f = features[i];
        if (!f.hasValidBounds) continue;
        guikit::Rect b = f.normBounds;
        if (b.w < 1e-7f) { b.x -= 0.5e-7f; b.w = 1e-7f; }
        if (b.h < 1e-7f) { b.y -= 0.5e-7f; b.h = 1e-7f; }
        items.push_back(Item{
            .featIdx = i,
            .box = b,
            .cx = b.x + b.w * 0.5f,
            .cy = b.y + b.h * 0.5f
        });
    }

    if (items.empty()) return;

    static constexpr size_t kNodeCap = 16;

    // 1. Sort items by X, slice into vertical strips, and sort each strip by Y (STR packing)
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        return a.cx < b.cx;
    });

    const size_t numLeaves = (items.size() + kNodeCap - 1) / kNodeCap;
    const size_t numStrips = std::max<size_t>(1, static_cast<size_t>(std::ceil(std::sqrt(static_cast<double>(numLeaves)))));
    const size_t itemsPerStrip = (items.size() + numStrips - 1) / numStrips;

    for (size_t s = 0; s < items.size(); s += itemsPerStrip) {
        const size_t end = std::min(items.size(), s + itemsPerStrip);
        std::sort(items.begin() + static_cast<std::ptrdiff_t>(s),
                  items.begin() + static_cast<std::ptrdiff_t>(end),
                  [](const Item& a, const Item& b) {
                      return a.cy < b.cy;
                  });
    }

    m_leafFeatureIndices.resize(items.size());
    for (size_t i = 0; i < items.size(); ++i) {
        m_leafFeatureIndices[i] = items[i].featIdx;
    }

    m_nodes.reserve(numLeaves * 2 + 4);

    // 2. Create leaf nodes
    uint32_t levelStart = 0;
    for (size_t i = 0; i < items.size(); i += kNodeCap) {
        const size_t end = std::min(items.size(), i + kNodeCap);
        guikit::Rect box = items[i].box;
        for (size_t j = i + 1; j < end; ++j) {
            const float x0 = std::min(box.x, items[j].box.x);
            const float y0 = std::min(box.y, items[j].box.y);
            const float x1 = std::max(box.Right(), items[j].box.Right());
            const float y1 = std::max(box.Bottom(), items[j].box.Bottom());
            box = guikit::Rect(x0, y0, x1 - x0, y1 - y0);
        }
        m_nodes.push_back(RTreeNode{
            .bounds = box,
            .firstChild = static_cast<uint32_t>(i),
            .count = static_cast<uint16_t>(end - i),
            .isLeaf = true
        });
    }
    uint32_t levelEnd = static_cast<uint32_t>(m_nodes.size());

    // 3. Build internal levels bottom-up until a single root node remains
    while (levelEnd - levelStart > 1) {
        const uint32_t nextLevelStart = levelEnd;
        for (uint32_t i = levelStart; i < levelEnd; i += static_cast<uint32_t>(kNodeCap)) {
            const uint32_t end = std::min(levelEnd, i + static_cast<uint32_t>(kNodeCap));
            guikit::Rect box = m_nodes[i].bounds;
            for (uint32_t j = i + 1; j < end; ++j) {
                const float x0 = std::min(box.x, m_nodes[j].bounds.x);
                const float y0 = std::min(box.y, m_nodes[j].bounds.y);
                const float x1 = std::max(box.Right(), m_nodes[j].bounds.Right());
                const float y1 = std::max(box.Bottom(), m_nodes[j].bounds.Bottom());
                box = guikit::Rect(x0, y0, x1 - x0, y1 - y0);
            }
            m_nodes.push_back(RTreeNode{
                .bounds = box,
                .firstChild = i,
                .count = static_cast<uint16_t>(end - i),
                .isLeaf = false
            });
        }
        levelStart = nextLevelStart;
        levelEnd = static_cast<uint32_t>(m_nodes.size());
    }

    m_rootIndex = m_nodes.empty() ? 0 : static_cast<uint32_t>(m_nodes.size() - 1);
}

void LayerSpatialIndex::QueryBox(guikit::Rect queryBox, std::vector<uint32_t>& outFeatureIndices) const {
    outFeatureIndices.clear();
    if (m_nodes.empty()) return;

    std::array<uint32_t, 128> stack{};
    size_t sp = 0;
    stack[sp++] = m_rootIndex;

    while (sp > 0) {
        const RTreeNode& node = m_nodes[stack[--sp]];
        if (!node.bounds.Intersects(queryBox)) {
            continue;
        }
        if (node.isLeaf) {
            const uint32_t end = node.firstChild + node.count;
            for (uint32_t i = node.firstChild; i < end; ++i) {
                outFeatureIndices.push_back(m_leafFeatureIndices[i]);
            }
        } else {
            const uint32_t end = node.firstChild + node.count;
            for (uint32_t i = node.firstChild; i < end; ++i) {
                if (sp < stack.size()) {
                    stack[sp++] = i;
                }
            }
        }
    }
}

void GeoLayer::FinalizeLayer(uint64_t layerUid, bool forceTriangulateAll) {
    uid = layerUid;
    const bool preserveStreamMetadata = isWindowStreamed && hasValidBounds;
    const guikit::GeoBounds savedBounds = bounds;
    const guikit::Rect savedNormBounds = normBounds;
    const size_t savedPointCount = pointFeatureCount;
    const size_t savedLineCount = lineFeatureCount;
    const size_t savedPolyCount = polygonFeatureCount;
    const ChartFeatureClass savedDominantClass = dominantChartClass;
    const uint32_t savedClassMask = chartClassMask;

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
             features.front().uid == ((layerUid << 32) | static_cast<uint64_t>(static_cast<uint32_t>(features.front().sqliteRowId))) &&
             !features.front().id.empty() &&
             !features.front().name.empty() &&
             features.back().uid == ((layerUid << 32) | static_cast<uint64_t>(static_cast<uint32_t>(features.back().sqliteRowId))));
        if (allPrebound) {
            bounds = savedBounds;
            normBounds = savedNormBounds;
            hasValidBounds = true;
            if (!features.empty()) {
                for (const GeoFeature& f : features) {
                    chartClassMask |= (1u << static_cast<uint8_t>(f.chartClass));
                    if (f.HasPolygons()) {
                        ++polygonFeatureCount;
                    } else if (f.HasLines()) {
                        ++lineFeatureCount;
                    } else {
                        ++pointFeatureCount;
                    }
                    totalVertices += std::max<size_t>(1u, f.VertexCount());
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
            spatialIndex.Build(features);
            return;
        }
    }

    BindFeaturePagerPointers();

    const bool doTriangulate = forceTriangulateAll || (features.size() <= 512 && !pager);
    const std::string layerHint = !tableName.empty() ? tableName : name;
    std::array<size_t, 16> classCounts{};
    uint32_t activeClassMask = 0;

    for (size_t i = 0; i < features.size(); ++i) {
        GeoFeature& feat = features[i];
        const uint64_t lowId = (isWindowStreamed && feat.sqliteRowId > 0)
            ? static_cast<uint64_t>(static_cast<uint32_t>(feat.sqliteRowId))
            : static_cast<uint64_t>(i + 1);
        const uint64_t expectedUid = (layerUid << 32) | lowId;
        if (!isWindowStreamed || feat.sqliteRowId <= 0 || feat.uid != expectedUid || feat.id.empty()) {
            feat.uid = expectedUid;
            if (isWindowStreamed && feat.sqliteRowId > 0) {
                char idBuf[40]{};
                std::snprintf(idBuf, sizeof(idBuf), "L%llu_R%lld",
                              static_cast<unsigned long long>(layerUid),
                              static_cast<long long>(feat.sqliteRowId));
                feat.id = idBuf;
            } else {
                char idBuf[40]{};
                std::snprintf(idBuf, sizeof(idBuf), "L%llu_F%zu",
                              static_cast<unsigned long long>(layerUid), i);
                feat.id = idBuf;
            }
        }
        if (feat.displayId.empty()) {
            char dispBuf[24]{};
            std::snprintf(dispBuf, sizeof(dispBuf), "%llu", static_cast<unsigned long long>(lowId));
            feat.displayId = dispBuf;
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

        const uint8_t clsIdx = static_cast<uint8_t>(feat.chartClass);
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
        size_t bestCount = 0;
        ChartFeatureClass bestClass = ChartFeatureClass::Generic;
        for (size_t c = 1; c < classCounts.size(); ++c) {
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
            ? (1u << static_cast<uint8_t>(dominantChartClass))
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
    spatialIndex.Build(features);
}

bool GeoLayer::ApplyWindowSnapshot(SpatialWindowSnapshot&& snapshot) noexcept {
    if (!snapshot.changed) {
        return false;
    }
    const size_t savedPointCount = pointFeatureCount;
    const size_t savedLineCount = lineFeatureCount;
    const size_t savedPolyCount = polygonFeatureCount;
    const ChartFeatureClass savedDominantClass = dominantChartClass;
    const uint32_t savedClassMask = chartClassMask;

    features = std::move(snapshot.features);
    spatialIndex = std::move(snapshot.spatialIndex);
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

size_t GeoLayer::EvictAllPagedPayloads() const noexcept {
    size_t freed = 0;
    for (const GeoFeature& feat : features) {
        freed += feat.UnloadGeometryPayload();
        freed += feat.UnloadPropertiesPayload();
    }
    if (pager) {
        freed += pager->EvictChunksToBudget(0, std::numeric_limits<uint64_t>::max());
        pager->CloseIdleConnection();
    }
    return freed;
}

guikit::GeoCoord SrsTransform::ToWgs84(double x, double y, int srsId) noexcept {
    // 1. WGS84 / UTM Northern Hemisphere (EPSG:32601 .. EPSG:32660)
    if (srsId >= 32601 && srsId <= 32660) {
        const int zone = srsId - 32600;
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
        const int zone = srsId - 32700;
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
        const int zone = srsId - 20900;
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
        const int zone = (srsId >= 28461) ? (srsId - 28460) : (srsId - 28400);
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
        const int zone = (srsId >= 20061) ? (srsId - 20060) : (srsId - 20000);
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
        const double lon = (x / R) * (180.0 / std::numbers::pi);
        const double lat = (2.0 * std::atan(std::exp(y / R)) - 0.5 * std::numbers::pi) * (180.0 / std::numbers::pi);
        return {
            guikit::MapGrid::ClampLat(lat, guikit::MapCrs::EPSG4326_PlateCarree),
            guikit::MapGrid::ClampLon(lon)
        };
    }

    if (srsId == 3395) {
        constexpr double R = guikit::MapGrid::kEarthRadiusMeters;
        constexpr double e = guikit::MapGrid::kWgs84Eccentricity;
        const double lon = (x / R) * (180.0 / std::numbers::pi);
        const double ts = std::exp(-y / R);
        double phi = 0.5 * std::numbers::pi - 2.0 * std::atan(ts);
        for (int iter = 0; iter < 6; ++iter) {
            const double esin = e * std::sin(phi);
            const double con = std::pow((1.0 - esin) / (1.0 + esin), 0.5 * e);
            phi = 0.5 * std::numbers::pi - 2.0 * std::atan(ts * con);
        }
        const double lat = phi * (180.0 / std::numbers::pi);
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

std::pair<double, double> SrsTransform::FromWgs84(guikit::GeoCoord coord, int srsId) noexcept {
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
        const double lonRad = guikit::MapGrid::ClampLon(coord.lon) * (std::numbers::pi / 180.0);
        const double phi = guikit::MapGrid::ClampLat(coord.lat, guikit::MapCrs::EPSG3395_WorldMercator) * (std::numbers::pi / 180.0);
        const double eSin = e * std::sin(phi);
        const double cf = std::pow((1.0 - eSin) / (1.0 + eSin), 0.5 * e);
        return {
            R * lonRad,
            R * std::log(std::tan(0.25 * std::numbers::pi + 0.5 * phi) * cf)
        };
    }
    if (srsId >= 32601 && srsId <= 32660) {
        const int zone = srsId - 32600;
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
        const int zone = srsId - 32700;
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
        const int zone = srsId - 20900;
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
        const int zone = (srsId >= 28461) ? (srsId - 28460) : (srsId - 28400);
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
        const int zone = (srsId >= 20061) ? (srsId - 20060) : (srsId - 20000);
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

std::pair<double, double> SrsTransform::Wgs84ToWebMercatorMeters(guikit::GeoCoord coord) noexcept {
    constexpr double R = guikit::MapGrid::kEarthRadiusMeters;
    const double lonRad = guikit::MapGrid::ClampLon(coord.lon) * (std::numbers::pi / 180.0);
    const double latRad = guikit::MapGrid::ClampLat(coord.lat, guikit::MapCrs::EPSG3857_WebMercator) * (std::numbers::pi / 180.0);
    const double x = R * lonRad;
    const double y = R * std::log(std::tan(0.25 * std::numbers::pi + 0.5 * latRad));
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
        bounds.nw.lat = std::max(bounds.nw.lat, pt.lat);
        bounds.nw.lon = std::min(bounds.nw.lon, pt.lon);
        bounds.se.lat = std::min(bounds.se.lat, pt.lat);
        bounds.se.lon = std::max(bounds.se.lon, pt.lon);
    }
}

void SrsTransform::UnionBounds(guikit::GeoBounds& bounds, bool& initialized, const guikit::GeoBounds& other) noexcept {
    ExpandBounds(bounds, initialized, other.nw);
    ExpandBounds(bounds, initialized, other.se);
}

bool SrsTransform::BoundsIntersect(const guikit::GeoBounds& a, const guikit::GeoBounds& b) noexcept {
    const double aMinLon = std::min(a.nw.lon, a.se.lon);
    const double aMaxLon = std::max(a.nw.lon, a.se.lon);
    const double aMinLat = std::min(a.se.lat, a.nw.lat);
    const double aMaxLat = std::max(a.se.lat, a.nw.lat);

    const double bMinLon = std::min(b.nw.lon, b.se.lon);
    const double bMaxLon = std::max(b.nw.lon, b.se.lon);
    const double bMinLat = std::min(b.se.lat, b.nw.lat);
    const double bMaxLat = std::max(b.se.lat, b.nw.lat);

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
    const double dLon = std::max(1e-5, std::abs(bounds.se.lon - bounds.nw.lon));
    const double dLat = std::max(1e-5, std::abs(bounds.nw.lat - bounds.se.lat));
    const double maxSpanDeg = std::max(dLon, dLat * 1.45);

    if (maxSpanDeg <= 0.005) {
        return 14.5f;
    }
    const double z = std::log2(360.0 / maxSpanDeg) - 0.35;
    return static_cast<float>(std::clamp(z, 1.5, 16.5));
}

guikit::Color SrsTransform::DefaultLayerColor(size_t paletteIndex) noexcept {
    static constexpr std::array<guikit::Color, 8> kPalette = {{
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
    std::string_view code,
    GeometryType geomType,
    std::string_view layerHint
) noexcept {
    const std::string_view effectiveCode = !code.empty() ? code : layerHint;
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
    std::span<const GeoLayer> layers,
    uint64_t uid,
    const GeoLayer** outLayer
) noexcept {
    if (uid == 0) return nullptr;
    const uint32_t targetLayerUid = static_cast<uint32_t>(uid >> 32);
    const uint32_t oneBasedIdx = static_cast<uint32_t>(uid & 0xFFFFFFFFu);

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
    [[nodiscard]] std::string_view GetSourceId() const noexcept override {
        return "carto_fallback";
    }

    [[nodiscard]] std::string_view GetDisplayName() const noexcept override {
        return "Cartographic Basemap Grid";
    }

    [[nodiscard]] std::shared_ptr<const RasterTileImage> TryGetCachedTile(
        const SlippyTileKey& key
    ) const noexcept override {
        const SlippyTileKey normKey = NormalizeKey(key);
        std::lock_guard<std::mutex> lk(m_mutex);
        auto it = m_cache.find(normKey);
        return (it != m_cache.end()) ? it->second : nullptr;
    }

    [[nodiscard]] std::shared_ptr<const RasterTileImage> FetchTileBlocking(
        const SlippyTileKey& key
    ) override {
        const SlippyTileKey normKey = NormalizeKey(key);
        if (auto cached = TryGetCachedTile(normKey)) {
            return cached;
        }

        auto tileImg = std::make_shared<RasterTileImage>();
        tileImg->key = normKey;
        tileImg->width = normKey.tileSizePx;
        tileImg->height = normKey.tileSizePx;

        const uint32_t sz = normKey.tileSizePx;
        tileImg->rgbaPixels.resize(static_cast<size_t>(sz) * sz);

        const bool evenChecker = ((normKey.x + normKey.y) & 1) == 0;
        const uint8_t baseR = evenChecker ? 236 : 231;
        const uint8_t baseG = evenChecker ? 233 : 228;
        const uint8_t baseB = evenChecker ? 222 : 216;
        const uint32_t subStep = std::max(16u, sz / 4u);

        for (uint32_t py = 0; py < sz; ++py) {
            for (uint32_t px = 0; px < sz; ++px) {
                uint8_t r = baseR;
                uint8_t g = baseG;
                uint8_t b = baseB;
                if (px == 0 || py == 0) {
                    r = 188; g = 198; b = 208;
                } else if ((px % subStep) == 0 || (py % subStep) == 0) {
                    r = 216; g = 214; b = 204;
                }
                tileImg->rgbaPixels[static_cast<size_t>(py) * sz + px] =
                    (static_cast<uint32_t>(255u) << 24u) |
                    (static_cast<uint32_t>(b) << 16u) |
                    (static_cast<uint32_t>(g) << 8u) |
                    static_cast<uint32_t>(r);
            }
        }

        {
            std::lock_guard<std::mutex> lk(m_mutex);
            if (m_cache.size() >= 256) {
                m_cache.erase(m_cache.begin());
            }
            m_cache[normKey] = tileImg;
        }
        return tileImg;
    }

    void ClearCache() noexcept override {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_cache.clear();
    }

private:
    [[nodiscard]] static SlippyTileKey NormalizeKey(const SlippyTileKey& k) noexcept {
        const int z = std::clamp(k.z, 0, 19);
        const int n = 1 << z;
        const int x = ((k.x % n) + n) % n;
        const int y = std::clamp(k.y, 0, n - 1);
        const uint32_t sz = (k.tileSizePx <= 160u) ? 128u : ((k.tileSizePx >= 384u) ? 512u : 256u);
        return SlippyTileKey{.z = z, .x = x, .y = y, .tileSizePx = sz};
    }

    mutable std::mutex m_mutex{};
    mutable std::unordered_map<SlippyTileKey, std::shared_ptr<const RasterTileImage>, SlippyTileKeyHash> m_cache{};
};

} // namespace

std::shared_ptr<IRasterTileProvider> CreateCartographicBasemapTileProvider() {
    return std::make_shared<CartographicFallbackTileProvider>();
}

} // namespace guikit::geo
