#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <new>
#include <span>
#include <type_traits>
#include <utility>
#include "guikit/core/allocator.hpp"
#include "keel/memory.h"
#include "keel/ref.h"
#include "keel/small_array.h"

namespace guikit {

/// Small-Buffer-Optimized (SBO) contiguous vector for UI layout and widget computations.
/// Backed by `keel::SmallArray<T, InlineCapacity>`: stores up to `InlineCapacity` elements inline
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

    UiVector() noexcept = default;

    explicit UiVector(size_t count) {
        m_arr.resize(static_cast<keel::isize>(count));
    }

    UiVector(size_t count, const T& value) {
        m_arr.assign(static_cast<keel::isize>(count), value);
    }

    UiVector(std::initializer_list<T> init) {
        m_arr.append(keel::ArrayRef<T>(init.begin(), static_cast<keel::isize>(init.size())));
    }

    explicit UiVector(std::span<const T> src) {
        m_arr.append(keel::ArrayRef<T>(src.data(), static_cast<keel::isize>(src.size())));
    }

    explicit UiVector(keel::ArrayRef<T> src) {
        m_arr.append(src);
    }

    UiVector(const UiVector& other)
        : m_arr(other.m_arr.clone()) {}

    UiVector(UiVector&& other) noexcept = default;

    UiVector& operator=(const UiVector& other) {
        if (this != &other) {
            m_arr.assign(other.m_arr);
        }
        return *this;
    }

    UiVector& operator=(UiVector&& other) noexcept = default;

    ~UiVector() noexcept = default;

    [[nodiscard]] T* data() noexcept { return m_arr.data(); }
    [[nodiscard]] const T* data() const noexcept { return m_arr.data(); }

    [[nodiscard]] size_t size() const noexcept { return static_cast<size_t>(m_arr.size()); }
    [[nodiscard]] size_t capacity() const noexcept { return static_cast<size_t>(m_arr.capacity()); }
    [[nodiscard]] bool empty() const noexcept { return m_arr.empty(); }
    [[nodiscard]] bool IsInline() const noexcept { return m_arr.is_inline(); }
    [[nodiscard]] static constexpr size_t InlineCapacityValue() noexcept { return InlineCapacity; }

    [[nodiscard]] iterator begin() noexcept { return m_arr.begin(); }
    [[nodiscard]] const_iterator begin() const noexcept { return m_arr.begin(); }
    [[nodiscard]] const_iterator cbegin() const noexcept { return m_arr.begin(); }

    [[nodiscard]] iterator end() noexcept { return m_arr.end(); }
    [[nodiscard]] const_iterator end() const noexcept { return m_arr.end(); }
    [[nodiscard]] const_iterator cend() const noexcept { return m_arr.end(); }

    [[nodiscard]] reference operator[](size_t idx) noexcept {
        assert(idx < size());
        return m_arr[static_cast<keel::isize>(idx)];
    }

    [[nodiscard]] const_reference operator[](size_t idx) const noexcept {
        assert(idx < size());
        return m_arr[static_cast<keel::isize>(idx)];
    }

    [[nodiscard]] reference front() noexcept { return m_arr.front(); }
    [[nodiscard]] const_reference front() const noexcept { return m_arr.front(); }

    [[nodiscard]] reference back() noexcept { return m_arr.back(); }
    [[nodiscard]] const_reference back() const noexcept { return m_arr.back(); }

    [[nodiscard]] std::span<T> Span() noexcept { return std::span<T>(m_arr.data(), size()); }
    [[nodiscard]] std::span<const T> Span() const noexcept { return std::span<const T>(m_arr.data(), size()); }
    [[nodiscard]] keel::ArrayRef<T> KeelRef() const noexcept { return m_arr; }
    [[nodiscard]] operator keel::ArrayRef<T>() const noexcept { return m_arr; }
    [[nodiscard]] operator keel::MutArrayRef<T>() noexcept { return m_arr; }

    void clear() noexcept { m_arr.clear(); }

    void reserve(size_t newCap) {
        m_arr.reserve(static_cast<keel::isize>(newCap));
    }

    void resize(size_t newSize) {
        m_arr.resize(static_cast<keel::isize>(newSize));
    }

    void resize(size_t newSize, const T& value) {
        m_arr.resize(static_cast<keel::isize>(newSize), value);
    }

    void assign(size_t count, const T& value) {
        m_arr.assign(static_cast<keel::isize>(count), value);
    }

    void assign(std::span<const T> src) {
        m_arr.assign(keel::ArrayRef<T>(src.data(), static_cast<keel::isize>(src.size())));
    }

    void push_back(const T& value) {
        m_arr.push_back(value);
    }

    void push_back(T&& value) {
        m_arr.push_back(std::move(value));
    }

    template <typename... Args>
    reference emplace_back(Args&&... args) {
        return m_arr.emplace_back(std::forward<Args>(args)...);
    }

    void pop_back() noexcept {
        m_arr.pop_back();
    }

    iterator erase(const_iterator pos) {
        assert(pos >= cbegin() && pos < cend());
        const keel::isize idx = static_cast<keel::isize>(pos - cbegin());
        m_arr.remove_at(idx);
        return m_arr.data() + idx;
    }

private:
    keel::SmallArray<T, static_cast<keel::isize>(InlineCapacity)> m_arr;
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
/// via `Allocator::TryExtendRaw` (`keel::Arena::try_extend`) when growing at the tip of the arena.
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
