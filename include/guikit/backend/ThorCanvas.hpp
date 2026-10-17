#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include "guikit/backend/ICanvas.hpp"

namespace tvg {
class SwCanvas;
class Paint;
class Text;
} // namespace tvg

namespace guikit {

/// ThorVG-based vector graphics canvas implementation.
class ThorCanvas : public ICanvas {
public:
    ThorCanvas();
    ~ThorCanvas() override;

    ThorCanvas(const ThorCanvas&) = delete;
    ThorCanvas& operator=(const ThorCanvas&) = delete;

    /// Configures the render target buffer (ARGB8888 / ABGR8888).
    void SetTarget(uint32_t* buffer, uint32_t width, uint32_t height, uint32_t stride);

    /// Loads a TrueType/OpenType font from file for text rendering.
    bool LoadFontFile(const std::string& path);

    /// Loads a TrueType/OpenType monospace font from file for FontStyle::Monospace rendering.
    bool LoadMonospaceFontFile(const std::string& path);

    // ICanvas implementation
    void Begin(uint32_t width, uint32_t height, float scale = 1.0f) override;
    void End() override;
    [[nodiscard]] float GetScale() const noexcept override { return m_scale; }
    void SetScale(float scale) override { m_scale = (scale > 0.0f) ? scale : 1.0f; }
    [[nodiscard]] std::string_view GetBackendName() const noexcept override { return "ThorVG (Vector Software)"; }
    void Clear(Color color, std::optional<Rect> rect = std::nullopt) override;

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

    Vec2 MeasureText(std::string_view text, float fontSize) override;
    Vec2 MeasureTextStyled(std::string_view text, float fontSize, FontStyle style) override;
    [[nodiscard]] float GetLineHeight(float fontSize) override;
    void DrawText(std::string_view text, Vec2 pos, float fontSize, Color color) override;
    void DrawTextStyled(
        std::string_view text,
        Vec2 pos,
        float fontSize,
        Color color,
        FontStyle style,
        float angleDeg = 0.0f
    ) override;
    void DrawTextBox(
        std::string_view text,
        Rect bounds,
        float fontSize,
        Color color,
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

    Vec2 GetImageSize(std::string_view source) override;
    bool DrawImage(
        std::string_view source,
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

    void SetGpuShadersEnabled(bool enabled) noexcept {
        m_gpuShadersEnabled = enabled;
        if (!enabled) {
            m_sonarGpuPasses.clear();
            m_spectrogramGpuPasses.clear();
            m_waterfallGpuPasses.clear();
        }
    }
    [[nodiscard]] bool SupportsGpuShaders() const noexcept override { return m_gpuShadersEnabled; }
    bool DrawSonarGpuField(const SonarGpuFieldDesc& desc, bool punchHole = true) override;
    bool DrawSpectrogramGpuField(const SpectrogramGpuFieldDesc& desc, bool punchHole = true) override;
    bool DrawWaterfallGpuField(const WaterfallGpuFieldDesc& desc, bool punchHole = true) override;
    bool CaptureRegion(Rect rect, std::vector<uint32_t>& outPixels, uint32_t& outW, uint32_t& outH) override;
    bool RestoreRegion(Rect rect, const std::vector<uint32_t>& inPixels, uint32_t inW, uint32_t inH) override;

    [[nodiscard]] const std::vector<SonarGpuFieldDesc>& GetSonarGpuPasses() const noexcept { return m_sonarGpuPasses; }
    [[nodiscard]] const std::vector<SpectrogramGpuFieldDesc>& GetSpectrogramGpuPasses() const noexcept { return m_spectrogramGpuPasses; }
    [[nodiscard]] const std::vector<WaterfallGpuFieldDesc>& GetWaterfallGpuPasses() const noexcept { return m_waterfallGpuPasses; }
    [[nodiscard]] bool HasActiveGpuPasses() const noexcept {
        return m_gpuShadersEnabled && (!m_sonarGpuPasses.empty() || !m_spectrogramGpuPasses.empty() || !m_waterfallGpuPasses.empty());
    }

private:
    struct ImageCacheEntry {
        std::string resolvedDataOrPath;
        const uint8_t* embeddedData{nullptr};
        size_t embeddedSize{0};
        std::string embeddedMimeType;
        Vec2 size{0.0f, 0.0f};
        bool isInlineSvg{false};
        bool isEmbedded{false};
        bool valid{false};
    };

    const ImageCacheEntry& ResolveImageEntry(std::string_view source);
    void ApplyClip(tvg::Paint* paint, std::optional<Rect> bounds = std::nullopt);
    void ApplyFont(tvg::Text* txt, FontStyle style) const;
    void PunchHole(Rect rect);

    tvg::SwCanvas* m_canvas{nullptr};
    tvg::Text* m_measureText{nullptr};
    uint32_t* m_targetBuffer{nullptr};
    uint32_t m_width{0};
    uint32_t m_height{0};
    uint32_t m_stride{0};
    float m_scale{1.0f};
    bool m_gpuShadersEnabled{false};

    std::vector<SonarGpuFieldDesc> m_sonarGpuPasses;
    std::vector<SpectrogramGpuFieldDesc> m_spectrogramGpuPasses;
    std::vector<WaterfallGpuFieldDesc> m_waterfallGpuPasses;

    std::vector<Rect> m_clipStack;
    std::vector<Rect> m_gpuClipStack;
    std::string m_defaultFontPath;
    std::string m_defaultFontName;
    bool m_fontLoaded{false};
    std::string m_monoFontPath;
    std::string m_monoFontName;
    bool m_monoFontLoaded{false};

    struct TextMeasureKey {
        uint64_t hash;
        uint32_t fontSizeBits;
        uint8_t style;

        bool operator==(const TextMeasureKey& other) const noexcept {
            return hash == other.hash && fontSizeBits == other.fontSizeBits && style == other.style;
        }
    };

    struct TextMeasureKeyHash {
        size_t operator()(const TextMeasureKey& k) const noexcept {
            uint64_t mixed = k.hash ^ (static_cast<uint64_t>(k.fontSizeBits) * 0x9e3779b97f4a7c15ULL);
            mixed ^= static_cast<uint64_t>(k.style) * 0x517cc1b727220a95ULL;
            return static_cast<size_t>(mixed);
        }
    };

    std::unordered_map<TextMeasureKey, Vec2, TextMeasureKeyHash> m_textMeasureCache;
    std::unordered_map<std::string, ImageCacheEntry> m_imageCache;
};

} // namespace guikit
