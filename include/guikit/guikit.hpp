#pragma once

// ==============================================================================
// GuiKit - Autonomous Immediate Mode Vector GUI Library (Umbrella Header)
// ==============================================================================

#define GUIKIT_VERSION_MAJOR 1
#define GUIKIT_VERSION_MINOR 0
#define GUIKIT_VERSION_PATCH 0
#define GUIKIT_VERSION_STRING "1.0.0"

#include "guikit/core.hpp"
#if defined(GUIKIT_HAS_SCENES) && GUIKIT_HAS_SCENES
#include "guikit/scenes.hpp"
#endif

namespace guikit {

#if defined(GUIKIT_HAS_BACKEND_THORVG) && GUIKIT_HAS_BACKEND_THORVG
void RegisterThorVectorBackend() noexcept;
#endif
#if defined(GUIKIT_HAS_BACKEND_SDL3) && GUIKIT_HAS_BACKEND_SDL3
void RegisterSdlWindowBackend() noexcept;
#endif

#if (defined(GUIKIT_HAS_BACKEND_THORVG) && GUIKIT_HAS_BACKEND_THORVG) || \
    (defined(GUIKIT_HAS_BACKEND_SDL3) && GUIKIT_HAS_BACKEND_SDL3)
namespace detail {
struct DefaultBackendsAutoRegistrar {
    DefaultBackendsAutoRegistrar() noexcept {
#if defined(GUIKIT_HAS_BACKEND_THORVG) && GUIKIT_HAS_BACKEND_THORVG
        RegisterThorVectorBackend();
#endif
#if defined(GUIKIT_HAS_BACKEND_SDL3) && GUIKIT_HAS_BACKEND_SDL3
        RegisterSdlWindowBackend();
#endif
    }
};
inline const DefaultBackendsAutoRegistrar g_defaultBackendsAutoRegistrar{};
} // namespace detail
#endif

struct Version {
    static constexpr int major = GUIKIT_VERSION_MAJOR;
    static constexpr int minor = GUIKIT_VERSION_MINOR;
    static constexpr int patch = GUIKIT_VERSION_PATCH;
    static constexpr std::string_view string = GUIKIT_VERSION_STRING;
};

} // namespace guikit
