#pragma once

#include <cstdint>
#include <cstring>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/backend/vector_backend.hpp"
#include "keel/array.h"
#include "keel/hash_map.h"
#include "keel/string.h"

namespace tvg {
class SwCanvas;
class Paint;
class Text;
} // namespace tvg

namespace guikit {

/// ThorVG-based vector graphics canvas implementation.
class ThorCanvas : public ICanvas {
public:
    explicit ThorCanvas(bool loadDefaultFonts = true);
    ~ThorCanvas() override;

    ThorCanvas(const ThorCanvas&) = delete;
    ThorCanvas& operator=(const ThorCanvas&) = delete;

    [[nodiscard]] SurfacePixelFormat GetPixelFormat() const noexcept override {
        return SurfacePixelFormat::ABGR8888_Premul;
    }

    /// Configures the render target buffer (ABGR8888 premultiplied).
    void SetTarget(uint32_t* buffer, uint32_t width, uint32_t height, uint32_t stride) override;

    /// Loads a TrueType/OpenType font from a virtual asset path or filesystem path for the specified `FontStyle` slot.
    bool LoadFontFile(keel::StringRef path, FontStyle style = FontStyle::Regular) override;

    /// Loads a TrueType/OpenType font directly from an in-memory buffer for the specified `FontStyle` slot.
    bool LoadFontMemory(
        keel::StringRef name,
        keel::ArrayRef<uint8_t> data,
        FontStyle style = FontStyle::Regular,
        keel::StringRef mimeType = "ttf"
    ) override;

    // ICanvas implementation
    void Begin(uint32_t width, uint32_t height, float scale = 1.0f) override;
    void End() override;
    [[nodiscard]] float GetScale() const noexcept override { return m_scale; }
    void SetScale(float scale) override { m_scale = (scale > 0.0f) ? scale : 1.0f; }
    [[nodiscard]] keel::StringRef GetBackendName() const noexcept override { return "ThorVG (Vector Software)"; }
    void Clear(Color color, keel::Optional<Rect> rect = keel::kNullOpt) override;

    void PushClip(Rect clipRect) override;
    void PopClip() override;

    void DrawRect(Rect rect, Color fill) override;
    void DrawRoundRect(
        Rect rect,
        float radius,
        Color fill,
        Color stroke = Color(0, 0, 0, 0),
        float strokeWidth = 0.0f
    ) override;

    Vec2 MeasureText(keel::StringRef text, float fontSize) override;
    Vec2 MeasureTextStyled(keel::StringRef text, float fontSize, FontStyle style) override;
    [[nodiscard]] float GetLineHeight(float fontSize) override;
    void DrawText(keel::StringRef text, Vec2 pos, float fontSize, Color color) override;
    void DrawTextStyled(
        keel::StringRef text,
        Vec2 pos,
        float fontSize,
        Color color,
        FontStyle style,
        float angleDeg = 0.0f
    ) override;
    void DrawTextBox(
        keel::StringRef text,
        Rect bounds,
        float fontSize,
        Color color,
        float alignX = 0.0f,
        float alignY = 0.5f
    ) override;
    void DrawTextBoxStyled(
        keel::StringRef text,
        Rect bounds,
        float fontSize,
        Color color,
        FontStyle style,
        float alignX = 0.0f,
        float alignY = 0.5f
    ) override;
    void DrawLine(Vec2 p1, Vec2 p2, Color color, float strokeWidth = 1.0f) override;
    void DrawTriangle(
        Vec2 p1,
        Vec2 p2,
        Vec2 p3,
        Color fill,
        Color stroke = Color(0, 0, 0, 0),
        float strokeWidth = 0.0f
    ) override;
    void DrawPath(const VectorPath& path, const PathPaint& paint) override;

