#include "guikit/context.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/math.h"
#include "keel/static_array.h"
#include "keel/time.h"
#include "keel/utf8.h"
#include <cassert>
#include "keel/memory.h"

namespace guikit {

namespace {

[[nodiscard]] bool ContainsId(keel::ArrayRef<Id> list, Id target) noexcept {
    for (const Id& id : list) {
        if (id == target) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] keel::usize FindIdIndex(keel::ArrayRef<Id> list, Id target) noexcept {
    for (keel::usize i = 0; i < list.size(); ++i) {
        if (list[i] == target) {
            return i;
        }
    }
    return keel::kInvalidIndex;
}

/// Searches backwards in `s[0 .. maxExclusivePos - 1]` for `c`, returning `keel::StringRef::npos` if not found.
[[nodiscard]] keel::usize StrRfindBefore(keel::StringRef s, char c, keel::usize maxExclusivePos) noexcept {
    if (maxExclusivePos == 0 || s.empty()) {
        return keel::StringRef::npos;
    }
    return s.substr(0, keel::min(maxExclusivePos, s.size())).rfind(c);
}

void StrEraseRange(keel::String& s, keel::usize pos, keel::usize count) noexcept {
    if (pos >= s.size() || count == 0) {
        return;
    }
    const keel::usize actual = keel::min(count, s.size() - pos);
    const keel::usize tail = s.size() - (pos + actual);
    if (tail > 0) {
        keel::mem_move(s.data() + pos, s.data() + pos + actual, tail);
    }
    s.resize(s.size() - actual);
}

void StrInsert(keel::String& s, keel::usize pos, keel::StringRef ins) {
    if (ins.empty()) {
        return;
    }
    pos = keel::min(pos, s.size());
    if (pos == s.size()) {
        s.append(ins);
        return;
    }
    if (s.data() != nullptr && ins.data() >= s.data() && ins.data() <= s.data() + s.size()) {
        keel::String copy = keel::String::from(ins);
        StrInsert(s, pos, copy.as_ref());
        return;
    }
    const keel::usize oldLen = s.size();
    const keel::usize insLen = ins.size();
    s.resize(oldLen + insLen);
    keel::mem_move(s.data() + pos + insLen, s.data() + pos, oldLen - pos);
    keel::mem_copy(s.data() + pos, ins.data(), insLen);
}

void StrInsertChar(keel::String& s, keel::usize pos, char c) {
    StrInsert(s, pos, keel::StringRef(&c, 1));
}

void StrReplace(keel::String& s, keel::usize pos, keel::usize count, keel::StringRef rep) {
    pos = keel::min(pos, s.size());
    if (s.data() != nullptr && rep.data() >= s.data() && rep.data() <= s.data() + s.size()) {
        keel::String copy = keel::String::from(rep);
        StrReplace(s, pos, count, copy.as_ref());
        return;
    }
    StrEraseRange(s, pos, count);
    StrInsert(s, pos, rep);
}

template <typename T, typename Pred>
void ArrayEraseIf(keel::Array<T>& arr, Pred&& pred) noexcept {
    keel::usize writeIdx = 0;
    for (keel::usize i = 0; i < arr.size(); ++i) {
        if (!pred(arr[i])) {
            if (writeIdx != i) {
                arr[writeIdx] = keel::move(arr[i]);
            }
            ++writeIdx;
        }
    }
    arr.resize(writeIdx);
}

/// The transform a scaled node adds on top of its parent's space: it scales about its own top-left corner.
[[nodiscard]] Transform2D NodeLocalTransform(const Node* node) noexcept {
    if (node == nullptr) {
        return Transform2D{};
    }
    const float relScale = node->GetRelativeScale();
    if (relScale == 1.0f || !(relScale > 0.0f)) {
        return Transform2D{};
    }
    return Transform2D::ScaleAbout(Vec2{node->computedRect.x, node->computedRect.y}, relScale);
}

/// The transform from a node's own coordinates to frame space, given its parent's.
[[nodiscard]] Transform2D NodeSpace(const Node* node, const Transform2D& parentSpace) noexcept {
    const Transform2D local = NodeLocalTransform(node);
    return local.IsIdentity() ? parentSpace : parentSpace.Then(local);
}

} // namespace

Context::Context()
    : m_startTimeNs(keel::monotonic_ns()) {
}

Context::~Context() {
    ClearOverlayPasses();
    ClearOverlayHitBuilders();
}

void Context::ClearOverlayPasses() noexcept {
    for (const auto& pass : m_overlayPasses) {
        if (pass.destroy && pass.ctx) {
            pass.destroy(pass.ctx);
        }
    }
    m_overlayPasses.clear();
}

void Context::ClearOverlayHitBuilders() noexcept {
    for (const auto& b : m_overlayHitBuilders) {
        if (b.destroy && b.ctx) {
            b.destroy(b.ctx);
        }
    }
    m_overlayHitBuilders.clear();
}

void Context::BeginFrame(Vec2 viewportSize, float displayScale, float rootZoom) {
    ++m_frameIndex;
    const float effDisplay = (displayScale > 0.0f) ? displayScale : 1.0f;
    const float effZoom = (rootZoom > 0.0f) ? rootZoom : 1.0f;
    const float effScale = effDisplay * effZoom;
    if (m_viewportSize != viewportSize || m_pixelsPerDp != effScale) {
        InvalidateAll();
    }
    if (m_pixelsPerDp != effScale) {
        // The cursor did not move on screen; only its dp coordinate changed
        m_mousePos = m_mousePosPx / effScale;
    }
    m_viewportSize = viewportSize;
    m_displayScale = effDisplay;
    m_rootZoom = effZoom;
    m_pixelsPerDp = effScale;

    if (!m_manualTime) {
        const keel::u64 nowNs = keel::monotonic_ns();
        m_currentTimeSec = (nowNs >= m_startTimeNs)
            ? static_cast<double>(nowNs - m_startTimeNs) * 1e-9
            : 0.0;
    }

    ClearOverlayPasses();
    ClearOverlayHitBuilders();
    m_currFrameModalOverlay = false;
    m_openPopupVisualHash = 0;
    m_currFocusableIds.clear();
    m_currModalFocusableIds.clear();
    m_currFrameModalId = Id::Null();
    m_currFrameModalBackdropRect = Rect{};
    m_modalScopeDepth = 0;
    m_modalClosedThisFrame = false;
    m_toastHostVisitedThisFrame = false;
    m_activeTooltip = {};
    m_activeTooltipAnchor = {};
    m_activeTooltipViewport = {};
    m_cursor = CursorType::Default;

    const auto& activeFocusables = (m_prevFrameModalId.IsValid() && !m_prevModalFocusableIds.empty())
        ? m_prevModalFocusableIds
        : m_prevFocusableIds;

    if (!m_openPopupId.IsValid() && !activeFocusables.empty()) {
        const bool multilineEditorFocused =
            m_focusedId.IsValid() &&
            m_editState.id == m_focusedId &&
            (m_editState.multiline || m_suspendDirectEditInput) &&
            !m_editState.readOnly;
        for (keel::usize evIdx = 0; evIdx < m_pendingKeyEvents.size(); ) {
            const QueuedKeyEvent ev = m_pendingKeyEvents[evIdx];
            const bool isIndent = m_keymap.Matches(KeyAction::Indent, ev.key, ev.mods);
            const bool isUnindent = m_keymap.Matches(KeyAction::Unindent, ev.key, ev.mods);
            if ((isIndent || isUnindent) && !multilineEditorFocused) {
                const bool reverse = isUnindent;
                m_pendingKeyEvents.remove_at(evIdx);
                const keel::usize curIdx = FindIdIndex(activeFocusables, m_focusedId);
                Id nextId = Id::Null();
                if (curIdx == keel::kInvalidIndex) {
                    nextId = reverse ? activeFocusables.back() : activeFocusables.front();
                } else {
                    const keel::usize n = activeFocusables.size();
                    const keel::usize nextIdx = reverse ? ((curIdx + n - 1) % n) : ((curIdx + 1) % n);
                    nextId = activeFocusables[nextIdx];
                }
                if (m_editState.id.IsValid() && m_editState.id != nextId) {
                    FlushActiveEditHistory();
                    m_editState.id = Id::Null();
                }
                m_focusedId = nextId;
                m_inputModality = InputModality::Keyboard;
                m_focusVisible = true;
                m_suspendDirectEditInput = false;
                InvalidateAll();
            } else {
                ++evIdx;
            }
        }
    }

    if (m_mouseClicked && m_openPopupId.IsValid()) {
        const Rect* selectBounds = m_prevFrameBounds.get(m_openPopupId);
        const bool overSelect = (selectBounds != nullptr) && selectBounds->Contains(m_mousePos);
        const bool overAnchor = (m_openPopupAnchorRect.w > 0.0f && m_openPopupAnchorRect.h > 0.0f) &&
                                m_openPopupAnchorRect.Contains(m_mousePos);
        const bool overPopup = m_openPopupRect.Contains(m_mousePos);
        if (!overSelect && !overAnchor && !overPopup) {
            ClosePopup();
            ConsumeMouseClick();
        }
    }

    if (m_mouseClicked && m_focusedId.IsValid()) {
        const bool overOwnPopup = (m_openPopupId == m_focusedId) && m_openPopupRect.Contains(m_mousePos);
        if (!overOwnPopup) {
            const Rect* focusedBounds = m_prevFrameBounds.get(m_focusedId);
            if (focusedBounds != nullptr) {
                if (!focusedBounds->Contains(m_mousePos)) {
                    if (m_editState.id == m_focusedId) {
                        FlushActiveEditHistory();
                        m_editState.id = Id::Null();
                    }
                    m_focusedId = Id::Null();
                    m_focusVisible = false;
                    m_suspendDirectEditInput = false;
                }
            } else {
                if (m_editState.id == m_focusedId) {
                    FlushActiveEditHistory();
                    m_editState.id = Id::Null();
                }
                m_focusedId = Id::Null();
                m_focusVisible = false;
                m_suspendDirectEditInput = false;
            }
        }
    }

    m_arena.reset();
    m_childIdCounts.clear();
    m_containerStack.clear();
    m_zoneStack.clear();
    m_currFrameBounds.clear();
    m_currUnclippedBounds.clear();
    m_currNodeSpaces.clear();
    m_currHitNodes.clear();
    m_currOverlayHitRegions.clear();
    m_activeOverlayHitCheckId = Id::Null();
    m_widgetExecDepth = 0;
    m_rowDisabled = false;

    m_rootZone = guikit::Zone{
        .theme = &m_theme,
        .scale = 1.0f,
        .density = m_rootDensity,
        .debugBounds = keel::kNullOpt,
        .inspectMode = keel::kNullOpt
    };
    m_zoneStack.push_back(&m_rootZone);

    // Create root container spanning the entire logical viewport
    const Vec2 logicalSize = m_viewportSize / m_pixelsPerDp;
    m_root = m_arena.create<Node>();
    m_root->type = NodeType::Column;
    m_root->direction = FlexDirection::Column;
    m_root->widthMode = SizeMode::Stretch;
    m_root->heightMode = SizeMode::Stretch;
    m_root->computedRect = Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y);
    m_root->id = Id{0xFFFF'FFFF'FFFF'FFFF};
    m_root->zone = &m_rootZone;
    m_root->uiSize = UiSize::Md;
    m_root->disabled = false;

    m_containerStack.push_back(m_root);
}

ZoneScope::~ZoneScope() noexcept {
    PopIfNeeded();
}

void ZoneScope::PopIfNeeded() noexcept {
    if (m_ctx != nullptr) {
        m_ctx->PopZone();
        m_ctx = nullptr;
    }
}

ZoneScope Context::Zone(const ZoneOptions& opts) {
    PushZone(opts);
    return ZoneScope(this);
}

void Context::PushZone(const ZoneOptions& opts) {
    m_zoneStack.push_back(ResolveZone(GetZone(), opts, m_arena));
}

void Context::PopZone() noexcept {
    if (m_zoneStack.size() > 1) {
        m_zoneStack.pop_back();
    }
}

Node* Context::CreateNode(NodeType type, Id id, bool disabled) {
    Node* node = m_arena.create<Node>();
    node->type = type;
    node->id = id;
    node->zone = GetZone();
    node->disabled = IsDisabled() || disabled;

    if (!m_containerStack.empty()) {
        m_containerStack.back()->AddChild(node);
    }
    return node;
}

Id Context::MakeChildIdForParent(Id parentId, keel::SourceLocation loc) {
    const char* filePtr = loc.file_name();
    keel::u64 fileHash = detail::Fnv1aOffsetBasis;
    if (filePtr != nullptr) {
        if (const keel::u64* cached = m_fileHashCache.get(filePtr)) {
            fileHash = *cached;
        } else {
            fileHash = detail::HashStringView(filePtr);
            m_fileHashCache.put(filePtr, fileHash);
        }
    }
    const keel::u64 callsiteHash = detail::HashCallsiteWithFileHash(
        parentId, fileHash, loc.line(), loc.column()
    );
    const keel::u32 occurrenceIndex = m_childIdCounts.get_or_insert(callsiteHash, 0u)++;
    return detail::FinalizeCallsiteId(callsiteHash, occurrenceIndex);
}

Id Context::MakeChildId(keel::SourceLocation loc) {
    const Id parentId = m_containerStack.empty() ? Id::Null() : m_containerStack.back()->id;
    return MakeChildIdForParent(parentId, loc);
}

Response Context::Interact(Id id) {
    Response res;
    res.id = id;
    res.ctx = this;

    const Rect* pUnclipped = m_prevUnclippedBounds.get(id);
    const Rect* pClipped = m_prevFrameBounds.get(id);
    if (pUnclipped != nullptr) {
        res.rect = CurrentSpace().Invert(*pUnclipped);
    } else if (pClipped != nullptr) {
        res.rect = CurrentSpace().Invert(*pClipped);
    }

    // Suppress interaction on background widgets occluded by floating popups, modals, splitter sashes, or higher Z-layer panels
    if (IsMouseOccludedInFrame(id, m_mousePos)) {
        if (m_mouseReleased && m_activeId == id) {
            m_activeId = Id::Null();
        }
        return res;
    }

    if (pClipped != nullptr) {
        res.hovered = pClipped->Contains(m_mousePos);
    }

    if (!m_mouseClicked && !m_rightMouseClicked && !m_middleMouseClicked &&
        (m_mouseDown || m_rightMouseDown || m_middleMouseDown) &&
        m_activeId.IsValid() && m_activeId != id) {
        res.hovered = false;
    }

    if (res.hovered) {
        m_hoveredId = id;
        if (m_mouseClicked) {
            m_activeId = id;
        }
        if (m_mouseDoubleClicked) {
            res.doubleClicked = true;
        }
        if (m_rightMouseClicked) {
            res.rightClicked = true;
        }
    }

    res.pressed = (m_activeId == id && m_mouseDown);
    res.clicked = (m_activeId == id && m_mouseReleased && res.hovered);

    if (m_mouseReleased && m_activeId == id) {
        m_activeId = Id::Null();
    }

    return res;
}

void Context::AdaptChildToContainer(Node* node) {
    if (!node) return;
    Node* parent = node->parent ? node->parent : (m_containerStack.empty() ? nullptr : m_containerStack.back());
    if (!parent) return;
    if (parent->direction == FlexDirection::Column || parent->direction == FlexDirection::Grid) {
        if (node->widthMode != SizeMode::Fit && node->widthMode != SizeMode::Fixed) {
            node->widthMode = SizeMode::Stretch;
        }
        if (parent->heightMode == SizeMode::Fixed && node->heightMode != SizeMode::Fixed) {
            node->heightMode = SizeMode::Stretch;
        }
    }
}

Transform2D Context::GetNodeSpace(Id id) const noexcept {
    if (const Transform2D* t = m_prevNodeSpaces.get(id)) {
        return *t;
    }
    return Transform2D{};
}

Transform2D Context::CurrentSpace() const noexcept {
    if (m_spaceOverride != nullptr) {
        return *m_spaceOverride;
    }
    if (!m_containerStack.empty()) {
        const Node* top = m_containerStack.back();
        if (top != nullptr && top->id.IsValid()) {
            return GetNodeSpace(top->id);
        }
    }
    return Transform2D{};
}

Rect Context::GetPrevFrameBounds(Id id) const {
    if (const Rect* r = m_prevFrameBounds.get(id)) {
        return CurrentSpace().Invert(*r);
    }
    return Rect{};
}

Rect Context::GetPrevUnclippedBounds(Id id) const {
    if (const Rect* r = m_prevUnclippedBounds.get(id)) {
        return CurrentSpace().Invert(*r);
    }
    return GetPrevFrameBounds(id);
}

Rect Context::GetParentPrevFrameBounds() const {
    keel::i32 startIdx = -1;
    Rect r{};
    for (keel::i32 i = static_cast<keel::i32>(m_containerStack.size()) - 1; i >= 0; --i) {
        const Node* parent = m_containerStack[static_cast<keel::usize>(i)];
        if (!parent || !parent->id.IsValid()) continue;
        const Rect* found = m_prevFrameBounds.get(parent->id);
        if (found != nullptr && found->w > 1.0f && found->h > 1.0f) {
            startIdx = i;
            r = *found;
            break;
        }
    }
    if (startIdx < 0) {
        return Rect{};
    }

    const keel::i32 stackSize = static_cast<keel::i32>(m_containerStack.size());
    for (keel::i32 i = startIdx; i < stackSize; ++i) {
        const Node* c = m_containerStack[static_cast<keel::usize>(i)];
        if (!c) continue;

        const float localToFrame = GetNodeSpace(c->id).scale;                        // c's own units
        const float outerToFrame = localToFrame / c->GetRelativeScale();             // c's parent's units
        if (i > startIdx) {
            if (c->heightMode == SizeMode::Fixed && c->fixedHeight > 0.0f) {
                const float h = keel::min(r.h, c->fixedHeight * outerToFrame);
                if (c->type == NodeType::BottomPanel) {
                    r.y = r.Bottom() - h;
                }
                r.h = h;
            }
            if (c->widthMode == SizeMode::Fixed && c->fixedWidth > 0.0f) {
                r.w = keel::min(r.w, c->fixedWidth * outerToFrame);
            }
        } else if ((c->type == NodeType::BottomPanel || c->type == NodeType::Sidebar) &&
                   c->heightMode == SizeMode::Fixed && c->fixedHeight > 0.0f) {
            r.h = c->fixedHeight * outerToFrame;
        }

        r.x += c->padding.left * localToFrame;
        r.y += c->padding.top * localToFrame;
        r.w = keel::max(0.0f, r.w - (c->padding.left + c->padding.right) * localToFrame);
        r.h = keel::max(0.0f, r.h - (c->padding.top + c->padding.bottom) * localToFrame);

        if (c->direction == FlexDirection::Column &&
            (i + 1 < stackSize || c->type == NodeType::BottomPanel || c->type == NodeType::Sidebar)) {
            const Node* stopChild = (i + 1 < stackSize)
                ? m_containerStack[static_cast<keel::usize>(i + 1)]
                : c->lastChild;
            for (const Node* ch = c->firstChild; ch != nullptr && ch != stopChild; ch = ch->nextSibling) {
                if (ch->heightMode == SizeMode::Fixed && ch->fixedHeight > 0.0f && !ch->sidebarCollapsed) {
                    const float consumeH = (ch->fixedHeight + c->gap) * localToFrame;
                    r.y += consumeH;
                    r.h = keel::max(0.0f, r.h - consumeH);
                }
            }
        }
    }

    return (r.w > 1.0f && r.h > 1.0f) ? CurrentSpace().Invert(r) : Rect{};
}

Rect Context::GetPopupViewportRect() const noexcept {
    return CurrentSpace().Invert(PopupViewportInFrame());
}

Rect Context::PopupViewportInFrame() const noexcept {
    const Vec2 logical = GetLogicalViewport();
    Rect vp(0.0f, 0.0f, logical.x, logical.y);
    for (const Node* n : m_containerStack) {
        if (!n || !n->id.IsValid()) continue;
        if (n->type == NodeType::Box && n->clipChildren) {
            const Rect* found = m_prevFrameBounds.get(n->id);
            if (found != nullptr && found->w > 1.0f && found->h > 1.0f) {
                Rect boxRect = *found;
                const float border = n->borderWidth * GetNodeSpace(n->id).scale;
                if (border > 0.0f && boxRect.w > border * 2.0f && boxRect.h > border * 2.0f) {
                    boxRect = boxRect.Inset(border);
                }
                const Rect intersected = vp.Intersect(boxRect);
                if (intersected.w > 1.0f && intersected.h > 1.0f) {
                    vp = intersected;
                }
            }
        }
    }
    return vp;
}

void Context::PushContainer(Node* node) {
    m_containerStack.push_back(node);
}

void Context::PopContainer(Node* node) {
    if (!m_containerStack.empty() && m_containerStack.back() == node) {
        m_containerStack.pop_back();
        if (node->type == NodeType::Element) {
            Element::SyncWithChildren(*this, node);
        }
    }
}

ContainerScope Context::Row(RowOptions opts, keel::SourceLocation loc) {
    return Open<guikit::Row>(opts, loc);
}

ContainerScope Context::Column(ColumnOptions opts, keel::SourceLocation loc) {
    return Open<guikit::Column>(opts, loc);
}

ContainerScope Context::Wrap(WrapOptions opts, keel::SourceLocation loc) {
    return Open<guikit::Wrap>(opts, loc);
}

GridScope Context::Grid(GridOptions opts, keel::SourceLocation loc) {
    return Open<guikit::Grid>(opts, loc);
}

void Context::Spacer(float flex, keel::SourceLocation loc) {
    Run<guikit::Spacer>(flex, loc);
}

void Context::Gap(float sizeDp, keel::SourceLocation loc) {
    guikit::Spacer::Gap(*this, sizeDp, loc);
}

void Context::Separator(float extentRatio, Color colorOverride, keel::SourceLocation loc) {
    guikit::Separator::Run(*this, extentRatio, colorOverride, loc);
}

void Context::Separator(Color colorOverride, keel::SourceLocation loc) {
    guikit::Separator::Run(*this, 0.0f, colorOverride, loc);
}

void Context::Separator(Orientation lineOrientation, float extentRatio, Color colorOverride, keel::SourceLocation loc) {
    guikit::Separator::Run(*this, extentRatio, colorOverride, loc, lineOrientation);
}

AccordionScope Context::Accordion(AccordionOptions opts, keel::SourceLocation loc) {
    return guikit::Accordion::Open(*this, nullptr, opts, loc);
}

AccordionScope Context::Accordion(keel::StringRef label, AccordionOptions opts, keel::SourceLocation loc) {
    opts.label.text = label;
    return guikit::Accordion::Open(*this, nullptr, opts, loc);
}

AccordionScope Context::Accordion(bool& open, AccordionOptions opts, keel::SourceLocation loc) {
    return guikit::Accordion::Open(*this, &open, opts, loc);
}

ContainerScope Context::Box(BoxOptions opts, keel::SourceLocation loc) {
    return Open<guikit::Box>(opts, loc);
}

ContainerScope Context::Card(CardOptions opts, keel::SourceLocation loc) {
    return Open<guikit::Card>(opts, loc);
}

ContainerScope Context::ScrollArea(ScrollAreaOptions opts, keel::SourceLocation loc) {
    return Open<guikit::ScrollArea>(opts, loc);
}

ContainerScope Context::Carousel(CarouselOptions opts, keel::SourceLocation loc) {
    return Open<guikit::Carousel>(opts, loc);
}

SidebarScope Context::Sidebar(SidebarOptions opts, keel::SourceLocation loc) {
    return guikit::Sidebar::Open(*this, nullptr, opts, loc);
}

SidebarScope Context::Sidebar(SidebarMode& mode, SidebarOptions opts, keel::SourceLocation loc) {
    return guikit::Sidebar::Open(*this, &mode, opts, loc);
}

SidebarScope Context::Sidebar(bool& open, SidebarOptions opts, keel::SourceLocation loc) {
    SidebarMode mode = open ? SidebarMode::Expanded : SidebarMode::Collapsed;
    SidebarScope scope = guikit::Sidebar::Open(*this, &mode, opts, loc);
    open = (mode != SidebarMode::Collapsed);
    return scope;
}

SceneScope Context::Scene(SceneOptions opts, keel::SourceLocation loc) {
    return Open<guikit::Scene>(opts, loc);
}

TreeViewScope Context::TreeView(TreeViewOptions opts, keel::SourceLocation loc) {
    return Open<guikit::TreeView>(opts, loc);
}

TreeItemScope Context::TreeItem(TreeItemOptions opts, keel::SourceLocation loc) {
    return guikit::TreeView::OpenItem(*this, nullptr, opts, loc);
}

TreeItemScope Context::TreeItem(keel::StringRef label, TreeItemOptions opts, keel::SourceLocation loc) {
    opts.label.text = label;
    return guikit::TreeView::OpenItem(*this, nullptr, opts, loc);
}

TreeItemScope Context::TreeItem(bool& open, TreeItemOptions opts, keel::SourceLocation loc) {
    return guikit::TreeView::OpenItem(*this, &open, opts, loc);
}

TreeItemScope Context::TreeItem(bool& open, keel::StringRef label, TreeItemOptions opts, keel::SourceLocation loc) {
    opts.label.text = label;
    return guikit::TreeView::OpenItem(*this, &open, opts, loc);
}

Response Context::Button(ButtonOptions opts, keel::SourceLocation loc) {
    return Run<guikit::Button>(opts, loc);
}

keel::StringRef Context::String(keel::StringRef text) {
    if (text.empty()) return keel::StringRef{};
    return m_arena.alloc_string(text);
}

keel::StringRef Context::AllocString(keel::StringRef text) {
    if (text.empty()) return keel::StringRef{};
    return m_arena.alloc_string(text);
}

Response Context::Text(UiText label, TextOptions opts, keel::SourceLocation loc) {
    return Run<guikit::Text>(label, opts, loc);
}

ElementScope Context::Element(ElementOptions opts, keel::SourceLocation loc) {
    return Open<guikit::Element>(opts, loc);
}

Response Context::Heading(HeadingOptions opts, keel::SourceLocation loc) {
    return Run<guikit::Heading>(opts, loc);
}

Response Context::Image(keel::StringRef source, ImageOptions opts, keel::SourceLocation loc) {
    return Run<guikit::Image>(source, opts, loc);
}

Response Context::Image(ImageOptions opts, keel::SourceLocation loc) {
    return Run<guikit::Image>(opts.source, opts, loc);
}

MarkdownResponse Context::Markdown(keel::StringRef markdownText, const MarkdownOptions& opts, keel::SourceLocation loc) {
    return Run<guikit::Markdown>(markdownText, opts, loc);
}

Response Context::Switch(bool& value, SwitchOptions opts, keel::SourceLocation loc) {
    return Run<guikit::Switch>(&value, value, opts, loc);
}

Response Context::Switch(const bool& checked, SwitchOptions opts, keel::SourceLocation loc) {
    return Run<guikit::Switch>(nullptr, checked, opts, loc);
}

Response Context::Checkbox(bool& value, CheckboxOptions opts, keel::SourceLocation loc) {
    return Run<guikit::Checkbox>(&value, value, opts, loc);
}

Response Context::Checkbox(const bool& checked, CheckboxOptions opts, keel::SourceLocation loc) {
    return Run<guikit::Checkbox>(nullptr, checked, opts, loc);
}

Response Context::Radio(bool& value, RadioOptions opts, keel::SourceLocation loc) {
    return Run<guikit::Radio>(&value, value, opts, loc);
}

Response Context::Radio(const bool& checked, RadioOptions opts, keel::SourceLocation loc) {
    return Run<guikit::Radio>(nullptr, checked, opts, loc);
}

Response Context::InputField(keel::StringRef value, InputFieldOptions opts, keel::SourceLocation loc) {
    return Run<guikit::InputField>(keel::Optional<keel::StringSink>{}, value, opts, loc);
}

Response Context::StepSlider(
    UiSize& size,
    StepSliderOptions opts,
    keel::SourceLocation loc
) {
    static constexpr keel::StaticArray<keel::StringRef, 6> kSizeLabelsWithNone = {{"A", "XS", "SM", "MD", "LG", "XL"}};
    static constexpr keel::StaticArray<UiSize, 6> kSizesWithNone = {{
        UiSize::None, UiSize::Xs, UiSize::Sm, UiSize::Md, UiSize::Lg, UiSize::Xl
    }};

    static constexpr keel::StaticArray<keel::StringRef, 5> kSizeLabels = {{"XS", "SM", "MD", "LG", "XL"}};
    static constexpr keel::StaticArray<UiSize, 5> kSizes = {{
        UiSize::Xs, UiSize::Sm, UiSize::Md, UiSize::Lg, UiSize::Xl
    }};

    const bool hasNone = opts.includeNone;
    const keel::ArrayRef<UiSize> sizesSpan = hasNone ? kSizesWithNone.as_ref() : kSizes.as_ref();
    const keel::ArrayRef<keel::StringRef> labelsSpan = hasNone ? kSizeLabelsWithNone.as_ref() : kSizeLabels.as_ref();

    keel::usize idx = hasNone ? 0 : 2;
    for (keel::usize i = 0; i < sizesSpan.size(); ++i) {
        if (sizesSpan[i] == size) {
            idx = i;
            break;
        }
    }

    if (opts.stepLabels.empty()) {
        opts.stepLabels = labelsSpan;
    }

    Response res = StepSlider(idx, opts.stepLabels.size(), opts, loc);
    if (res.changed && idx < sizesSpan.size()) {
        size = sizesSpan[idx];
    }
    return res;
}

Response Context::StepSlider(
    UiVariant& variant,
    StepSliderOptions opts,
    keel::SourceLocation loc
) {
    static constexpr keel::StaticArray<keel::StringRef, 8> kVariantLabels = {{
        "Def", "Pri", "Sec", "Suc", "Wrn", "Dgr", "Inf", "Gho"
    }};
    static constexpr keel::StaticArray<keel::StringRef, 8> kVariantFullNames = {{
        "Default", "Primary", "Secondary", "Success", "Warning", "Danger", "Info", "Ghost"
    }};
    static constexpr keel::StaticArray<UiVariant, 8> kVariants = {{
        UiVariant::Default, UiVariant::Primary, UiVariant::Secondary, UiVariant::Success,
        UiVariant::Warning, UiVariant::Danger, UiVariant::Info, UiVariant::Ghost
    }};

    keel::usize idx = 0;
    for (keel::usize i = 0; i < kVariants.size(); ++i) {
        if (kVariants[i] == variant) {
            idx = i;
            break;
        }
    }

    if (opts.stepLabels.empty()) {
        opts.stepLabels = kVariantLabels.as_ref();
    }
    if (opts.valueText.empty() && idx < kVariantFullNames.size()) {
        opts.valueText.text = kVariantFullNames[idx];
    }
    if (opts.valueText.variants.empty()) {
        opts.valueText.variants = kVariantFullNames.as_ref();
    }
    if (!opts.valueText.color.HasAny() && variant != UiVariant::Default) {
        opts.valueText.color = StateColor::FromTextVariant(variant);
    }
    if (!opts.color.HasAny() && variant != UiVariant::Default) {
        opts.color = StateColor::FromTextVariant(variant);
    }

    Response res = StepSlider(idx, opts.stepLabels.size(), opts, loc);
    if (res.changed && idx < kVariants.size()) {
        variant = kVariants[idx];
    }
    return res;
}

ToolbarScope Context::Toolbar(
    ToolbarOptions opts,
    keel::SourceLocation loc
) {
    return Open<guikit::Toolbar>(opts, loc);
}

Response Context::ToolButton(
    ToolButtonOptions opts,
    keel::SourceLocation loc
) {
    return Toolbar::ToolButton(*this, opts, loc);
}

SidebarMenuScope Context::SidebarMenu(
    SidebarMenuOptions opts,
    keel::SourceLocation loc
) {
    return Open<guikit::SidebarMenu>(opts, loc);
}

keel::u64 Context::ShowToast(ToastOptions opts) {
    const keel::u64 id = ++m_nextToastId;
    ToastEntry entry{};
    entry.id = id;
    entry.title = keel::String::from(opts.title.text);
    entry.message = keel::String::from(opts.message.text);
    entry.titleWrap = opts.title.wrap;
    entry.messageWrap = opts.message.wrap;
    entry.variant = opts.variant;
    entry.icon = opts.icon;
    if (!opts.icon.GetPath().empty()) {
        entry.iconPath = keel::String::from(opts.icon.GetPath());
    }
    entry.duration = opts.duration;
    entry.showProgress = opts.showProgress;
    entry.closable = opts.closable;
    entry.elapsedSec = 0.0;
    entry.lastTickSec = m_currentTimeSec;
    m_toasts.push_back(keel::move(entry));
    InvalidateAll();
    return id;
}

void Context::DismissToast(keel::u64 id) noexcept {
    for (keel::usize i = 0; i < m_toasts.size(); ++i) {
        if (m_toasts[i].id == id) {
            m_toasts.remove_at(i);
            InvalidateAll();
            return;
        }
    }
}

void Context::ClearToasts() noexcept {
    if (!m_toasts.empty()) {
        m_toasts.clear();
        InvalidateAll();
    }
}

void Context::RequestFocus(Id id, FocusReason reason) {
    if (m_focusedId != id && m_editState.id.IsValid() && m_editState.id != id) {
        FlushActiveEditHistory();
        m_editState.id = Id::Null();
    }
    m_focusedId = id;
    if (!id.IsValid()) {
        m_focusVisible = false;
    } else if (reason == FocusReason::Keyboard) {
        m_inputModality = InputModality::Keyboard;
        m_focusVisible = true;
    } else if (reason == FocusReason::Pointer) {
        m_inputModality = InputModality::Pointer;
        m_focusVisible = false;
    } else {
        m_focusVisible = (m_inputModality == InputModality::Keyboard);
    }
    ResetCursorBlink();
}

void Context::SetClipboardCallbacks(ClipboardGetFn getFn, ClipboardSetFn setFn) {
    m_clipboardGetFn = keel::move(getFn);
    m_clipboardSetFn = keel::move(setFn);
}

void Context::SetClipboardText(keel::StringRef text) {
    m_clipboardText.assign(text);
    if (m_clipboardSetFn) {
        m_clipboardSetFn(text);
    }
}

keel::String Context::GetClipboardText() const {
    if (m_clipboardGetFn) {
        keel::String ext = m_clipboardGetFn();
        if (!ext.empty() || m_clipboardText.empty()) {
            return ext;
        }
    }
    return m_clipboardText.clone();
}

bool Context::ActiveEditState::DeleteSelection() {
    if (!HasSelection() || readOnly) {
        return false;
    }
    SaveUndo();
    preferredColumn.reset();
    const keel::usize s = keel::min(SelectionStart(), text.size());
    const keel::usize e = keel::min(SelectionEnd(), text.size());
    if (e > s) {
        StrEraseRange(text, s, e - s);
        cursor = s;
        selectionAnchor = s;
        changed = true;
        return true;
    }
    selectionAnchor = cursor;
    return false;
}

void Context::ActiveEditState::Insert(keel::StringRef utf8Text) {
    if (readOnly || utf8Text.empty()) {
        return;
    }

    keel::String sanitized = keel::String::with_capacity(utf8Text.size());
    for (char c : utf8Text) {
        const keel::u8 u = static_cast<keel::u8>(c);
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            if (multiline) {
                sanitized.push_back('\n');
            }
            continue;
        }
        if (u < 32 && c != '\t') {
            continue;
        }
        sanitized.push_back(c);
    }

