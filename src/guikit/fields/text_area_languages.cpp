#include "text_area_internal.hpp"

namespace guikit {

namespace {

template <keel::usize N>
[[nodiscard]] constexpr keel::StaticArray<keel::StringRef, N> MakeSortedTokens(
    keel::StaticArray<keel::StringRef, N> arr
) noexcept {
    for (keel::usize i = 1u; i < N; ++i) {
        const keel::StringRef key = arr[i];
        keel::usize j = i;
        while (j > 0u && key < arr[j - 1u]) {
            arr[j] = arr[j - 1u];
            --j;
        }
        arr[j] = key;
    }
    return arr;
}

template <keel::usize N>
[[nodiscard]] constexpr bool VerifyStrictlySorted(const keel::StaticArray<keel::StringRef, N>& arr) noexcept {
    for (keel::usize i = 1u; i < N; ++i) {
        if (!(arr[i - 1u] < arr[i])) return false;
    }
    return true;
}

template <keel::usize N>
[[nodiscard]] constexpr bool VerifyStrictlySortedLowercase(const keel::StaticArray<keel::StringRef, N>& arr) noexcept {
    if (!VerifyStrictlySorted(arr)) return false;
    for (keel::usize i = 0u; i < N; ++i) {
        for (char c : arr[i]) {
            if (c >= 'A' && c <= 'Z') return false;
        }
    }
    return true;
}

// =============================================================================
// Compile-Time Sorted Built-in Language Token Bank (O(log K) binary search!)
// =============================================================================
// Storing every built-in table inside one constexpr struct guarantees:
//   1. Zero runtime initialization cost & zero frame-arena copies.
//   2. O(1) pointer-range membership check in IsBuiltInSortedTokenList().
//   3. Compile-time strict-order & duplicate verification via static_assert.

struct BuiltInTokenBank {
    // --- 1. C / C++20 ---
    keel::StaticArray<keel::StringRef, 68> cppKeywords = MakeSortedTokens<68>({
        "alignas", "alignof", "auto", "break", "case", "catch", "class", "concept",
        "const", "consteval", "constexpr", "constinit", "continue", "co_await",
        "co_return", "co_yield", "decltype", "default", "delete", "do", "else",
        "enum", "explicit", "export", "extern", "false", "final", "for", "friend",
        "goto", "if", "import", "inline", "module", "mutable", "namespace", "new",
        "noexcept", "nullptr", "operator", "override", "private", "protected", "public",
        "requires", "return", "sizeof", "static", "static_assert", "static_cast",
        "dynamic_cast", "reinterpret_cast", "const_cast", "struct", "switch", "template",
        "this", "throw", "true", "try", "typedef", "typeid", "typename", "union",
        "using", "virtual", "volatile", "while"
    });
    keel::StaticArray<keel::StringRef, 108> cppTypes = MakeSortedTokens<108>({
        "bool", "char", "char8_t", "char16_t", "char32_t", "wchar_t", "short", "int",
        "long", "signed", "unsigned", "float", "double", "void", "size_t", "ptrdiff_t",
        "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t",
        "uint64_t", "std", "string", "string_view", "vector", "array", "span",
        "optional", "pair", "tuple", "unique_ptr", "shared_ptr", "function",
        "keel", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64",
        "usize", "isize", "String", "StringRef", "Array", "ArrayRef", "SmallArray",
        "StaticArray", "Optional", "TextDocument",
        "guikit", "Context", "Response", "WidgetResponse", "ContainerScope",
        "UiTheme", "UiStyle", "UiSize", "UiVariant", "UiText", "TextRole", "TextWrap",
        "TextLines", "TextVariants", "FontStyle", "Density", "ThemeMode", "Color",
        "Rect", "Vec2", "Padding", "Icon", "IconId", "Align", "Alignment", "Justify",
        "SizeMode", "AxisSize", "Orientation", "ElementLayout", "HeadingLevel",
        "HeadingLayout", "HeadingSeparator", "CursorMode", "TabBarStyle", "SyntaxConfig",
        "SyntaxPalette", "TextSpan", "Element", "Button", "Slider", "StepSlider",
        "InputField", "Select", "Switch", "Checkbox", "Heading", "Text", "TextArea",
        "TabBar", "Toolbar"
    });

    // --- 2. MATLAB / Octave ---
    keel::StaticArray<keel::StringRef, 24> matlabKeywords = MakeSortedTokens<24>({
        "break", "case", "catch", "classdef", "continue", "else", "elseif", "end",
        "enumeration", "events", "for", "function", "global", "if", "methods",
        "otherwise", "parfor", "persistent", "properties", "return", "spmd",
        "switch", "try", "while"
    });
    keel::StaticArray<keel::StringRef, 21> matlabTypes = MakeSortedTokens<21>({
        "double", "single", "int8", "uint8", "int16", "uint16", "int32", "uint32",
        "int64", "uint64", "logical", "char", "string", "cell", "struct", "table",
        "timetable", "categorical", "function_handle", "gpuArray", "complex"
    });
    keel::StaticArray<keel::StringRef, 68> matlabBuiltins = MakeSortedTokens<68>({
        "fft", "ifft", "fftshift", "ifftshift", "abs", "angle", "real", "imag",
        "conj", "zeros", "ones", "eye", "linspace", "logspace", "sin", "cos",
        "tan", "asin", "acos", "atan2", "exp", "log", "log10", "sqrt", "sum",
        "mean", "std", "var", "max", "min", "norm", "dot", "cross", "inv", "pinv",
        "eig", "svd", "qr", "lu", "chol", "size", "length", "numel", "reshape",
        "repmat", "permute", "squeeze", "find", "sort", "conv", "filter", "fft2",
        "hann", "hamming", "blackman", "chebwin", "pi", "inf", "Inf", "nan", "NaN",
        "eps", "true", "false", "disp", "fprintf", "sprintf", "error"
    });
    keel::StaticArray<keel::StringRef, 11> matlabFoldOpen = MakeSortedTokens<11>({
        "function", "if", "for", "while", "switch", "try", "parfor", "classdef",
        "methods", "properties", "events"
    });
    keel::StaticArray<keel::StringRef, 1> matlabFoldClose = MakeSortedTokens<1>({
        "end"
    });

    // --- 3. JSON / JSONC ---
    keel::StaticArray<keel::StringRef, 3> jsonKeywords = MakeSortedTokens<3>({
        "true", "false", "null"
    });

    // --- 4. Python 3 ---
    keel::StaticArray<keel::StringRef, 39> pythonKeywords = MakeSortedTokens<39>({
        "False", "None", "True", "and", "as", "assert", "async", "await", "break",
        "case", "class", "continue", "def", "del", "elif", "else", "except", "finally",
        "for", "from", "global", "if", "import", "in", "is", "lambda", "match",
        "nonlocal", "not", "or", "pass", "raise", "return", "self", "cls", "try",
        "while", "with", "yield"
    });
    keel::StaticArray<keel::StringRef, 32> pythonTypes = MakeSortedTokens<32>({
        "int", "float", "complex", "bool", "str", "bytes", "bytearray", "memoryview",
        "list", "tuple", "dict", "set", "frozenset", "object", "type", "Optional",
        "List", "Dict", "Tuple", "Set", "Any", "Union", "Callable", "Sequence",
        "Mapping", "Iterable", "Iterator", "Generator", "Literal", "Final", "Protocol",
        "Exception"
    });
    keel::StaticArray<keel::StringRef, 36> pythonBuiltins = MakeSortedTokens<36>({
        "print", "len", "range", "enumerate", "zip", "map", "filter", "sorted",
        "reversed", "sum", "min", "max", "abs", "round", "pow", "divmod", "all", "any",
        "isinstance", "issubclass", "getattr", "setattr", "hasattr", "delattr",
        "open", "super", "property", "staticmethod", "classmethod", "id", "hash",
        "repr", "chr", "ord", "next", "iter"
    });

