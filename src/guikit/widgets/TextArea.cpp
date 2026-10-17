#include "guikit/widgets/TextArea.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Element.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"
#include "guikit/panels/Toolbar.hpp"
#include "guikit/widgets/Button.hpp"

namespace guikit {

namespace {

// --- Built-in Language Dictionaries ---

static constexpr std::array<std::string_view, 67> kCppKeywords = {
    "alignas", "alignof", "auto", "break", "case", "catch", "class", "concept",
    "const", "consteval", "constexpr", "constinit", "continue", "co_await",
    "co_return", "co_yield", "decltype", "default", "delete", "do", "else",
    "enum", "explicit", "export", "extern", "false", "final", "for", "friend",
    "goto", "if", "inline", "mutable", "namespace", "new", "noexcept", "nullptr",
    "operator", "override", "private", "protected", "public", "requires",
    "return", "sizeof", "static", "static_assert", "static_cast", "dynamic_cast",
    "reinterpret_cast", "const_cast", "struct", "switch", "template", "this",
    "throw", "true", "try", "typedef", "typeid", "typename", "union", "using",
    "virtual", "volatile", "while", "import"
};

static constexpr std::array<std::string_view, 86> kCppTypes = {
    "bool", "char", "char8_t", "char16_t", "char32_t", "wchar_t", "short", "int",
    "long", "signed", "unsigned", "float", "double", "void", "size_t", "ptrdiff_t",
    "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t",
    "uint64_t", "std", "string", "string_view", "vector", "array", "span",
    "optional", "pair", "tuple", "unique_ptr", "shared_ptr", "function",
    "guikit", "Context", "Response", "WidgetResponse", "ContainerScope",
    "UiTheme", "UiStyle", "UiSize", "UiVariant", "UiText", "TextRole", "TextWrap",
    "TextLines", "TextVariants", "FontStyle", "Density", "ThemeMode", "Color",
    "Rect", "Vec2", "Padding", "Icon", "IconId", "Align", "Alignment", "Justify",
    "SizeMode", "StretchMode", "Orientation", "ElementLayout", "InlineOrder",
    "HeadingDivider", "CursorMode", "TabBarStyle", "SyntaxConfig", "SyntaxPalette",
    "TextSpan", "Element", "Button", "Slider", "StepSlider", "InputField",
    "Select", "Switch", "Checkbox", "Heading", "Text", "TextArea", "TabBar",
    "Toolbar"
};

static constexpr std::array<std::string_view, 24> kMatlabKeywords = {
    "break", "case", "catch", "classdef", "continue", "else", "elseif", "end",
    "enumeration", "events", "for", "function", "global", "if", "methods",
    "otherwise", "parfor", "persistent", "properties", "return", "spmd",
    "switch", "try", "while"
};

static constexpr std::array<std::string_view, 21> kMatlabTypes = {
    "double", "single", "int8", "uint8", "int16", "uint16", "int32", "uint32",
    "int64", "uint64", "logical", "char", "string", "cell", "struct", "table",
    "timetable", "categorical", "function_handle", "gpuArray", "complex"
};

static constexpr std::array<std::string_view, 68> kMatlabBuiltins = {
    "fft", "ifft", "fftshift", "ifftshift", "abs", "angle", "real", "imag",
    "conj", "zeros", "ones", "eye", "linspace", "logspace", "sin", "cos",
    "tan", "asin", "acos", "atan2", "exp", "log", "log10", "sqrt", "sum",
    "mean", "std", "var", "max", "min", "norm", "dot", "cross", "inv", "pinv",
    "eig", "svd", "qr", "lu", "chol", "size", "length", "numel", "reshape",
    "repmat", "permute", "squeeze", "find", "sort", "conv", "filter", "fft2",
    "hann", "hamming", "blackman", "chebwin", "pi", "inf", "Inf", "nan", "NaN",
    "eps", "true", "false", "disp", "fprintf", "sprintf", "error"
};

static constexpr std::array<std::string_view, 11> kMatlabFoldOpen = {
    "function", "if", "for", "while", "switch", "try", "parfor", "classdef",
    "methods", "properties", "events"
};

static constexpr std::array<std::string_view, 1> kMatlabFoldClose = {
    "end"
};

static constexpr std::array<std::string_view, 3> kJsonKeywords = {
    "true", "false", "null"
};

static constexpr std::array<std::string_view, 35> kPythonKeywords = {
    "False", "None", "True", "and", "as", "assert", "async", "await", "break",
    "class", "continue", "def", "del", "elif", "else", "except", "finally",
    "for", "from", "global", "if", "import", "in", "is", "lambda", "nonlocal",
    "not", "or", "pass", "raise", "return", "try", "while", "with", "yield"
};

static constexpr std::array<std::string_view, 18> kPythonTypes = {
    "int", "float", "complex", "bool", "str", "bytes", "list", "tuple", "dict",
    "set", "frozenset", "object", "type", "Optional", "List", "Dict", "Tuple", "Any"
};

static constexpr std::array<std::string_view, 22> kPythonBuiltins = {
    "print", "len", "range", "enumerate", "zip", "map", "filter", "sorted",
    "sum", "min", "max", "abs", "round", "isinstance", "getattr", "setattr",
    "hasattr", "open", "super", "property", "staticmethod", "classmethod"
};

bool IsIdentStart(char c) noexcept {
    const unsigned char u = static_cast<unsigned char>(c);
    return std::isalpha(u) || c == '_';
}

bool IsIdentChar(char c) noexcept {
    const unsigned char u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '_';
}

char ToLowerAscii(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

bool ContainsToken(std::span<const std::string_view> list, std::string_view token) noexcept {
    for (std::string_view item : list) {
        if (item == token) return true;
    }
    return false;
}

float MeasureChunkNoTabs(
    ICanvas* canvas,
    std::string_view chunk,
    float fontSize,
    FontStyle style
) noexcept {
    if (chunk.empty() || fontSize <= 0.0f) return 0.0f;
    if (canvas) {
        return canvas->MeasureTextStyled(chunk, fontSize, style).x;
    }
    const float factor = (style == FontStyle::Monospace) ? 0.60f : 0.56f;
    return static_cast<float>(chunk.size()) * fontSize * factor;
}

float MeasureSegmentWidth(
    ICanvas* canvas,
    std::string_view seg,
    float fontSize,
    FontStyle style
) noexcept {
    if (seg.empty() || fontSize <= 0.0f) return 0.0f;
    const size_t firstTab = seg.find('\t');
    if (firstTab == std::string_view::npos) {
        return MeasureChunkNoTabs(canvas, seg, fontSize, style);
    }

    const float tabW = MeasureChunkNoTabs(canvas, " ", fontSize, style) * 4.0f;
    float totalW = 0.0f;
    size_t pos = 0;
    while (pos < seg.size()) {
        if (seg[pos] == '\t') {
            size_t tabEnd = pos + 1;
            while (tabEnd < seg.size() && seg[tabEnd] == '\t') {
                ++tabEnd;
            }
            totalW += static_cast<float>(tabEnd - pos) * tabW;
            pos = tabEnd;
        } else {
            size_t nextTab = seg.find('\t', pos);
            if (nextTab == std::string_view::npos) {
                nextTab = seg.size();
            }
            totalW += MeasureChunkNoTabs(canvas, seg.substr(pos, nextTab - pos), fontSize, style);
            pos = nextTab;
        }
    }
    return totalW;
}

size_t HitTestColumnInLine(
    ICanvas* canvas,
    std::string_view lineSub,
    float localX,
    float fontSize,
    FontStyle style
) noexcept {
    if (lineSub.empty() || localX <= 0.0f) return 0;
    size_t pos = 0;
    float prevW = 0.0f;
    while (pos < lineSub.size()) {
        const size_t nextPos = TextUtil::NextCharBoundary(lineSub, pos);
        const float nextW = MeasureSegmentWidth(canvas, lineSub.substr(0, nextPos), fontSize, style);
        const float mid = (prevW + nextW) * 0.5f;
        if (localX < mid) {
            return pos;
        }
        prevW = nextW;
        pos = nextPos;
    }
    return lineSub.size();
}

struct LogicalLine {
    size_t startOffset{0};
    size_t length{0};
};

struct VisualLine {
    size_t logicalLine{0};
    size_t startOffset{0}; ///< Global byte offset in text
    size_t startCol{0};    ///< Column byte offset within logical line
    size_t length{0};      ///< Byte length of this visual sub-line
    bool isFirstSubLine{true};
};

struct FoldRegion {
    size_t startLine{0}; ///< 0-based start line index
    size_t endLine{0};   ///< 0-based inclusive end line index
};

struct SearchMatch {
    size_t startOffset{0};
    size_t endOffset{0};
    size_t logicalLine{0};
    size_t startCol{0};
    size_t endCol{0};
};

void SplitLogicalLines(std::string_view text, std::vector<LogicalLine>& outLines) {
    outLines.clear();
    size_t start = 0;
    while (start <= text.size()) {
        const size_t nl = text.find('\n', start);
        if (nl == std::string_view::npos) {
            size_t len = text.size() - start;
            if (len > 0 && text[start + len - 1] == '\r') {
                --len;
            }
            outLines.push_back({start, len});
            break;
        }
        size_t len = nl - start;
        if (len > 0 && text[start + len - 1] == '\r') {
            --len;
        }
        outLines.push_back({start, len});
        start = nl + 1;
    }
    if (outLines.empty()) {
        outLines.push_back({0, 0});
    }
}

void ComputeFoldRegions(
    std::string_view text,
    const std::vector<LogicalLine>& logicalLines,
    const SyntaxConfig* synCfg,
    std::vector<FoldRegion>& outRegions
) {
    outRegions.clear();
    if (logicalLines.size() < 2) return;

    struct OpenEntry {
        size_t lineIdx{0};
        bool isKeyword{false};
    };
    std::vector<OpenEntry> stack;
    stack.reserve(16);

    const std::string_view lineComment = synCfg ? synCfg->lineComment : "//";
    const auto openKw = synCfg ? synCfg->foldOpenKeywords : std::span<const std::string_view>{};
    const auto closeKw = synCfg ? synCfg->foldCloseKeywords : std::span<const std::string_view>{};

    for (size_t i = 0; i < logicalLines.size(); ++i) {
        const std::string_view raw = text.substr(logicalLines[i].startOffset, logicalLines[i].length);
        size_t p = 0;
        while (p < raw.size() && (raw[p] == ' ' || raw[p] == '\t')) ++p;
        if (p >= raw.size()) continue;
        if (!lineComment.empty() && raw.substr(p).starts_with(lineComment)) continue;

        // Check leading keyword opener/closer (e.g. MATLAB function/if/for ... end)
        if (!openKw.empty() || !closeKw.empty()) {
            if (IsIdentStart(raw[p])) {
                size_t wEnd = p + 1;
                while (wEnd < raw.size() && IsIdentChar(raw[wEnd])) ++wEnd;
                const std::string_view firstWord = raw.substr(p, wEnd - p);
                if (ContainsToken(openKw, firstWord)) {
                    // Check that the same line doesn't already end with 'end'
                    bool closedOnSameLine = false;
                    size_t scan = wEnd;
                    while (scan < raw.size()) {
                        if (!lineComment.empty() && raw.substr(scan).starts_with(lineComment)) break;
                        if (IsIdentStart(raw[scan])) {
                            size_t e2 = scan + 1;
                            while (e2 < raw.size() && IsIdentChar(raw[e2])) ++e2;
                            if (ContainsToken(closeKw, raw.substr(scan, e2 - scan))) {
                                closedOnSameLine = true;
                            }
                            scan = e2;
                        } else {
                            ++scan;
                        }
                    }
                    if (!closedOnSameLine) {
                        stack.push_back({i, true});
                    }
                } else if (ContainsToken(closeKw, firstWord)) {
                    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
                        if (it->isKeyword) {
                            if (i > it->lineIdx) {
                                outRegions.push_back({it->lineIdx, i});
                            }
                            stack.erase(std::next(it).base());
                            break;
                        }
                    }
                }
            }
        }

        // Check brace '{' and '}' blocks
        bool inStr = false;
        char quoteC = '\0';
        for (size_t cIdx = p; cIdx < raw.size(); ++cIdx) {
            const char c = raw[cIdx];
            if (!inStr && !lineComment.empty() && raw.substr(cIdx).starts_with(lineComment)) {
                break;
            }
            if (c == '"' || c == '\'') {
                if (!inStr) {
                    if (c == '\'' && synCfg && synCfg->transposeApostrophe && cIdx > 0) {
                        const char prev = raw[cIdx - 1];
                        if (IsIdentChar(prev) || prev == ')' || prev == ']' || prev == '}' || prev == '.') {
                            continue;
                        }
                    }
                    inStr = true;
                    quoteC = c;
                } else if (quoteC == c && (cIdx == 0 || raw[cIdx - 1] != '\\')) {
                    inStr = false;
                }
                continue;
            }
            if (inStr) continue;

            if (c == '{') {
                stack.push_back({i, false});
            } else if (c == '}') {
                for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
                    if (!it->isKeyword) {
                        if (i > it->lineIdx) {
                            outRegions.push_back({it->lineIdx, i});
                        }
                        stack.erase(std::next(it).base());
                        break;
                    }
                }
            }
        }
    }

    std::sort(outRegions.begin(), outRegions.end(), [](const FoldRegion& a, const FoldRegion& b) {
        return a.startLine < b.startLine || (a.startLine == b.startLine && a.endLine > b.endLine);
    });
}

const FoldRegion* FindFoldRegionStartingAt(std::span<const FoldRegion> regions, size_t lineIdx) noexcept {
    for (const FoldRegion& r : regions) {
        if (r.startLine == lineIdx) return &r;
        if (r.startLine > lineIdx) break;
    }
    return nullptr;
}

bool IsLineFoldedStart(std::span<const size_t> foldedLines, size_t lineIdx) noexcept {
    for (size_t fl : foldedLines) {
        if (fl == lineIdx) return true;
    }
    return false;
}

bool IsLineHiddenByFold(
    std::span<const FoldRegion> regions,
    std::span<const size_t> foldedLines,
    size_t lineIdx
) noexcept {
    if (foldedLines.empty() || regions.empty()) return false;
    for (size_t fl : foldedLines) {
        if (lineIdx <= fl) continue;
        if (const FoldRegion* r = FindFoldRegionStartingAt(regions, fl)) {
            if (lineIdx > r->startLine && lineIdx <= r->endLine) {
                return true;
            }
        }
    }
    return false;
}

void BuildVisualLines(
    std::string_view text,
    const std::vector<LogicalLine>& logicalLines,
    bool wordWrap,
    float wrapWidth,
    float fontSize,
    FontStyle style,
    ICanvas* canvas,
    std::vector<VisualLine>& outVisual,
    std::span<const FoldRegion> foldRegions = {},
    std::span<const size_t> foldedLines = {}
) {
    outVisual.clear();
    outVisual.reserve(logicalLines.size());

    for (size_t i = 0; i < logicalLines.size(); ++i) {
        if (IsLineHiddenByFold(foldRegions, foldedLines, i)) {
            continue;
        }
        const LogicalLine& ll = logicalLines[i];
        const std::string_view lineStr = text.substr(ll.startOffset, ll.length);
        if (!wordWrap || wrapWidth <= 24.0f || lineStr.empty()) {
            outVisual.push_back({i, ll.startOffset, 0, ll.length, true});
            continue;
        }

        const float fullW = MeasureSegmentWidth(canvas, lineStr, fontSize, style);
        if (fullW <= wrapWidth + 0.5f) {
            outVisual.push_back({i, ll.startOffset, 0, ll.length, true});
            continue;
        }

        size_t subStart = 0;
        bool first = true;
        while (subStart < ll.length) {
            size_t cur = subStart;
            size_t lastBreak = subStart;
            while (cur < ll.length) {
                const size_t next = TextUtil::NextCharBoundary(lineStr, cur);
                const float w = MeasureSegmentWidth(canvas, lineStr.substr(subStart, next - subStart), fontSize, style);
                if (w > wrapWidth && cur > subStart) {
                    break;
                }
                cur = next;
                if (cur < ll.length && (lineStr[cur - 1] == ' ' || lineStr[cur - 1] == '\t' || lineStr[cur - 1] == ',' || lineStr[cur - 1] == ';')) {
                    lastBreak = cur;
                }
            }
            const size_t takeEnd = (lastBreak > subStart && cur < ll.length) ? lastBreak : cur;
            const size_t subLen = std::max<size_t>(1, takeEnd - subStart);
            outVisual.push_back({i, ll.startOffset + subStart, subStart, subLen, first});
            first = false;
            subStart += subLen;
        }
    }
    if (outVisual.empty()) {
        outVisual.push_back({0, 0, 0, 0, true});
    }
}

void FindAllSearchMatches(
    std::string_view text,
    const std::vector<LogicalLine>& logicalLines,
    std::string_view query,
    bool matchCase,
    bool wholeWord,
    std::vector<SearchMatch>& outMatches
) {
    outMatches.clear();
    if (query.empty() || text.empty()) return;

    const size_t qLen = query.size();
    for (size_t lineIdx = 0; lineIdx < logicalLines.size(); ++lineIdx) {
        const LogicalLine& ll = logicalLines[lineIdx];
        if (ll.length < qLen) continue;
        const std::string_view lineStr = text.substr(ll.startOffset, ll.length);

        for (size_t col = 0; col + qLen <= ll.length; ) {
            bool matched = true;
            if (matchCase) {
                matched = (lineStr.substr(col, qLen) == query);
            } else {
                for (size_t k = 0; k < qLen; ++k) {
                    if (ToLowerAscii(lineStr[col + k]) != ToLowerAscii(query[k])) {
                        matched = false;
                        break;
                    }
                }
            }

            if (matched && wholeWord) {
                const bool leftWord = (col > 0) && IsIdentChar(lineStr[col - 1]);
                const bool rightWord = (col + qLen < ll.length) && IsIdentChar(lineStr[col + qLen]);
                if (leftWord || rightWord) {
                    matched = false;
                }
            }

            if (matched) {
                outMatches.push_back({
                    .startOffset = ll.startOffset + col,
                    .endOffset = ll.startOffset + col + qLen,
                    .logicalLine = lineIdx,
                    .startCol = col,
                    .endCol = col + qLen
                });
                col += qLen;
            } else {
                ++col;
            }
        }
    }
}

bool FindMatchingBracketPair(
    std::string_view text,
    size_t cursorOffset,
    size_t& outOpenOffset,
    size_t& outCloseOffset
) noexcept {
    if (text.empty()) return false;
    const size_t safeCur = std::min(cursorOffset, text.size());

    auto isBracket = [](char c) noexcept {
        return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}';
    };

    size_t brPos = std::string_view::npos;
    if (safeCur < text.size() && isBracket(text[safeCur])) {
        brPos = safeCur;
    } else if (safeCur > 0 && isBracket(text[safeCur - 1])) {
        brPos = safeCur - 1;
    }
    if (brPos == std::string_view::npos) return false;

    const char c = text[brPos];
    const bool forward = (c == '(' || c == '[' || c == '{');
    const char target = (c == '(') ? ')' : (c == ')') ? '('
                      : (c == '[') ? ']' : (c == ']') ? '['
                      : (c == '{') ? '}' : '{';

    int depth = 0;
    if (forward) {
        const size_t maxScan = std::min(text.size(), brPos + 16384);
        for (size_t i = brPos; i < maxScan; ++i) {
            if (text[i] == c) ++depth;
            else if (text[i] == target) {
                --depth;
                if (depth == 0) {
                    outOpenOffset = brPos;
                    outCloseOffset = i;
                    return true;
                }
            }
        }
    } else {
        const size_t minScan = (brPos > 16384) ? (brPos - 16384) : 0;
        for (size_t i = brPos + 1; i > minScan; --i) {
            const size_t idx = i - 1;
            if (text[idx] == c) ++depth;
            else if (text[idx] == target) {
                --depth;
                if (depth == 0) {
                    outOpenOffset = idx;
                    outCloseOffset = brPos;
                    return true;
                }
            }
        }
    }
    return false;
}

std::string_view ExtractWordAtOffset(std::string_view text, size_t offset, size_t* outStartOffset = nullptr) noexcept {
    if (text.empty()) return {};
    size_t pos = std::min(offset, text.size());
    if (pos == text.size() || !IsIdentChar(text[pos])) {
        if (pos > 0 && IsIdentChar(text[pos - 1])) {
            --pos;
        } else {
            return {};
        }
    }
    size_t s = pos;
    while (s > 0 && IsIdentChar(text[s - 1])) --s;
    if (!IsIdentStart(text[s])) return {};
    size_t e = pos + 1;
    while (e < text.size() && IsIdentChar(text[e])) ++e;
    if (outStartOffset) *outStartOffset = s;
    return text.substr(s, e - s);
}

