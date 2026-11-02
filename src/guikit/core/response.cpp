#include "guikit/core/response.hpp"
#include "guikit/context.hpp"

namespace guikit {

void Response::Focus() {
    if (ctx && id.IsValid()) {
        ctx->RequestFocus(id);
    }
}

void Response::ScrollTo() {
    if (ctx && id.IsValid()) {
        ctx->ScrollToId(id);
    }
}

} // namespace guikit
