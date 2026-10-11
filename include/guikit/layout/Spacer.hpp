#pragma once

#include <source_location>

namespace guikit {

class Context;

/// Autonomous immediate mode Spacer layout entry point.
class Spacer {
public:
    static void Run(
        Context& ctx,
        float flex = 1.0f,
        std::source_location loc = std::source_location::current()
    );
};

} // namespace guikit
