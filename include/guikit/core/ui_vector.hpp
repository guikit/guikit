#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#include "guikit/core/allocator.hpp"

namespace guikit {

/// Small-Buffer-Optimized (SBO) contiguous vector for UI layout and widget computations.
/// Stores up to `InlineCapacity` elements directly on the stack/struct without heap allocation,
/// and never stores a self-referential pointer to its inline buffer (safe across moves/copies).
template <typename T, size_t InlineCapacity = 16>
class UiVector {
    static_assert(InlineCapacity > 0, "UiVector InlineCapacity must be greater than 0");

public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;

    constexpr UiVector() noexcept = default;

    explicit UiVector(size_t count) {
        resize(count);
    }

    UiVector(size_t count, const T& value) {
        assign(count, value);
    }

    UiVector(std::initializer_list<T> init) {
        assign(std::span<const T>(init.begin(), init.size()));
    }

    explicit UiVector(std::span<const T> src) {
        assign(src);
    }

    UiVector(const UiVector& other) {
        assign(other.Span());
    }

    UiVector(UiVector&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        MoveFrom(std::move(other));
    }

    UiVector& operator=(const UiVector& other) {
        if (this != &other) {
            assign(other.Span());
        }
        return *this;
    }

    UiVector& operator=(UiVector&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (this != &other) {
            DestroyElements(data(), m_size);
            FreeHeapBuffer();
            MoveFrom(std::move(other));
        }
        return *this;
    }

    ~UiVector() noexcept {
        DestroyElements(data(), m_size);
        FreeHeapBuffer();
    }

    [[nodiscard]] T* data() noexcept {
        return m_heap ? m_heap : InlinePtr();
    }

    [[nodiscard]] const T* data() const noexcept {
        return m_heap ? m_heap : InlinePtr();
    }

    [[nodiscard]] constexpr size_t size() const noexcept { return m_size; }
    [[nodiscard]] constexpr size_t capacity() const noexcept { return m_capacity; }
    [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }
    [[nodiscard]] constexpr bool IsInline() const noexcept { return m_heap == nullptr; }
    [[nodiscard]] static constexpr size_t InlineCapacityValue() noexcept { return InlineCapacity; }

    [[nodiscard]] iterator begin() noexcept { return data(); }
    [[nodiscard]] const_iterator begin() const noexcept { return data(); }
    [[nodiscard]] const_iterator cbegin() const noexcept { return data(); }

    [[nodiscard]] iterator end() noexcept { return data() + m_size; }
    [[nodiscard]] const_iterator end() const noexcept { return data() + m_size; }
    [[nodiscard]] const_iterator cend() const noexcept { return data() + m_size; }

    [[nodiscard]] reference operator[](size_t idx) noexcept {
        assert(idx < m_size);
        return data()[idx];
    }

    [[nodiscard]] const_reference operator[](size_t idx) const noexcept {
        assert(idx < m_size);
        return data()[idx];
    }

    [[nodiscard]] reference front() noexcept {
        assert(m_size > 0);
        return data()[0];
    }

    [[nodiscard]] const_reference front() const noexcept {
        assert(m_size > 0);
        return data()[0];
    }

    [[nodiscard]] reference back() noexcept {
        assert(m_size > 0);
        return data()[m_size - 1];
    }

    [[nodiscard]] const_reference back() const noexcept {
        assert(m_size > 0);
        return data()[m_size - 1];
    }

    [[nodiscard]] std::span<T> Span() noexcept { return std::span<T>(data(), m_size); }
    [[nodiscard]] std::span<const T> Span() const noexcept { return std::span<const T>(data(), m_size); }

    void clear() noexcept {
        DestroyElements(data(), m_size);
        m_size = 0;
    }

    void reserve(size_t newCap) {
        if (newCap <= m_capacity) {
            return;
        }
        GrowTo(newCap);
    }