    // --- 5. Rust ---
    keel::StaticArray<keel::StringRef, 41> rustKeywords = MakeSortedTokens<41>({
        "as", "async", "await", "break", "const", "continue", "crate", "dyn", "else",
        "enum", "extern", "false", "fn", "for", "if", "impl", "in", "let", "loop",
        "match", "mod", "move", "mut", "pub", "ref", "return", "self", "static",
        "struct", "super", "trait", "true", "type", "union", "unsafe", "use", "where",
        "while", "yield", "macro_rules", "raw"
    });
    keel::StaticArray<keel::StringRef, 40> rustTypes = MakeSortedTokens<40>({
        "bool", "char", "str", "i8", "i16", "i32", "i64", "i128", "isize",
        "u8", "u16", "u32", "u64", "u128", "usize", "f32", "f64", "Self",
        "String", "Vec", "Option", "Result", "Box", "Rc", "Arc", "RefCell", "Cell",
        "Mutex", "RwLock", "Pin", "Cow", "HashMap", "HashSet", "BTreeMap", "BTreeSet",
        "VecDeque", "Path", "PathBuf", "Slice", "Fn"
    });
    keel::StaticArray<keel::StringRef, 24> rustBuiltins = MakeSortedTokens<24>({
        "Some", "None", "Ok", "Err", "println", "print", "eprintln", "eprint",
        "format", "vec", "panic", "assert", "assert_eq", "assert_ne", "debug_assert",
        "todo", "unimplemented", "unreachable", "dbg", "matches", "include_str",
        "cfg", "derive", "drop"
    });

    // --- 6. TypeScript & JavaScript ---
    keel::StaticArray<keel::StringRef, 62> tsKeywords = MakeSortedTokens<62>({
        "abstract", "as", "asserts", "async", "await", "break", "case", "catch",
        "class", "const", "continue", "debugger", "declare", "default", "delete",
        "do", "else", "enum", "export", "extends", "false", "finally", "for", "from",
        "function", "get", "if", "implements", "import", "in", "infer", "instanceof",
        "interface", "is", "keyof", "let", "module", "namespace", "new", "null", "of",
        "override", "private", "protected", "public", "readonly", "return", "satisfies",
        "set", "static", "super", "switch", "this", "throw", "true", "try", "type",
        "typeof", "void", "while", "with", "yield"
    });
    keel::StaticArray<keel::StringRef, 36> tsTypes = MakeSortedTokens<36>({
        "any", "bigint", "boolean", "never", "number", "object", "string", "symbol",
        "undefined", "unknown", "void", "Array", "ReadonlyArray", "Map", "Set",
        "WeakMap", "WeakSet", "Promise", "Record", "Partial", "Required", "Readonly",
        "Pick", "Omit", "Exclude", "Extract", "ReturnType", "Awaited", "Date", "RegExp",
        "Error", "Uint8Array", "Float32Array", "Float64Array", "HTMLElement", "Function"
    });
    keel::StaticArray<keel::StringRef, 43> jsKeywords = MakeSortedTokens<43>({
        "async", "await", "break", "case", "catch", "class", "const", "continue",
        "debugger", "default", "delete", "do", "else", "export", "extends", "false",
        "finally", "for", "from", "function", "get", "if", "import", "in", "instanceof",
        "let", "new", "null", "of", "return", "set", "static", "super", "switch",
        "this", "throw", "true", "try", "typeof", "void", "while", "with", "yield"
    });
    keel::StaticArray<keel::StringRef, 26> jsBuiltins = MakeSortedTokens<26>({
        "console", "Math", "JSON", "Object", "Number", "String", "Boolean", "Symbol",
        "BigInt", "Proxy", "Reflect", "Intl", "window", "document", "globalThis",
        "fetch", "setTimeout", "setInterval", "clearTimeout", "clearInterval",
        "parseInt", "parseFloat", "isNaN", "isFinite", "NaN", "Infinity"
    });

    // --- 7. GLSL / WGSL Shaders ---
    keel::StaticArray<keel::StringRef, 42> glslKeywords = MakeSortedTokens<42>({
        "attribute", "break", "buffer", "case", "centroid", "coherent", "const",
        "continue", "default", "discard", "do", "else", "flat", "fn", "for", "highp",
        "if", "in", "inout", "invariant", "layout", "let", "lowp", "mediump",
        "noperspective", "out", "patch", "precision", "readonly", "restrict", "return",
        "sample", "shared", "smooth", "struct", "switch", "true", "false", "uniform",
        "var", "varying", "while"
    });
    keel::StaticArray<keel::StringRef, 48> glslTypes = MakeSortedTokens<48>({
        "void", "bool", "int", "uint", "float", "double", "f32", "i32", "u32",
        "vec2", "vec3", "vec4", "bvec2", "bvec3", "bvec4", "ivec2", "ivec3", "ivec4",
        "uvec2", "uvec3", "uvec4", "dvec2", "dvec3", "dvec4",
        "mat2", "mat3", "mat4", "mat2x2", "mat2x3", "mat2x4", "mat3x2", "mat3x3",
        "mat3x4", "mat4x2", "mat4x3", "mat4x4",
        "sampler1D", "sampler2D", "sampler3D", "samplerCube", "sampler2DArray",
        "sampler2DShadow", "isampler2D", "usampler2D", "image2D", "atomic_uint",
        "location", "binding"
    });
    keel::StaticArray<keel::StringRef, 52> glslBuiltins = MakeSortedTokens<52>({
        "abs", "acos", "all", "any", "asin", "atan", "ceil", "clamp", "cos", "cross",
        "dFdx", "dFdy", "degrees", "determinant", "distance", "dot", "exp", "exp2",
        "faceforward", "floor", "fma", "fract", "fwidth", "inverse", "inversesqrt",
        "length", "log", "log2", "max", "min", "mix", "mod", "normalize", "pow",
        "radians", "reflect", "refract", "round", "sign", "sin", "smoothstep", "sqrt",
        "step", "tan", "texelFetch", "texture", "texture2D", "textureLod", "textureSize",
        "transpose", "gl_Position", "gl_FragCoord"
    });

    // --- 8. Go ---
    keel::StaticArray<keel::StringRef, 29> goKeywords = MakeSortedTokens<29>({
        "break", "case", "chan", "const", "continue", "default", "defer", "else",
        "fallthrough", "false", "for", "func", "go", "goto", "if", "import",
        "interface", "iota", "map", "nil", "package", "range", "return", "select",
        "struct", "switch", "true", "type", "var"
    });
    keel::StaticArray<keel::StringRef, 22> goTypes = MakeSortedTokens<22>({
        "any", "bool", "byte", "comparable", "complex64", "complex128", "error",
        "float32", "float64", "int", "int8", "int16", "int32", "int64", "rune",
        "string", "uint", "uint8", "uint16", "uint32", "uint64", "uintptr"
    });
    keel::StaticArray<keel::StringRef, 17> goBuiltins = MakeSortedTokens<17>({
        "append", "cap", "clear", "close", "complex", "copy", "delete", "imag",
        "len", "make", "max", "min", "new", "panic", "print", "println", "recover"
    });

