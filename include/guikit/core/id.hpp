#pragma once

#include "keel/core.h"
#include "keel/hash.h"
#include "keel/ref.h"

namespace guikit {

/// 64-bit unique widget identifier generated automatically without manual string tags.
struct Id {
    keel::u64 value{0};

    constexpr Id() noexcept = default;
    constexpr explicit Id(keel::u64 inValue) noexcept : value(inValue) {}

    [[nodiscard]] static constexpr Id Null() noexcept { return Id{0}; }
    [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }

    [[nodiscard]] constexpr bool operator==(const Id& other) const noexcept = default;
    [[nodiscard]] constexpr bool operator<(const Id& other) const noexcept { return value < other.value; }
    [[nodiscard]] constexpr bool operator<=(const Id& other) const noexcept { return value <= other.value; }
    [[nodiscard]] constexpr bool operator>(const Id& other) const noexcept { return value > other.value; }
    [[nodiscard]] constexpr bool operator>=(const Id& other) const noexcept { return value >= other.value; }
};

namespace detail {

/// 64-bit FNV-1a hash algorithm (delegates to keel::fnv1a64_*).
inline constexpr keel::u64 Fnv1aOffsetBasis = keel::kFnv1aOffsetBasis;
inline constexpr keel::u64 Fnv1aPrime = keel::kFnv1aPrime;

constexpr keel::u64 HashBytes(const void* data, keel::usize size, keel::u64 hash = Fnv1aOffsetBasis) noexcept {
    if (__builtin_is_constant_evaluated()) {
        const auto* bytes = static_cast<const keel::u8*>(data);
        for (keel::usize i = 0; i < size; ++i) {
            hash ^= static_cast<keel::u64>(bytes[i]);
            hash *= Fnv1aPrime;
        }
        return hash;
    }
    return keel::fnv1a64_bytes(data, size, hash);
}

constexpr keel::u64 HashStringView(keel::StringRef str, keel::u64 hash = Fnv1aOffsetBasis) noexcept {
    return keel::fnv1a64_chars(str.data(), str.size(), hash);
}

constexpr keel::u64 HashInt64(keel::u64 val, keel::u64 hash = Fnv1aOffsetBasis) noexcept {
    return HashBytes(&val, sizeof(val), hash);
}

template <typename T>
constexpr keel::u64 HashVal(const T& val, keel::u64 hash = Fnv1aOffsetBasis) noexcept {
    using CleanT = keel::Decay<T>;
    if constexpr (keel::IsSame<CleanT, const char*>) {
        return val ? HashStringView(keel::StringRef(val), hash) : hash;
    } else if constexpr (keel::IsConvertible<CleanT, keel::StringRef>) {
        return HashStringView(keel::StringRef(val), hash);
    } else if constexpr (requires { val.text; val.icon; val.iconPlacement; }) {
        hash = HashVal(val.text, hash);
        const keel::u8 icType = static_cast<keel::u8>(val.icon.GetType());
        const keel::u16 icId = static_cast<keel::u16>(val.icon.GetId());
        const keel::u8 icPlace = static_cast<keel::u8>(val.iconPlacement);
        hash = HashBytes(&icType, sizeof(icType), hash);
        hash = HashBytes(&icId, sizeof(icId), hash);
        hash = HashStringView(val.icon.GetPath(), hash);
        hash = HashBytes(&icPlace, sizeof(icPlace), hash);
        return hash;
    } else if constexpr (requires { val.text; val.size; val.color; val.wrap; }) {
        hash = HashStringView(val.text, hash);
        hash = HashBytes(&val.size.px, sizeof(val.size.px), hash);
        const keel::u8 presetVal = static_cast<keel::u8>(val.size.preset);
        const keel::u8 hasRole = val.size.role.has_value() ? 1u : 0u;
        const keel::u8 roleVal = val.size.role.has_value() ? static_cast<keel::u8>(*val.size.role) : 0u;
        hash = HashBytes(&presetVal, sizeof(presetVal), hash);
        hash = HashBytes(&hasRole, sizeof(hasRole), hash);
        hash = HashBytes(&roleVal, sizeof(roleVal), hash);
        hash = HashVal(val.color, hash);
        hash = HashBytes(&val.wrap.minLines, sizeof(val.wrap.minLines), hash);
        hash = HashBytes(&val.wrap.maxLines, sizeof(val.wrap.maxLines), hash);
        const keel::u8 richVal = val.wrap.rich ? 1u : 0u;
        hash = HashBytes(&richVal, sizeof(richVal), hash);
        if constexpr (requires { val.variants; }) {
            for (keel::StringRef v : val.variants) {
                hash = HashStringView(v, hash);
            }
        }
        return hash;
    } else if constexpr (requires { val.normal; val.hover; val.active; val.disabled; }) {
        auto hashOptCol = [&](const auto& opt) noexcept {
            const keel::u8 has = opt.has_value() ? 1u : 0u;
            const keel::u32 rgba = opt.has_value() ? opt->ToUint32() : 0u;
            hash = HashBytes(&has, sizeof(has), hash);
            hash = HashBytes(&rgba, sizeof(rgba), hash);
        };
        hashOptCol(val.normal);
        hashOptCol(val.hover);
        hashOptCol(val.active);
        hashOptCol(val.disabled);
        if constexpr (requires { val.token; }) {
            const keel::u8 tok = static_cast<keel::u8>(val.token);
            hash = HashBytes(&tok, sizeof(tok), hash);
        }
        if constexpr (requires { val.surfaceTint; val.alphaScale; }) {
            hash = HashBytes(&val.surfaceTint, sizeof(val.surfaceTint), hash);
            hash = HashBytes(&val.alphaScale, sizeof(val.alphaScale), hash);
        }
        return hash;
    } else if constexpr (requires { val.width; val.radius; val.color; }) {
        hash = HashBytes(&val.width, sizeof(val.width), hash);
        const float r = val.radius.value_or(-1.0f);
        hash = HashBytes(&r, sizeof(r), hash);
        hash = HashVal(val.color, hash);
        return hash;
    } else if constexpr (requires { val.placement; val.width; val.color; }) {
        const keel::u8 pl = static_cast<keel::u8>(val.placement);
        hash = HashBytes(&pl, sizeof(pl), hash);
        hash = HashBytes(&val.width, sizeof(val.width), hash);
        hash = HashVal(val.color, hash);
        return hash;
    } else if constexpr (keel::IsEnum<CleanT>) {
        auto underlying = static_cast<keel::UnderlyingType<CleanT>>(val);
        return HashBytes(&underlying, sizeof(underlying), hash);
    } else {
        return HashBytes(&val, sizeof(val), hash);
    }
}

template <typename... Args>
constexpr keel::u64 HashArgs(const Args&... args) noexcept {
    keel::u64 h = Fnv1aOffsetBasis;
    ((h = HashVal(args, h)), ...);
    return h;
}

[[nodiscard]] inline constexpr keel::u64 HashCallsiteWithFileHash(
    Id parentId,
    keel::u64 fileHash,
    keel::u32 line,
    keel::u32 column
) noexcept {
    keel::u64 hash = Fnv1aOffsetBasis;
    if (parentId.IsValid()) {
        hash = HashInt64(parentId.value, hash);
    }
    hash = HashInt64(fileHash, hash);
    hash = HashInt64(static_cast<keel::u64>(line), hash);
    hash = HashInt64(static_cast<keel::u64>(column), hash);
    return hash;
}

[[nodiscard]] inline constexpr Id FinalizeCallsiteId(
    keel::u64 callsiteHash,
    keel::u32 occurrenceIndex
) noexcept {
    keel::u64 hash = HashInt64(static_cast<keel::u64>(occurrenceIndex), callsiteHash);
    if (hash == 0) {
        hash = 1;
    }
    return Id{hash};
}

} // namespace detail

/// Generates an automatic ID combining source location, occurrence index, and optional parent ID.
[[nodiscard]] inline constexpr Id MakeId(
    keel::u32 childIndex,
    keel::SourceLocation loc = keel::SourceLocation::current(),
    Id parentId = Id::Null()
) noexcept {
    const keel::u64 fileHash = detail::HashStringView(loc.file_name());
    const keel::u64 callsiteHash = detail::HashCallsiteWithFileHash(
        parentId, fileHash, loc.line(), loc.column()
    );
    return detail::FinalizeCallsiteId(callsiteHash, childIndex);
}

} // namespace guikit

// Keel hash support for guikit::Id
template <>
struct keel::Hash<guikit::Id> {
    [[nodiscard]] constexpr keel::u64 operator()(const guikit::Id& id) const noexcept {
        return keel::hash_u64(id.value);
    }
};
