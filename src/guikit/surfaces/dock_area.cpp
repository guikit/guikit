#include "guikit/surfaces/dock_area.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/solver.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/layout/scroll_area.hpp"
#include "guikit/style/style_painter.hpp"
#include "keel/math.h"

namespace guikit {

// ==============================================================================
// DockArea Immediate-Mode Widget, Layout Solver, DnD & Floating Cards
// ==============================================================================

namespace {

constexpr keel::f32 kMinSplitRatio = 0.05f;
constexpr keel::f32 kMaxSplitRatio = 0.95f;
constexpr keel::f32 kDragStartThresholdDp = 6.0f;
constexpr keel::f32 kFloatResizeEdgeDp = 6.0f;
constexpr keel::f32 kFloatMinWidthDp = 160.0f;
constexpr keel::f32 kFloatMinHeightDp = 110.0f;
constexpr keel::f32 kMinReadableTabWidthDp = 76.0f;
constexpr keel::f32 kOverflowBtnWidthDp = 24.0f;

constexpr keel::u8 kResizeLeft   = 1u << 0;
constexpr keel::u8 kResizeRight  = 1u << 1;
constexpr keel::u8 kResizeTop    = 1u << 2;
constexpr keel::u8 kResizeBottom = 1u << 3;

enum class DockDropKind : keel::u8 {
    None = 0,
    KeepInPlace,
    ReorderTab,
    InsertTab,
    GroupCenter,
    GroupSide,
    RootEdge,
    TearFloat
};

struct DockDropTarget {
    DockDropKind kind{DockDropKind::None};
    keel::u32 targetNodeIdx{kDockInvalidNode};
    DockSide side{DockSide::Center};
    keel::usize insertIndex{0};
    Rect previewRect{};
    Rect groupRect{};
    Rect compassRect{};
    Rect caretRect{};
    bool showCompass{false};
    bool centerIsKeep{false};
};

struct PrevTabGeo {
    DockPanelId panelId{};
    Rect tabRect{};
    Rect closeRect{};
    bool closable{false};
    bool floatable{true};
    bool allowDock{true};
    bool expandable{true};
    bool active{false};
    bool disabled{false};
};

struct PrevGroupGeo {
    keel::u32 nodeIdx{kDockInvalidNode};
    DockPanelId activePanelId{};
    Rect groupRect{};
    Rect tabBarRect{};
    Rect tabStripRect{};
    Rect overflowBtnRect{};
    Rect floatBtnRect{};
    Rect expandBtnRect{};
    Rect contentRect{};
    bool hasOverflow{false};
    bool activeFloatable{true};
    bool activeExpandable{true};
    keel::f32 scrollX{0.0f};
    keel::f32 maxScrollX{0.0f};
    keel::SmallArray<PrevTabGeo, 8> tabs;
};

struct PrevSashGeo {
    keel::u32 nodeIdx{kDockInvalidNode};
    Orientation orientation{Orientation::Horizontal};
    Rect sashRect{};
    Rect parentRect{};
    keel::f32 minFirstSpan{0.0f};
    keel::f32 minSecondSpan{0.0f};
    keel::f32 availSpan{0.0f};
};

struct PrevFloatGeo {
    DockPanelId panelId{};
    Rect cardRect{};
    Rect titleRect{};
    Rect contentRect{};
    Rect expandRect{};
    Rect redockRect{};
    Rect closeRect{};
    keel::u32 zOrder{0u};
    bool closable{false};
    bool allowDock{true};
    bool expandable{true};
    bool minimized{false};
};

struct PaneAnimState {
    DockPanelId keyPanelId{};
    Rect fromRect{};
    Rect currentRect{};
};

struct DockAreaPersistState {
    DockLayout ownedLayout;
    Rect prevInnerRect{};
    keel::Array<PrevGroupGeo> prevGroups;
    keel::Array<PrevSashGeo> prevSashes;
    keel::Array<PrevFloatGeo> prevFloats;

    // Sash interaction
    keel::u32 hoveredSashNode{kDockInvalidNode};
    keel::u32 activeSashNode{kDockInvalidNode};
    Vec2 sashDragStartMouse{};
    keel::f32 sashDragStartRatio{0.5f};

    // Tab hover & drag-and-drop
    DockPanelId hoveredTabId{};
    bool hoveredTabClose{false};
    bool tabDragPending{false};
    bool tabDragging{false};
    bool dragFromFloat{false};
    DockPanelId dragPanelId{};
    keel::u32 dragSourceNode{kDockInvalidNode};
    keel::usize dragSourceIndex{0u};
    Vec2 dragStartMouse{};
    Vec2 dragGrabOffset{};
    bool dragFloatable{true};
    bool dragAllowDock{true};
    DockDropTarget dropTarget{};

    // Floating card interaction
    DockPanelId hoveredFloatId{};
    bool hoveredFloatClose{false};
    bool hoveredFloatRedock{false};
    bool hoveredFloatExpand{false};
    DockPanelId activeFloatMoveId{};
    Vec2 floatDragStartMouse{};
    Rect floatDragStartBounds{};
    DockPanelId activeFloatResizeId{};
    keel::u8 floatResizeMask{0u};
    Rect floatResizeStartBounds{};

    // Tab-bar trailing action button hover state
    keel::u32 hoveredTabFloatNode{kDockInvalidNode};
    keel::u32 hoveredTabExpandNode{kDockInvalidNode};

    // Expanded modal / fullscreen overlay state
    Rect expandedBackdropRect{};
    Rect expandedCardRect{};
    Rect expandedTitleRect{};
    Rect expandedContentRect{};
    Rect expandedFullscreenBtnRect{};
    Rect expandedCloseBtnRect{};
    bool hoveredExpandedFullscreen{false};
    bool hoveredExpandedClose{false};

    // Last-focused / closed panel tracking for current frame
    DockPanelId lastActivePanelId{};

    // Smooth layout transition state
    keel::u64 lastLayoutRevision{0u};
    keel::f64 animStartTimeSec{0.0};
    keel::f64 animDurationSec{0.0};
    bool animating{false};
    keel::Array<PaneAnimState> paneAnims;

    // Tab-bar horizontal scroll & overflow dropdown state
    struct TabScrollEntry {
        keel::u32 nodeIdx{kDockInvalidNode};
        keel::f32 scrollX{0.0f};
    };
    keel::SmallArray<TabScrollEntry, 16> tabScrolls;
    keel::u32 scrollEnsureActiveNode{kDockInvalidNode};
    keel::u32 hoveredOverflowNode{kDockInvalidNode};
    keel::u32 openOverflowNode{kDockInvalidNode};
    Rect openOverflowAnchorRect{};
    Rect openOverflowPopupRect{};
    keel::i32 hoveredOverflowItem{-1};
    bool hoveredOverflowItemClose{false};
};

[[nodiscard]] keel::f32 GetTabScrollX(const DockAreaPersistState& st, keel::u32 nodeIdx) noexcept {
    for (const auto& e : st.tabScrolls) {
        if (e.nodeIdx == nodeIdx) return e.scrollX;
    }
    return 0.0f;
}

void SetTabScrollX(DockAreaPersistState& st, keel::u32 nodeIdx, keel::f32 x) {
    for (auto& e : st.tabScrolls) {
        if (e.nodeIdx == nodeIdx) {
            e.scrollX = x;
            return;
        }
    }
    st.tabScrolls.push_back(DockAreaPersistState::TabScrollEntry{.nodeIdx = nodeIdx, .scrollX = x});
}

struct RegisteredPanelEntry {
    DockPanelId panelId{};
    UiText title{};
    Icon icon{};
    UiText badge{};
    UiText tooltip{};
    bool closable{false};
    bool floatable{true};
    bool allowDock{true};
    bool expandable{true};
    bool disabled{false};
    bool isFloating{false};
    keel::u32 floatZOrder{0u};
    Node* contentNode{nullptr};
};

struct LaidOutTab {
    DockPanelId panelId{};
    const RegisteredPanelEntry* meta{nullptr};
    Rect tabRect{};
    Rect closeRect{};
    bool active{false};
    bool hovered{false};
    bool closeHovered{false};
    bool beingDragged{false};
};

struct LaidOutGroup {
    keel::u32 nodeIdx{kDockInvalidNode};
    Rect groupRect{};
    Rect tabBarRect{};
    Rect tabStripRect{};
    Rect overflowBtnRect{};
    Rect floatBtnRect{};
    Rect expandBtnRect{};
    Rect contentRect{};
    LaidOutTab* tabs{nullptr};
    keel::usize tabCount{0u};
    bool showTabBar{true};
    bool hasOverflow{false};
    bool overflowHovered{false};
    bool overflowOpen{false};
    bool floatBtnHovered{false};
    bool expandBtnHovered{false};
    bool activeExpanded{false};
};

struct LaidOutSash {
    keel::u32 nodeIdx{kDockInvalidNode};
    Orientation orientation{Orientation::Horizontal};
    Rect sashRect{};
    bool hovered{false};
    bool dragging{false};
};

// Arena-allocated, trivially destructible payload attached to NodeType::DockArea
struct DockAreaPayload {
    DockAreaOptions opts{};
    DockLayout* layout{nullptr};
    DockAreaPersistState* state{nullptr};
    Context* ctx{nullptr};
    ICanvas* canvas{nullptr};
    Id expandedModalId{};
    DockPanelId closedPanelThisFrame{};
    DockPanelId activePanelId{};
    bool layoutChanged{false};

    RegisteredPanelEntry* panels{nullptr};
    keel::usize panelCount{0u};
    keel::usize panelCap{0u};

    LaidOutGroup* groups{nullptr};
    keel::usize groupCount{0u};

    LaidOutSash* sashes{nullptr};
    keel::usize sashCount{0u};

