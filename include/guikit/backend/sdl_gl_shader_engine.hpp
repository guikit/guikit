#pragma once

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
    [[nodiscard]] keel::StringRef GetBackendSummary() const noexcept { return m_backendSummary.as_ref(); }

    /// Renders all active `GpuShaderPassDesc` passes recorded on `canvas` into the current OpenGL backbuffer.
    /// Must be called after `SDL_RenderClear(renderer)` and before compositing the UI overlay texture.
    void RenderUnderlays(const ICanvas& canvas, keel::u32 viewportWidth, keel::u32 viewportHeight);

    /// Advances per-frame maintenance and prunes GPU textures / staging buffers of inactive widgets,
    /// even when the active screen records zero GPU shader passes.
    void TickFrame(const ICanvas& canvas);

    /// Immediately releases all cached per-widget GPU data/LUT textures and CPU upload scratch buffers.
    void ReleaseCachedTextures();

    /// Returns true if any cached GPU texture is no longer referenced by an active pass on screen.
    [[nodiscard]] bool HasIdleCachedResources() const noexcept;

private:
    struct CompiledProgram {
        keel::u32 program{0};
        keel::i32 uViewport{-1};
        keel::i32 uPlotRect{-1};
        keel::i32 uGridDims{-1};
        keel::i32 uBgColor{-1};
        keel::i32 uParams{-1};
        keel::i32 uDataTex{-1};
        keel::i32 uLutTex{-1};
    };

    struct WidgetTextureCache {
        keel::u32 dataTex{0};
        keel::u32 lutTex{0};
        keel::u32 width{0};
        keel::u32 height{0};
        keel::u64 uploadedFullGen{0};
        keel::u64 uploadedSliceGen{0};
        keel::u32 uploadedHeadRow{0};
        keel::u8 uploadedColormapId{255};
        keel::u64 lastUsedFrame{0};
        keel::u64 lastUsedTimeNs{0};
    };

    const CompiledProgram* GetOrCompileProgram(const ICanvas::GpuShaderSource& src);
    void RenderPass(const ICanvas::GpuShaderPassDesc& pass, keel::u32 viewportW, keel::u32 viewportH);
    void PruneStaleTextures();

    SDL_Renderer* m_renderer{nullptr};
    bool m_available{false};
    keel::SmallString<64> m_backendSummary{"Disabled"};
    keel::u64 m_frameCounter{0};
    keel::u64 m_lastPruneTimeNs{0};

    keel::u32 m_quadVbo{0};

    keel::HashMap<keel::String, CompiledProgram> m_programs;
    keel::HashMap<keel::u64, WidgetTextureCache> m_textureCaches;
    keel::Array<keel::u32> m_uploadScratch;
};

} // namespace guikit
