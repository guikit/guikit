#include "guikit/panels/Table.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Control.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"
#include "guikit/panels/ScrollArea.hpp"

namespace guikit {

namespace {

constexpr size_t kMaxTableColumns = 16;

struct TableFastCell {
    size_t rowIndex{0};
    uint16_t colIndex{0};
    bool isBadge{false};
    UiText text{};
    TableCellOptions opts{};
};

struct TableRowRecord {
    size_t rowIndex{0};
    TableRowOptions opts{};
};

struct TableState {
    Vec2 scrollOffset{0.0f, 0.0f};
    bool draggingScrollbarV{false};
    bool draggingScrollbarH{false};
    float dragStartOffsetV{0.0f};
    float dragStartOffsetH{0.0f};
    Vec2 dragStartMouseV{0.0f, 0.0f};
    Vec2 dragStartMouseH{0.0f, 0.0f};

    uint16_t userColCount{0};
    std::array<float, kMaxTableColumns> userColWidths{};
    std::array<bool, kMaxTableColumns> hasUserColWidth{};
    int resizingCol{-1};
    float resizeStartX{0.0f};
    float resizeStartWidth{0.0f};

    int selectedRow{-1};
    int sortColumn{-1};
    TableSortDirection sortDirection{TableSortDirection::None};

    double lastClickTime{-10.0};
    int lastClickRow{-1};
    double lastDividerClickTime{-10.0};
    int lastDividerCol{-1};

    size_t prevTotalRows{0};
    float lastViewportHeight{240.0f};
    Rect lastArrangedBounds{};

    // Persistent frame buffers (cleared each frame, zero heap allocation after warmup)
    std::vector<TableRowRecord> visibleRows{};
    std::vector<TableFastCell> fastCells{};
};

float ResolveDefaultRowHeight(UiSize size, const UiTheme& theme) noexcept {
    switch (size) {
        case UiSize::Xs: return theme.Scale(24.0f);
        case UiSize::Sm: return theme.Scale(28.0f);
        case UiSize::Md: return theme.Scale(32.0f);
        case UiSize::Lg: return theme.Scale(38.0f);
        case UiSize::Xl: return theme.Scale(44.0f);
        default:         return theme.Scale(32.0f);
    }
}

float ResolveDefaultHeaderHeight(UiSize size, const UiTheme& theme) noexcept {
    switch (size) {
        case UiSize::Xs: return theme.Scale(26.0f);
        case UiSize::Sm: return theme.Scale(30.0f);
        case UiSize::Md: return theme.Scale(34.0f);
        case UiSize::Lg: return theme.Scale(40.0f);
        case UiSize::Xl: return theme.Scale(46.0f);
        default:         return theme.Scale(34.0f);
    }
}

float ResolveDefaultFooterHeight(UiSize size, const UiTheme& theme) noexcept {
    switch (size) {
        case UiSize::Xs: return theme.Scale(22.0f);
        case UiSize::Sm: return theme.Scale(24.0f);
        case UiSize::Md: return theme.Scale(26.0f);
        case UiSize::Lg: return theme.Scale(30.0f);
        case UiSize::Xl: return theme.Scale(34.0f);
        default:         return theme.Scale(26.0f);
    }
}

float ResolveTableFontSize(UiSize size, const UiTheme& theme, bool isHeader) noexcept {
    if (isHeader) {
        return theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, size);
    }
    return theme.ResolveFontSize(UiText{}, TextRole::BodyMedium, size);
}

float AlignToFactor(Align align) noexcept {
    switch (align) {
        case Align::Center: return 0.5f;
        case Align::End:    return 1.0f;
        default:            return 0.0f;
    }
}

} // namespace

struct TableData {
    TableOptions opts{};
    TableState* state{nullptr};

    uint16_t columnCount{0};
    std::array<TableColumn, kMaxTableColumns> columns{};
    std::array<float, kMaxTableColumns> resolvedColX{};
    std::array<float, kMaxTableColumns> resolvedColW{};
    float rowNumberColW{0.0f};
    float totalColumnsW{0.0f};

    float rowHeight{32.0f};
    float headerHeight{34.0f};
    float footerHeight{0.0f};

    size_t firstVisibleRow{0};
    size_t endVisibleRow{0};
    size_t emittedRowCount{0};

    bool hasActiveRow{false};
    size_t activeRowIndex{0};
    uint16_t activeColCursor{0};

    int hoveredHeaderCol{-1};
    int hoveredDividerCol{-1};
    int hoveredRow{-1};
    int clickedRow{-1};
    int doubleClickedRow{-1};
    bool sortChanged{false};
    bool selectionChanged{false};

    Rect frameRect{};
    Rect headerRect{};
    Rect bodyViewportRect{};
    Rect footerRect{};
};

namespace {

void ResolveColumnWidths(TableData& data, float viewportW, const UiTheme& theme) noexcept {
    const float scale = (theme.scale > 0.0f) ? theme.scale : 1.0f;
    data.rowNumberColW = data.opts.showRowNumbers ? std::round(44.0f * scale) : 0.0f;
    const float availForCols = std::max(0.0f, viewportW - data.rowNumberColW);

    float fixedSum = 0.0f;
    float flexSum = 0.0f;

    for (uint16_t i = 0; i < data.columnCount; ++i) {
        const auto& col = data.columns[i];
        const float minW = std::max(28.0f * scale, col.minWidth * scale);
        const float maxW = (col.maxWidth > 0.0f) ? (col.maxWidth * scale) : 100000.0f;

        if (data.state && data.state->hasUserColWidth[i]) {
            const float w = std::clamp(data.state->userColWidths[i] * scale, minW, maxW);
            data.resolvedColW[i] = w;
            fixedSum += w;
        } else if (col.width > 0.0f) {
            const float w = std::clamp(col.width * scale, minW, maxW);
            data.resolvedColW[i] = w;
            fixedSum += w;
        } else {
            data.resolvedColW[i] = 0.0f;
            flexSum += std::max(0.1f, col.flex);
        }
    }

    const float remainingW = std::max(0.0f, availForCols - fixedSum);
    float curX = data.rowNumberColW;

    for (uint16_t i = 0; i < data.columnCount; ++i) {
        const auto& col = data.columns[i];
        const float minW = std::max(28.0f * scale, col.minWidth * scale);
        const float maxW = (col.maxWidth > 0.0f) ? (col.maxWidth * scale) : 100000.0f;

        if (data.resolvedColW[i] <= 0.0f) {
            const float weight = std::max(0.1f, col.flex);
            const float flexShare = (flexSum > 0.0f) ? (remainingW * (weight / flexSum)) : minW;
            data.resolvedColW[i] = std::clamp(std::round(flexShare), minW, maxW);
        }

        // Stretch last column if auto-flex columns slightly undershoot viewport due to rounding
        if (i + 1 == data.columnCount && flexSum > 0.0f &&
            !(data.state && data.state->hasUserColWidth[i]) &&
            curX + data.resolvedColW[i] < viewportW) {
            data.resolvedColW[i] = std::min(maxW, viewportW - curX);
        }

        data.resolvedColX[i] = curX;
        curX += data.resolvedColW[i];
    }

    data.totalColumnsW = curX;
}

void ComputeTableRegions(
    const Node* node,
    TableData& data,
    const Rect& outerBounds,
    const UiTheme& theme
) noexcept {
    data.frameRect = outerBounds;
    data.headerHeight = data.opts.stickyHeader
        ? (data.opts.headerHeight.has_value()
            ? theme.Scale(*data.opts.headerHeight)
            : ResolveDefaultHeaderHeight(node->uiSize, theme))
        : 0.0f;
    data.rowHeight = data.opts.rowHeight.has_value()
        ? std::max(18.0f, theme.Scale(*data.opts.rowHeight))
        : ResolveDefaultRowHeight(node->uiSize, theme);
    data.footerHeight = (data.opts.showFooter || !data.opts.footerText.empty())
        ? ResolveDefaultFooterHeight(node->uiSize, theme)
        : 0.0f;

    const float headerH = std::min(data.headerHeight, data.frameRect.h);
    const float footerH = std::min(data.footerHeight, std::max(0.0f, data.frameRect.h - headerH));
    const float bodyH = std::max(0.0f, data.frameRect.h - headerH - footerH);

    data.headerRect = Rect(data.frameRect.x, data.frameRect.y, data.frameRect.w, headerH);
    data.bodyViewportRect = Rect(data.frameRect.x, data.frameRect.y + headerH, data.frameRect.w, bodyH);
    data.footerRect = Rect(data.frameRect.x, data.frameRect.Bottom() - footerH, data.frameRect.w, footerH);
}

void UpdateVisibleRowRange(TableData& data) noexcept {
    if (!data.state) return;
    const float viewH = std::max(data.rowHeight, data.state->lastViewportHeight);
    const size_t rawFirst = (data.rowHeight > 0.0f)
        ? static_cast<size_t>(std::max(0.0f, std::floor(data.state->scrollOffset.y / data.rowHeight)))
        : 0;
    const size_t visibleCount = (data.rowHeight > 0.0f)
        ? (static_cast<size_t>(std::ceil(viewH / data.rowHeight)) + 2)
        : 32;

    data.firstVisibleRow = (rawFirst > 0) ? (rawFirst - 1) : 0;
    if (data.opts.totalRows > 0) {
        data.firstVisibleRow = std::min(data.firstVisibleRow, data.opts.totalRows);
        data.endVisibleRow = std::min(data.opts.totalRows, data.firstVisibleRow + visibleCount);
    } else {
        data.endVisibleRow = data.firstVisibleRow + visibleCount;
    }
}

} // namespace

