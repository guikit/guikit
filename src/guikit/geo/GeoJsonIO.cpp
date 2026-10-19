#include "guikit/geo/GeoJsonIO.hpp"
#include "guikit/geo/PolygonTriangulator.hpp"
#include "guikit/core/Assets.hpp"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <sstream>
#include <variant>

namespace guikit::geo {

namespace {

// ============================================================================
// Self-Contained RFC 8259 JSON DOM for GeoJSON Parsing
// ============================================================================

struct RawCoordsSlice {
    std::string_view text{};
};

struct JsonValue;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::vector<std::pair<std::string, JsonValue>>;

struct JsonValue {
    using Storage = std::variant<
        std::monostate,
        bool,
        int64_t,
        double,
        std::string,
        JsonArray,
        JsonObject,
        RawCoordsSlice
    >;
    Storage data{std::monostate{}};

    [[nodiscard]] bool IsNull() const noexcept   { return std::holds_alternative<std::monostate>(data); }
    [[nodiscard]] bool IsBool() const noexcept   { return std::holds_alternative<bool>(data); }
    [[nodiscard]] bool IsInt() const noexcept    { return std::holds_alternative<int64_t>(data); }
    [[nodiscard]] bool IsDouble() const noexcept { return std::holds_alternative<double>(data); }
    [[nodiscard]] bool IsNumber() const noexcept { return IsInt() || IsDouble(); }
    [[nodiscard]] bool IsString() const noexcept { return std::holds_alternative<std::string>(data); }
    [[nodiscard]] bool IsArray() const noexcept  { return std::holds_alternative<JsonArray>(data); }
    [[nodiscard]] bool IsObject() const noexcept { return std::holds_alternative<JsonObject>(data); }
    [[nodiscard]] bool IsCoordsSlice() const noexcept { return std::holds_alternative<RawCoordsSlice>(data); }

    [[nodiscard]] double AsDouble(double fallback = 0.0) const noexcept {
        if (const auto* d = std::get_if<double>(&data)) return *d;
        if (const auto* i = std::get_if<int64_t>(&data)) return static_cast<double>(*i);
        return fallback;
    }

    [[nodiscard]] std::string_view AsString(std::string_view fallback = {}) const noexcept {
        if (const auto* s = std::get_if<std::string>(&data)) return *s;
        return fallback;
    }

    [[nodiscard]] const JsonArray* AsArray() const noexcept {
        return std::get_if<JsonArray>(&data);
    }

    [[nodiscard]] const JsonObject* AsObject() const noexcept {
        return std::get_if<JsonObject>(&data);
    }

    [[nodiscard]] std::string_view AsCoordsSlice() const noexcept {
        if (const auto* cs = std::get_if<RawCoordsSlice>(&data)) return cs->text;
        return {};
    }

