#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
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
        if (m_currentChunkIndex + 1 < m_chunks.size()) {
            ++m_currentChunkIndex;
            chunk = &m_chunks[m_currentChunkIndex];
            chunk->offset = 0;
        } else {
            const size_t nextCapacity = std::max(m_defaultChunkSize, bytes + alignment);
            Grow(nextCapacity);
            chunk = &m_chunks[m_currentChunkIndex];
        }

        current = reinterpret_cast<uintptr_t>(chunk->data.get() + chunk->offset);
        aligned = (current + alignment - 1) & ~(alignment - 1);
        padding = aligned - current;
        chunk->offset += padding + bytes;
        return reinterpret_cast<void*>(aligned);
    }

    /// Allocates and constructs an object in-place.
    /// Non-trivially-destructible objects are registered in an intrusive arena list
    /// so their destructors run on Reset() or ~Allocator().
    template <typename T, typename... Args>
    [[nodiscard]] T* Allocate(Args&&... args) {
        void* ptr = AllocateRaw(sizeof(T), alignof(T));
        T* obj = ::new (ptr) T(std::forward<Args>(args)...);
        if constexpr (!std::is_trivially_destructible_v<T>) {
            auto* node = static_cast<DtorNode*>(AllocateRaw(sizeof(DtorNode), alignof(DtorNode)));
            node->obj = obj;
            node->destroy = [](void* p) noexcept { static_cast<T*>(p)->~T(); };
            node->next = m_dtorHead;
            m_dtorHead = node;
        }
        return obj;
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
