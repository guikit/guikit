#include "guikit/views/tree_view.hpp"
#include "guikit/layout/row.hpp"
#include "guikit/layout/spacer.hpp"
#include "guikit/surfaces/box.hpp"
#include "guikit/actions/button.hpp"
#include "guikit/fields/input_field.hpp"
#include "guikit/display/text.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/element.hpp"
#include "guikit/core/node.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/layout/scroll_area.hpp"
#include "guikit/controls/checkbox.hpp"
#include "keel/arena.h"
#include "keel/core.h"
#include "keel/math.h"

namespace guikit {

struct TreeViewPayload {
    Context* ctx{nullptr};
    Node* outerNode{nullptr};
    Node* headerNode{nullptr};
    Node* scrollNode{nullptr};
    TreeViewState* state{nullptr};
    TreeViewOptions opts{};
    keel::StringRef activeFilter{};
    keel::i32 visibleItemCount{0};
    keel::ArenaArray<VisibleTreeItemEntry> currVisibleItems{};

    keel::u16 columnCount{0};
    keel::StaticArray<TableColumn, TreeViewState::kMaxColumns> columns{};
    keel::StaticArray<keel::f32, TreeViewState::kMaxColumns> resolvedColX{};
    keel::StaticArray<keel::f32, TreeViewState::kMaxColumns> resolvedColW{};
    keel::i32 hoveredDividerCol{-1};
    keel::f32 resolvedColOriginX{0.0f};
    keel::f32 resolvedColTotalW{0.0f};
};

struct TreeCellPayload {
    TableCellOptions opts{};
    Align resolvedAlign{Align::Start};
};

struct TreeItemPayload {
    Context* ctx{nullptr};
    Node* branchNode{nullptr};
    Node* rowNode{nullptr};
    TreeViewPayload* tree{nullptr};
    TreeItemPayload* parentItem{nullptr};
    TreeItemState* state{nullptr};
    TreeItemOptions opts{};
    Id idOuter{Id::Null()};
    Id idRow{Id::Null()};
    keel::u16 depth{0};
    keel::i32 visibleRowIndex{0};
    keel::usize visibleEntryIdx{0};
    bool open{true};
    bool effectiveOpen{true};
    bool selected{false};
    bool hasChildItems{false};
    bool ancestorMatched{false};
    bool matchesFilter{true};
    mutable ICanvas* lastCanvas{nullptr};
};

namespace {

bool ContainsIgnoreCase(keel::StringRef haystack, keel::StringRef needle) noexcept {
    if (needle.empty()) return true;
    return TextUtil::FindCaseInsensitive(haystack, needle) != keel::kInvalidIndex;
}

keel::f32 AlignToFactor(Align align) noexcept {
    switch (align) {
        case Align::Center: return 0.5f;
        case Align::End:    return 1.0f;
        default:            return 0.0f;
    }
}

keel::f32 ResolveTreeHeaderHeight(UiSize size, const UiTheme& theme) noexcept {
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs: return 24.0f;
        case UiSize::Preset::Sm: return 28.0f;
        case UiSize::Preset::Lg: return 36.0f;
        case UiSize::Preset::Xl: return 42.0f;
        case UiSize::Preset::Md:
        case UiSize::Preset::None:
            return 32.0f;
        default:
            return 32.0f * theme.SizeFactor(size);
    }
}

void ResolveTreeColumns(TreeViewPayload& tree, keel::f32 totalW, const UiTheme& /*theme*/) noexcept {
    if (tree.columnCount == 0) return;
    const keel::f32 scale = 1.0f;
    const keel::f32 availW = keel::max(0.0f, totalW);

    keel::f32 fixedSum = 0.0f;
    keel::f32 flexSum = 0.0f;

    for (keel::u16 i = 0; i < tree.columnCount; ++i) {
        const auto& col = tree.columns[i];
        const keel::f32 minW = keel::max(36.0f * scale, col.minWidth * scale);
        const keel::f32 maxW = (col.maxWidth > 0.0f) ? (col.maxWidth * scale) : 100000.0f;

        if (tree.state && tree.state->hasUserColWidth[i]) {
            const keel::f32 w = keel::clamp(tree.state->userColWidths[i] * scale, minW, maxW);
            tree.resolvedColW[i] = w;
            fixedSum += w;
        } else if (col.width > 0.0f) {
            const keel::f32 w = keel::clamp(col.width * scale, minW, maxW);
            tree.resolvedColW[i] = w;
            fixedSum += w;
        } else {
            tree.resolvedColW[i] = 0.0f;
            flexSum += keel::max(0.1f, col.flex);
        }
    }

    const keel::f32 remainingW = keel::max(0.0f, availW - fixedSum);
    keel::f32 curX = 0.0f;

    for (keel::u16 i = 0; i < tree.columnCount; ++i) {
        const auto& col = tree.columns[i];
        const keel::f32 minW = keel::max(36.0f * scale, col.minWidth * scale);
        const keel::f32 maxW = (col.maxWidth > 0.0f) ? (col.maxWidth * scale) : 100000.0f;

        if (tree.resolvedColW[i] <= 0.0f) {
            const keel::f32 weight = keel::max(0.1f, col.flex);
            const keel::f32 flexShare = (flexSum > 0.0f) ? (remainingW * (weight / flexSum)) : minW;
            tree.resolvedColW[i] = keel::clamp(keel::round(flexShare), minW, maxW);
        }

        if (i + 1 == tree.columnCount && flexSum > 0.0f &&
            !(tree.state && tree.state->hasUserColWidth[i]) &&
            curX + tree.resolvedColW[i] < availW) {
            tree.resolvedColW[i] = keel::min(maxW, availW - curX);
        }

        tree.resolvedColX[i] = curX;
        curX += tree.resolvedColW[i];
    }
}

keel::f32 ResolveIndentStep(const TreeViewPayload* tree, UiSize size, const UiTheme& theme) noexcept {
    if (tree && tree->opts.indent.has_value()) {
        return keel::max(8.0f, *tree->opts.indent);
    }
    const ComponentMetrics& cm = theme.GetMetrics(size);
    return keel::max(14.0f, keel::round(cm.defaultHeight * 0.50f));
}

struct RowMetrics {
    keel::f32 padX{8.0f};
    keel::f32 padY{4.0f};
    keel::f32 gap{6.0f};
    keel::f32 indentStep{18.0f};
    keel::f32 indentWidth{0.0f};
    keel::f32 chevronSlotW{14.0f};
    keel::f32 chevronSize{11.0f};
    keel::f32 checkboxSize{15.0f};
    keel::f32 iconSize{15.0f};
    keel::f32 labelFont{13.0f};
    keel::f32 descFont{11.0f};
    keel::f32 detailFont{12.0f};
    keel::f32 badgeFont{11.0f};
    keel::f32 minRowH{28.0f};
};

RowMetrics ComputeRowMetrics(
    const TreeItemPayload& p,
    UiSize size,
    const UiTheme& theme
) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);

    RowMetrics rm{};
    rm.labelFont = theme.ResolveFontSize(p.opts.label, TextRole::LabelLarge, size);
    rm.descFont = theme.ResolveFontSize(p.opts.description, TextRole::LabelSmall, size);
    rm.detailFont = theme.ResolveFontSize(p.opts.detail, TextRole::LabelMedium, size);
    rm.badgeFont = theme.ResolveFontSize(p.opts.badge, TextRole::LabelSmall, size);
    rm.padX = keel::max(6.0f, keel::round(cm.paddingX * 0.65f));
    rm.padY = keel::max(3.0f, keel::round(cm.paddingY * 0.55f));
    rm.gap = keel::max(5.0f, keel::round(cm.itemGap * 0.85f));
    rm.indentStep = ResolveIndentStep(p.tree, size, theme);
    rm.indentWidth = keel::as<keel::f32>(p.depth) * rm.indentStep;
    rm.chevronSize = keel::max(10.0f, keel::round(rm.labelFont * 0.82f));
    rm.chevronSlotW = keel::max(14.0f, rm.chevronSize + 3.0f);
    rm.checkboxSize = keel::max(14.0f, keel::round(cm.defaultHeight * 0.44f));
    rm.iconSize = keel::max(14.0f, keel::round(rm.labelFont * 1.08f));
    rm.minRowH = keel::max(24.0f, keel::round(cm.defaultHeight * 0.82f));
    return rm;
}

Vec2 MeasureEmbeddedChild(
    const Node* child,
    keel::f32 availRowWidth,
    bool singleChildInRow,
    keel::Optional<keel::f32> rowControlWidth,
    const RowMetrics& rm,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    if (!child || child->isSpacer) return {0.0f, 0.0f};
    const UiTheme& childTheme = child->GetTheme(theme);

    // Container cell (e.g., item.Cell() or ui.Row()) containing multiple grandchildren
    if (child->firstChild != nullptr &&
        (child->type == NodeType::Cell || child->type == NodeType::Row ||
         child->type == NodeType::Column || child->type == NodeType::Box)) {
        keel::f32 sumW = 0.0f;
        keel::f32 maxH = 0.0f;
        keel::u32 gcCount = 0;
        const keel::f32 gap = (child->gap > 0.0f) ? child->gap : rm.gap;
        for (const Node* gc = child->firstChild; gc != nullptr; gc = gc->nextSibling) {
            const Vec2 gm = MeasureEmbeddedChild(gc, availRowWidth, false, keel::kNullOpt, rm, canvas, childTheme);
            if (child->direction == FlexDirection::Column) {
                sumW = keel::max(sumW, gm.x);
                maxH += gm.y;
            } else {
                sumW += gm.x;
                maxH = keel::max(maxH, gm.y);
            }
            ++gcCount;
        }
        if (gcCount > 1) {
            if (child->direction == FlexDirection::Column) {
                maxH += keel::as<keel::f32>(gcCount - 1) * gap;
            } else {
                sumW += keel::as<keel::f32>(gcCount - 1) * gap;
            }
        }
        return {sumW + child->padding.Horizontal(), maxH + child->padding.Vertical()};
    }

    Vec2 intr = child->measureFn
        ? child->measureFn(child, 0.0f, canvas, childTheme)
        : child->intrinsicSize;
    if (child->widthMode == SizeMode::Fixed && child->fixedWidth > 0.0f) {
        intr.x = child->fixedWidth;
    }
    if (child->heightMode == SizeMode::Fixed && child->fixedHeight > 0.0f) {
        intr.y = child->fixedHeight;
    }

    const bool isPlainOrBadgeElement = (child->type == NodeType::Element && child->firstChild == nullptr);
    keel::f32 cw = intr.x;
    if (rowControlWidth.has_value() && *rowControlWidth > 0.0f && singleChildInRow) {
        cw = *rowControlWidth;
    } else if (child->widthMode == SizeMode::Fixed && child->fixedWidth > 0.0f) {
        cw = child->fixedWidth;
    } else if (child->widthMode == SizeMode::Stretch && !isPlainOrBadgeElement) {
        cw = keel::max(intr.x, 130.0f);
        if (availRowWidth > 0.0f) {
            cw = keel::min(cw, availRowWidth * 0.42f);
        }
    }
    return {cw, intr.y};
}

