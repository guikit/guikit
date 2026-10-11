#include <cmath>
#include <iostream>
#include "guikit/core/Allocator.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Solver.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/panels/Carousel.hpp"

static int g_failures = 0;

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            std::cerr << "[FAIL] " << #cond << " (" << __FILE__ << ":" << __LINE__   \
                      << ")\n";                                                      \
            ++g_failures;                                                            \
        }                                                                            \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                        \
    do {                                                                             \
        if (std::abs((a) - (b)) > (eps)) {                                           \
            std::cerr << "[FAIL] " << #a << " (" << (a) << ") != " << #b            \
                      << " (" << (b) << ") [" << __FILE__ << ":" << __LINE__        \
                      << "]\n";                                                      \
            ++g_failures;                                                            \
        }                                                                            \
    } while (0)

void TestRowEqualStretch() {
    using namespace guikit;

    BumpAllocator arena;

    // Row width 600, height 100, gap 10, 3 stretch children
    Node* row = arena.Allocate<Node>();
    row->direction = FlexDirection::Row;
    row->widthMode = SizeMode::Stretch;
    row->heightMode = SizeMode::Stretch;
    row->gap = 10.0f;

    Node* c1 = arena.Allocate<Node>();
    c1->widthMode = SizeMode::Stretch;
    c1->heightMode = SizeMode::Stretch;
    row->AddChild(c1);

    Node* c2 = arena.Allocate<Node>();
    c2->widthMode = SizeMode::Stretch;
    c2->heightMode = SizeMode::Stretch;
    row->AddChild(c2);

    Node* c3 = arena.Allocate<Node>();
    c3->widthMode = SizeMode::Stretch;
    c3->heightMode = SizeMode::Stretch;
    row->AddChild(c3);

    LayoutSolver::Measure(row);
    LayoutSolver::Arrange(row, Rect(0.0f, 0.0f, 620.0f, 100.0f));

    // Available width = 620, 2 gaps of 10 = 20, remaining = 600 / 3 = 200 each
    CHECK_NEAR(c1->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.w, 200.0f, 0.001f);

    CHECK_NEAR(c2->computedRect.x, 210.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.w, 200.0f, 0.001f);

    CHECK_NEAR(c3->computedRect.x, 420.0f, 0.001f);
    CHECK_NEAR(c3->computedRect.w, 200.0f, 0.001f);
}

