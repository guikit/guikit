#include "guikit/backend/freetype_text_engine.hpp"
#include <cstring>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_LCD_FILTER_H
#include FT_SYNTHESIS_H
#include "guikit/backend/text_compositor.hpp"
#include "guikit/core/assets.hpp"
#include "keel/math.h"
#include "keel/utf8.h"

namespace guikit {

namespace {

[[nodiscard]] constexpr keel::usize StyleToSlot(FontStyle style) noexcept {
    const auto idx = static_cast<keel::usize>(style);
    return (idx < 5u) ? idx : 0u;
}

[[nodiscard]] constexpr keel::u16 QuantizeSizeQ4(float scaledFontSizePx) noexcept {
    return static_cast<keel::u16>(keel::clamp(keel::round_i32(scaledFontSizePx * 4.0f), 4, 2048));
}

[[nodiscard]] constexpr keel::u8 PackGlyphFlags(
    FontHinting hinting,
    bool subpixel,
    keel::u8 phase,
    bool synthBold,
    bool synthItalic
) noexcept {
    keel::u8 f = static_cast<keel::u8>(static_cast<keel::u8>(hinting) & 0x03u);
    if (subpixel) f |= (1u << 2);
    f |= static_cast<keel::u8>((phase & 0x03u) << 3);
    if (synthBold) f |= (1u << 5);
    if (synthItalic) f |= (1u << 6);
    return f;
}

[[nodiscard]] constexpr bool UsesSubpixelPhase(FontHinting hinting) noexcept {
    return hinting != FontHinting::Normal;
}

} // namespace

FreeTypeTextEngine::FreeTypeTextEngine() noexcept {
    if (FT_Init_FreeType(&m_library) != 0) {
        m_library = nullptr;
        return;
    }
    // Configure 5-tap balanced LCD filter when ClearType subpixel rendering is enabled;
    // on modern Harmony LCD builds this returns FT_Err_Unimplemented_Feature and is safely ignored.
    (void)FT_Library_SetLcdFilter(m_library, FT_LCD_FILTER_DEFAULT);
}

FreeTypeTextEngine::~FreeTypeTextEngine() noexcept {
    TrimCaches();
    for (auto& slot : m_faces) {
        if (slot.face != nullptr) {
            FT_Done_Face(slot.face);
            slot.face = nullptr;
        }
        slot.ownedData = keel::Array<keel::u8>{};
        slot.staticData = nullptr;
        slot.staticSize = 0;
    }
    if (m_library != nullptr) {
        FT_Done_FreeType(m_library);
        m_library = nullptr;
    }
}

bool FreeTypeTextEngine::LoadFontMemory(keel::ArrayRef<keel::u8> data, FontStyle style, bool copyData) {
    if (!m_library || data.empty()) {
        return false;
    }

    const keel::usize idx = StyleToSlot(style);
    FaceSlot& slot = m_faces[idx];

    keel::Array<keel::u8> newOwned;
    const keel::u8* bufPtr = data.data();
    const keel::usize bufSize = data.size();
    if (copyData) {
        newOwned.resize(bufSize);
        std::memcpy(newOwned.data(), bufPtr, bufSize);
        bufPtr = newOwned.data();
    }

    FT_Face newFace = nullptr;
    const FT_Error err = FT_New_Memory_Face(
        m_library,
        reinterpret_cast<const FT_Byte*>(bufPtr),
        static_cast<FT_Long>(bufSize),
        0,
        &newFace
    );
    if (err != 0 || !newFace) {
        return false;
    }

    (void)FT_Select_Charmap(newFace, FT_ENCODING_UNICODE);

    if (slot.face != nullptr) {
        FT_Done_Face(slot.face);
        slot.face = nullptr;
    }
    slot.face = newFace;
    slot.ownedData = keel::move(newOwned);
    slot.staticData = copyData ? nullptr : data.data();
    slot.staticSize = copyData ? 0u : bufSize;
    slot.currentSizeQ4 = 0u;

    TrimCaches();
    return true;
}

bool FreeTypeTextEngine::LoadDefaultEmbeddedFonts() {
    if (!m_library) {
        return false;
    }
    struct DefaultFontSlot {
        keel::StringRef path;
        FontStyle style;
    };
    static constexpr DefaultFontSlot kDefaults[] = {
        {"assets/fonts/Roboto-Regular.ttf", FontStyle::Regular},
        {"assets/fonts/Roboto-Bold.ttf", FontStyle::Bold},
        {"assets/fonts/Roboto-Italic.ttf", FontStyle::Italic},
        {"assets/fonts/Roboto-BoldItalic.ttf", FontStyle::BoldItalic},
        {"assets/fonts/JetBrainsMono-Regular.ttf", FontStyle::Monospace},
    };
    bool loadedRegular = false;
    for (const auto& entry : kDefaults) {
        if (const auto asset = Assets::Find(entry.path); asset && !asset->Empty()) {
            const bool ok = LoadFontMemory(asset->Bytes(), entry.style, /*copyData=*/false);
            if (ok && entry.style == FontStyle::Regular) {
                loadedRegular = true;
            }
        }
    }
    return loadedRegular || HasFontForStyle(FontStyle::Regular);
}

bool FreeTypeTextEngine::GetFontMetrics(
    float scaledFontSizePx,
    FontStyle style,
    FontHinting hinting,
    FontMetrics& outMetrics
) {
    outMetrics = FontMetrics{};
    if (scaledFontSizePx <= 0.0f) {
        return false;
    }
    const ResolvedFace rf = ResolveFace(style);
    if (!rf.slot) {
        return false;
    }
    const keel::u16 sizeQ4 = QuantizeSizeQ4(scaledFontSizePx);
    if (!EnsureFaceSize(*rf.slot, sizeQ4)) {
        return false;
    }
    FT_Face face = rf.slot->face;
    if (face->units_per_EM > 0u && face->ascender > face->descender) {
        const float scale = scaledFontSizePx / static_cast<float>(face->units_per_EM);
        outMetrics.ascentPx = static_cast<float>(face->ascender) * scale;
        outMetrics.descentPx = static_cast<float>(-face->descender) * scale;
        outMetrics.lineHeightPx = static_cast<float>(face->ascender - face->descender) * scale;
    } else {
        outMetrics.ascentPx = static_cast<float>(face->size->metrics.ascender) / 64.0f;
        outMetrics.descentPx = static_cast<float>(-face->size->metrics.descender) / 64.0f;
        outMetrics.lineHeightPx = scaledFontSizePx * 1.25f;
    }
    const keel::u32 capIdx = FT_Get_Char_Index(face, static_cast<keel::u32>('H'));
    if (capIdx != 0u) {
        const GlyphEntry& capG = ResolveGlyph(rf, capIdx, sizeQ4, hinting, false, 0u);
        outMetrics.capHeightPx = (capG.bearingY > 0) ? static_cast<float>(capG.bearingY) : (scaledFontSizePx * 0.72f);
    } else {
        outMetrics.capHeightPx = scaledFontSizePx * 0.72f;
    }
    return true;
}

float FreeTypeTextEngine::GetKerningPx(
    keel::u32 prevCodepoint,
    keel::u32 codepoint,
    float scaledFontSizePx,
    FontStyle style,
    FontHinting hinting
) {
    if (prevCodepoint == 0u || codepoint == 0u || scaledFontSizePx <= 0.0f) {
        return 0.0f;
    }
    const ResolvedFace rf = ResolveFace(style);
    if (!rf.slot) {
        return 0.0f;
    }
    const keel::u16 sizeQ4 = QuantizeSizeQ4(scaledFontSizePx);
    if (!EnsureFaceSize(*rf.slot, sizeQ4)) {
        return 0.0f;
    }
    FT_Face face = rf.slot->face;
    if (FT_HAS_KERNING(face) == 0) {
        return 0.0f;
    }
    const keel::u32 g1 = FT_Get_Char_Index(face, prevCodepoint);
    const keel::u32 g2 = FT_Get_Char_Index(face, codepoint);
    if (g1 == 0u || g2 == 0u) {
        return 0.0f;
    }
    const FT_UInt kernMode = UsesSubpixelPhase(hinting) ? FT_KERNING_UNFITTED : FT_KERNING_DEFAULT;
    FT_Vector kern{};
    if (FT_Get_Kerning(face, g1, g2, kernMode, &kern) != 0) {
        return 0.0f;
    }
    return static_cast<float>(kern.x) / 64.0f;
}

bool FreeTypeTextEngine::RasterizeCodepoint(
    keel::u32 codepoint,
    float scaledFontSizePx,
    FontStyle style,
    FontHinting hinting,
    bool subpixel,
    keel::u8 phase,
    RasterizedGlyph& outGlyph
) {
    outGlyph = RasterizedGlyph{};
    if (scaledFontSizePx <= 0.0f) {
        return false;
    }
    const ResolvedFace rf = ResolveFace(style);
    if (!rf.slot) {
        return false;
    }
    const keel::u16 sizeQ4 = QuantizeSizeQ4(scaledFontSizePx);
    if (!EnsureFaceSize(*rf.slot, sizeQ4)) {
        return false;
    }
    FT_Face face = rf.slot->face;
    const keel::u32 glyphIdx = FT_Get_Char_Index(face, codepoint);
    const GlyphEntry& g = ResolveGlyph(rf, glyphIdx, sizeQ4, hinting, subpixel, phase);
    outGlyph.bitmap = g.bitmap.empty() ? nullptr : g.bitmap.data();
    outGlyph.width = g.width;
    outGlyph.height = g.height;
    outGlyph.strideBytes = g.strideBytes;
    outGlyph.bearingX = g.bearingX;
    outGlyph.bearingY = g.bearingY;
    outGlyph.advancePx = g.advancePx;
    outGlyph.isSubpixel = g.isSubpixel;
    outGlyph.empty = g.empty;
    return true;
}

bool FreeTypeTextEngine::HasFontForStyle(FontStyle style) const noexcept {
    if (!m_library) return false;
    const keel::usize idx = StyleToSlot(style);
    if (m_faces[idx].face != nullptr) return true;
    if (style == FontStyle::BoldItalic) {
        if (m_faces[StyleToSlot(FontStyle::Bold)].face != nullptr ||
            m_faces[StyleToSlot(FontStyle::Italic)].face != nullptr) {
            return true;
        }
    }
    return m_faces[0].face != nullptr;
}

FreeTypeTextEngine::ResolvedFace FreeTypeTextEngine::ResolveFace(FontStyle style) noexcept {
    const keel::usize idx = StyleToSlot(style);
    if (m_faces[idx].face != nullptr) {
        return ResolvedFace{
            .slot = &m_faces[idx],
            .slotIndex = static_cast<keel::u8>(idx),
            .synthBold = false,
            .synthItalic = false
        };
    }

    if (style == FontStyle::BoldItalic) {
        const keel::usize boldIdx = StyleToSlot(FontStyle::Bold);
        if (m_faces[boldIdx].face != nullptr) {
            return ResolvedFace{
                .slot = &m_faces[boldIdx],
                .slotIndex = static_cast<keel::u8>(boldIdx),
                .synthBold = false,
                .synthItalic = true
            };
        }
        const keel::usize italIdx = StyleToSlot(FontStyle::Italic);
        if (m_faces[italIdx].face != nullptr) {
            return ResolvedFace{
                .slot = &m_faces[italIdx],
                .slotIndex = static_cast<keel::u8>(italIdx),
                .synthBold = true,
                .synthItalic = false
            };
        }
    }

    if (m_faces[0].face != nullptr) {
        return ResolvedFace{
            .slot = &m_faces[0],
            .slotIndex = 0u,
            .synthBold = (style == FontStyle::Bold || style == FontStyle::BoldItalic),
            .synthItalic = (style == FontStyle::Italic || style == FontStyle::BoldItalic)
        };
    }

    return ResolvedFace{};
}

bool FreeTypeTextEngine::EnsureFaceSize(FaceSlot& slot, keel::u16 sizeQ4) noexcept {
    if (!slot.face) return false;
    if (slot.currentSizeQ4 == sizeQ4) return true;
    // sizeQ4 is in quarter-pixels; FT_F26Dot6 is 1/64 px -> multiply by 16
    const FT_F26Dot6 charHeight = static_cast<FT_F26Dot6>(sizeQ4) * 16;
    if (FT_Set_Char_Size(slot.face, 0, charHeight, 72, 72) != 0) {
        return false;
    }
    slot.currentSizeQ4 = sizeQ4;
    return true;
}

const FreeTypeTextEngine::GlyphEntry& FreeTypeTextEngine::ResolveGlyph(
    const ResolvedFace& rf,
    keel::u32 glyphIndex,
    keel::u16 sizeQ4,
    FontHinting hinting,
    bool subpixel,
    keel::u8 phase
) {
    const keel::u8 effPhase = UsesSubpixelPhase(hinting) ? static_cast<keel::u8>(phase & 0x03u) : 0u;
    const GlyphKey key{
        .glyphIndex = glyphIndex,
        .sizeQ4 = sizeQ4,
        .slotIndex = rf.slotIndex,
        .flags = PackGlyphFlags(hinting, subpixel, effPhase, rf.synthBold, rf.synthItalic)
    };

    if (const keel::u32* idxPtr = m_glyphMap.get(key)) {
        const keel::u32 idx = *idxPtr;
        MoveToFront(idx);
        return m_glyphNodes[idx].entry;
    }

    GlyphEntry entry{};
    entry.isSubpixel = subpixel;
    entry.empty = true;

    if (rf.slot && EnsureFaceSize(*rf.slot, sizeQ4)) {
        FT_Face face = rf.slot->face;

        FT_Matrix mat{0x10000L, 0L, 0L, 0x10000L};
        if (rf.synthItalic) {
            mat.xy = static_cast<FT_Fixed>(0.18f * 65536.0f);
        }
        FT_Vector delta{static_cast<FT_Pos>(effPhase) * 16, 0};
        FT_Set_Transform(face, &mat, &delta);

        FT_Int32 loadFlags = FT_LOAD_DEFAULT;
        if (hinting == FontHinting::None) {
            loadFlags = FT_LOAD_NO_HINTING;
        } else if (hinting == FontHinting::Light) {
            loadFlags = FT_LOAD_TARGET_LIGHT;
        } else {
            loadFlags = subpixel ? FT_LOAD_TARGET_LCD : FT_LOAD_TARGET_NORMAL;
        }

        if (FT_Load_Glyph(face, glyphIndex, loadFlags) == 0) {
            FT_GlyphSlot gslot = face->glyph;
            if (rf.synthBold && gslot->format == FT_GLYPH_FORMAT_OUTLINE) {
                FT_GlyphSlot_Embolden(gslot);
            }

            // Record advance before rendering (unhinted linear advance for None/Light, hinted advance for Normal)
            if (UsesSubpixelPhase(hinting) && gslot->linearHoriAdvance > 0) {
                entry.advancePx = static_cast<float>(gslot->linearHoriAdvance) / 65536.0f;
            } else {
                entry.advancePx = static_cast<float>(gslot->advance.x) / 64.0f;
            }
            if (rf.synthBold) {
                entry.advancePx += 0.35f;
            }

            const FT_Render_Mode renderMode = subpixel ? FT_RENDER_MODE_LCD : FT_RENDER_MODE_NORMAL;
            if (FT_Render_Glyph(gslot, renderMode) == 0) {
                const FT_Bitmap& bm = gslot->bitmap;
                if (bm.buffer != nullptr && bm.width > 0u && bm.rows > 0u) {
                    const bool isLcdBm = (bm.pixel_mode == FT_PIXEL_MODE_LCD);
                    const keel::u32 pixelW = isLcdBm ? (bm.width / 3u) : bm.width;
                    const keel::u32 pixelH = bm.rows;
                    if (pixelW > 0u && pixelH > 0u) {
                        const keel::u32 rowBytes = isLcdBm ? (pixelW * 3u) : pixelW;
                        entry.width = static_cast<keel::u16>(pixelW);
                        entry.height = static_cast<keel::u16>(pixelH);
                        entry.strideBytes = static_cast<keel::u16>(rowBytes);
                        entry.bearingX = static_cast<keel::i16>(gslot->bitmap_left);
                        entry.bearingY = static_cast<keel::i16>(gslot->bitmap_top);
                        entry.isSubpixel = isLcdBm;
                        entry.empty = false;
                        entry.bitmap.resize(static_cast<keel::usize>(rowBytes) * static_cast<keel::usize>(pixelH));

                        const keel::i32 pitch = bm.pitch;
                        for (keel::u32 row = 0u; row < pixelH; ++row) {
                            const keel::u8* srcRow = (pitch >= 0)
                                ? (bm.buffer + static_cast<keel::usize>(row) * static_cast<keel::usize>(pitch))
                                : (bm.buffer + static_cast<keel::usize>(pixelH - 1u - row) * static_cast<keel::usize>(-pitch));
                            std::memcpy(
                                entry.bitmap.data() + static_cast<keel::usize>(row) * static_cast<keel::usize>(rowBytes),
                                srcRow,
                                rowBytes
                            );
                        }
                    }
                }
            }
        }
        FT_Set_Transform(face, nullptr, nullptr);
    }

    const keel::usize newBytes = entry.bitmap.capacity();
    while (!m_glyphMap.empty() &&
           (m_glyphMap.size() >= kMaxGlyphEntries ||
            (newBytes > 0u && m_totalGlyphBytes + newBytes > kMaxGlyphBytes))) {
        EvictTail();
    }

    keel::u32 slotIdx = kNil;
    if (m_freeHead != kNil) {
        slotIdx = m_freeHead;
        m_freeHead = m_glyphNodes[slotIdx].next;
        m_glyphNodes[slotIdx].key = key;
        m_glyphNodes[slotIdx].entry = keel::move(entry);
        m_glyphNodes[slotIdx].prev = kNil;
        m_glyphNodes[slotIdx].next = kNil;
    } else {
        if (m_glyphNodes.capacity() == 0u) {
            m_glyphNodes.reserve(512u);
        }
        slotIdx = static_cast<keel::u32>(m_glyphNodes.size());
        m_glyphNodes.push_back(GlyphNode{
            .key = key,
            .entry = keel::move(entry),
            .prev = kNil,
            .next = kNil
        });
    }

    m_totalGlyphBytes += newBytes;
    AttachFront(slotIdx);
    m_glyphMap.put(key, slotIdx);
    return m_glyphNodes[slotIdx].entry;
}

Vec2 FreeTypeTextEngine::MeasureText(
    keel::StringRef text,
    float scaledFontSizePx,
    FontStyle style,
    FontHinting hinting
) {
    if (text.empty() || scaledFontSizePx <= 0.0f) {
        return {0.0f, 0.0f};
    }

    const ResolvedFace rf = ResolveFace(style);
    if (!rf.slot) {
        return {0.0f, 0.0f};
    }

    const keel::u16 sizeQ4 = QuantizeSizeQ4(scaledFontSizePx);
    if (!EnsureFaceSize(*rf.slot, sizeQ4)) {
        return {0.0f, 0.0f};
    }

    FT_Face face = rf.slot->face;
    const bool hasKerning = FT_HAS_KERNING(face) != 0;
    const bool subPhase = UsesSubpixelPhase(hinting);
    const FT_UInt kernMode = subPhase ? FT_KERNING_UNFITTED : FT_KERNING_DEFAULT;

    float totalAdvance = 0.0f;
    keel::u32 prevGlyph = 0u;
    keel::usize offset = 0u;
    while (offset < text.size()) {
        const keel::u32 cp = keel::utf8_decode_next(text, offset);
        if (cp == '\n' || cp == '\r') continue;
        if (cp == '\t') {
            const keel::u32 spIdx = FT_Get_Char_Index(face, static_cast<keel::u32>(' '));
            const GlyphEntry& sp = ResolveGlyph(rf, spIdx, sizeQ4, hinting, false, 0u);
            totalAdvance += (sp.advancePx > 0.0f) ? (sp.advancePx * 4.0f) : (scaledFontSizePx * 2.4f);
            prevGlyph = 0u;
            continue;
        }
        const keel::u32 glyphIdx = FT_Get_Char_Index(face, cp);
        if (hasKerning && prevGlyph != 0u && glyphIdx != 0u) {
            FT_Vector kern{};
            if (FT_Get_Kerning(face, prevGlyph, glyphIdx, kernMode, &kern) == 0) {
                totalAdvance += static_cast<float>(kern.x) / 64.0f;
            }
        }
        const GlyphEntry& g = ResolveGlyph(rf, glyphIdx, sizeQ4, hinting, false, 0u);
        totalAdvance += g.advancePx;
        prevGlyph = glyphIdx;
    }

    const float glyphBoxH = (face->units_per_EM > 0u && face->ascender > face->descender)
        ? (scaledFontSizePx * static_cast<float>(face->ascender - face->descender) / static_cast<float>(face->units_per_EM))
        : (scaledFontSizePx * 1.25f);
    return {totalAdvance, glyphBoxH};
}

bool FreeTypeTextEngine::DrawText(
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
) {
    if (!dstBuffer || text.empty() || scaledFontSizePx <= 0.0f || color.a == 0u) {
        return true;
    }

    const ResolvedFace rf = ResolveFace(style);
    if (!rf.slot) {
        return false;
    }

    const keel::u16 sizeQ4 = QuantizeSizeQ4(scaledFontSizePx);
    if (!EnsureFaceSize(*rf.slot, sizeQ4)) {
        return false;
    }

    FT_Face face = rf.slot->face;
    const float ascPx = (face->units_per_EM > 0u && face->ascender > 0)
        ? (scaledFontSizePx * static_cast<float>(face->ascender) / static_cast<float>(face->units_per_EM))
        : (static_cast<float>(face->size->metrics.ascender) / 64.0f);
    const keel::i32 baselineY = keel::round_i32(posY + ascPx);

    const FontSmoothing effSmooth = TextCompositor::ResolveEffectiveSmoothing(settings.smoothing, canvasScale);
    const bool useSubpixel = (effSmooth == FontSmoothing::SubpixelRGB || effSmooth == FontSmoothing::SubpixelBGR);
    const bool bgrOrder = (effSmooth == FontSmoothing::SubpixelBGR);
    const bool subPhase = UsesSubpixelPhase(settings.hinting);
    const bool hasKerning = FT_HAS_KERNING(face) != 0;
    const FT_UInt kernMode = subPhase ? FT_KERNING_UNFITTED : FT_KERNING_DEFAULT;

    float penX = posX;
    keel::u32 prevGlyph = 0u;
    keel::usize offset = 0u;

    while (offset < text.size()) {
        const keel::u32 cp = keel::utf8_decode_next(text, offset);
        if (cp == '\n' || cp == '\r') continue;
        if (cp == '\t') {
            const keel::u32 spIdx = FT_Get_Char_Index(face, static_cast<keel::u32>(' '));
            const GlyphEntry& sp = ResolveGlyph(rf, spIdx, sizeQ4, settings.hinting, false, 0u);
            penX += (sp.advancePx > 0.0f) ? (sp.advancePx * 4.0f) : (scaledFontSizePx * 2.4f);
            prevGlyph = 0u;
            continue;
        }

        const keel::u32 glyphIdx = FT_Get_Char_Index(face, cp);
        if (hasKerning && prevGlyph != 0u && glyphIdx != 0u) {
            FT_Vector kern{};
            if (FT_Get_Kerning(face, prevGlyph, glyphIdx, kernMode, &kern) == 0) {
                penX += static_cast<float>(kern.x) / 64.0f;
            }
        }

        keel::i32 cellX = 0;
        keel::u8 phase = 0u;
        if (subPhase) {
            // Quantize penX to nearest quarter-pixel [0, 0.25, 0.5, 0.75]
            const keel::i32 qx = keel::round_i32(penX * 4.0f);
            cellX = qx >> 2;
            phase = static_cast<keel::u8>(qx & 3);
        } else {
            cellX = keel::round_i32(penX);
        }

        const GlyphEntry& g = ResolveGlyph(rf, glyphIdx, sizeQ4, settings.hinting, useSubpixel, phase);
        if (!g.empty && !g.bitmap.empty()) {
            const keel::i32 gx = cellX + g.bearingX;
            const keel::i32 gy = baselineY - g.bearingY;
            if (g.isSubpixel) {
                TextCompositor::BlitSubpixelMask(
                    dstBuffer, dstW, dstH, dstStride,
                    g.bitmap.data(), g.width, g.height, g.strideBytes,
                    gx, gy, clipPx, color, settings, scaledFontSizePx, bgrOrder
                );
            } else {
                TextCompositor::BlitGrayscaleMask(
                    dstBuffer, dstW, dstH, dstStride,
                    g.bitmap.data(), g.width, g.height, g.strideBytes,
                    gx, gy, clipPx, color, settings, scaledFontSizePx
                );
            }
        }

        penX += g.advancePx;
        prevGlyph = glyphIdx;
    }

    return true;
}

bool FreeTypeTextEngine::DrawTextBox(
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
) {
    if (!dstBuffer || text.empty() || scaledFontSizePx <= 0.0f || color.a == 0u) {
        return true;
    }

    const ResolvedFace rf = ResolveFace(style);
    if (!rf.slot) {
        return false;
    }

    const keel::u16 sizeQ4 = QuantizeSizeQ4(scaledFontSizePx);
    if (!EnsureFaceSize(*rf.slot, sizeQ4)) {
        return false;
    }

    FT_Face face = rf.slot->face;
    const float ascPx = (face->units_per_EM > 0u && face->ascender > 0)
        ? (scaledFontSizePx * static_cast<float>(face->ascender) / static_cast<float>(face->units_per_EM))
        : (static_cast<float>(face->size->metrics.ascender) / 64.0f);

    // Measure cap-height ('H') for optically centered vertical placement inside buttons/inputs/tabs
    const keel::u32 capIdx = FT_Get_Char_Index(face, static_cast<keel::u32>('H'));
    const GlyphEntry& capGlyph = ResolveGlyph(rf, capIdx, sizeQ4, settings.hinting, false, 0u);
    const float capH = (!capGlyph.empty && capGlyph.height > 0u)
        ? static_cast<float>(capGlyph.height)
        : (scaledFontSizePx * 0.72f);
    const float capBearingY = (!capGlyph.empty && capGlyph.bearingY > 0)
        ? static_cast<float>(capGlyph.bearingY)
        : capH;

    const float totalW = MeasureText(text, scaledFontSizePx, style, settings.hinting).x;
    const float startX = boundsPx.x + (boundsPx.w - totalW) * alignX;
    const float baselineY = boundsPx.y + (boundsPx.h - capH) * alignY + capBearingY;
    const float topY = baselineY - ascPx;

    return DrawText(
        dstBuffer, dstW, dstH, dstStride,
        text, startX, topY,
        scaledFontSizePx, color, style, clipPx, settings, canvasScale
    );
}

void FreeTypeTextEngine::TrimCaches() noexcept {
    m_glyphMap = {};
    m_glyphNodes = {};
    m_totalGlyphBytes = 0u;
    m_head = kNil;
    m_tail = kNil;
    m_freeHead = kNil;
}

void FreeTypeTextEngine::EvictTail() noexcept {
    if (m_tail == kNil) {
        return;
    }
    const keel::u32 slot = m_tail;
    GlyphNode& victim = m_glyphNodes[slot];
    const keel::usize oldBytes = victim.entry.bitmap.capacity();
    m_totalGlyphBytes = (m_totalGlyphBytes >= oldBytes) ? (m_totalGlyphBytes - oldBytes) : 0u;
    (void)m_glyphMap.remove(victim.key);
    Detach(slot);
    victim.entry.bitmap = keel::Array<keel::u8>{};
    victim.next = m_freeHead;
    m_freeHead = slot;
}

void FreeTypeTextEngine::Detach(keel::u32 idx) noexcept {
    GlyphNode& n = m_glyphNodes[idx];
    if (n.prev != kNil) {
        m_glyphNodes[n.prev].next = n.next;
    } else {
        m_head = n.next;
    }
    if (n.next != kNil) {
        m_glyphNodes[n.next].prev = n.prev;
    } else {
        m_tail = n.prev;
    }
    n.prev = kNil;
    n.next = kNil;
}

void FreeTypeTextEngine::AttachFront(keel::u32 idx) noexcept {
    GlyphNode& n = m_glyphNodes[idx];
    n.prev = kNil;
    n.next = m_head;
    if (m_head != kNil) {
        m_glyphNodes[m_head].prev = idx;
    } else {
        m_tail = idx;
    }
    m_head = idx;
}

void FreeTypeTextEngine::MoveToFront(keel::u32 idx) noexcept {
    if (idx == m_head) {
        return;
    }
    Detach(idx);
    AttachFront(idx);
}

} // namespace guikit
