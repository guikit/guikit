#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <thread>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <ctime>
    #include <unistd.h>
#endif

namespace guikit {

/// Lightweight, cross-platform performance metrics tracker for immediate mode frame loops.
/// Computes smoothed FPS, process CPU utilization percentage, CPU frame work time,
/// and GPU transfer/render time with frame budget percentage.
class FrameMetrics {
public:
    FrameMetrics() noexcept {
        m_numProcessors = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
        m_lastCpuCheckTime = std::chrono::steady_clock::now();
        m_lastProcessCpuSec = QueryProcessCpuSeconds();
        m_lastDisplayUpdateTime = m_lastCpuCheckTime;
        m_lastFrameTime = m_lastCpuCheckTime;
        m_lastPresentedTime = m_lastCpuCheckTime;
        m_fpsWindowStart = m_lastCpuCheckTime;
    }

    /// Call at the very beginning of CPU frame work.
    void BeginFrame() noexcept {
        const auto now = std::chrono::steady_clock::now();
        m_frameStart = now;
        m_lastFrameTime = now;
        m_hasPreviousFrame = true;
    }

    /// Call immediately after presenting the frame buffer to the display (e.g. after SDL_RenderPresent).
    void MarkFramePresented() noexcept {
        const auto now = std::chrono::steady_clock::now();
        m_lastPresentedTime = now;
        m_hasPresentedFrames = true;
        m_isIdle = false;
        ++m_fpsFrameCount;
    }

    /// Call immediately after CPU work (UI layout, immediate mode execution, and software rasterization).
    void MarkCpuDone() noexcept {
        m_cpuDone = std::chrono::steady_clock::now();
        const float workMs = std::chrono::duration<float, std::milli>(m_cpuDone - m_frameStart).count();
        if (m_smoothedCpuMs <= 0.0f) {
            m_smoothedCpuMs = workMs;
        } else {
            m_smoothedCpuMs = m_smoothedCpuMs * 0.85f + workMs * 0.15f;
        }
    }

    /// Call right before submitting texture updates and draw commands to the GPU.
    void MarkGpuStart() noexcept {
        m_gpuStart = std::chrono::steady_clock::now();
    }

    /// Call right after GPU texture upload and render commands are submitted.
    void MarkGpuDone() noexcept {
        m_gpuDone = std::chrono::steady_clock::now();
        const float gpuMs = std::chrono::duration<float, std::milli>(m_gpuDone - m_gpuStart).count();
        if (m_smoothedGpuMs <= 0.0f) {
            m_smoothedGpuMs = gpuMs;
        } else {
            m_smoothedGpuMs = m_smoothedGpuMs * 0.85f + gpuMs * 0.15f;
        }
    }

    /// Call at the end of the frame (after present / delay).
    void EndFrame(float targetFps = 60.0f) noexcept {
        const auto now = std::chrono::steady_clock::now();

        // Evaluate idle status based on time elapsed since the last physically presented frame
        if (m_hasPresentedFrames) {
            const float idleElapsed = std::chrono::duration<float>(now - m_lastPresentedTime).count();
            if (idleElapsed > 0.25f) {
                m_isIdle = true;
                m_smoothedFps = 0.0f;
                m_fpsFrameCount = 0;
                m_fpsWindowStart = now;
            } else {
                m_isIdle = false;
                const float fpsElapsed = std::chrono::duration<float>(now - m_fpsWindowStart).count();
                const float windowThreshold = (m_smoothedFps <= 0.0f) ? 0.20f : 0.30f;
                if (fpsElapsed >= windowThreshold && m_fpsFrameCount > 0) {
                    const float measuredFps = static_cast<float>(m_fpsFrameCount) / fpsElapsed;
                    if (m_smoothedFps <= 0.0f) {
                        m_smoothedFps = measuredFps;
                    } else {
                        m_smoothedFps = m_smoothedFps * 0.70f + measuredFps * 0.30f;
                    }
                    m_fpsFrameCount = 0;
                    m_fpsWindowStart = now;
                }
            }
        } else {
            m_isIdle = true;
            m_smoothedFps = 0.0f;
            m_fpsWindowStart = now;
        }

        // Update process CPU utilization percentage at ~0.5 second intervals
        const double elapsedCpuSec = std::chrono::duration<double>(now - m_lastCpuCheckTime).count();
        if (elapsedCpuSec >= 0.5) {
            const double curCpuSec = QueryProcessCpuSeconds();
            if (curCpuSec >= 0.0 && m_lastProcessCpuSec >= 0.0) {
                const double deltaCpu = curCpuSec - m_lastProcessCpuSec;
                const float pct = static_cast<float>((deltaCpu / (elapsedCpuSec * m_numProcessors)) * 100.0);
                m_cpuPercent = std::clamp(pct, 0.0f, 100.0f);
            }
            m_lastProcessCpuSec = curCpuSec;
            m_lastCpuCheckTime = now;
        }

        // Update formatted status display string at ~0.3 second intervals to prevent visual jitter
        const double displayElapsed = std::chrono::duration<double>(now - m_lastDisplayUpdateTime).count();
        if (displayElapsed >= 0.3 || m_displayFps <= 0.0f || m_isIdle) {
            m_displayFps = m_isIdle ? 0.0f : m_smoothedFps;
            m_displayCpuPercent = m_cpuPercent;
            m_displayCpuTimeMs = m_isIdle ? 0.0f : m_smoothedCpuMs;
            m_displayGpuTimeMs = m_isIdle ? 0.0f : m_smoothedGpuMs;

            const float targetFrameMs = (targetFps > 0.0f) ? (1000.0f / targetFps) : 16.67f;
            m_displayGpuBudgetPercent = std::clamp((m_displayGpuTimeMs / targetFrameMs) * 100.0f, 0.0f, 100.0f);

            char buf[128];
            if (m_isIdle) {
                std::snprintf(
                    buf,
                    sizeof(buf),
                    "0 FPS (Idle) | CPU: %.1f%% (0.00 ms) | GPU: 0.00 ms (0%% budget)",
                    m_displayCpuPercent
                );
            } else {
                char cpuTimeBuf[16];
                if (m_displayCpuTimeMs < 1.0f) {
                    std::snprintf(cpuTimeBuf, sizeof(cpuTimeBuf), "%.2f ms", m_displayCpuTimeMs);
                } else {
                    std::snprintf(cpuTimeBuf, sizeof(cpuTimeBuf), "%.1f ms", m_displayCpuTimeMs);
                }

                char gpuTimeBuf[16];
                if (m_displayGpuTimeMs < 1.0f) {
                    std::snprintf(gpuTimeBuf, sizeof(gpuTimeBuf), "%.2f ms", m_displayGpuTimeMs);
                } else {
                    std::snprintf(gpuTimeBuf, sizeof(gpuTimeBuf), "%.1f ms", m_displayGpuTimeMs);
                }

                std::snprintf(
                    buf,
                    sizeof(buf),
                    "%.0f FPS | CPU: %.1f%% (%s) | GPU: %s (%.0f%% budget)",
                    m_displayFps,
                    m_displayCpuPercent,
                    cpuTimeBuf,
                    gpuTimeBuf,
                    m_displayGpuBudgetPercent
                );
            }
            m_statusString = buf;
            m_lastDisplayUpdateTime = now;
        }
    }

