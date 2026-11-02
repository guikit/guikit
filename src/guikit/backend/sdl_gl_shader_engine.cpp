#include "guikit/backend/sdl_gl_shader_engine.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#ifdef _WIN32
#define GUIKIT_GL_APIENTRY __stdcall
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#else
#define GUIKIT_GL_APIENTRY
#endif

namespace guikit {

namespace {

// Standard OpenGL 2.0 / OpenGL ES 2.0 constants
constexpr uint32_t kGL_FALSE                        = 0;
constexpr uint32_t kGL_TRIANGLES                    = 0x0004;
constexpr uint32_t kGL_SCISSOR_TEST                 = 0x0C11;
constexpr uint32_t kGL_SCISSOR_BOX                  = 0x0C10;
constexpr uint32_t kGL_VIEWPORT                     = 0x0BA2;
constexpr uint32_t kGL_BLEND                        = 0x0BE2;
constexpr uint32_t kGL_UNPACK_ROW_LENGTH            = 0x0CF2;
constexpr uint32_t kGL_UNPACK_SKIP_ROWS             = 0x0CF3;
constexpr uint32_t kGL_UNPACK_SKIP_PIXELS           = 0x0CF4;
constexpr uint32_t kGL_UNPACK_ALIGNMENT             = 0x0CF5;
constexpr uint32_t kGL_TEXTURE_2D                   = 0x0DE1;
constexpr uint32_t kGL_TEXTURE_BINDING_2D           = 0x8069;
constexpr uint32_t kGL_UNSIGNED_BYTE                = 0x1401;
constexpr uint32_t kGL_FLOAT                        = 0x1406;
constexpr uint32_t kGL_RGBA                         = 0x1908;
constexpr uint32_t kGL_NEAREST                      = 0x2600;
constexpr uint32_t kGL_LINEAR                       = 0x2601;
constexpr uint32_t kGL_TEXTURE_MAG_FILTER           = 0x2800;
constexpr uint32_t kGL_TEXTURE_MIN_FILTER           = 0x2801;
constexpr uint32_t kGL_TEXTURE_WRAP_S               = 0x2802;
constexpr uint32_t kGL_TEXTURE_WRAP_T               = 0x2803;
constexpr uint32_t kGL_CLAMP_TO_EDGE                = 0x812F;
constexpr uint32_t kGL_TEXTURE0                     = 0x84C0;
constexpr uint32_t kGL_TEXTURE1                     = 0x84C1;
constexpr uint32_t kGL_ACTIVE_TEXTURE               = 0x84E0;
constexpr uint32_t kGL_ARRAY_BUFFER                 = 0x8892;
constexpr uint32_t kGL_ARRAY_BUFFER_BINDING         = 0x8894;
constexpr uint32_t kGL_STATIC_DRAW                  = 0x88E4;
constexpr uint32_t kGL_FRAGMENT_SHADER              = 0x8B30;
constexpr uint32_t kGL_VERTEX_SHADER                = 0x8B31;
constexpr uint32_t kGL_COMPILE_STATUS               = 0x8B81;
constexpr uint32_t kGL_LINK_STATUS                  = 0x8B82;
constexpr uint32_t kGL_CURRENT_PROGRAM              = 0x8B8D;
constexpr uint32_t kGL_VERTEX_ATTRIB_ARRAY_ENABLED  = 0x8622;

// Dynamically resolved OpenGL 2.0 / ES 2.0 function table
struct GlProcs {
    void     (GUIKIT_GL_APIENTRY* GetIntegerv)(uint32_t pname, int* data) = nullptr;
    uint8_t  (GUIKIT_GL_APIENTRY* IsEnabled)(uint32_t cap) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Enable)(uint32_t cap) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Disable)(uint32_t cap) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Viewport)(int x, int y, int width, int height) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Scissor)(int x, int y, int width, int height) = nullptr;
    void     (GUIKIT_GL_APIENTRY* PixelStorei)(uint32_t pname, int param) = nullptr;
    void     (GUIKIT_GL_APIENTRY* ActiveTexture)(uint32_t texture) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GenTextures)(int n, uint32_t* textures) = nullptr;
    void     (GUIKIT_GL_APIENTRY* DeleteTextures)(int n, const uint32_t* textures) = nullptr;
    void     (GUIKIT_GL_APIENTRY* BindTexture)(uint32_t target, uint32_t texture) = nullptr;
    void     (GUIKIT_GL_APIENTRY* TexParameteri)(uint32_t target, uint32_t pname, int param) = nullptr;
    void     (GUIKIT_GL_APIENTRY* TexImage2D)(uint32_t target, int level, int internalformat, int width, int height, int border, uint32_t format, uint32_t type, const void* pixels) = nullptr;
    void     (GUIKIT_GL_APIENTRY* TexSubImage2D)(uint32_t target, int level, int xoffset, int yoffset, int width, int height, uint32_t format, uint32_t type, const void* pixels) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GenBuffers)(int n, uint32_t* buffers) = nullptr;
    void     (GUIKIT_GL_APIENTRY* DeleteBuffers)(int n, const uint32_t* buffers) = nullptr;
    void     (GUIKIT_GL_APIENTRY* BindBuffer)(uint32_t target, uint32_t buffer) = nullptr;
    void     (GUIKIT_GL_APIENTRY* BufferData)(uint32_t target, ptrdiff_t size, const void* data, uint32_t usage) = nullptr;
    uint32_t (GUIKIT_GL_APIENTRY* CreateShader)(uint32_t type) = nullptr;
    void     (GUIKIT_GL_APIENTRY* DeleteShader)(uint32_t shader) = nullptr;
    void     (GUIKIT_GL_APIENTRY* ShaderSource)(uint32_t shader, int count, const char* const* string, const int* length) = nullptr;
    void     (GUIKIT_GL_APIENTRY* CompileShader)(uint32_t shader) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GetShaderiv)(uint32_t shader, uint32_t pname, int* params) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GetShaderInfoLog)(uint32_t shader, int bufSize, int* length, char* infoLog) = nullptr;
    uint32_t (GUIKIT_GL_APIENTRY* CreateProgram)() = nullptr;
    void     (GUIKIT_GL_APIENTRY* DeleteProgram)(uint32_t program) = nullptr;
    void     (GUIKIT_GL_APIENTRY* AttachShader)(uint32_t program, uint32_t shader) = nullptr;
    void     (GUIKIT_GL_APIENTRY* BindAttribLocation)(uint32_t program, uint32_t index, const char* name) = nullptr;
    void     (GUIKIT_GL_APIENTRY* LinkProgram)(uint32_t program) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GetProgramiv)(uint32_t program, uint32_t pname, int* params) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GetProgramInfoLog)(uint32_t program, int bufSize, int* length, char* infoLog) = nullptr;
    void     (GUIKIT_GL_APIENTRY* UseProgram)(uint32_t program) = nullptr;
    int      (GUIKIT_GL_APIENTRY* GetUniformLocation)(uint32_t program, const char* name) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Uniform1i)(int location, int v0) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Uniform1f)(int location, float v0) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Uniform2f)(int location, float v0, float v1) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Uniform3f)(int location, float v0, float v1, float v2) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Uniform4f)(int location, float v0, float v1, float v2, float v3) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Uniform4fv)(int location, int count, const float* value) = nullptr;
    void     (GUIKIT_GL_APIENTRY* EnableVertexAttribArray)(uint32_t index) = nullptr;
    void     (GUIKIT_GL_APIENTRY* DisableVertexAttribArray)(uint32_t index) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GetVertexAttribiv)(uint32_t index, uint32_t pname, int* params) = nullptr;
    void     (GUIKIT_GL_APIENTRY* VertexAttribPointer)(uint32_t index, int size, uint32_t type, uint8_t normalized, int stride, const void* pointer) = nullptr;
    void     (GUIKIT_GL_APIENTRY* DrawArrays)(uint32_t mode, int first, int count) = nullptr;
};