std::string_view ExtractCodeWithoutLineComment(
    std::string_view lineText,
    std::string_view commentPrefix
) noexcept {
    size_t start = 0;
    while (start < lineText.size() && (lineText[start] == ' ' || lineText[start] == '\t' || lineText[start] == '\r')) {
        ++start;
    }
    if (start >= lineText.size()) {
        return {};
    }
    const std::string_view sub = lineText.substr(start);
    if (sub.starts_with("/*") || sub.starts_with("*/") || sub.starts_with("%{") || sub.starts_with("%}")) {
        return {};
    }

    bool inQuotes = false;
    char quoteChar = '\0';
    size_t codeLen = sub.size();
    for (size_t i = 0; i < sub.size(); ++i) {
        const char c = sub[i];
        if (inQuotes) {
            if (c == '\\' && i + 1 < sub.size()) {
                ++i;
                continue;
            }
            if (c == quoteChar) {
                inQuotes = false;
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            inQuotes = true;
            quoteChar = c;
            continue;
        }
        if (!commentPrefix.empty() && sub.substr(i).starts_with(commentPrefix)) {
            codeLen = i;
            break;
        }
        if (commentPrefix.empty() && sub.substr(i).starts_with("//")) {
            codeLen = i;
            break;
        }
    }

    std::string_view code = sub.substr(0, codeLen);
    while (!code.empty() && (code.back() == ' ' || code.back() == '\t' || code.back() == '\r')) {
        code.remove_suffix(1);
    }
    if (code == "{" || code == "}" || code == "};" || code == "}," || code == ";" || code == ")" || code == "]") {
        return {};
    }
    return code;
}

bool HasCodeBeforeOffsetOnLine(
    std::string_view docText,
    const LogicalLine& ll,
    size_t cursorOffset
) noexcept {
    if (cursorOffset <= ll.startOffset) {
        return false;
    }
    const size_t len = std::min(cursorOffset - ll.startOffset, ll.length);
    const std::string_view prefix = docText.substr(ll.startOffset, len);
    for (char c : prefix) {
        if (c != ' ' && c != '\t' && c != '\r') {
            return true;
        }
    }
    return false;
}

std::string_view ExtractLeadingIdentifierOrKey(std::string_view code) noexcept {
    size_t i = 0;
    while (i < code.size() && (code[i] == ' ' || code[i] == '\t' || code[i] == '(' || code[i] == '[' || code[i] == '*' || code[i] == '&')) {
        ++i;
    }
    if (i >= code.size()) {
        return {};
    }
    if (code[i] == '"') {
        const size_t start = i++;
        while (i < code.size() && code[i] != '"') {
            if (code[i] == '\\' && i + 1 < code.size()) ++i;
            ++i;
        }
        if (i < code.size()) ++i;
        return code.substr(start, i - start);
    }
    if (IsIdentStart(code[i])) {
        const size_t start = i++;
        while (i < code.size() && IsIdentChar(code[i])) {
            ++i;
        }
        return code.substr(start, i - start);
    }
    return {};
}

std::string_view ExtractAssignedTargetIdentifier(std::string_view code) noexcept {
    const std::string_view lead = ExtractLeadingIdentifierOrKey(code);
    if (lead == "for" || lead == "while" || lead == "if" || lead == "elif" ||
        lead == "else" || lead == "switch" || lead == "return" || lead == "function" ||
        lead == "def" || lead == "class" || lead == "rule" || lead == "when" ||
        lead == "emit" || lead == "alert") {
        return {};
    }

    int parenDepth = 0;
    size_t eqPos = std::string_view::npos;
    for (size_t i = 0; i < code.size(); ++i) {
        const char c = code[i];
        if (c == '(' || c == '[') {
            ++parenDepth;
        } else if (c == ')' || c == ']') {
            if (parenDepth > 0) --parenDepth;
        } else if (c == '=' && parenDepth == 0) {
            const char prev = (i > 0) ? code[i - 1] : '\0';
            const char next = (i + 1 < code.size()) ? code[i + 1] : '\0';
            if (prev != '=' && prev != '!' && prev != '<' && prev != '>' && next != '=') {
                eqPos = i;
                break;
            }
        }
    }
    if (eqPos == std::string_view::npos || eqPos == 0) {
        return {};
    }

    size_t p = eqPos;
    if (p > 0 && (code[p - 1] == '+' || code[p - 1] == '-' || code[p - 1] == '*' ||
                  code[p - 1] == '/' || code[p - 1] == '|' || code[p - 1] == '&' || code[p - 1] == '^')) {
        --p;
    }
    while (p > 0 && (code[p - 1] == ' ' || code[p - 1] == '\t')) {
        --p;
    }
    if (p > 0 && (code[p - 1] == ')' || code[p - 1] == ']')) {
        const char closeBr = code[p - 1];
        const char openBr = (closeBr == ')') ? '(' : '[';
        int depth = 0;
        while (p > 0) {
            --p;
            if (code[p] == closeBr) ++depth;
            else if (code[p] == openBr) {
                --depth;
                if (depth == 0) break;
            }
        }
        while (p > 0 && (code[p - 1] == ' ' || code[p - 1] == '\t')) {
            --p;
        }
    }
    const size_t endIdent = p;
    while (p > 0 && IsIdentChar(code[p - 1])) {
        --p;
    }
    if (endIdent > p && IsIdentStart(code[p])) {
        return code.substr(p, endIdent - p);
    }
    return {};
}

bool DoesLineMatchBaselineSignature(
    std::string_view currentCode,
    std::string_view baselineCode
) noexcept {
    if (currentCode.empty()) {
        return false;
    }
    if (baselineCode.empty()) {
        return true;
    }
    if (currentCode == baselineCode) {
        return true;
    }
    const std::string_view baseTarget = ExtractAssignedTargetIdentifier(baselineCode);
    if (!baseTarget.empty()) {
        const std::string_view curTarget = ExtractAssignedTargetIdentifier(currentCode);
        if (curTarget == baseTarget) {
            return true;
        }
        return false;
    }
    const std::string_view baseLead = ExtractLeadingIdentifierOrKey(baselineCode);
    const std::string_view curLead = ExtractLeadingIdentifierOrKey(currentCode);
    return !baseLead.empty() && (curLead == baseLead);
}

size_t ResolveTrackedCodeLine(
    std::string_view docText,
    std::span<const LogicalLine> logicalLines,
    size_t requestedLine1,
    std::string_view explicitAnchor,
    std::span<const size_t> baselineToCurrentLine,
    std::span<const std::string> baselineSignatures,
    std::string_view commentPrefix
) noexcept {
    if (logicalLines.empty() || requestedLine1 == 0) {
        return 0;
    }
    auto getLineCode = [&](size_t line1) noexcept -> std::string_view {
        if (line1 < 1 || line1 > logicalLines.size()) {
            return {};
        }
        const LogicalLine& ll = logicalLines[line1 - 1];
        return ExtractCodeWithoutLineComment(docText.substr(ll.startOffset, ll.length), commentPrefix);
    };

    const size_t mappedLine1 = (requestedLine1 < baselineToCurrentLine.size() && baselineToCurrentLine[requestedLine1] > 0)
        ? baselineToCurrentLine[requestedLine1]
        : requestedLine1;
    const std::string_view baseSig = (requestedLine1 < baselineSignatures.size())
        ? std::string_view(baselineSignatures[requestedLine1])
        : std::string_view{};

    if (!explicitAnchor.empty()) {
        const std::string_view mappedCode = getLineCode(mappedLine1);
        if (!mappedCode.empty() && (mappedCode.find(explicitAnchor) != std::string_view::npos || DoesLineMatchBaselineSignature(mappedCode, baseSig))) {
            return mappedLine1;
        }
        for (size_t delta = 1; delta <= logicalLines.size(); ++delta) {
            if (mappedLine1 + delta <= logicalLines.size()) {
                const std::string_view c = getLineCode(mappedLine1 + delta);
                if (!c.empty() && c.find(explicitAnchor) != std::string_view::npos) {
                    return mappedLine1 + delta;
                }
            }
            if (mappedLine1 > delta) {
                const std::string_view c = getLineCode(mappedLine1 - delta);
                if (!c.empty() && c.find(explicitAnchor) != std::string_view::npos) {
                    return mappedLine1 - delta;
                }
            }
        }
        return 0;
    }

    const std::string_view mappedCode = getLineCode(mappedLine1);
    if (DoesLineMatchBaselineSignature(mappedCode, baseSig)) {
        return mappedLine1;
    }
    if (!baseSig.empty()) {
        for (size_t delta = 1; delta <= 2; ++delta) {
            if (mappedLine1 + delta <= logicalLines.size() && getLineCode(mappedLine1 + delta) == baseSig) {
                return mappedLine1 + delta;
            }
            if (mappedLine1 > delta && getLineCode(mappedLine1 - delta) == baseSig) {
                return mappedLine1 - delta;
            }
        }
    }
    return 0;
}

ElementSpec MakeTextAreaElementSpec(
    const TextAreaOptions& opts,
    UiSize size,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const InputFieldMetrics& im = theme.GetInputField(size);
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .icon = opts.icon,
        .badge = opts.badge,
        .variant = opts.variant,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = opts.width,
        .widgetSize = size,
        .labelGap = im.labelGap,
        .descGap = im.descGap,
        .inlineGap = im.paddingX,
        .divider = opts.divider,
        .dividerStyle = opts.dividerStyle,
        .stretchableControl = true,
        .stretchHeightWhenStandalone = !opts.autoHeight,
        .disabled = disabled
    };
}

struct EditorFontMetrics {
    float fontSize{13.0f};
    float glyphBoxH{18.0f};
    float lineHeight{21.0f};
    float textOffsetY{1.5f};
    float spaceAdvance{7.8f};
    float tabAdvance{31.2f};
    float digitAdvance{7.8f};
};

EditorFontMetrics ResolveEditorFontMetrics(
    ICanvas* canvas,
    const UiTheme& theme,
    float baseFontSize,
    float effZoom,
    FontStyle style
) noexcept {
    EditorFontMetrics m{};
    const float clampedZoom = std::clamp(effZoom, 0.5f, 3.0f);
    m.fontSize = baseFontSize * clampedZoom;

    if (canvas) {
        const Vec2 sample = canvas->MeasureTextStyled("Mg", m.fontSize, style);
        m.glyphBoxH = (sample.y > 0.0f) ? sample.y : theme.GetLineHeight(m.fontSize);
        const float spW = canvas->MeasureTextStyled(" ", m.fontSize, style).x;
        m.spaceAdvance = (spW > 0.0f) ? spW : (m.fontSize * 0.60f);
        const float digW = canvas->MeasureTextStyled("8", m.fontSize, FontStyle::Monospace).x;
        m.digitAdvance = (digW > 0.0f) ? digW : m.spaceAdvance;
    } else {
        m.glyphBoxH = theme.GetLineHeight(m.fontSize);
        const float factor = (style == FontStyle::Monospace) ? 0.60f : 0.56f;
        m.spaceAdvance = m.fontSize * factor;
        m.digitAdvance = m.fontSize * 0.60f;
    }

    m.lineHeight = std::max(theme.GetLineHeight(m.fontSize), m.glyphBoxH * 1.14f);
    m.textOffsetY = (m.lineHeight - m.glyphBoxH) * 0.5f;
    m.tabAdvance = m.spaceAdvance * 4.0f;
    return m;
}

Vec2 MeasureEditorBox(
    std::string_view text,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float effZoom
) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const FontStyle textStyle = opts.monospace ? FontStyle::Monospace : FontStyle::Regular;
    const float baseFontSize = opts.monospace
        ? theme.GetTypography(TextRole::CodeMedium).fontSize
        : theme.ResolveFontSize(TextRole::BodyMedium, size);
    const EditorFontMetrics fm = ResolveEditorFontMetrics(canvas, theme, baseFontSize, effZoom, textStyle);
    const float fontSize = fm.fontSize;
    const float lineHeight = fm.lineHeight;
    const float padY = std::max(theme.Scale(8.0f), cm.paddingY);
    const float padX = std::max(theme.Scale(10.0f), cm.paddingX);
    const float statusH = opts.showStatusBar ? theme.Scale(22.0f) : 0.0f;

    size_t lineCount = 1;
    size_t maxLineChars = 0;
    size_t curChars = 0;
    for (char c : text) {
        if (c == '\n') {
            ++lineCount;
            maxLineChars = std::max(maxLineChars, curChars);
            curChars = 0;
        } else if (c != '\r') {
            ++curChars;
        }
    }
    maxLineChars = std::max(maxLineChars, curChars);

    float boxH = 0.0f;
    if (opts.fixedHeight.has_value()) {
        boxH = theme.Scale(*opts.fixedHeight);
    } else if (opts.autoHeight) {
        boxH = static_cast<float>(lineCount) * lineHeight + padY * 2.0f + statusH;
    } else if (opts.rows.has_value()) {
        const uint32_t r = std::max(1u, *opts.rows);
        boxH = static_cast<float>(r) * lineHeight + padY * 2.0f + statusH;
    } else {
        boxH = 4.0f * lineHeight + padY * 2.0f + statusH;
    }

    size_t digits = 2;
    size_t n = lineCount;
    while (n >= 100) {
        ++digits;
        n /= 10;
    }
    const bool hasBpGutter = (opts.breakpoints != nullptr) || opts.enableBreakpoints || (opts.executionLine > 0);
    const float bpWidth = hasBpGutter ? std::max(theme.Scale(28.0f) * effZoom, fm.spaceAdvance * 3.4f) : 0.0f;
    const float foldWidth = opts.codeFolding ? std::max(theme.Scale(14.0f) * effZoom, fm.spaceAdvance * 1.85f) : 0.0f;
    const float gutterPadX = fm.spaceAdvance * 1.25f;
    const float gutterWidth = opts.showLineNumbers
        ? (bpWidth + static_cast<float>(digits) * fm.digitAdvance + gutterPadX * 2.0f + foldWidth)
        : (bpWidth + foldWidth);

    float boxW = theme.Scale(280.0f);
    if (opts.stretch != StretchMode::Fill && opts.width.has_value()) {
        boxW = theme.Scale(*opts.width);
    } else {
        const float charAdv = MeasureChunkNoTabs(canvas, "M", fontSize, textStyle);
        const float estimatedW = gutterWidth + padX * 2.0f + static_cast<float>(std::min<size_t>(maxLineChars, 80)) * charAdv + theme.Scale(24.0f);
        boxW = std::max(boxW, estimatedW);
    }

    return {boxW, boxH};
}

struct SearchOverlayRects {
    Rect panelRect{};
    Rect toggleReplaceBtn{};
    Rect findInputRect{};
    Rect caseBtnRect{};
    Rect wordBtnRect{};
    Rect countRect{};
    Rect prevBtnRect{};
    Rect nextBtnRect{};
    Rect closeBtnRect{};
    Rect replaceInputRect{};
    Rect replaceOneBtnRect{};
    Rect replaceAllBtnRect{};
};

struct DebugToolbarRects {
    Rect panelRect{};
    Rect gripRect{};
    float minBarX{0.0f};
    float maxBarX{0.0f};
    std::array<Rect, 6> btnRects{}; ///< Continue, StepOver, StepInto, StepOut, Restart, Stop
};

struct EditorLayoutRects {
    ElementGeometry elemGeom{};
    Rect editorBox{};
    Rect innerBounds{};
    Rect statusBarRect{};
    Rect gutterRect{};
    Rect minimapRect{};
    Rect textViewport{};
    Rect vTrackRect{};
    Rect vThumbRect{};
    Rect hTrackRect{};
    Rect hThumbRect{};
    Rect copyBtnRect{};
    Rect searchBtnRect{};
    Rect zoomBtnRect{};
    SearchOverlayRects searchOverlay{};
    DebugToolbarRects debugToolbar{};
    UiSize toolbarSize{UiSize::Sm};
    EditorFontMetrics metrics{};
    float fontSize{13.0f};
    float glyphBoxH{18.0f};
    float lineHeight{18.0f};
    float gutterWidth{0.0f};
    float bpColWidth{0.0f};
    float foldColWidth{0.0f};
    float minimapWidth{0.0f};
    float statusBarHeight{0.0f};
    float maxScrollY{0.0f};
    float maxScrollX{0.0f};
    float totalContentH{0.0f};
    float maxLineW{0.0f};
    bool needVScroll{false};
    bool needHScroll{false};
    bool hasMinimap{false};
};

EditorLayoutRects ComputeEditorLayout(
    Rect bounds,
    std::string_view text,
    const std::vector<LogicalLine>& logicalLines,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float effZoom,
    float scrollX,
    float scrollY,
    bool copyFeedbackActive,
    bool searchOpen,
    bool replaceOpen,
    bool gotoOpen,
    std::span<const FoldRegion> foldRegions,
    std::span<const size_t> foldedLines,
    std::vector<VisualLine>& outVisualLines,
    float debugToolbarRatio = -1.0f
) {
    EditorLayoutRects L{};
    L.toolbarSize = opts.toolbarSize.value_or(size);
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const ComponentMetrics& tbCm = theme.GetMetrics(L.toolbarSize);
    const float tbGap = theme.GetGap(L.toolbarSize);
    const ScrollbarMetrics& sm = theme.GetScrollbar(size);
    const FontStyle textStyle = opts.monospace ? FontStyle::Monospace : FontStyle::Regular;

    const float baseFontSize = opts.monospace
        ? theme.GetTypography(TextRole::CodeMedium).fontSize
        : theme.ResolveFontSize(TextRole::BodyMedium, size);
    L.metrics = ResolveEditorFontMetrics(canvas, theme, baseFontSize, effZoom, textStyle);
    L.fontSize = L.metrics.fontSize;
    L.glyphBoxH = L.metrics.glyphBoxH;
    L.lineHeight = L.metrics.lineHeight;

    const Vec2 intrinsicBox = MeasureEditorBox(text, opts, size, canvas, theme, effZoom);
    const ElementSpec spec = MakeTextAreaElementSpec(opts, size, opts.disabled, theme);
    L.elemGeom = Element::Resolve(bounds, intrinsicBox, spec, canvas, theme);
    L.editorBox = L.elemGeom.controlRect;

    const float padY = std::max(theme.Scale(8.0f), cm.paddingY);
    const float padX = std::max(theme.Scale(10.0f), cm.paddingX);

    size_t digits = 2;
    size_t n = std::max<size_t>(1, logicalLines.size());
    while (n >= 100) {
        ++digits;
        n /= 10;
    }

    const bool hasBpGutter = (opts.breakpoints != nullptr) || opts.enableBreakpoints || (opts.executionLine > 0);
    L.bpColWidth = hasBpGutter ? std::max(theme.Scale(28.0f) * effZoom, L.metrics.spaceAdvance * 3.4f) : 0.0f;
    L.foldColWidth = opts.codeFolding ? std::max(theme.Scale(14.0f) * effZoom, L.metrics.spaceAdvance * 1.85f) : 0.0f;
    const float gutterPadX = L.metrics.spaceAdvance * 1.25f;
    L.gutterWidth = opts.showLineNumbers
        ? (L.bpColWidth + static_cast<float>(digits) * L.metrics.digitAdvance + gutterPadX * 2.0f + L.foldColWidth)
        : (L.bpColWidth + L.foldColWidth);

    // Unzoomed (100%) gutter width for zoom-invariant floating toolbar track bounds
    const EditorFontMetrics baseMetrics = ResolveEditorFontMetrics(canvas, theme, baseFontSize, 1.0f, textStyle);
    const float baseBpColW = hasBpGutter ? std::max(theme.Scale(28.0f), baseMetrics.spaceAdvance * 3.4f) : 0.0f;
    const float baseFoldColW = opts.codeFolding ? std::max(theme.Scale(14.0f), baseMetrics.spaceAdvance * 1.85f) : 0.0f;
    const float baseGutterPadX = baseMetrics.spaceAdvance * 1.25f;
    const float baseGutterW = opts.showLineNumbers
        ? (baseBpColW + static_cast<float>(digits) * baseMetrics.digitAdvance + baseGutterPadX * 2.0f + baseFoldColW)
        : (baseBpColW + baseFoldColW);

    const Rect fullInner(
        L.editorBox.x + 1.0f,
        L.editorBox.y + 1.0f,
        std::max(0.0f, L.editorBox.w - 2.0f),
        std::max(0.0f, L.editorBox.h - 2.0f)
    );

    L.statusBarHeight = (opts.showStatusBar && fullInner.h > theme.Scale(60.0f)) ? theme.Scale(22.0f) : 0.0f;
    L.innerBounds = Rect(
        fullInner.x,
        fullInner.y,
        fullInner.w,
        std::max(0.0f, fullInner.h - L.statusBarHeight)
    );
    if (L.statusBarHeight > 0.0f) {
        L.statusBarRect = Rect(
            fullInner.x,
            fullInner.Bottom() - L.statusBarHeight,
            fullInner.w,
            L.statusBarHeight
        );
    }

    if (L.gutterWidth > 0.0f) {
        L.gutterRect = Rect(
            L.innerBounds.x,
            L.innerBounds.y,
            std::min(L.gutterWidth, L.innerBounds.w),
            L.innerBounds.h
        );
    }

    L.hasMinimap = opts.showMinimap && (L.innerBounds.w > theme.Scale(280.0f)) && (L.innerBounds.h > theme.Scale(60.0f));
    L.minimapWidth = L.hasMinimap ? theme.Scale(64.0f) : 0.0f;

    float prelimTextW = std::max(0.0f, L.innerBounds.w - L.gutterWidth - L.minimapWidth - padX * 2.0f);
    BuildVisualLines(text, logicalLines, opts.wordWrap, prelimTextW, L.fontSize, textStyle, canvas, outVisualLines, foldRegions, foldedLines);

    L.totalContentH = static_cast<float>(outVisualLines.size()) * L.lineHeight;
    const float availTextH = std::max(0.0f, L.innerBounds.h - padY * 2.0f);
    L.needVScroll = !opts.autoHeight && (L.totalContentH > availTextH + 1.0f) && sm.trackWidth > 0.0f;

    const float vBarReserve = L.needVScroll ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;
    if (L.needVScroll && opts.wordWrap) {
        prelimTextW = std::max(0.0f, L.innerBounds.w - L.gutterWidth - L.minimapWidth - padX * 2.0f - vBarReserve);
        BuildVisualLines(text, logicalLines, opts.wordWrap, prelimTextW, L.fontSize, textStyle, canvas, outVisualLines, foldRegions, foldedLines);
        L.totalContentH = static_cast<float>(outVisualLines.size()) * L.lineHeight;
    }

    L.maxLineW = 0.0f;
    if (!opts.wordWrap) {
        for (size_t li = 0; li < logicalLines.size(); ++li) {
            const LogicalLine& ll = logicalLines[li];
            if (ll.length == 0) continue;
            const std::string_view ls = text.substr(ll.startOffset, ll.length);
            float lineW = MeasureSegmentWidth(canvas, ls, L.fontSize, textStyle);
            if (!opts.inlineValues.empty()) {
                for (const CodeInlineValue& iv : opts.inlineValues) {
                    if (iv.line == li + 1 && !iv.text.empty()) {
                        const float ivFs = L.fontSize * 0.88f;
                        lineW += L.metrics.spaceAdvance * 2.2f + MeasureChunkNoTabs(canvas, iv.text, ivFs, FontStyle::Monospace) + theme.Scale(12.0f);
                    }
                }
            }
            L.maxLineW = std::max(L.maxLineW, lineW);
        }
    }

    const float availTextW = std::max(0.0f, L.innerBounds.w - L.gutterWidth - L.minimapWidth - padX * 2.0f - vBarReserve);
    L.needHScroll = !opts.wordWrap && (L.maxLineW > availTextW + 2.0f) && sm.trackWidth > 0.0f;
    const float hBarReserve = L.needHScroll ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;

    const float finalViewportH = std::max(0.0f, L.innerBounds.h - padY * 2.0f - hBarReserve);
    L.needVScroll = !opts.autoHeight && (L.totalContentH > finalViewportH + 1.0f) && sm.trackWidth > 0.0f;
    const float finalVBarReserve = L.needVScroll ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;

    if (L.hasMinimap) {
        L.minimapRect = Rect(
            L.innerBounds.Right() - finalVBarReserve - L.minimapWidth,
            L.innerBounds.y,
            L.minimapWidth,
            L.innerBounds.h - hBarReserve
        );
    }

    L.textViewport = Rect(
        L.innerBounds.x + L.gutterWidth + padX,
        L.innerBounds.y + padY,
        std::max(0.0f, L.innerBounds.w - L.gutterWidth - L.minimapWidth - padX * 2.0f - finalVBarReserve),
        finalViewportH
    );

    L.maxScrollY = std::max(0.0f, L.totalContentH - L.textViewport.h);
    L.maxScrollX = opts.wordWrap ? 0.0f : std::max(0.0f, L.maxLineW + theme.Scale(24.0f) - L.textViewport.w);

    const float clampedScrollY = std::clamp(scrollY, 0.0f, L.maxScrollY);
    const float clampedScrollX = std::clamp(scrollX, 0.0f, L.maxScrollX);

    if (L.needVScroll) {
        const float trackX = L.innerBounds.Right() - sm.trackWidth - sm.margin;
        const float trackY = L.innerBounds.y + sm.margin;
        const float trackH = std::max(0.0f, L.innerBounds.h - sm.margin * 2.0f - hBarReserve);
        L.vTrackRect = Rect(trackX, trackY, sm.trackWidth, trackH);
        if (trackH > 0.0f && L.totalContentH > 0.0f) {
            const float visibleRatio = std::clamp(L.textViewport.h / L.totalContentH, 0.05f, 1.0f);
            const float thumbH = std::min(trackH, std::max(sm.minThumbLength, trackH * visibleRatio));
            const float maxTravel = std::max(0.0f, trackH - thumbH);
            const float thumbY = trackY + ((L.maxScrollY > 0.0f) ? (clampedScrollY / L.maxScrollY) * maxTravel : 0.0f);
            L.vThumbRect = Rect(trackX, thumbY, sm.trackWidth, thumbH);
        }
    }

    if (L.needHScroll) {
        const float trackX = L.innerBounds.x + L.gutterWidth + sm.margin;
        const float trackY = L.innerBounds.Bottom() - sm.trackWidth - sm.margin;
        const float trackW = std::max(0.0f, L.innerBounds.w - L.gutterWidth - L.minimapWidth - sm.margin * 2.0f - (L.needVScroll ? (sm.trackWidth + sm.margin) : 0.0f));
        L.hTrackRect = Rect(trackX, trackY, trackW, sm.trackWidth);
        const float totalW = L.maxLineW + theme.Scale(24.0f);
        if (trackW > 0.0f && totalW > 0.0f) {
            const float visibleRatio = std::clamp(L.textViewport.w / totalW, 0.05f, 1.0f);
            const float thumbW = std::min(trackW, std::max(sm.minThumbLength, trackW * visibleRatio));
            const float maxTravel = std::max(0.0f, trackW - thumbW);
            const float thumbX = trackX + ((L.maxScrollX > 0.0f) ? (clampedScrollX / L.maxScrollX) * maxTravel : 0.0f);
            L.hThumbRect = Rect(thumbX, trackY, thumbW, sm.trackWidth);
        }
    }

    // Top-right quick buttons (Copy, Search, Zoom %) when Search overlay is closed — sized via UiSize & Button::Measure
    const float overlayMargin = std::max(theme.Scale(4.0f), tbGap * 0.75f);
    const float overlayRight = L.innerBounds.Right() - finalVBarReserve - L.minimapWidth - overlayMargin;
    const float overlayTop = L.innerBounds.y + overlayMargin;
    float rightCursor = overlayRight;

    float stableCopyReserve = 0.0f;
    if (opts.showCopyButton && L.innerBounds.w > tbCm.defaultWidth * 1.5f) {
        ButtonOptions stableCopyOpts{};
        stableCopyOpts.label = "Copy";
        stableCopyOpts.icon = Icon::Copy();
        stableCopyOpts.size = L.toolbarSize;
        stableCopyReserve = Button::Measure(stableCopyOpts, L.toolbarSize, canvas, theme).x + tbGap * 0.75f;
    }

    if (!searchOpen && !gotoOpen) {
        if (opts.showCopyButton && L.innerBounds.w > tbCm.defaultWidth * 1.5f) {
            ButtonOptions copyOpts{};
            copyOpts.label = copyFeedbackActive ? "Copied" : "Copy";
            copyOpts.icon = copyFeedbackActive ? Icon::Check() : Icon::Copy();
            copyOpts.size = L.toolbarSize;
            const Vec2 copySz = Button::Measure(copyOpts, L.toolbarSize, canvas, theme);
            L.copyBtnRect = Rect(rightCursor - copySz.x, overlayTop, copySz.x, copySz.y);
            rightCursor -= (copySz.x + tbGap * 0.75f);
        }

        if (opts.allowZoom && std::abs(effZoom - 1.0f) > 0.01f && L.innerBounds.w > tbCm.defaultWidth * 1.75f) {
            char zBuf[16]{};
            std::snprintf(zBuf, sizeof(zBuf), "%d%%", static_cast<int>(std::lround(effZoom * 100.0f)));
            ButtonOptions zoomOpts{};
            zoomOpts.label = zBuf;
            zoomOpts.size = L.toolbarSize;
            const Vec2 zSz = Button::Measure(zoomOpts, L.toolbarSize, canvas, theme);
            L.zoomBtnRect = Rect(rightCursor - zSz.x, overlayTop, zSz.x, zSz.y);
            rightCursor -= (zSz.x + tbGap * 0.75f);
        }
    }

    // Search & Replace / GoToLine floating overlay geometry — sized via UiSize & Button::Measure
    if ((searchOpen || gotoOpen) && L.innerBounds.w > tbCm.defaultHeight * 6.0f) {
        ButtonOptions sqOpts{};
        sqOpts.icon = Icon::Close();
        sqOpts.square = true;
        sqOpts.size = L.toolbarSize;
        const Vec2 sqSz = Button::Measure(sqOpts, L.toolbarSize, canvas, theme);
        const float rowH = sqSz.y;
        const float sqBtn = sqSz.x;
        const float pad = std::max(theme.Scale(4.0f), tbGap * 0.65f);
        const float gap = std::max(theme.Scale(3.0f), tbGap * 0.50f);
        const bool showRep = searchOpen && replaceOpen && !opts.readOnly;
        const float countW = tbCm.defaultHeight * 2.0f;
        const float maxPanelW = tbCm.defaultHeight * 13.5f;
        const float panelW = std::min(maxPanelW, std::max(tbCm.defaultHeight * 6.5f, L.textViewport.w - tbGap));
        const float panelH = showRep ? (rowH * 2.0f + pad * 2.0f + gap) : (rowH + pad * 2.0f);
        const float panelX = overlayRight - panelW;
        const float panelY = overlayTop;

        SearchOverlayRects& S = L.searchOverlay;
        S.panelRect = Rect(panelX, panelY, panelW, panelH);

        const float y1 = panelY + pad;
        float x = panelX + pad;

        if (searchOpen && !opts.readOnly) {
            S.toggleReplaceBtn = Rect(x, y1, sqBtn, rowH);
            x += sqBtn + gap;
        }

        const float rightControlsW = searchOpen
            ? (sqBtn * 3.0f + countW + gap * 4.0f)
            : (sqBtn + gap);
        const float findTotalW = std::max(tbCm.defaultHeight * 2.0f, (panelX + panelW - pad) - x - rightControlsW);
        S.findInputRect = Rect(x, y1, findTotalW, rowH);

        if (searchOpen) {
            const float inset = std::max(theme.Scale(2.0f), pad * 0.45f);
            const float toggleSz = std::max(theme.Scale(16.0f), rowH - inset * 2.0f);
            S.wordBtnRect = Rect(S.findInputRect.Right() - toggleSz - inset, y1 + inset, toggleSz, toggleSz);
            S.caseBtnRect = Rect(S.wordBtnRect.x - toggleSz - inset * 0.75f, y1 + inset, toggleSz, toggleSz);

            float rx = S.findInputRect.Right() + gap;
            S.countRect = Rect(rx, y1, countW, rowH);
            rx += countW + gap;
            S.prevBtnRect = Rect(rx, y1, sqBtn, rowH);
            rx += sqBtn + gap;
            S.nextBtnRect = Rect(rx, y1, sqBtn, rowH);
            rx += sqBtn + gap;
            S.closeBtnRect = Rect(rx, y1, sqBtn, rowH);
        } else {
            // Go to Line mode
            S.closeBtnRect = Rect(S.findInputRect.Right() + gap, y1, sqBtn, rowH);
        }

        if (showRep) {
            const float y2 = y1 + rowH + gap;
            const float repX = S.findInputRect.x;
            ButtonOptions rep1Opts{};
            rep1Opts.label = "1";
            rep1Opts.icon = Icon::Replace();
            rep1Opts.size = L.toolbarSize;
            const float w1 = Button::Measure(rep1Opts, L.toolbarSize, canvas, theme).x;

            ButtonOptions repAllOpts{};
            repAllOpts.label = "All";
            repAllOpts.icon = Icon::ReplaceAll();
            repAllOpts.size = L.toolbarSize;
            const float w2 = Button::Measure(repAllOpts, L.toolbarSize, canvas, theme).x;

            S.replaceInputRect = Rect(repX, y2, S.findInputRect.w, rowH);
            float rx = S.replaceInputRect.Right() + gap;
            S.replaceOneBtnRect = Rect(rx, y2, w1, rowH);
            rx += w1 + gap;
            S.replaceAllBtnRect = Rect(rx, y2, w2, rowH);
        }
    }

    // Floating Debug Toolbar at top of editor — zoom-invariant horizontal track with Left/Center/Right & grip drag
    if (opts.showDebugToolbar && L.innerBounds.w > tbCm.defaultHeight * 6.5f) {
        ButtonOptions dbgBtnOpts{};
        dbgBtnOpts.icon = Icon::DebugContinue();
        dbgBtnOpts.square = true;
        dbgBtnOpts.variant = UiVariant::Ghost;
        dbgBtnOpts.size = L.toolbarSize;
        const Vec2 btnSz = Button::Measure(dbgBtnOpts, L.toolbarSize, canvas, theme);
        const float btnW = btnSz.x;
        const float btnH2 = btnSz.y;
        const float gap = std::max(theme.Scale(2.0f), tbGap * 0.50f);
        const float pad = std::max(theme.Scale(4.0f), tbGap * 0.65f);
        const float gripW = tbCm.iconSize * 1.15f;
        const float sepExtra = tbGap; // Group separator slot before Restart & Stop
        const float barW = gripW + 6.0f * btnW + 5.0f * gap + sepExtra + pad * 2.0f;

        // Zoom-invariant horizontal track bounds [minBarX, maxBarX]
        const float fixedVBarReserve = (!opts.autoHeight && sm.trackWidth > 0.0f) ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;
        const float fixedRightLimit = L.innerBounds.Right() - fixedVBarReserve - L.minimapWidth - overlayMargin - stableCopyReserve;
        const float minBarX = L.innerBounds.x + baseGutterW + padX;
        const float maxBarX = std::max(minBarX, fixedRightLimit - barW);
        L.debugToolbar.minBarX = minBarX;
        L.debugToolbar.maxBarX = maxBarX;

        float barX = minBarX;
        if (debugToolbarRatio >= 0.0f) {
            barX = minBarX + std::clamp(debugToolbarRatio, 0.0f, 1.0f) * std::max(0.0f, maxBarX - minBarX);
        } else if (opts.debugToolbarAlign == Align::Start) {
            barX = minBarX;
        } else if (opts.debugToolbarAlign == Align::End) {
            barX = maxBarX;
        } else {
            // Align::Center — centered within zoom-invariant track [minBarX, maxBarX]
            barX = std::clamp(L.innerBounds.x + (L.innerBounds.w - barW) * 0.5f, minBarX, maxBarX);
        }

        if (L.searchOverlay.panelRect.w > 0.0f && barX + barW > L.searchOverlay.panelRect.x - tbGap) {
            barX = std::max(minBarX, L.searchOverlay.panelRect.x - barW - tbGap);
        }
        L.debugToolbar.panelRect = Rect(barX, overlayTop, barW, btnH2 + pad * 2.0f);
        L.debugToolbar.gripRect = Rect(barX, overlayTop, pad + gripW, btnH2 + pad * 2.0f);

        // If the zoom % badge is visible and overlaps the debug toolbar, shift the zoom badge to the left of the debug toolbar
        if (L.zoomBtnRect.w > 0.0f) {
            const float overlapPad = tbGap * 0.75f;
            if (L.debugToolbar.panelRect.Right() + overlapPad > L.zoomBtnRect.x &&
                L.debugToolbar.panelRect.x < L.zoomBtnRect.Right() + overlapPad) {
                L.zoomBtnRect.x = L.debugToolbar.panelRect.x - overlapPad - L.zoomBtnRect.w;
            }
        }

        float bx = barX + pad + gripW;
        for (size_t i = 0; i < 6; ++i) {
            if (i == 4) {
                bx += sepExtra; // Group separator before Restart & Stop
            }
            L.debugToolbar.btnRects[i] = Rect(bx, overlayTop + pad, btnW, btnH2);
            bx += btnW + gap;
        }
    }

    return L;
}

