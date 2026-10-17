#include "guikit/backend/ThorCanvas.hpp"
#include "guikit/backend/VectorBackend.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <thread>
#include "guikit/core/Assets.hpp"
#include "guikit/core/TextUtil.hpp"
#include "guikit/core/Tokens.hpp"
#include "thorvg.h"

namespace guikit {

namespace {

std::string ExtractFontName(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    const std::string base = (slash == std::string::npos) ? path : path.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot > 0) {
        return base.substr(0, dot);
    }
    return base;
}

[[nodiscard]] inline uint32_t AlphaBlendSwar(uint32_t c, uint32_t a) noexcept {
    ++a;
    return (((((c >> 8) & 0x00FF00FFu) * a) & 0xFF00FF00u) +
            ((((c & 0x00FF00FFu) * a) >> 8) & 0x00FF00FFu));
}

[[nodiscard]] inline uint32_t BlendPreNormalSwar(uint32_t s, uint32_t d) noexcept {
    const uint32_t ia = ((~s) >> 24) + 1u;
    return s + (((((d >> 8) & 0x00FF00FFu) * ia) & 0xFF00FF00u) +
                ((((d & 0x00FF00FFu) * ia) >> 8) & 0x00FF00FFu));
}

[[nodiscard]] inline uint32_t PremultiplyColor(Color c) noexcept {
    if (c.a == 255) {
        return c.ToUint32();
    }
    if (c.a == 0) {
        return 0u;
    }
    const uint32_t a = c.a;
    const uint32_t r = (static_cast<uint32_t>(c.r) * a + 127u) / 255u;
    const uint32_t g = (static_cast<uint32_t>(c.g) * a + 127u) / 255u;
    const uint32_t b = (static_cast<uint32_t>(c.b) * a + 127u) / 255u;
    return (a << 24) | (b << 16) | (g << 8) | r;
}

} // namespace

ThorCanvas::ThorCanvas(bool loadDefaultFonts) {
    // Use 0 worker threads (synchronous main-thread rasterization) to eliminate
    // multi-core mutex contention and condition_variable wakeups on UI primitives.
    tvg::Initializer::init(0);
    m_canvas = tvg::SwCanvas::gen(tvg::EngineOption::None);

    if (!loadDefaultFonts) {
        return;
    }

    // Auto-detect default fonts across embedded assets and system locations
    const char* defaultFontCandidates[] = {
        "assets/fonts/Roboto-Regular.ttf",
        "../assets/fonts/Roboto-Regular.ttf",
        "../../assets/fonts/Roboto-Regular.ttf",
        "../../../assets/fonts/Roboto-Regular.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf"
    };
    for (const char* candidate : defaultFontCandidates) {
        if (LoadFontFile(candidate)) {
            break;
        }
    }

    // Auto-detect monospace fonts for code editors & code blocks
    const char* monoFontCandidates[] = {
        "assets/fonts/JetBrainsMono-Regular.ttf",
        "../assets/fonts/JetBrainsMono-Regular.ttf",
        "../../assets/fonts/JetBrainsMono-Regular.ttf",
        "assets/fonts/RobotoMono-Regular.ttf",
        "../assets/fonts/RobotoMono-Regular.ttf",
        "../../assets/fonts/RobotoMono-Regular.ttf",
        "C:/Windows/Fonts/CascadiaMono.ttf",
        "C:/Windows/Fonts/CascadiaCode.ttf",
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/cour.ttf"
    };
    for (const char* candidate : monoFontCandidates) {
        if (LoadMonospaceFontFile(candidate)) {
            break;
        }
    }
}

ThorCanvas::~ThorCanvas() {
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

void ThorCanvas::SyncViewportClip() {
    if (!m_canvas || m_width == 0 || m_height == 0) return;
    FlushCanvas();
    if (m_clipStack.empty()) {
        m_canvas->viewport(0, 0, static_cast<int32_t>(m_width), static_cast<int32_t>(m_height));
    } else {
        const Rect& cr = m_clipStack.back();
        const int32_t x0 = std::clamp(static_cast<int32_t>(std::floor(cr.x)), 0, static_cast<int32_t>(m_width));
        const int32_t y0 = std::clamp(static_cast<int32_t>(std::floor(cr.y)), 0, static_cast<int32_t>(m_height));
        const int32_t x1 = std::clamp(static_cast<int32_t>(std::ceil(cr.Right())), 0, static_cast<int32_t>(m_width));
        const int32_t y1 = std::clamp(static_cast<int32_t>(std::ceil(cr.Bottom())), 0, static_cast<int32_t>(m_height));
        m_canvas->viewport(x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0));
    }
}

void ThorCanvas::SetTarget(uint32_t* buffer, uint32_t width, uint32_t height, uint32_t stride) {
    m_targetBuffer = buffer;
    m_width = width;
    m_height = height;
    m_stride = stride;
    m_hasPendingPaints = false;

    if (m_canvas && m_targetBuffer && m_width > 0 && m_height > 0) {
        m_canvas->target(m_targetBuffer, m_stride, m_width, m_height, tvg::ColorSpace::ABGR8888);
    }
}

bool ThorCanvas::LoadFontFile(const std::string& path) {
    if (path.empty()) return false;

    if (auto asset = Assets::Find(path); asset && !asset->Empty()) {
        const std::string fontName = ExtractFontName(path);
        const char* mime = asset->mimeType.empty() ? "ttf" : asset->mimeType.data();
        if (tvg::Text::load(
                fontName.c_str(),
                reinterpret_cast<const char*>(asset->data),
                static_cast<uint32_t>(asset->size),
                mime,
                false) == tvg::Result::Success) {
            m_defaultFontPath = Assets::NormalizePath(path);
            m_defaultFontName = fontName;
            m_fontLoaded = true;
            m_textMeasureCache.clear();
            m_textStampCache.clear();
            return true;
        }
    }

    tvg::Result res = tvg::Text::load(path.c_str());
    if (res == tvg::Result::Success) {
        m_defaultFontPath = path;
        m_defaultFontName = ExtractFontName(path);
        m_fontLoaded = true;
        m_textMeasureCache.clear();
        m_textStampCache.clear();
        return true;
    }
    return false;
}

bool ThorCanvas::LoadMonospaceFontFile(const std::string& path) {
    if (path.empty()) return false;

    if (auto asset = Assets::Find(path); asset && !asset->Empty()) {
        const std::string fontName = ExtractFontName(path);
        const char* mime = asset->mimeType.empty() ? "ttf" : asset->mimeType.data();
        if (tvg::Text::load(
                fontName.c_str(),
                reinterpret_cast<const char*>(asset->data),
                static_cast<uint32_t>(asset->size),
                mime,
                false) == tvg::Result::Success) {
            m_monoFontPath = Assets::NormalizePath(path);
            m_monoFontName = fontName;
            m_monoFontLoaded = true;
            m_textMeasureCache.clear();
            m_textStampCache.clear();
            return true;
        }
    }

    tvg::Result res = tvg::Text::load(path.c_str());
    if (res == tvg::Result::Success) {
        m_monoFontPath = path;
        m_monoFontName = ExtractFontName(path);
        m_monoFontLoaded = true;
        m_textMeasureCache.clear();
        m_textStampCache.clear();
        return true;
    }
    return false;
}

