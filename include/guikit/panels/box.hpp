#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/core/zone.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a visual Box container.
struct BoxOptions {
    keel::Optional<float> gap{keel::kNullOpt};        ///< Gap between stacked children (nullopt = from theme)
    AxisSize width{SizeMode::Stretch};
    AxisSize height{SizeMode::Fit};
    keel::Optional<float> minWidth{keel::kNullOpt};
    keel::Optional<float> maxWidth{keel::kNullOpt};
    keel::Optional<float> minHeight{keel::kNullOpt};
    keel::Optional<float> maxHeight{keel::kNullOpt};
    float flexGrow{1.0f};
    keel::Optional<Padding> padding{keel::kNullOpt};   ///< Container padding (nullopt = zero)
    StateColor background{};
    Border border{};
    bool clipChildren{false};
    bool disabled{false};
    ZoneOptions zone{}; ///< Subtree environment (theme, scale, density, debug/inspect)
};

/// Autonomous immediate mode Box container entry point.
class Box {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const BoxOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace guikit