    // --- 9. C# ---
    keel::StaticArray<keel::StringRef, 69> csharpKeywords = MakeSortedTokens<69>({
        "abstract", "as", "async", "await", "base", "break", "case", "catch", "checked",
        "class", "const", "continue", "default", "delegate", "do", "else", "enum",
        "event", "explicit", "extern", "false", "finally", "fixed", "for", "foreach",
        "get", "goto", "if", "implicit", "in", "init", "interface", "internal", "is",
        "lock", "namespace", "new", "null", "operator", "out", "override", "params",
        "partial", "private", "protected", "public", "readonly", "record", "ref",
        "required", "return", "sealed", "set", "sizeof", "stackalloc", "static",
        "struct", "switch", "this", "throw", "true", "try", "typeof", "unchecked",
        "unsafe", "using", "var", "virtual", "while"
    });
    keel::StaticArray<keel::StringRef, 36> csharpTypes = MakeSortedTokens<36>({
        "bool", "byte", "sbyte", "char", "decimal", "double", "float", "int", "uint",
        "nint", "nuint", "long", "ulong", "short", "ushort", "object", "string",
        "dynamic", "void", "String", "Task", "ValueTask", "List", "Dictionary",
        "HashSet", "Span", "ReadOnlySpan", "Memory", "Action", "Func", "IEnumerable",
        "IReadOnlyList", "Nullable", "Exception", "Guid", "DateTime"
    });
    keel::StaticArray<keel::StringRef, 12> csharpBuiltins = MakeSortedTokens<12>({
        "Console", "Math", "Convert", "Array", "Enumerable", "StringBuilder",
        "Environment", "Path", "File", "Directory", "nameof", "yield"
    });

    // --- 10. Java ---
    keel::StaticArray<keel::StringRef, 56> javaKeywords = MakeSortedTokens<56>({
        "abstract", "assert", "break", "case", "catch", "class", "const", "continue",
        "default", "do", "else", "enum", "extends", "false", "final", "finally", "for",
        "goto", "if", "implements", "import", "instanceof", "interface", "native",
        "new", "non-sealed", "null", "package", "permits", "private", "protected",
        "public", "record", "return", "sealed", "static", "strictfp", "super",
        "switch", "synchronized", "this", "throw", "throws", "transient", "true",
        "try", "var", "volatile", "while", "yield", "module", "open", "opens",
        "requires", "exports", "provides"
    });
    keel::StaticArray<keel::StringRef, 34> javaTypes = MakeSortedTokens<34>({
        "boolean", "byte", "char", "double", "float", "int", "long", "short", "void",
        "Object", "String", "Integer", "Long", "Double", "Float", "Boolean", "Character",
        "Byte", "Short", "List", "ArrayList", "Map", "HashMap", "Set", "HashSet",
        "Optional", "Stream", "CompletableFuture", "Collection", "Iterable", "Runnable",
        "Exception", "RuntimeException", "Thread"
    });
    keel::StaticArray<keel::StringRef, 10> javaBuiltins = MakeSortedTokens<10>({
        "System", "Math", "Arrays", "Collections", "Objects", "Files", "Paths",
        "StringBuilder", "Override", "Deprecated"
    });

    // --- 11. Kotlin ---
    keel::StaticArray<keel::StringRef, 66> kotlinKeywords = MakeSortedTokens<66>({
        "abstract", "actual", "annotation", "as", "break", "by", "catch", "class",
        "companion", "const", "constructor", "continue", "crossinline", "data", "do",
        "else", "enum", "expect", "external", "false", "final", "finally", "for",
        "fun", "get", "if", "import", "in", "infix", "init", "inline", "inner",
        "interface", "internal", "is", "lateinit", "noinline", "null", "object", "open",
        "operator", "out", "override", "package", "private", "protected", "public",
        "reified", "return", "sealed", "set", "super", "suspend", "tailrec", "this",
        "throw", "true", "try", "typealias", "val", "value", "var", "vararg", "when",
        "where", "while"
    });
    keel::StaticArray<keel::StringRef, 38> kotlinTypes = MakeSortedTokens<38>({
        "Any", "Array", "Boolean", "Byte", "Char", "CharSequence", "Collection",
        "Comparable", "CoroutineScope", "Deferred", "Double", "Float", "Flow", "Int",
        "IntArray", "Iterable", "Job", "List", "Long", "Map", "MutableList",
        "MutableMap", "MutableSet", "Nothing", "Number", "Pair", "Result", "Sequence",
        "Set", "Short", "StateFlow", "String", "Throwable", "Triple", "UByte", "UInt",
        "ULong", "Unit"
    });
    keel::StaticArray<keel::StringRef, 30> kotlinBuiltins = MakeSortedTokens<30>({
        "TODO", "also", "apply", "arrayOf", "async", "check", "emptyList", "emptyMap",
        "emptySet", "error", "launch", "lazy", "let", "listOf", "mapOf", "maxOf",
        "minOf", "mutableListOf", "mutableMapOf", "mutableSetOf", "print", "println",
        "remember", "repeat", "require", "run", "setOf", "takeIf", "with", "withContext"
    });

    // --- 12. Swift ---
    keel::StaticArray<keel::StringRef, 73> swiftKeywords = MakeSortedTokens<73>({
        "actor", "any", "as", "associatedtype", "async", "await", "borrowing", "break",
        "case", "catch", "class", "consuming", "continue", "default", "defer", "deinit",
        "didSet", "do", "else", "enum", "extension", "fallthrough", "false", "fileprivate",
        "final", "for", "func", "get", "guard", "if", "import", "in", "indirect",
        "infix", "init", "inout", "internal", "is", "lazy", "let", "mutating", "nil",
        "nonmutating", "open", "operator", "override", "private", "protocol", "public",
        "repeat", "rethrows", "return", "self", "sending", "set", "some", "static",
        "struct", "subscript", "super", "switch", "throw", "throws", "true", "try",
        "typealias", "unowned", "var", "weak", "where", "while", "willSet", "yield"
    });
    keel::StaticArray<keel::StringRef, 40> swiftTypes = MakeSortedTokens<40>({
        "Any", "AnyObject", "Array", "Bool", "Character", "Codable", "Comparable",
        "Data", "Date", "Decodable", "Dictionary", "Double", "Encodable", "Equatable",
        "Error", "Float", "Hashable", "Identifiable", "Int", "Int8", "Int16", "Int32",
        "Int64", "Never", "Optional", "Range", "Result", "Self", "Sendable", "Set",
        "String", "Substring", "Task", "UInt", "UInt8", "UInt16", "UInt32", "UInt64",
        "URL", "Void"
    });
    keel::StaticArray<keel::StringRef, 18> swiftBuiltins = MakeSortedTokens<18>({
        "abs", "assert", "assertionFailure", "debugPrint", "dump", "fatalError", "max",
        "min", "precondition", "preconditionFailure", "print", "readLine", "sequence",
        "stride", "swap", "type", "withTaskGroup", "zip"
    });

