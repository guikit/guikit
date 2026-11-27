#include "guikit/surfaces/dock_area.hpp"
#include "keel/format.h"
#include "keel/math.h"

namespace guikit {

// ==============================================================================
// DockLayout — Pure Declarative Binary Split Tree, Floating Cards & Codec
// ==============================================================================

namespace {

constexpr keel::f32 kMinSplitRatio = 0.05f;
constexpr keel::f32 kMaxSplitRatio = 0.95f;
constexpr keel::f32 kFloatMinWidthDp = 160.0f;
constexpr keel::f32 kFloatMinHeightDp = 110.0f;

[[nodiscard]] constexpr keel::f32 ClampRatio(keel::f32 r) noexcept {
    if (!(r >= 0.0f && r <= 1.0f)) {
        return 0.5f;
    }
    return keel::clamp(r, kMinSplitRatio, kMaxSplitRatio);
}

[[nodiscard]] bool IsAsciiWhitespace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void AppendCompactFloat(keel::String& out, keel::f32 v) {
    // Round to 3 decimal places for clean human-readable output, trimming trailing zeros
    const keel::f32 rounded = keel::round(v * 1000.0f) * 0.001f;
    keel::String tmp = keel::format("{:.3f}", rounded);
    while (tmp.size() > 1u && tmp.back() == '0') {
        tmp.pop_back();
    }
    if (!tmp.empty() && tmp.back() == '.') {
        tmp.pop_back();
    }
    if (tmp == "-0") {
        out.append("0");
    } else {
        out.append(tmp.as_ref());
    }
}

void SerializeNodeRecursive(const DockLayout& layout, keel::u32 idx, keel::String& out) {
    const auto nodes = layout.Nodes();
    if (idx >= nodes.size()) return;
    const DockNode& n = nodes[idx];
    if (n.kind == DockNodeKind::Tabs) {
        out.append("T(");
        out.append(keel::format("{}", n.activeTab).as_ref());
        out.push_back(';');
        for (keel::usize i = 0; i < n.panelIds.size(); ++i) {
            if (i > 0) out.push_back(',');
            out.append(n.panelIds[i].as_ref());
        }
        out.push_back(')');
    } else {
        out.push_back(n.orientation == Orientation::Horizontal ? 'H' : 'V');
        out.push_back('(');
        AppendCompactFloat(out, ClampRatio(n.ratio));
        out.push_back(',');
        SerializeNodeRecursive(layout, n.first, out);
        out.push_back(',');
        SerializeNodeRecursive(layout, n.second, out);
        out.push_back(')');
    }
}

class LayoutParser {
public:
    explicit LayoutParser(keel::StringRef src) noexcept : m_src(src) {}

    bool Parse(
        keel::Array<DockNode>& outNodes,
        keel::u32& outRoot,
        keel::Array<DockFloatCard>& outFloating
    ) {
        SkipWs();
        if (AtEnd()) return false;

        if (Peek() == 'D' && m_pos + 2u < m_src.size() && m_src[m_pos + 1u] == '2' && m_src[m_pos + 2u] == '(') {
            m_pos += 3u;
            SkipWs();
            if (!AtEnd() && Peek() != ';' && Peek() != ')') {
                if (!ParseTreeNode(outNodes, outRoot)) return false;
            }
            SkipWs();
            while (!AtEnd() && Peek() == ';') {
                ++m_pos;
                SkipWs();
                if (!ParseFloatCard(outFloating)) return false;
                SkipWs();
            }
            if (!Consume(')')) return false;
        } else {
            if (!ParseTreeNode(outNodes, outRoot)) return false;
        }

        SkipWs();
        if (!AtEnd()) return false;
        return (outRoot != kDockInvalidNode) || !outFloating.empty();
    }

private:
    [[nodiscard]] bool AtEnd() const noexcept { return m_pos >= m_src.size(); }
    [[nodiscard]] char Peek() const noexcept { return AtEnd() ? '\0' : m_src[m_pos]; }

    void SkipWs() noexcept {
        while (!AtEnd() && IsAsciiWhitespace(m_src[m_pos])) {
            ++m_pos;
        }
    }

    bool Consume(char ch) noexcept {
        SkipWs();
        if (AtEnd() || m_src[m_pos] != ch) return false;
        ++m_pos;
        return true;
    }

    bool SeenPanel(keel::StringRef id) const noexcept {
        for (const auto& s : m_seenIds) {
            if (s.as_ref() == id) return true;
        }
        return false;
    }

    bool RegisterPanel(keel::StringRef id) {
        if (!DockLayout::IsValidPanelId(id) || SeenPanel(id)) {
            return false;
        }
        m_seenIds.push_back(DockPanelId(id));
        return true;
    }

