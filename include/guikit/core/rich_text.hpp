#pragma once

#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "keel/array.h"
#include "keel/hash_map.h"
#include "keel/lru_cache.h"
#include "keel/smart_ptr.h"
#include "keel/string.h"

namespace guikit {

// ============================================================================
// 1. Inline Rich-Text AST Spans & Parser
// ============================================================================

enum class InlineKind : keel::u8 {
    Text = 0,
    Code,
    Link
};

struct InlineSpan {
    InlineKind kind{InlineKind::Text};
    FontStyle style{FontStyle::Regular};
    bool strikethrough{false};
    keel::String text{};
    keel::String url{};

    [[nodiscard]] InlineSpan clone() const {
        InlineSpan copy;
        copy.kind = kind;
        copy.style = style;
        copy.strikethrough = strikethrough;
        copy.text = text.clone();
        copy.url = url.clone();
        return copy;
    }
};

struct FloatPair {
    float first{0.0f};
    float second{0.0f};
};

[[nodiscard]] constexpr FontStyle CombineFontStyles(FontStyle a, FontStyle b) noexcept {
    if (a == FontStyle::Monospace || b == FontStyle::Monospace) {
        return FontStyle::Monospace;
    }
    const bool bold = (a == FontStyle::Bold || a == FontStyle::BoldItalic ||
                       b == FontStyle::Bold || b == FontStyle::BoldItalic);
    const bool italic = (a == FontStyle::Italic || a == FontStyle::BoldItalic ||
                         b == FontStyle::Italic || b == FontStyle::BoldItalic);
    if (bold && italic) return FontStyle::BoldItalic;
    if (bold) return FontStyle::Bold;
    if (italic) return FontStyle::Italic;
    return FontStyle::Regular;
}

[[nodiscard]] float MeasureStyledSlice(
    ICanvas* canvas,
    keel::StringRef text,
    float fontSize,
    FontStyle style
) noexcept;

/// Parses inline rich-text markup (**bold**, *italic*, `code`, ~~strikethrough~~, [label](url))
/// with support for nested styles inside bold/italic/strikethrough/link spans.
[[nodiscard]] keel::Array<InlineSpan> ParseInlineSpans(
    keel::StringRef line,
    FontStyle inheritedStyle = FontStyle::Regular,
    bool inheritedStrike = false
);

/// Fast single-pass intrinsic width measurement returning {minUnbreakableTokenWidth, unwrappedSingleLineWidth}.
[[nodiscard]] FloatPair MeasureInlineSpansIntrinsicWidths(
    ICanvas* canvas,
    keel::ArrayRef<InlineSpan> spans,
    float bodyFs,
    float inlineCodeFs,
    FontStyle rowBaseStyle,
    float scale
) noexcept;

/// Cached O(1) intrinsic width measurement for a rich-text markup string: returns {minTokenWidth, unwrappedWidth}.
/// Avoids re-parsing and re-measuring fonts on steady-state frames.
[[nodiscard]] FloatPair GetOrMeasureRichTextIntrinsicWidths(
    ICanvas* canvas,
    keel::StringRef richMarkup,
    float bodyFs,
    float inlineCodeFs,
    FontStyle baseStyle = FontStyle::Regular,
    float scale = 1.0f
) noexcept;

// ============================================================================
// 2. Document Text Map (Selection, Cursor Navigation, Link Hit-Test & Search)
// ============================================================================

struct PositionedToken {
    Rect rect{};
    keel::usize charStart{0};
    keel::usize charEnd{0};
    keel::usize lineIndex{0};
    float fontSize{13.0f};
    FontStyle fontStyle{FontStyle::Regular};
    float textPadX{0.0f};
};

struct PositionedLine {
    float top{0.0f};
    float bottom{0.0f};
    keel::usize firstToken{0};
    keel::usize tokenCount{0};
    keel::usize charStart{0};
    keel::usize charEnd{0};
};

struct LinkHitRegion {
    Rect localRect{};
    keel::StringRef url{};
};

struct SearchMatchRange {
    keel::usize start{0};
    keel::usize end{0};
};

struct RenderOverlayState {
    bool hasSelection{false};
    keel::usize selMin{0};
    keel::usize selMax{0};
    bool showCaret{false};
    keel::usize caretOffset{0};
    const keel::Array<SearchMatchRange>* searchMatches{nullptr};
    keel::i32 activeSearchMatch{-1};
    const keel::HashMap<keel::i32, bool>* taskOverrides{nullptr};
};

struct DocumentTextMap {
    keel::String plainText{};
    keel::Array<PositionedToken> tokens{};
    keel::Array<PositionedLine> lines{};

