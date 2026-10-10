#pragma once

#include <algorithm>
#include <cmath>
#include "guikit/backend/ICanvas.hpp"
#include "guikit/layout/Node.hpp"

namespace guikit {

/// Resolves 2D flex layout for an arbitrary tree of GuiKit nodes.
class LayoutSolver {
public:
    /// Measures intrinsic content sizes bottom-up.
    static void Measure(Node* root, ICanvas* canvas = nullptr) {
        if (!root) return;
        MeasureNode(root, canvas);
    }

    /// Computes final coordinates and dimensions top-down based on parent constraints.
    static void Arrange(Node* root, Rect availableRect) {
        if (!root) return;
        ArrangeNode(root, availableRect);
    }

private:
    static void MeasureNode(Node* node, ICanvas* canvas = nullptr) {
        // Measure children bottom-up first
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            MeasureNode(child, canvas);
        }

        // If this is a leaf widget, compute its intrinsic dimensions
        if (node->childCount == 0) {
            switch (node->type) {
                case NodeType::Text: {
                    node->intrinsicSize = Text::Measure(node->text, node->textOpts, canvas);
                    break;
                }
                case NodeType::Button: {
                    node->intrinsicSize = Button::Measure(node->buttonOpts, node->uiSize, canvas);
                    break;
                }
                case NodeType::Switch: {
                    node->intrinsicSize = Switch::Measure(node->text, node->switchOpts, node->uiSize, canvas);
                    break;
                }
                case NodeType::Checkbox: {
                    node->intrinsicSize = Checkbox::Measure(node->text, node->checkboxOpts, node->uiSize, canvas);
                    break;
                }
                case NodeType::InputField: {
                    node->intrinsicSize = InputField::Measure(node->text, node->displayText, node->inputOpts, node->uiSize, canvas);
                    break;
                }
                case NodeType::Select: {
                    node->intrinsicSize = Select::Measure(node->text, node->selectOpts, node->uiSize, canvas);
                    break;
                }
                case NodeType::Heading: {
                    node->intrinsicSize = Heading::Measure(node->headingOpts, node->uiSize, canvas);
                    break;
                }
                case NodeType::Spacer: {
                    node->intrinsicSize = {0.0f, 0.0f};
                    break;
                }
                case NodeType::TabItem: {
                    node->intrinsicSize = TabBar::MeasureTab(node->tabItemOpts, node->tabBarOpts, node->uiSize, canvas);
                    break;
                }
                case NodeType::Separator: {
                    node->intrinsicSize = {
                        node->fixedWidth > 0.0f ? node->fixedWidth : 10.0f,
                        node->fixedHeight > 0.0f ? node->fixedHeight : 20.0f
                    };
                    break;
                }
                default: {
                    if (node->widthMode == SizeMode::Fixed || node->heightMode == SizeMode::Fixed) {
                        node->intrinsicSize = {node->fixedWidth, node->fixedHeight};
                    } else {
                        node->intrinsicSize = {0.0f, 0.0f};
                    }
                    break;
                }
            }
            return;
        }

        // Container intrinsic size aggregation
        if (node->type == NodeType::Carousel) {
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
            node->scrollContentSize = {totalWidth, maxHeight + node->padding.Vertical()};
            node->intrinsicSize = {totalWidth, maxHeight + node->padding.Vertical()};
            return;
        }