    bool ParseUsize(keel::usize& outVal) noexcept {
        SkipWs();
        if (AtEnd() || m_src[m_pos] < '0' || m_src[m_pos] > '9') return false;
        keel::usize val = 0u;
        keel::usize digits = 0u;
        while (!AtEnd() && m_src[m_pos] >= '0' && m_src[m_pos] <= '9') {
            if (digits > 10u) return false;
            val = val * 10u + keel::as<keel::usize>(m_src[m_pos] - '0');
            ++m_pos;
            ++digits;
        }
        outVal = val;
        return digits > 0u;
    }

    bool ParseFloat(keel::f32& outVal) noexcept {
        SkipWs();
        if (AtEnd()) return false;
        const keel::usize start = m_pos;
        bool neg = false;
        if (m_src[m_pos] == '-') {
            neg = true;
            ++m_pos;
        } else if (m_src[m_pos] == '+') {
            ++m_pos;
        }
        keel::f64 intPart = 0.0;
        keel::usize intDigits = 0u;
        while (!AtEnd() && m_src[m_pos] >= '0' && m_src[m_pos] <= '9') {
            intPart = intPart * 10.0 + keel::as<keel::f64>(m_src[m_pos] - '0');
            ++m_pos;
            ++intDigits;
        }
        keel::f64 fracPart = 0.0;
        keel::f64 fracScale = 0.1;
        keel::usize fracDigits = 0u;
        if (!AtEnd() && m_src[m_pos] == '.') {
            ++m_pos;
            while (!AtEnd() && m_src[m_pos] >= '0' && m_src[m_pos] <= '9') {
                fracPart += keel::as<keel::f64>(m_src[m_pos] - '0') * fracScale;
                fracScale *= 0.1;
                ++m_pos;
                ++fracDigits;
            }
        }
        if (intDigits == 0u && fracDigits == 0u) {
            m_pos = start;
            return false;
        }
        const keel::f64 combined = neg ? -(intPart + fracPart) : (intPart + fracPart);
        if (!keel::is_finite(combined)) return false;
        outVal = keel::as<keel::f32>(combined);
        return true;
    }

    bool ParsePanelId(DockPanelId& outId) noexcept {
        SkipWs();
        const keel::usize start = m_pos;
        while (!AtEnd()) {
            const char c = m_src[m_pos];
            if (c == ',' || c == ';' || c == ')' || c == '(') break;
            ++m_pos;
        }
        const keel::StringRef raw = m_src.substr(start, m_pos - start).trim();
        if (!DockLayout::IsValidPanelId(raw)) return false;
        outId = DockPanelId(raw);
        return true;
    }

    bool ParseTreeNode(keel::Array<DockNode>& outNodes, keel::u32& outIdx, keel::u32 depth = 0u) {
        if (depth > 64u) return false;
        SkipWs();
        if (AtEnd()) return false;
        const char tag = m_src[m_pos];
        if (tag == 'T') {
            ++m_pos;
            if (!Consume('(')) return false;
            keel::usize activeTab = 0u;
            if (!ParseUsize(activeTab)) return false;
            if (!Consume(';')) return false;

            DockNode node{};
            node.kind = DockNodeKind::Tabs;
            while (true) {
                DockPanelId pid{};
                if (!ParsePanelId(pid)) return false;
                if (!RegisterPanel(pid.as_ref())) return false;
                node.panelIds.push_back(pid);
                SkipWs();
                if (!AtEnd() && Peek() == ',') {
                    ++m_pos;
                    continue;
                }
                break;
            }
            if (!Consume(')')) return false;
            if (node.panelIds.empty() || activeTab >= node.panelIds.size()) return false;
            node.activeTab = activeTab;
            outIdx = keel::as<keel::u32>(outNodes.size());
            outNodes.push_back(keel::move(node));
            return true;
        }
        if (tag == 'H' || tag == 'V') {
            ++m_pos;
            if (!Consume('(')) return false;
            keel::f32 ratio = 0.5f;
            if (!ParseFloat(ratio)) return false;
            if (!(ratio > 0.0f && ratio < 1.0f)) return false;
            if (!Consume(',')) return false;

            const keel::u32 splitIdx = keel::as<keel::u32>(outNodes.size());
            DockNode splitNode{};
            splitNode.kind = DockNodeKind::Split;
            splitNode.orientation = (tag == 'H') ? Orientation::Horizontal : Orientation::Vertical;
            splitNode.ratio = ClampRatio(ratio);
            outNodes.push_back(keel::move(splitNode));

            keel::u32 firstIdx = kDockInvalidNode;
            keel::u32 secondIdx = kDockInvalidNode;
            if (!ParseTreeNode(outNodes, firstIdx, depth + 1u)) return false;
            if (!Consume(',')) return false;
            if (!ParseTreeNode(outNodes, secondIdx, depth + 1u)) return false;
            if (!Consume(')')) return false;

            outNodes[splitIdx].first = firstIdx;
            outNodes[splitIdx].second = secondIdx;
            outIdx = splitIdx;
            return true;
        }
        return false;
    }