    // --- 13. Verilog / SystemVerilog ---
    keel::StaticArray<keel::StringRef, 92> verilogKeywords = MakeSortedTokens<92>({
        "always", "always_comb", "always_ff", "always_latch", "and", "assert", "assign",
        "automatic", "begin", "break", "case", "casex", "casez", "class", "clocking",
        "const", "continue", "cover", "default", "defparam", "disable", "do", "edge",
        "else", "end", "endcase", "endclass", "endclocking", "endfunction", "endgenerate",
        "endinterface", "endmodule", "endpackage", "endprogram", "endproperty",
        "endsequence", "endtask", "enum", "export", "extends", "extern", "final", "for",
        "force", "foreach", "forever", "fork", "function", "generate", "genvar", "if",
        "iff", "import", "initial", "inout", "input", "inside", "interface", "join",
        "join_any", "join_none", "local", "localparam", "modport", "module", "negedge",
        "new", "null", "output", "package", "packed", "parameter", "posedge", "priority",
        "program", "property", "protected", "rand", "randc", "repeat", "return",
        "sequence", "static", "struct", "super", "task", "this", "typedef", "union",
        "unique", "virtual", "while"
    });
    keel::StaticArray<keel::StringRef, 20> verilogTypes = MakeSortedTokens<20>({
        "bit", "byte", "chandle", "event", "int", "integer", "logic", "longint",
        "real", "realtime", "reg", "shortint", "shortreal", "signed", "string",
        "time", "tri", "unsigned", "void", "wire"
    });
    keel::StaticArray<keel::StringRef, 24> verilogBuiltins = MakeSortedTokens<24>({
        "$bits", "$cast", "$clog2", "$countones", "$display", "$dumpfile", "$dumpvars",
        "$error", "$fatal", "$fell", "$finish", "$info", "$itor", "$past", "$random",
        "$readmemb", "$readmemh", "$realtime", "$rose", "$signed", "$stop", "$time",
        "$unsigned", "$write"
    });
    keel::StaticArray<keel::StringRef, 13> verilogFoldOpen = MakeSortedTokens<13>({
        "begin", "case", "casex", "casez", "class", "fork", "function", "generate",
        "interface", "module", "package", "program", "task"
    });
    keel::StaticArray<keel::StringRef, 13> verilogFoldClose = MakeSortedTokens<13>({
        "end", "endcase", "endclass", "endfunction", "endgenerate", "endinterface",
        "endmodule", "endpackage", "endprogram", "endtask", "join", "join_any",
        "join_none"
    });

    // --- 14. VHDL (all lowercase -> case-insensitive O(log K) lookup!) ---
    keel::StaticArray<keel::StringRef, 95> vhdlKeywords = MakeSortedTokens<95>({
        "abs", "access", "after", "alias", "all", "and", "architecture", "array",
        "assert", "attribute", "begin", "block", "body", "buffer", "bus", "case",
        "component", "configuration", "constant", "context", "default", "downto",
        "else", "elsif", "end", "entity", "exit", "file", "for", "force", "function",
        "generate", "generic", "guarded", "if", "impure", "in", "inertial", "inout",
        "is", "label", "library", "linkage", "literal", "loop", "map", "mod", "nand",
        "new", "next", "nor", "not", "null", "of", "on", "open", "or", "others", "out",
        "package", "port", "postponed", "procedure", "process", "property", "pure",
        "range", "record", "register", "reject", "rem", "report", "return", "rol",
        "ror", "select", "severity", "shared", "signal", "subtype", "then", "to",
        "transport", "type", "unaffected", "units", "until", "use", "variable", "wait",
        "when", "while", "with", "xnor", "xor"
    });
    keel::StaticArray<keel::StringRef, 32> vhdlTypes = MakeSortedTokens<32>({
        "bit", "bit_vector", "boolean", "character", "delay_length", "ieee", "integer",
        "line", "natural", "numeric_std", "positive", "real", "severity_level",
        "sfixed", "signed", "sla", "sll", "sra", "srl", "std_logic",
        "std_logic_1164", "std_logic_arith", "std_logic_signed", "std_logic_unsigned",
        "std_logic_vector", "std_ulogic", "std_ulogic_vector", "string",
        "text", "time", "ufixed", "unsigned"
    });
    keel::StaticArray<keel::StringRef, 21> vhdlBuiltins = MakeSortedTokens<21>({
        "conv_integer", "conv_std_logic_vector", "event", "falling_edge", "high",
        "image", "left", "length", "low", "now", "resize", "reverse_range", "right",
        "rising_edge", "shift_left", "shift_right", "to_integer", "to_signed",
        "to_stdlogicvector", "to_unsigned", "val"
    });
    keel::StaticArray<keel::StringRef, 14> vhdlFoldOpen = MakeSortedTokens<14>({
        "architecture", "case", "component", "entity", "for", "function", "generate",
        "if", "loop", "package", "procedure", "process", "record", "while"
    });
    keel::StaticArray<keel::StringRef, 1> vhdlFoldClose = MakeSortedTokens<1>({
        "end"
    });

    // --- 15. Lua ---
    keel::StaticArray<keel::StringRef, 22> luaKeywords = MakeSortedTokens<22>({
        "and", "break", "do", "else", "elseif", "end", "false", "for", "function",
        "goto", "if", "in", "local", "nil", "not", "or", "repeat", "return", "then",
        "true", "until", "while"
    });
    keel::StaticArray<keel::StringRef, 28> luaBuiltins = MakeSortedTokens<28>({
        "_G", "assert", "collectgarbage", "coroutine", "debug", "dofile", "error",
        "getmetatable", "io", "ipairs", "load", "math", "next", "os", "package",
        "pairs", "pcall", "print", "rawequal", "rawget", "rawlen", "rawset", "require",
        "select", "setmetatable", "string", "table", "tonumber"
    });
    keel::StaticArray<keel::StringRef, 5> luaFoldOpen = MakeSortedTokens<5>({
        "for", "function", "if", "repeat", "while"
    });
    keel::StaticArray<keel::StringRef, 2> luaFoldClose = MakeSortedTokens<2>({
        "end", "until"
    });

    // --- 16. CMake (all lowercase -> case-insensitive O(log K) lookup!) ---
    keel::StaticArray<keel::StringRef, 31> cmakeKeywords = MakeSortedTokens<31>({
        "and", "block", "break", "continue", "defined", "else", "elseif", "endblock",
        "endforeach", "endfunction", "endif", "endmacro", "endwhile", "equal", "exists",
        "false", "foreach", "function", "greater", "if", "in_list", "less", "macro",
        "matches", "not", "off", "on", "or", "return", "true", "while"
    });
    keel::StaticArray<keel::StringRef, 22> cmakeTypes = MakeSortedTokens<22>({
        "alias", "bool", "cache", "command", "config", "fatal_error", "filepath",
        "force", "imported", "interface", "internal", "module", "object", "path",
        "private", "public", "quiet", "required", "shared", "static", "status", "string"
    });
    keel::StaticArray<keel::StringRef, 36> cmakeBuiltins = MakeSortedTokens<36>({
        "add_compile_definitions", "add_compile_options", "add_custom_command",
        "add_custom_target", "add_dependencies", "add_executable", "add_library",
        "add_subdirectory", "add_test", "cmake_minimum_required", "cmake_parse_arguments",
        "configure_file", "enable_testing", "execute_process", "fetchcontent_declare",
        "file", "find_library", "find_package", "find_path", "find_program",
        "get_filename_component", "get_target_property", "include", "include_guard",
        "install", "list", "message", "option", "project", "set", "set_target_properties",
        "string", "target_compile_definitions", "target_compile_features",
        "target_include_directories", "target_link_libraries"
    });
    keel::StaticArray<keel::StringRef, 6> cmakeFoldOpen = MakeSortedTokens<6>({
        "block", "foreach", "function", "if", "macro", "while"
    });
    keel::StaticArray<keel::StringRef, 6> cmakeFoldClose = MakeSortedTokens<6>({
        "endblock", "endforeach", "endfunction", "endif", "endmacro", "endwhile"
    });

    // --- 17. Bash / Shell ---
    keel::StaticArray<keel::StringRef, 26> shellKeywords = MakeSortedTokens<26>({
        "break", "case", "continue", "declare", "do", "done", "elif", "else", "esac",
        "export", "false", "fi", "for", "function", "if", "in", "local", "readonly",
        "return", "select", "set", "then", "time", "true", "until", "while"
    });
    keel::StaticArray<keel::StringRef, 36> shellBuiltins = MakeSortedTokens<36>({
        "alias", "awk", "cat", "cd", "chmod", "chown", "cmake", "cp", "ctest", "curl",
        "cut", "echo", "env", "eval", "exec", "exit", "find", "git", "grep", "head",
        "kill", "ln", "ls", "make", "mkdir", "mv", "printf", "pwd", "read", "rm",
        "sed", "shift", "sort", "source", "tail", "xargs"
    });
    keel::StaticArray<keel::StringRef, 5> shellFoldOpen = MakeSortedTokens<5>({
        "case", "for", "if", "until", "while"
    });
    keel::StaticArray<keel::StringRef, 3> shellFoldClose = MakeSortedTokens<3>({
        "done", "esac", "fi"
    });

