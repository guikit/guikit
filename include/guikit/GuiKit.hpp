#pragma once

// ==============================================================================
// GuiKit - Autonomous Immediate Mode Vector GUI Library (Umbrella Header)
// ==============================================================================

#define GUIKIT_VERSION_MAJOR 1
#define GUIKIT_VERSION_MINOR 0
#define GUIKIT_VERSION_PATCH 0
#define GUIKIT_VERSION_STRING "1.0.0"

#include "guikit/Core.hpp"
#include "guikit/Scenes.hpp"
#if __has_include("thorvg.h")
#include "guikit/backend/ThorCanvas.hpp"
#endif
#if __has_include(<SDL3/SDL.h>)
#include "guikit/backend/SdlWindowBackend.hpp"
#endif

namespace guikit {

struct Version {
    static constexpr int major = GUIKIT_VERSION_MAJOR;
    static constexpr int minor = GUIKIT_VERSION_MINOR;
    static constexpr int patch = GUIKIT_VERSION_PATCH;
    static constexpr std::string_view string = GUIKIT_VERSION_STRING;
};

} // namespace guikit
