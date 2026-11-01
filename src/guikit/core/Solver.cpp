#include "guikit/core/Solver.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/panels/Modal.hpp"

namespace guikit {

namespace {

static void MeasureNode(Node* node, ICanvas* canvas, const UiTheme& theme);
static void ArrangeNode(Node* node, Rect availableRect, const UiTheme& theme, ICanvas* canvas = nullptr);
static float ResolveNodeHeight(Node* node, float availableWidth, ICanvas* canvas, const UiTheme& theme);
static void ArrangeRow(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas);
static void ArrangeWrap(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas);
static void ArrangeColumn(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas);
static void ArrangeGrid(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas);

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

    static void MeasureNode(Node* node, ICanvas* canvas, const UiTheme& theme) {
        const UiTheme& activeTheme = (node->theme != nullptr) ? *node->theme : theme;

        if (IsShellContainer(node)) {
            if (!node->explicitGap) {
                node->gap = activeTheme.shell.panelGap;
            }
            const bool isTopLevelShell = !IsShellContainer(node->parent);
            if (isTopLevelShell && !node->explicitPadding) {
                node->padding = activeTheme.shell.windowPadding;
            }
            for (Node* c = node->firstChild; c != nullptr; c = c->nextSibling) {
                if (c->type == NodeType::Toolbar && !c->explicitRadius && c->widthMode == SizeMode::Stretch) {
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
                        const UiTheme& ct = (c->theme != nullptr) ? *c->theme : activeTheme;
                        c->borderColor = ct.palette.borderSubtle;
                    }
                }
            }
        }

        // Measure children bottom-up first
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            MeasureNode(child, canvas, activeTheme);
        }

        // Custom autonomous measure delegate (leaves or specialized containers)
        if (node->measureFn) {
            node->intrinsicSize = node->measureFn(node, 0.0f, canvas, activeTheme);
            if (node->minWidth > 0.0f) node->intrinsicSize.x = std::max(node->intrinsicSize.x, node->minWidth);
            if (node->minHeight > 0.0f) node->intrinsicSize.y = std::max(node->intrinsicSize.y, node->minHeight);
            return;
        }

        // If this is a leaf widget without custom measure delegate, compute its intrinsic dimensions
        if (node->childCount == 0) {
            if (node->widthMode == SizeMode::Fixed || node->heightMode == SizeMode::Fixed) {
                node->intrinsicSize = {node->fixedWidth, node->fixedHeight};
            } else {
                node->intrinsicSize = {0.0f, 0.0f};
            }
            if (node->minWidth > 0.0f) node->intrinsicSize.x = std::max(node->intrinsicSize.x, node->minWidth);
            if (node->minHeight > 0.0f) node->intrinsicSize.y = std::max(node->intrinsicSize.y, node->minHeight);
            return;
        }

