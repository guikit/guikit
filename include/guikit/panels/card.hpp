#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"
#include "keel/optional.h"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a visual Card surface container.
struct CardOptions {
    keel::Optional<keel::f32> minWidth{keel::kNullOpt}; ///< Minimum card width constraint (nullopt = auto content intrinsic)
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};
    AxisSize width{SizeMode::Stretch};
    AxisSize height{SizeMode::Fit};
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};
    keel::f32 flexGrow{1.0f};
    keel::Optional<keel::f32> gap{keel::kNullOpt};        ///< Gap between child elements inside card (nullopt = from theme)
    keel::Optional<Padding> padding{keel::kNullOpt};   ///< Card padding (nullopt = auto from theme)
    StateColor background{};
    Border border{.width = 1.0f};
    bool elevated{false};                           ///< Renders elevated surface background with soft ambient drop shadow
    bool clipChildren{true};                        ///< Clip children to prevent content bleeding outside rounded card
    bool disabled{false};
};

/// Autonomous immediate mode Card surface container entry point.
class Card {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const CardOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace guikit
