#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/types.hpp"
#include "keel/array.h"
#include "keel/hash_map.h"

namespace guikit {

/// Deterministic O(1) Least-Recently-Used (LRU) cache with bounded capacity.
/// Backed by a flat `keel::Array` with intrusive index doubly-linked list and `keel::HashMap` (zero node heap allocations).
template <typename Key, typename Value, size_t Capacity = 64>
class LruCache {
    static_assert(Capacity > 0, "LruCache Capacity must be greater than 0");
    static constexpr uint32_t kNil = UINT32_MAX;

    struct Entry {
        Key key{};
        Value value{};
        uint32_t prev{kNil};
        uint32_t next{kNil};
    };

public:
    [[nodiscard]] const Value* Find(const Key& key) noexcept {
        const uint32_t* idxPtr = m_map.get(key);
        if (!idxPtr) {
            return nullptr;
        }
        const uint32_t idx = *idxPtr;
        MoveToFront(idx);
        return &m_entries[static_cast<keel::isize>(idx)].value;
    }

    const Value& Insert(const Key& key, Value value) {
        if (uint32_t* idxPtr = m_map.get(key)) {
            const uint32_t idx = *idxPtr;
            m_entries[static_cast<keel::isize>(idx)].value = std::move(value);
            MoveToFront(idx);
            return m_entries[static_cast<keel::isize>(idx)].value;
        }

        uint32_t slot = kNil;
        if (static_cast<size_t>(m_entries.size()) < Capacity) {
            if (m_entries.capacity() == 0) {
                m_entries.reserve(static_cast<keel::isize>(Capacity));
            }
            slot = static_cast<uint32_t>(m_entries.size());
            m_entries.push_back(Entry{
                .key = key,
                .value = std::move(value),
                .prev = kNil,
                .next = kNil
            });
        } else {
            slot = m_tail;
            Entry& victim = m_entries[static_cast<keel::isize>(slot)];
            (void)m_map.remove(victim.key);
            Detach(slot);
            victim.key = key;
            victim.value = std::move(value);
        }

        AttachFront(slot);
        m_map.put(key, slot);
        return m_entries[static_cast<keel::isize>(slot)].value;
    }

    void Clear() noexcept {
        m_map.clear();
        m_entries.clear();
        m_head = kNil;
        m_tail = kNil;
    }

    [[nodiscard]] size_t Size() const noexcept { return static_cast<size_t>(m_map.size()); }

private:
    void Detach(uint32_t idx) noexcept {
        Entry& e = m_entries[static_cast<keel::isize>(idx)];
        if (e.prev != kNil) {
            m_entries[static_cast<keel::isize>(e.prev)].next = e.next;
        } else {
            m_head = e.next;
        }
        if (e.next != kNil) {
            m_entries[static_cast<keel::isize>(e.next)].prev = e.prev;
        } else {
            m_tail = e.prev;
        }
        e.prev = kNil;
        e.next = kNil;
    }

    void AttachFront(uint32_t idx) noexcept {
        Entry& e = m_entries[static_cast<keel::isize>(idx)];
        e.prev = kNil;
        e.next = m_head;
        if (m_head != kNil) {
            m_entries[static_cast<keel::isize>(m_head)].prev = idx;
        } else {
            m_tail = idx;
        }
        m_head = idx;
    }

    void MoveToFront(uint32_t idx) noexcept {
        if (m_head == idx) return;
        Detach(idx);
        AttachFront(idx);
    }

    keel::Array<Entry> m_entries{};
    keel::HashMap<Key, uint32_t> m_map{};
    uint32_t m_head{kNil};
    uint32_t m_tail{kNil};
};

// ============================================================================
// 1. Inline Rich-Text AST Spans & Parser
// ============================================================================

enum class InlineKind : uint8_t {
    Text = 0,
    Code,
    Link
};

struct InlineSpan {
    InlineKind kind{InlineKind::Text};
    FontStyle style{FontStyle::Regular};
    bool strikethrough{false};
    std::string text{};
    std::string url{};
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
    std::string_view text,
    float fontSize,
    FontStyle style
) noexcept;

/// Parses inline rich-text markup (**bold**, *italic*, `code`, ~~strikethrough~~, [label](url))
/// with support for nested styles inside bold/italic/strikethrough/link spans.
[[nodiscard]] std::vector<InlineSpan> ParseInlineSpans(
    std::string_view line,
    FontStyle inheritedStyle = FontStyle::Regular,
    bool inheritedStrike = false
);

/// Fast single-pass intrinsic width measurement returning {minUnbreakableTokenWidth, unwrappedSingleLineWidth}.
[[nodiscard]] std::pair<float, float> MeasureInlineSpansIntrinsicWidths(
    ICanvas* canvas,
    const std::vector<InlineSpan>& spans,
    float bodyFs,
    float inlineCodeFs,
    FontStyle rowBaseStyle,
    float scale
) noexcept;

/// Cached O(1) intrinsic width measurement for a rich-text markup string: returns {minTokenWidth, unwrappedWidth}.
/// Avoids re-parsing and re-measuring fonts on steady-state frames.
[[nodiscard]] std::pair<float, float> GetOrMeasureRichTextIntrinsicWidths(
    ICanvas* canvas,
    std::string_view richMarkup,
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
    size_t charStart{0};
    size_t charEnd{0};
    size_t lineIndex{0};
    float fontSize{13.0f};
    FontStyle fontStyle{FontStyle::Regular};
    float textPadX{0.0f};
};

struct PositionedLine {
    float top{0.0f};
    float bottom{0.0f};
    size_t firstToken{0};
    size_t tokenCount{0};
    size_t charStart{0};
    size_t charEnd{0};
};

struct LinkHitRegion {
    Rect localRect{};
    std::string_view url{};
};

struct SearchMatchRange {
    size_t start{0};
    size_t end{0};
};

struct RenderOverlayState {
    bool hasSelection{false};
    size_t selMin{0};
    size_t selMax{0};
    bool showCaret{false};
    size_t caretOffset{0};
    const std::vector<SearchMatchRange>* searchMatches{nullptr};
    int activeSearchMatch{-1};
    const keel::HashMap<int, bool>* taskOverrides{nullptr};
};

struct DocumentTextMap {
    std::string plainText{};
    std::vector<PositionedToken> tokens{};
    std::vector<PositionedLine> lines{};

