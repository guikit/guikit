#include "guikit/backend/SdlGlShaderEngine.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#ifdef _WIN32
#define GUIKIT_GL_APIENTRY __stdcall
#else
#define GUIKIT_GL_APIENTRY
#endif

namespace guikit {

namespace {

// Standard OpenGL 2.0 / OpenGL ES 2.0 constants
constexpr uint32_t kGL_FALSE                  = 0;
constexpr uint32_t kGL_TRIANGLES              = 0x0004;
constexpr uint32_t kGL_SCISSOR_TEST           = 0x0C11;
constexpr uint32_t kGL_SCISSOR_BOX            = 0x0C10;
constexpr uint32_t kGL_VIEWPORT               = 0x0BA2;
constexpr uint32_t kGL_BLEND                  = 0x0BE2;
constexpr uint32_t kGL_UNPACK_ROW_LENGTH      = 0x0CF2;
constexpr uint32_t kGL_UNPACK_SKIP_ROWS       = 0x0CF3;
constexpr uint32_t kGL_UNPACK_SKIP_PIXELS     = 0x0CF4;
constexpr uint32_t kGL_UNPACK_ALIGNMENT       = 0x0CF5;
constexpr uint32_t kGL_TEXTURE_2D             = 0x0DE1;
constexpr uint32_t kGL_TEXTURE_BINDING_2D     = 0x8069;
constexpr uint32_t kGL_UNSIGNED_BYTE          = 0x1401;
constexpr uint32_t kGL_FLOAT                  = 0x1406;
constexpr uint32_t kGL_RGBA                   = 0x1908;
constexpr uint32_t kGL_NEAREST                = 0x2600;
constexpr uint32_t kGL_LINEAR                 = 0x2601;
constexpr uint32_t kGL_TEXTURE_MAG_FILTER     = 0x2800;
constexpr uint32_t kGL_TEXTURE_MIN_FILTER     = 0x2801;
constexpr uint32_t kGL_TEXTURE_WRAP_S         = 0x2802;
constexpr uint32_t kGL_TEXTURE_WRAP_T         = 0x2803;
constexpr uint32_t kGL_CLAMP_TO_EDGE          = 0x812F;
constexpr uint32_t kGL_TEXTURE0               = 0x84C0;
constexpr uint32_t kGL_TEXTURE1               = 0x84C1;
constexpr uint32_t kGL_ACTIVE_TEXTURE         = 0x84E0;
constexpr uint32_t kGL_ARRAY_BUFFER           = 0x8892;
constexpr uint32_t kGL_ARRAY_BUFFER_BINDING   = 0x8894;
constexpr uint32_t kGL_STATIC_DRAW            = 0x88E4;
constexpr uint32_t kGL_FRAGMENT_SHADER        = 0x8B30;
constexpr uint32_t kGL_VERTEX_SHADER          = 0x8B31;
constexpr uint32_t kGL_COMPILE_STATUS         = 0x8B81;
constexpr uint32_t kGL_LINK_STATUS            = 0x8B82;
constexpr uint32_t kGL_CURRENT_PROGRAM        = 0x8B8D;

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
    void     (GUIKIT_GL_APIENTRY* EnableVertexAttribArray)(uint32_t index) = nullptr;
    void     (GUIKIT_GL_APIENTRY* DisableVertexAttribArray)(uint32_t index) = nullptr;
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
    ok &= LoadProc(g_gl.EnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= LoadProc(g_gl.DisableVertexAttribArray, "glDisableVertexAttribArray");
    ok &= LoadProc(g_gl.VertexAttribPointer, "glVertexAttribPointer");
    ok &= LoadProc(g_gl.DrawArrays, "glDrawArrays");
    return ok;
}

uint32_t CompileGlShader(uint32_t type, const char* src) noexcept {
    uint32_t shader = g_gl.CreateShader(type);
    if (!shader) return 0;
    g_gl.ShaderSource(shader, 1, &src, nullptr);
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

uint32_t BuildGlProgram(const char* vsSrc, const char* fsSrc) noexcept {
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

// Shared Vertex Shader: maps unit quad [0..1]x[0..1] to u_plotRect in top-left screen coordinates
constexpr const char* kSharedVertSrc = R"(
#ifdef GL_ES
precision highp float;
#endif
attribute vec2 a_uv;
varying vec2 v_uv;
uniform vec2 u_viewport;
uniform vec4 u_plotRect;

void main() {
    v_uv = a_uv;
    vec2 screenPos = u_plotRect.xy + a_uv * u_plotRect.zw;
    vec2 ndc = vec2(
        (screenPos.x / u_viewport.x) * 2.0 - 1.0,
        1.0 - (screenPos.y / u_viewport.y) * 2.0
    );
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)";

// Sonar / Radar Fragment Shader (PPI 360, Sector Wedge, B-Scan + 4-Tap Bilinear + TVG + Gain + Threshold + Colormap)
constexpr const char* kSonarFragSrc = R"(
#ifdef GL_ES
precision highp float;
#endif
varying vec2 v_uv;

uniform vec4 u_plotRect;
uniform vec2 u_origin;
uniform float u_maxRadiusPx;
uniform vec4 u_rangeParams;   // x=minRange, y=visibleMaxRange, z=matrixMaxRange, w=displayMode
uniform vec4 u_bearingParams; // x=matrixStartBrg, y=matrixEndBrg, z=headingDeg, w=sectorApertureDeg
uniform vec4 u_bscanRect;     // x, y, w, h
uniform vec4 u_dspParams;     // x=gainLinear, y=tvgAmount, z=clutterThreshold, w=unused
uniform vec2 u_gridDims;      // x=numRangeBins, y=numBeams
uniform vec3 u_bgColor;
uniform sampler2D u_polarTex;
uniform sampler2D u_lutTex;

float normalizeBearingDeg(float deg) {
    float r = mod(deg, 360.0);
    return (r < 0.0) ? (r + 360.0) : r;
}

float signedAngleDiffDeg(float a, float b) {
    float d = mod(a - b + 540.0, 360.0) - 180.0;
    return d;
}

float decodeRG16(vec4 texel) {
    return (texel.r * 65280.0 + texel.g * 255.0) / 65535.0;
}

float samplePolarTexel(float rIdx, float bIdx) {
    vec2 uv = vec2((rIdx + 0.5) / u_gridDims.x, (bIdx + 0.5) / u_gridDims.y);
    return decodeRG16(texture2D(u_polarTex, uv));
}

void main() {
    vec2 pt = u_plotRect.xy + v_uv * u_plotRect.zw;
    float minRange = u_rangeParams.x;
    float visibleMaxRange = u_rangeParams.y;
    float matrixMaxRange = u_rangeParams.z;
    int mode = int(u_rangeParams.w + 0.5);

    float matrixStartBrg = u_bearingParams.x;
    float matrixEndBrg = u_bearingParams.y;
    float headingDeg = u_bearingParams.z;
    float sectorApertureDeg = u_bearingParams.w;

    float rangeMeters = 0.0;
    float bearingDeg = 0.0;
    bool validGeom = false;

    if (mode == 0) {
        float dx = pt.x - u_origin.x;
        float dy = u_origin.y - pt.y;
        float distPx = sqrt(dx * dx + dy * dy);
        if (distPx <= u_maxRadiusPx) {
            rangeMeters = (distPx / max(1.0, u_maxRadiusPx)) * visibleMaxRange;
            float rad = (abs(dx) < 1e-5 && abs(dy) < 1e-5) ? 0.0 : atan(dx, dy);
            bearingDeg = normalizeBearingDeg(rad * 57.2957795);
            validGeom = (rangeMeters >= minRange);
        }
    } else if (mode == 1) {
        float dx = pt.x - u_origin.x;
        float dy = u_origin.y - pt.y;
        float distPx = sqrt(dx * dx + dy * dy);
        if (distPx <= u_maxRadiusPx) {
            float rad = (abs(dx) < 1e-5 && abs(dy) < 1e-5) ? 0.0 : atan(dx, dy);
            float relDeg = rad * 57.2957795;
            if (abs(relDeg) <= sectorApertureDeg * 0.5) {
                rangeMeters = (distPx / max(1.0, u_maxRadiusPx)) * visibleMaxRange;
                bearingDeg = normalizeBearingDeg(headingDeg + relDeg);
                validGeom = (rangeMeters >= minRange);
            }
        }
    } else {
        if (pt.x >= u_bscanRect.x && pt.x <= (u_bscanRect.x + u_bscanRect.z) &&
            pt.y >= u_bscanRect.y && pt.y <= (u_bscanRect.y + u_bscanRect.w)) {
            float u = (pt.x - u_bscanRect.x) / max(1.0, u_bscanRect.z);
            float v = ((u_bscanRect.y + u_bscanRect.w) - pt.y) / max(1.0, u_bscanRect.w);
            rangeMeters = v * visibleMaxRange;
            float relDeg = (u - 0.5) * sectorApertureDeg;
            bearingDeg = normalizeBearingDeg(headingDeg + relDeg);
            validGeom = (rangeMeters >= minRange);
        }
    }

    if (!validGeom) {
        gl_FragColor = vec4(u_bgColor, 1.0);
        return;
    }

    float echo = 0.0;
    if (rangeMeters >= minRange && rangeMeters <= matrixMaxRange) {
        float rNorm = clamp(rangeMeters / max(1.0, matrixMaxRange), 0.0, 1.0);
        float spanDeg = matrixEndBrg - matrixStartBrg;
        float beamNorm = 0.0;
        bool inCoverage = true;
        if (abs(spanDeg) >= 359.5) {
            beamNorm = normalizeBearingDeg(bearingDeg - matrixStartBrg) / 360.0;
        } else {
            float rel = signedAngleDiffDeg(bearingDeg, headingDeg);
            float halfSpan = max(1.0, abs(spanDeg) * 0.5);
            if (abs(rel) > halfSpan) {
                inCoverage = false;
            } else {
                beamNorm = (rel + halfSpan) / (2.0 * halfSpan);
            }
        }

        if (inCoverage) {
            float bFloat = clamp(beamNorm, 0.0, 1.0) * max(0.0, u_gridDims.y - 1.0);
            float rFloat = rNorm * max(0.0, u_gridDims.x - 1.0);

            float b0 = floor(bFloat);
            float b1 = min(u_gridDims.y - 1.0, b0 + 1.0);
            float fb = bFloat - b0;

            float r0 = floor(rFloat);
            float r1 = min(u_gridDims.x - 1.0, r0 + 1.0);
            float fr = rFloat - r0;

            float v00 = samplePolarTexel(r0, b0);
            float v01 = samplePolarTexel(r1, b0);
            float v10 = samplePolarTexel(r0, b1);
            float v11 = samplePolarTexel(r1, b1);

            float v0 = v00 + fr * (v01 - v00);
            float v1 = v10 + fr * (v11 - v10);
            float raw = v0 + fb * (v1 - v0);

            float gainLinear = u_dspParams.x;
            float tvgAmount = u_dspParams.y;
            float thresh = clamp(u_dspParams.z, 0.0, 0.85);

            float tvgBoost = 1.0 + tvgAmount * (1.35 * rNorm + 0.55 * sqrt(rNorm));
            float val = raw * tvgBoost * gainLinear;
            if (val > thresh) {
                echo = clamp((val - thresh) / max(0.05, 1.0 - thresh), 0.0, 1.0);
            }
        }
    }

    vec4 mapped = texture2D(u_lutTex, vec2((echo * 255.0 + 0.5) / 256.0, 0.5));
    gl_FragColor = vec4(mapped.rgb, 1.0);
}
)";

// STFT Spectrogram Fragment Shader (16-bit dB Grid + 4-Tap Bilinear + Dynamic Range Window + Colormap)
constexpr const char* kSpecFragSrc = R"(
#ifdef GL_ES
precision highp float;
#endif
varying vec2 v_uv;

uniform vec2 u_dbParams; // x=dbFloor, y=dbCeil
uniform vec2 u_gridDims; // x=texWidth, y=texHeight
uniform vec3 u_bgColor;
uniform sampler2D u_dbTex;
uniform sampler2D u_lutTex;

float decodeRG16(vec4 texel) {
    return (texel.r * 65280.0 + texel.g * 255.0) / 65535.0;
}

float sampleDbTexel(float xIdx, float yIdx) {
    vec2 uv = vec2((xIdx + 0.5) / u_gridDims.x, (yIdx + 0.5) / u_gridDims.y);
    return -140.0 + decodeRG16(texture2D(u_dbTex, uv)) * 160.0;
}

void main() {
    float xFloat = clamp(v_uv.x * u_gridDims.x - 0.5, 0.0, max(0.0, u_gridDims.x - 1.0));
    float yFloat = clamp(v_uv.y * u_gridDims.y - 0.5, 0.0, max(0.0, u_gridDims.y - 1.0));

    float x0 = floor(xFloat);
    float x1 = min(u_gridDims.x - 1.0, x0 + 1.0);
    float fx = xFloat - x0;

    float y0 = floor(yFloat);
    float y1 = min(u_gridDims.y - 1.0, y0 + 1.0);
    float fy = yFloat - y0;

    float v00 = sampleDbTexel(x0, y0);
    float v10 = sampleDbTexel(x1, y0);
    float v01 = sampleDbTexel(x0, y1);
    float v11 = sampleDbTexel(x1, y1);

    float v0 = v00 + fx * (v10 - v00);
    float v1 = v01 + fx * (v11 - v01);
    float dbVal = v0 + fy * (v1 - v0);

    float dbFloor = u_dbParams.x;
    float dbCeil = u_dbParams.y;
    float norm = clamp((dbVal - dbFloor) / max(1.0, dbCeil - dbFloor), 0.0, 1.0);

    vec4 mapped = texture2D(u_lutTex, vec2((norm * 255.0 + 0.5) / 256.0, 0.5));
    gl_FragColor = vec4(mapped.rgb, 1.0);
}
)";

// Hardware Circular Ring-Buffer Waterfall Fragment Shader (O(1) single-row upload + 4-Tap Modulo Bilinear + Zoom/Pan + Gate + Gamma + LUT)
constexpr const char* kWaterfallFragSrc = R"(
#ifdef GL_ES
precision highp float;
#endif
varying vec2 v_uv;

uniform vec2 u_dbParams;    // x=dbFloor, y=dbCeil
uniform vec4 u_dspParams;   // x=gainDb, y=thresholdGateDb, z=gateSuppression, w=gamma
uniform vec2 u_gridDims;    // x=texWidth (bins), y=texHeight (historyRows)
uniform vec4 u_ringParams;  // x=headRow, y=validRows, z=invertTimeAxis, w=unused
uniform vec2 u_xViewNorm;   // x=xMinNorm, y=xMaxNorm
uniform vec3 u_bgColor;
uniform sampler2D u_dbTex;
uniform sampler2D u_lutTex;

float decodeRG16(vec4 texel) {
    return (texel.r * 65280.0 + texel.g * 255.0) / 65535.0;
}

float sampleRingDbTexel(float xIdx, float rowIdx) {
    vec2 uv = vec2((xIdx + 0.5) / u_gridDims.x, (rowIdx + 0.5) / u_gridDims.y);
    return -140.0 + decodeRG16(texture2D(u_dbTex, uv)) * 160.0;
}

void main() {
    float timeUv = (u_ringParams.z > 0.5) ? (1.0 - v_uv.y) : v_uv.y;
    float ageFloat = clamp(timeUv * max(1.0, u_gridDims.y - 1.0), 0.0, max(0.0, u_gridDims.y - 1.0));
    if (ageFloat > (u_ringParams.y - 0.5)) {
        gl_FragColor = vec4(u_bgColor, 1.0);
        return;
    }

    float uNorm = mix(u_xViewNorm.x, u_xViewNorm.y, v_uv.x);
    float xFloat = clamp(uNorm * u_gridDims.x - 0.5, 0.0, max(0.0, u_gridDims.x - 1.0));

    float x0 = floor(xFloat);
    float x1 = min(u_gridDims.x - 1.0, x0 + 1.0);
    float fx = xFloat - x0;

    float age0 = floor(ageFloat);
    float age1 = min(max(0.0, u_ringParams.y - 1.0), age0 + 1.0);
    float fy = ageFloat - age0;

    float row0 = mod(u_ringParams.x - age0 + u_gridDims.y * 4.0, u_gridDims.y);
    float row1 = mod(u_ringParams.x - age1 + u_gridDims.y * 4.0, u_gridDims.y);

    float v00 = sampleRingDbTexel(x0, row0);
    float v10 = sampleRingDbTexel(x1, row0);
    float v01 = sampleRingDbTexel(x0, row1);
    float v11 = sampleRingDbTexel(x1, row1);

    float v0 = v00 + fx * (v10 - v00);
    float v1 = v01 + fx * (v11 - v01);
    float dbVal = (v0 + fy * (v1 - v0)) + u_dspParams.x;

    float dbFloor = u_dbParams.x;
    float dbCeil = u_dbParams.y;
    float norm = clamp((dbVal - dbFloor) / max(1.0, dbCeil - dbFloor), 0.0, 1.0);

    float threshDb = u_dspParams.y;
    float gateSupp = clamp(u_dspParams.z, 0.0, 1.0);
    if (gateSupp > 0.001 && dbVal < threshDb) {
        float soft = clamp((dbVal - (threshDb - 10.0)) / 10.0, 0.0, 1.0);
        norm *= mix(1.0, soft * 0.25, gateSupp);
    }

    float gamma = max(0.25, u_dspParams.w);
    norm = pow(clamp(norm, 0.0, 1.0), gamma);

    vec4 mapped = texture2D(u_lutTex, vec2((norm * 255.0 + 0.5) / 256.0, 0.5));
    gl_FragColor = vec4(mapped.rgb, 1.0);
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

    m_sonarProgram = BuildGlProgram(kSharedVertSrc, kSonarFragSrc);
    m_specProgram = BuildGlProgram(kSharedVertSrc, kSpecFragSrc);
    m_waterfallProgram = BuildGlProgram(kSharedVertSrc, kWaterfallFragSrc);
    if (!m_sonarProgram || !m_specProgram || !m_waterfallProgram) {
        if (m_sonarProgram) g_gl.DeleteProgram(m_sonarProgram);
        if (m_specProgram) g_gl.DeleteProgram(m_specProgram);
        if (m_waterfallProgram) g_gl.DeleteProgram(m_waterfallProgram);
        m_sonarProgram = 0;
        m_specProgram = 0;
        m_waterfallProgram = 0;
        m_backendSummary = "Unavailable (GLSL compilation failed)";
        return false;
    }

    // Resolve Sonar uniform locations
    m_uSonarViewport      = g_gl.GetUniformLocation(m_sonarProgram, "u_viewport");
    m_uSonarPlotRect      = g_gl.GetUniformLocation(m_sonarProgram, "u_plotRect");
    m_uSonarOrigin        = g_gl.GetUniformLocation(m_sonarProgram, "u_origin");
    m_uSonarMaxRadiusPx   = g_gl.GetUniformLocation(m_sonarProgram, "u_maxRadiusPx");
    m_uSonarRangeParams   = g_gl.GetUniformLocation(m_sonarProgram, "u_rangeParams");
    m_uSonarBearingParams = g_gl.GetUniformLocation(m_sonarProgram, "u_bearingParams");
    m_uSonarBScanRect     = g_gl.GetUniformLocation(m_sonarProgram, "u_bscanRect");
    m_uSonarDspParams     = g_gl.GetUniformLocation(m_sonarProgram, "u_dspParams");
    m_uSonarGridDims      = g_gl.GetUniformLocation(m_sonarProgram, "u_gridDims");
    m_uSonarBgColor       = g_gl.GetUniformLocation(m_sonarProgram, "u_bgColor");
    m_uSonarPolarTex      = g_gl.GetUniformLocation(m_sonarProgram, "u_polarTex");
    m_uSonarLutTex        = g_gl.GetUniformLocation(m_sonarProgram, "u_lutTex");

    // Resolve Spectrogram uniform locations
    m_uSpecViewport       = g_gl.GetUniformLocation(m_specProgram, "u_viewport");
    m_uSpecPlotRect       = g_gl.GetUniformLocation(m_specProgram, "u_plotRect");
    m_uSpecDbParams       = g_gl.GetUniformLocation(m_specProgram, "u_dbParams");
    m_uSpecGridDims       = g_gl.GetUniformLocation(m_specProgram, "u_gridDims");
    m_uSpecBgColor        = g_gl.GetUniformLocation(m_specProgram, "u_bgColor");
    m_uSpecDbTex          = g_gl.GetUniformLocation(m_specProgram, "u_dbTex");
    m_uSpecLutTex         = g_gl.GetUniformLocation(m_specProgram, "u_lutTex");

    // Resolve Waterfall uniform locations
    m_uWfViewport         = g_gl.GetUniformLocation(m_waterfallProgram, "u_viewport");
    m_uWfPlotRect         = g_gl.GetUniformLocation(m_waterfallProgram, "u_plotRect");
    m_uWfDbParams         = g_gl.GetUniformLocation(m_waterfallProgram, "u_dbParams");
    m_uWfDspParams        = g_gl.GetUniformLocation(m_waterfallProgram, "u_dspParams");
    m_uWfGridDims         = g_gl.GetUniformLocation(m_waterfallProgram, "u_gridDims");
    m_uWfRingParams       = g_gl.GetUniformLocation(m_waterfallProgram, "u_ringParams");
    m_uWfXViewNorm        = g_gl.GetUniformLocation(m_waterfallProgram, "u_xViewNorm");
    m_uWfBgColor          = g_gl.GetUniformLocation(m_waterfallProgram, "u_bgColor");
    m_uWfDbTex            = g_gl.GetUniformLocation(m_waterfallProgram, "u_dbTex");
    m_uWfLutTex           = g_gl.GetUniformLocation(m_waterfallProgram, "u_lutTex");

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
        for (auto& [_, entry] : m_sonarCaches) {
            if (entry.dataTex) g_gl.DeleteTextures(1, &entry.dataTex);
            if (entry.lutTex) g_gl.DeleteTextures(1, &entry.lutTex);
        }
        for (auto& [_, entry] : m_specCaches) {
            if (entry.dataTex) g_gl.DeleteTextures(1, &entry.dataTex);
            if (entry.lutTex) g_gl.DeleteTextures(1, &entry.lutTex);
        }
        for (auto& [_, entry] : m_waterfallCaches) {
            if (entry.dataTex) g_gl.DeleteTextures(1, &entry.dataTex);
            if (entry.lutTex) g_gl.DeleteTextures(1, &entry.lutTex);
        }
        if (m_quadVbo) {
            g_gl.DeleteBuffers(1, &m_quadVbo);
            m_quadVbo = 0;
        }
        if (m_sonarProgram) {
            g_gl.DeleteProgram(m_sonarProgram);
            m_sonarProgram = 0;
        }
        if (m_specProgram) {
            g_gl.DeleteProgram(m_specProgram);
            m_specProgram = 0;
        }
        if (m_waterfallProgram) {
            g_gl.DeleteProgram(m_waterfallProgram);
            m_waterfallProgram = 0;
        }
    }
    m_sonarCaches.clear();
    m_specCaches.clear();
    m_waterfallCaches.clear();
    m_renderer = nullptr;
    m_available = false;
    m_backendSummary = "Disabled";
}

void SdlGlShaderEngine::PruneStaleTextures() {
    constexpr uint64_t kMaxIdleFrames = 300;
    std::erase_if(m_sonarCaches, [&](auto& kv) {
        if (m_frameCounter - kv.second.lastUsedFrame > kMaxIdleFrames) {
            if (kv.second.dataTex) g_gl.DeleteTextures(1, &kv.second.dataTex);
            if (kv.second.lutTex) g_gl.DeleteTextures(1, &kv.second.lutTex);
            return true;
        }
        return false;
    });
    std::erase_if(m_specCaches, [&](auto& kv) {
        if (m_frameCounter - kv.second.lastUsedFrame > kMaxIdleFrames) {
            if (kv.second.dataTex) g_gl.DeleteTextures(1, &kv.second.dataTex);
            if (kv.second.lutTex) g_gl.DeleteTextures(1, &kv.second.lutTex);
            return true;
        }
        return false;
    });
    std::erase_if(m_waterfallCaches, [&](auto& kv) {
        if (m_frameCounter - kv.second.lastUsedFrame > kMaxIdleFrames) {
            if (kv.second.dataTex) g_gl.DeleteTextures(1, &kv.second.dataTex);
            if (kv.second.lutTex) g_gl.DeleteTextures(1, &kv.second.lutTex);
            return true;
        }
        return false;
    });
}

void SdlGlShaderEngine::RenderSonarPass(
    const ICanvas::SonarGpuFieldDesc& pass,
    uint32_t viewportW,
    uint32_t viewportH
) {
    if (pass.clipRect.w <= 0.5f || pass.clipRect.h <= 0.5f ||
        pass.numBeams == 0 || pass.numRangeBins == 0) {
        return;
    }

    auto& cache = m_sonarCaches[pass.widgetId];
    cache.lastUsedFrame = m_frameCounter;

    // 1. Ensure 256x1 Colormap LUT texture is created & up to date
    g_gl.ActiveTexture(kGL_TEXTURE1);
    if (!cache.lutTex) {
        g_gl.GenTextures(1, &cache.lutTex);
        g_gl.BindTexture(kGL_TEXTURE_2D, cache.lutTex);
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MIN_FILTER, static_cast<int>(kGL_LINEAR));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MAG_FILTER, static_cast<int>(kGL_LINEAR));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_S, static_cast<int>(kGL_CLAMP_TO_EDGE));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_T, static_cast<int>(kGL_CLAMP_TO_EDGE));
        g_gl.TexImage2D(kGL_TEXTURE_2D, 0, static_cast<int>(kGL_RGBA), 256, 1, 0, kGL_RGBA, kGL_UNSIGNED_BYTE, pass.colormapAbgr);
        cache.uploadedColormapId = pass.colormapId;
    } else {
        g_gl.BindTexture(kGL_TEXTURE_2D, cache.lutTex);
        if (cache.uploadedColormapId != pass.colormapId) {
            g_gl.TexSubImage2D(kGL_TEXTURE_2D, 0, 0, 0, 256, 1, kGL_RGBA, kGL_UNSIGNED_BYTE, pass.colormapAbgr);
            cache.uploadedColormapId = pass.colormapId;
        }
    }

    // 2. Ensure Polar Echo Data texture (width = numRangeBins, height = numBeams) is up to date
    g_gl.ActiveTexture(kGL_TEXTURE0);
    const bool dimsChanged = (!cache.dataTex || cache.width != pass.numRangeBins || cache.height != pass.numBeams);
    if (dimsChanged) {
        if (!cache.dataTex) {
            g_gl.GenTextures(1, &cache.dataTex);
        }
        cache.width = pass.numRangeBins;
        cache.height = pass.numBeams;
        cache.uploadedGeneration = 0;

        g_gl.BindTexture(kGL_TEXTURE_2D, cache.dataTex);
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MIN_FILTER, static_cast<int>(kGL_NEAREST));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MAG_FILTER, static_cast<int>(kGL_NEAREST));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_S, static_cast<int>(kGL_CLAMP_TO_EDGE));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_T, static_cast<int>(kGL_CLAMP_TO_EDGE));
    } else {
        g_gl.BindTexture(kGL_TEXTURE_2D, cache.dataTex);
    }

    if ((dimsChanged || cache.uploadedGeneration != pass.dataGeneration) && pass.polarData) {
        const size_t total = static_cast<size_t>(pass.numBeams) * pass.numRangeBins;
        m_uploadScratch.resize(total);
        for (size_t i = 0; i < total; ++i) {
            m_uploadScratch[i] = PackRG16Norm(pass.polarData[i]);
        }
        if (dimsChanged) {
            g_gl.TexImage2D(kGL_TEXTURE_2D, 0, static_cast<int>(kGL_RGBA),
                            static_cast<int>(pass.numRangeBins), static_cast<int>(pass.numBeams),
                            0, kGL_RGBA, kGL_UNSIGNED_BYTE, m_uploadScratch.data());
        } else {
            g_gl.TexSubImage2D(kGL_TEXTURE_2D, 0, 0, 0,
                               static_cast<int>(pass.numRangeBins), static_cast<int>(pass.numBeams),
                               kGL_RGBA, kGL_UNSIGNED_BYTE, m_uploadScratch.data());
        }
        cache.uploadedGeneration = pass.dataGeneration;
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

    // 4. Bind Sonar Shader Program & Uniforms
    g_gl.UseProgram(m_sonarProgram);
    g_gl.Uniform2f(m_uSonarViewport, static_cast<float>(viewportW), static_cast<float>(viewportH));
    g_gl.Uniform4f(m_uSonarPlotRect, pass.plotRect.x, pass.plotRect.y, pass.plotRect.w, pass.plotRect.h);
    g_gl.Uniform2f(m_uSonarOrigin, pass.origin.x, pass.origin.y);
    g_gl.Uniform1f(m_uSonarMaxRadiusPx, pass.maxRadiusPx);
    g_gl.Uniform4f(m_uSonarRangeParams,
                   pass.minRangeMeters,
                   pass.visibleMaxRangeMeters,
                   pass.matrixMaxRangeMeters,
                   static_cast<float>(pass.displayMode));
    g_gl.Uniform4f(m_uSonarBearingParams,
                   pass.matrixStartBearingDeg,
                   pass.matrixEndBearingDeg,
                   pass.headingDeg,
                   pass.sectorApertureDeg);
    g_gl.Uniform4f(m_uSonarBScanRect,
                   pass.bscanInnerRect.x,
                   pass.bscanInnerRect.y,
                   pass.bscanInnerRect.w,
                   pass.bscanInnerRect.h);

    const float gainLinear = std::pow(10.0f, pass.gainDb / 28.0f);
    g_gl.Uniform4f(m_uSonarDspParams, gainLinear, pass.tvgAmount, pass.clutterThreshold, 0.0f);
    g_gl.Uniform2f(m_uSonarGridDims, static_cast<float>(pass.numRangeBins), static_cast<float>(pass.numBeams));
    g_gl.Uniform3f(m_uSonarBgColor,
                   static_cast<float>(pass.bgColor.r) / 255.0f,
                   static_cast<float>(pass.bgColor.g) / 255.0f,
                   static_cast<float>(pass.bgColor.b) / 255.0f);
    g_gl.Uniform1i(m_uSonarPolarTex, 0);
    g_gl.Uniform1i(m_uSonarLutTex, 1);

    g_gl.DrawArrays(kGL_TRIANGLES, 0, 6);
}