    [[nodiscard]] const JsonValue* Find(std::string_view key) const noexcept {
        const auto* obj = AsObject();
        if (!obj) return nullptr;
        for (const auto& [k, v] : *obj) {
            if (k == key) return &v;
        }
        return nullptr;
    }
};

void AppendUtf8Codepoint(std::string& out, uint32_t cp) {
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | ((cp >> 6) & 0x1Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | ((cp >> 12) & 0x0Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0x10FFFFu) {
        out.push_back(static_cast<char>(0xF0u | ((cp >> 18) & 0x07u)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

class JsonParser {
public:
    explicit JsonParser(std::string_view src) : m_src(src) {
        // Skip UTF-8 BOM if present
        if (m_src.size() >= 3 &&
            static_cast<uint8_t>(m_src[0]) == 0xEF &&
            static_cast<uint8_t>(m_src[1]) == 0xBB &&
            static_cast<uint8_t>(m_src[2]) == 0xBF) {
            m_pos = 3;
        }
    }

    bool ParseRoot(JsonValue& outRoot, std::string& outError) {
        SkipWhitespace();
        if (AtEnd()) {
            outError = "Empty JSON document";
            return false;
        }
        if (!ParseValue(outRoot, 0)) {
            outError = m_error;
            return false;
        }
        SkipWhitespace();
        return true;
    }

private:
    static constexpr int kMaxDepth = 128;

    [[nodiscard]] bool AtEnd() const noexcept { return m_pos >= m_src.size(); }
    [[nodiscard]] char Peek() const noexcept { return AtEnd() ? '\0' : m_src[m_pos]; }
    char Advance() noexcept {
        char c = Peek();
        if (!AtEnd()) {
            ++m_pos;
            if (c == '\n') {
                ++m_line;
                m_col = 1;
            } else {
                ++m_col;
            }
        }
        return c;
    }

    void SkipWhitespace() noexcept {
        while (m_pos < m_src.size()) {
            const char c = m_src[m_pos];
            if (c == ' ' || c == '\t' || c == '\r') {
                ++m_pos;
                ++m_col;
            } else if (c == '\n') {
                ++m_pos;
                ++m_line;
                m_col = 1;
            } else {
                break;
            }
        }
    }

    bool Fail(std::string_view msg) {
        if (m_error.empty()) {
            m_error = std::format("JSON parse error at line {}, col {}: {}", m_line, m_col, msg);
        }
        return false;
    }

    // Fast zero-allocation scanner for balanced `[...]` coordinate arrays
    bool ParseRawBracketSlice(JsonValue& out) {
        SkipWhitespace();
        if (Peek() != '[') return Fail("Expected '[' for coordinates array");
        const size_t startPos = m_pos;
        int bracketDepth = 0;
        bool inString = false;

        while (m_pos < m_src.size()) {
            const char c = m_src[m_pos++];
            if (c == '\n') {
                ++m_line;
                m_col = 1;
            } else {
                ++m_col;
            }
            if (inString) {
                if (c == '\\' && m_pos < m_src.size()) {
                    ++m_pos;
                    ++m_col;
                } else if (c == '"') {
                    inString = false;
                }
                continue;
            }
            if (c == '"') {
                inString = true;
            } else if (c == '[') {
                ++bracketDepth;
            } else if (c == ']') {
                --bracketDepth;
                if (bracketDepth == 0) {
                    out.data = RawCoordsSlice{m_src.substr(startPos, m_pos - startPos)};
                    return true;
                }
            }
        }
        return Fail("Unterminated '[' in coordinates array");
    }

    bool ParseValue(JsonValue& out, int depth) {
        if (depth > kMaxDepth) {
            return Fail("Maximum nesting depth exceeded");
        }
        SkipWhitespace();
        if (AtEnd()) {
            return Fail("Unexpected end of input");
        }

        const char c = Peek();
        switch (c) {
            case '{': return ParseObject(out, depth + 1);
            case '[': return ParseArray(out, depth + 1);
            case '"': {
                std::string s;
                if (!ParseString(s)) return false;
                out.data = std::move(s);
                return true;
            }
            case 't': return ParseLiteral("true", true, out);
            case 'f': return ParseLiteral("false", false, out);
            case 'n': return ParseNull(out);
            default:
                if (c == '-' || (c >= '0' && c <= '9')) {
                    return ParseNumber(out);
                }
                return Fail(std::format("Unexpected character '{}'", c));
        }
    }

    bool ParseLiteral(std::string_view lit, bool val, JsonValue& out) {
        if (m_src.substr(m_pos, lit.size()) != lit) {
            return Fail(std::format("Expected '{}'", lit));
        }
        for (size_t i = 0; i < lit.size(); ++i) Advance();
        out.data = val;
        return true;
    }

    bool ParseNull(JsonValue& out) {
        if (m_src.substr(m_pos, 4) != "null") {
            return Fail("Expected 'null'");
        }
        for (int i = 0; i < 4; ++i) Advance();
        out.data = std::monostate{};
        return true;
    }

    bool ParseHex4(uint32_t& outVal) {
        if (m_pos + 4 > m_src.size()) {
            return Fail("Incomplete \\uXXXX escape");
        }
        uint32_t val = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = Advance();
            val <<= 4;
            if (c >= '0' && c <= '9') val |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') val |= static_cast<uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val |= static_cast<uint32_t>(c - 'A' + 10);
            else return Fail("Invalid hex digit in \\uXXXX escape");
        }
        outVal = val;
        return true;
    }

    bool ParseString(std::string& out) {
        if (Advance() != '"') return Fail("Expected '\"'");
        out.clear();

        while (!AtEnd()) {
            const char c = Advance();
            if (c == '"') {
                return true;
            }
            if (c == '\\') {
                if (AtEnd()) return Fail("Unterminated escape sequence");
                const char esc = Advance();
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
                        uint32_t cp = 0;
                        if (!ParseHex4(cp)) return false;
                        // Handle UTF-16 surrogate pair
                        if (cp >= 0xD800u && cp <= 0xDBFFu) {
                            if (m_pos + 2 <= m_src.size() && m_src[m_pos] == '\\' && m_src[m_pos + 1] == 'u') {
                                Advance();
                                Advance();
                                uint32_t low = 0;
                                if (!ParseHex4(low)) return false;
                                if (low >= 0xDC00u && low <= 0xDFFFu) {
                                    cp = 0x10000u + (((cp - 0xD800u) << 10) | (low - 0xDC00u));
                                }
                            }
                        }
                        AppendUtf8Codepoint(out, cp);
                        break;
                    }
                    default:
                        out.push_back(esc);
                        break;
                }
            } else {
                out.push_back(c);
            }
        }
        return Fail("Unterminated string literal");
    }

    bool ParseNumber(JsonValue& out) {
        const size_t start = m_pos;
        bool isFloat = false;

        if (Peek() == '-') Advance();
        if (AtEnd()) return Fail("Incomplete number");

        if (Peek() == '0') {
            Advance();
        } else if (Peek() >= '1' && Peek() <= '9') {
            while (Peek() >= '0' && Peek() <= '9') Advance();
        } else {
            return Fail("Invalid number format");
        }

        if (Peek() == '.') {
            isFloat = true;
            Advance();
            if (!(Peek() >= '0' && Peek() <= '9')) {
                return Fail("Expected digit after decimal point");
            }
            while (Peek() >= '0' && Peek() <= '9') Advance();
        }

        if (Peek() == 'e' || Peek() == 'E') {
            isFloat = true;
            Advance();
            if (Peek() == '+' || Peek() == '-') Advance();
            if (!(Peek() >= '0' && Peek() <= '9')) {
                return Fail("Expected digit in exponent");
            }
            while (Peek() >= '0' && Peek() <= '9') Advance();
        }

        const std::string_view token = m_src.substr(start, m_pos - start);
        if (!isFloat) {
            int64_t iVal = 0;
            const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), iVal);
            if (ec == std::errc{} && ptr == token.data() + token.size()) {
                out.data = iVal;
                return true;
            }
        }

        double dVal = 0.0;
        const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), dVal);
        if (ec == std::errc{} && ptr == token.data() + token.size()) {
            out.data = dVal;
            return true;
        }

