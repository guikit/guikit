#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace guikit {

/// Non-owning view over an embedded or runtime-registered binary asset.
struct AssetView {
    const uint8_t* data{nullptr};
    size_t size{0};
    std::string_view mimeType{}; // Format/extension hint: "png", "svg", "jpg", "ttf", etc.

    [[nodiscard]] constexpr bool Empty() const noexcept {
        return data == nullptr || size == 0;
    }

    [[nodiscard]] constexpr std::span<const uint8_t> Bytes() const noexcept {
        return {data, size};
    }

    [[nodiscard]] constexpr std::string_view AsStringView() const noexcept {
        return (data != nullptr && size > 0)
            ? std::string_view(reinterpret_cast<const char*>(data), size)
            : std::string_view{};
    }
};

namespace detail {

struct EmbeddedAssetEntry {
    std::string_view path;
    std::string_view mimeType;
    const uint8_t* data{nullptr};
    size_t size{0};
};

/// Provided by the build-time generated EmbeddedAssets.cpp translation unit.
[[nodiscard]] std::span<const EmbeddedAssetEntry> GetEmbeddedAssets() noexcept;

} // namespace detail

/// Unified virtual asset registry supporting build-time embedded assets (Native + WASM)
/// and runtime in-memory overrides.
class Assets {
public:
    /// Normalizes a virtual asset path:
    /// - Converts '\' to '/'
    /// - Strips leading "./", "../", and "/" prefixes
    /// - Canonicalizes "images/..." or "fonts/..." to "assets/..." when matching embedded roots
    [[nodiscard]] static std::string NormalizePath(std::string_view path);

    /// Infers a short format/MIME extension ("png", "svg", "jpg", "ttf", "otf") from a path.
    [[nodiscard]] static std::string_view InferMimeType(std::string_view path) noexcept;

    /// Looks up an asset by virtual path (checks runtime overrides first, then build-time embedded assets).
    [[nodiscard]] static std::optional<AssetView> Find(std::string_view virtualPath);

    /// Returns true if the virtual asset exists in the runtime or build-time registry.
    [[nodiscard]] static bool Exists(std::string_view virtualPath);

    /// Registers or overrides a non-owning in-memory asset at runtime.
    /// The caller must ensure `data` remains valid for the lifetime of the registration.
    static void Register(
        std::string_view virtualPath,
        const uint8_t* data,
        size_t size,
        std::string_view mimeType = {}
    );

    /// Registers or overrides an owned byte buffer in the runtime asset registry.
    static void RegisterOwned(
        std::string_view virtualPath,
        std::vector<uint8_t> bytes,
        std::string_view mimeType = {}
    );

    /// Removes a runtime-registered asset override (restoring any build-time embedded asset at that path).
    static bool Unregister(std::string_view virtualPath);

    /// Clears all runtime-registered asset overrides.
    static void ClearRuntime();

    /// Returns a list of all available virtual asset paths (embedded + runtime).
    [[nodiscard]] static std::vector<std::string> ListPaths();
};

} // namespace guikit
