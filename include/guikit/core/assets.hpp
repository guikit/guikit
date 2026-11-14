#pragma once

#include <cstddef>
#include <cstdint>
#include "keel/array.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "keel/string.h"

namespace guikit {

/// Non-owning view over an embedded or runtime-registered binary asset.
struct AssetView {
    const uint8_t* data{nullptr};
    size_t size{0};
    keel::StringRef mimeType{}; // Format/extension hint: "png", "svg", "jpg", "ttf", etc.

    [[nodiscard]] constexpr bool Empty() const noexcept {
        return data == nullptr || size == 0;
    }

    [[nodiscard]] constexpr keel::ArrayRef<uint8_t> Bytes() const noexcept {
        return {data, size};
    }

    [[nodiscard]] constexpr keel::StringRef AsStringView() const noexcept {
        return (data != nullptr && size > 0)
            ? keel::StringRef(reinterpret_cast<const char*>(data), size)
            : keel::StringRef{};
    }
};

namespace detail {

struct EmbeddedAssetEntry {
    keel::StringRef path;
    keel::StringRef mimeType;
    const uint8_t* data{nullptr};
    size_t size{0};
};

/// Provided by the build-time generated embedded_assets.cpp translation unit.
[[nodiscard]] keel::ArrayRef<EmbeddedAssetEntry> GetEmbeddedAssets() noexcept;

} // namespace detail

/// Unified virtual asset registry supporting build-time embedded assets (Native + WASM)
/// and runtime in-memory overrides.
class Assets {
public:
    /// Normalizes a virtual asset path:
    /// - Converts '\' to '/'
    /// - Strips leading "./", "../", and "/" prefixes
    /// - Canonicalizes "images/..." or "fonts/..." to "assets/..." when matching embedded roots
    [[nodiscard]] static keel::String NormalizePath(keel::StringRef path);

    /// Infers a short format/MIME extension ("png", "svg", "jpg", "ttf", "otf") from a path.
    [[nodiscard]] static keel::StringRef InferMimeType(keel::StringRef path) noexcept;

    /// Looks up an asset by virtual path (checks runtime overrides first, then build-time embedded assets).
    [[nodiscard]] static keel::Optional<AssetView> Find(keel::StringRef virtualPath);

    /// Returns true if the virtual asset exists in the runtime or build-time registry.
    [[nodiscard]] static bool Exists(keel::StringRef virtualPath);

    /// Registers or overrides a non-owning in-memory asset at runtime.
    /// The caller must ensure `data` remains valid for the lifetime of the registration.
    static void Register(
        keel::StringRef virtualPath,
        const uint8_t* data,
        size_t size,
        keel::StringRef mimeType = {}
    );

    /// Registers or overrides an owned byte buffer in the runtime asset registry.
    static void RegisterOwned(
        keel::StringRef virtualPath,
        keel::ArrayRef<uint8_t> bytes,
        keel::StringRef mimeType = {}
    );

    /// Removes a runtime-registered asset override (restoring any build-time embedded asset at that path).
    static bool Unregister(keel::StringRef virtualPath);

    /// Clears all runtime-registered asset overrides.
    static void ClearRuntime();

    /// Returns a list of all available virtual asset paths (embedded + runtime).
    [[nodiscard]] static keel::Array<keel::String> ListPaths();
};

} // namespace guikit
