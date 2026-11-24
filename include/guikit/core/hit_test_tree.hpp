#pragma once

#include "guikit/core/id.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/types.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/hash_map.h"
#include "keel/ref.h"

namespace guikit {

struct UiTheme;

/// Retained Z-ordered hit-testing record captured at the end of each frame.
struct HitNodeRecord {
    Id id{Id::Null()};
    Id portalId{Id::Null()};
    Rect rect{};
    keel::u32 preIndex{0};
    keel::u32 postIndex{0};
    NodeType type{NodeType::Box};
    keel::u8 zLayer{0};
    bool blocksHit{false};
};

/// Manages previous/current frame node bounds, coordinate-space transforms, and Z-order hit records.
class HitTestTree {
public:
    HitTestTree() = default;
    ~HitTestTree() = default;

    void BeginFrame() noexcept;

    /// Walks the laid-out node tree to populate current-frame bounds, node spaces, and Z-order hit records.
    void CollectFromTree(Node* root, Vec2 logicalSize, const UiTheme& rootTheme);

    /// Promotes current-frame bounds/hit-records into previous-frame lookup tables for next frame's hit testing.
    void CommitFrame();

    [[nodiscard]] Transform2D GetNodeSpace(Id id) const noexcept;
    [[nodiscard]] Rect GetPrevFrameBounds(Id id) const noexcept;
    [[nodiscard]] Rect GetPrevUnclippedBounds(Id id) const noexcept;
    [[nodiscard]] const Rect* FindPrevBoundsPtr(Id id) const noexcept { return m_prevFrameBounds.get(id); }
    [[nodiscard]] const HitNodeRecord* FindPrevHitNode(Id id) const noexcept;
    [[nodiscard]] keel::ArrayRef<HitNodeRecord> GetPrevHitNodes() const noexcept { return m_prevHitNodes; }
    void ClearPopoverBlock(Id id) noexcept;

    [[nodiscard]] static bool DoesNodeBlockHit(const Node* node) noexcept;

private:
    keel::HashMap<Id, Rect> m_prevFrameBounds;
    keel::HashMap<Id, Rect> m_currFrameBounds;
    keel::HashMap<Id, Rect> m_prevUnclippedBounds;
    keel::HashMap<Id, Rect> m_currUnclippedBounds;
    keel::HashMap<Id, Transform2D> m_prevNodeSpaces;
    keel::HashMap<Id, Transform2D> m_currNodeSpaces;
    keel::Array<HitNodeRecord> m_prevHitNodes;
    keel::Array<HitNodeRecord> m_currHitNodes;
    keel::HashMap<Id, keel::usize> m_prevHitNodeMap;
};

} // namespace guikit
