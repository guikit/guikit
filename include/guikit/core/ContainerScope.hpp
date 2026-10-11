#pragma once

namespace guikit {

class Context;
struct Node;

/// RAII Scope guard for hierarchical layout and visual containers.
class [[nodiscard]] ContainerScope {
public:
    ContainerScope(Context* ctx, Node* node) noexcept
        : m_ctx(ctx), m_node(node) {}

    ~ContainerScope();

    ContainerScope(const ContainerScope&) = delete;
    ContainerScope& operator=(const ContainerScope&) = delete;

    ContainerScope(ContainerScope&& other) noexcept
        : m_ctx(other.m_ctx), m_node(other.m_node) {
        other.m_ctx = nullptr;
        other.m_node = nullptr;
    }

    ContainerScope& operator=(ContainerScope&& other) noexcept {
        if (this != &other) {
            m_ctx = other.m_ctx;
            m_node = other.m_node;
            other.m_ctx = nullptr;
            other.m_node = nullptr;
        }
        return *this;
    }

    /// Evaluates to true allowing `if (auto row = ui.Row()) { ... }` syntax.
    [[nodiscard]] constexpr operator bool() const noexcept {
        return m_node != nullptr;
    }

    [[nodiscard]] Node* GetNode() const noexcept {
        return m_node;
    }

protected:
    Context* m_ctx{nullptr};
    Node* m_node{nullptr};
};

} // namespace guikit
