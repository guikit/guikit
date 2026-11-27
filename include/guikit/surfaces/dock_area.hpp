#pragma once

#include "guikit/context.hpp"
#include "guikit/core/container_scope.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/style/tokens.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/small_array.h"
#include "keel/string.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;

/// Fixed-capacity UTF-8 panel identifier stored inline without heap allocations.
using DockPanelId = keel::FixedString<64>;

/// Sentinel index indicating an absent or uninitialized node in `DockLayout`.
inline constexpr keel::u32 kDockInvalidNode = 0xFFFFFFFFu;

/// Target side when docking or splitting a panel relative to another panel or the DockArea root.
enum class DockSide : keel::u8 {
    Center = 0, ///< Insert as a tab inside the target tab group
    Left,       ///< Split horizontally, placing the panel on the left
    Right,      ///< Split horizontally, placing the panel on the right
    Top,        ///< Split vertically, placing the panel on the top
    Bottom      ///< Split vertically, placing the panel on the bottom
};

/// Discriminator for nodes in the binary docking tree.
enum class DockNodeKind : keel::u8 {
    Tabs = 0, ///< Leaf tab group containing one or more docked panel IDs
    Split     ///< Internal binary split dividing space along `orientation`
};

/// Single node inside a `DockLayout` binary tree.
struct DockNode {
    DockNodeKind kind{DockNodeKind::Tabs};
    Orientation orientation{Orientation::Horizontal}; ///< Split axis when `kind == DockNodeKind::Split`
    keel::f32 ratio{0.5f};                            ///< First-child fraction clamped to [0.05..0.95]
    keel::u32 first{kDockInvalidNode};                ///< Left/Top child node index
    keel::u32 second{kDockInvalidNode};               ///< Right/Bottom child node index
    keel::SmallArray<DockPanelId, 8> panelIds;        ///< Ordered tab IDs when `kind == DockNodeKind::Tabs`
    keel::usize activeTab{0};                         ///< Selected tab index within `panelIds`
};

/// In-window floating card state for a torn-off dock panel.
struct DockFloatCard {
    DockPanelId panelId{};
    Rect bounds{40.0f, 40.0f, 320.0f, 240.0f}; ///< Bounds in logical dp relative to `DockArea`
    keel::u32 zOrder{0};                       ///< Higher values render & hit-test above lower values
    bool minimized{false};                     ///< True when collapsed to its title bar
    DockPanelId lastDockTarget{};              ///< Sibling panel remembered for one-click redock
    DockSide lastDockSide{DockSide::Center};   ///< Dock relation relative to `lastDockTarget`
    keel::f32 lastDockRatio{0.5f};             ///< Split ratio remembered if redocking via split
};

/// Declarative, serializable model of a binary docking split tree, tab groups, and floating cards.
/// Owned by the application (or persisted in `Context`'s `StateStore` via the stateless `DockArea(ui, opts)` overload).
class DockLayout {
public:
    DockLayout() = default;
    ~DockLayout() = default;
    DockLayout(DockLayout&&) noexcept = default;
    DockLayout& operator=(DockLayout&&) noexcept = default;
    DockLayout(const DockLayout&) = delete;
    DockLayout& operator=(const DockLayout&) = delete;

    /// Creates a deep copy of this layout (nodes, floating cards, and closed panel history).
    [[nodiscard]] DockLayout Clone() const;

    /// Resets the entire layout to empty (clears tree, floating cards, and closed history).
    void Clear() noexcept;

    /// Returns true if neither docked panels nor floating cards are present.
    [[nodiscard]] bool IsEmpty() const noexcept;

    /// Returns true if `id` is present anywhere in the layout (docked or floating).
    [[nodiscard]] bool HasPanel(keel::StringRef id) const noexcept;

    /// Returns true if `id` is currently docked inside the split tree.
    [[nodiscard]] bool HasDockedPanel(keel::StringRef id) const noexcept;

    /// Returns true if `id` is currently open as a floating card.
    [[nodiscard]] bool IsPanelFloating(keel::StringRef id) const noexcept;

    /// Returns true if `id` was explicitly closed via `ClosePanel()` (prevents auto-dock resurrection).
    [[nodiscard]] bool IsPanelClosed(keel::StringRef id) const noexcept;

    /// Returns true if `id` is the active tab of its docked group or is an open non-minimized floating card.
    [[nodiscard]] bool IsPanelActive(keel::StringRef id) const noexcept;