GlProcs g_gl{};

template <typename FnPtr>
bool LoadProc(FnPtr& out, const char* name) noexcept {
    out = reinterpret_cast<FnPtr>(SDL_GL_GetProcAddress(name));
    return out != nullptr;
}

bool LoadAllGlProcs() noexcept {
    bool ok = true;
    ok &= LoadProc(g_gl.GetIntegerv, "glGetIntegerv");
    ok &= LoadProc(g_gl.IsEnabled, "glIsEnabled");
    ok &= LoadProc(g_gl.Enable, "glEnable");
    ok &= LoadProc(g_gl.Disable, "glDisable");
    ok &= LoadProc(g_gl.Viewport, "glViewport");
    ok &= LoadProc(g_gl.Scissor, "glScissor");
    ok &= LoadProc(g_gl.PixelStorei, "glPixelStorei");
    ok &= LoadProc(g_gl.ActiveTexture, "glActiveTexture");
    ok &= LoadProc(g_gl.GenTextures, "glGenTextures");
    ok &= LoadProc(g_gl.DeleteTextures, "glDeleteTextures");
    ok &= LoadProc(g_gl.BindTexture, "glBindTexture");
    ok &= LoadProc(g_gl.TexParameteri, "glTexParameteri");
    ok &= LoadProc(g_gl.TexImage2D, "glTexImage2D");
    ok &= LoadProc(g_gl.TexSubImage2D, "glTexSubImage2D");
    ok &= LoadProc(g_gl.GenBuffers, "glGenBuffers");
    ok &= LoadProc(g_gl.DeleteBuffers, "glDeleteBuffers");
    ok &= LoadProc(g_gl.BindBuffer, "glBindBuffer");
    ok &= LoadProc(g_gl.BufferData, "glBufferData");
    ok &= LoadProc(g_gl.CreateShader, "glCreateShader");
    ok &= LoadProc(g_gl.DeleteShader, "glDeleteShader");
    ok &= LoadProc(g_gl.ShaderSource, "glShaderSource");
    ok &= LoadProc(g_gl.CompileShader, "glCompileShader");
    ok &= LoadProc(g_gl.GetShaderiv, "glGetShaderiv");
    ok &= LoadProc(g_gl.GetShaderInfoLog, "glGetShaderInfoLog");
    ok &= LoadProc(g_gl.CreateProgram, "glCreateProgram");
    ok &= LoadProc(g_gl.DeleteProgram, "glDeleteProgram");
    ok &= LoadProc(g_gl.AttachShader, "glAttachShader");
    ok &= LoadProc(g_gl.BindAttribLocation, "glBindAttribLocation");
    ok &= LoadProc(g_gl.LinkProgram, "glLinkProgram");
    ok &= LoadProc(g_gl.GetProgramiv, "glGetProgramiv");
    ok &= LoadProc(g_gl.GetProgramInfoLog, "glGetProgramInfoLog");
    ok &= LoadProc(g_gl.UseProgram, "glUseProgram");
    ok &= LoadProc(g_gl.GetUniformLocation, "glGetUniformLocation");
    ok &= LoadProc(g_gl.Uniform1i, "glUniform1i");
    ok &= LoadProc(g_gl.Uniform1f, "glUniform1f");
    ok &= LoadProc(g_gl.Uniform2f, "glUniform2f");
    ok &= LoadProc(g_gl.Uniform3f, "glUniform3f");
    ok &= LoadProc(g_gl.Uniform4f, "glUniform4f");
    ok &= LoadProc(g_gl.Uniform4fv, "glUniform4fv");
    ok &= LoadProc(g_gl.EnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= LoadProc(g_gl.DisableVertexAttribArray, "glDisableVertexAttribArray");
    ok &= LoadProc(g_gl.GetVertexAttribiv, "glGetVertexAttribiv");
    ok &= LoadProc(g_gl.VertexAttribPointer, "glVertexAttribPointer");
    ok &= LoadProc(g_gl.DrawArrays, "glDrawArrays");
    return ok;
}

uint32_t CompileGlShader(uint32_t type, std::string_view src) noexcept {
    uint32_t shader = g_gl.CreateShader(type);
    if (!shader) return 0;
    const char* ptr = src.data();
    const int len = static_cast<int>(src.size());
    g_gl.ShaderSource(shader, 1, &ptr, &len);
    g_gl.CompileShader(shader);
    int compiled = 0;
    g_gl.GetShaderiv(shader, kGL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char logBuf[512]{};
        g_gl.GetShaderInfoLog(shader, sizeof(logBuf) - 1, nullptr, logBuf);
        SDL_Log("SdlGlShaderEngine shader compile error: %s", logBuf);
        g_gl.DeleteShader(shader);
        return 0;
    }
    return shader;
}

uint32_t BuildGlProgram(std::string_view vsSrc, std::string_view fsSrc) noexcept {
    uint32_t vs = CompileGlShader(kGL_VERTEX_SHADER, vsSrc);
    if (!vs) return 0;
    uint32_t fs = CompileGlShader(kGL_FRAGMENT_SHADER, fsSrc);
    if (!fs) {
        g_gl.DeleteShader(vs);
        return 0;
    }

    uint32_t prog = g_gl.CreateProgram();
    g_gl.AttachShader(prog, vs);
    g_gl.AttachShader(prog, fs);
    g_gl.BindAttribLocation(prog, 0, "a_uv");
    g_gl.LinkProgram(prog);

    g_gl.DeleteShader(vs);
    g_gl.DeleteShader(fs);

    int linked = 0;
    g_gl.GetProgramiv(prog, kGL_LINK_STATUS, &linked);
    if (!linked) {
        char logBuf[512]{};
        g_gl.GetProgramInfoLog(prog, sizeof(logBuf) - 1, nullptr, logBuf);
        SDL_Log("SdlGlShaderEngine program link error: %s", logBuf);
        g_gl.DeleteProgram(prog);
        return 0;
    }
    return prog;
}

// Shared Vertex Shader: maps unit quad [0..1]x[0..1] to u_PlotRect in top-left screen coordinates
constexpr std::string_view kSharedVertSrc = R"(
#ifdef GL_ES
precision highp float;
#endif
attribute vec2 a_uv;
varying vec2 v_uv;
uniform vec2 u_Viewport;
uniform vec4 u_PlotRect;

void main() {
    v_uv = a_uv;
    vec2 screenPos = u_PlotRect.xy + a_uv * u_PlotRect.zw;
    vec2 ndc = vec2(
        (screenPos.x / u_Viewport.x) * 2.0 - 1.0,
        1.0 - (screenPos.y / u_Viewport.y) * 2.0
    );
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

// Lightweight probe fragment shader to validate GLSL compilation on initialization
constexpr std::string_view kProbeFragSrc = R"(
#ifdef GL_ES
precision highp float;
#endif
varying vec2 v_uv;
void main() {
    gl_FragColor = vec4(v_uv, 0.0, 1.0);
}
)";

[[nodiscard]] inline uint32_t PackRG16Norm(float normVal) noexcept {
    const float clamped = std::clamp(normVal, 0.0f, 1.0f);
    const uint32_t q = static_cast<uint32_t>(std::lround(clamped * 65535.0f));
    const uint32_t hi = (q >> 8) & 0xFFu;
    const uint32_t lo = q & 0xFFu;
    return hi | (lo << 8) | 0xFF000000u;
}

} // namespace