// ============================================================================
// TableRowScope & TableScope
// ============================================================================

TableRowScope::~TableRowScope() noexcept {
    if (m_table && m_visible) {
        m_table->EndActiveRow();
    }
}

TableScope::TableScope(Context* ctx, Node* tableNode, TableData* data) noexcept
    : m_ctx(ctx)
    , m_node(tableNode)
    , m_data(data) {}

TableScope::~TableScope() noexcept {
    if (!m_ctx || !m_node) return;

    if (m_data) {
        if (m_data->hasActiveRow) {
            EndActiveRow();
        }
        const size_t totalRows = (m_data->opts.totalRows > 0)
            ? m_data->opts.totalRows
            : m_data->emittedRowCount;
        if (m_data->state) {
            m_data->state->prevTotalRows = totalRows;
        }
        m_node->scrollContentSize = Vec2(
            m_data->totalColumnsW,
            static_cast<float>(totalRows) * m_data->rowHeight
        );

        // Compute comprehensive visualHash from scroll offsets, column geometry,
        // visible cells, and embedded child widgets so dirty-tracking always
        // repaints the entire Table coherently.
        uint64_t h = m_node->visualHash;
        if (m_data->state) {
            h = detail::HashArgs(
                h,
                m_data->state->scrollOffset.x,
                m_data->state->scrollOffset.y,
                m_data->state->resizingCol,
                m_data->state->draggingScrollbarV,
                m_data->state->draggingScrollbarH,
                m_node->scrollbarHovered,
                m_node->scrollbarActive,
                m_node->scrollbarHoveredX,
                m_node->scrollbarActiveX,
                m_data->state->selectedRow,
                m_data->state->sortColumn,
                static_cast<uint8_t>(m_data->state->sortDirection),
                m_data->hoveredRow,
                m_data->hoveredHeaderCol,
                m_data->hoveredDividerCol,
                m_data->firstVisibleRow,
                m_data->endVisibleRow,
                m_data->state->visibleRows.size(),
                m_data->state->fastCells.size()
            );
            for (uint16_t i = 0; i < m_data->columnCount; ++i) {
                h = detail::HashArgs(h, m_data->resolvedColX[i], m_data->resolvedColW[i]);
            }
            for (const auto& rowRec : m_data->state->visibleRows) {
                h = detail::HashArgs(
                    h,
                    rowRec.rowIndex,
                    rowRec.opts.disabled,
                    rowRec.opts.tint.has_value(),
                    static_cast<uint8_t>(rowRec.opts.tint.value_or(UiVariant::Default))
                );
            }
            for (const auto& cell : m_data->state->fastCells) {
                h = detail::HashArgs(
                    h,
                    cell.rowIndex,
                    cell.colIndex,
                    cell.isBadge,
                    cell.text.text,
                    static_cast<uint8_t>(cell.opts.variant)
                );
            }
            for (const Node* c = m_node->firstChild; c != nullptr; c = c->nextSibling) {
                h = detail::HashArgs(h, c->id.value, c->colSpan, c->childIndex);
                for (const Node* gc = c->firstChild; gc != nullptr; gc = gc->nextSibling) {
                    h = detail::HashArgs(
                        h,
                        gc->id.value,
                        gc->visualHash,
                        gc->hovered,
                        gc->pressed,
                        gc->checked
                    );
                }
            }
        }
        m_node->visualHash = h;
    }

    m_ctx->PopContainer(m_node);
}

size_t TableScope::FirstVisibleRow() const noexcept {
    return m_data ? m_data->firstVisibleRow : 0;
}

size_t TableScope::EndVisibleRow() const noexcept {
    return m_data ? m_data->endVisibleRow : 0;
}

bool TableScope::SortChanged() const noexcept {
    return m_data ? m_data->sortChanged : false;
}

int TableScope::SortedColumn() const noexcept {
    return (m_data && m_data->state) ? m_data->state->sortColumn : -1;
}

TableSortDirection TableScope::SortDirection() const noexcept {
    return (m_data && m_data->state) ? m_data->state->sortDirection : TableSortDirection::None;
}

bool TableScope::SelectionChanged() const noexcept {
    return m_data ? m_data->selectionChanged : false;
}

int TableScope::SelectedRow() const noexcept {
    return (m_data && m_data->state) ? m_data->state->selectedRow : -1;
}

int TableScope::HoveredRow() const noexcept {
    return m_data ? m_data->hoveredRow : -1;
}

int TableScope::ClickedRow() const noexcept {
    return m_data ? m_data->clickedRow : -1;
}

int TableScope::DoubleClickedRow() const noexcept {
    return m_data ? m_data->doubleClickedRow : -1;
}

TableRowScope TableScope::Row(size_t rowIndex, const TableRowOptions& opts) {
    if (!m_ctx || !m_data || !m_data->state) {
        return TableRowScope{};
    }
    if (m_data->hasActiveRow) {
        EndActiveRow();
    }

    m_data->emittedRowCount = std::max(m_data->emittedRowCount, rowIndex + 1);

    // Virtualization culling: skip rows outside [firstVisibleRow, endVisibleRow)
    if (rowIndex < m_data->firstVisibleRow || rowIndex >= m_data->endVisibleRow) {
        return TableRowScope{};
    }

    m_data->hasActiveRow = true;
    m_data->activeRowIndex = rowIndex;
    m_data->activeColCursor = 0;

    m_data->state->visibleRows.push_back(TableRowRecord{
        .rowIndex = rowIndex,
        .opts = opts
    });

    const int iRow = static_cast<int>(rowIndex);
    const bool selected = (m_data->state->selectedRow == iRow);
    const bool hovered = (m_data->hoveredRow == iRow) && !opts.disabled;
    const bool clicked = (m_data->clickedRow == iRow) && !opts.disabled;
    const bool dblClicked = (m_data->doubleClickedRow == iRow) && !opts.disabled;

    return TableRowScope(this, rowIndex, true, selected, hovered, clicked, dblClicked);
}

void TableScope::EndActiveRow() noexcept {
    if (!m_data || !m_data->hasActiveRow) return;
    m_data->hasActiveRow = false;
}

void TableScope::CellText(const UiText& text, const TableCellOptions& opts) {
    if (!m_ctx || !m_data || !m_data->state || !m_data->hasActiveRow) return;
    const uint16_t colIdx = m_data->activeColCursor++;
    if (colIdx >= m_data->columnCount) return;

    m_data->state->fastCells.push_back(TableFastCell{
        .rowIndex = m_data->activeRowIndex,
        .colIndex = colIdx,
        .isBadge = false,
        .text = m_ctx->AllocText(text),
        .opts = opts
    });
}

void TableScope::CellBadge(std::string_view text, UiVariant variant, const TableCellOptions& opts) {
    if (!m_ctx || !m_data || !m_data->state || !m_data->hasActiveRow) return;
    const uint16_t colIdx = m_data->activeColCursor++;
    if (colIdx >= m_data->columnCount) return;

    TableCellOptions badgeOpts = opts;
    badgeOpts.variant = variant;

    m_data->state->fastCells.push_back(TableFastCell{
        .rowIndex = m_data->activeRowIndex,
        .colIndex = colIdx,
        .isBadge = true,
        .text = m_ctx->AllocText(UiText(text)),
        .opts = badgeOpts
    });
}