    /// Docks (or moves) `id` relative to `targetPanelId` (or the root when `targetPanelId` is empty).
    /// Also removes `id` from `closedPanels` and any previous docked/floating location.
    bool DockPanel(
        keel::StringRef id,
        DockSide side = DockSide::Center,
        keel::StringRef targetPanelId = {},
        keel::f32 ratio = 0.5f
    );

    /// Docks (or moves) `id` onto an outer edge of the entire root tree (`Left`, `Right`, `Top`, `Bottom`, or `Center`).
    bool DockPanelToRoot(keel::StringRef id, DockSide side, keel::f32 ratio = 0.25f);

    /// Docks (or moves) `id` relative to an explicit tab-group node index (`targetNodeIdx`).
    /// When `side == DockSide::Center`, optional `insertIndex` controls exact insertion position within the tab strip.
    bool MovePanelToNode(
        keel::StringRef id,
        keel::u32 targetNodeIdx,
        DockSide side = DockSide::Center,
        keel::f32 ratio = 0.5f,
        keel::usize insertIndex = keel::kInvalidIndex
    );

    /// Reorders a tab within a single tab-group node (`tabNodeIdx`).
    bool ReorderTab(keel::u32 tabNodeIdx, keel::usize fromIndex, keel::usize toIndex);

    /// Removes `id` from the docked tree or floating cards and collapses any orphaned split ancestors.
    bool RemovePanel(keel::StringRef id);

    /// Alias for `RemovePanel(id)`.
    bool UndockPanel(keel::StringRef id) { return RemovePanel(id); }

    /// Removes `id` from the layout and records it as closed so immediate-mode auto-binding will not re-add it.
    bool ClosePanel(keel::StringRef id);

    /// Reopens a previously closed panel into its remembered target (or the first tab group / root).
    bool ReopenPanel(keel::StringRef id);

    /// Detaches `id` into an in-window floating card at `localBounds` (remembering its prior dock target for redock).
    bool FloatPanel(keel::StringRef id, Rect localBounds);

    /// Returns floating card `id` back into the docked tree using its remembered target (or first tab group / root).
    bool RedockPanel(keel::StringRef id);

    /// Activates `id` in its tab group (or brings it to front if floating).
    bool SetActivePanel(keel::StringRef id);

    /// Raises floating card `id` above all other floating cards.
    bool BringFloatToFront(keel::StringRef id);

    /// Expands `id` into a focus modal overlay above the viewport (optionally immediately in fullscreen).
    /// Orthogonal to the dock tree and floating cards: restoring returns the panel to its exact prior location.
    bool ExpandPanel(keel::StringRef id, bool fullscreen = false);

    /// Toggles or sets fullscreen (100% viewport) mode for the currently expanded panel.
    bool SetExpandedFullscreen(bool fullscreen) noexcept;
    bool ToggleExpandedFullscreen() noexcept { return SetExpandedFullscreen(!m_expandedFullscreen); }

    /// Restores the currently expanded panel back to its docked or floating position.
    bool RestoreExpanded() noexcept;

    /// Queries for the currently expanded panel overlay state.
    [[nodiscard]] keel::StringRef ExpandedPanelId() const noexcept { return m_expandedPanelId.as_ref(); }
    [[nodiscard]] bool HasExpandedPanel() const noexcept { return !m_expandedPanelId.empty(); }
    [[nodiscard]] bool IsPanelExpanded(keel::StringRef id) const noexcept {
        return !id.empty() && m_expandedPanelId.as_ref() == id;
    }
    [[nodiscard]] bool IsExpandedFullscreen() const noexcept {
        return !m_expandedPanelId.empty() && m_expandedFullscreen;
    }

    /// Sets the split ratio of node `nodeIdx` (clamped to [0.05..0.95]).
    bool SetSplitRatio(keel::u32 nodeIdx, keel::f32 ratio) noexcept;

    /// Returns the index of the `Tabs` node hosting docked panel `id`, or `kDockInvalidNode`.
    [[nodiscard]] keel::u32 FindPanelNode(keel::StringRef id) const noexcept;

    /// Returns the first `Tabs` leaf node in the tree, or `kDockInvalidNode` if empty.
    [[nodiscard]] keel::u32 FindFirstTabNode() const noexcept;

    /// Finds a floating card by panel ID, or nullptr if not floating.
    [[nodiscard]] DockFloatCard* FindFloatCard(keel::StringRef id) noexcept;
    [[nodiscard]] const DockFloatCard* FindFloatCard(keel::StringRef id) const noexcept;

