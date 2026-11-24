#include "guikit/core/debug_inspector.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/theme.hpp"
#include "keel/format.h"
#include "keel/math.h"

namespace guikit {

void DebugInspector::RenderDebugBounds(Node* root, ICanvas& canvas, const UiTheme& rootTheme) const {
    RenderDebugBoundsRecursive(root, root, canvas, rootTheme, m_debugBoundsEnabled);
}

void DebugInspector::RenderDebugBoundsRecursive(
    Node* node,
    const Node* rootNode,
    ICanvas& canvas,
    const UiTheme& rootTheme,
    bool inheritedDebugBounds
) const {
    if (!node) return;
    if (node->computedRect.w <= 0.0f || node->computedRect.h <= 0.0f) return;

    const bool activeDebug = node->GetDebugBoundsOverride().value_or(inheritedDebugBounds);
    const Transform2D local = NodeLocalTransform(node);
    const bool transformed = !local.IsIdentity();
    if (transformed) {
        canvas.PushTransform(local);
    }

    if (node != rootNode && activeDebug) {
        const NodeCategory cat = node->GetCategory();

        Color stroke;
        Color padColor;
        switch (cat) {
            case NodeCategory::Container:
                stroke = Color(192, 132, 252, 220); // Purple-400 (#C084FC)
                padColor = Color(192, 132, 252, 35);
                break;
            case NodeCategory::Layout:
                stroke = Color(56, 189, 248, 220);  // Sky-400 (#38BDF8)
                padColor = Color(56, 189, 248, 30);
                break;
            case NodeCategory::Widget:
                stroke = Color(74, 222, 128, 220);  // Green-400 (#4ADE80)
                padColor = Color(74, 222, 128, 35);
                break;
            case NodeCategory::Spacer:
                stroke = Color(245, 158, 11, 220);  // Amber-500 (#F59E0B)
                padColor = Color(245, 158, 11, 35);
                break;
        }

        const Rect strokeRect(
            node->computedRect.x + 0.5f,
            node->computedRect.y + 0.5f,
            keel::max(0.0f, node->computedRect.w - 1.0f),
            keel::max(0.0f, node->computedRect.h - 1.0f)
        );
        canvas.DrawRoundRect(strokeRect, 0.0f, Color(0, 0, 0, 0), stroke, 1.0f);

        if (cat == NodeCategory::Spacer) {
            canvas.DrawRect(node->computedRect, padColor);
        }

        const auto& p = node->padding;
        if (p.top > 0.0f || p.bottom > 0.0f || p.left > 0.0f || p.right > 0.0f) {
            const keel::f32 x = node->computedRect.x;
            const keel::f32 y = node->computedRect.y;
            const keel::f32 w = node->computedRect.w;
            const keel::f32 h = node->computedRect.h;

            if (p.top > 0.0f) {
                canvas.DrawRect(Rect(x, y, w, p.top), padColor);
            }
            if (p.bottom > 0.0f) {
                canvas.DrawRect(Rect(x, y + h - p.bottom, w, p.bottom), padColor);
            }
            const keel::f32 midH = keel::max(0.0f, h - p.top - p.bottom);
            if (midH > 0.0f) {
                if (p.left > 0.0f) {
                    canvas.DrawRect(Rect(x, y + p.top, p.left, midH), padColor);
                }
                if (p.right > 0.0f) {
                    canvas.DrawRect(Rect(x + w - p.right, y + p.top, p.right, midH), padColor);
                }
            }
        }
    }

    if (node->clipChildren) {
        canvas.PushClip(GetNodeContentClipRect(node, node->GetTheme(rootTheme)));
    }

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        RenderDebugBoundsRecursive(child, rootNode, canvas, rootTheme, activeDebug);
    }

    if (node->clipChildren) {
        canvas.PopClip();
    }

    if (transformed) {
        canvas.PopTransform();
    }
}

