#pragma once

#include <cstdint>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
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

/// Clean configuration options for TextArea widget.
struct TextAreaOptions {
    std::string_view label{};                      ///< Optional caption text above area
    std::string_view placeholder{};                ///< Placeholder text when empty
    std::optional<uint32_t> rows{std::nullopt};    ///< Visible height expressed in rows/lines
    std::optional<float> fixedHeight{std::nullopt};///< Explicit height in points/pixels
    bool autoHeight{false};                        ///< Automatically fit height to content line count
    bool readOnly{false};                          ///< Non-editable display mode
    bool showLineNumbers{true};                    ///< Toggleable line numbers gutter (1, 2, 3...)
    bool monospace{true};                          ///< Monospace typography (TextRole::CodeMedium)
    bool wordWrap{false};                          ///< Wrap long lines (default false for code)
    SyntaxHighlighter syntaxHighlighter{nullptr};  ///< Optional syntax highlighting hook
    void* highlighterUserData{nullptr};            ///< User data passed to syntax highlighter
    UiVariant variant{UiVariant::Default};         ///< Visual variant styling
    std::optional<UiSize> size{std::nullopt};      ///< Local size override
    bool disabled{false};                          ///< Interaction suppression
};

/// Persistent interactive state for TextArea.
struct TextAreaState {
    float scrollY{0.0f};
    size_t cursor{0};
};

/// Internal runtime state allocated in frame arena for TextArea nodes.
struct TextAreaData {
    TextAreaOptions opts{};
    std::string_view text{};
    size_t cursor{0};
    bool cursorVisible{true};
    float scrollY{0.0f};
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

    /// Measures intrinsic dimensions based on content lines, rows, and autoHeight.
    static Vec2 Measure(
        std::string_view text,
        const TextAreaOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme
    );
    static Vec2 Measure(
        std::string_view text,
        const TextAreaOptions& opts,
        UiSize size,
        ICanvas* canvas = nullptr
    );

    /// Renders the frame, gutter line numbers, and virtualized visible lines with syntax spans.
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
        const UiTheme& theme = UiTheme::Dark()
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
