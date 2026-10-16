#pragma once

#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "guikit/backend/ThorCanvas.hpp"

struct SDL_Renderer;

namespace guikit {

/// Production-ready hardware GPU shader engine (OpenGL 2.1+ / OpenGL ES 2.0 / WebGL 1 & 2)
/// for rendering high-density polar sonar/radar and STFT spectrogram fields underneath
/// the vector UI layer.
class SdlGlShaderEngine {
public:
    SdlGlShaderEngine() = default;
    ~SdlGlShaderEngine();

    SdlGlShaderEngine(const SdlGlShaderEngine&) = delete;
    SdlGlShaderEngine& operator=(const SdlGlShaderEngine&) = delete;

    /// Initializes OpenGL entry points and compiles the Sonar and Spectrogram GLSL shaders.
    /// Returns true if the active SDL_Renderer is backed by OpenGL / OpenGL ES / WebGL and shaders compiled cleanly.
    bool Initialize(SDL_Renderer* renderer);

    /// Releases all GPU textures, buffers, and shader programs.
    void Shutdown();

    /// Returns true if the GPU shader pipeline is initialized and operational.
    [[nodiscard]] bool IsAvailable() const noexcept { return m_available; }

    /// Returns a human-readable description of the active GPU shader backend.
    [[nodiscard]] std::string_view GetBackendSummary() const noexcept { return m_backendSummary; }

    /// Renders all active Sonar and Spectrogram GPU passes registered on `canvas` into the current OpenGL backbuffer.
    /// Must be called after `SDL_RenderClear(renderer)` and before compositing the UI overlay texture.
    void RenderUnderlays(const ThorCanvas& canvas, uint32_t viewportWidth, uint32_t viewportHeight);

private:
    struct SonarTextureCache {
        uint32_t dataTex{0};
        uint32_t lutTex{0};
        uint32_t width{0};
        uint32_t height{0};
        uint64_t uploadedGeneration{0};
        uint8_t uploadedColormapId{255};
        uint64_t lastUsedFrame{0};
    };

    struct SpectrogramTextureCache {
        uint32_t dataTex{0};
        uint32_t lutTex{0};
        uint32_t width{0};
        uint32_t height{0};
        uint64_t uploadedGeneration{0};
        uint8_t uploadedColormapId{255};
        uint64_t lastUsedFrame{0};
    };

    struct WaterfallTextureCache {
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

    void RenderSonarPass(const ICanvas::SonarGpuFieldDesc& pass, uint32_t viewportW, uint32_t viewportH);
    void RenderSpectrogramPass(const ICanvas::SpectrogramGpuFieldDesc& pass, uint32_t viewportW, uint32_t viewportH);
    void RenderWaterfallPass(const ICanvas::WaterfallGpuFieldDesc& pass, uint32_t viewportW, uint32_t viewportH);
    void PruneStaleTextures();

    SDL_Renderer* m_renderer{nullptr};
    bool m_available{false};
    std::string m_backendSummary{"Disabled"};
    uint64_t m_frameCounter{0};

    uint32_t m_quadVbo{0};

    // Sonar Polar/Sector/B-Scan shader program & uniform locations
    uint32_t m_sonarProgram{0};
    int m_uSonarViewport{-1};
    int m_uSonarPlotRect{-1};
    int m_uSonarOrigin{-1};
    int m_uSonarMaxRadiusPx{-1};
    int m_uSonarRangeParams{-1};   // (minRange, visibleMaxRange, matrixMaxRange, displayMode)
    int m_uSonarBearingParams{-1}; // (matrixStartBrg, matrixEndBrg, headingDeg, sectorApertureDeg)
    int m_uSonarBScanRect{-1};     // (x, y, w, h)
    int m_uSonarDspParams{-1};     // (gainLinear, tvgAmount, clutterThreshold, 0)
    int m_uSonarGridDims{-1};      // (numRangeBins, numBeams)
    int m_uSonarBgColor{-1};
    int m_uSonarPolarTex{-1};
    int m_uSonarLutTex{-1};

    // STFT Spectrogram shader program & uniform locations
    uint32_t m_specProgram{0};
    int m_uSpecViewport{-1};
    int m_uSpecPlotRect{-1};
    int m_uSpecDbParams{-1};       // (dbFloor, dbCeil)
    int m_uSpecGridDims{-1};       // (texWidth, texHeight)
    int m_uSpecBgColor{-1};
    int m_uSpecDbTex{-1};
    int m_uSpecLutTex{-1};

    // Circular Ring-Buffer Waterfall (RTSA Spectrum & DOA/BTR) shader program & uniform locations
    uint32_t m_waterfallProgram{0};
    int m_uWfViewport{-1};
    int m_uWfPlotRect{-1};
    int m_uWfDbParams{-1};         // (dbFloor, dbCeil)
    int m_uWfDspParams{-1};        // (gainDb, thresholdGateDb, gateSuppression, gamma)
    int m_uWfGridDims{-1};         // (texWidth, texHeight)
    int m_uWfRingParams{-1};       // (headRow, validRows, invertTimeAxis, 0)
    int m_uWfXViewNorm{-1};        // (xMinNorm, xMaxNorm)
    int m_uWfBgColor{-1};
    int m_uWfDbTex{-1};
    int m_uWfLutTex{-1};

    std::unordered_map<uint64_t, SonarTextureCache> m_sonarCaches;
    std::unordered_map<uint64_t, SpectrogramTextureCache> m_specCaches;
    std::unordered_map<uint64_t, WaterfallTextureCache> m_waterfallCaches;
    std::vector<uint32_t> m_uploadScratch;
};

} // namespace guikit
