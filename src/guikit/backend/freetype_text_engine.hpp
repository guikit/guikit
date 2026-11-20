#pragma once

#include "guikit/backend/i_canvas.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/hash_map.h"

struct FT_LibraryRec_;
struct FT_FaceRec_;

namespace guikit {

/// Internal FreeType-backed hinted glyph rasterizer and per-glyph LRU atlas.
///
/// Shares `TextCompositor` for gamma-correct grayscale and LCD RGB/BGR compositing.
/// Never leaks `<ft2build.h>` or FreeType types into public `include/guikit/` headers.
class FreeTypeTextEngine {
public:
    FreeTypeTextEngine() noexcept;
    ~FreeTypeTextEngine() noexcept;

    FreeTypeTextEngine(const FreeTypeTextEngine&) = delete;
    FreeTypeTextEngine& operator=(const FreeTypeTextEngine&) = delete;

    [[nodiscard]] bool IsAvailable() const noexcept { return m_library != nullptr; }

    /// Loads a TrueType/OpenType font from memory into `style` slot.
    /// If `copyData` is false, `data` must point to static/embedded lifetime storage.
    bool LoadFontMemory(keel::ArrayRef<keel::u8> data, FontStyle style, bool copyData);

    /// Returns true if a valid face can be resolved for `style` (directly or via Regular fallback).
    [[nodiscard]] bool HasFontForStyle(FontStyle style) const noexcept;

    /// Measures single-line UTF-8 text width and line box height at `scaledFontSizePx`.
    [[nodiscard]] Vec2 MeasureText(
        keel::StringRef text,
        float scaledFontSizePx,
        FontStyle style,
        FontHinting hinting
    );

    /// Draws single-line UTF-8 text at physical pixel top-left `(posX, posY)`.
    bool DrawText(
        keel::u32* dstBuffer,
        keel::u32 dstW,
        keel::u32 dstH,
        keel::u32 dstStride,
        keel::StringRef text,
        float posX,
        float posY,
        float scaledFontSizePx,
        Color color,
        FontStyle style,
        Rect clipPx,
        const TextRenderSettings& settings,
        float canvasScale
    );

    /// Draws single-line UTF-8 text aligned inside `boundsPx` using true cap-height centering.
    bool DrawTextBox(
        keel::u32* dstBuffer,
        keel::u32 dstW,
        keel::u32 dstH,
        keel::u32 dstStride,
        keel::StringRef text,
        Rect boundsPx,
        float scaledFontSizePx,
        Color color,
        FontStyle style,
        float alignX,
        float alignY,
        Rect clipPx,
        const TextRenderSettings& settings,
        float canvasScale
    );

    /// Releases cached glyph bitmaps while keeping loaded font faces alive.
    void TrimCaches() noexcept;

    [[nodiscard]] keel::usize GetCachedGlyphCount() const noexcept { return m_glyphMap.size(); }

private:
    static constexpr keel::usize kFontStyleCount = 5u;
    static constexpr keel::usize kMaxGlyphEntries = 4096u;
    static constexpr keel::usize kMaxGlyphBytes = 4u * 1024u * 1024u;
    static constexpr keel::u32 kNil = 0xFFFFFFFFu;

    struct FaceSlot {
        FT_FaceRec_* face{nullptr};
        keel::Array<keel::u8> ownedData;
        const keel::u8* staticData{nullptr};
        keel::usize staticSize{0};
        keel::u16 currentSizeQ4{0};
    };

    struct ResolvedFace {
        FaceSlot* slot{nullptr};
        keel::u8 slotIndex{0};
        bool synthBold{false};
        bool synthItalic{false};
    };

    struct GlyphKey {
        keel::u32 glyphIndex{0};
        keel::u16 sizeQ4{0};
        keel::u8 slotIndex{0};
        keel::u8 flags{0}; ///< bits: [0..1] hinting, [2] subpixel, [3..4] phase (0..3), [5] bold, [6] italic

        bool operator==(const GlyphKey& o) const noexcept {
            return glyphIndex == o.glyphIndex &&
                   sizeQ4 == o.sizeQ4 &&
                   slotIndex == o.slotIndex &&
                   flags == o.flags;
        }
    };

    struct GlyphKeyHash {
        keel::u64 operator()(const GlyphKey& k) const noexcept {
            keel::u64 h = static_cast<keel::u64>(k.glyphIndex);
            h ^= (static_cast<keel::u64>(k.sizeQ4) |
                  (static_cast<keel::u64>(k.slotIndex) << 16) |
                  (static_cast<keel::u64>(k.flags) << 24)) * 0x9e3779b97f4a7c15ULL;
            h ^= h >> 33;
            return h * 0xff51afd7ed558ccdULL;
        }
    };

    struct GlyphEntry {
        keel::Array<keel::u8> bitmap; ///< 1 byte/px (Grayscale) or 3 bytes/px RGB (LCD)
        keel::u16 width{0};
        keel::u16 height{0};
        keel::u16 strideBytes{0};
        keel::i16 bearingX{0};
        keel::i16 bearingY{0};
        float advancePx{0.0f};
        bool isSubpixel{false};
        bool empty{false};
    };

    struct GlyphNode {
        GlyphKey key{};
        GlyphEntry entry{};
        keel::u32 prev{kNil};
        keel::u32 next{kNil};
    };

    [[nodiscard]] ResolvedFace ResolveFace(FontStyle style) noexcept;
    bool EnsureFaceSize(FaceSlot& slot, keel::u16 sizeQ4) noexcept;
    [[nodiscard]] const GlyphEntry& ResolveGlyph(
        const ResolvedFace& rf,
        keel::u32 glyphIndex,
        keel::u16 sizeQ4,
        FontHinting hinting,
        bool subpixel,
        keel::u8 phase
    );

    void EvictTail() noexcept;
    void Detach(keel::u32 idx) noexcept;
    void AttachFront(keel::u32 idx) noexcept;
    void MoveToFront(keel::u32 idx) noexcept;

    FT_LibraryRec_* m_library{nullptr};
    keel::StaticArray<FaceSlot, kFontStyleCount> m_faces{};

    keel::Array<GlyphNode> m_glyphNodes;
    keel::HashMap<GlyphKey, keel::u32, GlyphKeyHash> m_glyphMap;
    keel::usize m_totalGlyphBytes{0};
    keel::u32 m_head{kNil};
    keel::u32 m_tail{kNil};
    keel::u32 m_freeHead{kNil};
};

} // namespace guikit
