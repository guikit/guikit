#include "guikit/geo/GeoJsonIO.hpp"
#include "guikit/core/Assets.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <numbers>
#include <sstream>

namespace guikit {

namespace {

// ============================================================================
// Lightweight Zero-Dependency Recursive-Descent JSON DOM for RFC 7946 GeoJSON
// ============================================================================
enum class JsonType : uint8_t {
    Null = 0,
    Bool,
    Number,
    String,
    Array,
    Object
};

struct JsonValue {
    JsonType type{JsonType::Null};
    bool boolVal{false};
    double numVal{0.0};
    std::string strVal;
    std::vector<JsonValue> arrVal;
    std::vector<std::pair<std::string, JsonValue>> objVal;

    [[nodiscard]] const JsonValue* Find(std::string_view key) const noexcept {
        if (type != JsonType::Object) return nullptr;
        for (const auto& kv : objVal) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }

    [[nodiscard]] std::string_view AsString(std::string_view fallback = {}) const noexcept {
        return (type == JsonType::String) ? std::string_view(strVal) : fallback;
    }

    [[nodiscard]] double AsNumber(double fallback = 0.0) const noexcept {
        return (type == JsonType::Number) ? numVal : fallback;
    }

    [[nodiscard]] std::string ToAttributeString() const {
        char buf[64]{};
        switch (type) {
            case JsonType::Null:   return "";
            case JsonType::Bool:   return boolVal ? "true" : "false";
            case JsonType::Number: {
                if (std::abs(numVal - std::round(numVal)) < 1e-9 && std::abs(numVal) < 1e12) {
                    std::snprintf(buf, sizeof(buf), "%.0f", numVal);
                } else {
                    std::snprintf(buf, sizeof(buf), "%.4g", numVal);
                }
                return std::string(buf);
            }
            case JsonType::String: return strVal;
            case JsonType::Array:  return "[...]";
            case JsonType::Object: return "{...}";
        }
        return "";
    }
};

class JsonParser {
public:
    explicit JsonParser(std::string_view src) noexcept : m_src(src) {}

    [[nodiscard]] bool ParseRoot(JsonValue& out, std::string& err) {
        SkipWhitespace();
        if (m_pos >= m_src.size()) {
            err = "Empty JSON document";
            return false;
        }
        if (!ParseValue(out, err)) {
            return false;
        }
        return true;
    }

private:
    void SkipWhitespace() noexcept {
        // Also skip UTF-8 BOM if at start
        if (m_pos == 0 && m_src.size() >= 3 &&
            static_cast<uint8_t>(m_src[0]) == 0xEF &&
            static_cast<uint8_t>(m_src[1]) == 0xBB &&
            static_cast<uint8_t>(m_src[2]) == 0xBF) {
            m_pos = 3;
        }
        while (m_pos < m_src.size()) {
            const unsigned char c = static_cast<unsigned char>(m_src[m_pos]);
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++m_pos;
            } else {
                break;
            }
        }
    }

    [[nodiscard]] bool ParseValue(JsonValue& out, std::string& err) {
        SkipWhitespace();
        if (m_pos >= m_src.size()) {
            err = "Unexpected end of JSON input";
            return false;
        }
        const char c = m_src[m_pos];
        if (c == '{') return ParseObject(out, err);
        if (c == '[') return ParseArray(out, err);
        if (c == '"') {
            out.type = JsonType::String;
            return ParseString(out.strVal, err);
        }
        if (c == 't' || c == 'f') return ParseBool(out, err);
        if (c == 'n') return ParseNull(out, err);
        if (c == '-' || (c >= '0' && c <= '9')) return ParseNumber(out, err);

        err = std::string("Unexpected character in JSON: '") + c + "'";
        return false;
    }