        if (node->type == NodeType::ScrollArea) {
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
            node->scrollContentSize = {maxWidth + node->padding.Horizontal(), totalHeight};
            node->intrinsicSize = {0.0f, 0.0f};
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

                    const int span = std::clamp(lineEnd->colSpan, 1, totalCols);
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
    static float ResolveNodeHeight(Node* node, float availableWidth) {
        if (!node) return 0.0f;
        if (node->heightMode == SizeMode::Fixed) {
            return node->fixedHeight;
        }
        if (node->isSpacer) {
            return 0.0f;
        }
        if (node->childCount == 0) {
            return node->intrinsicSize.y;
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
                    if (lineEnd->isSpacer) {
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

        if (node->direction == FlexDirection::Column && node->heightMode == SizeMode::Fit) {
            const float childAvailW = std::max(0.0f, availableWidth - node->padding.Horizontal());
            float sumH = node->padding.Vertical();
            uint32_t activeChildren = 0;

            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->isSpacer) continue;
                const float ch = ResolveNodeHeight(child, childAvailW);
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
                    if (lineEnd->isSpacer) {
                        lineEnd = lineEnd->nextSibling;
                        continue;
                    }

                    int span = std::clamp(lineEnd->colSpan, 1, totalCols);
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
                    const float childH = ResolveNodeHeight(lineEnd, childW);

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
                    lineMaxH = ResolveNodeHeight(lineStart, childW);
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

    static void ArrangeNode(Node* node, Rect availableRect) {
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
                h = (node->childCount == 0) ? node->intrinsicSize.y : ResolveNodeHeight(node, w);
            } else if (node->heightMode == SizeMode::Fixed) {
                h = node->fixedHeight;
            }
        }

        node->computedRect = Rect(availableRect.x, availableRect.y, w, h);

        if (node->childCount == 0) {
            return;
        }

        if (node->type == NodeType::ScrollArea) {
            const ScrollbarMetrics sm = GetScrollbarMetrics(node->uiSize);
            const float fullW = std::max(0.0f, node->computedRect.w - node->padding.Horizontal());
            const float viewportH = std::max(0.0f, node->computedRect.h - node->padding.Vertical());

            // 1. Determine if vertical scrollbar is required
            bool needScrollbar = (node->scrollbarPolicy == VisibilityPolicy::Always);
            if (!needScrollbar && node->scrollbarPolicy == VisibilityPolicy::Auto && node->scrollOrientation != Orientation::Horizontal) {
                float testH = 0.0f;
                uint32_t active = 0;
                for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                    if (child->isSpacer) continue;
                    testH += ResolveNodeHeight(child, fullW);
                    ++active;
                }
                if (active > 1) {
                    testH += static_cast<float>(active - 1) * node->gap;
                }
                testH += node->padding.Vertical();
                if (testH > viewportH) {
                    needScrollbar = true;
                }
            }

            // 2. Reserve gutter so scrollbar never overlays content
            const float activeGutter = (needScrollbar && node->scrollOrientation != Orientation::Horizontal)
                ? sm.gutter
                : 0.0f;
            const float viewportW = std::max(0.0f, fullW - activeGutter);

            float contentH = 0.0f;
            uint32_t activeChildren = 0;
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                if (child->isSpacer) continue;
                contentH += ResolveNodeHeight(child, viewportW);
                ++activeChildren;
            }
            if (activeChildren > 1) {
                contentH += static_cast<float>(activeChildren - 1) * node->gap;
            }
            contentH += node->padding.Vertical();

            node->scrollContentSize = {viewportW, contentH};

            const float maxScrollY = std::max(0.0f, contentH - viewportH);
            node->scrollOffset.y = std::clamp(node->scrollOffset.y, 0.0f, maxScrollY);

            const Rect scrollContentRect(
                node->computedRect.x + node->padding.left - node->scrollOffset.x,
                node->computedRect.y + node->padding.top - node->scrollOffset.y,
                viewportW,
                contentH
            );

            if (node->direction == FlexDirection::Grid) {
                ArrangeGrid(node, scrollContentRect);
            } else if (node->direction == FlexDirection::Row) {
                ArrangeRow(node, scrollContentRect);
            } else {
                ArrangeColumn(node, scrollContentRect);
            }
            return;
        }

        if (node->type == NodeType::Carousel) {
            ArrangeCarousel(node, availableRect);
            return;
        }

        const Rect contentRect(
            node->computedRect.x + node->padding.left,
            node->computedRect.y + node->padding.top,
            std::max(0.0f, node->computedRect.w - node->padding.Horizontal()),
            std::max(0.0f, node->computedRect.h - node->padding.Vertical())
        );

        if (node->direction == FlexDirection::Row) {
            ArrangeRow(node, contentRect);
        } else if (node->direction == FlexDirection::Wrap) {
            ArrangeWrap(node, contentRect);
        } else if (node->direction == FlexDirection::Grid) {
            ArrangeGrid(node, contentRect);
        } else {
            ArrangeColumn(node, contentRect);
        }
    }

