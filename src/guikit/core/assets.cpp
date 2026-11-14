#include "guikit/core/assets.hpp"
#include "keel/algo.h"
#include "keel/array.h"
#include "keel/hash_map.h"
#include "keel/string.h"

namespace guikit {

namespace {

struct RuntimeAssetRecord {
    keel::Array<uint8_t> ownedBytes{}; // Always padded with a trailing '\0' when non-empty
    const uint8_t* externalPtr{nullptr};
    size_t size{0};
    keel::String mimeType{};

    [[nodiscard]] const uint8_t* DataPtr() const noexcept {
        return externalPtr != nullptr ? externalPtr : (ownedBytes.empty() ? nullptr : ownedBytes.data());
    }
};

keel::HashMap<keel::String, RuntimeAssetRecord>& GetRuntimeRegistry() {
    static keel::HashMap<keel::String, RuntimeAssetRecord> s_registry;
    return s_registry;
}

} // namespace

keel::String Assets::NormalizePath(keel::StringRef path) {
    path = path.trim();
    if (path.empty()) {
        return keel::String{};
    }

    // Convert '\' to '/' and collapse duplicate slashes in one pass
    keel::String collapsed = keel::String::with_capacity(path.size());
    for (char rawC : path) {
        const char c = (rawC == '\\') ? '/' : rawC;
        if (c == '/' && !collapsed.empty() && collapsed.back() == '/') {
            continue;
        }
        collapsed.push_back(c);
    }

    keel::StringRef sv = collapsed.as_ref();
    // Strip leading "./", "../", "/" segments
    for (;;) {
        if (sv.starts_with("./")) {
            sv.remove_prefix(2);
        } else if (sv.starts_with("../")) {
            sv.remove_prefix(3);
        } else if (sv.starts_with('/')) {
            sv.remove_prefix(1);
        } else {
            break;
        }
    }

    // If an absolute or nested build path contains "/assets/", canonicalize from "assets/"
    if (!sv.starts_with("assets/")) {
        const size_t pos = sv.find("/assets/");
        if (pos != keel::StringRef::npos) {
            sv.remove_prefix(pos + 1);
        }
    }

    if (sv.size() == collapsed.size()) {
        return collapsed;
    }
    return keel::String::from(sv);
}

keel::StringRef Assets::InferMimeType(keel::StringRef path) noexcept {
    const size_t dot = path.rfind('.');
    if (dot == keel::StringRef::npos || dot + 1 >= path.size()) {
        return {};
    }
    const keel::StringRef ext = path.substr(dot + 1);
    if (ext.equals_ignore_ascii_case("png"))  return "png";
    if (ext.equals_ignore_ascii_case("svg"))  return "svg";
    if (ext.equals_ignore_ascii_case("jpg") || ext.equals_ignore_ascii_case("jpeg")) return "jpg";
    if (ext.equals_ignore_ascii_case("ttf"))  return "ttf";
    if (ext.equals_ignore_ascii_case("otf"))  return "otf";
    if (ext.equals_ignore_ascii_case("webp")) return "webp";
    if (ext.equals_ignore_ascii_case("json") || ext.equals_ignore_ascii_case("lot")) return "lottie+json";
    return ext;
}

keel::Optional<AssetView> Assets::Find(keel::StringRef virtualPath) {
    const keel::String norm = NormalizePath(virtualPath);
    if (norm.empty()) {
        return keel::kNullOpt;
    }

    const auto& runtimeReg = GetRuntimeRegistry();
    if (const RuntimeAssetRecord* rec = runtimeReg.get(norm.as_ref())) {
        return AssetView{
            .data = rec->DataPtr(),
            .size = rec->size,
            .mimeType = rec->mimeType.as_ref()
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
        keel::String withPrefix = keel::String::with_capacity(7 + norm.size());
        withPrefix.append("assets/");
        withPrefix.append(norm);
        if (const RuntimeAssetRecord* rec = runtimeReg.get(withPrefix.as_ref())) {
            return AssetView{
                .data = rec->DataPtr(),
                .size = rec->size,
                .mimeType = rec->mimeType.as_ref()
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

    return keel::kNullOpt;
}

bool Assets::Exists(keel::StringRef virtualPath) {
    const auto found = Find(virtualPath);
    return found.has_value() && !found->Empty();
}

void Assets::Register(
    keel::StringRef virtualPath,
    const uint8_t* data,
    size_t size,
    keel::StringRef mimeType
) {
    keel::String norm = NormalizePath(virtualPath);
    if (norm.empty() || data == nullptr || size == 0) {
        return;
    }
    RuntimeAssetRecord rec{};
    rec.externalPtr = data;
    rec.size = size;
    rec.mimeType = mimeType.empty() ? keel::String::from(InferMimeType(norm)) : keel::String::from(mimeType);
    GetRuntimeRegistry().put(keel::move(norm), keel::move(rec));
}

void Assets::RegisterOwned(
    keel::StringRef virtualPath,
    keel::ArrayRef<uint8_t> bytes,
    keel::StringRef mimeType
) {
    keel::String norm = NormalizePath(virtualPath);
    if (norm.empty() || bytes.empty()) {
        return;
    }
    RuntimeAssetRecord rec{};
    rec.size = bytes.size();
    rec.ownedBytes.reserve(bytes.size() + 1);
    rec.ownedBytes.append(bytes);
    rec.ownedBytes.push_back(0); // Null-terminate for non-copy SVG/text loaders
    rec.mimeType = mimeType.empty() ? keel::String::from(InferMimeType(norm)) : keel::String::from(mimeType);
    GetRuntimeRegistry().put(keel::move(norm), keel::move(rec));
}

bool Assets::Unregister(keel::StringRef virtualPath) {
    const keel::String norm = NormalizePath(virtualPath);
    if (norm.empty()) {
        return false;
    }
    return GetRuntimeRegistry().remove(norm.as_ref());
}

void Assets::ClearRuntime() {
    GetRuntimeRegistry().clear();
}

keel::Array<keel::String> Assets::ListPaths() {
    keel::Array<keel::String> result;
    const auto embedded = detail::GetEmbeddedAssets();
    const auto& runtimeReg = GetRuntimeRegistry();
    result.reserve(embedded.size() + runtimeReg.size());

    for (const auto& entry : embedded) {
        result.push_back(keel::String::from(entry.path));
    }
    for (const auto& [path, _] : runtimeReg) {
        bool found = false;
        for (const auto& existing : result) {
            if (existing == path) {
                found = true;
                break;
            }
        }
        if (!found) {
            result.push_back(path.clone());
        }
    }
    keel::sort(result);
    return result;
}

} // namespace guikit