    if (sanitized.empty() && !HasSelection()) {
        return;
    }

    preferredColumn.reset();

    if (autoCloseBrackets && multiline && !readOnly && sanitized.size() == 1) {
        const char c = sanitized[0];
        const keel::usize safeCur = keel::min(cursor, text.size());

        // Overtype closing bracket/quote if cursor is right before it
        if (!HasSelection() && safeCur < text.size() &&
            (c == ')' || c == ']' || c == '}' || c == '"' || c == '\'') &&
            text[safeCur] == c) {
            cursor = safeCur + 1;
            selectionAnchor = cursor;
            return;
        }

        char closeChar = 0;
        if (c == '(') closeChar = ')';
        else if (c == '[') closeChar = ']';
        else if (c == '{') closeChar = '}';
        else if (c == '"') closeChar = '"';
        else if (c == '\'') closeChar = '\'';

        if (closeChar != 0) {
            if (HasSelection()) {
                SaveUndo();
                const keel::usize s = keel::min(SelectionStart(), text.size());
                const keel::usize e = keel::min(SelectionEnd(), text.size());
                StrInsertChar(text, e, closeChar);
                StrInsertChar(text, s, c);
                selectionAnchor = s + 1;
                cursor = e + 1;
                changed = true;
                return;
            } else {
                bool allowAutoClose = true;
                if (c == '\'' && safeCur > 0) {
                    const char prev = text[safeCur - 1];
                    const bool prevIdentOrClose =
                        (prev >= 'a' && prev <= 'z') ||
                        (prev >= 'A' && prev <= 'Z') ||
                        (prev >= '0' && prev <= '9') ||
                        prev == '_' || prev == ')' || prev == ']' || prev == '}' || prev == '.';
                    if (prevIdentOrClose) {
                        allowAutoClose = false; // e.g. MATLAB matrix transpose A' or A.'
                    }
                }
                if ((c == '"' || c == '\'') && safeCur > 0 && (text[safeCur - 1] == '\\' || text[safeCur - 1] == c)) {
                    allowAutoClose = false;
                }
                const bool nextOk = (safeCur == text.size()) ||
                                    text[safeCur] == '\n' || text[safeCur] == ' ' || text[safeCur] == '\t' ||
                                    text[safeCur] == ')' || text[safeCur] == ']' || text[safeCur] == '}' ||
                                    text[safeCur] == ',' || text[safeCur] == ';';
                if (allowAutoClose && nextOk) {
                    SaveUndo();
                    const char pairStr[2] = {c, closeChar};
                    StrInsert(text, safeCur, keel::StringRef(pairStr, 2));
                    cursor = safeCur + 1;
                    selectionAnchor = cursor;
                    changed = true;
                    return;
                }
            }
        }
    }

