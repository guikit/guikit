#pragma once

#include "guikit/core/hit_test_tree.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/types.hpp"
#include "keel/core.h"

namespace guikit {

class ICanvas;
struct UiTheme;

/// Renders 1px layout debug bounds and interactive Chrome DevTools-style box-model inspection overlays.
class DebugInspector {
public:
    DebugInspector() = default;
    ~DebugInspector() = default;

    /// Returns true if state changed (caller should invalidate frame).
    bool SetDebugBounds(bool enabled) noexcept {
        if (m_debugBoundsEnabled != enabled) {
            m_debugBoundsEnabled = enabled;
            return true;
        }
        return false;
    }
    [[nodiscard]] bool IsDebugBounds() const noexcept { return m_debugBoundsEnabled; }

    /// Returns true if state changed (caller should invalidate frame).
    bool SetInspectMode(bool enabled) noexcept {
        if (m_inspectModeEnabled != enabled) {
            m_inspectModeEnabled = enabled;
            return true;
        }
        return false;
    }
    [[nodiscard]] bool IsInspectMode() const noexcept { return m_inspectModeEnabled; }

    [[nodiscard]] bool IsAnyScopedInspectActive() const noexcept { return m_anyScopedInspectActive; }
    [[nodiscard]] bool HadDebugBoundsLastFrame() const noexcept { return m_hadDebugBoundsLastFrame; }
    [[nodiscard]] bool HadInspectModeLastFrame() const noexcept { return m_hadInspectModeLastFrame; }

    void CommitFrameState(bool anyDebugBounds, bool anyInspectMode) noexcept {
        m_anyScopedInspectActive = anyInspectMode;
        m_hadDebugBoundsLastFrame = anyDebugBounds;
        m_hadInspectModeLastFrame = anyInspectMode;
    }

    void RenderDebugBounds(Node* root, ICanvas& canvas, const UiTheme& rootTheme) const;
    void RenderInspectOverlay(
        Node* root,
        ICanvas& canvas,
        Vec2 logicalSize,
        Vec2 mousePos,
        const UiTheme& rootTheme,
        const HitTestTree& hitTree
    ) const;

    [[nodiscard]] static Node* FindDeepestNodeAt(
        Node* root,
        Vec2 pos,
        Rect clipRect,
        const UiTheme& rootTheme
    ) noexcept;

private:
    void RenderDebugBoundsRecursive(
        Node* node,
        const Node* rootNode,
        ICanvas& canvas,
        const UiTheme& rootTheme,
        bool inheritedDebugBounds
    ) const;

    bool m_debugBoundsEnabled{false};
    bool m_inspectModeEnabled{false};
    bool m_anyScopedInspectActive{false};
    bool m_hadDebugBoundsLastFrame{false};
    bool m_hadInspectModeLastFrame{false};
};

} // namespace guikit
