#include "guikit/geo/geo_json_io.hpp"
#include "guikit/core/assets.hpp"
#include "guikit/geo/polygon_triangulator.hpp"
#include <cmath>
#include "keel/algo.h"
#include "keel/arena.h"
#include "keel/format.h"
#include "keel/fs.h"
#include "keel/number.h"
#include "keel/small_array.h"

namespace guikit::geo {

namespace {

// ============================================================================
// Self-Contained Arena-Backed RFC 8259 JSON DOM for GeoJSON Parsing
// ============================================================================

struct JsonKeyValue;

struct JsonValue {
    enum class Kind : keel::u8 {
        Null = 0,
        Bool,
        Int,
        Double,
        String,
        Array,
        Object,
        CoordsSlice
    };

    Kind kind{Kind::Null};
    union {
        bool boolVal;
        keel::i64 intVal;
        double doubleVal{0.0};
    };
    keel::StringRef textVal{};
    keel::ArrayRef<JsonValue> arrVal{};
    keel::ArrayRef<JsonKeyValue> objVal{};

    [[nodiscard]] constexpr bool IsNull() const noexcept        { return kind == Kind::Null; }
    [[nodiscard]] constexpr bool IsBool() const noexcept        { return kind == Kind::Bool; }
    [[nodiscard]] constexpr bool IsInt() const noexcept         { return kind == Kind::Int; }
    [[nodiscard]] constexpr bool IsDouble() const noexcept      { return kind == Kind::Double; }
    [[nodiscard]] constexpr bool IsNumber() const noexcept      { return IsInt() || IsDouble(); }
    [[nodiscard]] constexpr bool IsString() const noexcept      { return kind == Kind::String; }
    [[nodiscard]] constexpr bool IsArray() const noexcept       { return kind == Kind::Array; }
    [[nodiscard]] constexpr bool IsObject() const noexcept      { return kind == Kind::Object; }
    [[nodiscard]] constexpr bool IsCoordsSlice() const noexcept { return kind == Kind::CoordsSlice; }

    [[nodiscard]] constexpr double AsDouble(double fallback = 0.0) const noexcept {
        if (kind == Kind::Double) return doubleVal;
        if (kind == Kind::Int) return static_cast<double>(intVal);
        return fallback;
    }

    [[nodiscard]] constexpr keel::StringRef AsString(keel::StringRef fallback = {}) const noexcept {
        if (kind == Kind::String) return textVal;
        return fallback;
    }

    [[nodiscard]] constexpr keel::StringRef AsCoordsSlice() const noexcept {
        if (kind == Kind::CoordsSlice) return textVal;
        return {};
    }

