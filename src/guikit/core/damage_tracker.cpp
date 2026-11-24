#include "guikit/core/damage_tracker.hpp"
#include "guikit/style/theme.hpp"

namespace guikit {

void DamageTracker::ComputeFrameDamage(
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
) {
    keel::HashMap<Id, WidgetSnapshot> currSnapshots;
    currSnapshots.reserve(m_prevSnapshots.size() + 16);
    keel::Array<Rect> damageRects = keel::move(m_explicitDamageRects);
    m_explicitDamageRects.clear();

    auto addDamage = [&](Rect r) {
        if (r.w > 0.0f && r.h > 0.0f) {
            damageRects.push_back(r);
        }
    };

    for (const Rect& r : extraDamageRects) {
        addDamage(r);
    }

    bool anyDebugBounds = debugBoundsEnabled;
    bool anyInspectMode = inspectModeEnabled;

    auto collectSnapshots = [&](auto& self, Node* n, Rect parentClip, const UiTheme& parentTheme, const Transform2D& parentSpace) -> void {
        if (!n) return;
        const UiTheme& activeTheme = n->GetTheme(parentTheme);
        const Transform2D space = NodeSpace(n, parentSpace);
        const Rect frameRect = space.Apply(n->computedRect);
        if (n->IsOutOfFlow()) {
            parentClip = Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y);
        }
        if (n->GetDebugBoundsOverride().value_or(false)) {
            anyDebugBounds = true;
        }
        if (n->GetInspectModeOverride().value_or(false)) {
            anyInspectMode = true;
        }
        if (n->id.IsValid()) {
            WidgetSnapshot snap;
            Rect paddedNodeRect = n->IsLeaf() ? frameRect.Inset(-2.0f) : frameRect;
            if (n->type == NodeType::Sidebar || n->type == NodeType::BottomPanel) {
                paddedNodeRect = frameRect.Inset(-16.0f);
            } else if (n->type == NodeType::Card && n->renderFn != nullptr) {
                paddedNodeRect = frameRect.Inset(-12.0f);
            } else if (n->type == NodeType::Modal) {
                paddedNodeRect = frameRect.Inset(-16.0f);
                const Rect bd = parentSpace.Apply(GetNodeContentClipRect(n, activeTheme));
                if (bd.w > 0.0f && bd.h > 0.0f) {
                    paddedNodeRect = paddedNodeRect.Union(bd);
                }
            } else if (n->type == NodeType::Popover) {
                paddedNodeRect = frameRect.Inset(-16.0f);
            }
            snap.rect = paddedNodeRect.Intersect(parentClip);
            const bool trackHoverFlag = n->IsLeaf() || n->type == NodeType::TreeItem || n->type == NodeType::Element;
            snap.flags = ((trackHoverFlag && n->hovered) ? 1u : 0u) |
                         (n->pressed ? 2u : 0u) |
                         (n->disabled ? 4u : 0u) |
                         (n->checked ? 8u : 0u) |
                         (n->active ? 64u : 0u) |
                         (n->focused ? 128u : 0u) |
                         ((n->scroll && n->scroll->hoveredY) ? 256u : 0u) |
                         ((n->scroll && n->scroll->activeY) ? 512u : 0u) |
                         ((n->scroll && n->scroll->hoveredX) ? 1024u : 0u) |
                         ((n->scroll && n->scroll->activeX) ? 2048u : 0u);
            snap.bgColor = n->backgroundColor.ToUint32();
            snap.borderColor = n->borderColor.ToUint32();

            const Vec2 scrollOff = n->GetScrollOffset();
            keel::u64 h = n->visualHash != 0 ? n->visualHash : detail::HashStringView(n->text);
            h = detail::HashArgs(
                h,
                n->computedRect.x,
                n->computedRect.y,
                n->computedRect.w,
                n->computedRect.h,
                scrollOff.x,
                scrollOff.y
            );
            if (editInfo.id.IsValid() && n->id == editInfo.id) {
                snap.flags |= (editInfo.cursorVisible ? 16u : 0u) | 32u;
                snap.cursor = editInfo.cursor;
                snap.selectionAnchor = editInfo.selectionAnchor;
                h = detail::HashStringView(editInfo.text, h);
            }
            snap.visualHash = h;

            const WidgetSnapshot* prevSnap = m_prevSnapshots.get(n->id);
            if (prevSnap != nullptr) {
                if (*prevSnap != snap) {
                    const bool curValid = (snap.rect.w > 0.0f && snap.rect.h > 0.0f);
                    const bool prevValid = (prevSnap->rect.w > 0.0f && prevSnap->rect.h > 0.0f);
                    if (curValid && prevValid) {
                        addDamage(snap.rect.Union(prevSnap->rect));
                    } else if (curValid) {
                        addDamage(snap.rect);
                    } else if (prevValid) {
                        addDamage(prevSnap->rect);
                    }
                }
            } else {
                addDamage(snap.rect);
            }

            currSnapshots.put(n->id, snap);
        }

        Rect effectiveClip = parentClip;
        if (n->clipChildren || n->type == NodeType::StatusBar || n->type == NodeType::Toolbar ||
            n->type == NodeType::Sidebar || n->type == NodeType::BottomPanel) {
            effectiveClip = parentClip.Intersect(space.Apply(GetNodeContentClipRect(n, activeTheme)));
        }

        for (Node* c = n->firstChild; c != nullptr; c = c->nextSibling) {
            self(self, c, effectiveClip, activeTheme, space);
        }
    };