    keel::f32 animProgress{1.0f};
    Rect innerRect{};
};

// Arena-allocated, trivially destructible payload attached to NodeType::DockFloat (and expanded modal card)
struct DockFloatPayload {
    DockPanelId panelId{};
    UiText title{};
    Icon icon{};
    UiText badge{};
    bool closable{false};
    bool allowDock{true};
    bool expandable{false};
    bool isExpanded{false};
    bool expandedFullscreen{false};
    bool minimized{false};
    bool disabled{false};
    bool closeHovered{false};
    bool redockHovered{false};
    bool expandHovered{false};
    bool dragging{false};
    UiSize size{UiSize::Md};
    keel::f32 titleBarHeight{30.0f};
    Rect titleRect{};
    Rect contentRect{};
    Rect expandRect{};
    Rect redockRect{};
    Rect closeRect{};
};

// Arena-allocated payload attached to the expanded panel's NodeType::Modal portal
struct ExpandedModalPayload {
    DockAreaPersistState* state{nullptr};
    Rect backdropRect{};
    bool fullscreen{false};
    keel::f32 backdropOpacity{0.58f};
    ICanvas* canvas{nullptr};
};

[[nodiscard]] TabChromeStyle ToTabChromeStyle(TabBarStyle s) noexcept {
    switch (s) {
        case TabBarStyle::Underline: return TabChromeStyle::Underline;
        case TabBarStyle::Card:      return TabChromeStyle::Card;
        case TabBarStyle::Folder:    return TabChromeStyle::Folder;
        case TabBarStyle::Pill:
        default:                     return TabChromeStyle::Pill;
    }
}

[[nodiscard]] keel::f32 EaseOutCubic(keel::f32 t) noexcept {
    const keel::f32 c = keel::clamp(t, 0.0f, 1.0f);
    const keel::f32 inv = 1.0f - c;
    return 1.0f - inv * inv * inv;
}

[[nodiscard]] Rect LerpRect(Rect a, Rect b, keel::f32 t) noexcept {
    return Rect(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.w + (b.w - a.w) * t,
        a.h + (b.h - a.h) * t
    );
}

[[nodiscard]] Rect ExpandSashHit(Rect sash, Orientation orient, keel::f32 pad) noexcept {
    if (orient == Orientation::Horizontal) {
        return Rect(sash.x - pad, sash.y, sash.w + pad * 2.0f, sash.h);
    }
    return Rect(sash.x, sash.y - pad, sash.w, sash.h + pad * 2.0f);
}

[[nodiscard]] Rect ComputeTabCloseRect(Rect tabRect, const ComponentMetrics& m) noexcept {
    const keel::f32 btnSz = keel::max(12.0f, m.iconSize * 0.92f);
    const keel::f32 padX = keel::min(m.paddingX * 0.65f, tabRect.w * 0.14f);
    return Rect(
        tabRect.Right() - keel::max(4.0f, padX) - btnSz,
        tabRect.y + (tabRect.h - btnSz) * 0.5f,
        btnSz,
        btnSz
    );
}

[[nodiscard]] const RegisteredPanelEntry* FindRegisteredPanel(
    const DockAreaPayload& payload,
    keel::StringRef id
) noexcept {
    for (keel::usize i = 0; i < payload.panelCount; ++i) {
        if (payload.panels[i].panelId.as_ref() == id) {
            return &payload.panels[i];
        }
    }
    return nullptr;
}

[[nodiscard]] RegisteredPanelEntry* FindRegisteredPanelMut(
    DockAreaPayload& payload,
    keel::StringRef id
) noexcept {
    for (keel::usize i = 0; i < payload.panelCount; ++i) {
        if (payload.panels[i].panelId.as_ref() == id) {
            return &payload.panels[i];
        }
    }
    return nullptr;
}

[[nodiscard]] keel::f32 ComputeMinSubtreeSpan(
    keel::ArrayRef<DockNode> nodes,
    keel::u32 idx,
    Orientation axis,
    keel::f32 minPane,
    keel::f32 sashPx,
    keel::u32 depth = 0u
) noexcept {
    if (idx >= nodes.size() || depth > 64u) return minPane;
    const DockNode& n = nodes[idx];
    if (n.kind == DockNodeKind::Tabs) {
        return minPane;
    }
    const keel::f32 a = ComputeMinSubtreeSpan(nodes, n.first, axis, minPane, sashPx, depth + 1u);
    const keel::f32 b = ComputeMinSubtreeSpan(nodes, n.second, axis, minPane, sashPx, depth + 1u);
    if (n.orientation == axis) {
        return a + b + sashPx;
    }
    return keel::max(a, b);
}

[[nodiscard]] keel::f32 ClampSplitForMinSizes(
    keel::f32 ratio,
    keel::f32 availSpan,
    keel::f32 minFirst,
    keel::f32 minSecond
) noexcept {
    keel::f32 minR = kMinSplitRatio;
    keel::f32 maxR = kMaxSplitRatio;
    if (availSpan > 1.0f) {
        const keel::f32 minSum = minFirst + minSecond;
        if (minSum <= availSpan) {
            minR = keel::max(minR, minFirst / availSpan);
            maxR = keel::min(maxR, 1.0f - (minSecond / availSpan));
        } else if (minSum > 0.0f) {
            const keel::f32 prop = keel::clamp(minFirst / minSum, kMinSplitRatio, kMaxSplitRatio);
            minR = prop;
            maxR = prop;
        }
    }
    if (minR > maxR) {
        const keel::f32 mid = (minR + maxR) * 0.5f;
        return keel::clamp(mid, kMinSplitRatio, kMaxSplitRatio);
    }
    return keel::clamp(ratio, minR, maxR);
}

[[nodiscard]] Rect ClampFloatBounds(Rect r, Rect container) noexcept {
    const keel::f32 maxW = (container.w > 1.0f) ? keel::max(kFloatMinWidthDp, container.w - 16.0f) : 1200.0f;
    const keel::f32 maxH = (container.h > 1.0f) ? keel::max(kFloatMinHeightDp, container.h - 16.0f) : 900.0f;
    r.w = keel::clamp(r.w, keel::min(kFloatMinWidthDp, maxW), maxW);
    r.h = keel::clamp(r.h, keel::min(kFloatMinHeightDp, maxH), maxH);
    if (container.w > 1.0f) {
        const keel::f32 maxX = keel::max(0.0f, container.w - r.w);
        r.x = keel::clamp(r.x, 0.0f, maxX);
    }
    if (container.h > 1.0f) {
        const keel::f32 maxY = keel::max(0.0f, container.h - keel::min(r.h, 32.0f));
        r.y = keel::clamp(r.y, 0.0f, maxY);
    }
    return Rect(keel::round(r.x), keel::round(r.y), keel::round(r.w), keel::round(r.h));
}

[[nodiscard]] keel::u8 HitTestFloatResizeEdges(Rect cardRect, Vec2 mouse, bool minimized) noexcept {
    if (minimized) return 0u;
    const Rect outer(
        cardRect.x - 2.0f,
        cardRect.y - 2.0f,
        cardRect.w + 4.0f,
        cardRect.h + 4.0f
    );
    if (!outer.Contains(mouse)) return 0u;

    keel::u8 mask = 0u;
    if (mouse.x <= cardRect.x + kFloatResizeEdgeDp) mask |= kResizeLeft;
    else if (mouse.x >= cardRect.Right() - kFloatResizeEdgeDp) mask |= kResizeRight;
    if (mouse.y <= cardRect.y + kFloatResizeEdgeDp) mask |= kResizeTop;
    else if (mouse.y >= cardRect.Bottom() - kFloatResizeEdgeDp) mask |= kResizeBottom;

    // Corner grab boost in bottom-right corner
    if (mouse.x >= cardRect.Right() - 14.0f && mouse.y >= cardRect.Bottom() - 14.0f) {
        mask = kResizeRight | kResizeBottom;
    }
    return mask;
}

[[nodiscard]] CursorType ResizeMaskToCursor(keel::u8 mask) noexcept {
    const bool l = (mask & kResizeLeft) != 0u;
    const bool r = (mask & kResizeRight) != 0u;
    const bool t = (mask & kResizeTop) != 0u;
    const bool b = (mask & kResizeBottom) != 0u;
    if ((l && t) || (r && b)) return CursorType::ResizeNWSE;
    if ((r && t) || (l && b)) return CursorType::ResizeNESW;
    if (l || r) return CursorType::ResizeEW;
    if (t || b) return CursorType::ResizeNS;
    return CursorType::Default;
}

void DrawDockBackIcon(ICanvas& canvas, Rect r, Color col) noexcept {
    // Clean geometric "dock into frame" vector glyph (outer box + anchored bottom-left pane)
    const keel::f32 stroke = keel::max(1.25f, r.w * 0.11f);
    canvas.DrawRoundRect(r, 1.5f, Color::Transparent(), col, stroke);
    const Rect inner(
        r.x + stroke * 1.6f,
        r.y + r.h * 0.52f,
        keel::max(2.0f, r.w - stroke * 3.2f),
        keel::max(2.0f, r.h * 0.48f - stroke * 1.6f)
    );
    canvas.DrawRoundRect(inner, 1.0f, col, Color::Transparent(), 0.0f);
}

void DrawFloatDetachIcon(ICanvas& canvas, Rect r, Color col) noexcept {
    // Clean geometric "detach as floating card" glyph: outer frame + lifted inner window with top bar
    const keel::f32 stroke = keel::max(1.2f, r.w * 0.105f);
    const Rect back(r.x, r.y + r.h * 0.28f, r.w * 0.72f, r.h * 0.72f);
    canvas.DrawRoundRect(back, 1.5f, Color::Transparent(), col.WithAlphaF(0.60f), stroke);
    const Rect front(r.x + r.w * 0.24f, r.y, r.w * 0.76f, r.h * 0.72f);
    canvas.DrawRoundRect(front, 1.5f, Color::Transparent(), col, stroke);
    const Rect titleBar(front.x + stroke, front.y + stroke, keel::max(1.0f, front.w - stroke * 2.0f), keel::max(1.5f, front.h * 0.26f));
    canvas.DrawRoundRect(titleBar, 0.5f, col, Color::Transparent(), 0.0f);
}

void DrawExpandSquareIcon(ICanvas& canvas, Rect r, Color col) noexcept {
    // Crisp single rounded square (Expand / Fullscreen)
    const keel::f32 stroke = keel::max(1.25f, r.w * 0.115f);
    const Rect sq(r.x + 0.5f, r.y + 0.5f, keel::max(4.0f, r.w - 1.0f), keel::max(4.0f, r.h - 1.0f));
    canvas.DrawRoundRect(sq, 1.75f, Color::Transparent(), col, stroke);
}

void DrawRestoreSquaresIcon(ICanvas& canvas, Rect r, Color col) noexcept {
    // Crisp overlapping double squares (Exit Fullscreen / Restore to modal)
    const keel::f32 stroke = keel::max(1.2f, r.w * 0.11f);
    const keel::f32 sz = keel::max(4.0f, r.w * 0.72f);
    const Rect front(r.x, r.Bottom() - sz, sz, sz);
    canvas.DrawRoundRect(front, 1.5f, Color::Transparent(), col, stroke);
    // Top-right back window outline (top + right edges only so it never overlaps front interior)
    const keel::f32 bx1 = r.x + r.w * 0.28f;
    const keel::f32 by1 = r.y;
    const keel::f32 bx2 = r.Right();
    const keel::f32 by2 = r.y + sz;
    canvas.DrawLine(Vec2(bx1, by1), Vec2(bx2, by1), col, stroke);
    canvas.DrawLine(Vec2(bx2, by1), Vec2(bx2, by2), col, stroke);
    canvas.DrawLine(Vec2(bx1, by1), Vec2(bx1, front.y), col, stroke);
    canvas.DrawLine(Vec2(front.Right(), by2), Vec2(bx2, by2), col, stroke);
}

[[nodiscard]] Rect ComputeSidePreviewRect(Rect base, DockSide side, keel::f32 fraction = 0.5f) noexcept {
    switch (side) {
        case DockSide::Left:   return Rect(base.x, base.y, base.w * fraction, base.h);
        case DockSide::Right:  return Rect(base.Right() - base.w * fraction, base.y, base.w * fraction, base.h);
        case DockSide::Top:    return Rect(base.x, base.y, base.w, base.h * fraction);
        case DockSide::Bottom: return Rect(base.x, base.Bottom() - base.h * fraction, base.w, base.h * fraction);
        case DockSide::Center:
        default:               return base;
    }
}

[[nodiscard]] Rect ExpandHitRect(Rect r, keel::f32 pad) noexcept {
    return Rect(r.x - pad, r.y - pad, r.w + pad * 2.0f, r.h + pad * 2.0f);
}

struct CompassRects {
    Rect center{};
    Rect left{};
    Rect right{};
    Rect top{};
    Rect bottom{};
    keel::f32 gap{4.0f};
    bool valid{false};
};

struct OuterEdgeRects {
    Rect left{};
    Rect right{};
    Rect top{};
    Rect bottom{};
    bool valid{false};
};

[[nodiscard]] OuterEdgeRects ComputeOuterEdgeRects(Rect areaRect) noexcept {
    OuterEdgeRects r{};
    if (areaRect.w < 140.0f || areaRect.h < 120.0f) {
        return r;
    }
    const keel::f32 btn = 26.0f;
    const keel::f32 inset = 6.0f;
    r.left   = Rect(keel::round(areaRect.x + inset), keel::round(areaRect.Center().y - btn * 0.5f), btn, btn);
    r.right  = Rect(keel::round(areaRect.Right() - inset - btn), keel::round(areaRect.Center().y - btn * 0.5f), btn, btn);
    r.top    = Rect(keel::round(areaRect.Center().x - btn * 0.5f), keel::round(areaRect.y + inset), btn, btn);
    r.bottom = Rect(keel::round(areaRect.Center().x - btn * 0.5f), keel::round(areaRect.Bottom() - inset - btn), btn, btn);
    r.valid  = true;
    return r;
}

[[nodiscard]] CompassRects ComputeGroupCompassRects(Rect bodyRect) noexcept {
    CompassRects r{};
    if (bodyRect.w < 64.0f || bodyRect.h < 60.0f) {
        return r;
    }
    const keel::f32 maxBtn = keel::min((bodyRect.w - 14.0f) / 3.0f, (bodyRect.h - 12.0f) / 3.0f);
    const keel::f32 btn = keel::clamp(keel::floor(maxBtn), 18.0f, 30.0f);
    const keel::f32 gap = (btn >= 24.0f) ? 4.0f : 3.0f;
    const Vec2 c = bodyRect.Center();
    r.center = Rect(keel::round(c.x - btn * 0.5f), keel::round(c.y - btn * 0.5f), btn, btn);
    r.left   = Rect(r.center.x - gap - btn, r.center.y, btn, btn);
    r.right  = Rect(r.center.Right() + gap, r.center.y, btn, btn);
    r.top    = Rect(r.center.x, r.center.y - gap - btn, btn, btn);
    r.bottom = Rect(r.center.x, r.center.Bottom() + gap, btn, btn);
    r.gap    = gap;
    r.valid  = true;
    return r;
}

[[nodiscard]] Rect TrimCompassBoundsAgainstOuter(Rect bodyRect, const OuterEdgeRects& outer) noexcept {
    if (!outer.valid) return bodyRect;
    const CompassRects probe = ComputeGroupCompassRects(bodyRect);
    if (!probe.valid) return bodyRect;

    Rect r = bodyRect;
    const keel::f32 margin = 6.0f;
    const keel::f32 minSpan = 62.0f;
    auto overlaps = [](Rect a, Rect b) noexcept {
        return a.x < b.Right() && a.Right() > b.x && a.y < b.Bottom() && a.Bottom() > b.y;
    };

    // Trim only on edges where the untrimmed compass would actually collide with or crowd an outer tile
    if (overlaps(ExpandHitRect(probe.bottom, margin), outer.bottom) && r.Bottom() > outer.bottom.y - margin) {
        const keel::f32 trimmedH = (outer.bottom.y - margin) - r.y;
        if (trimmedH >= minSpan) r.h = trimmedH;
    }
    if (overlaps(ExpandHitRect(probe.top, margin), outer.top) && r.y < outer.top.Bottom() + margin) {
        const keel::f32 newY = outer.top.Bottom() + margin;
        if (r.Bottom() - newY >= minSpan) {
            r.h = r.Bottom() - newY;
            r.y = newY;
        }
    }
    if (overlaps(ExpandHitRect(probe.left, margin), outer.left) && r.x < outer.left.Right() + margin) {
        const keel::f32 newX = outer.left.Right() + margin;
        if (r.Right() - newX >= minSpan) {
            r.w = r.Right() - newX;
            r.x = newX;
        }
    }
    if (overlaps(ExpandHitRect(probe.right, margin), outer.right) && r.Right() > outer.right.x - margin) {
        const keel::f32 trimmedW = (outer.right.x - margin) - r.x;
        if (trimmedW >= minSpan) r.w = trimmedW;
    }
    return r;
}

[[nodiscard]] DockDropTarget EvaluateDropTarget(
    const DockAreaPersistState& st,
    const DockLayout& layout,
    const DockAreaOptions& opts,
    Vec2 mouse
) noexcept {
    DockDropTarget out{};
    const Rect area = st.prevInnerRect;
    if (area.w <= 1.0f || area.h <= 1.0f) return out;

    const bool canDock = opts.dockable && st.dragAllowDock;

    // Empty docked tree: dropping anywhere inside DockArea docks as the root panel
    if (layout.RootIndex() == kDockInvalidNode) {
        if (canDock && area.Contains(mouse)) {
            out.kind = DockDropKind::RootEdge;
            out.side = DockSide::Center;
            out.previewRect = area;
            return out;
        }
        if (opts.tearable && st.dragFloatable && !st.dragFromFloat) {
            out.kind = DockDropKind::TearFloat;
            out.previewRect = Rect(mouse.x - 140.0f, mouse.y - 16.0f, 300.0f, 220.0f);
        }
        return out;
    }

    const bool isSoleDockedPanel = !st.dragFromFloat &&
        st.prevGroups.size() == 1u &&
        st.prevGroups[0].tabs.size() <= 1u;

    const OuterEdgeRects outer = (canDock && !isSoleDockedPanel)
        ? ComputeOuterEdgeRects(area)
        : OuterEdgeRects{};

    // Locate the docked group under the cursor (used for compass overlay & explicit tile hit-testing)
    const PrevGroupGeo* hoveredGrp = nullptr;
    if (canDock) {
        for (const auto& grp : st.prevGroups) {
            if (grp.groupRect.Contains(mouse)) {
                hoveredGrp = &grp;
                break;
            }
        }
    }

    CompassRects comp{};
    if (hoveredGrp != nullptr) {
        const bool isOwnGroup = (!st.dragFromFloat && hoveredGrp->nodeIdx == st.dragSourceNode);
        const bool isOwnSoleGroup = isOwnGroup && (hoveredGrp->tabs.size() <= 1u);
        if (!isOwnSoleGroup) {
            const Rect rawBody = (hoveredGrp->contentRect.h > 8.0f && hoveredGrp->contentRect.w > 8.0f)
                ? hoveredGrp->contentRect
                : hoveredGrp->groupRect;
            const Rect compassBounds = TrimCompassBoundsAgainstOuter(rawBody, outer);
            comp = ComputeGroupCompassRects(compassBounds);
            out.groupRect = hoveredGrp->groupRect;
            out.compassRect = compassBounds;
            out.showCompass = comp.valid;
            out.centerIsKeep = isOwnGroup;
        }
    }

    // 1. Explicit outer perimeter tiles (1:1 visual = hitbox, front-most visual layer)
    if (outer.valid) {
        constexpr keel::f32 kOuterHitPad = 3.0f;
        if (ExpandHitRect(outer.left, kOuterHitPad).Contains(mouse)) {
            out.kind = DockDropKind::RootEdge;
            out.side = DockSide::Left;
            out.previewRect = ComputeSidePreviewRect(area, DockSide::Left, 0.25f);
            return out;
        }
        if (ExpandHitRect(outer.right, kOuterHitPad).Contains(mouse)) {
            out.kind = DockDropKind::RootEdge;
            out.side = DockSide::Right;
            out.previewRect = ComputeSidePreviewRect(area, DockSide::Right, 0.25f);
            return out;
        }
        if (ExpandHitRect(outer.top, kOuterHitPad).Contains(mouse)) {
            out.kind = DockDropKind::RootEdge;
            out.side = DockSide::Top;
            out.previewRect = ComputeSidePreviewRect(area, DockSide::Top, 0.25f);
            return out;
        }
        if (ExpandHitRect(outer.bottom, kOuterHitPad).Contains(mouse)) {
            out.kind = DockDropKind::RootEdge;
            out.side = DockSide::Bottom;
            out.previewRect = ComputeSidePreviewRect(area, DockSide::Bottom, 0.25f);
            return out;
        }
    }

    // 2. Tab strips (intra-group reorder or exact inter-group tab insertion with caret)
    for (const auto& grp : st.prevGroups) {
        const Rect stripR = (grp.tabStripRect.w > 0.0f) ? grp.tabStripRect : grp.tabBarRect;
        if (stripR.w > 0.0f && stripR.h > 0.0f && stripR.Contains(mouse)) {
            out.showCompass = false;
            const bool sameGroup = (!st.dragFromFloat && grp.nodeIdx == st.dragSourceNode);
            if (sameGroup && opts.reorderable && grp.tabs.size() > 1u) {
                keel::usize insertIdx = grp.tabs.size() - 1u;
                keel::f32 caretX = grp.tabs.back().tabRect.Right();
                for (keel::usize i = 0; i < grp.tabs.size(); ++i) {
                    const Rect tr = grp.tabs[i].tabRect;
                    if (mouse.x < tr.Center().x) {
                        insertIdx = i;
                        caretX = tr.x;
                        break;
                    }
                }
                if (insertIdx > st.dragSourceIndex) {
                    insertIdx = keel::min(insertIdx, grp.tabs.size() - 1u);
                }
                caretX = keel::clamp(caretX, stripR.x + 2.0f, stripR.Right() - 2.0f);
                out.kind = DockDropKind::ReorderTab;
                out.targetNodeIdx = grp.nodeIdx;
                out.insertIndex = insertIdx;
                out.groupRect = grp.groupRect;
                out.caretRect = Rect(caretX - 1.5f, stripR.y + 3.0f, 3.0f, keel::max(8.0f, stripR.h - 6.0f));
                return out;
            }
            if (!sameGroup && canDock) {
                keel::usize insertIdx = grp.tabs.size();
                keel::f32 caretX = grp.tabs.empty() ? (stripR.x + 4.0f) : grp.tabs.back().tabRect.Right();
                for (keel::usize i = 0; i < grp.tabs.size(); ++i) {
                    const Rect tr = grp.tabs[i].tabRect;
                    if (mouse.x < tr.Center().x) {
                        insertIdx = i;
                        caretX = tr.x;
                        break;
                    }
                }
                caretX = keel::clamp(caretX, stripR.x + 2.0f, stripR.Right() - 2.0f);
                out.kind = DockDropKind::InsertTab;
                out.targetNodeIdx = grp.nodeIdx;
                out.side = DockSide::Center;
                out.insertIndex = insertIdx;
                out.groupRect = grp.groupRect;
                out.previewRect = stripR;
                out.caretRect = Rect(caretX - 1.5f, stripR.y + 3.0f, 3.0f, keel::max(8.0f, stripR.h - 6.0f));
                return out;
            }
            return out;
        }
    }

    // 3. Explicit 5-way compass tiles centered in hovered pane
    //    (On own multi-tab group, Center = KeepInPlace so there is no awkward hole and users can cancel cleanly)
    if (hoveredGrp != nullptr && out.showCompass) {
        const keel::f32 hitPad = comp.gap * 0.5f;
        DockSide chosen = DockSide::Center;
        bool hitCompass = false;
        if (ExpandHitRect(comp.center, hitPad).Contains(mouse)) {
            chosen = DockSide::Center;
            hitCompass = true;
        } else if (ExpandHitRect(comp.left, hitPad).Contains(mouse)) {
            chosen = DockSide::Left;
            hitCompass = true;
        } else if (ExpandHitRect(comp.right, hitPad).Contains(mouse)) {
            chosen = DockSide::Right;
            hitCompass = true;
        } else if (ExpandHitRect(comp.top, hitPad).Contains(mouse)) {
            chosen = DockSide::Top;
            hitCompass = true;
        } else if (ExpandHitRect(comp.bottom, hitPad).Contains(mouse)) {
            chosen = DockSide::Bottom;
            hitCompass = true;
        }

        if (hitCompass) {
            if (chosen == DockSide::Center && out.centerIsKeep) {
                out.kind = DockDropKind::KeepInPlace;
                out.targetNodeIdx = hoveredGrp->nodeIdx;
                out.side = DockSide::Center;
                out.previewRect = hoveredGrp->groupRect;
                return out;
            }
            out.kind = (chosen == DockSide::Center) ? DockDropKind::GroupCenter : DockDropKind::GroupSide;
            out.targetNodeIdx = hoveredGrp->nodeIdx;
            out.side = chosen;
            out.previewRect = ComputeSidePreviewRect(hoveredGrp->groupRect, chosen, 0.5f);
            return out;
        }
    }

    // 4. Anywhere outside explicit dock targets:
    //    - Inside DockArea -> safe cancel (DockDropKind::None, compass stays visible for targeting)
    //    - Dragged OUTSIDE DockArea -> deliberate TearFloat into an in-window floating card
    if (opts.tearable && st.dragFloatable && !st.dragFromFloat && !area.Contains(mouse)) {
        const keel::f32 fw = keel::clamp(area.w * 0.34f, kFloatMinWidthDp, 380.0f);
        const keel::f32 fh = keel::clamp(area.h * 0.42f, kFloatMinHeightDp, 280.0f);
        const Rect localFloat = ClampFloatBounds(
            Rect(mouse.x - area.x - fw * 0.5f, mouse.y - area.y - 16.0f, fw, fh),
            area
        );
        out.kind = DockDropKind::TearFloat;
        out.previewRect = Rect(area.x + localFloat.x, area.y + localFloat.y, localFloat.w, localFloat.h);
    }
    return out;
}

void TriggerLayoutAnimation(DockAreaPersistState& st, const DockLayout& layout, const UiTheme& theme, keel::f64 nowSec, bool enabled) {
    if (!enabled || theme.animationScale <= 0.001f || st.prevGroups.empty()) {
        st.animating = false;
        st.paneAnims.clear();
        st.lastLayoutRevision = layout.Revision();
        return;
    }

    // Snapshot previous rendered rect per active/first panel ID in each group
    st.paneAnims.clear();
    for (const auto& grp : st.prevGroups) {
        DockPanelId key{};
        for (const auto& t : grp.tabs) {
            if (t.active) {
                key = t.panelId;
                break;
            }
        }
        if (key.empty() && !grp.tabs.empty()) {
            key = grp.tabs[0].panelId;
        }
        if (!key.empty()) {
            st.paneAnims.push_back(PaneAnimState{
                .keyPanelId = key,
                .fromRect = grp.groupRect,
                .currentRect = grp.groupRect
            });
        }
    }
    st.animStartTimeSec = nowSec;
    st.animDurationSec = 0.16 * keel::as<keel::f64>(theme.animationScale);
    st.animating = !st.paneAnims.empty();
    st.lastLayoutRevision = layout.Revision();
}

void DrawDockTab(
    ICanvas& canvas,
    const UiTheme& theme,
    const LaidOutTab& tab,
    TabBarStyle style,
    UiSize uiSize,
    bool areaDisabled
) {
    const ComponentMetrics& m = theme.GetMetrics(uiSize);
    const auto& pal = theme.palette;
    const bool disabled = areaDisabled || (tab.meta && tab.meta->disabled);
    const UiText label = (tab.meta && !tab.meta->title.empty()) ? tab.meta->title : UiText(tab.panelId.as_ref());
    const Icon icon = tab.meta ? tab.meta->icon : Icon{};
    const UiText badge = tab.meta ? tab.meta->badge : UiText{};
    const bool closable = tab.meta && tab.meta->closable;

    const keel::f32 labelFont = theme.ResolveFontSize(label, TextRole::LabelMedium, uiSize);
    const keel::f32 badgeFont = theme.ResolveFontSize(badge, m.fontSize * 0.76f);
    const Rect bounds = tab.tabRect;
    if (bounds.w <= 2.0f || bounds.h <= 2.0f) return;

    canvas.PushClip(bounds);

    const keel::f32 yShift = theme.Painter().DrawTabChrome(
        canvas,
        theme,
        bounds,
        ToTabChromeStyle(style),
        tab.active,
        tab.hovered,
        false,
        disabled,
        true,
        pal.accent,
        pal.surfaceActive,
        m.cornerRadius
    );

    Color fgCol = disabled
        ? pal.textDisabled
        : ((tab.active || tab.hovered) ? pal.textPrimary : pal.textSecondary);
    if (tab.beingDragged) {
        fgCol = fgCol.WithAlphaF(0.45f);
    }

    const keel::f32 closeReserve = closable ? (tab.closeRect.w + 6.0f) : 0.0f;
    const keel::f32 padX = keel::min(m.paddingX * 0.85f, bounds.w * 0.12f);
    const keel::f32 innerW = keel::max(0.0f, bounds.w - padX * 2.0f - closeReserve);
    const bool hasIcon = icon.IsValid();
    const bool hasLabel = !label.empty();

    keel::f32 badgeW = 0.0f;
    if (!badge.empty()) {
        badgeW = TextUtil::MeasureWidth(badge, badgeFont, &canvas) + 10.0f;
    }
    const bool showBadge = !badge.empty() &&
        (innerW >= (hasIcon ? m.iconSize + 5.0f : 0.0f) + badgeW + (hasLabel ? labelFont * 2.0f + 5.0f : 0.0f));

    const keel::f32 iconAndBadgeW = (hasIcon ? m.iconSize : 0.0f) + (showBadge ? badgeW + 5.0f : 0.0f);
    const keel::f32 availLabelW = keel::max(0.0f, innerW - iconAndBadgeW - ((hasIcon && hasLabel) ? 5.0f : 0.0f));
    const bool showLabel = hasLabel && (!hasIcon || availLabelW >= labelFont * 1.2f);

    keel::FixedString<256> truncated;
    keel::StringRef displayLabel = label.text;
    keel::f32 labelW = 0.0f;
    if (showLabel) {
        labelW = canvas.MeasureText(displayLabel, labelFont).x;
        if (labelW > availLabelW + 0.5f && availLabelW > 0.0f) {
            truncated = TextUtil::TruncateWithEllipsis(label.text, availLabelW, labelFont, &canvas);
            displayLabel = truncated;
            labelW = keel::min(availLabelW, canvas.MeasureText(displayLabel, labelFont).x);
        }
    }

    keel::f32 curX = bounds.x + padX;
    const keel::f32 centerY = bounds.y + bounds.h * 0.5f + yShift;

    if (hasIcon) {
        const keel::f32 icSz = keel::min(m.iconSize, bounds.h * 0.58f);
        icon.Draw(canvas, Rect(curX, centerY - icSz * 0.5f, icSz, icSz), fgCol);
        curX += icSz + (showLabel ? 5.0f : 0.0f);
    }

    if (showLabel) {
        theme.Painter().DrawControlText(
            canvas,
            theme,
            displayLabel,
            Rect(curX, bounds.y + yShift, availLabelW, bounds.h),
            labelFont,
            fgCol,
            disabled,
            0.0f,
            0.5f
        );
        curX += labelW;
    }

    if (showBadge) {
        curX += 5.0f;
        const keel::f32 badgeH = badgeFont + 5.0f;
        const Rect badgeR(curX, centerY - badgeH * 0.5f, badgeW, badgeH);
        const Color badgeBg = tab.active ? pal.accent.WithAlphaF(0.22f) : pal.surfaceHover;
        const Color badgeInk = tab.active ? theme.EnsureReadableInk(pal.accent, pal.surface) : pal.textSecondary;
        theme.Painter().DrawBadge(canvas, theme, badgeR, badgeBg, Color::Transparent(), 0.0f, 4.0f);
        canvas.DrawTextBox(badge.text, badgeR, badgeFont, badgeInk, 0.5f, 0.5f);
    }

    if (closable && tab.closeRect.w > 4.0f && bounds.w > tab.closeRect.w + 8.0f) {
        if (tab.closeHovered && !disabled) {
            canvas.DrawRoundRect(tab.closeRect, keel::min(3.5f, m.cornerRadius), pal.surfaceHover);
        }
        const keel::f32 icSz = keel::max(8.0f, tab.closeRect.w * 0.65f);
        const Rect icRect(
            tab.closeRect.x + (tab.closeRect.w - icSz) * 0.5f,
            tab.closeRect.y + (tab.closeRect.h - icSz) * 0.5f,
            icSz,
            icSz
        );
        const Color closeCol = disabled ? pal.textDisabled : (tab.closeHovered ? pal.textPrimary : pal.textSecondary);
        Icon::Close().Draw(canvas, icRect, closeCol);
    }

    canvas.PopClip();
}

void DrawDragOverlay(
    ICanvas& canvas,
    const UiTheme& theme,
    const DockAreaPersistState& st,
    const DockDropTarget& drop,
    Rect areaRect,
    Vec2 mouse,
    UiText dragLabel,
    Icon dragIcon,
    UiSize uiSize
) {
    const auto& pal = theme.palette;
    const ComponentMetrics& m = theme.GetMetrics(uiSize);

    // 1. Drop-zone preview fill & border
    if ((drop.kind == DockDropKind::ReorderTab || drop.kind == DockDropKind::InsertTab) && drop.caretRect.w > 0.0f) {
        if (drop.kind == DockDropKind::InsertTab && drop.previewRect.w > 2.0f && drop.previewRect.h > 2.0f) {
            canvas.DrawRoundRect(drop.previewRect, 4.0f, pal.accent.WithAlphaF(0.14f), pal.accent.WithAlphaF(0.85f), 1.5f);
        }
        canvas.DrawRoundRect(drop.caretRect, 1.5f, pal.accent);
        const keel::f32 capSz = 5.0f;
        canvas.DrawRoundRect(
            Rect(drop.caretRect.Center().x - capSz * 0.5f, drop.caretRect.y - capSz * 0.5f, capSz, capSz),
            capSz * 0.5f,
            pal.accent
        );
        canvas.DrawRoundRect(
            Rect(drop.caretRect.Center().x - capSz * 0.5f, drop.caretRect.Bottom() - capSz * 0.5f, capSz, capSz),
            capSz * 0.5f,
            pal.accent
        );
    } else if (drop.kind != DockDropKind::None && drop.previewRect.w > 2.0f && drop.previewRect.h > 2.0f) {
        const keel::f32 radius = (theme.shellStyle == ShellStyle::Floating) ? theme.shell.panelRadius : 4.0f;
        const Color fillCol = (drop.kind == DockDropKind::KeepInPlace)
            ? pal.accent.WithAlphaF(0.08f)
            : ((drop.kind == DockDropKind::TearFloat) ? pal.surfaceElevated.WithAlphaF(0.22f) : pal.accent.WithAlphaF(0.22f));
        const Color borderCol = (drop.kind == DockDropKind::KeepInPlace)
            ? pal.accent.WithAlphaF(0.55f)
            : ((drop.kind == DockDropKind::TearFloat) ? pal.accent.WithAlphaF(0.75f) : pal.accent);
        const keel::f32 borderW = (drop.kind == DockDropKind::KeepInPlace || drop.kind == DockDropKind::TearFloat) ? 1.5f : 2.0f;
        canvas.DrawRoundRect(drop.previewRect, radius, fillCol, borderCol, borderW);
    }

    auto drawSplitIndicatorTile = [&](Rect r, DockSide side, bool active) {
        const Color bg = active ? pal.accent : pal.surfaceElevated.WithAlphaF(0.95f);
        const Color bd = active ? pal.accent : pal.border;
        const Color fg = active ? pal.accentText : pal.textSecondary;
        const keel::f32 tileRad = keel::min(4.0f, theme.GetRadius(UiSize::Sm));
        theme.Painter().DrawFrame(canvas, theme, FrameKind::Button, FramePaintParams{
            .rect = r,
            .radius = tileRad,
            .bg = bg,
            .border = bd,
            .borderWidth = 1.0f,
            .hovered = active,
            .active = active,
            .elevation = Elevation::Sm
        });
        const Rect inner(r.x + 5.0f, r.y + 5.0f, keel::max(4.0f, r.w - 10.0f), keel::max(4.0f, r.h - 10.0f));
        if (side == DockSide::Center) {
            canvas.DrawRoundRect(inner, 1.5f, fg.WithAlphaF(0.35f), fg, 1.25f);
        } else {
            canvas.DrawRoundRect(inner, 1.5f, Color::Transparent(), fg.WithAlphaF(0.65f), 1.0f);
            canvas.DrawRoundRect(ComputeSidePreviewRect(inner, side, 0.45f), 1.0f, fg);
        }
    };

    // 2. Docking compass centered inside the hovered group's trimmed content area (ALWAYS 5 tiles, no hole)
    if (drop.showCompass && drop.compassRect.w > 0.0f && drop.compassRect.h > 0.0f) {
        const CompassRects comp = ComputeGroupCompassRects(drop.compassRect);
        if (comp.valid) {
            // Seamless cross backdrop (border silhouette pass first, then interior fill pass -> zero crossing seams!)
            const keel::f32 backPad = 5.0f;
            const Color backBg = pal.surface.WithAlphaF(0.84f);
            const Color backBd = pal.borderSubtle.WithAlphaF(0.90f);
            const Rect horizBar(
                comp.left.x - backPad,
                comp.left.y - backPad,
                (comp.right.Right() - comp.left.x) + backPad * 2.0f,
                comp.left.h + backPad * 2.0f
            );
            const Rect vertBar(
                comp.top.x - backPad,
                comp.top.y - backPad,
                comp.top.w + backPad * 2.0f,
                (comp.bottom.Bottom() - comp.top.y) + backPad * 2.0f
            );
            canvas.DrawRoundRect(ExpandHitRect(horizBar, 1.0f), 7.0f, backBd);
            canvas.DrawRoundRect(ExpandHitRect(vertBar, 1.0f), 7.0f, backBd);
            canvas.DrawRoundRect(horizBar, 6.0f, backBg);
            canvas.DrawRoundRect(vertBar, 6.0f, backBg);

            const bool anyCompassHit = (drop.kind == DockDropKind::GroupCenter ||
                                        drop.kind == DockDropKind::GroupSide ||
                                        drop.kind == DockDropKind::KeepInPlace);
            drawSplitIndicatorTile(comp.center, DockSide::Center, anyCompassHit && drop.side == DockSide::Center);
            drawSplitIndicatorTile(comp.left,   DockSide::Left,   anyCompassHit && drop.side == DockSide::Left);
            drawSplitIndicatorTile(comp.right,  DockSide::Right,  anyCompassHit && drop.side == DockSide::Right);
            drawSplitIndicatorTile(comp.top,    DockSide::Top,    anyCompassHit && drop.side == DockSide::Top);
            drawSplitIndicatorTile(comp.bottom, DockSide::Bottom, anyCompassHit && drop.side == DockSide::Bottom);
        }
    }

    // 3. 4 outer-edge dock indicator tiles (exact 1:1 geometry with EvaluateDropTarget!)
    const bool isSoleDockedPanel = !st.dragFromFloat &&
        st.prevGroups.size() == 1u &&
        st.prevGroups[0].tabs.size() <= 1u;
    if (st.dragAllowDock && !isSoleDockedPanel) {
        const OuterEdgeRects outer = ComputeOuterEdgeRects(areaRect);
        if (outer.valid) {
            drawSplitIndicatorTile(outer.left,   DockSide::Left,   drop.kind == DockDropKind::RootEdge && drop.side == DockSide::Left);
            drawSplitIndicatorTile(outer.right,  DockSide::Right,  drop.kind == DockDropKind::RootEdge && drop.side == DockSide::Right);
            drawSplitIndicatorTile(outer.top,    DockSide::Top,    drop.kind == DockDropKind::RootEdge && drop.side == DockSide::Top);
            drawSplitIndicatorTile(outer.bottom, DockSide::Bottom, drop.kind == DockDropKind::RootEdge && drop.side == DockSide::Bottom);
        }
    }

    // 4. Drag ghost tab capsule offset below-right of pointer so compass tiles under cursor remain unobstructed
    const keel::f32 fontSize = theme.ResolveFontSize(dragLabel, TextRole::LabelMedium, uiSize);
    keel::StringRef hint{};
    switch (drop.kind) {
        case DockDropKind::KeepInPlace: hint = "Keep"; break;
        case DockDropKind::ReorderTab:  hint = "Reorder"; break;
        case DockDropKind::InsertTab:   hint = "Insert Tab"; break;
        case DockDropKind::GroupCenter: hint = "Tab"; break;
        case DockDropKind::GroupSide:
            switch (drop.side) {
                case DockSide::Left:   hint = "Split Left"; break;
                case DockSide::Right:  hint = "Split Right"; break;
                case DockSide::Top:    hint = "Split Top"; break;
                case DockSide::Bottom: hint = "Split Bottom"; break;
                default:               hint = "Split"; break;
            }
            break;
        case DockDropKind::RootEdge:
            switch (drop.side) {
                case DockSide::Left:   hint = "Outer Left"; break;
                case DockSide::Right:  hint = "Outer Right"; break;
                case DockSide::Top:    hint = "Outer Top"; break;
                case DockSide::Bottom: hint = "Outer Bottom"; break;
                default:               hint = "Dock"; break;
            }
            break;
        case DockDropKind::TearFloat:   hint = "Float"; break;
        case DockDropKind::None:
        default:                        break;
    }

    const keel::f32 labelW = canvas.MeasureText(dragLabel.text, fontSize).x;
    const keel::f32 hintFont = fontSize * 0.85f;
    const keel::f32 hintW = hint.empty() ? 0.0f : (canvas.MeasureText(hint, hintFont).x + 12.0f);
    const keel::f32 icSz = dragIcon.IsValid() ? m.iconSize : 0.0f;
    const keel::f32 ghostW = keel::clamp(
        18.0f + icSz + (icSz > 0.0f ? 6.0f : 0.0f) + labelW + (hintW > 0.0f ? 8.0f + hintW : 0.0f),
        80.0f,
        280.0f
    );
    const keel::f32 ghostH = keel::max(26.0f, m.defaultHeight * 0.92f);
    keel::f32 ghostX = mouse.x + 16.0f;
    keel::f32 ghostY = mouse.y + 16.0f;
    if (areaRect.w > ghostW + 16.0f && ghostX + ghostW > areaRect.Right() - 4.0f) {
        ghostX = mouse.x - ghostW - 14.0f;
    }
    if (areaRect.h > ghostH + 16.0f && ghostY + ghostH > areaRect.Bottom() - 4.0f) {
        ghostY = mouse.y - ghostH - 14.0f;
    }
    const Rect ghostR(keel::round(ghostX), keel::round(ghostY), keel::round(ghostW), keel::round(ghostH));

    const keel::f32 ghostRad = (m.cornerRadius > 0.01f) ? (m.cornerRadius + 2.0f) : 0.0f;
    theme.Painter().DrawSurface(canvas, theme, SurfacePaintParams{
        .rect = ghostR,
        .role = SurfaceRole::Popup,
        .radius = ghostRad,
        .bg = pal.surfaceElevated,
        .border = pal.accent,
        .borderWidth = 1.5f,
        .explicitBorder = true,
        .elevation = Elevation::Md
    });

    keel::f32 cx = ghostR.x + 9.0f;
    if (dragIcon.IsValid()) {
        dragIcon.Draw(canvas, Rect(cx, ghostR.Center().y - icSz * 0.5f, icSz, icSz), pal.textPrimary);
        cx += icSz + 6.0f;
    }
    const keel::f32 maxLblW = keel::max(10.0f, ghostR.Right() - cx - (hintW > 0.0f ? hintW + 10.0f : 8.0f));
    canvas.DrawTextBox(dragLabel.text, Rect(cx, ghostR.y, maxLblW, ghostR.h), fontSize, pal.textPrimary, 0.0f, 0.5f);
    if (!hint.empty()) {
        const Rect badgeR(ghostR.Right() - hintW - 6.0f, ghostR.Center().y - (hintFont + 5.0f) * 0.5f, hintW, hintFont + 5.0f);
        theme.Painter().DrawBadge(canvas, theme, badgeR, pal.accent.WithAlphaF(0.22f), Color::Transparent(), 0.0f, 4.0f);
        canvas.DrawTextBox(hint, badgeR, hintFont, theme.EnsureReadableInk(pal.accent, pal.surfaceElevated), 0.5f, 0.5f);
    }
}

// --- NodeType::DockFloat hooks ---

Rect DockFloatContentClip(const Node* node, const UiTheme& /*theme*/) noexcept {
    if (!node) return Rect{};
    if (const auto* fp = node->As<DockFloatPayload>()) {
        return fp->contentRect;
    }
    return node->computedRect;
}

void ArrangeDockFloat(Node* node, const Rect& availableRect, const UiTheme& theme) noexcept {
    if (!node) return;
    node->computedRect = availableRect;
    auto* fp = node->As<DockFloatPayload>();
    const bool isExpandedFs = (fp && fp->isExpanded && fp->expandedFullscreen);
    const keel::f32 bw = isExpandedFs ? 0.0f : keel::max(1.0f, node->borderWidth);
    const keel::f32 titleH = fp ? fp->titleBarHeight : 30.0f;
    const Rect titleR(
        availableRect.x + bw,
        availableRect.y + bw,
        keel::max(0.0f, availableRect.w - bw * 2.0f),
        keel::min(titleH, keel::max(0.0f, availableRect.h - bw * 2.0f))
    );
    const Rect contentR(
        availableRect.x + bw,
        titleR.Bottom(),
        keel::max(0.0f, availableRect.w - bw * 2.0f),
        keel::max(0.0f, availableRect.Bottom() - bw - titleR.Bottom())
    );

    if (fp) {
        fp->titleRect = titleR;
        fp->contentRect = contentR;
        const ComponentMetrics& m = theme.GetMetrics(fp->size);
        const keel::f32 btnSz = keel::clamp(titleH - 8.0f, 18.0f, 24.0f);
        keel::f32 rx = titleR.Right() - 6.0f;
        if (fp->isExpanded) {
            // Expanded modal header: [Fullscreen/Restore □/❐] [Return to dock ×]
            rx -= btnSz;
            fp->closeRect = Rect(rx, titleR.Center().y - btnSz * 0.5f, btnSz, btnSz);
            rx -= 4.0f + btnSz;
            fp->expandRect = Rect(rx, titleR.Center().y - btnSz * 0.5f, btnSz, btnSz);
            fp->redockRect = Rect{};
        } else {
            if (fp->closable) {
                rx -= btnSz;
                fp->closeRect = Rect(rx, titleR.Center().y - btnSz * 0.5f, btnSz, btnSz);
                rx -= 3.0f;
            } else {
                fp->closeRect = Rect{};
            }
            if (fp->allowDock) {
                rx -= btnSz;
                fp->redockRect = Rect(rx, titleR.Center().y - btnSz * 0.5f, btnSz, btnSz);
                rx -= 3.0f;
            } else {
                fp->redockRect = Rect{};
            }
            if (fp->expandable) {
                rx -= btnSz;
                fp->expandRect = Rect(rx, titleR.Center().y - btnSz * 0.5f, btnSz, btnSz);
            } else {
                fp->expandRect = Rect{};
            }
        }
        (void)m;
    }

    if (fp && fp->minimized) {
        for (Node* c = node->firstChild; c != nullptr; c = c->nextSibling) {
            c->computedRect = Rect{};
        }
        return;
    }

    // Arrange direct child widgets inside contentR (standard vertical column layout with padding/gap)
    const Rect inner(
        contentR.x + node->padding.left,
        contentR.y + node->padding.top,
        keel::max(0.0f, contentR.w - node->padding.Horizontal()),
        keel::max(0.0f, contentR.h - node->padding.Vertical())
    );
    if (node->childCount == 1u && node->firstChild &&
        node->firstChild->widthMode == SizeMode::Stretch &&
        node->firstChild->heightMode == SizeMode::Stretch) {
        Solver::Arrange(node->firstChild, inner, theme, nullptr);
        return;
    }

    keel::f32 fixedSum = 0.0f;
    keel::f32 stretchWeight = 0.0f;
    keel::u32 visibleCount = 0u;
    for (Node* c = node->firstChild; c != nullptr; c = c->nextSibling) {
        if (c->IsOutOfFlow()) continue;
        ++visibleCount;
        if (c->heightMode == SizeMode::Stretch) {
            stretchWeight += keel::max(0.001f, c->flexGrow);
        } else if (c->heightMode == SizeMode::Fixed) {
            fixedSum += c->fixedHeight;
        } else {
            fixedSum += c->intrinsicSize.y;
        }
    }
    const keel::f32 totalGap = (visibleCount > 1u) ? (keel::as<keel::f32>(visibleCount - 1u) * node->gap) : 0.0f;
    const keel::f32 freeH = keel::max(0.0f, inner.h - fixedSum - totalGap);
    keel::f32 cy = inner.y;
    for (Node* c = node->firstChild; c != nullptr; c = c->nextSibling) {
        if (c->IsOutOfFlow()) continue;
        keel::f32 ch = 0.0f;
        if (c->heightMode == SizeMode::Stretch && stretchWeight > 0.0f) {
            ch = freeH * (keel::max(0.001f, c->flexGrow) / stretchWeight);
        } else if (c->heightMode == SizeMode::Fixed) {
            ch = c->fixedHeight;
        } else {
            ch = c->intrinsicSize.y;
        }
        const keel::f32 cw = (c->widthMode == SizeMode::Fixed && c->fixedWidth > 0.0f)
            ? keel::min(inner.w, c->fixedWidth)
            : ((c->widthMode == SizeMode::Fit && c->intrinsicSize.x > 0.0f) ? keel::min(inner.w, c->intrinsicSize.x) : inner.w);
        Solver::Arrange(c, Rect(inner.x, cy, cw, keel::max(0.0f, ch)), theme, nullptr);
        cy += ch + node->gap;
    }
}

void RenderDockFloat(const Node* node, ICanvas& canvas, const UiTheme& theme) noexcept {
    if (!node) return;
    const auto* fp = node->As<DockFloatPayload>();
    if (!fp) return;
    const auto& pal = theme.palette;
    const ComponentMetrics& m = theme.GetMetrics(fp->size);

    const Rect tr = fp->titleRect;
    if (tr.w <= 2.0f || tr.h <= 2.0f) return;

    // Subtle title bar fill & bottom divider (clipped cleanly to card's rounded corners when present)
    const bool clipTitleRound = (!fp->expandedFullscreen && node->cornerRadius > 0.5f && canvas.SupportsRoundedClip());
    if (clipTitleRound) {
        const keel::f32 bw = keel::max(1.0f, node->borderWidth);
        canvas.PushRoundedClip(
            Rect(
                node->computedRect.x + bw,
                node->computedRect.y + bw,
                keel::max(0.0f, node->computedRect.w - bw * 2.0f),
                keel::max(0.0f, node->computedRect.h - bw * 2.0f)
            ),
            keel::max(0.0f, node->cornerRadius - bw)
        );
    }
    canvas.DrawRect(tr, pal.surfaceElevated.WithAlphaF(fp->isExpanded ? 0.92f : 0.75f));
    if (clipTitleRound) {
        canvas.PopClip();
    }
    theme.Painter().DrawDivider(canvas, theme, Vec2(tr.x, tr.Bottom()), Vec2(tr.Right(), tr.Bottom()), pal.borderSubtle, 1.0f);

    keel::f32 cx = tr.x + (fp->isExpanded ? 12.0f : 8.0f);
    keel::f32 rightLimit = tr.Right() - 8.0f;
    if (fp->expandRect.w > 0.0f) rightLimit = keel::min(rightLimit, fp->expandRect.x - 6.0f);
    if (fp->redockRect.w > 0.0f) rightLimit = keel::min(rightLimit, fp->redockRect.x - 6.0f);
    if (fp->closeRect.w > 0.0f)  rightLimit = keel::min(rightLimit, fp->closeRect.x - 6.0f);

    if (fp->icon.IsValid() && cx + m.iconSize < rightLimit) {
        const keel::f32 icSz = keel::min(m.iconSize, tr.h * 0.58f);
        fp->icon.Draw(canvas, Rect(cx, tr.Center().y - icSz * 0.5f, icSz, icSz), pal.textPrimary);
        cx += icSz + 6.0f;
    }

    const UiText title = !fp->title.empty() ? fp->title : UiText(fp->panelId.as_ref());
    const keel::f32 fontSize = theme.ResolveFontSize(title, TextRole::LabelLarge, fp->size);
    const keel::f32 badgeFont = theme.ResolveFontSize(fp->badge, m.fontSize * 0.76f);
    const keel::f32 badgeW = fp->badge.empty() ? 0.0f : (TextUtil::MeasureWidth(fp->badge, badgeFont, &canvas) + 12.0f);
    const bool showBadge = fp->isExpanded && !fp->badge.empty() && (rightLimit - cx > badgeW + 60.0f);
    const keel::f32 availW = keel::max(0.0f, rightLimit - cx - (showBadge ? (badgeW + 8.0f) : 0.0f));
    if (availW > 8.0f) {
        const keel::FixedString<256> trunc = TextUtil::TruncateWithEllipsis(title.text, availW, fontSize, &canvas);
        theme.Painter().DrawControlText(
            canvas,
            theme,
            trunc.as_ref(),
            Rect(cx, tr.y, availW, tr.h),
            fontSize,
            pal.textPrimary,
            fp->disabled,
            0.0f,
            0.5f
        );
        cx += keel::min(availW, canvas.MeasureText(trunc.as_ref(), fontSize).x);
    }

    if (showBadge) {
        cx += 8.0f;
        const keel::f32 badgeH = badgeFont + 5.0f;
        const Rect badgeR(cx, tr.Center().y - badgeH * 0.5f, badgeW, badgeH);
        theme.Painter().DrawBadge(canvas, theme, badgeR, pal.accent.WithAlphaF(0.20f), Color::Transparent(), 0.0f, 4.0f);
        canvas.DrawTextBox(fp->badge.text, badgeR, badgeFont, theme.EnsureReadableInk(pal.accent, pal.surfaceElevated), 0.5f, 0.5f);
    }

    // Expand / Fullscreen / Restore button
    if (fp->expandRect.w > 4.0f) {
        if (fp->expandHovered && !fp->disabled) {
            canvas.DrawRoundRect(fp->expandRect, keel::min(4.0f, m.cornerRadius), pal.surfaceHover);
        }
        const keel::f32 icSz = keel::max(9.0f, fp->expandRect.w * 0.54f);
        const Rect ir(
            fp->expandRect.Center().x - icSz * 0.5f,
            fp->expandRect.Center().y - icSz * 0.5f,
            icSz,
            icSz
        );
        const Color icCol = fp->expandHovered ? pal.textPrimary : pal.textSecondary;
        if (fp->isExpanded && fp->expandedFullscreen) {
            DrawRestoreSquaresIcon(canvas, ir, icCol);
        } else {
            DrawExpandSquareIcon(canvas, ir, icCol);
        }
    }

    // Redock button (normal floating cards)
    if (!fp->isExpanded && fp->allowDock && fp->redockRect.w > 4.0f) {
        if (fp->redockHovered && !fp->disabled) {
            canvas.DrawRoundRect(fp->redockRect, keel::min(4.0f, m.cornerRadius), pal.surfaceHover);
        }
        const keel::f32 icSz = keel::max(9.0f, fp->redockRect.w * 0.58f);
        const Rect ir(
            fp->redockRect.Center().x - icSz * 0.5f,
            fp->redockRect.Center().y - icSz * 0.5f,
            icSz,
            icSz
        );
        DrawDockBackIcon(canvas, ir, fp->redockHovered ? pal.accent : pal.textSecondary);
    }

    // Close button (or Return-to-dock × on expanded modal)
    if ((fp->isExpanded || fp->closable) && fp->closeRect.w > 4.0f) {
        if (fp->closeHovered && !fp->disabled) {
            canvas.DrawRoundRect(fp->closeRect, keel::min(4.0f, m.cornerRadius), pal.surfaceHover);
        }
        const keel::f32 icSz = keel::max(9.0f, fp->closeRect.w * 0.54f);
        const Rect ir(
            fp->closeRect.Center().x - icSz * 0.5f,
            fp->closeRect.Center().y - icSz * 0.5f,
            icSz,
            icSz
        );
        Icon::Close().Draw(canvas, ir, fp->closeHovered ? pal.textPrimary : pal.textSecondary);
    }

    // Subtle bottom-right resize grip indicator (normal floating cards only)
    if (!fp->isExpanded && !fp->minimized && fp->contentRect.w > 24.0f && fp->contentRect.h > 24.0f) {
        const Color gripCol = pal.textMuted.WithAlphaF(0.45f);
        const keel::f32 rx = node->computedRect.Right() - 4.0f;
        const keel::f32 ry = node->computedRect.Bottom() - 4.0f;
        canvas.DrawLine(Vec2(rx - 7.0f, ry), Vec2(rx, ry - 7.0f), gripCol, 1.25f);
        canvas.DrawLine(Vec2(rx - 3.5f, ry), Vec2(rx, ry - 3.5f), gripCol, 1.25f);
    }
}

// --- Expanded Modal Portal hooks (NodeType::Modal) ---

Rect GetExpandedModalBackdropRect(const Node* node, const UiTheme& /*theme*/) noexcept {
    if (!node) return Rect{};
    if (const auto* ep = node->As<ExpandedModalPayload>()) {
        return ep->backdropRect;
    }
    return node->computedRect;
}

void ArrangeExpandedModal(Node* node, const Rect& availableRect, const UiTheme& theme) noexcept {
    if (!node) return;
    const auto* ep = node->As<ExpandedModalPayload>();
    const Rect backdrop = (ep && ep->backdropRect.w > 1.0f && ep->backdropRect.h > 1.0f)
        ? ep->backdropRect
        : availableRect;
    node->computedRect = backdrop;

    const bool fs = ep ? ep->fullscreen : false;
    Rect cardRect = backdrop;
    if (!fs) {
        const keel::f32 marginX = keel::min(48.0f, backdrop.w * 0.08f);
        const keel::f32 marginY = keel::min(48.0f, backdrop.h * 0.08f);
        const keel::f32 maxW = keel::max(120.0f, backdrop.w - marginX * 2.0f);
        const keel::f32 maxH = keel::max(100.0f, backdrop.h - marginY * 2.0f);
        const keel::f32 w = keel::round(keel::clamp(backdrop.w * 0.84f, keel::min(360.0f, maxW), maxW));
        const keel::f32 h = keel::round(keel::clamp(backdrop.h * 0.84f, keel::min(260.0f, maxH), maxH));
        cardRect = Rect(
            keel::round(backdrop.x + (backdrop.w - w) * 0.5f),
            keel::round(backdrop.y + (backdrop.h - h) * 0.5f),
            w,
            h
        );
    }

    DockAreaPersistState* st = ep ? ep->state : nullptr;
    if (st) {
        st->expandedBackdropRect = backdrop;
        st->expandedCardRect = cardRect;
        st->expandedTitleRect = Rect{};
        st->expandedContentRect = cardRect;
        st->expandedFullscreenBtnRect = Rect{};
        st->expandedCloseBtnRect = Rect{};
    }

    for (Node* c = node->firstChild; c != nullptr; c = c->nextSibling) {
        if (c->IsOutOfFlow()) continue;
        Solver::Arrange(c, cardRect, theme, ep ? ep->canvas : nullptr);
        if (st) {
            if (const auto* fp = c->As<DockFloatPayload>()) {
                st->expandedTitleRect = fp->titleRect;
                st->expandedContentRect = fp->contentRect;
                st->expandedFullscreenBtnRect = fp->expandRect;
                st->expandedCloseBtnRect = fp->closeRect;
            }
        }
    }
}

void RenderExpandedModal(const Node* node, ICanvas& canvas, const UiTheme& /*theme*/) noexcept {
    const auto* ep = node ? node->As<ExpandedModalPayload>() : nullptr;
    if (!ep || ep->fullscreen) return;
    if (ep->backdropRect.w <= 0.0f || ep->backdropRect.h <= 0.0f) return;
    const keel::f32 alpha = keel::clamp(ep->backdropOpacity, 0.0f, 0.95f);
    if (alpha > 0.001f) {
        canvas.DrawRect(
            ep->backdropRect,
            Color(4, 7, 14, keel::as<keel::u8>(keel::clamp(keel::round_i32(alpha * 255.0f), 0, 255)))
        );
    }
}

// --- NodeType::DockArea Measure / Arrange / Render ---

Vec2 MeasureDockArea(
    const Node* node,
    keel::f32 /*availableWidth*/,
    ICanvas* /*canvas*/,
    const UiTheme& /*theme*/
) noexcept {
    if (!node) return {0.0f, 0.0f};
    keel::f32 w = (node->widthMode == SizeMode::Fixed && node->fixedWidth > 0.0f) ? node->fixedWidth : 320.0f;
    keel::f32 h = (node->heightMode == SizeMode::Fixed && node->fixedHeight > 0.0f) ? node->fixedHeight : 240.0f;
    return {w, h};
}

void SortDockAreaChildrenForZOrder(Node* dockNode, const DockAreaPayload& payload) noexcept {
    if (!dockNode || dockNode->childCount <= 1u) return;

    // Collect children into a small stack/arena buffer and re-link: docked nodes first, then DockFloat by ascending zOrder
    keel::SmallArray<Node*, 32> docked;
    keel::SmallArray<Node*, 16> floats;
    for (Node* c = dockNode->firstChild; c != nullptr; c = c->nextSibling) {
        if (c->type == NodeType::DockFloat) {
            floats.push_back(c);
        } else {
            docked.push_back(c);
        }
    }

    // Insertion sort floats by layout zOrder (tiny count, zero allocations)
    auto getZ = [&](const Node* n) noexcept -> keel::u32 {
        const auto* fp = n->As<DockFloatPayload>();
        if (!fp || !payload.layout) return 0u;
        if (const DockFloatCard* fc = payload.layout->FindFloatCard(fp->panelId.as_ref())) {
            return fc->zOrder;
        }
        return 0u;
    };
    for (keel::usize i = 1u; i < floats.size(); ++i) {
        Node* key = floats[i];
        const keel::u32 keyZ = getZ(key);
        keel::usize j = i;
        while (j > 0u && getZ(floats[j - 1u]) > keyZ) {
            floats[j] = floats[j - 1u];
            --j;
        }
        floats[j] = key;
    }

    dockNode->firstChild = nullptr;
    dockNode->lastChild = nullptr;
    dockNode->childCount = 0u;
    auto append = [&](Node* c) noexcept {
        c->nextSibling = nullptr;
        c->parent = dockNode;
        c->childIndex = dockNode->childCount;
        if (!dockNode->firstChild) {
            dockNode->firstChild = c;
            dockNode->lastChild = c;
        } else {
            dockNode->lastChild->nextSibling = c;
            dockNode->lastChild = c;
        }
        ++dockNode->childCount;
    };
    for (Node* c : docked) append(c);
    for (Node* c : floats) append(c);
}

void ArrangeDockArea(
    Node* node,
    const Rect& availableRect,
    const UiTheme& theme
) noexcept {
    if (!node) return;
    auto* payload = node->As<DockAreaPayload>();
    if (!payload || !payload->layout || !payload->state || !payload->ctx) return;

    keel::f32 w = availableRect.w;
    keel::f32 h = availableRect.h;
    if (node->widthMode == SizeMode::Fixed && node->fixedWidth > 0.0f) w = node->fixedWidth;
    if (node->heightMode == SizeMode::Fixed && node->fixedHeight > 0.0f) h = node->fixedHeight;
    node->computedRect = Rect(availableRect.x, availableRect.y, w, h);

    const keel::f32 bw = node->borderWidth;
    const bool isFloatingShell = (theme.shellStyle == ShellStyle::Floating);
    const keel::f32 shellPad = isFloatingShell ? keel::max(0.0f, theme.shell.panelGap * 0.5f) : 0.0f;
    const Rect innerRect(
        node->computedRect.x + node->padding.left + bw + shellPad,
        node->computedRect.y + node->padding.top + bw + shellPad,
        keel::max(0.0f, node->computedRect.w - node->padding.Horizontal() - bw * 2.0f - shellPad * 2.0f),
        keel::max(0.0f, node->computedRect.h - node->padding.Vertical() - bw * 2.0f - shellPad * 2.0f)
    );
    payload->innerRect = innerRect;

    DockLayout& layout = *payload->layout;
    DockAreaPersistState& st = *payload->state;
    const DockAreaOptions& opts = payload->opts;
    const ComponentMetrics& m = theme.GetMetrics(opts.size);
    const keel::f32 sashPx = isFloatingShell
        ? keel::max(keel::round(opts.sashSize), keel::round(theme.shell.panelGap))
        : keel::max(2.0f, keel::round(opts.sashSize));
    const keel::f32 minPane = keel::max(48.0f, opts.minPaneSize);

    // Count groups and sashes in layout
    keel::usize maxGroups = 0u;
    keel::usize maxSashes = 0u;
    for (const auto& dn : layout.Nodes()) {
        if (dn.kind == DockNodeKind::Tabs) ++maxGroups;
        else ++maxSashes;
    }

    keel::Arena& arena = payload->ctx->GetArena();
    payload->groups = (maxGroups > 0u) ? arena.alloc_array<LaidOutGroup>(maxGroups) : nullptr;
    payload->groupCount = 0u;
    payload->sashes = (maxSashes > 0u) ? arena.alloc_array<LaidOutSash>(maxSashes) : nullptr;
    payload->sashCount = 0u;

    st.prevInnerRect = innerRect;
    st.prevGroups.clear();
    st.prevSashes.clear();
    st.prevFloats.clear();

    // Recursive tree layout
    auto layoutRec = [&](auto&& self, keel::u32 idx, Rect rect, keel::u32 depth) -> void {
        const auto nodes = layout.Nodes();
        if (idx >= nodes.size() || depth > 64u) return;
        const DockNode& dn = nodes[idx];

        if (dn.kind == DockNodeKind::Split) {
            const bool isHoriz = (dn.orientation == Orientation::Horizontal);
            const keel::f32 totalSpan = isHoriz ? rect.w : rect.h;
            const keel::f32 effSash = keel::min(sashPx, totalSpan);
            const keel::f32 availSpan = keel::max(0.0f, totalSpan - effSash);

            const keel::f32 minFirst = ComputeMinSubtreeSpan(nodes, dn.first, dn.orientation, minPane, sashPx);
            const keel::f32 minSecond = ComputeMinSubtreeSpan(nodes, dn.second, dn.orientation, minPane, sashPx);
            const keel::f32 r = ClampSplitForMinSizes(dn.ratio, availSpan, minFirst, minSecond);

            Rect firstR{}, sashR{}, secondR{};
            if (isHoriz) {
                const keel::f32 fw = keel::round(availSpan * r);
                const keel::f32 sw = keel::max(0.0f, availSpan - fw);
                firstR = Rect(rect.x, rect.y, fw, rect.h);
                sashR = Rect(rect.x + fw, rect.y, effSash, rect.h);
                secondR = Rect(sashR.Right(), rect.y, sw, rect.h);
            } else {
                const keel::f32 fh = keel::round(availSpan * r);
                const keel::f32 sh = keel::max(0.0f, availSpan - fh);
                firstR = Rect(rect.x, rect.y, rect.w, fh);
                sashR = Rect(rect.x, rect.y + fh, rect.w, effSash);
                secondR = Rect(rect.x, sashR.Bottom(), rect.w, sh);
            }

            if (payload->sashCount < maxSashes) {
                payload->sashes[payload->sashCount++] = LaidOutSash{
                    .nodeIdx = idx,
                    .orientation = dn.orientation,
                    .sashRect = sashR,
                    .hovered = (st.hoveredSashNode == idx),
                    .dragging = (st.activeSashNode == idx)
                };
            }
            st.prevSashes.push_back(PrevSashGeo{
                .nodeIdx = idx,
                .orientation = dn.orientation,
                .sashRect = sashR,
                .parentRect = rect,
                .minFirstSpan = minFirst,
                .minSecondSpan = minSecond,
                .availSpan = availSpan
            });

            self(self, dn.first, firstR, depth + 1u);
            self(self, dn.second, secondR, depth + 1u);
            return;
        }

        // Leaf Tab Group
        Rect groupRect = rect;
        if (st.animating && payload->animProgress < 1.0f) {
            const DockPanelId key = (dn.activeTab < dn.panelIds.size())
                ? dn.panelIds[dn.activeTab]
                : (dn.panelIds.empty() ? DockPanelId{} : dn.panelIds[0]);
            for (auto& pa : st.paneAnims) {
                if (pa.keyPanelId == key) {
                    groupRect = LerpRect(pa.fromRect, rect, EaseOutCubic(payload->animProgress));
                    pa.currentRect = groupRect;
                    break;
                }
            }
        }

        const bool showBar = opts.showTabBarForSingle || (dn.panelIds.size() > 1u);
        const keel::f32 paneBorder = isFloatingShell ? keel::max(1.0f, theme.shell.panelBorderWidth) : 1.0f;
        const keel::f32 barH = showBar ? keel::min(m.defaultHeight, keel::max(0.0f, groupRect.h - paneBorder * 2.0f)) : 0.0f;

        const Rect tabBarRect(
            groupRect.x + paneBorder,
            groupRect.y + paneBorder,
            keel::max(0.0f, groupRect.w - paneBorder * 2.0f),
            barH
        );
        const Rect contentRect(
            groupRect.x + paneBorder,
            tabBarRect.Bottom(),
            keel::max(0.0f, groupRect.w - paneBorder * 2.0f),
            keel::max(0.0f, groupRect.Bottom() - paneBorder - tabBarRect.Bottom())
        );

        const keel::usize tabCount = dn.panelIds.size();
        const DockPanelId activeId = (dn.activeTab < tabCount)
            ? dn.panelIds[dn.activeTab]
            : (tabCount > 0u ? dn.panelIds[0] : DockPanelId{});
        const RegisteredPanelEntry* activeMeta = !activeId.empty()
            ? FindRegisteredPanel(*payload, activeId.as_ref())
            : nullptr;
        const bool activeFloatable = !activeMeta || activeMeta->floatable;
        const bool activeExpandable = !activeMeta || activeMeta->expandable;
        const bool activeDisabledTab = (activeMeta && activeMeta->disabled);
        const bool activeIsExpanded = !activeId.empty() && opts.expandable && layout.IsPanelExpanded(activeId.as_ref());

        LaidOutTab* laidTabs = (tabCount > 0u) ? arena.alloc_array<LaidOutTab>(tabCount) : nullptr;
        PrevGroupGeo prevGrp{};
        prevGrp.nodeIdx = idx;
        prevGrp.activePanelId = activeId;
        prevGrp.groupRect = groupRect;
        prevGrp.tabBarRect = tabBarRect;
        prevGrp.tabStripRect = tabBarRect;
        prevGrp.contentRect = contentRect;
        prevGrp.activeFloatable = activeFloatable && !activeDisabledTab;
        prevGrp.activeExpandable = activeExpandable && !activeDisabledTab;

        Rect tabStripRect = tabBarRect;
        Rect overflowBtnRect{};
        Rect floatBtnRect{};
        Rect expandBtnRect{};
        bool hasOverflow = false;
        keel::f32 scrollX = 0.0f;
        keel::f32 maxScrollX = 0.0f;

        if (showBar && tabCount > 0u && tabBarRect.w > 4.0f) {
            const bool wantExpandBtn = opts.expandable && prevGrp.activeExpandable;
            const bool wantFloatBtn = opts.tearable && prevGrp.activeFloatable;
            const keel::f32 actBtnSz = keel::clamp(tabBarRect.h - 8.0f, 16.0f, 20.0f);
            const keel::f32 actGap = 2.0f;
            keel::f32 actionsW = 0.0f;
            if (wantExpandBtn) actionsW += actBtnSz + 3.0f;
            if (wantFloatBtn)  actionsW += actBtnSz + (wantExpandBtn ? actGap : 3.0f);
            const bool showActionBtns = (actionsW > 0.0f) && (tabBarRect.w >= 170.0f);
            const keel::f32 rightActionReserve = showActionBtns ? (actionsW + 4.0f) : 0.0f;

            const keel::f32 tabGap = (opts.tabStyle == TabBarStyle::Card || opts.tabStyle == TabBarStyle::Folder) ? 2.0f : 3.0f;
            keel::SmallArray<keel::f32, 16> naturalWidths;
            keel::SmallArray<keel::f32, 16> minWidths;
            keel::f32 sumNatural = 0.0f;
            keel::f32 sumMin = 0.0f;
            for (keel::usize ti = 0; ti < tabCount; ++ti) {
                const RegisteredPanelEntry* meta = FindRegisteredPanel(*payload, dn.panelIds[ti].as_ref());
                const UiText lbl = (meta && !meta->title.empty()) ? meta->title : UiText(dn.panelIds[ti].as_ref());
                const keel::f32 fontSize = theme.ResolveFontSize(lbl, TextRole::LabelMedium, opts.size);
                keel::f32 tw = m.paddingX * 1.7f + TextUtil::MeasureWidth(lbl, fontSize, payload->canvas);
                if (meta && meta->icon.IsValid()) tw += m.iconSize + 5.0f;
                if (meta && !meta->badge.empty()) {
                    tw += TextUtil::MeasureWidth(meta->badge, m.fontSize * 0.76f, payload->canvas) + 15.0f;
                }
                if (meta && meta->closable) tw += keel::max(12.0f, m.iconSize * 0.92f) + 6.0f;
                tw = keel::max(tw, 56.0f);
                const keel::f32 mw = keel::min(tw, kMinReadableTabWidthDp);
                naturalWidths.push_back(tw);
                minWidths.push_back(mw);
                sumNatural += tw;
                sumMin += mw;
            }
            const keel::f32 totalGaps = (tabCount > 1u) ? keel::as<keel::f32>(tabCount - 1u) * tabGap : 0.0f;
            const keel::f32 availFull = keel::max(0.0f, tabBarRect.w - rightActionReserve - 4.0f - totalGaps);

            keel::SmallArray<keel::f32, 16> finalWidths;
            bool placeActionBtns = showActionBtns;
            if (sumNatural <= availFull) {
                for (keel::usize ti = 0; ti < tabCount; ++ti) finalWidths.push_back(keel::round(naturalWidths[ti]));
                SetTabScrollX(st, idx, 0.0f);
                tabStripRect = Rect(tabBarRect.x, tabBarRect.y, keel::max(0.0f, tabBarRect.w - rightActionReserve), tabBarRect.h);
            } else if (sumMin <= availFull || tabCount == 1u) {
                const keel::f32 extraAvail = keel::max(0.0f, availFull - sumMin);
                const keel::f32 extraNeed = keel::max(1.0f, sumNatural - sumMin);
                const keel::f32 t = keel::clamp(extraAvail / extraNeed, 0.0f, 1.0f);
                for (keel::usize ti = 0; ti < tabCount; ++ti) {
                    const keel::f32 w = (tabCount == 1u)
                        ? keel::max(24.0f, availFull)
                        : (minWidths[ti] + (naturalWidths[ti] - minWidths[ti]) * t);
                    finalWidths.push_back(keel::round(w));
                }
                SetTabScrollX(st, idx, 0.0f);
                tabStripRect = Rect(tabBarRect.x, tabBarRect.y, keel::max(0.0f, tabBarRect.w - rightActionReserve), tabBarRect.h);
            } else {
                // Overflow mode: keep tabs readable, enable wheel scrolling + trailing overflow dropdown button
                // On narrow overflowing panes (< 280dp), hide Float/Expand buttons so '▼' sits cleanly at the right edge
                hasOverflow = true;
                placeActionBtns = showActionBtns && (tabBarRect.w >= 280.0f);
                const keel::f32 ovRightReserve = placeActionBtns ? (actionsW + 4.0f) : 0.0f;
                const keel::f32 btnW = keel::min(kOverflowBtnWidthDp, tabBarRect.w * 0.35f);
                overflowBtnRect = Rect(
                    tabBarRect.Right() - ovRightReserve - btnW,
                    tabBarRect.y + 1.0f,
                    btnW,
                    keel::max(0.0f, tabBarRect.h - 2.0f)
                );
                tabStripRect = Rect(tabBarRect.x, tabBarRect.y, keel::max(0.0f, overflowBtnRect.x - tabBarRect.x - 1.0f), tabBarRect.h);

                keel::f32 totalContentW = 0.0f;
                for (keel::usize ti = 0; ti < tabCount; ++ti) {
                    const keel::f32 w = keel::round(keel::clamp(naturalWidths[ti], minWidths[ti], 172.0f));
                    finalWidths.push_back(w);
                    totalContentW += w;
                }
                totalContentW += totalGaps;
                const keel::f32 visibleStripW = keel::max(0.0f, tabStripRect.w - 4.0f);
                maxScrollX = keel::max(0.0f, totalContentW - visibleStripW);
                scrollX = keel::clamp(GetTabScrollX(st, idx), 0.0f, maxScrollX);

                // Ensure active tab is scrolled into view when activated via click or overflow menu
                if (st.scrollEnsureActiveNode == idx && dn.activeTab < tabCount) {
                    keel::f32 actStart = 0.0f;
                    for (keel::usize ti = 0; ti < dn.activeTab; ++ti) {
                        actStart += finalWidths[ti] + tabGap;
                    }
                    const keel::f32 actEnd = actStart + finalWidths[dn.activeTab];
                    if (actStart < scrollX) {
                        scrollX = actStart;
                    } else if (actEnd > scrollX + visibleStripW) {
                        scrollX = actEnd - visibleStripW;
                    }
                    scrollX = keel::clamp(scrollX, 0.0f, maxScrollX);
                }
                SetTabScrollX(st, idx, scrollX);
            }

            if (placeActionBtns) {
                keel::f32 rx = tabBarRect.Right() - 4.0f;
                const keel::f32 by = keel::round(tabBarRect.Center().y - actBtnSz * 0.5f);
                if (wantExpandBtn) {
                    rx -= actBtnSz;
                    expandBtnRect = Rect(keel::round(rx), by, actBtnSz, actBtnSz);
                    rx -= actGap;
                }
                if (wantFloatBtn) {
                    rx -= actBtnSz;
                    floatBtnRect = Rect(keel::round(rx), by, actBtnSz, actBtnSz);
                }
            }

            keel::f32 tx = tabStripRect.x + 2.0f - scrollX;
            for (keel::usize ti = 0; ti < tabCount; ++ti) {
                const DockPanelId& pid = dn.panelIds[ti];
                const RegisteredPanelEntry* meta = FindRegisteredPanel(*payload, pid.as_ref());
                const Rect tr(tx, tabBarRect.y + 1.0f, keel::max(16.0f, finalWidths[ti]), keel::max(0.0f, tabBarRect.h - 1.0f));
                const bool closable = (meta && meta->closable);
                const Rect cr = closable ? ComputeTabCloseRect(tr, m) : Rect{};
                const bool isActive = (ti == dn.activeTab);
                const bool isHovered = (st.hoveredTabId == pid);

                laidTabs[ti] = LaidOutTab{
                    .panelId = pid,
                    .meta = meta,
                    .tabRect = tr,
                    .closeRect = cr,
                    .active = isActive,
                    .hovered = isHovered,
                    .closeHovered = isHovered && st.hoveredTabClose,
                    .beingDragged = st.tabDragging && !st.dragFromFloat && (st.dragPanelId == pid)
                };
                prevGrp.tabs.push_back(PrevTabGeo{
                    .panelId = pid,
                    .tabRect = tr,
                    .closeRect = cr,
                    .closable = closable,
                    .floatable = !meta || meta->floatable,
                    .allowDock = !meta || meta->allowDock,
                    .expandable = !meta || meta->expandable,
                    .active = isActive,
                    .disabled = (meta && meta->disabled)
                });
                tx += tr.w + tabGap;
            }
        }

        prevGrp.tabStripRect = tabStripRect;
        prevGrp.overflowBtnRect = overflowBtnRect;
        prevGrp.floatBtnRect = floatBtnRect;
        prevGrp.expandBtnRect = expandBtnRect;
        prevGrp.hasOverflow = hasOverflow;
        prevGrp.scrollX = scrollX;
        prevGrp.maxScrollX = maxScrollX;

        if (payload->groupCount < maxGroups) {
            payload->groups[payload->groupCount++] = LaidOutGroup{
                .nodeIdx = idx,
                .groupRect = groupRect,
                .tabBarRect = tabBarRect,
                .tabStripRect = tabStripRect,
                .overflowBtnRect = overflowBtnRect,
                .floatBtnRect = floatBtnRect,
                .expandBtnRect = expandBtnRect,
                .contentRect = contentRect,
                .tabs = laidTabs,
                .tabCount = tabCount,
                .showTabBar = showBar,
                .hasOverflow = hasOverflow,
                .overflowHovered = (st.hoveredOverflowNode == idx),
                .overflowOpen = (st.openOverflowNode == idx),
                .floatBtnHovered = (st.hoveredTabFloatNode == idx),
                .expandBtnHovered = (st.hoveredTabExpandNode == idx),
                .activeExpanded = activeIsExpanded
            };
        }
        st.prevGroups.push_back(keel::move(prevGrp));

        // Arrange the active panel's content node inside contentRect (unless currently expanded in modal overlay)
        if (!activeIsExpanded && activeMeta && activeMeta->contentNode && !activeMeta->isFloating) {
            activeMeta->contentNode->computedRect = contentRect;
            Solver::Arrange(activeMeta->contentNode, contentRect, theme, payload->canvas);
        }
    };

    if (layout.RootIndex() != kDockInvalidNode) {
        layoutRec(layoutRec, layout.RootIndex(), innerRect, 0u);
    }
    st.scrollEnsureActiveNode = kDockInvalidNode;

    // Arrange floating cards and snapshot their geometry
    const keel::f32 floatTitleH = keel::max(28.0f, m.defaultHeight);
    for (auto& fc : layout.MutableFloatingCards()) {
        if (opts.expandable && layout.IsPanelExpanded(fc.panelId.as_ref())) {
            continue;
        }
        fc.bounds = ClampFloatBounds(fc.bounds, innerRect);
        const keel::f32 cardH = fc.minimized ? (floatTitleH + 2.0f) : fc.bounds.h;
        const Rect cardRect(
            keel::round(innerRect.x + fc.bounds.x),
            keel::round(innerRect.y + fc.bounds.y),
            keel::round(fc.bounds.w),
            keel::round(cardH)
        );
        const RegisteredPanelEntry* meta = FindRegisteredPanel(*payload, fc.panelId.as_ref());
        Rect titleR{}, contentR{}, expandR{}, redockR{}, closeR{};
        if (meta && meta->contentNode && meta->isFloating) {
            if (auto* fp = meta->contentNode->As<DockFloatPayload>()) {
                fp->titleBarHeight = floatTitleH;
            }
            Solver::Arrange(meta->contentNode, cardRect, theme, payload->canvas);
            if (const auto* fp = meta->contentNode->As<DockFloatPayload>()) {
                titleR = fp->titleRect;
                contentR = fp->contentRect;
                expandR = fp->expandRect;
                redockR = fp->redockRect;
                closeR = fp->closeRect;
            }
        } else {
            titleR = Rect(cardRect.x + 1.0f, cardRect.y + 1.0f, keel::max(0.0f, cardRect.w - 2.0f), floatTitleH);
            contentR = Rect(cardRect.x + 1.0f, titleR.Bottom(), keel::max(0.0f, cardRect.w - 2.0f), keel::max(0.0f, cardRect.Bottom() - 1.0f - titleR.Bottom()));
        }
        st.prevFloats.push_back(PrevFloatGeo{
            .panelId = fc.panelId,
            .cardRect = cardRect,
            .titleRect = titleR,
            .contentRect = contentR,
            .expandRect = expandR,
            .redockRect = redockR,
            .closeRect = closeR,
            .zOrder = fc.zOrder,
            .closable = (meta && meta->closable),
            .allowDock = !meta || meta->allowDock,
            .expandable = !meta || meta->expandable,
            .minimized = fc.minimized
        });
    }

    // Ensure child linked list is strictly z-ordered (docked first, then floating by ascending zOrder)
    SortDockAreaChildrenForZOrder(node, *payload);
}

void RenderDockArea(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) noexcept {
    if (!node) return;
    const auto* payload = node->As<DockAreaPayload>();
    if (!payload || !payload->layout) return;

    const auto& pal = theme.palette;
    const DockAreaOptions& opts = payload->opts;
    const bool isFloatingShell = (theme.shellStyle == ShellStyle::Floating);
    const keel::f32 paneRadius = isFloatingShell ? theme.shell.panelRadius : 0.0f;
    const SurfaceRole paneRole = isFloatingShell ? SurfaceRole::Flat : SurfaceRole::DockEdge;
    const keel::f32 paneBorderW = isFloatingShell ? keel::max(1.0f, theme.shell.panelBorderWidth) : 1.0f;

    // Empty docked workspace placeholder (when all panels are closed or floating)
    if (payload->groupCount == 0u && payload->innerRect.w > 24.0f && payload->innerRect.h > 24.0f) {
        theme.Painter().DrawSurface(canvas, theme, SurfacePaintParams{
            .rect = payload->innerRect,
            .role = SurfaceRole::Sunken,
            .radius = paneRadius,
            .explicitRadius = true,
            .bg = pal.background,
            .border = pal.borderSubtle,
            .borderWidth = 1.0f,
            .explicitBorder = true
        });
        const keel::f32 hintFont = theme.ResolveFontSize(UiText{}, TextRole::BodySmall, opts.size);
        canvas.DrawTextBox(
            "Drop a panel here to dock",
            payload->innerRect,
            hintFont,
            pal.textMuted,
            0.5f,
            0.5f
        );
    }

    // 1. Render every docked tab group surface, tab bar, and frame
    for (keel::usize gi = 0; gi < payload->groupCount; ++gi) {
        const LaidOutGroup& grp = payload->groups[gi];
        if (grp.groupRect.w <= 1.0f || grp.groupRect.h <= 1.0f) continue;

        const SurfacePaintParams surfParams{
            .rect = grp.groupRect,
            .role = paneRole,
            .radius = paneRadius,
            .explicitRadius = true,
            .bg = pal.surface,
            .border = pal.borderSubtle,
            .borderWidth = paneBorderW,
            .explicitBorder = true
        };
        theme.Painter().DrawSurfaceBg(canvas, theme, surfParams);

        if (grp.showTabBar && grp.tabBarRect.w > 2.0f && grp.tabBarRect.h > 2.0f) {
            if (isFloatingShell && paneRadius > 0.5f && canvas.SupportsRoundedClip()) {
                canvas.PushRoundedClip(
                    Rect(
                        grp.groupRect.x + paneBorderW,
                        grp.groupRect.y + paneBorderW,
                        keel::max(0.0f, grp.groupRect.w - paneBorderW * 2.0f),
                        keel::max(0.0f, grp.groupRect.h - paneBorderW * 2.0f)
                    ),
                    keel::max(0.0f, paneRadius - paneBorderW)
                );
            } else {
                canvas.PushClip(grp.tabBarRect);
            }

            canvas.DrawRect(grp.tabBarRect, pal.surfaceElevated.WithAlphaF(0.65f));
            theme.Painter().DrawTabBarBaseline(canvas, theme, grp.tabBarRect);

            // Clip tabs strictly to `tabStripRect` (leaving space for the overflow dropdown trigger when present)
            canvas.PushClip(grp.tabStripRect);
            for (keel::usize ti = 0; ti < grp.tabCount; ++ti) {
                const Rect& tr = grp.tabs[ti].tabRect;
                if (tr.Right() < grp.tabStripRect.x || tr.x > grp.tabStripRect.Right()) continue;
                DrawDockTab(canvas, theme, grp.tabs[ti], opts.tabStyle, opts.size, node->disabled);
            }
            canvas.PopClip();

            if (grp.hasOverflow && grp.overflowBtnRect.w > 4.0f) {
                const Rect ob = grp.overflowBtnRect;
                theme.Painter().DrawDivider(canvas, theme, Vec2(ob.x, ob.y + 3.0f), Vec2(ob.x, ob.Bottom() - 3.0f), pal.borderSubtle, 1.0f);
                if ((grp.overflowHovered || grp.overflowOpen) && !node->disabled) {
                    const Rect pill(ob.x + 2.0f, ob.y + 2.0f, keel::max(2.0f, ob.w - 4.0f), keel::max(2.0f, ob.h - 4.0f));
                    canvas.DrawRoundRect(pill, 4.0f, grp.overflowOpen ? pal.surfaceActive : pal.surfaceHover);
                }
                const keel::f32 icSz = keel::clamp(ob.w * 0.52f, 10.0f, 14.0f);
                const Rect icR(ob.Center().x - icSz * 0.5f, ob.Center().y - icSz * 0.5f, icSz, icSz);
                Icon::ChevronDown().Draw(
                    canvas,
                    icR,
                    node->disabled ? pal.textDisabled : ((grp.overflowHovered || grp.overflowOpen) ? pal.textPrimary : pal.textSecondary)
                );
            }

            if (grp.floatBtnRect.w > 4.0f) {
                const Rect fb = grp.floatBtnRect;
                if (grp.floatBtnHovered && !node->disabled) {
                    canvas.DrawRoundRect(fb, 4.0f, pal.surfaceHover);
                }
                const keel::f32 icSz = keel::clamp(fb.w * 0.58f, 9.0f, 12.0f);
                const Rect icR(fb.Center().x - icSz * 0.5f, fb.Center().y - icSz * 0.5f, icSz, icSz);
                DrawFloatDetachIcon(
                    canvas,
                    icR,
                    node->disabled ? pal.textDisabled : (grp.floatBtnHovered ? pal.textPrimary : pal.textSecondary)
                );
            }

            if (grp.expandBtnRect.w > 4.0f) {
                const Rect eb = grp.expandBtnRect;
                if (grp.expandBtnHovered && !node->disabled) {
                    canvas.DrawRoundRect(eb, 4.0f, pal.surfaceHover);
                }
                const keel::f32 icSz = keel::clamp(eb.w * 0.54f, 9.0f, 12.0f);
                const Rect icR(eb.Center().x - icSz * 0.5f, eb.Center().y - icSz * 0.5f, icSz, icSz);
                DrawExpandSquareIcon(
                    canvas,
                    icR,
                    node->disabled ? pal.textDisabled : (grp.expandBtnHovered ? pal.textPrimary : pal.textSecondary)
                );
            }
            canvas.PopClip();
        }

        if (grp.activeExpanded && grp.contentRect.w > 40.0f && grp.contentRect.h > 24.0f) {
            const keel::f32 hintFont = theme.ResolveFontSize(UiText{}, TextRole::BodySmall, opts.size);
            canvas.DrawTextBox("Expanded in overlay", grp.contentRect, hintFont, pal.textMuted, 0.5f, 0.5f);
        }

        theme.Painter().DrawSurfaceFrame(canvas, theme, surfParams);
    }

    // 2. Render split resize sashes via IStylePainter::DrawSplitSash
    for (keel::usize si = 0; si < payload->sashCount; ++si) {
        const LaidOutSash& s = payload->sashes[si];
        if (s.sashRect.w <= 0.0f || s.sashRect.h <= 0.0f) continue;
        theme.Painter().DrawSplitSash(
            canvas,
            theme,
            s.sashRect,
            s.orientation == Orientation::Horizontal,
            !isFloatingShell,
            s.hovered,
            s.dragging,
            node->disabled || !opts.resizable
        );
    }
}

// --- DockArea Open & Panel implementations ---

struct DockInteractOutcome {
    DockPanelId closedPanel{};
    bool layoutChanged{false};
    bool structuralChange{false};
};

void UpdateOverflowPopup(
    Context& ctx,
    DockLayout& layout,
    DockAreaPersistState& st,
    const DockAreaOptions& opts,
    const UiTheme& theme,
    Id idOverflowPopup,
    DockInteractOutcome& out
) {
    const DockNode& ovNode = layout.Nodes()[st.openOverflowNode];
    const ComponentMetrics& m = theme.GetMetrics(opts.size);
    const keel::f32 rowH = keel::max(26.0f, m.defaultHeight);
    const keel::f32 pad = 4.0f;
    const keel::f32 menuW = 220.0f;
    const keel::f32 maxMenuH = 300.0f;
    const keel::f32 menuH = keel::min(maxMenuH, pad * 2.0f + keel::as<keel::f32>(ovNode.panelIds.size()) * rowH);
    const Rect vp = ctx.GetPopupViewportRect();
    keel::f32 px = st.openOverflowAnchorRect.Right() - menuW;
    keel::f32 py = st.openOverflowAnchorRect.Bottom() + 4.0f;
    if (py + menuH > vp.Bottom() - 6.0f && st.openOverflowAnchorRect.y - 4.0f - menuH >= vp.y + 6.0f) {
        py = st.openOverflowAnchorRect.y - 4.0f - menuH;
    }
    px = keel::clamp(px, vp.x + 6.0f, keel::max(vp.x + 6.0f, vp.Right() - 6.0f - menuW));
    py = keel::clamp(py, vp.y + 6.0f, keel::max(vp.y + 6.0f, vp.Bottom() - 6.0f - menuH));
    const Rect popupRect(keel::round(px), keel::round(py), keel::round(menuW), keel::round(menuH));
    st.openOverflowPopupRect = popupRect;
    ctx.OpenPopup(idOverflowPopup, popupRect, st.openOverflowAnchorRect);

    auto popHit = ctx.ScopedOverlayHitCheck(idOverflowPopup, OverlayLayer::Popup);
    const Vec2 mouse = ctx.GetMousePos();
    if (ctx.ConsumeKey(Key::Escape)) {
        st.openOverflowNode = kDockInvalidNode;
        ctx.ClosePopup();
        ctx.InvalidateAll();
    } else if (popupRect.Contains(mouse)) {
        const keel::f32 relY = mouse.y - (popupRect.y + pad);
        if (relY >= 0.0f) {
            const keel::usize idxRow = keel::as<keel::usize>(relY / rowH);
            if (idxRow < ovNode.panelIds.size()) {
                st.hoveredOverflowItem = keel::as<keel::i32>(idxRow);
                const Rect rowR(popupRect.x + pad, popupRect.y + pad + keel::as<keel::f32>(idxRow) * rowH, popupRect.w - pad * 2.0f, rowH);
                const DockPanelId targetId = ovNode.panelIds[idxRow];
                bool itemClosable = false;
                for (const auto& grp : st.prevGroups) {
                    if (grp.nodeIdx != st.openOverflowNode) continue;
                    for (const auto& t : grp.tabs) {
                        if (t.panelId == targetId) {
                            itemClosable = t.closable;
                            break;
                        }
                    }
                    break;
                }
                const keel::f32 btnSz = 16.0f;
                const Rect closeR(rowR.Right() - btnSz - 6.0f, rowR.Center().y - btnSz * 0.5f, btnSz, btnSz);
                st.hoveredOverflowItemClose = itemClosable && closeR.Contains(mouse);
                ctx.SetCursor(CursorType::Pointer);

                if (ctx.IsMouseClicked()) {
                    if (st.hoveredOverflowItemClose) {
                        layout.ClosePanel(targetId.as_ref());
                        out.closedPanel = targetId;
                        out.layoutChanged = true;
                        out.structuralChange = true;
                        if (st.openOverflowNode >= layout.Nodes().size() ||
                            layout.Nodes()[st.openOverflowNode].kind != DockNodeKind::Tabs) {
                            st.openOverflowNode = kDockInvalidNode;
                            ctx.ClosePopup();
                        }
                    } else {
                        layout.SetActivePanel(targetId.as_ref());
                        st.lastActivePanelId = targetId;
                        st.scrollEnsureActiveNode = st.openOverflowNode;
                        st.openOverflowNode = kDockInvalidNode;
                        ctx.ClosePopup();
                    }
                    ctx.ConsumeMouseClick();
                    ctx.InvalidateAll();
                }
            }
        }
    } else if (ctx.IsMouseClicked() && !st.openOverflowAnchorRect.Contains(mouse)) {
        st.openOverflowNode = kDockInvalidNode;
        ctx.ClosePopup();
        ctx.InvalidateAll();
    }
}

void ApplyCommonDropTarget(
    DockLayout& layout,
    DockAreaPersistState& st,
    keel::StringRef panelId,
    const DockDropTarget& drop,
    DockInteractOutcome& out
) {
    if (drop.kind == DockDropKind::InsertTab) {
        if (layout.MovePanelToNode(panelId, drop.targetNodeIdx, DockSide::Center, 0.5f, drop.insertIndex)) {
            out.layoutChanged = true;
            out.structuralChange = true;
            st.scrollEnsureActiveNode = drop.targetNodeIdx;
        }
    } else if (drop.kind == DockDropKind::GroupCenter || drop.kind == DockDropKind::GroupSide) {
        if (layout.MovePanelToNode(panelId, drop.targetNodeIdx, drop.side, 0.5f)) {
            out.layoutChanged = true;
            out.structuralChange = true;
            st.scrollEnsureActiveNode = drop.targetNodeIdx;
        }
    } else if (drop.kind == DockDropKind::RootEdge) {
        if (layout.DockPanelToRoot(panelId, drop.side, 0.25f)) {
            out.layoutChanged = true;
            out.structuralChange = true;
        }
    }
}

[[nodiscard]] bool UpdateActiveDragOrResize(
    Context& ctx,
    DockLayout& layout,
    DockAreaPersistState& st,
    const DockAreaOptions& opts,
    Id idInteract,
    Vec2 mouse,
    DockInteractOutcome& out
) {
    // --- A. Continue active sash drag ---
    if (st.activeSashNode != kDockInvalidNode) {
        const PrevSashGeo* sashGeo = nullptr;
        for (const auto& s : st.prevSashes) {
            if (s.nodeIdx == st.activeSashNode) {
                sashGeo = &s;
                break;
            }
        }
        if (!sashGeo || ctx.IsMouseReleased() || !ctx.IsMouseDown()) {
            st.activeSashNode = kDockInvalidNode;
            if (ctx.GetActiveId() == idInteract) ctx.SetActiveId(Id::Null());
            ctx.RequestRepaint(2);
        } else {
            const bool isHoriz = (sashGeo->orientation == Orientation::Horizontal);
            ctx.SetCursor(isHoriz ? CursorType::ResizeEW : CursorType::ResizeNS);
            if (sashGeo->availSpan > 1.0f) {
                const keel::f32 delta = isHoriz
                    ? (mouse.x - st.sashDragStartMouse.x)
                    : (mouse.y - st.sashDragStartMouse.y);
                const keel::f32 rawRatio = st.sashDragStartRatio + delta / sashGeo->availSpan;
                const keel::f32 clamped = ClampSplitForMinSizes(
                    rawRatio,
                    sashGeo->availSpan,
                    sashGeo->minFirstSpan,
                    sashGeo->minSecondSpan
                );
                if (layout.SetSplitRatio(st.activeSashNode, clamped)) {
                    out.layoutChanged = true;
                    st.animating = false;
                    ctx.InvalidateAll();
                }
            }
        }
        return true;
    }

    // --- B. Continue active floating card resize ---
    if (!st.activeFloatResizeId.empty()) {
        DockFloatCard* card = layout.FindFloatCard(st.activeFloatResizeId.as_ref());
        if (!card || ctx.IsMouseReleased() || !ctx.IsMouseDown()) {
            st.activeFloatResizeId.clear();
            st.floatResizeMask = 0u;
            if (ctx.GetActiveId() == idInteract) ctx.SetActiveId(Id::Null());
            ctx.RequestRepaint(2);
        } else {
            ctx.SetCursor(ResizeMaskToCursor(st.floatResizeMask));
            const Vec2 delta = mouse - st.floatDragStartMouse;
            Rect r = st.floatResizeStartBounds;
            const keel::f32 minW = kFloatMinWidthDp;
            const keel::f32 minH = kFloatMinHeightDp;
            if ((st.floatResizeMask & kResizeLeft) != 0u) {
                const keel::f32 maxDx = r.w - minW;
                const keel::f32 dx = keel::min(delta.x, maxDx);
                r.x += dx;
                r.w -= dx;
            }
            if ((st.floatResizeMask & kResizeRight) != 0u) {
                r.w = keel::max(minW, r.w + delta.x);
            }
            if ((st.floatResizeMask & kResizeTop) != 0u) {
                const keel::f32 maxDy = r.h - minH;
                const keel::f32 dy = keel::min(delta.y, maxDy);
                r.y += dy;
                r.h -= dy;
            }
            if ((st.floatResizeMask & kResizeBottom) != 0u) {
                r.h = keel::max(minH, r.h + delta.y);
            }
            card->bounds = ClampFloatBounds(r, st.prevInnerRect);
            out.layoutChanged = true;
            ctx.InvalidateAll();
        }
        return true;
    }

    // --- C. Continue active floating card move (with live redock drop-zone detection!) ---
    if (!st.activeFloatMoveId.empty()) {
        DockFloatCard* card = layout.FindFloatCard(st.activeFloatMoveId.as_ref());
        if (!card) {
            st.activeFloatMoveId.clear();
            st.tabDragging = false;
            st.dragFromFloat = false;
            if (ctx.GetActiveId() == idInteract) ctx.SetActiveId(Id::Null());
        } else {
            const Vec2 delta = mouse - st.floatDragStartMouse;
            if (!st.tabDragging && delta.Length() >= kDragStartThresholdDp) {
                st.tabDragging = true;
                st.dragFromFloat = true;
                st.dragPanelId = st.activeFloatMoveId;
            }
            if (ctx.IsMouseReleased() || !ctx.IsMouseDown()) {
                if (st.tabDragging) {
                    const DockDropTarget drop = EvaluateDropTarget(st, layout, opts, mouse);
                    ApplyCommonDropTarget(layout, st, st.activeFloatMoveId.as_ref(), drop, out);
                }
                st.activeFloatMoveId.clear();
                st.tabDragging = false;
                st.dragFromFloat = false;
                st.dragPanelId.clear();
                if (ctx.GetActiveId() == idInteract) ctx.SetActiveId(Id::Null());
                ctx.InvalidateAll();
            } else {
                ctx.SetCursor(CursorType::Move);
                Rect next = st.floatDragStartBounds;
                next.x += delta.x;
                next.y += delta.y;
                card->bounds = ClampFloatBounds(next, st.prevInnerRect);
                if (st.tabDragging && opts.dockable && st.dragAllowDock) {
                    st.dropTarget = EvaluateDropTarget(st, layout, opts, mouse);
                }
                out.layoutChanged = true;
                ctx.InvalidateAll();
            }
        }
        return true;
    }

    // --- D. Continue pending or active tab drag-and-drop ---
    if (st.tabDragPending || st.tabDragging) {
        if (!st.tabDragging && (mouse - st.dragStartMouse).Length() >= kDragStartThresholdDp) {
            st.tabDragging = true;
        }
        if (ctx.IsMouseReleased() || !ctx.IsMouseDown()) {
            if (st.tabDragging) {
                const DockDropTarget drop = EvaluateDropTarget(st, layout, opts, mouse);
                if (drop.kind == DockDropKind::ReorderTab) {
                    if (layout.ReorderTab(drop.targetNodeIdx, st.dragSourceIndex, drop.insertIndex)) {
                        out.layoutChanged = true;
                        st.scrollEnsureActiveNode = drop.targetNodeIdx;
                    }
                } else if (drop.kind == DockDropKind::TearFloat && opts.tearable && st.dragFloatable) {
                    const Rect localRect(
                        drop.previewRect.x - st.prevInnerRect.x,
                        drop.previewRect.y - st.prevInnerRect.y,
                        drop.previewRect.w,
                        drop.previewRect.h
                    );
                    if (layout.FloatPanel(st.dragPanelId.as_ref(), localRect)) {
                        out.layoutChanged = true;
                        out.structuralChange = true;
                    }
                } else {
                    ApplyCommonDropTarget(layout, st, st.dragPanelId.as_ref(), drop, out);
                }
                ctx.InvalidateAll();
            }
            st.tabDragPending = false;
            st.tabDragging = false;
            st.dragFromFloat = false;
            st.dragPanelId.clear();
            if (ctx.GetActiveId() == idInteract) ctx.SetActiveId(Id::Null());
        } else if (st.tabDragging) {
            ctx.SetCursor(CursorType::Move);
            st.dropTarget = EvaluateDropTarget(st, layout, opts, mouse);
            ctx.RequestRepaint(2);
        }
        return true;
    }

    return false;
}

[[nodiscard]] bool HitTestIdleFloatingCards(
    Context& ctx,
    DockLayout& layout,
    DockAreaPersistState& st,
    const DockAreaOptions& opts,
    Id idInteract,
    Vec2 mouse,
    DockInteractOutcome& out
) {
    keel::SmallArray<keel::usize, 16> floatOrder;
    for (keel::usize i = 0; i < st.prevFloats.size(); ++i) floatOrder.push_back(i);
    for (keel::usize i = 1u; i < floatOrder.size(); ++i) {
        const keel::usize key = floatOrder[i];
        const keel::u32 keyZ = st.prevFloats[key].zOrder;
        keel::usize j = i;
        while (j > 0u && st.prevFloats[floatOrder[j - 1u]].zOrder < keyZ) {
            floatOrder[j] = floatOrder[j - 1u];
            --j;
        }
        floatOrder[j] = key;
    }

    for (const keel::usize fi : floatOrder) {
        const PrevFloatGeo& fg = st.prevFloats[fi];
        const keel::u8 resizeMask = HitTestFloatResizeEdges(fg.cardRect, mouse, fg.minimized);
        if (!fg.cardRect.Contains(mouse) && resizeMask == 0u) {
            continue;
        }
        st.hoveredFloatId = fg.panelId;

        if (ctx.IsMouseClicked() || ctx.IsRightMouseClicked() || ctx.IsMiddleMouseClicked()) {
            if (layout.BringFloatToFront(fg.panelId.as_ref())) {
                ctx.InvalidateAll();
            }
        }

        if (fg.closable && fg.closeRect.Contains(mouse)) {
            st.hoveredFloatClose = true;
            ctx.SetCursor(CursorType::Pointer);
            if (ctx.IsMouseClicked()) {
                layout.ClosePanel(fg.panelId.as_ref());
                out.closedPanel = fg.panelId;
                out.layoutChanged = true;
                out.structuralChange = true;
                ctx.ConsumeMouseClick();
                ctx.InvalidateAll();
            }
            return true;
        }

        if (opts.expandable && fg.expandable && fg.expandRect.Contains(mouse)) {
            st.hoveredFloatExpand = true;
            ctx.SetCursor(CursorType::Pointer);
            ctx.SetTooltip("Expand panel", fg.expandRect);
            if (ctx.IsMouseClicked()) {
                if (layout.ExpandPanel(fg.panelId.as_ref(), false)) {
                    out.layoutChanged = true;
                    ctx.InvalidateAll();
                }
                ctx.ConsumeMouseClick();
            }
            return true;
        }

        if (fg.allowDock && fg.redockRect.Contains(mouse)) {
            st.hoveredFloatRedock = true;
            ctx.SetCursor(CursorType::Pointer);
            ctx.SetTooltip("Return panel to dock", fg.redockRect);
            if (ctx.IsMouseClicked()) {
                if (layout.RedockPanel(fg.panelId.as_ref())) {
                    out.layoutChanged = true;
                    out.structuralChange = true;
                    ctx.InvalidateAll();
                }
                ctx.ConsumeMouseClick();
            }
            return true;
        }

        if (resizeMask != 0u && !fg.titleRect.Contains(mouse)) {
            ctx.SetCursor(ResizeMaskToCursor(resizeMask));
            if (ctx.IsMouseClicked()) {
                if (const DockFloatCard* fc = layout.FindFloatCard(fg.panelId.as_ref())) {
                    st.activeFloatResizeId = fg.panelId;
                    st.floatResizeMask = resizeMask;
                    st.floatDragStartMouse = mouse;
                    st.floatResizeStartBounds = fc->bounds;
                    ctx.SetActiveId(idInteract);
                    ctx.ConsumeMouseClick();
                }
            }
            return true;
        }

        if (fg.titleRect.Contains(mouse)) {
            if (fg.closable && ctx.IsMiddleMouseClicked()) {
                layout.ClosePanel(fg.panelId.as_ref());
                out.closedPanel = fg.panelId;
                out.layoutChanged = true;
                out.structuralChange = true;
                ctx.ConsumeMouseClick();
                ctx.InvalidateAll();
            } else if (fg.allowDock && ctx.IsMouseDoubleClicked()) {
                if (layout.RedockPanel(fg.panelId.as_ref())) {
                    out.layoutChanged = true;
                    out.structuralChange = true;
                    ctx.InvalidateAll();
                }
                ctx.ConsumeMouseClick();
            } else if (ctx.IsMouseClicked()) {
                if (const DockFloatCard* fc = layout.FindFloatCard(fg.panelId.as_ref())) {
                    st.activeFloatMoveId = fg.panelId;
                    st.dragAllowDock = fg.allowDock;
                    st.dragFloatable = true;
                    st.floatDragStartMouse = mouse;
                    st.floatDragStartBounds = fc->bounds;
                    st.lastActivePanelId = fg.panelId;
                    ctx.SetActiveId(idInteract);
                    ctx.ConsumeMouseClick();
                }
            }
            return true;
        }

        // Mouse is inside floating card body: do NOT let docked sashes/tabs underneath react
        return true;
    }

    return false;
}

void UpdateIdleHitTest(
    Context& ctx,
    DockLayout& layout,
    DockAreaPersistState& st,
    const DockAreaOptions& opts,
    Id idInteract,
    Id idOverflowPopup,
    Vec2 mouse,
    DockInteractOutcome& out
) {
    // E1. Floating cards in descending zOrder
    if (HitTestIdleFloatingCards(ctx, layout, st, opts, idInteract, mouse, out)) {
        return;
    }

    // E2. Docked split sashes
    if (opts.resizable) {
        for (const auto& s : st.prevSashes) {
            if (!ExpandSashHit(s.sashRect, s.orientation, 3.0f).Contains(mouse)) continue;
            st.hoveredSashNode = s.nodeIdx;
            ctx.SetCursor(s.orientation == Orientation::Horizontal ? CursorType::ResizeEW : CursorType::ResizeNS);
            if (ctx.IsMouseDoubleClicked()) {
                const keel::f32 resetR = ClampSplitForMinSizes(0.5f, s.availSpan, s.minFirstSpan, s.minSecondSpan);
                if (layout.SetSplitRatio(s.nodeIdx, resetR)) {
                    out.layoutChanged = true;
                    out.structuralChange = true;
                    ctx.InvalidateAll();
                }
                ctx.ConsumeMouseClick();
            } else if (ctx.IsMouseClicked()) {
                const auto nodes = layout.Nodes();
                st.activeSashNode = s.nodeIdx;
                st.sashDragStartMouse = mouse;
                st.sashDragStartRatio = (s.nodeIdx < nodes.size()) ? nodes[s.nodeIdx].ratio : 0.5f;
                ctx.SetActiveId(idInteract);
                ctx.ConsumeMouseClick();
            }
            return;
        }
    }

    // E3. Docked tabs, horizontal overflow scroll & overflow dropdown button
    for (const auto& grp : st.prevGroups) {
        if (!grp.tabBarRect.Contains(mouse)) continue;

        // Mouse-wheel horizontal scroll on overflowing tab strips
        if (grp.hasOverflow && grp.maxScrollX > 0.5f) {
            const Vec2 wheel = ctx.GetMouseWheel();
            const keel::f32 delta = (keel::abs(wheel.x) > 0.01f) ? (-wheel.x * 40.0f) : (-wheel.y * 40.0f);
            if (keel::abs(delta) > 0.01f) {
                const keel::f32 nextScroll = keel::clamp(grp.scrollX + delta, 0.0f, grp.maxScrollX);
                if (keel::abs(nextScroll - grp.scrollX) > 0.25f) {
                    SetTabScrollX(st, grp.nodeIdx, nextScroll);
                    ctx.ConsumeMouseWheel();
                    ctx.InvalidateAll();
                }
            }
        }

        // Overflow dropdown trigger ('▼')
        if (grp.hasOverflow && grp.overflowBtnRect.Contains(mouse)) {
            st.hoveredOverflowNode = grp.nodeIdx;
            ctx.SetCursor(CursorType::Pointer);
            ctx.SetTooltip("Show all tabs in group", grp.overflowBtnRect);
            if (ctx.IsMouseClicked()) {
                if (st.openOverflowNode == grp.nodeIdx && ctx.IsPopupOpen(idOverflowPopup)) {
                    st.openOverflowNode = kDockInvalidNode;
                    ctx.ClosePopup();
                } else {
                    st.openOverflowNode = grp.nodeIdx;
                    st.openOverflowAnchorRect = grp.overflowBtnRect;
                    const Rect initialPopup(grp.overflowBtnRect.Right() - 220.0f, grp.overflowBtnRect.Bottom() + 4.0f, 220.0f, 160.0f);
                    st.openOverflowPopupRect = initialPopup;
                    ctx.OpenPopup(idOverflowPopup, initialPopup, grp.overflowBtnRect);
                }
                ctx.ConsumeMouseClick();
                ctx.InvalidateAll();
            }
            return;
        }

        // Trailing Expand button (Modal / Fullscreen overlay)
        if (opts.expandable && grp.activeExpandable && !grp.activePanelId.empty() && grp.expandBtnRect.Contains(mouse)) {
            st.hoveredTabExpandNode = grp.nodeIdx;
            ctx.SetCursor(CursorType::Pointer);
            ctx.SetTooltip("Expand panel (double-click tab)", grp.expandBtnRect);
            if (ctx.IsMouseClicked()) {
                if (layout.ExpandPanel(grp.activePanelId.as_ref(), false)) {
                    st.lastActivePanelId = grp.activePanelId;
                    out.layoutChanged = true;
                    ctx.InvalidateAll();
                }
                ctx.ConsumeMouseClick();
            }
            return;
        }

        // Trailing Float / Undock button (deliberate detach as floating card)
        if (opts.tearable && grp.activeFloatable && !grp.activePanelId.empty() && grp.floatBtnRect.Contains(mouse)) {
            st.hoveredTabFloatNode = grp.nodeIdx;
            ctx.SetCursor(CursorType::Pointer);
            ctx.SetTooltip("Detach as floating card", grp.floatBtnRect);
            if (ctx.IsMouseClicked()) {
                const Rect area = st.prevInnerRect;
                const keel::f32 fw = keel::clamp(area.w * 0.36f, kFloatMinWidthDp, 400.0f);
                const keel::f32 fh = keel::clamp(area.h * 0.46f, kFloatMinHeightDp, 320.0f);
                const Rect localFloat = ClampFloatBounds(
                    Rect((area.w - fw) * 0.5f, keel::max(24.0f, (area.h - fh) * 0.35f), fw, fh),
                    area
                );
                if (layout.FloatPanel(grp.activePanelId.as_ref(), localFloat)) {
                    st.lastActivePanelId = grp.activePanelId;
                    out.layoutChanged = true;
                    out.structuralChange = true;
                    ctx.InvalidateAll();
                }
                ctx.ConsumeMouseClick();
            }
            return;
        }

        if (!grp.tabStripRect.Contains(mouse)) return;

        for (keel::usize ti = 0; ti < grp.tabs.size(); ++ti) {
            const PrevTabGeo& t = grp.tabs[ti];
            if (!t.tabRect.Contains(mouse)) continue;
            st.hoveredTabId = t.panelId;
            if (t.disabled) return;

            if (t.closable && t.closeRect.Contains(mouse)) {
                st.hoveredTabClose = true;
                ctx.SetCursor(CursorType::Pointer);
                if (ctx.IsMouseClicked()) {
                    layout.ClosePanel(t.panelId.as_ref());
                    out.closedPanel = t.panelId;
                    out.layoutChanged = true;
                    out.structuralChange = true;
                    ctx.ConsumeMouseClick();
                    ctx.InvalidateAll();
                }
            } else if (t.closable && ctx.IsMiddleMouseClicked()) {
                layout.ClosePanel(t.panelId.as_ref());
                out.closedPanel = t.panelId;
                out.layoutChanged = true;
                out.structuralChange = true;
                ctx.ConsumeMouseClick();
                ctx.InvalidateAll();
            } else if (opts.expandable && t.expandable && ctx.IsMouseDoubleClicked()) {
                // Double-click on tab opens the panel in Expanded Modal overlay (without starting a drag)
                st.tabDragPending = false;
                st.tabDragging = false;
                if (layout.ExpandPanel(t.panelId.as_ref(), false)) {
                    st.lastActivePanelId = t.panelId;
                    out.layoutChanged = true;
                    ctx.InvalidateAll();
                }
                ctx.ConsumeMouseClick();
            } else if (ctx.IsMouseClicked()) {
                if (!t.active) {
                    layout.SetActivePanel(t.panelId.as_ref());
                    st.scrollEnsureActiveNode = grp.nodeIdx;
                    ctx.InvalidateAll();
                }
                st.lastActivePanelId = t.panelId;
                if (opts.reorderable || opts.dockable || (opts.tearable && t.floatable)) {
                    st.tabDragPending = true;
                    st.dragFromFloat = false;
                    st.dragPanelId = t.panelId;
                    st.dragSourceNode = grp.nodeIdx;
                    st.dragSourceIndex = ti;
                    st.dragStartMouse = mouse;
                    st.dragGrabOffset = mouse - t.tabRect.Pos();
                    st.dragFloatable = t.floatable;
                    st.dragAllowDock = t.allowDock;
                    ctx.SetActiveId(idInteract);
                }
                ctx.ConsumeMouseClick();
            }
            return;
        }
        return;
    }
}

void DrawOverflowMenuOverlay(
    ICanvas& c,
    const UiTheme& themeCopy,
    const DockAreaPayload* plPtr,
    Rect popRect,
    keel::u32 ovIdx,
    keel::i32 hovIdx,
    bool hovClose,
    UiSize szCopy
) {
    if (!plPtr || !plPtr->layout || ovIdx >= plPtr->layout->Nodes().size()) return;
    const DockNode& dn = plPtr->layout->Nodes()[ovIdx];
    if (dn.kind != DockNodeKind::Tabs) return;
    const auto& pal = themeCopy.palette;
    const ComponentMetrics& m = themeCopy.GetMetrics(szCopy);
    const keel::f32 pad = 4.0f;
    const keel::f32 rowH = keel::max(26.0f, m.defaultHeight);
    const keel::f32 fontSize = themeCopy.ResolveFontSize(UiText{}, TextRole::BodySmall, szCopy);

    themeCopy.Painter().DrawDropdownSurface(c, themeCopy, popRect, m.cornerRadius + 2.0f);
    c.PushClip(Rect(popRect.x + 1.0f, popRect.y + 1.0f, keel::max(0.0f, popRect.w - 2.0f), keel::max(0.0f, popRect.h - 2.0f)));
    for (keel::usize i = 0; i < dn.panelIds.size(); ++i) {
        const Rect rowR(popRect.x + pad, popRect.y + pad + keel::as<keel::f32>(i) * rowH, popRect.w - pad * 2.0f, rowH);
        if (rowR.Bottom() > popRect.Bottom()) break;
        const bool isAct = (i == dn.activeTab);
        const bool isHov = (keel::as<keel::i32>(i) == hovIdx);
        const bool solidSel = themeCopy.Painter().DrawSelectionHighlight(
            c, themeCopy, rowR, m.cornerRadius, isHov, isAct, false, false, false,
            themeCopy.GetSemanticAccent(UiVariant::Danger)
        );
        const Color ink = solidSel ? pal.accentText : ((isAct || isHov) ? pal.textPrimary : pal.textSecondary);
        const RegisteredPanelEntry* meta = FindRegisteredPanel(*plPtr, dn.panelIds[i].as_ref());

        keel::f32 cx = rowR.x + 8.0f;
        const keel::f32 icSz = keel::min(14.0f, m.iconSize);
        if (isAct) {
            Icon::Check().Draw(c, Rect(cx, rowR.Center().y - icSz * 0.5f, icSz, icSz), solidSel ? pal.accentText : pal.accent);
        } else if (meta && meta->icon.IsValid()) {
            meta->icon.Draw(c, Rect(cx, rowR.Center().y - icSz * 0.5f, icSz, icSz), ink);
        }
        cx += icSz + 7.0f;

        const bool closable = (meta && meta->closable);
        const keel::f32 rightLimit = closable ? (rowR.Right() - 26.0f) : (rowR.Right() - 8.0f);
        const UiText lbl = (meta && !meta->title.empty()) ? meta->title : UiText(dn.panelIds[i].as_ref());
        const keel::FixedString<128> trunc = TextUtil::TruncateWithEllipsis(lbl.text, keel::max(12.0f, rightLimit - cx), fontSize, &c);
        c.DrawTextBox(trunc.as_ref(), Rect(cx, rowR.y, keel::max(12.0f, rightLimit - cx), rowR.h), fontSize, ink, 0.0f, 0.5f);

        if (closable) {
            const keel::f32 btnSz = 16.0f;
            const Rect closeR(rowR.Right() - btnSz - 6.0f, rowR.Center().y - btnSz * 0.5f, btnSz, btnSz);
            if (isHov && hovClose) {
                c.DrawRoundRect(closeR, 3.0f, solidSel ? pal.accentText.WithAlphaF(0.20f) : pal.surfaceHover);
            }
            const keel::f32 xSz = 10.0f;
            Icon::Close().Draw(
                c,
                Rect(closeR.Center().x - xSz * 0.5f, closeR.Center().y - xSz * 0.5f, xSz, xSz),
                (isHov && hovClose) ? (solidSel ? pal.accentText : pal.textPrimary) : ink.WithAlphaF(0.75f)
            );
        }
    }
    c.PopClip();
}

Node* CreateDockScrollAreaNode(
    Context& ctx,
    Id scrollId,
    bool panelDisabled,
    const DockPanelOptions& opts,
    keel::StringRef panelId
) {
    Node* scrollNode = ctx.CreateNode(NodeType::ScrollArea, scrollId, panelDisabled);
    scrollNode->direction = FlexDirection::Column;
    scrollNode->widthMode = SizeMode::Stretch;
    scrollNode->heightMode = SizeMode::Stretch;
    scrollNode->padding = opts.padding.value_or(Padding::All(0.0f));
    scrollNode->gap = opts.gap.value_or(0.0f);
    scrollNode->clipChildren = true;
    scrollNode->SetSurfaceRole(SurfaceRole::None);
    scrollNode->scroll = ctx.AllocPayload<ScrollData>();
    scrollNode->scroll->orientation = Orientation::Vertical;
    scrollNode->scroll->policy = VisibilityPolicy::Auto;
    scrollNode->scroll->solverManaged = true;
    scrollNode->panelHooks = detail::ScrollArea::Hooks();
    auto& scrollState = ctx.GetOrCreatePersistentState<ScrollState>(scrollId);
    scrollNode->scroll->offset = scrollState.offset;
    scrollNode->visualHash = detail::HashArgs(panelId, scrollState.offset.x, scrollState.offset.y);
    return scrollNode;
}

void UpdateExpandedModalInteraction(
    Context& ctx,
    DockLayout& layout,
    DockAreaPersistState& st,
    const DockAreaOptions& opts,
    Id idExpandedModal,
    DockInteractOutcome& out
) {
    const Vec2 mouse = ctx.GetMousePos();
    const bool popupOpen = ctx.GetOpenPopupId().IsValid() || ctx.WasPopupDismissedThisFrame();
    const bool isTopModal = ctx.IsTopModal(idExpandedModal);
    const bool hasPopoverAbove = ctx.HasPrevFramePopoverOver(idExpandedModal);

    if (opts.expandCloseOnEscape && !popupOpen && isTopModal && !hasPopoverAbove && ctx.ConsumeKey(Key::Escape)) {
        layout.RestoreExpanded();
        ctx.ClearModalBlock(idExpandedModal);
        out.layoutChanged = true;
        ctx.InvalidateAll();
        return;
    }

    if (popupOpen || !isTopModal) {
        return;
    }

    auto hitScope = ctx.ScopedOverlayHitCheck(idExpandedModal, OverlayLayer::Portal);
    const bool occluded = ctx.IsOverlayOccluded(idExpandedModal, mouse, OverlayLayer::Portal);
    if (occluded) {
        return;
    }

    // Header Close / Return-to-dock button ('×')
    if (st.expandedCloseBtnRect.w > 1.0f && st.expandedCloseBtnRect.Contains(mouse)) {
        st.hoveredExpandedClose = true;
        ctx.SetCursor(CursorType::Pointer);
        ctx.SetTooltip("Return to dock", st.expandedCloseBtnRect);
        if (ctx.IsMouseClicked()) {
            layout.RestoreExpanded();
            ctx.ClearModalBlock(idExpandedModal);
            ctx.ConsumeMouseClick();
            out.layoutChanged = true;
            ctx.InvalidateAll();
        }
        return;
    }

    // Header Fullscreen / Restore button ('□' / '❐')
    if (st.expandedFullscreenBtnRect.w > 1.0f && st.expandedFullscreenBtnRect.Contains(mouse)) {
        st.hoveredExpandedFullscreen = true;
        ctx.SetCursor(CursorType::Pointer);
        ctx.SetTooltip(layout.IsExpandedFullscreen() ? "Exit fullscreen" : "Fullscreen", st.expandedFullscreenBtnRect);
        if (ctx.IsMouseClicked()) {
            layout.ToggleExpandedFullscreen();
            ctx.ConsumeMouseClick();
            out.layoutChanged = true;
            ctx.InvalidateAll();
        }
        return;
    }

    // Double-click on expanded modal header toggles Modal (~84%) <-> Fullscreen (100%)
    if (st.expandedTitleRect.w > 1.0f && st.expandedTitleRect.Contains(mouse)) {
        if (ctx.IsMouseDoubleClicked()) {
            layout.ToggleExpandedFullscreen();
            ctx.ConsumeMouseClick();
            out.layoutChanged = true;
            ctx.InvalidateAll();
        }
        return;
    }

    // Click on dimmed backdrop outside modal card restores panel to dock (when enabled)
    if (!layout.IsExpandedFullscreen() && opts.expandCloseOnBackdrop && ctx.IsMouseClicked()) {
        const bool hasPrevCard = (st.expandedCardRect.w > 1.0f && st.expandedCardRect.h > 1.0f);
        if (hasPrevCard && st.expandedBackdropRect.Contains(mouse) && !st.expandedCardRect.Contains(mouse)) {
            layout.RestoreExpanded();
            ctx.ClearModalBlock(idExpandedModal);
            ctx.ConsumeMouseClick();
            out.layoutChanged = true;
            ctx.InvalidateAll();
        }
    }
}

DockAreaScope OpenDockAreaImpl(
    Context& ctx,
    DockLayout* externalLayout,
    const DockAreaOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const Id idInteract(detail::HashArgs(id.value, keel::u64(0x444F434B5F494E54ull)));
    const Id idOverflowPopup(detail::HashArgs(id.value, keel::u64(0x444F434B5F4F5646ull)));
    const Id idExpandedModal(detail::HashArgs(id.value, keel::u64(0x444F434B5F455850ull)));

    auto& st = ctx.GetOrCreatePersistentState<DockAreaPersistState>(id);
    DockLayout& layout = externalLayout ? *externalLayout : st.ownedLayout;

    Node* node = ctx.CreateNode(NodeType::DockArea, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const bool activeDisabled = node->disabled;
    const keel::f64 nowSec = ctx.GetCurrentTimeSec();
    const keel::u64 revBefore = layout.Revision();

    Response res{};
    res.id = id;
    res.rect = ctx.GetPrevUnclippedBounds(id);
    res.ctx = &ctx;

    DockInteractOutcome outcome{};

    const bool prevSashHover = (st.hoveredSashNode != kDockInvalidNode);
    const DockPanelId prevTabHover = st.hoveredTabId;
    const bool prevTabCloseHover = st.hoveredTabClose;
    const DockPanelId prevFloatHover = st.hoveredFloatId;
    const bool prevFloatCloseHover = st.hoveredFloatClose;
    const bool prevFloatRedockHover = st.hoveredFloatRedock;
    const bool prevFloatExpandHover = st.hoveredFloatExpand;
    const keel::u32 prevOverflowHover = st.hoveredOverflowNode;
    const keel::u32 prevTabFloatHover = st.hoveredTabFloatNode;
    const keel::u32 prevTabExpandHover = st.hoveredTabExpandNode;
    const bool prevExpFsHover = st.hoveredExpandedFullscreen;
    const bool prevExpCloseHover = st.hoveredExpandedClose;

    st.hoveredSashNode = kDockInvalidNode;
    st.hoveredTabId.clear();
    st.hoveredTabClose = false;
    st.hoveredFloatId.clear();
    st.hoveredFloatClose = false;
    st.hoveredFloatRedock = false;
    st.hoveredFloatExpand = false;
    st.hoveredOverflowNode = kDockInvalidNode;
    st.hoveredTabFloatNode = kDockInvalidNode;
    st.hoveredTabExpandNode = kDockInvalidNode;
    st.hoveredExpandedFullscreen = false;
    st.hoveredExpandedClose = false;
    st.hoveredOverflowItem = -1;
    st.hoveredOverflowItemClose = false;
    st.dropTarget = DockDropTarget{};

    if (!opts.expandable && layout.HasExpandedPanel()) {
        layout.RestoreExpanded();
    }

    if (st.openOverflowNode != kDockInvalidNode) {
        const auto nodes = layout.Nodes();
        if (!ctx.IsPopupOpen(idOverflowPopup) ||
            st.openOverflowNode >= nodes.size() ||
            nodes[st.openOverflowNode].kind != DockNodeKind::Tabs) {
            st.openOverflowNode = kDockInvalidNode;
        }
    }

    // Handle active tab-overflow dropdown menu interaction
    if (!activeDisabled && st.openOverflowNode != kDockInvalidNode && ctx.IsPopupOpen(idOverflowPopup)) {
        UpdateOverflowPopup(ctx, layout, st, opts, theme, idOverflowPopup, outcome);
    }

    // Handle Expanded Modal / Fullscreen overlay interaction (before background dock hit-testing)
    const bool hadExpandedAtStart = opts.expandable && layout.HasExpandedPanel();
    if (hadExpandedAtStart) {
        // Cancel any lingering tab/float drag when expanded modal is open
        st.tabDragPending = false;
        st.tabDragging = false;
        st.activeFloatMoveId.clear();
        st.activeFloatResizeId.clear();
        st.activeSashNode = kDockInvalidNode;
        if (!activeDisabled) {
            UpdateExpandedModalInteraction(ctx, layout, st, opts, idExpandedModal, outcome);
        }
    } else if (ctx.HasPrevFrameModal(idExpandedModal)) {
        ctx.ClearModalBlock(idExpandedModal);
        ctx.InvalidateAll();
    }

    // Handle Escape to cancel in-progress tab/float drag
    if (!hadExpandedAtStart && (st.tabDragPending || st.tabDragging) && ctx.ConsumeKey(Key::Escape)) {
        st.tabDragPending = false;
        st.tabDragging = false;
        st.dragFromFloat = false;
        st.dragPanelId.clear();
        if (ctx.GetActiveId() == idInteract) {
            ctx.SetActiveId(Id::Null());
        }
        ctx.RequestRepaint(2);
    }

    if (!activeDisabled && !hadExpandedAtStart) {
        // Panel-layer hit scope allows DockArea to drive its own sashes, tab bars, and floating card
        // title bars/resize borders without being self-occluded by its own DockFloat children (zLayer=1),
        // while still respecting higher Modals, Popovers, and Popups.
        auto hitScope = ctx.ScopedOverlayHitCheck(idInteract, OverlayLayer::Panel);
        const Vec2 mouse = ctx.GetMousePos();
        const Id curActive = ctx.GetActiveId();
        const bool weOwnActive = (curActive == idInteract);
        const bool otherActive = curActive.IsValid() && !weOwnActive;
        const bool blockedByTopOverlay = !weOwnActive && ctx.IsOverlayOccluded(idInteract, mouse, OverlayLayer::Panel);

        if (!UpdateActiveDragOrResize(ctx, layout, st, opts, idInteract, mouse, outcome) &&
            !otherActive && !blockedByTopOverlay) {
            UpdateIdleHitTest(ctx, layout, st, opts, idInteract, idOverflowPopup, mouse, outcome);
        }
    }

    // Trigger or advance smooth pane transition animation
    if (outcome.structuralChange ||
        (layout.Revision() != st.lastLayoutRevision &&
         st.activeSashNode == kDockInvalidNode &&
         st.activeFloatMoveId.empty() &&
         st.activeFloatResizeId.empty() &&
         !layout.HasExpandedPanel())) {
        TriggerLayoutAnimation(st, layout, theme, nowSec, opts.animateLayout);
    }

    keel::f32 animProgress = 1.0f;
    if (st.animating) {
        if (!opts.animateLayout || st.animDurationSec <= 1e-4) {
            st.animating = false;
            st.paneAnims.clear();
        } else {
            const keel::f64 elapsed = nowSec - st.animStartTimeSec;
            if (elapsed >= st.animDurationSec) {
                st.animating = false;
                st.paneAnims.clear();
                animProgress = 1.0f;
            } else {
                animProgress = keel::clamp(keel::as<keel::f32>(elapsed / st.animDurationSec), 0.0f, 1.0f);
                ctx.RequestRepaint(1);
                ctx.InvalidateAll();
            }
        }
    }

    const bool hoverVisualChanged =
        (prevSashHover != (st.hoveredSashNode != kDockInvalidNode)) ||
        (prevTabHover != st.hoveredTabId) ||
        (prevTabCloseHover != st.hoveredTabClose) ||
        (prevFloatHover != st.hoveredFloatId) ||
        (prevFloatCloseHover != st.hoveredFloatClose) ||
        (prevFloatRedockHover != st.hoveredFloatRedock) ||
        (prevFloatExpandHover != st.hoveredFloatExpand) ||
        (prevOverflowHover != st.hoveredOverflowNode) ||
        (prevTabFloatHover != st.hoveredTabFloatNode) ||
        (prevTabExpandHover != st.hoveredTabExpandNode) ||
        (prevExpFsHover != st.hoveredExpandedFullscreen) ||
        (prevExpCloseHover != st.hoveredExpandedClose);
    if (hoverVisualChanged) {
        ctx.RequestRepaint(2);
    }

    // Configure DockArea container node
    node->widthMode = opts.width.ResolveMode(SizeMode::Stretch);
    node->fixedWidth = opts.width.Dp();
    node->heightMode = opts.height.ResolveMode(SizeMode::Stretch);
    node->fixedHeight = opts.height.Dp();
    if (opts.width.HasCustomStretchWeight()) {
        node->flexGrow = opts.width.StretchWeight();
    } else if (opts.height.HasCustomStretchWeight()) {
        node->flexGrow = opts.height.StretchWeight();
    }
    node->minWidth = opts.minWidth.value_or(0.0f);
    node->maxWidth = opts.maxWidth.value_or(0.0f);
    node->minHeight = opts.minHeight.value_or(0.0f);
    node->maxHeight = opts.maxHeight.value_or(0.0f);
    node->clipChildren = true;
    node->uiSize = opts.size;

    const Color resolvedBg = opts.background.IsBackgroundEnabled(false)
        ? theme.ResolveColor(opts.background, false, false, activeDisabled, theme.palette.background)
        : theme.palette.background;
    const Color resolvedBorder = (opts.border.width > 0.0f || opts.border.color.HasAny())
        ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, theme.palette.borderSubtle)
        : Color::Transparent();
    node->backgroundColor = resolvedBg;
    node->borderColor = resolvedBorder;
    node->borderWidth = (opts.border.width > 0.0f) ? opts.border.width : ((resolvedBorder.a > 0u) ? 1.0f : 0.0f);
    node->cornerRadius = opts.border.radius.value_or(0.0f);
    node->explicitBorder = opts.border.HasExplicitStroke() ? 1 : 0;
    node->explicitRadius = opts.border.radius.has_value() ? 1 : 0;
    node->SetSurfaceRole(SurfaceRole::Flat);

    auto* payload = ctx.AllocPayload<DockAreaPayload>();
    payload->opts = opts;
    payload->layout = &layout;
    payload->state = &st;
    payload->ctx = &ctx;
    payload->canvas = ctx.GetCanvas();
    payload->expandedModalId = idExpandedModal;
    payload->closedPanelThisFrame = outcome.closedPanel;
    payload->activePanelId = st.lastActivePanelId;
    payload->layoutChanged = outcome.layoutChanged || (layout.Revision() != revBefore);
    payload->animProgress = animProgress;

    node->SetPayload(payload);
    node->measureFn = &MeasureDockArea;
    node->arrangeFn = &ArrangeDockArea;
    node->renderFn = &RenderDockArea;
    node->visualHash = detail::HashArgs(
        layout.Revision(),
        keel::as<keel::u8>(theme.shellStyle),
        keel::as<keel::u8>(opts.tabStyle),
        opts.sashSize,
        opts.showTabBarForSingle,
        st.hoveredSashNode,
        st.activeSashNode,
        st.hoveredTabId.as_ref(),
        st.hoveredTabClose,
        st.hoveredFloatId.as_ref(),
        st.hoveredFloatClose,
        st.hoveredFloatRedock,
        st.hoveredFloatExpand,
        st.hoveredTabFloatNode,
        st.hoveredTabExpandNode,
        st.hoveredExpandedFullscreen,
        st.hoveredExpandedClose,
        layout.ExpandedPanelId(),
        layout.IsExpandedFullscreen(),
        st.tabDragging,
        keel::as<keel::u8>(st.dropTarget.kind),
        keel::as<keel::u8>(st.dropTarget.side),
        st.dropTarget.targetNodeIdx,
        st.dropTarget.insertIndex,
        st.hoveredOverflowNode,
        st.openOverflowNode,
        st.hoveredOverflowItem,
        animProgress
    );

    // Register overflow dropdown menu overlay when open
    if (st.openOverflowNode != kDockInvalidNode && ctx.IsPopupOpen(idOverflowPopup)) {
        const UiTheme themeCopy = theme;
        const Rect popRect = st.openOverflowPopupRect;
        const keel::u32 ovIdx = st.openOverflowNode;
        const keel::i32 hovIdx = st.hoveredOverflowItem;
        const bool hovClose = st.hoveredOverflowItemClose;
        const UiSize szCopy = opts.size;
        const DockAreaPayload* plPtr = payload;
        ctx.RegisterOverlay([=](ICanvas& c) {
            DrawOverflowMenuOverlay(c, themeCopy, plPtr, popRect, ovIdx, hovIdx, hovClose, szCopy);
        }, OverlayLayer::Popup);
    }

    // Register drag-and-drop overlay pass on OverlayLayer::Popup while dragging a tab or moving a float card
    if (st.tabDragging && !st.dragPanelId.empty()) {
        const Vec2 mouseNow = ctx.GetMousePos();
        const UiTheme themeCopy = theme;
        const DockDropTarget dropSnap = st.dropTarget;
        const Rect areaSnap = st.prevInnerRect;
        const DockAreaPersistState* stPtr = &st;
        const DockAreaPayload* plPtr = payload;
        const DockPanelId dragIdCopy = st.dragPanelId;
        const UiSize szCopy = opts.size;
        ctx.RegisterOverlay([=](ICanvas& c) {
            UiText lbl(dragIdCopy.as_ref());
            Icon ic{};
            if (const RegisteredPanelEntry* meta = FindRegisteredPanel(*plPtr, dragIdCopy.as_ref())) {
                if (!meta->title.empty()) lbl = meta->title;
                ic = meta->icon;
            }
            DrawDragOverlay(c, themeCopy, *stPtr, dropSnap, areaSnap, mouseNow, lbl, ic, szCopy);
        }, OverlayLayer::Popup);
    }

    ctx.FinishContainer(node);

    const keel::StringRef closedRef = outcome.closedPanel.empty()
        ? keel::StringRef{}
        : ctx.AllocString(outcome.closedPanel.as_ref());
    const keel::StringRef activeRef = st.lastActivePanelId.empty()
        ? keel::StringRef{}
        : ctx.AllocString(st.lastActivePanelId.as_ref());

    return DockAreaScope(&ctx, node, res, &layout, payload->layoutChanged, closedRef, activeRef);
}

} // namespace

DockAreaScope detail::DockArea::Open(
    Context& ctx,
    DockLayout& layout,
    const DockAreaOptions& opts,
    keel::SourceLocation loc
) {
    return OpenDockAreaImpl(ctx, &layout, opts, loc);
}

DockAreaScope detail::DockArea::Open(
    Context& ctx,
    const DockAreaOptions& opts,
    keel::SourceLocation loc
) {
    return OpenDockAreaImpl(ctx, nullptr, opts, loc);
}

DockPanelScope DockAreaScope::Panel(
    keel::StringRef panelId,
    const DockPanelOptions& opts,
    keel::SourceLocation loc
) {
    if (!m_ctx || !m_node) return DockPanelScope{};
    return detail::DockArea::OpenPanel(*m_ctx, m_node, m_layout, panelId, nullptr, opts, loc);
}

DockPanelScope DockAreaScope::Panel(
    keel::StringRef panelId,
    bool& open,
    const DockPanelOptions& opts,
    keel::SourceLocation loc
) {
    if (!m_ctx || !m_node) return DockPanelScope{};
    return detail::DockArea::OpenPanel(*m_ctx, m_node, m_layout, panelId, &open, opts, loc);
}

DockPanelScope detail::DockArea::OpenPanel(
    Context& ctx,
    Node* dockNode,
    DockLayout* layout,
    keel::StringRef panelId,
    bool* externalOpen,
    const DockPanelOptions& opts,
    keel::SourceLocation loc
) {
    if (!dockNode || !layout || !DockLayout::IsValidPanelId(panelId)) {
        return DockPanelScope{};
    }
    auto* payload = dockNode->As<DockAreaPayload>();
    if (!payload || !payload->state) return DockPanelScope{};

    DockAreaPersistState& st = *payload->state;
    const bool closedThisFrame = (payload->closedPanelThisFrame.as_ref() == panelId);

    // Synchronize optional external `bool& open`
    if (externalOpen != nullptr) {
        if (closedThisFrame) {
            *externalOpen = false;
        } else if (!*externalOpen) {
            if (layout->HasPanel(panelId)) {
                layout->ClosePanel(panelId);
            }
            return DockPanelScope(&ctx, nullptr, false, false, false, false, Rect{});
        } else if (layout->IsPanelClosed(panelId)) {
            layout->ReopenPanel(panelId);
        }
    }

    if (closedThisFrame) {
        return DockPanelScope(&ctx, nullptr, false, false, false, true, Rect{});
    }

    if (layout->IsPanelClosed(panelId)) {
        return DockPanelScope(&ctx, nullptr, false, false, false, false, Rect{});
    }

    // Auto-bind newly declared panels that aren't in DockLayout yet (works out of the box with empty DockLayout{})
    if (!layout->HasPanel(panelId)) {
        layout->DockPanel(panelId, DockSide::Center, {}, 0.5f);
        payload->layoutChanged = true;
    }

    // Show tab tooltip on hover
    if (st.hoveredTabId.as_ref() == panelId && !st.hoveredTabClose && !opts.tooltip.empty() && !st.tabDragging) {
        for (const auto& grp : st.prevGroups) {
            for (const auto& t : grp.tabs) {
                if (t.panelId.as_ref() == panelId) {
                    ctx.SetTooltip(opts.tooltip, t.tabRect);
                    break;
                }
            }
        }
    }

    const bool isFloating = layout->IsPanelFloating(panelId);
    const bool isExpanded = payload->opts.expandable && opts.expandable && layout->IsPanelExpanded(panelId);
    const bool otherExpandedFullscreen = payload->opts.expandable &&
        layout->HasExpandedPanel() &&
        layout->IsExpandedFullscreen() &&
        !isExpanded;
    const bool isVisible = isExpanded || (!otherExpandedFullscreen && layout->IsPanelActive(panelId));

    // Register panel metadata in arena array for TabBar / Floating Card rendering (even when inactive!)
    RegisteredPanelEntry* entry = FindRegisteredPanelMut(*payload, panelId);
    if (!entry) {
        if (payload->panelCount >= payload->panelCap) {
            const keel::usize newCap = (payload->panelCap == 0u) ? 16u : (payload->panelCap * 2u);
            auto* nextArr = ctx.GetArena().alloc_array<RegisteredPanelEntry>(newCap);
            for (keel::usize i = 0; i < payload->panelCount; ++i) {
                nextArr[i] = payload->panels[i];
            }
            payload->panels = nextArr;
            payload->panelCap = newCap;
        }
        entry = &payload->panels[payload->panelCount++];
    }

    const DockFloatCard* fc = isFloating ? layout->FindFloatCard(panelId) : nullptr;
    entry->panelId = DockPanelId(panelId);
    entry->title = opts.title.empty() ? UiText(ctx.AllocString(panelId)) : ctx.AllocText(opts.title);
    entry->icon = ctx.AllocIcon(opts.icon);
    entry->badge = ctx.AllocText(opts.badge);
    entry->tooltip = ctx.AllocText(opts.tooltip);
    entry->closable = opts.closable;
    entry->floatable = opts.floatable;
    entry->allowDock = opts.allowDock;
    entry->expandable = opts.expandable;
    entry->disabled = opts.disabled || dockNode->disabled;
    entry->isFloating = isFloating;
    entry->floatZOrder = fc ? fc->zOrder : 0u;
    entry->contentNode = nullptr;

    // Look up previous-frame content bounds for caller queries (`p.Bounds()`)
    Rect prevBounds{};
    if (isExpanded) {
        prevBounds = st.expandedContentRect;
    } else if (isFloating) {
        for (const auto& fg : st.prevFloats) {
            if (fg.panelId.as_ref() == panelId) {
                prevBounds = fg.contentRect;
                break;
            }
        }
    } else {
        for (const auto& grp : st.prevGroups) {
            for (const auto& t : grp.tabs) {
                if (t.panelId.as_ref() == panelId) {
                    prevBounds = grp.contentRect;
                    break;
                }
            }
        }
    }

    // Lazy execution: skip building child widget tree for hidden/inactive tabs (and occluded panes in fullscreen)
    if (!isVisible) {
        return DockPanelScope(&ctx, nullptr, false, false, isFloating, false, prevBounds);
    }

    // Wrap visible panel body in stable IdScope(panelId) so internal widget state (scroll, focus, cursor)
    // survives dragging between tab groups, floating cards, and expanded modal/fullscreen!
    ctx.PushId(panelId);
    const Id panelNodeId = ctx.MakeChildId(loc);
    const UiTheme& theme = dockNode->GetTheme(ctx.GetTheme());
    const bool panelDisabled = opts.disabled || dockNode->disabled;

    if (isExpanded) {
        const Rect vp = ctx.GetPopupViewportRect();
        const bool isFs = layout->IsExpandedFullscreen();
        const Id modalId = payload->expandedModalId;

        Node* modalNode = ctx.CreateNode(NodeType::Modal, modalId, false);
        modalNode->direction = FlexDirection::Column;
        modalNode->widthMode = SizeMode::Stretch;
        modalNode->heightMode = SizeMode::Stretch;
        modalNode->clipChildren = true;
        modalNode->uiSize = payload->opts.size;

        auto* ep = ctx.AllocPayload<ExpandedModalPayload>(ExpandedModalPayload{
            .state = &st,
            .backdropRect = vp,
            .fullscreen = isFs,
            .backdropOpacity = payload->opts.expandBackdropOpacity,
            .canvas = ctx.GetCanvas()
        });
        static constexpr PanelHooks kExpandedModalHooks{
            .contentClip = &GetExpandedModalBackdropRect
        };
        modalNode->SetPayload(ep);
        modalNode->arrangeFn = &ArrangeExpandedModal;
        modalNode->renderFn = &RenderExpandedModal;
        modalNode->panelHooks = &kExpandedModalHooks;
        modalNode->visualHash = detail::HashArgs(
            vp.x, vp.y, vp.w, vp.h,
            isFs,
            payload->opts.expandBackdropOpacity,
            panelId
        );

        ctx.BeginModalScope(modalId, vp);
        ctx.FinishContainer(modalNode);

        // Card container uses NodeType::Box (NOT DockFloat) so HitTestTree preserves Modal's zLayer=2 and portalId!
        const Id cardNodeId = opts.scrollable
            ? Id(detail::HashArgs(panelNodeId.value, keel::u64(0x45585043415244ull)))
            : panelNodeId;
        Node* cardNode = ctx.CreateNode(NodeType::Box, cardNodeId, panelDisabled);
        cardNode->direction = FlexDirection::Column;
        cardNode->widthMode = SizeMode::Stretch;
        cardNode->heightMode = SizeMode::Stretch;
        cardNode->padding = opts.scrollable ? Padding::All(0.0f) : opts.padding.value_or(Padding::All(0.0f));
        cardNode->gap = opts.scrollable ? 0.0f : opts.gap.value_or(0.0f);
        cardNode->clipChildren = opts.clipChildren;
        cardNode->backgroundColor = theme.palette.surface;
        cardNode->borderColor = isFs ? Color::Transparent() : theme.palette.border;
        cardNode->borderWidth = isFs ? 0.0f : 1.0f;
        cardNode->cornerRadius = isFs ? 0.0f : (theme.GetMetrics(payload->opts.size).cornerRadius + 4.0f);
        cardNode->explicitBorder = 1;
        cardNode->explicitRadius = 1;
        cardNode->SetSurfaceRole(isFs ? SurfaceRole::Flat : SurfaceRole::Popup);

        auto* fp = ctx.AllocPayload<DockFloatPayload>();
        fp->panelId = entry->panelId;
        fp->title = entry->title;
        fp->icon = entry->icon;
        fp->badge = entry->badge;
        fp->closable = true;
        fp->allowDock = false;
        fp->expandable = true;
        fp->isExpanded = true;
        fp->expandedFullscreen = isFs;
        fp->minimized = false;
        fp->disabled = panelDisabled;
        fp->closeHovered = st.hoveredExpandedClose;
        fp->expandHovered = st.hoveredExpandedFullscreen;
        fp->size = payload->opts.size;
        fp->titleBarHeight = keel::max(34.0f, theme.GetMetrics(payload->opts.size).defaultHeight + 4.0f);

        static constexpr PanelHooks kExpandedCardHooks{
            .contentClip = &DockFloatContentClip
        };
        cardNode->SetPayload(fp);
        cardNode->arrangeFn = &ArrangeDockFloat;
        cardNode->renderFn = &RenderDockFloat;
        cardNode->panelHooks = &kExpandedCardHooks;
        cardNode->visualHash = detail::HashArgs(
            panelId,
            isFs,
            fp->closeHovered,
            fp->expandHovered
        );

        entry->contentNode = cardNode;
        ctx.FinishContainer(cardNode);

        Node* outerCard = nullptr;
        Node* bodyNode = cardNode;
        if (opts.scrollable) {
            Node* scrollNode = CreateDockScrollAreaNode(ctx, panelNodeId, panelDisabled, opts, panelId);
            ctx.FinishContainer(scrollNode);
            outerCard = cardNode;
            bodyNode = scrollNode;
        }

        return DockPanelScope(
            &ctx,
            bodyNode,
            /*hasIdScope=*/true,
            /*active=*/true,
            /*floating=*/isFloating,
            /*closed=*/false,
            prevBounds,
            outerCard,
            modalNode,
            /*expanded=*/true
        );
    }

    Node* panelNode = nullptr;
    Node* outerFloatContainer = nullptr;
    if (isFloating) {
        // When scrollable, outer DockFloat uses a distinct frame Id so the inner ScrollArea
        // keeps `panelNodeId` and shares its exact `ScrollState` across dock <-> float transitions.
        const Id floatNodeId = opts.scrollable
            ? Id(detail::HashArgs(panelNodeId.value, keel::u64(0x464C4F4154ull)))
            : panelNodeId;
        panelNode = ctx.CreateNode(NodeType::DockFloat, floatNodeId, panelDisabled);
        panelNode->direction = FlexDirection::Column;
        panelNode->widthMode = SizeMode::Stretch;
        panelNode->heightMode = SizeMode::Stretch;
        panelNode->padding = opts.scrollable ? Padding::All(0.0f) : opts.padding.value_or(Padding::All(0.0f));
        panelNode->gap = opts.scrollable ? 0.0f : opts.gap.value_or(0.0f);
        panelNode->clipChildren = opts.clipChildren;
        panelNode->backgroundColor = theme.palette.surface;
        panelNode->borderColor = theme.palette.border;
        panelNode->borderWidth = 1.0f;
        panelNode->cornerRadius = theme.GetMetrics(payload->opts.size).cornerRadius + 2.0f;
        panelNode->explicitBorder = 1;
        panelNode->explicitRadius = 1;
        panelNode->SetSurfaceRole(SurfaceRole::Popup);

        auto* fp = ctx.AllocPayload<DockFloatPayload>();
        fp->panelId = entry->panelId;
        fp->title = entry->title;
        fp->icon = entry->icon;
        fp->badge = entry->badge;
        fp->closable = entry->closable;
        fp->allowDock = entry->allowDock;
        fp->expandable = payload->opts.expandable && entry->expandable;
        fp->minimized = fc ? fc->minimized : false;
        fp->disabled = panelDisabled;
        fp->closeHovered = (st.hoveredFloatId.as_ref() == panelId && st.hoveredFloatClose);
        fp->redockHovered = (st.hoveredFloatId.as_ref() == panelId && st.hoveredFloatRedock);
        fp->expandHovered = (st.hoveredFloatId.as_ref() == panelId && st.hoveredFloatExpand);
        fp->dragging = (st.activeFloatMoveId.as_ref() == panelId);
        fp->size = payload->opts.size;
        fp->titleBarHeight = keel::max(28.0f, theme.GetMetrics(payload->opts.size).defaultHeight);

        static constexpr PanelHooks kDockFloatHooks{
            .contentClip = &DockFloatContentClip
        };
        panelNode->SetPayload(fp);
        panelNode->arrangeFn = &ArrangeDockFloat;
        panelNode->renderFn = &RenderDockFloat;
        panelNode->panelHooks = &kDockFloatHooks;
        panelNode->visualHash = detail::HashArgs(
            panelId,
            fp->closeHovered,
            fp->redockHovered,
            fp->expandHovered,
            fp->dragging,
            fp->minimized,
            fc ? fc->zOrder : 0u
        );

        entry->contentNode = panelNode;
        ctx.FinishContainer(panelNode);

        if (opts.scrollable) {
            Node* scrollNode = CreateDockScrollAreaNode(ctx, panelNodeId, panelDisabled, opts, panelId);
            ctx.FinishContainer(scrollNode);
            outerFloatContainer = panelNode;
            panelNode = scrollNode;
        }

        return DockPanelScope(&ctx, panelNode, /*hasIdScope=*/true, /*active=*/true, /*floating=*/true, /*closed=*/false, prevBounds, outerFloatContainer);
    } else if (opts.scrollable) {
        panelNode = CreateDockScrollAreaNode(ctx, panelNodeId, panelDisabled, opts, panelId);
    } else {
        panelNode = ctx.CreateNode(NodeType::Box, panelNodeId, panelDisabled);
        panelNode->direction = FlexDirection::Column;
        panelNode->widthMode = SizeMode::Stretch;
        panelNode->heightMode = SizeMode::Stretch;
        panelNode->padding = opts.padding.value_or(Padding::All(0.0f));
        panelNode->gap = opts.gap.value_or(0.0f);
        panelNode->clipChildren = opts.clipChildren;
        panelNode->SetSurfaceRole(SurfaceRole::None);
    }

    entry->contentNode = panelNode;
    ctx.FinishContainer(panelNode);

    return DockPanelScope(&ctx, panelNode, /*hasIdScope=*/true, /*active=*/true, isFloating, /*closed=*/false, prevBounds);
}

} // namespace guikit