    [[nodiscard]] bool ParseObject(JsonValue& out, std::string& err) {
        out.type = JsonType::Object;
        ++m_pos; // skip '{'
        SkipWhitespace();
        if (m_pos < m_src.size() && m_src[m_pos] == '}') {
            ++m_pos;
            return true;
        }
        while (m_pos < m_src.size()) {
            SkipWhitespace();
            if (m_pos >= m_src.size() || m_src[m_pos] != '"') {
                err = "Expected string key in JSON object";
                return false;
            }
            std::string key;
            if (!ParseString(key, err)) return false;

            SkipWhitespace();
            if (m_pos >= m_src.size() || m_src[m_pos] != ':') {
                err = "Expected ':' after key in JSON object";
                return false;
            }
            ++m_pos;

            JsonValue val;
            if (!ParseValue(val, err)) return false;
            out.objVal.emplace_back(std::move(key), std::move(val));

            SkipWhitespace();
            if (m_pos < m_src.size() && m_src[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_pos < m_src.size() && m_src[m_pos] == '}') {
                ++m_pos;
                return true;
            }
            err = "Expected ',' or '}' in JSON object";
            return false;
        }
        err = "Unterminated JSON object";
        return false;
    }

    [[nodiscard]] bool ParseArray(JsonValue& out, std::string& err) {
        out.type = JsonType::Array;
        ++m_pos; // skip '['
        SkipWhitespace();
        if (m_pos < m_src.size() && m_src[m_pos] == ']') {
            ++m_pos;
            return true;
        }
        while (m_pos < m_src.size()) {
            JsonValue elem;
            if (!ParseValue(elem, err)) return false;
            out.arrVal.push_back(std::move(elem));

            SkipWhitespace();
            if (m_pos < m_src.size() && m_src[m_pos] == ',') {
                ++m_pos;
                continue;
            }
            if (m_pos < m_src.size() && m_src[m_pos] == ']') {
                ++m_pos;
                return true;
            }
            err = "Expected ',' or ']' in JSON array";
            return false;
        }
        err = "Unterminated JSON array";
        return false;
    }

    [[nodiscard]] bool ParseString(std::string& out, std::string& err) {
        ++m_pos; // skip '"'
        out.clear();
        while (m_pos < m_src.size()) {
            const char c = m_src[m_pos++];
            if (c == '"') {
                return true;
            }
            if (c == '\\') {
                if (m_pos >= m_src.size()) {
                    err = "Unterminated escape sequence in JSON string";
                    return false;
                }
                const char esc = m_src[m_pos++];
                switch (esc) {
                    case '"':  out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/'); break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u': {
                        if (m_pos + 4 <= m_src.size()) {
                            uint32_t cp = 0;
                            for (int i = 0; i < 4; ++i) {
                                const char hc = m_src[m_pos + i];
                                cp <<= 4;
                                if (hc >= '0' && hc <= '9') cp |= static_cast<uint32_t>(hc - '0');
                                else if (hc >= 'a' && hc <= 'f') cp |= static_cast<uint32_t>(hc - 'a' + 10);
                                else if (hc >= 'A' && hc <= 'F') cp |= static_cast<uint32_t>(hc - 'A' + 10);
                            }
                            m_pos += 4;
                            if (cp < 0x80) {
                                out.push_back(static_cast<char>(cp));
                            } else if (cp < 0x800) {
                                out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                            } else {
                                out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                            }
                        }
                        break;
                    }
                    default: out.push_back(esc); break;
                }
            } else {
                out.push_back(c);
            }
        }
        err = "Unterminated JSON string";
        return false;
    }

    [[nodiscard]] bool ParseBool(JsonValue& out, std::string& err) {
        if (m_src.substr(m_pos, 4) == "true") {
            out.type = JsonType::Bool;
            out.boolVal = true;
            m_pos += 4;
            return true;
        }
        if (m_src.substr(m_pos, 5) == "false") {
            out.type = JsonType::Bool;
            out.boolVal = false;
            m_pos += 5;
            return true;
        }
        err = "Invalid boolean literal in JSON";
        return false;
    }

    [[nodiscard]] bool ParseNull(JsonValue& out, std::string& err) {
        if (m_src.substr(m_pos, 4) == "null") {
            out.type = JsonType::Null;
            m_pos += 4;
            return true;
        }
        err = "Invalid null literal in JSON";
        return false;
    }

    [[nodiscard]] bool ParseNumber(JsonValue& out, std::string& err) {
        const size_t start = m_pos;
        if (m_src[m_pos] == '-') ++m_pos;
        while (m_pos < m_src.size() && std::isdigit(static_cast<unsigned char>(m_src[m_pos]))) ++m_pos;
        if (m_pos < m_src.size() && m_src[m_pos] == '.') {
            ++m_pos;
            while (m_pos < m_src.size() && std::isdigit(static_cast<unsigned char>(m_src[m_pos]))) ++m_pos;
        }
        if (m_pos < m_src.size() && (m_src[m_pos] == 'e' || m_src[m_pos] == 'E')) {
            ++m_pos;
            if (m_pos < m_src.size() && (m_src[m_pos] == '+' || m_src[m_pos] == '-')) ++m_pos;
            while (m_pos < m_src.size() && std::isdigit(static_cast<unsigned char>(m_src[m_pos]))) ++m_pos;
        }

        double val = 0.0;
        const auto [ptr, ec] = std::from_chars(m_src.data() + start, m_src.data() + m_pos, val);
        if (ec != std::errc{}) {
            err = "Invalid numeric literal in JSON";
            return false;
        }
        out.type = JsonType::Number;
        out.numVal = val;
        return true;
    }

    std::string_view m_src;
    size_t m_pos{0};
};

// ============================================================================
// Coordinate & Geometry Extraction Helpers
// ============================================================================
[[nodiscard]] bool ExtractPointCoord(const JsonValue& arr, GeoPoint& outPt) noexcept {
    if (arr.type != JsonType::Array || arr.arrVal.size() < 2) return false;
    if (arr.arrVal[0].type != JsonType::Number || arr.arrVal[1].type != JsonType::Number) return false;
    outPt.lon = arr.arrVal[0].numVal;
    outPt.lat = arr.arrVal[1].numVal;
    if (arr.arrVal.size() >= 3 && arr.arrVal[2].type == JsonType::Number) {
        outPt.z = arr.arrVal[2].numVal;
        outPt.hasZ = true;
    } else {
        outPt.z = 0.0;
        outPt.hasZ = false;
    }
    return true;
}

[[nodiscard]] std::vector<GeoPoint> ExtractPointList(const JsonValue& arr) {
    std::vector<GeoPoint> pts;
    if (arr.type != JsonType::Array) return pts;
    pts.reserve(arr.arrVal.size());
    for (const auto& item : arr.arrVal) {
        GeoPoint pt{};
        if (ExtractPointCoord(item, pt)) {
            pts.push_back(pt);
        }
    }
    return pts;
}

[[nodiscard]] GeoPolygonRings ExtractPolygonRings(const JsonValue& ringsArr) {
    GeoPolygonRings poly{};
    if (ringsArr.type != JsonType::Array || ringsArr.arrVal.empty()) return poly;
    poly.exterior = ExtractPointList(ringsArr.arrVal[0]);
    for (size_t i = 1; i < ringsArr.arrVal.size(); ++i) {
        auto hole = ExtractPointList(ringsArr.arrVal[i]);
        if (hole.size() >= 3) {
            poly.holes.push_back(std::move(hole));
        }
    }
    return poly;
}

[[nodiscard]] std::optional<double> TryGetNumericAttr(const GeoFeature& f, std::initializer_list<std::string_view> keys) noexcept {
    for (std::string_view k : keys) {
        const std::string_view sv = f.GetAttribute(k);
        if (!sv.empty()) {
            double v = 0.0;
            const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), v);
            if (ec == std::errc{}) return v;
        }
    }
    return std::nullopt;
}