    [[nodiscard]] const JsonValue* Find(keel::StringRef key) const noexcept;
};

struct JsonKeyValue {
    keel::StringRef key{};
    JsonValue value{};
};

const JsonValue* JsonValue::Find(keel::StringRef key) const noexcept {
    if (kind != Kind::Object) return nullptr;
    for (const auto& kv : objVal) {
        if (kv.key == key) return &kv.value;
    }
    return nullptr;
}

void AppendUtf8Codepoint(keel::String& out, keel::u32 cp) {
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
    JsonParser(keel::StringRef src, keel::Arena& arena)
        : m_src(src), m_arena(arena) {
        // Skip UTF-8 BOM if present
        if (m_src.size() >= 3 &&
            static_cast<keel::u8>(m_src[0]) == 0xEF &&
            static_cast<keel::u8>(m_src[1]) == 0xBB &&
            static_cast<keel::u8>(m_src[2]) == 0xBF) {
            m_pos = 3;
        }
    }

    bool ParseRoot(JsonValue& outRoot, keel::String& outError) {
        SkipWhitespace();
        if (AtEnd()) {
            outError.assign("Empty JSON document");
            return false;
        }
        if (!ParseValue(outRoot, 0)) {
            outError = keel::move(m_error);
            return false;
        }
        SkipWhitespace();
        return true;
    }

private:
    static constexpr keel::i32 kMaxDepth = 128;

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

    bool Fail(keel::StringRef msg) {
        if (m_error.empty()) {
            m_error = keel::format("JSON parse error at line %zu, col %zu: %s", m_line, m_col, msg);
        }
        return false;
    }

    // Fast zero-allocation scanner for balanced `[...]` coordinate arrays
    bool ParseRawBracketSlice(JsonValue& out) {
        SkipWhitespace();
        if (Peek() != '[') return Fail("Expected '[' for coordinates array");
        const keel::usize startPos = m_pos;
        keel::i32 bracketDepth = 0;
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
                    out = JsonValue{};
                    out.kind = JsonValue::Kind::CoordsSlice;
                    out.textVal = m_src.substr(startPos, m_pos - startPos);
                    return true;
                }
            }
        }
        return Fail("Unterminated '[' in coordinates array");
    }

    bool ParseValue(JsonValue& out, keel::i32 depth) {
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
                keel::StringRef s;
                if (!ParseString(s)) return false;
                out = JsonValue{};
                out.kind = JsonValue::Kind::String;
                out.textVal = s;
                return true;
            }
            case 't': return ParseLiteral("true", true, out);
            case 'f': return ParseLiteral("false", false, out);
            case 'n': return ParseNull(out);
            default:
                if (c == '-' || (c >= '0' && c <= '9')) {
                    return ParseNumber(out);
                }
                {
                    char buf[64];
                    return Fail(keel::format_buf(buf, "Unexpected character '%c'", c));
                }
        }
    }

    bool ParseLiteral(keel::StringRef lit, bool val, JsonValue& out) {
        if (m_src.substr(m_pos, lit.size()) != lit) {
            char buf[64];
            return Fail(keel::format_buf(buf, "Expected '%s'", lit));
        }
        for (keel::usize i = 0; i < lit.size(); ++i) Advance();
        out = JsonValue{};
        out.kind = JsonValue::Kind::Bool;
        out.boolVal = val;
        return true;
    }

    bool ParseNull(JsonValue& out) {
        if (m_src.substr(m_pos, 4) != "null") {
            return Fail("Expected 'null'");
        }
        for (keel::i32 i = 0; i < 4; ++i) Advance();
        out = JsonValue{};
        out.kind = JsonValue::Kind::Null;
        return true;
    }

    bool ParseHex4(keel::u32& outVal) {
        if (m_pos + 4 > m_src.size()) {
            return Fail("Incomplete \\uXXXX escape");
        }
        keel::u32 val = 0;
        for (keel::i32 i = 0; i < 4; ++i) {
            const char c = Advance();
            val <<= 4;
            if (c >= '0' && c <= '9') val |= static_cast<keel::u32>(c - '0');
            else if (c >= 'a' && c <= 'f') val |= static_cast<keel::u32>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') val |= static_cast<keel::u32>(c - 'A' + 10);
            else return Fail("Invalid hex digit in \\uXXXX escape");
        }
        outVal = val;
        return true;
    }

    bool ParseString(keel::StringRef& out) {
        if (Advance() != '"') return Fail("Expected '\"'");
        const keel::usize rawStart = m_pos;

        // Fast zero-copy scan for unescaped strings (direct slice into m_src!)
        bool hasEscape = false;
        keel::usize scanPos = m_pos;
        while (scanPos < m_src.size()) {
            const char sc = m_src[scanPos];
            if (sc == '"') {
                const keel::usize len = scanPos - rawStart;
                for (keel::usize k = 0; k <= len; ++k) Advance();
                out = m_src.substr(rawStart, len);
                return true;
            }
            if (sc == '\\') {
                hasEscape = true;
                break;
            }
            ++scanPos;
        }
        if (!hasEscape && scanPos >= m_src.size()) {
            return Fail("Unterminated string literal");
        }

        m_scratchStr.clear();
        while (!AtEnd()) {
            const char c = Advance();
            if (c == '"') {
                out = m_arena.alloc_string(m_scratchStr.as_ref());
                return true;
            }
            if (c == '\\') {
                if (AtEnd()) return Fail("Unterminated escape sequence");
                const char esc = Advance();
                switch (esc) {
                    case '"':  m_scratchStr.push_back('"'); break;
                    case '\\': m_scratchStr.push_back('\\'); break;
                    case '/':  m_scratchStr.push_back('/'); break;
                    case 'b':  m_scratchStr.push_back('\b'); break;
                    case 'f':  m_scratchStr.push_back('\f'); break;
                    case 'n':  m_scratchStr.push_back('\n'); break;
                    case 'r':  m_scratchStr.push_back('\r'); break;
                    case 't':  m_scratchStr.push_back('\t'); break;
                    case 'u': {
                        keel::u32 cp = 0;
                        if (!ParseHex4(cp)) return false;
                        // Handle UTF-16 surrogate pair
                        if (cp >= 0xD800u && cp <= 0xDBFFu) {
                            if (m_pos + 2 <= m_src.size() && m_src[m_pos] == '\\' && m_src[m_pos + 1] == 'u') {
                                Advance();
                                Advance();
                                keel::u32 low = 0;
                                if (!ParseHex4(low)) return false;
                                if (low >= 0xDC00u && low <= 0xDFFFu) {
                                    cp = 0x10000u + (((cp - 0xD800u) << 10) | (low - 0xDC00u));
                                }
                            }
                        }
                        AppendUtf8Codepoint(m_scratchStr, cp);
                        break;
                    }
                    default:
                        m_scratchStr.push_back(esc);
                        break;
                }
            } else {
                m_scratchStr.push_back(c);
            }
        }
        return Fail("Unterminated string literal");
    }

    bool ParseNumber(JsonValue& out) {
        const keel::usize start = m_pos;
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

        const keel::StringRef token = m_src.substr(start, m_pos - start);
        if (!isFloat) {
            if (const auto res = keel::parse_i64(token); res.is_ok()) {
                out = JsonValue{};
                out.kind = JsonValue::Kind::Int;
                out.intVal = static_cast<keel::i64>(res.value());
                return true;
            }
        }

        if (const auto res = keel::parse_f64(token); res.is_ok()) {
            out = JsonValue{};
            out.kind = JsonValue::Kind::Double;
            out.doubleVal = res.value();
            return true;
        }

        return Fail("Invalid numeric literal");
    }

    bool ParseArray(JsonValue& out, keel::i32 depth) {
        if (Advance() != '[') return Fail("Expected '['");
        SkipWhitespace();
        if (Peek() == ']') {
            Advance();
            out = JsonValue{};
            out.kind = JsonValue::Kind::Array;
            return true;
        }

        keel::SmallArray<JsonValue, 16> elems;
        while (true) {
            JsonValue elem{};
            if (!ParseValue(elem, depth)) return false;
            elems.push_back(elem);

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

        JsonValue* dst = m_arena.alloc_array<JsonValue>(elems.size());
        for (keel::usize i = 0; i < elems.size(); ++i) {
            dst[i] = elems[i];
        }
        out = JsonValue{};
        out.kind = JsonValue::Kind::Array;
        out.arrVal = keel::ArrayRef<JsonValue>(dst, elems.size());
        return true;
    }

    bool ParseObject(JsonValue& out, keel::i32 depth) {
        if (Advance() != '{') return Fail("Expected '{'");
        SkipWhitespace();
        if (Peek() == '}') {
            Advance();
            out = JsonValue{};
            out.kind = JsonValue::Kind::Object;
            return true;
        }

        keel::SmallArray<JsonKeyValue, 16> pairs;
        while (true) {
            SkipWhitespace();
            if (Peek() != '"') {
                return Fail("Expected string key in object");
            }
            keel::StringRef key;
            if (!ParseString(key)) return false;

            SkipWhitespace();
            if (Advance() != ':') {
                return Fail("Expected ':' after object key");
            }

            SkipWhitespace();
            JsonValue val{};
            if (key == "coordinates" && Peek() == '[') {
                if (!ParseRawBracketSlice(val)) return false;
            } else {
                if (!ParseValue(val, depth)) return false;
            }
            pairs.push_back(JsonKeyValue{key, val});

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

        JsonKeyValue* dst = m_arena.alloc_array<JsonKeyValue>(pairs.size());
        for (keel::usize i = 0; i < pairs.size(); ++i) {
            dst[i] = pairs[i];
        }
        out = JsonValue{};
        out.kind = JsonValue::Kind::Object;
        out.objVal = keel::ArrayRef<JsonKeyValue>(dst, pairs.size());
        return true;
    }

    keel::StringRef m_src;
    keel::Arena& m_arena;
    keel::usize m_pos{0};
    keel::usize m_line{1};
    keel::usize m_col{1};
    keel::String m_scratchStr{};
    keel::String m_error{};
};

// ============================================================================
// Direct Streaming Coordinate Slice Reader (Zero-DOM Allocation)
// ============================================================================

class CoordSliceReader {
public:
    explicit CoordSliceReader(keel::StringRef sv, keel::i32 srsId) noexcept
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
        const keel::usize start = m_pos;
        if (m_sv[m_pos] == '+' || m_sv[m_pos] == '-') ++m_pos;
        while (m_pos < m_sv.size()) {
            const char c = m_sv[m_pos];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
                ++m_pos;
            } else {
                break;
            }
        }
        if (m_pos == start) return false;
        const auto res = keel::parse_f64(m_sv.substr(start, m_pos - start));
        if (!res.is_ok()) return false;
        out = res.value();
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
    [[nodiscard]] bool ReadCoordRing(keel::Array<guikit::GeoCoord>& outRing) {
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
    [[nodiscard]] bool ReadMultiLineRings(keel::Array<keel::Array<guikit::GeoCoord>>& outLines) {
        if (!Consume('[')) return false;
        if (Consume(']')) return true;
        keel::usize added = 0;
        while (true) {
            keel::Array<guikit::GeoCoord> line;
            if (!ReadCoordRing(line)) return false;
            if (line.size() >= 2) {
                outLines.push_back(keel::move(line));
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
    [[nodiscard]] bool ReadPolygonRings(keel::Array<GeoPolygon>& outPolys) {
        if (!Consume('[')) return false;
        if (Consume(']')) return true;

        GeoPolygon poly;
        if (!ReadCoordRing(poly.exteriorRing)) return false;

        while (Consume(',')) {
            if (Consume(']')) {
                if (poly.exteriorRing.size() >= 3) {
                    outPolys.push_back(keel::move(poly));
                    return true;
                }
                return false;
            }
            keel::Array<guikit::GeoCoord> hole;
            if (!ReadCoordRing(hole)) return false;
            if (hole.size() >= 3) {
                poly.interiorRings.push_back(keel::move(hole));
            }
        }

        if (!Consume(']')) return false;
        if (poly.exteriorRing.size() >= 3) {
            outPolys.push_back(keel::move(poly));
            return true;
        }
        return false;
    }

    // Reads `[[[[lon, lat], ...], ...], ...]` into multiple GeoPolygons
    [[nodiscard]] bool ReadMultiPolygonRings(keel::Array<GeoPolygon>& outPolys) {
        if (!Consume('[')) return false;
        if (Consume(']')) return false;
        const keel::usize before = outPolys.size();
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
    keel::StringRef m_sv;
    keel::i32 m_srsId{4326};
    keel::usize m_pos{0};
};

// ============================================================================
// GeoJSON Geometry & Attribute Extraction Helpers
// ============================================================================

void AppendCompactJsonValue(keel::String& out, const JsonValue& v) {
    switch (v.kind) {
        case JsonValue::Kind::Null:
            out.append("null");
            break;
        case JsonValue::Kind::Bool:
            out.append(v.boolVal ? "true" : "false");
            break;
        case JsonValue::Kind::Int: {
            char buf[32];
            out.append(keel::format_buf(buf, "{}", v.intVal));
            break;
        }
        case JsonValue::Kind::Double: {
            char buf[64];
            out.append(keel::format_buf(buf, "%.6g", v.doubleVal));
            break;
        }
        case JsonValue::Kind::String:
        case JsonValue::Kind::CoordsSlice:
            out.append(v.textVal);
            break;
        case JsonValue::Kind::Array:
            out.push_back('[');
            for (keel::usize i = 0; i < v.arrVal.size(); ++i) {
                if (i > 0) out.append(", ");
                AppendCompactJsonValue(out, v.arrVal[i]);
            }
            out.push_back(']');
            break;
        case JsonValue::Kind::Object:
            out.push_back('{');
            for (keel::usize i = 0; i < v.objVal.size(); ++i) {
                if (i > 0) out.append(", ");
                out.append(v.objVal[i].key);
                out.append(": ");
                AppendCompactJsonValue(out, v.objVal[i].value);
            }
            out.push_back('}');
            break;
    }
}

[[nodiscard]] AttributeValue JsonToAttributeValue(const JsonValue& v) {
    switch (v.kind) {
        case JsonValue::Kind::Null:   return AttributeValue(nullptr);
        case JsonValue::Kind::Bool:   return AttributeValue(v.boolVal);
        case JsonValue::Kind::Int:    return AttributeValue(v.intVal);
        case JsonValue::Kind::Double: return AttributeValue(v.doubleVal);
        case JsonValue::Kind::String: return AttributeValue(v.textVal);
        default: {
            keel::String compact;
            AppendCompactJsonValue(compact, v);
            return AttributeValue(keel::move(compact));
        }
    }
}

bool ParseGeometryObject(const JsonValue& geomObj, keel::i32 srsId, GeoFeature& feat, bool isTopLevel = true) {
    if (geomObj.IsNull()) return false;
    const auto* typeVal = geomObj.Find("type");
    if (!typeVal || !typeVal->IsString()) return false;

    const keel::StringRef gType = typeVal->AsString();

    if (gType == "GeometryCollection") {
        if (isTopLevel) {
            feat.type = GeometryType::GeometryCollection;
        }
        const auto* geomsVal = geomObj.Find("geometries");
        if (geomsVal && geomsVal->IsArray()) {
            bool anyOk = false;
            for (const JsonValue& subGeom : geomsVal->arrVal) {
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
    const keel::StringRef coordsSlice = coordsVal->AsCoordsSlice();
    if (coordsSlice.empty()) return false;

    CoordSliceReader reader(coordsSlice, srsId);

    if (gType == "Point") {
        if (isTopLevel) feat.type = GeometryType::Point;
        guikit::GeoCoord pt{};
        double optZ = static_cast<double>(NAN);
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
        keel::Array<guikit::GeoCoord> pts;
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
        keel::Array<guikit::GeoCoord> line;
        if (reader.ReadCoordRing(line) && line.size() >= 2) {
            feat.lines.push_back(keel::move(line));
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

keel::i32 DetectGeoJsonSrsId(const JsonValue& rootObj) {
    const auto* crsVal = rootObj.Find("crs");
    if (!crsVal || !crsVal->IsObject()) return 4326;
    const auto* propsVal = crsVal->Find("properties");
    if (!propsVal || !propsVal->IsObject()) return 4326;
    const auto* nameVal = propsVal->Find("name");
    if (!nameVal || !nameVal->IsString()) return 4326;

    const keel::StringRef crsName = nameVal->AsString();
    if (crsName.contains("3857") || crsName.contains("900913")) {
        return 3857;
    }
    if (crsName.contains("3395")) {
        return 3395;
    }
    return 4326;
}

void ParseFeatureObject(const JsonValue& featObj, keel::i32 srsId, keel::Array<GeoFeature>& outFeatures) {
    if (!featObj.IsObject()) return;

    GeoFeature feat;

    if (const auto* idVal = featObj.Find("id")) {
        if (idVal->IsString()) {
            feat.displayId.assign(idVal->AsString());
        } else if (idVal->IsInt()) {
            char buf[32];
            feat.displayId.assign(keel::format_buf(buf, "{}", idVal->intVal));
        } else if (idVal->IsDouble()) {
            char buf[64];
            feat.displayId.assign(keel::format_buf(buf, "%.6g", idVal->doubleVal));
        }
    }

    if (const auto* propsVal = featObj.Find("properties")) {
        if (propsVal->IsObject()) {
            feat.properties.reserve(propsVal->objVal.size());
            for (const auto& kv : propsVal->objVal) {
                if (kv.value.IsNull()) continue;
                feat.properties.push_back(GeoProperty{
                    keel::String::from(kv.key),
                    JsonToAttributeValue(kv.value)
                });
            }
        }
    }

    if (const auto* geomVal = featObj.Find("geometry")) {
        if (ParseGeometryObject(*geomVal, srsId, feat, true)) {
            outFeatures.push_back(keel::move(feat));
        }
    }
}

void EscapeJsonString(keel::String& out, keel::StringRef sv) {
    out.push_back('"');
    for (char c : sv) {
        switch (c) {
            case '"':  out.append("\\\""); break;
            case '\\': out.append("\\\\"); break;
            case '\b': out.append("\\b"); break;
            case '\f': out.append("\\f"); break;
            case '\n': out.append("\\n"); break;
            case '\r': out.append("\\r"); break;
            case '\t': out.append("\\t"); break;
            default:
                if (static_cast<keel::u8>(c) < 0x20u) {
                    char buf[8];
                    out.append(keel::format_buf(buf, "\\u%04x", static_cast<keel::u32>(static_cast<keel::u8>(c))));
                } else {
                    out.push_back(c);
                }
                break;
        }
    }
    out.push_back('"');
}

void AppendRingJson(keel::String& out, keel::ArrayRef<guikit::GeoCoord> ring, bool closeRing) {
    out.push_back('[');
    for (keel::usize i = 0; i < ring.size(); ++i) {
        if (i > 0) out.append(", ");
        char buf[64];
        out.append(keel::format_buf(buf, "[%.6f, %.6f]", ring[i].lon, ring[i].lat));
    }
    if (closeRing && !ring.empty()) {
        const auto& f = ring.front();
        const auto& b = ring.back();
        if (std::abs(f.lon - b.lon) > 1e-9 || std::abs(f.lat - b.lat) > 1e-9) {
            char buf[64];
            out.append(keel::format_buf(buf, ", [%.6f, %.6f]", f.lon, f.lat));
        }
    }
    out.push_back(']');
}

[[nodiscard]] keel::i32 CartographicSortRank(ChartFeatureClass cls) noexcept {
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

[[nodiscard]] LayerStyle DefaultStyleForChartClass(ChartFeatureClass cls, keel::usize idx) noexcept {
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
    keel::StringRef filePath,
    keel::String& outContent,
    keel::String& outResolvedPath,
    keel::String& outError
) {
    if (filePath.empty()) {
        outError.assign("Empty file path");
        return false;
    }

    // 1. Check embedded assets first (Native + WebAssembly)
    if (auto asset = guikit::Assets::Find(filePath); asset && !asset->Empty()) {
        outContent = keel::String::from(asset->AsStringView());
        outResolvedPath = guikit::Assets::NormalizePath(filePath);
        return true;
    }

    // 2. Check disk (direct path first, then relative parent prefixes for demo working directories)
    const char* prefixes[] = {"", "../", "../../", "../../../"};
    for (const char* prefix : prefixes) {
        keel::String candidate = keel::String::from(prefix);
        candidate.append(filePath);
        auto res = keel::fs_read_text(candidate.as_ref());
        if (!res.is_ok()) continue;
        outContent = keel::move(res.value());
        outResolvedPath = keel::move(candidate);
        return true;
    }

    outError = keel::format("Cannot open GeoJSON file: %s", filePath);
    return false;
}

} // namespace

GeoJsonResult GeoJsonIO::ParseFromString(keel::StringRef jsonText, keel::StringRef defaultLayerName) {
    GeoJsonResult res;
    keel::Arena domArena(keel::max<keel::usize>(keel::Arena::kDefaultReserveSize, jsonText.size() * 4u + 1024u * 1024u));
    JsonValue root{};
    JsonParser parser(jsonText, domArena);
    if (!parser.ParseRoot(root, res.error)) {
        return res;
    }

    if (!root.IsObject()) {
        res.error.assign("GeoJSON root must be a JSON object");
        return res;
    }

    res.layer.name.assign(defaultLayerName);
    res.layer.format.assign("GeoJSON");
    res.layer.srsId = DetectGeoJsonSrsId(root);

    if (const auto* nameVal = root.Find("name"); nameVal && nameVal->IsString()) {
        const keel::StringRef docName = nameVal->AsString();
        if (!docName.empty()) {
            res.layer.name.assign(docName);
        }
    }

    const auto* typeVal = root.Find("type");
    if (!typeVal || !typeVal->IsString()) {
        res.error.assign("Missing 'type' property on GeoJSON root object");
        return res;
    }

    const keel::StringRef rootType = typeVal->AsString();
    if (rootType == "FeatureCollection") {
        const auto* featsVal = root.Find("features");
        if (!featsVal || !featsVal->IsArray()) {
            res.error.assign("FeatureCollection is missing 'features' array");
            return res;
        }
        res.layer.features.reserve(featsVal->arrVal.size());
        for (const JsonValue& featVal : featsVal->arrVal) {
            ParseFeatureObject(featVal, res.layer.srsId, res.layer.features);
        }
    } else if (rootType == "Feature") {
        ParseFeatureObject(root, res.layer.srsId, res.layer.features);
    } else {
        GeoFeature singleFeat;
        if (ParseGeometryObject(root, res.layer.srsId, singleFeat, true)) {
            res.layer.features.push_back(keel::move(singleFeat));
        } else {
            res.error = keel::format("Unsupported or empty GeoJSON root type '%s'", rootType);
            return res;
        }
    }

    res.layer.FinalizeLayer(1);
    res.ok = true;
    return res;
}

GeoJsonResult GeoJsonIO::LoadFromFile(keel::StringRef filePath) {
    GeoJsonResult res;
    keel::String content;
    keel::String resolvedPath;
    if (!TryReadTextFromAssetsOrDisk(filePath, content, resolvedPath, res.error)) {
        return res;
    }

    const keel::StringRef stemRef = keel::path_stem(filePath);
    const keel::StringRef stem = stemRef.empty() ? keel::StringRef("GeoJSON Layer") : stemRef;

    res = ParseFromString(content.as_ref(), stem);
    res.layer.sourcePath = keel::move(resolvedPath);
    return res;
}

GeoJsonLoadResult GeoJsonIO::ParseThematicLayersFromString(
    keel::StringRef jsonText,
    keel::StringRef defaultLayerName,
    bool splitByThematicClass
) {
    GeoJsonLoadResult res{};
    GeoJsonResult singleRes = ParseFromString(jsonText, defaultLayerName);
    if (!singleRes.ok) {
        res.errorMessage = keel::move(singleRes.error);
        return res;
    }

    if (singleRes.layer.features.empty()) {
        res.errorMessage.assign("No valid geographic features found in GeoJSON");
        return res;
    }

    if (!splitByThematicClass) {
        singleRes.layer.style = DefaultStyleForChartClass(singleRes.layer.dominantChartClass, 0);
        res.totalBounds = singleRes.layer.bounds;
        res.hasValidBounds = singleRes.layer.hasValidBounds;
        res.totalFeatures = singleRes.layer.features.size();
        res.totalVertices = singleRes.layer.totalVertices;
        res.layers.push_back(keel::move(singleRes.layer));
        res.ok = true;
        return res;
    }

    struct RankedLayer {
        keel::i32 rankKey = 0;
        GeoLayer layer{};
    };
    keel::Array<RankedLayer> grouped;
    for (GeoFeature& feat : singleRes.layer.features) {
        const keel::i32 key = CartographicSortRank(feat.chartClass) * 100 + static_cast<keel::i32>(feat.chartClass);
        GeoLayer* target = nullptr;
        for (auto& entry : grouped) {
            if (entry.rankKey == key) {
                target = &entry.layer;
                break;
            }
        }
        if (!target) {
            RankedLayer rl{};
            rl.rankKey = key;
            rl.layer.name.assign(ChartFeatureClassToString(feat.chartClass));
            rl.layer.format.assign("GeoJSON");
            rl.layer.srsId = singleRes.layer.srsId;
            rl.layer.dominantChartClass = feat.chartClass;
            grouped.push_back(keel::move(rl));
            target = &grouped.back().layer;
        }
        target->features.push_back(keel::move(feat));
    }

    keel::sort(grouped, [](const RankedLayer& a, const RankedLayer& b) noexcept { return a.rankKey < b.rankKey; });

    keel::u64 nextLayerUid = 1;
    keel::usize styleIdx = 0;
    for (auto& entry : grouped) {
        auto& layer = entry.layer;
        layer.style = DefaultStyleForChartClass(layer.dominantChartClass, styleIdx++);
        layer.FinalizeLayer(nextLayerUid++);
        if (layer.hasValidBounds) {
            SrsTransform::UnionBounds(res.totalBounds, res.hasValidBounds, layer.bounds);
        }
        res.totalFeatures += layer.features.size();
        res.totalVertices += layer.totalVertices;
        res.layers.push_back(keel::move(layer));
    }

    res.ok = true;
    return res;
}

GeoJsonLoadResult GeoJsonIO::LoadThematicLayersFromFile(
    keel::StringRef filePath,
    bool splitByThematicClass
) {
    GeoJsonLoadResult res{};
    keel::String content;
    keel::String resolvedPath;
    if (!TryReadTextFromAssetsOrDisk(filePath, content, resolvedPath, res.errorMessage)) {
        return res;
    }

    const keel::StringRef stemRef = keel::path_stem(filePath);
    const keel::StringRef stem = stemRef.empty() ? keel::StringRef("GeoJSON Layer") : stemRef;

    res = ParseThematicLayersFromString(content.as_ref(), stem, splitByThematicClass);
    for (auto& layer : res.layers) {
        layer.sourcePath.assign(resolvedPath.as_ref());
    }
    return res;
}

keel::String GeoJsonIO::SerializeLayer(const GeoLayer& layer, bool prettyPrint) {
    const keel::StringRef nl = prettyPrint ? "\n" : "";
    const keel::StringRef ind1 = prettyPrint ? "  " : "";
    const keel::StringRef ind2 = prettyPrint ? "    " : "";
    const keel::StringRef ind3 = prettyPrint ? "      " : "";

    keel::String out = keel::String::with_capacity(layer.features.size() * 256 + 128);
    out.push_back('{');
    out.append(nl);
    out.append(ind1);
    out.append("\"type\": \"FeatureCollection\",");
    out.append(nl);
    out.append(ind1);
    out.append("\"name\": ");
    EscapeJsonString(out, layer.name.as_ref());
    out.push_back(',');
    out.append(nl);
    out.append(ind1);
    out.append("\"features\": [");
    out.append(nl);

    for (keel::usize fi = 0; fi < layer.features.size(); ++fi) {
        const GeoFeature& feat = layer.features[fi];
        (void)feat.EnsureGeometryLoaded();
        (void)feat.EnsurePropertiesLoaded();
        out.append(ind2);
        out.push_back('{');
        out.append(nl);
        out.append(ind3);
        out.append("\"type\": \"Feature\",");
        out.append(nl);

        if (!feat.displayId.empty()) {
            out.append(ind3);
            out.append("\"id\": ");
            EscapeJsonString(out, feat.displayId.as_ref());
            out.push_back(',');
            out.append(nl);
        }

        // Properties
        out.append(ind3);
        out.append("\"properties\": {");
        for (keel::usize pi = 0; pi < feat.properties.size(); ++pi) {
            if (pi > 0) out.append(", ");
            const auto& [k, v] = feat.properties[pi];
            EscapeJsonString(out, k.as_ref());
            out.append(": ");
            switch (v.kind()) {
                case AttributeValue::Kind::Null:
                    out.append("null");
                    break;
                case AttributeValue::Kind::Bool:
                    out.append(v.as_bool() ? "true" : "false");
                    break;
                case AttributeValue::Kind::Int: {
                    char buf[32];
                    out.append(keel::format_buf(buf, "{}", v.as_int()));
                    break;
                }
                case AttributeValue::Kind::Double: {
                    char buf[64];
                    out.append(keel::format_buf(buf, "%.6g", v.as_double()));
                    break;
                }
                case AttributeValue::Kind::String:
                    EscapeJsonString(out, v.as_string());
                    break;
            }
        }
        out.append("},");
        out.append(nl);

        // Geometry
        out.append(ind3);
        out.append("\"geometry\": ");
        if (!feat.polygons.empty()) {
            if (feat.polygons.size() == 1 && feat.type != GeometryType::MultiPolygon) {
                out.append("{\"type\": \"Polygon\", \"coordinates\": [");
                AppendRingJson(out, feat.polygons[0].exteriorRing, true);
                for (const auto& hole : feat.polygons[0].interiorRings) {
                    out.append(", ");
                    AppendRingJson(out, hole, true);
                }
                out.append("]}");
            } else {
                out.append("{\"type\": \"MultiPolygon\", \"coordinates\": [");
                for (keel::usize p = 0; p < feat.polygons.size(); ++p) {
                    if (p > 0) out.append(", ");
                    out.push_back('[');
                    AppendRingJson(out, feat.polygons[p].exteriorRing, true);
                    for (const auto& hole : feat.polygons[p].interiorRings) {
                        out.append(", ");
                        AppendRingJson(out, hole, true);
                    }
                    out.push_back(']');
                }
                out.append("]}");
            }
        } else if (!feat.lines.empty()) {
            if (feat.lines.size() == 1 && feat.type != GeometryType::MultiLineString) {
                out.append("{\"type\": \"LineString\", \"coordinates\": ");
                AppendRingJson(out, feat.lines[0], false);
                out.push_back('}');
            } else {
                out.append("{\"type\": \"MultiLineString\", \"coordinates\": [");
                for (keel::usize l = 0; l < feat.lines.size(); ++l) {
                    if (l > 0) out.append(", ");
                    AppendRingJson(out, feat.lines[l], false);
                }
                out.append("]}");
            }
        } else if (!feat.points.empty()) {
            if (feat.points.size() == 1 && feat.type != GeometryType::MultiPoint) {
                char buf[96];
                out.append(keel::format_buf(
                    buf,
                    "{\"type\": \"Point\", \"coordinates\": [%.6f, %.6f]}",
                    feat.points[0].lon,
                    feat.points[0].lat
                ));
            } else {
                out.append("{\"type\": \"MultiPoint\", \"coordinates\": ");
                AppendRingJson(out, feat.points, false);
                out.push_back('}');
            }
        } else {
            out.append("null");
        }

        out.append(nl);
        out.append(ind2);
        out.push_back('}');
        if (fi + 1 < layer.features.size()) {
            out.push_back(',');
        }
        out.append(nl);
    }

    out.append(ind1);
    out.push_back(']');
    out.append(nl);
    out.push_back('}');
    out.append(nl);
    return out;
}

bool GeoJsonIO::SaveToFile(const GeoLayer& layer, keel::StringRef filePath, keel::String* outError) {
    const keel::String json = SerializeLayer(layer, true);
    const keel::Error err = keel::fs_write_text(filePath, json.as_ref());
    if (err) {
        if (outError) {
            *outError = keel::format("Cannot open file for writing: %s", filePath);
        }
        return false;
    }
    return true;
}

keel::Array<GeoLayer> GeoJsonIO::BuildDefaultHydrographicChart() {
    auto loaded = LoadThematicLayersFromFile("assets/geo/demo_bay.geojson", true);
    if (loaded.ok && !loaded.layers.empty()) {
        return keel::move(loaded.layers);
    }
    return {};
}

} // namespace guikit::geo
