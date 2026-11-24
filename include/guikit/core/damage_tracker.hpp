#pragma once

#include "guikit/core/id.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/types.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/hash_map.h"
#include "keel/math.h"
#include "keel/ref.h"

namespace guikit {

struct UiTheme;

/// Per-widget visual fingerprint used by `DamageTracker` for frame-to-frame dirty rect diffing.
struct WidgetSnapshot {
    Rect rect{};
    keel::u64 visualHash{0};
    keel::u32 flags{0}; ///< bits: 0=hovered, 1=pressed, 2=disabled, 3=checked, 4=cursorVisible, 5=activeEdit, 6=tabActive, 7=focused, 8..11=scrollbar
    keel::usize cursor{0};
    keel::usize selectionAnchor{0};
    keel::u32 bgColor{0};
    keel::u32 borderColor{0};

    constexpr bool operator==(const WidgetSnapshot& o) const noexcept {
        return rect == o.rect &&
               visualHash == o.visualHash &&
               flags == o.flags &&
               cursor == o.cursor &&
               selectionAnchor == o.selectionAnchor &&
               bgColor == o.bgColor &&
               borderColor == o.borderColor;
    }

    constexpr bool operator!=(const WidgetSnapshot& o) const noexcept {
        return !(*this == o);
    }
};

/// Active text-edit metadata folded into the edited node's damage snapshot.
struct EditSnapshotInfo {
    Id id{Id::Null()};
    keel::StringRef text{};
    keel::usize cursor{0};
    keel::usize selectionAnchor{0};
    bool cursorVisible{false};
};

/// Overlay and debug state flags influencing full-repaint vs partial dirty-rect decisions.
struct OverlayDamageInfo {
    bool anyOverlayStateChanged{false};
    bool hasActiveFullRepaintOverlay{false};
};

/// Computes minimal disjoint dirty damage rectangles between consecutive UI frames.
class DamageTracker {
public:
    DamageTracker() = default;
    ~DamageTracker() = default;

    void SetDirtyRectEnabled(bool enabled) noexcept {
        m_dirtyRectEnabled = enabled;
        m_forceFullRepaint = true;
    }
    [[nodiscard]] bool IsDirtyRectEnabled() const noexcept { return m_dirtyRectEnabled; }

    void InvalidateAll() noexcept { m_forceFullRepaint = true; }
    void InvalidateFrameRect(Rect frameRect) noexcept {
        if (frameRect.w > 0.0f && frameRect.h > 0.0f) {
            m_explicitDamageRects.push_back(frameRect);
        }
    }

    [[nodiscard]] bool HasDirtyDamage() const noexcept { return m_hasDirtyDamage; }
    [[nodiscard]] bool IsFullRepaint() const noexcept { return m_isFullRepaint; }
    [[nodiscard]] Rect GetDirtyRect() const noexcept { return m_dirtyRect; }
    [[nodiscard]] keel::ArrayRef<Rect> GetDirtyRects() const noexcept { return m_dirtyRects; }

    [[nodiscard]] Rect GetPhysicalDirtyRect(keel::f32 pixelsPerDp) const noexcept {
        const keel::f32 x1 = keel::floor(m_dirtyRect.x * pixelsPerDp);
        const keel::f32 y1 = keel::floor(m_dirtyRect.y * pixelsPerDp);
        const keel::f32 x2 = keel::ceil(m_dirtyRect.Right() * pixelsPerDp);
        const keel::f32 y2 = keel::ceil(m_dirtyRect.Bottom() * pixelsPerDp);
        return Rect(x1, y1, keel::max(0.0f, x2 - x1), keel::max(0.0f, y2 - y1));
    }

    /// Snapshots the frame graph, diffs against the previous frame, merges overlapping damage rectangles,
    /// and updates `HasDirtyDamage()` / `IsFullRepaint()` / `GetDirtyRects()`.
    void ComputeFrameDamage(
        Node* root,
        Vec2 logicalSize,
        const UiTheme& rootTheme,
        const EditSnapshotInfo& editInfo,
        keel::ArrayRef<Rect> extraDamageRects,
        bool debugBoundsEnabled,
        bool inspectModeEnabled,
        bool hadDebugBoundsLastFrame,
        bool hadInspectModeLastFrame,
        const OverlayDamageInfo& overlayInfo,
        bool& outAnyDebugBounds,
        bool& outAnyInspectMode
    );

private:
    keel::HashMap<Id, WidgetSnapshot> m_prevSnapshots;
    keel::Array<Rect> m_explicitDamageRects;
    keel::Array<Rect> m_dirtyRects;
    Rect m_dirtyRect{0.0f, 0.0f, 0.0f, 0.0f};
    bool m_hasDirtyDamage{true};
    bool m_isFullRepaint{true};
    bool m_forceFullRepaint{true};
    bool m_dirtyRectEnabled{true};
};

} // namespace guikit
