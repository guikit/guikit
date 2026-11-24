#pragma once

#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "keel/string.h"

namespace guikit {

class Context;
class ICanvas;

/// Represents a styled span of characters within a single line for syntax highlighting.
struct TextSpan {
    keel::u32 startCol{0};   ///< Zero-based character offset within the line
    keel::u32 length{0};     ///< Number of characters in this token/span
    Color color{};          ///< Color for this span
};

/// Signature for syntax highlighter function: called only for currently visible lines.
using SyntaxHighlighter = void (*)(
    keel::StringRef lineText,
    keel::usize lineIndex,
    keel::Array<TextSpan>& outSpans,
    const UiTheme& theme
);

/// Configurable syntax highlighting rules and token dictionary.
/// Supports custom keywords, types, builtins, comments, strings, numbers, preprocessor directives,
/// section headers, MATLAB transpose disambiguation, JSON keys, code folding, and color palette overrides.
struct SyntaxConfig {
    bool enabled{false};                                        ///< Whether rule-based syntax highlighting is active
    keel::StringRef languageName{"Plain Text"};                 ///< Language display name in status bar
    keel::ArrayRef<keel::StringRef> keywords{};                 ///< Control flow / language keywords
    keel::ArrayRef<keel::StringRef> types{};                    ///< Built-in and library type identifiers
    keel::ArrayRef<keel::StringRef> builtins{};                 ///< Built-in standard library functions / constants
    keel::ArrayRef<keel::StringRef> foldOpenKeywords{};         ///< Block opening keywords (e.g. function, if, for in MATLAB)
    keel::ArrayRef<keel::StringRef> foldCloseKeywords{};        ///< Block closing keywords (e.g. end in MATLAB)
    keel::StringRef lineComment{"//"};                          ///< Single-line comment prefix (empty to disable)
    keel::StringRef sectionPrefix{};                            ///< Section header prefix (e.g. "%%" in MATLAB)
    keel::StringRef blockCommentStart{"/*"};                    ///< Block comment opening delimiter (e.g. "/*" or "%{")
    keel::StringRef blockCommentEnd{"*/"};                      ///< Block comment closing delimiter (e.g. "*/" or "%}")
    char preprocessorPrefix{'#'};                               ///< Preprocessor line prefix ('\0' to disable)
    bool highlightStrings{true};                                ///< Highlight "..." and '...' literals
    bool highlightNumbers{true};                                ///< Highlight numeric literals (42, 3.14f, 0xFF, 1e-6i)
    bool highlightFunctions{true};                              ///< Highlight identifiers followed by '('
    bool highlightPunctuation{true};                            ///< Highlight operators and punctuation
    bool transposeApostrophe{false};                            ///< Disambiguate MATLAB transpose A' from 'string' literal
    bool highlightJsonKeys{false};                              ///< Distinguish JSON object keys ("key":) from string values
    keel::Optional<SyntaxPalette> palette{keel::kNullOpt};      ///< Optional custom color palette override

    /// Built-in C / C++20 syntax preset.
    [[nodiscard]] static SyntaxConfig Cpp() noexcept;

    /// Built-in MATLAB / Octave DSP & scientific computing syntax preset.
    [[nodiscard]] static SyntaxConfig Matlab() noexcept;

    /// Built-in JSON / configuration syntax preset.
    [[nodiscard]] static SyntaxConfig Json() noexcept;

    /// Built-in Python 3 syntax preset.
    [[nodiscard]] static SyntaxConfig Python() noexcept;

    /// Resolves a Markdown / language identifier tag (e.g. "cpp", "matlab", "json", "py") to a SyntaxConfig preset.
    [[nodiscard]] static SyntaxConfig ForLanguage(keel::StringRef langTag) noexcept;

