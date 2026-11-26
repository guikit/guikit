#include "guikit/core/solver.hpp"
#include "keel/math.h"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/style/theme.hpp"

namespace guikit {

namespace {

static constexpr keel::u32 kMaxLayoutDepth = 128;

[[nodiscard]] constexpr keel::f32 ResolveRunGap(const Node* node) noexcept {
    return (node->runGap >= 0.0f) ? node->runGap : node->gap;
}

/// While a node with its own scale processes itself, its outer sizing fields (kept in the parent's units) are
/// viewed in the node's local units, so the layout code below works in one unit system. The originals are
/// restored exactly on exit; in `Measure` mode the freshly measured intrinsic size is published in parent units.
class LocalUnitsView {
public:
    enum class Mode : keel::u8 { Measure, Use };

    LocalUnitsView(Node* node, Mode mode) noexcept
        : m_node(node), m_mode(mode), m_scale(node != nullptr ? node->GetRelativeScale() : 1.0f) {
        m_active = node != nullptr && m_scale > 0.0f && keel::abs(m_scale - 1.0f) > 1e-6f;
        if (!m_active) {
            return;
        }
        m_fixedW = node->fixedWidth;
        m_fixedH = node->fixedHeight;
        m_minW = node->minWidth;
        m_maxW = node->maxWidth;
        m_minH = node->minHeight;
        m_maxH = node->maxHeight;
        m_intrinsic = node->intrinsicSize;
        const keel::f32 inv = 1.0f / m_scale;
        node->fixedWidth *= inv;
        node->fixedHeight *= inv;
        node->minWidth *= inv;
        node->maxWidth *= inv;
        node->minHeight *= inv;
        node->maxHeight *= inv;
        node->intrinsicSize = node->intrinsicSize * inv;
    }

    ~LocalUnitsView() noexcept {
        if (!m_active) {
            return;
        }
        m_node->fixedWidth = m_fixedW;
        m_node->fixedHeight = m_fixedH;
        m_node->minWidth = m_minW;
        m_node->maxWidth = m_maxW;
        m_node->minHeight = m_minH;
        m_node->maxHeight = m_maxH;
        m_node->intrinsicSize = (m_mode == Mode::Measure) ? (m_node->intrinsicSize * m_scale) : m_intrinsic;
    }

    LocalUnitsView(const LocalUnitsView&) = delete;
    LocalUnitsView& operator=(const LocalUnitsView&) = delete;

    [[nodiscard]] bool Active() const noexcept { return m_active; }
    [[nodiscard]] keel::f32 Scale() const noexcept { return m_active ? m_scale : 1.0f; }

private:
    Node* m_node{nullptr};
    Mode m_mode{Mode::Use};
    keel::f32 m_scale{1.0f};
    bool m_active{false};
    keel::f32 m_fixedW{0.0f}, m_fixedH{0.0f}, m_minW{0.0f}, m_maxW{0.0f}, m_minH{0.0f}, m_maxH{0.0f};
    Vec2 m_intrinsic{0.0f, 0.0f};
};

static keel::f32 GetScrollAreaMaxHeight(const Node* node) noexcept {
    if (!node || !node->scroll) {
        return 0.0f;
    }
    return node->scroll->maxHeight;
}

static void MeasureNode(Node* node, ICanvas* canvas, const UiTheme& theme, keel::u32 depth);
static void ArrangeNode(Node* node, Rect availableRect, const UiTheme& theme, ICanvas* canvas, keel::u32 depth);
static keel::f32 ResolveNodeHeight(Node* node, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme, keel::u32 depth);
static void ArrangeRow(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas, keel::u32 depth);
static void ArrangeWrap(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas, keel::u32 depth);
static void ArrangeColumn(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas, keel::u32 depth);
static void ArrangeGrid(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas, keel::u32 depth);

    static bool HasDirectShellPanel(const Node* node) noexcept {
        if (node == nullptr) return false;
        for (const Node* c = node->firstChild; c != nullptr; c = c->nextSibling) {
            if (c->type == NodeType::Sidebar || c->type == NodeType::BottomPanel || c->type == NodeType::StatusBar) {
                return true;
            }
        }
        return false;
    }

    static bool IsShellContainer(const Node* node) noexcept {
        if (node == nullptr) return false;
        if (node->isShell) {
            return true;
        }
        if (node->type != NodeType::Column && node->type != NodeType::Row && node->type != NodeType::Box) {
            return false;
        }
        if (HasDirectShellPanel(node)) {
            return true;
        }
        // Synthetic root or styled surface Box without direct shell panels is a host viewport, not part of an inner shell
        if (node->parent == nullptr || (node->type == NodeType::Box && node->backgroundColor.a > 0)) {
            return false;
        }
        for (const Node* c = node->firstChild; c != nullptr; c = c->nextSibling) {
            if ((c->type == NodeType::Column || c->type == NodeType::Row || c->type == NodeType::Box) &&
                HasDirectShellPanel(c)) {
                return true;
            }
        }
        if (HasDirectShellPanel(node->parent)) {
            for (const Node* c = node->firstChild; c != nullptr; c = c->nextSibling) {
                if (c->type == NodeType::Toolbar) {
                    return true;
                }
            }
        }
        return false;
    }

    static void ResolveShellStylesRecursive(Node* node, const UiTheme& theme, keel::u32 depth) noexcept {
        if (!node || depth >= kMaxLayoutDepth) return;
        const UiTheme& activeTheme = node->GetTheme(theme);

        if (IsShellContainer(node)) {
            const keel::f32 d = node->GetDensity();
            if (!node->explicitGap) {
                node->gap = activeTheme.shell.panelGap * d;
            }
            const bool parentOwnsShellPadding = IsShellContainer(node->parent) &&
                !node->parent->explicitPadding &&
                (node->parent->GetTheme(theme).shellStyle == activeTheme.shellStyle);
            const bool isTopLevelShell = !parentOwnsShellPadding;
            if (isTopLevelShell && !node->explicitPadding) {
                node->padding = activeTheme.shell.windowPadding.ScaledBy(d);
            }
            for (Node* c = node->firstChild; c != nullptr; c = c->nextSibling) {
                if (c->type == NodeType::Toolbar && !c->explicitRadius &&
                    (c->widthMode == SizeMode::Stretch || c->heightMode == SizeMode::Stretch)) {
                    c->cornerRadius = activeTheme.shell.panelRadius;
                    if (c->borderWidth <= 0.0f) {
                        c->borderWidth = activeTheme.shell.panelBorderWidth;
                    }
                } else if ((c->type == NodeType::Card || c->type == NodeType::Box) &&
                           !c->explicitRadius && c->backgroundColor.a > 0) {
                    c->cornerRadius = activeTheme.shell.panelRadius;
                    if (activeTheme.shellStyle == ShellStyle::Docked) {
                        c->borderWidth = 0.0f;
                    } else if (c->borderWidth <= 0.0f) {
                        c->borderWidth = activeTheme.shell.panelBorderWidth;
                        const UiTheme& ct = c->GetTheme(activeTheme);
                        c->borderColor = ct.palette.borderSubtle;
                    }
                }
            }
        }

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            ResolveShellStylesRecursive(child, activeTheme, depth + 1);
        }
    }

    static void MeasureNodeLocal(Node* node, ICanvas* canvas, const UiTheme& theme, keel::u32 depth);

    /// Measures `node` and leaves its intrinsic size in its parent's units.
    static void MeasureNode(Node* node, ICanvas* canvas, const UiTheme& theme, keel::u32 depth) {
        const LocalUnitsView local(node, LocalUnitsView::Mode::Measure);
        MeasureNodeLocal(node, canvas, theme, depth);
    }

    static void MeasureNodeLocal(Node* node, ICanvas* canvas, const UiTheme& theme, keel::u32 depth) {
        if (!node || depth >= kMaxLayoutDepth) return;
        const UiTheme& activeTheme = node->GetTheme(theme);

        // Measure children bottom-up first
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            MeasureNode(child, canvas, activeTheme, depth + 1);
        }

        // Custom autonomous measure delegate (leaves or specialized containers)
        if (node->measureFn) {
            node->intrinsicSize = node->measureFn(node, 0.0f, canvas, activeTheme);
            if (node->minWidth > 0.0f) node->intrinsicSize.x = keel::max(node->intrinsicSize.x, node->minWidth);
            if (node->maxWidth > 0.0f) node->intrinsicSize.x = keel::min(node->intrinsicSize.x, node->maxWidth);
            if (node->minHeight > 0.0f) node->intrinsicSize.y = keel::max(node->intrinsicSize.y, node->minHeight);
            if (node->maxHeight > 0.0f) node->intrinsicSize.y = keel::min(node->intrinsicSize.y, node->maxHeight);
            return;
        }