    void AddToken(Rect r, keel::StringRef str, float fontSize, FontStyle style, float textPadX = 0.0f);
    void AppendNewline();
};

[[nodiscard]] FloatPair SubRangePixelBounds(
    ICanvas* canvas,
    const PositionedToken& tok,
    keel::StringRef fullPlainText,
    keel::usize selMin,
    keel::usize selMax
);

[[nodiscard]] keel::usize HitTestDocumentCharOffset(
    const DocumentTextMap& map,
    ICanvas* canvas,
    Vec2 mousePos
);

[[nodiscard]] keel::usize FindLineForCharOffset(
    const DocumentTextMap& map,
    keel::usize offset
) noexcept;

[[nodiscard]] float GetCursorXForOffset(
    const DocumentTextMap& map,
    ICanvas* canvas,
    keel::usize offset
);

[[nodiscard]] keel::Array<SearchMatchRange> FindSearchMatches(
    keel::StringRef text,
    keel::StringRef query
);

void DrawTokenSelectionAndSearchHighlights(
    ICanvas& canvas,
    const PositionedToken& tok,
    keel::StringRef fullPlainText,
    const RenderOverlayState* overlay,
    const UiTheme& theme,
    Vec2 docOrigin
);

void DrawTokenCaretIfPresent(
    ICanvas& canvas,
    const PositionedToken& tok,
    keel::StringRef fullPlainText,
    const RenderOverlayState* overlay,
    const UiTheme& theme,
    Vec2 docOrigin
);

// ============================================================================
// 3. Self-Contained Inline Flow Layout Engine & Zero-Measurement Renderer
// ============================================================================

struct InlineAtom {
    InlineKind kind{InlineKind::Text};
    FontStyle style{FontStyle::Regular};
    bool strikethrough{false};
    bool isWhitespace{false};
    keel::StringRef text{};
    keel::StringRef url{};
    float width{0.0f};
    float fontSize{13.0f};
    float padX{0.0f};
};

struct PositionedAtom {
    InlineAtom atom{};
    Rect localRect{};            ///< Relative to the InlineFlowLayout's origin (0, 0)
    float textLocalY{0.0f};      ///< Relative to the InlineFlowLayout's origin (0, 0)
    keel::usize lineIdx{0};
    keel::usize tokenIndex{keel::kInvalidIndex}; ///< Index into DocumentTextMap::tokens
};

/// Self-contained, immutable inline flow layout in local (0, 0) coordinates.
/// Owns `textPool` (heap-allocated `keel::Array<char>` with zero SSO) so all `InlineAtom::text`
/// and `InlineAtom::url` views remain valid across moves, clones, and cache retention.
struct InlineFlowLayout {
    keel::Array<char> textPool{};
    float width{0.0f};
    float height{0.0f};
    float lineH{0.0f};
    float scale{1.0f};
    keel::usize lineCount{0};
    keel::Array<PositionedAtom> items{};

    InlineFlowLayout() = default;
    InlineFlowLayout(InlineFlowLayout&&) noexcept = default;
    InlineFlowLayout& operator=(InlineFlowLayout&&) noexcept = default;
    InlineFlowLayout(const InlineFlowLayout&) = delete;
    InlineFlowLayout& operator=(const InlineFlowLayout&) = delete;