    void resize(size_t newSize) {
        if (newSize < m_size) {
            DestroyElements(data() + newSize, m_size - newSize);
            m_size = static_cast<uint32_t>(newSize);
        } else if (newSize > m_size) {
            reserve(newSize);
            T* cur = data();
            for (size_t i = m_size; i < newSize; ++i) {
                ::new (static_cast<void*>(cur + i)) T{};
            }
            m_size = static_cast<uint32_t>(newSize);
        }
    }

    void resize(size_t newSize, const T& value) {
        if (newSize < m_size) {
            DestroyElements(data() + newSize, m_size - newSize);
            m_size = static_cast<uint32_t>(newSize);
        } else if (newSize > m_size) {
            reserve(newSize);
            T* cur = data();
            for (size_t i = m_size; i < newSize; ++i) {
                ::new (static_cast<void*>(cur + i)) T(value);
            }
            m_size = static_cast<uint32_t>(newSize);
        }
    }

    void assign(size_t count, const T& value) {
        clear();
        reserve(count);
        T* cur = data();
        for (size_t i = 0; i < count; ++i) {
            ::new (static_cast<void*>(cur + i)) T(value);
        }
        m_size = static_cast<uint32_t>(count);
    }

    void assign(std::span<const T> src) {
        if (src.data() >= data() && src.data() < data() + m_size) {
            UiVector tmp(src.size());
            for (size_t i = 0; i < src.size(); ++i) {
                tmp[i] = src[i];
            }
            *this = std::move(tmp);
            return;
        }
        clear();
        if (src.empty()) {
            return;
        }
        reserve(src.size());
        T* dst = data();
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(static_cast<void*>(dst), src.data(), sizeof(T) * src.size());
        } else {
            for (size_t i = 0; i < src.size(); ++i) {
                ::new (static_cast<void*>(dst + i)) T(src[i]);
            }
        }
        m_size = static_cast<uint32_t>(src.size());
    }

    void push_back(const T& value) {
        emplace_back(value);
    }

    void push_back(T&& value) {
        emplace_back(std::move(value));
    }

    template <typename... Args>
    reference emplace_back(Args&&... args) {
        if (m_size < m_capacity) {
            T* slot = data() + m_size;
            T* elem = ::new (static_cast<void*>(slot)) T(std::forward<Args>(args)...);
            ++m_size;
            return *elem;
        }
        return EmplaceBackSlow(std::forward<Args>(args)...);
    }

    void pop_back() noexcept {
        assert(m_size > 0);
        --m_size;
        if constexpr (!std::is_trivially_destructible_v<T>) {
            data()[m_size].~T();
        }
    }

    iterator erase(const_iterator pos) {
        assert(pos >= cbegin() && pos < cend());
        const size_t idx = static_cast<size_t>(pos - cbegin());
        T* cur = data();
        for (size_t i = idx; i + 1 < m_size; ++i) {
            cur[i] = std::move(cur[i + 1]);
        }
        pop_back();
        return cur + idx;
    }

private:
    [[nodiscard]] T* InlinePtr() noexcept {
        return std::launder(reinterpret_cast<T*>(m_inline));
    }

    [[nodiscard]] const T* InlinePtr() const noexcept {
        return std::launder(reinterpret_cast<const T*>(m_inline));
    }

    static T* AllocateHeapBuffer(size_t cap) {
        return static_cast<T*>(::operator new(sizeof(T) * cap, std::align_val_t(alignof(T))));
    }

    void FreeHeapBuffer() noexcept {
        if (m_heap != nullptr) {
            ::operator delete(m_heap, std::align_val_t(alignof(T)));
            m_heap = nullptr;
        }
    }

