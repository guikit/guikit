#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include "guikit/core/Types.hpp"

namespace guikit {

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

    /// Draws a single-line text string with specified font size and color.
    virtual void DrawText(std::string_view text, Vec2 pos, float fontSize, Color color) = 0;

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

    /// Draws a hardware or memory texture (e.g. SDL_Texture* handle or image id).
    virtual void DrawTexture(
        uint64_t /*textureId*/,
        Rect /*destRect*/,
        std::optional<Rect> /*srcRect*/ = std::nullopt,
        Color /*tint*/ = Color(255, 255, 255, 255)
    ) {}
};

} // namespace guikit
