#include "guikit/context.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/math.h"
#include "keel/memory.h"
#include "keel/time.h"
#include <cassert>

namespace guikit {

Context::Context()
    : m_startTimeNs(keel::monotonic_ns()) {
}

Context::~Context() {
    m_overlays.ClearOverlayPasses();
    m_overlays.ClearOverlayHitBuilders();
}

void Context::BeginFrame(Vec2 viewportSize, keel::f32 displayScale, keel::f32 rootZoom) {
    ++m_frameIndex;
    const keel::f32 effDisplay = (displayScale > 0.0f) ? displayScale : 1.0f;
    const keel::f32 effZoom = (rootZoom > 0.0f) ? rootZoom : 1.0f;
    const keel::f32 effScale = effDisplay * effZoom;
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
            ? keel::as<keel::f64>(nowNs - m_startTimeNs) * 1e-9
            : 0.0;
    }

    m_overlays.BeginFrame();
    m_focus.BeginFrame();
    m_hitTree.BeginFrame();
    m_idStack.clear();
    m_cursor = CursorType::Default;

    const Id topModalId = m_overlays.GetPrevFrameModalId();
    if (!m_overlays.GetOpenPopupId().IsValid()) {
        const Id curFocused = m_focus.GetFocusedId();
        const bool multilineEditorFocused =
            curFocused.IsValid() &&
            m_editState.id == curFocused &&
            (m_editState.multiline || m_suspendDirectEditInput) &&
            !m_editState.readOnly;
        for (keel::usize evIdx = 0; evIdx < m_pendingKeyEvents.size(); ) {
            const QueuedKeyEvent ev = m_pendingKeyEvents[evIdx];
            const bool isIndent = m_keymap.Matches(KeyAction::Indent, ev.key, ev.mods);
            const bool isUnindent = m_keymap.Matches(KeyAction::Unindent, ev.key, ev.mods);
            if ((isIndent || isUnindent) && !multilineEditorFocused) {
                const Id nextId = m_focus.ComputeNextFocusTarget(topModalId, isUnindent);
                if (!nextId.IsValid()) {
                    ++evIdx;
                    continue;
                }
                m_pendingKeyEvents.remove_at(evIdx);
                if (m_editState.id.IsValid() && m_editState.id != nextId) {
                    FlushActiveEditHistory();
                    m_editState.id = Id::Null();
                }
                m_focus.SetFocusedId(nextId);
                m_focus.SetInputModality(InputModality::Keyboard);
                m_focus.SetFocusVisible(true);
                m_suspendDirectEditInput = false;
                InvalidateAll();
            } else {
                ++evIdx;
            }
        }
    }

    if (m_mouseClicked && m_overlays.GetOpenPopupId().IsValid()) {
        const Rect* selectBounds = m_hitTree.FindPrevBoundsPtr(m_overlays.GetOpenPopupId());
        const bool overSelect = (selectBounds != nullptr) && selectBounds->Contains(m_mousePos);
        const Rect anchorRect = m_overlays.GetOpenPopupAnchorRect();
        const bool overAnchor = (anchorRect.w > 0.0f && anchorRect.h > 0.0f) &&
                                anchorRect.Contains(m_mousePos);
        const bool overPopup = IsPointInOpenPopup(m_mousePos);
        if (!overSelect && !overAnchor && !overPopup) {
            ClosePopup();
        }
    }

    if (m_mouseClicked && m_focus.GetFocusedId().IsValid()) {
        const Id curFocused = m_focus.GetFocusedId();
        const bool overOwnPopup = (m_overlays.GetOpenPopupId() == curFocused) && IsPointInOpenPopup(m_mousePos);
        if (!overOwnPopup) {
            const Rect* focusedBounds = m_hitTree.FindPrevBoundsPtr(curFocused);
            if (focusedBounds == nullptr || !focusedBounds->Contains(m_mousePos)) {
                if (m_editState.id == curFocused) {
                    FlushActiveEditHistory();
                    m_editState.id = Id::Null();
                }
                m_focus.SetFocusedId(Id::Null());
                m_focus.SetFocusVisible(false);
                m_suspendDirectEditInput = false;
            }
        }
    }