    static void DestroyElements(T* ptr, size_t count) noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_t i = count; i > 0; --i) {
                ptr[i - 1].~T();
            }
        } else {
            (void)ptr;
            (void)count;
        }
    }

    static void RelocateElements(T* dst, T* src, size_t count) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (count == 0) {
            return;
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(static_cast<void*>(dst), static_cast<const void*>(src), sizeof(T) * count);
        } else {
            for (size_t i = 0; i < count; ++i) {
                ::new (static_cast<void*>(dst + i)) T(std::move_if_noexcept(src[i]));
                src[i].~T();
            }
        }
    }

    void GrowTo(size_t minCap) {
        const size_t doubled = static_cast<size_t>(m_capacity) * 2;
        const size_t newCap = std::max(minCap, doubled);
        T* newBuf = AllocateHeapBuffer(newCap);
        T* oldBuf = data();
        RelocateElements(newBuf, oldBuf, m_size);
        FreeHeapBuffer();
        m_heap = newBuf;
        m_capacity = static_cast<uint32_t>(newCap);
    }

    template <typename... Args>
    reference EmplaceBackSlow(Args&&... args) {
        const size_t newCap = std::max<size_t>(static_cast<size_t>(m_capacity) * 2, InlineCapacity + 1);
        T* newBuf = AllocateHeapBuffer(newCap);
        // Construct new element before relocating/destroying old buffer to stay safe if args alias an existing element
        T* newElem = nullptr;
        try {
            newElem = ::new (static_cast<void*>(newBuf + m_size)) T(std::forward<Args>(args)...);
        } catch (...) {
            ::operator delete(newBuf, std::align_val_t(alignof(T)));
            throw;
        }
        RelocateElements(newBuf, data(), m_size);
        FreeHeapBuffer();
        m_heap = newBuf;
        m_capacity = static_cast<uint32_t>(newCap);
        ++m_size;
        return *newElem;
    }

    void MoveFrom(UiVector&& other) noexcept(std::is_nothrow_move_constructible_v<T>) {
        if (!other.IsInline()) {
            m_heap = other.m_heap;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_heap = nullptr;
            other.m_size = 0;
            other.m_capacity = static_cast<uint32_t>(InlineCapacity);
        } else {
            m_heap = nullptr;
            m_size = other.m_size;
            m_capacity = static_cast<uint32_t>(InlineCapacity);
            RelocateElements(InlinePtr(), other.InlinePtr(), other.m_size);
            other.m_size = 0;
        }
    }

    T* m_heap{nullptr};
    uint32_t m_size{0};
    uint32_t m_capacity{static_cast<uint32_t>(InlineCapacity)};
    alignas(T) std::byte m_inline[sizeof(T) * InlineCapacity];
};

namespace detail {

template <typename T, bool TriviallyDestructible = std::is_trivially_destructible_v<T>>
struct ArenaVectorBase {
    Allocator* m_arena{nullptr};
    T* m_data{nullptr};
    uint32_t m_size{0};
    uint32_t m_capacity{0};

    constexpr ArenaVectorBase() noexcept = default;
    constexpr explicit ArenaVectorBase(Allocator* arena) noexcept : m_arena(arena) {}
    ~ArenaVectorBase() noexcept = default;
};

template <typename T>
struct ArenaVectorBase<T, false> {
    Allocator* m_arena{nullptr};
    T* m_data{nullptr};
    uint32_t m_size{0};
    uint32_t m_capacity{0};

    constexpr ArenaVectorBase() noexcept = default;
    constexpr explicit ArenaVectorBase(Allocator* arena) noexcept : m_arena(arena) {}

    ~ArenaVectorBase() noexcept {
        for (size_t i = m_size; i > 0; --i) {
            m_data[i - 1].~T();
        }
    }
};

} // namespace detail

/// Bump-arena-backed contiguous vector for per-frame `Node::payload` structures.
/// Trivially destructible when `T` is trivially destructible, and expands in-place
/// via `Allocator::TryExtendRaw` when growing at the tip of the active arena chunk.
template <typename T>
class UiArenaVector : private detail::ArenaVectorBase<T> {
    using Base = detail::ArenaVectorBase<T>;
    using Base::m_arena;
    using Base::m_data;
    using Base::m_size;
    using Base::m_capacity;

public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using iterator = T*;
    using const_iterator = const T*;

    constexpr UiArenaVector() noexcept = default;

    constexpr explicit UiArenaVector(Allocator& arena, size_t initialReserve = 0)
        : Base(&arena) {
        if (initialReserve > 0) {
            reserve(initialReserve);
        }
    }

    constexpr explicit UiArenaVector(Allocator* arena, size_t initialReserve = 0)
        : Base(arena) {
        if (arena != nullptr && initialReserve > 0) {
            reserve(initialReserve);
        }
    }