void PopulateFeatureHydroAttributes(GeoFeature& f) {
    if (f.featureCode.empty()) {
        for (std::string_view k : {"feature_code", "OBJL", "acronym", "s57_class", "feature_type", "layer", "type", "class"}) {
            const std::string_view v = f.GetAttribute(k);
            if (!v.empty()) {
                f.featureCode = std::string(v);
                break;
            }
        }
    }
    if (f.name.empty()) {
        for (std::string_view k : {"OBJNAM", "name", "NOBJNM", "label_en", "label_ru", "buoy_name_en", "buoy_name_ru", "title", "label", "seamark:name", "id"}) {
            const std::string_view v = f.GetAttribute(k);
            if (!v.empty()) {
                f.name = std::string(v);
                break;
            }
        }
    }

    f.depthMinMeters = TryGetNumericAttr(f, {"DRVAL1", "min_depth_m", "depth_min", "min_depth", "drval1"});
    f.depthMaxMeters = TryGetNumericAttr(f, {"DRVAL2", "max_depth_m", "depth_max", "max_depth", "drval2"});
    f.contourDepthMeters = TryGetNumericAttr(f, {"VALDCO", "VALSOU", "depth_m", "depth", "elevation", "valdco", "valsou", "value"});

    if (!f.contourDepthMeters.has_value() && !f.points.empty() && f.points.front().hasZ) {
        f.contourDepthMeters = f.points.front().z;
    }

    f.featureClass = ClassifyFeatureFromAttributes(f);
    f.RecomputeBBox();
}