ContainerScope TableScope::Cell(const TableCellOptions& opts) {
    if (!m_ctx || !m_node || !m_data || !m_data->hasActiveRow) {
        return ContainerScope(nullptr, nullptr);
    }
    const uint16_t colIdx = m_data->activeColCursor++;
    if (colIdx >= m_data->columnCount) {
        return ContainerScope(nullptr, nullptr);
    }

    const Id cellId(detail::HashArgs(m_node->id.value, m_data->activeRowIndex, colIdx));
    Node* cellNode = m_ctx->CreateNode(NodeType::Cell, cellId);
    cellNode->direction = FlexDirection::Row;
    cellNode->align = Alignment::Center;
    const Align effectiveAlign = opts.align.value_or(m_data->columns[colIdx].align);
    cellNode->justify = (effectiveAlign == Align::Center)
        ? Justify::Center
        : ((effectiveAlign == Align::End) ? Justify::End : Justify::Start);
    cellNode->padding = Padding::Symmetric(m_ctx->GetTheme().Scale(6.0f), m_ctx->GetTheme().Scale(2.0f));
    cellNode->gap = m_ctx->GetTheme().Scale(4.0f);
    cellNode->clipChildren = true;
    cellNode->colSpan = static_cast<uint8_t>(colIdx);
    cellNode->childIndex = static_cast<uint32_t>(m_data->activeRowIndex);

    m_ctx->PushScale(m_ctx->GetCurrentScale());
    m_ctx->PushSize(m_ctx->GetCurrentSize());
    m_ctx->PushDisabled(m_ctx->IsDisabled());
    m_ctx->PushContainer(cellNode);
    return ContainerScope(m_ctx, cellNode);
}

void TableScope::ScrollToRow(size_t rowIndex) {
    if (!m_data || !m_data->state) return;
    const float rowTop = static_cast<float>(rowIndex) * m_data->rowHeight;
    const float rowBottom = rowTop + m_data->rowHeight;
    const float viewH = std::max(m_data->rowHeight, m_data->state->lastViewportHeight);
    if (rowTop < m_data->state->scrollOffset.y) {
        m_data->state->scrollOffset.y = rowTop;
    } else if (rowBottom > m_data->state->scrollOffset.y + viewH) {
        m_data->state->scrollOffset.y = std::max(0.0f, rowBottom - viewH);
    }
    const size_t knownTotalRows = (m_data->opts.totalRows > 0)
        ? m_data->opts.totalRows
        : m_data->state->prevTotalRows;
    if (knownTotalRows > 0) {
        const float maxScrollY = std::max(0.0f, static_cast<float>(knownTotalRows) * m_data->rowHeight - viewH);
        m_data->state->scrollOffset.y = std::clamp(m_data->state->scrollOffset.y, 0.0f, maxScrollY);
    }
    UpdateVisibleRowRange(*m_data);
    if (m_node) {
        m_node->scrollOffset = m_data->state->scrollOffset;
    }
    if (m_ctx) {
        if (m_data->frameRect.w > 0.0f && m_data->frameRect.h > 0.0f) {
            m_ctx->InvalidateRect(m_data->frameRect);
        } else {
            m_ctx->InvalidateAll();
        }
        m_ctx->RequestRepaint(2);
    }
}

// ============================================================================
// Table::Open
// ============================================================================