    collectSnapshots(collectSnapshots, root, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), rootTheme, Transform2D{});
    outAnyDebugBounds = anyDebugBounds;
    outAnyInspectMode = anyInspectMode;

    // Detect removed widgets
    for (const auto& [id, prevSnap] : m_prevSnapshots) {
        if (!currSnapshots.contains(id)) {
            addDamage(prevSnap.rect);
        }
    }

    const bool debugBoundsChanged = (anyDebugBounds != hadDebugBoundsLastFrame);
    const bool inspectModeChanged = (anyInspectMode != hadInspectModeLastFrame);
    const bool anyOverlayStateChanged =
        overlayInfo.anyOverlayStateChanged || debugBoundsChanged || inspectModeChanged;
    const bool activeOverlayRequiresFullOnDamage =
        !damageRects.empty() &&
        (overlayInfo.hasActiveFullRepaintOverlay || anyDebugBounds || anyInspectMode);

    m_dirtyRects.clear();
    const keel::f32 totalViewportArea = logicalSize.x * logicalSize.y;
    if (!m_dirtyRectEnabled || m_forceFullRepaint || m_prevSnapshots.empty() ||
        anyOverlayStateChanged || activeOverlayRequiresFullOnDamage) {
        m_isFullRepaint = true;
        m_hasDirtyDamage = true;
        m_dirtyRect = Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y);
        m_dirtyRects.push_back(m_dirtyRect);
    } else if (damageRects.empty()) {
        m_isFullRepaint = false;
        m_hasDirtyDamage = false;
        m_dirtyRect = Rect{0.0f, 0.0f, 0.0f, 0.0f};
    } else {
        for (const Rect& raw : damageRects) {
            const keel::f32 x1 = keel::max(0.0f, keel::floor(raw.x));
            const keel::f32 y1 = keel::max(0.0f, keel::floor(raw.y));
            const keel::f32 x2 = keel::min(logicalSize.x, keel::ceil(raw.Right()));
            const keel::f32 y2 = keel::min(logicalSize.y, keel::ceil(raw.Bottom()));
            if (x2 > x1 && y2 > y1) {
                m_dirtyRects.emplace_back(x1, y1, x2 - x1, y2 - y1);
            }
        }

        bool mergedAny = true;
        while (mergedAny && m_dirtyRects.size() > 1) {
            mergedAny = false;
            for (keel::usize i = 0; i < m_dirtyRects.size() && !mergedAny; ++i) {
                for (keel::usize j = i + 1; j < m_dirtyRects.size(); ++j) {
                    if (m_dirtyRects[i].Intersects(m_dirtyRects[j])) {
                        m_dirtyRects[i] = m_dirtyRects[i].Union(m_dirtyRects[j]);
                        m_dirtyRects.remove_at(j);
                        mergedAny = true;
                        break;
                    }
                }
            }
        }

        if (m_dirtyRects.empty()) {
            m_isFullRepaint = false;
            m_hasDirtyDamage = false;
            m_dirtyRect = Rect{0.0f, 0.0f, 0.0f, 0.0f};
        } else {
            Rect unionRect = m_dirtyRects[0];
            keel::f32 totalDamageArea = m_dirtyRects[0].Area();
            for (keel::usize i = 1; i < m_dirtyRects.size(); ++i) {
                unionRect = unionRect.Union(m_dirtyRects[i]);
                totalDamageArea += m_dirtyRects[i].Area();
            }

            if (totalDamageArea >= 0.96f * totalViewportArea || m_dirtyRects.size() > 8) {
                m_isFullRepaint = true;
                m_dirtyRect = Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y);
                m_dirtyRects.assign(1, m_dirtyRect);
            } else {
                m_isFullRepaint = false;
                m_dirtyRect = unionRect;
            }
            m_hasDirtyDamage = true;
        }
    }

    m_forceFullRepaint = false;
    m_prevSnapshots = keel::move(currSnapshots);
}

} // namespace guikit