    /// Returns read-only access to the node pool, root index, floating cards, and closed IDs.
    [[nodiscard]] keel::ArrayRef<DockNode> Nodes() const noexcept { return m_nodes; }
    [[nodiscard]] keel::u32 RootIndex() const noexcept { return m_rootIndex; }
    [[nodiscard]] keel::ArrayRef<DockFloatCard> FloatingCards() const noexcept { return m_floating; }
    [[nodiscard]] keel::Array<DockFloatCard>& MutableFloatingCards() noexcept { return m_floating; }
    [[nodiscard]] keel::ArrayRef<DockPanelId> ClosedPanels() const noexcept { return m_closedPanels; }
    [[nodiscard]] keel::u64 Revision() const noexcept { return m_revision; }

    /// Serializes the layout to a compact, human-readable text format:
    /// - Dock tree only: `H(0.22,T(0;files),V(0.72,T(0;editor,preview),T(0;log,terminal)))`
    /// - With floating cards: `D2(H(...);F(inspector,420,80,320,260))`
    [[nodiscard]] keel::String SaveLayout() const;

    /// Strictly parses and validates a serialized layout string.
    /// Leaves the existing layout completely untouched and returns `false` on any syntax/validation error.
    bool LoadLayout(keel::StringRef serialized);

    /// Returns true if `id` is non-empty, fits `DockPanelId`, and contains no format control characters.
    [[nodiscard]] static bool IsValidPanelId(keel::StringRef id) noexcept;

private:
    struct ClosedPanelRecord {
        DockPanelId panelId{};
        DockPanelId lastDockTarget{};
        DockSide lastDockSide{DockSide::Center};
        keel::f32 lastDockRatio{0.5f};
    };

    void CaptureRestoreHint(
        keel::StringRef id,
        DockPanelId& outTarget,
        DockSide& outSide,
        keel::f32& outRatio
    ) const noexcept;
    bool RemoveFromDockedTree(keel::StringRef id);
    bool RemoveFromFloating(keel::StringRef id);
    void EraseClosedRecord(keel::StringRef id);
    keel::u32 FindParentSplit(keel::u32 childIdx) const noexcept;
    void CompactNodes();
    keel::u32 NextZOrder() const noexcept;

    keel::Array<DockNode> m_nodes;
    keel::u32 m_rootIndex{kDockInvalidNode};
    keel::Array<DockFloatCard> m_floating;
    keel::Array<DockPanelId> m_closedPanels;
    keel::Array<ClosedPanelRecord> m_closedHistory;
    DockPanelId m_expandedPanelId{};
    bool m_expandedFullscreen{false};
    keel::u64 m_revision{1};
};

/// Per-panel presentation & behavior options passed to `dock.Panel(id, opts)`.
struct DockPanelOptions {
    UiText title{};
    Icon icon{};
    UiText badge{};
    UiText tooltip{};
    bool closable{false};
    bool floatable{true};
    bool allowDock{true};
    bool expandable{true};
    keel::Optional<Padding> padding{Padding::All(0.0f)};
    keel::Optional<keel::f32> gap{0.0f};
    bool scrollable{false};
    bool clipChildren{true};
    bool disabled{false};
};

/// Configuration options for `guikit::DockArea`.
struct DockAreaOptions {
    keel::f32 sashSize{6.0f};          ///< Thickness of split resize sashes in logical dp
    keel::f32 minPaneSize{96.0f};      ///< Minimum width/height of any docked tab group in logical dp
    bool resizable{true};              ///< Allow dragging split sashes to resize panes
    bool reorderable{true};            ///< Allow dragging tabs within a tab group to reorder
    bool dockable{true};               ///< Allow dragging tabs between groups and splitting panes/edges
    bool tearable{true};               ///< Allow detaching a panel into an in-window floating card
    bool expandable{true};             ///< Allow double-clicking a tab or clicking Expand to open in modal/fullscreen
    bool expandCloseOnBackdrop{true};  ///< Clicking the dimmed backdrop outside the expanded modal restores the panel
    bool expandCloseOnEscape{true};    ///< Pressing Escape while a panel is expanded restores the panel to dock
    keel::f32 expandBackdropOpacity{0.58f}; ///< Dimmed backdrop alpha behind the expanded modal (0.0..0.95)
    bool showTabBarForSingle{true};    ///< Render the tab bar even when a group has only one panel
    bool animateLayout{true};          ///< Smoothly interpolate pane rects on structural layout changes
    TabBarStyle tabStyle{TabBarStyle::Underline};
    UiSize size{UiSize::Md};
    AxisSize width{SizeMode::Stretch};
    AxisSize height{SizeMode::Stretch};
    keel::Optional<keel::f32> minWidth{keel::kNullOpt};
    keel::Optional<keel::f32> maxWidth{keel::kNullOpt};
    keel::Optional<keel::f32> minHeight{keel::kNullOpt};
    keel::Optional<keel::f32> maxHeight{keel::kNullOpt};
    StateColor background{};
    Border border{};
    bool disabled{false};
};

