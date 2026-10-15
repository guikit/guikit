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
    bool codeLineNumbers{true};                             ///< Show line numbers inside fenced code blocks
    bool syntaxHighlight{true};                             ///< Apply syntax highlighting inside fenced code blocks
    bool showCopyButton{true};                              ///< Show interactive Copy button on fenced code blocks
    bool imageCheckerboard{false};                          ///< Render checkerboard behind embedded Markdown images
    float imageMaxHeight{260.0f};                           ///< Maximum height for embedded images in points
    float imageRadius{6.0f};                                ///< Corner radius for embedded images in points
    float blockGap{12.0f};                                  ///< Vertical rhythm gap between Markdown blocks
    std::function<void(std::string_view)> onLinkClick{};    ///< Optional callback invoked when a link [text](url) is clicked
    std::optional<UiSize> size{std::nullopt};
    bool disabled{false};
};

/// Extended response returned by `ui.Markdown(...)`.
struct MarkdownResponse : public Response {
    std::string_view clickedLink{};                         ///< Non-empty on the frame a Markdown hyperlink was clicked
    int copiedBlockIndex{-1};                               ///< >= 0 on the frame a code block's Copy button was clicked
};

/// Rich Markdown document viewer widget with inline span flow, alerts, code blocks, tables, and vector/raster images.
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