void ThorCanvas::ApplyFont(tvg::Text* txt, FontStyle style) const {
    if (!txt) return;
    if (style == FontStyle::Monospace && m_monoFontLoaded) {
        if (!m_monoFontName.empty() && txt->font(m_monoFontName.c_str()) == tvg::Result::Success) {
            return;
        }
        if (!m_monoFontPath.empty() && txt->font(m_monoFontPath.c_str()) == tvg::Result::Success) {
            return;
        }
    }
    if (m_fontLoaded) {
        if (!m_defaultFontName.empty() && txt->font(m_defaultFontName.c_str()) == tvg::Result::Success) {
            return;
        }
        if (!m_defaultFontPath.empty() && txt->font(m_defaultFontPath.c_str()) == tvg::Result::Success) {
            return;
        }
        txt->font(nullptr);
    }
}

void ThorCanvas::Begin(uint32_t width, uint32_t height, float scale) {
    m_scale = (scale > 0.0f) ? scale : 1.0f;
    m_clipStack.clear();
    m_gpuClipStack.clear();
    if (m_textStampCache.size() > 4096) {
        m_textStampCache.clear();
    }
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

void ThorCanvas::End() {
    FlushCanvas();
}

void ThorCanvas::Clear(Color color, std::optional<Rect> rect) {
    if (!m_targetBuffer || m_width == 0 || m_height == 0) return;

    FlushCanvas();
    const uint32_t rawColor = color.ToUint32();

    if (!rect.has_value()) {
        m_sonarGpuPasses.clear();
        m_spectrogramGpuPasses.clear();
        m_waterfallGpuPasses.clear();
        for (uint32_t y = 0; y < m_height; ++y) {
            std::fill_n(m_targetBuffer + y * m_stride, m_width, rawColor);
        }
    } else {
        const Rect sClear(rect->x * m_scale, rect->y * m_scale, rect->w * m_scale, rect->h * m_scale);
        std::erase_if(m_sonarGpuPasses, [&](const SonarGpuFieldDesc& p) {
            return p.plotRect.Intersects(sClear);
        });
        std::erase_if(m_spectrogramGpuPasses, [&](const SpectrogramGpuFieldDesc& p) {
            return p.plotRect.Intersects(sClear);
        });
        std::erase_if(m_waterfallGpuPasses, [&](const WaterfallGpuFieldDesc& p) {
            return p.plotRect.Intersects(sClear);
        });

        const int startX = std::max(0, static_cast<int>(std::floor(sClear.x)));
        const int startY = std::max(0, static_cast<int>(std::floor(sClear.y)));
        const int endX = std::min(static_cast<int>(m_width), static_cast<int>(std::ceil(sClear.Right())));
        const int endY = std::min(static_cast<int>(m_height), static_cast<int>(std::ceil(sClear.Bottom())));
        const int w = endX - startX;
        if (w > 0 && endY > startY) {
            for (int y = startY; y < endY; ++y) {
                std::fill_n(m_targetBuffer + y * m_stride + startX, w, rawColor);
            }
        }
    }
}

void ThorCanvas::PushClip(Rect clipRect) {
    const float sx1 = std::floor(clipRect.x * m_scale);
    const float sy1 = std::floor(clipRect.y * m_scale);
    const float sx2 = std::ceil(clipRect.Right() * m_scale);
    const float sy2 = std::ceil(clipRect.Bottom() * m_scale);
    const Rect sClip(sx1, sy1, std::max(0.0f, sx2 - sx1), std::max(0.0f, sy2 - sy1));
    if (!m_clipStack.empty()) {
        const Rect& parent = m_clipStack.back();
        const float x1 = std::max(parent.x, sClip.x);
        const float y1 = std::max(parent.y, sClip.y);
        const float x2 = std::min(parent.Right(), sClip.Right());
        const float y2 = std::min(parent.Bottom(), sClip.Bottom());
        m_clipStack.emplace_back(x1, y1, std::max(0.0f, x2 - x1), std::max(0.0f, y2 - y1));

        const Rect& gpuParent = m_gpuClipStack.back();
        const float gx1 = std::max(gpuParent.x, sClip.x);
        const float gy1 = std::max(gpuParent.y, sClip.y);
        const float gx2 = std::min(gpuParent.Right(), sClip.Right());
        const float gy2 = std::min(gpuParent.Bottom(), sClip.Bottom());
        m_gpuClipStack.emplace_back(gx1, gy1, std::max(0.0f, gx2 - gx1), std::max(0.0f, gy2 - gy1));
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

void ThorCanvas::ApplyClip(tvg::Paint* paint, std::optional<Rect> bounds) {
    // Hardware/engine integer viewport clipping is synchronized on PushClip/PopClip via m_canvas->viewport(),
    // so per-paint RLE clip masks are never needed for rectangular stack clips.
    (void)paint;
    (void)bounds;
}

void ThorCanvas::DrawRect(Rect rect, Color fill) {
    if (fill.a == 0 || rect.w <= 0.0f || rect.h <= 0.0f || !m_canvas) return;
    Rect sRect(rect.x * m_scale, rect.y * m_scale, rect.w * m_scale, rect.h * m_scale);
    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        if (cr.w <= 0.0f || cr.h <= 0.0f) return;
        sRect = sRect.Intersect(cr);
        if (sRect.w <= 0.0f || sRect.h <= 0.0f) return;
    }

    if (m_targetBuffer && m_width > 0 && m_height > 0) {
        const int x0 = std::clamp(static_cast<int>(std::round(sRect.x)), 0, static_cast<int>(m_width));
        const int y0 = std::clamp(static_cast<int>(std::round(sRect.y)), 0, static_cast<int>(m_height));
        const int x1 = std::clamp(static_cast<int>(std::round(sRect.Right())), 0, static_cast<int>(m_width));
        const int y1 = std::clamp(static_cast<int>(std::round(sRect.Bottom())), 0, static_cast<int>(m_height));
        const int w = x1 - x0;
        if (w > 0 && y1 > y0) {
            FlushCanvas();
            if (fill.a == 255) {
                const uint32_t rawCol = fill.ToUint32();
                for (int y = y0; y < y1; ++y) {
                    std::fill_n(m_targetBuffer + static_cast<size_t>(y) * m_stride + x0, w, rawCol);
                }
            } else {
                const uint32_t sPre = PremultiplyColor(fill);
                for (int y = y0; y < y1; ++y) {
                    uint32_t* dst = m_targetBuffer + static_cast<size_t>(y) * m_stride + x0;
                    for (int x = 0; x < w; ++x) {
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

    const float sRadius = std::max(0.0f, radius * m_scale);
    if (sRadius <= 0.05f) {
        if (stroke.a == 0 || strokeWidth <= 0.0f) {
            DrawRect(rect, fill);
            return;
        }
        if (fill.a > 0) {
            DrawRect(rect, fill);
        }
        const float hw = strokeWidth * 0.5f;
        const float x0 = rect.x - hw;
        const float y0 = rect.y - hw;
        const float w = rect.w + strokeWidth;
        const float h = rect.h + strokeWidth;
        DrawRect(Rect(x0, y0, w, strokeWidth), stroke);
        DrawRect(Rect(x0, y0 + h - strokeWidth, w, strokeWidth), stroke);
        if (h > strokeWidth * 2.0f) {
            DrawRect(Rect(x0, y0 + strokeWidth, strokeWidth, h - strokeWidth * 2.0f), stroke);
            DrawRect(Rect(x0 + w - strokeWidth, y0 + strokeWidth, strokeWidth, h - strokeWidth * 2.0f), stroke);
        }
        return;
    }

    const Rect sRect(rect.x * m_scale, rect.y * m_scale, rect.w * m_scale, rect.h * m_scale);
    const float sStrokeW = (stroke.a > 0 && strokeWidth > 0.0f) ? (strokeWidth * m_scale) : 0.0f;
    const float pad = (sStrokeW > 0.0f) ? (sStrokeW * 0.5f + 1.0f) : 1.0f;
    const Rect outerBounds(sRect.x - pad, sRect.y - pad, sRect.w + pad * 2.0f, sRect.h + pad * 2.0f);
    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        if (cr.w <= 0.0f || cr.h <= 0.0f || !outerBounds.Intersects(cr)) return;
    }

    if (m_targetBuffer && m_width > 0 && m_height > 0 &&
        ((sRect.w <= 320.0f && sRect.h <= 80.0f) || (sRect.w <= 96.0f && sRect.h <= 96.0f))) {
        const int dstX = static_cast<int>(std::round(sRect.x));
        const int dstY = static_cast<int>(std::round(sRect.y));
        const TextStampEntry& stamp = ResolveShapeStamp(sRect.w, sRect.h, sRadius, fill, stroke, sStrokeW);
        BlitTextStamp(stamp, dstX, dstY);
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

Vec2 ThorCanvas::MeasureText(std::string_view text, float fontSize) {
    return {MeasureTextStyled(text, fontSize, FontStyle::Regular).x, GetLineHeight(fontSize)};
}

Vec2 ThorCanvas::MeasureTextStyled(std::string_view text, float fontSize, FontStyle style) {
    if (text.empty() || fontSize <= 0.0f) {
        return {0.0f, 0.0f};
    }

    uint64_t h = 14695981039346656037ULL;
    for (char c : text) {
        h = (h ^ static_cast<uint8_t>(c)) * 1099511628211ULL;
    }
    uint32_t fsBits = 0;
    std::memcpy(&fsBits, &fontSize, sizeof(float));
    const TextMeasureKey key{h, fsBits, static_cast<uint8_t>(style)};

    auto it = m_textMeasureCache.find(key);
    if (it != m_textMeasureCache.end()) {
        return it->second;
    }

    if (!m_measureText) {
        m_measureText = tvg::Text::gen();
    }
    ApplyFont(m_measureText, style);

    m_measureText->size(fontSize);
    std::string textStr(text);
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
    if (style == FontStyle::Bold || style == FontStyle::BoldItalic) {
        totalAdvance += 0.6f;
    }

    float glyphBoxH = GetLineHeight(fontSize);
    tvg::TextMetrics tm{};
    if (m_measureText->metrics(tm) == tvg::Result::Success && (tm.ascent - tm.descent) > 0.0f) {
        glyphBoxH = tm.ascent - tm.descent;
    }

    const Vec2 result{totalAdvance, glyphBoxH};

    if (m_textMeasureCache.size() > 2048) {
        m_textMeasureCache.clear();
    }
    m_textMeasureCache.emplace(key, result);
    return result;
}

float ThorCanvas::GetLineHeight(float fontSize) {
    if (fontSize <= 0.0f) return 0.0f;
    return fontSize * kDefaultLineHeightMultiplier;
}

const ThorCanvas::TextStampEntry& ThorCanvas::ResolveTextStamp(
    std::string_view text,
    float scaledFontSize,
    Color color,
    FontStyle style
) {
    uint64_t h = 14695981039346656037ULL;
    for (char c : text) {
        h = (h ^ static_cast<uint8_t>(c)) * 1099511628211ULL;
    }
    const float qFontSize = std::max(1.0f, std::round(scaledFontSize * 4.0f) * 0.25f);
    uint32_t fsBits = 0;
    std::memcpy(&fsBits, &qFontSize, sizeof(float));

    const TextStampKey key{
        .textHash = h,
        .fontSizeBits = fsBits,
        .colorRgba = color.ToUint32(),
        .textLen = static_cast<uint16_t>(std::min<size_t>(text.size(), 65535u)),
        .style = static_cast<uint8_t>(style)
    };

    auto it = m_textStampCache.find(key);
    if (it != m_textStampCache.end()) {
        return it->second;
    }

    if (m_textStampCache.size() > 4096) {
        m_textStampCache.clear();
    }

    TextStampEntry entry{};
    const Vec2 m = MeasureTextStyled(text, qFontSize, style);
    if (m.x <= 0.0f || m.y <= 0.0f) {
        entry.empty = true;
        auto [insIt, _] = m_textStampCache.emplace(key, std::move(entry));
        return insIt->second;
    }

    const int padLeft = 3;
    const int padTop = std::max(3, static_cast<int>(std::ceil(qFontSize * 0.25f)));
    const int padRight = std::max(6, static_cast<int>(std::ceil(qFontSize * 0.35f)));
    const int padBottom = std::max(4, static_cast<int>(std::ceil(qFontSize * 0.40f)));
    const int bufW = std::clamp(static_cast<int>(std::ceil(m.x)) + padLeft + padRight, 4, 2048);
    const int bufH = std::clamp(static_cast<int>(std::ceil(m.y)) + padTop + padBottom, 4, 256);

    if (!m_stampCanvas) {
        m_stampCanvas = tvg::SwCanvas::gen(tvg::EngineOption::None);
    }

    m_stampScratch.assign(static_cast<size_t>(bufW) * static_cast<size_t>(bufH), 0u);
    m_stampCanvas->target(m_stampScratch.data(), static_cast<uint32_t>(bufW), static_cast<uint32_t>(bufW), static_cast<uint32_t>(bufH), tvg::ColorSpace::ABGR8888);

    const bool isItalic = (style == FontStyle::Italic || style == FontStyle::BoldItalic);
    const bool isBold = (style == FontStyle::Bold || style == FontStyle::BoldItalic);
    std::string textStr(text);

    auto addStampPass = [&](float offsetX) {
        auto* txt = tvg::Text::gen();
        txt->text(textStr.c_str());
        ApplyFont(txt, style);
        txt->size(qFontSize);
        if (isItalic) {
            txt->italic(0.18f);
        }
        txt->fill(color.r, color.g, color.b);
        txt->opacity(color.a);
        txt->align(0.0f, 0.0f);
        txt->translate(static_cast<float>(padLeft) + offsetX, static_cast<float>(padTop));
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

    int minX = bufW;
    int minY = bufH;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < bufH; ++y) {
        const uint32_t* row = m_stampScratch.data() + static_cast<size_t>(y) * bufW;
        for (int x = 0; x < bufW; ++x) {
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
        const int cropW = maxX - minX + 1;
        const int cropH = maxY - minY + 1;
        entry.width = static_cast<uint16_t>(cropW);
        entry.height = static_cast<uint16_t>(cropH);
        entry.offsetX = static_cast<int16_t>(minX - padLeft);
        entry.offsetY = static_cast<int16_t>(minY - padTop);
        entry.pixels.resize(static_cast<size_t>(cropW) * static_cast<size_t>(cropH));
        for (int row = 0; row < cropH; ++row) {
            std::memcpy(
                entry.pixels.data() + static_cast<size_t>(row) * cropW,
                m_stampScratch.data() + static_cast<size_t>(minY + row) * bufW + minX,
                static_cast<size_t>(cropW) * sizeof(uint32_t)
            );
        }
    }

    auto [insIt, _] = m_textStampCache.emplace(key, std::move(entry));
    return insIt->second;
}

const ThorCanvas::TextStampEntry& ThorCanvas::ResolveShapeStamp(
    float scaledW,
    float scaledH,
    float scaledRadius,
    Color fill,
    Color stroke,
    float scaledStrokeWidth
) {
    const uint16_t qW = static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(scaledW * 4.0f)), 1, 4096));
    const uint16_t qH = static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(scaledH * 4.0f)), 1, 4096));
    const uint16_t qRad = static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(scaledRadius * 4.0f)), 0, 4096));
    const uint16_t qStrokeW = (stroke.a > 0 && scaledStrokeWidth > 0.0f)
        ? static_cast<uint16_t>(std::clamp(static_cast<int>(std::round(scaledStrokeWidth * 4.0f)), 1, 1024))
        : 0u;

    const ShapeStampKey key{
        .fillRgba = (fill.a > 0) ? fill.ToUint32() : 0u,
        .strokeRgba = (qStrokeW > 0) ? stroke.ToUint32() : 0u,
        .qW = qW,
        .qH = qH,
        .qRad = qRad,
        .qStrokeW = qStrokeW
    };

    auto it = m_shapeStampCache.find(key);
    if (it != m_shapeStampCache.end()) {
        return it->second;
    }

    if (m_shapeStampCache.size() > 1024) {
        m_shapeStampCache.clear();
    }

    TextStampEntry entry{};
    const float w = static_cast<float>(qW) * 0.25f;
    const float h = static_cast<float>(qH) * 0.25f;
    const float rad = static_cast<float>(qRad) * 0.25f;
    const float sw = static_cast<float>(qStrokeW) * 0.25f;

    const int pad = static_cast<int>(std::ceil(sw * 0.5f)) + 2;
    const int bufW = std::clamp(static_cast<int>(std::ceil(w)) + pad * 2, 4, 512);
    const int bufH = std::clamp(static_cast<int>(std::ceil(h)) + pad * 2, 4, 256);

    if (!m_stampCanvas) {
        m_stampCanvas = tvg::SwCanvas::gen(tvg::EngineOption::None);
    }

    m_stampScratch.assign(static_cast<size_t>(bufW) * static_cast<size_t>(bufH), 0u);
    m_stampCanvas->target(m_stampScratch.data(), static_cast<uint32_t>(bufW), static_cast<uint32_t>(bufW), static_cast<uint32_t>(bufH), tvg::ColorSpace::ABGR8888);

    auto* shape = tvg::Shape::gen();
    shape->appendRect(static_cast<float>(pad), static_cast<float>(pad), w, h, rad, rad);
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

    int minX = bufW;
    int minY = bufH;
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < bufH; ++y) {
        const uint32_t* row = m_stampScratch.data() + static_cast<size_t>(y) * bufW;
        for (int x = 0; x < bufW; ++x) {
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
        const int cropW = maxX - minX + 1;
        const int cropH = maxY - minY + 1;
        entry.width = static_cast<uint16_t>(cropW);
        entry.height = static_cast<uint16_t>(cropH);
        entry.offsetX = static_cast<int16_t>(minX - pad);
        entry.offsetY = static_cast<int16_t>(minY - pad);
        entry.pixels.resize(static_cast<size_t>(cropW) * static_cast<size_t>(cropH));
        for (int row = 0; row < cropH; ++row) {
            std::memcpy(
                entry.pixels.data() + static_cast<size_t>(row) * cropW,
                m_stampScratch.data() + static_cast<size_t>(minY + row) * bufW + minX,
                static_cast<size_t>(cropW) * sizeof(uint32_t)
            );
        }
    }

    auto [insIt, _] = m_shapeStampCache.emplace(key, std::move(entry));
    return insIt->second;
}

