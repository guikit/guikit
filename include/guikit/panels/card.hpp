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
    float minWidth{0.0f};                           ///< Minimum card width constraint (0 = auto content intrinsic)
    keel::Optional<float> maxWidth{keel::kNullOpt};
    keel::Optional<float> width{keel::kNullOpt};
    keel::Optional<float> height{keel::kNullOpt};
    keel::Optional<float> minHeight{keel::kNullOpt};
    keel::Optional<float> maxHeight{keel::kNullOpt};
    float flexGrow{1.0f};
    keel::Optional<float> gap{keel::kNullOpt};        ///< Gap between child elements inside card (nullopt = from theme)
    keel::Optional<Padding> padding{keel::kNullOpt};   ///< Card padding (nullopt = auto from theme)
    StateColor background{};
    Border border{.width = 1.0f};
    bool clipChildren{true};                        ///< Clip children to prevent content bleeding outside rounded card
    SizeMode widthMode{SizeMode::Stretch};
    SizeMode heightMode{SizeMode::Fit};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    keel::Optional<float> scale{keel::kNullOpt};
    keel::Optional<bool> debugBounds{keel::kNullOpt};
    keel::Optional<bool> inspectMode{keel::kNullOpt};
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