Vec2 MeasureControlsSlot(
    const Node* rowNode,
    const TreeItemPayload& p,
    keel::f32 availRowWidth,
    keel::f32 /*leftContentWidth*/,
    const RowMetrics& rm,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    if (!rowNode || !rowNode->firstChild) {
        return {0.0f, 0.0f};
    }

    keel::f32 totalW = 0.0f;
    keel::f32 maxH = 0.0f;
    keel::u32 count = 0;
    const bool singleChild = (rowNode->childCount == 1);

    for (const Node* child = rowNode->firstChild; child != nullptr; child = child->nextSibling) {
        const Vec2 cm = MeasureEmbeddedChild(child, availRowWidth, singleChild, p.opts.controlWidth, rm, canvas, theme);
        totalW += cm.x;
        maxH = keel::max(maxH, cm.y);
        ++count;
    }

    if (count > 1) {
        totalW += keel::as<keel::f32>(count - 1) * rm.gap;
    }
    return {totalW, maxH};
}

TreeViewPayload* FindEnclosingTreeView(Context& ctx) noexcept {
    for (Node* curr = ctx.GetCurrentContainer(); curr != nullptr; curr = curr->parent) {
        if (curr->type == NodeType::ScrollArea && curr->parent && curr->parent->type == NodeType::TreeView) {
            return curr->parent->As<TreeViewPayload>();
        }
        if (curr->type == NodeType::TreeView) {
            return curr->As<TreeViewPayload>();
        }
    }
    return nullptr;
}

} // namespace

TreeViewScope::~TreeViewScope() noexcept {
    Finalize();
}

void TreeViewScope::Finalize() noexcept {
    if (!m_payload || !m_ctx) {
        return;
    }
    TreeViewPayload* p = m_payload;
    m_payload = nullptr;

    if (!p->activeFilter.empty() && p->visibleItemCount == 0 && m_node) {
        guikit::Text(*m_ctx, "No matching items", {.role = TextRole::LabelMedium, .variant = UiVariant::Muted});
    }

    if (p->state) {
        p->state->prevVisibleItems.clear();
        p->state->prevVisibleItems.reserve(p->currVisibleItems.size());
        for (const VisibleTreeItemEntry& entry : p->currVisibleItems) {
            p->state->prevVisibleItems.push_back(entry);
        }
    }
}

void TreeViewScope::ExpandAll() noexcept {
    if (m_payload && m_payload->state) {
        m_payload->state->bulkToggleGen++;
        m_payload->state->bulkToggleOpen = true;
        if (m_ctx) m_ctx->InvalidateAll();
    }
}

void TreeViewScope::CollapseAll() noexcept {
    if (m_payload && m_payload->state) {
        m_payload->state->bulkToggleGen++;
        m_payload->state->bulkToggleOpen = false;
        if (m_ctx) m_ctx->InvalidateAll();
    }
}

keel::StringRef TreeViewScope::GetFilterText() const noexcept {
    return m_payload ? m_payload->activeFilter : keel::StringRef{};
}

keel::i32 TreeViewScope::VisibleItemCount() const noexcept {
    return m_payload ? m_payload->visibleItemCount : 0;
}

keel::i32 TreeViewScope::GetSelectedIndex() const noexcept {
    if (!m_payload) return -1;
    if (m_payload->opts.selectedIndex) return *m_payload->opts.selectedIndex;
    return m_payload->state ? m_payload->state->selectedIndex : -1;
}

Id TreeViewScope::GetSelectedId() const noexcept {
    if (!m_payload) return Id::Null();
    if (m_payload->opts.selectedId) return *m_payload->opts.selectedId;
    return m_payload->state ? m_payload->state->selectedId : Id::Null();
}

TreeItemScope::~TreeItemScope() noexcept {
    Finalize();
}

ContainerScope TreeItemScope::Cell(
    const TableCellOptions& opts,
    keel::SourceLocation loc
) {
    if (!m_visible || !m_ctx || !m_node) {
        return ContainerScope(nullptr, nullptr);
    }
    const Id cellId = m_ctx->MakeChildId(loc);
    const UiSize activeSize = m_node->uiSize;
    const UiTheme& theme = m_ctx->GetTheme();
    const ComponentMetrics& cm = theme.GetMetrics(activeSize);

    Node* cellNode = m_ctx->CreateNode(NodeType::Cell, cellId);
    cellNode->direction = FlexDirection::Row;
    cellNode->gap = keel::max(4.0f, keel::round(cm.itemGap * 0.85f)) * cellNode->GetDensity();
    cellNode->align = Alignment::Center;
    cellNode->widthMode = SizeMode::Fit;
    cellNode->heightMode = SizeMode::Fit;
    cellNode->uiSize = activeSize;

    auto* cp = m_ctx->AllocPayload<TreeCellPayload>();
    cp->opts = opts;
    cellNode->SetPayload(cp);

    m_ctx->PushContainer(cellNode);
    return ContainerScope(m_ctx, cellNode);
}

void TreeItemScope::Finalize() noexcept {
    if (!m_payload) {
        return;
    }
    TreeItemPayload* p = m_payload;
    m_payload = nullptr;

    if (p->rowNode) {
        keel::u64 h = detail::HashArgs(p->rowNode->visualHash, p->hasChildItems);
        for (const Node* ch = p->rowNode->firstChild; ch != nullptr; ch = ch->nextSibling) {
            h = detail::HashArgs(h, ch->id.value, ch->visualHash, ch->hovered, ch->pressed, ch->checked);
            for (const Node* gc = ch->firstChild; gc != nullptr; gc = gc->nextSibling) {
                h = detail::HashArgs(h, gc->id.value, gc->visualHash, gc->hovered, gc->pressed, gc->checked);
            }
        }
        p->rowNode->visualHash = h;
    }

    if (p->tree && p->visibleEntryIdx < p->tree->currVisibleItems.size()) {
        p->tree->currVisibleItems[p->visibleEntryIdx].isBranch = p->hasChildItems || p->opts.forceBranch;
    }

    const bool filterActive = (p->tree != nullptr && !p->tree->activeFilter.empty());
    if (filterActive && !p->matchesFilter && p->rowNode) {
        const keel::StringRef q = p->tree->activeFilter;
        auto nodeMatches = [&](const Node* n) noexcept -> bool {
            if (!n) return false;
            if (!n->text.empty() && ContainsIgnoreCase(n->text, q)) return true;
            if (n->type == NodeType::Element) {
                if (const auto* ep = n->As<ElementPayload>()) {
                    if (ContainsIgnoreCase(ep->opts.label.text, q) ||
                        ContainsIgnoreCase(ep->opts.description.text, q) ||
                        ContainsIgnoreCase(ep->opts.badge.text, q)) {
                        return true;
                    }
                }
            }
            return false;
        };
        for (const Node* ch = p->rowNode->firstChild; ch != nullptr && !p->matchesFilter; ch = ch->nextSibling) {
            if (nodeMatches(ch)) {
                p->matchesFilter = true;
            } else if (ch->type == NodeType::Cell || ch->type == NodeType::Row ||
                       ch->type == NodeType::Column || ch->type == NodeType::Box) {
                for (const Node* gc = ch->firstChild; gc != nullptr; gc = gc->nextSibling) {
                    if (nodeMatches(gc)) {
                        p->matchesFilter = true;
                        break;
                    }
                }
            }
        }
        if (p->matchesFilter) {
            for (TreeItemPayload* anc = p->parentItem; anc != nullptr; anc = anc->parentItem) {
                anc->matchesFilter = true;
            }
        }
    }

    if (filterActive && !p->matchesFilter) {
        if (p->branchNode && p->branchNode->parent) {
            p->branchNode->parent->RemoveChild(p->branchNode);
        }
        if (p->tree) {
            if (p->visibleEntryIdx < p->tree->currVisibleItems.size()) {
                p->tree->currVisibleItems.resize(p->visibleEntryIdx);
            }
            p->tree->visibleItemCount = p->visibleRowIndex;
        }
    }
}