void ExtractFeaturesFromGeometryNode(
    const JsonValue& geomNode,
    const std::vector<GeoAttribute>& props,
    std::string_view explicitId,
    uint64_t& nextFeatureId,
    std::vector<GeoFeature>& outFeatures
) {
    if (geomNode.type != JsonType::Object) return;
    const JsonValue* typeNode = geomNode.Find("type");
    if (!typeNode || typeNode->type != JsonType::String) return;
    const std::string_view gType = typeNode->strVal;

    if (gType == "GeometryCollection") {
        const JsonValue* geoms = geomNode.Find("geometries");
        if (geoms && geoms->type == JsonType::Array) {
            for (const auto& sub : geoms->arrVal) {
                ExtractFeaturesFromGeometryNode(sub, props, explicitId, nextFeatureId, outFeatures);
            }
        }
        return;
    }

    const JsonValue* coords = geomNode.Find("coordinates");
    if (!coords || coords->type != JsonType::Array) return;

    GeoFeature feat{};
    feat.id = nextFeatureId++;
    feat.attributes = props;
    if (!explicitId.empty()) {
        feat.name = std::string(explicitId);
    }

    if (gType == "Point") {
        feat.geometryType = GeometryType::Point;
        GeoPoint pt{};
        if (ExtractPointCoord(*coords, pt)) {
            feat.points.push_back(pt);
        }
    } else if (gType == "MultiPoint") {
        feat.geometryType = GeometryType::MultiPoint;
        feat.points = ExtractPointList(*coords);
    } else if (gType == "LineString") {
        feat.geometryType = GeometryType::LineString;
        auto line = ExtractPointList(*coords);
        if (line.size() >= 2) {
            feat.lines.push_back(std::move(line));
        }
    } else if (gType == "MultiLineString") {
        feat.geometryType = GeometryType::MultiLineString;
        for (const auto& lineNode : coords->arrVal) {
            auto line = ExtractPointList(lineNode);
            if (line.size() >= 2) {
                feat.lines.push_back(std::move(line));
            }
        }
    } else if (gType == "Polygon") {
        feat.geometryType = GeometryType::Polygon;
        auto poly = ExtractPolygonRings(*coords);
        if (poly.exterior.size() >= 3) {
            feat.polygons.push_back(std::move(poly));
        }
    } else if (gType == "MultiPolygon") {
        feat.geometryType = GeometryType::MultiPolygon;
        for (const auto& polyNode : coords->arrVal) {
            auto poly = ExtractPolygonRings(polyNode);
            if (poly.exterior.size() >= 3) {
                feat.polygons.push_back(std::move(poly));
            }
        }
    } else {
        return;
    }

    if (feat.points.empty() && feat.lines.empty() && feat.polygons.empty()) {
        return;
    }

    PopulateFeatureHydroAttributes(feat);
    outFeatures.push_back(std::move(feat));
}

