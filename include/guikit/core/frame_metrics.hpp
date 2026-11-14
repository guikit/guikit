#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace guikit {

/// Lightweight, cross-platform performance metrics tracker for immediate mode frame loops.
/// Computes smoothed FPS, process CPU utilization percentage, CPU frame work time,
/// and GPU transfer/render time with frame budget percentage.
class FrameMetrics {
public:
    FrameMetrics() noexcept;

    /// Call at the very beginning of CPU frame work.
    void BeginFrame() noexcept;

    /// Call immediately after presenting the frame buffer to the display (e.g. after SDL_RenderPresent).
    void MarkFramePresented() noexcept;

    /// Call immediately after CPU work (UI layout, immediate mode execution, and software rasterization).
    void MarkCpuDone() noexcept;

    /// Call right before submitting texture updates and draw commands to the GPU.
    void MarkGpuStart() noexcept;

    /// Call right after GPU texture upload and render commands are submitted.
    void MarkGpuDone() noexcept;

    /// Call at the end of the frame (after present / delay).
    void EndFrame(float targetFps = 60.0f) noexcept;

    [[nodiscard]] float GetFps() const noexcept { return m_displayFps; }
    [[nodiscard]] float GetCpuPercent() const noexcept { return m_displayCpuPercent; }
    [[nodiscard]] float GetCpuTimeMs() const noexcept { return m_displayCpuTimeMs; }
    [[nodiscard]] float GetGpuTimeMs() const noexcept { return m_displayGpuTimeMs; }
    [[nodiscard]] float GetGpuBudgetPercent() const noexcept { return m_displayGpuBudgetPercent; }
    [[nodiscard]] size_t GetRamUsageBytes() const noexcept { return m_ramBytes; }
    [[nodiscard]] float GetRamUsageMb() const noexcept { return m_displayRamMb; }
    [[nodiscard]] bool IsIdle() const noexcept { return m_isIdle; }

    [[nodiscard]] std::string_view GetFpsString() const noexcept { return m_fpsString; }
    [[nodiscard]] std::string_view GetCpuString() const noexcept { return m_cpuString; }
    [[nodiscard]] std::string_view GetGpuString() const noexcept { return m_gpuString; }
    [[nodiscard]] std::string_view GetRamString() const noexcept { return m_ramString; }

    /// Returns human-readable status line, e.g. "60 FPS | CPU: 1.4% (1.80 ms) | GPU: 0.20 ms (2% budget) | RAM: 42.3 MB"
    [[nodiscard]] std::string_view GetStatusString() const noexcept {
        return m_statusString;
    }

private:
    void UpdateProcessTelemetry(std::chrono::steady_clock::time_point now) noexcept;
    void RefreshDisplayStrings(std::chrono::steady_clock::time_point now, float targetFps) noexcept;

    static size_t QueryProcessRamBytes() noexcept;
    static double QueryProcessCpuSeconds() noexcept;

    std::chrono::steady_clock::time_point m_frameStart{};
    std::chrono::steady_clock::time_point m_cpuDone{};
    std::chrono::steady_clock::time_point m_gpuStart{};
    std::chrono::steady_clock::time_point m_gpuDone{};
    std::chrono::steady_clock::time_point m_lastFrameTime{};
    std::chrono::steady_clock::time_point m_lastFrameEnd{};
    std::chrono::steady_clock::time_point m_lastPresentedTime{};
    std::chrono::steady_clock::time_point m_lastCpuCheckTime{};
    std::chrono::steady_clock::time_point m_lastDisplayUpdateTime{};
    std::chrono::steady_clock::time_point m_fpsWindowStart{};
    std::chrono::steady_clock::time_point m_pendingWakeStart{};

    double m_lastProcessCpuSec{-1.0};
    double m_accumulatedWorkSec{0.0};
    int m_numProcessors{1};
    uint32_t m_fpsFrameCount{0};
    bool m_hasPreviousFrame{false};
    bool m_hasPresentedFrames{false};
    bool m_isIdle{false};
    bool m_wokeFromIdle{false};
    bool m_pendingWakePresent{false};

    float m_lastTargetFps{60.0f};
    float m_pendingWakeCpuMs{0.0f};
    float m_pendingWakeGpuMs{0.0f};
    float m_smoothedFps{0.0f};
    float m_smoothedCpuMs{0.0f};
    float m_smoothedGpuMs{0.0f};
    float m_cpuPercent{0.0f};

    float m_displayFps{0.0f};
    float m_displayCpuPercent{0.0f};
    float m_displayCpuTimeMs{0.0f};
    float m_displayGpuTimeMs{0.0f};
    float m_displayGpuBudgetPercent{0.0f};
    size_t m_ramBytes{0};
    float m_displayRamMb{0.0f};

    std::string m_fpsString{"0 FPS (Idle)"};
    std::string m_cpuString{"CPU: 0.00 ms (0.0%)"};
    std::string m_gpuString{"GPU: 0.00 ms (0% budget)"};
    std::string m_ramString{"RAM: 0.0 MB"};
    std::string m_statusString{"Initializing metrics..."};
};

} // namespace guikit
