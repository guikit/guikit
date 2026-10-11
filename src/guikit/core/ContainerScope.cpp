#include "guikit/core/ContainerScope.hpp"
#include "guikit/Context.hpp"

namespace guikit {

ContainerScope::~ContainerScope() {
    if (m_ctx && m_node) {
        m_ctx->PopContainer(m_node);
        m_ctx = nullptr;
        m_node = nullptr;
    }
}

} // namespace guikit
