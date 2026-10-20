#include "guikit/panels/TreeView.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Element.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"
#include "guikit/panels/ScrollArea.hpp"
#include "guikit/widgets/Checkbox.hpp"

namespace guikit {

struct TreeViewPayload {
    Context* ctx{nullptr};
    Node* outerNode{nullptr};
    Node* headerNode{nullptr};
    Node* scrollNode{nullptr};
    TreeViewState* state{nullptr};
    TreeViewOptions opts{};
    std::string_view activeFilter{};
    uint32_t nextRootSlot{0};
    int visibleItemCount{0};
    std::vector<VisibleTreeItemEntry> currVisibleItems{};

    uint16_t columnCount{0};
    std::array<TableColumn, TreeViewState::kMaxColumns> columns{};
    std::array<float, TreeViewState::kMaxColumns> resolvedColX{};
    std::array<float, TreeViewState::kMaxColumns> resolvedColW{};
    int hoveredDividerCol{-1};
    float resolvedColOriginX{0.0f};
    float resolvedColTotalW{0.0f};
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
    uint16_t depth{0};
    uint32_t nextChildSlot{0};
    int visibleRowIndex{0};
    size_t visibleEntryIdx{0};
    bool open{true};
    bool effectiveOpen{true};
    bool selected{false};
    bool hasChildItems{false};
    bool ancestorMatched{false};
    bool matchesFilter{true};
    mutable ICanvas* lastCanvas{nullptr};
};

namespace {

bool ContainsIgnoreCase(std::string_view haystack, std::string_view needle) noexcept {
    if (needle.empty()) return true;
    if (haystack.size() < needle.size()) return false;
    for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < needle.size(); ++j) {
            const unsigned char c1 = static_cast<unsigned char>(haystack[i + j]);
            const unsigned char c2 = static_cast<unsigned char>(needle[j]);
            if (std::tolower(c1) != std::tolower(c2)) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

float AlignToFactor(Align align) noexcept {
    switch (align) {
        case Align::Center: return 0.5f;
        case Align::End:    return 1.0f;
        default:            return 0.0f;
    }
}

float ResolveTreeHeaderHeight(UiSize size, const UiTheme& theme) noexcept {
    switch (size) {
        case UiSize::Xs: return theme.Scale(24.0f);
        case UiSize::Sm: return theme.Scale(28.0f);
        case UiSize::Md: return theme.Scale(32.0f);
        case UiSize::Lg: return theme.Scale(36.0f);
        case UiSize::Xl: return theme.Scale(42.0f);
        default:         return theme.Scale(32.0f);
    }
}

void ResolveTreeColumns(TreeViewPayload& tree, float totalW, const UiTheme& theme) noexcept {
    if (tree.columnCount == 0) return;
    const float scale = (theme.scale > 0.0f) ? theme.scale : 1.0f;
    const float availW = std::max(0.0f, totalW);

    float fixedSum = 0.0f;
    float flexSum = 0.0f;

    for (uint16_t i = 0; i < tree.columnCount; ++i) {
        const auto& col = tree.columns[i];
        const float minW = std::max(36.0f * scale, col.minWidth * scale);
        const float maxW = (col.maxWidth > 0.0f) ? (col.maxWidth * scale) : 100000.0f;

        if (tree.state && tree.state->hasUserColWidth[i]) {
            const float w = std::clamp(tree.state->userColWidths[i] * scale, minW, maxW);
            tree.resolvedColW[i] = w;
            fixedSum += w;
        } else if (col.width > 0.0f) {
            const float w = std::clamp(col.width * scale, minW, maxW);
            tree.resolvedColW[i] = w;
            fixedSum += w;
        } else {
            tree.resolvedColW[i] = 0.0f;
            flexSum += std::max(0.1f, col.flex);
        }
    }

    const float remainingW = std::max(0.0f, availW - fixedSum);
    float curX = 0.0f;

    for (uint16_t i = 0; i < tree.columnCount; ++i) {
        const auto& col = tree.columns[i];
        const float minW = std::max(36.0f * scale, col.minWidth * scale);
        const float maxW = (col.maxWidth > 0.0f) ? (col.maxWidth * scale) : 100000.0f;

        if (tree.resolvedColW[i] <= 0.0f) {
            const float weight = std::max(0.1f, col.flex);
            const float flexShare = (flexSum > 0.0f) ? (remainingW * (weight / flexSum)) : minW;
            tree.resolvedColW[i] = std::clamp(std::round(flexShare), minW, maxW);
        }

        if (i + 1 == tree.columnCount && flexSum > 0.0f &&
            !(tree.state && tree.state->hasUserColWidth[i]) &&
            curX + tree.resolvedColW[i] < availW) {
            tree.resolvedColW[i] = std::min(maxW, availW - curX);
        }

        tree.resolvedColX[i] = curX;
        curX += tree.resolvedColW[i];
    }
}

float ResolveIndentStep(const TreeViewPayload* tree, UiSize size, const UiTheme& theme) noexcept {
    if (tree && tree->opts.indent.has_value()) {
        return std::max(theme.Scale(8.0f), theme.Scale(*tree->opts.indent));
    }
    const ComponentMetrics& cm = theme.GetMetrics(size);
    return std::max(theme.Scale(14.0f), std::round(cm.defaultHeight * 0.50f));
}

struct RowMetrics {
    float padX{8.0f};
    float padY{4.0f};
    float gap{6.0f};
    float indentStep{18.0f};
    float indentWidth{0.0f};
    float chevronSlotW{14.0f};
    float chevronSize{11.0f};
    float checkboxSize{15.0f};
    float iconSize{15.0f};
    float labelFont{13.0f};
    float descFont{11.0f};
    float detailFont{12.0f};
    float badgeFont{11.0f};
    float minRowH{28.0f};
};

RowMetrics ComputeRowMetrics(
    const TreeItemPayload& p,
    UiSize size,
    const UiTheme& theme
) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const HeadingMetrics& hm = theme.GetHeading(size);

    RowMetrics rm{};
    rm.labelFont = theme.ResolveFontSize(p.opts.label, TextRole::LabelLarge, size);
    rm.descFont = theme.ResolveFontSize(p.opts.description, TextRole::LabelSmall, size);
    rm.detailFont = theme.ResolveFontSize(p.opts.detail, TextRole::LabelMedium, size);
    rm.badgeFont = theme.ResolveFontSize(p.opts.badge, TextRole::LabelSmall, size);
    rm.padX = std::max(theme.Scale(6.0f), std::round(cm.paddingX * 0.65f));
    rm.padY = std::max(theme.Scale(3.0f), std::round(cm.paddingY * 0.55f));
    rm.gap = std::max(theme.Scale(5.0f), std::round(hm.gap * 0.85f));
    rm.indentStep = ResolveIndentStep(p.tree, size, theme);
    rm.indentWidth = static_cast<float>(p.depth) * rm.indentStep;
    rm.chevronSize = std::max(theme.Scale(10.0f), std::round(rm.labelFont * 0.82f));
    rm.chevronSlotW = std::max(theme.Scale(14.0f), rm.chevronSize + theme.Scale(3.0f));
    rm.checkboxSize = std::max(theme.Scale(14.0f), std::round(cm.defaultHeight * 0.44f));
    rm.iconSize = std::max(theme.Scale(14.0f), std::round(rm.labelFont * 1.08f));
    rm.minRowH = std::max(theme.Scale(24.0f), std::round(cm.defaultHeight * 0.82f));
    return rm;
}

Vec2 MeasureEmbeddedChild(
    const Node* child,
    float availRowWidth,
    bool singleChildInRow,
    std::optional<float> rowControlWidth,
    const RowMetrics& rm,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    if (!child || child->isSpacer) return {0.0f, 0.0f};
    const UiTheme& childTheme = (child->theme != nullptr) ? *child->theme : theme;

    // Container cell (e.g., item.Cell() or ui.Row()) containing multiple grandchildren
    if (child->firstChild != nullptr &&
        (child->type == NodeType::Cell || child->type == NodeType::Row ||
         child->type == NodeType::Column || child->type == NodeType::Box)) {
        float sumW = 0.0f;
        float maxH = 0.0f;
        uint32_t gcCount = 0;
        const float gap = (child->gap > 0.0f) ? child->gap : rm.gap;
        for (const Node* gc = child->firstChild; gc != nullptr; gc = gc->nextSibling) {
            const Vec2 gm = MeasureEmbeddedChild(gc, availRowWidth, false, std::nullopt, rm, canvas, childTheme);
            if (child->direction == FlexDirection::Column) {
                sumW = std::max(sumW, gm.x);
                maxH += gm.y;
            } else {
                sumW += gm.x;
                maxH = std::max(maxH, gm.y);
            }
            ++gcCount;
        }
        if (gcCount > 1) {
            if (child->direction == FlexDirection::Column) {
                maxH += static_cast<float>(gcCount - 1) * gap;
            } else {
                sumW += static_cast<float>(gcCount - 1) * gap;
            }
        }
        return {sumW + child->padding.Horizontal(), maxH + child->padding.Vertical()};
    }

    Vec2 intr = child->measureFn
        ? child->measureFn(child, 0.0f, canvas, childTheme)
        : child->intrinsicSize;
    if (child->fixedWidth > 0.0f && !child->ctrlStretchable) {
        intr.x = std::max(intr.x, childTheme.Scale(child->fixedWidth));
    }
    if (child->fixedHeight > 0.0f) {
        intr.y = childTheme.Scale(child->fixedHeight);
    }

    const bool isPlainOrBadgeElement = (child->type == NodeType::Element && child->firstChild == nullptr);
    float cw = intr.x;
    if (rowControlWidth.has_value() && *rowControlWidth > 0.0f && singleChildInRow) {
        cw = theme.Scale(*rowControlWidth);
    } else if (child->fixedWidth > 0.0f) {
        cw = childTheme.Scale(child->fixedWidth);
    } else if (child->ctrlStretchable && !isPlainOrBadgeElement) {
        cw = std::max(intr.x, theme.Scale(130.0f));
        if (availRowWidth > 0.0f) {
            cw = std::min(cw, availRowWidth * 0.42f);
        }
    }
    return {cw, intr.y};
}

Vec2 MeasureControlsSlot(
    const Node* rowNode,
    const TreeItemPayload& p,
    float availRowWidth,
    float /*leftContentWidth*/,
    const RowMetrics& rm,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    if (!rowNode || !rowNode->firstChild) {
        return {0.0f, 0.0f};
    }

    float totalW = 0.0f;
    float maxH = 0.0f;
    uint32_t count = 0;
    const bool singleChild = (rowNode->childCount == 1);

    for (const Node* child = rowNode->firstChild; child != nullptr; child = child->nextSibling) {
        const Vec2 cm = MeasureEmbeddedChild(child, availRowWidth, singleChild, p.opts.controlWidth, rm, canvas, theme);
        totalW += cm.x;
        maxH = std::max(maxH, cm.y);
        ++count;
    }

    if (count > 1) {
        totalW += static_cast<float>(count - 1) * rm.gap;
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
        m_ctx->Text("No matching items", {.role = TextRole::LabelMedium, .variant = UiVariant::Muted});
    }

    if (p->state) {
        p->state->prevVisibleItems = std::move(p->currVisibleItems);
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

std::string_view TreeViewScope::GetFilterText() const noexcept {
    return m_payload ? m_payload->activeFilter : std::string_view{};
}

int TreeViewScope::VisibleItemCount() const noexcept {
    return m_payload ? m_payload->visibleItemCount : 0;
}

int TreeViewScope::GetSelectedIndex() const noexcept {
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
    std::source_location loc
) {
    if (!m_visible || !m_ctx || !m_node) {
        return ContainerScope(nullptr, nullptr);
    }
    const Id cellId = m_ctx->MakeChildId(loc);
    const UiSize activeSize = m_ctx->GetCurrentSize();
    const bool activeDisabled = m_ctx->IsDisabled();
    const float activeScale = m_ctx->GetCurrentScale();
    const UiTheme& theme = m_ctx->GetTheme();
    const HeadingMetrics& hm = theme.GetHeading(activeSize);

    Node* cellNode = m_ctx->CreateNode(NodeType::Cell, cellId);
    cellNode->direction = FlexDirection::Row;
    cellNode->gap = std::max(theme.Scale(4.0f), std::round(hm.gap * 0.85f));
    cellNode->align = Alignment::Center;
    cellNode->widthMode = SizeMode::Fit;
    cellNode->heightMode = SizeMode::Fit;
    cellNode->uiSize = activeSize;
    cellNode->disabled = activeDisabled;

    auto* cp = m_ctx->AllocPayload<TreeCellPayload>();
    cp->opts = opts;
    cellNode->SetPayload(cp);

    m_ctx->PushScale(activeScale);
    m_ctx->PushSize(activeSize);
    m_ctx->PushDisabled(activeDisabled);
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
        uint64_t h = detail::HashArgs(p->rowNode->visualHash, p->hasChildItems);
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
        const std::string_view q = p->tree->activeFilter;
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

TreeViewScope TreeView::Open(
    Context& ctx,
    const TreeViewOptions& opts,
    std::source_location loc
) {
    const Id idTree = ctx.MakeChildId(loc);
    const Id idScroll = Id{detail::HashArgs(idTree, 0x72EE'5C80'0001ULL)};
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const UiTheme& theme = ctx.GetTheme();
    const auto& pal = theme.palette;

    TreeViewState& st = ctx.GetOrCreateState<TreeViewState>(idTree);
    if (opts.selectedIndex != nullptr) {
        st.selectedIndex = *opts.selectedIndex;
    }
    if (opts.selectedId != nullptr) {
        st.selectedId = *opts.selectedId;
    }

    // Handle keyboard navigation across visible tree items from previous frame
    const Rect prevTreeBounds = ctx.GetPrevFrameBounds(idTree);
    const bool treeHovered = prevTreeBounds.w > 0.0f && ctx.IsRectHovered(idTree, prevTreeBounds);
    bool anyRowFocused = (ctx.GetFocusedId() == idTree);
    int currentVisIdx = -1;

    for (size_t i = 0; i < st.prevVisibleItems.size(); ++i) {
        const auto& entry = st.prevVisibleItems[i];
        if (ctx.GetFocusedId() == entry.idRow) {
            anyRowFocused = true;
        }
        const bool matchId = st.selectedId.IsValid() && entry.idOuter == st.selectedId;
        const bool matchIdx = (st.selectedIndex >= 0 && entry.index == st.selectedIndex);
        if (matchId || matchIdx) {
            currentVisIdx = static_cast<int>(i);
        }
    }

    if (!activeDisabled && (anyRowFocused || treeHovered) && !st.prevVisibleItems.empty()) {
        auto selectVisibleAt = [&](int visIdx) {
            visIdx = std::clamp(visIdx, 0, static_cast<int>(st.prevVisibleItems.size()) - 1);
            const auto& target = st.prevVisibleItems[static_cast<size_t>(visIdx)];
            st.selectedId = target.idOuter;
            st.selectedIndex = target.index;
            if (opts.selectedId) *opts.selectedId = target.idOuter;
            if (opts.selectedIndex) *opts.selectedIndex = target.index;
            ctx.RequestFocus(target.idRow);
            ctx.ScrollToId(target.idRow);
            ctx.InvalidateAll();
        };

        auto setBranchOpen = [&](const VisibleTreeItemEntry& entry, bool newOpen) {
            if (entry.externalOpen) {
                *entry.externalOpen = newOpen;
            }
            ctx.GetOrCreateState<TreeItemState>(entry.idOuter, TreeItemState{.open = entry.defaultOpen}).open = newOpen;
            ctx.InvalidateAll();
        };

        if (ctx.ConsumeKey(Key::Down)) {
            selectVisibleAt((currentVisIdx < 0) ? 0 : (currentVisIdx + 1));
        } else if (ctx.ConsumeKey(Key::Up)) {
            selectVisibleAt((currentVisIdx <= 0) ? 0 : (currentVisIdx - 1));
        } else if (ctx.ConsumeKey(Key::Home)) {
            selectVisibleAt(0);
        } else if (ctx.ConsumeKey(Key::End)) {
            selectVisibleAt(static_cast<int>(st.prevVisibleItems.size()) - 1);
        } else if (currentVisIdx >= 0 && static_cast<size_t>(currentVisIdx) < st.prevVisibleItems.size()) {
            const auto& cur = st.prevVisibleItems[static_cast<size_t>(currentVisIdx)];
            if (ctx.ConsumeKey(Key::Right)) {
                if (cur.isBranch && !cur.isOpen) {
                    setBranchOpen(cur, true);
                } else if (cur.isBranch && cur.isOpen &&
                           static_cast<size_t>(currentVisIdx + 1) < st.prevVisibleItems.size() &&
                           st.prevVisibleItems[static_cast<size_t>(currentVisIdx + 1)].parentIdOuter == cur.idOuter) {
                    selectVisibleAt(currentVisIdx + 1);
                }
            } else if (ctx.ConsumeKey(Key::Left)) {
                if (cur.isBranch && cur.isOpen) {
                    setBranchOpen(cur, false);
                } else if (cur.parentIdOuter.IsValid()) {
                    for (size_t i = 0; i < st.prevVisibleItems.size(); ++i) {
                        if (st.prevVisibleItems[i].idOuter == cur.parentIdOuter) {
                            selectVisibleAt(static_cast<int>(i));
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

    Node* outerNode = ctx.CreateNode(NodeType::TreeView, idTree);
    outerNode->direction = FlexDirection::Column;
    outerNode->gap = 0.0f;
    outerNode->uiSize = activeSize;
    outerNode->disabled = activeDisabled;
    outerNode->debugBoundsOverride = opts.debugBounds;
    outerNode->inspectModeOverride = opts.inspectMode;

    if (opts.width.has_value() && *opts.width > 0.0f) {
        if (opts.stretch == StretchMode::Fill && *opts.width <= 1.0f) {
            outerNode->widthMode = SizeMode::Stretch;
        } else {
            outerNode->widthMode = SizeMode::Fixed;
            outerNode->fixedWidth = theme.Scale(*opts.width);
        }
    } else {
        outerNode->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    }

    if (opts.height.has_value() && *opts.height > 0.0f) {
        outerNode->heightMode = SizeMode::Fixed;
        outerNode->fixedHeight = theme.Scale(*opts.height);
    } else if (outerNode->parent &&
               (outerNode->parent->heightMode == SizeMode::Stretch || outerNode->parent->heightMode == SizeMode::Fixed)) {
        outerNode->heightMode = SizeMode::Stretch;
    } else {
        outerNode->heightMode = SizeMode::Fit;
    }

    outerNode->cornerRadius = opts.border.radius.has_value()
        ? theme.Scale(*opts.border.radius)
        : theme.GetRadius(activeSize);
    outerNode->borderWidth = (opts.border.width > 0.0f) ? theme.Scale(opts.border.width) : 0.0f;
    if (outerNode->borderWidth > 0.0f) {
        outerNode->borderColor = opts.border.color.Resolve(
            treeHovered,
            anyRowFocused,
            activeDisabled,
            pal.borderSubtle,
            pal.borderHover,
            pal.focusRing,
            pal.borderSubtle.WithAlphaF(0.40f)
        );
    }
    if (opts.background.IsNone()) {
        outerNode->backgroundColor = Color::Transparent();
    } else if (!opts.background.HasAny() || opts.background.IsSurface()) {
        outerNode->backgroundColor = pal.surface;
    } else {
        outerNode->backgroundColor = opts.background.Resolve(
            treeHovered,
            anyRowFocused,
            activeDisabled,
            pal.surface,
            pal.surfaceHover,
            pal.surfaceActive,
            pal.surface.WithAlphaF(0.40f)
        );
    }

    auto* payload = ctx.AllocPayload<TreeViewPayload>();
    payload->ctx = &ctx;
    payload->outerNode = outerNode;
    payload->state = &st;
    payload->opts = opts;
    payload->opts.title = ctx.AllocText(opts.title);

    const uint16_t colCount = static_cast<uint16_t>(
        std::min(opts.columns.size(), TreeViewState::kMaxColumns)
    );
    payload->columnCount = colCount;
    for (uint16_t i = 0; i < colCount; ++i) {
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
        const float scale = (theme.scale > 0.0f) ? theme.scale : 1.0f;
        const float gripHalfW = 6.0f * scale;
        const float prevOriginX = (st.lastColTotalW > 0.0f) ? st.lastColOriginX : st.lastHeaderRect.x;
        const float prevTotalW = (st.lastColTotalW > 0.0f) ? st.lastColTotalW : st.lastHeaderRect.w;
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
            if (mouseDown && st.resizingCol < static_cast<int>(colCount)) {
                const uint16_t colIdx = static_cast<uint16_t>(st.resizingCol);
                const float minW = std::max(36.0f * scale, payload->columns[colIdx].minWidth * scale);
                const float maxW = (payload->columns[colIdx].maxWidth > 0.0f)
                    ? (payload->columns[colIdx].maxWidth * scale)
                    : 100000.0f;
                const float newW = std::clamp(st.resizeStartWidth + (mousePos.x - st.resizeStartX), minW, maxW);
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
            const float localX = mousePos.x - prevOriginX;
            for (uint16_t i = 0; i + 1 < colCount; ++i) {
                if (!payload->columns[i].resizable) continue;
                const float divX = payload->resolvedColX[i] + payload->resolvedColW[i];
                if (std::abs(localX - divX) <= gripHalfW) {
                    payload->hoveredDividerCol = static_cast<int>(i);
                    if (mouseClicked) {
                        const double now = (ctx.GetCurrentTime)();
                        if (st.lastDividerCol == static_cast<int>(i) && (now - st.lastDividerClickTime) <= 0.35) {
                            st.hasUserColWidth.fill(false);
                            st.resizingCol = -1;
                            ResolveTreeColumns(*payload, prevTotalW, theme);
                        } else {
                            // Snapshot columns 0..i so the left edge of column `i` stays anchored during drag
                            for (uint16_t j = 0; j <= i; ++j) {
                                if (!st.hasUserColWidth[j]) {
                                    st.userColWidths[j] = payload->resolvedColW[j] / scale;
                                    st.hasUserColWidth[j] = true;
                                }
                            }
                            st.resizingCol = static_cast<int>(i);
                            st.resizeStartX = mousePos.x;
                            st.resizeStartWidth = payload->resolvedColW[i];
                            ctx.SetActiveId(idTree);
                        }
                        st.lastDividerClickTime = now;
                        st.lastDividerCol = static_cast<int>(i);
                        ctx.InvalidateAll();
                    }
                    break;
                }
            }
        }
    }

    outerNode->SetPayload(payload);
    outerNode->postRenderFn = &TreeView::RenderOverlay;

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(outerNode);

    if (!opts.title.empty() || opts.searchable || opts.showToolbar) {
        const ComponentMetrics& cm = theme.GetMetrics(activeSize);
        const HeadingMetrics& hm = theme.GetHeading(activeSize);
        const float unscaledScale = std::max(0.01f, theme.scale);
        const float barPad = std::max(4.0f, std::round((cm.paddingY * 0.85f) / unscaledScale));
        const float barGap = std::max(4.0f, std::round((hm.gap * 0.85f) / unscaledScale));
        if (auto bar = ctx.Row({
            .gap = barGap,
            .padding = Padding(barPad, barPad, barPad, barPad),
            .align = Alignment::Center
        })) {
            if (!opts.title.empty()) {
                ctx.Text(opts.title, {
                    .role = TextRole::LabelSmall,
                    .variant = UiVariant::Secondary
                });
                if (!opts.searchable) {
                    ctx.Spacer();
                }
            }
            if (opts.searchable) {
                std::string& queryRef = (opts.filterText != nullptr) ? *opts.filterText : st.filterText;
                if (ctx.InputField(queryRef, {
                    .placeholder = opts.searchPlaceholder,
                    .prefix = Icon::Search(),
                    .stretch = StretchMode::Fill,
                    .clearable = true,
                    .disabled = activeDisabled
                }).changed) {
                    ctx.InvalidateAll();
                }
            } else if (opts.title.empty()) {
                ctx.Spacer();
            }

            if (opts.showToolbar) {
                if (ctx.Button({
                    .text = "Expand",
                    .tooltip = "Expand all tree branches",
                    .icon = Icon::ChevronDown(),
                    .color = StateColor::Ghost(),
                    .stretch = StretchMode::Compact,
                    .disabled = activeDisabled
                })) {
                    st.bulkToggleGen++;
                    st.bulkToggleOpen = true;
                    ctx.InvalidateAll();
                }
                if (ctx.Button({
                    .text = "Collapse",
                    .tooltip = "Collapse all tree branches",
                    .icon = Icon::ChevronRight(),
                    .color = StateColor::Ghost(),
                    .stretch = StretchMode::Compact,
                    .disabled = activeDisabled
                })) {
                    st.bulkToggleGen++;
                    st.bulkToggleOpen = false;
                    ctx.InvalidateAll();
                }
            }
        }
        if (auto sep = ctx.Box({
            .widthMode = SizeMode::Stretch,
            .heightMode = SizeMode::Fixed,
            .fixedHeight = 1.0f,
            .background = {pal.borderSubtle}
        })) {}
    }

    if (colCount > 0 && opts.stickyHeader) {
        const Id idHeader = Id{detail::HashArgs(idTree, 0x72EE'4EAD'0001ULL)};
        Node* headerNode = ctx.CreateNode(NodeType::Box, idHeader);
        headerNode->theme = pushedTheme;
        headerNode->widthMode = SizeMode::Stretch;
        headerNode->heightMode = SizeMode::Fixed;
        headerNode->fixedHeight = ResolveTreeHeaderHeight(activeSize, theme);
        headerNode->uiSize = activeSize;
        headerNode->disabled = activeDisabled;
        headerNode->SetPayload(payload);
        headerNode->renderFn = &TreeView::RenderHeader;
        uint64_t hHash = detail::HashArgs(colCount, st.resizingCol, payload->hoveredDividerCol, activeDisabled);
        for (uint16_t i = 0; i < colCount; ++i) {
            hHash = detail::HashArgs(hHash, payload->columns[i].title, st.userColWidths[i], st.hasUserColWidth[i]);
        }
        headerNode->visualHash = hHash;
        payload->headerNode = headerNode;
    }

    const std::string& activeQuery = (opts.filterText != nullptr) ? *opts.filterText : st.filterText;
    payload->activeFilter = ctx.AllocString(activeQuery);

    Node* scrollNode = ctx.CreateNode(NodeType::ScrollArea, idScroll);
    ctx.PopContainer(outerNode);

    scrollNode->theme = pushedTheme;
    scrollNode->direction = FlexDirection::Column;
    scrollNode->clipChildren = true;
    scrollNode->widthMode = SizeMode::Stretch;
    scrollNode->heightMode = (outerNode->heightMode == SizeMode::Fit) ? SizeMode::Fit : SizeMode::Stretch;
    scrollNode->flexGrow = 1.0f;
    if (opts.gridLines && (!opts.rowGap.has_value() || *opts.rowGap == 2.0f)) {
        scrollNode->gap = 0.0f;
    } else {
        scrollNode->gap = opts.rowGap.has_value() ? theme.Scale(*opts.rowGap) : theme.Scale(2.0f);
    }
    if (opts.padding.has_value()) {
        scrollNode->padding = theme.ResolvePadding(*opts.padding, activeSize);
    } else if (colCount > 0) {
        scrollNode->padding = opts.gridLines
            ? Padding::All(0.0f)
            : Padding(theme.Scale(4.0f), 0.0f, theme.Scale(4.0f), 0.0f);
    } else {
        scrollNode->padding = Padding::All(theme.Scale(6.0f));
    }
    scrollNode->scrollOrientation = Orientation::Vertical;
    scrollNode->scrollbarPolicy = opts.scrollbar;
    scrollNode->uiSize = activeSize;
    scrollNode->disabled = activeDisabled;
    scrollNode->debugBoundsOverride = opts.debugBounds;
    scrollNode->inspectModeOverride = opts.inspectMode;
    scrollNode->postRenderFn = &ScrollArea::RenderScrollbar;

    auto& scrollState = ctx.GetOrCreateState<ScrollState>(idScroll);
    const Rect prevScrollBounds = ctx.GetPrevFrameBounds(idScroll);
    if (prevScrollBounds.w > 0.0f && prevScrollBounds.h > 0.0f && !activeDisabled) {
        const Vec2 wheel = ctx.GetMouseWheel();
        if (prevScrollBounds.Contains(ctx.GetMousePos()) && wheel.y != 0.0f && !ctx.GetKeyModifiers().ctrl) {
            const ScrollbarMetrics& sm = theme.GetScrollbar(activeSize);
            scrollState.offset.y = std::max(0.0f, scrollState.offset.y - wheel.y * sm.scrollStep);
        }
    }
    scrollNode->scrollOffset = scrollState.offset;
    payload->scrollNode = scrollNode;

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(scrollNode);

    return TreeViewScope(&ctx, scrollNode, payload);
}

TreeItemScope TreeView::OpenItem(
    Context& ctx,
    bool* externalOpen,
    const TreeItemOptions& opts,
    std::source_location loc
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
    const uint32_t slotIdx = parentPayload
        ? (parentPayload->nextChildSlot++)
        : (treePayload ? (treePayload->nextRootSlot++) : (targetParent ? targetParent->childCount : 0u));
    const Id parentId = parentPayload ? parentPayload->idOuter : (targetParent ? targetParent->id : Id::Null());
    const Id idOuter = Id{detail::HashArgs(parentId, slotIdx, loc.file_name(), loc.line(), loc.column())};
    const Id idRow = Id{detail::HashArgs(idOuter, 0x72EE'17E0'0001ULL)};

    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const UiTheme& theme = ctx.GetTheme();

    TreeItemState& st = ctx.GetOrCreateState<TreeItemState>(idOuter, TreeItemState{.open = opts.defaultOpen});
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
        const bool occluded = ctx.GetOpenPopupId().IsValid() && ctx.GetOpenPopupRect().Contains(mousePos);
        res.hovered = !occluded && res.rect.Contains(mousePos);
    } else {
        res = ctx.Interact(idRow);
    }

    bool wasToggled = false;
    bool wasCheckChanged = false;
    const int itemTraversalIndex = treePayload ? treePayload->visibleItemCount : 0;

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
            const std::string_view q = treePayload->activeFilter;
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
    branchNode->direction = FlexDirection::Column;
    branchNode->widthMode = SizeMode::Stretch;
    branchNode->heightMode = SizeMode::Fit;
    branchNode->gap = (treePayload && treePayload->scrollNode) ? treePayload->scrollNode->gap : theme.Scale(2.0f);
    branchNode->uiSize = activeSize;
    branchNode->disabled = activeDisabled;
    branchNode->postRenderFn = &TreeView::RenderBranchGuides;
    if (targetParent) {
        targetParent->AddChild(branchNode);
    }

    // Create rowNode as child #0 of branchNode
    Node* rowNode = ctx.AllocPayload<Node>();
    rowNode->type = NodeType::TreeItem;
    rowNode->id = idRow;
    rowNode->theme = pushedTheme;
    rowNode->uiSize = activeSize;
    rowNode->disabled = activeDisabled;
    rowNode->hovered = res.hovered;
    rowNode->pressed = res.pressed;
    rowNode->focused = (ctx.GetFocusedId() == idRow);
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
    payload->depth = parentPayload ? static_cast<uint16_t>(parentPayload->depth + 1u) : 0u;
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
        static_cast<uint8_t>(opts.icon.GetType()),
        static_cast<uint8_t>(opts.icon.GetId()),
        static_cast<uint8_t>(opts.openIcon.GetType()),
        static_cast<uint8_t>(opts.openIcon.GetId()),
        opts.color,
        treePayload ? static_cast<uint8_t>(treePayload->opts.selectionStyle) : uint8_t{0},
        treePayload ? treePayload->opts.color : StateColor{},
        treePayload ? treePayload->opts.striped : false,
        treePayload ? treePayload->opts.gridLines : false,
        treePayload ? treePayload->columnCount : uint16_t{0},
        payload->visibleRowIndex & 1
    );

    rowNode->measureFn = &TreeView::MeasureRow;
    rowNode->arrangeFn = &TreeView::ArrangeRow;
    rowNode->renderFn = &TreeView::RenderRow;

    // Reset current-frame hit rects until ArrangeRow computes fresh ones
    st.chevronRect = Rect{};
    st.checkboxRect = Rect{};
    st.controlsRect = Rect{};

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(rowNode);

    res.focused = rowNode->focused;
    return TreeItemScope(&ctx, rowNode, payload, res, isOpen, wasToggled, isSelected, wasCheckChanged);
}

Vec2 TreeView::MeasureRow(
    const Node* rowNode,
    float availableWidth,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    if (!rowNode) return {0.0f, 0.0f};
    const auto* p = rowNode->As<TreeItemPayload>();
    if (!p) return {0.0f, 0.0f};
    p->lastCanvas = canvas;

    const RowMetrics rm = ComputeRowMetrics(*p, rowNode->uiSize, theme);
    const Icon activeIcon = (p->open && p->opts.openIcon.IsValid()) ? p->opts.openIcon : p->opts.icon;

    float leftW = rm.padX + rm.indentWidth + rm.chevronSlotW + rm.gap;
    if (p->opts.checked != nullptr) {
        leftW += rm.checkboxSize + rm.gap;
    }
    if (activeIcon.IsValid()) {
        leftW += rm.iconSize + rm.gap;
    }

    float textW = 0.0f;
    if (p->opts.label.HasContentOrReserve()) {
        textW = TextUtil::MeasureWidth(p->opts.label, rm.labelFont, canvas);
    }
    if (p->opts.description.HasContentOrReserve()) {
        textW = std::max(textW, TextUtil::MeasureWidth(p->opts.description, rm.descFont, canvas));
    }
    leftW += textW;

    float rightDecorW = 0.0f;
    if (p->opts.detail.HasContentOrReserve()) {
        rightDecorW += rm.gap + TextUtil::MeasureWidth(p->opts.detail, rm.detailFont, canvas);
    }
    if (p->opts.badge.HasContentOrReserve()) {
        const float badgePadX = std::max(theme.Scale(5.0f), rm.badgeFont * 0.45f);
        rightDecorW += rm.gap + TextUtil::MeasureWidth(p->opts.badge, rm.badgeFont, canvas) + badgePadX * 2.0f;
    }

    const Vec2 ctrlSlot = MeasureControlsSlot(rowNode, *p, availableWidth, leftW + rightDecorW, rm, canvas, theme);
    float totalW = leftW + rightDecorW + rm.padX;
    if (ctrlSlot.x > 0.0f) {
        totalW += rm.gap + ctrlSlot.x;
    }

    const float labelLineH = canvas ? canvas->GetLineHeight(rm.labelFont) : (rm.labelFont * 1.28f);
    float contentH = std::max({labelLineH, rm.chevronSize, rm.iconSize});
    if (p->opts.checked != nullptr) {
        contentH = std::max(contentH, rm.checkboxSize);
    }
    if (p->opts.description.HasContentOrReserve()) {
        const float descLineH = canvas ? canvas->GetLineHeight(rm.descFont) : (rm.descFont * 1.25f);
        contentH += theme.Scale(2.0f) + descLineH;
    }
    contentH = std::max(contentH, ctrlSlot.y);

    const float totalH = std::max(rm.minRowH, contentH + rm.padY * 2.0f);
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
        std::max(0.0f, cellRect.w - containerNode->padding.Horizontal()),
        std::max(0.0f, cellRect.h - containerNode->padding.Vertical())
    );
    const float gap = (containerNode->gap > 0.0f) ? containerNode->gap : rm.gap;

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

    float totalW = 0.0f;
    uint32_t count = 0;
    uint32_t stretchCount = 0;

    for (Node* gc = containerNode->firstChild; gc != nullptr; gc = gc->nextSibling) {
        if (gc->isSpacer) continue;
        const UiTheme& gcTheme = (gc->theme != nullptr) ? *gc->theme : theme;
        Vec2 intr = gc->measureFn
            ? gc->measureFn(gc, inner.w, canvas, gcTheme)
            : gc->intrinsicSize;
        if (gc->fixedWidth > 0.0f && !gc->ctrlStretchable) {
            intr.x = std::max(intr.x, gcTheme.Scale(gc->fixedWidth));
        }
        if (gc->fixedHeight > 0.0f) {
            intr.y = gcTheme.Scale(gc->fixedHeight);
        }
        gc->intrinsicSize = intr;

        if (gc->fixedWidth > 0.0f) {
            totalW += gcTheme.Scale(gc->fixedWidth);
        } else if (gc->ctrlStretchable) {
            ++stretchCount;
        } else {
            totalW += intr.x;
        }
        ++count;
    }

    const float totalGaps = (count > 1) ? (static_cast<float>(count - 1) * gap) : 0.0f;
    const float freeW = std::max(0.0f, inner.w - totalW - totalGaps);
    const float stretchShare = (stretchCount > 0) ? (freeW / static_cast<float>(stretchCount)) : 0.0f;
    const float usedW = (stretchCount > 0) ? inner.w : std::min(inner.w, totalW + totalGaps);

    float curX = inner.x + (inner.w - usedW) * AlignToFactor(effectiveAlign);

    for (Node* gc = containerNode->firstChild; gc != nullptr; gc = gc->nextSibling) {
        if (gc->isSpacer) {
            gc->computedRect = Rect(curX, inner.y, 0.0f, 0.0f);
            continue;
        }
        const UiTheme& gcTheme = (gc->theme != nullptr) ? *gc->theme : theme;
        const Vec2 intr = gc->intrinsicSize;

        float cw = intr.x;
        if (gc->fixedWidth > 0.0f) {
            cw = gcTheme.Scale(gc->fixedWidth);
        } else if (gc->ctrlStretchable) {
            cw = std::max(intr.x, stretchShare);
        }
        cw = std::min(cw, std::max(0.0f, inner.Right() - curX));

        const float ch = std::min(inner.h - gcTheme.Scale(2.0f), std::max(intr.y, gcTheme.Scale(18.0f)));
        const float cy = inner.y + (inner.h - ch) * 0.5f;
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

void TreeView::ArrangeRow(
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

    const float chevX = bounds.x + rm.padX + rm.indentWidth;
    p->state->chevronRect = Rect(chevX - theme.Scale(2.0f), bounds.y, rm.chevronSlotW + theme.Scale(4.0f), bounds.h);

    float curX = chevX + rm.chevronSlotW + rm.gap;
    if (p->opts.checked != nullptr) {
        const float chkY = bounds.y + (bounds.h - rm.checkboxSize) * 0.5f;
        p->state->checkboxRect = Rect(curX - theme.Scale(2.0f), chkY - theme.Scale(2.0f), rm.checkboxSize + theme.Scale(4.0f), rm.checkboxSize + theme.Scale(4.0f));
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
        uint16_t colIdx = 1;
        for (Node* child = rowNode->firstChild; child != nullptr; child = child->nextSibling, ++colIdx) {
            if (colIdx >= p->tree->columnCount) {
                child->computedRect = Rect{};
                continue;
            }

            const float colX = bounds.x + p->tree->resolvedColX[colIdx];
            const float colW = p->tree->resolvedColW[colIdx];
            const Align colAlign = p->tree->columns[colIdx].align;
            const Rect innerRect(
                colX + rm.padX,
                bounds.y,
                std::max(0.0f, colW - rm.padX * 2.0f),
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

            const UiTheme& childTheme = (child->theme != nullptr) ? *child->theme : theme;
            Vec2 intr = child->measureFn
                ? child->measureFn(child, innerRect.w, p->lastCanvas, childTheme)
                : child->intrinsicSize;
            if (child->fixedWidth > 0.0f && !child->ctrlStretchable) {
                intr.x = std::max(intr.x, childTheme.Scale(child->fixedWidth));
            }
            if (child->fixedHeight > 0.0f) {
                intr.y = childTheme.Scale(child->fixedHeight);
            }

            // Non-interactive Text or standalone Element (icon+label / pill badge) inside a column
            const bool isPlainOrBadgeElement = (child->type == NodeType::Element && child->firstChild == nullptr);
            if (child->type == NodeType::Text || isPlainOrBadgeElement) {
                const float tw = std::min(innerRect.w, intr.x);
                const float th = std::min(innerRect.h, intr.y);
                Align effAlign = colAlign;
                if (child->type == NodeType::Text && child->align != Alignment::Start) {
                    effAlign = child->align;
                } else if (isPlainOrBadgeElement) {
                    if (const auto* ep = child->As<ElementPayload>(); ep && ep->opts.align != Align::Start) {
                        effAlign = ep->opts.align;
                    }
                }
                const float tx = innerRect.x + (innerRect.w - tw) * AlignToFactor(effAlign);
                const float ty = innerRect.y + (innerRect.h - th) * 0.5f;
                const Rect tRect(tx, ty, tw, th);
                if (child->arrangeFn) {
                    child->arrangeFn(child, tRect, childTheme);
                } else {
                    child->computedRect = tRect;
                }
                continue;
            }

            // Interactive Control widget inside a column
            float cw = intr.x;
            if (child->fixedWidth > 0.0f) {
                cw = std::min(innerRect.w, childTheme.Scale(child->fixedWidth));
            } else if (child->ctrlStretchable) {
                cw = innerRect.w;
            } else {
                cw = std::min(innerRect.w, intr.x);
            }
            const float ch = std::min(innerRect.h - childTheme.Scale(4.0f), std::max(intr.y, childTheme.Scale(18.0f)));
            const float cx = innerRect.x + (innerRect.w - cw) * AlignToFactor(colAlign);
            const float cy = innerRect.y + (innerRect.h - ch) * 0.5f;
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
    float rightEdge = bounds.Right() - rm.padX;
    if (!p->opts.badge.empty()) {
        const float badgePadX = std::max(theme.Scale(5.0f), rm.badgeFont * 0.45f);
        const float badgeW = TextUtil::MeasureWidth(p->opts.badge, rm.badgeFont, p->lastCanvas) + badgePadX * 2.0f;
        rightEdge -= (badgeW + rm.gap);
    }

    const Vec2 ctrlSlot = MeasureControlsSlot(rowNode, *p, bounds.w, curX, rm, p->lastCanvas, theme);
    float ctrlStartX = std::max(curX, rightEdge - ctrlSlot.x);
    float x = ctrlStartX;
    Rect unionControls{};
    const bool singleChild = (rowNode->childCount == 1);

    for (Node* child = rowNode->firstChild; child != nullptr; child = child->nextSibling) {
        if (child->isSpacer) {
            child->computedRect = Rect(x, bounds.y, 0.0f, 0.0f);
            continue;
        }

        const Vec2 cm = MeasureEmbeddedChild(child, bounds.w, singleChild, p->opts.controlWidth, rm, p->lastCanvas, theme);
        const float cw = std::min(cm.x, std::max(0.0f, rightEdge - x));
        const float ch = std::min(bounds.h - theme.Scale(2.0f), std::max(cm.y, theme.Scale(18.0f)));
        const float cy = bounds.y + (bounds.h - ch) * 0.5f;
        const Rect cRect(x, cy, cw, ch);

        if (child->firstChild != nullptr &&
            (child->type == NodeType::Cell || child->type == NodeType::Row ||
             child->type == NodeType::Column || child->type == NodeType::Box)) {
            ArrangeEmbeddedContainerChildren(child, cRect, Align::End, rm, p->lastCanvas, theme, unionControls);
        } else {
            const UiTheme& childTheme = (child->theme != nullptr) ? *child->theme : theme;
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

void TreeView::RenderRow(
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
    const float radius = p->opts.border.radius.has_value()
        ? theme.Scale(*p->opts.border.radius)
        : (hasGridLines ? 0.0f : std::max(theme.Scale(4.0f), theme.GetRadius(rowNode->uiSize) * 0.70f));

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
        const float alpha = (selStyle == TreeSelectionStyle::Pill)
            ? (hovered ? 0.26f : 0.20f)
            : (hovered ? 0.20f : 0.14f);
        rowBg = accentCol.WithAlphaF(disabled ? 0.08f : alpha);
    } else if (!disabled && (hovered || pressed)) {
        rowBg = pressed ? pal.surfaceActive.WithAlphaF(0.75f) : pal.surfaceHover.WithAlphaF(0.55f);
    }

    const float borderW = (p->opts.border.width > 0.0f) ? theme.Scale(p->opts.border.width) : 0.0f;
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
        const float lineY = std::floor(bounds.Bottom()) - 0.5f;
        canvas.DrawLine({bounds.x, lineY}, {bounds.Right(), lineY}, pal.borderSubtle.WithAlpha(85), 1.0f);
    }

    // Left vertical accent indicator bar when selected in Indicator mode
    if (selected && selStyle == TreeSelectionStyle::Indicator) {
        const float barW = std::max(theme.Scale(3.0f), 3.0f);
        const float barPadY = std::max(theme.Scale(4.0f), bounds.h * 0.18f);
        const Rect barRect(bounds.x + 1.0f, bounds.y + barPadY, barW, std::max(4.0f, bounds.h - barPadY * 2.0f));
        canvas.DrawRoundRect(barRect, barW * 0.5f, disabled ? accentCol.WithAlphaF(0.40f) : accentCol);
    }

    // Focus ring
    if (rowNode->focused && !disabled) {
        canvas.DrawRoundRect(bounds.Inset(1.0f), radius, Color::Transparent(), pal.focusRing.WithAlphaF(0.75f), 1.0f);
    }

    float curX = bounds.x + rm.padX + rm.indentWidth;

    // 2. Chevron (for branch nodes)
    if (isBranch) {
        const Icon chevIcon = p->open ? Icon::ChevronDown() : Icon::ChevronRight();
        const float chevX = curX + (rm.chevronSlotW - rm.chevronSize) * 0.5f;
        const float chevY = bounds.y + (bounds.h - rm.chevronSize) * 0.5f;
        const bool chevHover = hovered && p->state && p->state->chevronRect.Contains(p->ctx ? p->ctx->GetMousePos() : Vec2{-1.0f, -1.0f});
        Color chevCol = selected ? accentCol : (chevHover ? pal.textPrimary : pal.textSecondary);
        if (disabled) chevCol = chevCol.WithAlphaF(0.40f);
        chevIcon.Draw(canvas, Rect(chevX, chevY, rm.chevronSize, rm.chevronSize), chevCol);
    }
    curX += rm.chevronSlotW + rm.gap;

    // 3. Optional Checkbox (supports checked + indeterminate tri-state)
    if (p->opts.checked != nullptr) {
        const float chkY = bounds.y + (bounds.h - rm.checkboxSize) * 0.5f;
        const Rect chkRect(curX, chkY, rm.checkboxSize, rm.checkboxSize);
        const float chkRadius = std::max(theme.Scale(3.0f), rm.checkboxSize * 0.22f);
        const bool isChecked = *p->opts.checked;
        const bool isIndet = p->opts.indeterminate;

        Color boxBg = (isChecked || isIndet) ? accentCol : pal.surface;
        Color boxBorder = (isChecked || isIndet) ? accentCol : pal.border;
        if (disabled) {
            boxBg = boxBg.WithAlphaF(0.40f);
            boxBorder = boxBorder.WithAlphaF(0.40f);
        }
        canvas.DrawRoundRect(chkRect, chkRadius, boxBg, boxBorder, 1.25f);

        const Color glyphCol = disabled ? pal.textPrimary.WithAlphaF(0.45f) : Color(255, 255, 255, 255);
        if (isIndet) {
            const float dashW = rm.checkboxSize * 0.52f;
            const float dashH = std::max(1.8f, theme.Scale(2.0f));
            canvas.DrawRoundRect(
                Rect(chkRect.Center().x - dashW * 0.5f, chkRect.Center().y - dashH * 0.5f, dashW, dashH),
                dashH * 0.5f,
                glyphCol
            );
        } else if (isChecked) {
            Icon::Check().Draw(canvas, chkRect.Inset(theme.Scale(2.0f)), glyphCol);
        }
        curX += rm.checkboxSize + rm.gap;
    }

    // 4. Optional Icon (supports openIcon when expanded)
    const Icon activeIcon = (p->open && p->opts.openIcon.IsValid()) ? p->opts.openIcon : p->opts.icon;
    if (activeIcon.IsValid()) {
        const float iconY = bounds.y + (bounds.h - rm.iconSize) * 0.5f;
        Color iconCol = selected ? accentCol : theme.ResolveTextColor(p->opts.label, UiVariant::Secondary, disabled, hovered, selected);
        if (!p->opts.label.color.HasAny() && !selected) {
            iconCol = hovered ? pal.textPrimary : pal.textSecondary;
            if (disabled) iconCol = iconCol.WithAlphaF(0.40f);
        }
        activeIcon.Draw(canvas, Rect(curX, iconY, rm.iconSize, rm.iconSize), iconCol);
        curX += rm.iconSize + rm.gap;
    }

    // 5. Compute right boundary of Column 0 (or full row in single-column mode)
    const bool multiCol = (p->tree != nullptr && p->tree->columnCount > 0);
    float rightLimit = multiCol
        ? (bounds.x + p->tree->resolvedColW[0] - rm.padX)
        : (bounds.Right() - rm.padX);

    // 5a. Trailing Badge (in Col 0 or row right)
    if (!p->opts.badge.empty() && rightLimit > curX + theme.Scale(24.0f)) {
        const float badgePadX = std::max(theme.Scale(5.0f), rm.badgeFont * 0.45f);
        const float badgeTextW = TextUtil::MeasureWidth(p->opts.badge, rm.badgeFont, &canvas);
        const float badgeW = badgeTextW + badgePadX * 2.0f;
        const float badgeH = std::max(theme.Scale(16.0f), rm.badgeFont + theme.Scale(4.0f));
        const float badgeX = rightLimit - badgeW;
        const float badgeY = bounds.y + (bounds.h - badgeH) * 0.5f;

        Color badgeBg = selected ? accentCol.WithAlphaF(0.24f) : pal.surfaceHover;
        Color badgeTextCol = selected ? accentCol : pal.textSecondary;
        if (p->opts.badge.color.HasAny()) {
            const Color resolved = p->opts.badge.color.Resolve(hovered, selected, disabled, accentCol);
            badgeBg = resolved.WithAlphaF(disabled ? 0.12f : 0.20f);
            badgeTextCol = resolved;
        } else if (disabled) {
            badgeBg = badgeBg.WithAlphaF(0.40f);
            badgeTextCol = badgeTextCol.WithAlphaF(0.40f);
        }

        canvas.DrawRoundRect(Rect(badgeX, badgeY, badgeW, badgeH), badgeH * 0.5f, badgeBg);
        canvas.DrawTextBox(p->opts.badge.text, Rect(badgeX, badgeY, badgeW, badgeH), rm.badgeFont, badgeTextCol, 0.5f, 0.5f);
        rightLimit = badgeX - rm.gap;
    }

    // 5b. Embedded trailing controls slot reservation (single-column mode only)
    if (!multiCol && p->state && p->state->controlsRect.w > 0.0f) {
        rightLimit = std::min(rightLimit, p->state->controlsRect.x - rm.gap);
    }

    // 5c. Detail metadata text (to the left of controls/badge)
    if (!p->opts.detail.empty() && rightLimit > curX + theme.Scale(40.0f)) {
        const float detailW = std::min(
            TextUtil::MeasureWidth(p->opts.detail, rm.detailFont, &canvas),
            std::max(0.0f, (rightLimit - curX) * 0.45f)
        );
        if (detailW > theme.Scale(12.0f)) {
            const float detailX = rightLimit - detailW;
            const Color detailCol = theme.ResolveTextColor(p->opts.detail, UiVariant::Muted, disabled, hovered, selected);
            const std::string displayDetail = TextUtil::TruncateWithEllipsis(p->opts.detail.text, detailW, rm.detailFont, &canvas);
            canvas.DrawTextBox(displayDetail, Rect(detailX, bounds.y, detailW, bounds.h), rm.detailFont, detailCol, 1.0f, 0.5f);
            rightLimit = detailX - rm.gap;
        }
    }

    // 6. Primary Label + optional Description
    const float availTextW = std::max(0.0f, rightLimit - curX);
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
            const std::string displayLabel = TextUtil::TruncateWithEllipsis(p->opts.label.text, availTextW, rm.labelFont, &canvas);
            canvas.DrawTextBox(displayLabel, Rect(curX, bounds.y, availTextW, bounds.h), rm.labelFont, labelCol, 0.0f, 0.5f);
        } else {
            const float labelLineH = canvas.GetLineHeight(rm.labelFont);
            const float descLineH = canvas.GetLineHeight(rm.descFont);
            const float lineGap = theme.Scale(2.0f);
            const float totalTextH = labelLineH + lineGap + descLineH;
            const float startY = bounds.y + (bounds.h - totalTextH) * 0.5f;

            const std::string displayLabel = TextUtil::TruncateWithEllipsis(p->opts.label.text, availTextW, rm.labelFont, &canvas);
            canvas.DrawTextBox(displayLabel, Rect(curX, startY, availTextW, labelLineH), rm.labelFont, labelCol, 0.0f, 0.5f);

            const Color descCol = theme.ResolveTextColor(p->opts.description, UiVariant::Muted, disabled, hovered, selected);
            const std::string displayDesc = TextUtil::TruncateWithEllipsis(p->opts.description.text, availTextW, rm.descFont, &canvas);
            canvas.DrawTextBox(displayDesc, Rect(curX, startY + labelLineH + lineGap, availTextW, descLineH), rm.descFont, descCol, 0.0f, 0.5f);
        }
    }
}

void TreeView::RenderHeader(
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
    const float scale = (theme.scale > 0.0f) ? theme.scale : 1.0f;
    const ComponentMetrics& cm = theme.GetMetrics(headerNode->uiSize);
    const float cellPadX = std::max(theme.Scale(6.0f), std::round(cm.paddingX * 0.65f));
    const float headerFontSize = theme.ResolveFontSize(UiText{}, TextRole::LabelSmall, headerNode->uiSize);

    const float colOriginX = (tree->resolvedColTotalW > 0.0f) ? tree->resolvedColOriginX : headerRect.x;
    const float colTotalW = (tree->resolvedColTotalW > 0.0f) ? tree->resolvedColTotalW : headerRect.w;
    ResolveTreeColumns(*const_cast<TreeViewPayload*>(tree), colTotalW, theme);

    if (tree->state) {
        tree->state->lastHeaderRect = headerRect;
        tree->state->lastColOriginX = colOriginX;
        tree->state->lastColTotalW = colTotalW;
    }

    canvas.PushClip(headerRect);
    canvas.DrawRect(headerRect, pal.surfaceElevated);

    for (uint16_t i = 0; i < tree->columnCount; ++i) {
        const auto& col = tree->columns[i];
        const float colX = colOriginX + tree->resolvedColX[i];
        const float colW = tree->resolvedColW[i];
        const Rect colHdrRect(colX, headerRect.y, colW, headerRect.h);
        if (!colHdrRect.Intersects(headerRect)) continue;

        Rect titleRect(
            colX + cellPadX,
            headerRect.y,
            std::max(0.0f, colW - cellPadX * 2.0f),
            headerRect.h
        );
        const Color titleCol = headerNode->disabled ? pal.textSecondary.WithAlphaF(0.45f) : pal.textSecondary;

        if (col.icon.IsValid()) {
            const float iconSz = std::min(13.0f * scale, headerRect.h * 0.55f);
            const Rect iconRect(titleRect.x, headerRect.y + (headerRect.h - iconSz) * 0.5f, iconSz, iconSz);
            col.icon.Draw(canvas, iconRect, titleCol);
            const float adv = iconSz + 5.0f * scale;
            titleRect.x += adv;
            titleRect.w = std::max(0.0f, titleRect.w - adv);
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

        if (i + 1 < tree->columnCount) {
            const float divX = std::min(std::floor(colX + colW) + 0.5f, headerRect.Right() - 1.5f);
            const bool dividerActive = (tree->state && tree->state->resizingCol == static_cast<int>(i)) ||
                                       (tree->hoveredDividerCol == static_cast<int>(i));
            const Color divCol = dividerActive ? pal.accent : pal.borderSubtle;
            const float divThick = dividerActive ? (2.0f * scale) : 1.0f;
            const float padY = dividerActive ? (3.0f * scale) : (5.0f * scale);
            canvas.DrawLine(
                Vec2(divX, headerRect.y + padY),
                Vec2(divX, headerRect.Bottom() - padY),
                divCol,
                divThick
            );
        }
    }

    const float botY = std::floor(headerRect.Bottom()) - 0.5f;
    canvas.DrawLine(
        Vec2(headerRect.x, botY),
        Vec2(headerRect.Right(), botY),
        pal.borderSubtle,
        1.0f
    );
    canvas.PopClip();
}

void TreeView::RenderOverlay(
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
    const float scale = (theme.scale > 0.0f) ? theme.scale : 1.0f;
    const float colOriginX = (tree->resolvedColTotalW > 0.0f) ? tree->resolvedColOriginX : bodyRect.x;

    canvas.PushClip(bodyRect);

    if (tree->opts.gridLines) {
        for (uint16_t i = 0; i + 1 < tree->columnCount; ++i) {
            const float divX = std::floor(colOriginX + tree->resolvedColX[i] + tree->resolvedColW[i]) + 0.5f;
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

    const int activeDivCol = (tree->state && tree->state->resizingCol >= 0)
        ? tree->state->resizingCol
        : tree->hoveredDividerCol;
    if (activeDivCol >= 0 && static_cast<uint16_t>(activeDivCol) + 1 < tree->columnCount) {
        const uint16_t colIdx = static_cast<uint16_t>(activeDivCol);
        const float guideX = std::clamp(
            std::floor(colOriginX + tree->resolvedColX[colIdx] + tree->resolvedColW[colIdx]) + 0.5f,
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

void TreeView::RenderBranchGuides(
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
    const float guideX = std::round(parentRow->computedRect.x + rm.padX + rm.indentWidth + rm.chevronSlotW * 0.5f) + 0.5f;
    const float startY = parentRow->computedRect.Bottom();

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

    const float endY = (guides == TreeGuides::Connected)
        ? (std::round(lastChildRow->computedRect.Center().y) + 0.5f)
        : lastChildRow->computedRect.Bottom();

    if (endY > startY) {
        canvas.DrawLine({guideX, startY}, {guideX, endY}, guideCol, 1.0f);
    }

    if (guides == TreeGuides::Connected) {
        const float tickLen = std::max(theme.Scale(6.0f), rm.indentStep - rm.chevronSlotW * 0.5f - theme.Scale(2.0f));
        for (const Node* cb = firstChildBranch; cb != nullptr; cb = cb->nextSibling) {
            if (cb->type == NodeType::TreeBranch && cb->firstChild && cb->firstChild->computedRect.h > 0.0f) {
                const float cy = std::round(cb->firstChild->computedRect.Center().y) + 0.5f;
                canvas.DrawLine({guideX, cy}, {guideX + tickLen, cy}, guideCol, 1.0f);
            }
        }
    }
}

} // namespace guikit
