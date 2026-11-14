#include "guikit/backend/sdl_gl_shader_engine.hpp"
#include "keel/small_array.h"
#include "keel/time.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <cstring>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

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
constexpr keel::u32 kGL_FALSE                        = 0;
constexpr keel::u32 kGL_TRIANGLES                    = 0x0004;
constexpr keel::u32 kGL_SCISSOR_TEST                 = 0x0C11;
constexpr keel::u32 kGL_SCISSOR_BOX                  = 0x0C10;
constexpr keel::u32 kGL_VIEWPORT                     = 0x0BA2;
constexpr keel::u32 kGL_BLEND                        = 0x0BE2;
constexpr keel::u32 kGL_UNPACK_ROW_LENGTH            = 0x0CF2;
constexpr keel::u32 kGL_UNPACK_SKIP_ROWS             = 0x0CF3;
constexpr keel::u32 kGL_UNPACK_SKIP_PIXELS           = 0x0CF4;
constexpr keel::u32 kGL_UNPACK_ALIGNMENT             = 0x0CF5;
constexpr keel::u32 kGL_TEXTURE_2D                   = 0x0DE1;
constexpr keel::u32 kGL_TEXTURE_BINDING_2D           = 0x8069;
constexpr keel::u32 kGL_UNSIGNED_BYTE                = 0x1401;
constexpr keel::u32 kGL_FLOAT                        = 0x1406;
constexpr keel::u32 kGL_RGBA                         = 0x1908;
constexpr keel::u32 kGL_NEAREST                      = 0x2600;
constexpr keel::u32 kGL_LINEAR                       = 0x2601;
constexpr keel::u32 kGL_TEXTURE_MAG_FILTER           = 0x2800;
constexpr keel::u32 kGL_TEXTURE_MIN_FILTER           = 0x2801;
constexpr keel::u32 kGL_TEXTURE_WRAP_S               = 0x2802;
constexpr keel::u32 kGL_TEXTURE_WRAP_T               = 0x2803;
constexpr keel::u32 kGL_CLAMP_TO_EDGE                = 0x812F;
constexpr keel::u32 kGL_TEXTURE0                     = 0x84C0;
constexpr keel::u32 kGL_TEXTURE1                     = 0x84C1;
constexpr keel::u32 kGL_ACTIVE_TEXTURE               = 0x84E0;
constexpr keel::u32 kGL_ARRAY_BUFFER                 = 0x8892;
constexpr keel::u32 kGL_ARRAY_BUFFER_BINDING         = 0x8894;
constexpr keel::u32 kGL_STATIC_DRAW                  = 0x88E4;
constexpr keel::u32 kGL_FRAGMENT_SHADER              = 0x8B30;
constexpr keel::u32 kGL_VERTEX_SHADER                = 0x8B31;
constexpr keel::u32 kGL_COMPILE_STATUS               = 0x8B81;
constexpr keel::u32 kGL_LINK_STATUS                  = 0x8B82;
constexpr keel::u32 kGL_CURRENT_PROGRAM              = 0x8B8D;
constexpr keel::u32 kGL_VERTEX_ATTRIB_ARRAY_ENABLED  = 0x8622;