[[nodiscard]] GisFeatureStyle DefaultStyleForClass(GisFeatureClass cls) noexcept {
    GisFeatureStyle s{};
    switch (cls) {
        case GisFeatureClass::DepthArea:
            s.fillColor = Color::Rgba(120, 200, 240, 220);
            s.strokeColor = Color::Rgba(85, 165, 215, 110);
            s.strokeWidthPx = 0.8f;
            break;
        case GisFeatureClass::LandArea:
            s.fillColor = Color::Rgba(218, 202, 158, 245);
            s.strokeColor = Color::Rgba(120, 102, 68, 220);
            s.strokeWidthPx = 1.4f;
            break;
        case GisFeatureClass::UrbanBuilding:
            s.fillColor = Color::Rgba(192, 175, 140, 235);
            s.strokeColor = Color::Rgba(110, 95, 70, 235);
            s.strokeWidthPx = 1.2f;
            break;
        case GisFeatureClass::RoadRailway:
            s.fillColor = Color::Transparent();
            s.strokeColor = Color::Rgba(145, 125, 95, 210);
            s.strokeWidthPx = 1.6f;
            s.filled = false;
            break;
        case GisFeatureClass::Coastline:
            s.fillColor = Color::Transparent();
            s.strokeColor = Color::Rgba(75, 62, 42, 245);
            s.strokeWidthPx = 2.0f;
            s.filled = false;
            break;
        case GisFeatureClass::DepthContour:
            s.fillColor = Color::Transparent();
            s.strokeColor = Color::Rgba(42, 128, 185, 210);
            s.strokeWidthPx = 1.2f;
            s.filled = false;
            break;
        case GisFeatureClass::Sounding:
            s.fillColor = Color::Rgba(30, 64, 98, 245);
            s.strokeColor = Color::Rgba(255, 255, 255, 210);
            s.pointRadiusPx = 3.0f;
            break;
        case GisFeatureClass::Seamark:
            s.fillColor = Color::Rgba(217, 70, 239, 235);
            s.strokeColor = Color::Rgba(134, 25, 143, 250);
            s.pointRadiusPx = 6.5f;
            break;
        case GisFeatureClass::Light:
            s.fillColor = Color::Rgba(250, 204, 21, 240);
            s.strokeColor = Color::Rgba(217, 119, 6, 250);
            s.pointRadiusPx = 7.0f;
            break;
        case GisFeatureClass::UnderwaterHazard:
            s.fillColor = Color::Rgba(239, 68, 68, 220);
            s.strokeColor = Color::Rgba(153, 27, 27, 250);
            s.pointRadiusPx = 6.0f;
            break;
        case GisFeatureClass::RestrictedArea:
            s.fillColor = Color::Rgba(192, 38, 211, 35);
            s.strokeColor = Color::Rgba(192, 38, 211, 200);
            s.strokeWidthPx = 1.6f;
            s.dashed = true;
            break;
        case GisFeatureClass::Fairway:
            s.fillColor = Color::Rgba(16, 185, 129, 28);
            s.strokeColor = Color::Rgba(16, 185, 129, 210);
            s.strokeWidthPx = 1.8f;
            s.dashed = true;
            break;
        case GisFeatureClass::CablePipeline:
            s.fillColor = Color::Transparent();
            s.strokeColor = Color::Rgba(168, 85, 247, 215);
            s.strokeWidthPx = 1.5f;
            s.dashed = true;
            s.filled = false;
            break;
        case GisFeatureClass::Generic:
            s.fillColor = Color::Rgba(56, 189, 248, 75);
            s.strokeColor = Color::Rgba(2, 132, 199, 230);
            s.strokeWidthPx = 1.6f;
            break;
    }
    return s;
}

