#include <iostream>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/core/TextUtil.hpp"
#include "guikit/core/Theme.hpp"
#include <SDL3/SDL.h>
#include "guikit/backend/SdlCanvas.hpp"
#include "guikit/backend/SdlGlShaderEngine.hpp"
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
    CHECK(fontOk);

    const Vec2 lenaSize = canvas.GetImageSize("assets/images/lena.png");
    CHECK(lenaSize.x > 0.0f && lenaSize.y > 0.0f);

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
        if (auto row = ui.Row({.gap = 8.0f})) {
            if (auto b1 = ui.Box({.fixedWidth = 120.0f})) {
                bOverview = ui.Button({.label = "Overview", .stretch = StretchMode::Fill});
            }
            if (auto b2 = ui.Box({.fixedWidth = 150.0f})) {
                bFlex = ui.Button({.label = "Flexbox Demo", .stretch = StretchMode::Fill});
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
        if (auto col = ui.Column({.gap = 16.0f, .padding = Padding::All(24.0f)})) {
            // Wide header (requires ~600px+)
            if (auto header = ui.Row({.gap = 12.0f})) {
                ui.Text("GUIKIT GUI", {.role = TextRole::TitleLarge});
                ui.Text("| Autonomous Vector GUI Engine", {.role = TextRole::LabelMedium, .variant = UiVariant::Muted});
                ui.Spacer();
                ui.Text("C++20 Immediate + ThorVG", {.role = TextRole::LabelMedium, .variant = UiVariant::Muted});
            }

            ui.Spacer();

            // Footer with Exit button pushed to right
            if (auto footer = ui.Row({.gap = 12.0f})) {
                ui.Text("Zero heap allocations per frame | 0% idle CPU", {.role = TextRole::LabelMedium, .variant = UiVariant::Muted});
                ui.Spacer();
                if (auto b = ui.Box({.fixedWidth = 80.0f})) {
                    exitBtn = ui.Button({.label = "Exit", .stretch = StretchMode::Fill});
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

    ui.GetTheme().Spacing().md = 10.0f;
    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(20.0f)})) {
            // Available width inside padding = 300 - 40 = 260.
            // b1 (fixed 120), b2 (fixed 120), b3 (fixed 120)
            // b1 (120) + gap (10) + b2 (120) = 250 <= 260 (Line 1)
            // b3 wraps to Line 2! (y = 20 + 36 + 10 = 66)
            if (auto wrap = ui.Wrap({.gap = 10.0f, .runGap = 10.0f})) {
                if (auto box1 = ui.Box({.fixedWidth = 120.0f, .fixedHeight = 36.0f})) {
                    b1 = ui.Button({.label = "Button 1", .stretch = StretchMode::Fill});
                }
                if (auto box2 = ui.Box({.fixedWidth = 120.0f, .fixedHeight = 36.0f})) {
                    b2 = ui.Button({.label = "Button 2", .stretch = StretchMode::Fill});
                }
                if (auto box3 = ui.Box({.fixedWidth = 120.0f, .fixedHeight = 36.0f})) {
                    b3 = ui.Button({.label = "Button 3", .stretch = StretchMode::Fill});
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
        if (auto mainCol = ui.Column({.gap = 16.0f, .padding = Padding::All(24.0f)})) {
            if (auto header = ui.Row({.gap = 12.0f})) {
                ui.Text("GUIKIT GUI", {.role = TextRole::TitleLarge});
                ui.Spacer();
            }
            if (auto navRow = ui.Row({.gap = 8.0f})) {
                if (auto box = ui.Box({.fixedWidth = 120.0f})) {
                    ui.Button({.label = "Overview"});
                }
            }
            if (auto card = ui.Box({.gap = 16.0f, .padding = Padding::All(20.0f)})) {
                ui.Text("Card title", {.role = TextRole::HeadlineMedium});
                if (auto wrap = ui.Wrap({.gap = 12.0f, .runGap = 8.0f})) {
                    ui.Button({.label = "Btn 1"});
                    ui.Button({.label = "Btn 2"});
                }
            }
            ui.Spacer();
            if (auto footer = ui.Row({.gap = 12.0f})) {
                ui.Text("Footer text", {.role = TextRole::LabelMedium, .variant = UiVariant::Muted});
                ui.Spacer();
                if (auto box = ui.Box({.fixedWidth = 80.0f})) {
                    exitBtn = ui.Button({.label = "Exit"});
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
            btnEnabled = ui.Button({.label = "Active"});
            btnDirectDisabled = ui.Button({.label = "Local Disabled", .disabled = true});

            ui.Disabled(true, [&] {
                btnScopedDisabled = ui.Button({.label = "Scope Disabled"});
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
        if (auto col = ui.Column({.gap = 12.0f})) {
            if (auto slotLg = ui.Box({.size = UiSize::Lg})) {
                btnLarge = ui.Button({.label = "Large Action", .variant = UiVariant::Primary, .stretch = StretchMode::Fill});
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
        if (auto row = ui.Row({.gap = 8.0f})) {
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
    if (auto col = ui.Column({.gap = 8.0f})) {
        ui.Text(ui.Format("Value: {}", 99));
        ui.Button({.label = ui.String("Action")});
    }
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    if (auto col = ui.Column({.gap = 8.0f})) {
        ui.Text(ui.Format("Value: {}", 100));
        ui.Button({.label = ui.String("Action")});
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
        if (auto card = ui.Box({.gap = 16.0f, .padding = Padding::All(10.0f)})) {
            b1 = ui.Button({.label = "First Item"});
            b2 = ui.Button({.label = "Second Item"});
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
        if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(16.0f)})) {
            rDisp = ui.Text("Display Title", {.role = TextRole::DisplayLarge});
            rHead = ui.Text("Headline Section", {.role = TextRole::HeadlineMedium, .variant = UiVariant::Primary});
            rTitle = ui.Text("Title Header", {.role = TextRole::TitleLarge});
            rSub = ui.Text("Subtitle Header", {.role = TextRole::TitleMedium});
            rBody = ui.Text("Standard body text content", {.role = TextRole::BodyMedium});
            rCap = ui.Text("Muted caption info", {.role = TextRole::LabelMedium, .variant = UiVariant::Muted});
            rCode = ui.Text("auto x = 42;", {.role = TextRole::CodeMedium});
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

    static_assert(sizeof(SwitchOptions) <= 400);
    static_assert(sizeof(CheckboxOptions) <= 320);

    const auto theme = UiTheme::Dark();
    // 1. Test measurement
    const SwitchMetrics sm = theme.GetSwitch(UiSize::Md);
    Vec2 sMeasNoLabel = Switch::Measure({}, UiSize::Md);
    CHECK(sMeasNoLabel.x == sm.trackSize.x);
    CHECK(sMeasNoLabel.y == sm.trackSize.y);

    Vec2 sMeasWithLabel = Switch::Measure({.label = "Enable"}, UiSize::Md);
    CHECK(sMeasWithLabel.x > sm.trackSize.x);

    Vec2 sMeasWithDesc = Switch::Measure({.label = "Enable", .description = "Details"}, UiSize::Md);
    CHECK(sMeasWithDesc.y > sMeasWithLabel.y);

    Vec2 sMeasBoth = Switch::Measure({
        .textLeft = "Dark",
        .textRight = "Light",
        .symmetric = true
    }, UiSize::Md);
    CHECK(sMeasBoth.x > sMeasWithLabel.x);

    Vec2 sMeasBothDesc = Switch::Measure({
        .label = "Theme",
        .textLeft = "Dark",
        .textRight = "Light",
        .description = "Switch theme",
        .symmetric = true
    }, UiSize::Md);
    CHECK(sMeasBothDesc.y > sMeasBoth.y);

    const CheckboxMetrics cm = theme.GetCheckbox(UiSize::Md);
    Vec2 cMeasNoLabel = Checkbox::Measure({}, UiSize::Md);
    CHECK(cMeasNoLabel.x == cm.boxSize);
    CHECK(cMeasNoLabel.y == cm.boxSize);

    Vec2 cMeasWithLabel = Checkbox::Measure({.label = "Enable"}, UiSize::Md);
    CHECK(cMeasWithLabel.x > cm.boxSize);

    Vec2 cMeasWithDesc = Checkbox::Measure({.label = "Enable", .description = "Details"}, UiSize::Md);
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
    bool bothState = false;
    bool optSwitchState = false;
    bool optCheckState = false;

    Response rSwitch;
    Response rCheck;
    Response rToggle;
    Response rIndeterminate;
    Response rDesc;
    Response rStacked;
    Response rBoth;
    Response rOptSwitch;
    Response rOptCheck;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
            rSwitch = ui.Switch(switchState, {.label = "Dark Mode"});
            rCheck = ui.Checkbox(checkState, {.label = "VSync"});
            rToggle = ui.Switch(toggleState, {.label = "Turbo Mode", .inlineOrder = InlineOrder::LabelFirst});
            rIndeterminate = ui.Checkbox(false, {.label = "Select All", .indeterminate = true});
            rDesc = ui.Switch(descState, {
                .label = "Auto Breakdown",
                .description = "Automatic stop of testing on load drop"
            });
            rStacked = ui.Switch(stackedState, {
                .label = "Stacked Mode",
                .description = "Description below switch control",
                .layout = ElementLayout::Stacked
            });
            rBoth = ui.Switch(bothState, {
                .textLeft = "Dark",
                .textRight = "Light",
                .layout = ElementLayout::Inline,
                .symmetric = true
            });
            rOptSwitch = ui.Switch(optSwitchState, {.label = "Option Switch"});
            rOptCheck = ui.Checkbox(optCheckState, {.label = "Option Checkbox"});
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
    CHECK(rBoth.rect.w > 0.0f);
    CHECK(!switchState);
    CHECK(!checkState);
    CHECK(!descState);
    CHECK(!stackedState);
    CHECK(!bothState);

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
        if (auto col = ui.Column({.gap = 16.0f, .padding = Padding::All(20.0f)})) {
            // Numeric field with unit and description (as in user screenshot)
            rSpeed = ui.InputField(speedDrop, {
                .label = "Speed Drop dF/dt",
                .description = "Automatic stop of testing on load drop",
                .suffix = "%",
                .valueFormat = "%.0f",
                .layout = ElementLayout::Stacked
            });

            // Integer count with unit suffix
            rCount = ui.InputField(count, {
                .label = "Cycle Count",
                .suffix = "cycles",
                .layout = ElementLayout::Stacked
            });

            // Text input with placeholder
            rName = ui.InputField(sampleName, {
                .label = "Sample ID",
                .placeholder = "Enter identifier...",
                .layout = ElementLayout::Stacked
            });

            // Field with label set exclusively via InputFieldOptions
            rOptInput = ui.InputField(optInputVal, {
                .label = "Tolerance Margin",
                .suffix = "mm",
                .layout = ElementLayout::Stacked
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

    // 1. Click speed field to focus (click past end of "50" so cursor is placed at end)
    ui.OnMouseMove({rSpeed.rect.x + 100.0f, rSpeed.rect.y + 25.0f});
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

    // 4. Click name field (past end of "Specimen-A") and edit string
    ui.OnMouseMove({rName.rect.x + 150.0f, rName.rect.y + 25.0f});
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

    // 4a. LMB click at the very start of the text (x + 12.0f -> index 0) and insert prefix
    ui.OnMouseMove({rName.rect.x + 12.0f, rName.rect.y + 25.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    ui.OnMouseUp(1);
    ui.OnTextInput("ID:");
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(sampleName == "ID:Specimen-A-X");

    // 4b. Keyboard selection, Clipboard (Ctrl+A, Ctrl+C, Ctrl+X, Ctrl+V), and Undo (Ctrl+Z)
    ui.OnKeyDown(Key::A, {.ctrl = true});
    ui.OnKeyDown(Key::C, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(ui.GetClipboardText() == "ID:Specimen-A-X");

    // Select last 3 chars ("A-X") using End then Shift+Left x3, then Ctrl+X to cut
    ui.OnKeyDown(Key::End);
    ui.OnKeyDown(Key::Left, {.shift = true});
    ui.OnKeyDown(Key::Left, {.shift = true});
    ui.OnKeyDown(Key::Left, {.shift = true});
    ui.OnKeyDown(Key::X, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(ui.GetClipboardText() == "A-X");
    CHECK(sampleName == "ID:Specimen-");

    // Undo (Ctrl+Z) restores "ID:Specimen-A-X"
    ui.OnKeyDown(Key::Z, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(sampleName == "ID:Specimen-A-X");

    // Paste (Ctrl+V) at end with newline sanitization
    ui.SetClipboardText("-99\r\n");
    ui.OnKeyDown(Key::End);
    ui.OnKeyDown(Key::V, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(sampleName == "ID:Specimen-A-X-99");

    // Word deletion via Ctrl+Backspace (deletes "99", then "-")
    ui.OnKeyDown(Key::Backspace, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(sampleName == "ID:Specimen-A-X-");

    ui.OnKeyDown(Key::Backspace, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(sampleName == "ID:Specimen-A-X");

    // 4c. LMB drag selection: press at start (x + 12) and drag to right (x + 200) to select all, then replace
    ui.OnMouseMove({rName.rect.x + 12.0f, rName.rect.y + 25.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseMove({rName.rect.x + 200.0f, rName.rect.y + 25.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    size_t selS = 0, selE = 0;
    CHECK(ui.GetTextEditSelection(rName.id, selS, selE));
    CHECK(selS == 0);
    CHECK(selE == sampleName.size());

    ui.OnMouseUp(1);
    ui.OnTextInput("Specimen-A-X");
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(sampleName == "Specimen-A-X");

    // 4d. Double-click word selection on "Specimen" (near start at x + 20.0f)
    ui.SetCurrentTime(10.0);
    ui.OnMouseMove({rName.rect.x + 20.0f, rName.rect.y + 25.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.AdvanceTime(0.10); // within 0.35s double-click threshold
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(ui.GetTextEditSelection(rName.id, selS, selE));
    CHECK(selS == 0);
    CHECK(selE == 8); // "Specimen"

    // 4e. UTF-8 Cyrillic editing & word navigation
    sampleName = "Привет мир";
    ui.ClearFocus();
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Focus and place cursor at end of "Привет мир"
    ui.AdvanceTime(1.0);
    ui.OnMouseMove({rName.rect.x + 200.0f, rName.rect.y + 25.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    ui.OnMouseUp(1);

    // Backspace deletes single 2-byte UTF-8 character 'р' -> "Привет ми"
    ui.OnKeyDown(Key::Backspace);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(sampleName == "Привет ми");

    // Ctrl+Backspace deletes the whole Cyrillic word "ми" -> "Привет "
    ui.OnKeyDown(Key::Backspace, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(sampleName == "Привет ");

    // Restore sampleName to "Specimen-A-X"
    sampleName = "Specimen-A-X";

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
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(20.0f)})) {
            rBaud = ui.Select(baudIndex, {"9600", "19200", "38400", "115200"}, {
                .label = "Baud Rate",
                .description = "Serial bus communication speed",
                .layout = ElementLayout::Stacked
            });

            rMode = ui.Select(modeIndex, {"Auto", "Manual", "Direct"}, {
                .label = "Control Mode",
                .disabled = disabledMode
            });

            rProfile = ui.Select(profile, {"Eco", "Standard", "Performance", "Extreme"}, {
                .label = "Profile"
            });
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
        rLim = ui.Select(limIdx, {"Item 1", "Item 2", "Item 3", "Item 4", "Item 5"}, {
            .label = "Limited Dropdown",
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
        // 1. Standalone Grid with 12-column slots
        if (auto grid = ui.Grid({.gap = 12.0f})) {
            if (auto c1 = grid.Cell6()) {
                sw1Res = ui.Switch(switchVal1, {.label = "Sensor A"});
            }
            if (auto c2 = grid.Cell6()) {
                sw2Res = ui.Switch(switchVal2, {.label = "Sensor B"});
            }
            if (auto c3 = grid.Cell12()) {
                inp1Res = ui.InputField(speedVal, {.label = "Speed Target"});
            }
        }

        // 2. High-level Card surface containing an explicit Grid layout
        if (auto card = ui.Card()) {
            ui.Heading({.title = "Motion Controller", .subtitle = "Direct axis tuning"});
            if (auto grid = ui.Grid()) {
                if (auto c1 = grid.Cell6()) {
                    inp2Res = ui.InputField(nameVal, {.label = "Device Name"});
                }
                if (auto c2 = grid.Cell6()) {
                    ui.Button({.label = "Calibrate Zero"});
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
                btn1Res = ui.Button({.label = "Top Item", .stretch = StretchMode::Fill});
            }
            if (auto box2 = ui.Box({.heightMode = SizeMode::Fixed, .fixedHeight = 200.0f})) {
                btn2Res = ui.Button({.label = "Middle Item"});
            }
            if (auto box3 = ui.Box({.heightMode = SizeMode::Fixed, .fixedHeight = 200.0f})) {
                btn3Res = ui.Button({.label = "Bottom Item"});
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
    if (auto scrollH = ui.ScrollArea({.orientation = Orientation::Horizontal, .gap = 8.0f})) {
        Node* n = scrollH.GetNode();
        CHECK(n != nullptr);
        if (n) {
            CHECK(n->direction == FlexDirection::Row);
            CHECK(n->gap == 8.0f);
        }
    }
    ui.EndFrame(canvas);

    // Regression test: ScrollArea containing a single tall Markdown widget
    // Verify both mouse wheel and scrollbar thumb drag produce dirty damage and update child position immediately
    {
        static constexpr std::string_view kTallMd =
            "# Section 1\n\nParagraph 1.\n\n"
            "## Section 2\n\nParagraph 2.\n\n"
            "## Section 3\n\nParagraph 3.\n\n"
            "```cpp\nint a = 1;\nint b = 2;\nint c = 3;\nint d = 4;\nint e = 5;\n```\n\n"
            "## Section 4\n\nParagraph 4.\n\n"
            "## Section 5\n\nParagraph 5.\n\n"
            "## Section 6\n\nParagraph 6.\n";

        Node* scrollNode = nullptr;
        auto renderMdScroll = [&]() {
            if (auto sc = ui.ScrollArea({.padding = Padding::All(8.0f)})) {
                scrollNode = sc.GetNode();
                ui.Markdown(kTallMd);
            }
        };

        // Settle initial frames in a 600x220 viewport so Markdown (~500px tall) overflows vertically
        for (int i = 0; i < 3; ++i) {
            ui.BeginFrame({600.0f, 220.0f});
            renderMdScroll();
            ui.EndFrame(canvas);
        }
        CHECK(!ui.HasDirtyDamage());
        CHECK(scrollNode != nullptr);
        CHECK(scrollNode->scrollContentSize.y > 220.0f);
        CHECK_NEAR(scrollNode->scrollOffset.y, 0.0f, 0.01f);

        // 1. Mouse wheel scroll over Markdown must scroll and produce dirty damage
        ui.OnMouseMove({200.0f, 100.0f});
        ui.OnMouseWheel(0.0f, -2.0f);
        ui.BeginFrame({600.0f, 220.0f});
        renderMdScroll();
        ui.EndFrame(canvas);

        CHECK(ui.HasDirtyDamage());
        CHECK(scrollNode->scrollOffset.y > 10.0f);
        const float wheelScrollY = scrollNode->scrollOffset.y;

        // 2. Drag vertical scrollbar thumb further down -> must update scrollOffset and child computedRect in same frame
        const float sbX = 600.0f - sm.margin - sm.trackWidth * 0.5f;
        const float trackLen = 220.0f - 2.0f * sm.margin;
        const float maxScroll = scrollNode->scrollContentSize.y - 220.0f;
        const float thumbLen = std::clamp((220.0f / scrollNode->scrollContentSize.y) * trackLen, sm.minThumbLength, trackLen);
        const float travel = std::max(0.0f, trackLen - thumbLen);
        const float thumbY = sm.margin + (wheelScrollY / maxScroll) * travel + thumbLen * 0.5f;

        ui.OnMouseMove({sbX, thumbY});
        ui.OnMouseDown(1);
        ui.BeginFrame({600.0f, 220.0f});
        renderMdScroll();
        ui.EndFrame(canvas);
        CHECK(scrollNode->scrollbarActive);

        ui.OnMouseMove({sbX, thumbY + 40.0f});
        ui.BeginFrame({600.0f, 220.0f});
        renderMdScroll();
        ui.EndFrame(canvas);

        CHECK(ui.HasDirtyDamage());
        CHECK(scrollNode->scrollOffset.y > wheelScrollY + 10.0f);
        CHECK(scrollNode->firstChild != nullptr);
        if (scrollNode->firstChild) {
            CHECK_NEAR(scrollNode->firstChild->computedRect.y, 8.0f - scrollNode->scrollOffset.y, 0.5f);
        }

        ui.OnMouseUp(1);
        ui.BeginFrame({600.0f, 220.0f});
        renderMdScroll();
        ui.EndFrame(canvas);
    }
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

    // Mouse movement wakes up reactive loop for 1 frame
    ui.OnMouseMove({25.0f, 25.0f});
    CHECK(ui.GetRepaintFramesLeft() == 1);
    CHECK(ui.NeedsRepaint());
    CHECK(ui.GetRecommendedSleepTimeoutMs() == 0);

    // Drain frame
    ui.BeginFrame({100.0f, 100.0f});
    ui.EndFrame(canvas);
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
                ui.Button({.label = "Test Button"});
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
            ui.TabBar(tabIdx, {{"Tab1"}, {"Tab2"}}, {.style = currentTabStyle});
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

    // Frame 8: Slider dragging produces dirty damage while dragging
    float sliderVal = 10.0f;
    auto renderSlider = [&]() {
        if (auto b = ui.Box({.fixedWidth = 300.0f})) {
            ui.Slider(sliderVal, 0.0f, 100.0f);
        }
    };

    // Initial render & settle layout completely
    for (int i = 0; i < 3; ++i) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderSlider();
        ui.EndFrame(canvas);
    }
    CHECK(!ui.HasDirtyDamage());

    // Mouse down on slider produces dirty damage
    ui.OnMouseMove({150.0f, 20.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderSlider();
    ui.EndFrame(canvas);
    CHECK(ui.HasDirtyDamage());
    CHECK(!ui.IsFullRepaint());

    // Moving mouse while dragging MUST produce dirty damage on every frame!
    ui.OnMouseMove({220.0f, 20.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderSlider();
    ui.EndFrame(canvas);
    CHECK(ui.HasDirtyDamage());
    CHECK(!ui.IsFullRepaint());

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderSlider();
    ui.EndFrame(canvas);
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
            ui.TabBar(pillIdx, {{"Alpha"}, {"Beta"}, {"Gamma"}}, {.style = TabBarStyle::Pill});
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

    // Frame: Test TabBar with custom backgroundColor, borderColor, radius, and padding
    int styledIdx = 0;
    Response styledRes;
    const Color barBg(30, 32, 40, 255);
    const Color barBorder(80, 85, 95, 255);
    auto renderStyledTabBar = [&]() {
        if (auto col = ui.Column({.padding = Padding::All(10.0f)})) {
            styledRes = ui.TabBar(styledIdx, {{"First"}, {"Second"}, {"Third"}}, {
                .style = TabBarStyle::Pill,
                .fullWidth = true,
                .padding = Padding::All(4.0f),
                .borderRadius = 10.0f,
                .backgroundColor = barBg,
                .borderColor = barBorder,
                .borderWidth = 1.5f
            });
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderStyledTabBar();
    ui.EndFrame(canvas);

    // Click on "Third" tab (index 2, right third of 800px fullWidth bar -> x ~ 650, y ~ 28)
    ui.OnMouseMove({650.0f, 28.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderStyledTabBar();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderStyledTabBar();
    ui.EndFrame(canvas);
    CHECK(styledIdx == 2);
    CHECK(styledRes.changed);

    // Frame: Test dirty rect damage tracking on TabBar backgroundColor change
    Color dynamicBg(20, 20, 20, 255);
    auto renderDynamicBgTabBar = [&]() {
        if (auto col = ui.Column({.padding = Padding::All(10.0f)})) {
            ui.TabBar(styledIdx, {{"One"}, {"Two"}}, {
                .style = TabBarStyle::Pill,
                .backgroundColor = dynamicBg
            });
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderDynamicBgTabBar();
    ui.EndFrame(canvas);

    // Idle frame: no changes -> HasDirtyDamage must be false
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderDynamicBgTabBar();
    ui.EndFrame(canvas);
    CHECK(!ui.HasDirtyDamage());

    // Next frame: change backgroundColor -> must generate dirty damage
    dynamicBg = Color(50, 60, 70, 255);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderDynamicBgTabBar();
    ui.EndFrame(canvas);
    CHECK(ui.HasDirtyDamage());
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
            btnNew = tb.ToolButton({.label = "New", .icon = Icon::Plus()});
            btnSave = tb.ToolButton({.label = "Save", .icon = Icon::Check(), .disabled = true});
            tb.Separator();
            btnSettings = tb.ToolButton({.icon = Icon::Settings(), .active = toolSettingsActive});
            tb.Spacer();
            tb.ToolButton({.label = "Status", .icon = Icon::Eye()});
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
        if (auto row = ui.Row({.gap = 8.0f})) {
            btn = ui.Button({
                .label = {state ? "VSync: ON" : "VSync: OFF", {"VSync: ON", "VSync: OFF"}}
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

    // Also verify Slider and StepSlider intrinsic width stability across min/max and variants
    {
        static constexpr std::array<std::string_view, 2> autoPxVars = {"Auto", "%.0f px"};
        SliderOptions slOpts{.valueFormat = {"Auto", autoPxVars}, .showValue = true, .stretch = StretchMode::Compact};
        const Vec2 szAuto = Slider::Measure(slOpts, 0.0, 0.0, 400.0, UiSize::Md, &canvas, ui.GetTheme());
        slOpts.valueFormat = {"%.0f px", autoPxVars};
        const Vec2 szPx = Slider::Measure(slOpts, 400.0, 0.0, 400.0, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(szAuto.x, szPx.x, 0.001f);

        static constexpr std::array<std::string_view, 3> shortSteps = {"A", "B", "C"};
        static constexpr std::array<std::string_view, 3> fullNames = {"Short", "Medium", "ExtraLongVariantName"};
        StepSliderOptions stOpts{
            .stepLabels = shortSteps,
            .showValue = true,
            .valueText = {"Short", fullNames},
            .stretch = StretchMode::Compact
        };
        const Vec2 st0 = StepSlider::Measure(stOpts, 0, 3, UiSize::Md, &canvas, ui.GetTheme());
        stOpts.valueText = {"ExtraLongVariantName", fullNames};
        const Vec2 st2 = StepSlider::Measure(stOpts, 2, 3, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(st0.x, st2.x, 0.001f);

        // Verify InputField Compact auto-width calculation from valueFormat variants (and that placeholder does not affect width)
        static constexpr std::array<std::string_view, 2> shortNumVars = {"0", "99"};
        static constexpr std::array<std::string_view, 2> longEndpointVars = {"localhost", "https://production-cluster.guikit.dev:8443"};
        InputFieldOptions inShortOpts{
            .placeholder = "Very long placeholder hint that must not inflate compact box width...",
            .suffix = "%",
            .valueFormat = {"%.0f", shortNumVars},
            .stretch = StretchMode::Compact
        };
        InputFieldOptions inLongOpts{
            .placeholder = "Host...",
            .prefix = "https://",
            .valueFormat = {longEndpointVars},
            .stretch = StretchMode::Compact
        };
        const Vec2 szShortIn = InputField::Measure("5", inShortOpts, UiSize::Md, &canvas, ui.GetTheme());
        const Vec2 szShortIn2 = InputField::Measure("99", inShortOpts, UiSize::Md, &canvas, ui.GetTheme());
        const Vec2 szLongIn = InputField::Measure("localhost", inLongOpts, UiSize::Md, &canvas, ui.GetTheme());
        const float defInW = ui.GetTheme().GetInputField(UiSize::Md).defaultWidth;

        // Short numeric compact input with variants must be narrower than defaultWidth (160pt) even with a long placeholder
        CHECK(szShortIn.x < defInW);
        CHECK_NEAR(szShortIn.x, szShortIn2.x, 0.001f);
        // Long endpoint compact input with variants must expand beyond defaultWidth to fit the longest variant
        CHECK(szLongIn.x > defInW);
    }
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
                btnLg = ui.Button({.label = "Large Action"});
                swLg = ui.Switch(switchVal, {.label = "Large Switch"});
                cbLg = ui.Checkbox(checkVal, {.label = "Large Checkbox"});
                inLg = ui.InputField(inputVal, {.label = "Large Input"});
                selLg = ui.Select(selectIdx, {"One", "Two"}, {.label = "Large Select"});
            } else {
                btnSm = ui.Button({.label = "Small Action"});
                swSm = ui.Switch(switchVal, {.label = "Small Switch"});
                cbSm = ui.Checkbox(checkVal, {.label = "Small Checkbox"});
                inSm = ui.InputField(inputVal, {.label = "Small Input"});
                selSm = ui.Select(selectIdx, {"One", "Two"}, {.label = "Small Select"});
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
            btnCompact = ui.Button({.label = "Compact", .stretch = StretchMode::Compact});
            btnStretch1 = ui.Button({.label = "Stretch", .stretch = StretchMode::Fill});
            btnStretch2 = ui.Button({.label = "Stretch", .stretch = StretchMode::Fill});
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
            b1 = ui.Button({.label = "First"});
            b2 = ui.Button({.label = "Second"});
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
    ui.GetTheme().Spacing().md = 25.0f;
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
                btnInherit = ui.Button({.label = "Inherit"});
                btnXl = ui.Button({.label = "Large", .size = UiSize::Xl});
                btnXs = ui.Button({.label = "Small", .size = UiSize::Xs});
            }

            // Switches: Inherited (Md), Local override Xl, Local override Xs
            if (auto r2 = ui.Row()) {
                swInherit = ui.Switch(swVal, {.label = "Inherit"});
                swXl = ui.Switch(swVal, {.label = "Large", .size = UiSize::Xl});
                swXs = ui.Switch(swVal, {.label = "Small", .size = UiSize::Xs});
            }

            // Checkboxes: Inherited (Md), Local override Xl, Local override Xs
            if (auto r3 = ui.Row()) {
                cbInherit = ui.Checkbox(cbVal, {.label = "Inherit"});
                cbXl = ui.Checkbox(cbVal, {.label = "Large", .size = UiSize::Xl});
                cbXs = ui.Checkbox(cbVal, {.label = "Small", .size = UiSize::Xs});
            }

            // InputFields: Inherited (Md), Local override Xl, Local override Xs
            if (auto r4 = ui.Row()) {
                inInherit = ui.InputField(inVal, {.label = "Inherit"});
                inXl = ui.InputField(inVal, {.label = "Large", .size = UiSize::Xl});
                inXs = ui.InputField(inVal, {.label = "Small", .size = UiSize::Xs});
            }

            // Selects: Inherited (Md), Local override Xl, Local override Xs
            if (auto r5 = ui.Row()) {
                selInherit = ui.Select(selIdx, selItems, {.label = "Inherit"});
                selXl = ui.Select(selIdx, selItems, {.label = "Large", .size = UiSize::Xl});
                selXs = ui.Select(selIdx, selItems, {.label = "Small", .size = UiSize::Xs});
            }

            // Toolbar ToolButtons: Inherited (Md), Local override Xl, Local override Xs
            if (auto tb = ui.Toolbar()) {
                toolInherit = tb.ToolButton({.label = "Inherit", .icon = Icon::Play()});
                toolXl = tb.ToolButton({.label = "Large", .icon = Icon::Play(), .size = UiSize::Xl});
                toolXs = tb.ToolButton({.label = "Small", .icon = Icon::Play(), .size = UiSize::Xs});
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

    Response head1, head2, head3, head4;
    std::string dynamicSubtitle = "Initial Subtitle";
    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
            head1 = ui.Heading({.title = "System Overview", .subtitle = dynamicSubtitle});
            head2 = ui.Heading({
                .title = "Telemetry Stream",
                .subtitle = "1000 samples/sec",
                .icon = Icon::Check(),
                .badge = "ACTIVE",
                .variant = UiVariant::Success
            });
            head3 = ui.Heading({.title = "Section Divider", .divider = true});
            head4 = ui.Heading({.title = "Trailing Divider", .divider = true, .dividerStyle = HeadingDivider::Trailing});
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
    CHECK(head3.id.IsValid());
    CHECK(head4.id.IsValid());

    const Rect r1 = ui.GetPrevFrameBounds(head1.id);
    const Rect r2 = ui.GetPrevFrameBounds(head2.id);
    const Rect r3 = ui.GetPrevFrameBounds(head3.id);
    const Rect r4 = ui.GetPrevFrameBounds(head4.id);

    // Width should stretch across container (800 - 32 padding = 768)
    CHECK_NEAR(r1.w, 768.0f, 1.0f);
    CHECK_NEAR(r2.w, 768.0f, 1.0f);
    CHECK_NEAR(r3.w, 768.0f, 1.0f);
    CHECK_NEAR(r4.w, 768.0f, 1.0f);
    CHECK(r1.h > 0.0f);
    CHECK(r2.h > 0.0f);
    CHECK(r3.h > 0.0f);
    CHECK(r4.h > 0.0f);
    CHECK(r2.y >= r1.Bottom());
    CHECK(r3.y >= r2.Bottom());
    CHECK(r4.y >= r3.Bottom());

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
    ui.Button({.label = "TestBtn"});
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
    ui.Button({.label = "TestBtn"});
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
    ui.Button({.label = "GPU Button"});
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
                    ui.Text("GUIKIT GUI", {.role = TextRole::TitleLarge, .variant = UiVariant::Info});
                    ui.Text("| Autonomous Vector GUI Engine", {.role = TextRole::LabelMedium, .variant = UiVariant::Muted});
                    ui.Spacer();
                    ui.Text("C++20 Immediate + ThorVG", {.role = TextRole::LabelMedium, .variant = UiVariant::Muted});
                }

                if (auto tb = ui.Toolbar()) {
                    tb.ToolButton({.label = "Density: Standard"});
                    tb.Separator();
                    tb.ToolButton({.label = "Scale: 100%"});
                    tb.Separator();
                    tb.ToolButton({.label = "Theme: Dark"});
                }

                const std::vector<TabItem> navTabs = {
                    {.label = "Overview", .icon = Icon::Eye()},
                    {.label = "Flexbox Demo", .icon = Icon::Settings()},
                    {.label = "Industrial Dashboard", .icon = Icon::Folder(), .badge = "Live"}
                };
                ui.TabBar(activeTabIndex, navTabs);

                if (activeTabIndex == 2) {
                    if (auto scroll = ui.ScrollArea()) {
                        if (auto dashboardGrid = ui.Grid()) {
                            if (auto colLeft = dashboardGrid.Cell6()) {
                                if (auto card1 = ui.Card()) {
                                    cardId = card1.GetNode()->id;
                                    ui.Heading({.title = "Primary Stop Conditions", .subtitle = "Sensory tripwire thresholds & limits"});
                                    if (auto grid1 = ui.Grid()) {
                                        if (auto c = grid1.Cell12()) {
                                            ui.Switch(enableSensors, {
                                                .label = "Enable Sensor Drop Limits",
                                                .description = "Immediate stop on threshold breach"
                                            });
                                        }
                                        if (auto c = grid1.Cell6()) {
                                            ui.InputField(maxTravel, {.label = "Max Travel", .suffix = "mm", .valueFormat = "%.1f"});
                                        }
                                        if (auto c = grid1.Cell6()) {
                                            ui.InputField(maxVelocity, {.label = "Max Velocity", .suffix = "mm/s", .valueFormat = "%.1f"});
                                        }
                                        if (auto c = grid1.Cell12()) {
                                            ui.Select(sensorChannel, {
                                                "Channel A (Load Cell 50kN)",
                                                "Channel B (Optical Extensometer)",
                                                "Channel C (Laser Interferometer)"
                                            }, {
                                                .label = "Primary Sensor Channel",
                                                .description = "Active physical transducer bus",
                                                .layout = ElementLayout::Stacked
                                            });
                                        }
                                    }
                                }

                                if (auto card2 = ui.Card()) {
                                    ui.Heading({.title = "Emergency Interlocks", .subtitle = "Hardware line supervision & failsafes", .variant = UiVariant::Danger});
                                    if (auto grid2 = ui.Grid()) {
                                        if (auto c = grid2.Cell12()) {
                                            ui.Switch(hwEstop, {
                                                .label = "Hardware E-Stop Line",
                                                .description = "Continuous 24V loop monitoring",
                                                .variant = UiVariant::Danger
                                            });
                                        }
                                        if (auto c = grid2.Cell6()) {
                                            ui.Select(interlockMode, {
                                                "Immediate Cutoff",
                                                "Controlled Ramp-down",
                                                "Hold Position"
                                            }, {
                                                .label = "Interlock Action"
                                            });
                                        }
                                        if (auto c = grid2.Cell6()) {
                                            ui.InputField(rampDown, {.label = "Ramp Duration", .suffix = "ms", .valueFormat = "%.0f"});
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
        if (auto c = ui.Carousel({.gap = 8.0f})) {
            carouselNode = c.GetNode();
            b1 = ui.Button({.label = "Button 1"});
            b2 = ui.Button({.label = "Button 2"});
            b3 = ui.Button({.label = "Button 3"});
            ui.Button({.label = "Button 4"});
            ui.Button({.label = "Button 5"});
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
    if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(12.0f)})) {
        if (auto row = ui.Row({.gap = 12.0f})) {
            ui.Button({.label = "Inspect Me"});
            ui.Spacer();
            ui.Switch(false, {.label = "Toggle Switch"});
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

        ui.Button({.label = "Child Button"});
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
    if (auto col = ui.Column({.gap = 12.0f})) {
        if (auto row = ui.Row({.gap = 8.0f, .align = Alignment::Center})) {
            ui.Button({.label = "Btn 1"});
            ui.Button({.label = "Btn 2"});
        }
        if (auto wrap = ui.Wrap({.gap = 4.0f, .runGap = 8.0f})) {
            ui.Button({.label = "W1"});
            ui.Button({.label = "W2"});
        }
        if (auto grid = ui.Grid({.columns = 12, .gap = 12.0f})) {
            if (auto c6 = grid.Cell6()) {
                ui.Text("Half Width Cell");
            }
            if (auto c5 = grid.Cell(5)) {
                CHECK(c5.GetNode() != nullptr);
                CHECK(c5.GetNode()->colSpan == 5);
                ui.Text("Span 5 Cell");
            }
            if (auto c7 = grid.Cell(7)) {
                CHECK(c7.GetNode() != nullptr);
                CHECK(c7.GetNode()->colSpan == 7);
                ui.Text("Span 7 Cell");
            }
        }
    }

    // 2. Panels: Box, Card, ScrollArea, Carousel
    if (ContainerScope box = ui.Box({.gap = 8.0f, .padding = Padding::All(8.0f), .borderRadius = 6.0f})) {
        Node* node = box.GetNode();
        CHECK(node != nullptr);
        CHECK(node->cornerRadius == 6.0f);
        CHECK(node->padding.left == 8.0f);
        ui.Text("Inside Box");
    }

    if (auto card = ui.Card({.gap = 12.0f, .padding = Padding::All(16.0f), .borderRadius = 8.0f})) {
        Node* node = card.GetNode();
        CHECK(node != nullptr);
        CHECK(node->cornerRadius == 8.0f);
        CHECK(node->padding.top == 16.0f);
        ui.Text("Inside Card");
    }

    if (auto scroll = ui.ScrollArea({.orientation = Orientation::Vertical, .gap = 8.0f, .padding = Padding::Symmetric(12.0f, 0.0f)})) {
        Node* node = scroll.GetNode();
        CHECK(node != nullptr);
        CHECK(node->padding.left == 12.0f);
        CHECK(node->gap == 8.0f);
        ui.Button({.label = "Scroll Item 1"});
    }

    if (auto carousel = ui.Carousel({.gap = 16.0f, .padding = Padding::All(6.0f), .align = Alignment::End, .justify = Justify::Center})) {
        Node* node = carousel.GetNode();
        CHECK(node != nullptr);
        CHECK(node->align == Alignment::End);
        CHECK(node->justify == Justify::Center);
        CHECK(node->padding.left == 6.0f);
        CHECK(node->gap == 16.0f);
        ui.Button({.label = "Carousel Item 1"});
    }

    // 3. Static layout facades: Row::Open, Column::Open, Wrap::Open, Grid::Open, Spacer::Run
    if (auto col2 = Column::Open(ui, {.gap = 8.0f})) {
        if (auto row2 = Row::Open(ui)) {
            ui.Button({.label = "Static Row Btn"});
            Spacer::Run(ui);
            ui.Button({.label = "Static Row Btn 2"});
        }
        if (auto wrap2 = Wrap::Open(ui)) {
            ui.Button({.label = "Static Wrap Btn"});
        }
        if (auto grid2 = Grid::Open(ui)) {
            if (auto cell = grid2.Cell()) {
                ui.Text("Static Grid Cell");
            }
        }
    }

    ui.EndFrame(canvas);
}

namespace {
using namespace guikit;

// Definition of a 3rd party user-defined widget
struct MyCustomBadge {
    static Response Run(Context& ctx, std::string_view text, UiVariant variant = UiVariant::Success, std::source_location loc = std::source_location::current()) {
        const Id id = ctx.MakeChildId(loc);
        Node* node = ctx.CreateNode(NodeType::Custom, id);
        node->text = ctx.AllocString(text);
        node->textColor = ctx.GetTheme().palette.textPrimary;
        node->widthMode = SizeMode::Fit;
        node->heightMode = SizeMode::Fit;
        ctx.AdaptChildToContainer(node);
        return ctx.Interact(id);
    }
};

// Definition of a 3rd party user-defined container
struct MyCustomPanel {
    static ContainerScope Open(Context& ctx, float minWidth = 100.0f, std::source_location loc = std::source_location::current()) {
        const Id id = ctx.MakeChildId(loc);
        Node* node = ctx.CreateNode(NodeType::Container, id);
        node->direction = FlexDirection::Column;
        node->minWidth = minWidth;
        node->widthMode = SizeMode::Stretch;
        node->heightMode = SizeMode::Fit;
        ctx.PushContainer(node);
        return ContainerScope(&ctx, node);
    }
};
} // namespace

void TestGenericTemplateFacade() {
    using namespace guikit;

    static_assert(WidgetRunnable<MyCustomBadge, std::string_view>);
    static_assert(WidgetRunnable<MyCustomBadge, std::string_view, UiVariant>);
    static_assert(WidgetRunnable<Button, ButtonOptions>);
    static_assert(ContainerOpenable<MyCustomPanel>);
    static_assert(ContainerOpenable<MyCustomPanel, float>);
    static_assert(ContainerOpenable<Row, RowOptions>);
    static_assert(ContainerOpenable<Card, CardOptions>);

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 400;
    const uint32_t height = 300;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    if (auto panel = ui.Open<MyCustomPanel>(150.0f)) {
        CHECK(panel.GetNode() != nullptr);
        CHECK(panel.GetNode()->minWidth == 150.0f);
        auto badge = ui.Run<MyCustomBadge>("LIVE STATUS", UiVariant::Success);
        CHECK(badge.id.IsValid());
        auto btn = ui.Run<Button>(ButtonOptions{.label = "Generic Button"});
        CHECK(btn.id.IsValid());
    }
    ui.EndFrame(canvas);
}

void TestContextCleanArchitectureAndStateGC() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 400;
    const uint32_t height = 300;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    // 1. Verify ActiveEditState buffer text manipulation
    Id inputId(9999);
    ui.RequestFocus(inputId);
    size_t cursor = 0;
    bool changed = false;
    bool submitted = false;

    // Start editing with "Hello"
    std::string_view text1 = ui.GetOrStartTextEdit(inputId, "Hello", cursor, changed, submitted);
    CHECK(text1 == "Hello");
    CHECK(cursor == 5);

    // Test text insertion
    ui.OnTextInput(" World");
    std::string_view text2 = ui.GetOrStartTextEdit(inputId, text1, cursor, changed, submitted);
    CHECK(text2 == "Hello World");
    CHECK(cursor == 11);
    CHECK(changed);

    // Test backspace
    ui.OnKeyDown(Key::Backspace);
    std::string_view text3 = ui.GetOrStartTextEdit(inputId, text2, cursor, changed, submitted);
    CHECK(text3 == "Hello Worl");
    CHECK(cursor == 10);

    // Test Home, Delete
    ui.OnKeyDown(Key::Home);
    ui.OnKeyDown(Key::Delete);
    std::string_view text4 = ui.GetOrStartTextEdit(inputId, text3, cursor, changed, submitted);
    CHECK(text4 == "ello Worl");
    CHECK(cursor == 0);

    // Test Enter submission
    ui.OnKeyDown(Key::Enter);
    ui.GetOrStartTextEdit(inputId, text4, cursor, changed, submitted);
    CHECK(submitted);
    CHECK(ui.GetFocusedId() == Id::Null());

    // 2. Verify State Garbage Collection (Pruning stale states after 600 frames)
    struct TemporaryWidgetState {
        int value{42};
    };

    Id staleId(1111);
    Id activeId(2222);

    // Frame 1: Create both states
    ui.BeginFrame({400.0f, 300.0f});
    ui.GetOrCreateState<TemporaryWidgetState>(staleId).value = 100;
    ui.GetOrCreateState<TemporaryWidgetState>(activeId).value = 200;
    CHECK(ui.GetCustomStateCount() == 2);
    ui.EndFrame(canvas);

    // Run 599 frames keeping activeId refreshed, but leaving staleId untouched
    for (int f = 2; f <= 599; ++f) {
        ui.BeginFrame({400.0f, 300.0f});
        ui.GetOrCreateState<TemporaryWidgetState>(activeId).value = 200 + f;
        ui.EndFrame(canvas);
    }
    CHECK(ui.GetCustomStateCount() == 2); // Not yet pruned at frame 599

    // Frame 600: Pruning trigger! staleId was created at frame 1, so 1 + 600 < 600 is false.
    ui.BeginFrame({400.0f, 300.0f});
    ui.GetOrCreateState<TemporaryWidgetState>(activeId).value = 800;
    ui.EndFrame(canvas);

    // Advance to frame 602 (staleId active frame was 1; 1 + 600 < 602 is true!)
    for (int f = 601; f < 1200; ++f) {
        ui.BeginFrame({400.0f, 300.0f});
        ui.GetOrCreateState<TemporaryWidgetState>(activeId).value = 900;
        ui.EndFrame(canvas);
    }

    // At frame 1200 (next 600-frame periodic GC boundary):
    ui.BeginFrame({400.0f, 300.0f});
    ui.GetOrCreateState<TemporaryWidgetState>(activeId).value = 999;
    ui.EndFrame(canvas);

    // Stale state should now be garbage-collected!
    CHECK(ui.FindState<TemporaryWidgetState>(staleId) == nullptr);
    CHECK(ui.FindState<TemporaryWidgetState>(activeId) != nullptr);
    CHECK(ui.FindState<TemporaryWidgetState>(activeId)->value == 999);
}

void TestScopedThemeAndScaleOverrides() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    UiTheme customTheme = UiTheme::Light();
    customTheme.palette.surface = Color(123, 234, 45, 255);

    UiTheme inspectorTheme = UiTheme::Dark();
    inspectorTheme.palette.surface = Color(10, 20, 30, 255);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)}, 1.0f);

    const float baseBtnHeight = ui.GetTheme().GetMetrics(UiSize::Md).defaultHeight;
    CHECK_NEAR(ui.GetCurrentScale(), 1.0f, 0.001f);

    // 1. Scoped theme override
    if (auto box = ui.Box({.theme = &customTheme})) {
        CHECK(ui.GetTheme().palette.surface == Color(123, 234, 45, 255));
        CHECK_NEAR(ui.GetCurrentScale(), 1.0f, 0.001f);
    }
    // Should restore after box closes
    CHECK(ui.GetTheme().palette.surface != Color(123, 234, 45, 255));

    // 2. Scoped scale override + nested theme override inheriting parent scale
    if (auto col = ui.Column({.scale = 2.0f})) {
        CHECK_NEAR(ui.GetCurrentScale(), 2.0f, 0.001f);
        const float scaledBtnHeight = ui.GetTheme().GetMetrics(UiSize::Md).defaultHeight;
        CHECK_NEAR(scaledBtnHeight, baseBtnHeight * 2.0f, 0.001f);

        // Nested container overriding only .theme must inherit parent scale (2.0f)
        if (auto innerCard = ui.Card({.theme = &customTheme})) {
            CHECK(ui.GetTheme().palette.surface == Color(123, 234, 45, 255));
            CHECK_NEAR(ui.GetCurrentScale(), 2.0f, 0.001f);
            CHECK_NEAR(ui.GetTheme().GetMetrics(UiSize::Md).defaultHeight, baseBtnHeight * 2.0f, 0.001f);
        }
        // After innerCard closes, parent scale and theme remain 2.0f
        CHECK_NEAR(ui.GetCurrentScale(), 2.0f, 0.001f);
        CHECK_NEAR(ui.GetTheme().GetMetrics(UiSize::Md).defaultHeight, baseBtnHeight * 2.0f, 0.001f);
    }
    // Should restore after col closes
    CHECK_NEAR(ui.GetCurrentScale(), 1.0f, 0.001f);
    CHECK_NEAR(ui.GetTheme().GetMetrics(UiSize::Md).defaultHeight, baseBtnHeight, 0.001f);

    // 3. Toolbar inside a scaled container must preserve parent scale after closing
    if (auto shellCol = ui.Column({.scale = 1.5f})) {
        CHECK_NEAR(ui.GetCurrentScale(), 1.5f, 0.001f);
        if (auto tb = ui.Toolbar()) {
            tb.ToolButton({.label = "Action"});
        }
        // Scale must still be 1.5f after ToolbarScope destructor runs
        CHECK_NEAR(ui.GetCurrentScale(), 1.5f, 0.001f);
        CHECK_NEAR(ui.GetTheme().GetMetrics(UiSize::Md).defaultHeight, baseBtnHeight * 1.5f, 0.001f);
    }
    CHECK_NEAR(ui.GetCurrentScale(), 1.0f, 0.001f);

    // 4. Isolated sidebar vs scaled content in row + scoped debugBounds / inspectMode
    if (auto row = ui.Row()) {
        if (auto sidebar = ui.Box({.theme = &inspectorTheme, .scale = 1.0f, .debugBounds = false, .inspectMode = false})) {
            CHECK(ui.GetTheme().palette.surface == Color(10, 20, 30, 255));
            CHECK_NEAR(ui.GetCurrentScale(), 1.0f, 0.001f);
            CHECK_NEAR(ui.GetTheme().GetMetrics(UiSize::Md).defaultHeight, baseBtnHeight, 0.001f);
            CHECK(sidebar.GetNode()->debugBoundsOverride.has_value());
            CHECK(*sidebar.GetNode()->debugBoundsOverride == false);
            CHECK(sidebar.GetNode()->inspectModeOverride.has_value());
            CHECK(*sidebar.GetNode()->inspectModeOverride == false);
        }

        if (auto content = ui.Card({.scale = 1.5f, .debugBounds = true, .inspectMode = true})) {
            CHECK_NEAR(ui.GetCurrentScale(), 1.5f, 0.001f);
            CHECK_NEAR(ui.GetTheme().GetMetrics(UiSize::Md).defaultHeight, baseBtnHeight * 1.5f, 0.001f);
            CHECK(content.GetNode()->debugBoundsOverride.has_value());
            CHECK(*content.GetNode()->debugBoundsOverride == true);
            CHECK(content.GetNode()->inspectModeOverride.has_value());
            CHECK(*content.GetNode()->inspectModeOverride == true);
        }
    }

    // 5. Explicit linear dimensions (dp) scale proportionally with .scale, while Fill % width stays unscaled
    if (auto scaledCol = ui.Column({.gap = 10.0f, .padding = Padding::All(12.0f), .scale = 2.0f})) {
        const Node* colNode = scaledCol.GetNode();
        CHECK(colNode != nullptr);
        CHECK_NEAR(ui.GetTheme().scale, 2.0f, 0.001f);
        CHECK_NEAR(colNode->gap, 20.0f, 0.001f);
        CHECK_NEAR(colNode->padding.left, 24.0f, 0.001f);
        CHECK_NEAR(colNode->padding.top, 24.0f, 0.001f);

        // Token-based padding must not be double-scaled
        const Padding tokenPad = ui.GetTheme().ResolvePadding(Padding(UiSize::Sm));
        CHECK_NEAR(tokenPad.left, ui.GetTheme().GetPadding(UiSize::Sm).left, 0.001f);

        if (auto fixedBox = ui.Box({.fixedWidth = 100.0f, .fixedHeight = 40.0f, .borderWidth = 2.0f, .borderRadius = 6.0f})) {
            const Node* boxNode = fixedBox.GetNode();
            CHECK_NEAR(boxNode->fixedWidth, 200.0f, 0.001f);
            CHECK_NEAR(boxNode->fixedHeight, 80.0f, 0.001f);
            CHECK_NEAR(boxNode->borderWidth, 4.0f, 0.001f);
            CHECK_NEAR(boxNode->cornerRadius, 12.0f, 0.001f);
        }

        if (auto sb = ui.Sidebar({.width = 160.0f, .minWidth = 80.0f, .gap = 8.0f})) {
            const Node* sbNode = sb.GetNode();
            CHECK_NEAR(sbNode->fixedWidth, 320.0f, 0.001f);
            CHECK_NEAR(sbNode->minWidth, 160.0f, 0.001f);
            CHECK_NEAR(sbNode->gap, 16.0f, 0.001f);
        }

        const Vec2 btnCompact = Button::Measure(ButtonOptions{.text = "OK", .stretch = StretchMode::Compact, .width = 120.0f}, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(btnCompact.x, 240.0f, 0.001f);

        const Vec2 inpCompact = InputField::Measure("12", InputFieldOptions{.stretch = StretchMode::Compact, .width = 90.0f}, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(inpCompact.x, 180.0f, 0.001f);

        const Vec2 selCompact = Select::Measure(SelectOptions{.stretch = StretchMode::Compact, .width = 110.0f}, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(selCompact.x, 220.0f, 0.001f);

        // StretchMode::Fill treats .width as percentage (0..100%), which must NOT be multiplied by scale
        const Rect fillBox = Select::GetBoxRect(Rect(0.0f, 0.0f, 400.0f, 60.0f), SelectOptions{.label = "Mode", .layout = ElementLayout::Inline, .stretch = StretchMode::Fill, .width = 50.0f}, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(fillBox.w, 200.0f, 0.01f);
    }

    CHECK_NEAR(ui.GetCurrentScale(), 1.0f, 0.001f);

    ui.EndFrame(canvas);
}

void TestSliderAndStepSliderWidgets() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 400;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    float floatVal = 5.0f;
    int intVal = 4;
    size_t stepIndex = 1;
    UiSize activeSize = UiSize::Sm;
    UiSize activeRadius = UiSize::None;
    static constexpr std::array<std::string_view, 3> stepLabels = {"Low", "Medium", "High"};

    Response r1, r2, r3, r4, r5;
    auto renderUI = [&]() {
        if (auto col = ui.Column()) {
            r1 = ui.Slider(floatVal, 0.0f, 10.0f, {.label = "Gain", .valueFormat = "%.1f dB"});
            r2 = ui.Slider(intVal, 1, 8, {.label = "Columns", .valueFormat = "%d ch"});
            r3 = ui.StepSlider(stepIndex, stepLabels, {.label = "Preset"});
            r4 = ui.StepSlider(activeSize, {.label = "Size"});
            r5 = ui.StepSlider(activeRadius, {.label = "Radius", .includeNone = true});
        }
    };

    // Frame 1: Initial layout and bounds registration
    ui.BeginFrame({400.0f, 600.0f});
    renderUI();
    ui.EndFrame(canvas);

    CHECK_NEAR(floatVal, 5.0f, 0.001f);
    CHECK(intVal == 4);
    CHECK(stepIndex == 1);
    CHECK(activeSize == UiSize::Sm);
    CHECK(activeRadius == UiSize::None);
    const Rect r1Bounds = ui.GetPrevFrameBounds(r1.id);
    CHECK(r1Bounds.w > 0.0f);
    CHECK(r1Bounds.h > 0.0f);

    // Frame 2: Interact with float slider via click/drag
    // Mouse clicked on first slider at roughly 80% of width
    const Vec2 clickPos = {r1Bounds.x + r1Bounds.w * 0.8f, r1Bounds.Center().y};
    ui.OnMouseMove(clickPos);
    ui.OnMouseDown(1);

    ui.BeginFrame({400.0f, 600.0f});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(r1.changed == true);
    CHECK(floatVal > 6.0f); // Was moved towards 80%

    ui.OnMouseUp(1);

    // Frame 3: Mouse wheel on integer slider (r2)
    const Rect r2Bounds = ui.GetPrevFrameBounds(r2.id);
    CHECK(r2Bounds.w > 0.0f);
    ui.OnMouseMove(r2Bounds.Center());
    ui.OnMouseWheel(0.0f, 1.0f); // Scroll up: increase by step 1
    ui.BeginFrame({400.0f, 600.0f});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(r2.changed == true);
    CHECK(intVal == 5);

    // Reset mouse wheel
    ui.OnMouseWheel(0.0f, 0.0f);

    // Frame 4: StepSlider click on 3rd option ("High", index 2)
    const Rect r3Bounds = ui.GetPrevFrameBounds(r3.id);
    CHECK(r3Bounds.w > 0.0f);
    // Click near right edge of step slider track (index 2 of 3)
    const Vec2 stepClickPos = {r3Bounds.x + r3Bounds.w * 0.95f, r3Bounds.Center().y};
    ui.OnMouseMove(stepClickPos);
    ui.OnMouseDown(1);
    ui.BeginFrame({400.0f, 600.0f});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(r3.changed == true);
    CHECK(stepIndex == 2);
    ui.OnMouseUp(1);

    // Frame 5: StepSlider for UiSize
    activeSize = UiSize::Lg;
    activeRadius = UiSize::Md;
    ui.BeginFrame({400.0f, 600.0f});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(activeSize == UiSize::Lg);
    CHECK(activeRadius == UiSize::Md);
}

void TestPixelGeometryAndWidgetUiSizeDecoupling() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    ButtonResponse btnLg;
    ButtonResponse btnSm;

    auto render = [&]() {
        // 1. Explicit arbitrary float pixel values for layout containers
        if (auto row = ui.Row({.gap = 13.5f, .padding = Padding::Symmetric(15.0f, 7.5f)})) {
            Node* n = row.GetNode();
            CHECK(n != nullptr);
            CHECK_NEAR(n->gap, 13.5f, 0.001f);
            CHECK_NEAR(n->padding.left, 15.0f, 0.001f);
            CHECK_NEAR(n->padding.top, 7.5f, 0.001f);
        }

        if (auto col = ui.Column({.gap = 21.25f})) {
            Node* n = col.GetNode();
            CHECK(n != nullptr);
            CHECK_NEAR(n->gap, 21.25f, 0.001f);
        }

        if (auto wrap = ui.Wrap({.gap = 9.0f, .runGap = 14.5f})) {
            Node* n = wrap.GetNode();
            CHECK(n != nullptr);
            CHECK_NEAR(n->gap, 9.0f, 0.001f);
            CHECK_NEAR(n->runGap, 14.5f, 0.001f);
        }

        if (auto grid = ui.Grid({.gap = 11.0f, .runGap = 18.0f})) {
            Node* n = grid.GetNode();
            CHECK(n != nullptr);
            CHECK_NEAR(n->gap, 11.0f, 0.001f);
            CHECK_NEAR(n->runGap, 18.0f, 0.001f);

            if (auto cell = grid.Cell(6, {.gap = 5.5f, .padding = 10.0f})) {
                Node* cn = cell.GetNode();
                CHECK(cn != nullptr);
                CHECK_NEAR(cn->gap, 5.5f, 0.001f);
                CHECK_NEAR(cn->padding.left, 10.0f, 0.001f);
            }
        }

        // 2. Card and Box with float radius and single float padding
        if (auto card = ui.Card({.gap = 7.0f, .padding = 20.0f, .borderRadius = 12.5f})) {
            Node* n = card.GetNode();
            CHECK(n != nullptr);
            CHECK_NEAR(n->gap, 7.0f, 0.001f);
            CHECK_NEAR(n->padding.left, 20.0f, 0.001f);
            CHECK_NEAR(n->cornerRadius, 12.5f, 0.001f);
        }

        if (auto box = ui.Box({.gap = 16.0f, .padding = 8.0f, .borderRadius = 4.0f})) {
            Node* n = box.GetNode();
            CHECK(n != nullptr);
            CHECK_NEAR(n->gap, 16.0f, 0.001f);
            CHECK_NEAR(n->padding.left, 8.0f, 0.001f);
            CHECK_NEAR(n->cornerRadius, 4.0f, 0.001f);
        }

        // 3. Fallback to theme defaults when gap / radius are omitted (nullopt)
        if (auto defaultCard = ui.Card({.size = UiSize::Lg})) {
            Node* n = defaultCard.GetNode();
            CHECK(n != nullptr);
            CHECK_NEAR(n->gap, ui.GetTheme().GetGap(UiSize::Lg), 0.001f);
            CHECK_NEAR(n->cornerRadius, ui.GetTheme().GetRadius(UiSize::Lg), 0.001f);
            CHECK_NEAR(n->padding.left, ui.GetTheme().GetPadding(UiSize::Lg).left, 0.001f);

            // Child inherits UiSize::Lg height (48px)
            btnLg = ui.Button({.label = "Inherited Lg"});

            // Explicit override to UiSize::Sm height (32px)
            btnSm = ui.Button({.label = "Explicit Sm", .size = UiSize::Sm});
        }
    };

    // Frame 1: Measure and layout
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    // Frame 2: Retrieve cached bounds from previous frame
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    CHECK_NEAR(btnLg.rect.h, 48.0f, 0.001f);
    CHECK_NEAR(btnSm.rect.h, 32.0f, 0.001f);
}

void TestTextWrappingAndBackendIndependence() {
    using namespace guikit;
    std::cout << "  - TestTextWrappingAndBackendIndependence...\n";

    // 1. TextUtil unit checks with fallback ratio (canvas = nullptr)
    const float singleW = TextUtil::MeasureWidth("Hello World", 14.0f, nullptr);
    CHECK(singleW > 0.0f);

    // Truncation with ellipsis
    const std::string trunc = TextUtil::TruncateWithEllipsis("Very long string that needs truncation", 60.0f, 14.0f, nullptr);
    CHECK(!trunc.empty());
    CHECK(trunc.ends_with("..."));
    CHECK(TextUtil::MeasureWidth(trunc, 14.0f, nullptr) <= 60.0f + 0.1f);

    // Multi-line word wrap
    const std::string sentence = "Sensory tripwire thresholds & limits for automated closed-loop machinery";
    const auto lines = TextUtil::WrapText(sentence, 120.0f, 12.0f, nullptr);
    CHECK(lines.size() >= 2);
    for (const auto& line : lines) {
        CHECK(TextUtil::MeasureWidth(line, 12.0f, nullptr) <= 120.0f + 1.0f);
    }

    const float wrappedH = TextUtil::MeasureWrappedHeight(sentence, 120.0f, 12.0f, 18.0f, nullptr);
    CHECK_NEAR(wrappedH, static_cast<float>(lines.size()) * 18.0f, 0.01f);

    // 2. Heading wordWrap test: single-line vs multi-line height resolution
    HeadingOptions optsWrap{
        .title = "Primary Stop Conditions",
        .subtitle = "Sensory tripwire thresholds & limits for automated closed-loop machinery",
        .wordWrap = true
    };
    HeadingOptions optsNoWrap{
        .title = "Primary Stop Conditions",
        .subtitle = "Sensory tripwire thresholds & limits for automated closed-loop machinery",
        .wordWrap = false
    };

    const float hNoWrap = Heading::Measure(optsNoWrap, UiSize::Md, 150.0f).y;
    const float hWrap = Heading::Measure(optsWrap, UiSize::Md, 150.0f).y;
    // Multi-line wrapped heading in narrow width must be taller than single-line heading!
    CHECK(hWrap > hNoWrap);

    // 3. Backend independence: SdlCanvas (without ThorVG) layout and rendering
    SdlCanvas sdlCanvas(nullptr);
    sdlCanvas.Begin(300, 400, 1.0f);

    Context ui;
    ui.BeginFrame({300.0f, 400.0f}, 1.0f);
    Response headResp;
    Response btn1Resp, btn2Resp, btn3Resp;
    if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(10.0f)})) {
        headResp = ui.Heading(optsWrap);
        if (auto row = ui.Row({.gap = 8.0f})) {
            btn1Resp = ui.Button({.label = "Col 1 (Stretch)", .stretch = StretchMode::Fill});
            btn2Resp = ui.Button({.label = "Col 2 (Stretch)", .stretch = StretchMode::Fill});
            btn3Resp = ui.Button({.label = "Col 3 (Stretch)", .stretch = StretchMode::Fill});
        }
    }
    ui.EndFrame(sdlCanvas);
    sdlCanvas.End();

    // Verify Heading wrapped height is reflected in placed bounds
    const Rect headBounds = ui.GetPrevFrameBounds(headResp.id);
    CHECK_NEAR(headBounds.w, 280.0f, 1.0f);
    CHECK(headBounds.h > hNoWrap);

    // Verify 3 stretch buttons divide available row width without overlap
    const Rect b1 = ui.GetPrevFrameBounds(btn1Resp.id);
    const Rect b2 = ui.GetPrevFrameBounds(btn2Resp.id);
    const Rect b3 = ui.GetPrevFrameBounds(btn3Resp.id);

    // Available width = 280, gap = 8, 2 gaps = 16, each button = (280 - 16) / 3 = 88.0f
    CHECK_NEAR(b1.w, 88.0f, 1.0f);
    CHECK_NEAR(b2.w, 88.0f, 1.0f);
    CHECK_NEAR(b3.w, 88.0f, 1.0f);
    // Strict non-overlap: b2 starts after b1 ends, b3 starts after b2 ends
    CHECK(b2.x >= b1.Right() - 0.01f);
    CHECK(b3.x >= b2.Right() - 0.01f);

    // 4. TabBar rendering at 100% and 150% scales: labels must not falsely truncate
    static const std::array<guikit::TabItem, 3> testTabs = {{
        {.label = "Overview"},
        {.label = "Industrial Dashboard", .badge = "Live"},
        {.label = "Network Diagnostics"}
    }};

    for (float testScale : {1.0f, 1.5f}) {
        int tabIdx = 0;
        ThorCanvas thorCanvas;
        std::vector<uint32_t> px(800 * 600, 0);
        thorCanvas.SetTarget(px.data(), 800, 600, 800);
        thorCanvas.Begin(800, 600, testScale);

        Context tabUi;
        tabUi.BeginFrame({800.0f, 600.0f}, testScale);
        Response tabRes = tabUi.TabBar(tabIdx, testTabs, {.gap = 12.0f});
        tabUi.EndFrame(thorCanvas);
        thorCanvas.End();

        CHECK(tabRes.clicked == false);
    }

    // 5. Verify TextUtil::TruncateWithEllipsis behaves deterministically
    const std::string fullText = "Industrial Dashboard";
    const std::string trFull = TextUtil::TruncateWithEllipsis(fullText, 400.0f, 14.0f, nullptr);
    CHECK(trFull == fullText);

    const std::string trShort = TextUtil::TruncateWithEllipsis(fullText, 60.0f, 14.0f, nullptr);
    CHECK(trShort.ends_with("..."));
    CHECK(trShort.length() < fullText.length());
}

void TestTextAreaWidgetWorkflow() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    std::string sampleCode = "ui.Button({\n    .label = \"Test\",\n    .variant = guikit::UiVariant::Primary\n});";

    // 1. Measure test: autoHeight must scale with line count, Element header, and zoom
    const auto mSingle = TextArea::Measure("Single Line", {.autoHeight = true}, UiSize::Md, &canvas);
    const auto mMulti = TextArea::Measure(sampleCode, {.autoHeight = true}, UiSize::Md, &canvas);
    const auto mWithHeader = TextArea::Measure(sampleCode, {
        .label = "A text area",
        .description = "This is a text area",
        .autoHeight = true
    }, UiSize::Md, &canvas);
    CHECK(mMulti.y > mSingle.y * 2.0f);
    CHECK(mWithHeader.y > mMulti.y);

    // 2. Configurable Syntax Highlighter test (Cpp, Json, Custom)
    std::vector<TextSpan> spans;
    const auto theme = UiTheme::Dark();
    const std::string_view cppLine = "const int count = Compute(42, \"ok\"); // comment";
    TextArea::HighlightLine(cppLine, 0, SyntaxConfig::Cpp(), spans, theme);
    CHECK(!spans.empty());
    // Verify spans cover the entire line contiguously with no gaps or overlaps
    size_t expectedCol = 0;
    bool foundKeyword = false;
    bool foundType = false;
    bool foundFunc = false;
    bool foundNumber = false;
    bool foundString = false;
    bool foundComment = false;
    for (const auto& sp : spans) {
        CHECK(sp.startCol == expectedCol);
        expectedCol = sp.startCol + sp.length;
        const std::string_view tok = cppLine.substr(sp.startCol, sp.length);
        if (tok == "const" && sp.color == theme.syntax.keyword) foundKeyword = true;
        if (tok == "int" && sp.color == theme.syntax.type) foundType = true;
        if (tok == "Compute" && sp.color == theme.syntax.function) foundFunc = true;
        if (tok == "42" && sp.color == theme.syntax.number) foundNumber = true;
        if (tok == "\"ok\"" && sp.color == theme.syntax.string) foundString = true;
        if (tok.starts_with("//") && sp.color == theme.syntax.comment) foundComment = true;
    }
    CHECK(expectedCol == cppLine.size());
    CHECK(foundKeyword);
    CHECK(foundType);
    CHECK(foundFunc);
    CHECK(foundNumber);
    CHECK(foundString);
    CHECK(foundComment);

    // Custom SyntaxConfig test
    static constexpr std::array<std::string_view, 2> customKw = {"rule", "when"};
    static constexpr std::array<std::string_view, 1> customTy = {"Sensor"};
    SyntaxConfig customCfg{
        .enabled = true,
        .keywords = customKw,
        .types = customTy,
        .lineComment = "#"
    };
    TextArea::HighlightLine("rule Sensor # note", 0, customCfg, spans, theme);
    CHECK(spans.size() >= 3);
    CHECK(spans.front().color == theme.syntax.keyword);
    CHECK(spans.back().color == theme.syntax.comment);

    // 3. Interactive 2D editing, multiline navigation (Up/Down/Tab/Enter), and Ctrl+Wheel zoom
    std::string editableText = "int a = 1;\nint b = 2;";
    float editorZoom = 1.0f;
    Response editRes;
    auto renderEditor = [&]() {
        editRes = ui.TextArea(editableText, {
            .label = "A text area",
            .description = "This is a text area",
            .autoHeight = true,
            .showLineNumbers = true,
            .showCopyButton = true,
            .allowZoom = true,
            .zoom = editorZoom,
            .syntax = SyntaxConfig::Cpp()
        });
    };

    // Frame 1 & 2: Settle layout
    for (int f = 0; f < 2; ++f) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderEditor();
        ui.EndFrame(canvas);
    }
    const float heightBeforeZoom = editRes.rect.h;
    CHECK(editRes.rect.w > 100.0f);
    CHECK(heightBeforeZoom > 50.0f);

    // Click inside editor to focus & place cursor
    const Vec2 clickPt(editRes.rect.x + 80.0f, editRes.rect.y + 60.0f);
    ui.OnMouseMove(clickPt);
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderEditor();
    ui.EndFrame(canvas);
    ui.OnMouseUp(1);

    CHECK(ui.GetFocusedId() == editRes.id);

    // Press Home, then Down arrow, then Tab -> indents second line with 4 spaces!
    ui.OnKeyDown(Key::Home);
    ui.OnKeyDown(Key::Down);
    ui.OnKeyDown(Key::Home);
    ui.OnKeyDown(Key::Tab);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderEditor();
    ui.EndFrame(canvas);

    CHECK(editableText == "int a = 1;\n    int b = 2;");
    CHECK(editRes.changed);

    // Increase zoom via editorZoom option -> autoHeight editor grows taller!
    editorZoom = 1.5f;
    for (int f = 0; f < 2; ++f) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderEditor();
        ui.EndFrame(canvas);
    }
    CHECK(editRes.rect.h > heightBeforeZoom);

    // 4. Verify glyph metric box height and precise mouse coordinate hit-testing per line & column
    const float codeFontSize = theme.GetTypography(TextRole::CodeMedium).fontSize;
    const float glyphBoxH = canvas.MeasureTextStyled("Mg", codeFontSize, FontStyle::Monospace).y;
    CHECK(glyphBoxH > codeFontSize * 1.35f);

    std::string coordText = "AAAA\nBBBB\nCCCC";
    Response coordRes;
    auto renderCoordEditor = [&]() {
        coordRes = ui.TextArea(coordText, {
            .autoHeight = true,
            .showLineNumbers = true,
            .showCopyButton = false,
            .syntax = SyntaxConfig::Plain()
        });
    };
    for (int f = 0; f < 2; ++f) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderCoordEditor();
        ui.EndFrame(canvas);
    }

    // Compute exact linear line height and gutter width for Md size
    const auto& cm = theme.GetMetrics(UiSize::Md);
    const float lineH = std::max(theme.GetLineHeight(codeFontSize), glyphBoxH * 1.14f);
    const float padY = std::max(theme.Scale(8.0f), cm.paddingY);
    const float padX = std::max(theme.Scale(10.0f), cm.paddingX);
    const float charW = canvas.MeasureTextStyled("B", codeFontSize, FontStyle::Monospace).x;
    const float digitW = canvas.MeasureTextStyled("8", codeFontSize, FontStyle::Monospace).x;
    const float gutterW = 2.0f * digitW + charW * 1.25f * 2.0f;
    const float textStartX = coordRes.rect.x + 1.0f + gutterW + padX;
    const float line1CenterY = coordRes.rect.y + 1.0f + padY + lineH * 1.5f; // Center of second line ("BBBB")

    // Click right after the 2nd 'B' on line 1 (column 2) and insert 'X'
    const Vec2 line1Col2Pt(textStartX + charW * 2.0f, line1CenterY);
    ui.OnMouseMove(line1Col2Pt);
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderCoordEditor();
    ui.EndFrame(canvas);
    ui.OnMouseUp(1);

    ui.OnTextInput("X");
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderCoordEditor();
    ui.EndFrame(canvas);

    CHECK(coordText == "AAAA\nBBXBB\nCCCC");

    // 5. Verify continuous sub-pixel zoom without std::ceil step quantization:
    // Every 0.01x zoom increment must smoothly increase height (never staying frozen and jumping).
    float prevSubZoomH = 0.0f;
    for (int step = 0; step <= 4; ++step) {
        const float z = 1.0f + static_cast<float>(step) * 0.01f;
        const Vec2 measured = TextArea::Measure(coordText, {
            .autoHeight = true,
            .showLineNumbers = true,
            .monospace = true,
            .zoom = z
        }, UiSize::Md, &canvas, theme);
        if (step > 0) {
            CHECK(measured.y > prevSubZoomH);
        }
        prevSubZoomH = measured.y;
    }

    // 6. Verify Ctrl+Plus and Ctrl+0 keyboard zoom shortcuts on focused editor
    const float hAt1x = coordRes.rect.h;
    ui.OnKeyDown(Key::Plus, {.ctrl = true});
    for (int f = 0; f < 2; ++f) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderCoordEditor();
        ui.EndFrame(canvas);
    }
    CHECK(coordRes.rect.h > hAt1x);

    ui.OnKeyDown(Key::Zero, {.ctrl = true});
    for (int f = 0; f < 2; ++f) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderCoordEditor();
        ui.EndFrame(canvas);
    }
    CHECK(std::abs(coordRes.rect.h - hAt1x) < 0.05f);
}

void TestButtonAlignmentAndProceduralIcons() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 400;
    const uint32_t height = 500;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    // Verify IconId enum values & procedural drawing
    auto editIcon = Icon::Edit();
    CHECK(editIcon.IsValid());
    CHECK(editIcon.GetId() == IconId::Edit);

    auto copyIcon = Icon::Copy();
    CHECK(copyIcon.IsValid());
    CHECK(copyIcon.GetId() == IconId::Copy);

    auto eyeOffIcon = Icon(IconId::EyeOff);
    CHECK(eyeOffIcon.IsValid());
    CHECK(eyeOffIcon.GetId() == IconId::EyeOff);

    // Render frame with Left-Aligned (Start) navigation rail buttons
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)}, 1.0f);
    if (auto col = ui.Column({.gap = 4.0f})) {
        ui.Button({
            .label = "Button",
            .icon = Icon::Play(),
            .controlAlign = Align::Start,
            .stretch = StretchMode::Fill
        });
        ui.Button({
            .label = "Input",
            .icon = Icon::Edit(),
            .controlAlign = Align::Start,
            .stretch = StretchMode::Fill
        });
        ui.Button({
            .label = "Containers",
            .icon = Icon::Copy(),
            .controlAlign = Align::Start,
            .stretch = StretchMode::Fill
        });
        ui.Button({
            .label = "End-Aligned",
            .icon = Icon::Settings(),
            .controlAlign = Align::End,
            .stretch = StretchMode::Fill
        });
        ui.Button({
            .label = "Center-Aligned",
            .icon = Icon::Folder(),
            .controlAlign = Align::Center,
            .stretch = StretchMode::Fill
        });
        // Label only left-aligned
        ui.Button({
            .label = "Label Only Left",
            .controlAlign = Align::Start,
            .stretch = StretchMode::Fill
        });
        // Icon only left-aligned
        ui.Button({
            .icon = Icon::Plus(),
            .controlAlign = Align::Start,
            .stretch = StretchMode::Fill
        });
    }
    ui.EndFrame(canvas);

    // Verify that Content Alignment (.align) affects only content inside button capsule,
    // and does NOT move the button capsule relative to bounds or external labels
    class RecordingCanvas : public ThorCanvas {
    public:
        void DrawRoundRect(Rect rect, float radius, Color fill, Color stroke = Color(0, 0, 0, 0), float strokeWidth = 0.0f) override {
            lastCapsuleRect = rect;
            ThorCanvas::DrawRoundRect(rect, radius, fill, stroke, strokeWidth);
        }
        void DrawTextBox(std::string_view text, Rect bounds, float fontSize, Color color, float alignX = 0.0f, float alignY = 0.5f) override {
            lastTextBounds = bounds;
            lastAlignX = alignX;
            ThorCanvas::DrawTextBox(text, bounds, fontSize, color, alignX, alignY);
        }
        Rect lastCapsuleRect{};
        Rect lastTextBounds{};
        float lastAlignX{0.0f};
    };

    RecordingCanvas recCanvas;
    std::vector<uint32_t> recPixels(800 * 600, 0);
    recCanvas.SetTarget(recPixels.data(), 800, 600, 800);
    const UiTheme& darkTheme = UiTheme::Dark();
    const Rect widgetBounds(50.0f, 100.0f, 400.0f, 80.0f);

    ButtonOptions bStart{
        .label = "Action Label",
        .text = "Click Action",
        .layout = ElementLayout::Stacked,
        .stretch = StretchMode::Compact,
        .control = {.width = 150.0f, .align = Align::Start}
    };
    ButtonOptions bCenter{
        .label = "Action Label",
        .text = "Click Action",
        .layout = ElementLayout::Stacked,
        .stretch = StretchMode::Compact,
        .control = {.width = 150.0f, .align = Align::Center}
    };
    ButtonOptions bEnd{
        .label = "Action Label",
        .text = "Click Action",
        .layout = ElementLayout::Stacked,
        .stretch = StretchMode::Compact,
        .control = {.width = 150.0f, .align = Align::End}
    };

    Button::Render(recCanvas, widgetBounds, bStart, UiSize::Md, false, false, false, darkTheme);
    const Rect capStart = recCanvas.lastCapsuleRect;
    const float alignXStart = recCanvas.lastAlignX;

    Button::Render(recCanvas, widgetBounds, bCenter, UiSize::Md, false, false, false, darkTheme);
    const Rect capCenter = recCanvas.lastCapsuleRect;
    const float alignXCenter = recCanvas.lastAlignX;

    Button::Render(recCanvas, widgetBounds, bEnd, UiSize::Md, false, false, false, darkTheme);
    const Rect capEnd = recCanvas.lastCapsuleRect;
    const float alignXEnd = recCanvas.lastAlignX;

    // Capsule positions must be strictly identical (not shifted horizontally by .align)
    CHECK_NEAR(capStart.x, widgetBounds.x, 0.01f);
    CHECK_NEAR(capCenter.x, widgetBounds.x, 0.01f);
    CHECK_NEAR(capEnd.x, widgetBounds.x, 0.01f);
    CHECK_NEAR(capStart.w, 150.0f, 0.01f);
    CHECK_NEAR(capCenter.w, 150.0f, 0.01f);
    CHECK_NEAR(capEnd.w, 150.0f, 0.01f);

    // Meanwhile, text content alignment inside the capsule must reflect .align (0.0f, 0.5f, 1.0f)
    CHECK_NEAR(alignXStart, 0.0f, 0.01f);
    CHECK_NEAR(alignXCenter, 0.5f, 0.01f);
    CHECK_NEAR(alignXEnd, 1.0f, 0.01f);

    std::cout << "  [PASS] TestButtonAlignmentAndProceduralIcons completed\n";
}

void TestWidgetRadiusOverride() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 400;
    const uint32_t height = 300;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    std::string inputText = "hello";

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)}, 1.0f);
    if (auto col = ui.Column({.gap = 8.0f})) {
        auto btnDefault = ui.Button({.label = "Default Radius"});
        auto btnCustom = ui.Button({.label = "Custom Radius", .control = {.borderRadius = 16.0f}});
        auto inputDefault = ui.InputField(inputText, {.label = "Default Radius Input"});
        auto inputCustom = ui.InputField(inputText, {.label = "Custom Radius Input", .control = {.borderRadius = 14.0f}});

        CHECK(btnDefault.id.IsValid());
        CHECK(btnCustom.id.IsValid());
        CHECK(inputDefault.id.IsValid());
        CHECK(inputCustom.id.IsValid());
    }
    ui.EndFrame(canvas);

    std::cout << "  [PASS] TestWidgetRadiusOverride completed\n";
}

void TestButtonExternalLabelDescriptionAndPlacement() {
    std::cout << "[TEST] Running TestButtonExternalLabelDescriptionAndPlacement...\n";

    ThorCanvas canvas;
    Context ui;

    const uint32_t width = 600;
    const uint32_t height = 500;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    // 1. Measure comparisons
    const UiTheme& theme = UiTheme::Dark();
    const Vec2 plainSize = Button::Measure({.label = "Deploy"}, UiSize::Md, &canvas, theme);
    const Vec2 stackedSize = Button::Measure({
        .label = "Deployment Action",
        .text = "Deploy",
        .description = "Immediate canary release",
        .layout = ElementLayout::Stacked
    }, UiSize::Md, &canvas, theme);
    const Vec2 leadingSize = Button::Measure({
        .label = "Deployment Action",
        .text = "Deploy",
        .description = "Immediate canary release",
        .layout = ElementLayout::Inline,
        .inlineOrder = InlineOrder::LabelFirst
    }, UiSize::Md, &canvas, theme);

    // Stacked height must be strictly taller than plain button due to external label and description
    CHECK(stackedSize.y > plainSize.y);
    // Leading width must be strictly wider than plain button due to text placed beside button
    CHECK(leadingSize.x > plainSize.x);

    // 2. Full immediate mode execution and rendering across all placements
    size_t selIdx = 0;
    static const std::array<std::string_view, 3> items = {"Alpha", "Beta", "Gamma"};

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)}, 1.0f);
    if (auto col = ui.Column({.gap = 8.0f})) {
        auto bStacked = ui.Button({
            .label = "Action Header",
            .text = "Click Stacked",
            .description = "Helper info stacked",
            .layout = ElementLayout::Stacked
        });
        auto bLeading = ui.Button({
            .label = "Action Header",
            .text = "Click Leading",
            .description = "Helper info leading",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::LabelFirst
        });
        auto bTrailing = ui.Button({
            .label = "Action Header",
            .text = "Click Trailing",
            .description = "Helper info trailing",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::ControlFirst
        });
        auto bTop = ui.Button({
            .label = "Action Header",
            .text = "Click Top",
            .description = "Helper info top",
            .layout = ElementLayout::Top
        });
        auto bBottom = ui.Button({
            .label = "Action Header",
            .text = "Click Bottom",
            .description = "Helper info bottom",
            .layout = ElementLayout::Bottom
        });

        // Select with custom radius override
        auto selRes = ui.Select(selIdx, items, {
            .label = "Select Header",
            .description = "Choose option",
            .layout = ElementLayout::Stacked,
            .control = {.borderRadius = 12.0f}
        });

        CHECK(bStacked.id.IsValid());
        CHECK(bLeading.id.IsValid());
        CHECK(bTrailing.id.IsValid());
        CHECK(bTop.id.IsValid());
        CHECK(bBottom.id.IsValid());
        CHECK(selRes.id.IsValid());
    }
    ui.EndFrame(canvas);

    std::cout << "  [PASS] TestButtonExternalLabelDescriptionAndPlacement completed\n";
}

void TestWidgetStretchModesAndExplicitWidth() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 1200;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    // 1. Button explicit width & minWidth in standalone Measure
    {
        ButtonOptions btnDef{.text = "Go"};
        Vec2 szDef = Button::Measure(btnDef, UiSize::Md, &canvas, ui.GetTheme());

        ButtonOptions btnWidth{.text = "Go", .width = 180.0f};
        Vec2 szWidth = Button::Measure(btnWidth, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(szWidth.x, 180.0f, 0.01f);

        ButtonOptions btnMinSmall{.text = "Go", .minWidth = 140.0f};
        Vec2 szMinSmall = Button::Measure(btnMinSmall, UiSize::Md, &canvas, ui.GetTheme());
        CHECK(szMinSmall.x >= 140.0f);

        ButtonOptions btnMinLarge{.text = "Very long button text that exceeds minimum", .minWidth = 50.0f};
        Vec2 szMinLarge = Button::Measure(btnMinLarge, UiSize::Md, &canvas, ui.GetTheme());
        CHECK(szMinLarge.x > 100.0f);
    }

    // 2. Slider standalone Measure with width
    {
        SliderOptions sDef{.showValue = false};
        Vec2 szSDef = Slider::Measure(sDef, 50.0, 0.0, 100.0, UiSize::Md, &canvas, ui.GetTheme());
        const float defaultW = ui.GetTheme().GetSlider(UiSize::Md).defaultWidth;
        const float thumbSize = ui.GetTheme().GetSlider(UiSize::Md).thumbSize;
        CHECK_NEAR(szSDef.x, defaultW + thumbSize, 0.01f);

        SliderOptions sExplicit{.showValue = false, .stretch = StretchMode::Compact, .width = 240.0f};
        Vec2 szSExp = Slider::Measure(sExplicit, 50.0, 0.0, 100.0, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(szSExp.x, 240.0f + thumbSize, 0.01f);

        SliderOptions sWithVal{.showValue = true};
        Vec2 szSVal = Slider::Measure(sWithVal, 50.0, 0.0, 100.0, UiSize::Md, &canvas, ui.GetTheme());
        CHECK(szSVal.x > szSDef.x);
    }

    // 3. Select standalone Measure with intrinsic width & explicit width
    {
        SelectOptions selDef;
        Vec2 szSelDef = Select::Measure(selDef, UiSize::Md, &canvas, ui.GetTheme());
        const float defW = ui.GetTheme().GetSelect(UiSize::Md).defaultWidth;
        CHECK_NEAR(szSelDef.x, defW, 0.01f);

        SelectOptions selExplicit{.stretch = StretchMode::Compact, .width = 320.0f};
        Vec2 szExp = Select::Measure(selExplicit, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(szExp.x, 320.0f, 0.01f);

        SelectOptions selMin{.minWidth = 250.0f};
        Vec2 szMin = Select::Measure(selMin, UiSize::Md, &canvas, ui.GetTheme());
        CHECK(szMin.x >= 250.0f);
    }

    // 4. InputField standalone Measure with explicit width & minWidth
    {
        InputFieldOptions inDef;
        Vec2 szInDef = InputField::Measure("", inDef, UiSize::Md, &canvas, ui.GetTheme());
        const float defInW = ui.GetTheme().GetInputField(UiSize::Md).defaultWidth;
        CHECK_NEAR(szInDef.x, defInW, 0.01f);

        InputFieldOptions inExp{.stretch = StretchMode::Compact, .width = 280.0f};
        Vec2 szInExp = InputField::Measure("", inExp, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(szInExp.x, 280.0f, 0.01f);

        InputFieldOptions inMin{.minWidth = 220.0f};
        Vec2 szInMin = InputField::Measure("", inMin, UiSize::Md, &canvas, ui.GetTheme());
        CHECK(szInMin.x >= 220.0f);
    }

    // 5. Layout tests: Row with Compact vs Between vs Fill across widgets
    Response btnCompact, btnFill1, btnFill2;
    Response swCompact, swBetween;
    Response cbCompact, cbBetween;
    Response slCompact, slBetween;
    Response selCompact, selBetween;
    Response inCompact, inBetween;

    bool swVal = true;
    bool cbVal = true;
    float slVal = 50.0f;
    size_t selIdx = 0;
    static const std::array<std::string_view, 2> selOpts = {"Alpha", "Beta"};
    std::string inText = "Test";

    auto render = [&]() {
        if (auto col = ui.Column({.widthMode = SizeMode::Stretch})) {
            // Button Row: Compact vs Fill
            if (auto r1 = ui.Row({.gap = 10.0f})) {
                btnCompact = ui.Button({.label = "Compact", .stretch = StretchMode::Compact});
                btnFill1 = ui.Button({.label = "Fill 1", .stretch = StretchMode::Fill});
                btnFill2 = ui.Button({.label = "Fill 2", .stretch = StretchMode::Fill});
            }

            // Switch in InlineOrder::LabelFirst: Compact vs Space
            swCompact = ui.Switch(swVal, {
                .label = "Switch Compact",
                .inlineOrder = InlineOrder::LabelFirst,
                .stretch = StretchMode::Compact
            });
            swBetween = ui.Switch(swVal, {
                .label = "Switch Space",
                .inlineOrder = InlineOrder::LabelFirst,
                .stretch = StretchMode::Space
            });

            // Checkbox in InlineOrder::LabelFirst: Compact vs Space
            cbCompact = ui.Checkbox(cbVal, {
                .label = "Checkbox Compact",
                .inlineOrder = InlineOrder::LabelFirst,
                .stretch = StretchMode::Compact
            });
            cbBetween = ui.Checkbox(cbVal, {
                .label = "Checkbox Space",
                .inlineOrder = InlineOrder::LabelFirst,
                .stretch = StretchMode::Space
            });

            // Slider: Compact vs Space
            slCompact = ui.Slider(slVal, 0.0f, 100.0f, {
                .label = "Slider Compact",
                .layout = ElementLayout::Inline,
                .inlineOrder = InlineOrder::LabelFirst,
                .stretch = StretchMode::Compact
            });
            slBetween = ui.Slider(slVal, 0.0f, 100.0f, {
                .label = "Slider Space",
                .layout = ElementLayout::Inline,
                .inlineOrder = InlineOrder::LabelFirst,
                .stretch = StretchMode::Space
            });

            // Select: Compact vs Space
            selCompact = ui.Select(selIdx, selOpts, {
                .label = "Select Compact",
                .layout = ElementLayout::Inline,
                .inlineOrder = InlineOrder::LabelFirst,
                .stretch = StretchMode::Compact
            });
            selBetween = ui.Select(selIdx, selOpts, {
                .label = "Select Space",
                .layout = ElementLayout::Inline,
                .inlineOrder = InlineOrder::LabelFirst,
                .stretch = StretchMode::Space
            });

            // InputField: Compact vs Space
            inCompact = ui.InputField(inText, {
                .label = "Input Compact",
                .layout = ElementLayout::Inline,
                .inlineOrder = InlineOrder::LabelFirst,
                .stretch = StretchMode::Compact
            });
            inBetween = ui.InputField(inText, {
                .label = "Input Space",
                .layout = ElementLayout::Inline,
                .inlineOrder = InlineOrder::LabelFirst,
                .stretch = StretchMode::Space
            });
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    const Rect rBtnCompact = ui.GetPrevFrameBounds(btnCompact.id);
    const Rect rBtnFill1 = ui.GetPrevFrameBounds(btnFill1.id);
    const Rect rBtnFill2 = ui.GetPrevFrameBounds(btnFill2.id);

    CHECK(rBtnCompact.w > 0.0f);
    CHECK(rBtnFill1.w > rBtnCompact.w);
    CHECK_NEAR(rBtnFill1.w, rBtnFill2.w, 0.01f);

    const Rect rSwCompact = ui.GetPrevFrameBounds(swCompact.id);
    const Rect rSwBetween = ui.GetPrevFrameBounds(swBetween.id);
    // Space stretches across container width (800)
    CHECK(rSwBetween.w > rSwCompact.w);
    CHECK_NEAR(rSwBetween.w, 800.0f, 0.01f);

    const Rect rCbCompact = ui.GetPrevFrameBounds(cbCompact.id);
    const Rect rCbBetween = ui.GetPrevFrameBounds(cbBetween.id);
    CHECK(rCbBetween.w > rCbCompact.w);
    CHECK_NEAR(rCbBetween.w, 800.0f, 0.01f);

    const Rect rSlCompact = ui.GetPrevFrameBounds(slCompact.id);
    const Rect rSlBetween = ui.GetPrevFrameBounds(slBetween.id);
    CHECK(rSlBetween.w > rSlCompact.w);
    CHECK_NEAR(rSlBetween.w, 800.0f, 0.01f);

    const Rect rSelCompact = ui.GetPrevFrameBounds(selCompact.id);
    const Rect rSelBetween = ui.GetPrevFrameBounds(selBetween.id);
    CHECK(rSelBetween.w > rSelCompact.w);
    CHECK_NEAR(rSelBetween.w, 800.0f, 0.01f);

    const Rect rInCompact = ui.GetPrevFrameBounds(inCompact.id);
    const Rect rInBetween = ui.GetPrevFrameBounds(inBetween.id);
    CHECK(rInBetween.w > rInCompact.w);
    CHECK_NEAR(rInBetween.w, 800.0f, 0.01f);

    // 6. Test Button in Placement::Stacked with StretchMode::Space vs Fill
    Response btnStBetween, btnStFill;
    ui.BeginFrame({800.0f, 600.0f});
    if (auto col = ui.Column()) {
        btnStBetween = ui.Button({
            .label = "Header",
            .text = "Action",
            .layout = ElementLayout::Stacked,
            .stretch = StretchMode::Space
        });
        btnStFill = ui.Button({
            .label = "Header",
            .text = "Action",
            .layout = ElementLayout::Stacked,
            .stretch = StretchMode::Fill
        });
    }
    ui.EndFrame(canvas);

    ui.BeginFrame({800.0f, 600.0f});
    if (auto col = ui.Column()) {
        btnStBetween = ui.Button({
            .label = "Header",
            .text = "Action",
            .layout = ElementLayout::Stacked,
            .stretch = StretchMode::Space
        });
        btnStFill = ui.Button({
            .label = "Header",
            .text = "Action",
            .layout = ElementLayout::Stacked,
            .stretch = StretchMode::Fill
        });
    }
    ui.EndFrame(canvas);

    const Rect rStBetween = ui.GetPrevFrameBounds(btnStBetween.id);
    const Rect rStFill = ui.GetPrevFrameBounds(btnStFill.id);
    CHECK_NEAR(rStBetween.w, 800.0f, 0.01f);
    CHECK_NEAR(rStFill.w, 800.0f, 0.01f);

    // 7. Test Button in InlineOrder::ControlFirst with StretchMode::Compact, Space, Fill
    Response btnTrCompact, btnTrBetween, btnTrFill;
    ui.BeginFrame({800.0f, 600.0f});
    if (auto col = ui.Column()) {
        btnTrCompact = ui.Button({
            .label = "Trailing Label",
            .text = "Action",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::ControlFirst,
            .stretch = StretchMode::Compact
        });
        btnTrBetween = ui.Button({
            .label = "Trailing Label",
            .text = "Action",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::ControlFirst,
            .stretch = StretchMode::Space
        });
        btnTrFill = ui.Button({
            .label = "Trailing Label",
            .text = "Action",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::ControlFirst,
            .stretch = StretchMode::Fill
        });
    }
    ui.EndFrame(canvas);

    ui.BeginFrame({800.0f, 600.0f});
    if (auto col = ui.Column()) {
        btnTrCompact = ui.Button({
            .label = "Trailing Label",
            .text = "Action",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::ControlFirst,
            .stretch = StretchMode::Compact
        });
        btnTrBetween = ui.Button({
            .label = "Trailing Label",
            .text = "Action",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::ControlFirst,
            .stretch = StretchMode::Space
        });
        btnTrFill = ui.Button({
            .label = "Trailing Label",
            .text = "Action",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::ControlFirst,
            .stretch = StretchMode::Fill
        });
    }
    ui.EndFrame(canvas);

    const Rect rTrCompact = ui.GetPrevFrameBounds(btnTrCompact.id);
    const Rect rTrBetween = ui.GetPrevFrameBounds(btnTrBetween.id);
    const Rect rTrFill = ui.GetPrevFrameBounds(btnTrFill.id);

    CHECK(rTrCompact.w < 800.0f);
    CHECK_NEAR(rTrBetween.w, 800.0f, 0.01f);
    CHECK_NEAR(rTrFill.w, 800.0f, 0.01f);

    std::cout << "  [PASS] TestWidgetStretchModesAndExplicitWidth completed\n";
}

namespace {
class LabelRecordingCanvas : public guikit::ThorCanvas {
public:
    struct TextBoxCall {
        std::string text;
        guikit::Rect bounds;
        float alignX;
        guikit::Color color;
    };
    void DrawRoundRect(guikit::Rect rect, float radius, guikit::Color fill, guikit::Color stroke = guikit::Color(0, 0, 0, 0), float strokeWidth = 0.0f) override {
        lastRoundRect = rect;
        ThorCanvas::DrawRoundRect(rect, radius, fill, stroke, strokeWidth);
    }
    void DrawTextBox(std::string_view text, guikit::Rect bounds, float fontSize, guikit::Color color, float alignX = 0.0f, float alignY = 0.5f) override {
        textBoxCalls.push_back({std::string(text), bounds, alignX, color});
        ThorCanvas::DrawTextBox(text, bounds, fontSize, color, alignX, alignY);
    }
    void Reset() {
        lastRoundRect = {};
        textBoxCalls.clear();
    }
    const TextBoxCall* FindTextBox(std::string_view text) const {
        for (const auto& c : textBoxCalls) {
            if (c.text == text) return &c;
        }
        return nullptr;
    }
    guikit::Rect lastRoundRect{};
    std::vector<TextBoxCall> textBoxCalls;
};
} // namespace

void TestLabelAlignmentGeometryOptionA() {
    using namespace guikit;

    LabelRecordingCanvas recCanvas;
    std::vector<uint32_t> pixels(800 * 600, 0);
    recCanvas.SetTarget(pixels.data(), 800, 600, 800);
    const UiTheme& darkTheme = UiTheme::Dark();
    const Rect widgetBounds(50.0f, 100.0f, 800.0f, 80.0f);

    // 1. Button in Top layout with Compact stretch:
    // Label block width must be tied to control width (150px) rather than whole container (800px)
    {
        recCanvas.Reset();
        ButtonOptions opts{
            .label = "Header Label",
            .text = "Click Me",
            .description = {"Warning helper", UiVariant::Warning},
            .layout = ElementLayout::Top,
            .elementAlign = Align::Center,
            .stretch = StretchMode::Compact,
            .width = 150.0f
        };
        Button::Render(recCanvas, widgetBounds, opts, UiSize::Md, false, false, false, darkTheme);
        const auto* call = recCanvas.FindTextBox("Header Label");
        CHECK(call != nullptr);
        if (call) {
            CHECK_NEAR(call->bounds.x, 50.0f, 0.01f);
            CHECK_NEAR(call->bounds.w, 150.0f, 0.01f);
            CHECK_NEAR(call->alignX, 0.5f, 0.01f);
            CHECK(call->color == darkTheme.palette.textPrimary);
        }
        const auto* descCall = recCanvas.FindTextBox("Warning helper");
        CHECK(descCall != nullptr);
        if (descCall) {
            CHECK(descCall->color == darkTheme.GetTextColor(UiVariant::Warning));
        }
    }

    // 2. Button in Top layout with Fill stretch:
    // Label block width spans entire container (800px)
    {
        recCanvas.Reset();
        ButtonOptions opts{
            .label = "Header Label",
            .text = "Click Me",
            .layout = ElementLayout::Top,
            .elementAlign = Align::End,
            .stretch = StretchMode::Fill
        };
        Button::Render(recCanvas, widgetBounds, opts, UiSize::Md, false, false, false, darkTheme);
        const auto* call = recCanvas.FindTextBox("Header Label");
        CHECK(call != nullptr);
        if (call) {
            CHECK_NEAR(call->bounds.x, 50.0f, 0.01f);
            CHECK_NEAR(call->bounds.w, 800.0f, 0.01f);
            CHECK_NEAR(call->alignX, 1.0f, 0.01f);
        }
    }

    // 2b. Button in Top layout with Space stretch and elementAlign = End:
    // Both Label and compact Button capsule (150px) align to the right edge of the 800px row
    {
        recCanvas.Reset();
        ButtonOptions opts{
            .label = "Header Label",
            .text = "Click Me",
            .layout = ElementLayout::Top,
            .elementAlign = Align::End,
            .stretch = StretchMode::Space,
            .width = 150.0f
        };
        Button::Render(recCanvas, widgetBounds, opts, UiSize::Md, false, false, false, darkTheme);
        const auto* call = recCanvas.FindTextBox("Header Label");
        CHECK(call != nullptr);
        if (call) {
            CHECK_NEAR(call->bounds.x, 50.0f, 0.01f);
            CHECK_NEAR(call->bounds.w, 800.0f, 0.01f);
            CHECK_NEAR(call->alignX, 1.0f, 0.01f);
            CHECK_NEAR(recCanvas.lastRoundRect.x, 700.0f, 0.01f);
            CHECK_NEAR(recCanvas.lastRoundRect.w, 150.0f, 0.01f);
        }
    }

    // 3. Button in Inline layout with StretchMode::Space & LabelFirst:
    // Label is docked at left edge (50px), Button capsule is docked at right edge (700px)
    {
        recCanvas.Reset();
        ButtonOptions opts{
            .label = "Setting Title",
            .text = "Action",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::LabelFirst,
            .elementAlign = Align::End,
            .stretch = StretchMode::Space,
            .width = 150.0f
        };
        Button::Render(recCanvas, widgetBounds, opts, UiSize::Md, false, false, false, darkTheme);
        const auto* call = recCanvas.FindTextBox("Setting Title");
        CHECK(call != nullptr);
        if (call) {
            // Button capsule is at 50 + 800 - 150 = 700
            const float expectedCapX = 700.0f;
            CHECK_NEAR(recCanvas.lastRoundRect.x, expectedCapX, 0.01f);
            CHECK_NEAR(call->bounds.x, 50.0f, 0.01f);
            const float measuredW = recCanvas.MeasureText("Setting Title", darkTheme.GetTypography(TextRole::LabelLarge).fontSize).x;
            CHECK_NEAR(call->bounds.w, measuredW, 0.01f);
            CHECK_NEAR(call->alignX, 1.0f, 0.01f);
        }
    }

    // 3b. Button in Top layout with StretchMode::Fill & .width = 50.0f (50% of 800px = 400px) & Align::Center:
    {
        recCanvas.Reset();
        ButtonOptions opts{
            .label = "Header Label",
            .text = "Action",
            .layout = ElementLayout::Top,
            .elementAlign = Align::Center,
            .stretch = StretchMode::Fill,
            .width = 50.0f
        };
        Button::Render(recCanvas, widgetBounds, opts, UiSize::Md, false, false, false, darkTheme);
        CHECK_NEAR(recCanvas.lastRoundRect.w, 400.0f, 0.01f);
        CHECK_NEAR(recCanvas.lastRoundRect.x, 250.0f, 0.01f); // 50 + (800 - 400) * 0.5
    }

    // 3c. Button in Inline layout with StretchMode::Fill & .width = 90.0f (90% of 800px = 720px):
    // Long label must be cropped with "..." to fit inside remaining width (800 - 720 - gap)
    {
        recCanvas.Reset();
        ButtonOptions opts{
            .label = "Very Long Setting Title That Exceeds Available Remaining Width",
            .text = "Action",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::LabelFirst,
            .stretch = StretchMode::Fill,
            .width = 90.0f
        };
        Button::Render(recCanvas, widgetBounds, opts, UiSize::Md, false, false, false, darkTheme);
        CHECK_NEAR(recCanvas.lastRoundRect.w, 720.0f, 0.01f);
        CHECK_NEAR(recCanvas.lastRoundRect.x, 130.0f, 0.01f); // 50 + 800 - 720
        CHECK(!recCanvas.textBoxCalls.empty());
        if (!recCanvas.textBoxCalls.empty()) {
            const auto& firstCall = recCanvas.textBoxCalls.front();
            CHECK(firstCall.text.ends_with("..."));
            CHECK(firstCall.bounds.Right() <= recCanvas.lastRoundRect.x);
        }
    }

    // 4. InputField in Inline layout with StretchMode::Space & ControlFirst:
    // Control box is docked at left edge (50px), Label is docked at far right edge (850 - measuredW)
    {
        recCanvas.Reset();
        InputFieldOptions opts{
            .label = "Trailing Field Info",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::ControlFirst,
            .elementAlign = Align::Center,
            .stretch = StretchMode::Space,
            .width = 200.0f
        };
        InputField::Render(recCanvas, widgetBounds, "text", opts, UiSize::Md, false, false, 0, false, false, 1.5f, darkTheme);
        const auto* call = recCanvas.FindTextBox("Trailing Field Info");
        CHECK(call != nullptr);
        if (call) {
            const float measuredW = recCanvas.MeasureText("Trailing Field Info", darkTheme.GetTypography(TextRole::LabelLarge).fontSize).x;
            const float expectedTextX = widgetBounds.x + widgetBounds.w - measuredW;
            CHECK_NEAR(call->bounds.x, expectedTextX, 0.01f);
            CHECK_NEAR(call->bounds.w, measuredW, 0.01f);
            CHECK_NEAR(call->alignX, 0.5f, 0.01f);
        }
    }

    // 4b. InputField in Stacked layout with StretchMode::Fill & .width = 50.0f (50% of 800px = 400px) & Align::End:
    {
        recCanvas.Reset();
        InputFieldOptions opts{
            .label = "Endpoint",
            .description = {"Primary host", UiVariant::Warning},
            .layout = ElementLayout::Stacked,
            .elementAlign = Align::End,
            .stretch = StretchMode::Fill,
            .width = 50.0f
        };
        InputField::Render(recCanvas, widgetBounds, "text", opts, UiSize::Md, false, false, 0, false, false, 1.5f, darkTheme);
        CHECK_NEAR(recCanvas.lastRoundRect.w, 400.0f, 0.01f);
        CHECK_NEAR(recCanvas.lastRoundRect.x, 450.0f, 0.01f); // 50 + (800 - 400) * 1.0
        const auto* descCall = recCanvas.FindTextBox("Primary host");
        CHECK(descCall != nullptr);
        if (descCall) {
            CHECK_NEAR(descCall->alignX, 1.0f, 0.01f);
        }
    }

    // 4c. Select in Inline layout with StretchMode::Fill & .width = 90.0f (90% of 800px = 720px):
    // Long label must be cropped with "..." to fit inside remaining width (800 - 720 - gap)
    {
        recCanvas.Reset();
        SelectOptions opts{
            .label = "Very Long Dropdown Setting Title That Exceeds Available Remaining Width",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::LabelFirst,
            .stretch = StretchMode::Fill,
            .width = 90.0f
        };
        Select::Render(recCanvas, widgetBounds, "Option A", opts, UiSize::Md, false, false, false, darkTheme);
        const Rect boxRect = Select::GetBoxRect(widgetBounds, opts, UiSize::Md, &recCanvas, darkTheme);
        CHECK_NEAR(boxRect.w, 720.0f, 0.01f);
        CHECK_NEAR(boxRect.x, 130.0f, 0.01f); // 50 + 800 - 720
        CHECK(!recCanvas.textBoxCalls.empty());
        if (!recCanvas.textBoxCalls.empty()) {
            const auto& firstCall = recCanvas.textBoxCalls.front();
            CHECK(firstCall.text.ends_with("..."));
            CHECK(firstCall.bounds.Right() <= boxRect.x);
        }
    }

    // 5. Switch in Top layout with Compact stretch:
    // Label block width is max(trackSize.x, measuredTextW)
    {
        recCanvas.Reset();
        SwitchOptions opts{
            .label = "Short",
            .layout = ElementLayout::Top,
            .elementAlign = Align::Center,
            .stretch = StretchMode::Compact
        };
        Switch::Render(recCanvas, widgetBounds, false, opts, UiSize::Md, false, false, false, darkTheme);
        const auto* call = recCanvas.FindTextBox("Short");
        CHECK(call != nullptr);
        if (call) {
            CHECK_NEAR(call->bounds.x, 50.0f, 0.01f);
            // Label block should NOT be 800px; it is bounded by track or text width
            CHECK(call->bounds.w < 100.0f);
            CHECK_NEAR(call->alignX, 0.5f, 0.01f);
        }
    }

    // 6. Switch in Inline layout with StretchMode::Space & ControlFirst:
    // Track is at bounds.x (50px), Label is docked at far right edge
    {
        recCanvas.Reset();
        SwitchOptions opts{
            .label = "Power Switch",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::ControlFirst,
            .stretch = StretchMode::Space
        };
        Switch::Render(recCanvas, widgetBounds, false, opts, UiSize::Md, false, false, false, darkTheme);
        const auto* call = recCanvas.FindTextBox("Power Switch");
        CHECK(call != nullptr);
        if (call) {
            const float measuredW = recCanvas.MeasureText("Power Switch", darkTheme.GetTypography(TextRole::LabelLarge).fontSize).x;
            const float expectedTextX = widgetBounds.x + widgetBounds.w - measuredW;
            CHECK_NEAR(call->bounds.x, expectedTextX, 0.01f);
            CHECK_NEAR(call->bounds.w, measuredW, 0.01f);
        }
    }

    // 7. Checkbox in Inline layout with StretchMode::Space & ControlFirst:
    // Checkbox is at bounds.x (50px), Label is docked at far right edge
    {
        recCanvas.Reset();
        CheckboxOptions opts{
            .label = "Enable Feature",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::ControlFirst,
            .stretch = StretchMode::Space
        };
        Checkbox::Render(recCanvas, widgetBounds, false, opts, UiSize::Md, false, false, false, darkTheme);
        const auto* call = recCanvas.FindTextBox("Enable Feature");
        CHECK(call != nullptr);
        if (call) {
            const float measuredW = recCanvas.MeasureText("Enable Feature", darkTheme.GetTypography(TextRole::LabelLarge).fontSize).x;
            const float expectedTextX = widgetBounds.x + widgetBounds.w - measuredW;
            CHECK_NEAR(call->bounds.x, expectedTextX, 0.01f);
            CHECK_NEAR(call->bounds.w, measuredW, 0.01f);
        }
    }

    // 8. Slider in Inline layout with StretchMode::Space & ControlFirst:
    // Control is at bounds.x (50px), Header is docked at far right edge
    {
        recCanvas.Reset();
        SliderOptions opts{
            .label = "Volume Level",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::ControlFirst,
            .showValue = false,
            .stretch = StretchMode::Space,
            .width = 150.0f
        };
        Slider::Render(recCanvas, widgetBounds, 0.5f, 0.0f, 1.0f, opts, UiSize::Md, false, false, false, darkTheme);
        const auto* call = recCanvas.FindTextBox("Volume Level");
        CHECK(call != nullptr);
        if (call) {
            const float measuredW = recCanvas.MeasureText("Volume Level", darkTheme.GetTypography(TextRole::LabelLarge).fontSize).x;
            const float expectedTextX = widgetBounds.x + widgetBounds.w - measuredW;
            CHECK_NEAR(call->bounds.x, expectedTextX, 0.01f);
            CHECK_NEAR(call->bounds.w, measuredW, 0.01f);
        }
    }

    // 8b. Slider in Inline layout with StretchMode::Fill & .width = 85.0f (85% of 800px = 680px):
    // Long label must be cropped with "..." to fit inside remaining width
    {
        recCanvas.Reset();
        SliderOptions opts{
            .label = "Very Long Slider Label That Exceeds Remaining Inline Space",
            .layout = ElementLayout::Inline,
            .inlineOrder = InlineOrder::LabelFirst,
            .showValue = false,
            .stretch = StretchMode::Fill,
            .width = 85.0f
        };
        Slider::Render(recCanvas, widgetBounds, 0.5f, 0.0f, 1.0f, opts, UiSize::Md, false, false, false, darkTheme);
        CHECK(!recCanvas.textBoxCalls.empty());
        if (!recCanvas.textBoxCalls.empty()) {
            const auto& firstCall = recCanvas.textBoxCalls.front();
            CHECK(firstCall.text.ends_with("..."));
            CHECK(firstCall.bounds.Right() <= (widgetBounds.x + widgetBounds.w - 680.0f));
        }
    }

    std::cout << "  [PASS] TestLabelAlignmentGeometryOptionA completed\n";
}

void TestUiTextAndControlCompanionText() {
    using namespace guikit;
    std::cout << "  - TestUiTextAndControlCompanionText...\n";

    static_assert(sizeof(UiText) <= 40);

    const UiTheme& darkTheme = UiTheme::Dark();
    LabelRecordingCanvas recCanvas;
    std::vector<uint32_t> pixels(800 * 600, 0);
    recCanvas.SetTarget(pixels.data(), 800, 600, 800);
    const Rect widgetBounds(20.0f, 20.0f, 400.0f, 120.0f);

    // 1. UiText construction and default vs overridden typography/color resolution
    {
        UiText plain = "Plain";
        CHECK(plain.text == "Plain");
        CHECK(!plain.role.has_value());
        CHECK(!plain.variant.has_value());

        // Default role/color resolution:
        // Label -> LabelLarge (14px at Md), Default (textPrimary)
        CHECK_NEAR(darkTheme.ResolveFontSize(plain, TextRole::LabelLarge, UiSize::Md), 14.0f, 0.01f);
        CHECK(darkTheme.ResolveTextColor(plain, UiVariant::Default) == darkTheme.palette.textPrimary);

        // Companion text (textLeft/textRight/valueText) -> LabelMedium (12px at Md), Secondary (textSecondary)
        CHECK_NEAR(darkTheme.ResolveFontSize(plain, TextRole::LabelMedium, UiSize::Md), 12.0f, 0.01f);
        CHECK(darkTheme.ResolveTextColor(plain, UiVariant::Secondary) == darkTheme.palette.textSecondary);

        // Description -> LabelSmall (11px at Md), Muted (textSecondary)
        CHECK_NEAR(darkTheme.ResolveFontSize(plain, TextRole::LabelSmall, UiSize::Md), 11.0f, 0.01f);
        CHECK(darkTheme.ResolveTextColor(plain, UiVariant::Muted) == darkTheme.palette.textSecondary);

        // Overridden role and variant
        UiText custom = {"Alert", TextRole::TitleMedium, UiVariant::Danger};
        CHECK_NEAR(darkTheme.ResolveFontSize(custom, TextRole::LabelSmall, UiSize::Md),
                   darkTheme.GetTypography(TextRole::TitleMedium).fontSize, 0.01f);
        CHECK(darkTheme.ResolveTextColor(custom, UiVariant::Muted) == darkTheme.GetTextColor(UiVariant::Danger));
    }

    // 2. Switch with external .label + internal .textLeft / .textRight + .description
    {
        recCanvas.Reset();
        SwitchOptions opts{
            .label = "System Theme",
            .textLeft = "Dark",
            .textRight = {"Light", UiVariant::Info},
            .description = {"Applies globally", UiVariant::Warning},
            .layout = ElementLayout::Top,
            .symmetric = true
        };
        Switch::Render(recCanvas, widgetBounds, true, opts, UiSize::Md, false, false, false, darkTheme);

        const auto* lblCall = recCanvas.FindTextBox("System Theme");
        const auto* leftCall = recCanvas.FindTextBox("Dark");
        const auto* rightCall = recCanvas.FindTextBox("Light");
        const auto* descCall = recCanvas.FindTextBox("Applies globally");

        CHECK(lblCall != nullptr);
        CHECK(leftCall != nullptr);
        CHECK(rightCall != nullptr);
        CHECK(descCall != nullptr);

        if (lblCall && leftCall && rightCall && descCall) {
            CHECK(lblCall->color == darkTheme.palette.textPrimary);
            CHECK(leftCall->color == darkTheme.palette.textSecondary);
            CHECK(rightCall->color == darkTheme.GetTextColor(UiVariant::Info));
            CHECK(descCall->color == darkTheme.GetTextColor(UiVariant::Warning));
            // leftText is to the left of rightText
            CHECK(leftCall->bounds.Right() < rightCall->bounds.x);
            // External label is above the control row
            CHECK(lblCall->bounds.Bottom() <= leftCall->bounds.y + 1.0f);
        }
    }

    // 3. Slider with .textLeft, .textRight, .valueFormat, and all 5 .valuePlacement positions
    for (Placement vp : {Placement::Leading, Placement::Trailing, Placement::Top, Placement::Bottom, Placement::Thumb}) {
        recCanvas.Reset();
        SliderOptions opts{
            .label = "Master Gain",
            .textLeft = "0%",
            .textRight = "100%",
            .description = "Preamp level",
            .valueFormat = "%.0f%%",
            .layout = ElementLayout::Stacked,
            .showValue = true,
            .valuePlacement = vp,
            .variant = UiVariant::Danger // Even with Danger variant, value readout defaults to neutral Secondary!
        };

        Vec2 measured = Slider::Measure(opts, 50.0, 0.0, 100.0, UiSize::Md, &recCanvas, darkTheme);
        CHECK(measured.x > 0.0f);
        CHECK(measured.y > 0.0f);

        Slider::Render(recCanvas, Rect(20.0f, 20.0f, 400.0f, measured.y), 50.0, 0.0, 100.0, opts, UiSize::Md, false, false, false, darkTheme);

        const auto* lblCall = recCanvas.FindTextBox("Master Gain");
        const auto* leftCall = recCanvas.FindTextBox("0%");
        const auto* rightCall = recCanvas.FindTextBox("100%");
        const auto* valCall = recCanvas.FindTextBox("50%");

        CHECK(lblCall != nullptr);
        CHECK(leftCall != nullptr);
        CHECK(rightCall != nullptr);
        CHECK(valCall != nullptr);

        if (lblCall && leftCall && rightCall && valCall) {
            // Verify value readout is NOT colored with Danger variant by default, but Secondary!
            CHECK(valCall->color == darkTheme.palette.textSecondary);
            CHECK(leftCall->color == darkTheme.palette.textSecondary);
            CHECK(rightCall->color == darkTheme.palette.textSecondary);

            if (vp == Placement::Top || vp == Placement::Thumb) {
                CHECK(valCall->bounds.Bottom() <= leftCall->bounds.Center().y);
            } else if (vp == Placement::Bottom) {
                CHECK(valCall->bounds.y >= leftCall->bounds.Center().y);
            } else if (vp == Placement::Leading) {
                CHECK(valCall->bounds.x < rightCall->bounds.x);
            } else if (vp == Placement::Trailing) {
                CHECK(valCall->bounds.x > leftCall->bounds.x);
            }
        }
    }

    // 3b. Slider with styled .valueFormat (format + semantic variant)
    {
        recCanvas.Reset();
        SliderOptions opts{
            .label = "Temperature",
            .valueFormat = {"%.1f C", UiVariant::Warning},
            .showValue = true
        };
        Slider::Render(recCanvas, Rect(20.0f, 20.0f, 300.0f, 60.0f), 36.6, 0.0, 100.0, opts, UiSize::Md, false, false, false, darkTheme);
        const auto* valCall = recCanvas.FindTextBox("36.6 C");
        CHECK(valCall != nullptr);
        if (valCall) {
            CHECK(valCall->color == darkTheme.GetTextColor(UiVariant::Warning));
        }
    }

    // 4. InputField with UiText prefix and suffix
    {
        recCanvas.Reset();
        InputFieldOptions opts{
            .label = "Endpoint",
            .prefix = {"https://", UiVariant::Info},
            .suffix = ":443"
        };
        InputField::Render(recCanvas, widgetBounds, "guikit.dev", opts, UiSize::Md, false, false, 0, false, false, 1.5f, darkTheme);
        const auto* prefCall = recCanvas.FindTextBox("https://");
        const auto* sufCall = recCanvas.FindTextBox(":443");
        CHECK(prefCall != nullptr);
        CHECK(sufCall != nullptr);
        if (prefCall && sufCall) {
            CHECK(prefCall->color == darkTheme.GetTextColor(UiVariant::Info));
            CHECK(sufCall->color == darkTheme.palette.textSecondary);
        }
    }

    std::cout << "  [PASS] TestUiTextAndControlCompanionText completed\n";
}

void TestSidebarAndSidebarMenuWorkflow() {
    using namespace guikit;

    // 1. Verify unified typography resolution (Single Source of Truth)
    {
        const UiTheme darkTheme = UiTheme::Dark(Density::Standard);
        const UiText defaultText{"Action"};
        // Primary control text (LabelLarge default role) -> ComponentMetrics::fontSize
        CHECK_NEAR(darkTheme.ResolveFontSize(defaultText, TextRole::LabelLarge, UiSize::Xs), 11.0f, 0.01f);
        CHECK_NEAR(darkTheme.ResolveFontSize(defaultText, TextRole::LabelLarge, UiSize::Sm), 13.0f, 0.01f);
        CHECK_NEAR(darkTheme.ResolveFontSize(defaultText, TextRole::LabelLarge, UiSize::Md), 14.0f, 0.01f);
        CHECK_NEAR(darkTheme.ResolveFontSize(defaultText, TextRole::LabelLarge, UiSize::Lg), 16.0f, 0.01f);
        CHECK_NEAR(darkTheme.ResolveFontSize(defaultText, TextRole::LabelLarge, UiSize::Xl), 18.0f, 0.01f);

        // Companion text (LabelMedium default role) -> ComponentMetrics::companionFontSize
        CHECK_NEAR(darkTheme.ResolveFontSize(defaultText, TextRole::LabelMedium, UiSize::Sm), 11.0f, 0.01f);
        CHECK_NEAR(darkTheme.ResolveFontSize(defaultText, TextRole::LabelMedium, UiSize::Md), 12.0f, 0.01f);

        // Description text (LabelSmall default role) -> ComponentMetrics::descFontSize
        CHECK_NEAR(darkTheme.ResolveFontSize(defaultText, TextRole::LabelSmall, UiSize::Sm), 10.0f, 0.01f);
        CHECK_NEAR(darkTheme.ResolveFontSize(defaultText, TextRole::LabelSmall, UiSize::Md), 11.0f, 0.01f);
    }

    // 2. Test multiple Sidebars docking in call order (Left1, Left2, Right1, Right2) + Center Workspace
    Context ui;
    const uint32_t width = 1000;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    ThorCanvas canvas;
    canvas.SetTarget(pixels.data(), width, height, width);

    Id sbLeft1Id, sbLeft2Id, sbRight1Id, sbRight2Id;
    Response centerBtn;

    auto renderMultiSidebar = [&]() {
        if (auto workspace = ui.Row({.gap = 10.0f, .padding = Padding::All(0.0f), .heightMode = SizeMode::Stretch})) {
            // Left Sidebar #1: Fixed width 120px -> docks at x = 0..120
            if (auto sb1 = ui.Sidebar({.side = SidebarSide::Left, .width = 120.0f})) {
                sbLeft1Id = sb1.GetNode()->id;
                ui.Text("Rail 1");
            }
            // Left Sidebar #2: Fixed width 200px -> docks right after Left #1 at x = 130..330
            if (auto sb2 = ui.Sidebar({.side = SidebarSide::Left, .width = 200.0f})) {
                sbLeft2Id = sb2.GetNode()->id;
                ui.Text("Inspector 2");
            }
            // Right Sidebar #1: Fixed width 150px -> docks at right edge x = 850..1000
            if (auto sb3 = ui.Sidebar({.side = SidebarSide::Right, .width = 150.0f})) {
                sbRight1Id = sb3.GetNode()->id;
                ui.Text("Right 1");
            }
            // Right Sidebar #2: Fixed width 100px -> docks to the left of Right #1 at x = 740..840
            if (auto sb4 = ui.Sidebar({.side = SidebarSide::Right, .width = 100.0f})) {
                sbRight2Id = sb4.GetNode()->id;
                ui.Text("Right 2");
            }
            // Remaining child occupies the remaining center space: x = 340..730 (width = 390)
            centerBtn = ui.Button({.text = "Center Workspace", .stretch = StretchMode::Fill});
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMultiSidebar();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMultiSidebar();
    ui.EndFrame(canvas);

    const Rect rLeft1  = ui.GetPrevFrameBounds(sbLeft1Id);
    const Rect rLeft2  = ui.GetPrevFrameBounds(sbLeft2Id);
    const Rect rRight1 = ui.GetPrevFrameBounds(sbRight1Id);
    const Rect rRight2 = ui.GetPrevFrameBounds(sbRight2Id);
    const Rect rCenter = ui.GetPrevFrameBounds(centerBtn.id);

    CHECK_NEAR(rLeft1.x, 0.0f, 0.01f);
    CHECK_NEAR(rLeft1.w, 120.0f, 0.01f);
    CHECK_NEAR(rLeft1.h, 600.0f, 0.01f);

    CHECK_NEAR(rLeft2.x, 130.0f, 0.01f);
    CHECK_NEAR(rLeft2.w, 200.0f, 0.01f);

    CHECK_NEAR(rRight1.x, 850.0f, 0.01f);
    CHECK_NEAR(rRight1.w, 150.0f, 0.01f);

    CHECK_NEAR(rRight2.x, 740.0f, 0.01f);
    CHECK_NEAR(rRight2.w, 100.0f, 0.01f);

    CHECK_NEAR(rCenter.x, 340.0f, 0.01f);
    CHECK_NEAR(rCenter.w, 390.0f, 0.01f);

    // 3. Test Auto-Fit Sidebar width + SidebarMenu (Scope API & Collapsed mode)
    int activeMenuVal = 0;
    bool menuCollapsed = false;
    Id fitSidebarId;
    Response item0Res, item1Res;

    auto renderAutoFitMenu = [&]() {
        if (auto workspace = ui.Row({.gap = 8.0f, .heightMode = SizeMode::Stretch})) {
            if (auto sb = ui.Sidebar({.side = SidebarSide::Left})) {
                fitSidebarId = sb.GetNode()->id;
                if (auto menu = ui.SidebarMenu({
                    .style = SidebarMenuStyle::Indicator,
                    .collapsed = menuCollapsed,
                    .size = UiSize::Sm
                })) {
                    menu.Section("NAVIGATION");
                    item0Res = menu.Item(activeMenuVal, 0, {.text = "Dashboard", .icon = Icon::Settings(), .badge = "4"});
                    item1Res = menu.Item(activeMenuVal, 1, {.text = "Analytics", .icon = Icon::Search()});
                    menu.Separator();
                }
            }
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderAutoFitMenu();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderAutoFitMenu();
    ui.EndFrame(canvas);

    const Rect expandedSbBounds = ui.GetPrevFrameBounds(fitSidebarId);
    const Rect item1Bounds = ui.GetPrevFrameBounds(item1Res.id);
    CHECK(expandedSbBounds.w > 90.0f);
    CHECK(item1Bounds.w > 70.0f);

    // Click second menu item ("Analytics")
    ui.OnMouseMove(item1Bounds.Center());
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderAutoFitMenu();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderAutoFitMenu();
    ui.EndFrame(canvas);

    CHECK(activeMenuVal == 1);

    // Collapse SidebarMenu into icon-only rail -> auto-fit Sidebar width shrinks!
    menuCollapsed = true;
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderAutoFitMenu();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderAutoFitMenu();
    ui.EndFrame(canvas);

    const Rect collapsedSbBounds = ui.GetPrevFrameBounds(fitSidebarId);
    CHECK(collapsedSbBounds.w > 24.0f);
    CHECK(collapsedSbBounds.w < expandedSbBounds.w);

    // 4. Test declarative Span/Initializer-list overload of ui.SidebarMenu and collapsed ignoring explicit width
    int spanIdx = 0;
    bool fixedSbCollapsed = false;
    Id fixedSbId = Id::Null();
    Response spanMenuRes;
    auto renderSpanMenu = [&]() {
        if (auto sb = ui.Sidebar({.width = 200.0f})) {
            fixedSbId = sb.GetNode()->id;
            spanMenuRes = ui.SidebarMenu(spanIdx, {
                {.text = "First", .icon = Icon::Play()},
                {.text = "Second", .icon = Icon::Check(), .badge = "9"}
            }, {.collapsed = fixedSbCollapsed});
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderSpanMenu();
    ui.EndFrame(canvas);
    CHECK(spanMenuRes.id.IsValid());
    CHECK(spanIdx == 0);
    CHECK_NEAR(ui.GetPrevFrameBounds(fixedSbId).w, 200.0f, 0.5f);

    fixedSbCollapsed = true;
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderSpanMenu();
    ui.EndFrame(canvas);
    CHECK(ui.GetPrevFrameBounds(fixedSbId).w < 80.0f);

    std::cout << "  [PASS] TestSidebarAndSidebarMenuWorkflow completed\n";
}

void TestUiTextModesAndVerticalSpaceReservation() {
    using namespace guikit;
    std::cout << "  - TestUiTextModesAndVerticalSpaceReservation...\n";

    const UiTheme& darkTheme = UiTheme::Dark();
    LabelRecordingCanvas recCanvas;
    std::vector<uint32_t> pixels(800 * 600, 0);
    recCanvas.SetTarget(pixels.data(), 800, 600, 800);

    // 1. Single-line truncation with "..." when text exceeds bounds
    {
        recCanvas.Reset();
        InputFieldOptions opts{
            .label = "Very long field label that should be truncated with ellipsis in a narrow box",
            .description = "Very long description text that should also be cropped on a single line",
            .layout = ElementLayout::Stacked
        };
        const Vec2 m = InputField::Measure("", opts, UiSize::Md, &recCanvas, darkTheme, 120.0f);
        InputField::Render(recCanvas, Rect(10.0f, 10.0f, 120.0f, m.y), "", opts, UiSize::Md, false, false, 0, false, false, 1.5f, darkTheme);

        CHECK(recCanvas.textBoxCalls.size() == 2);
        if (recCanvas.textBoxCalls.size() == 2) {
            CHECK(recCanvas.textBoxCalls[0].text.ends_with("..."));
            CHECK(recCanvas.textBoxCalls[1].text.ends_with("..."));
        }
    }

    // 2. Multi-line word wrapping (TextWrap::Word) + Max lines truncation (TextLines::Max(2))
    {
        recCanvas.Reset();
        InputFieldOptions singleOpts{
            .label = "Username",
            .description = {"Single line description", TextWrap::SingleLine},
            .layout = ElementLayout::Stacked
        };
        InputFieldOptions multiOpts{
            .label = "Username",
            .description = {
                "This validation description is intentionally long so that it wraps across multiple lines in a narrow container",
                UiVariant::Danger,
                TextWrap::Word,
                TextLines::Max(2)
            },
            .layout = ElementLayout::Stacked
        };

        const Vec2 singleM = InputField::Measure("", singleOpts, UiSize::Md, &recCanvas, darkTheme, 140.0f);
        const Vec2 multiM = InputField::Measure("", multiOpts, UiSize::Md, &recCanvas, darkTheme, 140.0f);
        const float descFontSize = darkTheme.ResolveFontSize(multiOpts.description, TextRole::LabelSmall, UiSize::Md);
        const float descLineH = descFontSize * kDefaultLineHeightMultiplier;

        // Multi-line with Max(2) must be exactly 1 description line taller than single-line
        CHECK_NEAR(multiM.y - singleM.y, descLineH, 0.01f);

        InputField::Render(recCanvas, Rect(10.0f, 10.0f, 140.0f, multiM.y), "", multiOpts, UiSize::Md, false, false, 0, false, false, 1.5f, darkTheme);
        // 1 call for "Username" + 2 calls for the 2 wrapped lines of description
        CHECK(recCanvas.textBoxCalls.size() == 3);
        if (recCanvas.textBoxCalls.size() == 3) {
            CHECK(recCanvas.textBoxCalls[0].text == "Username");
            CHECK(recCanvas.textBoxCalls[2].text.ends_with("..."));
        }
    }

    // 3. Zero vertical layout shift when conditional error description appears/disappears (TextLines::Reserve(1))
    {
        Context ui;
        std::string email = "user@";
        bool hasError = false;
        Response inputRes;
        Response submitBtnRes;

        auto renderForm = [&]() {
            if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
                inputRes = ui.InputField(email, {
                    .label = "Email",
                    .description = {
                        hasError ? "Invalid email address format" : "",
                        UiVariant::Danger,
                        TextLines::Reserve(1)
                    }
                });
                submitBtnRes = ui.Button({.text = "Submit"});
            }
        };

        // Frame 1: No error (description is empty, but 1 line is reserved)
        recCanvas.Reset();
        hasError = false;
        ui.BeginFrame({260.0f, 300.0f});
        renderForm();
        ui.EndFrame(recCanvas);

        const Rect btnRectNoError = ui.GetPrevFrameBounds(submitBtnRes.id);
        const Rect inputRectNoError = ui.GetPrevFrameBounds(inputRes.id);
        CHECK(recCanvas.FindTextBox("Invalid email address format") == nullptr);

        // Frame 2: Error appears!
        recCanvas.Reset();
        hasError = true;
        ui.BeginFrame({260.0f, 300.0f});
        renderForm();
        ui.EndFrame(recCanvas);

        const Rect btnRectWithError = ui.GetPrevFrameBounds(submitBtnRes.id);
        const Rect inputRectWithError = ui.GetPrevFrameBounds(inputRes.id);
        CHECK(recCanvas.FindTextBox("Invalid email address format") != nullptr);

        // Zero vertical shift!
        CHECK_NEAR(inputRectNoError.h, inputRectWithError.h, 0.001f);
        CHECK_NEAR(btnRectNoError.y, btnRectWithError.y, 0.001f);
    }

    // 4. Zero vertical shift for multi-line fixed reservation (TextWrap::Word + TextLines::Fixed(2))
    {
        Context ui;
        std::string password;
        int errorStage = 0; // 0 = no error, 1 = 1-line error, 2 = 3-line error (clamped to 2 lines)
        Response submitBtnRes;

        auto renderPasswordForm = [&]() {
            std::string_view errText = "";
            if (errorStage == 1) {
                errText = "Too short";
            } else if (errorStage == 2) {
                errText = "Password must be at least 12 characters long and include uppercase, lowercase, numbers, and special symbols";
            }

            if (auto col = ui.Column({.gap = 10.0f, .padding = Padding::All(12.0f)})) {
                ui.InputField(password, {
                    .label = "Password",
                    .description = {
                        errText,
                        UiVariant::Danger,
                        TextWrap::Word,
                        TextLines::Fixed(2)
                    }
                });
                submitBtnRes = ui.Button({.text = "Continue"});
            }
        };

        errorStage = 0;
        ui.BeginFrame({180.0f, 300.0f});
        renderPasswordForm();
        ui.EndFrame(recCanvas);
        const float btnYStage0 = ui.GetPrevFrameBounds(submitBtnRes.id).y;

        errorStage = 1;
        ui.BeginFrame({180.0f, 300.0f});
        renderPasswordForm();
        ui.EndFrame(recCanvas);
        const float btnYStage1 = ui.GetPrevFrameBounds(submitBtnRes.id).y;

        errorStage = 2;
        ui.BeginFrame({180.0f, 300.0f});
        renderPasswordForm();
        ui.EndFrame(recCanvas);
        const float btnYStage2 = ui.GetPrevFrameBounds(submitBtnRes.id).y;

        CHECK_NEAR(btnYStage0, btnYStage1, 0.001f);
        CHECK_NEAR(btnYStage0, btnYStage2, 0.001f);
    }

    // 5. Standalone ui.Text with TextWrap::Word and TextLines::Reserve
    {
        Context ui;
        bool showNotice = false;
        Response belowBtn;

        auto renderTextDemo = [&]() {
            if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(10.0f)})) {
                ui.Text({
                    showNotice ? "This standalone text widget wraps across two lines cleanly" : "",
                    UiVariant::Secondary,
                    TextWrap::Word,
                    TextLines::Fixed(2)
                });
                belowBtn = ui.Button({.text = "OK"});
            }
        };

        showNotice = false;
        ui.BeginFrame({160.0f, 300.0f});
        renderTextDemo();
        ui.EndFrame(recCanvas);
        const float yHidden = ui.GetPrevFrameBounds(belowBtn.id).y;

        showNotice = true;
        recCanvas.Reset();
        ui.BeginFrame({160.0f, 300.0f});
        renderTextDemo();
        ui.EndFrame(recCanvas);
        const float yShown = ui.GetPrevFrameBounds(belowBtn.id).y;

        CHECK_NEAR(yHidden, yShown, 0.001f);
        CHECK(recCanvas.textBoxCalls.size() >= 2); // Wrapped text + button text
    }

    std::cout << "  [PASS] TestUiTextModesAndVerticalSpaceReservation completed\n";
}

void TestSelectItemsAndUnifiedAffixes() {
    using namespace guikit;
    std::cout << "  - TestSelectItemsAndUnifiedAffixes...\n";

    const UiTheme& darkTheme = UiTheme::Dark();
    LabelRecordingCanvas recCanvas;
    std::vector<uint32_t> pixels(800 * 600, 0);
    recCanvas.SetTarget(pixels.data(), 800, 600, 800);

    // 1. Unified UiAffix in InputField: icon only, text only, and combined icon + text
    {
        recCanvas.Reset();
        InputFieldOptions opts{
            .label = "Endpoint",
            .prefix = {Icon::Search(), "https://", UiVariant::Info},
            .suffix = {"Verified", Icon::Check(), UiVariant::Success}
        };
        CHECK(opts.prefix.HasIcon());
        CHECK(opts.prefix.HasText());
        CHECK(opts.prefix.iconPlacement == Placement::Leading);
        CHECK(opts.suffix.HasIcon());
        CHECK(opts.suffix.HasText());
        CHECK(opts.suffix.iconPlacement == Placement::Trailing);

        InputField::Render(recCanvas, Rect(20.0f, 20.0f, 400.0f, 70.0f), "guikit.dev", opts, UiSize::Md, false, false, 0, false, false, 1.5f, darkTheme);

        const auto* pCall = recCanvas.FindTextBox("https://");
        const auto* valCall = recCanvas.FindTextBox("guikit.dev");
        const auto* sCall = recCanvas.FindTextBox("Verified");
        CHECK(pCall != nullptr);
        CHECK(valCall != nullptr);
        CHECK(sCall != nullptr);
        if (pCall && valCall && sCall) {
            CHECK(pCall->bounds.Right() <= valCall->bounds.x + 1.0f);
            CHECK(valCall->bounds.Right() <= sCall->bounds.x + 1.0f);
            CHECK(pCall->color == darkTheme.GetTextColor(UiVariant::Info));
            CHECK(sCall->color == darkTheme.GetTextColor(UiVariant::Success));
        }
    }

    // 2. Select with SelectItem (per-item prefix, suffix, and disabled items)
    {
        Context ui;
        size_t selectedIdx = 0;
        Response selRes;

        const std::vector<SelectItem> items = {
            {.text = "1920x1080", .prefix = "FHD", .suffix = {"60 Hz", UiVariant::Muted}},
            {.text = "2560x1440", .prefix = {Icon::Settings(), "QHD"}, .suffix = {"144 Hz", UiVariant::Info}},
            {.text = "3840x2160", .prefix = "4K", .suffix = {"Pro", Icon::Check(), UiVariant::Warning}, .disabled = true},
            {.text = "5120x2880", .prefix = "5K", .suffix = {"240 Hz", UiVariant::Success}}
        };

        auto renderSelect = [&]() {
            if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(16.0f)})) {
                selRes = ui.Select(selectedIdx, items, {
                    .label = "Resolution",
                    .stretch = StretchMode::Compact
                });
            }
        };

        // Frame 1 & 2: Closed Select box shows item 0's prefix ("FHD"), text ("1920x1080"), and suffix ("60 Hz")
        ui.BeginFrame({500.0f, 400.0f});
        renderSelect();
        ui.EndFrame(recCanvas);

        recCanvas.Reset();
        ui.InvalidateAll();
        ui.BeginFrame({500.0f, 400.0f});
        renderSelect();
        ui.EndFrame(recCanvas);

        const auto* fhdCall = recCanvas.FindTextBox("FHD");
        const auto* textCall = recCanvas.FindTextBox("1920x1080");
        const auto* hzCall = recCanvas.FindTextBox("60 Hz");
        CHECK(fhdCall != nullptr);
        CHECK(textCall != nullptr);
        CHECK(hzCall != nullptr);
        if (fhdCall && textCall && hzCall) {
            CHECK(fhdCall->bounds.Right() <= textCall->bounds.x + 1.0f);
            CHECK(textCall->bounds.x < hzCall->bounds.x);
        }

        // Click to open popup
        const Rect selBounds = ui.GetPrevFrameBounds(selRes.id);
        ui.OnMouseMove(selBounds.Center());
        ui.OnMouseDown(1);
        ui.BeginFrame({500.0f, 400.0f});
        renderSelect();
        ui.EndFrame(recCanvas);

        recCanvas.Reset();
        ui.OnMouseUp(1);
        ui.BeginFrame({500.0f, 400.0f});
        renderSelect();
        ui.EndFrame(recCanvas);

        CHECK(ui.IsPopupOpen(selRes.id));
        // Popup should render all items' prefixes and suffixes
        CHECK(recCanvas.FindTextBox("QHD") != nullptr);
        CHECK(recCanvas.FindTextBox("144 Hz") != nullptr);
        CHECK(recCanvas.FindTextBox("4K") != nullptr);
        CHECK(recCanvas.FindTextBox("Pro") != nullptr);
        CHECK(recCanvas.FindTextBox("5K") != nullptr);
        CHECK(recCanvas.FindTextBox("240 Hz") != nullptr);

        // Press Key::Down -> moves from index 0 to index 1 ("2560x1440")
        ui.OnKeyDown(Key::Down);
        ui.BeginFrame({500.0f, 400.0f});
        renderSelect();
        ui.EndFrame(recCanvas);
        CHECK(selectedIdx == 1);
        CHECK(selRes.changed);

        // Press Key::Down again -> skips disabled index 2 ("3840x2160") and lands on index 3 ("5120x2880")!
        ui.OnKeyDown(Key::Down);
        ui.BeginFrame({500.0f, 400.0f});
        renderSelect();
        ui.EndFrame(recCanvas);
        CHECK(selectedIdx == 3);
        CHECK(selRes.changed);

        // Press Key::Up -> skips disabled index 2 and lands back on index 1!
        ui.OnKeyDown(Key::Up);
        ui.BeginFrame({500.0f, 400.0f});
        renderSelect();
        ui.EndFrame(recCanvas);
        CHECK(selectedIdx == 1);
        CHECK(selRes.changed);

        // Try clicking on disabled item 2 in popup -> should NOT select index 2 or close popup
        const Rect popupRect = ui.GetOpenPopupRect();
        const float itemH = darkTheme.GetSelect(UiSize::Md).itemHeight;
        const Vec2 item2Center(popupRect.x + popupRect.w * 0.5f, popupRect.y + 4.0f + 2.5f * itemH);
        ui.OnMouseMove(item2Center);
        ui.OnMouseDown(1);
        ui.BeginFrame({500.0f, 400.0f});
        renderSelect();
        ui.EndFrame(recCanvas);

        ui.OnMouseUp(1);
        ui.BeginFrame({500.0f, 400.0f});
        renderSelect();
        ui.EndFrame(recCanvas);

        CHECK(selectedIdx == 1);
        CHECK(ui.IsPopupOpen(selRes.id));
    }

    std::cout << "  [PASS] TestSelectItemsAndUnifiedAffixes completed\n";
}

void TestSelectPopupScrollingWithMaxVisibleItems() {
    using namespace guikit;
    const UiTheme& darkTheme = UiTheme::Dark();
    LabelRecordingCanvas recCanvas;
    std::vector<uint32_t> pixels(800 * 600, 0);
    recCanvas.SetTarget(pixels.data(), 800, 600, 800);

    const std::array<SelectItem, 8> items = {{
        {.text = "Option 0", .suffix = "S0"},
        {.text = "Option 1", .suffix = "S1"},
        {.text = "Option 2", .suffix = "S2"},
        {.text = "Option 3", .suffix = "S3"},
        {.text = "Option 4", .suffix = "S4"},
        {.text = "Option 5", .suffix = "S5"},
        {.text = "Option 6", .suffix = "S6"},
        {.text = "Option 7", .suffix = "S7"}
    }};

    // 1. Verify Compact Select reserves scrollbar gutter width when items.size() > maxVisibleItems
    {
        Context ui;
        size_t idxA = 0;
        size_t idxB = 0;
        WidgetResponse resNoScroll;
        WidgetResponse resScroll;

        auto renderPair = [&]() {
            if (auto col = ui.Column({.gap = 8.0f})) {
                resNoScroll = ui.Select(idxA, std::span<const SelectItem>(items), {
                    .maxVisibleItems = 10,
                    .stretch = StretchMode::Compact
                });
                resScroll = ui.Select(idxB, std::span<const SelectItem>(items), {
                    .maxVisibleItems = 3,
                    .stretch = StretchMode::Compact
                });
            }
        };

        ui.BeginFrame({500.0f, 300.0f});
        renderPair();
        ui.EndFrame(recCanvas);

        ui.BeginFrame({500.0f, 300.0f});
        renderPair();
        ui.EndFrame(recCanvas);

        const float expectedGutter = darkTheme.GetScrollbar(UiSize::Md).gutter;
        CHECK_NEAR(resScroll.rect.w - resNoScroll.rect.w, expectedGutter, 0.5f);
    }

    // 2. Interactive popup scrolling: culling, keyboard auto-scroll, wheel isolation from ScrollArea, thumb drag, item click
    {
        Context ui;
        size_t selectedIdx = 0;
        WidgetResponse selRes;

        auto renderScene = [&]() {
            if (auto sa = ui.ScrollArea()) {
                if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
                    selRes = ui.Select(selectedIdx, std::span<const SelectItem>(items), {
                        .label = "Scrollable Select",
                        .maxVisibleItems = 3
                    });
                    // Extra tall content so parent ScrollArea is scrollable if it receives wheel events
                    for (int i = 0; i < 15; ++i) {
                        ui.Button({.label = "Background Filler Row"});
                    }
                }
            }
        };

        // Initial layout passes
        ui.BeginFrame({500.0f, 300.0f});
        renderScene();
        ui.EndFrame(recCanvas);

        ui.BeginFrame({500.0f, 300.0f});
        renderScene();
        ui.EndFrame(recCanvas);

        const float initialSelectY = selRes.rect.y;

        // Open the Select popup
        ui.OnMouseMove(selRes.rect.Center());
        ui.OnMouseDown(1);
        ui.BeginFrame({500.0f, 300.0f});
        renderScene();
        ui.EndFrame(recCanvas);

        recCanvas.Reset();
        ui.OnMouseUp(1);
        ui.BeginFrame({500.0f, 300.0f});
        renderScene();
        ui.EndFrame(recCanvas);

        CHECK(ui.IsPopupOpen(selRes.id));
        const Rect popupRect = ui.GetOpenPopupRect();
        const float itemH = darkTheme.GetSelect(UiSize::Md).itemHeight;
        CHECK_NEAR(popupRect.h, 3.0f * itemH + 8.0f, 0.5f);

        // At scrollY == 0: Options 0, 1, 2 are visible; Options 3..7 are culled
        CHECK(recCanvas.FindTextBox("Option 0") != nullptr);
        CHECK(recCanvas.FindTextBox("Option 1") != nullptr);
        CHECK(recCanvas.FindTextBox("Option 2") != nullptr);
        CHECK(recCanvas.FindTextBox("Option 3") == nullptr);
        CHECK(recCanvas.FindTextBox("Option 7") == nullptr);

        // Navigate Down 4 times with keyboard -> selects Option 4 and auto-scrolls it into view
        for (int step = 0; step < 4; ++step) {
            recCanvas.Reset();
            ui.OnKeyDown(Key::Down);
            ui.BeginFrame({500.0f, 300.0f});
            renderScene();
            ui.EndFrame(recCanvas);
        }
        CHECK(selectedIdx == 4);
        // Option 4 must now be visible (along with Options 2 and 3), while Option 0 and Option 1 are culled above
        CHECK(recCanvas.FindTextBox("Option 0") == nullptr);
        CHECK(recCanvas.FindTextBox("Option 1") == nullptr);
        CHECK(recCanvas.FindTextBox("Option 2") != nullptr);
        CHECK(recCanvas.FindTextBox("Option 3") != nullptr);
        CHECK(recCanvas.FindTextBox("Option 4") != nullptr);
        CHECK(recCanvas.FindTextBox("Option 5") == nullptr);

        // Scroll mouse wheel down inside open popup -> scrolls popup to bottom (Option 7 visible),
        // while parent ScrollArea MUST NOT scroll (selRes.rect.y remains unchanged!)
        recCanvas.Reset();
        ui.OnMouseMove(popupRect.Center());
        ui.OnMouseWheel(0.0f, -5.0f);
        ui.BeginFrame({500.0f, 300.0f});
        renderScene();
        ui.EndFrame(recCanvas);

        // Render one more frame so any parent ScrollArea scroll offset change would take effect on layout
        recCanvas.Reset();
        ui.InvalidateAll();
        ui.BeginFrame({500.0f, 300.0f});
        renderScene();
        ui.EndFrame(recCanvas);

        CHECK_NEAR(selRes.rect.y, initialSelectY, 0.01f);
        CHECK(recCanvas.FindTextBox("Option 3") == nullptr);
        CHECK(recCanvas.FindTextBox("Option 5") != nullptr);
        CHECK(recCanvas.FindTextBox("Option 6") != nullptr);
        CHECK(recCanvas.FindTextBox("Option 7") != nullptr);

        // Drag scrollbar thumb back to top -> releasing mouse button must NOT close popup or select an item
        const auto& sm = darkTheme.GetScrollbar(UiSize::Md);
        const float trackX = popupRect.Right() - sm.margin - sm.trackWidth * 0.5f;
        const float bottomThumbY = popupRect.Bottom() - 4.0f - sm.margin - sm.minThumbLength * 0.5f;

        ui.OnMouseMove({trackX, bottomThumbY});
        ui.OnMouseDown(1);
        ui.BeginFrame({500.0f, 300.0f});
        renderScene();
        ui.EndFrame(recCanvas);

        // Drag thumb up to top of popup
        ui.OnMouseMove({trackX, popupRect.y + 8.0f});
        ui.BeginFrame({500.0f, 300.0f});
        renderScene();
        ui.EndFrame(recCanvas);

        recCanvas.Reset();
        ui.OnMouseUp(1);
        ui.BeginFrame({500.0f, 300.0f});
        renderScene();
        ui.EndFrame(recCanvas);

        CHECK(ui.IsPopupOpen(selRes.id));
        CHECK(selectedIdx == 4); // Unchanged by thumb drag release!
        CHECK(recCanvas.FindTextBox("Option 0") != nullptr);
        CHECK(recCanvas.FindTextBox("Option 7") == nullptr);

        // Scroll back to bottom with wheel and click on Option 7 (last visible row in popup)
        ui.OnMouseMove(popupRect.Center());
        ui.OnMouseWheel(0.0f, -10.0f);
        ui.BeginFrame({500.0f, 300.0f});
        renderScene();
        ui.EndFrame(recCanvas);

        const Vec2 lastVisibleRowCenter(popupRect.x + popupRect.w * 0.4f, popupRect.Bottom() - 4.0f - itemH * 0.5f);
        ui.OnMouseMove(lastVisibleRowCenter);
        ui.OnMouseDown(1);
        ui.BeginFrame({500.0f, 300.0f});
        renderScene();
        ui.EndFrame(recCanvas);

        ui.OnMouseUp(1);
        ui.BeginFrame({500.0f, 300.0f});
        renderScene();
        ui.EndFrame(recCanvas);

        CHECK(!ui.IsPopupOpen(selRes.id));
        CHECK(selectedIdx == 7);
        CHECK(selRes.changed);
    }

    std::cout << "  [PASS] TestSelectPopupScrollingWithMaxVisibleItems completed\n";
}

void TestBaseElementsAllInOneEnhancements() {
    using namespace guikit;
    const UiTheme& darkTheme = UiTheme::Dark();
    LabelRecordingCanvas recCanvas;
    std::vector<uint32_t> pixels(800 * 600, 0);
    recCanvas.SetTarget(pixels.data(), 800, 600, 800);

    // 1. Type-safe Node::SetPayload<T> and Node::As<T>() verification
    {
        Node n;
        ButtonOptions bOpts{.label = "Test"};
        n.SetPayload(&bOpts);
        CHECK(n.As<ButtonOptions>() == &bOpts);
        CHECK(n.As<SwitchOptions>() == nullptr);
        CHECK(n.As<InputFieldData>() == nullptr);
    }

    // 2. SwitchOptions::width and CheckboxOptions::width (Compact px width & Fill percentage width)
    {
        Context ui;
        bool sw = true;
        bool cb = true;
        Response swCompact, swWide, cbFill50, cbFill100;

        auto renderWidths = [&]() {
            if (auto col = ui.Column({.gap = 8.0f})) {
                swCompact = ui.Switch(sw, {.label = "Sw", .stretch = StretchMode::Compact});
                swWide = ui.Switch(sw, {.label = "Sw", .stretch = StretchMode::Compact, .width = 240.0f});
                cbFill50 = ui.Checkbox(cb, {.label = "Cb", .stretch = StretchMode::Fill, .width = 50.0f});
                cbFill100 = ui.Checkbox(cb, {.label = "Cb", .stretch = StretchMode::Fill, .width = 100.0f});
            }
        };

        ui.BeginFrame({400.0f, 300.0f});
        renderWidths();
        ui.EndFrame(recCanvas);

        ui.BeginFrame({400.0f, 300.0f});
        renderWidths();
        ui.EndFrame(recCanvas);

        CHECK_NEAR(swWide.rect.w, 240.0f, 0.5f);
        CHECK(swWide.rect.w > swCompact.rect.w);
    }

    // 3. Keyboard Tab / Shift+Tab Focus Traversal & Space / Enter Activation + Tooltip Overlay
    {
        Context ui;
        int btnClicks = 0;
        bool cbChecked = false;
        bool swState = false;
        float sliderVal = 50.0f;
        Response btnRes, cbRes, swRes, slRes;

        auto renderFocusScene = [&]() {
            if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(16.0f)})) {
                btnRes = ui.Button({.label = "FirstBtn", .tooltip = "Action Tooltip Text"});
                if (btnRes.clicked) ++btnClicks;
                cbRes = ui.Checkbox(cbChecked, {.label = "SecondCb"});
                swRes = ui.Switch(swState, {.label = "ThirdSw"});
                slRes = ui.Slider(sliderVal, 0.0f, 100.0f, {.label = "FourthSl", .step = 10.0});
            }
        };

        // Frame 1 & 2: Settle layout
        ui.BeginFrame({500.0f, 400.0f});
        renderFocusScene();
        ui.EndFrame(recCanvas);

        ui.BeginFrame({500.0f, 400.0f});
        renderFocusScene();
        ui.EndFrame(recCanvas);

        CHECK(!ui.GetFocusedId().IsValid());

        // Press Tab -> focuses FirstBtn
        ui.OnKeyDown(Key::Tab);
        ui.BeginFrame({500.0f, 400.0f});
        renderFocusScene();
        ui.EndFrame(recCanvas);
        CHECK(ui.GetFocusedId() == btnRes.id);

        // Press Space while FirstBtn is focused -> activates button!
        ui.OnKeyDown(Key::Space);
        ui.BeginFrame({500.0f, 400.0f});
        renderFocusScene();
        ui.EndFrame(recCanvas);
        CHECK(btnClicks == 1);

        // Press Tab -> focuses SecondCb, then Space toggles cbChecked -> true
        ui.OnKeyDown(Key::Tab);
        ui.OnKeyDown(Key::Space);
        ui.BeginFrame({500.0f, 400.0f});
        renderFocusScene();
        ui.EndFrame(recCanvas);
        CHECK(ui.GetFocusedId() == cbRes.id);
        CHECK(cbChecked == true);

        // Press Tab -> focuses ThirdSw, then Enter toggles swState -> true
        ui.OnKeyDown(Key::Tab);
        ui.OnKeyDown(Key::Enter);
        ui.BeginFrame({500.0f, 400.0f});
        renderFocusScene();
        ui.EndFrame(recCanvas);
        CHECK(ui.GetFocusedId() == swRes.id);
        CHECK(swState == true);

        // Press Tab -> focuses FourthSl, then Right arrow steps sliderVal from 50 -> 60
        ui.OnKeyDown(Key::Tab);
        ui.OnKeyDown(Key::Right);
        ui.BeginFrame({500.0f, 400.0f});
        renderFocusScene();
        ui.EndFrame(recCanvas);
        CHECK(ui.GetFocusedId() == slRes.id);
        CHECK_NEAR(sliderVal, 60.0f, 0.01f);

        // Shift+Tab -> moves focus backward from FourthSl back to ThirdSw
        ui.OnKeyDown(Key::Tab, {.shift = true});
        ui.BeginFrame({500.0f, 400.0f});
        renderFocusScene();
        ui.EndFrame(recCanvas);
        CHECK(ui.GetFocusedId() == swRes.id);

        // Hover over FirstBtn -> renders "Action Tooltip Text" in overlay!
        recCanvas.Reset();
        ui.OnMouseMove(btnRes.rect.Center());
        ui.BeginFrame({500.0f, 400.0f});
        renderFocusScene();
        ui.EndFrame(recCanvas);
        CHECK(recCanvas.FindTextBox("Action Tooltip Text") != nullptr);
    }

    // 4. InputField: Password Masking & Reveal Toggle, Clearable Button, Numeric min/max/step
    {
        Context ui;
        std::string pwdText = "secret";
        std::string searchText = "query123";
        int numVal = 10;
        Response pwdRes, searchRes, numRes;

        auto renderInputs = [&]() {
            if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
                pwdRes = ui.InputField(pwdText, {
                    .label = "Password",
                    .password = true,
                    .passwordReveal = true
                });
                searchRes = ui.InputField(searchText, {
                    .label = "Search",
                    .clearable = true
                });
                numRes = ui.InputField(numVal, {
                    .label = "Count",
                    .min = 0.0,
                    .max = 15.0,
                    .step = 5.0
                });
            }
        };

        ui.BeginFrame({400.0f, 300.0f});
        renderInputs();
        ui.EndFrame(recCanvas);

        recCanvas.Reset();
        ui.InvalidateAll();
        ui.BeginFrame({400.0f, 300.0f});
        renderInputs();
        ui.EndFrame(recCanvas);

        // Password field must render "******" and NOT "secret"
        CHECK(recCanvas.FindTextBox("******") != nullptr);
        CHECK(recCanvas.FindTextBox("secret") == nullptr);

        // Click on the eye reveal button on the right side of pwdRes box -> reveals "secret"!
        const auto& im = darkTheme.GetInputField(UiSize::Md);
        const Vec2 revealBtnPos(pwdRes.rect.Right() - im.paddingX - im.iconSize * 0.5f, pwdRes.rect.Bottom() - im.defaultHeight * 0.5f);
        recCanvas.Reset();
        ui.OnMouseMove(revealBtnPos);
        ui.OnMouseDown(1);
        ui.BeginFrame({400.0f, 300.0f});
        renderInputs();
        ui.EndFrame(recCanvas);
        ui.OnMouseUp(1);

        CHECK(recCanvas.FindTextBox("secret") != nullptr);

        // Click on the clear ('X') button on the right side of searchRes box -> clears searchText!
        const Vec2 clearBtnPos(searchRes.rect.Right() - im.paddingX - im.iconSize * 0.5f, searchRes.rect.Bottom() - im.defaultHeight * 0.5f);
        ui.OnMouseMove(clearBtnPos);
        ui.OnMouseDown(1);
        ui.BeginFrame({400.0f, 300.0f});
        renderInputs();
        ui.EndFrame(recCanvas);
        ui.OnMouseUp(1);

        CHECK(searchText.empty());
        CHECK(searchRes.changed);

        // Focus numRes and press Key::Up twice (step = 5, max = 15): 10 -> 15 -> clamped at 15!
        ui.RequestFocus(numRes.id);
        ui.OnKeyDown(Key::Up);
        ui.BeginFrame({400.0f, 300.0f});
        renderInputs();
        ui.EndFrame(recCanvas);
        CHECK(numVal == 15);
        CHECK(numRes.changed);

        ui.OnKeyDown(Key::Up);
        ui.BeginFrame({400.0f, 300.0f});
        renderInputs();
        ui.EndFrame(recCanvas);
        CHECK(numVal == 15); // Clamped to max = 15

        // Press Key::Down 4 times: 15 -> 10 -> 5 -> 0 -> clamped at 0!
        for (int i = 0; i < 4; ++i) {
            ui.OnKeyDown(Key::Down);
            ui.BeginFrame({400.0f, 300.0f});
            renderInputs();
            ui.EndFrame(recCanvas);
        }
        CHECK(numVal == 0); // Clamped to min = 0
    }

    // 5. Heading unified ElementComposer model (.label / .description aliases, ElementLayout, Align, StretchMode & width)
    {
        Context ui;
        Response hLeft, hCenter, hRight, hTop, hInline, hCompact;
        auto renderElementHeading = [&]() {
            if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(16.0f)})) {
                hLeft = ui.Heading({
                    .label = "LeftH",
                    .description = "SubLeft",
                    .elementAlign = Align::Start
                });
                hCenter = ui.Heading({
                    .label = "CenterH",
                    .description = "SubCenter",
                    .elementAlign = Align::Center,
                    .divider = true,
                    .dividerStyle = HeadingDivider::Trailing
                });
                hRight = ui.Heading({
                    .label = "RightH",
                    .description = "SubRight",
                    .elementAlign = Align::End
                });
                hTop = ui.Heading({
                    .label = "TitleBottom",
                    .description = "OverlineTop",
                    .layout = ElementLayout::Top
                });
                hInline = ui.Heading({
                    .label = "InlineTitle",
                    .description = "InlineSub",
                    .layout = ElementLayout::Inline
                });
                hCompact = ui.Heading({
                    .label = "CompactH",
                    .stretch = StretchMode::Compact,
                    .width = 220.0f
                });
            }
        };

        ui.BeginFrame({800.0f, 600.0f});
        renderElementHeading();
        ui.EndFrame(recCanvas);

        recCanvas.Reset();
        ui.InvalidateAll();
        ui.BeginFrame({800.0f, 600.0f});
        renderElementHeading();
        ui.EndFrame(recCanvas);

        // Compact heading respects explicit pixel width
        CHECK_NEAR(ui.GetPrevFrameBounds(hCompact.id).w, 220.0f, 1.0f);

        // Element alignment sets alignX on subtitle text boxes: Start=0.0, Center=0.5, End=1.0
        const auto* tbSubLeft = recCanvas.FindTextBox("SubLeft");
        const auto* tbSubCenter = recCanvas.FindTextBox("SubCenter");
        const auto* tbSubRight = recCanvas.FindTextBox("SubRight");
        CHECK(tbSubLeft != nullptr);
        CHECK(tbSubCenter != nullptr);
        CHECK(tbSubRight != nullptr);
        if (tbSubLeft && tbSubCenter && tbSubRight) {
            CHECK_NEAR(tbSubLeft->alignX, 0.0f, 0.01f);
            CHECK_NEAR(tbSubCenter->alignX, 0.5f, 0.01f);
            CHECK_NEAR(tbSubRight->alignX, 1.0f, 0.01f);
        }

        // ElementLayout::Top places description ("OverlineTop") at the top of hTop bounds
        const auto* tbOverline = recCanvas.FindTextBox("OverlineTop");
        CHECK(tbOverline != nullptr);
        if (tbOverline) {
            CHECK_NEAR(tbOverline->bounds.y, ui.GetPrevFrameBounds(hTop.id).y, 1.0f);
        }

        // ElementLayout::Inline places description ("InlineSub") to the right of the primary title bar
        const auto* tbInlineSub = recCanvas.FindTextBox("InlineSub");
        CHECK(tbInlineSub != nullptr);
        if (tbInlineSub) {
            CHECK(tbInlineSub->bounds.x > ui.GetPrevFrameBounds(hInline.id).x + 40.0f);
        }
    }

    std::cout << "  [PASS] TestBaseElementsAllInOneEnhancements completed\n";
}

void TestImageSceneAndMarkdownWidgets() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 900;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    static constexpr std::string_view kInlineSvg =
        R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 100" width="200" height="100">)"
        R"(<rect x="0" y="0" width="200" height="100" fill="#38BDF8"/></svg>)";

    static constexpr std::string_view kSampleMd =
        "# Title Heading\n\n"
        "Paragraph with **bold**, *italic*, `code`, and [Link](https://guikit.dev).\n\n"
        "> [!NOTE]\n"
        "> Alert note block.\n\n"
        "```cpp\n"
        "int x = 42;\n"
        "```\n\n"
        "| Col A | Col B |\n"
        "| :--- | :---: |\n"
        "| Val 1 | Val 2 |\n";

    Response imgRes;
    Rect sceneVisRect{};
    float sceneZoom = 0.0f;
    Vec2 itemR1ScreenCenter{};
    Rect itemR1WorldBounds{};
    bool r1Hovered = false;
    bool r1Selected = false;
    size_t selectedCount = 0;
    MarkdownResponse mdRes;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
            imgRes = ui.Image(kInlineSvg, {
                .label = "Inline Vector SVG",
                .stretch = StretchMode::Fill,
                .width = 50.0f,
                .checkerboard = true
            });

            if (auto scene = ui.Scene({
                .worldSize = Vec2{1000.0f, 800.0f},
                .background = SceneBackground::GridDots,
                .height = 200.0f
            })) {
                sceneVisRect = scene.GetVisibleWorldRect();
                sceneZoom = scene.GetZoom();
                scene.DrawImage(kInlineSvg, {0.0f, 0.0f, 200.0f, 100.0f});

                if (auto item = scene.Item({.id = "R1", .title = "10 kOhm", .category = "Resistor"})) {
                    r1Hovered = item.IsHovered();
                    r1Selected = item.IsSelected();
                    scene.DrawRect({300.0f, 100.0f, 60.0f, 20.0f}, Color(16, 185, 129));
                    // Distant text callout must NOT inflate geometry worldBounds
                    scene.DrawText("R1 10k", {300.0f, 60.0f}, 10.0f, Color(255, 255, 255));
                }

                for (const auto& info : scene.GetItems()) {
                    if (info.id == "R1") {
                        itemR1WorldBounds = info.worldBounds;
                    }
                }
                itemR1ScreenCenter = scene.WorldToScreen({330.0f, 110.0f});
                selectedCount = scene.GetSelectedCount();
            }

            mdRes = ui.Markdown(kSampleMd, {
                .codeLineNumbers = true,
                .syntaxHighlight = true,
                .showCopyButton = true
            });
        }
    };

    // Frame 1: Measure & Arrange
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Frame 2: Verify layout geometry, camera state & SceneItem auto-BBox
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // 1. Verify inline SVG natural size query via ThorCanvas
    const Vec2 svgNat = canvas.GetImageSize(kInlineSvg);
    CHECK_NEAR(svgNat.x, 200.0f, 0.5f);
    CHECK_NEAR(svgNat.y, 100.0f, 0.5f);

    // 2. Verify Image widget layout (50% of 768px = 384px wide, 2:1 aspect ratio -> >= 192px tall with label)
    CHECK_NEAR(imgRes.rect.w, 768.0f, 1.0f);
    CHECK(imgRes.rect.h >= 192.0f);

    // 3. Verify Scene 2D camera fit-to-viewport & visible world rect
    CHECK(sceneZoom > 0.1f && sceneZoom <= 1.0f);
    CHECK(sceneVisRect.w > 0.0f && sceneVisRect.h > 0.0f);

    // 3b. Verify SceneItemScope auto-BBox (strictly wraps geometry [300,100,60,20], ignoring text callout at y=60)
    CHECK_NEAR(itemR1WorldBounds.x, 300.0f, 0.5f);
    CHECK_NEAR(itemR1WorldBounds.y, 100.0f, 0.5f);
    CHECK_NEAR(itemR1WorldBounds.w, 60.0f, 0.5f);
    CHECK_NEAR(itemR1WorldBounds.h, 20.0f, 0.5f);
    CHECK(!r1Selected);
    CHECK(selectedCount == 0);

    // 3c. Hover and click on R1 in screen space -> selects R1
    ui.OnMouseMove(itemR1ScreenCenter);
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Next frame reads updated selection state
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(r1Hovered);
    CHECK(r1Selected);
    CHECK(selectedCount == 1);

    // 4. Verify Markdown widget measured non-zero height for headings, alert, code block, and table
    CHECK_NEAR(mdRes.rect.w, 768.0f, 1.0f);
    CHECK(mdRes.rect.h > 180.0f);

    // 5. Verify Image inside a constrained-height Row container never overflows container bounds
    {
        Response stageImgRes;
        for (int frame = 0; frame < 2; ++frame) {
            ui.BeginFrame({900.0f, 300.0f});
            if (auto stageBox = ui.Box({
                .widthMode = SizeMode::Stretch,
                .heightMode = SizeMode::Stretch,
                .padding = Padding::All(24.0f)
            })) {
                if (auto centerRow = ui.Row({
                    .align = Alignment::Center,
                    .justify = Justify::Center,
                    .heightMode = SizeMode::Stretch
                })) {
                    stageImgRes = ui.Image(kInlineSvg, {
                        .label = "A vector image",
                        .description = "This is a vector image",
                        .fit = ObjectFit::Contain,
                        .stretch = StretchMode::Fill,
                        .width = 85.0f,
                        .checkerboard = true
                    });
                }
            }
            ui.EndFrame(canvas);
        }
        CHECK(stageImgRes.rect.y >= 24.0f - 0.5f);
        CHECK(stageImgRes.rect.Bottom() <= 300.0f - 24.0f + 0.5f);
        CHECK(stageImgRes.rect.h <= 300.0f - 48.0f + 0.5f);
    }

    std::cout << "  [PASS] TestImageSceneAndMarkdownWidgets completed\n";
}

void TestUnifiedElementHeaderDecorationsAcrossWidgets() {
    using namespace guikit;
    std::cout << "  [TEST] TestUnifiedElementHeaderDecorationsAcrossWidgets...\n";

    Context ui;
    ThorCanvas canvas;
    std::vector<uint32_t> pixels(800 * 900, 0);
    canvas.SetTarget(pixels.data(), 800, 900, 800);

    // 1. Verify that adding icon, badge, and Element/Control frame to widgets increases measured width/height appropriately
    const UiTheme& theme = ui.GetTheme();
    {
        ButtonOptions baseBtn{.label = "A button", .text = "Click me", .description = "This is a button"};
        ButtonOptions decBtn = baseBtn;
        decBtn.labelIcon = Icon::Edit();
        decBtn.badge = {"v1.0", UiVariant::Info};
        decBtn.background = true;
        decBtn.borderWidth = 1.0f;

        const Vec2 szBase = Button::Measure(baseBtn, UiSize::Md, &canvas, theme);
        const Vec2 szDec  = Button::Measure(decBtn, UiSize::Md, &canvas, theme);
        CHECK(szDec.x > szBase.x);
        CHECK(szDec.y > szBase.y);
    }
    {
        // Verify Control well frame increases measured size for non-intrinsic-box controls (Slider, Switch, Checkbox)
        SliderOptions baseSld{.label = "Gain"};
        SliderOptions wellSld = baseSld;
        wellSld.control.background = true;
        wellSld.control.borderWidth = 1.0f;
        const Vec2 sBase = Slider::Measure(baseSld, 50.0, 0.0, 100.0, UiSize::Md, &canvas, theme);
        const Vec2 sWell = Slider::Measure(wellSld, 50.0, 0.0, 100.0, UiSize::Md, &canvas, theme);
        CHECK(sWell.x > sBase.x);
        CHECK(sWell.y > sBase.y);

        SwitchOptions baseSw{.label = "Power"};
        SwitchOptions wellSw = baseSw;
        wellSw.control.background = true;
        wellSw.control.borderWidth = 1.0f;
        const Vec2 swBase = Switch::Measure(baseSw, UiSize::Md, &canvas, theme);
        const Vec2 swWell = Switch::Measure(wellSw, UiSize::Md, &canvas, theme);
        CHECK(swWell.x > swBase.x);
        CHECK(swWell.y > swBase.y);
    }

    // 2. Render all Element-based widgets with icon/badge and Element/Control frames in a live frame
    float sliderVal = 42.0f;
    size_t stepIdx = 1;
    std::string inputVal = "hello";
    size_t selectIdx = 0;
    bool swVal = true;
    bool cbVal = false;
    static constexpr std::array<std::string_view, 3> items = {"One", "Two", "Three"};

    for (int frame = 0; frame < 2; ++frame) {
        ui.BeginFrame({800.0f, 900.0f});
        if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(12.0f)})) {
            ui.Heading({
                .title = "A heading",
                .subtitle = "This is a heading",
                .icon = Icon::Edit(),
                .badge = {"v2.4", UiVariant::Info},
                .background = true,
                .borderWidth = 1.0f,
                .divider = true
            });
            ui.Element({
                .label = "An element",
                .description = "This is an element",
                .icon = Icon::Settings(),
                .badge = "Core",
                .background = true,
                .borderWidth = 1.0f,
                .divider = true
            });
            ui.Button({
                .label = "A button",
                .text = "Click me",
                .description = "This is a button",
                .icon = Icon::Play(),
                .labelIcon = Icon::Edit(),
                .badge = "Action",
                .background = true,
                .borderWidth = 1.0f,
                .control = {.background = true, .borderWidth = 1.5f}
            });
            ui.Slider(sliderVal, 0.0f, 100.0f, {
                .label = "A slider",
                .description = "This is a slider",
                .icon = Icon::Settings(),
                .badge = "dB",
                .background = true,
                .borderWidth = 1.0f,
                .control = {.background = true, .borderWidth = 1.0f}
            });
            ui.StepSlider(stepIdx, items, {
                .label = "A step slider",
                .description = "This is a step slider",
                .icon = Icon::Settings(),
                .badge = "3-step",
                .background = true,
                .borderWidth = 1.0f,
                .control = {.background = true, .borderWidth = 1.0f}
            });
            ui.InputField(inputVal, {
                .label = "A text input",
                .description = "This is a text input",
                .icon = Icon::Edit(),
                .badge = "UTF-8",
                .background = true,
                .borderWidth = 1.0f,
                .control = {.background = true, .borderWidth = 1.5f}
            });
            ui.Select(selectIdx, items, {
                .label = "A select",
                .description = "This is a select",
                .icon = Icon::Plus(),
                .badge = "Menu",
                .background = true,
                .borderWidth = 1.0f,
                .control = {.background = true, .borderWidth = 1.5f}
            });
            ui.Switch(swVal, {
                .label = "A switch",
                .description = "This is a switch",
                .icon = Icon::Refresh(),
                .badge = "Toggle",
                .background = true,
                .borderWidth = 1.0f,
                .control = {.background = true, .borderWidth = 1.0f}
            });
            ui.Checkbox(cbVal, {
                .label = "A checkbox",
                .description = "This is a checkbox",
                .icon = Icon::Check(),
                .badge = "Bool",
                .background = true,
                .borderWidth = 1.0f,
                .control = {.background = true, .borderWidth = 1.0f}
            });
        }
        ui.EndFrame(canvas);
    }

    std::cout << "  [PASS] TestUnifiedElementHeaderDecorationsAcrossWidgets completed\n";
}

void TestSignalViewWidgetWorkflow() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const uint32_t width = 960;
    const uint32_t height = 540;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    // Generate 240,000 samples (5.0 seconds @ 48 kHz) with a 2 kHz sine + 8 kHz harmonic
    const size_t numSamples = 240000;
    const float sampleRate = 48000.0f;
    std::vector<float> samples(numSamples, 0.0f);
    for (size_t i = 0; i < numSamples; ++i) {
        const float t = static_cast<float>(i) / sampleRate;
        samples[i] = 0.6f * std::sin(2.0f * 3.14159265f * 2000.0f * t) +
                     0.25f * std::sin(2.0f * 3.14159265f * 8000.0f * t);
    }

    float splitRatio = 0.50f;
    float waveformGain = 1.0f;
    double playheadSec = 1.25;
    SignalSelection selection{
        .active = true,
        .startTimeSec = 0.5,
        .endTimeSec = 2.0,
        .hasSpectralBox = true,
        .lowFreqHz = 1000.0f,
        .highFreqHz = 10000.0f
    };
    std::vector<SignalMarker> markers = {
        {1.0, "Cue A", Color{56, 189, 248, 255}},
        {3.5, "Cue B", Color{245, 158, 11, 255}}
    };
    SignalPanelOrder panelOrder = SignalPanelOrder::WaveformTop;
    SignalViewResponse resp{};

    auto renderFrame = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto box = ui.Box({
            .widthMode = SizeMode::Stretch,
            .heightMode = SizeMode::Stretch,
            .padding = Padding::All(8.0f)
        })) {
            resp = ui.SignalView(samples, {
                .sampleRate = sampleRate,
                .panelOrder = panelOrder,
                .splitRatio = splitRatio,
                .amplitudeScale = AmplitudeScale::dBFS,
                .frequencyScale = FrequencyScale::Linear,
                .colormap = Colormap::Inferno,
                .fftWindow = FftWindowType::Hann,
                .fftSize = 512,
                .splitRatioBinding = &splitRatio,
                .waveformGainBinding = &waveformGain,
                .playheadTimeSec = &playheadSec,
                .selection = &selection,
                .markers = &markers,
                .heightMode = SizeMode::Stretch
            });
        }
        ui.EndFrame(canvas);
    };

    // Frame 1 & 2: Initial layout and render
    renderFrame();
    renderFrame();

    CHECK(resp.rect.w > 800.0f);
    CHECK(resp.rect.h > 400.0f);
    CHECK(resp.totalSamples == numSamples);
    CHECK_NEAR(static_cast<float>(resp.totalDurationSec), 5.0f, 0.01f);
    CHECK_NEAR(static_cast<float>(resp.viewStartSec), 0.0f, 0.01f);
    CHECK_NEAR(static_cast<float>(resp.viewEndSec), 5.0f, 0.01f);
    CHECK(resp.selectionPeakDbFs > -10.0f);
    CHECK(resp.selectionRmsDbFs > -15.0f);
    CHECK(resp.selectionCentroidHz > 1500.0f);

    // Verify wheel zoom in on plot center
    const Vec2 centerPt{resp.rect.x + resp.rect.w * 0.5f, resp.rect.y + resp.rect.h * 0.5f};
    ui.OnMouseMove(centerPt);
    ui.OnMouseWheel(0.0f, 2.0f);
    renderFrame();

    CHECK(resp.changed);
    CHECK((resp.viewEndSec - resp.viewStartSec) < 4.5);
    CHECK(resp.cursorInsidePlot);

    // Verify panel order switch (SpectrogramTop)
    panelOrder = SignalPanelOrder::SpectrogramTop;
    renderFrame();
    CHECK(resp.rect.w > 800.0f);

    std::cout << "  [PASS] TestSignalViewWidgetWorkflow completed\n";
}

void TestMapViewWidgetWorkflow() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const uint32_t width = 960;
    const uint32_t height = 540;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    MapCrs crs = MapCrs::EPSG3857_WebMercator;
    std::optional<GeoCoord> reqCenter{};
    std::optional<float> reqZoom{};
    MapViewState lastView{};
    size_t itemCount = 0;

    auto renderFrame = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto map = ui.MapView({
            .crs = crs,
            .coordFormat = GeoCoordFormat::NauticalDMM,
            .initialCenter = {54.1825, 7.8853},
            .initialTileZoom = 7.0f,
            .requestCenterGeo = reqCenter,
            .requestTileZoom = reqZoom,
            .heightMode = SizeMode::Stretch
        })) {
            map.DrawGeoLine({53.8900, 9.1433}, {54.1825, 7.8853}, Color(232, 121, 249, 220), 1.8f);
            map.DrawSeamark({54.1825, 7.8853}, {
                .id = "WP-HELGOLAND",
                .title = "Helgoland Light",
                .category = "IALA Lighthouse"
            });
            lastView = map.GetViewState();
            itemCount = map.GetItems().size();
        }
        reqCenter.reset();
        reqZoom.reset();
        ui.EndFrame(canvas);
    };

    // Frame 1 & 2: Layout and camera initialization
    renderFrame();
    renderFrame();

    CHECK(lastView.tileZoom == 7);
    CHECK(!lastView.visibleTiles.empty());
    CHECK(!lastView.meridians.empty());
    CHECK(!lastView.parallels.empty());
    CHECK(itemCount >= 1);
    CHECK_NEAR(static_cast<float>(lastView.center.lat), 54.1825f, 0.25f);
    CHECK_NEAR(static_cast<float>(lastView.center.lon), 7.8853f, 0.25f);

    // Switch CRS to EPSG:3395 and verify geographic center is preserved
    crs = MapCrs::EPSG3395_WorldMercator;
    renderFrame();
    CHECK(lastView.crs == MapCrs::EPSG3395_WorldMercator);
    CHECK_NEAR(static_cast<float>(lastView.center.lat), 54.1825f, 0.25f);
    CHECK_NEAR(static_cast<float>(lastView.center.lon), 7.8853f, 0.25f);

    // Programmatic camera jump to Strait of Gibraltar (z=8)
    reqCenter = GeoCoord{36.1095, -5.3453};
    reqZoom = 8.0f;
    renderFrame();
    CHECK(lastView.tileZoom == 8);
    CHECK_NEAR(static_cast<float>(lastView.center.lat), 36.1095f, 0.25f);
    CHECK_NEAR(static_cast<float>(lastView.center.lon), -5.3453f, 0.25f);

    // Tab-switching & 600-frame GC regression:
    // Simulate a parent stage container where another tab is active for 605 frames (pruning SceneState),
    // then switch to the MapView tab with dirty-rect tracking enabled and verify full tile grid on Frame 1.
    ui.SetDirtyRectEnabled(true);
    bool showMapTab = false;
    MapViewState tabSwitchView{};
    auto renderTabbedApp = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto stage = ui.Box({.widthMode = SizeMode::Stretch, .heightMode = SizeMode::Stretch})) {
            if (!showMapTab) {
                ui.Text("Other Tab Content");
            } else {
                if (auto map = ui.MapView({
                    .crs = MapCrs::EPSG3857_WebMercator,
                    .coordFormat = GeoCoordFormat::NauticalDMM,
                    .initialCenter = {54.1825, 7.8853},
                    .initialTileZoom = 7.0f,
                    .heightMode = SizeMode::Stretch
                })) {
                    tabSwitchView = map.GetViewState();
                }
            }
        }
        ui.EndFrame(canvas);
    };

    for (int i = 0; i < 605; ++i) {
        renderTabbedApp();
    }

    // Activate MapView tab: very first frame should inherit parent stage bounds and render full z=7 tile grid
    showMapTab = true;
    renderTabbedApp();
    CHECK(ui.HasDirtyDamage());
    CHECK(tabSwitchView.tileZoom == 7);
    CHECK(tabSwitchView.visibleTiles.size() >= 6);
    CHECK(!tabSwitchView.meridians.empty());
    CHECK(!tabSwitchView.parallels.empty());
    CHECK_NEAR(static_cast<float>(tabSwitchView.center.lat), 54.1825f, 0.25f);
    CHECK_NEAR(static_cast<float>(tabSwitchView.center.lon), 7.8853f, 0.25f);

    std::cout << "  [PASS] TestMapViewWidgetWorkflow completed\n";
}

void TestSonarViewWidgetWorkflow() {
    using namespace guikit;
    std::cout << "  - TestSonarViewWidgetWorkflow...\n";

    Context ui;
    ThorCanvas canvas;

    const uint32_t width = 900;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    const uint32_t numBeams = 90;
    const uint32_t numBins = 128;
    std::vector<float> polarMatrix(numBeams * numBins, 0.25f);
    for (size_t i = 0; i < polarMatrix.size(); ++i) {
        polarMatrix[i] = 0.15f + 0.65f * static_cast<float>((i * 17) % 100) / 100.0f;
    }

    std::vector<SonarTarget> targets = {
        {
            .id = 10,
            .callsign = "DIVER-01",
            .classification = SonarTargetClass::Diver,
            .rangeMeters = 180.0f,
            .bearingDeg = 15.0f,
            .speedMetersPerSec = 1.1f,
            .courseDeg = 195.0f,
            .depthMeters = 7.0f,
            .snrDb = 20.5f
        },
        {
            .id = 20,
            .callsign = "UUV-02",
            .classification = SonarTargetClass::UUV,
            .rangeMeters = 240.0f,
            .bearingDeg = 340.0f,
            .speedMetersPerSec = 2.0f,
            .courseDeg = 140.0f,
            .depthMeters = 12.0f,
            .snrDb = 24.0f
        }
    };

    SonarDisplayMode displayMode = SonarDisplayMode::PPI_360;
    SonarScanMode scanMode = SonarScanMode::MultibeamContinuous;
    Colormap colormap = Colormap::Gold;
    int selectedId = 10;
    float eblBrg = 25.0f;
    float vrmRng = 200.0f;
    float rangeZoom = 1.0f;
    SonarViewResponse lastResp{};

    auto renderFrame = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        lastResp = ui.SonarView(polarMatrix, {
            .label = "Test Diver Detection Sonar",
            .badge = "DDS",
            .numBeams = numBeams,
            .numRangeBins = numBins,
            .displayMode = displayMode,
            .scanMode = scanMode,
            .colormap = colormap,
            .maxRangeMeters = 400.0f,
            .sectorApertureDeg = 120.0f,
            .gainDb = 3.0f,
            .tvgAmount = 0.5f,
            .clutterThreshold = 0.12f,
            .persistence = 0.8f,
            .sweepAngleDeg = 45.0f,
            .targets = targets,
            .guardZone = {
                .enabled = true,
                .minRangeMeters = 100.0f,
                .maxRangeMeters = 300.0f,
                .startBearingDeg = 320.0f,
                .endBearingDeg = 40.0f
            },
            .selectedTargetId = &selectedId,
            .eblBearingDeg = &eblBrg,
            .vrmRangeMeters = &vrmRng,
            .viewRangeZoom = &rangeZoom,
            .heightMode = SizeMode::Stretch
        });
        ui.EndFrame(canvas);
    };

    renderFrame();
    renderFrame();

    CHECK(lastResp.id.IsValid());
    CHECK(lastResp.guardZoneAlarm);
    CHECK(lastResp.guardZoneIntruderCount == 2);
    CHECK(lastResp.selectedTargetId == 10);
    CHECK_NEAR(lastResp.visibleMaxRangeMeters, 400.0f, 0.5f);

    // Switch to SectorWedge and RotatingSweep + Jet
    displayMode = SonarDisplayMode::SectorWedge;
    scanMode = SonarScanMode::RotatingSweep;
    colormap = Colormap::Jet;
    renderFrame();
    CHECK(lastResp.displayMode == SonarDisplayMode::SectorWedge);

    // Switch to BScan
    displayMode = SonarDisplayMode::BScan;
    colormap = Colormap::Green;
    renderFrame();
    CHECK(lastResp.displayMode == SonarDisplayMode::BScan);

    std::cout << "  [PASS] TestSonarViewWidgetWorkflow completed\n";
}

void TestSonarAndSignalGpuShaderPipeline() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0xFF112233u);
    canvas.SetTarget(pixels.data(), width, height, width);

    std::vector<float> sonarMatrix(90 * 128, 0.4f);
    std::vector<float> audioSamples(4096, 0.0f);
    for (size_t i = 0; i < audioSamples.size(); ++i) {
        audioSamples[i] = std::sin(static_cast<float>(i) * 0.1f) * 0.5f;
    }

    bool btnToggle = false;
    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(8.0f)})) {
            if (ui.Button({.label = btnToggle ? "Mode B" : "Mode A"})) {
                btnToggle = !btnToggle;
            }
            ui.SonarView(sonarMatrix, {
                .label = "GPU Sonar Test",
                .numBeams = 90,
                .numRangeBins = 128,
                .height = 220.0f
            });
            ui.SignalView(audioSamples, {
                .label = "GPU Signal Test",
                .sampleRate = 48000.0f,
                .height = 220.0f
            });
        }
    };

    // 1. Render with GPU shaders enabled (Frame 1: initial layout, Frame 2: bounds converged)
    canvas.SetGpuShadersEnabled(true);
    CHECK(canvas.SupportsGpuShaders());

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(canvas.HasActiveGpuPasses());
    CHECK(canvas.GetSonarGpuPasses().size() == 1);
    CHECK(canvas.GetSpectrogramGpuPasses().size() == 1);

    const auto& sonarPass = canvas.GetSonarGpuPasses()[0];
    CHECK(sonarPass.numBeams == 90);
    CHECK(sonarPass.numRangeBins == 128);
    CHECK(sonarPass.plotRect.w > 100.0f);
    CHECK(sonarPass.clipRect.w > 100.0f);

    // Verify PunchHole created transparent underlay pixels inside sonarPass.plotRect
    size_t transparentCount = 0;
    const int sx0 = static_cast<int>(sonarPass.plotRect.x) + 4;
    const int sy0 = static_cast<int>(sonarPass.plotRect.y) + 4;
    const int sx1 = static_cast<int>(sonarPass.plotRect.Right()) - 4;
    const int sy1 = static_cast<int>(sonarPass.plotRect.Bottom()) - 4;
    for (int y = sy0; y < sy1; y += 4) {
        for (int x = sx0; x < sx1; x += 4) {
            const uint32_t px = pixels[static_cast<size_t>(y) * width + static_cast<size_t>(x)];
            if ((px >> 24) == 0) {
                transparentCount++;
            }
        }
    }
    CHECK(transparentCount > 50);

    // 2. Partial dirty-rect update on top button (outside SonarView & SignalView) preserves cached GPU passes
    btnToggle = true;
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(!ui.IsFullRepaint());
    CHECK(canvas.HasActiveGpuPasses());
    CHECK(canvas.GetSonarGpuPasses().size() == 1);
    CHECK(canvas.GetSpectrogramGpuPasses().size() == 1);

    // 3. Toggle GPU shaders OFF -> automatic CPU fallback with opaque background in plotRect
    canvas.SetGpuShadersEnabled(false);
    CHECK(!canvas.SupportsGpuShaders());
    CHECK(!canvas.HasActiveGpuPasses());

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    size_t opaqueCount = 0;
    for (int y = sy0; y < sy1; y += 8) {
        for (int x = sx0; x < sx1; x += 8) {
            const uint32_t px = pixels[static_cast<size_t>(y) * width + static_cast<size_t>(x)];
            if ((px >> 24) == 255) {
                opaqueCount++;
            }
        }
    }
    CHECK(opaqueCount > 20);

    // 4. Test real SDL3 OpenGL renderer + SdlGlShaderEngine underlay rendering & pixel readback
    if (SDL_Init(SDL_INIT_VIDEO)) {
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
        SDL_Window* win = nullptr;
        SDL_Renderer* ren = nullptr;
        if (SDL_CreateWindowAndRenderer("GpuTest", width, height, SDL_WINDOW_HIDDEN, &win, &ren)) {
            SdlGlShaderEngine gpuEngine;
            const bool glOk = gpuEngine.Initialize(ren);
            CHECK(glOk);
            CHECK(gpuEngine.IsAvailable());
            canvas.SetGpuShadersEnabled(glOk);
            ui.InvalidateAll();

            SDL_Texture* tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, width, height);
            SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);
            SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND_PREMULTIPLIED);

            for (int f = 0; f < 3; ++f) {
                if (f == 1) {
                    // Mutate sonar matrix & button on frame 1 to exercise glTexSubImage2D & partial updates
                    sonarMatrix[10] = 0.9f;
                    btnToggle = !btnToggle;
                }
                ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
                renderUI();
                ui.EndFrame(canvas);

                SDL_UpdateTexture(tex, nullptr, pixels.data(), width * sizeof(uint32_t));
                SDL_RenderClear(ren);
                if (canvas.HasActiveGpuPasses()) {
                    gpuEngine.RenderUnderlays(canvas, width, height);
                }
                SDL_RenderTexture(ren, tex, nullptr, nullptr);

                if (f == 2) {
                    SDL_Surface* readSurf = SDL_RenderReadPixels(ren, nullptr);
                    CHECK(readSurf != nullptr);
                    if (readSurf) {
                        CHECK(readSurf->w == static_cast<int>(width));
                        CHECK(readSurf->h == static_cast<int>(height));
                        SDL_DestroySurface(readSurf);
                    }
                }

                SDL_RenderPresent(ren);
            }

            // 5. Verify mouse crosshair movement over SignalView uses StaticBaseLayerCache (RestoreRegion),
            //    keeps partial dirty rect (!IsFullRepaint), preserves GPU underlay pass, and runs in < 2.0 ms/frame.
            const Rect specPlot = canvas.GetSpectrogramGpuPasses()[0].plotRect;
            double totalCrosshairMs = 0.0;
            constexpr int kMoveFrames = 20;
            for (int m = 0; m < kMoveFrames; ++m) {
                ui.OnMouseMove({specPlot.x + 25.0f + static_cast<float>(m * 4), specPlot.y + 15.0f + static_cast<float>((m % 5) * 3)});
                const auto t0 = std::chrono::steady_clock::now();
                ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
                renderUI();
                ui.EndFrame(canvas);
                const auto t1 = std::chrono::steady_clock::now();
                totalCrosshairMs += std::chrono::duration<double, std::milli>(t1 - t0).count();

                CHECK(ui.HasDirtyDamage());
                CHECK(!ui.IsFullRepaint());
                CHECK(canvas.HasActiveGpuPasses());
                CHECK(canvas.GetSpectrogramGpuPasses().size() == 1);
            }
            const double avgCrosshairMs = totalCrosshairMs / static_cast<double>(kMoveFrames);
            std::cout << "  [BENCH] SignalView mouse crosshair move CPU frame time: " << avgCrosshairMs << " ms/frame\n";
            CHECK(avgCrosshairMs < 5.0);

            gpuEngine.Shutdown();
            SDL_DestroyTexture(tex);
            SDL_DestroyRenderer(ren);
            SDL_DestroyWindow(win);
        }
        SDL_Quit();
    }

    std::cout << "  [PASS] TestSonarAndSignalGpuShaderPipeline completed\n";
}

void TestWaterfallViewWidgetAndGpuRingBuffer() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    canvas.SetGpuShadersEnabled(true);

    const uint32_t width = 960;
    const uint32_t height = 640;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    constexpr uint32_t kBins = 256;
    std::vector<float> specSlice(kBins, -95.0f);
    std::vector<float> doaSlice(181, -85.0f);

    // Inject synthetic peaks
    for (uint32_t i = 0; i < kBins; ++i) {
        const float d1 = (static_cast<float>(i) - 64.0f) / 3.0f;
        const float d2 = (static_cast<float>(i) - 128.0f) / 4.0f;
        specSlice[i] = -95.0f + 65.0f * std::exp(-0.5f * d1 * d1) + 48.0f * std::exp(-0.5f * d2 * d2);
    }
    for (size_t i = 0; i < doaSlice.size(); ++i) {
        const float deg = -90.0f + static_cast<float>(i);
        const float b1 = (deg + 25.0f) / 3.5f;
        const float b2 = (deg - 32.0f) / 4.0f;
        doaSlice[i] = -85.0f + 58.0f * std::exp(-0.5f * b1 * b1) + 46.0f * std::exp(-0.5f * b2 * b2);
    }

    WaterfallTrack tracks[2] = {
        {.id = 101, .callsign = "TGT-A", .status = WaterfallTrackStatus::Tracking, .currentX = -25.0f, .driftRatePerSec = 0.45f, .levelDb = -27.0f, .snrDb = 24.5f, .gateWidth = 6.0f},
        {.id = 102, .callsign = "TGT-B", .status = WaterfallTrackStatus::Tracking, .currentX = 32.0f, .driftRatePerSec = -0.20f, .levelDb = -39.0f, .snrDb = 17.2f, .gateWidth = 6.0f}
    };

    uint64_t seq = 1;
    float f0 = 250.0f;
    WaterfallViewResponse respSpec;
    WaterfallViewResponse respDoa;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(12.0f)})) {
            respSpec = ui.WaterfallView(specSlice, {
                .label = "RTSA Spectrum",
                .mode = WaterfallMode::SpectrumRtsa,
                .viewLayout = WaterfallLayout::SplitTraceAndWaterfall,
                .colormap = Colormap::Turbo,
                .numBins = kBins,
                .historyRows = 128,
                .historyDurationSec = 10.0f,
                .minX = 0.0f,
                .maxX = 1000.0f,
                .sliceSequence = seq,
                .dbFloor = -110.0f,
                .dbCeil = -10.0f,
                .thresholdGateDb = -65.0f,
                .showLiveTrace = true,
                .showPeakHold = true,
                .showAverageTrace = true,
                .showThresholdGate = true,
                .showPeakMarkers = true,
                .showHarmonicComb = true,
                .harmonicFundamentalX = &f0,
                .height = 250.0f
            });

            respDoa = ui.WaterfallView(doaSlice, {
                .label = "DOA BTR Tracker",
                .mode = WaterfallMode::DoaBtr,
                .viewLayout = WaterfallLayout::SplitTraceAndWaterfall,
                .colormap = Colormap::Blue,
                .numBins = static_cast<uint32_t>(doaSlice.size()),
                .historyRows = 128,
                .historyDurationSec = 15.0f,
                .minX = -90.0f,
                .maxX = 90.0f,
                .sliceSequence = seq,
                .dbFloor = -100.0f,
                .dbCeil = -15.0f,
                .thresholdGateDb = -60.0f,
                .tracks = tracks,
                .height = 250.0f
            });
        }
    };

    // Frame 1..3: Layout and ingest first 3 slices
    for (int f = 0; f < 3; ++f) {
        seq = static_cast<uint64_t>(f + 1);
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderUI();
        ui.EndFrame(canvas);
    }

    CHECK(respSpec.rect.w > 200.0f);
    CHECK(respSpec.rect.h > 200.0f);
    CHECK(respDoa.rect.w > 200.0f);
    CHECK(respDoa.rect.h > 200.0f);
    CHECK(respSpec.populatedRows >= 3);
    CHECK(respDoa.populatedRows >= 3);
    CHECK(respSpec.dominantPeakDb > -40.0f);
    CHECK(respDoa.dominantPeakDb > -40.0f);
    CHECK(canvas.GetWaterfallGpuPasses().size() == 2);

    // Pause ingestion (keep seq constant) and test mouse crosshair movement over WaterfallView uses StaticBaseLayerCache
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    const Rect wfRect = canvas.GetWaterfallGpuPasses()[0].plotRect;
    ui.OnMouseMove({wfRect.x + 40.0f, wfRect.y + 30.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(respSpec.hovered);
    CHECK(respSpec.cursorInside);
    CHECK(!ui.IsFullRepaint());
    CHECK(canvas.GetWaterfallGpuPasses().size() == 2);

    std::cout << "  [PASS] TestWaterfallViewWidgetAndGpuRingBuffer completed\n";
}

void TestRangeSliderProgressBarSpinBoxAndRadioButton() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 700;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    float lowFreq = 20.0f;
    float highFreq = 60.0f;
    float progressVal = 72.5f;
    float vuLevel = 78.0f;
    int spinVal = 10;
    int radioMode = 0;

    WidgetResponse rRange;
    WidgetResponse rProg;
    WidgetResponse rVu;
    WidgetResponse rSpin;
    WidgetResponse rRadio0;
    WidgetResponse rRadio1;
    WidgetResponse rRadio2;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
            rRange = ui.RangeSlider(lowFreq, highFreq, 0.0f, 100.0f, {
                .label = "Passband Window",
                .description = "Band-pass filter cutoff interval",
                .valueFormat = "%.0f .. %.0f MHz",
                .step = 1.0,
                .minSpan = 5.0,
                .layout = ElementLayout::Stacked,
                .showValue = true
            });

            rProg = ui.ProgressBar(progressVal, 0.0f, 100.0f, {
                .label = "Calibration Progress",
                .valueFormat = "%.0f %%",
                .layout = ElementLayout::Stacked,
                .showValue = true
            });

            rVu = ui.ProgressBar(vuLevel, 0.0f, 100.0f, {
                .label = "RF Input Level Meter",
                .valueFormat = "%.0f dBuV",
                .peakHoldValue = 90.0,
                .segments = 24,
                .layout = ElementLayout::Stacked,
                .showValue = true,
                .multiZoneColor = true
            });

            rSpin = ui.InputField(spinVal, {
                .label = "Channel Index",
                .layout = ElementLayout::Stacked,
                .min = 0.0,
                .max = 20.0,
                .step = 2.0,
                .stepButtons = true
            });

            rRadio0 = ui.RadioButton(radioMode, 0, {.label = "CW Mode"});
            rRadio1 = ui.RadioButton(radioMode, 1, {.label = "FM Mode"});
            rRadio2 = ui.RadioButton(radioMode, 2, {.label = "QAM Mode"});
        }
    };

    // Frame 1 & 2: Measure & Layout
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rRange.rect.w > 200.0f);
    CHECK(rProg.rect.w > 200.0f);
    CHECK(rVu.rect.w > 200.0f);
    CHECK(rSpin.rect.w > 100.0f);
    CHECK(rRadio0.rect.w > 40.0f);
    CHECK(rRadio1.rect.w > 40.0f);
    CHECK(rRadio2.rect.w > 40.0f);

    // 1. Test RadioButton selection: click rRadio2 to select mode 2
    ui.OnMouseMove({rRadio2.rect.x + 10.0f, rRadio2.rect.y + 8.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(rRadio2.clicked);
    CHECK(rRadio2.changed);
    CHECK(radioMode == 2);

    // 2. Test SpinBox [+] button (located at right end of rSpin input box)
    const float spinBoxBottomY = rSpin.rect.y + rSpin.rect.h - 16.0f;
    const float plusBtnX = rSpin.rect.x + rSpin.rect.w - 14.0f;
    const float minusBtnX = rSpin.rect.x + rSpin.rect.w - 42.0f;

    ui.OnMouseMove({plusBtnX, spinBoxBottomY});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(rSpin.changed);
    CHECK(spinVal == 12);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Click [-] button to decrement back to 10
    ui.OnMouseMove({minusBtnX, spinBoxBottomY});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(rSpin.changed);
    CHECK(spinVal == 10);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // 3. Test RangeSlider center span drag (at 40% of track width, which is inside [20%, 60%])
    const float rangeTrackY = rRange.rect.y + rRange.rect.h - 10.0f;
    const float spanCenterX = rRange.rect.x + rRange.rect.w * 0.40f;
    ui.OnMouseMove({spanCenterX, rangeTrackY});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Drag span right by 10% of width
    ui.OnMouseMove({spanCenterX + rRange.rect.w * 0.10f, rangeTrackY});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(lowFreq > 25.0f);
    CHECK(highFreq > 65.0f);
    CHECK_NEAR(highFreq - lowFreq, 40.0f, 1.0f); // Span preserved!

    std::cout << "  [PASS] TestRangeSliderProgressBarSpinBoxAndRadioButton completed\n";
}

void TestTableVirtualizedDataGridWorkflow() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    constexpr size_t kTotalRows = 100000;
    int selectedRow = 0;
    int sortCol = 1;
    TableSortDirection sortDir = TableSortDirection::Ascending;
    bool row0Checked = false;
    size_t visStart = 0;
    size_t visEnd = 0;
    bool sortChanged = false;
    bool selChanged = false;
    Rect row0CheckRect{};

    int pendingJump = -1;
    Rect tableBounds{};
    Rect firstProgressRect{};

    static const std::array<TableColumn, 5> cols = {{
        {.title = "Track",  .width = 70.0f,  .align = Align::Center, .sortable = true},
        {.title = "ID",     .width = 100.0f, .sortable = true},
        {.title = "Signal", .flex = 1.5f,    .minWidth = 80.0f,      .sortable = true},
        {.title = "RSSI",   .flex = 1.2f,    .minWidth = 90.0f,      .sortable = true},
        {.title = "State",  .width = 110.0f, .align = Align::Center, .sortable = true}
    }};

    auto renderUI = [&]() {
        if (auto col = ui.Column({.padding = Padding::All(16.0f), .widthMode = SizeMode::Stretch, .heightMode = SizeMode::Stretch})) {
            if (auto table = ui.Table(cols, {
                .label = "Virtualized Signal Table",
                .totalRows = kTotalRows,
                .selectedRow = &selectedRow,
                .sortColumn = &sortCol,
                .sortDirection = &sortDir,
                .showRowNumbers = true,
                .showFooter = true,
                .height = 360.0f
            })) {
                tableBounds = table.GetNode()->computedRect;
                if (pendingJump >= 0) {
                    table.ScrollToRow(static_cast<size_t>(pendingJump));
                    pendingJump = -1;
                }
                visStart = table.FirstVisibleRow();
                visEnd = table.EndVisibleRow();
                sortChanged = table.SortChanged();
                selChanged = table.SelectionChanged();

                for (size_t r = visStart; r < visEnd; ++r) {
                    if (auto row = table.Row(r)) {
                        if (auto cell = table.Cell({.align = Align::Center})) {
                            if (r == 0) {
                                auto rChk = ui.Checkbox(row0Checked, {.size = UiSize::Sm});
                                row0CheckRect = rChk.rect;
                            } else {
                                bool dummy = (r % 2 == 0);
                                ui.Checkbox(dummy, {.size = UiSize::Sm});
                            }
                        }
                        table.CellText(ui.Format("ID-{:06}", r));
                        table.CellText("OFDM-64 Carrier");
                        if (auto cell = table.Cell()) {
                            auto rProg = ui.ProgressBar(-65.0f, -100.0f, -30.0f, {.size = UiSize::Xs});
                            if (r == 50000) {
                                firstProgressRect = rProg.rect;
                            }
                        }
                        table.CellBadge("LOCKED", UiVariant::Success);
                    }
                }
            }
        }
    };

    // Frame 1 & 2: Measure & Layout
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // 1. Verify O(1) virtualization: out of 100,000 rows, only ~14-20 rows are materialized
    CHECK(visStart == 0);
    CHECK(visEnd > 5);
    CHECK(visEnd < 30);
    CHECK(row0CheckRect.w > 10.0f);

    // 2. Click embedded Checkbox inside Row 0 Cell 0
    ui.OnMouseMove({row0CheckRect.x + 5.0f, row0CheckRect.y + 5.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(row0Checked == true);

    // 3. Click row 3 in the table body to select it
    const float row3Y = row0CheckRect.y + 3.0f * 32.0f;
    ui.OnMouseMove({300.0f, row3Y});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(selectedRow == 3);
    CHECK(selChanged == true);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // 4. Press Key::End while table is focused -> jumps selection to row 99,999 and scrolls viewport!
    ui.OnKeyDown(Key::End);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(selectedRow == 99999);
    CHECK(selChanged == true);

    // Next frame: visible range reflects the new scroll position at the bottom of the 100,000-row table
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(visStart > 99970);
    CHECK(visEnd == 100000);

    // 5. Programmatic ScrollToRow(50000) updates FirstVisibleRow/EndVisibleRow on the SAME frame
    pendingJump = 50000;
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(visStart <= 50000);
    CHECK(visEnd > 50000);

    // Settle layout for dirty-rect verification
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // 6. Drag the right divider of Col 3 ("RSSI", preceded by flex Col 2 "Signal")
    // Verify that Col 3's left edge stays pinned (Col 2 does not counter-shrink) and the whole Table is marked dirty.
    const float initialProgX = firstProgressRect.x;
    const float initialProgW = firstProgressRect.w;
    const float col3RightDivX = firstProgressRect.Right() + 6.0f; // cellNode horizontal padding = 6.0f
    const float headerCenterY = row0CheckRect.y - 20.0f;

    ui.OnMouseMove({col3RightDivX, headerCenterY});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Drag divider right by +45px
    ui.OnMouseMove({col3RightDivX + 45.0f, headerCenterY});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Wait one frame for child cell geometry to reflect the new column width
    const Rect dirtyOnResize = ui.GetDirtyRect();
    CHECK(ui.HasDirtyDamage());
    // Entire table width must be covered by the dirty rect (not just the ProgressBar rect)
    CHECK(dirtyOnResize.w >= tableBounds.w - 2.0f);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Left edge of Col 3 must remain stationary while its width grew by ~45px
    CHECK(std::abs(firstProgressRect.x - initialProgX) < 2.0f);
    CHECK(std::abs(firstProgressRect.w - (initialProgW + 45.0f)) < 3.0f);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    std::cout << "  [PASS] TestTableVirtualizedDataGridWorkflow completed\n";
}

void TestCodeEditorIdeAndDebuggerWorkflow() {
    using namespace guikit;
    std::cout << "[TEST] Running TestCodeEditorIdeAndDebuggerWorkflow...\n";

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 900;
    const uint32_t height = 650;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    // 1. Verify built-in multi-language presets
    const auto cppSyn = SyntaxConfig::Cpp();
    const auto matSyn = SyntaxConfig::Matlab();
    const auto jsonSyn = SyntaxConfig::Json();
    const auto pySyn = SyntaxConfig::Python();

    CHECK(cppSyn.enabled && cppSyn.languageName == "C++20");
    CHECK(matSyn.enabled && matSyn.languageName == "MATLAB" && matSyn.transposeApostrophe);
    CHECK(jsonSyn.enabled && jsonSyn.languageName == "JSON" && jsonSyn.highlightJsonKeys);
    CHECK(pySyn.enabled && pySyn.languageName == "Python");

    // 1b. Verify built-in SyntaxPalette color scheme presets
    const auto vsPal = SyntaxPalette::VsCodeDark();
    const auto monoPal = SyntaxPalette::Monokai();
    const auto dracPal = SyntaxPalette::Dracula();
    const auto onePal = SyntaxPalette::OneDark();
    const auto matPal = SyntaxPalette::MatlabClassic();
    const auto solPal = SyntaxPalette::Solarized();
    const auto ghPal = SyntaxPalette::GitHubLight();
    CHECK(vsPal.keyword != monoPal.keyword);
    CHECK(dracPal.keyword != matPal.keyword);
    CHECK(onePal.string != solPal.string);
    CHECK(ghPal.comment != vsPal.comment);

    // 2. Render MATLAB editor with IDE & Debugger features enabled (including showWhitespace & custom SyntaxPalette)
    std::string code =
        "%% Spectral Section\n"
        "function y = filterSig(x)\n"
        "\tw = hann(64, 'periodic');\n"
        "    y = (x') * w;\n"
        "end\n";
    std::vector<size_t> breakpoints = {3};
    std::vector<CodeDiagnostic> diagnostics = {
        {3, 4, 8, DiagnosticSeverity::Warning, "Test warning", "W01"}
    };
    std::vector<CodeInlineValue> inlineValues = {
        {3, "w = 64x1 double"}
    };
    std::vector<CodeDebugVar> debugVars = {
        {"w", "[0.0 .. 1.0]", "double (64x1)"}
    };
    size_t execLine = 3;
    TextAreaResponse lastResp{};

    auto renderIde = [&]() {
        lastResp = ui.TextArea(code, {
            .label = "MATLAB IDE",
            .fixedHeight = 420.0f,
            .autoHeight = false,
            .showLineNumbers = true,
            .syntax = matSyn,
            .syntaxPalette = monoPal,
            .showMinimap = true,
            .showStatusBar = true,
            .showIndentGuides = true,
            .showWhitespace = true,
            .codeFolding = true,
            .enableBreakpoints = true,
            .breakpoints = &breakpoints,
            .executionLine = execLine,
            .executionLinePtr = &execLine,
            .showDebugToolbar = true,
            .diagnostics = diagnostics,
            .inlineValues = inlineValues,
            .debugVariables = debugVars
        });
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);

    CHECK(lastResp.rect.w > 200.0f);
    CHECK(lastResp.rect.h > 200.0f);

    // Click inside editor to focus it
    const Vec2 editorCenter(lastResp.rect.x + 180.0f, lastResp.rect.y + 90.0f);
    ui.OnMouseMove(editorCenter);
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    ui.OnMouseUp(1);

    // 3. Test multi-step Undo (Ctrl+Z) & Redo (Ctrl+Y) and auto-closing brackets at EOL
    ui.OnKeyDown(Key::End, {});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);

    const std::string initialCode = code;
    ui.OnTextInput("A");
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    const std::string afterA = code;
    CHECK(afterA != initialCode);

    ui.OnTextInput("(");
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    const std::string afterParen = code;
    // Auto-close should have inserted "()" at EOL
    CHECK(afterParen.size() == afterA.size() + 2);

    // Undo once -> back to afterA
    ui.OnKeyDown(Key::Z, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    CHECK(code == afterA);

    // Undo second time -> back to initialCode
    ui.OnKeyDown(Key::Z, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    CHECK(code == initialCode);

    // Redo (Ctrl+Y) -> back to afterA
    ui.OnKeyDown(Key::Y, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    CHECK(code == afterA);

    // Undo back to initialCode
    ui.OnKeyDown(Key::Z, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    CHECK(code == initialCode);

    // 4. Test F9 Breakpoint toggle on active cursor line
    const size_t bpCountBefore = breakpoints.size();
    ui.OnKeyDown(Key::F9, {});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    CHECK(lastResp.toggledBreakpointLine > 0);
    CHECK(breakpoints.size() != bpCountBefore);

    // Toggle F9 again to restore original breakpoint on line 3
    ui.OnKeyDown(Key::F9, {});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    breakpoints = {3};
    execLine = 3;

    // 4b. Test automatic line-shift tracking for breakpoints, executionLine, inlineValues, and diagnostics
    ui.OnKeyDown(Key::Home, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);

    // Press Enter at start of line 1 -> shifts line 3 down to line 4
    ui.OnKeyDown(Key::Enter, {});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    CHECK(lastResp.lineDelta == 1);
    CHECK(execLine == 4);
    CHECK(!breakpoints.empty() && breakpoints[0] == 4);
    if (const auto* st = ui.FindState<TextAreaState>(lastResp.id)) {
        CHECK(st->resolvedInlineValues.size() == 1);
        CHECK(st->resolvedInlineValues[0].line == 4);
        CHECK(st->resolvedDiagnostics.size() == 1);
        CHECK(st->resolvedDiagnostics[0].line == 4);
    }

    // Undo Enter -> shifts line 4 back to line 3
    ui.OnKeyDown(Key::Z, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    CHECK(lastResp.lineDelta == -1);
    CHECK(execLine == 3);
    CHECK(!breakpoints.empty() && breakpoints[0] == 3);
    if (const auto* st = ui.FindState<TextAreaState>(lastResp.id)) {
        CHECK(st->resolvedInlineValues.size() == 1);
        CHECK(st->resolvedInlineValues[0].line == 3);
    }

    // Move to line 3 (Down twice from line 1) and toggle comment (Ctrl+/) -> inline value badge must hide on comment line
    ui.OnKeyDown(Key::Down, {});
    ui.OnKeyDown(Key::Down, {});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    CHECK(lastResp.cursorLine == 3);

    ui.OnKeyDown(Key::Slash, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    if (const auto* st = ui.FindState<TextAreaState>(lastResp.id)) {
        CHECK(st->resolvedInlineValues.empty());
    }

    // Uncomment line 3 (Ctrl+/) -> inline value badge reappears on line 3
    ui.OnKeyDown(Key::Slash, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    if (const auto* st = ui.FindState<TextAreaState>(lastResp.id)) {
        CHECK(st->resolvedInlineValues.size() == 1);
        CHECK(st->resolvedInlineValues[0].line == 3);
    }

    // 5. Test Debugger Keyboard Shortcuts (F10 StepOver, F5 Continue, Shift+F5 Stop)
    ui.OnKeyDown(Key::F10, {});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    CHECK(lastResp.debugAction == CodeDebugAction::StepOver);

    ui.OnKeyDown(Key::F5, {});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    CHECK(lastResp.debugAction == CodeDebugAction::Continue);

    ui.OnKeyDown(Key::F5, {.shift = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    CHECK(lastResp.debugAction == CodeDebugAction::Stop);

    // 6. Test Ctrl+F Find overlay opening and searching without modifying code buffer
    ui.OnKeyDown(Key::F, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);

    ui.OnTextInput("hann");
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    // Code buffer must remain untouched while typing in Find overlay!
    CHECK(code == initialCode);

    // Close Find overlay with Escape
    ui.OnKeyDown(Key::Escape, {});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);

    // 7. Test Debug Toolbar grip dragging and debugToolbarAlign presets (Start / Center / End)
    Align dbgAlign = Align::Start;
    auto renderIdeAligned = [&]() {
        lastResp = ui.TextArea(code, {
            .label = "MATLAB IDE",
            .fixedHeight = 420.0f,
            .autoHeight = false,
            .showLineNumbers = true,
            .syntax = matSyn,
            .syntaxPalette = monoPal,
            .showMinimap = true,
            .showStatusBar = true,
            .showIndentGuides = true,
            .showWhitespace = true,
            .codeFolding = true,
            .enableBreakpoints = true,
            .breakpoints = &breakpoints,
            .executionLine = execLine,
            .executionLinePtr = &execLine,
            .showDebugToolbar = true,
            .debugToolbarAlign = dbgAlign,
            .diagnostics = diagnostics,
            .inlineValues = inlineValues,
            .debugVariables = debugVars
        });
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIdeAligned();
    ui.EndFrame(canvas);
    if (const auto* st = ui.FindState<TextAreaState>(lastResp.id)) {
        CHECK(st->lastDebugToolbarAlign == Align::Start);
        CHECK(st->debugToolbarRatio < 0.0f);
    }

    // Switch alignment to Center and grab the 6-dot grip handle on the left of the centered debug toolbar
    dbgAlign = Align::Center;
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIdeAligned();
    ui.EndFrame(canvas);

    // The centered debug toolbar's left grip is near the horizontal center minus half the toolbar width (~95px)
    const float gripX = lastResp.rect.x + lastResp.rect.w * 0.5f - 88.0f;
    const float gripY = lastResp.rect.y + 42.0f;
    ui.OnMouseMove({gripX, gripY});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIdeAligned();
    ui.EndFrame(canvas);
    if (const auto* st = ui.FindState<TextAreaState>(lastResp.id)) {
        CHECK(st->draggingDebugToolbar);
    }

    // Drag the toolbar grip 120px to the right
    ui.OnMouseMove({gripX + 120.0f, gripY});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIdeAligned();
    ui.EndFrame(canvas);
    if (const auto* st = ui.FindState<TextAreaState>(lastResp.id)) {
        CHECK(st->draggingDebugToolbar);
        CHECK(st->debugToolbarRatio > 0.5f);
    }

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIdeAligned();
    ui.EndFrame(canvas);
    if (const auto* st = ui.FindState<TextAreaState>(lastResp.id)) {
        CHECK(!st->draggingDebugToolbar);
        CHECK(st->debugToolbarRatio > 0.5f);
    }

    // Changing debugToolbarAlign to Align::End resets custom ratio back to preset
    dbgAlign = Align::End;
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIdeAligned();
    ui.EndFrame(canvas);
    if (const auto* st = ui.FindState<TextAreaState>(lastResp.id)) {
        CHECK(st->lastDebugToolbarAlign == Align::End);
        CHECK(st->debugToolbarRatio < 0.0f);
    }

    std::cout << "  [PASS] TestCodeEditorIdeAndDebuggerWorkflow completed\n";
}

void TestKeymapAndMarkdownTopTierFeatures() {
    std::cout << "  [RUN] TestKeymapAndMarkdownTopTierFeatures...\n";
    using namespace guikit;

    // 1. Verify centralized Keymap & KeyAction (and Action alias)
    Keymap km = Keymap::Default();
    CHECK(km.Matches(Action::Copy, Key::C, {.ctrl = true}));
    CHECK(km.Matches(Action::Cut, Key::X, {.ctrl = true}));
    CHECK(km.Matches(Action::Paste, Key::V, {.ctrl = true}));
    CHECK(km.Matches(Action::SelectAll, Key::A, {.ctrl = true}));
    CHECK(km.Matches(Action::Find, Key::F, {.ctrl = true}));
    CHECK(km.Matches(Action::MoveLeft, Key::Left, {}));
    CHECK(km.Matches(Action::SelectRight, Key::Right, {.shift = true}));
    CHECK(km.Matches(Action::MoveWordLeft, Key::Left, {.ctrl = true}));
    CHECK(km.Matches(Action::SelectWordRight, Key::Right, {.ctrl = true, .shift = true}));
    CHECK(km.Matches(Action::ZoomIn, Key::Plus, {.ctrl = true}));
    CHECK(km.Matches(Action::ZoomOut, Key::Minus, {.ctrl = true}));

    // Verify custom rebinding
    km.Bind(Action::Copy, KeyChord{Key::K, {.ctrl = true, .alt = true}});
    CHECK(!km.Matches(Action::Copy, Key::C, {.ctrl = true}));
    CHECK(km.Matches(Action::Copy, Key::K, {.ctrl = true, .alt = true}));

    // 2. Verify SyntaxConfig::ForLanguage
    CHECK(SyntaxConfig::ForLanguage("cpp").keywords.size() > 10);
    CHECK(SyntaxConfig::ForLanguage("matlab").lineComment == "%");
    CHECK(SyntaxConfig::ForLanguage("json").preprocessorPrefix == '\0');
    CHECK(SyntaxConfig::ForLanguage("python").lineComment == "#");
    CHECK(SyntaxConfig::ForLanguage("plain").keywords.empty());

    // 3. Verify Markdown Smooth Zoom, Selection, Cursor Navigation, Copy, Search & Task Lists
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 640;
    const uint32_t height = 480;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    std::string clipboardText;
    ui.SetClipboardCallbacks(
        [&]() -> std::string { return clipboardText; },
        [&](std::string_view s) { clipboardText = std::string(s); }
    );

    const std::string_view mdDoc =
        "# Title Heading\n\n"
        "Paragraph with **bold** and `code` tokens.\n\n"
        "- [ ] First interactive task\n"
        "- [x] Second completed task\n\n"
        "```matlab\n"
        "% MATLAB block\n"
        "y = fft(x);\n"
        "```\n";

    float zoom = 1.0f;
    std::string searchQuery;
    MarkdownResponse mdRes{};
    Id scrollId = Id::Null();

    auto renderMd = [&]() {
        if (auto sa = ui.ScrollArea({.padding = Padding::All(4.0f)})) {
            scrollId = sa.GetNode()->id;
            mdRes = ui.Markdown(mdDoc, {
                .allowZoom = true,
                .zoom = &zoom,
                .bodyFontSize = 14.0f,
                .headingScale = 1.1f,
                .codeFontSize = 13.0f,
                .lineHeightScale = 1.2f,
                .documentPadding = 8.0f,
                .blockGap = 12.0f,
                .paragraphIndent = 8.0f,
                .listIndent = 20.0f,
                .blockPadding = 12.0f,
                .blockRadius = 6.0f,
                .codeShowWhitespace = true,
                .codeIndentGuides = true,
                .codePalette = SyntaxPalette::Dracula(),
                .enableSelection = true,
                .enableSearch = true,
                .interactiveTasks = true,
                .searchQuery = searchQuery
            });
        }
    };

    // Frame 1 & 2: Initial layout
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMd();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMd();
    ui.EndFrame(canvas);
    CHECK_NEAR(mdRes.currentZoom, 1.0f, 0.001f);

    // Test Ctrl + MouseWheel zooms Markdown WITHOUT scrolling the parent ScrollArea!
    ui.OnMouseMove({200.0f, 120.0f});
    ui.SetKeyModifiers({.ctrl = true});
    ui.OnMouseWheel(0.0f, 1.0f);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMd();
    ui.EndFrame(canvas);
    ui.SetKeyModifiers({});
    CHECK_NEAR(zoom, 1.1f, 0.001f);
    CHECK_NEAR(mdRes.currentZoom, 1.1f, 0.001f);
    const auto* saState = ui.FindState<ScrollState>(scrollId);
    CHECK(saState != nullptr);
    CHECK_NEAR(saState->offset.y, 0.0f, 0.001f);

    // Click inside Markdown to focus it, then test SelectAll (Ctrl+A) and Copy (Ctrl+C) via KeyAction
    ui.OnMouseMove({100.0f, 40.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMd();
    ui.EndFrame(canvas);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMd();
    ui.EndFrame(canvas);
    CHECK(ui.GetFocusedId() == mdRes.id);

    // SelectAll via KeyAction::SelectAll (Ctrl+A)
    ui.OnKeyDown(Key::A, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMd();
    ui.EndFrame(canvas);
    CHECK(!mdRes.selectedText.empty());
    CHECK(mdRes.selectedText.find("Title Heading") != std::string_view::npos);
    CHECK(mdRes.selectedText.find("y = fft(x);") != std::string_view::npos);

    // Copy via KeyAction::Copy (Ctrl+C)
    ui.OnKeyDown(Key::C, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMd();
    ui.EndFrame(canvas);
    CHECK(!clipboardText.empty());
    CHECK(clipboardText.find("Title Heading") != std::string::npos);

    // Test keyboard cursor & selection movement (MoveDocStart -> SelectWordRight -> Copy)
    ui.OnKeyDown(Key::Home, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMd();
    ui.EndFrame(canvas);
    CHECK(mdRes.selectedText.empty());

    ui.OnKeyDown(Key::Right, {.ctrl = true, .shift = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMd();
    ui.EndFrame(canvas);
    CHECK(mdRes.selectedText == "Title");

    // Test Search match counting
    searchQuery = "task";
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMd();
    ui.EndFrame(canvas);
    CHECK(mdRes.searchMatchCount == 2);
    CHECK(mdRes.activeSearchMatch == 0);

    std::cout << "  [PASS] TestKeymapAndMarkdownTopTierFeatures completed\n";
}

void TestUnifiedColormapHistogramAndImageView() {
    using namespace guikit;
    std::cout << "  -> TestUnifiedColormapHistogramAndImageView...\n";

    // 1. Verify unified Colormap LUTs & inversion symmetry across all 4 scientific widgets
    static_assert(std::is_same_v<decltype(SignalViewOptions::colormap), Colormap>);
    static_assert(std::is_same_v<decltype(SonarViewOptions::colormap), Colormap>);
    static_assert(std::is_same_v<decltype(WaterfallViewOptions::colormap), Colormap>);
    static_assert(std::is_same_v<decltype(ImageViewParams::colormap), Colormap>);

    const auto allNames = GetAllColormapNames();
    CHECK(allNames.size() == static_cast<size_t>(Colormap::Count));
    CHECK(GetColormapName(Colormap::Grayscale) == "Grayscale");
    CHECK(GetColormapName(Colormap::Bone) == "Bone");
    CHECK(GetColormapName(Colormap::Inferno) == "Inferno");
    CHECK(GetColormapName(Colormap::Turbo) == "Turbo");
    CHECK(GetColormapName(Colormap::Red) == "Red");
    CHECK(GetColormapName(Colormap::Green) == "Green");
    CHECK(GetColormapName(Colormap::Blue) == "Blue");
    CHECK(GetColormapName(Colormap::Gold) == "Gold");

    for (size_t m = 0; m < static_cast<size_t>(Colormap::Count); ++m) {
        const auto cmap = static_cast<Colormap>(m);
        const auto& lutNorm = GetColormapLut(cmap, false);
        const auto& lutInv  = GetColormapLut(cmap, true);
        const auto& abgrNorm = GetColormapLutAbgr(cmap, false);
        const auto& abgrInv  = GetColormapLutAbgr(cmap, true);
        CHECK(lutNorm[0].r == lutInv[255].r);
        CHECK(lutNorm[0].g == lutInv[255].g);
        CHECK(lutNorm[0].b == lutInv[255].b);
        CHECK(abgrNorm[0] == abgrInv[255]);
        CHECK(abgrNorm[255] == abgrInv[0]);
    }

    // 2. Verify Quantile Auto Window on a synthetic 256-bin distribution
    std::array<uint32_t, 256> testBins{};
    for (size_t i = 20; i <= 200; ++i) {
        testBins[i] = 100;
    }
    ChannelTransferParams tp{};
    tp.ResetToRange(0.0f, 65535.0f);
    ImageView::ApplyQuantileAutoWindow(testBins, 0.0f, 65535.0f, AutoWindowTarget::All, AutoWindowStrength::Medium, tp);
    CHECK(tp.Low() > 4000.0f);
    CHECK(tp.High() < 55000.0f);
    CHECK(tp.window < 60000.0f);

    // 3. Test standalone multi-channel Histogram & 16-bit / RGB ImageView rendering and interaction
    Context ui;
    ThorCanvas canvas;
    const uint32_t W = 1000;
    const uint32_t H = 700;
    std::vector<uint32_t> framePixels(W * H, 0);
    canvas.SetTarget(framePixels.data(), W, H, W);

    // Prepare 64x48 16-bit single-channel test image and 64x48 3-channel RGB test image
    constexpr uint32_t imgW = 64;
    constexpr uint32_t imgH = 48;
    std::vector<uint16_t> mono16(imgW * imgH, 0);
    std::vector<float> rgbFloat(imgW * imgH * 3, 0.0f);
    for (uint32_t y = 0; y < imgH; ++y) {
        for (uint32_t x = 0; x < imgW; ++y) {
            break;
        }
        for (uint32_t x = 0; x < imgW; ++x) {
            mono16[y * imgW + x] = static_cast<uint16_t>((x * 1000 + y * 500) % 65536);
            const size_t base = (y * imgW + x) * 3;
            rgbFloat[base + 0] = static_cast<float>(x * 4);
            rgbFloat[base + 1] = static_cast<float>(y * 5);
            rgbFloat[base + 2] = static_cast<float>((x + y) * 2);
        }
    }

    ImageViewParams monoParams{};
    ImageViewParams rgbParams{};
    ImageViewResponse monoResp{};
    ImageViewResponse rgbResp{};

    auto renderFrame = [&](bool useRgb) {
        if (useRgb) {
            const auto d = ImageViewData::FromFloat(rgbFloat, imgW, imgH, 3, 11);
            rgbResp = ui.ImageView(d, rgbParams, {.height = 500.0f, .showSidebar = true});
        } else {
            const auto d = ImageViewData::FromU16(mono16, imgW, imgH, 1, 10);
            monoResp = ui.ImageView(d, monoParams, {.height = 500.0f, .showSidebar = true});
        }
    };

    // Frame 1: Initialize 16-bit mono ImageView
    ui.BeginFrame({static_cast<float>(W), static_cast<float>(H)});
    renderFrame(false);
    ui.EndFrame(canvas);

    CHECK(monoParams.initialized);
    CHECK(monoResp.dataMax > monoResp.dataMin);
    CHECK(monoParams.channels[0].window > 1000.0f);

    // Frame 2: Switch colormap to Inferno and rotate 90 deg
    monoParams.colormap = Colormap::Inferno;
    monoParams.rotation = ImageRotation::deg90;
    ui.BeginFrame({static_cast<float>(W), static_cast<float>(H)});
    renderFrame(false);
    ui.EndFrame(canvas);
    CHECK(monoResp.zoom > 0.05f);

    // Frame 3: Initialize 3-channel RGB ImageView
    ui.BeginFrame({static_cast<float>(W), static_cast<float>(H)});
    renderFrame(true);
    ui.EndFrame(canvas);
    CHECK(rgbParams.initialized);
    CHECK(rgbResp.dataMax > rgbResp.dataMin);

    // Frame 4: Test RMB Line Profile drag on the RGB viewport (x=600, y=250 -> x=700, y=300)
    ui.OnMouseMove({600.0f, 250.0f});
    ui.OnMouseDown(3); // RMB
    ui.BeginFrame({static_cast<float>(W), static_cast<float>(H)});
    renderFrame(true);
    ui.EndFrame(canvas);

    ui.OnMouseMove({700.0f, 300.0f});
    ui.BeginFrame({static_cast<float>(W), static_cast<float>(H)});
    renderFrame(true);
    ui.EndFrame(canvas);
    CHECK(rgbResp.profileActive);
    CHECK(rgbResp.profileSampleCount >= 2u);

    ui.OnMouseUp(3);
    ui.BeginFrame({static_cast<float>(W), static_cast<float>(H)});
    renderFrame(true);
    ui.EndFrame(canvas);
    CHECK(rgbResp.profileActive);
}

void TestUnifiedElementExplicitHeight() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 600;
    const uint32_t height = 800;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    std::string inputVal = "Test";
    size_t selectIdx = 0;
    static constexpr std::array<std::string_view, 2> selectItems = {"A", "B"};
    float sliderVal = 50.0f;
    bool switchVal = false;
    bool checkVal = true;

    Response rBtn, rInput, rSelect, rSlider, rSwitch, rCheck, rImg;

    auto renderFrame = [&]() {
        if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(16.0f)})) {
            rBtn = ui.Button({.text = "Tall Button", .height = 64.0f});
            rInput = ui.InputField(inputVal, {.height = 56.0f});
            rSelect = ui.Select(selectIdx, selectItems, {.height = 52.0f});
            rSlider = ui.Slider(sliderVal, 0.0f, 100.0f, {.height = 60.0f});
            rSwitch = ui.Switch(switchVal, {.height = 48.0f});
            rCheck = ui.Checkbox(checkVal, {.height = 44.0f});
            rImg = ui.Image("assets/images/missing_preview.svg", {.width = 120.0f, .height = 90.0f});
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderFrame();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderFrame();
    ui.EndFrame(canvas);

    CHECK(std::abs(rBtn.rect.h - 64.0f) < 1.0f);
    CHECK(std::abs(rInput.rect.h - 56.0f) < 1.0f);
    CHECK(std::abs(rSelect.rect.h - 52.0f) < 1.0f);
    CHECK(std::abs(rSlider.rect.h - 60.0f) < 1.0f);
    CHECK(std::abs(rSwitch.rect.h - 48.0f) < 1.0f);
    CHECK(std::abs(rCheck.rect.h - 44.0f) < 1.0f);
    CHECK(std::abs(rImg.rect.h - 90.0f) < 1.0f);
}

void TestElementOuterFrameAndControlWellFrame() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 800;
    const uint32_t height = 600;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    const UiTheme& theme = ui.GetTheme();

    // 1. Verify Control::Measure & Control::ResolveInnerRect for hasIntrinsicBox = false (well frame) vs true
    ControlSpec wellSpec{
        .background = true,
        .borderWidth = 2.0f,
        .borderRadius = 6.0f,
        .hasIntrinsicBox = false
    };
    const float wellPad = Control::ComputeWellPadding(wellSpec, UiSize::Md, theme);
    CHECK(wellPad > 0.0f);

    const Vec2 rawTrackSize{180.0f, 24.0f};
    const Vec2 measuredWell = Control::Measure(rawTrackSize, wellSpec, StretchMode::Compact, UiSize::Md, theme);
    CHECK_NEAR(measuredWell.x, rawTrackSize.x + wellPad * 2.0f, 0.01f);
    CHECK_NEAR(measuredWell.y, rawTrackSize.y + wellPad * 2.0f, 0.01f);

    const Rect outerCtrlRect{10.0f, 20.0f, measuredWell.x, measuredWell.y};
    const Rect innerCtrlRect = Control::ResolveInnerRect(outerCtrlRect, wellSpec, UiSize::Md, theme);
    CHECK_NEAR(innerCtrlRect.x, outerCtrlRect.x + wellPad, 0.01f);
    CHECK_NEAR(innerCtrlRect.y, outerCtrlRect.y + wellPad, 0.01f);
    CHECK_NEAR(innerCtrlRect.w, rawTrackSize.x, 0.01f);
    CHECK_NEAR(innerCtrlRect.h, rawTrackSize.y, 0.01f);

    ControlSpec intrinsicBoxSpec = wellSpec;
    intrinsicBoxSpec.hasIntrinsicBox = true;
    CHECK_NEAR(Control::ComputeWellPadding(intrinsicBoxSpec, UiSize::Md, theme), 0.0f, 0.001f);

    // 2. Verify Element outer frame padding and Control well frame in full immediate-mode frame
    float sliderVal = 0.5f;
    bool switchVal = true;
    bool checkVal = false;
    Response rSliderPlain, rSliderFramed, rSwitchWell, rCheckWell, rBtnFramed;

    auto renderFrame = [&]() {
        if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(12.0f)})) {
            rSliderPlain = ui.Slider(sliderVal, 0.0f, 1.0f, {.label = "Plain Slider"});
            rSliderFramed = ui.Slider(sliderVal, 0.0f, 1.0f, {
                .label = "Framed Slider",
                .background = true,
                .borderWidth = 1.5f,
                .borderRadius = 8.0f,
                .control = {
                    .background = true,
                    .borderWidth = 1.0f,
                    .borderRadius = 6.0f
                }
            });
            rSwitchWell = ui.Switch(switchVal, {
                .label = "Switch With Well",
                .background = true,
                .control = {
                    .background = true,
                    .borderWidth = 1.0f
                }
            });
            rCheckWell = ui.Checkbox(checkVal, {
                .label = "Checkbox With Well",
                .control = {
                    .background = true,
                    .borderWidth = 1.0f
                }
            });
            rBtnFramed = ui.Button({
                .label = "Outer Framed Button",
                .text = "Click",
                .background = true,
                .borderWidth = 1.0f,
                .control = {
                    .background = false,
                    .borderWidth = 2.0f
                }
            });
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderFrame();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderFrame();
    ui.EndFrame(canvas);

    // Framed slider must be taller than plain slider due to both outer Element frame and inner Control well
    CHECK(rSliderFramed.rect.h > rSliderPlain.rect.h + 12.0f);
    CHECK(rSwitchWell.rect.h > 0.0f);
    CHECK(rCheckWell.rect.h > 0.0f);
    CHECK(rBtnFramed.rect.h > 0.0f);
}

void TestAttitudeViewWidgetWorkflow() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 900;
    const uint32_t height = 650;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    float fovPitch = 50.0f;
    AttitudeDisplayMode mode = AttitudeDisplayMode::FullPFD;
    HorizonReferenceMode refMode = HorizonReferenceMode::WesternMovingHorizon;
    AttitudeColorScheme scheme = AttitudeColorScheme::GlassCockpit;
    float roll = 28.0f;
    float pitch = 12.5f;
    AttitudeViewResponse resp{};

    auto renderFrame = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto col = ui.Column({
            .padding = Padding::All(12.0f),
            .widthMode = SizeMode::Stretch,
            .heightMode = SizeMode::Stretch
        })) {
            resp = ui.AttitudeView({
                .label = "Primary Flight Display",
                .description = "Avionics ADI & Air Data Tapes",
                .badge = UiText("PFD", UiVariant::Info),
                .displayMode = mode,
                .referenceMode = refMode,
                .colorScheme = scheme,
                .flightDirectorStyle = FlightDirectorStyle::SingleCueVBar,
                .aircraftSymbol = AircraftSymbolStyle::GullWing,
                .rollDeg = roll,
                .pitchDeg = pitch,
                .headingDeg = 135.0f,
                .airspeedKnots = 265.0f,
                .machNumber = 0.76f,
                .altitudeFeet = 18400.0f,
                .verticalSpeedFpm = 1450.0f,
                .slipSkid = -0.25f,
                .gLoad = 1.35f,
                .radioAltitudeFeet = 1200.0f,
                .showFlightPathVector = true,
                .showIlsDiamonds = true,
                .locDeviationDots = 0.6f,
                .gsDeviationDots = -0.4f,
                .showMouseStickOverlay = true,
                .fieldOfViewPitchDeg = fovPitch,
                .fovPitchDegPtr = &fovPitch,
                .heightMode = SizeMode::Stretch
            });
        }
        ui.EndFrame(canvas);
    };

    // Frame 1 & 2: Establish layout and verify ADI sphere geometry
    renderFrame();
    renderFrame();
    CHECK(resp.rect.w > 600.0f);
    CHECK(resp.rect.h > 400.0f);
    CHECK(resp.adiRect.w > 200.0f);
    CHECK(resp.adiRadius > 100.0f);
    CHECK_NEAR(resp.visibleFovPitchDeg, 50.0f, 0.01f);

    // Frame 3: Move mouse inside ADI sphere (right and up from center -> positive roll, positive pitch)
    const Vec2 hoverPt{resp.adiCenter.x + resp.adiRadius * 0.5f, resp.adiCenter.y - resp.adiRadius * 0.5f};
    ui.OnMouseMove(hoverPt);
    renderFrame();
    CHECK(resp.hovered);
    CHECK(resp.cursorInside);
    CHECK(resp.cursorInsideAdi);
    CHECK(resp.cursorNorm.x > 0.45f);
    CHECK(resp.cursorNorm.y < -0.45f);
    CHECK(resp.cursorRollDeg > 25.0f);
    CHECK(resp.cursorPitchDeg > 15.0f);

    // Frame 4: Mouse wheel zoom changes pitch FOV
    ui.OnMouseWheel(0.0f, 2.0f);
    renderFrame();
    CHECK(resp.changed);
    CHECK_NEAR(fovPitch, 42.0f, 0.01f);

    // Frame 5: Test CompactADI + Russian KPPI Reference Mode + Inverted Flight (roll = 175 deg, pitch = -45 deg)
    mode = AttitudeDisplayMode::CompactADI;
    refMode = HorizonReferenceMode::EasternMovingAircraft;
    scheme = AttitudeColorScheme::TacticalDark;
    roll = 175.0f;
    pitch = -45.0f;
    renderFrame();
    CHECK(resp.displayMode == AttitudeDisplayMode::CompactADI);

    // Frame 6: Test HudOverlay mode + HudGreen color scheme
    mode = AttitudeDisplayMode::HudOverlay;
    scheme = AttitudeColorScheme::HudGreen;
    roll = -62.0f;
    pitch = 38.0f;
    renderFrame();
    CHECK(resp.displayMode == AttitudeDisplayMode::HudOverlay);
}

void TestCarClusterViewWidgetWorkflow() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const uint32_t width = 960;
    const uint32_t height = 560;
    std::vector<uint32_t> pixels(width * height, 0);
    canvas.SetTarget(pixels.data(), width, height, width);

    ClusterLayoutMode layoutMode = ClusterLayoutMode::DualDial;
    PowertrainMode powertrain = PowertrainMode::IceTurbo;
    ClusterDriveTheme driveTheme = ClusterDriveTheme::ComfortCyan;
    ClusterCenterMfd centerMfd = ClusterCenterMfd::AdasLaneRadar;
    SpeedUnit speedUnit = SpeedUnit::Kmh;
    int gear = 4;
    CarClusterViewResponse resp{};

    auto renderFrame = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto col = ui.Column({
            .padding = Padding::All(12.0f),
            .widthMode = SizeMode::Stretch,
            .heightMode = SizeMode::Stretch
        })) {
            resp = ui.CarClusterView({
                .label = "Digital Instrument Cluster",
                .description = "OEM Cockpit & ADAS Telemetry",
                .badge = UiText("AUTO", UiVariant::Primary),
                .layoutMode = layoutMode,
                .powertrain = powertrain,
                .driveTheme = driveTheme,
                .centerMfd = centerMfd,
                .speedUnit = speedUnit,
                .gearSelector = GearSelector::Sport,
                .speedKmh = 148.0f,
                .rpm = 5800.0f,
                .currentGear = gear,
                .evPowerKw = 185.0f,
                .throttlePct = 64.0f,
                .brakePct = 0.0f,
                .steeringAngleDeg = 14.0f,
                .boostBar = 1.15f,
                .fuelOrSocPct = 72.0f,
                .latG = 0.68f,
                .lonG = 0.42f,
                .showMouseDriveOverlay = true,
                .manualGearPtr = &gear,
                .centerMfdPtr = &centerMfd,
                .heightMode = SizeMode::Stretch
            });
        }
        ui.EndFrame(canvas);
    };

    // Frame 1 & 2: Establish layout and verify DualDial sub-rectangles
    renderFrame();
    renderFrame();
    CHECK(resp.rect.w > 600.0f);
    CHECK(resp.rect.h > 300.0f);
    CHECK(resp.clusterRect.w > 500.0f);
    CHECK(resp.leftGaugeRect.w > 100.0f);
    CHECK(resp.centerMfdRect.w > 80.0f);
    CHECK(resp.rightGaugeRect.w > 100.0f);

    // Frame 3: Hover mouse in upper-right quadrant -> positive steering, positive throttle
    const Vec2 clusterCenter = resp.clusterRect.Center();
    ui.OnMouseMove({clusterCenter.x + resp.clusterRect.w * 0.25f, clusterCenter.y - resp.clusterRect.h * 0.25f});
    renderFrame();
    CHECK(resp.hovered);
    CHECK(resp.cursorInside);
    CHECK(resp.cursorSteerDeg > 15.0f);
    CHECK(resp.cursorThrottlePct > 40.0f);
    CHECK_NEAR(resp.cursorBrakePct, 0.0f, 0.01f);

    // Frame 4: Mouse wheel scroll upshifts manualGearPtr (4 -> 5)
    ui.OnMouseWheel(0.0f, 1.0f);
    renderFrame();
    CHECK(resp.changed);
    CHECK(gear == 5);

    // Frame 5: Click inside Center MFD cycles centerMfdPtr (AdasLaneRadar -> TpmsChassis)
    ui.OnMouseMove(resp.centerMfdRect.Center());
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();
    CHECK(centerMfd == ClusterCenterMfd::TpmsChassis);

    // Frame 6: Test TrackCentral layout + ElectricDualMotor + GForceDynamics + Imperial mph
    layoutMode = ClusterLayoutMode::TrackCentral;
    powertrain = PowertrainMode::ElectricDualMotor;
    driveTheme = ClusterDriveTheme::SportCrimson;
    centerMfd = ClusterCenterMfd::GForceDynamics;
    speedUnit = SpeedUnit::Mph;
    renderFrame();
    CHECK(resp.layoutMode == ClusterLayoutMode::TrackCentral);

    // Frame 7: Test ModernWide layout + TrackGold theme
    layoutMode = ClusterLayoutMode::ModernWide;
    driveTheme = ClusterDriveTheme::TrackGold;
    renderFrame();
    CHECK(resp.layoutMode == ClusterLayoutMode::ModernWide);
}

void TestBpmnEditorWidgetWorkflow() {
    std::cout << "  -> TestBpmnEditorWidgetWorkflow\n";
    Context ui;
    ThorCanvas canvas;
    std::vector<uint32_t> pixels(960 * 600, 0);
    canvas.SetTarget(pixels.data(), 960, 600, 960);

    BpmnDiagram diagram;
    diagram.name = "Unit Test BPMN Process";
    const uint32_t laneId = diagram.AddLane("Test Pool", "Main Lane", Rect(40.0f, 40.0f, 800.0f, 220.0f));
    const uint32_t nStart = diagram.AddNode({
        .type = BpmnNodeType::StartEvent,
        .label = "Start",
        .pos = {100.0f, 120.0f},
        .eventDef = BpmnEventDefinition::Message,
        .laneId = laneId
    });
    const uint32_t nTask = diagram.AddNode({
        .type = BpmnNodeType::Task,
        .label = "Process Order",
        .pos = {220.0f, 106.0f},
        .taskType = BpmnTaskType::Service,
        .laneId = laneId,
        .slaDurationMs = 300.0f
    });
    const uint32_t nBoundary = diagram.AddNode({
        .type = BpmnNodeType::BoundaryEvent,
        .label = "Timeout",
        .eventDef = BpmnEventDefinition::Timer,
        .attachedToNodeId = nTask,
        .boundaryRatioX = 0.70f,
        .laneId = laneId
    });
    const uint32_t nEnd = diagram.AddNode({
        .type = BpmnNodeType::EndEvent,
        .label = "Done",
        .pos = {440.0f, 120.0f},
        .laneId = laneId
    });
    const uint32_t e1 = diagram.AddEdge(nStart, nTask, BpmnFlowType::SequenceFlow);
    const uint32_t e2 = diagram.AddEdge(nTask, nEnd, BpmnFlowType::DefaultFlow, "OK");
    (void)nBoundary;
    CHECK(e1 != 0);
    CHECK(e2 != 0);
    CHECK(diagram.nodes.size() == 4);
    CHECK(diagram.edges.size() == 2);

    // Verify BoundaryEvent snapped to bottom border of nTask
    const BpmnNode* boundPtr = diagram.FindNode(nBoundary);
    const BpmnNode* taskPtr = diagram.FindNode(nTask);
    CHECK(boundPtr != nullptr);
    CHECK(taskPtr != nullptr);
    CHECK_NEAR(boundPtr->pos.y + boundPtr->size.y * 0.5f, taskPtr->pos.y + taskPtr->size.y, 0.5f);

    bool tokenSim = false;
    BpmnRoutingMode routing = BpmnRoutingMode::Orthogonal;
    uint32_t selNode = 0;
    BpmnEditorResponse resp{};

    auto renderFrame = [&]() {
        ui.BeginFrame({960.0f, 600.0f});
        resp = ui.BpmnEditor(diagram, {
            .routingMode = routing,
            .themePreset = BpmnThemePreset::BpmnIoClassic,
            .tokenSimulation = tokenSim,
            .autoSpawnTokens = true,
            .simulationSpeed = 2.0f,
            .showSlaHeatmap = true,
            .tokenSimulationPtr = &tokenSim,
            .routingModePtr = &routing,
            .selectedNodeIdPtr = &selNode
        });
        ui.EndFrame(canvas);
    };

    ui.SetCurrentTime(1.0);
    renderFrame();
    renderFrame();
    CHECK(resp.canvasRect.w > 700.0f);
    CHECK(resp.canvasRect.h > 400.0f);

    // Programmatically select nTask via two-way binding and verify response
    selNode = nTask;
    renderFrame();
    CHECK(resp.selectedNodeId == nTask);

    // Enable Token Simulation and step time so autoSpawnTokens spawns a token
    tokenSim = true;
    ui.AdvanceTime(0.05);
    renderFrame();
    CHECK(resp.simulationActive);
    CHECK(resp.activeTokenCount >= 1);

    // Step simulation forward and switch to Bezier routing
    routing = BpmnRoutingMode::Bezier;
    ui.AdvanceTime(0.5);
    renderFrame();
    CHECK(diagram.FindNode(nStart)->tokenExecutionCount >= 1);
}

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    std::cout << "[RUN] Testing GuiKit Context & Immediate Mode API...\n";

    TestTextWrappingAndBackendIndependence();

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
    TestGenericTemplateFacade();
    TestContextCleanArchitectureAndStateGC();
    TestScopedThemeAndScaleOverrides();
    TestSliderAndStepSliderWidgets();
    TestPixelGeometryAndWidgetUiSizeDecoupling();
    TestTextAreaWidgetWorkflow();
    TestButtonAlignmentAndProceduralIcons();
    TestWidgetRadiusOverride();
    TestButtonExternalLabelDescriptionAndPlacement();
    TestWidgetStretchModesAndExplicitWidth();
    TestLabelAlignmentGeometryOptionA();
    TestUiTextAndControlCompanionText();
    TestSidebarAndSidebarMenuWorkflow();
    TestUiTextModesAndVerticalSpaceReservation();
    TestSelectItemsAndUnifiedAffixes();
    TestSelectPopupScrollingWithMaxVisibleItems();
    TestBaseElementsAllInOneEnhancements();
    TestImageSceneAndMarkdownWidgets();
    TestUnifiedElementHeaderDecorationsAcrossWidgets();
    TestSignalViewWidgetWorkflow();
    TestMapViewWidgetWorkflow();
    TestSonarViewWidgetWorkflow();
    TestSonarAndSignalGpuShaderPipeline();
    TestWaterfallViewWidgetAndGpuRingBuffer();
    TestRangeSliderProgressBarSpinBoxAndRadioButton();
    TestTableVirtualizedDataGridWorkflow();
    TestCodeEditorIdeAndDebuggerWorkflow();
    TestKeymapAndMarkdownTopTierFeatures();
    TestUnifiedColormapHistogramAndImageView();
    TestUnifiedElementExplicitHeight();
    TestElementOuterFrameAndControlWellFrame();
    TestAttitudeViewWidgetWorkflow();
    TestCarClusterViewWidgetWorkflow();
    TestBpmnEditorWidgetWorkflow();

    if (g_failures == 0) {
        std::cout << "[PASS] All GuiKit Context tests passed successfully!\n";
        return 0;
    } else {
        std::cerr << "[FAIL] Total failures: " << g_failures << "\n";
        return 1;
    }
}

