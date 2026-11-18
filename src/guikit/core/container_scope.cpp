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

Response ContainerScope::GetResponse() const noexcept {
    Response res{};
    if (!m_ctx || !m_node) return res;
    res.id = m_node->id;
    res.ctx = m_ctx;
    res.rect = GetBounds();
    if (m_node->disabled || res.rect.w <= 0.0f || res.rect.h <= 0.0f) {
        return res;
    }
    res.hovered = m_ctx->IsRectHovered(m_node->id, res.rect);
    if (res.hovered) {
        const bool childActive = m_ctx->GetActiveId().IsValid() && m_ctx->GetActiveId() != m_node->id;
        res.clicked = !childActive && m_ctx->IsMouseClicked();
        res.doubleClicked = !childActive && m_ctx->IsMouseDoubleClicked();
        res.rightClicked = m_ctx->IsRightMouseClicked();
        res.pressed = !childActive && m_ctx->IsMouseDown();
    }
    return res;
}

} // namespace guikit
