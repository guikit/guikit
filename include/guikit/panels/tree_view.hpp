#pragma once

#include "guikit/core/container_scope.hpp"
#include "guikit/core/id.hpp"
#include "guikit/core/response.hpp"
#include "guikit/core/theme.hpp"
#include "guikit/core/tokens.hpp"
#include "guikit/core/types.hpp"
#include "guikit/graphics/icon.hpp"
#include "guikit/panels/table.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/ref.h"
#include "keel/static_array.h"
#include "keel/string.h"

namespace guikit {

class Context;
class ICanvas;
struct Node;
struct TreeViewPayload;
struct TreeItemPayload;

using TreeColumn = TableColumn;

/// Visual style of hierarchy indentation guide lines in a TreeView.
enum class TreeGuides : keel::u8 {
    None = 0,   ///< Clean indentation without guide lines
    Vertical,   ///< Vertical indent guide lines along open branches
    Connected   ///< Vertical + horizontal L-shaped branch connectors to each child item
};

/// Visual highlight presentation for selected rows in a TreeView.
enum class TreeSelectionStyle : keel::u8 {
    Pill = 0,   ///< Rounded capsule highlight across the row
    Indicator   ///< Subtle tinted row background with a vertical accent bar on the left edge
};

/// Non-owning mutable UTF-8 string sink for `TreeViewOptions::filterText` (accepts `&myKeelString`,
/// `&mySmallString`, `&myFixedString`, `&myStdString`, `keel::StringSink`, or `nullptr`).
class TreeFilterSink {
public:
    constexpr TreeFilterSink() noexcept = default;
    constexpr TreeFilterSink(decltype(nullptr)) noexcept {}
    constexpr TreeFilterSink(keel::StringSink sink) noexcept : m_sink(sink) {}

    template <keel::WritableString S>
    constexpr TreeFilterSink(S* str) noexcept : m_sink(str ? keel::StringSink(*str) : keel::StringSink{}) {}

    template <keel::WritableString S>
    explicit constexpr TreeFilterSink(S& str) noexcept : m_sink(str) {}

    [[nodiscard]] constexpr bool valid() const noexcept { return m_sink.valid(); }
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return m_sink.valid(); }
    [[nodiscard]] constexpr bool operator==(decltype(nullptr)) const noexcept { return !m_sink.valid(); }
    [[nodiscard]] constexpr bool operator!=(decltype(nullptr)) const noexcept { return m_sink.valid(); }
    [[nodiscard]] constexpr keel::StringRef as_ref() const noexcept { return m_sink.as_ref(); }
    void assign(keel::StringRef text) const { m_sink.assign(text); }
    [[nodiscard]] constexpr const keel::StringSink& sink() const noexcept { return m_sink; }

private:
    keel::StringSink m_sink{};
};

/// Configuration options for a TreeView hierarchical container.
struct TreeViewOptions {
    UiText title{};                                                 ///< Optional header title displayed in the top bar
    keel::ArrayRef<TableColumn> columns{};                          ///< Optional multi-column definitions (Col 0 = Hierarchy, Col 1..N-1 = trailing cells)
    bool stickyHeader{true};                                        ///< Render column header bar when columns are configured
    bool resizableColumns{true};                                    ///< Allow dragging column dividers in the header bar to resize
    bool gridLines{false};                                          ///< Draw vertical column & horizontal row grid lines
    TreeGuides guides{TreeGuides::Connected};                       ///< Hierarchy guide line style
    TreeSelectionStyle selectionStyle{TreeSelectionStyle::Indicator}; ///< Selection highlight style
    StateColor color{};                                             ///< Accent color override for selection, guides, and checkboxes (Default = theme accent)
    keel::Optional<float> indent{keel::kNullOpt};                   ///< Per-level indentation in points (nullopt = derived from UiSize)
    keel::Optional<float> rowGap{2.0f};                             ///< Vertical gap between consecutive tree rows
    keel::Optional<Padding> padding{keel::kNullOpt};                ///< Internal viewport padding around tree items
    bool striped{false};                                            ///< Alternating subtle row background shading
    bool expandOnSelect{false};                                     ///< Clicking anywhere on a branch row also toggles open/collapsed state
    bool searchable{false};                                         ///< Show built-in search/filter bar at the top
    keel::StringRef searchPlaceholder{"Filter tree..."};            ///< Placeholder text for the search input
    TreeFilterSink filterText{};                                    ///< Optional bound external filter query string
    bool showToolbar{false};                                        ///< Show Expand All / Collapse All buttons in the header bar
    keel::i32* selectedIndex{nullptr};                                    ///< Optional bound 0-based index of the selected item (-1 = none)
    Id* selectedId{nullptr};                                        ///< Optional bound Id of the selected item
    VisibilityPolicy scrollbar{VisibilityPolicy::Auto};             ///< Scrollbar visibility policy
    StretchMode stretch{StretchMode::Fill};                         ///< Horizontal sizing mode
    keel::Optional<float> width{keel::kNullOpt};                    ///< Optional explicit width (px in Compact/Space, ratio 0.0..1.0 in Fill)
    keel::Optional<float> height{keel::kNullOpt};                   ///< Optional explicit height in points
    StateColor background{};                                        ///< Panel background color (Default = surface, None = transparent)
    Border border{.width = 1.0f};                                   ///< Outer frame border stroke & corner radius
    bool disabled{false};                                           ///< Disable the entire TreeView and all nested items
    const UiTheme* theme{nullptr};                                  ///< Optional scoped theme override
    keel::Optional<float> scale{keel::kNullOpt};                    ///< Optional local UI scale factor override
    keel::Optional<bool> debugBounds{keel::kNullOpt};               ///< Scoped debug bounding box overlay override
    keel::Optional<bool> inspectMode{keel::kNullOpt};               ///< Scoped interactive DevTools inspector override
};