[[nodiscard]] std::string EscapeJsonString(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out.push_back(c); break;
        }
    }
    return out;
}

} // namespace

GeoJsonLoadResult GeoJsonIO::ParseFromString(
    std::string_view jsonText,
    std::string_view defaultLayerName,
    bool splitByThematicClass
) {
    GeoJsonLoadResult res{};
    JsonValue root;
    JsonParser parser(jsonText);
    if (!parser.ParseRoot(root, res.errorMessage)) {
        return res;
    }

    if (root.type != JsonType::Object) {
        res.errorMessage = "GeoJSON root must be a JSON Object";
        return res;
    }

    std::vector<GeoFeature> allFeatures;
    uint64_t nextFeatureId = 1;

    auto processFeatureObject = [&](const JsonValue& featObj) {
        if (featObj.type != JsonType::Object) return;
        std::vector<GeoAttribute> props;
        if (const JsonValue* propNode = featObj.Find("properties"); propNode && propNode->type == JsonType::Object) {
            props.reserve(propNode->objVal.size());
            for (const auto& kv : propNode->objVal) {
                if (kv.second.type != JsonType::Null) {
                    props.push_back(GeoAttribute{kv.first, kv.second.ToAttributeString()});
                }
            }
        }
        std::string explicitId;
        if (const JsonValue* idNode = featObj.Find("id"); idNode && idNode->type != JsonType::Null) {
            explicitId = idNode->ToAttributeString();
        }
        if (const JsonValue* geomNode = featObj.Find("geometry"); geomNode && geomNode->type == JsonType::Object) {
            ExtractFeaturesFromGeometryNode(*geomNode, props, explicitId, nextFeatureId, allFeatures);
        }
    };

    const std::string_view rootType = root.Find("type") ? root.Find("type")->AsString() : "";
    if (rootType == "FeatureCollection") {
        if (const JsonValue* feats = root.Find("features"); feats && feats->type == JsonType::Array) {
            allFeatures.reserve(feats->arrVal.size());
            for (const auto& fNode : feats->arrVal) {
                processFeatureObject(fNode);
            }
        }
    } else if (rootType == "Feature") {
        processFeatureObject(root);
    } else {
        // Bare Geometry object
        ExtractFeaturesFromGeometryNode(root, {}, {}, nextFeatureId, allFeatures);
    }

    if (allFeatures.empty()) {
        res.errorMessage = "No valid geographic features found in GeoJSON";
        return res;
    }

    uint32_t nextLayerId = 1;
    if (splitByThematicClass) {
        std::map<GisFeatureClass, GeoLayer> grouped;
        for (auto& feat : allFeatures) {
            auto it = grouped.find(feat.featureClass);
            if (it == grouped.end()) {
                GeoLayer layer{};
                layer.id = nextLayerId++;
                layer.name = std::string(GisFeatureClassName(feat.featureClass));
                layer.dominantClass = feat.featureClass;
                layer.zOrder = DefaultZOrderForClass(feat.featureClass);
                layer.defaultStyle = DefaultStyleForClass(feat.featureClass);
                it = grouped.emplace(feat.featureClass, std::move(layer)).first;
            }
            it->second.features.push_back(std::move(feat));
        }

        for (auto& [cls, layer] : grouped) {
            layer.RecomputeBBox();
            res.totalBounds.Expand(layer.bbox);
            res.totalFeatures += layer.features.size();
            res.totalVertices += layer.TotalVertexCount();
            res.layers.push_back(std::move(layer));
        }
    } else {
        GeoLayer single{};
        single.id = nextLayerId++;
        single.name = std::string(defaultLayerName);
        single.dominantClass = allFeatures.front().featureClass;
        single.zOrder = DefaultZOrderForClass(single.dominantClass);
        single.defaultStyle = DefaultStyleForClass(single.dominantClass);
        single.features = std::move(allFeatures);
        single.RecomputeBBox();

        res.totalBounds = single.bbox;
        res.totalFeatures = single.features.size();
        res.totalVertices = single.TotalVertexCount();
        res.layers.push_back(std::move(single));
    }

    std::sort(res.layers.begin(), res.layers.end(), [](const GeoLayer& a, const GeoLayer& b) {
        return a.zOrder < b.zOrder;
    });

    res.ok = true;
    return res;
}