    SaveUndo();
    if (HasSelection()) {
        const keel::usize s = keel::min(SelectionStart(), text.size());
        const keel::usize e = keel::min(SelectionEnd(), text.size());
        if (e > s) {
            StrEraseRange(text, s, e - s);
        }
        cursor = s;
        selectionAnchor = s;
    }

    if (cursor > text.size()) {
        cursor = text.size();
    }
    if (!sanitized.empty()) {
        StrInsert(text, cursor, sanitized.as_ref());
        cursor += sanitized.size();
        selectionAnchor = cursor;
    }
    changed = true;
}

bool Context::ActiveEditState::HandleKey(Context& ctx, Key key, KeyModifiers mods, bool& outLostFocus) {
    const KeyAction action = ctx.ResolveAction(key, mods);
    const bool isVerticalMove =
        action == KeyAction::MoveUp || action == KeyAction::SelectUp ||
        action == KeyAction::MoveDown || action == KeyAction::SelectDown ||
        action == KeyAction::MovePageUp || action == KeyAction::SelectPageUp ||
        action == KeyAction::MovePageDown || action == KeyAction::SelectPageDown;
    if (!isVerticalMove) {
        preferredColumn.reset();
    }

    auto utf8OffsetAtColumn = [](keel::StringRef lineSv, keel::usize targetCodepointCol) noexcept -> keel::usize {
        keel::usize byteOff = 0;
        for (keel::usize colIdx = 0; colIdx < targetCodepointCol && byteOff < lineSv.size(); ++colIdx) {
            byteOff = keel::utf8_next_boundary(lineSv, byteOff);
        }
        return keel::min(byteOff, lineSv.size());
    };

    auto moveLinesVertically = [&](keel::i32 lineDelta, bool extendSel) -> bool {
        if (!multiline) return false;
        const keel::usize safeCur = keel::min(cursor, text.size());
        const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', safeCur);
        keel::usize curLineStart = (prevNl == keel::StringRef::npos) ? 0 : (prevNl + 1);
        const keel::usize col = keel::utf8_codepoint_count(
            text.substr(curLineStart, safeCur - curLineStart)
        );
        if (!preferredColumn.has_value()) {
            preferredColumn = col;
        }
        const keel::usize targetCol = *preferredColumn;

        if (lineDelta < 0) {
            const keel::i32 steps = -lineDelta;
            for (keel::i32 s = 0; s < steps; ++s) {
                if (curLineStart == 0) {
                    if (s == 0) {
                        cursor = 0;
                        if (!extendSel) selectionAnchor = cursor;
                        return true;
                    }
                    break;
                }
                const keel::usize prevLineEnd = curLineStart - 1;
                const keel::usize prevPrevNl = StrRfindBefore(text.as_ref(), '\n', prevLineEnd);
                curLineStart = (prevPrevNl == keel::StringRef::npos) ? 0 : (prevPrevNl + 1);
            }
            keel::usize lineEnd = text.find('\n', curLineStart);
            if (lineEnd == keel::StringRef::npos) lineEnd = text.size();
            const keel::StringRef targetLine = text.substr(curLineStart, lineEnd - curLineStart);
            cursor = curLineStart + utf8OffsetAtColumn(targetLine, targetCol);
        } else if (lineDelta > 0) {
            const keel::i32 steps = lineDelta;
            keel::usize curLineEnd = text.find('\n', curLineStart);
            for (keel::i32 s = 0; s < steps; ++s) {
                if (curLineEnd == keel::StringRef::npos) {
                    if (s == 0) {
                        cursor = text.size();
                        if (!extendSel) selectionAnchor = cursor;
                        return true;
                    }
                    break;
                }
                curLineStart = curLineEnd + 1;
                curLineEnd = text.find('\n', curLineStart);
            }
            const keel::usize lineEnd = (curLineEnd == keel::StringRef::npos) ? text.size() : curLineEnd;
            const keel::StringRef targetLine = text.substr(curLineStart, lineEnd - curLineStart);
            cursor = curLineStart + utf8OffsetAtColumn(targetLine, targetCol);
        }

        if (!extendSel) {
            selectionAnchor = cursor;
        }
        return true;
    };

    switch (action) {
        case KeyAction::MoveLineUp:
        case KeyAction::MoveLineDown: {
            if (!multiline || readOnly) return false;
            const keel::usize safeCur = keel::min(cursor, text.size());
            const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', safeCur);
            const keel::usize curLineStart = (prevNl == keel::StringRef::npos) ? 0 : (prevNl + 1);
            keel::usize curLineEnd = text.find('\n', safeCur);
            if (curLineEnd == keel::StringRef::npos) curLineEnd = text.size();
            const keel::usize col = safeCur - curLineStart;

            if (action == KeyAction::MoveLineUp) {
                if (curLineStart > 0) {
                    SaveUndo();
                    const keel::usize prevLineEnd = curLineStart - 1;
                    const keel::usize prevPrevNl = StrRfindBefore(text.as_ref(), '\n', prevLineEnd);
                    const keel::usize prevLineStart = (prevPrevNl == keel::StringRef::npos) ? 0 : (prevPrevNl + 1);
                    keel::String curLine = keel::String::from(text.substr(curLineStart, curLineEnd - curLineStart));
                    keel::String prevLine = keel::String::from(text.substr(prevLineStart, prevLineEnd - prevLineStart));
                    keel::String swapped = keel::String::with_capacity(curLine.size() + 1 + prevLine.size());
                    swapped.append(curLine.as_ref());
                    swapped.push_back('\n');
                    swapped.append(prevLine.as_ref());
                    StrReplace(text, prevLineStart, curLineEnd - prevLineStart, swapped.as_ref());
                    cursor = prevLineStart + keel::min(col, curLine.size());
                    selectionAnchor = cursor;
                    changed = true;
                }
            } else {
                if (curLineEnd < text.size()) {
                    SaveUndo();
                    const keel::usize nextLineStart = curLineEnd + 1;
                    keel::usize nextLineEnd = text.find('\n', nextLineStart);
                    if (nextLineEnd == keel::StringRef::npos) nextLineEnd = text.size();
                    keel::String curLine = keel::String::from(text.substr(curLineStart, curLineEnd - curLineStart));
                    keel::String nextLine = keel::String::from(text.substr(nextLineStart, nextLineEnd - nextLineStart));
                    keel::String swapped = keel::String::with_capacity(nextLine.size() + 1 + curLine.size());
                    swapped.append(nextLine.as_ref());
                    swapped.push_back('\n');
                    swapped.append(curLine.as_ref());
                    StrReplace(text, curLineStart, nextLineEnd - curLineStart, swapped.as_ref());
                    cursor = curLineStart + nextLine.size() + 1 + keel::min(col, curLine.size());
                    selectionAnchor = cursor;
                    changed = true;
                }
            }
            return true;
        }

        case KeyAction::SelectAll:
            SelectAll();
            return true;

        case KeyAction::Copy:
            if (HasSelection()) {
                ctx.SetClipboardText(SelectedText());
            }
            return true;

        case KeyAction::Cut:
            if (HasSelection()) {
                ctx.SetClipboardText(SelectedText());
                if (!readOnly) {
                    DeleteSelection();
                }
            }
            return true;

        case KeyAction::Paste:
            if (!readOnly) {
                const keel::String clip = ctx.GetClipboardText();
                if (!clip.empty()) {
                    Insert(clip.as_ref());
                }
            }
            return true;

        case KeyAction::Undo:
            if (!readOnly) {
                Undo();
            }
            return true;

        case KeyAction::Redo:
            if (!readOnly) {
                Redo();
            }
            return true;

        case KeyAction::DuplicateLine: {
            if (!multiline || readOnly) return false;
            if (HasSelection()) {
                const keel::String sel = keel::String::from(SelectedText());
                if (!sel.empty()) {
                    keel::usize nextPos = text.find(sel.as_ref(), SelectionEnd());
                    if (nextPos == keel::StringRef::npos) {
                        nextPos = text.find(sel.as_ref(), 0);
                    }
                    if (nextPos != keel::StringRef::npos && nextPos != SelectionStart()) {
                        selectionAnchor = nextPos;
                        cursor = nextPos + sel.size();
                        return true;
                    }
                }
            }
            const keel::usize safeCur = keel::min(cursor, text.size());
            const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', safeCur);
            const keel::usize curLineStart = (prevNl == keel::StringRef::npos) ? 0 : (prevNl + 1);
            keel::usize curLineEnd = text.find('\n', safeCur);
            if (curLineEnd == keel::StringRef::npos) curLineEnd = text.size();
            const keel::usize col = safeCur - curLineStart;
            keel::String dup = keel::String::with_capacity(1 + (curLineEnd - curLineStart));
            dup.push_back('\n');
            dup.append(text.substr(curLineStart, curLineEnd - curLineStart));
            SaveUndo();
            StrInsert(text, curLineEnd, dup.as_ref());
            cursor = curLineEnd + 1 + col;
            selectionAnchor = cursor;
            changed = true;
            return true;
        }

        case KeyAction::DeleteLine: {
            if (!multiline || readOnly) return false;
            SaveUndo();
            const keel::usize safeCur = keel::min(cursor, text.size());
            const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', safeCur);
            const keel::usize curLineStart = (prevNl == keel::StringRef::npos) ? 0 : (prevNl + 1);
            keel::usize curLineEnd = text.find('\n', safeCur);
            if (curLineEnd != keel::StringRef::npos) {
                StrEraseRange(text, curLineStart, curLineEnd - curLineStart + 1);
                cursor = keel::min(curLineStart, text.size());
            } else if (curLineStart > 0) {
                StrEraseRange(text, curLineStart - 1, text.size() - (curLineStart - 1));
                cursor = text.size();
            } else {
                text.clear();
                cursor = 0;
            }
            selectionAnchor = cursor;
            changed = true;
            return true;
        }

        case KeyAction::DeletePrevChar:
        case KeyAction::DeletePrevWord: {
            if (readOnly) return true;
            const bool byWord = (action == KeyAction::DeletePrevWord);
            if (HasSelection()) {
                DeleteSelection();
            } else if (cursor > 0 && !text.empty()) {
                SaveUndo();
                const keel::usize safeCur = keel::min(cursor, text.size());
                if (autoCloseBrackets && multiline && !byWord && safeCur < text.size()) {
                    const char prevC = text[safeCur - 1];
                    const char nextC = text[safeCur];
                    if ((prevC == '(' && nextC == ')') ||
                        (prevC == '[' && nextC == ']') ||
                        (prevC == '{' && nextC == '}') ||
                        (prevC == '"' && nextC == '"') ||
                        (prevC == '\'' && nextC == '\'')) {
                        StrEraseRange(text, safeCur - 1, 2);
                        cursor = safeCur - 1;
                        selectionAnchor = cursor;
                        changed = true;
                        return true;
                    }
                }
                const keel::usize prev = byWord
                    ? TextUtil::PrevWordBoundary(text.as_ref(), safeCur)
                    : TextUtil::PrevCharBoundary(text.as_ref(), safeCur);
                if (safeCur > prev) {
                    StrEraseRange(text, prev, safeCur - prev);
                    cursor = prev;
                    selectionAnchor = cursor;
                    changed = true;
                }
            } else {
                return false;
            }
            return true;
        }

        case KeyAction::DeleteNextChar:
        case KeyAction::DeleteNextWord: {
            if (readOnly) return true;
            const bool byWord = (action == KeyAction::DeleteNextWord);
            if (HasSelection()) {
                DeleteSelection();
            } else if (cursor < text.size()) {
                SaveUndo();
                const keel::usize next = byWord
                    ? TextUtil::NextWordBoundary(text.as_ref(), cursor)
                    : TextUtil::NextCharBoundary(text.as_ref(), cursor);
                if (next > cursor) {
                    StrEraseRange(text, cursor, next - cursor);
                    selectionAnchor = cursor;
                    changed = true;
                }
            }
            return true;
        }

        case KeyAction::MoveLeft:
        case KeyAction::SelectLeft:
        case KeyAction::MoveWordLeft:
        case KeyAction::SelectWordLeft: {
            const bool extendSel = (action == KeyAction::SelectLeft || action == KeyAction::SelectWordLeft);
            const bool byWord = (action == KeyAction::MoveWordLeft || action == KeyAction::SelectWordLeft);
            if (!extendSel && HasSelection()) {
                if (byWord) {
                    cursor = TextUtil::PrevWordBoundary(text.as_ref(), SelectionStart());
                } else {
                    cursor = SelectionStart();
                }
                selectionAnchor = cursor;
            } else {
                cursor = byWord
                    ? TextUtil::PrevWordBoundary(text.as_ref(), cursor)
                    : TextUtil::PrevCharBoundary(text.as_ref(), cursor);
                if (!extendSel) {
                    selectionAnchor = cursor;
                }
            }
            return true;
        }

        case KeyAction::MoveRight:
        case KeyAction::SelectRight:
        case KeyAction::MoveWordRight:
        case KeyAction::SelectWordRight: {
            const bool extendSel = (action == KeyAction::SelectRight || action == KeyAction::SelectWordRight);
            const bool byWord = (action == KeyAction::MoveWordRight || action == KeyAction::SelectWordRight);
            if (!extendSel && HasSelection()) {
                if (byWord) {
                    cursor = TextUtil::NextWordBoundary(text.as_ref(), SelectionEnd());
                } else {
                    cursor = SelectionEnd();
                }
                selectionAnchor = cursor;
            } else {
                cursor = byWord
                    ? TextUtil::NextWordBoundary(text.as_ref(), cursor)
                    : TextUtil::NextCharBoundary(text.as_ref(), cursor);
                if (!extendSel) {
                    selectionAnchor = cursor;
                }
            }
            return true;
        }

        case KeyAction::MoveUp:
            return moveLinesVertically(-1, false);
        case KeyAction::SelectUp:
            return moveLinesVertically(-1, true);
        case KeyAction::MoveDown:
            return moveLinesVertically(1, false);
        case KeyAction::SelectDown:
            return moveLinesVertically(1, true);
        case KeyAction::MovePageUp:
            return moveLinesVertically(-20, false);
        case KeyAction::SelectPageUp:
            return moveLinesVertically(-20, true);
        case KeyAction::MovePageDown:
            return moveLinesVertically(20, false);
        case KeyAction::SelectPageDown:
            return moveLinesVertically(20, true);

        case KeyAction::MoveLineStart:
        case KeyAction::SelectLineStart:
        case KeyAction::MoveDocStart:
        case KeyAction::SelectDocStart: {
            const bool extendSel = (action == KeyAction::SelectLineStart || action == KeyAction::SelectDocStart);
            const bool docStart = (action == KeyAction::MoveDocStart || action == KeyAction::SelectDocStart);
            if (multiline && !docStart) {
                const keel::usize safeCur = keel::min(cursor, text.size());
                const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', safeCur);
                const keel::usize lineStart = (prevNl == keel::StringRef::npos) ? 0 : prevNl + 1;
                keel::usize firstNonWs = lineStart;
                while (firstNonWs < text.size() && (text[firstNonWs] == ' ' || text[firstNonWs] == '\t')) {
                    ++firstNonWs;
                }
                if (safeCur == lineStart && firstNonWs > lineStart && autoCloseBrackets) {
                    cursor = firstNonWs;
                } else if (safeCur > firstNonWs && firstNonWs > lineStart && autoCloseBrackets) {
                    cursor = firstNonWs;
                } else {
                    cursor = lineStart;
                }
            } else {
                cursor = 0;
            }
            if (!extendSel) {
                selectionAnchor = cursor;
            }
            return true;
        }

        case KeyAction::MoveLineEnd:
        case KeyAction::SelectLineEnd:
        case KeyAction::MoveDocEnd:
        case KeyAction::SelectDocEnd: {
            const bool extendSel = (action == KeyAction::SelectLineEnd || action == KeyAction::SelectDocEnd);
            const bool docEnd = (action == KeyAction::MoveDocEnd || action == KeyAction::SelectDocEnd);
            if (multiline && !docEnd) {
                const keel::usize nextNl = text.find('\n', cursor);
                cursor = (nextNl == keel::StringRef::npos) ? text.size() : nextNl;
            } else {
                cursor = text.size();
            }
            if (!extendSel) {
                selectionAnchor = cursor;
            }
            return true;
        }

        case KeyAction::Indent:
        case KeyAction::Unindent: {
            if (multiline && !readOnly) {
                const keel::usize stepCols = keel::clamp<keel::usize>(tabSize, 1, 16);
                keel::FixedString<20> indentUnit;
                if (insertSpaces) {
                    for (keel::usize i = 0; i < stepCols; ++i) indentUnit.push_back(' ');
                } else {
                    indentUnit.push_back('\t');
                }
                const keel::usize unitLen = indentUnit.size();
                const bool unindent = (action == KeyAction::Unindent);
                const bool multiLineSel = HasSelection() && (SelectedText().find('\n') != keel::StringRef::npos);
                if (unindent || multiLineSel) {
                    const keel::usize selS = keel::min(SelectionStart(), text.size());
                    keel::usize selE = keel::min(SelectionEnd(), text.size());
                    if (selE > selS && text[selE - 1] == '\n') {
                        --selE;
                    }
                    const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', selS);
                    const keel::usize blockStart = (prevNl == keel::StringRef::npos) ? 0 : (prevNl + 1);
                    keel::usize blockEnd = text.find('\n', selE);
                    if (blockEnd == keel::StringRef::npos) blockEnd = text.size();

                    SaveUndo();
                    keel::usize pos = blockStart;
                    bool anyModified = false;
                    while (pos <= blockEnd && pos <= text.size()) {
                        keel::usize lineEnd = text.find('\n', pos);
                        if (lineEnd == keel::StringRef::npos) lineEnd = text.size();

                        if (!unindent) {
                            StrInsert(text, pos, indentUnit.as_ref());
                            if (cursor >= pos) cursor += unitLen;
                            if (selectionAnchor >= pos) selectionAnchor += unitLen;
                            blockEnd += unitLen;
                            lineEnd += unitLen;
                            anyModified = true;
                        } else {
                            keel::usize rem = 0;
                            if (pos < lineEnd && text[pos] == '\t') {
                                rem = 1;
                            } else {
                                while (rem < stepCols && (pos + rem) < lineEnd && text[pos + rem] == ' ') {
                                    ++rem;
                                }
                            }
                            if (rem > 0) {
                                StrEraseRange(text, pos, rem);
                                if (cursor > pos) {
                                    cursor = (cursor >= pos + rem) ? (cursor - rem) : pos;
                                }
                                if (selectionAnchor > pos) {
                                    selectionAnchor = (selectionAnchor >= pos + rem) ? (selectionAnchor - rem) : pos;
                                }
                                blockEnd -= rem;
                                lineEnd -= rem;
                                anyModified = true;
                            }
                        }

                        if (lineEnd >= blockEnd || lineEnd >= text.size()) {
                            break;
                        }
                        pos = lineEnd + 1;
                    }
                    if (anyModified) {
                        changed = true;
                    }
                    return true;
                }
                Insert(indentUnit.as_ref());
                return true;
            }
            return false;
        }

        case KeyAction::Confirm:
            if (multiline) {
                if (!readOnly) {
                    const keel::usize stepCols = keel::clamp<keel::usize>(tabSize, 1, 16);
                    keel::FixedString<20> indentUnit;
                    if (insertSpaces) {
                        for (keel::usize i = 0; i < stepCols; ++i) indentUnit.push_back(' ');
                    } else {
                        indentUnit.push_back('\t');
                    }
                    const keel::usize safeCur = keel::min(cursor, text.size());
                    const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', safeCur);
                    const keel::usize curLineStart = (prevNl == keel::StringRef::npos) ? 0 : (prevNl + 1);
                    keel::String indent;
                    for (keel::usize i = curLineStart; i < safeCur && (text[i] == ' ' || text[i] == '\t'); ++i) {
                        indent.push_back(text[i]);
                    }
                    if (autoCloseBrackets && !HasSelection() && safeCur > 0 && safeCur < text.size()) {
                        const char prevC = text[safeCur - 1];
                        const char nextC = text[safeCur];
                        if ((prevC == '{' && nextC == '}') ||
                            (prevC == '[' && nextC == ']') ||
                            (prevC == '(' && nextC == ')')) {
                            SaveUndo();
                            keel::String mid = keel::String::with_capacity(1 + indent.size() + indentUnit.size());
                            mid.push_back('\n');
                            mid.append(indent.as_ref());
                            mid.append(indentUnit.as_ref());
                            keel::String full = keel::String::with_capacity(mid.size() + 1 + indent.size());
                            full.append(mid.as_ref());
                            full.push_back('\n');
                            full.append(indent.as_ref());
                            StrInsert(text, safeCur, full.as_ref());
                            cursor = safeCur + mid.size();
                            selectionAnchor = cursor;
                            changed = true;
                            return true;
                        }
                    }
                    if (autoCloseBrackets && !HasSelection() && safeCur > curLineStart) {
                        keel::usize checkPos = safeCur;
                        while (checkPos > curLineStart && (text[checkPos - 1] == ' ' || text[checkPos - 1] == '\t')) {
                            --checkPos;
                        }
                        if (checkPos > curLineStart && (text[checkPos - 1] == '{' || text[checkPos - 1] == '[' || text[checkPos - 1] == '(')) {
                            indent.append(indentUnit.as_ref());
                        }
                    }
                    keel::String nlIndent = keel::String::with_capacity(1 + indent.size());
                    nlIndent.push_back('\n');
                    nlIndent.append(indent.as_ref());
                    Insert(nlIndent.as_ref());
                }
                return true;
            }
            submitted = true;
            outLostFocus = true;
            return true;

        case KeyAction::Cancel:
            if (HasSelection()) {
                selectionAnchor = cursor;
                return true;
            }
            ctx.FlushActiveEditHistory();
            outLostFocus = true;
            id = Id::Null();
            return true;

        default:
            return false;
    }
}

void Context::RequestRepaint(keel::i32 frames) noexcept {
    if (frames <= 0) return;
    const keel::i32 target = (!m_containerStack.empty() && frames == 1) ? 2 : frames;
    m_repaintFrames = keel::max(m_repaintFrames, target);
}

bool Context::NeedsRepaint() const noexcept {
    if (m_repaintFrames > 0) return true;
    if (m_mouseDown || m_rightMouseDown || m_middleMouseDown) return true;
    if (m_activeId.IsValid()) return true;
    for (const auto& t : m_toasts) {
        if (t.duration > 0.0f) return true;
    }
    return false;
}

keel::i32 Context::GetRecommendedSleepTimeoutMs() const noexcept {
    if (NeedsRepaint()) {
        return 0;
    }
    if (m_editState.id.IsValid() && m_cursorConfig.mode == CursorBlinkMode::Blinking) {
        return 120; // Periodically wake up for smooth cursor blinking
    }
    if (m_hasUntrimmedInactiveStates && (m_idleHeartbeatMs <= 0 || m_idleHeartbeatMs > 500)) {
        return 500;
    }
    return m_idleHeartbeatMs; // User-configurable heartbeat interval (or <= 0 for deep sleep)
}

void Context::OnMouseMove(Vec2 pos) {
    const float scale = m_pixelsPerDp;
    const Vec2 newPos = pos / scale;
    m_mousePosPx = pos;
    if (keel::abs(newPos.x - m_mousePos.x) < 0.25f && keel::abs(newPos.y - m_mousePos.y) < 0.25f) {
        return;
    }
    m_mousePos = newPos;
    RequestRepaint(1);
    if (m_inspectModeEnabled || m_anyScopedInspectActive) {
        InvalidateAll();
    }
}

void Context::OnMouseDown(keel::i32 button) {
    if (button == 1 || button == 2 || button == 3) {
        m_inputModality = InputModality::Pointer;
        m_focusVisible = false;
    }
    if (button == 1) { // Left mouse button
        m_mouseDown = true;
        m_mouseClicked = true;
        const double now = m_currentTimeSec;
        const float dx = m_mousePos.x - m_lastClickPos.x;
        const float dy = m_mousePos.y - m_lastClickPos.y;
        if (m_lastClickTimeSec >= 0.0 && (now - m_lastClickTimeSec) <= 0.36 && (dx * dx + dy * dy) <= 64.0f) {
            m_mouseDoubleClicked = true;
            m_lastClickTimeSec = -1.0;
        } else {
            m_lastClickTimeSec = now;
            m_lastClickPos = m_mousePos;
        }
        ResetCursorBlink();
        RequestRepaint(3);
    } else if (button == 2) { // Middle mouse button
        m_middleMouseDown = true;
        m_middleMouseClicked = true;
        RequestRepaint(3);
    } else if (button == 3) { // Right mouse button
        m_rightMouseDown = true;
        m_rightMouseClicked = true;
        RequestRepaint(3);
    }
}

void Context::OnMouseUp(keel::i32 button) {
    if (button == 1) { // Left mouse button
        m_mouseDown = false;
        m_mouseReleased = true;
        RequestRepaint(3);
    } else if (button == 2) { // Middle mouse button
        m_middleMouseDown = false;
        m_middleMouseReleased = true;
        RequestRepaint(3);
    } else if (button == 3) { // Right mouse button
        m_rightMouseDown = false;
        m_rightMouseReleased = true;
        RequestRepaint(3);
    }
}

void Context::OnMouseWheel(float x, float y) {
    m_mouseWheel = {x, y};
    RequestRepaint(3);
}

bool Context::IsDirectEditAllowed() const noexcept {
    if (m_suspendDirectEditInput || m_mouseClicked || !m_focusedId.IsValid() || m_editState.id != m_focusedId) {
        return false;
    }
    if (m_openPopupId.IsValid() && m_openPopupId != m_focusedId) {
        return false;
    }
    if (m_prevFrameModalId.IsValid()) {
        if (!ContainsId(m_prevModalFocusableIds, m_focusedId)) {
            return false;
        }
    }
    return true;
}

void Context::OnTextInput(keel::StringRef utf8Text) {
    ResetCursorBlink();
    RequestRepaint(3);
    if (IsDirectEditAllowed()) {
        m_editState.Insert(utf8Text);
    } else {
        m_pendingTextInput.push_back(keel::String::from(utf8Text));
    }
}

static bool IsKeyboardFocusTriggerKey(Key key, KeyModifiers mods) noexcept {
    if (mods.ctrl || mods.alt) {
        return false;
    }
    switch (key) {
        case Key::Tab:
        case Key::Enter:
        case Key::Space:
        case Key::Left:
        case Key::Right:
        case Key::Up:
        case Key::Down:
        case Key::Home:
        case Key::End:
        case Key::PageUp:
        case Key::PageDown:
            return true;
        default:
            return false;
    }
}

void Context::OnKeyDown(Key key, KeyModifiers mods) {
    m_keyMods = mods;
    if (IsKeyboardFocusTriggerKey(key, mods)) {
        m_inputModality = InputModality::Keyboard;
        if (m_focusedId.IsValid()) {
            m_focusVisible = true;
        }
    }
    ResetCursorBlink();
    RequestRepaint(3);

    if (m_keymap.Matches(KeyAction::Cancel, key, mods) && m_openPopupId.IsValid()) {
        ClosePopup();
        return;
    }

    if (IsDirectEditAllowed()) {
        bool lostFocus = false;
        if (m_editState.HandleKey(*this, key, mods, lostFocus)) {
            if (lostFocus) {
                m_focusedId = Id::Null();
            }
            return;
        }
    }
    m_pendingKeyEvents.push_back({key, mods});
}

static Rect ExpandPopupDamageRect(Rect r) noexcept {
    if (r.w <= 0.0f || r.h <= 0.0f) return Rect{};
    return Rect(r.x - 12.0f, r.y - 12.0f, r.w + 24.0f, r.h + 32.0f);
}

void Context::OpenPopup(Id id, Rect bounds, Rect anchorRect) {
    const Rect frameBounds = CurrentSpace().Apply(bounds);
    const Rect anchorFrameBounds = (anchorRect.w > 0.0f && anchorRect.h > 0.0f)
        ? CurrentSpace().Apply(anchorRect)
        : Rect{};
    if (m_openPopupId != id || m_openPopupRect != frameBounds || m_openPopupAnchorRect != anchorFrameBounds) {
        if (m_openPopupId.IsValid() && m_openPopupRect.w > 0.0f && m_openPopupRect.h > 0.0f) {
            InvalidateFrameRect(ExpandPopupDamageRect(m_openPopupRect));
        }
        m_openPopupId = id;
        m_openPopupRect = frameBounds;
        m_openPopupAnchorRect = anchorFrameBounds;
        InvalidateFrameRect(ExpandPopupDamageRect(m_openPopupRect));
    }
}

void Context::ClosePopup() {
    if (m_openPopupId.IsValid()) {
        const Id closedId = m_openPopupId;
        if (m_openPopupRect.w > 0.0f && m_openPopupRect.h > 0.0f) {
            InvalidateFrameRect(ExpandPopupDamageRect(m_openPopupRect));
        }
        m_openPopupId = Id::Null();
        m_openPopupRect = Rect{};
        m_openPopupAnchorRect = Rect{};
        m_openPopupVisualHash = 0;
        ArrayEraseIf(m_prevOverlayHitRegions, [closedId](const OverlayHitRegion& r) { return r.id == closedId; });
        ArrayEraseIf(m_currOverlayHitRegions, [closedId](const OverlayHitRegion& r) { return r.id == closedId; });
        RequestRepaint(2);
    }
}

void Context::InvalidateAll() noexcept {
    m_forceFullRepaint = true;
    RequestRepaint(2);
}

void Context::InvalidateRect(Rect rect) noexcept {
    InvalidateFrameRect(CurrentSpace().Apply(rect));
}

void Context::InvalidateFrameRect(Rect frameRect) noexcept {
    if (frameRect.w > 0.0f && frameRect.h > 0.0f) {
        m_explicitDamageRects.push_back(frameRect);
        RequestRepaint(2);
    }
}

void Context::RegisterOverlayHitRegion(Id id, Rect rect) {
    if (id.IsValid() && rect.w > 0.0f && rect.h > 0.0f) {
        m_currOverlayHitRegions.push_back(OverlayHitRegion{id, CurrentSpace().Apply(rect)});
    }
}

bool Context::IsMouseOccluded(Id id, Vec2 pos) const noexcept {
    return IsMouseOccludedInFrame(id, CurrentSpace().Apply(pos));
}

bool Context::IsMouseOccludedInFrame(Id id, Vec2 pos) const noexcept {
    if (m_openPopupId.IsValid() && id != m_openPopupId && m_openPopupRect.Contains(pos)) {
        return true;
    }
    if (IsModalBlockingInFrame(pos)) {
        return true;
    }
    if (m_activeId.IsValid() && m_activeId == id) {
        return false;
    }
    for (const auto& ov : m_prevOverlayHitRegions) {
        if (m_activeId.IsValid() && m_activeId == ov.id) {
            return ov.id != id;
        }
        if (ov.rect.Contains(pos)) {
            return ov.id != id;
        }
    }

    const keel::usize* idxA = m_prevHitNodeMap.get(id);
    if (idxA != nullptr) {
        const HitNodeRecord& a = m_prevHitNodes[*idxA];
        for (const auto& b : m_prevHitNodes) {
            if (!b.blocksHit || b.id == id || !b.rect.Contains(pos)) {
                continue;
            }
            // Skip if B is an ancestor of A
            if (b.preIndex < a.preIndex && b.postIndex >= a.postIndex) {
                continue;
            }
            const bool bIsDescendantOfA = (a.preIndex < b.preIndex && b.postIndex <= a.postIndex);
            if (bIsDescendantOfA) {
                // Child panels/widgets placed inside a Scene/MapView occlude the Scene canvas
                if (a.type == NodeType::Scene) {
                    return true;
                }
                continue;
            }
            // Cross-layer occlusion (e.g. Sidebar, BottomPanel, ActivityBar, StatusBar, Modal over workspace)
            if (b.zLayer > a.zLayer) {
                return true;
            }
            // Same-layer occlusion when A is a Scene and B is rendered after A
            if (b.zLayer == a.zLayer && a.type == NodeType::Scene && b.preIndex > a.postIndex) {
                return true;
            }
        }
    } else {
        // Sub-part ID (e.g. AccordionHeader / TreeItemRow): check against innermost container on stack
        for (keel::usize sIdx = m_containerStack.size(); sIdx > 0; --sIdx) {
            const Node* cNode = m_containerStack[sIdx - 1];
            if (!cNode || !cNode->id.IsValid()) continue;
            const keel::usize* idxC = m_prevHitNodeMap.get(cNode->id);
            if (idxC != nullptr) {
                const HitNodeRecord& c = m_prevHitNodes[*idxC];
                for (const auto& b : m_prevHitNodes) {
                    if (!b.blocksHit || b.id == c.id || !b.rect.Contains(pos)) {
                        continue;
                    }
                    // Skip if B is an ancestor of C
                    if (b.preIndex < c.preIndex && b.postIndex >= c.postIndex) {
                        continue;
                    }
                    const bool bIsDescendantOfC = (c.preIndex < b.preIndex && b.postIndex <= c.postIndex);
                    if (bIsDescendantOfC) {
                        if (c.type == NodeType::Scene) {
                            return true;
                        }
                        continue;
                    }
                    if (b.zLayer > c.zLayer) {
                        return true;
                    }
                    if (b.zLayer == c.zLayer && c.type == NodeType::Scene && b.preIndex > c.postIndex) {
                        return true;
                    }
                }
                break;
            }
        }
    }
    return false;
}

bool Context::IsRectHovered(Id id, const Rect& rect) const noexcept {
    if (!CurrentSpace().Apply(rect).Contains(m_mousePos)) {
        return false;
    }
    if (id.IsValid()) {
        const Rect* prevRect = m_prevFrameBounds.get(id);
        if (prevRect != nullptr && !prevRect->Contains(m_mousePos)) {
            return false;
        }
    }
    if (IsMouseOccludedInFrame(id, m_mousePos)) {
        return false;
    }
    return true;
}

bool Context::IsMouseBlockedForCurrentScope() const noexcept {
    if (m_activeOverlayHitCheckId.IsValid()) {
        if (m_openPopupId.IsValid() && m_openPopupId != m_activeOverlayHitCheckId && m_openPopupRect.Contains(m_mousePos)) {
            return true;
        }
        if (m_activeOverlayHitCheckId != m_prevFrameModalId &&
            m_activeOverlayHitCheckId != m_currFrameModalId &&
            IsModalBlockingInFrame(m_mousePos)) {
            return true;
        }
        return false;
    }

    if (m_openPopupId.IsValid() && m_openPopupRect.Contains(m_mousePos)) {
        return true;
    }
    if (IsModalBlockingInFrame(m_mousePos)) {
        return true;
    }

    const bool clickImpulse = m_mouseClicked || m_rightMouseClicked || m_middleMouseClicked;

    // If an overlay hit region (splitter sash / toggle pill) is currently being dragged,
    // block all non-overlay scopes
    for (const auto& ov : m_prevOverlayHitRegions) {
        if (m_activeId.IsValid() && m_activeId == ov.id) {
            return true;
        }
    }

    const bool hasActiveCapture = m_activeId.IsValid() && !clickImpulse;

    // Block non-overlay scopes when hovering/clicking an overlay hit region
    // unless a non-overlay widget/handle already owns active drag capture
    if (!hasActiveCapture) {
        for (const auto& ov : m_prevOverlayHitRegions) {
            if (ov.rect.Contains(m_mousePos)) {
                return true;
            }
        }
    }

    // Determine current scope Z-layer from container stack
    keel::u8 currentZLayer = (m_modalScopeDepth > 0) ? 2 : 0;
    for (const Node* n : m_containerStack) {
        if (!n) continue;
        if (n->IsOutOfFlow()) {
            currentZLayer = keel::max<keel::u8>(currentZLayer, 2);
        } else if (n->type == NodeType::Sidebar ||
                   n->type == NodeType::BottomPanel ||
                   n->type == NodeType::StatusBar) {
            currentZLayer = keel::max<keel::u8>(currentZLayer, 1);
        }
    }

    // Block lower Z-layer scopes if the mouse is inside a higher Z-layer panel/overlay (e.g. Sidebar, Popover, Modal),
    // unless an active drag capture is already in progress
    if (currentZLayer < 2 && !hasActiveCapture) {
        for (const auto& b : m_prevHitNodes) {
            if (b.blocksHit && b.zLayer > currentZLayer && b.rect.Contains(m_mousePos)) {
                return true;
            }
        }
    }

    // If user code is executing directly inside a Scene / MapView scope (not inside a child container or widget Run()),
    // gate mouse queries to unoccluded points inside the Scene viewport (and not over Scene HUD/scrollbars)
    if (m_widgetExecDepth == 0 && !m_containerStack.empty()) {
        const Node* topContainer = m_containerStack.back();
        if (topContainer && topContainer->type == NodeType::Scene && topContainer->id.IsValid()) {
            const Id sceneId = topContainer->id;
            if (hasActiveCapture && m_activeId != sceneId) {
                return true;
            }
            if (IsMouseOccludedInFrame(sceneId, m_mousePos)) {
                return true;
            }
            if (topContainer->scrollbarActive || topContainer->scrollbarActiveX) {
                return true;
            }
            if ((topContainer->scrollbarHovered || topContainer->scrollbarHoveredX) && m_activeId != sceneId) {
                return true;
            }
            const Rect* vpRect = m_prevFrameBounds.get(sceneId);
            if (vpRect != nullptr && vpRect->w > 0.1f && vpRect->h > 0.1f) {
                if (!vpRect->Contains(m_mousePos) && m_activeId != sceneId) {
                    return true;
                }
            }
        }
    }

    return false;
}

void Context::SetTooltip(UiText tooltip, Rect anchorRect) {
    if (tooltip.empty() || m_openPopupId.IsValid() || IsModalBlockingInFrame(m_mousePos)) {
        return;
    }
    m_activeTooltip = AllocText(tooltip);
    m_activeTooltipAnchor = CurrentSpace().Apply(anchorRect);
    m_activeTooltipViewport = PopupViewportInFrame();
}

void Context::RegisterFocusable(Id id, bool disabled) {
    if (!disabled && id.IsValid()) {
        if (m_modalScopeDepth > 0) {
            m_currModalFocusableIds.push_back(id);
        }
        m_currFocusableIds.push_back(id);
    }
}

bool Context::ConsumeKey(Key key) {
    for (keel::usize i = 0; i < m_pendingKeyEvents.size(); ++i) {
        if (m_pendingKeyEvents[i].key == key) {
            m_pendingKeyEvents.remove_at(i);
            return true;
        }
    }
    return false;
}

bool Context::ConsumeKey(Key key, KeyModifiers& outMods) {
    for (keel::usize i = 0; i < m_pendingKeyEvents.size(); ++i) {
        if (m_pendingKeyEvents[i].key == key) {
            outMods = m_pendingKeyEvents[i].mods;
            m_pendingKeyEvents.remove_at(i);
            return true;
        }
    }
    return false;
}

bool Context::ConsumeShortcut(Key key, bool ctrl, bool shift, bool alt) {
    for (keel::usize i = 0; i < m_pendingKeyEvents.size(); ++i) {
        const auto& ev = m_pendingKeyEvents[i];
        if (ev.key == key && ev.mods.ctrl == ctrl && ev.mods.shift == shift && ev.mods.alt == alt) {
            m_pendingKeyEvents.remove_at(i);
            return true;
        }
    }
    return false;
}

bool Context::ConsumeAction(KeyAction action) {
    if (action == KeyAction::None) {
        return false;
    }
    for (keel::usize i = 0; i < m_pendingKeyEvents.size(); ++i) {
        const auto& ev = m_pendingKeyEvents[i];
        if (m_keymap.Matches(action, ev.key, ev.mods)) {
            m_pendingKeyEvents.remove_at(i);
            return true;
        }
    }
    return false;
}

keel::Array<keel::String> Context::ConsumePendingTextInput() {
    keel::Array<keel::String> res = keel::move(m_pendingTextInput);
    m_pendingTextInput.clear();
    return res;
}

void Context::FlushActiveEditHistory() noexcept {
    if (!m_editState.id.IsValid()) {
        return;
    }
    if (!m_editState.undoStack.empty() || !m_editState.redoStack.empty()) {
        TextEditHistory* hist = m_editHistories.get(m_editState.id);
        if (!hist) {
            m_editHistories.put(m_editState.id, TextEditHistory{});
            hist = m_editHistories.get(m_editState.id);
        }
        if (hist) {
            hist->lastText.assign(m_editState.text.as_ref());
            hist->undoStack = keel::move(m_editState.undoStack);
            hist->redoStack = keel::move(m_editState.redoStack);
        }
        m_editState.undoStack.clear();
        m_editState.redoStack.clear();
    } else if (TextEditHistory* existing = m_editHistories.get(m_editState.id)) {
        existing->lastText.assign(m_editState.text.as_ref());
        existing->undoStack.clear();
        existing->redoStack.clear();
    }
}

keel::StringRef Context::EnsureTextEdit(
    Id id,
    keel::StringRef initialText,
    bool multiline,
    bool readOnly
) {
    if (m_editState.id != id) {
        FlushActiveEditHistory();
        m_editState.id = id;
        m_editState.text.assign(initialText);
        m_editState.cursor = m_editState.text.size();
        m_editState.selectionAnchor = m_editState.cursor;
        m_editState.undoStack.clear();
        m_editState.redoStack.clear();
        if (id.IsValid()) {
            if (TextEditHistory* hist = m_editHistories.get(id)) {
                if (hist->lastText == initialText) {
                    m_editState.undoStack = keel::move(hist->undoStack);
                    m_editState.redoStack = keel::move(hist->redoStack);
                } else {
                    hist->lastText.assign(initialText);
                    hist->undoStack.clear();
                    hist->redoStack.clear();
                }
            }
        }
        m_editState.preferredColumn.reset();
        m_editState.changed = false;
        m_editState.submitted = false;
        m_editState.multiline = multiline;
        m_editState.readOnly = readOnly;
        m_editState.autoCloseBrackets = false;
        m_editState.tabSize = 4;
        m_editState.insertSpaces = true;
    } else {
        m_editState.multiline = multiline;
        m_editState.readOnly = readOnly;
        if ((multiline || readOnly) && !m_editState.changed && m_editState.text != initialText) {
            m_editState.text.assign(initialText);
            m_editState.cursor = keel::min(m_editState.cursor, m_editState.text.size());
            m_editState.selectionAnchor = keel::min(m_editState.selectionAnchor, m_editState.text.size());
            m_editState.undoStack.clear();
            m_editState.redoStack.clear();
            m_editState.preferredColumn.reset();
        }
    }
    return m_editState.text.as_ref();
}

keel::StringRef Context::GetOrStartTextEdit(
    Id id,
    keel::StringRef initialText,
    keel::usize& outCursor,
    bool& outChanged,
    bool& outSubmitted,
    bool multiline
) {
    keel::usize anchor = 0;
    return GetOrStartTextEdit(id, initialText, outCursor, anchor, outChanged, outSubmitted, multiline, false);
}

keel::StringRef Context::GetOrStartTextEdit(
    Id id,
    keel::StringRef initialText,
    keel::usize& outCursor,
    keel::usize& outSelectionAnchor,
    bool& outChanged,
    bool& outSubmitted,
    bool multiline,
    bool readOnly
) {
    EnsureTextEdit(id, initialText, multiline, readOnly);

    for (const auto& txt : m_pendingTextInput) {
        m_editState.Insert(txt.as_ref());
    }
    m_pendingTextInput.clear();

    for (keel::usize i = 0; i < m_pendingKeyEvents.size(); ) {
        const QueuedKeyEvent ev = m_pendingKeyEvents[i];
        bool lostFocus = false;
        if (m_editState.HandleKey(*this, ev.key, ev.mods, lostFocus)) {
            if (lostFocus) {
                m_focusedId = Id::Null();
            }
            m_pendingKeyEvents.remove_at(i);
        } else {
            ++i;
        }
    }

    m_editState.cursor = keel::min(m_editState.cursor, m_editState.text.size());
    m_editState.selectionAnchor = keel::min(m_editState.selectionAnchor, m_editState.text.size());

    outCursor = m_editState.cursor;
    outSelectionAnchor = m_editState.selectionAnchor;
    outChanged = m_editState.changed;
    outSubmitted = m_editState.submitted;

    m_editState.changed = false;
    m_editState.submitted = false;
    if (outSubmitted) {
        FlushActiveEditHistory();
        m_editState.id = Id::Null();
    }

    return m_editState.text.as_ref();
}

void Context::SetTextEditBuffer(Id id, keel::StringRef text) {
    if (!id.IsValid()) return;
    m_editState.id = id;
    m_editState.text.assign(text);
    m_editState.cursor = m_editState.text.size();
    m_editState.selectionAnchor = m_editState.cursor;
    m_editState.preferredColumn.reset();
    m_editState.changed = false;
    m_editState.submitted = false;
    ResetCursorBlink();
}

void Context::SetTextEditCursor(Id id, keel::usize cursor, keel::usize selectionAnchor) noexcept {
    if (m_editState.id == id && id.IsValid()) {
        m_editState.cursor = keel::min(cursor, m_editState.text.size());
        m_editState.selectionAnchor = keel::min(selectionAnchor, m_editState.text.size());
        m_editState.preferredColumn.reset();
        ResetCursorBlink();
    }
}

void Context::SetTextEditAutoClose(Id id, bool enabled) noexcept {
    if (m_editState.id == id && id.IsValid()) {
        m_editState.autoCloseBrackets = enabled;
    }
}

void Context::SetTextEditIndentation(Id id, keel::u8 tabSize, bool insertSpaces) noexcept {
    if (m_editState.id == id && id.IsValid()) {
        m_editState.tabSize = static_cast<keel::u8>(keel::clamp<keel::i32>(tabSize, 1, 16));
        m_editState.insertSpaces = insertSpaces;
    }
}

void Context::ApplyTextEditChange(Id id, keel::StringRef newText, keel::usize newCursor, keel::usize newSelectionAnchor) {
    if (!id.IsValid() || m_editState.id != id || m_editState.readOnly) return;
    m_editState.SaveUndo();
    m_editState.text.assign(newText);
    m_editState.cursor = keel::min(newCursor, m_editState.text.size());
    m_editState.selectionAnchor = keel::min(newSelectionAnchor, m_editState.text.size());
    m_editState.preferredColumn.reset();
    m_editState.changed = true;
    ResetCursorBlink();
}

bool Context::UndoTextEdit(Id id) {
    if (m_editState.id == id && id.IsValid() && !m_editState.readOnly) {
        if (m_editState.Undo()) {
            ResetCursorBlink();
            return true;
        }
    }
    return false;
}

bool Context::RedoTextEdit(Id id) {
    if (m_editState.id == id && id.IsValid() && !m_editState.readOnly) {
        if (m_editState.Redo()) {
            ResetCursorBlink();
            return true;
        }
    }
    return false;
}

bool Context::GetTextEditSelection(Id id, keel::usize& outStart, keel::usize& outEnd) const noexcept {
    if (m_editState.id == id && id.IsValid()) {
        outStart = m_editState.SelectionStart();
        outEnd = m_editState.SelectionEnd();
        return m_editState.HasSelection();
    }
    outStart = 0;
    outEnd = 0;
    return false;
}

void Context::ResetCursorBlink() noexcept {
    m_lastInputTimeSec = m_currentTimeSec;
}

void Context::SetCurrentTime(double timeSeconds) noexcept {
    m_manualTime = true;
    m_currentTimeSec = timeSeconds;
}

void Context::AdvanceTime(double deltaSeconds) noexcept {
    m_manualTime = true;
    m_currentTimeSec += deltaSeconds;
}

bool Context::IsCursorVisible(keel::Optional<CursorBlinkMode> modeOverride) const noexcept {
    const CursorBlinkMode mode = modeOverride.value_or(m_cursorConfig.mode);
    switch (mode) {
        case CursorBlinkMode::Hidden:
            return false;
        case CursorBlinkMode::Solid:
            return true;
        case CursorBlinkMode::Blinking: {
            const double elapsedSinceInput = m_currentTimeSec - m_lastInputTimeSec;
            // Typing reset phase: cursor remains solid/visible for 0.5s after user interaction
            if (elapsedSinceInput >= 0.0 && elapsedSinceInput < 0.5) {
                return true;
            }
            if (m_cursorConfig.blinkPeriod <= 0.0f) {
                return true;
            }
            const double cycleTime = elapsedSinceInput - 0.5;
            const double phase = keel::fmod(cycleTime, static_cast<double>(m_cursorConfig.blinkPeriod));
            return phase < (static_cast<double>(m_cursorConfig.blinkPeriod) * 0.5);
        }
    }
    return true;
}

static Rect GetNodeContentClipRect(const Node* node, const UiTheme& theme) {
    if (!node) return Rect();
    if (node->type == NodeType::ScrollArea || ((node->type == NodeType::Sidebar || node->type == NodeType::BottomPanel) && node->sidebarScrollable)) {
        return ScrollArea::GetContentClipRect(node, theme);
    }
    if (node->type == NodeType::Scene) {
        return Scene::GetViewportRect(node, nullptr, theme);
    }
    if (node->type == NodeType::Carousel) {
        return Carousel::GetContentClipRect(node);
    }
    if (node->type == NodeType::Toolbar) {
        return Toolbar::GetContentClipRect(node);
    }
    if (node->type == NodeType::Table) {
        return Table::GetBodyClipRect(node, theme);
    }
    if (node->type == NodeType::Modal) {
        return Modal::GetBackdropRect(node);
    }
    return node->computedRect;
}

static bool DoesNodeBlockHit(const Node* node) noexcept {
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
            return !node->sidebarCollapsed;
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
        case NodeType::Container:
        case NodeType::Box:
        case NodeType::ScrollArea:
        case NodeType::TreeBranch:
            return (node->backgroundColor.a > 0) ||
                   (node->borderWidth > 0.0f && node->borderColor.a > 0);
        default:
            return true;
    }
}