TableScope Table::Open(
    Context& ctx,
    std::span<const TableColumn> columns,
    const TableOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    TableOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    UiTheme* pushedTheme = ctx.PushScopedTheme(optsCopy.theme, optsCopy.scale);
    const UiTheme& theme = ctx.GetTheme();

    Node* node = ctx.CreateNode(NodeType::Table, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = optsCopy.debugBounds;
    node->inspectModeOverride = optsCopy.inspectMode;
    node->direction = FlexDirection::Column;
    node->clipChildren = true;
    node->scrollOrientation = Orientation::Both;
    node->scrollbarPolicy = optsCopy.scrollbar;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    if (optsCopy.width.has_value() && optsCopy.stretch != StretchMode::Fill) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = theme.Scale(*optsCopy.width);
    } else {
        node->widthMode = (optsCopy.stretch == StretchMode::Fill) ? SizeMode::Stretch : SizeMode::Fit;
    }

    if (optsCopy.height.has_value()) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(*optsCopy.height);
    } else {
        node->heightMode = SizeMode::Stretch;
        node->flexGrow = 1.0f;
    }

    auto& state = ctx.GetOrCreateState<TableState>(id);
    state.visibleRows.clear();
    state.fastCells.clear();

    const uint16_t colCount = static_cast<uint16_t>(std::min(columns.size(), kMaxTableColumns));
    if (state.userColCount != colCount) {
        state.userColCount = colCount;
        state.hasUserColWidth.fill(false);
        state.userColWidths.fill(0.0f);
        state.resizingCol = -1;
    }

    // Sync external bindings into persistent state
    if (optsCopy.selectedRow) {
        state.selectedRow = *optsCopy.selectedRow;
    }
    if (optsCopy.sortColumn) {
        state.sortColumn = *optsCopy.sortColumn;
    }
    if (optsCopy.sortDirection) {
        state.sortDirection = *optsCopy.sortDirection;
    }

    auto* data = ctx.AllocPayload<TableData>();
    data->opts = optsCopy;
    data->opts.tooltip = ctx.AllocText(optsCopy.tooltip);
    data->opts.footerText = ctx.AllocText(optsCopy.footerText);
    data->state = &state;

    data->columnCount = colCount;
    for (uint16_t i = 0; i < data->columnCount; ++i) {
        data->columns[i] = columns[i];
        data->columns[i].title = ctx.AllocText(columns[i].title);
    }

    // Compute regions from previous frame bounds (or parent bounds on first frame)
    // so header/scrollbar hit-testing and row virtualization run BEFORE emitting rows!
    Rect prevBounds = (state.lastArrangedBounds.w > 0.0f && state.lastArrangedBounds.h > 0.0f)
        ? state.lastArrangedBounds
        : ctx.GetPrevFrameBounds(id);
    if (prevBounds.w <= 0.0f || prevBounds.h <= 0.0f) {
        prevBounds = ctx.GetParentPrevFrameBounds();
    }
    const Rect estBounds = (prevBounds.w > 0.0f && prevBounds.h > 0.0f)
        ? prevBounds
        : Rect(0.0f, 0.0f, optsCopy.width.value_or(640.0f), optsCopy.height.value_or(320.0f));

    ComputeTableRegions(node, *data, estBounds, theme);
    ResolveColumnWidths(*data, data->frameRect.w, theme);

    if (data->bodyViewportRect.h > 0.0f) {
        state.lastViewportHeight = data->bodyViewportRect.h;
    }

    const size_t knownTotalRows = (optsCopy.totalRows > 0) ? optsCopy.totalRows : state.prevTotalRows;
    const float estContentH = static_cast<float>(knownTotalRows) * data->rowHeight;
    const float maxScrollY = std::max(0.0f, estContentH - state.lastViewportHeight);
    const float maxScrollX = std::max(0.0f, data->totalColumnsW - data->bodyViewportRect.w);

    // Handle scrollbars, mouse wheel, column resize, header sort, and row selection BEFORE virtualization!
    if (prevBounds.w > 0.0f && prevBounds.h > 0.0f && !activeDisabled) {
        const Vec2 mousePos = ctx.GetMousePos();
        const bool mouseClicked = ctx.IsMouseClicked();
        const bool mouseDown = ctx.IsMouseDown();
        const Vec2 prevScroll = state.scrollOffset;

        // 0. Interactive vertical & horizontal scrollbars (evaluated first so virtualization uses updated scrollOffset!)
        const ScrollbarMetrics& sm = theme.GetScrollbar(activeSize);
        const bool showV = (node->scrollbarPolicy == VisibilityPolicy::Always) ||
            (node->scrollbarPolicy == VisibilityPolicy::Auto && estContentH > data->bodyViewportRect.h + 0.5f);
        const bool showH = (node->scrollbarPolicy == VisibilityPolicy::Always) ||
            (node->scrollbarPolicy == VisibilityPolicy::Auto && data->totalColumnsW > data->bodyViewportRect.w + 0.5f);
        const float cornerReserve = (showV && showH) ? (sm.trackWidth + sm.margin) : 0.0f;

        bool hovV = false, actV = false;
        const Id vScrollId(id.value ^ 0x517CC1B727220A95ULL);
        if (state.resizingCol < 0) {
            ScrollArea::UpdateAxisScrollbar(
                ctx,
                vScrollId,
                data->bodyViewportRect,
                estContentH,
                state.scrollOffset.y,
                state.draggingScrollbarV,
                state.dragStartOffsetV,
                state.dragStartMouseV,
                hovV,
                actV,
                false,
                activeSize,
                theme,
                cornerReserve
            );
        }
        node->scrollbarHovered = hovV;
        node->scrollbarActive = actV;

        bool hovH = false, actH = false;
        const Id hScrollId(id.value ^ 0x9E3779B97F4A7C15ULL);
        if (state.resizingCol < 0) {
            ScrollArea::UpdateAxisScrollbar(
                ctx,
                hScrollId,
                data->bodyViewportRect,
                data->totalColumnsW,
                state.scrollOffset.x,
                state.draggingScrollbarH,
                state.dragStartOffsetH,
                state.dragStartMouseH,
                hovH,
                actH,
                true,
                activeSize,
                theme,
                cornerReserve
            );
        }
        node->scrollbarHoveredX = hovH;
        node->scrollbarActiveX = actH;

        const bool interactingScrollbar = state.draggingScrollbarV || state.draggingScrollbarH || hovV || hovH;

        // 1. Mouse wheel scrolling inside table frame
        if (data->frameRect.Contains(mousePos)) {
            const Vec2 wheel = ctx.GetMouseWheel();
            if (wheel.x != 0.0f || wheel.y != 0.0f) {
                if (ctx.GetKeyModifiers().shift && wheel.x == 0.0f) {
                    state.scrollOffset.x -= wheel.y * sm.scrollStep;
                } else {
                    state.scrollOffset.y -= wheel.y * sm.scrollStep;
                    state.scrollOffset.x -= wheel.x * sm.scrollStep;
                }
            }
        }
        state.scrollOffset.y = std::clamp(state.scrollOffset.y, 0.0f, maxScrollY);
        state.scrollOffset.x = std::clamp(state.scrollOffset.x, 0.0f, maxScrollX);

        // 2. Column resize drag or header sort click
        const float scale = (theme.scale > 0.0f) ? theme.scale : 1.0f;
        const float gripHalfW = 6.0f * scale;
        const Rect headerHitRect(
            data->headerRect.x - gripHalfW,
            data->headerRect.y,
            data->headerRect.w + gripHalfW * 2.0f,
            data->headerRect.h
        );

        if (state.resizingCol >= 0) {
            if (mouseDown && state.resizingCol < static_cast<int>(data->columnCount)) {
                const uint16_t colIdx = static_cast<uint16_t>(state.resizingCol);
                const float minW = std::max(28.0f * scale, data->columns[colIdx].minWidth * scale);
                const float maxW = (data->columns[colIdx].maxWidth > 0.0f)
                    ? (data->columns[colIdx].maxWidth * scale)
                    : 100000.0f;
                const float newW = std::clamp(state.resizeStartWidth + (mousePos.x - state.resizeStartX), minW, maxW);
                state.userColWidths[colIdx] = newW / scale;
                state.hasUserColWidth[colIdx] = true;
                ResolveColumnWidths(*data, data->frameRect.w, theme);
                const float newMaxScrollX = std::max(0.0f, data->totalColumnsW - data->bodyViewportRect.w);
                state.scrollOffset.x = std::clamp(state.scrollOffset.x, 0.0f, newMaxScrollX);
                ctx.InvalidateRect(data->frameRect);
                ctx.RequestRepaint(2);
            } else {
                state.resizingCol = -1;
                if (ctx.GetActiveId() == id) {
                    ctx.SetActiveId(Id::Null());
                }
                ctx.InvalidateRect(data->frameRect);
                ctx.RequestRepaint(2);
            }
        } else if (!interactingScrollbar && optsCopy.stickyHeader && headerHitRect.Contains(mousePos)) {
            const float localHeaderX = mousePos.x - data->headerRect.x + state.scrollOffset.x;

            // Check column divider grips first
            if (optsCopy.resizableColumns) {
                for (uint16_t i = 0; i < data->columnCount; ++i) {
                    if (!data->columns[i].resizable) continue;
                    const float divX = data->resolvedColX[i] + data->resolvedColW[i];
                    if (std::abs(localHeaderX - divX) <= gripHalfW) {
                        data->hoveredDividerCol = static_cast<int>(i);
                        if (mouseClicked) {
                            const double now = (ctx.GetCurrentTime)();
                            if (state.lastDividerCol == static_cast<int>(i) && (now - state.lastDividerClickTime) <= 0.35) {
                                state.hasUserColWidth.fill(false);
                                state.resizingCol = -1;
                                ResolveColumnWidths(*data, data->frameRect.w, theme);
                                const float newMaxScrollX = std::max(0.0f, data->totalColumnsW - data->bodyViewportRect.w);
                                state.scrollOffset.x = std::clamp(state.scrollOffset.x, 0.0f, newMaxScrollX);
                                ctx.InvalidateRect(data->frameRect);
                                ctx.RequestRepaint(2);
                            } else {
                                // Snapshot all current resolved column widths so flex columns to the left
                                // do not counter-shrink or shift the left border of column `i` during drag!
                                for (uint16_t j = 0; j < data->columnCount; ++j) {
                                    if (!state.hasUserColWidth[j]) {
                                        state.userColWidths[j] = data->resolvedColW[j] / scale;
                                        state.hasUserColWidth[j] = true;
                                    }
                                }
                                state.resizingCol = static_cast<int>(i);
                                state.resizeStartX = mousePos.x;
                                state.resizeStartWidth = data->resolvedColW[i];
                                ctx.SetActiveId(id);
                                ctx.InvalidateRect(data->frameRect);
                                ctx.RequestRepaint(2);
                            }
                            state.lastDividerClickTime = now;
                            state.lastDividerCol = static_cast<int>(i);
                        }
                        break;
                    }
                }
            }

            // If not hovering a resize divider, check column header hover & click-to-sort
            if (data->hoveredDividerCol < 0 && data->headerRect.Contains(mousePos)) {
                for (uint16_t i = 0; i < data->columnCount; ++i) {
                    const float colLeft = data->resolvedColX[i];
                    const float colRight = colLeft + data->resolvedColW[i];
                    if (localHeaderX >= colLeft && localHeaderX < colRight) {
                        data->hoveredHeaderCol = static_cast<int>(i);
                        if (mouseClicked && data->columns[i].sortable) {
                            if (state.sortColumn == static_cast<int>(i)) {
                                state.sortDirection = (state.sortDirection == TableSortDirection::Ascending)
                                    ? TableSortDirection::Descending
                                    : TableSortDirection::Ascending;
                            } else {
                                state.sortColumn = static_cast<int>(i);
                                state.sortDirection = TableSortDirection::Ascending;
                            }
                            data->sortChanged = true;
                            if (optsCopy.sortColumn) *optsCopy.sortColumn = state.sortColumn;
                            if (optsCopy.sortDirection) *optsCopy.sortDirection = state.sortDirection;
                            ctx.InvalidateRect(data->frameRect);
                            ctx.RequestRepaint(2);
                        }
                        break;
                    }
                }
            }
        }

        // 3. Body row hover & click selection (excluding scrollbar gutters)
        const float scrollbarGutterW = showV ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;
        const float scrollbarGutterH = showH ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;
        const Rect interactiveBodyRect(
            data->bodyViewportRect.x,
            data->bodyViewportRect.y,
            std::max(0.0f, data->bodyViewportRect.w - scrollbarGutterW),
            std::max(0.0f, data->bodyViewportRect.h - scrollbarGutterH)
        );

        if (state.resizingCol < 0 && !interactingScrollbar && interactiveBodyRect.Contains(mousePos)) {
            const float contentY = (mousePos.y - data->bodyViewportRect.y) + state.scrollOffset.y;
            if (contentY >= 0.0f && data->rowHeight > 0.0f) {
                const size_t hitRow = static_cast<size_t>(contentY / data->rowHeight);
                if (knownTotalRows == 0 || hitRow < knownTotalRows) {
                    data->hoveredRow = static_cast<int>(hitRow);
                    if (mouseClicked) {
                        data->clickedRow = static_cast<int>(hitRow);
                        const double now = (ctx.GetCurrentTime)();
                        if (state.lastClickRow == static_cast<int>(hitRow) && (now - state.lastClickTime) <= 0.35) {
                            data->doubleClickedRow = static_cast<int>(hitRow);
                        }
                        state.lastClickTime = now;
                        state.lastClickRow = static_cast<int>(hitRow);

                        if (state.selectedRow != static_cast<int>(hitRow)) {
                            state.selectedRow = static_cast<int>(hitRow);
                            data->selectionChanged = true;
                            if (optsCopy.selectedRow) {
                                *optsCopy.selectedRow = state.selectedRow;
                            }
                            ctx.InvalidateRect(data->frameRect);
                        }
                        ctx.RequestFocus(id);
                    }
                }
            }
        }

        // 4. Keyboard navigation when table is focused or hovered
        if ((ctx.GetFocusedId() == id || data->frameRect.Contains(mousePos)) && knownTotalRows > 0) {
            int nextSel = state.selectedRow;
            if (ctx.ConsumeAction(KeyAction::MoveDown)) {
                nextSel = std::clamp(nextSel + 1, 0, static_cast<int>(knownTotalRows) - 1);
            } else if (ctx.ConsumeAction(KeyAction::MoveUp)) {
                nextSel = std::clamp((nextSel < 0 ? 0 : nextSel - 1), 0, static_cast<int>(knownTotalRows) - 1);
            } else if (ctx.ConsumeAction(KeyAction::MoveLineStart) || ctx.ConsumeAction(KeyAction::MoveDocStart)) {
                nextSel = 0;
            } else if (ctx.ConsumeAction(KeyAction::MoveLineEnd) || ctx.ConsumeAction(KeyAction::MoveDocEnd)) {
                nextSel = static_cast<int>(knownTotalRows) - 1;
            } else if (ctx.ConsumeAction(KeyAction::MovePageDown)) {
                const int pageStep = std::max(1, static_cast<int>(state.lastViewportHeight / std::max(1.0f, data->rowHeight)));
                nextSel = std::clamp((nextSel < 0 ? 0 : nextSel) + pageStep, 0, static_cast<int>(knownTotalRows) - 1);
            } else if (ctx.ConsumeAction(KeyAction::MovePageUp)) {
                const int pageStep = std::max(1, static_cast<int>(state.lastViewportHeight / std::max(1.0f, data->rowHeight)));
                nextSel = std::clamp((nextSel < 0 ? 0 : nextSel) - pageStep, 0, static_cast<int>(knownTotalRows) - 1);
            }

            if (nextSel != state.selectedRow && nextSel >= 0) {
                state.selectedRow = nextSel;
                data->selectionChanged = true;
                if (optsCopy.selectedRow) {
                    *optsCopy.selectedRow = state.selectedRow;
                }
                // Keep newly selected row in view
                const float rTop = static_cast<float>(nextSel) * data->rowHeight;
                const float rBot = rTop + data->rowHeight;
                if (rTop < state.scrollOffset.y) {
                    state.scrollOffset.y = rTop;
                } else if (rBot > state.scrollOffset.y + state.lastViewportHeight) {
                    state.scrollOffset.y = std::max(0.0f, rBot - state.lastViewportHeight);
                }
                ctx.InvalidateRect(data->frameRect);
                ctx.RequestRepaint(2);
            }
        }

        if (std::abs(state.scrollOffset.x - prevScroll.x) > 0.01f ||
            std::abs(state.scrollOffset.y - prevScroll.y) > 0.01f) {
            ctx.InvalidateRect(data->frameRect);
            ctx.RequestRepaint(2);
        }
    }

    // Compute visible row range [firstVisibleRow, endVisibleRow) with 1 overscan row above/below
    UpdateVisibleRowRange(*data);
    const size_t visibleCount = (data->endVisibleRow >= data->firstVisibleRow)
        ? (data->endVisibleRow - data->firstVisibleRow)
        : 32;

    const size_t cap = std::min<size_t>(visibleCount + 2, 128);
    state.visibleRows.reserve(cap);
    state.fastCells.reserve(cap * std::max<size_t>(1, data->columnCount));

    node->scrollOffset = state.scrollOffset;
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        optsCopy.striped,
        optsCopy.gridLines,
        optsCopy.background,
        optsCopy.border,
        optsCopy.stickyHeader,
        optsCopy.showRowNumbers,
        optsCopy.showFooter,
        optsCopy.footerText.text,
        data->columnCount,
        activeDisabled
    );
    node->measureFn = &Table::Measure;
    node->arrangeFn = &Table::Arrange;
    node->renderFn = &Table::RenderBody;
    node->postRenderFn = &Table::RenderChromeAndScrollbars;

    if (node->parent && node->parent->type == NodeType::Element) {
        Control::SetupNode(
            ctx,
            node,
            ControlSpec{
                .width = optsCopy.width,
                .height = optsCopy.height,
                .defaultLayout = ElementLayout::Top,
                .stretchable = true,
                .stretchHeightWhenStandalone = !optsCopy.height.has_value(),
                .hasIntrinsicBox = true
            },
            optsCopy.stretch,
            Align::Start
        );
    } else {
        ctx.AdaptChildToContainer(node);
    }

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);

    return TableScope(&ctx, node, data);
}

