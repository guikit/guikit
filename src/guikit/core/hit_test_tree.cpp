#include "guikit/core/hit_test_tree.hpp"
#include "guikit/style/theme.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

void RecordBoundsCallback(void* user, Id id, Rect rect) {
    auto* map = static_cast<keel::HashMap<Id, Rect>*>(user);
    if (map && id.IsValid()) {
        map->put(id, rect);
    }
}

void CollectBoundsRecursive(
    Node* node,
    const Node* rootNode,
    keel::HashMap<Id, Rect>& outBounds,
    keel::HashMap<Id, Rect>& outUnclippedBounds,
    keel::Array<HitNodeRecord>& outHitNodes,
    keel::u32& ioOrderCounter,
    keel::u8 inheritedZLayer,
    Id inheritedPortalId,
    Rect parentClip,
    const UiTheme& theme,
    const Transform2D& parentSpace,
    keel::HashMap<Id, Transform2D>& outSpaces
) {
    if (!node) return;

    const UiTheme& activeTheme = node->GetTheme(theme);

    const Transform2D space = NodeSpace(node, parentSpace);
    if (!space.IsIdentity() && node->id.IsValid()) {
        outSpaces.put(node->id, space);
    }
    const Rect frameRect = space.Apply(node->computedRect);

    // Out-of-flow modals and popovers are rendered at top level above ancestor ScrollAreas/clips
    keel::u8 curZLayer = inheritedZLayer;
    Id curPortalId = inheritedPortalId;
    if (node->type == NodeType::Modal) {
        parentClip = frameRect;
        curZLayer = 2;
        if (node->id.IsValid()) curPortalId = node->id;
    } else if (node->type == NodeType::Popover) {
        parentClip = Rect(-100000.0f, -100000.0f, 200000.0f, 200000.0f);
        curZLayer = 2;
        if (node->id.IsValid()) curPortalId = node->id;
    } else if ((node->type == NodeType::Sidebar || node->type == NodeType::BottomPanel) && !node->IsCollapsed()) {
        curZLayer = keel::max<keel::u8>(curZLayer, 1);
    } else if (node->type == NodeType::StatusBar) {
        curZLayer = keel::max<keel::u8>(curZLayer, 1);
    }

    const keel::u32 preIdx = ++ioOrderCounter;
    keel::usize hitRecordIdx = keel::kInvalidIndex;

    if (node->id.IsValid()) {
        if ((frameRect.w > 0.0f || node->type == NodeType::Sidebar) &&
            (frameRect.h > 0.0f || node->type == NodeType::BottomPanel)) {
            outUnclippedBounds.put(node->id, frameRect);
        }
        const Rect clipped = frameRect.Intersect(parentClip);
        if ((clipped.w > 0.0f || node->type == NodeType::Sidebar) &&
            (clipped.h > 0.0f || node->type == NodeType::BottomPanel)) {
            outBounds.put(node->id, clipped);
        }

        if (node != rootNode && clipped.w > 0.0f && clipped.h > 0.0f) {
            hitRecordIdx = outHitNodes.size();
            HitNodeRecord rec{};
            rec.id = node->id;
            rec.portalId = curPortalId;
            rec.rect = clipped;
            rec.preIndex = preIdx;
            rec.postIndex = preIdx;
            rec.type = node->type;
            rec.zLayer = curZLayer;
            rec.blocksHit = HitTestTree::DoesNodeBlockHit(node);
            outHitNodes.push_back(rec);
        }

        if (node->panelHooks && node->panelHooks->collectBounds) {
            node->panelHooks->collectBounds(node, &RecordBoundsCallback, &outBounds, parentClip, space);
        }
    }

    Rect effectiveClip = parentClip;
    if (node->clipChildren) {
        effectiveClip = parentClip.Intersect(space.Apply(GetNodeContentClipRect(node, activeTheme)));
    }

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        CollectBoundsRecursive(
            child,
            rootNode,
            outBounds,
            outUnclippedBounds,
            outHitNodes,
            ioOrderCounter,
            curZLayer,
            curPortalId,
            effectiveClip,
            activeTheme,
            space,
            outSpaces
        );
    }

    if (hitRecordIdx != keel::kInvalidIndex) {
        outHitNodes[hitRecordIdx].postIndex = ioOrderCounter;
    }
}

} // namespace

