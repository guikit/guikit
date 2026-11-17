#include "guikit/core/zone.hpp"
#include "guikit/context.hpp"

namespace guikit {

ZoneScope::ZoneScope(Context& ctx, const ZoneOptions& opts)
    : m_ctx(&ctx) {
    ctx.PushZone(opts);
}

ZoneScope::ZoneScope(Context& ctx, const ZoneState& state)
    : m_ctx(&ctx) {
    ctx.PushZoneState(state);
}

ZoneScope::~ZoneScope() {
    if (m_ctx != nullptr) {
        m_ctx->PopZone();
    }
}

ZoneScope& ZoneScope::operator=(ZoneScope&& other) noexcept {
    if (this != &other) {
        if (m_ctx != nullptr) {
            m_ctx->PopZone();
        }
        m_ctx = other.m_ctx;
        other.m_ctx = nullptr;
    }
    return *this;
}

} // namespace guikit