TreeViewScope detail::TreeView::Open(
    Context& ctx,
    const TreeViewOptions& opts,
    keel::SourceLocation loc
) {
    const Id idTree = ctx.MakeChildId(loc);
    const Id idScroll = Id{detail::HashArgs(idTree, 0x72EE'5C80'0001ULL)};

    const UiSize activeSize = opts.size;
    Node* outerNode = ctx.CreateNode(NodeType::TreeView, idTree, opts.disabled);
    const UiTheme& theme = outerNode->GetTheme(ctx.GetTheme());
    const auto& pal = theme.palette;
    const keel::f32 density = outerNode->GetDensity();
    const bool activeDisabled = outerNode->disabled;

    TreeViewState& st = ctx.GetOrCreatePersistentState<TreeViewState>(idTree);
    if (opts.selectedIndex != nullptr) {
        st.selectedIndex = *opts.selectedIndex;
    }
    if (opts.selectedId != nullptr) {
        st.selectedId = *opts.selectedId;
    }

    // Handle keyboard navigation across visible tree items from previous frame
    const Rect prevTreeBounds = ctx.GetPrevFrameBounds(idTree);
    const bool treeHovered = prevTreeBounds.w > 0.0f && ctx.IsRectHovered(idTree, prevTreeBounds);
    bool anyRowFocused = ctx.IsFocused(idTree);
    bool anyRowFocusVisible = ctx.IsFocusVisible(idTree);
    keel::i32 currentVisIdx = -1;

    for (keel::usize i = 0; i < st.prevVisibleItems.size(); ++i) {
        const auto& entry = st.prevVisibleItems[i];
        if (ctx.IsFocused(entry.idRow)) {
            anyRowFocused = true;
            if (ctx.IsFocusVisible(entry.idRow)) {
                anyRowFocusVisible = true;
            }
        }
        const bool matchId = st.selectedId.IsValid() && entry.idOuter == st.selectedId;
        const bool matchIdx = (st.selectedIndex >= 0 && entry.index == st.selectedIndex);
        if (matchId || matchIdx) {
            currentVisIdx = keel::as<keel::i32>(i);
        }
    }

    if (!activeDisabled && (anyRowFocused || treeHovered) && !st.prevVisibleItems.empty()) {
        auto selectVisibleAt = [&](keel::i32 visIdx) {
            visIdx = keel::clamp(visIdx, 0, keel::as<keel::i32>(st.prevVisibleItems.size()) - 1);
            const auto& target = st.prevVisibleItems[keel::as<keel::usize>(visIdx)];
            st.selectedId = target.idOuter;
            st.selectedIndex = target.index;
            if (opts.selectedId) *opts.selectedId = target.idOuter;
            if (opts.selectedIndex) *opts.selectedIndex = target.index;
            ctx.RequestFocus(target.idRow, FocusReason::Keyboard);
            ctx.ScrollToId(target.idRow);
            ctx.InvalidateAll();
        };

        auto setBranchOpen = [&](const VisibleTreeItemEntry& entry, bool newOpen) {
            if (entry.externalOpen) {
                *entry.externalOpen = newOpen;
            }
            ctx.GetOrCreatePersistentState<TreeItemState>(entry.idOuter, TreeItemState{.open = entry.defaultOpen}).open = newOpen;
            ctx.InvalidateAll();
        };

        if (ctx.ConsumeKey(Key::Down)) {
            selectVisibleAt((currentVisIdx < 0) ? 0 : (currentVisIdx + 1));
        } else if (ctx.ConsumeKey(Key::Up)) {
            selectVisibleAt((currentVisIdx <= 0) ? 0 : (currentVisIdx - 1));
        } else if (ctx.ConsumeKey(Key::Home)) {
            selectVisibleAt(0);
        } else if (ctx.ConsumeKey(Key::End)) {
            selectVisibleAt(keel::as<keel::i32>(st.prevVisibleItems.size()) - 1);
        } else if (currentVisIdx >= 0 && keel::as<keel::usize>(currentVisIdx) < st.prevVisibleItems.size()) {
            const auto& cur = st.prevVisibleItems[keel::as<keel::usize>(currentVisIdx)];
            if (ctx.ConsumeKey(Key::Right)) {
                if (cur.isBranch && !cur.isOpen) {
                    setBranchOpen(cur, true);
                } else if (cur.isBranch && cur.isOpen &&
                           keel::as<keel::usize>(currentVisIdx + 1) < st.prevVisibleItems.size() &&
                           st.prevVisibleItems[keel::as<keel::usize>(currentVisIdx + 1)].parentIdOuter == cur.idOuter) {
                    selectVisibleAt(currentVisIdx + 1);
                }
            } else if (ctx.ConsumeKey(Key::Left)) {
                if (cur.isBranch && cur.isOpen) {
                    setBranchOpen(cur, false);
                } else if (cur.parentIdOuter.IsValid()) {
                    for (keel::usize i = 0; i < st.prevVisibleItems.size(); ++i) {
                        if (st.prevVisibleItems[i].idOuter == cur.parentIdOuter) {
                            selectVisibleAt(keel::as<keel::i32>(i));
                            break;
                        }
                    }
                }
            } else if (ctx.ConsumeKey(Key::Enter)) {
                if (cur.isBranch) {
                    setBranchOpen(cur, !cur.isOpen);
                }
            } else if (ctx.ConsumeKey(Key::Space)) {
                if (cur.externalChecked) {
                    *cur.externalChecked = !(*cur.externalChecked);
                    ctx.InvalidateAll();
                } else if (cur.isBranch) {
                    setBranchOpen(cur, !cur.isOpen);
                }
            }
        }
    }

    outerNode->direction = FlexDirection::Column;
    outerNode->gap = 0.0f;
    outerNode->uiSize = activeSize;

    outerNode->widthMode = Control::ResolveWidthMode(ctx, opts.width, SizeMode::Stretch);
    if (opts.width.IsFixed()) {
        outerNode->widthMode = SizeMode::Fixed;
        outerNode->fixedWidth = opts.width.Dp();
    }

    if (opts.height.IsFixed()) {
        outerNode->heightMode = SizeMode::Fixed;
        outerNode->fixedHeight = opts.height.Dp();
    } else if (opts.height.ResolveMode(SizeMode::Stretch) == SizeMode::Fit) {
        outerNode->heightMode = SizeMode::Fit;
    } else if (outerNode->parent &&
               (outerNode->parent->heightMode == SizeMode::Stretch || outerNode->parent->heightMode == SizeMode::Fixed)) {
        outerNode->heightMode = SizeMode::Stretch;
    } else {
        outerNode->heightMode = SizeMode::Fit;
    }

    if (outerNode->parent && outerNode->parent->type == NodeType::Element) {
        Control::SetupNode(
            ctx,
            outerNode,
            ControlSpec{
                .width = opts.width,
                .height = opts.height.IsFixed() ? opts.height : (opts.height.HasCustomStretchWeight() ? opts.height : AxisSize(outerNode->heightMode)),
                .defaultWidthMode = SizeMode::Stretch,
                .defaultHeightMode = SizeMode::Stretch,
                .defaultLayout = ElementLayout::Top
            }
        );
    } else {
        outerNode->flexGrow = Control::ResolveFlexGrow(outerNode->parent, opts.width, opts.height, 1.0f);
    }

    outerNode->SetSurfaceRole((opts.border.width > 0.0f) ? SurfaceRole::Sunken : SurfaceRole::None);
    outerNode->cornerRadius = opts.border.radius.has_value()
        ? *opts.border.radius
        : theme.GetRadius(activeSize);
    outerNode->borderWidth = (opts.border.width > 0.0f) ? opts.border.width : 0.0f;
    if (outerNode->borderWidth > 0.0f) {
        outerNode->borderColor = opts.border.color.Resolve(
            treeHovered,
            anyRowFocusVisible,
            activeDisabled,
            pal.borderSubtle,
            pal.borderHover,
            pal.focusRing,
            pal.borderSubtle.WithAlphaF(theme.disabledOpacity)
        );
    }
    if (opts.background.IsNone()) {
        outerNode->backgroundColor = Color::Transparent();
    } else if (!opts.background.HasAny() || opts.background.IsSurface()) {
        outerNode->backgroundColor = opts.background.IsSurface()
            ? pal.surface.ScaleAlpha(opts.background.alphaScale)
            : pal.surface;
    } else {
        outerNode->backgroundColor = theme.ResolveColor(
            opts.background,
            treeHovered,
            anyRowFocusVisible,
            activeDisabled,
            pal.surface,
            pal.surfaceHover,
            pal.surfaceActive,
            pal.surface.WithAlphaF(theme.disabledOpacity)
        );
    }

    auto* payload = ctx.AllocPayload<TreeViewPayload>();
    payload->ctx = &ctx;
    payload->outerNode = outerNode;
    payload->state = &st;
    payload->currVisibleItems.bind_arena(ctx.GetArena());
    payload->opts = opts;
    payload->opts.title = ctx.AllocText(opts.title);

    const keel::u16 colCount = keel::as<keel::u16>(
        keel::min(opts.columns.size(), TreeViewState::kMaxColumns)
    );
    payload->columnCount = colCount;
    for (keel::u16 i = 0; i < colCount; ++i) {
        payload->columns[i] = opts.columns[i];
        payload->columns[i].title = ctx.AllocText(opts.columns[i].title);
        if (!opts.columns[i].icon.GetPath().empty()) {
            payload->columns[i].icon = Icon::File(ctx.AllocString(opts.columns[i].icon.GetPath()));
        }
    }
    if (st.userColCount != colCount) {
        st.userColCount = colCount;
        st.hasUserColWidth.fill(false);
        st.userColWidths.fill(0.0f);
        st.resizingCol = -1;
    }

    // Handle column divider hover, drag resize, and double-click reset
    if (colCount > 0 && opts.stickyHeader && opts.resizableColumns && !activeDisabled &&
        st.lastHeaderRect.w > 0.0f && st.lastHeaderRect.h > 0.0f) {
        constexpr keel::f32 scale = 1.0f;
        const keel::f32 gripHalfW = 6.0f;
        const keel::f32 prevOriginX = (st.lastColTotalW > 0.0f) ? st.lastColOriginX : st.lastHeaderRect.x;
        const keel::f32 prevTotalW = (st.lastColTotalW > 0.0f) ? st.lastColTotalW : st.lastHeaderRect.w;
        ResolveTreeColumns(*payload, prevTotalW, theme);

        const Vec2 mousePos = ctx.GetMousePos();
        const bool mouseDown = ctx.IsMouseDown();
        const bool mouseClicked = ctx.IsMouseClicked();
        const Rect headerHitRect(
            st.lastHeaderRect.x - gripHalfW,
            st.lastHeaderRect.y,
            st.lastHeaderRect.w + gripHalfW * 2.0f,
            st.lastHeaderRect.h
        );

        if (st.resizingCol >= 0) {
            if (mouseDown && st.resizingCol < keel::as<keel::i32>(colCount)) {
                const keel::u16 colIdx = keel::as<keel::u16>(st.resizingCol);
                const keel::f32 minW = keel::max(36.0f * scale, payload->columns[colIdx].minWidth * scale);
                const keel::f32 maxW = (payload->columns[colIdx].maxWidth > 0.0f)
                    ? (payload->columns[colIdx].maxWidth * scale)
                    : 100000.0f;
                const keel::f32 newW = keel::clamp(st.resizeStartWidth + (mousePos.x - st.resizeStartX), minW, maxW);
                st.userColWidths[colIdx] = newW / scale;
                st.hasUserColWidth[colIdx] = true;
                ResolveTreeColumns(*payload, prevTotalW, theme);
                ctx.InvalidateAll();
            } else {
                st.resizingCol = -1;
                if (ctx.GetActiveId() == idTree) {
                    ctx.SetActiveId(Id::Null());
                }
                ctx.InvalidateAll();
            }
        } else if (headerHitRect.Contains(mousePos) && !ctx.IsMouseOccluded(idTree, mousePos)) {
            const keel::f32 localX = mousePos.x - prevOriginX;
            for (keel::u16 i = 0; i + 1 < colCount; ++i) {
                if (!payload->columns[i].resizable) continue;
                const keel::f32 divX = payload->resolvedColX[i] + payload->resolvedColW[i];
                if (keel::abs(localX - divX) <= gripHalfW) {
                    payload->hoveredDividerCol = keel::as<keel::i32>(i);
                    if (mouseClicked) {
                        const keel::f64 now = (ctx.GetCurrentTime)();
                        if (st.lastDividerCol == keel::as<keel::i32>(i) && (now - st.lastDividerClickTime) <= 0.35) {
                            st.hasUserColWidth.fill(false);
                            st.resizingCol = -1;
                            ResolveTreeColumns(*payload, prevTotalW, theme);
                        } else {
                            // Snapshot columns 0..i so the left edge of column `i` stays anchored during drag
                            for (keel::u16 j = 0; j <= i; ++j) {
                                if (!st.hasUserColWidth[j]) {
                                    st.userColWidths[j] = payload->resolvedColW[j] / scale;
                                    st.hasUserColWidth[j] = true;
                                }
                            }
                            st.resizingCol = keel::as<keel::i32>(i);
                            st.resizeStartX = mousePos.x;
                            st.resizeStartWidth = payload->resolvedColW[i];
                            ctx.SetActiveId(idTree);
                        }
                        st.lastDividerClickTime = now;
                        st.lastDividerCol = keel::as<keel::i32>(i);
                        ctx.InvalidateAll();
                    }
                    break;
                }
            }
        }
    }

    outerNode->SetPayload(payload);
    outerNode->postRenderFn = &detail::TreeView::RenderOverlay;

    ctx.FinishContainer(outerNode);

    if (!opts.title.empty() || opts.searchable || opts.showToolbar) {
        const ComponentMetrics& cm = theme.GetMetrics(activeSize);
        const keel::f32 barPad = keel::max(4.0f, keel::round(cm.paddingY * 0.85f));
        const keel::f32 barGap = keel::max(4.0f, keel::round(cm.itemGap * 0.85f));
        if (auto bar = guikit::Row(ctx, {
            .gap = barGap,
            .padding = Padding(barPad, barPad, barPad, barPad),
            .align = Alignment::Center
        })) {
            if (!opts.title.empty()) {
                guikit::Text(ctx, opts.title, {
                    .role = TextRole::LabelSmall,
                    .variant = UiVariant::Secondary
                });
                if (!opts.searchable) {
                    guikit::Spacer(ctx);
                }
            }
            if (opts.searchable) {
                if (opts.filterText.valid()) {
                    st.filterText.assign(opts.filterText.as_ref());
                }
                if (guikit::InputField(ctx, st.filterText, {
                    .placeholder = opts.searchPlaceholder,
                    .prefix = Icon::Search(),
                    .clearable = true,
                    .disabled = activeDisabled,
                    .width = SizeMode::Stretch
                }).changed) {
                    if (opts.filterText.valid()) {
                        opts.filterText.assign(st.filterText.as_ref());
                    }
                    ctx.InvalidateAll();
                }
            } else if (opts.title.empty()) {
                guikit::Spacer(ctx);
            }

            if (opts.showToolbar) {
                if (guikit::Button(ctx, {
                    .text = "Expand",
                    .icon = Icon::ChevronDown(),
                    .tooltip = "Expand all tree branches",
                    .disabled = activeDisabled,
                    .width = SizeMode::Fit,
                    .style = ButtonStyle::Ghost
                })) {
                    st.bulkToggleGen++;
                    st.bulkToggleOpen = true;
                    ctx.InvalidateAll();
                }
                if (guikit::Button(ctx, {
                    .text = "Collapse",
                    .icon = Icon::ChevronRight(),
                    .tooltip = "Collapse all tree branches",
                    .disabled = activeDisabled,
                    .width = SizeMode::Fit,
                    .style = ButtonStyle::Ghost
                })) {
                    st.bulkToggleGen++;
                    st.bulkToggleOpen = false;
                    ctx.InvalidateAll();
                }
            }
        }
        if (auto sep = guikit::Box(ctx, {
            .width = SizeMode::Stretch,
            .height = 1.0f,
            .background = {pal.borderSubtle}
        })) {}
    }

    if (colCount > 0 && opts.stickyHeader) {
        const Id idHeader = Id{detail::HashArgs(idTree, 0x72EE'4EAD'0001ULL)};
        Node* headerNode = ctx.CreateNode(NodeType::Box, idHeader);
        headerNode->widthMode = SizeMode::Stretch;
        headerNode->heightMode = SizeMode::Fixed;
        headerNode->fixedHeight = ResolveTreeHeaderHeight(activeSize, theme);
        headerNode->uiSize = activeSize;
        headerNode->disabled = activeDisabled;
        headerNode->SetPayload(payload);
        headerNode->renderFn = &detail::TreeView::RenderHeader;
        keel::u64 hHash = detail::HashArgs(colCount, st.resizingCol, payload->hoveredDividerCol, activeDisabled);
        for (keel::u16 i = 0; i < colCount; ++i) {
            hHash = detail::HashArgs(hHash, payload->columns[i].title, st.userColWidths[i], st.hasUserColWidth[i]);
        }
        headerNode->visualHash = hHash;
        payload->headerNode = headerNode;
    }

    const keel::StringRef activeQuery = opts.filterText.valid() ? opts.filterText.as_ref() : st.filterText.as_ref();
    payload->activeFilter = ctx.AllocString(activeQuery);

    Node* scrollNode = ctx.CreateNode(NodeType::ScrollArea, idScroll);
    ctx.PopContainer(outerNode);
    if (opts.height.ResolveMode(SizeMode::Stretch) == SizeMode::Fit) {
        outerNode->heightMode = SizeMode::Fit;
        if (!outerNode->parent || outerNode->parent->direction == FlexDirection::Column) {
            outerNode->flexGrow = 0.0f;
        }
    }

    scrollNode->direction = FlexDirection::Column;
    scrollNode->clipChildren = true;
    scrollNode->widthMode = SizeMode::Stretch;
    scrollNode->heightMode = (outerNode->heightMode == SizeMode::Fit) ? SizeMode::Fit : SizeMode::Stretch;
    scrollNode->flexGrow = 1.0f;
    if (opts.gridLines && (!opts.rowGap.has_value() || *opts.rowGap == 2.0f)) {
        scrollNode->gap = 0.0f;
    } else {
        scrollNode->gap = (opts.rowGap.has_value() ? *opts.rowGap : 2.0f) * density;
    }
    if (opts.padding.has_value()) {
        scrollNode->padding = theme.ResolvePadding(*opts.padding, activeSize, density);
    } else if (colCount > 0) {
        scrollNode->padding = opts.gridLines
            ? Padding::All(0.0f)
            : Padding(4.0f * density, 0.0f, 4.0f * density, 0.0f);
    } else {
        scrollNode->padding = Padding::All(6.0f * density);
    }
    scrollNode->scroll = ctx.AllocPayload<ScrollData>();
    scrollNode->scroll->orientation = Orientation::Vertical;
    scrollNode->scroll->policy = opts.scrollbar;
    scrollNode->scroll->solverManaged = true;
    scrollNode->uiSize = activeSize;
    scrollNode->disabled = activeDisabled;
    scrollNode->postRenderFn = &detail::ScrollArea::RenderScrollbar;
    scrollNode->panelHooks = detail::ScrollArea::Hooks();

    auto& scrollState = ctx.GetOrCreatePersistentState<ScrollState>(idScroll);
    scrollNode->scroll->offset = scrollState.offset;
    payload->scrollNode = scrollNode;

    ctx.PushContainer(scrollNode);

    return TreeViewScope(&ctx, scrollNode, payload);
}