SdlGlShaderEngine::~SdlGlShaderEngine() {
    Shutdown();
}

bool SdlGlShaderEngine::Initialize(SDL_Renderer* renderer) {
    Shutdown();
    if (!renderer) {
        return false;
    }

    const char* rendererName = SDL_GetRendererName(renderer);
    if (!rendererName) {
        return false;
    }

    const std::string_view nameView(rendererName);
    const bool isGlBackend =
        (nameView.find("opengl") != std::string_view::npos) ||
        (nameView.find("OpenGL") != std::string_view::npos) ||
        (nameView.find("webgl") != std::string_view::npos);
    if (!isGlBackend) {
        m_backendSummary = std::string("Unavailable (SDL driver: ") + rendererName + ")";
        return false;
    }

    SDL_FlushRenderer(renderer);

    if (!LoadAllGlProcs()) {
        m_backendSummary = "Unavailable (GL proc load failed)";
        return false;
    }

    // Validate GLSL compiler and linker with a minimal probe shader
    const uint32_t probeProgram = BuildGlProgram(kSharedVertSrc, kProbeFragSrc);
    if (!probeProgram) {
        m_backendSummary = "Unavailable (GLSL compilation failed)";
        return false;
    }
    g_gl.DeleteProgram(probeProgram);

    // Create static unit quad VBO (2 triangles covering [0..1]x[0..1])
    constexpr float kQuadVerts[12] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f
    };

    int prevVbo = 0;
    g_gl.GetIntegerv(kGL_ARRAY_BUFFER_BINDING, &prevVbo);
    g_gl.GenBuffers(1, &m_quadVbo);
    g_gl.BindBuffer(kGL_ARRAY_BUFFER, m_quadVbo);
    g_gl.BufferData(kGL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, kGL_STATIC_DRAW);
    g_gl.BindBuffer(kGL_ARRAY_BUFFER, static_cast<uint32_t>(prevVbo));

    SDL_FlushRenderer(renderer);

    m_renderer = renderer;
    m_available = true;
    m_backendSummary = std::string("Active (SDL ") + rendererName + " GLSL)";
    return true;
}

