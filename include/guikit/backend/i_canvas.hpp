#pragma once

#include "guikit/core/types.hpp"
#include "keel/array.h"
#include "keel/optional.h"

namespace guikit {

/// Typographic style variants for rich text and Markdown rendering.
enum class FontStyle : keel::u8 {
    Regular = 0,
    Bold,
    Italic,
    BoldItalic,
    Monospace
};

/// Anti-aliasing / subpixel smoothing mode for text rasterization.
enum class FontSmoothing : keel::u8 {
    Auto = 0,     ///< SubpixelRGB when canvas scale < 1.5, Grayscale on HiDPI (>= 1.5)
    Grayscale,    ///< 8-bit grayscale coverage anti-aliasing
    SubpixelRGB,  ///< 3x horizontal LCD subpixel anti-aliasing (RGB panel stripe)
    SubpixelBGR   ///< 3x horizontal LCD subpixel anti-aliasing (BGR panel stripe)
};

/// Outline grid-fitting hint level (honored by FreeType backend; ignored gracefully by outline-only fallbacks).
enum class FontHinting : keel::u8 {
    None = 0,     ///< Unhinted outlines (macOS / HiDPI style)
    Light,        ///< Vertical-only auto-hinting (FreeType Slight — crispest on 1x displays)
    Normal        ///< Full grid-fitting hinting
};

/// Engine-wide text rasterization and compositing quality settings.
struct TextRenderSettings {
    FontSmoothing smoothing{FontSmoothing::Auto};
    FontHinting hinting{FontHinting::Light};
    bool gammaCorrectBlending{true};  ///< Linear sRGB alpha/LCD compositing (prevents starved light-on-dark stems)
    keel::f32 contrast{1.0f};             ///< Stroke contrast multiplier [0.5 .. 2.0] (1.0 = neutral)
    keel::f32 stemDarkening{0.15f};       ///< Subtle coverage boost on small sizes [0.0 .. 0.5] (CoreText/DirectWrite style)

    [[nodiscard]] constexpr bool operator==(const TextRenderSettings& other) const noexcept = default;
};

/// Native 32-bit pixel format written by software/hybrid vector canvases into target memory.
enum class SurfacePixelFormat : keel::u8 {
    ABGR8888_Premul = 0, ///< 0xAABBGGRR premultiplied alpha (SDL_PIXELFORMAT_ABGR8888 on little-endian)
    ARGB8888_Premul = 1  ///< 0xAARRGGBB premultiplied alpha (SDL_PIXELFORMAT_ARGB8888 / Blend2D PRGB32)
};

/// Path command verbs for engine-agnostic 2D vector paths.
/// Values match standard vector rasterizer command encodings (Close=0, MoveTo=1, LineTo=2, CubicTo=3).
enum class PathVerb : keel::u8 {
    Close   = 0,
    MoveTo  = 1,
    LineTo  = 2,
    CubicTo = 3
};

/// Polygon interior winding rule (IHO S-52 / SVG / OGC standard).
enum class FillRule : keel::u8 {
    NonZero = 0, ///< Non-zero winding rule (exterior polygons without holes)
    EvenOdd = 1  ///< Even-odd alternating rule (polygons with interior island holes)
};

/// Engine-agnostic flat contiguous 2D vector path buffer.
/// Zero virtual call overhead during vertex assembly; reusable across frames via Clear().
struct VectorPath {
    keel::Array<PathVerb> verbs;
    keel::Array<Vec2> points;

    [[nodiscard]] VectorPath clone() const {
        VectorPath copy;
        copy.verbs = verbs.clone();
        copy.points = points.clone();
        return copy;
    }

    void Clear() noexcept {
        verbs.clear();
        points.clear();
    }

    void Reserve(keel::usize verbCap, keel::usize ptCap) {
        verbs.reserve(verbCap);
        points.reserve(ptCap);
    }

    void MoveTo(Vec2 p) {
        verbs.push_back(PathVerb::MoveTo);
        points.push_back(p);
    }

    void LineTo(Vec2 p) {
        verbs.push_back(PathVerb::LineTo);
        points.push_back(p);
    }

    void CubicTo(Vec2 c1, Vec2 c2, Vec2 p) {
        verbs.push_back(PathVerb::CubicTo);
        points.push_back(c1);
        points.push_back(c2);
        points.push_back(p);
    }

    void Close() {
        verbs.push_back(PathVerb::Close);
    }

