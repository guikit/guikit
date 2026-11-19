#include "guikit/panels/table.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/element.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/panels/scroll_area.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/math.h"
#include "keel/static_array.h"

namespace guikit {

namespace {

constexpr keel::usize kMaxTableColumns = 16;

struct TableRowRecord {
    keel::usize rowIndex{0};
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

    keel::u16 userColCount{0};
    keel::StaticArray<float, kMaxTableColumns> userColWidths{};
    keel::StaticArray<bool, kMaxTableColumns> hasUserColWidth{};
    keel::i32 resizingCol{-1};
    float resizeStartX{0.0f};
    float resizeStartWidth{0.0f};

    keel::i32 selectedRow{-1};
    keel::i32 sortColumn{-1};
    TableSortDirection sortDirection{TableSortDirection::None};

    double lastClickTime{-10.0};
    keel::i32 lastClickRow{-1};
    double lastDividerClickTime{-10.0};
    keel::i32 lastDividerCol{-1};

    keel::usize prevTotalRows{0};
    float lastViewportHeight{240.0f};
    Rect lastArrangedBounds{};

    // Persistent frame buffers (cleared each frame, zero heap allocation after warmup)
    keel::Array<TableRowRecord> visibleRows{};
};

float ResolveDefaultRowHeight(UiSize size, const UiTheme& theme) noexcept {
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs: return 24.0f;
        case UiSize::Preset::Sm: return 28.0f;
        case UiSize::Preset::Lg: return 38.0f;
        case UiSize::Preset::Xl: return 44.0f;
        case UiSize::Preset::Md:
        case UiSize::Preset::None:
            return 32.0f;
        default:
            return 32.0f * theme.SizeFactor(size);
    }
}

float ResolveDefaultHeaderHeight(UiSize size, const UiTheme& theme) noexcept {
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs: return 26.0f;
        case UiSize::Preset::Sm: return 30.0f;
        case UiSize::Preset::Lg: return 40.0f;
        case UiSize::Preset::Xl: return 46.0f;
        case UiSize::Preset::Md:
        case UiSize::Preset::None:
            return 34.0f;
        default:
            return 34.0f * theme.SizeFactor(size);
    }
}

float ResolveDefaultFooterHeight(UiSize size, const UiTheme& theme) noexcept {
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs: return 22.0f;
        case UiSize::Preset::Sm: return 24.0f;
        case UiSize::Preset::Lg: return 30.0f;
        case UiSize::Preset::Xl: return 34.0f;
        case UiSize::Preset::Md:
        case UiSize::Preset::None:
            return 26.0f;
        default:
            return 26.0f * theme.SizeFactor(size);
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

    keel::u16 columnCount{0};
    keel::StaticArray<TableColumn, kMaxTableColumns> columns{};
    keel::StaticArray<float, kMaxTableColumns> resolvedColX{};
    keel::StaticArray<float, kMaxTableColumns> resolvedColW{};
    float rowNumberColW{0.0f};
    float totalColumnsW{0.0f};

    float rowHeight{32.0f};
    float headerHeight{34.0f};
    float footerHeight{0.0f};

    keel::usize firstVisibleRow{0};
    keel::usize endVisibleRow{0};
    keel::usize emittedRowCount{0};

    bool hasActiveRow{false};
    keel::usize activeRowIndex{0};
    keel::u16 activeColCursor{0};
    Node* activeRowPrevLastChild{nullptr};
    mutable ICanvas* lastCanvas{nullptr};

    keel::i32 hoveredHeaderCol{-1};
    keel::i32 hoveredDividerCol{-1};
    keel::i32 hoveredRow{-1};
    keel::i32 clickedRow{-1};
    keel::i32 doubleClickedRow{-1};
    bool sortChanged{false};
    bool selectionChanged{false};

    Rect frameRect{};
    Rect headerRect{};
    Rect bodyViewportRect{};
    Rect footerRect{};
};

