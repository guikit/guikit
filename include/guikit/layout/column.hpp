#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/core/zone.hpp"
#include "keel/core.h"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a vertical Column layout.
struct ColumnOptions {
    keel::Optional<float> gap{keel::kNullOpt};
    keel::Optional<Padding> padding{keel::kNullOpt};
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    AxisSize width{SizeMode::Stretch};
    AxisSize height{SizeMode::Stretch};
    keel::Optional<float> minWidth{keel::kNullOpt};
    keel::Optional<float> maxWidth{keel::kNullOpt};
    keel::Optional<float> minHeight{keel::kNullOpt};
    keel::Optional<float> maxHeight{keel::kNullOpt};
    float flexGrow{1.0f};
    bool disabled{false};
    ZoneOptions zone{}; ///< Subtree environment (theme, scale, density, debug/inspect)
};

/// Autonomous immediate mode Column layout entry point.
class Column {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const ColumnOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace guikit
