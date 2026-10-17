#include "guikit/geo/GeoTypes.hpp"
#include "guikit/geo/PolygonTriangulator.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>

namespace guikit {

namespace {

[[nodiscard]] std::string ToUpperAscii(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

[[nodiscard]] bool EqualsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<double> TryParseDouble(std::string_view sv) noexcept {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) sv.remove_suffix(1);
    if (sv.empty()) return std::nullopt;

    double val = 0.0;
    const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
    if (ec == std::errc{}) {
        return val;
    }
    return std::nullopt;
}

} // namespace

std::string_view GeoFeature::GetAttribute(std::string_view key) const noexcept {
    for (const auto& attr : attributes) {
        if (EqualsIgnoreCase(attr.key, key)) {
            return attr.value;
        }
    }
    return {};
}

void GeoFeature::SetAttribute(std::string key, std::string value) {
    for (auto& attr : attributes) {
        if (EqualsIgnoreCase(attr.key, key)) {
            attr.value = std::move(value);
            return;
        }
    }
    attributes.push_back(GeoAttribute{std::move(key), std::move(value)});
}

void GeoFeature::RecomputeBBox() noexcept {
    bbox = GeoBBox{};
    for (const auto& pt : points) {
        bbox.Expand(pt);
    }
    for (const auto& line : lines) {
        for (const auto& pt : line) {
            bbox.Expand(pt);
        }
    }
    for (auto& poly : polygons) {
        for (const auto& pt : poly.exterior) {
            bbox.Expand(pt);
        }
        for (const auto& hole : poly.holes) {
            for (const auto& pt : hole) {
                bbox.Expand(pt);
            }
        }
        // Only run ear-clipping fallback for polygons without interior holes;
        // polygons with holes are rendered directly as compound Even-Odd paths by ThorVG.
        if (poly.cachedTriangles.empty() && poly.holes.empty() && poly.exterior.size() >= 3) {
            poly.cachedTriangles = PolygonTriangulator::Triangulate(poly);
        }
    }
}

size_t GeoFeature::TotalVertexCount() const noexcept {
    size_t count = points.size();
    for (const auto& l : lines) count += l.size();
    for (const auto& p : polygons) {
        count += p.exterior.size();
        for (const auto& h : p.holes) count += h.size();
    }
    return count;
}

void GeoLayer::RecomputeBBox() noexcept {
    bbox = GeoBBox{};
    for (auto& f : features) {
        if (!f.bbox.IsValid()) {
            f.RecomputeBBox();
        }
        bbox.Expand(f.bbox);
    }
}

size_t GeoLayer::TotalVertexCount() const noexcept {
    size_t total = 0;
    for (const auto& f : features) {
        total += f.TotalVertexCount();
    }
    return total;
}

GisFeatureClass ClassifyFeatureFromAttributes(const GeoFeature& f) noexcept {
    const std::string code = ToUpperAscii(f.featureCode);

    auto checkCode = [](std::string_view upper) -> std::optional<GisFeatureClass> {
        if (upper.empty()) return std::nullopt;
        if (upper == "LNDARE" || upper == "LAND" || upper == "ISLAND" || upper == "LNDRGN" || upper == "VEGATN") {
            return GisFeatureClass::LandArea;
        }
        if (upper == "COALNE" || upper == "SLCONS" || upper == "BERTHS" || upper == "STSLIN" ||
            upper == "COASTLINE" || upper == "SHORELINE") {
            return GisFeatureClass::Coastline;
        }
        if (upper == "DEPARE" || upper == "DRGARE" || upper == "LAKARE" || upper == "RIVERS" ||
            upper == "DEPTH_AREA" || upper == "BATHYMETRY" || upper == "UNSARE") {
            return GisFeatureClass::DepthArea;
        }
        if (upper == "DEPCNT" || upper == "DEPTH_CONTOUR" || upper == "ISOBATH" || upper == "CONTOUR") {
            return GisFeatureClass::DepthContour;
        }
        if (upper == "SOUNDG" || upper == "SOUNDING" || upper == "DEPTH_POINT") {
            return GisFeatureClass::Sounding;
        }
        if (upper.starts_with("BOY") || upper.starts_with("BCN") || upper == "SEAMARK" ||
            upper == "LNDMRK" || upper == "DAYMAR" || upper == "TOPMAR" || upper == "FOGSIG" ||
            upper == "RTPBCN" || upper == "RADSTA" || upper == "PILBOP" || upper == "DISMAR") {
            return GisFeatureClass::Seamark;
        }
        if (upper == "LIGHTS" || upper == "LIGHT" || upper == "LIGHTHOUSE") {
            return GisFeatureClass::Light;
        }
        if (upper == "UWTROC" || upper == "WRECKS" || upper == "OBSTRN" || upper == "SBDARE_PT" ||
            upper == "ROCK" || upper == "WRECK" || upper == "HAZARD" || upper == "FOUL") {
            return GisFeatureClass::UnderwaterHazard;
        }
        if (upper == "RESARE" || upper == "ACHARE" || upper == "ACHBRT" || upper == "CTNARE" ||
            upper == "MIPARE" || upper == "FSHZNE" || upper == "DMPGRD" || upper == "TSEZNE" ||
            upper == "RESTRICTED") {
            return GisFeatureClass::RestrictedArea;
        }
        if (upper == "FAIRWY" || upper == "NAVLNE" || upper == "RECTRC" || upper == "DWRTCL" ||
            upper == "DWRTPT" || upper == "TSSLPT" || upper == "TSSBND" || upper == "FAIRWAY" || upper == "ROUTE") {
            return GisFeatureClass::Fairway;
        }
        if (upper == "CBLSUB" || upper == "CBLARE" || upper == "PIPSOL" || upper == "PIPARE" ||
            upper == "CABLE" || upper == "PIPELINE") {
            return GisFeatureClass::CablePipeline;
        }
        if (upper == "BUISGL" || upper == "BUAARE" || upper == "HRBFAC" || upper == "PONTON" ||
            upper == "MORFAC" || upper == "CRANES" || upper == "BUILDING") {
            return GisFeatureClass::UrbanBuilding;
        }
        if (upper == "ROADWY" || upper == "RAILWY" || upper == "BRIDGE" || upper == "CAUSWY") {
            return GisFeatureClass::RoadRailway;
        }
        return std::nullopt;
    };

    if (auto cls = checkCode(code)) {
        return *cls;
    }

    // Check common GeoJSON / S-57 property keys
    static constexpr std::string_view kClassKeys[] = {
        "OBJL", "acronym", "s57_class", "feature_type", "layer", "type", "class", "category", "natural", "seamark:type"
    };
    for (std::string_view k : kClassKeys) {
        const std::string_view val = f.GetAttribute(k);
        if (!val.empty()) {
            const std::string u = ToUpperAscii(val);
            if (auto cls = checkCode(u)) {
                return *cls;
            }
            if (u.find("SOUND") != std::string::npos) return GisFeatureClass::Sounding;
            if (u.find("CONTOUR") != std::string::npos || u.find("ISOBATH") != std::string::npos) return GisFeatureClass::DepthContour;
            if (u.find("DEPTH") != std::string::npos || u.find("BATHY") != std::string::npos) {
                return (f.geometryType == GeometryType::Polygon || f.geometryType == GeometryType::MultiPolygon)
                    ? GisFeatureClass::DepthArea
                    : GisFeatureClass::DepthContour;
            }
            if (u.find("LAND") != std::string::npos || u.find("ISLAND") != std::string::npos) return GisFeatureClass::LandArea;
            if (u.find("COAST") != std::string::npos || u.find("SHORE") != std::string::npos) return GisFeatureClass::Coastline;
            if (u.find("BUOY") != std::string::npos || u.find("BEACON") != std::string::npos || u.find("SEAMARK") != std::string::npos) return GisFeatureClass::Seamark;
            if (u.find("LIGHT") != std::string::npos) return GisFeatureClass::Light;
            if (u.find("WRECK") != std::string::npos || u.find("ROCK") != std::string::npos || u.find("OBSTR") != std::string::npos) return GisFeatureClass::UnderwaterHazard;
        }
    }

    // Check if depth attributes exist
    if (f.depthMinMeters.has_value() || f.depthMaxMeters.has_value() || !f.GetAttribute("DRVAL1").empty()) {
        if (f.geometryType == GeometryType::Polygon || f.geometryType == GeometryType::MultiPolygon) {
            return GisFeatureClass::DepthArea;
        }
    }
    if (f.contourDepthMeters.has_value() || !f.GetAttribute("VALDCO").empty()) {
        if (f.geometryType == GeometryType::LineString || f.geometryType == GeometryType::MultiLineString) {
            return GisFeatureClass::DepthContour;
        }
    }
    if (!f.GetAttribute("VALSOU").empty() || (!f.points.empty() && f.points.front().hasZ)) {
        if (f.geometryType == GeometryType::Point || f.geometryType == GeometryType::MultiPoint) {
            return GisFeatureClass::Sounding;
        }
    }

    // Fallback by geometry type
    if (f.geometryType == GeometryType::Polygon || f.geometryType == GeometryType::MultiPolygon) {
        return GisFeatureClass::LandArea;
    }
    if (f.geometryType == GeometryType::LineString || f.geometryType == GeometryType::MultiLineString) {
        return GisFeatureClass::Coastline;
    }
    return GisFeatureClass::Generic;
}

int DefaultZOrderForClass(GisFeatureClass cls) noexcept {
    switch (cls) {
        case GisFeatureClass::DepthArea:        return 10;
        case GisFeatureClass::LandArea:         return 20;
        case GisFeatureClass::UrbanBuilding:    return 25;
        case GisFeatureClass::RoadRailway:      return 28;
        case GisFeatureClass::RestrictedArea:   return 30;
        case GisFeatureClass::Fairway:          return 35;
        case GisFeatureClass::CablePipeline:    return 40;
        case GisFeatureClass::DepthContour:     return 50;
        case GisFeatureClass::Coastline:        return 60;
        case GisFeatureClass::Sounding:         return 70;
        case GisFeatureClass::UnderwaterHazard: return 80;
        case GisFeatureClass::Light:            return 90;
        case GisFeatureClass::Seamark:          return 95;
        case GisFeatureClass::Generic:          return 55;
    }
    return 55;
}

} // namespace guikit
