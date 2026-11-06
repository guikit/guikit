#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include "guikit/backend/i_canvas.hpp"
#include "keel/hash_map.h"
#include "keel/string.h"

#if defined(_WIN32) && defined(_MSC_VER)
// Automatically export high-performance discrete GPU selection symbols (NVIDIA Optimus & AMD PowerXpress)
// from any Windows executable linking guikit::backend_sdl3 so OpenGL never falls back to Intel iGPU OpenGL-on-D3D12.
#pragma comment(linker, "/INCLUDE:NvOptimusEnablement")
#pragma comment(linker, "/INCLUDE:AmdPowerXpressRequestHighPerformance")
#endif

struct SDL_Renderer;

namespace guikit {

/// Domain-agnostic hardware GPU shader pass engine (OpenGL 2.1+ / OpenGL ES 2.0 / WebGL 1 & 2).
/// Lazily compiles and caches widget-supplied fragment shaders (`ICanvas::GpuShaderSource`),
/// manages per-widget 2D data textures (with O(1) circular ring-buffer row updates) and 1D LUT textures,
/// and renders 2D shader underlay quads beneath the vector UI layer.
class SdlGlShaderEngine {
public:
    SdlGlShaderEngine() = default;
    ~SdlGlShaderEngine();

    SdlGlShaderEngine(const SdlGlShaderEngine&) = delete;
    SdlGlShaderEngine& operator=(const SdlGlShaderEngine&) = delete;

    /// Initializes OpenGL entry points and validates the GLSL compiler on `renderer`.
    /// Returns true if the active SDL_Renderer is backed by OpenGL / OpenGL ES / WebGL.
    bool Initialize(SDL_Renderer* renderer);

    /// Releases all cached GPU textures, vertex buffers, and compiled shader programs.
    void Shutdown();

    /// Returns true if the GPU shader pipeline is initialized and operational.
    [[nodiscard]] bool IsAvailable() const noexcept { return m_available; }

    /// Returns a human-readable description of the active GPU shader backend.
    [[nodiscard]] std::string_view GetBackendSummary() const noexcept { return m_backendSummary; }

    /// Renders all active `GpuShaderPassDesc` passes recorded on `canvas` into the current OpenGL backbuffer.
    /// Must be called after `SDL_RenderClear(renderer)` and before compositing the UI overlay texture.
    void RenderUnderlays(const ICanvas& canvas, uint32_t viewportWidth, uint32_t viewportHeight);

private:
    struct CompiledProgram {
        uint32_t program{0};
        int uViewport{-1};
        int uPlotRect{-1};
        int uGridDims{-1};
        int uBgColor{-1};
        int uParams{-1};
        int uDataTex{-1};
        int uLutTex{-1};
    };

    struct WidgetTextureCache {
        uint32_t dataTex{0};
        uint32_t lutTex{0};
        uint32_t width{0};
        uint32_t height{0};
        uint64_t uploadedFullGen{0};
        uint64_t uploadedSliceGen{0};
        uint32_t uploadedHeadRow{0};
        uint8_t uploadedColormapId{255};
        uint64_t lastUsedFrame{0};
    };

    const CompiledProgram* GetOrCompileProgram(const ICanvas::GpuShaderSource& src);
    void RenderPass(const ICanvas::GpuShaderPassDesc& pass, uint32_t viewportW, uint32_t viewportH);
    void PruneStaleTextures();

    SDL_Renderer* m_renderer{nullptr};
    bool m_available{false};
    std::string m_backendSummary{"Disabled"};
    uint64_t m_frameCounter{0};

    uint32_t m_quadVbo{0};

    keel::HashMap<keel::String, CompiledProgram> m_programs;
    keel::HashMap<uint64_t, WidgetTextureCache> m_textureCaches;
    std::vector<uint32_t> m_uploadScratch;
};

} // namespace guikit