        return Fail("Invalid numeric literal");
    }

    bool ParseArray(JsonValue& out, int depth) {
        if (Advance() != '[') return Fail("Expected '['");
        JsonArray arr;
        SkipWhitespace();
        if (Peek() == ']') {
            Advance();
            out.data = std::move(arr);
            return true;
        }

        while (true) {
            JsonValue elem;
            if (!ParseValue(elem, depth)) return false;
            arr.push_back(std::move(elem));

            SkipWhitespace();
            const char c = Peek();
            if (c == ',') {
                Advance();
                SkipWhitespace();
                if (Peek() == ']') {
                    Advance();
                    break;
                }
            } else if (c == ']') {
                Advance();
                break;
            } else {
                return Fail("Expected ',' or ']' in array");
            }
        }

        out.data = std::move(arr);
        return true;
    }

    bool ParseObject(JsonValue& out, int depth) {
        if (Advance() != '{') return Fail("Expected '{'");
        JsonObject obj;
        SkipWhitespace();
        if (Peek() == '}') {
            Advance();
            out.data = std::move(obj);
            return true;
        }

        while (true) {
            SkipWhitespace();
            if (Peek() != '"') {
                return Fail("Expected string key in object");
            }
            std::string key;
            if (!ParseString(key)) return false;

            SkipWhitespace();
            if (Advance() != ':') {
                return Fail("Expected ':' after object key");
            }

            SkipWhitespace();
            JsonValue val;
            if (key == "coordinates" && Peek() == '[') {
                if (!ParseRawBracketSlice(val)) return false;
            } else {
                if (!ParseValue(val, depth)) return false;
            }
            obj.emplace_back(std::move(key), std::move(val));

            SkipWhitespace();
            const char c = Peek();
            if (c == ',') {
                Advance();
                SkipWhitespace();
                if (Peek() == '}') {
                    Advance();
                    break;
                }
            } else if (c == '}') {
                Advance();
                break;
            } else {
                return Fail("Expected ',' or '}' in object");
            }
        }

        out.data = std::move(obj);
        return true;
    }

    std::string_view m_src;
    size_t m_pos{0};
    size_t m_line{1};
    size_t m_col{1};
    std::string m_error{};
};

// ============================================================================
// Direct Streaming Coordinate Slice Reader (Zero-DOM Allocation)
// ============================================================================

class CoordSliceReader {
public:
    explicit CoordSliceReader(std::string_view sv, int srsId) noexcept
        : m_sv(sv), m_srsId(srsId) {}

    void SkipWs() noexcept {
        while (m_pos < m_sv.size()) {
            const char c = m_sv[m_pos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++m_pos;
            } else {
                break;
            }
        }
    }

    [[nodiscard]] char Peek() noexcept {
        SkipWs();
        return (m_pos < m_sv.size()) ? m_sv[m_pos] : '\0';
    }