void ThorCanvas::BlitTextStamp(
    const TextStampEntry& stamp,
    int dstX,
    int dstY,
    std::optional<Rect> extraClip
) {
    if (stamp.empty || stamp.pixels.empty() || !m_targetBuffer || m_width == 0 || m_height == 0) {
        return;
    }

    const int x0 = dstX + stamp.offsetX;
    const int y0 = dstY + stamp.offsetY;
    const int x1 = x0 + static_cast<int>(stamp.width);
    const int y1 = y0 + static_cast<int>(stamp.height);

    int clipX0 = std::max(0, x0);
    int clipY0 = std::max(0, y0);
    int clipX1 = std::min(static_cast<int>(m_width), x1);
    int clipY1 = std::min(static_cast<int>(m_height), y1);

    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        clipX0 = std::max(clipX0, static_cast<int>(std::floor(cr.x)));
        clipY0 = std::max(clipY0, static_cast<int>(std::floor(cr.y)));
        clipX1 = std::min(clipX1, static_cast<int>(std::ceil(cr.Right())));
        clipY1 = std::min(clipY1, static_cast<int>(std::ceil(cr.Bottom())));
    }
    if (extraClip.has_value()) {
        clipX0 = std::max(clipX0, static_cast<int>(std::floor(extraClip->x)));
        clipY0 = std::max(clipY0, static_cast<int>(std::floor(extraClip->y)));
        clipX1 = std::min(clipX1, static_cast<int>(std::ceil(extraClip->Right())));
        clipY1 = std::min(clipY1, static_cast<int>(std::ceil(extraClip->Bottom())));
    }

    if (clipX0 >= clipX1 || clipY0 >= clipY1) {
        return;
    }

    FlushCanvas();

    const int spanW = clipX1 - clipX0;
    const int srcOffX = clipX0 - x0;
    for (int y = clipY0; y < clipY1; ++y) {
        const uint32_t* srcRow = stamp.pixels.data() + static_cast<size_t>(y - y0) * stamp.width + srcOffX;
        uint32_t* dstRow = m_targetBuffer + static_cast<size_t>(y) * m_stride + clipX0;
        for (int x = 0; x < spanW; ++x) {
            const uint32_t s = srcRow[x];
            if (s == 0u) continue;
            if (s >= 0xFF000000u) {
                dstRow[x] = s;
            } else {
                dstRow[x] = BlendPreNormalSwar(s, dstRow[x]);
            }
        }
    }
}

