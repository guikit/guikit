#include "guikit/layout/Column.hpp"
#include "guikit/Context.hpp"

namespace guikit {

ColumnScope Column::Open(
    Context& ctx,
    const ColumnOptions& opts,
    std::source_location loc
) {
    return ctx.Column(opts, loc);
}

} // namespace guikit