    /// Plain text (no syntax highlighting).
    [[nodiscard]] static constexpr SyntaxConfig Plain() noexcept { return SyntaxConfig{.enabled = false}; }
};

/// Severity level for inline code diagnostics (compiler/linter/runtime messages).
enum class DiagnosticSeverity : keel::u8 {
    Info = 0,
    Warning = 1,
    Error = 2
};

/// Represents an inline compiler, static analyzer, or runtime diagnostic marker (wavy underline + tooltip).
struct CodeDiagnostic {
    keel::usize line{1};                                             ///< 1-based line number
    keel::u32 startCol{0};                                       ///< 0-based start column (0 = first non-whitespace char)
    keel::u32 endCol{0};                                         ///< 0-based end column (0 = end of line)
    DiagnosticSeverity severity{DiagnosticSeverity::Error};     ///< Diagnostic severity (Error, Warning, Info)
    keel::StringRef message{};                                  ///< Diagnostic message displayed in hover tooltip & status bar
    keel::StringRef code{};                                     ///< Optional diagnostic identifier (e.g. "C2065", "MLINT")
    keel::StringRef anchor{};                                   ///< Optional source code substring anchor for automatic line/column tracking
};

/// Represents an inline debugger variable value annotation rendered at the end of a source line.
struct CodeInlineValue {
    keel::usize line{1};                                             ///< 1-based line number
    keel::StringRef text{};                                     ///< Inline readout text (e.g. "N = 1024 | snr = +24.8 dB")
    UiVariant variant{UiVariant::Info};                         ///< Semantic accent variant for the inline readout
    keel::StringRef anchor{};                                   ///< Optional source code substring anchor for automatic line tracking
};

/// Represents a live variable in the debugger scope for interactive hover DataTip inspection.
struct CodeDebugVar {
    keel::StringRef name{};                                     ///< Variable / symbol identifier in source code
    keel::StringRef value{};                                    ///< Live value text (e.g. "[64x1 complex double] peak = 1.00")
    keel::StringRef type{};                                     ///< Optional type description (e.g. "Eigen::VectorXcf")
};

/// Debugger control actions triggered via keyboard shortcuts (F5/F9/F10/F11) or floating debug toolbar.
enum class CodeDebugAction : keel::u8 {
    None = 0,
    Continue,           ///< F5 or Continue/Resume button
    StepOver,           ///< F10 or Step Over button
    StepInto,           ///< F11 or Step Into button
    StepOut,            ///< Shift+F11 or Step Out button
    Restart,            ///< Restart debugging session button
    Stop,               ///< Shift+F5 or Stop debugging button
    ToggleBreakpoint    ///< F9 or click in breakpoint gutter
};

/// Extended response returned by TextArea with live cursor, search, and debugger telemetry.
struct TextAreaResponse : Response {
    keel::usize cursorLine{1};                                       ///< 1-based current cursor line number
    keel::usize cursorColumn{1};                                     ///< 1-based current cursor column number
    keel::usize totalLines{1};                                       ///< Total number of lines in document
    keel::usize searchMatchCount{0};                                 ///< Total number of active Find matches
    keel::usize activeSearchMatch{0};                                ///< 1-based index of active Find match (0 if none)
    CodeDebugAction debugAction{CodeDebugAction::None};         ///< Debugger action triggered during this frame
    keel::usize toggledBreakpointLine{0};                            ///< 1-based line number where breakpoint was toggled (0 if none)
    keel::StringRef hoveredWord{};                              ///< Identifier token currently under the mouse cursor
    keel::i32 lineDelta{0};                                           ///< Net number of lines inserted (+N) or deleted (-N) during this frame
    keel::usize editLine{0};                                         ///< 1-based line number where line insertion/deletion occurred (0 if none)
};

/// Clean configuration options for TextArea / Code Editor widget.
struct TextAreaOptions {
    UiText placeholder{};                               ///< Placeholder text when empty
    keel::Optional<keel::u32> rows{keel::kNullOpt};      ///< Visible height expressed in rows/lines
    bool autoHeight{false};                             ///< Automatically fit height to content line count
    bool readOnly{false};                               ///< Non-editable display mode (still allows selection & copy)
    bool showLineNumbers{true};                         ///< Toggleable line numbers gutter (1, 2, 3...)
    bool showCopyButton{false};                         ///< Optional quick Copy action button in top-right corner
    bool monospace{true};                               ///< Monospace typography (FontStyle::Monospace / CodeMedium)
    bool wordWrap{false};                               ///< Wrap long lines to viewport width (default false for code)
    keel::u8 tabSize{4};                                 ///< Visual tab width and indentation step in spaces (1..16)
    bool insertSpaces{true};                            ///< Insert spaces instead of '\t' on Tab / smart indent
    bool allowZoom{true};                               ///< Allow interactive Ctrl + Mouse Wheel zoom (50%..300%)
    keel::f32 zoom{1.0f};                                   ///< Base editor zoom factor (1.0f = 100%)
    keel::Optional<SyntaxConfig> syntax{keel::kNullOpt};      ///< Configurable rule-based syntax highlighting
    keel::Optional<SyntaxPalette> syntaxPalette{keel::kNullOpt}; ///< Optional syntax color palette override
    SyntaxHighlighter syntaxHighlighter{nullptr};       ///< Optional custom syntax highlighting callback
    keel::Optional<CursorBlinkMode> cursorMode{keel::kNullOpt}; ///< Caret visibility and blinking behavior
    UiText tooltip{};                                   ///< Optional hover tooltip text
    bool disabled{false};                               ///< Interaction suppression
    UiSize size{UiSize::Md};                            ///< Control size token (`Xs`..`Xl`, `Dp(...)`, `Factor(...)`)
    AxisSize width{};                                   ///< Horizontal sizing policy (`Auto` -> `Stretch`, `Fit`, or explicit `dp`)
    AxisSize height{};                                  ///< Vertical sizing policy (`Auto` -> `Fit`, `Stretch`, or explicit `dp`)
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};     ///< Optional minimum width in logical dp
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};     ///< Optional maximum width in logical dp
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};    ///< Optional minimum height in logical dp
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};    ///< Optional maximum height in logical dp
    StateColor color{};                                 ///< Per-state focus/accent color override
    StateColor background{};                            ///< Optional background fill override (`{}` = theme default, `StateColor::None()` = transparent)
    Border border{};                                    ///< Optional border width, color, and radius override

    // --- IDE & Code Intelligence Features ---
    bool showMinimap{false};                            ///< Render syntax-colored code overview minimap on the right
    bool showStatusBar{false};                          ///< Render bottom status bar (Ln/Col, Language, UTF-8, Debugger)
    bool showIndentGuides{true};                        ///< Render vertical indentation guides every 4 spaces
    bool showWhitespace{false};                         ///< Render visible whitespace markers (spaces ·, tabs →, line endings ↵)
    bool codeFolding{false};                            ///< Enable interactive block folding chevrons in the gutter
    bool autoCloseBrackets{true};                       ///< Automatically close (), [], {}, "", '' and smart-indent on Enter
    bool enableSearch{true};                            ///< Enable interactive Ctrl+F / Ctrl+H / Ctrl+G Find & Replace bar
    bool highlightOccurrences{true};                    ///< Highlight all occurrences of word under cursor or selection
    bool matchBrackets{true};                           ///< Highlight matching () [] {} brackets around the cursor

    // --- Debugger & Diagnostics Interface ---
    bool enableBreakpoints{false};                      ///< Enable gutter breakpoint toggling even if breakpoints == nullptr
    IndexListSink breakpoints{};                        ///< Optional external 1-based breakpoint line numbers
    keel::usize executionLine{0};                            ///< Active debugger instruction pointer (1-based line, 0 = none)
    keel::usize* executionLinePtr{nullptr};                  ///< Optional pointer to external executionLine shifted automatically on edits
    bool showDebugToolbar{false};                       ///< Show floating debugger toolbar (Continue, Step Over/Into/Out, Stop)
    Align debugToolbarAlign{Align::Center};             ///< Horizontal placement of floating debug toolbar (Start=Left, Center, End=Right)
    keel::Optional<UiSize> toolbarSize{keel::kNullOpt}; ///< Optional UiSize override for floating debug/search/action toolbars (nullopt = inherit editor size)
    keel::ArrayRef<CodeDiagnostic> diagnostics{};       ///< Inline compiler/linter diagnostics (wavy squiggles + tooltips)
    keel::ArrayRef<CodeInlineValue> inlineValues{};     ///< Live debugger inline variable values at end of lines
    keel::ArrayRef<CodeDebugVar> debugVariables{};      ///< Live variables for hover DataTip inspection over identifiers
};

