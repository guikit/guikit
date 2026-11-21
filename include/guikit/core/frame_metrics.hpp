#pragma once

#include "keel/ref.h"
#include "keel/string.h"

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
    void EndFrame(keel::f32 targetFps = 60.0f) noexcept;

    [[nodiscard]] keel::f32 GetFps() const noexcept { return m_displayFps; }
    [[nodiscard]] keel::f32 GetCpuPercent() const noexcept { return m_displayCpuPercent; }
    [[nodiscard]] keel::f32 GetCpuTimeMs() const noexcept { return m_displayCpuTimeMs; }
    [[nodiscard]] keel::f32 GetGpuTimeMs() const noexcept { return m_displayGpuTimeMs; }
    [[nodiscard]] keel::f32 GetGpuBudgetPercent() const noexcept { return m_displayGpuBudgetPercent; }
    [[nodiscard]] keel::usize GetRamUsageBytes() const noexcept { return m_ramBytes; }
    [[nodiscard]] keel::f32 GetRamUsageMb() const noexcept { return m_displayRamMb; }
    [[nodiscard]] bool IsIdle() const noexcept { return m_isIdle; }

    [[nodiscard]] keel::StringRef GetFpsString() const noexcept { return m_fpsString.as_ref(); }
    [[nodiscard]] keel::StringRef GetCpuString() const noexcept { return m_cpuString.as_ref(); }
    [[nodiscard]] keel::StringRef GetGpuString() const noexcept { return m_gpuString.as_ref(); }
    [[nodiscard]] keel::StringRef GetRamString() const noexcept { return m_ramString.as_ref(); }

    /// Returns human-readable status line, e.g. "60 FPS | CPU: 1.4% (1.80 ms) | GPU: 0.20 ms (2% budget) | RAM: 42.3 MB"
    [[nodiscard]] keel::StringRef GetStatusString() const noexcept {
        return m_statusString.as_ref();
    }

private:
    void UpdateProcessTelemetry(keel::u64 nowNs) noexcept;
    void RefreshDisplayStrings(keel::u64 nowNs, keel::f32 targetFps) noexcept;

    static keel::usize QueryProcessRamBytes() noexcept;
    static keel::f64 QueryProcessCpuSeconds() noexcept;

    keel::u64 m_frameStartNs{0};
    keel::u64 m_cpuDoneNs{0};
    keel::u64 m_gpuStartNs{0};
    keel::u64 m_gpuDoneNs{0};
    keel::u64 m_lastFrameTimeNs{0};
    keel::u64 m_lastFrameEndNs{0};
    keel::u64 m_lastPresentedTimeNs{0};
    keel::u64 m_lastCpuCheckTimeNs{0};
    keel::u64 m_lastDisplayUpdateTimeNs{0};
    keel::u64 m_fpsWindowStartNs{0};
    keel::u64 m_pendingWakeStartNs{0};

    keel::f64 m_lastProcessCpuSec{-1.0};
    keel::f64 m_accumulatedWorkSec{0.0};
    keel::i32 m_numProcessors{1};
    keel::u32 m_fpsFrameCount{0};
    bool m_hasPreviousFrame{false};
    bool m_hasPresentedFrames{false};
    bool m_isIdle{false};
    bool m_wokeFromIdle{false};
    bool m_pendingWakePresent{false};

    keel::f32 m_lastTargetFps{60.0f};
    keel::f32 m_pendingWakeCpuMs{0.0f};
    keel::f32 m_pendingWakeGpuMs{0.0f};
    keel::f32 m_smoothedFps{0.0f};
    keel::f32 m_smoothedCpuMs{0.0f};
    keel::f32 m_smoothedGpuMs{0.0f};
    keel::f32 m_cpuPercent{0.0f};

    keel::f32 m_displayFps{0.0f};
    keel::f32 m_displayCpuPercent{0.0f};
    keel::f32 m_displayCpuTimeMs{0.0f};
    keel::f32 m_displayGpuTimeMs{0.0f};
    keel::f32 m_displayGpuBudgetPercent{0.0f};
    keel::usize m_ramBytes{0};
    keel::f32 m_displayRamMb{0.0f};

    keel::FixedString<64> m_fpsString{"0 FPS (Idle)"};
    keel::FixedString<64> m_cpuString{"CPU: 0.00 ms (0.0%)"};
    keel::FixedString<64> m_gpuString{"GPU: 0.00 ms (0% budget)"};
    keel::FixedString<64> m_ramString{"RAM: 0.0 MB"};
    keel::FixedString<192> m_statusString{"Initializing metrics..."};
};

} // namespace guikit
