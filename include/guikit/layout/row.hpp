#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a horizontal Row layout.
struct RowOptions {
    keel::Optional<float> gap{keel::kNullOpt};
    keel::Optional<Padding> padding{keel::kNullOpt};
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    AxisSize width{};
    AxisSize height{SizeMode::Fit};
    keel::Optional<float> minWidth{keel::kNullOpt};
    keel::Optional<float> maxWidth{keel::kNullOpt};
    keel::Optional<float> minHeight{keel::kNullOpt};
    keel::Optional<float> maxHeight{keel::kNullOpt};
    float flexGrow{1.0f};
    bool equalWidths{false};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    keel::Optional<float> scale{keel::kNullOpt};
    keel::Optional<bool> debugBounds{keel::kNullOpt};
    keel::Optional<bool> inspectMode{keel::kNullOpt};
};

/// Autonomous immediate mode Row layout entry point.
class Row {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const RowOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace guikit