size_t FindVisualLineForOffset(const std::vector<VisualLine>& visualLines, size_t offset) noexcept {
    if (visualLines.empty()) return 0;
    for (size_t i = 0; i < visualLines.size(); ++i) {
        const VisualLine& vl = visualLines[i];
        const size_t vlEnd = vl.startOffset + vl.length;
        if (offset >= vl.startOffset && offset <= vlEnd) {
            if (offset == vlEnd && i + 1 < visualLines.size() && visualLines[i + 1].logicalLine == vl.logicalLine) {
                continue;
            }
            return i;
        }
    }
    return visualLines.size() - 1;
}

size_t FindLogicalLineForOffset(const std::vector<LogicalLine>& logicalLines, size_t offset) noexcept {
    if (logicalLines.empty()) return 0;
    for (size_t i = 0; i < logicalLines.size(); ++i) {
        const LogicalLine& ll = logicalLines[i];
        if (offset >= ll.startOffset && offset <= ll.startOffset + ll.length) {
            return i;
        }
    }
    return logicalLines.size() - 1;
}

void DrawWavyUnderline(ICanvas& canvas, float x0, float x1, float y, Color col, float scale) {
    const float step = std::max(2.5f, 3.0f * scale);
    const float amp = std::max(1.2f, 1.5f * scale);
    float x = x0;
    bool up = true;
    while (x < x1) {
        const float nx = std::min(x1, x + step);
        const float yA = up ? (y - amp) : (y + amp);
        const float yB = up ? (y + amp) : (y - amp);
        canvas.DrawLine({x, yA}, {nx, yB}, col, std::max(1.0f, 1.25f * scale));
        x = nx;
        up = !up;
    }
}

} // namespace

SyntaxConfig SyntaxConfig::Cpp() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "C++20",
        .keywords = kCppKeywords,
        .types = kCppTypes,
        .builtins = {},
        .foldOpenKeywords = {},
        .foldCloseKeywords = {},
        .lineComment = "//",
        .sectionPrefix = "// ===",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '#',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .transposeApostrophe = false,
        .highlightJsonKeys = false
    };
}

SyntaxConfig SyntaxConfig::Matlab() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "MATLAB",
        .keywords = kMatlabKeywords,
        .types = kMatlabTypes,
        .builtins = kMatlabBuiltins,
        .foldOpenKeywords = kMatlabFoldOpen,
        .foldCloseKeywords = kMatlabFoldClose,
        .lineComment = "%",
        .sectionPrefix = "%%",
        .blockCommentStart = "%{",
        .blockCommentEnd = "%}",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .transposeApostrophe = true,
        .highlightJsonKeys = false
    };
}

SyntaxConfig SyntaxConfig::Json() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "JSON",
        .keywords = kJsonKeywords,
        .types = {},
        .builtins = {},
        .foldOpenKeywords = {},
        .foldCloseKeywords = {},
        .lineComment = "//",
        .sectionPrefix = "",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = false,
        .highlightPunctuation = true,
        .transposeApostrophe = false,
        .highlightJsonKeys = true
    };
}

SyntaxConfig SyntaxConfig::Python() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "Python",
        .keywords = kPythonKeywords,
        .types = kPythonTypes,
        .builtins = kPythonBuiltins,
        .foldOpenKeywords = {},
        .foldCloseKeywords = {},
        .lineComment = "#",
        .sectionPrefix = "# %%",
        .blockCommentStart = "\"\"\"",
        .blockCommentEnd = "\"\"\"",
        .preprocessorPrefix = '@',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .transposeApostrophe = false,
        .highlightJsonKeys = false
    };
}

SyntaxConfig SyntaxConfig::ForLanguage(std::string_view langTag) noexcept {
    std::string lower;
    lower.reserve(langTag.size());
    for (char c : langTag) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            lower.push_back(ToLowerAscii(c));
        }
    }
    if (lower == "matlab" || lower == "m" || lower == "octave") {
        return Matlab();
    }
    if (lower == "json" || lower == "jsonc") {
        return Json();
    }
    if (lower == "py" || lower == "python" || lower == "python3") {
        return Python();
    }
    if (lower == "plain" || lower == "text" || lower == "txt" || lower == "none") {
        return Plain();
    }
    return Cpp();
}

void TextArea::HighlightLine(
    std::string_view lineText,
    size_t /*lineIndex*/,
    const SyntaxConfig& config,
    std::vector<TextSpan>& outSpans,
    const UiTheme& theme,
    const SyntaxPalette* paletteOverride,
    bool* inOutBlockComment
) {
    outSpans.clear();
    if (lineText.empty()) return;

    const SyntaxPalette& pal = paletteOverride
        ? *paletteOverride
        : (config.palette.has_value() ? *config.palette : theme.syntax);

    if (!config.enabled) {
        outSpans.push_back({0, static_cast<uint32_t>(lineText.size()), pal.identifier});
        return;
    }

    const size_t len = lineText.size();
    size_t i = 0;

    // Stateful multiline block comment continuation from previous line
    if (inOutBlockComment && *inOutBlockComment && !config.blockCommentEnd.empty()) {
        const size_t endPos = lineText.find(config.blockCommentEnd);
        if (endPos == std::string_view::npos) {
            outSpans.push_back({0, static_cast<uint32_t>(len), pal.comment});
            return;
        }
        i = endPos + config.blockCommentEnd.size();
        outSpans.push_back({0, static_cast<uint32_t>(i), pal.comment});
        *inOutBlockComment = false;
    }

    // Skip leading whitespace
    if (i == 0) {
        while (i < len && (lineText[i] == ' ' || lineText[i] == '\t')) {
            ++i;
        }
        if (i > 0) {
            outSpans.push_back({0, static_cast<uint32_t>(i), pal.identifier});
        }
    }

    // Section header check (e.g. "%%" in MATLAB or "// ===" in C++)
    if (!config.sectionPrefix.empty() && i + config.sectionPrefix.size() <= len &&
        lineText.substr(i, config.sectionPrefix.size()) == config.sectionPrefix) {
        outSpans.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(len - i), pal.preprocessor});
        return;
    }

    // Preprocessor directive check
    if (config.preprocessorPrefix != '\0' && i < len && lineText[i] == config.preprocessorPrefix) {
        size_t qPos = lineText.find_first_of("\"<", i + 1);
        if (qPos != std::string_view::npos && config.highlightStrings) {
            outSpans.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(qPos - i), pal.preprocessor});
            outSpans.push_back({static_cast<uint32_t>(qPos), static_cast<uint32_t>(len - qPos), pal.string});
        } else {
            outSpans.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(len - i), pal.preprocessor});
        }
        return;
    }

    while (i < len) {
        // 1. Whitespace run
        if (lineText[i] == ' ' || lineText[i] == '\t') {
            const size_t start = i;
            while (i < len && (lineText[i] == ' ' || lineText[i] == '\t')) {
                ++i;
            }
            outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), pal.identifier});
            continue;
        }

        // 2. Block comment (checked BEFORE lineComment so MATLAB "%{" is recognized before "%")
        if (!config.blockCommentStart.empty() &&
            i + config.blockCommentStart.size() <= len &&
            lineText.substr(i, config.blockCommentStart.size()) == config.blockCommentStart) {
            const size_t start = i;
            i += config.blockCommentStart.size();
            if (!config.blockCommentEnd.empty()) {
                const size_t endPos = lineText.find(config.blockCommentEnd, i);
                if (endPos != std::string_view::npos) {
                    i = endPos + config.blockCommentEnd.size();
                } else {
                    i = len;
                    if (inOutBlockComment) {
                        *inOutBlockComment = true;
                    }
                }
            } else {
                i = len;
            }
            outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), pal.comment});
            continue;
        }

        // 3. Single-line comment
        if (!config.lineComment.empty() &&
            i + config.lineComment.size() <= len &&
            lineText.substr(i, config.lineComment.size()) == config.lineComment) {
            outSpans.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(len - i), pal.comment});
            break;
        }

        // 4. String / char literal (with MATLAB transpose apostrophe disambiguation & JSON key detection)
        if (config.highlightStrings && (lineText[i] == '"' || lineText[i] == '\'')) {
            if (lineText[i] == '\'' && config.transposeApostrophe && i > 0) {
                const char prev = lineText[i - 1];
                if (IsIdentChar(prev) || prev == ')' || prev == ']' || prev == '}' || prev == '.') {
                    outSpans.push_back({static_cast<uint32_t>(i), 1u, config.highlightPunctuation ? pal.punctuation : pal.identifier});
                    ++i;
                    continue;
                }
            }
            const char quote = lineText[i];
            const size_t start = i++;
            while (i < len) {
                if (lineText[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (lineText[i] == quote) {
                    ++i;
                    break;
                }
                ++i;
            }
            Color strCol = pal.string;
            if (config.highlightJsonKeys && quote == '"') {
                size_t look = i;
                while (look < len && (lineText[look] == ' ' || lineText[look] == '\t')) ++look;
                if (look < len && lineText[look] == ':') {
                    strCol = pal.type;
                }
            }
            outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), strCol});
            continue;
        }

        // 5. Numeric literal
        if (config.highlightNumbers &&
            (std::isdigit(static_cast<unsigned char>(lineText[i])) ||
             (lineText[i] == '.' && i + 1 < len && std::isdigit(static_cast<unsigned char>(lineText[i + 1]))))) {
            const size_t start = i++;
            while (i < len && (std::isalnum(static_cast<unsigned char>(lineText[i])) || lineText[i] == '.' ||
                               (!config.transposeApostrophe && lineText[i] == '\''))) {
                ++i;
            }
            outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), pal.number});
            continue;
        }

        // 6. Identifier / keyword / type / builtin / function call
        if (IsIdentStart(lineText[i])) {
            const size_t start = i++;
            while (i < len && IsIdentChar(lineText[i])) {
                ++i;
            }
            const std::string_view word = lineText.substr(start, i - start);
            Color tokenColor = pal.identifier;
            if (ContainsToken(config.keywords, word)) {
                tokenColor = pal.keyword;
            } else if (ContainsToken(config.types, word)) {
                tokenColor = pal.type;
            } else if (ContainsToken(config.builtins, word)) {
                tokenColor = pal.function;
            } else if (config.highlightFunctions) {
                size_t look = i;
                while (look < len && (lineText[look] == ' ' || lineText[look] == '\t')) {
                    ++look;
                }
                if (look < len && lineText[look] == '(') {
                    tokenColor = pal.function;
                }
            }
            outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), tokenColor});
            continue;
        }

        // 7. Punctuation / operators
        const size_t start = i++;
        while (i < len &&
               lineText[i] != ' ' && lineText[i] != '\t' &&
               lineText[i] != '"' && lineText[i] != '\'' &&
               !IsIdentStart(lineText[i]) &&
               !std::isdigit(static_cast<unsigned char>(lineText[i]))) {
            if (!config.blockCommentStart.empty() &&
                i + config.blockCommentStart.size() <= len &&
                lineText.substr(i, config.blockCommentStart.size()) == config.blockCommentStart) {
                break;
            }
            if (!config.lineComment.empty() &&
                i + config.lineComment.size() <= len &&
                lineText.substr(i, config.lineComment.size()) == config.lineComment) {
                break;
            }
            ++i;
        }
        outSpans.push_back({
            static_cast<uint32_t>(start),
            static_cast<uint32_t>(i - start),
            config.highlightPunctuation ? pal.punctuation : pal.identifier
        });
    }
}

void TextArea::CppHighlighter(
    std::string_view lineText,
    size_t lineIndex,
    std::vector<TextSpan>& outSpans,
    const UiTheme& theme,
    void* /*userData*/
) {
    HighlightLine(lineText, lineIndex, SyntaxConfig::Cpp(), outSpans, theme, nullptr, nullptr);
}

