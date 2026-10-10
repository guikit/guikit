#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <format>
#include <string_view>

namespace guikit {

/// Zero-heap hybrid string optimized for immediate mode UI labels.
/// Holds either a pointer to a static string literal (zero copies), or an inline fixed buffer (zero heap allocations).
template <size_t Capacity = 64>
class BasicUiString {
public:
    constexpr BasicUiString() noexcept
        : m_ptr(m_buffer.data()), m_length(0) {
        m_buffer[0] = '\0';
    }

    /// Constructs from a static string literal or null-terminated string with zero copy.
    constexpr BasicUiString(const char* literal) noexcept {
        SetStatic(literal);
    }

    /// Constructs from a string_view, storing characters in the inline buffer.
    BasicUiString(std::string_view sv) noexcept {
        Set(sv);
    }

    /// Assigns a static string literal with zero copy.
    BasicUiString& operator=(const char* literal) noexcept {
        SetStatic(literal);
        return *this;
    }

    /// Assigns from string_view into the inline buffer.
    BasicUiString& operator=(std::string_view sv) noexcept {
        Set(sv);
        return *this;
    }

    /// Assigns static literal directly without copying characters.
    void SetStatic(const char* literal) noexcept {
        if (literal) {
            m_ptr = literal;
            m_length = static_cast<uint32_t>(std::char_traits<char>::length(literal));
        } else {
            Clear();
        }
    }

    /// Copies string_view into the inline buffer.
    void Set(std::string_view sv) noexcept {
        m_length = static_cast<uint32_t>(std::min(sv.size(), Capacity - 1));
        std::memcpy(m_buffer.data(), sv.data(), m_length);
        m_buffer[m_length] = '\0';
        m_ptr = m_buffer.data();
    }

    /// Formats arguments directly into the inline buffer with zero heap allocations.
    template <typename... Args>
    void Format(std::format_string<Args...> fmt, Args&&... args) {
        auto res = std::format_to_n(m_buffer.data(), Capacity - 1, fmt, std::forward<Args>(args)...);
        m_length = static_cast<uint32_t>(res.out - m_buffer.data());
        m_buffer[m_length] = '\0';
        m_ptr = m_buffer.data();
    }

    /// Factory method formatting a new UiString directly in-place.
    template <typename... Args>
    [[nodiscard]] static BasicUiString FormatNew(std::format_string<Args...> fmt, Args&&... args) {
        BasicUiString s;
        s.Format(fmt, std::forward<Args>(args)...);
        return s;
    }

    /// Resets string to empty state.
    void Clear() noexcept {
        m_buffer[0] = '\0';
        m_ptr = m_buffer.data();
        m_length = 0;
    }

    // Branchless accessors
    [[nodiscard]] constexpr operator std::string_view() const noexcept {
        return std::string_view(m_ptr, m_length);
    }

    [[nodiscard]] constexpr std::string_view View() const noexcept {
        return std::string_view(m_ptr, m_length);
    }

    [[nodiscard]] constexpr const char* c_str() const noexcept {
        return m_ptr;
    }

    [[nodiscard]] constexpr const char* data() const noexcept {
        return m_ptr;
    }

    [[nodiscard]] constexpr size_t size() const noexcept {
        return m_length;
    }

    [[nodiscard]] constexpr size_t length() const noexcept {
        return m_length;
    }

    [[nodiscard]] constexpr bool empty() const noexcept {
        return m_length == 0;
    }

    [[nodiscard]] static constexpr size_t capacity() noexcept {
        return Capacity - 1;
    }

    [[nodiscard]] constexpr bool operator==(std::string_view other) const noexcept {
        return View() == other;
    }

    [[nodiscard]] constexpr bool operator==(const char* other) const noexcept {
        return other ? (View() == std::string_view(other)) : empty();
    }

    [[nodiscard]] constexpr bool operator==(const BasicUiString& other) const noexcept {
        return View() == other.View();
    }

private:
    const char* m_ptr{nullptr};
    uint32_t m_length{0};
    std::array<char, Capacity> m_buffer{};
};

/// Primary string type for UI labels with 64-byte inline capacity.
using UiString = BasicUiString<64>;

} // namespace guikit