/// Configuration options for an individual TreeItem (branch or leaf) inside a TreeView or container.
struct TreeItemOptions {
    UiText label{};                                 ///< Primary item title text
    UiText description{};                           ///< Optional secondary subtitle below label
    UiText detail{};                                ///< Optional inline metadata text on the right side of the row
    UiText badge{};                                 ///< Optional trailing pill badge / counter
    UiText tooltip{};                               ///< Optional floating tooltip on hover
    Icon icon{};                                    ///< Optional leading icon (used when collapsed or leaf)
    Icon openIcon{};                                ///< Optional leading icon when expanded (falls back to icon if invalid)
    StateColor color{};                             ///< Optional per-item accent color override
    bool defaultOpen{true};                         ///< Initial open state for branch nodes when managed by Context
    bool selected{false};                           ///< Explicit selection state override
    bool* checked{nullptr};                         ///< Optional bound boolean checkbox state (renders interactive checkbox when non-null)
    bool indeterminate{false};                      ///< Renders checkbox in indeterminate [-] tri-state
    bool forceBranch{false};                        ///< Forces rendering the expand/collapse chevron even if no child TreeItems are emitted
    keel::Optional<float> controlWidth{keel::kNullOpt}; ///< Optional explicit width in points for embedded trailing control slot
    StateColor background{};                        ///< Optional custom row background override
    Border border{};                                ///< Optional custom row border override
    bool disabled{false};                           ///< Disable this item and its children
    const UiTheme* theme{nullptr};                  ///< Optional scoped theme override
    keel::Optional<float> scale{keel::kNullOpt};    ///< Optional local UI scale factor override
};

/// Persistent per-TreeItem state stored in Context.
struct TreeItemState {
    bool open{true};
    bool hasChildItems{false};
    keel::u32 seenBulkToggleGen{0};
    Rect chevronRect{};
    Rect checkboxRect{};
    Rect controlsRect{};
};

/// Entry recorded for each visible TreeItem during a frame to power keyboard navigation on the next frame.
struct VisibleTreeItemEntry {
    Id idOuter{Id::Null()};
    Id idRow{Id::Null()};
    Id parentIdOuter{Id::Null()};
    keel::i32 index{0};
    keel::u16 depth{0};
    bool isBranch{false};
    bool isOpen{false};
    bool defaultOpen{true};
    bool* externalOpen{nullptr};
    bool* externalChecked{nullptr};
};

/// Persistent per-TreeView state stored in Context.
struct TreeViewState {
    static constexpr keel::usize kMaxColumns = 16;

    keel::SmallString<128> filterText{};
    Id selectedId{Id::Null()};
    keel::i32 selectedIndex{-1};
    keel::u32 bulkToggleGen{0};
    bool bulkToggleOpen{true};
    keel::Array<VisibleTreeItemEntry> prevVisibleItems{};

    keel::u16 userColCount{0};
    keel::StaticArray<float, kMaxColumns> userColWidths{};
    keel::StaticArray<bool, kMaxColumns> hasUserColWidth{};
    keel::i32 resizingCol{-1};
    float resizeStartX{0.0f};
    float resizeStartWidth{0.0f};
    double lastDividerClickTime{-10.0};
    keel::i32 lastDividerCol{-1};
    Rect lastHeaderRect{};
    float lastColOriginX{0.0f};
    float lastColTotalW{0.0f};
};

/// RAII scope returned by `ui.TreeView(...)`.
class [[nodiscard]] TreeViewScope : public ContainerScope {
public:
    TreeViewScope() noexcept : ContainerScope(nullptr, nullptr) {}
    TreeViewScope(Context* ctx, Node* scrollNode, TreeViewPayload* payload) noexcept
        : ContainerScope(ctx, scrollNode), m_payload(payload) {}

    ~TreeViewScope() noexcept;

    TreeViewScope(const TreeViewScope&) = delete;
    TreeViewScope& operator=(const TreeViewScope&) = delete;

    TreeViewScope(TreeViewScope&& other) noexcept
        : ContainerScope(keel::move(other)), m_payload(other.m_payload) {
        other.m_payload = nullptr;
    }

    TreeViewScope& operator=(TreeViewScope&& other) noexcept {
        if (this != &other) {
            Finalize();
            ContainerScope::operator=(keel::move(other));
            m_payload = other.m_payload;
            other.m_payload = nullptr;
        }
        return *this;
    }