    void AddClosedRing(keel::ArrayRef<Vec2> ring) {
        if (ring.size() < 3) return;
        verbs.push_back(PathVerb::MoveTo);
        points.push_back(ring[0]);
        for (keel::usize i = 1; i < ring.size(); ++i) {
            verbs.push_back(PathVerb::LineTo);
            points.push_back(ring[i]);
        }
        verbs.push_back(PathVerb::Close);
    }

    [[nodiscard]] bool Empty() const noexcept {
        return verbs.empty() || points.empty();
    }
};

/// Declarative fill and stroke style for ICanvas::DrawPath.
struct PathPaint {
    Color fillColor{0, 0, 0, 0};
    Color strokeColor{0, 0, 0, 0};
    keel::f32 strokeWidthPx{1.0f};
    FillRule fillRule{FillRule::NonZero};
    bool renderFill{false};
    bool renderStroke{false};
    bool dashed{false};
    keel::f32 dashOnPx{5.0f};
    keel::f32 dashOffPx{4.0f};
};

/// Abstract interface for vector graphics rendering backends.
class ICanvas {
public:
    virtual ~ICanvas() = default;

    /// Returns the native 32-bit pixel format written by this canvas into its target buffer.
    [[nodiscard]] virtual SurfacePixelFormat GetPixelFormat() const noexcept {
        return SurfacePixelFormat::ABGR8888_Premul;
    }

    /// Returns true if this canvas rasterizes into a CPU-side 32-bit pixel buffer bound via `SetTarget(...)`,
    /// or false if it renders directly to a hardware GPU / window surface without a CPU framebuffer.
    [[nodiscard]] virtual bool UsesCpuFramebuffer() const noexcept {
        return true;
    }

    /// Binds an in-memory 32-bit pixel buffer as the rendering target.
    virtual void SetTarget(keel::u32* /*buffer*/, keel::u32 /*width*/, keel::u32 /*height*/, keel::u32 /*stride*/) {}

    /// Loads a TrueType/OpenType font from a virtual asset path or filesystem path for the specified `FontStyle` slot.
    virtual bool LoadFontFile(keel::StringRef /*path*/, FontStyle /*style*/ = FontStyle::Regular) {
        return false;
    }

    /// Loads a TrueType/OpenType font directly from an in-memory buffer for the specified `FontStyle` slot.
    virtual bool LoadFontMemory(
        keel::StringRef /*name*/,
        keel::ArrayRef<keel::u8> /*data*/,
        FontStyle /*style*/ = FontStyle::Regular,
        keel::StringRef /*mimeType*/ = "ttf"
    ) {
        return false;
    }

    /// Prepares the canvas for a new frame with dimensions and optional DPI / UI scale factor.
    virtual void Begin(keel::u32 width, keel::u32 height, keel::f32 scale = 1.0f) = 0;

    /// Finalizes and commits all recorded rendering commands.
    virtual void End() = 0;

    /// Returns the current number of physical pixels per logical unit, including any pushed transforms.
    [[nodiscard]] virtual keel::f32 GetScale() const noexcept = 0;

    /// Sets or overrides the current canvas scale factor.
    virtual void SetScale(keel::f32 scale) = 0;

    /// Enters a local coordinate system: a point `p` drawn after this call lands where `transform.Apply(p)` would
    /// land before it. Transforms nest; text and strokes are rasterized at the resulting pixel scale, so scaled
    /// content stays crisp. Must be balanced by `PopTransform()`.
    virtual void PushTransform(const Transform2D& transform) = 0;

    /// Leaves the innermost transform pushed with `PushTransform()`.
    virtual void PopTransform() = 0;

    /// Human-readable backend identifier name (e.g. "ThorVG (Vector Software)", "Blend2D (JIT SIMD)", "SDL3 (Direct GPU)").
    [[nodiscard]] virtual keel::StringRef GetBackendName() const noexcept = 0;

    /// Clears the entire canvas surface or a targeted sub-rectangle to a specified background color.
    virtual void Clear(Color color, keel::Optional<Rect> rect = keel::kNullOpt) = 0;

    /// Pushes a rectangular clipping boundary.
    virtual void PushClip(Rect clipRect) = 0;

    /// Restores the previous clipping boundary.
    virtual void PopClip() = 0;

    /// Draws a solid axis-aligned rectangle.
    virtual void DrawRect(Rect rect, Color fill) = 0;

    /// Draws a rounded rectangle with optional fill, stroke, and corner radius.
    virtual void DrawRoundRect(
        Rect rect,
        keel::f32 radius,
        Color fill,
        Color stroke = Color(0, 0, 0, 0),
        keel::f32 strokeWidth = 0.0f
    ) = 0;