TreeItemScope detail::TreeView::OpenItem(
    Context& ctx,
    bool* externalOpen,
    const TreeItemOptions& opts,
    keel::SourceLocation loc
) {
    Node* currContainer = ctx.GetCurrentContainer();
    Node* parentRow = (currContainer && currContainer->type == NodeType::TreeItem) ? currContainer : nullptr;
    auto* parentPayload = parentRow ? parentRow->As<TreeItemPayload>() : nullptr;
    TreeViewPayload* treePayload = parentPayload ? parentPayload->tree : FindEnclosingTreeView(ctx);

    if (parentPayload) {
        parentPayload->hasChildItems = true;
        if (parentPayload->state) {
            parentPayload->state->hasChildItems = true;
        }
        if (!parentPayload->effectiveOpen) {
            return TreeItemScope{};
        }
    }

    Node* targetParent = (parentRow && parentRow->parent) ? parentRow->parent : currContainer;
    const Id parentId = parentPayload ? parentPayload->idOuter : (targetParent ? targetParent->id : Id::Null());
    const Id idOuter = ctx.MakeChildIdForParent(parentId, loc);
    const Id idRow = Id{detail::HashArgs(idOuter, 0x72EE'17E0'0001ULL)};

    const UiSize activeSize = opts.size;
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();

    TreeItemState& st = ctx.GetOrCreatePersistentState<TreeItemState>(idOuter, TreeItemState{.open = opts.defaultOpen});
    if (treePayload && treePayload->state && st.seenBulkToggleGen != treePayload->state->bulkToggleGen) {
        st.seenBulkToggleGen = treePayload->state->bulkToggleGen;
        st.open = treePayload->state->bulkToggleOpen;
        if (externalOpen) {
            *externalOpen = st.open;
        }
    }

    bool isOpen = (externalOpen != nullptr) ? *externalOpen : st.open;
    const bool isBranch = st.hasChildItems || opts.forceBranch;

    // Check if mouse is over embedded trailing controls from previous frame
    const Vec2 mousePos = ctx.GetMousePos();
    const bool overEmbeddedControls = (st.controlsRect.w > 0.0f && st.controlsRect.h > 0.0f && st.controlsRect.Contains(mousePos));

    Response res{};
    res.id = idRow;
    res.ctx = &ctx;
    if (overEmbeddedControls) {
        res.rect = ctx.GetPrevFrameBounds(idRow);
        const bool occluded = ctx.IsMouseOccluded(idRow, mousePos);
        res.hovered = !occluded && res.rect.Contains(mousePos);
    } else {
        res = ctx.Interact(idRow);
    }

    bool wasToggled = false;
    bool wasCheckChanged = false;
    const keel::i32 itemTraversalIndex = treePayload ? treePayload->visibleItemCount : 0;

    bool isSelected = opts.selected;
    if (treePayload && treePayload->state) {
        if (treePayload->state->selectedId.IsValid() && treePayload->state->selectedId == idOuter) {
            isSelected = true;
        } else if (!treePayload->state->selectedId.IsValid() &&
                   treePayload->state->selectedIndex >= 0 &&
                   treePayload->state->selectedIndex == itemTraversalIndex) {
            isSelected = true;
        }
    }

    if (activeDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.doubleClicked = false;
        res.rightClicked = false;
    } else {
        const bool clickedChevron = isBranch && st.chevronRect.w > 0.0f && st.chevronRect.Contains(mousePos);
        const bool clickedCheckbox = (opts.checked != nullptr) && st.checkboxRect.w > 0.0f && st.checkboxRect.Contains(mousePos);

        if (res.clicked) {
            ctx.RequestFocus(idRow);
            if (clickedChevron) {
                isOpen = !isOpen;
                wasToggled = true;
                res.changed = true;
            } else if (clickedCheckbox) {
                *opts.checked = !(*opts.checked);
                wasCheckChanged = true;
                res.changed = true;
            } else {
                isSelected = true;
                if (treePayload && treePayload->state) {
                    treePayload->state->selectedId = idOuter;
                    treePayload->state->selectedIndex = itemTraversalIndex;
                    if (treePayload->opts.selectedId) {
                        *treePayload->opts.selectedId = idOuter;
                    }
                    if (treePayload->opts.selectedIndex) {
                        *treePayload->opts.selectedIndex = itemTraversalIndex;
                    }
                }
                if (isBranch && treePayload && treePayload->opts.expandOnSelect) {
                    isOpen = !isOpen;
                    wasToggled = true;
                    res.changed = true;
                }
            }
            ctx.InvalidateAll();
        }

        if (res.doubleClicked && !clickedChevron && !clickedCheckbox && isBranch) {
            isOpen = !isOpen;
            wasToggled = true;
            res.changed = true;
            ctx.InvalidateAll();
        }
    }

    st.open = isOpen;
    if (externalOpen) {
        *externalOpen = isOpen;
    }

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    const bool filterActive = (treePayload != nullptr && !treePayload->activeFilter.empty());
    const bool effectiveOpen = filterActive ? true : isOpen;

    bool selfMatches = true;
    bool ancestorMatched = (parentPayload != nullptr && parentPayload->ancestorMatched);
    if (filterActive) {
        if (ancestorMatched) {
            selfMatches = true;
        } else {
            const keel::StringRef q = treePayload->activeFilter;
            selfMatches = ContainsIgnoreCase(opts.label.text, q) ||
                          ContainsIgnoreCase(opts.description.text, q) ||
                          ContainsIgnoreCase(opts.detail.text, q) ||
                          ContainsIgnoreCase(opts.badge.text, q);
            if (selfMatches) {
                ancestorMatched = true;
            }
        }
        if (selfMatches) {
            for (TreeItemPayload* anc = parentPayload; anc != nullptr; anc = anc->parentItem) {
                anc->matchesFilter = true;
            }
        }
    }

    // Create branchNode under targetParent (NOT under parentRow if nested inside another TreeItem)
    Node* branchNode = ctx.AllocPayload<Node>();
    branchNode->type = NodeType::TreeBranch;
    branchNode->id = idOuter;
    branchNode->zone = ctx.GetZone();
    const keel::f32 itemDensity = branchNode->GetDensity();
    branchNode->direction = FlexDirection::Column;
    branchNode->widthMode = SizeMode::Stretch;
    branchNode->heightMode = SizeMode::Fit;
    branchNode->gap = (treePayload && treePayload->scrollNode) ? treePayload->scrollNode->gap : (2.0f * itemDensity);
    branchNode->uiSize = activeSize;
    branchNode->disabled = activeDisabled;
    branchNode->postRenderFn = &detail::TreeView::RenderBranchGuides;
    if (targetParent) {
        targetParent->AddChild(branchNode);
    }

    // Create rowNode as child #0 of branchNode
    Node* rowNode = ctx.AllocPayload<Node>();
    rowNode->type = NodeType::TreeItem;
    rowNode->id = idRow;
    rowNode->zone = branchNode->zone;
    rowNode->uiSize = activeSize;
    rowNode->disabled = activeDisabled;
    rowNode->hovered = res.hovered;
    rowNode->pressed = res.pressed;
    rowNode->focused = ctx.IsFocusVisible(idRow);
    rowNode->checked = isOpen;
    rowNode->active = isSelected;
    rowNode->widthMode = SizeMode::Stretch;
    rowNode->heightMode = SizeMode::Fit;
    branchNode->AddChild(rowNode);

    auto* payload = ctx.AllocPayload<TreeItemPayload>();
    payload->ctx = &ctx;
    payload->branchNode = branchNode;
    payload->rowNode = rowNode;
    payload->tree = treePayload;
    payload->parentItem = parentPayload;
    payload->state = &st;
    payload->opts = opts;
    payload->opts.label = ctx.AllocText(opts.label);
    payload->opts.description = ctx.AllocText(opts.description);
    payload->opts.detail = ctx.AllocText(opts.detail);
    payload->opts.badge = ctx.AllocText(opts.badge);
    payload->opts.tooltip = ctx.AllocText(opts.tooltip);
    if (!opts.icon.GetPath().empty()) {
        payload->opts.icon = Icon::File(ctx.AllocString(opts.icon.GetPath()));
    }
    if (!opts.openIcon.GetPath().empty()) {
        payload->opts.openIcon = Icon::File(ctx.AllocString(opts.openIcon.GetPath()));
    }
    payload->idOuter = idOuter;
    payload->idRow = idRow;
    payload->depth = parentPayload ? keel::as<keel::u16>(parentPayload->depth + 1u) : 0u;
    payload->open = isOpen;
    payload->effectiveOpen = effectiveOpen;
    payload->selected = isSelected;
    payload->hasChildItems = false;
    payload->ancestorMatched = ancestorMatched;
    payload->matchesFilter = selfMatches;

    if (treePayload) {
        payload->visibleRowIndex = treePayload->visibleItemCount++;
        payload->visibleEntryIdx = treePayload->currVisibleItems.size();
        treePayload->currVisibleItems.push_back(VisibleTreeItemEntry{
            .idOuter = idOuter,
            .idRow = idRow,
            .parentIdOuter = parentPayload ? parentPayload->idOuter : Id::Null(),
            .index = payload->visibleRowIndex,
            .depth = payload->depth,
            .isBranch = isBranch,
            .isOpen = isOpen,
            .defaultOpen = opts.defaultOpen,
            .externalOpen = externalOpen,
            .externalChecked = opts.checked
        });
    }

    branchNode->SetPayload(payload);
    rowNode->SetPayload(payload);
    rowNode->text = payload->opts.label.text;

    const bool chkVal = (opts.checked != nullptr) ? *opts.checked : false;
    rowNode->visualHash = detail::HashArgs(
        payload->opts.label,
        payload->opts.description,
        payload->opts.detail,
        payload->opts.badge,
        payload->depth,
        isOpen,
        isSelected,
        opts.checked != nullptr,
        chkVal,
        opts.indeterminate,
        opts.forceBranch,
        opts.background,
        opts.border,
        activeDisabled,
        keel::as<keel::u8>(opts.icon.GetType()),
        keel::as<keel::u8>(opts.icon.GetId()),
        keel::as<keel::u8>(opts.openIcon.GetType()),
        keel::as<keel::u8>(opts.openIcon.GetId()),
        opts.color,
        treePayload ? keel::as<keel::u8>(treePayload->opts.selectionStyle) : keel::u8{0},
        treePayload ? treePayload->opts.color : StateColor{},
        treePayload ? treePayload->opts.striped : false,
        treePayload ? treePayload->opts.gridLines : false,
        treePayload ? treePayload->columnCount : keel::u16{0},
        payload->visibleRowIndex & 1
    );

    rowNode->measureFn = &detail::TreeView::MeasureRow;
    rowNode->arrangeFn = &detail::TreeView::ArrangeRow;
    rowNode->renderFn = &detail::TreeView::RenderRow;

    // Reset current-frame hit rects until ArrangeRow computes fresh ones
    st.chevronRect = Rect{};
    st.checkboxRect = Rect{};
    st.controlsRect = Rect{};

    ctx.FinishContainer(rowNode);

    res.focused = rowNode->focused;
    return TreeItemScope(&ctx, rowNode, payload, res, isOpen, wasToggled, isSelected, wasCheckChanged);
}

