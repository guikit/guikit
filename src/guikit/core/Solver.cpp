#include "guikit/core/Solver.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/Theme.hpp"

namespace guikit {

namespace {

static void MeasureNode(Node* node, ICanvas* canvas, const UiTheme& theme);
static void ArrangeNode(Node* node, Rect availableRect, const UiTheme& theme, ICanvas* canvas = nullptr);
static float ResolveNodeHeight(Node* node, float availableWidth, ICanvas* canvas, const UiTheme& theme);
static void ArrangeRow(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas);
static void ArrangeWrap(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas);
static void ArrangeColumn(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas);
static void ArrangeGrid(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas);

    static void MeasureNode(Node* node, ICanvas* canvas, const UiTheme& theme) {
        const UiTheme& activeTheme = (node->theme != nullptr) ? *node->theme : theme;

        // Measure children bottom-up first
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            MeasureNode(child, canvas, activeTheme);
        }

        // Custom autonomous measure delegate (leaves or specialized containers)
        if (node->measureFn) {
            node->intrinsicSize = node->measureFn(node, 0.0f, canvas, activeTheme);
            return;
        }

        // If this is a leaf widget without custom measure delegate, compute its intrinsic dimensions
        if (node->childCount == 0) {
            if (node->widthMode == SizeMode::Fixed || node->heightMode == SizeMode::Fixed) {
                node->intrinsicSize = {node->fixedWidth, node->fixedHeight};
            } else {
                node->intrinsicSize = {0.0f, 0.0f};
            }
            return;
        }

