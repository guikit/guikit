#include "guikit/core/Response.hpp"
#include "guikit/Context.hpp"

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