    [[nodiscard]] bool Consume(char expected) noexcept {
        SkipWs();
        if (m_pos < m_sv.size() && m_sv[m_pos] == expected) {
            ++m_pos;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool ReadDouble(double& out) noexcept {
        SkipWs();
        if (m_pos >= m_sv.size()) return false;
        const char* begin = m_sv.data() + m_pos;
        const char* end = m_sv.data() + m_sv.size();
        const auto [ptr, ec] = std::from_chars(begin, end, out);
        if (ec != std::errc{} || ptr == begin) return false;
        m_pos = static_cast<size_t>(ptr - m_sv.data());
        return true;
    }

    // Reads `[lon, lat, ...]` (and optional 3rd Z/depth value)
    [[nodiscard]] bool ReadCoordPair(guikit::GeoCoord& out, double* outOptionalZ = nullptr) noexcept {
        if (!Consume('[')) return false;
        double x = 0.0, y = 0.0;
        if (!ReadDouble(x)) return false;
        if (!Consume(',')) return false;
        if (!ReadDouble(y)) return false;
        bool firstExtra = true;
        while (Peek() == ',') {
            ++m_pos;
            double extra = 0.0;
            if (!ReadDouble(extra)) break;
            if (firstExtra && outOptionalZ) {
                *outOptionalZ = extra;
                firstExtra = false;
            }
        }
        if (!Consume(']')) return false;
        out = SrsTransform::ToWgs84(x, y, m_srsId);
        return true;
    }

    // Reads `[[lon, lat], [lon, lat], ...]`
    [[nodiscard]] bool ReadCoordRing(std::vector<guikit::GeoCoord>& outRing) {
        if (!Consume('[')) return false;
        outRing.clear();
        if (Consume(']')) return true;
        while (true) {
            guikit::GeoCoord pt{};
            if (!ReadCoordPair(pt)) return false;
            outRing.push_back(pt);
            if (Consume(',')) {
                if (Consume(']')) break;
            } else if (Consume(']')) {
                break;
            } else {
                return false;
            }
        }
        return true;
    }

    // Reads `[[[lon, lat], ...], ...]` into LineStrings
    [[nodiscard]] bool ReadMultiLineRings(std::vector<std::vector<guikit::GeoCoord>>& outLines) {
        if (!Consume('[')) return false;
        if (Consume(']')) return true;
        size_t added = 0;
        while (true) {
            std::vector<guikit::GeoCoord> line;
            if (!ReadCoordRing(line)) return false;
            if (line.size() >= 2) {
                outLines.push_back(std::move(line));
                ++added;
            }
            if (Consume(',')) {
                if (Consume(']')) break;
            } else if (Consume(']')) {
                break;
            } else {
                return false;
            }
        }
        return added > 0;
    }

    // Reads `[[[lon, lat], ...], ...]` into one GeoPolygon (exterior + interior holes)
    [[nodiscard]] bool ReadPolygonRings(std::vector<GeoPolygon>& outPolys) {
        if (!Consume('[')) return false;
        if (Consume(']')) return true;

        GeoPolygon poly;
        if (!ReadCoordRing(poly.exteriorRing)) return false;

        while (Consume(',')) {
            if (Consume(']')) {
                if (poly.exteriorRing.size() >= 3) {
                    outPolys.push_back(std::move(poly));
                    return true;
                }
                return false;
            }
            std::vector<guikit::GeoCoord> hole;
            if (!ReadCoordRing(hole)) return false;
            if (hole.size() >= 3) {
                poly.interiorRings.push_back(std::move(hole));
            }
        }

        if (!Consume(']')) return false;
        if (poly.exteriorRing.size() >= 3) {
            outPolys.push_back(std::move(poly));
            return true;
        }
        return false;
    }

    // Reads `[[[[lon, lat], ...], ...], ...]` into multiple GeoPolygons
    [[nodiscard]] bool ReadMultiPolygonRings(std::vector<GeoPolygon>& outPolys) {
        if (!Consume('[')) return false;
        if (Consume(']')) return false;
        const size_t before = outPolys.size();
        while (true) {
            (void)ReadPolygonRings(outPolys);
            if (Consume(',')) {
                if (Consume(']')) break;
            } else if (Consume(']')) {
                break;
            } else {
                return false;
            }
        }
        return outPolys.size() > before;
    }

private:
    std::string_view m_sv;
    int m_srsId{4326};
    size_t m_pos{0};
};

// ============================================================================
// GeoJSON Geometry & Attribute Extraction Helpers
// ============================================================================

[[nodiscard]] std::string CompactJsonValueToString(const JsonValue& v) {
    if (v.IsNull()) return "null";
    if (const auto* b = std::get_if<bool>(&v.data)) return *b ? "true" : "false";
    if (const auto* i = std::get_if<int64_t>(&v.data)) return std::to_string(*i);
    if (const auto* d = std::get_if<double>(&v.data)) {
        char buf[64]{};
        std::snprintf(buf, sizeof(buf), "%.6g", *d);
        return std::string(buf);
    }
    if (const auto* s = std::get_if<std::string>(&v.data)) return *s;
    if (const auto* cs = std::get_if<RawCoordsSlice>(&v.data)) return std::string(cs->text);
    if (const auto* arr = v.AsArray()) {
        std::string out = "[";
        for (size_t i = 0; i < arr->size(); ++i) {
            if (i > 0) out += ", ";
            out += CompactJsonValueToString((*arr)[i]);
        }
        out += "]";
        return out;
    }
    if (const auto* obj = v.AsObject()) {
        std::string out = "{";
        for (size_t i = 0; i < obj->size(); ++i) {
            if (i > 0) out += ", ";
            out += (*obj)[i].first + ": " + CompactJsonValueToString((*obj)[i].second);
        }
        out += "}";
        return out;
    }
    return {};
}

[[nodiscard]] AttributeValue JsonToAttributeValue(const JsonValue& v) {
    if (v.IsNull()) return AttributeValue(nullptr);
    if (const auto* b = std::get_if<bool>(&v.data)) return AttributeValue(*b);
    if (const auto* i = std::get_if<int64_t>(&v.data)) return AttributeValue(*i);
    if (const auto* d = std::get_if<double>(&v.data)) return AttributeValue(*d);
    if (const auto* s = std::get_if<std::string>(&v.data)) return AttributeValue(*s);
    return AttributeValue(CompactJsonValueToString(v));
}

bool ParseGeometryObject(const JsonValue& geomObj, int srsId, GeoFeature& feat, bool isTopLevel = true) {
    if (geomObj.IsNull()) return false;
    const auto* typeVal = geomObj.Find("type");
    if (!typeVal || !typeVal->IsString()) return false;

    const std::string_view gType = typeVal->AsString();

    if (gType == "GeometryCollection") {
        if (isTopLevel) {
            feat.type = GeometryType::GeometryCollection;
        }
        const auto* geomsVal = geomObj.Find("geometries");
        if (geomsVal && geomsVal->IsArray()) {
            bool anyOk = false;
            for (const JsonValue& subGeom : *geomsVal->AsArray()) {
                if (ParseGeometryObject(subGeom, srsId, feat, false)) {
                    anyOk = true;
                }
            }
            return anyOk;
        }
        return false;
    }

    const auto* coordsVal = geomObj.Find("coordinates");
    if (!coordsVal) return false;
    const std::string_view coordsSlice = coordsVal->AsCoordsSlice();
    if (coordsSlice.empty()) return false;

    CoordSliceReader reader(coordsSlice, srsId);

    if (gType == "Point") {
        if (isTopLevel) feat.type = GeometryType::Point;
        guikit::GeoCoord pt{};
        double optZ = std::numeric_limits<double>::quiet_NaN();
        if (reader.ReadCoordPair(pt, &optZ)) {
            feat.points.push_back(pt);
            if (!std::isnan(optZ) && std::isnan(feat.depthMeters)) {
                feat.depthMeters = static_cast<float>(optZ);
            }
            return true;
        }
        return false;
    }

    if (gType == "MultiPoint") {
        if (isTopLevel) feat.type = GeometryType::MultiPoint;
        std::vector<guikit::GeoCoord> pts;
        if (reader.ReadCoordRing(pts) && !pts.empty()) {
            for (const auto& pt : pts) {
                feat.points.push_back(pt);
            }
            return true;
        }
        return false;
    }

    if (gType == "LineString") {
        if (isTopLevel) feat.type = GeometryType::LineString;
        std::vector<guikit::GeoCoord> line;
        if (reader.ReadCoordRing(line) && line.size() >= 2) {
            feat.lines.push_back(std::move(line));
            return true;
        }
        return false;
    }

    if (gType == "MultiLineString") {
        if (isTopLevel) feat.type = GeometryType::MultiLineString;
        return reader.ReadMultiLineRings(feat.lines);
    }

    if (gType == "Polygon") {
        if (isTopLevel) feat.type = GeometryType::Polygon;
        return reader.ReadPolygonRings(feat.polygons);
    }

    if (gType == "MultiPolygon") {
        if (isTopLevel) feat.type = GeometryType::MultiPolygon;
        return reader.ReadMultiPolygonRings(feat.polygons);
    }

    return false;
}

int DetectGeoJsonSrsId(const JsonValue& rootObj) {
    const auto* crsVal = rootObj.Find("crs");
    if (!crsVal || !crsVal->IsObject()) return 4326;
    const auto* propsVal = crsVal->Find("properties");
    if (!propsVal || !propsVal->IsObject()) return 4326;
    const auto* nameVal = propsVal->Find("name");
    if (!nameVal || !nameVal->IsString()) return 4326;

    const std::string_view crsName = nameVal->AsString();
    if (crsName.find("3857") != std::string_view::npos || crsName.find("900913") != std::string_view::npos) {
        return 3857;
    }
    if (crsName.find("3395") != std::string_view::npos) {
        return 3395;
    }
    return 4326;
}

void ParseFeatureObject(const JsonValue& featObj, int srsId, std::vector<GeoFeature>& outFeatures) {
    if (!featObj.IsObject()) return;

    GeoFeature feat;

    if (const auto* idVal = featObj.Find("id")) {
        if (idVal->IsString()) {
            feat.displayId = std::string(idVal->AsString());
        } else if (const auto* i = std::get_if<int64_t>(&idVal->data)) {
            feat.displayId = std::to_string(*i);
        } else if (const auto* d = std::get_if<double>(&idVal->data)) {
            feat.displayId = std::format("{}", *d);
        }
    }

    if (const auto* propsVal = featObj.Find("properties")) {
        if (const auto* propsObj = propsVal->AsObject()) {
            feat.properties.reserve(propsObj->size());
            for (const auto& [k, v] : *propsObj) {
                if (v.IsNull()) continue;
                feat.properties.emplace_back(k, JsonToAttributeValue(v));
            }
        }
    }

    if (const auto* geomVal = featObj.Find("geometry")) {
        if (ParseGeometryObject(*geomVal, srsId, feat, true)) {
            outFeatures.push_back(std::move(feat));
        }
    }
}

void EscapeJsonString(std::string& out, std::string_view sv) {
    out.push_back('"');
    for (char c : sv) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20u) {
                    char buf[8]{};
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out.push_back(c);
                }
                break;
        }
    }
    out.push_back('"');
}

