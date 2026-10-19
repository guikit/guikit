#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Represents a styled span of characters within a single line for syntax highlighting.
struct TextSpan {
    uint32_t startCol{0};   ///< Zero-based character offset within the line
    uint32_t length{0};     ///< Number of characters in this token/span
    Color color{};          ///< Color for this span
};

/// Signature for syntax highlighter function: called only for currently visible lines.
using SyntaxHighlighter = void (*)(
    std::string_view lineText,
    size_t lineIndex,
    std::vector<TextSpan>& outSpans,
    const UiTheme& theme,
    void* userData
);

/// Configurable syntax highlighting rules and token dictionary.
/// Supports custom keywords, types, builtins, comments, strings, numbers, preprocessor directives,
/// section headers, MATLAB transpose disambiguation, JSON keys, code folding, and color palette overrides.
struct SyntaxConfig {
    bool enabled{false};                                        ///< Whether rule-based syntax highlighting is active
    std::string_view languageName{"Plain Text"};                ///< Language display name in status bar
    std::span<const std::string_view> keywords{};               ///< Control flow / language keywords
    std::span<const std::string_view> types{};                  ///< Built-in and library type identifiers
    std::span<const std::string_view> builtins{};               ///< Built-in standard library functions / constants
    std::span<const std::string_view> foldOpenKeywords{};       ///< Block opening keywords (e.g. function, if, for in MATLAB)
    std::span<const std::string_view> foldCloseKeywords{};      ///< Block closing keywords (e.g. end in MATLAB)
    std::string_view lineComment{"//"};                         ///< Single-line comment prefix (empty to disable)
    std::string_view sectionPrefix{};                           ///< Section header prefix (e.g. "%%" in MATLAB)
    std::string_view blockCommentStart{"/*"};                   ///< Block comment opening delimiter (e.g. "/*" or "%{")
    std::string_view blockCommentEnd{"*/"};                     ///< Block comment closing delimiter (e.g. "*/" or "%}")
    char preprocessorPrefix{'#'};                               ///< Preprocessor line prefix ('\0' to disable)
    bool highlightStrings{true};                                ///< Highlight "..." and '...' literals
    bool highlightNumbers{true};                                ///< Highlight numeric literals (42, 3.14f, 0xFF, 1e-6i)
    bool highlightFunctions{true};                              ///< Highlight identifiers followed by '('
    bool highlightPunctuation{true};                            ///< Highlight operators and punctuation
    bool transposeApostrophe{false};                            ///< Disambiguate MATLAB transpose A' from 'string' literal
    bool highlightJsonKeys{false};                              ///< Distinguish JSON object keys ("key":) from string values
    std::optional<SyntaxPalette> palette{std::nullopt};         ///< Optional custom color palette override

    /// Built-in C / C++20 syntax preset.
    [[nodiscard]] static SyntaxConfig Cpp() noexcept;

    /// Built-in MATLAB / Octave DSP & scientific computing syntax preset.
    [[nodiscard]] static SyntaxConfig Matlab() noexcept;

    /// Built-in JSON / configuration syntax preset.
    [[nodiscard]] static SyntaxConfig Json() noexcept;

    /// Built-in Python 3 syntax preset.
    [[nodiscard]] static SyntaxConfig Python() noexcept;

    /// Resolves a Markdown / language identifier tag (e.g. "cpp", "matlab", "json", "py") to a SyntaxConfig preset.
    [[nodiscard]] static SyntaxConfig ForLanguage(std::string_view langTag) noexcept;

    /// Plain text (no syntax highlighting).
    [[nodiscard]] static constexpr SyntaxConfig Plain() noexcept { return SyntaxConfig{.enabled = false}; }
};

/// Severity level for inline code diagnostics (compiler/linter/runtime messages).
enum class DiagnosticSeverity : uint8_t {
    Info = 0,
    Warning = 1,
    Error = 2
};