// Dynamically resolved OpenGL 2.0 / ES 2.0 function table
struct GlProcs {
    void     (GUIKIT_GL_APIENTRY* GetIntegerv)(keel::u32 pname, keel::i32* data) = nullptr;
    keel::u8  (GUIKIT_GL_APIENTRY* IsEnabled)(keel::u32 cap) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Enable)(keel::u32 cap) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Disable)(keel::u32 cap) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Viewport)(keel::i32 x, keel::i32 y, keel::i32 width, keel::i32 height) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Scissor)(keel::i32 x, keel::i32 y, keel::i32 width, keel::i32 height) = nullptr;
    void     (GUIKIT_GL_APIENTRY* PixelStorei)(keel::u32 pname, keel::i32 param) = nullptr;
    void     (GUIKIT_GL_APIENTRY* ActiveTexture)(keel::u32 texture) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GenTextures)(keel::i32 n, keel::u32* textures) = nullptr;
    void     (GUIKIT_GL_APIENTRY* DeleteTextures)(keel::i32 n, const keel::u32* textures) = nullptr;
    void     (GUIKIT_GL_APIENTRY* BindTexture)(keel::u32 target, keel::u32 texture) = nullptr;
    void     (GUIKIT_GL_APIENTRY* TexParameteri)(keel::u32 target, keel::u32 pname, keel::i32 param) = nullptr;
    void     (GUIKIT_GL_APIENTRY* TexImage2D)(keel::u32 target, keel::i32 level, keel::i32 internalformat, keel::i32 width, keel::i32 height, keel::i32 border, keel::u32 format, keel::u32 type, const void* pixels) = nullptr;
    void     (GUIKIT_GL_APIENTRY* TexSubImage2D)(keel::u32 target, keel::i32 level, keel::i32 xoffset, keel::i32 yoffset, keel::i32 width, keel::i32 height, keel::u32 format, keel::u32 type, const void* pixels) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GenBuffers)(keel::i32 n, keel::u32* buffers) = nullptr;
    void     (GUIKIT_GL_APIENTRY* DeleteBuffers)(keel::i32 n, const keel::u32* buffers) = nullptr;
    void     (GUIKIT_GL_APIENTRY* BindBuffer)(keel::u32 target, keel::u32 buffer) = nullptr;
    void     (GUIKIT_GL_APIENTRY* BufferData)(keel::u32 target, keel::isize size, const void* data, keel::u32 usage) = nullptr;
    keel::u32 (GUIKIT_GL_APIENTRY* CreateShader)(keel::u32 type) = nullptr;
    void     (GUIKIT_GL_APIENTRY* DeleteShader)(keel::u32 shader) = nullptr;
    void     (GUIKIT_GL_APIENTRY* ShaderSource)(keel::u32 shader, keel::i32 count, const char* const* string, const keel::i32* length) = nullptr;
    void     (GUIKIT_GL_APIENTRY* CompileShader)(keel::u32 shader) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GetShaderiv)(keel::u32 shader, keel::u32 pname, keel::i32* params) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GetShaderInfoLog)(keel::u32 shader, keel::i32 bufSize, keel::i32* length, char* infoLog) = nullptr;
    keel::u32 (GUIKIT_GL_APIENTRY* CreateProgram)() = nullptr;
    void     (GUIKIT_GL_APIENTRY* DeleteProgram)(keel::u32 program) = nullptr;
    void     (GUIKIT_GL_APIENTRY* AttachShader)(keel::u32 program, keel::u32 shader) = nullptr;
    void     (GUIKIT_GL_APIENTRY* BindAttribLocation)(keel::u32 program, keel::u32 index, const char* name) = nullptr;
    void     (GUIKIT_GL_APIENTRY* LinkProgram)(keel::u32 program) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GetProgramiv)(keel::u32 program, keel::u32 pname, keel::i32* params) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GetProgramInfoLog)(keel::u32 program, keel::i32 bufSize, keel::i32* length, char* infoLog) = nullptr;
    void     (GUIKIT_GL_APIENTRY* UseProgram)(keel::u32 program) = nullptr;
    keel::i32      (GUIKIT_GL_APIENTRY* GetUniformLocation)(keel::u32 program, const char* name) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Uniform1i)(keel::i32 location, keel::i32 v0) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Uniform1f)(keel::i32 location, float v0) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Uniform2f)(keel::i32 location, float v0, float v1) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Uniform3f)(keel::i32 location, float v0, float v1, float v2) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Uniform4f)(keel::i32 location, float v0, float v1, float v2, float v3) = nullptr;
    void     (GUIKIT_GL_APIENTRY* Uniform4fv)(keel::i32 location, keel::i32 count, const float* value) = nullptr;
    void     (GUIKIT_GL_APIENTRY* EnableVertexAttribArray)(keel::u32 index) = nullptr;
    void     (GUIKIT_GL_APIENTRY* DisableVertexAttribArray)(keel::u32 index) = nullptr;
    void     (GUIKIT_GL_APIENTRY* GetVertexAttribiv)(keel::u32 index, keel::u32 pname, keel::i32* params) = nullptr;
    void     (GUIKIT_GL_APIENTRY* VertexAttribPointer)(keel::u32 index, keel::i32 size, keel::u32 type, keel::u8 normalized, keel::i32 stride, const void* pointer) = nullptr;
    void     (GUIKIT_GL_APIENTRY* DrawArrays)(keel::u32 mode, keel::i32 first, keel::i32 count) = nullptr;
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