    Vec2 GetImageSize(keel::StringRef source) override;
    bool DrawImage(
        keel::StringRef source,
        Rect destRect,
        float cornerRadius = 0.0f,
        uint8_t opacity = 255
    ) override;
    bool DrawPixels(
        const uint32_t* pixels,
        uint32_t width,
        uint32_t height,
        Rect destRect,
        float cornerRadius = 0.0f,
        uint8_t opacity = 255
    ) override;

    void SetGpuShadersEnabled(bool enabled) noexcept override {
        m_gpuShadersEnabled = enabled;
        if (!enabled) {
            m_gpuPasses.clear();
        }
    }
    [[nodiscard]] bool SupportsGpuShaders() const noexcept override { return m_gpuShadersEnabled; }
    bool DrawGpuShaderPass(const GpuShaderPassDesc& pass, bool punchHole = true) override;
    bool CaptureRegion(Rect rect, keel::Array<uint32_t>& outPixels, uint32_t& outW, uint32_t& outH) override;
    bool RestoreRegion(Rect rect, keel::ArrayRef<uint32_t> inPixels, uint32_t inW, uint32_t inH) override;

    [[nodiscard]] keel::ArrayRef<GpuShaderPassDesc> GetGpuShaderPasses() const noexcept override { return m_gpuPasses; }
    [[nodiscard]] bool HasActiveGpuPasses() const noexcept override {
        return m_gpuShadersEnabled && !m_gpuPasses.empty();
    }

private:
    struct ImageCacheEntry {
        keel::String resolvedDataOrPath;
        const uint8_t* embeddedData{nullptr};
        size_t embeddedSize{0};
        keel::SmallString<16> embeddedMimeType;
        Vec2 size{0.0f, 0.0f};
        bool isInlineSvg{false};
        bool isEmbedded{false};
        bool valid{false};
        uint32_t cachedRasterW{0};
        uint32_t cachedRasterH{0};
        keel::Array<uint32_t> cachedPixels;
    };

    ImageCacheEntry& ResolveImageEntry(keel::StringRef source);
    bool LoadFontMemoryInternal(
        keel::StringRef name,
        keel::ArrayRef<uint8_t> data,
        FontStyle style,
        keel::StringRef mimeType,
        bool copyData
    );
    [[nodiscard]] bool NeedsSyntheticBold(FontStyle style) const noexcept;
    [[nodiscard]] bool NeedsSyntheticItalic(FontStyle style) const noexcept;
    void ApplyClip(tvg::Paint* paint, keel::Optional<Rect> bounds = keel::kNullOpt);
    void ApplyFont(tvg::Text* txt, FontStyle style) const;
    void PunchHole(Rect rect);
    void FlushCanvas();
    void SyncViewportClip();

    tvg::SwCanvas* m_canvas{nullptr};
    tvg::SwCanvas* m_stampCanvas{nullptr};
    keel::Array<uint32_t> m_stampScratch;
    tvg::Text* m_measureText{nullptr};
    uint32_t* m_targetBuffer{nullptr};
    uint32_t m_width{0};
    uint32_t m_height{0};
    uint32_t m_stride{0};
    float m_scale{1.0f};
    bool m_gpuShadersEnabled{false};
    bool m_hasPendingPaints{false};

    keel::Array<GpuShaderPassDesc> m_gpuPasses;

    keel::Array<Rect> m_clipStack;
    keel::Array<Rect> m_gpuClipStack;

    struct FontSlot {
        keel::String name;
        bool loaded{false};
    };
    static constexpr size_t kFontStyleCount = 5;
    keel::StaticArray<FontSlot, kFontStyleCount> m_fonts{};

    struct TextMeasureKey {
        uint64_t hash;
        uint32_t fontSizeBits;
        uint8_t style;

        bool operator==(const TextMeasureKey& other) const noexcept {
            return hash == other.hash && fontSizeBits == other.fontSizeBits && style == other.style;
        }
    };

    struct TextMeasureKeyHash {
        keel::u64 operator()(const TextMeasureKey& k) const noexcept {
            uint64_t mixed = k.hash ^ (static_cast<uint64_t>(k.fontSizeBits) * 0x9e3779b97f4a7c15ULL);
            mixed ^= static_cast<uint64_t>(k.style) * 0x517cc1b727220a95ULL;
            return mixed;
        }
    };

