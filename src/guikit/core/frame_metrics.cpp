#include "guikit/core/frame_metrics.hpp"

#include "keel/core.h"
#include "keel/format.h"
#include "keel/process.h"
#include "keel/thread_pool.h"
#include "keel/time.h"

namespace guikit {

namespace {

[[nodiscard]] keel::i32 QueryNumProcessors() noexcept {
    return keel::max(1, static_cast<keel::i32>(keel::hardware_concurrency()));
}

[[nodiscard]] constexpr double DeltaSeconds(keel::u64 endNs, keel::u64 startNs) noexcept {
    return (endNs > startNs) ? (static_cast<double>(endNs - startNs) * 1e-9) : 0.0;
}

[[nodiscard]] constexpr float DeltaMillisecondsF(keel::u64 endNs, keel::u64 startNs) noexcept {
    return (endNs > startNs) ? static_cast<float>(static_cast<double>(endNs - startNs) * 1e-6) : 0.0f;
}

} // namespace

FrameMetrics::FrameMetrics() noexcept {
    m_numProcessors = QueryNumProcessors();
    const keel::u64 nowNs = keel::monotonic_ns();
    m_lastCpuCheckTimeNs = nowNs;
    m_lastProcessCpuSec = QueryProcessCpuSeconds();
    m_ramBytes = QueryProcessRamBytes();
    m_displayRamMb = static_cast<float>(static_cast<double>(m_ramBytes) / (1024.0 * 1024.0));
    m_lastDisplayUpdateTimeNs = nowNs;
    m_lastFrameTimeNs = nowNs;
    m_lastFrameEndNs = nowNs;
    m_lastPresentedTimeNs = nowNs;
    m_fpsWindowStartNs = nowNs;
}

void FrameMetrics::BeginFrame() noexcept {
    const keel::u64 nowNs = keel::monotonic_ns();
    m_wokeFromIdle = false;
    if (m_hasPreviousFrame) {
        const keel::u64 betweenFramesNs = (nowNs > m_lastFrameEndNs) ? (nowNs - m_lastFrameEndNs) : 0ULL;
        const float sleepSec = static_cast<float>(static_cast<double>(betweenFramesNs) * 1e-9);
        if (sleepSec > 0.25f) {
            m_wokeFromIdle = true;
            m_pendingWakePresent = false;
            m_isIdle = true;
            m_smoothedFps = 0.0f;
            m_smoothedCpuMs = 0.0f;
            m_smoothedGpuMs = 0.0f;
            m_fpsFrameCount = 0;
            m_fpsWindowStartNs = nowNs;
            UpdateProcessTelemetry(nowNs);
            RefreshDisplayStrings(nowNs, m_lastTargetFps);
        } else {
            if (sleepSec > 0.035f) {
                m_fpsWindowStartNs += betweenFramesNs;
            }
            if (m_pendingWakePresent) {
                m_isIdle = false;
                m_smoothedCpuMs = m_pendingWakeCpuMs;
                m_smoothedGpuMs = m_pendingWakeGpuMs;
                m_fpsWindowStartNs = m_pendingWakeStartNs;
                m_fpsFrameCount = 1;
                m_pendingWakePresent = false;
            }
        }
    }
    m_frameStartNs = nowNs;
    m_lastFrameTimeNs = nowNs;
    m_hasPreviousFrame = true;
}

void FrameMetrics::MarkFramePresented() noexcept {
    const keel::u64 nowNs = keel::monotonic_ns();
    m_hasPresentedFrames = true;
    if (m_wokeFromIdle) {
        m_pendingWakePresent = true;
        m_pendingWakeStartNs = m_frameStartNs;
        return;
    }
    m_lastPresentedTimeNs = nowNs;
    m_isIdle = false;
    ++m_fpsFrameCount;
}

void FrameMetrics::MarkCpuDone() noexcept {
    m_cpuDoneNs = keel::monotonic_ns();
    const float workMs = DeltaMillisecondsF(m_cpuDoneNs, m_frameStartNs);
    m_accumulatedWorkSec += static_cast<double>(keel::max(0.0f, workMs)) * 1e-3;
    if (m_wokeFromIdle) {
        m_pendingWakeCpuMs = workMs;
        return;
    }
    if (m_smoothedCpuMs <= 0.0f) {
        m_smoothedCpuMs = workMs;
    } else {
        const float alpha = (workMs < m_smoothedCpuMs) ? 0.30f : 0.15f;
        m_smoothedCpuMs = m_smoothedCpuMs * (1.0f - alpha) + workMs * alpha;
    }
}

void FrameMetrics::MarkGpuStart() noexcept {
    m_gpuStartNs = keel::monotonic_ns();
}

void FrameMetrics::MarkGpuDone() noexcept {
    m_gpuDoneNs = keel::monotonic_ns();
    const float gpuMs = DeltaMillisecondsF(m_gpuDoneNs, m_gpuStartNs);
    m_accumulatedWorkSec += static_cast<double>(keel::max(0.0f, gpuMs)) * 1e-3;
    if (m_wokeFromIdle) {
        m_pendingWakeGpuMs = gpuMs;
        return;
    }
    if (m_smoothedGpuMs <= 0.0f) {
        m_smoothedGpuMs = gpuMs;
    } else {
        const float alpha = (gpuMs < m_smoothedGpuMs) ? 0.30f : 0.15f;
        m_smoothedGpuMs = m_smoothedGpuMs * (1.0f - alpha) + gpuMs * alpha;
    }
}

void FrameMetrics::EndFrame(float targetFps) noexcept {
    const keel::u64 nowNs = keel::monotonic_ns();
    m_lastFrameEndNs = nowNs;
    m_lastTargetFps = targetFps;

    if (m_hasPresentedFrames && !m_wokeFromIdle) {
        const float idleElapsed = static_cast<float>(DeltaSeconds(nowNs, m_lastPresentedTimeNs));
        if (idleElapsed > 0.25f) {
            m_isIdle = true;
            m_smoothedFps = 0.0f;
            m_smoothedCpuMs = 0.0f;
            m_smoothedGpuMs = 0.0f;
            m_fpsFrameCount = 0;
            m_fpsWindowStartNs = nowNs;
        } else {
            m_isIdle = false;
            const float fpsElapsed = static_cast<float>(DeltaSeconds(nowNs, m_fpsWindowStartNs));
            const float windowThreshold = (m_smoothedFps <= 0.0f) ? 0.20f : 0.30f;
            if (fpsElapsed >= windowThreshold && m_fpsFrameCount > 0) {
                const float measuredFps = static_cast<float>(m_fpsFrameCount) / fpsElapsed;
                if (m_smoothedFps <= 0.0f) {
                    m_smoothedFps = measuredFps;
                } else {
                    m_smoothedFps = m_smoothedFps * 0.70f + measuredFps * 0.30f;
                }
                m_fpsFrameCount = 0;
                m_fpsWindowStartNs = nowNs;
            }
        }
    } else {
        m_isIdle = true;
        m_smoothedFps = 0.0f;
        m_fpsWindowStartNs = nowNs;
    }

    UpdateProcessTelemetry(nowNs);

    const double displayElapsed = DeltaSeconds(nowNs, m_lastDisplayUpdateTimeNs);
    if (displayElapsed >= 0.3 || m_displayFps <= 0.0f || m_isIdle) {
        RefreshDisplayStrings(nowNs, targetFps);
    }
}

void FrameMetrics::UpdateProcessTelemetry(keel::u64 nowNs) noexcept {
    const double elapsedCpuSec = DeltaSeconds(nowNs, m_lastCpuCheckTimeNs);
    if (elapsedCpuSec >= 0.5) {
        const double curCpuSec = QueryProcessCpuSeconds();
        if (curCpuSec >= 0.0 && m_lastProcessCpuSec >= 0.0) {
            const double deltaCpu = curCpuSec - m_lastProcessCpuSec;
            const float pct = static_cast<float>((deltaCpu / (elapsedCpuSec * m_numProcessors)) * 100.0);
            m_cpuPercent = keel::clamp(pct, 0.0f, 100.0f);
        } else if (curCpuSec < 0.0) {
            const float pct = static_cast<float>((m_accumulatedWorkSec / elapsedCpuSec) * 100.0);
            m_cpuPercent = m_isIdle ? 0.0f : keel::clamp(pct, 0.0f, 100.0f);
        }
        m_lastProcessCpuSec = curCpuSec;
        m_accumulatedWorkSec = 0.0;
        m_ramBytes = QueryProcessRamBytes();
        m_lastCpuCheckTimeNs = nowNs;
    }
}

void FrameMetrics::RefreshDisplayStrings(keel::u64 nowNs, float targetFps) noexcept {
    if (m_ramBytes == 0) {
        m_ramBytes = QueryProcessRamBytes();
    }
    m_displayFps = m_isIdle ? 0.0f : m_smoothedFps;
    m_displayCpuPercent = m_cpuPercent;
    m_displayCpuTimeMs = m_isIdle ? 0.0f : m_smoothedCpuMs;
    m_displayGpuTimeMs = m_isIdle ? 0.0f : m_smoothedGpuMs;
    m_displayRamMb = static_cast<float>(static_cast<double>(m_ramBytes) / (1024.0 * 1024.0));

    const float targetFrameMs = (targetFps > 0.0f) ? (1000.0f / targetFps) : 16.67f;
    m_displayGpuBudgetPercent = keel::clamp((m_displayGpuTimeMs / targetFrameMs) * 100.0f, 0.0f, 100.0f);

    if (m_isIdle) {
        m_fpsString = "0 FPS (Idle)";
    } else {
        m_fpsString = keel::format_fixed<64>("{:.0f} FPS", m_displayFps);
    }
    m_cpuString = keel::format_fixed<64>(
        "CPU: {:.2f} ms ({:.1f}%)",
        m_displayCpuTimeMs,
        m_displayCpuPercent
    );
    m_gpuString = keel::format_fixed<64>(
        "GPU: {:.2f} ms ({:.0f}% budget)",
        m_displayGpuTimeMs,
        m_displayGpuBudgetPercent
    );
    m_ramString = keel::format_fixed<64>(
        "RAM: {:.1f} MB",
        m_displayRamMb
    );

    m_statusString = keel::format_fixed<192>(
        "{} | {} | {} | {}",
        m_fpsString,
        m_cpuString,
        m_gpuString,
        m_ramString
    );
    m_lastDisplayUpdateTimeNs = nowNs;
}

keel::usize FrameMetrics::QueryProcessRamBytes() noexcept {
    return static_cast<keel::usize>(keel::process_rss_bytes());
}

double FrameMetrics::QueryProcessCpuSeconds() noexcept {
    const keel::u64 cpuNs = keel::process_cpu_time_ns();
    if (cpuNs == 0u) {
        return -1.0;
    }
    return static_cast<double>(cpuNs) * 1e-9;
}

} // namespace guikit