    UiArenaVector(const UiArenaVector& other)
        : Base(other.m_arena) {
        if (other.m_size > 0 && m_arena != nullptr) {
            reserve(other.m_size);
            if constexpr (std::is_trivially_copyable_v<T>) {
                std::memcpy(static_cast<void*>(m_data), other.m_data, sizeof(T) * other.m_size);
            } else {
                for (size_t i = 0; i < other.m_size; ++i) {
                    ::new (static_cast<void*>(m_data + i)) T(other.m_data[i]);
                }
            }
            m_size = other.m_size;
        }
    }

    UiArenaVector(UiArenaVector&& other) noexcept
        : Base(other.m_arena) {
        m_data = other.m_data;
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }

    UiArenaVector& operator=(const UiArenaVector& other) {
        if (this != &other) {
            clear();
            if (m_arena == nullptr) {
                m_arena = other.m_arena;
            }
            if (other.m_size > 0 && m_arena != nullptr) {
                reserve(other.m_size);
                if constexpr (std::is_trivially_copyable_v<T>) {
                    std::memcpy(static_cast<void*>(m_data), other.m_data, sizeof(T) * other.m_size);
                } else {
                    for (size_t i = 0; i < other.m_size; ++i) {
                        ::new (static_cast<void*>(m_data + i)) T(other.m_data[i]);
                    }
                }
                m_size = other.m_size;
            }
        }
        return *this;
    }

    UiArenaVector& operator=(UiArenaVector&& other) noexcept {
        if (this != &other) {
            clear();
            m_arena = other.m_arena;
            m_data = other.m_data;
            m_size = other.m_size;
            m_capacity = other.m_capacity;
            other.m_data = nullptr;
            other.m_size = 0;
            other.m_capacity = 0;
        }
        return *this;
    }

    ~UiArenaVector() noexcept = default;

    constexpr void BindArena(Allocator& arena) noexcept {
        m_arena = &arena;
    }

    [[nodiscard]] constexpr Allocator* GetArena() const noexcept { return m_arena; }

    [[nodiscard]] constexpr T* data() noexcept { return m_data; }
    [[nodiscard]] constexpr const T* data() const noexcept { return m_data; }

    [[nodiscard]] constexpr size_t size() const noexcept { return m_size; }
    [[nodiscard]] constexpr size_t capacity() const noexcept { return m_capacity; }
    [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }

    [[nodiscard]] constexpr iterator begin() noexcept { return m_data; }
    [[nodiscard]] constexpr const_iterator begin() const noexcept { return m_data; }
    [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return m_data; }

    [[nodiscard]] constexpr iterator end() noexcept { return m_data + m_size; }
    [[nodiscard]] constexpr const_iterator end() const noexcept { return m_data + m_size; }
    [[nodiscard]] constexpr const_iterator cend() const noexcept { return m_data + m_size; }

    [[nodiscard]] constexpr reference operator[](size_t idx) noexcept {
        assert(idx < m_size);
        return m_data[idx];
    }

    [[nodiscard]] constexpr const_reference operator[](size_t idx) const noexcept {
        assert(idx < m_size);
        return m_data[idx];
    }

    [[nodiscard]] constexpr reference front() noexcept {
        assert(m_size > 0);
        return m_data[0];
    }

    [[nodiscard]] constexpr const_reference front() const noexcept {
        assert(m_size > 0);
        return m_data[0];
    }

    [[nodiscard]] constexpr reference back() noexcept {
        assert(m_size > 0);
        return m_data[m_size - 1];
    }

    [[nodiscard]] constexpr const_reference back() const noexcept {
        assert(m_size > 0);
        return m_data[m_size - 1];
    }

    [[nodiscard]] constexpr std::span<T> Span() noexcept { return std::span<T>(m_data, m_size); }
    [[nodiscard]] constexpr std::span<const T> Span() const noexcept { return std::span<const T>(m_data, m_size); }

