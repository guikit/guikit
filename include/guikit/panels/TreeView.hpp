#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Id.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/graphics/Icon.hpp"
#include "guikit/panels/Table.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;
struct TreeViewPayload;
struct TreeItemPayload;

using TreeColumn = TableColumn;

/// Visual style of hierarchy indentation guide lines in a TreeView.
enum class TreeGuides : uint8_t {
    None = 0,   ///< Clean indentation without guide lines
    Vertical,   ///< Vertical indent guide lines along open branches
    Connected   ///< Vertical + horizontal L-shaped branch connectors to each child item
};

/// Visual highlight presentation for selected rows in a TreeView.
enum class TreeSelectionStyle : uint8_t {
    Pill = 0,   ///< Rounded capsule highlight across the row
    Indicator   ///< Subtle tinted row background with a vertical accent bar on the left edge
};

/// Configuration options for a TreeView hierarchical container.
struct TreeViewOptions {
    UiText title{};                                                 ///< Optional header title displayed in the top bar
    std::span<const TableColumn> columns{};                         ///< Optional multi-column definitions (Col 0 = Hierarchy, Col 1..N-1 = trailing cells)
    bool stickyHeader{true};                                        ///< Render column header bar when columns are configured
    bool resizableColumns{true};                                    ///< Allow dragging column dividers in the header bar to resize
    bool gridLines{false};                                          ///< Draw vertical column & horizontal row grid lines
    TreeGuides guides{TreeGuides::Connected};                       ///< Hierarchy guide line style
    TreeSelectionStyle selectionStyle{TreeSelectionStyle::Indicator}; ///< Selection highlight style
    UiVariant variant{UiVariant::Primary};                          ///< Accent color variant for selection, guides, and badges
    std::optional<float> indent{std::nullopt};                      ///< Per-level indentation in points (nullopt = derived from UiSize)
    std::optional<float> rowGap{2.0f};                              ///< Vertical gap between consecutive tree rows
    std::optional<Padding> padding{std::nullopt};                   ///< Internal viewport padding around tree items
    bool striped{false};                                            ///< Alternating subtle row background shading
    bool expandOnSelect{false};                                     ///< Clicking anywhere on a branch row also toggles open/collapsed state
    bool searchable{false};                                         ///< Show built-in search/filter bar at the top
    std::string_view searchPlaceholder{"Filter tree..."};           ///< Placeholder text for the search input
    std::string* filterText{nullptr};                               ///< Optional bound external filter query string
    bool showToolbar{false};                                        ///< Show Expand All / Collapse All buttons in the header bar
    int* selectedIndex{nullptr};                                    ///< Optional bound 0-based index of the selected item (-1 = none)
    Id* selectedId{nullptr};                                        ///< Optional bound Id of the selected item
    VisibilityPolicy scrollbar{VisibilityPolicy::Auto};             ///< Scrollbar visibility policy
    StretchMode stretch{StretchMode::Fill};                         ///< Horizontal sizing mode
    std::optional<float> width{std::nullopt};                       ///< Optional explicit width (px in Compact/Space, % in Fill)
    std::optional<float> height{std::nullopt};                      ///< Optional explicit height in points
    StateColor background{};                                        ///< Panel background color (Default = surface, None = transparent)
    Border border{.width = 1.0f};                                   ///< Outer frame border stroke & corner radius
    bool disabled{false};                                           ///< Disable the entire TreeView and all nested items
    const UiTheme* theme{nullptr};                                  ///< Optional scoped theme override
    std::optional<float> scale{std::nullopt};                       ///< Optional local UI scale factor override
    std::optional<bool> debugBounds{std::nullopt};                  ///< Scoped debug bounding box overlay override
    std::optional<bool> inspectMode{std::nullopt};                  ///< Scoped interactive DevTools inspector override
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
    std::optional<UiVariant> variant{std::nullopt}; ///< Optional per-item accent variant override
    bool defaultOpen{true};                         ///< Initial open state for branch nodes when managed by Context
    bool selected{false};                           ///< Explicit selection state override
    bool* checked{nullptr};                         ///< Optional bound boolean checkbox state (renders interactive checkbox when non-null)
    bool indeterminate{false};                      ///< Renders checkbox in indeterminate [-] tri-state
    bool forceBranch{false};                        ///< Forces rendering the expand/collapse chevron even if no child TreeItems are emitted
    std::optional<float> controlWidth{std::nullopt};///< Optional explicit width in points for embedded trailing control slot
    StateColor background{};                        ///< Optional custom row background override
    Border border{};                                ///< Optional custom row border override
    bool disabled{false};                           ///< Disable this item and its children
    const UiTheme* theme{nullptr};                  ///< Optional scoped theme override
    std::optional<float> scale{std::nullopt};       ///< Optional local UI scale factor override
};