    // --- 18. PowerShell (all lowercase -> case-insensitive O(log K) lookup!) ---
    keel::StaticArray<keel::StringRef, 36> psKeywords = MakeSortedTokens<36>({
        "begin", "break", "catch", "class", "continue", "data", "default", "do",
        "dynamicparam", "else", "elseif", "end", "enum", "exit", "filter", "finally",
        "for", "foreach", "from", "function", "hidden", "if", "in", "param", "process",
        "return", "static", "switch", "throw", "trap", "try", "until", "using", "var",
        "while", "workflow"
    });
    keel::StaticArray<keel::StringRef, 28> psTypes = MakeSortedTokens<28>({
        "array", "bool", "byte", "char", "datetime", "decimal", "double", "float",
        "guid", "hashtable", "int", "int32", "int64", "long", "object",
        "pscustomobject", "psobject", "regex", "scriptblock", "string", "timespan",
        "type", "uint32", "uint64", "uri", "version", "void", "xml"
    });
    keel::StaticArray<keel::StringRef, 58> psBuiltins = MakeSortedTokens<58>({
        "$_", "$args", "$error", "$false", "$input", "$lastexitcode", "$null",
        "$psscriptroot", "$psversiontable", "$true", "add-content", "clear-host",
        "convertfrom-json", "convertto-json", "copy-item", "export-modulemember",
        "foreach-object", "get-childitem", "get-command", "get-content", "get-help",
        "get-item", "get-location", "get-process", "get-service", "import-module",
        "invoke-command", "invoke-expression", "invoke-restmethod", "invoke-webrequest",
        "join-path", "measure-object", "move-item", "new-item", "new-object",
        "out-file", "out-null", "read-host", "remove-item", "select-object",
        "select-string", "set-content", "set-item", "set-location", "set-strictmode",
        "sort-object", "split-path", "start-process", "start-sleep", "stop-process",
        "test-path", "where-object", "write-debug", "write-error", "write-host",
        "write-output", "write-verbose", "write-warning"
    });

    // --- 19. Windows Batch / CMD (all lowercase -> case-insensitive O(log K) lookup!) ---
    keel::StaticArray<keel::StringRef, 27> batchKeywords = MakeSortedTokens<27>({
        "call", "cmdextversion", "defined", "do", "else", "endlocal", "equ",
        "errorlevel", "exist", "exit", "for", "geq", "goto", "gtr", "if", "in", "leq",
        "lss", "neq", "not", "off", "on", "pause", "rem", "set", "setlocal", "shift"
    });
    keel::StaticArray<keel::StringRef, 7> batchTypes = MakeSortedTokens<7>({
        "disabledelayedexpansion", "disableextensions", "enabledelayedexpansion",
        "enableextensions", "eof", "nul", "prn"
    });
    keel::StaticArray<keel::StringRef, 42> batchBuiltins = MakeSortedTokens<42>({
        "assoc", "attrib", "cd", "chdir", "chcp", "choice", "cls", "cmake", "color",
        "copy", "ctest", "del", "dir", "echo", "erase", "find", "findstr", "ftype",
        "git", "md", "mkdir", "move", "msbuild", "net", "ninja", "popd", "powershell",
        "pushd", "pwsh", "rd", "reg", "ren", "rename", "rmdir", "robocopy", "sc",
        "start", "taskkill", "tasklist", "timeout", "title", "xcopy"
    });
    keel::StaticArray<keel::StringRef, 1> batchFoldOpen = MakeSortedTokens<1>({
        "setlocal"
    });
    keel::StaticArray<keel::StringRef, 1> batchFoldClose = MakeSortedTokens<1>({
        "endlocal"
    });

    // --- 18. SQL (all lowercase -> case-insensitive O(log K) lookup!) ---
    keel::StaticArray<keel::StringRef, 73> sqlKeywords = MakeSortedTokens<73>({
        "add", "all", "alter", "and", "as", "asc", "begin", "between", "by", "cascade",
        "case", "check", "column", "commit", "constraint", "create", "cross", "default",
        "delete", "desc", "distinct", "drop", "else", "end", "except", "exists", "false",
        "foreign", "from", "full", "group", "having", "if", "in", "index", "inner",
        "insert", "intersect", "into", "is", "join", "key", "left", "like", "limit",
        "not", "null", "offset", "on", "or", "order", "outer", "over", "partition",
        "primary", "references", "returning", "right", "rollback", "select", "set",
        "table", "then", "true", "union", "unique", "update", "values", "view", "when",
        "where", "window", "with"
    });
    keel::StaticArray<keel::StringRef, 24> sqlTypes = MakeSortedTokens<24>({
        "bigint", "binary", "blob", "bool", "boolean", "bytea", "char", "clob", "date",
        "datetime", "decimal", "double", "float", "int", "integer", "json", "jsonb",
        "numeric", "real", "serial", "smallint", "text", "timestamp", "varchar"
    });
    keel::StaticArray<keel::StringRef, 20> sqlBuiltins = MakeSortedTokens<20>({
        "abs", "avg", "cast", "coalesce", "concat", "count", "current_date",
        "current_timestamp", "dense_rank", "lag", "lead", "length", "lower", "max",
        "min", "now", "nullif", "rank", "row_number", "sum"
    });

    // --- 19. YAML / TOML / INI ---
    keel::StaticArray<keel::StringRef, 12> yamlKeywords = MakeSortedTokens<12>({
        "FALSE", "False", "NO", "NULL", "No", "OFF", "ON", "TRUE", "True", "YES",
        "false", "true"
    });
    keel::StaticArray<keel::StringRef, 6> yamlTypes = MakeSortedTokens<6>({
        "no", "null", "off", "on", "yes", "~"
    });

