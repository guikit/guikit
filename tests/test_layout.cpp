#include "guikit/core.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/solver.hpp"
#include "guikit/style/theme.hpp"
#include "guikit/surfaces/carousel.hpp"
#include "guikit/layout/scroll_area.hpp"
#include "keel/keel.h"
#include "keel/math.h"

static keel::i32 g_failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            keel::eprintln("[FAIL] {} ({}:{})", #cond, __FILE__, __LINE__); \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                                            \
    do {                                                                                                 \
        if (keel::abs((a) - (b)) > (eps)) {                                                               \
            keel::eprintln("[FAIL] {} ({}) != {} ({}) [{}:{}]", #a, (a), #b, (b), __FILE__, __LINE__);   \
            ++g_failures;                                                                                \
        }                                                                                                \
    } while (0)

void TestRowEqualStretch() {
    using namespace guikit;

    keel::Arena arena;

    // Row width 600, height 100, gap 10, 3 stretch children
    Node* row = arena.create<Node>();
    row->direction = FlexDirection::Row;
    row->widthMode = SizeMode::Stretch;
    row->heightMode = SizeMode::Stretch;
    row->gap = 10.0f;

    Node* c1 = arena.create<Node>();
    c1->widthMode = SizeMode::Stretch;
    c1->heightMode = SizeMode::Stretch;
    row->AddChild(c1);

    Node* c2 = arena.create<Node>();
    c2->widthMode = SizeMode::Stretch;
    c2->heightMode = SizeMode::Stretch;
    row->AddChild(c2);

    Node* c3 = arena.create<Node>();
    c3->widthMode = SizeMode::Stretch;
    c3->heightMode = SizeMode::Stretch;
    row->AddChild(c3);

    Solver::Measure(row);
    Solver::Arrange(row, Rect(0.0f, 0.0f, 620.0f, 100.0f));

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

    keel::Arena arena;

    // Row width 800, Left item (fixed 100), Spacer, Right item (fixed 100)
    Node* row = arena.create<Node>();
    row->direction = FlexDirection::Row;
    row->widthMode = SizeMode::Stretch;
    row->heightMode = SizeMode::Stretch;
    row->gap = 0.0f;

    Node* left = arena.create<Node>();
    left->widthMode = SizeMode::Fixed;
    left->fixedWidth = 100.0f;
    row->AddChild(left);

    Node* spacer = arena.create<Node>();
    spacer->isSpacer = true;
    spacer->widthMode = SizeMode::Stretch;
    row->AddChild(spacer);

    Node* right = arena.create<Node>();
    right->widthMode = SizeMode::Fixed;
    right->fixedWidth = 100.0f;
    row->AddChild(right);

    Solver::Measure(row);
    Solver::Arrange(row, Rect(0.0f, 0.0f, 800.0f, 50.0f));

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

    keel::Arena arena;

    // Vertical column height 600: Header (fixed 50), Spacer, Footer (fixed 40)
    Node* col = arena.create<Node>();
    col->direction = FlexDirection::Column;
    col->widthMode = SizeMode::Stretch;
    col->heightMode = SizeMode::Stretch;
    col->gap = 0.0f;

    Node* header = arena.create<Node>();
    header->heightMode = SizeMode::Fixed;
    header->fixedHeight = 50.0f;
    col->AddChild(header);

    Node* spacer = arena.create<Node>();
    spacer->isSpacer = true;
    spacer->heightMode = SizeMode::Stretch;
    col->AddChild(spacer);

    Node* footer = arena.create<Node>();
    footer->heightMode = SizeMode::Fixed;
    footer->fixedHeight = 40.0f;
    col->AddChild(footer);

    Solver::Measure(col);
    Solver::Arrange(col, Rect(0.0f, 0.0f, 400.0f, 600.0f));

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

    keel::Arena arena;

    // Outer Column: 800 x 600
    Node* root = arena.create<Node>();
    root->direction = FlexDirection::Column;
    root->widthMode = SizeMode::Stretch;
    root->heightMode = SizeMode::Stretch;
    root->gap = 10.0f;

    // Row 1: height 50, two 50/50 buttons
    Node* row1 = arena.create<Node>();
    row1->direction = FlexDirection::Row;
    row1->widthMode = SizeMode::Stretch;
    row1->heightMode = SizeMode::Fixed;
    row1->fixedHeight = 50.0f;
    row1->gap = 10.0f;
    root->AddChild(row1);

    Node* btnA = arena.create<Node>();
    btnA->widthMode = SizeMode::Stretch;
    row1->AddChild(btnA);

    Node* btnB = arena.create<Node>();
    btnB->widthMode = SizeMode::Stretch;
    row1->AddChild(btnB);

    // Vertical Spacer
    Node* vSpacer = arena.create<Node>();
    vSpacer->isSpacer = true;
    vSpacer->heightMode = SizeMode::Stretch;
    root->AddChild(vSpacer);

    // Row 2 (Footer): height 40, one centered button
    Node* footer = arena.create<Node>();
    footer->direction = FlexDirection::Row;
    footer->widthMode = SizeMode::Stretch;
    footer->heightMode = SizeMode::Fixed;
    footer->fixedHeight = 40.0f;
    root->AddChild(footer);

    Node* footerBtn = arena.create<Node>();
    footerBtn->widthMode = SizeMode::Stretch;
    footer->AddChild(footerBtn);

    Solver::Measure(root);
    Solver::Arrange(root, Rect(0.0f, 0.0f, 810.0f, 600.0f));

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

    keel::Arena arena;

    // Outer Column: Header (50), Card (Fit with 90px intrinsic), Spacer, Footer (40)
    // Gap = 10. Total minimum required height = 50 + 10 + 90 + 10 + 0 + 10 + 40 = 210.
    Node* root = arena.create<Node>();
    root->direction = FlexDirection::Column;
    root->widthMode = SizeMode::Stretch;
    root->heightMode = SizeMode::Stretch;
    root->gap = 10.0f;

    Node* header = arena.create<Node>();
    header->heightMode = SizeMode::Fixed;
    header->fixedHeight = 50.0f;
    root->AddChild(header);

    Node* card = arena.create<Node>();
    card->direction = FlexDirection::Column;
    card->widthMode = SizeMode::Stretch;
    card->heightMode = SizeMode::Fit;
    card->gap = 10.0f;

    Node* item1 = arena.create<Node>();
    item1->heightMode = SizeMode::Fixed;
    item1->fixedHeight = 40.0f;
    card->AddChild(item1);

    Node* item2 = arena.create<Node>();
    item2->heightMode = SizeMode::Fixed;
    item2->fixedHeight = 40.0f;
    card->AddChild(item2);

    root->AddChild(card);

    Node* spacer = arena.create<Node>();
    spacer->isSpacer = true;
    spacer->heightMode = SizeMode::Stretch;
    root->AddChild(spacer);

    Node* footer = arena.create<Node>();
    footer->heightMode = SizeMode::Fixed;
    footer->fixedHeight = 40.0f;
    root->AddChild(footer);

    Solver::Measure(root);

    // 1. With ample height (400px): spacer absorbs remaining space, footer sticks to bottom
    Solver::Arrange(root, Rect(0.0f, 0.0f, 300.0f, 400.0f));
    CHECK_NEAR(header->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(header->computedRect.h, 50.0f, 0.001f);
    CHECK_NEAR(card->computedRect.y, 60.0f, 0.001f);
    CHECK_NEAR(card->computedRect.h, 90.0f, 0.001f);
    // freeSpace = 400 - 210 = 190
    CHECK_NEAR(spacer->computedRect.h, 190.0f, 0.001f);
    CHECK_NEAR(footer->computedRect.Bottom(), 400.0f, 0.001f);

    // 2. In severe height deficit (100px < 210px):
    // Spacer shrinks to 0; no non-spacer widgets squish below intrinsic size; zero overlap.
    Solver::Arrange(root, Rect(0.0f, 0.0f, 300.0f, 100.0f));
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

    keel::Arena arena;

    // Wrap container: gap = 10, runGap = 12, widthMode = Stretch, heightMode = Fit
    Node* wrap = arena.create<Node>();
    wrap->direction = FlexDirection::Wrap;
    wrap->widthMode = SizeMode::Stretch;
    wrap->heightMode = SizeMode::Fit;
    wrap->gap = 10.0f;
    wrap->runGap = 12.0f;

    // 4 items: each fixedWidth = 80, fixedHeight = 30
    Node* items[4];
    for (keel::i32 i = 0; i < 4; ++i) {
        items[i] = arena.create<Node>();
        items[i]->widthMode = SizeMode::Fixed;
        items[i]->heightMode = SizeMode::Fixed;
        items[i]->fixedWidth = 80.0f;
        items[i]->fixedHeight = 30.0f;
        wrap->AddChild(items[i]);
    }

    Solver::Measure(wrap);

    // Intrinsic unconstrained width: 4 * 80 + 3 * 10 = 350, height = 30
    CHECK_NEAR(wrap->intrinsicSize.x, 350.0f, 0.001f);
    CHECK_NEAR(wrap->intrinsicSize.y, 30.0f, 0.001f);

    // Case 1: Wide viewport (400px >= 350px): all 4 items fit on 1 line
    Solver::Arrange(wrap, Rect(0.0f, 0.0f, 400.0f, 100.0f));
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
    Solver::Arrange(wrap, Rect(0.0f, 0.0f, 200.0f, 100.0f));
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
    Solver::Arrange(wrap, Rect(0.0f, 0.0f, 100.0f, 200.0f));
    CHECK_NEAR(wrap->computedRect.h, 156.0f, 0.001f);
    for (keel::i32 i = 0; i < 4; ++i) {
        CHECK_NEAR(items[i]->computedRect.x, 0.0f, 0.001f);
        CHECK_NEAR(items[i]->computedRect.y, keel::as<keel::f32>(i * (30 + 12)), 0.001f);
    }
}

void TestWrapInColumnWithSpacer() {
    using namespace guikit;

    keel::Arena arena;

    // Outer column (width 200, height 300, gap 10)
    // Contains: Wrap (2 items per line = 72px), Spacer, Footer (30px)
    Node* col = arena.create<Node>();
    col->direction = FlexDirection::Column;
    col->widthMode = SizeMode::Stretch;
    col->heightMode = SizeMode::Stretch;
    col->gap = 10.0f;

    Node* wrap = arena.create<Node>();
    wrap->direction = FlexDirection::Wrap;
    wrap->widthMode = SizeMode::Stretch;
    wrap->heightMode = SizeMode::Fit;
    wrap->gap = 10.0f;
    wrap->runGap = 12.0f;

    for (keel::i32 i = 0; i < 4; ++i) {
        Node* item = arena.create<Node>();
        item->widthMode = SizeMode::Fixed;
        item->heightMode = SizeMode::Fixed;
        item->fixedWidth = 80.0f;
        item->fixedHeight = 30.0f;
        wrap->AddChild(item);
    }
    col->AddChild(wrap);

    Node* spacer = arena.create<Node>();
    spacer->isSpacer = true;
    spacer->heightMode = SizeMode::Stretch;
    col->AddChild(spacer);

    Node* footer = arena.create<Node>();
    footer->heightMode = SizeMode::Fixed;
    footer->fixedHeight = 30.0f;
    col->AddChild(footer);

    Solver::Measure(col);
    // At width 200, Wrap wraps into 2 lines (height 72)
    // Total column height = 300
    // non-spacer baseSum = 72 + 30 = 102
    // gaps = 2 * 10 = 20
    // spacer freeSpace = 300 - 20 - 102 = 178
    // footer y = 72 + 10 + 178 + 10 = 270 (Bottom() == 300)
    Solver::Arrange(col, Rect(0.0f, 0.0f, 200.0f, 300.0f));

    CHECK_NEAR(wrap->computedRect.h, 72.0f, 0.001f);
    CHECK_NEAR(spacer->computedRect.h, 178.0f, 0.001f);
    CHECK_NEAR(footer->computedRect.y, 270.0f, 0.001f);
    CHECK_NEAR(footer->computedRect.Bottom(), 300.0f, 0.001f);
}

void TestGridLayoutSolver() {
    using namespace guikit;

    keel::Arena arena;

    // Grid container with width 600, gap 12, runGap 16
    Node* grid = arena.create<Node>();
    grid->direction = FlexDirection::Grid;
    grid->SetPayload(arena.create<GridData>(GridData{.columns = 12}));
    grid->gap = 12.0f;
    grid->runGap = 16.0f;
    grid->widthMode = SizeMode::Fixed;
    grid->fixedWidth = 600.0f;
    grid->heightMode = SizeMode::Fit;

    // Row 1: Two cells with colSpan = 6
    Node* c1 = arena.create<Node>();
    c1->colSpan = 6;
    c1->heightMode = SizeMode::Fixed;
    c1->fixedHeight = 40.0f;
    grid->AddChild(c1);

    Node* c2 = arena.create<Node>();
    c2->colSpan = 6;
    c2->heightMode = SizeMode::Fixed;
    c2->fixedHeight = 40.0f;
    grid->AddChild(c2);

    // Row 2: Three cells with colSpan = 4
    Node* c3 = arena.create<Node>();
    c3->colSpan = 4;
    c3->heightMode = SizeMode::Fixed;
    c3->fixedHeight = 50.0f;
    grid->AddChild(c3);

    Node* c4 = arena.create<Node>();
    c4->colSpan = 4;
    c4->heightMode = SizeMode::Fixed;
    c4->fixedHeight = 50.0f;
    grid->AddChild(c4);

    Node* c5 = arena.create<Node>();
    c5->colSpan = 4;
    c5->heightMode = SizeMode::Fixed;
    c5->fixedHeight = 50.0f;
    grid->AddChild(c5);

    // Row 3: One cell with colSpan = 12
    Node* c6 = arena.create<Node>();
    c6->colSpan = 12;
    c6->heightMode = SizeMode::Fixed;
    c6->fixedHeight = 60.0f;
    grid->AddChild(c6);

    Solver::Measure(grid);
    Solver::Arrange(grid, Rect(0.0f, 0.0f, 600.0f, 0.0f));

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

    keel::Arena arena;

    // Grid container with width 400, gap 10, runGap 15
    Node* grid = arena.create<Node>();
    grid->direction = FlexDirection::Grid;
    grid->SetPayload(arena.create<GridData>(GridData{.columns = 12}));
    grid->gap = 10.0f;
    grid->runGap = 15.0f;
    grid->widthMode = SizeMode::Fixed;
    grid->fixedWidth = 400.0f;
    grid->heightMode = SizeMode::Fit;

    // Two cells with span 6 and minWidth 250
    Node* c1 = arena.create<Node>();
    c1->colSpan = 6;
    c1->minWidth = 250.0f;
    c1->heightMode = SizeMode::Fixed;
    c1->fixedHeight = 40.0f;
    grid->AddChild(c1);

    Node* c2 = arena.create<Node>();
    c2->colSpan = 6;
    c2->minWidth = 250.0f;
    c2->heightMode = SizeMode::Fixed;
    c2->fixedHeight = 40.0f;
    grid->AddChild(c2);

    // Case 1: Constrained space (width 400 < 2 * 250) -> wrap and expand to 100% width
    Solver::Measure(grid);
    Solver::Arrange(grid, Rect(0.0f, 0.0f, 400.0f, 0.0f));

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
    Solver::Measure(grid);
    Solver::Arrange(grid, Rect(0.0f, 0.0f, 600.0f, 0.0f));

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

    keel::Arena arena;

    // Viewport container 400x300
    Node* scroll = arena.create<Node>();
    scroll->type = NodeType::ScrollArea;
    scroll->direction = FlexDirection::Column;
    scroll->clipChildren = true;
    scroll->widthMode = SizeMode::Stretch;
    scroll->heightMode = SizeMode::Stretch;
    scroll->scroll = arena.create<ScrollData>();
    scroll->scroll->offset = {0.0f, 50.0f};

    // 3 children, 150px height each -> total content height = 450px
    Node* c1 = arena.create<Node>();
    c1->heightMode = SizeMode::Fixed;
    c1->fixedHeight = 150.0f;
    scroll->AddChild(c1);

    Node* c2 = arena.create<Node>();
    c2->heightMode = SizeMode::Fixed;
    c2->fixedHeight = 150.0f;
    scroll->AddChild(c2);

    Node* c3 = arena.create<Node>();
    c3->heightMode = SizeMode::Fixed;
    c3->fixedHeight = 150.0f;
    scroll->AddChild(c3);

    Solver::Measure(scroll);
    Solver::Arrange(scroll, Rect(0.0f, 0.0f, 400.0f, 300.0f));

    // ScrollArea viewport dimensions remain strictly bounded
    CHECK_NEAR(scroll->computedRect.w, 400.0f, 0.001f);
    CHECK_NEAR(scroll->computedRect.h, 300.0f, 0.001f);
    CHECK_NEAR(scroll->scroll->contentSize.y, 450.0f, 0.001f);

    // Children are offset by -50px
    CHECK_NEAR(c1->computedRect.y, -50.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.y, 100.0f, 0.001f);
    CHECK_NEAR(c3->computedRect.y, 250.0f, 0.001f);

    // Verify that content width strictly reserves scrollbar gutter (so scrollbar never overlays children)
    const ScrollbarMetrics& sm = UiTheme::Dark().GetScrollbar(scroll->uiSize);
    CHECK_NEAR(scroll->scroll->contentSize.x, 400.0f - sm.gutter, 0.001f);
    CHECK_NEAR(c1->computedRect.w, 400.0f - sm.gutter, 0.001f);

    // Verify over-scroll clamping (offset 500 clamped to max 450 - 300 = 150)
    scroll->scroll->offset = {0.0f, 500.0f};
    Solver::Arrange(scroll, Rect(0.0f, 0.0f, 400.0f, 300.0f));

    CHECK_NEAR(scroll->scroll->offset.y, 150.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.y, -150.0f, 0.001f);
    CHECK_NEAR(c2->computedRect.y, 0.0f, 0.001f);
    CHECK_NEAR(c3->computedRect.y, 150.0f, 0.001f);

    // Verify that when scrollbar policy is Never, no gutter is reserved and children occupy full 400px
    scroll->scroll->policy = VisibilityPolicy::Never;
    Solver::Arrange(scroll, Rect(0.0f, 0.0f, 400.0f, 300.0f));
    CHECK_NEAR(scroll->scroll->contentSize.x, 400.0f, 0.001f);
    CHECK_NEAR(c1->computedRect.w, 400.0f, 0.001f);
}

void TestCustomMeasureAndArrangeDelegates() {
    using namespace guikit;

    keel::Arena arena;

    // Custom widget with measureFn
    Node* customLeaf = arena.create<Node>();
    customLeaf->measureFn = [](const Node*, keel::f32 /*availableWidth*/, ICanvas*, const UiTheme&) -> Vec2 {
        return {123.0f, 45.0f};
    };

    Solver::Measure(customLeaf);
    CHECK_NEAR(customLeaf->intrinsicSize.x, 123.0f, 0.001f);
    CHECK_NEAR(customLeaf->intrinsicSize.y, 45.0f, 0.001f);

    // Custom container with arrangeFn
    Node* customContainer = arena.create<Node>();
    Node* child = arena.create<Node>();
    customContainer->AddChild(child);

    customContainer->arrangeFn = [](Node* node, const Rect& availableRect, const UiTheme& /*theme*/) {
        node->computedRect = availableRect;
        if (node->firstChild) {
            node->firstChild->computedRect = Rect(availableRect.x + 10.0f, availableRect.y + 10.0f, 50.0f, 50.0f);
        }
    };

    Solver::Arrange(customContainer, Rect(100.0f, 100.0f, 300.0f, 200.0f));
    CHECK_NEAR(customContainer->computedRect.x, 100.0f, 0.001f);
    CHECK_NEAR(customContainer->computedRect.w, 300.0f, 0.001f);
    CHECK_NEAR(child->computedRect.x, 110.0f, 0.001f);
    CHECK_NEAR(child->computedRect.y, 110.0f, 0.001f);
    CHECK_NEAR(child->computedRect.w, 50.0f, 0.001f);
}

void TestStretchColumnInConstrainedRowWithHugeContent() {
    using namespace guikit;

    keel::Arena arena;

    // Row: 1000px wide, 600px high, gap 20px
    Node* row = arena.create<Node>();
    row->direction = FlexDirection::Row;
    row->widthMode = SizeMode::Stretch;
    row->heightMode = SizeMode::Stretch;
    row->gap = 20.0f;

    // Sidebar: Fixed 300px
    Node* sidebar = arena.create<Node>();
    sidebar->widthMode = SizeMode::Fixed;
    sidebar->fixedWidth = 300.0f;
    sidebar->heightMode = SizeMode::Stretch;
    row->AddChild(sidebar);

    // Content Column: Stretch width (flexGrow = 1.0f)
    Node* contentCol = arena.create<Node>();
    contentCol->direction = FlexDirection::Column;
    contentCol->widthMode = SizeMode::Stretch;
    contentCol->heightMode = SizeMode::Stretch;
    contentCol->gap = 10.0f;
    row->AddChild(contentCol);

    // Inside content column: a Carousel with Stretch width and multiple tabs totaling 1500px width
    Node* carousel = arena.create<Node>();
    carousel->type = NodeType::Carousel;
    carousel->direction = FlexDirection::Row;
    carousel->widthMode = SizeMode::Stretch;
    carousel->heightMode = SizeMode::Fixed;
    carousel->fixedHeight = 40.0f;
    carousel->gap = 10.0f;
    carousel->measureFn = &detail::Carousel::Measure;
    carousel->arrangeFn = &detail::Carousel::Arrange;
    carousel->scroll = arena.create<ScrollData>();
    carousel->scroll->orientation = Orientation::Horizontal;
    carousel->scroll->solverManaged = false;

    auto* cData = arena.create<CarouselData>();
    carousel->SetPayload(cData);

    // Add 10 tabs, each 150px wide = 1500px + 90px gap = 1590px total
    for (keel::i32 i = 0; i < 10; ++i) {
        Node* tab = arena.create<Node>();
        tab->widthMode = SizeMode::Fixed;
        tab->fixedWidth = 150.0f;
        tab->heightMode = SizeMode::Stretch;
        carousel->AddChild(tab);
    }
    contentCol->AddChild(carousel);

    // Also a Grid with two Cell6 items (span 6 each), each containing a card with 380px intrinsic width
    Node* grid = arena.create<Node>();
    grid->direction = FlexDirection::Grid;
    grid->SetPayload(arena.create<GridData>(GridData{.columns = 12}));
    grid->widthMode = SizeMode::Stretch;
    grid->heightMode = SizeMode::Fit;
    grid->gap = 16.0f;

    Node* cell1 = arena.create<Node>();
    cell1->direction = FlexDirection::Column;
    cell1->colSpan = 6;
    cell1->minWidth = 380.0f;
    cell1->heightMode = SizeMode::Fixed;
    cell1->fixedHeight = 200.0f;
    grid->AddChild(cell1);

    Node* cell2 = arena.create<Node>();
    cell2->direction = FlexDirection::Column;
    cell2->colSpan = 6;
    cell2->minWidth = 380.0f;
    cell2->heightMode = SizeMode::Fixed;
    cell2->fixedHeight = 200.0f;
    grid->AddChild(cell2);

    contentCol->AddChild(grid);

    Solver::Measure(row);
    Solver::Arrange(row, Rect(0.0f, 0.0f, 1000.0f, 600.0f));

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
    CHECK(carousel->GetScrollContentSize().x > 1500.0f);

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

void TestCarouselArrowScaling() {
    using namespace guikit;

    keel::Arena arena;

    // 1. Arrange at 1.0x scale (default theme, defaultHeight 36px)
    Node* c1 = arena.create<Node>();
    c1->type = NodeType::Carousel;
    c1->direction = FlexDirection::Row;
    c1->widthMode = SizeMode::Stretch;
    c1->heightMode = SizeMode::Fixed;
    c1->fixedHeight = 36.0f;
    c1->measureFn = &detail::Carousel::Measure;
    c1->arrangeFn = &detail::Carousel::Arrange;
    auto* d1 = arena.create<CarouselData>();
    c1->payload = d1;

    for (keel::i32 i = 0; i < 10; ++i) {
        Node* tab = arena.create<Node>();
        tab->widthMode = SizeMode::Fixed;
        tab->fixedWidth = 100.0f;
        c1->AddChild(tab);
    }

    UiTheme theme1 = UiTheme::Dark();
    Solver::Measure(c1, nullptr, theme1);
    Solver::Arrange(c1, Rect(0.0f, 0.0f, 300.0f, 36.0f), theme1);

    CHECK(d1->showRightArrow == true);
    const keel::f32 h1 = d1->rightArrowRect.h;
    const keel::f32 w1 = d1->rightArrowRect.w;
    CHECK_NEAR(h1, 36.0f, 0.001f); // Full-height scroll button spans content height
    CHECK(w1 >= 18.0f && w1 <= 23.0f); // Md arrow button width token is 19.5 dp

    // 2. Arrange at 2.0x UI size factor (UiSize::Factor(2.0f), height 72px)
    Node* c2 = arena.create<Node>();
    c2->type = NodeType::Carousel;
    c2->uiSize = UiSize::Factor(2.0f);
    c2->direction = FlexDirection::Row;
    c2->widthMode = SizeMode::Stretch;
    c2->heightMode = SizeMode::Fixed;
    c2->fixedHeight = 72.0f;
    c2->measureFn = &detail::Carousel::Measure;
    c2->arrangeFn = &detail::Carousel::Arrange;
    auto* d2 = arena.create<CarouselData>();
    c2->payload = d2;

    for (keel::i32 i = 0; i < 10; ++i) {
        Node* tab = arena.create<Node>();
        tab->widthMode = SizeMode::Fixed;
        tab->fixedWidth = 200.0f;
        c2->AddChild(tab);
    }

    UiTheme theme2 = UiTheme::Dark();
    Solver::Measure(c2, nullptr, theme2);
    Solver::Arrange(c2, Rect(0.0f, 0.0f, 300.0f, 72.0f), theme2);

    CHECK(d2->showRightArrow == true);
    const keel::f32 h2 = d2->rightArrowRect.h;
    const keel::f32 w2 = d2->rightArrowRect.w;
    CHECK_NEAR(h2, 72.0f, 0.001f); // Full-height scroll button
    // The 2.0x arrow button width must scale cleanly with UiSize (not capped!)
    CHECK_NEAR(w2, w1 * 2.0f, 2.0f);
    CHECK(w2 > 37.0f);

    // 3. Tall card ribbon (height 140px at Md): width stays token-sized (~19.5dp), height stretches to full 140px
    Node* c3 = arena.create<Node>();
    c3->type = NodeType::Carousel;
    c3->direction = FlexDirection::Row;
    c3->widthMode = SizeMode::Stretch;
    c3->heightMode = SizeMode::Fixed;
    c3->fixedHeight = 140.0f;
    c3->padding = Padding::All(8.0f);
    c3->measureFn = &detail::Carousel::Measure;
    c3->arrangeFn = &detail::Carousel::Arrange;
    auto* d3 = arena.create<CarouselData>();
    c3->payload = d3;

    for (keel::i32 i = 0; i < 6; ++i) {
        Node* card = arena.create<Node>();
        card->widthMode = SizeMode::Fixed;
        card->fixedWidth = 160.0f;
        c3->AddChild(card);
    }

    Solver::Measure(c3, nullptr, theme1);
    Solver::Arrange(c3, Rect(0.0f, 0.0f, 300.0f, 140.0f), theme1);

    CHECK(d3->showRightArrow == true);
    CHECK_NEAR(d3->rightArrowRect.w, w1, 0.001f);
    CHECK_NEAR(d3->rightArrowRect.y, 8.0f, 0.001f);
    CHECK_NEAR(d3->rightArrowRect.h, 124.0f, 0.001f); // 140 - 2*8 padding
}

void TestAspectRatioMinHeightAndStretchFallback() {
    using namespace guikit;

    keel::Arena arena;

    // 1. AspectRatio auto-height in a Fit Column
    Node* col = arena.create<Node>();
    col->direction = FlexDirection::Column;
    col->widthMode = SizeMode::Fixed;
    col->fixedWidth = 400.0f;
    col->heightMode = SizeMode::Fit;
    col->gap = 10.0f;

    Node* imgNode = arena.create<Node>();
    imgNode->type = NodeType::Image;
    imgNode->widthMode = SizeMode::Stretch;
    imgNode->heightMode = SizeMode::Fit;
    imgNode->aspectRatio = 2.0f; // width / height = 2.0 -> height = 400 / 2 = 200
    col->AddChild(imgNode);

    // 2. Stretch child inside a Fit Column falls back to minHeight instead of collapsing to 0!
    Node* sceneNode = arena.create<Node>();
    sceneNode->type = NodeType::Scene;
    sceneNode->widthMode = SizeMode::Stretch;
    sceneNode->heightMode = SizeMode::Stretch;
    sceneNode->minHeight = 240.0f;
    col->AddChild(sceneNode);

    Solver::Measure(col);
    Solver::Arrange(col, Rect(0.0f, 0.0f, 400.0f, 0.0f));

    CHECK_NEAR(imgNode->computedRect.w, 400.0f, 0.001f);
    CHECK_NEAR(imgNode->computedRect.h, 200.0f, 0.001f);

    CHECK_NEAR(sceneNode->computedRect.w, 400.0f, 0.001f);
    CHECK_NEAR(sceneNode->computedRect.h, 240.0f, 0.001f);
    CHECK_NEAR(col->computedRect.h, 450.0f, 0.001f); // 200 + 10 + 240
}

void TestRowEqualWidthsFitAndStretch() {
    using namespace guikit;

    keel::Arena arena;

    // 1. Fit Row with equalWidths = true: all items adopt the widest child's intrinsic width (80px)
    Node* fitRow = arena.create<Node>();
    fitRow->direction = FlexDirection::Row;
    fitRow->widthMode = SizeMode::Fit;
    fitRow->heightMode = SizeMode::Fit;
    fitRow->equalWidths = true;
    fitRow->gap = 8.0f;

    const keel::f32 widths[5] = {80.0f, 62.0f, 60.0f, 56.0f, 52.0f};
    Node* items[5]{};
    for (keel::i32 i = 0; i < 5; ++i) {
        Node* n = arena.create<Node>();
        n->widthMode = SizeMode::Fit;
        n->heightMode = SizeMode::Fixed;
        n->fixedHeight = 32.0f;
        const keel::f32 w = widths[i];
        n->measureFn = (w == 80.0f)
            ? +[](const Node*, keel::f32, ICanvas*, const UiTheme&) -> Vec2 { return {80.0f, 32.0f}; }
            : (w == 62.0f)
                ? +[](const Node*, keel::f32, ICanvas*, const UiTheme&) -> Vec2 { return {62.0f, 32.0f}; }
                : (w == 60.0f)
                    ? +[](const Node*, keel::f32, ICanvas*, const UiTheme&) -> Vec2 { return {60.0f, 32.0f}; }
                    : (w == 56.0f)
                        ? +[](const Node*, keel::f32, ICanvas*, const UiTheme&) -> Vec2 { return {56.0f, 32.0f}; }
                        : +[](const Node*, keel::f32, ICanvas*, const UiTheme&) -> Vec2 { return {52.0f, 32.0f}; };
        fitRow->AddChild(n);
        items[i] = n;
    }

    Solver::Measure(fitRow);
    // Intrinsic width = 5 * 80 + 4 * 8 = 432
    CHECK_NEAR(fitRow->intrinsicSize.x, 432.0f, 0.001f);

    Solver::Arrange(fitRow, Rect(0.0f, 0.0f, 1000.0f, 40.0f));
    CHECK_NEAR(fitRow->computedRect.w, 432.0f, 0.001f);
    for (keel::i32 i = 0; i < 5; ++i) {
        CHECK_NEAR(items[i]->computedRect.w, 80.0f, 0.001f);
        CHECK_NEAR(items[i]->computedRect.x, keel::as<keel::f32>(i) * 88.0f, 0.001f);
    }

    // 2. Stretch Row with equalWidths = true: items divide row width equally
    Node* stretchRow = arena.create<Node>();
    stretchRow->direction = FlexDirection::Row;
    stretchRow->widthMode = SizeMode::Stretch;
    stretchRow->heightMode = SizeMode::Fit;
    stretchRow->equalWidths = true;
    stretchRow->gap = 10.0f;

    Node* s1 = arena.create<Node>();
    s1->widthMode = SizeMode::Fit;
    s1->measureFn = +[](const Node*, keel::f32, ICanvas*, const UiTheme&) -> Vec2 { return {50.0f, 32.0f}; };
    stretchRow->AddChild(s1);

    Node* s2 = arena.create<Node>();
    s2->widthMode = SizeMode::Fit;
    s2->measureFn = +[](const Node*, keel::f32, ICanvas*, const UiTheme&) -> Vec2 { return {110.0f, 32.0f}; };
    stretchRow->AddChild(s2);

    Solver::Measure(stretchRow);
    Solver::Arrange(stretchRow, Rect(0.0f, 0.0f, 410.0f, 32.0f));
    CHECK_NEAR(s1->computedRect.w, 200.0f, 0.001f);
    CHECK_NEAR(s2->computedRect.w, 200.0f, 0.001f);
    CHECK_NEAR(s2->computedRect.x, 210.0f, 0.001f);
}

void TestRowTextDeficitShrinkAndCenterClamp() {
    using namespace guikit;

    keel::Arena arena;

    // 1. Row (300px, gap 8px) with Fixed left slot (52px), long Text (240px), and short readout Text (56px):
    // Long text absorbs deficit first (shrinks to 176px), short readout keeps its full 56px width!
    Node* row = arena.create<Node>();
    row->direction = FlexDirection::Row;
    row->widthMode = SizeMode::Stretch;
    row->heightMode = SizeMode::Fixed;
    row->fixedHeight = 28.0f;
    row->gap = 8.0f;

    Node* leftSlot = arena.create<Node>();
    leftSlot->widthMode = SizeMode::Fixed;
    leftSlot->fixedWidth = 52.0f;
    leftSlot->heightMode = SizeMode::Stretch;
    row->AddChild(leftSlot);

    Node* longTitle = arena.create<Node>();
    longTitle->type = NodeType::Text;
    longTitle->widthMode = SizeMode::Fit;
    longTitle->measureFn = +[](const Node*, keel::f32, ICanvas*, const UiTheme&) -> Vec2 { return {240.0f, 16.0f}; };
    row->AddChild(longTitle);

    Node* shortBadge = arena.create<Node>();
    shortBadge->type = NodeType::Text;
    shortBadge->widthMode = SizeMode::Fit;
    shortBadge->measureFn = +[](const Node*, keel::f32, ICanvas*, const UiTheme&) -> Vec2 { return {56.0f, 14.0f}; };
    row->AddChild(shortBadge);

    Solver::Measure(row);
    Solver::Arrange(row, Rect(0.0f, 0.0f, 300.0f, 28.0f));

    CHECK_NEAR(leftSlot->computedRect.w, 52.0f, 0.001f);
    CHECK_NEAR(longTitle->computedRect.x, 60.0f, 0.001f);
    CHECK_NEAR(longTitle->computedRect.w, 176.0f, 0.001f);
    CHECK_NEAR(shortBadge->computedRect.w, 56.0f, 0.001f);
    CHECK_NEAR(shortBadge->computedRect.Right(), 300.0f, 0.001f);

    // 2. Justify::Center Row (240px) with a single Fit child (320px):
    // Child is clamped to 240px and anchored at x = 0.0f (no negative offset).
    Node* centerRow = arena.create<Node>();
    centerRow->direction = FlexDirection::Row;
    centerRow->justify = Justify::Center;
    centerRow->widthMode = SizeMode::Stretch;
    centerRow->heightMode = SizeMode::Fixed;
    centerRow->fixedHeight = 40.0f;

    Node* wideChild = arena.create<Node>();
    wideChild->widthMode = SizeMode::Fit;
    wideChild->measureFn = +[](const Node*, keel::f32, ICanvas*, const UiTheme&) -> Vec2 { return {320.0f, 32.0f}; };
    centerRow->AddChild(wideChild);

    Solver::Measure(centerRow);
    Solver::Arrange(centerRow, Rect(0.0f, 0.0f, 240.0f, 40.0f));

    CHECK_NEAR(wideChild->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(wideChild->computedRect.w, 240.0f, 0.001f);

    // 2b. Fit Card with minWidth = 360px inside Justify::Center Row:
    // - On wide viewport (800px): Card is 360px wide and centered at x = (800 - 360) / 2 = 220px.
    // - On narrow phone viewport (320px): Card clamps to 320px at x = 0px, and its Fit child clamps to inner width.
    Node* cardCenterRow = arena.create<Node>();
    cardCenterRow->direction = FlexDirection::Row;
    cardCenterRow->justify = Justify::Center;
    cardCenterRow->align = Alignment::Center;
    cardCenterRow->widthMode = SizeMode::Stretch;
    cardCenterRow->heightMode = SizeMode::Stretch;

    Node* cardNode = arena.create<Node>();
    cardNode->type = NodeType::Card;
    cardNode->direction = FlexDirection::Column;
    cardNode->widthMode = SizeMode::Fit;
    cardNode->heightMode = SizeMode::Fit;
    cardNode->minWidth = 360.0f;
    cardNode->padding = Padding::All(16.0f);

    Node* cardText = arena.create<Node>();
    cardText->type = NodeType::Text;
    cardText->widthMode = SizeMode::Fit;
    cardText->heightMode = SizeMode::Fit;
    cardText->measureFn = +[](const Node*, keel::f32, ICanvas*, const UiTheme&) -> Vec2 { return {200.0f, 20.0f}; };
    cardNode->AddChild(cardText);
    cardCenterRow->AddChild(cardNode);

    Solver::Measure(cardCenterRow);
    CHECK_NEAR(cardNode->intrinsicSize.x, 360.0f, 0.001f);

    Solver::Arrange(cardCenterRow, Rect(0.0f, 0.0f, 800.0f, 400.0f));
    CHECK_NEAR(cardNode->computedRect.x, 220.0f, 0.001f);
    CHECK_NEAR(cardNode->computedRect.w, 360.0f, 0.001f);

    Solver::Arrange(cardCenterRow, Rect(0.0f, 0.0f, 320.0f, 400.0f));
    CHECK_NEAR(cardNode->computedRect.x, 0.0f, 0.001f);
    CHECK_NEAR(cardNode->computedRect.w, 320.0f, 0.001f);
    CHECK(cardText->computedRect.Right() <= cardNode->computedRect.Right() - 16.0f + 0.001f);

    // 3. Full-stretch clipped Box inside ScrollArea stretches vertically to full viewport height
    Node* scrollBackdrop = arena.create<Node>();
    scrollBackdrop->type = NodeType::ScrollArea;
    scrollBackdrop->direction = FlexDirection::Column;
    scrollBackdrop->scroll = arena.create<ScrollData>();
    scrollBackdrop->scroll->policy = VisibilityPolicy::Never;
    scrollBackdrop->padding = Padding::All(18.0f);
    scrollBackdrop->widthMode = SizeMode::Stretch;
    scrollBackdrop->heightMode = SizeMode::Stretch;

    Node* winFrame = arena.create<Node>();
    winFrame->type = NodeType::Box;
    winFrame->widthMode = SizeMode::Stretch;
    winFrame->heightMode = SizeMode::Stretch;
    winFrame->clipChildren = true;
    scrollBackdrop->AddChild(winFrame);

    Node* titlebar = arena.create<Node>();
    titlebar->heightMode = SizeMode::Fixed;
    titlebar->fixedHeight = 28.0f;
    winFrame->AddChild(titlebar);

    Solver::Measure(scrollBackdrop);
    Solver::Arrange(scrollBackdrop, Rect(0.0f, 0.0f, 800.0f, 500.0f));

    CHECK_NEAR(winFrame->computedRect.x, 18.0f, 0.001f);
    CHECK_NEAR(winFrame->computedRect.y, 18.0f, 0.001f);
    CHECK_NEAR(winFrame->computedRect.w, 764.0f, 0.001f);
    CHECK_NEAR(winFrame->computedRect.h, 464.0f, 0.001f);

    // 4. Stretch Scene with fallback minHeight=220 inside a Fixed BottomPanel (height=180) with a Toolbar (height=34)
    // must shrink to exact remaining freeSpace (146px) instead of overflowing BottomPanel to 220px
    Node* bpNode = arena.create<Node>();
    bpNode->type = NodeType::BottomPanel;
    bpNode->direction = FlexDirection::Column;
    bpNode->widthMode = SizeMode::Stretch;
    bpNode->heightMode = SizeMode::Fixed;
    bpNode->fixedHeight = 180.0f;
    bpNode->clipChildren = true;

    Node* tbNode = arena.create<Node>();
    tbNode->type = NodeType::Toolbar;
    tbNode->widthMode = SizeMode::Stretch;
    tbNode->heightMode = SizeMode::Fixed;
    tbNode->fixedHeight = 34.0f;
    bpNode->AddChild(tbNode);

    Node* scNode = arena.create<Node>();
    scNode->type = NodeType::Scene;
    scNode->widthMode = SizeMode::Stretch;
    scNode->heightMode = SizeMode::Stretch;
    scNode->minHeight = 220.0f;
    scNode->clipChildren = true;
    bpNode->AddChild(scNode);

    Solver::Measure(bpNode);
    Solver::Arrange(bpNode, Rect(0.0f, 620.0f, 1000.0f, 180.0f));

    CHECK_NEAR(tbNode->computedRect.h, 34.0f, 0.001f);
    CHECK_NEAR(scNode->computedRect.y, 654.0f, 0.001f);
    CHECK_NEAR(scNode->computedRect.h, 146.0f, 0.001f);
    CHECK_NEAR(scNode->computedRect.Bottom(), bpNode->computedRect.Bottom(), 0.001f);
}

void TestScrollAreaMaxHeightWrapFitAndDeepTreeGuard() {
    using namespace guikit;

    keel::Arena arena;

    // 1. ScrollArea with Fit height + maxHeight = 200px inside a Fit Column:
    //    - Short content (120px) shrink-wraps to 120px.
    //    - Tall content (450px) caps viewport height at 200px while preserving scrollContentSize.y = 450px.
    Node* parentCol = arena.create<Node>();
    parentCol->direction = FlexDirection::Column;
    parentCol->widthMode = SizeMode::Fixed;
    parentCol->fixedWidth = 400.0f;
    parentCol->heightMode = SizeMode::Fit;

    Node* scroll = arena.create<Node>();
    scroll->type = NodeType::ScrollArea;
    scroll->direction = FlexDirection::Column;
    scroll->clipChildren = true;
    scroll->widthMode = SizeMode::Stretch;
    scroll->heightMode = SizeMode::Fit;
    scroll->scroll = arena.create<ScrollData>();
    scroll->scroll->maxHeight = 200.0f;

    Node* item1 = arena.create<Node>();
    item1->heightMode = SizeMode::Fixed;
    item1->fixedHeight = 120.0f;
    scroll->AddChild(item1);
    parentCol->AddChild(scroll);

    Solver::Measure(parentCol);
    Solver::Arrange(parentCol, Rect(0.0f, 0.0f, 400.0f, 0.0f));
    CHECK_NEAR(scroll->intrinsicSize.y, 120.0f, 0.001f);
    CHECK_NEAR(scroll->computedRect.h, 120.0f, 0.001f);

    Node* item2 = arena.create<Node>();
    item2->heightMode = SizeMode::Fixed;
    item2->fixedHeight = 330.0f;
    scroll->AddChild(item2);

    Solver::Measure(parentCol);
    Solver::Arrange(parentCol, Rect(0.0f, 0.0f, 400.0f, 0.0f));
    CHECK_NEAR(scroll->intrinsicSize.y, 200.0f, 0.001f);
    CHECK_NEAR(scroll->computedRect.h, 200.0f, 0.001f);
    CHECK_NEAR(scroll->scroll->contentSize.y, 450.0f, 0.001f);
    CHECK_NEAR(parentCol->computedRect.h, 200.0f, 0.001f);

    // 2. Wrap container with widthMode = SizeMode::Fit shrink-wraps to single-line content width
    Node* wrapRow = arena.create<Node>();
    wrapRow->direction = FlexDirection::Row;
    wrapRow->widthMode = SizeMode::Stretch;
    wrapRow->heightMode = SizeMode::Fit;

    Node* fitWrap = arena.create<Node>();
    fitWrap->direction = FlexDirection::Wrap;
    fitWrap->widthMode = SizeMode::Fit;
    fitWrap->heightMode = SizeMode::Fit;
    fitWrap->gap = 10.0f;
    fitWrap->runGap = 8.0f;

    for (keel::i32 i = 0; i < 2; ++i) {
        Node* chip = arena.create<Node>();
        chip->widthMode = SizeMode::Fixed;
        chip->fixedWidth = 90.0f;
        chip->heightMode = SizeMode::Fixed;
        chip->fixedHeight = 28.0f;
        fitWrap->AddChild(chip);
    }
    wrapRow->AddChild(fitWrap);

    Solver::Measure(wrapRow);
    Solver::Arrange(wrapRow, Rect(0.0f, 0.0f, 600.0f, 40.0f));
    CHECK_NEAR(fitWrap->intrinsicSize.x, 190.0f, 0.001f);
    CHECK_NEAR(fitWrap->computedRect.w, 190.0f, 0.001f);
    CHECK_NEAR(fitWrap->computedRect.h, 28.0f, 0.001f);

    // 3. Deep nesting recursion guard (> 128 levels) completes safely without unbounded recursion
    Node* deepRoot = arena.create<Node>();
    Node* cur = deepRoot;
    for (keel::i32 d = 0; d < 160; ++d) {
        Node* next = arena.create<Node>();
        next->direction = FlexDirection::Column;
        next->widthMode = SizeMode::Stretch;
        next->heightMode = SizeMode::Fit;
        cur->AddChild(next);
        cur = next;
    }
    Solver::Measure(deepRoot);
    Solver::Arrange(deepRoot, Rect(0.0f, 0.0f, 300.0f, 200.0f));
    CHECK_NEAR(deepRoot->computedRect.w, 300.0f, 0.001f);
}

void TestGapDpAndContainerSizingConstraints() {
    using namespace guikit;

    class LayoutMockCanvas final : public ICanvas {
    public:
        void Begin(keel::u32, keel::u32, keel::f32 s = 1.0f) override { m_scale = s; }
        void End() override {}
        [[nodiscard]] keel::f32 GetScale() const noexcept override { return m_scale; }
        void SetScale(keel::f32 s) override { m_scale = s; }
        void PushTransform(const Transform2D&) override {}
        void PopTransform() override {}
        [[nodiscard]] keel::StringRef GetBackendName() const noexcept override { return "LayoutMock"; }
        void Clear(Color, keel::Optional<Rect> = keel::kNullOpt) override {}
        void PushClip(Rect) override {}
        void PopClip() override {}
        void DrawRect(Rect, Color) override {}
        void DrawRoundRect(Rect, keel::f32, Color, Color = Color(0, 0, 0, 0), keel::f32 = 0.0f) override {}
        Vec2 MeasureText(keel::StringRef t, keel::f32 fs) override {
            return {keel::as<keel::f32>(t.size()) * fs * 0.5f, fs};
        }
        [[nodiscard]] keel::f32 GetLineHeight(keel::f32 fs) override { return fs * 1.2f; }
        void DrawText(keel::StringRef, Vec2, keel::f32, Color) override {}
        void DrawTextBox(keel::StringRef, Rect, keel::f32, Color, keel::f32 = 0.0f, keel::f32 = 0.5f) override {}
        void DrawLine(Vec2, Vec2, Color, keel::f32 = 1.0f) override {}
    private:
        keel::f32 m_scale{1.0f};
    };

    Context ui;
    LayoutMockCanvas canvas;

    Response b1{}, b2{}, b3{};
    Node* col1Node = nullptr;
    Node* col2Node = nullptr;
    Node* constrainedBox = nullptr;
    Node* constrainedCard = nullptr;

    auto renderFrame = [&]() {
        ui.BeginFrame({600.0f, 600.0f}, 1.0f);
        if (auto root = guikit::Column(ui, {.gap = 0.0f, .padding = Padding::All(0.0f), .width = SizeMode::Stretch, .height = SizeMode::Stretch})) {
            // 1. ui.Gap(24.0f) fixed pixel offset vs ui.Spacer(1.0f) spring
            if (auto r = guikit::Row(ui, {.gap = 0.0f, .padding = Padding::All(0.0f), .width = SizeMode::Stretch})) {
                b1 = guikit::Button(ui, {.text = "A", .width = 80.0f});
                guikit::Gap(ui, 24.0f);
                b2 = guikit::Button(ui, {.text = "B", .width = 80.0f});
                guikit::Spacer(ui, 1.0f);
                b3 = guikit::Button(ui, {.text = "C", .width = 80.0f});
            }

            // 2. Weighted flexGrow (1 : 3) across sibling Columns inside a Row
            if (auto flexRow = guikit::Row(ui, {.gap = 0.0f, .padding = Padding::All(0.0f), .width = SizeMode::Stretch, .height = 60.0f})) {
                if (auto c1 = guikit::Column(ui, {.width = SizeMode::Stretch, .height = SizeMode::Stretch, .flexGrow = 1.0f})) {
                    col1Node = c1.GetNode();
                }
                if (auto c2 = guikit::Column(ui, {.width = SizeMode::Stretch, .height = SizeMode::Stretch, .flexGrow = 3.0f})) {
                    col2Node = c2.GetNode();
                }
            }

            // 3. Container maxWidth & minHeight on Box, and minWidth & maxHeight on Card
            if (auto bx = guikit::Box(ui, {
                .width = SizeMode::Stretch,
                .maxWidth = 280.0f,
                .minHeight = 90.0f,
                .padding = Padding::All(0.0f)
            })) {
                constrainedBox = bx.GetNode();
            }
            if (auto cd = guikit::Card(ui, {
                .minWidth = 220.0f,
                .width = SizeMode::Fit,
                .height = SizeMode::Fit,
                .maxHeight = 50.0f,
                .padding = Padding::All(0.0f)
            })) {
                constrainedCard = cd.GetNode();
                guikit::Button(ui, {.text = "Tall", .width = 60.0f, .height = 120.0f});
            }
        }
        ui.EndFrame(canvas);
    };

    renderFrame();
    renderFrame();

    CHECK_NEAR(b1.rect.x, 0.0f, 0.01f);
    CHECK_NEAR(b1.rect.w, 80.0f, 0.01f);
    CHECK_NEAR(b2.rect.x, 104.0f, 0.01f);
    CHECK_NEAR(b2.rect.w, 80.0f, 0.01f);
    CHECK_NEAR(b3.rect.x, 520.0f, 0.01f);
    CHECK_NEAR(b3.rect.w, 80.0f, 0.01f);

    CHECK(col1Node != nullptr && col2Node != nullptr);
    if (col1Node && col2Node) {
        CHECK_NEAR(col1Node->computedRect.w, 150.0f, 0.01f);
        CHECK_NEAR(col2Node->computedRect.w, 450.0f, 0.01f);
    }

    CHECK(constrainedBox != nullptr);
    if (constrainedBox) {
        CHECK_NEAR(constrainedBox->computedRect.w, 280.0f, 0.01f);
        CHECK_NEAR(constrainedBox->computedRect.h, 90.0f, 0.01f);
    }

    CHECK(constrainedCard != nullptr);
    if (constrainedCard) {
        CHECK_NEAR(constrainedCard->computedRect.w, 220.0f, 0.01f);
        CHECK_NEAR(constrainedCard->computedRect.h, 50.0f, 0.01f);
    }
}

// A node with its own scale lays its subtree out in local units: the parent sees its outer size in parent
// units (local size x scale), the node keeps the top-left corner the parent gives it, and its computedRect and
// children are in local units. Measuring twice must not compound the conversion.
void TestScaledNodeLaysOutInLocalUnits() {
    using namespace guikit;

    keel::Arena arena;
    Node* col = arena.create<Node>();
    col->direction = FlexDirection::Column;
    col->widthMode = SizeMode::Stretch;
    col->heightMode = SizeMode::Stretch;
    col->align = Alignment::Start;
    col->gap = 10.0f;

    // Fit-sized box at 2x containing a 50 x 20 leaf, padding 5 (all local)
    Node* fitBox = arena.create<Node>();
    fitBox->direction = FlexDirection::Column;
    fitBox->widthMode = SizeMode::Fit;
    fitBox->heightMode = SizeMode::Fit;
    fitBox->padding = Padding::All(5.0f);
    Zone fitZone{.scale = 2.0f};
    fitBox->zone = &fitZone;
    col->AddChild(fitBox);

    Node* leaf = arena.create<Node>();
    leaf->zone = fitBox->zone;
    leaf->widthMode = SizeMode::Fixed;
    leaf->heightMode = SizeMode::Fixed;
    leaf->fixedWidth = 50.0f;
    leaf->fixedHeight = 20.0f;
    fitBox->AddChild(leaf);

    // Stretch-width box at 0.5x: 400 parent units wide = 800 local units
    Node* wideBox = arena.create<Node>();
    wideBox->direction = FlexDirection::Column;
    wideBox->widthMode = SizeMode::Stretch;
    wideBox->heightMode = SizeMode::Fixed;
    wideBox->fixedHeight = 40.0f; // parent units
    Zone wideZone{.scale = 0.5f};
    wideBox->zone = &wideZone;
    col->AddChild(wideBox);

    Node* after = arena.create<Node>();
    after->widthMode = SizeMode::Fixed;
    after->heightMode = SizeMode::Fixed;
    after->fixedWidth = 30.0f;
    after->fixedHeight = 30.0f;
    col->AddChild(after);

    for (keel::i32 pass = 0; pass < 2; ++pass) {
        Solver::Measure(col);
        Solver::Arrange(col, Rect(0.0f, 0.0f, 400.0f, 300.0f));

        // Outer size in parent units: (50 + 10) x (20 + 10) local -> 120 x 60
        CHECK_NEAR(fitBox->intrinsicSize.x, 120.0f, 0.001f);
        CHECK_NEAR(fitBox->intrinsicSize.y, 60.0f, 0.001f);
        // Own rect in local units, top-left shared with the parent
        CHECK_NEAR(fitBox->computedRect.x, 0.0f, 0.001f);
        CHECK_NEAR(fitBox->computedRect.y, 0.0f, 0.001f);
        CHECK_NEAR(fitBox->computedRect.w, 60.0f, 0.001f);
        CHECK_NEAR(fitBox->computedRect.h, 30.0f, 0.001f);
        CHECK_NEAR(leaf->computedRect.x, 5.0f, 0.001f);
        CHECK_NEAR(leaf->computedRect.y, 5.0f, 0.001f);
        CHECK_NEAR(leaf->computedRect.w, 50.0f, 0.001f);
        CHECK_NEAR(leaf->computedRect.h, 20.0f, 0.001f);

        // Stretch at 0.5x: 400 parent units -> 800 local, fixed 40 parent units -> 80 local; starts after 60 + 10
        CHECK_NEAR(wideBox->computedRect.y, 70.0f, 0.001f);
        CHECK_NEAR(wideBox->computedRect.w, 800.0f, 0.001f);
        CHECK_NEAR(wideBox->computedRect.h, 80.0f, 0.001f);
        CHECK_NEAR(wideBox->fixedHeight, 40.0f, 0.001f); // outer field restored in parent units

        // The column advances by each child's outer height
        CHECK_NEAR(after->computedRect.y, 70.0f + 40.0f + 10.0f, 0.001f);
    }
}

void TestPayloadTypeIdAndSolverMeasurePurity() {
    using namespace guikit;

    keel::Arena arena;

    // 1. Node payload type verification uses zero-RTTI keel::TypeId
    struct PayloadA { keel::i32 value{42}; };
    struct PayloadB { keel::f32 value{3.5f}; };

    Node* n = arena.create<Node>();
    auto* a = arena.create<PayloadA>();
    n->SetPayload(a);
    CHECK(n->As<PayloadA>() == a);
    CHECK(n->As<PayloadB>() == nullptr);
    n->SetPayload<PayloadA>(nullptr);
    CHECK(n->As<PayloadA>() == nullptr);

    // 2. Solver::Measure must remain pure (never mutating visual styling like cornerRadius/borderWidth);
    //    Solver::ResolveShellStyles applies shell panel styling explicitly before Measure.
    UiTheme floatingTheme = UiTheme::Dark();
    floatingTheme.shellStyle = ShellStyle::Floating;
    floatingTheme.shell.panelRadius = 10.0f;
    floatingTheme.shell.panelBorderWidth = 1.5f;
    floatingTheme.shell.panelGap = 8.0f;

    Node* shellRoot = arena.create<Node>();
    shellRoot->type = NodeType::Row;
    shellRoot->direction = FlexDirection::Row;
    shellRoot->gap = 0.0f;

    Node* sb = arena.create<Node>();
    sb->type = NodeType::Sidebar;
    sb->widthMode = SizeMode::Fixed;
    sb->fixedWidth = 200.0f;
    shellRoot->AddChild(sb);

    Node* mainCard = arena.create<Node>();
    mainCard->type = NodeType::Card;
    mainCard->backgroundColor = Color(30, 35, 45, 255);
    mainCard->cornerRadius = 0.0f;
    mainCard->borderWidth = 0.0f;
    shellRoot->AddChild(mainCard);

    Solver::Measure(shellRoot, nullptr, floatingTheme);
    CHECK_NEAR(mainCard->cornerRadius, 0.0f, 0.001f);
    CHECK_NEAR(mainCard->borderWidth, 0.0f, 0.001f);
    CHECK_NEAR(shellRoot->gap, 0.0f, 0.001f);

    Solver::ResolveShellStyles(shellRoot, floatingTheme);
    CHECK_NEAR(mainCard->cornerRadius, 10.0f, 0.001f);
    CHECK_NEAR(mainCard->borderWidth, 1.5f, 0.001f);
    CHECK_NEAR(shellRoot->gap, 8.0f, 0.001f);
}

void TestRowAndColumnJustifySpaceBetween() {
    using namespace guikit;

    keel::Arena arena;

    // 1. Row SpaceBetween: width=600, 3 fixed children of width 100 -> x = 0, 250, 500
    {
        Node* row = arena.create<Node>();
        row->direction = FlexDirection::Row;
        row->widthMode = SizeMode::Stretch;
        row->heightMode = SizeMode::Stretch;
        row->justify = Justify::SpaceBetween;
        row->gap = 10.0f;

        Node* a = arena.create<Node>();
        a->widthMode = SizeMode::Fixed;
        a->fixedWidth = 100.0f;
        a->heightMode = SizeMode::Fixed;
        a->fixedHeight = 40.0f;
        row->AddChild(a);

        Node* b = arena.create<Node>();
        b->widthMode = SizeMode::Fixed;
        b->fixedWidth = 100.0f;
        b->heightMode = SizeMode::Fixed;
        b->fixedHeight = 40.0f;
        row->AddChild(b);

        Node* c = arena.create<Node>();
        c->widthMode = SizeMode::Fixed;
        c->fixedWidth = 100.0f;
        c->heightMode = SizeMode::Fixed;
        c->fixedHeight = 40.0f;
        row->AddChild(c);

        Solver::Measure(row);
        Solver::Arrange(row, Rect(0.0f, 0.0f, 600.0f, 100.0f));

        CHECK_NEAR(a->computedRect.x, 0.0f, 0.001f);
        CHECK_NEAR(b->computedRect.x, 250.0f, 0.001f);
        CHECK_NEAR(c->computedRect.x, 500.0f, 0.001f);
        CHECK_NEAR(c->computedRect.Right(), 600.0f, 0.001f);
    }

    // 2. Column SpaceBetween: height=400, 3 fixed children of height 50 -> y = 0, 175, 350
    {
        Node* col = arena.create<Node>();
        col->direction = FlexDirection::Column;
        col->widthMode = SizeMode::Stretch;
        col->heightMode = SizeMode::Stretch;
        col->justify = Justify::SpaceBetween;
        col->gap = 8.0f;

        Node* a = arena.create<Node>();
        a->widthMode = SizeMode::Fixed;
        a->fixedWidth = 120.0f;
        a->heightMode = SizeMode::Fixed;
        a->fixedHeight = 50.0f;
        col->AddChild(a);

        Node* b = arena.create<Node>();
        b->widthMode = SizeMode::Fixed;
        b->fixedWidth = 120.0f;
        b->heightMode = SizeMode::Fixed;
        b->fixedHeight = 50.0f;
        col->AddChild(b);

        Node* c = arena.create<Node>();
        c->widthMode = SizeMode::Fixed;
        c->fixedWidth = 120.0f;
        c->heightMode = SizeMode::Fixed;
        c->fixedHeight = 50.0f;
        col->AddChild(c);

        Solver::Measure(col);
        Solver::Arrange(col, Rect(0.0f, 0.0f, 200.0f, 400.0f));

        CHECK_NEAR(a->computedRect.y, 0.0f, 0.001f);
        CHECK_NEAR(b->computedRect.y, 175.0f, 0.001f);
        CHECK_NEAR(c->computedRect.y, 350.0f, 0.001f);
        CHECK_NEAR(c->computedRect.Bottom(), 400.0f, 0.001f);
    }

    // 3. Single child with SpaceBetween stays pinned at Start without divide-by-zero
    {
        Node* row = arena.create<Node>();
        row->direction = FlexDirection::Row;
        row->widthMode = SizeMode::Stretch;
        row->heightMode = SizeMode::Stretch;
        row->justify = Justify::SpaceBetween;

        Node* solo = arena.create<Node>();
        solo->widthMode = SizeMode::Fixed;
        solo->fixedWidth = 80.0f;
        solo->heightMode = SizeMode::Fixed;
        solo->fixedHeight = 40.0f;
        row->AddChild(solo);

        Solver::Measure(row);
        Solver::Arrange(row, Rect(0.0f, 0.0f, 400.0f, 80.0f));
        CHECK_NEAR(solo->computedRect.x, 0.0f, 0.001f);
    }
}

void TestGridZeroRunGapIndependentOfColumnGap() {
    using namespace guikit;

    keel::Arena arena;

    // Grid width 600, gap = 12, explicit runGap = 0 -> horizontal gap is 12, vertical row gap must stay 0!
    Node* grid = arena.create<Node>();
    grid->direction = FlexDirection::Grid;
    grid->SetPayload(arena.create<GridData>(GridData{.columns = 12}));
    grid->gap = 12.0f;
    grid->runGap = 0.0f;
    grid->widthMode = SizeMode::Fixed;
    grid->fixedWidth = 600.0f;
    grid->heightMode = SizeMode::Fit;

    Node* r1a = arena.create<Node>();
    r1a->colSpan = 6;
    r1a->heightMode = SizeMode::Fixed;
    r1a->fixedHeight = 40.0f;
    grid->AddChild(r1a);

    Node* r1b = arena.create<Node>();
    r1b->colSpan = 6;
    r1b->heightMode = SizeMode::Fixed;
    r1b->fixedHeight = 40.0f;
    grid->AddChild(r1b);

    Node* r2 = arena.create<Node>();
    r2->colSpan = 12;
    r2->heightMode = SizeMode::Fixed;
    r2->fixedHeight = 50.0f;
    grid->AddChild(r2);

    Solver::Measure(grid);
    Solver::Arrange(grid, Rect(0.0f, 0.0f, 600.0f, 0.0f));

    // Row 1: two 6-col cells separated by column gap = 12px
    CHECK_NEAR(r1b->computedRect.x - r1a->computedRect.Right(), 12.0f, 0.001f);
    // Row 2: directly flush underneath Row 1 with 0px vertical gap
    CHECK_NEAR(r2->computedRect.y, 40.0f, 0.001f);
    CHECK_NEAR(grid->computedRect.h, 90.0f, 0.001f);
}

void TestDockLayoutTreeAndSerialization() {
    using namespace guikit;

    DockLayout layout;
    CHECK(layout.IsEmpty());

    // 1. Build IDE layout via DockPanel across all sides (Center, Left, Right, Top, Bottom)
    CHECK(layout.DockPanel("editor"));
    CHECK(layout.DockPanel("preview", DockSide::Center, "editor"));
    CHECK(layout.DockPanel("files", DockSide::Left, "editor", 0.22f));
    CHECK(layout.DockPanel("log", DockSide::Bottom, "editor", 0.28f));
    CHECK(layout.DockPanel("terminal", DockSide::Center, "log"));
    CHECK(layout.DockPanel("inspector", DockSide::Right, "editor", 0.25f));
    CHECK(layout.DockPanel("header", DockSide::Top, "editor", 0.20f));

    CHECK(!layout.IsEmpty());
    CHECK(layout.HasDockedPanel("files"));
    CHECK(layout.HasDockedPanel("editor"));
    CHECK(layout.HasDockedPanel("preview"));
    CHECK(layout.HasDockedPanel("log"));
    CHECK(layout.HasDockedPanel("terminal"));
    CHECK(layout.HasDockedPanel("inspector"));
    CHECK(layout.HasDockedPanel("header"));

    // 2. Reorder tab inside a multi-tab group while preserving active panel
    CHECK(layout.SetActivePanel("editor"));
    CHECK(layout.IsPanelActive("editor"));
    CHECK(!layout.IsPanelActive("preview"));
    const keel::u32 edNode = layout.FindPanelNode("editor");
    CHECK(edNode != kDockInvalidNode);
    CHECK(layout.ReorderTab(edNode, 0u, 1u));
    CHECK(layout.IsPanelActive("editor"));
    CHECK(layout.Nodes()[edNode].panelIds[1].as_ref() == "editor");

    // 2b. Atomic MovePanelToNode with explicit insertIndex (insert at index 0)
    CHECK(layout.MovePanelToNode("terminal", edNode, DockSide::Center, 0.5f, 0u));
    const keel::u32 edNode2 = layout.FindPanelNode("editor");
    CHECK(edNode2 != kDockInvalidNode);
    CHECK(layout.Nodes()[edNode2].panelIds[0].as_ref() == "terminal");
    CHECK(layout.IsPanelActive("terminal"));
    // Move terminal back to log group so subsequent checks match
    CHECK(layout.DockPanel("terminal", DockSide::Center, "log"));
    CHECK(layout.SetActivePanel("editor"));

    // 3. RemovePanel collapses orphaned Split ancestors and compacts node pool
    CHECK(layout.RemovePanel("header"));
    CHECK(!layout.HasPanel("header"));
    CHECK(layout.RemovePanel("inspector"));
    CHECK(!layout.HasPanel("inspector"));
    CHECK(layout.HasDockedPanel("editor"));

    // 4. FloatPanel & RedockPanel remember prior dock location
    CHECK(layout.FloatPanel("preview", Rect(120.0f, 60.0f, 340.0f, 240.0f)));
    CHECK(layout.IsPanelFloating("preview"));
    CHECK(!layout.HasDockedPanel("preview"));
    CHECK(layout.IsPanelActive("preview"));

    // 5. ClosePanel & ReopenPanel
    CHECK(layout.ClosePanel("terminal"));
    CHECK(layout.IsPanelClosed("terminal"));
    CHECK(!layout.HasPanel("terminal"));
    CHECK(layout.ReopenPanel("terminal"));
    CHECK(!layout.IsPanelClosed("terminal"));
    CHECK(layout.HasDockedPanel("terminal"));

    // 6. Round-trip SaveLayout / LoadLayout (D2 with floating card)
    const keel::String savedWithFloat = layout.SaveLayout();
    CHECK(savedWithFloat.starts_with("D2("));
    CHECK(savedWithFloat.contains(";F(preview,"));

    DockLayout restored;
    CHECK(restored.LoadLayout(savedWithFloat.as_ref()));
    CHECK(restored.HasDockedPanel("files"));
    CHECK(restored.HasDockedPanel("editor"));
    CHECK(restored.HasDockedPanel("log"));
    CHECK(restored.HasDockedPanel("terminal"));
    CHECK(restored.IsPanelFloating("preview"));
    const DockFloatCard* rf = restored.FindFloatCard("preview");
    CHECK(rf != nullptr);
    if (rf) {
        CHECK_NEAR(rf->bounds.x, 120.0f, 0.5f);
        CHECK_NEAR(rf->bounds.y, 60.0f, 0.5f);
        CHECK_NEAR(rf->bounds.w, 340.0f, 0.5f);
        CHECK_NEAR(rf->bounds.h, 240.0f, 0.5f);
    }

    // 7. Redock floating card and verify plain-tree serialization format
    CHECK(layout.RedockPanel("preview"));
    CHECK(!layout.IsPanelFloating("preview"));
    CHECK(layout.HasDockedPanel("preview"));
    const keel::String savedPlain = layout.SaveLayout();
    CHECK(savedPlain.starts_with("H(") || savedPlain.starts_with("V(") || savedPlain.starts_with("T("));
    DockLayout restoredPlain;
    CHECK(restoredPlain.LoadLayout(savedPlain.as_ref()));
    CHECK(restoredPlain.SaveLayout() == savedPlain);

    // 8. Parse explicit spec example strings
    DockLayout specTree;
    CHECK(specTree.LoadLayout("H(0.22,T(0;files),V(0.72,T(0;editor,preview),T(0;log,terminal)))"));
    CHECK(specTree.HasDockedPanel("files"));
    CHECK(specTree.IsPanelActive("editor"));
    CHECK(!specTree.IsPanelActive("preview"));
    CHECK(specTree.IsPanelActive("log"));

    DockLayout specFloat;
    CHECK(specFloat.LoadLayout("D2(H(0.3,T(0;files),T(0;editor));F(inspector,420,80,320,260))"));
    CHECK(specFloat.HasDockedPanel("files"));
    CHECK(specFloat.HasDockedPanel("editor"));
    CHECK(specFloat.IsPanelFloating("inspector"));

    // 9. Invalid strings must fail without corrupting existing layout
    const keel::String beforeBad = specFloat.SaveLayout();
    CHECK(!specFloat.LoadLayout(""));
    CHECK(!specFloat.LoadLayout("H(0.5,T(0;a))"));                                 // missing 2nd child
    CHECK(!specFloat.LoadLayout("H(1.5,T(0;a),T(0;b))"));                          // ratio >= 1
    CHECK(!specFloat.LoadLayout("H(0.0,T(0;a),T(0;b))"));                          // ratio <= 0
    CHECK(!specFloat.LoadLayout("T(2;a,b)"));                                      // activeTab out of range
    CHECK(!specFloat.LoadLayout("H(0.5,T(0;dup),T(0;dup))"));                      // duplicate panel ID
    CHECK(!specFloat.LoadLayout("D2(T(0;dup);F(dup,10,10,200,200))"));             // duplicate across dock/float
    CHECK(!specFloat.LoadLayout("D2(T(0;a);F(b,10,10,-50,200))"));                 // negative width
    CHECK(!specFloat.LoadLayout("T(0;a) trailing"));                               // trailing garbage
    CHECK(specFloat.SaveLayout() == beforeBad);
}

int main() {
    keel::println("[RUN] Testing GuiKit Layout Solver...");

    TestRowEqualStretch();
    TestScaledNodeLaysOutInLocalUnits();
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
    TestCarouselArrowScaling();
    TestAspectRatioMinHeightAndStretchFallback();
    TestRowEqualWidthsFitAndStretch();
    TestRowTextDeficitShrinkAndCenterClamp();
    TestScrollAreaMaxHeightWrapFitAndDeepTreeGuard();
    TestGapDpAndContainerSizingConstraints();
    TestPayloadTypeIdAndSolverMeasurePurity();
    TestRowAndColumnJustifySpaceBetween();
    TestGridZeroRunGapIndependentOfColumnGap();
    TestDockLayoutTreeAndSerialization();

    if (g_failures == 0) {
        keel::println("[PASS] All GuiKit Layout tests passed successfully!");
        return 0;
    } else {
        keel::eprintln("[FAIL] Total failures: {}", g_failures);
        return 1;
    }
}
