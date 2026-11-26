#include "guikit/views/table.hpp"
#include "guikit/actions/button.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/element.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/scrollbar.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/layout/scroll_area.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/format.h"
#include "keel/math.h"

namespace guikit {

namespace {

struct TableRowRecord {
    keel::usize rowIndex{0};
    TableRowOptions opts{};
};

struct TableCellBinding {
    Node* child{nullptr};
    keel::usize rowIndex{0};
    keel::usize colIndex{0};
};

struct TableState {
    Vec2 scrollOffset{0.0f, 0.0f};
    bool draggingScrollbarV{false};
    bool draggingScrollbarH{false};
    keel::f32 dragStartOffsetV{0.0f};
    keel::f32 dragStartOffsetH{0.0f};
    Vec2 dragStartMouseV{0.0f, 0.0f};
    Vec2 dragStartMouseH{0.0f, 0.0f};
    keel::u8 pressedButtonV{0};
    keel::u8 pressedButtonH{0};
    keel::f64 nextRepeatSecV{0.0};
    keel::f64 nextRepeatSecH{0.0};

    keel::usize userColCount{0};
    keel::Array<keel::f32> userColWidths{};
    keel::Array<keel::u8> hasUserColWidth{};
    keel::i32 resizingCol{-1};
    keel::f32 resizeStartX{0.0f};
    keel::f32 resizeStartWidth{0.0f};

    keel::i32 selectedRow{-1};
    keel::i32 selectionAnchor{-1};
    keel::Array<keel::usize> selectedRows{};
    keel::i32 sortColumn{-1};
    TableSortDirection sortDirection{TableSortDirection::None};

    keel::f64 lastClickTime{-10.0};
    keel::i32 lastClickRow{-1};
    keel::f64 lastDividerClickTime{-10.0};
    keel::i32 lastDividerCol{-1};

    keel::usize prevTotalRows{0};
    keel::f32 lastViewportHeight{240.0f};
    Rect lastArrangedBounds{};

    // Persistent frame buffers (cleared each frame, zero heap allocation after warmup)
    keel::Array<TableRowRecord> visibleRows{};
    keel::Array<TableCellBinding> cellBindings{};
};

keel::f32 ResolveDefaultRowHeight(UiSize size, const UiTheme& theme, keel::f32 density) noexcept {
    keel::f32 base = 32.0f;
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs: base = 24.0f; break;
        case UiSize::Preset::Sm: base = 28.0f; break;
        case UiSize::Preset::Lg: base = 38.0f; break;
        case UiSize::Preset::Xl: base = 44.0f; break;
        case UiSize::Preset::Md:
        case UiSize::Preset::None:
            base = 32.0f;
            break;
        default:
            base = 32.0f * theme.SizeFactor(size);
            break;
    }
    return keel::max(18.0f, keel::round(base * density));
}

keel::f32 ResolveDefaultHeaderHeight(UiSize size, const UiTheme& theme, keel::f32 density) noexcept {
    keel::f32 base = 34.0f;
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs: base = 26.0f; break;
        case UiSize::Preset::Sm: base = 30.0f; break;
        case UiSize::Preset::Lg: base = 40.0f; break;
        case UiSize::Preset::Xl: base = 46.0f; break;
        case UiSize::Preset::Md:
        case UiSize::Preset::None:
            base = 34.0f;
            break;
        default:
            base = 34.0f * theme.SizeFactor(size);
            break;
    }
    return keel::max(20.0f, keel::round(base * density));
}

keel::f32 ResolveDefaultFooterHeight(UiSize size, const UiTheme& theme, keel::f32 density) noexcept {
    keel::f32 base = 26.0f;
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs: base = 22.0f; break;
        case UiSize::Preset::Sm: base = 24.0f; break;
        case UiSize::Preset::Lg: base = 30.0f; break;
        case UiSize::Preset::Xl: base = 34.0f; break;
        case UiSize::Preset::Md:
        case UiSize::Preset::None:
            base = 26.0f;
            break;
        default:
            base = 26.0f * theme.SizeFactor(size);
            break;
    }
    return keel::max(18.0f, keel::round(base * density));
}

keel::f32 ResolveTableFontSize(const UiText& text, TextRole defaultRole, const UiTheme& theme) noexcept {
    return theme.ResolveFontSize(text, defaultRole, UiSize::Md);
}

keel::f32 AlignToFactor(Align align) noexcept {
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

    keel::usize columnCount{0};
    keel::usize frozenColCount{0};
    TableColumn* columns{nullptr};
    keel::f32* resolvedColX{nullptr};
    keel::f32* resolvedColW{nullptr};
    keel::f32 rowNumberColW{0.0f};
    keel::f32 frozenAreaW{0.0f};
    keel::f32 totalColumnsW{0.0f};
    keel::f32 scrollbarGutterW{0.0f};
    keel::f32 scrollbarGutterH{0.0f};

    keel::f32 rowHeight{32.0f};
    keel::f32 headerHeight{34.0f};
    keel::f32 footerHeight{0.0f};
    keel::f32 bevelInset{0.0f};

    keel::usize firstVisibleRow{0};
    keel::usize endVisibleRow{0};
    keel::usize firstVisibleCol{0};
    keel::usize endVisibleCol{0};
    keel::usize emittedRowCount{0};

    bool hasActiveRow{false};
    bool activeRowSelected{false};
    bool activeRowDisabled{false};
    keel::usize activeRowIndex{0};
    keel::usize activeColCursor{0};
    keel::usize activeRowBindingStart{0};
    Node* activeRowLastScannedChild{nullptr};
    Node* scrollPane{nullptr};
    Node* frozenPane{nullptr};
    const Zone* selectedTextZone{nullptr};
    mutable ICanvas* lastCanvas{nullptr};

    keel::i32 hoveredHeaderCol{-1};
    keel::i32 pressedHeaderCol{-1};
    keel::i32 hoveredDividerCol{-1};
    keel::i32 hoveredRow{-1};
    keel::i32 clickedRow{-1};
    keel::i32 doubleClickedRow{-1};
    bool sortChanged{false};
    bool selectionChanged{false};

    Rect frameRect{};
    Rect innerFrameRect{};
    Rect headerRect{};
    Rect bodyViewportRect{};
    Rect footerRect{};
};