GeoJsonLoadResult GeoJsonIO::LoadFromFile(
    const std::string& filePath,
    bool splitByThematicClass
) {
    GeoJsonLoadResult res{};
    if (filePath.empty()) {
        res.errorMessage = "Empty file path";
        return res;
    }

    // Extract filename stem as default layer name
    std::string defaultName = filePath;
    if (const size_t slash = defaultName.find_last_of("/\\"); slash != std::string::npos) {
        defaultName = defaultName.substr(slash + 1);
    }

    // 1. Check embedded assets first (works on both Native and WebAssembly!)
    if (auto asset = Assets::Find(filePath); asset && !asset->Empty()) {
        res = ParseFromString(asset->AsStringView(), defaultName, splitByThematicClass);
        const std::string normPath = Assets::NormalizePath(filePath);
        for (auto& layer : res.layers) {
            layer.sourcePath = normPath;
        }
        return res;
    }

    // 2. Fallback to disk file (checking common relative prefixes)
    std::ifstream ifs;
    std::string resolvedPath = filePath;
    const char* prefixes[] = {"", "../", "../../", "../../../"};
    for (const char* prefix : prefixes) {
        std::string candidate = std::string(prefix) + filePath;
        ifs.open(candidate, std::ios::binary);
        if (ifs.is_open()) {
            resolvedPath = std::move(candidate);
            break;
        }
        ifs.clear();
    }

    if (!ifs.is_open()) {
        res.errorMessage = "Cannot open file: " + filePath;
        return res;
    }

    std::ostringstream oss;
    oss << ifs.rdbuf();
    const std::string content = oss.str();

    res = ParseFromString(content, defaultName, splitByThematicClass);
    for (auto& layer : res.layers) {
        layer.sourcePath = resolvedPath;
    }
    return res;
}

