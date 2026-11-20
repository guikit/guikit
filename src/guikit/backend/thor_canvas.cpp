#include "guikit/backend/thor_canvas.hpp"
#include "guikit/backend/text_compositor.hpp"
#include "guikit/backend/vector_backend.hpp"
#if defined(GUIKIT_HAS_FREETYPE) && GUIKIT_HAS_FREETYPE
#include "freetype_text_engine.hpp"
#endif
#include <cstdlib>
#include <cstring>
#include "guikit/core/assets.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/core/tokens.hpp"
#include "keel/algo.h"
#include "keel/bits.h"
#include "keel/fs.h"
#include "keel/math.h"
#include "keel/utf8.h"
#include "thorvg.h"
#if __has_include("tvgLodePng.h")
#include "tvgLodePng.h"
#define GUIKIT_THORVG_HAS_INTERNAL_LODEPNG 1
#endif

namespace guikit {

namespace {

/// ThorVG's `Text::size()` takes points and scales them by 96/72 internally (`FontLoader::DPI`).
/// GuiKit font sizes are dp (pixels at canvas scale 1), so they are converted to points here;
/// otherwise every glyph renders 33% larger than the theme's typography tokens.
constexpr float kTvgPointsPerPixel = 72.0f / 96.0f;

[[nodiscard]] constexpr float ToTvgFontSize(float px) noexcept {
    return px * kTvgPointsPerPixel;
}

keel::String ExtractFontName(keel::StringRef path) {
    keel::usize lastSep = keel::StringRef::npos;
    for (keel::usize i = 0; i < path.size(); ++i) {
        if (path[i] == '/' || path[i] == '\\') {
            lastSep = i;
        }
    }
    const keel::StringRef base = (lastSep == keel::StringRef::npos) ? path : path.substr(lastSep + 1);
    const keel::usize dot = base.rfind('.');
    if (dot != keel::StringRef::npos && dot > 0) {
        return keel::String::from(base.substr(0, dot));
    }
    return keel::String::from(base);
}

[[nodiscard]] constexpr keel::usize FontStyleSlotIndex(FontStyle style) noexcept {
    const auto idx = static_cast<keel::usize>(style);
    return (idx < 5u) ? idx : 0u;
}

[[nodiscard]] keel::StringRef ClampUtf8Prefix(keel::StringRef text, keel::usize maxBytes) noexcept {
    if (text.size() <= maxBytes) {
        return text;
    }
    keel::usize len = maxBytes;
    while (len > 0 && !keel::utf8_is_lead_byte(static_cast<keel::u8>(text[len]))) {
        --len;
    }
    return text.substr(0, len);
}

struct ScopedCStr {
    char inlineBuf[512];
    keel::String heapFallback{};
    const char* ptr{nullptr};

    explicit ScopedCStr(keel::StringRef sv) {
        if (sv.size() < sizeof(inlineBuf)) {
            if (!sv.empty()) {
                std::memcpy(inlineBuf, sv.data(), sv.size());
            }
            inlineBuf[sv.size()] = '\0';
            ptr = inlineBuf;
        } else {
            heapFallback = keel::String::from(sv);
            ptr = heapFallback.c_str();
        }
    }