    bool ParseFloatCard(keel::Array<DockFloatCard>& outFloating) {
        if (!Consume('F')) return false;
        if (!Consume('(')) return false;
        DockPanelId pid{};
        if (!ParsePanelId(pid)) return false;
        if (!RegisterPanel(pid.as_ref())) return false;
        if (!Consume(',')) return false;
        keel::f32 x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
        if (!ParseFloat(x)) return false;
        if (!Consume(',')) return false;
        if (!ParseFloat(y)) return false;
        if (!Consume(',')) return false;
        if (!ParseFloat(w)) return false;
        if (!Consume(',')) return false;
        if (!ParseFloat(h)) return false;
        if (w <= 0.0f || h <= 0.0f) return false;
        bool minimized = false;
        SkipWs();
        if (!AtEnd() && Peek() == ',') {
            ++m_pos;
            keel::usize minFlag = 0u;
            if (!ParseUsize(minFlag) || minFlag > 1u) return false;
            minimized = (minFlag != 0u);
        }
        if (!Consume(')')) return false;

        DockFloatCard card{};
        card.panelId = pid;
        card.bounds = Rect(x, y, w, h);
        card.zOrder = keel::as<keel::u32>(outFloating.size() + 1u);
        card.minimized = minimized;
        outFloating.push_back(card);
        return true;
    }

    keel::StringRef m_src{};
    keel::usize m_pos{0u};
    keel::Array<DockPanelId> m_seenIds;
};

[[nodiscard]] DockPanelId FindFirstPanelInSubtree(
    keel::ArrayRef<DockNode> nodes,
    keel::u32 idx,
    keel::u32 depth = 0u
) noexcept {
    if (idx >= nodes.size() || depth > 64u) return {};
    const DockNode& n = nodes[idx];
    if (n.kind == DockNodeKind::Tabs) {
        return n.panelIds.empty() ? DockPanelId{} : n.panelIds[0];
    }
    DockPanelId p = FindFirstPanelInSubtree(nodes, n.first, depth + 1u);
    if (!p.empty()) return p;
    return FindFirstPanelInSubtree(nodes, n.second, depth + 1u);
}

} // namespace

bool DockLayout::IsValidPanelId(keel::StringRef id) noexcept {
    if (id.empty() || id.size() >= 64u) return false;
    if (IsAsciiWhitespace(id.front()) || IsAsciiWhitespace(id.back())) return false;
    for (keel::usize i = 0; i < id.size(); ++i) {
        const keel::u8 c = keel::as<keel::u8>(id[i]);
        if (c < 0x20u || c == '(' || c == ')' || c == ',' || c == ';') {
            return false;
        }
    }
    return true;
}

DockLayout DockLayout::Clone() const {
    DockLayout copy;
    copy.m_rootIndex = m_rootIndex;
    copy.m_revision = m_revision;
    copy.m_nodes.reserve(m_nodes.size());
    for (const auto& n : m_nodes) {
        DockNode cn{};
        cn.kind = n.kind;
        cn.orientation = n.orientation;
        cn.ratio = n.ratio;
        cn.first = n.first;
        cn.second = n.second;
        cn.activeTab = n.activeTab;
        for (const auto& pid : n.panelIds) {
            cn.panelIds.push_back(pid);
        }
        copy.m_nodes.push_back(keel::move(cn));
    }
    copy.m_floating.reserve(m_floating.size());
    for (const auto& f : m_floating) {
        copy.m_floating.push_back(f);
    }
    copy.m_closedPanels.reserve(m_closedPanels.size());
    for (const auto& c : m_closedPanels) {
        copy.m_closedPanels.push_back(c);
    }
    copy.m_closedHistory.reserve(m_closedHistory.size());
    for (const auto& h : m_closedHistory) {
        copy.m_closedHistory.push_back(h);
    }
    copy.m_expandedPanelId = m_expandedPanelId;
    copy.m_expandedFullscreen = m_expandedFullscreen;
    return copy;
}

void DockLayout::Clear() noexcept {
    m_nodes.clear();
    m_rootIndex = kDockInvalidNode;
    m_floating.clear();
    m_closedPanels.clear();
    m_closedHistory.clear();
    m_expandedPanelId.clear();
    m_expandedFullscreen = false;
    ++m_revision;
}

bool DockLayout::IsEmpty() const noexcept {
    return m_rootIndex == kDockInvalidNode && m_floating.empty();
}

keel::u32 DockLayout::FindPanelNode(keel::StringRef id) const noexcept {
    if (id.empty()) return kDockInvalidNode;
    for (keel::usize i = 0; i < m_nodes.size(); ++i) {
        const DockNode& n = m_nodes[i];
        if (n.kind != DockNodeKind::Tabs) continue;
        for (const auto& pid : n.panelIds) {
            if (pid.as_ref() == id) {
                return keel::as<keel::u32>(i);
            }
        }
    }
    return kDockInvalidNode;
}

keel::u32 DockLayout::FindFirstTabNode() const noexcept {
    if (m_rootIndex >= m_nodes.size()) return kDockInvalidNode;
    keel::u32 cur = m_rootIndex;
    for (keel::u32 step = 0; step < 64u && cur < m_nodes.size(); ++step) {
        if (m_nodes[cur].kind == DockNodeKind::Tabs) {
            return cur;
        }
        cur = m_nodes[cur].first;
    }
    for (keel::usize i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i].kind == DockNodeKind::Tabs) {
            return keel::as<keel::u32>(i);
        }
    }
    return kDockInvalidNode;
}

