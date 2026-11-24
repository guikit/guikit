#pragma once

#include "guikit/context.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring an auto-wrapping flow layout.
struct WrapOptions {
    keel::Optional<keel::f32> gap{keel::kNullOpt};        ///< Horizontal gap between items on same line
    keel::Optional<keel::f32> runGap{keel::kNullOpt};     ///< Vertical gap between consecutive wrapped lines
    keel::Optional<Padding> padding{keel::kNullOpt};
    Alignment align{Alignment::Center};
    Justify justify{Justify::Start};
    AxisSize width{SizeMode::Stretch};
    AxisSize height{SizeMode::Fit};
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};
    keel::f32 flexGrow{1.0f};
    bool disabled{false};
};

/// Autonomous immediate mode Wrap layout entry point.
namespace detail {

class Wrap {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const WrapOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace detail

[[nodiscard]] inline ContainerScope Wrap(
    Context& ui,
    WrapOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::Wrap::Open(ui, opts, loc);
}

} // namespace guikit
