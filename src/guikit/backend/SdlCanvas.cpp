#include "guikit/backend/SdlCanvas.hpp"
#include "guikit/core/TextUtil.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>
#include "guikit/core/Tokens.hpp"

namespace guikit {

namespace {

[[nodiscard]] constexpr SDL_FColor ToSdlFColor(Color c) noexcept {
    return SDL_FColor{
        c.r / 255.0f,
        c.g / 255.0f,
        c.b / 255.0f,
        c.a / 255.0f
    };
}

} // namespace

SdlCanvas::SdlCanvas(SDL_Renderer* renderer) noexcept
    : m_renderer(renderer)
{
}

void SdlCanvas::Begin(uint32_t width, uint32_t height, float scale) {
    m_width = width;
    m_height = height;
    m_scale = (scale > 0.0f) ? scale : 1.0f;
    m_clipStack.clear();

    if (m_renderer) {
        SDL_SetRenderScale(m_renderer, m_scale, m_scale);
        SDL_SetRenderClipRect(m_renderer, nullptr);
    }
}

void SdlCanvas::End() {
    if (m_renderer) {
        SDL_SetRenderClipRect(m_renderer, nullptr);
    }
}

void SdlCanvas::SetScale(float scale) {
    m_scale = (scale > 0.0f) ? scale : 1.0f;
    if (m_renderer) {
        SDL_SetRenderScale(m_renderer, m_scale, m_scale);
    }
}

void SdlCanvas::Clear(Color color, std::optional<Rect> rect) {
    if (!m_renderer) return;

    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    if (!rect.has_value()) {
        SDL_RenderClear(m_renderer);
    } else {
        const SDL_FRect r{rect->x, rect->y, rect->w, rect->h};
        SDL_RenderFillRect(m_renderer, &r);
    }
}

void SdlCanvas::PushClip(Rect clipRect) {
    if (!m_clipStack.empty()) {
        const Rect& parent = m_clipStack.back();
        const float x1 = std::max(parent.x, clipRect.x);
        const float y1 = std::max(parent.y, clipRect.y);
        const float x2 = std::min(parent.Right(), clipRect.Right());
        const float y2 = std::min(parent.Bottom(), clipRect.Bottom());
        m_clipStack.emplace_back(x1, y1, std::max(0.0f, x2 - x1), std::max(0.0f, y2 - y1));
    } else {
        m_clipStack.push_back(clipRect);
    }
    ApplyActiveClip();
}

void SdlCanvas::PopClip() {
    if (!m_clipStack.empty()) {
        m_clipStack.pop_back();
        ApplyActiveClip();
    }
}

void SdlCanvas::ApplyActiveClip() {
    if (!m_renderer) return;

    if (m_clipStack.empty()) {
        SDL_SetRenderClipRect(m_renderer, nullptr);
    } else {
        const Rect& cr = m_clipStack.back();
        const SDL_Rect sdlR{
            static_cast<int>(std::floor(cr.x)),
            static_cast<int>(std::floor(cr.y)),
            static_cast<int>(std::ceil(cr.w)),
            static_cast<int>(std::ceil(cr.h))
        };
        SDL_SetRenderClipRect(m_renderer, &sdlR);
    }
}

void SdlCanvas::DrawRect(Rect rect, Color fill) {
    if (!m_renderer || fill.a == 0 || rect.w <= 0.0f || rect.h <= 0.0f) return;

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, fill.r, fill.g, fill.b, fill.a);
    const SDL_FRect r{rect.x, rect.y, rect.w, rect.h};
    SDL_RenderFillRect(m_renderer, &r);
}

void SdlCanvas::DrawRoundRect(
    Rect rect,
    float radius,
    Color fill,
    Color stroke,
    float strokeWidth
) {
    if (!m_renderer || rect.w <= 0.0f || rect.h <= 0.0f) return;

    const float maxRadius = std::min(rect.w, rect.h) * 0.5f;
    const float r = std::clamp(radius, 0.0f, maxRadius);

    if (r <= 1.0f) {
        if (fill.a > 0) {
            DrawRect(rect, fill);
        }
        if (stroke.a > 0 && strokeWidth > 0.0f) {
            SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(m_renderer, stroke.r, stroke.g, stroke.b, stroke.a);
            const SDL_FRect sr{rect.x, rect.y, rect.w, rect.h};
            SDL_RenderRect(m_renderer, &sr);
        }
        return;
    }

    // Tessellate rounded perimeter using 6 segments per corner
    constexpr int kSegmentsPerCorner = 6;
    constexpr int kPerimeterPoints = kSegmentsPerCorner * 4;

    std::vector<SDL_FPoint> perimeter;
    perimeter.reserve(kPerimeterPoints + 1);

    const auto addArc = [&](float cx, float cy, float startAngle, float endAngle) {
        for (int i = 0; i < kSegmentsPerCorner; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kSegmentsPerCorner);
            const float angle = startAngle + t * (endAngle - startAngle);
            perimeter.push_back(SDL_FPoint{
                cx + std::cos(angle) * r,
                cy + std::sin(angle) * r
            });
        }
    };

    // 1. Top-left corner (PI to 3*PI/2)
    addArc(rect.x + r, rect.y + r, std::numbers::pi_v<float>, 1.5f * std::numbers::pi_v<float>);
    // 2. Top-right corner (3*PI/2 to 2*PI)
    addArc(rect.Right() - r, rect.y + r, 1.5f * std::numbers::pi_v<float>, 2.0f * std::numbers::pi_v<float>);
    // 3. Bottom-right corner (0 to PI/2)
    addArc(rect.Right() - r, rect.Bottom() - r, 0.0f, 0.5f * std::numbers::pi_v<float>);
    // 4. Bottom-left corner (PI/2 to PI)
    addArc(rect.x + r, rect.Bottom() - r, 0.5f * std::numbers::pi_v<float>, std::numbers::pi_v<float>);

    if (fill.a > 0) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);

        // Center vertex
        const SDL_FPoint center{rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f};
        const SDL_FColor fcol = ToSdlFColor(fill);

        std::vector<SDL_Vertex> vertices;
        vertices.reserve(kPerimeterPoints + 1);
        vertices.push_back(SDL_Vertex{center, fcol, {0.0f, 0.0f}});

        for (const auto& pt : perimeter) {
            vertices.push_back(SDL_Vertex{pt, fcol, {0.0f, 0.0f}});
        }

        std::vector<int> indices;
        indices.reserve(kPerimeterPoints * 3);
        for (int i = 0; i < kPerimeterPoints; ++i) {
            indices.push_back(0); // center
            indices.push_back(i + 1);
            indices.push_back(((i + 1) % kPerimeterPoints) + 1);
        }

        SDL_RenderGeometry(
            m_renderer,
            nullptr,
            vertices.data(),
            static_cast<int>(vertices.size()),
            indices.data(),
            static_cast<int>(indices.size())
        );
    }

    if (stroke.a > 0 && strokeWidth > 0.0f) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, stroke.r, stroke.g, stroke.b, stroke.a);
        perimeter.push_back(perimeter.front()); // Close loop
        SDL_RenderLines(m_renderer, perimeter.data(), static_cast<int>(perimeter.size()));
    }
}

