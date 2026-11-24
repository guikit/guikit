#pragma once

#include "guikit/context.hpp"
#include "keel/core.h"

namespace guikit {

class Context;

/// Autonomous immediate mode Spacer & fixed Gap layout entry point.
namespace detail {

class Spacer {
public:
    /// Inserts an elastic spring spacer with proportional `flex` weight (`flex = 1.0f` by default).
    static void Run(
        Context& ctx,
        keel::f32 flex = 1.0f,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Inserts a fixed non-stretching gap of `sizeDp` logical dp along the parent container's main axis.
    static void Gap(
        Context& ctx,
        keel::f32 sizeDp = 8.0f,
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace detail

inline void Spacer(
    Context& ui,
    keel::f32 flex = 1.0f,
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    WidgetExecGuard guard(ui);
    detail::Spacer::Run(ui, flex, loc);
}

inline void Gap(
    Context& ui,
    keel::f32 sizeDp = 8.0f,
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    detail::Spacer::Gap(ui, sizeDp, loc);
}

} // namespace guikit