    [[nodiscard]] InlineFlowLayout clone() const {
        InlineFlowLayout copy;
        copy.textPool = textPool.clone();
        copy.width = width;
        copy.height = height;
        copy.lineH = lineH;
        copy.scale = scale;
        copy.lineCount = lineCount;
        copy.items = items.clone();
        copy.RebaseViews(textPool.data(), textPool.size());
        return copy;
    }

private:
    void RebaseViews(const char* oldBase, keel::usize poolLen) noexcept {
        if (!oldBase || poolLen == 0 || textPool.empty()) return;
        const char* newBase = textPool.data();
        for (PositionedAtom& pa : items) {
            if (!pa.atom.text.empty() && pa.atom.text.data() >= oldBase && pa.atom.text.data() < oldBase + poolLen) {
                pa.atom.text = keel::StringRef(newBase + (pa.atom.text.data() - oldBase), pa.atom.text.size());
            }
            if (!pa.atom.url.empty() && pa.atom.url.data() >= oldBase && pa.atom.url.data() < oldBase + poolLen) {
                pa.atom.url = keel::StringRef(newBase + (pa.atom.url.data() - oldBase), pa.atom.url.size());
            }
        }
    }
};

/// Builds a self-contained InlineFlowLayout in local (0, 0) coordinates from pre-parsed spans.
[[nodiscard]] InlineFlowLayout BuildInlineFlowLayout(
    ICanvas* canvas,
    keel::ArrayRef<InlineSpan> spans,
    float maxWidth,
    float baseFontSize,
    float codeBaseFontSize,
    float lineHeightScale,
    float firstLineIndent,
    const UiTheme& theme,
    float zoom = 1.0f,
    FontStyle overrideStyle = FontStyle::Regular,
    Align horizontalAlign = Align::Start
);

/// Retrieves or builds a cached InlineFlowLayout for a rich-text markup string (supports explicit '\n' line breaks).
[[nodiscard]] keel::Arc<const InlineFlowLayout> GetOrBuildRichTextFlow(
    ICanvas* canvas,
    keel::StringRef richMarkup,
    float maxWidth,
    float baseFontSize,
    float codeBaseFontSize,
    float lineHeightScale,
    const UiTheme& theme,
    float zoom = 1.0f,
    FontStyle overrideStyle = FontStyle::Regular,
    Align horizontalAlign = Align::Start
);

/// Registers all atoms of `flow` into `textMap` and `linkRegions` at document-local offset `flowDocOrigin`.
void CommitInlineFlowToDocument(
    InlineFlowLayout& flow,
    Vec2 flowDocOrigin,
    DocumentTextMap& textMap,
    keel::Array<LinkHitRegion>& linkRegions
);

/// Renders a pre-built InlineFlowLayout at `flowScreenOrigin` with zero font measurements (optional DocumentTextMap).
void RenderInlineFlowLayout(
    ICanvas& canvas,
    const InlineFlowLayout& flow,
    Vec2 flowScreenOrigin,
    Vec2 docScreenOrigin,
    Color baseColor,
    const DocumentTextMap* textMap,
    const RenderOverlayState* overlay,
    const UiTheme& theme,
    Vec2 localMousePos = {-100000.0f, -100000.0f}
);

/// Convenience overload taking `const DocumentTextMap&` directly.
inline void RenderInlineFlowLayout(
    ICanvas& canvas,
    const InlineFlowLayout& flow,
    Vec2 flowScreenOrigin,
    Vec2 docScreenOrigin,
    Color baseColor,
    const DocumentTextMap& textMap,
    const RenderOverlayState* overlay,
    const UiTheme& theme,
    Vec2 localMousePos = {-100000.0f, -100000.0f}
) {
    RenderInlineFlowLayout(canvas, flow, flowScreenOrigin, docScreenOrigin, baseColor, &textMap, overlay, theme, localMousePos);
}

} // namespace guikit