/// RAII scope returned by `dock.Panel(id, opts)`.
/// Evaluates to `true` (`operator bool()`) only when the panel is currently visible (active tab or open floating card),
/// skipping child widget execution for hidden/inactive tabs.
class [[nodiscard]] DockPanelScope : public ContainerScope {
public:
    DockPanelScope() noexcept : ContainerScope(nullptr, nullptr) {}

    DockPanelScope(
        Context* ctx,
        Node* node,
        bool hasIdScope,
        bool active,
        bool floating,
        bool closed,
        Rect bounds,
        Node* outerContainer = nullptr,
        Node* modalContainer = nullptr,
        bool expanded = false
    ) noexcept
        : ContainerScope(ctx, node)
        , m_outerContainer(outerContainer)
        , m_modalContainer(modalContainer)
        , m_hasIdScope(hasIdScope)
        , m_active(active)
        , m_floating(floating)
        , m_expanded(expanded)
        , m_closed(closed)
        , m_bounds(bounds) {}

    ~DockPanelScope() noexcept {
        ClosePanel();
    }

    DockPanelScope(const DockPanelScope&) = delete;
    DockPanelScope& operator=(const DockPanelScope&) = delete;

    DockPanelScope(DockPanelScope&& other) noexcept
        : ContainerScope(keel::move(other))
        , m_outerContainer(other.m_outerContainer)
        , m_modalContainer(other.m_modalContainer)
        , m_hasIdScope(other.m_hasIdScope)
        , m_active(other.m_active)
        , m_floating(other.m_floating)
        , m_expanded(other.m_expanded)
        , m_closed(other.m_closed)
        , m_bounds(other.m_bounds) {
        other.m_outerContainer = nullptr;
        other.m_modalContainer = nullptr;
        other.m_hasIdScope = false;
        other.m_active = false;
        other.m_floating = false;
        other.m_expanded = false;
        other.m_closed = false;
    }

    DockPanelScope& operator=(DockPanelScope&& other) noexcept {
        if (this != &other) {
            ClosePanel();
            ContainerScope::operator=(keel::move(other));
            m_outerContainer = other.m_outerContainer;
            m_modalContainer = other.m_modalContainer;
            m_hasIdScope = other.m_hasIdScope;
            m_active = other.m_active;
            m_floating = other.m_floating;
            m_expanded = other.m_expanded;
            m_closed = other.m_closed;
            m_bounds = other.m_bounds;
            other.m_outerContainer = nullptr;
            other.m_modalContainer = nullptr;
            other.m_hasIdScope = false;
            other.m_active = false;
            other.m_floating = false;
            other.m_expanded = false;
            other.m_closed = false;
        }
        return *this;
    }

    /// Evaluates to true only when this panel is visible and its body should be emitted.
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return m_node != nullptr && m_active && !m_closed;
    }

    [[nodiscard]] constexpr bool IsActive() const noexcept { return m_active; }
    [[nodiscard]] constexpr bool IsFloating() const noexcept { return m_floating; }
    [[nodiscard]] constexpr bool IsExpanded() const noexcept { return m_expanded; }
    [[nodiscard]] constexpr bool IsClosed() const noexcept { return m_closed; }
    [[nodiscard]] constexpr bool Closed() const noexcept { return m_closed; }
    [[nodiscard]] constexpr Rect Bounds() const noexcept { return m_bounds; }

private:
    void ClosePanel() noexcept {
        Context* savedCtx = m_ctx;
        Node* outer = m_outerContainer;
        Node* modal = m_modalContainer;
        const bool popId = m_hasIdScope;
        m_outerContainer = nullptr;
        m_modalContainer = nullptr;
        m_hasIdScope = false;
        ContainerScope::Close();
        if (outer != nullptr && savedCtx != nullptr) {
            savedCtx->PopContainer(outer);
        }
        if (modal != nullptr && savedCtx != nullptr) {
            const Id modalId = modal->id;
            savedCtx->PopContainer(modal);
            savedCtx->EndModalScope(true, modalId);
        }
        if (popId && savedCtx != nullptr) {
            savedCtx->PopId();
        }
    }

    Node* m_outerContainer{nullptr};
    Node* m_modalContainer{nullptr};
    bool m_hasIdScope{false};
    bool m_active{false};
    bool m_floating{false};
    bool m_expanded{false};
    bool m_closed{false};
    Rect m_bounds{};
};

