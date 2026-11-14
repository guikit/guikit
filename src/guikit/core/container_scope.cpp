#include "guikit/core/container_scope.hpp"
#include "guikit/context.hpp"

namespace guikit {

ContainerScope::~ContainerScope() {
    Close();
}

ContainerScope& ContainerScope::operator=(ContainerScope&& other) noexcept {
    if (this != &other) {
        Close();
        m_ctx = other.m_ctx;
        m_node = other.m_node;
        other.m_ctx = nullptr;
        other.m_node = nullptr;
    }
    return *this;
}

void ContainerScope::Close() noexcept {
    if (m_ctx && m_node) {
        m_ctx->PopContainer(m_node);
        m_ctx = nullptr;
        m_node = nullptr;
    }
}

} // namespace guikit
