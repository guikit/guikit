#pragma once

#include "guikit/backend/i_canvas.hpp"
#include "keel/arena.h"
#include "keel/array.h"
#include "keel/small_array.h"

namespace guikit {

/// Tagged command kind recorded inside an arena-backed `DrawList`.
enum class DrawCommandType : keel::u8 {
    Clear = 0,
    PushClip,
    PopClip,
    PushTransform,
    PopTransform,
    DrawRect,
    DrawRoundRect,
    DrawShadow,
    DrawGlassRect,
    DrawGradientRoundRect,
    DrawText,
    DrawTextStyled,
    DrawTextBox,
    DrawTextBoxStyled,
    DrawLine,
    DrawArc,
    DrawTriangle,
    DrawPath,
    DrawImage,
    DrawPixels,
    DrawTexture,
    DrawGpuShaderPass
};

/// Arena-allocated payload for `DrawCommandType::DrawPath`.
struct DrawPathPayload {
    keel::ArrayRef<PathVerb> verbs;
    keel::ArrayRef<Vec2> points;
    PathPaint paint{};
};

/// Arena-allocated payload for `DrawCommandType::DrawGpuShaderPass`.
struct DrawGpuPassPayload {
    GpuShaderPassDesc desc{};
    bool punchHole{true};
};

/// Arena-allocated payload for `DrawCommandType::DrawGradientRoundRect`.
struct DrawGradientPayload {
    Gradient fill{};
    Gradient strokeGradient{};
    bool hasStrokeGradient{false};
};

/// Compact POD command record stored in `DrawList`.
/// String/path/shader payloads reside in the bound `keel::Arena` (zero per-command heap allocations).
struct DrawCommand {
    DrawCommandType type{DrawCommandType::DrawRect};
    FontStyle fontStyle{FontStyle::Regular};
    bool flag{false};        ///< Optional rect / roundCaps / hasSrcRect
    keel::u8 opacity{255};
    Rect rect{};             ///< Primary rect or p1/p2 packed bounds
    Rect auxRect{};          ///< srcRect or triangle p1/p2/p3 aux coords
    Color color{};           ///< Fill / text / line / tint color
    Color strokeColor{};     ///< Optional stroke color
    keel::f32 p0{0.0f};      ///< radius / fontSize / strokeWidth / startAngle / scale
    keel::f32 p1{0.0f};      ///< strokeWidth / alignX / angleDeg / endAngle / blurRadius / offset.x
    keel::f32 p2{0.0f};      ///< alignY / spread / offset.y
    keel::f32 p3{0.0f};      ///< shadow offset.y / extra scalar
    keel::u64 handle{0};     ///< textureId
    keel::StringRef text{};  ///< Arena-allocated text or image source
    PixelRef pixels{};       ///< Pixel view for DrawPixels
    const void* payload{nullptr}; ///< Arena-allocated DrawPathPayload* or DrawGpuPassPayload*
};

/// Arena-backed 2D draw command stream recorded per frame and replayed via `ICanvas::Submit(const DrawList&)`.
class DrawList {
public:
    DrawList() noexcept = default;

    /// Resets recorded commands and binds either an external frame arena (zero heap allocs) or the internal arena.
    void Reset(keel::Arena* externalArena = nullptr) noexcept;

    [[nodiscard]] keel::usize CommandCount() const noexcept { return m_commands.size(); }
    [[nodiscard]] bool Empty() const noexcept { return m_commands.empty(); }
    [[nodiscard]] keel::ArrayRef<DrawCommand> Commands() const noexcept { return m_commands.as_ref(); }

    void PushClear(Color color, keel::Optional<Rect> rect = keel::kNullOpt);
    void PushClip(Rect clipRect, keel::f32 cornerRadius = 0.0f);
    void PopClip();
    void PushTransform(const Transform2D& transform);
    void PopTransform();
    void PushRect(Rect rect, Color fill);
    void PushRoundRect(Rect rect, keel::f32 radius, Color fill, Color stroke = Color(0, 0, 0, 0), keel::f32 strokeWidth = 0.0f);
    void PushShadow(Rect rect, keel::f32 cornerRadius, keel::f32 blurRadius, Vec2 offset, Color color, keel::f32 spread);
    void PushGlassRect(Rect rect, keel::f32 radius, const GlassParams& params);
    void PushGradientRoundRect(
        Rect rect,
        keel::f32 radius,
        const Gradient& fill,
        Color stroke = Color(0, 0, 0, 0),
        keel::f32 strokeWidth = 0.0f,
        const Gradient* strokeGradient = nullptr
    );
    void PushText(keel::StringRef text, Vec2 pos, keel::f32 fontSize, Color color);
    void PushTextStyled(keel::StringRef text, Vec2 pos, keel::f32 fontSize, Color color, FontStyle style, keel::f32 angleDeg = 0.0f);
    void PushTextBox(keel::StringRef text, Rect bounds, keel::f32 fontSize, Color color, keel::f32 alignX = 0.0f, keel::f32 alignY = 0.5f);
    void PushTextBoxStyled(keel::StringRef text, Rect bounds, keel::f32 fontSize, Color color, FontStyle style, keel::f32 alignX = 0.0f, keel::f32 alignY = 0.5f);
    void PushLine(Vec2 p1, Vec2 p2, Color color, keel::f32 strokeWidth = 1.0f);
    void PushArc(Vec2 center, keel::f32 radius, keel::f32 startAngleRad, keel::f32 endAngleRad, Color color, keel::f32 strokeWidth = 1.5f, bool roundCaps = true);
    void PushTriangle(Vec2 p1, Vec2 p2, Vec2 p3, Color fill, Color stroke = Color(0, 0, 0, 0), keel::f32 strokeWidth = 0.0f);
    void PushPath(const VectorPath& path, const PathPaint& paint);
    void PushImage(keel::StringRef source, Rect destRect, keel::f32 cornerRadius = 0.0f, keel::u8 opacity = 255);
    void PushPixels(PixelRef pixels, Rect destRect, keel::f32 cornerRadius = 0.0f, keel::u8 opacity = 255, bool copyPixels = false);
    void PushTexture(keel::u64 textureId, Rect destRect, keel::Optional<Rect> srcRect = keel::kNullOpt, Color tint = Color(255, 255, 255, 255));
    void PushGpuShaderPass(const GpuShaderPassDesc& pass, bool punchHole = true);

