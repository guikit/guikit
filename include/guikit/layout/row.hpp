#pragma once

#include "guikit/context.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

struct UiTheme;
class Context;

/// Options for configuring a horizontal Row layout.
struct RowOptions {
    keel::Optional<keel::f32> gap{keel::kNullOpt};
    keel::Optional<Padding> padding{keel::kNullOpt};
    Alignment align{Alignment::Stretch};
    Justify justify{Justify::Start};
    AxisSize width{};
    AxisSize height{SizeMode::Fit};
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};
    keel::f32 flexGrow{1.0f};
    bool equalWidths{false};
    bool disabled{false};
};

/// Autonomous immediate mode Row layout entry point.
namespace detail {

class Row {
public:
    [[nodiscard]] static ContainerScope Open(
        Context& ctx,
        const RowOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );
};

} // namespace detail

[[nodiscard]] inline ContainerScope Row(
    Context& ui,
    RowOptions opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::Row::Open(ui, opts, loc);
}

} // namespace guikit