Vec2 detail::TreeView::MeasureRow(
    const Node* rowNode,
    keel::f32 availableWidth,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    if (!rowNode) return {0.0f, 0.0f};
    const auto* p = rowNode->As<TreeItemPayload>();
    if (!p) return {0.0f, 0.0f};
    p->lastCanvas = canvas;

    const RowMetrics rm = ComputeRowMetrics(*p, rowNode->uiSize, theme);
    const Icon activeIcon = (p->open && p->opts.openIcon.IsValid()) ? p->opts.openIcon : p->opts.icon;

    keel::f32 leftW = rm.padX + rm.indentWidth + rm.chevronSlotW + rm.gap;
    if (p->opts.checked != nullptr) {
        leftW += rm.checkboxSize + rm.gap;
    }
    if (activeIcon.IsValid()) {
        leftW += rm.iconSize + rm.gap;
    }

    keel::f32 textW = 0.0f;
    if (p->opts.label.HasContentOrReserve()) {
        textW = TextUtil::MeasureWidth(p->opts.label, rm.labelFont, canvas);
    }
    if (p->opts.description.HasContentOrReserve()) {
        textW = keel::max(textW, TextUtil::MeasureWidth(p->opts.description, rm.descFont, canvas));
    }
    leftW += textW;

    keel::f32 rightDecorW = 0.0f;
    if (p->opts.detail.HasContentOrReserve()) {
        rightDecorW += rm.gap + TextUtil::MeasureWidth(p->opts.detail, rm.detailFont, canvas);
    }
    if (p->opts.badge.HasContentOrReserve()) {
        const keel::f32 badgePadX = keel::max(5.0f, rm.badgeFont * 0.45f);
        rightDecorW += rm.gap + TextUtil::MeasureWidth(p->opts.badge, rm.badgeFont, canvas) + badgePadX * 2.0f;
    }

    const Vec2 ctrlSlot = MeasureControlsSlot(rowNode, *p, availableWidth, leftW + rightDecorW, rm, canvas, theme);
    keel::f32 totalW = leftW + rightDecorW + rm.padX;
    if (ctrlSlot.x > 0.0f) {
        totalW += rm.gap + ctrlSlot.x;
    }

    const keel::f32 labelLineH = canvas ? canvas->GetLineHeight(rm.labelFont) : (rm.labelFont * 1.28f);
    keel::f32 contentH = keel::max(labelLineH, keel::max(rm.chevronSize, rm.iconSize));
    if (p->opts.checked != nullptr) {
        contentH = keel::max(contentH, rm.checkboxSize);
    }
    if (p->opts.description.HasContentOrReserve()) {
        const keel::f32 descLineH = canvas ? canvas->GetLineHeight(rm.descFont) : (rm.descFont * 1.25f);
        contentH += 2.0f + descLineH;
    }
    contentH = keel::max(contentH, ctrlSlot.y);

    const keel::f32 totalH = keel::max(rm.minRowH, contentH + rm.padY * 2.0f);
    return {totalW, totalH};
}

