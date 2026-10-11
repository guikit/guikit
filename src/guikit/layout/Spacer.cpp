#include "guikit/layout/Spacer.hpp"
#include "guikit/Context.hpp"

namespace guikit {

void Spacer::Run(
    Context& ctx,
    float flex,
    std::source_location loc
) {
    ctx.Spacer(flex, loc);
}

} // namespace guikit