/// Represents an inline compiler, static analyzer, or runtime diagnostic marker (wavy underline + tooltip).
struct CodeDiagnostic {
    size_t line{1};                                             ///< 1-based line number
    uint32_t startCol{0};                                       ///< 0-based start column (0 = first non-whitespace char)
    uint32_t endCol{0};                                         ///< 0-based end column (0 = end of line)
    DiagnosticSeverity severity{DiagnosticSeverity::Error};     ///< Diagnostic severity (Error, Warning, Info)
    std::string_view message{};                                 ///< Diagnostic message displayed in hover tooltip & status bar
    std::string_view code{};                                    ///< Optional diagnostic identifier (e.g. "C2065", "MLINT")
    std::string_view anchor{};                                  ///< Optional source code substring anchor for automatic line/column tracking
};

/// Represents an inline debugger variable value annotation rendered at the end of a source line.
struct CodeInlineValue {
    size_t line{1};                                             ///< 1-based line number
    std::string_view text{};                                    ///< Inline readout text (e.g. "N = 1024 | snr = +24.8 dB")
    UiVariant variant{UiVariant::Info};                         ///< Semantic accent variant for the inline readout
    std::string_view anchor{};                                  ///< Optional source code substring anchor for automatic line tracking
};

/// Represents a live variable in the debugger scope for interactive hover DataTip inspection.
struct CodeDebugVar {
    std::string_view name{};                                    ///< Variable / symbol identifier in source code
    std::string_view value{};                                   ///< Live value text (e.g. "[64x1 complex double] peak = 1.00")
    std::string_view type{};                                    ///< Optional type description (e.g. "Eigen::VectorXcf")
};

/// Debugger control actions triggered via keyboard shortcuts (F5/F9/F10/F11) or floating debug toolbar.
enum class CodeDebugAction : uint8_t {
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
    size_t cursorLine{1};                                       ///< 1-based current cursor line number
    size_t cursorColumn{1};                                     ///< 1-based current cursor column number
    size_t totalLines{1};                                       ///< Total number of lines in document
    size_t searchMatchCount{0};                                 ///< Total number of active Find matches
    size_t activeSearchMatch{0};                                ///< 1-based index of active Find match (0 if none)
    CodeDebugAction debugAction{CodeDebugAction::None};         ///< Debugger action triggered during this frame
    size_t toggledBreakpointLine{0};                            ///< 1-based line number where breakpoint was toggled (0 if none)
    std::string_view hoveredWord{};                             ///< Identifier token currently under the mouse cursor
    int lineDelta{0};                                           ///< Net number of lines inserted (+N) or deleted (-N) during this frame
    size_t editLine{0};                                         ///< 1-based line number where line insertion/deletion occurred (0 if none)
};