DockFloatCard* DockLayout::FindFloatCard(keel::StringRef id) noexcept {
    for (auto& f : m_floating) {
        if (f.panelId.as_ref() == id) return &f;
    }
    return nullptr;
}

const DockFloatCard* DockLayout::FindFloatCard(keel::StringRef id) const noexcept {
    for (const auto& f : m_floating) {
        if (f.panelId.as_ref() == id) return &f;
    }
    return nullptr;
}

bool DockLayout::HasDockedPanel(keel::StringRef id) const noexcept {
    return FindPanelNode(id) != kDockInvalidNode;
}

bool DockLayout::IsPanelFloating(keel::StringRef id) const noexcept {
    return FindFloatCard(id) != nullptr;
}

bool DockLayout::HasPanel(keel::StringRef id) const noexcept {
    return HasDockedPanel(id) || IsPanelFloating(id);
}

bool DockLayout::IsPanelClosed(keel::StringRef id) const noexcept {
    for (const auto& c : m_closedPanels) {
        if (c.as_ref() == id) return true;
    }
    return false;
}

bool DockLayout::IsPanelActive(keel::StringRef id) const noexcept {
    if (!id.empty() && m_expandedPanelId.as_ref() == id) {
        return true;
    }
    const keel::u32 nodeIdx = FindPanelNode(id);
    if (nodeIdx != kDockInvalidNode && nodeIdx < m_nodes.size()) {
        const DockNode& n = m_nodes[nodeIdx];
        if (n.activeTab < n.panelIds.size() && n.panelIds[n.activeTab].as_ref() == id) {
            return true;
        }
        return false;
    }
    if (const DockFloatCard* f = FindFloatCard(id)) {
        return !f->minimized;
    }
    return false;
}

keel::u32 DockLayout::NextZOrder() const noexcept {
    keel::u32 maxZ = 0u;
    for (const auto& f : m_floating) {
        if (f.zOrder > maxZ) maxZ = f.zOrder;
    }
    return maxZ + 1u;
}

keel::u32 DockLayout::FindParentSplit(keel::u32 childIdx) const noexcept {
    for (keel::usize i = 0; i < m_nodes.size(); ++i) {
        const DockNode& n = m_nodes[i];
        if (n.kind == DockNodeKind::Split && (n.first == childIdx || n.second == childIdx)) {
            return keel::as<keel::u32>(i);
        }
    }
    return kDockInvalidNode;
}

void DockLayout::CompactNodes() {
    if (m_rootIndex >= m_nodes.size()) {
        m_nodes.clear();
        m_rootIndex = kDockInvalidNode;
        return;
    }

    keel::Array<DockNode> compacted;
    compacted.reserve(m_nodes.size());

    auto copyRec = [&](auto&& self, keel::u32 oldIdx, keel::u32 depth) -> keel::u32 {
        if (oldIdx >= m_nodes.size() || depth > 64u) return kDockInvalidNode;
        const keel::u32 newIdx = keel::as<keel::u32>(compacted.size());
        DockNode copy{};
        copy.kind = m_nodes[oldIdx].kind;
        copy.orientation = m_nodes[oldIdx].orientation;
        copy.ratio = ClampRatio(m_nodes[oldIdx].ratio);
        copy.activeTab = m_nodes[oldIdx].activeTab;
        for (const auto& pid : m_nodes[oldIdx].panelIds) {
            copy.panelIds.push_back(pid);
        }
        compacted.push_back(keel::move(copy));
        if (m_nodes[oldIdx].kind == DockNodeKind::Split) {
            const keel::u32 f = self(self, m_nodes[oldIdx].first, depth + 1u);
            const keel::u32 s = self(self, m_nodes[oldIdx].second, depth + 1u);
            compacted[newIdx].first = f;
            compacted[newIdx].second = s;
        }
        return newIdx;
    };

    const keel::u32 newRoot = copyRec(copyRec, m_rootIndex, 0u);
    m_nodes = keel::move(compacted);
    m_rootIndex = newRoot;
}

void DockLayout::EraseClosedRecord(keel::StringRef id) {
    for (keel::usize i = 0; i < m_closedPanels.size(); ++i) {
        if (m_closedPanels[i].as_ref() == id) {
            m_closedPanels.remove_at(i);
            break;
        }
    }
    for (keel::usize i = 0; i < m_closedHistory.size(); ++i) {
        if (m_closedHistory[i].panelId.as_ref() == id) {
            m_closedHistory.remove_at(i);
            break;
        }
    }
}