        if (node->type == NodeType::ScrollArea || node->type == NodeType::Sidebar) {
            float totalHeight = node->padding.Vertical();
            float maxWidth = 0.0f;
            uint32_t activeChildren = 0;

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
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

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                const float ch = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight : child->intrinsicSize.y;
                totalWidth += cw;
                maxHeight = std::max(maxHeight, ch);
                ++activeChildren;
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
                    if (lineEnd->isSpacer) {
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
            return std::max(node->minHeight, h);
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
                    if (lineEnd->isSpacer || lineEnd->type == NodeType::Sidebar) {
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

            return totalH + node->padding.Vertical();
        }

        if (node->direction == FlexDirection::Row && node->heightMode == SizeMode::Fit) {
            const float contentW = std::max(0.0f, availableWidth - node->padding.Horizontal());
            uint32_t flowChildCount = 0;
            float baseSum = 0.0f;
            float totalFlexGrow = 0.0f;

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type == NodeType::Sidebar) continue;
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
                if (child->isSpacer || child->type == NodeType::Sidebar) continue;
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
                if (child->isSpacer || child->type == NodeType::Sidebar) continue;
                const float ch = ResolveNodeHeight(child, childAvailW, canvas, activeTheme);
                sumH += ch;
                ++activeChildren;
            }

            if (activeChildren > 1) {
                sumH += static_cast<float>(activeChildren - 1) * node->gap;
            }
            return sumH;
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
                    if (lineEnd->isSpacer || lineEnd->type == NodeType::Sidebar) {
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

            return totalH + node->padding.Vertical();
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

        node->computedRect = Rect(availableRect.x, availableRect.y, w, h);

        if (node->childCount == 0) {
            return;
        }

        if (node->type == NodeType::ScrollArea || (node->type == NodeType::Sidebar && node->sidebarScrollable)) {
            const ScrollbarMetrics& sm = activeTheme.GetScrollbar(node->uiSize);
            const float fullW = std::max(0.0f, node->computedRect.w - node->padding.Horizontal());
            const float fullH = std::max(0.0f, node->computedRect.h - node->padding.Vertical());
            const bool allowV = (node->scrollOrientation == Orientation::Vertical || node->scrollOrientation == Orientation::Both);
            const bool allowH = (node->scrollOrientation == Orientation::Horizontal || node->scrollOrientation == Orientation::Both);

            // 1. Determine if horizontal scrollbar is required
            float naturalContentW = fullW;
            if (allowH) {
                if (node->direction == FlexDirection::Row) {
                    float sumW = 0.0f;
                    uint32_t active = 0;
                    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                        if (child->isSpacer || child->type == NodeType::Sidebar) continue;
                        sumW += (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                        ++active;
                    }
                    if (active > 1) sumW += static_cast<float>(active - 1) * node->gap;
                    naturalContentW = std::max(fullW, sumW);
                } else {
                    float maxW = fullW;
                    for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                        if (child->isSpacer || child->type == NodeType::Sidebar) continue;
                        const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
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
                const float measureW = allowH ? naturalContentW : fullW;
                float testH = 0.0f;
                uint32_t active = 0;
                for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                    if (child->isSpacer || child->type == NodeType::Sidebar) continue;
                    testH += ResolveNodeHeight(child, measureW, canvas, activeTheme);
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
                if (child->isSpacer || child->type == NodeType::Sidebar) continue;
                const float ch = ResolveNodeHeight(child, effectiveContentW, canvas, activeTheme);
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

            const Rect scrollContentRect(
                node->computedRect.x + node->padding.left - node->scrollOffset.x,
                node->computedRect.y + node->padding.top - node->scrollOffset.y,
                effectiveContentW,
                allowV ? contentH : viewportH
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

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->type != NodeType::Sidebar) continue;

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
                    if (leftSidebarCount > 0) {
                        leftX += node->gap;
                    }
                    sbW = std::min(sbW, std::max(0.0f, rightX - leftX));
                    ArrangeNode(child, Rect(leftX, topY, sbW, availH), activeTheme, canvas);
                    leftX += sbW;
                    ++leftSidebarCount;
                } else {
                    if (rightSidebarCount > 0) {
                        rightX -= node->gap;
                    }
                    sbW = std::min(sbW, std::max(0.0f, rightX - leftX));
                    ArrangeNode(child, Rect(rightX - sbW, topY, sbW, availH), activeTheme, canvas);
                    rightX -= sbW;
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

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Sidebar) continue;
            ++flowChildCount;
            if (child->isSpacer || child->widthMode == SizeMode::Stretch) {
                totalFlexGrow += std::max(0.01f, child->flexGrow);
                ++flexCount;
            } else if (child->widthMode == SizeMode::Fixed) {
                baseSum += child->fixedWidth;
            } else { // SizeMode::Fit
                baseSum += child->intrinsicSize.x;
            }
        }

        const float totalGap = (flowChildCount > 1)
            ? static_cast<float>(flowChildCount - 1) * node->gap
            : 0.0f;

        const float freeSpace = std::max(0.0f, contentRect.w - totalGap - baseSum);
        float currentX = contentRect.x;

        // Main axis justification when there are no stretch elements
        if (flexCount == 0 && freeSpace > 0.0f) {
            if (node->justify == Justify::Center) {
                currentX += freeSpace * 0.5f;
            } else if (node->justify == Justify::End) {
                currentX += freeSpace;
            }
        }

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Sidebar) continue;
            float childW = 0.0f;
            if (child->isSpacer || child->widthMode == SizeMode::Stretch) {
                childW = (totalFlexGrow > 0.0f)
                    ? (freeSpace * (std::max(0.01f, child->flexGrow) / totalFlexGrow))
                    : 0.0f;
            } else if (child->widthMode == SizeMode::Fixed) {
                childW = child->fixedWidth;
            } else { // SizeMode::Fit
                childW = child->intrinsicSize.x;
                if (node->widthMode != SizeMode::Fit && contentRect.w > 0.0f && flowChildCount == 1) {
                    childW = std::min(childW, contentRect.w);
                }
            }

            if (child->minWidth > 0.0f) {
                childW = std::max(childW, child->minWidth);
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
            if (node->heightMode == SizeMode::Stretch && contentRect.h > 0.0f) {
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
                if (lineEnd->isSpacer || lineEnd->type == NodeType::Sidebar) {
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
                if (child->isSpacer || child->type == NodeType::Sidebar) continue;

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
            node->computedRect.h = (currentY - contentRect.y) + node->padding.Vertical();
        }
    }

    static void ArrangeColumn(Node* node, const Rect& contentRect, const UiTheme& theme, ICanvas* canvas) {
        const bool parentHasDefiniteHeight = (node->heightMode != SizeMode::Fit) &&
            (node->type != NodeType::ScrollArea) &&
            !(node->type == NodeType::Sidebar && node->sidebarScrollable);

        uint32_t flowChildCount = 0;
        float baseSum = 0.0f;
        float totalFlexGrow = 0.0f;
        uint32_t flexCount = 0;

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->type == NodeType::Sidebar) continue;
            ++flowChildCount;
            if (child->isSpacer || (child->heightMode == SizeMode::Stretch && parentHasDefiniteHeight)) {
                totalFlexGrow += std::max(0.01f, child->flexGrow);
                ++flexCount;
            } else if (child->heightMode == SizeMode::Fixed) {
                baseSum += child->fixedHeight;
            } else if (!child->isSpacer) { // SizeMode::Fit or Stretch inside Fit/ScrollArea parent
                baseSum += ResolveNodeHeight(child, contentRect.w, canvas, theme);
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
            if (child->type == NodeType::Sidebar) continue;
            float childH = 0.0f;
            if (child->isSpacer || (child->heightMode == SizeMode::Stretch && parentHasDefiniteHeight)) {
                childH = (totalFlexGrow > 0.0f)
                    ? (freeSpace * (std::max(0.01f, child->flexGrow) / totalFlexGrow))
                    : 0.0f;
                if (!child->isSpacer && child->minHeight > 0.0f) {
                    childH = std::max(childH, child->minHeight);
                }
            } else if (child->heightMode == SizeMode::Fixed) {
                childH = child->fixedHeight;
            } else if (!child->isSpacer) { // SizeMode::Fit or Stretch inside Fit/ScrollArea parent
                childH = ResolveNodeHeight(child, contentRect.w, canvas, theme);
            }

            float childW = contentRect.w;
            if (child->widthMode == SizeMode::Fixed) {
                childW = child->fixedWidth;
            } else if (child->widthMode == SizeMode::Fit || (child->widthMode != SizeMode::Stretch && node->align != Alignment::Stretch)) {
                childW = child->intrinsicSize.x;
                if (node->widthMode != SizeMode::Fit && contentRect.w > 0.0f) {
                    childW = std::min(childW, contentRect.w);
                }
            }

            if (child->minWidth > 0.0f) {
                childW = std::max(childW, child->minWidth);
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

        if (node->heightMode == SizeMode::Fit) {
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
                if (lineEnd->isSpacer || lineEnd->type == NodeType::Sidebar) {
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
                if (child->isSpacer || child->type == NodeType::Sidebar) continue;

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
