#include "guikit/core/container_scope.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"

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

Rect ContainerScope::GetBounds() const noexcept {
    if (!m_ctx || !m_node) return {};
    return m_ctx->GetPrevFrameBounds(m_node->id);
}

bool ContainerScope::IsHovered() const noexcept {
    if (!m_ctx || !m_node || m_node->disabled) return false;
    const Rect bounds = GetBounds();
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return false;
    return m_ctx->IsRectHovered(m_node->id, bounds);
}

bool ContainerScope::IsTriggered(InteractionTrigger trigger) const noexcept {
    if (!IsHovered()) return false;
    switch (trigger) {
        case InteractionTrigger::Click:
            return !m_ctx->GetActiveId().IsValid() && m_ctx->IsMouseClicked();
        case InteractionTrigger::RightClick:
            return m_ctx->IsRightMouseClicked();
        case InteractionTrigger::DoubleClick:
            return !m_ctx->GetActiveId().IsValid() && m_ctx->IsMouseDoubleClicked();
    }
    return false;
}

} // namespace guikit
