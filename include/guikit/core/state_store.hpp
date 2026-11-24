#pragma once

#include "guikit/core/id.hpp"
#include "keel/core.h"
#include "keel/hash_map.h"
#include "keel/memory.h"

namespace guikit {

namespace detail {

struct StateKey {
    Id id;
    keel::TypeId type;

    constexpr bool operator==(const StateKey& other) const noexcept {
        return id == other.id && type == other.type;
    }
};

struct StateKeyHash {
    keel::u64 operator()(const StateKey& k) const noexcept {
        const keel::u64 h1 = keel::Hash<Id>{}(k.id);
        const keel::u64 h2 = keel::Hash<keel::TypeId>{}(k.type);
        return keel::hash_u64(h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2)));
    }
};

struct StoredState {
    void* ptr{nullptr};
    void (*destroy)(void*) noexcept {nullptr};
    void (*trim)(void*) noexcept {nullptr};
    keel::u64 lastFrameActive{0};
    keel::f64 lastTimeActiveSec{0.0};
    bool persistent{false};
    bool cachesTrimmed{false};

    StoredState() noexcept = default;

    template <typename T>
    static StoredState Create(T&& val, keel::u64 frameIndex, keel::f64 timeSec, bool isPersistent = false) {
        using Decayed = keel::Decay<T>;
        Decayed* p = keel::default_allocator()->create<Decayed>(keel::forward<T>(val));
        StoredState s;
        s.ptr = p;
        s.destroy = [](void* raw) noexcept {
            keel::default_allocator()->destroy(static_cast<Decayed*>(raw));
        };
        if constexpr (requires(Decayed& obj) { obj.TrimInactiveCaches(); }) {
            s.trim = [](void* raw) noexcept {
                static_cast<Decayed*>(raw)->TrimInactiveCaches();
            };
        }
        s.lastFrameActive = frameIndex;
        s.lastTimeActiveSec = timeSec;
        s.persistent = isPersistent;
        return s;
    }

    ~StoredState() noexcept {
        if (ptr && destroy) {
            destroy(ptr);
        }
    }

    StoredState(const StoredState&) = delete;
    StoredState& operator=(const StoredState&) = delete;

    StoredState(StoredState&& other) noexcept
        : ptr(other.ptr)
        , destroy(other.destroy)
        , trim(other.trim)
        , lastFrameActive(other.lastFrameActive)
        , lastTimeActiveSec(other.lastTimeActiveSec)
        , persistent(other.persistent)
        , cachesTrimmed(other.cachesTrimmed) {
        other.ptr = nullptr;
        other.destroy = nullptr;
        other.trim = nullptr;
    }

    StoredState& operator=(StoredState&& other) noexcept {
        if (this != &other) {
            if (ptr && destroy) {
                destroy(ptr);
            }
            ptr = other.ptr;
            destroy = other.destroy;
            trim = other.trim;
            lastFrameActive = other.lastFrameActive;
            lastTimeActiveSec = other.lastTimeActiveSec;
            persistent = other.persistent;
            cachesTrimmed = other.cachesTrimmed;
            other.ptr = nullptr;
            other.destroy = nullptr;
            other.trim = nullptr;
        }
        return *this;
    }
};

} // namespace detail

/// Type-erased, zero-RTTI widget state store keyed by `(Id, keel::TypeId)`.
/// Supports transient (GC'd after 600 frames / 10s idle) and persistent states, plus lazy cache trimming.
class StateStore {
public:
    StateStore() = default;
    ~StateStore() = default;
    StateStore(const StateStore&) = delete;
    StateStore& operator=(const StateStore&) = delete;

    template <typename T>
    [[nodiscard]] T& GetOrCreate(
        Id id,
        T defaultVal,
        keel::u64 frameIndex,
        keel::f64 timeSec,
        bool persistent = false
    ) {
        detail::StateKey key{id, keel::type_id<T>()};
        detail::StoredState* found = m_states.get(key);
        if (!found) {
            detail::StoredState& inserted = m_states.get_or_insert(
                key,
                detail::StoredState::Create<T>(keel::move(defaultVal), frameIndex, timeSec, persistent)
            );
            return *static_cast<T*>(inserted.ptr);
        }
        found->lastFrameActive = frameIndex;
        found->lastTimeActiveSec = timeSec;
        if (persistent) {
            found->persistent = true;
        }
        found->cachesTrimmed = false;
        return *static_cast<T*>(found->ptr);
    }

    template <typename T>
    [[nodiscard]] const T* Find(Id id) const {
        detail::StateKey key{id, keel::type_id<T>()};
        const detail::StoredState* found = m_states.get(key);
        if (!found) {
            return nullptr;
        }
        return static_cast<const T*>(found->ptr);
    }

    template <typename T>
    [[nodiscard]] T* FindMutable(Id id, keel::u64 frameIndex, keel::f64 timeSec) {
        detail::StateKey key{id, keel::type_id<T>()};
        detail::StoredState* found = m_states.get(key);
        if (!found) {
            return nullptr;
        }
        found->lastFrameActive = frameIndex;
        found->lastTimeActiveSec = timeSec;
        found->cachesTrimmed = false;
        return static_cast<T*>(found->ptr);
    }

    [[nodiscard]] keel::usize Size() const noexcept { return m_states.size(); }
    [[nodiscard]] bool HasUntrimmedInactive() const noexcept { return m_hasUntrimmedInactive; }

    /// Trims regenerable caches on idle states and prunes stale transient states.
    /// Returns true if any memory was trimmed or pruned.
    bool Maintain(
        keel::u64 frameIndex,
        keel::f64 currentTimeSec,
        keel::i32 repaintFrames,
        Id focusedId,
        Id activeId
    );

private:
    keel::HashMap<detail::StateKey, detail::StoredState, detail::StateKeyHash> m_states;
    keel::f64 m_lastCacheTrimSweepSec{0.0};
    keel::f64 m_lastTransientPruneSec{0.0};
    bool m_hasUntrimmedInactive{false};
};

} // namespace guikit