void ThorCanvas::DrawText(std::string_view text, Vec2 pos, float fontSize, Color color) {
    DrawTextStyled(text, pos, fontSize, color, FontStyle::Regular);
}

void ThorCanvas::DrawTextStyled(
    std::string_view text,
    Vec2 pos,
    float fontSize,
    Color color,
    FontStyle style,
    float angleDeg
) {
    if (text.empty() || color.a == 0 || !m_canvas) return;
    if (!m_clipStack.empty() && (m_clipStack.back().w <= 0.0f || m_clipStack.back().h <= 0.0f)) return;

    const bool hasRot = std::abs(angleDeg) > 0.01f;
    if (!hasRot && m_targetBuffer && m_width > 0 && m_height > 0 && text.find('\n') == std::string_view::npos) {
        const std::string_view stampText = (text.size() > 512) ? text.substr(0, 512) : text;
        const Vec2 m = MeasureTextStyled(stampText, fontSize, style);
        const Rect approxBounds(
            (pos.x - 2.0f) * m_scale,
            (pos.y - fontSize * 0.25f) * m_scale,
            (m.x + 6.0f) * m_scale,
            (m.y + fontSize * 0.5f) * m_scale
        );
        if (!m_clipStack.empty() && !approxBounds.Intersects(m_clipStack.back())) {
            return;
        }
        const int dstX = static_cast<int>(std::round(pos.x * m_scale));
        const int dstY = static_cast<int>(std::round(pos.y * m_scale));
        const TextStampEntry& stamp = ResolveTextStamp(stampText, fontSize * m_scale, color, style);
        BlitTextStamp(stamp, dstX, dstY);
        return;
    }

    const bool isItalic = (style == FontStyle::Italic || style == FontStyle::BoldItalic);
    const bool isBold = (style == FontStyle::Bold || style == FontStyle::BoldItalic);
    const float rad = hasRot ? (angleDeg * 0.017453292519943295f) : 0.0f;
    const float cosA = hasRot ? std::cos(rad) : 1.0f;
    const float sinA = hasRot ? std::sin(rad) : 0.0f;

    auto emitTextPass = [&](float offset) {
        auto txt = tvg::Text::gen();
        std::string textStr(text);
        txt->text(textStr.c_str());

        ApplyFont(txt, style);

        txt->size(fontSize * m_scale);
        if (isItalic) {
            txt->italic(0.18f);
        }
        txt->fill(color.r, color.g, color.b);
        txt->opacity(color.a);
        txt->align(0.0f, 0.0f);
        if (hasRot) {
            txt->rotate(angleDeg);
        }
        txt->translate((pos.x + offset * cosA) * m_scale, (pos.y + offset * sinA) * m_scale);

        m_canvas->add(txt);
        m_hasPendingPaints = true;
    };

    emitTextPass(0.0f);
    if (isBold) {
        emitTextPass(0.55f);
    }
}