    void AddToken(Rect r, std::string_view str, float fontSize, FontStyle style, float textPadX = 0.0f);
    void AppendNewline();
};

[[nodiscard]] std::pair<float, float> SubRangePixelBounds(
    ICanvas* canvas,
    const PositionedToken& tok,
    std::string_view fullPlainText,
    size_t selMin,
    size_t selMax
);

[[nodiscard]] size_t HitTestDocumentCharOffset(
    const DocumentTextMap& map,
    ICanvas* canvas,
    Vec2 mousePos
);

[[nodiscard]] size_t FindLineForCharOffset(
    const DocumentTextMap& map,
    size_t offset
) noexcept;

[[nodiscard]] float GetCursorXForOffset(
    const DocumentTextMap& map,
    ICanvas* canvas,
    size_t offset
);

[[nodiscard]] std::vector<SearchMatchRange> FindSearchMatches(
    std::string_view text,
    std::string_view query
);

void DrawTokenSelectionAndSearchHighlights(
    ICanvas& canvas,
    const PositionedToken& tok,
    std::string_view fullPlainText,
    const RenderOverlayState* overlay,
    const UiTheme& theme,
    Vec2 docOrigin
);

void DrawTokenCaretIfPresent(
    ICanvas& canvas,
    const PositionedToken& tok,
    std::string_view fullPlainText,
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
    std::string_view text{};
    std::string_view url{};
    float width{0.0f};
    float fontSize{13.0f};
    float padX{0.0f};
};

struct PositionedAtom {
    InlineAtom atom{};
    Rect localRect{};            ///< Relative to the InlineFlowLayout's origin (0, 0)
    float textLocalY{0.0f};      ///< Relative to the InlineFlowLayout's origin (0, 0)
    size_t lineIdx{0};
    size_t tokenIndex{SIZE_MAX}; ///< Index into DocumentTextMap::tokens
};

/// Self-contained, immutable inline flow layout in local (0, 0) coordinates.
/// Owns `textPool` (heap-allocated `std::vector<char>` with zero SSO) so all `InlineAtom::text`
/// and `InlineAtom::url` views remain valid across moves, copies, and cache retention.
struct InlineFlowLayout {
    std::vector<char> textPool{};
    float width{0.0f};
    float height{0.0f};
    float lineH{0.0f};
    float scale{1.0f};
    size_t lineCount{0};
    std::vector<PositionedAtom> items{};

    InlineFlowLayout() = default;
    InlineFlowLayout(InlineFlowLayout&&) noexcept = default;
    InlineFlowLayout& operator=(InlineFlowLayout&&) noexcept = default;

    InlineFlowLayout(const InlineFlowLayout& other)
        : textPool(other.textPool),
          width(other.width),
          height(other.height),
          lineH(other.lineH),
          scale(other.scale),
          lineCount(other.lineCount),
          items(other.items) {
        RebaseViews(other.textPool.data(), other.textPool.size());
    }

    InlineFlowLayout& operator=(const InlineFlowLayout& other) {
        if (this != &other) {
            textPool = other.textPool;
            width = other.width;
            height = other.height;
            lineH = other.lineH;
            scale = other.scale;
            lineCount = other.lineCount;
            items = other.items;
            RebaseViews(other.textPool.data(), other.textPool.size());
        }
        return *this;
    }

private:
    void RebaseViews(const char* oldBase, size_t poolLen) noexcept {
        if (!oldBase || poolLen == 0 || textPool.empty()) return;
        const char* newBase = textPool.data();
        for (PositionedAtom& pa : items) {
            if (!pa.atom.text.empty() && pa.atom.text.data() >= oldBase && pa.atom.text.data() < oldBase + poolLen) {
                pa.atom.text = std::string_view(newBase + (pa.atom.text.data() - oldBase), pa.atom.text.size());
            }
            if (!pa.atom.url.empty() && pa.atom.url.data() >= oldBase && pa.atom.url.data() < oldBase + poolLen) {
                pa.atom.url = std::string_view(newBase + (pa.atom.url.data() - oldBase), pa.atom.url.size());
            }
        }
    }
};

/// Builds a self-contained InlineFlowLayout in local (0, 0) coordinates from pre-parsed spans.
[[nodiscard]] InlineFlowLayout BuildInlineFlowLayout(
    ICanvas* canvas,
    const std::vector<InlineSpan>& spans,
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
[[nodiscard]] std::shared_ptr<const InlineFlowLayout> GetOrBuildRichTextFlow(
    ICanvas* canvas,
    std::string_view richMarkup,
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
    std::vector<LinkHitRegion>& linkRegions
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
