#include "text_area_internal.hpp"

namespace guikit {

using namespace detail::ta_impl;

namespace {

// --- Built-in Language Dictionaries ---

static constexpr keel::StaticArray<keel::StringRef, 67> kCppKeywords = {
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

static constexpr keel::StaticArray<keel::StringRef, 87> kCppTypes = {
    "bool", "char", "char8_t", "char16_t", "char32_t", "wchar_t", "short", "int",
    "long", "signed", "unsigned", "float", "double", "void", "size_t", "ptrdiff_t",
    "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t",
    "uint64_t", "std", "string", "string_view", "vector", "array", "span",
    "optional", "pair", "tuple", "unique_ptr", "shared_ptr", "function",
    "guikit", "Context", "Response", "WidgetResponse", "ContainerScope",
    "UiTheme", "UiStyle", "UiSize", "UiVariant", "UiText", "TextRole", "TextWrap",
    "TextLines", "TextVariants", "FontStyle", "Density", "ThemeMode", "Color",
    "Rect", "Vec2", "Padding", "Icon", "IconId", "Align", "Alignment", "Justify",
    "SizeMode", "AxisSize", "Orientation", "ElementLayout", "HeadingLevel",
    "HeadingLayout", "HeadingSeparator", "CursorMode", "TabBarStyle", "SyntaxConfig",
    "SyntaxPalette", "TextSpan", "Element", "Button", "Slider", "StepSlider",
    "InputField", "Select", "Switch", "Checkbox", "Heading", "Text", "TextArea",
    "TabBar", "Toolbar"
};

static constexpr keel::StaticArray<keel::StringRef, 24> kMatlabKeywords = {
    "break", "case", "catch", "classdef", "continue", "else", "elseif", "end",
    "enumeration", "events", "for", "function", "global", "if", "methods",
    "otherwise", "parfor", "persistent", "properties", "return", "spmd",
    "switch", "try", "while"
};

static constexpr keel::StaticArray<keel::StringRef, 21> kMatlabTypes = {
    "double", "single", "int8", "uint8", "int16", "uint16", "int32", "uint32",
    "int64", "uint64", "logical", "char", "string", "cell", "struct", "table",
    "timetable", "categorical", "function_handle", "gpuArray", "complex"
};

static constexpr keel::StaticArray<keel::StringRef, 68> kMatlabBuiltins = {
    "fft", "ifft", "fftshift", "ifftshift", "abs", "angle", "real", "imag",
    "conj", "zeros", "ones", "eye", "linspace", "logspace", "sin", "cos",
    "tan", "asin", "acos", "atan2", "exp", "log", "log10", "sqrt", "sum",
    "mean", "std", "var", "max", "min", "norm", "dot", "cross", "inv", "pinv",
    "eig", "svd", "qr", "lu", "chol", "size", "length", "numel", "reshape",
    "repmat", "permute", "squeeze", "find", "sort", "conv", "filter", "fft2",
    "hann", "hamming", "blackman", "chebwin", "pi", "inf", "Inf", "nan", "NaN",
    "eps", "true", "false", "disp", "fprintf", "sprintf", "error"
};

static constexpr keel::StaticArray<keel::StringRef, 11> kMatlabFoldOpen = {
    "function", "if", "for", "while", "switch", "try", "parfor", "classdef",
    "methods", "properties", "events"
};

static constexpr keel::StaticArray<keel::StringRef, 1> kMatlabFoldClose = {
    "end"
};

static constexpr keel::StaticArray<keel::StringRef, 3> kJsonKeywords = {
    "true", "false", "null"
};

static constexpr keel::StaticArray<keel::StringRef, 35> kPythonKeywords = {
    "False", "None", "True", "and", "as", "assert", "async", "await", "break",
    "class", "continue", "def", "del", "elif", "else", "except", "finally",
    "for", "from", "global", "if", "import", "in", "is", "lambda", "nonlocal",
    "not", "or", "pass", "raise", "return", "try", "while", "with", "yield"
};

static constexpr keel::StaticArray<keel::StringRef, 18> kPythonTypes = {
    "int", "float", "complex", "bool", "str", "bytes", "list", "tuple", "dict",
    "set", "frozenset", "object", "type", "Optional", "List", "Dict", "Tuple", "Any"
};

static constexpr keel::StaticArray<keel::StringRef, 22> kPythonBuiltins = {
    "print", "len", "range", "enumerate", "zip", "map", "filter", "sorted",
    "sum", "min", "max", "abs", "round", "isinstance", "getattr", "setattr",
    "hasattr", "open", "super", "property", "staticmethod", "classmethod"
};


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

SyntaxConfig SyntaxConfig::ForLanguage(keel::StringRef langTag) noexcept {
    keel::SmallString<32> lower;
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

void detail::TextArea::HighlightLine(
    keel::StringRef lineText,
    keel::usize /*lineIndex*/,
    const SyntaxConfig& config,
    keel::Array<TextSpan>& outSpans,
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
        outSpans.push_back({0, keel::as<keel::u32>(lineText.size()), pal.identifier});
        return;
    }

    const keel::usize len = lineText.size();
    keel::usize i = 0;

    // Stateful multiline block comment continuation from previous line
    if (inOutBlockComment && *inOutBlockComment && !config.blockCommentEnd.empty()) {
        const keel::usize endPos = lineText.find(config.blockCommentEnd);
        if (endPos == keel::StringRef::npos) {
            outSpans.push_back({0, keel::as<keel::u32>(len), pal.comment});
            return;
        }
        i = endPos + config.blockCommentEnd.size();
        outSpans.push_back({0, keel::as<keel::u32>(i), pal.comment});
        *inOutBlockComment = false;
    }

    // Skip leading whitespace
    if (i == 0) {
        while (i < len && (lineText[i] == ' ' || lineText[i] == '\t')) {
            ++i;
        }
        if (i > 0) {
            outSpans.push_back({0, keel::as<keel::u32>(i), pal.identifier});
        }
    }

    // Section header check (e.g. "%%" in MATLAB or "// ===" in C++)
    if (!config.sectionPrefix.empty() && i + config.sectionPrefix.size() <= len &&
        lineText.substr(i, config.sectionPrefix.size()) == config.sectionPrefix) {
        outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(len - i), pal.preprocessor});
        return;
    }