        if (node->type == NodeType::ScrollArea || node->type == NodeType::Sidebar || node->type == NodeType::BottomPanel) {
            if ((node->type == NodeType::Sidebar || node->type == NodeType::BottomPanel) && node->sidebarCollapsed) {
                node->scrollContentSize = {0.0f, 0.0f};
                node->intrinsicSize = {0.0f, 0.0f};
                return;
            }
            float totalHeight = node->padding.Vertical();
            float maxWidth = 0.0f;
            uint32_t activeChildren = 0;

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type == NodeType::Modal) continue;
                const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                const float ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
                totalHeight += ch;
                maxWidth = std::max(maxWidth, cw);
                ++activeChildren;
            }

            if (activeChildren > 1) {
                totalHeight += static_cast<float>(activeChildren - 1) * node->gap;
            }
            const float contentW = maxWidth + node->padding.Horizontal();
            node->scrollContentSize = {contentW, totalHeight};
            if (node->type == NodeType::Sidebar) {
                float w = (node->widthMode == SizeMode::Fixed) ? node->fixedWidth : contentW;
                if (node->minWidth > 0.0f) {
                    w = std::max(w, node->minWidth);
                }
                node->intrinsicSize = {w, totalHeight};
            } else if (node->type == NodeType::BottomPanel) {
                float h = (node->heightMode == SizeMode::Fixed) ? node->fixedHeight : totalHeight;
                if (node->minHeight > 0.0f) {
                    h = std::max(h, node->minHeight);
                }
                node->intrinsicSize = {contentW, h};
            } else if (node->heightMode == SizeMode::Fit) {
                node->intrinsicSize = {contentW, totalHeight};
            } else {
                node->intrinsicSize = {0.0f, 0.0f};
            }
            return;
        }

        if (node->direction == FlexDirection::Row || node->direction == FlexDirection::Wrap) {
            float totalWidth = node->padding.Horizontal();
            float maxHeight = 0.0f;
            uint32_t activeChildren = 0;

            if (node->direction == FlexDirection::Row && (node->equalWidths || node->widthMode == SizeMode::Fit)) {
                float nonStretchSum = 0.0f;
                float stretchFlexSum = 0.0f;
                float maxUnitFlexWidth = 0.0f;

                for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                    if (child->type == NodeType::Modal) continue;
                    const float ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
                    maxHeight = std::max(maxHeight, ch);
                    ++activeChildren;

                    if (child->isSpacer) {
                        continue;
                    }
                    const bool isEqualTrack = node->equalWidths && (child->widthMode != SizeMode::Fixed) && (child->type != NodeType::Sidebar);
                    if (isEqualTrack || child->widthMode == SizeMode::Stretch) {
                        const float fg = isEqualTrack ? 1.0f : std::max(0.01f, child->flexGrow);
                        const float cw = std::max(child->minWidth, child->intrinsicSize.x);
                        stretchFlexSum += fg;
                        maxUnitFlexWidth = std::max(maxUnitFlexWidth, cw / fg);
                    } else {
                        const float cw = (child->widthMode == SizeMode::Fixed)
                            ? child->fixedWidth
                            : std::max(child->minWidth, child->intrinsicSize.x);
                        nonStretchSum += cw;
                    }
                }
                totalWidth += nonStretchSum + stretchFlexSum * maxUnitFlexWidth;
            } else {
                for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                    if (child->type == NodeType::Modal) continue;
                    const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                    const float ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
                    totalWidth += cw;
                    maxHeight = std::max(maxHeight, ch);
                    ++activeChildren;
                }
            }

            if (activeChildren > 1) {
                totalWidth += static_cast<float>(activeChildren - 1) * node->gap;
            }
            node->intrinsicSize = {totalWidth, maxHeight + node->padding.Vertical()};
        } else if (node->direction == FlexDirection::Grid) {
            const int totalCols = (node->gridColumns > 0) ? node->gridColumns : 12;
            const float gap = node->gap;
            const float runGap = (node->runGap > 0.0f) ? node->runGap : gap;

            float maxLineWidth = 0.0f;
            float totalHeight = 0.0f;
            bool hasLines = false;

            Node* lineStart = node->firstChild;
            while (lineStart != nullptr) {
                Node* lineEnd = lineStart;
                int currentSpan = 0;
                float currentLineWidth = 0.0f;
                float lineMaxH = 0.0f;
                uint32_t itemsInLine = 0;

                while (lineEnd != nullptr) {
                    if (lineEnd->isSpacer || lineEnd->type == NodeType::Modal) {
                        lineEnd = lineEnd->nextSibling;
                        continue;
                    }

                    const int span = std::clamp<int>(lineEnd->colSpan, 1, totalCols);
                    if (itemsInLine > 0 && currentSpan + span > totalCols) {
                        break;
                    }

                    const float cw = (lineEnd->widthMode == SizeMode::Fixed) ? lineEnd->fixedWidth : lineEnd->intrinsicSize.x;
                    const float ch = (lineEnd->heightMode == SizeMode::Fixed) ? lineEnd->fixedHeight : lineEnd->intrinsicSize.y;

                    currentLineWidth += cw;
                    lineMaxH = std::max(lineMaxH, ch);
                    currentSpan += span;
                    ++itemsInLine;
                    lineEnd = lineEnd->nextSibling;
                }

                if (itemsInLine == 0) {
                    break;
                }

                if (itemsInLine > 1) {
                    currentLineWidth += static_cast<float>(itemsInLine - 1) * gap;
                }

                maxLineWidth = std::max(maxLineWidth, currentLineWidth);

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
            float totalHeight = node->padding.Vertical();
            float maxWidth = 0.0f;
            uint32_t activeChildren = 0;

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type == NodeType::Modal) continue;
                if (child->type == NodeType::BottomPanel && child->sidebarCollapsed) continue;
                const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                const float ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
                totalHeight += ch;
                maxWidth = std::max(maxWidth, cw);
                ++activeChildren;
            }

            if (activeChildren > 1) {
                totalHeight += static_cast<float>(activeChildren - 1) * node->gap;
            }
            node->intrinsicSize = {maxWidth + node->padding.Horizontal(), totalHeight};
        }

        if (node->minWidth > 0.0f) {
            node->intrinsicSize.x = std::max(node->intrinsicSize.x, node->minWidth);
        }
        if (node->minHeight > 0.0f) {
            node->intrinsicSize.y = std::max(node->intrinsicSize.y, node->minHeight);
        }
    }

    static float ResolveColumnChildWidth(const Node* parent, const Node* child, float availW) noexcept {
        if (child->widthMode == SizeMode::Fixed) {
            return child->fixedWidth;
        }
        float childW = availW;
        if (child->widthMode == SizeMode::Fit || (child->widthMode != SizeMode::Stretch && parent->align != Alignment::Stretch)) {
            childW = std::max(child->intrinsicSize.x, child->minWidth);
        } else if (child->minWidth > 0.0f) {
            childW = std::max(childW, child->minWidth);
        }
        if (availW > 0.0f) {
            childW = std::min(childW, availW);
        }
        return childW;
    }

    /// Resolves actual vertical dimension for a node given constrained available width.
    static float ResolveNodeHeight(Node* node, float availableWidth, ICanvas* canvas, const UiTheme& theme) {
        if (!node) return 0.0f;
        if (node->heightMode == SizeMode::Fixed) {
            return node->fixedHeight;
        }
        if (node->isSpacer) {
            return 0.0f;
        }
        if (node->widthMode == SizeMode::Fixed && node->fixedWidth > 0.0f) {
            availableWidth = (availableWidth > 0.0f) ? std::min(availableWidth, node->fixedWidth) : node->fixedWidth;
        }
        const UiTheme& activeTheme = (node->theme != nullptr) ? *node->theme : theme;
        if ((node->type == NodeType::Element || node->type == NodeType::TreeItem || node->type == NodeType::Scene) && node->measureFn) {
            return std::max(node->minHeight, node->measureFn(node, availableWidth, canvas, activeTheme).y);
        }
        if (node->childCount == 0) {
            float h = node->intrinsicSize.y;
            if (node->measureFn) {
                h = node->measureFn(node, availableWidth, canvas, activeTheme).y;
            } else if (node->aspectRatio > 0.0f && availableWidth > 0.0f) {
                h = availableWidth / node->aspectRatio;
            }
            const float resolvedH = std::max(node->minHeight, h);
            if (availableWidth > 0.0f) {
                node->intrinsicSize.y = resolvedH;
            }
            return resolvedH;
        }

        if (node->direction == FlexDirection::Wrap) {
            const float maxLineWidth = std::max(0.0f, availableWidth - node->padding.Horizontal());
            Node* lineStart = node->firstChild;
            float totalH = 0.0f;
            bool hasLines = false;

            while (lineStart != nullptr) {
                Node* lineEnd = lineStart;
                float currentLineWidth = 0.0f;
                float lineMaxH = 0.0f;
                uint32_t itemsInLine = 0;

                while (lineEnd != nullptr) {
                    if (lineEnd->isSpacer || lineEnd->type == NodeType::Sidebar || lineEnd->type == NodeType::Modal) {
                        lineEnd = lineEnd->nextSibling;
                        continue;
                    }

                    const float cw = (lineEnd->widthMode == SizeMode::Fixed) ? lineEnd->fixedWidth : lineEnd->intrinsicSize.x;
                    const float ch = (lineEnd->heightMode == SizeMode::Fixed) ? lineEnd->fixedHeight : lineEnd->intrinsicSize.y;
                    const float neededWidth = (itemsInLine > 0) ? (node->gap + cw) : cw;

                    if (itemsInLine > 0 && currentLineWidth + neededWidth > maxLineWidth) {
                        break;
                    }

                    currentLineWidth += neededWidth;
                    lineMaxH = std::max(lineMaxH, ch);
                    ++itemsInLine;
                    lineEnd = lineEnd->nextSibling;
                }

                if (itemsInLine == 0) {
                    break;
                }

                if (hasLines) {
                    totalH += node->runGap;
                }
                totalH += lineMaxH;
                hasLines = true;
                lineStart = lineEnd;
            }

            return std::max(node->minHeight, totalH + node->padding.Vertical());
        }

        if (node->direction == FlexDirection::Row && node->heightMode == SizeMode::Fit) {
            const float contentW = std::max(0.0f, availableWidth - node->padding.Horizontal());
            uint32_t flowChildCount = 0;
            float baseSum = 0.0f;
            float totalFlexGrow = 0.0f;

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;
                ++flowChildCount;
                if (child->isSpacer || child->widthMode == SizeMode::Stretch) {
                    totalFlexGrow += std::max(0.01f, child->flexGrow);
                } else if (child->widthMode == SizeMode::Fixed) {
                    baseSum += child->fixedWidth;
                } else {
                    baseSum += child->intrinsicSize.x;
                }
            }

            const float totalGap = (flowChildCount > 1)
                ? static_cast<float>(flowChildCount - 1) * node->gap
                : 0.0f;
            const float freeSpace = std::max(0.0f, contentW - totalGap - baseSum);

            float maxH = 0.0f;
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->isSpacer || child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;
                float childW = 0.0f;
                if (child->widthMode == SizeMode::Stretch) {
                    childW = (totalFlexGrow > 0.0f)
                        ? (freeSpace * (std::max(0.01f, child->flexGrow) / totalFlexGrow))
                        : 0.0f;
                } else if (child->widthMode == SizeMode::Fixed) {
                    childW = child->fixedWidth;
                } else {
                    childW = child->intrinsicSize.x;
                }
                if (child->minWidth > 0.0f) {
                    childW = std::max(childW, child->minWidth);
                }
                const float ch = ResolveNodeHeight(child, childW, canvas, activeTheme);
                maxH = std::max(maxH, ch);
            }
            return maxH + node->padding.Vertical();
        }

        if (node->direction == FlexDirection::Column && node->heightMode == SizeMode::Fit) {
            const float childAvailW = std::max(0.0f, availableWidth - node->padding.Horizontal());
            float sumH = node->padding.Vertical();
            uint32_t activeChildren = 0;

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->isSpacer || child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;
                if (child->type == NodeType::BottomPanel && child->sidebarCollapsed) continue;
                const float cw = ResolveColumnChildWidth(node, child, childAvailW);
                const float ch = ResolveNodeHeight(child, cw, canvas, activeTheme);
                sumH += ch;
                ++activeChildren;
            }

            if (activeChildren > 1) {
                sumH += static_cast<float>(activeChildren - 1) * node->gap;
            }
            return std::max(node->minHeight, sumH);
        }

        if (node->direction == FlexDirection::Grid) {
            const float availW = std::max(0.0f, availableWidth - node->padding.Horizontal());
            const int totalCols = (node->gridColumns > 0) ? node->gridColumns : 12;
            const float gap = node->gap;
            const float runGap = (node->runGap > 0.0f) ? node->runGap : node->gap;

            float totalH = 0.0f;
            bool hasLines = false;
            Node* lineStart = node->firstChild;

            while (lineStart != nullptr) {
                Node* lineEnd = lineStart;
                int currentLineSpan = 0;
                float lineMaxH = 0.0f;
                uint32_t itemsInLine = 0;

                while (lineEnd != nullptr) {
                    if (lineEnd->isSpacer || lineEnd->type == NodeType::Sidebar || lineEnd->type == NodeType::Modal) {
                        lineEnd = lineEnd->nextSibling;
                        continue;
                    }

                    int span = std::clamp<int>(lineEnd->colSpan, 1, totalCols);
                    const float reqMinW = (lineEnd->minWidth > 0.0f) ? lineEnd->minWidth : ((node->minColWidth > 0.0f) ? node->minColWidth : lineEnd->intrinsicSize.x);

                    if (itemsInLine > 0) {
                        if (currentLineSpan + span > totalCols) {
                            break;
                        }
                        if (reqMinW > 0.0f) {
                            const float testW = (static_cast<float>(span) / static_cast<float>(totalCols)) * (availW + gap) - gap;
                            if (testW < reqMinW) {
                                break;
                            }
                        }
                    } else {
                        if (reqMinW > 0.0f) {
                            const float testW = (static_cast<float>(span) / static_cast<float>(totalCols)) * (availW + gap) - gap;
                            if (testW < reqMinW) {
                                span = totalCols;
                            }
                        }
                    }

                    const float childW = std::max(0.0f, (static_cast<float>(span) / static_cast<float>(totalCols)) * (availW + gap) - gap);
                    const float childH = ResolveNodeHeight(lineEnd, childW, canvas, activeTheme);

                    lineMaxH = std::max(lineMaxH, childH);
                    currentLineSpan += span;
                    ++itemsInLine;
                    lineEnd = lineEnd->nextSibling;
                }

                if (itemsInLine == 0) {
                    break;
                }

                if (itemsInLine == 1 && lineEnd != nullptr) {
                    const float childW = availW;
                    lineMaxH = ResolveNodeHeight(lineStart, childW, canvas, activeTheme);
                }

                if (hasLines) {
                    totalH += runGap;
                }
                totalH += lineMaxH;
                hasLines = true;
                lineStart = lineEnd;
            }

            return std::max(node->minHeight, totalH + node->padding.Vertical());
        }

        return node->intrinsicSize.y;
    }

    static void ArrangeNode(Node* node, Rect availableRect, const UiTheme& theme, ICanvas* canvas) {
        if (!node) return;

        const UiTheme& activeTheme = (node->theme != nullptr) ? *node->theme : theme;

        // Custom autonomous arrangement delegate (e.g. Carousel, custom containers)
        if (node->arrangeFn) {
            node->arrangeFn(node, availableRect, activeTheme);
            return;
        }

        // Resolve this node's own width and height
        float w = availableRect.w;
        float h = availableRect.h;

        if (node->parent == nullptr && !node->isSpacer) {
            if (node->widthMode == SizeMode::Fit) {
                w = node->intrinsicSize.x;
            } else if (node->widthMode == SizeMode::Fixed) {
                w = node->fixedWidth;
            }

            if (node->heightMode == SizeMode::Fit) {
                h = ResolveNodeHeight(node, w, canvas, activeTheme);
            } else if (node->heightMode == SizeMode::Fixed) {
                h = node->fixedHeight;
            }
        }

        if (node->type == NodeType::Modal) {
            const float targetW = node->fixedWidth > 0.0f ? node->fixedWidth : node->intrinsicSize.x;
            const float resolvedH = (node->heightMode == SizeMode::Fixed)
                ? node->fixedHeight
                : ResolveNodeHeight(node, targetW, canvas, activeTheme);
            node->computedRect = Modal::ResolveDialogRect(node, resolvedH, availableRect);
        } else {
            node->computedRect = Rect(availableRect.x, availableRect.y, w, h);
        }

        if (node->childCount == 0) {
            return;
        }

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Modal) {
                ArrangeNode(child, node->computedRect, activeTheme, canvas);
            }
        }

        if (node->type == NodeType::ScrollArea || ((node->type == NodeType::Sidebar || node->type == NodeType::BottomPanel) && node->sidebarScrollable)) {
            const ScrollbarMetrics& sm = activeTheme.GetScrollbar(node->uiSize);
            const float fullW = std::max(0.0f, node->computedRect.w - node->padding.Horizontal());
            const float fullH = std::max(0.0f, node->computedRect.h - node->padding.Vertical());
            const bool allowV = (node->scrollOrientation == Orientation::Vertical || node->scrollOrientation == Orientation::Both);
            const bool allowH = (node->scrollOrientation == Orientation::Horizontal || node->scrollOrientation == Orientation::Both);

            // 1. Determine if horizontal scrollbar is required
            float naturalContentW = 0.0f;
            if (allowH) {
                if (node->direction == FlexDirection::Row) {
                    float sumW = 0.0f;
                    uint32_t active = 0;
                    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                        if (child->isSpacer || child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;
                        const bool isFullStretchViewport = (child->widthMode == SizeMode::Stretch && child->heightMode == SizeMode::Stretch && child->clipChildren);
                        const float cw = (child->widthMode == SizeMode::Fixed)
                            ? child->fixedWidth
                            : (isFullStretchViewport ? child->minWidth : child->intrinsicSize.x);
                        sumW += cw;
                        ++active;
                    }
                    if (active > 1) sumW += static_cast<float>(active - 1) * node->gap;
                    naturalContentW = sumW;
                } else {
                    float maxW = 0.0f;
                    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                        if (child->isSpacer || child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;
                        const bool isFullStretchViewport = (child->widthMode == SizeMode::Stretch && child->heightMode == SizeMode::Stretch && child->clipChildren);
                        const float cw = (child->widthMode == SizeMode::Fixed)
                            ? child->fixedWidth
                            : (isFullStretchViewport ? child->minWidth : child->intrinsicSize.x);
                        maxW = std::max(maxW, cw);
                    }
                    naturalContentW = maxW;
                }
            }

            const bool needScrollbarX = allowH && (
                node->scrollbarPolicy == VisibilityPolicy::Always ||
                (node->scrollbarPolicy == VisibilityPolicy::Auto && naturalContentW > fullW + 0.5f)
            );
            const float gutterY = needScrollbarX ? sm.gutter : 0.0f;
            const float viewportH = std::max(0.0f, fullH - gutterY);
            const float totalViewportH = std::max(0.0f, node->computedRect.h - gutterY);

            // 2. Determine if vertical scrollbar is required
            bool needScrollbarY = allowV && (node->scrollbarPolicy == VisibilityPolicy::Always);
            if (!needScrollbarY && allowV && node->scrollbarPolicy == VisibilityPolicy::Auto) {
                const float measureW = allowH ? std::max(fullW, naturalContentW) : fullW;
                float testH = 0.0f;
                uint32_t active = 0;
                for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                    if (child->isSpacer || child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;
                    const bool isFullStretchViewport = (child->widthMode == SizeMode::Stretch && child->heightMode == SizeMode::Stretch && child->clipChildren);
                    testH += isFullStretchViewport
                        ? std::max(viewportH, child->minHeight)
                        : ResolveNodeHeight(child, measureW, canvas, activeTheme);
                    ++active;
                }
                if (active > 1) {
                    testH += static_cast<float>(active - 1) * node->gap;
                }
                testH += node->padding.Vertical();
                if (testH > totalViewportH + 0.5f) {
                    needScrollbarY = true;
                }
            }

            // 3. Reserve gutter so scrollbar never overlays content
            const float activeGutterX = (needScrollbarY && allowV) ? sm.gutter : 0.0f;
            const float viewportW = std::max(0.0f, fullW - activeGutterX);
            const float totalViewportW = std::max(0.0f, node->computedRect.w - activeGutterX);
            const float effectiveContentW = allowH ? std::max(viewportW, naturalContentW) : viewportW;

            float contentH = 0.0f;
            uint32_t activeChildren = 0;
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->isSpacer || child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;
                const bool isFullStretchViewport = (child->widthMode == SizeMode::Stretch && child->heightMode == SizeMode::Stretch && child->clipChildren);
                const float ch = isFullStretchViewport
                    ? std::max(viewportH, child->minHeight)
                    : ResolveNodeHeight(child, effectiveContentW, canvas, activeTheme);
                if (node->direction == FlexDirection::Row) {
                    contentH = std::max(contentH, ch);
                } else {
                    contentH += ch;
                }
                ++activeChildren;
            }
            if (node->direction != FlexDirection::Row && activeChildren > 1) {
                contentH += static_cast<float>(activeChildren - 1) * node->gap;
            }
            contentH += node->padding.Vertical();

            node->scrollContentSize = {effectiveContentW + node->padding.Horizontal(), contentH};

            const float maxScrollY = allowV ? std::max(0.0f, contentH - totalViewportH) : 0.0f;
            const float maxScrollX = allowH ? std::max(0.0f, node->scrollContentSize.x - totalViewportW) : 0.0f;
            node->scrollOffset.y = std::clamp(node->scrollOffset.y, 0.0f, maxScrollY);
            node->scrollOffset.x = std::clamp(node->scrollOffset.x, 0.0f, maxScrollX);

            const float effectiveContentH = allowV
                ? std::max(viewportH, contentH - node->padding.Vertical())
                : viewportH;
            const Rect scrollContentRect(
                node->computedRect.x + node->padding.left - node->scrollOffset.x,
                node->computedRect.y + node->padding.top - node->scrollOffset.y,
                effectiveContentW,
                effectiveContentH
            );

            if (node->direction == FlexDirection::Grid) {
                ArrangeGrid(node, scrollContentRect, activeTheme, canvas);
            } else if (node->direction == FlexDirection::Row) {
                ArrangeRow(node, scrollContentRect, activeTheme, canvas);
            } else {
                ArrangeColumn(node, scrollContentRect, activeTheme, canvas);
            }
            return;
        }

        Rect contentRect(
            node->computedRect.x + node->padding.left,
            node->computedRect.y + node->padding.top,
            std::max(0.0f, node->computedRect.w - node->padding.Horizontal()),
            std::max(0.0f, node->computedRect.h - node->padding.Vertical())
        );

        // Dock direct NodeType::Sidebar children to Left / Right in call order
        bool hasSidebars = false;
        uint32_t nonSidebarCount = 0;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Modal) continue;
            if (child->type == NodeType::Sidebar) {
                hasSidebars = true;
            } else {
                ++nonSidebarCount;
            }
        }

        if (hasSidebars) {
            float leftX = contentRect.x;
            float rightX = contentRect.Right();
            const float topY = contentRect.y;
            const float availH = contentRect.h;
            uint32_t leftSidebarCount = 0;
            uint32_t rightSidebarCount = 0;
            bool prevLeftFlush = false;
            bool prevRightFlush = false;

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type != NodeType::Sidebar) continue;

                if (child->sidebarCollapsed) {
                    const float edgeX = !child->sidebarRight ? leftX : rightX;
                    child->computedRect = Rect(edgeX, topY, 0.0f, availH);
                    continue;
                }

                const UiTheme& childTheme = (child->theme != nullptr) ? *child->theme : activeTheme;
                float sbW = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                if (child->widthMode != SizeMode::Fixed && child->sidebarScrollable) {
                    const bool needSb = (child->scrollbarPolicy == VisibilityPolicy::Always) ||
                        (child->scrollbarPolicy == VisibilityPolicy::Auto && child->scrollContentSize.y > availH + 0.5f);
                    if (needSb) {
                        sbW += childTheme.GetScrollbar(child->uiSize).gutter;
                    }
                }
                if (child->minWidth > 0.0f) {
                    sbW = std::max(sbW, child->minWidth);
                }

                if (!child->sidebarRight) {
                    if (leftSidebarCount > 0 && !prevLeftFlush) {
                        leftX += node->gap;
                    }
                    sbW = std::min(sbW, std::max(0.0f, rightX - leftX));
                    ArrangeNode(child, Rect(leftX, topY, sbW, availH), activeTheme, canvas);
                    leftX += sbW;
                    prevLeftFlush = (child->sidebarFlush != 0);
                    ++leftSidebarCount;
                } else {
                    if (rightSidebarCount > 0 && !prevRightFlush) {
                        rightX -= node->gap;
                    }
                    sbW = std::min(sbW, std::max(0.0f, rightX - leftX));
                    ArrangeNode(child, Rect(rightX - sbW, topY, sbW, availH), activeTheme, canvas);
                    rightX -= sbW;
                    prevRightFlush = (child->sidebarFlush != 0);
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
            contentRect = Rect(leftX, topY, std::max(0.0f, rightX - leftX), availH);
        }

        if (node->direction == FlexDirection::Row) {
            ArrangeRow(node, contentRect, activeTheme, canvas);
        } else if (node->direction == FlexDirection::Wrap) {
            ArrangeWrap(node, contentRect, activeTheme, canvas);
        } else if (node->direction == FlexDirection::Grid) {
            ArrangeGrid(node, contentRect, activeTheme, canvas);
        } else {
            ArrangeColumn(node, contentRect, activeTheme, canvas);
        }
    }

    static void ArrangeRow(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas) {
        uint32_t flowChildCount = 0;
        float baseSum = 0.0f;
        float totalFlexGrow = 0.0f;
        uint32_t flexCount = 0;

        float maxEqualTrackW = 0.0f;
        uint32_t spacerCount = 0;
        if (node->equalWidths) {
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;
                if (child->isSpacer) {
                    ++spacerCount;
                } else if (child->widthMode != SizeMode::Fixed) {
                    maxEqualTrackW = std::max(maxEqualTrackW, std::max(child->minWidth, child->intrinsicSize.x));
                }
            }
        }
        const bool stretchEqualTracks = node->equalWidths &&
            (node->widthMode != SizeMode::Fit) &&
            (spacerCount == 0) &&
            (node->justify == Justify::Start);

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;
            ++flowChildCount;
            const bool isEqualTrack = node->equalWidths && !child->isSpacer && (child->widthMode != SizeMode::Fixed);
            if (child->isSpacer || (isEqualTrack && stretchEqualTracks) || (!node->equalWidths && child->widthMode == SizeMode::Stretch)) {
                const float fg = (isEqualTrack && stretchEqualTracks) ? 1.0f : std::max(0.01f, child->flexGrow);
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

        const float totalGap = (flowChildCount > 1)
            ? static_cast<float>(flowChildCount - 1) * node->gap
            : 0.0f;

        const float rawFreeSpace = contentRect.w - totalGap - baseSum;
        const float freeSpace = std::max(0.0f, rawFreeSpace);
        float currentX = contentRect.x;

        // Compute horizontal deficit absorption for Fit Text/Heading nodes in constrained Rows
        const bool constrainedRow = (node->widthMode != SizeMode::Fit && contentRect.w > 0.0f);
        float remDeficit = (constrainedRow && rawFreeSpace < 0.0f && flowChildCount > 1) ? -rawFreeSpace : 0.0f;
        float totalLongTextExcess = 0.0f;
        const float shortBadgeThreshold = theme.Scale(68.0f);
        if (remDeficit > 0.0f) {
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type == NodeType::Sidebar || child->type == NodeType::Modal || child->isSpacer) continue;
                const bool isEqualTrack = node->equalWidths && (child->widthMode != SizeMode::Fixed);
                if (!isEqualTrack && child->widthMode == SizeMode::Fit &&
                    (child->type == NodeType::Text || child->type == NodeType::Heading)) {
                    const float preserveW = std::max(child->minWidth, std::min(child->intrinsicSize.x, shortBadgeThreshold));
                    totalLongTextExcess += std::max(0.0f, child->intrinsicSize.x - preserveW);
                }
            }
        }
        const float takeFromLongText = std::min(remDeficit, totalLongTextExcess);
        const float postStage1Deficit = std::max(0.0f, remDeficit - takeFromLongText);
        float totalRemainingTextW = 0.0f;
        if (postStage1Deficit > 0.0f) {
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type == NodeType::Sidebar || child->type == NodeType::Modal || child->isSpacer) continue;
                const bool isEqualTrack = node->equalWidths && (child->widthMode != SizeMode::Fixed);
                if (!isEqualTrack && child->widthMode == SizeMode::Fit &&
                    (child->type == NodeType::Text || child->type == NodeType::Heading)) {
                    const float preserveW = std::max(child->minWidth, std::min(child->intrinsicSize.x, shortBadgeThreshold));
                    totalRemainingTextW += std::max(0.0f, preserveW - child->minWidth);
                }
            }
        }
        const float takeFromShortText = std::min(postStage1Deficit, totalRemainingTextW);

        // Main axis justification when there are no stretch elements
        if (flexCount == 0) {
            float effectiveFreeSpace = rawFreeSpace + takeFromLongText + takeFromShortText;
            if (constrainedRow && flowChildCount == 1 && effectiveFreeSpace < 0.0f) {
                const Node* singleChild = nullptr;
                for (Node* c = node->firstChild; c != nullptr; c = c->nextSibling) {
                    if (c->type != NodeType::Sidebar && c->type != NodeType::Modal) {
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
            }
        }

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;
            float childW = 0.0f;
            const bool isEqualTrack = node->equalWidths && !child->isSpacer && (child->widthMode != SizeMode::Fixed);
            if (child->isSpacer || (isEqualTrack && stretchEqualTracks) || (!node->equalWidths && child->widthMode == SizeMode::Stretch)) {
                const float fg = (isEqualTrack && stretchEqualTracks) ? 1.0f : std::max(0.01f, child->flexGrow);
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
                    childW = std::min(childW, contentRect.w);
                } else if (remDeficit > 0.0f && (child->type == NodeType::Text || child->type == NodeType::Heading)) {
                    const float preserveW = std::max(child->minWidth, std::min(child->intrinsicSize.x, shortBadgeThreshold));
                    const float excessW = std::max(0.0f, child->intrinsicSize.x - preserveW);
                    if (takeFromLongText > 0.0f && totalLongTextExcess > 0.0f && excessW > 0.0f) {
                        childW -= takeFromLongText * (excessW / totalLongTextExcess);
                    }
                    if (takeFromShortText > 0.0f && totalRemainingTextW > 0.0f) {
                        const float remShrinkW = std::max(0.0f, preserveW - child->minWidth);
                        childW -= takeFromShortText * (remShrinkW / totalRemainingTextW);
                    }
                    childW = std::max(child->minWidth, childW);
                }
            }

            if (child->minWidth > 0.0f) {
                childW = std::max(childW, child->minWidth);
            }
            if (constrainedRow && flowChildCount == 1 && child->widthMode != SizeMode::Fixed) {
                childW = std::min(childW, contentRect.w);
            }

            float childH = contentRect.h;
            if (child->heightMode == SizeMode::Fixed) {
                childH = child->fixedHeight;
            } else if (child->heightMode == SizeMode::Fit || (child->heightMode != SizeMode::Stretch && node->align != Alignment::Stretch)) {
                childH = ResolveNodeHeight(child, childW, canvas, theme);
            }
            if (!child->isSpacer && child->minHeight > 0.0f) {
                childH = std::max(childH, child->minHeight);
            }
            if (node->heightMode == SizeMode::Stretch && contentRect.h > 0.0f && child->heightMode != SizeMode::Fixed) {
                childH = std::min(childH, contentRect.h);
            }

            float childY = contentRect.y;
            if (node->align == Alignment::Center) {
                childY = contentRect.y + (contentRect.h - childH) * 0.5f;
            } else if (node->align == Alignment::End) {
                childY = contentRect.Bottom() - childH;
            }

            ArrangeNode(child, Rect(currentX, childY, childW, childH), theme, canvas);
            currentX += childW + node->gap;
        }

        if (node->widthMode == SizeMode::Fit) {
            node->computedRect.w = (currentX > contentRect.x)
                ? (currentX - contentRect.x - node->gap) + node->padding.Horizontal()
                : node->padding.Horizontal();
        }
    }

    static void ArrangeWrap(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas) {
        const float maxLineWidth = std::max(0.0f, contentRect.w);
        Node* lineStart = node->firstChild;
        float currentY = contentRect.y;
        bool hasLines = false;

        while (lineStart != nullptr) {
            Node* lineEnd = lineStart;
            float currentLineWidth = 0.0f;
            float lineMaxH = 0.0f;
            uint32_t itemsInLine = 0;

            while (lineEnd != nullptr) {
                if (lineEnd->isSpacer || lineEnd->type == NodeType::Sidebar || lineEnd->type == NodeType::Modal) {
                    lineEnd = lineEnd->nextSibling;
                    continue;
                }

                const float cw = (lineEnd->widthMode == SizeMode::Fixed) ? lineEnd->fixedWidth : lineEnd->intrinsicSize.x;
                const float ch = (lineEnd->heightMode == SizeMode::Fixed) ? lineEnd->fixedHeight : lineEnd->intrinsicSize.y;
                const float neededWidth = (itemsInLine > 0) ? (node->gap + cw) : cw;

                if (itemsInLine > 0 && currentLineWidth + neededWidth > maxLineWidth) {
                    break;
                }

                currentLineWidth += neededWidth;
                lineMaxH = std::max(lineMaxH, ch);
                ++itemsInLine;
                lineEnd = lineEnd->nextSibling;
            }

            if (itemsInLine == 0) {
                break;
            }

            if (hasLines) {
                currentY += node->runGap;
            }

            // Main axis justification for this line
            float itemX = contentRect.x;
            float itemGap = node->gap;
            const float freeLineWidth = std::max(0.0f, maxLineWidth - currentLineWidth);

            if (node->justify == Justify::Center) {
                itemX += freeLineWidth * 0.5f;
            } else if (node->justify == Justify::End) {
                itemX += freeLineWidth;
            } else if (node->justify == Justify::SpaceBetween && itemsInLine > 1) {
                itemGap += freeLineWidth / static_cast<float>(itemsInLine - 1);
            }

            // Place items along this line
            for (Node* child = lineStart; child != lineEnd; child = child->nextSibling) {
                if (child->isSpacer || child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;

                const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                float ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;

                if (node->align == Alignment::Stretch && child->heightMode == SizeMode::Stretch) {
                    ch = lineMaxH;
                }

                float childY = currentY;
                if (node->align == Alignment::Center) {
                    childY = currentY + (lineMaxH - ch) * 0.5f;
                } else if (node->align == Alignment::End) {
                    childY = currentY + (lineMaxH - ch);
                }

                ArrangeNode(child, Rect(itemX, childY, cw, ch), theme, canvas);
                itemX += cw + itemGap;
            }

            currentY += lineMaxH;
            hasLines = true;
            lineStart = lineEnd;
        }

        if (node->heightMode == SizeMode::Fit) {
            node->computedRect.h = std::max(
                node->minHeight,
                (currentY - contentRect.y) + node->padding.Vertical()
            );
        }
    }

    static void ArrangeColumn(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas) {
        const bool parentHasDefiniteHeight = (node->heightMode != SizeMode::Fit) &&
            (node->type != NodeType::ScrollArea || node->scrollbarPolicy == VisibilityPolicy::Never) &&
            !((node->type == NodeType::Sidebar || node->type == NodeType::BottomPanel) && node->sidebarScrollable);

        auto isChildStretchHeight = [&](const Node* child) noexcept {
            if (child->heightMode != SizeMode::Stretch) return false;
            if (parentHasDefiniteHeight) return true;
            return child->widthMode == SizeMode::Stretch && child->clipChildren;
        };

        uint32_t flowChildCount = 0;
        float baseSum = 0.0f;
        float totalFlexGrow = 0.0f;
        uint32_t flexCount = 0;

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;
            if (child->type == NodeType::BottomPanel && child->sidebarCollapsed) continue;
            ++flowChildCount;
            if (child->isSpacer || isChildStretchHeight(child)) {
                totalFlexGrow += std::max(0.01f, child->flexGrow);
                ++flexCount;
            } else if (child->heightMode == SizeMode::Fixed) {
                baseSum += child->fixedHeight;
            } else if (!child->isSpacer) { // SizeMode::Fit or Stretch inside Fit/ScrollArea parent
                const float cw = ResolveColumnChildWidth(node, child, contentRect.w);
                baseSum += ResolveNodeHeight(child, cw, canvas, theme);
            }
        }

        const float totalGap = (flowChildCount > 1)
            ? static_cast<float>(flowChildCount - 1) * node->gap
            : 0.0f;

        const float freeSpace = std::max(0.0f, contentRect.h - totalGap - baseSum);
        float currentY = contentRect.y;

        // Main axis justification when there are no stretch elements
        if (flexCount == 0 && freeSpace > 0.0f) {
            if (node->justify == Justify::Center) {
                currentY += freeSpace * 0.5f;
            } else if (node->justify == Justify::End) {
                currentY += freeSpace;
            }
        }

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;
            if (child->type == NodeType::BottomPanel && child->sidebarCollapsed) {
                const float edgeY = (currentY > contentRect.y)
                    ? std::clamp(currentY - node->gap, contentRect.y, contentRect.Bottom())
                    : contentRect.Bottom();
                child->computedRect = Rect(contentRect.x, edgeY, contentRect.w, 0.0f);
                continue;
            }
            const float childW = ResolveColumnChildWidth(node, child, contentRect.w);

            float childH = 0.0f;
            if (child->isSpacer || isChildStretchHeight(child)) {
                childH = (totalFlexGrow > 0.0f)
                    ? (freeSpace * (std::max(0.01f, child->flexGrow) / totalFlexGrow))
                    : 0.0f;
                if (!child->isSpacer && child->minHeight > 0.0f) {
                    childH = std::max(childH, child->minHeight);
                }
            } else if (child->heightMode == SizeMode::Fixed) {
                childH = child->fixedHeight;
            } else if (!child->isSpacer) { // SizeMode::Fit or Stretch inside Fit/ScrollArea parent
                childH = ResolveNodeHeight(child, childW, canvas, theme);
            }

            float childX = contentRect.x;
            if (node->align == Alignment::Center) {
                childX = contentRect.x + (contentRect.w - childW) * 0.5f;
            } else if (node->align == Alignment::End) {
                childX = contentRect.Right() - childW;
            }

            ArrangeNode(child, Rect(childX, currentY, childW, childH), theme, canvas);
            currentY += child->computedRect.h + node->gap;
        }

        if (node->heightMode == SizeMode::Fit && node->type != NodeType::Modal) {
            node->computedRect.h = (currentY > contentRect.y)
                ? (currentY - contentRect.y - node->gap) + node->padding.Vertical()
                : node->padding.Vertical();
        }
    }

    static void ArrangeGrid(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas) {
        const float availW = contentRect.w;
        const int totalCols = (node->gridColumns > 0) ? node->gridColumns : 12;
        const float gap = node->gap;
        const float runGap = (node->runGap > 0.0f) ? node->runGap : node->gap;

        float currentY = contentRect.y;
        bool hasLines = false;
        Node* lineStart = node->firstChild;

        while (lineStart != nullptr) {
            Node* lineEnd = lineStart;
            int currentLineSpan = 0;
            float lineMaxH = 0.0f;
            uint32_t itemsInLine = 0;

            // Pass 1: measure items fitting in line and find maximum line height
            while (lineEnd != nullptr) {
                if (lineEnd->isSpacer || lineEnd->type == NodeType::Sidebar || lineEnd->type == NodeType::Modal) {
                    lineEnd = lineEnd->nextSibling;
                    continue;
                }

                int span = std::clamp<int>(lineEnd->colSpan, 1, totalCols);
                const float reqMinW = (lineEnd->minWidth > 0.0f) ? lineEnd->minWidth : ((node->minColWidth > 0.0f) ? node->minColWidth : lineEnd->intrinsicSize.x);

                if (itemsInLine > 0) {
                    if (currentLineSpan + span > totalCols) {
                        break;
                    }
                    if (reqMinW > 0.0f) {
                        const float testW = (static_cast<float>(span) / static_cast<float>(totalCols)) * (availW + gap) - gap;
                        if (testW < reqMinW) {
                            break;
                        }
                    }
                } else {
                    if (reqMinW > 0.0f) {
                        const float testW = (static_cast<float>(span) / static_cast<float>(totalCols)) * (availW + gap) - gap;
                        if (testW < reqMinW) {
                            span = totalCols;
                        }
                    }
                }

                const float childW = std::max(0.0f, (static_cast<float>(span) / static_cast<float>(totalCols)) * (availW + gap) - gap);
                float childH = 0.0f;
                if (lineEnd->heightMode == SizeMode::Fixed) {
                    childH = lineEnd->fixedHeight;
                } else {
                    childH = ResolveNodeHeight(lineEnd, childW, canvas, theme);
                }

                lineMaxH = std::max(lineMaxH, childH);
                currentLineSpan += span;
                ++itemsInLine;
                lineEnd = lineEnd->nextSibling;
            }

            if (itemsInLine == 0) {
                break;
            }

            if (itemsInLine == 1 && lineEnd != nullptr) {
                const float childW = availW;
                lineMaxH = (lineStart->heightMode == SizeMode::Fixed)
                    ? lineStart->fixedHeight
                    : ResolveNodeHeight(lineStart, childW, canvas, theme);
            }

            if (hasLines) {
                currentY += runGap;
            }

            // Pass 2: position items along the line
            float currentX = contentRect.x;
            for (Node* child = lineStart; child != lineEnd; child = child->nextSibling) {
                if (child->isSpacer || child->type == NodeType::Sidebar || child->type == NodeType::Modal) continue;

                int span = std::clamp<int>(child->colSpan, 1, totalCols);
                const float reqMinW = (child->minWidth > 0.0f) ? child->minWidth : ((node->minColWidth > 0.0f) ? node->minColWidth : child->intrinsicSize.x);
                if (itemsInLine == 1 && (lineEnd != nullptr || (reqMinW > 0.0f && ((static_cast<float>(span) / static_cast<float>(totalCols)) * (availW + gap) - gap) < reqMinW))) {
                    span = totalCols;
                }

                const float childW = std::max(0.0f, (static_cast<float>(span) / static_cast<float>(totalCols)) * (availW + gap) - gap);

                float childH = 0.0f;
                if (child->heightMode == SizeMode::Fixed) {
                    childH = child->fixedHeight;
                } else if (child->heightMode == SizeMode::Stretch || node->align == Alignment::Stretch) {
                    childH = lineMaxH;
                } else {
                    childH = ResolveNodeHeight(child, childW, canvas, theme);
                }

                float childY = currentY;
                if (node->align == Alignment::Center) {
                    childY = currentY + (lineMaxH - childH) * 0.5f;
                } else if (node->align == Alignment::End) {
                    childY = currentY + (lineMaxH - childH);
                }

                ArrangeNode(child, Rect(currentX, childY, childW, childH), theme, canvas);
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
        }
    }

} // namespace

void Solver::Measure(Node* root, ICanvas* canvas, const UiTheme& theme) {
    if (!root) return;
    MeasureNode(root, canvas, theme);
}

void Solver::Measure(Node* root, ICanvas* canvas) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    Measure(root, canvas, defaultTheme);
}

void Solver::Arrange(Node* root, Rect availableRect, const UiTheme& theme, ICanvas* canvas) {
    if (!root) return;
    ArrangeNode(root, availableRect, theme, canvas);
}

void Solver::Arrange(Node* root, Rect availableRect) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    Arrange(root, availableRect, defaultTheme, nullptr);
}

} // namespace guikit