template <typename HitVecT>
static void CollectBoundsRecursive(
    Node* node,
    const Node* rootNode,
    keel::HashMap<Id, Rect>& outBounds,
    keel::HashMap<Id, Rect>& outUnclippedBounds,
    HitVecT& outHitNodes,
    keel::u32& ioOrderCounter,
    keel::u8 inheritedZLayer,
    Rect parentClip,
    const UiTheme& theme,
    const Transform2D& parentSpace,
    keel::HashMap<Id, Transform2D>& outSpaces
) {
    if (!node) return;

    const UiTheme& activeTheme = node->GetTheme(theme);

    // Bounds and hit records are kept in frame space; the node's own coordinates are in its local space
    const Transform2D space = NodeSpace(node, parentSpace);
    if (!space.IsIdentity() && node->id.IsValid()) {
        outSpaces.put(node->id, space);
    }
    const Rect frameRect = space.Apply(node->computedRect);

    // Out-of-flow modals and popovers are rendered at top level above ancestor ScrollAreas/clips
    keel::u8 curZLayer = inheritedZLayer;
    if (node->type == NodeType::Modal) {
        parentClip = frameRect;
        curZLayer = 2;
    } else if (node->type == NodeType::Popover) {
        parentClip = Rect(-100000.0f, -100000.0f, 200000.0f, 200000.0f);
        curZLayer = 2;
    } else if ((node->type == NodeType::Sidebar || node->type == NodeType::BottomPanel) && !node->sidebarCollapsed) {
        curZLayer = keel::max<keel::u8>(curZLayer, 1);
    } else if (node->type == NodeType::StatusBar) {
        curZLayer = keel::max<keel::u8>(curZLayer, 1);
    }

    const keel::u32 preIdx = ++ioOrderCounter;
    keel::usize hitRecordIdx = keel::usize(-1);

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
            using ElemT = keel::RemoveCvRef<decltype(*outHitNodes.data())>;
            ElemT rec{};
            rec.id = node->id;
            rec.rect = clipped;
            rec.preIndex = preIdx;
            rec.postIndex = preIdx;
            rec.type = node->type;
            rec.zLayer = curZLayer;
            rec.blocksHit = DoesNodeBlockHit(node);
            outHitNodes.push_back(rec);
        }

        if (node->type == NodeType::Carousel || node->type == NodeType::Toolbar) {
            Carousel::CollectBounds(node, outBounds, parentClip, space);
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
            effectiveClip,
            activeTheme,
            space,
            outSpaces
        );
    }

    if (hitRecordIdx != keel::usize(-1)) {
        outHitNodes[hitRecordIdx].postIndex = ioOrderCounter;
    }
}