    struct TextStampKey {
        uint64_t textHash{0};
        uint32_t fontSizeBits{0};
        uint32_t colorRgba{0};
        uint16_t textLen{0};
        uint8_t style{0};

        bool operator==(const TextStampKey& o) const noexcept {
            return textHash == o.textHash &&
                   fontSizeBits == o.fontSizeBits &&
                   colorRgba == o.colorRgba &&
                   textLen == o.textLen &&
                   style == o.style;
        }
    };

    struct TextStampKeyHash {
        keel::u64 operator()(const TextStampKey& k) const noexcept {
            uint64_t h = k.textHash;
            h ^= static_cast<uint64_t>(k.fontSizeBits) * 0x9e3779b97f4a7c15ULL;
            h ^= static_cast<uint64_t>(k.colorRgba) * 0xbf58476d1ce4e5b9ULL;
            h ^= (static_cast<uint64_t>(k.textLen) << 8 | k.style) * 0x94d049bb133111ebULL;
            return h;
        }
    };

    struct TextStampEntry {
        keel::Array<uint32_t> pixels;
        uint16_t width{0};
        uint16_t height{0};
        int16_t offsetX{0};
        int16_t offsetY{0};
        bool empty{false};
    };

    struct ShapeStampKey {
        uint32_t fillRgba{0};
        uint32_t strokeRgba{0};
        uint16_t qW{0};
        uint16_t qH{0};
        uint16_t qRad{0};
        uint16_t qStrokeW{0};

        bool operator==(const ShapeStampKey& o) const noexcept {
            return fillRgba == o.fillRgba &&
                   strokeRgba == o.strokeRgba &&
                   qW == o.qW &&
                   qH == o.qH &&
                   qRad == o.qRad &&
                   qStrokeW == o.qStrokeW;
        }
    };

    struct ShapeStampKeyHash {
        keel::u64 operator()(const ShapeStampKey& k) const noexcept {
            uint64_t h = static_cast<uint64_t>(k.fillRgba) | (static_cast<uint64_t>(k.strokeRgba) << 32);
            h ^= (static_cast<uint64_t>(k.qW) | (static_cast<uint64_t>(k.qH) << 16) |
                  (static_cast<uint64_t>(k.qRad) << 32) | (static_cast<uint64_t>(k.qStrokeW) << 48)) * 0x9e3779b97f4a7c15ULL;
            h ^= h >> 33;
            return h * 0xff51afd7ed558ccdULL;
        }
    };

    const TextStampEntry& ResolveTextStamp(
        keel::StringRef text,
        float scaledFontSize,
        Color color,
        FontStyle style
    );
    const TextStampEntry& ResolveShapeStamp(
        float scaledW,
        float scaledH,
        float scaledRadius,
        Color fill,
        Color stroke,
        float scaledStrokeWidth
    );
    void BlitTextStamp(
        const TextStampEntry& stamp,
        int dstX,
        int dstY,
        keel::Optional<Rect> extraClip = keel::kNullOpt
    );

    keel::HashMap<TextMeasureKey, Vec2, TextMeasureKeyHash> m_textMeasureCache;
    keel::HashMap<TextStampKey, TextStampEntry, TextStampKeyHash> m_textStampCache;
    keel::HashMap<ShapeStampKey, TextStampEntry, ShapeStampKeyHash> m_shapeStampCache;
    keel::HashMap<keel::String, ImageCacheEntry> m_imageCache;
};

/// Creates a new `ThorCanvas` instance.
[[nodiscard]] keel::UniquePtr<ICanvas> CreateThorVectorCanvas(bool loadDefaultFonts = true);

/// Registers the ThorVG vector canvas factory and image decoder as the default vector backend.
void RegisterThorVectorBackend() noexcept;

} // namespace guikit