namespace {

bool IsInteractiveEmbeddedLeaf(const Node* node) noexcept {
    if (!node || node->isSpacer) return false;
    if (node->type == NodeType::Text) return false;
    if (node->type == NodeType::Element && node->firstChild == nullptr) return false;
    if (node->type == NodeType::Cell) {
        return node->firstChild != nullptr;
    }
    return true;
}

void ArrangeEmbeddedContainerChildren(
    Node* containerNode,
    const Rect& cellRect,
    Align colAlign,
    const RowMetrics& rm,
    ICanvas* canvas,
    const UiTheme& theme,
    Rect& ioUnionControls
) noexcept {
    if (!containerNode) return;
    containerNode->computedRect = cellRect;
    if (!containerNode->firstChild) return;

    const Rect inner(
        cellRect.x + containerNode->padding.left,
        cellRect.y + containerNode->padding.top,
        keel::max(0.0f, cellRect.w - containerNode->padding.Horizontal()),
        keel::max(0.0f, cellRect.h - containerNode->padding.Vertical())
    );
    const keel::f32 gap = (containerNode->gap > 0.0f) ? containerNode->gap : rm.gap;

    Align effectiveAlign = colAlign;
    if (containerNode->type == NodeType::Cell) {
        if (const auto* cp = containerNode->As<TreeCellPayload>()) {
            if (cp->opts.align.has_value()) {
                effectiveAlign = *cp->opts.align;
            }
        }
    } else if (containerNode->justify == Justify::Center) {
        effectiveAlign = Align::Center;
    } else if (containerNode->justify == Justify::End) {
        effectiveAlign = Align::End;
    }

    keel::f32 totalW = 0.0f;
    keel::u32 count = 0;
    keel::u32 stretchCount = 0;

    for (Node* gc = containerNode->firstChild; gc != nullptr; gc = gc->nextSibling) {
        if (gc->isSpacer) continue;
        const UiTheme& gcTheme = gc->GetTheme(theme);
        Vec2 intr = gc->measureFn
            ? gc->measureFn(gc, inner.w, canvas, gcTheme)
            : gc->intrinsicSize;
        if (gc->widthMode == SizeMode::Fixed && gc->fixedWidth > 0.0f) {
            intr.x = gc->fixedWidth;
        }
        if (gc->heightMode == SizeMode::Fixed && gc->fixedHeight > 0.0f) {
            intr.y = gc->fixedHeight;
        }
        gc->intrinsicSize = intr;

        if (gc->widthMode == SizeMode::Fixed && gc->fixedWidth > 0.0f) {
            totalW += gc->fixedWidth;
        } else if (gc->widthMode == SizeMode::Stretch) {
            ++stretchCount;
        } else {
            totalW += intr.x;
        }
        ++count;
    }

    const keel::f32 totalGaps = (count > 1) ? (keel::as<keel::f32>(count - 1) * gap) : 0.0f;
    const keel::f32 freeW = keel::max(0.0f, inner.w - totalW - totalGaps);
    const keel::f32 stretchShare = (stretchCount > 0) ? (freeW / keel::as<keel::f32>(stretchCount)) : 0.0f;
    const keel::f32 usedW = (stretchCount > 0) ? inner.w : keel::min(inner.w, totalW + totalGaps);

    keel::f32 curX = inner.x + (inner.w - usedW) * AlignToFactor(effectiveAlign);

    for (Node* gc = containerNode->firstChild; gc != nullptr; gc = gc->nextSibling) {
        if (gc->isSpacer) {
            gc->computedRect = Rect(curX, inner.y, 0.0f, 0.0f);
            continue;
        }
        const UiTheme& gcTheme = gc->GetTheme(theme);
        const Vec2 intr = gc->intrinsicSize;

        keel::f32 cw = intr.x;
        if (gc->widthMode == SizeMode::Fixed && gc->fixedWidth > 0.0f) {
            cw = gc->fixedWidth;
        } else if (gc->widthMode == SizeMode::Stretch) {
            cw = keel::max(intr.x, stretchShare);
        }
        cw = keel::min(cw, keel::max(0.0f, inner.Right() - curX));

        const keel::f32 ch = keel::min(inner.h - 2.0f, keel::max(intr.y, 18.0f));
        const keel::f32 cy = inner.y + (inner.h - ch) * 0.5f;
        const Rect cRect(curX, cy, cw, ch);

        if (gc->type == NodeType::Cell) {
            if (auto* gcp = gc->As<TreeCellPayload>()) {
                gcp->resolvedAlign = effectiveAlign;
            }
        }
        if (gc->arrangeFn) {
            gc->arrangeFn(gc, cRect, gcTheme);
        } else {
            gc->computedRect = cRect;
        }

        if (IsInteractiveEmbeddedLeaf(gc)) {
            ioUnionControls = (ioUnionControls.w <= 0.0f) ? cRect : ioUnionControls.Union(cRect);
        }
        curX += cw + gap;
    }
}

} // namespace

void detail::TreeView::ArrangeRow(
    Node* rowNode,
    const Rect& availableRect,
    const UiTheme& theme
) noexcept {
    if (!rowNode) return;
    rowNode->computedRect = availableRect;

    auto* p = rowNode->As<TreeItemPayload>();
    if (!p || !p->state) return;

    const RowMetrics rm = ComputeRowMetrics(*p, rowNode->uiSize, theme);
    const Rect bounds = availableRect;

    const keel::f32 chevX = bounds.x + rm.padX + rm.indentWidth;
    p->state->chevronRect = Rect(chevX - 2.0f, bounds.y, rm.chevronSlotW + 4.0f, bounds.h);

    keel::f32 curX = chevX + rm.chevronSlotW + rm.gap;
    if (p->opts.checked != nullptr) {
        const keel::f32 chkY = bounds.y + (bounds.h - rm.checkboxSize) * 0.5f;
        p->state->checkboxRect = Rect(curX - 2.0f, chkY - 2.0f, rm.checkboxSize + 4.0f, rm.checkboxSize + 4.0f);
        curX += rm.checkboxSize + rm.gap;
    } else {
        p->state->checkboxRect = Rect{};
    }

    // Multi-Column Mode: Col 0 is Hierarchy, Col 1..N-1 map to direct children of rowNode in order
    if (p->tree && p->tree->columnCount > 0) {
        ResolveTreeColumns(*p->tree, bounds.w, theme);
        p->tree->resolvedColOriginX = bounds.x;
        p->tree->resolvedColTotalW = bounds.w;
        if (p->tree->state) {
            p->tree->state->lastColOriginX = bounds.x;
            p->tree->state->lastColTotalW = bounds.w;
        }

        if (!rowNode->firstChild) {
            p->state->controlsRect = Rect{};
            return;
        }

        Rect unionControls{};
        keel::u16 colIdx = 1;
        for (Node* child = rowNode->firstChild; child != nullptr; child = child->nextSibling, ++colIdx) {
            if (colIdx >= p->tree->columnCount) {
                child->computedRect = Rect{};
                continue;
            }

            const keel::f32 colX = bounds.x + p->tree->resolvedColX[colIdx];
            const keel::f32 colW = p->tree->resolvedColW[colIdx];
            const Align colAlign = p->tree->columns[colIdx].align;
            const Rect innerRect(
                colX + rm.padX,
                bounds.y,
                keel::max(0.0f, colW - rm.padX * 2.0f),
                bounds.h
            );

            if (child->isSpacer) {
                child->computedRect = Rect(innerRect.x, innerRect.y, 0.0f, 0.0f);
                continue;
            }

            // Multi-widget cell container (item.Cell(), ui.Row(), etc.)
            if (child->firstChild != nullptr &&
                (child->type == NodeType::Cell || child->type == NodeType::Row ||
                 child->type == NodeType::Column || child->type == NodeType::Box)) {
                ArrangeEmbeddedContainerChildren(child, innerRect, colAlign, rm, p->lastCanvas, theme, unionControls);
                continue;
            }

            if (child->type == NodeType::Cell) {
                child->computedRect = innerRect;
                continue;
            }

            const UiTheme& childTheme = child->GetTheme(theme);
            Vec2 intr = child->measureFn
                ? child->measureFn(child, innerRect.w, p->lastCanvas, childTheme)
                : child->intrinsicSize;
            if (child->widthMode == SizeMode::Fixed && child->fixedWidth > 0.0f) {
                intr.x = child->fixedWidth;
            }
            if (child->heightMode == SizeMode::Fixed && child->fixedHeight > 0.0f) {
                intr.y = child->fixedHeight;
            }

            // Non-interactive Text or standalone Element (icon+label / pill badge) inside a column
            const bool isPlainOrBadgeElement = (child->type == NodeType::Element && child->firstChild == nullptr);
            if (child->type == NodeType::Text || isPlainOrBadgeElement) {
                const keel::f32 tw = keel::min(innerRect.w, intr.x);
                const keel::f32 th = keel::min(innerRect.h, intr.y);
                Align effAlign = colAlign;
                if (child->type == NodeType::Text && child->align != Alignment::Start) {
                    effAlign = child->align;
                } else if (isPlainOrBadgeElement) {
                    if (const auto* ep = child->As<ElementPayload>(); ep && ep->opts.align != Align::Start) {
                        effAlign = ep->opts.align;
                    }
                }
                const keel::f32 tx = innerRect.x + (innerRect.w - tw) * AlignToFactor(effAlign);
                const keel::f32 ty = innerRect.y + (innerRect.h - th) * 0.5f;
                const Rect tRect(tx, ty, tw, th);
                if (child->arrangeFn) {
                    child->arrangeFn(child, tRect, childTheme);
                } else {
                    child->computedRect = tRect;
                }
                continue;
            }

            // Interactive Control widget inside a column
            keel::f32 cw = intr.x;
            if (child->widthMode == SizeMode::Fixed && child->fixedWidth > 0.0f) {
                cw = keel::min(innerRect.w, child->fixedWidth);
            } else if (child->widthMode == SizeMode::Stretch) {
                cw = innerRect.w;
            } else {
                cw = keel::min(innerRect.w, intr.x);
            }
            const keel::f32 ch = keel::min(innerRect.h - 4.0f, keel::max(intr.y, 18.0f));
            const keel::f32 cx = innerRect.x + (innerRect.w - cw) * AlignToFactor(colAlign);
            const keel::f32 cy = innerRect.y + (innerRect.h - ch) * 0.5f;
            const Rect cRect(cx, cy, cw, ch);

            if (child->arrangeFn) {
                child->arrangeFn(child, cRect, childTheme);
            } else {
                child->computedRect = cRect;
            }
            unionControls = (unionControls.w <= 0.0f) ? cRect : unionControls.Union(cRect);
        }

        p->state->controlsRect = unionControls;
        return;
    }

    if (!rowNode->firstChild) {
        p->state->controlsRect = Rect{};
        return;
    }

    // Single-Column Mode: reserve badge width on the far right, then place embedded controls to the left of badge
    keel::f32 rightEdge = bounds.Right() - rm.padX;
    if (!p->opts.badge.empty()) {
        const keel::f32 badgePadX = keel::max(5.0f, rm.badgeFont * 0.45f);
        const keel::f32 badgeW = TextUtil::MeasureWidth(p->opts.badge, rm.badgeFont, p->lastCanvas) + badgePadX * 2.0f;
        rightEdge -= (badgeW + rm.gap);
    }

    const Vec2 ctrlSlot = MeasureControlsSlot(rowNode, *p, bounds.w, curX, rm, p->lastCanvas, theme);
    keel::f32 ctrlStartX = keel::max(curX, rightEdge - ctrlSlot.x);
    keel::f32 x = ctrlStartX;
    Rect unionControls{};
    const bool singleChild = (rowNode->childCount == 1);

    for (Node* child = rowNode->firstChild; child != nullptr; child = child->nextSibling) {
        if (child->isSpacer) {
            child->computedRect = Rect(x, bounds.y, 0.0f, 0.0f);
            continue;
        }

        const Vec2 cm = MeasureEmbeddedChild(child, bounds.w, singleChild, p->opts.controlWidth, rm, p->lastCanvas, theme);
        const keel::f32 cw = keel::min(cm.x, keel::max(0.0f, rightEdge - x));
        const keel::f32 ch = keel::min(bounds.h - 2.0f, keel::max(cm.y, 18.0f));
        const keel::f32 cy = bounds.y + (bounds.h - ch) * 0.5f;
        const Rect cRect(x, cy, cw, ch);

        if (child->firstChild != nullptr &&
            (child->type == NodeType::Cell || child->type == NodeType::Row ||
             child->type == NodeType::Column || child->type == NodeType::Box)) {
            ArrangeEmbeddedContainerChildren(child, cRect, Align::End, rm, p->lastCanvas, theme, unionControls);
        } else {
            const UiTheme& childTheme = child->GetTheme(theme);
            if (child->type == NodeType::Cell) {
                if (auto* cp = child->As<TreeCellPayload>()) {
                    cp->resolvedAlign = Align::End;
                }
            }
            if (child->arrangeFn) {
                child->arrangeFn(child, cRect, childTheme);
            } else {
                child->computedRect = cRect;
            }
            if (IsInteractiveEmbeddedLeaf(child)) {
                unionControls = (unionControls.w <= 0.0f) ? cRect : unionControls.Union(cRect);
            }
        }
        x += cw + rm.gap;
    }

    p->state->controlsRect = unionControls;
}