void ThorCanvas::DrawTextBox(
    std::string_view text,
    Rect bounds,
    float fontSize,
    Color color,
    float alignX,
    float alignY
) {
    if (text.empty() || color.a == 0 || bounds.w <= 0.0f || bounds.h <= 0.0f || !m_canvas) return;
    const Rect sBounds(bounds.x * m_scale, bounds.y * m_scale, bounds.w * m_scale, bounds.h * m_scale);
    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        if (cr.w <= 0.0f || cr.h <= 0.0f || !sBounds.Intersects(cr)) return;
    }

    std::string truncated;
    std::string_view drawText = text;
    if (bounds.w > 0.0f && MeasureText(text, fontSize).x > bounds.w + kTextSubpixelTolerance) {
        truncated = TextUtil::TruncateWithEllipsis(text, bounds.w, fontSize, this);
        drawText = truncated;
    }
    if (drawText.empty()) return;

    if (m_targetBuffer && m_width > 0 && m_height > 0 &&
        drawText.find('\n') == std::string_view::npos) {
        const std::string_view stampText = (drawText.size() > 512) ? drawText.substr(0, 512) : drawText;
        const Vec2 m = MeasureTextStyled(stampText, fontSize, FontStyle::Regular);
        const float tx = (bounds.x + (bounds.w - m.x) * alignX) * m_scale;
        const float ty = (bounds.y + (bounds.h - m.y) * alignY) * m_scale;
        const int dstX = static_cast<int>(std::round(tx));
        const int dstY = static_cast<int>(std::round(ty));
        const TextStampEntry& stamp = ResolveTextStamp(stampText, fontSize * m_scale, color, FontStyle::Regular);
        BlitTextStamp(stamp, dstX, dstY, Rect(sBounds.x - 2.0f, sBounds.y - 2.0f, sBounds.w + 4.0f, sBounds.h + 4.0f));
        return;
    }

    auto txt = tvg::Text::gen();
    std::string textStr(drawText);
    txt->text(textStr.c_str());

    ApplyFont(txt, FontStyle::Regular);

    txt->size(fontSize * m_scale);
    txt->fill(color.r, color.g, color.b);
    txt->opacity(color.a);
    txt->layout(sBounds.w, sBounds.h);
    txt->align(alignX, alignY);
    txt->translate(sBounds.x, sBounds.y);

    m_canvas->add(txt);
    m_hasPendingPaints = true;
}