    [[nodiscard]] const char* c_str() const noexcept { return ptr; }
};

[[nodiscard]] bool IsStaticEmbeddedAssetPointer(const keel::u8* ptr) noexcept {
    if (!ptr) return false;
    for (const auto& entry : detail::GetEmbeddedAssets()) {
        if (entry.data == ptr) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline keel::u32 AlphaBlendSwar(keel::u32 c, keel::u32 a) noexcept {
    ++a;
    return (((((c >> 8) & 0x00FF00FFu) * a) & 0xFF00FF00u) +
            ((((c & 0x00FF00FFu) * a) >> 8) & 0x00FF00FFu));
}

[[nodiscard]] inline keel::u32 BlendPreNormalSwar(keel::u32 s, keel::u32 d) noexcept {
    const keel::u32 ia = ((~s) >> 24) + 1u;
    return s + (((((d >> 8) & 0x00FF00FFu) * ia) & 0xFF00FF00u) +
                ((((d & 0x00FF00FFu) * ia) >> 8) & 0x00FF00FFu));
}

[[nodiscard]] inline keel::u32 PremultiplyColor(Color c) noexcept {
    if (c.a == 255) {
        return c.ToUint32();
    }
    if (c.a == 0) {
        return 0u;
    }
    const keel::u32 a = c.a;
    const keel::u32 r = (static_cast<keel::u32>(c.r) * a + 127u) / 255u;
    const keel::u32 g = (static_cast<keel::u32>(c.g) * a + 127u) / 255u;
    const keel::u32 b = (static_cast<keel::u32>(c.b) * a + 127u) / 255u;
    return (a << 24) | (b << 16) | (g << 8) | r;
}

} // namespace

ThorCanvas::ThorCanvas(bool loadDefaultFonts) {
    // Use 0 worker threads (synchronous main-thread rasterization) to eliminate
    // multi-core mutex contention and condition_variable wakeups on UI primitives.
    tvg::Initializer::init(0);
    m_canvas = tvg::SwCanvas::gen(tvg::EngineOption::None);
#if defined(GUIKIT_HAS_FREETYPE) && GUIKIT_HAS_FREETYPE
    m_ftEngine = new FreeTypeTextEngine();
    if (!m_ftEngine->IsAvailable()) {
        delete m_ftEngine;
        m_ftEngine = nullptr;
    }
#endif

    if (!loadDefaultFonts) {
        return;
    }

    // Auto-detect primary UI font across embedded assets and system locations
    const keel::StringRef defaultFontCandidates[] = {
        "assets/fonts/Roboto-Regular.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf"
    };
    for (keel::StringRef candidate : defaultFontCandidates) {
        if (LoadFontFile(candidate, FontStyle::Regular)) {
            break;
        }
    }

    // Auto-detect monospace font for code editors, Markdown code blocks & telemetry
    const keel::StringRef monoFontCandidates[] = {
        "assets/fonts/JetBrainsMono-Regular.ttf",
        "assets/fonts/RobotoMono-Regular.ttf",
        "C:/Windows/Fonts/CascadiaMono.ttf",
        "C:/Windows/Fonts/CascadiaCode.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/cour.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/System/Library/Fonts/Menlo.ttc"
    };
    for (keel::StringRef candidate : monoFontCandidates) {
        if (LoadFontFile(candidate, FontStyle::Monospace)) {
            break;
        }
    }
}

ThorCanvas::~ThorCanvas() {
#if defined(GUIKIT_HAS_FREETYPE) && GUIKIT_HAS_FREETYPE
    delete m_ftEngine;
    m_ftEngine = nullptr;
#endif
    if (m_measureText) {
        m_measureText->unref();
        m_measureText = nullptr;
    }
    if (m_stampCanvas) {
        m_stampCanvas->sync();
        delete m_stampCanvas;
        m_stampCanvas = nullptr;
    }
    if (m_canvas) {
        m_canvas->sync();
        delete m_canvas;
        m_canvas = nullptr;
    }
    tvg::Initializer::term();
}

void ThorCanvas::FlushCanvas() {
    if (m_canvas && m_hasPendingPaints) {
        m_canvas->draw(false);
        m_canvas->sync();
        m_canvas->remove();
        m_canvas->sync();
        m_hasPendingPaints = false;
    }
}

void ThorCanvas::TrimCaches() noexcept {
    FlushCanvas();
    m_textMeasureCache.clear();
    m_textStampCache.Clear();
    m_shapeStampCache.Clear();
    m_imageCache = {};
    m_imageCacheBytes = 0;
    m_stampScratch = {};
    m_subpixelRowScratch = {};
    m_filteredMaskScratch = {};
#if defined(GUIKIT_HAS_FREETYPE) && GUIKIT_HAS_FREETYPE
    if (m_ftEngine) {
        m_ftEngine->TrimCaches();
    }
#endif
}

keel::StringRef ThorCanvas::GetTextEngineName() const noexcept {
#if defined(GUIKIT_HAS_FREETYPE) && GUIKIT_HAS_FREETYPE
    if (m_ftEngine && m_ftEngine->IsAvailable()) {
        return "FreeType";
    }
#endif
    return "ThorVG";
}

bool ThorCanvas::SupportsFontHinting() const noexcept {
#if defined(GUIKIT_HAS_FREETYPE) && GUIKIT_HAS_FREETYPE
    return m_ftEngine && m_ftEngine->IsAvailable();
#else
    return false;
#endif
}

void ThorCanvas::SetTextRenderSettings(const TextRenderSettings& settings) noexcept {
    TextRenderSettings clamped = settings;
    clamped.contrast = keel::clamp(clamped.contrast, 0.5f, 2.0f);
    clamped.stemDarkening = keel::clamp(clamped.stemDarkening, 0.0f, 0.5f);
    if (clamped.hinting != m_textSettings.hinting) {
        m_textMeasureCache.clear();
    }
    m_textSettings = clamped;
}

void ThorCanvas::SyncViewportClip() {
    if (!m_canvas || m_width == 0 || m_height == 0) return;
    FlushCanvas();
    if (m_clipStack.empty()) {
        m_canvas->viewport(0, 0, static_cast<keel::i32>(m_width), static_cast<keel::i32>(m_height));
    } else {
        const Rect& cr = m_clipStack.back();
        const keel::i32 x0 = keel::clamp(keel::floor_i32(cr.x), 0, static_cast<keel::i32>(m_width));
        const keel::i32 y0 = keel::clamp(keel::floor_i32(cr.y), 0, static_cast<keel::i32>(m_height));
        const keel::i32 x1 = keel::clamp(keel::ceil_i32(cr.Right()), 0, static_cast<keel::i32>(m_width));
        const keel::i32 y1 = keel::clamp(keel::ceil_i32(cr.Bottom()), 0, static_cast<keel::i32>(m_height));
        m_canvas->viewport(x0, y0, keel::max(0, x1 - x0), keel::max(0, y1 - y0));
    }
}

void ThorCanvas::SetTarget(keel::u32* buffer, keel::u32 width, keel::u32 height, keel::u32 stride) {
    m_targetBuffer = buffer;
    m_width = width;
    m_height = height;
    m_stride = stride;
    m_hasPendingPaints = false;

    if (m_canvas && m_targetBuffer && m_width > 0 && m_height > 0) {
        m_canvas->target(m_targetBuffer, m_stride, m_width, m_height, tvg::ColorSpace::ABGR8888);
    }
}

bool ThorCanvas::LoadFontMemoryInternal(
    keel::StringRef name,
    keel::ArrayRef<keel::u8> data,
    FontStyle style,
    keel::StringRef mimeType,
    bool copyData
) {
    if (name.empty() || data.empty()) return false;

    const keel::SmallString<128> fontName(name);
    const keel::SmallString<32> mimeStr(mimeType.empty() ? keel::StringRef("ttf") : mimeType);
    if (tvg::Text::load(
            fontName.c_str(),
            reinterpret_cast<const char*>(data.data()),
            static_cast<keel::u32>(data.size()),
            mimeStr.c_str(),
            copyData) != tvg::Result::Success) {
        return false;
    }

    FontSlot& slot = m_fonts[FontStyleSlotIndex(style)];
    slot.name.assign(name);
    slot.loaded = true;
#if defined(GUIKIT_HAS_FREETYPE) && GUIKIT_HAS_FREETYPE
    if (m_ftEngine && m_ftEngine->IsAvailable()) {
        (void)m_ftEngine->LoadFontMemory(data, style, copyData);
    }
#endif
    m_textMeasureCache.clear();
    m_textStampCache.Clear();
    return true;
}

bool ThorCanvas::LoadFontMemory(
    keel::StringRef name,
    keel::ArrayRef<keel::u8> data,
    FontStyle style,
    keel::StringRef mimeType
) {
    return LoadFontMemoryInternal(name, data, style, mimeType, true);
}

bool ThorCanvas::LoadFontFile(keel::StringRef path, FontStyle style) {
    if (path.empty()) return false;

    // 1. Check virtual / build-time embedded asset registry first (Native + WebAssembly)
    if (auto asset = Assets::Find(path); asset && !asset->Empty()) {
        const keel::String fontName = ExtractFontName(path);
        const keel::StringRef mime = asset->mimeType.empty() ? Assets::InferMimeType(path) : asset->mimeType;
        const bool copyData = !IsStaticEmbeddedAssetPointer(asset->data);
        if (LoadFontMemoryInternal(fontName, asset->Bytes(), style, mime, copyData)) {
            return true;
        }
    }

    // 2. Check disk (UTF-8 aware path resolution, probing direct path and relative parent prefixes)
    const keel::String fontName = ExtractFontName(path);
    const keel::StringRef mime = Assets::InferMimeType(path);
    const bool isAbsolute = keel::path_is_absolute(path);
    const keel::StringRef prefixes[] = {"", "../", "../../", "../../../"};

    for (keel::StringRef prefix : prefixes) {
        if (isAbsolute && !prefix.empty()) break;
        keel::SmallString<256> candidate(prefix);
        candidate.append(path);
        auto res = keel::fs_read_file(candidate);
        if (!res.is_ok() || res.value().empty()) continue;
        const auto& bytes = res.value();
        if (LoadFontMemoryInternal(fontName, bytes, style, mime, true)) {
            return true;
        }
    }

    return false;
}

bool ThorCanvas::NeedsSyntheticBold(FontStyle style) const noexcept {
    if (style == FontStyle::Bold) {
        return !m_fonts[FontStyleSlotIndex(FontStyle::Bold)].loaded;
    }
    if (style == FontStyle::BoldItalic) {
        return !m_fonts[FontStyleSlotIndex(FontStyle::BoldItalic)].loaded &&
               !m_fonts[FontStyleSlotIndex(FontStyle::Bold)].loaded;
    }
    return false;
}

bool ThorCanvas::NeedsSyntheticItalic(FontStyle style) const noexcept {
    if (style == FontStyle::Italic) {
        return !m_fonts[FontStyleSlotIndex(FontStyle::Italic)].loaded;
    }
    if (style == FontStyle::BoldItalic) {
        return !m_fonts[FontStyleSlotIndex(FontStyle::BoldItalic)].loaded &&
               !m_fonts[FontStyleSlotIndex(FontStyle::Italic)].loaded;
    }
    return false;
}

void ThorCanvas::ApplyFont(tvg::Text* txt, FontStyle style) const {
    if (!txt) return;

    const FontSlot& primary = m_fonts[FontStyleSlotIndex(style)];
    if (primary.loaded && !primary.name.empty() && txt->font(primary.name.c_str()) == tvg::Result::Success) {
        return;
    }

    if (style == FontStyle::BoldItalic) {
        const FontSlot& boldSlot = m_fonts[FontStyleSlotIndex(FontStyle::Bold)];
        if (boldSlot.loaded && !boldSlot.name.empty() && txt->font(boldSlot.name.c_str()) == tvg::Result::Success) {
            return;
        }
        const FontSlot& italicSlot = m_fonts[FontStyleSlotIndex(FontStyle::Italic)];
        if (italicSlot.loaded && !italicSlot.name.empty() && txt->font(italicSlot.name.c_str()) == tvg::Result::Success) {
            return;
        }
    }

    const FontSlot& regular = m_fonts[FontStyleSlotIndex(FontStyle::Regular)];
    if (regular.loaded && !regular.name.empty() && txt->font(regular.name.c_str()) == tvg::Result::Success) {
        return;
    }
    txt->font(nullptr);
}

void ThorCanvas::Begin(keel::u32 width, keel::u32 height, float scale) {
    m_scale = (scale > 0.0f) ? scale : 1.0f;
    m_offset = {0.0f, 0.0f};
    m_transformStack.clear();
    m_clipStack.clear();
    m_gpuClipStack.clear();
    if (m_width != width || m_height != height) {
        m_width = width;
        m_height = height;
        m_hasPendingPaints = false;
        if (m_targetBuffer && m_canvas) {
            m_canvas->target(m_targetBuffer, m_stride, m_width, m_height, tvg::ColorSpace::ABGR8888);
        }
    } else {
        SyncViewportClip();
    }
}

void ThorCanvas::PushTransform(const Transform2D& transform) {
    m_transformStack.push_back(TransformState{m_scale, m_offset});
    // transform.Apply(p), then the current mapping p * m_scale + m_offset
    const Transform2D current{m_scale, m_offset};
    const Transform2D combined = current.Then(transform);
    m_scale = combined.scale;
    m_offset = combined.offset;
}

void ThorCanvas::PopTransform() {
    if (m_transformStack.empty()) {
        return;
    }
    const TransformState prev = m_transformStack.back();
    m_transformStack.pop_back();
    m_scale = prev.scale;
    m_offset = prev.offset;
}

void ThorCanvas::End() {
    FlushCanvas();
}

void ThorCanvas::Clear(Color color, keel::Optional<Rect> rect) {
    if (!m_targetBuffer || m_width == 0 || m_height == 0) return;

    FlushCanvas();
    const keel::u32 rawColor = color.ToUint32();

    if (!rect.has_value()) {
        m_gpuPasses.clear();
        for (keel::u32 y = 0; y < m_height; ++y) {
            keel::fill(keel::MutArrayRef<keel::u32>(m_targetBuffer + y * m_stride, m_width), rawColor);
        }
    } else {
        const Rect sClear(Px(*rect));
        auto isCoveredByClear = [&](const Rect& clipR) noexcept {
            if (clipR.w <= 0.5f || clipR.h <= 0.5f) return true;
            return sClear.x <= clipR.x + 1.0f &&
                   sClear.y <= clipR.y + 1.0f &&
                   sClear.Right() >= clipR.Right() - 1.0f &&
                   sClear.Bottom() >= clipR.Bottom() - 1.0f;
        };
        keel::usize writeIdx = 0;
        for (keel::usize i = 0; i < m_gpuPasses.size(); ++i) {
            if (!isCoveredByClear(m_gpuPasses[i].clipRect)) {
                if (writeIdx != i) {
                    m_gpuPasses[writeIdx] = keel::move(m_gpuPasses[i]);
                }
                ++writeIdx;
            }
        }
        m_gpuPasses.resize(writeIdx);

        const keel::i32 startX = keel::max(0, keel::floor_i32(sClear.x));
        const keel::i32 startY = keel::max(0, keel::floor_i32(sClear.y));
        const keel::i32 endX = keel::min(static_cast<keel::i32>(m_width), keel::ceil_i32(sClear.Right()));
        const keel::i32 endY = keel::min(static_cast<keel::i32>(m_height), keel::ceil_i32(sClear.Bottom()));
        const keel::i32 w = endX - startX;
        if (w > 0 && endY > startY) {
            for (keel::i32 y = startY; y < endY; ++y) {
                keel::fill(
                    keel::MutArrayRef<keel::u32>(m_targetBuffer + y * m_stride + startX, static_cast<keel::usize>(w)),
                    rawColor
                );
            }
        }
    }
}

void ThorCanvas::PushClip(Rect clipRect) {
    const float sx1 = keel::floor(PxX(clipRect.x));
    const float sy1 = keel::floor(PxY(clipRect.y));
    const float sx2 = keel::ceil(PxX(clipRect.Right()));
    const float sy2 = keel::ceil(PxY(clipRect.Bottom()));
    const Rect sClip(sx1, sy1, keel::max(0.0f, sx2 - sx1), keel::max(0.0f, sy2 - sy1));
    if (!m_clipStack.empty()) {
        const Rect& parent = m_clipStack.back();
        const float x1 = keel::max(parent.x, sClip.x);
        const float y1 = keel::max(parent.y, sClip.y);
        const float x2 = keel::min(parent.Right(), sClip.Right());
        const float y2 = keel::min(parent.Bottom(), sClip.Bottom());
        m_clipStack.emplace_back(x1, y1, keel::max(0.0f, x2 - x1), keel::max(0.0f, y2 - y1));

        const Rect& gpuParent = m_gpuClipStack.back();
        const float gx1 = keel::max(gpuParent.x, sClip.x);
        const float gy1 = keel::max(gpuParent.y, sClip.y);
        const float gx2 = keel::min(gpuParent.Right(), sClip.Right());
        const float gy2 = keel::min(gpuParent.Bottom(), sClip.Bottom());
        m_gpuClipStack.emplace_back(gx1, gy1, keel::max(0.0f, gx2 - gx1), keel::max(0.0f, gy2 - gy1));
    } else {
        m_clipStack.push_back(sClip);
        // Root clip (depth 0) is the frame's dirty-rect damage clip; GPU underlays must cover the full viewport
        m_gpuClipStack.emplace_back(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
    }
    SyncViewportClip();
}

void ThorCanvas::PopClip() {
    if (!m_clipStack.empty()) {
        m_clipStack.pop_back();
    }
    if (!m_gpuClipStack.empty()) {
        m_gpuClipStack.pop_back();
    }
    SyncViewportClip();
}

void ThorCanvas::ApplyClip(tvg::Paint* paint, keel::Optional<Rect> bounds) {
    // Hardware/engine integer viewport clipping is synchronized on PushClip/PopClip via m_canvas->viewport(),
    // so per-paint RLE clip masks are never needed for rectangular stack clips.
    (void)paint;
    (void)bounds;
}

void ThorCanvas::DrawRect(Rect rect, Color fill) {
    if (fill.a == 0 || rect.w <= 0.0f || rect.h <= 0.0f || !m_canvas) return;
    Rect sRect(Px(rect));
    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        if (cr.w <= 0.0f || cr.h <= 0.0f) return;
        sRect = sRect.Intersect(cr);
        if (sRect.w <= 0.0f || sRect.h <= 0.0f) return;
    }

    if (m_targetBuffer && m_width > 0 && m_height > 0) {
        const keel::i32 x0 = keel::clamp(keel::round_i32(sRect.x), 0, static_cast<keel::i32>(m_width));
        const keel::i32 y0 = keel::clamp(keel::round_i32(sRect.y), 0, static_cast<keel::i32>(m_height));
        const keel::i32 x1 = keel::clamp(keel::round_i32(sRect.Right()), 0, static_cast<keel::i32>(m_width));
        const keel::i32 y1 = keel::clamp(keel::round_i32(sRect.Bottom()), 0, static_cast<keel::i32>(m_height));
        const keel::i32 w = x1 - x0;
        if (w > 0 && y1 > y0) {
            FlushCanvas();
            if (fill.a == 255) {
                const keel::u32 rawCol = fill.ToUint32();
                for (keel::i32 y = y0; y < y1; ++y) {
                    keel::fill(
                        keel::MutArrayRef<keel::u32>(
                            m_targetBuffer + static_cast<keel::usize>(y) * m_stride + x0,
                            static_cast<keel::usize>(w)
                        ),
                        rawCol
                    );
                }
            } else {
                const keel::u32 sPre = PremultiplyColor(fill);
                for (keel::i32 y = y0; y < y1; ++y) {
                    keel::u32* dst = m_targetBuffer + static_cast<keel::usize>(y) * m_stride + x0;
                    for (keel::i32 x = 0; x < w; ++x) {
                        dst[x] = BlendPreNormalSwar(sPre, dst[x]);
                    }
                }
            }
        }
        return;
    }

    auto shape = tvg::Shape::gen();
    shape->appendRect(sRect.x, sRect.y, sRect.w, sRect.h);
    shape->fill(fill.r, fill.g, fill.b, fill.a);
    m_canvas->add(shape);
    m_hasPendingPaints = true;
}

void ThorCanvas::DrawRoundRect(
    Rect rect,
    float radius,
    Color fill,
    Color stroke,
    float strokeWidth
) {
    if (rect.w <= 0.0f || rect.h <= 0.0f || !m_canvas) return;
    if (fill.a == 0 && (stroke.a == 0 || strokeWidth <= 0.0f)) return;

    const float sRadius = keel::max(0.0f, radius * m_scale);
    if (sRadius <= 0.05f) {
        if (stroke.a == 0 || strokeWidth <= 0.0f) {
            DrawRect(rect, fill);
            return;
        }
        if (fill.a > 0) {
            DrawRect(rect, fill);
        }
        // Snap to the pixel grid in logical units (through the current transform)
        const float rx0 = FromPxX(keel::round(PxX(rect.x)));
        const float ry0 = FromPxY(keel::round(PxY(rect.y)));
        const float rx1 = FromPxX(keel::round(PxX(rect.Right())));
        const float ry1 = FromPxY(keel::round(PxY(rect.Bottom())));
        const float hw = strokeWidth * 0.5f;
        const float maxRight = (m_width > 0)
            ? (FromPxX(static_cast<float>(m_width)) - strokeWidth)
            : (rx1 - hw);
        const float maxBottom = (m_height > 0)
            ? (FromPxY(static_cast<float>(m_height)) - strokeWidth)
            : (ry1 - hw);
        const float x0 = (m_width > 0) ? keel::max(FromPxX(0.0f), rx0 - hw) : (rx0 - hw);
        const float y0 = (m_height > 0) ? keel::max(FromPxY(0.0f), ry0 - hw) : (ry0 - hw);
        const float xR = keel::min(rx1 - hw, maxRight);
        const float yB = keel::min(ry1 - hw, maxBottom);
        const float w = keel::max(strokeWidth, (xR - x0) + strokeWidth);
        const float h = keel::max(strokeWidth, (yB - y0) + strokeWidth);
        DrawRect(Rect(x0, y0, w, strokeWidth), stroke);
        DrawRect(Rect(x0, yB, w, strokeWidth), stroke);
        if (h > strokeWidth * 2.0f) {
            DrawRect(Rect(x0, y0 + strokeWidth, strokeWidth, h - strokeWidth * 2.0f), stroke);
            DrawRect(Rect(xR, y0 + strokeWidth, strokeWidth, h - strokeWidth * 2.0f), stroke);
        }
        return;
    }

    const Rect sRect(Px(rect));
    const float sStrokeW = (stroke.a > 0 && strokeWidth > 0.0f) ? (strokeWidth * m_scale) : 0.0f;
    const float pad = (sStrokeW > 0.0f) ? (sStrokeW * 0.5f + 1.0f) : 1.0f;
    const Rect outerBounds(sRect.x - pad, sRect.y - pad, sRect.w + pad * 2.0f, sRect.h + pad * 2.0f);
    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        if (cr.w <= 0.0f || cr.h <= 0.0f || !outerBounds.Intersects(cr)) return;
    }

    if (m_targetBuffer && m_width > 0 && m_height > 0 &&
        ((sRect.w <= 180.0f && sRect.h <= 52.0f) || (sRect.w <= 72.0f && sRect.h <= 72.0f))) {
        const keel::i32 qx = keel::round_i32(sRect.x * 4.0f);
        const keel::i32 qy = keel::round_i32(sRect.y * 4.0f);
        const keel::i32 dstX = keel::floor_i32(static_cast<float>(qx) * 0.25f);
        const keel::i32 dstY = keel::floor_i32(static_cast<float>(qy) * 0.25f);
        const keel::u8 phaseX = static_cast<keel::u8>(qx - dstX * 4);
        const keel::u8 phaseY = static_cast<keel::u8>(qy - dstY * 4);
        const ShapeStampEntry& stamp = ResolveShapeStamp(sRect.w, sRect.h, sRadius, fill, stroke, sStrokeW, phaseX, phaseY);
        BlitShapeStamp(stamp, dstX, dstY);
        return;
    }

    auto shape = tvg::Shape::gen();
    shape->appendRect(sRect.x, sRect.y, sRect.w, sRect.h, sRadius, sRadius);

    if (fill.a > 0) {
        shape->fill(fill.r, fill.g, fill.b, fill.a);
    }

    if (sStrokeW > 0.0f) {
        shape->strokeWidth(sStrokeW);
        shape->strokeFill(stroke.r, stroke.g, stroke.b, stroke.a);
    }

    m_canvas->add(shape);
    m_hasPendingPaints = true;
}

Vec2 ThorCanvas::MeasureText(keel::StringRef text, float fontSize) {
    return MeasureTextStyled(text, fontSize, FontStyle::Regular);
}

Vec2 ThorCanvas::MeasureTextStyled(keel::StringRef text, float fontSize, FontStyle style) {
    if (text.empty() || fontSize <= 0.0f) {
        return {0.0f, 0.0f};
    }

    keel::u64 h = 14695981039346656037ULL;
    for (char c : text) {
        h = (h ^ static_cast<keel::u8>(c)) * 1099511628211ULL;
    }
    keel::u32 fsBits = 0;
    std::memcpy(&fsBits, &fontSize, sizeof(float));
    const TextMeasureKey key{h, fsBits, static_cast<keel::u8>(style)};

    if (const Vec2* cached = m_textMeasureCache.get(key)) {
        return *cached;
    }

#if defined(GUIKIT_HAS_FREETYPE) && GUIKIT_HAS_FREETYPE
    if (m_ftEngine && m_ftEngine->HasFontForStyle(style)) {
        const Vec2 result = m_ftEngine->MeasureText(text, fontSize, style, m_textSettings.hinting);
        m_textMeasureCache.put(key, result);
        return result;
    }
#endif

    if (!m_measureText) {
        m_measureText = tvg::Text::gen();
    }
    ApplyFont(m_measureText, style);

    m_measureText->size(ToTvgFontSize(fontSize));
    const ScopedCStr textStr(text);
    m_measureText->text(textStr.c_str());

    float totalAdvance = 0.0f;
    const char* cur = textStr.c_str();
    while (*cur) {
        if (*cur == '\t') {
            tvg::GlyphMetrics spGm{};
            const char* spNext = nullptr;
            if (m_measureText->metrics(" ", spGm, &spNext) == tvg::Result::Success) {
                totalAdvance += spGm.advance * 4.0f;
            } else {
                totalAdvance += fontSize * 2.4f;
            }
            cur++;
            continue;
        }
        tvg::GlyphMetrics gm{};
        const char* next = nullptr;
        if (m_measureText->metrics(cur, gm, &next) == tvg::Result::Success && next) {
            totalAdvance += gm.advance;
            cur = next;
        } else {
            totalAdvance += fontSize * 0.60f;
            cur++;
        }
    }
    if (NeedsSyntheticBold(style)) {
        totalAdvance += 0.6f;
    }
    if (NeedsSyntheticItalic(style)) {
        totalAdvance += fontSize * 0.12f;
    }

    float glyphBoxH = GetLineHeight(fontSize);
    tvg::TextMetrics tm{};
    if (m_measureText->metrics(tm) == tvg::Result::Success && (tm.ascent - tm.descent) > 0.0f) {
        glyphBoxH = tm.ascent - tm.descent;
    }

    const Vec2 result{totalAdvance, glyphBoxH};
    m_textMeasureCache.put(key, result);
    return result;
}

float ThorCanvas::GetLineHeight(float fontSize) {
    if (fontSize <= 0.0f) return 0.0f;
    return fontSize * kDefaultLineHeightMultiplier;
}

const ThorCanvas::TextMaskEntry& ThorCanvas::ResolveTextMask(
    keel::StringRef text,
    float scaledFontSize,
    FontStyle style,
    bool subpixel
) {
    keel::u64 h = 14695981039346656037ULL;
    for (char c : text) {
        h = (h ^ static_cast<keel::u8>(c)) * 1099511628211ULL;
    }
    const float qFontSize = keel::max(1.0f, keel::round(scaledFontSize * 4.0f) * 0.25f);
    keel::u32 fsBits = 0;
    std::memcpy(&fsBits, &qFontSize, sizeof(float));

    const Vec2 m = MeasureTextStyled(text, qFontSize, style);
    const keel::i32 padLeft = subpixel ? 4 : 3;
    const keel::i32 padTop = keel::max(3, keel::ceil_i32(qFontSize * 0.25f));
    const keel::i32 padRight = keel::max(6, keel::ceil_i32(qFontSize * 0.35f)) + (subpixel ? 1 : 0);
    const keel::i32 padBottom = keel::max(4, keel::ceil_i32(qFontSize * 0.40f));
    const keel::i32 rawBufW = keel::ceil_i32(m.x) + padLeft + padRight;
    // Gracefully fall back to grayscale on ultra-wide lines (> 1364 px) so 3x scratch stays bounded
    const bool effSubpixel = subpixel && (rawBufW <= 1364);

    const TextMaskKey key{
        .textHash = h,
        .fontSizeBits = fsBits,
        .textLen = static_cast<keel::u16>(keel::min<keel::usize>(text.size(), 65535u)),
        .style = static_cast<keel::u8>(style),
        .subpixel = static_cast<keel::u8>(effSubpixel ? 1u : 0u)
    };

    if (const TextMaskEntry* cached = m_textStampCache.Get(key)) {
        return *cached;
    }

    TextMaskEntry entry{};
    entry.isSubpixel = effSubpixel;
    if (m.x <= 0.0f || m.y <= 0.0f) {
        entry.empty = true;
        return m_textStampCache.Insert(key, keel::move(entry));
    }

    const keel::i32 bufW = keel::clamp(rawBufW, 4, effSubpixel ? 1364 : 2048);
    const keel::i32 bufH = keel::clamp(keel::ceil_i32(m.y) + padTop + padBottom, 4, 256);
    const keel::i32 rasterW = effSubpixel ? (bufW * 3) : bufW;

    if (!m_stampCanvas) {
        m_stampCanvas = tvg::SwCanvas::gen(tvg::EngineOption::None);
    }

    m_stampScratch.assign(static_cast<keel::usize>(rasterW) * static_cast<keel::usize>(bufH), 0u);
    m_stampCanvas->target(
        m_stampScratch.data(),
        static_cast<keel::u32>(rasterW),
        static_cast<keel::u32>(rasterW),
        static_cast<keel::u32>(bufH),
        tvg::ColorSpace::ABGR8888
    );

    const bool isItalic = NeedsSyntheticItalic(style);
    const bool isBold = NeedsSyntheticBold(style);
    const ScopedCStr textStr(text);

    auto addStampPass = [&](float offsetX) {
        auto* txt = tvg::Text::gen();
        txt->text(textStr.c_str());
        ApplyFont(txt, style);
        txt->size(ToTvgFontSize(qFontSize));
        if (isItalic) {
            txt->italic(0.18f);
        }
        // Rasterize pure opaque white so pixel alpha holds exact 8-bit outline coverage
        txt->fill(255, 255, 255);
        txt->opacity(255);
        txt->align(0.0f, 0.0f);
        if (effSubpixel) {
            const tvg::Matrix mat{
                3.0f, 0.0f, (static_cast<float>(padLeft) + offsetX) * 3.0f,
                0.0f, 1.0f, static_cast<float>(padTop),
                0.0f, 0.0f, 1.0f
            };
            txt->transform(mat);
        } else {
            txt->translate(static_cast<float>(padLeft) + offsetX, static_cast<float>(padTop));
        }
        m_stampCanvas->add(txt);
    };

    addStampPass(0.0f);
    if (isBold) {
        addStampPass(0.55f * m_scale);
    }

    m_stampCanvas->draw(false);
    m_stampCanvas->sync();
    m_stampCanvas->remove();
    m_stampCanvas->sync();

    if (effSubpixel) {
        const keel::usize rowSubBytes = static_cast<keel::usize>(bufW) * 3u;
        m_subpixelRowScratch.resize(rowSubBytes);
        m_filteredMaskScratch.resize(rowSubBytes * static_cast<keel::usize>(bufH));

        keel::i32 minX = bufW;
        keel::i32 minY = bufH;
        keel::i32 maxX = -1;
        keel::i32 maxY = -1;

        for (keel::i32 y = 0; y < bufH; ++y) {
            const keel::u32* srcRow = m_stampScratch.data() + static_cast<keel::usize>(y) * static_cast<keel::usize>(rasterW);
            bool anyNonZero = false;
            for (keel::i32 sx = 0; sx < rasterW; ++sx) {
                const keel::u8 a = static_cast<keel::u8>(srcRow[sx] >> 24);
                m_subpixelRowScratch[static_cast<keel::usize>(sx)] = a;
                if (a != 0u) anyNonZero = true;
            }
            keel::u8* dstFilteredRow = m_filteredMaskScratch.data() + static_cast<keel::usize>(y) * rowSubBytes;
            if (!anyNonZero) {
                std::memset(dstFilteredRow, 0, rowSubBytes);
                continue;
            }
            TextCompositor::FilterSubpixel3xRow(
                m_subpixelRowScratch.data(),
                dstFilteredRow,
                static_cast<keel::u32>(bufW)
            );
            for (keel::i32 px = 0; px < bufW; ++px) {
                const keel::u8* rgb = dstFilteredRow + static_cast<keel::usize>(px) * 3u;
                if ((rgb[0] | rgb[1] | rgb[2]) != 0u) {
                    if (px < minX) minX = px;
                    if (px > maxX) maxX = px;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        }

        if (maxX < minX || maxY < minY) {
            entry.empty = true;
        } else {
            const keel::i32 cropW = maxX - minX + 1;
            const keel::i32 cropH = maxY - minY + 1;
            entry.width = static_cast<keel::u16>(cropW);
            entry.height = static_cast<keel::u16>(cropH);
            entry.offsetX = static_cast<keel::i16>(minX - padLeft);
            entry.offsetY = static_cast<keel::i16>(minY - padTop);
            const keel::usize cropRowBytes = static_cast<keel::usize>(cropW) * 3u;
            entry.mask.resize(cropRowBytes * static_cast<keel::usize>(cropH));
            for (keel::i32 row = 0; row < cropH; ++row) {
                std::memcpy(
                    entry.mask.data() + static_cast<keel::usize>(row) * cropRowBytes,
                    m_filteredMaskScratch.data() + static_cast<keel::usize>(minY + row) * rowSubBytes + static_cast<keel::usize>(minX) * 3u,
                    cropRowBytes
                );
            }
        }
    } else {
        keel::i32 minX = bufW;
        keel::i32 minY = bufH;
        keel::i32 maxX = -1;
        keel::i32 maxY = -1;
        for (keel::i32 y = 0; y < bufH; ++y) {
            const keel::u32* row = m_stampScratch.data() + static_cast<keel::usize>(y) * bufW;
            for (keel::i32 x = 0; x < bufW; ++x) {
                if ((row[x] >> 24) != 0u) {
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                }
            }
        }

        if (maxX < minX || maxY < minY) {
            entry.empty = true;
        } else {
            const keel::i32 cropW = maxX - minX + 1;
            const keel::i32 cropH = maxY - minY + 1;
            entry.width = static_cast<keel::u16>(cropW);
            entry.height = static_cast<keel::u16>(cropH);
            entry.offsetX = static_cast<keel::i16>(minX - padLeft);
            entry.offsetY = static_cast<keel::i16>(minY - padTop);
            entry.mask.resize(static_cast<keel::usize>(cropW) * static_cast<keel::usize>(cropH));
            for (keel::i32 row = 0; row < cropH; ++row) {
                const keel::u32* srcRow = m_stampScratch.data() + static_cast<keel::usize>(minY + row) * bufW + minX;
                keel::u8* dstRow = entry.mask.data() + static_cast<keel::usize>(row) * static_cast<keel::usize>(cropW);
                for (keel::i32 x = 0; x < cropW; ++x) {
                    dstRow[x] = static_cast<keel::u8>(srcRow[x] >> 24);
                }
            }
        }
    }

    if (m_stampScratch.capacity() > 65536u) {
        m_stampScratch = keel::Array<keel::u32>{};
    }
    if (m_filteredMaskScratch.capacity() > 131072u) {
        m_filteredMaskScratch = keel::Array<keel::u8>{};
    }

    return m_textStampCache.Insert(key, keel::move(entry));
}

const ThorCanvas::ShapeStampEntry& ThorCanvas::ResolveShapeStamp(
    float scaledW,
    float scaledH,
    float scaledRadius,
    Color fill,
    Color stroke,
    float scaledStrokeWidth,
    keel::u8 phaseX,
    keel::u8 phaseY
) {
    const keel::u16 qW = static_cast<keel::u16>(keel::clamp(keel::round_i32(scaledW * 4.0f), 1, 4096));
    const keel::u16 qH = static_cast<keel::u16>(keel::clamp(keel::round_i32(scaledH * 4.0f), 1, 4096));
    const keel::u16 qRad = static_cast<keel::u16>(keel::clamp(keel::round_i32(scaledRadius * 4.0f), 0, 4096));
    const keel::u16 qStrokeW = (stroke.a > 0 && scaledStrokeWidth > 0.0f)
        ? static_cast<keel::u16>(keel::clamp(keel::round_i32(scaledStrokeWidth * 4.0f), 1, 1024))
        : 0u;
    const keel::u8 pxPhase = static_cast<keel::u8>(phaseX & 3u);
    const keel::u8 pyPhase = static_cast<keel::u8>(phaseY & 3u);
    const keel::u8 packedPhase = static_cast<keel::u8>((pyPhase << 2) | pxPhase);

    const ShapeStampKey key{
        .fillRgba = (fill.a > 0) ? fill.ToUint32() : 0u,
        .strokeRgba = (qStrokeW > 0) ? stroke.ToUint32() : 0u,
        .qW = qW,
        .qH = qH,
        .qRad = qRad,
        .qStrokeW = qStrokeW,
        .subpixelPhase = packedPhase
    };

    if (const ShapeStampEntry* cached = m_shapeStampCache.Get(key)) {
        return *cached;
    }

    ShapeStampEntry entry{};
    const float w = static_cast<float>(qW) * 0.25f;
    const float h = static_cast<float>(qH) * 0.25f;
    const float rad = static_cast<float>(qRad) * 0.25f;
    const float sw = static_cast<float>(qStrokeW) * 0.25f;
    const float subX = static_cast<float>(pxPhase) * 0.25f;
    const float subY = static_cast<float>(pyPhase) * 0.25f;

    const keel::i32 pad = keel::ceil_i32(sw * 0.5f) + 2;
    const keel::i32 bufW = keel::clamp(keel::ceil_i32(w + subX) + pad * 2, 4, 512);
    const keel::i32 bufH = keel::clamp(keel::ceil_i32(h + subY) + pad * 2, 4, 256);

    if (!m_stampCanvas) {
        m_stampCanvas = tvg::SwCanvas::gen(tvg::EngineOption::None);
    }

    m_stampScratch.assign(static_cast<keel::usize>(bufW) * static_cast<keel::usize>(bufH), 0u);
    m_stampCanvas->target(m_stampScratch.data(), static_cast<keel::u32>(bufW), static_cast<keel::u32>(bufW), static_cast<keel::u32>(bufH), tvg::ColorSpace::ABGR8888);

    auto* shape = tvg::Shape::gen();
    shape->appendRect(static_cast<float>(pad) + subX, static_cast<float>(pad) + subY, w, h, rad, rad);
    if (fill.a > 0) {
        shape->fill(fill.r, fill.g, fill.b, fill.a);
    }
    if (qStrokeW > 0) {
        shape->strokeWidth(sw);
        shape->strokeFill(stroke.r, stroke.g, stroke.b, stroke.a);
    }
    m_stampCanvas->add(shape);
    m_stampCanvas->draw(false);
    m_stampCanvas->sync();
    m_stampCanvas->remove();
    m_stampCanvas->sync();

    keel::i32 minX = bufW;
    keel::i32 minY = bufH;
    keel::i32 maxX = -1;
    keel::i32 maxY = -1;
    for (keel::i32 y = 0; y < bufH; ++y) {
        const keel::u32* row = m_stampScratch.data() + static_cast<keel::usize>(y) * bufW;
        for (keel::i32 x = 0; x < bufW; ++x) {
            if (row[x] != 0u) {
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        entry.empty = true;
    } else {
        const keel::i32 cropW = maxX - minX + 1;
        const keel::i32 cropH = maxY - minY + 1;
        entry.width = static_cast<keel::u16>(cropW);
        entry.height = static_cast<keel::u16>(cropH);
        entry.offsetX = static_cast<keel::i16>(minX - pad);
        entry.offsetY = static_cast<keel::i16>(minY - pad);
        entry.pixels.resize(static_cast<keel::usize>(cropW) * static_cast<keel::usize>(cropH));
        for (keel::i32 row = 0; row < cropH; ++row) {
            std::memcpy(
                entry.pixels.data() + static_cast<keel::usize>(row) * cropW,
                m_stampScratch.data() + static_cast<keel::usize>(minY + row) * bufW + minX,
                static_cast<keel::usize>(cropW) * sizeof(keel::u32)
            );
        }
    }

    if (m_stampScratch.capacity() > 65536u) {
        m_stampScratch = keel::Array<keel::u32>{};
    }

    return m_shapeStampCache.Insert(key, keel::move(entry));
}

void ThorCanvas::BlitTextMask(
    const TextMaskEntry& mask,
    keel::i32 dstX,
    keel::i32 dstY,
    Color color,
    float scaledFontSize,
    bool bgrOrder,
    keel::Optional<Rect> extraClip
) {
    if (mask.empty || mask.mask.empty() || !m_targetBuffer || m_width == 0 || m_height == 0 || color.a == 0u) {
        return;
    }

    Rect clipPx(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
    if (!m_clipStack.empty()) {
        clipPx = clipPx.Intersect(m_clipStack.back());
    }
    if (extraClip.has_value()) {
        clipPx = clipPx.Intersect(*extraClip);
    }
    if (clipPx.w <= 0.0f || clipPx.h <= 0.0f) {
        return;
    }

    FlushCanvas();

    const keel::i32 x0 = dstX + mask.offsetX;
    const keel::i32 y0 = dstY + mask.offsetY;
    if (mask.isSubpixel) {
        TextCompositor::BlitSubpixelMask(
            m_targetBuffer,
            m_width,
            m_height,
            m_stride,
            mask.mask.data(),
            mask.width,
            mask.height,
            static_cast<keel::u32>(mask.width) * 3u,
            x0,
            y0,
            clipPx,
            color,
            m_textSettings,
            scaledFontSize,
            bgrOrder
        );
    } else {
        TextCompositor::BlitGrayscaleMask(
            m_targetBuffer,
            m_width,
            m_height,
            m_stride,
            mask.mask.data(),
            mask.width,
            mask.height,
            mask.width,
            x0,
            y0,
            clipPx,
            color,
            m_textSettings,
            scaledFontSize
        );
    }
}

void ThorCanvas::BlitShapeStamp(
    const ShapeStampEntry& stamp,
    keel::i32 dstX,
    keel::i32 dstY,
    keel::Optional<Rect> extraClip
) {
    if (stamp.empty || stamp.pixels.empty() || !m_targetBuffer || m_width == 0 || m_height == 0) {
        return;
    }

    const keel::i32 x0 = dstX + stamp.offsetX;
    const keel::i32 y0 = dstY + stamp.offsetY;
    const keel::i32 x1 = x0 + static_cast<keel::i32>(stamp.width);
    const keel::i32 y1 = y0 + static_cast<keel::i32>(stamp.height);

    keel::i32 clipX0 = keel::max(0, x0);
    keel::i32 clipY0 = keel::max(0, y0);
    keel::i32 clipX1 = keel::min(static_cast<keel::i32>(m_width), x1);
    keel::i32 clipY1 = keel::min(static_cast<keel::i32>(m_height), y1);

    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        clipX0 = keel::max(clipX0, keel::floor_i32(cr.x));
        clipY0 = keel::max(clipY0, keel::floor_i32(cr.y));
        clipX1 = keel::min(clipX1, keel::ceil_i32(cr.Right()));
        clipY1 = keel::min(clipY1, keel::ceil_i32(cr.Bottom()));
    }
    if (extraClip.has_value()) {
        clipX0 = keel::max(clipX0, keel::floor_i32(extraClip->x));
        clipY0 = keel::max(clipY0, keel::floor_i32(extraClip->y));
        clipX1 = keel::min(clipX1, keel::ceil_i32(extraClip->Right()));
        clipY1 = keel::min(clipY1, keel::ceil_i32(extraClip->Bottom()));
    }

    if (clipX0 >= clipX1 || clipY0 >= clipY1) {
        return;
    }

    FlushCanvas();

    const keel::i32 spanW = clipX1 - clipX0;
    const keel::i32 srcOffX = clipX0 - x0;
    for (keel::i32 y = clipY0; y < clipY1; ++y) {
        const keel::u32* srcRow = stamp.pixels.data() + static_cast<keel::usize>(y - y0) * stamp.width + srcOffX;
        keel::u32* dstRow = m_targetBuffer + static_cast<keel::usize>(y) * m_stride + clipX0;
        for (keel::i32 x = 0; x < spanW; ++x) {
            const keel::u32 s = srcRow[x];
            if (s == 0u) continue;
            if (s >= 0xFF000000u) {
                dstRow[x] = s;
            } else {
                dstRow[x] = BlendPreNormalSwar(s, dstRow[x]);
            }
        }
    }
}

void ThorCanvas::DrawText(keel::StringRef text, Vec2 pos, float fontSize, Color color) {
    DrawTextStyled(text, pos, fontSize, color, FontStyle::Regular);
}

void ThorCanvas::DrawTextStyled(
    keel::StringRef text,
    Vec2 pos,
    float fontSize,
    Color color,
    FontStyle style,
    float angleDeg
) {
    if (text.empty() || color.a == 0 || !m_canvas) return;
    if (!m_clipStack.empty() && (m_clipStack.back().w <= 0.0f || m_clipStack.back().h <= 0.0f)) return;

    const bool hasRot = keel::abs(angleDeg) > 0.01f;
    if (!hasRot && m_targetBuffer && m_width > 0 && m_height > 0 && text.find('\n') == keel::StringRef::npos) {
        const keel::StringRef stampText = ClampUtf8Prefix(text, 512);
        const Vec2 m = MeasureTextStyled(stampText, fontSize, style);
        const Rect approxBounds(
            PxX(pos.x - 2.0f),
            PxY(pos.y - fontSize * 0.25f),
            (m.x + 6.0f) * m_scale,
            (m.y + fontSize * 0.5f) * m_scale
        );
        if (!m_clipStack.empty() && !approxBounds.Intersects(m_clipStack.back())) {
            return;
        }
        const float scaledFs = fontSize * m_scale;
#if defined(GUIKIT_HAS_FREETYPE) && GUIKIT_HAS_FREETYPE
        if (m_ftEngine && m_ftEngine->HasFontForStyle(style)) {
            Rect clipPx(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
            if (!m_clipStack.empty()) {
                clipPx = clipPx.Intersect(m_clipStack.back());
            }
            if (clipPx.w > 0.0f && clipPx.h > 0.0f) {
                FlushCanvas();
                if (m_ftEngine->DrawText(
                        m_targetBuffer, m_width, m_height, m_stride,
                        stampText, PxX(pos.x), PxY(pos.y), scaledFs,
                        color, style, clipPx, m_textSettings, m_scale)) {
                    return;
                }
            } else {
                return;
            }
        }
#endif
        const FontSmoothing effSmooth = TextCompositor::ResolveEffectiveSmoothing(m_textSettings.smoothing, m_scale);
        const bool useSubpixel = (effSmooth == FontSmoothing::SubpixelRGB || effSmooth == FontSmoothing::SubpixelBGR);
        const bool bgrOrder = (effSmooth == FontSmoothing::SubpixelBGR);
        const keel::i32 dstX = keel::round_i32(PxX(pos.x));
        const keel::i32 dstY = keel::round_i32(PxY(pos.y));
        const TextMaskEntry& mask = ResolveTextMask(stampText, scaledFs, style, useSubpixel);
        BlitTextMask(mask, dstX, dstY, color, scaledFs, bgrOrder);
        return;
    }

    const bool isItalic = NeedsSyntheticItalic(style);
    const bool isBold = NeedsSyntheticBold(style);
    const float rad = hasRot ? keel::deg_to_rad(angleDeg) : 0.0f;
    const float cosA = hasRot ? keel::cos(rad) : 1.0f;
    const float sinA = hasRot ? keel::sin(rad) : 0.0f;
    const ScopedCStr textStr(text);

    auto emitTextPass = [&](float offset) {
        auto txt = tvg::Text::gen();
        txt->text(textStr.c_str());

        ApplyFont(txt, style);

        txt->size(ToTvgFontSize(fontSize * m_scale));
        if (isItalic) {
            txt->italic(0.18f);
        }
        txt->fill(color.r, color.g, color.b);
        txt->opacity(color.a);
        txt->align(0.0f, 0.0f);
        if (hasRot) {
            txt->rotate(angleDeg);
        }
        txt->translate(PxX(pos.x + offset * cosA), PxY(pos.y + offset * sinA));

        m_canvas->add(txt);
        m_hasPendingPaints = true;
    };

    emitTextPass(0.0f);
    if (isBold) {
        emitTextPass(0.55f);
    }
}

void ThorCanvas::DrawTextBox(
    keel::StringRef text,
    Rect bounds,
    float fontSize,
    Color color,
    float alignX,
    float alignY
) {
    DrawTextBoxStyled(text, bounds, fontSize, color, FontStyle::Regular, alignX, alignY);
}

void ThorCanvas::DrawTextBoxStyled(
    keel::StringRef text,
    Rect bounds,
    float fontSize,
    Color color,
    FontStyle style,
    float alignX,
    float alignY
) {
    if (text.empty() || color.a == 0 || bounds.w <= 0.0f || bounds.h <= 0.0f || !m_canvas) return;
    const Rect sBounds(Px(bounds));
    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        if (cr.w <= 0.0f || cr.h <= 0.0f || !sBounds.Intersects(cr)) return;
    }

    keel::FixedString<256> truncated;
    keel::StringRef drawText = text;
    if (bounds.w > 0.0f && MeasureTextStyled(text, fontSize, style).x > bounds.w + kTextSubpixelTolerance) {
        truncated = TextUtil::TruncateWithEllipsis(text, bounds.w, fontSize, this, style);
        drawText = truncated;
    }
    if (drawText.empty()) return;

    if (m_targetBuffer && m_width > 0 && m_height > 0 &&
        drawText.find('\n') == keel::StringRef::npos) {
        const keel::StringRef stampText = ClampUtf8Prefix(drawText, 512);
        const float scaledFs = fontSize * m_scale;
#if defined(GUIKIT_HAS_FREETYPE) && GUIKIT_HAS_FREETYPE
        if (m_ftEngine && m_ftEngine->HasFontForStyle(style)) {
            const float vPad = keel::max(4.0f, scaledFs * 0.35f);
            Rect clipPx(0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height));
            if (!m_clipStack.empty()) {
                clipPx = clipPx.Intersect(m_clipStack.back());
            }
            clipPx = clipPx.Intersect(Rect(sBounds.x - 2.0f, sBounds.y - vPad, sBounds.w + 4.0f, sBounds.h + vPad * 2.0f));
            if (clipPx.w > 0.0f && clipPx.h > 0.0f) {
                FlushCanvas();
                if (m_ftEngine->DrawTextBox(
                        m_targetBuffer, m_width, m_height, m_stride,
                        stampText, sBounds, scaledFs, color, style,
                        alignX, alignY, clipPx, m_textSettings, m_scale)) {
                    return;
                }
            } else {
                return;
            }
        }
#endif
        const FontSmoothing effSmooth = TextCompositor::ResolveEffectiveSmoothing(m_textSettings.smoothing, m_scale);
        const bool useSubpixel = (effSmooth == FontSmoothing::SubpixelRGB || effSmooth == FontSmoothing::SubpixelBGR);
        const bool bgrOrder = (effSmooth == FontSmoothing::SubpixelBGR);
        const TextMaskEntry& capStamp = ResolveTextMask("H", scaledFs, style, false);
        const bool capEmpty = capStamp.empty;
        const keel::i32 capRawOffY = static_cast<keel::i32>(capStamp.offsetY);
        const keel::i32 capRawH = static_cast<keel::i32>(capStamp.height);
        const TextMaskEntry& stamp = ResolveTextMask(stampText, scaledFs, style, useSubpixel);
        if (stamp.empty) return;
        const keel::i32 capOffY = capEmpty ? static_cast<keel::i32>(stamp.offsetY) : capRawOffY;
        const keel::i32 capH = capEmpty ? static_cast<keel::i32>(stamp.height) : capRawH;
        const bool hasEdgeSpace = (stampText.front() == ' ' || stampText.back() == ' ');
        float tx = sBounds.x;
        if (alignX > 0.01f && !hasEdgeSpace) {
            tx = sBounds.x + (sBounds.w - static_cast<float>(stamp.width)) * alignX - static_cast<float>(stamp.offsetX);
        } else if (alignX > 0.01f) {
            const Vec2 m = MeasureTextStyled(stampText, fontSize, style);
            tx = PxX(bounds.x + (bounds.w - m.x) * alignX);
        }
        const float ty = sBounds.y + (sBounds.h - static_cast<float>(capH)) * alignY - static_cast<float>(capOffY);
        const keel::i32 dstX = keel::round_i32(tx);
        const keel::i32 dstY = keel::round_i32(ty);
        const float vPad = keel::max(4.0f, scaledFs * 0.35f);
        BlitTextMask(stamp, dstX, dstY, color, scaledFs, bgrOrder, Rect(sBounds.x - 2.0f, sBounds.y - vPad, sBounds.w + 4.0f, sBounds.h + vPad * 2.0f));
        return;
    }

    const bool isItalic = NeedsSyntheticItalic(style);
    const bool isBold = NeedsSyntheticBold(style);
    const ScopedCStr textStr(drawText);
    auto emitBoxPass = [&](float offsetX) {
        auto txt = tvg::Text::gen();
        txt->text(textStr.c_str());

        ApplyFont(txt, style);

        txt->size(ToTvgFontSize(fontSize * m_scale));
        if (isItalic) {
            txt->italic(0.18f);
        }
        txt->fill(color.r, color.g, color.b);
        txt->opacity(color.a);
        txt->layout(sBounds.w, sBounds.h);
        txt->align(alignX, alignY);
        txt->translate(sBounds.x + offsetX * m_scale, sBounds.y);

        m_canvas->add(txt);
        m_hasPendingPaints = true;
    };
    emitBoxPass(0.0f);
    if (isBold) {
        emitBoxPass(0.55f);
    }
}

void ThorCanvas::DrawLine(Vec2 p1, Vec2 p2, Color color, float strokeWidth) {
    if (color.a == 0 || strokeWidth <= 0.0f || !m_canvas) return;
    if (!m_hasPendingPaints) {
        if (keel::abs(p1.y - p2.y) <= 0.05f) {
            const float x0 = keel::min(p1.x, p2.x);
            const float x1 = keel::max(p1.x, p2.x);
            const float hw = strokeWidth * 0.5f;
            DrawRect(Rect(x0 - hw, p1.y - hw, (x1 - x0) + strokeWidth, strokeWidth), color);
            return;
        }
        if (keel::abs(p1.x - p2.x) <= 0.05f) {
            const float y0 = keel::min(p1.y, p2.y);
            const float y1 = keel::max(p1.y, p2.y);
            const float hw = strokeWidth * 0.5f;
            DrawRect(Rect(p1.x - hw, y0 - hw, strokeWidth, (y1 - y0) + strokeWidth), color);
            return;
        }
    }
    Vec2 sp1(Px(p1));
    Vec2 sp2(Px(p2));
    if (!keel::is_finite(sp1.x) || !keel::is_finite(sp1.y) || !keel::is_finite(sp2.x) || !keel::is_finite(sp2.y)) {
        return;
    }
    const float sWidth = strokeWidth * m_scale;
    const float pad = sWidth * 0.5f + 4.0f;

    float clipMinX = -pad;
    float clipMinY = -pad;
    float clipMaxX = static_cast<float>(m_width) + pad;
    float clipMaxY = static_cast<float>(m_height) + pad;
    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        if (cr.w <= 0.0f || cr.h <= 0.0f) return;
        clipMinX = keel::max(clipMinX, cr.x - pad);
        clipMinY = keel::max(clipMinY, cr.y - pad);
        clipMaxX = keel::min(clipMaxX, cr.Right() + pad);
        clipMaxY = keel::min(clipMaxY, cr.Bottom() + pad);
    }

    const float dx = sp2.x - sp1.x;
    const float dy = sp2.y - sp1.y;
    float t0 = 0.0f;
    float t1 = 1.0f;
    auto clipTest = [&](float p, float q) noexcept -> bool {
        if (keel::abs(p) < 1e-7f) return q >= 0.0f;
        const float r = q / p;
        if (p < 0.0f) {
            if (r > t1) return false;
            if (r > t0) t0 = r;
        } else {
            if (r < t0) return false;
            if (r < t1) t1 = r;
        }
        return true;
    };
    if (!clipTest(-dx, sp1.x - clipMinX) ||
        !clipTest( dx, clipMaxX - sp1.x) ||
        !clipTest(-dy, sp1.y - clipMinY) ||
        !clipTest( dy, clipMaxY - sp1.y)) {
        return;
    }
    if (t1 < 1.0f) {
        sp2 = Vec2(sp1.x + t1 * dx, sp1.y + t1 * dy);
    }
    if (t0 > 0.0f) {
        sp1 = Vec2(sp1.x + t0 * dx, sp1.y + t0 * dy);
    }

    auto shape = tvg::Shape::gen();
    shape->moveTo(sp1.x, sp1.y);
    shape->lineTo(sp2.x, sp2.y);
    shape->strokeWidth(sWidth);
    shape->strokeCap(tvg::StrokeCap::Round);
    shape->strokeFill(color.r, color.g, color.b, color.a);

    m_canvas->add(shape);
    m_hasPendingPaints = true;
}

void ThorCanvas::DrawTriangle(
    Vec2 p1,
    Vec2 p2,
    Vec2 p3,
    Color fill,
    Color stroke,
    float strokeWidth
) {
    if ((fill.a == 0 && (stroke.a == 0 || strokeWidth <= 0.0f)) || !m_canvas) return;
    const Vec2 sp1(Px(p1));
    const Vec2 sp2(Px(p2));
    const Vec2 sp3(Px(p3));
    const float pad = strokeWidth * m_scale + 1.0f;
    const float minX = keel::min(sp1.x, keel::min(sp2.x, sp3.x)) - pad;
    const float minY = keel::min(sp1.y, keel::min(sp2.y, sp3.y)) - pad;
    const float maxX = keel::max(sp1.x, keel::max(sp2.x, sp3.x)) + pad;
    const float maxY = keel::max(sp1.y, keel::max(sp2.y, sp3.y)) + pad;
    const Rect triBounds(minX, minY, maxX - minX, maxY - minY);

    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        if (cr.w <= 0.0f || cr.h <= 0.0f || !triBounds.Intersects(cr)) return;
    }

    auto shape = tvg::Shape::gen();
    shape->moveTo(sp1.x, sp1.y);
    shape->lineTo(sp2.x, sp2.y);
    shape->lineTo(sp3.x, sp3.y);
    shape->close();

    if (fill.a > 0) {
        shape->fill(fill.r, fill.g, fill.b, fill.a);
    }
    if (stroke.a > 0 && strokeWidth > 0.0f) {
        shape->strokeWidth(strokeWidth * m_scale);
        shape->strokeJoin(tvg::StrokeJoin::Round);
        shape->strokeFill(stroke.r, stroke.g, stroke.b, stroke.a);
    }

    m_canvas->add(shape);
    m_hasPendingPaints = true;
}

void ThorCanvas::DrawPath(const VectorPath& path, const PathPaint& paint) {
    static_assert(sizeof(PathVerb) == sizeof(tvg::PathCommand));
    static_assert(static_cast<keel::u8>(PathVerb::Close) == static_cast<keel::u8>(tvg::PathCommand::Close));
    static_assert(static_cast<keel::u8>(PathVerb::MoveTo) == static_cast<keel::u8>(tvg::PathCommand::MoveTo));
    static_assert(static_cast<keel::u8>(PathVerb::LineTo) == static_cast<keel::u8>(tvg::PathCommand::LineTo));
    static_assert(static_cast<keel::u8>(PathVerb::CubicTo) == static_cast<keel::u8>(tvg::PathCommand::CubicTo));
    static_assert(sizeof(Vec2) == sizeof(tvg::Point) && alignof(Vec2) == alignof(tvg::Point));

    if (path.Empty() || !m_canvas) return;
    const bool hasFill = paint.renderFill && paint.fillColor.a > 0;
    const bool hasStroke = paint.renderStroke && paint.strokeColor.a > 0 && paint.strokeWidthPx > 0.0f;
    if (!hasFill && !hasStroke) return;

    auto* shape = tvg::Shape::gen();
    const float s = m_scale;

    if (s == 1.0f && m_offset.x == 0.0f && m_offset.y == 0.0f) {
        shape->appendPath(
            reinterpret_cast<const tvg::PathCommand*>(path.verbs.data()),
            static_cast<keel::u32>(path.verbs.size()),
            reinterpret_cast<const tvg::Point*>(path.points.data()),
            static_cast<keel::u32>(path.points.size())
        );
    } else {
        keel::usize ptIdx = 0;
        const keel::usize numPts = path.points.size();
        for (PathVerb verb : path.verbs) {
            switch (verb) {
                case PathVerb::MoveTo:
                    if (ptIdx < numPts) {
                        const Vec2& p = path.points[ptIdx++];
                        shape->moveTo(PxX(p.x), PxY(p.y));
                    }
                    break;
                case PathVerb::LineTo:
                    if (ptIdx < numPts) {
                        const Vec2& p = path.points[ptIdx++];
                        shape->lineTo(PxX(p.x), PxY(p.y));
                    }
                    break;
                case PathVerb::CubicTo:
                    if (ptIdx + 2 < numPts) {
                        const Vec2& c1 = path.points[ptIdx];
                        const Vec2& c2 = path.points[ptIdx + 1];
                        const Vec2& p  = path.points[ptIdx + 2];
                        ptIdx += 3;
                        shape->cubicTo(PxX(c1.x), PxY(c1.y), PxX(c2.x), PxY(c2.y), PxX(p.x), PxY(p.y));
                    }
                    break;
                case PathVerb::Close:
                    shape->close();
                    break;
            }
        }
    }

    if (hasFill) {
        shape->fillRule(paint.fillRule == FillRule::EvenOdd ? tvg::FillRule::EvenOdd : tvg::FillRule::NonZero);
        shape->fill(paint.fillColor.r, paint.fillColor.g, paint.fillColor.b, paint.fillColor.a);
    }

    if (hasStroke) {
        shape->strokeWidth(paint.strokeWidthPx * s);
        shape->strokeJoin(tvg::StrokeJoin::Bevel);
        shape->strokeCap(tvg::StrokeCap::Butt);
        shape->strokeFill(paint.strokeColor.r, paint.strokeColor.g, paint.strokeColor.b, paint.strokeColor.a);
        if (paint.dashed) {
            const float dashPattern[2] = {paint.dashOnPx * s, paint.dashOffPx * s};
            shape->strokeDash(dashPattern, 2);
        }
    }

    m_canvas->add(shape);
    m_hasPendingPaints = true;
}

ThorCanvas::ImageCacheEntry& ThorCanvas::ResolveImageEntry(keel::StringRef source) {
    static ImageCacheEntry s_emptyEntry{};
    if (source.empty()) return s_emptyEntry;

    if (ImageCacheEntry* existing = m_imageCache.get(source)) {
        return *existing;
    }

    if (m_imageCache.size() >= kMaxImageCacheEntries) {
        FlushCanvas();
        m_imageCache = {};
        m_imageCacheBytes = 0;
    }

    ImageCacheEntry entry{};
    const keel::StringRef trimmed = source.trim_left();

    auto* probe = tvg::Picture::gen();
    if (trimmed.starts_with("<svg") || trimmed.starts_with("<?xml")) {
        entry.isInlineSvg = true;
        entry.resolvedDataOrPath.assign(trimmed);
        if (probe->load(entry.resolvedDataOrPath.data(),
                        static_cast<keel::u32>(entry.resolvedDataOrPath.size()),
                        "svg",
                        nullptr,
                        false) == tvg::Result::Success) {
            float w = 0.0f, h = 0.0f;
            probe->size(&w, &h);
            if (w > 0.0f && h > 0.0f) {
                entry.size = {w, h};
                entry.valid = true;
            }
        }
    } else if (auto asset = Assets::Find(source); asset && !asset->Empty()) {
        const keel::SmallString<16> mimeStr(asset->mimeType);
        const char* mime = mimeStr.empty() ? nullptr : mimeStr.c_str();
        if (probe->load(reinterpret_cast<const char*>(asset->data),
                        static_cast<keel::u32>(asset->size),
                        mime,
                        nullptr,
                        false) == tvg::Result::Success) {
            float w = 0.0f, h = 0.0f;
            probe->size(&w, &h);
            if (w > 0.0f && h > 0.0f) {
                entry.embeddedData = asset->data;
                entry.embeddedSize = asset->size;
                entry.embeddedMimeType.assign(asset->mimeType);
                entry.isEmbedded = true;
                entry.size = {w, h};
                entry.valid = true;
            }
        }
    }

    if (!entry.valid && !entry.isInlineSvg) {
        const char* prefixes[] = {"", "../", "../../", "../../../"};
        for (const char* prefix : prefixes) {
            keel::String candidate(prefix);
            candidate.append(source);
            if (probe->load(candidate.c_str()) == tvg::Result::Success) {
                float w = 0.0f, h = 0.0f;
                probe->size(&w, &h);
                if (w > 0.0f && h > 0.0f) {
                    entry.resolvedDataOrPath = keel::move(candidate);
                    entry.size = {w, h};
                    entry.valid = true;
                    break;
                }
            }
        }
    }
    delete probe;

    return m_imageCache.get_or_insert(keel::String::from(source), keel::move(entry));
}

Vec2 ThorCanvas::GetImageSize(keel::StringRef source) {
    const auto& entry = ResolveImageEntry(source);
    return entry.valid ? entry.size : Vec2{0.0f, 0.0f};
}

bool ThorCanvas::DrawImage(
    keel::StringRef source,
    Rect destRect,
    float cornerRadius,
    keel::u8 opacity
) {
    if (source.empty() || opacity == 0 || destRect.w <= 0.0f || destRect.h <= 0.0f || !m_canvas) {
        return false;
    }

    const Rect sBounds(Px(destRect));
    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        if (cr.w <= 0.0f || cr.h <= 0.0f || !sBounds.Intersects(cr)) {
            return true; // Valid image, simply clipped out of view
        }
    }

    auto& entry = ResolveImageEntry(source);
    if (!entry.valid || entry.size.x <= 0.0f || entry.size.y <= 0.0f) {
        return false;
    }

    const float sRad = keel::max(0.0f, cornerRadius * m_scale);
    const keel::i32 dstX0 = keel::round_i32(sBounds.x);
    const keel::i32 dstY0 = keel::round_i32(sBounds.y);
    const keel::i32 dstX1 = keel::round_i32(sBounds.Right());
    const keel::i32 dstY1 = keel::round_i32(sBounds.Bottom());
    const keel::u32 dstW = static_cast<keel::u32>(keel::max(1, dstX1 - dstX0));
    const keel::u32 dstH = static_cast<keel::u32>(keel::max(1, dstY1 - dstY0));

    const bool isSvgSource =
        entry.isInlineSvg ||
        entry.embeddedMimeType.as_ref() == "svg" ||
        source.ends_with(".svg") ||
        source.ends_with(".SVG");
    const keel::u32 natW = keel::max(1u, keel::round_u32(entry.size.x));
    const keel::u32 natH = keel::max(1u, keel::round_u32(entry.size.y));
    const bool canCacheNativeBitmap = (!isSvgSource && natW <= 512u && natH <= 512u);
    const bool canCacheSmallSvg = (isSvgSource && sRad <= 0.0f && m_targetBuffer && dstW <= 256u && dstH <= 256u);

    if (canCacheNativeBitmap || canCacheSmallSvg) {
        const keel::u32 cacheW = canCacheNativeBitmap ? natW : dstW;
        const keel::u32 cacheH = canCacheNativeBitmap ? natH : dstH;
        if (entry.cachedRasterW != cacheW || entry.cachedRasterH != cacheH || entry.cachedPixels.empty()) {
            const keel::usize oldBytes = entry.cachedPixels.capacity() * sizeof(keel::u32);
            m_imageCacheBytes = (m_imageCacheBytes >= oldBytes) ? (m_imageCacheBytes - oldBytes) : 0u;
            entry.cachedPixels = {};
            entry.cachedRasterW = 0;
            entry.cachedRasterH = 0;

            const keel::usize newBytes = static_cast<keel::usize>(cacheW) * static_cast<keel::usize>(cacheH) * sizeof(keel::u32);
            if (m_imageCacheBytes + newBytes > kMaxImageCacheBytes) {
                FlushCanvas();
                for (auto [k, other] : m_imageCache) {
                    if (&other != &entry && other.cachedPixels.capacity() > 0u) {
                        const keel::usize ob = other.cachedPixels.capacity() * sizeof(keel::u32);
                        m_imageCacheBytes = (m_imageCacheBytes >= ob) ? (m_imageCacheBytes - ob) : 0u;
                        other.cachedPixels = {};
                        other.cachedRasterW = 0;
                        other.cachedRasterH = 0;
                    }
                }
            }

            if (!m_stampCanvas) {
                m_stampCanvas = tvg::SwCanvas::gen(tvg::EngineOption::None);
            }
            entry.cachedPixels.assign(static_cast<keel::usize>(cacheW) * static_cast<keel::usize>(cacheH), 0u);
            m_stampCanvas->target(entry.cachedPixels.data(), cacheW, cacheW, cacheH, tvg::ColorSpace::ABGR8888);

            auto* pic = tvg::Picture::gen();
            tvg::Result res = tvg::Result::InvalidArguments;
            if (entry.isInlineSvg) {
                res = pic->load(entry.resolvedDataOrPath.data(),
                                static_cast<keel::u32>(entry.resolvedDataOrPath.size()),
                                "svg",
                                nullptr,
                                false);
            } else if (entry.isEmbedded) {
                const char* mime = entry.embeddedMimeType.empty() ? nullptr : entry.embeddedMimeType.c_str();
                res = pic->load(reinterpret_cast<const char*>(entry.embeddedData),
                                static_cast<keel::u32>(entry.embeddedSize),
                                mime,
                                nullptr,
                                false);
            } else {
                res = pic->load(entry.resolvedDataOrPath.c_str());
            }
            if (res == tvg::Result::Success) {
                pic->size(static_cast<float>(cacheW), static_cast<float>(cacheH));
                m_stampCanvas->add(pic);
                m_stampCanvas->draw(false);
                m_stampCanvas->sync();
                m_stampCanvas->remove();
                m_stampCanvas->sync();
                entry.cachedRasterW = cacheW;
                entry.cachedRasterH = cacheH;
                m_imageCacheBytes += entry.cachedPixels.capacity() * sizeof(keel::u32);
            } else {
                entry.cachedPixels = {};
                delete pic;
                return false;
            }
        }
        if (sRad <= 0.0f && m_targetBuffer) {
            return DrawPixels({entry.cachedPixels, cacheW, cacheH}, destRect, 0.0f, opacity);
        }
        auto* pic = tvg::Picture::gen();
        if (pic->load(entry.cachedPixels.data(), cacheW, cacheH, tvg::ColorSpace::ABGR8888, false) != tvg::Result::Success) {
            delete pic;
            return false;
        }
        const float sx = sBounds.w / static_cast<float>(cacheW);
        const float sy = sBounds.h / static_cast<float>(cacheH);
        const tvg::Matrix m = {
            sx, 0.0f, sBounds.x,
            0.0f, sy, sBounds.y,
            0.0f, 0.0f, 1.0f
        };
        pic->transform(m);
        if (opacity < 255) {
            pic->opacity(opacity);
        }
        if (sRad > 0.0f) {
            Rect clipR = sBounds;
            float effRad = sRad;
            if (!m_clipStack.empty()) {
                const Rect& activeClip = m_clipStack.back();
                if (!activeClip.Contains(sBounds.Pos()) || !activeClip.Contains({sBounds.Right(), sBounds.Bottom()})) {
                    clipR = sBounds.Intersect(activeClip);
                    effRad = 0.0f;
                }
            }
            auto* clipper = tvg::Shape::gen();
            clipper->appendRect(clipR.x, clipR.y, clipR.w, clipR.h, effRad, effRad);
            pic->clip(clipper);
        }
        m_canvas->add(pic);
        m_hasPendingPaints = true;
        return true;
    }

    auto* pic = tvg::Picture::gen();
    tvg::Result res = tvg::Result::InvalidArguments;
    if (entry.isInlineSvg) {
        res = pic->load(entry.resolvedDataOrPath.data(),
                        static_cast<keel::u32>(entry.resolvedDataOrPath.size()),
                        "svg",
                        nullptr,
                        false);
    } else if (entry.isEmbedded) {
        const char* mime = entry.embeddedMimeType.empty() ? nullptr : entry.embeddedMimeType.c_str();
        res = pic->load(reinterpret_cast<const char*>(entry.embeddedData),
                        static_cast<keel::u32>(entry.embeddedSize),
                        mime,
                        nullptr,
                        false);
    } else {
        res = pic->load(entry.resolvedDataOrPath.c_str());
    }

    if (res != tvg::Result::Success) {
        delete pic;
        return false;
    }

    const float sx = sBounds.w / entry.size.x;
    const float sy = sBounds.h / entry.size.y;
    const tvg::Matrix m = {
        sx, 0.0f, sBounds.x,
        0.0f, sy, sBounds.y,
        0.0f, 0.0f, 1.0f
    };
    pic->transform(m);

    if (opacity < 255) {
        pic->opacity(opacity);
    }

    if (sRad > 0.0f) {
        Rect clipR = sBounds;
        float effRad = sRad;
        if (!m_clipStack.empty()) {
            const Rect& activeClip = m_clipStack.back();
            if (!activeClip.Contains(sBounds.Pos()) || !activeClip.Contains({sBounds.Right(), sBounds.Bottom()})) {
                clipR = sBounds.Intersect(activeClip);
                effRad = 0.0f;
            }
        }
        auto* clipper = tvg::Shape::gen();
        clipper->appendRect(clipR.x, clipR.y, clipR.w, clipR.h, effRad, effRad);
        pic->clip(clipper);
    }

    m_canvas->add(pic);
    m_hasPendingPaints = true;
    return true;
}

bool ThorCanvas::DrawPixels(
    PixelRef pixels,
    Rect destRect,
    float cornerRadius,
    keel::u8 opacity
) {
    if (pixels.Empty() || opacity == 0 || destRect.w <= 0.0f || destRect.h <= 0.0f || !m_canvas) {
        return false;
    }

    const keel::u32 width = pixels.Width();
    const keel::u32 height = pixels.Height();

    const float sRad = keel::max(0.0f, cornerRadius * m_scale);
    Rect sBounds(Px(destRect));
    if (sRad <= 0.0f) {
        const keel::i32 dstX0 = keel::round_i32(sBounds.x);
        const keel::i32 dstY0 = keel::round_i32(sBounds.y);
        const keel::i32 dstX1 = keel::round_i32(sBounds.Right());
        const keel::i32 dstY1 = keel::round_i32(sBounds.Bottom());
        const keel::i32 dstW = dstX1 - dstX0;
        const keel::i32 dstH = dstY1 - dstY0;
        if (dstW <= 0 || dstH <= 0) {
            return true;
        }

        if (m_targetBuffer && m_width > 0 && m_height > 0) {
            keel::i32 clipX0 = keel::max(0, dstX0);
            keel::i32 clipY0 = keel::max(0, dstY0);
            keel::i32 clipX1 = keel::min(static_cast<keel::i32>(m_width), dstX1);
            keel::i32 clipY1 = keel::min(static_cast<keel::i32>(m_height), dstY1);

            if (!m_clipStack.empty()) {
                const Rect& cr = m_clipStack.back();
                clipX0 = keel::max(clipX0, keel::floor_i32(cr.x));
                clipY0 = keel::max(clipY0, keel::floor_i32(cr.y));
                clipX1 = keel::min(clipX1, keel::ceil_i32(cr.Right()));
                clipY1 = keel::min(clipY1, keel::ceil_i32(cr.Bottom()));
            }
            if (clipX0 >= clipX1 || clipY0 >= clipY1) {
                return true;
            }

            FlushCanvas();

            const keel::i32 spanW = clipX1 - clipX0;
            if (dstW == static_cast<keel::i32>(width) && dstH == static_cast<keel::i32>(height)) {
                const keel::i32 srcOffX = clipX0 - dstX0;
                for (keel::i32 y = clipY0; y < clipY1; ++y) {
                    const keel::u32* srcRow = pixels.Row(static_cast<keel::u32>(y - dstY0)).data() + srcOffX;
                    keel::u32* dstRow = m_targetBuffer + static_cast<keel::usize>(y) * m_stride + clipX0;
                    if (opacity == 255) {
                        for (keel::i32 x = 0; x < spanW; ++x) {
                            const keel::u32 s = srcRow[x];
                            if (s >= 0xFF000000u) {
                                dstRow[x] = s;
                            } else if (s != 0u) {
                                dstRow[x] = BlendPreNormalSwar(s, dstRow[x]);
                            }
                        }
                    } else {
                        for (keel::i32 x = 0; x < spanW; ++x) {
                            const keel::u32 s = AlphaBlendSwar(srcRow[x], opacity);
                            if (s != 0u) {
                                dstRow[x] = BlendPreNormalSwar(s, dstRow[x]);
                            }
                        }
                    }
                }
            } else {
                const keel::u32 stepX = (width << 16) / static_cast<keel::u32>(dstW);
                const keel::u32 stepY = (height << 16) / static_cast<keel::u32>(dstH);
                const keel::u32 startU = static_cast<keel::u32>(clipX0 - dstX0) * stepX + (stepX >> 1);
                keel::u32 curV = static_cast<keel::u32>(clipY0 - dstY0) * stepY + (stepY >> 1);
                const keel::u32 maxU = (width - 1u) << 16;

                for (keel::i32 y = clipY0; y < clipY1; ++y, curV += stepY) {
                    const keel::u32 sy = keel::min(height - 1u, curV >> 16);
                    const keel::u32* srcRow = pixels.Row(sy).data();
                    keel::u32* dstRow = m_targetBuffer + static_cast<keel::usize>(y) * m_stride + clipX0;
                    keel::u32 curU = startU;
                    if (opacity == 255) {
                        for (keel::i32 x = 0; x < spanW; ++x, curU += stepX) {
                            const keel::u32 s = srcRow[keel::min(curU, maxU) >> 16];
                            if (s >= 0xFF000000u) {
                                dstRow[x] = s;
                            } else if (s != 0u) {
                                dstRow[x] = BlendPreNormalSwar(s, dstRow[x]);
                            }
                        }
                    } else {
                        for (keel::i32 x = 0; x < spanW; ++x, curU += stepX) {
                            const keel::u32 s = AlphaBlendSwar(srcRow[keel::min(curU, maxU) >> 16], opacity);
                            if (s != 0u) {
                                dstRow[x] = BlendPreNormalSwar(s, dstRow[x]);
                            }
                        }
                    }
                }
            }
            return true;
        }

        sBounds = Rect(static_cast<float>(dstX0), static_cast<float>(dstY0), static_cast<float>(dstW), static_cast<float>(dstH));
    }
    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        if (cr.w <= 0.0f || cr.h <= 0.0f || !sBounds.Intersects(cr)) {
            return true;
        }
    }

