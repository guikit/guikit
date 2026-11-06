#include "guikit/core/assets.hpp"
#include <algorithm>
#include "keel/array.h"
#include "keel/hash_map.h"
#include "keel/string.h"

namespace guikit {

namespace {

struct RuntimeAssetRecord {
    keel::Array<uint8_t> ownedBytes{}; // Always padded with a trailing '\0' when non-empty
    const uint8_t* externalPtr{nullptr};
    size_t size{0};
    std::string mimeType{};

    [[nodiscard]] const uint8_t* DataPtr() const noexcept {
        return externalPtr != nullptr ? externalPtr : (ownedBytes.empty() ? nullptr : ownedBytes.data());
    }
};

keel::HashMap<keel::String, RuntimeAssetRecord>& GetRuntimeRegistry() {
    static keel::HashMap<keel::String, RuntimeAssetRecord> s_registry;
    return s_registry;
}

[[nodiscard]] bool EqualsIgnoreCase(std::string_view a, std::string_view b) noexcept {
    return keel::StringRef(a.data(), a.size()).equals_ignore_ascii_case(keel::StringRef(b.data(), b.size()));
}

} // namespace

std::string Assets::NormalizePath(std::string_view path) {
    const keel::StringRef trimmed = keel::StringRef(path.data(), path.size()).trim();
    path = std::string_view(trimmed.data(), trimmed.size());
    if (path.empty()) {
        return {};
    }

    std::string normalized;
    normalized.reserve(path.size());
    for (char c : path) {
        normalized.push_back(c == '\\' ? '/' : c);
    }

    // Collapse duplicate slashes
    std::string collapsed;
    collapsed.reserve(normalized.size());
    for (char c : normalized) {
        if (c == '/' && !collapsed.empty() && collapsed.back() == '/') {
            continue;
        }
        collapsed.push_back(c);
    }

    std::string_view sv(collapsed);
    // Strip leading "./", "../", "/" segments
    for (;;) {
        if (sv.starts_with("./")) {
            sv.remove_prefix(2);
        } else if (sv.starts_with("../")) {
            sv.remove_prefix(3);
        } else if (sv.starts_with("/")) {
            sv.remove_prefix(1);
        } else {
            break;
        }
    }

    // If an absolute or nested build path contains "/assets/", canonicalize from "assets/"
    if (!sv.starts_with("assets/")) {
        const size_t pos = sv.find("/assets/");
        if (pos != std::string_view::npos) {
            sv.remove_prefix(pos + 1);
        }
    }

    return std::string(sv);
}

std::string_view Assets::InferMimeType(std::string_view path) noexcept {
    const size_t dot = path.find_last_of('.');
    if (dot == std::string_view::npos || dot + 1 >= path.size()) {
        return {};
    }
    const std::string_view ext = path.substr(dot + 1);
    if (EqualsIgnoreCase(ext, "png"))  return "png";
    if (EqualsIgnoreCase(ext, "svg"))  return "svg";
    if (EqualsIgnoreCase(ext, "jpg") || EqualsIgnoreCase(ext, "jpeg")) return "jpg";
    if (EqualsIgnoreCase(ext, "ttf"))  return "ttf";
    if (EqualsIgnoreCase(ext, "otf"))  return "otf";
    if (EqualsIgnoreCase(ext, "webp")) return "webp";
    if (EqualsIgnoreCase(ext, "json") || EqualsIgnoreCase(ext, "lot")) return "lottie+json";
    return ext;
}

std::optional<AssetView> Assets::Find(std::string_view virtualPath) {
    const std::string norm = NormalizePath(virtualPath);
    if (norm.empty()) {
        return std::nullopt;
    }

    const auto& runtimeReg = GetRuntimeRegistry();
    if (const RuntimeAssetRecord* rec = runtimeReg.get(keel::StringRef(norm.data(), norm.size()))) {
        return AssetView{
            .data = rec->DataPtr(),
            .size = rec->size,
            .mimeType = rec->mimeType
        };
    }

    const auto embedded = detail::GetEmbeddedAssets();
    for (const auto& entry : embedded) {
        if (entry.path == norm) {
            return AssetView{
                .data = entry.data,
                .size = entry.size,
                .mimeType = entry.mimeType
            };
        }
    }

    // Also allow lookup without the leading "assets/" prefix (e.g. "images/lena.png" -> "assets/images/lena.png")
    if (!norm.starts_with("assets/")) {
        const std::string withPrefix = "assets/" + norm;
        if (const RuntimeAssetRecord* rec = runtimeReg.get(keel::StringRef(withPrefix.data(), withPrefix.size()))) {
            return AssetView{
                .data = rec->DataPtr(),
                .size = rec->size,
                .mimeType = rec->mimeType
            };
        }
        for (const auto& entry : embedded) {
            if (entry.path == withPrefix) {
                return AssetView{
                    .data = entry.data,
                    .size = entry.size,
                    .mimeType = entry.mimeType
                };
            }
        }
    }

    return std::nullopt;
}

bool Assets::Exists(std::string_view virtualPath) {
    const auto found = Find(virtualPath);
    return found.has_value() && !found->Empty();
}

void Assets::Register(
    std::string_view virtualPath,
    const uint8_t* data,
    size_t size,
    std::string_view mimeType
) {
    const std::string norm = NormalizePath(virtualPath);
    if (norm.empty() || data == nullptr || size == 0) {
        return;
    }
    RuntimeAssetRecord rec{};
    rec.externalPtr = data;
    rec.size = size;
    rec.mimeType = mimeType.empty() ? std::string(InferMimeType(norm)) : std::string(mimeType);
    GetRuntimeRegistry().put(keel::String::from(keel::StringRef(norm.data(), norm.size())), std::move(rec));
}

void Assets::RegisterOwned(
    std::string_view virtualPath,
    std::vector<uint8_t> bytes,
    std::string_view mimeType
) {
    const std::string norm = NormalizePath(virtualPath);
    if (norm.empty() || bytes.empty()) {
        return;
    }
    RuntimeAssetRecord rec{};
    rec.size = bytes.size();
    rec.ownedBytes.reserve(static_cast<keel::isize>(bytes.size() + 1));
    rec.ownedBytes.append(keel::ArrayRef<uint8_t>(bytes.data(), bytes.size()));
    rec.ownedBytes.push_back(0); // Null-terminate for non-copy SVG/text loaders
    rec.mimeType = mimeType.empty() ? std::string(InferMimeType(norm)) : std::string(mimeType);
    GetRuntimeRegistry().put(keel::String::from(keel::StringRef(norm.data(), norm.size())), std::move(rec));
}

bool Assets::Unregister(std::string_view virtualPath) {
    const std::string norm = NormalizePath(virtualPath);
    if (norm.empty()) {
        return false;
    }
    return GetRuntimeRegistry().remove(keel::StringRef(norm.data(), norm.size()));
}

void Assets::ClearRuntime() {
    GetRuntimeRegistry().clear();
}

std::vector<std::string> Assets::ListPaths() {
    std::vector<std::string> result;
    const auto embedded = detail::GetEmbeddedAssets();
    const auto& runtimeReg = GetRuntimeRegistry();
    result.reserve(embedded.size() + runtimeReg.size());

    for (const auto& entry : embedded) {
        result.emplace_back(entry.path);
    }
    for (const auto& [path, _] : runtimeReg) {
        const std::string_view sv(path.data(), path.size());
        if (std::find(result.begin(), result.end(), sv) == result.end()) {
            result.emplace_back(sv);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace guikit