/// Persistent per-TreeItem state stored in Context.
struct TreeItemState {
    bool open{true};
    bool hasChildItems{false};
    uint32_t seenBulkToggleGen{0};
    Rect chevronRect{};
    Rect checkboxRect{};
    Rect controlsRect{};
};

/// Entry recorded for each visible TreeItem during a frame to power keyboard navigation on the next frame.
struct VisibleTreeItemEntry {
    Id idOuter{Id::Null()};
    Id idRow{Id::Null()};
    Id parentIdOuter{Id::Null()};
    int index{0};
    uint16_t depth{0};
    bool isBranch{false};
    bool isOpen{false};
    bool defaultOpen{true};
    bool* externalOpen{nullptr};
    bool* externalChecked{nullptr};
};

/// Persistent per-TreeView state stored in Context.
struct TreeViewState {
    static constexpr size_t kMaxColumns = 16;

    std::string filterText{};
    Id selectedId{Id::Null()};
    int selectedIndex{-1};
    uint32_t bulkToggleGen{0};
    bool bulkToggleOpen{true};
    std::vector<VisibleTreeItemEntry> prevVisibleItems{};

    uint16_t userColCount{0};
    std::array<float, kMaxColumns> userColWidths{};
    std::array<bool, kMaxColumns> hasUserColWidth{};
    int resizingCol{-1};
    float resizeStartX{0.0f};
    float resizeStartWidth{0.0f};
    double lastDividerClickTime{-10.0};
    int lastDividerCol{-1};
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
        : ContainerScope(std::move(other)), m_payload(other.m_payload) {
        other.m_payload = nullptr;
    }

    TreeViewScope& operator=(TreeViewScope&& other) noexcept {
        if (this != &other) {
            Finalize();
            ContainerScope::operator=(std::move(other));
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
    [[nodiscard]] std::string_view GetFilterText() const noexcept;

    /// Returns the number of visible TreeItems emitted so far in this TreeView during the current frame.
    [[nodiscard]] int VisibleItemCount() const noexcept;

    /// Returns the 0-based traversal index of the currently selected item (-1 if none).
    [[nodiscard]] int GetSelectedIndex() const noexcept;

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
        : ContainerScope(std::move(other))
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
            ContainerScope::operator=(std::move(other));
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
    /// Any number of widgets emitted inside `if (auto cell = item.Cell()) { ... }` are grouped into that single column.
    [[nodiscard]] ContainerScope Cell(
        const TableCellOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Emits a text cell in the next column (`Col 1`, `Col 2`, ...) on this TreeItem row.
    void CellText(
        UiText text,
        const TableCellOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    /// Emits a rounded pill badge in the next column (`Col 1`, `Col 2`, ...) on this TreeItem row.
    void CellBadge(
        UiText text,
        UiVariant variant = UiVariant::Default,
        const TableCellOptions& opts = {},
        std::source_location loc = std::source_location::current()
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
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] static TreeItemScope OpenItem(
        Context& ctx,
        bool* externalOpen,
        const TreeItemOptions& opts = {},
        std::source_location loc = std::source_location::current()
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
