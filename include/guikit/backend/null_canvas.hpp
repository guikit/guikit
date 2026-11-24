#pragma once

#include "guikit/backend/i_canvas.hpp"
#include "keel/array.h"
#include "keel/small_array.h"

namespace guikit {

/// Zero-dependency headless `ICanvas` implementation built into `guikit::core`.
/// Provides deterministic typographic metrics, transform/clip stacks, GPU shader pass
/// recording, and draw-call counters for backend-free builds, unit tests, and custom host scaffolding.
class NullCanvas : public ICanvas, public IGpuPassRenderer {
public:
    struct Stats {
        keel::u64 beginCount{0};
        keel::u64 endCount{0};
        keel::u64 clearCount{0};
        keel::u64 drawRectCount{0};
        keel::u64 drawRoundRectCount{0};
        keel::u64 drawTextCount{0};
        keel::u64 drawLineCount{0};
        keel::u64 drawTriangleCount{0};
        keel::u64 drawPathCount{0};
        keel::u64 drawImageCount{0};
        keel::u64 drawPixelsCount{0};
        keel::u64 drawTextureCount{0};
        keel::u64 drawGpuShaderPassCount{0};

        void Reset() noexcept {
            *this = Stats{};
        }
    };

    NullCanvas() noexcept = default;
    ~NullCanvas() override = default;

    [[nodiscard]] IGpuPassRenderer* QueryGpuPassRenderer() noexcept override { return this; }
    [[nodiscard]] const IGpuPassRenderer* QueryGpuPassRenderer() const noexcept override { return this; }

    void Begin(keel::u32 width, keel::u32 height, keel::f32 scale = 1.0f) override;
    void End() override;

    [[nodiscard]] keel::f32 GetScale() const noexcept override { return m_scale * m_xfScale; }
    void SetScale(keel::f32 scale) override;

    void PushTransform(const Transform2D& transform) override;
    void PopTransform() override;

    [[nodiscard]] keel::StringRef GetBackendName() const noexcept override {
        return "NullCanvas (Headless)";
    }

    void Clear(Color color, keel::Optional<Rect> rect = keel::kNullOpt) override;
    void PushClip(Rect clipRect) override;
    void PopClip() override;

    void DrawRect(Rect rect, Color fill) override;
    void DrawRoundRect(
        Rect rect,
        keel::f32 radius,
        Color fill,
        Color stroke = Color(0, 0, 0, 0),
        keel::f32 strokeWidth = 0.0f
    ) override;
    void DrawShadow(
        Rect rect,
        keel::f32 cornerRadius,
        keel::f32 blurRadius,
        Vec2 offset = {0.0f, 4.0f},
        Color color = Color(0, 0, 0, 96),
        keel::f32 spread = 0.0f
    ) override;

    Vec2 MeasureText(keel::StringRef text, keel::f32 fontSize) override;
    Vec2 MeasureTextStyled(keel::StringRef text, keel::f32 fontSize, FontStyle style) override;
    [[nodiscard]] keel::f32 GetLineHeight(keel::f32 fontSize) override;

    void DrawText(keel::StringRef text, Vec2 pos, keel::f32 fontSize, Color color) override;
    void DrawTextStyled(
        keel::StringRef text,
        Vec2 pos,
        keel::f32 fontSize,
        Color color,
        FontStyle style,
        keel::f32 angleDeg = 0.0f
    ) override;
    void DrawTextBox(
        keel::StringRef text,
        Rect bounds,
        keel::f32 fontSize,
        Color color,
        keel::f32 alignX = 0.0f,
        keel::f32 alignY = 0.5f
    ) override;
    void DrawTextBoxStyled(
        keel::StringRef text,
        Rect bounds,
        keel::f32 fontSize,
        Color color,
        FontStyle style,
        keel::f32 alignX = 0.0f,
        keel::f32 alignY = 0.5f
    ) override;

    void DrawLine(Vec2 p1, Vec2 p2, Color color, keel::f32 strokeWidth = 1.0f) override;
    void DrawArc(
        Vec2 center,
        keel::f32 radius,
        keel::f32 startAngleRad,
        keel::f32 endAngleRad,
        Color color,
        keel::f32 strokeWidth = 1.5f,
        bool roundCaps = true
    ) override;
    void DrawTriangle(
        Vec2 p1,
        Vec2 p2,
        Vec2 p3,
        Color fill,
        Color stroke = Color(0, 0, 0, 0),
        keel::f32 strokeWidth = 0.0f
    ) override;
    void DrawPath(const VectorPath& path, const PathPaint& paint) override;

    bool DrawImage(
        keel::StringRef source,
        Rect destRect,
        keel::f32 cornerRadius = 0.0f,
        keel::u8 opacity = 255
    ) override;
    bool DrawPixels(
        PixelRef pixels,
        Rect destRect,
        keel::f32 cornerRadius = 0.0f,
        keel::u8 opacity = 255
    ) override;
    void DrawTexture(
        keel::u64 textureId,
        Rect destRect,
        keel::Optional<Rect> srcRect = keel::kNullOpt,
        Color tint = Color(255, 255, 255, 255)
    ) override;

    void SetGpuShadersEnabled(bool enabled) noexcept override {
        m_gpuShadersEnabled = enabled;
        if (!enabled) {
            m_gpuPasses.clear();
        }
    }
    [[nodiscard]] bool SupportsGpuShaders() const noexcept override { return m_gpuShadersEnabled; }
    [[nodiscard]] bool HasActiveGpuPasses() const noexcept override { return !m_gpuPasses.empty(); }
    [[nodiscard]] keel::ArrayRef<GpuShaderPassDesc> GetGpuShaderPasses() const noexcept override {
        return m_gpuPasses;
    }
    bool DrawGpuShaderPass(const GpuShaderPassDesc& pass, bool punchHole = true) override;

    [[nodiscard]] keel::StringRef GetTextEngineName() const noexcept override {
        return "Null (Deterministic Metrics)";
    }
    void SetTextRenderSettings(const TextRenderSettings& settings) noexcept override {
        m_textSettings = settings;
    }
    [[nodiscard]] const TextRenderSettings& GetTextRenderSettings() const noexcept override {
        return m_textSettings;
    }

    [[nodiscard]] const Stats& GetStats() const noexcept { return m_stats; }
    void ResetStats() noexcept { m_stats.Reset(); }

private:
    struct TransformState {
        keel::f32 scale{1.0f};
        Vec2 offset{0.0f, 0.0f};
    };

    [[nodiscard]] Rect Px(Rect r) const noexcept {
        const keel::f32 eff = m_scale * m_xfScale;
        return Rect(
            (r.x * m_xfScale + m_xfOffset.x) * m_scale,
            (r.y * m_xfScale + m_xfOffset.y) * m_scale,
            r.w * eff,
            r.h * eff
        );
    }

    keel::u32 m_width{0};
    keel::u32 m_height{0};
    keel::f32 m_scale{1.0f};
    keel::f32 m_xfScale{1.0f};
    Vec2 m_xfOffset{0.0f, 0.0f};
    keel::SmallArray<TransformState, 8> m_transformStack;
    keel::SmallArray<Rect, 16> m_clipStack;
    bool m_gpuShadersEnabled{false};
    keel::Array<GpuShaderPassDesc> m_gpuPasses;
    TextRenderSettings m_textSettings{};
    Stats m_stats{};
};

} // namespace guikit
