#include "guikit/backend/ThorCanvas.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <thread>
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

} // namespace

ThorCanvas::ThorCanvas() {
    // Use 0 worker threads (synchronous main-thread rasterization) to eliminate
    // multi-core mutex contention and condition_variable wakeups on UI primitives.
    tvg::Initializer::init(0);
    m_canvas = tvg::SwCanvas::gen(tvg::EngineOption::None);

    // Auto-detect default fonts across common asset and system locations
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
    if (m_canvas) {
        m_canvas->sync();
        delete m_canvas;
        m_canvas = nullptr;
    }
    tvg::Initializer::term();
}

void ThorCanvas::SetTarget(uint32_t* buffer, uint32_t width, uint32_t height, uint32_t stride) {
    m_targetBuffer = buffer;
    m_width = width;
    m_height = height;
    m_stride = stride;

    if (m_canvas && m_targetBuffer && m_width > 0 && m_height > 0) {
        m_canvas->target(m_targetBuffer, m_stride, m_width, m_height, tvg::ColorSpace::ABGR8888);
    }
}

bool ThorCanvas::LoadFontFile(const std::string& path) {
    if (path.empty()) return false;
    tvg::Result res = tvg::Text::load(path.c_str());
    if (res == tvg::Result::Success) {
        m_defaultFontPath = path;
        m_defaultFontName = ExtractFontName(path);
        m_fontLoaded = true;
        m_textMeasureCache.clear();
        return true;
    }
    return false;
}