void detail::TreeView::RenderRow(
    const Node* rowNode,
    ICanvas& canvas,
    const UiTheme& theme
) noexcept {
    if (!rowNode) return;
    const Rect bounds = rowNode->computedRect;
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const auto* p = rowNode->As<TreeItemPayload>();
    if (!p) return;

    const auto& pal = theme.palette;
    const RowMetrics rm = ComputeRowMetrics(*p, rowNode->uiSize, theme);
    const bool hovered = rowNode->hovered;
    const bool pressed = rowNode->pressed;
    const bool disabled = rowNode->disabled;
    const bool selected = p->selected;
    const bool isBranch = p->hasChildItems || p->opts.forceBranch;

    const TreeSelectionStyle selStyle = p->tree ? p->tree->opts.selectionStyle : TreeSelectionStyle::Indicator;
    const Color accentCol = p->opts.color.HasAny()
        ? p->opts.color.Resolve(hovered, selected, disabled, pal.accent)
        : ((p->tree && p->tree->opts.color.HasAny())
            ? p->tree->opts.color.Resolve(hovered, selected, disabled, pal.accent)
            : pal.accent);
    const bool hasGridLines = (p->tree != nullptr && p->tree->opts.gridLines);
    const keel::f32 radius = p->opts.border.radius.has_value()
        ? *p->opts.border.radius
        : (hasGridLines ? 0.0f : keel::max(4.0f, theme.GetRadius(rowNode->uiSize) * 0.70f));

    // 1. Row background (zebra stripe, custom background, hover, selection)
    Color rowBg = Color::Transparent();
    if (p->tree && p->tree->opts.striped && (p->visibleRowIndex & 1) == 1) {
        rowBg = pal.surfaceHover.WithAlphaF(0.22f);
    }
    if (p->opts.background.HasAny()) {
        rowBg = p->opts.background.Resolve(
            hovered,
            pressed || selected,
            disabled,
            pal.surfaceHover,
            pal.surfaceHover,
            pal.surfaceActive,
            pal.surfaceHover.WithAlphaF(0.30f)
        );
    } else if (selected) {
        const keel::f32 alpha = (selStyle == TreeSelectionStyle::Pill)
            ? (hovered ? 0.26f : 0.20f)
            : (hovered ? 0.20f : 0.14f);
        rowBg = accentCol.WithAlphaF(disabled ? 0.08f : alpha);
    } else if (!disabled && (hovered || pressed)) {
        rowBg = pressed ? pal.surfaceActive.WithAlphaF(0.75f) : pal.surfaceHover.WithAlphaF(0.55f);
    }

    const keel::f32 borderW = (p->opts.border.width > 0.0f) ? p->opts.border.width : 0.0f;
    Color borderCol = Color::Transparent();
    if (borderW > 0.0f) {
        borderCol = p->opts.border.color.Resolve(hovered, selected, disabled, pal.borderSubtle, pal.borderHover, accentCol, pal.borderSubtle);
    }
    if (rowBg.a > 0 || borderW > 0.0f) {
        if (radius > 0.0f) {
            canvas.DrawRoundRect(bounds, radius, rowBg, borderCol, borderW);
        } else {
            canvas.DrawRect(bounds, rowBg);
        }
    }

    if (hasGridLines) {
        const keel::f32 lineY = keel::floor(bounds.Bottom()) - 0.5f;
        canvas.DrawLine({bounds.x, lineY}, {bounds.Right(), lineY}, pal.borderSubtle.WithAlpha(85), 1.0f);
    }

    // Left vertical accent indicator bar when selected in Indicator mode
    if (selected && selStyle == TreeSelectionStyle::Indicator) {
        const keel::f32 barW = 3.0f;
        const keel::f32 barPadY = keel::max(4.0f, bounds.h * 0.18f);
        const Rect barRect(bounds.x + 1.0f, bounds.y + barPadY, barW, keel::max(4.0f, bounds.h - barPadY * 2.0f));
        canvas.DrawRoundRect(barRect, barW * 0.5f, disabled ? accentCol.WithAlphaF(theme.disabledOpacity) : accentCol);
    }

    // Focus ring
    if (rowNode->focused && !disabled) {
        canvas.DrawRoundRect(bounds.Inset(1.0f), radius, Color::Transparent(), pal.focusRing.WithAlphaF(0.75f), 1.0f);
    }

    keel::f32 curX = bounds.x + rm.padX + rm.indentWidth;

    // 2. Chevron (for branch nodes)
    if (isBranch) {
        const Icon chevIcon = p->open ? Icon::ChevronDown() : Icon::ChevronRight();
        const keel::f32 chevX = curX + (rm.chevronSlotW - rm.chevronSize) * 0.5f;
        const keel::f32 chevY = bounds.y + (bounds.h - rm.chevronSize) * 0.5f;
        const bool chevHover = hovered && p->state && p->state->chevronRect.Contains(p->ctx ? p->ctx->GetMousePos() : Vec2{-1.0f, -1.0f});
        Color chevCol = selected ? accentCol : (chevHover ? pal.textPrimary : pal.textSecondary);
        if (disabled) chevCol = chevCol.WithAlphaF(theme.disabledOpacity);
        chevIcon.Draw(canvas, Rect(chevX, chevY, rm.chevronSize, rm.chevronSize), chevCol);
    }
    curX += rm.chevronSlotW + rm.gap;

    // 3. Optional Checkbox (supports checked + indeterminate tri-state)
    if (p->opts.checked != nullptr) {
        const keel::f32 chkY = bounds.y + (bounds.h - rm.checkboxSize) * 0.5f;
        const Rect chkRect(curX, chkY, rm.checkboxSize, rm.checkboxSize);
        const keel::f32 chkRadius = keel::max(3.0f, rm.checkboxSize * 0.22f);
        const bool isChecked = *p->opts.checked;
        const bool isIndet = p->opts.indeterminate;

        Color boxBg = (isChecked || isIndet) ? accentCol : pal.surface;
        Color boxBorder = (isChecked || isIndet) ? accentCol : pal.border;
        if (disabled) {
            boxBg = boxBg.WithAlphaF(theme.disabledOpacity);
            boxBorder = boxBorder.WithAlphaF(theme.disabledOpacity);
        }
        canvas.DrawRoundRect(chkRect, chkRadius, boxBg, boxBorder, 1.25f);

        const Color glyphCol = disabled ? pal.textPrimary.WithAlphaF(theme.disabledOpacity) : Color(255, 255, 255, 255);
        if (isIndet) {
            const keel::f32 dashW = rm.checkboxSize * 0.52f;
            const keel::f32 dashH = 2.0f;
            canvas.DrawRoundRect(
                Rect(chkRect.Center().x - dashW * 0.5f, chkRect.Center().y - dashH * 0.5f, dashW, dashH),
                dashH * 0.5f,
                glyphCol
            );
        } else if (isChecked) {
            Icon::Check().Draw(canvas, chkRect.Inset(2.0f), glyphCol);
        }
        curX += rm.checkboxSize + rm.gap;
    }

    // 4. Optional Icon (supports openIcon when expanded)
    const Icon activeIcon = (p->open && p->opts.openIcon.IsValid()) ? p->opts.openIcon : p->opts.icon;
    if (activeIcon.IsValid()) {
        const keel::f32 iconY = bounds.y + (bounds.h - rm.iconSize) * 0.5f;
        Color iconCol = selected ? accentCol : theme.ResolveTextColor(p->opts.label, UiVariant::Secondary, disabled, hovered, selected);
        if (!p->opts.label.color.HasAny() && !selected) {
            iconCol = hovered ? pal.textPrimary : pal.textSecondary;
            if (disabled) iconCol = iconCol.WithAlphaF(theme.disabledOpacity);
        }
        activeIcon.Draw(canvas, Rect(curX, iconY, rm.iconSize, rm.iconSize), iconCol);
        curX += rm.iconSize + rm.gap;
    }

    // 5. Compute right boundary of Column 0 (or full row in single-column mode)
    const bool multiCol = (p->tree != nullptr && p->tree->columnCount > 0);
    keel::f32 rightLimit = multiCol
        ? (bounds.x + p->tree->resolvedColW[0] - rm.padX)
        : (bounds.Right() - rm.padX);

    // 5a. Trailing Badge (in Col 0 or row right)
    if (!p->opts.badge.empty() && rightLimit > curX + 24.0f) {
        const keel::f32 badgePadX = keel::max(5.0f, rm.badgeFont * 0.45f);
        const keel::f32 badgeTextW = TextUtil::MeasureWidth(p->opts.badge, rm.badgeFont, &canvas);
        const keel::f32 badgeW = badgeTextW + badgePadX * 2.0f;
        const keel::f32 badgeH = keel::max(16.0f, rm.badgeFont + 4.0f);
        const keel::f32 badgeX = rightLimit - badgeW;
        const keel::f32 badgeY = bounds.y + (bounds.h - badgeH) * 0.5f;

        Color badgeBg = selected ? accentCol.WithAlphaF(0.24f) : pal.surfaceHover;
        Color badgeTextCol = selected ? accentCol : pal.textSecondary;
        if (p->opts.badge.color.HasAny()) {
            const Color resolved = p->opts.badge.color.Resolve(hovered, selected, disabled, accentCol);
            badgeBg = resolved.WithAlphaF(disabled ? 0.12f : 0.20f);
            badgeTextCol = resolved;
        } else if (disabled) {
            badgeBg = badgeBg.WithAlphaF(theme.disabledOpacity);
            badgeTextCol = badgeTextCol.WithAlphaF(theme.disabledOpacity);
        }

        const Rect badgeR(badgeX, badgeY, badgeW, badgeH);
        theme.Painter().DrawBadge(canvas, theme, badgeR, badgeBg, Color::Transparent(), 0.0f, badgeH * 0.5f);
        canvas.DrawTextBox(p->opts.badge.text, badgeR, rm.badgeFont, badgeTextCol, 0.5f, 0.5f);
        rightLimit = badgeX - rm.gap;
    }

    // 5b. Embedded trailing controls slot reservation (single-column mode only)
    if (!multiCol && p->state && p->state->controlsRect.w > 0.0f) {
        rightLimit = keel::min(rightLimit, p->state->controlsRect.x - rm.gap);
    }

    // 5c. Detail metadata text (to the left of controls/badge)
    if (!p->opts.detail.empty() && rightLimit > curX + 40.0f) {
        const keel::f32 detailW = keel::min(
            TextUtil::MeasureWidth(p->opts.detail, rm.detailFont, &canvas),
            keel::max(0.0f, (rightLimit - curX) * 0.45f)
        );
        if (detailW > 12.0f) {
            const keel::f32 detailX = rightLimit - detailW;
            const Color detailCol = theme.ResolveTextColor(p->opts.detail, UiVariant::Muted, disabled, hovered, selected);
            const auto displayDetail = TextUtil::TruncateWithEllipsis(p->opts.detail.text, detailW, rm.detailFont, &canvas);
            canvas.DrawTextBox(displayDetail, Rect(detailX, bounds.y, detailW, bounds.h), rm.detailFont, detailCol, 1.0f, 0.5f);
            rightLimit = detailX - rm.gap;
        }
    }

    // 6. Primary Label + optional Description
    const keel::f32 availTextW = keel::max(0.0f, rightLimit - curX);
    if (availTextW > 2.0f) {
        const Color labelCol = theme.ResolveTextColor(
            p->opts.label,
            selected ? UiVariant::Default : UiVariant::Default,
            disabled,
            hovered,
            selected
        );
        const bool hasDesc = !p->opts.description.empty();
        if (!hasDesc) {
            const auto displayLabel = TextUtil::TruncateWithEllipsis(p->opts.label.text, availTextW, rm.labelFont, &canvas);
            canvas.DrawTextBox(displayLabel, Rect(curX, bounds.y, availTextW, bounds.h), rm.labelFont, labelCol, 0.0f, 0.5f);
        } else {
            const keel::f32 labelLineH = canvas.GetLineHeight(rm.labelFont);
            const keel::f32 descLineH = canvas.GetLineHeight(rm.descFont);
            const keel::f32 lineGap = 2.0f;
            const keel::f32 totalTextH = labelLineH + lineGap + descLineH;
            const keel::f32 startY = bounds.y + (bounds.h - totalTextH) * 0.5f;

            const auto displayLabel = TextUtil::TruncateWithEllipsis(p->opts.label.text, availTextW, rm.labelFont, &canvas);
            canvas.DrawTextBox(displayLabel, Rect(curX, startY, availTextW, labelLineH), rm.labelFont, labelCol, 0.0f, 0.5f);

            const Color descCol = theme.ResolveTextColor(p->opts.description, UiVariant::Muted, disabled, hovered, selected);
            const auto displayDesc = TextUtil::TruncateWithEllipsis(p->opts.description.text, availTextW, rm.descFont, &canvas);
            canvas.DrawTextBox(displayDesc, Rect(curX, startY + labelLineH + lineGap, availTextW, descLineH), rm.descFont, descCol, 0.0f, 0.5f);
        }
    }
}