    m_arena.reset();
    m_childIdCounts.clear();
    m_containerStack.clear();
    m_zoneStack.clear();
    m_runningOverlayHitBuilder = false;
    m_overlayBuilderZLayer = 0;
    m_overlayBuilderPortalId = Id::Null();
    m_activeOverlayHitCheckId = Id::Null();
    m_activeOverlayHitCheckLayer = OverlayLayer::Popup;
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

ScopedId::~ScopedId() noexcept {
    PopIfNeeded();
}

void ScopedId::PopIfNeeded() noexcept {
    if (m_ctx != nullptr) {
        m_ctx->PopId();
        m_ctx = nullptr;
    }
}

ScopedId Context::IdScope(keel::StringRef key) {
    PushId(key);
    return ScopedId(this);
}

ScopedId Context::IdScope(keel::u64 key) {
    PushId(key);
    return ScopedId(this);
}

ScopedId Context::IdScope(keel::i64 key) {
    PushId(key);
    return ScopedId(this);
}

ScopedId Context::IdScope(const void* ptr) {
    PushId(ptr);
    return ScopedId(this);
}

void Context::PushId(keel::StringRef key) {
    const keel::u64 base = m_idStack.empty() ? detail::Fnv1aOffsetBasis : m_idStack.back();
    m_idStack.push_back(detail::HashStringView(key, base));
}

void Context::PushId(keel::u64 key) {
    const keel::u64 base = m_idStack.empty() ? detail::Fnv1aOffsetBasis : m_idStack.back();
    m_idStack.push_back(detail::HashInt64(key, base));
}

void Context::PushId(keel::i64 key) {
    PushId(keel::as<keel::u64>(key));
}

void Context::PushId(const void* ptr) {
    PushId(keel::as<keel::u64>(reinterpret_cast<keel::usize>(ptr)));
}

void Context::PopId() noexcept {
    if (!m_idStack.empty()) {
        m_idStack.pop_back();
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
    const Id effectiveParent = m_idStack.empty()
        ? parentId
        : Id(detail::HashArgs(parentId.value, m_idStack.back()));
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
        effectiveParent, fileHash, loc.line(), loc.column()
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

    const Rect prevUnclipped = m_hitTree.GetPrevUnclippedBounds(id);
    const Rect* pClipped = m_hitTree.FindPrevBoundsPtr(id);
    if (prevUnclipped.w > 0.0f || prevUnclipped.h > 0.0f) {
        res.rect = CurrentSpace().Invert(prevUnclipped);
    } else if (pClipped != nullptr) {
        res.rect = CurrentSpace().Invert(*pClipped);
    }

    // Suppress interaction on background widgets occluded by floating popups, modals, splitter sashes, or higher Z-layer panels
    if (IsMouseOccludedInFrame(id, m_mousePos)) {
        if (m_mouseReleased && m_focus.GetActiveId() == id) {
            m_focus.SetActiveId(Id::Null());
        }
        return res;
    }

    if (pClipped != nullptr) {
        res.hovered = pClipped->Contains(m_mousePos);
    }

    const Id curActive = m_focus.GetActiveId();
    if (!m_mouseClicked && !m_rightMouseClicked && !m_middleMouseClicked &&
        (m_mouseDown || m_rightMouseDown || m_middleMouseDown) &&
        curActive.IsValid() && curActive != id) {
        res.hovered = false;
    }

    if (res.hovered) {
        m_focus.SetHoveredId(id);
        if (m_mouseClicked) {
            m_focus.SetActiveId(id);
        }
        if (m_mouseDoubleClicked) {
            res.doubleClicked = true;
        }
        if (m_rightMouseClicked) {
            res.rightClicked = true;
        }
    }

    const Id activeNow = m_focus.GetActiveId();
    res.pressed = (activeNow == id && m_mouseDown);
    res.clicked = (activeNow == id && m_mouseReleased && res.hovered);

    if (m_mouseReleased && activeNow == id) {
        m_focus.SetActiveId(Id::Null());
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
    return m_hitTree.GetNodeSpace(id);
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
    if (const Rect* r = m_hitTree.FindPrevBoundsPtr(id)) {
        return CurrentSpace().Invert(*r);
    }
    return Rect{};
}

Rect Context::GetPrevUnclippedBounds(Id id) const {
    const Rect r = m_hitTree.GetPrevUnclippedBounds(id);
    if (r.w > 0.0f || r.h > 0.0f) {
        return CurrentSpace().Invert(r);
    }
    return GetPrevFrameBounds(id);
}

Rect Context::GetParentPrevFrameBounds() const {
    keel::i32 startIdx = -1;
    Rect r{};
    for (keel::i32 i = keel::as<keel::i32>(m_containerStack.size()) - 1; i >= 0; --i) {
        const Node* parent = m_containerStack[keel::as<keel::usize>(i)];
        if (!parent || !parent->id.IsValid()) continue;
        const Rect* found = m_hitTree.FindPrevBoundsPtr(parent->id);
        if (found != nullptr && found->w > 1.0f && found->h > 1.0f) {
            startIdx = i;
            r = *found;
            break;
        }
    }
    if (startIdx < 0) {
        return Rect{};
    }

    const keel::i32 stackSize = keel::as<keel::i32>(m_containerStack.size());
    for (keel::i32 i = startIdx; i < stackSize; ++i) {
        const Node* c = m_containerStack[keel::as<keel::usize>(i)];
        if (!c) continue;

        const keel::f32 localToFrame = GetNodeSpace(c->id).scale;                        // c's own units
        const keel::f32 outerToFrame = localToFrame / c->GetRelativeScale();             // c's parent's units
        if (i > startIdx) {
            if (c->heightMode == SizeMode::Fixed && c->fixedHeight > 0.0f) {
                const keel::f32 h = keel::min(r.h, c->fixedHeight * outerToFrame);
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
                ? m_containerStack[keel::as<keel::usize>(i + 1)]
                : c->lastChild;
            for (const Node* ch = c->firstChild; ch != nullptr && ch != stopChild; ch = ch->nextSibling) {
                if (ch->heightMode == SizeMode::Fixed && ch->fixedHeight > 0.0f && !ch->IsCollapsed()) {
                    const keel::f32 consumeH = (ch->fixedHeight + c->gap) * localToFrame;
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
            const Rect* found = m_hitTree.FindPrevBoundsPtr(n->id);
            if (found != nullptr && found->w > 1.0f && found->h > 1.0f) {
                Rect boxRect = *found;
                const keel::f32 border = n->borderWidth * GetNodeSpace(n->id).scale;
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
        if (node->panelHooks && node->panelHooks->onPopContainer) {
            node->panelHooks->onPopContainer(*this, node);
        }
    }
}

keel::StringRef Context::String(keel::StringRef text) {
    if (text.empty()) return keel::StringRef{};
    return m_arena.alloc_string(text);
}

keel::StringRef Context::AllocString(keel::StringRef text) {
    if (text.empty()) return keel::StringRef{};
    return m_arena.alloc_string(text);
}

void Context::RequestFocus(Id id, FocusReason reason) {
    if (m_focus.GetFocusedId() != id && m_editState.id.IsValid() && m_editState.id != id) {
        FlushActiveEditHistory();
        m_editState.id = Id::Null();
    }
    m_focus.SetFocusedId(id);
    if (!id.IsValid()) {
        m_focus.SetFocusVisible(false);
    } else if (reason == FocusReason::Keyboard) {
        m_focus.SetInputModality(InputModality::Keyboard);
        m_focus.SetFocusVisible(true);
    } else if (reason == FocusReason::Pointer) {
        m_focus.SetInputModality(InputModality::Pointer);
        m_focus.SetFocusVisible(false);
    } else {
        m_focus.SetFocusVisible(m_focus.GetInputModality() == InputModality::Keyboard);
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

void Context::RequestRepaint(keel::i32 frames) noexcept {
    if (frames <= 0) return;
    const keel::i32 target = (!m_containerStack.empty() && frames == 1) ? 2 : frames;
    m_repaintFrames = keel::max(m_repaintFrames, target);
}

bool Context::NeedsRepaint() const noexcept {
    if (m_repaintFrames > 0) return true;
    if (m_mouseDown || m_rightMouseDown || m_middleMouseDown) return true;
    if (m_focus.GetActiveId().IsValid()) return true;
    if (m_overlays.AnyHookNeedsRepaint(*this)) return true;
    return false;
}

keel::i32 Context::GetRecommendedSleepTimeoutMs() const noexcept {
    if (NeedsRepaint()) {
        return 0;
    }
    if (m_editState.id.IsValid() && m_cursorConfig.mode == CursorBlinkMode::Blinking) {
        return 120; // Periodically wake up for smooth cursor blinking
    }
    if (m_stateStore.HasUntrimmedInactive() && (m_idleHeartbeatMs <= 0 || m_idleHeartbeatMs > 500)) {
        return 500;
    }
    return m_idleHeartbeatMs; // User-configurable heartbeat interval (or <= 0 for deep sleep)
}

void Context::OnMouseMove(Vec2 pos) {
    const keel::f32 scale = m_pixelsPerDp;
    const Vec2 newPos = pos / scale;
    m_mousePosPx = pos;
    if (keel::abs(newPos.x - m_mousePos.x) < 0.25f && keel::abs(newPos.y - m_mousePos.y) < 0.25f) {
        return;
    }
    m_mousePos = newPos;
    RequestRepaint(1);
    if (m_inspector.IsInspectMode() || m_inspector.IsAnyScopedInspectActive()) {
        InvalidateAll();
    }
}

void Context::OnMouseDown(keel::i32 button) {
    if (button == 1 || button == 2 || button == 3) {
        m_focus.SetInputModality(InputModality::Pointer);
        m_focus.SetFocusVisible(false);
    }
    if (button == 1) { // Left mouse button
        m_mouseDown = true;
        m_mouseClicked = true;
        const keel::f64 now = m_currentTimeSec;
        const keel::f32 dx = m_mousePos.x - m_lastClickPos.x;
        const keel::f32 dy = m_mousePos.y - m_lastClickPos.y;
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

void Context::OnMouseWheel(keel::f32 x, keel::f32 y) {
    m_mouseWheel = {x, y};
    RequestRepaint(3);
}

bool Context::IsFocused(Id id) const noexcept {
    if (!id.IsValid() || m_focus.GetFocusedId() != id) return false;
    const Id openPopup = m_overlays.GetOpenPopupId();
    const Id openPopupPortal = m_overlays.GetOpenPopupPortalId();
    const bool insideOpenPopupPortal = openPopupPortal.IsValid() && CurrentPortalId() == openPopupPortal;
    if (openPopup.IsValid() && openPopup != id && !insideOpenPopupPortal) return false;
    if (HasModalAboveCurrentScope()) return false;
    return true;
}

bool Context::IsDirectEditAllowed() const noexcept {
    const Id curFocused = m_focus.GetFocusedId();
    if (m_suspendDirectEditInput || m_mouseClicked || !curFocused.IsValid() || m_editState.id != curFocused) {
        return false;
    }
    const Id openPopup = m_overlays.GetOpenPopupId();
    if (openPopup.IsValid() && openPopup != curFocused) {
        const Id openPopupPortal = m_overlays.GetOpenPopupPortalId();
        const HitNodeRecord* focHit = openPopupPortal.IsValid() ? m_hitTree.FindPrevHitNode(curFocused) : nullptr;
        if (!focHit || focHit->portalId != openPopupPortal) {
            return false;
        }
    }
    const Id topModalId = m_overlays.GetPrevFrameModalId();
    if (topModalId.IsValid() && !m_focus.IsInsideModal(topModalId, curFocused)) {
        return false;
    }
    return true;
}

void Context::OnTextInput(keel::StringRef utf8Text) {
    m_ime.Clear();
    ResetCursorBlink();
    RequestRepaint(3);
    if (IsDirectEditAllowed()) {
        m_editState.Insert(utf8Text);
    } else {
        m_pendingTextInput.push_back(keel::String::from(utf8Text));
    }
}

void Context::OnTextComposition(keel::StringRef preeditText, keel::usize cursor, keel::usize selectionLength) {
    ResetCursorBlink();
    RequestRepaint(3);
    if (preeditText.empty()) {
        m_ime.Clear();
    } else {
        m_ime.text.assign(preeditText);
        m_ime.cursor = keel::min(cursor, m_ime.text.size());
        m_ime.selectionLength = keel::min(selectionLength, m_ime.text.size() - m_ime.cursor);
        m_ime.active = true;
    }
    InvalidateAll();
}

void Context::OnTextCompositionEnd(keel::StringRef committedText) {
    m_ime.Clear();
    if (!committedText.empty()) {
        OnTextInput(committedText);
    } else {
        RequestRepaint(2);
        InvalidateAll();
    }
}

void Context::OnFilesDropped(keel::ArrayRef<keel::StringRef> paths, Vec2 posPx) {
    m_droppedFiles.clear();
    m_droppedFiles.reserve(paths.size());
    for (keel::StringRef p : paths) {
        if (!p.empty()) {
            m_droppedFiles.push_back(keel::String::from(p));
        }
    }
    const keel::f32 scale = (m_pixelsPerDp > 0.0f) ? m_pixelsPerDp : 1.0f;
    m_droppedFilesPos = posPx / scale;
    RequestRepaint(3);
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
        m_focus.SetInputModality(InputModality::Keyboard);
        if (m_focus.GetFocusedId().IsValid()) {
            m_focus.SetFocusVisible(true);
        }
    }
    ResetCursorBlink();
    RequestRepaint(3);

    if (m_keymap.Matches(KeyAction::Cancel, key, mods) && m_overlays.GetOpenPopupId().IsValid()) {
        if (IsDirectEditAllowed() && m_overlays.GetOpenPopupPortalId().IsValid()) {
            bool lostFocus = false;
            if (m_editState.HandleKey(*this, key, mods, lostFocus)) {
                if (lostFocus) {
                    m_focus.SetFocusedId(Id::Null());
                }
                return;
            }
        }
        ClosePopup();
        return;
    }

    if (IsDirectEditAllowed()) {
        bool lostFocus = false;
        if (m_editState.HandleKey(*this, key, mods, lostFocus)) {
            if (lostFocus) {
                m_focus.SetFocusedId(Id::Null());
            }
            return;
        }
    }
    m_pendingKeyEvents.push_back({key, mods});
}

void Context::OpenPopup(Id id, Rect bounds, Rect anchorRect, Id portalId) {
    const Rect frameBounds = CurrentSpace().Apply(bounds);
    const Rect anchorFrameBounds = (anchorRect.w > 0.0f && anchorRect.h > 0.0f)
        ? CurrentSpace().Apply(anchorRect)
        : Rect{};
    keel::SmallArray<Rect, 16> invalidated;
    m_overlays.OpenPopup(id, frameBounds, anchorFrameBounds, invalidated, portalId);
    for (const Rect& r : invalidated) {
        InvalidateFrameRect(r);
    }
}

void Context::AddOpenPopupRect(Rect bounds) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }
    m_overlays.AddOpenPopupRect(CurrentSpace().Apply(bounds));
}

bool Context::IsPointInOpenPopup(Vec2 framePos) const noexcept {
    return m_overlays.IsPointInOpenPopup(framePos);
}

void Context::ClosePopup() {
    const Id portalId = m_overlays.GetOpenPopupPortalId();
    keel::SmallArray<Rect, 16> invalidated;
    if (m_overlays.ClosePopup(invalidated)) {
        if (portalId.IsValid()) {
            ClearPopoverBlock(portalId);
        }
        for (const Rect& r : invalidated) {
            InvalidateFrameRect(r);
        }
        RequestRepaint(2);
    }
}

void Context::InvalidateAll() noexcept {
    m_damage.InvalidateAll();
    RequestRepaint(2);
}

void Context::InvalidateRect(Rect rect) noexcept {
    InvalidateFrameRect(CurrentSpace().Apply(rect));
}

void Context::InvalidateFrameRect(Rect frameRect) noexcept {
    if (frameRect.w > 0.0f && frameRect.h > 0.0f) {
        m_damage.InvalidateFrameRect(frameRect);
        RequestRepaint(2);
    }
}

void Context::RegisterOverlayHitRegion(Id id, Rect rect, OverlayLayer layer) {
    if (id.IsValid() && rect.w > 0.0f && rect.h > 0.0f) {
        const keel::u8 scopeZ = m_runningOverlayHitBuilder ? m_overlayBuilderZLayer : CurrentScopeZLayer();
        const Id ownerPortal = m_runningOverlayHitBuilder ? m_overlayBuilderPortalId : CurrentPortalId();
        const bool isGlobalTopOverlay = (keel::as<keel::u8>(layer) >= keel::as<keel::u8>(OverlayLayer::Popup));
        const keel::u8 effectiveZ = isGlobalTopOverlay
            ? keel::as<keel::u8>(layer)
            : keel::max<keel::u8>(keel::as<keel::u8>(layer), scopeZ);
        m_overlays.AddOverlayHitRegion(OverlayHitRegion{
            .id = id,
            .rect = CurrentSpace().Apply(rect),
            .layer = layer,
            .zLayer = effectiveZ,
            .ownerPortalId = isGlobalTopOverlay ? Id::Null() : ownerPortal
        });
    }
}

keel::u8 Context::CurrentScopeZLayer() const noexcept {
    keel::u8 zLayer = m_overlays.IsInModalScope() ? 2 : 0;
    for (const Node* n : m_containerStack) {
        if (!n) continue;
        if (n->IsOutOfFlow()) {
            zLayer = keel::max<keel::u8>(zLayer, 2);
        } else if (n->type == NodeType::Sidebar ||
                   n->type == NodeType::BottomPanel ||
                   n->type == NodeType::StatusBar) {
            zLayer = keel::max<keel::u8>(zLayer, 1);
        }
    }
    return zLayer;
}

Id Context::CurrentPortalId() const noexcept {
    for (keel::usize i = m_containerStack.size(); i > 0; --i) {
        const Node* n = m_containerStack[i - 1];
        if (n && n->IsOutOfFlow() && n->id.IsValid()) {
            return n->id;
        }
    }
    return m_overlays.GetActiveModalId();
}

void Context::BeginModalScope(Id id, Rect backdropRect) noexcept {
    const Id curFocused = m_focus.GetFocusedId();
    if (!m_overlays.IsInModalScope() && curFocused.IsValid()) {
        if (m_focus.IsInCurrentFocusables(curFocused)) {
            if (!m_focus.GetSavedFocusBeforeModal().IsValid()) {
                m_focus.SetSavedFocusBeforeModal(curFocused);
            }
            if (m_editState.id == curFocused) {
                FlushActiveEditHistory();
                m_editState.id = Id::Null();
            }
            m_focus.SetFocusedId(Id::Null());
            m_suspendDirectEditInput = false;
            m_pendingTextInput.clear();
        }
    }
    m_overlays.PushModal(id, CurrentSpace().Apply(backdropRect));
}

void Context::EndModalScope(bool stillOpen, Id id) noexcept {
    const Id closedId = m_overlays.PopModal(stillOpen, id);
    if (!stillOpen) {
        m_focus.ClearModalFocusables(closedId);
        InvalidateAll();
    }
}

void Context::ClearModalBlock(Id id) noexcept {
    m_overlays.ClearModalBlock(id);
    m_focus.ClearModalFocusables(id);
}

void Context::ClearPopoverBlock(Id id) noexcept {
    m_hitTree.ClearPopoverBlock(id);
}

bool Context::HasPrevFrameModal(Id id) const noexcept {
    return m_overlays.HasPrevFrameModal(id);
}

bool Context::IsTopModal(Id id) const noexcept {
    return m_overlays.IsTopModal(id);
}

bool Context::HasModalAboveCurrentScope() const noexcept {
    return m_overlays.HasModalAboveCurrentScope();
}

bool Context::HasPrevFramePopoverOver(Id modalId) const noexcept {
    if (!modalId.IsValid()) return false;
    const HitNodeRecord* modalRec = m_hitTree.FindPrevHitNode(modalId);
    if (modalRec == nullptr) return false;
    for (const auto& b : m_hitTree.GetPrevHitNodes()) {
        if (b.type != NodeType::Popover || !b.blocksHit) continue;
        const bool isDescendant = (modalRec->preIndex < b.preIndex && b.postIndex <= modalRec->postIndex);
        const bool isAfter = (b.preIndex > modalRec->postIndex);
        if (isDescendant || isAfter) {
            return true;
        }
    }
    return false;
}

bool Context::IsModalBlockingInFrame(Vec2 pos, Id exemptModalId) const noexcept {
    return m_overlays.IsModalBlockingInFrame(pos, exemptModalId);
}

bool Context::IsPopoverBlockingInFrame(Vec2 pos, Id ownerPortalOverride) const noexcept {
    const Id activePortalId = ownerPortalOverride.IsValid() ? ownerPortalOverride : CurrentPortalId();
    const HitNodeRecord* activePortalRec = activePortalId.IsValid()
        ? m_hitTree.FindPrevHitNode(activePortalId)
        : nullptr;

    for (const auto& b : m_hitTree.GetPrevHitNodes()) {
        if (b.type != NodeType::Popover || !b.blocksHit || !b.rect.Contains(pos)) {
            continue;
        }
        if (activePortalId.IsValid()) {
            if (activePortalId == b.id) {
                continue;
            }
            if (activePortalRec != nullptr) {
                const bool bIsDescendant = (activePortalRec->preIndex < b.preIndex && b.postIndex <= activePortalRec->postIndex);
                const bool bIsAfter = (b.preIndex > activePortalRec->postIndex);
                if (!bIsDescendant && !bIsAfter) {
                    continue;
                }
            } else {
                continue;
            }
        }
        return true;
    }
    return false;
}

bool Context::IsOverlayOccludedInFrame(Id id, Vec2 pos, OverlayLayer layer) const noexcept {
    const keel::u8 layerRank = keel::as<keel::u8>(layer);

    const Id openPopup = m_overlays.GetOpenPopupId();
    const Id openPopupPortal = m_overlays.GetOpenPopupPortalId();
    if (layerRank <= keel::as<keel::u8>(OverlayLayer::Popup) &&
        openPopup.IsValid() && openPopup != id && openPopupPortal != id && IsPointInOpenPopup(pos)) {
        return true;
    }
    for (const auto& ov : m_overlays.GetPrevOverlayHitRegions()) {
        if (keel::as<keel::u8>(ov.layer) > layerRank && ov.id != id && ov.rect.Contains(pos)) {
            return true;
        }
    }

    if (layerRank < keel::as<keel::u8>(OverlayLayer::Popup)) {
        const Id exemptModal = (layer == OverlayLayer::Portal) ? m_overlays.FindModalIdInPrevOrCurr(id) : Id::Null();
        if (IsModalBlockingInFrame(pos, exemptModal)) {
            return true;
        }
        if (layer == OverlayLayer::Panel) {
            if (IsPopoverBlockingInFrame(pos)) {
                return true;
            }
        } else if (layer == OverlayLayer::Portal && exemptModal.IsValid()) {
            if (IsPopoverBlockingInFrame(pos, exemptModal)) {
                return true;
            }
        }
    }

    return false;
}

bool Context::IsMouseOccluded(Id id, Vec2 pos) const noexcept {
    return IsMouseOccludedInFrame(id, CurrentSpace().Apply(pos));
}

bool Context::IsMouseOccludedInFrame(Id id, Vec2 pos) const noexcept {
    const Id openPopup = m_overlays.GetOpenPopupId();
    const Id openPopupPortal = m_overlays.GetOpenPopupPortalId();
    const bool insideOpenPopupPortal = openPopupPortal.IsValid() && CurrentPortalId() == openPopupPortal;
    if (openPopup.IsValid() && id != openPopup && !insideOpenPopupPortal && IsPointInOpenPopup(pos)) {
        return true;
    }
    if (IsModalBlockingInFrame(pos)) {
        return true;
    }
    const Id curActive = m_focus.GetActiveId();
    if (curActive.IsValid() && curActive == id) {
        return false;
    }
    if (IsPopoverBlockingInFrame(pos)) {
        return true;
    }

    const HitNodeRecord* hitA = m_hitTree.FindPrevHitNode(id);
    keel::u8 targetZLayer = CurrentScopeZLayer();
    Id targetPortalId = CurrentPortalId();
    if (hitA != nullptr) {
        targetZLayer = keel::max<keel::u8>(targetZLayer, hitA->zLayer);
        if (!targetPortalId.IsValid()) {
            targetPortalId = hitA->portalId;
        }
    }

    for (const auto& ov : m_overlays.GetPrevOverlayHitRegions()) {
        if (curActive.IsValid() && curActive == ov.id) {
            return ov.id != id;
        }
        if (OverlayManager::DoesOverlayRegionOcclude(ov, id, targetZLayer, targetPortalId, pos)) {
            return true;
        }
    }

    const auto prevHitNodes = m_hitTree.GetPrevHitNodes();
    auto checkOccludedByNodes = [&](const HitNodeRecord& target) -> bool {
        for (const auto& b : prevHitNodes) {
            if (!b.blocksHit || b.id == target.id || !b.rect.Contains(pos)) {
                continue;
            }
            if (b.preIndex < target.preIndex && b.postIndex >= target.postIndex) {
                continue;
            }
            const bool bIsDescendant = (target.preIndex < b.preIndex && b.postIndex <= target.postIndex);
            if (bIsDescendant) {
                if (target.type == NodeType::Scene) {
                    return true;
                }
                continue;
            }
            if (b.zLayer > target.zLayer) {
                return true;
            }
            if (b.zLayer == target.zLayer && target.type == NodeType::Scene && b.preIndex > target.postIndex) {
                return true;
            }
        }
        return false;
    };

    if (hitA != nullptr) {
        return checkOccludedByNodes(*hitA);
    }

    for (keel::usize sIdx = m_containerStack.size(); sIdx > 0; --sIdx) {
        const Node* cNode = m_containerStack[sIdx - 1];
        if (!cNode || !cNode->id.IsValid()) continue;
        if (const HitNodeRecord* hitC = m_hitTree.FindPrevHitNode(cNode->id)) {
            return checkOccludedByNodes(*hitC);
        }
    }
    return false;
}

bool Context::IsRectHovered(Id id, const Rect& rect) const noexcept {
    if (!CurrentSpace().Apply(rect).Contains(m_mousePos)) {
        return false;
    }
    if (id.IsValid()) {
        const Rect* prevRect = m_hitTree.FindPrevBoundsPtr(id);
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
    const Id curActive = m_focus.GetActiveId();
    if (m_activeOverlayHitCheckId.IsValid()) {
        if (curActive.IsValid() && curActive == m_activeOverlayHitCheckId) {
            return false;
        }
        return IsOverlayOccludedInFrame(m_activeOverlayHitCheckId, m_mousePos, m_activeOverlayHitCheckLayer);
    }

    const Id openPopupPortal = m_overlays.GetOpenPopupPortalId();
    const bool insideOpenPopupPortal = openPopupPortal.IsValid() && CurrentPortalId() == openPopupPortal;
    if (m_overlays.GetOpenPopupId().IsValid() && !insideOpenPopupPortal && IsPointInOpenPopup(m_mousePos)) {
        return true;
    }
    if (IsModalBlockingInFrame(m_mousePos)) {
        return true;
    }

    const bool clickImpulse = m_mouseClicked || m_rightMouseClicked || m_middleMouseClicked;

    for (const auto& ov : m_overlays.GetPrevOverlayHitRegions()) {
        if (curActive.IsValid() && curActive == ov.id) {
            return true;
        }
    }

    const bool hasActiveCapture = curActive.IsValid() && !clickImpulse;
    const keel::u8 currentZLayer = CurrentScopeZLayer();
    const Id currentPortalId = CurrentPortalId();

    if (!hasActiveCapture) {
        for (const auto& ov : m_overlays.GetPrevOverlayHitRegions()) {
            if (OverlayManager::DoesOverlayRegionOcclude(ov, Id::Null(), currentZLayer, currentPortalId, m_mousePos)) {
                return true;
            }
        }
        if (IsPopoverBlockingInFrame(m_mousePos)) {
            return true;
        }
    }

    if (currentZLayer < 2 && !hasActiveCapture) {
        for (const auto& b : m_hitTree.GetPrevHitNodes()) {
            if (b.blocksHit && b.zLayer > currentZLayer && b.rect.Contains(m_mousePos)) {
                return true;
            }
        }
    }

    if (m_widgetExecDepth == 0 && !m_containerStack.empty()) {
        const Node* topContainer = m_containerStack.back();
        if (topContainer && topContainer->type == NodeType::Scene && topContainer->id.IsValid()) {
            const Id sceneId = topContainer->id;
            if (hasActiveCapture && curActive != sceneId) {
                return true;
            }
            if (IsMouseOccludedInFrame(sceneId, m_mousePos)) {
                return true;
            }
            if (topContainer->scroll && (topContainer->scroll->activeY || topContainer->scroll->activeX)) {
                return true;
            }
            if (topContainer->scroll && (topContainer->scroll->hoveredY || topContainer->scroll->hoveredX) && curActive != sceneId) {
                return true;
            }
            const Rect* vpRect = m_hitTree.FindPrevBoundsPtr(sceneId);
            if (vpRect != nullptr && vpRect->w > 0.1f && vpRect->h > 0.1f) {
                if (!vpRect->Contains(m_mousePos) && curActive != sceneId) {
                    return true;
                }
            }
        }
    }

    return false;
}

void Context::SetTooltip(UiText tooltip, Rect anchorRect) {
    if (tooltip.empty() || m_overlays.GetOpenPopupId().IsValid() || IsModalBlockingInFrame(m_mousePos) || IsPopoverBlockingInFrame(m_mousePos)) {
        return;
    }
    m_overlays.SetTooltip(AllocText(tooltip), CurrentSpace().Apply(anchorRect), PopupViewportInFrame());
}

void Context::RegisterFocusable(Id id, bool disabled) {
    m_focus.RegisterFocusable(id, disabled, m_overlays.GetActiveModalId());
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
    if (!m_editState.undoOps.empty() || !m_editState.redoOps.empty()) {
        TextEditHistory* hist = m_editHistories.get(m_editState.id);
        if (!hist) {
            m_editHistories.put(m_editState.id, TextEditHistory{});
            hist = m_editHistories.get(m_editState.id);
        }
        if (hist) {
            hist->lastText.assign(m_editState.text.as_ref());
            hist->undoOps = keel::move(m_editState.undoOps);
            hist->redoOps = keel::move(m_editState.redoOps);
        }
        m_editState.undoOps.clear();
        m_editState.redoOps.clear();
    } else if (TextEditHistory* existing = m_editHistories.get(m_editState.id)) {
        existing->lastText.assign(m_editState.text.as_ref());
        existing->undoOps.clear();
        existing->redoOps.clear();
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
        m_ime.Clear();
        m_editState.id = id;
        m_editState.text.assign(initialText);
        m_editState.cursor = m_editState.text.size();
        m_editState.selectionAnchor = m_editState.cursor;
        m_editState.undoOps.clear();
        m_editState.redoOps.clear();
        if (id.IsValid()) {
            if (TextEditHistory* hist = m_editHistories.get(id)) {
                if (hist->lastText == initialText) {
                    m_editState.undoOps = keel::move(hist->undoOps);
                    m_editState.redoOps = keel::move(hist->redoOps);
                } else {
                    hist->lastText.assign(initialText);
                    hist->undoOps.clear();
                    hist->redoOps.clear();
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
            m_editState.undoOps.clear();
            m_editState.redoOps.clear();
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
                m_focus.SetFocusedId(Id::Null());
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
        m_editState.tabSize = keel::as<keel::u8>(keel::clamp<keel::i32>(tabSize, 1, 16));
        m_editState.insertSpaces = insertSpaces;
    }
}

void Context::ApplyTextEditChange(Id id, keel::StringRef newText, keel::usize newCursor, keel::usize newSelectionAnchor) {
    if (!id.IsValid() || m_editState.id != id || m_editState.readOnly) return;
    m_editState.ApplyDiff(newText, newCursor, newSelectionAnchor);
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

void Context::SetCurrentTime(keel::f64 timeSeconds) noexcept {
    m_manualTime = true;
    m_currentTimeSec = timeSeconds;
}

void Context::AdvanceTime(keel::f64 deltaSeconds) noexcept {
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
            const keel::f64 elapsedSinceInput = m_currentTimeSec - m_lastInputTimeSec;
            // Typing reset phase: cursor remains solid/visible for 0.5s after user interaction
            if (elapsedSinceInput >= 0.0 && elapsedSinceInput < 0.5) {
                return true;
            }
            if (m_cursorConfig.blinkPeriod <= 0.0f) {
                return true;
            }
            const keel::f64 cycleTime = elapsedSinceInput - 0.5;
            const keel::f64 phase = keel::fmod(cycleTime, keel::as<keel::f64>(m_cursorConfig.blinkPeriod));
            return phase < (keel::as<keel::f64>(m_cursorConfig.blinkPeriod) * 0.5);
        }
    }
    return true;
}

bool Context::UpdateScrollAreasRecursive(Node* node) {
    if (!node) return false;

    const bool isModal = (node->type == NodeType::Modal);
    if (isModal) {
        m_overlays.PushModal(node->id, CurrentSpace().Apply(GetNodeContentClipRect(node, node->GetTheme(m_theme))));
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

    if (node->panelHooks && node->panelHooks->update) {
        const Vec2 prevScroll = node->GetScrollOffset();
        ++m_widgetExecDepth;
        node->panelHooks->update(*this, node);
        if (m_widgetExecDepth > 0) {
            --m_widgetExecDepth;
        }
        if (node->IsSolverScrollable() || node->type == NodeType::Carousel || node->type == NodeType::Toolbar) {
            const Vec2 curScroll = node->GetScrollOffset();
            if (keel::abs(curScroll.x - prevScroll.x) > 0.01f ||
                keel::abs(curScroll.y - prevScroll.y) > 0.01f) {
                scrollChanged = true;
            }
        }
    }

    if (pushedToStack && !m_containerStack.empty() && m_containerStack.back() == node) {
        m_containerStack.pop_back();
    }
    if (isModal) {
        (void)m_overlays.PopModal(true, node->id);
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
        if (p->id.IsValid() && p->panelHooks && p->panelHooks->scrollToChild) {
            p->panelHooks->scrollToChild(*this, p, target);
            break;
        }
    }
}

void Context::EndFrame(ICanvas& canvas) {
    m_lastCanvas = &canvas;

    // Balance any unclosed container, zone, identity, and modal scopes
    while (m_containerStack.size() > 1) {
        m_containerStack.pop_back();
    }
    m_zoneStack.clear();
    m_idStack.clear();
    m_overlays.ClearActiveModalStack();

    const Vec2 logicalSize = m_viewportSize / m_pixelsPerDp;

    // Resolve shell styling before pure geometric layout passes
    Solver::ResolveShellStyles(m_root, m_theme);
    Solver::Measure(m_root, &canvas, m_theme);
    Solver::Arrange(m_root, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), m_theme, &canvas);

    // Update interactive scrollbars; re-arrange only if a scrollbar drag or track click modified scrollOffset
    if (UpdateScrollAreasRecursive(m_root)) {
        Solver::Arrange(m_root, Rect(0.0f, 0.0f, logicalSize.x, logicalSize.y), m_theme, &canvas);
    }

    // Run registered end-of-frame overlay hooks (e.g. detached Toast host)
    m_overlays.RunEndFrameHooks(*this);

    // Cache computed bounds, Z-order hit records, overlay hit regions, and focusable widget list for next frame
    m_hitTree.CollectFromTree(m_root, logicalSize, m_theme);
    for (const auto& builder : m_overlays.GetOverlayHitBuilders()) {
        if (builder.invoke) {
            m_spaceOverride = &builder.space;
            m_runningOverlayHitBuilder = true;
            m_overlayBuilderZLayer = builder.scopeZLayer;
            m_overlayBuilderPortalId = builder.ownerPortalId;
            builder.invoke(builder.ctx);
            m_runningOverlayHitBuilder = false;
            m_overlayBuilderZLayer = 0;
            m_overlayBuilderPortalId = Id::Null();
            m_spaceOverride = nullptr;
        }
    }
    m_overlays.ClearOverlayHitBuilders();
    m_overlays.CommitOverlayHitRegions();
    m_hitTree.CommitFrame();
    m_focus.CommitFrame();

    if (const Id portalId = m_overlays.GetOpenPopupPortalId(); portalId.IsValid()) {
        const Rect portalRect = m_hitTree.GetPrevUnclippedBounds(portalId);
        if (portalRect.w > 0.0f && portalRect.h > 0.0f) {
            m_overlays.UpdateOpenPopupRect(portalRect);
        }
    }

    // Suspend focus from occluded background widgets when a Modal opens, and restore it when all Modals close
    if (m_overlays.HasCurrFrameModals()) {
        const Id curFocused = m_focus.GetFocusedId();
        if (curFocused.IsValid()) {
            const Id topModalId = m_overlays.GetCurrFrameModalId();
            if (!m_focus.IsInsideModal(topModalId, curFocused)) {
                if (!m_focus.GetSavedFocusBeforeModal().IsValid()) {
                    m_focus.SetSavedFocusBeforeModal(curFocused);
                }
                if (m_editState.id == curFocused) {
                    FlushActiveEditHistory();
                    m_editState.id = Id::Null();
                }
                m_focus.SetFocusedId(Id::Null());
                m_focus.SetFocusVisible(false);
                m_suspendDirectEditInput = false;
            }
        }
    } else if (m_focus.GetSavedFocusBeforeModal().IsValid()) {
        if (!m_focus.GetFocusedId().IsValid()) {
            const Id saved = m_focus.GetSavedFocusBeforeModal();
            if (m_focus.IsInPrevFocusables(saved)) {
                m_focus.SetFocusedId(saved);
                m_focus.SetFocusVisible(m_focus.GetInputModality() == InputModality::Keyboard);
            }
        }
        m_focus.SetSavedFocusBeforeModal(Id::Null());
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
        m_focus.SetActiveId(Id::Null());
    }

    // Evaluate overlay state transitions and compute minimal disjoint dirty damage rectangles
    const OverlayDamageSummary ovSummary = m_overlays.FinalizeFrameState(*this, m_mousePos);
    const EditSnapshotInfo editInfo{
        .id = m_editState.id,
        .text = m_editState.text.as_ref(),
        .cursor = m_editState.cursor,
        .selectionAnchor = m_editState.selectionAnchor,
        .cursorVisible = IsCursorVisible()
    };
    const OverlayDamageInfo ovDamageInfo{
        .anyOverlayStateChanged = ovSummary.anyOverlayStateChanged,
        .hasActiveFullRepaintOverlay = ovSummary.hasActiveFullRepaintOverlay
    };
    bool anyDebugBounds = false;
    bool anyInspectMode = false;
    m_damage.ComputeFrameDamage(
        m_root,
        logicalSize,
        m_theme,
        editInfo,
        keel::ArrayRef<Rect>(ovSummary.popupDamageRects.data(), ovSummary.popupDamageRects.size()),
        m_inspector.IsDebugBounds(),
        m_inspector.IsInspectMode(),
        m_inspector.HadDebugBoundsLastFrame(),
        m_inspector.HadInspectModeLastFrame(),
        ovDamageInfo,
        anyDebugBounds,
        anyInspectMode
    );
    m_inspector.CommitFrameState(anyDebugBounds, anyInspectMode);

    // Overlays draw (and query the context) in the space they were registered from
    auto invokeOverlay = [&](const OverlayPass& pass, ICanvas& sink) {
        const bool transformed = !pass.space.IsIdentity();
        if (transformed) {
            sink.PushTransform(pass.space);
        }
        m_spaceOverride = &pass.space;
        pass.invoke(pass.ctx, sink);
        m_spaceOverride = nullptr;
        if (transformed) {
            sink.PopTransform();
        }
    };

    auto runOverlayPasses = [&](ICanvas& sink, OverlayLayer targetLayer, const Rect& clipBounds, bool intersectWithClip) {
        for (const auto& pass : m_overlays.GetOverlayPasses()) {
            if (pass.layer != targetLayer) continue;
            if (pass.clipRect.w > 0.0f && pass.clipRect.h > 0.0f) {
                if (intersectWithClip) {
                    const Rect clip = pass.clipRect.Intersect(clipBounds);
                    if (clip.w <= 0.0f || clip.h <= 0.0f) continue;
                    sink.PushClip(clip);
                    invokeOverlay(pass, sink);
                    sink.PopClip();
                } else {
                    if (!pass.clipRect.Intersects(clipBounds)) continue;
                    sink.PushClip(pass.clipRect);
                    invokeOverlay(pass, sink);
                    sink.PopClip();
                }
            } else {
                invokeOverlay(pass, sink);
            }
        }
    };

    // If damage exists, clear target buffer and rasterize scene graph
    if (m_damage.HasDirtyDamage()) {
        const keel::u32 vpW = keel::as<keel::u32>(m_viewportSize.x);
        const keel::u32 vpH = keel::as<keel::u32>(m_viewportSize.y);
        canvas.Begin(vpW, vpH, m_pixelsPerDp);

        auto renderFullPass = [&](ICanvas& sink, const Rect& dirty) {
            sink.PushClip(dirty);
            sink.Clear(m_clearColor);
            RenderNodeRecursive(m_root, sink, dirty, m_theme);
            runOverlayPasses(sink, OverlayLayer::Panel, dirty, false);
            RenderModalsRecursive(m_root, sink, dirty, m_theme);
            runOverlayPasses(sink, OverlayLayer::Popup, dirty, false);
            runOverlayPasses(sink, OverlayLayer::Notification, dirty, false);
            if (ovSummary.hasTooltip) {
                m_overlays.RenderTooltipOverlay(sink, logicalSize, m_mousePos, m_theme);
            }
            if (anyDebugBounds) {
                m_inspector.RenderDebugBounds(m_root, sink, m_theme);
            }
            if (anyInspectMode) {
                m_inspector.RenderInspectOverlay(m_root, sink, logicalSize, m_mousePos, m_theme, m_hitTree);
            }
            sink.PopClip();
        };

        auto renderDirtyRectPass = [&](ICanvas& sink, const Rect& dr) {
            sink.PushClip(dr);
            sink.Clear(m_clearColor, dr);
            RenderNodeRecursive(m_root, sink, dr, m_theme);
            runOverlayPasses(sink, OverlayLayer::Panel, dr, true);
            RenderModalsRecursive(m_root, sink, dr, m_theme);
            runOverlayPasses(sink, OverlayLayer::Popup, dr, true);
            runOverlayPasses(sink, OverlayLayer::Notification, dr, true);
            sink.PopClip();
        };

        if (m_damage.IsFullRepaint()) {
            const Rect dirty = m_damage.GetDirtyRect();
            if (m_drawListEnabled) {
                m_drawList.Reset(&m_arena);
                DrawListRecorder recorder(m_drawList, &canvas);
                recorder.Begin(vpW, vpH, m_pixelsPerDp);
                renderFullPass(recorder, dirty);
                if (recorder.HadPartialFlush()) {
                    recorder.FlushToTarget();
                } else {
                    canvas.Submit(m_drawList);
                }
            } else {
                renderFullPass(canvas, dirty);
            }
            canvas.End();
        } else {
            for (const Rect& dr : m_damage.GetDirtyRects()) {
                if (m_drawListEnabled) {
                    m_drawList.Reset(&m_arena);
                    DrawListRecorder recorder(m_drawList, &canvas);
                    recorder.Begin(vpW, vpH, m_pixelsPerDp);
                    renderDirtyRectPass(recorder, dr);
                    if (recorder.HadPartialFlush()) {
                        recorder.FlushToTarget();
                    } else {
                        canvas.Submit(m_drawList);
                    }
                } else {
                    renderDirtyRectPass(canvas, dr);
                }
                canvas.End();
            }
        }
    }

    m_pendingKeyEvents.clear();
    m_pendingTextInput.clear();
    m_droppedFiles.clear();

    bool trimmedAny = m_stateStore.Maintain(
        m_frameIndex,
        m_currentTimeSec,
        m_repaintFrames,
        m_focus.GetFocusedId(),
        m_focus.GetActiveId()
    );
    if (!m_focus.GetFocusedId().IsValid() && !m_editState.id.IsValid() && m_editState.TrimIdleMemory()) {
        trimmedAny = true;
    }

    if (trimmedAny) {
        keel::memory_trim();
    }

    if (m_repaintFrames > 0) {
        --m_repaintFrames;
    }
    m_containerStack.clear();
}

namespace {

/// Masks outer corner wedges when a rounded clipping container hosts children that actually touch
/// the corner regions (e.g. full-bleed toolbars, titlebars, or sidebars) so rectangular child fills
/// never bleed outside the parent's rounded corners.
void MaskRoundedContainerCorners(
    ICanvas& canvas,
    const Node* node,
    const Rect& childClipRect,
    Color clearColor
) {
    if (!node || !node->clipChildren || node->cornerRadius <= 1.5f || node->firstChild == nullptr) {
        return;
    }

    const Rect& r = node->computedRect;
    const keel::f32 rad = keel::clamp(node->cornerRadius, 0.0f, keel::min(r.w, r.h) * 0.5f);
    if (rad <= 1.5f) {
        return;
    }

    const Rect tlCorner(r.x, r.y, rad, rad);
    const Rect trCorner(r.Right() - rad, r.y, rad, rad);
    const Rect blCorner(r.x, r.Bottom() - rad, rad, rad);
    const Rect brCorner(r.Right() - rad, r.Bottom() - rad, rad, rad);
    bool maskTL = false;
    bool maskTR = false;
    bool maskBL = false;
    bool maskBR = false;
    for (const Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        if (child->IsOutOfFlow()) continue;
        const Rect childBounds = NodeLocalTransform(child).Apply(child->computedRect).Intersect(childClipRect);
        if (childBounds.w <= 0.0f || childBounds.h <= 0.0f) continue;
        if (childBounds.Intersects(tlCorner)) maskTL = true;
        if (childBounds.Intersects(trCorner)) maskTR = true;
        if (childBounds.Intersects(blCorner)) maskBL = true;
        if (childBounds.Intersects(brCorner)) maskBR = true;
        if (maskTL && maskTR && maskBL && maskBR) break;
    }
    if (!(maskTL || maskTR || maskBL || maskBR)) {
        return;
    }

    keel::SmallArray<Color, 8> semiLayers;
    Color outerBg = clearColor;
    for (const Node* anc = node->parent; anc != nullptr; anc = anc->parent) {
        if (anc->backgroundColor.a == 255u) {
            outerBg = anc->backgroundColor;
            break;
        }
        if (anc->backgroundColor.a > 0u) {
            semiLayers.push_back(anc->backgroundColor);
        }
    }
    for (keel::usize li = semiLayers.size(); li > 0u; --li) {
        const Color layer = semiLayers[li - 1u];
        outerBg = Color::Mix255(outerBg, layer.WithAlpha(255u), layer.a);
    }
    if (outerBg.a == 0u) {
        return;
    }

    const keel::i32 steps = keel::ceil_i32(rad);
    for (keel::i32 i = 0; i < steps; ++i) {
        const keel::f32 dy = keel::as<keel::f32>(i);
        const keel::f32 rem = keel::max(0.0f, rad - dy - 0.35f);
        const keel::f32 dx = keel::max(0.0f, rad - keel::sqrt(keel::max(0.0f, rad * rad - rem * rem)));
        if (dx > 0.25f) {
            const keel::f32 w = keel::min(dx + 0.5f, r.w * 0.5f);
            const keel::f32 h = keel::min(1.25f, r.h - dy);
            if (maskTL) canvas.DrawRect(Rect(r.x, r.y + dy, w, h), outerBg);
            if (maskTR) canvas.DrawRect(Rect(r.Right() - w, r.y + dy, w, h), outerBg);
            if (maskBL) canvas.DrawRect(Rect(r.x, r.Bottom() - dy - h, w, h), outerBg);
            if (maskBR) canvas.DrawRect(Rect(r.Right() - w, r.Bottom() - dy - h, w, h), outerBg);
        }
    }
}

} // namespace

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

    // 1. Render container surface background & shadow before children via active IStylePainter
    const bool isContainerSurface = (!node->IsLeaf() && !node->IsOutOfFlow());
    const SurfaceRole nodeSurfRole = node->GetSurfaceRole();
    const SurfaceRole resolvedSurfaceRole = (nodeSurfRole != SurfaceRole::None)
        ? nodeSurfRole
        : ((node->borderColor.a > 0 && node->borderWidth > 0.0f) ? SurfaceRole::Flat : SurfaceRole::None);
    const SurfacePaintParams surfParams{
        .rect = node->computedRect,
        .role = resolvedSurfaceRole,
        .dockSeam = node->GetDockSeam(),
        .radius = node->cornerRadius,
        .explicitRadius = node->explicitRadius != 0,
        .bg = node->backgroundColor,
        .border = node->borderColor,
        .borderWidth = node->borderWidth,
        .elevation = (resolvedSurfaceRole == SurfaceRole::Raised) ? Elevation::Md
                   : (resolvedSurfaceRole == SurfaceRole::Popup ? Elevation::Lg : Elevation::None)
    };
    const bool hasContainerBorder = isContainerSurface && resolvedSurfaceRole != SurfaceRole::None &&
        ((node->borderColor.a > 0 && node->borderWidth > 0.0f) ||
         resolvedSurfaceRole == SurfaceRole::Sunken ||
         resolvedSurfaceRole == SurfaceRole::Raised);
    if (isContainerSurface && (node->backgroundColor.a > 0 || resolvedSurfaceRole == SurfaceRole::Raised || resolvedSurfaceRole == SurfaceRole::Popup)) {
        activeTheme.Painter().DrawSurfaceBg(canvas, activeTheme, surfParams);
    }

    // Render specific widget contents
    if (node->renderFn) {
        node->renderFn(node, canvas, activeTheme);
    }

    keel::f32 borderInset = 0.0f;
    const Rect innerContainerRect = hasContainerBorder
        ? GetNodeInnerBorderRect(node, &borderInset)
        : node->computedRect;

    const bool roundedOverride = (node->explicitRadius != 0) && (node->cornerRadius > 0.01f);
    const bool useBevel = (activeTheme.Painter().GetChromeMetrics(activeTheme).frameBevelInset > 0.0f) && !roundedOverride;
    const keel::f32 effOuterRadius = useBevel ? 0.0f : node->cornerRadius;
    const keel::f32 innerRadius = keel::max(0.0f, effOuterRadius - borderInset);
    const RenderFunc chromeFn = (node->panelHooks && node->panelHooks->renderChrome)
        ? node->panelHooks->renderChrome
        : nullptr;

    Rect childClipRect = node->computedRect;
    Rect childDirtyRect = nodeDirty;
    const bool nativeRoundedClip = canvas.SupportsRoundedClip();
    bool pushedOuterClip = false;
    bool pushedChildClip = false;

    if (node->clipChildren) {
        childClipRect = GetNodeContentClipRect(node, activeTheme).Intersect(innerContainerRect);
        const bool sameAsInner =
            keel::abs(childClipRect.x - innerContainerRect.x) < 0.01f &&
            keel::abs(childClipRect.y - innerContainerRect.y) < 0.01f &&
            keel::abs(childClipRect.w - innerContainerRect.w) < 0.01f &&
            keel::abs(childClipRect.h - innerContainerRect.h) < 0.01f;

        if (chromeFn != nullptr || (nativeRoundedClip && innerRadius > 0.5f)) {
            if (nativeRoundedClip && innerRadius > 0.5f) {
                canvas.PushRoundedClip(innerContainerRect, innerRadius);
            } else {
                canvas.PushClip(innerContainerRect);
            }
            pushedOuterClip = true;
            if (!sameAsInner) {
                canvas.PushClip(childClipRect);
                pushedChildClip = true;
            }
        } else {
            canvas.PushClip(childClipRect);
            pushedChildClip = true;
        }
        childDirtyRect = nodeDirty.Intersect(childClipRect);
    }

    // Recurse on children (skip out-of-flow overlays during normal tree walk; overlays render in RenderModalsRecursive)
    if (childDirtyRect.w > 0.0f && childDirtyRect.h > 0.0f) {
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->IsOutOfFlow()) continue;
            RenderNodeRecursive(child, canvas, childDirtyRect, activeTheme);
        }
    }

    if (pushedChildClip) {
        canvas.PopClip();
    }

    // Internal container chrome (e.g. ScrollArea scrollbars) renders inside the container's rounded clip & border
    if (chromeFn != nullptr) {
        chromeFn(node, canvas, activeTheme);
    }

    if (pushedOuterClip) {
        canvas.PopClip();
    }

    // Legacy fallback corner mask ONLY for canvases without native PushRoundedClip (e.g. legacy 2D SDL renderer)
    if (isContainerSurface && !nativeRoundedClip) {
        MaskRoundedContainerCorners(canvas, node, innerContainerRect, m_clearColor);
    }

    // 2. Render container border / 3D bevel / directional sheen / dock seam on top of children & internal chrome
    if (hasContainerBorder) {
        activeTheme.Painter().DrawSurfaceFrame(canvas, activeTheme, surfParams);
    }

    // Render post-pass external overlay decorations (after border and outside clip)
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
            const Rect backdropClip = GetNodeContentClipRect(child, activeTheme);
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
    if (m_inspector.SetDebugBounds(enabled)) {
        InvalidateAll();
    }
}

void Context::SetInspectMode(bool enabled) noexcept {
    if (m_inspector.SetInspectMode(enabled)) {
        InvalidateAll();
    }
}

} // namespace guikit