    /// Imperatively expands all branch items inside this TreeView.
    void ExpandAll() noexcept;

    /// Imperatively collapses all branch items inside this TreeView.
    void CollapseAll() noexcept;

    /// Returns the active search/filter text for this TreeView.
    [[nodiscard]] keel::StringRef GetFilterText() const noexcept;

    /// Returns the number of visible TreeItems emitted so far in this TreeView during the current frame.
    [[nodiscard]] keel::i32 VisibleItemCount() const noexcept;

    /// Returns the 0-based traversal index of the currently selected item (-1 if none).
    [[nodiscard]] keel::i32 GetSelectedIndex() const noexcept;

    /// Returns the Id of the currently selected item (Id::Null() if none).
    [[nodiscard]] Id GetSelectedId() const noexcept;

private:
    void Finalize() noexcept;
    TreeViewPayload* m_payload{nullptr};
};

/// RAII scope returned by `ui.TreeItem(...)`.
/// Allows both embedding inline `Control` widgets onto the item's own row AND nesting child `ui.TreeItem(...)` branches.
class [[nodiscard]] TreeItemScope : public ContainerScope {
public:
    Response response{};
    bool open{true};
    bool toggled{false};
    bool selected{false};
    bool checkChanged{false};

    TreeItemScope() noexcept : ContainerScope(nullptr, nullptr), m_visible(false) {}
    TreeItemScope(
        Context* ctx,
        Node* rowNode,
        TreeItemPayload* payload,
        Response res,
        bool isOpen,
        bool wasToggled,
        bool isSelected,
        bool wasCheckChanged
    ) noexcept
        : ContainerScope(ctx, rowNode)
        , response(res)
        , open(isOpen)
        , toggled(wasToggled)
        , selected(isSelected)
        , checkChanged(wasCheckChanged)
        , m_payload(payload)
        , m_visible(rowNode != nullptr) {}

    ~TreeItemScope() noexcept;

    TreeItemScope(const TreeItemScope&) = delete;
    TreeItemScope& operator=(const TreeItemScope&) = delete;

    TreeItemScope(TreeItemScope&& other) noexcept
        : ContainerScope(keel::move(other))
        , response(other.response)
        , open(other.open)
        , toggled(other.toggled)
        , selected(other.selected)
        , checkChanged(other.checkChanged)
        , m_payload(other.m_payload)
        , m_visible(other.m_visible) {
        other.m_payload = nullptr;
        other.m_visible = false;
    }

    TreeItemScope& operator=(TreeItemScope&& other) noexcept {
        if (this != &other) {
            Finalize();
            ContainerScope::operator=(keel::move(other));
            response = other.response;
            open = other.open;
            toggled = other.toggled;
            selected = other.selected;
            checkChanged = other.checkChanged;
            m_payload = other.m_payload;
            m_visible = other.m_visible;
            other.m_payload = nullptr;
            other.m_visible = false;
        }
        return *this;
    }

    /// Evaluates to true when this item's row is visible (not inside a collapsed parent branch).
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return m_visible && m_node != nullptr;
    }

    [[nodiscard]] constexpr bool IsOpen() const noexcept { return open; }
    [[nodiscard]] constexpr bool IsSelected() const noexcept { return selected; }
    [[nodiscard]] const Response* operator->() const noexcept { return &response; }

    /// Opens a multi-widget cell container for the next column (`Col 1`, `Col 2`, ...) on this TreeItem row.
    /// Note: Single widgets (`ui.Text`, `ui.Element`, `ui.Checkbox`, `ui.ProgressBar`, `ui.Button`, etc.)
    /// placed directly inside `if (auto item = ui.TreeItem(...))` automatically occupy `Col 1..N-1` without needing `item.Cell()`.
    [[nodiscard]] ContainerScope Cell(
        const TableCellOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

private:
    void Finalize() noexcept;
    TreeItemPayload* m_payload{nullptr};
    bool m_visible{false};
};

/// Autonomous immediate-mode TreeView and TreeItem implementation.
class TreeView {
public:
    [[nodiscard]] static TreeViewScope Open(
        Context& ctx,
        const TreeViewOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] static TreeItemScope OpenItem(
        Context& ctx,
        bool* externalOpen,
        const TreeItemOptions& opts = {},
        keel::SourceLocation loc = keel::SourceLocation::current()
    );

    [[nodiscard]] static Vec2 MeasureRow(
        const Node* rowNode,
        float availableWidth,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept;

    static void ArrangeRow(
        Node* rowNode,
        const Rect& availableRect,
        const UiTheme& theme
    ) noexcept;

    static void RenderRow(
        const Node* rowNode,
        ICanvas& canvas,
        const UiTheme& theme
    ) noexcept;

    static void RenderHeader(
        const Node* headerNode,
        ICanvas& canvas,
        const UiTheme& theme
    ) noexcept;

    static void RenderOverlay(
        const Node* outerNode,
        ICanvas& canvas,
        const UiTheme& theme
    ) noexcept;

    static void RenderBranchGuides(
        const Node* branchNode,
        ICanvas& canvas,
        const UiTheme& theme
    ) noexcept;
};

} // namespace guikit