void HitTestTree::BeginFrame() noexcept {
    m_currFrameBounds.clear();
    m_currUnclippedBounds.clear();
    m_currNodeSpaces.clear();
    m_currHitNodes.clear();
}

void HitTestTree::CollectFromTree(Node* root, Vec2 logicalSize, const UiTheme& rootTheme) {
    keel::u32 hitOrderCounter = 0;
    CollectBoundsRecursive(
        root,
        root,
        m_currFrameBounds,
        m_currUnclippedBounds,
        m_currHitNodes,
        hitOrderCounter,
        0,
        Id::Null(),
        Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y),
        rootTheme,
        Transform2D{},
        m_currNodeSpaces
    );
}

void HitTestTree::CommitFrame() {
    m_prevHitNodes = keel::move(m_currHitNodes);
    m_currHitNodes.clear();
    m_prevHitNodeMap.clear();
    m_prevHitNodeMap.reserve(m_prevHitNodes.size());
    for (keel::usize i = 0; i < m_prevHitNodes.size(); ++i) {
        m_prevHitNodeMap.put(m_prevHitNodes[i].id, i);
    }

    m_prevFrameBounds = keel::move(m_currFrameBounds);
    m_currFrameBounds.clear();
    m_prevUnclippedBounds = keel::move(m_currUnclippedBounds);
    m_currUnclippedBounds.clear();
    m_prevNodeSpaces = keel::move(m_currNodeSpaces);
    m_currNodeSpaces.clear();
}

Transform2D HitTestTree::GetNodeSpace(Id id) const noexcept {
    if (const Transform2D* t = m_prevNodeSpaces.get(id)) {
        return *t;
    }
    return Transform2D{};
}

Rect HitTestTree::GetPrevFrameBounds(Id id) const noexcept {
    if (const Rect* r = m_prevFrameBounds.get(id)) {
        return *r;
    }
    return Rect{};
}

Rect HitTestTree::GetPrevUnclippedBounds(Id id) const noexcept {
    if (const Rect* r = m_prevUnclippedBounds.get(id)) {
        return *r;
    }
    return Rect{};
}

const HitNodeRecord* HitTestTree::FindPrevHitNode(Id id) const noexcept {
    if (const keel::usize* idx = m_prevHitNodeMap.get(id)) {
        if (*idx < m_prevHitNodes.size()) {
            return &m_prevHitNodes[*idx];
        }
    }
    return nullptr;
}

void HitTestTree::ClearPopoverBlock(Id id) noexcept {
    if (!id.IsValid()) return;
    for (auto& rec : m_prevHitNodes) {
        if (rec.id == id && rec.type == NodeType::Popover) {
            rec.blocksHit = false;
        }
    }
}

bool HitTestTree::DoesNodeBlockHit(const Node* node) noexcept {
    if (!node) return false;
    switch (node->type) {
        case NodeType::Spacer:
        case NodeType::Text:
        case NodeType::Heading:
        case NodeType::Element:
        case NodeType::Separator:
            return false;
        case NodeType::Sidebar:
        case NodeType::BottomPanel:
            return !node->IsCollapsed();
        case NodeType::StatusBar:
        case NodeType::Toolbar:
        case NodeType::Card:
        case NodeType::Accordion:
        case NodeType::AccordionHeader:
        case NodeType::Modal:
        case NodeType::Table:
        case NodeType::TreeView:
        case NodeType::TreeItem:
        case NodeType::Carousel:
        case NodeType::Scene:
            return true;
        case NodeType::Row:
        case NodeType::Column:
        case NodeType::Wrap:
        case NodeType::Grid:
        case NodeType::Cell:
        case NodeType::Box:
        case NodeType::ScrollArea:
        case NodeType::TreeBranch:
            return (node->backgroundColor.a > 0) ||
                   (node->borderWidth > 0.0f && node->borderColor.a > 0);
        default:
            return true;
    }
}

} // namespace guikit