struct TextAreaLogicalLine {
    keel::usize startOffset{0};
    keel::usize length{0};
};

struct TextAreaVisualLine {
    keel::usize logicalLine{0};
    keel::usize startOffset{0}; ///< Global byte offset in text
    keel::usize startCol{0};    ///< Column byte offset within logical line
    keel::usize length{0};      ///< Byte length of this visual sub-line
    bool isFirstSubLine{true};
};

struct TextAreaFoldRegion {
    keel::usize startLine{0}; ///< 0-based start line index
    keel::usize endLine{0};   ///< 0-based inclusive end line index
};

/// Persistent interactive state for TextArea.
struct TextAreaState {
    keel::f32 scrollX{0.0f};
    keel::f32 scrollY{0.0f};
    keel::f32 zoom{1.0f};
    keel::f32 lastOptsZoom{1.0f};
    keel::f32 prevAppliedZoom{1.0f};
    keel::usize cursor{0};
    keel::usize selectionStart{0};
    keel::usize selectionEnd{0};
    keel::usize prevCursor{0};
    keel::usize prevTextSize{0};
    keel::usize prevLineCount{0};
    bool draggingText{false};
    bool draggingWord{false};
    keel::usize wordDragStart{0};
    keel::usize wordDragEnd{0};
    bool draggingVThumb{false};
    bool draggingHThumb{false};
    bool draggingMinimap{false};
    bool draggingDebugToolbar{false};
    keel::f32 dragGrabOffset{0.0f};
    keel::f32 debugToolbarGrabOffset{0.0f};
    keel::f32 debugToolbarRatio{-1.0f}; ///< Custom horizontal ratio [0..1] when dragged (-1 = use debugToolbarAlign)
    Align lastDebugToolbarAlign{Align::Center};
    keel::f64 lastClickTime{-10.0};
    Vec2 lastClickPos{-1000.0f, -1000.0f};
    keel::i32 clickCount{0};
    keel::f64 copyFeedbackUntil{-1.0};

