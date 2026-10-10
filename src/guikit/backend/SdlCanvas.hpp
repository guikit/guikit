#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>
#include <SDL3/SDL.h>
#include "guikit/backend/ICanvas.hpp"

namespace guikit {

/// Hardware-accelerated SDL3 renderer backend for GuiKit GUI.
/// Renders directly to GPU targets via SDL_Renderer without intermediate RAM pixel buffers.
class SdlCanvas : public ICanvas {
public:
    explicit SdlCanvas(SDL_Renderer* renderer = nullptr) noexcept;
    ~SdlCanvas() override = default;

    SdlCanvas(const SdlCanvas&) = delete;
    SdlCanvas& operator=(const SdlCanvas&) = delete;

    /// Configures or updates the target SDL_Renderer handle.
    void SetRenderer(SDL_Renderer* renderer) noexcept { m_renderer = renderer; }
    [[nodiscard]] SDL_Renderer* GetRenderer() const noexcept { return m_renderer; }

    // ICanvas implementation
    void Begin(uint32_t width, uint32_t height, float scale = 1.0f) override;
    void End() override;
    [[nodiscard]] float GetScale() const noexcept override { return m_scale; }
    void SetScale(float scale) override;
    [[nodiscard]] std::string_view GetBackendName() const noexcept override { return "SDL3 (Direct GPU)"; }

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
    void DrawText(std::string_view text, Vec2 pos, float fontSize, Color color) override;
    void DrawTextBox(
        std::string_view text,
        Rect bounds,
        float fontSize,
        Color color,
        float alignX = 0.0f,
        float alignY = 0.5f
    ) override;
    void DrawLine(Vec2 p1, Vec2 p2, Color color, float strokeWidth = 1.0f) override;

    void DrawTexture(
        uint64_t textureId,
        Rect destRect,
        std::optional<Rect> srcRect = std::nullopt,
        Color tint = Color(255, 255, 255, 255)
    ) override;

private:
    void ApplyActiveClip();

    SDL_Renderer* m_renderer{nullptr};
    uint32_t m_width{0};
    uint32_t m_height{0};
    float m_scale{1.0f};
    std::vector<Rect> m_clipStack;
};

} // namespace guikit