    /// Measures the dimensions of a single-line text string using backend font metrics.
    virtual Vec2 MeasureText(keel::StringRef text, keel::f32 fontSize) = 0;

    /// Measures the dimensions of a styled single-line text string (Bold, Italic, Monospace).
    virtual Vec2 MeasureTextStyled(keel::StringRef text, keel::f32 fontSize, FontStyle /*style*/) {
        return MeasureText(text, fontSize);
    }

    /// Returns the vertical typographic line height for the specified font size according to backend font metrics.
    [[nodiscard]] virtual keel::f32 GetLineHeight(keel::f32 fontSize) = 0;

    /// Draws a single-line text string with specified font size and color.
    virtual void DrawText(keel::StringRef text, Vec2 pos, keel::f32 fontSize, Color color) = 0;

    /// Draws a single-line text string with typographic style and optional rotation angle in degrees.
    virtual void DrawTextStyled(
        keel::StringRef text,
        Vec2 pos,
        keel::f32 fontSize,
        Color color,
        FontStyle /*style*/,
        keel::f32 /*angleDeg*/ = 0.0f
    ) {
        DrawText(text, pos, fontSize, color);
    }

    /// Draws text constrained within a bounding rectangle with horizontal and vertical alignment factors [0..1].
    virtual void DrawTextBox(
        keel::StringRef text,
        Rect bounds,
        keel::f32 fontSize,
        Color color,
        keel::f32 alignX = 0.0f,
        keel::f32 alignY = 0.5f
    ) = 0;

    /// Draws styled text constrained within a bounding rectangle with horizontal and vertical alignment factors [0..1].
    virtual void DrawTextBoxStyled(
        keel::StringRef text,
        Rect bounds,
        keel::f32 fontSize,
        Color color,
        FontStyle /*style*/,
        keel::f32 alignX = 0.0f,
        keel::f32 alignY = 0.5f
    ) {
        DrawTextBox(text, bounds, fontSize, color, alignX, alignY);
    }

    /// Convenience overload of DrawTextBox with explicit FontStyle.
    void DrawTextBox(
        keel::StringRef text,
        Rect bounds,
        keel::f32 fontSize,
        Color color,
        keel::f32 alignX,
        keel::f32 alignY,
        FontStyle style
    ) {
        DrawTextBoxStyled(text, bounds, fontSize, color, style, alignX, alignY);
    }

    /// Draws a soft Gaussian drop shadow for a rounded rectangle.
    /// GPU SDF backends (`GlEs3Canvas`) evaluate an analytical blurred rounded-box shadow in a single quad;
    /// other canvases fall back to a soft multi-layer rounded-rect approximation.
    virtual void DrawShadow(
        Rect rect,
        keel::f32 cornerRadius,
        keel::f32 blurRadius,
        Vec2 offset = {0.0f, 4.0f},
        Color color = Color(0, 0, 0, 96),
        keel::f32 spread = 0.0f
    );

    /// Draws a line segment between two points.
    virtual void DrawLine(Vec2 p1, Vec2 p2, Color color, keel::f32 strokeWidth = 1.0f) = 0;

    /// Draws a smooth circular arc (angles in radians: 0 = +X right, increasing clockwise in screen space).
    /// GPU SDF backends (`GlEs3Canvas`) render this in a single pixel-exact quad; other canvases fall back
    /// to an exact cubic-Bezier annular path with integrated round or flat endcaps.
    virtual void DrawArc(
        Vec2 center,
        keel::f32 radius,
        keel::f32 startAngleRad,
        keel::f32 endAngleRad,
        Color color,
        keel::f32 strokeWidth = 1.5f,
        bool roundCaps = true
    );

