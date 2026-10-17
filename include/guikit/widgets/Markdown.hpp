#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/widgets/TextArea.hpp"

namespace guikit {

class Context;
class ICanvas;

/// Configuration options for the Markdown document viewer widget.
struct MarkdownOptions {
    UiText label{};                                         ///< Optional external heading/label
    UiText description{};                                   ///< Optional external caption/description
    ElementLayout layout{ElementLayout::Stacked};
    InlineOrder inlineOrder{InlineOrder::LabelFirst};
    Align elementAlign{Align::Start};
    StretchMode stretch{StretchMode::Fill};
    std::optional<float> width{std::nullopt};               ///< Explicit width in px or % (in Fill mode)

    // --- Smooth Zooming ---
    bool allowZoom{true};                                   ///< Allow Ctrl+MouseWheel & ZoomIn/ZoomOut/ZoomReset actions (0.5x .. 3.0x)
    float* zoom{nullptr};                                   ///< Optional external two-way bound zoom multiplier (1.0f = 100%)

    // --- Typography & Font Sizing ---
    std::optional<float> bodyFontSize{std::nullopt};        ///< Base body font size override in points (unscaled)
    float headingScale{1.0f};                               ///< Multiplier applied to H1..H6 heading sizes (0.6f .. 1.8f)
    std::optional<float> inlineCodeFontSize{std::nullopt};  ///< Monospace font size override in points for inline `code` pills
    std::optional<float> codeFontSize{std::nullopt};        ///< Monospace font size override in points for display/fenced code blocks
    float lineHeightScale{1.0f};                            ///< Multiplier for line height across document blocks (0.8f .. 2.0f)

    // --- Page Layout, Indents & Spacing ---
    float documentPadding{0.0f};                            ///< Inner document padding around page content in points
    std::optional<float> maxContentWidth{std::nullopt};     ///< Optional max reading column width in points (centered when narrower)
    float blockGap{12.0f};                                  ///< Vertical rhythm gap between Markdown blocks
    float paragraphIndent{0.0f};                            ///< Left / first-line indent for paragraphs in points
    float listIndent{18.0f};                                ///< Horizontal indent step per nested list level in points
    float blockPadding{12.0f};                              ///< Inner padding inside blockquotes, alerts, and code blocks in points
    float blockRadius{6.0f};                                ///< Corner radius for code blocks, alerts, blockquotes, and tables in points
    bool headingDividers{true};                             ///< Render subtle horizontal divider line under H1 and H2 headings

    // --- Code Block Styling (shared with TextArea / CodeEditor) ---
    bool codeLineNumbers{true};                             ///< Show line numbers inside fenced code blocks
    bool syntaxHighlight{true};                             ///< Apply language-aware syntax highlighting inside fenced code blocks
    bool showCopyButton{true};                              ///< Show interactive Copy button on fenced code blocks
    bool codeShowWhitespace{false};                         ///< Show visible space dots and tab arrows inside fenced code blocks
    bool codeIndentGuides{true};                            ///< Show vertical indentation guides inside fenced code blocks
    std::optional<SyntaxPalette> codePalette{std::nullopt}; ///< Optional syntax color scheme (Dracula, Monokai, Solarized, etc.) for code blocks

    // --- Embedded Images ---
    bool imageCheckerboard{false};                          ///< Render checkerboard behind embedded Markdown images
    float imageMaxHeight{260.0f};                           ///< Maximum height for embedded images in points
    float imageRadius{6.0f};                                ///< Corner radius for embedded images in points

    // --- Interactive Features (Selection, Cursor, Search, Tasks) ---
    bool enableSelection{true};                             ///< Allow mouse & keyboard cursor navigation, text selection, and Copy (Ctrl+C)
    bool enableSearch{true};                                ///< Allow Ctrl+F in-document search bar with match highlighting
    bool interactiveTasks{true};                            ///< Allow clicking - [ ] / - [x] task list checkboxes to toggle state
    std::string_view searchQuery{};                         ///< Optional external search query to highlight in document

    // --- Callbacks ---
    std::function<void(std::string_view)> onLinkClick{};    ///< Optional callback invoked when a link [text](url) is clicked
    std::function<void(int, bool)> onTaskToggle{};          ///< Optional callback invoked when a task checkbox is toggled (taskIndex, checked)

    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

/// Extended response returned by `ui.Markdown(...)`.
struct MarkdownResponse : public Response {
    std::string_view clickedLink{};                         ///< Non-empty on the frame a Markdown hyperlink was clicked
    int copiedBlockIndex{-1};                               ///< >= 0 on the frame a code block's Copy button was clicked
    int toggledTaskIndex{-1};                               ///< >= 0 on the frame a task list checkbox was toggled
    bool toggledTaskChecked{false};                         ///< New checked state when toggledTaskIndex >= 0
    float currentZoom{1.0f};                                ///< Effective zoom factor (0.5f .. 3.0f)
    int searchMatchCount{0};                                ///< Number of active search matches in the document
    int activeSearchMatch{-1};                              ///< 0-based active search match index
    std::string_view selectedText{};                        ///< Currently selected text slice (valid for frame lifetime)
};

/// Rich Markdown document viewer widget with inline span flow, alerts, multi-language code blocks, tables, selection, search, and smooth zoom.
class Markdown {
public:
    /// Immediate mode entry point for Markdown widget.
    static MarkdownResponse Run(
        Context& ctx,
        std::string_view markdownText,
        const MarkdownOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Measures the full height and width of the Markdown document given constrained `availableWidth`.
    static Vec2 Measure(
        std::string_view markdownText,
        const MarkdownOptions& opts,
        UiSize size,
        ICanvas* canvas,
        const UiTheme& theme,
        float availableWidth = 0.0f
    );

    /// Renders the Markdown document within `bounds`.
    static void Render(
        ICanvas& canvas,
        Rect bounds,
        std::string_view markdownText,
        const MarkdownOptions& opts,
        UiSize size,
        Vec2 mousePos = {-1.0f, -1.0f},
        int hoveredCopyBlock = -1,
        int activeCopiedBlock = -1,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
