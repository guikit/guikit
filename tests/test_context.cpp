#include <iostream>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/backend/SdlCanvas.hpp"
#include "guikit/backend/ThorCanvas.hpp"
#include "thorvg.h"

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

void TestImmediateApiWorkflow() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse b1;
    WidgetResponse b2;

    auto renderUI = [&]() {
        if (auto row = ui.Row()) {
            b1 = ui.Button({.label = "Button A"});
            ui.Spacer();
            b2 = ui.Button({.label = "Button B"});
        }
    };

    // Frame 1: Initial layout establishment
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    CHECK(!b1.clicked);
    CHECK(!b2.clicked);
    ui.EndFrame(canvas);

    // Frame 2: Hover over Button A (Button A is at x=0, y=0, w ~ 70+, h ~ 36)
    ui.OnMouseMove({20.0f, 15.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    CHECK(b1.hovered);
    CHECK(!b2.hovered);
    CHECK(!b1.clicked);
    ui.EndFrame(canvas);

    // Frame 3: Mouse down on Button A
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    CHECK(b1.pressed);
    CHECK(!b1.clicked);
    ui.EndFrame(canvas);

    // Frame 4: Mouse up on Button A -> Triggers .clicked == true!
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    CHECK(b1.clicked);
    CHECK(bool(b1)); // Implicit bool conversion works
    CHECK(!b2.clicked);
    ui.EndFrame(canvas);

    // Frame 5: Mouse button is up, click event is consumed
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    CHECK(!b1.clicked);
    ui.EndFrame(canvas);
}

void TestThorCanvasTextRendering() {
    using namespace guikit;
    ThorCanvas canvas;
    const uint32_t width = 200;
    const uint32_t height = 100;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    bool fontOk = canvas.LoadFontFile("assets/fonts/Roboto-Regular.ttf");
    if (!fontOk) {
        fontOk = canvas.LoadFontFile("../assets/fonts/Roboto-Regular.ttf");
    }
    CHECK(fontOk);

    std::fill(pixels.begin(), pixels.end(), 0);
    canvas.Begin(width, height);

    // Direct ThorVG test with layout box:
    auto txtBox = tvg::Text::gen();
    txtBox->text("Overview");
    txtBox->font(nullptr);
    txtBox->size(14.0f);
    txtBox->layout(120.0f, 36.0f);
    txtBox->align(0.5f, 0.5f);
    txtBox->translate(10.0f, 10.0f);
    txtBox->fill(255, 255, 255);
    // Add to canvas directly or via ThorCanvas
    // We can test this!

    int minY = 999, maxY = -1, minX = 999, maxX = -1;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            if (pixels[y * width + x] != 0) {
                if ((int)y < minY) minY = (int)y;
                if ((int)y > maxY) maxY = (int)y;
                if ((int)x < minX) minX = (int)x;
                if ((int)x > maxX) maxX = (int)x;
            }
        }
    }
    std::cout << "[PIXEL BOUNDS] pos=(10, 10), fontSize=24 -> X=[" << minX << ".." << maxX 
              << "], Y=[" << minY << ".." << maxY << "], width=" << (maxX - minX + 1)
              << ", height=" << (maxY - minY + 1) << "\n";
    CHECK(minX >= 0 && minY >= 0);
}

void TestHeaderRowTextMetricsAndNoOverlap() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    // Accurate text advance verification
    Vec2 titleMetrics = canvas.MeasureText("GUIKIT GUI", 24.0f);
    CHECK(titleMetrics.x > 150.0f);

    Vec2 subMetrics = canvas.MeasureText("| Autonomous Vector GUI Engine", 14.0f);
    CHECK(subMetrics.x > 250.0f);

    WidgetResponse bOverview;
    WidgetResponse bFlex;

    auto renderRow = [&]() {
        if (auto row = ui.Row({.gap = UiSize::Sm})) {
            if (auto b1 = ui.Box({.fixedWidth = 120.0f})) {
                bOverview = ui.Button("Overview");
            }
            if (auto b2 = ui.Box({.fixedWidth = 150.0f})) {
                bFlex = ui.Button("Flexbox Demo");
            }
        }
    };

    // Frame 1: Measure and establish bounds
    ui.BeginFrame({800.0f, 600.0f});
    renderRow();
    ui.EndFrame(canvas);

    // Frame 2: Retrieve cached bounds from previous frame
    ui.BeginFrame({800.0f, 600.0f});
    renderRow();
    ui.EndFrame(canvas);

    CHECK_NEAR(bOverview.rect.x, 0.0f, 0.001f);
    CHECK_NEAR(bOverview.rect.w, 120.0f, 0.001f);
    CHECK_NEAR(bFlex.rect.x, 128.0f, 0.001f);
    CHECK_NEAR(bFlex.rect.w, 150.0f, 0.001f);
    CHECK(bFlex.rect.x >= bOverview.rect.Right()); // Strictly sequential with zero overlap
}

void TestFooterExitButtonStaysVisibleInNarrowWindow() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 500;
    const uint32_t height = 400;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse exitBtn;
    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = UiSize::Lg, .padding = Padding::All(24.0f)})) {
            // Wide header (requires ~600px+)
            if (auto header = ui.Row({.gap = UiSize::Md})) {
                ui.Title("GUIKIT GUI");
                ui.Caption("| Autonomous Vector GUI Engine");
                ui.Spacer();
                ui.Caption("C++20 Immediate + ThorVG");
            }

            ui.Spacer();

            // Footer with Exit button pushed to right
            if (auto footer = ui.Row({.gap = UiSize::Md})) {
                ui.Caption("Zero heap allocations per frame | 0% idle CPU");
                ui.Spacer();
                if (auto b = ui.Box({.fixedWidth = 80.0f})) {
                    exitBtn = ui.Button("Exit");
                }
            }
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // The Exit button must stay strictly within the 500px window!
    CHECK(exitBtn.rect.Right() <= 500.0f);
    // Specifically, it should align with the column's right padding: 500 - 24 = 476.0f
    CHECK_NEAR(exitBtn.rect.Right(), 476.0f, 1.0f);
    // And vertically, it must be pushed to the bottom margin by Spacer: 400 - 24 = 376.0f
    CHECK_NEAR(exitBtn.rect.Bottom(), 376.0f, 1.0f);
}

void TestWrapContainerInContextApi() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const uint32_t width = 300;
    const uint32_t height = 400;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse b1;
    WidgetResponse b2;
    WidgetResponse b3;

    ui.GetStyle().Spacing().md = 10.0f;
    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = UiSize::Md, .padding = Padding::All(20.0f)})) {
            // Available width inside padding = 300 - 40 = 260.
            // b1 (fixed 120), b2 (fixed 120), b3 (fixed 120)
            // b1 (120) + gap (10) + b2 (120) = 250 <= 260 (Line 1)
            // b3 wraps to Line 2! (y = 20 + 36 + 10 = 66)
            if (auto wrap = ui.Wrap({.gap = UiSize::Md, .runGap = UiSize::Md})) {
                if (auto box1 = ui.Box({.fixedWidth = 120.0f, .fixedHeight = 36.0f})) {
                    b1 = ui.Button("Button 1");
                }
                if (auto box2 = ui.Box({.fixedWidth = 120.0f, .fixedHeight = 36.0f})) {
                    b2 = ui.Button("Button 2");
                }
                if (auto box3 = ui.Box({.fixedWidth = 120.0f, .fixedHeight = 36.0f})) {
                    b3 = ui.Button("Button 3");
                }
            }
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Frame 2: inspect bounds
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK_NEAR(b1.rect.x, 20.0f, 0.001f);
    CHECK_NEAR(b1.rect.y, 20.0f, 0.001f);
    CHECK_NEAR(b2.rect.x, 150.0f, 0.001f); // 20 + 120 + 10
    CHECK_NEAR(b2.rect.y, 20.0f, 0.001f);

    // b3 must be wrapped to second line
    CHECK_NEAR(b3.rect.x, 20.0f, 0.001f);
    CHECK_NEAR(b3.rect.y, 66.0f, 0.001f); // 20 + 36 + 10
    CHECK_NEAR(b3.rect.w, 120.0f, 0.001f);
    CHECK_NEAR(b3.rect.h, 36.0f, 0.001f);
}

void TestFooterExitButtonPushedToBottomWithCardAndWrap() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse exitBtn;
    auto renderUI = [&]() {
        if (auto mainCol = ui.Column({.gap = UiSize::Lg, .padding = Padding::All(24.0f)})) {
            if (auto header = ui.Row({.gap = UiSize::Md})) {
                ui.Title("GUIKIT GUI");
                ui.Spacer();
            }
            if (auto navRow = ui.Row({.gap = UiSize::Sm})) {
                if (auto box = ui.Box({.fixedWidth = 120.0f})) {
                    ui.Button("Overview");
                }
            }
            if (auto card = ui.Box({.gap = UiSize::Lg, .padding = Padding::All(20.0f)})) {
                ui.Headline("Card title");
                if (auto wrap = ui.Wrap({.gap = UiSize::Md, .runGap = UiSize::Sm})) {
                    ui.Button("Btn 1");
                    ui.Button("Btn 2");
                }
            }
            ui.Spacer();
            if (auto footer = ui.Row({.gap = UiSize::Md})) {
                ui.Caption("Footer text");
                ui.Spacer();
                if (auto box = ui.Box({.fixedWidth = 80.0f})) {
                    exitBtn = ui.Button("Exit");
                }
            }
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK_NEAR(exitBtn.rect.Bottom(), 600.0f - 24.0f, 1.0f);
}

void TestUiTokensAndIconFactories() {
    using namespace guikit;

    const auto theme = UiTheme::Dark();
    // Verify 5-level scale metrics
    const auto mXs = theme.GetMetrics(UiSize::Xs);
    const auto mSm = theme.GetMetrics(UiSize::Sm);
    const auto mMd = theme.GetMetrics(UiSize::Md);
    const auto mLg = theme.GetMetrics(UiSize::Lg);
    const auto mXl = theme.GetMetrics(UiSize::Xl);

    CHECK(mXs.defaultHeight < mSm.defaultHeight);
    CHECK(mSm.defaultHeight < mMd.defaultHeight);
    CHECK(mMd.defaultHeight < mLg.defaultHeight);
    CHECK(mLg.defaultHeight < mXl.defaultHeight);

    CHECK_NEAR(mMd.defaultHeight, 40.0f, 0.001f);
    CHECK_NEAR(mLg.defaultHeight, 48.0f, 0.001f);

    // Verify Icon value object
    const Icon icPlay = Icon::Play();
    const Icon icClose = Icon::Close();
    CHECK(icPlay.IsValid());
    CHECK(icPlay.GetId() == IconId::Play);
    CHECK(icPlay != icClose);

    const Icon emptyIcon;
    CHECK(emptyIcon.IsEmpty());
    CHECK(!emptyIcon);

    // Verify Icon drawing does not crash
    ThorCanvas canvas;
    const uint32_t width = 100;
    const uint32_t height = 100;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);
    canvas.Begin(width, height);
    icPlay.Draw(canvas, Rect(10.0f, 10.0f, 24.0f, 24.0f), Color::Hex(0xFFFFFFFF));
    icClose.Draw(canvas, Rect(40.0f, 10.0f, 24.0f, 24.0f), Color::Hex(0xFFFFFFFF));
    canvas.End();
}

