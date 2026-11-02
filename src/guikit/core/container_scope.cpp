#include "guikit/core/container_scope.hpp"
#include "guikit/context.hpp"

namespace guikit {

ContainerScope::~ContainerScope() {
    if (m_ctx && m_node) {
        m_ctx->PopContainer(m_node);
        m_ctx = nullptr;
        m_node = nullptr;
    }
}

} // namespace guikit