keel::u32 CompileGlShader(keel::u32 type, keel::StringRef src) noexcept {
    keel::u32 shader = g_gl.CreateShader(type);
    if (!shader) return 0;
    const char* ptr = src.data();
    const keel::i32 len = static_cast<keel::i32>(src.size());
    g_gl.ShaderSource(shader, 1, &ptr, &len);
    g_gl.CompileShader(shader);
    keel::i32 compiled = 0;
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

keel::u32 BuildGlProgram(keel::StringRef vsSrc, keel::StringRef fsSrc) noexcept {
    keel::u32 vs = CompileGlShader(kGL_VERTEX_SHADER, vsSrc);
    if (!vs) return 0;
    keel::u32 fs = CompileGlShader(kGL_FRAGMENT_SHADER, fsSrc);
    if (!fs) {
        g_gl.DeleteShader(vs);
        return 0;
    }

    keel::u32 prog = g_gl.CreateProgram();
    g_gl.AttachShader(prog, vs);
    g_gl.AttachShader(prog, fs);
    g_gl.BindAttribLocation(prog, 0, "a_uv");
    g_gl.LinkProgram(prog);

    g_gl.DeleteShader(vs);
    g_gl.DeleteShader(fs);

    keel::i32 linked = 0;
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
constexpr keel::StringRef kSharedVertSrc = R"(
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
constexpr keel::StringRef kProbeFragSrc = R"(
#ifdef GL_ES
precision highp float;
#endif
varying vec2 v_uv;
void main() {
    gl_FragColor = vec4(v_uv, 0.0, 1.0);
}
)";

[[nodiscard]] inline keel::u32 PackRG16Norm(float normVal) noexcept {
    const float clamped = keel::clamp(normVal, 0.0f, 1.0f);
    const keel::u32 q = static_cast<keel::u32>(std::lround(clamped * 65535.0f));
    const keel::u32 hi = (q >> 8) & 0xFFu;
    const keel::u32 lo = q & 0xFFu;
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

    const keel::StringRef nameView(rendererName);
    const bool isGlBackend =
        nameView.contains("opengl") ||
        nameView.contains("OpenGL") ||
        nameView.contains("webgl");
    if (!isGlBackend) {
        m_backendSummary.assign("Unavailable (SDL driver: ");
        m_backendSummary.append(rendererName);
        m_backendSummary.append(")");
        return false;
    }

    SDL_FlushRenderer(renderer);

    if (!LoadAllGlProcs()) {
        m_backendSummary.assign("Unavailable (GL proc load failed)");
        return false;
    }

    // Validate GLSL compiler and linker with a minimal probe shader
    const keel::u32 probeProgram = BuildGlProgram(kSharedVertSrc, kProbeFragSrc);
    if (!probeProgram) {
        m_backendSummary.assign("Unavailable (GLSL compilation failed)");
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

    keel::i32 prevVbo = 0;
    g_gl.GetIntegerv(kGL_ARRAY_BUFFER_BINDING, &prevVbo);
    g_gl.GenBuffers(1, &m_quadVbo);
    g_gl.BindBuffer(kGL_ARRAY_BUFFER, m_quadVbo);
    g_gl.BufferData(kGL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, kGL_STATIC_DRAW);
    g_gl.BindBuffer(kGL_ARRAY_BUFFER, static_cast<keel::u32>(prevVbo));

    SDL_FlushRenderer(renderer);

    m_renderer = renderer;
    m_available = true;
    m_backendSummary.assign("Active (SDL ");
    m_backendSummary.append(rendererName);
    m_backendSummary.append(" GLSL)");
    return true;
}

void SdlGlShaderEngine::Shutdown() {
    if (m_available) {
        for (const auto& [_, entry] : m_textureCaches) {
            if (entry.dataTex) g_gl.DeleteTextures(1, &entry.dataTex);
            if (entry.lutTex) g_gl.DeleteTextures(1, &entry.lutTex);
        }
        for (const auto& [_, prog] : m_programs) {
            if (prog.program) g_gl.DeleteProgram(prog.program);
        }
        if (m_quadVbo) {
            g_gl.DeleteBuffers(1, &m_quadVbo);
            m_quadVbo = 0;
        }
    }
    m_textureCaches = {};
    m_programs = {};
    m_uploadScratch = {};
    m_renderer = nullptr;
    m_available = false;
    m_backendSummary.assign("Disabled");
}

const SdlGlShaderEngine::CompiledProgram* SdlGlShaderEngine::GetOrCompileProgram(const ICanvas::GpuShaderSource& src) {
    if (src.shaderId.empty() || src.glslEs2.empty()) {
        return nullptr;
    }

    if (const CompiledProgram* existing = m_programs.get(src.shaderId)) {
        return (existing->program != 0) ? existing : nullptr;
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

    const CompiledProgram& inserted = m_programs.get_or_insert(keel::String::from(src.shaderId), compiled);
    return (inserted.program != 0) ? &inserted : nullptr;
}

void SdlGlShaderEngine::ReleaseCachedTextures() {
    bool releasedMemory = false;
    if (m_available && m_renderer && !m_textureCaches.empty()) {
        SDL_FlushRenderer(m_renderer);
        for (const auto& [_, entry] : m_textureCaches) {
            if (entry.dataTex) g_gl.DeleteTextures(1, &entry.dataTex);
            if (entry.lutTex) g_gl.DeleteTextures(1, &entry.lutTex);
        }
        releasedMemory = true;
    }
    if (!m_textureCaches.empty() || m_uploadScratch.capacity() > 0u) {
        m_textureCaches = {};
        m_uploadScratch = {};
        releasedMemory = true;
    }
    if (releasedMemory) {
#if defined(__GLIBC__)
        ::malloc_trim(0);
#endif
    }
}

bool SdlGlShaderEngine::HasIdleCachedResources() const noexcept {
    if (!m_available) {
        return false;
    }
    for (const auto& [_, entry] : m_textureCaches) {
        if (entry.lastUsedFrame < m_frameCounter) {
            return true;
        }
    }
    return false;
}

void SdlGlShaderEngine::TickFrame(const ICanvas& canvas) {
    if (!m_available || !m_renderer) {
        return;
    }
    ++m_frameCounter;
    const keel::u64 nowNs = keel::monotonic_ns();
    if (canvas.HasActiveGpuPasses()) {
        for (const auto& pass : canvas.GetGpuShaderPasses()) {
            if (WidgetTextureCache* existing = m_textureCaches.get(pass.widgetId)) {
                existing->lastUsedFrame = m_frameCounter;
                existing->lastUsedTimeNs = nowNs;
            }
        }
    }
    if (m_textureCaches.empty() && m_uploadScratch.capacity() == 0u) {
        return;
    }
    if ((m_frameCounter & 31u) == 0u || (nowNs >= m_lastPruneTimeNs + 500'000'000ULL)) {
        m_lastPruneTimeNs = nowNs;
        PruneStaleTextures();
    }
}

void SdlGlShaderEngine::PruneStaleTextures() {
    constexpr keel::u64 kMaxIdleFrames = 120;
    constexpr keel::u64 kMaxIdleTimeNs = 1'500'000'000ULL;
    const keel::u64 nowNs = keel::monotonic_ns();
    keel::SmallArray<keel::u64, 16> staleIds;
    for (const auto& [id, entry] : m_textureCaches) {
        const keel::u64 idleFrames = (m_frameCounter > entry.lastUsedFrame)
            ? (m_frameCounter - entry.lastUsedFrame)
            : 0;
        const bool staleByFrames = idleFrames > kMaxIdleFrames;
        const bool staleByTime = (idleFrames > 2) &&
            (entry.lastUsedTimeNs > 0) &&
            (nowNs >= entry.lastUsedTimeNs + kMaxIdleTimeNs);
        if (staleByFrames || staleByTime) {
            staleIds.push_back(id);
        }
    }
    bool releasedMemory = !staleIds.empty();
    if (!staleIds.empty() && m_renderer) {
        SDL_FlushRenderer(m_renderer);
    }
    for (keel::u64 id : staleIds) {
        if (const WidgetTextureCache* entry = m_textureCaches.get(id)) {
            if (entry->dataTex) g_gl.DeleteTextures(1, &entry->dataTex);
            if (entry->lutTex) g_gl.DeleteTextures(1, &entry->lutTex);
        }
        (void)m_textureCaches.remove(id);
    }
    if (m_textureCaches.empty()) {
        if (m_uploadScratch.capacity() > 0u) {
            m_uploadScratch = {};
            releasedMemory = true;
        }
        if (releasedMemory) {
            m_textureCaches = {};
        }
    }
    if (releasedMemory) {
#if defined(__GLIBC__)
        ::malloc_trim(0);
#endif
    }
}

void SdlGlShaderEngine::RenderPass(
    const ICanvas::GpuShaderPassDesc& pass,
    keel::u32 viewportW,
    keel::u32 viewportH
) {
    if (pass.clipRect.w <= 0.5f || pass.clipRect.h <= 0.5f ||
        pass.texWidth == 0 || pass.texHeight == 0 || !pass.dataFloats) {
        return;
    }

    const CompiledProgram* prog = GetOrCompileProgram(pass.shader);
    if (!prog) {
        return;
    }

    auto& cache = m_textureCaches.get_or_insert(pass.widgetId, WidgetTextureCache{});
    cache.lastUsedFrame = m_frameCounter;
    cache.lastUsedTimeNs = keel::monotonic_ns();

    // 1. Ensure 256x1 Colormap LUT texture is created & up to date
    if (pass.lutAbgr256 != nullptr) {
        g_gl.ActiveTexture(kGL_TEXTURE1);
        if (!cache.lutTex) {
            g_gl.GenTextures(1, &cache.lutTex);
            g_gl.BindTexture(kGL_TEXTURE_2D, cache.lutTex);
            g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MIN_FILTER, static_cast<keel::i32>(kGL_LINEAR));
            g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MAG_FILTER, static_cast<keel::i32>(kGL_LINEAR));
            g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_S, static_cast<keel::i32>(kGL_CLAMP_TO_EDGE));
            g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_T, static_cast<keel::i32>(kGL_CLAMP_TO_EDGE));
            g_gl.TexImage2D(kGL_TEXTURE_2D, 0, static_cast<keel::i32>(kGL_RGBA), 256, 1, 0, kGL_RGBA, kGL_UNSIGNED_BYTE, pass.lutAbgr256);
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
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MIN_FILTER, static_cast<keel::i32>(kGL_NEAREST));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MAG_FILTER, static_cast<keel::i32>(kGL_NEAREST));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_S, static_cast<keel::i32>(kGL_CLAMP_TO_EDGE));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_T, static_cast<keel::i32>(kGL_CLAMP_TO_EDGE));
    } else {
        g_gl.BindTexture(kGL_TEXTURE_2D, cache.dataTex);
    }

    const float invNormScale = 1.0f / keel::max(1e-6f, pass.dataNormScale);
    const bool needFullUpload = dimsChanged || (cache.uploadedFullGen != pass.fullGeneration);
    const keel::u64 deltaSlices = (pass.sliceGeneration > 0 && pass.sliceGeneration >= cache.uploadedSliceGen)
        ? (pass.sliceGeneration - cache.uploadedSliceGen)
        : 0;

    if (needFullUpload || (pass.sliceGeneration > 0 && (pass.sliceGeneration < cache.uploadedSliceGen || deltaSlices >= pass.texHeight))) {
        const keel::usize total = static_cast<keel::usize>(pass.texWidth) * pass.texHeight;
        m_uploadScratch.resize(total);
        for (keel::usize i = 0; i < total; ++i) {
            const float normVal = (pass.dataFloats[i] - pass.dataNormOffset) * invNormScale;
            m_uploadScratch[i] = PackRG16Norm(normVal);
        }
        if (dimsChanged) {
            g_gl.TexImage2D(kGL_TEXTURE_2D, 0, static_cast<keel::i32>(kGL_RGBA),
                            static_cast<keel::i32>(pass.texWidth), static_cast<keel::i32>(pass.texHeight),
                            0, kGL_RGBA, kGL_UNSIGNED_BYTE, m_uploadScratch.data());
        } else {
            g_gl.TexSubImage2D(kGL_TEXTURE_2D, 0, 0, 0,
                               static_cast<keel::i32>(pass.texWidth), static_cast<keel::i32>(pass.texHeight),
                               kGL_RGBA, kGL_UNSIGNED_BYTE, m_uploadScratch.data());
        }
        cache.uploadedFullGen = pass.fullGeneration;
        cache.uploadedSliceGen = pass.sliceGeneration;
        cache.uploadedHeadRow = pass.headRow;
        if (total > 65536u) {
            m_uploadScratch = {};
        }
    } else if (deltaSlices > 0) {
        // O(1) Circular Ring-Buffer Update: upload only the newly written row(s) via glTexSubImage2D
        m_uploadScratch.resize(pass.texWidth);
        for (keel::u64 s = 0; s < deltaSlices; ++s) {
            const keel::u32 rowIdx = static_cast<keel::u32>(
                (static_cast<keel::u64>(pass.headRow) + static_cast<keel::u64>(pass.texHeight) * 4ULL - s) % pass.texHeight
            );
            const float* rowSrc = pass.dataFloats + static_cast<keel::usize>(rowIdx) * pass.texWidth;
            for (keel::u32 x = 0; x < pass.texWidth; ++x) {
                const float normVal = (rowSrc[x] - pass.dataNormOffset) * invNormScale;
                m_uploadScratch[x] = PackRG16Norm(normVal);
            }
            g_gl.TexSubImage2D(kGL_TEXTURE_2D, 0, 0, static_cast<keel::i32>(rowIdx),
                               static_cast<keel::i32>(pass.texWidth), 1,
                               kGL_RGBA, kGL_UNSIGNED_BYTE, m_uploadScratch.data());
        }
        cache.uploadedSliceGen = pass.sliceGeneration;
        cache.uploadedHeadRow = pass.headRow;
    }

    // 3. Configure Scissor Box (converting top-left UI coords to bottom-left GL coords)
    const keel::i32 scX = keel::max(0, static_cast<keel::i32>(std::floor(pass.clipRect.x)));
    const keel::i32 scTop = keel::max(0, static_cast<keel::i32>(std::floor(pass.clipRect.y)));
    const keel::i32 scRight = keel::min(static_cast<keel::i32>(viewportW), static_cast<keel::i32>(std::ceil(pass.clipRect.Right())));
    const keel::i32 scBottom = keel::min(static_cast<keel::i32>(viewportH), static_cast<keel::i32>(std::ceil(pass.clipRect.Bottom())));
    const keel::i32 scW = scRight - scX;
    const keel::i32 scH = scBottom - scTop;
    if (scW <= 0 || scH <= 0) return;

    const keel::i32 glScY = static_cast<keel::i32>(viewportH) - scBottom;
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
        g_gl.Uniform4fv(prog->uParams, static_cast<keel::i32>(pass.params.size()), &pass.params[0].x);
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
    keel::u32 viewportWidth,
    keel::u32 viewportHeight
) {
    if (!m_available || !m_renderer || viewportWidth == 0 || viewportHeight == 0 || !canvas.HasActiveGpuPasses()) {
        return;
    }

    if (m_frameCounter == 0) {
        m_frameCounter = 1;
    }

    SDL_FlushRenderer(m_renderer);

    // Save current OpenGL state
    keel::i32 prevProgram = 0;
    keel::i32 prevVbo = 0;
    keel::i32 prevActiveTex = 0;
    keel::i32 prevTex0 = 0;
    keel::i32 prevTex1 = 0;
    keel::i32 prevViewport[4] = {0, 0, 0, 0};
    keel::i32 prevScissorBox[4] = {0, 0, 0, 0};
    keel::i32 prevUnpackAlign = 4;
    keel::i32 prevAttrEnabled[3] = {0, 0, 0};
#ifndef __EMSCRIPTEN__
    keel::i32 prevUnpackRowLength = 0;
    keel::i32 prevUnpackSkipRows = 0;
    keel::i32 prevUnpackSkipPixels = 0;
#endif
    const keel::u8 prevScissorEnabled = g_gl.IsEnabled(kGL_SCISSOR_TEST);
    const keel::u8 prevBlendEnabled = g_gl.IsEnabled(kGL_BLEND);

    g_gl.GetIntegerv(kGL_CURRENT_PROGRAM, &prevProgram);
    g_gl.GetIntegerv(kGL_ARRAY_BUFFER_BINDING, &prevVbo);
    g_gl.GetIntegerv(kGL_ACTIVE_TEXTURE, &prevActiveTex);
    g_gl.GetIntegerv(kGL_VIEWPORT, prevViewport);
    g_gl.GetIntegerv(kGL_SCISSOR_BOX, prevScissorBox);
    g_gl.GetIntegerv(kGL_UNPACK_ALIGNMENT, &prevUnpackAlign);
    for (keel::u32 i = 0; i < 3; ++i) {
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
    g_gl.Viewport(0, 0, static_cast<keel::i32>(viewportWidth), static_cast<keel::i32>(viewportHeight));
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
    for (keel::u32 i = 0; i < 3; ++i) {
        if (prevAttrEnabled[i]) {
            g_gl.EnableVertexAttribArray(i);
        } else {
            g_gl.DisableVertexAttribArray(i);
        }
    }
    g_gl.BindBuffer(kGL_ARRAY_BUFFER, static_cast<keel::u32>(prevVbo));
    g_gl.UseProgram(static_cast<keel::u32>(prevProgram));

    g_gl.ActiveTexture(kGL_TEXTURE1);
    g_gl.BindTexture(kGL_TEXTURE_2D, static_cast<keel::u32>(prevTex1));
    g_gl.ActiveTexture(kGL_TEXTURE0);
    g_gl.BindTexture(kGL_TEXTURE_2D, static_cast<keel::u32>(prevTex0));
    g_gl.ActiveTexture(static_cast<keel::u32>(prevActiveTex));

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
