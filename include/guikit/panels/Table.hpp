#pragma once

#include <cstdint>
#include <optional>
#include <source_location>
#include <span>
#include <string_view>
#include <vector>
#include "guikit/core/ContainerScope.hpp"
#include "guikit/core/Id.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Tokens.hpp"
#include "guikit/core/Types.hpp"

namespace guikit {

class Context;
class ICanvas;
struct Node;
struct TableData;

/// Sort direction for a sortable table column.
enum class TableSortDirection : uint8_t {
    None = 0,
    Ascending,
    Descending
};

/// Specification for a single column in a Table / DataGrid.
struct TableColumn {
    UiText title{};                         ///< Header caption text
    Icon icon{};                            ///< Optional leading icon in column header
    float width{0.0f};                      ///< Initial fixed width in points (0.0f = proportional flex column)
    float flex{1.0f};                       ///< Proportional flex weight when width <= 0.0f
    float minWidth{52.0f};                  ///< Minimum column width in points (enforced during resize and layout)
    float maxWidth{0.0f};                   ///< Optional maximum column width in points (0.0f = unlimited)
    Align align{Align::Start};              ///< Horizontal alignment for header and cells in this column
    bool sortable{false};                   ///< Whether clicking the column header cycles sort direction
    bool resizable{true};                   ///< Whether the right border of this column can be dragged to resize
};

/// Per-cell alignment override when using `table.Cell(...)`.
struct TableCellOptions {
    std::optional<Align> align{std::nullopt}; ///< Override horizontal alignment (nullopt = inherit from TableColumn)
};

/// Per-row visual & interaction options.
struct TableRowOptions {
    std::optional<UiVariant> tint{std::nullopt}; ///< Optional subtle row background tint (Success/Warning/Danger/Info/Primary)
    bool disabled{false};                        ///< Suppresses row selection and dims cell contents
};

/// Configuration options for the Table / DataGrid container.
struct TableOptions {
    UiText tooltip{};                               ///< Optional tooltip when hovering header

    // --- Virtualization & Row Metrics ---
    size_t totalRows{0};                            ///< Total row count (0 = auto-count; >0 enables O(1) virtualized scrolling)
    std::optional<float> rowHeight{std::nullopt};   ///< Row height in points (nullopt = derived from UiSize)
    std::optional<float> headerHeight{std::nullopt};///< Sticky header height in points (nullopt = derived from UiSize)

    // --- Selection & Sorting Bindings ---
    int* selectedRow{nullptr};                      ///< Optional bound index of selected row (-1 = none)
    int* sortColumn{nullptr};                       ///< Optional bound index of sorted column (-1 = none)
    TableSortDirection* sortDirection{nullptr};     ///< Optional bound sort direction (Ascending / Descending)

    // --- Appearance & Features ---
    bool stickyHeader{true};                        ///< Pin column header bar at top of viewport
    bool striped{true};                             ///< Alternating zebra row background shading
    bool gridLines{true};                           ///< Draw vertical column & horizontal row grid lines
    StateColor background{};
    Border border{.width = 1.0f};
    bool hoverableRows{true};                       ///< Highlight row under mouse cursor
    bool resizableColumns{true};                    ///< Allow dragging column dividers in header to resize
    bool showRowNumbers{false};                     ///< Prepend an automatic '#' row index column on the left
    bool showFooter{false};                         ///< Show bottom summary footer bar (visible row range, total rows, footerText)
    UiText footerText{};                            ///< Optional right-aligned summary text in footer bar

    // --- Sizing & Theme ---
    VisibilityPolicy scrollbar{VisibilityPolicy::Auto};
    std::optional<float> width{std::nullopt};
    std::optional<float> height{std::nullopt};      ///< Explicit height in points (nullopt = Stretch in flex parent or fit rows)
    StretchMode stretch{StretchMode::Fill};
    bool disabled{false};
    const UiTheme* theme{nullptr};
    std::optional<float> scale{std::nullopt};
    std::optional<bool> debugBounds{std::nullopt};
    std::optional<bool> inspectMode{std::nullopt};
};

class TableScope;

/// RAII scope representing a single visible row inside a Table.
class TableRowScope {
public:
    TableRowScope() noexcept = default;
    TableRowScope(TableScope* table, size_t rowIndex, bool visible, bool selected, bool hovered, bool clicked, bool doubleClicked) noexcept
        : m_table(table)
        , m_rowIndex(rowIndex)
        , m_visible(visible)
        , m_selected(selected)
        , m_hovered(hovered)
        , m_clicked(clicked)
        , m_doubleClicked(doubleClicked) {}

    ~TableRowScope() noexcept;

    TableRowScope(const TableRowScope&) = delete;
    TableRowScope& operator=(const TableRowScope&) = delete;