    // --- 20. CSS / SCSS ---
    keel::StaticArray<keel::StringRef, 42> cssKeywords = MakeSortedTokens<42>({
        "absolute", "auto", "baseline", "block", "bold", "border-box", "both", "center",
        "column", "content-box", "cover", "currentColor", "dashed", "dotted", "ease",
        "ease-in", "ease-in-out", "ease-out", "end", "fixed", "flex", "flex-end",
        "flex-start", "grid", "hidden", "important", "inherit", "initial", "inline",
        "inline-block", "inline-flex", "linear", "none", "normal", "nowrap", "pointer",
        "relative", "row", "scroll", "solid", "space-between", "sticky"
    });
    keel::StaticArray<keel::StringRef, 52> cssTypes = MakeSortedTokens<52>({
        "align-items", "animation", "aspect-ratio", "backdrop-filter", "background",
        "background-color", "background-image", "border", "border-bottom",
        "border-color", "border-left", "border-radius", "border-right", "border-top",
        "bottom", "box-shadow", "box-sizing", "color", "cursor", "display", "fill",
        "filter", "flex-direction", "flex-grow", "flex-wrap", "font-family",
        "font-size", "font-weight", "gap", "grid-template-columns",
        "grid-template-rows", "height", "justify-content", "left", "line-height",
        "margin", "max-height", "max-width", "min-height", "min-width", "opacity",
        "overflow", "padding", "position", "right", "stroke", "text-align", "top",
        "transform", "transition", "width", "z-index"
    });
    keel::StaticArray<keel::StringRef, 22> cssBuiltins = MakeSortedTokens<22>({
        "attr", "blur", "calc", "clamp", "color-mix", "conic-gradient", "cubic-bezier",
        "env", "hsl", "hsla", "linear-gradient", "max", "min", "minmax", "oklch",
        "radial-gradient", "repeat", "rgb", "rgba", "rotate", "scale", "var"
    });
};

[[nodiscard]] constexpr bool VerifyTokenBank(const BuiltInTokenBank& b) noexcept {
    return VerifyStrictlySorted(b.cppKeywords) &&
           VerifyStrictlySorted(b.cppTypes) &&
           VerifyStrictlySorted(b.matlabKeywords) &&
           VerifyStrictlySorted(b.matlabTypes) &&
           VerifyStrictlySorted(b.matlabBuiltins) &&
           VerifyStrictlySorted(b.matlabFoldOpen) &&
           VerifyStrictlySorted(b.matlabFoldClose) &&
           VerifyStrictlySorted(b.jsonKeywords) &&
           VerifyStrictlySorted(b.pythonKeywords) &&
           VerifyStrictlySorted(b.pythonTypes) &&
           VerifyStrictlySorted(b.pythonBuiltins) &&
           VerifyStrictlySorted(b.rustKeywords) &&
           VerifyStrictlySorted(b.rustTypes) &&
           VerifyStrictlySorted(b.rustBuiltins) &&
           VerifyStrictlySorted(b.tsKeywords) &&
           VerifyStrictlySorted(b.tsTypes) &&
           VerifyStrictlySorted(b.jsKeywords) &&
           VerifyStrictlySorted(b.jsBuiltins) &&
           VerifyStrictlySorted(b.glslKeywords) &&
           VerifyStrictlySorted(b.glslTypes) &&
           VerifyStrictlySorted(b.glslBuiltins) &&
           VerifyStrictlySorted(b.goKeywords) &&
           VerifyStrictlySorted(b.goTypes) &&
           VerifyStrictlySorted(b.goBuiltins) &&
           VerifyStrictlySorted(b.csharpKeywords) &&
           VerifyStrictlySorted(b.csharpTypes) &&
           VerifyStrictlySorted(b.csharpBuiltins) &&
           VerifyStrictlySorted(b.javaKeywords) &&
           VerifyStrictlySorted(b.javaTypes) &&
           VerifyStrictlySorted(b.javaBuiltins) &&
           VerifyStrictlySorted(b.kotlinKeywords) &&
           VerifyStrictlySorted(b.kotlinTypes) &&
           VerifyStrictlySorted(b.kotlinBuiltins) &&
           VerifyStrictlySorted(b.swiftKeywords) &&
           VerifyStrictlySorted(b.swiftTypes) &&
           VerifyStrictlySorted(b.swiftBuiltins) &&
           VerifyStrictlySorted(b.verilogKeywords) &&
           VerifyStrictlySorted(b.verilogTypes) &&
           VerifyStrictlySorted(b.verilogBuiltins) &&
           VerifyStrictlySorted(b.verilogFoldOpen) &&
           VerifyStrictlySorted(b.verilogFoldClose) &&
           VerifyStrictlySortedLowercase(b.vhdlKeywords) &&
           VerifyStrictlySortedLowercase(b.vhdlTypes) &&
           VerifyStrictlySortedLowercase(b.vhdlBuiltins) &&
           VerifyStrictlySortedLowercase(b.vhdlFoldOpen) &&
           VerifyStrictlySortedLowercase(b.vhdlFoldClose) &&
           VerifyStrictlySorted(b.luaKeywords) &&
           VerifyStrictlySorted(b.luaBuiltins) &&
           VerifyStrictlySorted(b.luaFoldOpen) &&
           VerifyStrictlySorted(b.luaFoldClose) &&
           VerifyStrictlySortedLowercase(b.cmakeKeywords) &&
           VerifyStrictlySortedLowercase(b.cmakeTypes) &&
           VerifyStrictlySortedLowercase(b.cmakeBuiltins) &&
           VerifyStrictlySortedLowercase(b.cmakeFoldOpen) &&
           VerifyStrictlySortedLowercase(b.cmakeFoldClose) &&
           VerifyStrictlySorted(b.shellKeywords) &&
           VerifyStrictlySorted(b.shellBuiltins) &&
           VerifyStrictlySorted(b.shellFoldOpen) &&
           VerifyStrictlySorted(b.shellFoldClose) &&
           VerifyStrictlySortedLowercase(b.psKeywords) &&
           VerifyStrictlySortedLowercase(b.psTypes) &&
           VerifyStrictlySortedLowercase(b.psBuiltins) &&
           VerifyStrictlySortedLowercase(b.batchKeywords) &&
           VerifyStrictlySortedLowercase(b.batchTypes) &&
           VerifyStrictlySortedLowercase(b.batchBuiltins) &&
           VerifyStrictlySortedLowercase(b.batchFoldOpen) &&
           VerifyStrictlySortedLowercase(b.batchFoldClose) &&
           VerifyStrictlySortedLowercase(b.sqlKeywords) &&
           VerifyStrictlySortedLowercase(b.sqlTypes) &&
           VerifyStrictlySortedLowercase(b.sqlBuiltins) &&
           VerifyStrictlySorted(b.yamlKeywords) &&
           VerifyStrictlySorted(b.yamlTypes) &&
           VerifyStrictlySorted(b.cssKeywords) &&
           VerifyStrictlySorted(b.cssTypes) &&
           VerifyStrictlySorted(b.cssBuiltins);
}

static constexpr BuiltInTokenBank kTokenBank{};
static_assert(VerifyTokenBank(kTokenBank), "Every built-in language dictionary must be strictly sorted with no duplicates");

[[nodiscard]] bool TryResolveLanguageTag(keel::StringRef lower, SyntaxConfig& out) noexcept {
    if (lower == "plain" || lower == "text" || lower == "txt" || lower == "none") {
        out = SyntaxConfig::Plain();
        return true;
    }
    if (lower == "c" || lower == "h" || lower == "cpp" || lower == "cc" || lower == "cxx" ||
        lower == "hpp" || lower == "hxx" || lower == "hh" || lower == "inl" || lower == "c++" ||
        lower == "cu" || lower == "cuh") {
        out = SyntaxConfig::Cpp();
        return true;
    }
    if (lower == "matlab" || lower == "m" || lower == "octave") {
        out = SyntaxConfig::Matlab();
        return true;
    }
    if (lower == "json" || lower == "jsonc" || lower == "geojson" || lower == "arb") {
        out = SyntaxConfig::Json();
        return true;
    }
    if (lower == "py" || lower == "pyw" || lower == "pyi" || lower == "python" || lower == "python3") {
        out = SyntaxConfig::Python();
        return true;
    }
    if (lower == "rs" || lower == "rust") {
        out = SyntaxConfig::Rust();
        return true;
    }
    if (lower == "ts" || lower == "tsx" || lower == "mts" || lower == "cts" || lower == "typescript") {
        out = SyntaxConfig::TypeScript();
        return true;
    }
    if (lower == "js" || lower == "jsx" || lower == "mjs" || lower == "cjs" || lower == "javascript") {
        out = SyntaxConfig::JavaScript();
        return true;
    }
    if (lower == "glsl" || lower == "vert" || lower == "frag" || lower == "comp" ||
        lower == "geom" || lower == "tesc" || lower == "tese" || lower == "hlsl" ||
        lower == "wgsl" || lower == "shader") {
        out = SyntaxConfig::Glsl();
        return true;
    }
    if (lower == "go" || lower == "golang") {
        out = SyntaxConfig::Go();
        return true;
    }
    if (lower == "cs" || lower == "csx" || lower == "csharp" || lower == "c#") {
        out = SyntaxConfig::CSharp();
        return true;
    }
    if (lower == "java") {
        out = SyntaxConfig::Java();
        return true;
    }
    if (lower == "kt" || lower == "kts" || lower == "kotlin") {
        out = SyntaxConfig::Kotlin();
        return true;
    }
    if (lower == "swift") {
        out = SyntaxConfig::Swift();
        return true;
    }
    if (lower == "v" || lower == "vh" || lower == "sv" || lower == "svh" ||
        lower == "verilog" || lower == "systemverilog") {
        out = SyntaxConfig::Verilog();
        return true;
    }
    if (lower == "vhd" || lower == "vhdl") {
        out = SyntaxConfig::Vhdl();
        return true;
    }
    if (lower == "lua") {
        out = SyntaxConfig::Lua();
        return true;
    }
    if (lower == "cmake" || lower == "cmakelists" || lower == "cmakelists.txt") {
        out = SyntaxConfig::Cmake();
        return true;
    }
    if (lower == "sh" || lower == "bash" || lower == "zsh" || lower == "fish" ||
        lower == "shell" || lower == "console" || lower == "env") {
        out = SyntaxConfig::Shell();
        return true;
    }
    if (lower == "ps1" || lower == "psm1" || lower == "psd1" ||
        lower == "pwsh" || lower == "powershell") {
        out = SyntaxConfig::PowerShell();
        return true;
    }
    if (lower == "bat" || lower == "cmd" || lower == "batch" || lower == "dos") {
        out = SyntaxConfig::Batch();
        return true;
    }
    if (lower == "sql" || lower == "sqlite" || lower == "postgres" ||
        lower == "postgresql" || lower == "mysql" || lower == "plsql" || lower == "tsql") {
        out = SyntaxConfig::Sql();
        return true;
    }
    if (lower == "yaml" || lower == "yml" || lower == "toml" ||
        lower == "ini" || lower == "cfg" || lower == "conf") {
        out = SyntaxConfig::Yaml();
        return true;
    }
    if (lower == "css" || lower == "scss" || lower == "sass" || lower == "less") {
        out = SyntaxConfig::Css();
        return true;
    }
    if (lower == "xml" || lower == "html" || lower == "htm" || lower == "xhtml" ||
        lower == "svg" || lower == "bpmn" || lower == "plist" || lower == "xaml" ||
        lower == "csproj" || lower == "vcxproj" || lower == "manifest" || lower == "xsd") {
        out = SyntaxConfig::Xml();
        return true;
    }
    return false;
}

} // namespace

namespace detail::ta_impl {

bool IsBuiltInSortedTokenList(keel::ArrayRef<keel::StringRef> list) noexcept {
    if (list.empty()) return false;
    const keel::usize addr = reinterpret_cast<keel::usize>(list.data());
    const keel::usize bankStart = reinterpret_cast<keel::usize>(&kTokenBank);
    const keel::usize bankEnd = bankStart + sizeof(kTokenBank);
    return addr >= bankStart && addr < bankEnd;
}

} // namespace detail::ta_impl

// =============================================================================
// Public SyntaxConfig Built-in Language Presets
// =============================================================================

SyntaxConfig SyntaxConfig::Cpp() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "C++20",
        .keywords = kTokenBank.cppKeywords,
        .types = kTokenBank.cppTypes,
        .lineComment = "//",
        .sectionPrefix = "// ===",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '#',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true
    };
}

