#pragma once

// ==============================================================================
// GuiKit - Autonomous Immediate Mode Vector GUI Library (Core Umbrella Header)
// ==============================================================================

#define GUIKIT_VERSION_MAJOR 1
#define GUIKIT_VERSION_MINOR 0
#define GUIKIT_VERSION_PATCH 0
#define GUIKIT_VERSION_STRING "1.0.0"

#include "guikit/core.hpp"
#include "keel/core.h"

namespace guikit {

struct Version {
    static constexpr keel::i32 major = GUIKIT_VERSION_MAJOR;
    static constexpr keel::i32 minor = GUIKIT_VERSION_MINOR;
    static constexpr keel::i32 patch = GUIKIT_VERSION_PATCH;
    static constexpr keel::StringRef string = GUIKIT_VERSION_STRING;
};

} // namespace guikit