    /// Draws a filled and/or stroked triangle defined by three points.
    virtual void DrawTriangle(
        Vec2 p1,
        Vec2 p2,
        Vec2 p3,
        Color fill,
        Color stroke = Color(0, 0, 0, 0),
        keel::f32 strokeWidth = 0.0f
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

    /// Draws an arbitrary 2D vector path (polygons with interior holes, compound polylines, Bezier curves).
    virtual void DrawPath(const VectorPath& path, const PathPaint& paint) {
        if (path.Empty()) return;
        const Color col = (paint.renderStroke && paint.strokeColor.a > 0) ? paint.strokeColor : paint.fillColor;
        const keel::f32 sw = (paint.renderStroke && paint.strokeWidthPx > 0.0f) ? paint.strokeWidthPx : 1.0f;
        if (col.a == 0) return;

        Vec2 startPt{};
        Vec2 curPt{};
        bool hasSubpath = false;
        keel::usize ptIdx = 0;
        const keel::usize numPts = path.points.size();
        for (PathVerb verb : path.verbs) {
            switch (verb) {
                case PathVerb::MoveTo:
                    if (ptIdx < numPts) {
                        startPt = path.points[ptIdx++];
                        curPt = startPt;
                        hasSubpath = true;
                    }
                    break;
                case PathVerb::LineTo:
                    if (ptIdx < numPts) {
                        const Vec2 nextPt = path.points[ptIdx++];
                        if (hasSubpath) {
                            DrawLine(curPt, nextPt, col, sw);
                        }
                        curPt = nextPt;
                        hasSubpath = true;
                    }
                    break;
                case PathVerb::CubicTo:
                    if (ptIdx + 2 < numPts) {
                        const Vec2 endPt = path.points[ptIdx + 2];
                        ptIdx += 3;
                        if (hasSubpath) {
                            DrawLine(curPt, endPt, col, sw);
                        }
                        curPt = endPt;
                        hasSubpath = true;
                    }
                    break;
                case PathVerb::Close:
                    if (hasSubpath) {
                        DrawLine(curPt, startPt, col, sw);
                        curPt = startPt;
                    }
                    break;
            }
        }
    }

    /// Queries the natural (unscaled) dimensions of a vector/raster image (SVG, PNG, JPG, or inline <svg>).
    /// Returns {0, 0} if the image cannot be found or loaded.
    virtual Vec2 GetImageSize(keel::StringRef /*source*/) {
        return {0.0f, 0.0f};
    }

    /// Draws a vector or raster image (SVG, PNG, JPG, or inline <svg>) stretched to `destRect`.
    /// Returns true if the image was loaded and queued for rendering.
    virtual bool DrawImage(
        keel::StringRef /*source*/,
        Rect /*destRect*/,
        keel::f32 /*cornerRadius*/ = 0.0f,
        keel::u8 /*opacity*/ = 255
    ) {
        return false;
    }

    /// Draws an in-memory 32-bit ABGR8888 2D raster view (`width x height`) stretched to `destRect`.
    virtual bool DrawPixels(
        PixelRef pixels,
        Rect destRect,
        keel::f32 cornerRadius = 0.0f,
        keel::u8 opacity = 255
    ) {
        (void)cornerRadius;
        if (pixels.Empty() || opacity == 0 || destRect.w <= 0.0f || destRect.h <= 0.0f) {
            return false;
        }
        const keel::u32 width = pixels.Width();
        const keel::u32 height = pixels.Height();
        const keel::f32 cw = destRect.w / keel::as<keel::f32>(width);
        const keel::f32 ch = destRect.h / keel::as<keel::f32>(height);
        for (keel::u32 y = 0; y < height; ++y) {
            const keel::f32 ry = destRect.y + keel::as<keel::f32>(y) * ch;
            const auto row = pixels.Row(y);
            for (keel::u32 x = 0; x < width; ++x) {
                const keel::u32 px = row[x];
                const keel::u8 r = keel::as<keel::u8>(px & 0xFFu);
                const keel::u8 g = keel::as<keel::u8>((px >> 8) & 0xFFu);
                const keel::u8 b = keel::as<keel::u8>((px >> 16) & 0xFFu);
                const keel::u8 a = keel::as<keel::u8>(((px >> 24) & 0xFFu) * opacity / 255u);
                DrawRect(Rect(destRect.x + keel::as<keel::f32>(x) * cw, ry, cw + 0.5f, ch + 0.5f), Color(r, g, b, a));
            }
        }
        return true;
    }

    /// Draws a hardware or memory texture (e.g. SDL_Texture* handle or image id).
    virtual void DrawTexture(
        keel::u64 /*textureId*/,
        Rect /*destRect*/,
        keel::Optional<Rect> /*srcRect*/ = keel::kNullOpt,
        Color /*tint*/ = Color(255, 255, 255, 255)
    ) {}

    /// Multi-API fragment shader source bundle for hardware GPU underlay passes.
    struct GpuShaderSource {
        keel::StringRef shaderId;     ///< Unique program cache key (e.g. "guikit.sonar.v1")
        keel::StringRef glslEs2;      ///< OpenGL 2.1 / OpenGL ES 2.0 / WebGL 1.0 & 2.0 fragment shader source
        keel::StringRef wgsl{};       ///< Optional WebGPU WGSL source
        keel::StringRef hlsl{};       ///< Optional Direct3D 11/12 HLSL source
        keel::StringRef msl{};        ///< Optional Apple Metal MSL source
    };

    /// Generic 16-byte aligned `float4` uniform register (`std140` / `cbuffer` layout).
    struct GpuFloat4 {
        keel::f32 x{0.0f};
        keel::f32 y{0.0f};
        keel::f32 z{0.0f};
        keel::f32 w{0.0f};
    };

    /// Declarative, domain-agnostic descriptor for a 2D hardware GPU shader underlay pass.
    struct GpuShaderPassDesc {
        keel::u64 widgetId{0};
        GpuShaderSource shader{};

        Rect plotRect{};
        Rect clipRect{};
        Color bgColor{0, 0, 0, 255};

        // 2D Data Texture (Slot 0: `u_DataTex`)
        keel::MdRef<keel::f32, 2> dataField{};    ///< Contiguous row-major [rows (texHeight) x cols (texWidth)] float field
        keel::u64 fullGeneration{0};          ///< Incremented when the full 2D grid changes
        keel::u64 sliceGeneration{0};         ///< Optional: incremented when a single row is written in a circular ring buffer
        keel::u32 headRow{0};                 ///< Optional: ring-buffer head row index [0 .. rows-1] for O(1) sub-row upload
        keel::f32 dataNormOffset{0.0f};          ///< Value mapped to 0.0 in RG16 upload (0.0 for [0..1] data, -140.0 for dB data)
        keel::f32 dataNormScale{1.0f};           ///< Span mapped to [0..1] in RG16 upload (1.0 for [0..1] data, 160.0 for dB data)

        // 1D Colormap LUT Texture (Slot 1: `u_LutTex`, 256 x 1 ABGR8888)
        keel::u8 colormapId{0};
        keel::ArrayRef<keel::u32> lutAbgr256{}; ///< Persistent 256-entry ABGR8888 LUT slice (zero-copy)

        // Standard 8 x float4 (128-byte std140) uniform parameter bank (`uniform vec4 u_Params[8]`)
        keel::StaticArray<GpuFloat4, 8> params{};
    };

    /// Enables or disables GPU shader underlay pass recording on this canvas.
    virtual void SetGpuShadersEnabled(bool /*enabled*/) noexcept {}

    /// Returns true if GPU shader field rendering is active on this canvas.
    [[nodiscard]] virtual bool SupportsGpuShaders() const noexcept { return false; }

    /// Returns true if one or more GPU shader underlay passes were recorded in the current frame.
    [[nodiscard]] virtual bool HasActiveGpuPasses() const noexcept { return false; }

    /// Returns recorded GPU shader underlay passes for the current frame.
    [[nodiscard]] virtual keel::ArrayRef<GpuShaderPassDesc> GetGpuShaderPasses() const noexcept {
        return {};
    }

    /// Queues a generic 2D GPU shader underlay pass and optionally punches a transparent window in the CPU layer.
    /// Returns true if handled by GPU, or false if the caller should fall back to CPU rasterization.
    virtual bool DrawGpuShaderPass(const GpuShaderPassDesc& /*pass*/, bool /*punchHole*/ = true) { return false; }

    /// Flushes pending draw commands and captures a physical pixel snapshot of `rect` for retained base-layer caching.
    virtual bool CaptureRegion(Rect /*rect*/, keel::Array<keel::u32>& /*outPixels*/, keel::u32& /*outW*/, keel::u32& /*outH*/) {
        return false;
    }

    /// Flushes pending draw commands and blits a previously captured pixel snapshot directly into the target surface.
    virtual bool RestoreRegion(Rect /*rect*/, PixelRef /*pixels*/) {
        return false;
    }

    /// Releases regenerable internal glyph, shape, image, and scratch caches when a canvas goes idle.
    virtual void TrimCaches() noexcept {}

    /// Returns the active text rasterizer backend name (e.g. "ThorVG", "FreeType", "FreeType + HarfBuzz").
    [[nodiscard]] virtual keel::StringRef GetTextEngineName() const noexcept { return "Default"; }

    /// Returns true if the active text rasterizer honors `TextRenderSettings::hinting`.
    [[nodiscard]] virtual bool SupportsFontHinting() const noexcept { return false; }

    /// Configures text anti-aliasing, hinting, gamma-corrected blending, contrast, and stem darkening.
    virtual void SetTextRenderSettings(const TextRenderSettings& /*settings*/) noexcept {}

    /// Returns the active text rendering quality settings.
    [[nodiscard]] virtual const TextRenderSettings& GetTextRenderSettings() const noexcept {
        static constexpr TextRenderSettings kDefault{};
        return kDefault;
    }
};

} // namespace guikit
