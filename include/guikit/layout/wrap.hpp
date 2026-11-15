#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring an auto-wrapping flow layout.
struct WrapOptions {
    keel::Optional<float> gap{keel::kNullOpt};        ///< Horizontal gap between items on same line
    keel::Optional<float> runGap{keel::kNullOpt};     ///< Vertical gap between consecutive wrapped lines
    keel::Optional<Padding> padding{keel::kNullOpt};
    Alignment align{Alignment::Center};
    Justify justify{Justify::Start};
    AxisSize width{SizeMode::Stretch};
    AxisSize height{SizeMode::Fit};
    keel::Optional<float> minWidth{keel::kNullOpt};
    keel::Optional<float> maxWidth{keel::kNullOpt};
    keel::Optional<float> minHeight{keel::kNullOpt};
    keel::Optional<float> maxHeight{keel::kNullOpt};
    float flexGrow{1.0f};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    keel::Optional<float> scale{keel::kNullOpt};
    keel::Optional<bool> debugBounds{keel::kNullOpt};
    keel::Optional<bool> inspectMode{keel::kNullOpt};
};

/// Autonomous immediate mode Wrap layout entry point.
class Wrap {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const WrapOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace guikit