void AppendRingJson(std::string& out, const std::vector<guikit::GeoCoord>& ring, bool closeRing) {
    out.push_back('[');
    for (size_t i = 0; i < ring.size(); ++i) {
        if (i > 0) out += ", ";
        char buf[64]{};
        std::snprintf(buf, sizeof(buf), "[%.6f, %.6f]", ring[i].lon, ring[i].lat);
        out += buf;
    }
    if (closeRing && !ring.empty()) {
        const auto& f = ring.front();
        const auto& b = ring.back();
        if (std::abs(f.lon - b.lon) > 1e-9 || std::abs(f.lat - b.lat) > 1e-9) {
            char buf[64]{};
            std::snprintf(buf, sizeof(buf), ", [%.6f, %.6f]", f.lon, f.lat);
            out += buf;
        }
    }
    out.push_back(']');
}

[[nodiscard]] int CartographicSortRank(ChartFeatureClass cls) noexcept {
    switch (cls) {
        case ChartFeatureClass::CoverageMask:       return 5;
        case ChartFeatureClass::DepthArea:          return 10;
        case ChartFeatureClass::SeabedOrMarineArea: return 20;
        case ChartFeatureClass::FairwayArea:        return 25;
        case ChartFeatureClass::LandArea:           return 30;
        case ChartFeatureClass::BuiltUpOrStructure: return 40;
        case ChartFeatureClass::SubmarineCablePipe: return 50;
        case ChartFeatureClass::DepthContour:       return 55;
        case ChartFeatureClass::Coastline:          return 60;
        case ChartFeatureClass::NavigationLine:     return 65;
        case ChartFeatureClass::Generic:            return 70;
        case ChartFeatureClass::Sounding:           return 80;
        case ChartFeatureClass::UnderwaterHazard:   return 85;
        case ChartFeatureClass::BuoyOrBeacon:       return 90;
        case ChartFeatureClass::TextLabel:          return 95;
    }
    return 70;
}