bool ThorCanvas::LoadMonospaceFontFile(const std::string& path) {
    if (path.empty()) return false;
    tvg::Result res = tvg::Text::load(path.c_str());
    if (res == tvg::Result::Success) {
        m_monoFontPath = path;
        m_monoFontName = ExtractFontName(path);
        m_monoFontLoaded = true;
        m_textMeasureCache.clear();
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
    if (m_width != width || m_height != height) {
        m_width = width;
        m_height = height;
        if (m_targetBuffer && m_canvas) {
            m_canvas->target(m_targetBuffer, m_stride, m_width, m_height, tvg::ColorSpace::ABGR8888);
        }
    }
}

void ThorCanvas::End() {
    if (m_canvas) {
        m_canvas->draw(false);
        m_canvas->sync();
        m_canvas->remove();
    }
}

void ThorCanvas::Clear(Color color, std::optional<Rect> rect) {
    if (!m_targetBuffer || m_width == 0 || m_height == 0) return;

    const uint32_t rawColor = color.ToUint32();

    if (!rect.has_value()) {
        m_sonarGpuPasses.clear();
        m_spectrogramGpuPasses.clear();
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
}

void ThorCanvas::PopClip() {
    if (!m_clipStack.empty()) {
        m_clipStack.pop_back();
    }
    if (!m_gpuClipStack.empty()) {
        m_gpuClipStack.pop_back();
    }
}

void ThorCanvas::ApplyClip(tvg::Paint* paint, std::optional<Rect> bounds) {
    if (!paint || m_clipStack.empty()) return;
    const Rect& cr = m_clipStack.back();
    // If the active clip rect covers the entire canvas (or larger), no clipping shape is needed!
    if (cr.x <= 0.0f && cr.y <= 0.0f &&
        cr.w >= static_cast<float>(m_width) &&
        cr.h >= static_cast<float>(m_height)) {
        return;
    }
    // If the shape's outer bounding box is already 100% inside the active clip rect, skip RLE clip mask!
    if (bounds.has_value() && cr.w > 0.0f && cr.h > 0.0f) {
        if (bounds->x >= cr.x && bounds->y >= cr.y &&
            bounds->Right() <= cr.Right() && bounds->Bottom() <= cr.Bottom()) {
            return;
        }
    }
    auto clipShape = tvg::Shape::gen();
    if (cr.w <= 0.0f || cr.h <= 0.0f) {
        clipShape->appendRect(0.0f, 0.0f, 0.0f, 0.0f);
    } else {
        clipShape->appendRect(cr.x, cr.y, cr.w, cr.h);
    }
    paint->clip(clipShape);
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

    auto shape = tvg::Shape::gen();
    shape->appendRect(sRect.x, sRect.y, sRect.w, sRect.h);
    shape->fill(fill.r, fill.g, fill.b, fill.a);
    m_canvas->add(shape);
}

void ThorCanvas::DrawRoundRect(
    Rect rect,
    float radius,
    Color fill,
    Color stroke,
    float strokeWidth
) {
    if (rect.w <= 0.0f || rect.h <= 0.0f || !m_canvas) return;
    const Rect sRect(rect.x * m_scale, rect.y * m_scale, rect.w * m_scale, rect.h * m_scale);
    const float pad = (stroke.a > 0 && strokeWidth > 0.0f) ? (strokeWidth * m_scale * 0.5f + 1.0f) : 1.0f;
    const Rect outerBounds(sRect.x - pad, sRect.y - pad, sRect.w + pad * 2.0f, sRect.h + pad * 2.0f);
    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        if (cr.w <= 0.0f || cr.h <= 0.0f || !outerBounds.Intersects(cr)) return;
    }

    const float sRadius = radius * m_scale;
    auto shape = tvg::Shape::gen();
    shape->appendRect(sRect.x, sRect.y, sRect.w, sRect.h, sRadius, sRadius);

    if (fill.a > 0) {
        shape->fill(fill.r, fill.g, fill.b, fill.a);
    }

    if (stroke.a > 0 && strokeWidth > 0.0f) {
        shape->strokeWidth(strokeWidth * m_scale);
        shape->strokeFill(stroke.r, stroke.g, stroke.b, stroke.a);
    }

    ApplyClip(shape, outerBounds);
    m_canvas->add(shape);
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

    const bool isItalic = (style == FontStyle::Italic || style == FontStyle::BoldItalic);
    const bool isBold = (style == FontStyle::Bold || style == FontStyle::BoldItalic);
    const bool hasRot = std::abs(angleDeg) > 0.01f;
    const float rad = hasRot ? (angleDeg * 0.017453292519943295f) : 0.0f;
    const float cosA = hasRot ? std::cos(rad) : 1.0f;
    const float sinA = hasRot ? std::sin(rad) : 0.0f;

    std::optional<Rect> approxBounds{};
    if (!hasRot) {
        const Vec2 m = MeasureTextStyled(text, fontSize, style);
        approxBounds = Rect(
            (pos.x - 2.0f) * m_scale,
            (pos.y - fontSize * 0.25f) * m_scale,
            (m.x + 6.0f) * m_scale,
            (m.y + fontSize * 0.5f) * m_scale
        );
    }

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

        ApplyClip(txt, approxBounds);
        m_canvas->add(txt);
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

    ApplyClip(txt, Rect(sBounds.x - 2.0f, sBounds.y - 2.0f, sBounds.w + 4.0f, sBounds.h + 4.0f));
    m_canvas->add(txt);
}

void ThorCanvas::DrawLine(Vec2 p1, Vec2 p2, Color color, float strokeWidth) {
    if (color.a == 0 || strokeWidth <= 0.0f || !m_canvas) return;
    const Vec2 sp1(p1.x * m_scale, p1.y * m_scale);
    const Vec2 sp2(p2.x * m_scale, p2.y * m_scale);
    const float sWidth = strokeWidth * m_scale;
    const float pad = sWidth * 0.5f + 1.0f;
    const Rect lineBounds(
        std::min(sp1.x, sp2.x) - pad,
        std::min(sp1.y, sp2.y) - pad,
        std::abs(sp2.x - sp1.x) + pad * 2.0f,
        std::abs(sp2.y - sp1.y) + pad * 2.0f
    );

    if (!m_clipStack.empty()) {
        const Rect& cr = m_clipStack.back();
        if (cr.w <= 0.0f || cr.h <= 0.0f || !lineBounds.Intersects(cr)) return;
    }

    auto shape = tvg::Shape::gen();
    shape->moveTo(sp1.x, sp1.y);
    shape->lineTo(sp2.x, sp2.y);
    shape->strokeWidth(sWidth);
    shape->strokeCap(tvg::StrokeCap::Round);
    shape->strokeFill(color.r, color.g, color.b, color.a);

    ApplyClip(shape, lineBounds);
    m_canvas->add(shape);
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

    ApplyClip(shape, triBounds);
    m_canvas->add(shape);
}

const ThorCanvas::ImageCacheEntry& ThorCanvas::ResolveImageEntry(std::string_view source) {
    static const ImageCacheEntry kEmptyEntry{};
    if (source.empty()) return kEmptyEntry;

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
    } else {
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

    const auto& entry = ResolveImageEntry(source);
    if (!entry.valid || entry.size.x <= 0.0f || entry.size.y <= 0.0f) {
        return false;
    }

    auto* pic = tvg::Picture::gen();
    tvg::Result res = tvg::Result::InvalidArguments;
    if (entry.isInlineSvg) {
        res = pic->load(entry.resolvedDataOrPath.data(),
                        static_cast<uint32_t>(entry.resolvedDataOrPath.size()),
                        "svg",
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

    const float sRad = std::max(0.0f, cornerRadius * m_scale);
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
    } else {
        ApplyClip(pic, sBounds);
    }

    m_canvas->add(pic);
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

    const Rect sBounds(destRect.x * m_scale, destRect.y * m_scale, destRect.w * m_scale, destRect.h * m_scale);
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

    const float sRad = std::max(0.0f, cornerRadius * m_scale);
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
    } else {
        ApplyClip(pic, sBounds);
    }

    m_canvas->add(pic);
    return true;
}

void ThorCanvas::PunchHole(Rect rect) {
    if (!m_targetBuffer || m_width == 0 || m_height == 0) return;

    // Flush all background shapes queued prior to this GPU underlay window
    if (m_canvas) {
        m_canvas->draw(false);
        m_canvas->sync();
        m_canvas->remove();
    }

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

    if (m_canvas) {
        m_canvas->draw(false);
        m_canvas->sync();
        m_canvas->remove();
    }

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

    if (m_canvas) {
        m_canvas->draw(false);
        m_canvas->sync();
        m_canvas->remove();
    }

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

} // namespace guikit