SyntaxConfig SyntaxConfig::Matlab() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "MATLAB",
        .keywords = kTokenBank.matlabKeywords,
        .types = kTokenBank.matlabTypes,
        .builtins = kTokenBank.matlabBuiltins,
        .foldOpenKeywords = kTokenBank.matlabFoldOpen,
        .foldCloseKeywords = kTokenBank.matlabFoldClose,
        .lineComment = "%",
        .sectionPrefix = "%%",
        .blockCommentStart = "%{",
        .blockCommentEnd = "%}",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .transposeApostrophe = true
    };
}

SyntaxConfig SyntaxConfig::Json() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "JSON",
        .keywords = kTokenBank.jsonKeywords,
        .lineComment = "//",
        .sectionPrefix = "",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = false,
        .highlightPunctuation = true,
        .highlightJsonKeys = true,
        .singleQuoteStrings = false
    };
}

SyntaxConfig SyntaxConfig::Python() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "Python",
        .keywords = kTokenBank.pythonKeywords,
        .types = kTokenBank.pythonTypes,
        .builtins = kTokenBank.pythonBuiltins,
        .lineComment = "#",
        .sectionPrefix = "# %%",
        .blockCommentStart = "\"\"\"",
        .blockCommentEnd = "\"\"\"",
        .preprocessorPrefix = '@',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true
    };
}

SyntaxConfig SyntaxConfig::Rust() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "Rust",
        .keywords = kTokenBank.rustKeywords,
        .types = kTokenBank.rustTypes,
        .builtins = kTokenBank.rustBuiltins,
        .lineComment = "//",
        .sectionPrefix = "// ===",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '#',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .rustLifetimes = true
    };
}

SyntaxConfig SyntaxConfig::TypeScript() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "TypeScript",
        .keywords = kTokenBank.tsKeywords,
        .types = kTokenBank.tsTypes,
        .builtins = kTokenBank.jsBuiltins,
        .lineComment = "//",
        .sectionPrefix = "// ===",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .backtickStrings = true
    };
}

SyntaxConfig SyntaxConfig::JavaScript() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "JavaScript",
        .keywords = kTokenBank.jsKeywords,
        .builtins = kTokenBank.jsBuiltins,
        .lineComment = "//",
        .sectionPrefix = "// ===",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .backtickStrings = true
    };
}

SyntaxConfig SyntaxConfig::Glsl() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "GLSL",
        .keywords = kTokenBank.glslKeywords,
        .types = kTokenBank.glslTypes,
        .builtins = kTokenBank.glslBuiltins,
        .lineComment = "//",
        .sectionPrefix = "// ===",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '#',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true
    };
}

SyntaxConfig SyntaxConfig::Go() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "Go",
        .keywords = kTokenBank.goKeywords,
        .types = kTokenBank.goTypes,
        .builtins = kTokenBank.goBuiltins,
        .lineComment = "//",
        .sectionPrefix = "// ===",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .backtickStrings = true
    };
}

SyntaxConfig SyntaxConfig::CSharp() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "C#",
        .keywords = kTokenBank.csharpKeywords,
        .types = kTokenBank.csharpTypes,
        .builtins = kTokenBank.csharpBuiltins,
        .lineComment = "//",
        .sectionPrefix = "#region",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '#',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true
    };
}

SyntaxConfig SyntaxConfig::Java() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "Java",
        .keywords = kTokenBank.javaKeywords,
        .types = kTokenBank.javaTypes,
        .builtins = kTokenBank.javaBuiltins,
        .lineComment = "//",
        .sectionPrefix = "// ===",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '@',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true
    };
}

SyntaxConfig SyntaxConfig::Kotlin() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "Kotlin",
        .keywords = kTokenBank.kotlinKeywords,
        .types = kTokenBank.kotlinTypes,
        .builtins = kTokenBank.kotlinBuiltins,
        .lineComment = "//",
        .sectionPrefix = "// ===",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '@',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true
    };
}

SyntaxConfig SyntaxConfig::Swift() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "Swift",
        .keywords = kTokenBank.swiftKeywords,
        .types = kTokenBank.swiftTypes,
        .builtins = kTokenBank.swiftBuiltins,
        .lineComment = "//",
        .sectionPrefix = "// MARK:",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '#',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .singleQuoteStrings = false
    };
}