/// Clean configuration options for TextArea / Code Editor widget.
struct TextAreaOptions {
    UiText label{};                                     ///< Optional external caption text above/beside area
    UiText description{};                               ///< Optional secondary description / helper text
    Icon icon{};                                        ///< Optional leading icon in the external header
    UiText badge{};                                     ///< Optional badge pill in the external header
    std::string_view placeholder{};                     ///< Placeholder text when empty
    std::optional<uint32_t> rows{std::nullopt};         ///< Visible height expressed in rows/lines
    std::optional<float> fixedHeight{std::nullopt};     ///< Explicit height of the editor box in points/pixels
    bool autoHeight{false};                             ///< Automatically fit height to content line count
    bool readOnly{false};                               ///< Non-editable display mode (still allows selection & copy)
    bool showLineNumbers{true};                         ///< Toggleable line numbers gutter (1, 2, 3...)
    bool showCopyButton{false};                         ///< Optional quick Copy action button in top-right corner
    bool monospace{true};                               ///< Monospace typography (FontStyle::Monospace / CodeMedium)
    bool wordWrap{false};                               ///< Wrap long lines to viewport width (default false for code)
    bool allowZoom{true};                               ///< Allow interactive Ctrl + Mouse Wheel zoom (50%..300%)
    float zoom{1.0f};                                   ///< Base editor zoom factor (1.0f = 100%)
    std::optional<SyntaxConfig> syntax{std::nullopt};   ///< Configurable rule-based syntax highlighting
    std::optional<SyntaxPalette> syntaxPalette{std::nullopt}; ///< Optional syntax color palette override
    SyntaxHighlighter syntaxHighlighter{nullptr};       ///< Optional custom syntax highlighting callback
    void* highlighterUserData{nullptr};                 ///< User data passed to syntaxHighlighter
    std::optional<CursorBlinkMode> cursorMode{std::nullopt}; ///< Caret visibility and blinking behavior
    UiVariant variant{UiVariant::Default};              ///< Visual variant styling
    ElementLayout layout{ElementLayout::Stacked};       ///< Placement of external label and description
    InlineOrder inlineOrder{InlineOrder::LabelFirst};   ///< Horizontal ordering when layout == ElementLayout::Inline
    Align align{Align::Start};                          ///< Horizontal alignment of external label and description
    StretchMode stretch{StretchMode::Fill};             ///< Horizontal sizing behavior (Compact, Space, Fill)
    std::optional<float> width{std::nullopt};           ///< Explicit width (dp in Compact/Space, ratio 0.0..1.0 in Fill)
    SizeMode heightMode{SizeMode::Fit};                 ///< Vertical sizing behavior in layout (Fit or Stretch)
    std::optional<float> borderRadius{std::nullopt};    ///< Optional corner radius override
    std::optional<float> scale{std::nullopt};           ///< Optional local UI scale factor override
    const UiTheme* theme{nullptr};                      ///< Optional local theme override
    std::string_view tooltip{};                         ///< Optional hover tooltip text
    bool disabled{false};                               ///< Interaction suppression

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
    std::vector<size_t>* breakpoints{nullptr};          ///< Optional external 1-based breakpoint line numbers
    size_t executionLine{0};                            ///< Active debugger instruction pointer (1-based line, 0 = none)
    size_t* executionLinePtr{nullptr};                  ///< Optional pointer to external executionLine shifted automatically on edits
    bool showDebugToolbar{false};                       ///< Show floating debugger toolbar (Continue, Step Over/Into/Out, Stop)
    Align debugToolbarAlign{Align::Center};             ///< Horizontal placement of floating debug toolbar (Start=Left, Center, End=Right)
    std::optional<UiSize> toolbarSize{std::nullopt};    ///< Optional UiSize override for floating debug/search/action toolbars (nullopt = inherit editor size)
    std::span<const CodeDiagnostic> diagnostics{};      ///< Inline compiler/linter diagnostics (wavy squiggles + tooltips)
    std::span<const CodeInlineValue> inlineValues{};    ///< Live debugger inline variable values at end of lines
    std::span<const CodeDebugVar> debugVariables{};     ///< Live variables for hover DataTip inspection over identifiers
};

/// Persistent interactive state for TextArea.
struct TextAreaState {
    float scrollX{0.0f};
    float scrollY{0.0f};
    float zoom{1.0f};
    float lastOptsZoom{1.0f};
    float prevAppliedZoom{1.0f};
    size_t cursor{0};
    size_t selectionStart{0};
    size_t selectionEnd{0};
    size_t prevCursor{0};
    size_t prevTextSize{0};
    size_t prevLineCount{0};
    bool draggingText{false};
    bool draggingWord{false};
    size_t wordDragStart{0};
    size_t wordDragEnd{0};
    bool draggingVThumb{false};
    bool draggingHThumb{false};
    bool draggingMinimap{false};
    bool draggingDebugToolbar{false};
    float dragGrabOffset{0.0f};
    float debugToolbarGrabOffset{0.0f};
    float debugToolbarRatio{-1.0f}; ///< Custom horizontal ratio [0..1] when dragged (-1 = use debugToolbarAlign)
    Align lastDebugToolbarAlign{Align::Center};
    double lastClickTime{-10.0};
    Vec2 lastClickPos{-1000.0f, -1000.0f};
    int clickCount{0};
    double copyFeedbackUntil{-1.0};

    // Find & Replace / Go to Line state
    bool searchOpen{false};
    bool replaceOpen{false};
    bool gotoOpen{false};
    int overlayFocus{0}; ///< 0 = Code Editor, 1 = Find input, 2 = Replace input, 3 = GoToLine input
    std::string searchQuery{};
    std::string replaceQuery{};
    std::string gotoQuery{};
    bool matchCase{false};
    bool wholeWord{false};
    size_t activeMatchIdx{0};
    size_t searchMatchCount{0};

