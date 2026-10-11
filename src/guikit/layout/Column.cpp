#include "guikit/layout/Column.hpp"
#include "guikit/Context.hpp"

namespace guikit {

ContainerScope Column::Open(
    Context& ctx,
    const ColumnOptions& opts,
    std::source_location loc
) {
    return ctx.Column(opts, loc);
}

} // namespace guikit