    /// Replays all recorded commands in order onto `target`.
    void Replay(ICanvas& target) const;

private:
    [[nodiscard]] keel::Arena& ActiveArena() noexcept {
        return m_externalArena ? *m_externalArena : m_ownedArena;
    }
    [[nodiscard]] keel::StringRef CopyString(keel::StringRef s);

    keel::Arena m_ownedArena{4096};
    keel::Arena* m_externalArena{nullptr};
    keel::Array<DrawCommand> m_commands;
};

/// `ICanvas` adapter that records draw commands into a `DrawList` while forwarding metrics/font queries
/// (and flushing transparently on CPU surface readback/restore) to an optional target `ICanvas`.
class DrawListRecorder final : public ICanvas, public ICpuSurfaceCanvas, public IGpuPassRenderer {
public:
    explicit DrawListRecorder(DrawList& list, ICanvas* target = nullptr) noexcept;

    [[nodiscard]] ICpuSurfaceCanvas* QueryCpuSurface() noexcept override {
        return (m_target && m_target->QueryCpuSurface()) ? static_cast<ICpuSurfaceCanvas*>(this) : nullptr;
    }
    [[nodiscard]] const ICpuSurfaceCanvas* QueryCpuSurface() const noexcept override {
        return (m_target && m_target->QueryCpuSurface()) ? static_cast<const ICpuSurfaceCanvas*>(this) : nullptr;
    }
    [[nodiscard]] IGpuPassRenderer* QueryGpuPassRenderer() noexcept override {
        return (m_target && m_target->QueryGpuPassRenderer()) ? static_cast<IGpuPassRenderer*>(this) : nullptr;
    }
    [[nodiscard]] const IGpuPassRenderer* QueryGpuPassRenderer() const noexcept override {
        return (m_target && m_target->QueryGpuPassRenderer()) ? static_cast<const IGpuPassRenderer*>(this) : nullptr;
    }

    // ICpuSurfaceCanvas (flushes pending DrawList to target before pixel capture/restore)
    [[nodiscard]] SurfacePixelFormat GetPixelFormat() const noexcept override;
    void SetTarget(keel::u32* buffer, keel::u32 width, keel::u32 height, keel::u32 stride) override;
    bool CaptureRegion(Rect rect, keel::Array<keel::u32>& outPixels, keel::u32& outW, keel::u32& outH) override;
    bool RestoreRegion(Rect rect, PixelRef pixels) override;

    // IGpuPassRenderer
    void SetGpuShadersEnabled(bool enabled) noexcept override;
    [[nodiscard]] bool SupportsGpuShaders() const noexcept override;
    [[nodiscard]] bool HasActiveGpuPasses() const noexcept override;
    [[nodiscard]] keel::ArrayRef<GpuShaderPassDesc> GetGpuShaderPasses() const noexcept override;
    bool DrawGpuShaderPass(const GpuShaderPassDesc& pass, bool punchHole = true) override;

    // ICanvas
    void Begin(keel::u32 width, keel::u32 height, keel::f32 scale = 1.0f) override;
    void End() override;
    [[nodiscard]] keel::f32 GetScale() const noexcept override { return m_baseTransform.scale * m_xfScale; }
    [[nodiscard]] Transform2D GetPixelTransform() const noexcept override {
        return m_baseTransform.Then(Transform2D{m_xfScale, m_xfOffset});
    }
    void SetScale(keel::f32 scale) override {
        const keel::f32 s = (scale > 0.0f) ? scale : 1.0f;
        m_scale = s;
        m_baseTransform.scale = s;
    }
    void PushTransform(const Transform2D& transform) override;
    void PopTransform() override;
    [[nodiscard]] keel::StringRef GetBackendName() const noexcept override {
        return m_target ? m_target->GetBackendName() : keel::StringRef("DrawListRecorder");
    }

