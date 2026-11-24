#include "guikit/backend/null_canvas.hpp"

#include "guikit/style/tokens.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

[[nodiscard]] keel::usize CountUtf8Codepoints(keel::StringRef text) noexcept {
    keel::usize count = 0;
    for (keel::usize i = 0; i < text.size(); ++i) {
        const auto byte = keel::as<keel::u8>(text[i]);
        if ((byte & 0xC0u) != 0x80u) {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] keel::f32 StyleAdvanceRatio(FontStyle style) noexcept {
    switch (style) {
        case FontStyle::Bold:
        case FontStyle::BoldItalic:
            return kDefaultCharAdvanceRatio * 1.04f;
        case FontStyle::Monospace:
            return 0.60f;
        case FontStyle::Italic:
        case FontStyle::Regular:
        default:
            return kDefaultCharAdvanceRatio;
    }
}

} // namespace

void NullCanvas::Begin(keel::u32 width, keel::u32 height, keel::f32 scale) {
    m_width = width;
    m_height = height;
    m_scale = (scale > 0.0f) ? scale : 1.0f;
    m_xfScale = 1.0f;
    m_xfOffset = {0.0f, 0.0f};
    m_transformStack.clear();
    m_clipStack.clear();
    ++m_stats.beginCount;
}

void NullCanvas::End() {
    ++m_stats.endCount;
}

void NullCanvas::SetScale(keel::f32 scale) {
    m_scale = (scale > 0.0f) ? scale : 1.0f;
}

void NullCanvas::PushTransform(const Transform2D& transform) {
    m_transformStack.push_back(TransformState{m_xfScale, m_xfOffset});
    const Transform2D combined = Transform2D{m_xfScale, m_xfOffset}.Then(transform);
    m_xfScale = combined.scale;
    m_xfOffset = combined.offset;
}

void NullCanvas::PopTransform() {
    if (m_transformStack.empty()) {
        return;
    }
    const TransformState prev = m_transformStack.back();
    m_transformStack.pop_back();
    m_xfScale = prev.scale;
    m_xfOffset = prev.offset;
}

void NullCanvas::Clear(Color /*color*/, keel::Optional<Rect> rect) {
    ++m_stats.clearCount;
    if (!rect.has_value()) {
        m_gpuPasses.clear();
        return;
    }
    const Rect sClear = Px(*rect);
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
}

void NullCanvas::PushClip(Rect clipRect) {
    const Rect mapped = Px(clipRect);
    if (!m_clipStack.empty()) {
        m_clipStack.push_back(m_clipStack.back().Intersect(mapped));
    } else {
        m_clipStack.push_back(mapped);
    }
}

void NullCanvas::PopClip() {
    if (!m_clipStack.empty()) {
        m_clipStack.pop_back();
    }
}

void NullCanvas::DrawRect(Rect rect, Color fill) {
    if (fill.a == 0 || rect.w <= 0.0f || rect.h <= 0.0f) return;
    ++m_stats.drawRectCount;
}

void NullCanvas::DrawRoundRect(
    Rect rect,
    keel::f32 /*radius*/,
    Color fill,
    Color stroke,
    keel::f32 strokeWidth
) {
    if (rect.w <= 0.0f || rect.h <= 0.0f) return;
    if (fill.a == 0 && (stroke.a == 0 || strokeWidth <= 0.0f)) return;
    ++m_stats.drawRoundRectCount;
}

void NullCanvas::DrawShadow(
    Rect rect,
    keel::f32 /*cornerRadius*/,
    keel::f32 /*blurRadius*/,
    Vec2 /*offset*/,
    Color color,
    keel::f32 /*spread*/
) {
    if (color.a == 0 || rect.w <= 0.0f || rect.h <= 0.0f) return;
    ++m_stats.drawRoundRectCount;
}

Vec2 NullCanvas::MeasureText(keel::StringRef text, keel::f32 fontSize) {
    return MeasureTextStyled(text, fontSize, FontStyle::Regular);
}

Vec2 NullCanvas::MeasureTextStyled(keel::StringRef text, keel::f32 fontSize, FontStyle style) {
    if (fontSize <= 0.0f) {
        return {0.0f, 0.0f};
    }
    const keel::usize codepoints = CountUtf8Codepoints(text);
    const keel::f32 width = keel::as<keel::f32>(codepoints) * (fontSize * StyleAdvanceRatio(style));
    return {width, GetLineHeight(fontSize)};
}

keel::f32 NullCanvas::GetLineHeight(keel::f32 fontSize) {
    if (fontSize <= 0.0f) return 0.0f;
    return fontSize * kDefaultLineHeightMultiplier;
}

void NullCanvas::DrawText(keel::StringRef text, Vec2 /*pos*/, keel::f32 fontSize, Color color) {
    if (text.empty() || fontSize <= 0.0f || color.a == 0) return;
    ++m_stats.drawTextCount;
}

void NullCanvas::DrawTextStyled(
    keel::StringRef text,
    Vec2 /*pos*/,
    keel::f32 fontSize,
    Color color,
    FontStyle /*style*/,
    keel::f32 /*angleDeg*/
) {
    if (text.empty() || fontSize <= 0.0f || color.a == 0) return;
    ++m_stats.drawTextCount;
}

void NullCanvas::DrawTextBox(
    keel::StringRef text,
    Rect bounds,
    keel::f32 fontSize,
    Color color,
    keel::f32 /*alignX*/,
    keel::f32 /*alignY*/
) {
    if (text.empty() || fontSize <= 0.0f || color.a == 0 || bounds.w <= 0.0f || bounds.h <= 0.0f) return;
    ++m_stats.drawTextCount;
}

void NullCanvas::DrawTextBoxStyled(
    keel::StringRef text,
    Rect bounds,
    keel::f32 fontSize,
    Color color,
    FontStyle /*style*/,
    keel::f32 /*alignX*/,
    keel::f32 /*alignY*/
) {
    if (text.empty() || fontSize <= 0.0f || color.a == 0 || bounds.w <= 0.0f || bounds.h <= 0.0f) return;
    ++m_stats.drawTextCount;
}

void NullCanvas::DrawLine(Vec2 /*p1*/, Vec2 /*p2*/, Color color, keel::f32 strokeWidth) {
    if (color.a == 0 || strokeWidth <= 0.0f) return;
    ++m_stats.drawLineCount;
}

void NullCanvas::DrawArc(
    Vec2 /*center*/,
    keel::f32 radius,
    keel::f32 startAngleRad,
    keel::f32 endAngleRad,
    Color color,
    keel::f32 strokeWidth,
    bool /*roundCaps*/
) {
    if (color.a == 0 || radius <= 0.25f || strokeWidth <= 0.0f ||
        keel::abs(endAngleRad - startAngleRad) < 1e-4f) {
        return;
    }
    ++m_stats.drawPathCount;
}

void NullCanvas::DrawTriangle(
    Vec2 /*p1*/,
    Vec2 /*p2*/,
    Vec2 /*p3*/,
    Color fill,
    Color stroke,
    keel::f32 strokeWidth
) {
    if (fill.a == 0 && (stroke.a == 0 || strokeWidth <= 0.0f)) return;
    ++m_stats.drawTriangleCount;
}

void NullCanvas::DrawPath(const VectorPath& path, const PathPaint& paint) {
    if (path.Empty()) return;
    if ((!paint.renderFill || paint.fillColor.a == 0) &&
        (!paint.renderStroke || paint.strokeColor.a == 0 || paint.strokeWidthPx <= 0.0f)) {
        return;
    }
    ++m_stats.drawPathCount;
}

bool NullCanvas::DrawImage(
    keel::StringRef source,
    Rect destRect,
    keel::f32 /*cornerRadius*/,
    keel::u8 opacity
) {
    if (source.empty() || opacity == 0 || destRect.w <= 0.0f || destRect.h <= 0.0f) return false;
    ++m_stats.drawImageCount;
    return true;
}

bool NullCanvas::DrawPixels(
    PixelRef pixels,
    Rect destRect,
    keel::f32 /*cornerRadius*/,
    keel::u8 opacity
) {
    if (pixels.Empty() || opacity == 0 || destRect.w <= 0.0f || destRect.h <= 0.0f) return false;
    ++m_stats.drawPixelsCount;
    return true;
}

void NullCanvas::DrawTexture(
    keel::u64 textureId,
    Rect destRect,
    keel::Optional<Rect> /*srcRect*/,
    Color tint
) {
    if (textureId == 0 || tint.a == 0 || destRect.w <= 0.0f || destRect.h <= 0.0f) return;
    ++m_stats.drawTextureCount;
}

bool NullCanvas::DrawGpuShaderPass(const GpuShaderPassDesc& pass, bool /*punchHole*/) {
    if (!m_gpuShadersEnabled || pass.shader.shaderId.empty() || pass.shader.glslEs2.empty() ||
        pass.dataField.empty() || pass.plotRect.w <= 0.0f || pass.plotRect.h <= 0.0f) {
        return false;
    }
    ++m_stats.drawGpuShaderPassCount;

    GpuShaderPassDesc scaled = pass;
    scaled.plotRect = Px(pass.plotRect);
    scaled.clipRect = m_clipStack.empty() ? scaled.plotRect : scaled.plotRect.Intersect(m_clipStack.back());
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

} // namespace guikit
