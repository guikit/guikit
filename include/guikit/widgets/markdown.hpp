#pragma once

#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/widgets/text_area.hpp"
#include "keel/core.h"
#include "keel/function.h"
#include "keel/optional.h"
#include "keel/ref.h"

namespace guikit {

class Context;
class ICanvas;

/// Configuration options for the Markdown document viewer widget.
struct MarkdownOptions {
    Align align{Align::Start};
    AxisSize width{};                                          ///< Horizontal sizing policy (`Auto` -> `Stretch`, `Fit`, or `Fixed(dp)`)
    AxisSize height{SizeMode::Fit};                            ///< Vertical sizing policy (`Auto`/`Fit`, `Stretch`, or `Fixed(dp)`)
    StateColor background{};                                   ///< Optional background fill color
    Border border{};                                           ///< Optional border configuration (color, width, radius)

    // --- Smooth Zooming ---
    bool allowZoom{true};                                      ///< Allow Ctrl+MouseWheel & ZoomIn/ZoomOut/ZoomReset actions (0.5x .. 3.0x)
    float* zoom{nullptr};                                      ///< Optional external two-way bound zoom multiplier (1.0f = 100%)

    // --- Typography & Font Sizing ---
    keel::Optional<float> bodyFontSize{keel::kNullOpt};        ///< Base body font size override in points (unscaled)
    float headingScale{1.0f};                                  ///< Multiplier applied to H1..H6 heading sizes (0.6f .. 1.8f)
    keel::Optional<float> inlineCodeFontSize{keel::kNullOpt};  ///< Monospace font size override in points for inline `code` pills
    keel::Optional<float> codeFontSize{keel::kNullOpt};        ///< Monospace font size override in points for display/fenced code blocks
    float lineHeightScale{1.0f};                               ///< Multiplier for line height across document blocks (0.8f .. 2.0f)

    // --- Page Layout, Indents & Spacing ---
    float documentPadding{0.0f};                               ///< Inner document padding around page content in points
    keel::Optional<float> maxContentWidth{keel::kNullOpt};     ///< Optional max reading column width in points (centered when narrower)
    float blockGap{12.0f};                                     ///< Vertical rhythm gap between Markdown blocks
    float paragraphIndent{0.0f};                               ///< Left / first-line indent for paragraphs in points
    float listIndent{18.0f};                                   ///< Horizontal indent step per nested list level in points
    float blockPadding{12.0f};                                 ///< Inner padding inside blockquotes, alerts, and code blocks in points
    float blockRadius{6.0f};                                   ///< Corner radius for code blocks, alerts, blockquotes, and tables in points
    bool headingDividers{true};                                ///< Render subtle horizontal divider line under H1 and H2 headings

    // --- Code Block Styling (shared with TextArea / CodeEditor) ---
    bool codeLineNumbers{true};                                ///< Show line numbers inside fenced code blocks
    bool syntaxHighlight{true};                                ///< Apply language-aware syntax highlighting inside fenced code blocks
    bool showCopyButton{true};                                 ///< Show interactive Copy button on fenced code blocks
    bool codeShowWhitespace{false};                            ///< Show visible space dots and tab arrows inside fenced code blocks
    bool codeIndentGuides{true};                               ///< Show vertical indentation guides inside fenced code blocks
    keel::Optional<SyntaxPalette> codePalette{keel::kNullOpt}; ///< Optional syntax color scheme (Dracula, Monokai, Solarized, etc.) for code blocks

    // --- Embedded Images ---
    bool imageCheckerboard{false};                             ///< Render checkerboard behind embedded Markdown images
    float imageMaxHeight{260.0f};                              ///< Maximum height for embedded images in points
    float imageRadius{6.0f};                                   ///< Corner radius for embedded images in points

    // --- Interactive Features (Selection, Cursor, Search, Tasks) ---
    bool enableSelection{true};                                ///< Allow mouse & keyboard cursor navigation, text selection, and Copy (Ctrl+C)
    bool enableSearch{true};                                   ///< Allow Ctrl+F in-document search bar with match highlighting
    bool interactiveTasks{true};                               ///< Allow clicking - [ ] / - [x] task list checkboxes to toggle state
    keel::StringRef searchQuery{};                             ///< Optional external search query to highlight in document

    // --- Callbacks ---
    keel::SmallFunction<void(keel::StringRef)> onLinkClick{};  ///< Optional callback invoked when a link [text](url) is clicked
    keel::SmallFunction<void(keel::i32, bool)> onTaskToggle{};       ///< Optional callback invoked when a task checkbox is toggled (taskIndex, checked)

    UiText tooltip{};
    UiSize size{UiSize::Md};
    bool disabled{false};
};

/// Extended response returned by `ui.Markdown(...)`.
struct MarkdownResponse : public Response {
    keel::StringRef clickedLink{};                          ///< Non-empty on the frame a Markdown hyperlink was clicked
    keel::i32 copiedBlockIndex{-1};                               ///< >= 0 on the frame a code block's Copy button was clicked
    keel::i32 toggledTaskIndex{-1};                               ///< >= 0 on the frame a task list checkbox was toggled
    bool toggledTaskChecked{false};                         ///< New checked state when toggledTaskIndex >= 0
    float currentZoom{1.0f};                                ///< Effective zoom factor (0.5f .. 3.0f)
    keel::i32 searchMatchCount{0};                                ///< Number of active search matches in the document
    keel::i32 activeSearchMatch{-1};                              ///< 0-based active search match index
    keel::StringRef selectedText{};                         ///< Currently selected text slice (valid for frame lifetime)
};

/// Rich Markdown document viewer widget with inline span flow, alerts, multi-language code blocks, tables, selection, search, and smooth zoom.
class Markdown {
public:
    /// Immediate mode entry point for Markdown widget.
    static MarkdownResponse Run(
        Context& ctx,
        keel::StringRef markdownText,
        const MarkdownOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Measures the full height and width of the Markdown document given constrained `availableWidth`.
    static Vec2 Measure(
        keel::StringRef markdownText,
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
        keel::StringRef markdownText,
        const MarkdownOptions& opts,
        UiSize size,
        Vec2 mousePos = {-1.0f, -1.0f},
        keel::i32 hoveredCopyBlock = -1,
        keel::i32 activeCopiedBlock = -1,
        bool disabled = false,
        const UiTheme& theme = UiTheme::Dark()
    );
};

} // namespace guikit