    TableRowScope(TableRowScope&& other) noexcept
        : m_table(other.m_table)
        , m_rowIndex(other.m_rowIndex)
        , m_visible(other.m_visible)
        , m_selected(other.m_selected)
        , m_hovered(other.m_hovered)
        , m_clicked(other.m_clicked)
        , m_doubleClicked(other.m_doubleClicked) {
        other.m_table = nullptr;
        other.m_visible = false;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return m_visible; }
    [[nodiscard]] size_t RowIndex() const noexcept { return m_rowIndex; }
    [[nodiscard]] bool Selected() const noexcept { return m_selected; }
    [[nodiscard]] bool Hovered() const noexcept { return m_hovered; }
    [[nodiscard]] bool Clicked() const noexcept { return m_clicked; }
    [[nodiscard]] bool DoubleClicked() const noexcept { return m_doubleClicked; }

private:
    TableScope* m_table{nullptr};
    size_t m_rowIndex{0};
    bool m_visible{false};
    bool m_selected{false};
    bool m_hovered{false};
    bool m_clicked{false};
    bool m_doubleClicked{false};
};

/// RAII scope returned by `ui.Table(...)` providing row virtualization and cell emission methods.
class TableScope {
public:
    TableScope() noexcept = default;
    TableScope(Context* ctx, Node* tableNode, TableData* data) noexcept;
    ~TableScope() noexcept;

    TableScope(const TableScope&) = delete;
    TableScope& operator=(const TableScope&) = delete;

    TableScope(TableScope&& other) noexcept
        : m_ctx(other.m_ctx)
        , m_node(other.m_node)
        , m_data(other.m_data)
        , m_nextAutoRow(other.m_nextAutoRow) {
        other.m_ctx = nullptr;
        other.m_node = nullptr;
        other.m_data = nullptr;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return m_node != nullptr; }
    [[nodiscard]] Node* GetNode() const noexcept { return m_node; }

    /// First visible row index in the virtualized scroll viewport (inclusive).
    [[nodiscard]] size_t FirstVisibleRow() const noexcept;

    /// One-past-last visible row index in the virtualized scroll viewport (exclusive).
    [[nodiscard]] size_t EndVisibleRow() const noexcept;

    /// Returns true on the frame the user clicked a sortable column header.
    [[nodiscard]] bool SortChanged() const noexcept;

    /// Active sorted column index (-1 if none).
    [[nodiscard]] int SortedColumn() const noexcept;

    /// Active sort direction (None / Ascending / Descending).
    [[nodiscard]] TableSortDirection SortDirection() const noexcept;

    /// Returns true on the frame the selected row changed via click or keyboard navigation.
    [[nodiscard]] bool SelectionChanged() const noexcept;

    /// Currently selected row index (-1 if none).
    [[nodiscard]] int SelectedRow() const noexcept;

    /// Row index currently hovered by the mouse cursor (-1 if none).
    [[nodiscard]] int HoveredRow() const noexcept;

    /// Row index clicked this frame (-1 if none).
    [[nodiscard]] int ClickedRow() const noexcept;

    /// Row index double-clicked this frame (-1 if none).
    [[nodiscard]] int DoubleClickedRow() const noexcept;

    /// Begins a table row at explicit `rowIndex`. Returns false if row is outside the visible virtualized range.
    [[nodiscard]] TableRowScope Row(size_t rowIndex, const TableRowOptions& opts = {});

    /// Begins the next sequential row (for small static tables without explicit row index loop).
    [[nodiscard]] TableRowScope Row(const TableRowOptions& opts = {}) {
        return Row(m_nextAutoRow++, opts);
    }

    /// Opens an explicit multi-widget container cell (`NodeType::Cell`) in the current column of the active row.
    /// Note: Single widgets (`ui.Text`, `ui.Element`, `ui.Checkbox`, `ui.ProgressBar`, `ui.Button`, etc.)
    /// placed directly inside `if (auto row = table.Row(r))` automatically occupy sequential columns without needing `table.Cell()`.
    [[nodiscard]] ContainerScope Cell(const TableCellOptions& opts = {});

    /// Programmatically scrolls the table viewport so `rowIndex` is visible.
    void ScrollToRow(size_t rowIndex);

private:
    friend class TableRowScope;
    void EndActiveRow() noexcept;

    Context* m_ctx{nullptr};
    Node* m_node{nullptr};
    TableData* m_data{nullptr};
    size_t m_nextAutoRow{0};
};

/// High-performance virtualized Table / DataGrid container with sticky headers, sorting, and column resizing.
class Table {
public:
    [[nodiscard]] static TableScope Open(
        Context& ctx,
        std::span<const TableColumn> columns,
        const TableOptions& opts = {},
        std::source_location loc = std::source_location::current()
    );

    [[nodiscard]] static Vec2 Measure(
        const Node* node,
        float availableWidth,
        ICanvas* canvas,
        const UiTheme& theme
    );

    static void Arrange(
        Node* node,
        const Rect& availableRect,
        const UiTheme& theme
    );

    [[nodiscard]] static Rect GetBodyClipRect(
        const Node* node,
        const UiTheme& theme
    );

    static void Update(
        Context& ctx,
        Node* node
    );

    static void RenderBody(
        const Node* node,
        ICanvas& canvas,
        const UiTheme& theme
    );

    static void RenderChromeAndScrollbars(
        const Node* node,
        ICanvas& canvas,
        const UiTheme& theme
    );
};

} // namespace guikit