    // Find & Replace / Go to Line state
    bool searchOpen{false};
    bool replaceOpen{false};
    bool gotoOpen{false};
    keel::i32 overlayFocus{0}; ///< 0 = Code Editor, 1 = Find input, 2 = Replace input, 3 = GoToLine input
    keel::String searchQuery{};
    keel::String replaceQuery{};
    keel::String gotoQuery{};
    bool matchCase{false};
    bool wholeWord{false};
    keel::usize activeMatchIdx{0};
    keel::usize searchMatchCount{0};

    // Code folding & debugger state
    keel::Array<keel::usize> foldedLines{};         ///< 0-based start line indices of folded blocks
    keel::Array<keel::usize> internalBreakpoints{}; ///< 1-based breakpoint lines when opts.breakpoints == nullptr
    keel::usize prevExecutionLine{0};

    // Line-shift & statement-anchor tracking state
    bool baselineInitialized{false};
    keel::String prevDocumentText{};
    keel::Array<keel::usize> baselineToCurrentLine{};
    keel::Array<keel::String> baselineSignatures{};
    keel::Array<CodeInlineValue> resolvedInlineValues{};
    keel::Array<CodeDiagnostic> resolvedDiagnostics{};

    // Cached line / fold / visual layout buffers (avoids per-frame heap allocations & static re-parsing)
    keel::u64 cachedDocHash{0};
    keel::usize cachedDocSize{keel::as<keel::usize>(-1)};
    bool cachedFoldEnabled{false};
    keel::u64 cachedFoldSyntaxHash{0};
    keel::Array<TextAreaLogicalLine> cachedLogicalLines{};
    keel::Array<TextAreaFoldRegion> cachedFoldRegions{};
    keel::Array<TextAreaVisualLine> scratchVisualLines{};

    void TrimInactiveCaches() noexcept {
        baselineInitialized = false;
        prevDocumentText = {};
        baselineToCurrentLine = {};
        baselineSignatures = {};
        resolvedInlineValues = {};
        resolvedDiagnostics = {};
        cachedDocHash = 0;
        cachedDocSize = keel::as<keel::usize>(-1);
        cachedFoldSyntaxHash = 0;
        cachedLogicalLines = {};
        cachedFoldRegions = {};
        scratchVisualLines = {};
    }
};

/// Internal runtime state allocated in frame arena for TextArea nodes.
struct TextAreaData {
    TextAreaOptions opts{};
    keel::StringRef text{};
    keel::usize cursor{0};
    keel::usize selectionStart{0};
    keel::usize selectionEnd{0};
    bool cursorVisible{true};
    keel::f32 cursorWidth{1.8f};
    keel::f32 scrollX{0.0f};
    keel::f32 scrollY{0.0f};
    keel::f32 zoom{1.0f};
    bool copyHovered{false};
    bool copyFeedbackActive{false};
    bool searchBtnHovered{false};
    bool zoomBadgeHovered{false};
    bool vThumbHovered{false};
    bool vThumbDragging{false};
    bool hThumbHovered{false};
    bool hThumbDragging{false};
    bool minimapHovered{false};
    bool minimapDragging{false};
    bool debugToolbarDragging{false};
    keel::f32 debugToolbarRatio{-1.0f};