// ============================================================================
// Table::Measure & Table::Arrange & Table::GetBodyClipRect
// ============================================================================

Vec2 Table::Measure(
    const Node* node,
    float availableWidth,
    ICanvas* /*canvas*/,
    const UiTheme& theme
) {
    const auto* data = node ? node->As<TableData>() : nullptr;
    if (!data) return {200.0f, 160.0f};

    const float headerH = data->opts.stickyHeader
        ? (data->opts.headerHeight.has_value()
            ? theme.Scale(*data->opts.headerHeight)
            : ResolveDefaultHeaderHeight(node->uiSize, theme))
        : 0.0f;
    const float rowH = data->opts.rowHeight.has_value()
        ? std::max(18.0f, theme.Scale(*data->opts.rowHeight))
        : ResolveDefaultRowHeight(node->uiSize, theme);
    const float footerH = (data->opts.showFooter || !data->opts.footerText.empty())
        ? ResolveDefaultFooterHeight(node->uiSize, theme)
        : 0.0f;

    const size_t totalRows = (data->opts.totalRows > 0)
        ? data->opts.totalRows
        : data->emittedRowCount;

    float naturalW = data->opts.showRowNumbers ? theme.Scale(44.0f) : 0.0f;
    for (uint16_t i = 0; i < data->columnCount; ++i) {
        const float colW = (data->columns[i].width > 0.0f)
            ? theme.Scale(data->columns[i].width)
            : theme.Scale(std::max(80.0f, data->columns[i].minWidth));
        naturalW += colW;
    }
    if (data->opts.width.has_value() && data->opts.stretch != StretchMode::Fill && *data->opts.width > 0.0f) {
        naturalW = theme.Scale(*data->opts.width);
    } else if (availableWidth > 0.0f && data->opts.stretch == StretchMode::Fill) {
        naturalW = std::max(naturalW, availableWidth);
    }

    const size_t fitRows = std::clamp<size_t>(totalRows, 3, 12);
    const float naturalTableH = data->opts.height.has_value()
        ? theme.Scale(*data->opts.height)
        : (headerH + static_cast<float>(fitRows) * rowH + footerH);

    return {naturalW, naturalTableH};
}

