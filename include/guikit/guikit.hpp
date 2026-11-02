#pragma once

// ==============================================================================
// GuiKit - Autonomous Immediate Mode Vector GUI Library (Umbrella Header)
// ==============================================================================

#define GUIKIT_VERSION_MAJOR 1
#define GUIKIT_VERSION_MINOR 0
#define GUIKIT_VERSION_PATCH 0
#define GUIKIT_VERSION_STRING "1.0.0"

#include "guikit/core.hpp"
#include "guikit/scenes.hpp"
#if __has_include("thorvg.h")
#include "guikit/backend/thor_canvas.hpp"
#endif
#if __has_include(<SDL3/SDL.h>)
#include "guikit/backend/sdl_window_backend.hpp"
#endif

namespace guikit {

struct Version {
    static constexpr int major = GUIKIT_VERSION_MAJOR;
    static constexpr int minor = GUIKIT_VERSION_MINOR;
    static constexpr int patch = GUIKIT_VERSION_PATCH;
    static constexpr std::string_view string = GUIKIT_VERSION_STRING;
};

} // namespace guikit