    auto* pic = tvg::Picture::gen();
    if (pic->load(pixels.Data(), width, height, tvg::ColorSpace::ABGR8888S, true) != tvg::Result::Success) {
        delete pic;
        return false;
    }

    const float sx = sBounds.w / static_cast<float>(width);
    const float sy = sBounds.h / static_cast<float>(height);
    const tvg::Matrix m = {
        sx, 0.0f, sBounds.x,
        0.0f, sy, sBounds.y,
        0.0f, 0.0f, 1.0f
    };
    pic->transform(m);

    if (opacity < 255) {
        pic->opacity(opacity);
    }

    if (sRad > 0.0f) {
        Rect clipR = sBounds;
        float effRad = sRad;
        if (!m_clipStack.empty()) {
            const Rect& activeClip = m_clipStack.back();
            if (!activeClip.Contains(sBounds.Pos()) || !activeClip.Contains({sBounds.Right(), sBounds.Bottom()})) {
                clipR = sBounds.Intersect(activeClip);
                effRad = 0.0f;
            }
        }
        auto* clipper = tvg::Shape::gen();
        clipper->appendRect(clipR.x, clipR.y, clipR.w, clipR.h, effRad, effRad);
        pic->clip(clipper);
    }

    m_canvas->add(pic);
    m_hasPendingPaints = true;
    return true;
}

void ThorCanvas::PunchHole(Rect rect) {
    if (!m_targetBuffer || m_width == 0 || m_height == 0) return;

    // Flush all background shapes queued prior to this GPU underlay window
    FlushCanvas();

    Rect sRect(Px(rect));
    if (!m_clipStack.empty()) {
        sRect = sRect.Intersect(m_clipStack.back());
    }
    if (sRect.w <= 0.0f || sRect.h <= 0.0f) return;

    const keel::i32 startX = keel::max(0, keel::floor_i32(sRect.x));
    const keel::i32 startY = keel::max(0, keel::floor_i32(sRect.y));
    const keel::i32 endX = keel::min(static_cast<keel::i32>(m_width), keel::ceil_i32(sRect.Right()));
    const keel::i32 endY = keel::min(static_cast<keel::i32>(m_height), keel::ceil_i32(sRect.Bottom()));
    const keel::i32 w = endX - startX;
    if (w > 0 && endY > startY) {
        for (keel::i32 y = startY; y < endY; ++y) {
            keel::fill(
                keel::MutArrayRef<keel::u32>(m_targetBuffer + y * m_stride + startX, static_cast<keel::usize>(w)),
                0x00000000u
            );
        }
    }
}

bool ThorCanvas::DrawGpuShaderPass(const GpuShaderPassDesc& pass, bool punchHole) {
    if (!m_gpuShadersEnabled || pass.shader.shaderId.empty() || pass.shader.glslEs2.empty() ||
        pass.dataField.empty() || pass.plotRect.w <= 0.0f || pass.plotRect.h <= 0.0f) {
        return false;
    }

    if (punchHole) {
        PunchHole(pass.plotRect);
    }

    GpuShaderPassDesc scaled = pass;
    scaled.plotRect = Px(pass.plotRect);
    scaled.clipRect = m_gpuClipStack.empty() ? scaled.plotRect : scaled.plotRect.Intersect(m_gpuClipStack.back());
    if (scaled.clipRect.w <= 0.5f || scaled.clipRect.h <= 0.5f) {
        keel::usize writeIdx = 0;
        for (keel::usize i = 0; i < m_gpuPasses.size(); ++i) {
            if (m_gpuPasses[i].widgetId != scaled.widgetId) {
                if (writeIdx != i) {
                    m_gpuPasses[writeIdx] = keel::move(m_gpuPasses[i]);
                }
                ++writeIdx;
            }
        }
        m_gpuPasses.resize(writeIdx);
        return true;
    }

    for (auto& existing : m_gpuPasses) {
        if (existing.widgetId == scaled.widgetId) {
            existing = scaled;
            return true;
        }
    }
    m_gpuPasses.push_back(scaled);
    return true;
}

bool ThorCanvas::CaptureRegion(Rect rect, keel::Array<keel::u32>& outPixels, keel::u32& outW, keel::u32& outH) {
    if (!m_targetBuffer || m_width == 0 || m_height == 0 || rect.w <= 0.0f || rect.h <= 0.0f) {
        return false;
    }

    const Rect sRect(Px(rect));
    const Rect visRect = m_gpuClipStack.empty() ? sRect : sRect.Intersect(m_gpuClipStack.back());
    const keel::i32 x0 = keel::clamp(keel::round_i32(visRect.x), 0, static_cast<keel::i32>(m_width));
    const keel::i32 y0 = keel::clamp(keel::round_i32(visRect.y), 0, static_cast<keel::i32>(m_height));
    const keel::i32 x1 = keel::clamp(keel::round_i32(visRect.Right()), 0, static_cast<keel::i32>(m_width));
    const keel::i32 y1 = keel::clamp(keel::round_i32(visRect.Bottom()), 0, static_cast<keel::i32>(m_height));
    const keel::i32 w = x1 - x0;
    const keel::i32 h = y1 - y0;
    if (w <= 0 || h <= 0) {
        return false;
    }

    if (!m_clipStack.empty()) {
        const Rect& clip = m_clipStack.back();
        if (clip.x > static_cast<float>(x0) + 1.0f || clip.y > static_cast<float>(y0) + 1.0f ||
            clip.Right() < static_cast<float>(x1) - 1.0f || clip.Bottom() < static_cast<float>(y1) - 1.0f) {
            return false; // Region is partially clipped by damage rect; cannot capture full snapshot
        }
    }

    FlushCanvas();

    outW = static_cast<keel::u32>(w);
    outH = static_cast<keel::u32>(h);
    keel::usize totalPx = 0;
    if (!keel::checked_mul(static_cast<keel::usize>(outW), static_cast<keel::usize>(outH), totalPx)) {
        return false;
    }
    outPixels.resize(totalPx);
    for (keel::i32 row = 0; row < h; ++row) {
        std::memcpy(
            outPixels.data() + static_cast<keel::usize>(row) * outW,
            m_targetBuffer + static_cast<keel::usize>(y0 + row) * m_stride + x0,
            static_cast<keel::usize>(w) * sizeof(keel::u32)
        );
    }
    return true;
}

bool ThorCanvas::RestoreRegion(Rect rect, PixelRef pixels) {
    if (!m_targetBuffer || m_width == 0 || m_height == 0 || pixels.Empty()) {
        return false;
    }

    const keel::u32 inW = pixels.Width();
    const keel::u32 inH = pixels.Height();

    const Rect sRect(Px(rect));
    const Rect visRect = m_gpuClipStack.empty() ? sRect : sRect.Intersect(m_gpuClipStack.back());
    const keel::i32 x0 = keel::clamp(keel::round_i32(visRect.x), 0, static_cast<keel::i32>(m_width));
    const keel::i32 y0 = keel::clamp(keel::round_i32(visRect.y), 0, static_cast<keel::i32>(m_height));
    const keel::i32 x1 = keel::clamp(keel::round_i32(visRect.Right()), 0, static_cast<keel::i32>(m_width));
    const keel::i32 y1 = keel::clamp(keel::round_i32(visRect.Bottom()), 0, static_cast<keel::i32>(m_height));
    const keel::i32 w = x1 - x0;
    const keel::i32 h = y1 - y0;
    if (w != static_cast<keel::i32>(inW) || h != static_cast<keel::i32>(inH)) {
        return false;
    }

    keel::i32 clipX0 = x0;
    keel::i32 clipY0 = y0;
    keel::i32 clipX1 = x1;
    keel::i32 clipY1 = y1;
    if (!m_clipStack.empty()) {
        const Rect& clip = m_clipStack.back();
        clipX0 = keel::max(clipX0, keel::floor_i32(clip.x));
        clipY0 = keel::max(clipY0, keel::floor_i32(clip.y));
        clipX1 = keel::min(clipX1, keel::ceil_i32(clip.Right()));
        clipY1 = keel::min(clipY1, keel::ceil_i32(clip.Bottom()));
    }
    if (clipX1 <= clipX0 || clipY1 <= clipY0) {
        return true;
    }

    FlushCanvas();

    const keel::i32 copyW = clipX1 - clipX0;
    const keel::i32 srcOffX = clipX0 - x0;
    for (keel::i32 y = clipY0; y < clipY1; ++y) {
        const keel::i32 srcRow = y - y0;
        std::memcpy(
            m_targetBuffer + static_cast<keel::usize>(y) * m_stride + clipX0,
            pixels.Row(static_cast<keel::u32>(srcRow)).data() + srcOffX,
            static_cast<keel::usize>(copyW) * sizeof(keel::u32)
        );
    }
    return true;
}

keel::UniquePtr<ICanvas> CreateThorVectorCanvas(bool loadDefaultFonts) {
    return keel::make_unique<ThorCanvas>(loadDefaultFonts);
}

namespace {

#ifdef GUIKIT_THORVG_HAS_INTERNAL_LODEPNG
struct LodePngStateGuard {
    LodePNGState state{};
    LodePngStateGuard() noexcept { lodepng_state_init(&state); }
    ~LodePngStateGuard() noexcept { lodepng_state_cleanup(&state); }
    LodePngStateGuard(const LodePngStateGuard&) = delete;
    LodePngStateGuard& operator=(const LodePngStateGuard&) = delete;
};

struct MallocBytesGuard {
    keel::u8* ptr{nullptr};
    explicit MallocBytesGuard(keel::u8* p) noexcept : ptr(p) {}
    ~MallocBytesGuard() noexcept {
        if (ptr) std::free(ptr);
    }
    MallocBytesGuard(const MallocBytesGuard&) = delete;
    MallocBytesGuard& operator=(const MallocBytesGuard&) = delete;
};
#endif

bool DecodeThorImageFromMemory(
    keel::ArrayRef<keel::u8> encodedBytes,
    keel::u32& outWidth,
    keel::u32& outHeight,
    keel::Array<keel::u32>& outRgbaPixels
) {
    outWidth = 0;
    outHeight = 0;
    if (encodedBytes.size() < 8) {
        return false;
    }

#ifdef GUIKIT_THORVG_HAS_INTERNAL_LODEPNG
    static constexpr keel::u8 kPngSignature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (encodedBytes.size() >= 32 && std::memcmp(encodedBytes.data(), kPngSignature, sizeof(kPngSignature)) == 0) {
        // 1. Inspect IHDR dimensions first to guard against decompression bombs
        {
            LodePngStateGuard inspectGuard;
            unsigned inspectW = 0; // keel-lint: ignore
            unsigned inspectH = 0; // keel-lint: ignore
            if (lodepng_inspect(&inspectW, &inspectH, &inspectGuard.state, encodedBytes.data(), encodedBytes.size()) != 0 ||
                inspectW == 0 || inspectH == 0 || inspectW > 4096 || inspectH > 4096) {
                return false;
            }
        }

        // 2. Decode into 8-bit RGBA with RAII cleanup
        LodePngStateGuard decodeGuard;
        decodeGuard.state.info_raw.colortype = LCT_RGBA;
        decodeGuard.state.info_raw.bitdepth = 8;

        keel::u8* rawPtr = nullptr;
        unsigned srcW = 0;         // keel-lint: ignore
        unsigned srcH = 0;         // keel-lint: ignore
        const unsigned err = lodepng_decode( // keel-lint: ignore
            &rawPtr,
            &srcW,
            &srcH,
            &decodeGuard.state,
            encodedBytes.data(),
            encodedBytes.size()
        );
        MallocBytesGuard decodedBytes(rawPtr);

        if (err != 0 || !decodedBytes.ptr || srcW == 0 || srcH == 0 || srcW > 4096 || srcH > 4096) {
            return false;
        }

        if (decodeGuard.state.info_png.iccp_defined) {
            lodepng_toSrgb(decodedBytes.ptr, decodedBytes.ptr, srcW, srcH, &decodeGuard.state);
        }

        outWidth = srcW;
        outHeight = srcH;
        keel::usize totalPx = 0;
        if (!keel::checked_mul(static_cast<keel::usize>(srcW), static_cast<keel::usize>(srcH), totalPx)) {
            return false;
        }
        outRgbaPixels.resize(totalPx);
        const keel::u8* srcBuf = decodedBytes.ptr;

#if !defined(__BYTE_ORDER__) || (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
        std::memcpy(outRgbaPixels.data(), srcBuf, totalPx * sizeof(keel::u32));
#else
        for (keel::usize i = 0; i < totalPx; ++i) {
            outRgbaPixels[i] = keel::read_le32(&srcBuf[i * 4u]);
        }
#endif
        return true;
    }
#endif

    // Generic ThorVG public API fallback (supports PNG, JPEG, WebP, SVG)
    struct TvgInitScope {
        TvgInitScope() noexcept { tvg::Initializer::init(0); }
        ~TvgInitScope() noexcept { tvg::Initializer::term(); }
    } initScope;

    auto* pic = tvg::Picture::gen();
    if (!pic) return false;

    const char* mime = "png";
    if (encodedBytes.size() >= 3 && encodedBytes[0] == 0xFF && encodedBytes[1] == 0xD8) {
        mime = "jpg";
    } else if (encodedBytes.size() >= 12 && std::memcmp(encodedBytes.data(), "RIFF", 4) == 0 &&
               std::memcmp(encodedBytes.data() + 8, "WEBP", 4) == 0) {
        mime = "webp";
    }

    if (pic->load(
            reinterpret_cast<const char*>(encodedBytes.data()),
            static_cast<keel::u32>(encodedBytes.size()),
            mime,
            nullptr,
            true) != tvg::Result::Success) {
        delete pic;
        return false;
    }

    float wf = 0.0f;
    float hf = 0.0f;
    pic->size(&wf, &hf);
    const keel::u32 w = keel::round_u32(wf);
    const keel::u32 h = keel::round_u32(hf);
    if (w == 0 || h == 0 || w > 4096 || h > 4096) {
        delete pic;
        return false;
    }

    outWidth = w;
    outHeight = h;
    outRgbaPixels.assign(static_cast<keel::usize>(w) * h, 0u);

    auto* swCanvas = tvg::SwCanvas::gen();
    if (!swCanvas) {
        delete pic;
        return false;
    }
    if (swCanvas->target(outRgbaPixels.data(), w, w, h, tvg::ColorSpace::ABGR8888S) != tvg::Result::Success) {
        delete pic;
        delete swCanvas;
        return false;
    }
    swCanvas->add(pic);
    swCanvas->draw(true);
    swCanvas->sync();
    delete swCanvas;
    return true;
}

} // namespace

void RegisterThorVectorBackend() noexcept {
    RegisterDefaultVectorCanvasFactory(&CreateThorVectorCanvas);
    RegisterDefaultImageDecoder(&DecodeThorImageFromMemory);
}

namespace {
struct ThorVectorBackendModuleRegistrar {
    ThorVectorBackendModuleRegistrar() noexcept { RegisterThorVectorBackend(); }
} const g_thorVectorBackendModuleRegistrar{};
} // namespace

} // namespace guikit
