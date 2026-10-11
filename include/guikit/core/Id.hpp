#pragma once

#include <cstdint>
#include <functional>
#include <source_location>
#include <string_view>

namespace guikit {

/// 64-bit unique widget identifier generated automatically without manual string tags.
struct Id {
    uint64_t value{0};

    constexpr Id() noexcept = default;
    constexpr explicit Id(uint64_t inValue) noexcept : value(inValue) {}

    [[nodiscard]] static constexpr Id Null() noexcept { return Id{0}; }
    [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }

    [[nodiscard]] constexpr bool operator==(const Id& other) const noexcept = default;
    [[nodiscard]] constexpr auto operator<=>(const Id& other) const noexcept = default;
};

namespace detail {

/// 64-bit FNV-1a hash algorithm.
inline constexpr uint64_t Fnv1aOffsetBasis = 14695981039346656037ULL;
inline constexpr uint64_t Fnv1aPrime = 1099511628211ULL;

constexpr uint64_t HashBytes(const void* data, size_t size, uint64_t hash = Fnv1aOffsetBasis) noexcept {
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= static_cast<uint64_t>(bytes[i]);
        hash *= Fnv1aPrime;
    }
    return hash;
}

constexpr uint64_t HashStringView(std::string_view str, uint64_t hash = Fnv1aOffsetBasis) noexcept {
    for (char c : str) {
        hash ^= static_cast<uint64_t>(static_cast<uint8_t>(c));
        hash *= Fnv1aPrime;
    }
    return hash;
}

constexpr uint64_t HashInt64(uint64_t val, uint64_t hash = Fnv1aOffsetBasis) noexcept {
    return HashBytes(&val, sizeof(val), hash);
}

} // namespace detail

/// Generates an automatic ID combining source location, container child index, and optional parent ID.
[[nodiscard]] inline constexpr Id MakeId(
    uint32_t childIndex,
    std::source_location loc = std::source_location::current(),
    Id parentId = Id::Null()
) noexcept {
    uint64_t hash = detail::Fnv1aOffsetBasis;

    // Incorporate parent ID if nested inside a container
    if (parentId.IsValid()) {
        hash = detail::HashInt64(parentId.value, hash);
    }

    // Incorporate source location: file name, line, and column
    hash = detail::HashStringView(loc.file_name(), hash);
    hash = detail::HashInt64(static_cast<uint64_t>(loc.line()), hash);
    hash = detail::HashInt64(static_cast<uint64_t>(loc.column()), hash);

    // Incorporate container child index to disambiguate loops and multiple elements in single line
    hash = detail::HashInt64(static_cast<uint64_t>(childIndex), hash);

    // Ensure 0 is reserved for Id::Null
    if (hash == 0) {
        hash = 1;
    }

    return Id{hash};
}

} // namespace guikit

// Standard hash support for guikit::Id
template <>
struct std::hash<guikit::Id> {
    size_t operator()(const guikit::Id& id) const noexcept {
        return static_cast<size_t>(id.value);
    }
};