namespace {

void ApplySelectedRowZone(Node* n, const Zone* selZone) noexcept {
    if (!n || !selZone) return;
    if (n->type == NodeType::Cell) {
        for (Node* gc = n->firstChild; gc != nullptr; gc = gc->nextSibling) {
            ApplySelectedRowZone(gc, selZone);
        }
        return;
    }
    if (n->type == NodeType::Text ||
        n->type == NodeType::Heading ||
        n->type == NodeType::Readout ||
        detail::Button::IsGhostNode(n)) {
        n->zone = selZone;
    }
}

void SyncActiveRowCells(Node* tableNode, TableData& data) noexcept {
    if (!tableNode || !data.state || !data.hasActiveRow) return;
    Node* cur = data.activeRowLastScannedChild
        ? data.activeRowLastScannedChild->nextSibling
        : tableNode->firstChild;
    while (cur != nullptr) {
        data.state->cellBindings.push_back(TableCellBinding{
            .child = cur,
            .rowIndex = data.activeRowIndex,
            .colIndex = data.activeColCursor++
        });
        data.activeRowLastScannedChild = cur;
        cur = cur->nextSibling;
    }
}

bool HasSelectedRow(const keel::Array<keel::usize>& rows, keel::usize target) noexcept {
    keel::usize lo = 0;
    keel::usize hi = rows.size();
    while (lo < hi) {
        const keel::usize mid = lo + (hi - lo) / 2;
        if (rows[mid] < target) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo < rows.size() && rows[lo] == target;
}

void InsertSelectedRow(keel::Array<keel::usize>& rows, keel::usize target) {
    keel::usize lo = 0;
    keel::usize hi = rows.size();
    while (lo < hi) {
        const keel::usize mid = lo + (hi - lo) / 2;
        if (rows[mid] < target) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < rows.size() && rows[lo] == target) return;
    rows.push_back(target);
    for (keel::usize i = rows.size() - 1; i > lo; --i) {
        rows[i] = rows[i - 1];
    }
    rows[lo] = target;
}

void RemoveSelectedRow(keel::Array<keel::usize>& rows, keel::usize target) noexcept {
    keel::usize lo = 0;
    keel::usize hi = rows.size();
    while (lo < hi) {
        const keel::usize mid = lo + (hi - lo) / 2;
        if (rows[mid] < target) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo >= rows.size() || rows[lo] != target) return;
    for (keel::usize i = lo + 1; i < rows.size(); ++i) {
        rows[i - 1] = rows[i];
    }
    rows.pop_back();
}

void SetSingleSelection(TableState& st, keel::usize row) {
    st.selectedRows.clear();
    st.selectedRows.push_back(row);
    st.selectedRow = keel::as<keel::i32>(row);
    st.selectionAnchor = st.selectedRow;
}

void SetRangeSelection(TableState& st, keel::usize a, keel::usize b, bool additive) {
    const keel::usize first = keel::min(a, b);
    const keel::usize last = keel::max(a, b);
    if (!additive) {
        st.selectedRows.clear();
        st.selectedRows.reserve(last - first + 1);
        for (keel::usize r = first; r <= last; ++r) {
            st.selectedRows.push_back(r);
        }
    } else {
        for (keel::usize r = first; r <= last; ++r) {
            InsertSelectedRow(st.selectedRows, r);
        }
    }
}

void SyncExternalSelectedRowsIn(TableState& st, const IndexListSink& sink) {
    st.selectedRows.clear();
    const keel::ArrayRef<keel::usize> src = sink.as_ref();
    st.selectedRows.reserve(src.size());
    for (keel::usize i = 0; i < src.size(); ++i) {
        InsertSelectedRow(st.selectedRows, src[i]);
    }
}

void SyncExternalSelectedRowsOut(const TableState& st, const IndexListSink& sink) {
    if (!sink.has_target()) return;
    sink.clear();
    for (keel::usize i = 0; i < st.selectedRows.size(); ++i) {
        sink.push_back(st.selectedRows[i]);
    }
}

void UpdateVisibleColRange(TableData& data) noexcept {
    if (data.columnCount == 0 || !data.resolvedColX || !data.resolvedColW) {
        data.firstVisibleCol = 0;
        data.endVisibleCol = 0;
        return;
    }
    const keel::usize scrollStart = keel::min(data.frozenColCount, data.columnCount);
    if (scrollStart >= data.columnCount) {
        data.firstVisibleCol = scrollStart;
        data.endVisibleCol = scrollStart;
        return;
    }
    const keel::f32 scrollX = data.state ? data.state->scrollOffset.x : 0.0f;
    const keel::f32 viewLeft = data.frozenAreaW + scrollX;
    const keel::f32 viewRight = scrollX + keel::max(0.0f, data.bodyViewportRect.w);

    keel::usize lo = scrollStart;
    keel::usize hi = data.columnCount;
    while (lo < hi) {
        const keel::usize mid = lo + (hi - lo) / 2;
        if (data.resolvedColX[mid] + data.resolvedColW[mid] <= viewLeft) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    data.firstVisibleCol = lo;

    keel::usize endLo = lo;
    keel::usize endHi = data.columnCount;
    while (endLo < endHi) {
        const keel::usize mid = endLo + (endHi - endLo) / 2;
        if (data.resolvedColX[mid] < viewRight) {
            endLo = mid + 1;
        } else {
            endHi = mid;
        }
    }
    data.endVisibleCol = endLo;
}

struct TableScrollLayout {
    bool showV{false};
    bool showH{false};
    keel::f32 gutterW{0.0f};
    keel::f32 gutterH{0.0f};
    keel::f32 visibleW{0.0f};
    keel::f32 visibleH{0.0f};
    keel::f32 contentW{0.0f};
    keel::f32 contentH{0.0f};
    keel::f32 maxScrollX{0.0f};
    keel::f32 maxScrollY{0.0f};
    keel::f32 cornerReserve{0.0f};
};

void ResolveColumnWidths(TableData& data, keel::f32 viewportW, const UiTheme& /*theme*/) noexcept;

TableScrollLayout ResolveTableScrollLayout(
    const Node* node,
    TableData& data,
    keel::usize knownTotalRows,
    const UiTheme& theme
) noexcept {
    const VisibilityPolicy sbPol = (node && node->scroll) ? node->scroll->policy : data.opts.scrollbar;
    const ScrollbarMetrics& sm = theme.GetScrollbar(node ? node->uiSize : data.opts.size);
    const keel::f32 gutter = sm.trackWidth + sm.margin * 2.0f;
    const keel::f32 contentH = keel::as<keel::f32>(knownTotalRows) * data.rowHeight;

    // Pass 1: estimate vertical scrollbar, resolve column widths, then check horizontal scrollbar
    bool showV = (sbPol == VisibilityPolicy::Always) ||
        (sbPol == VisibilityPolicy::Auto && contentH > data.bodyViewportRect.h + 0.5f);
    keel::f32 gutterW = showV ? gutter : 0.0f;
    keel::f32 visibleW = keel::max(0.0f, data.bodyViewportRect.w - gutterW);
    ResolveColumnWidths(data, visibleW, theme);

    bool showH = (sbPol == VisibilityPolicy::Always) ||
        (sbPol == VisibilityPolicy::Auto && data.totalColumnsW > visibleW + 0.5f);
    keel::f32 gutterH = showH ? gutter : 0.0f;

    // Pass 2: if horizontal scrollbar reduced usable height enough to trigger vertical scroll, re-resolve once
    if (showH && !showV && sbPol == VisibilityPolicy::Auto &&
        contentH > keel::max(0.0f, data.bodyViewportRect.h - gutterH) + 0.5f) {
        showV = true;
        gutterW = gutter;
        visibleW = keel::max(0.0f, data.bodyViewportRect.w - gutterW);
        ResolveColumnWidths(data, visibleW, theme);
    }

    const keel::f32 visibleH = keel::max(0.0f, data.bodyViewportRect.h - gutterH);
    data.scrollbarGutterW = gutterW;
    data.scrollbarGutterH = gutterH;

    return TableScrollLayout{
        .showV = showV,
        .showH = showH,
        .gutterW = gutterW,
        .gutterH = gutterH,
        .visibleW = visibleW,
        .visibleH = visibleH,
        .contentW = data.totalColumnsW,
        .contentH = contentH,
        .maxScrollX = keel::max(0.0f, data.totalColumnsW - visibleW),
        .maxScrollY = keel::max(0.0f, contentH - visibleH),
        .cornerReserve = (showV && showH) ? (sm.trackWidth + sm.margin) : 0.0f
    };
}

void ResolveColumnWidths(TableData& data, keel::f32 viewportW, const UiTheme& /*theme*/) noexcept {
    data.rowNumberColW = data.opts.showRowNumbers ? 44.0f : 0.0f;
    if (data.columnCount == 0 || !data.columns || !data.resolvedColX || !data.resolvedColW) {
        data.totalColumnsW = data.rowNumberColW;
        UpdateVisibleColRange(data);
        return;
    }

    const keel::f32 availForCols = keel::max(0.0f, viewportW - data.rowNumberColW);

    keel::f32 fixedSum = 0.0f;
    keel::f32 flexSum = 0.0f;

    for (keel::usize i = 0; i < data.columnCount; ++i) {
        const auto& col = data.columns[i];
        const keel::f32 minW = keel::max(28.0f, col.minWidth);
        const keel::f32 maxW = (col.maxWidth > 0.0f) ? col.maxWidth : 100000.0f;

        if (data.state && i < data.state->hasUserColWidth.size() && data.state->hasUserColWidth[i] != 0u) {
            const keel::f32 w = keel::clamp(data.state->userColWidths[i], minW, maxW);
            data.resolvedColW[i] = w;
            fixedSum += w;
        } else if (col.width > 0.0f) {
            const keel::f32 w = keel::clamp(col.width, minW, maxW);
            data.resolvedColW[i] = w;
            fixedSum += w;
        } else {
            data.resolvedColW[i] = 0.0f;
            flexSum += keel::max(0.1f, col.flex);
        }
    }

    const keel::f32 remainingW = keel::max(0.0f, availForCols - fixedSum);
    keel::f32 curX = data.rowNumberColW;

    for (keel::usize i = 0; i < data.columnCount; ++i) {
        const auto& col = data.columns[i];
        const keel::f32 minW = keel::max(28.0f, col.minWidth);
        const keel::f32 maxW = (col.maxWidth > 0.0f) ? col.maxWidth : 100000.0f;

        if (data.resolvedColW[i] <= 0.0f) {
            const keel::f32 weight = keel::max(0.1f, col.flex);
            const keel::f32 flexShare = (flexSum > 0.0f) ? (remainingW * (weight / flexSum)) : minW;
            data.resolvedColW[i] = keel::clamp(keel::round(flexShare), minW, maxW);
        }

        // Stretch last column if auto-flex columns slightly undershoot viewport due to rounding
        const bool hasUserW = data.state && i < data.state->hasUserColWidth.size() && data.state->hasUserColWidth[i] != 0u;
        if (i + 1 == data.columnCount && flexSum > 0.0f && !hasUserW && curX + data.resolvedColW[i] < viewportW) {
            data.resolvedColW[i] = keel::min(maxW, viewportW - curX);
        }

        data.resolvedColX[i] = curX;
        curX += data.resolvedColW[i];
    }

    data.totalColumnsW = curX;
    data.frozenAreaW = (data.frozenColCount > 0 && data.frozenColCount <= data.columnCount)
        ? (data.resolvedColX[data.frozenColCount - 1] + data.resolvedColW[data.frozenColCount - 1])
        : 0.0f;
    UpdateVisibleColRange(data);
}

void ComputeTableRegions(
    const Node* node,
    TableData& data,
    const Rect& outerBounds,
    const UiTheme& theme
) noexcept {
    data.frameRect = outerBounds;
    const keel::f32 inset = (data.opts.border.width > 0.0f)
        ? theme.Painter().GetChromeMetrics(theme).frameBevelInset
        : 0.0f;
    data.bevelInset = inset;
    data.innerFrameRect = (inset > 0.0f)
        ? Rect(
              outerBounds.x + inset,
              outerBounds.y + inset,
              keel::max(0.0f, outerBounds.w - inset * 2.0f),
              keel::max(0.0f, outerBounds.h - inset * 2.0f)
          )
        : outerBounds;

    const keel::f32 density = (node && node->zone) ? node->zone->density : 1.0f;
    data.headerHeight = data.opts.stickyHeader
        ? (data.opts.headerHeight.has_value()
            ? *data.opts.headerHeight
            : ResolveDefaultHeaderHeight(node->uiSize, theme, density))
        : 0.0f;
    data.rowHeight = data.opts.rowHeight.has_value()
        ? keel::max(18.0f, *data.opts.rowHeight)
        : ResolveDefaultRowHeight(node->uiSize, theme, density);
    data.footerHeight = (data.opts.showFooter || !data.opts.footerText.empty())
        ? ResolveDefaultFooterHeight(node->uiSize, theme, density)
        : 0.0f;

    const keel::f32 headerH = keel::min(data.headerHeight, data.innerFrameRect.h);
    const keel::f32 footerH = keel::min(data.footerHeight, keel::max(0.0f, data.innerFrameRect.h - headerH));
    const keel::f32 bodyH = keel::max(0.0f, data.innerFrameRect.h - headerH - footerH);

    data.headerRect = Rect(data.innerFrameRect.x, data.innerFrameRect.y, data.innerFrameRect.w, headerH);
    data.bodyViewportRect = Rect(data.innerFrameRect.x, data.innerFrameRect.y + headerH, data.innerFrameRect.w, bodyH);
    data.footerRect = Rect(data.innerFrameRect.x, data.innerFrameRect.Bottom() - footerH, data.innerFrameRect.w, footerH);
}

void UpdateVisibleRowRange(TableData& data) noexcept {
    if (!data.state) return;
    const keel::f32 viewH = keel::max(data.rowHeight, data.state->lastViewportHeight);
    const keel::usize rawFirst = (data.rowHeight > 0.0f)
        ? keel::as<keel::usize>(keel::max(0.0f, keel::floor(data.state->scrollOffset.y / data.rowHeight)))
        : 0;
    const keel::usize visibleCount = (data.rowHeight > 0.0f)
        ? (keel::as<keel::usize>(keel::ceil(viewH / data.rowHeight)) + 2)
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
        m_ctx->PopId();
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
        if (m_node->scroll) {
            m_node->scroll->contentSize = Vec2(
                m_data->totalColumnsW,
                keel::as<keel::f32>(totalRows) * m_data->rowHeight
            );
        }

        // Compute comprehensive visualHash from scroll offsets, column geometry,
        // visible rows, and bound cell children so dirty-tracking always
        // repaints the entire Table coherently.
        keel::u64 h = m_node->visualHash;
        if (m_data->state) {
            const ScrollData* sd = m_node->scroll;
            h = detail::HashArgs(
                h,
                m_data->state->scrollOffset.x,
                m_data->state->scrollOffset.y,
                m_data->state->resizingCol,
                m_data->state->draggingScrollbarV,
                m_data->state->draggingScrollbarH,
                sd ? sd->hoveredY : false,
                sd ? sd->activeY : false,
                sd ? sd->hoveredX : false,
                sd ? sd->activeX : false,
                m_data->state->selectedRow,
                m_data->state->selectedRows.size(),
                m_data->state->sortColumn,
                keel::as<keel::u8>(m_data->state->sortDirection),
                m_data->hoveredRow,
                m_data->hoveredHeaderCol,
                m_data->pressedHeaderCol,
                m_data->hoveredDividerCol,
                m_data->firstVisibleRow,
                m_data->endVisibleRow,
                m_data->firstVisibleCol,
                m_data->endVisibleCol,
                m_data->frozenColCount,
                m_data->state->visibleRows.size()
            );
            for (keel::usize i = 0; i < m_data->state->selectedRows.size(); ++i) {
                h = detail::HashArgs(h, m_data->state->selectedRows[i]);
            }
            for (keel::usize i = 0; i < m_data->columnCount; ++i) {
                h = detail::HashArgs(h, m_data->resolvedColX[i], m_data->resolvedColW[i]);
            }
            for (const auto& rowRec : m_data->state->visibleRows) {
                h = detail::HashArgs(
                    h,
                    rowRec.rowIndex,
                    rowRec.opts.disabled,
                    rowRec.opts.tint.has_value(),
                    keel::as<keel::u8>(rowRec.opts.tint.value_or(UiVariant::Default))
                );
            }
            for (const auto& binding : m_data->state->cellBindings) {
                const Node* c = binding.child;
                if (!c) continue;
                h = detail::HashArgs(
                    h,
                    c->id.value,
                    c->visualHash,
                    c->hovered,
                    c->pressed,
                    c->checked,
                    binding.rowIndex,
                    binding.colIndex
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

        // When frozenColumns > 0, partition bound cell children into two sibling clip panes:
        // scrollPane (rendered first, clipped to [frozenW .. right]) and frozenPane (rendered on top,
        // clipped to [0 .. frozenW]) so horizontally scrolled widgets never bleed over pinned columns.
        if (m_data->frozenColCount > 0 && m_data->state && !m_data->state->cellBindings.empty()) {
            m_node->firstChild = nullptr;
            m_node->lastChild = nullptr;
            m_node->childCount = 0;

            const Id scrollPaneId(detail::HashArgs(m_node->id.value, keel::u64(0x5343524FULL)));
            const Id frozenPaneId(detail::HashArgs(m_node->id.value, keel::u64(0x46525A4EULL)));
            Node* scrollPane = m_ctx->CreateNode(NodeType::Box, scrollPaneId, m_node->disabled);
            Node* frozenPane = m_ctx->CreateNode(NodeType::Box, frozenPaneId, m_node->disabled);
            scrollPane->clipChildren = true;
            frozenPane->clipChildren = true;
            scrollPane->renderFn = nullptr;
            frozenPane->renderFn = nullptr;

            auto appendChild = [](Node* parent, Node* child) noexcept {
                child->parent = parent;
                child->nextSibling = nullptr;
                if (!parent->firstChild) {
                    parent->firstChild = child;
                    parent->lastChild = child;
                } else {
                    parent->lastChild->nextSibling = child;
                    parent->lastChild = child;
                }
                ++parent->childCount;
            };

            for (const auto& binding : m_data->state->cellBindings) {
                Node* child = binding.child;
                if (!child) continue;
                if (binding.colIndex < m_data->frozenColCount) {
                    appendChild(frozenPane, child);
                } else {
                    appendChild(scrollPane, child);
                }
            }

            m_data->scrollPane = scrollPane;
            m_data->frozenPane = frozenPane;
        }
    }

    m_ctx->PopContainer(m_node);
}

keel::usize TableScope::FirstVisibleRow() const noexcept {
    return m_data ? m_data->firstVisibleRow : 0;
}

keel::usize TableScope::EndVisibleRow() const noexcept {
    return m_data ? m_data->endVisibleRow : 0;
}

keel::usize TableScope::FirstVisibleColumn() const noexcept {
    return m_data ? m_data->firstVisibleCol : 0;
}

keel::usize TableScope::EndVisibleColumn() const noexcept {
    return m_data ? m_data->endVisibleCol : 0;
}

keel::usize TableScope::FrozenColumnCount() const noexcept {
    return m_data ? m_data->frozenColCount : 0;
}

keel::usize TableScope::ColumnCount() const noexcept {
    return m_data ? m_data->columnCount : 0;
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

bool TableScope::IsRowSelected(keel::usize rowIndex) const noexcept {
    if (!m_data || !m_data->state) return false;
    return HasSelectedRow(m_data->state->selectedRows, rowIndex);
}

keel::usize TableScope::SelectedRowCount() const noexcept {
    return (m_data && m_data->state) ? m_data->state->selectedRows.size() : 0;
}

keel::ArrayRef<keel::usize> TableScope::SelectedRows() const noexcept {
    if (!m_data || !m_data->state || m_data->state->selectedRows.empty()) return {};
    return keel::ArrayRef<keel::usize>(m_data->state->selectedRows.data(), m_data->state->selectedRows.size());
}

void TableScope::ClearSelection() noexcept {
    if (!m_data || !m_data->state) return;
    if (m_data->state->selectedRow < 0 && m_data->state->selectedRows.empty()) return;
    m_data->state->selectedRow = -1;
    m_data->state->selectionAnchor = -1;
    m_data->state->selectedRows.clear();
    m_data->selectionChanged = true;
    if (m_data->opts.selectedRow) {
        *m_data->opts.selectedRow = -1;
    }
    SyncExternalSelectedRowsOut(*m_data->state, m_data->opts.selectedRows);
    if (m_ctx) {
        m_ctx->InvalidateRect(m_data->frameRect);
    }
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
    m_data->activeRowBindingStart = m_data->state->cellBindings.size();
    m_data->activeRowLastScannedChild = m_node ? m_node->lastChild : nullptr;

    m_data->state->visibleRows.push_back(TableRowRecord{
        .rowIndex = rowIndex,
        .opts = opts
    });

    const keel::i32 iRow = keel::as<keel::i32>(rowIndex);
    const bool selected = HasSelectedRow(m_data->state->selectedRows, rowIndex);
    const bool hovered = (m_data->hoveredRow == iRow) && !opts.disabled;
    const bool clicked = (m_data->clickedRow == iRow) && !opts.disabled;
    const bool dblClicked = (m_data->doubleClickedRow == iRow) && !opts.disabled;

    m_data->activeRowSelected = selected;
    m_data->activeRowDisabled = opts.disabled;

    m_ctx->PushId(keel::as<keel::u64>(rowIndex));
    const bool prevDisabled = m_ctx->PushRowDisabled(opts.disabled);

    return TableRowScope(this, m_ctx, rowIndex, true, selected, hovered, clicked, dblClicked, prevDisabled);
}

void TableScope::EndActiveRow() noexcept {
    if (!m_data || !m_data->hasActiveRow) return;
    SyncActiveRowCells(m_node, *m_data);
    if (m_data->activeRowSelected && !m_data->activeRowDisabled && m_data->selectedTextZone && m_data->state) {
        for (keel::usize i = m_data->activeRowBindingStart; i < m_data->state->cellBindings.size(); ++i) {
            ApplySelectedRowZone(m_data->state->cellBindings[i].child, m_data->selectedTextZone);
        }
    }
    m_data->hasActiveRow = false;
}

ContainerScope TableScope::Cell(const TableCellOptions& opts) {
    if (!m_ctx || !m_node || !m_data || !m_data->state || !m_data->hasActiveRow) {
        return ContainerScope(nullptr, nullptr);
    }
    SyncActiveRowCells(m_node, *m_data);
    return CellAt(m_data->activeColCursor, opts);
}

ContainerScope TableScope::CellAt(keel::usize colIndex, const TableCellOptions& opts) {
    if (!m_ctx || !m_node || !m_data || !m_data->state || !m_data->hasActiveRow) {
        return ContainerScope(nullptr, nullptr);
    }
    SyncActiveRowCells(m_node, *m_data);
    if (colIndex >= m_data->columnCount) {
        return ContainerScope(nullptr, nullptr);
    }

    const Id cellId(detail::HashArgs(m_node->id.value, m_data->activeRowIndex, colIndex));
    Node* cellNode = m_ctx->CreateNode(NodeType::Cell, cellId);
    cellNode->direction = FlexDirection::Row;
    cellNode->align = Alignment::Center;
    const Align effectiveAlign = opts.align.value_or(m_data->columns[colIndex].align);
    cellNode->justify = (effectiveAlign == Align::Center)
        ? Justify::Center
        : ((effectiveAlign == Align::End) ? Justify::End : Justify::Start);
    const keel::f32 cellDensity = m_ctx->GetDensity();
    cellNode->padding = Padding::Symmetric(6.0f * cellDensity, 2.0f * cellDensity);
    cellNode->gap = 4.0f * cellDensity;
    cellNode->clipChildren = true;

    m_data->state->cellBindings.push_back(TableCellBinding{
        .child = cellNode,
        .rowIndex = m_data->activeRowIndex,
        .colIndex = colIndex
    });
    m_data->activeColCursor = colIndex + 1;
    m_data->activeRowLastScannedChild = cellNode;

    m_ctx->PushContainer(cellNode);
    return ContainerScope(m_ctx, cellNode);
}

void TableScope::ScrollToRow(keel::usize rowIndex) {
    if (!m_data || !m_data->state) return;
    const keel::f32 rowTop = keel::as<keel::f32>(rowIndex) * m_data->rowHeight;
    const keel::f32 rowBottom = rowTop + m_data->rowHeight;
    const keel::f32 viewH = keel::max(m_data->rowHeight, m_data->state->lastViewportHeight - m_data->scrollbarGutterH);
    if (rowTop < m_data->state->scrollOffset.y) {
        m_data->state->scrollOffset.y = rowTop;
    } else if (rowBottom > m_data->state->scrollOffset.y + viewH) {
        m_data->state->scrollOffset.y = keel::max(0.0f, rowBottom - viewH);
    }
    const keel::usize knownTotalRows = (m_data->opts.totalRows > 0)
        ? m_data->opts.totalRows
        : m_data->state->prevTotalRows;
    if (knownTotalRows > 0) {
        const keel::f32 maxScrollY = keel::max(0.0f, keel::as<keel::f32>(knownTotalRows) * m_data->rowHeight - viewH);
        m_data->state->scrollOffset.y = keel::clamp(m_data->state->scrollOffset.y, 0.0f, maxScrollY);
    }
    UpdateVisibleRowRange(*m_data);
    if (m_node && m_node->scroll) {
        m_node->scroll->offset = m_data->state->scrollOffset;
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

TableScope detail::Table::Open(
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
    node->scroll = ctx.AllocPayload<ScrollData>();
    node->scroll->orientation = Orientation::Both;
    node->scroll->policy = optsCopy.scrollbar;
    node->scroll->solverManaged = false;
    node->uiSize = activeSize;

    node->widthMode = Control::ResolveWidthMode(ctx, optsCopy.width, SizeMode::Stretch);
    if (optsCopy.width.IsFixed()) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = optsCopy.width.Dp();
    }

    node->heightMode = Control::ResolveHeightMode(optsCopy.height, SizeMode::Stretch);
    if (optsCopy.height.IsFixed()) {
        node->fixedHeight = optsCopy.height.Dp();
    }
    node->flexGrow = Control::ResolveFlexGrow(node->parent, optsCopy.width, optsCopy.height, 1.0f);

    auto& state = ctx.GetOrCreatePersistentState<TableState>(id);
    state.visibleRows.clear();
    state.cellBindings.clear();

    const keel::usize colCount = columns.size();
    if (state.userColCount != colCount) {
        state.userColCount = colCount;
        state.userColWidths.clear();
        state.hasUserColWidth.clear();
        state.userColWidths.reserve(colCount);
        state.hasUserColWidth.reserve(colCount);
        for (keel::usize i = 0; i < colCount; ++i) {
            state.userColWidths.push_back(0.0f);
            state.hasUserColWidth.push_back(0u);
        }
        state.resizingCol = -1;
    }

    // Sync external bindings into persistent state
    if (optsCopy.multiSelect) {
        if (optsCopy.selectedRows.has_target()) {
            SyncExternalSelectedRowsIn(state, optsCopy.selectedRows);
            if (optsCopy.selectedRow) {
                state.selectedRow = *optsCopy.selectedRow;
            } else if (state.selectedRow >= 0 &&
                       !HasSelectedRow(state.selectedRows, keel::as<keel::usize>(state.selectedRow))) {
                state.selectedRow = state.selectedRows.empty()
                    ? -1
                    : keel::as<keel::i32>(state.selectedRows[state.selectedRows.size() - 1]);
            }
        } else if (optsCopy.selectedRow && *optsCopy.selectedRow != state.selectedRow) {
            if (*optsCopy.selectedRow >= 0) {
                SetSingleSelection(state, keel::as<keel::usize>(*optsCopy.selectedRow));
            } else {
                state.selectedRow = -1;
                state.selectionAnchor = -1;
                state.selectedRows.clear();
            }
        }
    } else {
        if (optsCopy.selectedRow) {
            state.selectedRow = *optsCopy.selectedRow;
        }
        state.selectedRows.clear();
        if (state.selectedRow >= 0) {
            state.selectedRows.push_back(keel::as<keel::usize>(state.selectedRow));
            state.selectionAnchor = state.selectedRow;
        } else {
            state.selectionAnchor = -1;
        }
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
    data->opts.emptyText = ctx.AllocText(optsCopy.emptyText);
    data->state = &state;
    if (theme.Painter().GetChromeMetrics(theme).solidSelectionHighlight) {
        auto* selTheme = ctx.AllocPayload<UiTheme>(theme);
        selTheme->palette.textPrimary = theme.palette.accentText;
        selTheme->palette.textSecondary = theme.palette.accentText.WithAlphaF(0.85f);
        selTheme->palette.textMuted = theme.palette.accentText.WithAlphaF(0.70f);
        auto* selZone = ctx.AllocPayload<Zone>(*node->zone);
        selZone->theme = selTheme;
        data->selectedTextZone = selZone;
    }

    data->columnCount = colCount;
    data->frozenColCount = keel::min<keel::usize>(optsCopy.frozenColumns, colCount);
    if (colCount > 0) {
        auto& arena = ctx.GetArena();
        data->columns = arena.alloc_array<TableColumn>(colCount);
        data->resolvedColX = arena.alloc_array<keel::f32>(colCount);
        data->resolvedColW = arena.alloc_array<keel::f32>(colCount);
        for (keel::usize i = 0; i < colCount; ++i) {
            data->columns[i] = columns[i];
            data->columns[i].title = ctx.AllocText(columns[i].title);
            data->columns[i].icon = ctx.AllocIcon(columns[i].icon);
            data->resolvedColX[i] = 0.0f;
            data->resolvedColW[i] = 0.0f;
        }
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

    if (data->bodyViewportRect.h > 0.0f) {
        state.lastViewportHeight = data->bodyViewportRect.h;
    }

    const keel::usize knownTotalRows = (optsCopy.totalRows > 0) ? optsCopy.totalRows : state.prevTotalRows;
    const TableScrollLayout sl = ResolveTableScrollLayout(node, *data, knownTotalRows, theme);
    const keel::f32 visibleColsW = sl.visibleW;
    const keel::f32 visibleRowsH = sl.visibleH;
    const keel::f32 estContentH = sl.contentH;
    const keel::f32 maxScrollY = sl.maxScrollY;
    const keel::f32 maxScrollX = sl.maxScrollX;

    // Handle scrollbars, mouse wheel, column resize, header sort, and row selection BEFORE virtualization!
    if (prevBounds.w > 0.0f && prevBounds.h > 0.0f && !activeDisabled) {
        const Vec2 mousePos = ctx.GetMousePos();
        const bool mouseClicked = ctx.IsMouseClicked();
        const bool mouseDown = ctx.IsMouseDown();
        const Vec2 prevScroll = state.scrollOffset;

        // 0. Interactive vertical & horizontal scrollbars (evaluated first so virtualization uses updated scrollOffset!)
        const ScrollbarMetrics& sm = theme.GetScrollbar(activeSize);
        const keel::f32 cornerReserve = sl.cornerReserve;

        bool hovV = false, actV = false;
        keel::u8 hovPartV = 0u;
        const Id vScrollId(id.value ^ 0x517CC1B727220A95ULL);
        if (state.resizingCol < 0) {
            detail::ScrollArea::UpdateAxisScrollbar(
                ctx,
                vScrollId,
                data->bodyViewportRect,
                estContentH + sl.gutterH,
                state.scrollOffset.y,
                state.draggingScrollbarV,
                state.dragStartOffsetV,
                state.dragStartMouseV,
                hovV,
                actV,
                false,
                activeSize,
                theme,
                cornerReserve,
                &hovPartV,
                &state.pressedButtonV,
                &state.nextRepeatSecV
            );
        }
        if (node->scroll) {
            node->scroll->hoveredY = hovV;
            node->scroll->activeY = actV;
            node->scroll->partHoveredY = hovPartV;
            node->scroll->partPressedY = state.draggingScrollbarV
                ? static_cast<keel::u8>(ScrollbarPart::Thumb)
                : state.pressedButtonV;
        }

        bool hovH = false, actH = false;
        keel::u8 hovPartH = 0u;
        const Id hScrollId(id.value ^ 0x9E3779B97F4A7C15ULL);
        if (state.resizingCol < 0) {
            detail::ScrollArea::UpdateAxisScrollbar(
                ctx,
                hScrollId,
                data->bodyViewportRect,
                data->totalColumnsW + sl.gutterW,
                state.scrollOffset.x,
                state.draggingScrollbarH,
                state.dragStartOffsetH,
                state.dragStartMouseH,
                hovH,
                actH,
                true,
                activeSize,
                theme,
                cornerReserve,
                &hovPartH,
                &state.pressedButtonH,
                &state.nextRepeatSecH
            );
        }
        if (node->scroll) {
            node->scroll->hoveredX = hovH;
            node->scroll->activeX = actH;
            node->scroll->partHoveredX = hovPartH;
            node->scroll->partPressedX = state.draggingScrollbarH
                ? static_cast<keel::u8>(ScrollbarPart::Thumb)
                : state.pressedButtonH;
        }

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
        UpdateVisibleColRange(*data);

        // 2. Column resize drag or header sort click
        const keel::f32 gripHalfW = 6.0f;
        const Rect headerHitRect(
            data->headerRect.x - gripHalfW,
            data->headerRect.y,
            data->headerRect.w + gripHalfW * 2.0f,
            data->headerRect.h
        );

        if (state.resizingCol >= 0) {
            ctx.SetCursor(CursorType::ResizeEW);
            if (mouseDown && keel::as<keel::usize>(state.resizingCol) < data->columnCount) {
                const keel::usize colIdx = keel::as<keel::usize>(state.resizingCol);
                const keel::f32 minW = keel::max(28.0f, data->columns[colIdx].minWidth);
                const keel::f32 maxW = (data->columns[colIdx].maxWidth > 0.0f)
                    ? data->columns[colIdx].maxWidth
                    : 100000.0f;
                const keel::f32 newW = keel::clamp(state.resizeStartWidth + (mousePos.x - state.resizeStartX), minW, maxW);
                state.userColWidths[colIdx] = newW;
                state.hasUserColWidth[colIdx] = 1u;
                ResolveColumnWidths(*data, visibleColsW, theme);
                const keel::f32 newMaxScrollX = keel::max(0.0f, data->totalColumnsW - visibleColsW);
                state.scrollOffset.x = keel::clamp(state.scrollOffset.x, 0.0f, newMaxScrollX);
                UpdateVisibleColRange(*data);
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
            const keel::f32 frozenClipW = (data->frozenColCount > 0)
                ? keel::clamp(data->frozenAreaW, 0.0f, keel::max(0.0f, data->headerRect.w))
                : 0.0f;
            const keel::f32 frozenBoundaryX = data->headerRect.x + frozenClipW;

            // Check column divider grips first (frozen columns take priority over scrolled columns)
            if (optsCopy.resizableColumns) {
                keel::i32 hitDivCol = -1;
                if (frozenClipW > 0.0f && mousePos.x <= frozenBoundaryX + gripHalfW) {
                    for (keel::usize i = 0; i < data->frozenColCount; ++i) {
                        if (!data->columns[i].resizable) continue;
                        const keel::f32 divScreenX = data->headerRect.x + data->resolvedColX[i] + data->resolvedColW[i];
                        if (divScreenX <= frozenBoundaryX + gripHalfW && keel::abs(mousePos.x - divScreenX) <= gripHalfW) {
                            hitDivCol = keel::as<keel::i32>(i);
                            break;
                        }
                    }
                }
                if (hitDivCol < 0 && mousePos.x > frozenBoundaryX) {
                    const keel::usize hitStart = (data->firstVisibleCol > data->frozenColCount)
                        ? (data->firstVisibleCol - 1)
                        : data->frozenColCount;
                    for (keel::usize i = hitStart; i < data->endVisibleCol; ++i) {
                        if (!data->columns[i].resizable) continue;
                        const keel::f32 divScreenX = data->headerRect.x + data->resolvedColX[i] + data->resolvedColW[i] - state.scrollOffset.x;
                        if (divScreenX > frozenBoundaryX && keel::abs(mousePos.x - divScreenX) <= gripHalfW) {
                            hitDivCol = keel::as<keel::i32>(i);
                            break;
                        }
                    }
                }

                if (hitDivCol >= 0) {
                    const keel::usize colIdx = keel::as<keel::usize>(hitDivCol);
                    data->hoveredDividerCol = hitDivCol;
                    ctx.SetCursor(CursorType::ResizeEW);
                    if (mouseClicked) {
                        const keel::f64 now = (ctx.GetCurrentTime)();
                        if (state.lastDividerCol == hitDivCol && (now - state.lastDividerClickTime) <= 0.35) {
                            for (keel::usize k = 0; k < state.hasUserColWidth.size(); ++k) {
                                state.hasUserColWidth[k] = 0u;
                            }
                            state.resizingCol = -1;
                            ResolveColumnWidths(*data, visibleColsW, theme);
                            const keel::f32 newMaxScrollX = keel::max(0.0f, data->totalColumnsW - visibleColsW);
                            state.scrollOffset.x = keel::clamp(state.scrollOffset.x, 0.0f, newMaxScrollX);
                            UpdateVisibleColRange(*data);
                            ctx.InvalidateRect(data->frameRect);
                            ctx.RequestRepaint(2);
                        } else {
                            // Snapshot all current resolved column widths so flex columns to the left
                            // do not counter-shrink or shift the left border of column `colIdx` during drag!
                            for (keel::usize j = 0; j < data->columnCount; ++j) {
                                if (state.hasUserColWidth[j] == 0u) {
                                    state.userColWidths[j] = data->resolvedColW[j];
                                    state.hasUserColWidth[j] = 1u;
                                }
                            }
                            state.resizingCol = hitDivCol;
                            state.resizeStartX = mousePos.x;
                            state.resizeStartWidth = data->resolvedColW[colIdx];
                            ctx.SetActiveId(id);
                            ctx.InvalidateRect(data->frameRect);
                            ctx.RequestRepaint(2);
                        }
                        state.lastDividerClickTime = now;
                        state.lastDividerCol = hitDivCol;
                    }
                }
            }

            // If not hovering a resize divider, check column header hover & click-to-sort
            if (data->hoveredDividerCol < 0 && data->headerRect.Contains(mousePos)) {
                keel::i32 hitHdrCol = -1;
                if (frozenClipW > 0.0f && mousePos.x <= frozenBoundaryX) {
                    for (keel::usize i = 0; i < data->frozenColCount; ++i) {
                        const keel::f32 colLeft = data->headerRect.x + data->resolvedColX[i];
                        const keel::f32 colRight = colLeft + data->resolvedColW[i];
                        if (mousePos.x >= colLeft && mousePos.x < colRight) {
                            hitHdrCol = keel::as<keel::i32>(i);
                            break;
                        }
                    }
                } else {
                    for (keel::usize i = data->firstVisibleCol; i < data->endVisibleCol; ++i) {
                        const keel::f32 colLeft = data->headerRect.x + data->resolvedColX[i] - state.scrollOffset.x;
                        const keel::f32 colRight = colLeft + data->resolvedColW[i];
                        if (mousePos.x >= colLeft && mousePos.x < colRight) {
                            hitHdrCol = keel::as<keel::i32>(i);
                            break;
                        }
                    }
                }

                if (hitHdrCol >= 0) {
                    const keel::usize colIdx = keel::as<keel::usize>(hitHdrCol);
                    data->hoveredHeaderCol = hitHdrCol;
                    if (mouseDown && data->columns[colIdx].sortable) {
                        data->pressedHeaderCol = hitHdrCol;
                    }
                    if (mouseClicked && data->columns[colIdx].sortable) {
                        if (state.sortColumn == hitHdrCol) {
                            state.sortDirection = (state.sortDirection == TableSortDirection::Ascending)
                                ? TableSortDirection::Descending
                                : TableSortDirection::Ascending;
                        } else {
                            state.sortColumn = hitHdrCol;
                            state.sortDirection = TableSortDirection::Ascending;
                        }
                        data->sortChanged = true;
                        if (optsCopy.sortColumn) *optsCopy.sortColumn = state.sortColumn;
                        if (optsCopy.sortDirection) *optsCopy.sortDirection = state.sortDirection;
                        ctx.InvalidateRect(data->frameRect);
                        ctx.RequestRepaint(2);
                    }
                }
            }
        }

        // 3. Body row hover & click selection (excluding scrollbar gutters)
        const Rect interactiveBodyRect(
            data->bodyViewportRect.x,
            data->bodyViewportRect.y,
            sl.visibleW,
            sl.visibleH
        );

        if (state.resizingCol < 0 && !interactingScrollbar && tableHovered && interactiveBodyRect.Contains(mousePos)) {
            const keel::f32 contentY = (mousePos.y - data->bodyViewportRect.y) + state.scrollOffset.y;
            if (contentY >= 0.0f && data->rowHeight > 0.0f) {
                const keel::usize hitRow = keel::as<keel::usize>(contentY / data->rowHeight);
                if (knownTotalRows > 0 && hitRow < knownTotalRows) {
                    const keel::i32 iHitRow = keel::as<keel::i32>(hitRow);
                    data->hoveredRow = iHitRow;
                    if (mouseClicked) {
                        data->clickedRow = iHitRow;
                        const keel::f64 now = (ctx.GetCurrentTime)();
                        if (state.lastClickRow == iHitRow && (now - state.lastClickTime) <= 0.35) {
                            data->doubleClickedRow = iHitRow;
                        }
                        state.lastClickTime = now;
                        state.lastClickRow = iHitRow;

                        const KeyModifiers mods = ctx.GetKeyModifiers();
                        const bool ctrlOrCmd = mods.ctrl;
                        bool selMutated = false;

                        if (optsCopy.multiSelect) {
                            if (mods.shift && state.selectionAnchor >= 0) {
                                SetRangeSelection(state, keel::as<keel::usize>(state.selectionAnchor), hitRow, ctrlOrCmd);
                                state.selectedRow = iHitRow;
                                selMutated = true;
                            } else if (ctrlOrCmd) {
                                if (HasSelectedRow(state.selectedRows, hitRow)) {
                                    RemoveSelectedRow(state.selectedRows, hitRow);
                                    if (state.selectedRow == iHitRow) {
                                        state.selectedRow = state.selectedRows.empty()
                                            ? -1
                                            : keel::as<keel::i32>(state.selectedRows[state.selectedRows.size() - 1]);
                                    }
                                } else {
                                    InsertSelectedRow(state.selectedRows, hitRow);
                                    state.selectedRow = iHitRow;
                                    state.selectionAnchor = iHitRow;
                                }
                                selMutated = true;
                            } else {
                                const bool alreadyOnlyThis = (state.selectedRows.size() == 1 &&
                                    state.selectedRows[0] == hitRow &&
                                    state.selectedRow == iHitRow);
                                if (!alreadyOnlyThis) {
                                    SetSingleSelection(state, hitRow);
                                    selMutated = true;
                                }
                            }
                        } else {
                            if (state.selectedRow != iHitRow || state.selectedRows.size() != 1 || state.selectedRows[0] != hitRow) {
                                SetSingleSelection(state, hitRow);
                                selMutated = true;
                            }
                        }

                        if (selMutated) {
                            data->selectionChanged = true;
                            if (optsCopy.selectedRow) {
                                *optsCopy.selectedRow = state.selectedRow;
                            }
                            SyncExternalSelectedRowsOut(state, optsCopy.selectedRows);
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
            const keel::i32 maxRow = keel::as<keel::i32>(knownTotalRows) - 1;
            if (optsCopy.multiSelect && ctx.IsFocused(id) && ctx.ConsumeAction(KeyAction::SelectAll)) {
                SetRangeSelection(state, 0, knownTotalRows - 1, false);
                if (state.selectedRow < 0) state.selectedRow = 0;
                if (state.selectionAnchor < 0) state.selectionAnchor = 0;
                data->selectionChanged = true;
                if (optsCopy.selectedRow) {
                    *optsCopy.selectedRow = state.selectedRow;
                }
                SyncExternalSelectedRowsOut(state, optsCopy.selectedRows);
                ctx.InvalidateRect(data->frameRect);
                ctx.RequestRepaint(2);
            } else {
                keel::i32 nextSel = state.selectedRow;
                bool extendRange = false;
                bool moved = false;

                if (optsCopy.multiSelect && ctx.ConsumeAction(KeyAction::SelectDown)) {
                    nextSel = keel::clamp(nextSel + 1, 0, maxRow);
                    extendRange = true;
                    moved = true;
                } else if (optsCopy.multiSelect && ctx.ConsumeAction(KeyAction::SelectUp)) {
                    nextSel = keel::clamp((nextSel < 0 ? 0 : nextSel - 1), 0, maxRow);
                    extendRange = true;
                    moved = true;
                } else if (ctx.ConsumeAction(KeyAction::MoveDown)) {
                    nextSel = keel::clamp(nextSel + 1, 0, maxRow);
                    moved = true;
                } else if (ctx.ConsumeAction(KeyAction::MoveUp)) {
                    nextSel = keel::clamp((nextSel < 0 ? 0 : nextSel - 1), 0, maxRow);
                    moved = true;
                } else if (ctx.ConsumeAction(KeyAction::MoveLineStart) || ctx.ConsumeAction(KeyAction::MoveDocStart)) {
                    nextSel = 0;
                    moved = true;
                } else if (ctx.ConsumeAction(KeyAction::MoveLineEnd) || ctx.ConsumeAction(KeyAction::MoveDocEnd)) {
                    nextSel = maxRow;
                    moved = true;
                } else if (ctx.ConsumeAction(KeyAction::MovePageDown)) {
                    const keel::i32 pageStep = keel::max(1, keel::as<keel::i32>(state.lastViewportHeight / keel::max(1.0f, data->rowHeight)));
                    nextSel = keel::clamp((nextSel < 0 ? 0 : nextSel) + pageStep, 0, maxRow);
                    moved = true;
                } else if (ctx.ConsumeAction(KeyAction::MovePageUp)) {
                    const keel::i32 pageStep = keel::max(1, keel::as<keel::i32>(state.lastViewportHeight / keel::max(1.0f, data->rowHeight)));
                    nextSel = keel::clamp((nextSel < 0 ? 0 : nextSel) - pageStep, 0, maxRow);
                    moved = true;
                }

                if (moved && nextSel >= 0 && (nextSel != state.selectedRow || (!extendRange && state.selectedRows.size() > 1))) {
                    if (extendRange) {
                        if (state.selectionAnchor < 0) {
                            state.selectionAnchor = (state.selectedRow >= 0) ? state.selectedRow : nextSel;
                        }
                        SetRangeSelection(state, keel::as<keel::usize>(state.selectionAnchor), keel::as<keel::usize>(nextSel), false);
                        state.selectedRow = nextSel;
                    } else {
                        SetSingleSelection(state, keel::as<keel::usize>(nextSel));
                    }
                    data->selectionChanged = true;
                    if (optsCopy.selectedRow) {
                        *optsCopy.selectedRow = state.selectedRow;
                    }
                    SyncExternalSelectedRowsOut(state, optsCopy.selectedRows);
                    // Keep newly selected row in view (above any horizontal scrollbar)
                    const keel::f32 rTop = keel::as<keel::f32>(nextSel) * data->rowHeight;
                    const keel::f32 rBot = rTop + data->rowHeight;
                    if (rTop < state.scrollOffset.y) {
                        state.scrollOffset.y = rTop;
                    } else if (rBot > state.scrollOffset.y + visibleRowsH) {
                        state.scrollOffset.y = keel::max(0.0f, rBot - visibleRowsH);
                    }
                    ctx.InvalidateRect(data->frameRect);
                    ctx.RequestRepaint(2);
                }
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

    if (node->scroll) {
        node->scroll->offset = state.scrollOffset;
    }
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
        optsCopy.emptyText.text,
        optsCopy.frozenColumns,
        optsCopy.multiSelect,
        data->rowHeight,
        data->headerHeight,
        data->footerHeight,
        data->columnCount,
        activeDisabled
    );
    static constexpr PanelHooks kTableHooks{
        .contentClip = &detail::Table::GetBodyClipRect,
        .update = &detail::Table::Update
    };
    node->measureFn = &detail::Table::Measure;
    node->arrangeFn = &detail::Table::Arrange;
    node->panelHooks = &kTableHooks;
    node->renderFn = &detail::Table::RenderBody;
    node->postRenderFn = &detail::Table::RenderChromeAndScrollbars;

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

Vec2 detail::Table::Measure(
    const Node* node,
    keel::f32 availableWidth,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const auto* data = node ? node->As<TableData>() : nullptr;
    if (!data) return {200.0f, 160.0f};
    data->lastCanvas = canvas;

    const keel::f32 density = (node && node->zone) ? node->zone->density : 1.0f;
    const keel::f32 headerH = data->opts.stickyHeader
        ? (data->opts.headerHeight.has_value()
            ? *data->opts.headerHeight
            : ResolveDefaultHeaderHeight(node->uiSize, theme, density))
        : 0.0f;
    const keel::f32 rowH = data->opts.rowHeight.has_value()
        ? keel::max(18.0f, *data->opts.rowHeight)
        : ResolveDefaultRowHeight(node->uiSize, theme, density);
    const keel::f32 footerH = (data->opts.showFooter || !data->opts.footerText.empty())
        ? ResolveDefaultFooterHeight(node->uiSize, theme, density)
        : 0.0f;

    const keel::usize totalRows = (data->opts.totalRows > 0)
        ? data->opts.totalRows
        : data->emittedRowCount;

    const SizeMode wMode = data->opts.width.ResolveMode(SizeMode::Stretch);
    keel::f32 measuredW = 200.0f;
    if (data->opts.width.IsFixed()) {
        measuredW = data->opts.width.Dp();
    } else if (wMode == SizeMode::Fit) {
        keel::f32 naturalW = data->opts.showRowNumbers ? 44.0f : 0.0f;
        for (keel::usize i = 0; i < data->columnCount; ++i) {
            const keel::f32 colW = (data->columns[i].width > 0.0f)
                ? data->columns[i].width
                : keel::max(80.0f, data->columns[i].minWidth);
            naturalW += colW;
        }
        measuredW = naturalW + data->bevelInset * 2.0f;
    } else {
        // Stretch mode: Table is horizontally scrollable — never expand ancestor containers
        // to the total width of all columns.
        measuredW = (availableWidth > 0.0f) ? availableWidth : 200.0f;
    }

    const SizeMode hMode = data->opts.height.ResolveMode(SizeMode::Stretch);
    keel::f32 measuredH = 160.0f;
    if (data->opts.height.IsFixed()) {
        measuredH = data->opts.height.Dp();
    } else if (hMode == SizeMode::Fit) {
        const keel::usize fitRows = keel::clamp<keel::usize>(totalRows, 3, 12);
        measuredH = headerH + keel::as<keel::f32>(fitRows) * rowH + footerH + data->bevelInset * 2.0f;
    } else {
        // Stretch mode: Table is vertically scrollable — keep intrinsic minimum compact
        // so flexGrow fills the parent viewport without overflowing scroll containers.
        const keel::usize minRows = keel::min<keel::usize>(keel::max<keel::usize>(totalRows, 1), 3);
        measuredH = headerH + keel::as<keel::f32>(minRows) * rowH + footerH + data->bevelInset * 2.0f;
    }

    return {measuredW, measuredH};
}

void detail::Table::Arrange(
    Node* node,
    const Rect& availableRect,
    const UiTheme& theme
) {
    if (!node) return;
    node->computedRect = availableRect;
    auto* data = node->As<TableData>();
    if (!data || !data->state) return;

    ComputeTableRegions(node, *data, availableRect, theme);
    const keel::usize knownTotalRows = (data->opts.totalRows > 0)
        ? data->opts.totalRows
        : ((data->emittedRowCount > 0) ? data->emittedRowCount : data->state->prevTotalRows);
    (void)ResolveTableScrollLayout(node, *data, knownTotalRows, theme);

    data->state->lastArrangedBounds = availableRect;
    if (data->bodyViewportRect.h > 0.0f) {
        data->state->lastViewportHeight = data->bodyViewportRect.h;
    }

    // Update frozen/scroll clip pane bounds when frozenColumns > 0
    const keel::f32 frozenClipW = (data->frozenColCount > 0)
        ? keel::clamp(data->frozenAreaW, 0.0f, keel::max(0.0f, data->bodyViewportRect.w))
        : 0.0f;
    if (data->scrollPane) {
        data->scrollPane->computedRect = Rect(
            data->bodyViewportRect.x + frozenClipW,
            data->bodyViewportRect.y,
            keel::max(0.0f, data->bodyViewportRect.w - frozenClipW),
            data->bodyViewportRect.h
        );
    }
    if (data->frozenPane) {
        data->frozenPane->computedRect = Rect(
            data->bodyViewportRect.x,
            data->bodyViewportRect.y,
            frozenClipW,
            data->bodyViewportRect.h
        );
    }

    // Position all bound cell children (both direct widgets and explicit NodeType::Cell containers)
    // inside the virtualized body viewport
    const Vec2 scrollOff = node->GetScrollOffset();
    for (const auto& binding : data->state->cellBindings) {
        Node* child = binding.child;
        if (!child) continue;
        const keel::usize colIdx = binding.colIndex;
        if (colIdx >= data->columnCount || !data->resolvedColX || !data->resolvedColW) {
            child->computedRect = Rect(0.0f, 0.0f, 0.0f, 0.0f);
            continue;
        }

        const keel::usize rowIndex = binding.rowIndex;
        const bool isFrozen = (colIdx < data->frozenColCount);
        const keel::f32 cellX = data->bodyViewportRect.x + data->resolvedColX[colIdx] - (isFrozen ? 0.0f : scrollOff.x);
        const keel::f32 cellY = data->bodyViewportRect.y + keel::as<keel::f32>(rowIndex) * data->rowHeight - scrollOff.y;
        const keel::f32 cellW = data->resolvedColW[colIdx];
        const keel::f32 cellH = data->rowHeight;

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

            keel::f32 totalChildrenW = 0.0f;
            keel::u32 numGrandChildren = 0;
            for (Node* gc = child->firstChild; gc != nullptr; gc = gc->nextSibling) {
                const UiTheme& gcTheme = gc->GetTheme(theme);
                const Vec2 m = gc->measureFn
                    ? gc->measureFn(gc, inner.w, data->lastCanvas, gcTheme)
                    : Vec2(gc->fixedWidth, gc->fixedHeight);
                gc->intrinsicSize = m;
                const keel::f32 gcW = (gc->widthMode == SizeMode::Fixed && gc->fixedWidth > 0.0f)
                    ? gc->fixedWidth
                    : m.x;
                totalChildrenW += (gc->widthMode == SizeMode::Stretch) ? inner.w : gcW;
                ++numGrandChildren;
            }
            if (numGrandChildren > 1) {
                totalChildrenW += child->gap * keel::as<keel::f32>(numGrandChildren - 1);
            }

            keel::f32 curX = inner.x;
            if (child->justify == Justify::Center && totalChildrenW < inner.w) {
                curX += (inner.w - totalChildrenW) * 0.5f;
            } else if (child->justify == Justify::End && totalChildrenW < inner.w) {
                curX += (inner.w - totalChildrenW);
            }

            for (Node* gc = child->firstChild; gc != nullptr; gc = gc->nextSibling) {
                const UiTheme& gcTheme = gc->GetTheme(theme);
                const keel::f32 gcTargetW = (gc->widthMode == SizeMode::Fixed && gc->fixedWidth > 0.0f)
                    ? gc->fixedWidth
                    : gc->intrinsicSize.x;
                const keel::f32 w = (gc->widthMode == SizeMode::Stretch)
                    ? keel::max(0.0f, inner.Right() - curX)
                    : keel::min(gcTargetW, keel::max(0.0f, inner.Right() - curX));
                const keel::f32 gcTargetH = (gc->heightMode == SizeMode::Fixed && gc->fixedHeight > 0.0f)
                    ? gc->fixedHeight
                    : gc->intrinsicSize.y;
                const keel::f32 h = (gc->heightMode == SizeMode::Stretch)
                    ? inner.h
                    : keel::min(gcTargetH, inner.h);
                const keel::f32 y = inner.y + (inner.h - h) * 0.5f;
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
            const keel::f32 padX = isTextOrBadge ? 8.0f : 6.0f;
            const keel::f32 padY = isTextOrBadge ? 0.0f : 2.0f;
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

            keel::f32 w = m.x;
            if (isTextOrBadge) {
                w = keel::min(inner.w, m.x);
            } else if (child->widthMode == SizeMode::Fixed && child->fixedWidth > 0.0f) {
                w = keel::min(inner.w, child->fixedWidth);
            } else if (child->widthMode == SizeMode::Stretch) {
                w = inner.w;
            } else {
                w = keel::min(inner.w, m.x);
            }

            const keel::f32 targetH = (child->heightMode == SizeMode::Fixed && child->fixedHeight > 0.0f)
                ? child->fixedHeight
                : m.y;
            const keel::f32 h = (child->heightMode == SizeMode::Stretch)
                ? inner.h
                : keel::min(targetH, inner.h);
            const keel::f32 x = inner.x + (inner.w - w) * AlignToFactor(effAlign);
            const keel::f32 y = inner.y + (inner.h - h) * 0.5f;
            child->computedRect = Rect(x, y, w, h);
            if (child->arrangeFn) {
                child->arrangeFn(child, child->computedRect, childTheme);
            }
        }
    }
}

Rect detail::Table::GetBodyClipRect(
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

void detail::Table::Update(
    Context& ctx,
    Node* node
) {
    if (!node || node->disabled) return;
    auto* data = node->As<TableData>();
    if (!data || !data->state) return;

    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    ComputeTableRegions(node, *data, node->computedRect, theme);
    const keel::usize knownTotalRows = (data->opts.totalRows > 0)
        ? data->opts.totalRows
        : ((data->emittedRowCount > 0) ? data->emittedRowCount : data->state->prevTotalRows);
    const TableScrollLayout sl = ResolveTableScrollLayout(node, *data, knownTotalRows, theme);

    const Vec2 scrollOff = node->GetScrollOffset();
    data->state->scrollOffset.y = keel::clamp(data->state->scrollOffset.y, 0.0f, sl.maxScrollY);
    data->state->scrollOffset.x = keel::clamp(data->state->scrollOffset.x, 0.0f, sl.maxScrollX);

    if (keel::abs(data->state->scrollOffset.x - scrollOff.x) > 0.01f ||
        keel::abs(data->state->scrollOffset.y - scrollOff.y) > 0.01f) {
        if (node->scroll) {
            node->scroll->offset = data->state->scrollOffset;
        }
        detail::Table::Arrange(node, node->computedRect, theme);
        ctx.InvalidateRect(data->frameRect);
        ctx.RequestRepaint(2);
    }
}

// ============================================================================
// Table::RenderBody & Table::RenderChromeAndScrollbars
// ============================================================================

void detail::Table::RenderBody(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node) return;
    const auto* data = node->As<TableData>();
    if (!data || !data->state) return;

    const auto& pal = theme.palette;
    const keel::f32 radius = data->opts.border.radius.has_value()
        ? *data->opts.border.radius
        : theme.GetRadius(node->uiSize);
    const keel::f32 innerRadius = keel::max(0.0f, radius - data->bevelInset);
    const bool solidSelTheme = theme.Painter().GetChromeMetrics(theme).solidSelectionHighlight;

    // 1. Draw outer table surface background via active IStylePainter
    if (data->opts.background.IsBackgroundEnabled(true)) {
        const Color bgCol = theme.ResolveColor(data->opts.background, false, false, node->disabled, pal.surface);
        if (bgCol.a > 0) {
            theme.Painter().DrawSurfaceBg(canvas, theme, SurfacePaintParams{
                .rect = data->frameRect,
                .role = SurfaceRole::Sunken,
                .radius = radius,
                .explicitRadius = data->opts.border.radius.has_value(),
                .bg = bgCol
            });
        }
    }

    // 2. Render virtualized rows & grid lines clipped to innerFrameRect + bodyViewportRect
    if (data->bodyViewportRect.w > 0.0f && data->bodyViewportRect.h > 0.0f) {
        const bool clipRounded = (innerRadius > 0.5f);
        if (clipRounded) {
            canvas.PushRoundedClip(data->innerFrameRect, innerRadius);
        }
        canvas.PushClip(data->bodyViewportRect);

        const keel::f32 bodyFontSize = ResolveTableFontSize(UiText{}, TextRole::BodyMedium, theme);
        const keel::f32 cellPadX = 8.0f;
        const keel::f32 frozenClipW = (data->frozenColCount > 0)
            ? keel::clamp(data->frozenAreaW, 0.0f, keel::max(0.0f, data->bodyViewportRect.w))
            : 0.0f;

        // Draw row backgrounds (selection/hover via IStylePainter, tint, zebra striping, horizontal grid lines)
        const Vec2 scrollOff = node->GetScrollOffset();
        const keel::f32 rowNumScrollX = (data->frozenColCount > 0) ? 0.0f : scrollOff.x;
        for (const auto& rowRec : data->state->visibleRows) {
            const keel::usize rIdx = rowRec.rowIndex;
            const keel::f32 rowY = data->bodyViewportRect.y + keel::as<keel::f32>(rIdx) * data->rowHeight - scrollOff.y;
            const Rect rowRect(data->bodyViewportRect.x, rowY, data->bodyViewportRect.w, data->rowHeight);

            if (!rowRect.Intersects(data->bodyViewportRect)) continue;

            const keel::i32 iRow = keel::as<keel::i32>(rIdx);
            const bool isSelected = HasSelectedRow(data->state->selectedRows, rIdx);
            const bool isHovered = data->opts.hoverableRows && (data->hoveredRow == iRow) && !rowRec.opts.disabled;

            bool usedSolidSelection = false;
            const Color dangerBg = theme.GetSemanticAccent(UiVariant::Danger);
            if (isSelected) {
                usedSolidSelection = theme.Painter().DrawSelectionHighlight(
                    canvas,
                    theme,
                    rowRect,
                    0.0f,
                    isHovered,
                    true,
                    false,
                    false,
                    rowRec.opts.disabled,
                    dangerBg
                );
            } else if (isHovered) {
                if (solidSelTheme) {
                    canvas.DrawRect(rowRect, pal.surfaceHover.WithAlphaF(0.65f));
                } else {
                    (void)theme.Painter().DrawSelectionHighlight(
                        canvas,
                        theme,
                        rowRect,
                        0.0f,
                        true,
                        false,
                        false,
                        false,
                        rowRec.opts.disabled,
                        dangerBg
                    );
                }
            } else if (rowRec.opts.tint.has_value()) {
                const Color tintCol = theme.GetSemanticAccent(*rowRec.opts.tint).WithAlpha(28);
                canvas.DrawRect(rowRect, tintCol);
            } else if (data->opts.striped && (rIdx % 2 == 1)) {
                canvas.DrawRect(rowRect, pal.surfaceElevated.WithAlpha(90));
            }

            // Optional automatic '#' row number cell (pinned when frozenColumns > 0)
            if (data->opts.showRowNumbers && data->rowNumberColW > 0.0f) {
                char numBuf[24]{};
                const keel::StringRef numStr = keel::format_buf(numBuf, "%zu", rIdx + 1);
                const Rect numRect(
                    data->bodyViewportRect.x - rowNumScrollX + cellPadX * 0.5f,
                    rowY,
                    keel::max(0.0f, data->rowNumberColW - cellPadX),
                    data->rowHeight
                );
                canvas.DrawTextBox(
                    numStr,
                    numRect,
                    bodyFontSize * 0.9f,
                    usedSolidSelection ? pal.accentText : pal.textMuted,
                    1.0f,
                    0.5f
                );
            }

            if (data->opts.gridLines) {
                const keel::f32 lineY = keel::floor(rowRect.Bottom()) - 0.5f;
                canvas.DrawLine(
                    Vec2(rowRect.x, lineY),
                    Vec2(rowRect.Right(), lineY),
                    pal.borderSubtle.WithAlpha(95),
                    1.0f
                );
            }
        }

        // Draw vertical column grid lines across the body viewport (frozen + visible scrollable columns)
        if (data->opts.gridLines) {
            if (data->opts.showRowNumbers && data->rowNumberColW > 0.0f) {
                const keel::f32 divX = keel::floor(data->bodyViewportRect.x + data->rowNumberColW - rowNumScrollX) + 0.5f;
                if (divX >= data->bodyViewportRect.x && divX <= data->bodyViewportRect.Right()) {
                    canvas.DrawLine(
                        Vec2(divX, data->bodyViewportRect.y),
                        Vec2(divX, data->bodyViewportRect.Bottom()),
                        pal.borderSubtle.WithAlpha(95),
                        1.0f
                    );
                }
            }
            for (keel::usize i = 0; i < data->frozenColCount; ++i) {
                const keel::f32 divX = keel::floor(
                    data->bodyViewportRect.x + data->resolvedColX[i] + data->resolvedColW[i]
                ) + 0.5f;
                if (divX >= data->bodyViewportRect.x && divX < data->bodyViewportRect.Right() - 0.5f) {
                    canvas.DrawLine(
                        Vec2(divX, data->bodyViewportRect.y),
                        Vec2(divX, data->bodyViewportRect.Bottom()),
                        pal.borderSubtle.WithAlpha(95),
                        1.0f
                    );
                }
            }
            const keel::f32 scrollLeftBound = data->bodyViewportRect.x + frozenClipW + 0.5f;
            for (keel::usize i = data->firstVisibleCol; i < data->endVisibleCol; ++i) {
                const keel::f32 divX = keel::floor(
                    data->bodyViewportRect.x + data->resolvedColX[i] + data->resolvedColW[i] - scrollOff.x
                ) + 0.5f;
                if (divX > scrollLeftBound && divX < data->bodyViewportRect.Right() - 0.5f) {
                    canvas.DrawLine(
                        Vec2(divX, data->bodyViewportRect.y),
                        Vec2(divX, data->bodyViewportRect.Bottom()),
                        pal.borderSubtle.WithAlpha(85),
                        1.0f
                    );
                }
            }
        }

        // Draw centered empty-state placeholder when dataset is empty
        const keel::usize totalRows = (data->opts.totalRows > 0)
            ? data->opts.totalRows
            : data->emittedRowCount;
        if (totalRows == 0 && !data->opts.emptyText.empty()) {
            const Rect emptyRect(
                data->bodyViewportRect.x + 16.0f,
                data->bodyViewportRect.y + 8.0f,
                keel::max(0.0f, data->bodyViewportRect.w - data->scrollbarGutterW - 32.0f),
                keel::max(0.0f, data->bodyViewportRect.h - data->scrollbarGutterH - 16.0f)
            );
            if (emptyRect.w > 4.0f && emptyRect.h > 4.0f) {
                const keel::f32 emptyFontSize = ResolveTableFontSize(data->opts.emptyText, TextRole::BodyMedium, theme);
                canvas.DrawTextBox(
                    data->opts.emptyText.text,
                    emptyRect,
                    emptyFontSize,
                    theme.ResolveColor(data->opts.emptyText.color, false, false, node->disabled, pal.textMuted),
                    0.5f,
                    0.5f
                );
            }
        }

        // Draw full-height vertical resize guide line when hovering or dragging a column divider
        const keel::i32 activeDivCol = (data->state->resizingCol >= 0)
            ? data->state->resizingCol
            : data->hoveredDividerCol;
        if (activeDivCol >= 0 && keel::as<keel::usize>(activeDivCol) < data->columnCount) {
            const keel::usize colIdx = keel::as<keel::usize>(activeDivCol);
            const bool divFrozen = (colIdx < data->frozenColCount);
            const keel::f32 guideX = keel::clamp(
                keel::floor(data->bodyViewportRect.x + data->resolvedColX[colIdx] + data->resolvedColW[colIdx] - (divFrozen ? 0.0f : scrollOff.x)) + 0.5f,
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
                (data->state->resizingCol >= 0) ? 1.5f : 1.0f
            );
        }

        canvas.PopClip();
        if (clipRounded) {
            canvas.PopClip();
        }
    }
}

void detail::Table::RenderChromeAndScrollbars(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node) return;
    const auto* data = node->As<TableData>();
    if (!data || !data->state) return;

    const auto& pal = theme.palette;
    const keel::f32 radius = data->opts.border.radius.has_value()
        ? *data->opts.border.radius
        : theme.GetRadius(node->uiSize);
    const keel::f32 innerRadius = keel::max(0.0f, radius - data->bevelInset);
    const keel::f32 cellPadX = 8.0f;

    const bool clipRounded = (innerRadius > 0.5f);
    if (clipRounded) {
        canvas.PushRoundedClip(data->innerFrameRect, innerRadius);
    }

    // 1. Render sticky column header bar (scrollable + frozen columns via IStylePainter::DrawTableHeaderCell)
    const Vec2 scrollOff = node->GetScrollOffset();
    const keel::f32 headerFrozenClipW = (data->frozenColCount > 0)
        ? keel::clamp(data->frozenAreaW, 0.0f, keel::max(0.0f, data->headerRect.w))
        : 0.0f;

    if (data->opts.stickyHeader && data->headerRect.w > 0.0f && data->headerRect.h > 0.0f) {
        canvas.PushClip(data->headerRect);
        canvas.DrawRect(data->headerRect, pal.surfaceElevated);

        const keel::f32 headerFontSize = ResolveTableFontSize(UiText{}, TextRole::LabelSmall, theme);

        auto drawRowNumHeader = [&](keel::f32 x) {
            const Rect cellRect(x, data->headerRect.y, data->rowNumberColW, data->headerRect.h);
            if (!cellRect.Intersects(data->headerRect)) return;
            theme.Painter().DrawTableHeaderCell(canvas, theme, TableHeaderCellPaintParams{
                .rect = cellRect,
                .hovered = false,
                .pressed = false,
                .sorted = false,
                .sortable = false,
                .dividerActive = false,
                .showRightDivider = true,
                .isFrozenSeam = false
            });
            const Rect numHdrRect(
                x + cellPadX * 0.5f,
                data->headerRect.y,
                keel::max(0.0f, data->rowNumberColW - cellPadX),
                data->headerRect.h
            );
            canvas.DrawTextBox("#", numHdrRect, headerFontSize, pal.textMuted, 1.0f, 0.5f);
        };

        auto drawHeaderCol = [&](keel::usize i, keel::f32 colX, bool isFrozenSeam) {
            const auto& col = data->columns[i];
            const keel::f32 colW = data->resolvedColW[i];
            const Rect colHdrRect(colX, data->headerRect.y, colW, data->headerRect.h);
            if (!colHdrRect.Intersects(data->headerRect)) return;

            const keel::i32 iCol = keel::as<keel::i32>(i);
            const bool isHovered = (data->hoveredHeaderCol == iCol) && col.sortable;
            const bool isPressed = (data->pressedHeaderCol == iCol) && col.sortable;
            const bool isSorted = (data->state->sortColumn == iCol) &&
                                  (data->state->sortDirection != TableSortDirection::None);
            const bool dividerActive = (data->state->resizingCol == iCol) ||
                                       (data->hoveredDividerCol == iCol);

            theme.Painter().DrawTableHeaderCell(canvas, theme, TableHeaderCellPaintParams{
                .rect = colHdrRect,
                .hovered = isHovered,
                .pressed = isPressed,
                .sorted = isSorted,
                .sortable = col.sortable,
                .dividerActive = dividerActive,
                .showRightDivider = true,
                .isFrozenSeam = isFrozenSeam
            });

            const keel::f32 pressShift = theme.Painter().GetButtonContentOffset(theme, isPressed, false, node->disabled);
            const Vec2 pressOff(pressShift, pressShift);

            const keel::f32 sortArrowReserve = col.sortable ? 14.0f : 0.0f;
            Rect titleRect(
                colX + cellPadX + pressOff.x,
                data->headerRect.y + pressOff.y,
                keel::max(0.0f, colW - cellPadX * 2.0f - sortArrowReserve),
                data->headerRect.h
            );

            const Color titleCol = isSorted ? pal.accent : pal.textSecondary;
            if (col.icon.IsValid()) {
                const keel::f32 iconSz = 13.0f;
                const Rect iconRect(
                    titleRect.x,
                    data->headerRect.y + (data->headerRect.h - iconSz) * 0.5f + pressOff.y,
                    iconSz,
                    iconSz
                );
                col.icon.Draw(canvas, iconRect, titleCol);
                titleRect.x += iconSz + 5.0f;
                titleRect.w = keel::max(0.0f, titleRect.w - (iconSz + 5.0f));
            }

            if (!col.title.empty() && titleRect.w > 2.0f) {
                const keel::f32 colFontSize = ResolveTableFontSize(col.title, TextRole::LabelSmall, theme);
                const Color resolvedTitleCol = isSorted
                    ? titleCol
                    : theme.ResolveColor(col.title.color, isHovered, isPressed, node->disabled, titleCol);
                const auto truncated = TextUtil::TruncateWithEllipsis(
                    col.title.text,
                    titleRect.w,
                    colFontSize,
                    &canvas
                );
                canvas.DrawTextBox(
                    truncated,
                    titleRect,
                    colFontSize,
                    resolvedTitleCol,
                    AlignToFactor(col.align),
                    0.5f
                );
            }

            // Draw sort indicator chevron/triangle
            if (col.sortable) {
                const keel::f32 arrowCx = colX + colW - cellPadX - 4.0f + pressOff.x;
                const keel::f32 arrowCy = data->headerRect.Center().y + pressOff.y;
                const keel::f32 s = 3.5f;
                if (isSorted) {
                    if (data->state->sortDirection == TableSortDirection::Ascending) {
                        canvas.DrawLine(Vec2(arrowCx - s, arrowCy + s * 0.5f), Vec2(arrowCx, arrowCy - s * 0.6f), pal.accent, 1.6f);
                        canvas.DrawLine(Vec2(arrowCx, arrowCy - s * 0.6f), Vec2(arrowCx + s, arrowCy + s * 0.5f), pal.accent, 1.6f);
                    } else {
                        canvas.DrawLine(Vec2(arrowCx - s, arrowCy - s * 0.5f), Vec2(arrowCx, arrowCy + s * 0.6f), pal.accent, 1.6f);
                        canvas.DrawLine(Vec2(arrowCx, arrowCy + s * 0.6f), Vec2(arrowCx + s, arrowCy - s * 0.5f), pal.accent, 1.6f);
                    }
                } else if (isHovered) {
                    // Subtle dual-chevron hint on hover
                    const Color hintCol = pal.textMuted.WithAlpha(160);
                    canvas.DrawLine(Vec2(arrowCx - s * 0.8f, arrowCy - 1.5f), Vec2(arrowCx, arrowCy - s), hintCol, 1.2f);
                    canvas.DrawLine(Vec2(arrowCx, arrowCy - s), Vec2(arrowCx + s * 0.8f, arrowCy - 1.5f), hintCol, 1.2f);
                    canvas.DrawLine(Vec2(arrowCx - s * 0.8f, arrowCy + 1.5f), Vec2(arrowCx, arrowCy + s), hintCol, 1.2f);
                    canvas.DrawLine(Vec2(arrowCx, arrowCy + s), Vec2(arrowCx + s * 0.8f, arrowCy + 1.5f), hintCol, 1.2f);
                }
            }
        };

        // 1a. Scrollable header columns (clipped to [frozenClipW .. headerRect.Right()])
        const bool hasFrozenHdr = (headerFrozenClipW > 0.0f);
        if (hasFrozenHdr) {
            canvas.PushClip(Rect(
                data->headerRect.x + headerFrozenClipW,
                data->headerRect.y,
                keel::max(0.0f, data->headerRect.w - headerFrozenClipW),
                data->headerRect.h
            ));
        }

        if (!hasFrozenHdr && data->opts.showRowNumbers && data->rowNumberColW > 0.0f) {
            drawRowNumHeader(data->headerRect.x - scrollOff.x);
        }
        for (keel::usize i = data->firstVisibleCol; i < data->endVisibleCol; ++i) {
            drawHeaderCol(i, data->headerRect.x + data->resolvedColX[i] - scrollOff.x, false);
        }
        const keel::f32 colsEndX = keel::max(
            data->headerRect.x + headerFrozenClipW,
            data->headerRect.x + data->totalColumnsW - scrollOff.x
        );
        if (colsEndX < data->headerRect.Right() - 0.5f) {
            theme.Painter().DrawTableHeaderCell(canvas, theme, TableHeaderCellPaintParams{
                .rect = Rect(colsEndX, data->headerRect.y, data->headerRect.Right() - colsEndX, data->headerRect.h),
                .hovered = false,
                .pressed = false,
                .sorted = false,
                .sortable = false,
                .dividerActive = false,
                .showRightDivider = false,
                .isFrozenSeam = false
            });
        }

        if (hasFrozenHdr) {
            canvas.PopClip();

            // 1b. Frozen header columns pinned on the left
            canvas.PushClip(Rect(data->headerRect.x, data->headerRect.y, headerFrozenClipW, data->headerRect.h));
            if (data->opts.showRowNumbers && data->rowNumberColW > 0.0f) {
                drawRowNumHeader(data->headerRect.x);
            }
            for (keel::usize i = 0; i < data->frozenColCount; ++i) {
                drawHeaderCol(i, data->headerRect.x + data->resolvedColX[i], (i + 1 == data->frozenColCount));
            }
            canvas.PopClip();
        }

        // Bottom separator line under sticky header
        const keel::f32 hdrBotY = keel::floor(data->headerRect.Bottom()) - 0.5f;
        canvas.DrawLine(
            Vec2(data->headerRect.x, hdrBotY),
            Vec2(data->headerRect.Right(), hdrBotY),
            pal.border,
            1.0f
        );
        canvas.PopClip();
    }

    // 1c. Frozen column vertical seam & subtle right drop shadow across header + body
    if (data->frozenColCount > 0) {
        const keel::f32 maxFrozenW = keel::max(0.0f, data->bodyViewportRect.w - data->scrollbarGutterW);
        const keel::f32 seamW = keel::clamp(data->frozenAreaW, 0.0f, maxFrozenW);
        if (seamW > 0.5f && seamW < maxFrozenW - 0.5f) {
            const keel::f32 seamTop = (data->opts.stickyHeader && data->headerRect.h > 0.0f)
                ? data->headerRect.y
                : data->bodyViewportRect.y;
            const keel::f32 seamBottom = keel::max(seamTop, data->bodyViewportRect.Bottom() - data->scrollbarGutterH);
            const keel::f32 seamX = keel::floor(data->bodyViewportRect.x + seamW) + 0.5f;
            canvas.DrawLine(
                Vec2(seamX, seamTop),
                Vec2(seamX, seamBottom),
                pal.borderHover,
                1.0f
            );
            if (scrollOff.x > 0.5f && seamBottom > seamTop) {
                const keel::f32 shadowBaseX = seamX + 0.5f;
                const keel::f32 shadowH = seamBottom - seamTop;
                const keel::f32 maxShadowW = keel::max(0.0f, data->bodyViewportRect.x + maxFrozenW - shadowBaseX);
                static constexpr keel::StaticArray<keel::u8, 4> kShadowAlphas = {42u, 26u, 14u, 6u};
                for (keel::usize s = 0; s < kShadowAlphas.size(); ++s) {
                    if (keel::as<keel::f32>(s) >= maxShadowW) break;
                    canvas.DrawRect(
                        Rect(shadowBaseX + keel::as<keel::f32>(s), seamTop, 1.0f, shadowH),
                        Color(0, 0, 0, kShadowAlphas[s])
                    );
                }
            }
        }
    }

    // 2. Render optional footer bar (showing visible rows / total rows + custom footerText)
    if (data->footerRect.w > 0.0f && data->footerRect.h > 0.0f) {
        canvas.PushClip(data->footerRect);
        canvas.DrawRect(data->footerRect, pal.surfaceElevated);
        const keel::f32 topY = keel::floor(data->footerRect.y) + 0.5f;
        canvas.DrawLine(
            Vec2(data->footerRect.x, topY),
            Vec2(data->footerRect.Right(), topY),
            pal.borderSubtle,
            1.0f
        );

        const keel::f32 footerFontSize = ResolveTableFontSize(UiText{}, TextRole::LabelSmall, theme);
        const keel::usize totalRows = (data->opts.totalRows > 0)
            ? data->opts.totalRows
            : data->emittedRowCount;

        char summaryBuf[96]{};
        keel::StringRef summaryStr{"0 rows"};
        if (totalRows > 0) {
            const keel::usize dispFirst = keel::min(totalRows, data->firstVisibleRow + 1);
            const keel::usize dispLast = keel::min(totalRows, data->endVisibleRow);
            const keel::usize selCount = data->state->selectedRows.size();
            if (selCount > 1) {
                summaryStr = keel::format_buf(
                    summaryBuf,
                    "Rows %zu..%zu of %zu  |  Selected: %zu rows",
                    dispFirst,
                    dispLast,
                    totalRows,
                    selCount
                );
            } else if (data->state->selectedRow >= 0) {
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
            const keel::f32 customFooterFont = ResolveTableFontSize(data->opts.footerText, TextRole::LabelSmall, theme);
            canvas.DrawTextBox(
                data->opts.footerText.text,
                rightRect,
                customFooterFont,
                theme.ResolveColor(data->opts.footerText.color, false, false, node->disabled, pal.textSecondary),
                1.0f,
                0.5f
            );
        }
        canvas.PopClip();
    }

    // 3. Render vertical & horizontal scrollbars on the body viewport
    const ScrollData* sd = node->scroll;
    const VisibilityPolicy sbPol = sd ? sd->policy : VisibilityPolicy::Auto;
    const Vec2 scrollContent = node->GetScrollContentSize();
    const keel::f32 visibleColsW = keel::max(0.0f, data->bodyViewportRect.w - data->scrollbarGutterW);
    const keel::f32 visibleRowsH = keel::max(0.0f, data->bodyViewportRect.h - data->scrollbarGutterH);
    const bool showV = (sbPol == VisibilityPolicy::Always) ||
        (sbPol == VisibilityPolicy::Auto &&
         scrollContent.y > visibleRowsH + 0.5f);
    const bool showH = (sbPol == VisibilityPolicy::Always) ||
        (sbPol == VisibilityPolicy::Auto &&
         scrollContent.x > visibleColsW + 0.5f);

    const ScrollbarMetrics& sm = theme.GetScrollbar(node->uiSize);
    const keel::f32 cornerReserve = (showV && showH) ? (sm.trackWidth + sm.margin) : 0.0f;

    if (showV) {
        detail::ScrollArea::RenderAxisScrollbar(
            canvas,
            data->bodyViewportRect,
            scrollContent.y + data->scrollbarGutterH,
            scrollOff.y,
            false,
            sd ? sd->hoveredY : false,
            sd ? sd->activeY : false,
            node->uiSize,
            theme,
            cornerReserve,
            sd ? sd->partHoveredY : 0u,
            sd ? sd->partPressedY : 0u
        );
    }
    if (showH) {
        detail::ScrollArea::RenderAxisScrollbar(
            canvas,
            data->bodyViewportRect,
            scrollContent.x + data->scrollbarGutterW,
            scrollOff.x,
            true,
            sd ? sd->hoveredX : false,
            sd ? sd->activeX : false,
            node->uiSize,
            theme,
            cornerReserve,
            sd ? sd->partHoveredX : 0u,
            sd ? sd->partPressedX : 0u
        );
    }
    if (showV && showH) {
        const Rect cornerRect(
            keel::floor(data->bodyViewportRect.Right() - sm.trackWidth - sm.margin),
            keel::floor(data->bodyViewportRect.Bottom() - sm.trackWidth - sm.margin),
            sm.trackWidth,
            sm.trackWidth
        );
        theme.Painter().DrawScrollbarCorner(canvas, theme, cornerRect);
    }

    if (clipRounded) {
        canvas.PopClip();
    }

    // 4. Outer surface frame / 3D sunken bevel via active IStylePainter
    const keel::f32 bw = (data->opts.border.width > 0.0f) ? data->opts.border.width : 0.0f;
    if (bw > 0.0f) {
        const Color borderCol = (data->state->resizingCol >= 0)
            ? pal.accent
            : theme.ResolveColor(data->opts.border.color, false, false, node->disabled, pal.border);
        if (borderCol.a > 0) {
            theme.Painter().DrawSurfaceFrame(canvas, theme, SurfacePaintParams{
                .rect = data->frameRect,
                .role = SurfaceRole::Sunken,
                .radius = radius,
                .explicitRadius = data->opts.border.radius.has_value(),
                .border = borderCol,
                .borderWidth = bw,
                .explicitBorder = data->opts.border.HasExplicitStroke()
            });
        }
    }
}

} // namespace guikit