std::string GeoJsonIO::SerializeToString(
    const std::vector<GeoLayer>& layers,
    bool prettyPrint
) {
    std::ostringstream out;
    const char* nl = prettyPrint ? "\n" : "";
    const char* sp = prettyPrint ? "  " : "";

    out << "{" << nl;
    out << sp << "\"type\": \"FeatureCollection\"," << nl;
    out << sp << "\"features\": [" << nl;

    bool firstFeature = true;
    char coordBuf[96]{};

    auto writePoint = [&](const GeoPoint& pt) {
        if (pt.hasZ) {
            std::snprintf(coordBuf, sizeof(coordBuf), "[%.6f, %.6f, %.2f]", pt.lon, pt.lat, pt.z);
        } else {
            std::snprintf(coordBuf, sizeof(coordBuf), "[%.6f, %.6f]", pt.lon, pt.lat);
        }
        out << coordBuf;
    };

    auto writeRing = [&](const std::vector<GeoPoint>& ring, bool closeLoop) {
        out << "[";
        for (size_t i = 0; i < ring.size(); ++i) {
            if (i > 0) out << ", ";
            writePoint(ring[i]);
        }
        if (closeLoop && ring.size() >= 3) {
            const auto& f = ring.front();
            const auto& b = ring.back();
            if (std::abs(f.lon - b.lon) > 1e-9 || std::abs(f.lat - b.lat) > 1e-9) {
                out << ", ";
                writePoint(f);
            }
        }
        out << "]";
    };

    for (const auto& layer : layers) {
        for (const auto& feat : layer.features) {
            if (!firstFeature) out << "," << nl;
            firstFeature = false;

            out << sp << sp << "{" << nl;
            out << sp << sp << sp << "\"type\": \"Feature\"," << nl;

            // Properties
            out << sp << sp << sp << "\"properties\": {";
            bool firstProp = true;
            auto emitKV = [&](std::string_view k, std::string_view v) {
                if (!firstProp) out << ", ";
                firstProp = false;
                out << "\"" << EscapeJsonString(k) << "\": \"" << EscapeJsonString(v) << "\"";
            };
            if (!feat.featureCode.empty()) emitKV("OBJL", feat.featureCode);
            if (!feat.name.empty()) emitKV("name", feat.name);
            for (const auto& attr : feat.attributes) {
                if (attr.key == "OBJL" || attr.key == "name") continue;
                emitKV(attr.key, attr.value);
            }
            out << "}," << nl;

            // Geometry
            out << sp << sp << sp << "\"geometry\": { \"type\": \"" << GeometryTypeName(feat.geometryType) << "\", \"coordinates\": ";
            switch (feat.geometryType) {
                case GeometryType::Point:
                    if (!feat.points.empty()) writePoint(feat.points.front());
                    else out << "[0, 0]";
                    break;
                case GeometryType::MultiPoint:
                    writeRing(feat.points, false);
                    break;
                case GeometryType::LineString:
                    if (!feat.lines.empty()) writeRing(feat.lines.front(), false);
                    else out << "[]";
                    break;
                case GeometryType::MultiLineString:
                    out << "[";
                    for (size_t i = 0; i < feat.lines.size(); ++i) {
                        if (i > 0) out << ", ";
                        writeRing(feat.lines[i], false);
                    }
                    out << "]";
                    break;
                case GeometryType::Polygon:
                    out << "[";
                    if (!feat.polygons.empty()) {
                        writeRing(feat.polygons.front().exterior, true);
                        for (const auto& h : feat.polygons.front().holes) {
                            out << ", ";
                            writeRing(h, true);
                        }
                    }
                    out << "]";
                    break;
                case GeometryType::MultiPolygon:
                    out << "[";
                    for (size_t p = 0; p < feat.polygons.size(); ++p) {
                        if (p > 0) out << ", ";
                        out << "[";
                        writeRing(feat.polygons[p].exterior, true);
                        for (const auto& h : feat.polygons[p].holes) {
                            out << ", ";
                            writeRing(h, true);
                        }
                        out << "]";
                    }
                    out << "]";
                    break;
            }
            out << " }" << nl;
            out << sp << sp << "}";
        }
    }

    out << nl << sp << "]" << nl << "}" << nl;
    return out.str();
}

bool GeoJsonIO::SaveToFile(
    const std::string& filePath,
    const std::vector<GeoLayer>& layers,
    bool prettyPrint
) {
    std::ofstream ofs(filePath, std::ios::binary);
    if (!ofs) return false;
    const std::string s = SerializeToString(layers, prettyPrint);
    ofs.write(s.data(), static_cast<std::streamsize>(s.size()));
    return ofs.good();
}

std::vector<GeoLayer> GeoJsonIO::BuildDefaultHydrographicChart() {
    // Load the embedded/disk demo_bay.geojson if available
    auto loaded = LoadFromFile("assets/geo/demo_bay.geojson", true);
    if (loaded.ok && !loaded.layers.empty()) {
        return std::move(loaded.layers);
    }
    return {};
}

} // namespace guikit