void TestRowWithSpacer() {
    using namespace guikit;

    BumpAllocator arena;

    // Row width 800, Left item (fixed 100), Spacer, Right item (fixed 100)
    Node* row = arena.Allocate<Node>();
    row->direction = FlexDirection::Row;
    row->widthMode = SizeMode::Stretch;
    row->heightMode = SizeMode::Stretch;
    row->gap = 0.0f;

    Node* left = arena.Allocate<Node>();
    left->widthMode = SizeMode::Fixed;
    left->fixedWidth = 100.0f;
    row->AddChild(left);

    Node* spacer = arena.Allocate<Node>();
    spacer->isSpacer = true;
    spacer->widthMode = SizeMode::Stretch;
    row->AddChild(spacer);

    Node* right = arena.Allocate<Node>();
    right->widthMode = SizeMode::Fixed;
    right->fixedWidth = 100.0f;
    row->AddChild(right);

    LayoutSolver::Measure(row);
    LayoutSolver::Arrange(row, Rect(0.0f, 0.0f, 800.0f, 50.0f));

    CHECK_NEAR(left->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(left->computedRect.w, 100.0f, 0.001f);

    // Spacer should expand to 600 (800 - 100 - 100)
    CHECK_NEAR(spacer->computedRect.x, 100.0f, 0.001f);
    CHECK_NEAR(spacer->computedRect.w, 600.0f, 0.001f);

    // Right item should be pushed to x=700
    CHECK_NEAR(right->computedRect.x, 700.0f, 0.001f);
    CHECK_NEAR(right->computedRect.w, 100.0f, 0.001f);
    CHECK_NEAR(right->computedRect.Right(), 800.0f, 0.001f);
}

void TestColumnStickyFooter() {
    using namespace guikit;

    BumpAllocator arena;

    // Vertical column height 600: Header (fixed 50), Spacer, Footer (fixed 40)
    Node* col = arena.Allocate<Node>();
    col->direction = FlexDirection::Column;
    col->widthMode = SizeMode::Stretch;
    col->heightMode = SizeMode::Stretch;
    col->gap = 0.0f;

    Node* header = arena.Allocate<Node>();
    header->heightMode = SizeMode::Fixed;
    header->fixedHeight = 50.0f;
    col->AddChild(header);

    Node* spacer = arena.Allocate<Node>();
    spacer->isSpacer = true;
    spacer->heightMode = SizeMode::Stretch;
    col->AddChild(spacer);

    Node* footer = arena.Allocate<Node>();
    footer->heightMode = SizeMode::Fixed;
    footer->fixedHeight = 40.0f;
    col->AddChild(footer);

    LayoutSolver::Measure(col);
    LayoutSolver::Arrange(col, Rect(0.0f, 0.0f, 400.0f, 600.0f));

    CHECK_NEAR(header->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(header->computedRect.h, 50.0f, 0.001f);

    // Spacer height = 600 - 50 - 40 = 510
    CHECK_NEAR(spacer->computedRect.y, 50.0f, 0.001f);
    CHECK_NEAR(spacer->computedRect.h, 510.0f, 0.001f);

    // Footer should be at y = 560 with bottom at 600
    CHECK_NEAR(footer->computedRect.y, 560.0f, 0.001f);
    CHECK_NEAR(footer->computedRect.h, 40.0f, 0.001f);
    CHECK_NEAR(footer->computedRect.Bottom(), 600.0f, 0.001f);
}

void TestNestedHierarchy() {
    using namespace guikit;

    BumpAllocator arena;

    // Outer Column: 800 x 600
    Node* root = arena.Allocate<Node>();
    root->direction = FlexDirection::Column;
    root->widthMode = SizeMode::Stretch;
    root->heightMode = SizeMode::Stretch;
    root->gap = 10.0f;

    // Row 1: height 50, two 50/50 buttons
    Node* row1 = arena.Allocate<Node>();
    row1->direction = FlexDirection::Row;
    row1->widthMode = SizeMode::Stretch;
    row1->heightMode = SizeMode::Fixed;
    row1->fixedHeight = 50.0f;
    row1->gap = 10.0f;
    root->AddChild(row1);

    Node* btnA = arena.Allocate<Node>();
    btnA->widthMode = SizeMode::Stretch;
    row1->AddChild(btnA);

    Node* btnB = arena.Allocate<Node>();
    btnB->widthMode = SizeMode::Stretch;
    row1->AddChild(btnB);

    // Vertical Spacer
    Node* vSpacer = arena.Allocate<Node>();
    vSpacer->isSpacer = true;
    vSpacer->heightMode = SizeMode::Stretch;
    root->AddChild(vSpacer);

    // Row 2 (Footer): height 40, one centered button
    Node* footer = arena.Allocate<Node>();
    footer->direction = FlexDirection::Row;
    footer->widthMode = SizeMode::Stretch;
    footer->heightMode = SizeMode::Fixed;
    footer->fixedHeight = 40.0f;
    root->AddChild(footer);

    Node* footerBtn = arena.Allocate<Node>();
    footerBtn->widthMode = SizeMode::Stretch;
    footer->AddChild(footerBtn);

    LayoutSolver::Measure(root);
    LayoutSolver::Arrange(root, Rect(0.0f, 0.0f, 810.0f, 600.0f));

    // Row 1: width 810, gap 10 -> each button gets 400
    CHECK_NEAR(row1->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(row1->computedRect.h, 50.0f, 0.001f);
    CHECK_NEAR(btnA->computedRect.w, 400.0f, 0.001f);
    CHECK_NEAR(btnB->computedRect.w, 400.0f, 0.001f);
    CHECK_NEAR(btnB->computedRect.x, 410.0f, 0.001f);

    // Footer at bottom: y = 600 - 40 = 560
    CHECK_NEAR(footer->computedRect.Bottom(), 600.0f, 0.001f);
    CHECK_NEAR(footer->computedRect.h, 40.0f, 0.001f);
    CHECK_NEAR(footerBtn->computedRect.w, 810.0f, 0.001f);
}

void TestColumnDeficitNoSquishAndCrop() {
    using namespace guikit;

    BumpAllocator arena;

    // Outer Column: Header (50), Card (Fit with 90px intrinsic), Spacer, Footer (40)
    // Gap = 10. Total minimum required height = 50 + 10 + 90 + 10 + 0 + 10 + 40 = 210.
    Node* root = arena.Allocate<Node>();
    root->direction = FlexDirection::Column;
    root->widthMode = SizeMode::Stretch;
    root->heightMode = SizeMode::Stretch;
    root->gap = 10.0f;

    Node* header = arena.Allocate<Node>();
    header->heightMode = SizeMode::Fixed;
    header->fixedHeight = 50.0f;
    root->AddChild(header);

    Node* card = arena.Allocate<Node>();
    card->direction = FlexDirection::Column;
    card->widthMode = SizeMode::Stretch;
    card->heightMode = SizeMode::Fit;
    card->gap = 10.0f;

    Node* item1 = arena.Allocate<Node>();
    item1->heightMode = SizeMode::Fixed;
    item1->fixedHeight = 40.0f;
    card->AddChild(item1);

    Node* item2 = arena.Allocate<Node>();
    item2->heightMode = SizeMode::Fixed;
    item2->fixedHeight = 40.0f;
    card->AddChild(item2);

    root->AddChild(card);

    Node* spacer = arena.Allocate<Node>();
    spacer->isSpacer = true;
    spacer->heightMode = SizeMode::Stretch;
    root->AddChild(spacer);

    Node* footer = arena.Allocate<Node>();
    footer->heightMode = SizeMode::Fixed;
    footer->fixedHeight = 40.0f;
    root->AddChild(footer);

    LayoutSolver::Measure(root);

    // 1. With ample height (400px): spacer absorbs remaining space, footer sticks to bottom
    LayoutSolver::Arrange(root, Rect(0.0f, 0.0f, 300.0f, 400.0f));
    CHECK_NEAR(header->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(header->computedRect.h, 50.0f, 0.001f);
    CHECK_NEAR(card->computedRect.y, 60.0f, 0.001f);
    CHECK_NEAR(card->computedRect.h, 90.0f, 0.001f);
    // freeSpace = 400 - 210 = 190
    CHECK_NEAR(spacer->computedRect.h, 190.0f, 0.001f);
    CHECK_NEAR(footer->computedRect.Bottom(), 400.0f, 0.001f);

    // 2. In severe height deficit (100px < 210px):
    // Spacer shrinks to 0; no non-spacer widgets squish below intrinsic size; zero overlap.
    LayoutSolver::Arrange(root, Rect(0.0f, 0.0f, 300.0f, 100.0f));
    CHECK_NEAR(header->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(header->computedRect.h, 50.0f, 0.001f);
    CHECK_NEAR(card->computedRect.y, 60.0f, 0.001f);
    CHECK_NEAR(card->computedRect.h, 90.0f, 0.001f); // Card does NOT squash!
    CHECK_NEAR(spacer->computedRect.h, 0.0f, 0.001f);  // Spacer shrinks to 0
    CHECK_NEAR(footer->computedRect.y, 170.0f, 0.001f); // Placed strictly after card and gaps (60 + 90 + 10 + 0 + 10)
    CHECK_NEAR(footer->computedRect.h, 40.0f, 0.001f);
    CHECK(footer->computedRect.y >= card->computedRect.Bottom()); // Zero overlap!
}

void TestWrapContainer() {
    using namespace guikit;

    BumpAllocator arena;

    // Wrap container: gap = 10, runGap = 12, widthMode = Stretch, heightMode = Fit
    Node* wrap = arena.Allocate<Node>();
    wrap->direction = FlexDirection::Wrap;
    wrap->widthMode = SizeMode::Stretch;
    wrap->heightMode = SizeMode::Fit;
    wrap->gap = 10.0f;
    wrap->runGap = 12.0f;

    // 4 items: each fixedWidth = 80, fixedHeight = 30
    Node* items[4];
    for (int i = 0; i < 4; ++i) {
        items[i] = arena.Allocate<Node>();
        items[i]->widthMode = SizeMode::Fixed;
        items[i]->heightMode = SizeMode::Fixed;
        items[i]->fixedWidth = 80.0f;
        items[i]->fixedHeight = 30.0f;
        wrap->AddChild(items[i]);
    }

    LayoutSolver::Measure(wrap);

    // Intrinsic unconstrained width: 4 * 80 + 3 * 10 = 350, height = 30
    CHECK_NEAR(wrap->intrinsicSize.x, 350.0f, 0.001f);
    CHECK_NEAR(wrap->intrinsicSize.y, 30.0f, 0.001f);

    // Case 1: Wide viewport (400px >= 350px): all 4 items fit on 1 line
    LayoutSolver::Arrange(wrap, Rect(0.0f, 0.0f, 400.0f, 100.0f));
    CHECK_NEAR(wrap->computedRect.h, 30.0f, 0.001f);
    CHECK_NEAR(items[0]->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(items[0]->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(items[1]->computedRect.x, 90.0f, 0.001f);
    CHECK_NEAR(items[1]->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(items[2]->computedRect.x, 180.0f, 0.001f);
    CHECK_NEAR(items[2]->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(items[3]->computedRect.x, 270.0f, 0.001f);
    CHECK_NEAR(items[3]->computedRect.y, 0.0f, 0.001f);

    // Case 2: Constrained viewport (200px): 2 items per line (80 + 10 + 80 = 170 <= 200)
    // Line 1: items[0], items[1] at y = 0
    // Line 2: items[2], items[3] at y = 30 + 12 = 42
    // Total wrap container height = 30 + 12 + 30 = 72
    LayoutSolver::Arrange(wrap, Rect(0.0f, 0.0f, 200.0f, 100.0f));
    CHECK_NEAR(wrap->computedRect.h, 72.0f, 0.001f);
    CHECK_NEAR(items[0]->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(items[0]->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(items[1]->computedRect.x, 90.0f, 0.001f);
    CHECK_NEAR(items[1]->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(items[2]->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(items[2]->computedRect.y, 42.0f, 0.001f);
    CHECK_NEAR(items[3]->computedRect.x, 90.0f, 0.001f);
    CHECK_NEAR(items[3]->computedRect.y, 42.0f, 0.001f);

    // Case 3: Narrow viewport (100px): 1 item per line (80 <= 100, 80 + 10 + 80 = 170 > 100)
    // 4 lines! Height = 4 * 30 + 3 * 12 = 120 + 36 = 156
    LayoutSolver::Arrange(wrap, Rect(0.0f, 0.0f, 100.0f, 200.0f));
    CHECK_NEAR(wrap->computedRect.h, 156.0f, 0.001f);
    for (int i = 0; i < 4; ++i) {
        CHECK_NEAR(items[i]->computedRect.x, 0.0f, 0.001f);
        CHECK_NEAR(items[i]->computedRect.y, static_cast<float>(i * (30 + 12)), 0.001f);
    }
}

void TestWrapInColumnWithSpacer() {
    using namespace guikit;

    BumpAllocator arena;

    // Outer column (width 200, height 300, gap 10)
    // Contains: Wrap (2 items per line = 72px), Spacer, Footer (30px)
    Node* col = arena.Allocate<Node>();
    col->direction = FlexDirection::Column;
    col->widthMode = SizeMode::Stretch;
    col->heightMode = SizeMode::Stretch;
    col->gap = 10.0f;

    Node* wrap = arena.Allocate<Node>();
    wrap->direction = FlexDirection::Wrap;
    wrap->widthMode = SizeMode::Stretch;
    wrap->heightMode = SizeMode::Fit;
    wrap->gap = 10.0f;
    wrap->runGap = 12.0f;

    for (int i = 0; i < 4; ++i) {
        Node* item = arena.Allocate<Node>();
        item->widthMode = SizeMode::Fixed;
        item->heightMode = SizeMode::Fixed;
        item->fixedWidth = 80.0f;
        item->fixedHeight = 30.0f;
        wrap->AddChild(item);
    }
    col->AddChild(wrap);

    Node* spacer = arena.Allocate<Node>();
    spacer->isSpacer = true;
    spacer->heightMode = SizeMode::Stretch;
    col->AddChild(spacer);

    Node* footer = arena.Allocate<Node>();
    footer->heightMode = SizeMode::Fixed;
    footer->fixedHeight = 30.0f;
    col->AddChild(footer);

    LayoutSolver::Measure(col);
    // At width 200, Wrap wraps into 2 lines (height 72)
    // Total column height = 300
    // non-spacer baseSum = 72 + 30 = 102
    // gaps = 2 * 10 = 20
    // spacer freeSpace = 300 - 20 - 102 = 178
    // footer y = 72 + 10 + 178 + 10 = 270 (Bottom() == 300)
    LayoutSolver::Arrange(col, Rect(0.0f, 0.0f, 200.0f, 300.0f));

    CHECK_NEAR(wrap->computedRect.h, 72.0f, 0.001f);
    CHECK_NEAR(spacer->computedRect.h, 178.0f, 0.001f);
    CHECK_NEAR(footer->computedRect.y, 270.0f, 0.001f);
    CHECK_NEAR(footer->computedRect.Bottom(), 300.0f, 0.001f);
}

void TestGridLayoutSolver() {
    using namespace guikit;

    BumpAllocator arena;

    // Grid container with width 600, gap 12, runGap 16
    Node* grid = arena.Allocate<Node>();
    grid->direction = FlexDirection::Grid;
    grid->gridColumns = 12;
    grid->gap = 12.0f;
    grid->runGap = 16.0f;
    grid->widthMode = SizeMode::Fixed;
    grid->fixedWidth = 600.0f;
    grid->heightMode = SizeMode::Fit;

    // Row 1: Two cells with colSpan = 6
    Node* c1 = arena.Allocate<Node>();
    c1->colSpan = 6;
    c1->heightMode = SizeMode::Fixed;
    c1->fixedHeight = 40.0f;
    grid->AddChild(c1);

    Node* c2 = arena.Allocate<Node>();
    c2->colSpan = 6;
    c2->heightMode = SizeMode::Fixed;
    c2->fixedHeight = 40.0f;
    grid->AddChild(c2);

    // Row 2: Three cells with colSpan = 4
    Node* c3 = arena.Allocate<Node>();
    c3->colSpan = 4;
    c3->heightMode = SizeMode::Fixed;
    c3->fixedHeight = 50.0f;
    grid->AddChild(c3);

    Node* c4 = arena.Allocate<Node>();
    c4->colSpan = 4;
    c4->heightMode = SizeMode::Fixed;
    c4->fixedHeight = 50.0f;
    grid->AddChild(c4);

    Node* c5 = arena.Allocate<Node>();
    c5->colSpan = 4;
    c5->heightMode = SizeMode::Fixed;
    c5->fixedHeight = 50.0f;
    grid->AddChild(c5);

    // Row 3: One cell with colSpan = 12
    Node* c6 = arena.Allocate<Node>();
    c6->colSpan = 12;
    c6->heightMode = SizeMode::Fixed;
    c6->fixedHeight = 60.0f;
    grid->AddChild(c6);

    LayoutSolver::Measure(grid);
    LayoutSolver::Arrange(grid, Rect(0.0f, 0.0f, 600.0f, 0.0f));

    // Row 1 checks: W(6) = (6/12)*(600+12) - 12 = 306 - 12 = 294
    CHECK_NEAR(c1->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.w, 294.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.h, 40.0f, 0.001f);

    CHECK_NEAR(c2->computedRect.x, 306.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.w, 294.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.h, 40.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.Right(), 600.0f, 0.001f);

    // Row 2 checks: W(4) = (4/12)*(600+12) - 12 = 204 - 12 = 192
    // y = 40 + runGap(16) = 56
    CHECK_NEAR(c3->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(c3->computedRect.y, 56.0f, 0.001f);
    CHECK_NEAR(c3->computedRect.w, 192.0f, 0.001f);
    CHECK_NEAR(c3->computedRect.h, 50.0f, 0.001f);

    CHECK_NEAR(c4->computedRect.x, 204.0f, 0.001f);
    CHECK_NEAR(c4->computedRect.y, 56.0f, 0.001f);
    CHECK_NEAR(c4->computedRect.w, 192.0f, 0.001f);

    CHECK_NEAR(c5->computedRect.x, 408.0f, 0.001f);
    CHECK_NEAR(c5->computedRect.y, 56.0f, 0.001f);
    CHECK_NEAR(c5->computedRect.w, 192.0f, 0.001f);
    CHECK_NEAR(c5->computedRect.Right(), 600.0f, 0.001f);

    // Row 3 checks: W(12) = 600
    // y = 56 + 50 + runGap(16) = 122
    CHECK_NEAR(c6->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(c6->computedRect.y, 122.0f, 0.001f);
    CHECK_NEAR(c6->computedRect.w, 600.0f, 0.001f);
    CHECK_NEAR(c6->computedRect.h, 60.0f, 0.001f);

    // Total grid height in Fit mode: 122 + 60 = 182
    CHECK_NEAR(grid->computedRect.h, 182.0f, 0.001f);
}

void TestGridResponsiveWrapAndExpand() {
    using namespace guikit;

    BumpAllocator arena;

    // Grid container with width 400, gap 10, runGap 15
    Node* grid = arena.Allocate<Node>();
    grid->direction = FlexDirection::Grid;
    grid->gridColumns = 12;
    grid->gap = 10.0f;
    grid->runGap = 15.0f;
    grid->widthMode = SizeMode::Fixed;
    grid->fixedWidth = 400.0f;
    grid->heightMode = SizeMode::Fit;

    // Two cells with span 6 and minWidth 250
    Node* c1 = arena.Allocate<Node>();
    c1->colSpan = 6;
    c1->minWidth = 250.0f;
    c1->heightMode = SizeMode::Fixed;
    c1->fixedHeight = 40.0f;
    grid->AddChild(c1);

    Node* c2 = arena.Allocate<Node>();
    c2->colSpan = 6;
    c2->minWidth = 250.0f;
    c2->heightMode = SizeMode::Fixed;
    c2->fixedHeight = 40.0f;
    grid->AddChild(c2);

    // Case 1: Constrained space (width 400 < 2 * 250) -> wrap and expand to 100% width
    LayoutSolver::Measure(grid);
    LayoutSolver::Arrange(grid, Rect(0.0f, 0.0f, 400.0f, 0.0f));

    CHECK_NEAR(c1->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.w, 400.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.h, 40.0f, 0.001f);

    CHECK_NEAR(c2->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.y, 55.0f, 0.001f); // 40 + 15
    CHECK_NEAR(c2->computedRect.w, 400.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.h, 40.0f, 0.001f);
    CHECK_NEAR(grid->computedRect.h, 95.0f, 0.001f); // 55 + 40

    // Case 2: Sufficient space (width 600 >= 2 * 250) -> fit side by side
    grid->fixedWidth = 600.0f;
    LayoutSolver::Measure(grid);
    LayoutSolver::Arrange(grid, Rect(0.0f, 0.0f, 600.0f, 0.0f));

    // W(6) = (6/12)*(600+10) - 10 = 305 - 10 = 295
    CHECK_NEAR(c1->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.w, 295.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.h, 40.0f, 0.001f);

    CHECK_NEAR(c2->computedRect.x, 305.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.w, 295.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.h, 40.0f, 0.001f);
    CHECK_NEAR(grid->computedRect.h, 40.0f, 0.001f);
}

void TestScrollAreaLayoutSolver() {
    using namespace guikit;

    BumpAllocator arena;

    // Viewport container 400x300
    Node* scroll = arena.Allocate<Node>();
    scroll->type = NodeType::ScrollArea;
    scroll->direction = FlexDirection::Column;
    scroll->clipChildren = true;
    scroll->widthMode = SizeMode::Stretch;
    scroll->heightMode = SizeMode::Stretch;
    scroll->scrollOffset = {0.0f, 50.0f};

    // 3 children, 150px height each -> total content height = 450px
    Node* c1 = arena.Allocate<Node>();
    c1->heightMode = SizeMode::Fixed;
    c1->fixedHeight = 150.0f;
    scroll->AddChild(c1);

    Node* c2 = arena.Allocate<Node>();
    c2->heightMode = SizeMode::Fixed;
    c2->fixedHeight = 150.0f;
    scroll->AddChild(c2);

    Node* c3 = arena.Allocate<Node>();
    c3->heightMode = SizeMode::Fixed;
    c3->fixedHeight = 150.0f;
    scroll->AddChild(c3);

    LayoutSolver::Measure(scroll);
    LayoutSolver::Arrange(scroll, Rect(0.0f, 0.0f, 400.0f, 300.0f));

    // ScrollArea viewport dimensions remain strictly bounded
    CHECK_NEAR(scroll->computedRect.w, 400.0f, 0.001f);
    CHECK_NEAR(scroll->computedRect.h, 300.0f, 0.001f);
    CHECK_NEAR(scroll->scrollContentSize.y, 450.0f, 0.001f);

    // Children are offset by -50px
    CHECK_NEAR(c1->computedRect.y, -50.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.y, 100.0f, 0.001f);
    CHECK_NEAR(c3->computedRect.y, 250.0f, 0.001f);

    // Verify that content width strictly reserves scrollbar gutter (so scrollbar never overlays children)
    const ScrollbarMetrics& sm = UiTheme::Dark().GetScrollbar(scroll->uiSize);
    CHECK_NEAR(scroll->scrollContentSize.x, 400.0f - sm.gutter, 0.001f);
    CHECK_NEAR(c1->computedRect.w, 400.0f - sm.gutter, 0.001f);

    // Verify over-scroll clamping (offset 500 clamped to max 450 - 300 = 150)
    scroll->scrollOffset = {0.0f, 500.0f};
    LayoutSolver::Arrange(scroll, Rect(0.0f, 0.0f, 400.0f, 300.0f));

    CHECK_NEAR(scroll->scrollOffset.y, 150.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.y, -150.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(c3->computedRect.y, 150.0f, 0.001f);

    // Verify that when scrollbar policy is Never, no gutter is reserved and children occupy full 400px
    scroll->scrollbarPolicy = VisibilityPolicy::Never;
    LayoutSolver::Arrange(scroll, Rect(0.0f, 0.0f, 400.0f, 300.0f));
    CHECK_NEAR(scroll->scrollContentSize.x, 400.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.w, 400.0f, 0.001f);
}

void TestCustomMeasureAndArrangeDelegates() {
    using namespace guikit;

    BumpAllocator arena;

    // Custom widget with measureFn
    Node* customLeaf = arena.Allocate<Node>();
    customLeaf->measureFn = [](const Node*, ICanvas*, const UiTheme&) -> Vec2 {
        return {123.0f, 45.0f};
    };

    LayoutSolver::Measure(customLeaf);
    CHECK_NEAR(customLeaf->intrinsicSize.x, 123.0f, 0.001f);
    CHECK_NEAR(customLeaf->intrinsicSize.y, 45.0f, 0.001f);

    // Custom container with arrangeFn
    Node* customContainer = arena.Allocate<Node>();
    Node* child = arena.Allocate<Node>();
    customContainer->AddChild(child);

    customContainer->arrangeFn = [](Node* node, const Rect& availableRect, const UiTheme& /*theme*/) {
        node->computedRect = availableRect;
        if (node->firstChild) {
            node->firstChild->computedRect = Rect(availableRect.x + 10.0f, availableRect.y + 10.0f, 50.0f, 50.0f);
        }
    };

    LayoutSolver::Arrange(customContainer, Rect(100.0f, 100.0f, 300.0f, 200.0f));
    CHECK_NEAR(customContainer->computedRect.x, 100.0f, 0.001f);
    CHECK_NEAR(customContainer->computedRect.w, 300.0f, 0.001f);
    CHECK_NEAR(child->computedRect.x, 110.0f, 0.001f);
    CHECK_NEAR(child->computedRect.y, 110.0f, 0.001f);
    CHECK_NEAR(child->computedRect.w, 50.0f, 0.001f);
}

void TestStretchColumnInConstrainedRowWithHugeContent() {
    using namespace guikit;

    BumpAllocator arena;

    // Row: 1000px wide, 600px high, gap 20px
    Node* row = arena.Allocate<Node>();
    row->direction = FlexDirection::Row;
    row->widthMode = SizeMode::Stretch;
    row->heightMode = SizeMode::Stretch;
    row->gap = 20.0f;

    // Sidebar: Fixed 300px
    Node* sidebar = arena.Allocate<Node>();
    sidebar->widthMode = SizeMode::Fixed;
    sidebar->fixedWidth = 300.0f;
    sidebar->heightMode = SizeMode::Stretch;
    row->AddChild(sidebar);

    // Content Column: Stretch width (flexGrow = 1.0f)
    Node* contentCol = arena.Allocate<Node>();
    contentCol->direction = FlexDirection::Column;
    contentCol->widthMode = SizeMode::Stretch;
    contentCol->heightMode = SizeMode::Stretch;
    contentCol->gap = 10.0f;
    row->AddChild(contentCol);

    // Inside content column: a Carousel with Stretch width and multiple tabs totaling 1500px width
    Node* carousel = arena.Allocate<Node>();
    carousel->type = NodeType::Carousel;
    carousel->direction = FlexDirection::Row;
    carousel->widthMode = SizeMode::Stretch;
    carousel->heightMode = SizeMode::Fixed;
    carousel->fixedHeight = 40.0f;
    carousel->gap = 10.0f;
    carousel->measureFn = &Carousel::Measure;
    carousel->arrangeFn = &Carousel::Arrange;

    auto* cData = arena.Allocate<CarouselData>();
    carousel->payload = cData;

    // Add 10 tabs, each 150px wide = 1500px + 90px gap = 1590px total
    for (int i = 0; i < 10; ++i) {
        Node* tab = arena.Allocate<Node>();
        tab->widthMode = SizeMode::Fixed;
        tab->fixedWidth = 150.0f;
        tab->heightMode = SizeMode::Stretch;
        carousel->AddChild(tab);
    }
    contentCol->AddChild(carousel);

    // Also a Grid with two Cell6 items (span 6 each), each containing a card with 380px intrinsic width
    Node* grid = arena.Allocate<Node>();
    grid->direction = FlexDirection::Grid;
    grid->gridColumns = 12;
    grid->widthMode = SizeMode::Stretch;
    grid->heightMode = SizeMode::Fit;
    grid->gap = 16.0f;

    Node* cell1 = arena.Allocate<Node>();
    cell1->direction = FlexDirection::Column;
    cell1->colSpan = 6;
    cell1->minWidth = 380.0f;
    cell1->heightMode = SizeMode::Fixed;
    cell1->fixedHeight = 200.0f;
    grid->AddChild(cell1);

    Node* cell2 = arena.Allocate<Node>();
    cell2->direction = FlexDirection::Column;
    cell2->colSpan = 6;
    cell2->minWidth = 380.0f;
    cell2->heightMode = SizeMode::Fixed;
    cell2->fixedHeight = 200.0f;
    grid->AddChild(cell2);

    contentCol->AddChild(grid);

    LayoutSolver::Measure(row);
    LayoutSolver::Arrange(row, Rect(0.0f, 0.0f, 1000.0f, 600.0f));

    // 1. Sidebar takes exactly 300px
    CHECK_NEAR(sidebar->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(sidebar->computedRect.w, 300.0f, 0.001f);

    // 2. Content Column takes exactly remaining space: 1000 - 300 - 20 = 680px
    // It must NEVER expand to 1590px!
    CHECK_NEAR(contentCol->computedRect.x, 320.0f, 0.001f);
    CHECK_NEAR(contentCol->computedRect.w, 680.0f, 0.001f);
    CHECK_NEAR(contentCol->computedRect.Right(), 1000.0f, 0.001f);

    // 3. Carousel is constrained to contentCol's width (680px)
    CHECK_NEAR(carousel->computedRect.w, 680.0f, 0.001f);
    CHECK(carousel->scrollContentSize.x > 1500.0f);

    // Since content (1590px) > available (680px), right arrow must be visible!
    CHECK(cData->showRightArrow == true);

    // 4. Responsive Grid auto-wraps:
    // With availW = 680px, testW for span 6 is (6/12)*(680+16) - 16 = 332px < reqMinW (380px).
    // So cell1 wraps to full width (span = 12, w = 680px) on line 1, and cell2 wraps to full width on line 2!
    CHECK_NEAR(cell1->computedRect.x, 320.0f, 0.001f);
    CHECK_NEAR(cell1->computedRect.w, 680.0f, 0.001f);
    CHECK_NEAR(cell1->computedRect.y, carousel->computedRect.Bottom() + 10.0f, 0.001f);

    CHECK_NEAR(cell2->computedRect.x, 320.0f, 0.001f);
    CHECK_NEAR(cell2->computedRect.w, 680.0f, 0.001f);
    CHECK_NEAR(cell2->computedRect.y, cell1->computedRect.Bottom() + 16.0f, 0.001f);
}

int main() {
    std::cout << "[RUN] Testing GuiKit Layout Solver...\n";

    TestRowEqualStretch();
    TestRowWithSpacer();
    TestColumnStickyFooter();
    TestNestedHierarchy();
    TestColumnDeficitNoSquishAndCrop();
    TestWrapContainer();
    TestWrapInColumnWithSpacer();
    TestGridLayoutSolver();
    TestGridResponsiveWrapAndExpand();
    TestScrollAreaLayoutSolver();
    TestCustomMeasureAndArrangeDelegates();
    TestStretchColumnInConstrainedRowWithHugeContent();

    if (g_failures == 0) {
        std::cout << "[PASS] All GuiKit Layout tests passed successfully!\n";
        return 0;
    } else {
        std::cerr << "[FAIL] Total failures: " << g_failures << "\n";
        return 1;
    }
}