namespace {

void ResolveColumnWidths(TableData& data, float viewportW, const UiTheme& /*theme*/) noexcept {
    const float scale = 1.0f;
    data.rowNumberColW = data.opts.showRowNumbers ? keel::round(44.0f * scale) : 0.0f;
    const float availForCols = keel::max(0.0f, viewportW - data.rowNumberColW);

    float fixedSum = 0.0f;
    float flexSum = 0.0f;

    for (keel::u16 i = 0; i < data.columnCount; ++i) {
        const auto& col = data.columns[i];
        const float minW = keel::max(28.0f * scale, col.minWidth * scale);
        const float maxW = (col.maxWidth > 0.0f) ? (col.maxWidth * scale) : 100000.0f;

        if (data.state && data.state->hasUserColWidth[i]) {
            const float w = keel::clamp(data.state->userColWidths[i] * scale, minW, maxW);
            data.resolvedColW[i] = w;
            fixedSum += w;
        } else if (col.width > 0.0f) {
            const float w = keel::clamp(col.width * scale, minW, maxW);
            data.resolvedColW[i] = w;
            fixedSum += w;
        } else {
            data.resolvedColW[i] = 0.0f;
            flexSum += keel::max(0.1f, col.flex);
        }
    }

    const float remainingW = keel::max(0.0f, availForCols - fixedSum);
    float curX = data.rowNumberColW;

    for (keel::u16 i = 0; i < data.columnCount; ++i) {
        const auto& col = data.columns[i];
        const float minW = keel::max(28.0f * scale, col.minWidth * scale);
        const float maxW = (col.maxWidth > 0.0f) ? (col.maxWidth * scale) : 100000.0f;

        if (data.resolvedColW[i] <= 0.0f) {
            const float weight = keel::max(0.1f, col.flex);
            const float flexShare = (flexSum > 0.0f) ? (remainingW * (weight / flexSum)) : minW;
            data.resolvedColW[i] = keel::clamp(keel::round(flexShare), minW, maxW);
        }

        // Stretch last column if auto-flex columns slightly undershoot viewport due to rounding
        if (i + 1 == data.columnCount && flexSum > 0.0f &&
            !(data.state && data.state->hasUserColWidth[i]) &&
            curX + data.resolvedColW[i] < viewportW) {
            data.resolvedColW[i] = keel::min(maxW, viewportW - curX);
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
            ? *data.opts.headerHeight
            : ResolveDefaultHeaderHeight(node->uiSize, theme))
        : 0.0f;
    data.rowHeight = data.opts.rowHeight.has_value()
        ? keel::max(18.0f, *data.opts.rowHeight)
        : ResolveDefaultRowHeight(node->uiSize, theme);
    data.footerHeight = (data.opts.showFooter || !data.opts.footerText.empty())
        ? ResolveDefaultFooterHeight(node->uiSize, theme)
        : 0.0f;

    const float headerH = keel::min(data.headerHeight, data.frameRect.h);
    const float footerH = keel::min(data.footerHeight, keel::max(0.0f, data.frameRect.h - headerH));
    const float bodyH = keel::max(0.0f, data.frameRect.h - headerH - footerH);

    data.headerRect = Rect(data.frameRect.x, data.frameRect.y, data.frameRect.w, headerH);
    data.bodyViewportRect = Rect(data.frameRect.x, data.frameRect.y + headerH, data.frameRect.w, bodyH);
    data.footerRect = Rect(data.frameRect.x, data.frameRect.Bottom() - footerH, data.frameRect.w, footerH);
}

void UpdateVisibleRowRange(TableData& data) noexcept {
    if (!data.state) return;
    const float viewH = keel::max(data.rowHeight, data.state->lastViewportHeight);
    const keel::usize rawFirst = (data.rowHeight > 0.0f)
        ? static_cast<keel::usize>(keel::max(0.0f, keel::floor(data.state->scrollOffset.y / data.rowHeight)))
        : 0;
    const keel::usize visibleCount = (data.rowHeight > 0.0f)
        ? (static_cast<keel::usize>(keel::ceil(viewH / data.rowHeight)) + 2)
        : 32;

    data.firstVisibleRow = (rawFirst > 0) ? (rawFirst - 1) : 0;
    if (data.opts.totalRows > 0) {
        data.firstVisibleRow = keel::min(data.firstVisibleRow, data.opts.totalRows);
        data.endVisibleRow = keel::min(data.opts.totalRows, data.firstVisibleRow + visibleCount);
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
    if (m_ctx && m_visible) {
        m_ctx->PopRowDisabled(m_prevDisabled);
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
        const keel::usize totalRows = (m_data->opts.totalRows > 0)
            ? m_data->opts.totalRows
            : m_data->emittedRowCount;
        if (m_data->state) {
            m_data->state->prevTotalRows = totalRows;
        }
        m_node->scrollContentSize = Vec2(
            m_data->totalColumnsW,
            static_cast<float>(totalRows) * m_data->rowHeight
        );

        // Recompute actual childCount of m_node after virtualized row ID seeding
        keel::u32 actualChildren = 0;
        for (const Node* c = m_node->firstChild; c != nullptr; c = c->nextSibling) {
            ++actualChildren;
        }
        m_node->childCount = actualChildren;

        // Compute comprehensive visualHash from scroll offsets, column geometry,
        // visible rows, and direct/cell child widgets so dirty-tracking always
        // repaints the entire Table coherently.
        keel::u64 h = m_node->visualHash;
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
                static_cast<keel::u8>(m_data->state->sortDirection),
                m_data->hoveredRow,
                m_data->hoveredHeaderCol,
                m_data->hoveredDividerCol,
                m_data->firstVisibleRow,
                m_data->endVisibleRow,
                m_data->state->visibleRows.size()
            );
            for (keel::u16 i = 0; i < m_data->columnCount; ++i) {
                h = detail::HashArgs(h, m_data->resolvedColX[i], m_data->resolvedColW[i]);
            }
            for (const auto& rowRec : m_data->state->visibleRows) {
                h = detail::HashArgs(
                    h,
                    rowRec.rowIndex,
                    rowRec.opts.disabled,
                    rowRec.opts.tint.has_value(),
                    static_cast<keel::u8>(rowRec.opts.tint.value_or(UiVariant::Default))
                );
            }
            for (const Node* c = m_node->firstChild; c != nullptr; c = c->nextSibling) {
                h = detail::HashArgs(
                    h,
                    c->id.value,
                    c->visualHash,
                    c->hovered,
                    c->pressed,
                    c->checked,
                    c->colSpan,
                    c->childIndex
                );
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

keel::usize TableScope::FirstVisibleRow() const noexcept {
    return m_data ? m_data->firstVisibleRow : 0;
}

keel::usize TableScope::EndVisibleRow() const noexcept {
    return m_data ? m_data->endVisibleRow : 0;
}

bool TableScope::SortChanged() const noexcept {
    return m_data ? m_data->sortChanged : false;
}

keel::i32 TableScope::SortedColumn() const noexcept {
    return (m_data && m_data->state) ? m_data->state->sortColumn : -1;
}

TableSortDirection TableScope::SortDirection() const noexcept {
    return (m_data && m_data->state) ? m_data->state->sortDirection : TableSortDirection::None;
}

bool TableScope::SelectionChanged() const noexcept {
    return m_data ? m_data->selectionChanged : false;
}

keel::i32 TableScope::SelectedRow() const noexcept {
    return (m_data && m_data->state) ? m_data->state->selectedRow : -1;
}

keel::i32 TableScope::HoveredRow() const noexcept {
    return m_data ? m_data->hoveredRow : -1;
}

keel::i32 TableScope::ClickedRow() const noexcept {
    return m_data ? m_data->clickedRow : -1;
}

keel::i32 TableScope::DoubleClickedRow() const noexcept {
    return m_data ? m_data->doubleClickedRow : -1;
}

TableRowScope TableScope::Row(keel::usize rowIndex, const TableRowOptions& opts) {
    if (!m_ctx || !m_data || !m_data->state) {
        return TableRowScope{};
    }
    if (m_data->hasActiveRow) {
        EndActiveRow();
    }

    m_data->emittedRowCount = keel::max(m_data->emittedRowCount, rowIndex + 1);

    // Virtualization culling: skip rows outside [firstVisibleRow, endVisibleRow)
    if (rowIndex < m_data->firstVisibleRow || rowIndex >= m_data->endVisibleRow) {
        return TableRowScope{};
    }

    m_data->hasActiveRow = true;
    m_data->activeRowIndex = rowIndex;
    m_data->activeColCursor = 0;
    m_data->activeRowPrevLastChild = m_node ? m_node->lastChild : nullptr;
    if (m_node) {
        m_node->childCount = static_cast<keel::u32>(rowIndex * 64);
    }

    m_data->state->visibleRows.push_back(TableRowRecord{
        .rowIndex = rowIndex,
        .opts = opts
    });

    const keel::i32 iRow = static_cast<keel::i32>(rowIndex);
    const bool selected = (m_data->state->selectedRow == iRow);
    const bool hovered = (m_data->hoveredRow == iRow) && !opts.disabled;
    const bool clicked = (m_data->clickedRow == iRow) && !opts.disabled;
    const bool dblClicked = (m_data->doubleClickedRow == iRow) && !opts.disabled;

    const bool prevDisabled = m_ctx->PushRowDisabled(opts.disabled);
    return TableRowScope(this, m_ctx, rowIndex, true, selected, hovered, clicked, dblClicked, prevDisabled);
}

void TableScope::EndActiveRow() noexcept {
    if (!m_data || !m_data->hasActiveRow) return;
    if (m_node) {
        Node* startChild = m_data->activeRowPrevLastChild
            ? m_data->activeRowPrevLastChild->nextSibling
            : m_node->firstChild;
        keel::u16 colIdx = 0;
        for (Node* c = startChild; c != nullptr; c = c->nextSibling) {
            c->colSpan = static_cast<keel::u8>(keel::min<keel::u16>(colIdx++, 255));
            c->childIndex = static_cast<keel::u32>(m_data->activeRowIndex);
        }
    }
    m_data->hasActiveRow = false;
}

ContainerScope TableScope::Cell(const TableCellOptions& opts) {
    if (!m_ctx || !m_node || !m_data || !m_data->hasActiveRow) {
        return ContainerScope(nullptr, nullptr);
    }
    keel::u16 colIdx = 0;
    for (const Node* c = m_data->activeRowPrevLastChild ? m_data->activeRowPrevLastChild->nextSibling : m_node->firstChild;
         c != nullptr;
         c = c->nextSibling) {
        ++colIdx;
    }
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
    const float cellDensity = m_ctx->GetDensity();
    cellNode->padding = Padding::Symmetric(6.0f * cellDensity, 2.0f * cellDensity);
    cellNode->gap = 4.0f * cellDensity;
    cellNode->clipChildren = true;
    cellNode->colSpan = static_cast<keel::u8>(colIdx);
    cellNode->childIndex = static_cast<keel::u32>(m_data->activeRowIndex);

    m_ctx->PushContainer(cellNode);
    return ContainerScope(m_ctx, cellNode);
}

void TableScope::ScrollToRow(keel::usize rowIndex) {
    if (!m_data || !m_data->state) return;
    const float rowTop = static_cast<float>(rowIndex) * m_data->rowHeight;
    const float rowBottom = rowTop + m_data->rowHeight;
    const float viewH = keel::max(m_data->rowHeight, m_data->state->lastViewportHeight);
    if (rowTop < m_data->state->scrollOffset.y) {
        m_data->state->scrollOffset.y = rowTop;
    } else if (rowBottom > m_data->state->scrollOffset.y + viewH) {
        m_data->state->scrollOffset.y = keel::max(0.0f, rowBottom - viewH);
    }
    const keel::usize knownTotalRows = (m_data->opts.totalRows > 0)
        ? m_data->opts.totalRows
        : m_data->state->prevTotalRows;
    if (knownTotalRows > 0) {
        const float maxScrollY = keel::max(0.0f, static_cast<float>(knownTotalRows) * m_data->rowHeight - viewH);
        m_data->state->scrollOffset.y = keel::clamp(m_data->state->scrollOffset.y, 0.0f, maxScrollY);
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
    keel::ArrayRef<TableColumn> columns,
    const TableOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const TableOptions& optsCopy = opts;

    Node* node = ctx.CreateNode(NodeType::Table, id, optsCopy.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const bool activeDisabled = node->disabled;
    node->direction = FlexDirection::Column;
    node->clipChildren = true;
    node->scrollOrientation = Orientation::Both;
    node->scrollbarPolicy = optsCopy.scrollbar;
    node->uiSize = activeSize;

    node->widthMode = Control::ResolveWidthMode(ctx, optsCopy.width, SizeMode::Stretch);
    if (optsCopy.width.IsFixed()) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = optsCopy.width.Dp();
    }

    node->heightMode = Control::ResolveHeightMode(optsCopy.height, SizeMode::Stretch);
    if (optsCopy.height.IsFixed()) {
        node->fixedHeight = optsCopy.height.Dp();
    } else if (node->heightMode == SizeMode::Stretch) {
        node->flexGrow = 1.0f;
    }

    auto& state = ctx.GetOrCreatePersistentState<TableState>(id);
    state.visibleRows.clear();

    const keel::u16 colCount = static_cast<keel::u16>(keel::min(columns.size(), kMaxTableColumns));
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
    for (keel::u16 i = 0; i < data->columnCount; ++i) {
        data->columns[i] = columns[i];
        data->columns[i].title = ctx.AllocText(columns[i].title);
        data->columns[i].icon = ctx.AllocIcon(columns[i].icon);
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
        : Rect(
              0.0f,
              0.0f,
              optsCopy.width.IsFixed() ? optsCopy.width.Dp() : 640.0f,
              optsCopy.height.IsFixed() ? optsCopy.height.Dp() : 320.0f
          );

    ComputeTableRegions(node, *data, estBounds, theme);
    ResolveColumnWidths(*data, data->frameRect.w, theme);

    if (data->bodyViewportRect.h > 0.0f) {
        state.lastViewportHeight = data->bodyViewportRect.h;
    }

    const keel::usize knownTotalRows = (optsCopy.totalRows > 0) ? optsCopy.totalRows : state.prevTotalRows;
    const float estContentH = static_cast<float>(knownTotalRows) * data->rowHeight;
    const float maxScrollY = keel::max(0.0f, estContentH - state.lastViewportHeight);
    const float maxScrollX = keel::max(0.0f, data->totalColumnsW - data->bodyViewportRect.w);

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
        const bool tableHovered = ctx.IsRectHovered(id, data->frameRect);

        // 1. Mouse wheel scrolling inside table frame
        if (tableHovered) {
            const Vec2 wheel = ctx.GetMouseWheel();
            if (wheel.x != 0.0f || wheel.y != 0.0f) {
                const Vec2 prevOffset = state.scrollOffset;
                if (ctx.GetKeyModifiers().shift && wheel.x == 0.0f) {
                    state.scrollOffset.x -= wheel.y * sm.scrollStep;
                } else {
                    state.scrollOffset.y -= wheel.y * sm.scrollStep;
                    state.scrollOffset.x -= wheel.x * sm.scrollStep;
                }
                state.scrollOffset.y = keel::clamp(state.scrollOffset.y, 0.0f, maxScrollY);
                state.scrollOffset.x = keel::clamp(state.scrollOffset.x, 0.0f, maxScrollX);
                if (keel::abs(state.scrollOffset.x - prevOffset.x) > 0.01f ||
                    keel::abs(state.scrollOffset.y - prevOffset.y) > 0.01f) {
                    ctx.ConsumeMouseWheel();
                }
            }
        }
        state.scrollOffset.y = keel::clamp(state.scrollOffset.y, 0.0f, maxScrollY);
        state.scrollOffset.x = keel::clamp(state.scrollOffset.x, 0.0f, maxScrollX);

        // 2. Column resize drag or header sort click
        const float scale = 1.0f;
        const float gripHalfW = 6.0f * scale;
        const Rect headerHitRect(
            data->headerRect.x - gripHalfW,
            data->headerRect.y,
            data->headerRect.w + gripHalfW * 2.0f,
            data->headerRect.h
        );

        if (state.resizingCol >= 0) {
            ctx.SetCursor(CursorType::ResizeEW);
            if (mouseDown && state.resizingCol < static_cast<keel::i32>(data->columnCount)) {
                const keel::u16 colIdx = static_cast<keel::u16>(state.resizingCol);
                const float minW = keel::max(28.0f * scale, data->columns[colIdx].minWidth * scale);
                const float maxW = (data->columns[colIdx].maxWidth > 0.0f)
                    ? (data->columns[colIdx].maxWidth * scale)
                    : 100000.0f;
                const float newW = keel::clamp(state.resizeStartWidth + (mousePos.x - state.resizeStartX), minW, maxW);
                state.userColWidths[colIdx] = newW / scale;
                state.hasUserColWidth[colIdx] = true;
                ResolveColumnWidths(*data, data->frameRect.w, theme);
                const float newMaxScrollX = keel::max(0.0f, data->totalColumnsW - data->bodyViewportRect.w);
                state.scrollOffset.x = keel::clamp(state.scrollOffset.x, 0.0f, newMaxScrollX);
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
        } else if (!interactingScrollbar && optsCopy.stickyHeader && headerHitRect.Contains(mousePos) && !ctx.IsMouseOccluded(id, mousePos)) {
            const float localHeaderX = mousePos.x - data->headerRect.x + state.scrollOffset.x;

            // Check column divider grips first
            if (optsCopy.resizableColumns) {
                for (keel::u16 i = 0; i < data->columnCount; ++i) {
                    if (!data->columns[i].resizable) continue;
                    const float divX = data->resolvedColX[i] + data->resolvedColW[i];
                    if (keel::abs(localHeaderX - divX) <= gripHalfW) {
                        data->hoveredDividerCol = static_cast<keel::i32>(i);
                        ctx.SetCursor(CursorType::ResizeEW);
                        if (mouseClicked) {
                            const double now = (ctx.GetCurrentTime)();
                            if (state.lastDividerCol == static_cast<keel::i32>(i) && (now - state.lastDividerClickTime) <= 0.35) {
                                state.hasUserColWidth.fill(false);
                                state.resizingCol = -1;
                                ResolveColumnWidths(*data, data->frameRect.w, theme);
                                const float newMaxScrollX = keel::max(0.0f, data->totalColumnsW - data->bodyViewportRect.w);
                                state.scrollOffset.x = keel::clamp(state.scrollOffset.x, 0.0f, newMaxScrollX);
                                ctx.InvalidateRect(data->frameRect);
                                ctx.RequestRepaint(2);
                            } else {
                                // Snapshot all current resolved column widths so flex columns to the left
                                // do not counter-shrink or shift the left border of column `i` during drag!
                                for (keel::u16 j = 0; j < data->columnCount; ++j) {
                                    if (!state.hasUserColWidth[j]) {
                                        state.userColWidths[j] = data->resolvedColW[j] / scale;
                                        state.hasUserColWidth[j] = true;
                                    }
                                }
                                state.resizingCol = static_cast<keel::i32>(i);
                                state.resizeStartX = mousePos.x;
                                state.resizeStartWidth = data->resolvedColW[i];
                                ctx.SetActiveId(id);
                                ctx.InvalidateRect(data->frameRect);
                                ctx.RequestRepaint(2);
                            }
                            state.lastDividerClickTime = now;
                            state.lastDividerCol = static_cast<keel::i32>(i);
                        }
                        break;
                    }
                }
            }

            // If not hovering a resize divider, check column header hover & click-to-sort
            if (data->hoveredDividerCol < 0 && data->headerRect.Contains(mousePos)) {
                for (keel::u16 i = 0; i < data->columnCount; ++i) {
                    const float colLeft = data->resolvedColX[i];
                    const float colRight = colLeft + data->resolvedColW[i];
                    if (localHeaderX >= colLeft && localHeaderX < colRight) {
                        data->hoveredHeaderCol = static_cast<keel::i32>(i);
                        if (mouseClicked && data->columns[i].sortable) {
                            if (state.sortColumn == static_cast<keel::i32>(i)) {
                                state.sortDirection = (state.sortDirection == TableSortDirection::Ascending)
                                    ? TableSortDirection::Descending
                                    : TableSortDirection::Ascending;
                            } else {
                                state.sortColumn = static_cast<keel::i32>(i);
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
            keel::max(0.0f, data->bodyViewportRect.w - scrollbarGutterW),
            keel::max(0.0f, data->bodyViewportRect.h - scrollbarGutterH)
        );

        if (state.resizingCol < 0 && !interactingScrollbar && tableHovered && interactiveBodyRect.Contains(mousePos)) {
            const float contentY = (mousePos.y - data->bodyViewportRect.y) + state.scrollOffset.y;
            if (contentY >= 0.0f && data->rowHeight > 0.0f) {
                const keel::usize hitRow = static_cast<keel::usize>(contentY / data->rowHeight);
                if (knownTotalRows == 0 || hitRow < knownTotalRows) {
                    data->hoveredRow = static_cast<keel::i32>(hitRow);
                    if (mouseClicked) {
                        data->clickedRow = static_cast<keel::i32>(hitRow);
                        const double now = (ctx.GetCurrentTime)();
                        if (state.lastClickRow == static_cast<keel::i32>(hitRow) && (now - state.lastClickTime) <= 0.35) {
                            data->doubleClickedRow = static_cast<keel::i32>(hitRow);
                        }
                        state.lastClickTime = now;
                        state.lastClickRow = static_cast<keel::i32>(hitRow);

                        if (state.selectedRow != static_cast<keel::i32>(hitRow)) {
                            state.selectedRow = static_cast<keel::i32>(hitRow);
                            data->selectionChanged = true;
                            if (optsCopy.selectedRow) {
                                *optsCopy.selectedRow = state.selectedRow;
                            }
                            ctx.InvalidateRect(data->frameRect);
                        }
                        const Id focusedId = ctx.GetFocusedId();
                        const bool clickedFocusedChild = focusedId.IsValid() && focusedId != id &&
                            ctx.IsFocused(focusedId) &&
                            ctx.GetPrevFrameBounds(focusedId).Contains(mousePos);
                        if (!clickedFocusedChild) {
                            ctx.RequestFocus(id);
                        }
                    }
                }
            }
        }

        // 4. Keyboard navigation when table is focused or hovered
        if ((ctx.IsFocused(id) || tableHovered) && knownTotalRows > 0) {
            keel::i32 nextSel = state.selectedRow;
            if (ctx.ConsumeAction(KeyAction::MoveDown)) {
                nextSel = keel::clamp(nextSel + 1, 0, static_cast<keel::i32>(knownTotalRows) - 1);
            } else if (ctx.ConsumeAction(KeyAction::MoveUp)) {
                nextSel = keel::clamp((nextSel < 0 ? 0 : nextSel - 1), 0, static_cast<keel::i32>(knownTotalRows) - 1);
            } else if (ctx.ConsumeAction(KeyAction::MoveLineStart) || ctx.ConsumeAction(KeyAction::MoveDocStart)) {
                nextSel = 0;
            } else if (ctx.ConsumeAction(KeyAction::MoveLineEnd) || ctx.ConsumeAction(KeyAction::MoveDocEnd)) {
                nextSel = static_cast<keel::i32>(knownTotalRows) - 1;
            } else if (ctx.ConsumeAction(KeyAction::MovePageDown)) {
                const keel::i32 pageStep = keel::max(1, static_cast<keel::i32>(state.lastViewportHeight / keel::max(1.0f, data->rowHeight)));
                nextSel = keel::clamp((nextSel < 0 ? 0 : nextSel) + pageStep, 0, static_cast<keel::i32>(knownTotalRows) - 1);
            } else if (ctx.ConsumeAction(KeyAction::MovePageUp)) {
                const keel::i32 pageStep = keel::max(1, static_cast<keel::i32>(state.lastViewportHeight / keel::max(1.0f, data->rowHeight)));
                nextSel = keel::clamp((nextSel < 0 ? 0 : nextSel) - pageStep, 0, static_cast<keel::i32>(knownTotalRows) - 1);
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
                    state.scrollOffset.y = keel::max(0.0f, rBot - state.lastViewportHeight);
                }
                ctx.InvalidateRect(data->frameRect);
                ctx.RequestRepaint(2);
            }
        }

        if (keel::abs(state.scrollOffset.x - prevScroll.x) > 0.01f ||
            keel::abs(state.scrollOffset.y - prevScroll.y) > 0.01f) {
            ctx.InvalidateRect(data->frameRect);
            ctx.RequestRepaint(2);
        }
    }

    // Compute visible row range [firstVisibleRow, endVisibleRow) with 1 overscan row above/below
    UpdateVisibleRowRange(*data);
    const keel::usize visibleCount = (data->endVisibleRow >= data->firstVisibleRow)
        ? (data->endVisibleRow - data->firstVisibleRow)
        : 32;

    const keel::usize cap = keel::min<keel::usize>(visibleCount + 2, 128);
    state.visibleRows.reserve(cap);

    node->scrollOffset = state.scrollOffset;
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        optsCopy.striped,
        optsCopy.gridLines,
        optsCopy.background,
        optsCopy.border,
        optsCopy.width,
        optsCopy.height,
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
                .defaultWidthMode = SizeMode::Stretch,
                .defaultHeightMode = SizeMode::Stretch,
                .defaultLayout = ElementLayout::Top
            }
        );
    } else {
        ctx.AdaptChildToContainer(node);
        if (optsCopy.height.ResolveMode(SizeMode::Stretch) == SizeMode::Fit) {
            node->heightMode = SizeMode::Fit;
            if (!node->parent || node->parent->direction == FlexDirection::Column) {
                node->flexGrow = 0.0f;
            }
        }
    }

    ctx.FinishContainer(node);

    return TableScope(&ctx, node, data);
}

// ============================================================================
// Table::Measure & Table::Arrange & Table::GetBodyClipRect
// ============================================================================

Vec2 Table::Measure(
    const Node* node,
    float availableWidth,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const auto* data = node ? node->As<TableData>() : nullptr;
    if (!data) return {200.0f, 160.0f};
    data->lastCanvas = canvas;

    const float headerH = data->opts.stickyHeader
        ? (data->opts.headerHeight.has_value()
            ? *data->opts.headerHeight
            : ResolveDefaultHeaderHeight(node->uiSize, theme))
        : 0.0f;
    const float rowH = data->opts.rowHeight.has_value()
        ? keel::max(18.0f, *data->opts.rowHeight)
        : ResolveDefaultRowHeight(node->uiSize, theme);
    const float footerH = (data->opts.showFooter || !data->opts.footerText.empty())
        ? ResolveDefaultFooterHeight(node->uiSize, theme)
        : 0.0f;

    const keel::usize totalRows = (data->opts.totalRows > 0)
        ? data->opts.totalRows
        : data->emittedRowCount;

    float naturalW = data->opts.showRowNumbers ? 44.0f : 0.0f;
    for (keel::u16 i = 0; i < data->columnCount; ++i) {
        const float colW = (data->columns[i].width > 0.0f)
            ? data->columns[i].width
            : keel::max(80.0f, data->columns[i].minWidth);
        naturalW += colW;
    }
    if (data->opts.width.IsFixed()) {
        naturalW = data->opts.width.Dp();
    } else if (availableWidth > 0.0f && data->opts.width.ResolveMode(SizeMode::Stretch) == SizeMode::Stretch) {
        naturalW = keel::max(naturalW, availableWidth);
    }

    const keel::usize fitRows = keel::clamp<keel::usize>(totalRows, 3, 12);
    const float naturalTableH = data->opts.height.IsFixed()
        ? data->opts.height.Dp()
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

    // Position all cell children (both direct widgets and explicit NodeType::Cell containers)
    // inside the virtualized body viewport
    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
        const keel::u16 colIdx = child->colSpan;
        if (colIdx >= data->columnCount) {
            child->computedRect = Rect(0.0f, 0.0f, 0.0f, 0.0f);
            continue;
        }

        const keel::usize rowIndex = static_cast<keel::usize>(child->childIndex);
        const float cellX = data->bodyViewportRect.x + data->resolvedColX[colIdx] - node->scrollOffset.x;
        const float cellY = data->bodyViewportRect.y + static_cast<float>(rowIndex) * data->rowHeight - node->scrollOffset.y;
        const float cellW = data->resolvedColW[colIdx];
        const float cellH = data->rowHeight;

        if (child->type == NodeType::Cell) {
            child->computedRect = Rect(cellX, cellY, cellW, cellH);

            // Arrange widgets inside the cell using standard horizontal flex layout
            const Padding pad = child->padding;
            const Rect inner(
                cellX + pad.left,
                cellY + pad.top,
                keel::max(0.0f, cellW - pad.Horizontal()),
                keel::max(0.0f, cellH - pad.Vertical())
            );

            float totalChildrenW = 0.0f;
            keel::u32 numGrandChildren = 0;
            for (Node* gc = child->firstChild; gc != nullptr; gc = gc->nextSibling) {
                const UiTheme& gcTheme = gc->GetTheme(theme);
                const Vec2 m = gc->measureFn
                    ? gc->measureFn(gc, inner.w, data->lastCanvas, gcTheme)
                    : Vec2(gc->fixedWidth, gc->fixedHeight);
                gc->intrinsicSize = m;
                const float gcW = (gc->widthMode == SizeMode::Fixed && gc->fixedWidth > 0.0f)
                    ? gc->fixedWidth
                    : m.x;
                totalChildrenW += (gc->widthMode == SizeMode::Stretch) ? inner.w : gcW;
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
                const UiTheme& gcTheme = gc->GetTheme(theme);
                const float gcTargetW = (gc->widthMode == SizeMode::Fixed && gc->fixedWidth > 0.0f)
                    ? gc->fixedWidth
                    : gc->intrinsicSize.x;
                const float w = (gc->widthMode == SizeMode::Stretch)
                    ? keel::max(0.0f, inner.Right() - curX)
                    : keel::min(gcTargetW, keel::max(0.0f, inner.Right() - curX));
                const float gcTargetH = (gc->heightMode == SizeMode::Fixed && gc->fixedHeight > 0.0f)
                    ? gc->fixedHeight
                    : gc->intrinsicSize.y;
                const float h = (gc->heightMode == SizeMode::Stretch)
                    ? inner.h
                    : keel::min(gcTargetH, inner.h);
                const float y = inner.y + (inner.h - h) * 0.5f;
                gc->computedRect = Rect(curX, y, w, h);
                if (gc->arrangeFn) {
                    gc->arrangeFn(gc, gc->computedRect, gcTheme);
                }
                curX += w + child->gap;
            }
        } else {
            // Direct widget cell (Text, Element, Checkbox, ProgressBar, Button, InputField, Select, etc.)
            const UiTheme& childTheme = child->GetTheme(theme);
            const bool isPlainOrBadgeElement = (child->type == NodeType::Element && child->firstChild == nullptr);
            const bool isTextOrBadge = (child->type == NodeType::Text) || isPlainOrBadgeElement;
            const float padX = isTextOrBadge ? 8.0f : 6.0f;
            const float padY = isTextOrBadge ? 0.0f : 2.0f;
            const Rect inner(
                cellX + padX,
                cellY + padY,
                keel::max(0.0f, cellW - padX * 2.0f),
                keel::max(0.0f, cellH - padY * 2.0f)
            );

            const Vec2 m = child->measureFn
                ? child->measureFn(child, inner.w, data->lastCanvas, childTheme)
                : Vec2(child->fixedWidth, child->fixedHeight);
            child->intrinsicSize = m;

            Align effAlign = data->columns[colIdx].align;
            if (child->type == NodeType::Text && child->align != Alignment::Start) {
                effAlign = child->align;
            } else if (isPlainOrBadgeElement) {
                if (const auto* ep = child->As<ElementPayload>(); ep && ep->opts.align != Align::Start) {
                    effAlign = ep->opts.align;
                }
            }

            float w = m.x;
            if (isTextOrBadge) {
                w = keel::min(inner.w, m.x);
            } else if (child->widthMode == SizeMode::Fixed && child->fixedWidth > 0.0f) {
                w = keel::min(inner.w, child->fixedWidth);
            } else if (child->widthMode == SizeMode::Stretch) {
                w = inner.w;
            } else {
                w = keel::min(inner.w, m.x);
            }

            const float targetH = (child->heightMode == SizeMode::Fixed && child->fixedHeight > 0.0f)
                ? child->fixedHeight
                : m.y;
            const float h = (child->heightMode == SizeMode::Stretch)
                ? inner.h
                : keel::min(targetH, inner.h);
            const float x = inner.x + (inner.w - w) * AlignToFactor(effAlign);
            const float y = inner.y + (inner.h - h) * 0.5f;
            child->computedRect = Rect(x, y, w, h);
            if (child->arrangeFn) {
                child->arrangeFn(child, child->computedRect, childTheme);
            }
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

    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    ComputeTableRegions(node, *data, node->computedRect, theme);
    ResolveColumnWidths(*data, data->frameRect.w, theme);

    const float maxScrollY = keel::max(0.0f, node->scrollContentSize.y - data->bodyViewportRect.h);
    const float maxScrollX = keel::max(0.0f, node->scrollContentSize.x - data->bodyViewportRect.w);
    data->state->scrollOffset.y = keel::clamp(data->state->scrollOffset.y, 0.0f, maxScrollY);
    data->state->scrollOffset.x = keel::clamp(data->state->scrollOffset.x, 0.0f, maxScrollX);

    if (keel::abs(data->state->scrollOffset.x - node->scrollOffset.x) > 0.01f ||
        keel::abs(data->state->scrollOffset.y - node->scrollOffset.y) > 0.01f) {
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
    const float scale = 1.0f;
    const float radius = data->opts.border.radius.has_value()
        ? *data->opts.border.radius
        : theme.GetRadius(node->uiSize);

    // 1. Draw outer table surface background
    if (data->opts.background.IsBackgroundEnabled(true)) {
        const Color bgCol = theme.ResolveColor(data->opts.background, false, false, node->disabled, pal.surface);
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
            const keel::usize rIdx = rowRec.rowIndex;
            const float rowY = data->bodyViewportRect.y + static_cast<float>(rIdx) * data->rowHeight - node->scrollOffset.y;
            const Rect rowRect(data->bodyViewportRect.x, rowY, data->bodyViewportRect.w, data->rowHeight);

            if (!rowRect.Intersects(data->bodyViewportRect)) continue;

            const keel::i32 iRow = static_cast<keel::i32>(rIdx);
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
                char numBuf[24]{};
                const keel::StringRef numStr = keel::format_buf(numBuf, "%zu", rIdx + 1);
                const Rect numRect(
                    data->bodyViewportRect.x - node->scrollOffset.x + cellPadX * 0.5f,
                    rowY,
                    keel::max(0.0f, data->rowNumberColW - cellPadX),
                    data->rowHeight
                );
                canvas.DrawTextBox(
                    numStr,
                    numRect,
                    bodyFontSize * 0.9f,
                    pal.textMuted,
                    1.0f,
                    0.5f
                );
            }

            if (data->opts.gridLines) {
                const float lineY = keel::floor(rowRect.Bottom()) - 0.5f;
                canvas.DrawLine(
                    Vec2(rowRect.x, lineY),
                    Vec2(rowRect.Right(), lineY),
                    pal.borderSubtle.WithAlpha(95),
                    1.0f
                );
            }
        }

        // Draw vertical column grid lines across the body viewport
        if (data->opts.gridLines) {
            if (data->opts.showRowNumbers && data->rowNumberColW > 0.0f) {
                const float divX = keel::floor(data->bodyViewportRect.x + data->rowNumberColW - node->scrollOffset.x) + 0.5f;
                if (divX >= data->bodyViewportRect.x && divX <= data->bodyViewportRect.Right()) {
                    canvas.DrawLine(
                        Vec2(divX, data->bodyViewportRect.y),
                        Vec2(divX, data->bodyViewportRect.Bottom()),
                        pal.borderSubtle.WithAlpha(95),
                        1.0f
                    );
                }
            }
            for (keel::u16 i = 0; i < data->columnCount; ++i) {
                const float divX = keel::floor(
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
        const keel::i32 activeDivCol = (data->state->resizingCol >= 0)
            ? data->state->resizingCol
            : data->hoveredDividerCol;
        if (activeDivCol >= 0 && static_cast<keel::u16>(activeDivCol) < data->columnCount) {
            const keel::u16 colIdx = static_cast<keel::u16>(activeDivCol);
            const float guideX = keel::clamp(
                keel::floor(data->bodyViewportRect.x + data->resolvedColX[colIdx] + data->resolvedColW[colIdx] - node->scrollOffset.x) + 0.5f,
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
    const float scale = 1.0f;
    const float radius = data->opts.border.radius.has_value()
        ? *data->opts.border.radius
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
                keel::max(0.0f, data->rowNumberColW - cellPadX),
                data->headerRect.h
            );
            canvas.DrawTextBox("#", numHdrRect, headerFontSize, pal.textMuted, 1.0f, 0.5f);
        }

        for (keel::u16 i = 0; i < data->columnCount; ++i) {
            const auto& col = data->columns[i];
            const float colX = data->headerRect.x + data->resolvedColX[i] - node->scrollOffset.x;
            const float colW = data->resolvedColW[i];
            const Rect colHdrRect(colX, data->headerRect.y, colW, data->headerRect.h);

            if (!colHdrRect.Intersects(data->headerRect)) continue;

            const bool isHovered = (data->hoveredHeaderCol == static_cast<keel::i32>(i)) && col.sortable;
            const bool isSorted = (data->state->sortColumn == static_cast<keel::i32>(i)) &&
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
                keel::max(0.0f, colW - cellPadX * 2.0f - sortArrowReserve),
                data->headerRect.h
            );

            const Color titleCol = isSorted ? pal.accent : pal.textSecondary;
            if (col.icon.IsValid()) {
                const float iconSz = 13.0f * scale;
                const Rect iconRect(titleRect.x, data->headerRect.y + (data->headerRect.h - iconSz) * 0.5f, iconSz, iconSz);
                col.icon.Draw(canvas, iconRect, titleCol);
                titleRect.x += iconSz + 5.0f * scale;
                titleRect.w = keel::max(0.0f, titleRect.w - (iconSz + 5.0f * scale));
            }

            if (!col.title.empty() && titleRect.w > 2.0f) {
                const auto truncated = TextUtil::TruncateWithEllipsis(
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
            const float divX = keel::min(keel::floor(colX + colW) + 0.5f, data->headerRect.Right() - 1.5f);
            const bool dividerActive = (data->state->resizingCol == static_cast<keel::i32>(i)) ||
                                       (data->hoveredDividerCol == static_cast<keel::i32>(i));
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
        const float hdrBotY = keel::floor(data->headerRect.Bottom()) - 0.5f;
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
        const float topY = keel::floor(data->footerRect.y) + 0.5f;
        canvas.DrawLine(
            Vec2(data->footerRect.x, topY),
            Vec2(data->footerRect.Right(), topY),
            pal.borderSubtle,
            1.0f
        );

        const float footerFontSize = ResolveTableFontSize(node->uiSize, theme, true);
        const keel::usize totalRows = (data->opts.totalRows > 0)
            ? data->opts.totalRows
            : data->emittedRowCount;

        char summaryBuf[96]{};
        keel::StringRef summaryStr{"0 rows"};
        if (totalRows > 0) {
            const keel::usize dispFirst = keel::min(totalRows, data->firstVisibleRow + 1);
            const keel::usize dispLast = keel::min(totalRows, data->endVisibleRow);
            if (data->state->selectedRow >= 0) {
                summaryStr = keel::format_buf(
                    summaryBuf,
                    "Rows %zu..%zu of %zu  |  Selected: #%d",
                    dispFirst,
                    dispLast,
                    totalRows,
                    data->state->selectedRow + 1
                );
            } else {
                summaryStr = keel::format_buf(
                    summaryBuf,
                    "Rows %zu..%zu of %zu",
                    dispFirst,
                    dispLast,
                    totalRows
                );
            }
        }

        const Rect leftRect(
            data->footerRect.x + cellPadX,
            data->footerRect.y,
            keel::max(0.0f, data->footerRect.w * 0.6f - cellPadX),
            data->footerRect.h
        );
        canvas.DrawTextBox(summaryStr, leftRect, footerFontSize, pal.textMuted, 0.0f, 0.5f);

        if (!data->opts.footerText.empty()) {
            const Rect rightRect(
                data->footerRect.x + data->footerRect.w * 0.4f,
                data->footerRect.y,
                keel::max(0.0f, data->footerRect.w * 0.6f - cellPadX),
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
    const float bw = (data->opts.border.width > 0.0f) ? data->opts.border.width : 0.0f;
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