void SdlGlShaderEngine::Shutdown() {
    if (m_available) {
        for (auto& [_, entry] : m_textureCaches) {
            if (entry.dataTex) g_gl.DeleteTextures(1, &entry.dataTex);
            if (entry.lutTex) g_gl.DeleteTextures(1, &entry.lutTex);
        }
        for (auto& [_, prog] : m_programs) {
            if (prog.program) g_gl.DeleteProgram(prog.program);
        }
        if (m_quadVbo) {
            g_gl.DeleteBuffers(1, &m_quadVbo);
            m_quadVbo = 0;
        }
    }
    m_textureCaches.clear();
    m_programs.clear();
    m_renderer = nullptr;
    m_available = false;
    m_backendSummary = "Disabled";
}

const SdlGlShaderEngine::CompiledProgram* SdlGlShaderEngine::GetOrCompileProgram(const ICanvas::GpuShaderSource& src) {
    if (src.shaderId.empty() || src.glslEs2.empty()) {
        return nullptr;
    }

    const std::string key(src.shaderId);
    if (auto it = m_programs.find(key); it != m_programs.end()) {
        return (it->second.program != 0) ? &it->second : nullptr;
    }

    CompiledProgram compiled{};
    compiled.program = BuildGlProgram(kSharedVertSrc, src.glslEs2);
    if (compiled.program != 0) {
        compiled.uViewport = g_gl.GetUniformLocation(compiled.program, "u_Viewport");
        compiled.uPlotRect = g_gl.GetUniformLocation(compiled.program, "u_PlotRect");
        compiled.uGridDims = g_gl.GetUniformLocation(compiled.program, "u_GridDims");
        compiled.uBgColor  = g_gl.GetUniformLocation(compiled.program, "u_BgColor");
        compiled.uParams   = g_gl.GetUniformLocation(compiled.program, "u_Params");
        if (compiled.uParams < 0) {
            compiled.uParams = g_gl.GetUniformLocation(compiled.program, "u_Params[0]");
        }
        compiled.uDataTex  = g_gl.GetUniformLocation(compiled.program, "u_DataTex");
        compiled.uLutTex   = g_gl.GetUniformLocation(compiled.program, "u_LutTex");
    }

    auto [insIt, _] = m_programs.emplace(key, compiled);
    return (insIt->second.program != 0) ? &insIt->second : nullptr;
}

