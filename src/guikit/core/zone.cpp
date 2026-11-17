#include "guikit/core/zone.hpp"
#include "guikit/context.hpp"

namespace guikit {

ZoneScope::ZoneScope(Context& ctx, const ZoneOptions& opts)
    : m_ctx(&ctx)
    , m_theme(ctx.PushZone(opts))
    , m_depth(ctx.GetZoneDepth()) {}

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
        m_theme = other.m_theme;
        m_depth = other.m_depth;
        other.m_ctx = nullptr;
        other.m_theme = nullptr;
    }
    return *this;
}

void ZoneScope::Detach() noexcept {
    if (m_ctx != nullptr) {
        m_ctx->ReleaseZone(m_depth);
        m_ctx = nullptr;
        m_theme = nullptr;
    }
}

} // namespace guikit
