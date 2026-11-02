#pragma once

#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>
#include "guikit/backend/IWindowBackend.hpp"
#include "guikit/backend/SdlGlShaderEngine.hpp"
#include "guikit/backend/VectorBackend.hpp"

namespace guikit {

/// Queries the physical pixel density of the display hosting `window` in pixels per millimeter (px/mm).
/// - Windows (`_WIN32`): Queries per-monitor hardware EDID physical dimensions via `MonitorFromWindow` + GDI `GetDeviceCaps`.
/// - WebAssembly (`__EMSCRIPTEN__`): Uses `emscripten_get_device_pixel_ratio()` relative to standard 96 CSS DPI.
/// - Linux / macOS / Fallback: Uses `SDL_GetWindowDisplayScale(window)` relative to 96 DPI.
[[nodiscard]] float QueryDisplayPixelsPerMm(SDL_Window* window = nullptr) noexcept;

/// SDL3 + OpenGL ES3 / WebGL2 implementation of `guikit::IWindowBackend` with interchangeable `ICanvas`.
class SdlWindowBackend final : public IWindowBackend {
public:
    explicit SdlWindowBackend(std::unique_ptr<ICanvas> canvas = nullptr);
    ~SdlWindowBackend() override;

    SdlWindowBackend(const SdlWindowBackend&) = delete;
    SdlWindowBackend& operator=(const SdlWindowBackend&) = delete;

    bool Initialize(const MainWindowConfig& config, Context& ui, const WindowHostCallbacks& callbacks) override;
    void Shutdown() noexcept override;

    bool PumpEvents(Context& ui, bool reactiveMode, int sleepTimeoutMs) override;
    [[nodiscard]] bool UsesMainLoopCallback() const noexcept override;
    void RunMainLoopCallback(std::function<bool()> stepFn) override;

    [[nodiscard]] ICanvas& GetCanvas() noexcept override { return *m_canvas; }
    [[nodiscard]] const ICanvas& GetCanvas() const noexcept override { return *m_canvas; }

    void PrepareFrameTarget(Context& ui) override;
    void PresentFrame(const Context& ui, FrameMetrics& metrics, bool forcePresent) override;
    void PaceFrame(double elapsedMs, bool presented) noexcept override;

    [[nodiscard]] int GetWidth() const noexcept override { return m_width; }
    [[nodiscard]] int GetHeight() const noexcept override { return m_height; }
    [[nodiscard]] float GetDisplayPixelsPerMm() const noexcept override { return m_displayPxPerMm; }
    [[nodiscard]] bool IsMinimized() const noexcept override { return m_minimized; }
    [[nodiscard]] bool IsLiveResizeFrame() const noexcept override { return m_isLiveResizeFrame; }

    [[nodiscard]] bool IsVsyncEnabled() const noexcept override { return m_vsyncEnabled; }
    void SetVsyncEnabled(bool enabled) noexcept override;

    [[nodiscard]] bool IsGpuShadersEnabled() const noexcept override { return m_gpuShadersEnabled; }
    [[nodiscard]] bool IsGpuShadersAvailable() const noexcept override { return m_gpuShaderEngine.IsAvailable(); }
    void SetGpuShadersEnabled(bool enabled) noexcept override;

    void UpdateSystemCursor(CursorType cursor) noexcept override;
    void OpenUrl(std::string_view url) const noexcept override;

    [[nodiscard]] SDL_Window* GetSdlWindow() const noexcept { return m_window; }
    [[nodiscard]] SDL_Renderer* GetSdlRenderer() const noexcept { return m_renderer; }
    [[nodiscard]] SdlGlShaderEngine& GetShaderEngine() noexcept { return m_gpuShaderEngine; }
    [[nodiscard]] const SdlGlShaderEngine& GetShaderEngine() const noexcept { return m_gpuShaderEngine; }

private:
    void ProcessEvent(const SDL_Event& event, Context& ui);
    static bool SDLCALL LiveResizeEventWatch(void* userdata, SDL_Event* event);

    MainWindowConfig m_config;
    WindowHostCallbacks m_callbacks;
    Context* m_boundUi = nullptr;

    bool m_sdlVideoInitialized = false;
    bool m_eventWatchInstalled = false;
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_screenTexture = nullptr;

    int m_width = 1380;
    int m_height = 800;
    int m_bufWidth = 0;
    int m_bufHeight = 0;
    std::vector<uint32_t> m_pixelBuffer;

    std::unique_ptr<ICanvas> m_canvas;
    bool m_hasExplicitCanvas = false;
    bool m_usingFallbackCanvas = false;
    SdlGlShaderEngine m_gpuShaderEngine;

    bool m_shouldExit = false;
    bool m_minimized = false;
    bool m_vsyncEnabled = true;
    bool m_gpuShadersEnabled = false;
    bool m_isLiveResizeFrame = false;
    uint32_t m_pressedMouseMask = 0;
    float m_displayPxPerMm = 96.0f / 25.4f;

    std::chrono::steady_clock::time_point m_lastLiveResizeTime{};
    std::chrono::steady_clock::time_point m_lastEmscriptenRenderTime{};
    std::function<bool()> m_emscriptenStepFn;

    std::array<SDL_Cursor*, 9> m_cursors{};
    CursorType m_activeCursor = CursorType::Default;
};

/// Creates a new `SdlWindowBackend` instance using the default registered vector canvas.
[[nodiscard]] std::unique_ptr<IWindowBackend> CreateSdlWindowBackend();

/// Creates a new `SdlWindowBackend` instance with an explicitly injected `ICanvas` backend.
[[nodiscard]] std::unique_ptr<IWindowBackend> CreateSdlWindowBackend(std::unique_ptr<ICanvas> canvas);

/// Registers `CreateSdlWindowBackend` as the default `IWindowBackend` factory for `guikit::MainWindow`.
void RegisterSdlWindowBackend() noexcept;

namespace detail {
struct SdlBackendAutoRegistrar {
    SdlBackendAutoRegistrar() noexcept { RegisterSdlWindowBackend(); }
};
inline const SdlBackendAutoRegistrar g_sdlBackendAutoRegistrar{};
} // namespace detail

} // namespace guikit