void Table::Arrange(
    Node* node,
    const Rect& availableRect,
    const UiTheme& theme
) {
    if (!node) return;
    node->computedRect = availableRect;
    auto* data = node->As<TableData>();
    if (!data) return;

    ComputeTableRegions(node, *data, availableRect, theme);
    ResolveColumnWidths(*data, data->frameRect.w, theme);

    if (data->state) {
        data->state->lastArrangedBounds = availableRect;
        if (data->bodyViewportRect.h > 0.0f) {
            data->state->lastViewportHeight = data->bodyViewportRect.h;
        }
    }

    // Position all widget Cell children (NodeType::Cell) inside the virtualized body viewport
    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        if (child->type != NodeType::Cell) continue;
        const uint16_t colIdx = child->colSpan;
        if (colIdx >= data->columnCount) continue;

        const size_t rowIndex = static_cast<size_t>(child->childIndex);
        const float cellX = data->bodyViewportRect.x + data->resolvedColX[colIdx] - node->scrollOffset.x;
        const float cellY = data->bodyViewportRect.y + static_cast<float>(rowIndex) * data->rowHeight - node->scrollOffset.y;
        const float cellW = data->resolvedColW[colIdx];
        const float cellH = data->rowHeight;

        child->computedRect = Rect(cellX, cellY, cellW, cellH);

        // Arrange widgets inside the cell using standard horizontal flex layout
        const Padding pad = child->padding;
        const Rect inner(
            cellX + pad.left,
            cellY + pad.top,
            std::max(0.0f, cellW - pad.Horizontal()),
            std::max(0.0f, cellH - pad.Vertical())
        );

        // Count and measure cell children
        float totalChildrenW = 0.0f;
        uint32_t numGrandChildren = 0;
        for (Node* gc = child->firstChild; gc != nullptr; gc = gc->nextSibling) {
            const Vec2 m = gc->measureFn
                ? gc->measureFn(gc, inner.w, nullptr, theme)
                : Vec2(gc->fixedWidth, gc->fixedHeight);
            gc->intrinsicSize = m;
            totalChildrenW += (gc->widthMode == SizeMode::Stretch) ? inner.w : m.x;
            ++numGrandChildren;
        }
        if (numGrandChildren > 1) {
            totalChildrenW += child->gap * static_cast<float>(numGrandChildren - 1);
        }

        float curX = inner.x;
        if (child->justify == Justify::Center && totalChildrenW < inner.w) {
            curX += (inner.w - totalChildrenW) * 0.5f;
        } else if (child->justify == Justify::End && totalChildrenW < inner.w) {
            curX += (inner.w - totalChildrenW);
        }

        for (Node* gc = child->firstChild; gc != nullptr; gc = gc->nextSibling) {
            const float w = (gc->widthMode == SizeMode::Stretch)
                ? std::max(0.0f, inner.Right() - curX)
                : std::min(gc->intrinsicSize.x, std::max(0.0f, inner.Right() - curX));
            const float h = (gc->heightMode == SizeMode::Stretch)
                ? inner.h
                : std::min(gc->intrinsicSize.y, inner.h);
            const float y = inner.y + (inner.h - h) * 0.5f;
            gc->computedRect = Rect(curX, y, w, h);
            if (gc->arrangeFn) {
                gc->arrangeFn(gc, gc->computedRect, theme);
            }
            curX += w + child->gap;
        }
    }
}

Rect Table::GetBodyClipRect(
    const Node* node,
    const UiTheme& theme
) {
    if (!node) return {};
    const auto* data = node->As<TableData>();
    if (!data) return node->computedRect;
    if (data->bodyViewportRect.w > 0.0f || data->bodyViewportRect.h > 0.0f) {
        return data->bodyViewportRect;
    }
    TableData tmp = *data;
    ComputeTableRegions(node, tmp, node->computedRect, theme);
    return tmp.bodyViewportRect;
}

void Table::Update(
    Context& ctx,
    Node* node
) {
    if (!node || node->disabled) return;
    auto* data = node->As<TableData>();
    if (!data || !data->state) return;

    const UiTheme& theme = node->theme ? *node->theme : ctx.GetTheme();
    ComputeTableRegions(node, *data, node->computedRect, theme);
    ResolveColumnWidths(*data, data->frameRect.w, theme);

    const float maxScrollY = std::max(0.0f, node->scrollContentSize.y - data->bodyViewportRect.h);
    const float maxScrollX = std::max(0.0f, node->scrollContentSize.x - data->bodyViewportRect.w);
    data->state->scrollOffset.y = std::clamp(data->state->scrollOffset.y, 0.0f, maxScrollY);
    data->state->scrollOffset.x = std::clamp(data->state->scrollOffset.x, 0.0f, maxScrollX);

    if (std::abs(data->state->scrollOffset.x - node->scrollOffset.x) > 0.01f ||
        std::abs(data->state->scrollOffset.y - node->scrollOffset.y) > 0.01f) {
        node->scrollOffset = data->state->scrollOffset;
        Table::Arrange(node, node->computedRect, theme);
        ctx.InvalidateRect(data->frameRect);
        ctx.RequestRepaint(2);
    }
}

// ============================================================================
// Table::RenderBody & Table::RenderChromeAndScrollbars
// ============================================================================