        // If this is a leaf widget without custom measure delegate, compute its intrinsic dimensions
        if (node->childCount == 0) {
            if (node->widthMode == SizeMode::Fixed || node->heightMode == SizeMode::Fixed) {
                node->intrinsicSize = {node->fixedWidth, node->fixedHeight};
            } else {
                node->intrinsicSize = {0.0f, 0.0f};
            }
            if (node->minWidth > 0.0f) node->intrinsicSize.x = keel::max(node->intrinsicSize.x, node->minWidth);
            if (node->maxWidth > 0.0f) node->intrinsicSize.x = keel::min(node->intrinsicSize.x, node->maxWidth);
            if (node->minHeight > 0.0f) node->intrinsicSize.y = keel::max(node->intrinsicSize.y, node->minHeight);
            if (node->maxHeight > 0.0f) node->intrinsicSize.y = keel::min(node->intrinsicSize.y, node->maxHeight);
            return;
        }

        if (node->type == NodeType::ScrollArea || node->type == NodeType::Sidebar || node->type == NodeType::BottomPanel) {
            if ((node->type == NodeType::Sidebar || node->type == NodeType::BottomPanel) && node->IsCollapsed()) {
                if (node->scroll) {
                    node->scroll->contentSize = {0.0f, 0.0f};
                }
                node->intrinsicSize = {0.0f, 0.0f};
                return;
            }
            keel::f32 totalHeight = node->padding.Vertical();
            keel::f32 maxWidth = 0.0f;
            keel::u32 activeChildren = 0;

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->IsOutOfFlow()) continue;
                const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                const keel::f32 ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
                totalHeight += ch;
                maxWidth = keel::max(maxWidth, cw);
                ++activeChildren;
            }

            if (activeChildren > 1) {
                totalHeight += keel::as<keel::f32>(activeChildren - 1) * node->gap;
            }
            const keel::f32 contentW = maxWidth + node->padding.Horizontal();
            if (node->scroll) {
                node->scroll->contentSize = {contentW, totalHeight};
            }
            if (node->type == NodeType::Sidebar) {
                keel::f32 w = (node->widthMode == SizeMode::Fixed) ? node->fixedWidth : contentW;
                if (node->minWidth > 0.0f) {
                    w = keel::max(w, node->minWidth);
                }
                node->intrinsicSize = {w, totalHeight};
            } else if (node->type == NodeType::BottomPanel) {
                keel::f32 h = (node->heightMode == SizeMode::Fixed) ? node->fixedHeight : totalHeight;
                if (node->minHeight > 0.0f) {
                    h = keel::max(h, node->minHeight);
                }
                node->intrinsicSize = {contentW, h};
            } else if (node->heightMode == SizeMode::Fixed) {
                const keel::f32 w = (node->widthMode == SizeMode::Fixed)
                    ? node->fixedWidth
                    : ((node->widthMode == SizeMode::Fit) ? contentW : 0.0f);
                node->intrinsicSize = {w, node->fixedHeight};
            } else if (node->heightMode == SizeMode::Fit) {
                const keel::f32 maxH = GetScrollAreaMaxHeight(node);
                const keel::f32 h = (maxH > 0.0f) ? keel::min(totalHeight, maxH) : totalHeight;
                const keel::f32 w = (node->widthMode == SizeMode::Fixed) ? node->fixedWidth : contentW;
                node->intrinsicSize = {w, h};
            } else {
                const keel::f32 w = (node->widthMode == SizeMode::Fixed)
                    ? node->fixedWidth
                    : ((node->widthMode == SizeMode::Fit) ? contentW : 0.0f);
                node->intrinsicSize = {w, 0.0f};
            }
            return;
        }

        if (node->direction == FlexDirection::Row || node->direction == FlexDirection::Wrap) {
            keel::f32 totalWidth = node->padding.Horizontal();
            keel::f32 maxHeight = 0.0f;
            keel::u32 activeChildren = 0;

            if (node->direction == FlexDirection::Row && (node->equalWidths || node->widthMode == SizeMode::Fit)) {
                keel::f32 nonStretchSum = 0.0f;
                keel::f32 stretchFlexSum = 0.0f;
                keel::f32 maxUnitFlexWidth = 0.0f;

                for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                    if (child->IsOutOfFlow()) continue;
                    const keel::f32 ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
                    maxHeight = keel::max(maxHeight, ch);
                    ++activeChildren;

                    if (child->isSpacer) {
                        continue;
                    }
                    const bool isEqualTrack = node->equalWidths && (child->widthMode != SizeMode::Fixed) && (child->type != NodeType::Sidebar);
                    if (isEqualTrack || child->widthMode == SizeMode::Stretch) {
                        const keel::f32 fg = isEqualTrack ? 1.0f : keel::max(0.01f, child->flexGrow);
                        const keel::f32 cw = keel::max(child->minWidth, child->intrinsicSize.x);
                        stretchFlexSum += fg;
                        maxUnitFlexWidth = keel::max(maxUnitFlexWidth, cw / fg);
                    } else {
                        const keel::f32 cw = (child->widthMode == SizeMode::Fixed)
                            ? child->fixedWidth
                            : keel::max(child->minWidth, child->intrinsicSize.x);
                        nonStretchSum += cw;
                    }
                }
                totalWidth += nonStretchSum + stretchFlexSum * maxUnitFlexWidth;
            } else {
                for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                    if (child->IsOutOfFlow()) continue;
                    const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                    const keel::f32 ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
                    totalWidth += cw;
                    maxHeight = keel::max(maxHeight, ch);
                    ++activeChildren;
                }
            }

            if (activeChildren > 1) {
                totalWidth += keel::as<keel::f32>(activeChildren - 1) * node->gap;
            }
            node->intrinsicSize = {totalWidth, maxHeight + node->padding.Vertical()};
        } else if (node->direction == FlexDirection::Grid) {
            const keel::i32 totalCols = keel::as<keel::i32>(node->GetGridColumns());
            const keel::f32 gap = node->gap;
            const keel::f32 runGap = ResolveRunGap(node);

            keel::f32 maxLineWidth = 0.0f;
            keel::f32 totalHeight = 0.0f;
            bool hasLines = false;

            Node* lineStart = node->firstChild;
            while (lineStart != nullptr) {
                Node* lineEnd = lineStart;
                keel::i32 currentSpan = 0;
                keel::f32 currentLineWidth = 0.0f;
                keel::f32 lineMaxH = 0.0f;
                keel::u32 itemsInLine = 0;

                while (lineEnd != nullptr) {
                    if (lineEnd->isSpacer || lineEnd->IsOutOfFlow()) {
                        lineEnd = lineEnd->nextSibling;
                        continue;
                    }

                    const keel::i32 span = keel::clamp<keel::i32>(lineEnd->colSpan, 1, totalCols);
                    if (itemsInLine > 0 && currentSpan + span > totalCols) {
                        break;
                    }

                    const keel::f32 cw = (lineEnd->widthMode == SizeMode::Fixed) ? lineEnd->fixedWidth : lineEnd->intrinsicSize.x;
                    const keel::f32 ch = (lineEnd->heightMode == SizeMode::Fixed) ? lineEnd->fixedHeight : lineEnd->intrinsicSize.y;

                    currentLineWidth += cw;
                    lineMaxH = keel::max(lineMaxH, ch);
                    currentSpan += span;
                    ++itemsInLine;
                    lineEnd = lineEnd->nextSibling;
                }

                if (itemsInLine == 0) {
                    break;
                }

                if (itemsInLine > 1) {
                    currentLineWidth += keel::as<keel::f32>(itemsInLine - 1) * gap;
                }

                maxLineWidth = keel::max(maxLineWidth, currentLineWidth);

                if (hasLines) {
                    totalHeight += runGap;
                }
                totalHeight += lineMaxH;
                hasLines = true;
                lineStart = lineEnd;
            }

            node->intrinsicSize = {
                maxLineWidth + node->padding.Horizontal(),
                totalHeight + node->padding.Vertical()
            };
        } else { // FlexDirection::Column
            keel::f32 totalHeight = node->padding.Vertical();
            keel::f32 maxWidth = 0.0f;
            keel::u32 activeChildren = 0;

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->IsOutOfFlow()) continue;
                if (child->type == NodeType::BottomPanel && child->IsCollapsed()) continue;
                const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                const keel::f32 ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
                totalHeight += ch;
                maxWidth = keel::max(maxWidth, cw);
                ++activeChildren;
            }

            if (activeChildren > 1) {
                totalHeight += keel::as<keel::f32>(activeChildren - 1) * node->gap;
            }
            node->intrinsicSize = {maxWidth + node->padding.Horizontal(), totalHeight};
        }

        if (node->minWidth > 0.0f) {
            node->intrinsicSize.x = keel::max(node->intrinsicSize.x, node->minWidth);
        }
        if (node->maxWidth > 0.0f) {
            node->intrinsicSize.x = keel::min(node->intrinsicSize.x, node->maxWidth);
        }
        if (node->minHeight > 0.0f) {
            node->intrinsicSize.y = keel::max(node->intrinsicSize.y, node->minHeight);
        }
        if (node->maxHeight > 0.0f) {
            node->intrinsicSize.y = keel::min(node->intrinsicSize.y, node->maxHeight);
        }
    }

    static keel::f32 ResolveColumnChildWidth(const Node* parent, const Node* child, keel::f32 availW) noexcept {
        if (child->widthMode == SizeMode::Fixed) {
            return child->fixedWidth;
        }
        keel::f32 childW = availW;
        if (child->widthMode == SizeMode::Fit || (child->widthMode != SizeMode::Stretch && parent->align != Alignment::Stretch)) {
            childW = keel::max(child->intrinsicSize.x, child->minWidth);
        } else if (child->minWidth > 0.0f) {
            childW = keel::max(childW, child->minWidth);
        }
        if (child->maxWidth > 0.0f) {
            childW = keel::min(childW, child->maxWidth);
        }
        if (availW > 0.0f) {
            childW = keel::min(childW, availW);
        }
        return childW;
    }

    static keel::f32 ResolveNodeHeightLocal(Node* node, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme, keel::u32 depth);

    /// Resolves a node's height for an available width, both in its parent's units.
    static keel::f32 ResolveNodeHeight(Node* node, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme, keel::u32 depth) {
        const LocalUnitsView local(node, LocalUnitsView::Mode::Use);
        const keel::f32 s = local.Scale();
        return ResolveNodeHeightLocal(node, availableWidth / s, canvas, theme, depth) * s;
    }

    static keel::f32 ResolveNodeHeightLocal(Node* node, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme, keel::u32 depth) {
        if (!node || depth >= kMaxLayoutDepth) return 0.0f;
        if (node->heightMode == SizeMode::Fixed) {
            return node->fixedHeight;
        }
        if (node->isSpacer) {
            return 0.0f;
        }
        if (node->widthMode == SizeMode::Fixed && node->fixedWidth > 0.0f) {
            availableWidth = (availableWidth > 0.0f) ? keel::min(availableWidth, node->fixedWidth) : node->fixedWidth;
        }
        const UiTheme& activeTheme = node->GetTheme(theme);
        if ((node->type == NodeType::Element || node->type == NodeType::TreeItem || node->type == NodeType::Scene) && node->measureFn) {
            return keel::max(node->minHeight, node->measureFn(node, availableWidth, canvas, activeTheme).y);
        }
        if (node->childCount == 0) {
            keel::f32 h = node->intrinsicSize.y;
            if (node->measureFn) {
                h = node->measureFn(node, availableWidth, canvas, activeTheme).y;
            } else if (node->aspectRatio > 0.0f && availableWidth > 0.0f) {
                h = availableWidth / node->aspectRatio;
            }
            const keel::f32 resolvedH = keel::max(node->minHeight, h);
            if (availableWidth > 0.0f) {
                node->intrinsicSize.y = resolvedH;
            }
            return resolvedH;
        }

        if (node->direction == FlexDirection::Wrap) {
            const keel::f32 maxLineWidth = keel::max(0.0f, availableWidth - node->padding.Horizontal());
            Node* lineStart = node->firstChild;
            keel::f32 totalH = 0.0f;
            bool hasLines = false;

            while (lineStart != nullptr) {
                Node* lineEnd = lineStart;
                keel::f32 currentLineWidth = 0.0f;
                keel::f32 lineMaxH = 0.0f;
                keel::u32 itemsInLine = 0;

                while (lineEnd != nullptr) {
                    if (lineEnd->isSpacer || lineEnd->type == NodeType::Sidebar || lineEnd->IsOutOfFlow()) {
                        lineEnd = lineEnd->nextSibling;
                        continue;
                    }

                    const keel::f32 cw = (lineEnd->widthMode == SizeMode::Fixed) ? lineEnd->fixedWidth : lineEnd->intrinsicSize.x;
                    const keel::f32 ch = (lineEnd->heightMode == SizeMode::Fixed) ? lineEnd->fixedHeight : lineEnd->intrinsicSize.y;
                    const keel::f32 neededWidth = (itemsInLine > 0) ? (node->gap + cw) : cw;

                    if (itemsInLine > 0 && currentLineWidth + neededWidth > maxLineWidth) {
                        break;
                    }

                    currentLineWidth += neededWidth;
                    lineMaxH = keel::max(lineMaxH, ch);
                    ++itemsInLine;
                    lineEnd = lineEnd->nextSibling;
                }

                if (itemsInLine == 0) {
                    break;
                }

                if (hasLines) {
                    totalH += ResolveRunGap(node);
                }
                totalH += lineMaxH;
                hasLines = true;
                lineStart = lineEnd;
            }

            keel::f32 resolvedH = keel::max(node->minHeight, totalH + node->padding.Vertical());
            if (node->maxHeight > 0.0f) {
                resolvedH = keel::min(resolvedH, node->maxHeight);
            }
            return resolvedH;
        }

        if (node->direction == FlexDirection::Row && node->heightMode == SizeMode::Fit) {
            const keel::f32 contentW = keel::max(0.0f, availableWidth - node->padding.Horizontal());
            keel::u32 flowChildCount = 0;
            keel::f32 baseSum = 0.0f;
            keel::f32 totalFlexGrow = 0.0f;

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;
                ++flowChildCount;
                if (child->isSpacer || child->widthMode == SizeMode::Stretch) {
                    totalFlexGrow += keel::max(0.01f, child->flexGrow);
                } else if (child->widthMode == SizeMode::Fixed) {
                    baseSum += child->fixedWidth;
                } else {
                    baseSum += child->intrinsicSize.x;
                }
            }

            const keel::f32 totalGap = (flowChildCount > 1)
                ? keel::as<keel::f32>(flowChildCount - 1) * node->gap
                : 0.0f;
            const keel::f32 freeSpace = keel::max(0.0f, contentW - totalGap - baseSum);

            keel::f32 maxH = 0.0f;
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->isSpacer || child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;
                keel::f32 childW = 0.0f;
                if (child->widthMode == SizeMode::Stretch) {
                    childW = (totalFlexGrow > 0.0f)
                        ? (freeSpace * (keel::max(0.01f, child->flexGrow) / totalFlexGrow))
                        : 0.0f;
                } else if (child->widthMode == SizeMode::Fixed) {
                    childW = child->fixedWidth;
                } else {
                    childW = child->intrinsicSize.x;
                }
                if (child->minWidth > 0.0f) {
                    childW = keel::max(childW, child->minWidth);
                }
                const keel::f32 ch = ResolveNodeHeight(child, childW, canvas, activeTheme, depth + 1);
                maxH = keel::max(maxH, ch);
            }
            keel::f32 resolvedH = keel::max(node->minHeight, maxH + node->padding.Vertical());
            const keel::f32 maxAllowedH = GetScrollAreaMaxHeight(node);
            if (maxAllowedH > 0.0f) {
                resolvedH = keel::min(resolvedH, maxAllowedH);
            }
            if (node->maxHeight > 0.0f) {
                resolvedH = keel::min(resolvedH, node->maxHeight);
            }
            return resolvedH;
        }

        if (node->direction == FlexDirection::Column && node->heightMode == SizeMode::Fit) {
            const keel::f32 childAvailW = keel::max(0.0f, availableWidth - node->padding.Horizontal());
            keel::f32 sumH = node->padding.Vertical();
            keel::u32 activeChildren = 0;

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->isSpacer || child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;
                if (child->type == NodeType::BottomPanel && child->IsCollapsed()) continue;
                const keel::f32 cw = ResolveColumnChildWidth(node, child, childAvailW);
                const keel::f32 ch = ResolveNodeHeight(child, cw, canvas, activeTheme, depth + 1);
                sumH += ch;
                ++activeChildren;
            }

            if (activeChildren > 1) {
                sumH += keel::as<keel::f32>(activeChildren - 1) * node->gap;
            }
            keel::f32 resolvedH = keel::max(node->minHeight, sumH);
            const keel::f32 maxAllowedH = GetScrollAreaMaxHeight(node);
            if (maxAllowedH > 0.0f) {
                resolvedH = keel::min(resolvedH, maxAllowedH);
            }
            if (node->maxHeight > 0.0f) {
                resolvedH = keel::min(resolvedH, node->maxHeight);
            }
            return resolvedH;
        }

        if (node->direction == FlexDirection::Grid) {
            const keel::f32 availW = keel::max(0.0f, availableWidth - node->padding.Horizontal());
            const keel::i32 totalCols = keel::as<keel::i32>(node->GetGridColumns());
            const keel::f32 minColW = node->GetMinColWidth();
            const keel::f32 gap = node->gap;
            const keel::f32 runGap = ResolveRunGap(node);

            keel::f32 totalH = 0.0f;
            bool hasLines = false;
            Node* lineStart = node->firstChild;

            while (lineStart != nullptr) {
                Node* lineEnd = lineStart;
                keel::i32 currentLineSpan = 0;
                keel::f32 lineMaxH = 0.0f;
                keel::u32 itemsInLine = 0;

                while (lineEnd != nullptr) {
                    if (lineEnd->isSpacer || lineEnd->type == NodeType::Sidebar || lineEnd->IsOutOfFlow()) {
                        lineEnd = lineEnd->nextSibling;
                        continue;
                    }

                    keel::i32 span = keel::clamp<keel::i32>(lineEnd->colSpan, 1, totalCols);
                    const keel::f32 reqMinW = (lineEnd->minWidth > 0.0f) ? lineEnd->minWidth : ((minColW > 0.0f) ? minColW : lineEnd->intrinsicSize.x);

                    if (itemsInLine > 0) {
                        if (currentLineSpan + span > totalCols) {
                            break;
                        }
                        if (reqMinW > 0.0f) {
                            const keel::f32 testW = (keel::as<keel::f32>(span) / keel::as<keel::f32>(totalCols)) * (availW + gap) - gap;
                            if (testW < reqMinW) {
                                break;
                            }
                        }
                    } else {
                        if (reqMinW > 0.0f) {
                            const keel::f32 testW = (keel::as<keel::f32>(span) / keel::as<keel::f32>(totalCols)) * (availW + gap) - gap;
                            if (testW < reqMinW) {
                                span = totalCols;
                            }
                        }
                    }

                    const keel::f32 childW = keel::max(0.0f, (keel::as<keel::f32>(span) / keel::as<keel::f32>(totalCols)) * (availW + gap) - gap);
                    const keel::f32 childH = ResolveNodeHeight(lineEnd, childW, canvas, activeTheme, depth + 1);

                    lineMaxH = keel::max(lineMaxH, childH);
                    currentLineSpan += span;
                    ++itemsInLine;
                    lineEnd = lineEnd->nextSibling;
                }

                if (itemsInLine == 0) {
                    break;
                }

                if (itemsInLine == 1 && lineEnd != nullptr) {
                    const keel::f32 childW = availW;
                    lineMaxH = ResolveNodeHeight(lineStart, childW, canvas, activeTheme, depth + 1);
                }

                if (hasLines) {
                    totalH += runGap;
                }
                totalH += lineMaxH;
                hasLines = true;
                lineStart = lineEnd;
            }

            keel::f32 resolvedH = keel::max(node->minHeight, totalH + node->padding.Vertical());
            if (node->maxHeight > 0.0f) {
                resolvedH = keel::min(resolvedH, node->maxHeight);
            }
            return resolvedH;
        }

        return node->intrinsicSize.y;
    }

    static void ArrangeNodeLocal(Node* node, Rect availableRect, const UiTheme& theme, ICanvas* canvas, keel::u32 depth);

    /// Arranges `node` into `availableRect` (parent units). A scaled node keeps the rect's top-left corner and
    /// lays itself and its subtree out in its local units, so `computedRect` ends up in local units.
    static void ArrangeNode(Node* node, Rect availableRect, const UiTheme& theme, ICanvas* canvas, keel::u32 depth) {
        const LocalUnitsView local(node, LocalUnitsView::Mode::Use);
        const keel::f32 s = local.Scale();
        ArrangeNodeLocal(node, Rect(availableRect.x, availableRect.y, availableRect.w / s, availableRect.h / s), theme, canvas, depth);
    }

    static void ArrangeNodeLocal(Node* node, Rect availableRect, const UiTheme& theme, ICanvas* canvas, keel::u32 depth) {
        if (!node || depth >= kMaxLayoutDepth) return;

        const UiTheme& activeTheme = node->GetTheme(theme);

        // Custom autonomous arrangement delegate (e.g. Toolbar, Carousel, custom containers)
        if (node->arrangeFn) {
            node->arrangeFn(node, availableRect, activeTheme);
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->IsOutOfFlow()) {
                    ArrangeNode(child, node->computedRect, activeTheme, canvas, depth + 1);
                }
            }
            return;
        }

        // Resolve this node's own width and height
        keel::f32 w = availableRect.w;
        keel::f32 h = availableRect.h;

        if (node->parent == nullptr && !node->isSpacer) {
            if (node->widthMode == SizeMode::Fit) {
                w = node->intrinsicSize.x;
            } else if (node->widthMode == SizeMode::Fixed) {
                w = node->fixedWidth;
            }

            if (node->heightMode == SizeMode::Fit) {
                h = ResolveNodeHeightLocal(node, w, canvas, activeTheme, depth + 1);
            } else if (node->heightMode == SizeMode::Fixed) {
                h = node->fixedHeight;
            }
        }

        if (node->maxWidth > 0.0f) {
            w = keel::min(w, node->maxWidth);
        }
        const keel::f32 maxAllowedH = GetScrollAreaMaxHeight(node);
        if (maxAllowedH > 0.0f) {
            h = keel::min(h, maxAllowedH);
        }
        if (node->maxHeight > 0.0f) {
            h = keel::min(h, node->maxHeight);
        }

        if (node->panelHooks != nullptr && node->panelHooks->placeOutOfFlow != nullptr) {
            const keel::f32 targetW = (node->widthMode == SizeMode::Fixed && node->fixedWidth > 0.0f)
                ? node->fixedWidth
                : node->intrinsicSize.x;
            const keel::f32 resolvedH = (node->heightMode == SizeMode::Fixed && node->fixedHeight > 0.0f)
                ? node->fixedHeight
                : ResolveNodeHeightLocal(node, targetW, canvas, activeTheme, depth + 1);
            node->computedRect = node->panelHooks->placeOutOfFlow(node, targetW, resolvedH, availableRect);
        } else {
            node->computedRect = Rect(availableRect.x, availableRect.y, w, h);
        }

        if (node->childCount == 0) {
            return;
        }

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->IsOutOfFlow()) {
                ArrangeNode(child, node->computedRect, activeTheme, canvas, depth + 1);
            }
        }

        if (node->IsSolverScrollable()) {
            ScrollData& sd = *node->scroll;
            const ScrollbarMetrics& sm = activeTheme.GetScrollbar(node->uiSize);
            const keel::f32 fullW = keel::max(0.0f, node->computedRect.w - node->padding.Horizontal());
            const keel::f32 fullH = keel::max(0.0f, node->computedRect.h - node->padding.Vertical());
            const bool allowV = (sd.orientation == Orientation::Vertical || sd.orientation == Orientation::Both);
            const bool allowH = (sd.orientation == Orientation::Horizontal || sd.orientation == Orientation::Both);

            // 1. Determine if horizontal scrollbar is required
            keel::f32 naturalContentW = 0.0f;
            if (allowH) {
                if (node->direction == FlexDirection::Row) {
                    keel::f32 sumW = 0.0f;
                    keel::u32 active = 0;
                    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                        if (child->isSpacer || child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;
                        const bool isFullStretchViewport = (child->widthMode == SizeMode::Stretch && child->heightMode == SizeMode::Stretch && child->clipChildren);
                        const keel::f32 cw = (child->widthMode == SizeMode::Fixed)
                            ? child->fixedWidth
                            : (isFullStretchViewport ? child->minWidth : child->intrinsicSize.x);
                        sumW += cw;
                        ++active;
                    }
                    if (active > 1) sumW += keel::as<keel::f32>(active - 1) * node->gap;
                    naturalContentW = sumW;
                } else {
                    keel::f32 maxW = 0.0f;
                    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                        if (child->isSpacer || child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;
                        const bool isFullStretchViewport = (child->widthMode == SizeMode::Stretch && child->heightMode == SizeMode::Stretch && child->clipChildren);
                        const keel::f32 cw = (child->widthMode == SizeMode::Fixed)
                            ? child->fixedWidth
                            : (isFullStretchViewport ? child->minWidth : child->intrinsicSize.x);
                        maxW = keel::max(maxW, cw);
                    }
                    naturalContentW = maxW;
                }
            }

            const bool needScrollbarX = allowH && (
                sd.policy == VisibilityPolicy::Always ||
                (sd.policy == VisibilityPolicy::Auto && naturalContentW > fullW + 0.5f)
            );
            const keel::f32 gutterY = needScrollbarX ? sm.gutter : 0.0f;
            const keel::f32 viewportH = keel::max(0.0f, fullH - gutterY);
            const keel::f32 totalViewportH = keel::max(0.0f, node->computedRect.h - gutterY);

            // 2. Determine if vertical scrollbar is required
            bool needScrollbarY = allowV && (sd.policy == VisibilityPolicy::Always);
            if (!needScrollbarY && allowV && sd.policy == VisibilityPolicy::Auto) {
                const keel::f32 measureW = allowH ? keel::max(fullW, naturalContentW) : fullW;
                keel::f32 testH = 0.0f;
                keel::u32 active = 0;
                for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                    if (child->isSpacer || child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;
                    const bool isFullStretchViewport = (child->widthMode == SizeMode::Stretch && child->heightMode == SizeMode::Stretch && child->clipChildren);
                    testH += isFullStretchViewport
                        ? keel::max(viewportH, child->minHeight)
                        : ResolveNodeHeight(child, measureW, canvas, activeTheme, depth + 1);
                    ++active;
                }
                if (active > 1) {
                    testH += keel::as<keel::f32>(active - 1) * node->gap;
                }
                testH += node->padding.Vertical();
                if (testH > totalViewportH + 0.5f) {
                    needScrollbarY = true;
                }
            }

            // 3. Reserve gutter so scrollbar never overlays content
            const keel::f32 activeGutterX = (needScrollbarY && allowV) ? sm.gutter : 0.0f;
            const keel::f32 viewportW = keel::max(0.0f, fullW - activeGutterX);
            const keel::f32 totalViewportW = keel::max(0.0f, node->computedRect.w - activeGutterX);
            const keel::f32 effectiveContentW = allowH ? keel::max(viewportW, naturalContentW) : viewportW;

            keel::f32 contentH = 0.0f;
            keel::u32 activeChildren = 0;
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->isSpacer || child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;
                const bool isFullStretchViewport = (child->widthMode == SizeMode::Stretch && child->heightMode == SizeMode::Stretch && child->clipChildren);
                const keel::f32 ch = isFullStretchViewport
                    ? keel::max(viewportH, child->minHeight)
                    : ResolveNodeHeight(child, effectiveContentW, canvas, activeTheme, depth + 1);
                if (node->direction == FlexDirection::Row) {
                    contentH = keel::max(contentH, ch);
                } else {
                    contentH += ch;
                }
                ++activeChildren;
            }
            if (node->direction != FlexDirection::Row && activeChildren > 1) {
                contentH += keel::as<keel::f32>(activeChildren - 1) * node->gap;
            }
            contentH += node->padding.Vertical();

            sd.contentSize = {effectiveContentW + node->padding.Horizontal(), contentH};

            const keel::f32 maxScrollY = allowV ? keel::max(0.0f, contentH - totalViewportH) : 0.0f;
            const keel::f32 maxScrollX = allowH ? keel::max(0.0f, sd.contentSize.x - totalViewportW) : 0.0f;
            sd.offset.y = keel::clamp(sd.offset.y, 0.0f, maxScrollY);
            sd.offset.x = keel::clamp(sd.offset.x, 0.0f, maxScrollX);

            const keel::f32 effectiveContentH = allowV
                ? keel::max(viewportH, contentH - node->padding.Vertical())
                : viewportH;
            const Rect scrollContentRect(
                node->computedRect.x + node->padding.left - sd.offset.x,
                node->computedRect.y + node->padding.top - sd.offset.y,
                effectiveContentW,
                effectiveContentH
            );

            if (node->direction == FlexDirection::Grid) {
                ArrangeGrid(node, scrollContentRect, activeTheme, canvas, depth + 1);
            } else if (node->direction == FlexDirection::Row) {
                ArrangeRow(node, scrollContentRect, activeTheme, canvas, depth + 1);
            } else {
                ArrangeColumn(node, scrollContentRect, activeTheme, canvas, depth + 1);
            }
            return;
        }

        Rect contentRect(
            node->computedRect.x + node->padding.left,
            node->computedRect.y + node->padding.top,
            keel::max(0.0f, node->computedRect.w - node->padding.Horizontal()),
            keel::max(0.0f, node->computedRect.h - node->padding.Vertical())
        );

        // Dock direct NodeType::Sidebar children to Left / Right in call order
        bool hasSidebars = false;
        keel::u32 nonSidebarCount = 0;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->IsOutOfFlow()) continue;
            if (child->type == NodeType::Sidebar) {
                hasSidebars = true;
            } else {
                ++nonSidebarCount;
            }
        }

        if (hasSidebars) {
            keel::f32 leftX = contentRect.x;
            keel::f32 rightX = contentRect.Right();
            const keel::f32 topY = contentRect.y;
            const keel::f32 availH = contentRect.h;
            keel::u32 leftSidebarCount = 0;
            keel::u32 rightSidebarCount = 0;
            bool prevLeftFlush = false;
            bool prevRightFlush = false;

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type != NodeType::Sidebar) continue;

                if (child->IsCollapsed()) {
                    const keel::f32 edgeX = !child->IsDockedRight() ? leftX : rightX;
                    child->computedRect = Rect(edgeX, topY, 0.0f, availH);
                    continue;
                }

                const UiTheme& childTheme = child->GetTheme(activeTheme);
                keel::f32 sbW = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                if (child->widthMode != SizeMode::Fixed && child->IsSolverScrollable()) {
                    const ScrollData& csd = *child->scroll;
                    const bool needSb = (csd.policy == VisibilityPolicy::Always) ||
                        (csd.policy == VisibilityPolicy::Auto && csd.contentSize.y > availH + 0.5f);
                    if (needSb) {
                        sbW += childTheme.GetScrollbar(child->uiSize).gutter;
                    }
                }
                if (child->minWidth > 0.0f) {
                    sbW = keel::max(sbW, child->minWidth);
                }

                if (!child->IsDockedRight()) {
                    if (leftSidebarCount > 0 && !prevLeftFlush) {
                        leftX += node->gap;
                    }
                    sbW = keel::min(sbW, keel::max(0.0f, rightX - leftX));
                    ArrangeNode(child, Rect(leftX, topY, sbW, availH), activeTheme, canvas, depth + 1);
                    leftX += sbW;
                    prevLeftFlush = child->IsFlushDock();
                    ++leftSidebarCount;
                } else {
                    if (rightSidebarCount > 0 && !prevRightFlush) {
                        rightX -= node->gap;
                    }
                    sbW = keel::min(sbW, keel::max(0.0f, rightX - leftX));
                    ArrangeNode(child, Rect(rightX - sbW, topY, sbW, availH), activeTheme, canvas, depth + 1);
                    rightX -= sbW;
                    prevRightFlush = child->IsFlushDock();
                    ++rightSidebarCount;
                }
            }

            if (nonSidebarCount == 0) {
                return;
            }

            if (leftSidebarCount > 0) {
                leftX += node->gap;
            }
            if (rightSidebarCount > 0) {
                rightX -= node->gap;
            }
            contentRect = Rect(leftX, topY, keel::max(0.0f, rightX - leftX), availH);
        }

        if (node->direction == FlexDirection::Row) {
            ArrangeRow(node, contentRect, activeTheme, canvas, depth + 1);
        } else if (node->direction == FlexDirection::Wrap) {
            ArrangeWrap(node, contentRect, activeTheme, canvas, depth + 1);
        } else if (node->direction == FlexDirection::Grid) {
            ArrangeGrid(node, contentRect, activeTheme, canvas, depth + 1);
        } else {
            ArrangeColumn(node, contentRect, activeTheme, canvas, depth + 1);
        }
    }

    static void ArrangeRow(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas, keel::u32 depth) {
        keel::u32 flowChildCount = 0;
        keel::f32 baseSum = 0.0f;
        keel::f32 totalFlexGrow = 0.0f;
        keel::u32 flexCount = 0;

        keel::f32 maxEqualTrackW = 0.0f;
        keel::u32 spacerCount = 0;
        if (node->equalWidths) {
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;
                if (child->isSpacer) {
                    ++spacerCount;
                } else if (child->widthMode != SizeMode::Fixed) {
                    maxEqualTrackW = keel::max(maxEqualTrackW, keel::max(child->minWidth, child->intrinsicSize.x));
                }
            }
        }
        const bool stretchEqualTracks = node->equalWidths &&
            (node->widthMode != SizeMode::Fit) &&
            (spacerCount == 0) &&
            (node->justify == Justify::Start);

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;
            ++flowChildCount;
            const bool isEqualTrack = node->equalWidths && !child->isSpacer && (child->widthMode != SizeMode::Fixed);
            if (child->isSpacer || (isEqualTrack && stretchEqualTracks) || (!node->equalWidths && child->widthMode == SizeMode::Stretch)) {
                const keel::f32 fg = (isEqualTrack && stretchEqualTracks) ? 1.0f : keel::max(0.01f, child->flexGrow);
                totalFlexGrow += fg;
                ++flexCount;
            } else if (child->widthMode == SizeMode::Fixed) {
                baseSum += child->fixedWidth;
            } else if (isEqualTrack) {
                baseSum += maxEqualTrackW;
            } else { // SizeMode::Fit
                baseSum += child->intrinsicSize.x;
            }
        }

        const keel::f32 totalGap = (flowChildCount > 1)
            ? keel::as<keel::f32>(flowChildCount - 1) * node->gap
            : 0.0f;

        const keel::f32 rawFreeSpace = contentRect.w - totalGap - baseSum;
        const keel::f32 freeSpace = keel::max(0.0f, rawFreeSpace);
        keel::f32 currentX = contentRect.x;

        // Compute horizontal deficit absorption for Fit Text/Heading nodes in constrained Rows
        const bool constrainedRow = (node->widthMode != SizeMode::Fit && contentRect.w > 0.0f);
        keel::f32 remDeficit = (constrainedRow && rawFreeSpace < 0.0f && flowChildCount > 1) ? -rawFreeSpace : 0.0f;
        keel::f32 totalLongTextExcess = 0.0f;
        const keel::f32 shortBadgeThreshold = 68.0f;
        if (remDeficit > 0.0f) {
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type == NodeType::Sidebar || child->IsOutOfFlow() || child->isSpacer) continue;
                const bool isEqualTrack = node->equalWidths && (child->widthMode != SizeMode::Fixed);
                if (!isEqualTrack && child->widthMode == SizeMode::Fit &&
                    (child->type == NodeType::Text || child->type == NodeType::Heading)) {
                    const keel::f32 preserveW = keel::max(child->minWidth, keel::min(child->intrinsicSize.x, shortBadgeThreshold));
                    totalLongTextExcess += keel::max(0.0f, child->intrinsicSize.x - preserveW);
                }
            }
        }
        const keel::f32 takeFromLongText = keel::min(remDeficit, totalLongTextExcess);
        const keel::f32 postStage1Deficit = keel::max(0.0f, remDeficit - takeFromLongText);
        keel::f32 totalRemainingTextW = 0.0f;
        if (postStage1Deficit > 0.0f) {
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type == NodeType::Sidebar || child->IsOutOfFlow() || child->isSpacer) continue;
                const bool isEqualTrack = node->equalWidths && (child->widthMode != SizeMode::Fixed);
                if (!isEqualTrack && child->widthMode == SizeMode::Fit &&
                    (child->type == NodeType::Text || child->type == NodeType::Heading)) {
                    const keel::f32 preserveW = keel::max(child->minWidth, keel::min(child->intrinsicSize.x, shortBadgeThreshold));
                    totalRemainingTextW += keel::max(0.0f, preserveW - child->minWidth);
                }
            }
        }
        const keel::f32 takeFromShortText = keel::min(postStage1Deficit, totalRemainingTextW);

        // Main axis justification when there are no stretch elements
        keel::f32 itemGap = node->gap;
        if (flexCount == 0) {
            keel::f32 effectiveFreeSpace = rawFreeSpace + takeFromLongText + takeFromShortText;
            if (constrainedRow && flowChildCount == 1 && effectiveFreeSpace < 0.0f) {
                const Node* singleChild = nullptr;
                for (Node* c = node->firstChild; c != nullptr; c = c->nextSibling) {
                    if (c->type != NodeType::Sidebar && !c->IsOutOfFlow()) {
                        singleChild = c;
                        break;
                    }
                }
                if (singleChild && singleChild->widthMode == SizeMode::Fit && !node->equalWidths) {
                    effectiveFreeSpace = 0.0f;
                }
            }
            if (node->justify == Justify::Center) {
                currentX += effectiveFreeSpace * 0.5f;
            } else if (node->justify == Justify::End && freeSpace > 0.0f) {
                currentX += freeSpace;
            } else if (node->justify == Justify::SpaceBetween && freeSpace > 0.0f && flowChildCount > 1u) {
                itemGap += freeSpace / keel::as<keel::f32>(flowChildCount - 1u);
            }
        }

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;
            keel::f32 childW = 0.0f;
            const bool isEqualTrack = node->equalWidths && !child->isSpacer && (child->widthMode != SizeMode::Fixed);
            if (child->isSpacer || (isEqualTrack && stretchEqualTracks) || (!node->equalWidths && child->widthMode == SizeMode::Stretch)) {
                const keel::f32 fg = (isEqualTrack && stretchEqualTracks) ? 1.0f : keel::max(0.01f, child->flexGrow);
                childW = (totalFlexGrow > 0.0f)
                    ? (freeSpace * (fg / totalFlexGrow))
                    : 0.0f;
            } else if (child->widthMode == SizeMode::Fixed) {
                childW = child->fixedWidth;
            } else if (isEqualTrack) {
                childW = maxEqualTrackW;
            } else { // SizeMode::Fit
                childW = child->intrinsicSize.x;
                if (constrainedRow && flowChildCount == 1) {
                    childW = keel::min(childW, contentRect.w);
                } else if (remDeficit > 0.0f && (child->type == NodeType::Text || child->type == NodeType::Heading)) {
                    const keel::f32 preserveW = keel::max(child->minWidth, keel::min(child->intrinsicSize.x, shortBadgeThreshold));
                    const keel::f32 excessW = keel::max(0.0f, child->intrinsicSize.x - preserveW);
                    if (takeFromLongText > 0.0f && totalLongTextExcess > 0.0f && excessW > 0.0f) {
                        childW -= takeFromLongText * (excessW / totalLongTextExcess);
                    }
                    if (takeFromShortText > 0.0f && totalRemainingTextW > 0.0f) {
                        const keel::f32 remShrinkW = keel::max(0.0f, preserveW - child->minWidth);
                        childW -= takeFromShortText * (remShrinkW / totalRemainingTextW);
                    }
                    childW = keel::max(child->minWidth, childW);
                }
            }

            if (child->minWidth > 0.0f) {
                childW = keel::max(childW, child->minWidth);
            }
            if (child->maxWidth > 0.0f) {
                childW = keel::min(childW, child->maxWidth);
            }
            if (constrainedRow && flowChildCount == 1 && child->widthMode != SizeMode::Fixed) {
                childW = keel::min(childW, contentRect.w);
            }

            keel::f32 childH = contentRect.h;
            if (child->heightMode == SizeMode::Fixed) {
                childH = child->fixedHeight;
            } else if (child->heightMode == SizeMode::Fit || (child->heightMode != SizeMode::Stretch && node->align != Alignment::Stretch)) {
                childH = ResolveNodeHeight(child, childW, canvas, theme, depth);
            }
            if (!child->isSpacer && child->minHeight > 0.0f) {
                childH = keel::max(childH, child->minHeight);
            }
            if (child->maxHeight > 0.0f) {
                childH = keel::min(childH, child->maxHeight);
            }
            if (node->heightMode != SizeMode::Fit && contentRect.h > 0.0f && child->heightMode != SizeMode::Fixed) {
                childH = keel::min(childH, contentRect.h);
            }

            keel::f32 childY = contentRect.y;
            if (node->align == Alignment::Center) {
                childY = contentRect.y + (contentRect.h - childH) * 0.5f;
            } else if (node->align == Alignment::End) {
                childY = contentRect.Bottom() - childH;
            }

            ArrangeNode(child, Rect(currentX, childY, childW, childH), theme, canvas, depth);
            currentX += childW + itemGap;
        }

        if (node->widthMode == SizeMode::Fit && node->type != NodeType::ScrollArea) {
            node->computedRect.w = (currentX > contentRect.x)
                ? (currentX - contentRect.x - itemGap) + node->padding.Horizontal()
                : node->padding.Horizontal();
            if (node->minWidth > 0.0f) {
                node->computedRect.w = keel::max(node->computedRect.w, node->minWidth);
            }
            if (node->maxWidth > 0.0f) {
                node->computedRect.w = keel::min(node->computedRect.w, node->maxWidth);
            }
        }
    }

    static void ArrangeWrap(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas, keel::u32 depth) {
        const keel::f32 maxLineWidth = keel::max(0.0f, contentRect.w);
        Node* lineStart = node->firstChild;
        keel::f32 currentY = contentRect.y;
        keel::f32 maxUsedLineW = 0.0f;
        bool hasLines = false;

        while (lineStart != nullptr) {
            Node* lineEnd = lineStart;
            keel::f32 currentLineWidth = 0.0f;
            keel::f32 lineMaxH = 0.0f;
            keel::u32 itemsInLine = 0;

            while (lineEnd != nullptr) {
                if (lineEnd->isSpacer || lineEnd->type == NodeType::Sidebar || lineEnd->IsOutOfFlow()) {
                    lineEnd = lineEnd->nextSibling;
                    continue;
                }

                const keel::f32 cw = (lineEnd->widthMode == SizeMode::Fixed) ? lineEnd->fixedWidth : lineEnd->intrinsicSize.x;
                const keel::f32 ch = (lineEnd->heightMode == SizeMode::Fixed) ? lineEnd->fixedHeight : lineEnd->intrinsicSize.y;
                const keel::f32 neededWidth = (itemsInLine > 0) ? (node->gap + cw) : cw;

                if (itemsInLine > 0 && currentLineWidth + neededWidth > maxLineWidth) {
                    break;
                }

                currentLineWidth += neededWidth;
                lineMaxH = keel::max(lineMaxH, ch);
                ++itemsInLine;
                lineEnd = lineEnd->nextSibling;
            }

            if (itemsInLine == 0) {
                break;
            }

            if (hasLines) {
                currentY += ResolveRunGap(node);
            }

            // Main axis justification for this line
            keel::f32 itemX = contentRect.x;
            keel::f32 itemGap = node->gap;
            const keel::f32 freeLineWidth = keel::max(0.0f, maxLineWidth - currentLineWidth);

            if (node->justify == Justify::Center) {
                itemX += freeLineWidth * 0.5f;
            } else if (node->justify == Justify::End) {
                itemX += freeLineWidth;
            } else if (node->justify == Justify::SpaceBetween && itemsInLine > 1) {
                itemGap += freeLineWidth / keel::as<keel::f32>(itemsInLine - 1);
            }

            // Place items along this line
            for (Node* child = lineStart; child != lineEnd; child = child->nextSibling) {
                if (child->isSpacer || child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;

                const keel::f32 cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                keel::f32 ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;

                if (node->align == Alignment::Stretch && child->heightMode == SizeMode::Stretch) {
                    ch = lineMaxH;
                }

                keel::f32 childY = currentY;
                if (node->align == Alignment::Center) {
                    childY = currentY + (lineMaxH - ch) * 0.5f;
                } else if (node->align == Alignment::End) {
                    childY = currentY + (lineMaxH - ch);
                }

                ArrangeNode(child, Rect(itemX, childY, cw, ch), theme, canvas, depth);
                itemX += cw + itemGap;
            }

            maxUsedLineW = keel::max(maxUsedLineW, currentLineWidth);
            currentY += lineMaxH;
            hasLines = true;
            lineStart = lineEnd;
        }

        if (node->widthMode == SizeMode::Fit) {
            node->computedRect.w = keel::max(
                node->minWidth,
                maxUsedLineW + node->padding.Horizontal()
            );
            if (node->maxWidth > 0.0f) {
                node->computedRect.w = keel::min(node->computedRect.w, node->maxWidth);
            }
        }
        if (node->heightMode == SizeMode::Fit) {
            node->computedRect.h = keel::max(
                node->minHeight,
                (currentY - contentRect.y) + node->padding.Vertical()
            );
            if (node->maxHeight > 0.0f) {
                node->computedRect.h = keel::min(node->computedRect.h, node->maxHeight);
            }
        }
    }

    static void ArrangeColumn(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas, keel::u32 depth) {
        const bool isScrollAreaWithVisibleBar = (node->type == NodeType::ScrollArea) &&
            (!node->scroll || node->scroll->policy != VisibilityPolicy::Never);
        const bool isDockedScrollable = (node->type == NodeType::Sidebar || node->type == NodeType::BottomPanel) &&
            node->IsSolverScrollable();
        const bool isScrollViewport = (node->heightMode != SizeMode::Fit) &&
            (isScrollAreaWithVisibleBar || isDockedScrollable);
        const bool parentHasDefiniteHeight = (node->heightMode != SizeMode::Fit) &&
            !isScrollAreaWithVisibleBar &&
            !isDockedScrollable;

        auto isChildStretchHeight = [&](const Node* child) noexcept {
            if (child->heightMode != SizeMode::Stretch) return false;
            if (parentHasDefiniteHeight) return true;
            return child->widthMode == SizeMode::Stretch && child->clipChildren;
        };

        auto isChildScrollGrowHeight = [&](const Node* child) noexcept {
            return isScrollViewport &&
                child->heightMode == SizeMode::Stretch &&
                !isChildStretchHeight(child);
        };

        keel::u32 flowChildCount = 0;
        keel::f32 baseSum = 0.0f;
        keel::f32 totalFlexGrow = 0.0f;
        keel::u32 flexCount = 0;

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;
            if (child->type == NodeType::BottomPanel && child->IsCollapsed()) continue;
            ++flowChildCount;
            if (child->isSpacer || isChildStretchHeight(child)) {
                totalFlexGrow += keel::max(0.01f, child->flexGrow);
                ++flexCount;
            } else if (isChildScrollGrowHeight(child)) {
                const keel::f32 cw = ResolveColumnChildWidth(node, child, contentRect.w);
                baseSum += ResolveNodeHeight(child, cw, canvas, theme, depth);
                totalFlexGrow += keel::max(0.01f, child->flexGrow);
                ++flexCount;
            } else if (child->heightMode == SizeMode::Fixed) {
                baseSum += child->fixedHeight;
            } else if (!child->isSpacer) { // SizeMode::Fit or Stretch inside Fit parent
                const keel::f32 cw = ResolveColumnChildWidth(node, child, contentRect.w);
                baseSum += ResolveNodeHeight(child, cw, canvas, theme, depth);
            }
        }

        const keel::f32 totalGap = (flowChildCount > 1)
            ? keel::as<keel::f32>(flowChildCount - 1) * node->gap
            : 0.0f;

        const keel::f32 freeSpace = keel::max(0.0f, contentRect.h - totalGap - baseSum);
        keel::f32 currentY = contentRect.y;

        // Main axis justification when there are no stretch elements
        keel::f32 itemGap = node->gap;
        if (flexCount == 0 && freeSpace > 0.0f) {
            if (node->justify == Justify::Center) {
                currentY += freeSpace * 0.5f;
            } else if (node->justify == Justify::End) {
                currentY += freeSpace;
            } else if (node->justify == Justify::SpaceBetween && flowChildCount > 1u) {
                itemGap += freeSpace / keel::as<keel::f32>(flowChildCount - 1u);
            }
        }

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;
            if (child->type == NodeType::BottomPanel && child->IsCollapsed()) {
                const keel::f32 edgeY = (currentY > contentRect.y)
                    ? keel::clamp(currentY - itemGap, contentRect.y, contentRect.Bottom())
                    : contentRect.Bottom();
                child->computedRect = Rect(contentRect.x, edgeY, contentRect.w, 0.0f);
                continue;
            }
            const keel::f32 childW = ResolveColumnChildWidth(node, child, contentRect.w);

            keel::f32 childH = 0.0f;
            if (child->isSpacer || isChildStretchHeight(child)) {
                childH = (totalFlexGrow > 0.0f)
                    ? (freeSpace * (keel::max(0.01f, child->flexGrow) / totalFlexGrow))
                    : 0.0f;
                if (!child->isSpacer && !parentHasDefiniteHeight && child->minHeight > 0.0f) {
                    childH = keel::max(childH, child->minHeight);
                }
            } else if (isChildScrollGrowHeight(child)) {
                const keel::f32 baseH = ResolveNodeHeight(child, childW, canvas, theme, depth);
                const keel::f32 extraH = (totalFlexGrow > 0.0f)
                    ? (freeSpace * (keel::max(0.01f, child->flexGrow) / totalFlexGrow))
                    : 0.0f;
                childH = baseH + extraH;
            } else if (child->heightMode == SizeMode::Fixed) {
                childH = child->fixedHeight;
            } else if (!child->isSpacer) { // SizeMode::Fit or Stretch inside Fit parent
                childH = ResolveNodeHeight(child, childW, canvas, theme, depth);
            }
            if (child->maxHeight > 0.0f) {
                childH = keel::min(childH, child->maxHeight);
            }

            keel::f32 childX = contentRect.x;
            if (node->align == Alignment::Center) {
                childX = contentRect.x + (contentRect.w - childW) * 0.5f;
            } else if (node->align == Alignment::End) {
                childX = contentRect.Right() - childW;
            }

            ArrangeNode(child, Rect(childX, currentY, childW, childH), theme, canvas, depth);
            currentY += child->computedRect.h * child->GetRelativeScale() + itemGap;
        }

        if (node->heightMode == SizeMode::Fit && !node->IsOutOfFlow() && node->type != NodeType::ScrollArea) {
            node->computedRect.h = (currentY > contentRect.y)
                ? (currentY - contentRect.y - itemGap) + node->padding.Vertical()
                : node->padding.Vertical();
            if (node->minHeight > 0.0f) {
                node->computedRect.h = keel::max(node->computedRect.h, node->minHeight);
            }
            if (node->maxHeight > 0.0f) {
                node->computedRect.h = keel::min(node->computedRect.h, node->maxHeight);
            }
        }
    }

    static void ArrangeGrid(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas, keel::u32 depth) {
        const keel::f32 availW = contentRect.w;
        const keel::i32 totalCols = keel::as<keel::i32>(node->GetGridColumns());
        const keel::f32 minColW = node->GetMinColWidth();
        const keel::f32 gap = node->gap;
        const keel::f32 runGap = ResolveRunGap(node);

        keel::f32 currentY = contentRect.y;
        bool hasLines = false;
        Node* lineStart = node->firstChild;

        while (lineStart != nullptr) {
            Node* lineEnd = lineStart;
            keel::i32 currentLineSpan = 0;
            keel::f32 lineMaxH = 0.0f;
            keel::u32 itemsInLine = 0;

            // Pass 1: measure items fitting in line and find maximum line height
            while (lineEnd != nullptr) {
                if (lineEnd->isSpacer || lineEnd->type == NodeType::Sidebar || lineEnd->IsOutOfFlow()) {
                    lineEnd = lineEnd->nextSibling;
                    continue;
                }

                keel::i32 span = keel::clamp<keel::i32>(lineEnd->colSpan, 1, totalCols);
                const keel::f32 reqMinW = (lineEnd->minWidth > 0.0f) ? lineEnd->minWidth : ((minColW > 0.0f) ? minColW : lineEnd->intrinsicSize.x);

                if (itemsInLine > 0) {
                    if (currentLineSpan + span > totalCols) {
                        break;
                    }
                    if (reqMinW > 0.0f) {
                        const keel::f32 testW = (keel::as<keel::f32>(span) / keel::as<keel::f32>(totalCols)) * (availW + gap) - gap;
                        if (testW < reqMinW) {
                            break;
                        }
                    }
                } else {
                    if (reqMinW > 0.0f) {
                        const keel::f32 testW = (keel::as<keel::f32>(span) / keel::as<keel::f32>(totalCols)) * (availW + gap) - gap;
                        if (testW < reqMinW) {
                            span = totalCols;
                        }
                    }
                }

                const keel::f32 childW = keel::max(0.0f, (keel::as<keel::f32>(span) / keel::as<keel::f32>(totalCols)) * (availW + gap) - gap);
                keel::f32 childH = 0.0f;
                if (lineEnd->heightMode == SizeMode::Fixed) {
                    childH = lineEnd->fixedHeight;
                } else {
                    childH = ResolveNodeHeight(lineEnd, childW, canvas, theme, depth);
                }

                lineMaxH = keel::max(lineMaxH, childH);
                currentLineSpan += span;
                ++itemsInLine;
                lineEnd = lineEnd->nextSibling;
            }

            if (itemsInLine == 0) {
                break;
            }

            if (itemsInLine == 1 && lineEnd != nullptr) {
                const keel::f32 childW = availW;
                lineMaxH = (lineStart->heightMode == SizeMode::Fixed)
                    ? lineStart->fixedHeight
                    : ResolveNodeHeight(lineStart, childW, canvas, theme, depth);
            }

            if (hasLines) {
                currentY += runGap;
            }

            // Pass 2: position items along the line
            keel::f32 currentX = contentRect.x;
            for (Node* child = lineStart; child != lineEnd; child = child->nextSibling) {
                if (child->isSpacer || child->type == NodeType::Sidebar || child->IsOutOfFlow()) continue;

                keel::i32 span = keel::clamp<keel::i32>(child->colSpan, 1, totalCols);
                const keel::f32 reqMinW = (child->minWidth > 0.0f) ? child->minWidth : ((minColW > 0.0f) ? minColW : child->intrinsicSize.x);
                if (itemsInLine == 1 && (lineEnd != nullptr || (reqMinW > 0.0f && ((keel::as<keel::f32>(span) / keel::as<keel::f32>(totalCols)) * (availW + gap) - gap) < reqMinW))) {
                    span = totalCols;
                }

                const keel::f32 childW = keel::max(0.0f, (keel::as<keel::f32>(span) / keel::as<keel::f32>(totalCols)) * (availW + gap) - gap);

                keel::f32 childH = 0.0f;
                if (child->heightMode == SizeMode::Fixed) {
                    childH = child->fixedHeight;
                } else if (child->heightMode == SizeMode::Stretch || node->align == Alignment::Stretch) {
                    childH = lineMaxH;
                } else {
                    childH = ResolveNodeHeight(child, childW, canvas, theme, depth);
                }

                keel::f32 childY = currentY;
                if (node->align == Alignment::Center) {
                    childY = currentY + (lineMaxH - childH) * 0.5f;
                } else if (node->align == Alignment::End) {
                    childY = currentY + (lineMaxH - childH);
                }

                ArrangeNode(child, Rect(currentX, childY, childW, childH), theme, canvas, depth);
                currentX += childW + gap;
            }

            currentY += lineMaxH;
            hasLines = true;
            lineStart = lineEnd;
        }

        if (node->heightMode == SizeMode::Fit) {
            node->computedRect.h = (currentY > contentRect.y)
                ? (currentY - contentRect.y) + node->padding.Vertical()
                : node->padding.Vertical();
            if (node->minHeight > 0.0f) {
                node->computedRect.h = keel::max(node->computedRect.h, node->minHeight);
            }
            if (node->maxHeight > 0.0f) {
                node->computedRect.h = keel::min(node->computedRect.h, node->maxHeight);
            }
        }
    }

} // namespace

void Solver::ResolveShellStyles(Node* root, const UiTheme& theme) {
    if (!root) return;
    ResolveShellStylesRecursive(root, theme, 0);
}

void Solver::Measure(Node* root, ICanvas* canvas, const UiTheme& theme) {
    if (!root) return;
    MeasureNode(root, canvas, theme, 0);
}

void Solver::Measure(Node* root, ICanvas* canvas) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    Measure(root, canvas, defaultTheme);
}

keel::f32 Solver::ResolveHeight(Node* node, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) {
    return ResolveNodeHeight(node, availableWidth, canvas, theme, 0);
}

void Solver::Arrange(Node* root, Rect availableRect, const UiTheme& theme, ICanvas* canvas) {
    if (!root) return;
    ArrangeNode(root, availableRect, theme, canvas, 0);
}

void Solver::Arrange(Node* root, Rect availableRect) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    Arrange(root, availableRect, defaultTheme, nullptr);
}

} // namespace guikit