    [[nodiscard]] float GetFps() const noexcept { return m_displayFps; }
    [[nodiscard]] float GetCpuPercent() const noexcept { return m_displayCpuPercent; }
    [[nodiscard]] float GetCpuTimeMs() const noexcept { return m_displayCpuTimeMs; }
    [[nodiscard]] float GetGpuTimeMs() const noexcept { return m_displayGpuTimeMs; }
    [[nodiscard]] float GetGpuBudgetPercent() const noexcept { return m_displayGpuBudgetPercent; }
    [[nodiscard]] bool IsIdle() const noexcept { return m_isIdle; }

    /// Returns human-readable status line, e.g. "60 FPS | CPU: 1.4% (1.8 ms) | GPU: 0.2 ms (2% budget)"
    [[nodiscard]] std::string_view GetStatusString() const noexcept {
        return m_statusString;
    }

private:
    static double QueryProcessCpuSeconds() noexcept {
#if defined(_WIN32)
        FILETIME creation{}, exit{}, kernel{}, user{};
        if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
            return -1.0;
        }
        ULARGE_INTEGER k, u;
        k.LowPart = kernel.dwLowDateTime;
        k.HighPart = kernel.dwHighDateTime;
        u.LowPart = user.dwLowDateTime;
        u.HighPart = user.dwHighDateTime;
        return static_cast<double>(k.QuadPart + u.QuadPart) * 1e-7;
#else
        timespec ts{};
        if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0) {
            return -1.0;
        }
        return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
#endif
    }

    std::chrono::steady_clock::time_point m_frameStart{};
    std::chrono::steady_clock::time_point m_cpuDone{};
    std::chrono::steady_clock::time_point m_gpuStart{};
    std::chrono::steady_clock::time_point m_gpuDone{};
    std::chrono::steady_clock::time_point m_lastFrameTime{};
    std::chrono::steady_clock::time_point m_lastPresentedTime{};
    std::chrono::steady_clock::time_point m_lastCpuCheckTime{};
    std::chrono::steady_clock::time_point m_lastDisplayUpdateTime{};
    std::chrono::steady_clock::time_point m_fpsWindowStart{};

    double m_lastProcessCpuSec{-1.0};
    int m_numProcessors{1};
    uint32_t m_fpsFrameCount{0};
    bool m_hasPreviousFrame{false};
    bool m_hasPresentedFrames{false};
    bool m_isIdle{false};

    float m_smoothedFps{0.0f};
    float m_smoothedCpuMs{0.0f};
    float m_smoothedGpuMs{0.0f};
    float m_cpuPercent{0.0f};

    float m_displayFps{0.0f};
    float m_displayCpuPercent{0.0f};
    float m_displayCpuTimeMs{0.0f};
    float m_displayGpuTimeMs{0.0f};
    float m_displayGpuBudgetPercent{0.0f};

    std::string m_statusString{"Initializing metrics..."};
};

} // namespace guikit