void DockLayout::CaptureRestoreHint(
    keel::StringRef id,
    DockPanelId& outTarget,
    DockSide& outSide,
    keel::f32& outRatio
) const noexcept {
    outTarget.clear();
    outSide = DockSide::Center;
    outRatio = 0.5f;

    const keel::u32 tabIdx = FindPanelNode(id);
    if (tabIdx == kDockInvalidNode || tabIdx >= m_nodes.size()) return;
    const DockNode& tab = m_nodes[tabIdx];
    for (const auto& pid : tab.panelIds) {
        if (pid.as_ref() != id) {
            outTarget = pid;
            outSide = DockSide::Center;
            outRatio = 0.5f;
            return;
        }
    }
    const keel::u32 parentIdx = FindParentSplit(tabIdx);
    if (parentIdx != kDockInvalidNode && parentIdx < m_nodes.size()) {
        const DockNode& parent = m_nodes[parentIdx];
        const bool isFirst = (parent.first == tabIdx);
        const keel::u32 sibIdx = isFirst ? parent.second : parent.first;
        outTarget = FindFirstPanelInSubtree(m_nodes, sibIdx);
        if (parent.orientation == Orientation::Horizontal) {
            outSide = isFirst ? DockSide::Left : DockSide::Right;
        } else {
            outSide = isFirst ? DockSide::Top : DockSide::Bottom;
        }
        outRatio = isFirst ? ClampRatio(parent.ratio) : ClampRatio(1.0f - parent.ratio);
    }
}

bool DockLayout::RemoveFromDockedTree(keel::StringRef id) {
    const keel::u32 tabIdx = FindPanelNode(id);
    if (tabIdx == kDockInvalidNode || tabIdx >= m_nodes.size()) return false;

    DockNode& tab = m_nodes[tabIdx];
    keel::usize foundIdx = keel::kInvalidIndex;
    for (keel::usize i = 0; i < tab.panelIds.size(); ++i) {
        if (tab.panelIds[i].as_ref() == id) {
            foundIdx = i;
            break;
        }
    }
    if (foundIdx == keel::kInvalidIndex) return false;

    tab.panelIds.remove_at(foundIdx);
    if (!tab.panelIds.empty()) {
        if (tab.activeTab >= tab.panelIds.size()) {
            tab.activeTab = tab.panelIds.size() - 1u;
        } else if (foundIdx < tab.activeTab) {
            --tab.activeTab;
        }
        ++m_revision;
        return true;
    }

    // Tab group is now empty: collapse its parent split node and promote sibling
    if (tabIdx == m_rootIndex) {
        m_nodes.clear();
        m_rootIndex = kDockInvalidNode;
    } else {
        const keel::u32 parentIdx = FindParentSplit(tabIdx);
        if (parentIdx != kDockInvalidNode && parentIdx < m_nodes.size()) {
            const keel::u32 sibIdx = (m_nodes[parentIdx].first == tabIdx)
                ? m_nodes[parentIdx].second
                : m_nodes[parentIdx].first;
            if (parentIdx == m_rootIndex) {
                m_rootIndex = sibIdx;
            } else {
                const keel::u32 grandIdx = FindParentSplit(parentIdx);
                if (grandIdx != kDockInvalidNode && grandIdx < m_nodes.size()) {
                    if (m_nodes[grandIdx].first == parentIdx) {
                        m_nodes[grandIdx].first = sibIdx;
                    } else if (m_nodes[grandIdx].second == parentIdx) {
                        m_nodes[grandIdx].second = sibIdx;
                    }
                }
            }
            CompactNodes();
        }
    }
    ++m_revision;
    return true;
}

bool DockLayout::RemoveFromFloating(keel::StringRef id) {
    for (keel::usize i = 0; i < m_floating.size(); ++i) {
        if (m_floating[i].panelId.as_ref() == id) {
            m_floating.remove_at(i);
            ++m_revision;
            return true;
        }
    }
    return false;
}

bool DockLayout::RemovePanel(keel::StringRef id) {
    if (id.empty()) return false;
    if (m_expandedPanelId.as_ref() == id) {
        m_expandedPanelId.clear();
        m_expandedFullscreen = false;
    }
    const bool r1 = RemoveFromDockedTree(id);
    const bool r2 = RemoveFromFloating(id);
    return r1 || r2;
}

bool DockLayout::ClosePanel(keel::StringRef id) {
    if (!IsValidPanelId(id)) return false;

    ClosedPanelRecord rec{};
    rec.panelId = DockPanelId(id);
    if (const DockFloatCard* f = FindFloatCard(id)) {
        rec.lastDockTarget = f->lastDockTarget;
        rec.lastDockSide = f->lastDockSide;
        rec.lastDockRatio = f->lastDockRatio;
    } else {
        CaptureRestoreHint(id, rec.lastDockTarget, rec.lastDockSide, rec.lastDockRatio);
    }

    const bool removed = RemovePanel(id);
    if (!IsPanelClosed(id)) {
        m_closedPanels.push_back(DockPanelId(id));
        m_closedHistory.push_back(rec);
        ++m_revision;
    }
    return removed;
}

