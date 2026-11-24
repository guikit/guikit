#include "guikit/context.hpp"
#include "guikit/core/style_painter.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/math.h"
#include "keel/static_array.h"
#include "keel/time.h"
#include "keel/utf8.h"
#include <cassert>
#include "keel/memory.h"

namespace guikit {

namespace {

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

} // namespace

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
            ConsumeMouseClick();
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
        const keel::u8 u = keel::as<keel::u8>(c);
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
    if (openPopup.IsValid() && openPopup != id) return false;
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
        return false;
    }
    const Id topModalId = m_overlays.GetPrevFrameModalId();
    if (topModalId.IsValid() && !m_focus.IsInsideModal(topModalId, curFocused)) {
        return false;
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
        m_focus.SetInputModality(InputModality::Keyboard);
        if (m_focus.GetFocusedId().IsValid()) {
            m_focus.SetFocusVisible(true);
        }
    }
    ResetCursorBlink();
    RequestRepaint(3);

    if (m_keymap.Matches(KeyAction::Cancel, key, mods) && m_overlays.GetOpenPopupId().IsValid()) {
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

void Context::OpenPopup(Id id, Rect bounds, Rect anchorRect) {
    const Rect frameBounds = CurrentSpace().Apply(bounds);
    const Rect anchorFrameBounds = (anchorRect.w > 0.0f && anchorRect.h > 0.0f)
        ? CurrentSpace().Apply(anchorRect)
        : Rect{};
    keel::SmallArray<Rect, 16> invalidated;
    m_overlays.OpenPopup(id, frameBounds, anchorFrameBounds, invalidated);
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
    keel::SmallArray<Rect, 16> invalidated;
    if (m_overlays.ClosePopup(invalidated)) {
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
    if (layerRank <= keel::as<keel::u8>(OverlayLayer::Popup) &&
        openPopup.IsValid() && openPopup != id && IsPointInOpenPopup(pos)) {
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
    if (openPopup.IsValid() && id != openPopup && IsPointInOpenPopup(pos)) {
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

    if (m_overlays.GetOpenPopupId().IsValid() && IsPointInOpenPopup(m_mousePos)) {
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

    auto runOverlayPasses = [&](OverlayLayer targetLayer, const Rect& clipBounds, bool intersectWithClip) {
        for (const auto& pass : m_overlays.GetOverlayPasses()) {
            if (pass.layer != targetLayer) continue;
            if (pass.clipRect.w > 0.0f && pass.clipRect.h > 0.0f) {
                if (intersectWithClip) {
                    const Rect clip = pass.clipRect.Intersect(clipBounds);
                    if (clip.w <= 0.0f || clip.h <= 0.0f) continue;
                    canvas.PushClip(clip);
                    invokeOverlay(pass);
                    canvas.PopClip();
                } else {
                    if (!pass.clipRect.Intersects(clipBounds)) continue;
                    canvas.PushClip(pass.clipRect);
                    invokeOverlay(pass);
                    canvas.PopClip();
                }
            } else {
                invokeOverlay(pass);
            }
        }
    };

    // If damage exists, clear target buffer and rasterize scene graph
    if (m_damage.HasDirtyDamage()) {
        canvas.Begin(keel::as<keel::u32>(m_viewportSize.x), keel::as<keel::u32>(m_viewportSize.y), m_pixelsPerDp);

        if (m_damage.IsFullRepaint()) {
            const Rect dirty = m_damage.GetDirtyRect();
            canvas.PushClip(dirty);
            canvas.Clear(m_clearColor);
            RenderNodeRecursive(m_root, canvas, dirty, m_theme);
            runOverlayPasses(OverlayLayer::Panel, dirty, false);
            RenderModalsRecursive(m_root, canvas, dirty, m_theme);
            runOverlayPasses(OverlayLayer::Popup, dirty, false);
            runOverlayPasses(OverlayLayer::Notification, dirty, false);
            if (ovSummary.hasTooltip) {
                m_overlays.RenderTooltipOverlay(canvas, logicalSize, m_mousePos, m_theme);
            }
            if (anyDebugBounds) {
                m_inspector.RenderDebugBounds(m_root, canvas, m_theme);
            }
            if (anyInspectMode) {
                m_inspector.RenderInspectOverlay(m_root, canvas, logicalSize, m_mousePos, m_theme, m_hitTree);
            }

            canvas.PopClip();
            canvas.End();
        } else {
            for (const Rect& dr : m_damage.GetDirtyRects()) {
                canvas.PushClip(dr);
                canvas.Clear(m_clearColor, dr);
                RenderNodeRecursive(m_root, canvas, dr, m_theme);
                runOverlayPasses(OverlayLayer::Panel, dr, true);
                RenderModalsRecursive(m_root, canvas, dr, m_theme);
                runOverlayPasses(OverlayLayer::Popup, dr, true);
                runOverlayPasses(OverlayLayer::Notification, dr, true);
                canvas.PopClip();
                canvas.End();
            }
        }
    }

    m_pendingKeyEvents.clear();
    m_pendingTextInput.clear();

    bool trimmedAny = m_stateStore.Maintain(
        m_frameIndex,
        m_currentTimeSec,
        m_repaintFrames,
        m_focus.GetFocusedId(),
        m_focus.GetActiveId()
    );
    if (!m_focus.GetFocusedId().IsValid() && !m_editState.id.IsValid() &&
        (m_editState.text.capacity() > 4096 || !m_editState.undoStack.empty() || !m_editState.redoStack.empty())) {
        m_editState.text = {};
        m_editState.undoStack = {};
        m_editState.redoStack = {};
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
        const keel::f32 rad = keel::clamp(node->cornerRadius, 0.0f, keel::min(r.w, r.h) * 0.5f);
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
                    const keel::f32 dy = keel::as<keel::f32>(i);
                    const keel::f32 rem = keel::max(0.0f, rad - dy - 0.35f);
                    const keel::f32 dx = keel::max(0.0f, rad - keel::sqrt(keel::max(0.0f, rad * rad - rem * rem)));
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
        ContainerFrameRole role = ContainerFrameRole::Generic;
        if (node->type == NodeType::Card) {
            role = (node->renderFn != nullptr) ? ContainerFrameRole::CardElevated : ContainerFrameRole::CardFlat;
        } else if (node->type == NodeType::Sidebar) {
            role = ContainerFrameRole::Sidebar;
        } else if (node->type == NodeType::Toolbar) {
            role = ContainerFrameRole::Toolbar;
        } else if (node->type == NodeType::StatusBar) {
            role = ContainerFrameRole::StatusBar;
        } else if (node->type == NodeType::BottomPanel) {
            role = ContainerFrameRole::BottomPanel;
        }
        activeTheme.Painter().DrawContainerBorder(
            canvas,
            activeTheme,
            role,
            node->computedRect,
            node->cornerRadius,
            node->borderColor,
            node->borderWidth,
            node->IsDockedRight(),
            node->direction == FlexDirection::Column
        );
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