    [[nodiscard]] bool SupportsRoundedClip() const noexcept override {
        return m_target ? m_target->SupportsRoundedClip() : true;
    }
    [[nodiscard]] bool SupportsBackdropBlur() const noexcept override {
        return m_target ? m_target->SupportsBackdropBlur() : false;
    }
    void Clear(Color color, keel::Optional<Rect> rect = keel::kNullOpt) override;
    void PushClip(Rect clipRect) override;
    void PushRoundedClip(Rect clipRect, keel::f32 cornerRadius) override;
    void PopClip() override;

    void DrawRect(Rect rect, Color fill) override;
    void DrawRoundRect(Rect rect, keel::f32 radius, Color fill, Color stroke = Color(0, 0, 0, 0), keel::f32 strokeWidth = 0.0f) override;
    void DrawShadow(Rect rect, keel::f32 cornerRadius, keel::f32 blurRadius, Vec2 offset = {0.0f, 4.0f}, Color color = Color(0, 0, 0, 96), keel::f32 spread = 0.0f) override;
    void DrawGlassRect(Rect rect, keel::f32 radius, const GlassParams& params) override;
    void DrawGradientRoundRect(
        Rect rect,
        keel::f32 radius,
        const Gradient& fill,
        Color stroke = Color(0, 0, 0, 0),
        keel::f32 strokeWidth = 0.0f,
        const Gradient* strokeGradient = nullptr
    ) override;

    Vec2 MeasureText(keel::StringRef text, keel::f32 fontSize) override;
    Vec2 MeasureTextStyled(keel::StringRef text, keel::f32 fontSize, FontStyle style) override;
    [[nodiscard]] keel::f32 GetLineHeight(keel::f32 fontSize) override;

    void DrawText(keel::StringRef text, Vec2 pos, keel::f32 fontSize, Color color) override;
    void DrawTextStyled(keel::StringRef text, Vec2 pos, keel::f32 fontSize, Color color, FontStyle style, keel::f32 angleDeg = 0.0f) override;
    void DrawTextBox(keel::StringRef text, Rect bounds, keel::f32 fontSize, Color color, keel::f32 alignX = 0.0f, keel::f32 alignY = 0.5f) override;
    void DrawTextBoxStyled(keel::StringRef text, Rect bounds, keel::f32 fontSize, Color color, FontStyle style, keel::f32 alignX = 0.0f, keel::f32 alignY = 0.5f) override;

    void DrawLine(Vec2 p1, Vec2 p2, Color color, keel::f32 strokeWidth = 1.0f) override;
    void DrawArc(Vec2 center, keel::f32 radius, keel::f32 startAngleRad, keel::f32 endAngleRad, Color color, keel::f32 strokeWidth = 1.5f, bool roundCaps = true) override;
    void DrawTriangle(Vec2 p1, Vec2 p2, Vec2 p3, Color fill, Color stroke = Color(0, 0, 0, 0), keel::f32 strokeWidth = 0.0f) override;
    void DrawPath(const VectorPath& path, const PathPaint& paint) override;

    Vec2 GetImageSize(keel::StringRef source) override;
    bool DrawImage(keel::StringRef source, Rect destRect, keel::f32 cornerRadius = 0.0f, keel::u8 opacity = 255) override;
    bool DrawPixels(PixelRef pixels, Rect destRect, keel::f32 cornerRadius = 0.0f, keel::u8 opacity = 255) override;
    void DrawTexture(keel::u64 textureId, Rect destRect, keel::Optional<Rect> srcRect = keel::kNullOpt, Color tint = Color(255, 255, 255, 255)) override;

    [[nodiscard]] keel::StringRef GetTextEngineName() const noexcept override;
    [[nodiscard]] bool SupportsFontHinting() const noexcept override;
    void SetTextRenderSettings(const TextRenderSettings& settings) noexcept override;
    [[nodiscard]] const TextRenderSettings& GetTextRenderSettings() const noexcept override;

    /// Flushes currently buffered commands to `m_target` and clears the buffer (used before pixel readback).
    void FlushToTarget();

    /// Returns true if any commands were already flushed directly to `m_target` (e.g. for `CaptureRegion`).
    [[nodiscard]] bool HadPartialFlush() const noexcept { return m_flushedCount > 0u; }

private:
    struct TransformState {
        keel::f32 scale{1.0f};
        Vec2 offset{0.0f, 0.0f};
    };

    DrawList& m_list;
    ICanvas* m_target{nullptr};
    Transform2D m_baseTransform{1.0f, Vec2{0.0f, 0.0f}};
    keel::f32 m_scale{1.0f};
    keel::f32 m_xfScale{1.0f};
    Vec2 m_xfOffset{0.0f, 0.0f};
    keel::SmallArray<TransformState, 8> m_transformStack;
    keel::usize m_flushedCount{0};
    bool m_hadGpuPass{false};
};

} // namespace guikit
