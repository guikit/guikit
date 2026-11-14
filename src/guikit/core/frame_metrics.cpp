#include "guikit/core/frame_metrics.hpp"

#include <algorithm>
#include <cstdio>
#include <thread>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <psapi.h>
#elif defined(__APPLE__)
    #include <ctime>
    #include <mach/mach.h>
    #include <unistd.h>
#elif defined(__EMSCRIPTEN__)
    #include <malloc.h>
#else
    #include <ctime>
    #include <unistd.h>
#endif

namespace guikit {

FrameMetrics::FrameMetrics() noexcept {
    m_numProcessors = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
    m_lastCpuCheckTime = std::chrono::steady_clock::now();
    m_lastProcessCpuSec = QueryProcessCpuSeconds();
    m_ramBytes = QueryProcessRamBytes();
    m_displayRamMb = static_cast<float>(static_cast<double>(m_ramBytes) / (1024.0 * 1024.0));
    m_lastDisplayUpdateTime = m_lastCpuCheckTime;
    m_lastFrameTime = m_lastCpuCheckTime;
    m_lastFrameEnd = m_lastCpuCheckTime;
    m_lastPresentedTime = m_lastCpuCheckTime;
    m_fpsWindowStart = m_lastCpuCheckTime;
}

void FrameMetrics::BeginFrame() noexcept {
    const auto now = std::chrono::steady_clock::now();
    m_wokeFromIdle = false;
    if (m_hasPreviousFrame) {
        const auto betweenFrames = now - m_lastFrameEnd;
        const float sleepSec = std::chrono::duration<float>(betweenFrames).count();
        if (sleepSec > 0.25f) {
            m_wokeFromIdle = true;
            m_pendingWakePresent = false;
            m_isIdle = true;
            m_smoothedFps = 0.0f;
            m_smoothedCpuMs = 0.0f;
            m_smoothedGpuMs = 0.0f;
            m_fpsFrameCount = 0;
            m_fpsWindowStart = now;
            UpdateProcessTelemetry(now);
            RefreshDisplayStrings(now, m_lastTargetFps);
        } else {
            if (sleepSec > 0.035f) {
                m_fpsWindowStart += betweenFrames;
            }
            if (m_pendingWakePresent) {
                m_isIdle = false;
                m_smoothedCpuMs = m_pendingWakeCpuMs;
                m_smoothedGpuMs = m_pendingWakeGpuMs;
                m_fpsWindowStart = m_pendingWakeStart;
                m_fpsFrameCount = 1;
                m_pendingWakePresent = false;
            }
        }
    }
    m_frameStart = now;
    m_lastFrameTime = now;
    m_hasPreviousFrame = true;
}

void FrameMetrics::MarkFramePresented() noexcept {
    const auto now = std::chrono::steady_clock::now();
    m_hasPresentedFrames = true;
    if (m_wokeFromIdle) {
        m_pendingWakePresent = true;
        m_pendingWakeStart = m_frameStart;
        return;
    }
    m_lastPresentedTime = now;
    m_isIdle = false;
    ++m_fpsFrameCount;
}

void FrameMetrics::MarkCpuDone() noexcept {
    m_cpuDone = std::chrono::steady_clock::now();
    const float workMs = std::chrono::duration<float, std::milli>(m_cpuDone - m_frameStart).count();
    m_accumulatedWorkSec += static_cast<double>(std::max(0.0f, workMs)) * 1e-3;
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
    m_gpuStart = std::chrono::steady_clock::now();
}

void FrameMetrics::MarkGpuDone() noexcept {
    m_gpuDone = std::chrono::steady_clock::now();
    const float gpuMs = std::chrono::duration<float, std::milli>(m_gpuDone - m_gpuStart).count();
    m_accumulatedWorkSec += static_cast<double>(std::max(0.0f, gpuMs)) * 1e-3;
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
    const auto now = std::chrono::steady_clock::now();
    m_lastFrameEnd = now;
    m_lastTargetFps = targetFps;

    if (m_hasPresentedFrames && !m_wokeFromIdle) {
        const float idleElapsed = std::chrono::duration<float>(now - m_lastPresentedTime).count();
        if (idleElapsed > 0.25f) {
            m_isIdle = true;
            m_smoothedFps = 0.0f;
            m_smoothedCpuMs = 0.0f;
            m_smoothedGpuMs = 0.0f;
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

    UpdateProcessTelemetry(now);

    const double displayElapsed = std::chrono::duration<double>(now - m_lastDisplayUpdateTime).count();
    if (displayElapsed >= 0.3 || m_displayFps <= 0.0f || m_isIdle) {
        RefreshDisplayStrings(now, targetFps);
    }
}

void FrameMetrics::UpdateProcessTelemetry(std::chrono::steady_clock::time_point now) noexcept {
    const double elapsedCpuSec = std::chrono::duration<double>(now - m_lastCpuCheckTime).count();
    if (elapsedCpuSec >= 0.5) {
#if defined(__EMSCRIPTEN__)
        const float pct = static_cast<float>((m_accumulatedWorkSec / elapsedCpuSec) * 100.0);
        m_cpuPercent = m_isIdle ? 0.0f : std::clamp(pct, 0.0f, 100.0f);
#else
        const double curCpuSec = QueryProcessCpuSeconds();
        if (curCpuSec >= 0.0 && m_lastProcessCpuSec >= 0.0) {
            const double deltaCpu = curCpuSec - m_lastProcessCpuSec;
            const float pct = static_cast<float>((deltaCpu / (elapsedCpuSec * m_numProcessors)) * 100.0);
            m_cpuPercent = std::clamp(pct, 0.0f, 100.0f);
        }
        m_lastProcessCpuSec = curCpuSec;
#endif
        m_accumulatedWorkSec = 0.0;
        m_ramBytes = QueryProcessRamBytes();
        m_lastCpuCheckTime = now;
    }
}

void FrameMetrics::RefreshDisplayStrings(std::chrono::steady_clock::time_point now, float targetFps) noexcept {
    if (m_ramBytes == 0) {
        m_ramBytes = QueryProcessRamBytes();
    }
    m_displayFps = m_isIdle ? 0.0f : m_smoothedFps;
    m_displayCpuPercent = m_cpuPercent;
    m_displayCpuTimeMs = m_isIdle ? 0.0f : m_smoothedCpuMs;
    m_displayGpuTimeMs = m_isIdle ? 0.0f : m_smoothedGpuMs;
    m_displayRamMb = static_cast<float>(static_cast<double>(m_ramBytes) / (1024.0 * 1024.0));

    const float targetFrameMs = (targetFps > 0.0f) ? (1000.0f / targetFps) : 16.67f;
    m_displayGpuBudgetPercent = std::clamp((m_displayGpuTimeMs / targetFrameMs) * 100.0f, 0.0f, 100.0f);

    char fpsBuf[32];
    char cpuBuf[48];
    char gpuBuf[48];
    char ramBuf[32];

    if (m_isIdle) {
        std::snprintf(fpsBuf, sizeof(fpsBuf), "0 FPS (Idle)");
    } else {
        std::snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS", m_displayFps);
    }
    std::snprintf(
        cpuBuf,
        sizeof(cpuBuf),
        "CPU: %.2f ms (%.1f%%)",
        m_displayCpuTimeMs,
        m_displayCpuPercent
    );
    std::snprintf(
        gpuBuf,
        sizeof(gpuBuf),
        "GPU: %.2f ms (%.0f%% budget)",
        m_displayGpuTimeMs,
        m_displayGpuBudgetPercent
    );
    std::snprintf(
        ramBuf,
        sizeof(ramBuf),
        "RAM: %.1f MB",
        m_displayRamMb
    );

    m_fpsString = fpsBuf;
    m_cpuString = cpuBuf;
    m_gpuString = gpuBuf;
    m_ramString = ramBuf;

    char buf[160];
    std::snprintf(
        buf,
        sizeof(buf),
        "%s | %s | %s | %s",
        fpsBuf,
        cpuBuf,
        gpuBuf,
        ramBuf
    );
    m_statusString = buf;
    m_lastDisplayUpdateTime = now;
}

size_t FrameMetrics::QueryProcessRamBytes() noexcept {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<size_t>(pmc.WorkingSetSize);
    }
    return 0;
#elif defined(__APPLE__)
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        return static_cast<size_t>(info.resident_size);
    }
    return 0;
#elif defined(__EMSCRIPTEN__)
    const struct mallinfo mi = mallinfo();
    if (mi.uordblks > 0) {
        return static_cast<size_t>(static_cast<unsigned int>(mi.uordblks));
    }
    return static_cast<size_t>(__builtin_wasm_memory_size(0)) * 65536ULL;
#else
    FILE* fp = std::fopen("/proc/self/statm", "r");
    if (!fp) {
        return 0;
    }
    unsigned long totalPages = 0;
    unsigned long rssPages = 0;
    const int matched = std::fscanf(fp, "%lu %lu", &totalPages, &rssPages);
    std::fclose(fp);
    if (matched == 2) {
        const long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize > 0) {
            return static_cast<size_t>(rssPages) * static_cast<size_t>(pageSize);
        }
    }
    return 0;
#endif
}

double FrameMetrics::QueryProcessCpuSeconds() noexcept {
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
#elif defined(__EMSCRIPTEN__)
    return 0.0;
#else
    timespec ts{};
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) != 0) {
        return -1.0;
    }
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) * 1e-9;
#endif
}

} // namespace guikit
