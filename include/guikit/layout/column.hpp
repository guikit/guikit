#pragma once

#include "guikit/context.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/style/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a vertical Column layout.
struct ColumnOptions {
    keel::Optional<keel::f32> gap{keel::kNullOpt};
    keel::Optional<Padding> padding{keel::kNullOpt};
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    AxisSize width{SizeMode::Stretch};
    AxisSize height{SizeMode::Stretch};
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};
    keel::f32 flexGrow{1.0f};
    bool disabled{false};
    bool shell{false}; ///< Opt-in as an application shell container (applies theme.shell windowPadding, panelGap, and child panel framing)
};

/// Autonomous immediate mode Column layout entry point.
namespace detail {

class Column {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const ColumnOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace detail

[[nodiscard]] inline ContainerScope Column(
    Context& ui,
    ColumnOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::Column::Open(ui, opts, loc);
}

} // namespace guikit