bool DockLayout::ReopenPanel(keel::StringRef id) {
    if (!IsValidPanelId(id)) return false;
    if (HasPanel(id)) {
        EraseClosedRecord(id);
        return SetActivePanel(id);
    }

    DockPanelId target{};
    DockSide side = DockSide::Center;
    keel::f32 ratio = 0.5f;
    for (const auto& h : m_closedHistory) {
        if (h.panelId.as_ref() == id) {
            target = h.lastDockTarget;
            side = h.lastDockSide;
            ratio = h.lastDockRatio;
            break;
        }
    }
    EraseClosedRecord(id);
    if (!target.empty() && HasDockedPanel(target.as_ref())) {
        return DockPanel(id, side, target.as_ref(), ratio);
    }
    return DockPanel(id, DockSide::Center, {}, 0.5f);
}

bool DockLayout::DockPanelToRoot(keel::StringRef id, DockSide side, keel::f32 ratio) {
    if (!IsValidPanelId(id)) return false;
    EraseClosedRecord(id);

    // Avoid splitting sole root panel onto itself
    if (m_rootIndex < m_nodes.size() &&
        m_nodes[m_rootIndex].kind == DockNodeKind::Tabs &&
        m_nodes[m_rootIndex].panelIds.size() == 1u &&
        m_nodes[m_rootIndex].panelIds[0].as_ref() == id) {
        return false;
    }

    RemoveFromDockedTree(id);
    RemoveFromFloating(id);

    if (m_rootIndex >= m_nodes.size()) {
        DockNode root{};
        root.kind = DockNodeKind::Tabs;
        root.panelIds.push_back(DockPanelId(id));
        root.activeTab = 0u;
        m_nodes.clear();
        m_nodes.push_back(keel::move(root));
        m_rootIndex = 0u;
        ++m_revision;
        return true;
    }

    if (side == DockSide::Center) {
        const keel::u32 firstTab = FindFirstTabNode();
        if (firstTab != kDockInvalidNode && firstTab < m_nodes.size()) {
            m_nodes[firstTab].panelIds.push_back(DockPanelId(id));
            m_nodes[firstTab].activeTab = m_nodes[firstTab].panelIds.size() - 1u;
            ++m_revision;
            return true;
        }
        return false;
    }

    const keel::u32 oldRoot = m_rootIndex;
    const keel::u32 newTabIdx = keel::as<keel::u32>(m_nodes.size());
    DockNode newTab{};
    newTab.kind = DockNodeKind::Tabs;
    newTab.panelIds.push_back(DockPanelId(id));
    newTab.activeTab = 0u;
    m_nodes.push_back(keel::move(newTab));

    const bool isHoriz = (side == DockSide::Left || side == DockSide::Right);
    const bool isFirst = (side == DockSide::Left || side == DockSide::Top);
    const keel::f32 clamped = ClampRatio(ratio);

    const keel::u32 splitIdx = keel::as<keel::u32>(m_nodes.size());
    DockNode split{};
    split.kind = DockNodeKind::Split;
    split.orientation = isHoriz ? Orientation::Horizontal : Orientation::Vertical;
    split.first = isFirst ? newTabIdx : oldRoot;
    split.second = isFirst ? oldRoot : newTabIdx;
    split.ratio = isFirst ? clamped : ClampRatio(1.0f - clamped);
    m_nodes.push_back(keel::move(split));
    m_rootIndex = splitIdx;
    ++m_revision;
    return true;
}