void SdlGlShaderEngine::RenderSpectrogramPass(
    const ICanvas::SpectrogramGpuFieldDesc& pass,
    uint32_t viewportW,
    uint32_t viewportH
) {
    if (pass.clipRect.w <= 0.5f || pass.clipRect.h <= 0.5f ||
        pass.texWidth == 0 || pass.texHeight == 0) {
        return;
    }

    auto& cache = m_specCaches[pass.widgetId];
    cache.lastUsedFrame = m_frameCounter;

    // 1. Ensure 256x1 Colormap LUT texture is created & up to date
    g_gl.ActiveTexture(kGL_TEXTURE1);
    if (!cache.lutTex) {
        g_gl.GenTextures(1, &cache.lutTex);
        g_gl.BindTexture(kGL_TEXTURE_2D, cache.lutTex);
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MIN_FILTER, static_cast<int>(kGL_LINEAR));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MAG_FILTER, static_cast<int>(kGL_LINEAR));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_S, static_cast<int>(kGL_CLAMP_TO_EDGE));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_T, static_cast<int>(kGL_CLAMP_TO_EDGE));
        g_gl.TexImage2D(kGL_TEXTURE_2D, 0, static_cast<int>(kGL_RGBA), 256, 1, 0, kGL_RGBA, kGL_UNSIGNED_BYTE, pass.colormapAbgr);
        cache.uploadedColormapId = pass.colormapId;
    } else {
        g_gl.BindTexture(kGL_TEXTURE_2D, cache.lutTex);
        if (cache.uploadedColormapId != pass.colormapId) {
            g_gl.TexSubImage2D(kGL_TEXTURE_2D, 0, 0, 0, 256, 1, kGL_RGBA, kGL_UNSIGNED_BYTE, pass.colormapAbgr);
            cache.uploadedColormapId = pass.colormapId;
        }
    }

    // 2. Ensure Spectrogram dB Grid texture is up to date
    g_gl.ActiveTexture(kGL_TEXTURE0);
    const bool dimsChanged = (!cache.dataTex || cache.width != pass.texWidth || cache.height != pass.texHeight);
    if (dimsChanged) {
        if (!cache.dataTex) {
            g_gl.GenTextures(1, &cache.dataTex);
        }
        cache.width = pass.texWidth;
        cache.height = pass.texHeight;
        cache.uploadedGeneration = 0;

        g_gl.BindTexture(kGL_TEXTURE_2D, cache.dataTex);
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MIN_FILTER, static_cast<int>(kGL_NEAREST));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MAG_FILTER, static_cast<int>(kGL_NEAREST));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_S, static_cast<int>(kGL_CLAMP_TO_EDGE));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_T, static_cast<int>(kGL_CLAMP_TO_EDGE));
    } else {
        g_gl.BindTexture(kGL_TEXTURE_2D, cache.dataTex);
    }

    if ((dimsChanged || cache.uploadedGeneration != pass.dataGeneration) && pass.dbGrid) {
        const size_t total = static_cast<size_t>(pass.texWidth) * pass.texHeight;
        m_uploadScratch.resize(total);
        for (size_t i = 0; i < total; ++i) {
            const float normDb = (pass.dbGrid[i] - (-140.0f)) / 160.0f;
            m_uploadScratch[i] = PackRG16Norm(normDb);
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
        cache.uploadedGeneration = pass.dataGeneration;
    }

    // 3. Configure Scissor Box
    const int scX = std::max(0, static_cast<int>(std::floor(pass.clipRect.x)));
    const int scTop = std::max(0, static_cast<int>(std::floor(pass.clipRect.y)));
    const int scRight = std::min(static_cast<int>(viewportW), static_cast<int>(std::ceil(pass.clipRect.Right())));
    const int scBottom = std::min(static_cast<int>(viewportH), static_cast<int>(std::ceil(pass.clipRect.Bottom())));
    const int scW = scRight - scX;
    const int scH = scBottom - scTop;
    if (scW <= 0 || scH <= 0) return;

    const int glScY = static_cast<int>(viewportH) - scBottom;
    g_gl.Scissor(scX, glScY, scW, scH);

    // 4. Bind Spectrogram Shader Program & Uniforms
    g_gl.UseProgram(m_specProgram);
    g_gl.Uniform2f(m_uSpecViewport, static_cast<float>(viewportW), static_cast<float>(viewportH));
    g_gl.Uniform4f(m_uSpecPlotRect, pass.plotRect.x, pass.plotRect.y, pass.plotRect.w, pass.plotRect.h);
    g_gl.Uniform2f(m_uSpecDbParams, pass.dbFloor, pass.dbCeil);
    g_gl.Uniform2f(m_uSpecGridDims, static_cast<float>(pass.texWidth), static_cast<float>(pass.texHeight));
    g_gl.Uniform3f(m_uSpecBgColor,
                   static_cast<float>(pass.bgColor.r) / 255.0f,
                   static_cast<float>(pass.bgColor.g) / 255.0f,
                   static_cast<float>(pass.bgColor.b) / 255.0f);
    g_gl.Uniform1i(m_uSpecDbTex, 0);
    g_gl.Uniform1i(m_uSpecLutTex, 1);

    g_gl.DrawArrays(kGL_TRIANGLES, 0, 6);
}