    void clear() noexcept {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_t i = m_size; i > 0; --i) {
                m_data[i - 1].~T();
            }
        }
        m_size = 0;
    }

    void reserve(size_t newCap) {
        if (newCap <= m_capacity) {
            return;
        }
        assert(m_arena != nullptr && "UiArenaVector requires a bound Allocator before allocating");
        if (m_arena == nullptr) {
            return;
        }

        if (m_data != nullptr &&
            m_arena->TryExtendRaw(m_data, sizeof(T) * m_capacity, sizeof(T) * newCap)) {
            m_capacity = static_cast<uint32_t>(newCap);
            return;
        }

        T* newBuf = static_cast<T*>(m_arena->AllocateRaw(sizeof(T) * newCap, alignof(T)));
        if (m_size > 0 && m_data != nullptr) {
            if constexpr (std::is_trivially_copyable_v<T>) {
                std::memcpy(static_cast<void*>(newBuf), static_cast<const void*>(m_data), sizeof(T) * m_size);
            } else {
                for (size_t i = 0; i < m_size; ++i) {
                    ::new (static_cast<void*>(newBuf + i)) T(std::move_if_noexcept(m_data[i]));
                    m_data[i].~T();
                }
            }
        }
        m_data = newBuf;
        m_capacity = static_cast<uint32_t>(newCap);
    }

    void resize(size_t newSize) {
        if (newSize < m_size) {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_t i = m_size; i > newSize; --i) {
                    m_data[i - 1].~T();
                }
            }
            m_size = static_cast<uint32_t>(newSize);
        } else if (newSize > m_size) {
            reserve(newSize);
            for (size_t i = m_size; i < newSize; ++i) {
                ::new (static_cast<void*>(m_data + i)) T{};
            }
            m_size = static_cast<uint32_t>(newSize);
        }
    }

    void push_back(const T& value) {
        emplace_back(value);
    }

    void push_back(T&& value) {
        emplace_back(std::move(value));
    }

    template <typename... Args>
    reference emplace_back(Args&&... args) {
        if (m_size < m_capacity) {
            T* elem = ::new (static_cast<void*>(m_data + m_size)) T(std::forward<Args>(args)...);
            ++m_size;
            return *elem;
        }
        return EmplaceBackSlow(std::forward<Args>(args)...);
    }

    void pop_back() noexcept {
        assert(m_size > 0);
        --m_size;
        if constexpr (!std::is_trivially_destructible_v<T>) {
            m_data[m_size].~T();
        }
    }

private:
    template <typename... Args>
    reference EmplaceBackSlow(Args&&... args) {
        assert(m_arena != nullptr && "UiArenaVector requires a bound Allocator before allocating");
        const size_t nextCap = (m_capacity == 0) ? 16 : (static_cast<size_t>(m_capacity) * 2);
        if (m_data != nullptr &&
            m_arena->TryExtendRaw(m_data, sizeof(T) * m_capacity, sizeof(T) * nextCap)) {
            m_capacity = static_cast<uint32_t>(nextCap);
            T* elem = ::new (static_cast<void*>(m_data + m_size)) T(std::forward<Args>(args)...);
            ++m_size;
            return *elem;
        }

        T* newBuf = static_cast<T*>(m_arena->AllocateRaw(sizeof(T) * nextCap, alignof(T)));
        // Construct new element before relocating/destroying old elements so self-aliasing args remain valid
        T* elem = ::new (static_cast<void*>(newBuf + m_size)) T(std::forward<Args>(args)...);
        if (m_size > 0 && m_data != nullptr) {
            if constexpr (std::is_trivially_copyable_v<T>) {
                std::memcpy(static_cast<void*>(newBuf), static_cast<const void*>(m_data), sizeof(T) * m_size);
            } else {
                for (size_t i = 0; i < m_size; ++i) {
                    ::new (static_cast<void*>(newBuf + i)) T(std::move_if_noexcept(m_data[i]));
                    m_data[i].~T();
                }
            }
        }
        m_data = newBuf;
        m_capacity = static_cast<uint32_t>(nextCap);
        ++m_size;
        return *elem;
    }
};

} // namespace guikit