bool Context::UpdateScrollAreasRecursive(Node* node) {
    if (!node) return false;

    const bool isModal = (node->type == NodeType::Modal);
    if (isModal) {
        ++m_modalScopeDepth;
    }

    const bool pushedToStack = (node != m_root);
    if (pushedToStack) {
        m_containerStack.push_back(node);
    }

    bool scrollChanged = false;

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        if (UpdateScrollAreasRecursive(child)) {
            scrollChanged = true;
        }
    }

    const Vec2 prevScroll = node->scrollOffset;

    ++m_widgetExecDepth;
    if (node->type == NodeType::ScrollArea || ((node->type == NodeType::Sidebar || node->type == NodeType::BottomPanel) && node->sidebarScrollable)) {
        ScrollArea::Update(*this, node);
        if (keel::abs(node->scrollOffset.x - prevScroll.x) > 0.01f ||
            keel::abs(node->scrollOffset.y - prevScroll.y) > 0.01f) {
            scrollChanged = true;
        }
    } else if (node->type == NodeType::Scene) {
        Scene::Update(*this, node);
    } else if (node->type == NodeType::Carousel || (node->type == NodeType::Toolbar && node->As<CarouselData>() != nullptr)) {
        Carousel::Update(*this, node);
        if (keel::abs(node->scrollOffset.x - prevScroll.x) > 0.01f ||
            keel::abs(node->scrollOffset.y - prevScroll.y) > 0.01f) {
            scrollChanged = true;
        }
    } else if (node->type == NodeType::Table) {
        Table::Update(*this, node);
    }
    if (m_widgetExecDepth > 0) {
        --m_widgetExecDepth;
    }

    if (pushedToStack && !m_containerStack.empty() && m_containerStack.back() == node) {
        m_containerStack.pop_back();
    }
    if (isModal && m_modalScopeDepth > 0) {
        --m_modalScopeDepth;
    }
    return scrollChanged;
}