bool DockLayout::MovePanelToNode(
    keel::StringRef id,
    keel::u32 targetNodeIdx,
    DockSide side,
    keel::f32 ratio,
    keel::usize insertIndex
) {
    if (!IsValidPanelId(id)) return false;
    EraseClosedRecord(id);

    if (targetNodeIdx >= m_nodes.size() || m_nodes[targetNodeIdx].kind != DockNodeKind::Tabs) {
        return DockPanelToRoot(id, side, ratio);
    }

    // Check if dropping a sole tab onto its own group (no-op)
    const DockNode& tgtBefore = m_nodes[targetNodeIdx];
    if (tgtBefore.panelIds.size() == 1u && tgtBefore.panelIds[0].as_ref() == id) {
        return false;
    }

    // Remember a stable anchor panel inside targetNodeIdx before removing `id` (compaction may re-index)
    DockPanelId anchorId{};
    for (const auto& pid : tgtBefore.panelIds) {
        if (pid.as_ref() != id) {
            anchorId = pid;
            break;
        }
    }

    RemoveFromDockedTree(id);
    RemoveFromFloating(id);

    const keel::u32 resolvedTarget = anchorId.empty() ? FindFirstTabNode() : FindPanelNode(anchorId.as_ref());
    if (resolvedTarget == kDockInvalidNode || resolvedTarget >= m_nodes.size()) {
        return DockPanelToRoot(id, side, ratio);
    }

    if (side == DockSide::Center) {
        DockNode& dst = m_nodes[resolvedTarget];
        const keel::usize pos = (insertIndex < dst.panelIds.size()) ? insertIndex : dst.panelIds.size();
        dst.panelIds.push_back(DockPanelId(id));
        for (keel::usize i = dst.panelIds.size() - 1u; i > pos; --i) {
            dst.panelIds[i] = dst.panelIds[i - 1u];
        }
        dst.panelIds[pos] = DockPanelId(id);
        dst.activeTab = pos;
        ++m_revision;
        return true;
    }

    const keel::u32 parentIdx = FindParentSplit(resolvedTarget);
    const keel::u32 newTabIdx = keel::as<keel::u32>(m_nodes.size());
    DockNode newTab{};
    newTab.kind = DockNodeKind::Tabs;
    newTab.panelIds.push_back(DockPanelId(id));
    newTab.activeTab = 0u;
    m_nodes.push_back(keel::move(newTab));

    const bool isHoriz = (side == DockSide::Left || side == DockSide::Right);
    const bool isFirst = (side == DockSide::Left || side == DockSide::Top);
    const keel::f32 clamped = ClampRatio(ratio);

    const keel::u32 splitIdx = keel::as<keel::u32>(m_nodes.size());
    DockNode split{};
    split.kind = DockNodeKind::Split;
    split.orientation = isHoriz ? Orientation::Horizontal : Orientation::Vertical;
    split.first = isFirst ? newTabIdx : resolvedTarget;
    split.second = isFirst ? resolvedTarget : newTabIdx;
    split.ratio = isFirst ? clamped : ClampRatio(1.0f - clamped);
    m_nodes.push_back(keel::move(split));

    if (parentIdx == kDockInvalidNode) {
        m_rootIndex = splitIdx;
    } else if (m_nodes[parentIdx].first == resolvedTarget) {
        m_nodes[parentIdx].first = splitIdx;
    } else {
        m_nodes[parentIdx].second = splitIdx;
    }
    ++m_revision;
    return true;
}

bool DockLayout::DockPanel(
    keel::StringRef id,
    DockSide side,
    keel::StringRef targetPanelId,
    keel::f32 ratio
) {
    if (!IsValidPanelId(id)) return false;
    EraseClosedRecord(id);

    if (targetPanelId.empty()) {
        return DockPanelToRoot(id, side, ratio);
    }
    if (targetPanelId == id) {
        return SetActivePanel(id);
    }
    const keel::u32 targetNode = FindPanelNode(targetPanelId);
    if (targetNode == kDockInvalidNode) {
        return DockPanelToRoot(id, side, ratio);
    }
    return MovePanelToNode(id, targetNode, side, ratio);
}

bool DockLayout::ReorderTab(keel::u32 tabNodeIdx, keel::usize fromIndex, keel::usize toIndex) {
    if (tabNodeIdx >= m_nodes.size() || m_nodes[tabNodeIdx].kind != DockNodeKind::Tabs) {
        return false;
    }
    DockNode& tab = m_nodes[tabNodeIdx];
    const keel::usize n = tab.panelIds.size();
    if (n <= 1u || fromIndex >= n) return false;
    if (toIndex >= n) toIndex = n - 1u;
    if (fromIndex == toIndex) return false;

    const DockPanelId activeId = (tab.activeTab < n) ? tab.panelIds[tab.activeTab] : DockPanelId{};
    const DockPanelId moved = tab.panelIds[fromIndex];
    tab.panelIds.remove_at(fromIndex);
    tab.panelIds.push_back(moved);
    for (keel::usize i = tab.panelIds.size() - 1u; i > toIndex; --i) {
        tab.panelIds[i] = tab.panelIds[i - 1u];
    }
    tab.panelIds[toIndex] = moved;

    for (keel::usize i = 0; i < tab.panelIds.size(); ++i) {
        if (tab.panelIds[i] == activeId) {
            tab.activeTab = i;
            break;
        }
    }
    ++m_revision;
    return true;
}

bool DockLayout::FloatPanel(keel::StringRef id, Rect localBounds) {
    if (!IsValidPanelId(id)) return false;
    EraseClosedRecord(id);

    if (DockFloatCard* existing = FindFloatCard(id)) {
        existing->bounds = localBounds;
        existing->zOrder = NextZOrder();
        existing->minimized = false;
        ++m_revision;
        return true;
    }

    DockPanelId hintTarget{};
    DockSide hintSide = DockSide::Center;
    keel::f32 hintRatio = 0.5f;
    CaptureRestoreHint(id, hintTarget, hintSide, hintRatio);

    RemoveFromDockedTree(id);

    DockFloatCard card{};
    card.panelId = DockPanelId(id);
    card.bounds = Rect(
        localBounds.x,
        localBounds.y,
        keel::max(kFloatMinWidthDp, localBounds.w),
        keel::max(kFloatMinHeightDp, localBounds.h)
    );
    card.zOrder = NextZOrder();
    card.minimized = false;
    card.lastDockTarget = hintTarget;
    card.lastDockSide = hintSide;
    card.lastDockRatio = hintRatio;
    m_floating.push_back(card);
    ++m_revision;
    return true;
}