/// RAII scope returned by `guikit::DockArea(ui, layout, opts)`.
class [[nodiscard]] DockAreaScope : public ContainerScope {
public:
    Response response{};

    DockAreaScope() noexcept : ContainerScope(nullptr, nullptr) {}

    DockAreaScope(
        Context* ctx,
        Node* node,
        Response res,
        DockLayout* layout,
        bool layoutChanged,
        keel::StringRef closedPanelId,
        keel::StringRef activePanelId
    ) noexcept
        : ContainerScope(ctx, node)
        , response(res)
        , m_layout(layout)
        , m_layoutChanged(layoutChanged)
        , m_closedPanelId(closedPanelId)
        , m_activePanelId(activePanelId) {}

    DockAreaScope(DockAreaScope&& other) noexcept
        : ContainerScope(keel::move(other))
        , response(other.response)
        , m_layout(other.m_layout)
        , m_layoutChanged(other.m_layoutChanged)
        , m_closedPanelId(other.m_closedPanelId)
        , m_activePanelId(other.m_activePanelId) {
        other.m_layout = nullptr;
        other.m_layoutChanged = false;
    }

    DockAreaScope& operator=(DockAreaScope&& other) noexcept {
        if (this != &other) {
            ContainerScope::operator=(keel::move(other));
            response = other.response;
            m_layout = other.m_layout;
            m_layoutChanged = other.m_layoutChanged;
            m_closedPanelId = other.m_closedPanelId;
            m_activePanelId = other.m_activePanelId;
            other.m_layout = nullptr;
            other.m_layoutChanged = false;
        }
        return *this;
    }

    /// Registers a dockable panel and opens its scoped content container if currently visible.
    /// Wraps panel content in `ui.IdScope(panelId)` so widget state survives moves across groups & floating cards.
    [[nodiscard]] DockPanelScope Panel(
        keel::StringRef panelId,
        const DockPanelOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    /// Registers a dockable panel synchronized with an external `open` boolean.
    /// Clicking Close (`X`) sets `open = false`; setting `open = true` reopens the panel automatically.
    [[nodiscard]] DockPanelScope Panel(
        keel::StringRef panelId,
        bool& open,
        const DockPanelOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] constexpr bool LayoutChanged() const noexcept { return m_layoutChanged; }
    [[nodiscard]] constexpr keel::StringRef ClosedPanelId() const noexcept { return m_closedPanelId; }
    [[nodiscard]] constexpr keel::StringRef ActivePanelId() const noexcept { return m_activePanelId; }
    [[nodiscard]] DockLayout* Layout() const noexcept { return m_layout; }
    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

private:
    DockLayout* m_layout{nullptr};
    bool m_layoutChanged{false};
    keel::StringRef m_closedPanelId{};
    keel::StringRef m_activePanelId{};
};

namespace detail {

class DockArea {
public:
    [[nodiscard]] static DockAreaScope Open(
        Context& ctx,
        DockLayout& layout,
        const DockAreaOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] static DockAreaScope Open(
        Context& ctx,
        const DockAreaOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] static DockPanelScope OpenPanel(
        Context& ctx,
        Node* dockNode,
        DockLayout* layout,
        keel::StringRef panelId,
        bool* externalOpen,
        const DockPanelOptions& opts,
        keel::SourceLocation loc
    );
};

} // namespace detail

/// Opens an interactive docking workspace bound to an application-owned `DockLayout`.
[[nodiscard]] inline DockAreaScope DockArea(
    Context& ui,
    DockLayout& layout,
    const DockAreaOptions& opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::DockArea::Open(ui, layout, opts, loc);
}

/// Opens an interactive docking workspace whose `DockLayout` is managed automatically in `Context`'s `StateStore`.
[[nodiscard]] inline DockAreaScope DockArea(
    Context& ui,
    const DockAreaOptions& opts = {},
    keel::SourceLocation loc = keel::SourceLocation::current()
) {
    return detail::DockArea::Open(ui, opts, loc);
}

} // namespace guikit
