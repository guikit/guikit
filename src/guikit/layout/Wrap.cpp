#include "guikit/layout/Wrap.hpp"
#include "guikit/Context.hpp"

namespace guikit {

ContainerScope Wrap::Open(
    Context& ctx,
    const WrapOptions& opts,
    std::source_location loc
) {
    return ctx.Wrap(opts, loc);
}

} // namespace guikit