bool DockLayout::RedockPanel(keel::StringRef id) {
    DockFloatCard* card = FindFloatCard(id);
    if (!card) return false;

    const DockPanelId target = card->lastDockTarget;
    const DockSide side = card->lastDockSide;
    const keel::f32 ratio = card->lastDockRatio;

    RemoveFromFloating(id);

    if (!target.empty() && HasDockedPanel(target.as_ref())) {
        return DockPanel(id, side, target.as_ref(), ratio);
    }
    return DockPanel(id, DockSide::Center, {}, 0.5f);
}

bool DockLayout::SetActivePanel(keel::StringRef id) {
    const keel::u32 nodeIdx = FindPanelNode(id);
    if (nodeIdx != kDockInvalidNode && nodeIdx < m_nodes.size()) {
        DockNode& tab = m_nodes[nodeIdx];
        for (keel::usize i = 0; i < tab.panelIds.size(); ++i) {
            if (tab.panelIds[i].as_ref() == id) {
                if (tab.activeTab != i) {
                    tab.activeTab = i;
                    ++m_revision;
                }
                return true;
            }
        }
    }
    return BringFloatToFront(id);
}

bool DockLayout::BringFloatToFront(keel::StringRef id) {
    DockFloatCard* card = FindFloatCard(id);
    if (!card) return false;
    const keel::u32 nextZ = NextZOrder();
    if (card->zOrder + 1u < nextZ || card->minimized) {
        card->zOrder = nextZ;
        card->minimized = false;
        ++m_revision;
    }
    return true;
}

bool DockLayout::ExpandPanel(keel::StringRef id, bool fullscreen) {
    if (!IsValidPanelId(id) || !HasPanel(id)) {
        return false;
    }
    SetActivePanel(id);
    if (m_expandedPanelId.as_ref() != id || m_expandedFullscreen != fullscreen) {
        m_expandedPanelId = DockPanelId(id);
        m_expandedFullscreen = fullscreen;
        ++m_revision;
    }
    return true;
}

bool DockLayout::SetExpandedFullscreen(bool fullscreen) noexcept {
    if (m_expandedPanelId.empty()) {
        return false;
    }
    if (m_expandedFullscreen != fullscreen) {
        m_expandedFullscreen = fullscreen;
        ++m_revision;
    }
    return true;
}

bool DockLayout::RestoreExpanded() noexcept {
    if (m_expandedPanelId.empty()) {
        return false;
    }
    m_expandedPanelId.clear();
    m_expandedFullscreen = false;
    ++m_revision;
    return true;
}

bool DockLayout::SetSplitRatio(keel::u32 nodeIdx, keel::f32 ratio) noexcept {
    if (nodeIdx >= m_nodes.size() || m_nodes[nodeIdx].kind != DockNodeKind::Split) {
        return false;
    }
    const keel::f32 clamped = ClampRatio(ratio);
    if (keel::abs(m_nodes[nodeIdx].ratio - clamped) > 1e-5f) {
        m_nodes[nodeIdx].ratio = clamped;
        ++m_revision;
        return true;
    }
    return false;
}

keel::String DockLayout::SaveLayout() const {
    keel::String out;
    if (m_rootIndex >= m_nodes.size() && m_floating.empty()) {
        return out;
    }
    if (m_floating.empty()) {
        SerializeNodeRecursive(*this, m_rootIndex, out);
        return out;
    }

    out.append("D2(");
    if (m_rootIndex < m_nodes.size()) {
        SerializeNodeRecursive(*this, m_rootIndex, out);
    }
    for (const auto& f : m_floating) {
        out.append(";F(");
        out.append(f.panelId.as_ref());
        out.push_back(',');
        AppendCompactFloat(out, f.bounds.x);
        out.push_back(',');
        AppendCompactFloat(out, f.bounds.y);
        out.push_back(',');
        AppendCompactFloat(out, f.bounds.w);
        out.push_back(',');
        AppendCompactFloat(out, f.bounds.h);
        if (f.minimized) {
            out.append(",1");
        }
        out.push_back(')');
    }
    out.push_back(')');
    return out;
}

bool DockLayout::LoadLayout(keel::StringRef serialized) {
    keel::Array<DockNode> parsedNodes;
    keel::u32 parsedRoot = kDockInvalidNode;
    keel::Array<DockFloatCard> parsedFloating;

    LayoutParser parser(serialized);
    if (!parser.Parse(parsedNodes, parsedRoot, parsedFloating)) {
        return false;
    }

    m_nodes = keel::move(parsedNodes);
    m_rootIndex = parsedRoot;
    m_floating = keel::move(parsedFloating);
    m_closedPanels.clear();
    m_closedHistory.clear();
    m_expandedPanelId.clear();
    m_expandedFullscreen = false;
    ++m_revision;
    return true;
}

} // namespace guikit
