#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#include "keel/arena.h"
#include "keel/memory.h"

namespace guikit {

/// High-performance linear arena allocator for per-frame scratch objects, backed by `keel::Arena`.
/// Reserves virtual address space upfront and commits 64 KB pages on demand with O(1) bump allocation and O(1) reset.
class Allocator {
public:
    static constexpr size_t DefaultChunkSize = 64 * 1024; // 64 KB

    explicit Allocator(size_t chunkSize = DefaultChunkSize) noexcept
        : m_arena(std::max<keel::isize>(keel::Arena::kDefaultReserveSize, static_cast<keel::isize>(chunkSize) * 16)) {}

    ~Allocator() noexcept {
        DestroyAll();
    }

    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;

    Allocator(Allocator&& other) noexcept
        : m_arena(std::move(other.m_arena))
        , m_dtorHead(other.m_dtorHead) {
        other.m_dtorHead = nullptr;
    }

    Allocator& operator=(Allocator&& other) noexcept {
        if (this != &other) {
            DestroyAll();
            m_arena = std::move(other.m_arena);
            m_dtorHead = other.m_dtorHead;
            other.m_dtorHead = nullptr;
        }
        return *this;
    }

    /// Allocates raw uninitialized memory with specified alignment.
    [[nodiscard]] void* AllocateRaw(size_t bytes, size_t alignment = alignof(std::max_align_t)) {
        if (bytes == 0) {
            return nullptr;
        }
        const size_t safeAlign = std::max<size_t>(alignment, 1);
        return m_arena.alloc(static_cast<keel::isize>(bytes), static_cast<keel::isize>(safeAlign));
    }

    /// Attempts to extend the most recent raw allocation at the top of the arena in-place.
    [[nodiscard]] bool TryExtendRaw(void* ptr, size_t oldBytes, size_t newBytes) noexcept {
        return m_arena.try_extend(ptr, static_cast<keel::isize>(oldBytes), static_cast<keel::isize>(newBytes));
    }

    /// Allocates and constructs an object in-place.
    /// Non-trivially-destructible objects are registered in an intrusive arena list
    /// so their destructors run on Reset() or ~Allocator().
    template <typename T, typename... Args>
    [[nodiscard]] T* Allocate(Args&&... args) {
        void* ptr = AllocateRaw(sizeof(T), alignof(T));
        if constexpr (std::is_trivially_destructible_v<T>) {
            return ::new (ptr) T(std::forward<Args>(args)...);
        } else {
            auto* node = static_cast<DtorNode*>(AllocateRaw(sizeof(DtorNode), alignof(DtorNode)));
            T* obj = ::new (ptr) T(std::forward<Args>(args)...);
            node->obj = obj;
            node->destroy = [](void* p) noexcept { static_cast<T*>(p)->~T(); };
            node->next = m_dtorHead;
            m_dtorHead = node;
            return obj;
        }
    }

    /// Allocates and default-constructs a contiguous array of `count` elements in the arena.
    /// Non-trivially-destructible element types are destroyed in reverse order on Reset() or ~Allocator().
    template <typename T>
    [[nodiscard]] T* AllocateArray(size_t count) {
        if (count == 0) {
            return nullptr;
        }
        T* arr = static_cast<T*>(AllocateRaw(sizeof(T) * count, alignof(T)));
        if constexpr (std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < count; ++i) {
                ::new (&arr[i]) T{};
            }
            return arr;
        } else {
            struct ArrayDtorRecord {
                DtorNode node{};
                T* data{nullptr};
                size_t count{0};
            };
            auto* rec = static_cast<ArrayDtorRecord*>(AllocateRaw(sizeof(ArrayDtorRecord), alignof(ArrayDtorRecord)));
            size_t constructed = 0;
            try {
                for (; constructed < count; ++constructed) {
                    ::new (&arr[constructed]) T{};
                }
            } catch (...) {
                for (size_t i = constructed; i > 0; --i) {
                    arr[i - 1].~T();
                }
                throw;
            }
            rec->data = arr;
            rec->count = count;
            rec->node.obj = rec;
            rec->node.destroy = [](void* p) noexcept {
                auto* r = static_cast<ArrayDtorRecord*>(p);
                for (size_t i = r->count; i > 0; --i) {
                    r->data[i - 1].~T();
                }
            };
            rec->node.next = m_dtorHead;
            m_dtorHead = &rec->node;
            return arr;
        }
    }

    /// Allocates and default-constructs a contiguous span of `count` elements in the arena.
    template <typename T>
    [[nodiscard]] std::span<T> AllocateSpan(size_t count) {
        return std::span<T>(AllocateArray<T>(count), count);
    }

    /// Invokes destructors for non-trivial allocations and resets the arena cursor to zero in O(1).
    /// Does NOT uncommit or release underlying virtual memory pages.
    void Reset() noexcept {
        DestroyAll();
        m_arena.reset();
    }

    /// Returns total bytes currently used in this arena frame.
    [[nodiscard]] size_t GetTotalAllocatedBytes() const noexcept {
        return static_cast<size_t>(m_arena.used());
    }

    /// Returns a light-weight `keel::Allocator` handle backed directly by the underlying `keel::Arena`.
    [[nodiscard]] keel::Allocator AsKeelAllocator() noexcept {
        return *m_arena.allocator();
    }

    [[nodiscard]] keel::Arena& GetKeelArena() noexcept { return m_arena; }
    [[nodiscard]] const keel::Arena& GetKeelArena() const noexcept { return m_arena; }

private:
    struct DtorNode {
        void* obj{nullptr};
        void (*destroy)(void*) noexcept {nullptr};
        DtorNode* next{nullptr};
    };

    void DestroyAll() noexcept {
        for (DtorNode* n = m_dtorHead; n != nullptr; n = n->next) {
            n->destroy(n->obj);
        }
        m_dtorHead = nullptr;
    }

    keel::Arena m_arena;
    DtorNode* m_dtorHead{nullptr};
};

using BumpAllocator = Allocator;

} // namespace guikit