    // Search & Replace overlay snapshot
    bool searchOpen{false};
    bool replaceOpen{false};
    bool gotoOpen{false};
    keel::i32 overlayFocus{0};
    keel::StringRef searchQuery{};
    keel::StringRef replaceQuery{};
    keel::StringRef gotoQuery{};
    bool matchCase{false};
    bool wholeWord{false};
    keel::usize activeMatchIdx{0};
    keel::usize searchMatchCount{0};
    keel::i32 hoveredOverlayBtn{-1};

    // Folding & Debugger snapshot
    keel::ArrayRef<keel::usize> foldedLines{};
    keel::ArrayRef<keel::usize> breakpoints{};
    keel::ArrayRef<TextAreaLogicalLine> precomputedLogicalLines{};
    keel::ArrayRef<TextAreaFoldRegion> precomputedFoldRegions{};
    keel::i32 hoveredGutterLine{-1};       ///< 0-based line hovered in breakpoint gutter
    keel::i32 hoveredFoldLine{-1};         ///< 0-based line hovered in folding chevron area
    keel::i32 hoveredDebugBtn{-1};         ///< Hovered button index in floating debug toolbar (0..5 = buttons, 98 = grip)
    keel::StringRef hoveredWord{};   ///< Identifier currently under mouse cursor
    keel::usize hoveredWordLine{0};       ///< 0-based line of hoveredWord
    keel::usize hoveredWordCol{0};        ///< 0-based start col of hoveredWord
};

/// Autonomous high-performance TextArea / Code Editor widget logic (execution, measuring, and virtualized rendering).
namespace detail {

class TextArea {
public:
    /// Primary immediate mode entry point for TextArea (editable or read-only).
    static TextAreaResponse Run(
        Context& ctx,
        keel::Optional<keel::StringSink> stringValue,
        keel::StringRef displayFallback,
        const TextAreaOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures intrinsic dimensions based on content lines, rows, autoHeight, and external Element labels.
    static Vec2 Measure(
        keel::StringRef text,
        const TextAreaOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        keel::f32 availableWidth = 0.0f
    );
    static Vec2 Measure(
        keel::StringRef text,
        const TextAreaOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Renders the external Element labels, editor frame, gutter, folding, breakpoints, minimap, search bar, debug bar, and syntax spans.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        keel::StringRef text,
        const TextAreaOptions& opts,
        UiSize size,
        bool hovered = false,
        bool focused = false,
        keel::usize cursor = 0,
        bool disabled = false,
        bool cursorVisible = true,
        keel::f32 scrollY = 0.0f,
        const UiTheme& theme = UiTheme::Dark(),
        const TextAreaData* extData = nullptr
    );

    /// Configurable rule-based line highlighter emitting contiguous styled spans covering lineText.
    /// Optional inOutBlockComment allows stateful multi-line block comment tracking across lines.
    static void HighlightLine(
        keel::StringRef lineText,
        keel::usize lineIndex,
        const SyntaxConfig& config,
        keel::Array<TextSpan>& outSpans,
        const UiTheme& theme,
        const SyntaxPalette* paletteOverride = nullptr,
        bool* inOutBlockComment = nullptr
    );

    /// Built-in high-performance C++ syntax highlighter for code snippets and editors.
    static void CppHighlighter(
        keel::StringRef lineText,
        keel::usize lineIndex,
        keel::Array<TextSpan>& outSpans,
        const UiTheme& theme
    );
};

} // namespace detail

using CodeEditorOptions = TextAreaOptions;
using CodeEditorResponse = TextAreaResponse;

template <keel::WritableString S>
TextAreaResponse TextArea(
    Context& ui,
    S& value,
    const TextAreaOptions& opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::TextArea::Run(ui, keel::Optional<keel::StringSink>(keel::StringSink(value)), keel::StringRef{}, opts, loc);
}

inline TextAreaResponse TextArea(
    Context& ui,
    keel::StringRef value,
    const TextAreaOptions& opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    return detail::TextArea::Run(ui, keel::Optional<keel::StringSink>{}, value, opts, loc);
}

template <keel::WritableString S>
CodeEditorResponse CodeEditor(
    Context& ui,
    S& value,
    const CodeEditorOptions& opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return guikit::TextArea(ui, value, opts, loc);
}

inline CodeEditorResponse CodeEditor(
    Context& ui,
    keel::StringRef value,
    const CodeEditorOptions& opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return guikit::TextArea(ui, value, opts, loc);
}

} // namespace guikit
