#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>
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

    /// Declarative descriptor for hardware GPU shader rendering of a polar/sector/B-scan sonar/radar field.
    struct SonarGpuFieldDesc {
        uint64_t widgetId{0};
        uint64_t dataGeneration{0};
        const float* polarData{nullptr}; ///< Row-major [numBeams * numRangeBins] normalized echo [0..1]
        uint32_t numBeams{0};
        uint32_t numRangeBins{0};
        uint8_t colormapId{0};
        uint32_t colormapAbgr[256]{};
        Rect plotRect{};
        Rect clipRect{};
        Vec2 origin{};
        float maxRadiusPx{100.0f};
        float minRangeMeters{0.0f};
        float visibleMaxRangeMeters{500.0f};
        float matrixMaxRangeMeters{500.0f};
        float matrixStartBearingDeg{0.0f};
        float matrixEndBearingDeg{360.0f};
        uint8_t displayMode{0}; ///< 0 = PPI_360, 1 = SectorWedge, 2 = BScan
        Rect bscanInnerRect{};
        float headingDeg{0.0f};
        float sectorApertureDeg{120.0f};
        float gainDb{0.0f};
        float tvgAmount{0.5f};
        float clutterThreshold{0.1f};
        Color bgColor{6, 10, 18, 255};
    };

    /// Declarative descriptor for hardware GPU shader rendering of an STFT spectrogram / waterfall field.
    struct SpectrogramGpuFieldDesc {
        uint64_t widgetId{0};
        uint64_t dataGeneration{0};
        const float* dbGrid{nullptr}; ///< Row-major [texHeight * texWidth] dB values
        uint32_t texWidth{0};
        uint32_t texHeight{0};
        uint8_t colormapId{0};
        uint32_t colormapAbgr[256]{};
        Rect plotRect{};
        Rect clipRect{};
        float dbFloor{-100.0f};
        float dbCeil{0.0f};
        Color bgColor{8, 6, 22, 255};
    };

    /// Declarative descriptor for hardware GPU ring-buffer shader rendering of a scrolling Waterfall (RTSA Spectrum or DOA/BTR).
    struct WaterfallGpuFieldDesc {
        uint64_t widgetId{0};
        uint64_t fullGeneration{0};         ///< Incremented when the entire ring buffer is reset, resized, or bulk-loaded
        uint64_t sliceGeneration{0};        ///< Incremented each time a new row is written at `headRow`
        const float* ringBufferDb{nullptr}; ///< Row-major [texHeight * texWidth] circular buffer of dB values
        uint32_t texWidth{0};               ///< Number of X bins (frequency bins or azimuth beams)
        uint32_t texHeight{0};              ///< Number of history rows in the circular buffer
        uint32_t headRow{0};                ///< Index [0 .. texHeight-1] of the most recently written row (t = 0s)
        uint32_t validRows{0};              ///< Number of populated rows (1 .. texHeight)
        uint8_t colormapId{0};
        uint32_t colormapAbgr[256]{};
        Rect plotRect{};
        Rect clipRect{};
        float xMinNorm{0.0f};               ///< Visible X start normalized [0..1] for hardware zoom/pan
        float xMaxNorm{1.0f};               ///< Visible X end normalized [0..1] for hardware zoom/pan
        float dbFloor{-110.0f};
        float dbCeil{0.0f};
        float gainDb{0.0f};
        float thresholdGateDb{-140.0f};     ///< Noise gate threshold in dB
        float gateSuppression{0.0f};        ///< [0..1] suppression factor below thresholdGateDb
        float gamma{1.0f};                  ///< Contrast gamma exponent
        bool invertTimeAxis{false};         ///< false = newest at top flowing down; true = newest at bottom flowing up
        Color bgColor{6, 9, 18, 255};
    };

    /// Returns true if GPU shader field rendering is active on this canvas.
    [[nodiscard]] virtual bool SupportsGpuShaders() const noexcept { return false; }

    /// Queues a GPU shader pass for a sonar/radar polar heatmap and optionally punches a transparent window in the CPU layer.
    /// Returns true if handled by GPU, or false if the caller should fall back to CPU rasterization.
    virtual bool DrawSonarGpuField(const SonarGpuFieldDesc& /*desc*/, bool /*punchHole*/ = true) { return false; }

    /// Queues a GPU shader pass for an STFT spectrogram heatmap and optionally punches a transparent window in the CPU layer.
    /// Returns true if handled by GPU, or false if the caller should fall back to CPU rasterization.
    virtual bool DrawSpectrogramGpuField(const SpectrogramGpuFieldDesc& /*desc*/, bool /*punchHole*/ = true) { return false; }

    /// Queues a GPU ring-buffer shader pass for a scrolling Waterfall (RTSA or DOA/BTR) and optionally punches a transparent window in the CPU layer.
    /// Returns true if handled by GPU, or false if the caller should fall back to CPU rasterization.
    virtual bool DrawWaterfallGpuField(const WaterfallGpuFieldDesc& /*desc*/, bool /*punchHole*/ = true) { return false; }

    /// Flushes pending draw commands and captures a physical pixel snapshot of `rect` for retained base-layer caching.
    virtual bool CaptureRegion(Rect /*rect*/, std::vector<uint32_t>& /*outPixels*/, uint32_t& /*outW*/, uint32_t& /*outH*/) {
        return false;
    }

    /// Flushes pending draw commands and blits a previously captured pixel snapshot directly into the target surface.
    virtual bool RestoreRegion(Rect /*rect*/, const std::vector<uint32_t>& /*inPixels*/, uint32_t /*inW*/, uint32_t /*inH*/) {
        return false;
    }
};

} // namespace guikit