void Table::RenderBody(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node) return;
    const auto* data = node->As<TableData>();
    if (!data || !data->state) return;

    const auto& pal = theme.palette;
    const float scale = theme.scale;
    const float radius = data->opts.border.radius.has_value()
        ? theme.Scale(*data->opts.border.radius)
        : theme.GetRadius(node->uiSize);

    // 1. Draw outer table surface background
    if (data->opts.background.IsBackgroundEnabled(true)) {
        const Color bgCol = data->opts.background.IsSurface()
            ? pal.surface
            : data->opts.background.Resolve(false, false, node->disabled, pal.surface);
        if (bgCol.a > 0) {
            canvas.DrawRoundRect(data->frameRect, radius, bgCol);
        }
    }

    // 3. Render virtualized rows & fast cells clipped to bodyViewportRect
    if (data->bodyViewportRect.w > 0.0f && data->bodyViewportRect.h > 0.0f) {
        canvas.PushClip(data->bodyViewportRect);

        const float bodyFontSize = ResolveTableFontSize(node->uiSize, theme, false);
        const float cellPadX = 8.0f * scale;

        // Draw row backgrounds (zebra striping, tint, hover, selection, horizontal grid lines)
        for (const auto& rowRec : data->state->visibleRows) {
            const size_t rIdx = rowRec.rowIndex;
            const float rowY = data->bodyViewportRect.y + static_cast<float>(rIdx) * data->rowHeight - node->scrollOffset.y;
            const Rect rowRect(data->bodyViewportRect.x, rowY, data->bodyViewportRect.w, data->rowHeight);

            if (!rowRect.Intersects(data->bodyViewportRect)) continue;

            const int iRow = static_cast<int>(rIdx);
            const bool isSelected = (data->state->selectedRow == iRow);
            const bool isHovered = data->opts.hoverableRows && (data->hoveredRow == iRow) && !rowRec.opts.disabled;

            if (isSelected) {
                canvas.DrawRect(rowRect, pal.accent.WithAlpha(42));
                // Left accent indicator bar on selected row
                canvas.DrawRect(Rect(rowRect.x, rowRect.y, 3.0f * scale, rowRect.h), pal.accent);
            } else if (isHovered) {
                canvas.DrawRect(rowRect, pal.surfaceHover.WithAlpha(180));
            } else if (rowRec.opts.tint.has_value()) {
                const Color tintCol = theme.GetVariant(*rowRec.opts.tint).bg.WithAlpha(28);
                canvas.DrawRect(rowRect, tintCol);
            } else if (data->opts.striped && (rIdx % 2 == 1)) {
                canvas.DrawRect(rowRect, pal.surfaceElevated.WithAlpha(90));
            }

            // Optional automatic '#' row number cell
            if (data->opts.showRowNumbers && data->rowNumberColW > 0.0f) {
                char numBuf[24];
                std::snprintf(numBuf, sizeof(numBuf), "%zu", rIdx + 1);
                const Rect numRect(
                    data->bodyViewportRect.x - node->scrollOffset.x + cellPadX * 0.5f,
                    rowY,
                    std::max(0.0f, data->rowNumberColW - cellPadX),
                    data->rowHeight
                );
                canvas.DrawTextBox(
                    numBuf,
                    numRect,
                    bodyFontSize * 0.9f,
                    pal.textMuted,
                    1.0f,
                    0.5f
                );
            }

            if (data->opts.gridLines) {
                const float lineY = std::floor(rowRect.Bottom()) - 0.5f;
                canvas.DrawLine(
                    Vec2(rowRect.x, lineY),
                    Vec2(rowRect.Right(), lineY),
                    pal.borderSubtle.WithAlpha(95),
                    1.0f
                );
            }
        }

        // Draw fast zero-node cells (CellText & CellBadge)
        for (const auto& cell : data->state->fastCells) {
            if (cell.colIndex >= data->columnCount) continue;
            const float cellX = data->bodyViewportRect.x + data->resolvedColX[cell.colIndex] - node->scrollOffset.x;
            const float cellY = data->bodyViewportRect.y + static_cast<float>(cell.rowIndex) * data->rowHeight - node->scrollOffset.y;
            const float cellW = data->resolvedColW[cell.colIndex];
            const float cellH = data->rowHeight;
            const Rect cellRect(cellX, cellY, cellW, cellH);

            if (!cellRect.Intersects(data->bodyViewportRect)) continue;

            const Align align = cell.opts.align.value_or(data->columns[cell.colIndex].align);
            const float hAlign = AlignToFactor(align);

            if (cell.isBadge) {
                const float badgeFontSize = ResolveTableFontSize(node->uiSize, theme, true);
                const Vec2 textSz = canvas.MeasureText(cell.text.text, badgeFontSize);
                const float pillPadX = 7.0f * scale;
                const float pillH = std::min(cellH - 6.0f * scale, std::max(16.0f * scale, textSz.y + 4.0f * scale));
                const float pillW = std::min(std::max(0.0f, cellW - cellPadX * 2.0f), textSz.x + pillPadX * 2.0f);

                const float innerX = cellX + cellPadX;
                const float innerW = std::max(0.0f, cellW - cellPadX * 2.0f);
                const float pillX = innerX + (innerW - pillW) * hAlign;
                const float pillY = cellY + (cellH - pillH) * 0.5f;
                const Rect pillRect(pillX, pillY, pillW, pillH);

                const VariantColors vc = theme.GetVariant(cell.opts.variant);
                canvas.DrawRoundRect(pillRect, pillH * 0.5f, vc.bg.WithAlpha(48), vc.border.WithAlpha(140), 1.0f);
                canvas.DrawTextBox(cell.text.text, pillRect, badgeFontSize, vc.bg, 0.5f, 0.5f);
            } else {
                Rect textRect(
                    cellX + cellPadX,
                    cellY,
                    std::max(0.0f, cellW - cellPadX * 2.0f),
                    cellH
                );

                Color textCol = (cell.opts.variant != UiVariant::Default)
                    ? theme.GetVariant(cell.opts.variant).bg
                    : theme.ResolveTextColor(cell.text, UiVariant::Default, node->disabled);

                if (cell.opts.icon.IsValid()) {
                    const float iconSz = std::min(14.0f * scale, cellH * 0.55f);
                    const Rect iconRect(textRect.x, cellY + (cellH - iconSz) * 0.5f, iconSz, iconSz);
                    cell.opts.icon.Draw(canvas, iconRect, textCol);
                    const float advance = iconSz + 5.0f * scale;
                    textRect.x += advance;
                    textRect.w = std::max(0.0f, textRect.w - advance);
                }

                if (!cell.text.empty() && textRect.w > 2.0f) {
                    const std::string truncated = TextUtil::TruncateWithEllipsis(
                        cell.text.text,
                        textRect.w,
                        bodyFontSize,
                        &canvas
                    );
                    canvas.DrawTextBox(truncated, textRect, bodyFontSize, textCol, hAlign, 0.5f);
                }
            }
        }

        // Draw vertical column grid lines across the body viewport
        if (data->opts.gridLines) {
            if (data->opts.showRowNumbers && data->rowNumberColW > 0.0f) {
                const float divX = std::floor(data->bodyViewportRect.x + data->rowNumberColW - node->scrollOffset.x) + 0.5f;
                if (divX >= data->bodyViewportRect.x && divX <= data->bodyViewportRect.Right()) {
                    canvas.DrawLine(
                        Vec2(divX, data->bodyViewportRect.y),
                        Vec2(divX, data->bodyViewportRect.Bottom()),
                        pal.borderSubtle.WithAlpha(95),
                        1.0f
                    );
                }
            }
            for (uint16_t i = 0; i < data->columnCount; ++i) {
                const float divX = std::floor(
                    data->bodyViewportRect.x + data->resolvedColX[i] + data->resolvedColW[i] - node->scrollOffset.x
                ) + 0.5f;
                if (divX >= data->bodyViewportRect.x && divX < data->bodyViewportRect.Right() - 0.5f) {
                    canvas.DrawLine(
                        Vec2(divX, data->bodyViewportRect.y),
                        Vec2(divX, data->bodyViewportRect.Bottom()),
                        pal.borderSubtle.WithAlpha(85),
                        1.0f
                    );
                }
            }
        }

        // Draw full-height vertical resize guide line when hovering or dragging a column divider
        const int activeDivCol = (data->state->resizingCol >= 0)
            ? data->state->resizingCol
            : data->hoveredDividerCol;
        if (activeDivCol >= 0 && static_cast<uint16_t>(activeDivCol) < data->columnCount) {
            const uint16_t colIdx = static_cast<uint16_t>(activeDivCol);
            const float guideX = std::clamp(
                std::floor(data->bodyViewportRect.x + data->resolvedColX[colIdx] + data->resolvedColW[colIdx] - node->scrollOffset.x) + 0.5f,
                data->bodyViewportRect.x + 1.0f,
                data->bodyViewportRect.Right() - 1.5f
            );
            const Color guideCol = (data->state->resizingCol >= 0)
                ? pal.accent.WithAlpha(210)
                : pal.accent.WithAlpha(110);
            canvas.DrawLine(
                Vec2(guideX, data->bodyViewportRect.y),
                Vec2(guideX, data->bodyViewportRect.Bottom()),
                guideCol,
                (data->state->resizingCol >= 0) ? (1.5f * scale) : 1.0f
            );
        }

        canvas.PopClip();
    }
}