void SdlGlShaderEngine::RenderWaterfallPass(
    const ICanvas::WaterfallGpuFieldDesc& pass,
    uint32_t viewportW,
    uint32_t viewportH
) {
    if (pass.clipRect.w <= 0.5f || pass.clipRect.h <= 0.5f ||
        pass.texWidth == 0 || pass.texHeight == 0) {
        return;
    }

    auto& cache = m_waterfallCaches[pass.widgetId];
    cache.lastUsedFrame = m_frameCounter;

    // 1. Ensure 256x1 Colormap LUT texture is created & up to date
    g_gl.ActiveTexture(kGL_TEXTURE1);
    if (!cache.lutTex) {
        g_gl.GenTextures(1, &cache.lutTex);
        g_gl.BindTexture(kGL_TEXTURE_2D, cache.lutTex);
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MIN_FILTER, static_cast<int>(kGL_LINEAR));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_MAG_FILTER, static_cast<int>(kGL_LINEAR));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_S, static_cast<int>(kGL_CLAMP_TO_EDGE));
        g_gl.TexParameteri(kGL_TEXTURE_2D, kGL_TEXTURE_WRAP_T, static_cast<int>(kGL_CLAMP_TO_EDGE));
        g_gl.TexImage2D(kGL_TEXTURE_2D, 0, static_cast<int>(kGL_RGBA), 256, 1, 0, kGL_RGBA, kGL_UNSIGNED_BYTE, pass.colormapAbgr);
        cache.uploadedColormapId = pass.colormapId;
    } else {
        g_gl.BindTexture(kGL_TEXTURE_2D, cache.lutTex);
        if (cache.uploadedColormapId != pass.colormapId) {
            g_gl.TexSubImage2D(kGL_TEXTURE_2D, 0, 0, 0, 256, 1, kGL_RGBA, kGL_UNSIGNED_BYTE, pass.colormapAbgr);
            cache.uploadedColormapId = pass.colormapId;
        }
    }

    // 2. Ensure Circular Ring-Buffer Texture (texWidth x texHeight) is up to date
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

    if (pass.ringBufferDb) {
        const bool needFullUpload = dimsChanged || (cache.uploadedFullGen != pass.fullGeneration);
        const uint64_t deltaSlices = (pass.sliceGeneration >= cache.uploadedSliceGen)
            ? (pass.sliceGeneration - cache.uploadedSliceGen)
            : pass.texHeight;

        if (needFullUpload || deltaSlices >= pass.texHeight) {
            const size_t total = static_cast<size_t>(pass.texWidth) * pass.texHeight;
            m_uploadScratch.resize(total);
            for (size_t i = 0; i < total; ++i) {
                const float normDb = (pass.ringBufferDb[i] - (-140.0f)) / 160.0f;
                m_uploadScratch[i] = PackRG16Norm(normDb);
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
            // O(1) Ring-Buffer Update: upload only the newly written row(s) via glTexSubImage2D!
            m_uploadScratch.resize(pass.texWidth);
            for (uint64_t s = 0; s < deltaSlices; ++s) {
                const uint32_t rowIdx = static_cast<uint32_t>(
                    (static_cast<uint64_t>(pass.headRow) + static_cast<uint64_t>(pass.texHeight) * 4ULL - s) % pass.texHeight
                );
                const float* rowSrc = pass.ringBufferDb + static_cast<size_t>(rowIdx) * pass.texWidth;
                for (uint32_t x = 0; x < pass.texWidth; ++x) {
                    const float normDb = (rowSrc[x] - (-140.0f)) / 160.0f;
                    m_uploadScratch[x] = PackRG16Norm(normDb);
                }
                g_gl.TexSubImage2D(kGL_TEXTURE_2D, 0, 0, static_cast<int>(rowIdx),
                                   static_cast<int>(pass.texWidth), 1,
                                   kGL_RGBA, kGL_UNSIGNED_BYTE, m_uploadScratch.data());
            }
            cache.uploadedSliceGen = pass.sliceGeneration;
            cache.uploadedHeadRow = pass.headRow;
        }
    }

    // 3. Configure Scissor Box
    const int scX = std::max(0, static_cast<int>(std::floor(pass.clipRect.x)));
    const int scTop = std::max(0, static_cast<int>(std::floor(pass.clipRect.y)));
    const int scRight = std::min(static_cast<int>(viewportW), static_cast<int>(std::ceil(pass.clipRect.Right())));
    const int scBottom = std::min(static_cast<int>(viewportH), static_cast<int>(std::ceil(pass.clipRect.Bottom())));
    const int scW = scRight - scX;
    const int scH = scBottom - scTop;
    if (scW <= 0 || scH <= 0) return;

    const int glScY = static_cast<int>(viewportH) - scBottom;
    g_gl.Scissor(scX, glScY, scW, scH);

    // 4. Bind Waterfall Shader Program & Uniforms
    g_gl.UseProgram(m_waterfallProgram);
    g_gl.Uniform2f(m_uWfViewport, static_cast<float>(viewportW), static_cast<float>(viewportH));
    g_gl.Uniform4f(m_uWfPlotRect, pass.plotRect.x, pass.plotRect.y, pass.plotRect.w, pass.plotRect.h);
    g_gl.Uniform2f(m_uWfDbParams, pass.dbFloor, pass.dbCeil);
    g_gl.Uniform4f(m_uWfDspParams, pass.gainDb, pass.thresholdGateDb, pass.gateSuppression, pass.gamma);
    g_gl.Uniform2f(m_uWfGridDims, static_cast<float>(pass.texWidth), static_cast<float>(pass.texHeight));
    g_gl.Uniform4f(m_uWfRingParams,
                   static_cast<float>(pass.headRow % pass.texHeight),
                   static_cast<float>(std::min(pass.validRows, pass.texHeight)),
                   pass.invertTimeAxis ? 1.0f : 0.0f,
                   0.0f);
    g_gl.Uniform2f(m_uWfXViewNorm, pass.xMinNorm, pass.xMaxNorm);
    g_gl.Uniform3f(m_uWfBgColor,
                   static_cast<float>(pass.bgColor.r) / 255.0f,
                   static_cast<float>(pass.bgColor.g) / 255.0f,
                   static_cast<float>(pass.bgColor.b) / 255.0f);
    g_gl.Uniform1i(m_uWfDbTex, 0);
    g_gl.Uniform1i(m_uWfLutTex, 1);

    g_gl.DrawArrays(kGL_TRIANGLES, 0, 6);
}

void SdlGlShaderEngine::RenderUnderlays(
    const ThorCanvas& canvas,
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
    g_gl.VertexAttribPointer(0, 2, kGL_FLOAT, kGL_FALSE, 2 * sizeof(float), nullptr);

    for (const auto& sonarPass : canvas.GetSonarGpuPasses()) {
        RenderSonarPass(sonarPass, viewportWidth, viewportHeight);
    }

    for (const auto& specPass : canvas.GetSpectrogramGpuPasses()) {
        RenderSpectrogramPass(specPass, viewportWidth, viewportHeight);
    }

    for (const auto& wfPass : canvas.GetWaterfallGpuPasses()) {
        RenderWaterfallPass(wfPass, viewportWidth, viewportHeight);
    }

    if ((m_frameCounter & 63u) == 0u) {
        PruneStaleTextures();
    }

    // Restore OpenGL state cleanly so SDL_Renderer can composite the UI layer without interference
    g_gl.DisableVertexAttribArray(0);
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