[[nodiscard]] LayerStyle DefaultStyleForChartClass(ChartFeatureClass cls, size_t idx) noexcept {
    LayerStyle s{};
    s.primaryColor = SrsTransform::DefaultLayerColor(idx);
    switch (cls) {
        case ChartFeatureClass::DepthArea:
            s.primaryColor = guikit::Color(56, 189, 248, 255);
            s.fillOpacity = 1.0f;
            s.renderStroke = false;
            s.showLabels = false;
            break;
        case ChartFeatureClass::LandArea:
            s.primaryColor = guikit::Color(214, 197, 152, 255);
            s.fillOpacity = 1.0f;
            s.strokeWidthPx = 1.4f;
            break;
        case ChartFeatureClass::Coastline:
            s.primaryColor = guikit::Color(92, 76, 52, 255);
            s.renderFill = false;
            s.strokeWidthPx = 2.0f;
            break;
        case ChartFeatureClass::DepthContour:
            s.primaryColor = guikit::Color(14, 116, 144, 255);
            s.renderFill = false;
            s.strokeWidthPx = 1.2f;
            break;
        case ChartFeatureClass::Sounding:
            s.primaryColor = guikit::Color(3, 105, 161, 255);
            s.pointRadiusPx = 3.5f;
            break;
        case ChartFeatureClass::BuoyOrBeacon:
            s.primaryColor = guikit::Color(232, 121, 249, 255);
            s.pointRadiusPx = 6.5f;
            s.renderAsSeamark = true;
            break;
        case ChartFeatureClass::UnderwaterHazard:
            s.primaryColor = guikit::Color(239, 68, 68, 255);
            s.pointRadiusPx = 6.0f;
            s.renderAsSeamark = true;
            break;
        default:
            break;
    }
    return s;
}

bool TryReadTextFromAssetsOrDisk(
    const std::string& filePath,
    std::string& outContent,
    std::string& outResolvedPath,
    std::string& outError
) {
    if (filePath.empty()) {
        outError = "Empty file path";
        return false;
    }

    // 1. Check embedded assets first (Native + WebAssembly)
    if (auto asset = guikit::Assets::Find(filePath); asset && !asset->Empty()) {
        outContent = std::string(asset->AsStringView());
        outResolvedPath = guikit::Assets::NormalizePath(filePath);
        return true;
    }

    // 2. Check disk (direct path first, then relative parent prefixes for demo working directories)
    const char* prefixes[] = {"", "../", "../../", "../../../"};
    for (const char* prefix : prefixes) {
        const std::string candidate = std::string(prefix) + filePath;
        std::ifstream ifs(std::filesystem::u8path(candidate), std::ios::binary | std::ios::ate);
        if (!ifs) continue;

        const auto endPos = ifs.tellg();
        outContent.clear();
        if (endPos > 0) {
            outContent.resize(static_cast<size_t>(endPos));
            ifs.seekg(0, std::ios::beg);
            ifs.read(outContent.data(), static_cast<std::streamsize>(outContent.size()));
        }
        outResolvedPath = candidate;
        return true;
    }

    outError = std::format("Cannot open GeoJSON file: {}", filePath);
    return false;
}

} // namespace