void Context::ScrollToId(Id id) {
    if (!id.IsValid() || !m_root) return;

    Node* target = nullptr;
    auto findNode = [&](auto& self, Node* n) -> void {
        if (!n || target) return;
        if (n->id == id) {
            target = n;
            return;
        }
        for (Node* c = n->firstChild; c != nullptr; c = c->nextSibling) {
            self(self, c);
        }
    };
    findNode(findNode, m_root);

    if (!target) return;

    for (Node* p = target->parent; p != nullptr; p = p->parent) {
        if ((p->type == NodeType::ScrollArea || ((p->type == NodeType::Sidebar || p->type == NodeType::BottomPanel) && p->sidebarScrollable)) && p->id.IsValid()) {
            ScrollArea::ScrollToChild(*this, p, target);
            break;
        } else if ((p->type == NodeType::Carousel || p->type == NodeType::Toolbar) && p->id.IsValid()) {
            Carousel::ScrollToChild(*this, p, target);
            break;
        }
    }
}

void Context::EndFrame(ICanvas& canvas) {
    m_lastCanvas = &canvas;

    // Balance any unclosed container & zone scopes
    while (m_containerStack.size() > 1) {
        m_containerStack.pop_back();
    }
    m_zoneStack.clear();
    m_modalScopeDepth = 0;

    const Vec2 logicalSize = m_viewportSize / m_pixelsPerDp;

    // Resolve geometry using canvas font metrics
    Solver::Measure(m_root, &canvas, m_theme);
    Solver::Arrange(m_root, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), m_theme, &canvas);

    // Update interactive scrollbars; re-arrange only if a scrollbar drag or track click modified scrollOffset
    if (UpdateScrollAreasRecursive(m_root)) {
        Solver::Arrange(m_root, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), m_theme, &canvas);
    }

    // Automatically run ToastHost if there are active toasts and user didn't call ui.ToastHost() explicitly
    if (!m_toastHostVisitedThisFrame && !m_toasts.empty()) {
        Toast::RunHost(*this, m_defaultToastHostOpts);
    }

    // Cache computed bounds, Z-order hit records, overlay hit regions, and focusable widget list for next frame
    keel::u32 hitOrderCounter = 0;
    CollectBoundsRecursive(
        m_root,
        m_root,
        m_currFrameBounds,
        m_currUnclippedBounds,
        m_currHitNodes,
        hitOrderCounter,
        0,
        Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y),
        m_theme,
        Transform2D{},
        m_currNodeSpaces
    );
    for (const auto& builder : m_overlayHitBuilders) {
        if (builder.invoke) {
            m_spaceOverride = &builder.space; // regions are given in the space the builder was registered from
            builder.invoke(builder.ctx);
            m_spaceOverride = nullptr;
        }
    }
    ClearOverlayHitBuilders();
    m_prevOverlayHitRegions = keel::move(m_currOverlayHitRegions);
    m_currOverlayHitRegions.clear();
    m_prevHitNodes = keel::move(m_currHitNodes);
    m_currHitNodes.clear();
    m_prevHitNodeMap.clear();
    m_prevHitNodeMap.reserve(m_prevHitNodes.size());
    for (keel::usize i = 0; i < m_prevHitNodes.size(); ++i) {
        m_prevHitNodeMap.put(m_prevHitNodes[i].id, i);
    }

    m_prevFrameBounds = keel::move(m_currFrameBounds);
    m_prevUnclippedBounds = keel::move(m_currUnclippedBounds);
    m_prevNodeSpaces = keel::move(m_currNodeSpaces);
    m_currNodeSpaces.clear();
    m_prevFocusableIds = keel::move(m_currFocusableIds);
    m_prevModalFocusableIds = keel::move(m_currModalFocusableIds);

    // Suspend focus from occluded background widgets when a Modal opens, and restore it when the Modal closes
    if (m_currFrameModalId.IsValid()) {
        if (m_focusedId.IsValid()) {
            const bool insideModal = ContainsId(m_prevModalFocusableIds, m_focusedId);
            if (!insideModal) {
                if (!m_savedFocusBeforeModal.IsValid()) {
                    m_savedFocusBeforeModal = m_focusedId;
                }
                if (m_editState.id == m_focusedId) {
                    FlushActiveEditHistory();
                    m_editState.id = Id::Null();
                }
                m_focusedId = Id::Null();
                m_focusVisible = false;
                m_suspendDirectEditInput = false;
            }
        }
    } else if (m_savedFocusBeforeModal.IsValid()) {
        if (!m_focusedId.IsValid()) {
            if (ContainsId(m_prevFocusableIds, m_savedFocusBeforeModal)) {
                m_focusedId = m_savedFocusBeforeModal;
                m_focusVisible = (m_inputModality == InputModality::Keyboard);
            }
        }
        m_savedFocusBeforeModal = Id::Null();
    }

    // Reset single-frame mouse impulse states
    m_mouseClicked = false;
    m_mouseReleased = false;
    m_mouseDoubleClicked = false;
    m_rightMouseClicked = false;
    m_rightMouseReleased = false;
    m_middleMouseClicked = false;
    m_middleMouseReleased = false;
    m_mouseWheel = {0.0f, 0.0f};
    if (!m_mouseDown && !m_rightMouseDown && !m_middleMouseDown) {
        m_activeId = Id::Null();
    }

    // Calculate dirty damage regions by snapshot diffing
    keel::HashMap<Id, WidgetSnapshot> currSnapshots;
    currSnapshots.reserve(m_prevSnapshots.size() + 16);
    keel::Array<Rect> damageRects = keel::move(m_explicitDamageRects);
    m_explicitDamageRects.clear();

    auto addDamage = [&](Rect r) {
        if (r.w > 0.0f && r.h > 0.0f) {
            damageRects.push_back(r);
        }
    };

    const bool cursorVis = IsCursorVisible();
    bool anyDebugBounds = m_debugBoundsEnabled;
    bool anyInspectMode = m_inspectModeEnabled;

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
                const Rect bd = parentSpace.Apply(Modal::GetBackdropRect(n));
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
                         (n->scrollbarHovered ? 256u : 0u) |
                         (n->scrollbarActive ? 512u : 0u) |
                         (n->scrollbarHoveredX ? 1024u : 0u) |
                         (n->scrollbarActiveX ? 2048u : 0u);
            snap.bgColor = n->backgroundColor.ToUint32();
            snap.borderColor = n->borderColor.ToUint32();

            keel::u64 h = n->visualHash != 0 ? n->visualHash : detail::HashStringView(n->text);
            h = detail::HashArgs(
                h,
                n->computedRect.x,
                n->computedRect.y,
                n->computedRect.w,
                n->computedRect.h,
                n->scrollOffset.x,
                n->scrollOffset.y
            );
            if (m_editState.id.IsValid() && n->id == m_editState.id) {
                snap.flags |= (cursorVis ? 16u : 0u) | 32u;
                snap.cursor = m_editState.cursor;
                snap.selectionAnchor = m_editState.selectionAnchor;
                h = detail::HashStringView(m_editState.text.as_ref(), h);
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

    collectSnapshots(collectSnapshots, m_root, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), m_theme, Transform2D{});
    m_anyScopedInspectActive = anyInspectMode;

    // Detect removed widgets
    for (const auto& [id, prevSnap] : m_prevSnapshots) {
        if (!currSnapshots.contains(id)) {
            addDamage(prevSnap.rect);
        }
    }

    if (m_openPopupId != m_prevOpenPopupId ||
        m_openPopupRect != m_prevOpenPopupRect ||
        (m_openPopupId.IsValid() && m_openPopupVisualHash != m_prevOpenPopupVisualHash)) {
        if (m_prevOpenPopupId.IsValid() && m_prevOpenPopupRect.w > 0.0f && m_prevOpenPopupRect.h > 0.0f) {
            addDamage(ExpandPopupDamageRect(m_prevOpenPopupRect));
        }
        if (m_openPopupId.IsValid() && m_openPopupRect.w > 0.0f && m_openPopupRect.h > 0.0f) {
            addDamage(ExpandPopupDamageRect(m_openPopupRect));
        }
    }
    m_prevOpenPopupId = m_openPopupId;
    m_prevOpenPopupRect = m_openPopupRect;
    m_prevOpenPopupVisualHash = m_openPopupVisualHash;

    const bool hasTooltip = !m_activeTooltip.empty();
    const keel::u64 tooltipHash = hasTooltip
        ? detail::HashArgs(
              m_activeTooltip,
              m_activeTooltipAnchor,
              m_activeTooltipViewport,
              m_activeTooltipAnchor.w <= 0.0f ? m_mousePos.x : 0.0f,
              m_activeTooltipAnchor.w <= 0.0f ? m_mousePos.y : 0.0f
          )
        : 0;
    const bool tooltipChanged = (hasTooltip != m_hadTooltipLastFrame) ||
                                (hasTooltip && tooltipHash != m_prevTooltipHash);

    const bool hasModal = m_currFrameModalId.IsValid();
    const bool modalChanged = (hasModal != m_hadModalLastFrame) || m_modalClosedThisFrame;

    const bool hasToasts = !m_toasts.empty();
    const bool toastsChanged = (hasToasts != m_hadToastsLastFrame);

    const bool modalOverlayChanged = (m_currFrameModalOverlay != m_hadModalOverlayLastFrame);
    const bool debugBoundsChanged = (anyDebugBounds != m_hadDebugBoundsLastFrame);
    const bool inspectModeChanged = (anyInspectMode != m_hadInspectModeLastFrame);

    const bool anyOverlayStateChanged =
        modalChanged || toastsChanged || modalOverlayChanged ||
        tooltipChanged || debugBoundsChanged || inspectModeChanged;

    const bool activeOverlayRequiresFullOnDamage =
        !damageRects.empty() &&
        (hasModal || hasToasts || m_currFrameModalOverlay ||
         hasTooltip || anyDebugBounds || anyInspectMode);

    // Determine whether full repaint or partial dirty rect is required
    m_dirtyRects.clear();
    const float totalViewportArea = logicalSize.x * logicalSize.y;
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
        // Snap each damage rect to integer logical pixel bounds and coalesce overlapping regions
        for (const Rect& raw : damageRects) {
            const float x1 = keel::max(0.0f, keel::floor(raw.x));
            const float y1 = keel::max(0.0f, keel::floor(raw.y));
            const float x2 = keel::min(logicalSize.x, keel::ceil(raw.Right()));
            const float y2 = keel::min(logicalSize.y, keel::ceil(raw.Bottom()));
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
            float totalDamageArea = m_dirtyRects[0].Area();
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
    m_hadTooltipLastFrame = hasTooltip;
    m_prevTooltipHash = tooltipHash;
    m_hadToastsLastFrame = hasToasts;
    m_hadModalLastFrame = hasModal || m_modalClosedThisFrame;
    m_hadModalOverlayLastFrame = m_currFrameModalOverlay;
    m_hadDebugBoundsLastFrame = anyDebugBounds;
    m_hadInspectModeLastFrame = anyInspectMode;
    m_prevFrameModalId = m_currFrameModalId;
    m_prevFrameModalBackdropRect = m_currFrameModalBackdropRect;
    m_prevSnapshots = keel::move(currSnapshots);

    // Overlays draw (and query the context) in the space they were registered from
    auto invokeOverlay = [&](const OverlayPass& pass) {
        const bool transformed = !pass.space.IsIdentity();
        if (transformed) {
            canvas.PushTransform(pass.space);
        }
        m_spaceOverride = &pass.space;
        pass.invoke(pass.ctx, canvas);
        m_spaceOverride = nullptr;
        if (transformed) {
            canvas.PopTransform();
        }
    };

    // If damage exists, clear target buffer and rasterize scene graph
    if (m_hasDirtyDamage) {
        canvas.Begin(static_cast<keel::u32>(m_viewportSize.x), static_cast<keel::u32>(m_viewportSize.y), m_pixelsPerDp);

        if (m_isFullRepaint) {
            canvas.PushClip(m_dirtyRect);
            canvas.Clear(m_clearColor);
            RenderNodeRecursive(m_root, canvas, m_dirtyRect, m_theme);
            RenderModalsRecursive(m_root, canvas, m_dirtyRect, m_theme);

            for (const auto& pass : m_overlayPasses) {
                if (pass.clipRect.w > 0.0f && pass.clipRect.h > 0.0f) {
                    if (!pass.clipRect.Intersects(m_dirtyRect)) continue;
                    canvas.PushClip(pass.clipRect);
                    invokeOverlay(pass);
                    canvas.PopClip();
                } else {
                    invokeOverlay(pass);
                }
            }
            if (hasTooltip) {
                RenderTooltipOverlay(canvas, logicalSize);
            }
            if (anyDebugBounds) {
                RenderDebugBoundsRecursive(m_root, canvas, m_debugBoundsEnabled);
            }
            if (anyInspectMode) {
                RenderInspectOverlay(m_root, canvas, logicalSize);
            }

            canvas.PopClip();
            canvas.End();
        } else {
            for (const Rect& dr : m_dirtyRects) {
                canvas.PushClip(dr);
                canvas.Clear(m_clearColor, dr);
                RenderNodeRecursive(m_root, canvas, dr, m_theme);
                RenderModalsRecursive(m_root, canvas, dr, m_theme);
                for (const auto& pass : m_overlayPasses) {
                    if (pass.clipRect.w > 0.0f && pass.clipRect.h > 0.0f) {
                        const Rect clip = pass.clipRect.Intersect(dr);
                        if (clip.w <= 0.0f || clip.h <= 0.0f) continue;
                        canvas.PushClip(clip);
                        invokeOverlay(pass);
                        canvas.PopClip();
                    } else {
                        invokeOverlay(pass);
                    }
                }
                canvas.PopClip();
                canvas.End();
            }
        }
    }

    m_pendingKeyEvents.clear();
    m_pendingTextInput.clear();

    bool trimmedAny = false;
    if (m_currentTimeSec < m_lastCacheTrimSweepSec) {
        m_lastCacheTrimSweepSec = m_currentTimeSec;
    }
    if (m_currentTimeSec < m_lastTransientPruneSec) {
        m_lastTransientPruneSec = m_currentTimeSec;
    }

    // Trim regenerable offscreen pixel/tile/layout caches from inactive widget states
    // (idle > 120 frames in continuous loops, or >= 1.5s + > 3 frames in reactive idle mode)
    // while keeping user-facing persistent state (camera, zoom, selections, markers, undo history) intact.
    const bool checkTrimNow = (m_frameIndex > 0) &&
        ((m_frameIndex % 60 == 0) ||
         m_repaintFrames <= 1 ||
         (m_currentTimeSec - m_lastCacheTrimSweepSec >= 0.5));
    if (checkTrimNow) {
        m_lastCacheTrimSweepSec = m_currentTimeSec;
        bool stillHasUntrimmed = false;
        for (auto [key, state] : m_customStates) {
            if (!state.trim || state.cachesTrimmed) {
                continue;
            }
            if (state.lastFrameActive > m_frameIndex) {
                state.lastFrameActive = m_frameIndex;
            }
            if (m_currentTimeSec < state.lastTimeActiveSec) {
                state.lastTimeActiveSec = m_currentTimeSec;
            }
            const keel::u64 idleFrames = m_frameIndex - state.lastFrameActive;
            if (idleFrames == 0 || key.id == m_focusedId || key.id == m_activeId) {
                continue;
            }
            const double idleSec = m_currentTimeSec - state.lastTimeActiveSec;
            if (idleFrames > 120 || (idleFrames > 3 && idleSec >= 1.5)) {
                state.trim(state.ptr);
                state.cachesTrimmed = true;
                trimmedAny = true;
            } else {
                stillHasUntrimmed = true;
            }
        }
        m_hasUntrimmedInactiveStates = stillHasUntrimmed;

        if (!m_focusedId.IsValid() && !m_editState.id.IsValid() &&
            (m_editState.text.capacity() > 4096 || !m_editState.undoStack.empty() || !m_editState.redoStack.empty())) {
            m_editState.text = {};
            m_editState.undoStack = {};
            m_editState.redoStack = {};
            trimmedAny = true;
        }
    }

    // Periodically prune stale transient states (every 600 frames or >= 10s in reactive mode);
    // persistent widget states (tabs, trees, tables, scenes, scroll offsets, undo stacks) survive until Context destruction.
    if (m_frameIndex > 0 && ((m_frameIndex % 600 == 0) || (m_currentTimeSec - m_lastTransientPruneSec >= 10.0))) {
        m_lastTransientPruneSec = m_currentTimeSec;
        keel::SmallArray<detail::StateKey, 16> staleKeys;
        for (const auto& [key, state] : m_customStates) {
            if (state.persistent || key.id == m_focusedId || key.id == m_activeId) {
                continue;
            }
            const keel::u64 idleFrames = (m_frameIndex > state.lastFrameActive)
                ? (m_frameIndex - state.lastFrameActive)
                : 0;
            const double idleSec = (m_currentTimeSec > state.lastTimeActiveSec)
                ? (m_currentTimeSec - state.lastTimeActiveSec)
                : 0.0;
            if (idleFrames > 600 || (idleFrames > 5 && idleSec >= 10.0)) {
                staleKeys.push_back(key);
            }
        }
        for (const auto& key : staleKeys) {
            (void)m_customStates.remove(key);
            trimmedAny = true;
        }
    }

    if (trimmedAny) {
        keel::memory_trim();
    }

    if (m_repaintFrames > 0) {
        --m_repaintFrames;
    }
    m_containerStack.clear();
}