Node* DebugInspector::FindDeepestNodeAt(
    Node* root,
    Vec2 pos,
    Rect clipRect,
    const UiTheme& rootTheme
) noexcept {
    if (!root) return nullptr;
    const Transform2D local = NodeLocalTransform(root);
    if (!local.IsIdentity()) {
        pos = local.Invert(pos);
        clipRect = local.Invert(clipRect);
    }
    if (root->computedRect.w <= 0.0f || root->computedRect.h <= 0.0f) return nullptr;

    const Rect visibleRect = root->computedRect.Intersect(clipRect);
    if (!visibleRect.Contains(pos)) {
        return nullptr;
    }

    Rect effectiveClip = clipRect;
    if (root->clipChildren) {
        effectiveClip = clipRect.Intersect(GetNodeContentClipRect(root, root->GetTheme(rootTheme)));
    }

    Node* bestHit = nullptr;
    for (Node* c = root->firstChild; c != nullptr; c = c->nextSibling) {
        Node* hit = FindDeepestNodeAt(c, pos, effectiveClip, rootTheme);
        if (hit) {
            bestHit = hit;
        }
    }

    return bestHit ? bestHit : root;
}

void DebugInspector::RenderInspectOverlay(
    Node* root,
    ICanvas& canvas,
    Vec2 logicalSize,
    Vec2 mousePos,
    const UiTheme& rootTheme,
    const HitTestTree& hitTree
) const {
    if (!root) return;

    Node* target = FindDeepestNodeAt(root, mousePos, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), rootTheme);
    if (!target || target == root) return;

    if (!target->GetInspectModeOverride().value_or(m_inspectModeEnabled)) return;

    Rect ancestorClip(0.0f, 0.0f, logicalSize.x, logicalSize.y);
    for (const Node* p = target->parent; p != nullptr; p = p->parent) {
        if (p->clipChildren) {
            ancestorClip = ancestorClip.Intersect(
                hitTree.GetNodeSpace(p->id).Apply(GetNodeContentClipRect(p, p->GetTheme(rootTheme)))
            );
        }
    }
    canvas.PushClip(ancestorClip);
    const Transform2D targetSpace = hitTree.GetNodeSpace(target->id);
    canvas.PushTransform(targetSpace);

    const Rect contentRect(
        target->computedRect.x + target->padding.left,
        target->computedRect.y + target->padding.top,
        keel::max(0.0f, target->computedRect.w - target->padding.Horizontal()),
        keel::max(0.0f, target->computedRect.h - target->padding.Vertical())
    );
    if (contentRect.w > 0.0f && contentRect.h > 0.0f) {
        canvas.DrawRect(contentRect, Color(111, 168, 220, 100));
    }

    const auto& p = target->padding;
    if (p.top > 0.0f || p.bottom > 0.0f || p.left > 0.0f || p.right > 0.0f) {
        const Color devToolsPadColor(147, 196, 125, 140);
        const keel::f32 x = target->computedRect.x;
        const keel::f32 y = target->computedRect.y;
        const keel::f32 w = target->computedRect.w;
        const keel::f32 h = target->computedRect.h;

        if (p.top > 0.0f) {
            canvas.DrawRect(Rect(x, y, w, p.top), devToolsPadColor);
        }
        if (p.bottom > 0.0f) {
            canvas.DrawRect(Rect(x, y + h - p.bottom, w, p.bottom), devToolsPadColor);
        }
        const keel::f32 midH = keel::max(0.0f, h - p.top - p.bottom);
        if (midH > 0.0f) {
            if (p.left > 0.0f) {
                canvas.DrawRect(Rect(x, y + p.top, p.left, midH), devToolsPadColor);
            }
            if (p.right > 0.0f) {
                canvas.DrawRect(Rect(x + w - p.right, y + p.top, p.right, midH), devToolsPadColor);
            }
        }
    }

    canvas.DrawRoundRect(target->computedRect, 0.0f, Color(0, 0, 0, 0), Color(56, 189, 248, 255), 2.0f);
    canvas.PopTransform();
    canvas.PopClip();

    char typeBuf[64];
    keel::StringRef typeStr;
    if (target->type == NodeType::Cell) {
        typeStr = keel::format_buf(typeBuf, "Cell (%d)", target->colSpan);
    } else {
        typeStr = target->GetTypeName();
    }

    keel::StringRef labelStr;
    if (!target->text.empty()) {
        labelStr = target->text.substr(0, 24);
    }

    char line1Buf[128];
    const keel::StringRef line1 = labelStr.empty()
        ? typeStr
        : keel::format_buf(line1Buf, "%s \"%s\"", typeStr, labelStr);
    char line2Buf[64];
    const keel::StringRef line2 = keel::format_buf(line2Buf, "%.1f × %.1f px", target->computedRect.w, target->computedRect.h);
    char line3Buf[96];
    keel::StringRef line3;
    if (p.top > 0.0f || p.bottom > 0.0f || p.left > 0.0f || p.right > 0.0f) {
        line3 = keel::format_buf(line3Buf, "pad: %.0f, %.0f, %.0f, %.0f", p.top, p.right, p.bottom, p.left);
    } else if (target->gap > 0.0f) {
        line3 = keel::format_buf(line3Buf, "gap: %.0f px", target->gap);
    } else if (target->isSpacer) {
        line3 = keel::format_buf(line3Buf, "flex: %.1f", target->flexGrow);
    } else {
        const char* alignStr = "Start";
        if (target->align == Alignment::Center) alignStr = "Center";
        else if (target->align == Alignment::End) alignStr = "End";
        else if (target->align == Alignment::Stretch) alignStr = "Stretch";
        line3 = keel::format_buf(line3Buf, "align: %s", alignStr);
    }

    const keel::f32 fontSize = 11.0f;
    const keel::f32 w1 = canvas.MeasureText(line1, fontSize).x;
    const keel::f32 w2 = canvas.MeasureText(line2, fontSize).x;
    const keel::f32 w3 = canvas.MeasureText(line3, fontSize).x;
    const keel::f32 maxTextW = keel::max(w1, keel::max(w2, w3));

    const keel::f32 badgeW = keel::max(140.0f, maxTextW + 24.0f);
    const keel::f32 badgeH = 58.0f;

    keel::f32 bx = mousePos.x + 16.0f;
    keel::f32 by = mousePos.y + 16.0f;
    if (bx + badgeW > logicalSize.x - 8.0f) {
        bx = mousePos.x - badgeW - 12.0f;
    }
    if (by + badgeH > logicalSize.y - 8.0f) {
        by = mousePos.y - badgeH - 12.0f;
    }
    bx = keel::clamp(bx, 8.0f, keel::max(8.0f, logicalSize.x - badgeW - 8.0f));
    by = keel::clamp(by, 8.0f, keel::max(8.0f, logicalSize.y - badgeH - 8.0f));

    const Rect badgeRect(bx, by, badgeW, badgeH);
    canvas.DrawRoundRect(badgeRect, 6.0f, Color(15, 23, 42, 245), Color(71, 85, 105, 220), 1.0f);
    canvas.DrawTextBox(line1, Rect(bx + 10.0f, by + 5.0f, badgeW - 20.0f, 16.0f), fontSize, Color(56, 189, 248, 255), 0.0f, 0.5f);
    canvas.DrawTextBox(line2, Rect(bx + 10.0f, by + 21.0f, badgeW - 20.0f, 16.0f), fontSize, Color(241, 245, 249, 255), 0.0f, 0.5f);
    canvas.DrawTextBox(line3, Rect(bx + 10.0f, by + 37.0f, badgeW - 20.0f, 16.0f), fontSize, Color(148, 163, 184, 255), 0.0f, 0.5f);
}

} // namespace guikit
