#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace guikit {

/// High-performance linear arena allocator for per-frame scratch objects.
/// Guarantees O(1) allocation, zero heap churn during the frame, and O(1) reset.
class Allocator {
public:
    static constexpr size_t DefaultChunkSize = 64 * 1024; // 64 KB

    explicit Allocator(size_t chunkSize = DefaultChunkSize)
        : m_defaultChunkSize(chunkSize) {
        Grow(m_defaultChunkSize);
    }

    ~Allocator() noexcept {
        DestroyAll();
    }

    Allocator(const Allocator&) = delete;
    Allocator& operator=(const Allocator&) = delete;

    Allocator(Allocator&& other) noexcept
        : m_chunks(std::move(other.m_chunks))
        , m_currentChunkIndex(other.m_currentChunkIndex)
        , m_defaultChunkSize(other.m_defaultChunkSize)
        , m_dtorHead(other.m_dtorHead) {
        other.m_currentChunkIndex = 0;
        other.m_dtorHead = nullptr;
    }

    Allocator& operator=(Allocator&& other) noexcept {
        if (this != &other) {
            DestroyAll();
            m_chunks = std::move(other.m_chunks);
            m_currentChunkIndex = other.m_currentChunkIndex;
            m_defaultChunkSize = other.m_defaultChunkSize;
            m_dtorHead = other.m_dtorHead;
            other.m_currentChunkIndex = 0;
            other.m_dtorHead = nullptr;
        }
        return *this;
    }

    /// Allocates raw uninitialized memory with specified alignment.
    [[nodiscard]] void* AllocateRaw(size_t bytes, size_t alignment = alignof(std::max_align_t)) {
        if (bytes == 0) {
            return nullptr;
        }

        Chunk* chunk = &m_chunks[m_currentChunkIndex];
        uintptr_t current = reinterpret_cast<uintptr_t>(chunk->data.get() + chunk->offset);
        uintptr_t aligned = (current + alignment - 1) & ~(alignment - 1);
        size_t padding = aligned - current;

        if (chunk->offset + padding + bytes <= chunk->capacity) {
            chunk->offset += padding + bytes;
            return reinterpret_cast<void*>(aligned);
        }

        // Current chunk has insufficient space; move to next or allocate a new one
        const size_t nextCapacity = std::max(m_defaultChunkSize, bytes + alignment);
        if (m_currentChunkIndex + 1 < m_chunks.size()) {
            ++m_currentChunkIndex;
            chunk = &m_chunks[m_currentChunkIndex];
            chunk->offset = 0;
            if (chunk->capacity < bytes + alignment) {
                chunk->data = std::make_unique<std::byte[]>(nextCapacity);
                chunk->capacity = nextCapacity;
            }
        } else {
            Grow(nextCapacity);
            chunk = &m_chunks[m_currentChunkIndex];
        }

        current = reinterpret_cast<uintptr_t>(chunk->data.get() + chunk->offset);
        aligned = (current + alignment - 1) & ~(alignment - 1);
        padding = aligned - current;
        chunk->offset += padding + bytes;
        return reinterpret_cast<void*>(aligned);
    }

    /// Attempts to extend the most recent raw allocation in the active chunk in-place.
    /// Returns true if `ptr` sits at the top of the active chunk and has sufficient remaining capacity.
    [[nodiscard]] bool TryExtendRaw(void* ptr, size_t oldBytes, size_t newBytes) noexcept {
        if (ptr == nullptr || m_chunks.empty()) {
            return false;
        }
        if (newBytes <= oldBytes) {
            return true;
        }
        Chunk& chunk = m_chunks[m_currentChunkIndex];
        auto* bytePtr = static_cast<std::byte*>(ptr);
        if (bytePtr + oldBytes != chunk.data.get() + chunk.offset) {
            return false;
        }
        const size_t extra = newBytes - oldBytes;
        if (chunk.offset + extra > chunk.capacity) {
            return false;
        }
        chunk.offset += extra;
        return true;
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

    /// Invokes destructors for non-trivial allocations and resets all chunk offsets to zero.
    /// Does NOT free underlying arena chunk buffers.
    void Reset() noexcept {
        DestroyAll();
        m_currentChunkIndex = 0;
        for (auto& chunk : m_chunks) {
            chunk.offset = 0;
        }
    }

    /// Returns total bytes currently allocated across active chunks in this frame.
    [[nodiscard]] size_t GetTotalAllocatedBytes() const noexcept {
        size_t total = 0;
        for (size_t i = 0; i <= m_currentChunkIndex && i < m_chunks.size(); ++i) {
            total += m_chunks[i].offset;
        }
        return total;
    }

private:
    struct DtorNode {
        void* obj{nullptr};
        void (*destroy)(void*) noexcept {nullptr};
        DtorNode* next{nullptr};
    };

    struct Chunk {
        std::unique_ptr<std::byte[]> data;
        size_t capacity{0};
        size_t offset{0};
    };

    void DestroyAll() noexcept {
        for (DtorNode* n = m_dtorHead; n != nullptr; n = n->next) {
            n->destroy(n->obj);
        }
        m_dtorHead = nullptr;
    }

    void Grow(size_t capacity) {
        Chunk chunk;
        chunk.data = std::make_unique<std::byte[]>(capacity);
        chunk.capacity = capacity;
        chunk.offset = 0;
        m_chunks.push_back(std::move(chunk));
        m_currentChunkIndex = m_chunks.size() - 1;
    }

    std::vector<Chunk> m_chunks;
    size_t m_currentChunkIndex{0};
    size_t m_defaultChunkSize{DefaultChunkSize};
    DtorNode* m_dtorHead{nullptr};
};

using BumpAllocator = Allocator;

} // namespace guikit