void Context::RenderNodeRecursive(Node* node, ICanvas& canvas, const Rect& dirtyRect, const UiTheme& parentTheme) {
    if (!node) return;
    if (node->computedRect.w <= 0.0f || node->computedRect.h <= 0.0f) return;
    // A scaled node draws itself and its subtree in its local space; `dirtyRect` arrives in the parent's
    const Transform2D local = NodeLocalTransform(node);
    if (!node->IsOutOfFlow() && !local.Apply(node->computedRect).Intersects(dirtyRect)) return;
    const bool transformed = !local.IsIdentity();
    if (transformed) {
        canvas.PushTransform(local);
    }
    const Rect nodeDirty = transformed ? local.Invert(dirtyRect) : dirtyRect;

    const UiTheme& activeTheme = node->GetTheme(parentTheme);

    // 1. Render container background fill before children (skip for self-rendering leaf widgets and out-of-flow overlays)
    const bool isContainerSurface = (!node->IsLeaf() && !node->IsOutOfFlow());
    const bool hasContainerBorder = isContainerSurface && (node->borderColor.a > 0 && node->borderWidth > 0.0f);
    if (isContainerSurface && node->backgroundColor.a > 0) {
        if (node->cornerRadius > 0.0f) {
            canvas.DrawRoundRect(
                node->computedRect,
                node->cornerRadius,
                node->backgroundColor,
                Color::Transparent(),
                0.0f
            );
        } else {
            canvas.DrawRect(node->computedRect, node->backgroundColor);
        }
    }

    // Render specific widget contents
    if (node->renderFn) {
        node->renderFn(node, canvas, activeTheme);
    }

    Rect childDirtyRect = nodeDirty;
    if (node->clipChildren) {
        const Rect clipRect = GetNodeContentClipRect(node, activeTheme);
        canvas.PushClip(clipRect);
        childDirtyRect = nodeDirty.Intersect(clipRect);
    }

    // Recurse on children (skip out-of-flow overlays during normal tree walk; overlays render in RenderModalsRecursive)
    if (childDirtyRect.w > 0.0f && childDirtyRect.h > 0.0f) {
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->IsOutOfFlow()) continue;
            RenderNodeRecursive(child, canvas, childDirtyRect, activeTheme);
        }
    }

    if (node->clipChildren) {
        canvas.PopClip();
    }

    // Mask outer corner wedges when a rounded clipping container hosts children so rectangular child fills
    // (e.g. full-bleed toolbars, titlebars, or sidebars) never bleed outside the parent's rounded corners.
    if (isContainerSurface && node->clipChildren && node->cornerRadius > 1.5f && node->firstChild != nullptr) {
        const Rect& r = node->computedRect;
        const float rad = keel::clamp(node->cornerRadius, 0.0f, keel::min(r.w, r.h) * 0.5f);
        if (rad > 1.5f) {
            Color outerBg = m_clearColor;
            for (const Node* anc = node->parent; anc != nullptr; anc = anc->parent) {
                if (anc->backgroundColor.a == 255) {
                    outerBg = anc->backgroundColor;
                    break;
                }
            }
            if (outerBg.a > 0) {
                const keel::i32 steps = keel::ceil_i32(rad);
                for (keel::i32 i = 0; i < steps; ++i) {
                    const float dy = static_cast<float>(i);
                    const float rem = keel::max(0.0f, rad - dy - 0.35f);
                    const float dx = keel::max(0.0f, rad - keel::sqrt(keel::max(0.0f, rad * rad - rem * rem)));
                    if (dx > 0.25f) {
                        canvas.DrawRect(Rect(r.x - 0.5f, r.y + dy, dx + 1.0f, 1.25f), outerBg);
                        canvas.DrawRect(Rect(r.Right() - dx - 0.5f, r.y + dy, dx + 1.0f, 1.25f), outerBg);
                        canvas.DrawRect(Rect(r.x - 0.5f, r.Bottom() - dy - 1.0f, dx + 1.0f, 1.25f), outerBg);
                        canvas.DrawRect(Rect(r.Right() - dx - 0.5f, r.Bottom() - dy - 1.0f, dx + 1.0f, 1.25f), outerBg);
                    }
                }
            }
        }
    }

    // 2. Render container border stroke on top of children (after PopClip) so children never overwrite parent border
    if (hasContainerBorder) {
        const Rect& r = node->computedRect;
        if (node->cornerRadius <= 0.0f && node->type == NodeType::Sidebar) {
            const float x = node->sidebarRight ? r.x : keel::max(r.x, r.Right() - node->borderWidth);
            canvas.DrawRect(Rect(x, r.y, node->borderWidth, r.h), node->borderColor);
        } else if (node->cornerRadius <= 0.0f &&
                   (node->type == NodeType::BottomPanel || node->type == NodeType::StatusBar)) {
            canvas.DrawRect(Rect(r.x, r.y, r.w, node->borderWidth), node->borderColor);
        } else if (node->cornerRadius <= 0.0f && node->type == NodeType::Toolbar) {
            if (node->direction == FlexDirection::Column) {
                const float x = keel::max(r.x, r.Right() - node->borderWidth);
                canvas.DrawRect(Rect(x, r.y, node->borderWidth, r.h), node->borderColor);
            } else {
                const float y = keel::max(r.y, r.Bottom() - node->borderWidth);
                canvas.DrawRect(Rect(r.x, y, r.w, node->borderWidth), node->borderColor);
            }
        } else {
            canvas.DrawRoundRect(
                r,
                node->cornerRadius,
                Color::Transparent(),
                node->borderColor,
                node->borderWidth
            );
        }
    }

    // Render post-pass overlay contents (after children and outside clip)
    if (node->postRenderFn) {
        node->postRenderFn(node, canvas, activeTheme);
    }

    if (transformed) {
        canvas.PopTransform();
    }
}