void TestTwoLevelDisabledWorkflow() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 400;
    const uint32_t height = 200;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse btnEnabled;
    WidgetResponse btnDirectDisabled;
    WidgetResponse btnScopedDisabled;

    auto renderUI = [&]() {
        if (auto row = ui.Row()) {
            btnEnabled = ui.Button("Active");
            btnDirectDisabled = ui.Button("Local Disabled", {.disabled = true});

            ui.Disabled(true, [&] {
                btnScopedDisabled = ui.Button("Scope Disabled");
            });
        }
    };

    // Frame 1: Layout establishment
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Frame 2: Mouse click over the disabled button
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    ui.OnMouseMove(btnDirectDisabled.rect.Center());
    ui.OnMouseDown(1);
    ui.OnMouseUp(1);
    renderUI();
    ui.EndFrame(canvas);

    CHECK(!btnDirectDisabled.clicked);
    CHECK(!btnDirectDisabled.hovered);

    // Click over scoped disabled button
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    ui.OnMouseMove(btnScopedDisabled.rect.Center());
    ui.OnMouseDown(1);
    ui.OnMouseUp(1);
    renderUI();
    ui.EndFrame(canvas);

    CHECK(!btnScopedDisabled.clicked);
    CHECK(!btnScopedDisabled.hovered);

    // Click over active button
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    ui.OnMouseMove(btnEnabled.rect.Center());
    ui.OnMouseDown(1);
    renderUI();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    ui.OnMouseUp(1);
    renderUI();
    ui.EndFrame(canvas);

    CHECK(btnEnabled.clicked);
}

void TestBoxSlotWithUiSizeToken() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 400;
    const uint32_t height = 200;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse btnLarge;
    WidgetResponse btnSquareIcon;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = UiSize::Md})) {
            if (auto slotLg = ui.Box({.size = UiSize::Lg})) {
                btnLarge = ui.Button("Large Action", {.variant = UiVariant::Primary});
            }
            if (auto slotSquare = ui.Box({.fixedWidth = 32.0f, .size = UiSize::Sm})) {
                btnSquareIcon = ui.Button({.icon = Icon::Close()});
            }
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Large button must fill the 48px slot allocated by UiSize::Lg
    CHECK_NEAR(btnLarge.rect.h, 48.0f, 0.001f);
    CHECK_NEAR(btnLarge.rect.w, 400.0f, 0.001f); // Stretches across column

    // Square icon button in 32x32 slot
    CHECK_NEAR(btnSquareIcon.rect.w, 32.0f, 0.001f);
    CHECK_NEAR(btnSquareIcon.rect.h, 32.0f, 0.001f);
}

