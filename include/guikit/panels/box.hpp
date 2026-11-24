#pragma once

#include "guikit/context.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a visual Box container.
struct BoxOptions {
    keel::Optional<keel::f32> gap{keel::kNullOpt};        ///< Gap between stacked children (nullopt = from theme)
    AxisSize width{SizeMode::Stretch};
    AxisSize height{SizeMode::Fit};
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};
    keel::f32 flexGrow{1.0f};
    keel::Optional<Padding> padding{keel::kNullOpt};   ///< Container padding (nullopt = zero)
    StateColor background{};
    Border border{};
    bool clipChildren{false};
    bool disabled{false};
};

/// Autonomous immediate mode Box container entry point.
namespace detail {

class Box {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const BoxOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace detail

[[nodiscard]] inline ContainerScope Box(
    Context& ui,
    BoxOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::Box::Open(ui, opts, loc);
}

} // namespace guikit