    // Preprocessor directive check
    if (config.preprocessorPrefix != '\0' && i < len && lineText[i] == config.preprocessorPrefix) {
        keel::usize qPos = keel::StringRef::npos;
        for (keel::usize k = i + 1; k < len; ++k) {
            if (lineText[k] == '"' || lineText[k] == '<') {
                qPos = k;
                break;
            }
        }
        if (qPos != keel::StringRef::npos && config.highlightStrings) {
            outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(qPos - i), pal.preprocessor});
            outSpans.push_back({keel::as<keel::u32>(qPos), keel::as<keel::u32>(len - qPos), pal.string});
        } else {
            outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(len - i), pal.preprocessor});
        }
        return;
    }

    while (i < len) {
        // 1. Whitespace run
        if (lineText[i] == ' ' || lineText[i] == '\t') {
            const keel::usize start = i;
            while (i < len && (lineText[i] == ' ' || lineText[i] == '\t')) {
                ++i;
            }
            outSpans.push_back({keel::as<keel::u32>(start), keel::as<keel::u32>(i - start), pal.identifier});
            continue;
        }

        // 2. Block comment (checked BEFORE lineComment so MATLAB "%{" is recognized before "%")
        if (!config.blockCommentStart.empty() &&
            i + config.blockCommentStart.size() <= len &&
            lineText.substr(i, config.blockCommentStart.size()) == config.blockCommentStart) {
            const keel::usize start = i;
            i += config.blockCommentStart.size();
            if (!config.blockCommentEnd.empty()) {
                const keel::usize endPos = lineText.find(config.blockCommentEnd, i);
                if (endPos != keel::StringRef::npos) {
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
            outSpans.push_back({keel::as<keel::u32>(start), keel::as<keel::u32>(i - start), pal.comment});
            continue;
        }

        // 3. Single-line comment
        if (!config.lineComment.empty() &&
            i + config.lineComment.size() <= len &&
            lineText.substr(i, config.lineComment.size()) == config.lineComment) {
            outSpans.push_back({keel::as<keel::u32>(i), keel::as<keel::u32>(len - i), pal.comment});
            break;
        }

        // 4. String / char literal (with MATLAB transpose apostrophe disambiguation & JSON key detection)
        if (config.highlightStrings && (lineText[i] == '"' || lineText[i] == '\'')) {
            if (lineText[i] == '\'' && config.transposeApostrophe && i > 0) {
                const char prev = lineText[i - 1];
                if (IsIdentChar(prev) || prev == ')' || prev == ']' || prev == '}' || prev == '.') {
                    outSpans.push_back({keel::as<keel::u32>(i), 1u, config.highlightPunctuation ? pal.punctuation : pal.identifier});
                    ++i;
                    continue;
                }
            }
            const char quote = lineText[i];
            const keel::usize start = i++;
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
                keel::usize look = i;
                while (look < len && (lineText[look] == ' ' || lineText[look] == '\t')) ++look;
                if (look < len && lineText[look] == ':') {
                    strCol = pal.type;
                }
            }
            outSpans.push_back({keel::as<keel::u32>(start), keel::as<keel::u32>(i - start), strCol});
            continue;
        }

        // 5. Numeric literal
        if (config.highlightNumbers &&
            (keel::is_ascii_digit(lineText[i]) ||
             (lineText[i] == '.' && i + 1 < len && keel::is_ascii_digit(lineText[i + 1])))) {
            const keel::usize start = i++;
            while (i < len && (keel::is_ascii_alnum(lineText[i]) || lineText[i] == '.' ||
                               (!config.transposeApostrophe && lineText[i] == '\''))) {
                ++i;
            }
            outSpans.push_back({keel::as<keel::u32>(start), keel::as<keel::u32>(i - start), pal.number});
            continue;
        }

        // 6. Identifier / keyword / type / builtin / function call
        if (IsIdentStart(lineText[i])) {
            const keel::usize start = i++;
            while (i < len && IsIdentChar(lineText[i])) {
                ++i;
            }
            const keel::StringRef word = lineText.substr(start, i - start);
            Color tokenColor = pal.identifier;
            if (ContainsToken(config.keywords, word)) {
                tokenColor = pal.keyword;
            } else if (ContainsToken(config.types, word)) {
                tokenColor = pal.type;
            } else if (ContainsToken(config.builtins, word)) {
                tokenColor = pal.function;
            } else if (config.highlightFunctions) {
                keel::usize look = i;
                while (look < len && (lineText[look] == ' ' || lineText[look] == '\t')) {
                    ++look;
                }
                if (look < len && lineText[look] == '(') {
                    tokenColor = pal.function;
                }
            }
            outSpans.push_back({keel::as<keel::u32>(start), keel::as<keel::u32>(i - start), tokenColor});
            continue;
        }

        // 7. Punctuation / operators
        const keel::usize start = i++;
        while (i < len &&
               lineText[i] != ' ' && lineText[i] != '\t' &&
               lineText[i] != '"' && lineText[i] != '\'' &&
               !IsIdentStart(lineText[i]) &&
               !keel::is_ascii_digit(lineText[i])) {
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
            keel::as<keel::u32>(start),
            keel::as<keel::u32>(i - start),
            config.highlightPunctuation ? pal.punctuation : pal.identifier
        });
    }
}

void detail::TextArea::CppHighlighter(
    keel::StringRef lineText,
    keel::usize lineIndex,
    keel::Array<TextSpan>& outSpans,
    const UiTheme& theme
) {
    HighlightLine(lineText, lineIndex, SyntaxConfig::Cpp(), outSpans, theme, nullptr, nullptr);
}


} // namespace guikit