void TestButtonWidgetRunAndResponseChaining() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 400;
    const uint32_t height = 200;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    ButtonResponse btn1;
    ButtonResponse btn2;

    auto renderUI = [&]() {
        if (auto row = ui.Row({.gap = UiSize::Sm})) {
            // Autonomous Button::Run via ui.Button
            btn1 = ui.Button({
                .label = "Next",
                .icon = Icon::ChevronRight(),
                .variant = UiVariant::Primary,
                .iconPlacement = Placement::Trailing
            });

            // Direct call to Button::Run
            btn2 = Button::Run(ui, {
                .label = "Prev",
                .icon = Icon::ChevronLeft(),
                .variant = UiVariant::Secondary,
                .iconPlacement = Placement::Leading
            });
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Verify IDs and geometry
    CHECK(btn1.id.IsValid());
    CHECK(btn2.id.IsValid());
    CHECK(btn1.id != btn2.id);
    CHECK(btn1.rect.w > 0.0f);
    CHECK(btn2.rect.w > 0.0f);

    // Test Response imperative actions
    btn1.Focus();

    // Verify Response conversion to bool
    CHECK(!btn1);
}

void TestUiStringAndContextFormatting() {
    using namespace guikit;

    // Test static literal assignment (0 copy)
    UiString s1 = "Static literal";
    CHECK(s1 == "Static literal");
    CHECK(s1.size() == 14);
    CHECK(!s1.empty());

    // Test inline buffer formatting
    UiString s2;
    s2.Format("Count: {}", 42);
    CHECK(s2 == "Count: 42");
    CHECK(s2.size() == 9);

    // Test factory formatting
    UiString s3 = UiString::FormatNew("Speed: {:.1f}", 12.5f);
    CHECK(s3 == "Speed: 12.5");

    // Test Context arena formatting and interning in UI frame
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 400;
    const uint32_t height = 200;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    if (auto col = ui.Column({.gap = UiSize::Sm})) {
        ui.Text(ui.Format("Value: {}", 99));
        ui.Button(ui.String("Action"));
    }
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    if (auto col = ui.Column({.gap = UiSize::Sm})) {
        ui.Text(ui.Format("Value: {}", 100));
        ui.Button(ui.String("Action"));
    }
    ui.EndFrame(canvas);
}

void TestBoxContainerGapSpacing() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 400;
    const uint32_t height = 300;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse b1;
    WidgetResponse b2;

    auto renderUI = [&]() {
        if (auto card = ui.Box({.gap = UiSize::Lg, .padding = Padding::All(10.0f)})) {
            b1 = ui.Button("First Item");
            b2 = ui.Button("Second Item");
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Verify Box applies 16px gap between children
    CHECK_NEAR(b2.rect.y, b1.rect.Bottom() + 16.0f, 0.01f);
}

void TestAutonomousTextWidgetAndTypographyRoles() {
    using namespace guikit;

    const auto theme = UiTheme::Dark();
    // 1. Verify intrinsic metrics and measurement for different roles
    const TypographyMetrics mDisplay = theme.GetTypography(TextRole::DisplayLarge);
    CHECK(mDisplay.fontSize == 40.0f);
    CHECK(mDisplay.lineHeight == 48.0f);
    CHECK(!mDisplay.isMonospace);

    const TypographyMetrics mBody = theme.GetTypography(TextRole::BodyMedium);
    CHECK(mBody.fontSize == 14.0f);
    CHECK(mBody.lineHeight == 20.0f);

    const TypographyMetrics mCode = theme.GetTypography(TextRole::CodeMedium);
    CHECK(mCode.fontSize == 13.0f);
    CHECK(mCode.lineHeight == 18.0f);
    CHECK(mCode.isMonospace);

    Vec2 estDisplay = Text::Measure("Heading", {.role = TextRole::DisplayLarge});
    Vec2 estBody = Text::Measure("Heading", {.role = TextRole::BodyMedium});
    CHECK(estDisplay.x > estBody.x);
    CHECK(estDisplay.y > estBody.y);

    // 2. Immediate mode rendering and semantic shorthands
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    Response rDisp;
    Response rHead;
    Response rTitle;
    Response rSub;
    Response rBody;
    Response rCap;
    Response rCode;
    Response rCustom;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = UiSize::Sm, .padding = Padding::All(16.0f)})) {
            rDisp = ui.Display("Display Title");
            rHead = ui.Headline("Headline Section", UiVariant::Primary);
            rTitle = ui.Title("Title Header");
            rSub = ui.Subtitle("Subtitle Header");
            rBody = ui.Body("Standard body text content");
            rCap = ui.Caption("Muted caption info");
            rCode = ui.Code("auto x = 42;");
            rCustom = ui.Text("Aligned End", {.role = TextRole::LabelLarge, .variant = UiVariant::Success, .align = Align::End});
        }
    };

    // Frame 1: Measure and layout
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Frame 2: Bounds populated and interaction check
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rDisp.rect.w > 0.0f);
    CHECK(rDisp.rect.h >= mDisplay.lineHeight);
    CHECK(rBody.rect.w > 0.0f);
    CHECK(rBody.rect.h >= mBody.lineHeight);
    CHECK(rDisp.rect.h > rBody.rect.h);
    CHECK(rCustom.rect.w > 0.0f);

    // 3. Test interaction: hover, press, and click on text widget
    ui.OnMouseMove({rDisp.rect.x + 5.0f, rDisp.rect.y + 5.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(rDisp.hovered);

    // Mouse press
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(rDisp.pressed);

    // Mouse release completes click
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(rDisp.clicked);
    CHECK(static_cast<bool>(rDisp) == true);
}

void TestSwitchAndCheckboxWidgets() {
    using namespace guikit;

    static_assert(sizeof(SwitchOptions) <= 40);
    static_assert(sizeof(CheckboxOptions) <= 40);

    const auto theme = UiTheme::Dark();
    // 1. Test measurement
    const SwitchMetrics sm = theme.GetSwitch(UiSize::Md);
    Vec2 sMeasNoLabel = Switch::Measure("", {}, UiSize::Md);
    CHECK(sMeasNoLabel.x == sm.trackSize.x);
    CHECK(sMeasNoLabel.y == sm.trackSize.y);

    Vec2 sMeasWithLabel = Switch::Measure("Enable", {}, UiSize::Md);
    CHECK(sMeasWithLabel.x > sm.trackSize.x);

    Vec2 sMeasWithDesc = Switch::Measure("Enable", {.description = "Details"}, UiSize::Md);
    CHECK(sMeasWithDesc.y > sMeasWithLabel.y);

    const CheckboxMetrics cm = theme.GetCheckbox(UiSize::Md);
    Vec2 cMeasNoLabel = Checkbox::Measure("", {}, UiSize::Md);
    CHECK(cMeasNoLabel.x == cm.boxSize);
    CHECK(cMeasNoLabel.y == cm.boxSize);

    Vec2 cMeasWithLabel = Checkbox::Measure("Enable", {}, UiSize::Md);
    CHECK(cMeasWithLabel.x > cm.boxSize);

    Vec2 cMeasWithDesc = Checkbox::Measure("Enable", {.description = "Details"}, UiSize::Md);
    CHECK(cMeasWithDesc.y > cMeasWithLabel.y);

    // 2. Interactive mutation test
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 500;
    const uint32_t height = 400;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    bool switchState = false;
    bool checkState = false;
    bool toggleState = false;
    bool descState = false;
    bool stackedState = false;
    bool optSwitchState = false;
    bool optCheckState = false;

    Response rSwitch;
    Response rCheck;
    Response rToggle;
    Response rIndeterminate;
    Response rDesc;
    Response rStacked;
    Response rOptSwitch;
    Response rOptCheck;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = UiSize::Md, .padding = Padding::All(16.0f)})) {
            rSwitch = ui.Switch("Dark Mode", &switchState);
            rCheck = ui.Checkbox("VSync", &checkState);
            rToggle = ui.Toggle("Turbo Mode", &toggleState, {.labelPlacement = Placement::Leading});
            rIndeterminate = ui.Checkbox("Select All", false, {.indeterminate = true});
            rDesc = ui.Switch("Auto Breakdown", &descState, {
                .description = "Automatic stop of testing on load drop"
            });
            rStacked = ui.Switch("Stacked Mode", &stackedState, {
                .description = "Description below switch control",
                .labelPlacement = Placement::Stacked
            });
            rOptSwitch = ui.Switch(&optSwitchState, {.label = "Option Switch"});
            rOptCheck = ui.Checkbox(&optCheckState, {.label = "Option Checkbox"});
        }
    };

    // Frame 1: Measure and layout
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Frame 2: Bounds populated
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rSwitch.rect.w > 0.0f);
    CHECK(rCheck.rect.w > 0.0f);
    CHECK(rToggle.rect.w > 0.0f);
    CHECK(rIndeterminate.rect.w > 0.0f);
    CHECK(rDesc.rect.w > 0.0f);
    CHECK(rDesc.rect.h > rSwitch.rect.h);
    CHECK(rStacked.rect.w > 0.0f);
    CHECK(rStacked.rect.h > rSwitch.rect.h);
    CHECK(!switchState);
    CHECK(!checkState);
    CHECK(!descState);
    CHECK(!stackedState);

    // 3. Click Switch to toggle value
    ui.OnMouseMove({rSwitch.rect.x + 5.0f, rSwitch.rect.y + 5.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(rSwitch.pressed);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rSwitch.clicked);
    CHECK(rSwitch.changed);
    CHECK(static_cast<bool>(rSwitch) == true);
    CHECK(switchState == true);

    // 4. Click Checkbox to toggle value
    ui.OnMouseMove({rCheck.rect.x + 5.0f, rCheck.rect.y + 5.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(rCheck.pressed);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rCheck.clicked);
    CHECK(rCheck.changed);
    CHECK(static_cast<bool>(rCheck) == true);
    CHECK(checkState == true);

    // 5. Click Switch with description to toggle value
    ui.OnMouseMove({rDesc.rect.x + 10.0f, rDesc.rect.y + 10.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(rDesc.pressed);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rDesc.clicked);
    CHECK(rDesc.changed);
    CHECK(static_cast<bool>(rDesc) == true);
    CHECK(descState == true);

    // 6. Click Stacked Switch to toggle value
    ui.OnMouseMove({rStacked.rect.x + 10.0f, rStacked.rect.y + 10.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(rStacked.pressed);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rStacked.clicked);
    CHECK(rStacked.changed);
    CHECK(static_cast<bool>(rStacked) == true);
    CHECK(stackedState == true);

    // 7. Click Option Switch (label configured via SwitchOptions)
    CHECK(rOptSwitch.rect.w > 0.0f);
    CHECK(!optSwitchState);
    ui.OnMouseMove({rOptSwitch.rect.x + 5.0f, rOptSwitch.rect.y + 5.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(optSwitchState == true);
    CHECK(rOptSwitch.changed);

    // 8. Click Option Checkbox (label configured via CheckboxOptions)
    CHECK(rOptCheck.rect.w > 0.0f);
    CHECK(!optCheckState);
    ui.OnMouseMove({rOptCheck.rect.x + 5.0f, rOptCheck.rect.y + 5.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(optCheckState == true);
    CHECK(rOptCheck.changed);
}

void TestInputFieldWidget() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const uint32_t width = 600;
    const uint32_t height = 500;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    float speedDrop = 50.0f;
    int count = 42;
    std::string sampleName = "Specimen-A";
    float optInputVal = 12.3f;

    Response rSpeed;
    Response rCount;
    Response rName;
    Response rOptInput;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = UiSize::Lg, .padding = Padding::All(20.0f)})) {
            // Numeric field with unit and description (as in user screenshot)
            rSpeed = ui.InputField("Speed Drop dF/dt", speedDrop, {
                .description = "Automatic stop of testing on load drop",
                .suffix = "%",
                .format = "%.0f",
                .labelPlacement = Placement::Stacked
            });

            // Integer count with unit suffix
            rCount = ui.InputField("Cycle Count", count, {
                .suffix = "cycles",
                .labelPlacement = Placement::Stacked
            });

            // Text input with placeholder
            rName = ui.InputField("Sample ID", sampleName, {
                .placeholder = "Enter identifier...",
                .labelPlacement = Placement::Stacked
            });

            // Field with label set exclusively via InputFieldOptions
            rOptInput = ui.InputField(optInputVal, {
                .label = "Tolerance Margin",
                .suffix = "mm",
                .labelPlacement = Placement::Stacked
            });
        }
    };

    // Frame 1: Measure & Layout
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Frame 2: Bounds populated
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rSpeed.rect.w > 0.0f);
    CHECK(rSpeed.rect.h > 36.0f); // Default box is 36px, stacked adds label and description
    CHECK(rCount.rect.w > 0.0f);
    CHECK(rName.rect.w > 0.0f);
    CHECK(rOptInput.rect.w > 0.0f);
    CHECK(rOptInput.rect.h > 36.0f);
    CHECK(!rSpeed.focused);

    // 1. Click speed field to focus
    ui.OnMouseMove({rSpeed.rect.x + 15.0f, rSpeed.rect.y + 25.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rSpeed.focused);
    CHECK(ui.IsFocused(rSpeed.id));

    // 2. Type text into focused speed field: "50" -> backspace -> "5" -> insert "5" -> "55"
    ui.OnKeyDown(Key::Backspace);
    ui.OnTextInput("5");
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rSpeed.changed);
    CHECK_NEAR(speedDrop, 55.0f, 0.01f);

    // 3. Press Enter to submit and blur
    ui.OnKeyDown(Key::Enter);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rSpeed.submitted);
    CHECK(!rSpeed.focused);
    CHECK(!ui.IsFocused(rSpeed.id));

    // 4. Click name field and edit string
    ui.OnMouseMove({rName.rect.x + 15.0f, rName.rect.y + 25.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rName.focused);

    ui.OnTextInput("-X");
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rName.changed);
    CHECK(sampleName == "Specimen-A-X");

    // 5. Click outside (e.g. at 5, 5 in padding) to blur focus
    ui.OnMouseMove({5.0f, 5.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(!rName.focused);
    CHECK(!ui.IsFocused(rName.id));
}

void TestSelectWidget() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    size_t baudIndex = 1; // "19200"
    int modeIndex = 0;   // "Auto"
    std::string profile = "Standard";
    bool disabledMode = false;

    WidgetResponse rBaud;
    WidgetResponse rMode;
    WidgetResponse rProfile;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = UiSize::Md, .padding = Padding::All(20.0f)})) {
            rBaud = ui.Select("Baud Rate", baudIndex, {"9600", "19200", "38400", "115200"}, {
                .description = "Serial bus communication speed",
                .labelPlacement = Placement::Stacked
            });

            rMode = ui.Select("Control Mode", modeIndex, {"Auto", "Manual", "Direct"}, {
                .disabled = disabledMode
            });

            rProfile = ui.Select("Profile", profile, {"Eco", "Standard", "Performance", "Extreme"});
        }
    };

    // Frame 1: Measure & Layout pass
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Frame 2: Bounds populated
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rBaud.rect.w > 0.0f);
    CHECK(rBaud.rect.h > 36.0f); // Stacked adds label and description
    CHECK(rMode.rect.w > 0.0f);
    CHECK(rProfile.rect.w > 0.0f);
    CHECK(!ui.IsPopupOpen(rBaud.id));
    CHECK(!ui.IsPopupOpen(rMode.id));
    CHECK(!ui.IsPopupOpen(rProfile.id));

    // 1. Click Baud Rate select box to open dropdown popup
    ui.OnMouseMove({rBaud.rect.x + 20.0f, rBaud.rect.y + 25.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(ui.IsPopupOpen(rBaud.id));
    Rect popupRect = ui.GetOpenPopupRect();
    CHECK(popupRect.w > 0.0f);
    CHECK(popupRect.h > 0.0f);

    // 2. Hover over item 3 ("115200") in the popup and click it
    // Item 3 rect is at popupRect.y + 4.0f + 3 * itemHeight (32px)
    const float item3Y = popupRect.y + 4.0f + 3.0f * 32.0f + 10.0f;
    ui.OnMouseMove({popupRect.x + 30.0f, item3Y});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rBaud.changed);
    CHECK(baudIndex == 3);
    CHECK(!ui.IsPopupOpen(rBaud.id)); // Popup is now closed

    // 3. Toggle open and close on clicking the select box again
    ui.OnMouseMove({rBaud.rect.x + 20.0f, rBaud.rect.y + 25.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(ui.IsPopupOpen(rBaud.id));

    // Click select box again
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(!ui.IsPopupOpen(rBaud.id)); // Closed on toggle

    // 4. Open popup and close via Escape key
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(ui.IsPopupOpen(rBaud.id));

    ui.OnKeyDown(Key::Escape);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(!ui.IsPopupOpen(rBaud.id)); // Closed on Escape

    // 5. Open popup and close via outside click
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(ui.IsPopupOpen(rBaud.id));

    // Click at outside position (5, 5)
    ui.OnMouseMove({5.0f, 5.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(!ui.IsPopupOpen(rBaud.id)); // Closed on outside click

    // 6. Keyboard navigation with Down / Up / Enter
    ui.OnMouseMove({rBaud.rect.x + 20.0f, rBaud.rect.y + 25.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(ui.IsPopupOpen(rBaud.id));

    // Current baudIndex is 3 (max). Navigate Up to 2
    ui.OnKeyDown(Key::Up);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(baudIndex == 2);
    CHECK(rBaud.changed);

    // Press Enter to accept and close
    ui.OnKeyDown(Key::Enter);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(!ui.IsPopupOpen(rBaud.id));
    CHECK(baudIndex == 2);

    // 7. String-based selection with Profile
    ui.OnMouseMove({rProfile.rect.x + 20.0f, rProfile.rect.y + 15.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(ui.IsPopupOpen(rProfile.id));

    popupRect = ui.GetOpenPopupRect();
    // Item 3 in Profile is "Extreme" (index 3)
    const float extremeY = popupRect.y + 4.0f + 3.0f * 32.0f + 10.0f;
    ui.OnMouseMove({popupRect.x + 30.0f, extremeY});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rProfile.changed);
    CHECK(profile == "Extreme");
    CHECK(!ui.IsPopupOpen(rProfile.id));

    // 8. Disabled mode suppresses interaction
    disabledMode = true;
    ui.OnMouseMove({rMode.rect.x + 20.0f, rMode.rect.y + 15.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(!rMode.clicked);
    CHECK(!ui.IsPopupOpen(rMode.id));
    CHECK(modeIndex == 0);

    // 9. Verify maxVisibleItems limits popup height to visibleCount * itemHeight + 8
    size_t limIdx = 0;
    WidgetResponse rLim;
    auto renderLimited = [&]() {
        rLim = ui.Select("Limited Dropdown", limIdx, {"Item 1", "Item 2", "Item 3", "Item 4", "Item 5"}, {
            .maxVisibleItems = 2
        });
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderLimited();
    ui.EndFrame(canvas);

    ui.OnMouseMove({rLim.rect.x + 10.0f, rLim.rect.y + 10.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderLimited();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderLimited();
    ui.EndFrame(canvas);

    CHECK(ui.IsPopupOpen(rLim.id));
    const Rect limPopupRect = ui.GetOpenPopupRect();
    // 2 visible items: 2 * 32.0f + 8.0f = 72.0f
    CHECK_NEAR(limPopupRect.h, 72.0f, 0.5f);
}

void TestCursorBlinkModesAndPhaseReset() {
    using namespace guikit;

    Context ui;

    // 1. Default Blinking Mode: 1.06s cycle, 530ms on / 530ms off, 500ms initial grace on input
    ui.SetCurrentTime(0.0);
    ui.ResetCursorBlink();
    CHECK(ui.IsCursorVisible() == true);

    // Within 500ms grace period: always visible
    ui.SetCurrentTime(0.4);
    CHECK(ui.IsCursorVisible() == true);

    // 0.6s total: 100ms into blink cycle (< 530ms on-phase) -> visible
    ui.SetCurrentTime(0.6);
    CHECK(ui.IsCursorVisible() == true);

    // 1.1s total: 600ms into cycle (> 530ms off-phase) -> hidden
    ui.SetCurrentTime(1.1);
    CHECK(ui.IsCursorVisible() == false);

    // 2. Typing Phase Reset: user presses a key or inputs text -> resets to visible
    ui.OnKeyDown(Key::Right);
    CHECK(ui.IsCursorVisible() == true);

    // Still within grace period after keystroke
    ui.AdvanceTime(0.3);
    CHECK(ui.IsCursorVisible() == true);

    // 3. Solid Mode: always visible
    ui.SetCursorConfig({.mode = CursorBlinkMode::Solid});
    ui.SetCurrentTime(10.0);
    CHECK(ui.IsCursorVisible() == true);
    ui.SetCurrentTime(100.0);
    CHECK(ui.IsCursorVisible() == true);

    // 4. Hidden Mode: always hidden
    ui.SetCursorConfig({.mode = CursorBlinkMode::Hidden});
    CHECK(ui.IsCursorVisible() == false);

    // 5. Local mode override via parameter
    CHECK(ui.IsCursorVisible(CursorBlinkMode::Solid) == true);
    CHECK(ui.IsCursorVisible(CursorBlinkMode::Hidden) == false);
}

void TestGridAndCardLayout() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    bool switchVal1 = false;
    bool switchVal2 = true;
    float speedVal = 120.0f;
    std::string nameVal = "Axis-1";

    WidgetResponse sw1Res;
    WidgetResponse sw2Res;
    WidgetResponse inp1Res;
    WidgetResponse inp2Res;

    auto renderUI = [&]() {
        // 1. Standalone CellGrid with 12-column slots
        if (auto grid = ui.CellGrid({.gap = UiSize::Md})) {
            if (auto c1 = grid.Cell6()) {
                sw1Res = ui.Switch("Sensor A", &switchVal1);
            }
            if (auto c2 = grid.Cell6()) {
                sw2Res = ui.Switch("Sensor B", &switchVal2);
            }
            if (auto c3 = grid.Cell12()) {
                inp1Res = ui.InputField("Speed Target", speedVal);
            }
        }

        // 2. High-level Card surface containing an explicit CellGrid layout
        if (auto card = ui.Card()) {
            ui.Heading({.title = "Motion Controller", .subtitle = "Direct axis tuning"});
            if (auto grid = ui.CellGrid()) {
                if (auto c1 = grid.Cell6()) {
                    inp2Res = ui.InputField("Device Name", nameVal);
                }
                if (auto c2 = grid.Cell6()) {
                    ui.Button("Calibrate Zero");
                }
            }
        }
    };

    // Frame 1: Initial render pass
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Frame 2: verify bounds and positioning
    // Viewport width = 800.
    // Standalone grid content width = 800.
    // Gap = 12 (UiSize::Md).
    // Cell 1 (span 6): W(6) = (6/12) * (800 + 12) - 12 = 0.5 * 812 - 12 = 406 - 12 = 394.
    // Cell 2 (span 6): starts at 394 + 12 = 406, width = 394, ends at 800.
    Rect bSw1 = ui.GetPrevFrameBounds(sw1Res.id);
    Rect bSw2 = ui.GetPrevFrameBounds(sw2Res.id);
    Rect bInp1 = ui.GetPrevFrameBounds(inp1Res.id);
    Rect bInp2 = ui.GetPrevFrameBounds(inp2Res.id);

    CHECK(bSw1.w > 0.0f);
    CHECK(bSw2.w > 0.0f);
    // In Cell6, the switch should be contained within the left half [0, 394]
    CHECK(bSw1.x < 394.0f);
    CHECK(bSw1.Right() <= 394.0f + 1.0f);

    // Sensor B switch should be in the right half [406, 800]
    CHECK(bSw2.x >= 406.0f - 1.0f);
    CHECK(bSw2.Right() <= 800.0f + 1.0f);

    // Input 1 was in Cell12, so it should be on line 2 (y > bSw1.Bottom())
    CHECK(bInp1.y >= bSw1.Bottom());
    // And should stretch across the full width [0, 800]
    CHECK_NEAR(bInp1.w, 800.0f, 1.0f);

    // Inside Card:
    // Card has padding 16 on all sides. Content width = 800 - 32 = 768.
    // Inner grid has default gap 12.
    // Cell 1 (span 6): W(6) = (6/12)*(768 + 12) - 12 = 0.5 * 780 - 12 = 390 - 12 = 378.
    // Input 2 should start at 16, width 378!
    CHECK_NEAR(bInp2.x, 16.0f, 1.0f);
    CHECK_NEAR(bInp2.w, 378.0f, 1.0f);

    // Frame 3: Mouse down on Switch inside Cell6
    ui.OnMouseMove({bSw1.x + 10.0f, bSw1.y + 10.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(sw1Res.pressed);

    // Frame 4: Mouse up on Switch -> click fired and boolean toggled
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(sw1Res.clicked);
    CHECK(switchVal1 == true);
}

void TestScrollAreaImmediateMode() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 400;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse btn1Res;
    WidgetResponse btn2Res;
    WidgetResponse btn3Res;

    auto renderUI = [&]() {
        if (auto scroll = ui.ScrollArea()) {
            if (auto box1 = ui.Box({.heightMode = SizeMode::Fixed, .fixedHeight = 200.0f})) {
                btn1Res = ui.Button("Top Item");
            }
            if (auto box2 = ui.Box({.heightMode = SizeMode::Fixed, .fixedHeight = 200.0f})) {
                btn2Res = ui.Button("Middle Item");
            }
            if (auto box3 = ui.Box({.heightMode = SizeMode::Fixed, .fixedHeight = 200.0f})) {
                btn3Res = ui.Button("Bottom Item");
            }
        }
    };

    // Frame 1: Initial layout
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    Rect bBtn1 = ui.GetPrevFrameBounds(btn1Res.id);
    Rect bBtn3 = ui.GetPrevFrameBounds(btn3Res.id);

    // Button 1 is visible at the top
    CHECK_NEAR(bBtn1.y, 0.0f, 1.0f);
    // Button 1 right edge strictly respects the scrollbar gutter (800 - 14 = 786px)
    const ScrollbarMetrics sm = UiTheme::Dark().GetScrollbar(UiSize::Md);
    CHECK_NEAR(bBtn1.Right(), 800.0f - sm.gutter, 1.0f);
    // Button 3 was placed at y = 400, which is outside the viewport (0..400) -> its clipped interaction bounds have h = 0
    CHECK(bBtn3.h <= 0.0f);

    // Frame 2: Mouse wheel down by 5 ticks -> 5 * 40 = 200px scroll
    ui.OnMouseMove({100.0f, 100.0f});
    ui.OnMouseWheel(0.0f, -5.0f);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    bBtn1 = ui.GetPrevFrameBounds(btn1Res.id);
    bBtn3 = ui.GetPrevFrameBounds(btn3Res.id);

    // Button 1 scrolled above top (clipped)
    CHECK(bBtn1.h <= 0.0f);
    // Button 3 is now visible in the viewport [200, 400]
    CHECK(bBtn3.h > 0.0f);
    CHECK(bBtn3.y >= 200.0f - 1.0f);

    // Frame 3: Click on Button 3
    ui.OnMouseMove({bBtn3.Center().x, bBtn3.Center().y});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(btn3Res.pressed);

    // Frame 4: Release mouse -> clicked event
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(btn3Res.clicked);

    // Frame 5: ScrollTo brings Button 1 back into view
    btn1Res.ScrollTo();
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    bBtn1 = ui.GetPrevFrameBounds(btn1Res.id);
    CHECK(bBtn1.h > 0.0f);
    CHECK_NEAR(bBtn1.y, 0.0f, 1.0f);

    // Verify ScrollAreaOptions with gap and horizontal orientation
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    if (auto scrollH = ui.ScrollArea({.orientation = Orientation::Horizontal, .gap = UiSize::Sm})) {
        Node* n = scrollH.GetNode();
        CHECK(n != nullptr);
        if (n) {
            CHECK(n->direction == FlexDirection::Row);
            CHECK(n->gap == UiTheme::Dark().GetGap(UiSize::Sm));
        }
    }
    ui.EndFrame(canvas);
}

void TestReactiveEventLoop() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    std::vector<uint32_t> pixels(100 * 100, 0);
    canvas.SetTarget(pixels.data(), 100, 100, 100);

    // Initial state: starts with 3 repaint frames to render initial scene
    CHECK(ui.GetRepaintFramesLeft() == 3);
    CHECK(ui.NeedsRepaint());
    CHECK(ui.GetRecommendedSleepTimeoutMs() == 0);

    // Drain initial 3 frames
    ui.BeginFrame({100.0f, 100.0f}); ui.EndFrame(canvas);
    CHECK(ui.GetRepaintFramesLeft() == 2);
    CHECK(ui.NeedsRepaint());

    ui.BeginFrame({100.0f, 100.0f}); ui.EndFrame(canvas);
    CHECK(ui.GetRepaintFramesLeft() == 1);
    CHECK(ui.NeedsRepaint());

    ui.BeginFrame({100.0f, 100.0f}); ui.EndFrame(canvas);
    CHECK(ui.GetRepaintFramesLeft() == 0);
    CHECK(!ui.NeedsRepaint());
    CHECK(ui.GetRecommendedSleepTimeoutMs() > 0);

    // Mouse movement wakes up reactive loop
    ui.OnMouseMove({25.0f, 25.0f});
    CHECK(ui.GetRepaintFramesLeft() == 3);
    CHECK(ui.NeedsRepaint());
    CHECK(ui.GetRecommendedSleepTimeoutMs() == 0);

    // Drain 3 frames
    for (int i = 0; i < 3; ++i) {
        ui.BeginFrame({100.0f, 100.0f});
        ui.EndFrame(canvas);
    }
    CHECK(!ui.NeedsRepaint());

    // Holding mouse button down prevents sleeping
    ui.OnMouseDown(1);
    CHECK(ui.IsMouseDown());
    CHECK(ui.NeedsRepaint());

    for (int i = 0; i < 5; ++i) {
        ui.BeginFrame({100.0f, 100.0f});
        ui.EndFrame(canvas);
    }
    // Still needs repaint because button is held down
    CHECK(ui.NeedsRepaint());

    // Releasing mouse button queues 3 settle frames
    ui.OnMouseUp(1);
    CHECK(!ui.IsMouseDown());
    CHECK(ui.GetRepaintFramesLeft() == 3);

    for (int i = 0; i < 3; ++i) {
        ui.BeginFrame({100.0f, 100.0f});
        ui.EndFrame(canvas);
    }
    CHECK(!ui.NeedsRepaint());

    // Key event wakes up loop
    ui.OnKeyDown(Key::Enter);
    CHECK(ui.GetRepaintFramesLeft() == 3);
    CHECK(ui.NeedsRepaint());

    // Drain frames
    for (int i = 0; i < 3; ++i) {
        ui.BeginFrame({100.0f, 100.0f});
        ui.EndFrame(canvas);
    }
    CHECK(!ui.NeedsRepaint());

    // Configurable idle heartbeat
    CHECK(ui.GetIdleHeartbeatMs() == 500);
    CHECK(ui.GetRecommendedSleepTimeoutMs() == 500);

    ui.SetIdleHeartbeatMs(1000);
    CHECK(ui.GetIdleHeartbeatMs() == 1000);
    CHECK(ui.GetRecommendedSleepTimeoutMs() == 1000);

    ui.SetIdleHeartbeatMs(-1); // Deep sleep
    CHECK(ui.GetIdleHeartbeatMs() == -1);
    CHECK(ui.GetRecommendedSleepTimeoutMs() == -1);

    ui.SetIdleHeartbeatMs(500); // Restore default
}

void TestDirtyRectDamageTracking() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    auto renderUI = [&](std::string* editVal = nullptr) {
        if (auto row = ui.Row()) {
            if (auto b = ui.Box({.fixedWidth = 120.0f})) {
                ui.Button("Test Button");
            }
            if (editVal) {
                if (auto b = ui.Box({.fixedWidth = 200.0f})) {
                    auto res = ui.InputField(*editVal);
                    if (res.clicked) {
                        res.Focus();
                    }
                }
            }
        }
    };

    // Frame 1: Initial layout establishment must trigger a full repaint
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(ui.IsFullRepaint());
    CHECK(ui.HasDirtyDamage());
    CHECK(ui.GetDirtyRect().w == static_cast<float>(width));
    CHECK(ui.GetDirtyRect().h == static_cast<float>(height));

    // Frame 2: Identical state with zero input produces zero damage
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(!ui.HasDirtyDamage());
    CHECK(!ui.IsFullRepaint());

    // Frame 3: Hover over the button marks only the button bounds as dirty
    ui.OnMouseMove({20.0f, 15.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(ui.HasDirtyDamage());
    CHECK(!ui.IsFullRepaint());
    CHECK(ui.GetDirtyRect().w < 200.0f);
    CHECK(ui.GetDirtyRect().h < 100.0f);

    // Frame 4: Caret blink toggles in an InputField
    std::string text = "Editable";
    bool shouldFocus = true;
    auto renderInput = [&]() {
        if (auto row = ui.Row()) {
            if (auto b = ui.Box({.fixedWidth = 200.0f})) {
                auto res = ui.InputField(text);
                if (shouldFocus) {
                    res.Focus();
                    shouldFocus = false;
                }
            }
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderInput();
    ui.EndFrame(canvas);

    // Settle edit state
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderInput();
    ui.EndFrame(canvas);

    // Advance time past solid phase (0.5s) and first half of blink cycle to cause cursor to hide
    ui.AdvanceTime(1.1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderInput();
    ui.EndFrame(canvas);

    CHECK(ui.HasDirtyDamage());
    CHECK(!ui.IsFullRepaint());
    CHECK(ui.GetDirtyRect().w < 300.0f);
    CHECK(ui.GetDirtyRect().h < 100.0f);

    // Frame 5: InvalidateAll forces full repaint
    ui.InvalidateAll();
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI(&text);
    ui.EndFrame(canvas);

    CHECK(ui.IsFullRepaint());
    CHECK(ui.HasDirtyDamage());
    CHECK(ui.GetDirtyRect().w == static_cast<float>(width));

    // Frame 6: Disabling dirty rect optimization forces full repaint
    ui.SetDirtyRectEnabled(false);
    CHECK(!ui.IsDirtyRectEnabled());
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI(&text);
    ui.EndFrame(canvas);

    CHECK(ui.IsFullRepaint());
    ui.SetDirtyRectEnabled(true);

    // Frame 7: Verify that toggling TabBarStyle (Underline -> Pill) produces dirty damage without any mouse input
    int tabIdx = 0;
    TabBarStyle currentTabStyle = TabBarStyle::Underline;
    auto renderTabTest = [&]() {
        if (auto col = ui.Column()) {
            ui.TabBar(tabIdx, {"Tab1", "Tab2"}, {.style = currentTabStyle});
        }
    };

    // Settle layout completely
    for (int i = 0; i < 3; ++i) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderTabTest();
        ui.EndFrame(canvas);
    }
    CHECK(!ui.HasDirtyDamage());

    // Switch style to Pill with ZERO mouse input
    currentTabStyle = TabBarStyle::Pill;
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderTabTest();
    ui.EndFrame(canvas);

    // Must produce dirty damage covering the tab bar immediately!
    CHECK(ui.HasDirtyDamage());
    CHECK(!ui.IsFullRepaint());
    CHECK(ui.GetDirtyRect().w > 0.0f);
    CHECK(ui.GetDirtyRect().h > 0.0f);
}

void TestTabBarWidgetWorkflow() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    int activeTab = 0;
    const std::vector<TabItem> tabs = {
        {.label = "Overview", .icon = Icon::Eye()},
        {.label = "Telemetry", .icon = Icon::Play(), .badge = "Live"},
        {.label = "Disabled", .disabled = true},
        {.label = "Settings", .icon = Icon::Settings()}
    };

    Response tabRes;
    auto renderUI = [&]() {
        if (auto col = ui.Column({.padding = Padding::All(20.0f)})) {
            tabRes = ui.TabBar(activeTab, tabs, {.style = TabBarStyle::Underline});
        }
    };

    // Frame 1: Initial layout establishment
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(activeTab == 0);
    CHECK(!tabRes.changed);

    // Frame 2: Layout is established
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Tab 0 is approx x=20..130, Tab 1 is approx x=136..260.
    // Move mouse over Tab 1 ("Telemetry") and click
    ui.OnMouseMove({180.0f, 36.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(activeTab == 1);
    CHECK(tabRes.changed);

    // Frame: Next frame without click should have changed == false
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(!tabRes.changed);
    CHECK(activeTab == 1);

    // Frame: Clicking on Tab 2 (disabled, approx x=270..350) should NOT change active index
    ui.OnMouseMove({300.0f, 36.0f});
    ui.OnMouseDown(1);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(activeTab == 1);
    CHECK(!tabRes.changed);

    // Frame: Test Pill style and string_view initializer_list overload
    int pillIdx = 0;
    auto renderPill = [&]() {
        if (auto col = ui.Column({.padding = Padding::All(10.0f)})) {
            ui.TabBar(pillIdx, {"Alpha", "Beta", "Gamma"}, {.style = TabBarStyle::Pill});
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderPill();
    ui.EndFrame(canvas);

    CHECK(pillIdx == 0);

    // Verify identical tab geometry between Underline and Pill styles
    for (const auto& tab : tabs) {
        const Vec2 underDims = TabBar::MeasureTab(tab, {.style = TabBarStyle::Underline}, UiSize::Md);
        const Vec2 pillDims = TabBar::MeasureTab(tab, {.style = TabBarStyle::Pill}, UiSize::Md);
        CHECK_NEAR(underDims.x, pillDims.x, 0.001f);
        CHECK_NEAR(underDims.y, pillDims.y, 0.001f);
    }
}

void TestToolbarWidgetWorkflow() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    Response btnNew;
    Response btnSave;
    Response btnSettings;
    bool toolSettingsActive = false;

    auto renderToolbar = [&]() {
        if (auto tb = ui.Toolbar({.size = UiSize::Md, .padding = UiSize::Sm})) {
            btnNew = tb.ToolButton("New", {.icon = Icon::Plus()});
            btnSave = tb.ToolButton("Save", {.icon = Icon::Check(), .disabled = true});
            tb.Separator();
            btnSettings = tb.ToolButton("", {.icon = Icon::Settings(), .active = toolSettingsActive});
            tb.Spacer();
            tb.ToolButton("Status", {.icon = Icon::Eye()});
        }
    };

    // Frame 1: Establish layout
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderToolbar();
    ui.EndFrame(canvas);

    CHECK(!btnNew.clicked);
    CHECK(!btnSave.clicked);
    CHECK(ui.GetPrevFrameBounds(btnNew.id).w > 0.0f);
    CHECK(ui.GetPrevFrameBounds(btnNew.id).h > 0.0f);

    // Frame 2: Hover over "New" button (x ~ 25.0f, y ~ 20.0f)
    ui.OnMouseMove({25.0f, 20.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderToolbar();
    CHECK(btnNew.hovered);
    ui.EndFrame(canvas);

    // Frame 3: Mouse down & up to click "New" button
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderToolbar();
    CHECK(btnNew.pressed);
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderToolbar();
    CHECK(btnNew.clicked);
    ui.EndFrame(canvas);

    // Frame 4: Disabled button cannot be clicked
    // btnSave is at x ~ 90..130
    ui.OnMouseMove({90.0f, 20.0f});
    ui.OnMouseDown(1);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderToolbar();
    CHECK(!btnSave.clicked);
    CHECK(!btnSave.hovered);
    ui.EndFrame(canvas);
}

void TestLabelVariantsStableSizing() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    bool state = false;
    Response btn;

    auto render = [&]() {
        if (auto row = ui.Row({.gap = UiSize::Sm})) {
            btn = ui.Button(state ? "VSync: ON" : "VSync: OFF", {
                .labelVariants = {"VSync: ON", "VSync: OFF"}
            });
        }
    };

    // Frame 1: OFF state
    state = false;
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);
    const Rect boundsOff = ui.GetPrevFrameBounds(btn.id);

    // Frame 2: ON state
    state = true;
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);
    const Rect boundsOn = ui.GetPrevFrameBounds(btn.id);

    // Bounds must be identical across state transitions
    CHECK(boundsOff.w > 0.0f);
    CHECK(boundsOn.w > 0.0f);
    CHECK_NEAR(boundsOff.w, boundsOn.w, 0.001f);
    CHECK_NEAR(boundsOff.h, boundsOn.h, 0.001f);
}

void TestContainerSizeInheritanceAndWidgetStretch() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    // 1. Verify container UiSize inheritance across widgets
    Response btnLg, btnSm;
    bool switchVal = false;
    bool checkVal = false;
    std::string inputVal = "42";
    int selectIdx = 0;
    Response swLg, swSm, cbLg, cbSm, inLg, inSm, selLg, selSm;

    auto renderSizeTest = [&](UiSize sz, bool isLarge) {
        if (auto col = ui.Column({.size = sz})) {
            if (isLarge) {
                btnLg = ui.Button("Large Action");
                swLg = ui.Switch("Large Switch", &switchVal);
                cbLg = ui.Checkbox("Large Checkbox", &checkVal);
                inLg = ui.InputField("Large Input", inputVal);
                selLg = ui.Select("Large Select", selectIdx, {"One", "Two"});
            } else {
                btnSm = ui.Button("Small Action");
                swSm = ui.Switch("Small Switch", &switchVal);
                cbSm = ui.Checkbox("Small Checkbox", &checkVal);
                inSm = ui.InputField("Small Input", inputVal);
                selSm = ui.Select("Small Select", selectIdx, {"One", "Two"});
            }
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderSizeTest(UiSize::Lg, true);
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderSizeTest(UiSize::Lg, true);
    ui.EndFrame(canvas);

    const Rect bLg = ui.GetPrevFrameBounds(btnLg.id);
    const Rect sLg = ui.GetPrevFrameBounds(swLg.id);
    const Rect cLg = ui.GetPrevFrameBounds(cbLg.id);
    const Rect iLg = ui.GetPrevFrameBounds(inLg.id);
    const Rect slLg = ui.GetPrevFrameBounds(selLg.id);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderSizeTest(UiSize::Sm, false);
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderSizeTest(UiSize::Sm, false);
    ui.EndFrame(canvas);

    const Rect bSm = ui.GetPrevFrameBounds(btnSm.id);
    const Rect sSm = ui.GetPrevFrameBounds(swSm.id);
    const Rect cSm = ui.GetPrevFrameBounds(cbSm.id);
    const Rect iSm = ui.GetPrevFrameBounds(inSm.id);
    const Rect slSm = ui.GetPrevFrameBounds(selSm.id);

    // Large widgets must have strictly greater height than small widgets
    CHECK(bLg.h > bSm.h);
    CHECK(sLg.h > sSm.h);
    CHECK(cLg.h > cSm.h);
    CHECK(iLg.h > iSm.h);
    CHECK(slLg.h > slSm.h);

    // 2. Verify Button stretch = true vs stretch = false in horizontal Row
    Response btnCompact, btnStretch1, btnStretch2;
    auto renderStretchRow = [&]() {
        if (auto row = ui.Row()) {
            btnCompact = ui.Button("Compact", {.stretch = false});
            btnStretch1 = ui.Button("Stretch", {.stretch = true});
            btnStretch2 = ui.Button("Stretch", {.stretch = true});
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderStretchRow();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderStretchRow();
    ui.EndFrame(canvas);

    const Rect rCompact = ui.GetPrevFrameBounds(btnCompact.id);
    const Rect rStr1 = ui.GetPrevFrameBounds(btnStretch1.id);
    const Rect rStr2 = ui.GetPrevFrameBounds(btnStretch2.id);

    CHECK(rCompact.w > 0.0f);
    CHECK(rStr1.w > rCompact.w);
    CHECK(rStr2.w > rCompact.w);
    CHECK_NEAR(rStr1.w, rStr2.w, 0.001f);
}

void TestUiStyleAndDensityScaling() {
    using namespace guikit;

    // 1. SpacingScale and RadiusScale defaults and presets
    const auto stdStyle = UiStyle::Standard();
    CHECK(stdStyle.GetDensity() == Density::Standard);
    CHECK(stdStyle.GetGap(UiSize::None) == 0.0f);
    CHECK(stdStyle.GetGap(UiSize::Xs) == 4.0f);
    CHECK(stdStyle.GetGap(UiSize::Sm) == 8.0f);
    CHECK(stdStyle.GetGap(UiSize::Md) == 12.0f);
    CHECK(stdStyle.GetGap(UiSize::Lg) == 16.0f);
    CHECK(stdStyle.GetGap(UiSize::Xl) == 24.0f);

    const auto compactStyle = UiStyle::Compact();
    CHECK(compactStyle.GetDensity() == Density::Compact);
    CHECK(compactStyle.GetGap(UiSize::Md) == 8.0f);
    CHECK(compactStyle.GetRadius(UiSize::Md) == 4.0f);

    const auto touchStyle = UiStyle::Touch();
    CHECK(touchStyle.GetDensity() == Density::Touch);
    CHECK(touchStyle.GetGap(UiSize::Md) == 16.0f);
    CHECK(touchStyle.GetRadius(UiSize::Md) == 8.0f);

    // 2. Context dynamic style switching
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 400;
    const uint32_t height = 300;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse b1, b2;

    auto render = [&]() {
        if (auto col = ui.Column()) {
            b1 = ui.Button("First");
            b2 = ui.Button("Second");
        }
    };

    // Frame 1: Standard density (default Md gap = 12px)
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    CHECK_NEAR(b2.rect.y, b1.rect.Bottom() + 12.0f, 0.01f);

    // Frame 2: Switch to Compact density (Md gap = 8px)
    ui.SetDensity(Density::Compact);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    CHECK_NEAR(b2.rect.y, b1.rect.Bottom() + 8.0f, 0.01f);

    // Frame 3: Switch to Touch density (Md gap = 16px)
    ui.SetDensity(Density::Touch);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    CHECK_NEAR(b2.rect.y, b1.rect.Bottom() + 16.0f, 0.01f);

    // Frame 4: Custom spacing scale override
    ui.GetStyle().Spacing().md = 25.0f;
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    CHECK_NEAR(b2.rect.y, b1.rect.Bottom() + 25.0f, 0.01f);
}

void TestLeafWidgetSizeOverrides() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 1200;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse btnInherit, btnXl, btnXs;
    WidgetResponse swInherit, swXl, swXs;
    WidgetResponse cbInherit, cbXl, cbXs;
    WidgetResponse inInherit, inXl, inXs;
    WidgetResponse selInherit, selXl, selXs;
    WidgetResponse toolInherit, toolXl, toolXs;

    bool swVal = true;
    bool cbVal = true;
    std::string inVal = "Test";
    size_t selIdx = 0;
    const std::vector<std::string_view> selItems = {"Option 1", "Option 2"};

    auto render = [&]() {
        // Container has default ambient size (UiSize::Md)
        if (auto col = ui.Column()) {
            // Buttons: Inherited (Md), Local override Xl, Local override Xs
            if (auto r1 = ui.Row()) {
                btnInherit = ui.Button("Inherit");
                btnXl = ui.Button("Large", {.size = UiSize::Xl});
                btnXs = ui.Button("Small", {.size = UiSize::Xs});
            }

            // Switches: Inherited (Md), Local override Xl, Local override Xs
            if (auto r2 = ui.Row()) {
                swInherit = ui.Switch("Inherit", &swVal);
                swXl = ui.Switch("Large", &swVal, {.size = UiSize::Xl});
                swXs = ui.Switch("Small", &swVal, {.size = UiSize::Xs});
            }

            // Checkboxes: Inherited (Md), Local override Xl, Local override Xs
            if (auto r3 = ui.Row()) {
                cbInherit = ui.Checkbox("Inherit", &cbVal);
                cbXl = ui.Checkbox("Large", &cbVal, {.size = UiSize::Xl});
                cbXs = ui.Checkbox("Small", &cbVal, {.size = UiSize::Xs});
            }

            // InputFields: Inherited (Md), Local override Xl, Local override Xs
            if (auto r4 = ui.Row()) {
                inInherit = ui.InputField("Inherit", inVal);
                inXl = ui.InputField("Large", inVal, {.size = UiSize::Xl});
                inXs = ui.InputField("Small", inVal, {.size = UiSize::Xs});
            }

            // Selects: Inherited (Md), Local override Xl, Local override Xs
            if (auto r5 = ui.Row()) {
                selInherit = ui.Select("Inherit", selIdx, selItems);
                selXl = ui.Select("Large", selIdx, selItems, {.size = UiSize::Xl});
                selXs = ui.Select("Small", selIdx, selItems, {.size = UiSize::Xs});
            }

            // Toolbar ToolButtons: Inherited (Md), Local override Xl, Local override Xs
            if (auto tb = ui.Toolbar()) {
                toolInherit = tb.ToolButton("Inherit", {.icon = Icon::Play()});
                toolXl = tb.ToolButton("Large", {.icon = Icon::Play(), .size = UiSize::Xl});
                toolXs = tb.ToolButton("Small", {.icon = Icon::Play(), .size = UiSize::Xs});
            }
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    const Rect rBtnDef = ui.GetPrevFrameBounds(btnInherit.id);
    const Rect rBtnXl = ui.GetPrevFrameBounds(btnXl.id);
    const Rect rBtnXs = ui.GetPrevFrameBounds(btnXs.id);

    CHECK(rBtnXl.h > rBtnDef.h);
    CHECK(rBtnDef.h > rBtnXs.h);

    const Rect rSwDef = ui.GetPrevFrameBounds(swInherit.id);
    const Rect rSwXl = ui.GetPrevFrameBounds(swXl.id);
    const Rect rSwXs = ui.GetPrevFrameBounds(swXs.id);

    CHECK(rSwXl.h > rSwDef.h);
    CHECK(rSwDef.h > rSwXs.h);

    const Rect rCbDef = ui.GetPrevFrameBounds(cbInherit.id);
    const Rect rCbXl = ui.GetPrevFrameBounds(cbXl.id);
    const Rect rCbXs = ui.GetPrevFrameBounds(cbXs.id);

    CHECK(rCbXl.h > rCbDef.h);
    CHECK(rCbDef.h > rCbXs.h);

    const Rect rInDef = ui.GetPrevFrameBounds(inInherit.id);
    const Rect rInXl = ui.GetPrevFrameBounds(inXl.id);
    const Rect rInXs = ui.GetPrevFrameBounds(inXs.id);

    CHECK(rInXl.h > rInDef.h);
    CHECK(rInDef.h > rInXs.h);

    const Rect rSelDef = ui.GetPrevFrameBounds(selInherit.id);
    const Rect rSelXl = ui.GetPrevFrameBounds(selXl.id);
    const Rect rSelXs = ui.GetPrevFrameBounds(selXs.id);

    CHECK(rSelXl.h > rSelDef.h);
    CHECK(rSelDef.h > rSelXs.h);

    const Rect rToolDef = ui.GetPrevFrameBounds(toolInherit.id);
    const Rect rToolXl = ui.GetPrevFrameBounds(toolXl.id);
    const Rect rToolXs = ui.GetPrevFrameBounds(toolXs.id);

    CHECK(rToolXl.h > rToolDef.h);
    CHECK(rToolDef.h > rToolXs.h);
}

void TestHeadingWidgetWorkflow() {
    using namespace guikit;

    // 1. Standalone Heading::Measure tests
    HeadingOptions optsTitleOnly{.title = "System Configuration"};
    Vec2 sizeTitleOnly = Heading::Measure(optsTitleOnly, UiSize::Md);
    CHECK(sizeTitleOnly.x > 0.0f);
    CHECK(sizeTitleOnly.y > 0.0f);

    HeadingOptions optsWithSub{.title = "System Configuration", .subtitle = "Hardware line supervision & failsafes"};
    Vec2 sizeWithSub = Heading::Measure(optsWithSub, UiSize::Md);
    CHECK(sizeWithSub.x >= sizeTitleOnly.x);
    // Heading with subtitle must be taller than title-only
    CHECK(sizeWithSub.y > sizeTitleOnly.y);

    HeadingOptions optsWithBadgeIcon{
        .title = "Telemetry Stream",
        .subtitle = "1000 samples/sec",
        .icon = Icon::Check(),
        .badge = "ACTIVE"
    };
    Vec2 sizeWithBadgeIcon = Heading::Measure(optsWithBadgeIcon, UiSize::Md);
    CHECK(sizeWithBadgeIcon.x > sizeTitleOnly.x);

    // UiSize scaling for Measure
    Vec2 sizeXs = Heading::Measure(optsWithBadgeIcon, UiSize::Xs);
    Vec2 sizeMd = Heading::Measure(optsWithBadgeIcon, UiSize::Md);
    Vec2 sizeXl = Heading::Measure(optsWithBadgeIcon, UiSize::Xl);
    CHECK(sizeXl.y > sizeMd.y);
    CHECK(sizeMd.y > sizeXs.y);
    CHECK(sizeXl.x > sizeMd.x);
    CHECK(sizeMd.x > sizeXs.x);

    // 2. Immediate mode layout & bounds
    Context ui;
    const int width = 800;
    const int height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    ThorCanvas canvas;
    canvas.SetTarget(pixels.data(), width, height, width);

    Response head1, head2;
    std::string dynamicSubtitle = "Initial Subtitle";
    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = UiSize::Md, .padding = Padding::All(16.0f)})) {
            head1 = ui.Heading({.title = "System Overview", .subtitle = dynamicSubtitle});
            head2 = ui.Heading({
                .title = "Telemetry Stream",
                .subtitle = "1000 samples/sec",
                .icon = Icon::Check(),
                .badge = "ACTIVE",
                .variant = UiVariant::Success
            });
        }
    };

    // Settle 2 frames
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(head1.id.IsValid());
    CHECK(head2.id.IsValid());

    const Rect r1 = ui.GetPrevFrameBounds(head1.id);
    const Rect r2 = ui.GetPrevFrameBounds(head2.id);

    // Width should stretch across container (800 - 32 padding = 768)
    CHECK_NEAR(r1.w, 768.0f, 1.0f);
    CHECK_NEAR(r2.w, 768.0f, 1.0f);
    CHECK(r1.h > 0.0f);
    CHECK(r2.h > 0.0f);
    CHECK(r2.y >= r1.Bottom());

    // 3. Dirty rect damage tracking when heading text changes
    // Frame 3: no changes -> no dirty damage
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(!ui.HasDirtyDamage());

    // Frame 4: change dynamic subtitle -> dirty damage must trigger and cover r1
    dynamicSubtitle = "Updated Subtitle with Alert";
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(ui.HasDirtyDamage());
    CHECK(!ui.IsFullRepaint());
    CHECK(ui.GetDirtyRect().Intersects(r1));
}

void TestDpiScalingAndBackendMetadata() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const uint32_t winWidth = 1200;
    const uint32_t winHeight = 900;
    std::vector<uint32_t> pixels(winWidth * winHeight, 0);
    canvas.SetTarget(pixels.data(), winWidth, winHeight, winWidth);

    // Backend metadata check
    CHECK(canvas.GetBackendName() == "ThorVG (Vector Software)");

    // Frame 1: 100% scale
    ui.BeginFrame({static_cast<float>(winWidth), static_cast<float>(winHeight)}, 1.0f);
    CHECK_NEAR(ui.GetLogicalViewport().x, 1200.0f, 0.01f);
    CHECK_NEAR(ui.GetLogicalViewport().y, 900.0f, 0.01f);
    CHECK_NEAR(ui.GetDpiScale(), 1.0f, 0.01f);
    ui.Button("TestBtn");
    ui.EndFrame(canvas);
    CHECK_NEAR(canvas.GetScale(), 1.0f, 0.01f);

    // Frame 2: 150% scale (DPI scale = 1.5)
    ui.BeginFrame({static_cast<float>(winWidth), static_cast<float>(winHeight)}, 1.5f);
    CHECK_NEAR(ui.GetLogicalViewport().x, 800.0f, 0.01f);
    CHECK_NEAR(ui.GetLogicalViewport().y, 600.0f, 0.01f);
    CHECK_NEAR(ui.GetDpiScale(), 1.5f, 0.01f);

    // Mouse movement in physical window pixels: x=150, y=30
    // Under 1.5x scale, logical mouse coordinates should be x=100, y=20
    ui.OnMouseMove({150.0f, 30.0f});
    CHECK_NEAR(ui.GetMousePos().x, 100.0f, 0.01f);
    CHECK_NEAR(ui.GetMousePos().y, 20.0f, 0.01f);

    // Render button and test layout
    ui.Button("TestBtn");
    ui.EndFrame(canvas);

    // Canvas should receive 1.5x scale
    CHECK_NEAR(canvas.GetScale(), 1.5f, 0.01f);

    // Physical dirty rect should be scaled by 1.5x
    const Rect logicalDirty = ui.GetDirtyRect();
    const Rect physicalDirty = ui.GetPhysicalDirtyRect();
    CHECK(physicalDirty.w >= logicalDirty.w * 1.5f - 0.5f);
    CHECK(physicalDirty.h >= logicalDirty.h * 1.5f - 0.5f);
}

void TestSdlCanvasBackendInterface() {
    using namespace guikit;

    // SdlCanvas without active renderer (headless / stub test)
    SdlCanvas canvas(nullptr);
    CHECK(canvas.GetRenderer() == nullptr);
    CHECK(canvas.GetBackendName() == "SDL3 (Direct GPU)");

    canvas.Begin(800, 600, 1.25f);
    CHECK_NEAR(canvas.GetScale(), 1.25f, 0.01f);

    canvas.SetScale(2.0f);
    CHECK_NEAR(canvas.GetScale(), 2.0f, 0.01f);

    // Test text measurement works headlessly
    const Vec2 dims = canvas.MeasureText("Hello GuiKit", 16.0f);
    CHECK(dims.x > 0.0f);
    CHECK(dims.y > 0.0f);

    // Drawing calls with nullptr renderer are safe and do not crash
    canvas.Clear(Color(0, 0, 0));
    canvas.PushClip(Rect(0, 0, 100, 100));
    canvas.DrawRect(Rect(10, 10, 50, 50), Color(255, 255, 255));
    canvas.DrawRoundRect(Rect(10, 10, 50, 50), 4.0f, Color(255, 255, 255));
    canvas.DrawLine(Vec2(0, 0), Vec2(100, 100), Color(255, 255, 255), 1.0f);
    canvas.DrawText("Hello", Vec2(10, 10), 14.0f, Color(255, 255, 255));
    canvas.DrawTextBox("Hello Box", Rect(0, 0, 100, 40), 14.0f, Color(255, 255, 255));
    canvas.PopClip();
    canvas.End();

    // Verify Context renders into SdlCanvas via ICanvas polymorphism
    Context ui;
    ui.BeginFrame({800.0f, 600.0f}, 1.25f);
    ui.Button("GPU Button");
    ui.EndFrame(canvas);
}

void TestIndustrialDashboardNoBlackLines() {
    using namespace guikit;

    const std::vector<float> testScales = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};

    for (float testScale : testScales) {
        Context ui;
        ThorCanvas canvas;
        const uint32_t width = 1000;
        const uint32_t height = 750;
        std::vector<uint32_t> pixels(width * height, 0);
        canvas.SetTarget(pixels.data(), width, height, width);

        bool enableSensors = true;
        float maxTravel = 150.0f;
        float maxVelocity = 25.0f;
        size_t sensorChannel = 0;
        size_t activeTabIndex = 2; // Industrial Dashboard
        bool hwEstop = false;
        size_t interlockMode = 0;
        float rampDown = 250.0f;

        Id cardId = Id::Null();

        auto renderUI = [&]() {
            if (auto mainCol = ui.Column()) {
                if (auto header = ui.Row({.align = Alignment::Center})) {
                    ui.Title("GUIKIT GUI", UiVariant::Info);
                    ui.Caption("| Autonomous Vector GUI Engine");
                    ui.Spacer();
                    ui.Caption("C++20 Immediate + ThorVG");
                }

                if (auto tb = ui.Toolbar()) {
                    tb.ToolButton("Density: Standard");
                    tb.Separator();
                    tb.ToolButton("Scale: 100%");
                    tb.Separator();
                    tb.ToolButton("Theme: Dark");
                }

                const std::vector<TabItem> navTabs = {
                    {.label = "Overview", .icon = Icon::Eye()},
                    {.label = "Flexbox Demo", .icon = Icon::Settings()},
                    {.label = "Industrial Dashboard", .icon = Icon::Folder(), .badge = "Live"}
                };
                ui.TabBar(activeTabIndex, navTabs);

                if (activeTabIndex == 2) {
                    if (auto scroll = ui.ScrollArea()) {
                        if (auto dashboardGrid = ui.CellGrid()) {
                            if (auto colLeft = dashboardGrid.Cell6()) {
                                if (auto card1 = ui.Card()) {
                                    cardId = card1.GetNode()->id;
                                    ui.Heading({.title = "Primary Stop Conditions", .subtitle = "Sensory tripwire thresholds & limits"});
                                    if (auto grid1 = ui.CellGrid()) {
                                        if (auto c = grid1.Cell12()) {
                                            ui.Switch("Enable Sensor Drop Limits", &enableSensors, {
                                                .description = "Immediate stop on threshold breach"
                                            });
                                        }
                                        if (auto c = grid1.Cell6()) {
                                            ui.InputNumber("Max Travel", maxTravel, "mm", {.format = "%.1f"});
                                        }
                                        if (auto c = grid1.Cell6()) {
                                            ui.InputNumber("Max Velocity", maxVelocity, "mm/s", {.format = "%.1f"});
                                        }
                                        if (auto c = grid1.Cell12()) {
                                            ui.Select("Primary Sensor Channel", sensorChannel, {
                                                "Channel A (Load Cell 50kN)",
                                                "Channel B (Optical Extensometer)",
                                                "Channel C (Laser Interferometer)"
                                            }, {
                                                .description = "Active physical transducer bus",
                                                .labelPlacement = Placement::Stacked
                                            });
                                        }
                                    }
                                }

                                if (auto card2 = ui.Card()) {
                                    ui.Heading({.title = "Emergency Interlocks", .subtitle = "Hardware line supervision & failsafes", .variant = UiVariant::Danger});
                                    if (auto grid2 = ui.CellGrid()) {
                                        if (auto c = grid2.Cell12()) {
                                            ui.Switch("Hardware E-Stop Line", &hwEstop, {
                                                .description = "Continuous 24V loop monitoring",
                                                .variant = UiVariant::Danger
                                            });
                                        }
                                        if (auto c = grid2.Cell6()) {
                                            ui.Select("Interlock Action", interlockMode, {
                                                "Immediate Cutoff",
                                                "Controlled Ramp-down",
                                                "Hold Position"
                                            });
                                        }
                                        if (auto c = grid2.Cell6()) {
                                            ui.InputNumber("Ramp Duration", rampDown, "ms", {.format = "%.0f"});
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        };

        // Frame 1: Full initial frame
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)}, testScale);
        renderUI();
        ui.EndFrame(canvas);

        Rect cardRect = ui.GetPrevFrameBounds(cardId);

        // Helper to check for horizontal clearColor seams in card interior
        auto checkCardSeams = [&](int frameNum) {
            const float s = testScale;
            // Physical pixel bounds of card interior
            const uint32_t startY = static_cast<uint32_t>(std::max(0.0f, (cardRect.y + 15.0f) * s));
            const uint32_t endY = static_cast<uint32_t>(std::min(static_cast<float>(height), (cardRect.Bottom() - 15.0f) * s));
            const uint32_t startX = static_cast<uint32_t>(std::max(0.0f, (cardRect.x + 15.0f) * s));
            const uint32_t endX = static_cast<uint32_t>(std::min(static_cast<float>(width), (cardRect.Right() - 15.0f) * s));

            for (uint32_t y = startY; y < endY; ++y) {
                int clearCount = 0;
                for (uint32_t x = startX; x < endX; ++x) {
                    uint32_t p = pixels[y * width + x];
                    uint8_t r = p & 0xFF;
                    uint8_t g = (p >> 8) & 0xFF;
                    uint8_t b = (p >> 16) & 0xFF;
                    if (std::abs(r - 11) <= 2 && std::abs(g - 15) <= 2 && std::abs(b - 25) <= 2) {
                        ++clearCount;
                    }
                }
                if (clearCount > 50) {
                    std::cerr << "[SCALE " << testScale << " FRAME " << frameNum << " DEFECT] Row y=" << y << " has " << clearCount << " clearColor pixels inside card!\n";
                    CHECK(clearCount <= 50);
                }
            }
        };

        checkCardSeams(1);

        // Frame 2: Mouse move across widgets (hovering)
        // Switch position
        ui.OnMouseMove({(cardRect.x + 30.0f) * testScale, (cardRect.y + 70.0f) * testScale});
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)}, testScale);
        renderUI();
        ui.EndFrame(canvas);
        checkCardSeams(2);

        // Frame 3: Move to InputField
        ui.OnMouseMove({(cardRect.x + 30.0f) * testScale, (cardRect.y + 130.0f) * testScale});
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)}, testScale);
        renderUI();
        ui.EndFrame(canvas);
        checkCardSeams(3);

        // Frame 4: Move to Select
        ui.OnMouseMove({(cardRect.x + 30.0f) * testScale, (cardRect.y + 190.0f) * testScale});
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)}, testScale);
        renderUI();
        ui.EndFrame(canvas);
        checkCardSeams(4);
    }
}

void TestCarouselWidgetAndTabBarOverflow() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 300;
    const uint32_t height = 400;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    // 1. Standalone Carousel with buttons
    WidgetResponse b1, b2, b3;
    Node* carouselNode = nullptr;
    auto renderCarousel = [&]() {
        if (auto c = ui.Carousel({.gap = UiSize::Sm})) {
            carouselNode = c.GetNode();
            b1 = ui.Button("Button 1");
            b2 = ui.Button("Button 2");
            b3 = ui.Button("Button 3");
            ui.Button("Button 4");
            ui.Button("Button 5");
        }
    };

    // Frame 1: Measure and layout
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderCarousel();
    ui.EndFrame(canvas);

    // Frame 2: Bounds collected
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderCarousel();
    ui.EndFrame(canvas);

    CHECK(b1.rect.w > 0.0f);
    CHECK(b2.rect.w > 0.0f);
    const float initialB1X = b1.rect.x;

    // At offset 0: left arrow hidden, right arrow visible
    CHECK(carouselNode != nullptr);
    if (carouselNode) {
        auto* cData = carouselNode->As<CarouselData>();
        CHECK(cData != nullptr);
        if (cData) {
            CHECK(!cData->showLeftArrow);
            CHECK(cData->showRightArrow);
            CHECK(cData->leftArrowRect.w == 0.0f);
            CHECK(cData->rightArrowRect.w > 0.0f);
        }
    }

    // Initial offset is 0, so right arrow is visible at the right edge (x ~ 268..300, y ~ 0..36)
    // Click near right edge: x = 285, y = 18
    ui.OnMouseMove({285.0f, 18.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderCarousel();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderCarousel();
    ui.EndFrame(canvas);

    // Frame after click propagation
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderCarousel();
    ui.EndFrame(canvas);

    // Button 1 should have moved left due to arrow scroll!
    CHECK(b1.rect.x < initialB1X || b1.rect.w == 0.0f);
    CHECK(b3.rect.w > 0.0f); // Button 3 is now scrolled into view

    // Scrolled: left arrow should now be visible!
    if (carouselNode) {
        auto* cData = carouselNode->As<CarouselData>();
        CHECK(cData != nullptr);
        if (cData) {
            CHECK(cData->showLeftArrow);
            CHECK(cData->leftArrowRect.w > 0.0f);
        }
    }

    // Test mouse wheel on carousel
    const float prevB3X = b3.rect.x;
    ui.OnMouseMove({150.0f, 18.0f});
    ui.OnMouseWheel(0.0f, -2.0f); // Wheel down/right
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderCarousel();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderCarousel();
    ui.EndFrame(canvas);

    CHECK(b3.rect.x < prevB3X); // Scrolled further left

    // 2. TabBar with overflow and auto-scroll to active tab
    int activeTab = 0;
    const std::vector<TabItem> longTabs = {
        {.label = "General"},
        {.label = "Network"},
        {.label = "Security"},
        {.label = "Storage"},
        {.label = "Sensors"},
        {.label = "Diagnostics"},
        {.label = "Advanced"}
    };

    auto renderTabBar = [&]() {
        ui.TabBar(activeTab, longTabs);
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderTabBar();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderTabBar();
    ui.EndFrame(canvas);

    CHECK(activeTab == 0);

    // Switch active tab programmatically to last tab (Advanced)
    activeTab = 6;
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderTabBar();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderTabBar();
    ui.EndFrame(canvas);

    CHECK(activeTab == 6);
}

void TestDebugBoundsAndInspectMode() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    CHECK(!ui.IsDebugBounds());
    CHECK(!ui.IsInspectMode());

    ui.SetDebugBounds(true);
    CHECK(ui.IsDebugBounds());

    ui.SetInspectMode(true);
    CHECK(ui.IsInspectMode());

    // Frame with debug bounds and inspect mode enabled
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    if (auto col = ui.Column({.gap = UiSize::Sm, .padding = Padding::All(12.0f)})) {
        if (auto row = ui.Row({.gap = UiSize::Md})) {
            ui.Button("Inspect Me");
            ui.Spacer();
            ui.Switch("Toggle Switch", false);
        }
        if (auto card = ui.Card()) {
            ui.Text("Card Inner Text");
        }
    }
    // Hover mouse over button area
    ui.OnMouseMove({30.0f, 25.0f});
    ui.EndFrame(canvas);

    // Frame renders fully without error, has damage
    CHECK(ui.HasDirtyDamage());
    CHECK(ui.IsFullRepaint());

    // Toggle off
    ui.SetDebugBounds(false);
    ui.SetInspectMode(false);
    CHECK(!ui.IsDebugBounds());
    CHECK(!ui.IsInspectMode());

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    if (auto col = ui.Column()) {
        ui.Text("Normal Mode");
    }
    ui.EndFrame(canvas);
}

void TestPostRenderDelegateWorkflow() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 400;
    const uint32_t height = 300;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    static std::vector<int> executionTrace;
    executionTrace.clear();

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});

    if (auto c = ui.Column()) {
        Node* containerNode = c.GetNode();
        CHECK(containerNode != nullptr);
        if (containerNode) {
            containerNode->renderFn = [](const Node*, ICanvas&, const UiTheme&) {
                executionTrace.push_back(1); // Pre-pass: before children
            };
            containerNode->postRenderFn = [](const Node*, ICanvas&, const UiTheme&) {
                executionTrace.push_back(3); // Post-pass: after children
            };
        }

        ui.Button("Child Button");
        Node* childNode = containerNode ? containerNode->firstChild : nullptr;
        CHECK(childNode != nullptr);
        if (childNode) {
            childNode->renderFn = [](const Node*, ICanvas&, const UiTheme&) {
                executionTrace.push_back(2); // Child render pass
            };
        }
    }

    ui.EndFrame(canvas);

    // Verify exact execution sequence: [1 (Pre-pass), 2 (Child pass), 3 (Post-pass)]
    CHECK(executionTrace.size() == 3);
    if (executionTrace.size() == 3) {
        CHECK(executionTrace[0] == 1);
        CHECK(executionTrace[1] == 2);
        CHECK(executionTrace[2] == 3);
    }
}

void TestContainersAndLayoutsReorganization() {
    using namespace guikit;

    std::cout << "  - TestContainersAndLayoutsReorganization...\n";
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    ui.BeginFrame({800.0f, 600.0f});

    // 1. Layouts: Row, Column, Wrap, Grid
    if (auto col = ui.Column({.gap = UiSize::Md})) {
        if (auto row = ui.Row({.gap = UiSize::Sm, .align = Alignment::Center})) {
            ui.Button("Btn 1");
            ui.Button("Btn 2");
        }
        if (auto wrap = ui.Wrap({.gap = UiSize::Xs, .runGap = UiSize::Sm})) {
            ui.Button("W1");
            ui.Button("W2");
        }
        if (auto grid = ui.Grid({.columns = 12, .gap = UiSize::Md})) {
            if (auto c6 = grid.Cell6()) {
                ui.Text("Half Width Cell");
            }
        }
    }

    // 2. Containers: Box, Card, ScrollArea, Carousel
    if (Container box = ui.Box({.gap = UiSize::Sm, .padding = Padding::All(8.0f), .radius = UiSize::Md})) {
        Node* node = box.GetNode();
        CHECK(node != nullptr);
        CHECK(node->cornerRadius == ui.GetTheme().GetRadius(UiSize::Md));
        CHECK(node->padding.left == 8.0f);
        ui.Text("Inside Box");
    }

    if (auto card = ui.Card({.gap = UiSize::Md, .padding = Padding::All(16.0f), .radius = UiSize::Lg})) {
        Node* node = card.GetNode();
        CHECK(node != nullptr);
        CHECK(node->cornerRadius == ui.GetTheme().GetRadius(UiSize::Lg));
        CHECK(node->padding.top == 16.0f);
        ui.Text("Inside Card");
    }

    if (auto scroll = ui.ScrollArea({.orientation = Orientation::Vertical, .gap = UiSize::Sm, .padding = Padding::Symmetric(12.0f, 0.0f)})) {
        Node* node = scroll.GetNode();
        CHECK(node != nullptr);
        CHECK(node->padding.left == 12.0f);
        CHECK(node->gap == ui.GetTheme().GetGap(UiSize::Sm));
        ui.Button("Scroll Item 1");
    }

    if (auto carousel = ui.Carousel({.gap = UiSize::Lg, .padding = Padding::All(6.0f), .align = Alignment::End, .justify = Justify::Center})) {
        Node* node = carousel.GetNode();
        CHECK(node != nullptr);
        CHECK(node->align == Alignment::End);
        CHECK(node->justify == Justify::Center);
        CHECK(node->padding.left == 6.0f);
        CHECK(node->gap == ui.GetTheme().GetGap(UiSize::Lg));
        ui.Button("Carousel Item 1");
    }

    // 3. Static layout facades: Row::Open, Column::Open, Wrap::Open, Grid::Open, Spacer::Run
    if (auto col2 = Column::Open(ui, {.gap = UiSize::Sm})) {
        if (auto row2 = Row::Open(ui)) {
            ui.Button("Static Row Btn");
            Spacer::Run(ui);
            ui.Button("Static Row Btn 2");
        }
        if (auto wrap2 = Wrap::Open(ui)) {
            ui.Button("Static Wrap Btn");
        }
        if (auto grid2 = Grid::Open(ui)) {
            if (auto cell = grid2.Cell()) {
                ui.Text("Static Grid Cell");
            }
        }
    }

    ui.EndFrame(canvas);
}

int main() {
    std::cout << "[RUN] Testing GuiKit Context & Immediate Mode API...\n";

    TestIndustrialDashboardNoBlackLines();
    TestImmediateApiWorkflow();
    TestThorCanvasTextRendering();
    TestHeaderRowTextMetricsAndNoOverlap();
    TestFooterExitButtonStaysVisibleInNarrowWindow();
    TestWrapContainerInContextApi();
    TestFooterExitButtonPushedToBottomWithCardAndWrap();
    TestUiTokensAndIconFactories();
    TestTwoLevelDisabledWorkflow();
    TestBoxSlotWithUiSizeToken();
    TestButtonWidgetRunAndResponseChaining();
    TestUiStringAndContextFormatting();
    TestBoxContainerGapSpacing();
    TestAutonomousTextWidgetAndTypographyRoles();
    TestSwitchAndCheckboxWidgets();
    TestInputFieldWidget();
    TestSelectWidget();
    TestCursorBlinkModesAndPhaseReset();
    TestGridAndCardLayout();
    TestScrollAreaImmediateMode();
    TestReactiveEventLoop();
    TestDirtyRectDamageTracking();
    TestTabBarWidgetWorkflow();
    TestToolbarWidgetWorkflow();
    TestCarouselWidgetAndTabBarOverflow();
    TestLabelVariantsStableSizing();
    TestContainerSizeInheritanceAndWidgetStretch();
    TestUiStyleAndDensityScaling();
    TestLeafWidgetSizeOverrides();
    TestHeadingWidgetWorkflow();
    TestDpiScalingAndBackendMetadata();
    TestSdlCanvasBackendInterface();
    TestDebugBoundsAndInspectMode();
    TestPostRenderDelegateWorkflow();
    TestContainersAndLayoutsReorganization();

    if (g_failures == 0) {
        std::cout << "[PASS] All GuiKit Context tests passed successfully!\n";
        return 0;
    } else {
        std::cerr << "[FAIL] Total failures: " << g_failures << "\n";
        return 1;
    }
}