SyntaxConfig SyntaxConfig::Verilog() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "SystemVerilog",
        .keywords = kTokenBank.verilogKeywords,
        .types = kTokenBank.verilogTypes,
        .builtins = kTokenBank.verilogBuiltins,
        .foldOpenKeywords = kTokenBank.verilogFoldOpen,
        .foldCloseKeywords = kTokenBank.verilogFoldClose,
        .lineComment = "//",
        .sectionPrefix = "// ===",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '`',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .singleQuoteStrings = false
    };
}

SyntaxConfig SyntaxConfig::Vhdl() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "VHDL",
        .keywords = kTokenBank.vhdlKeywords,
        .types = kTokenBank.vhdlTypes,
        .builtins = kTokenBank.vhdlBuiltins,
        .foldOpenKeywords = kTokenBank.vhdlFoldOpen,
        .foldCloseKeywords = kTokenBank.vhdlFoldClose,
        .lineComment = "--",
        .sectionPrefix = "-- ===",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .transposeApostrophe = true,
        .caseInsensitiveKeywords = true
    };
}

SyntaxConfig SyntaxConfig::Lua() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "Lua",
        .keywords = kTokenBank.luaKeywords,
        .builtins = kTokenBank.luaBuiltins,
        .foldOpenKeywords = kTokenBank.luaFoldOpen,
        .foldCloseKeywords = kTokenBank.luaFoldClose,
        .lineComment = "--",
        .sectionPrefix = "-- ===",
        .blockCommentStart = "--[[",
        .blockCommentEnd = "]]",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true
    };
}

SyntaxConfig SyntaxConfig::Cmake() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "CMake",
        .keywords = kTokenBank.cmakeKeywords,
        .types = kTokenBank.cmakeTypes,
        .builtins = kTokenBank.cmakeBuiltins,
        .foldOpenKeywords = kTokenBank.cmakeFoldOpen,
        .foldCloseKeywords = kTokenBank.cmakeFoldClose,
        .lineComment = "#",
        .sectionPrefix = "# ===",
        .blockCommentStart = "#[[",
        .blockCommentEnd = "]]",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .caseInsensitiveKeywords = true,
        .singleQuoteStrings = false
    };
}

SyntaxConfig SyntaxConfig::Shell() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "Shell",
        .keywords = kTokenBank.shellKeywords,
        .builtins = kTokenBank.shellBuiltins,
        .foldOpenKeywords = kTokenBank.shellFoldOpen,
        .foldCloseKeywords = kTokenBank.shellFoldClose,
        .lineComment = "#",
        .sectionPrefix = "# ===",
        .blockCommentStart = "",
        .blockCommentEnd = "",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .backtickStrings = true
    };
}

SyntaxConfig SyntaxConfig::PowerShell() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "PowerShell",
        .keywords = kTokenBank.psKeywords,
        .types = kTokenBank.psTypes,
        .builtins = kTokenBank.psBuiltins,
        .lineComment = "#",
        .sectionPrefix = "#region",
        .blockCommentStart = "<#",
        .blockCommentEnd = "#>",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .caseInsensitiveKeywords = true,
        .allowHyphenInIdentifiers = true
    };
}

SyntaxConfig SyntaxConfig::Batch() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "Batch",
        .keywords = kTokenBank.batchKeywords,
        .types = kTokenBank.batchTypes,
        .builtins = kTokenBank.batchBuiltins,
        .foldOpenKeywords = kTokenBank.batchFoldOpen,
        .foldCloseKeywords = kTokenBank.batchFoldClose,
        .lineComment = "::",
        .lineCommentKeyword = "rem",
        .sectionPrefix = ":: ===",
        .blockCommentStart = "",
        .blockCommentEnd = "",
        .preprocessorPrefix = '@',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = false,
        .highlightPunctuation = true,
        .caseInsensitiveKeywords = true,
        .singleQuoteStrings = false
    };
}

SyntaxConfig SyntaxConfig::Sql() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "SQL",
        .keywords = kTokenBank.sqlKeywords,
        .types = kTokenBank.sqlTypes,
        .builtins = kTokenBank.sqlBuiltins,
        .lineComment = "--",
        .sectionPrefix = "-- ===",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .caseInsensitiveKeywords = true
    };
}

SyntaxConfig SyntaxConfig::Yaml() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "YAML",
        .keywords = kTokenBank.yamlKeywords,
        .types = kTokenBank.yamlTypes,
        .lineComment = "#",
        .sectionPrefix = "# ===",
        .blockCommentStart = "",
        .blockCommentEnd = "",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = false,
        .highlightPunctuation = true,
        .allowHyphenInIdentifiers = true,
        .highlightUnquotedKeys = true
    };
}

SyntaxConfig SyntaxConfig::Css() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "CSS",
        .keywords = kTokenBank.cssKeywords,
        .types = kTokenBank.cssTypes,
        .builtins = kTokenBank.cssBuiltins,
        .lineComment = "//",
        .sectionPrefix = "/* ===",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '@',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true,
        .allowHyphenInIdentifiers = true,
        .highlightUnquotedKeys = true
    };
}

SyntaxConfig SyntaxConfig::Xml() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .languageName = "XML",
        .lineComment = "",
        .sectionPrefix = "",
        .blockCommentStart = "<!--",
        .blockCommentEnd = "-->",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = false,
        .highlightFunctions = false,
        .highlightPunctuation = true,
        .allowHyphenInIdentifiers = true,
        .highlightMarkupTags = true
    };
}

SyntaxConfig SyntaxConfig::ForLanguage(keel::StringRef langTag) noexcept {
    keel::SmallString<32> lower;
    lower.reserve(langTag.size());
    for (char c : langTag) {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            lower.push_back(detail::ta_impl::ToLowerAscii(c));
        }
    }
    SyntaxConfig resolved{};
    if (TryResolveLanguageTag(lower.as_ref(), resolved)) {
        return resolved;
    }
    return Cpp();
}

SyntaxConfig SyntaxConfig::ForFilename(keel::StringRef filenameOrPath) noexcept {
    if (filenameOrPath.empty()) return Plain();

    // Extract basename after last '/' or '\\'
    keel::usize baseStart = 0u;
    for (keel::usize i = 0u; i < filenameOrPath.size(); ++i) {
        if (filenameOrPath[i] == '/' || filenameOrPath[i] == '\\') {
            baseStart = i + 1u;
        }
    }
    const keel::StringRef base = filenameOrPath.substr(baseStart);
    if (base.empty()) return Plain();

    keel::SmallString<64> lowerBase;
    lowerBase.reserve(base.size());
    for (char c : base) {
        lowerBase.push_back(detail::ta_impl::ToLowerAscii(c));
    }

    // Special extensionless / compound filenames
    if (lowerBase == "cmakelists.txt") return Cmake();
    if (lowerBase == ".bashrc" || lowerBase == ".zshrc" || lowerBase == ".profile" || lowerBase == ".env") {
        return Shell();
    }

    // Extension after last '.'
    keel::usize dotPos = keel::StringRef::npos;
    for (keel::usize i = lowerBase.size(); i > 0u; --i) {
        if (lowerBase[i - 1u] == '.') {
            dotPos = i - 1u;
            break;
        }
    }
    if (dotPos == keel::StringRef::npos || dotPos + 1u >= lowerBase.size()) {
        return Plain();
    }
    const keel::StringRef ext = lowerBase.as_ref().substr(dotPos + 1u);
    SyntaxConfig resolved{};
    if (TryResolveLanguageTag(ext, resolved)) {
        return resolved;
    }
    return Plain();
}

} // namespace guikit
