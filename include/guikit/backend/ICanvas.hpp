#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include "guikit/core/Types.hpp"

namespace guikit {

/// Typographic style variants for rich text and Markdown rendering.
enum class FontStyle : uint8_t {
    Regular = 0,
    Bold,
    Italic,
    BoldItalic,
    Monospace
};

/// Abstract interface for vector graphics rendering backends.
class ICanvas {
public:
    virtual ~ICanvas() = default;

    /// Prepares the canvas for a new frame with dimensions and optional DPI / UI scale factor.
    virtual void Begin(uint32_t width, uint32_t height, float scale = 1.0f) = 0;

    /// Finalizes and commits all recorded rendering commands.
    virtual void End() = 0;

    /// Returns the current canvas scale factor.
    [[nodiscard]] virtual float GetScale() const noexcept = 0;

    /// Sets or overrides the current canvas scale factor.
    virtual void SetScale(float scale) = 0;

    /// Human-readable backend identifier name (e.g. "ThorVG (Vector Software)", "SDL3 (Direct GPU)").
    [[nodiscard]] virtual std::string_view GetBackendName() const noexcept = 0;

    /// Clears the entire canvas surface or a targeted sub-rectangle to a specified background color.
    virtual void Clear(Color color, std::optional<Rect> rect = std::nullopt) = 0;

    /// Pushes a rectangular clipping boundary.
    virtual void PushClip(Rect clipRect) = 0;

    /// Restores the previous clipping boundary.
    virtual void PopClip() = 0;

    /// Draws a solid axis-aligned rectangle.
    virtual void DrawRect(Rect rect, Color fill) = 0;

    /// Draws a rounded rectangle with optional fill, stroke, and corner radius.
    virtual void DrawRoundRect(
        Rect rect,
        float radius,
        Color fill,
        Color stroke = Color(0, 0, 0, 0),
        float strokeWidth = 0.0f
    ) = 0;

    /// Measures the dimensions of a single-line text string using backend font metrics.
    virtual Vec2 MeasureText(std::string_view text, float fontSize) = 0;

    /// Measures the dimensions of a styled single-line text string (Bold, Italic, Monospace).
    virtual Vec2 MeasureTextStyled(std::string_view text, float fontSize, FontStyle /*style*/) {
        return MeasureText(text, fontSize);
    }

    /// Returns the vertical typographic line height for the specified font size according to backend font metrics.
    [[nodiscard]] virtual float GetLineHeight(float fontSize) = 0;

    /// Draws a single-line text string with specified font size and color.
    virtual void DrawText(std::string_view text, Vec2 pos, float fontSize, Color color) = 0;

    /// Draws a single-line text string with typographic style and optional rotation angle in degrees.
    virtual void DrawTextStyled(
        std::string_view text,
        Vec2 pos,
        float fontSize,
        Color color,
        FontStyle /*style*/,
        float /*angleDeg*/ = 0.0f
    ) {
        DrawText(text, pos, fontSize, color);
    }

    /// Draws text constrained within a bounding rectangle with horizontal and vertical alignment factors [0..1].
    virtual void DrawTextBox(
        std::string_view text,
        Rect bounds,
        float fontSize,
        Color color,
        float alignX = 0.0f,
        float alignY = 0.5f
    ) = 0;

    /// Draws a line segment between two points.
    virtual void DrawLine(Vec2 p1, Vec2 p2, Color color, float strokeWidth = 1.0f) = 0;

    /// Draws a filled and/or stroked triangle defined by three points.
    virtual void DrawTriangle(
        Vec2 p1,
        Vec2 p2,
        Vec2 p3,
        Color fill,
        Color stroke = Color(0, 0, 0, 0),
        float strokeWidth = 0.0f
    ) {
        if (fill.a > 0) {
            DrawLine(p1, p2, fill, 1.0f);
            DrawLine(p2, p3, fill, 1.0f);
            DrawLine(p3, p1, fill, 1.0f);
        }
        if (stroke.a > 0 && strokeWidth > 0.0f) {
            DrawLine(p1, p2, stroke, strokeWidth);
            DrawLine(p2, p3, stroke, strokeWidth);
            DrawLine(p3, p1, stroke, strokeWidth);
        }
    }

    /// Queries the natural (unscaled) dimensions of a vector/raster image (SVG, PNG, JPG, or inline <svg>).
    /// Returns {0, 0} if the image cannot be found or loaded.
    virtual Vec2 GetImageSize(std::string_view /*source*/) {
        return {0.0f, 0.0f};
    }

    /// Draws a vector or raster image (SVG, PNG, JPG, or inline <svg>) stretched to `destRect`.
    /// Returns true if the image was loaded and queued for rendering.
    virtual bool DrawImage(
        std::string_view /*source*/,
        Rect /*destRect*/,
        float /*cornerRadius*/ = 0.0f,
        uint8_t /*opacity*/ = 255
    ) {
        return false;
    }

    /// Draws an in-memory 32-bit ABGR8888 pixel buffer (`(a << 24) | (b << 16) | (g << 8) | r`) stretched to `destRect`.
    virtual bool DrawPixels(
        const uint32_t* pixels,
        uint32_t width,
        uint32_t height,
        Rect destRect,
        float cornerRadius = 0.0f,
        uint8_t opacity = 255
    ) {
        (void)cornerRadius;
        if (!pixels || width == 0 || height == 0 || opacity == 0 || destRect.w <= 0.0f || destRect.h <= 0.0f) {
            return false;
        }
        const float cw = destRect.w / static_cast<float>(width);
        const float ch = destRect.h / static_cast<float>(height);
        for (uint32_t y = 0; y < height; ++y) {
            const float ry = destRect.y + static_cast<float>(y) * ch;
            for (uint32_t x = 0; x < width; ++x) {
                const uint32_t px = pixels[y * width + x];
                const uint8_t r = static_cast<uint8_t>(px & 0xFFu);
                const uint8_t g = static_cast<uint8_t>((px >> 8) & 0xFFu);
                const uint8_t b = static_cast<uint8_t>((px >> 16) & 0xFFu);
                const uint8_t a = static_cast<uint8_t>(((px >> 24) & 0xFFu) * opacity / 255u);
                DrawRect(Rect(destRect.x + static_cast<float>(x) * cw, ry, cw + 0.5f, ch + 0.5f), Color(r, g, b, a));
            }
        }
        return true;
    }

    /// Draws a hardware or memory texture (e.g. SDL_Texture* handle or image id).
    virtual void DrawTexture(
        uint64_t /*textureId*/,
        Rect /*destRect*/,
        std::optional<Rect> /*srcRect*/ = std::nullopt,
        Color /*tint*/ = Color(255, 255, 255, 255)
    ) {}
};

} // namespace guikit