void Context::RenderModalsRecursive(Node* node, ICanvas& canvas, const Rect& dirtyRect, const UiTheme& parentTheme) {
    if (!node) return;
    const UiTheme& activeTheme = node->GetTheme(parentTheme);

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        if (child->type == NodeType::Modal) {
            const Rect backdropClip = Modal::GetBackdropRect(child);
            if (backdropClip.w > 0.0f && backdropClip.h > 0.0f) {
                canvas.PushClip(backdropClip);
                RenderNodeRecursive(child, canvas, dirtyRect, activeTheme);
                canvas.PopClip();
            } else {
                RenderNodeRecursive(child, canvas, dirtyRect, activeTheme);
            }
        } else if (child->type == NodeType::Popover) {
            RenderNodeRecursive(child, canvas, dirtyRect, activeTheme);
        }
        // Modals nested in a scaled subtree render in that subtree's space
        const Transform2D childLocal = NodeLocalTransform(child);
        if (childLocal.IsIdentity()) {
            RenderModalsRecursive(child, canvas, dirtyRect, activeTheme);
        } else {
            canvas.PushTransform(childLocal);
            RenderModalsRecursive(child, canvas, childLocal.Invert(dirtyRect), activeTheme);
            canvas.PopTransform();
        }
    }
}

void Context::SetThemeMode(ThemeMode mode) noexcept {
    if (m_theme.mode != mode) {
        m_theme.SetMode(mode);
        m_clearColor = m_theme.palette.background;
        InvalidateAll();
    }
}

void Context::SetDebugBounds(bool enabled) noexcept {
    if (m_debugBoundsEnabled != enabled) {
        m_debugBoundsEnabled = enabled;
        InvalidateAll();
    }
}

void Context::SetInspectMode(bool enabled) noexcept {
    if (m_inspectModeEnabled != enabled) {
        m_inspectModeEnabled = enabled;
        InvalidateAll();
    }
}

void Context::RenderDebugBoundsRecursive(Node* node, ICanvas& canvas, bool inheritedDebugBounds) {
    if (!node) return;
    if (node->computedRect.w <= 0.0f || node->computedRect.h <= 0.0f) return;

    const bool activeDebug = node->GetDebugBoundsOverride().value_or(inheritedDebugBounds);
    const Transform2D local = NodeLocalTransform(node);
    const bool transformed = !local.IsIdentity();
    if (transformed) {
        canvas.PushTransform(local);
    }

    if (node != m_root && activeDebug) {
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

        // Draw 1px crisp bounding outline inset by 0.5px
        const Rect strokeRect(
            node->computedRect.x + 0.5f,
            node->computedRect.y + 0.5f,
            keel::max(0.0f, node->computedRect.w - 1.0f),
            keel::max(0.0f, node->computedRect.h - 1.0f)
        );
        canvas.DrawRoundRect(strokeRect, 0.0f, Color(0, 0, 0, 0), stroke, 1.0f);

        // Draw spacer fill
        if (cat == NodeCategory::Spacer) {
            canvas.DrawRect(node->computedRect, padColor);
        }

        // Draw padding strips if node defines internal padding
        const auto& p = node->padding;
        if (p.top > 0.0f || p.bottom > 0.0f || p.left > 0.0f || p.right > 0.0f) {
            const float x = node->computedRect.x;
            const float y = node->computedRect.y;
            const float w = node->computedRect.w;
            const float h = node->computedRect.h;

            if (p.top > 0.0f) {
                canvas.DrawRect(Rect(x, y, w, p.top), padColor);
            }
            if (p.bottom > 0.0f) {
                canvas.DrawRect(Rect(x, y + h - p.bottom, w, p.bottom), padColor);
            }
            const float midH = keel::max(0.0f, h - p.top - p.bottom);
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
        canvas.PushClip(GetNodeContentClipRect(node, node->GetTheme(m_theme)));
    }

    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        RenderDebugBoundsRecursive(child, canvas, activeDebug);
    }

    if (node->clipChildren) {
        canvas.PopClip();
    }

    if (transformed) {
        canvas.PopTransform();
    }
}

Node* Context::FindDeepestNodeAt(Node* root, Vec2 pos, Rect clipRect) {
    if (!root) return nullptr;
    // `pos` and `clipRect` arrive in the parent's space; a scaled node is tested in its own
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
        effectiveClip = clipRect.Intersect(GetNodeContentClipRect(root, root->GetTheme(m_theme)));
    }

    Node* bestHit = nullptr;
    for (Node* c = root->firstChild; c != nullptr; c = c->nextSibling) {
        Node* hit = FindDeepestNodeAt(c, pos, effectiveClip);
        if (hit) {
            bestHit = hit;
        }
    }

    return bestHit ? bestHit : root;
}

void Context::RenderInspectOverlay(Node* root, ICanvas& canvas, Vec2 logicalSize) {
    if (!root) return;

    Node* target = FindDeepestNodeAt(root, m_mousePos, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y));
    if (!target || target == m_root) return;

    if (!target->GetInspectModeOverride().value_or(m_inspectModeEnabled)) return;

    // Intersect ancestor clipChildren rects so inspected scrolled children don't bleed outside ScrollArea
    Rect ancestorClip(0.0f, 0.0f, logicalSize.x, logicalSize.y);
    for (const Node* p = target->parent; p != nullptr; p = p->parent) {
        if (p->clipChildren) {
            ancestorClip = ancestorClip.Intersect(GetNodeSpace(p->id).Apply(GetNodeContentClipRect(p, p->GetTheme(m_theme))));
        }
    }
    canvas.PushClip(ancestorClip);
    // The highlight is drawn in the target's own space (the clip above is in frame space)
    const Transform2D targetSpace = GetNodeSpace(target->id);
    canvas.PushTransform(targetSpace);

    // 1. Highlight inspected target with Chrome DevTools Box Model styling
    // Content box: translucent blue (#6FA8DC, ~40% opacity)
    const Rect contentRect(
        target->computedRect.x + target->padding.left,
        target->computedRect.y + target->padding.top,
        keel::max(0.0f, target->computedRect.w - target->padding.Horizontal()),
        keel::max(0.0f, target->computedRect.h - target->padding.Vertical())
    );
    if (contentRect.w > 0.0f && contentRect.h > 0.0f) {
        canvas.DrawRect(contentRect, Color(111, 168, 220, 100));
    }

    // Padding strips: translucent green (#93C47D, ~55% opacity)
    const auto& p = target->padding;
    if (p.top > 0.0f || p.bottom > 0.0f || p.left > 0.0f || p.right > 0.0f) {
        const Color devToolsPadColor(147, 196, 125, 140);
        const float x = target->computedRect.x;
        const float y = target->computedRect.y;
        const float w = target->computedRect.w;
        const float h = target->computedRect.h;

        if (p.top > 0.0f) {
            canvas.DrawRect(Rect(x, y, w, p.top), devToolsPadColor);
        }
        if (p.bottom > 0.0f) {
            canvas.DrawRect(Rect(x, y + h - p.bottom, w, p.bottom), devToolsPadColor);
        }
        const float midH = keel::max(0.0f, h - p.top - p.bottom);
        if (midH > 0.0f) {
            if (p.left > 0.0f) {
                canvas.DrawRect(Rect(x, y + p.top, p.left, midH), devToolsPadColor);
            }
            if (p.right > 0.0f) {
                canvas.DrawRect(Rect(x + w - p.right, y + p.top, p.right, midH), devToolsPadColor);
            }
        }
    }

    // Outer 2px solid cyan boundary
    canvas.DrawRoundRect(target->computedRect, 0.0f, Color(0, 0, 0, 0), Color(56, 189, 248, 255), 2.0f);
    canvas.PopTransform();
    canvas.PopClip();

    // 2. Build DevTools floating info badge
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
    } else if (target->type == NodeType::Heading) {
        if (const auto* h = target->As<HeadingOptions>()) {
            if (!h->label.empty()) {
                labelStr = h->label.text.substr(0, 24);
            }
        }
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

    const float fontSize = 11.0f;
    const float w1 = canvas.MeasureText(line1, fontSize).x;
    const float w2 = canvas.MeasureText(line2, fontSize).x;
    const float w3 = canvas.MeasureText(line3, fontSize).x;
    const float maxTextW = keel::max(w1, keel::max(w2, w3));

    const float badgeW = keel::max(140.0f, maxTextW + 24.0f);
    const float badgeH = 58.0f;

    // Positioning with smart flip if close to viewport boundaries
    float bx = m_mousePos.x + 16.0f;
    float by = m_mousePos.y + 16.0f;
    if (bx + badgeW > logicalSize.x - 8.0f) {
        bx = m_mousePos.x - badgeW - 12.0f;
    }
    if (by + badgeH > logicalSize.y - 8.0f) {
        by = m_mousePos.y - badgeH - 12.0f;
    }
    bx = keel::clamp(bx, 8.0f, keel::max(8.0f, logicalSize.x - badgeW - 8.0f));
    by = keel::clamp(by, 8.0f, keel::max(8.0f, logicalSize.y - badgeH - 8.0f));

    const Rect badgeRect(bx, by, badgeW, badgeH);

    // Draw dark tooltip backdrop
    canvas.DrawRoundRect(badgeRect, 6.0f, Color(15, 23, 42, 245), Color(71, 85, 105, 220), 1.0f);

    // Render 3 lines of text inside badge
    canvas.DrawTextBox(line1, Rect(bx + 10.0f, by + 5.0f, badgeW - 20.0f, 16.0f), fontSize, Color(56, 189, 248, 255), 0.0f, 0.5f);
    canvas.DrawTextBox(line2, Rect(bx + 10.0f, by + 21.0f, badgeW - 20.0f, 16.0f), fontSize, Color(241, 245, 249, 255), 0.0f, 0.5f);
    canvas.DrawTextBox(line3, Rect(bx + 10.0f, by + 37.0f, badgeW - 20.0f, 16.0f), fontSize, Color(148, 163, 184, 255), 0.0f, 0.5f);
}

void Context::RenderTooltipOverlay(ICanvas& canvas, Vec2 logicalSize) {
    if (m_activeTooltip.empty()) return;

    const float fontSize = m_theme.ResolveFontSize(m_activeTooltip, TextRole::LabelSmall, UiSize::Sm);
    const float lineH = canvas.GetLineHeight(fontSize);

    const float maxTextW = 280.0f;
    const float natW = TextUtil::MeasureUiTextWidth(m_activeTooltip, fontSize, &canvas);
    const float contentW = keel::min(natW, maxTextW);
    const float contentH = TextUtil::MeasureUiTextHeight(
        m_activeTooltip, keel::max(1.0f, contentW), fontSize, lineH, &canvas);

    const float padX = 10.0f;
    const float padY = 6.0f;
    const float boxW = keel::max(28.0f, contentW + padX * 2.0f);
    const float boxH = keel::max(24.0f, contentH + padY * 2.0f);

    const Rect vp = (m_activeTooltipViewport.w > 1.0f && m_activeTooltipViewport.h > 1.0f)
        ? m_activeTooltipViewport
        : Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y);

    float bx = m_mousePos.x + 12.0f;
    float by = m_mousePos.y + 18.0f;
    if (m_activeTooltipAnchor.w > 0.0f && m_activeTooltipAnchor.h > 0.0f) {
        bx = m_activeTooltipAnchor.x + (m_activeTooltipAnchor.w - boxW) * 0.5f;
        by = m_activeTooltipAnchor.y - boxH - 6.0f;
        if (by < vp.y + 6.0f) {
            by = m_activeTooltipAnchor.y + m_activeTooltipAnchor.h + 6.0f;
        }
    }

    bx = keel::clamp(bx, vp.x + 6.0f, keel::max(vp.x + 6.0f, vp.Right() - boxW - 6.0f));
    by = keel::clamp(by, vp.y + 6.0f, keel::max(vp.y + 6.0f, vp.Bottom() - boxH - 6.0f));

    canvas.PushClip(vp);
    const Rect boxRect(bx, by, boxW, boxH);
    canvas.DrawRoundRect(Rect(bx, by + 2.0f, boxW, boxH), 6.0f, Color(0, 0, 0, 60), Color::Transparent(), 0.0f);
    canvas.DrawRoundRect(boxRect, 6.0f, Color(15, 23, 42, 245), Color(71, 85, 105, 220), 1.0f);

    const Color textColor = m_theme.ResolveTextColor(m_activeTooltip, Color(248, 250, 252, 255));

    TextUtil::DrawUiText(
        canvas, m_activeTooltip,
        Rect(bx + padX, by + padY, contentW + 2.0f, contentH),
        fontSize, lineH, textColor, 0.0f, 0.5f);
    canvas.PopClip();
}

} // namespace guikit