    static void ArrangeCarousel(Node* node, const Rect& availableRect) {
        float w = availableRect.w;
        float h = availableRect.h;

        if (node->parent == nullptr && !node->isSpacer) {
            if (node->widthMode == SizeMode::Fit) {
                w = node->intrinsicSize.x;
            } else if (node->widthMode == SizeMode::Fixed) {
                w = node->fixedWidth;
            }

            if (node->heightMode == SizeMode::Fit) {
                h = (node->childCount == 0) ? node->intrinsicSize.y : node->intrinsicSize.y;
            } else if (node->heightMode == SizeMode::Fixed) {
                h = node->fixedHeight;
            }
        } else {
            if (node->heightMode == SizeMode::Fixed) {
                h = node->fixedHeight;
            } else if (node->heightMode == SizeMode::Fit) {
                h = node->intrinsicSize.y;
            }
        }

        node->computedRect = Rect(availableRect.x, availableRect.y, w, h);
        if (node->childCount == 0) return;

        const float startX = node->computedRect.x + node->padding.left;
        const float startY = node->computedRect.y + node->padding.top;
        const float fullW = std::max(0.0f, node->computedRect.w - node->padding.Horizontal());
        const float fullH = std::max(0.0f, node->computedRect.h - node->padding.Vertical());

        // 1. Calculate total width of all children
        float totalChildrenW = 0.0f;
        uint32_t activeChildren = 0;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
            totalChildrenW += cw;
            ++activeChildren;
        }
        if (activeChildren > 1) {
            totalChildrenW += static_cast<float>(activeChildren - 1) * node->gap;
        }
        node->scrollContentSize = {totalChildrenW, fullH};

        // 2. Check if overflow occurs
        if (totalChildrenW <= fullW) {
            // Fits within available width without scrolling
            node->showLeftArrow = false;
            node->showRightArrow = false;
            node->leftArrowRect = Rect(0.0f, 0.0f, 0.0f, 0.0f);
            node->rightArrowRect = Rect(0.0f, 0.0f, 0.0f, 0.0f);
            node->carouselViewportRect = Rect(startX, startY, fullW, fullH);
            node->scrollOffset.x = 0.0f;

            // Arrange children in a standard row
            float curX = startX;
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                float childW = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                float childH = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight :
                               (child->heightMode == SizeMode::Stretch ? fullH : child->intrinsicSize.y);
                float childY = startY;
                if (node->align == Alignment::Center) {
                    childY += (fullH - childH) * 0.5f;
                } else if (node->align == Alignment::End) {
                    childY += (fullH - childH);
                }
                child->computedRect = Rect(curX, childY, childW, childH);
                ArrangeNode(child, child->computedRect);
                curX += childW + node->gap;
            }
            return;
        }

        // 3. Overflow detected: compute arrow dimensions
        const ComponentMetrics m = GetComponentMetrics(node->uiSize);
        const float arrowMargin = 4.0f;
        const float arrowBtnH = std::min(fullH - 6.0f, 26.0f);
        const float arrowBtnW = arrowBtnH; // Clean square pill button
        const float arrowGap = 6.0f;
        const float arrowY = startY + (fullH - arrowBtnH) * 0.5f;
        const float btnGutter = arrowMargin + arrowBtnW + arrowGap;

        // Dynamic gutters: only allocate gutter space when an arrow is active
        bool canScrollLeft = (node->scrollOffset.x > 1.0f);
        float leftGutter = canScrollLeft ? btnGutter : 0.0f;
        bool canScrollRight = (node->scrollOffset.x < (totalChildrenW - (fullW - leftGutter)) - 1.0f);
        float rightGutter = canScrollRight ? btnGutter : 0.0f;

        float viewportX = startX + leftGutter;
        float viewportW = std::max(0.0f, fullW - leftGutter - rightGutter);
        float maxScrollX = std::max(0.0f, totalChildrenW - viewportW);

        // Auto-scroll to active tab if requested
        if (node->scrollToActiveTab) {
            float relX = 0.0f;
            for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
                const float cw = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
                if (child->tabActive) {
                    if (relX < node->scrollOffset.x) {
                        node->scrollOffset.x = relX;
                    } else if (relX + cw > node->scrollOffset.x + viewportW) {
                        node->scrollOffset.x = (relX + cw) - viewportW;
                    }
                    break;
                }
                relX += cw + node->gap;
            }