TextAreaResponse TextArea::Run(
    Context& ctx,
    std::string* stringValue,
    std::string_view displayFallback,
    const TextAreaOptions& opts,
    std::source_location loc
) {
    const auto scopedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool wasFocused = ctx.IsFocused(id) && !effectiveDisabled;

    ctx.RegisterFocusable(id, effectiveDisabled);

    TextAreaResponse res{};
    static_cast<Response&>(res) = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    }

    const bool mouseJustPressed = res.hovered && ctx.IsMouseClicked() && !effectiveDisabled;
    if ((mouseJustPressed || res.clicked) && !effectiveDisabled) {
        ctx.RequestFocus(id);
    }

    const bool isFocused = ctx.IsFocused(id) && !effectiveDisabled;
    res.focused = isFocused;

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    TextAreaOptions optsCopy = opts;
    optsCopy.label = ctx.AllocText(opts.label);
    optsCopy.description = ctx.AllocText(opts.description);
    optsCopy.badge = ctx.AllocText(opts.badge);
    optsCopy.placeholder = ctx.AllocString(opts.placeholder);
    optsCopy.tooltip = ctx.AllocString(opts.tooltip);

    const std::string_view callerText = (stringValue != nullptr) ? std::string_view(*stringValue) : displayFallback;
    std::string_view valStr = callerText;
    const std::string_view lineCmtPrefix = (opts.syntax.has_value() && !opts.syntax->lineComment.empty())
        ? opts.syntax->lineComment
        : "//";

    auto& state = ctx.GetOrCreateState<TextAreaState>(id);
    if (!isFocused) {
        state.overlayFocus = 0;
        ctx.SetSuspendDirectEditInput(false);
    }

    // Initialize or reset baseline line-tracking table when document is first loaded or externally replaced
    if (!state.baselineInitialized || callerText != state.prevDocumentText) {
        if (state.baselineInitialized && ctx.HasActiveTextEdit(id)) {
            ctx.SetTextEditBuffer(id, callerText);
        }
        std::vector<LogicalLine> initLines;
        SplitLogicalLines(callerText, initLines);
        state.baselineToCurrentLine.assign(initLines.size() + 1, 0);
        state.baselineSignatures.assign(initLines.size() + 1, std::string{});
        for (size_t i = 0; i < initLines.size(); ++i) {
            state.baselineToCurrentLine[i + 1] = i + 1;
            const std::string_view rawLine = callerText.substr(initLines[i].startOffset, initLines[i].length);
            state.baselineSignatures[i + 1] = std::string(ExtractCodeWithoutLineComment(rawLine, lineCmtPrefix));
        }
        state.prevDocumentText.assign(callerText);
        state.prevCursor = std::min(state.cursor, callerText.size());
        state.prevTextSize = callerText.size();
        state.prevLineCount = initLines.size();
        state.baselineInitialized = true;
    }

    const float reqZoom = std::clamp(opts.zoom, 0.5f, 3.0f);
    if (std::abs(state.lastOptsZoom - reqZoom) > 0.001f) {
        state.zoom = reqZoom;
        state.lastOptsZoom = reqZoom;
    }
    state.zoom = std::clamp(state.zoom, 0.5f, 3.0f);

    if (opts.debugToolbarAlign != state.lastDebugToolbarAlign) {
        state.lastDebugToolbarAlign = opts.debugToolbarAlign;
        state.debugToolbarRatio = -1.0f;
    }

    if (opts.allowZoom && !effectiveDisabled && (isFocused || res.hovered)) {
        if (ctx.ConsumeAction(KeyAction::ZoomIn)) {
            state.zoom = std::clamp(state.zoom + 0.10f, 0.5f, 3.0f);
        } else if (ctx.ConsumeAction(KeyAction::ZoomOut)) {
            state.zoom = std::clamp(state.zoom - 0.10f, 0.5f, 3.0f);
        } else if (ctx.ConsumeAction(KeyAction::ZoomReset)) {
            state.zoom = 1.0f;
        }
    }

    auto applyZoomAnchor = [&](float newZoom) {
        const float clampedNew = std::clamp(newZoom, 0.5f, 3.0f);
        if (state.prevAppliedZoom > 0.001f && std::abs(clampedNew - state.prevAppliedZoom) > 0.0001f) {
            const float ratio = clampedNew / state.prevAppliedZoom;
            state.scrollY *= ratio;
            state.scrollX *= ratio;
        }
        state.zoom = clampedNew;
        state.prevAppliedZoom = clampedNew;
    };
    applyZoomAnchor(state.zoom);

    if (!ctx.IsMouseDown()) {
        state.draggingText = false;
        state.draggingWord = false;
        state.draggingVThumb = false;
        state.draggingHThumb = false;
        state.draggingMinimap = false;
        state.draggingDebugToolbar = false;
    }

    const double now = ctx.GetCurrentTime();
    const bool copyFeedbackActive = (now < state.copyFeedbackUntil);
    if (copyFeedbackActive) {
        ctx.RequestRepaint(2);
    }

    const UiTheme& theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();
    const FontStyle textStyle = optsCopy.monospace ? FontStyle::Monospace : FontStyle::Regular;

    const Rect refBounds = (res.rect.w > 0.0f && res.rect.h > 0.0f)
        ? res.rect
        : Rect({0.0f, 0.0f}, TextArea::Measure(valStr, optsCopy, activeSize, canvas, theme));

    // Synchronize active edit buffer for both editable and readOnly
    if (isFocused || ctx.HasActiveTextEdit(id)) {
        valStr = ctx.EnsureTextEdit(id, valStr, /*multiline=*/true, opts.readOnly);
        ctx.SetTextEditAutoClose(id, opts.autoCloseBrackets);
    }

    std::vector<LogicalLine> logicalLines;
    SplitLogicalLines(valStr, logicalLines);

    auto resolveDebugAndDiagnosticMarkers = [&](std::string_view curDoc) {
        if (opts.executionLinePtr != nullptr) {
            optsCopy.executionLine = *opts.executionLinePtr;
        } else if (opts.executionLine > 0 && opts.executionLine < state.baselineToCurrentLine.size() && state.baselineToCurrentLine[opts.executionLine] > 0) {
            optsCopy.executionLine = state.baselineToCurrentLine[opts.executionLine];
        } else {
            optsCopy.executionLine = opts.executionLine;
        }
        if (optsCopy.executionLine > logicalLines.size()) {
            optsCopy.executionLine = logicalLines.empty() ? 0 : logicalLines.size();
        }

        state.resolvedInlineValues.clear();
        state.resolvedInlineValues.reserve(opts.inlineValues.size());
        for (const CodeInlineValue& iv : opts.inlineValues) {
            if (iv.text.empty()) continue;
            const size_t resolvedLine = ResolveTrackedCodeLine(
                curDoc, logicalLines, iv.line, iv.anchor,
                state.baselineToCurrentLine, state.baselineSignatures, lineCmtPrefix
            );
            if (resolvedLine >= 1 && resolvedLine <= logicalLines.size()) {
                state.resolvedInlineValues.push_back(CodeInlineValue{
                    .line = resolvedLine,
                    .text = ctx.AllocString(iv.text),
                    .variant = iv.variant,
                    .anchor = ctx.AllocString(iv.anchor)
                });
            }
        }
        optsCopy.inlineValues = state.resolvedInlineValues;

        state.resolvedDiagnostics.clear();
        state.resolvedDiagnostics.reserve(opts.diagnostics.size());
        for (const CodeDiagnostic& diag : opts.diagnostics) {
            const size_t resolvedLine = ResolveTrackedCodeLine(
                curDoc, logicalLines, diag.line, diag.anchor,
                state.baselineToCurrentLine, state.baselineSignatures, lineCmtPrefix
            );
            if (resolvedLine >= 1 && resolvedLine <= logicalLines.size()) {
                CodeDiagnostic d = diag;
                d.line = resolvedLine;
                d.message = ctx.AllocString(diag.message);
                d.code = ctx.AllocString(diag.code);
                d.anchor = ctx.AllocString(diag.anchor);
                if (!diag.anchor.empty()) {
                    const LogicalLine& ll = logicalLines[resolvedLine - 1];
                    const std::string_view fullLine = curDoc.substr(ll.startOffset, ll.length);
                    if (const size_t anchorPos = fullLine.find(diag.anchor); anchorPos != std::string_view::npos) {
                        d.startCol = static_cast<uint32_t>(anchorPos);
                        d.endCol = static_cast<uint32_t>(anchorPos + diag.anchor.size());
                    }
                }
                state.resolvedDiagnostics.push_back(d);
            }
        }
        optsCopy.diagnostics = state.resolvedDiagnostics;
    };

    resolveDebugAndDiagnosticMarkers(valStr);

    std::vector<FoldRegion> foldRegions;
    if (opts.codeFolding) {
        ComputeFoldRegions(valStr, logicalLines, opts.syntax.has_value() ? &*opts.syntax : nullptr, foldRegions);
        // Prune any stale foldedLines that no longer start a valid fold region
        state.foldedLines.erase(
            std::remove_if(state.foldedLines.begin(), state.foldedLines.end(), [&](size_t fl) {
                return FindFoldRegionStartingAt(foldRegions, fl) == nullptr;
            }),
            state.foldedLines.end()
        );
    } else {
        state.foldedLines.clear();
    }

    std::vector<size_t>& activeBreakpoints = (opts.breakpoints != nullptr)
        ? *opts.breakpoints
        : state.internalBreakpoints;

    auto toggleBreakpointAtLine = [&](size_t line1Based) {
        if (line1Based == 0 || line1Based > logicalLines.size()) return;
        auto it = std::find(activeBreakpoints.begin(), activeBreakpoints.end(), line1Based);
        if (it != activeBreakpoints.end()) {
            activeBreakpoints.erase(it);
        } else {
            activeBreakpoints.push_back(line1Based);
            std::sort(activeBreakpoints.begin(), activeBreakpoints.end());
        }
        res.debugAction = CodeDebugAction::ToggleBreakpoint;
        res.toggledBreakpointLine = line1Based;
    };

    auto ensureLineUnfolded = [&](size_t line0Based) {
        if (state.foldedLines.empty()) return;
        state.foldedLines.erase(
            std::remove_if(state.foldedLines.begin(), state.foldedLines.end(), [&](size_t fl) {
                if (const FoldRegion* r = FindFoldRegionStartingAt(foldRegions, fl)) {
                    return line0Based > r->startLine && line0Based <= r->endLine;
                }
                return false;
            }),
            state.foldedLines.end()
        );
    };

    // Unfold if debugger executionLine stepped into a folded block
    if (optsCopy.executionLine > 0 && optsCopy.executionLine != state.prevExecutionLine) {
        ensureLineUnfolded(optsCopy.executionLine - 1);
    }

    std::vector<VisualLine> visualLines;
    EditorLayoutRects L = ComputeEditorLayout(
        refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
        state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
        state.searchOpen, state.replaceOpen, state.gotoOpen,
        foldRegions, state.foldedLines, visualLines,
        state.debugToolbarRatio
    );

    auto scrollLogicalLineIntoView = [&](size_t line0Based) {
        ensureLineUnfolded(line0Based);
        BuildVisualLines(valStr, logicalLines, opts.wordWrap, L.textViewport.w, L.fontSize, textStyle, canvas, visualLines, foldRegions, state.foldedLines);
        L.totalContentH = static_cast<float>(visualLines.size()) * L.lineHeight;
        L.maxScrollY = std::max(0.0f, L.totalContentH - L.textViewport.h);
        size_t targetVis = 0;
        for (size_t v = 0; v < visualLines.size(); ++v) {
            if (visualLines[v].logicalLine >= line0Based) {
                targetVis = v;
                break;
            }
        }
        const float lineTop = static_cast<float>(targetVis) * L.lineHeight;
        const float lineBot = lineTop + L.lineHeight;
        if (lineTop < state.scrollY || lineBot > state.scrollY + L.textViewport.h) {
            state.scrollY = std::clamp(lineTop - L.textViewport.h * 0.35f, 0.0f, L.maxScrollY);
        }
    };

    if (optsCopy.executionLine > 0 && optsCopy.executionLine != state.prevExecutionLine) {
        scrollLogicalLineIntoView(optsCopy.executionLine - 1);
        state.prevExecutionLine = optsCopy.executionLine;
    } else if (optsCopy.executionLine == 0) {
        state.prevExecutionLine = 0;
    }

    // --- Handle IDE & Debugger Keyboard Shortcuts when focused ---
    bool modifiedByIdeCommand = false;
    if (isFocused && !effectiveDisabled) {
        // KeyAction::Find (default Ctrl+F)
        if (opts.enableSearch && ctx.ConsumeAction(KeyAction::Find)) {
            state.searchOpen = true;
            state.gotoOpen = false;
            state.overlayFocus = 1;
            size_t s0 = 0, s1 = 0;
            if (ctx.GetTextEditSelection(id, s0, s1) && s1 > s0 && (s1 - s0) <= 64) {
                const std::string_view sel = valStr.substr(s0, s1 - s0);
                if (sel.find('\n') == std::string_view::npos) {
                    state.searchQuery.assign(sel);
                }
            }
        }
        // KeyAction::Replace (default Ctrl+H)
        if (opts.enableSearch && !opts.readOnly && ctx.ConsumeAction(KeyAction::Replace)) {
            state.searchOpen = true;
            state.replaceOpen = true;
            state.gotoOpen = false;
            state.overlayFocus = state.searchQuery.empty() ? 1 : 2;
            size_t s0 = 0, s1 = 0;
            if (state.searchQuery.empty() && ctx.GetTextEditSelection(id, s0, s1) && s1 > s0 && (s1 - s0) <= 64) {
                const std::string_view sel = valStr.substr(s0, s1 - s0);
                if (sel.find('\n') == std::string_view::npos) {
                    state.searchQuery.assign(sel);
                }
            }
        }
        // KeyAction::GoToLine (default Ctrl+G)
        if (opts.enableSearch && ctx.ConsumeAction(KeyAction::GoToLine)) {
            state.gotoOpen = true;
            state.searchOpen = false;
            state.overlayFocus = 3;
            state.gotoQuery.clear();
        }
        // KeyAction::ToggleComment (default Ctrl+/)
        if (!opts.readOnly && ctx.ConsumeAction(KeyAction::ToggleComment)) {
            const std::string_view cmtPrefix = (opts.syntax.has_value() && !opts.syntax->lineComment.empty())
                ? opts.syntax->lineComment
                : "//";
            size_t selS = state.selectionStart;
            size_t selE = state.selectionEnd;
            if (selE > selS && selE <= valStr.size() && valStr[selE - 1] == '\n') {
                --selE;
            }
            const size_t firstLine = FindLogicalLineForOffset(logicalLines, selS);
            const size_t lastLine = FindLogicalLineForOffset(logicalLines, selE);

            bool allCommented = true;
            for (size_t li = firstLine; li <= lastLine; ++li) {
                const std::string_view ls = valStr.substr(logicalLines[li].startOffset, logicalLines[li].length);
                size_t p = 0;
                while (p < ls.size() && (ls[p] == ' ' || ls[p] == '\t')) ++p;
                if (p >= ls.size()) continue;
                if (!ls.substr(p).starts_with(cmtPrefix)) {
                    allCommented = false;
                    break;
                }
            }

            std::string newDoc(valStr);
            size_t newCur = state.cursor;
            size_t newAnc = (state.cursor == state.selectionStart) ? state.selectionEnd : state.selectionStart;
            ptrdiff_t cumulativeShift = 0;

            for (size_t li = firstLine; li <= lastLine; ++li) {
                const size_t lineStart = static_cast<size_t>(static_cast<ptrdiff_t>(logicalLines[li].startOffset) + cumulativeShift);
                const size_t lineLen = logicalLines[li].length;
                const std::string_view ls = std::string_view(newDoc).substr(lineStart, lineLen);
                size_t p = 0;
                while (p < ls.size() && (ls[p] == ' ' || ls[p] == '\t')) ++p;
                if (p >= ls.size() && firstLine != lastLine) continue;

                const size_t editPos = lineStart + p;
                if (allCommented && ls.substr(p).starts_with(cmtPrefix)) {
                    size_t remLen = cmtPrefix.size();
                    if (p + remLen < ls.size() && ls[p + remLen] == ' ') {
                        ++remLen;
                    }
                    newDoc.erase(editPos, remLen);
                    if (newCur > editPos) newCur = (newCur >= editPos + remLen) ? (newCur - remLen) : editPos;
                    if (newAnc > editPos) newAnc = (newAnc >= editPos + remLen) ? (newAnc - remLen) : editPos;
                    cumulativeShift -= static_cast<ptrdiff_t>(remLen);
                } else if (!allCommented) {
                    const std::string ins = std::string(cmtPrefix) + " ";
                    newDoc.insert(editPos, ins);
                    if (newCur >= editPos) newCur += ins.size();
                    if (newAnc >= editPos) newAnc += ins.size();
                    cumulativeShift += static_cast<ptrdiff_t>(ins.size());
                }
            }

            ctx.ApplyTextEditChange(id, newDoc, newCur, newAnc);
            valStr = ctx.EnsureTextEdit(id, newDoc, true, opts.readOnly);
            SplitLogicalLines(valStr, logicalLines);
            modifiedByIdeCommand = true;
        }

        // Debugger actions: ToggleBreakpoint (F9), DebugContinue (F5), DebugStop (Shift+F5), DebugStepOver (F10), DebugStepInto (F11), DebugStepOut (Shift+F11)
        if ((opts.breakpoints != nullptr || opts.enableBreakpoints) && ctx.ConsumeAction(KeyAction::ToggleBreakpoint)) {
            const size_t curLine0 = FindLogicalLineForOffset(logicalLines, state.cursor);
            toggleBreakpointAtLine(curLine0 + 1);
        }
        if (ctx.ConsumeAction(KeyAction::DebugContinue)) {
            res.debugAction = CodeDebugAction::Continue;
        } else if (ctx.ConsumeAction(KeyAction::DebugStop)) {
            res.debugAction = CodeDebugAction::Stop;
        }
        if (ctx.ConsumeAction(KeyAction::DebugStepOver)) {
            res.debugAction = CodeDebugAction::StepOver;
        }
        if (ctx.ConsumeAction(KeyAction::DebugStepInto)) {
            res.debugAction = CodeDebugAction::StepInto;
        } else if (ctx.ConsumeAction(KeyAction::DebugStepOut)) {
            res.debugAction = CodeDebugAction::StepOut;
        }
    }

    // Compute search matches if search is open
    std::vector<SearchMatch> searchMatches;
    if (state.searchOpen && !state.searchQuery.empty()) {
        FindAllSearchMatches(valStr, logicalLines, state.searchQuery, state.matchCase, state.wholeWord, searchMatches);
    }
    state.searchMatchCount = searchMatches.size();
    if (state.activeMatchIdx >= searchMatches.size()) {
        state.activeMatchIdx = 0;
    }

    auto jumpToSearchMatch = [&](size_t matchIdx) {
        if (searchMatches.empty()) return;
        state.activeMatchIdx = matchIdx % searchMatches.size();
        const SearchMatch& m = searchMatches[state.activeMatchIdx];
        ctx.SetTextEditCursor(id, m.endOffset, m.startOffset);
        state.cursor = m.endOffset;
        state.selectionStart = m.startOffset;
        state.selectionEnd = m.endOffset;
        scrollLogicalLineIntoView(m.logicalLine);
    };

    auto performReplaceOne = [&]() {
        if (opts.readOnly || searchMatches.empty()) return;
        if (state.activeMatchIdx >= searchMatches.size()) state.activeMatchIdx = 0;
        const SearchMatch m = searchMatches[state.activeMatchIdx];
        std::string newDoc(valStr);
        newDoc.replace(m.startOffset, m.endOffset - m.startOffset, state.replaceQuery);
        const size_t newCur = m.startOffset + state.replaceQuery.size();
        ctx.ApplyTextEditChange(id, newDoc, newCur, newCur);
        valStr = ctx.EnsureTextEdit(id, newDoc, true, opts.readOnly);
        SplitLogicalLines(valStr, logicalLines);
        FindAllSearchMatches(valStr, logicalLines, state.searchQuery, state.matchCase, state.wholeWord, searchMatches);
        state.searchMatchCount = searchMatches.size();
        if (!searchMatches.empty()) {
            if (state.activeMatchIdx >= searchMatches.size()) state.activeMatchIdx = 0;
            jumpToSearchMatch(state.activeMatchIdx);
        }
        modifiedByIdeCommand = true;
    };

    auto performReplaceAll = [&]() {
        if (opts.readOnly || searchMatches.empty() || state.searchQuery.empty()) return;
        std::string newDoc;
        newDoc.reserve(valStr.size());
        size_t lastPos = 0;
        for (const SearchMatch& m : searchMatches) {
            newDoc.append(valStr.substr(lastPos, m.startOffset - lastPos));
            newDoc.append(state.replaceQuery);
            lastPos = m.endOffset;
        }
        newDoc.append(valStr.substr(lastPos));
        const size_t newCur = std::min(state.cursor, newDoc.size());
        ctx.ApplyTextEditChange(id, newDoc, newCur, newCur);
        valStr = ctx.EnsureTextEdit(id, newDoc, true, opts.readOnly);
        SplitLogicalLines(valStr, logicalLines);
        searchMatches.clear();
        state.searchMatchCount = 0;
        state.activeMatchIdx = 0;
        modifiedByIdeCommand = true;
    };

    // FindNext / FindPrev (default F3 / Shift+F3): Find Next / Previous even when focus is in code editor
    if (isFocused && !effectiveDisabled) {
        const bool findNext = ctx.ConsumeAction(KeyAction::FindNext);
        const bool findPrev = !findNext && ctx.ConsumeAction(KeyAction::FindPrev);
        if (findNext || findPrev) {
            if (!state.searchOpen) {
                state.searchOpen = true;
            }
            if (!searchMatches.empty()) {
                const size_t n = searchMatches.size();
                const size_t nextIdx = findPrev ? ((state.activeMatchIdx + n - 1) % n) : ((state.activeMatchIdx + 1) % n);
                jumpToSearchMatch(nextIdx);
            }
        }
    }

    // Handle keyboard input inside Find / Replace / GoToLine overlay when overlayFocus != 0
    if (isFocused && !effectiveDisabled && state.overlayFocus != 0) {
        std::string* targetStr = (state.overlayFocus == 1) ? &state.searchQuery
                               : (state.overlayFocus == 2) ? &state.replaceQuery
                               : &state.gotoQuery;

        const auto typedList = ctx.ConsumePendingTextInput();
        bool queryEdited = false;
        for (const std::string& t : typedList) {
            for (char c : t) {
                if (c >= 32 && c != 127) {
                    if (state.overlayFocus == 3 && !std::isdigit(static_cast<unsigned char>(c))) {
                        continue;
                    }
                    targetStr->push_back(c);
                    queryEdited = true;
                }
            }
        }

        while (ctx.ConsumeAction(KeyAction::DeletePrevWord)) {
            if (!targetStr->empty()) {
                targetStr->clear();
                queryEdited = true;
            }
        }
        while (ctx.ConsumeAction(KeyAction::DeletePrevChar)) {
            if (!targetStr->empty()) {
                targetStr->pop_back();
                queryEdited = true;
            }
        }
        while (ctx.ConsumeAction(KeyAction::DeleteNextChar) || ctx.ConsumeAction(KeyAction::DeleteNextWord)) {
            targetStr->clear();
            queryEdited = true;
        }
        if (ctx.ConsumeAction(KeyAction::Indent) || ctx.ConsumeAction(KeyAction::Unindent)) {
            if (state.searchOpen) {
                if (!state.replaceOpen && !opts.readOnly) {
                    state.replaceOpen = true;
                }
                if (state.replaceOpen && !opts.readOnly) {
                    state.overlayFocus = (state.overlayFocus == 1) ? 2 : 1;
                }
            }
        }
        if (ctx.ConsumeAction(KeyAction::Cancel)) {
            state.searchOpen = false;
            state.gotoOpen = false;
            state.overlayFocus = 0;
        }
        KeyModifiers kMods{};
        if (ctx.ConsumeAction(KeyAction::Confirm) || ctx.ConsumeKey(Key::Enter, kMods)) {
            if (state.overlayFocus == 1) {
                if (!searchMatches.empty()) {
                    const size_t n = searchMatches.size();
                    const size_t nextIdx = kMods.shift ? ((state.activeMatchIdx + n - 1) % n) : ((state.activeMatchIdx + 1) % n);
                    jumpToSearchMatch(nextIdx);
                }
            } else if (state.overlayFocus == 2) {
                if (kMods.ctrl && kMods.alt) {
                    performReplaceAll();
                } else {
                    performReplaceOne();
                }
            } else if (state.overlayFocus == 3) {
                size_t targetLine1 = 0;
                const auto [ptr, ec] = std::from_chars(state.gotoQuery.data(), state.gotoQuery.data() + state.gotoQuery.size(), targetLine1);
                if (ec == std::errc{} && targetLine1 >= 1) {
                    const size_t line0 = std::min(targetLine1 - 1, logicalLines.size() - 1);
                    const size_t destOffset = logicalLines[line0].startOffset;
                    ctx.SetTextEditCursor(id, destOffset, destOffset);
                    state.cursor = destOffset;
                    state.selectionStart = destOffset;
                    state.selectionEnd = destOffset;
                    scrollLogicalLineIntoView(line0);
                }
                state.gotoOpen = false;
                state.overlayFocus = 0;
            }
        }

        if (queryEdited && state.overlayFocus == 1) {
            FindAllSearchMatches(valStr, logicalLines, state.searchQuery, state.matchCase, state.wholeWord, searchMatches);
            state.searchMatchCount = searchMatches.size();
            state.activeMatchIdx = 0;
            for (size_t mi = 0; mi < searchMatches.size(); ++mi) {
                if (searchMatches[mi].startOffset >= state.cursor) {
                    state.activeMatchIdx = mi;
                    break;
                }
            }
            if (!searchMatches.empty()) {
                scrollLogicalLineIntoView(searchMatches[state.activeMatchIdx].logicalLine);
            }
        }
    } else if (isFocused && !effectiveDisabled && (state.searchOpen || state.gotoOpen)) {
        // Pressing Cancel (Escape) while in editor with search/goto bar open closes the bar without losing focus
        if (ctx.ConsumeAction(KeyAction::Cancel)) {
            state.searchOpen = false;
            state.gotoOpen = false;
            state.overlayFocus = 0;
        }
    }

    // Recompute layout if overlay state changed
    if (opts.codeFolding) {
        ComputeFoldRegions(valStr, logicalLines, opts.syntax.has_value() ? &*opts.syntax : nullptr, foldRegions);
    }
    L = ComputeEditorLayout(
        refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
        state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
        state.searchOpen, state.replaceOpen, state.gotoOpen,
        foldRegions, state.foldedLines, visualLines,
        state.debugToolbarRatio
    );

    const Vec2 mousePos = ctx.GetMousePos();
    const bool copyHovered = opts.showCopyButton && res.hovered && L.copyBtnRect.w > 0.0f && L.copyBtnRect.Contains(mousePos);
    const bool zoomHovered = opts.allowZoom && res.hovered && L.zoomBtnRect.w > 0.0f && L.zoomBtnRect.Contains(mousePos);
    const bool vThumbHovered = L.needVScroll && res.hovered && L.vThumbRect.Contains(mousePos);
    const bool hThumbHovered = L.needHScroll && res.hovered && L.hThumbRect.Contains(mousePos);
    const bool vTrackHovered = L.needVScroll && res.hovered && L.vTrackRect.Contains(mousePos);
    const bool hTrackHovered = L.needHScroll && res.hovered && L.hTrackRect.Contains(mousePos);
    const bool minimapHovered = L.hasMinimap && res.hovered && L.minimapRect.Contains(mousePos);

    // Hit-test Search/Replace overlay buttons
    int hoveredOverlayBtn = -1;
    if ((state.searchOpen || state.gotoOpen) && res.hovered && L.searchOverlay.panelRect.Contains(mousePos)) {
        const auto& S = L.searchOverlay;
        if (S.toggleReplaceBtn.w > 0.0f && S.toggleReplaceBtn.Contains(mousePos)) hoveredOverlayBtn = 0;
        else if (S.caseBtnRect.w > 0.0f && S.caseBtnRect.Contains(mousePos)) hoveredOverlayBtn = 1;
        else if (S.wordBtnRect.w > 0.0f && S.wordBtnRect.Contains(mousePos)) hoveredOverlayBtn = 2;
        else if (S.prevBtnRect.w > 0.0f && S.prevBtnRect.Contains(mousePos)) hoveredOverlayBtn = 3;
        else if (S.nextBtnRect.w > 0.0f && S.nextBtnRect.Contains(mousePos)) hoveredOverlayBtn = 4;
        else if (S.closeBtnRect.w > 0.0f && S.closeBtnRect.Contains(mousePos)) hoveredOverlayBtn = 5;
        else if (S.replaceOneBtnRect.w > 0.0f && S.replaceOneBtnRect.Contains(mousePos)) hoveredOverlayBtn = 6;
        else if (S.replaceAllBtnRect.w > 0.0f && S.replaceAllBtnRect.Contains(mousePos)) hoveredOverlayBtn = 7;
        else if (S.findInputRect.Contains(mousePos)) hoveredOverlayBtn = 10;
        else if (S.replaceInputRect.w > 0.0f && S.replaceInputRect.Contains(mousePos)) hoveredOverlayBtn = 11;
        else hoveredOverlayBtn = 99;
    }

    // Hit-test Debug Toolbar buttons & 6-dot drag grip handle
    int hoveredDebugBtn = -1;
    if (opts.showDebugToolbar && res.hovered && L.debugToolbar.panelRect.w > 0.0f && L.debugToolbar.panelRect.Contains(mousePos)) {
        for (int i = 0; i < 6; ++i) {
            if (L.debugToolbar.btnRects[static_cast<size_t>(i)].Contains(mousePos)) {
                hoveredDebugBtn = i;
                break;
            }
        }
        if (hoveredDebugBtn < 0) {
            hoveredDebugBtn = L.debugToolbar.gripRect.Contains(mousePos) ? 98 : 99;
        }
    }

    // Hit-test Gutter (Breakpoints & Code Folding chevrons)
    int hoveredGutterLine = -1;
    int hoveredFoldLine = -1;
    if (res.hovered && L.gutterRect.w > 0.0f && L.gutterRect.Contains(mousePos)) {
        const float relY = (mousePos.y - L.textViewport.y) + state.scrollY;
        const int rawVisIdx = static_cast<int>(std::floor(relY / std::max(1.0f, L.lineHeight)));
        if (rawVisIdx >= 0 && rawVisIdx < static_cast<int>(visualLines.size())) {
            const VisualLine& vl = visualLines[static_cast<size_t>(rawVisIdx)];
            if (vl.isFirstSubLine) {
                if (opts.codeFolding && mousePos.x >= L.gutterRect.Right() - L.foldColWidth - 2.0f) {
                    if (FindFoldRegionStartingAt(foldRegions, vl.logicalLine) != nullptr) {
                        hoveredFoldLine = static_cast<int>(vl.logicalLine);
                    }
                } else if (opts.breakpoints != nullptr || opts.enableBreakpoints) {
                    hoveredGutterLine = static_cast<int>(vl.logicalLine);
                }
            }
        }
    }

    // Handle mouse wheel
    if (res.hovered && !effectiveDisabled) {
        const Vec2 wheel = ctx.GetMouseWheel();
        if (std::abs(wheel.y) > 0.001f || std::abs(wheel.x) > 0.001f) {
            if (opts.allowZoom && ctx.GetKeyModifiers().ctrl && std::abs(wheel.y) > 0.001f) {
                const float prevZoom = state.zoom;
                applyZoomAnchor(state.zoom + (wheel.y > 0.0f ? 0.10f : -0.10f));
                if (std::abs(state.zoom - prevZoom) > 0.001f) {
                    L = ComputeEditorLayout(
                        refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
                        state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                        state.searchOpen, state.replaceOpen, state.gotoOpen,
                        foldRegions, state.foldedLines, visualLines,
                        state.debugToolbarRatio
                    );
                }
                ctx.ConsumeMouseWheel();
            } else if (!opts.wordWrap && (ctx.GetKeyModifiers().shift || std::abs(wheel.x) > std::abs(wheel.y)) && L.maxScrollX > 0.0f) {
                const float delta = (std::abs(wheel.x) > 0.001f) ? wheel.x : wheel.y;
                state.scrollX = std::clamp(state.scrollX - delta * L.lineHeight * 2.5f, 0.0f, L.maxScrollX);
                ctx.ConsumeMouseWheel();
            } else if (!opts.autoHeight && L.maxScrollY > 0.0f && std::abs(wheel.y) > 0.001f) {
                state.scrollY = std::clamp(state.scrollY - wheel.y * L.lineHeight * 2.5f, 0.0f, L.maxScrollY);
                ctx.ConsumeMouseWheel();
            }
        }
    }

    // Handle LMB press on overlay buttons, debug toolbar, gutter, minimap, scrollbars, or text viewport
    bool consumedByChrome = false;
    if (mouseJustPressed) {
        if (hoveredOverlayBtn >= 0) {
            consumedByChrome = true;
            if (hoveredOverlayBtn == 0) {
                state.replaceOpen = !state.replaceOpen;
            } else if (hoveredOverlayBtn == 1) {
                state.matchCase = !state.matchCase;
                FindAllSearchMatches(valStr, logicalLines, state.searchQuery, state.matchCase, state.wholeWord, searchMatches);
                state.searchMatchCount = searchMatches.size();
            } else if (hoveredOverlayBtn == 2) {
                state.wholeWord = !state.wholeWord;
                FindAllSearchMatches(valStr, logicalLines, state.searchQuery, state.matchCase, state.wholeWord, searchMatches);
                state.searchMatchCount = searchMatches.size();
            } else if (hoveredOverlayBtn == 3 && !searchMatches.empty()) {
                const size_t n = searchMatches.size();
                jumpToSearchMatch((state.activeMatchIdx + n - 1) % n);
            } else if (hoveredOverlayBtn == 4 && !searchMatches.empty()) {
                jumpToSearchMatch((state.activeMatchIdx + 1) % searchMatches.size());
            } else if (hoveredOverlayBtn == 5) {
                state.searchOpen = false;
                state.gotoOpen = false;
                state.overlayFocus = 0;
            } else if (hoveredOverlayBtn == 6) {
                performReplaceOne();
            } else if (hoveredOverlayBtn == 7) {
                performReplaceAll();
            } else if (hoveredOverlayBtn == 10) {
                state.overlayFocus = state.gotoOpen ? 3 : 1;
            } else if (hoveredOverlayBtn == 11) {
                state.overlayFocus = 2;
            }
        } else if (hoveredDebugBtn >= 0) {
            consumedByChrome = true;
            if (hoveredDebugBtn == 0) res.debugAction = CodeDebugAction::Continue;
            else if (hoveredDebugBtn == 1) res.debugAction = CodeDebugAction::StepOver;
            else if (hoveredDebugBtn == 2) res.debugAction = CodeDebugAction::StepInto;
            else if (hoveredDebugBtn == 3) res.debugAction = CodeDebugAction::StepOut;
            else if (hoveredDebugBtn == 4) res.debugAction = CodeDebugAction::Restart;
            else if (hoveredDebugBtn == 5) res.debugAction = CodeDebugAction::Stop;
            else if (hoveredDebugBtn == 98 || hoveredDebugBtn == 99) {
                const float dist = (mousePos - state.lastClickPos).Length();
                if ((now - state.lastClickTime) <= 0.35 && dist <= 6.0f) {
                    state.debugToolbarRatio = -1.0f;
                    state.draggingDebugToolbar = false;
                    L = ComputeEditorLayout(
                        refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
                        state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                        state.searchOpen, state.replaceOpen, state.gotoOpen,
                        foldRegions, state.foldedLines, visualLines,
                        state.debugToolbarRatio
                    );
                } else {
                    state.draggingDebugToolbar = true;
                    state.debugToolbarGrabOffset = mousePos.x - L.debugToolbar.panelRect.x;
                }
                state.lastClickTime = now;
                state.lastClickPos = mousePos;
            }
        } else if (hoveredFoldLine >= 0) {
            consumedByChrome = true;
            const size_t fl = static_cast<size_t>(hoveredFoldLine);
            auto it = std::find(state.foldedLines.begin(), state.foldedLines.end(), fl);
            if (it != state.foldedLines.end()) {
                state.foldedLines.erase(it);
            } else {
                state.foldedLines.push_back(fl);
            }
            L = ComputeEditorLayout(
                refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
                state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                state.searchOpen, state.replaceOpen, state.gotoOpen,
                foldRegions, state.foldedLines, visualLines,
                state.debugToolbarRatio
            );
        } else if (hoveredGutterLine >= 0) {
            consumedByChrome = true;
            toggleBreakpointAtLine(static_cast<size_t>(hoveredGutterLine) + 1);
        } else if (copyHovered) {
            consumedByChrome = true;
            std::string_view toCopy = valStr;
            if (state.selectionEnd > state.selectionStart && state.selectionEnd <= valStr.size()) {
                toCopy = valStr.substr(state.selectionStart, state.selectionEnd - state.selectionStart);
            }
            ctx.SetClipboardText(toCopy);
            state.copyFeedbackUntil = now + 1.5;
            ctx.RequestRepaint(3);
        } else if (zoomHovered) {
            consumedByChrome = true;
            applyZoomAnchor(1.0f);
            L = ComputeEditorLayout(
                refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
                state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                state.searchOpen, state.replaceOpen, state.gotoOpen,
                foldRegions, state.foldedLines, visualLines,
                state.debugToolbarRatio
            );
        } else if (minimapHovered) {
            consumedByChrome = true;
            state.draggingMinimap = true;
        } else if (vThumbHovered) {
            consumedByChrome = true;
            state.draggingVThumb = true;
            state.dragGrabOffset = mousePos.y - L.vThumbRect.y;
        } else if (vTrackHovered) {
            consumedByChrome = true;
            const float halfThumb = L.vThumbRect.h * 0.5f;
            const float maxTravel = std::max(1.0f, L.vTrackRect.h - L.vThumbRect.h);
            const float t = std::clamp((mousePos.y - L.vTrackRect.y - halfThumb) / maxTravel, 0.0f, 1.0f);
            state.scrollY = t * L.maxScrollY;
            state.draggingVThumb = true;
            state.dragGrabOffset = halfThumb;
        } else if (hThumbHovered) {
            consumedByChrome = true;
            state.draggingHThumb = true;
            state.dragGrabOffset = mousePos.x - L.hThumbRect.x;
        } else if (hTrackHovered) {
            consumedByChrome = true;
            const float halfThumb = L.hThumbRect.w * 0.5f;
            const float maxTravel = std::max(1.0f, L.hTrackRect.w - L.hThumbRect.w);
            const float t = std::clamp((mousePos.x - L.hTrackRect.x - halfThumb) / maxTravel, 0.0f, 1.0f);
            state.scrollX = t * L.maxScrollX;
            state.draggingHThumb = true;
            state.dragGrabOffset = halfThumb;
        } else if (L.editorBox.Contains(mousePos)) {
            state.overlayFocus = 0;
            state.draggingText = true;
        }
    }

    if (state.draggingDebugToolbar && ctx.IsMouseDown() && opts.showDebugToolbar && L.debugToolbar.panelRect.w > 0.0f) {
        hoveredDebugBtn = 98;
        const float travel = L.debugToolbar.maxBarX - L.debugToolbar.minBarX;
        if (travel > 0.5f) {
            const float targetX = mousePos.x - state.debugToolbarGrabOffset;
            state.debugToolbarRatio = std::clamp((targetX - L.debugToolbar.minBarX) / travel, 0.0f, 1.0f);
            L = ComputeEditorLayout(
                refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
                state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                state.searchOpen, state.replaceOpen, state.gotoOpen,
                foldRegions, state.foldedLines, visualLines,
                state.debugToolbarRatio
            );
        }
    }
    if (state.draggingMinimap && ctx.IsMouseDown() && L.hasMinimap && L.minimapRect.h > 0.0f) {
        const float microStep = theme.Scale(3.0f);
        const float totalMiniH = std::max(1.0f, static_cast<float>(visualLines.size()) * microStep);
        const float effMiniH = std::min(L.minimapRect.h, totalMiniH);
        const float t = std::clamp((mousePos.y - L.minimapRect.y) / std::max(1.0f, effMiniH), 0.0f, 1.0f);
        state.scrollY = t * L.maxScrollY;
    }
    if (state.draggingVThumb && ctx.IsMouseDown() && L.needVScroll) {
        const float maxTravel = std::max(1.0f, L.vTrackRect.h - L.vThumbRect.h);
        const float t = std::clamp((mousePos.y - L.vTrackRect.y - state.dragGrabOffset) / maxTravel, 0.0f, 1.0f);
        state.scrollY = t * L.maxScrollY;
    }
    if (state.draggingHThumb && ctx.IsMouseDown() && L.needHScroll) {
        const float maxTravel = std::max(1.0f, L.hTrackRect.w - L.hThumbRect.w);
        const float t = std::clamp((mousePos.x - L.hTrackRect.x - state.dragGrabOffset) / maxTravel, 0.0f, 1.0f);
        state.scrollX = t * L.maxScrollX;
    }

    state.scrollY = std::clamp(state.scrollY, 0.0f, L.maxScrollY);
    state.scrollX = std::clamp(state.scrollX, 0.0f, L.maxScrollX);

    size_t cursor = state.cursor;
    size_t selAnchor = state.cursor;
    std::string_view displayText = valStr;

    if (isFocused || ctx.HasActiveTextEdit(id)) {
        // 2D Mouse hit-testing for cursor & selection
        if (!consumedByChrome && (mouseJustPressed || (state.draggingText && ctx.IsMouseDown()))) {
            const float relY = (mousePos.y - L.textViewport.y) + state.scrollY;
            const float relX = (mousePos.x - L.textViewport.x) + state.scrollX;

            const int rawVisIdx = static_cast<int>(std::floor(relY / std::max(1.0f, L.lineHeight)));
            const size_t visIdx = static_cast<size_t>(std::clamp(rawVisIdx, 0, static_cast<int>(visualLines.size()) - 1));
            const VisualLine& vl = visualLines[visIdx];
            const std::string_view subStr = valStr.substr(vl.startOffset, vl.length);
            const size_t colInSub = HitTestColumnInLine(canvas, subStr, relX, L.fontSize, textStyle);
            const size_t hitIdx = vl.startOffset + colInSub;

            if (mouseJustPressed) {
                const float dist = (mousePos - state.lastClickPos).Length();
                if (wasFocused && (now - state.lastClickTime) <= 0.35 && dist <= 6.0f) {
                    state.clickCount += 1;
                } else {
                    state.clickCount = 1;
                }
                state.lastClickTime = now;
                state.lastClickPos = mousePos;

                if (state.clickCount == 1) {
                    state.draggingWord = false;
                    if (ctx.GetKeyModifiers().shift && wasFocused) {
                        const size_t anchor = ctx.GetTextEditAnchor(id);
                        ctx.SetTextEditCursor(id, hitIdx, anchor);
                    } else {
                        ctx.SetTextEditCursor(id, hitIdx, hitIdx);
                    }
                } else if (state.clickCount == 2) {
                    size_t wStart = 0;
                    size_t wEnd = 0;
                    TextUtil::SelectWordAt(valStr, hitIdx, wStart, wEnd);
                    state.draggingWord = true;
                    state.wordDragStart = wStart;
                    state.wordDragEnd = wEnd;
                    ctx.SetTextEditCursor(id, wEnd, wStart);
                } else {
                    state.draggingWord = false;
                    const LogicalLine& ll = logicalLines[vl.logicalLine];
                    ctx.SetTextEditCursor(id, ll.startOffset + ll.length, ll.startOffset);
                }
            } else if (state.draggingText) {
                if (mousePos.y < L.textViewport.y && state.scrollY > 0.0f) {
                    state.scrollY = std::max(0.0f, state.scrollY - L.lineHeight * 0.35f);
                } else if (mousePos.y > L.textViewport.Bottom() && state.scrollY < L.maxScrollY) {
                    state.scrollY = std::min(L.maxScrollY, state.scrollY + L.lineHeight * 0.35f);
                }
                if (!opts.wordWrap) {
                    if (mousePos.x < L.textViewport.x && state.scrollX > 0.0f) {
                        state.scrollX = std::max(0.0f, state.scrollX - L.lineHeight * 0.5f);
                    } else if (mousePos.x > L.textViewport.Right() && state.scrollX < L.maxScrollX) {
                        state.scrollX = std::min(L.maxScrollX, state.scrollX + L.lineHeight * 0.5f);
                    }
                }

                if (state.clickCount >= 3) {
                    const LogicalLine& ll = logicalLines[vl.logicalLine];
                    ctx.SetTextEditCursor(id, ll.startOffset + ll.length, ll.startOffset);
                } else if (state.draggingWord) {
                    size_t wStart = 0;
                    size_t wEnd = 0;
                    TextUtil::SelectWordAt(valStr, hitIdx, wStart, wEnd);
                    if (hitIdx < state.wordDragStart) {
                        ctx.SetTextEditCursor(id, wStart, state.wordDragEnd);
                    } else {
                        ctx.SetTextEditCursor(id, wEnd, state.wordDragStart);
                    }
                } else {
                    const size_t anchor = ctx.GetTextEditAnchor(id);
                    ctx.SetTextEditCursor(id, hitIdx, anchor);
                }
            }
        }

        bool changed = false;
        bool submitted = false;
        displayText = ctx.GetOrStartTextEdit(id, valStr, cursor, selAnchor, changed, submitted, /*multiline=*/true, opts.readOnly);
        changed = changed || modifiedByIdeCommand || (displayText != state.prevDocumentText);

        if (changed && !opts.readOnly) {
            if (stringValue != nullptr) {
                *stringValue = displayText;
            }
            res.changed = true;
            std::vector<LogicalLine> oldLogicalLines;
            SplitLogicalLines(state.prevDocumentText, oldLogicalLines);
            SplitLogicalLines(displayText, logicalLines);
            const size_t oldLineCount = oldLogicalLines.size();
            const size_t newLineCount = logicalLines.size();

            // Automatically shift breakpoints, executionLine, and baseline line mapping when lines are inserted/deleted
            if (newLineCount != oldLineCount) {
                const ptrdiff_t diff = static_cast<ptrdiff_t>(newLineCount) - static_cast<ptrdiff_t>(oldLineCount);
                const size_t safePreCur = std::min(state.prevCursor, state.prevDocumentText.size());
                const size_t oldEditLine0 = FindLogicalLineForOffset(oldLogicalLines, safePreCur);
                const size_t oldEditLine1 = oldEditLine0 + 1;
                const size_t safePostCur = std::min(cursor, displayText.size());
                const size_t newEditLine0 = FindLogicalLineForOffset(logicalLines, safePostCur);
                const size_t newEditLine1 = newEditLine0 + 1;

                size_t shiftStartLine1 = oldEditLine1;
                if (diff > 0) {
                    const bool hasCodeBefore = HasCodeBeforeOffsetOnLine(
                        state.prevDocumentText,
                        oldLogicalLines[oldEditLine0],
                        safePreCur
                    );
                    shiftStartLine1 = hasCodeBefore ? (oldEditLine1 + 1) : oldEditLine1;
                } else {
                    shiftStartLine1 = newEditLine1 + 1;
                }

                const ptrdiff_t minTargetLine = (diff < 0) ? static_cast<ptrdiff_t>(newEditLine1) : 1;
                auto shiftLineNum = [&](size_t& lineNum) {
                    if (lineNum == 0) return;
                    if (lineNum >= shiftStartLine1) {
                        const ptrdiff_t shifted = std::max<ptrdiff_t>(minTargetLine, static_cast<ptrdiff_t>(lineNum) + diff);
                        lineNum = std::min<size_t>(static_cast<size_t>(shifted), newLineCount);
                    } else if (lineNum > newLineCount) {
                        lineNum = newLineCount;
                    }
                };

                for (size_t& bp : activeBreakpoints) {
                    shiftLineNum(bp);
                }
                if (activeBreakpoints.size() > 1) {
                    std::sort(activeBreakpoints.begin(), activeBreakpoints.end());
                    activeBreakpoints.erase(std::unique(activeBreakpoints.begin(), activeBreakpoints.end()), activeBreakpoints.end());
                }

                for (size_t b = 1; b < state.baselineToCurrentLine.size(); ++b) {
                    shiftLineNum(state.baselineToCurrentLine[b]);
                }

                if (opts.executionLinePtr != nullptr && *opts.executionLinePtr > 0) {
                    shiftLineNum(*opts.executionLinePtr);
                    optsCopy.executionLine = *opts.executionLinePtr;
                    state.prevExecutionLine = *opts.executionLinePtr;
                }

                res.lineDelta = static_cast<int>(diff);
                res.editLine = (diff > 0) ? oldEditLine1 : newEditLine1;
            }

            state.prevDocumentText.assign(displayText);
            state.prevLineCount = newLineCount;
            resolveDebugAndDiagnosticMarkers(displayText);

            if (opts.codeFolding) {
                ComputeFoldRegions(displayText, logicalLines, opts.syntax.has_value() ? &*opts.syntax : nullptr, foldRegions);
            }
            L = ComputeEditorLayout(
                refBounds, displayText, logicalLines, optsCopy, activeSize, canvas, theme,
                state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                state.searchOpen, state.replaceOpen, state.gotoOpen,
                foldRegions, state.foldedLines, visualLines,
                state.debugToolbarRatio
            );
        }

        state.cursor = cursor;
        state.selectionStart = std::min(cursor, selAnchor);
        state.selectionEnd = std::max(cursor, selAnchor);

        // If cursor moved into a folded region via keyboard navigation, unfold it
        const size_t curLogical = FindLogicalLineForOffset(logicalLines, cursor);
        if (IsLineHiddenByFold(foldRegions, state.foldedLines, curLogical)) {
            ensureLineUnfolded(curLogical);
            L = ComputeEditorLayout(
                refBounds, displayText, logicalLines, optsCopy, activeSize, canvas, theme,
                state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                state.searchOpen, state.replaceOpen, state.gotoOpen,
                foldRegions, state.foldedLines, visualLines,
                state.debugToolbarRatio
            );
        }

        // Auto-scroll to keep active cursor visible when cursor or text changes
        if (cursor != state.prevCursor || displayText.size() != state.prevTextSize) {
            const size_t curVisRow = FindVisualLineForOffset(visualLines, cursor);
            const VisualLine& cvl = visualLines[curVisRow];
            const float curTop = static_cast<float>(curVisRow) * L.lineHeight;
            const float curBot = curTop + L.lineHeight;

            if (curTop < state.scrollY) {
                state.scrollY = curTop;
            } else if (curBot > state.scrollY + L.textViewport.h && L.textViewport.h > 0.0f) {
                state.scrollY = curBot - L.textViewport.h;
            }

            if (!opts.wordWrap && L.textViewport.w > 0.0f) {
                const size_t colOffset = (cursor >= cvl.startOffset) ? std::min(cursor - cvl.startOffset, cvl.length) : 0;
                const std::string_view prefix = displayText.substr(cvl.startOffset, colOffset);
                const float curX = MeasureSegmentWidth(canvas, prefix, L.fontSize, textStyle);
                const float margin = theme.Scale(16.0f);
                if (curX < state.scrollX + margin) {
                    state.scrollX = std::max(0.0f, curX - margin);
                } else if (curX > state.scrollX + L.textViewport.w - margin) {
                    state.scrollX = std::min(L.maxScrollX, curX - L.textViewport.w + margin);
                }
            }

            state.scrollY = std::clamp(state.scrollY, 0.0f, L.maxScrollY);
            state.scrollX = std::clamp(state.scrollX, 0.0f, L.maxScrollX);
            state.prevCursor = cursor;
            state.prevTextSize = displayText.size();
        }
    } else {
        state.selectionStart = state.cursor;
        state.selectionEnd = state.cursor;
    }

    // Set suspend direct edit input for the next frame's pre-BeginFrame events if overlay input is focused
    ctx.SetSuspendDirectEditInput(isFocused && state.overlayFocus != 0);

    // Tooltips for Debug Toolbar & Search/Replace overlay buttons
    if (hoveredDebugBtn >= 0 && hoveredDebugBtn < 6) {
        static constexpr std::array<std::string_view, 6> kDbgTips = {
            "Continue (F5)",
            "Step Over (F10)",
            "Step Into (F11)",
            "Step Out (Shift+F11)",
            "Restart Debugger",
            "Stop Debugging (Shift+F5)"
        };
        ctx.SetTooltip(kDbgTips[static_cast<size_t>(hoveredDebugBtn)], L.debugToolbar.btnRects[static_cast<size_t>(hoveredDebugBtn)]);
    } else if (hoveredDebugBtn == 98 && !state.draggingDebugToolbar) {
        ctx.SetTooltip("Drag to move Debug Toolbar (Double-click to reset)", L.debugToolbar.gripRect);
    } else if (hoveredOverlayBtn >= 0) {
        static constexpr std::array<std::string_view, 8> kOverlayTips = {
            "Toggle Replace (Ctrl+H)",
            "Match Case",
            "Match Whole Word",
            "Previous Match (Shift+Enter)",
            "Next Match (Enter / F3)",
            "Close (Escape)",
            "Replace Next Match",
            "Replace All Matches"
        };
        if (hoveredOverlayBtn < 8) {
            ctx.SetTooltip(kOverlayTips[static_cast<size_t>(hoveredOverlayBtn)], Rect(mousePos.x - 4.0f, mousePos.y - 4.0f, 8.0f, 8.0f));
        }
    }

    // Detect hovered word & diagnostic tooltip in textViewport
    std::string_view hoveredWord{};
    size_t hoveredWordLine = 0;
    size_t hoveredWordCol = 0;
    if (res.hovered && L.textViewport.Contains(mousePos) && hoveredOverlayBtn < 0 && hoveredDebugBtn < 0) {
        const float relY = (mousePos.y - L.textViewport.y) + state.scrollY;
        const float relX = (mousePos.x - L.textViewport.x) + state.scrollX;
        const int rawVisIdx = static_cast<int>(std::floor(relY / std::max(1.0f, L.lineHeight)));
        if (rawVisIdx >= 0 && rawVisIdx < static_cast<int>(visualLines.size())) {
            const VisualLine& vl = visualLines[static_cast<size_t>(rawVisIdx)];
            const std::string_view subStr = displayText.substr(vl.startOffset, vl.length);
            const size_t colInSub = HitTestColumnInLine(canvas, subStr, relX, L.fontSize, textStyle);
            const size_t hitOffset = vl.startOffset + colInSub;
            size_t wordStartOff = 0;
            hoveredWord = ExtractWordAtOffset(displayText, hitOffset, &wordStartOff);
            if (!hoveredWord.empty()) {
                hoveredWordLine = vl.logicalLine;
                hoveredWordCol = wordStartOff - logicalLines[vl.logicalLine].startOffset;
            }

            // Check if hovering over a diagnostic on this line
            for (const CodeDiagnostic& diag : optsCopy.diagnostics) {
                if (diag.line == vl.logicalLine + 1 && !diag.message.empty()) {
                    const size_t col0 = vl.startCol + colInSub;
                    const size_t dStart = diag.startCol;
                    const size_t dEnd = (diag.endCol > diag.startCol) ? diag.endCol : logicalLines[vl.logicalLine].length;
                    if (col0 >= dStart && col0 <= dEnd) {
                        const std::string tip = diag.code.empty()
                            ? std::string(diag.message)
                            : std::format("[{}] {}", diag.code, diag.message);
                        ctx.SetTooltip(ctx.String(tip), Rect(mousePos.x - 4.0f, mousePos.y - 4.0f, 8.0f, 8.0f));
                        break;
                    }
                }
            }
        }
    }

    // Populate TextAreaResponse telemetry
    const size_t safeCur = std::min(cursor, displayText.size());
    const size_t curLogIdx = FindLogicalLineForOffset(logicalLines, safeCur);
    res.cursorLine = curLogIdx + 1;
    res.cursorColumn = (safeCur - logicalLines[curLogIdx].startOffset) + 1;
    res.totalLines = logicalLines.size();
    res.searchMatchCount = state.searchMatchCount;
    res.activeSearchMatch = (state.searchMatchCount > 0) ? (state.activeMatchIdx + 1) : 0;
    res.hoveredWord = hoveredWord;

    const bool cursorVisible = isFocused && ctx.IsCursorVisible(opts.cursorMode);

    Node* node = ctx.CreateNode(NodeType::TextArea, id);
    auto* data = ctx.AllocPayload<TextAreaData>();
    if (data) {
        data->opts = optsCopy;
        data->text = ctx.AllocString(displayText);
        data->cursor = cursor;
        data->selectionStart = state.selectionStart;
        data->selectionEnd = state.selectionEnd;
        data->cursorVisible = cursorVisible;
        data->scrollX = state.scrollX;
        data->scrollY = state.scrollY;
        data->zoom = state.zoom;
        data->copyHovered = copyHovered;
        data->copyFeedbackActive = copyFeedbackActive;
        data->zoomBadgeHovered = zoomHovered;
        data->vThumbHovered = vThumbHovered;
        data->vThumbDragging = state.draggingVThumb;
        data->hThumbHovered = hThumbHovered;
        data->hThumbDragging = state.draggingHThumb;
        data->minimapHovered = minimapHovered;
        data->minimapDragging = state.draggingMinimap;
        data->debugToolbarDragging = state.draggingDebugToolbar;
        data->debugToolbarRatio = state.debugToolbarRatio;
        data->searchOpen = state.searchOpen;
        data->replaceOpen = state.replaceOpen;
        data->gotoOpen = state.gotoOpen;
        data->overlayFocus = state.overlayFocus;
        data->searchQuery = ctx.AllocString(state.searchQuery);
        data->replaceQuery = ctx.AllocString(state.replaceQuery);
        data->gotoQuery = ctx.AllocString(state.gotoQuery);
        data->matchCase = state.matchCase;
        data->wholeWord = state.wholeWord;
        data->activeMatchIdx = state.activeMatchIdx;
        data->searchMatchCount = state.searchMatchCount;
        data->hoveredOverlayBtn = hoveredOverlayBtn;
        data->foldedLines = state.foldedLines;
        data->breakpoints = activeBreakpoints;
        data->hoveredGutterLine = hoveredGutterLine;
        data->hoveredFoldLine = hoveredFoldLine;
        data->hoveredDebugBtn = hoveredDebugBtn;
        data->hoveredWord = ctx.AllocString(hoveredWord);
        data->hoveredWordLine = hoveredWordLine;
        data->hoveredWordCol = hoveredWordCol;
    }
    node->SetPayload(data);

    uint64_t markersHash = detail::HashArgs(optsCopy.diagnostics.size(), optsCopy.inlineValues.size(), activeBreakpoints.size());
    for (const CodeInlineValue& iv : optsCopy.inlineValues) {
        markersHash = detail::HashArgs(markersHash, iv.line, iv.text, static_cast<uint8_t>(iv.variant));
    }
    for (const CodeDiagnostic& d : optsCopy.diagnostics) {
        markersHash = detail::HashArgs(markersHash, d.line, d.startCol, d.endCol, static_cast<uint8_t>(d.severity), d.message);
    }
    for (size_t bp : activeBreakpoints) {
        markersHash = detail::HashArgs(markersHash, bp);
    }

    node->visualHash = detail::HashArgs(
        data ? data->text : displayText,
        optsCopy.label.text,
        optsCopy.description.text,
        optsCopy.badge.text,
        optsCopy.placeholder,
        static_cast<uint8_t>(optsCopy.variant),
        static_cast<uint8_t>(optsCopy.layout),
        static_cast<uint8_t>(optsCopy.inlineOrder),
        static_cast<uint8_t>(optsCopy.elementAlign),
        static_cast<uint8_t>(optsCopy.stretch),
        optsCopy.width.value_or(-1.0f),
        optsCopy.divider,
        static_cast<uint8_t>(optsCopy.dividerStyle),
        optsCopy.readOnly,
        optsCopy.showLineNumbers,
        optsCopy.showCopyButton,
        optsCopy.monospace,
        optsCopy.wordWrap,
        optsCopy.autoHeight,
        optsCopy.rows.value_or(0u),
        optsCopy.fixedHeight.value_or(-1.0f),
        optsCopy.borderRadius.value_or(-1.0f),
        optsCopy.syntax.has_value() ? optsCopy.syntax->enabled : false,
        optsCopy.syntax.has_value() ? optsCopy.syntax->languageName : std::string_view{},
        optsCopy.showMinimap,
        optsCopy.showStatusBar,
        optsCopy.showIndentGuides,
        optsCopy.codeFolding,
        optsCopy.executionLine,
        optsCopy.showDebugToolbar,
        static_cast<uint8_t>(optsCopy.debugToolbarAlign),
        state.debugToolbarRatio,
        state.draggingDebugToolbar,
        optsCopy.toolbarSize.value_or(activeSize),
        markersHash,
        state.foldedLines.size(),
        state.searchOpen,
        state.replaceOpen,
        state.gotoOpen,
        state.overlayFocus,
        data ? data->searchQuery : std::string_view{},
        data ? data->replaceQuery : std::string_view{},
        data ? data->gotoQuery : std::string_view{},
        state.matchCase,
        state.wholeWord,
        state.activeMatchIdx,
        hoveredOverlayBtn,
        hoveredDebugBtn,
        hoveredGutterLine,
        hoveredFoldLine,
        data ? data->hoveredWord : std::string_view{},
        cursor,
        state.selectionStart,
        state.selectionEnd,
        cursorVisible,
        state.scrollX,
        state.scrollY,
        state.zoom,
        copyHovered,
        copyFeedbackActive,
        zoomHovered,
        vThumbHovered,
        state.draggingVThumb,
        hThumbHovered,
        state.draggingHThumb,
        minimapHovered,
        state.draggingMinimap,
        res.hovered,
        res.pressed,
        isFocused,
        effectiveDisabled
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* c, const UiTheme& t) -> Vec2 {
        const auto* d = n->As<TextAreaData>();
        if (!d) return {0.0f, 0.0f};
        TextAreaOptions mOpts = d->opts;
        mOpts.zoom = d->zoom;
        return TextArea::Measure(d->text, mOpts, n->uiSize, c, t, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& c, const UiTheme& t) {
        const auto* d = n->As<TextAreaData>();
        if (d) {
            TextArea::Render(
                c,
                n->computedRect,
                d->text,
                d->opts,
                n->uiSize,
                n->hovered,
                n->focused,
                d->cursor,
                n->disabled,
                d->cursorVisible,
                d->scrollY,
                t,
                d
            );
        }
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->focused = isFocused;
    node->theme = scopedTheme;
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = opts.heightMode;
    if (opts.fixedHeight.has_value() && !opts.label.HasContentOrReserve() && !opts.description.HasContentOrReserve() && !opts.divider) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(*opts.fixedHeight);
    }

    ctx.AdaptChildToContainer(node);
    if (scopedTheme != nullptr) {
        ctx.PopTheme();
    }

    return res;
}

