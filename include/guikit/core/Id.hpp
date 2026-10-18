#pragma once

#include <cstdint>
#include <functional>
#include <source_location>
#include <string_view>

#include <type_traits>

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

template <typename T>
constexpr uint64_t HashVal(const T& val, uint64_t hash = Fnv1aOffsetBasis) noexcept {
    using CleanT = std::decay_t<T>;
    if constexpr (std::is_same_v<CleanT, std::string_view>) {
        return HashStringView(val, hash);
    } else if constexpr (std::is_same_v<CleanT, const char*>) {
        return val ? HashStringView(std::string_view(val), hash) : hash;
    } else if constexpr (requires { val.text; val.icon; val.iconPlacement; }) {
        hash = HashVal(val.text, hash);
        const uint8_t icType = static_cast<uint8_t>(val.icon.GetType());
        const uint16_t icId = static_cast<uint16_t>(val.icon.GetId());
        const uint8_t icPlace = static_cast<uint8_t>(val.iconPlacement);
        hash = HashBytes(&icType, sizeof(icType), hash);
        hash = HashBytes(&icId, sizeof(icId), hash);
        hash = HashStringView(val.icon.GetPath(), hash);
        hash = HashBytes(&icPlace, sizeof(icPlace), hash);
        return hash;
    } else if constexpr (requires { val.text; val.size; val.color; val.wrap; }) {
        hash = HashStringView(val.text, hash);
        hash = HashBytes(&val.size.px, sizeof(val.size.px), hash);
        const uint8_t presetVal = static_cast<uint8_t>(val.size.preset);
        const uint8_t hasRole = val.size.role.has_value() ? 1u : 0u;
        const uint8_t roleVal = val.size.role.has_value() ? static_cast<uint8_t>(*val.size.role) : 0u;
        hash = HashBytes(&presetVal, sizeof(presetVal), hash);
        hash = HashBytes(&hasRole, sizeof(hasRole), hash);
        hash = HashBytes(&roleVal, sizeof(roleVal), hash);
        hash = HashVal(val.color, hash);
        hash = HashBytes(&val.wrap.minLines, sizeof(val.wrap.minLines), hash);
        hash = HashBytes(&val.wrap.maxLines, sizeof(val.wrap.maxLines), hash);
        if constexpr (requires { val.variants; }) {
            for (std::string_view v : val.variants) {
                hash = HashStringView(v, hash);
            }
        }
        return hash;
    } else if constexpr (requires { val.normal; val.hover; val.active; val.disabled; }) {
        auto hashOptCol = [&](const auto& opt) noexcept {
            const uint8_t has = opt.has_value() ? 1u : 0u;
            const uint32_t rgba = opt.has_value() ? opt->ToUint32() : 0u;
            hash = HashBytes(&has, sizeof(has), hash);
            hash = HashBytes(&rgba, sizeof(rgba), hash);
        };
        hashOptCol(val.normal);
        hashOptCol(val.hover);
        hashOptCol(val.active);
        hashOptCol(val.disabled);
        return hash;
    } else if constexpr (std::is_enum_v<CleanT>) {
        auto underlying = static_cast<std::underlying_type_t<CleanT>>(val);
        return HashBytes(&underlying, sizeof(underlying), hash);
    } else {
        return HashBytes(&val, sizeof(val), hash);
    }
}

template <typename... Args>
constexpr uint64_t HashArgs(const Args&... args) noexcept {
    uint64_t h = Fnv1aOffsetBasis;
    ((h = HashVal(args, h)), ...);
    return h;
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