            // Recompute gutters after auto-scroll updated scrollOffset
            canScrollLeft = (node->scrollOffset.x > 1.0f);
            leftGutter = canScrollLeft ? btnGutter : 0.0f;
            canScrollRight = (node->scrollOffset.x < (totalChildrenW - (fullW - leftGutter)) - 1.0f);
            rightGutter = canScrollRight ? btnGutter : 0.0f;

            viewportX = startX + leftGutter;
            viewportW = std::max(0.0f, fullW - leftGutter - rightGutter);
            maxScrollX = std::max(0.0f, totalChildrenW - viewportW);
        }

        // Clamp scroll offset
        node->scrollOffset.x = std::clamp(node->scrollOffset.x, 0.0f, maxScrollX);

        node->showLeftArrow = canScrollLeft;
        node->showRightArrow = canScrollRight;

        node->leftArrowRect = canScrollLeft
            ? Rect(startX + arrowMargin, arrowY, arrowBtnW, arrowBtnH)
            : Rect(0.0f, 0.0f, 0.0f, 0.0f);
        node->rightArrowRect = canScrollRight
            ? Rect(startX + fullW - arrowBtnW - arrowMargin, arrowY, arrowBtnW, arrowBtnH)
            : Rect(0.0f, 0.0f, 0.0f, 0.0f);
        node->carouselViewportRect = Rect(viewportX, startY, viewportW, fullH);

        // Arrange children within the scrollable viewport
        float curX = viewportX - node->scrollOffset.x;
        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            float childW = (child->widthMode == SizeMode::Fixed) ? child->fixedWidth : child->intrinsicSize.x;
            float childH = (child->heightMode == SizeMode::Fixed) ? child->fixedHeight :
                           (child->heightMode == SizeMode::Stretch ? fullH : child->intrinsicSize.y);
            float childY = startY;
            if (node->align == Alignment::Center) {
                childY += (fullH - childH) * 0.5f;
            } else if (node->align == Alignment::End) {
                childY += (fullH - childH);
            }
            child->computedRect = Rect(curX, childY, childW, childH);
            ArrangeNode(child, child->computedRect);
            curX += childW + node->gap;
        }
    }

    static void ArrangeRow(Node* node, const Rect& contentRect) {
        const float totalGap = (node->childCount > 1)
            ? static_cast<float>(node->childCount - 1) * node->gap
            : 0.0f;

        float baseSum = 0.0f;
        float totalFlexGrow = 0.0f;
        uint32_t flexCount = 0;

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->isSpacer) {
                totalFlexGrow += std::max(0.01f, child->flexGrow);
                ++flexCount;
            } else if (child->widthMode == SizeMode::Stretch) {
                baseSum += child->intrinsicSize.x;
                totalFlexGrow += std::max(0.01f, child->flexGrow);
                ++flexCount;
            } else if (child->widthMode == SizeMode::Fixed) {
                baseSum += child->fixedWidth;
            } else { // SizeMode::Fit
                baseSum += child->intrinsicSize.x;
            }
        }

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
            float childW = 0.0f;
            if (child->isSpacer) {
                childW = (totalFlexGrow > 0.0f)
                    ? (freeSpace * (std::max(0.01f, child->flexGrow) / totalFlexGrow))
                    : 0.0f;
            } else if (child->widthMode == SizeMode::Stretch) {
                const float flexW = (totalFlexGrow > 0.0f)
                    ? (freeSpace * (std::max(0.01f, child->flexGrow) / totalFlexGrow))
                    : 0.0f;
                childW = child->intrinsicSize.x + flexW;
            } else if (child->widthMode == SizeMode::Fixed) {
                childW = child->fixedWidth;
            } else { // SizeMode::Fit
                childW = child->intrinsicSize.x;
            }

            float childH = contentRect.h;
            if (child->heightMode == SizeMode::Fixed) {
                childH = child->fixedHeight;
            } else if (child->heightMode == SizeMode::Fit || node->align != Alignment::Stretch) {
                childH = child->intrinsicSize.y;
            }

            float childY = contentRect.y;
            if (node->align == Alignment::Center) {
                childY = contentRect.y + (contentRect.h - childH) * 0.5f;
            } else if (node->align == Alignment::End) {
                childY = contentRect.Bottom() - childH;
            }

            ArrangeNode(child, Rect(currentX, childY, childW, childH));
            currentX += childW + node->gap;
        }
    }

    static void ArrangeWrap(Node* node, const Rect& contentRect) {
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
                if (lineEnd->isSpacer) {
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
                if (child->isSpacer) continue;

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

                ArrangeNode(child, Rect(itemX, childY, cw, ch));
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

    static void ArrangeColumn(Node* node, const Rect& contentRect) {
        const float totalGap = (node->childCount > 1)
            ? static_cast<float>(node->childCount - 1) * node->gap
            : 0.0f;

        float baseSum = 0.0f;
        float totalFlexGrow = 0.0f;
        uint32_t flexCount = 0;

        for (Node* child = node->firstChild; child != nullptr; child = child->nextSibling) {
            if (child->isSpacer) {
                totalFlexGrow += std::max(0.01f, child->flexGrow);
                ++flexCount;
            } else if (child->heightMode == SizeMode::Stretch) {
                baseSum += child->intrinsicSize.y;
                totalFlexGrow += std::max(0.01f, child->flexGrow);
                ++flexCount;
            } else if (child->heightMode == SizeMode::Fixed) {
                baseSum += child->fixedHeight;
            } else { // SizeMode::Fit
                baseSum += ResolveNodeHeight(child, contentRect.w);
            }
        }

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
            float childH = 0.0f;
            if (child->isSpacer) {
                childH = (totalFlexGrow > 0.0f)
                    ? (freeSpace * (std::max(0.01f, child->flexGrow) / totalFlexGrow))
                    : 0.0f;
            } else if (child->heightMode == SizeMode::Stretch) {
                if (node->childCount == 1 && (node->fixedHeight > 0.0f || node->heightMode == SizeMode::Fixed)) {
                    childH = contentRect.h;
                } else {
                    const float flexH = (totalFlexGrow > 0.0f)
                        ? (freeSpace * (std::max(0.01f, child->flexGrow) / totalFlexGrow))
                        : 0.0f;
                    childH = child->intrinsicSize.y + flexH;
                }
            } else if (child->heightMode == SizeMode::Fixed) {
                childH = child->fixedHeight;
            } else { // SizeMode::Fit
                childH = ResolveNodeHeight(child, contentRect.w);
            }

            float childW = contentRect.w;
            if (child->widthMode == SizeMode::Fixed) {
                childW = child->fixedWidth;
            } else if (child->widthMode == SizeMode::Fit || node->align != Alignment::Stretch) {
                childW = child->intrinsicSize.x;
            }

            float childX = contentRect.x;
            if (node->align == Alignment::Center) {
                childX = contentRect.x + (contentRect.w - childW) * 0.5f;
            } else if (node->align == Alignment::End) {
                childX = contentRect.Right() - childW;
            }

            ArrangeNode(child, Rect(childX, currentY, childW, childH));
            currentY += child->computedRect.h + node->gap;
        }

        if (node->heightMode == SizeMode::Fit) {
            node->computedRect.h = (currentY > contentRect.y)
                ? (currentY - contentRect.y - node->gap) + node->padding.Vertical()
                : node->padding.Vertical();
        }
    }

    static void ArrangeGrid(Node* node, const Rect& contentRect) {
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
                if (lineEnd->isSpacer) {
                    lineEnd = lineEnd->nextSibling;
                    continue;
                }

                int span = std::clamp(lineEnd->colSpan, 1, totalCols);
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
                    childH = ResolveNodeHeight(lineEnd, childW);
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
                    : ResolveNodeHeight(lineStart, childW);
            }

            if (hasLines) {
                currentY += runGap;
            }

            // Pass 2: position items along the line
            float currentX = contentRect.x;
            for (Node* child = lineStart; child != lineEnd; child = child->nextSibling) {
                if (child->isSpacer) continue;

                int span = std::clamp(child->colSpan, 1, totalCols);
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
                    childH = ResolveNodeHeight(child, childW);
                }

                float childY = currentY;
                if (node->align == Alignment::Center) {
                    childY = currentY + (lineMaxH - childH) * 0.5f;
                } else if (node->align == Alignment::End) {
                    childY = currentY + (lineMaxH - childH);
                }

                ArrangeNode(child, Rect(currentX, childY, childW, childH));
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
};

} // namespace guikit
