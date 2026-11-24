#include "guikit/core/state_store.hpp"
#include "keel/small_array.h"

namespace guikit {

bool StateStore::Maintain(
    keel::u64 frameIndex,
    keel::f64 currentTimeSec,
    keel::i32 repaintFrames,
    Id focusedId,
    Id activeId
) {
    bool trimmedAny = false;
    if (currentTimeSec < m_lastCacheTrimSweepSec) {
        m_lastCacheTrimSweepSec = currentTimeSec;
    }
    if (currentTimeSec < m_lastTransientPruneSec) {
        m_lastTransientPruneSec = currentTimeSec;
    }

    // Trim regenerable offscreen pixel/tile/layout caches from inactive widget states
    // (idle > 120 frames in continuous loops, or >= 1.5s + > 3 frames in reactive idle mode)
    // while keeping user-facing persistent state (camera, zoom, selections, markers, undo history) intact.
    const bool checkTrimNow = (frameIndex > 0) &&
        ((frameIndex % 60 == 0) ||
         repaintFrames <= 1 ||
         (currentTimeSec - m_lastCacheTrimSweepSec >= 0.5));
    if (checkTrimNow) {
        m_lastCacheTrimSweepSec = currentTimeSec;
        bool stillHasUntrimmed = false;
        for (auto [key, state] : m_states) {
            if (!state.trim || state.cachesTrimmed) {
                continue;
            }
            if (state.lastFrameActive > frameIndex) {
                state.lastFrameActive = frameIndex;
            }
            if (currentTimeSec < state.lastTimeActiveSec) {
                state.lastTimeActiveSec = currentTimeSec;
            }
            const keel::u64 idleFrames = frameIndex - state.lastFrameActive;
            if (idleFrames == 0 || key.id == focusedId || key.id == activeId) {
                continue;
            }
            const keel::f64 idleSec = currentTimeSec - state.lastTimeActiveSec;
            if (idleFrames > 120 || (idleFrames > 3 && idleSec >= 1.5)) {
                state.trim(state.ptr);
                state.cachesTrimmed = true;
                trimmedAny = true;
            } else {
                stillHasUntrimmed = true;
            }
        }
        m_hasUntrimmedInactive = stillHasUntrimmed;
    }

    // Periodically prune stale transient states (every 600 frames or >= 10s in reactive mode);
    // persistent widget states (tabs, trees, tables, scenes, scroll offsets, undo stacks) survive until Context destruction.
    if (frameIndex > 0 && ((frameIndex % 600 == 0) || (currentTimeSec - m_lastTransientPruneSec >= 10.0))) {
        m_lastTransientPruneSec = currentTimeSec;
        keel::SmallArray<detail::StateKey, 16> staleKeys;
        for (const auto& [key, state] : m_states) {
            if (state.persistent || key.id == focusedId || key.id == activeId) {
                continue;
            }
            const keel::u64 idleFrames = (frameIndex > state.lastFrameActive)
                ? (frameIndex - state.lastFrameActive)
                : 0;
            const keel::f64 idleSec = (currentTimeSec > state.lastTimeActiveSec)
                ? (currentTimeSec - state.lastTimeActiveSec)
                : 0.0;
            if (idleFrames > 600 || (idleFrames > 5 && idleSec >= 10.0)) {
                staleKeys.push_back(key);
            }
        }
        for (const auto& key : staleKeys) {
            (void)m_states.remove(key);
            trimmedAny = true;
        }
    }

    return trimmedAny;
}

} // namespace guikit