Vec2 SdlCanvas::MeasureText(std::string_view text, float fontSize) {
    if (fontSize <= 0.0f) {
        return {0.0f, 0.0f};
    }
    const float totalAdvance = text.empty() ? 0.0f : static_cast<float>(text.length()) * (fontSize * kDefaultCharAdvanceRatio);
    return {totalAdvance, GetLineHeight(fontSize)};
}

float SdlCanvas::GetLineHeight(float fontSize) {
    if (fontSize <= 0.0f) return 0.0f;
    return fontSize * kDefaultLineHeightMultiplier;
}

void SdlCanvas::DrawText(std::string_view text, Vec2 pos, float fontSize, Color color) {
    if (!m_renderer || text.empty() || color.a == 0) return;

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
    const std::string textStr(text);
    SDL_RenderDebugText(m_renderer, pos.x, pos.y, textStr.c_str());
}

void SdlCanvas::DrawTextBox(
    std::string_view text,
    Rect bounds,
    float fontSize,
    Color color,
    float alignX,
    float alignY
) {
    if (text.empty() || color.a == 0 || bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    std::string truncated;
    std::string_view drawText = text;
    if (bounds.w > 0.0f && MeasureText(text, fontSize).x > bounds.w + kTextSubpixelTolerance) {
        truncated = TextUtil::TruncateWithEllipsis(text, bounds.w, fontSize, this);
        drawText = truncated;
    }

    const Vec2 dims = MeasureText(drawText, fontSize);
    const float x = bounds.x + std::max(0.0f, (bounds.w - dims.x) * alignX);
    const float y = bounds.y + (bounds.h - dims.y) * alignY;

    PushClip(bounds);
    DrawText(drawText, {x, y}, fontSize, color);
    PopClip();
}

void SdlCanvas::DrawLine(Vec2 p1, Vec2 p2, Color color, float strokeWidth) {
    if (!m_renderer || color.a == 0 || strokeWidth <= 0.0f) return;

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);

    if (strokeWidth <= 1.0f) {
        SDL_RenderLine(m_renderer, p1.x, p1.y, p2.x, p2.y);
    } else {
        const float dx = p2.x - p1.x;
        const float dy = p2.y - p1.y;
        const float len = std::hypot(dx, dy);
        if (len > 0.001f) {
            const float nx = -dy / len * (strokeWidth * 0.5f);
            const float ny = dx / len * (strokeWidth * 0.5f);
            const SDL_FPoint pts[5] = {
                {p1.x + nx, p1.y + ny},
                {p2.x + nx, p2.y + ny},
                {p2.x - nx, p2.y - ny},
                {p1.x - nx, p1.y - ny},
                {p1.x + nx, p1.y + ny}
            };
            SDL_RenderLines(m_renderer, pts, 5);
        } else {
            SDL_RenderPoint(m_renderer, p1.x, p1.y);
        }
    }
}

void SdlCanvas::DrawTexture(
    uint64_t textureId,
    Rect destRect,
    std::optional<Rect> srcRect,
    Color tint
) {
    if (!m_renderer || textureId == 0 || destRect.w <= 0.0f || destRect.h <= 0.0f) return;

    auto* tex = reinterpret_cast<SDL_Texture*>(textureId);
    SDL_SetTextureColorMod(tex, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(tex, tint.a);

    const SDL_FRect dst{destRect.x, destRect.y, destRect.w, destRect.h};
    if (srcRect.has_value()) {
        const SDL_FRect src{srcRect->x, srcRect->y, srcRect->w, srcRect->h};
        SDL_RenderTexture(m_renderer, tex, &src, &dst);
    } else {
        SDL_RenderTexture(m_renderer, tex, nullptr, &dst);
    }
}

} // namespace guikit