void ThorCanvas::DrawLine(Vec2 p1, Vec2 p2, Color color, float strokeWidth) {
    if (color.a == 0 || strokeWidth <= 0.0f || !m_canvas) return;
    if (std::abs(p1.y - p2.y) <= 0.05f) {
        const float x0 = std::min(p1.x, p2.x);
        const float x1 = std::max(p1.x, p2.x);
        const float hw = strokeWidth * 0.5f;
        DrawRect(Rect(x0 - hw, p1.y - hw, (x1 - x0) + strokeWidth, strokeWidth), color);
        return;
    }
    if (std::abs(p1.x - p2.x) <= 0.05f) {
        const float y0 = std::min(p1.y, p2.y);
        const float y1 = std::max(p1.y, p2.y);
        const float hw = strokeWidth * 0.5f;
        DrawRect(Rect(p1.x - hw, y0 - hw, strokeWidth, (y1 - y0) + strokeWidth), color);
        return;
    }
    Vec2 sp1(p1.x * m_scale, p1.y * m_scale);
    Vec2 sp2(p2.x * m_scale, p2.y * m_scale);
    if (!std::isfinite(sp1.x) || !std::isfinite(sp1.y) || !std::isfinite(sp2.x) || !std::isfinite(sp2.y)) {
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
        clipMinX = std::max(clipMinX, cr.x - pad);
        clipMinY = std::max(clipMinY, cr.y - pad);
        clipMaxX = std::min(clipMaxX, cr.Right() + pad);
        clipMaxY = std::min(clipMaxY, cr.Bottom() + pad);
    }

    const float dx = sp2.x - sp1.x;
    const float dy = sp2.y - sp1.y;
    float t0 = 0.0f;
    float t1 = 1.0f;
    auto clipTest = [&](float p, float q) noexcept -> bool {
        if (std::abs(p) < 1e-7f) return q >= 0.0f;
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
    const Vec2 sp1(p1.x * m_scale, p1.y * m_scale);
    const Vec2 sp2(p2.x * m_scale, p2.y * m_scale);
    const Vec2 sp3(p3.x * m_scale, p3.y * m_scale);
    const float pad = strokeWidth * m_scale + 1.0f;
    const float minX = std::min({sp1.x, sp2.x, sp3.x}) - pad;
    const float minY = std::min({sp1.y, sp2.y, sp3.y}) - pad;
    const float maxX = std::max({sp1.x, sp2.x, sp3.x}) + pad;
    const float maxY = std::max({sp1.y, sp2.y, sp3.y}) + pad;
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
    static_assert(static_cast<uint8_t>(PathVerb::Close) == static_cast<uint8_t>(tvg::PathCommand::Close));
    static_assert(static_cast<uint8_t>(PathVerb::MoveTo) == static_cast<uint8_t>(tvg::PathCommand::MoveTo));
    static_assert(static_cast<uint8_t>(PathVerb::LineTo) == static_cast<uint8_t>(tvg::PathCommand::LineTo));
    static_assert(static_cast<uint8_t>(PathVerb::CubicTo) == static_cast<uint8_t>(tvg::PathCommand::CubicTo));
    static_assert(sizeof(Vec2) == sizeof(tvg::Point) && alignof(Vec2) == alignof(tvg::Point));

    if (path.Empty() || !m_canvas) return;
    const bool hasFill = paint.renderFill && paint.fillColor.a > 0;
    const bool hasStroke = paint.renderStroke && paint.strokeColor.a > 0 && paint.strokeWidthPx > 0.0f;
    if (!hasFill && !hasStroke) return;

    auto* shape = tvg::Shape::gen();
    const float s = m_scale;

    if (s == 1.0f) {
        shape->appendPath(
            reinterpret_cast<const tvg::PathCommand*>(path.verbs.data()),
            static_cast<uint32_t>(path.verbs.size()),
            reinterpret_cast<const tvg::Point*>(path.points.data()),
            static_cast<uint32_t>(path.points.size())
        );
    } else {
        size_t ptIdx = 0;
        const size_t numPts = path.points.size();
        for (PathVerb verb : path.verbs) {
            switch (verb) {
                case PathVerb::MoveTo:
                    if (ptIdx < numPts) {
                        const Vec2& p = path.points[ptIdx++];
                        shape->moveTo(p.x * s, p.y * s);
                    }
                    break;
                case PathVerb::LineTo:
                    if (ptIdx < numPts) {
                        const Vec2& p = path.points[ptIdx++];
                        shape->lineTo(p.x * s, p.y * s);
                    }
                    break;
                case PathVerb::CubicTo:
                    if (ptIdx + 2 < numPts) {
                        const Vec2& c1 = path.points[ptIdx];
                        const Vec2& c2 = path.points[ptIdx + 1];
                        const Vec2& p  = path.points[ptIdx + 2];
                        ptIdx += 3;
                        shape->cubicTo(c1.x * s, c1.y * s, c2.x * s, c2.y * s, p.x * s, p.y * s);
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

ThorCanvas::ImageCacheEntry& ThorCanvas::ResolveImageEntry(std::string_view source) {
    static ImageCacheEntry s_emptyEntry{};
    if (source.empty()) return s_emptyEntry;

    std::string key(source);
    auto it = m_imageCache.find(key);
    if (it != m_imageCache.end()) {
        return it->second;
    }

    ImageCacheEntry entry{};
    std::string_view trimmed = source;
    while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t' || trimmed.front() == '\n' || trimmed.front() == '\r')) {
        trimmed.remove_prefix(1);
    }

    auto* probe = tvg::Picture::gen();
    if (trimmed.starts_with("<svg") || trimmed.starts_with("<?xml")) {
        entry.isInlineSvg = true;
        entry.resolvedDataOrPath = std::string(trimmed);
        if (probe->load(entry.resolvedDataOrPath.data(),
                        static_cast<uint32_t>(entry.resolvedDataOrPath.size()),
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
    } else if (auto asset = Assets::Find(key); asset && !asset->Empty()) {
        const std::string mimeStr(asset->mimeType);
        const char* mime = mimeStr.empty() ? nullptr : mimeStr.c_str();
        if (probe->load(reinterpret_cast<const char*>(asset->data),
                        static_cast<uint32_t>(asset->size),
                        mime,
                        nullptr,
                        false) == tvg::Result::Success) {
            float w = 0.0f, h = 0.0f;
            probe->size(&w, &h);
            if (w > 0.0f && h > 0.0f) {
                entry.embeddedData = asset->data;
                entry.embeddedSize = asset->size;
                entry.embeddedMimeType = mimeStr;
                entry.isEmbedded = true;
                entry.size = {w, h};
                entry.valid = true;
            }
        }
    }

    if (!entry.valid && !entry.isInlineSvg) {
        const char* prefixes[] = {"", "../", "../../", "../../../"};
        for (const char* prefix : prefixes) {
            std::string candidate = std::string(prefix) + key;
            if (probe->load(candidate.c_str()) == tvg::Result::Success) {
                float w = 0.0f, h = 0.0f;
                probe->size(&w, &h);
                if (w > 0.0f && h > 0.0f) {
                    entry.resolvedDataOrPath = std::move(candidate);
                    entry.size = {w, h};
                    entry.valid = true;
                    break;
                }
            }
        }
    }
    delete probe;

    auto [insertedIt, _] = m_imageCache.emplace(std::move(key), std::move(entry));
    return insertedIt->second;
}

Vec2 ThorCanvas::GetImageSize(std::string_view source) {
    const auto& entry = ResolveImageEntry(source);
    return entry.valid ? entry.size : Vec2{0.0f, 0.0f};
}

bool ThorCanvas::DrawImage(
    std::string_view source,
    Rect destRect,
    float cornerRadius,
    uint8_t opacity
) {
    if (source.empty() || opacity == 0 || destRect.w <= 0.0f || destRect.h <= 0.0f || !m_canvas) {
        return false;
    }

    const Rect sBounds(destRect.x * m_scale, destRect.y * m_scale, destRect.w * m_scale, destRect.h * m_scale);
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

    const float sRad = std::max(0.0f, cornerRadius * m_scale);
    const int dstX0 = static_cast<int>(std::round(sBounds.x));
    const int dstY0 = static_cast<int>(std::round(sBounds.y));
    const int dstX1 = static_cast<int>(std::round(sBounds.Right()));
    const int dstY1 = static_cast<int>(std::round(sBounds.Bottom()));
    const uint32_t dstW = static_cast<uint32_t>(std::max(1, dstX1 - dstX0));
    const uint32_t dstH = static_cast<uint32_t>(std::max(1, dstY1 - dstY0));

    if (sRad <= 0.0f && m_targetBuffer && dstW <= 256u && dstH <= 256u) {
        if (entry.cachedRasterW != dstW || entry.cachedRasterH != dstH || entry.cachedPixels.empty()) {
            if (!m_stampCanvas) {
                m_stampCanvas = tvg::SwCanvas::gen(tvg::EngineOption::None);
            }
            entry.cachedPixels.assign(static_cast<size_t>(dstW) * static_cast<size_t>(dstH), 0u);
            m_stampCanvas->target(entry.cachedPixels.data(), dstW, dstW, dstH, tvg::ColorSpace::ABGR8888);

            auto* pic = tvg::Picture::gen();
            tvg::Result res = tvg::Result::InvalidArguments;
            if (entry.isInlineSvg) {
                res = pic->load(entry.resolvedDataOrPath.data(),
                                static_cast<uint32_t>(entry.resolvedDataOrPath.size()),
                                "svg",
                                nullptr,
                                false);
            } else if (entry.isEmbedded) {
                const char* mime = entry.embeddedMimeType.empty() ? nullptr : entry.embeddedMimeType.c_str();
                res = pic->load(reinterpret_cast<const char*>(entry.embeddedData),
                                static_cast<uint32_t>(entry.embeddedSize),
                                mime,
                                nullptr,
                                false);
            } else {
                res = pic->load(entry.resolvedDataOrPath.c_str());
            }
            if (res == tvg::Result::Success) {
                pic->size(static_cast<float>(dstW), static_cast<float>(dstH));
                m_stampCanvas->add(pic);
                m_stampCanvas->draw(false);
                m_stampCanvas->sync();
                m_stampCanvas->remove();
                m_stampCanvas->sync();
                entry.cachedRasterW = dstW;
                entry.cachedRasterH = dstH;
            } else {
                delete pic;
                return false;
            }
        }
        return DrawPixels(entry.cachedPixels.data(), dstW, dstH, destRect, 0.0f, opacity);
    }

    auto* pic = tvg::Picture::gen();
    tvg::Result res = tvg::Result::InvalidArguments;
    if (entry.isInlineSvg) {
        res = pic->load(entry.resolvedDataOrPath.data(),
                        static_cast<uint32_t>(entry.resolvedDataOrPath.size()),
                        "svg",
                        nullptr,
                        false);
    } else if (entry.isEmbedded) {
        const char* mime = entry.embeddedMimeType.empty() ? nullptr : entry.embeddedMimeType.c_str();
        res = pic->load(reinterpret_cast<const char*>(entry.embeddedData),
                        static_cast<uint32_t>(entry.embeddedSize),
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
    const uint32_t* pixels,
    uint32_t width,
    uint32_t height,
    Rect destRect,
    float cornerRadius,
    uint8_t opacity
) {
    if (!pixels || width == 0 || height == 0 || opacity == 0 || destRect.w <= 0.0f || destRect.h <= 0.0f || !m_canvas) {
        return false;
    }

    const float sRad = std::max(0.0f, cornerRadius * m_scale);
    Rect sBounds(destRect.x * m_scale, destRect.y * m_scale, destRect.w * m_scale, destRect.h * m_scale);
    if (sRad <= 0.0f) {
        const int dstX0 = static_cast<int>(std::round(sBounds.x));
        const int dstY0 = static_cast<int>(std::round(sBounds.y));
        const int dstX1 = static_cast<int>(std::round(sBounds.Right()));
        const int dstY1 = static_cast<int>(std::round(sBounds.Bottom()));
        const int dstW = dstX1 - dstX0;
        const int dstH = dstY1 - dstY0;
        if (dstW <= 0 || dstH <= 0) {
            return true;
        }

        if (m_targetBuffer && m_width > 0 && m_height > 0) {
            int clipX0 = std::max(0, dstX0);
            int clipY0 = std::max(0, dstY0);
            int clipX1 = std::min(static_cast<int>(m_width), dstX1);
            int clipY1 = std::min(static_cast<int>(m_height), dstY1);

            if (!m_clipStack.empty()) {
                const Rect& cr = m_clipStack.back();
                clipX0 = std::max(clipX0, static_cast<int>(std::floor(cr.x)));
                clipY0 = std::max(clipY0, static_cast<int>(std::floor(cr.y)));
                clipX1 = std::min(clipX1, static_cast<int>(std::ceil(cr.Right())));
                clipY1 = std::min(clipY1, static_cast<int>(std::ceil(cr.Bottom())));
            }
            if (clipX0 >= clipX1 || clipY0 >= clipY1) {
                return true;
            }

            FlushCanvas();

            const int spanW = clipX1 - clipX0;
            if (dstW == static_cast<int>(width) && dstH == static_cast<int>(height)) {
                const int srcOffX = clipX0 - dstX0;
                for (int y = clipY0; y < clipY1; ++y) {
                    const uint32_t* srcRow = pixels + static_cast<size_t>(y - dstY0) * width + srcOffX;
                    uint32_t* dstRow = m_targetBuffer + static_cast<size_t>(y) * m_stride + clipX0;
                    if (opacity == 255) {
                        for (int x = 0; x < spanW; ++x) {
                            const uint32_t s = srcRow[x];
                            if (s >= 0xFF000000u) {
                                dstRow[x] = s;
                            } else if (s != 0u) {
                                dstRow[x] = BlendPreNormalSwar(s, dstRow[x]);
                            }
                        }
                    } else {
                        for (int x = 0; x < spanW; ++x) {
                            const uint32_t s = AlphaBlendSwar(srcRow[x], opacity);
                            if (s != 0u) {
                                dstRow[x] = BlendPreNormalSwar(s, dstRow[x]);
                            }
                        }
                    }
                }
            } else {
                const uint32_t stepX = (width << 16) / static_cast<uint32_t>(dstW);
                const uint32_t stepY = (height << 16) / static_cast<uint32_t>(dstH);
                const uint32_t startU = static_cast<uint32_t>(clipX0 - dstX0) * stepX + (stepX >> 1);
                uint32_t curV = static_cast<uint32_t>(clipY0 - dstY0) * stepY + (stepY >> 1);
                const uint32_t maxU = (width - 1u) << 16;

                for (int y = clipY0; y < clipY1; ++y, curV += stepY) {
                    const uint32_t sy = std::min(height - 1u, curV >> 16);
                    const uint32_t* srcRow = pixels + static_cast<size_t>(sy) * width;
                    uint32_t* dstRow = m_targetBuffer + static_cast<size_t>(y) * m_stride + clipX0;
                    uint32_t curU = startU;
                    if (opacity == 255) {
                        for (int x = 0; x < spanW; ++x, curU += stepX) {
                            const uint32_t s = srcRow[std::min(curU, maxU) >> 16];
                            if (s >= 0xFF000000u) {
                                dstRow[x] = s;
                            } else if (s != 0u) {
                                dstRow[x] = BlendPreNormalSwar(s, dstRow[x]);
                            }
                        }
                    } else {
                        for (int x = 0; x < spanW; ++x, curU += stepX) {
                            const uint32_t s = AlphaBlendSwar(srcRow[std::min(curU, maxU) >> 16], opacity);
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
    if (pic->load(pixels, width, height, tvg::ColorSpace::ABGR8888S, false) != tvg::Result::Success) {
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

    Rect sRect(rect.x * m_scale, rect.y * m_scale, rect.w * m_scale, rect.h * m_scale);
    if (!m_clipStack.empty()) {
        sRect = sRect.Intersect(m_clipStack.back());
    }
    if (sRect.w <= 0.0f || sRect.h <= 0.0f) return;

    const int startX = std::max(0, static_cast<int>(std::floor(sRect.x)));
    const int startY = std::max(0, static_cast<int>(std::floor(sRect.y)));
    const int endX = std::min(static_cast<int>(m_width), static_cast<int>(std::ceil(sRect.Right())));
    const int endY = std::min(static_cast<int>(m_height), static_cast<int>(std::ceil(sRect.Bottom())));
    const int w = endX - startX;
    if (w > 0 && endY > startY) {
        for (int y = startY; y < endY; ++y) {
            std::fill_n(m_targetBuffer + y * m_stride + startX, w, 0x00000000u);
        }
    }
}

bool ThorCanvas::DrawSonarGpuField(const SonarGpuFieldDesc& desc, bool punchHole) {
    if (!m_gpuShadersEnabled || !desc.polarData || desc.numBeams == 0 || desc.numRangeBins == 0 ||
        desc.plotRect.w <= 0.0f || desc.plotRect.h <= 0.0f) {
        return false;
    }

    if (punchHole) {
        PunchHole(desc.plotRect);
    }

    SonarGpuFieldDesc scaled = desc;
    scaled.plotRect = Rect(desc.plotRect.x * m_scale, desc.plotRect.y * m_scale,
                           desc.plotRect.w * m_scale, desc.plotRect.h * m_scale);
    scaled.clipRect = m_gpuClipStack.empty() ? scaled.plotRect : scaled.plotRect.Intersect(m_gpuClipStack.back());
    scaled.origin = Vec2(desc.origin.x * m_scale, desc.origin.y * m_scale);
    scaled.maxRadiusPx = desc.maxRadiusPx * m_scale;
    scaled.bscanInnerRect = Rect(desc.bscanInnerRect.x * m_scale, desc.bscanInnerRect.y * m_scale,
                                 desc.bscanInnerRect.w * m_scale, desc.bscanInnerRect.h * m_scale);

    for (auto& existing : m_sonarGpuPasses) {
        if (existing.widgetId == scaled.widgetId) {
            existing = scaled;
            return true;
        }
    }
    m_sonarGpuPasses.push_back(scaled);
    return true;
}

bool ThorCanvas::DrawSpectrogramGpuField(const SpectrogramGpuFieldDesc& desc, bool punchHole) {
    if (!m_gpuShadersEnabled || !desc.dbGrid || desc.texWidth == 0 || desc.texHeight == 0 ||
        desc.plotRect.w <= 0.0f || desc.plotRect.h <= 0.0f) {
        return false;
    }

    if (punchHole) {
        PunchHole(desc.plotRect);
    }

    SpectrogramGpuFieldDesc scaled = desc;
    scaled.plotRect = Rect(desc.plotRect.x * m_scale, desc.plotRect.y * m_scale,
                           desc.plotRect.w * m_scale, desc.plotRect.h * m_scale);
    scaled.clipRect = m_gpuClipStack.empty() ? scaled.plotRect : scaled.plotRect.Intersect(m_gpuClipStack.back());

    for (auto& existing : m_spectrogramGpuPasses) {
        if (existing.widgetId == scaled.widgetId) {
            existing = scaled;
            return true;
        }
    }
    m_spectrogramGpuPasses.push_back(scaled);
    return true;
}

bool ThorCanvas::DrawWaterfallGpuField(const WaterfallGpuFieldDesc& desc, bool punchHole) {
    if (!m_gpuShadersEnabled || !desc.ringBufferDb || desc.texWidth == 0 || desc.texHeight == 0 ||
        desc.plotRect.w <= 0.0f || desc.plotRect.h <= 0.0f) {
        return false;
    }

    if (punchHole) {
        PunchHole(desc.plotRect);
    }

    WaterfallGpuFieldDesc scaled = desc;
    scaled.plotRect = Rect(desc.plotRect.x * m_scale, desc.plotRect.y * m_scale,
                           desc.plotRect.w * m_scale, desc.plotRect.h * m_scale);
    scaled.clipRect = m_gpuClipStack.empty() ? scaled.plotRect : scaled.plotRect.Intersect(m_gpuClipStack.back());

    for (auto& existing : m_waterfallGpuPasses) {
        if (existing.widgetId == scaled.widgetId) {
            existing = scaled;
            return true;
        }
    }
    m_waterfallGpuPasses.push_back(scaled);
    return true;
}

bool ThorCanvas::CaptureRegion(Rect rect, std::vector<uint32_t>& outPixels, uint32_t& outW, uint32_t& outH) {
    if (!m_targetBuffer || m_width == 0 || m_height == 0 || rect.w <= 0.0f || rect.h <= 0.0f) {
        return false;
    }

    const Rect sRect(rect.x * m_scale, rect.y * m_scale, rect.w * m_scale, rect.h * m_scale);
    const int x0 = std::clamp(static_cast<int>(std::round(sRect.x)), 0, static_cast<int>(m_width));
    const int y0 = std::clamp(static_cast<int>(std::round(sRect.y)), 0, static_cast<int>(m_height));
    const int x1 = std::clamp(static_cast<int>(std::round(sRect.Right())), 0, static_cast<int>(m_width));
    const int y1 = std::clamp(static_cast<int>(std::round(sRect.Bottom())), 0, static_cast<int>(m_height));
    const int w = x1 - x0;
    const int h = y1 - y0;
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

    outW = static_cast<uint32_t>(w);
    outH = static_cast<uint32_t>(h);
    outPixels.resize(static_cast<size_t>(outW) * static_cast<size_t>(outH));
    for (int row = 0; row < h; ++row) {
        std::memcpy(
            outPixels.data() + static_cast<size_t>(row) * outW,
            m_targetBuffer + static_cast<size_t>(y0 + row) * m_stride + x0,
            static_cast<size_t>(w) * sizeof(uint32_t)
        );
    }
    return true;
}

bool ThorCanvas::RestoreRegion(Rect rect, const std::vector<uint32_t>& inPixels, uint32_t inW, uint32_t inH) {
    if (!m_targetBuffer || m_width == 0 || m_height == 0 || inW == 0 || inH == 0 ||
        inPixels.size() < static_cast<size_t>(inW) * static_cast<size_t>(inH)) {
        return false;
    }

    const Rect sRect(rect.x * m_scale, rect.y * m_scale, rect.w * m_scale, rect.h * m_scale);
    const int x0 = std::clamp(static_cast<int>(std::round(sRect.x)), 0, static_cast<int>(m_width));
    const int y0 = std::clamp(static_cast<int>(std::round(sRect.y)), 0, static_cast<int>(m_height));
    const int x1 = std::clamp(static_cast<int>(std::round(sRect.Right())), 0, static_cast<int>(m_width));
    const int y1 = std::clamp(static_cast<int>(std::round(sRect.Bottom())), 0, static_cast<int>(m_height));
    const int w = x1 - x0;
    const int h = y1 - y0;
    if (w != static_cast<int>(inW) || h != static_cast<int>(inH)) {
        return false;
    }

    int clipX0 = x0;
    int clipY0 = y0;
    int clipX1 = x1;
    int clipY1 = y1;
    if (!m_clipStack.empty()) {
        const Rect& clip = m_clipStack.back();
        clipX0 = std::max(clipX0, static_cast<int>(std::floor(clip.x)));
        clipY0 = std::max(clipY0, static_cast<int>(std::floor(clip.y)));
        clipX1 = std::min(clipX1, static_cast<int>(std::ceil(clip.Right())));
        clipY1 = std::min(clipY1, static_cast<int>(std::ceil(clip.Bottom())));
    }
    if (clipX1 <= clipX0 || clipY1 <= clipY0) {
        return true;
    }

    FlushCanvas();

    const int copyW = clipX1 - clipX0;
    const int srcOffX = clipX0 - x0;
    for (int y = clipY0; y < clipY1; ++y) {
        const int srcRow = y - y0;
        std::memcpy(
            m_targetBuffer + static_cast<size_t>(y) * m_stride + clipX0,
            inPixels.data() + static_cast<size_t>(srcRow) * inW + srcOffX,
            static_cast<size_t>(copyW) * sizeof(uint32_t)
        );
    }
    return true;
}

std::unique_ptr<ICanvas> CreateVectorCanvas(VectorBackendType /*type*/, bool loadDefaultFonts) {
    return std::make_unique<ThorCanvas>(loadDefaultFonts);
}

std::unique_ptr<ICanvas> CreateOffscreenVectorCanvas(VectorBackendType type) {
    return CreateVectorCanvas(type, /*loadDefaultFonts=*/false);
}

} // namespace guikit