GeoJsonResult GeoJsonIO::ParseFromString(std::string_view jsonText, std::string_view defaultLayerName) {
    GeoJsonResult res;
    JsonValue root;
    JsonParser parser(jsonText);
    if (!parser.ParseRoot(root, res.error)) {
        return res;
    }

    if (!root.IsObject()) {
        res.error = "GeoJSON root must be a JSON object";
        return res;
    }

    res.layer.name = std::string(defaultLayerName);
    res.layer.format = "GeoJSON";
    res.layer.srsId = DetectGeoJsonSrsId(root);

    if (const auto* nameVal = root.Find("name"); nameVal && nameVal->IsString()) {
        const std::string_view docName = nameVal->AsString();
        if (!docName.empty()) {
            res.layer.name = std::string(docName);
        }
    }

    const auto* typeVal = root.Find("type");
    if (!typeVal || !typeVal->IsString()) {
        res.error = "Missing 'type' property on GeoJSON root object";
        return res;
    }

    const std::string_view rootType = typeVal->AsString();
    if (rootType == "FeatureCollection") {
        const auto* featsVal = root.Find("features");
        if (!featsVal || !featsVal->IsArray()) {
            res.error = "FeatureCollection is missing 'features' array";
            return res;
        }
        const auto* featsArr = featsVal->AsArray();
        res.layer.features.reserve(featsArr->size());
        for (const JsonValue& featVal : *featsArr) {
            ParseFeatureObject(featVal, res.layer.srsId, res.layer.features);
        }
    } else if (rootType == "Feature") {
        ParseFeatureObject(root, res.layer.srsId, res.layer.features);
    } else {
        GeoFeature singleFeat;
        if (ParseGeometryObject(root, res.layer.srsId, singleFeat, true)) {
            res.layer.features.push_back(std::move(singleFeat));
        } else {
            res.error = std::format("Unsupported or empty GeoJSON root type '{}'", rootType);
            return res;
        }
    }

    res.layer.FinalizeLayer(1);
    res.ok = true;
    return res;
}

GeoJsonResult GeoJsonIO::LoadFromFile(const std::string& filePath) {
    GeoJsonResult res;
    std::string content;
    std::string resolvedPath;
    if (!TryReadTextFromAssetsOrDisk(filePath, content, resolvedPath, res.error)) {
        return res;
    }

    std::string stem = std::filesystem::u8path(filePath).stem().string();
    if (stem.empty()) stem = "GeoJSON Layer";

    res = ParseFromString(content, stem);
    res.layer.sourcePath = resolvedPath;
    return res;
}

GeoJsonLoadResult GeoJsonIO::ParseThematicLayersFromString(
    std::string_view jsonText,
    std::string_view defaultLayerName,
    bool splitByThematicClass
) {
    GeoJsonLoadResult res{};
    GeoJsonResult singleRes = ParseFromString(jsonText, defaultLayerName);
    if (!singleRes.ok) {
        res.errorMessage = singleRes.error;
        return res;
    }

    if (singleRes.layer.features.empty()) {
        res.errorMessage = "No valid geographic features found in GeoJSON";
        return res;
    }

    if (!splitByThematicClass) {
        singleRes.layer.style = DefaultStyleForChartClass(singleRes.layer.dominantChartClass, 0);
        res.totalBounds = singleRes.layer.bounds;
        res.hasValidBounds = singleRes.layer.hasValidBounds;
        res.totalFeatures = singleRes.layer.features.size();
        res.totalVertices = singleRes.layer.totalVertices;
        res.layers.push_back(std::move(singleRes.layer));
        res.ok = true;
        return res;
    }

    std::map<int, GeoLayer> grouped;
    for (GeoFeature& feat : singleRes.layer.features) {
        const int key = CartographicSortRank(feat.chartClass) * 100 + static_cast<int>(feat.chartClass);
        auto it = grouped.find(key);
        if (it == grouped.end()) {
            GeoLayer lyr{};
            lyr.name = std::string(ChartFeatureClassToString(feat.chartClass));
            lyr.format = "GeoJSON";
            lyr.srsId = singleRes.layer.srsId;
            lyr.dominantChartClass = feat.chartClass;
            it = grouped.emplace(key, std::move(lyr)).first;
        }
        it->second.features.push_back(std::move(feat));
    }

    uint64_t nextLayerUid = 1;
    size_t styleIdx = 0;
    for (auto& [rankKey, layer] : grouped) {
        layer.style = DefaultStyleForChartClass(layer.dominantChartClass, styleIdx++);
        layer.FinalizeLayer(nextLayerUid++);
        if (layer.hasValidBounds) {
            SrsTransform::UnionBounds(res.totalBounds, res.hasValidBounds, layer.bounds);
        }
        res.totalFeatures += layer.features.size();
        res.totalVertices += layer.totalVertices;
        res.layers.push_back(std::move(layer));
    }

    res.ok = true;
    return res;
}

GeoJsonLoadResult GeoJsonIO::LoadThematicLayersFromFile(
    const std::string& filePath,
    bool splitByThematicClass
) {
    GeoJsonLoadResult res{};
    std::string content;
    std::string resolvedPath;
    if (!TryReadTextFromAssetsOrDisk(filePath, content, resolvedPath, res.errorMessage)) {
        return res;
    }

    std::string stem = std::filesystem::u8path(filePath).stem().string();
    if (stem.empty()) stem = "GeoJSON Layer";

    res = ParseThematicLayersFromString(content, stem, splitByThematicClass);
    for (auto& layer : res.layers) {
        layer.sourcePath = resolvedPath;
    }
    return res;
}

