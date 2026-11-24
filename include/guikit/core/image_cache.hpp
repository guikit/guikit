#pragma once

#include "guikit/core/types.hpp"
#include "keel/array.h"
#include "keel/hash_map.h"
#include "keel/string.h"

namespace guikit {

/// Cached decoded RGBA8888 raster image entry managed by `ImageCache`.
struct CachedImageEntry {
    keel::u32 width{0};
    keel::u32 height{0};
    keel::Array<keel::u32> pixels; ///< RGBA8888 pixels (empty when `probedOnly` or `!valid`)
    bool probedOnly{false};
    bool valid{false};

    [[nodiscard]] PixelRef Pixels() const noexcept {
        if (!valid || probedOnly || width == 0u || height == 0u ||
            pixels.size() < keel::as<keel::usize>(width) * height) {
            return {};
        }
        return PixelRef(pixels.as_ref(), width, height);
    }

    [[nodiscard]] keel::usize ByteSize() const noexcept {
        return pixels.size() * sizeof(keel::u32);
    }
};

/// Shared bounded raster image decoder and cache for all rendering backends (`GlEs3Canvas`, `ThorCanvas`, custom).
/// Resolves embedded assets (`Assets::Find`) and disk files (`"", "../", "../../", "../../../"` relative probes),
/// inspects image headers without full pixel decode on `ProbeSize()`, and decodes via `DecodeImageFromMemory()`.
class ImageCache {
public:
    static constexpr keel::usize kDefaultMaxEntries = 64u;
    static constexpr keel::usize kDefaultMaxBytes = 16u * 1024u * 1024u;

    explicit ImageCache(
        keel::usize maxEntries = kDefaultMaxEntries,
        keel::usize maxBytes = kDefaultMaxBytes
    ) noexcept;

    /// Probes natural dimensions of `source` (inspecting header only when supported). Returns `{0, 0}` on failure.
    [[nodiscard]] Vec2 ProbeSize(keel::StringRef source);

    /// Resolves and decodes `source` into RGBA8888 pixels. Returns `nullptr` if loading or decoding fails.
    [[nodiscard]] const CachedImageEntry* ResolvePixels(keel::StringRef source);

    /// Clears all cached entries and releases pixel storage.
    void Clear() noexcept;

    [[nodiscard]] keel::usize EntryCount() const noexcept { return m_entries.size(); }
    [[nodiscard]] keel::usize ByteUsage() const noexcept { return m_totalBytes; }

private:
    bool LoadSourceBytes(keel::StringRef source, keel::Array<keel::u8>& outOwned, keel::ArrayRef<keel::u8>& outView) const;
    void EnforceBudget(keel::usize incomingBytes) noexcept;

    keel::HashMap<keel::String, CachedImageEntry> m_entries;
    keel::usize m_maxEntries{kDefaultMaxEntries};
    keel::usize m_maxBytes{kDefaultMaxBytes};
    keel::usize m_totalBytes{0};
};

} // namespace guikit
