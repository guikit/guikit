#pragma once

#include "keel/core.h"

namespace guikit {

class Context;

/// Autonomous immediate mode Spacer & fixed Gap layout entry point.
class Spacer {
public:
    /// Inserts an elastic spring spacer with proportional `flex` weight (`flex = 1.0f` by default).
    static void Run(
        Context& ctx,
        float flex = 1.0f,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Inserts a fixed non-stretching gap of `sizeDp` logical dp along the parent container's main axis.
    static void Gap(
        Context& ctx,
        float sizeDp = 8.0f,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace guikit
