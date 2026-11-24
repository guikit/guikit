#include "guikit/core/image_cache.hpp"

#include "guikit/backend/vector_backend.hpp"
#include "guikit/core/assets.hpp"
#include "keel/fs.h"

namespace guikit {

ImageCache::ImageCache(keel::usize maxEntries, keel::usize maxBytes) noexcept
    : m_maxEntries(maxEntries > 0u ? maxEntries : 1u)
    , m_maxBytes(maxBytes > 0u ? maxBytes : kDefaultMaxBytes) {
}

bool ImageCache::LoadSourceBytes(
    keel::StringRef source,
    keel::Array<keel::u8>& outOwned,
    keel::ArrayRef<keel::u8>& outView
) const {
    if (source.empty()) {
        return false;
    }

    if (const auto asset = Assets::Find(source); asset && !asset->Empty()) {
        outView = asset->Bytes();
        return true;
    }

    const bool isAbsolute = keel::path_is_absolute(source);
    static constexpr keel::StringRef kPrefixes[] = {"", "../", "../../", "../../../"};
    for (keel::StringRef prefix : kPrefixes) {
        if (isAbsolute && !prefix.empty()) {
            break;
        }
        keel::SmallString<256> candidate(prefix);
        candidate.append(source);
        if (auto res = keel::fs_read_file(candidate); res.is_ok() && !res.value().empty()) {
            outOwned = keel::move(res.value());
            outView = outOwned.as_ref();
            return true;
        }
    }
    return false;
}

void ImageCache::EnforceBudget(keel::usize incomingBytes) noexcept {
    if (m_entries.size() >= m_maxEntries ||
        (incomingBytes > 0u && m_totalBytes + incomingBytes > m_maxBytes)) {
        Clear();
    }
}

Vec2 ImageCache::ProbeSize(keel::StringRef source) {
    if (source.empty()) {
        return {0.0f, 0.0f};
    }

    if (const CachedImageEntry* existing = m_entries.get(source)) {
        return existing->valid
            ? Vec2{keel::as<keel::f32>(existing->width), keel::as<keel::f32>(existing->height)}
            : Vec2{0.0f, 0.0f};
    }

    keel::Array<keel::u8> ownedBytes;
    keel::ArrayRef<keel::u8> bytes{};
    if (!LoadSourceBytes(source, ownedBytes, bytes)) {
        EnforceBudget(0u);
        CachedImageEntry invalid{};
        invalid.valid = false;
        m_entries.put(keel::String::from(source), keel::move(invalid));
        return {0.0f, 0.0f};
    }

    keel::u32 w = 0u;
    keel::u32 h = 0u;
    if (InspectImageFromMemory(bytes, w, h) && w > 0u && h > 0u) {
        EnforceBudget(0u);
        CachedImageEntry probed{};
        probed.width = w;
        probed.height = h;
        probed.probedOnly = true;
        probed.valid = true;
        m_entries.put(keel::String::from(source), keel::move(probed));
        return {keel::as<keel::f32>(w), keel::as<keel::f32>(h)};
    }

    // Fallback to full decode when header inspection does not recognize the format
    if (const CachedImageEntry* decoded = ResolvePixels(source)) {
        return {keel::as<keel::f32>(decoded->width), keel::as<keel::f32>(decoded->height)};
    }
    return {0.0f, 0.0f};
}

const CachedImageEntry* ImageCache::ResolvePixels(keel::StringRef source) {
    if (source.empty()) {
        return nullptr;
    }

    if (const CachedImageEntry* existing = m_entries.get(source)) {
        if (!existing->valid) {
            return nullptr;
        }
        if (!existing->probedOnly) {
            return existing;
        }
    }

    keel::Array<keel::u8> ownedBytes;
    keel::ArrayRef<keel::u8> bytes{};
    if (!LoadSourceBytes(source, ownedBytes, bytes)) {
        EnforceBudget(0u);
        CachedImageEntry invalid{};
        invalid.valid = false;
        m_entries.put(keel::String::from(source), keel::move(invalid));
        return nullptr;
    }

    keel::u32 w = 0u;
    keel::u32 h = 0u;
    keel::Array<keel::u32> rgba;
    if (!DecodeImageFromMemory(bytes, w, h, rgba) || w == 0u || h == 0u ||
        rgba.size() < keel::as<keel::usize>(w) * h) {
        EnforceBudget(0u);
        CachedImageEntry invalid{};
        invalid.valid = false;
        m_entries.put(keel::String::from(source), keel::move(invalid));
        return nullptr;
    }

    const keel::usize entryBytes = rgba.size() * sizeof(keel::u32);
    EnforceBudget(entryBytes);

    CachedImageEntry entry{};
    entry.width = w;
    entry.height = h;
    entry.pixels = keel::move(rgba);
    entry.probedOnly = false;
    entry.valid = true;
    m_totalBytes += entryBytes;
    m_entries.put(keel::String::from(source), keel::move(entry));
    return m_entries.get(source);
}

void ImageCache::Clear() noexcept {
    m_entries = {};
    m_totalBytes = 0u;
}

} // namespace guikit