std::string GeoJsonIO::SerializeLayer(const GeoLayer& layer, bool prettyPrint) {
    const std::string_view nl = prettyPrint ? "\n" : "";
    const std::string_view ind1 = prettyPrint ? "  " : "";
    const std::string_view ind2 = prettyPrint ? "    " : "";
    const std::string_view ind3 = prettyPrint ? "      " : "";

    std::string out;
    out.reserve(layer.features.size() * 256 + 128);
    out += "{";
    out += nl;
    out += ind1;
    out += "\"type\": \"FeatureCollection\",";
    out += nl;
    out += ind1;
    out += "\"name\": ";
    EscapeJsonString(out, layer.name);
    out += ",";
    out += nl;
    out += ind1;
    out += "\"features\": [";
    out += nl;

    for (size_t fi = 0; fi < layer.features.size(); ++fi) {
        const GeoFeature& feat = layer.features[fi];
        (void)feat.EnsureGeometryLoaded();
        (void)feat.EnsurePropertiesLoaded();
        out += ind2;
        out += "{";
        out += nl;
        out += ind3;
        out += "\"type\": \"Feature\",";
        out += nl;

        if (!feat.displayId.empty()) {
            out += ind3;
            out += "\"id\": ";
            EscapeJsonString(out, feat.displayId);
            out += ",";
            out += nl;
        }

        // Properties
        out += ind3;
        out += "\"properties\": {";
        for (size_t pi = 0; pi < feat.properties.size(); ++pi) {
            if (pi > 0) out += ", ";
            const auto& [k, v] = feat.properties[pi];
            EscapeJsonString(out, k);
            out += ": ";
            if (v.IsNull()) {
                out += "null";
            } else if (const auto* b = std::get_if<bool>(&v.value)) {
                out += (*b ? "true" : "false");
            } else if (const auto* i = std::get_if<int64_t>(&v.value)) {
                out += std::to_string(*i);
            } else if (const auto* d = std::get_if<double>(&v.value)) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "%.6g", *d);
                out += buf;
            } else if (const auto* s = std::get_if<std::string>(&v.value)) {
                EscapeJsonString(out, *s);
            }
        }
        out += "},";
        out += nl;

        // Geometry
        out += ind3;
        out += "\"geometry\": ";
        if (!feat.polygons.empty()) {
            if (feat.polygons.size() == 1 && feat.type != GeometryType::MultiPolygon) {
                out += "{\"type\": \"Polygon\", \"coordinates\": [";
                AppendRingJson(out, feat.polygons[0].exteriorRing, true);
                for (const auto& hole : feat.polygons[0].interiorRings) {
                    out += ", ";
                    AppendRingJson(out, hole, true);
                }
                out += "]}";
            } else {
                out += "{\"type\": \"MultiPolygon\", \"coordinates\": [";
                for (size_t p = 0; p < feat.polygons.size(); ++p) {
                    if (p > 0) out += ", ";
                    out += "[";
                    AppendRingJson(out, feat.polygons[p].exteriorRing, true);
                    for (const auto& hole : feat.polygons[p].interiorRings) {
                        out += ", ";
                        AppendRingJson(out, hole, true);
                    }
                    out += "]";
                }
                out += "]}";
            }
        } else if (!feat.lines.empty()) {
            if (feat.lines.size() == 1 && feat.type != GeometryType::MultiLineString) {
                out += "{\"type\": \"LineString\", \"coordinates\": ";
                AppendRingJson(out, feat.lines[0], false);
                out += "}";
            } else {
                out += "{\"type\": \"MultiLineString\", \"coordinates\": [";
                for (size_t l = 0; l < feat.lines.size(); ++l) {
                    if (l > 0) out += ", ";
                    AppendRingJson(out, feat.lines[l], false);
                }
                out += "]}";
            }
        } else if (!feat.points.empty()) {
            if (feat.points.size() == 1 && feat.type != GeometryType::MultiPoint) {
                char buf[96]{};
                std::snprintf(
                    buf,
                    sizeof(buf),
                    "{\"type\": \"Point\", \"coordinates\": [%.6f, %.6f]}",
                    feat.points[0].lon,
                    feat.points[0].lat
                );
                out += buf;
            } else {
                out += "{\"type\": \"MultiPoint\", \"coordinates\": ";
                AppendRingJson(out, feat.points, false);
                out += "}";
            }
        } else {
            out += "null";
        }

        out += nl;
        out += ind2;
        out += "}";
        if (fi + 1 < layer.features.size()) {
            out += ",";
        }
        out += nl;
    }

    out += ind1;
    out += "]";
    out += nl;
    out += "}";
    out += nl;
    return out;
}

bool GeoJsonIO::SaveToFile(const GeoLayer& layer, const std::string& filePath, std::string* outError) {
    std::ofstream ofs(std::filesystem::u8path(filePath), std::ios::binary | std::ios::trunc);
    if (!ofs) {
        if (outError) {
            *outError = std::format("Cannot open file for writing: {}", filePath);
        }
        return false;
    }
    const std::string json = SerializeLayer(layer, true);
    ofs.write(json.data(), static_cast<std::streamsize>(json.size()));
    return ofs.good();
}

std::vector<GeoLayer> GeoJsonIO::BuildDefaultHydrographicChart() {
    auto loaded = LoadThematicLayersFromFile("assets/geo/demo_bay.geojson", true);
    if (loaded.ok && !loaded.layers.empty()) {
        return std::move(loaded.layers);
    }
    return {};
}

} // namespace guikit::geo
