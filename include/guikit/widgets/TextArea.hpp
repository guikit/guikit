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
/// Supports custom keywords, types, comments, strings, numbers, preprocessor directives,
/// function call detection, and optional color palette overrides.
struct SyntaxConfig {
    bool enabled{false};                                ///< Whether rule-based syntax highlighting is active
    std::span<const std::string_view> keywords{};       ///< Control flow / language keywords
    std::span<const std::string_view> types{};          ///< Built-in and library type identifiers
    std::string_view lineComment{"//"};                 ///< Single-line comment prefix (empty to disable)
    std::string_view blockCommentStart{"/*"};           ///< Block comment opening delimiter
    std::string_view blockCommentEnd{"*/"};             ///< Block comment closing delimiter
    char preprocessorPrefix{'#'};                       ///< Preprocessor line prefix ('\0' to disable)
    bool highlightStrings{true};                        ///< Highlight "..." and '...' literals
    bool highlightNumbers{true};                        ///< Highlight numeric literals (42, 3.14f, 0xFF)
    bool highlightFunctions{true};                      ///< Highlight identifiers followed by '('
    bool highlightPunctuation{true};                    ///< Highlight operators and punctuation
    std::optional<SyntaxPalette> palette{std::nullopt}; ///< Optional custom color palette override

    /// Built-in C / C++20 syntax preset.
    [[nodiscard]] static SyntaxConfig Cpp() noexcept;

    /// Built-in JSON / config syntax preset.
    [[nodiscard]] static SyntaxConfig Json() noexcept;

    /// Plain text (no syntax highlighting).
    [[nodiscard]] static constexpr SyntaxConfig Plain() noexcept { return SyntaxConfig{.enabled = false}; }
};

/// Clean configuration options for TextArea widget.
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
    Align elementAlign{Align::Start};                   ///< Horizontal alignment of external label and description
    StretchMode stretch{StretchMode::Fill};             ///< Horizontal sizing behavior (Compact, Space, Fill)
    std::optional<float> width{std::nullopt};           ///< Explicit width (dp in Compact/Space, 0..100% in Fill)
    SizeMode heightMode{SizeMode::Fit};                 ///< Vertical sizing behavior in layout (Fit or Stretch)
    bool divider{false};                                ///< Draw a structural divider line under header/widget
    ElementDivider dividerStyle{ElementDivider::Bottom};///< Divider placement (Bottom or Trailing)
    std::optional<float> borderRadius{std::nullopt};    ///< Optional corner radius override
    std::optional<UiSize> size{std::nullopt};           ///< Local size override
    std::optional<float> scale{std::nullopt};           ///< Optional local UI scale factor override
    const UiTheme* theme{nullptr};                      ///< Optional local theme override
    std::string_view tooltip{};                         ///< Optional hover tooltip text
    bool disabled{false};                               ///< Interaction suppression
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
    bool draggingText{false};
    bool draggingWord{false};
    size_t wordDragStart{0};
    size_t wordDragEnd{0};
    bool draggingVThumb{false};
    bool draggingHThumb{false};
    float dragGrabOffset{0.0f};
    double lastClickTime{-10.0};
    Vec2 lastClickPos{-1000.0f, -1000.0f};
    int clickCount{0};
    double copyFeedbackUntil{-1.0};
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
    bool zoomBadgeHovered{false};
    bool vThumbHovered{false};
    bool vThumbDragging{false};
    bool hThumbHovered{false};
    bool hThumbDragging{false};
};

/// Autonomous high-performance TextArea widget logic (execution, measuring, and virtualized rendering).
class TextArea {
public:
    /// Primary immediate mode entry point for TextArea (editable or read-only).
    static Response Run(
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

    /// Renders the external Element labels, editor frame, gutter line numbers, selection, caret, scrollbars, and syntax spans.
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
    static void HighlightLine(
        std::string_view lineText,
        size_t lineIndex,
        const SyntaxConfig& config,
        std::vector<TextSpan>& outSpans,
        const UiTheme& theme,
        const SyntaxPalette* paletteOverride = nullptr
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