void Table::RenderChromeAndScrollbars(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node) return;
    const auto* data = node->As<TableData>();
    if (!data || !data->state) return;

    const auto& pal = theme.palette;
    const float scale = theme.scale;
    const float radius = data->opts.border.radius.has_value()
        ? theme.Scale(*data->opts.border.radius)
        : theme.GetRadius(node->uiSize);
    const float cellPadX = 8.0f * scale;

    // 1. Render sticky column header bar
    if (data->opts.stickyHeader && data->headerRect.w > 0.0f && data->headerRect.h > 0.0f) {
        canvas.PushClip(data->headerRect);
        canvas.DrawRect(data->headerRect, pal.surfaceElevated);

        const float headerFontSize = ResolveTableFontSize(node->uiSize, theme, true);

        if (data->opts.showRowNumbers && data->rowNumberColW > 0.0f) {
            const Rect numHdrRect(
                data->headerRect.x - node->scrollOffset.x + cellPadX * 0.5f,
                data->headerRect.y,
                std::max(0.0f, data->rowNumberColW - cellPadX),
                data->headerRect.h
            );
            canvas.DrawTextBox("#", numHdrRect, headerFontSize, pal.textMuted, 1.0f, 0.5f);
        }

        for (uint16_t i = 0; i < data->columnCount; ++i) {
            const auto& col = data->columns[i];
            const float colX = data->headerRect.x + data->resolvedColX[i] - node->scrollOffset.x;
            const float colW = data->resolvedColW[i];
            const Rect colHdrRect(colX, data->headerRect.y, colW, data->headerRect.h);

            if (!colHdrRect.Intersects(data->headerRect)) continue;

            const bool isHovered = (data->hoveredHeaderCol == static_cast<int>(i)) && col.sortable;
            const bool isSorted = (data->state->sortColumn == static_cast<int>(i)) &&
                                  (data->state->sortDirection != TableSortDirection::None);

            if (isHovered) {
                canvas.DrawRect(colHdrRect, pal.surfaceHover);
            } else if (isSorted) {
                canvas.DrawRect(colHdrRect, pal.accent.WithAlpha(20));
            }

            const float sortArrowReserve = col.sortable ? (14.0f * scale) : 0.0f;
            Rect titleRect(
                colX + cellPadX,
                data->headerRect.y,
                std::max(0.0f, colW - cellPadX * 2.0f - sortArrowReserve),
                data->headerRect.h
            );

            const Color titleCol = isSorted ? pal.accent : pal.textSecondary;
            if (col.icon.IsValid()) {
                const float iconSz = 13.0f * scale;
                const Rect iconRect(titleRect.x, data->headerRect.y + (data->headerRect.h - iconSz) * 0.5f, iconSz, iconSz);
                col.icon.Draw(canvas, iconRect, titleCol);
                titleRect.x += iconSz + 5.0f * scale;
                titleRect.w = std::max(0.0f, titleRect.w - (iconSz + 5.0f * scale));
            }

            if (!col.title.empty() && titleRect.w > 2.0f) {
                const std::string truncated = TextUtil::TruncateWithEllipsis(
                    col.title.text,
                    titleRect.w,
                    headerFontSize,
                    &canvas
                );
                canvas.DrawTextBox(
                    truncated,
                    titleRect,
                    headerFontSize,
                    titleCol,
                    AlignToFactor(col.align),
                    0.5f
                );
            }

            // Draw sort indicator chevron/triangle
            if (col.sortable) {
                const float arrowCx = colX + colW - cellPadX - 4.0f * scale;
                const float arrowCy = data->headerRect.Center().y;
                const float s = 3.5f * scale;
                if (isSorted) {
                    if (data->state->sortDirection == TableSortDirection::Ascending) {
                        canvas.DrawLine(Vec2(arrowCx - s, arrowCy + s * 0.5f), Vec2(arrowCx, arrowCy - s * 0.6f), pal.accent, 1.6f * scale);
                        canvas.DrawLine(Vec2(arrowCx, arrowCy - s * 0.6f), Vec2(arrowCx + s, arrowCy + s * 0.5f), pal.accent, 1.6f * scale);
                    } else {
                        canvas.DrawLine(Vec2(arrowCx - s, arrowCy - s * 0.5f), Vec2(arrowCx, arrowCy + s * 0.6f), pal.accent, 1.6f * scale);
                        canvas.DrawLine(Vec2(arrowCx, arrowCy + s * 0.6f), Vec2(arrowCx + s, arrowCy - s * 0.5f), pal.accent, 1.6f * scale);
                    }
                } else if (isHovered) {
                    // Subtle dual-chevron hint on hover
                    const Color hintCol = pal.textMuted.WithAlpha(160);
                    canvas.DrawLine(Vec2(arrowCx - s * 0.8f, arrowCy - 1.5f * scale), Vec2(arrowCx, arrowCy - s), hintCol, 1.2f * scale);
                    canvas.DrawLine(Vec2(arrowCx, arrowCy - s), Vec2(arrowCx + s * 0.8f, arrowCy - 1.5f * scale), hintCol, 1.2f * scale);
                    canvas.DrawLine(Vec2(arrowCx - s * 0.8f, arrowCy + 1.5f * scale), Vec2(arrowCx, arrowCy + s), hintCol, 1.2f * scale);
                    canvas.DrawLine(Vec2(arrowCx, arrowCy + s), Vec2(arrowCx + s * 0.8f, arrowCy + 1.5f * scale), hintCol, 1.2f * scale);
                }
            }

            // Column divider line + resize handle highlight
            const float divX = std::min(std::floor(colX + colW) + 0.5f, data->headerRect.Right() - 1.5f);
            const bool dividerActive = (data->state->resizingCol == static_cast<int>(i)) ||
                                       (data->hoveredDividerCol == static_cast<int>(i));
            const Color divCol = dividerActive ? pal.accent : pal.border;
            const float divThickness = dividerActive ? (2.0f * scale) : 1.0f;
            canvas.DrawLine(
                Vec2(divX, data->headerRect.y + 4.0f * scale),
                Vec2(divX, data->headerRect.Bottom() - 4.0f * scale),
                divCol,
                divThickness
            );
        }

        // Bottom separator line under sticky header
        const float hdrBotY = std::floor(data->headerRect.Bottom()) - 0.5f;
        canvas.DrawLine(
            Vec2(data->headerRect.x, hdrBotY),
            Vec2(data->headerRect.Right(), hdrBotY),
            pal.border,
            1.0f
        );
        canvas.PopClip();
    }

    // 2. Render optional footer bar (showing visible rows / total rows + custom footerText)
    if (data->footerRect.w > 0.0f && data->footerRect.h > 0.0f) {
        canvas.PushClip(data->footerRect);
        canvas.DrawRect(data->footerRect, pal.surfaceElevated);
        const float topY = std::floor(data->footerRect.y) + 0.5f;
        canvas.DrawLine(
            Vec2(data->footerRect.x, topY),
            Vec2(data->footerRect.Right(), topY),
            pal.borderSubtle,
            1.0f
        );

        const float footerFontSize = ResolveTableFontSize(node->uiSize, theme, true);
        const size_t totalRows = (data->opts.totalRows > 0)
            ? data->opts.totalRows
            : data->emittedRowCount;

        char summaryBuf[96];
        if (totalRows > 0) {
            const size_t dispFirst = std::min(totalRows, data->firstVisibleRow + 1);
            const size_t dispLast = std::min(totalRows, data->endVisibleRow);
            if (data->state->selectedRow >= 0) {
                std::snprintf(
                    summaryBuf,
                    sizeof(summaryBuf),
                    "Rows %zu..%zu of %zu  |  Selected: #%d",
                    dispFirst,
                    dispLast,
                    totalRows,
                    data->state->selectedRow + 1
                );
            } else {
                std::snprintf(
                    summaryBuf,
                    sizeof(summaryBuf),
                    "Rows %zu..%zu of %zu",
                    dispFirst,
                    dispLast,
                    totalRows
                );
            }
        } else {
            std::snprintf(summaryBuf, sizeof(summaryBuf), "0 rows");
        }

        const Rect leftRect(
            data->footerRect.x + cellPadX,
            data->footerRect.y,
            std::max(0.0f, data->footerRect.w * 0.6f - cellPadX),
            data->footerRect.h
        );
        canvas.DrawTextBox(summaryBuf, leftRect, footerFontSize, pal.textMuted, 0.0f, 0.5f);

        if (!data->opts.footerText.empty()) {
            const Rect rightRect(
                data->footerRect.x + data->footerRect.w * 0.4f,
                data->footerRect.y,
                std::max(0.0f, data->footerRect.w * 0.6f - cellPadX),
                data->footerRect.h
            );
            canvas.DrawTextBox(
                data->opts.footerText.text,
                rightRect,
                footerFontSize,
                pal.textSecondary,
                1.0f,
                0.5f
            );
        }
        canvas.PopClip();
    }

    // 3. Render vertical & horizontal scrollbars on the body viewport
    const bool showV = (node->scrollbarPolicy == VisibilityPolicy::Always) ||
        (node->scrollbarPolicy == VisibilityPolicy::Auto &&
         node->scrollContentSize.y > data->bodyViewportRect.h + 0.5f);
    const bool showH = (node->scrollbarPolicy == VisibilityPolicy::Always) ||
        (node->scrollbarPolicy == VisibilityPolicy::Auto &&
         node->scrollContentSize.x > data->bodyViewportRect.w + 0.5f);

    const ScrollbarMetrics& sm = theme.GetScrollbar(node->uiSize);
    const float cornerReserve = (showV && showH) ? (sm.trackWidth + sm.margin) : 0.0f;

    if (showV) {
        ScrollArea::RenderAxisScrollbar(
            canvas,
            data->bodyViewportRect,
            node->scrollContentSize.y,
            node->scrollOffset.y,
            false,
            node->scrollbarHovered,
            node->scrollbarActive,
            node->uiSize,
            theme,
            cornerReserve
        );
    }
    if (showH) {
        ScrollArea::RenderAxisScrollbar(
            canvas,
            data->bodyViewportRect,
            node->scrollContentSize.x,
            node->scrollOffset.x,
            true,
            node->scrollbarHoveredX,
            node->scrollbarActiveX,
            node->uiSize,
            theme,
            cornerReserve
        );
    }

    // 4. Outer rounded frame border
    const float bw = (data->opts.border.width > 0.0f) ? theme.Scale(data->opts.border.width) : 0.0f;
    if (bw > 0.0f) {
        const Color borderCol = (data->state->resizingCol >= 0)
            ? pal.accent
            : data->opts.border.color.Resolve(false, false, node->disabled, pal.border);
        if (borderCol.a > 0) {
            canvas.DrawRoundRect(data->frameRect, radius, Color::Transparent(), borderCol, bw);
        }
    }
}

} // namespace guikit