void SdlGlShaderEngine::PruneStaleTextures() {
    constexpr uint64_t kMaxIdleFrames = 300;
    std::erase_if(m_textureCaches, [&](auto& kv) {
        if (m_frameCounter - kv.second.lastUsedFrame > kMaxIdleFrames) {
            if (kv.second.dataTex) g_gl.DeleteTextures(1, &kv.second.dataTex);
            if (kv.second.lutTex) g_gl.DeleteTextures(1, &kv.second.lutTex);
            return true;
        }
        return false;
    });
}

void SdlGlShaderEngine::RenderPass(
    const ICanvas::GpuShaderPassDesc& pass,
    uint32_t viewportW,
    uint32_t viewportH
) {
    if (pass.clipRect.w <= 0.5f || pass.clipRect.h <= 0.5f ||
        pass.texWidth == 0 || pass.texHeight == 0 || !pass.dataFloats) {
        return;
    }

    const CompiledProgram* prog = GetOrCompileProgram(pass.shader);
    if (!prog) {
        return;
    }

    auto& cache = m_textureCaches[pass.widgetId];
    cache.lastUsedFrame = m_frameCounter;

    // 1. Ensure 256x1 Colormap LUT texture is created & up to date
    if (pass.lutAbgr256 != nullptr) {
        g_gl.ActiveTexture(kGL_TEXTURE1);
        if (!cache.lutTex) {
            g_gl.GenTextures(1, &cache.lutTex);
            g_gl.BindTexture(kGL_TEXTURE_2D, cache.lutTex);
            g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MIN_FILTER, static_cast<int>(kGL_LINEAR));
            g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MAG_FILTER, static_cast<int>(kGL_LINEAR));
            g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_S, static_cast<int>(kGL_CLAMP_TO_EDGE));
            g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_T, static_cast<int>(kGL_CLAMP_TO_EDGE));
            g_gl.TexImage2D(kGL_TEXTURE_2D, 0, static_cast<int>(kGL_RGBA), 256, 1, 0, kGL_RGBA, kGL_UNSIGNED_BYTE, pass.lutAbgr256);
            cache.uploadedColormapId = pass.colormapId;
        } else {
            g_gl.BindTexture(kGL_TEXTURE_2D, cache.lutTex);
            if (cache.uploadedColormapId != pass.colormapId) {
                g_gl.TexSubImage2D(kGL_TEXTURE_2D, 0, 0, 0, 256, 1, kGL_RGBA, kGL_UNSIGNED_BYTE, pass.lutAbgr256);
                cache.uploadedColormapId = pass.colormapId;
            }
        }
    }

    // 2. Ensure 2D Data Texture (texWidth x texHeight) is up to date (supports full upload & O(1) ring-buffer row upload)
    g_gl.ActiveTexture(kGL_TEXTURE0);
    const bool dimsChanged = (!cache.dataTex || cache.width != pass.texWidth || cache.height != pass.texHeight);
    if (dimsChanged) {
        if (!cache.dataTex) {
            g_gl.GenTextures(1, &cache.dataTex);
        }
        cache.width = pass.texWidth;
        cache.height = pass.texHeight;
        cache.uploadedFullGen = 0;
        cache.uploadedSliceGen = 0;

        g_gl.BindTexture(kGL_TEXTURE_2D, cache.dataTex);
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MIN_FILTER, static_cast<int>(kGL_NEAREST));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MAG_FILTER, static_cast<int>(kGL_NEAREST));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_S, static_cast<int>(kGL_CLAMP_TO_EDGE));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_T, static_cast<int>(kGL_CLAMP_TO_EDGE));
    } else {
        g_gl.BindTexture(kGL_TEXTURE_2D, cache.dataTex);
    }

    const float invNormScale = 1.0f / std::max(1e-6f, pass.dataNormScale);
    const bool needFullUpload = dimsChanged || (cache.uploadedFullGen != pass.fullGeneration);
    const uint64_t deltaSlices = (pass.sliceGeneration > 0 && pass.sliceGeneration >= cache.uploadedSliceGen)
        ? (pass.sliceGeneration - cache.uploadedSliceGen)
        : 0;

    if (needFullUpload || (pass.sliceGeneration > 0 && (pass.sliceGeneration < cache.uploadedSliceGen || deltaSlices >= pass.texHeight))) {
        const size_t total = static_cast<size_t>(pass.texWidth) * pass.texHeight;
        m_uploadScratch.resize(total);
        for (size_t i = 0; i < total; ++i) {
            const float normVal = (pass.dataFloats[i] - pass.dataNormOffset) * invNormScale;
            m_uploadScratch[i] = PackRG16Norm(normVal);
        }
        if (dimsChanged) {
            g_gl.TexImage2D(kGL_TEXTURE_2D, 0, static_cast<int>(kGL_RGBA),
                            static_cast<int>(pass.texWidth), static_cast<int>(pass.texHeight),
                            0, kGL_RGBA, kGL_UNSIGNED_BYTE, m_uploadScratch.data());
        } else {
            g_gl.TexSubImage2D(kGL_TEXTURE_2D, 0, 0, 0,
                               static_cast<int>(pass.texWidth), static_cast<int>(pass.texHeight),
                               kGL_RGBA, kGL_UNSIGNED_BYTE, m_uploadScratch.data());
        }
        cache.uploadedFullGen = pass.fullGeneration;
        cache.uploadedSliceGen = pass.sliceGeneration;
        cache.uploadedHeadRow = pass.headRow;
    } else if (deltaSlices > 0) {
        // O(1) Circular Ring-Buffer Update: upload only the newly written row(s) via glTexSubImage2D
        m_uploadScratch.resize(pass.texWidth);
        for (uint64_t s = 0; s < deltaSlices; ++s) {
            const uint32_t rowIdx = static_cast<uint32_t>(
                (static_cast<uint64_t>(pass.headRow) + static_cast<uint64_t>(pass.texHeight) * 4ULL - s) % pass.texHeight
            );
            const float* rowSrc = pass.dataFloats + static_cast<size_t>(rowIdx) * pass.texWidth;
            for (uint32_t x = 0; x < pass.texWidth; ++x) {
                const float normVal = (rowSrc[x] - pass.dataNormOffset) * invNormScale;
                m_uploadScratch[x] = PackRG16Norm(normVal);
            }
            g_gl.TexSubImage2D(kGL_TEXTURE_2D, 0, 0, static_cast<int>(rowIdx),
                               static_cast<int>(pass.texWidth), 1,
                               kGL_RGBA, kGL_UNSIGNED_BYTE, m_uploadScratch.data());
        }
        cache.uploadedSliceGen = pass.sliceGeneration;
        cache.uploadedHeadRow = pass.headRow;
    }

    // 3. Configure Scissor Box (converting top-left UI coords to bottom-left GL coords)
    const int scX = std::max(0, static_cast<int>(std::floor(pass.clipRect.x)));
    const int scTop = std::max(0, static_cast<int>(std::floor(pass.clipRect.y)));
    const int scRight = std::min(static_cast<int>(viewportW), static_cast<int>(std::ceil(pass.clipRect.Right())));
    const int scBottom = std::min(static_cast<int>(viewportH), static_cast<int>(std::ceil(pass.clipRect.Bottom())));
    const int scW = scRight - scX;
    const int scH = scBottom - scTop;
    if (scW <= 0 || scH <= 0) return;

    const int glScY = static_cast<int>(viewportH) - scBottom;
    g_gl.Scissor(scX, glScY, scW, scH);

    // 4. Bind Shader Program & Standard Uniforms
    g_gl.UseProgram(prog->program);
    if (prog->uViewport >= 0) {
        g_gl.Uniform2f(prog->uViewport, static_cast<float>(viewportW), static_cast<float>(viewportH));
    }
    if (prog->uPlotRect >= 0) {
        g_gl.Uniform4f(prog->uPlotRect, pass.plotRect.x, pass.plotRect.y, pass.plotRect.w, pass.plotRect.h);
    }
    if (prog->uGridDims >= 0) {
        g_gl.Uniform2f(prog->uGridDims, static_cast<float>(pass.texWidth), static_cast<float>(pass.texHeight));
    }
    if (prog->uBgColor >= 0) {
        g_gl.Uniform3f(prog->uBgColor,
                       static_cast<float>(pass.bgColor.r) / 255.0f,
                       static_cast<float>(pass.bgColor.g) / 255.0f,
                       static_cast<float>(pass.bgColor.b) / 255.0f);
    }
    if (prog->uParams >= 0) {
        g_gl.Uniform4fv(prog->uParams, static_cast<int>(pass.params.size()), &pass.params[0].x);
    }
    if (prog->uDataTex >= 0) {
        g_gl.Uniform1i(prog->uDataTex, 0);
    }
    if (prog->uLutTex >= 0) {
        g_gl.Uniform1i(prog->uLutTex, 1);
    }

    g_gl.DrawArrays(kGL_TRIANGLES, 0, 6);
}

