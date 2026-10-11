#include "guikit/layout/Row.hpp"
#include "guikit/Context.hpp"

namespace guikit {

RowScope Row::Open(
    Context& ctx,
    const RowOptions& opts,
    std::source_location loc
) {
    return ctx.Row(opts, loc);
}

} // namespace guikit