void detail::TreeView::RenderHeader(
    const Node* headerNode,
    ICanvas& canvas,
    const UiTheme& theme
) noexcept {
    if (!headerNode) return;
    const Rect headerRect = headerNode->computedRect;
    if (headerRect.w <= 0.0f || headerRect.h <= 0.0f) return;

    auto* tree = headerNode->As<TreeViewPayload>();
    if (!tree || tree->columnCount == 0) return;

    const auto& pal = theme.palette;
    constexpr keel::f32 scale = 1.0f;
    const ComponentMetrics& cm = theme.GetMetrics(headerNode->uiSize);
    const keel::f32 cellPadX = keel::max(6.0f, keel::round(cm.paddingX * 0.65f));
    const keel::f32 headerFontSize = theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, headerNode->uiSize);

    const keel::f32 colOriginX = (tree->resolvedColTotalW > 0.0f) ? tree->resolvedColOriginX : headerRect.x;
    const keel::f32 colTotalW = (tree->resolvedColTotalW > 0.0f) ? tree->resolvedColTotalW : headerRect.w;
    ResolveTreeColumns(*const_cast<TreeViewPayload*>(tree), colTotalW, theme);

    if (tree->state) {
        tree->state->lastHeaderRect = headerRect;
        tree->state->lastColOriginX = colOriginX;
        tree->state->lastColTotalW = colTotalW;
    }

    canvas.PushClip(headerRect);
    canvas.DrawRect(headerRect, pal.surfaceElevated);

    for (keel::u16 i = 0; i < tree->columnCount; ++i) {
        const auto& col = tree->columns[i];
        const keel::f32 colX = colOriginX + tree->resolvedColX[i];
        const keel::f32 colW = tree->resolvedColW[i];
        const Rect colHdrRect(colX, headerRect.y, colW, headerRect.h);
        if (!colHdrRect.Intersects(headerRect)) continue;

        Rect titleRect(
            colX + cellPadX,
            headerRect.y,
            keel::max(0.0f, colW - cellPadX * 2.0f),
            headerRect.h
        );
        const Color titleCol = headerNode->disabled ? pal.textSecondary.WithAlphaF(theme.disabledOpacity) : pal.textSecondary;

        if (col.icon.IsValid()) {
            const keel::f32 iconSz = keel::min(13.0f * scale, headerRect.h * 0.55f);
            const Rect iconRect(titleRect.x, headerRect.y + (headerRect.h - iconSz) * 0.5f, iconSz, iconSz);
            col.icon.Draw(canvas, iconRect, titleCol);
            const keel::f32 adv = iconSz + 5.0f * scale;
            titleRect.x += adv;
            titleRect.w = keel::max(0.0f, titleRect.w - adv);
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

        if (i + 1 < tree->columnCount) {
            const keel::f32 divX = keel::min(keel::floor(colX + colW) + 0.5f, headerRect.Right() - 1.5f);
            const bool dividerActive = (tree->state && tree->state->resizingCol == keel::as<keel::i32>(i)) ||
                                       (tree->hoveredDividerCol == keel::as<keel::i32>(i));
            const Color divCol = dividerActive ? pal.accent : pal.borderSubtle;
            const keel::f32 divThick = dividerActive ? (2.0f * scale) : 1.0f;
            const keel::f32 padY = dividerActive ? (3.0f * scale) : (5.0f * scale);
            canvas.DrawLine(
                Vec2(divX, headerRect.y + padY),
                Vec2(divX, headerRect.Bottom() - padY),
                divCol,
                divThick
            );
        }
    }

    const keel::f32 botY = keel::floor(headerRect.Bottom()) - 0.5f;
    canvas.DrawLine(
        Vec2(headerRect.x, botY),
        Vec2(headerRect.Right(), botY),
        pal.borderSubtle,
        1.0f
    );
    canvas.PopClip();
}

void detail::TreeView::RenderOverlay(
    const Node* outerNode,
    ICanvas& canvas,
    const UiTheme& theme
) noexcept {
    if (!outerNode) return;
    const auto* tree = outerNode->As<TreeViewPayload>();
    if (!tree || tree->columnCount == 0 || !tree->scrollNode) return;

    const Rect bodyRect = tree->scrollNode->computedRect;
    if (bodyRect.w <= 0.0f || bodyRect.h <= 0.0f) return;

    const auto& pal = theme.palette;
    constexpr keel::f32 scale = 1.0f;
    const keel::f32 colOriginX = (tree->resolvedColTotalW > 0.0f) ? tree->resolvedColOriginX : bodyRect.x;

    canvas.PushClip(bodyRect);

    if (tree->opts.gridLines) {
        for (keel::u16 i = 0; i + 1 < tree->columnCount; ++i) {
            const keel::f32 divX = keel::floor(colOriginX + tree->resolvedColX[i] + tree->resolvedColW[i]) + 0.5f;
            if (divX >= bodyRect.x && divX < bodyRect.Right() - 0.5f) {
                canvas.DrawLine(
                    Vec2(divX, bodyRect.y),
                    Vec2(divX, bodyRect.Bottom()),
                    pal.borderSubtle.WithAlpha(85),
                    1.0f
                );
            }
        }
    }

    const keel::i32 activeDivCol = (tree->state && tree->state->resizingCol >= 0)
        ? tree->state->resizingCol
        : tree->hoveredDividerCol;
    if (activeDivCol >= 0 && keel::as<keel::u16>(activeDivCol) + 1 < tree->columnCount) {
        const keel::u16 colIdx = keel::as<keel::u16>(activeDivCol);
        const keel::f32 guideX = keel::clamp(
            keel::floor(colOriginX + tree->resolvedColX[colIdx] + tree->resolvedColW[colIdx]) + 0.5f,
            bodyRect.x + 1.0f,
            bodyRect.Right() - 1.5f
        );
        const Color guideCol = (tree->state && tree->state->resizingCol >= 0)
            ? pal.accent.WithAlpha(210)
            : pal.accent.WithAlpha(110);
        canvas.DrawLine(
            Vec2(guideX, bodyRect.y),
            Vec2(guideX, bodyRect.Bottom()),
            guideCol,
            (tree->state && tree->state->resizingCol >= 0) ? (1.5f * scale) : 1.0f
        );
    }

    canvas.PopClip();
}

void detail::TreeView::RenderBranchGuides(
    const Node* branchNode,
    ICanvas& canvas,
    const UiTheme& theme
) noexcept {
    if (!branchNode || !branchNode->firstChild) return;
    const auto* p = branchNode->As<TreeItemPayload>();
    if (!p || !p->effectiveOpen || !p->hasChildItems) return;

    const TreeGuides guides = p->tree ? p->tree->opts.guides : TreeGuides::Connected;
    if (guides == TreeGuides::None) return;

    const Node* parentRow = branchNode->firstChild;
    const Node* firstChildBranch = parentRow->nextSibling;
    if (!firstChildBranch) return;

    const RowMetrics rm = ComputeRowMetrics(*p, parentRow->uiSize, theme);
    const keel::f32 guideX = keel::round(parentRow->computedRect.x + rm.padX + rm.indentWidth + rm.chevronSlotW * 0.5f) + 0.5f;
    const keel::f32 startY = parentRow->computedRect.Bottom();

    const Node* lastChildRow = nullptr;
    for (const Node* cb = firstChildBranch; cb != nullptr; cb = cb->nextSibling) {
        if (cb->type == NodeType::TreeBranch && cb->firstChild && cb->firstChild->computedRect.h > 0.0f) {
            lastChildRow = cb->firstChild;
        }
    }
    if (!lastChildRow) return;

    const auto& pal = theme.palette;
    const Color accentCol = p->opts.color.HasAny()
        ? p->opts.color.Resolve(false, p->selected, parentRow->disabled, pal.accent)
        : ((p->tree && p->tree->opts.color.HasAny())
            ? p->tree->opts.color.Resolve(false, p->selected, parentRow->disabled, pal.accent)
            : pal.accent);
    const Color guideCol = p->selected
        ? accentCol.WithAlphaF(0.55f)
        : pal.border.WithAlphaF(0.65f);

    const keel::f32 endY = (guides == TreeGuides::Connected)
        ? (keel::round(lastChildRow->computedRect.Center().y) + 0.5f)
        : lastChildRow->computedRect.Bottom();

    if (endY > startY) {
        canvas.DrawLine({guideX, startY}, {guideX, endY}, guideCol, 1.0f);
    }

    if (guides == TreeGuides::Connected) {
        const keel::f32 tickLen = keel::max(6.0f, rm.indentStep - rm.chevronSlotW * 0.5f - 2.0f);
        for (const Node* cb = firstChildBranch; cb != nullptr; cb = cb->nextSibling) {
            if (cb->type == NodeType::TreeBranch && cb->firstChild && cb->firstChild->computedRect.h > 0.0f) {
                const keel::f32 cy = keel::round(cb->firstChild->computedRect.Center().y) + 0.5f;
                canvas.DrawLine({guideX, cy}, {guideX + tickLen, cy}, guideCol, 1.0f);
            }
        }
    }
}

} // namespace guikit
