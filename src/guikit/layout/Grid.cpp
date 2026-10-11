#include "guikit/layout/Grid.hpp"
#include "guikit/Context.hpp"

namespace guikit {

GridScope Grid::Open(
    Context& ctx,
    const GridOptions& opts,
    std::source_location loc
) {
    return ctx.Grid(opts, loc);
}

} // namespace guikit