void SdlGlShaderEngine::RenderUnderlays(
    const ICanvas& canvas,
    uint32_t viewportWidth,
    uint32_t viewportHeight
) {
    if (!m_available || !m_renderer || viewportWidth == 0 || viewportHeight == 0 || !canvas.HasActiveGpuPasses()) {
        return;
    }

    m_frameCounter++;

    SDL_FlushRenderer(m_renderer);

    // Save current OpenGL state
    int prevProgram = 0;
    int prevVbo = 0;
    int prevActiveTex = 0;
    int prevTex0 = 0;
    int prevTex1 = 0;
    int prevViewport[4] = {0, 0, 0, 0};
    int prevScissorBox[4] = {0, 0, 0, 0};
    int prevUnpackAlign = 4;
    int prevAttrEnabled[3] = {0, 0, 0};
#ifndef __EMSCRIPTEN__
    int prevUnpackRowLength = 0;
    int prevUnpackSkipRows = 0;
    int prevUnpackSkipPixels = 0;
#endif
    const uint8_t prevScissorEnabled = g_gl.IsEnabled(kGL_SCISSOR_TEST);
    const uint8_t prevBlendEnabled = g_gl.IsEnabled(kGL_BLEND);

    g_gl.GetIntegerv(kGL_CURRENT_PROGRAM, &prevProgram);
    g_gl.GetIntegerv(kGL_ARRAY_BUFFER_BINDING, &prevVbo);
    g_gl.GetIntegerv(kGL_ACTIVE_TEXTURE, &prevActiveTex);
    g_gl.GetIntegerv(kGL_VIEWPORT, prevViewport);
    g_gl.GetIntegerv(kGL_SCISSOR_BOX, prevScissorBox);
    g_gl.GetIntegerv(kGL_UNPACK_ALIGNMENT, &prevUnpackAlign);
    for (uint32_t i = 0; i < 3; ++i) {
        g_gl.GetVertexAttribiv(i, kGL_VERTEX_ATTRIB_ARRAY_ENABLED, &prevAttrEnabled[i]);
    }
#ifndef __EMSCRIPTEN__
    g_gl.GetIntegerv(kGL_UNPACK_ROW_LENGTH, &prevUnpackRowLength);
    g_gl.GetIntegerv(kGL_UNPACK_SKIP_ROWS, &prevUnpackSkipRows);
    g_gl.GetIntegerv(kGL_UNPACK_SKIP_PIXELS, &prevUnpackSkipPixels);
#endif

    g_gl.ActiveTexture(kGL_TEXTURE0);
    g_gl.GetIntegerv(kGL_TEXTURE_BINDING_2D, &prevTex0);
    g_gl.ActiveTexture(kGL_TEXTURE1);
    g_gl.GetIntegerv(kGL_TEXTURE_BINDING_2D, &prevTex1);

    // Set state for GPU underlay shader passes
    g_gl.Viewport(0, 0, static_cast<int>(viewportWidth), static_cast<int>(viewportHeight));
    g_gl.Enable(kGL_SCISSOR_TEST);
    g_gl.Disable(kGL_BLEND);
    g_gl.PixelStorei(kGL_UNPACK_ALIGNMENT, 4);
#ifndef __EMSCRIPTEN__
    g_gl.PixelStorei(kGL_UNPACK_ROW_LENGTH, 0);
    g_gl.PixelStorei(kGL_UNPACK_SKIP_ROWS, 0);
    g_gl.PixelStorei(kGL_UNPACK_SKIP_PIXELS, 0);
#endif

    g_gl.BindBuffer(kGL_ARRAY_BUFFER, m_quadVbo);
    g_gl.EnableVertexAttribArray(0);
    g_gl.DisableVertexAttribArray(1);
    g_gl.DisableVertexAttribArray(2);
    g_gl.VertexAttribPointer(0, 2, kGL_FLOAT, kGL_FALSE, 2 * sizeof(float), nullptr);

    for (const auto& pass : canvas.GetGpuShaderPasses()) {
        RenderPass(pass, viewportWidth, viewportHeight);
    }

    if ((m_frameCounter & 63u) == 0u) {
        PruneStaleTextures();
    }

    // Restore OpenGL state cleanly so SDL_Renderer (including opengles2/WebGL) can composite the UI layer without interference
    for (uint32_t i = 0; i < 3; ++i) {
        if (prevAttrEnabled[i]) {
            g_gl.EnableVertexAttribArray(i);
        } else {
            g_gl.DisableVertexAttribArray(i);
        }
    }
    g_gl.BindBuffer(kGL_ARRAY_BUFFER, static_cast<uint32_t>(prevVbo));
    g_gl.UseProgram(static_cast<uint32_t>(prevProgram));

    g_gl.ActiveTexture(kGL_TEXTURE1);
    g_gl.BindTexture(kGL_TEXTURE_2D, static_cast<uint32_t>(prevTex1));
    g_gl.ActiveTexture(kGL_TEXTURE0);
    g_gl.BindTexture(kGL_TEXTURE_2D, static_cast<uint32_t>(prevTex0));
    g_gl.ActiveTexture(static_cast<uint32_t>(prevActiveTex));

    g_gl.PixelStorei(kGL_UNPACK_ALIGNMENT, prevUnpackAlign);
#ifndef __EMSCRIPTEN__
    g_gl.PixelStorei(kGL_UNPACK_ROW_LENGTH, prevUnpackRowLength);
    g_gl.PixelStorei(kGL_UNPACK_SKIP_ROWS, prevUnpackSkipRows);
    g_gl.PixelStorei(kGL_UNPACK_SKIP_PIXELS, prevUnpackSkipPixels);
#endif
    g_gl.Viewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    g_gl.Scissor(prevScissorBox[0], prevScissorBox[1], prevScissorBox[2], prevScissorBox[3]);

    if (prevScissorEnabled) g_gl.Enable(kGL_SCISSOR_TEST);
    else g_gl.Disable(kGL_SCISSOR_TEST);

    if (prevBlendEnabled) g_gl.Enable(kGL_BLEND);
    else g_gl.Disable(kGL_BLEND);

    SDL_FlushRenderer(m_renderer);
}

} // namespace guikit