    // Code folding & debugger state
    std::vector<size_t> foldedLines{};       ///< 0-based start line indices of folded blocks
    std::vector<size_t> internalBreakpoints{}; ///< 1-based breakpoint lines when opts.breakpoints == nullptr
    size_t prevExecutionLine{0};

    // Line-shift & statement-anchor tracking state
    bool baselineInitialized{false};
    std::string prevDocumentText{};
    std::vector<size_t> baselineToCurrentLine{};
    std::vector<std::string> baselineSignatures{};
    std::vector<CodeInlineValue> resolvedInlineValues{};
    std::vector<CodeDiagnostic> resolvedDiagnostics{};
};

/// Internal runtime state allocated in frame arena for TextArea nodes.
struct TextAreaData {
    TextAreaOptions opts{};
    std::string_view text{};
    size_t cursor{0};
    size_t selectionStart{0};
    size_t selectionEnd{0};
    bool cursorVisible{true};
    float scrollX{0.0f};
    float scrollY{0.0f};
    float zoom{1.0f};
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
    float debugToolbarRatio{-1.0f};

    // Search & Replace overlay snapshot
    bool searchOpen{false};
    bool replaceOpen{false};
    bool gotoOpen{false};
    int overlayFocus{0};
    std::string_view searchQuery{};
    std::string_view replaceQuery{};
    std::string_view gotoQuery{};
    bool matchCase{false};
    bool wholeWord{false};
    size_t activeMatchIdx{0};
    size_t searchMatchCount{0};
    int hoveredOverlayBtn{-1};

    // Folding & Debugger snapshot
    std::span<const size_t> foldedLines{};
    std::span<const size_t> breakpoints{};
    int hoveredGutterLine{-1};      ///< 0-based line hovered in breakpoint gutter
    int hoveredFoldLine{-1};        ///< 0-based line hovered in folding chevron area
    int hoveredDebugBtn{-1};        ///< Hovered button index in floating debug toolbar (0..5 = buttons, 98 = grip)
    std::string_view hoveredWord{}; ///< Identifier currently under mouse cursor
    size_t hoveredWordLine{0};      ///< 0-based line of hoveredWord
    size_t hoveredWordCol{0};       ///< 0-based start col of hoveredWord
};

/// Autonomous high-performance TextArea / Code Editor widget logic (execution, measuring, and virtualized rendering).
class TextArea {
public:
    /// Primary immediate mode entry point for TextArea (editable or read-only).
    static TextAreaResponse Run(
        Context& ctx,
        std::string* stringValue,
        std::string_view displayFallback,
        const TextAreaOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures intrinsic dimensions based on content lines, rows, autoHeight, and external Element labels.
    static Vec2 Measure(
        std::string_view text,
        const TextAreaOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );
    static Vec2 Measure(
        std::string_view text,
        const TextAreaOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Renders the external Element labels, editor frame, gutter, folding, breakpoints, minimap, search bar, debug bar, and syntax spans.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        std::string_view text,
        const TextAreaOptions& opts,
        UiSize size,
        bool hovered = false,
        bool focused = false,
        size_t cursor = 0,
        bool disabled = false,
        bool cursorVisible = true,
        float scrollY = 0.0f,
        const UiTheme& theme = UiTheme::Dark(),
        const TextAreaData* extData = nullptr
    );

    /// Configurable rule-based line highlighter emitting contiguous styled spans covering lineText.
    /// Optional inOutBlockComment allows stateful multi-line block comment tracking across lines.
    static void HighlightLine(
        std::string_view lineText,
        size_t lineIndex,
        const SyntaxConfig& config,
        std::vector<TextSpan>& outSpans,
        const UiTheme& theme,
        const SyntaxPalette* paletteOverride = nullptr,
        bool* inOutBlockComment = nullptr
    );

    /// Built-in high-performance C++ syntax highlighter for code snippets and editors.
    static void CppHighlighter(
        std::string_view lineText,
        size_t lineIndex,
        std::vector<TextSpan>& outSpans,
        const UiTheme& theme,
        void* userData = nullptr
    );
};

} // namespace guikit