Vec2 TextArea::Measure(
    std::string_view text,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) {
    const Vec2 boxDims = MeasureEditorBox(text, opts, size, canvas, theme, opts.zoom);
    const ElementSpec spec = MakeTextAreaElementSpec(opts, size, opts.disabled, theme);
    return Element::Measure(boxDims, spec, canvas, theme, availableWidth);
}

Vec2 TextArea::Measure(
    std::string_view text,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    return Measure(text, opts, size, canvas, UiTheme::Dark(), 0.0f);
}

void TextArea::Render(
    ICanvas& canvas,
    Rect bounds,
    std::string_view text,
    const TextAreaOptions& opts,
    UiSize size,
    bool hovered,
    bool focused,
    size_t cursor,
    bool disabled,
    bool cursorVisible,
    float scrollY,
    const UiTheme& theme,
    const TextAreaData* extData
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const ThemePalette& pal = theme.palette;
    const SyntaxPalette& synPal = opts.syntaxPalette.has_value()
        ? *opts.syntaxPalette
        : ((opts.syntax.has_value() && opts.syntax->palette.has_value()) ? *opts.syntax->palette : theme.syntax);
    const bool hasCustomEditorSurface = (synPal.editorBg.a > 0);
    const Color editorBgCol = hasCustomEditorSurface ? synPal.editorBg : pal.inputBackground;
    const bool isEditorDark = (0.299f * editorBgCol.r + 0.587f * editorBgCol.g + 0.114f * editorBgCol.b) < 128.0f;
    const Color editorGutterBg = (synPal.gutterBg.a > 0)
        ? synPal.gutterBg
        : (isEditorDark ? Color(15, 23, 42, 190) : Color(241, 245, 249, 210));
    const Color editorBorderSubtle = hasCustomEditorSurface
        ? (isEditorDark ? Color(255, 255, 255, 26) : Color(15, 23, 42, 28))
        : pal.borderSubtle;
    const Color editorTextPrimary = hasCustomEditorSurface ? synPal.identifier : pal.textPrimary;
    const Color editorTextSecondary = hasCustomEditorSurface ? synPal.punctuation.WithAlphaF(0.85f) : pal.textSecondary;
    const Color editorTextMuted = hasCustomEditorSurface ? synPal.comment.WithAlphaF(0.82f) : pal.textMuted;
    const Color editorCaret = hasCustomEditorSurface
        ? (isEditorDark ? Color(248, 250, 252) : Color(15, 23, 42))
        : pal.caret;
    const ScrollbarMetrics& sm = theme.GetScrollbar(size);
    const FontStyle textStyle = opts.monospace ? FontStyle::Monospace : FontStyle::Regular;

    const float effZoom = std::clamp(extData ? extData->zoom : opts.zoom, 0.5f, 3.0f);
    const float effScrollX = extData ? extData->scrollX : 0.0f;
    const float effScrollY = extData ? extData->scrollY : scrollY;
    const size_t selStart = extData ? extData->selectionStart : cursor;
    const size_t selEnd = extData ? extData->selectionEnd : cursor;
    const bool hasSelection = (selEnd > selStart);
    const bool searchOpen = extData && extData->searchOpen;
    const bool replaceOpen = extData && extData->replaceOpen;
    const bool gotoOpen = extData && extData->gotoOpen;
    const std::span<const size_t> foldedLines = extData ? extData->foldedLines : std::span<const size_t>{};
    const std::span<const size_t> breakpoints = extData ? extData->breakpoints
        : (opts.breakpoints ? std::span<const size_t>(*opts.breakpoints) : std::span<const size_t>{});

    std::vector<LogicalLine> logicalLines;
    SplitLogicalLines(text, logicalLines);

    std::vector<FoldRegion> foldRegions;
    if (opts.codeFolding) {
        ComputeFoldRegions(text, logicalLines, opts.syntax.has_value() ? &*opts.syntax : nullptr, foldRegions);
    }

    std::vector<VisualLine> visualLines;
    const EditorLayoutRects L = ComputeEditorLayout(
        bounds, text, logicalLines, opts, size, &canvas, theme,
        effZoom, effScrollX, effScrollY,
        extData ? extData->copyFeedbackActive : false,
        searchOpen, replaceOpen, gotoOpen,
        foldRegions, foldedLines, visualLines,
        extData ? extData->debugToolbarRatio : -1.0f
    );

    // 1. Render external Element header, icon, badge, description, and divider
    const ElementSpec spec = MakeTextAreaElementSpec(opts, size, disabled, theme);
    Element::RenderText(canvas, L.elemGeom, spec, theme);

    const Rect editorBox = L.editorBox;
    if (editorBox.w <= 0.0f || editorBox.h <= 0.0f) return;

    // 2. Render editor container frame
    const float r = theme.ResolveRadius(opts.borderRadius, size);
    const Color bgCol = disabled ? editorBgCol.WithAlphaF(0.45f) : editorBgCol;
    Color borderCol = pal.inputBorder;
    float strokeW = theme.Scale(1.0f);
    if (!disabled) {
        if (focused) {
            borderCol = (opts.variant == UiVariant::Default) ? pal.focusRing : theme.GetVariant(opts.variant).bg;
            strokeW = theme.Scale(1.5f);
        } else if (hovered) {
            borderCol = pal.borderHover;
        }
    } else {
        borderCol = borderCol.WithAlphaF(0.45f);
    }

    canvas.DrawRoundRect(editorBox, r, bgCol, borderCol, strokeW);

    const Rect innerBounds = L.innerBounds;
    if (innerBounds.w <= 2.0f || innerBounds.h <= 2.0f) return;

    // Precompute search matches and word occurrence target
    std::vector<SearchMatch> searchMatches;
    if (searchOpen && extData && !extData->searchQuery.empty()) {
        FindAllSearchMatches(text, logicalLines, extData->searchQuery, extData->matchCase, extData->wholeWord, searchMatches);
    }

    const size_t safeCursor = std::min(cursor, text.size());
    std::string_view activeWord{};
    if (opts.highlightOccurrences && focused && !disabled && !searchOpen) {
        if (hasSelection && (selEnd - selStart) <= 48) {
            const std::string_view s = text.substr(selStart, selEnd - selStart);
            bool validIdent = !s.empty() && IsIdentStart(s[0]);
            for (char ch : s) {
                if (!IsIdentChar(ch)) { validIdent = false; break; }
            }
            if (validIdent) activeWord = s;
        } else if (!hasSelection) {
            activeWord = ExtractWordAtOffset(text, safeCursor);
        }
    }

    size_t matchBracketOpen = std::string_view::npos;
    size_t matchBracketClose = std::string_view::npos;
    const bool hasBracketPair = opts.matchBrackets && focused && !disabled &&
        FindMatchingBracketPair(text, safeCursor, matchBracketOpen, matchBracketClose);

    canvas.PushClip(innerBounds);

    // 3. Line numbers & debugger gutter background
    if (L.gutterWidth > 0.0f) {
        canvas.DrawRect(L.gutterRect, editorGutterBg);
        canvas.DrawLine(
            Vec2{L.gutterRect.Right(), L.gutterRect.y},
            Vec2{L.gutterRect.Right(), L.gutterRect.Bottom()},
            editorBorderSubtle,
            1.0f
        );
    }

    const Rect textViewport = L.textViewport;
    const float clampedScrollY = std::clamp(effScrollY, 0.0f, L.maxScrollY);
    const float clampedScrollX = std::clamp(effScrollX, 0.0f, L.maxScrollX);

    const size_t activeVisRow = FindVisualLineForOffset(visualLines, safeCursor);
    const size_t activeLogicalLine = visualLines[activeVisRow].logicalLine;

    // 4. Placeholder when empty
    if (text.empty() && !opts.placeholder.empty() && !focused) {
        canvas.DrawTextBox(
            opts.placeholder,
            Rect(textViewport.x, textViewport.y, textViewport.w, L.lineHeight),
            L.fontSize,
            editorTextMuted,
            0.0f,
            0.5f
        );
    }

    // 5. Virtualized visible lines loop
    const size_t totalVisLines = visualLines.size();
    const size_t firstVisLine = (L.lineHeight > 0.0f)
        ? static_cast<size_t>(std::max(0.0f, std::floor(clampedScrollY / L.lineHeight)))
        : 0;
    const size_t maxVisCount = (L.lineHeight > 0.0f)
        ? (static_cast<size_t>(std::ceil(textViewport.h / L.lineHeight)) + 2)
        : totalVisLines;
    const size_t lastVisLine = std::min(totalVisLines, firstVisLine + maxVisCount);

    std::vector<TextSpan> syntaxSpans;
    syntaxSpans.reserve(32);
    size_t cachedLogicalLine = static_cast<size_t>(-1);

    const bool useCallbackSyntax = (opts.syntaxHighlighter != nullptr);
    const bool useConfigSyntax = (!useCallbackSyntax && opts.syntax.has_value() && opts.syntax->enabled);
    const bool anySyntax = (useCallbackSyntax || useConfigSyntax) && !disabled;

    // Compute multiline block comment state up to the first visible logical line
    bool inBlockComment = false;
    if (useConfigSyntax && !opts.syntax->blockCommentStart.empty() && !opts.syntax->blockCommentEnd.empty() && firstVisLine < totalVisLines) {
        const size_t targetLog = visualLines[firstVisLine].logicalLine;
        const size_t scanStart = (targetLog > 250) ? (targetLog - 250) : 0;
        for (size_t li = scanStart; li < targetLog; ++li) {
            const std::string_view ls = text.substr(logicalLines[li].startOffset, logicalLines[li].length);
            size_t pos = 0;
            while (pos < ls.size()) {
                if (!inBlockComment) {
                    if (!opts.syntax->lineComment.empty() && ls.substr(pos).starts_with(opts.syntax->lineComment) &&
                        !ls.substr(pos).starts_with(opts.syntax->blockCommentStart)) {
                        break;
                    }
                    const size_t bStart = ls.find(opts.syntax->blockCommentStart, pos);
                    if (bStart == std::string_view::npos) break;
                    inBlockComment = true;
                    pos = bStart + opts.syntax->blockCommentStart.size();
                } else {
                    const size_t bEnd = ls.find(opts.syntax->blockCommentEnd, pos);
                    if (bEnd == std::string_view::npos) break;
                    inBlockComment = false;
                    pos = bEnd + opts.syntax->blockCommentEnd.size();
                }
            }
        }
    }

    Rect hoveredWordScreenRect{};

    for (size_t vIdx = firstVisLine; vIdx < lastVisLine; ++vIdx) {
        const VisualLine& vl = visualLines[vIdx];
        const float lineY = textViewport.y + static_cast<float>(vIdx) * L.lineHeight - clampedScrollY;
        if (lineY + L.lineHeight < innerBounds.y || lineY > innerBounds.Bottom()) {
            continue;
        }

        const bool isExecLine = (opts.executionLine == vl.logicalLine + 1);

        // Debugger execution line golden highlight OR active cursor line highlight
        if (isExecLine) {
            canvas.DrawRect(
                Rect(innerBounds.x + L.gutterWidth, lineY, innerBounds.w - L.gutterWidth - L.minimapWidth, L.lineHeight),
                Color(245, 158, 11, 42)
            );
            canvas.DrawRect(
                Rect(innerBounds.x + L.gutterWidth, lineY, theme.Scale(2.5f), L.lineHeight),
                Color(245, 158, 11, 230)
            );
        } else if (focused && !disabled && vIdx == activeVisRow && !hasSelection) {
            canvas.DrawRect(
                Rect(innerBounds.x + L.gutterWidth, lineY, innerBounds.w - L.gutterWidth - L.minimapWidth, L.lineHeight),
                synPal.lineHighlight
            );
        }

        const float textY = lineY + L.metrics.textOffsetY;

        // Gutter contents: Breakpoint circle, Execution arrow, Line number, Folding chevron
        if (L.gutterWidth > 0.0f && vl.isFirstSubLine) {
            const float centerY = lineY + L.lineHeight * 0.5f;

            // Breakpoint indicator (left lane of bpColWidth)
            const bool hasBp = std::find(breakpoints.begin(), breakpoints.end(), vl.logicalLine + 1) != breakpoints.end();
            const bool hovBp = extData && (extData->hoveredGutterLine == static_cast<int>(vl.logicalLine));
            if (hasBp || hovBp) {
                const float bpR = std::min(L.lineHeight * 0.28f, theme.Scale(5.0f) * effZoom);
                const float bpX = innerBounds.x + std::max(bpR + theme.Scale(3.5f), L.bpColWidth * 0.25f);
                const Color bpCol = hasBp ? Color(239, 68, 68, 245) : Color(239, 68, 68, 105);
                canvas.DrawRoundRect(Rect(bpX - bpR, centerY - bpR, bpR * 2.0f, bpR * 2.0f), bpR, bpCol, hasBp ? Color(254, 202, 202, 180) : Color(0, 0, 0, 0), 1.0f);
            }

            // Debugger execution arrow (▶) (right lane of bpColWidth, never overlapping breakpoint)
            if (isExecLine) {
                const float ax = innerBounds.x + std::max(theme.Scale(14.5f) * effZoom, L.bpColWidth * 0.56f);
                const float aw = theme.Scale(7.5f) * effZoom;
                const float ah = theme.Scale(5.0f) * effZoom;
                const Color arrowFill(251, 191, 36, 255);
                const Color arrowStroke(180, 83, 9, 255);
                canvas.DrawTriangle(
                    {ax, centerY - ah},
                    {ax, centerY + ah},
                    {ax + aw, centerY},
                    arrowFill,
                    arrowStroke,
                    theme.Scale(1.0f)
                );
            }

            // Line number
            if (opts.showLineNumbers) {
                char numBuf[16]{};
                const int nLen = std::snprintf(numBuf, sizeof(numBuf), "%zu", vl.logicalLine + 1);
                if (nLen > 0) {
                    const std::string_view numStr(numBuf, static_cast<size_t>(nLen));
                    const bool isCurrentLine = focused && !disabled && (vl.logicalLine == activeLogicalLine);
                    const Color numCol = disabled
                        ? pal.textDisabled
                        : (isExecLine ? Color(251, 191, 36) : (isCurrentLine ? editorTextPrimary : editorTextMuted));
                    const float numW = MeasureChunkNoTabs(&canvas, numStr, L.fontSize, FontStyle::Monospace);
                    const float gutterPadRight = L.metrics.spaceAdvance * 1.15f + L.foldColWidth;
                    const float numX = innerBounds.x + std::max(2.0f, L.gutterWidth - gutterPadRight - numW);
                    canvas.DrawTextStyled(numStr, Vec2{numX, textY}, L.fontSize, numCol, FontStyle::Monospace);
                }
            }

            // Code folding chevron (▾ / ▸)
            if (opts.codeFolding) {
                if (FindFoldRegionStartingAt(foldRegions, vl.logicalLine) != nullptr) {
                    const bool isFolded = IsLineFoldedStart(foldedLines, vl.logicalLine);
                    const bool hovFold = extData && (extData->hoveredFoldLine == static_cast<int>(vl.logicalLine));
                    const Color chevCol = hovFold ? editorTextPrimary : editorTextSecondary;
                    const float cx = L.gutterRect.Right() - L.foldColWidth * 0.5f;
                    const float icoSz = std::min(L.foldColWidth * 0.85f, theme.Scale(11.0f));
                    const Rect chevRect(cx - icoSz * 0.5f, centerY - icoSz * 0.5f, icoSz, icoSz);
                    if (hovFold) {
                        canvas.DrawRoundRect(chevRect, theme.Scale(2.5f), editorBorderSubtle);
                    }
                    const Icon chevIco = isFolded ? Icon::ChevronRight() : Icon::ChevronDown();
                    chevIco.Draw(canvas, chevRect, chevCol);
                }
            }
        }

        // Clip and render code/text within textViewport
        canvas.PushClip(Rect(textViewport.x, innerBounds.y, textViewport.w, innerBounds.h));

        const float startX = textViewport.x - clampedScrollX;
        const size_t vlEndOffset = vl.startOffset + vl.length;
        const LogicalLine& ll = logicalLines[vl.logicalLine];
        const std::string_view fullLineStr = text.substr(ll.startOffset, ll.length);
        const std::string_view subLineStr = text.substr(vl.startOffset, vl.length);

        // Indent guides (every 4 spaces)
        if (opts.showIndentGuides && vl.isFirstSubLine && !fullLineStr.empty()) {
            size_t leadSpaces = 0;
            for (char ch : fullLineStr) {
                if (ch == ' ') ++leadSpaces;
                else if (ch == '\t') leadSpaces += 4;
                else break;
            }
            const Color guideCol = hasCustomEditorSurface ? editorBorderSubtle : pal.borderSubtle.WithAlphaF(0.42f);
            for (size_t g = 4; g < leadSpaces; g += 4) {
                const float gx = startX + static_cast<float>(g) * L.metrics.spaceAdvance;
                if (gx >= textViewport.x && gx <= textViewport.Right()) {
                    canvas.DrawLine({gx, lineY}, {gx, lineY + L.lineHeight}, guideCol, 1.0f);
                }
            }
        }

        // Word occurrence highlights on this line
        if (!activeWord.empty() && subLineStr.size() >= activeWord.size()) {
            const size_t wLen = activeWord.size();
            for (size_t col = 0; col + wLen <= subLineStr.size(); ) {
                if (subLineStr.substr(col, wLen) == activeWord) {
                    const bool leftOk = (col == 0) || !IsIdentChar(subLineStr[col - 1]);
                    const bool rightOk = (col + wLen == subLineStr.size()) || !IsIdentChar(subLineStr[col + wLen]);
                    if (leftOk && rightOk) {
                        const float ox = startX + MeasureSegmentWidth(&canvas, subLineStr.substr(0, col), L.fontSize, textStyle);
                        const float ow = MeasureSegmentWidth(&canvas, activeWord, L.fontSize, textStyle);
                        canvas.DrawRoundRect(
                            Rect(ox - 1.0f, lineY + 1.0f, ow + 2.0f, L.lineHeight - 2.0f),
                            theme.Scale(2.5f),
                            pal.focusRing.WithAlphaF(0.14f),
                            pal.focusRing.WithAlphaF(0.38f),
                            1.0f
                        );
                        col += wLen;
                        continue;
                    }
                }
                ++col;
            }
        }

        // Search match highlights on this line
        if (!searchMatches.empty()) {
            for (size_t mi = 0; mi < searchMatches.size(); ++mi) {
                const SearchMatch& m = searchMatches[mi];
                if (m.logicalLine != vl.logicalLine) continue;
                if (m.endOffset <= vl.startOffset || m.startOffset >= vlEndOffset) continue;
                const size_t s0 = std::max(m.startOffset, vl.startOffset) - vl.startOffset;
                const size_t s1 = std::min(m.endOffset, vlEndOffset) - vl.startOffset;
                const float mx = startX + MeasureSegmentWidth(&canvas, subLineStr.substr(0, s0), L.fontSize, textStyle);
                const float mw = MeasureSegmentWidth(&canvas, subLineStr.substr(s0, s1 - s0), L.fontSize, textStyle);
                const bool isActiveMatch = (extData && mi == extData->activeMatchIdx);
                const Color mFill = isActiveMatch ? Color(245, 158, 11, 120) : Color(245, 158, 11, 58);
                const Color mStroke = isActiveMatch ? Color(251, 191, 36, 240) : Color(245, 158, 11, 140);
                canvas.DrawRoundRect(Rect(mx, lineY + 1.0f, mw, L.lineHeight - 2.0f), theme.Scale(2.5f), mFill, mStroke, 1.0f);
            }
        }

        // Selection highlight rectangle
        if (hasSelection && focused && !disabled) {
            const size_t s0 = std::max(selStart, vl.startOffset);
            const size_t s1 = std::min(selEnd, vlEndOffset);
            if (s1 > s0 || (selStart <= vl.startOffset && selEnd > vlEndOffset)) {
                const std::string_view preSel = text.substr(vl.startOffset, s0 - vl.startOffset);
                const std::string_view midSel = text.substr(s0, s1 - s0);
                const float selX0 = startX + MeasureSegmentWidth(&canvas, preSel, L.fontSize, textStyle);
                float selW = MeasureSegmentWidth(&canvas, midSel, L.fontSize, textStyle);
                if (selEnd > vlEndOffset) {
                    selW += L.metrics.spaceAdvance;
                }
                if (selW > 0.0f) {
                    canvas.DrawRect(Rect(selX0, lineY, selW, L.lineHeight), synPal.selectionBg);
                }
            }
        }

        // Bracket matching boxes
        if (hasBracketPair) {
            for (size_t bOff : {matchBracketOpen, matchBracketClose}) {
                if (bOff >= vl.startOffset && bOff < vlEndOffset) {
                    const size_t bCol = bOff - vl.startOffset;
                    const float bx = startX + MeasureSegmentWidth(&canvas, subLineStr.substr(0, bCol), L.fontSize, textStyle);
                    const float bw = L.metrics.spaceAdvance;
                    canvas.DrawRoundRect(
                        Rect(bx - 1.0f, lineY + 1.5f, bw + 2.0f, L.lineHeight - 3.0f),
                        theme.Scale(2.0f),
                        pal.focusRing.WithAlphaF(0.18f),
                        pal.focusRing.WithAlphaF(0.75f),
                        1.0f
                    );
                }
            }
        }

        // Track screen position of hoveredWord for DataTip overlay
        if (extData && !extData->hoveredWord.empty() && vl.logicalLine == extData->hoveredWordLine && vl.isFirstSubLine) {
            const size_t wCol = std::min(extData->hoveredWordCol, subLineStr.size());
            const float wx = startX + MeasureSegmentWidth(&canvas, subLineStr.substr(0, wCol), L.fontSize, textStyle);
            const float ww = MeasureSegmentWidth(&canvas, extData->hoveredWord, L.fontSize, textStyle);
            hoveredWordScreenRect = Rect(wx, lineY, ww, L.lineHeight);
        }

        // Render text or syntax spans
        const Color wsCol = editorTextMuted.WithAlphaF(0.55f);
        const float wsCenterY = lineY + L.lineHeight * 0.52f;
        const float wsDotR = std::max(1.1f, theme.Scale(1.35f) * effZoom);
        auto drawSpaceDots = [&](float baseX, size_t count) {
            if (!opts.showWhitespace || count == 0) return;
            for (size_t i = 0; i < count; ++i) {
                const float dx = baseX + (static_cast<float>(i) + 0.5f) * L.metrics.spaceAdvance;
                if (dx >= textViewport.x - 2.0f && dx <= textViewport.Right() + 2.0f) {
                    canvas.DrawRoundRect(Rect(dx - wsDotR, wsCenterY - wsDotR, wsDotR * 2.0f, wsDotR * 2.0f), wsDotR, wsCol);
                }
            }
        };
        auto drawTabArrows = [&](float baseX, size_t count) {
            if (!opts.showWhitespace || count == 0) return;
            const float strokeW = std::max(1.0f, theme.Scale(1.15f) * effZoom);
            const float headSz = std::min(L.lineHeight * 0.20f, L.metrics.spaceAdvance * 0.45f);
            for (size_t i = 0; i < count; ++i) {
                const float tx0 = baseX + static_cast<float>(i) * L.metrics.tabAdvance + L.metrics.spaceAdvance * 0.35f;
                const float tx1 = baseX + static_cast<float>(i + 1) * L.metrics.tabAdvance - L.metrics.spaceAdvance * 0.35f;
                if (tx1 >= textViewport.x - 2.0f && tx0 <= textViewport.Right() + 2.0f && tx1 > tx0) {
                    canvas.DrawLine({tx0, wsCenterY}, {tx1, wsCenterY}, wsCol, strokeW);
                    canvas.DrawLine({tx1 - headSz, wsCenterY - headSz}, {tx1, wsCenterY}, wsCol, strokeW);
                    canvas.DrawLine({tx1 - headSz, wsCenterY + headSz}, {tx1, wsCenterY}, wsCol, strokeW);
                }
            }
        };

        float curX = startX;
        if (vl.length > 0) {
            auto drawPiece = [&](std::string_view piece, Color col) {
                size_t p = 0;
                while (p < piece.size()) {
                    if (piece[p] == '\t') {
                        size_t tabEnd = p + 1;
                        while (tabEnd < piece.size() && piece[tabEnd] == '\t') {
                            ++tabEnd;
                        }
                        const size_t tabCount = tabEnd - p;
                        drawTabArrows(curX, tabCount);
                        curX += static_cast<float>(tabCount) * L.metrics.tabAdvance;
                        p = tabEnd;
                    } else {
                        size_t nextTab = piece.find('\t', p);
                        if (nextTab == std::string_view::npos) {
                            nextTab = piece.size();
                        }
                        const std::string_view seg = piece.substr(p, nextTab - p);

                        size_t firstNonSpace = 0;
                        while (firstNonSpace < seg.size() && seg[firstNonSpace] == ' ') {
                            ++firstNonSpace;
                        }
                        if (firstNonSpace > 0) {
                            drawSpaceDots(curX, firstNonSpace);
                            curX += static_cast<float>(firstNonSpace) * L.metrics.spaceAdvance;
                        }

                        if (firstNonSpace < seg.size()) {
                            size_t lastNonSpace = seg.size();
                            while (lastNonSpace > firstNonSpace && seg[lastNonSpace - 1] == ' ') {
                                --lastNonSpace;
                            }
                            const std::string_view body = seg.substr(firstNonSpace, lastNonSpace - firstNonSpace);
                            const float bodyW = MeasureChunkNoTabs(&canvas, body, L.fontSize, textStyle);
                            if (curX + bodyW >= textViewport.x - 4.0f && curX <= textViewport.Right() + 4.0f) {
                                canvas.DrawTextStyled(body, Vec2{curX, textY}, L.fontSize, col, textStyle);
                                if (opts.showWhitespace) {
                                    for (size_t bi = 0; bi < body.size(); ++bi) {
                                        if (body[bi] == ' ') {
                                            const float spX = curX + MeasureChunkNoTabs(&canvas, body.substr(0, bi), L.fontSize, textStyle) + L.metrics.spaceAdvance * 0.5f;
                                            canvas.DrawRoundRect(Rect(spX - wsDotR, wsCenterY - wsDotR, wsDotR * 2.0f, wsDotR * 2.0f), wsDotR, wsCol);
                                        }
                                    }
                                }
                            }
                            curX += bodyW;

                            const size_t trailingSpaces = seg.size() - lastNonSpace;
                            if (trailingSpaces > 0) {
                                drawSpaceDots(curX, trailingSpaces);
                                curX += static_cast<float>(trailingSpaces) * L.metrics.spaceAdvance;
                            }
                        }
                        p = nextTab;
                    }
                }
            };

            if (anySyntax) {
                if (cachedLogicalLine != vl.logicalLine) {
                    syntaxSpans.clear();
                    if (useCallbackSyntax) {
                        opts.syntaxHighlighter(fullLineStr, vl.logicalLine, syntaxSpans, theme, opts.highlighterUserData);
                    } else if (useConfigSyntax) {
                        HighlightLine(
                            fullLineStr,
                            vl.logicalLine,
                            *opts.syntax,
                            syntaxSpans,
                            theme,
                            opts.syntaxPalette.has_value() ? &*opts.syntaxPalette : nullptr,
                            &inBlockComment
                        );
                    }
                    cachedLogicalLine = vl.logicalLine;
                }

                const size_t subStartCol = vl.startCol;
                const size_t subEndCol = vl.startCol + vl.length;
                size_t curCol = subStartCol;

                for (const TextSpan& sp : syntaxSpans) {
                    const size_t spStart = sp.startCol;
                    const size_t spEnd = sp.startCol + sp.length;
                    if (spEnd <= subStartCol) continue;
                    if (spStart >= subEndCol) break;

                    const size_t clipStart = std::max(spStart, subStartCol);
                    const size_t clipEnd = std::min(spEnd, subEndCol);

                    if (clipStart > curCol) {
                        drawPiece(fullLineStr.substr(curCol, clipStart - curCol), synPal.identifier);
                    }
                    if (clipEnd > clipStart) {
                        drawPiece(fullLineStr.substr(clipStart, clipEnd - clipStart), sp.color);
                        curCol = clipEnd;
                    }
                }
                if (curCol < subEndCol) {
                    drawPiece(fullLineStr.substr(curCol, subEndCol - curCol), synPal.identifier);
                }
            } else {
                const Color textCol = disabled ? pal.textDisabled : editorTextPrimary;
                drawPiece(subLineStr, textCol);
            }
        }

        // Visible EOL return marker (↵) when showWhitespace is enabled and this is the end of the logical line
        if (opts.showWhitespace && (vl.startCol + vl.length == ll.length) && (vl.logicalLine + 1 < logicalLines.size())) {
            const float eolX = curX + L.metrics.spaceAdvance * 0.25f;
            const float eolW = L.metrics.spaceAdvance * 0.62f;
            const float eolTop = wsCenterY - L.lineHeight * 0.18f;
            const float eolBot = wsCenterY + L.lineHeight * 0.12f;
            const float headSz = std::min(L.lineHeight * 0.14f, L.metrics.spaceAdvance * 0.30f);
            const float strokeW = std::max(1.0f, theme.Scale(1.1f) * effZoom);
            if (eolX + eolW >= textViewport.x - 2.0f && eolX <= textViewport.Right() + 2.0f) {
                canvas.DrawLine({eolX + eolW, eolTop}, {eolX + eolW, eolBot}, wsCol, strokeW);
                canvas.DrawLine({eolX + eolW, eolBot}, {eolX, eolBot}, wsCol, strokeW);
                canvas.DrawLine({eolX + headSz, eolBot - headSz}, {eolX, eolBot}, wsCol, strokeW);
                canvas.DrawLine({eolX + headSz, eolBot + headSz}, {eolX, eolBot}, wsCol, strokeW);
            }
        }

        // Folded block summary pill "[...]" at end of line
        const bool isFoldedStart = opts.codeFolding && vl.isFirstSubLine && IsLineFoldedStart(foldedLines, vl.logicalLine);
        if (isFoldedStart) {
            const float pillX = curX + L.metrics.spaceAdvance * 0.8f;
            const float pillW = L.metrics.spaceAdvance * 4.2f;
            const float pillH = L.lineHeight - theme.Scale(4.0f);
            const Rect pillRect(pillX, lineY + theme.Scale(2.0f), pillW, pillH);
            canvas.DrawRoundRect(pillRect, theme.Scale(3.0f), editorGutterBg, editorBorderSubtle, 1.0f);
            canvas.DrawTextBox("...", pillRect, L.fontSize * 0.9f, editorTextSecondary, 0.5f, 0.45f);
            curX = pillRect.Right();
        }

        // Inline Debugger Variable Readouts (e.g. "N = 1024 | peak = +18.4 dB")
        // Attached to the end of the logical line (last visual sub-line, or folded block header)
        const bool isLastSubLine = (vl.startCol + vl.length >= ll.length) || isFoldedStart;
        if (isLastSubLine && !opts.inlineValues.empty()) {
            const std::string_view cmtPrefix = (opts.syntax.has_value() && !opts.syntax->lineComment.empty())
                ? opts.syntax->lineComment
                : "//";
            if (!ExtractCodeWithoutLineComment(fullLineStr, cmtPrefix).empty()) {
                for (const CodeInlineValue& iv : opts.inlineValues) {
                    if (iv.line == vl.logicalLine + 1 && !iv.text.empty()) {
                        const float ivFs = L.fontSize * 0.88f;
                        const float ivW = MeasureChunkNoTabs(&canvas, iv.text, ivFs, FontStyle::Monospace) + theme.Scale(12.0f);
                        float ivX = curX + L.metrics.spaceAdvance * 2.2f;
                        bool clampedRight = false;
                        if (opts.wordWrap && ivX + ivW > textViewport.Right() - theme.Scale(4.0f)) {
                            ivX = std::max(textViewport.x + theme.Scale(4.0f), textViewport.Right() - theme.Scale(4.0f) - ivW);
                            clampedRight = true;
                        }
                        const float ivH = L.lineHeight - theme.Scale(3.0f);
                        const Rect ivRect(ivX, lineY + theme.Scale(1.5f), ivW, ivH);
                        const Color accent = theme.GetTextColor(iv.variant);
                        if (clampedRight) {
                            canvas.DrawRoundRect(ivRect, theme.Scale(3.5f), editorBgCol.WithAlphaF(0.92f), Color::Transparent(), 0.0f);
                        }
                        canvas.DrawRoundRect(ivRect, theme.Scale(3.5f), accent.WithAlphaF(0.14f), accent.WithAlphaF(0.34f), 1.0f);
                        canvas.DrawTextStyled(iv.text, Vec2{ivX + theme.Scale(6.0f), textY + theme.Scale(0.8f)}, ivFs, accent.WithAlphaF(0.94f), FontStyle::Monospace);
                        curX = ivRect.Right();
                    }
                }
            }
        }

        // Diagnostic wavy underline squiggles (~~~)
        if (!opts.diagnostics.empty()) {
            for (const CodeDiagnostic& diag : opts.diagnostics) {
                if (diag.line != vl.logicalLine + 1) continue;
                size_t dStart = diag.startCol;
                size_t dEnd = (diag.endCol > diag.startCol) ? std::min<size_t>(diag.endCol, fullLineStr.size()) : fullLineStr.size();
                if (dStart == 0 && diag.endCol == 0) {
                    while (dStart < fullLineStr.size() && (fullLineStr[dStart] == ' ' || fullLineStr[dStart] == '\t')) ++dStart;
                }
                if (dEnd <= vl.startCol || dStart >= vl.startCol + vl.length) continue;
                const size_t s0 = std::max(dStart, vl.startCol) - vl.startCol;
                const size_t s1 = std::min(dEnd, vl.startCol + vl.length) - vl.startCol;
                const float x0 = startX + MeasureSegmentWidth(&canvas, subLineStr.substr(0, s0), L.fontSize, textStyle);
                const float x1 = startX + MeasureSegmentWidth(&canvas, subLineStr.substr(0, s1), L.fontSize, textStyle);
                const Color sqCol = (diag.severity == DiagnosticSeverity::Error) ? Color(239, 68, 68, 235)
                                  : (diag.severity == DiagnosticSeverity::Warning) ? Color(245, 158, 11, 235)
                                  : Color(56, 189, 248, 220);
                DrawWavyUnderline(canvas, x0, std::max(x0 + L.metrics.spaceAdvance, x1), lineY + L.lineHeight - theme.Scale(2.0f), sqCol, effZoom);
            }
        }

        // Render active cursor caret (when focus is in the code editor)
        if (focused && cursorVisible && !disabled && vIdx == activeVisRow && (!extData || extData->overlayFocus == 0)) {
            const size_t colInSub = (safeCursor >= vl.startOffset)
                ? std::min(safeCursor - vl.startOffset, vl.length)
                : 0;
            const std::string_view beforeCursor = text.substr(vl.startOffset, colInSub);
            const float caretX = startX + MeasureSegmentWidth(&canvas, beforeCursor, L.fontSize, textStyle);
            if (caretX >= textViewport.x - 2.0f && caretX <= textViewport.Right() + 2.0f) {
                const float caretH = std::max(L.glyphBoxH, L.lineHeight - theme.Scale(2.0f));
                const float caretY = lineY + (L.lineHeight - caretH) * 0.5f;
                canvas.DrawRect(
                    Rect(caretX, caretY, std::max(1.5f, theme.Scale(1.8f)), caretH),
                    editorCaret
                );
            }
        }

        canvas.PopClip();
    }

    // 6. Render Right-Hand Code Minimap
    if (L.hasMinimap && L.minimapRect.w > 0.0f) {
        const Color miniBg = (synPal.gutterBg.a > 0)
            ? synPal.gutterBg.WithAlphaF(0.92f)
            : (isEditorDark ? Color(11, 17, 30, 180) : Color(241, 245, 249, 195));
        canvas.DrawRect(L.minimapRect, miniBg);
        canvas.DrawLine(
            {L.minimapRect.x, L.minimapRect.y},
            {L.minimapRect.x, L.minimapRect.Bottom()},
            editorBorderSubtle,
            1.0f
        );

        const float microStep = theme.Scale(3.0f);
        const float microH = theme.Scale(1.6f);
        const size_t maxMiniLines = static_cast<size_t>(std::max(1.0f, L.minimapRect.h / microStep));
        const size_t startVis = (visualLines.size() > maxMiniLines && L.maxScrollY > 0.0f)
            ? static_cast<size_t>((clampedScrollY / L.maxScrollY) * static_cast<float>(visualLines.size() - maxMiniLines))
            : 0;
        const size_t endVis = std::min(visualLines.size(), startVis + maxMiniLines);

        // Draw viewport slider box on minimap
        const float visCountF = static_cast<float>(std::max<size_t>(1, lastVisLine - firstVisLine));
        const float vpTopY = L.minimapRect.y + static_cast<float>(firstVisLine > startVis ? firstVisLine - startVis : 0) * microStep;
        const float vpH = std::min(L.minimapRect.Bottom() - vpTopY, visCountF * microStep);
        const bool miniActive = extData && (extData->minimapHovered || extData->minimapDragging);
        canvas.DrawRect(
            Rect(L.minimapRect.x + 1.0f, vpTopY, L.minimapRect.w - 1.0f, std::max(theme.Scale(10.0f), vpH)),
            pal.focusRing.WithAlphaF(miniActive ? 0.22f : 0.12f)
        );

        const float charScale = (L.minimapRect.w - theme.Scale(8.0f)) / 90.0f;
        for (size_t mv = startVis; mv < endVis; ++mv) {
            const VisualLine& vl = visualLines[mv];
            const float my = L.minimapRect.y + static_cast<float>(mv - startVis) * microStep + theme.Scale(2.0f);
            const std::string_view ls = text.substr(vl.startOffset, vl.length);

            // Execution line marker on minimap
            if (opts.executionLine == vl.logicalLine + 1) {
                canvas.DrawRect(Rect(L.minimapRect.x + 1.0f, my - 0.5f, L.minimapRect.w - 2.0f, microStep), Color(251, 191, 36, 160));
            }
            // Breakpoint dot on minimap
            if (std::find(breakpoints.begin(), breakpoints.end(), vl.logicalLine + 1) != breakpoints.end()) {
                canvas.DrawRect(Rect(L.minimapRect.x + 2.0f, my, theme.Scale(3.5f), microStep), Color(239, 68, 68, 230));
            }

            // Draw micro-tokens
            size_t cIdx = 0;
            while (cIdx < ls.size() && cIdx < 88) {
                while (cIdx < ls.size() && (ls[cIdx] == ' ' || ls[cIdx] == '\t')) ++cIdx;
                if (cIdx >= ls.size() || cIdx >= 88) break;
                const size_t tokStart = cIdx;
                while (cIdx < ls.size() && ls[cIdx] != ' ' && ls[cIdx] != '\t' && cIdx < 88) ++cIdx;
                const std::string_view tok = ls.substr(tokStart, cIdx - tokStart);
                Color mCol = synPal.identifier.WithAlphaF(0.55f);
                if (useConfigSyntax) {
                    if (!opts.syntax->lineComment.empty() && tok.starts_with(opts.syntax->lineComment)) {
                        mCol = synPal.comment.WithAlphaF(0.65f);
                    } else if (ContainsToken(opts.syntax->keywords, tok)) {
                        mCol = synPal.keyword.WithAlphaF(0.75f);
                    } else if (ContainsToken(opts.syntax->types, tok)) {
                        mCol = synPal.type.WithAlphaF(0.75f);
                    } else if (tok.front() == '"' || tok.front() == '\'') {
                        mCol = synPal.string.WithAlphaF(0.70f);
                    }
                }
                const float mx = L.minimapRect.x + theme.Scale(4.0f) + static_cast<float>(tokStart) * charScale;
                const float mw = std::max(1.5f, static_cast<float>(cIdx - tokStart) * charScale);
                canvas.DrawRect(Rect(mx, my, mw, microH), mCol);
            }
        }
    }

    // 7. Render vertical & horizontal scrollbars (with overview ruler markers for search/breakpoints/diagnostics)
    if (L.needVScroll && L.vTrackRect.h > 0.0f) {
        canvas.DrawRoundRect(L.vTrackRect, sm.cornerRadius, pal.surfaceHover.WithAlphaF(0.25f));
        const bool vActive = extData && (extData->vThumbHovered || extData->vThumbDragging);
        const Color thumbCol = vActive ? pal.controlKnobHover : pal.controlKnob.WithAlphaF(0.65f);
        canvas.DrawRoundRect(L.vThumbRect, sm.cornerRadius, thumbCol);

        // Overview ruler ticks on vertical scrollbar track
        const float totalLinesF = static_cast<float>(std::max<size_t>(1, logicalLines.size()));
        for (const SearchMatch& m : searchMatches) {
            const float ty = L.vTrackRect.y + (static_cast<float>(m.logicalLine) / totalLinesF) * L.vTrackRect.h;
            canvas.DrawRect(Rect(L.vTrackRect.x, ty, L.vTrackRect.w, theme.Scale(2.0f)), Color(245, 158, 11, 210));
        }
        for (size_t bp : breakpoints) {
            if (bp >= 1 && bp <= logicalLines.size()) {
                const float ty = L.vTrackRect.y + (static_cast<float>(bp - 1) / totalLinesF) * L.vTrackRect.h;
                canvas.DrawRect(Rect(L.vTrackRect.x, ty, L.vTrackRect.w, theme.Scale(2.5f)), Color(239, 68, 68, 230));
            }
        }
        for (const CodeDiagnostic& d : opts.diagnostics) {
            if (d.line >= 1 && d.line <= logicalLines.size()) {
                const float ty = L.vTrackRect.y + (static_cast<float>(d.line - 1) / totalLinesF) * L.vTrackRect.h;
                const Color dc = (d.severity == DiagnosticSeverity::Error) ? Color(239, 68, 68, 230) : Color(245, 158, 11, 220);
                canvas.DrawRect(Rect(L.vTrackRect.x, ty, L.vTrackRect.w, theme.Scale(2.5f)), dc);
            }
        }
    }

    if (L.needHScroll && L.hTrackRect.w > 0.0f) {
        canvas.DrawRoundRect(L.hTrackRect, sm.cornerRadius, pal.surfaceHover.WithAlphaF(0.25f));
        const bool hActive = extData && (extData->hThumbHovered || extData->hThumbDragging);
        const Color thumbCol = hActive ? pal.controlKnobHover : pal.controlKnob.WithAlphaF(0.65f);
        canvas.DrawRoundRect(L.hThumbRect, sm.cornerRadius, thumbCol);
    }

    // 8. Render Debug Hover DataTip Card when hovering over a variable in opts.debugVariables
    if (extData && !extData->hoveredWord.empty() && hoveredWordScreenRect.w > 0.0f && !opts.debugVariables.empty()) {
        for (const CodeDebugVar& dv : opts.debugVariables) {
            if (dv.name == extData->hoveredWord) {
                const float tipFs = theme.GetFontSize(TextRole::LabelSmall, size);
                const std::string headerStr = dv.type.empty()
                    ? std::string(dv.name)
                    : std::format("{} : {}", dv.name, dv.type);
                const float w1 = MeasureChunkNoTabs(&canvas, headerStr, tipFs, FontStyle::Monospace);
                const float w2 = MeasureChunkNoTabs(&canvas, dv.value, tipFs, FontStyle::Monospace);
                const float cardW = std::max(w1, w2) + theme.Scale(18.0f);
                const float cardH = theme.Scale(38.0f);
                float cardX = std::clamp(hoveredWordScreenRect.x, textViewport.x + 4.0f, std::max(textViewport.x + 4.0f, textViewport.Right() - cardW - 4.0f));
                float cardY = hoveredWordScreenRect.y - cardH - theme.Scale(4.0f);
                if (cardY < innerBounds.y + 4.0f) {
                    cardY = hoveredWordScreenRect.Bottom() + theme.Scale(4.0f);
                }
                const Rect cardRect(cardX, cardY, cardW, cardH);
                canvas.DrawRoundRect(cardRect, theme.Scale(5.0f), pal.surfaceElevated, pal.focusRing, theme.Scale(1.2f));
                canvas.DrawTextStyled(headerStr, {cardX + theme.Scale(8.0f), cardY + theme.Scale(4.0f)}, tipFs, synPal.type, FontStyle::Monospace);
                canvas.DrawTextStyled(dv.value, {cardX + theme.Scale(8.0f), cardY + theme.Scale(20.0f)}, tipFs, pal.textPrimary, FontStyle::Monospace);
                break;
            }
        }
    }

    // 9. Render Floating Debug Toolbar (Continue, Step Over, Step Into, Step Out, Restart, Stop) — via UiSize & Button::Render
    const UiSize tbSize = L.toolbarSize;
    const ComponentMetrics& tbCm = theme.GetMetrics(tbSize);
    const float badgeFs = tbCm.companionFontSize;
    const float btnRad = tbCm.cornerRadius;
    if (opts.showDebugToolbar && L.debugToolbar.panelRect.w > 0.0f) {
        const auto& DT = L.debugToolbar;
        const bool gripDragging = extData && extData->debugToolbarDragging;
        const bool gripHovered = extData && (extData->hoveredDebugBtn == 98 || gripDragging);
        const Color dtBorderCol = gripDragging ? pal.focusRing : (gripHovered ? pal.textSecondary : pal.borderHover);
        canvas.DrawRoundRect(DT.panelRect, btnRad + theme.Scale(3.0f), pal.surfaceElevated.WithAlphaF(0.97f), dtBorderCol, theme.Scale(gripDragging ? 1.5f : 1.2f));

        // Draw VS Code-style 6-dot drag grip on the left side of the floating debug toolbar
        const float gripSlotW = DT.btnRects[0].x - DT.panelRect.x;
        const float dotSz = std::max(2.0f, tbCm.iconSize * 0.14f);
        const float dotStepY = tbCm.iconSize * 0.32f;
        const float dotStepX = tbCm.iconSize * 0.28f;
        const float gripX = DT.panelRect.x + (gripSlotW - dotStepX - dotSz) * 0.5f;
        const float gripCy = DT.panelRect.y + DT.panelRect.h * 0.5f;
        const Color gripCol = gripDragging ? pal.focusRing : (gripHovered ? pal.textPrimary : pal.textMuted.WithAlphaF(0.75f));
        for (int r = -1; r <= 1; ++r) {
            const float dy = gripCy + static_cast<float>(r) * dotStepY - dotSz * 0.5f;
            canvas.DrawRoundRect(Rect(gripX, dy, dotSz, dotSz), dotSz * 0.5f, gripCol);
            canvas.DrawRoundRect(Rect(gripX + dotStepX, dy, dotSz, dotSz), dotSz * 0.5f, gripCol);
        }

        // Draw vertical divider between Step Out (btn 3) and Restart (btn 4) via Toolbar::RenderSeparator
        const Rect sepRect(
            DT.btnRects[3].Right(),
            DT.panelRect.y,
            std::max(1.0f, DT.btnRects[4].x - DT.btnRects[3].Right()),
            DT.panelRect.h
        );
        Toolbar::RenderSeparator(canvas, sepRect, pal.borderSubtle, theme);

        static constexpr std::array<IconId, 6> kDbgIcons = {
            IconId::DebugContinue, // Continue (F5)
            IconId::StepOver,      // Step Over (F10)
            IconId::StepInto,      // Step Into (F11)
            IconId::StepOut,       // Step Out (Shift+F11)
            IconId::Refresh,       // Restart
            IconId::Stop           // Stop (Shift+F5)
        };
        const std::array<Color, 6> dbgColors = {
            Color(34, 197, 94),   // Continue (F5) - Emerald
            Color(56, 189, 248),  // Step Over (F10) - Sky Blue
            Color(56, 189, 248),  // Step Into (F11) - Sky Blue
            Color(56, 189, 248),  // Step Out (Shift+F11) - Sky Blue
            Color(34, 197, 94),   // Restart - Emerald/Green (VS Code style)
            Color(239, 68, 68)    // Stop (Shift+F5) - Red
        };
        for (int i = 0; i < 6; ++i) {
            const Rect br = DT.btnRects[static_cast<size_t>(i)];
            const bool bHov = extData && (extData->hoveredDebugBtn == i);
            const Color baseCol = dbgColors[static_cast<size_t>(i)];

            ButtonOptions bOpts{};
            bOpts.icon = Icon(kDbgIcons[static_cast<size_t>(i)]);
            bOpts.square = true;
            bOpts.variant = UiVariant::Ghost;
            bOpts.iconColor = bHov ? baseCol : baseCol.WithAlphaF(0.95f);
            bOpts.size = tbSize;
            bOpts.stretch = StretchMode::Fill;
            Button::Render(canvas, br, bOpts, tbSize, bHov, false, disabled, theme, false);
        }
    }

    // 10. Render Floating Find & Replace / Go to Line Overlay — via UiSize & Button::Render
    if ((searchOpen || gotoOpen) && L.searchOverlay.panelRect.w > 0.0f && extData) {
        const auto& S = L.searchOverlay;
        canvas.DrawRoundRect(S.panelRect, btnRad + theme.Scale(2.0f), pal.surfaceElevated.WithAlphaF(0.97f), pal.borderHover, 1.0f);

        const int hovBtn = extData->hoveredOverlayBtn;
        auto renderSquareToolBtn = [&](Rect rBtn, IconId iconId, bool activeToggle, int btnIdx) {
            if (rBtn.w <= 0.0f) return;
            ButtonOptions bOpts{};
            bOpts.icon = Icon(iconId);
            bOpts.square = true;
            bOpts.variant = activeToggle ? UiVariant::Primary : UiVariant::Ghost;
            bOpts.active = activeToggle;
            bOpts.size = tbSize;
            bOpts.stretch = StretchMode::Fill;
            Button::Render(canvas, rBtn, bOpts, tbSize, hovBtn == btnIdx, false, disabled, theme, false);
        };

        const float inputPadX = std::max(theme.Scale(6.0f), tbCm.paddingX * 0.55f);

        if (searchOpen) {
            renderSquareToolBtn(S.toggleReplaceBtn, replaceOpen ? IconId::ChevronDown : IconId::ChevronRight, replaceOpen, 0);

            const bool findFocused = (extData->overlayFocus == 1);
            canvas.DrawRoundRect(S.findInputRect, btnRad, pal.inputBackground, findFocused ? pal.focusRing : pal.inputBorder, 1.0f);
            const Rect findTextRect(S.findInputRect.x + inputPadX, S.findInputRect.y, std::max(10.0f, S.caseBtnRect.x - S.findInputRect.x - inputPadX * 1.3f), S.findInputRect.h);
            if (extData->searchQuery.empty() && !findFocused) {
                canvas.DrawTextBox("Find...", findTextRect, badgeFs, pal.textMuted, 0.0f, 0.5f);
            } else {
                canvas.DrawTextBox(extData->searchQuery, findTextRect, badgeFs, pal.textPrimary, 0.0f, 0.5f);
                if (findFocused && cursorVisible) {
                    const float qW = TextUtil::MeasureWidth(extData->searchQuery, badgeFs, &canvas);
                    const float cx = std::min(findTextRect.Right() - 2.0f, findTextRect.x + qW + 1.0f);
                    const float caretPad = std::max(3.0f, S.findInputRect.h * 0.16f);
                    canvas.DrawRect(Rect(cx, S.findInputRect.y + caretPad, 1.5f, S.findInputRect.h - caretPad * 2.0f), pal.caret);
                }
            }

            // Match Case ("Aa") square toggle button
            {
                ButtonOptions caseOpts{};
                caseOpts.label = "Aa";
                caseOpts.square = true;
                caseOpts.variant = extData->matchCase ? UiVariant::Primary : UiVariant::Ghost;
                caseOpts.active = extData->matchCase;
                caseOpts.size = tbSize;
                caseOpts.stretch = StretchMode::Fill;
                Button::Render(canvas, S.caseBtnRect, caseOpts, tbSize, hovBtn == 1, false, disabled, theme, false);
            }
            // Match Whole Word ("ab" with bottom bracket underline) square toggle button
            {
                ButtonOptions wordOpts{};
                wordOpts.label = "ab";
                wordOpts.square = true;
                wordOpts.variant = extData->wholeWord ? UiVariant::Primary : UiVariant::Ghost;
                wordOpts.active = extData->wholeWord;
                wordOpts.size = tbSize;
                wordOpts.stretch = StretchMode::Fill;
                Button::Render(canvas, S.wordBtnRect, wordOpts, tbSize, hovBtn == 2, false, disabled, theme, false);

                const Color fg = extData->wholeWord ? theme.GetVariant(UiVariant::Primary).text : ((hovBtn == 2) ? pal.textPrimary : pal.textSecondary);
                const float bx1 = S.wordBtnRect.x + S.wordBtnRect.w * 0.20f;
                const float bx2 = S.wordBtnRect.Right() - S.wordBtnRect.w * 0.20f;
                const float by = S.wordBtnRect.Bottom() - S.wordBtnRect.h * 0.14f;
                const float bh = std::max(1.8f, S.wordBtnRect.h * 0.10f);
                canvas.DrawLine({bx1, by - bh}, {bx1, by}, fg, 1.2f);
                canvas.DrawLine({bx1, by}, {bx2, by}, fg, 1.2f);
                canvas.DrawLine({bx2, by - bh}, {bx2, by}, fg, 1.2f);
            }

            char cntBuf[32]{};
            if (extData->searchQuery.empty()) {
                std::snprintf(cntBuf, sizeof(cntBuf), "0/0");
            } else if (searchMatches.empty()) {
                std::snprintf(cntBuf, sizeof(cntBuf), "0 res");
            } else {
                std::snprintf(cntBuf, sizeof(cntBuf), "%zu/%zu", extData->activeMatchIdx + 1, searchMatches.size());
            }
            const Color cntCol = (!extData->searchQuery.empty() && searchMatches.empty())
                ? theme.GetTextColor(UiVariant::Danger)
                : pal.textSecondary;
            canvas.DrawTextBox(cntBuf, S.countRect, badgeFs, cntCol, 0.5f, 0.5f);

            // Previous Match (ArrowUp), Next Match (ArrowDown), Close (Close)
            renderSquareToolBtn(S.prevBtnRect, IconId::ArrowUp, false, 3);
            renderSquareToolBtn(S.nextBtnRect, IconId::ArrowDown, false, 4);
            renderSquareToolBtn(S.closeBtnRect, IconId::Close, false, 5);

            if (replaceOpen && S.replaceInputRect.w > 0.0f) {
                const bool repFocused = (extData->overlayFocus == 2);
                canvas.DrawRoundRect(S.replaceInputRect, btnRad, pal.inputBackground, repFocused ? pal.focusRing : pal.inputBorder, 1.0f);
                const Rect repTextRect(S.replaceInputRect.x + inputPadX, S.replaceInputRect.y, S.replaceInputRect.w - inputPadX * 2.0f, S.replaceInputRect.h);
                if (extData->replaceQuery.empty() && !repFocused) {
                    canvas.DrawTextBox("Replace...", repTextRect, badgeFs, pal.textMuted, 0.0f, 0.5f);
                } else {
                    canvas.DrawTextBox(extData->replaceQuery, repTextRect, badgeFs, pal.textPrimary, 0.0f, 0.5f);
                    if (repFocused && cursorVisible) {
                        const float qW = TextUtil::MeasureWidth(extData->replaceQuery, badgeFs, &canvas);
                        const float cx = std::min(repTextRect.Right() - 2.0f, repTextRect.x + qW + 1.0f);
                        const float caretPad = std::max(3.0f, S.replaceInputRect.h * 0.16f);
                        canvas.DrawRect(Rect(cx, S.replaceInputRect.y + caretPad, 1.5f, S.replaceInputRect.h - caretPad * 2.0f), pal.caret);
                    }
                }
                // Replace One & Replace All buttons via Button::Render
                {
                    ButtonOptions rep1Opts{};
                    rep1Opts.label = "1";
                    rep1Opts.icon = Icon::Replace();
                    rep1Opts.variant = UiVariant::Ghost;
                    rep1Opts.size = tbSize;
                    rep1Opts.stretch = StretchMode::Fill;
                    Button::Render(canvas, S.replaceOneBtnRect, rep1Opts, tbSize, hovBtn == 6, false, disabled, theme, false);
                }
                {
                    ButtonOptions repAllOpts{};
                    repAllOpts.label = "All";
                    repAllOpts.icon = Icon::ReplaceAll();
                    repAllOpts.variant = UiVariant::Ghost;
                    repAllOpts.size = tbSize;
                    repAllOpts.stretch = StretchMode::Fill;
                    Button::Render(canvas, S.replaceAllBtnRect, repAllOpts, tbSize, hovBtn == 7, false, disabled, theme, false);
                }
            }
        } else if (gotoOpen) {
            const bool gotoFocused = (extData->overlayFocus == 3);
            canvas.DrawRoundRect(S.findInputRect, btnRad, pal.inputBackground, gotoFocused ? pal.focusRing : pal.inputBorder, 1.0f);
            const Rect gTextRect(S.findInputRect.x + inputPadX, S.findInputRect.y, S.findInputRect.w - inputPadX * 2.0f, S.findInputRect.h);
            if (extData->gotoQuery.empty()) {
                char ph[48]{};
                std::snprintf(ph, sizeof(ph), "Go to Line (1..%zu)...", logicalLines.size());
                canvas.DrawTextBox(ph, gTextRect, badgeFs, pal.textMuted, 0.0f, 0.5f);
            } else {
                canvas.DrawTextBox(extData->gotoQuery, gTextRect, badgeFs, pal.textPrimary, 0.0f, 0.5f);
            }
            renderSquareToolBtn(S.closeBtnRect, IconId::Close, false, 5);
        }
    }

    // 11. Render top-right quick overlays: Copy button & Zoom % reset button — via UiSize & Button::Render
    if (L.copyBtnRect.w > 0.0f) {
        const bool copyHov = extData && extData->copyHovered;
        const bool copied = extData && extData->copyFeedbackActive;
        ButtonOptions copyOpts{};
        copyOpts.label = copied ? "Copied" : "Copy";
        copyOpts.icon = copied ? Icon::Check() : Icon::Copy();
        copyOpts.variant = copied ? UiVariant::Success : UiVariant::Secondary;
        copyOpts.active = copied;
        copyOpts.size = tbSize;
        copyOpts.stretch = StretchMode::Fill;
        Button::Render(canvas, L.copyBtnRect, copyOpts, tbSize, copyHov, false, disabled, theme, false);
    }

    if (L.zoomBtnRect.w > 0.0f) {
        const bool zHov = extData && extData->zoomBadgeHovered;
        char zBuf[16]{};
        std::snprintf(zBuf, sizeof(zBuf), "%d%%", static_cast<int>(std::lround(effZoom * 100.0f)));
        ButtonOptions zoomOpts{};
        zoomOpts.label = zBuf;
        zoomOpts.variant = UiVariant::Secondary;
        zoomOpts.size = tbSize;
        zoomOpts.stretch = StretchMode::Fill;
        Button::Render(canvas, L.zoomBtnRect, zoomOpts, tbSize, zHov, false, disabled, theme, false);
    }

    canvas.PopClip();

    // 12. Render IDE Bottom Status Bar
    if (L.statusBarHeight > 0.0f && L.statusBarRect.w > 0.0f) {
        canvas.PushClip(L.statusBarRect);
        const Color sbBg = (synPal.gutterBg.a > 0)
            ? synPal.gutterBg
            : (isEditorDark ? Color(12, 18, 32, 235) : Color(226, 232, 240, 240));
        canvas.DrawRect(L.statusBarRect, sbBg);
        canvas.DrawLine(
            {L.statusBarRect.x, L.statusBarRect.y},
            {L.statusBarRect.Right(), L.statusBarRect.y},
            editorBorderSubtle,
            1.0f
        );

        const float sbFs = theme.GetFontSize(TextRole::LabelSmall, size);
        const size_t curLine1 = activeLogicalLine + 1;
        const size_t curCol1 = (safeCursor >= logicalLines[activeLogicalLine].startOffset)
            ? (safeCursor - logicalLines[activeLogicalLine].startOffset + 1)
            : 1;

        size_t errCnt = 0;
        size_t warnCnt = 0;
        for (const CodeDiagnostic& d : opts.diagnostics) {
            if (d.severity == DiagnosticSeverity::Error) ++errCnt;
            else if (d.severity == DiagnosticSeverity::Warning) ++warnCnt;
        }

        float sbX = L.statusBarRect.x + theme.Scale(8.0f);
        const float sbCy = L.statusBarRect.y + L.statusBarRect.h * 0.5f;
        const float sbIcoSz = theme.Scale(11.0f);

        // Debugger status indicator icon + label
        if (opts.executionLine > 0) {
            const Color pauseCol(251, 191, 36);
            Icon::Pause().Draw(canvas, Rect(sbX, sbCy - sbIcoSz * 0.5f, sbIcoSz, sbIcoSz), pauseCol);
            sbX += sbIcoSz + theme.Scale(4.0f);
            char dbgBuf[48]{};
            std::snprintf(dbgBuf, sizeof(dbgBuf), "PAUSED Ln %zu  |", opts.executionLine);
            const float dw = TextUtil::MeasureWidth(dbgBuf, sbFs, &canvas);
            canvas.DrawTextBox(dbgBuf, Rect(sbX, L.statusBarRect.y, dw + 6.0f, L.statusBarRect.h), sbFs, pauseCol, 0.0f, 0.5f);
            sbX += dw + theme.Scale(8.0f);
        }

        // Error & Warning icons + counts
        const Color errCol = (errCnt > 0) ? Color(239, 68, 68) : editorTextSecondary;
        Icon::Error().Draw(canvas, Rect(sbX, sbCy - sbIcoSz * 0.5f, sbIcoSz, sbIcoSz), errCol);
        sbX += sbIcoSz + theme.Scale(3.0f);
        char errBuf[16]{};
        std::snprintf(errBuf, sizeof(errBuf), "%zu", errCnt);
        const float ew = TextUtil::MeasureWidth(errBuf, sbFs, &canvas);
        canvas.DrawTextBox(errBuf, Rect(sbX, L.statusBarRect.y, ew + 4.0f, L.statusBarRect.h), sbFs, errCol, 0.0f, 0.5f);
        sbX += ew + theme.Scale(8.0f);

        const Color warnCol = (warnCnt > 0) ? Color(245, 158, 11) : editorTextSecondary;
        Icon::Warning().Draw(canvas, Rect(sbX, sbCy - sbIcoSz * 0.5f, sbIcoSz, sbIcoSz), warnCol);
        sbX += sbIcoSz + theme.Scale(3.0f);
        char warnBuf[16]{};
        std::snprintf(warnBuf, sizeof(warnBuf), "%zu", warnCnt);
        const float ww = TextUtil::MeasureWidth(warnBuf, sbFs, &canvas);
        canvas.DrawTextBox(warnBuf, Rect(sbX, L.statusBarRect.y, ww + 4.0f, L.statusBarRect.h), sbFs, warnCol, 0.0f, 0.5f);
        sbX += ww + theme.Scale(10.0f);

        char leftStatus[128]{};
        if (hasSelection) {
            std::snprintf(leftStatus, sizeof(leftStatus), "|  Ln %zu, Col %zu (%zu sel)  |  %zu lines  |  %zu BPs",
                          curLine1, curCol1, selEnd - selStart, logicalLines.size(), breakpoints.size());
        } else {
            std::snprintf(leftStatus, sizeof(leftStatus), "|  Ln %zu, Col %zu  |  %zu lines  |  %zu BPs",
                          curLine1, curCol1, logicalLines.size(), breakpoints.size());
        }
        canvas.DrawTextBox(
            leftStatus,
            Rect(sbX, L.statusBarRect.y, std::max(10.0f, L.statusBarRect.w * 0.65f - (sbX - L.statusBarRect.x)), L.statusBarRect.h),
            sbFs,
            editorTextSecondary,
            0.0f,
            0.5f
        );

        const std::string_view langName = (opts.syntax.has_value() && opts.syntax->enabled)
            ? opts.syntax->languageName
            : "Plain Text";
        char rightStatus[96]{};
        std::snprintf(rightStatus, sizeof(rightStatus), "Spaces: 4  |  UTF-8  |  %d%%  |  %.*s",
                      static_cast<int>(std::lround(effZoom * 100.0f)),
                      static_cast<int>(langName.size()), langName.data());
        canvas.DrawTextBox(
            rightStatus,
            Rect(L.statusBarRect.x + L.statusBarRect.w * 0.45f, L.statusBarRect.y, L.statusBarRect.w * 0.55f - theme.Scale(10.0f), L.statusBarRect.h),
            sbFs,
            editorTextSecondary,
            1.0f,
            0.5f
        );
        canvas.PopClip();
    }
}

} // namespace guikit
