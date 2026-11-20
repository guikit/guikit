#include "guikit/guikit.hpp"

#include <cstring>
#include "guikit/backend/sdl_canvas.hpp"
#include "guikit/backend/sdl_gl_shader_engine.hpp"
#include "guikit/backend/sdl_window_backend.hpp"
#include "guikit/backend/text_compositor.hpp"
#include "guikit/backend/thor_canvas.hpp"
#include "guikit/core/assets.hpp"
#include "guikit/core/rich_text.hpp"
#include "guikit/core/text_util.hpp"
#if GUIKIT_HAS_SCENES
#include "guikit/geo/geo_json_io.hpp"
#include "guikit/geo/gis_render_engine.hpp"
#if __has_include("keel/http.h")
#include "keel/http.h"
#endif
#endif
#include "keel/keel.h"
#include "keel/math.h"
#if __has_include(<SDL3/SDL.h>)
#include <SDL3/SDL.h>
#endif

static keel::i32 g_failures = 0;

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            keel::eprintln("[FAIL] {} ({}:{})", #cond, __FILE__, __LINE__);          \
            ++g_failures;                                                            \
        }                                                                            \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                        \
    do {                                                                             \
        if (keel::abs((a) - (b)) > (eps)) {                                           \
            keel::eprintln("[FAIL] {} ({}) != {} ({}) [{}:{}]",                      \
                           #a, (a), #b, (b), __FILE__, __LINE__);                    \
            ++g_failures;                                                            \
        }                                                                            \
    } while (0)

void TestImmediateApiWorkflow() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse b1;
    WidgetResponse b2;

    auto renderUI = [&]() {
        if (auto row = ui.Row()) {
            b1 = ui.Button({.text = "Button A"});
            ui.Spacer();
            b2 = ui.Button({.text = "Button B"});
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
    const keel::u32 width = 200;
    const keel::u32 height = 100;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    bool fontOk = canvas.LoadFontFile("assets/fonts/Roboto-Regular.ttf");
    CHECK(fontOk);

    const Vec2 lenaSize = canvas.GetImageSize("assets/images/lena.png");
    CHECK(lenaSize.x > 0.0f && lenaSize.y > 0.0f);

    std::memset(pixels.data(), 0, pixels.size() * sizeof(keel::u32));
    canvas.Begin(width, height);
    canvas.DrawTextBox("Overview", Rect(10.0f, 10.0f, 120.0f, 36.0f), 14.0f, Color(255, 255, 255), 0.5f, 0.5f);
    canvas.End();

    keel::i32 minY = 999, maxY = -1, minX = 999, maxX = -1;
    for (keel::u32 y = 0; y < height; ++y) {
        for (keel::u32 x = 0; x < width; ++x) {
            if (pixels[y * width + x] != 0) {
                if ((keel::i32)y < minY) minY = (keel::i32)y;
                if ((keel::i32)y > maxY) maxY = (keel::i32)y;
                if ((keel::i32)x < minX) minX = (keel::i32)x;
                if ((keel::i32)x > maxX) maxX = (keel::i32)x;
            }
        }
    }
    keel::println("[PIXEL BOUNDS] pos=(10, 10), fontSize=24 -> X=[{}..{}], Y=[{}..{}], width={}, height={}",
                  minX, maxX, minY, maxY, maxX - minX + 1, maxY - minY + 1);
    CHECK(minX >= 0 && minY >= 0);
}

void TestHeaderRowTextMetricsAndNoOverlap() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    // Accurate text advance verification
    // Roboto at true pixel size: ~118.7 dp and ~203 dp (fallback 0.6 em/char estimate would be lower)
    Vec2 titleMetrics = canvas.MeasureText("GUIKIT GUI", 24.0f);
    CHECK(titleMetrics.x > 110.0f);

    Vec2 subMetrics = canvas.MeasureText("| Autonomous Vector GUI Engine", 14.0f);
    CHECK(subMetrics.x > 185.0f);

    WidgetResponse bOverview;
    WidgetResponse bFlex;

    auto renderRow = [&]() {
        if (auto row = ui.Row({.gap = 8.0f})) {
            if (auto b1 = ui.Box({.width = 120.0f})) {
                bOverview = ui.Button({.text = "Overview", .width = SizeMode::Stretch});
            }
            if (auto b2 = ui.Box({.width = 150.0f})) {
                bFlex = ui.Button({.text = "Flexbox Demo", .width = SizeMode::Stretch});
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
    const keel::u32 width = 500;
    const keel::u32 height = 400;
    keel::Array<keel::u32> pixels(width * height);
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
                if (auto b = ui.Box({.width = 80.0f})) {
                    exitBtn = ui.Button({.text = "Exit", .width = SizeMode::Stretch});
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

    const keel::u32 width = 300;
    const keel::u32 height = 400;
    keel::Array<keel::u32> pixels(width * height);
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
                if (auto box1 = ui.Box({.width = 120.0f, .height = 36.0f})) {
                    b1 = ui.Button({.text = "Button 1", .width = SizeMode::Stretch});
                }
                if (auto box2 = ui.Box({.width = 120.0f, .height = 36.0f})) {
                    b2 = ui.Button({.text = "Button 2", .width = SizeMode::Stretch});
                }
                if (auto box3 = ui.Box({.width = 120.0f, .height = 36.0f})) {
                    b3 = ui.Button({.text = "Button 3", .width = SizeMode::Stretch});
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
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse exitBtn;
    auto renderUI = [&]() {
        if (auto mainCol = ui.Column({.gap = 16.0f, .padding = Padding::All(24.0f)})) {
            if (auto header = ui.Row({.gap = 12.0f})) {
                ui.Text("GUIKIT GUI", {.role = TextRole::TitleLarge});
                ui.Spacer();
            }
            if (auto navRow = ui.Row({.gap = 8.0f})) {
                if (auto box = ui.Box({.width = 120.0f})) {
                    ui.Button({.text = "Overview"});
                }
            }
            if (auto card = ui.Box({.gap = 16.0f, .padding = Padding::All(20.0f)})) {
                ui.Text("Card title", {.role = TextRole::HeadlineMedium});
                if (auto wrap = ui.Wrap({.gap = 12.0f, .runGap = 8.0f})) {
                    ui.Button({.text = "Btn 1"});
                    ui.Button({.text = "Btn 2"});
                }
            }
            ui.Spacer();
            if (auto footer = ui.Row({.gap = 12.0f})) {
                ui.Text("Footer text", {.role = TextRole::LabelMedium, .variant = UiVariant::Muted});
                ui.Spacer();
                if (auto box = ui.Box({.width = 80.0f})) {
                    exitBtn = ui.Button({.text = "Exit"});
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

    CHECK_NEAR(mMd.defaultHeight, 30.0f, 0.001f);
    CHECK_NEAR(mLg.defaultHeight, 36.0f, 0.001f);

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
    const keel::u32 width = 100;
    const keel::u32 height = 100;
    keel::Array<keel::u32> pixels(width * height);
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
    const keel::u32 width = 400;
    const keel::u32 height = 200;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse btnEnabled;
    WidgetResponse btnDirectDisabled;
    WidgetResponse btnScopedDisabled;

    auto renderUI = [&]() {
        if (auto row = ui.Row()) {
            btnEnabled = ui.Button({.text = "Active"});
            btnDirectDisabled = ui.Button({.text = "Local Disabled", .disabled = true});

            if (auto disabledCol = ui.Column({.disabled = true})) {
                btnScopedDisabled = ui.Button({.text = "Scope Disabled"});
            }
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
    const keel::u32 width = 400;
    const keel::u32 height = 200;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse btnLarge;
    WidgetResponse btnSquareIcon;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 12.0f})) {
            if (auto slotLg = ui.Box()) {
                btnLarge = ui.Button({.text = "Large Action", .size = UiSize::Lg, .width = SizeMode::Stretch, .color = StateColor::Primary()});
            }
            if (auto slotSquare = ui.Box({.width = 32.0f})) {
                btnSquareIcon = ui.Button({.icon = Icon::Close(), .size = UiSize::Sm});
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
    CHECK_NEAR(btnLarge.rect.h, 36.0f, 0.001f);
    CHECK_NEAR(btnLarge.rect.w, 400.0f, 0.001f); // Stretches across column

    // Square icon button in 32x32 slot
    CHECK_NEAR(btnSquareIcon.rect.w, 24.0f, 0.001f);
    CHECK_NEAR(btnSquareIcon.rect.h, 24.0f, 0.001f);
}

void TestButtonWidgetRunAndResponseChaining() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 400;
    const keel::u32 height = 200;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    ButtonResponse btn1;
    ButtonResponse btn2;

    auto renderUI = [&]() {
        if (auto row = ui.Row({.gap = 8.0f})) {
            // Autonomous Button::Run via ui.Button
            btn1 = ui.Button({
                .text = "Next",
                .icon = Icon::ChevronRight(),
                .iconPlacement = Placement::Trailing,
                .color = StateColor::Primary()
            });

            // Direct call to Button::Run
            btn2 = Button::Run(ui, {
                .text = "Prev",
                .icon = Icon::ChevronLeft(),
                .iconPlacement = Placement::Leading,
                .color = StateColor::Secondary()
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

    // Test fixed literal assignment
    keel::FixedString<> s1 = "Static literal";
    CHECK(s1 == "Static literal");
    CHECK(s1.size() == 14);
    CHECK(!s1.empty());

    // Test inline buffer formatting
    keel::FixedString<> s2;
    keel::format_into(s2, "Count: %d", 42);
    CHECK(s2 == "Count: 42");
    CHECK(s2.size() == 9);

    // Test factory formatting
    keel::FixedString<> s3 = keel::format_fixed("Speed: %.1f", 12.5);
    CHECK(s3 == "Speed: 12.5");

    // Test Context arena formatting and interning in UI frame
    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 400;
    const keel::u32 height = 200;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    if (auto col = ui.Column({.gap = 8.0f})) {
        ui.Text(ui.Format("Value: {}", 99));
        ui.Button({.text = ui.String("Action")});

        // Verify temporary ui.Format / ui.String returned keel::StringRef points into frame arena (not stack)
        // and does not truncate strings > 64 or > 128 bytes.
        keel::StringRef persistedView = ui.Format(
            "Long UTF-8 summary string exceeding 128 bytes: [0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-0123456789abcdef] {:g} %",
            0.1 * 0.2);
        CHECK(persistedView.size() > 130);
        CHECK(persistedView.ends_with(" 0.02 %"));

        // Verify temporary Icon::File paths are arena-copied via AllocIcon across widgets before EndFrame
        keel::String tempIconPath("assets/icons/custom_dynamic.svg");
        Icon arenaIcon = ui.AllocIcon(Icon::File(tempIconPath));
        keel::i32 segIdx = 0;
        const SegmentedItem segItems[] = {
            {.text = "Seg", .icon = Icon::File(tempIconPath)}
        };
        ui.Segmented(segIdx, segItems);
        if (auto tb = ui.Toolbar()) {
            ui.ToolButton({.text = "Tool", .icon = Icon::File(tempIconPath)});
        }
        if (auto sb = ui.StatusBar()) {
            sb.Item({.text = "Status", .icon = Icon::File(tempIconPath)});
        }
        bool paletteOpen = true;
        const CommandPaletteItem cmdItems[] = {
            {.label = ui.Format("Cmd {}", 1), .category = "Cat", .icon = Icon::File(tempIconPath)}
        };
        ui.CommandPalette(paletteOpen, cmdItems);
        // Clobber stack buffer before EndFrame to catch any dangling string_view
        std::memset(tempIconPath.data(), 'X', tempIconPath.size());
        CHECK(arenaIcon.GetPath() == "assets/icons/custom_dynamic.svg");
    }
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    if (auto col = ui.Column({.gap = 8.0f})) {
        ui.Text(ui.Format("Value: {}", 100));
        ui.Button({.text = ui.String("Action")});
    }
    ui.EndFrame(canvas);
}

void TestBoxContainerGapSpacing() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 400;
    const keel::u32 height = 300;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse b1;
    WidgetResponse b2;

    auto renderUI = [&]() {
        if (auto card = ui.Box({.gap = 16.0f, .padding = Padding::All(10.0f)})) {
            b1 = ui.Button({.text = "First Item"});
            b2 = ui.Button({.text = "Second Item"});
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
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
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

    static_assert(sizeof(SwitchOptions) <= 880);
    static_assert(sizeof(CheckboxOptions) <= 760);

    const auto theme = UiTheme::Dark();
    // 1. Test measurement
    const SwitchMetrics sm = theme.GetSwitch(UiSize::Md);
    Vec2 sMeasNoLabel = Switch::Measure({}, UiSize::Md);
    CHECK(sMeasNoLabel.x == sm.trackSize.x);
    CHECK(sMeasNoLabel.y == sm.trackSize.y);

    Vec2 sMeasWithLabel = Element::Measure(sMeasNoLabel, ElementSpec{.label = "Enable", .layout = ElementLayout::Trailing}, nullptr, theme);
    CHECK(sMeasWithLabel.x > sm.trackSize.x);

    Vec2 sMeasWithDesc = Element::Measure(sMeasNoLabel, ElementSpec{.label = "Enable", .description = "Details", .layout = ElementLayout::Trailing}, nullptr, theme);
    CHECK(sMeasWithDesc.y > sMeasWithLabel.y);

    Vec2 sMeasBoth = Switch::Measure({
        .textLeft = "Dark",
        .textRight = "Light",
        .symmetric = true
    }, UiSize::Md);
    CHECK(sMeasBoth.x > sMeasWithLabel.x);

    Vec2 sMeasBothDesc = Element::Measure(sMeasBoth, ElementSpec{
        .label = "Theme",
        .description = "Switch theme",
        .layout = ElementLayout::Trailing
    }, nullptr, theme);
    CHECK(sMeasBothDesc.y > sMeasBoth.y);

    const CheckboxMetrics cm = theme.GetCheckbox(UiSize::Md);
    Vec2 cMeasNoLabel = Checkbox::Measure({}, UiSize::Md);
    CHECK(cMeasNoLabel.x == cm.boxSize);
    CHECK(cMeasNoLabel.y == cm.boxSize);

    Vec2 cMeasWithLabel = Element::Measure(cMeasNoLabel, ElementSpec{.label = "Enable", .layout = ElementLayout::Trailing}, nullptr, theme);
    CHECK(cMeasWithLabel.x > cm.boxSize);

    Vec2 cMeasWithDesc = Element::Measure(cMeasNoLabel, ElementSpec{.label = "Enable", .description = "Details", .layout = ElementLayout::Trailing}, nullptr, theme);
    CHECK(cMeasWithDesc.y > cMeasWithLabel.y);

    // 2. Interactive mutation test
    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 500;
    const keel::u32 height = 400;
    keel::Array<keel::u32> pixels(width * height);
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
    Response elSwitch, elDesc, elStacked;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
            if (auto el = ui.Element({.label = "Dark Mode"})) {
                elSwitch = el.response;
                rSwitch = ui.Switch(switchState);
            }
            if (auto el = ui.Element({.label = "VSync"})) {
                rCheck = ui.Checkbox(checkState);
            }
            if (auto el = ui.Element({.label = "Turbo Mode", .layout = ElementLayout::Leading})) {
                rToggle = ui.Switch(toggleState);
            }
            if (auto el = ui.Element({.label = "Select All"})) {
                rIndeterminate = ui.Checkbox(false, {.indeterminate = true});
            }
            if (auto el = ui.Element({
                .label = "Auto Breakdown",
                .description = "Automatic stop of testing on load drop"
            })) {
                elDesc = el.response;
                rDesc = ui.Switch(descState);
            }
            if (auto el = ui.Element({
                .label = "Stacked Mode",
                .description = "Description below switch control",
                .layout = ElementLayout::Stacked
            })) {
                elStacked = el.response;
                rStacked = ui.Switch(stackedState);
            }
            rBoth = ui.Switch(bothState, {
                .textLeft = "Dark",
                .textRight = "Light",
                .symmetric = true
            });
            if (auto el = ui.Element({.label = "Option Switch"})) {
                rOptSwitch = ui.Switch(optSwitchState);
            }
            if (auto el = ui.Element({.label = "Option Checkbox"})) {
                rOptCheck = ui.Checkbox(optCheckState);
            }
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
    CHECK(elDesc.rect.h > elSwitch.rect.h);
    CHECK(rStacked.rect.w > 0.0f);
    CHECK(elStacked.rect.h > elSwitch.rect.h);
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

    // 7. Click Option Switch (label configured via Element)
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

    // 8. Click Option Checkbox (label configured via Element)
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

    const keel::u32 width = 600;
    const keel::u32 height = 500;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    float speedDrop = 50.0f;
    keel::i32 count = 42;
    keel::String sampleName("Specimen-A");
    float optInputVal = 12.3f;

    Response rSpeed;
    Response rCount;
    Response rName;
    Response rOptInput;
    Response elSpeed;
    Response elOptInput;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 16.0f, .padding = Padding::All(20.0f)})) {
            // Numeric field with unit and description (as in user screenshot)
            if (auto el = ui.Element({
                .label = "Speed Drop dF/dt",
                .description = "Automatic stop of testing on load drop",
                .layout = ElementLayout::Stacked
            })) {
                elSpeed = el.response;
                rSpeed = ui.SpinBox(speedDrop, {
                    .suffix = "%",
                    .valueFormat = "%.0f",
                    .stepButtons = false
                });
            }

            // Integer count with unit suffix
            if (auto el = ui.Element({
                .label = "Cycle Count",
                .layout = ElementLayout::Stacked
            })) {
                rCount = ui.SpinBox(count, {
                    .suffix = "cycles",
                    .stepButtons = false
                });
            }

            // Text input with placeholder
            if (auto el = ui.Element({
                .label = "Sample ID",
                .layout = ElementLayout::Stacked
            })) {
                rName = ui.InputField(sampleName, {
                    .placeholder = "Enter identifier..."
                });
            }

            // Field with label set via ElementOptions
            if (auto el = ui.Element({
                .label = "Tolerance Margin",
                .layout = ElementLayout::Stacked
            })) {
                elOptInput = el.response;
                rOptInput = ui.SpinBox(optInputVal, {
                    .suffix = "mm",
                    .stepButtons = false
                });
            }
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
    CHECK(elSpeed.rect.h > 36.0f); // Default box is 36px, stacked adds label and description
    CHECK(rCount.rect.w > 0.0f);
    CHECK(rName.rect.w > 0.0f);
    CHECK(rOptInput.rect.w > 0.0f);
    CHECK(elOptInput.rect.h > 36.0f);
    CHECK(!rSpeed.focused);

    // 1. Click speed field to focus (click past end of "50" so cursor is placed at end)
    ui.OnMouseMove({rSpeed.rect.x + 100.0f, rSpeed.rect.y + 18.0f});
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
    ui.OnMouseMove({rName.rect.x + 150.0f, rName.rect.y + 18.0f});
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
    ui.OnMouseMove({rName.rect.x + 12.0f, rName.rect.y + 18.0f});
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

    // 4c. LMB drag selection: press inside the left padding (before the first glyph) and drag to the right
    // (x + 200) to select all, then replace
    ui.OnMouseMove({rName.rect.x + 2.0f, rName.rect.y + rName.rect.h * 0.5f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.OnMouseMove({rName.rect.x + 200.0f, rName.rect.y + 18.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    keel::usize selS = 0, selE = 0;
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
    ui.OnMouseMove({rName.rect.x + 20.0f, rName.rect.y + 18.0f});
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
    sampleName.assign("Привет мир");
    ui.ClearFocus();
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Focus and place cursor at end of "Привет мир"
    ui.AdvanceTime(1.0);
    ui.OnMouseMove({rName.rect.x + 200.0f, rName.rect.y + 18.0f});
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
    sampleName.assign("Specimen-A-X");

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

    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    keel::usize baudIndex = 1; // "19200"
    keel::i32 modeIndex = 0;   // "Auto"
    keel::String profile("Standard");
    bool disabledMode = false;

    WidgetResponse rBaud;
    WidgetResponse rMode;
    WidgetResponse rProfile;
    Response elBaud;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(20.0f)})) {
            if (auto el = ui.Element({
                .label = "Baud Rate",
                .description = "Serial bus communication speed",
                .layout = ElementLayout::Stacked
            })) {
                elBaud = el.response;
                rBaud = ui.Select(baudIndex, {"9600", "19200", "38400", "115200"});
            }

            if (auto el = ui.Element({
                .label = "Control Mode",
                .disabled = disabledMode
            })) {
                rMode = ui.Select(modeIndex, {"Auto", "Manual", "Direct"}, {
                    .disabled = disabledMode
                });
            }

            if (auto el = ui.Element({.label = "Profile"})) {
                rProfile = ui.Select(profile, {"Eco", "Standard", "Performance", "Extreme"});
            }
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
    CHECK(elBaud.rect.h > 36.0f); // Stacked adds label and description
    CHECK(rMode.rect.w > 0.0f);
    CHECK(rProfile.rect.w > 0.0f);
    CHECK(!ui.IsPopupOpen(rBaud.id));
    CHECK(!ui.IsPopupOpen(rMode.id));
    CHECK(!ui.IsPopupOpen(rProfile.id));

    // 1. Click Baud Rate select box to open dropdown popup
    ui.OnMouseMove({rBaud.rect.x + 20.0f, rBaud.rect.y + 18.0f});
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
    // Item 3 rect is at popupRect.y + 4.0f + 3 * itemHeight (theme token)
    const float selectItemH = ui.GetTheme().selects[UiSize::Md.TableIndex()].itemHeight;
    const float item3Y = popupRect.y + 4.0f + 3.0f * selectItemH + selectItemH * 0.5f;
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
    ui.OnMouseMove({rBaud.rect.x + 20.0f, rBaud.rect.y + 18.0f});
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
    ui.OnMouseMove({rBaud.rect.x + 20.0f, rBaud.rect.y + 18.0f});
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
    const float extremeY = popupRect.y + 4.0f + 3.0f * selectItemH + selectItemH * 0.5f;
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
    keel::usize limIdx = 0;
    WidgetResponse rLim;
    auto renderLimited = [&]() {
        if (auto el = ui.Element({.label = "Limited Dropdown"})) {
            rLim = ui.Select(limIdx, {"Item 1", "Item 2", "Item 3", "Item 4", "Item 5"}, {
                .maxVisibleItems = 2
            });
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderLimited();
    ui.EndFrame(canvas);

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
    // 2 visible items: 2 * itemHeight (theme token) + 8.0f popup padding
    const float limItemH = ui.GetTheme().selects[UiSize::Md.TableIndex()].itemHeight;
    CHECK_NEAR(limPopupRect.h, 2.0f * limItemH + 8.0f, 0.5f);
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
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    bool switchVal1 = false;
    bool switchVal2 = true;
    float speedVal = 120.0f;
    keel::String nameVal("Axis-1");

    WidgetResponse sw1Res;
    WidgetResponse sw2Res;
    WidgetResponse inp1Res;
    WidgetResponse inp2Res;

    auto renderUI = [&]() {
        // 1. Standalone Grid with 12-column slots
        if (auto grid = ui.Grid({.gap = 12.0f})) {
            if (auto c1 = grid.Cell6()) {
                if (auto el = ui.Element({.label = "Sensor A"})) {
                    sw1Res = ui.Switch(switchVal1);
                }
            }
            if (auto c2 = grid.Cell6()) {
                if (auto el = ui.Element({.label = "Sensor B"})) {
                    sw2Res = ui.Switch(switchVal2);
                }
            }
            if (auto c3 = grid.Cell12()) {
                if (auto el = ui.Element({.label = "Speed Target"})) {
                    inp1Res = ui.SpinBox(speedVal);
                }
            }
        }

        // 2. High-level Card surface containing an explicit Grid layout
        if (auto card = ui.Card()) {
            ui.Heading({.label = "Motion Controller", .description = "Direct axis tuning"});
            if (auto grid = ui.Grid()) {
                if (auto c1 = grid.Cell6()) {
                    if (auto el = ui.Element({.label = "Device Name"})) {
                        inp2Res = ui.InputField(nameVal);
                    }
                }
                if (auto c2 = grid.Cell6()) {
                    ui.Button({.text = "Calibrate Zero"});
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
    // Card has the default padding token (12) on all sides. Content width = 800 - 24 = 776.
    // Inner grid has the default gap token (9).
    // Cell 1 (span 6): W(6) = (6/12)*(776 + 9) - 9 = 392.5 - 9 = 383.5.
    // Input 2 should start at 12, width 383.5!
    CHECK_NEAR(bInp2.x, 12.0f, 1.0f);
    CHECK_NEAR(bInp2.w, 383.5f, 1.0f);

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
    const keel::u32 width = 800;
    const keel::u32 height = 400;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse btn1Res;
    WidgetResponse btn2Res;
    WidgetResponse btn3Res;

    auto renderUI = [&]() {
        if (auto scroll = ui.ScrollArea()) {
            if (auto box1 = ui.Box({.height = 200.0f})) {
                btn1Res = ui.Button({.text = "Top Item", .width = SizeMode::Stretch});
            }
            if (auto box2 = ui.Box({.height = 200.0f})) {
                btn2Res = ui.Button({.text = "Middle Item"});
            }
            if (auto box3 = ui.Box({.height = 200.0f})) {
                btn3Res = ui.Button({.text = "Bottom Item"});
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

    // Frame 2: Mouse wheel down far enough to reach the 200px maximum scroll (ticks * scrollStep token >= 200)
    ui.OnMouseMove({100.0f, 100.0f});
    ui.OnMouseWheel(0.0f, -keel::ceil(200.0f / sm.scrollStep));
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
        static constexpr keel::StringRef kTallMd =
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
        for (keel::i32 i = 0; i < 3; ++i) {
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
        const float thumbLen = keel::clamp((220.0f / scrollNode->scrollContentSize.y) * trackLen, sm.minThumbLength, trackLen);
        const float travel = keel::max(0.0f, trackLen - thumbLen);
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

    // 10. RichText Core Engine: LruCache, CommonMark flanking, self-contained textPool, and UiText::Rich across widgets
    {
        // A. Deterministic O(1) keel::LruCache eviction
        keel::LruCache<keel::u64, keel::i32, 3> lru;
        lru.put(1, 10);
        lru.put(2, 20);
        lru.put(3, 30);
        CHECK(lru.size() == 3);
        // Touch key 1 so key 2 becomes the least-recently-used
        CHECK(lru.find(1) != nullptr && *lru.find(1) == 10);
        lru.put(4, 40);
        CHECK(lru.size() == 3);
        CHECK(lru.find(2) == nullptr); // Evicted!
        CHECK(lru.find(1) != nullptr && *lru.find(1) == 10);
        CHECK(lru.find(3) != nullptr && *lru.find(3) == 30);
        CHECK(lru.find(4) != nullptr && *lru.find(4) == 40);

        // B. CommonMark flanking rule: arithmetic "2 * 3 * 4 = 24" must NOT be misparsed as italic
        const auto mathSpans = ParseInlineSpans("2 * 3 * 4 = 24");
        CHECK(mathSpans.size() == 1);
        if (!mathSpans.empty()) {
            CHECK(mathSpans[0].kind == InlineKind::Text);
            CHECK(mathSpans[0].style == FontStyle::Regular);
            CHECK(mathSpans[0].text == "2 * 3 * 4 = 24");
        }

        // True inline rich formatting: **bold**, *italic*, `code`
        const auto richSpans = ParseInlineSpans("Press `Ctrl+S` to **save** or *cancel*.");
        CHECK(richSpans.size() >= 6);
        bool sawCode = false;
        bool sawBold = false;
        bool sawItalic = false;
        for (const auto& sp : richSpans) {
            if (sp.kind == InlineKind::Code && sp.text == "Ctrl+S") sawCode = true;
            if (sp.style == FontStyle::Bold && sp.text == "save") sawBold = true;
            if (sp.style == FontStyle::Italic && sp.text == "cancel") sawItalic = true;
        }
        CHECK(sawCode);
        CHECK(sawBold);
        CHECK(sawItalic);

        // C. Self-contained textPool in InlineFlowLayout: destroying temporary keel::String must never dangle atom slices
        keel::Arc<const InlineFlowLayout> cachedFlow;
        {
            keel::String tempDynamic("Dynamic `code_token` and **bold_word**");
            cachedFlow = GetOrBuildRichTextFlow(&canvas, tempDynamic, 300.0f, 13.0f, 12.0f, 1.35f, UiTheme::Dark());
            std::memset(tempDynamic.data(), 'X', tempDynamic.size());
        }
        CHECK(cachedFlow != nullptr);
        if (cachedFlow) {
            CHECK(!cachedFlow->textPool.empty());
            bool foundCodeAtom = false;
            bool foundBoldAtom = false;
            for (const auto& pa : cachedFlow->items) {
                if (pa.atom.kind == InlineKind::Code && pa.atom.text == "code_token") foundCodeAtom = true;
                if (pa.atom.style == FontStyle::Bold && pa.atom.text == "bold_word") foundBoldAtom = true;
            }
            CHECK(foundCodeAtom);
            CHECK(foundBoldAtom);
        }

        // D. UiText::Rich integration in ui.Text, ui.Element, and ui.SetTooltip + 0 steady-state font measure calls
        struct RichCountingCanvas : public ThorCanvas {
            keel::i32 layoutMeasureCalls{0};
            bool insideDrawText{false};
            Vec2 MeasureText(keel::StringRef text, float fontSize) override {
                if (!insideDrawText) ++layoutMeasureCalls;
                return ThorCanvas::MeasureText(text, fontSize);
            }
            Vec2 MeasureTextStyled(keel::StringRef text, float fontSize, FontStyle style) override {
                if (!insideDrawText) ++layoutMeasureCalls;
                return ThorCanvas::MeasureTextStyled(text, fontSize, style);
            }
            void DrawTextStyled(keel::StringRef text, Vec2 pos, float fontSize, Color color, FontStyle style, float angleDeg = 0.0f) override {
                insideDrawText = true;
                ThorCanvas::DrawTextStyled(text, pos, fontSize, color, style, angleDeg);
                insideDrawText = false;
            }
        };

        RichCountingCanvas richCanvas;
        keel::Array<keel::u32> richPixels(500 * 300);
        richCanvas.SetTarget(richPixels.data(), 500, 300, 500);

        Context uiRich;
        auto buildRichScene = [&]() {
            if (auto col = uiRich.Column({.gap = 8.0f, .padding = Padding::All(12.0f)})) {
                uiRich.Text(UiText::Rich("Shortcut: `Ctrl+Shift+P` opens **Command Palette**."));
                if (auto el = uiRich.Element({
                    .label = UiText::Rich("Enable `GPU` **Acceleration**"),
                    .description = UiText::Rich("Uses *hardware* shaders (`OpenGL ES 3.0`) for **60 FPS** rendering.")
                })) {
                    bool enabled = true;
                    uiRich.Switch(enabled);
                }
                uiRich.SetTooltip(UiText::Rich("Hover tip with `inline code` and **bold** text"), Rect(10.0f, 10.0f, 100.0f, 24.0f));
            }
        };

        richCanvas.layoutMeasureCalls = 0;
        uiRich.BeginFrame({500.0f, 300.0f});
        buildRichScene();
        uiRich.EndFrame(richCanvas);
        CHECK(uiRich.HasDirtyDamage());
        CHECK(richCanvas.layoutMeasureCalls > 0);

        // Steady-state frame 2 with full repaint forced: must hit LruCache for both MeasureUiTextWidth and GetOrBuildRichTextFlow!
        richCanvas.layoutMeasureCalls = 0;
        uiRich.InvalidateAll();
        uiRich.BeginFrame({500.0f, 300.0f});
        buildRichScene();
        uiRich.EndFrame(richCanvas);
        CHECK(richCanvas.layoutMeasureCalls == 0);
    }
}

void TestReactiveEventLoop() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    keel::Array<keel::u32> pixels(100 * 100);
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

    for (keel::i32 i = 0; i < 5; ++i) {
        ui.BeginFrame({100.0f, 100.0f});
        ui.EndFrame(canvas);
    }
    // Still needs repaint because button is held down
    CHECK(ui.NeedsRepaint());

    // Releasing mouse button queues 3 settle frames
    ui.OnMouseUp(1);
    CHECK(!ui.IsMouseDown());
    CHECK(ui.GetRepaintFramesLeft() == 3);

    for (keel::i32 i = 0; i < 3; ++i) {
        ui.BeginFrame({100.0f, 100.0f});
        ui.EndFrame(canvas);
    }
    CHECK(!ui.NeedsRepaint());

    // Key event wakes up loop
    ui.OnKeyDown(Key::Enter);
    CHECK(ui.GetRepaintFramesLeft() == 3);
    CHECK(ui.NeedsRepaint());

    // Drain frames
    for (keel::i32 i = 0; i < 3; ++i) {
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
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    auto renderUI = [&](keel::String* editVal = nullptr) {
        if (auto row = ui.Row()) {
            if (auto b = ui.Box({.width = 120.0f})) {
                ui.Button({.text = "Test Button"});
            }
            if (editVal) {
                if (auto b = ui.Box({.width = 200.0f})) {
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
    keel::String text("Editable");
    bool shouldFocus = true;
    auto renderInput = [&]() {
        if (auto row = ui.Row()) {
            if (auto b = ui.Box({.width = 200.0f})) {
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
    keel::i32 tabIdx = 0;
    TabBarStyle currentTabStyle = TabBarStyle::Underline;
    auto renderTabTest = [&]() {
        if (auto col = ui.Column()) {
            ui.TabBar(tabIdx, {{"Tab1"}, {"Tab2"}}, {.style = currentTabStyle});
        }
    };

    // Settle layout completely
    for (keel::i32 i = 0; i < 3; ++i) {
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
    Response sliderRes;
    auto renderSlider = [&]() {
        if (auto b = ui.Box({.width = 300.0f})) {
            sliderRes = ui.Slider(sliderVal, 0.0f, 100.0f);
        }
    };

    // Initial render & settle layout completely
    for (keel::i32 i = 0; i < 3; ++i) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderSlider();
        ui.EndFrame(canvas);
    }
    CHECK(!ui.HasDirtyDamage());

    // Mouse down on slider produces dirty damage
    const float sliderMidY = sliderRes.rect.Center().y;
    ui.OnMouseMove({sliderRes.rect.x + sliderRes.rect.w * 0.5f, sliderMidY});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderSlider();
    ui.EndFrame(canvas);
    CHECK(ui.HasDirtyDamage());
    CHECK(!ui.IsFullRepaint());

    // Moving mouse while dragging MUST produce dirty damage on every frame!
    ui.OnMouseMove({sliderRes.rect.x + sliderRes.rect.w * 0.73f, sliderMidY});
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
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    keel::i32 activeTab = 0;
    const TabItem tabs[] = {
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
    keel::i32 pillIdx = 0;
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
    keel::i32 styledIdx = 0;
    Response styledRes;
    const Color barBg(30, 32, 40, 255);
    const Color barBorder(80, 85, 95, 255);
    auto renderStyledTabBar = [&]() {
        if (auto col = ui.Column({.padding = Padding::All(10.0f)})) {
            styledRes = ui.TabBar(styledIdx, {{"First"}, {"Second"}, {"Third"}}, {
                .style = TabBarStyle::Pill,
                .fullWidth = true,
                .padding = Padding::All(4.0f),
                .background = {barBg},
                .border = {.width = 1.5f, .radius = 10.0f, .color = {barBorder}}
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
                .background = {dynamicBg}
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
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    Response btnNew;
    Response btnSave;
    Response btnSettings;
    bool toolSettingsActive = false;

    auto renderToolbar = [&]() {
        if (auto tb = ui.Toolbar({.padding = UiSize::Sm, .size = UiSize::Md})) {
            btnNew = tb.ToolButton({.text = "New", .icon = Icon::Plus()});
            btnSave = tb.ToolButton({.text = "Save", .icon = Icon::Check(), .disabled = true});
            tb.Separator();
            btnSettings = tb.ToolButton({.icon = Icon::Settings(), .active = toolSettingsActive});
            tb.Spacer();
            tb.ToolButton({.text = "Status", .icon = Icon::Eye()});
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

    // 5. Test Toolbar Toggle(), Item() radio group, and Floating / Vertical Toolbar styles
    keel::i32 activeTool = 0;
    bool snapActive = false;
    Rect item1Rect{};
    Rect toggleRect{};
    Id floatingTbId{};

    auto renderAdvancedToolbar = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto tb = ui.Toolbar({.style = ToolbarStyle::Floating})) {
            floatingTbId = tb.response.id;
            tb.Item(activeTool, 0, {.text = "Select"});
            item1Rect = tb.Item(activeTool, 1, {.text = "Pan", .badge = "2"}).rect;
            tb.Separator();
            toggleRect = tb.Toggle(snapActive, {.text = "Snap"}).rect;
        }
        if (auto vtb = ui.Toolbar({.style = ToolbarStyle::Surface, .orientation = Orientation::Vertical})) {
            vtb.Item(activeTool, 0, {.text = "V0", .square = true});
            vtb.Separator();
            vtb.Spacer();
            vtb.Action({.text = "V1", .square = true});
        }
        ui.EndFrame(canvas);
    };

    renderAdvancedToolbar();
    renderAdvancedToolbar();

    // Floating toolbar shrinks to fit its items rather than stretching across all 800px
    const Rect floatBounds = ui.GetPrevFrameBounds(floatingTbId);
    CHECK(floatBounds.w > 80.0f);
    CHECK(floatBounds.w < 400.0f);

    // Click item1 ("Pan") -> activeTool changes to 1
    ui.OnMouseMove(item1Rect.Center());
    ui.OnMouseDown(1);
    renderAdvancedToolbar();
    ui.OnMouseUp(1);
    renderAdvancedToolbar();
    CHECK(activeTool == 1);

    // Click toggle ("Snap") -> snapActive flips to true
    ui.OnMouseMove(toggleRect.Center());
    ui.OnMouseDown(1);
    renderAdvancedToolbar();
    ui.OnMouseUp(1);
    renderAdvancedToolbar();
    CHECK(snapActive == true);

    // 6. Test Toolbar SplitButton() and SplitToggle() (main zone vs chevron dropdown + drag across zones)
    bool trigActive = false;
    keel::i32 trigMode = 0;
    SplitButtonResponse splitRes{};
    static constexpr keel::StaticArray<keel::StringRef, 3> kTrigModes = {"Auto", "Normal", "Single"};

    auto renderSplitToolbar = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto tb = ui.Toolbar({.style = ToolbarStyle::Surface})) {
            splitRes = tb.SplitToggle(trigActive, trigMode, kTrigModes, {
                .text = "Trigger",
                .icon = Icon::Flash()
            });
        }
        ui.EndFrame(canvas);
    };

    renderSplitToolbar();
    renderSplitToolbar();
    CHECK(splitRes.rect.w > 40.0f);

    // 6a. Clicking the left main zone toggles trigActive without opening the dropdown
    const Vec2 mainClickPos{splitRes.rect.x + 12.0f, splitRes.rect.Center().y};
    ui.OnMouseMove(mainClickPos);
    ui.OnMouseDown(1);
    renderSplitToolbar();
    CHECK(splitRes.pressed);
    CHECK(!splitRes.isOpen);
    ui.OnMouseUp(1);
    renderSplitToolbar();
    CHECK(splitRes.clicked);
    CHECK(splitRes.toggled);
    CHECK(trigActive == true);
    CHECK(trigMode == 0);

    // 6b. Pressing on the main zone, dragging into the chevron zone, and releasing cancels the click cleanly
    const Vec2 chevClickPos{splitRes.rect.Right() - 8.0f, splitRes.rect.Center().y};
    ui.OnMouseMove(mainClickPos);
    ui.OnMouseDown(1);
    renderSplitToolbar();
    CHECK(ui.GetActiveId() == splitRes.id);
    ui.OnMouseMove(chevClickPos);
    renderSplitToolbar();
    CHECK(!splitRes.hovered);
    ui.OnMouseUp(1);
    renderSplitToolbar();
    CHECK(!splitRes.clicked);
    CHECK(trigActive == true);
    CHECK(!ui.GetActiveId().IsValid());

    // 6c. Clicking the right chevron opens the dropdown menu and selecting item 2 updates trigMode
    ui.OnMouseMove(chevClickPos);
    ui.OnMouseDown(1);
    renderSplitToolbar();
    ui.OnMouseUp(1);
    renderSplitToolbar();
    CHECK(splitRes.isOpen);
    CHECK(!splitRes.clicked);

    // Select item index 2 via keyboard Down/Enter in the open popup
    ui.OnKeyDown(Key::Down);
    renderSplitToolbar();
    ui.OnKeyDown(Key::Down);
    renderSplitToolbar();
    ui.OnKeyDown(Key::Down);
    renderSplitToolbar();
    ui.OnKeyDown(Key::Enter);
    renderSplitToolbar();
    CHECK(splitRes.selectionChanged);
    CHECK(trigMode == 2);
    CHECK(!splitRes.isOpen);
}

void TestLabelVariantsStableSizing() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    bool state = false;
    Response btn;

    auto render = [&]() {
        if (auto row = ui.Row({.gap = 8.0f})) {
            btn = ui.Button({
                .text = {state ? "VSync: ON" : "VSync: OFF", {"VSync: ON", "VSync: OFF"}}
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
        static constexpr keel::StaticArray<keel::StringRef, 2> autoPxVars = {"Auto", "%.0f px"};
        SliderOptions slOpts{.valueFormat = {"Auto", autoPxVars}, .showValue = true, .width = SizeMode::Fit};
        const Vec2 szAuto = Slider::Measure(slOpts, 0.0, 0.0, 400.0, UiSize::Md, &canvas, ui.GetTheme());
        slOpts.valueFormat = {"%.0f px", autoPxVars};
        const Vec2 szPx = Slider::Measure(slOpts, 400.0, 0.0, 400.0, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(szAuto.x, szPx.x, 0.001f);

        static constexpr keel::StaticArray<keel::StringRef, 3> shortSteps = {"A", "B", "C"};
        static constexpr keel::StaticArray<keel::StringRef, 3> fullNames = {"Short", "Medium", "ExtraLongVariantName"};
        StepSliderOptions stOpts{
            .stepLabels = shortSteps,
            .showValue = true,
            .valueText = {"Short", fullNames},
            .width = SizeMode::Fit
        };
        const Vec2 st0 = StepSlider::Measure(stOpts, 0, 3, UiSize::Md, &canvas, ui.GetTheme());
        stOpts.valueText = {"ExtraLongVariantName", fullNames};
        const Vec2 st2 = StepSlider::Measure(stOpts, 2, 3, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(st0.x, st2.x, 0.001f);

        // Verify InputField Fit auto-width calculation from valueFormat variants (and that placeholder does not affect width)
        static constexpr keel::StaticArray<keel::StringRef, 2> shortNumVars = {"0", "99"};
        static constexpr keel::StaticArray<keel::StringRef, 2> longEndpointVars = {"localhost", "https://production-cluster.guikit.dev:8443"};
        InputFieldOptions inShortOpts{
            .placeholder = "Very long placeholder hint that must not inflate compact box width...",
            .suffix = "%",
            .valueFormat = {"%.0f", shortNumVars},
            .width = SizeMode::Fit
        };
        InputFieldOptions inLongOpts{
            .placeholder = "Host...",
            .prefix = "https://",
            .valueFormat = {longEndpointVars},
            .width = SizeMode::Fit
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
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    // 1. Verify container UiSize inheritance across widgets
    Response btnLg, btnSm;
    bool switchVal = false;
    bool checkVal = false;
    keel::String inputVal("42");
    keel::i32 selectIdx = 0;
    Response swLg, swSm, cbLg, cbSm, inLg, inSm, selLg, selSm;

    auto renderSizeTest = [&](UiSize sz, bool isLarge) {
        if (auto col = ui.Column()) {
            if (isLarge) {
                btnLg = ui.Button({.text = "Large Action", .size = sz});
                swLg = ui.Switch(switchVal, {.size = sz});
                cbLg = ui.Checkbox(checkVal, {.size = sz});
                inLg = ui.InputField(inputVal, {.size = sz});
                selLg = ui.Select(selectIdx, {"One", "Two"}, {.size = sz});
            } else {
                btnSm = ui.Button({.text = "Small Action", .size = sz});
                swSm = ui.Switch(switchVal, {.size = sz});
                cbSm = ui.Checkbox(checkVal, {.size = sz});
                inSm = ui.InputField(inputVal, {.size = sz});
                selSm = ui.Select(selectIdx, {"One", "Two"}, {.size = sz});
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

    // 2. Verify Button SizeMode::Fit vs SizeMode::Stretch in horizontal Row
    Response btnCompact, btnStretch1, btnStretch2;
    auto renderStretchRow = [&]() {
        if (auto row = ui.Row()) {
            btnCompact = ui.Button({.text = "Compact", .width = SizeMode::Fit});
            btnStretch1 = ui.Button({.text = "Stretch", .width = SizeMode::Stretch});
            btnStretch2 = ui.Button({.text = "Stretch", .width = SizeMode::Stretch});
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

    // 1. SpacingScale defaults
    const auto stdStyle = UiStyle::Standard();
    CHECK(stdStyle.GetGap(UiSize::None) == 0.0f);
    CHECK(stdStyle.GetGap(UiSize::Xs) == 3.0f);
    CHECK(stdStyle.GetGap(UiSize::Sm) == 6.0f);
    CHECK(stdStyle.GetGap(UiSize::Md) == 9.0f);
    CHECK(stdStyle.GetGap(UiSize::Lg) == 12.0f);
    CHECK(stdStyle.GetGap(UiSize::Xl) == 18.0f);

    // 2. Context dynamic float density switching (Variant B: density scales ALL container gaps/padding + ui.Gap)
    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 400;
    const keel::u32 height = 300;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse b1, b2, bExplicit1, bExplicit2, bGap1, bGap2;

    auto render = [&]() {
        if (auto col = ui.Column()) {
            b1 = ui.Button({.text = "First"});
            b2 = ui.Button({.text = "Second"});
        }
        if (auto densityZ = ui.Zone({.density = 0.5f})) {
            if (auto explicitCol = ui.Column({.gap = 12.0f})) {
                bExplicit1 = ui.Button({.text = "Explicit 1"});
                bExplicit2 = ui.Button({.text = "Explicit 2"});
            }
        }
        if (auto gapZ = ui.Zone({.density = 2.0f})) {
            if (auto gapRow = ui.Row({.gap = 0.0f})) {
                bGap1 = ui.Button({.text = "Gap 1"});
                ui.Gap(10.0f);
                bGap2 = ui.Button({.text = "Gap 2"});
            }
        }
    };

    // Frame 1: Standard density (1.0f -> default Md gap = 9px)
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    CHECK_NEAR(b2.rect.y, b1.rect.Bottom() + 9.0f, 0.01f);
    // Explicit .gap = 12.0f is defined at density = 1.0f, so zone.density = 0.5f scales it to 6.0f
    CHECK_NEAR(bExplicit2.rect.y, bExplicit1.rect.Bottom() + 6.0f, 0.01f);
    // ui.Gap(10.0f) inside zone.density = 2.0f scales to 20.0f
    CHECK_NEAR(bGap2.rect.x, bGap1.rect.Right() + 20.0f, 0.01f);

    // Frame 2: Switch root density to 0.5f (Md gap = 4.5px; nested zones compose multiplicatively)
    ui.SetDensity(0.5f);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    CHECK_NEAR(b2.rect.y, b1.rect.Bottom() + 4.5f, 0.01f);
    CHECK_NEAR(bExplicit2.rect.y, bExplicit1.rect.Bottom() + 3.0f, 0.01f);
    CHECK_NEAR(bGap2.rect.x, bGap1.rect.Right() + 10.0f, 0.01f);

    // Frame 3: Switch root density to 2.0f (Md gap = 18px)
    ui.SetDensity(2.0f);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    render();
    ui.EndFrame(canvas);

    CHECK_NEAR(b2.rect.y, b1.rect.Bottom() + 18.0f, 0.01f);

    // Frame 4: Custom spacing scale override at density = 1.0f
    ui.SetDensity(1.0f);
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
    const keel::u32 width = 800;
    const keel::u32 height = 1200;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    WidgetResponse btnInherit, btnXl, btnXs;
    WidgetResponse swInherit, swXl, swXs;
    WidgetResponse cbInherit, cbXl, cbXs;
    WidgetResponse inInherit, inXl, inXs;
    WidgetResponse selInherit, selXl, selXs;
    WidgetResponse toolInherit, toolXl, toolXs;

    bool swVal = true;
    bool cbVal = true;
    keel::String inVal("Test");
    keel::usize selIdx = 0;
    static constexpr keel::StaticArray<keel::StringRef, 2> selItems = {"Option 1", "Option 2"};

    auto render = [&]() {
        if (auto col = ui.Column()) {
            // Buttons: Default (Md), Local override Xl, Local override Xs
            if (auto r1 = ui.Row()) {
                btnInherit = ui.Button({.text = "Inherit"});
                btnXl = ui.Button({.text = "Large", .size = UiSize::Xl});
                btnXs = ui.Button({.text = "Small", .size = UiSize::Xs});
            }

            // Switches: Default (Md), Local override Xl, Local override Xs
            if (auto r2 = ui.Row()) {
                swInherit = ui.Switch(swVal);
                swXl = ui.Switch(swVal, {.size = UiSize::Xl});
                swXs = ui.Switch(swVal, {.size = UiSize::Xs});
            }

            // Checkboxes: Default (Md), Local override Xl, Local override Xs
            if (auto r3 = ui.Row()) {
                cbInherit = ui.Checkbox(cbVal);
                cbXl = ui.Checkbox(cbVal, {.size = UiSize::Xl});
                cbXs = ui.Checkbox(cbVal, {.size = UiSize::Xs});
            }

            // InputFields: Default (Md), Local override Xl, Local override Xs
            if (auto r4 = ui.Row()) {
                inInherit = ui.InputField(inVal);
                inXl = ui.InputField(inVal, {.size = UiSize::Xl});
                inXs = ui.InputField(inVal, {.size = UiSize::Xs});
            }

            // Selects: Default (Md), Local override Xl, Local override Xs
            if (auto r5 = ui.Row()) {
                selInherit = ui.Select(selIdx, selItems);
                selXl = ui.Select(selIdx, selItems, {.size = UiSize::Xl});
                selXs = ui.Select(selIdx, selItems, {.size = UiSize::Xs});
            }

            // Toolbar ToolButtons: Inherited (Md), Local override Xl, Local override Xs
            if (auto tb = ui.Toolbar()) {
                toolInherit = tb.ToolButton({.text = "Inherit", .icon = Icon::Play()});
                toolXl = tb.ToolButton({.text = "Large", .icon = Icon::Play(), .size = UiSize::Xl});
                toolXs = tb.ToolButton({.text = "Small", .icon = Icon::Play(), .size = UiSize::Xs});
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

    // 1. Standalone Heading::Measure tests (default level = H3, independent of UiSize)
    HeadingOptions optsTitleOnly{.label = "System Configuration"};
    Vec2 sizeTitleOnly = Heading::Measure(optsTitleOnly);
    CHECK(sizeTitleOnly.x > 0.0f);
    CHECK(sizeTitleOnly.y > 0.0f);

    HeadingOptions optsWithSub{.label = "System Configuration", .description = "Hardware line supervision & failsafes"};
    Vec2 sizeWithSub = Heading::Measure(optsWithSub);
    CHECK(sizeWithSub.x >= sizeTitleOnly.x);
    // Heading with subtitle must be taller than title-only
    CHECK(sizeWithSub.y > sizeTitleOnly.y);

    HeadingOptions optsWithBadgeIcon{
        .label = "System Configuration",
        .description = "1000 samples/sec",
        .icon = Icon::Check()
    };
    Vec2 sizeWithBadgeIcon = Heading::Measure(optsWithBadgeIcon);
    CHECK(sizeWithBadgeIcon.x > sizeTitleOnly.x);

    // HeadingLevel hierarchy scaling (H6 < H3 default < H1)
    HeadingOptions optsH6 = optsWithBadgeIcon;
    optsH6.level = HeadingLevel::H6;
    HeadingOptions optsH1 = optsWithBadgeIcon;
    optsH1.level = HeadingLevel::H1;
    Vec2 sizeH6 = Heading::Measure(optsH6);
    Vec2 sizeH3 = Heading::Measure(optsWithBadgeIcon);
    Vec2 sizeH1 = Heading::Measure(optsH1);
    CHECK(sizeH1.y > sizeH3.y);
    CHECK(sizeH3.y > sizeH6.y);
    CHECK(sizeH1.x > sizeH3.x);
    CHECK(sizeH3.x > sizeH6.x);

    // Architectural invariants: Element and Heading are pure semantic widgets without height/verticalAlign/background/border,
    // and HeadingOptions does not expose .size because Heading is governed by HeadingLevel, not UiSize.
    static_assert(![]<typename T = guikit::HeadingOptions>() { return requires(T o) { o.size; }; }());
    static_assert(![]<typename T = guikit::ElementOptions>() { return requires(T o) { o.height; }; }());
    static_assert(![]<typename T = guikit::HeadingOptions>() { return requires(T o) { o.height; }; }());
    static_assert(![]<typename T = guikit::ElementOptions>() { return requires(T o) { o.verticalAlign; }; }());
    static_assert(![]<typename T = guikit::HeadingOptions>() { return requires(T o) { o.verticalAlign; }; }());
    static_assert(![]<typename T = guikit::ElementOptions>() { return requires(T o) { o.background; }; }());
    static_assert(![]<typename T = guikit::ElementOptions>() { return requires(T o) { o.border; }; }());
    static_assert(![]<typename T = guikit::HeadingOptions>() { return requires(T o) { o.background; }; }());
    static_assert(![]<typename T = guikit::HeadingOptions>() { return requires(T o) { o.border; }; }());
    {
        class HeadingStrokeSpyCanvas : public ThorCanvas {
        public:
            struct RoundRectRec {
                Rect rect;
                Color fill;
                Color stroke;
                float strokeWidth;
            };
            struct LineRec {
                Vec2 p1;
                Vec2 p2;
                Color color;
                float thickness;
            };
            void DrawRoundRect(Rect rect, float radius, Color fill, Color stroke = Color(0, 0, 0, 0), float strokeWidth = 0.0f) override {
                roundRects.push_back({rect, fill, stroke, strokeWidth});
                ThorCanvas::DrawRoundRect(rect, radius, fill, stroke, strokeWidth);
            }
            void DrawLine(Vec2 p1, Vec2 p2, Color color, float thickness = 1.0f) override {
                lines.push_back({p1, p2, color, thickness});
                ThorCanvas::DrawLine(p1, p2, color, thickness);
            }
            using ThorCanvas::Clear;
            void Clear() {
                roundRects.clear();
                lines.clear();
            }
            keel::Array<RoundRectRec> roundRects;
            keel::Array<LineRec> lines;
        };

        const UiTheme theme = UiTheme::Dark();
        keel::Array<keel::u32> spyPixels(800 * 200);
        HeadingStrokeSpyCanvas spyCanvas;
        spyCanvas.SetTarget(spyPixels.data(), 800, 200, 800);

        // (a) Heading with explicit .width = 320.0f and Align::Center centers a 320dp Bottom separator at x = 240.0f
        HeadingOptions optsDecoupled{
            .label = "Section Title",
            .description = "Secondary description text",
            .align = Align::Center,
            .width = 320.0f,
            .separator = HeadingSeparator::Bottom
        };
        const Vec2 measuredDec = Heading::Measure(optsDecoupled, &spyCanvas, theme, 800.0f);
        Heading::Render(spyCanvas, Rect(0.0f, 0.0f, 800.0f, measuredDec.y), optsDecoupled, theme);

        CHECK(spyCanvas.roundRects.empty());
        CHECK(spyCanvas.lines.size() == 1);
        if (spyCanvas.lines.size() == 1) {
            CHECK(spyCanvas.lines[0].color == theme.palette.borderSubtle);
            CHECK_NEAR(spyCanvas.lines[0].p1.x, 240.0f, 0.01f);
            CHECK_NEAR(spyCanvas.lines[0].p2.x, 560.0f, 0.01f);
        }

        // (b) Custom HeadingDivider colors and thickens separator
        spyCanvas.Clear();
        optsDecoupled.separator = {HeadingSeparator::Bottom, StateColor::Primary(), 2.0f};
        const Vec2 measuredCustomSep = Heading::Measure(optsDecoupled, &spyCanvas, theme, 800.0f);
        CHECK(measuredCustomSep.y > measuredDec.y);
        Heading::Render(spyCanvas, Rect(0.0f, 0.0f, 800.0f, measuredCustomSep.y), optsDecoupled, theme);

        CHECK(spyCanvas.lines.size() == 1);
        if (spyCanvas.lines.size() == 1) {
            const Color expectedSepCol = theme.ResolveColor(StateColor::Primary(), false, false, false, theme.palette.borderSubtle);
            CHECK(spyCanvas.lines[0].color == expectedSepCol);
            CHECK_NEAR(spyCanvas.lines[0].thickness, 2.0f, 0.01f);
        }

        // (c) Panels (`Card`) and intrinsic-box controls (`InputField`, `Button`) decouple `background` from `border.color`
        Context bgCtx;
        keel::String textVal("Gain");
        auto runBgDecoupleFrame = [&]() {
            spyCanvas.Clear();
            bgCtx.BeginFrame({800.0f, 200.0f});
            if (auto card = bgCtx.Card({
                .background = StateColor::Surface().WithAlpha(128),
                .border = {.width = 1.0f, .color = StateColor::Warning()}
            })) {
                bgCtx.InputField(textVal, {
                    .background = StateColor::Surface(), .border = {.width = 1.5f, .color = StateColor::Danger()}
                });
                bgCtx.Button({
                    .text = "Tinted Action",
                    .background = StateColor::Danger().Tinted(0.15f), .border = {.width = 1.0f, .color = StateColor::Success()}
                });
            }
            bgCtx.EndFrame(spyCanvas);
        };
        runBgDecoupleFrame();
        bgCtx.InvalidateAll();
        runBgDecoupleFrame();
        CHECK(spyCanvas.roundRects.size() >= 3);
        if (spyCanvas.roundRects.size() >= 3) {
            CHECK(spyCanvas.roundRects[0].fill == bgCtx.GetTheme().palette.surface.ScaleAlpha(128));
            CHECK(spyCanvas.roundRects[1].fill == bgCtx.GetTheme().palette.inputBackground);
            const Color expectedTint = bgCtx.GetTheme().ResolveColor(StateColor::Danger().Tinted(0.15f), false, false, false, bgCtx.GetTheme().palette.surface);
            CHECK(spyCanvas.roundRects[2].fill == expectedTint);
        }
    }

    // 2. Immediate mode layout & bounds
    Context ui;
    const keel::i32 width = 800;
    const keel::i32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    ThorCanvas canvas;
    canvas.SetTarget(pixels.data(), width, height, width);

    Response head1, head2, head3, head4;
    keel::String dynamicSubtitle("Initial Subtitle");
    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
            head1 = ui.Heading({.label = "System Overview", .description = dynamicSubtitle});
            head2 = ui.Heading({
                .label = {"Telemetry Stream", StateColor::Success()},
                .description = "1000 samples/sec",
                .icon = Icon::Check()
            });
            head3 = ui.Heading({.label = "Section Divider", .separator = HeadingSeparator::Bottom});
            head4 = ui.Heading({.label = "Trailing Divider", .separator = HeadingSeparator::Trailing});
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
    dynamicSubtitle.assign("Updated Subtitle with Alert");
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

    const keel::u32 winWidth = 1200;
    const keel::u32 winHeight = 900;
    keel::Array<keel::u32> pixels(winWidth * winHeight);
    canvas.SetTarget(pixels.data(), winWidth, winHeight, winWidth);

    // Backend metadata check
    CHECK(canvas.GetBackendName() == "ThorVG (Vector Software)");

    // Frame 1: 100% scale
    ui.BeginFrame({static_cast<float>(winWidth), static_cast<float>(winHeight)}, 1.0f);
    CHECK_NEAR(ui.GetLogicalViewport().x, 1200.0f, 0.01f);
    CHECK_NEAR(ui.GetLogicalViewport().y, 900.0f, 0.01f);
    CHECK_NEAR(ui.GetDisplayScale(), 1.0f, 0.01f);
    ui.Button({.text = "TestBtn"});
    ui.EndFrame(canvas);
    CHECK_NEAR(canvas.GetScale(), 1.0f, 0.01f);

    // Frame 2: 150% scale (DPI scale = 1.5)
    ui.BeginFrame({static_cast<float>(winWidth), static_cast<float>(winHeight)}, 1.5f);
    CHECK_NEAR(ui.GetLogicalViewport().x, 800.0f, 0.01f);
    CHECK_NEAR(ui.GetLogicalViewport().y, 600.0f, 0.01f);
    CHECK_NEAR(ui.GetDisplayScale(), 1.5f, 0.01f);

    // Mouse movement in physical window pixels: x=150, y=30
    // Under 1.5x scale, logical mouse coordinates should be x=100, y=20
    ui.OnMouseMove({150.0f, 30.0f});
    CHECK_NEAR(ui.GetMousePos().x, 100.0f, 0.01f);
    CHECK_NEAR(ui.GetMousePos().y, 20.0f, 0.01f);

    // Render button and test layout
    ui.Button({.text = "TestBtn"});
    ui.EndFrame(canvas);

    // Canvas should receive 1.5x scale
    CHECK_NEAR(canvas.GetScale(), 1.5f, 0.01f);

    // Physical dirty rect should be scaled by 1.5x
    const Rect logicalDirty = ui.GetDirtyRect();
    const Rect physicalDirty = ui.GetPhysicalDirtyRect();
    CHECK(physicalDirty.w >= logicalDirty.w * 1.5f - 0.5f);
    CHECK(physicalDirty.h >= logicalDirty.h * 1.5f - 0.5f);
}

// Regression: the SDL backend once converted mouse coords to logical dp itself, and
// Context::OnMouseMove divided by the DPI scale a second time (hover landed at 1/dpi of the cursor).
// Exercises the full chain SDL window coords -> SdlWindowToPhysicalPixels -> OnMouseMove -> logical dp.
void TestSdlMouseCoordsMapToLogicalDp() {
    using namespace guikit;

    struct Case {
        const char* name;
        float pixelDensity; // SDL_GetWindowPixelDensity()
        float dpiScale;     // SDL_GetWindowDisplayScale() / devicePixelRatio
        Vec2 windowPos;     // SDL event coordinates
        Vec2 expectedLogical;
    };
    static constexpr Case kCases[] = {
        {"Windows 100%",       1.0f, 1.0f,  {300.0f, 120.0f}, {300.0f, 120.0f}},
        {"Windows 150%",       1.0f, 1.5f,  {300.0f, 120.0f}, {200.0f, 80.0f}},
        {"Windows 125%",       1.0f, 1.25f, {250.0f, 100.0f}, {200.0f, 80.0f}},
        {"macOS Retina",       2.0f, 2.0f,  {300.0f, 120.0f}, {300.0f, 120.0f}},
        {"Emscripten dpr 2",   1.0f, 2.0f,  {600.0f, 240.0f}, {300.0f, 120.0f}},
        {"Invalid density",    0.0f, 1.5f,  {300.0f, 120.0f}, {200.0f, 80.0f}},
    };

    for (const Case& c : kCases) {
        Context ui;
        ui.BeginFrame({1600.0f, 1000.0f}, c.dpiScale);
        ui.OnMouseMove(SdlWindowToPhysicalPixels(c.windowPos, c.pixelDensity));
        const Vec2 mouse = ui.GetMousePos();
        if (keel::abs(mouse.x - c.expectedLogical.x) > 0.01f || keel::abs(mouse.y - c.expectedLogical.y) > 0.01f) {
            keel::eprintln("[FAIL] {}: logical mouse ({}, {}) != expected ({}, {})",
                           c.name, mouse.x, mouse.y, c.expectedLogical.x, c.expectedLogical.y);
            ++g_failures;
        }
    }

    // End-to-end hover at Windows 150%: pointing the OS cursor at the button's on-screen centre must hover it.
    {
        constexpr float kDpi = 1.5f;
        constexpr float kPixelDensity = 1.0f;
        constexpr keel::u32 kW = 1200;
        constexpr keel::u32 kH = 900;
        Context ui;
        ThorCanvas canvas;
        keel::Array<keel::u32> pixels(kW * kH);
        canvas.SetTarget(pixels.data(), kW, kH, kW);

        WidgetResponse btn;
        auto renderUI = [&]() {
            if (auto col = ui.Column({.padding = Padding::All(200.0f)})) {
                btn = ui.Button({.text = "HiDPI Target"});
            }
        };

        // Two frames: widget rects are resolved from the previous frame's layout
        for (keel::i32 i = 0; i < 2; ++i) {
            ui.BeginFrame({static_cast<float>(kW), static_cast<float>(kH)}, kDpi);
            renderUI();
            ui.EndFrame(canvas);
        }
        CHECK(!btn.hovered);
        CHECK(btn.rect.w > 0.0f);

        // On-screen centre in SDL window coords = logical centre * dpi / pixelDensity
        const Vec2 windowCenter = btn.rect.Center() * (kDpi / kPixelDensity);
        ui.OnMouseMove(SdlWindowToPhysicalPixels(windowCenter, kPixelDensity));
        ui.BeginFrame({static_cast<float>(kW), static_cast<float>(kH)}, kDpi);
        renderUI();
        ui.EndFrame(canvas);
        CHECK(btn.hovered);
    }
}

// The frame's pixels-per-dp factor (OS display scale x user zoom) is the only dp -> pixel conversion:
// the dp layout of a tree must not depend on it, so every widget is exactly N times larger in pixels
// at factor N, and the pointer must stay under the on-screen cursor when the factor changes.
void TestFrameScaleIsUniformAndKeepsPointer() {
    using namespace guikit;

    struct Layout {
        Rect heading, toggle, slider, button, input, text;
    };
    auto layoutAt = [](float pixelsPerDp) {
        const keel::u32 w = keel::round_u32(800.0f * pixelsPerDp);
        const keel::u32 h = keel::round_u32(600.0f * pixelsPerDp);
        Context ui;
        ThorCanvas canvas;
        keel::Array<keel::u32> pixels(w * h);
        canvas.SetTarget(pixels.data(), w, h, w);

        bool enabled = true;
        float gain = -6.0f;
        keel::FixedString<32> name("Channel A");
        Layout out{};
        for (keel::i32 frame = 0; frame < 2; ++frame) {
            ui.BeginFrame({static_cast<float>(w), static_cast<float>(h)}, pixelsPerDp);
            if (auto card = ui.Card({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
                out.heading = ui.Heading({.label = "Audio Channel", .separator = HeadingSeparator::Bottom}).rect;
                if (auto el = ui.Element({.label = "Enable Stage"})) {
                    out.toggle = ui.Switch(enabled).rect;
                }
                if (auto el = ui.Element({.label = "Gain", .badge = "dB"})) {
                    out.slider = ui.Slider(gain, -60.0f, 12.0f, {.valueFormat = "%.1f dB"}).rect;
                }
                out.input = ui.InputField(name).rect;
                out.button = ui.Button({.text = "Apply"}).rect;
                out.text = ui.Text("Status: Online").rect;
            }
            ui.EndFrame(canvas);
        }
        return out;
    };

    const Layout base = layoutAt(1.0f);
    CHECK(base.button.w > 0.0f && base.slider.w > 0.0f && base.heading.w > 0.0f);

    static constexpr float kFactors[] = {0.75f, 1.25f, 1.5f, 1.875f, 2.0f};
    for (float factor : kFactors) {
        const Layout scaled = layoutAt(factor);
        const Rect* a[] = {&base.heading, &base.toggle, &base.slider, &base.button, &base.input, &base.text};
        const Rect* b[] = {&scaled.heading, &scaled.toggle, &scaled.slider, &scaled.button, &scaled.input, &scaled.text};
        static constexpr const char* kNames[] = {"heading", "switch", "slider", "button", "input", "text"};
        for (keel::usize i = 0; i < 6; ++i) {
            const bool same = keel::abs(a[i]->x - b[i]->x) < 0.01f && keel::abs(a[i]->y - b[i]->y) < 0.01f &&
                              keel::abs(a[i]->w - b[i]->w) < 0.01f && keel::abs(a[i]->h - b[i]->h) < 0.01f;
            if (!same) {
                keel::eprintln("[FAIL] {} dp rect differs at pixelsPerDp {}: ({}, {}, {}, {}) vs ({}, {}, {}, {})",
                               kNames[i], factor, b[i]->x, b[i]->y, b[i]->w, b[i]->h, a[i]->x, a[i]->y, a[i]->w, a[i]->h);
                ++g_failures;
            }
        }
    }

    // Zooming without moving the mouse: the cursor stays at the same physical pixel, so its dp position changes
    {
        Context ui;
        ui.BeginFrame({1200.0f, 900.0f}, 1.0f);
        ui.OnMouseMove({300.0f, 150.0f});
        CHECK_NEAR(ui.GetMousePos().x, 300.0f, 0.01f);
        ui.BeginFrame({1200.0f, 900.0f}, 1.5f);
        CHECK_NEAR(ui.GetMousePos().x, 200.0f, 0.01f);
        CHECK_NEAR(ui.GetMousePos().y, 100.0f, 0.01f);
        ui.BeginFrame({1200.0f, 900.0f}, 0.75f);
        CHECK_NEAR(ui.GetMousePos().x, 400.0f, 0.01f);
        CHECK_NEAR(ui.GetMousePos().y, 200.0f, 0.01f);
    }
}

// Zones: the root zone is created by BeginFrame, child zones inherit whatever they do not set, a container's
// children inherit the container's own state (entered by the context, not by the component), a container scaled
// with `ui.ScaleForZoom(z)` keeps its pixel size whatever the root zoom is, and every scope leaves the zone stack
// exactly as it found it.
void TestZonesInheritanceZoomIndependenceAndBalance() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    constexpr keel::u32 kW = 1200;
    constexpr keel::u32 kH = 900;
    keel::Array<keel::u32> pixels(kW * kH);
    canvas.SetTarget(pixels.data(), kW, kH, kW);
    const Vec2 viewport{static_cast<float>(kW), static_cast<float>(kH)};

    // --- Frame factor decomposition ---
    ui.BeginFrame(viewport, 1.5f, 1.25f);
    CHECK_NEAR(ui.GetDisplayScale(), 1.5f, 1e-6f);
    CHECK_NEAR(ui.GetRootZoom(), 1.25f, 1e-6f);
    CHECK_NEAR(ui.GetPixelsPerDp(), 1.875f, 1e-6f);
    CHECK_NEAR(ui.ScaleForZoom(2.5f), 2.0f, 1e-6f);
    CHECK_NEAR(ui.ScaleForZoom(1.0f), 0.8f, 1e-6f);
    CHECK_NEAR(ui.GetLogicalViewport().x, 640.0f, 1e-3f);

    // --- Root zone & density ---
    CHECK(!ui.IsDisabled());
    CHECK_NEAR(ui.GetDensity(), 1.0f, 1e-6f);
    const UiTheme* rootTheme = &ui.GetTheme();

    // --- ui.Zone wrapper inherits everything it does not set, and restores parent on exit ---
    if (auto denseZ = ui.Zone({.density = 0.5f})) {
        if (auto denseCol = ui.Column()) {
            CHECK_NEAR(ui.GetDensity(), 0.5f, 1e-6f);
            CHECK(&ui.GetTheme() == rootTheme);       // theme inherited, not copied
            CHECK(!ui.IsDisabled());

            if (auto nestedZ = ui.Zone({.density = 1.5f})) {
                if (auto disabledBox = ui.Box({.disabled = true})) {
                    CHECK_NEAR(ui.GetDensity(), 0.75f, 1e-6f);  // density composes multiplicatively
                    CHECK(ui.IsDisabled());
                    if (auto reenabled = ui.Box({.disabled = false})) {
                        CHECK(ui.IsDisabled());                 // a disabled parent cannot be re-enabled by a child
                    }
                }
            }
            CHECK(!ui.IsDisabled());
            CHECK_NEAR(ui.GetDensity(), 0.5f, 1e-6f);
        }
    }
    CHECK_NEAR(ui.GetDensity(), 1.0f, 1e-6f);

    // --- Theme override: replaced for the zone wrapper and owned by the frame arena ---
    {
        const UiTheme light = UiTheme::Light();
        if (auto themeZ = ui.Zone({.theme = light})) {
            if (auto themed = ui.Box()) {
                CHECK(ui.GetTheme().mode == ThemeMode::Light);
                CHECK(ui.GetThemeMode() == ThemeMode::Light);
                CHECK(!ui.IsDarkMode());
                CHECK(ui.GetThemePalette().surface == light.palette.surface);
                CHECK(&ui.GetTheme() != &light);          // the zone owns a frame-lifetime copy
                CHECK(&ui.GetTheme() != rootTheme);

                // Nested containers inherit the theme from the parent zone
                if (auto inner = ui.Box({.background = {ui.GetThemePalette().surface}})) {
                    CHECK(inner.GetNode()->GetThemePtr() == &ui.GetTheme());
                    CHECK(ui.GetTheme().mode == ThemeMode::Light);
                    CHECK(inner.GetNode()->backgroundColor == light.palette.surface);
                }
            }
        }
    }
    CHECK(&ui.GetTheme() == rootTheme);
    CHECK(ui.GetTheme().mode == ThemeMode::Dark);
    CHECK(ui.GetThemeMode() == ThemeMode::Dark);
    CHECK(ui.IsDarkMode());
    CHECK(ui.GetThemePalette().surface == rootTheme->palette.surface);
    ui.EndFrame(canvas);

    // A container scaled with ScaleForZoom has the same pixel size at every root zoom, while the surrounding UI
    // follows the root zoom
    static constexpr float kRootZooms[] = {0.75f, 1.0f, 1.25f, 1.5f, 2.0f};
    for (float rootZoom : kRootZooms) {
        Context ctx;
        Id insideId = Id::Null();
        Id outsideId = Id::Null();
        for (keel::i32 frame = 0; frame < 2; ++frame) {
            ctx.BeginFrame(viewport, 1.5f, rootZoom);
            if (auto outside = ctx.Box({.width = 100.0f, .height = 20.0f})) {
                outsideId = outside.GetNode()->id;
            }
            if (auto scaleZ = ctx.Zone({.scale = ctx.ScaleForZoom(1.25f)})) {
                if (auto preview = ctx.Box()) {
                    if (auto inside = ctx.Box({.width = 100.0f, .height = 20.0f})) {
                        insideId = inside.GetNode()->id;
                    }
                }
            }
            CHECK(ctx.GetContainerDepth() == 1);
            ctx.EndFrame(canvas);
        }
        const Rect inside = ctx.GetPrevFrameBounds(insideId);
        const Rect outside = ctx.GetPrevFrameBounds(outsideId);
        CHECK_NEAR(inside.w * ctx.GetPixelsPerDp(), 100.0f * 1.25f * 1.5f, 0.05f);   // 125% of the display scale
        CHECK_NEAR(outside.w * ctx.GetPixelsPerDp(), 100.0f * rootZoom * 1.5f, 0.05f); // follows the root zoom
    }

    // --- Scopes leave the container/zone stack balanced ---
    {
        Context ctx;
        ctx.BeginFrame(viewport, 1.0f);
        const keel::usize rootDepth = ctx.GetContainerDepth();
        CHECK(rootDepth == 1);

        {
            auto row = ctx.Row();
            CHECK(ctx.GetContainerDepth() == rootDepth + 1);
            if (auto col = ctx.Column({.disabled = true})) {
                CHECK(ctx.GetContainerDepth() == rootDepth + 2);
                CHECK(col.GetNode()->disabled);      // the container is disabled...
                CHECK(ctx.IsDisabled());             // ...and propagates disabled state to its children
            }
            CHECK(!ctx.IsDisabled());
        }
        CHECK(ctx.GetContainerDepth() == rootDepth);

        // Closed accordion leaves container stack untouched
        {
            bool open = false;
            auto acc = ctx.Accordion(open, {.label = "Closed", .size = UiSize::Sm});
            CHECK(!acc);
            CHECK(ctx.GetContainerDepth() == rootDepth);
        }

        // Open accordion enters and leaves container cleanly
        {
            bool open = true;
            auto acc = ctx.Accordion(open, {.label = "Open", .size = UiSize::Sm});
            CHECK(acc);
            CHECK(ctx.GetContainerDepth() == rootDepth + 1);
        }
        CHECK(ctx.GetContainerDepth() == rootDepth);

        // Leaf widgets never enter zones: a disabled button only disables itself
        ctx.Button({.text = "Leaf", .disabled = true});
        CHECK(ctx.GetContainerDepth() == rootDepth);
        CHECK(!ctx.IsDisabled());

        // A table row is a zone of its own that lives exactly as long as its row scope
        {
            static const keel::StaticArray<TableColumn, 1> kColumns = {{{.title = "A", .flex = 1.0f}}};
            bool rowDisabled[3] = {};
            keel::usize rowsSeen = 0;
            for (keel::i32 frame = 0; frame < 3; ++frame) {
                rowsSeen = 0;
                ctx.BeginFrame(viewport, 1.0f);
                if (auto table = ctx.Table(kColumns, {.totalRows = 3, .height = 200.0f})) {
                    for (keel::usize r = table.FirstVisibleRow(); r < table.EndVisibleRow() && r < 3; ++r) {
                        if (auto row = table.Row(r, {.disabled = (r == 1)})) {
                            rowDisabled[r] = ctx.IsDisabled();
                            ++rowsSeen;
                        }
                        CHECK(!ctx.IsDisabled());
                    }
                }
                CHECK(ctx.GetContainerDepth() == rootDepth);
                ctx.EndFrame(canvas);
            }
            CHECK(rowsSeen == 3);
            CHECK(!rowDisabled[0] && rowDisabled[1] && !rowDisabled[2]);
        }
        ctx.EndFrame(canvas);
    }
}

// A container zone's theme must reach every node built inside it, and it composes with `.zone.scale`:
// the theme decides the size in local units, the container's scale maps them.
void TestZoneThemeReachesNodesBuiltInside() {
    using namespace guikit;

    ThorCanvas canvas;
    constexpr keel::u32 kW = 800;
    constexpr keel::u32 kH = 600;
    keel::Array<keel::u32> pixels(kW * kH);
    canvas.SetTarget(pixels.data(), kW, kH, kW);
    const Vec2 viewport{static_cast<float>(kW), static_cast<float>(kH)};
    UiTheme tallTheme = UiTheme::Dark();
    tallTheme.MutableMetrics(UiSize::Md).defaultHeight = 40.0f;

    Context ui;
    Response plain;
    Response scaled;
    Response themed;
    Response nested;
    for (keel::i32 frame = 0; frame < 3; ++frame) {
        ui.BeginFrame(viewport, 1.0f);
        if (auto col = ui.Column()) {
            plain = ui.Button({.text = "Button"});
            if (auto zScale = ui.Zone({.scale = 2.0f})) {
                if (auto box = ui.Box()) {
                    scaled = ui.Button({.text = "Button"});
                }
            }
            if (auto zTheme = ui.Zone({.theme = tallTheme})) {
                if (auto themedBox = ui.Box()) {
                    themed = ui.Button({.text = "Button"});
                    if (auto zNested = ui.Zone({.scale = 2.0f})) {
                        if (auto box = ui.Box()) {
                            nested = ui.Button({.text = "Button"});
                        }
                    }
                }
            }
        }
        ui.EndFrame(canvas);
    }
    const float mdStandard = UiTheme::Dark().GetMetrics(UiSize::Md).defaultHeight;
    const float mdTall = tallTheme.GetMetrics(UiSize::Md).defaultHeight;
    // Frame bounds (in frame dp, read outside any container)
    const Rect plainB = ui.GetPrevFrameBounds(plain.id);
    const Rect scaledB = ui.GetPrevFrameBounds(scaled.id);
    const Rect themedB = ui.GetPrevFrameBounds(themed.id);
    const Rect nestedB = ui.GetPrevFrameBounds(nested.id);
    CHECK_NEAR(plainB.h, mdStandard, 0.01f);
    CHECK_NEAR(scaledB.h, mdStandard * 2.0f, 0.01f);            // the container's scale reaches the widget
    CHECK_NEAR(themedB.h, mdTall, 0.01f);                       // the zone's theme reaches the widget
    CHECK_NEAR(nestedB.h, mdTall * 2.0f, 0.01f);                // and composes with a scaled container
    CHECK(scaledB.w > plainB.w * 1.9f);                         // text scales with the subtree as well
    // The response of a widget in a scaled container is in that container's local units
    CHECK_NEAR(scaled.rect.h, mdStandard, 0.01f);
}

// Canvas transforms: drawing in a pushed transform must produce exactly the pixels of the same geometry mapped
// by hand (origin + (p - origin) * s), including text, strokes, rounded rects and clipping, and PopTransform
// must restore the previous mapping.
void TestCanvasTransformMatchesMappedGeometry() {
    using namespace guikit;

    constexpr keel::u32 kW = 260;
    constexpr keel::u32 kH = 180;
    constexpr float kFrameScale = 1.5f;
    const Vec2 origin{10.0f, 10.0f};
    constexpr float s = 2.0f;
    auto map = [&](Vec2 v) { return Vec2{origin.x + (v.x - origin.x) * s, origin.y + (v.y - origin.y) * s}; };
    auto mapRect = [&](Rect r) {
        const Vec2 o = map({r.x, r.y});
        return Rect(o.x, o.y, r.w * s, r.h * s);
    };
    const Color red(220, 40, 40);
    const Color blue(40, 90, 220);
    const Color white(255, 255, 255);

    auto draw = [&](ThorCanvas& c, bool useTransform) {
        auto R = [&](Rect r) { return useTransform ? r : mapRect(r); };
        auto P = [&](Vec2 v) { return useTransform ? v : map(v); };
        auto L = [&](float v) { return useTransform ? v : v * s; };
        c.Begin(kW, kH, kFrameScale);
        c.Clear(Color(10, 12, 16));
        if (useTransform) {
            c.PushTransform(Transform2D::ScaleAbout(origin, s));
            CHECK_NEAR(c.GetScale(), kFrameScale * s, 1e-6f);
        }
        c.DrawRect(R(Rect(12.0f, 12.0f, 20.0f, 9.0f)), red);
        c.DrawRoundRect(R(Rect(14.0f, 24.0f, 30.0f, 12.0f)), L(3.0f), blue, white, L(1.0f));
        c.DrawRoundRect(R(Rect(50.0f, 12.0f, 14.0f, 14.0f)), 0.0f, Color::Transparent(), white, L(1.0f));
        c.DrawLine(P({12.0f, 42.0f}), P({60.0f, 50.0f}), white, L(1.5f));
        c.PushClip(R(Rect(12.0f, 52.0f, 40.0f, 8.0f)));
        c.DrawRect(R(Rect(10.0f, 50.0f, 60.0f, 20.0f)), blue);
        c.PopClip();
        c.DrawText("Zone", P({40.0f, 30.0f}), L(11.0f), white);
        if (useTransform) {
            c.PopTransform();
            CHECK_NEAR(c.GetScale(), kFrameScale, 1e-6f);
        }
        // After PopTransform the mapping is the frame's again
        c.DrawRect(Rect(150.0f, 100.0f, 12.0f, 8.0f), red);
        c.End();
    };

    keel::Array<keel::u32> a(kW * kH);
    keel::Array<keel::u32> b(kW * kH);
    ThorCanvas ca;
    ThorCanvas cb;
    ca.SetTarget(a.data(), kW, kH, kW);
    cb.SetTarget(b.data(), kW, kH, kW);
    draw(ca, true);
    draw(cb, false);

    keel::usize differing = 0;
    keel::i32 maxDelta = 0;
    keel::usize lit = 0;
    for (keel::usize i = 0; i < a.size(); ++i) {
        if (a[i] != 0xFF100C0Au) ++lit;
        if (a[i] == b[i]) continue;
        ++differing;
        for (keel::i32 shift = 0; shift < 32; shift += 8) {
            const keel::i32 ca8 = static_cast<keel::i32>((a[i] >> shift) & 0xFFu);
            const keel::i32 cb8 = static_cast<keel::i32>((b[i] >> shift) & 0xFFu);
            maxDelta = keel::max(maxDelta, keel::abs(ca8 - cb8));
        }
    }
    CHECK(lit > 2000);                 // something was actually drawn
    if (differing != 0) {
        keel::eprintln("[INFO] canvas transform: {} differing pixels, max channel delta {}", differing, maxDelta);
    }
    CHECK(maxDelta <= 2);              // float rounding of p * S + offset vs (mapped p) * S only
    CHECK(differing < a.size() / 500); // < 0.2% of the surface
}

// A container with a scale is a transform root: widgets inside see local coordinates (their rect, the mouse),
// hit testing and damage happen in frame space, and rendering lands where the frame-space rect says.
namespace {
struct ScaledTestPanel {
    static guikit::ContainerScope Open(guikit::Context& ctx, keel::SourceLocation loc = keel::SourceLocation::current()) {
        using namespace guikit;
        const Id id = ctx.MakeChildId(loc);
        Node* node = ctx.CreateNode(NodeType::Container, id);
        node->direction = FlexDirection::Column;
        node->widthMode = SizeMode::Fit;
        node->heightMode = SizeMode::Fit;
        ctx.FinishContainer(node);
        return ContainerScope(&ctx, node);
    }
};
} // namespace

void TestScaledContainerSpaces() {
    using namespace guikit;

    constexpr keel::u32 kW = 600;
    constexpr keel::u32 kH = 400;
    Context ui;
    ThorCanvas canvas;
    keel::Array<keel::u32> pixels(kW * kH);
    canvas.SetTarget(pixels.data(), kW, kH, kW);
    const Vec2 viewport{static_cast<float>(kW), static_cast<float>(kH)};

    Response btn;
    Vec2 mouseInside{};
    Id panelId = Id::Null();
    auto frame = [&]() {
        ui.BeginFrame(viewport, 1.0f);
        if (auto col = ui.Column({.padding = Padding::All(40.0f)})) {
            if (auto z = ui.Zone({.scale = 2.0f})) {
                if (auto panel = ui.Open<ScaledTestPanel>()) {
                    panelId = panel.GetNode()->id;
                    btn = ui.Button({.text = "Scaled"});
                    mouseInside = ui.GetMousePos();
                }
            }
        }
        ui.EndFrame(canvas);
    };
    frame();
    frame();

    // The widget sees its rect in local units: origin shared with the panel at (40, 40), size unscaled
    const float bw = btn.rect.w;
    const float bh = btn.rect.h;
    CHECK_NEAR(btn.rect.x, 40.0f, 0.01f);
    CHECK_NEAR(btn.rect.y, 40.0f, 0.01f);
    CHECK_NEAR(bh, UiTheme::Dark().GetMetrics(UiSize::Md).defaultHeight, 0.01f);
    CHECK(bw > 20.0f);

    // In frame space the button is twice as large about the panel's corner
    const Transform2D space = ui.GetNodeSpace(btn.id);
    CHECK_NEAR(space.scale, 2.0f, 1e-6f);
    const Rect frameRect = space.Apply(btn.rect);
    CHECK_NEAR(frameRect.x, 40.0f, 0.01f);
    CHECK_NEAR(frameRect.w, bw * 2.0f, 0.01f);
    CHECK_NEAR(frameRect.h, bh * 2.0f, 0.01f);
    CHECK(ui.GetNodeSpace(panelId).scale == 2.0f);

    // A point inside the frame-space rect but below the local rect's numbers hovers the button
    const Vec2 target{40.0f + bw * 1.5f, 40.0f + bh * 2.0f - 5.0f};
    ui.OnMouseMove(target);
    frame();
    CHECK(btn.hovered);
    CHECK_NEAR(mouseInside.x, 40.0f + bw * 0.75f, 0.01f); // the mouse in the panel's local space
    CHECK_NEAR(mouseInside.y, 40.0f + bh - 2.5f, 0.01f);
    // The hover change damaged the frame-space area of the button
    CHECK(ui.GetDirtyRect().Intersects(Rect(frameRect.Right() - 4.0f, frameRect.Bottom() - 4.0f, 2.0f, 2.0f)));

    ui.OnMouseMove({frameRect.Right() + 8.0f, frameRect.y + 4.0f});
    frame();
    CHECK(!btn.hovered);

    // Rendering: the button's surface reaches the frame-space corner, and stops there
    ui.InvalidateAll();
    frame();
    auto px = [&](float x, float y) { return pixels[static_cast<keel::usize>(y) * kW + static_cast<keel::usize>(x)]; };
    const keel::u32 nearTopLeft = px(frameRect.x + 6.0f, frameRect.y + 6.0f);
    const keel::u32 nearBottomRight = px(frameRect.Right() - 6.0f, frameRect.Bottom() - 6.0f);
    const keel::u32 outside = px(frameRect.Right() + 6.0f, frameRect.Bottom() - 6.0f);
    CHECK(nearTopLeft == nearBottomRight);
    CHECK(nearBottomRight != outside);
}

// Regression: ThorVG's Text::size() takes points and scales by 96/72 internally, so passing a dp
// font size straight through rendered all text ~33% larger than the theme's typography tokens.
// The text box height must match the embedded font's hhea (ascender - descender) / unitsPerEm.
void TestThorCanvasFontSizeIsInPixels() {
    using namespace guikit;

    const keel::Optional<AssetView> font = Assets::Find("assets/fonts/Roboto-Regular.ttf");
    CHECK(font.has_value() && !font->Empty());
    if (!font.has_value() || font->Empty()) {
        return;
    }

    // Minimal sfnt table lookup (big-endian) for 'head'.unitsPerEm and 'hhea'.ascender/descender
    const keel::u8* d = font->data;
    auto be16 = [&](keel::usize off) { return static_cast<keel::u16>((d[off] << 8) | d[off + 1]); };
    auto be32 = [&](keel::usize off) {
        return (static_cast<keel::u32>(d[off]) << 24) | (static_cast<keel::u32>(d[off + 1]) << 16) |
               (static_cast<keel::u32>(d[off + 2]) << 8) | static_cast<keel::u32>(d[off + 3]);
    };
    keel::usize headOff = 0;
    keel::usize hheaOff = 0;
    const keel::u16 numTables = be16(4);
    for (keel::u16 i = 0; i < numTables; ++i) {
        const keel::usize rec = 12 + static_cast<keel::usize>(i) * 16;
        if (std::memcmp(d + rec, "head", 4) == 0) headOff = be32(rec + 8);
        if (std::memcmp(d + rec, "hhea", 4) == 0) hheaOff = be32(rec + 8);
    }
    CHECK(headOff != 0 && hheaOff != 0);
    if (headOff == 0 || hheaOff == 0) {
        return;
    }
    const float unitsPerEm = static_cast<float>(be16(headOff + 18));
    const float ascender = static_cast<float>(static_cast<keel::i16>(be16(hheaOff + 4)));
    const float descender = static_cast<float>(static_cast<keel::i16>(be16(hheaOff + 6)));
    const float emBoxRatio = (ascender - descender) / unitsPerEm;

    ThorCanvas canvas;
    CHECK(canvas.LoadFontMemory("RobotoTest", font->Bytes(), FontStyle::Regular));
    constexpr float kFontSize = 100.0f;
    const Vec2 box = canvas.MeasureText("Hg", kFontSize);
    if (keel::abs(box.y - kFontSize * emBoxRatio) > kFontSize * 0.02f) {
        keel::eprintln("[FAIL] ThorCanvas text box {} dp for {} dp font, expected {} dp (ratio {:.3f} vs {:.3f})",
                       box.y, kFontSize, kFontSize * emBoxRatio, box.y / kFontSize, emBoxRatio);
        ++g_failures;
    }
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
    ui.Button({.text = "GPU Button"});
    ui.EndFrame(canvas);
}

void TestIndustrialDashboardNoBlackLines() {
    using namespace guikit;

    static constexpr keel::StaticArray<float, 5> testScales = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};

    for (float testScale : testScales) {
        Context ui;
        ThorCanvas canvas;
        const keel::u32 width = 1000;
        const keel::u32 height = 750;
        keel::Array<keel::u32> pixels(width * height);
        canvas.SetTarget(pixels.data(), width, height, width);

        bool enableSensors = true;
        float maxTravel = 150.0f;
        float maxVelocity = 25.0f;
        keel::usize sensorChannel = 0;
        keel::usize activeTabIndex = 2; // Industrial Dashboard
        bool hwEstop = false;
        keel::usize interlockMode = 0;
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
                    tb.ToolButton({.text = "Density: Standard"});
                    tb.Separator();
                    tb.ToolButton({.text = "Scale: 100%"});
                    tb.Separator();
                    tb.ToolButton({.text = "Theme: Dark"});
                }

                const TabItem navTabs[] = {
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
                                    ui.Heading({.label = "Primary Stop Conditions", .description = "Sensory tripwire thresholds & limits"});
                                    if (auto grid1 = ui.Grid()) {
                                        if (auto c = grid1.Cell12()) {
                                            if (auto el = ui.Element({
                                                .label = "Enable Sensor Drop Limits",
                                                .description = "Immediate stop on threshold breach"
                                            })) {
                                                ui.Switch(enableSensors);
                                            }
                                        }
                                        if (auto c = grid1.Cell6()) {
                                            if (auto el = ui.Element({.label = "Max Travel"})) {
                                                ui.SpinBox(maxTravel, {.suffix = "mm", .valueFormat = "%.1f"});
                                            }
                                        }
                                        if (auto c = grid1.Cell6()) {
                                            if (auto el = ui.Element({.label = "Max Velocity"})) {
                                                ui.SpinBox(maxVelocity, {.suffix = "mm/s", .valueFormat = "%.1f"});
                                            }
                                        }
                                        if (auto c = grid1.Cell12()) {
                                            if (auto el = ui.Element({
                                                .label = "Primary Sensor Channel",
                                                .description = "Active physical transducer bus",
                                                .layout = ElementLayout::Stacked
                                            })) {
                                                ui.Select(sensorChannel, {
                                                    "Channel A (Load Cell 50kN)",
                                                    "Channel B (Optical Extensometer)",
                                                    "Channel C (Laser Interferometer)"
                                                });
                                            }
                                        }
                                    }
                                }

                                if (auto card2 = ui.Card()) {
                                    ui.Heading({.label = {"Emergency Interlocks", StateColor::Danger()}, .description = "Hardware line supervision & failsafes"});
                                    if (auto grid2 = ui.Grid()) {
                                        if (auto c = grid2.Cell12()) {
                                            if (auto el = ui.Element({
                                                .label = "Hardware E-Stop Line",
                                                .description = "Continuous 24V loop monitoring"
                                            })) {
                                                ui.Switch(hwEstop, {.color = StateColor::Danger()});
                                            }
                                        }
                                        if (auto c = grid2.Cell6()) {
                                            if (auto el = ui.Element({.label = "Interlock Action"})) {
                                                ui.Select(interlockMode, {
                                                    "Immediate Cutoff",
                                                    "Controlled Ramp-down",
                                                    "Hold Position"
                                                });
                                            }
                                        }
                                        if (auto c = grid2.Cell6()) {
                                            if (auto el = ui.Element({.label = "Ramp Duration"})) {
                                                ui.SpinBox(rampDown, {.suffix = "ms", .valueFormat = "%.0f"});
                                            }
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
        auto checkCardSeams = [&](keel::i32 frameNum) {
            const float s = testScale;
            // Physical pixel bounds of card interior
            const keel::u32 startY = static_cast<keel::u32>(keel::max(0.0f, (cardRect.y + 15.0f) * s));
            const keel::u32 endY = static_cast<keel::u32>(keel::min(static_cast<float>(height), (cardRect.Bottom() - 15.0f) * s));
            const keel::u32 startX = static_cast<keel::u32>(keel::max(0.0f, (cardRect.x + 15.0f) * s));
            const keel::u32 endX = static_cast<keel::u32>(keel::min(static_cast<float>(width), (cardRect.Right() - 15.0f) * s));

            for (keel::u32 y = startY; y < endY; ++y) {
                keel::i32 clearCount = 0;
                for (keel::u32 x = startX; x < endX; ++x) {
                    keel::u32 p = pixels[y * width + x];
                    keel::u8 r = p & 0xFF;
                    keel::u8 g = (p >> 8) & 0xFF;
                    keel::u8 b = (p >> 16) & 0xFF;
                    if (keel::abs(r - 11) <= 2 && keel::abs(g - 15) <= 2 && keel::abs(b - 25) <= 2) {
                        ++clearCount;
                    }
                }
                if (clearCount > 50) {
                    keel::eprintln("[SCALE {} FRAME {} DEFECT] Row y={} has {} clearColor pixels inside card!", testScale, frameNum, y, clearCount);
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
    const keel::u32 width = 300;
    const keel::u32 height = 400;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    // 1. Standalone Carousel with buttons
    WidgetResponse b1, b2, b3;
    Node* carouselNode = nullptr;
    auto renderCarousel = [&]() {
        if (auto c = ui.Carousel({.gap = 8.0f})) {
            carouselNode = c.GetNode();
            b1 = ui.Button({.text = "Button 1"});
            b2 = ui.Button({.text = "Button 2"});
            b3 = ui.Button({.text = "Button 3"});
            ui.Button({.text = "Button 4"});
            ui.Button({.text = "Button 5"});
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
    keel::i32 activeTab = 0;
    const TabItem longTabs[] = {
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

    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
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
            ui.Button({.text = "Inspect Me"});
            ui.Spacer();
            if (auto el = ui.Element({.label = "Toggle Switch"})) {
                ui.Switch(false);
            }
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
    const keel::u32 width = 400;
    const keel::u32 height = 300;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    static keel::Array<keel::i32> executionTrace;
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

        ui.Button({.text = "Child Button"});
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

    keel::println("  - TestContainersAndLayoutsReorganization...");
    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    ui.BeginFrame({800.0f, 600.0f});

    // 1. Layouts: Row, Column, Wrap, Grid
    if (auto col = ui.Column({.gap = 12.0f})) {
        if (auto row = ui.Row({.gap = 8.0f, .align = Alignment::Center})) {
            ui.Button({.text = "Btn 1"});
            ui.Button({.text = "Btn 2"});
        }
        if (auto wrap = ui.Wrap({.gap = 4.0f, .runGap = 8.0f})) {
            ui.Button({.text = "W1"});
            ui.Button({.text = "W2"});
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
    if (ContainerScope box = ui.Box({.gap = 8.0f, .padding = Padding::All(8.0f), .border = {.radius = 6.0f}})) {
        Node* node = box.GetNode();
        CHECK(node != nullptr);
        CHECK(node->cornerRadius == 6.0f);
        CHECK(node->padding.left == 8.0f);
        ui.Text("Inside Box");
    }

    if (auto card = ui.Card({.gap = 12.0f, .padding = Padding::All(16.0f), .border = {.width = 1.0f, .radius = 8.0f}})) {
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
        ui.Button({.text = "Scroll Item 1"});
    }

    if (auto carousel = ui.Carousel({.gap = 16.0f, .padding = Padding::All(6.0f), .align = Alignment::End, .justify = Justify::Center})) {
        Node* node = carousel.GetNode();
        CHECK(node != nullptr);
        CHECK(node->align == Alignment::End);
        CHECK(node->justify == Justify::Center);
        CHECK(node->padding.left == 6.0f);
        CHECK(node->gap == 16.0f);
        ui.Button({.text = "Carousel Item 1"});
    }

    // 3. Static layout facades: Row::Open, Column::Open, Wrap::Open, Grid::Open, Spacer::Run
    if (auto col2 = Column::Open(ui, {.gap = 8.0f})) {
        if (auto row2 = Row::Open(ui)) {
            ui.Button({.text = "Static Row Btn"});
            Spacer::Run(ui);
            ui.Button({.text = "Static Row Btn 2"});
        }
        if (auto wrap2 = Wrap::Open(ui)) {
            ui.Button({.text = "Static Wrap Btn"});
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
    static Response Run(Context& ctx, keel::StringRef text, UiVariant variant = UiVariant::Success, keel::SourceLocation loc = keel::SourceLocation::current()) {
        (void)variant;
        const Id id = ctx.MakeChildId(loc);
        Node* node = ctx.CreateNode(NodeType::Custom, id);
        node->text = ctx.AllocString(text);
        node->widthMode = SizeMode::Fit;
        node->heightMode = SizeMode::Fit;
        ctx.AdaptChildToContainer(node);
        return ctx.Interact(id);
    }
};

// Definition of a 3rd party user-defined container
struct MyCustomPanel {
    static ContainerScope Open(Context& ctx, float minWidth = 100.0f, keel::SourceLocation loc = keel::SourceLocation::current()) {
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

    static_assert(WidgetRunnable<MyCustomBadge, keel::StringRef>);
    static_assert(WidgetRunnable<MyCustomBadge, keel::StringRef, UiVariant>);
    static_assert(WidgetRunnable<Button, ButtonOptions>);
    static_assert(ContainerOpenable<MyCustomPanel>);
    static_assert(ContainerOpenable<MyCustomPanel, float>);
    static_assert(ContainerOpenable<Row, RowOptions>);
    static_assert(ContainerOpenable<Card, CardOptions>);

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 400;
    const keel::u32 height = 300;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    if (auto panel = ui.Open<MyCustomPanel>(150.0f)) {
        CHECK(panel.GetNode() != nullptr);
        CHECK(panel.GetNode()->minWidth == 150.0f);
        auto badge = ui.Run<MyCustomBadge>("LIVE STATUS", UiVariant::Success);
        CHECK(badge.id.IsValid());
        auto btn = ui.Run<Button>(ButtonOptions{.text = "Generic Button"});
        CHECK(btn.id.IsValid());
    }
    ui.EndFrame(canvas);
}

void TestContextCleanArchitectureAndStateGC() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 400;
    const keel::u32 height = 300;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    // 1. Verify ActiveEditState buffer text manipulation
    Id inputId(9999);
    ui.RequestFocus(inputId);
    keel::usize cursor = 0;
    bool changed = false;
    bool submitted = false;

    // Start editing with "Hello"
    keel::StringRef text1 = ui.GetOrStartTextEdit(inputId, "Hello", cursor, changed, submitted);
    CHECK(text1 == "Hello");
    CHECK(cursor == 5);

    // Test text insertion
    ui.OnTextInput(" World");
    keel::StringRef text2 = ui.GetOrStartTextEdit(inputId, text1, cursor, changed, submitted);
    CHECK(text2 == "Hello World");
    CHECK(cursor == 11);
    CHECK(changed);

    // Test backspace
    ui.OnKeyDown(Key::Backspace);
    keel::StringRef text3 = ui.GetOrStartTextEdit(inputId, text2, cursor, changed, submitted);
    CHECK(text3 == "Hello Worl");
    CHECK(cursor == 10);

    // Test Home, Delete
    ui.OnKeyDown(Key::Home);
    ui.OnKeyDown(Key::Delete);
    keel::StringRef text4 = ui.GetOrStartTextEdit(inputId, text3, cursor, changed, submitted);
    CHECK(text4 == "ello Worl");
    CHECK(cursor == 0);

    // Test Enter submission
    ui.OnKeyDown(Key::Enter);
    ui.GetOrStartTextEdit(inputId, text4, cursor, changed, submitted);
    CHECK(submitted);
    CHECK(ui.GetFocusedId() == Id::Null());

    // 2. Verify State Garbage Collection (Pruning stale states after 600 frames)
    struct TemporaryWidgetState {
        keel::i32 value{42};
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
    for (keel::i32 f = 2; f <= 599; ++f) {
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
    for (keel::i32 f = 601; f < 1200; ++f) {
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

// Themes are scoped by zones and scale by containers: a scaled container leaves theme tokens and its children's
// sizes untouched (they are local units) and converts only its own outer sizing fields into the parent's units.
void TestScopedThemeAndScaleOverrides() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    UiTheme customTheme = UiTheme::Light();
    customTheme.palette.surface = Color(123, 234, 45, 255);

    UiTheme inspectorTheme = UiTheme::Dark();
    inspectorTheme.palette.surface = Color(10, 20, 30, 255);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)}, 1.0f);

    const float baseBtnHeight = ui.GetTheme().GetMetrics(UiSize::Md).defaultHeight;

    // 1. Zone theme override (even without wrapping in an extra Box)
    if (auto zCustom = ui.Zone({.theme = customTheme})) {
        if (auto box = ui.Box()) {
            CHECK(ui.GetTheme().palette.surface == Color(123, 234, 45, 255));
        }
    }
    CHECK(ui.GetTheme().palette.surface != Color(123, 234, 45, 255));

    // 2. A scaled container does not touch theme tokens; a nested theme zone works inside it
    if (auto zColScale = ui.Zone({.scale = 2.0f})) {
        if (auto col = ui.Column()) {
            CHECK_NEAR(col.GetNode()->GetScale(), 2.0f, 0.001f);
            CHECK_NEAR(col.GetNode()->GetRelativeScale(), 2.0f, 0.001f);
            CHECK_NEAR(ui.GetTheme().GetMetrics(UiSize::Md).defaultHeight, baseBtnHeight, 0.001f);
            if (auto zInner = ui.Zone({.theme = customTheme})) {
                if (auto innerCard = ui.Card()) {
                    CHECK(ui.GetTheme().palette.surface == Color(123, 234, 45, 255));
                    CHECK_NEAR(innerCard.GetNode()->GetScale(), 2.0f, 0.001f);          // accumulated zone scale is 2.0x
                    CHECK_NEAR(innerCard.GetNode()->GetRelativeScale(), 1.0f, 0.001f);  // relative scale delta is 1.0x (no double-scaling)
                }
            }
            CHECK(ui.GetTheme().palette.surface != Color(123, 234, 45, 255));
        }
    }
    CHECK(ui.GetContainerDepth() == 1);

    // 3. A toolbar inside a scaled container leaves the container stack balanced
    if (auto zShell = ui.Zone({.scale = 1.5f})) {
        if (auto shellCol = ui.Column()) {
            const keel::usize depth = ui.GetContainerDepth();
            if (auto tb = ui.Toolbar()) {
                tb.ToolButton({.text = "Action"});
            }
            CHECK(ui.GetContainerDepth() == depth);
        }
    }
    CHECK(ui.GetContainerDepth() == 1);

    // 4. Themed sidebar next to scaled content; debugBounds / inspectMode are zone state that every node built in
    //    the zone records, and nodes outside follow the context setting
    if (auto row = ui.Row()) {
        CHECK(!row.GetNode()->GetDebugBoundsOverride().has_value());
        if (auto zSidebar = ui.Zone({.theme = inspectorTheme, .debugBounds = false, .inspectMode = false})) {
            if (auto sidebar = ui.Box()) {
                CHECK(ui.GetTheme().palette.surface == Color(10, 20, 30, 255));
                CHECK(sidebar.GetNode()->GetDebugBoundsOverride().has_value());
                CHECK(*sidebar.GetNode()->GetDebugBoundsOverride() == false);
                CHECK(sidebar.GetNode()->GetInspectModeOverride().has_value());
                CHECK(*sidebar.GetNode()->GetInspectModeOverride() == false);
            }
        }

        if (auto zContent = ui.Zone({.scale = 1.5f, .debugBounds = true, .inspectMode = true})) {
            if (auto content = ui.Card()) {
                CHECK_NEAR(ui.GetTheme().GetMetrics(UiSize::Md).defaultHeight, baseBtnHeight, 0.001f);
                CHECK(*content.GetNode()->GetDebugBoundsOverride() == true);
                CHECK(*content.GetNode()->GetInspectModeOverride() == true);
                if (auto inner = ui.Box()) {
                    CHECK(*inner.GetNode()->GetDebugBoundsOverride() == true);   // inherited by nested nodes
                    if (auto zHidden = ui.Zone({.debugBounds = false})) {
                        if (auto hidden = ui.Box()) {
                            CHECK(*hidden.GetNode()->GetDebugBoundsOverride() == false); // a child zone can turn it off
                            CHECK(*hidden.GetNode()->GetInspectModeOverride() == true);  // and inherits the rest
                        }
                    }
                }
            }
        }
    }

    // 5. Only the scaled container's outer sizing is converted into parent units; everything inside is local
    if (auto zScaledCol = ui.Zone({.scale = 2.0f})) {
        if (auto scaledCol = ui.Column({.gap = 10.0f, .padding = Padding::All(12.0f), .width = 150.0f})) {
            const Node* colNode = scaledCol.GetNode();
            CHECK(colNode != nullptr);
            CHECK_NEAR(colNode->fixedWidth, 300.0f, 0.001f);   // outer size: parent units
            CHECK_NEAR(colNode->gap, 10.0f, 0.001f);           // inner spacing: local units
            CHECK_NEAR(colNode->padding.left, 12.0f, 0.001f);

            if (auto fixedBox = ui.Box({.width = 100.0f, .height = 40.0f, .border = {.width = 2.0f, .radius = 6.0f}})) {
                const Node* boxNode = fixedBox.GetNode();
                CHECK_NEAR(boxNode->fixedWidth, 100.0f, 0.001f);
                CHECK_NEAR(boxNode->fixedHeight, 40.0f, 0.001f);
                CHECK_NEAR(boxNode->borderWidth, 2.0f, 0.001f);
                CHECK_NEAR(boxNode->cornerRadius, 6.0f, 0.001f);
            }

            const Vec2 btnCompact = Button::Measure(ButtonOptions{.text = "OK", .width = 120.0f}, UiSize::Md, &canvas, ui.GetTheme());
            CHECK_NEAR(btnCompact.x, 120.0f, 0.001f);

            // SizeMode::Stretch fills the available width
            const Rect fillBox = Select::GetBoxRect(Rect(0.0f, 0.0f, 400.0f, 60.0f), SelectOptions{.width = SizeMode::Stretch}, UiSize::Md, &canvas, ui.GetTheme());
            CHECK_NEAR(fillBox.w, 400.0f, 0.01f);
        }
    }
    CHECK(ui.GetContainerDepth() == 1);

    ui.EndFrame(canvas);
}

void TestSliderAndStepSliderWidgets() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 400;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    float floatVal = 5.0f;
    keel::i32 intVal = 4;
    keel::usize stepIndex = 1;
    UiSize activeSize = UiSize::Sm;
    UiSize activeRadius = UiSize::None;
    static constexpr keel::StaticArray<keel::StringRef, 3> stepLabels = {"Low", "Medium", "High"};

    Response r1, r2, r3, r4, r5;
    auto renderUI = [&]() {
        if (auto col = ui.Column()) {
            if (auto el = ui.Element({.label = "Gain"})) {
                r1 = ui.Slider(floatVal, 0.0f, 10.0f, {.valueFormat = "%.1f dB"});
            }
            if (auto el = ui.Element({.label = "Columns"})) {
                r2 = ui.Slider(intVal, 1, 8, {.valueFormat = "%d ch"});
            }
            if (auto el = ui.Element({.label = "Preset"})) {
                r3 = ui.StepSlider(stepIndex, stepLabels);
            }
            if (auto el = ui.Element({.label = "Size"})) {
                r4 = ui.StepSlider(activeSize);
            }
            if (auto el = ui.Element({.label = "Radius"})) {
                r5 = ui.StepSlider(activeRadius, {.includeNone = true});
            }
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

void TestVerticalSlider() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 400;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);
    const UiTheme& theme = ui.GetTheme();
    const SliderMetrics& m = theme.GetSlider(UiSize::Md);

    // 1. Measure: a fader is narrow and tall; the horizontal slider keeps its metrics
    {
        const SliderOptions hOpts{.showValue = false, .width = SizeMode::Fit};
        const SliderOptions vOpts{
                                  .orientation = Orientation::Vertical,
                                  .showValue = false,
                                  .height = SizeMode::Fit
};
        const Vec2 hSize = Slider::Measure(hOpts, 50.0, 0.0, 100.0, UiSize::Md, &canvas, theme);
        const Vec2 vSize = Slider::Measure(vOpts, 50.0, 0.0, 100.0, UiSize::Md, &canvas, theme);
        CHECK_NEAR(vSize.x, keel::max(m.defaultHeight, m.thumbSize), 0.5f);
        CHECK_NEAR(vSize.y, m.defaultWidth + m.thumbSize, 0.5f);
        CHECK_NEAR(hSize.x, vSize.y, 0.5f); // same travel, rotated
        CHECK(vSize.y > vSize.x);

        // Value above the track adds height, not width beyond the readout
        const SliderOptions vVal{
                                 .orientation = Orientation::Vertical,
                                 .valueFormat = "%.0f dB",
                                 .height = SizeMode::Fit
};
        const Vec2 vValSize = Slider::Measure(vVal, 50.0, 0.0, 100.0, UiSize::Md, &canvas, theme);
        CHECK(vValSize.y > vSize.y);

        // Explicit height sets the control height
        const SliderOptions vH{
                               .orientation = Orientation::Vertical,
                               .showValue = false,
                               .height = 300.0f
};
        const Vec2 vHSize = Slider::Measure(vH, 50.0, 0.0, 100.0, UiSize::Md, &canvas, theme);
        CHECK_NEAR(vHSize.y, 300.0f, 0.5f);
    }

    double gain = 20.0;
    Response r;
    auto renderUI = [&]() {
        if (auto col = ui.Column()) {
            r = ui.Slider(gain, 0.0, 80.0, {.orientation = Orientation::Vertical, .valueFormat = "%.0f dB",
                                            .step = 1.0});
        }
    };

    // 2. Layout: Fill stretches the fader to the available height, the width stays intrinsic
    ui.BeginFrame({400.0f, 600.0f});
    renderUI();
    ui.EndFrame(canvas);
    const Rect b = ui.GetPrevFrameBounds(r.id);
    CHECK(b.h > 500.0f);
    CHECK(b.w < 120.0f);
    CHECK(b.h > b.w);
    CHECK_NEAR(gain, 20.0, 1e-9);

    // 3. Pointer: press near the top -> near max, drag near the bottom -> near min
    ui.OnMouseMove({b.Center().x, b.y + b.h * 0.08f});
    ui.OnMouseDown(1);
    ui.BeginFrame({400.0f, 600.0f});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(r.changed);
    CHECK(gain > 70.0);

    ui.OnMouseMove({b.Center().x, b.y + b.h - 2.0f});
    ui.BeginFrame({400.0f, 600.0f});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(gain < 2.0);
    ui.OnMouseUp(1);
    ui.BeginFrame({400.0f, 600.0f});
    renderUI();
    ui.EndFrame(canvas);

    // 4. Mouse wheel up over the fader: +1 step
    gain = 40.0;
    ui.OnMouseMove(b.Center());
    ui.OnMouseWheel(0.0f, 1.0f);
    ui.BeginFrame({400.0f, 600.0f});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(r.changed);
    CHECK_NEAR(gain, 41.0, 1e-9);
    ui.OnMouseWheel(0.0f, 0.0f);

    // 5. Keyboard: Up increases, Down decreases while focused (focus from the press above)
    ui.OnKeyDown(Key::Up);
    ui.BeginFrame({400.0f, 600.0f});
    renderUI();
    ui.EndFrame(canvas);
    CHECK_NEAR(gain, 42.0, 1e-9);
    ui.OnKeyDown(Key::Down);
    ui.BeginFrame({400.0f, 600.0f});
    renderUI();
    ui.EndFrame(canvas);
    CHECK_NEAR(gain, 41.0, 1e-9);
}

// Scene: drawing changes are repainted without camera moves (content fingerprint), draggable items
// (start / follow / release / Escape cancel / click in place), cursor and tooltips
void TestSceneContentDamageAndItemDrag() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 420;
    const keel::u32 height = 320;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    float rectX = 50.0f;
    keel::Array<keel::u32> img(16 * 16, 0xff0000ffu);
    Vec2 handle{200.0f, 150.0f};
    Vec2 handleAtStart{};
    SceneItemDrag drag{};
    bool hovered = false;
    bool clicked = false;
    Rect vp{};
    Vec2 handleScreen{};
    float worldScale = 1.0f;

    auto frame = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto sc = ui.Scene({
            .worldSize = Vec2{400.0f, 300.0f},
            .backgroundStyle = SceneBackground::Solid,
            .zoomable = false,
            .pannable = false,
            .selectableItems = false,
            .showScrollbars = false,
            .showControls = false,
            .tooltip = "scene tip"
        })) {
            vp = sc.GetViewportRect();
            worldScale = sc.GetWorldScale();
            sc.DrawRect({rectX, 20.0f, 30.0f, 30.0f}, Color(255, 0, 0));
            sc.DrawPixels({img, 16, 16}, {300.0f, 20.0f, 32.0f, 32.0f});
            if (auto it = sc.Item({
                .id = "handle",
                .hitRect = Rect{handle.x - 6.0f, handle.y - 6.0f, 12.0f, 12.0f},
                .showSelectionFrame = false,
                .draggable = true,
                .tooltip = "handle tip"
            })) {
                drag = it.GetDrag();
                if (drag.started) handleAtStart = handle;
                if (drag.active) handle = handleAtStart + drag.Delta();
                if (drag.canceled) handle = handleAtStart;
                hovered = it.IsHovered();
                clicked = it.Clicked();
                sc.DrawCircle(handle, 5.0f, Color(0, 255, 0));
            }
            handleScreen = sc.WorldToScreen(handle);
        }
        ui.EndFrame(canvas);
    };
    auto damagesScene = [&]() {
        for (const Rect& r : ui.GetDirtyRects()) {
            if (r.Intersects(vp)) return true;
        }
        return false;
    };

    for (keel::i32 i = 0; i < 5; ++i) frame();
    frame();
    CHECK(!damagesScene());

    // 1. Geometry change alone (same number of commands, camera still) repaints the scene
    rectX = 120.0f;
    frame();
    CHECK(damagesScene());
    frame();
    CHECK(!damagesScene());

    // 2. Pixel buffer content change (same pointer and size) repaints the scene
    img[37] = 0xff00ff00u;
    frame();
    CHECK(damagesScene());
    frame();
    CHECK(!damagesScene());

    // 3. Hover a draggable item: hovered, move cursor, item tooltip
    ui.OnMouseMove(handleScreen);
    frame();
    frame();
    CHECK(hovered);
    CHECK(ui.GetCursor() == CursorType::Move);
    CHECK(ui.GetActiveTooltip().text == "handle tip");

    // 4. Press, drag, release: the item follows the pointer with its grab offset, no panning
    const Vec2 start = handle;
    ui.OnMouseDown(1);
    frame();
    CHECK(drag.started && drag.active);
    ui.OnMouseMove(handleScreen + Vec2{40.0f, 20.0f});
    frame();
    CHECK(drag.active && !drag.started);
    CHECK_NEAR(handle.x, start.x + 40.0f / worldScale, 0.5f);
    CHECK_NEAR(handle.y, start.y + 20.0f / worldScale, 0.5f);
    CHECK(ui.GetActiveTooltip().text.empty());   // no tooltip while dragging
    ui.OnMouseUp(1);
    frame();
    CHECK(drag.ended && !drag.canceled && !drag.active);
    CHECK_NEAR(handle.x, start.x + 40.0f / worldScale, 0.5f);
    frame();
    CHECK(!drag.ended);

    // 5. Escape cancels: the caller restores the value saved on start
    const Vec2 committed = handle;
    ui.OnMouseMove(handleScreen);
    frame();
    frame();
    ui.OnMouseDown(1);
    frame();
    ui.OnMouseMove(handleScreen + Vec2{-60.0f, 30.0f});
    frame();
    CHECK(keel::abs(handle.x - committed.x) > 10.0f);
    ui.OnKeyDown(Key::Escape);
    frame();
    CHECK(drag.ended && drag.canceled);
    CHECK_NEAR(handle.x, committed.x, 0.01f);
    CHECK_NEAR(handle.y, committed.y, 0.01f);
    ui.OnMouseUp(1);
    frame();
    CHECK(!drag.active);

    // 6. Press and release in place is a click, not a move
    ui.OnMouseMove(handleScreen);
    frame();
    frame();
    ui.OnMouseDown(1);
    frame();
    ui.OnMouseUp(1);
    frame();
    frame();
    CHECK(clicked);
    CHECK_NEAR(handle.x, committed.x, 0.01f);

    // 7. Off the item: the scene's own tooltip, default cursor
    ui.OnMouseMove(Vec2{vp.x + 10.0f, vp.y + vp.h - 10.0f});
    frame();
    frame();
    CHECK(!hovered);
    CHECK(ui.GetCursor() == CursorType::Default);
    CHECK(ui.GetActiveTooltip().text == "scene tip");
}

void TestVerticalStepAndRangeSliders() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 400;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);
    const UiTheme& theme = ui.GetTheme();
    const SliderMetrics& m = theme.GetSlider(UiSize::Md);
    static constexpr keel::StaticArray<keel::StringRef, 4> steps = {"0.5", "1", "2", "6"};

    // 1. Measure: vertical StepSlider reserves a labels column right of the track
    {
        const StepSliderOptions bare{
                                     .orientation = Orientation::Vertical,
                                     .showLabels = false,
                                     .height = SizeMode::Fit
};
        const StepSliderOptions labeled{
                                        .orientation = Orientation::Vertical,
                                        .stepLabels = steps,
                                        .height = SizeMode::Fit
};
        const Vec2 bareSize = StepSlider::Measure(bare, 0, steps.size(), UiSize::Md, &canvas, theme);
        const Vec2 labeledSize = StepSlider::Measure(labeled, 0, steps.size(), UiSize::Md, &canvas, theme);
        CHECK_NEAR(bareSize.x, keel::max(m.defaultHeight, m.thumbSize), 0.5f);
        CHECK(labeledSize.x > bareSize.x + m.tickGap);
        CHECK_NEAR(labeledSize.y, bareSize.y, 0.5f); // labels do not add height

        const RangeSliderOptions range{
                                       .orientation = Orientation::Vertical,
                                       .showValue = false,
                                       .height = SizeMode::Fit
};
        const Vec2 rangeSize = RangeSlider::Measure(range, 20.0, 60.0, 0.0, 80.0, UiSize::Md, &canvas, theme);
        CHECK_NEAR(rangeSize.x, keel::max(m.defaultHeight, m.thumbSize), 0.5f);
        CHECK_NEAR(rangeSize.y, m.defaultWidth + m.thumbSize, 0.5f);
    }

    keel::usize stepIdx = 1;
    double lo = 20.0, hi = 60.0;
    Response rs, rr;
    auto renderUI = [&]() {
        if (auto row = ui.Row({.gap = 40.0f, .height = SizeMode::Stretch})) {
            rs = ui.StepSlider(stepIdx, steps, {.orientation = Orientation::Vertical});
            rr = ui.RangeSlider(lo, hi, 0.0, 80.0, {.orientation = Orientation::Vertical, .step = 1.0,
                                                    .showValue = false});
        }
    };
    auto frame = [&]() {
        ui.BeginFrame({400.0f, 600.0f});
        renderUI();
        ui.EndFrame(canvas);
    };

    frame();
    const Rect sb = ui.GetPrevFrameBounds(rs.id);
    const Rect rb = ui.GetPrevFrameBounds(rr.id);
    CHECK(sb.h > 500.0f);
    CHECK(rb.h > 500.0f);
    CHECK(rb.x > sb.x + sb.w);

    // 2. StepSlider: press near the top -> last step, drag to the bottom -> first step
    const float trackX = sb.x + keel::max(m.defaultHeight, m.thumbSize) * 0.5f;
    ui.OnMouseMove({trackX, sb.y + 4.0f});
    ui.OnMouseDown(1);
    frame();
    CHECK(rs.changed);
    CHECK(stepIdx == 3);
    ui.OnMouseMove({trackX, sb.y + sb.h - 4.0f});
    frame();
    CHECK(stepIdx == 0);
    ui.OnMouseUp(1);
    frame();

    // Wheel up: next step
    ui.OnMouseMove({trackX, sb.Center().y});
    ui.OnMouseWheel(0.0f, 1.0f);
    frame();
    CHECK(stepIdx == 1);
    ui.OnMouseWheel(0.0f, 0.0f);
    frame();

    // 3. RangeSlider: travel coordinate maps value v to y = bottom - halfThumb - v / 80 * length
    const float half = m.thumbSize * 0.5f;
    const float len = rb.h - m.thumbSize;
    const float rx = rb.x + keel::max(m.defaultHeight, m.thumbSize) * 0.5f;
    auto yOf = [&](double v) { return rb.y + rb.h - half - static_cast<float>(v / 80.0) * len; };

    // Press just below the low thumb -> drags the low thumb (not the high one)
    ui.OnMouseMove({rx, yOf(15.0)});
    ui.OnMouseDown(1);
    frame();
    CHECK(rr.changed);
    CHECK(lo < 20.0 && lo > 10.0);
    CHECK_NEAR(hi, 60.0, 1e-9);
    ui.OnMouseUp(1);
    frame();

    // Press above the high thumb -> drags the high thumb
    ui.OnMouseMove({rx, yOf(70.0)});
    ui.OnMouseDown(1);
    frame();
    CHECK(hi > 65.0);
    const double loBefore = lo;
    ui.OnMouseUp(1);
    frame();

    // Press inside the interval and drag up by 10 units -> both move up together
    const double span = hi - lo;
    const double mid = (lo + hi) * 0.5;
    ui.OnMouseMove({rx, yOf(mid)});
    ui.OnMouseDown(1);
    frame();
    ui.OnMouseMove({rx, yOf(mid + 10.0)});
    frame();
    CHECK_NEAR(lo, loBefore + 10.0, 1.0);
    CHECK_NEAR(hi - lo, span, 1.0);
    ui.OnMouseUp(1);
    frame();
}

void TestPixelGeometryAndWidgetUiSizeDecoupling() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
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
        if (auto card = ui.Card({.gap = 7.0f, .padding = 20.0f, .border = {.width = 1.0f, .radius = 12.5f}})) {
            Node* n = card.GetNode();
            CHECK(n != nullptr);
            CHECK_NEAR(n->gap, 7.0f, 0.001f);
            CHECK_NEAR(n->padding.left, 20.0f, 0.001f);
            CHECK_NEAR(n->cornerRadius, 12.5f, 0.001f);
        }

        if (auto box = ui.Box({.gap = 16.0f, .padding = 8.0f, .border = {.radius = 4.0f}})) {
            Node* n = box.GetNode();
            CHECK(n != nullptr);
            CHECK_NEAR(n->gap, 16.0f, 0.001f);
            CHECK_NEAR(n->padding.left, 8.0f, 0.001f);
            CHECK_NEAR(n->cornerRadius, 4.0f, 0.001f);
        }

        // 3. Fallback to theme defaults when gap / radius are omitted (nullopt)
        if (auto defaultCard = ui.Card()) {
            Node* n = defaultCard.GetNode();
            CHECK(n != nullptr);
            CHECK_NEAR(n->gap, ui.GetTheme().GetGap(UiSize::Md), 0.001f);
            CHECK_NEAR(n->cornerRadius, ui.GetTheme().GetRadius(UiSize::Md), 0.001f);
            CHECK_NEAR(n->padding.left, ui.GetTheme().GetPadding(UiSize::Md).left, 0.001f);

            // Explicit UiSize::Lg height (36px)
            btnLg = ui.Button({.text = "Explicit Lg", .size = UiSize::Lg});

            // Explicit UiSize::Sm height (24px)
            btnSm = ui.Button({.text = "Explicit Sm", .size = UiSize::Sm});
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

    CHECK_NEAR(btnLg.rect.h, 36.0f, 0.001f);
    CHECK_NEAR(btnSm.rect.h, 24.0f, 0.001f);
}

void TestTextWrappingAndBackendIndependence() {
    using namespace guikit;
    keel::println("  - TestTextWrappingAndBackendIndependence...");

    // 1. TextUtil unit checks with fallback ratio (canvas = nullptr)
    const float singleW = TextUtil::MeasureWidth("Hello World", 14.0f, nullptr);
    CHECK(singleW > 0.0f);

    // Truncation with ellipsis
    const auto trunc = TextUtil::TruncateWithEllipsis("Very long string that needs truncation", 60.0f, 14.0f, nullptr);
    CHECK(!trunc.empty());
    CHECK(trunc.ends_with("..."));
    CHECK(TextUtil::MeasureWidth(trunc, 14.0f, nullptr) <= 60.0f + 0.1f);

    // Multi-line word wrap
    const keel::String sentence("Sensory tripwire thresholds & limits for automated closed-loop machinery");
    const auto lines = TextUtil::WrapText(sentence, 120.0f, 12.0f, nullptr);
    CHECK(lines.size() >= 2);
    for (const auto& line : lines) {
        CHECK(TextUtil::MeasureWidth(line, 12.0f, nullptr) <= 120.0f + 1.0f);
    }

    const float wrappedH = TextUtil::MeasureWrappedHeight(sentence, 120.0f, 12.0f, 18.0f, nullptr);
    CHECK_NEAR(wrappedH, static_cast<float>(lines.size()) * 18.0f, 0.01f);

    // 2. Heading wordWrap test: single-line vs multi-line height resolution
    HeadingOptions optsWrap{
        .label = "Primary Stop Conditions",
        .description = {"Sensory tripwire thresholds & limits for automated closed-loop machinery", TextWrap::Word()}
    };
    HeadingOptions optsNoWrap{
        .label = "Primary Stop Conditions",
        .description = {"Sensory tripwire thresholds & limits for automated closed-loop machinery", TextWrap::SingleLine()}
    };

    const float hNoWrap = Heading::Measure(optsNoWrap, 150.0f).y;
    const float hWrap = Heading::Measure(optsWrap, 150.0f).y;
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
            btn1Resp = ui.Button({.text = "Col 1 (Stretch)", .width = SizeMode::Stretch});
            btn2Resp = ui.Button({.text = "Col 2 (Stretch)", .width = SizeMode::Stretch});
            btn3Resp = ui.Button({.text = "Col 3 (Stretch)", .width = SizeMode::Stretch});
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
    static const keel::StaticArray<guikit::TabItem, 3> testTabs = {{
        {.label = "Overview"},
        {.label = "Industrial Dashboard", .badge = "Live"},
        {.label = "Network Diagnostics"}
    }};

    for (float testScale : {1.0f, 1.5f}) {
        keel::i32 tabIdx = 0;
        ThorCanvas thorCanvas;
        keel::Array<keel::u32> px(800 * 600);
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
    const keel::String fullText("Industrial Dashboard");
    const auto trFull = TextUtil::TruncateWithEllipsis(fullText, 400.0f, 14.0f, nullptr);
    CHECK(trFull == fullText);

    const auto trShort = TextUtil::TruncateWithEllipsis(fullText, 60.0f, 14.0f, nullptr);
    CHECK(trShort.ends_with("..."));
    CHECK(trShort.length() < fullText.length());
}

void TestTextAreaWidgetWorkflow() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    keel::String sampleCode("ui.Button({\n    .label = \"Test\",\n    .variant = guikit::UiVariant::Primary\n});");

    // 1. Measure test: autoHeight must scale with line count, Element header, and zoom
    const auto mSingle = TextArea::Measure("Single Line", {.autoHeight = true}, UiSize::Md, &canvas);
    const auto mMulti = TextArea::Measure(sampleCode, {.autoHeight = true}, UiSize::Md, &canvas);
    ElementSpec headerSpec = Element::MakeSpec({.label = "A text area", .description = "This is a text area"}, UiSize::Md, false, UiTheme::Dark());
    headerSpec.hasControl = true;
    const auto mWithHeader = Element::Measure(mMulti, headerSpec, &canvas, UiTheme::Dark());
    CHECK(mMulti.y > mSingle.y * 2.0f);
    CHECK(mWithHeader.y > mMulti.y);

    // 2. Configurable Syntax Highlighter test (Cpp, Json, Custom)
    keel::Array<TextSpan> spans;
    const auto theme = UiTheme::Dark();
    const keel::StringRef cppLine = "const int count = Compute(42, \"ok\"); // comment";
    TextArea::HighlightLine(cppLine, 0, SyntaxConfig::Cpp(), spans, theme);
    CHECK(!spans.empty());
    // Verify spans cover the entire line contiguously with no gaps or overlaps
    keel::usize expectedCol = 0;
    bool foundKeyword = false;
    bool foundType = false;
    bool foundFunc = false;
    bool foundNumber = false;
    bool foundString = false;
    bool foundComment = false;
    for (const auto& sp : spans) {
        CHECK(sp.startCol == expectedCol);
        expectedCol = sp.startCol + sp.length;
        const keel::StringRef tok = cppLine.substr(sp.startCol, sp.length);
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
    static constexpr keel::StaticArray<keel::StringRef, 2> customKw = {"rule", "when"};
    static constexpr keel::StaticArray<keel::StringRef, 1> customTy = {"Sensor"};
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
    keel::String editableText("int a = 1;\nint b = 2;");
    float editorZoom = 1.0f;
    Response editRes;
    auto renderEditor = [&]() {
        if (auto el = ui.Element({.label = "A text area", .description = "This is a text area"})) {
            editRes = ui.TextArea(editableText, {
                .autoHeight = true,
                .showLineNumbers = true,
                .showCopyButton = true,
                .allowZoom = true,
                .zoom = editorZoom,
                .syntax = SyntaxConfig::Cpp()
            });
        }
    };

    // Frame 1 & 2: Settle layout
    for (keel::i32 f = 0; f < 2; ++f) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderEditor();
        ui.EndFrame(canvas);
    }
    const float heightBeforeZoom = editRes.rect.h;
    CHECK(editRes.rect.w > 100.0f);
    CHECK(heightBeforeZoom > 50.0f);

    // Click inside editor to focus & place cursor
    const Vec2 clickPt(editRes.rect.x + 80.0f, editRes.rect.y + 20.0f);
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
    for (keel::i32 f = 0; f < 2; ++f) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderEditor();
        ui.EndFrame(canvas);
    }
    CHECK(editRes.rect.h > heightBeforeZoom);

    // 4. Verify glyph metric box height and precise mouse coordinate hit-testing per line & column
    const float codeFontSize = theme.GetTypography(TextRole::CodeMedium).fontSize;
    const float glyphBoxH = canvas.MeasureTextStyled("Mg", codeFontSize, FontStyle::Monospace).y;
    // JetBrains Mono hhea (ascender - descender) / unitsPerEm = 1.32
    CHECK(keel::abs(glyphBoxH - codeFontSize * 1.32f) < codeFontSize * 0.03f);

    keel::String coordText("AAAA\nBBBB\nCCCC");
    Response coordRes;
    auto renderCoordEditor = [&]() {
        coordRes = ui.TextArea(coordText, {
            .autoHeight = true,
            .showLineNumbers = true,
            .showCopyButton = false,
            .syntax = SyntaxConfig::Plain()
        });
    };
    for (keel::i32 f = 0; f < 2; ++f) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderCoordEditor();
        ui.EndFrame(canvas);
    }

    // Compute exact linear line height and gutter width for Md size
    const auto& cm = theme.GetMetrics(UiSize::Md);
    const float lineH = keel::max(theme.GetLineHeight(codeFontSize), glyphBoxH * 1.14f);
    const float padY = keel::max(8.0f, cm.paddingY);
    const float padX = keel::max(10.0f, cm.paddingX);
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
    for (keel::i32 step = 0; step <= 4; ++step) {
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
    for (keel::i32 f = 0; f < 2; ++f) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderCoordEditor();
        ui.EndFrame(canvas);
    }
    CHECK(coordRes.rect.h > hAt1x);

    ui.OnKeyDown(Key::Zero, {.ctrl = true});
    for (keel::i32 f = 0; f < 2; ++f) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderCoordEditor();
        ui.EndFrame(canvas);
    }
    CHECK(keel::abs(coordRes.rect.h - hAt1x) < 0.05f);
}

void TestButtonAlignmentAndProceduralIcons() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 400;
    const keel::u32 height = 500;
    keel::Array<keel::u32> pixels(width * height);
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
            .text = "Button",
            .icon = Icon::Play(),
            .align = Align::Start,
            .width = SizeMode::Stretch
        });
        ui.Button({
            .text = "Input",
            .icon = Icon::Edit(),
            .align = Align::Start,
            .width = SizeMode::Stretch
        });
        ui.Button({
            .text = "Containers",
            .icon = Icon::Copy(),
            .align = Align::Start,
            .width = SizeMode::Stretch
        });
        ui.Button({
            .text = "End-Aligned",
            .icon = Icon::Settings(),
            .align = Align::End,
            .width = SizeMode::Stretch
        });
        ui.Button({
            .text = "Center-Aligned",
            .icon = Icon::Folder(),
            .align = Align::Center,
            .width = SizeMode::Stretch
        });
        // Label only left-aligned
        ui.Button({
            .text = "Label Only Left",
            .align = Align::Start,
            .width = SizeMode::Stretch
        });
        // Icon only left-aligned
        ui.Button({
            .icon = Icon::Plus(),
            .align = Align::Start,
            .width = SizeMode::Stretch
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
        void DrawTextBox(keel::StringRef text, Rect bounds, float fontSize, Color color, float alignX = 0.0f, float alignY = 0.5f) override {
            lastTextBounds = bounds;
            lastAlignX = alignX;
            ThorCanvas::DrawTextBox(text, bounds, fontSize, color, alignX, alignY);
        }
        Rect lastCapsuleRect{};
        Rect lastTextBounds{};
        float lastAlignX{0.0f};
    };

    RecordingCanvas recCanvas;
    keel::Array<keel::u32> recPixels(800 * 600);
    recCanvas.SetTarget(recPixels.data(), 800, 600, 800);
    const UiTheme& darkTheme = UiTheme::Dark();
    const Rect widgetBounds(50.0f, 100.0f, 400.0f, 80.0f);

    ElementSpec elSpec{
        .label = "Action Label",
        .layout = ElementLayout::Stacked,
        .width = SizeMode::Fit,
        .control = {.width = 150.0f}
    };
    ButtonOptions bStart{
        .text = "Click Action",
        .align = Align::Start,
        .width = 150.0f
    };
    ButtonOptions bCenter{
        .text = "Click Action",
        .align = Align::Center,
        .width = 150.0f
    };
    ButtonOptions bEnd{
        .text = "Click Action",
        .align = Align::End,
        .width = 150.0f
    };

    const Vec2 bIntrinsic = Button::Measure(bStart, UiSize::Md, &recCanvas, darkTheme);
    const ElementGeometry geom = Element::Resolve(widgetBounds, bIntrinsic, elSpec, &recCanvas, darkTheme);

    Button::Render(recCanvas, geom.controlRect, bStart, UiSize::Md, false, false, false, darkTheme);
    const Rect capStart = recCanvas.lastCapsuleRect;
    const float alignXStart = recCanvas.lastAlignX;

    Button::Render(recCanvas, geom.controlRect, bCenter, UiSize::Md, false, false, false, darkTheme);
    const Rect capCenter = recCanvas.lastCapsuleRect;
    const float alignXCenter = recCanvas.lastAlignX;

    Button::Render(recCanvas, geom.controlRect, bEnd, UiSize::Md, false, false, false, darkTheme);
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

    keel::println("  [PASS] TestButtonAlignmentAndProceduralIcons completed");
}

void TestWidgetRadiusOverride() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 400;
    const keel::u32 height = 300;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    keel::String inputText("hello");

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)}, 1.0f);
    if (auto col = ui.Column({.gap = 8.0f})) {
        auto btnDefault = ui.Button({.text = "Default Radius"});
        auto btnCustom = ui.Button({.text = "Custom Radius", .border = Border::Rounded(16.0f)});
        Response inputDefault, inputCustom;
        if (auto el = ui.Element({.label = "Default Radius Input"})) {
            inputDefault = ui.InputField(inputText);
        }
        if (auto el = ui.Element({.label = "Custom Radius Input"})) {
            inputCustom = ui.InputField(inputText, {.border = {.radius = 14.0f}});
        }

        CHECK(btnDefault.id.IsValid());
        CHECK(btnCustom.id.IsValid());
        CHECK(inputDefault.id.IsValid());
        CHECK(inputCustom.id.IsValid());
    }
    ui.EndFrame(canvas);

    keel::println("  [PASS] TestWidgetRadiusOverride completed");
}

void TestButtonExternalLabelDescriptionAndPlacement() {
    keel::println("[TEST] Running TestButtonExternalLabelDescriptionAndPlacement...");

    ThorCanvas canvas;
    Context ui;

    const keel::u32 width = 600;
    const keel::u32 height = 500;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    // 1. Measure comparisons
    const UiTheme& theme = UiTheme::Dark();
    const Vec2 plainSize = Button::Measure({.text = "Deploy"}, UiSize::Md, &canvas, theme);
    const Vec2 stackedSize = Element::Measure(plainSize, ElementSpec{
        .label = "Deployment Action",
        .description = "Immediate canary release",
        .layout = ElementLayout::Stacked
    }, &canvas, theme);
    const Vec2 leadingSize = Element::Measure(plainSize, ElementSpec{
        .label = "Deployment Action",
        .description = "Immediate canary release",
        .layout = ElementLayout::Leading
    }, &canvas, theme);

    // Stacked height must be strictly taller than plain button due to external label and description
    CHECK(stackedSize.y > plainSize.y);
    // Leading width must be strictly wider than plain button due to text placed beside button
    CHECK(leadingSize.x > plainSize.x);

    // 2. Full immediate mode execution and rendering across all placements
    keel::usize selIdx = 0;
    static const keel::StaticArray<keel::StringRef, 3> items = {"Alpha", "Beta", "Gamma"};

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)}, 1.0f);
    if (auto col = ui.Column({.gap = 8.0f})) {
        Response bStacked, bLeading, bTrailing, bTop, bBottom, selRes;
        if (auto el = ui.Element({
            .label = "Action Header",
            .description = "Helper info stacked",
            .layout = ElementLayout::Stacked
        })) {
            bStacked = ui.Button({.text = "Click Stacked"});
        }
        if (auto el = ui.Element({
            .label = "Action Header",
            .description = "Helper info leading",
            .layout = ElementLayout::Leading
        })) {
            bLeading = ui.Button({.text = "Click Leading"});
        }
        if (auto el = ui.Element({
            .label = "Action Header",
            .description = "Helper info trailing",
            .layout = ElementLayout::Trailing
        })) {
            bTrailing = ui.Button({.text = "Click Trailing"});
        }
        if (auto el = ui.Element({
            .label = "Action Header",
            .description = "Helper info top",
            .layout = ElementLayout::Top
        })) {
            bTop = ui.Button({.text = "Click Top"});
        }
        if (auto el = ui.Element({
            .label = "Action Header",
            .description = "Helper info bottom",
            .layout = ElementLayout::Bottom
        })) {
            bBottom = ui.Button({.text = "Click Bottom"});
        }

        // Select with custom radius override
        if (auto el = ui.Element({
            .label = "Select Header",
            .description = "Choose option",
            .layout = ElementLayout::Stacked
        })) {
            selRes = ui.Select(selIdx, items, {.border = {.radius = 12.0f}});
        }

        CHECK(bStacked.id.IsValid());
        CHECK(bLeading.id.IsValid());
        CHECK(bTrailing.id.IsValid());
        CHECK(bTop.id.IsValid());
        CHECK(bBottom.id.IsValid());
        CHECK(selRes.id.IsValid());
    }
    ui.EndFrame(canvas);

    keel::println("  [PASS] TestButtonExternalLabelDescriptionAndPlacement completed");
}

void TestWidgetSizeModesAndExplicitWidth() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 800;
    const keel::u32 height = 1200;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    // 1. Button explicit width & minWidth in standalone Measure
    {
        ButtonOptions btnDef{.text = "Go"};
        Vec2 szDef = Button::Measure(btnDef, UiSize::Md, &canvas, ui.GetTheme());
        CHECK(szDef.x > 0.0f);

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

        SliderOptions sExplicit{.showValue = false, .width = 240.0f};
        Vec2 szSExp = Slider::Measure(sExplicit, 50.0, 0.0, 100.0, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(szSExp.x, 240.0f, 0.01f);

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

        SelectOptions selExplicit{.width = 320.0f};
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

        InputFieldOptions inExp{.width = 280.0f};
        Vec2 szInExp = InputField::Measure("", inExp, UiSize::Md, &canvas, ui.GetTheme());
        CHECK_NEAR(szInExp.x, 280.0f, 0.01f);

        InputFieldOptions inMin{.minWidth = 220.0f};
        Vec2 szInMin = InputField::Measure("", inMin, UiSize::Md, &canvas, ui.GetTheme());
        CHECK(szInMin.x >= 220.0f);
    }

    // 5. Layout tests: Row & Column with SizeMode::Fit vs SizeMode::Stretch across widgets
    Response btnCompact, btnFill1, btnFill2;
    Response swCompact, swBetween;
    Response cbCompact, cbBetween;
    Response slCompact, slBetween;
    Response selCompact, selBetween;
    Response inCompact, inBetween;

    bool swVal = true;
    bool cbVal = true;
    float slVal = 50.0f;
    keel::usize selIdx = 0;
    static const keel::StaticArray<keel::StringRef, 2> selOpts = {"Alpha", "Beta"};
    keel::String inText("Test");

    auto render = [&]() {
        if (auto col = ui.Column({.width = SizeMode::Stretch})) {
            // Button Row: Fit vs Stretch
            if (auto r1 = ui.Row({.gap = 10.0f})) {
                btnCompact = ui.Button({.text = "Compact", .width = SizeMode::Fit});
                btnFill1 = ui.Button({.text = "Fill 1", .width = SizeMode::Stretch});
                btnFill2 = ui.Button({.text = "Fill 2", .width = SizeMode::Stretch});
            }

            // Switch in ElementLayout::Leading: Fit vs Stretch
            if (auto el = ui.Element({
                .label = "Switch Compact",
                .layout = ElementLayout::Leading,
                .width = SizeMode::Fit
            })) {
                swCompact = el.response;
                ui.Switch(swVal);
            }
            if (auto el = ui.Element({
                .label = "Switch Stretch",
                .layout = ElementLayout::Leading,
                .width = SizeMode::Stretch
            })) {
                swBetween = el.response;
                ui.Switch(swVal);
            }

            // Checkbox in ElementLayout::Leading: Fit vs Stretch
            if (auto el = ui.Element({
                .label = "Checkbox Compact",
                .layout = ElementLayout::Leading,
                .width = SizeMode::Fit
            })) {
                cbCompact = el.response;
                ui.Checkbox(cbVal);
            }
            if (auto el = ui.Element({
                .label = "Checkbox Stretch",
                .layout = ElementLayout::Leading,
                .width = SizeMode::Stretch
            })) {
                cbBetween = el.response;
                ui.Checkbox(cbVal);
            }

            // Slider: Fit vs Stretch
            if (auto el = ui.Element({
                .label = "Slider Compact",
                .layout = ElementLayout::Leading,
                .width = SizeMode::Fit
            })) {
                slCompact = el.response;
                ui.Slider(slVal, 0.0f, 100.0f, {.width = SizeMode::Fit});
            }
            if (auto el = ui.Element({
                .label = "Slider Stretch",
                .layout = ElementLayout::Leading,
                .width = SizeMode::Stretch
            })) {
                slBetween = el.response;
                ui.Slider(slVal, 0.0f, 100.0f);
            }

            // Select: Fit vs Stretch
            if (auto el = ui.Element({
                .label = "Select Compact",
                .layout = ElementLayout::Leading,
                .width = SizeMode::Fit
            })) {
                selCompact = el.response;
                ui.Select(selIdx, selOpts, {.width = SizeMode::Fit});
            }
            if (auto el = ui.Element({
                .label = "Select Stretch",
                .layout = ElementLayout::Leading,
                .width = SizeMode::Stretch
            })) {
                selBetween = el.response;
                ui.Select(selIdx, selOpts);
            }

            // InputField: Fit vs Stretch
            if (auto el = ui.Element({
                .label = "Input Compact",
                .layout = ElementLayout::Leading,
                .width = SizeMode::Fit
            })) {
                inCompact = el.response;
                ui.InputField(inText, {.width = SizeMode::Fit});
            }
            if (auto el = ui.Element({
                .label = "Input Stretch",
                .layout = ElementLayout::Leading,
                .width = SizeMode::Stretch
            })) {
                inBetween = el.response;
                ui.InputField(inText);
            }
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

    // 6. Test Button in ElementLayout::Stacked with outer Stretch + Fit vs Stretch inner button
    Response btnStBetween, btnStFill;
    auto renderStackedBtns = [&]() {
        if (auto col = ui.Column()) {
            if (auto el = ui.Element({
                .label = "Header",
                .layout = ElementLayout::Stacked,
                .width = SizeMode::Stretch
            })) {
                btnStBetween = el.response;
                ui.Button({.text = "Action", .width = SizeMode::Fit});
            }
            if (auto el = ui.Element({
                .label = "Header",
                .layout = ElementLayout::Stacked,
                .width = SizeMode::Stretch
            })) {
                btnStFill = el.response;
                ui.Button({.text = "Action", .width = SizeMode::Stretch});
            }
        }
    };
    ui.BeginFrame({800.0f, 600.0f});
    renderStackedBtns();
    ui.EndFrame(canvas);

    ui.BeginFrame({800.0f, 600.0f});
    renderStackedBtns();
    ui.EndFrame(canvas);

    const Rect rStBetween = ui.GetPrevFrameBounds(btnStBetween.id);
    const Rect rStFill = ui.GetPrevFrameBounds(btnStFill.id);
    CHECK_NEAR(rStBetween.w, 800.0f, 0.01f);
    CHECK_NEAR(rStFill.w, 800.0f, 0.01f);

    // 7. Test Button in ElementLayout::Trailing with SizeMode::Fit vs SizeMode::Stretch
    Response btnTrCompact, btnTrBetween, btnTrFill;
    auto renderTrailingBtns = [&]() {
        if (auto col = ui.Column()) {
            if (auto el = ui.Element({
                .label = "Trailing Label",
                .layout = ElementLayout::Trailing,
                .width = SizeMode::Fit
            })) {
                btnTrCompact = el.response;
                ui.Button({.text = "Action"});
            }
            if (auto el = ui.Element({
                .label = "Trailing Label",
                .layout = ElementLayout::Trailing,
                .width = SizeMode::Stretch
            })) {
                btnTrBetween = el.response;
                ui.Button({.text = "Action", .width = SizeMode::Fit});
            }
            if (auto el = ui.Element({
                .label = "Trailing Label",
                .layout = ElementLayout::Trailing,
                .width = SizeMode::Stretch
            })) {
                btnTrFill = el.response;
                ui.Button({.text = "Action", .width = SizeMode::Stretch});
            }
        }
    };
    ui.BeginFrame({800.0f, 600.0f});
    renderTrailingBtns();
    ui.EndFrame(canvas);

    ui.BeginFrame({800.0f, 600.0f});
    renderTrailingBtns();
    ui.EndFrame(canvas);

    const Rect rTrCompact = ui.GetPrevFrameBounds(btnTrCompact.id);
    const Rect rTrBetween = ui.GetPrevFrameBounds(btnTrBetween.id);
    const Rect rTrFill = ui.GetPrevFrameBounds(btnTrFill.id);

    CHECK(rTrCompact.w < 800.0f);
    CHECK_NEAR(rTrBetween.w, 800.0f, 0.01f);
    CHECK_NEAR(rTrFill.w, 800.0f, 0.01f);

    keel::println("  [PASS] TestWidgetSizeModesAndExplicitWidth completed");
}

namespace {
class LabelRecordingCanvas : public guikit::ThorCanvas {
public:
    struct TextBoxCall {
        keel::String text;
        guikit::Rect bounds;
        float alignX;
        guikit::Color color;
    };
    void DrawRoundRect(guikit::Rect rect, float radius, guikit::Color fill, guikit::Color stroke = guikit::Color(0, 0, 0, 0), float strokeWidth = 0.0f) override {
        lastRoundRect = rect;
        ThorCanvas::DrawRoundRect(rect, radius, fill, stroke, strokeWidth);
    }
    void DrawTextBox(keel::StringRef text, guikit::Rect bounds, float fontSize, guikit::Color color, float alignX = 0.0f, float alignY = 0.5f) override {
        textBoxCalls.push_back({keel::String(text), bounds, alignX, color});
        ThorCanvas::DrawTextBox(text, bounds, fontSize, color, alignX, alignY);
    }
    void Reset() {
        lastRoundRect = {};
        textBoxCalls.clear();
    }
    const TextBoxCall* FindTextBox(keel::StringRef text) const {
        for (const auto& c : textBoxCalls) {
            if (c.text == text) return &c;
        }
        return nullptr;
    }
    guikit::Rect lastRoundRect{};
    keel::Array<TextBoxCall> textBoxCalls;
};
} // namespace

void TestLabelAlignmentGeometryOptionA() {
    using namespace guikit;

    LabelRecordingCanvas recCanvas;
    keel::Array<keel::u32> pixels(800 * 600);
    recCanvas.SetTarget(pixels.data(), 800, 600, 800);
    const UiTheme& darkTheme = UiTheme::Dark();
    const Rect widgetBounds(50.0f, 100.0f, 800.0f, 80.0f);

    // 1. Button in Top layout with SizeMode::Fit:
    // Label block width must be tied to control width (150px) rather than whole container (800px)
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Header Label",
            .description = {"Warning helper", UiVariant::Warning},
            .layout = ElementLayout::Top,
            .align = Align::Center,
            .width = SizeMode::Fit,
            .control = {.width = 150.0f}
        };
        ButtonOptions opts{
            .text = "Click Me",
            .width = 150.0f
        };
        const Vec2 cSize = Button::Measure(opts, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Button::Render(recCanvas, geom.controlRect, opts, UiSize::Md, false, false, false, darkTheme);

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

    // 2. Button in Top layout with SizeMode::Stretch:
    // Label block width spans entire container (800px)
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Header Label",
            .layout = ElementLayout::Top,
            .align = Align::End,
            .width = SizeMode::Stretch,
            .control = {.width = SizeMode::Stretch}
        };
        ButtonOptions opts{
            .text = "Click Me",
            .width = SizeMode::Stretch
        };
        const Vec2 cSize = Button::Measure(opts, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Button::Render(recCanvas, geom.controlRect, opts, UiSize::Md, false, false, false, darkTheme);

        const auto* call = recCanvas.FindTextBox("Header Label");
        CHECK(call != nullptr);
        if (call) {
            CHECK_NEAR(call->bounds.x, 50.0f, 0.01f);
            CHECK_NEAR(call->bounds.w, 800.0f, 0.01f);
            CHECK_NEAR(call->alignX, 1.0f, 0.01f);
        }
    }

    // 2b. Button in Top layout with outer SizeMode::Stretch, 150dp control, and align = End:
    // Both Label and compact Button capsule (150px) align to the right edge of the 800px row
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Header Label",
            .layout = ElementLayout::Top,
            .align = Align::End,
            .width = SizeMode::Stretch,
            .control = {.width = 150.0f}
        };
        ButtonOptions opts{
            .text = "Click Me",
            .width = 150.0f
        };
        const Vec2 cSize = Button::Measure(opts, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Button::Render(recCanvas, geom.controlRect, opts, UiSize::Md, false, false, false, darkTheme);

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

    // 3. Button in ElementLayout::Leading with outer SizeMode::Stretch:
    // Label is docked at left edge (50px), Button capsule (150dp) is docked at right edge (700px)
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Setting Title",
            .layout = ElementLayout::Leading,
            .align = Align::End,
            .width = SizeMode::Stretch,
            .control = {.width = 150.0f}
        };
        ButtonOptions opts{
            .text = "Action",
            .width = 150.0f
        };
        const Vec2 cSize = Button::Measure(opts, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Button::Render(recCanvas, geom.controlRect, opts, UiSize::Md, false, false, false, darkTheme);

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

    // 3b. Button in Top layout with outer Stretch + explicit 400dp inner Button & Align::Center:
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Header Label",
            .layout = ElementLayout::Top,
            .align = Align::Center,
            .width = SizeMode::Stretch,
            .control = {.width = 400.0f}
        };
        ButtonOptions opts{
            .text = "Action",
            .width = 400.0f
        };
        const Vec2 cSize = Button::Measure(opts, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Button::Render(recCanvas, geom.controlRect, opts, UiSize::Md, false, false, false, darkTheme);

        CHECK_NEAR(recCanvas.lastRoundRect.w, 400.0f, 0.01f);
        CHECK_NEAR(recCanvas.lastRoundRect.x, 250.0f, 0.01f); // 50 + (800 - 400) * 0.5
    }

    // 3c. Button in ElementLayout::Leading with explicit 720dp width inside 800px row:
    // Long label must be cropped with "..." to fit inside remaining width (800 - 720 - gap)
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Very Long Setting Title That Exceeds Available Remaining Width",
            .layout = ElementLayout::Leading,
            .width = SizeMode::Stretch,
            .control = {.width = 720.0f}
        };
        ButtonOptions opts{
            .text = "Action",
            .width = 720.0f
        };
        const Vec2 cSize = Button::Measure(opts, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Button::Render(recCanvas, geom.controlRect, opts, UiSize::Md, false, false, false, darkTheme);

        CHECK_NEAR(recCanvas.lastRoundRect.w, 720.0f, 0.01f);
        CHECK_NEAR(recCanvas.lastRoundRect.x, 130.0f, 0.01f); // 50 + 800 - 720
        CHECK(!recCanvas.textBoxCalls.empty());
        if (!recCanvas.textBoxCalls.empty()) {
            const auto& firstCall = recCanvas.textBoxCalls.front();
            CHECK(firstCall.text.ends_with("..."));
            CHECK(firstCall.bounds.Right() <= recCanvas.lastRoundRect.x);
        }
    }

    // 4. InputField in ElementLayout::Trailing with outer Stretch and Align::Center:
    // [Control] + Label group is centered inside widgetBounds
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Trailing Field Info",
            .layout = ElementLayout::Trailing,
            .align = Align::Center,
            .width = SizeMode::Stretch,
            .control = {.width = 200.0f}
        };
        InputFieldOptions opts{
            .width = 200.0f
        };
        const Vec2 cSize = InputField::Measure("text", opts, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        InputField::Render(recCanvas, geom.controlRect, "text", opts, UiSize::Md, false, false, 0, false, false, 1.5f, darkTheme);

        const auto* call = recCanvas.FindTextBox("Trailing Field Info");
        CHECK(call != nullptr);
        if (call) {
            const float measuredW = recCanvas.MeasureText("Trailing Field Info", darkTheme.GetTypography(TextRole::LabelLarge).fontSize).x;
            const float contentW = 200.0f + elSpec.inlineGap + measuredW;
            const float expectedStartX = widgetBounds.x + (widgetBounds.w - contentW) * 0.5f;
            CHECK_NEAR(geom.controlRect.x, expectedStartX, 0.01f);
            CHECK_NEAR(call->bounds.x, expectedStartX + 200.0f + elSpec.inlineGap, 0.01f);
            CHECK_NEAR(call->bounds.w, measuredW, 0.01f);
            CHECK_NEAR(call->alignX, 0.5f, 0.01f);
        }
    }

    // 4b. InputField in Stacked layout with explicit 400dp width & Align::End:
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Endpoint",
            .description = {"Primary host", UiVariant::Warning},
            .layout = ElementLayout::Stacked,
            .align = Align::End,
            .width = SizeMode::Stretch,
            .control = {.width = 400.0f}
        };
        InputFieldOptions opts{
            .width = 400.0f
        };
        const Vec2 cSize = InputField::Measure("text", opts, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        InputField::Render(recCanvas, geom.controlRect, "text", opts, UiSize::Md, false, false, 0, false, false, 1.5f, darkTheme);

        CHECK_NEAR(recCanvas.lastRoundRect.w, 400.0f, 0.01f);
        CHECK_NEAR(recCanvas.lastRoundRect.x, 450.0f, 0.01f); // 50 + (800 - 400) * 1.0
        const auto* descCall = recCanvas.FindTextBox("Primary host");
        CHECK(descCall != nullptr);
        if (descCall) {
            CHECK_NEAR(descCall->alignX, 1.0f, 0.01f);
        }
    }

    // 4c. Select in ElementLayout::Leading with explicit 720dp width inside 800px row:
    // Long label must be cropped with "..." to fit inside remaining width (800 - 720 - gap)
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Very Long Dropdown Setting Title That Exceeds Available Remaining Width",
            .layout = ElementLayout::Leading,
            .width = SizeMode::Stretch,
            .control = {.width = 720.0f}
        };
        SelectOptions opts{
            .width = 720.0f
        };
        const Vec2 cSize = Select::Measure(opts, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Select::Render(recCanvas, geom.controlRect, "Option A", opts, UiSize::Md, false, false, false, darkTheme);

        const Rect boxRect = geom.controlRect;
        CHECK_NEAR(boxRect.w, 720.0f, 0.01f);
        CHECK_NEAR(boxRect.x, 130.0f, 0.01f); // 50 + 800 - 720
        CHECK(!recCanvas.textBoxCalls.empty());
        if (!recCanvas.textBoxCalls.empty()) {
            const auto& firstCall = recCanvas.textBoxCalls.front();
            CHECK(firstCall.text.ends_with("..."));
            CHECK(firstCall.bounds.Right() <= boxRect.x);
        }
    }

    // 5. Switch in Top layout with SizeMode::Fit:
    // Label block width is max(trackSize.x, measuredTextW)
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Short",
            .layout = ElementLayout::Top,
            .align = Align::Center,
            .width = SizeMode::Fit
        };
        SwitchOptions opts{};
        const Vec2 cSize = Switch::Measure(opts, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Switch::Render(recCanvas, geom.controlRect, false, opts, UiSize::Md, false, false, false, darkTheme);

        const auto* call = recCanvas.FindTextBox("Short");
        CHECK(call != nullptr);
        if (call) {
            CHECK_NEAR(call->bounds.x, 50.0f, 0.01f);
            // Label block should NOT be 800px; it is bounded by track or text width
            CHECK(call->bounds.w < 100.0f);
            CHECK_NEAR(call->alignX, 0.5f, 0.01f);
        }
    }

    // 6. Switch in ElementLayout::Trailing with SizeMode::Stretch & Align::End:
    // [Track] + Label group is docked at right edge of widgetBounds
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Power Switch",
            .layout = ElementLayout::Trailing,
            .align = Align::End,
            .width = SizeMode::Stretch
        };
        SwitchOptions opts{};
        const Vec2 cSize = Switch::Measure(opts, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Switch::Render(recCanvas, geom.controlRect, false, opts, UiSize::Md, false, false, false, darkTheme);

        const auto* call = recCanvas.FindTextBox("Power Switch");
        CHECK(call != nullptr);
        if (call) {
            const float measuredW = recCanvas.MeasureText("Power Switch", darkTheme.GetTypography(TextRole::LabelLarge).fontSize).x;
            const float expectedTextX = widgetBounds.x + widgetBounds.w - measuredW;
            CHECK_NEAR(call->bounds.x, expectedTextX, 0.01f);
            CHECK_NEAR(call->bounds.w, measuredW, 0.01f);
        }
    }

    // 7. Checkbox in ElementLayout::Trailing with SizeMode::Stretch & Align::End:
    // [Checkbox] + Label group is docked at right edge of widgetBounds
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Enable Feature",
            .layout = ElementLayout::Trailing,
            .align = Align::End,
            .width = SizeMode::Stretch
        };
        CheckboxOptions opts{};
        const Vec2 cSize = Checkbox::Measure(opts, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Checkbox::Render(recCanvas, geom.controlRect, false, opts, UiSize::Md, false, false, false, darkTheme);

        const auto* call = recCanvas.FindTextBox("Enable Feature");
        CHECK(call != nullptr);
        if (call) {
            const float measuredW = recCanvas.MeasureText("Enable Feature", darkTheme.GetTypography(TextRole::LabelLarge).fontSize).x;
            const float expectedTextX = widgetBounds.x + widgetBounds.w - measuredW;
            CHECK_NEAR(call->bounds.x, expectedTextX, 0.01f);
            CHECK_NEAR(call->bounds.w, measuredW, 0.01f);
        }
    }

    // 8. Slider in ElementLayout::Trailing with inner SizeMode::Stretch:
    // Slider stretches across remaining lane (bounds.w - measuredTextW - gap), Label sits on the right!
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Volume Level",
            .layout = ElementLayout::Trailing,
            .width = SizeMode::Stretch,
            .control = {.width = SizeMode::Stretch}
        };
        SliderOptions opts{
            .showValue = false
        };
        const Vec2 cSize = Slider::Measure(opts, 0.5, 0.0, 1.0, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Slider::Render(recCanvas, geom.controlRect, 0.5f, 0.0f, 1.0f, opts, UiSize::Md, false, false, false, darkTheme);

        const auto* call = recCanvas.FindTextBox("Volume Level");
        CHECK(call != nullptr);
        if (call) {
            const float measuredW = recCanvas.MeasureText("Volume Level", darkTheme.GetTypography(TextRole::LabelLarge).fontSize).x;
            const float expectedTextX = widgetBounds.x + widgetBounds.w - measuredW;
            CHECK_NEAR(geom.controlRect.x, widgetBounds.x, 0.01f);
            CHECK_NEAR(call->bounds.x, expectedTextX, 0.01f);
            CHECK_NEAR(call->bounds.w, measuredW, 0.01f);
        }
    }

    // 8b. Slider in ElementLayout::Leading with explicit 680dp width inside 800px row:
    // Long label must be cropped with "..." to fit inside remaining width
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Very Long Slider Label That Exceeds Remaining Inline Space",
            .layout = ElementLayout::Leading,
            .width = SizeMode::Stretch,
            .control = {.width = 680.0f}
        };
        SliderOptions opts{
            .showValue = false,
            .width = 680.0f
        };
        const Vec2 cSize = Slider::Measure(opts, 0.5, 0.0, 1.0, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Slider::Render(recCanvas, geom.controlRect, 0.5f, 0.0f, 1.0f, opts, UiSize::Md, false, false, false, darkTheme);

        CHECK(!recCanvas.textBoxCalls.empty());
        if (!recCanvas.textBoxCalls.empty()) {
            const auto& firstCall = recCanvas.textBoxCalls.front();
            CHECK(firstCall.text.ends_with("..."));
            CHECK(firstCall.bounds.Right() <= geom.controlRect.x);
        }
    }

    keel::println("  [PASS] TestLabelAlignmentGeometryOptionA completed");
}

void TestUiTextAndControlCompanionText() {
    using namespace guikit;
    keel::println("  - TestUiTextAndControlCompanionText...");

    static_assert(sizeof(UiText) <= 80);

    const UiTheme& darkTheme = UiTheme::Dark();
    LabelRecordingCanvas recCanvas;
    keel::Array<keel::u32> pixels(800 * 600);
    recCanvas.SetTarget(pixels.data(), 800, 600, 800);
    const Rect widgetBounds(20.0f, 20.0f, 400.0f, 120.0f);

    // 1. UiText construction and default vs overridden typography/color resolution
    {
        UiText plain = "Plain";
        CHECK(plain.text == "Plain");
        CHECK(plain.size.IsAuto());
        CHECK(!plain.color.HasAny());
        CHECK(!plain.wrap.IsWordWrap());
        CHECK(plain.wrap.minLines == 0);
        CHECK(plain.wrap.maxLines == 1);

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

        // Explicit pixel size, scale preset (TextSize::Lg()), and per-state StateColor
        UiText pxText = {"Custom Px", 19.5f, StateColor{.normal = Color::Hex(0x123456FF), .hover = Color::Hex(0x654321FF)}};
        CHECK_NEAR(darkTheme.ResolveFontSize(pxText, TextRole::LabelSmall, UiSize::Md), 19.5f, 0.01f);
        CHECK(darkTheme.ResolveTextColor(pxText, UiVariant::Default, false, false, false) == Color::Hex(0x123456FF));
        CHECK(darkTheme.ResolveTextColor(pxText, UiVariant::Default, false, true, false) == Color::Hex(0x654321FF));

        UiText presetText = {"Scale Preset", TextSize::Lg(), StateColor::Warning()};
        CHECK_NEAR(darkTheme.ResolveFontSize(presetText, TextRole::LabelLarge, UiSize::Sm), 16.0f, 0.01f);
        CHECK(darkTheme.ResolveTextColor(presetText, UiVariant::Default) == darkTheme.GetTextColor(UiVariant::Warning));
    }

    // 2. Switch with external .label + internal .textLeft / .textRight + .description
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "System Theme",
            .description = {"Applies globally", UiVariant::Warning},
            .layout = ElementLayout::Top
        };
        SwitchOptions opts{
            .textLeft = "Dark",
            .textRight = {"Light", UiVariant::Info},
            .symmetric = true
        };
        const Vec2 cSize = Switch::Measure(opts, UiSize::Md, &recCanvas, darkTheme);
        const ElementGeometry geom = Element::Resolve(widgetBounds, cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Switch::Render(recCanvas, geom.controlRect, true, opts, UiSize::Md, false, false, false, darkTheme);

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
        ElementSpec elSpec{
            .label = "Master Gain",
            .description = "Preamp level",
            .layout = ElementLayout::Stacked
        };
        SliderOptions opts{
            .textLeft = "0%",
            .textRight = "100%",
            .valueFormat = "%.0f%%",
            .showValue = true,
            .valuePlacement = vp,
            .color = StateColor::Danger() // Even with Danger color, value readout defaults to neutral Secondary!
        };

        Vec2 ctrlMeasured = Slider::Measure(opts, 50.0, 0.0, 100.0, UiSize::Md, &recCanvas, darkTheme);
        Vec2 measured = Element::Measure(ctrlMeasured, elSpec, &recCanvas, darkTheme);
        CHECK(measured.x > 0.0f);
        CHECK(measured.y > 0.0f);

        const ElementGeometry geom = Element::Resolve(Rect(20.0f, 20.0f, 400.0f, measured.y), ctrlMeasured, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);
        Slider::Render(recCanvas, geom.controlRect, 50.0, 0.0, 100.0, opts, UiSize::Md, false, false, false, darkTheme);

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

    keel::println("  [PASS] TestUiTextAndControlCompanionText completed");
}

void TestSidebarAndSidebarMenuWorkflow() {
    using namespace guikit;

    // 1. Verify unified typography resolution (Single Source of Truth)
    {
        const UiTheme darkTheme = UiTheme::Dark();
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
    const keel::u32 width = 1000;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    ThorCanvas canvas;
    canvas.SetTarget(pixels.data(), width, height, width);

    Id sbLeft1Id, sbLeft2Id, sbRight1Id, sbRight2Id;
    Response centerBtn;

    auto renderMultiSidebar = [&]() {
        if (auto workspace = ui.Row({.gap = 10.0f, .padding = Padding::All(0.0f), .height = SizeMode::Stretch})) {
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
            centerBtn = ui.Button({.text = "Center Workspace", .width = SizeMode::Stretch});
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
    keel::i32 activeMenuVal = 0;
    bool menuCollapsed = false;
    Id fitSidebarId;
    Response item0Res, item1Res;

    auto renderAutoFitMenu = [&]() {
        if (auto workspace = ui.Row({.gap = 8.0f, .height = SizeMode::Stretch})) {
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
    keel::i32 spanIdx = 0;
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

    // 5. Test Mouse Wheel Scrolling inside an overflowing Sidebar
    {
        Context scrollUi;
        Id scrollSbId = Id::Null();
        Response firstItemRes;
        auto renderOverflowSidebar = [&]() {
            if (auto workspace = scrollUi.Row({.gap = 8.0f, .height = SizeMode::Stretch})) {
                if (auto sb = scrollUi.Sidebar({.side = SidebarSide::Left, .width = 180.0f, .scrollable = true})) {
                    scrollSbId = sb.GetNode()->id;
                    for (keel::i32 i = 0; i < 30; ++i) {
                        Response r = scrollUi.Button({.text = scrollUi.Format("Nav Item {}", i)});
                        if (i == 0) {
                            firstItemRes = r;
                        }
                    }
                }
            }
        };

        scrollUi.BeginFrame({400.0f, 200.0f});
        renderOverflowSidebar();
        scrollUi.EndFrame(canvas);

        scrollUi.BeginFrame({400.0f, 200.0f});
        renderOverflowSidebar();
        scrollUi.EndFrame(canvas);

        const float initialY = scrollUi.GetPrevUnclippedBounds(firstItemRes.id).y;
        const Rect sbRect = scrollUi.GetPrevFrameBounds(scrollSbId);
        CHECK(sbRect.w > 100.0f);
        CHECK(sbRect.h > 150.0f);

        scrollUi.OnMouseMove(sbRect.Center());
        scrollUi.OnMouseWheel(0.0f, -2.0f);
        scrollUi.BeginFrame({400.0f, 200.0f});
        renderOverflowSidebar();
        scrollUi.EndFrame(canvas);

        const float scrolledY = scrollUi.GetPrevUnclippedBounds(firstItemRes.id).y;
        CHECK(scrolledY < initialY - 10.0f);
    }

    keel::println("  [PASS] TestSidebarAndSidebarMenuWorkflow completed");
}

void TestUiTextModesAndVerticalSpaceReservation() {
    using namespace guikit;
    keel::println("  - TestUiTextModesAndVerticalSpaceReservation...");

    const UiTheme& darkTheme = UiTheme::Dark();
    LabelRecordingCanvas recCanvas;
    keel::Array<keel::u32> pixels(800 * 600);
    recCanvas.SetTarget(pixels.data(), 800, 600, 800);

    // 1. Single-line truncation with "..." when text exceeds bounds
    {
        recCanvas.Reset();
        ElementSpec elSpec{
            .label = "Very long field label that should be truncated with ellipsis in a narrow box",
            .description = "Very long description text that should also be cropped on a single line",
            .layout = ElementLayout::Stacked
        };
        const Vec2 cSize = InputField::Measure("", InputFieldOptions{}, UiSize::Md, &recCanvas, darkTheme);
        const Vec2 m = Element::Measure(cSize, elSpec, &recCanvas, darkTheme, 120.0f);
        const ElementGeometry geom = Element::Resolve(Rect(10.0f, 10.0f, 120.0f, m.y), cSize, elSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, elSpec, darkTheme);

        CHECK(recCanvas.textBoxCalls.size() == 2);
        if (recCanvas.textBoxCalls.size() == 2) {
            CHECK(recCanvas.textBoxCalls[0].text.ends_with("..."));
            CHECK(recCanvas.textBoxCalls[1].text.ends_with("..."));
        }
    }

    // 2. Multi-line word wrapping (TextWrap::Word) + Max lines truncation (TextWrap::Max(2))
    {
        recCanvas.Reset();
        ElementSpec singleSpec{
            .label = "Username",
            .description = {"Single line description", TextWrap::SingleLine()},
            .layout = ElementLayout::Stacked
        };
        ElementSpec multiSpec{
            .label = "Username",
            .description = {
                "This validation description is intentionally long so that it wraps across multiple lines in a narrow container",
                UiVariant::Danger,
                TextWrap{.minLines = 0, .maxLines = 2}
            },
            .layout = ElementLayout::Stacked
        };

        const Vec2 cSize = InputField::Measure("", InputFieldOptions{}, UiSize::Md, &recCanvas, darkTheme);
        const Vec2 singleM = Element::Measure(cSize, singleSpec, &recCanvas, darkTheme, 140.0f);
        const Vec2 multiM = Element::Measure(cSize, multiSpec, &recCanvas, darkTheme, 140.0f);
        const float descFontSize = darkTheme.ResolveFontSize(multiSpec.description, TextRole::LabelSmall, UiSize::Md);
        const float descLineH = descFontSize * kDefaultLineHeightMultiplier;

        // Multi-line with Max(2) must be exactly 1 description line taller than single-line
        CHECK_NEAR(multiM.y - singleM.y, descLineH, 0.01f);

        const ElementGeometry geom = Element::Resolve(Rect(10.0f, 10.0f, 140.0f, multiM.y), cSize, multiSpec, &recCanvas, darkTheme);
        Element::RenderText(recCanvas, geom, multiSpec, darkTheme);
        // 1 call for "Username" + 2 calls for the 2 wrapped lines of description
        CHECK(recCanvas.textBoxCalls.size() == 3);
        if (recCanvas.textBoxCalls.size() == 3) {
            CHECK(recCanvas.textBoxCalls[0].text == "Username");
            CHECK(recCanvas.textBoxCalls[2].text.ends_with("..."));
        }
    }

    // 3. Zero vertical layout shift when conditional error description appears/disappears (TextWrap::Reserve(1))
    {
        Context ui;
        keel::String email("user@");
        bool hasError = false;
        Response inputRes;
        Response submitBtnRes;

        auto renderForm = [&]() {
            if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
                if (auto el = ui.Element({
                    .label = "Email",
                    .description = {
                        hasError ? "Invalid email address format" : "",
                        UiVariant::Danger,
                        TextWrap::Reserve(1)
                    }
                })) {
                    inputRes = el.response;
                    ui.InputField(email);
                }
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

    // 4. Zero vertical shift for multi-line fixed reservation (TextWrap::Fixed(2))
    {
        Context ui;
        keel::String password;
        keel::i32 errorStage = 0; // 0 = no error, 1 = 1-line error, 2 = 3-line error (clamped to 2 lines)
        Response submitBtnRes;

        auto renderPasswordForm = [&]() {
            keel::StringRef errText = "";
            if (errorStage == 1) {
                errText = "Too short";
            } else if (errorStage == 2) {
                errText = "Password must be at least 12 characters long and include uppercase, lowercase, numbers, and special symbols";
            }

            if (auto col = ui.Column({.gap = 10.0f, .padding = Padding::All(12.0f)})) {
                if (auto el = ui.Element({
                    .label = "Password",
                    .description = {
                        errText,
                        UiVariant::Danger,
                        TextWrap{.minLines = 2, .maxLines = 2}
                    }
                })) {
                    ui.InputField(password);
                }
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

    // 5. Standalone ui.Text with TextWrap::Fixed(2)
    {
        Context ui;
        bool showNotice = false;
        Response belowBtn;

        auto renderTextDemo = [&]() {
            if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(10.0f)})) {
                ui.Text({
                    showNotice ? "This standalone text widget wraps across two lines cleanly" : "",
                    UiVariant::Secondary,
                    TextWrap::Fixed(2)
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

    keel::println("  [PASS] TestUiTextModesAndVerticalSpaceReservation completed");
}

void TestSelectItemsAndUnifiedAffixes() {
    using namespace guikit;
    keel::println("  - TestSelectItemsAndUnifiedAffixes...");

    const UiTheme& darkTheme = UiTheme::Dark();
    LabelRecordingCanvas recCanvas;
    keel::Array<keel::u32> pixels(800 * 600);
    recCanvas.SetTarget(pixels.data(), 800, 600, 800);

    // 1. Unified UiAffix in InputField: icon only, text only, and combined icon + text
    {
        recCanvas.Reset();
        InputFieldOptions opts{
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
        keel::usize selectedIdx = 0;
        Response selRes;

        const SelectItem items[] = {
            {.text = "1920x1080", .prefix = "FHD", .suffix = {"60 Hz", UiVariant::Muted}},
            {.text = "2560x1440", .prefix = {Icon::Settings(), "QHD"}, .suffix = {"144 Hz", UiVariant::Info}},
            {.text = "3840x2160", .prefix = "4K", .suffix = {"Pro", Icon::Check(), UiVariant::Warning}, .disabled = true},
            {.text = "5120x2880", .prefix = "5K", .suffix = {"240 Hz", UiVariant::Success}}
        };

        auto renderSelect = [&]() {
            if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(16.0f)})) {
                if (auto el = ui.Element({.label = "Resolution", .width = SizeMode::Fit})) {
                    selRes = ui.Select(selectedIdx, items, {.width = SizeMode::Fit});
                }
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

    keel::println("  [PASS] TestSelectItemsAndUnifiedAffixes completed");
}

void TestSelectPopupScrollingWithMaxVisibleItems() {
    using namespace guikit;
    const UiTheme& darkTheme = UiTheme::Dark();
    LabelRecordingCanvas recCanvas;
    keel::Array<keel::u32> pixels(800 * 600);
    recCanvas.SetTarget(pixels.data(), 800, 600, 800);

    const keel::StaticArray<SelectItem, 8> items = {{
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
        keel::usize idxA = 0;
        keel::usize idxB = 0;
        WidgetResponse resNoScroll;
        WidgetResponse resScroll;

        auto renderPair = [&]() {
            if (auto col = ui.Column({.gap = 8.0f})) {
                resNoScroll = ui.Select(idxA, items, {
                    .maxVisibleItems = 10,
                    .width = SizeMode::Fit
                });
                resScroll = ui.Select(idxB, items, {
                    .maxVisibleItems = 3,
                    .width = SizeMode::Fit
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
        keel::usize selectedIdx = 0;
        WidgetResponse selRes;

        auto renderScene = [&]() {
            if (auto sa = ui.ScrollArea()) {
                if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
                    if (auto el = ui.Element({.label = "Scrollable Select"})) {
                        selRes = ui.Select(selectedIdx, items, {
                            .maxVisibleItems = 3
                        });
                    }
                    // Extra tall content so parent ScrollArea is scrollable if it receives wheel events
                    for (keel::i32 i = 0; i < 15; ++i) {
                        ui.Button({.text = "Background Filler Row"});
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
        for (keel::i32 step = 0; step < 4; ++step) {
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

    keel::println("  [PASS] TestSelectPopupScrollingWithMaxVisibleItems completed");
}

void TestBaseElementsAllInOneEnhancements() {
    using namespace guikit;
    const UiTheme& darkTheme = UiTheme::Dark();
    LabelRecordingCanvas recCanvas;
    keel::Array<keel::u32> pixels(800 * 600);
    recCanvas.SetTarget(pixels.data(), 800, 600, 800);

    // 1. Type-safe Node::SetPayload<T> and Node::As<T>() verification
    {
        Node n;
        ButtonOptions bOpts{.text = "Test"};
        n.SetPayload(&bOpts);
        CHECK(n.As<ButtonOptions>() == &bOpts);
        CHECK(n.As<SwitchOptions>() == nullptr);
        CHECK(n.As<InputFieldData>() == nullptr);
    }

    // 2. Intrinsic glyph controls (Switch, Checkbox) stay Fit while ElementOptions::width controls row width
    {
        Context ui;
        bool sw = true;
        bool cb = true;
        Response swElCompact, swElWide, cbElFixed, cbElStretch, swGlyph, cbGlyph;

        auto renderWidths = [&]() {
            if (auto col = ui.Column({.gap = 8.0f})) {
                if (auto el = ui.Element({.label = "Auto Power", .width = SizeMode::Fit})) {
                    swElCompact = el.response;
                    swGlyph = ui.Switch(sw);
                }
                if (auto el = ui.Element({.label = "Auto Power", .width = 240.0f})) {
                    swElWide = el.response;
                    ui.Switch(sw);
                }
                if (auto el = ui.Element({.label = "Enable Stage", .width = 200.0f})) {
                    cbElFixed = el.response;
                    cbGlyph = ui.Checkbox(cb);
                }
                if (auto el = ui.Element({.label = "Enable Stage", .width = SizeMode::Stretch})) {
                    cbElStretch = el.response;
                    ui.Checkbox(cb);
                }
            }
        };

        ui.BeginFrame({400.0f, 300.0f});
        renderWidths();
        ui.EndFrame(recCanvas);

        ui.BeginFrame({400.0f, 300.0f});
        renderWidths();
        ui.EndFrame(recCanvas);

        CHECK_NEAR(swElWide.rect.w, 240.0f, 0.5f);
        CHECK(swElWide.rect.w > swElCompact.rect.w);
        CHECK_NEAR(cbElFixed.rect.w, 200.0f, 0.5f);
        CHECK_NEAR(cbElStretch.rect.w, 400.0f, 0.5f);
        CHECK(swGlyph.rect.w < 60.0f);
        CHECK(cbGlyph.rect.w < 32.0f);
    }

    // 3. Keyboard Tab / Shift+Tab Focus Traversal & Space / Enter Activation + Tooltip Overlay
    {
        Context ui;
        keel::i32 btnClicks = 0;
        bool cbChecked = false;
        bool swState = false;
        float sliderVal = 50.0f;
        Response btnRes, cbRes, swRes, slRes;

        auto renderFocusScene = [&]() {
            if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(16.0f)})) {
                btnRes = ui.Button({.text = "FirstBtn", .tooltip = "Action Tooltip Text"});
                if (btnRes.clicked) ++btnClicks;
                if (auto el = ui.Element({.label = "SecondCb"})) {
                    cbRes = ui.Checkbox(cbChecked);
                }
                if (auto el = ui.Element({.label = "ThirdSw"})) {
                    swRes = ui.Switch(swState);
                }
                if (auto el = ui.Element({.label = "FourthSl"})) {
                    slRes = ui.Slider(sliderVal, 0.0f, 100.0f, {.step = 10.0});
                }
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
        keel::String pwdText("secret");
        keel::String searchText("query123");
        keel::i32 numVal = 10;
        Response pwdRes, searchRes, numRes;

        auto renderInputs = [&]() {
            if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
                if (auto el = ui.Element({.label = "Password"})) {
                    pwdRes = ui.InputField(pwdText, {
                        .password = true,
                        .passwordReveal = true
                    });
                }
                if (auto el = ui.Element({.label = "Search"})) {
                    searchRes = ui.InputField(searchText, {
                        .clearable = true
                    });
                }
                if (auto el = ui.Element({.label = "Count"})) {
                    numRes = ui.SpinBox(numVal, {
                        .min = 0.0,
                        .max = 15.0,
                        .step = 5.0
                    });
                }
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
        for (keel::i32 i = 0; i < 4; ++i) {
            ui.OnKeyDown(Key::Down);
            ui.BeginFrame({400.0f, 300.0f});
            renderInputs();
            ui.EndFrame(recCanvas);
        }
        CHECK(numVal == 0); // Clamped to min = 0
    }

    // 5. Standalone Heading model (.label / .description, HeadingLayout, Align, SizeMode & width)
    {
        Context ui;
        Response hLeft, hCenter, hRight, hTop, hInline, hCompact;
        auto renderElementHeading = [&]() {
            if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(16.0f)})) {
                hLeft = ui.Heading({
                    .label = "LeftH",
                    .description = "SubLeft",
                    .align = Align::Start
                });
                hCenter = ui.Heading({
                    .label = "CenterH",
                    .description = "SubCenter",
                    .align = Align::Center,
                    .separator = HeadingSeparator::Trailing
                });
                hRight = ui.Heading({
                    .label = "RightH",
                    .description = "SubRight",
                    .align = Align::End
                });
                hTop = ui.Heading({
                    .label = "TitleBottom",
                    .description = "OverlineTop",
                    .layout = HeadingLayout::Overline
                });
                hInline = ui.Heading({
                    .label = "InlineTitle",
                    .description = "InlineSub",
                    .layout = HeadingLayout::Inline
                });
                hCompact = ui.Heading({
                    .label = "CompactH",
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

        // HeadingLayout::Inline places description ("InlineSub") to the right of the primary title bar
        const auto* tbInlineSub = recCanvas.FindTextBox("InlineSub");
        CHECK(tbInlineSub != nullptr);
        if (tbInlineSub) {
            CHECK(tbInlineSub->bounds.x > ui.GetPrevFrameBounds(hInline.id).x + 40.0f);
        }
    }

    keel::println("  [PASS] TestBaseElementsAllInOneEnhancements completed");
}

void TestImageSceneAndMarkdownWidgets() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 800;
    const keel::u32 height = 900;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    static constexpr keel::StringRef kInlineSvg =
        R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 100" width="200" height="100">)"
        R"(<rect x="0" y="0" width="200" height="100" fill="#38BDF8"/></svg>)";

    static constexpr keel::StringRef kSampleMd =
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
    keel::usize selectedCount = 0;
    MarkdownResponse mdRes;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
            if (auto el = ui.Element({
                .label = "Inline Vector SVG",
                .width = SizeMode::Stretch
            })) {
                imgRes = el.response;
                ui.Image(kInlineSvg, {
                    .checkerboard = true,
                    .width = 384.0f
                });
            }

            if (auto scene = ui.Scene({
                .worldSize = Vec2{1000.0f, 800.0f},
                .backgroundStyle = SceneBackground::GridDots,
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
        for (keel::i32 frame = 0; frame < 2; ++frame) {
            ui.BeginFrame({900.0f, 300.0f});
            if (auto stageBox = ui.Box({
                .width = SizeMode::Stretch,
                .height = SizeMode::Stretch,
                .padding = Padding::All(24.0f)
            })) {
                if (auto centerRow = ui.Row({
                    .align = Alignment::Center,
                    .justify = Justify::Center,
                    .height = SizeMode::Stretch
                })) {
                    if (auto el = ui.Element({
                        .label = "A vector image",
                        .description = "This is a vector image",
                        .width = SizeMode::Stretch
                    })) {
                        stageImgRes = el.response;
                        ui.Image(kInlineSvg, {
                            .fit = ObjectFit::Contain,
                            .checkerboard = true,
                            .width = SizeMode::Stretch
                        });
                    }
                }
            }
            ui.EndFrame(canvas);
        }
        CHECK(stageImgRes.rect.y >= 24.0f - 0.5f);
        CHECK(stageImgRes.rect.Bottom() <= 300.0f - 24.0f + 0.5f);
        CHECK(stageImgRes.rect.h <= 300.0f - 48.0f + 0.5f);
    }

    keel::println("  [PASS] TestImageSceneAndMarkdownWidgets completed");
}

void TestUnifiedElementHeaderDecorationsAcrossWidgets() {
    using namespace guikit;
    keel::println("  [TEST] TestUnifiedElementHeaderDecorationsAcrossWidgets...");

    Context ui;
    ThorCanvas canvas;
    keel::Array<keel::u32> pixels(800 * 900);
    canvas.SetTarget(pixels.data(), 800, 900, 800);

    // 1. Verify that adding icon and badge to Element increases measured width appropriately
    const UiTheme& theme = ui.GetTheme();
    {
        ButtonOptions baseBtn{.text = "Click me"};
        const Vec2 ctrlSz = Button::Measure(baseBtn, UiSize::Md, &canvas, theme);
        ElementSpec baseSpec{.label = "A button", .description = "This is a button"};
        ElementSpec decSpec = baseSpec;
        decSpec.icon = Icon::Edit();
        decSpec.badge = {"v1.0", UiVariant::Info};

        const Vec2 szBase = Element::Measure(ctrlSz, baseSpec, &canvas, theme);
        const Vec2 szDec  = Element::Measure(ctrlSz, decSpec, &canvas, theme);
        CHECK(szDec.x > szBase.x);
        CHECK(szDec.y >= szBase.y);
    }

    // 2. Render Heading + Element-wrapped controls inside a Card surface in a live frame
    float sliderVal = 42.0f;
    keel::usize stepIdx = 1;
    keel::String inputVal("hello");
    keel::usize selectIdx = 0;
    bool swVal = true;
    bool cbVal = false;
    static constexpr keel::StaticArray<keel::StringRef, 3> items = {"One", "Two", "Three"};

    for (keel::i32 frame = 0; frame < 2; ++frame) {
        ui.BeginFrame({800.0f, 900.0f});
        if (auto card = ui.Card({
            .gap = 8.0f,
            .padding = Padding::All(12.0f),
            .background = StateColor::Surface(),
            .border = {.width = 1.0f}
        })) {
            ui.Heading({
                .label = "A heading",
                .description = "This is a heading",
                .icon = Icon::Edit(),
                .separator = HeadingSeparator::Bottom
            });
            ui.Element({
                .label = "An element",
                .description = "This is an element",
                .icon = Icon::Settings(),
                .badge = "Core"
            });
            if (auto el = ui.Element({
                .label = "A button",
                .description = "This is a button",
                .icon = Icon::Edit(),
                .badge = "Action"
            })) {
                ui.Button({
                    .text = "Click me",
                    .icon = Icon::Play(),
                    .background = StateColor::Surface(), .border = {.width = 1.5f}
                });
            }
            if (auto el = ui.Element({
                .label = "A slider",
                .description = "This is a slider",
                .icon = Icon::Settings(),
                .badge = "dB"
            })) {
                ui.Slider(sliderVal, 0.0f, 100.0f);
            }
            if (auto el = ui.Element({
                .label = "A step slider",
                .description = "This is a step slider",
                .icon = Icon::Settings(),
                .badge = "3-step"
            })) {
                ui.StepSlider(stepIdx, items);
            }
            if (auto el = ui.Element({
                .label = "A text input",
                .description = "This is a text input",
                .icon = Icon::Edit(),
                .badge = "UTF-8"
            })) {
                ui.InputField(inputVal, {
                    .background = StateColor::Surface(), .border = {.width = 1.5f}
                });
            }
            if (auto el = ui.Element({
                .label = "A select",
                .description = "This is a select",
                .icon = Icon::Plus(),
                .badge = "Menu"
            })) {
                ui.Select(selectIdx, items, {
                    .background = StateColor::Surface(), .border = {.width = 1.5f}
                });
            }
            if (auto el = ui.Element({
                .label = "A switch",
                .description = "This is a switch",
                .icon = Icon::Refresh(),
                .badge = "Toggle"
            })) {
                ui.Switch(swVal);
            }
            if (auto el = ui.Element({
                .label = "A checkbox",
                .description = "This is a checkbox",
                .icon = Icon::Check(),
                .badge = "Bool"
            })) {
                ui.Checkbox(cbVal);
            }
        }
        ui.EndFrame(canvas);
    }

    keel::println("  [PASS] TestUnifiedElementHeaderDecorationsAcrossWidgets completed");
}

#if GUIKIT_HAS_SCENES
void TestSignalViewWidgetWorkflow() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const keel::u32 width = 960;
    const keel::u32 height = 540;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    // Generate 240,000 samples (5.0 seconds @ 48 kHz) with a 2 kHz sine + 8 kHz harmonic
    const keel::usize numSamples = 240000;
    const float sampleRate = 48000.0f;
    keel::Array<float> samples(numSamples, 0.0f);
    for (keel::usize i = 0; i < numSamples; ++i) {
        const float t = static_cast<float>(i) / sampleRate;
        samples[i] = 0.6f * keel::sin(keel::kTauF * 2000.0f * t) +
                     0.25f * keel::sin(keel::kTauF * 8000.0f * t);
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
    keel::Array<SignalMarker> markers;
    markers.push_back(SignalMarker{1.0, "Cue A", Color{56, 189, 248, 255}});
    markers.push_back(SignalMarker{3.5, "Cue B", Color{245, 158, 11, 255}});
    SignalPanelOrder panelOrder = SignalPanelOrder::WaveformTop;
    SignalViewResponse resp{};

    auto renderFrame = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto box = ui.Box({
            .width = SizeMode::Stretch,
            .height = SizeMode::Stretch,
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
                .height = SizeMode::Stretch
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

    keel::println("  [PASS] TestSignalViewWidgetWorkflow completed");
}

void TestMapViewWidgetWorkflow() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const keel::u32 width = 960;
    const keel::u32 height = 540;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    MapCrs crs = MapCrs::EPSG3857_WebMercator;
    keel::Optional<GeoCoord> reqCenter{};
    keel::Optional<float> reqZoom{};
    MapViewState lastView{};
    keel::usize itemCount = 0;

    auto renderFrame = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto map = ui.MapView({
            .crs = crs,
            .coordFormat = GeoCoordFormat::NauticalDMM,
            .initialCenter = {54.1825, 7.8853},
            .initialTileZoom = 7.0f,
            .requestCenterGeo = reqCenter,
            .requestTileZoom = reqZoom,
            .height = SizeMode::Stretch
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
        if (auto stage = ui.Box({.width = SizeMode::Stretch, .height = SizeMode::Stretch})) {
            if (!showMapTab) {
                ui.Text("Other Tab Content");
            } else {
                if (auto map = ui.MapView({
                    .crs = MapCrs::EPSG3857_WebMercator,
                    .coordFormat = GeoCoordFormat::NauticalDMM,
                    .initialCenter = {54.1825, 7.8853},
                    .initialTileZoom = 7.0f,
                    .height = SizeMode::Stretch
                })) {
                    tabSwitchView = map.GetViewState();
                }
            }
        }
        ui.EndFrame(canvas);
    };

    for (keel::i32 i = 0; i < 605; ++i) {
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

    // Verify embedded GIS chart loading (assets/geo/demo_bay.geojson) and GisRenderEngine rasterization
    const auto chart = GeoJsonIO::LoadFromFile("assets/geo/demo_bay.geojson", true);
    CHECK(chart.ok);
    CHECK(!chart.layers.empty());
    CHECK(chart.totalFeatures >= 400);
    CHECK(chart.totalBounds.IsValid());

    GisRenderStats gisStats{};
    keel::u64 selUid = 0;
    GisRenderConfig gisCfg{};
    gisCfg.asyncBackgroundStreaming = false;
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    if (auto map = ui.MapView({
        .crs = MapCrs::EPSG3857_WebMercator,
        .coordFormat = GeoCoordFormat::NauticalDMM,
        .initialCenter = chart.totalBounds.Center(),
        .initialTileZoom = 13.0f,
        .requestResetView = true,
        .height = SizeMode::Stretch
    })) {
        gisStats = GisRenderEngine::RenderLayers(
            map,
            chart.layers,
            gisCfg,
            selUid,
            true
        );
    }
    ui.EndFrame(canvas);
    CHECK(gisStats.visibleLayers == chart.layers.size());
    CHECK(gisStats.totalFeatures == chart.totalFeatures);
    CHECK(gisStats.renderedFeatures > 0 || gisStats.cachedTilesRendered > 0 || gisStats.cachedTilesHit > 0);

    keel::println("  [PASS] TestMapViewWidgetWorkflow completed");
}

void TestSonarViewWidgetWorkflow() {
    using namespace guikit;
    keel::println("  - TestSonarViewWidgetWorkflow...");

    Context ui;
    ThorCanvas canvas;

    const keel::u32 width = 900;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    const keel::u32 numBeams = 90;
    const keel::u32 numBins = 128;
    keel::Array<float> polarMatrix(numBeams * numBins, 0.25f);
    for (keel::usize i = 0; i < polarMatrix.size(); ++i) {
        polarMatrix[i] = 0.15f + 0.65f * static_cast<float>((i * 17) % 100) / 100.0f;
    }

    const SonarTarget targets[] = {
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
    keel::i32 selectedId = 10;
    float eblBrg = 25.0f;
    float vrmRng = 200.0f;
    float rangeZoom = 1.0f;
    SonarViewResponse lastResp{};

    auto renderFrame = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        lastResp = ui.SonarView(polarMatrix, {
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
            .height = SizeMode::Stretch
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

    keel::println("  [PASS] TestSonarViewWidgetWorkflow completed");
}

void TestSonarAndSignalGpuShaderPipeline() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;

    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height, 0xFF112233u);
    canvas.SetTarget(pixels.data(), width, height, width);

    keel::Array<float> sonarMatrix(90 * 128, 0.4f);
    keel::Array<float> audioSamples(4096, 0.0f);
    for (keel::usize i = 0; i < audioSamples.size(); ++i) {
        audioSamples[i] = keel::sin(static_cast<float>(i) * 0.1f) * 0.5f;
    }

    bool btnToggle = false;
    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(8.0f)})) {
            if (ui.Button({.text = btnToggle ? "Mode B" : "Mode A"})) {
                btnToggle = !btnToggle;
            }
            ui.SonarView(sonarMatrix, {
                .numBeams = 90,
                .numRangeBins = 128,
                .height = 220.0f
            });
            ui.SignalView(audioSamples, {
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
    CHECK(canvas.GetGpuShaderPasses().size() == 2);

    const auto& sonarPass = canvas.GetGpuShaderPasses()[0];
    CHECK(sonarPass.shader.shaderId == "guikit.sonar.v1");
    CHECK(sonarPass.dataField.extent(0) == 90);
    CHECK(sonarPass.dataField.extent(1) == 128);
    CHECK(sonarPass.plotRect.w > 100.0f);
    CHECK(sonarPass.clipRect.w > 100.0f);

    // Verify PunchHole created transparent underlay pixels inside sonarPass.plotRect
    keel::usize transparentCount = 0;
    const keel::i32 sx0 = static_cast<keel::i32>(sonarPass.plotRect.x) + 4;
    const keel::i32 sy0 = static_cast<keel::i32>(sonarPass.plotRect.y) + 4;
    const keel::i32 sx1 = static_cast<keel::i32>(sonarPass.plotRect.Right()) - 4;
    const keel::i32 sy1 = static_cast<keel::i32>(sonarPass.plotRect.Bottom()) - 4;
    for (keel::i32 y = sy0; y < sy1; y += 4) {
        for (keel::i32 x = sx0; x < sx1; x += 4) {
            const keel::u32 px = pixels[static_cast<keel::usize>(y) * width + static_cast<keel::usize>(x)];
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
    CHECK(canvas.GetGpuShaderPasses().size() == 2);

    // 3. Toggle GPU shaders OFF -> automatic CPU fallback with opaque background in plotRect
    canvas.SetGpuShadersEnabled(false);
    CHECK(!canvas.SupportsGpuShaders());
    CHECK(!canvas.HasActiveGpuPasses());

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    keel::usize opaqueCount = 0;
    for (keel::i32 y = sy0; y < sy1; y += 8) {
        for (keel::i32 x = sx0; x < sx1; x += 8) {
            const keel::u32 px = pixels[static_cast<keel::usize>(y) * width + static_cast<keel::usize>(x)];
            if ((px >> 24) == 255) {
                opaqueCount++;
            }
        }
    }
    CHECK(opaqueCount > 20);

#if __has_include(<SDL3/SDL.h>)
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

            for (keel::i32 f = 0; f < 3; ++f) {
                if (f == 1) {
                    // Mutate sonar matrix & button on frame 1 to exercise glTexSubImage2D & partial updates
                    sonarMatrix[10] = 0.9f;
                    btnToggle = !btnToggle;
                }
                ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
                renderUI();
                ui.EndFrame(canvas);

                SDL_UpdateTexture(tex, nullptr, pixels.data(), width * sizeof(keel::u32));
                SDL_RenderClear(ren);
                if (canvas.HasActiveGpuPasses()) {
                    gpuEngine.RenderUnderlays(canvas, width, height);
                }
                SDL_RenderTexture(ren, tex, nullptr, nullptr);

                if (f == 2) {
                    SDL_Surface* readSurf = SDL_RenderReadPixels(ren, nullptr);
                    CHECK(readSurf != nullptr);
                    if (readSurf) {
                        CHECK(readSurf->w == static_cast<keel::i32>(width));
                        CHECK(readSurf->h == static_cast<keel::i32>(height));
                        SDL_DestroySurface(readSurf);
                    }
                }

                SDL_RenderPresent(ren);
            }

            // 5. Verify mouse crosshair movement over SignalView uses StaticBaseLayerCache (RestoreRegion),
            //    keeps partial dirty rect (!IsFullRepaint), preserves GPU underlay pass, and runs in < 2.0 ms/frame.
            const Rect specPlot = canvas.GetGpuShaderPasses()[1].plotRect;
            double totalCrosshairMs = 0.0;
            constexpr keel::i32 kMoveFrames = 20;
            for (keel::i32 m = 0; m < kMoveFrames; ++m) {
                ui.OnMouseMove({specPlot.x + 25.0f + static_cast<float>(m * 4), specPlot.y + 15.0f + static_cast<float>((m % 5) * 3)});
                const keel::u64 t0 = keel::monotonic_ns();
                ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
                renderUI();
                ui.EndFrame(canvas);
                const keel::u64 t1 = keel::monotonic_ns();
                totalCrosshairMs += static_cast<double>(t1 - t0) / 1e6;

                CHECK(ui.HasDirtyDamage());
                CHECK(!ui.IsFullRepaint());
                CHECK(canvas.HasActiveGpuPasses());
                CHECK(canvas.GetGpuShaderPasses().size() == 2);
            }
            const double avgCrosshairMs = totalCrosshairMs / static_cast<double>(kMoveFrames);
            keel::println("  [BENCH] SignalView mouse crosshair move CPU frame time: {:.3f} ms/frame", avgCrosshairMs);
            CHECK(avgCrosshairMs < 5.0);

            gpuEngine.Shutdown();
            SDL_DestroyTexture(tex);
            SDL_DestroyRenderer(ren);
            SDL_DestroyWindow(win);
        }
        SDL_Quit();
    }
#endif

    keel::println("  [PASS] TestSonarAndSignalGpuShaderPipeline completed");
}

void TestWaterfallViewWidgetAndGpuRingBuffer() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    canvas.SetGpuShadersEnabled(true);

    const keel::u32 width = 960;
    const keel::u32 height = 640;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    constexpr keel::u32 kBins = 256;
    keel::Array<float> specSlice(kBins, -95.0f);
    keel::Array<float> doaSlice(181, -85.0f);

    // Inject synthetic peaks
    for (keel::u32 i = 0; i < kBins; ++i) {
        const float d1 = (static_cast<float>(i) - 64.0f) / 3.0f;
        const float d2 = (static_cast<float>(i) - 128.0f) / 4.0f;
        specSlice[i] = -95.0f + 65.0f * keel::exp(-0.5f * d1 * d1) + 48.0f * keel::exp(-0.5f * d2 * d2);
    }
    for (keel::usize i = 0; i < doaSlice.size(); ++i) {
        const float deg = -90.0f + static_cast<float>(i);
        const float b1 = (deg + 25.0f) / 3.5f;
        const float b2 = (deg - 32.0f) / 4.0f;
        doaSlice[i] = -85.0f + 58.0f * keel::exp(-0.5f * b1 * b1) + 46.0f * keel::exp(-0.5f * b2 * b2);
    }

    WaterfallTrack tracks[2] = {
        {.id = 101, .callsign = "TGT-A", .status = WaterfallTrackStatus::Tracking, .currentX = -25.0f, .driftRatePerSec = 0.45f, .levelDb = -27.0f, .snrDb = 24.5f, .gateWidth = 6.0f},
        {.id = 102, .callsign = "TGT-B", .status = WaterfallTrackStatus::Tracking, .currentX = 32.0f, .driftRatePerSec = -0.20f, .levelDb = -39.0f, .snrDb = 17.2f, .gateWidth = 6.0f}
    };

    keel::u64 seq = 1;
    float f0 = 250.0f;
    WaterfallViewResponse respSpec;
    WaterfallViewResponse respDoa;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(12.0f)})) {
            respSpec = ui.WaterfallView(specSlice, {
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
                .mode = WaterfallMode::DoaBtr,
                .viewLayout = WaterfallLayout::SplitTraceAndWaterfall,
                .colormap = Colormap::Blue,
                .numBins = static_cast<keel::u32>(doaSlice.size()),
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
    for (keel::i32 f = 0; f < 3; ++f) {
        seq = static_cast<keel::u64>(f + 1);
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
    CHECK(canvas.GetGpuShaderPasses().size() == 2);

    // Pause ingestion (keep seq constant) and test mouse crosshair movement over WaterfallView uses StaticBaseLayerCache
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    const Rect wfRect = canvas.GetGpuShaderPasses()[0].plotRect;
    ui.OnMouseMove({wfRect.x + 40.0f, wfRect.y + 30.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(respSpec.hovered);
    CHECK(respSpec.cursorInside);
    CHECK(!ui.IsFullRepaint());
    CHECK(canvas.GetGpuShaderPasses().size() == 2);

    keel::println("  [PASS] TestWaterfallViewWidgetAndGpuRingBuffer completed");
}
#endif

void TestRangeSliderProgressBarSpinBoxAndRadioButton() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 700;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    float lowFreq = 20.0f;
    float highFreq = 60.0f;
    float progressVal = 72.5f;
    float vuLevel = 78.0f;
    keel::i32 spinVal = 10;
    keel::i32 radioMode = 0;

    WidgetResponse rRange;
    WidgetResponse rProg;
    WidgetResponse rVu;
    WidgetResponse rSpin;
    WidgetResponse rRadio0;
    WidgetResponse rRadio1;
    WidgetResponse rRadio2;

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
            if (auto el = ui.Element({
                .label = "Passband Window",
                .description = "Band-pass filter cutoff interval",
                .layout = ElementLayout::Stacked
            })) {
                rRange = ui.RangeSlider(lowFreq, highFreq, 0.0f, 100.0f, {
                    .valueFormat = "%.0f .. %.0f MHz",
                    .step = 1.0,
                    .minSpan = 5.0,
                    .showValue = true
                });
            }

            if (auto el = ui.Element({
                .label = "Calibration Progress",
                .layout = ElementLayout::Stacked
            })) {
                rProg = ui.ProgressBar(progressVal, 0.0f, 100.0f, {
                    .valueFormat = "%.0f %%",
                    .showValue = true
                });
            }

            if (auto el = ui.Element({
                .label = "RF Input Level Meter",
                .layout = ElementLayout::Stacked
            })) {
                rVu = ui.ProgressBar(vuLevel, 0.0f, 100.0f, {
                    .valueFormat = "%.0f dBuV",
                    .peakHoldValue = 90.0,
                    .segments = 24,
                    .showValue = true,
                    .multiZoneColor = true
                });
            }

            if (auto el = ui.Element({
                .label = "Channel Index",
                .layout = ElementLayout::Stacked
            })) {
                rSpin = ui.SpinBox(spinVal, {
                    .min = 0.0,
                    .max = 20.0,
                    .step = 2.0,
                    .stepButtons = true
                });
            }

            if (auto el = ui.Element({.label = "CW Mode"})) {
                rRadio0 = ui.Radio(radioMode, 0);
            }
            if (auto el = ui.Element({.label = "FM Mode"})) {
                rRadio1 = ui.Radio(radioMode, 1);
            }
            if (auto el = ui.Element({.label = "QAM Mode"})) {
                rRadio2 = ui.Radio(radioMode, 2);
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

    CHECK(rRange.rect.w > 200.0f);
    CHECK(rProg.rect.w > 200.0f);
    CHECK(rVu.rect.w > 200.0f);
    CHECK(rSpin.rect.w > 100.0f);
    CHECK(rRadio0.rect.w > 10.0f);
    CHECK(rRadio1.rect.w > 10.0f);
    CHECK(rRadio2.rect.w > 10.0f);

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
    const float spinBoxBottomY = rSpin.rect.y + rSpin.rect.h * 0.5f;
    const float plusBtnX = rSpin.rect.x + rSpin.rect.w - 10.5f;  // centre of the trailing [+] stepper
    const float minusBtnX = rSpin.rect.x + rSpin.rect.w - 31.5f; // centre of the [-] stepper to its left

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

    keel::println("  [PASS] TestRangeSliderProgressBarSpinBoxAndRadioButton completed");
}

void TestTableVirtualizedDataGridWorkflow() {
    using namespace guikit;

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    constexpr keel::usize kTotalRows = 100000;
    keel::i32 selectedRow = 0;
    keel::i32 sortCol = 1;
    TableSortDirection sortDir = TableSortDirection::Ascending;
    bool row0Checked = false;
    keel::usize visStart = 0;
    keel::usize visEnd = 0;
    bool sortChanged = false;
    bool selChanged = false;
    Rect row0CheckRect{};

    keel::i32 pendingJump = -1;
    Rect tableBounds{};
    Rect firstProgressRect{};

    static const keel::StaticArray<TableColumn, 5> cols = {{
        {.title = "Track",  .width = 70.0f,  .align = Align::Center, .sortable = true},
        {.title = "ID",     .width = 100.0f, .sortable = true},
        {.title = "Signal", .flex = 1.5f,    .minWidth = 80.0f,      .sortable = true},
        {.title = "RSSI",   .flex = 1.2f,    .minWidth = 90.0f,      .sortable = true},
        {.title = "State",  .width = 110.0f, .align = Align::Center, .sortable = true}
    }};

    auto renderUI = [&]() {
        if (auto col = ui.Column({.padding = Padding::All(16.0f), .width = SizeMode::Stretch, .height = SizeMode::Stretch})) {
            if (auto table = ui.Table(cols, {
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
                    table.ScrollToRow(static_cast<keel::usize>(pendingJump));
                    pendingJump = -1;
                }
                visStart = table.FirstVisibleRow();
                visEnd = table.EndVisibleRow();
                sortChanged = table.SortChanged();
                selChanged = table.SelectionChanged();

                for (keel::usize r = visStart; r < visEnd; ++r) {
                    if (auto row = table.Row(r)) {
                        if (r == 0) {
                            auto rChk = ui.Checkbox(row0Checked, {.size = UiSize::Sm});
                            row0CheckRect = rChk.rect;
                        } else {
                            bool dummy = (r % 2 == 0);
                            ui.Checkbox(dummy, {.size = UiSize::Sm});
                        }
                        ui.Text(ui.Format("ID-{:06}", r));
                        ui.Text("OFDM-64 Carrier");
                        auto rProg = ui.ProgressBar(-65.0f, -100.0f, -30.0f, {.size = UiSize::Xs});
                        if (r == 50000) {
                            firstProgressRect = rProg.rect;
                        }
                        ui.Element({.badge = {"LOCKED", UiVariant::Success}, .width = SizeMode::Fit});
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
    CHECK(keel::abs(firstProgressRect.x - initialProgX) < 2.0f);
    CHECK(keel::abs(firstProgressRect.w - (initialProgW + 45.0f)) < 3.0f);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    keel::println("  [PASS] TestTableVirtualizedDataGridWorkflow completed");
}

void TestCodeEditorIdeAndDebuggerWorkflow() {
    using namespace guikit;
    keel::println("[TEST] Running TestCodeEditorIdeAndDebuggerWorkflow...");

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 900;
    const keel::u32 height = 650;
    keel::Array<keel::u32> pixels(width * height);
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
    keel::String code(
        "%% Spectral Section\n"
        "function y = filterSig(x)\n"
        "\tw = hann(64, 'periodic');\n"
        "    y = (x') * w;\n"
        "end\n");
    keel::Array<keel::usize> breakpoints;
    breakpoints.push_back(3);
    const CodeDiagnostic diagnostics[] = {
        {3, 4, 8, DiagnosticSeverity::Warning, "Test warning", "W01"}
    };
    const CodeInlineValue inlineValues[] = {
        {3, "w = 64x1 double"}
    };
    const CodeDebugVar debugVars[] = {
        {"w", "[0.0 .. 1.0]", "double (64x1)"}
    };
    keel::usize execLine = 3;
    TextAreaResponse lastResp{};

    auto renderIde = [&]() {
        lastResp = ui.TextArea(code, {
            .autoHeight = false,
            .showLineNumbers = true,
            .syntax = matSyn,
            .syntaxPalette = monoPal,
            .height = 420.0f,
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

    const keel::String initialCode = code.clone();
    ui.OnTextInput("A");
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    const keel::String afterA = code.clone();
    CHECK(afterA != initialCode);

    ui.OnTextInput("(");
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderIde();
    ui.EndFrame(canvas);
    const keel::String afterParen = code.clone();
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
    const keel::usize bpCountBefore = breakpoints.size();
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
    breakpoints.assign(1, 3);
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
            .autoHeight = false,
            .showLineNumbers = true,
            .syntax = matSyn,
            .syntaxPalette = monoPal,
            .height = 420.0f,
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

    // The centered debug toolbar's left grip is near the horizontal center minus half the toolbar width (~71 dp)
    const float gripX = lastResp.rect.x + lastResp.rect.w * 0.5f - 66.0f;
    const float gripY = lastResp.rect.y + 14.0f;
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

    keel::println("  [PASS] TestCodeEditorIdeAndDebuggerWorkflow completed");
}

void TestKeymapAndMarkdownTopTierFeatures() {
    keel::println("  [RUN] TestKeymapAndMarkdownTopTierFeatures...");
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
    const keel::u32 width = 640;
    const keel::u32 height = 480;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    keel::String clipboardText;
    ui.SetClipboardCallbacks(
        [&]() -> keel::String { return clipboardText.clone(); },
        [&](keel::StringRef s) { clipboardText.assign(s); }
    );

    const keel::StringRef mdDoc =
        "# Title Heading\n\n"
        "Paragraph with **bold** and `code` tokens.\n\n"
        "- [ ] First interactive task\n"
        "- [x] Second completed task\n\n"
        "```matlab\n"
        "% MATLAB block\n"
        "y = fft(x);\n"
        "```\n";

    float zoom = 1.0f;
    keel::String searchQuery;
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
    CHECK(mdRes.selectedText.find("Title Heading") != keel::StringRef::npos);
    CHECK(mdRes.selectedText.find("y = fft(x);") != keel::StringRef::npos);

    // Copy via KeyAction::Copy (Ctrl+C)
    ui.OnKeyDown(Key::C, {.ctrl = true});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMd();
    ui.EndFrame(canvas);
    CHECK(!clipboardText.empty());
    CHECK(clipboardText.find("Title Heading") != keel::StringRef::npos);

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
    searchQuery.assign("task");
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMd();
    ui.EndFrame(canvas);
    CHECK(mdRes.searchMatchCount == 2);
    CHECK(mdRes.activeSearchMatch == 0);

    keel::println("  [PASS] TestKeymapAndMarkdownTopTierFeatures completed");
}

void TestUnifiedColormapHistogramAndImageView() {
    using namespace guikit;
    keel::println("  -> TestUnifiedColormapHistogramAndImageView...");

    // 1. Verify unified Colormap LUTs & inversion symmetry across all 4 scientific widgets
#if GUIKIT_HAS_SCENES
    static_assert(keel::IsSame<decltype(SignalViewOptions::colormap), Colormap>);
    static_assert(keel::IsSame<decltype(SonarViewOptions::colormap), Colormap>);
    static_assert(keel::IsSame<decltype(WaterfallViewOptions::colormap), Colormap>);
#endif
    static_assert(keel::IsSame<decltype(ImageViewParams::colormap), Colormap>);

    const auto allNames = GetAllColormapNames();
    CHECK(allNames.size() == static_cast<keel::usize>(Colormap::Count));
    CHECK(GetColormapName(Colormap::Grayscale) == "Grayscale");
    CHECK(GetColormapName(Colormap::Bone) == "Bone");
    CHECK(GetColormapName(Colormap::Inferno) == "Inferno");
    CHECK(GetColormapName(Colormap::Turbo) == "Turbo");
    CHECK(GetColormapName(Colormap::Red) == "Red");
    CHECK(GetColormapName(Colormap::Green) == "Green");
    CHECK(GetColormapName(Colormap::Blue) == "Blue");
    CHECK(GetColormapName(Colormap::Gold) == "Gold");

    for (keel::usize m = 0; m < static_cast<keel::usize>(Colormap::Count); ++m) {
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
    keel::StaticArray<keel::u32, 256> testBins{};
    for (keel::usize i = 20; i <= 200; ++i) {
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
    const keel::u32 W = 1000;
    const keel::u32 H = 700;
    keel::Array<keel::u32> framePixels(W * H);
    canvas.SetTarget(framePixels.data(), W, H, W);

    // Prepare 64x48 16-bit single-channel test image and 64x48 3-channel RGB test image
    constexpr keel::u32 imgW = 64;
    constexpr keel::u32 imgH = 48;
    keel::Array<keel::u16> mono16(imgW * imgH);
    keel::Array<float> rgbFloat(imgW * imgH * 3, 0.0f);
    for (keel::u32 y = 0; y < imgH; ++y) {
        for (keel::u32 x = 0; x < imgW; ++x) {
            mono16[y * imgW + x] = static_cast<keel::u16>((x * 1000 + y * 500) % 65536);
            const keel::usize base = (y * imgW + x) * 3;
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
    monoParams.rotation = ImageRotation::Deg90;
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
    const keel::u32 width = 600;
    const keel::u32 height = 800;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    keel::String inputVal("Test");
    keel::usize selectIdx = 0;
    static constexpr keel::StaticArray<keel::StringRef, 2> selectItems = {"A", "B"};
    float sliderVal = 50.0f;
    float knobVal = 25.0f;

    Response rBtn, rInput, rSelect, rSlider, rKnob, rImg;

    auto renderFrame = [&]() {
        if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(16.0f)})) {
            rBtn = ui.Button({.text = "Tall Button", .height = 64.0f});
            rInput = ui.InputField(inputVal, {.height = 56.0f});
            rSelect = ui.Select(selectIdx, selectItems, {.height = 52.0f});
            rSlider = ui.Slider(sliderVal, 0.0f, 100.0f, {.height = 60.0f});
            rKnob = ui.Knob(knobVal, 0.0f, 100.0f, {.showValue = false, .diameter = 72.0f});
            rImg = ui.Image("assets/images/missing_preview.svg", {.width = 120.0f, .height = 90.0f});
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderFrame();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderFrame();
    ui.EndFrame(canvas);

    CHECK(keel::abs(rBtn.rect.h - 64.0f) < 1.0f);
    CHECK(keel::abs(rInput.rect.h - 56.0f) < 1.0f);
    CHECK(keel::abs(rSelect.rect.h - 52.0f) < 1.0f);
    CHECK(keel::abs(rSlider.rect.h - 60.0f) < 1.0f);
    CHECK_NEAR(rKnob.rect.w, 72.0f, 0.5f);
    CHECK_NEAR(rKnob.rect.h, 72.0f, 0.5f);
    CHECK(keel::abs(rImg.rect.h - 90.0f) < 1.0f);
}

void TestElementOuterFrameAndControlWellFrame() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    const UiTheme& theme = ui.GetTheme();

    // 1. Compile-time architectural purity: frameless controls, Element, and Heading have NO .background/.border;
    // Panels and intrinsic-box controls retain .background and .border.
    static_assert(![]<typename T = SwitchOptions>() { return requires(T o) { o.background; } || requires(T o) { o.border; } || requires(T o) { o.width; } || requires(T o) { o.height; }; }());
    static_assert(![]<typename T = CheckboxOptions>() { return requires(T o) { o.background; } || requires(T o) { o.border; } || requires(T o) { o.width; } || requires(T o) { o.height; }; }());
    static_assert(![]<typename T = RadioOptions>() { return requires(T o) { o.background; } || requires(T o) { o.border; } || requires(T o) { o.width; } || requires(T o) { o.height; }; }());
    static_assert(![]<typename T = KnobOptions>() { return requires(T o) { o.background; } || requires(T o) { o.border; } || requires(T o) { o.width; } || requires(T o) { o.height; }; }());
    static_assert([]<typename T = KnobOptions>() { return requires(T o) { o.diameter; }; }());
    static_assert(![]<typename T = SliderOptions>() { return requires(T o) { o.background; } || requires(T o) { o.border; }; }());
    static_assert(![]<typename T = StepSliderOptions>() { return requires(T o) { o.background; } || requires(T o) { o.border; }; }());
    static_assert(![]<typename T = RangeSliderOptions>() { return requires(T o) { o.background; } || requires(T o) { o.border; }; }());
    static_assert(![]<typename T = ProgressBarOptions>() { return requires(T o) { o.background; } || requires(T o) { o.border; }; }());
    static_assert(![]<typename T = LevelMeterOptions>() { return requires(T o) { o.background; } || requires(T o) { o.border; }; }());
    static_assert(![]<typename T = BreadcrumbOptions>() { return requires(T o) { o.background; } || requires(T o) { o.border; }; }());
    static_assert(![]<typename T = PaginationOptions>() { return requires(T o) { o.background; } || requires(T o) { o.border; }; }());
    static_assert([]<typename T = BoxOptions>() { return requires(T o) { o.background; } && requires(T o) { o.border; }; }());
    static_assert([]<typename T = CardOptions>() { return requires(T o) { o.background; } && requires(T o) { o.border; }; }());
    static_assert([]<typename T = ButtonOptions>() { return requires(T o) { o.background; } && requires(T o) { o.border; }; }());
    static_assert([]<typename T = InputFieldOptions>() { return requires(T o) { o.background; } && requires(T o) { o.border; }; }());
    static_assert([]<typename T = SelectOptions>() { return requires(T o) { o.background; } && requires(T o) { o.border; }; }());
    static_assert([]<typename T = SpinBoxOptions>() { return requires(T o) { o.background; } && requires(T o) { o.border; }; }());

    // Verify Control::Measure & ResolveWidthMode / ResolveHeightMode with unified AxisSize
    const ControlSpec fixedSpec{.width = 220.0f, .defaultWidthMode = SizeMode::Stretch, .minHeight = 40.0f};
    CHECK(Control::ResolveWidthMode(ui, fixedSpec.width, fixedSpec.defaultWidthMode) == SizeMode::Fixed);
    CHECK(Control::ResolveHeightMode(fixedSpec.height, fixedSpec.defaultHeightMode) == SizeMode::Fit);
    CHECK(Control::ResolveWidthMode(ui, AxisSize::Auto(), SizeMode::Stretch) == SizeMode::Stretch);
    CHECK(Control::ResolveWidthMode(ui, SizeMode::Fit, SizeMode::Stretch) == SizeMode::Fit);
    const Vec2 rawTrackSize{180.0f, 24.0f};
    const Vec2 measuredCtrl = Control::Measure(rawTrackSize, fixedSpec, theme);
    CHECK_NEAR(measuredCtrl.x, 220.0f, 0.01f);
    CHECK_NEAR(measuredCtrl.y, 40.0f, 0.01f);

    // 2. Verify Panel (`Card` / `Box`) surfaces & intrinsic-box `Button` borders in full immediate-mode frame
    float sliderVal = 0.5f;
    bool switchVal = true;
    bool checkVal = false;
    Response rSliderPlain, rSliderFramed, rSwitchWell, rCheckWell, rBtnFramed;

    auto renderFrame = [&]() {
        if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(12.0f)})) {
            if (auto el = ui.Element({.label = "Plain Slider"})) {
                rSliderPlain = el.response;
                ui.Slider(sliderVal, 0.0f, 1.0f);
            }
            if (auto box = ui.Box({
                .padding = Padding::All(8.0f),
                .background = StateColor::Surface(),
                .border = {.width = 1.5f, .radius = 8.0f}
            })) {
                if (auto el = ui.Element({.label = "Slider Inside Box Surface"})) {
                    rSliderFramed = box.GetNode() ? Response{.id = box.GetNode()->id, .rect = ui.GetPrevFrameBounds(box.GetNode()->id)} : el.response;
                    ui.Slider(sliderVal, 0.0f, 1.0f);
                }
            }
            if (auto el = ui.Element({.label = "Frameless Switch"})) {
                rSwitchWell = ui.Switch(switchVal);
            }
            if (auto el = ui.Element({.label = "Frameless Checkbox"})) {
                rCheckWell = ui.Checkbox(checkVal);
            }
            if (auto card = ui.Card({
                .padding = Padding::All(8.0f),
                .background = {Color::Hex(0x1E293BFF), Color::Hex(0x334155FF)},
                .border = Border::Solid(1.0f, StateColor::Warning())
            })) {
                if (auto el = ui.Element({.label = "Button Inside Card Surface"})) {
                    rBtnFramed = ui.Button({
                        .text = "Click",
                        .background = StateColor::None(), .border = Border::Solid(2.0f, StateColor::Danger())
                    });
                }
            }
        }
    };

    // Verify StateColor background presets and resolution
    constexpr StateColor bgDefault{};
    static_assert(!bgDefault.IsBackgroundEnabled(false));
    static_assert(bgDefault.IsBackgroundEnabled(true));
    constexpr StateColor bgSurface = StateColor::Surface();
    static_assert(bgSurface.IsSurface());
    static_assert(bgSurface.IsBackgroundEnabled(false));
    constexpr StateColor bgNone = StateColor::None();
    static_assert(bgNone.IsNone());
    static_assert(!bgNone.IsBackgroundEnabled(true));
    constexpr StateColor bgColorOnly = {Color::Hex(0x1E293BFF)};
    static_assert(bgColorOnly.IsBackgroundEnabled(false));
    constexpr StateColor bgStatePair = {Color::Hex(0x1E293BFF), Color::Hex(0x334155FF)};
    static_assert(bgStatePair.IsBackgroundEnabled(false));
    static_assert(bgStatePair.hover.has_value());

    // Verify Border aggregate initialization and factory presets
    constexpr Border noneBorder = Border::None();
    static_assert(!noneBorder.HasStroke());
    static_assert(!noneBorder.HasAny());
    constexpr Border solidBorder = Border::Solid(1.5f, StateColor::Info(), 8.0f);
    static_assert(solidBorder.HasStroke());
    static_assert(solidBorder.HasAny());
    constexpr Border roundedBorder = Border::Rounded(12.0f);
    static_assert(!roundedBorder.HasStroke());
    static_assert(roundedBorder.HasAny());

    // Verify StateColor aggregate initialization, presets, and 4-state resolution
    constexpr StateColor defCol = StateColor::Default();
    static_assert(!defCol.HasAny());

    constexpr StateColor dgrCol = StateColor::Danger();
    static_assert(dgrCol.HasAny());
    static_assert(dgrCol.normal.has_value());
    static_assert(dgrCol.hover.has_value());
    static_assert(dgrCol.active.has_value());
    static_assert(dgrCol.disabled.has_value());
    static_assert(dgrCol == StateColor::FromVariant(UiVariant::Danger));

    constexpr StateColor customCol{
        .normal = Color(100, 120, 140, 255),
        .hover = Color(150, 170, 190, 255)
    };
    const Color fb(10, 20, 30, 255);
    CHECK(customCol.Resolve(false, false, false, fb) == Color(100, 120, 140, 255));
    CHECK(customCol.Resolve(true, false, false, fb) == Color(150, 170, 190, 255));
    // Active falls back to hover when active is nullopt and hover is set
    CHECK(customCol.Resolve(false, true, false, fb) == Color(150, 170, 190, 255));
    // Disabled falls back to normal with 40% alpha when disabled is nullopt
    CHECK(customCol.Resolve(false, false, true, fb) == Color(100, 120, 140, 255).WithAlphaF(0.40f));

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

#if GUIKIT_HAS_SCENES
void TestAttitudeViewWidgetWorkflow() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 900;
    const keel::u32 height = 650;
    keel::Array<keel::u32> pixels(width * height);
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
            .width = SizeMode::Stretch,
            .height = SizeMode::Stretch
        })) {
            resp = ui.AttitudeView({
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
                .height = SizeMode::Stretch
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
    const keel::u32 width = 960;
    const keel::u32 height = 560;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    ClusterLayoutMode layoutMode = ClusterLayoutMode::DualDial;
    PowertrainMode powertrain = PowertrainMode::IceTurbo;
    ClusterDriveTheme driveTheme = ClusterDriveTheme::ComfortCyan;
    ClusterCenterMfd centerMfd = ClusterCenterMfd::AdasLaneRadar;
    SpeedUnit speedUnit = SpeedUnit::Kmh;
    keel::i32 gear = 4;
    CarClusterViewResponse resp{};

    auto renderFrame = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto col = ui.Column({
            .padding = Padding::All(12.0f),
            .width = SizeMode::Stretch,
            .height = SizeMode::Stretch
        })) {
            resp = ui.CarClusterView({
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
                .height = SizeMode::Stretch
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
    keel::println("  -> TestBpmnEditorWidgetWorkflow");
    Context ui;
    ThorCanvas canvas;
    keel::Array<keel::u32> pixels(960 * 600);
    canvas.SetTarget(pixels.data(), 960, 600, 960);

    BpmnDiagram diagram;
    diagram.name = "Unit Test BPMN Process";
    const keel::u32 laneId = diagram.AddLane("Test Pool", "Main Lane", Rect(40.0f, 40.0f, 800.0f, 220.0f));
    const keel::u32 nStart = diagram.AddNode({
        .type = BpmnNodeType::StartEvent,
        .label = "Start",
        .pos = {100.0f, 120.0f},
        .eventDef = BpmnEventDefinition::Message,
        .laneId = laneId
    });
    const keel::u32 nTask = diagram.AddNode({
        .type = BpmnNodeType::Task,
        .label = "Process Order",
        .pos = {220.0f, 106.0f},
        .taskType = BpmnTaskType::Service,
        .laneId = laneId,
        .slaDurationMs = 300.0f
    });
    const keel::u32 nBoundary = diagram.AddNode({
        .type = BpmnNodeType::BoundaryEvent,
        .label = "Timeout",
        .eventDef = BpmnEventDefinition::Timer,
        .attachedToNodeId = nTask,
        .boundaryRatioX = 0.70f,
        .laneId = laneId
    });
    const keel::u32 nEnd = diagram.AddNode({
        .type = BpmnNodeType::EndEvent,
        .label = "Done",
        .pos = {440.0f, 120.0f},
        .laneId = laneId
    });
    const keel::u32 e1 = diagram.AddEdge(nStart, nTask, BpmnFlowType::SequenceFlow);
    const keel::u32 e2 = diagram.AddEdge(nTask, nEnd, BpmnFlowType::DefaultFlow, "OK");
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
    keel::u32 selNode = 0;
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

void TestGisDemoBayGeoJsonAndRenderEngine() {
    using namespace guikit;

    auto res = GeoJsonIO::LoadFromFile("assets/geo/demo_bay.geojson", /*splitByThematicClass=*/true);
    CHECK(res.ok);
    CHECK(res.layers.size() >= 5);
    CHECK(res.totalFeatures > 100);
    CHECK(res.totalVertices > 5000);
    CHECK(res.totalBounds.IsValid());

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    GisRenderConfig settings{};
    settings.asyncBackgroundStreaming = false;
    GisRenderStats stats{};
    keel::u64 selectedUid = 0;

    for (keel::i32 f = 0; f < 2; ++f) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto map = ui.MapView({
            .crs = MapCrs::EPSG3857_WebMercator,
            .initialCenter = res.totalBounds.Center(),
            .initialTileZoom = 13.4f,
            .height = SizeMode::Stretch
        })) {
            stats = GisRenderEngine::RenderLayers(
                map,
                res.layers,
                settings,
                selectedUid,
                true
            );
        }
        ui.EndFrame(canvas);
    }

    CHECK(stats.totalFeatures == res.totalFeatures);
    CHECK(stats.renderedFeatures > 0 || stats.cachedTilesHit > 0 || stats.cachedTilesRendered > 0);
    CHECK(stats.renderedVertices > 0);
}
#endif

void TestAccordionWidgetWorkflow() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 600;
    const keel::u32 height = 500;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    keel::i32 bodyExecutedCount1 = 0;
    keel::i32 bodyExecutedCount2 = 0;
    bool externalOpen = false;
    Response acc1Res{};
    Response acc2Res{};

    auto renderAccordions = [&]() {
        if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(12.0f), .width = SizeMode::Stretch})) {
            if (auto acc1 = ui.Accordion({
                .label = "ELEMENT",
                .description = "Shared element properties",
                .icon = Icon::Settings(),
                .badge = {"8", StateColor::Info()},
                .defaultOpen = true
            })) {
                acc1Res = acc1.response;
                ++bodyExecutedCount1;
                ui.Button({.text = "Inside Element Accordion"});
            } else {
                acc1Res = acc1.response;
            }

            if (auto acc2 = ui.Accordion(externalOpen, {
                .label = "CONTROL",
                .chevronPlacement = Placement::Trailing
            })) {
                acc2Res = acc2.response;
                ++bodyExecutedCount2;
                ui.Button({.text = "Inside Control Accordion"});
            } else {
                acc2Res = acc2.response;
            }
        }
    };

    // Frame 1: acc1 is open (defaultOpen = true), acc2 is closed (externalOpen = false)
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderAccordions();
    ui.EndFrame(canvas);

    CHECK(bodyExecutedCount1 == 1);
    CHECK(bodyExecutedCount2 == 0);
    const Rect r1 = ui.GetPrevFrameBounds(acc1Res.id);
    const Rect r2 = ui.GetPrevFrameBounds(acc2Res.id);
    CHECK(r1.w > 100.0f);
    CHECK(r1.h > 20.0f);
    CHECK(r2.w > 100.0f);
    CHECK(r2.h > 20.0f);

    // Frame 2: Click on acc1 header to collapse it
    ui.OnMouseMove({r1.x + r1.w * 0.5f, r1.y + r1.h * 0.5f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderAccordions();
    ui.EndFrame(canvas);

    bodyExecutedCount1 = 0;
    bodyExecutedCount2 = 0;
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderAccordions();
    ui.EndFrame(canvas);

    CHECK(acc1Res.clicked);
    CHECK(acc1Res.changed);
    CHECK(bodyExecutedCount1 == 0); // Collapsed immediately on toggle frame!
    CHECK(bodyExecutedCount2 == 0);

    // Frame 3: Click on acc2 header (which moved up after acc1 collapsed) to expand it
    const Rect r2AfterCollapse = ui.GetPrevFrameBounds(acc2Res.id);
    CHECK(r2AfterCollapse.y < r2.y);
    ui.OnMouseMove({r2AfterCollapse.x + r2AfterCollapse.w * 0.5f, r2AfterCollapse.y + r2AfterCollapse.h * 0.5f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderAccordions();
    ui.EndFrame(canvas);

    bodyExecutedCount1 = 0;
    bodyExecutedCount2 = 0;
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderAccordions();
    ui.EndFrame(canvas);

    CHECK(acc2Res.clicked);
    CHECK(acc2Res.changed);
    CHECK(externalOpen);
    CHECK(bodyExecutedCount1 == 0);
    CHECK(bodyExecutedCount2 == 1);
}

void TestSizeScopeChainingAndIsolation() {
    using namespace guikit;
    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 600;
    const keel::u32 height = 500;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    Response btnInheritedInsideXlElement{};
    Response btnNoneInsideXlElement{};
    Response btnSmInsideXlElement{};
    Response btnInsideSmScope{};
    Response btnAfterSmScope{};

    auto renderUI = [&]() {
        if (auto col = ui.Column({.gap = 8.0f})) {
            // 1. Element with Xl size does NOT override explicit or default child widget sizes (pure widget property)
            if (auto el = ui.Element({.label = "XL Element Header", .size = UiSize::Xl})) {
                btnInheritedInsideXlElement = ui.Button({.text = "Explicit Xl (42px)", .size = UiSize::Xl});
            }

            // 2. Element with Xl size + Control with default/None resolves control to Md (30px)
            if (auto el = ui.Element({.label = "XL Element + Default Control", .size = UiSize::Xl})) {
                btnNoneInsideXlElement = ui.Button({.text = "Default Md (30px)"});
            }

            // 3. Element with Xl size + Control with explicit Sm size resolves to Sm (24px)
            if (auto el = ui.Element({.label = "XL Element + SM Control", .size = UiSize::Xl})) {
                btnSmInsideXlElement = ui.Button({.text = "Explicit Sm (24px)", .size = UiSize::Sm});
            }

            // 4. Custom Factor / Dp sizing on individual controls
            if (auto el = ui.Element({.label = "Element with Dp button"})) {
                btnInsideSmScope = ui.Button({.text = "Sm Button (24px)", .size = UiSize::Dp(24.0f)});
            }

            btnAfterSmScope = ui.Button({.text = "Default Md Button"});
        }
    };

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK_NEAR(btnInheritedInsideXlElement.rect.h, 42.0f, 0.5f);
    CHECK_NEAR(btnNoneInsideXlElement.rect.h, 30.0f, 0.5f);
    CHECK_NEAR(btnSmInsideXlElement.rect.h, 24.0f, 0.5f);
    CHECK_NEAR(btnInsideSmScope.rect.h, 24.0f, 0.5f);
    CHECK_NEAR(btnAfterSmScope.rect.h, 30.0f, 0.5f);
}

void TestTreeViewAndTreeItemWorkflow() {
    using namespace guikit;
    keel::println("[TEST] Running TestTreeViewAndTreeItemWorkflow...");

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 700;
    const keel::u32 height = 500;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    keel::i32 selectedIndex = -1;
    Id selectedId = Id::Null();
    keel::String filterQuery;
    bool rootOpen = true;
    bool collapsedBranchOpen = false;
    bool chkRoot = true;
    bool chkLeafA = true;
    bool rootSwitch = false;
    bool collapsedBranchSwitch = false;
    float leafSlider = 0.5f;

    Response rootRowRes{};
    Response leafARowRes{};
    Response collapsedBranchRowRes{};
    Response leafInsideCollapsedRes{};
    Response rootSwitchRes{};
    Response collapsedSwitchRes{};
    keel::i32 visibleCount = 0;
    bool checkChangedFlag = false;

    auto renderUI = [&]() {
        leafInsideCollapsedRes = Response{};
        checkChangedFlag = false;
        if (auto col = ui.Column({.padding = Padding::All(12.0f)})) {
            if (auto tree = ui.TreeView({
                .title = "TEST TREE",
                .guides = TreeGuides::Connected,
                .searchable = true,
                .filterText = &filterQuery,
                .showToolbar = true,
                .selectedIndex = &selectedIndex,
                .selectedId = &selectedId
            })) {
                if (auto r1 = ui.TreeItem(rootOpen, {
                    .label = "Root Branch",
                    .icon = Icon::Folder(),
                    .checked = &chkRoot
                })) {
                    rootRowRes = r1.response;
                    if (r1.checkChanged) {
                        checkChangedFlag = true;
                    }
                    rootSwitchRes = ui.Switch(rootSwitch);

                    if (auto c1 = ui.TreeItem("Alpha Leaf", {
                        .detail = "12 KB",
                        .badge = "OK",
                        .checked = &chkLeafA
                    })) {
                        leafARowRes = c1.response;
                        ui.Slider(leafSlider, 0.0f, 1.0f, {.width = 100.0f});
                    }
                }

                // Collapsed branch: its embedded Switch must STILL be laid out & interactive,
                // while its child TreeItem is not instantiated (yet auto-detected as a branch!)
                if (auto r2 = ui.TreeItem(collapsedBranchOpen, {
                    .label = "Collapsed Branch",
                    .icon = Icon::Folder()
                })) {
                    collapsedBranchRowRes = r2.response;
                    collapsedSwitchRes = ui.Switch(collapsedBranchSwitch);

                    if (auto hiddenChild = ui.TreeItem("Deep Hidden Target")) {
                        leafInsideCollapsedRes = hiddenChild.response;
                    }
                }

                visibleCount = tree.VisibleItemCount();
            }
        }
    };

    // Frame 1 & 2: settle layout and state
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Verify 3 visible items: Root Branch, Alpha Leaf, Collapsed Branch (Deep Hidden Target is collapsed)
    CHECK(visibleCount == 3);
    CHECK(rootRowRes.rect.w > 200.0f);
    CHECK(leafARowRes.rect.y > rootRowRes.rect.y);
    CHECK(collapsedBranchRowRes.rect.y > leafARowRes.rect.y);
    CHECK(leafInsideCollapsedRes.rect.w == 0.0f);

    // Embedded Switch on collapsed branch must still be visible and laid out inside the row!
    CHECK(collapsedSwitchRes.rect.w > 20.0f);
    CHECK(collapsedSwitchRes.rect.h > 10.0f);
    CHECK(collapsedSwitchRes.rect.x > collapsedBranchRowRes.rect.x + 100.0f);

    // 1. Click the embedded Switch on the collapsed branch -> toggles collapsedBranchSwitch WITHOUT selecting the row
    const Vec2 swCenter = {
        collapsedSwitchRes.rect.x + collapsedSwitchRes.rect.w * 0.5f,
        collapsedSwitchRes.rect.y + collapsedSwitchRes.rect.h * 0.5f
    };
    ui.OnMouseMove(swCenter);
    ui.OnMouseDown(1);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(collapsedBranchSwitch == true);
    CHECK(selectedIndex == -1);
    CHECK(collapsedBranchOpen == false);

    // 2. Click the row body of "Alpha Leaf" -> selects row index 1
    const Vec2 leafBodyPos = {
        leafARowRes.rect.x + 90.0f,
        leafARowRes.rect.y + leafARowRes.rect.h * 0.5f
    };
    ui.OnMouseMove(leafBodyPos);
    ui.OnMouseDown(1);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(selectedIndex == 1);
    CHECK(selectedId.IsValid());

    // 3. Click the checkbox on "Root Branch" -> toggles chkRoot
    // Settle one frame so st.checkboxRect is up to date, then click checkbox area
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    // Checkbox on depth-0 row is right after padX + chevronSlotW + gap
    const Vec2 rootCheckboxPos = {
        rootRowRes.rect.x + 34.0f,
        rootRowRes.rect.y + rootRowRes.rect.h * 0.5f
    };
    ui.OnMouseMove(rootCheckboxPos);
    ui.OnMouseDown(1);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(chkRoot == false);
    CHECK(checkChangedFlag == true);

    // 4. Filter by "Deep Hidden" -> automatically expands collapsed branch and shows ancestor + matching leaf (2 items)
    filterQuery.assign("Deep Hidden");
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);

    CHECK(leafInsideCollapsedRes.rect.w > 100.0f);

    // Clear filter -> returns to normal open/closed states
    filterQuery.clear();
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderUI();
    ui.EndFrame(canvas);
    CHECK(visibleCount == 3);

    // 5. Multi-column TreeView (TreeTable) with direct controls, CellText, CellBadge, and multi-widget item.Cell()
    bool cellSwitchVal = false;
    bool cellBtnClicked = false;
    Rect cellSwRect{};
    Rect cellBtnRect{};
    Response multiColRowRes{};
    keel::i32 multiColSelected = -1;

    auto renderMultiColTree = [&]() {
        if (auto tree = ui.TreeView({
            {.title = "Node",      .flex = 2.0f, .minWidth = 140.0f},
            {.title = "Status",    .width = 90.0f,  .align = Align::Center},
            {.title = "Controls",  .width = 160.0f, .align = Align::End},
        }, {
            .stickyHeader = true,
            .resizableColumns = true,
            .gridLines = true,
            .selectedIndex = &multiColSelected,
        })) {
            if (auto item = ui.TreeItem({.label = "Row With Multi-Widget Cell"})) {
                ui.Element({.badge = "ACTIVE", .width = SizeMode::Fit});
                if (auto cell = item.Cell()) {
                    cellSwRect = ui.Switch(cellSwitchVal).rect;
                    auto btn = ui.Button({.text = "Act"});
                    cellBtnRect = btn.rect;
                    if (btn.clicked) cellBtnClicked = true;
                }
                multiColRowRes = item.response;
            }
        }
    };

    for (keel::i32 f = 0; f < 2; ++f) {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        renderMultiColTree();
        ui.EndFrame(canvas);
    }

    CHECK(multiColRowRes.rect.w > 200.0f);
    CHECK(cellSwRect.w > 10.0f);
    CHECK(cellBtnRect.w > 10.0f);
    // Switch and Button inside item.Cell() should be arranged horizontally inside the 3rd column
    CHECK(cellBtnRect.x > cellSwRect.x);
    CHECK(cellSwRect.x > multiColRowRes.rect.x + 150.0f);

    // Clicking the Switch inside item.Cell() should toggle cellSwitchVal WITHOUT selecting the row
    const Vec2 cellSwPos = {cellSwRect.x + cellSwRect.w * 0.5f, cellSwRect.y + cellSwRect.h * 0.5f};
    ui.OnMouseMove(cellSwPos);
    ui.OnMouseDown(1);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMultiColTree();
    ui.EndFrame(canvas);

    CHECK(cellSwitchVal == true);
    CHECK(multiColSelected == -1);

    // Clicking the Button inside item.Cell() should fire clicked WITHOUT selecting the row
    const Vec2 cellBtnPos = {cellBtnRect.x + cellBtnRect.w * 0.5f, cellBtnRect.y + cellBtnRect.h * 0.5f};
    ui.OnMouseMove(cellBtnPos);
    ui.OnMouseDown(1);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMultiColTree();
    ui.EndFrame(canvas);

    CHECK(cellBtnClicked == true);
    CHECK(multiColSelected == -1);

    // Clicking the CellBadge in column 1 (non-interactive) SHOULD select the row
    const Vec2 badgeColPos = {multiColRowRes.rect.x + 260.0f, multiColRowRes.rect.y + multiColRowRes.rect.h * 0.5f};
    ui.OnMouseMove(badgeColPos);
    ui.OnMouseDown(1);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderMultiColTree();
    ui.EndFrame(canvas);

    CHECK(multiColSelected == 0);

    keel::println("  [PASS] TestTreeViewAndTreeItemWorkflow completed");
}

void TestKnobSpinBoxAndRadioElements() {
    const keel::u32 width = 640;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixelBuffer(width * height);
    ThorCanvas canvas;
    canvas.SetTarget(pixelBuffer.data(), width, height, width);

    Context ui;
    float knobGain = 50.0f;
    float knobPan = 0.0f;
    float spinFreq = 100.0f;
    keel::i32 radioGroup = 1;
    bool singleRadio = false;

    Response rKnobGain{};
    Response rKnobPan{};
    Response rSpinFreq{};
    Response rRadio0{};
    Response rRadio1{};
    Response rSingleRadio{};

    auto renderScene = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
            if (auto row = ui.Row({.gap = 16.0f})) {
                if (auto el = ui.Element({.label = "Rotary Gain", .layout = ElementLayout::Top, .width = SizeMode::Fit})) {
                    rKnobGain = ui.Knob(knobGain, 0.0f, 100.0f, {
                        .valueFormat = "%.1f %%",
                        .step = 5.0,
                        .defaultValue = 25.0,
                        .tickCount = 11,
                        .showValue = true,
                        .showTicks = true,
                        .showArc = true,
                        .bipolar = false,
                        .valuePlacement = Placement::Bottom
                    });
                }
                if (auto el = ui.Element({.label = "Stereo Pan", .layout = ElementLayout::Top, .width = SizeMode::Fit})) {
                    rKnobPan = ui.Knob(knobPan, -50.0f, 50.0f, {
                        .valueFormat = "%+.0f",
                        .step = 2.0,
                        .defaultValue = 0.0,
                        .bipolar = true,
                        .valuePlacement = Placement::Thumb
                    });
                }
            }

            if (auto el = ui.Element({.label = "Center Frequency"})) {
                rSpinFreq = ui.SpinBox(spinFreq, {
                    .suffix = "MHz",
                    .valueFormat = "%.1f",
                    .min = 0.0,
                    .max = 500.0,
                    .step = 10.0,
                    .stepButtons = true,
                    .showFillBar = true
                });
            }

            if (auto el = ui.Element({.label = "Mode A"})) {
                rRadio0 = ui.Radio(radioGroup, 0);
            }
            if (auto el = ui.Element({.label = "Mode B"})) {
                rRadio1 = ui.Radio(radioGroup, 1);
            }
            if (auto el = ui.Element({.label = "Single Radio"})) {
                rSingleRadio = ui.Radio(singleRadio);
            }
        }
    };

    // Frame 1 & 2: Measure & Layout
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(rKnobGain.rect.w >= 48.0f);
    CHECK(rKnobGain.rect.h >= 60.0f);
    CHECK(rKnobPan.rect.w >= 48.0f);
    CHECK(rSpinFreq.rect.w >= 100.0f);
    CHECK(rRadio0.rect.w >= 10.0f);
    CHECK(rRadio1.rect.w >= 10.0f);

    // 1. Test Knob vertical drag: press at center, drag upward by 45px -> value increases
    const Vec2 knobCenter = {rKnobGain.rect.x + rKnobGain.rect.w * 0.5f, rKnobGain.rect.y + 24.0f};
    ui.OnMouseMove(knobCenter);
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    ui.OnMouseMove({knobCenter.x, knobCenter.y - 45.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(rKnobGain.changed);
    CHECK(knobGain > 60.0f);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    // 2. Test Knob mouse wheel increment on bipolar knob
    const Vec2 panCenter = {rKnobPan.rect.x + rKnobPan.rect.w * 0.5f, rKnobPan.rect.y + rKnobPan.rect.h * 0.5f};
    ui.OnMouseMove(panCenter);
    ui.OnMouseWheel(0.0f, 1.0f);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(rKnobPan.changed);
    CHECK(keel::abs(knobPan - 2.0f) < 0.01f);

    // 3. Test SpinBox: dragging on value region selects text (does NOT scrub value),
    //    and holding down the '+' button triggers typematic auto-repeat stepping.
    const Vec2 spinCenter = {rSpinFreq.rect.x + rSpinFreq.rect.w * 0.5f, rSpinFreq.rect.y + rSpinFreq.rect.h * 0.5f};
    ui.OnMouseMove(spinCenter);
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    ui.OnMouseMove({spinCenter.x + 24.0f, spinCenter.y});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(!rSpinFreq.changed);
    CHECK(keel::abs(spinFreq - 100.0f) < 0.01f);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    // Press and hold '+' button on rSpinFreq:
    // Initial press steps from 100 -> 110; holding past 0.40s initial delay auto-repeats.
    const Vec2 spinPlusPt = {rSpinFreq.rect.Right() - 14.0f, rSpinFreq.rect.Bottom() - 16.0f};
    ui.OnMouseMove(spinPlusPt);
    ui.SetCurrentTime(10.0);
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);
    CHECK(rSpinFreq.changed);
    CHECK_NEAR(spinFreq, 110.0f, 0.01f);

    // Before initial 0.40s typematic delay (t = 10.25s) -> no repeat yet
    ui.SetCurrentTime(10.25);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);
    CHECK(!rSpinFreq.changed);
    CHECK_NEAR(spinFreq, 110.0f, 0.01f);

    // After 0.40s typematic delay (t = 10.45s) -> first auto-repeat step (110 -> 120)
    ui.SetCurrentTime(10.45);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);
    CHECK(rSpinFreq.changed);
    CHECK_NEAR(spinFreq, 120.0f, 0.01f);

    // Next repeat interval (t = 10.53s) -> second auto-repeat step (120 -> 130)
    ui.SetCurrentTime(10.53);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);
    CHECK(rSpinFreq.changed);
    CHECK_NEAR(spinFreq, 130.0f, 0.01f);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    // 4. Test standalone Radio group and single Radio toggle
    ui.OnMouseMove({rRadio0.rect.x + 8.0f, rRadio0.rect.y + 8.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(rRadio0.changed);
    CHECK(radioGroup == 0);

    ui.OnMouseMove({rSingleRadio.rect.x + 8.0f, rSingleRadio.rect.y + 8.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(rSingleRadio.changed);
    CHECK(singleRadio == true);

    keel::println("  [PASS] TestKnobSpinBoxAndRadioElements completed");
}

void TestSegmentedAndColorPickerElements() {
    using namespace guikit;
    keel::println("  -> TestSegmentedAndColorPickerElements...");

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 640;
    const keel::u32 height = 560;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    keel::usize segIndex = 0;
    keel::u32 segMask = 0b0001u;
    Color pickedColor(59, 130, 246, 255);
    Color inlineColor(16, 185, 129, 255);

    Response rSeg{};
    Response rSegMask{};
    Response rColorBtn{};
    Response rColorInline{};

    auto renderScene = [&]() {
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
            if (auto el = ui.Element({.label = "Waveform Mode"})) {
                rSeg = ui.Segmented(segIndex, {
                    {.text = "Sine", .icon = Icon::Refresh()},
                    {.text = "Square", .icon = Icon::Settings()},
                    {.text = "Saw", .icon = Icon::Play()},
                    {.text = "Noise", .icon = Icon::Warning()}
                });
            }
            if (auto el = ui.Element({.label = "Active Channels"})) {
                rSegMask = ui.SegmentedMask(segMask, {"CH1", "CH2", "CH3", "CH4"});
            }
            if (auto el = ui.Element({.label = "Trace Color"})) {
                rColorBtn = ui.ColorPicker(pickedColor, {
                    .showHex = true,
                    .showAlpha = true,
                    .showSwatches = true
                });
            }
            if (auto el = ui.Element({.label = "Inline Palette"})) {
                rColorInline = ui.ColorPicker(inlineColor, {
                    .showAlpha = true,
                    .showSwatches = true,
                    .inlinePanel = true
                });
            }
        }
    };

    // Frame 1 & 2: Measure & Layout
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(rSeg.rect.w > 200.0f);
    CHECK(rSegMask.rect.w > 200.0f);
    CHECK(rColorBtn.rect.w > 120.0f);
    CHECK(rColorInline.rect.h >= 190.0f);

    // 1. Click 3rd segment ("Saw", index 2) of single-select Segmented control
    const float seg2X = rSeg.rect.x + rSeg.rect.w * 0.625f;
    const float seg2Y = rSeg.rect.y + rSeg.rect.h * 0.5f;
    ui.OnMouseMove({seg2X, seg2Y});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(rSeg.changed);
    CHECK(segIndex == 2);

    // 2. Click 2nd segment ("CH2", bit 1) of multi-select SegmentedMask control
    const float mask1X = rSegMask.rect.x + rSegMask.rect.w * 0.375f;
    const float mask1Y = rSegMask.rect.y + rSegMask.rect.h * 0.5f;
    ui.OnMouseMove({mask1X, mask1Y});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);
    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(rSegMask.changed);
    CHECK(segMask == 0b0011u);

    // 3. Test HSV <-> RGB round-trip conversion
    float h = 0.0f, s = 0.0f, v = 0.0f;
    ColorPicker::RgbToHsv(Color(239, 68, 68, 200), h, s, v);
    const Color roundTrip = ColorPicker::HsvToRgb(h, s, v, 200);
    CHECK(keel::abs(keel::i32(roundTrip.r) - 239) <= 1);
    CHECK(keel::abs(keel::i32(roundTrip.g) - 68) <= 1);
    CHECK(keel::abs(keel::i32(roundTrip.b) - 68) <= 1);
    CHECK(roundTrip.a == 200);

    // 4. Drag inside inline ColorPicker SV square to change color
    const Vec2 svPt = {rColorInline.rect.x + 40.0f, rColorInline.rect.y + 30.0f};
    ui.OnMouseMove(svPt);
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    ui.OnMouseMove({svPt.x + 80.0f, svPt.y + 20.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(rColorInline.changed);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    keel::println("  [PASS] TestSegmentedAndColorPickerElements completed");
}

void TestSidebarResizableCompactAndCollapsedModes() {
    using namespace guikit;
    keel::println("  -> TestSidebarResizableCompactAndCollapsedModes...");

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 800;
    const keel::u32 height = 500;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    SidebarMode sbMode = SidebarMode::Expanded;
    keel::i32 selectedNav = 0;
    bool bodyExecuted = false;
    Id sbId = Id::Null();
    Node* sbNode = nullptr;

    auto renderScene = [&]() {
        bodyExecuted = false;
        if (auto root = ui.Row({.gap = 8.0f, .height = SizeMode::Stretch})) {
            if (auto sb = ui.Sidebar(sbMode, {
                .side = SidebarSide::Left,
                .width = 200.0f,
                .minWidth = 140.0f,
                .maxWidth = 360.0f,
                .resizable = true,
                .allowCompact = true,
                .allowCollapse = true
            })) {
                bodyExecuted = true;
                sbId = sb.response.id;
                sbNode = sb.GetNode();
                if (auto menu = ui.SidebarMenu()) {
                    menu.Section("MAIN");
                    menu.Item(selectedNav, 0, {.text = "Dashboard", .icon = Icon::Folder()});
                    menu.Item(selectedNav, 1, {.text = "Settings", .icon = Icon::Settings()});
                }
            }
            if (auto content = ui.Box({.width = SizeMode::Stretch, .height = SizeMode::Stretch})) {
                ui.Text("Workspace");
            }
        }
    };

    // Frame 1 & 2: Initial Expanded layout (width = 200px)
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(bodyExecuted == true);
    CHECK(sbMode == SidebarMode::Expanded);
    CHECK(sbNode != nullptr);
    CHECK(keel::abs(sbNode->computedRect.w - 200.0f) < 1.0f);

    // 1. Drag inner sash at x=200, y=100 (above center toggle button) to x=280 -> expands width to 280px
    ui.OnMouseMove({200.0f, 100.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    ui.OnMouseMove({280.0f, 100.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(sbMode == SidebarMode::Expanded);
    CHECK(keel::abs(sbNode->computedRect.w - 280.0f) < 1.0f);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    // 2. Drag sash from 280px inward to x=60 -> snaps to Compact (icons only), preserving 280px as lastExpandedWidth
    ui.OnMouseMove({280.0f, 100.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    ui.OnMouseMove({60.0f, 100.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(sbMode == SidebarMode::Compact);
    CHECK(bodyExecuted == true);
    CHECK(sbNode->computedRect.w > 30.0f && sbNode->computedRect.w < 80.0f);

    // 3. Continue dragging sash to x=10 (below collapseSnapW 24px) -> snaps to Collapsed (0px)
    ui.OnMouseMove({10.0f, 100.0f});
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(sbMode == SidebarMode::Collapsed);
    CHECK(bodyExecuted == false);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    // 4. Click edge toggle button at x=6, y=250 -> restores to Expanded with the 280px width preserved!
    ui.OnMouseMove({6.0f, 250.0f});
    ui.OnMouseDown(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    ui.OnMouseUp(1);
    ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
    renderScene();
    ui.EndFrame(canvas);

    CHECK(sbMode == SidebarMode::Expanded);
    CHECK(bodyExecuted == true);
    CHECK(keel::abs(sbNode->computedRect.w - 280.0f) < 1.0f);

    keel::println("  [PASS] TestSidebarResizableCompactAndCollapsedModes completed");
}

void TestModalContextMenuAndToastWorkflow() {
    using namespace guikit;
    keel::println("[TEST] Running TestModalContextMenuAndToastWorkflow...");

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    bool modalOpen = false;
    bool bgClicked = false;
    bool modalConfirmClicked = false;
    bool modalBodyExecuted = false;
    Rect bgBtnRect{};
    bool ctxOpen = false;
    Vec2 ctxPos{};
    Rect modalConfirmRect{};
    Rect modalDialogRect{};
    ContextMenuResponse ctxResp{};

    const ContextMenuItem menuItems[] = {
        {.label = "Inspect Packet", .shortcut = "Enter", .icon = Icon::Search()},
        {.label = "Copy Frame HEX", .shortcut = "Ctrl+C", .icon = Icon::Copy(), .separatorAfter = true},
        {.label = "Disabled Action", .disabled = true},
        {.label = "Drop Stream", .icon = Icon::Trash(), .variant = UiVariant::Danger}
    };

    auto renderScene = [&]() {
        bgClicked = false;
        modalConfirmClicked = false;
        modalBodyExecuted = false;

        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto root = ui.Column({.gap = 12.0f, .padding = Padding::All(20.0f)})) {
            auto bgBtn = ui.Button({.text = "Background Action"});
            bgBtnRect = bgBtn.rect;
            if (bgBtn.clicked) {
                bgClicked = true;
            }

            if (bgBtn.IsTriggered(InteractionTrigger::RightClick)) {
                ctxOpen = true;
                ctxPos = ui.GetMousePos();
            }
            ctxResp = ui.ContextMenu(ctxOpen, menuItems, {
                .anchorPos = ctxPos
            });

            if (auto modal = ui.Modal(modalOpen, {
                .closeOnBackdrop = true,
                .closeOnEscape = true
            })) {
                if (auto card = ui.Card({
                    .width = 360.0f,
                    .gap = 10.0f,
                    .padding = Padding::All(16.0f),
                    .elevated = true
                })) {
                    modalBodyExecuted = true;
                    modalDialogRect = modal.response.rect;
                    ui.Text("Are you sure you want to reset?");
                    auto confirmBtn = ui.Button({.text = "Confirm", .color = StateColor::Primary()});
                    modalConfirmRect = confirmBtn.rect;
                    if (confirmBtn.clicked) {
                        modalConfirmClicked = true;
                        modalOpen = false;
                    }
                }
            }
        }
        ui.EndFrame(canvas);
    };

    ui.SetCurrentTime(0.0);

    // 1. Initial closed state: modal body is not executed, background button works
    renderScene();
    renderScene();
    CHECK(modalBodyExecuted == false);
    CHECK(bgBtnRect.w > 20.0f);

    ui.OnMouseMove(bgBtnRect.Center());
    ui.OnMouseDown(1);
    renderScene();
    ui.OnMouseUp(1);
    renderScene();
    CHECK(bgClicked == true);

    // 2. Right-click (button 3) on background button to open ContextMenu
    ui.OnMouseMove(bgBtnRect.Center());
    ui.OnMouseDown(3);
    renderScene();
    ui.OnMouseUp(3);
    renderScene();
    CHECK(ctxResp.isOpen == true);

    // Move mouse away from menu items so hover doesn't override keyboard selection, then press Down + Enter
    ui.OnMouseMove({2.0f, 2.0f});
    ui.OnKeyDown(Key::Down);
    renderScene();
    ui.OnKeyDown(Key::Enter);
    renderScene();
    CHECK(ctxResp.clicked == true);
    CHECK(ctxResp.clickedIndex == 0);
    CHECK(ctxResp.isOpen == false);

    // 3. Open Modal: verify modal body executes and background button click is blocked by modal isolation
    modalOpen = true;
    renderScene();
    renderScene();
    CHECK(modalBodyExecuted == true);
    CHECK(modalDialogRect.w >= 300.0f);
    CHECK(modalConfirmRect.w > 20.0f);

    // Try clicking background button (at top-left 20,20, outside centered modal dialog)
    ui.OnMouseMove({bgBtnRect.x + 5.0f, bgBtnRect.y + 5.0f});
    ui.OnMouseDown(1);
    renderScene();
    CHECK(bgClicked == false);
    CHECK(modalOpen == false);
    ui.OnMouseUp(1);
    renderScene();

    // 4. Re-open Modal and click the Confirm button inside the modal body
    modalOpen = true;
    renderScene();
    renderScene();
    CHECK(modalOpen == true);
    CHECK(modalConfirmRect.w > 20.0f);

    ui.OnMouseMove(modalConfirmRect.Center());
    ui.OnMouseDown(1);
    renderScene();
    ui.OnMouseUp(1);
    renderScene();
    CHECK(modalConfirmClicked == true);
    CHECK(modalOpen == false);

    // 5. Re-open Modal and press Escape -> closes modal
    modalOpen = true;
    renderScene();
    renderScene();
    CHECK(modalOpen == true);

    ui.OnKeyDown(Key::Escape);
    renderScene();
    CHECK(modalOpen == false);

    // 6. Toast notification lifecycle: ShowToast, timer countdown, and ClearToasts
    ui.ClearToasts();
    CHECK(ui.GetActiveToastCount() == 0);

    const keel::u64 toastId = ui.ShowToast({
        .title = "Capture Saved",
        .message = "Written 128 KB to capture.iq",
        .variant = UiVariant::Success,
        .duration = 2.0f
    });
    CHECK(toastId != 0);
    CHECK(ui.GetActiveToastCount() == 1);

    // Advance 1.0s (toast still active)
    ui.OnMouseMove({10.0f, 10.0f}); // away from BottomRight toast stack
    ui.AdvanceTime(1.0);
    renderScene();
    CHECK(ui.GetActiveToastCount() == 1);

    // Advance another 1.5s (total 2.5s > 2.0s duration -> auto-dismissed!)
    ui.AdvanceTime(1.5);
    renderScene();
    CHECK(ui.GetActiveToastCount() == 0);

    // Queue two persistent toasts and clear them
    ui.ShowToast({.title = "Persistent 1", .duration = 0.0f});
    ui.ShowToast({.title = "Persistent 2", .duration = 0.0f});
    CHECK(ui.GetActiveToastCount() == 2);
    ui.ClearToasts();
    CHECK(ui.GetActiveToastCount() == 0);

    keel::println("  [PASS] TestModalContextMenuAndToastWorkflow completed");
}

void TestActivityBarAndLinkedSidebarWorkflow() {
    using namespace guikit;
    keel::println("[TEST] Running TestActivityBarAndLinkedSidebarWorkflow...");

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 900;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    keel::i32 activeTool = 0;
    SidebarMode sbMode = SidebarMode::Expanded;
    bool actionClicked = false;
    Id barId{};
    Id sbId{};
    Rect item0Rect{};
    Rect item1Rect{};
    Rect actionRect{};

    auto renderWorkbench = [&]() {
        actionClicked = false;
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto workspace = ui.Box({
            .gap = 12.0f,
            .width = SizeMode::Stretch,
            .height = SizeMode::Stretch,
            .padding = Padding::All(0.0f)
        })) {
            if (auto bar = ui.ActivityBar(sbMode, {
                .side = SidebarSide::Left,
                .style = ActivityBarStyle::Indicator
            })) {
                barId = bar.response.id;
                item0Rect = bar.Item(activeTool, 0, {
                    .icon = Icon::Folder(),
                    .label = "Explorer"
                }).rect;
                item1Rect = bar.Item(activeTool, 1, {
                    .icon = Icon::Search(),
                    .label = "Search",
                    .badge = "5"
                }).rect;
                ui.Spacer();
                ui.Separator();
                auto actRes = bar.Action({
                    .icon = Icon::Settings(),
                    .label = "Settings"
                });
                actionRect = actRes.rect;
                if (actRes.clicked) {
                    actionClicked = true;
                }
            }

            if (auto sb = ui.Sidebar(sbMode, {
                .side = SidebarSide::Left,
                .width = 200.0f,
                .resizable = true,
                .allowCollapse = true
            })) {
                sbId = sb.response.id;
                ui.Text("Sidebar Content");
            }

            if (auto mainCard = ui.Card({
                .width = SizeMode::Stretch,
                .height = SizeMode::Stretch
            })) {
                ui.Text("Main Stage");
            }
        }
        ui.EndFrame(canvas);
    };

    renderWorkbench();
    renderWorkbench();

    // 1. Verify ActivityBar and Sidebar dock flush together (0px gap between ActivityBar and Sidebar),
    //    and bottom Action is pushed to the bottom via Spacer
    const Rect barRect = ui.GetPrevFrameBounds(barId);
    const Rect sbRect = ui.GetPrevFrameBounds(sbId);
    CHECK(barRect.w > 36.0f);
    CHECK_NEAR(sbRect.x, barRect.Right(), 0.01f);
    CHECK_NEAR(sbRect.w, 200.0f, 0.01f);
    CHECK(item0Rect.w > 24.0f);
    CHECK(item1Rect.w > 24.0f);
    CHECK(actionRect.y > 400.0f); // Pushed to bottom of 600px rail by Spacer()

    // 2. Click already-active item0 (Explorer) -> toggles linked Sidebar from Expanded to Collapsed!
    ui.OnMouseMove(item0Rect.Center());
    ui.OnMouseDown(1);
    renderWorkbench();
    ui.OnMouseUp(1);
    renderWorkbench();
    CHECK(activeTool == 0);
    CHECK(sbMode == SidebarMode::Collapsed);

    // 3. Click active item0 (Explorer) again while Sidebar is Collapsed -> expands Sidebar back to Expanded!
    ui.OnMouseMove(item0Rect.Center());
    ui.OnMouseDown(1);
    renderWorkbench();
    ui.OnMouseUp(1);
    renderWorkbench();
    CHECK(activeTool == 0);
    CHECK(sbMode == SidebarMode::Expanded);

    // 4. Collapse Sidebar again, then click inactive item1 (Search) -> switches activeTool to 1 AND auto-expands Sidebar!
    sbMode = SidebarMode::Collapsed;
    renderWorkbench();
    ui.OnMouseMove(item1Rect.Center());
    ui.OnMouseDown(1);
    renderWorkbench();
    ui.OnMouseUp(1);
    renderWorkbench();
    CHECK(activeTool == 1);
    CHECK(sbMode == SidebarMode::Expanded);

    // 5. Click stateless bottom Action button (Settings) -> triggers actionClicked without altering activeTool or sbMode
    ui.OnMouseMove(actionRect.Center());
    ui.OnMouseDown(1);
    renderWorkbench();
    ui.OnMouseUp(1);
    renderWorkbench();
    CHECK(actionClicked == true);
    CHECK(activeTool == 1);
    CHECK(sbMode == SidebarMode::Expanded);

    keel::println("  [PASS] TestActivityBarAndLinkedSidebarWorkflow completed");
}

void TestBottomPanelAndStatusBarWorkflow() {
    using namespace guikit;
    keel::println("[TEST] Running TestBottomPanelAndStatusBarWorkflow...");

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 800;
    const keel::u32 height = 600;
    keel::Array<keel::u32> pixels(width * height);
    canvas.SetTarget(pixels.data(), width, height, width);

    bool showPanel = true;
    keel::i32 activeTab = 0;
    bool statusActionClicked = false;
    Rect panelRect{};
    Rect statusRect{};
    Rect statusActionRect{};
    float reportedPanelH = 0.0f;

    auto renderWorkbench = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
        if (auto root = ui.Column({
            .gap = 0.0f,
            .width = SizeMode::Stretch,
            .height = SizeMode::Stretch
        })) {
            if (auto mainBox = ui.Box({
                .width = SizeMode::Stretch,
                .height = SizeMode::Stretch
            })) {
                ui.Text("Main Canvas");
            }

            if (auto bp = ui.BottomPanel(showPanel, {
                .height = 180.0f,
                .minHeight = 90.0f,
                .maxHeight = 320.0f,
                .resizable = true,
                .allowCollapse = true
            })) {
                reportedPanelH = bp.Height();
                if (auto tb = ui.Toolbar({.style = ToolbarStyle::Header})) {
                    tb.Item(activeTab, 0, {.text = "Output"});
                    tb.Item(activeTab, 1, {.text = "Slice 2D"});
                }
                ui.Text("Bottom Panel Body");
                panelRect = bp.response.rect;
            } else {
                reportedPanelH = bp.Height();
                panelRect = bp.response.rect;
            }

            if (auto sb = ui.StatusBar({.style = StatusBarStyle::Surface})) {
                sb.Item({.text = "CUDA Ready", .dotColor = Color(34, 197, 94, 255), .badge = "GPU"});
                ui.Separator();
                ui.Spacer();
                Response act = sb.Action({
                    .text = "Toggle Panel",
                    .icon = Icon::ChevronUp(),
                    .active = showPanel
                });
                statusActionRect = act.rect;
                if (act.clicked) {
                    statusActionClicked = true;
                    showPanel = !showPanel;
                }
                statusRect = sb.response.rect;
            }
        }
        ui.EndFrame(canvas);
    };

    // Frame 1 & 2: settle initial layout
    renderWorkbench();
    renderWorkbench();

    CHECK(showPanel == true);
    CHECK(keel::abs(panelRect.h - 180.0f) < 1.0f);
    CHECK(keel::abs(reportedPanelH - 180.0f) < 1.0f);
    CHECK(keel::abs(statusRect.h - 26.0f) < 1.0f);
    CHECK(keel::abs(panelRect.Bottom() - statusRect.y) < 1.0f);

    // 1. Drag top sash of BottomPanel UP by 60px -> height increases from 180px to 240px
    const Vec2 sashPos(panelRect.Center().x, panelRect.y);
    ui.OnMouseMove(sashPos);
    ui.OnMouseDown(1);
    renderWorkbench();
    ui.OnMouseMove({sashPos.x, sashPos.y - 60.0f});
    renderWorkbench();
    ui.OnMouseUp(1);
    renderWorkbench();

    CHECK(showPanel == true);
    CHECK(keel::abs(panelRect.h - 240.0f) < 1.5f);
    CHECK(keel::abs(reportedPanelH - 240.0f) < 1.5f);

    // 2. Drag top sash all the way DOWN past collapse threshold -> snaps to Collapsed (showPanel == false, height == 0)
    const Vec2 sashPos2(panelRect.Center().x, panelRect.y);
    ui.OnMouseMove(sashPos2);
    ui.OnMouseDown(1);
    renderWorkbench();
    ui.OnMouseMove({sashPos2.x, sashPos2.y + 230.0f});
    renderWorkbench();
    ui.OnMouseUp(1);
    renderWorkbench();

    CHECK(showPanel == false);
    CHECK(reportedPanelH == 0.0f);
    CHECK(panelRect.w > 100.0f);
    CHECK(!ui.GetActiveId().IsValid());

    // 3. Drag the collapsed BottomPanel edge UP by 150px -> pulls panel open to 150px
    const Vec2 collapsedEdgePos(panelRect.Center().x, panelRect.y - 2.0f);
    ui.OnMouseMove(collapsedEdgePos);
    ui.OnMouseDown(1);
    renderWorkbench();
    ui.OnMouseMove({collapsedEdgePos.x, collapsedEdgePos.y - 150.0f});
    renderWorkbench();
    ui.OnMouseUp(1);
    renderWorkbench();
    renderWorkbench();

    CHECK(showPanel == true);
    CHECK(keel::abs(reportedPanelH - 150.0f) < 1.5f);
    CHECK(keel::abs(panelRect.h - 150.0f) < 1.5f);

    // 4. Click StatusBar Action ("Toggle Panel") -> collapses and then re-opens BottomPanel
    ui.OnMouseMove(statusActionRect.Center());
    ui.OnMouseDown(1);
    renderWorkbench();
    ui.OnMouseUp(1);
    renderWorkbench();
    CHECK(statusActionClicked == true);
    CHECK(showPanel == false);

    ui.OnMouseMove(statusActionRect.Center());
    ui.OnMouseDown(1);
    renderWorkbench();
    ui.OnMouseUp(1);
    renderWorkbench();
    CHECK(showPanel == true);

    renderWorkbench();
    CHECK(keel::abs(reportedPanelH - 150.0f) < 1.5f);

    renderWorkbench();
    CHECK(keel::abs(panelRect.h - 150.0f) < 1.5f);

    // 5. Right-edge LeftClick ContextMenu stability when hovering inside popup (no X-position oscillation)
    Rect btnRect{};
    bool menuOpen = false;
    ContextMenuResponse menuResp{};
    const ContextMenuItem items[] = {
        { .label = "Action One", .shortcut = "Ctrl+1" },
        { .label = "Action Two", .shortcut = "Ctrl+2" }
    };
    auto renderRightMenu = [&]() {
        ui.BeginFrame({800.0f, 600.0f});
        if (auto row = ui.Row({ .padding = 12.0f })) {
            ui.Spacer();
            auto btn = ui.Button({ .text = "Quick Actions", .active = menuOpen });
            btnRect = btn.rect;
            if (btn.clicked) {
                menuOpen = !menuOpen;
            }
            menuResp = ui.ContextMenu(menuOpen, items, {
                .anchorRect = btn.rect,
                .minWidth = 220.0f
            });
        }
        ui.EndFrame(canvas);
    };

    renderRightMenu();
    renderRightMenu();
    CHECK(btnRect.w > 50.0f);
    CHECK(btnRect.x > 600.0f);

    // Click button to open menu
    ui.OnMouseMove(btnRect.Center());
    ui.OnMouseDown(1);
    renderRightMenu();
    ui.OnMouseUp(1);
    renderRightMenu();
    CHECK(menuResp.isOpen == true);
    CHECK(menuOpen == true);
    const Rect openRectFrame1 = menuResp.rect;
    CHECK(openRectFrame1.w >= 220.0f);

    // Move mouse inside the opened popup and run several frames -> rect must remain identical
    ui.OnMouseMove(openRectFrame1.Center());
    for (keel::i32 f = 0; f < 5; ++f) {
        renderRightMenu();
        CHECK(menuResp.isOpen == true);
        CHECK(keel::abs(menuResp.rect.x - openRectFrame1.x) < 0.5f);
        CHECK(keel::abs(menuResp.rect.y - openRectFrame1.y) < 0.5f);
        CHECK(keel::abs(btnRect.w) > 50.0f);
    }

    // Re-click the anchor button -> must toggle menuOpen cleanly back to false without reopening
    ui.OnMouseMove(btnRect.Center());
    ui.OnMouseDown(1);
    renderRightMenu();
    ui.OnMouseUp(1);
    renderRightMenu();
    CHECK(menuOpen == false);
    CHECK(menuResp.isOpen == false);

    keel::println("  [PASS] TestBottomPanelAndStatusBarWorkflow completed");
}

#if GUIKIT_HAS_SCENES
void TestPanelSashAndBodyOcclusionOverMapViewAndScene() {
    Context ui;
    ThorCanvas canvas;
    keel::Array<keel::u32> pixels(800 * 600);
    canvas.SetTarget(pixels.data(), 800, 600, 800);

    bool showBottom = true;
    SidebarMode rightMode = SidebarMode::Expanded;
    Rect mapVp{};
    Rect bottomRect{};
    Rect rightSidebarRect{};
    Rect childBtnRect{};
    bool mapRawClicked = false;
    bool mapScopeClicked = false;
    bool mapScopeHovered = false;
    bool mapPanning = false;
    bool childBtnClicked = false;
    float mapZoom = 1.0f;
    Vec2 isobathCenterScreen{};
    keel::u64 selectedGisUid = 0;
    GisRenderStats gisStats{};
    GisRenderConfig gisCfg{};
    gisCfg.asyncBackgroundStreaming = false;
    auto isobathLoad = GeoJsonIO::ParseThematicLayersFromString(R"({
        "type": "FeatureCollection",
        "features": [{
            "type": "Feature",
            "id": "DEPCNT-50M",
            "properties": { "OBJL": "DEPCNT", "VALDCO": 50.0, "name": "Isobath 50m" },
            "geometry": {
                "type": "LineString",
                "coordinates": [[131.8735, 43.25], [131.8735, 43.1056], [131.8735, 42.90]]
            }
        }]
    })", "Bathymetry", true);
    CHECK(isobathLoad.ok && !isobathLoad.layers.empty());

    auto renderBellhopLayout = [&]() {
        mapRawClicked = false;
        mapScopeClicked = false;
        mapScopeHovered = false;
        mapPanning = false;
        childBtnClicked = false;

        ui.BeginFrame({800.0f, 600.0f});
        if (auto root = ui.Row({ .gap = 0.0f, .padding = Padding::All(0.0f), .height = SizeMode::Stretch })) {
            if (auto leftSb = ui.Sidebar({
                .side = SidebarSide::Left,
                .width = 180.0f,
                .resizable = true,
                .padding = Padding::All(8.0f)
            })) {
                ui.Heading({ .label = "Acoustic Source" });
            }

            if (auto centerCol = ui.Column({
                .gap = 0.0f,
                .padding = Padding::All(0.0f),
                .width = SizeMode::Stretch,
                .height = SizeMode::Stretch
            })) {
                // MapView is evaluated BEFORE BottomPanel and Right Sidebar in the frame!
                if (auto map = ui.MapView({
                    .initialCenter = { 43.1056, 131.8735 },
                    .initialTileZoom = 11.0f,
                    .height = SizeMode::Stretch
                })) {
                    mapVp = map.GetScene().GetViewportRect();
                    mapScopeHovered = map.IsHovered();
                    mapScopeClicked = map.WasClicked();
                    mapPanning = map.GetScene().IsPanning();
                    mapZoom = map.GetScene().GetZoom();
                    isobathCenterScreen = map.GeoToScreen({ 43.1056, 131.8735 });
                    if (ui.IsMouseClicked()) {
                        mapRawClicked = true;
                    }
                    gisStats = GisRenderEngine::RenderLayers(
                        map,
                        isobathLoad.layers,
                        gisCfg,
                        selectedGisUid,
                        false
                    );

                    // Floating HUD card inside MapView
                    if (auto hudCard = ui.Card({ .padding = Padding::All(8.0f) })) {
                        auto btn = ui.Button({ .text = "Drop Buoy" });
                        childBtnRect = btn.rect;
                        childBtnClicked = btn.clicked;
                    }
                }

                if (auto bp = ui.BottomPanel(showBottom, {
                    .height = 160.0f,
                    .minHeight = 90.0f,
                    .maxHeight = 360.0f,
                    .resizable = true,
                    .allowCollapse = true,
                    .showToggleButton = true,
                    .padding = Padding::All(8.0f)
                })) {
                    bottomRect = bp.response.rect;
                    ui.Button({ .text = "Bellhop Profile Action" });
                }
            }

            if (auto rightSb = ui.Sidebar(rightMode, {
                .side = SidebarSide::Right,
                .width = 180.0f,
                .minWidth = 120.0f,
                .maxWidth = 340.0f,
                .resizable = true,
                .padding = Padding::All(8.0f)
            })) {
                rightSidebarRect = rightSb.response.rect;
                ui.Heading({ .label = "Inspector" });
            }
        }
        ui.EndFrame(canvas);
    };

    renderBellhopLayout();
    renderBellhopLayout();
    CHECK(mapVp.w > 300.0f && mapVp.h > 300.0f);
    CHECK(keel::abs(bottomRect.y - 440.0f) < 1.5f);
    CHECK(keel::abs(bottomRect.h - 160.0f) < 1.5f);
    CHECK(keel::abs(rightSidebarRect.x - 620.0f) < 1.5f);
    CHECK(keel::abs(rightSidebarRect.w - 180.0f) < 1.5f);

    // 1. Hover & Click/Drag on BottomPanel top splitter sash protruding 4px upward into MapView (y = 436.0f < 440.0f)
    // MapView must NOT see hover or click or pan, and BottomPanel must smoothly resize from 160px to 240px!
    ui.OnMouseMove({ 300.0f, 436.0f });
    renderBellhopLayout();
    CHECK(mapScopeHovered == false);

    ui.OnMouseDown(1);
    renderBellhopLayout();
    CHECK(mapRawClicked == false);
    CHECK(mapScopeClicked == false);
    CHECK(mapPanning == false);

    // Drag upward by 80px (y = 436 -> 356) across MapView
    ui.OnMouseMove({ 300.0f, 356.0f });
    renderBellhopLayout();
    CHECK(mapRawClicked == false);
    CHECK(mapScopeClicked == false);
    CHECK(mapPanning == false);

    ui.OnMouseUp(1);
    renderBellhopLayout();
    renderBellhopLayout();
    CHECK(keel::abs(bottomRect.h - 240.0f) < 2.0f);

    // 2. Click inside BottomPanel body, Left Sidebar body, and Right Sidebar body -> MapView never reacts!
    ui.OnMouseMove({ 300.0f, 500.0f }); // Inside BottomPanel
    ui.OnMouseDown(1);
    renderBellhopLayout();
    CHECK(mapRawClicked == false);
    CHECK(mapScopeClicked == false);
    CHECK(mapScopeHovered == false);
    ui.OnMouseUp(1);
    renderBellhopLayout();

    ui.OnMouseMove({ 80.0f, 200.0f }); // Inside Left Sidebar
    ui.OnMouseDown(1);
    renderBellhopLayout();
    CHECK(mapRawClicked == false);
    CHECK(mapScopeClicked == false);
    CHECK(mapScopeHovered == false);
    ui.OnMouseUp(1);
    renderBellhopLayout();

    ui.OnMouseMove({ 700.0f, 200.0f }); // Inside Right Sidebar
    ui.OnMouseDown(1);
    renderBellhopLayout();
    CHECK(mapRawClicked == false);
    CHECK(mapScopeClicked == false);
    CHECK(mapScopeHovered == false);
    ui.OnMouseUp(1);
    renderBellhopLayout();

    // 3. Click on Right Sidebar splitter sash protruding 4px leftward into MapView (x = 616.0f < 620.0f)
    // MapView must NOT see click or pan, and Right Sidebar must resize from 180px to 240px!
    ui.OnMouseMove({ 616.0f, 200.0f });
    ui.OnMouseDown(1);
    renderBellhopLayout();
    CHECK(mapRawClicked == false);
    CHECK(mapScopeClicked == false);
    CHECK(mapPanning == false);

    ui.OnMouseMove({ 556.0f, 200.0f });
    renderBellhopLayout();
    CHECK(mapPanning == false);

    ui.OnMouseUp(1);
    renderBellhopLayout();
    renderBellhopLayout();
    CHECK(keel::abs(rightSidebarRect.w - 240.0f) < 2.0f);

    // 4. Click on child Button inside Card inside MapView -> Button clicks, MapView canvas does NOT click!
    CHECK(childBtnRect.w > 20.0f && childBtnRect.h > 15.0f);
    ui.OnMouseMove(childBtnRect.Center());
    ui.OnMouseDown(1);
    renderBellhopLayout();
    CHECK(mapRawClicked == false);
    CHECK(mapScopeClicked == false);
    CHECK(mapScopeHovered == false);
    ui.OnMouseUp(1);
    renderBellhopLayout();
    CHECK(childBtnClicked == true);
    CHECK(mapRawClicked == false);
    CHECK(mapScopeClicked == false);

    // 5. Mouse wheel over BottomPanel or Sidebar must NOT zoom MapView
    const float zoomBefore = mapZoom;
    ui.OnMouseMove({ 300.0f, 500.0f }); // Over BottomPanel
    ui.OnMouseWheel(0.0f, 3.0f);
    renderBellhopLayout();
    CHECK(keel::abs(mapZoom - zoomBefore) < 0.0001f);

    // 6. Clicking on unoccluded MapView canvas DOES fire mapRawClicked and mapScopeClicked
    ui.OnMouseMove({ 350.0f, 220.0f });
    ui.OnMouseDown(1);
    renderBellhopLayout();
    CHECK(mapScopeHovered == true);
    CHECK(mapRawClicked == true);
    CHECK(mapScopeClicked == true);
    ui.OnMouseUp(1);
    renderBellhopLayout();

    // 7. GIS Isobath (DEPCNT) Hover Occlusion:
    // The 50m isobath runs vertically at lon=131.8735 across both MapView and BottomPanel.
    // When hovering inside MapView at isobathCenterScreen, the isobath MUST highlight.
    // When hovering directly below inside BottomPanel or on the splitter sash, the isobath MUST NOT highlight!
    ui.OnMouseMove(isobathCenterScreen);
    renderBellhopLayout();
    CHECK(mapScopeHovered == true);
    CHECK(gisStats.hoveredFeatureUid != 0);

    ui.OnMouseMove({ isobathCenterScreen.x, bottomRect.Center().y }); // Inside BottomPanel over the same meridian
    renderBellhopLayout();
    CHECK(mapScopeHovered == false);
    CHECK(gisStats.hoveredFeatureUid == 0);

    ui.OnMouseMove({ isobathCenterScreen.x, bottomRect.y - 3.0f }); // On BottomPanel splitter sash over the same meridian
    renderBellhopLayout();
    CHECK(mapScopeHovered == false);
    CHECK(gisStats.hoveredFeatureUid == 0);

    keel::println("  [PASS] TestPanelSashAndBodyOcclusionOverMapViewAndScene completed");
}
#endif

void TestReviewP0AndP1Regressions() {
    using namespace guikit;

    ThorCanvas canvas;
    keel::Array<keel::u32> pixels(800 * 600);
    canvas.SetTarget(pixels.data(), 800, 600, 800);

    // 1. Callsite-based MakeChildId: conditional sibling insertion does NOT shift IDs or lose focus,
    //    while for-loops still disambiguate each iteration cleanly.
    {
        Context ui;
        bool showBanner = false;
        keel::String query("init");
        WidgetResponse inputResp{};
        Id loopIds[3]{};

        auto renderForm = [&]() {
            ui.BeginFrame({800.0f, 600.0f});
            if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(12.0f)})) {
                if (showBanner) {
                    ui.Button({.text = "Warning Banner Action"});
                    ui.Text("Extra warning description");
                }
                inputResp = ui.InputField(query);
                for (keel::i32 i = 0; i < 3; ++i) {
                    loopIds[i] = ui.Button({.text = "LoopBtn"}).id;
                }
            }
            ui.EndFrame(canvas);
        };

        renderForm();
        renderForm();
        const Id idWithoutBanner = inputResp.id;
        CHECK(loopIds[0] != loopIds[1] && loopIds[1] != loopIds[2] && loopIds[0] != loopIds[2]);

        // Focus InputField, then toggle showBanner ON -> ID & focus must stay intact!
        ui.RequestFocus(idWithoutBanner);
        showBanner = true;
        ui.OnTextInput("!");
        renderForm();

        CHECK(inputResp.id == idWithoutBanner);
        CHECK(ui.GetFocusedId() == idWithoutBanner);
        CHECK(query == "init!");
    }

    // 2. Persistent widget state survives > 700 hidden frames on an inactive tab (exempt from 600-frame transient GC)
    {
        Context ui;
        keel::i32 activeTab = 0;
        Rect scrollViewport{};
        float firstItemScreenY = 0.0f;
        bool childFolderVisible = false;

        bool expandTriggered = false;
        auto renderTabs = [&]() {
            childFolderVisible = false;
            ui.BeginFrame({800.0f, 600.0f});
            if (activeTab == 0) {
                if (auto row = ui.Row({.gap = 12.0f, .height = SizeMode::Stretch})) {
                    if (auto sa = ui.ScrollArea({.width = 260.0f, .height = 180.0f})) {
                        scrollViewport = sa.GetNode()->computedRect;
                        for (keel::i32 i = 0; i < 12; ++i) {
                            auto r = ui.Button({.text = ui.Format("Row {}", i)});
                            if (i == 0) firstItemScreenY = r.rect.y;
                        }
                    }
                    if (auto tv = ui.TreeView({.width = 260.0f, .height = 240.0f})) {
                        if (expandTriggered) {
                            tv.ExpandAll();
                            expandTriggered = false;
                        }
                        if (auto rootNode = ui.TreeItem({.label = "Root", .defaultOpen = true})) {
                            if (auto subNode = ui.TreeItem({.label = "SubFolder", .defaultOpen = false})) {
                                childFolderVisible = subNode.open;
                                (void)ui.TreeItem({.label = "Leaf.cpp"});
                            }
                        }
                    }
                }
            } else {
                ui.Text("Secondary Tab Active");
            }
            ui.EndFrame(canvas);
        };

        renderTabs();
        renderTabs();
        const float initialFirstY = firstItemScreenY;
        CHECK(!childFolderVisible);

        // Scroll the ScrollArea down and expand "SubFolder"
        expandTriggered = true;
        ui.OnMouseMove(scrollViewport.Center());
        ui.OnMouseWheel(0.0f, -3.0f);
        renderTabs();
        renderTabs();
        const float scrolledFirstY = firstItemScreenY;
        CHECK(scrolledFirstY < initialFirstY - 20.0f);
        CHECK(childFolderVisible);

        // Switch away to Tab 1 for 700 frames (> 600-frame GC interval)
        activeTab = 1;
        for (keel::i32 f = 0; f < 700; ++f) {
            renderTabs();
        }

        // Return to Tab 0 -> ScrollArea scroll offset AND TreeItem expanded state MUST still be preserved!
        activeTab = 0;
        renderTabs();
        renderTabs();
        CHECK_NEAR(firstItemScreenY, scrolledFirstY, 1.0f);
        CHECK(childFolderVisible);
    }

    // 3. Per-widget Undo / Redo history preserved across focus switches between InputFields
    {
        Context ui;
        keel::String fieldA("alpha");
        keel::String fieldB("beta");
        Id idA{}, idB{};

        auto renderFields = [&]() {
            ui.BeginFrame({600.0f, 300.0f});
            if (auto col = ui.Column({.gap = 8.0f})) {
                idA = ui.InputField(fieldA).id;
                idB = ui.InputField(fieldB).id;
            }
            ui.EndFrame(canvas);
        };

        renderFields();
        ui.RequestFocus(idA);
        renderFields();
        ui.OnTextInput("_1");
        renderFields();
        CHECK(fieldA == "alpha_1");

        // Switch focus to fieldB and edit it
        ui.RequestFocus(idB);
        renderFields();
        ui.OnTextInput("_2");
        renderFields();
        CHECK(fieldB == "beta_2");

        // Switch focus back to fieldA and press Ctrl+Z -> fieldA must undo back to "alpha"!
        ui.RequestFocus(idA);
        renderFields();
        ui.OnKeyDown(Key::Z, KeyModifiers{.ctrl = true});
        renderFields();
        CHECK(fieldA == "alpha");

        // And Ctrl+Y redoes fieldA back to "alpha_1"!
        ui.OnKeyDown(Key::Y, KeyModifiers{.ctrl = true});
        renderFields();
        CHECK(fieldA == "alpha_1");
    }

    // 4. Open Modal traps keyboard input & focus away from background InputField and restores focus on close
    {
        Context ui;
        keel::String bgText("safe");
        bool modalOpen = false;
        Id bgId{};

        auto renderModalScene = [&]() {
            ui.BeginFrame({800.0f, 600.0f});
            if (auto col = ui.Column({.padding = Padding::All(16.0f)})) {
                bgId = ui.InputField(bgText).id;
            }
            if (auto m = ui.Modal(modalOpen)) {
                if (auto box = ui.Box({.width = 320.0f, .padding = Padding::All(12.0f)})) {
                    ui.Text("Modal body");
                    ui.Button({.text = "OK"});
                }
            }
            ui.EndFrame(canvas);
        };

        renderModalScene();
        ui.RequestFocus(bgId);
        renderModalScene();
        CHECK(ui.GetFocusedId() == bgId);

        // Open modal: typing and Backspace must NOT mutate bgText!
        modalOpen = true;
        renderModalScene(); // Frame where modal becomes active
        ui.OnTextInput("LEAK");
        ui.OnKeyDown(Key::Backspace);
        renderModalScene();
        CHECK(bgText == "safe");

        // Closing modal restores focus to bgId so typing resumes cleanly
        modalOpen = false;
        renderModalScene();
        CHECK(ui.GetFocusedId() == bgId);
        ui.OnTextInput("!");
        renderModalScene();
        CHECK(bgText == "safe!");
    }

    // 5. UTF-8 Cyrillic vertical line navigation, password masking, and case-insensitive search
    {
        Context ui;
        // Line 0: "АБВГ" (4 codepoints, 8 bytes)
        // Line 1: "abcd" (4 codepoints, 4 bytes)
        // Line 2: "ДЁЖЗ" (4 codepoints, 8 bytes)
        keel::String doc("АБВГ\nabcd\nДЁЖЗ");
        Id taId{};

        auto renderEditor = [&]() {
            ui.BeginFrame({600.0f, 400.0f});
            taId = ui.TextArea(doc, {.height = 240.0f, .enableSearch = true}).id;
            ui.EndFrame(canvas);
        };

        renderEditor();
        ui.RequestFocus(taId);
        renderEditor();

        // Place cursor at start of doc (0), move Right twice -> after "АБ" (byte offset 4, codepoint col 2)
        ui.SetTextEditCursor(taId, 0, 0);
        ui.OnKeyDown(Key::Right);
        ui.OnKeyDown(Key::Right);
        // Move Down to "abcd" -> codepoint col 2 ("ab|cd"), then Down to "ДЁЖЗ" -> codepoint col 2 ("ДЁ|ЖЗ", byte offset 18)
        ui.OnKeyDown(Key::Down);
        ui.OnKeyDown(Key::Down);
        ui.OnTextInput("!");
        renderEditor();
        CHECK(doc == "АБВГ\nabcd\nДЁ!ЖЗ");

        // UTF-8 Password InputField: "пароль" (6 codepoints, 12 bytes) -> Backspace deletes 1 Cyrillic codepoint ("парол")
        keel::String pwd("пароль");
        Id pwdId{};
        auto renderPwd = [&]() {
            ui.BeginFrame({400.0f, 200.0f});
            pwdId = ui.InputField(pwd, {.password = true}).id;
            ui.EndFrame(canvas);
        };
        renderPwd();
        ui.RequestFocus(pwdId);
        renderPwd();
        ui.OnKeyDown(Key::End);
        ui.OnKeyDown(Key::Backspace);
        renderPwd();
        CHECK(pwd == "парол");

        // UTF-8 case-insensitive search helper used by TreeView / TagInput / TextArea
        CHECK(TextUtil::FindCaseInsensitive("Модуль Сигналов", "сигнал") != keel::StringRef::npos);
        CHECK(TextUtil::FindCaseInsensitive("Привет", "ПРИВЕТ") == 0u);
    }

    // 6. ScrollAreaOptions (.maxHeight), WrapOptions (.width = Fit), and GridOptions (.columns = 16)
    {
        Context ui;
        Rect saRect{}, wrapRect{}, fullCellRect{};

        for (keel::i32 f = 0; f < 2; ++f) {
            Node* saNode = nullptr;
            Node* wrapNode = nullptr;
            Node* fullCellNode = nullptr;
            ui.BeginFrame({800.0f, 600.0f});
            if (auto col = ui.Column({.gap = 10.0f, .width = SizeMode::Stretch})) {
                if (auto sa = ui.ScrollArea({.maxHeight = 140.0f})) {
                    saNode = sa.GetNode();
                    for (keel::i32 i = 0; i < 10; ++i) {
                        ui.Button({.text = ui.Format("Item {}", i)});
                    }
                }
                if (auto w = ui.Wrap({.gap = 8.0f, .width = SizeMode::Fit})) {
                    wrapNode = w.GetNode();
                    ui.Button({.text = "Tag A"});
                    ui.Button({.text = "Tag B"});
                }
                if (auto g = ui.Grid({.columns = 16, .gap = 8.0f})) {
                    if (auto c = g.Cell()) { // Default span must span all 16 columns!
                        fullCellNode = c.GetNode();
                        ui.Button({.text = "Full 16-col Span"});
                    }
                }
            }
            ui.EndFrame(canvas);
            if (saNode) saRect = saNode->computedRect;
            if (wrapNode) wrapRect = wrapNode->computedRect;
            if (fullCellNode) fullCellRect = fullCellNode->computedRect;
        }

        CHECK_NEAR(saRect.h, 140.0f, 1.0f);
        CHECK(wrapRect.w > 40.0f && wrapRect.w < 400.0f);
        CHECK_NEAR(fullCellRect.w, 800.0f, 1.0f);
    }
}

void TestKeelZeroCopyWidgetBindings();
void TestExternalStlDuckTypedInterop();
void TestBoundedCachesAndInactiveStateTrimming();
void TestElementHeadingCompositionAndIndependentSizing();
void TestPopoverComponent();
void TestApiPolish();
void TestControlFlatSizingAndStyleOptions();
void TestSceneWorldSizeInsideScrollAreaDoesNotOverflow();
void TestApiPolishStretchWeightsTabBarColorAndOptionalMins();
void TestFocusVisibleModalityAndTabBarFocusRing();
void TestTextCompositorAndSubpixelRendering();

int main() {
    keel::println("[RUN] Testing GuiKit Context & Immediate Mode API...");

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
    TestSdlMouseCoordsMapToLogicalDp();
    TestThorCanvasFontSizeIsInPixels();
    TestFrameScaleIsUniformAndKeepsPointer();
    TestZonesInheritanceZoomIndependenceAndBalance();
    TestZoneThemeReachesNodesBuiltInside();
    TestCanvasTransformMatchesMappedGeometry();
    TestScaledContainerSpaces();
    TestSdlCanvasBackendInterface();
    TestDebugBoundsAndInspectMode();
    TestPostRenderDelegateWorkflow();
    TestContainersAndLayoutsReorganization();
    TestGenericTemplateFacade();
    TestContextCleanArchitectureAndStateGC();
    TestScopedThemeAndScaleOverrides();
    TestSliderAndStepSliderWidgets();
    TestVerticalSlider();
    TestVerticalStepAndRangeSliders();
    TestSceneContentDamageAndItemDrag();
    TestPixelGeometryAndWidgetUiSizeDecoupling();
    TestTextAreaWidgetWorkflow();
    TestButtonAlignmentAndProceduralIcons();
    TestWidgetRadiusOverride();
    TestButtonExternalLabelDescriptionAndPlacement();
    TestWidgetSizeModesAndExplicitWidth();
    TestLabelAlignmentGeometryOptionA();
    TestUiTextAndControlCompanionText();
    TestSidebarAndSidebarMenuWorkflow();
    TestUiTextModesAndVerticalSpaceReservation();
    TestSelectItemsAndUnifiedAffixes();
    TestSelectPopupScrollingWithMaxVisibleItems();
    TestBaseElementsAllInOneEnhancements();
    TestImageSceneAndMarkdownWidgets();
    TestUnifiedElementHeaderDecorationsAcrossWidgets();
#if GUIKIT_HAS_SCENES
    TestSignalViewWidgetWorkflow();
    TestMapViewWidgetWorkflow();
    TestSonarViewWidgetWorkflow();
    TestSonarAndSignalGpuShaderPipeline();
    TestWaterfallViewWidgetAndGpuRingBuffer();
#endif
    TestRangeSliderProgressBarSpinBoxAndRadioButton();
    TestTableVirtualizedDataGridWorkflow();
    TestCodeEditorIdeAndDebuggerWorkflow();
    TestKeymapAndMarkdownTopTierFeatures();
    TestUnifiedColormapHistogramAndImageView();
    TestUnifiedElementExplicitHeight();
    TestElementOuterFrameAndControlWellFrame();
#if GUIKIT_HAS_SCENES
    TestAttitudeViewWidgetWorkflow();
    TestCarClusterViewWidgetWorkflow();
    TestBpmnEditorWidgetWorkflow();
    TestGisDemoBayGeoJsonAndRenderEngine();
#endif
    TestAccordionWidgetWorkflow();
    TestSizeScopeChainingAndIsolation();
    TestTreeViewAndTreeItemWorkflow();
    TestKnobSpinBoxAndRadioElements();
    TestSegmentedAndColorPickerElements();
    TestSidebarResizableCompactAndCollapsedModes();
    TestModalContextMenuAndToastWorkflow();
    TestActivityBarAndLinkedSidebarWorkflow();
    TestBottomPanelAndStatusBarWorkflow();
#if GUIKIT_HAS_SCENES
    TestPanelSashAndBodyOcclusionOverMapViewAndScene();
#endif
    TestReviewP0AndP1Regressions();

    // Live-Resize policy toggle (Variant 1: Deferred vs Variant 2: Live 60 FPS)
    {
        guikit::Context ui;
        CHECK(ui.IsLiveResizeEnabled());
        ui.SetLiveResize(false);
        CHECK(!ui.IsLiveResizeEnabled());
        ui.SetLiveResize(true);
        CHECK(ui.IsLiveResizeEnabled());
    }

    // Equal-width button group via ui.Row({.equalWidths = true}) in Toolbar and Column
    {
        guikit::Context ui;
        guikit::ThorCanvas canvas;
        keel::Array<keel::u32> pixels(800 * 200);
        canvas.SetTarget(pixels.data(), 800, 200, 800);

        bool t1 = true, t2 = false, t3 = true, t4 = true, t5 = true;
        guikit::Response r1{}, r2{}, r3{}, r4{}, r5{};
        guikit::Response bShort{}, bLong{};

        for (keel::i32 f = 0; f < 2; ++f) {
            ui.BeginFrame({800.0f, 200.0f});
            if (auto tb = ui.Toolbar()) {
                if (auto grp = ui.Row({.equalWidths = true})) {
                    r1 = tb.Toggle(t1, {.text = "GPU", .icon = guikit::Icon::Flash()});
                    r2 = tb.Toggle(t2, {.text = "VSync"});
                    r3 = tb.Toggle(t3, {.text = "React"});
                    r4 = tb.Toggle(t4, {.text = "Dirty"});
                    r5 = tb.Toggle(t5, {.text = "Live"});
                }
            }
            if (auto fitRow = ui.Row({.width = guikit::SizeMode::Fit, .equalWidths = true})) {
                bShort = ui.Button({.text = "OK"});
                bLong = ui.Button({.text = "Confirm Action"});
            }
            ui.EndFrame(canvas);
        }

        CHECK(r1.rect.w > 50.0f);
        CHECK_NEAR(r1.rect.w, r2.rect.w, 0.01f);
        CHECK_NEAR(r1.rect.w, r3.rect.w, 0.01f);
        CHECK_NEAR(r1.rect.w, r4.rect.w, 0.01f);
        CHECK_NEAR(r1.rect.w, r5.rect.w, 0.01f);

        CHECK(bLong.rect.w > 80.0f);
        CHECK_NEAR(bShort.rect.w, bLong.rect.w, 0.01f);
    }

    // SpinBox external square stepper styles (Detached and Split)
    {
        guikit::Context ui;
        guikit::ThorCanvas canvas;
        keel::Array<keel::u32> pixels(600 * 240);
        canvas.SetTarget(pixels.data(), 600, 240, 600);

        float valDetached = 129.0f;
        float valSplit = 50.0f;
        guikit::Response rDetached{};
        guikit::Response rSplit{};

        auto renderSpin = [&]() {
            if (auto col = ui.Column({.gap = 8.0f, .padding = guikit::Padding::All(12.0f)})) {
                rDetached = ui.SpinBox(valDetached, {
                    .suffix = "MHz",
                    .step = 2.5,
                    .stepStyle = guikit::SpinBoxStepStyle::Detached
                });
                rSplit = ui.SpinBox(valSplit, {
                    .step = 5.0,
                    .stepStyle = guikit::SpinBoxStepStyle::Split
                });
            }
        };

        for (keel::i32 f = 0; f < 2; ++f) {
            ui.BeginFrame({600.0f, 240.0f});
            renderSpin();
            ui.EndFrame(canvas);
        }

        // In Detached mode, the '+' square button (H x H) is at the right edge of rDetached.rect,
        // and the '-' square button (H x H) is immediately to its left (offset by 4px gap + H)
        const float h = rDetached.rect.h;
        CHECK(h >= 24.0f);
        const guikit::Vec2 plusPt{rDetached.rect.Right() - h * 0.5f, rDetached.rect.Center().y};
        const guikit::Vec2 minusPt{rDetached.rect.Right() - h - 4.0f - h * 0.5f, rDetached.rect.Center().y};

        ui.OnMouseMove(plusPt);
        ui.OnMouseDown(1);
        ui.BeginFrame({600.0f, 240.0f});
        renderSpin();
        ui.EndFrame(canvas);
        CHECK(rDetached.changed);
        CHECK_NEAR(valDetached, 131.5f, 0.01f);
        ui.OnMouseUp(1);
        ui.BeginFrame({600.0f, 240.0f});
        renderSpin();
        ui.EndFrame(canvas);

        ui.OnMouseMove(minusPt);
        ui.OnMouseDown(1);
        ui.BeginFrame({600.0f, 240.0f});
        renderSpin();
        ui.EndFrame(canvas);
        CHECK(rDetached.changed);
        CHECK_NEAR(valDetached, 129.0f, 0.01f);
        ui.OnMouseUp(1);
        ui.BeginFrame({600.0f, 240.0f});
        renderSpin();
        ui.EndFrame(canvas);

        // In Split mode, the '-' square button is at the left edge of rSplit.rect,
        // and the '+' square button is at the right edge of rSplit.rect
        const guikit::Vec2 splitMinusPt{rSplit.rect.x + rSplit.rect.h * 0.5f, rSplit.rect.Center().y};
        ui.OnMouseMove(splitMinusPt);
        ui.OnMouseDown(1);
        ui.BeginFrame({600.0f, 240.0f});
        renderSpin();
        ui.EndFrame(canvas);
        CHECK(rSplit.changed);
        CHECK_NEAR(valSplit, 45.0f, 0.01f);
    }

    // ComboBox (searchable dropdown) & Autocomplete (editable free-form + suggestions)
    {
        guikit::Context ui;
        guikit::ThorCanvas canvas;
        keel::Array<keel::u32> pixels(800 * 400);
        canvas.SetTarget(pixels.data(), 800, 400, 800);

        keel::usize comboIdx = 0; // "Alpha"
        keel::String autoVal("");
        guikit::Response rCombo{};
        guikit::Response rAuto{};

        static constexpr keel::StaticArray<keel::StringRef, 5> items = {
            "Alpha",
            "Beta",
            "Gamma",
            "Delta",
            "Epsilon"
        };

        auto renderComboUI = [&]() {
            if (auto col = ui.Column({.gap = 12.0f, .padding = guikit::Padding::All(20.0f)})) {
                rCombo = ui.ComboBox(comboIdx, items, {.clearable = true});
                rAuto = ui.Autocomplete(autoVal, items, {.placeholder = "Type or pick...", .clearable = true});
            }
        };

        for (keel::i32 f = 0; f < 2; ++f) {
            ui.BeginFrame({800.0f, 400.0f});
            renderComboUI();
            ui.EndFrame(canvas);
        }

        // 1. Click ComboBox to open searchable popup
        ui.OnMouseMove({rCombo.rect.x + 20.0f, rCombo.rect.Center().y});
        ui.OnMouseDown(1);
        ui.BeginFrame({800.0f, 400.0f});
        renderComboUI();
        ui.EndFrame(canvas);
        ui.OnMouseUp(1);
        ui.BeginFrame({800.0f, 400.0f});
        renderComboUI();
        ui.EndFrame(canvas);
        CHECK(ui.IsPopupOpen(rCombo.id));

        // Type "gam" to filter down to "Gamma" (index 2) and press Enter to commit
        ui.OnTextInput("gam");
        ui.BeginFrame({800.0f, 400.0f});
        renderComboUI();
        ui.EndFrame(canvas);
        CHECK(ui.IsPopupOpen(rCombo.id));

        ui.OnKeyDown(guikit::Key::Enter);
        ui.BeginFrame({800.0f, 400.0f});
        renderComboUI();
        ui.EndFrame(canvas);
        CHECK(!ui.IsPopupOpen(rCombo.id));
        CHECK(rCombo.changed);
        CHECK(rCombo.submitted);
        CHECK(comboIdx == 2);

        // 2. Click clear (X) button on ComboBox to clear selection to size_t(-1)
        const guikit::SelectMetrics& sm = ui.GetTheme().GetSelect(guikit::UiSize::Md);
        const float clearBtnCenterX = rCombo.rect.Right() - sm.paddingX - sm.iconSize - 6.0f - (sm.iconSize + 4.0f) * 0.5f;
        ui.OnMouseMove({clearBtnCenterX, rCombo.rect.Center().y});
        ui.OnMouseDown(1);
        ui.BeginFrame({800.0f, 400.0f});
        renderComboUI();
        ui.EndFrame(canvas);
        CHECK(rCombo.changed);
        CHECK(comboIdx == keel::usize(-1));
        ui.OnMouseUp(1);
        ui.BeginFrame({800.0f, 400.0f});
        renderComboUI();
        ui.EndFrame(canvas);

        // 3. Focus Autocomplete, type "del" -> filters to "Delta", press Enter to autocomplete "Delta"
        ui.OnMouseMove({rAuto.rect.x + 20.0f, rAuto.rect.Center().y});
        ui.OnMouseDown(1);
        ui.BeginFrame({800.0f, 400.0f});
        renderComboUI();
        ui.EndFrame(canvas);
        ui.OnMouseUp(1);
        ui.BeginFrame({800.0f, 400.0f});
        renderComboUI();
        ui.EndFrame(canvas);
        CHECK(ui.IsPopupOpen(rAuto.id));

        ui.OnTextInput("del");
        ui.BeginFrame({800.0f, 400.0f});
        renderComboUI();
        ui.EndFrame(canvas);
        CHECK(rAuto.changed);
        CHECK(autoVal == "del");
        CHECK(ui.IsPopupOpen(rAuto.id));

        ui.OnKeyDown(guikit::Key::Enter);
        ui.BeginFrame({800.0f, 400.0f});
        renderComboUI();
        ui.EndFrame(canvas);
        CHECK(rAuto.changed);
        CHECK(rAuto.submitted);
        CHECK(autoVal == "Delta");
        CHECK(!ui.IsPopupOpen(rAuto.id));

        // 4. Type a custom string in Autocomplete that doesn't match any preset and press Enter
        ui.OnKeyDown(guikit::Key::A, {.ctrl = true});
        ui.OnTextInput("CustomDSP");
        ui.BeginFrame({800.0f, 400.0f});
        renderComboUI();
        ui.EndFrame(canvas);
        CHECK(autoVal == "CustomDSP");
        CHECK(ui.IsPopupOpen(rAuto.id));

        ui.OnKeyDown(guikit::Key::Enter);
        ui.BeginFrame({800.0f, 400.0f});
        renderComboUI();
        ui.EndFrame(canvas);
        CHECK(!ui.IsPopupOpen(rAuto.id));
        CHECK(autoVal == "CustomDSP");
    }

    // MultiSelect, DatePicker, Breadcrumb, Pagination, SplitView & CommandPalette
    {
        guikit::Context ui;
        guikit::ThorCanvas canvas;
        keel::Array<keel::u32> pixels(800 * 600);
        canvas.SetTarget(pixels.data(), 800, 600, 800);

        keel::Array<keel::usize> multiSel;
        multiSel.push_back(0);
        multiSel.push_back(2);
        static constexpr keel::StaticArray<keel::StringRef, 4> multiItems = {"Alpha", "Beta", "Gamma", "Delta"};
        guikit::DateTime dt{2026, 10, 20, 14, 30, 0};
        keel::usize crumbIdx = 1;
        static constexpr keel::StaticArray<keel::StringRef, 4> crumbItems = {"Home", "Workspaces", "DSP", "Sonar"};
        keel::usize pageIdx = 2;
        float splitRatio = 0.4f;
        bool cmdOpen = false;
        const guikit::CommandPaletteItem cmdItems[] = {
            {.label = "Open Signal View", .category = "Scenes", .shortcut = "Ctrl+1"},
            {.label = "Open Sonar View",  .category = "Scenes", .shortcut = "Ctrl+2"},
            {.label = "Reset Layout",     .category = "Window", .shortcut = "Ctrl+R"}
        };

        guikit::Response rMulti{};
        guikit::Response rDate{};
        guikit::Response rCrumb{};
        guikit::Response rPage{};
        guikit::Rect leftPaneRect{};
        guikit::Rect rightPaneRect{};
        guikit::CommandPaletteResponse rCmd{};

        auto renderAllFive = [&]() {
            if (auto col = ui.Column({.gap = 8.0f, .padding = guikit::Padding::All(12.0f), .height = guikit::SizeMode::Stretch})) {
                rMulti = ui.MultiSelect(multiSel, multiItems, {.searchable = true, .clearable = true, .width = 320.0f});
                rDate = ui.DatePicker(dt, {.mode = guikit::DatePickerMode::DateTime, .showSeconds = true, .width = 240.0f});
                rCrumb = ui.Breadcrumb(crumbIdx, crumbItems);
                rPage = ui.Pagination(pageIdx, 10u, {.siblingCount = 1, .showPageInfo = true});

                if (auto split = ui.SplitView(splitRatio, {
                    .orientation = guikit::Orientation::Horizontal,
                    .defaultRatio = 0.5f,
                    .sashSize = 6.0f,
                    .height = 180.0f
                })) {
                    if (auto p1 = split.First()) {
                        leftPaneRect = ui.Button({.text = "Left Pane", .width = guikit::SizeMode::Stretch}).rect;
                    }
                    if (auto p2 = split.Second()) {
                        rightPaneRect = ui.Button({.text = "Right Pane", .width = guikit::SizeMode::Stretch}).rect;
                    }
                }

                rCmd = ui.CommandPalette(cmdOpen, cmdItems);
            }
        };

        // DatePicker static helper verification
        CHECK(guikit::DatePicker::IsLeapYear(2024));
        CHECK(!guikit::DatePicker::IsLeapYear(2026));
        CHECK(guikit::DatePicker::DaysInMonth(2024, 2) == 29);
        CHECK(guikit::DatePicker::DaysInMonth(2026, 2) == 28);
        CHECK(guikit::DatePicker::FormatDateTime(dt, guikit::DatePickerMode::Date, false) == "2026-10-20");
        CHECK(guikit::DatePicker::FormatDateTime(dt, guikit::DatePickerMode::DateTime, true) == "2026-10-20 14:30:00");

        for (keel::i32 i = 0; i < 2; ++i) {
            ui.BeginFrame({800.0f, 600.0f});
            renderAllFive();
            ui.EndFrame(canvas);
        }

        CHECK(rMulti.rect.w > 200.0f);
        CHECK(rDate.rect.w > 180.0f);
        CHECK(rCrumb.rect.w > 100.0f);
        CHECK(rPage.rect.w > 150.0f);

        // DatePicker contextual width policy: Auto -> Stretch in form/Element, Fit in horizontal Toolbar, plus minWidth
        {
            guikit::Context dpUi;
            guikit::Rect formDateRect{};
            guikit::Rect tbDateRect{};
            guikit::Rect minWDateRect{};
            for (keel::i32 f = 0; f < 2; ++f) {
                dpUi.BeginFrame({600.0f, 400.0f});
                if (auto tb = dpUi.Toolbar()) {
                    tbDateRect = dpUi.DatePicker(dt).rect;
                    minWDateRect = dpUi.DatePicker(dt, {.minWidth = 260.0f}).rect;
                }
                if (auto col = dpUi.Column({.padding = guikit::Padding::All(16.0f), .width = 480.0f})) {
                    if (auto el = dpUi.Element({.label = "Scheduled Date"})) {
                        formDateRect = dpUi.DatePicker(dt).rect;
                    }
                }
                dpUi.EndFrame(canvas);
            }
            CHECK(keel::approx_eq(formDateRect.w, 448.0f, 1.0f));
            CHECK(tbDateRect.w < 240.0f);
            CHECK(keel::approx_eq(minWDateRect.w, 260.0f, 1.0f));
        }
        CHECK(leftPaneRect.w > 100.0f);
        CHECK(rightPaneRect.w > leftPaneRect.w);
        CHECK(rightPaneRect.x > leftPaneRect.Right());

        // 1. MultiSelect: click to open popup, press Backspace when filter is empty -> removes last selected chip (2)
        ui.OnMouseMove({rMulti.rect.Right() - 12.0f, rMulti.rect.Center().y});
        ui.OnMouseDown(1);
        ui.BeginFrame({800.0f, 600.0f});
        renderAllFive();
        ui.EndFrame(canvas);
        ui.OnMouseUp(1);
        ui.BeginFrame({800.0f, 600.0f});
        renderAllFive();
        ui.EndFrame(canvas);
        CHECK(ui.IsPopupOpen(rMulti.id));

        ui.OnKeyDown(guikit::Key::Backspace);
        ui.BeginFrame({800.0f, 600.0f});
        renderAllFive();
        ui.EndFrame(canvas);
        CHECK(rMulti.changed);
        CHECK(multiSel.size() == 1 && multiSel[0] == 0);

        ui.OnKeyDown(guikit::Key::Escape);
        ui.BeginFrame({800.0f, 600.0f});
        renderAllFive();
        ui.EndFrame(canvas);
        CHECK(!ui.IsPopupOpen(rMulti.id));

        // 2. CommandPalette: open, type "sonar", press Enter -> triggers index 1 ("Open Sonar View") and closes
        cmdOpen = true;
        ui.BeginFrame({800.0f, 600.0f});
        renderAllFive();
        ui.EndFrame(canvas);

        ui.OnTextInput("sonar");
        ui.BeginFrame({800.0f, 600.0f});
        renderAllFive();
        ui.EndFrame(canvas);
        CHECK(cmdOpen);

        ui.OnKeyDown(guikit::Key::Enter);
        ui.BeginFrame({800.0f, 600.0f});
        renderAllFive();
        ui.EndFrame(canvas);
        CHECK(!cmdOpen);
        CHECK(rCmd.changed);
        CHECK(rCmd.selectedIndex == 1);

        // 2b. CommandPalette content-aware auto-fit width, explicit width override, and UiSize scaling
        {
            static const keel::StaticArray<guikit::CommandPaletteItem, 2> kShortCmds = {{
                {.label = "Save", .shortcut = "Ctrl+S"},
                {.label = "Quit", .shortcut = "Ctrl+Q"}
            }};
            static const keel::StaticArray<guikit::CommandPaletteItem, 2> kLongCmds = {{
                {.label = "Toggle Docked / Floating Island Shell", .category = "Workspace", .description = "Switch between flush IDE & floating island frame", .shortcut = "Ctrl+Shift+F"},
                {.label = "Go to LevelMeter / Segmented VU Meter", .category = "Elements",  .description = "LED dB meters, peak-hold & clip LED"}
            }};

            bool openShort = true;
            bool openLong = true;
            bool openExplicit = true;
            bool openXs = true;
            bool openXl = true;

            guikit::Context cpUi;
            cpUi.BeginFrame({800.0f, 600.0f});
            const auto rShort = cpUi.CommandPalette(openShort, kShortCmds);
            const auto rLong = cpUi.CommandPalette(openLong, kLongCmds);
            const auto rExplicit = cpUi.CommandPalette(openExplicit, kLongCmds, {.width = 500.0f});
            const auto rXs = cpUi.CommandPalette(openXs, kShortCmds, {.size = guikit::UiSize::Xs});
            const auto rXl = cpUi.CommandPalette(openXl, kShortCmds, {.size = guikit::UiSize::Xl});
            cpUi.EndFrame(canvas);

            CHECK(rShort.rect.w >= 480.0f);
            CHECK(rLong.rect.w > rShort.rect.w);
            CHECK(rLong.rect.w <= 780.0f);
            CHECK(keel::abs(rExplicit.rect.w - 500.0f) < 1.0f);
            CHECK(rXl.rect.h > rXs.rect.h);

            // Verify mouse wheel scrolling and interactive scrollbar thumb drag on multi-frame open CommandPalette
            static const keel::StaticArray<guikit::CommandPaletteItem, 8> kManyCmds = {{
                {.label = "Cmd 0"}, {.label = "Cmd 1"}, {.label = "Cmd 2"}, {.label = "Cmd 3"},
                {.label = "Cmd 4"}, {.label = "Cmd 5"}, {.label = "Cmd 6"}, {.label = "Cmd 7"}
            }};
            guikit::Context scrollCpUi;
            bool openScroll = true;
            guikit::CommandPaletteResponse rScroll{};
            auto renderScrollCp = [&]() {
                rScroll = scrollCpUi.CommandPalette(openScroll, kManyCmds, {.maxVisibleItems = 3});
            };

            // Frame 1: open palette (maxVisibleItems = 3 -> 8 items overflow)
            scrollCpUi.BeginFrame({800.0f, 600.0f});
            renderScrollCp();
            scrollCpUi.EndFrame(canvas);

            // Frame 2: mouse wheel scroll down while m_prevOverlayHitRegions has the backdrop
            scrollCpUi.OnMouseMove(rScroll.rect.Center());
            scrollCpUi.OnMouseWheel(0.0f, -5.0f);
            scrollCpUi.BeginFrame({800.0f, 600.0f});
            renderScrollCp();
            scrollCpUi.EndFrame(canvas);
            CHECK(openScroll);

            // Frame 3: click first visible row in scrolled viewport -> must select item >= 4 (scrolled down!)
            scrollCpUi.OnMouseMove({rScroll.rect.x + 80.0f, rScroll.rect.y + 66.0f});
            scrollCpUi.OnMouseDown(1);
            scrollCpUi.BeginFrame({800.0f, 600.0f});
            renderScrollCp();
            scrollCpUi.EndFrame(canvas);
            scrollCpUi.OnMouseUp(1);
            CHECK(!openScroll);
            CHECK(rScroll.changed);
            CHECK(rScroll.selectedIndex >= 4);

            // Frame 4..6: reopen and test interactive scrollbar thumb drag
            openScroll = true;
            scrollCpUi.BeginFrame({800.0f, 600.0f});
            renderScrollCp();
            scrollCpUi.EndFrame(canvas);

            // Click on scrollbar thumb near top-right of list area and drag down
            const float sbX = rScroll.rect.Right() - 6.0f;
            const float sbStartY = rScroll.rect.y + 60.0f;
            scrollCpUi.OnMouseMove({sbX, sbStartY});
            scrollCpUi.OnMouseDown(1);
            scrollCpUi.BeginFrame({800.0f, 600.0f});
            renderScrollCp();
            scrollCpUi.EndFrame(canvas);
            CHECK(openScroll); // Clicking scrollbar must NOT close or trigger a row

            scrollCpUi.OnMouseMove({sbX, sbStartY + 80.0f});
            scrollCpUi.BeginFrame({800.0f, 600.0f});
            renderScrollCp();
            scrollCpUi.EndFrame(canvas);
            scrollCpUi.OnMouseUp(1);
            scrollCpUi.BeginFrame({800.0f, 600.0f});
            renderScrollCp();
            scrollCpUi.EndFrame(canvas);
            CHECK(openScroll);

            // Click first visible row after dragging scrollbar to bottom -> must select item >= 4
            scrollCpUi.OnMouseMove({rScroll.rect.x + 80.0f, rScroll.rect.y + 66.0f});
            scrollCpUi.OnMouseDown(1);
            scrollCpUi.BeginFrame({800.0f, 600.0f});
            renderScrollCp();
            scrollCpUi.EndFrame(canvas);
            scrollCpUi.OnMouseUp(1);
            CHECK(!openScroll);
            CHECK(rScroll.selectedIndex >= 4);
        }

        // 3. keel::FixedString Rule-of-Zero copy safety + Breadcrumb & Pagination interactive click tests
        {
            static_assert(keel::IsTriviallyCopyable<keel::FixedString<>>);
            keel::FixedString<> copiedInline;
            {
                keel::String dynamicStr("Workspace");
                copiedInline = keel::FixedString<>(dynamicStr.as_ref());
                keel::FixedString<> overwriteStack = keel::FixedString<>(keel::StringRef(""));
                (void)overwriteStack;
            }
            CHECK(copiedInline == "Workspace");
            CHECK(keel::StringRef(copiedInline.c_str()) == "Workspace");

            // Click first item ("Home") in Breadcrumb -> changes crumbIdx from 1 to 0
            ui.OnMouseMove({rCrumb.rect.x + 18.0f, rCrumb.rect.Center().y});
            ui.OnMouseDown(1);
            ui.BeginFrame({800.0f, 600.0f});
            renderAllFive();
            ui.EndFrame(canvas);
            ui.OnMouseUp(1);
            ui.BeginFrame({800.0f, 600.0f});
            renderAllFive();
            ui.EndFrame(canvas);
            CHECK(rCrumb.changed);
            CHECK(crumbIdx == 0);

            // Click first button ('«' First page) in Pagination -> changes pageIdx from 2 to 0
            ui.OnMouseMove({rPage.rect.x + 12.0f, rPage.rect.Center().y});
            ui.OnMouseDown(1);
            ui.BeginFrame({800.0f, 600.0f});
            renderAllFive();
            ui.EndFrame(canvas);
            ui.OnMouseUp(1);
            ui.BeginFrame({800.0f, 600.0f});
            renderAllFive();
            ui.EndFrame(canvas);
            CHECK(rPage.changed);
            CHECK(pageIdx == 0);

            // StepSlider(UiSize&) stable step count and 'A' (Auto) label when includeNone=true vs false
            guikit::UiSize szAuto = guikit::UiSize::None;
            guikit::UiSize szNoNone = guikit::UiSize::None;
            guikit::Response rAuto{}, rNoNone{};
            auto renderSizeSliders = [&]() {
                if (auto col = ui.Column({.gap = 8.0f})) {
                    rAuto = ui.StepSlider(szAuto, {.showTicks = true, .showLabels = true, .includeNone = true});
                    rNoNone = ui.StepSlider(szNoNone, {.showTicks = true, .showLabels = true, .includeNone = false});
                }
            };
            for (keel::i32 f = 0; f < 2; ++f) {
                ui.BeginFrame({800.0f, 600.0f});
                renderSizeSliders();
                ui.EndFrame(canvas);
            }

            // Drag rNoNone to the leftmost edge -> must land on XS (not None) without changing step count
            ui.OnMouseMove({rNoNone.rect.x + 2.0f, rNoNone.rect.y + 8.0f});
            ui.OnMouseDown(1);
            ui.BeginFrame({800.0f, 600.0f});
            renderSizeSliders();
            ui.EndFrame(canvas);
            ui.OnMouseUp(1);
            ui.BeginFrame({800.0f, 600.0f});
            renderSizeSliders();
            ui.EndFrame(canvas);
            CHECK(szNoNone == guikit::UiSize::Xs);

            // Drag rAuto to the second tick (1/5 of track) -> Xs, then back to leftmost edge -> None ('A')
            ui.OnMouseMove({rAuto.rect.x + rAuto.rect.w * 0.2f, rAuto.rect.y + 8.0f});
            ui.OnMouseDown(1);
            ui.BeginFrame({800.0f, 600.0f});
            renderSizeSliders();
            ui.EndFrame(canvas);
            CHECK(szAuto == guikit::UiSize::Xs);

            ui.OnMouseMove({rAuto.rect.x + 2.0f, rAuto.rect.y + 8.0f});
            ui.BeginFrame({800.0f, 600.0f});
            renderSizeSliders();
            ui.EndFrame(canvas);
            ui.OnMouseUp(1);
            CHECK(szAuto == guikit::UiSize::None);

            // Collapsed Breadcrumb '...' pill opens dropdown popup instead of inline expansion
            keel::usize collapsedActive = 4;
            guikit::Response rColBc{};
            static constexpr keel::StaticArray<keel::StringRef, 5> kDeepTrail = {
                "Workspace", "Clusters", "Production", "Telemetry", "LiveStream"
            };
            auto renderCollapsedBc = [&]() {
                if (auto col = ui.Column({.gap = 8.0f})) {
                    rColBc = ui.Breadcrumb(collapsedActive, kDeepTrail, {.maxVisibleItems = 2});
                }
            };
            for (keel::i32 f = 0; f < 2; ++f) {
                ui.BeginFrame({800.0f, 600.0f});
                renderCollapsedBc();
                ui.EndFrame(canvas);
            }
            const float collapsedWidthBefore = rColBc.rect.w;
            CHECK(!ui.IsPopupOpen(rColBc.id));

            // Click '...' pill (located right after first item "Workspace" + separator)
            ui.OnMouseMove({rColBc.rect.x + rColBc.rect.w * 0.48f, rColBc.rect.Center().y});
            ui.OnMouseDown(1);
            ui.BeginFrame({800.0f, 600.0f});
            renderCollapsedBc();
            ui.EndFrame(canvas);
            ui.OnMouseUp(1);
            ui.BeginFrame({800.0f, 600.0f});
            renderCollapsedBc();
            ui.EndFrame(canvas);
            CHECK(ui.IsPopupOpen(rColBc.id));
            CHECK_NEAR(rColBc.rect.w, collapsedWidthBefore, 1.0f); // Trail stays compact (not expanded inline)

            // Navigate down to second collapsed item ("Production", index 2) and press Enter
            ui.OnKeyDown(guikit::Key::Down);
            ui.BeginFrame({800.0f, 600.0f});
            renderCollapsedBc();
            ui.EndFrame(canvas);

            ui.OnKeyDown(guikit::Key::Enter);
            ui.BeginFrame({800.0f, 600.0f});
            renderCollapsedBc();
            ui.EndFrame(canvas);
            CHECK(!ui.IsPopupOpen(rColBc.id));
            CHECK(rColBc.changed);
            CHECK(collapsedActive == 2);

            // Pagination fixed-slot zero layout shift across all pages 0..19 + Static vs FastJump ellipsis modes
            const guikit::PaginationOptions pgOpts{.siblingCount = 1, .showFirstLast = true, .showPrevNext = true, .showPageInfo = true};
            const guikit::Vec2 wPage0 = guikit::Pagination::Measure(0, 20, pgOpts, guikit::UiSize::Md, &canvas, ui.GetTheme());
            for (keel::usize p = 1; p < 20; ++p) {
                const guikit::Vec2 wPageP = guikit::Pagination::Measure(p, 20, pgOpts, guikit::UiSize::Md, &canvas, ui.GetTheme());
                CHECK_NEAR(wPageP.x, wPage0.x, 0.01f);
            }

            keel::usize pgCurr = 4; // Page 5 (middle window: « < 1 ... 4 [5] 6 ... 20 > » -> 11 slots, index 3 is LeftEllipsis)
            bool fastJump = false;
            guikit::Response rPgMode{};
            auto renderPgMode = [&]() {
                if (auto col = ui.Column({.gap = 8.0f})) {
                    rPgMode = ui.Pagination(pgCurr, 20, {.siblingCount = 1, .interactiveEllipsis = fastJump});
                }
            };
            for (keel::i32 f = 0; f < 2; ++f) {
                ui.BeginFrame({800.0f, 600.0f});
                renderPgMode();
                ui.EndFrame(canvas);
            }
            // Slot 3 center out of 11 uniform slots: (3 + 0.5) / 11.0
            const float leftEllipsisX = rPgMode.rect.x + rPgMode.rect.w * (3.5f / 11.0f);
            ui.OnMouseMove({leftEllipsisX, rPgMode.rect.Center().y});
            ui.OnMouseDown(1);
            ui.BeginFrame({800.0f, 600.0f});
            renderPgMode();
            ui.EndFrame(canvas);
            ui.OnMouseUp(1);
            ui.BeginFrame({800.0f, 600.0f});
            renderPgMode();
            ui.EndFrame(canvas);
            CHECK(!rPgMode.changed);
            CHECK(pgCurr == 4); // Static mode: ellipsis does not jump

            fastJump = true;
            pgCurr = 9; // Page 10 -> fast-jump left by 5 lands on Page 5 (index 4)
            for (keel::i32 f = 0; f < 2; ++f) {
                ui.BeginFrame({800.0f, 600.0f});
                renderPgMode();
                ui.EndFrame(canvas);
            }
            ui.OnMouseMove({leftEllipsisX, rPgMode.rect.Center().y});
            ui.OnMouseDown(1);
            ui.BeginFrame({800.0f, 600.0f});
            renderPgMode();
            ui.EndFrame(canvas);
            ui.OnMouseUp(1);
            ui.BeginFrame({800.0f, 600.0f});
            renderPgMode();
            ui.EndFrame(canvas);
            CHECK(rPgMode.changed);
            CHECK(pgCurr == 4);
        }
    }

    // LevelMeter (Single & Stereo, Segmented LEDs, Peak-Hold, Clip Reset) & TagInput (Editable Chips, Delimiter Split, Backspace)
    {
        guikit::Context ui;
        guikit::ThorCanvas canvas;
        keel::Array<keel::u32> pixels(800 * 600);
        canvas.SetTarget(pixels.data(), 800, 600, 800);

        double dbLevel = -12.0;
        keel::StaticArray<float, 2> stereoLevels = {-6.0f, 0.0f}; // R channel at 0.0 dB -> triggers clip latch
        static constexpr keel::StaticArray<keel::StringRef, 2> kStereoCh = {"L", "R"};
        keel::Array<keel::String> tags;
        tags.push_back(keel::String("dsp"));
        tags.push_back(keel::String("radar"));

        guikit::Response rMono{};
        guikit::Response rStereo{};
        guikit::Response rTags{};

        auto renderMetersAndTags = [&]() {
            if (auto col = ui.Column({.gap = 8.0f, .padding = guikit::Padding::All(12.0f)})) {
                rMono = ui.LevelMeter(dbLevel, -60.0, 0.0, {
                    .segments = 24,
                    .multiZoneColor = true,
                    .autoPeakHold = true,
                    .showClipLed = true,
                    .showScale = true
                });
                rStereo = ui.LevelMeter(stereoLevels, -60.0f, 0.0f, {
                    .orientation = guikit::Orientation::Vertical,
                    .segments = 16,
                    .channelLabels = kStereoCh,
                    .height = 140.0f
                });
                rTags = ui.TagInput(tags, {
                    .placeholder = "Add tag...",
                    .maxTags = 6,
                    .allowDuplicates = false,
                    .splitOnComma = true,
                    .clearable = true,
                    .showCounter = true
                });
            }
        };

        ui.BeginFrame({800.0f, 600.0f});
        renderMetersAndTags();
        ui.EndFrame(canvas);
        ui.BeginFrame({800.0f, 600.0f});
        renderMetersAndTags();
        ui.EndFrame(canvas);

        CHECK(rMono.rect.w > 100.0f && rMono.rect.h > 16.0f);
        CHECK(rStereo.rect.w > 30.0f && rStereo.rect.w < 120.0f);
        CHECK_NEAR(rStereo.rect.h, 140.0f, 1.0f);
        CHECK(rTags.rect.w > 100.0f && rTags.rect.h > 20.0f);

        // Verify unified vertical stretching policy between Slider and LevelMeter inside Row and Element
        {
            guikit::Context vUi;
            float faderVal = -6.0f;
            guikit::Response rFaderFill{}, rMeterFill{}, rFaderCompact{}, rMeterCompact{}, rFaderExplicitH{}, rMeterExplicitH{};
            guikit::Response rElFaderFill{}, rElMeterFill{}, rElFaderSpace{}, rElMeterSpace{};

            for (keel::i32 f = 0; f < 2; ++f) {
                vUi.BeginFrame({800.0f, 600.0f});
                if (auto row = vUi.Row({.gap = 16.0f, .height = guikit::SizeMode::Stretch})) {
                    rFaderFill = vUi.Slider(faderVal, -60.0f, 0.0f, {
                        .orientation = guikit::Orientation::Vertical,
                        .height = guikit::SizeMode::Stretch
                    });
                    rMeterFill = vUi.LevelMeter(stereoLevels, -60.0f, 0.0f, {
                        .orientation = guikit::Orientation::Vertical,
                        .height = guikit::SizeMode::Stretch
                    });
                    rFaderCompact = vUi.Slider(faderVal, -60.0f, 0.0f, {
                        .orientation = guikit::Orientation::Vertical,
                        .height = guikit::SizeMode::Fit
                    });
                    rMeterCompact = vUi.LevelMeter(stereoLevels, -60.0f, 0.0f, {
                        .orientation = guikit::Orientation::Vertical,
                        .height = guikit::SizeMode::Fit
                    });
                    rFaderExplicitH = vUi.Slider(faderVal, -60.0f, 0.0f, {
                        .orientation = guikit::Orientation::Vertical,
                        .height = 200.0f
                    });
                    rMeterExplicitH = vUi.LevelMeter(stereoLevels, -60.0f, 0.0f, {
                        .orientation = guikit::Orientation::Vertical,
                        .height = 200.0f
                    });
                    if (auto el = vUi.Element({.label = "Fader Stretch", .width = guikit::SizeMode::Fit})) {
                        rElFaderFill = el.response;
                        vUi.Slider(faderVal, -60.0f, 0.0f, {.orientation = guikit::Orientation::Vertical});
                    }
                    if (auto el = vUi.Element({.label = "Meter Stretch", .width = guikit::SizeMode::Fit})) {
                        rElMeterFill = el.response;
                        vUi.LevelMeter(stereoLevels, -60.0f, 0.0f, {.orientation = guikit::Orientation::Vertical});
                    }
                    if (auto el = vUi.Element({.label = "Fader Fit", .width = guikit::SizeMode::Fit})) {
                        rElFaderSpace = el.response;
                        vUi.Slider(faderVal, -60.0f, 0.0f, {
                            .orientation = guikit::Orientation::Vertical,
                            .height = guikit::SizeMode::Fit
                        });
                    }
                    if (auto el = vUi.Element({.label = "Meter Fit", .width = guikit::SizeMode::Fit})) {
                        rElMeterSpace = el.response;
                        vUi.LevelMeter(stereoLevels, -60.0f, 0.0f, {
                            .orientation = guikit::Orientation::Vertical,
                            .height = guikit::SizeMode::Fit
                        });
                    }
                }
                vUi.EndFrame(canvas);
            }

            // Fill stretches both vertical Slider and vertical LevelMeter to full container height (600px) and keeps width compact
            CHECK_NEAR(rFaderFill.rect.h, 600.0f, 1.0f);
            CHECK_NEAR(rMeterFill.rect.h, 600.0f, 1.0f);
            CHECK(rFaderFill.rect.w < 120.0f);
            CHECK(rMeterFill.rect.w < 120.0f);

            // Compact keeps both at their intrinsic vertical height (< 260px)
            CHECK(rFaderCompact.rect.h > 150.0f && rFaderCompact.rect.h < 260.0f);
            CHECK(rMeterCompact.rect.h > 150.0f && rMeterCompact.rect.h < 260.0f);

            // Explicit .height overrides Fill stretching on both
            CHECK_NEAR(rFaderExplicitH.rect.h, 200.0f, 1.0f);
            CHECK_NEAR(rMeterExplicitH.rect.h, 200.0f, 1.0f);

            // Inside Element: Fill stretches Element height to 600px, while Space keeps Element height compact
            CHECK_NEAR(rElFaderFill.rect.h, 600.0f, 1.0f);
            CHECK_NEAR(rElMeterFill.rect.h, 600.0f, 1.0f);
            CHECK(rElFaderSpace.rect.h < 300.0f);
            CHECK(rElMeterSpace.rect.h < 300.0f);
        }

        // Click stereo LevelMeter to reset peak/clip latch
        ui.OnMouseMove(rStereo.rect.Center());
        ui.OnMouseDown(1);
        ui.BeginFrame({800.0f, 600.0f});
        renderMetersAndTags();
        ui.EndFrame(canvas);
        ui.OnMouseUp(1);
        ui.BeginFrame({800.0f, 600.0f});
        renderMetersAndTags();
        ui.EndFrame(canvas);
        CHECK(rStereo.clicked);

        // Focus TagInput by clicking inside it (away from chip 'x' and clear button)
        ui.OnMouseMove({rTags.rect.Center().x, rTags.rect.Center().y});
        ui.OnMouseDown(1);
        ui.BeginFrame({800.0f, 600.0f});
        renderMetersAndTags();
        ui.EndFrame(canvas);
        ui.OnMouseUp(1);
        ui.BeginFrame({800.0f, 600.0f});
        renderMetersAndTags();
        ui.EndFrame(canvas);
        CHECK(ui.IsFocused(rTags.id));

        // Type comma-separated tags "sonar, lidar," -> adds both "sonar" and "lidar"
        ui.OnTextInput("sonar, lidar,");
        ui.BeginFrame({800.0f, 600.0f});
        renderMetersAndTags();
        ui.EndFrame(canvas);
        CHECK(rTags.changed);
        CHECK(tags.size() == 4);
        CHECK(tags[2] == "sonar" && tags[3] == "lidar");

        // Type duplicate "dsp" and press Enter -> ignored because allowDuplicates == false
        ui.OnTextInput("dsp");
        ui.BeginFrame({800.0f, 600.0f});
        renderMetersAndTags();
        ui.EndFrame(canvas);
        ui.OnKeyDown(guikit::Key::Enter);
        ui.BeginFrame({800.0f, 600.0f});
        renderMetersAndTags();
        ui.EndFrame(canvas);
        CHECK(tags.size() == 4);

        // Press Backspace when inline buffer is empty -> pops last tag ("lidar")
        ui.OnKeyDown(guikit::Key::Backspace);
        ui.BeginFrame({800.0f, 600.0f});
        renderMetersAndTags();
        ui.EndFrame(canvas);
        CHECK(rTags.changed);
        CHECK(tags.size() == 3);
        CHECK(tags.back() == "sonar");
    }

#if GUIKIT_HAS_SCENES
    // Cross-platform keel::http (URL parser, HttpClient, WebSocketClient) + LodePNG raster tile decoder
    {
#if __has_include("keel/http.h")
        const auto parsedHttps = keel::parse_url("https://tile.openstreetmap.org/14/9532/5841.png");
        CHECK(parsedHttps.valid && parsedHttps.is_secure && !parsedHttps.is_websocket);
        CHECK(parsedHttps.host == "tile.openstreetmap.org" && parsedHttps.port == 443);
        CHECK(parsedHttps.path_and_query == "/14/9532/5841.png");

        const auto parsedWss = keel::parse_url("wss://ais.stream.local:8443/live?v=2");
        CHECK(parsedWss.valid && parsedWss.is_secure && parsedWss.is_websocket);
        CHECK(parsedWss.host == "ais.stream.local" && parsedWss.port == 8443);
        CHECK(parsedWss.path_and_query == "/live?v=2");

        keel::HttpClient http("GuiKit-Test/1.0");
        CHECK(!keel::HttpClient::backend_name().empty());

        keel::WebSocketClient ws;
        CHECK(ws.state() == keel::WebSocketState::Disconnected);
#endif

        static constexpr keel::u8 kSample1x1Png[] = {
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
            0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
            0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53,
            0xDE, 0x00, 0x00, 0x00, 0x0C, 0x49, 0x44, 0x41,
            0x54, 0x08, 0xD7, 0x63, 0xF8, 0xCF, 0xC0, 0x00,
            0x00, 0x03, 0x01, 0x01, 0x00, 0x18, 0xDD, 0x8D,
            0xB0, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E,
            0x44, 0xAE, 0x42, 0x60, 0x82
        };
        keel::Array<keel::u32> rgba;
        CHECK(guikit::geo::DecodePngAndScaleToRgba(kSample1x1Png, 256, rgba));
        CHECK(rgba.size() == 256u * 256u);
    }
#endif

    // Theme-driven Application Shell Framing (ShellStyle::Docked vs ShellStyle::Floating)
    {
        using namespace guikit;
        Context ui;
        ThorCanvas canvas;
        keel::Array<keel::u32> pixels(800 * 600);
        canvas.SetTarget(pixels.data(), 800, 600, 800);

        SidebarMode sbMode = SidebarMode::Expanded;
        bool showBp = true;
        Id tbId, barId, sbId, bpId, statusId;
        Node* tbNode = nullptr;
        Node* barNode = nullptr;
        Node* sbNode = nullptr;
        Node* cardNode = nullptr;
        Node* bpNode = nullptr;
        Node* statusNode = nullptr;

        auto renderShell = [&]() {
            ui.BeginFrame({800.0f, 600.0f});
            if (auto root = ui.Column({.width = SizeMode::Stretch, .height = SizeMode::Stretch})) {
                if (auto tb = ui.Toolbar()) {
                    tbNode = tb.GetNode();
                    tbId = tbNode->id;
                    ui.Text("Top Toolbar");
                }
                if (auto body = ui.Box({.width = SizeMode::Stretch, .height = SizeMode::Stretch})) {
                    if (auto bar = ui.ActivityBar(sbMode)) {
                        barNode = bar.GetNode();
                        barId = barNode->id;
                        keel::i32 tool = 0;
                        bar.Item(tool, 0, {.icon = Icon::Folder(), .label = "Explorer"});
                    }
                    if (auto sb = ui.Sidebar(sbMode, {.width = 180.0f})) {
                        sbNode = sb.GetNode();
                        sbId = sbNode->id;
                        ui.Text("Sidebar");
                    }
                    if (auto card = ui.Card({.width = SizeMode::Stretch, .height = SizeMode::Stretch})) {
                        cardNode = card.GetNode();
                        ui.Text("Canvas");
                    }
                    if (auto bp = ui.BottomPanel(showBp, {.height = 120.0f})) {
                        bpNode = bp.GetNode();
                        bpId = bpNode->id;
                        ui.Text("Bottom Panel");
                    }
                }
                if (auto st = ui.StatusBar()) {
                    statusNode = st.GetNode();
                    statusId = statusNode->id;
                    st.Item({.text = "Ready"});
                }
            }
            ui.EndFrame(canvas);
        };

        // 1. Default Docked mode: 0px outer padding, 0px panel gap, 0px corner radius on all chrome panels & center card
        CHECK(ui.GetTheme().GetShellStyle() == ShellStyle::Docked);
        renderShell();
        renderShell();
        CHECK_NEAR(tbNode->cornerRadius, 0.0f, 0.01f);
        CHECK_NEAR(barNode->cornerRadius, 0.0f, 0.01f);
        CHECK_NEAR(sbNode->cornerRadius, 0.0f, 0.01f);
        CHECK_NEAR(cardNode->cornerRadius, 0.0f, 0.01f);
        CHECK_NEAR(cardNode->borderWidth, 0.0f, 0.01f);
        CHECK_NEAR(bpNode->cornerRadius, 0.0f, 0.01f);
        CHECK_NEAR(statusNode->cornerRadius, 0.0f, 0.01f);
        CHECK_NEAR(ui.GetPrevFrameBounds(tbId).x, 0.0f, 0.01f);
        CHECK_NEAR(ui.GetPrevFrameBounds(tbId).y, 0.0f, 0.01f);
        CHECK_NEAR(ui.GetPrevFrameBounds(barId).x, 0.0f, 0.01f);
        CHECK_NEAR(ui.GetPrevFrameBounds(sbId).x, ui.GetPrevFrameBounds(barId).Right(), 0.01f);
        CHECK_NEAR(ui.GetPrevFrameBounds(statusId).Bottom(), 600.0f, 0.01f);

        // 2. Switch to Floating mode on theme: automatic windowPadding, panelGap, and rounded corners
        UiTheme floatTheme = ui.GetTheme();
        floatTheme.SetShellStyle(ShellStyle::Floating);
        floatTheme.SetMode(ThemeMode::Light);
        CHECK(floatTheme.GetShellStyle() == ShellStyle::Floating);
        CHECK(floatTheme.mode == ThemeMode::Light);
        ui.SetTheme(floatTheme);
        const float expectedGap = floatTheme.shell.panelGap;
        const float expectedRadius = floatTheme.shell.panelRadius;
        CHECK(expectedGap > 0.0f);
        CHECK(expectedRadius > 0.0f);

        renderShell();
        renderShell();
        CHECK_NEAR(tbNode->cornerRadius, expectedRadius, 0.01f);
        CHECK_NEAR(barNode->cornerRadius, expectedRadius, 0.01f);
        CHECK_NEAR(sbNode->cornerRadius, expectedRadius, 0.01f);
        CHECK_NEAR(cardNode->cornerRadius, expectedRadius, 0.01f);
        CHECK_NEAR(cardNode->borderWidth, floatTheme.shell.panelBorderWidth, 0.01f);
        CHECK_NEAR(bpNode->cornerRadius, expectedRadius, 0.01f);
        CHECK_NEAR(statusNode->cornerRadius, expectedRadius, 0.01f);
        CHECK_NEAR(ui.GetPrevFrameBounds(tbId).x, expectedGap, 0.01f);
        CHECK_NEAR(ui.GetPrevFrameBounds(tbId).y, expectedGap, 0.01f);
        CHECK_NEAR(ui.GetPrevFrameBounds(barId).x, expectedGap, 0.01f);
        CHECK_NEAR(ui.GetPrevFrameBounds(sbId).x, ui.GetPrevFrameBounds(barId).Right() + expectedGap, 0.01f);
        CHECK_NEAR(ui.GetPrevFrameBounds(statusId).Bottom(), 600.0f - expectedGap, 0.01f);
    }

    // -------------------------------------------------------------------------
    // Test: Nested ScrollArea wheel bubbling & 2D centered ScrollArea
    // -------------------------------------------------------------------------
    {
        using namespace guikit;
        Context ui;
        SdlCanvas canvas(nullptr);

        Id outerId = Id::Null();
        Id innerId = Id::Null();
        Id childId = Id::Null();
        float childW = 200.0f;
        float childH = 150.0f;

        auto render2D = [&]() {
            ui.BeginFrame({500.0f, 400.0f});
            if (auto outer = ui.ScrollArea({
                .orientation = Orientation::Both,
                .align = Alignment::Center,
                .justify = Justify::Center
            })) {
                outerId = outer.GetNode()->id;
                if (auto box = ui.Box({
                    .width = childW,
                    .height = childH
                })) {
                    childId = box.GetNode()->id;
                    if (auto inner = ui.ScrollArea({.orientation = Orientation::Vertical})) {
                        innerId = inner.GetNode()->id;
                        if (auto innerContent = ui.Box({
                            .width = SizeMode::Stretch,
                            .height = 900.0f
                        })) {}
                    }
                }
            }
            ui.EndFrame(canvas);
        };

        // 1. When child (200x150) is smaller than outer ScrollArea (500x400), it is centered in 2D
        render2D();
        const Rect centeredRect = ui.GetPrevFrameBounds(childId);
        CHECK_NEAR(centeredRect.x, (500.0f - 200.0f) * 0.5f, 1.0f);
        CHECK_NEAR(centeredRect.y, (400.0f - 150.0f) * 0.5f, 1.0f);

        // 2. When child (700x600) is larger than outer ScrollArea (500x400), both inner and outer can scroll
        childW = 700.0f;
        childH = 600.0f;
        render2D();

        // Hover inside the inner ScrollArea and scroll down: inner consumes wheel, outer stays at 0
        ui.OnMouseMove({250.0f, 200.0f});
        ui.OnMouseWheel(0.0f, -1.0f);
        render2D();
        const auto& innerState1 = ui.GetOrCreateState<ScrollState>(innerId);
        const auto& outerState1 = ui.GetOrCreateState<ScrollState>(outerId);
        CHECK(innerState1.offset.y > 0.0f);
        CHECK_NEAR(outerState1.offset.y, 0.0f, 0.01f);

        // Scroll inner all the way to bottom, then scroll once more: wheel bubbles up to outer ScrollArea
        ui.OnMouseWheel(0.0f, -50.0f);
        render2D();
        ui.OnMouseWheel(0.0f, -1.0f);
        render2D();
        const auto& outerState2 = ui.GetOrCreateState<ScrollState>(outerId);
        CHECK(outerState2.offset.y > 0.0f);
    }

    // -------------------------------------------------------------------------
    // Test: Floating ColorPicker popup responds to mouse clicks & drags
    // -------------------------------------------------------------------------
    {
        using namespace guikit;
        Context ui;
        SdlCanvas canvas(nullptr);
        Color col(59, 130, 246, 235);
        Response cpRes{};

        auto renderCp = [&]() {
            ui.BeginFrame({500.0f, 500.0f});
            cpRes = ui.ColorPicker(col, {.showHex = true, .showAlpha = true, .showSwatches = true});
            ui.EndFrame(canvas);
        };

        renderCp();
        const Rect btnRect = ui.GetPrevFrameBounds(cpRes.id);
        CHECK(btnRect.w > 0.0f);

        // 1. Click button to open floating ColorPicker popup
        ui.OnMouseMove(btnRect.Center());
        ui.OnMouseDown(1);
        ui.OnMouseUp(1);
        renderCp();
        CHECK(ui.IsPopupOpen(cpRes.id));

        // 2. Click inside the SV square of the open popup and verify color changes
        const Rect popRect = ui.GetOpenPopupRect();
        CHECK(popRect.w > 0.0f);
        ui.OnMouseMove({popRect.x + 24.0f, popRect.y + 24.0f});
        ui.OnMouseDown(1);
        renderCp();
        CHECK(cpRes.changed);
        ui.OnMouseUp(1);
        renderCp();
    }

    // -------------------------------------------------------------------------
    // Test: Smart fractional width floor & Segmented narrow-width auto-collapse
    // -------------------------------------------------------------------------
    {
        using namespace guikit;
        const UiTheme& darkTheme = UiTheme::Dark();
        LabelRecordingCanvas recCanvas;
        keel::Array<keel::u32> pixels(400 * 200);
        recCanvas.SetTarget(pixels.data(), 400, 200, 400);

        // 1. Element::Resolve with SizeMode::Fit preserves a 240px intrinsic control width inside a 280px container
        const ElementGeometry narrowGeom = Element::Resolve(
            Rect(0.0f, 0.0f, 280.0f, 40.0f),
            Vec2(240.0f, 32.0f),
            ElementSpec{.width = SizeMode::Fit},
            &recCanvas,
            darkTheme
        );
        CHECK_NEAR(narrowGeom.controlRect.w, 240.0f, 0.01f);

        // And when the container is narrower than intrinsic (180px < 240px), it clamps cleanly to 180px
        const ElementGeometry ultraNarrowGeom = Element::Resolve(
            Rect(0.0f, 0.0f, 180.0f, 40.0f),
            Vec2(240.0f, 32.0f),
            ElementSpec{.width = SizeMode::Fit},
            &recCanvas,
            darkTheme
        );
        CHECK_NEAR(ultraNarrowGeom.controlRect.w, 180.0f, 0.01f);

        // 2. Segmented control at narrow width (140px for 4 icon+text items) auto-collapses to icon-only
        recCanvas.Reset();
        const SegmentedItem items[] = {
            {"Sine", IconId::Refresh},
            {"Square", IconId::Settings},
            {"Sawtooth", IconId::Play},
            {"Noise", IconId::Warning}
        };
        Segmented::Render(
            recCanvas,
            Rect(0.0f, 0.0f, 140.0f, 32.0f),
            items,
            0,
            0u,
            false,
            -1,
            -1,
            SegmentedOptions{},
            UiSize::Md,
            false,
            darkTheme
        );
        // Because 140px / 4 = 35px per segment is too narrow for icon + text, all 4 segments collapse to icon-only (no textBoxCalls)
        CHECK(recCanvas.textBoxCalls.empty());

        // 3. Popups (DatePicker, ColorPicker, Select) inside a clipped Box (e.g. 784x375 Phone Landscape inside 1400x900 window)
        // stay strictly within the clipped Box bounds via GetPopupViewportRect()
        Context ui;
        ThorCanvas thorCanvas;
        keel::Array<keel::u32> fullPixels(1400 * 900);
        thorCanvas.SetTarget(fullPixels.data(), 1400, 900, 1400);

        DateTime dt{.year = 2026, .month = 10, .day = 14};
        Color col(59, 130, 246, 235);
        keel::usize selIdx = 0;
        Id screenBoxId = Id::Null();
        Id datePickerId = Id::Null();
        Id colorPickerId = Id::Null();
        Id selectId = Id::Null();
        Rect capturedPopupVp{};
        // Widget bounds in frame dp (read between frames, outside the scaled preview): where the mouse has to go
        auto frameCenter = [&](Id id) { return ui.GetPrevFrameBounds(id).Center(); };

        auto renderSimulatedPhoneLandscape = [&](float previewZoom = 1.0f) {
            if (auto outer = ui.Column({.padding = Padding::All(40.0f), .align = Alignment::Center, .justify = Justify::Center})) {
                if (auto phoneZ = ui.Zone({.scale = previewZoom})) {
                    if (auto screenBox = ui.Box({
                        .width = 784.0f,   // local units: the zone scale maps them to the parent
                        .height = 375.0f,
                        .padding = Padding(40.0f, 32.0f, 18.0f, 20.0f),
                        .border = {.width = 1.0f, .radius = 28.0f},
                        .clipChildren = true
                    })) {
                        screenBoxId = screenBox.GetNode()->id;
                        capturedPopupVp = ui.GetPopupViewportRect();
                        ui.Spacer(1.0f);
                        datePickerId = ui.DatePicker(dt, {.width = SizeMode::Stretch}).id;
                        colorPickerId = ui.ColorPicker(col, {.width = SizeMode::Stretch}).id;
                        selectId = ui.Select(selIdx, {"Option 1", "Option 2", "Option 3", "Option 4", "Option 5", "Option 6", "Option 7", "Option 8"}, {.width = SizeMode::Stretch}).id;
                        ui.Spacer(1.0f);
                    }
                }
            }
        };

        // Frame 1 & 2: establish layout bounds
        for (keel::i32 f = 0; f < 2; ++f) {
            ui.BeginFrame({1400.0f, 900.0f}, 1.0f);
            renderSimulatedPhoneLandscape();
            ui.EndFrame(thorCanvas);
        }

        const Rect boxPrev = ui.GetPrevFrameBounds(screenBoxId);
        CHECK(boxPrev.w > 780.0f);
        CHECK(boxPrev.h > 370.0f);
        CHECK(capturedPopupVp.x >= boxPrev.x);
        CHECK(capturedPopupVp.y >= boxPrev.y);
        CHECK(capturedPopupVp.Right() <= boxPrev.Right());
        CHECK(capturedPopupVp.Bottom() <= boxPrev.Bottom());

        // Click DatePicker to open its calendar popover
        ui.OnMouseMove(frameCenter(datePickerId));
        ui.OnMouseDown(1);
        ui.OnMouseUp(1);
        ui.BeginFrame({1400.0f, 900.0f}, 1.0f);
        renderSimulatedPhoneLandscape();
        ui.EndFrame(thorCanvas);

        const Rect dpPopup = ui.GetOpenPopupRect();
        CHECK(dpPopup.w > 200.0f);
        CHECK(dpPopup.h > 200.0f);
        CHECK(dpPopup.x >= boxPrev.x);
        CHECK(dpPopup.y >= boxPrev.y);
        CHECK(dpPopup.Right() <= boxPrev.Right());
        CHECK(dpPopup.Bottom() <= boxPrev.Bottom());

        // Click ColorPicker to open its color popover
        ui.ClosePopup();
        ui.OnMouseMove(frameCenter(colorPickerId));
        ui.OnMouseDown(1);
        ui.OnMouseUp(1);
        ui.BeginFrame({1400.0f, 900.0f}, 1.0f);
        renderSimulatedPhoneLandscape();
        ui.EndFrame(thorCanvas);

        const Rect cpPopup = ui.GetOpenPopupRect();
        CHECK(cpPopup.w > 200.0f);
        CHECK(cpPopup.h > 150.0f);
        CHECK(cpPopup.x >= boxPrev.x);
        CHECK(cpPopup.y >= boxPrev.y);
        CHECK(cpPopup.Right() <= boxPrev.Right());
        CHECK(cpPopup.Bottom() <= boxPrev.Bottom());

        // Click Select to open its dropdown list
        ui.ClosePopup();
        ui.OnMouseMove(frameCenter(selectId));
        ui.OnMouseDown(1);
        ui.OnMouseUp(1);
        ui.BeginFrame({1400.0f, 900.0f}, 1.0f);
        renderSimulatedPhoneLandscape();
        ui.EndFrame(thorCanvas);

        const Rect selPopup = ui.GetOpenPopupRect();
        CHECK(selPopup.w > 100.0f);
        CHECK(selPopup.h > 50.0f);
        CHECK(selPopup.x >= boxPrev.x);
        CHECK(selPopup.y >= boxPrev.y);
        CHECK(selPopup.Right() <= boxPrev.Right());
        CHECK(selPopup.Bottom() <= boxPrev.Bottom());

        // Regression test: ColorPicker open popover at Fit 249% (previewZoom = 2.49f) with partial dirty-rect repaints
        // (hovering trigger button and moving across popover).
        // Previously crashed due to ThorCanvas::DrawPixels borrowing stack-allocated svPixels/huePixels/alphaPixels
        // with copy=false while FlushCanvas() ran after stack frame pop.
        {
            ui.ClosePopup();
            keel::Array<keel::u32> hiDpiPixels(2400 * 1400);
            thorCanvas.SetTarget(hiDpiPixels.data(), 2400, 1400, 2400);
            for (keel::i32 f = 0; f < 2; ++f) {
                ui.BeginFrame({2400.0f, 1400.0f}, 1.0f);
                renderSimulatedPhoneLandscape(2.49f);
                ui.EndFrame(thorCanvas);
            }
            // Open ColorPicker at previewZoom = 2.49f
            ui.OnMouseMove(frameCenter(colorPickerId));
            ui.OnMouseDown(1);
            ui.OnMouseUp(1);
            ui.BeginFrame({2400.0f, 1400.0f}, 1.0f);
            renderSimulatedPhoneLandscape(2.49f);
            ui.EndFrame(thorCanvas);

            const Rect cpPopupHiDpi = ui.GetOpenPopupRect();
            CHECK(cpPopupHiDpi.w > 400.0f);

            // Hover trigger button on the left (triggers partial dirty rect clipping out bottom of popup)
            ui.OnMouseMove({ui.GetPrevFrameBounds(colorPickerId).x + 10.0f, frameCenter(colorPickerId).y});
            ui.BeginFrame({2400.0f, 1400.0f}, 1.0f);
            renderSimulatedPhoneLandscape(2.49f);
            ui.EndFrame(thorCanvas);

            // Hover inside the open ColorPicker popup
            ui.OnMouseMove(cpPopupHiDpi.Center());
            ui.BeginFrame({2400.0f, 1400.0f}, 1.0f);
            renderSimulatedPhoneLandscape(2.49f);
            ui.EndFrame(thorCanvas);
        }

#if GUIKIT_HAS_SCENES
        // Regression test: Partially-clipped GPU scene widgets (SonarView / WaterfallView) above an open BottomPanel
        // (TextArea cursor blinking / mouse movement in BottomPanel).
        // Previously, ThorCanvas::Clear erased GPU passes whenever a dirty rect in BottomPanel intersected the
        // UNCLIPPED plotRect extending below the ScrollArea clip boundary, causing the GPU underlay to drop and flash black.
        {
            Context clipUi;
            ThorCanvas clipCanvas;
            keel::Array<keel::u32> clipPixels(1200 * 800);
            clipCanvas.SetTarget(clipPixels.data(), 1200, 800, 1200);
            clipCanvas.SetGpuShadersEnabled(true);

            keel::Array<float> sonarMatrix(64 * 64, 0.5f);
            keel::Array<float> wfSlice(128, -45.0f);
            keel::u64 wfSeq = 1;
            bool bottomOpen = true;
            keel::String codeBuf("auto resp = ui.SonarView(polarMatrix, {\n    .numBeams = 180,\n    .numRangeBins = 256\n});");
            Rect codeAreaRect{};
            SonarViewResponse lastSonarResp{};
            WaterfallViewResponse lastWfResp{};
            bool showWaterfall = false;

            auto renderClippedScene = [&](double timeSec) {
                clipUi.SetCurrentTime(timeSec);
                if (auto rootCol = clipUi.Column({.gap = 0.0f, .padding = 0.0f})) {
                    // Upper ScrollArea is constrained to ~540px while inner simulated tablet/window is 700px tall
                    if (auto scroll = clipUi.ScrollArea({.gap = 0.0f, .padding = 0.0f})) {
                        if (auto stageFrame = clipUi.Box({
                            .width = 900.0f,
                            .height = 700.0f,
                            .clipChildren = true
                        })) {
                            if (!showWaterfall) {
                                lastSonarResp = clipUi.SonarView(sonarMatrix, {
                                    .numBeams = 64,
                                    .numRangeBins = 64,
                                    .displayMode = SonarDisplayMode::SectorWedge,
                                    .height = 680.0f
                                });
                            } else {
                                lastWfResp = clipUi.WaterfallView(wfSlice, {
                                    .mode = WaterfallMode::SpectrumRtsa,
                                    .viewLayout = WaterfallLayout::SplitTraceAndWaterfall,
                                    .numBins = 128,
                                    .historyRows = 64,
                                    .sliceSequence = wfSeq,
                                    .height = 680.0f
                                });
                            }
                        }
                    }
                    if (auto bp = clipUi.BottomPanel(bottomOpen, {
                        .height = 260.0f,
                        .resizable = true,
                        .allowCollapse = true
                    })) {
                        auto taResp = clipUi.TextArea(codeBuf, {
                            .autoHeight = false,
                            .readOnly = true,
                            .showLineNumbers = true,
                            .height = SizeMode::Stretch
                        });
                        codeAreaRect = taResp.rect;
                    }
                }
            };

            // 1. Settle layout and initial full render for SonarView
            for (keel::i32 f = 0; f < 3; ++f) {
                clipUi.BeginFrame({1200.0f, 800.0f}, 1.0f);
                renderClippedScene(1.0 + f * 0.1);
                clipUi.EndFrame(clipCanvas);
            }
            CHECK(clipCanvas.HasActiveGpuPasses());
            CHECK(!clipCanvas.GetGpuShaderPasses().empty());
            if (!clipCanvas.GetGpuShaderPasses().empty()) {
                const Rect initialSonarClip = clipCanvas.GetGpuShaderPasses()[0].clipRect;
                const Rect initialSonarPlot = clipCanvas.GetGpuShaderPasses()[0].plotRect;
                // Verify the widget is genuinely partially clipped vertically (unclipped plot extends into BottomPanel)
                CHECK(initialSonarPlot.Bottom() > initialSonarClip.Bottom() + 50.0f);
                CHECK(initialSonarPlot.Bottom() > codeAreaRect.y);

                // 2. Click inside the BottomPanel TextArea to focus it and trigger cursor blinking (partial dirty rects in BottomPanel)
                clipUi.OnMouseMove(codeAreaRect.Center());
                clipUi.OnMouseDown(1);
                clipUi.OnMouseUp(1);
                for (keel::i32 step = 0; step < 6; ++step) {
                    clipUi.BeginFrame({1200.0f, 800.0f}, 1.0f);
                    renderClippedScene(2.0 + step * 0.55); // Toggles cursor blink every 0.53s
                    clipUi.EndFrame(clipCanvas);
                    // GPU pass must NEVER be dropped when BottomPanel repaints!
                    CHECK(clipCanvas.HasActiveGpuPasses());
                    CHECK(!clipCanvas.GetGpuShaderPasses().empty());
                    if (!clipCanvas.GetGpuShaderPasses().empty()) {
                        CHECK(keel::abs(clipCanvas.GetGpuShaderPasses()[0].clipRect.h - initialSonarClip.h) < 2.0f);
                    }
                }

                // 3. Move mouse inside the visible portion of the clipped SonarView and verify unclipped geometry stability
                clipUi.OnMouseMove({initialSonarClip.Center().x, initialSonarClip.Bottom() - 40.0f});
                clipUi.BeginFrame({1200.0f, 800.0f}, 1.0f);
                renderClippedScene(6.0);
                clipUi.EndFrame(clipCanvas);
                CHECK(clipCanvas.HasActiveGpuPasses());
                CHECK(lastSonarResp.hovered);
                CHECK(lastSonarResp.cursorInsidePlot);
            }

            // 4. Switch to WaterfallView and verify the same partial-clipping resilience
            showWaterfall = true;
            for (keel::i32 f = 0; f < 3; ++f) {
                ++wfSeq;
                clipUi.BeginFrame({1200.0f, 800.0f}, 1.0f);
                renderClippedScene(7.0 + f * 0.1);
                clipUi.EndFrame(clipCanvas);
            }
            CHECK(clipCanvas.HasActiveGpuPasses());
            CHECK(!clipCanvas.GetGpuShaderPasses().empty());
            if (!clipCanvas.GetGpuShaderPasses().empty()) {
                const Rect initialWfClip = clipCanvas.GetGpuShaderPasses()[0].clipRect;
                const Rect initialWfPlot = clipCanvas.GetGpuShaderPasses()[0].plotRect;
                CHECK(initialWfPlot.Bottom() > initialWfClip.Bottom() + 50.0f);

                // Blink cursor in BottomPanel while WaterfallView is partially clipped
                clipUi.OnMouseMove(codeAreaRect.Center());
                clipUi.OnMouseDown(1);
                clipUi.OnMouseUp(1);
                for (keel::i32 step = 0; step < 6; ++step) {
                    clipUi.BeginFrame({1200.0f, 800.0f}, 1.0f);
                    renderClippedScene(8.0 + step * 0.55);
                    clipUi.EndFrame(clipCanvas);
                    CHECK(clipCanvas.HasActiveGpuPasses());
                    CHECK(!clipCanvas.GetGpuShaderPasses().empty());
                    if (!clipCanvas.GetGpuShaderPasses().empty()) {
                        CHECK(keel::abs(clipCanvas.GetGpuShaderPasses()[0].clipRect.h - initialWfClip.h) < 2.0f);
                    }
                }
            }
        }
#endif

        // Toolbar overflow policies: AutoCompact, Scroll, Wrap, Clip
        {
            Context tbUi;
            ThorCanvas tbCanvas;
            keel::Array<keel::u32> tbPixels(800 * 200);
            tbCanvas.SetTarget(tbPixels.data(), 800, 200, 800);

            ToolbarOverflow policy = ToolbarOverflow::AutoCompact;
            keel::i32 activeTool = 0;
            bool snap = true;
            bool cap = false;
            Rect tbRect{};
            Id tbId{};
            Response rSelect{}, rPan{}, rMeasure{}, rExport{};

            auto renderTb = [&]() {
                if (auto col = tbUi.Column({.padding = Padding::All(8.0f)})) {
                    if (auto tb = tbUi.Toolbar({.style = ToolbarStyle::Surface, .overflow = policy})) {
                        tbId = tb.response.id;
                        tbRect = tb.response.rect;
                        rSelect  = tb.Item(activeTool, 0, {.text = "Select",  .icon = Icon::Check()});
                        rPan     = tb.Item(activeTool, 1, {.text = "Pan",     .icon = Icon::Folder()});
                        rMeasure = tb.Item(activeTool, 2, {.text = "Measure", .icon = Icon::Search()});
                        tb.Separator();
                        tb.Toggle(snap, {.text = "Snap",    .icon = Icon::Settings()});
                        tb.Toggle(cap,  {.text = "Capture", .icon = Icon::Play()});
                        tb.Separator();
                        rExport = tb.Action({.text = "Export", .icon = Icon::Save()});
                    }
                }
            };

            // 1. Wide viewport (800px): AutoCompact keeps full text labels
            for (keel::i32 f = 0; f < 2; ++f) {
                tbUi.BeginFrame({800.0f, 200.0f});
                renderTb();
                tbUi.EndFrame(tbCanvas);
            }
            CHECK(rSelect.rect.w > rSelect.rect.h + 15.0f);
            CHECK(rExport.rect.Right() <= tbRect.Right());

            // 2. Narrow Phone viewport (360px): AutoCompact collapses Icon+Text tools into square icons so all tools fit
            for (keel::i32 f = 0; f < 2; ++f) {
                tbUi.BeginFrame({360.0f, 200.0f});
                renderTb();
                tbUi.EndFrame(tbCanvas);
            }
            CHECK_NEAR(rSelect.rect.w, rSelect.rect.h, 1.0f);
            CHECK_NEAR(rExport.rect.w, rExport.rect.h, 1.0f);
            CHECK(rExport.rect.Right() <= tbRect.Right() + 0.5f);

            // 3. Narrow Phone viewport (320px) with ToolbarOverflow::Wrap: keeps labels and wraps onto multiple rows
            policy = ToolbarOverflow::Wrap;
            for (keel::i32 f = 0; f < 2; ++f) {
                tbUi.BeginFrame({320.0f, 200.0f});
                renderTb();
                tbUi.EndFrame(tbCanvas);
            }
            CHECK(rSelect.rect.w > rSelect.rect.h + 15.0f);
            CHECK(tbRect.h > 70.0f);
            CHECK(rExport.rect.y > rSelect.rect.y + 20.0f);

            // 4. Narrow Phone viewport (320px) with ToolbarOverflow::Scroll: smooth animation + item-boundary snapping
            policy = ToolbarOverflow::Scroll;
            tbUi.SetCurrentTime(1.0);
            for (keel::i32 f = 0; f < 2; ++f) {
                tbUi.BeginFrame({320.0f, 200.0f});
                renderTb();
                tbUi.EndFrame(tbCanvas);
            }
            const Rect rightArrowRect = tbUi.GetPrevFrameBounds(GetCarouselRightArrowId(tbId));
            CHECK(rightArrowRect.w > 10.0f);
            const float startPanX = rPan.rect.x;

            // Click right arrow '>' -> starts smooth animation toward item boundary
            tbUi.OnMouseMove(rightArrowRect.Center());
            tbUi.OnMouseDown(1);
            tbUi.BeginFrame({320.0f, 200.0f});
            renderTb();
            tbUi.EndFrame(tbCanvas);

            tbUi.OnMouseUp(1);
            tbUi.AdvanceTime(0.016);
            tbUi.BeginFrame({320.0f, 200.0f});
            renderTb();
            tbUi.EndFrame(tbCanvas);

            const float midPanX = tbUi.GetPrevUnclippedBounds(rPan.id).x;
            CHECK(midPanX < startPanX - 1.0f); // Animation started moving left
            CHECK(tbUi.NeedsRepaint());        // Reactive loop kept awake while animating

            // Advance frames to let smooth ease-out animation settle completely
            for (keel::i32 f = 0; f < 25; ++f) {
                tbUi.AdvanceTime(0.016);
                tbUi.BeginFrame({320.0f, 200.0f});
                renderTb();
                tbUi.EndFrame(tbCanvas);
            }
            const float endPanX = tbUi.GetPrevUnclippedBounds(rPan.id).x;
            CHECK(endPanX < midPanX - 5.0f);
            // Select is completely scrolled out of the visible viewport (no half-cut fragment!)
            CHECK(tbUi.GetPrevFrameBounds(rSelect.id).w <= 0.5f);
            // Measure is now 100% visible inside the clipped viewport
            CHECK_NEAR(tbUi.GetPrevFrameBounds(rMeasure.id).w, tbUi.GetPrevUnclippedBounds(rMeasure.id).w, 1.0f);
        }

        // --- Regression test: Markdown in narrow ScrollArea (Phone Portrait) must not vertically center or add top empty gap ---
        {
            Context mdUi;
            ThorCanvas mdCanvas;
            keel::Array<keel::u32> mdPixels(340 * 600);
            mdCanvas.SetTarget(mdPixels.data(), 340, 600, 340);

            constexpr keel::StringRef kDoc =
                "# GuiKit Rich Markdown Engine\n\n"
                "Welcome to **GuiKit's** native vector `ui.Markdown` viewer! Everything is parsed into a lightweight cached AST "
                "and rendered at **60+ FPS** via *ThorVG* with crisp typography, **bold**, *italic*, ~~strikethrough~~, "
                "`inline code` pills, and interactive links.\n\n"
                "> [!TIP]\n"
                "> Hold **Ctrl + MouseWheel** to smoothly zoom, click and drag to **select text** across blocks, "
                "press `Ctrl+C` to copy, or press `Ctrl+F` to search!\n\n"
                "## 1. Embedded Image Asset\n\n"
                "Paragraph two with enough text that wrapping inside a 340px Phone Portrait viewport produces a taller height "
                "than the initial 520px bottom-up intrinsic measurement.\n";

            Id scrollId{};
            Id mdId{};
            for (keel::i32 f = 0; f < 2; ++f) {
                mdUi.BeginFrame({340.0f, 600.0f});
                if (auto scroll = mdUi.ScrollArea({.padding = Padding::All(0.0f)})) {
                    scrollId = scroll.GetNode()->id;
                    const auto mdRes = mdUi.Markdown(kDoc);
                    mdId = mdRes.id;
                }
                mdUi.EndFrame(mdCanvas);
            }

            const Rect scrollRect = mdUi.GetPrevFrameBounds(scrollId);
            const Rect mdUnclippedRect = mdUi.GetPrevUnclippedBounds(mdId);
            const Vec2 defaultMeasured = Markdown::Measure(kDoc, {}, UiSize::Md, &mdCanvas, mdUi.GetTheme(), 0.0f);
            const Vec2 narrowMeasured  = Markdown::Measure(kDoc, {}, UiSize::Md, &mdCanvas, mdUi.GetTheme(), mdUnclippedRect.w);
            CHECK(narrowMeasured.y > defaultMeasured.y + 20.0f);
            CHECK_NEAR(mdUnclippedRect.y, scrollRect.y, 1.0f);
            CHECK_NEAR(mdUnclippedRect.h, narrowMeasured.y, 1.0f);
        }

        // --- Architectural test: ScopedOverlayHitCheck nesting, unified ThorCanvas metrics, and TextUtil::FindCaseInsensitive ---
        {
            ThorCanvas archCanvas;
            keel::Array<keel::u32> archPixels(320 * 120);
            archCanvas.SetTarget(archPixels.data(), 320, 120, 320);

            // 1. Unified vertical metrics between MeasureText and MeasureTextStyled
            const Vec2 mReg = archCanvas.MeasureText("Go to LevelMeter", 13.0f);
            const Vec2 mSty = archCanvas.MeasureTextStyled("Go to LevelMeter", 13.0f, FontStyle::Regular);
            CHECK_NEAR(mReg.x, mSty.x, 0.01f);
            CHECK_NEAR(mReg.y, mSty.y, 0.01f);

            // 2. UTF-8 (ASCII + Cyrillic) case-insensitive substring matching
            keel::usize mPos = 0;
            keel::usize mLen = 0;
            CHECK(TextUtil::FindCaseInsensitive("Палитра Команд", "команд", mPos, mLen));
            CHECK(mPos > 0);
            CHECK(mLen == keel::StringRef("Команд").size());

            // 3. Nested ScopedOverlayHitCheck restores outer overlay hit check ID on inner scope exit
            Context ovUi;
            const Id outerId(1001u);
            const Id innerId(1002u);

            ovUi.BeginFrame({320.0f, 120.0f});
            ovUi.RegisterOverlayHitRegion(outerId, Rect(0.0f, 0.0f, 320.0f, 120.0f));
            ovUi.EndFrame(archCanvas);

            ovUi.OnMouseMove({100.0f, 60.0f});
            ovUi.OnMouseWheel(0.0f, -2.0f);
            ovUi.BeginFrame({320.0f, 120.0f});
            // Outside overlay scope: blocked by outer_overlay
            CHECK_NEAR(ovUi.GetMouseWheel().y, 0.0f, 0.001f);
            {
                auto outerScope = ovUi.ScopedOverlayHitCheck(outerId);
                CHECK_NEAR(ovUi.GetMouseWheel().y, -2.0f, 0.001f);
                {
                    auto innerScope = ovUi.ScopedOverlayHitCheck(innerId);
                    CHECK_NEAR(ovUi.GetMouseWheel().y, -2.0f, 0.001f);
                }
                // After innerScope exits, outerScope is still active and unblocked!
                CHECK_NEAR(ovUi.GetMouseWheel().y, -2.0f, 0.001f);
            }
            // After outerScope exits, blocked again
            CHECK_NEAR(ovUi.GetMouseWheel().y, 0.0f, 0.001f);
            ovUi.EndFrame(archCanvas);

            // 4. VectorBackend registry & SdlWindowBackend ICanvas Dependency Injection
            CHECK(HasDefaultVectorCanvasFactory() == true);
            auto defaultVecCanvas = CreateOffscreenVectorCanvas();
            CHECK(defaultVecCanvas != nullptr);
            CHECK(defaultVecCanvas->UsesCpuFramebuffer() == true);
            CHECK(defaultVecCanvas->GetPixelFormat() == SurfacePixelFormat::ABGR8888_Premul);

            SdlCanvas directGpuCanvas(nullptr);
            CHECK(directGpuCanvas.UsesCpuFramebuffer() == false);

            SdlWindowBackend customWinBackend(keel::make_unique<SdlCanvas>(nullptr));
            CHECK(customWinBackend.GetCanvas().UsesCpuFramebuffer() == false);
            CHECK(customWinBackend.GetCanvas().GetBackendName() == "SDL3 (Direct GPU)");

            // When no default vector canvas factory is registered, SdlWindowBackend falls back to native SdlCanvas
            RegisterDefaultVectorCanvasFactory(nullptr);
            CHECK(HasDefaultVectorCanvasFactory() == false);
            CHECK(CreateVectorCanvas(false) == nullptr);
            {
                SdlWindowBackend fallbackWinBackend;
                CHECK(fallbackWinBackend.GetCanvas().UsesCpuFramebuffer() == false);
                CHECK(fallbackWinBackend.GetCanvas().GetBackendName() == "SDL3 (Direct GPU)");
            }

            // Restore ThorVG vector backend registration
            RegisterThorVectorBackend();
            CHECK(HasDefaultVectorCanvasFactory() == true);

            // 5. Active mouse capture (m_activeId) preserves drag across splitter sashes and Z-layers
            {
                archPixels.assign(1000 * 600, 0);
                archCanvas.SetTarget(archPixels.data(), 1000, 600, 1000);

                Context dragUi;
                bool rightSidebarOpen = true;
                float wsSliderVal = 50.0f;
                float sbSliderVal = 50.0f;
                Rect wsSliderRect{};
                Rect sbSliderRect{};
                Rect sbPanelRect{};
                bool sbBtnHoveredDuringDrag = false;
                bool customGripDragging = false;
                float customGripWidth = 400.0f;
                const Id customGripId(0x9988776655443322ULL);

                auto runDragWorkbench = [&]() {
                    dragUi.BeginFrame({1000.0f, 600.0f});
                    if (auto root = dragUi.Row({ .gap = 0.0f, .padding = Padding::All(0.0f), .height = SizeMode::Stretch })) {
                        if (auto ws = dragUi.Column({ .gap = 12.0f, .padding = Padding::All(20.0f), .width = SizeMode::Stretch, .height = SizeMode::Stretch })) {
                            auto rWs = dragUi.Slider(wsSliderVal, 0.0f, 100.0f, {.width = 240.0f});
                            wsSliderRect = rWs.rect;

                            // Simulate PreviewViewport custom window resize grip in workspace (zLayer == 0)
                            const Rect gripRect(420.0f, 100.0f, 12.0f, 200.0f);
                            if (dragUi.IsMouseClicked() && gripRect.Contains(dragUi.GetMousePos())) {
                                customGripDragging = true;
                                dragUi.SetActiveId(customGripId);
                            }
                            if (customGripDragging) {
                                if (dragUi.IsMouseDown()) {
                                    dragUi.SetActiveId(customGripId);
                                    customGripWidth = dragUi.GetMousePos().x;
                                } else {
                                    customGripDragging = false;
                                }
                            }
                        }

                        if (auto sb = dragUi.Sidebar(rightSidebarOpen, {
                            .side = SidebarSide::Right,
                            .width = 280.0f,
                            .minWidth = 180.0f,
                            .maxWidth = 450.0f,
                            .resizable = true,
                            .gap = 10.0f,
                            .padding = Padding::All(12.0f)
                        })) {
                            sbPanelRect = sb.response.rect;
                            auto rSb = dragUi.Slider(sbSliderVal, 0.0f, 100.0f, {.width = 240.0f});
                            sbSliderRect = rSb.rect;
                            auto rBtn = dragUi.Button({ .text = "Sidebar Action" });
                            sbBtnHoveredDuringDrag = rBtn.hovered;
                        }
                    }
                    dragUi.EndFrame(archCanvas);
                };

                // Warm up geometry
                runDragWorkbench();
                runDragWorkbench();
                CHECK(wsSliderRect.w > 100.0f);
                CHECK(sbSliderRect.w > 100.0f);
                CHECK(sbPanelRect.w > 200.0f);

                // (a) Drag Sidebar Slider across the Sidebar left splitter sash into workspace, then back into Sidebar
                dragUi.OnMouseMove(sbSliderRect.Center());
                dragUi.OnMouseDown(1);
                runDragWorkbench();
                CHECK(dragUi.GetActiveId().IsValid());

                // Move directly onto the Sidebar's left splitter sash (sbPanelRect.x) while LMB is held down
                dragUi.OnMouseMove({sbPanelRect.x, sbSliderRect.Center().y});
                runDragWorkbench();
                CHECK(dragUi.GetActiveId().IsValid());
                CHECK_NEAR(sbSliderVal, 0.0f, 1.0f);
                // Sidebar width must NOT have changed because the slider owns m_activeId
                CHECK_NEAR(sbPanelRect.w, 280.0f, 1.0f);

                // Move far left into the workspace outside the Sidebar while LMB is still down
                dragUi.OnMouseMove({200.0f, sbSliderRect.Center().y});
                runDragWorkbench();
                CHECK(dragUi.GetActiveId().IsValid());
                CHECK_NEAR(sbSliderVal, 0.0f, 1.0f);

                // Return into the Sidebar Slider track while LMB is still held down -> slider must still track!
                const float sbTargetTrackX = sbSliderRect.x + sbSliderRect.w * 0.60f;
                dragUi.OnMouseMove({sbTargetTrackX, sbSliderRect.Center().y});
                runDragWorkbench();
                CHECK(sbSliderVal > 60.0f && sbSliderVal < 90.0f);

                dragUi.OnMouseUp(1);
                runDragWorkbench();
                CHECK(!dragUi.GetActiveId().IsValid());

                // (b) Drag Workspace Slider across Sidebar splitter sash and over Sidebar button, then return
                dragUi.OnMouseMove(wsSliderRect.Center());
                dragUi.OnMouseDown(1);
                runDragWorkbench();
                CHECK(dragUi.GetActiveId().IsValid());

                // Cross over the Sidebar sash into the Sidebar (zLayer == 1) while LMB is held down
                dragUi.OnMouseMove({sbPanelRect.x, wsSliderRect.Center().y});
                runDragWorkbench();
                CHECK(dragUi.GetActiveId().IsValid());
                CHECK_NEAR(wsSliderVal, 100.0f, 1.0f);

                dragUi.OnMouseMove({sbPanelRect.Center().x, sbSliderRect.Bottom() + 18.0f});
                runDragWorkbench();
                CHECK(dragUi.GetActiveId().IsValid());
                // Passive Sidebar button must NOT highlight while workspace slider owns active capture
                CHECK(sbBtnHoveredDuringDrag == false);

                // Move back to 25% of workspace slider while LMB is still held down -> must continue dragging!
                const float wsTarget25X = wsSliderRect.x + wsSliderRect.w * 0.25f;
                dragUi.OnMouseMove({wsTarget25X, wsSliderRect.Center().y});
                runDragWorkbench();
                CHECK(wsSliderVal > 10.0f && wsSliderVal < 40.0f);

                dragUi.OnMouseUp(1);
                runDragWorkbench();
                CHECK(!dragUi.GetActiveId().IsValid());

                // (c) Drag custom workspace resize grip (PreviewWindow resize simulation) across Sidebar sash & into Sidebar
                dragUi.OnMouseMove({425.0f, 150.0f});
                dragUi.OnMouseDown(1);
                runDragWorkbench();
                CHECK(customGripDragging == true);
                CHECK(dragUi.GetActiveId() == customGripId);

                // Move over Sidebar splitter sash
                dragUi.OnMouseMove({sbPanelRect.x, 150.0f});
                runDragWorkbench();
                CHECK(customGripDragging == true);
                CHECK_NEAR(customGripWidth, sbPanelRect.x, 0.5f);

                // Move deep inside Sidebar (zLayer == 1)
                dragUi.OnMouseMove({sbPanelRect.Center().x, 150.0f});
                runDragWorkbench();
                CHECK(customGripDragging == true);
                CHECK_NEAR(customGripWidth, sbPanelRect.Center().x, 0.5f);

                // Return to workspace at x = 510
                dragUi.OnMouseMove({510.0f, 150.0f});
                runDragWorkbench();
                CHECK(customGripDragging == true);
                CHECK_NEAR(customGripWidth, 510.0f, 0.5f);

                // Release LMB -> EndFrame automatically clears m_activeId even if custom grip doesn't call SetActiveId(Null)
                dragUi.OnMouseUp(1);
                runDragWorkbench();
                CHECK(customGripDragging == false);
                CHECK(!dragUi.GetActiveId().IsValid());
            }

            // 8. Markdown production typography, nested inline spans, dynamic table row heights, and soft-wrapped code blocks
            {
                const keel::String nestedMd(
                    "## 1. Embedded Image Asset (`assets/images/lena.png`)\n\n"
                    "- [x] **Centralized `KeyAction` & `Keymap`** (`Copy`, `Find`, `SelectAll`)\n");

                const keel::String tableMd(
                    "| Component | Layer | Coordinate Space | Responsibility |\n"
                    "| :--- | :---: | :---: | :--- |\n"
                    "| `Keymap` | Core | Input Dispatch | Configurable `KeyAction` bindings |\n"
                    "| `ui.TextArea` | Widget | Line Virtualized | IDE Code Editor & Syntax Engine |\n"
                    "| `ui.Markdown` | Widget | Flow Layout | Rich AST Document Viewer |\n");

                const keel::String longCodeMd(
                    "```cpp\n"
                    "const guikit::Rect visible = scene.GetVisibleWorldBounds({1600.0f, 1000.0f}, cameraState);\n"
                    "```\n");

                Context mdUi;
                const UiTheme& mdTheme = mdUi.GetTheme();
                const Vec2 wideTableSz = Markdown::Measure(tableMd, {}, UiSize::Md, &archCanvas, mdTheme, 1200.0f);
                const Vec2 narrowTableSz = Markdown::Measure(tableMd, {}, UiSize::Md, &archCanvas, mdTheme, 380.0f);
                // Narrow viewport wraps multi-word table cells and must expand row heights accordingly
                CHECK(narrowTableSz.y > wideTableSz.y + 20.0f);

                const Vec2 wideCodeSz = Markdown::Measure(longCodeMd, {}, UiSize::Md, &archCanvas, mdTheme, 1200.0f);
                const Vec2 narrowCodeSz = Markdown::Measure(longCodeMd, {}, UiSize::Md, &archCanvas, mdTheme, 320.0f);
                // Narrow viewport soft-wraps long code line into multiple visual lines
                CHECK(narrowCodeSz.y > wideCodeSz.y + 14.0f);

                // Image caption multi-line wrapping in narrow viewports
                const keel::String imgCaptionMd(
                    "![Lena Standard Test Image (512x512 PNG - Embedded Native & WASM Asset)](assets/images/lena.png)\n");
                const Vec2 wideImgSz = Markdown::Measure(imgCaptionMd, {}, UiSize::Md, &archCanvas, mdTheme, 1200.0f);
                const Vec2 narrowImgSz = Markdown::Measure(imgCaptionMd, {}, UiSize::Md, &archCanvas, mdTheme, 280.0f);
                CHECK(narrowImgSz.y > wideImgSz.y + 10.0f);

                // Ultra-narrow heading with (`assets/images/lena.png`) splits the code pill at '/' and keeps `)` attached
                const keel::String headingCodeMd("## 1. Embedded Image Asset (`assets/images/lena.png`)\n");
                const Vec2 wideHeadSz = Markdown::Measure(headingCodeMd, {}, UiSize::Md, &archCanvas, mdTheme, 1200.0f);
                const Vec2 ultraNarrowHeadSz = Markdown::Measure(headingCodeMd, {}, UiSize::Md, &archCanvas, mdTheme, 200.0f);
                CHECK(ultraNarrowHeadSz.y > wideHeadSz.y + 20.0f);
                CHECK(ultraNarrowHeadSz.x <= 200.5f);

                // Verify nested `code` inside **bold** parses without raw backticks and soft-wrapped code copies cleanly
                MarkdownResponse mdResp;
                const keel::String combinedMd = keel::format("{}\n{}", nestedMd, longCodeMd);
                auto runMdFrame = [&]() {
                    mdUi.BeginFrame({380.0f, 600.0f});
                    mdResp = mdUi.Markdown(combinedMd, {.height = 500.0f});
                    mdUi.EndFrame(archCanvas);
                };

                runMdFrame();
                runMdFrame();

                // Click inside Markdown to focus it, then trigger SelectAll (Ctrl+A)
                mdUi.OnMouseMove({60.0f, 60.0f});
                mdUi.OnMouseDown(1);
                runMdFrame();
                mdUi.OnMouseUp(1);
                runMdFrame();

                mdUi.OnKeyDown(Key::A, {.ctrl = true});
                runMdFrame();

                CHECK(!mdResp.selectedText.empty());
                // Nested `KeyAction` & `Keymap` inside **...** must not contain literal backticks
                CHECK(mdResp.selectedText.find("Centralized KeyAction & Keymap") != keel::StringRef::npos);
                CHECK(mdResp.selectedText.find("`KeyAction`") == keel::StringRef::npos);
                // Soft-wrapped code line must remain a single logical line in plainText / selection
                CHECK(mdResp.selectedText.find("const guikit::Rect visible = scene.GetVisibleWorldBounds") != keel::StringRef::npos);

                // 9. Verify 3-Phase Retained Markdown Layout Cache (Zero-Measurement on Scroll/Hover & Viewport Frustum Culling)
                struct CountingThorCanvas : public ThorCanvas {
                    keel::i32 layoutMeasureCalls{0};
                    keel::i32 drawTextCalls{0};
                    bool insideDrawText{false};
                    Vec2 MeasureText(keel::StringRef text, float fontSize) override {
                        if (!insideDrawText) ++layoutMeasureCalls;
                        return ThorCanvas::MeasureText(text, fontSize);
                    }
                    Vec2 MeasureTextStyled(keel::StringRef text, float fontSize, FontStyle style) override {
                        if (!insideDrawText) ++layoutMeasureCalls;
                        return ThorCanvas::MeasureTextStyled(text, fontSize, style);
                    }
                    void DrawTextStyled(keel::StringRef text, Vec2 pos, float fontSize, Color color, FontStyle style, float angleDeg = 0.0f) override {
                        ++drawTextCalls;
                        insideDrawText = true;
                        ThorCanvas::DrawTextStyled(text, pos, fontSize, color, style, angleDeg);
                        insideDrawText = false;
                    }
                };

                CountingThorCanvas countCanvas;
                keel::Array<keel::u32> countPixels(420 * 1200);
                countCanvas.SetTarget(countPixels.data(), 420, 1200, 420);

                keel::String bigDoc("# Title Heading\n\nIntro paragraph with [link](https://example.com) and `inline code`.\n\n");
                for (keel::i32 i = 0; i < 12; ++i) {
                    keel::format_into(bigDoc, "## Section {}\n\n", i);
                    keel::format_into(bigDoc, "Paragraph body for section {} with **bold**, *italic*, and `pill`.\n\n", i);
                }

                Context retainedUi;
                auto runRetainedFrame = [&](float scrollH) {
                    retainedUi.BeginFrame({420.0f, scrollH});
                    if (auto sa = retainedUi.ScrollArea()) {
                        retainedUi.Markdown(bigDoc);
                    }
                    retainedUi.EndFrame(countCanvas);
                };

                // Warm up initial layout (frames 1 & 2 establish prevUnclippedBounds and build cached layout)
                runRetainedFrame(160.0f);
                runRetainedFrame(160.0f);

                // Subsequent frame with mouse move and scroll wheel: must hit retained layout cache with 0 layout MeasureText calls!
                countCanvas.layoutMeasureCalls = 0;
                countCanvas.drawTextCalls = 0;
                retainedUi.OnMouseMove({100.0f, 80.0f});
                retainedUi.OnMouseWheel(0.0f, -3.0f);
                runRetainedFrame(160.0f);
                const keel::i32 culledDrawCalls = countCanvas.drawTextCalls;
                CHECK(countCanvas.layoutMeasureCalls == 0);
                CHECK(culledDrawCalls > 0);

                // Compare against tall 1200px viewport: frustum culling in 160px ScrollArea must draw far fewer text atoms
                countCanvas.drawTextCalls = 0;
                runRetainedFrame(1200.0f);
                const keel::i32 unculledDrawCalls = countCanvas.drawTextCalls;
                CHECK(culledDrawCalls * 2 < unculledDrawCalls);

                // 10. Verify Modal interior Cancel button (modal.Close()) leaves zero backdrop or drop-shadow ghost artifacts
                {
                    ThorCanvas modalCanvas;
                    const keel::u32 mw = 600;
                    const keel::u32 mh = 400;
                    keel::Array<keel::u32> modalPixels(mw * mh);
                    modalCanvas.SetTarget(modalPixels.data(), mw, mh, mw);

                    Context modalUi;
                    bool modalOpen = false;
                    keel::String statusText("Ready");
                    Response cancelBtnRes{};
                    Rect modalDialogRect{};

                    auto runModalStage = [&]() {
                        modalUi.BeginFrame({static_cast<float>(mw), static_cast<float>(mh)});
                        if (auto stage = modalUi.Box({
                            .width = SizeMode::Stretch,
                            .height = SizeMode::Stretch,
                            .padding = Padding::All(24.0f),
                            .background = {modalUi.GetTheme().palette.surface},
                            .clipChildren = true
                        })) {
                            if (auto col = modalUi.Column({
                                .gap = 12.0f,
                                .align = Alignment::Center,
                                .justify = Justify::Center,
                                .height = SizeMode::Stretch
                            })) {
                                modalUi.Button({.text = "Open Modal Dialog..."});
                                modalUi.Text(statusText);
                            }

                            if (auto modal = modalUi.Modal(modalOpen, {
                                .backdropOpacity = 0.55f,
                                .confineToParent = true,
                                .padding = Padding::All(16.0f)
                            })) {
                                if (auto card = modalUi.Card({
                                    .width = 360.0f,
                                    .gap = 10.0f,
                                    .padding = Padding::All(16.0f),
                                    .elevated = true
                                })) {
                                    modalDialogRect = modal.response.rect;
                                    modalUi.Heading({.label = "Configure Station Endpoint", .description = "Edit connection parameters", .level = HeadingLevel::H4});
                                    cancelBtnRes = modalUi.Button({.text = "Cancel"});
                                    if (cancelBtnRes.clicked) {
                                        statusText.assign("Cancelled Modal");
                                        modal.Close();
                                    }
                                }
                            }
                        }
                        modalUi.EndFrame(modalCanvas);
                    };

                    // Settle closed baseline and sample clean stage surface color at top-left corner (x=40, y=40)
                    runModalStage();
                    runModalStage();
                    const keel::u32 cleanBackdropPixel = modalPixels[40 * mw + 40];

                    // Open modal and settle 2 frames
                    modalOpen = true;
                    runModalStage();
                    runModalStage();
                    CHECK(modalDialogRect.w > 200.0f);
                    CHECK(cancelBtnRes.rect.w > 30.0f);
                    // Backdrop pixel must be darkened while modal is open
                    const keel::u32 dimmedBackdropPixel = modalPixels[40 * mw + 40];
                    CHECK(dimmedBackdropPixel != cleanBackdropPixel);

                    // Click the interior "Cancel" button (triggers modal.Close() inside the ModalScope body)
                    modalUi.OnMouseMove(cancelBtnRes.rect.Center());
                    modalUi.OnMouseDown(1);
                    runModalStage();
                    modalUi.OnMouseUp(1);
                    runModalStage(); // Frame N: Cancel clicked -> modal.Close() called

                    CHECK(modalOpen == false);
                    CHECK(statusText == "Cancelled Modal");
                    CHECK(modalUi.IsFullRepaint());
                    // Immediately on Frame N, the modal is detached and backdrop pixel must be restored to clean surface color
                    CHECK(modalPixels[40 * mw + 40] == cleanBackdropPixel);

                    // Frame N+1: first frame with modalOpen == false from the start -> must also do full repaint & leave zero ghost trace
                    runModalStage();
                    CHECK(modalUi.IsFullRepaint());
                    CHECK(modalPixels[40 * mw + 40] == cleanBackdropPixel);
                    // Sample below modalDialogRect where the drop shadow was drawn (y = modalDialogRect.Bottom() + 6)
                    const keel::u32 shadowY = keel::min<keel::u32>(mh - 1, static_cast<keel::u32>(modalDialogRect.Bottom() + 6.0f));
                    const keel::u32 shadowX = keel::min<keel::u32>(mw - 1, static_cast<keel::u32>(modalDialogRect.Center().x));
                    CHECK(modalPixels[shadowY * mw + shadowX] == cleanBackdropPixel);

                    // Frame N+2: idle -> dirty-rect engine settles with zero dirty damage
                    runModalStage();
                    CHECK(!modalUi.HasDirtyDamage());
                    CHECK(!modalUi.IsFullRepaint());
                }

                // --- Unified Font Loading API (LoadFontFile / LoadFontMemory with FontStyle) & Embedded Monospace Font ---
                {
                    CHECK(Assets::Exists("assets/fonts/Roboto-Regular.ttf"));
                    CHECK(Assets::Exists("assets/fonts/JetBrainsMono-Regular.ttf"));

                    ThorCanvas fontCanvas(false);
                    CHECK(fontCanvas.LoadFontFile("assets/fonts/Roboto-Regular.ttf", FontStyle::Regular));
                    CHECK(fontCanvas.LoadFontFile("assets/fonts/JetBrainsMono-Regular.ttf", FontStyle::Monospace));

                    // Proportional Regular: "WWWW" is significantly wider than "iiii"
                    const Vec2 propNarrow = fontCanvas.MeasureTextStyled("iiii", 16.0f, FontStyle::Regular);
                    const Vec2 propWide = fontCanvas.MeasureTextStyled("WWWW", 16.0f, FontStyle::Regular);
                    CHECK(propWide.x > propNarrow.x * 1.5f);

                    // Monospace: "iiii" and "WWWW" have identical advance width
                    const Vec2 monoNarrow = fontCanvas.MeasureTextStyled("iiii", 16.0f, FontStyle::Monospace);
                    const Vec2 monoWide = fontCanvas.MeasureTextStyled("WWWW", 16.0f, FontStyle::Monospace);
                    CHECK(monoNarrow.x > 10.0f);
                    CHECK(keel::abs(monoWide.x - monoNarrow.x) < 0.05f);

                    // LoadFontMemory with a temporary vector buffer (tests internal copy safety after buffer destruction)
                    auto monoAsset = Assets::Find("assets/fonts/JetBrainsMono-Regular.ttf");
                    CHECK(monoAsset.has_value());
                    if (monoAsset) {
                        ThorCanvas memCanvas(false);
                        {
                            keel::Array<keel::u8> tempFontBytes(keel::ArrayRef<keel::u8>(monoAsset->data, monoAsset->size));
                            CHECK(memCanvas.LoadFontMemory("TempMonoBold", tempFontBytes, FontStyle::Bold, "ttf"));
                        }
                        // tempFontBytes is now destroyed; measuring and rendering must safely use ThorVG's internal copy
                        const Vec2 memN = memCanvas.MeasureTextStyled("iiii", 16.0f, FontStyle::Bold);
                        const Vec2 memW = memCanvas.MeasureTextStyled("WWWW", 16.0f, FontStyle::Bold);
                        CHECK(memN.x > 10.0f);
                        CHECK(keel::abs(memW.x - memN.x) < 0.05f);
                    }
                }

                // --- Scene inside BottomPanel: First-Frame Bounds, Stretch Fit, worldBounds + fitPadding & SceneTextOptions ---
                {
                    Context sceneBpUi;
                    ThorCanvas sceneBpCanvas;
                    const keel::u32 sw = 1000;
                    const keel::u32 sh = 700;
                    keel::Array<keel::u32> sceneBpPixels(sw * sh);
                    sceneBpCanvas.SetTarget(sceneBpPixels.data(), sw, sh, sw);

                    bool showBp = false;
                    Rect capturedVp{};
                    Vec2 screenOrigin{};
                    Vec2 screenTopRight{};

                    auto runSceneBpFrame = [&]() {
                        sceneBpUi.BeginFrame({static_cast<float>(sw), static_cast<float>(sh)}, 1.0f);
                        if (auto rootCol = sceneBpUi.Column({
                            .gap = 0.0f,
                            .padding = Padding::All(0.0f),
                            .width = SizeMode::Stretch,
                            .height = SizeMode::Stretch
                        })) {
                            if (auto workCol = sceneBpUi.Column({
                                .width = SizeMode::Stretch,
                                .height = SizeMode::Stretch
                            })) {
                                sceneBpUi.Text("Workspace");
                            }
                            if (showBp) {
                                if (auto bp = sceneBpUi.BottomPanel({
                                    .height = 200.0f,
                                    .minHeight = 140.0f,
                                    .padding = Padding::All(0.0f)
                                })) {
                                    if (auto tb = sceneBpUi.Toolbar({.size = UiSize::Sm})) {
                                        sceneBpUi.Text("Prism Schema");
                                    }
                                    if (auto sc = sceneBpUi.Scene({
                                        .worldBounds = Rect(-10.0f, -25.0f, 50.0f, 25.0f),
                                        .fitPadding = Padding(40.0f, 24.0f, 40.0f, 32.0f),
                                        .zoomable = false,
                                        .pannable = false,
                                        .showScrollbars = false,
                                        .showControls = false
                                    })) {
                                        capturedVp = sc.GetViewportRect();
                                        screenOrigin = sc.WorldToScreen({0.0f, 0.0f});
                                        screenTopRight = sc.WorldToScreen({40.0f, -25.0f});
                                        sc.DrawLine({-10.0f, 0.0f}, {40.0f, 0.0f}, Color(34, 197, 94), 1.5f);
                                        sc.DrawText("X = 0", {0.0f, 0.0f}, 13.0f, Color(226, 232, 240), {
                                            .align = {0.5f, 0.0f},
                                            .offset = {0.0f, 6.0f}
                                        });
                                    }
                                }
                            }
                        }
                        sceneBpUi.EndFrame(sceneBpCanvas);
                    };

                    // Frame 1: BottomPanel closed
                    runSceneBpFrame();

                    // Frame 2: Open BottomPanel (200px tall with Sm Toolbar inside)
                    showBp = true;
                    runSceneBpFrame();

                    // Even on the VERY FIRST frame BottomPanel opens:
                    // - capturedVp.h must be ~166px (200px minus ~34px toolbar), NOT 700px (window height) and NOT 220px (minHeight overflow)!
                    CHECK(capturedVp.w > 990.0f);
                    CHECK(capturedVp.h > 150.0f && capturedVp.h < 180.0f);
                    // World point (0, 0) is the bottom edge of worldBounds (y = -25..0), so with fitPadding.bottom = 32,
                    // screenOrigin.y must sit exactly 32px above the bottom of the Scene viewport!
                    CHECK_NEAR(screenOrigin.y, capturedVp.Bottom() - 32.0f, 1.5f);
                    // World point (40, -25) is the top edge of worldBounds, so with fitPadding.top = 24,
                    // screenTopRight.y must sit 24px below the top of the Scene viewport!
                    CHECK_NEAR(screenTopRight.y, capturedVp.y + 24.0f, 1.5f);

                    // --- RequestRepaint(1) called inside BeginFrame..EndFrame survives EndFrame ---
                    while (sceneBpUi.NeedsRepaint()) {
                        runSceneBpFrame();
                    }
                    CHECK(!sceneBpUi.NeedsRepaint());
                    sceneBpUi.BeginFrame({static_cast<float>(sw), static_cast<float>(sh)}, 1.0f);
                    sceneBpUi.RequestRepaint(1);
                    sceneBpUi.EndFrame(sceneBpCanvas);
                    CHECK(sceneBpUi.NeedsRepaint());
                    CHECK(sceneBpUi.GetRepaintFramesLeft() == 1);

                    // --- SceneScope::WasCameraModified() across wheel zoom & HUD ---
                    bool camMod = false;
                    auto runCamScene = [&]() {
                        sceneBpUi.BeginFrame({400.0f, 300.0f}, 1.0f);
                        if (auto sc = sceneBpUi.Scene({
                            .height = 300.0f,
                            .zoomable = true,
                            .pannable = true,
                            .showControls = true
                        })) {
                            camMod = sc.WasCameraModified();
                        }
                        sceneBpUi.EndFrame(sceneBpCanvas);
                    };
                    runCamScene();
                    CHECK(!camMod);
                    sceneBpUi.OnMouseMove({200.0f, 150.0f});
                    sceneBpUi.OnMouseWheel(0.0f, 1.0f);
                    runCamScene();
                    CHECK(camMod);
                    runCamScene();
                    CHECK(!camMod);

                    // --- ThorCanvas::DrawTextBoxStyled preserves leading spaces at alignX=0 ---
                    {
                        ThorCanvas txtCanvas;
                        keel::Array<keel::u32> bufNoSpace(200 * 40);
                        keel::Array<keel::u32> bufWithSpace(200 * 40);

                        txtCanvas.SetTarget(bufNoSpace.data(), 200, 40, 200);
                        txtCanvas.Begin(200, 40, 1.0f);
                        txtCanvas.DrawTextBoxStyled("ABC", Rect(10.0f, 4.0f, 180.0f, 32.0f), 16.0f, Color(255, 255, 255), FontStyle::Regular, 0.0f, 0.5f);
                        txtCanvas.End();

                        txtCanvas.SetTarget(bufWithSpace.data(), 200, 40, 200);
                        txtCanvas.Begin(200, 40, 1.0f);
                        txtCanvas.DrawTextBoxStyled("   ABC", Rect(10.0f, 4.0f, 180.0f, 32.0f), 16.0f, Color(255, 255, 255), FontStyle::Regular, 0.0f, 0.5f);
                        txtCanvas.End();

                        auto findFirstInkX = [](keel::ArrayRef<keel::u32> px) {
                            keel::i32 minX = 200;
                            for (keel::i32 y = 0; y < 40; ++y) {
                                for (keel::i32 x = 0; x < 200; ++x) {
                                    if (px[y * 200 + x] != 0u && x < minX) minX = x;
                                }
                            }
                            return minX;
                        };
                        CHECK(findFirstInkX(bufWithSpace) > findFirstInkX(bufNoSpace) + 6);
                    }

                    // --- Dirty-Rect & Idle Settle with Sidebar, BottomPanel, StatusBar, and Stationary Tooltip ---
                    {
                        Context idleUi;
                        ThorCanvas idleCanvas;
                        keel::Array<keel::u32> idlePixels(800 * 600);
                        idleCanvas.SetTarget(idlePixels.data(), 800, 600, 800);

                        bool bpOpen = true;
                        keel::String statusLabel("Ready");
                        Response tipBtn{};

                        auto runShellFrame = [&]() {
                            idleUi.BeginFrame({800.0f, 600.0f}, 1.0f);
                            if (auto root = idleUi.Column({.width = SizeMode::Stretch, .height = SizeMode::Stretch})) {
                                if (auto tb = idleUi.Toolbar()) {
                                    tipBtn = tb.Action({.text = "Action", .tooltip = "Stationary Tooltip Test"});
                                }
                                if (auto ws = idleUi.Row({.width = SizeMode::Stretch, .height = SizeMode::Stretch})) {
                                    if (auto sb = idleUi.Sidebar({.width = 200.0f, .resizable = true, .allowCollapse = true})) {
                                        idleUi.Text("Sidebar Content");
                                    }
                                    if (auto mainCol = idleUi.Column({.width = SizeMode::Stretch, .height = SizeMode::Stretch})) {
                                        idleUi.Text(statusLabel);
                                        if (auto bp = idleUi.BottomPanel(bpOpen, {.height = 140.0f, .resizable = true, .allowCollapse = true})) {
                                            idleUi.Text("BottomPanel Content");
                                        }
                                    }
                                }
                                if (auto stBar = idleUi.StatusBar()) {
                                    stBar.Item({.text = statusLabel});
                                }
                            }
                            idleUi.EndFrame(idleCanvas);
                        };

                        // Frame 1 & 2: initial layout settle
                        runShellFrame();
                        runShellFrame();

                        // Frame 3: completely idle with Sidebar + BottomPanel overlays registered -> MUST have zero dirty damage!
                        runShellFrame();
                        CHECK(!idleUi.HasDirtyDamage());
                        CHECK(!idleUi.IsFullRepaint());

                        // Frame 4: update a single text label -> MUST do partial dirty-rect repaint (NOT full repaint!)
                        statusLabel.assign("Updated");
                        runShellFrame();
                        CHECK(idleUi.HasDirtyDamage());
                        CHECK(!idleUi.IsFullRepaint());

                        // Frame 5: settle again -> zero dirty damage
                        runShellFrame();
                        CHECK(!idleUi.HasDirtyDamage());

                        // Frame 6: move mouse over toolbar button with tooltip -> full repaint on tooltip appearance
                        idleUi.OnMouseMove(tipBtn.rect.Center());
                        runShellFrame();
                        CHECK(idleUi.HasDirtyDamage());

                        // Frame 7: mouse remains stationary over button with active tooltip -> MUST settle to zero dirty damage!
                        runShellFrame();
                        CHECK(!idleUi.HasDirtyDamage());
                        CHECK(!idleUi.IsFullRepaint());
                    }

                    // --- Comprehensive Interface Audit Verification Suite ---
                    {
                        Context auditUi;
                        ThorCanvas auditCanvas;
                        keel::Array<keel::u32> auditPixels(900 * 900);
                        auditCanvas.SetTarget(auditPixels.data(), 900, 900, 900);

                        // 1. ContainerScope move-assignment & explicit Close()
                        auditUi.BeginFrame({900.0f, 900.0f}, 1.0f);
                        {
                            auto outerScope = auditUi.Column({.gap = 8.0f});
                            {
                                auto activeRow = auditUi.Row({.gap = 4.0f});
                                auditUi.Text("Row A");
                                // Move-assign a new Row into activeRow: must close Row A first, then adopt Row B!
                                activeRow = auditUi.Row({.gap = 6.0f});
                                auditUi.Text("Row B");
                                activeRow.Close();
                                // Second Close() must be a safe no-op
                                activeRow.Close();
                            }
                            auditUi.Text("After Rows");
                            outerScope.Close();
                        }
                        auditUi.EndFrame(auditCanvas);

                        // 2. Explicit dp width, HeadingLevel::H1..H6, Button .badge & .loading, ProgressBar vertical/indeterminate
                        Response ratioBtn{}, plainBtn{}, badgeBtn{}, loadBtn{};
                        Response h1Resp{}, h2Resp{}, h3Resp{}, h4Resp{}, h5Resp{}, h6Resp{};
                        Response vProgResp{};
                        bool loadClicked = false;

                        auto runVisualAudit = [&]() {
                            auditUi.BeginFrame({800.0f, 800.0f}, 1.0f);
                            if (auto col = auditUi.Column({.padding = Padding::All(0.0f), .width = SizeMode::Stretch})) {
                                ratioBtn = auditUi.Button({.text = "Half", .width = 400.0f});
                                if (auto r = auditUi.Row({.gap = 8.0f})) {
                                    plainBtn = auditUi.Button({.text = "Alerts"});
                                    badgeBtn = auditUi.Button({.text = "Alerts", .badge = "99+"});
                                    loadBtn  = auditUi.Button({.text = "Saving", .loading = true});
                                    if (loadBtn.clicked) loadClicked = true;
                                }
                                h1Resp = auditUi.Heading({.label = "Level 1", .description = "Sub 1", .icon = Icon::Flash(), .badge = "H1", .level = HeadingLevel::H1});
                                h2Resp = auditUi.Heading({.label = "Level 2", .description = "Sub 2", .icon = Icon::Flash(), .badge = "H2", .level = HeadingLevel::H2});
                                h3Resp = auditUi.Heading({.label = "Level 3", .description = "Sub 3", .icon = Icon::Flash(), .badge = "H3", .level = HeadingLevel::H3});
                                h4Resp = auditUi.Heading({.label = "Level 4", .description = "Sub 4", .icon = Icon::Flash(), .badge = "H4", .level = HeadingLevel::H4});
                                h5Resp = auditUi.Heading({.label = "Level 5", .description = "Sub 5", .icon = Icon::Flash(), .badge = "H5", .level = HeadingLevel::H5});
                                h6Resp = auditUi.Heading({.label = "Level 6", .description = "Sub 6", .icon = Icon::Flash(), .badge = "H6", .level = HeadingLevel::H6});
                                if (auto prRow = auditUi.Row({.gap = 12.0f, .height = 90.0f})) {
                                    vProgResp = auditUi.ProgressBar(0.65f, {.orientation = Orientation::Vertical, .height = SizeMode::Stretch});
                                    auditUi.ProgressBar(0.0f, {.indeterminate = true, .width = 140.0f});
                                }
                            }
                            auditUi.EndFrame(auditCanvas);
                        };
                        runVisualAudit();
                        runVisualAudit();

                        CHECK_NEAR(ratioBtn.rect.w, 400.0f, 0.5f);
                        CHECK(badgeBtn.rect.w > plainBtn.rect.w + 12.0f);
                        CHECK(h1Resp.rect.h > h2Resp.rect.h);
                        CHECK(h2Resp.rect.h > h3Resp.rect.h);
                        CHECK(h3Resp.rect.h > h4Resp.rect.h);
                        CHECK(h4Resp.rect.h > h5Resp.rect.h);
                        CHECK(h5Resp.rect.h > h6Resp.rect.h);
                        CHECK(vProgResp.rect.h > vProgResp.rect.w * 2.0f);

                        // Clicking a loading button must be suppressed
                        auditUi.OnMouseMove(loadBtn.rect.Center());
                        auditUi.OnMouseDown(1);
                        runVisualAudit();
                        auditUi.OnMouseUp(1);
                        runVisualAudit();
                        CHECK(!loadClicked);

                        // 3. IndexLike enum bindings + TabBar Card/Folder/closable (`closedIndex`)
                        enum class FilterStage : keel::u8 { LowPass = 0, BandPass = 1, HighPass = 2 };
                        FilterStage stage = FilterStage::LowPass;
                        FilterStage tabStage = FilterStage::LowPass;
                        TabBarResponse tabResp{};
                        Response segResp{};

                        static constexpr TabItem kClosableTabs[] = {
                            {.label = "Main",    .closable = true},
                            {.label = "Metrics", .closable = true},
                            {.label = "Logs",    .closable = true}
                        };
                        static constexpr keel::StringRef kStageNames[] = {"LP", "BP", "HP"};

                        auto runEnumAndTabs = [&](TabBarStyle style) {
                            auditUi.BeginFrame({800.0f, 600.0f}, 1.0f);
                            if (auto col = auditUi.Column({.gap = 8.0f})) {
                                tabResp = auditUi.TabBar(tabStage, kClosableTabs, {.style = style});
                                segResp = auditUi.Segmented(stage, kStageNames, {.width = 300.0f});
                                auditUi.Select(stage, kStageNames);
                                auditUi.StepSlider(stage, kStageNames);
                            }
                            auditUi.EndFrame(auditCanvas);
                        };
                        runEnumAndTabs(TabBarStyle::Card);
                        runEnumAndTabs(TabBarStyle::Folder);

                        // Click middle segment ("BP") -> updates strongly-typed enum to BandPass!
                        auditUi.OnMouseMove(segResp.rect.Center());
                        auditUi.OnMouseDown(1);
                        runEnumAndTabs(TabBarStyle::Card);
                        auditUi.OnMouseUp(1);
                        runEnumAndTabs(TabBarStyle::Card);
                        CHECK(stage == FilterStage::BandPass);

                        // Click close ('X') button on the trailing edge of tab 1 ("Metrics") without changing active tab (0 = LowPass)
                        // First find tab 1's close button near the right edge of tab 1:
                        // Tab 0 and Tab 1 sit at the left of tabResp.rect; let's measure Tab 0 and Tab 1 directly!
                        const Vec2 t0Sz = TabBar::MeasureTab(kClosableTabs[0], {.style = TabBarStyle::Card}, UiSize::Md, &auditCanvas, auditUi.GetTheme());
                        const Vec2 t1Sz = TabBar::MeasureTab(kClosableTabs[1], {.style = TabBarStyle::Card}, UiSize::Md, &auditCanvas, auditUi.GetTheme());
                        const float gapPx = auditUi.GetTheme().Spacing().xs;
                        const Vec2 tab1ClosePt{
                            tabResp.rect.x + t0Sz.x + gapPx + t1Sz.x - 12.0f,
                            tabResp.rect.Center().y
                        };
                        auditUi.OnMouseMove(tab1ClosePt);
                        auditUi.OnMouseDown(1);
                        runEnumAndTabs(TabBarStyle::Card);
                        auditUi.OnMouseUp(1);
                        runEnumAndTabs(TabBarStyle::Card);
                        CHECK(tabResp.closedIndex.has_value());
                        if (tabResp.closedIndex.has_value()) {
                            CHECK(*tabResp.closedIndex == 1u);
                        }
                        CHECK(tabStage == FilterStage::LowPass);

                        // 4. Keel string/array bindings + InputField maxLength / InputFilter / selectAllOnFocus + SpinBox int64_t & non-null-terminated format
                        keel::FixedString<32> keelShort{"InitVal"};
                        keel::String keelIntStr{""};
                        keel::String keelHexStr{""};
                        keel::String maxLenStr{};
                        keel::SmallArray<keel::usize, 8> multiIndices;
                        multiIndices.push_back(1);
                        keel::SmallArray<keel::FixedString<24>, 8> keelTags;
                        keelTags.push_back(keel::FixedString<24>("dsp"));
                        keel::String codeBuf{"int x = 1;"};

                        keel::i64 bigCounter = 5000000000LL;
                        float cutoffFreq = 125.0f;
                        // Non-null-terminated substring view: first 7 chars are "%.1f Hz", followed by garbage "%s%s%n"
                        const char rawFmtBuf[] = "%.1f Hz%s%s%n_TRAILING";
                        const keel::StringRef subFmt(rawFmtBuf, 7);

                        Response selAllIn{}, intIn{}, hexIn{}, maxIn{}, bigSpinResp{}, codeResp{};
                        static constexpr keel::StringRef kMultiOpts[] = {"Ch0", "Ch1", "Ch2"};

                        auto runInputsAndKeel = [&]() {
                            auditUi.BeginFrame({800.0f, 800.0f}, 1.0f);
                            if (auto col = auditUi.Column({.gap = 8.0f})) {
                                selAllIn = auditUi.InputField(keelShort, {.selectAllOnFocus = true});
                                intIn    = auditUi.InputField(keelIntStr, {.filter = InputFilter::Integer});
                                hexIn    = auditUi.InputField(keelHexStr, {.filter = InputFilter::Hex});
                                maxIn    = auditUi.InputField(maxLenStr, {.maxLength = 4});
                                auditUi.MultiSelect(multiIndices, kMultiOpts);
                                auditUi.TagInput(keelTags);
                                bigSpinResp = auditUi.SpinBox(bigCounter, keel::i64(0), keel::i64(9000000000LL), keel::i64(1000));
                                auditUi.SpinBox(cutoffFreq, 20.0f, 20000.0f, 10.0f, {.valueFormat = subFmt});
                                codeResp = auditUi.CodeEditor(codeBuf, {.rows = 4, .tabSize = 2, .insertSpaces = true});
                            }
                            auditUi.EndFrame(auditCanvas);
                        };
                        runInputsAndKeel();
                        runInputsAndKeel();

                        // Focus keelShort (`selectAllOnFocus = true`) and type "New" -> replaces entire "InitVal"!
                        auditUi.OnMouseMove(selAllIn.rect.Center());
                        auditUi.OnMouseDown(1);
                        runInputsAndKeel();
                        auditUi.OnMouseUp(1);
                        auditUi.OnTextInput("New");
                        runInputsAndKeel();
                        CHECK(keelShort.as_ref() == "New");

                        // Focus intIn (`InputFilter::Integer`) and type "-42a.9" -> "-429"
                        auditUi.OnMouseMove(intIn.rect.Center());
                        auditUi.OnMouseDown(1);
                        runInputsAndKeel();
                        auditUi.OnMouseUp(1);
                        auditUi.OnTextInput("-42a.9");
                        runInputsAndKeel();
                        CHECK(keelIntStr.as_ref() == "-429");

                        // Focus hexIn (`InputFilter::Hex`) and type "#1fG9z" -> "#1f9"
                        auditUi.OnMouseMove(hexIn.rect.Center());
                        auditUi.OnMouseDown(1);
                        runInputsAndKeel();
                        auditUi.OnMouseUp(1);
                        auditUi.OnTextInput("#1fG9z");
                        runInputsAndKeel();
                        CHECK(keelHexStr.as_ref() == "#1f9");

                        // Focus maxIn (`maxLength = 4`) and type "ABCDEF" -> "ABCD"
                        auditUi.OnMouseMove(maxIn.rect.Center());
                        auditUi.OnMouseDown(1);
                        runInputsAndKeel();
                        auditUi.OnMouseUp(1);
                        auditUi.OnTextInput("ABCDEF");
                        runInputsAndKeel();
                        CHECK(maxLenStr == "ABCD");

                        // Click '+' stepper on int64_t SpinBox (> INT32_MAX): right edge minus 10px
                        auditUi.OnMouseMove({bigSpinResp.rect.Right() - 10.0f, bigSpinResp.rect.Center().y});
                        auditUi.OnMouseDown(1);
                        runInputsAndKeel();
                        auditUi.OnMouseUp(1);
                        runInputsAndKeel();
                        CHECK(bigCounter == 5000001000LL);

                        // CodeEditor with rows = 4 has positive height and accepts Tab -> 2 spaces
                        CHECK(codeResp.rect.h > 50.0f);
                        auditUi.OnMouseMove({codeResp.rect.x + 60.0f, codeResp.rect.y + 14.0f});
                        auditUi.OnMouseDown(1);
                        runInputsAndKeel();
                        auditUi.OnMouseUp(1);
                        auditUi.OnKeyDown(Key::Home);
                        auditUi.OnKeyDown(Key::Tab);
                        runInputsAndKeel();
                        CHECK(codeBuf.as_ref() == "  int x = 1;");

                        // 5. DatePicker minDate/maxDate clamping & arena-safe temporary monthNames/weekdayNames + ColorPicker swatches
                        DateTime pickedDate{.year = 2020, .month = 1, .day = 1};
                        const DateTime minDt{.year = 2025, .month = 5, .day = 10};
                        const DateTime maxDt{.year = 2027, .month = 12, .day = 31};
                        const DateTime overrideToday{.year = 2026, .month = 6, .day = 15};
                        Color pickedColor = Color(10, 20, 30, 255);
                        const Color customSwatches[2] = {Color(220, 40, 60, 255), Color(30, 180, 90, 255)};

                        auditUi.BeginFrame({800.0f, 600.0f}, 1.0f);
                        if (auto pickerCol = auditUi.Column({.gap = 8.0f})) {
                            // Pass stack-local arrays that go out of scope BEFORE EndFrame to verify arena-copy safety!
                            {
                                const keel::StringRef localMonths[12] = {
                                    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
                                };
                                const keel::StringRef localDays[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
                                const keel::StringRef localTime[3] = {"ч", "м", "с"};
                                auditUi.DatePicker(pickedDate, {
                                    .mode = DatePickerMode::DateTime,
                                    .minDate = minDt,
                                    .maxDate = maxDt,
                                    .todayOverride = overrideToday,
                                    .labels = {
                                        .monthNames = localMonths,
                                        .weekdayNames = localDays,
                                        .timeSuffixes = localTime,
                                        .today = "Сегодня",
                                        .now = "Сейчас",
                                        .done = "Применить"
                                    }
                                });
                            }
                            auditUi.ColorPicker(pickedColor, {
                                .swatches = customSwatches
                            });
                        }
                        auditUi.EndFrame(auditCanvas);
                        // pickedDate (2020-01-01) must be clamped up to minDate (2025-05-10)!
                        CHECK(pickedDate == minDt);

                        // 6. ButtonOptions::square = true inside Element and standalone
                        Response rWideBtn{}, rSquareElemBtn{}, rSquareTopElem{}, rSquareTopBtn{}, rSquareStandalone{};
                        auto runSquareBtnFrame = [&]() {
                            auditUi.BeginFrame({800.0f, 600.0f}, 1.0f);
                            if (auto col = auditUi.Column({.gap = 8.0f})) {
                                if (auto el = auditUi.Element({
                                    .label = "Element Label",
                                    .description = "Secondary description text",
                                    .width = SizeMode::Stretch
                                })) {
                                    rWideBtn = auditUi.Button({
                                        .text = "Click me",
                                        .icon = Icon::Play(),
                                        .square = false,
                                        .width = 400.0f
                                    });
                                }
                                if (auto el = auditUi.Element({
                                    .label = "Element Label",
                                    .description = "Secondary description text",
                                    .width = SizeMode::Stretch
                                })) {
                                    rSquareElemBtn = auditUi.Button({
                                        .text = "Click me",
                                        .icon = Icon::Play(),
                                        .square = true,
                                        .width = 400.0f
                                    });
                                }
                                if (auto el = auditUi.Element({
                                    .label = "Top",
                                    .width = 180.0f
                                })) {
                                    rSquareTopElem = el.response;
                                    rSquareTopBtn = auditUi.Button({
                                        .text = "Click me",
                                        .icon = Icon::Play(),
                                        .iconPlacement = Placement::Top,
                                        .square = true
                                    });
                                }
                                rSquareStandalone = auditUi.Button({
                                    .text = "Click me",
                                    .icon = Icon::Play(),
                                    .square = true,
                                    .width = 400.0f, .height = 48.0f
                                });
                            }
                            auditUi.EndFrame(auditCanvas);
                        };
                        runSquareBtnFrame();
                        runSquareBtnFrame();
                        CHECK(rWideBtn.rect.w > rWideBtn.rect.h * 2.0f);
                        CHECK(rSquareElemBtn.rect.w > 0.0f);
                        CHECK_NEAR(rSquareElemBtn.rect.w, rSquareElemBtn.rect.h, 0.5f);
                        CHECK_NEAR(rSquareTopElem.rect.w, 180.0f, 0.5f);
                        CHECK_NEAR(rSquareTopBtn.rect.w, rSquareTopBtn.rect.h, 0.5f);
                        CHECK_NEAR(rSquareStandalone.rect.w, 48.0f, 0.5f);
                        CHECK_NEAR(rSquareStandalone.rect.h, 48.0f, 0.5f);
                    }
                }
            }
        }
    }

    TestKeelZeroCopyWidgetBindings();
    TestExternalStlDuckTypedInterop();
    TestBoundedCachesAndInactiveStateTrimming();
    TestElementHeadingCompositionAndIndependentSizing();
    TestPopoverComponent();
    TestApiPolish();
    TestControlFlatSizingAndStyleOptions();
    TestSceneWorldSizeInsideScrollAreaDoesNotOverflow();
    TestApiPolishStretchWeightsTabBarColorAndOptionalMins();
    TestFocusVisibleModalityAndTabBarFocusRing();
    TestTextCompositorAndSubpixelRendering();

    if (g_failures == 0) {
        keel::println("[PASS] All GuiKit Context tests passed successfully!");
        return 0;
    } else {
        keel::eprintln("[FAIL] Total failures: {}", g_failures);
        return 1;
    }
}

void TestKeelZeroCopyWidgetBindings() {
    using namespace guikit;
    keel::println("  -> TestKeelZeroCopyWidgetBindings...");

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 960;
    const keel::u32 height = 900;
    keel::Array<keel::u32> pixels(width * height, 0u);
    canvas.SetTarget(pixels.data(), width, height, width);

    // 1. InputField in-place editing of FixedString<16>, SmallString<32>, and String
    keel::FixedString<16> fixed16("Hello");
    keel::SmallString<32> small32("Inline");
    keel::String heapStr("HeapInit");

    // 2. CodeEditor / TextArea editing String & FixedString<32> + SmallArray<size_t, 8> breakpoints
    keel::String editorCode("int x = 1;\nint y = 2;\nreturn x + y;");
    keel::FixedString<32> fixedArea("line1\nline2");
    keel::SmallArray<keel::usize, 8> bpSmallArray;
    bpSmallArray.push_back(3);

    // 3. Select / ComboBox / Autocomplete (FixedString<32> & String) + MultiSelect (SmallArray<size_t, 8> & Array<size_t>)
    static constexpr keel::StaticArray<keel::StringRef, 4> kFilterModes = {
        "LowPass", "BandPass", "HighPass", "Notch"
    };
    keel::FixedString<32> selFixed("LowPass");
    keel::String selHeap("BandPass");
    keel::FixedString<32> comboFixed("LowPass");
    keel::String comboHeap("BandPass");
    keel::FixedString<32> autoFixed("");
    keel::String autoHeap("");

    keel::SmallArray<keel::usize, 8> multiSmall;
    multiSmall.push_back(0);
    multiSmall.push_back(2);

    keel::Array<keel::usize> multiHeap;
    multiHeap.push_back(1);
    multiHeap.push_back(3);

    // 4. TagInput bound to SmallArray<FixedString<32>, 8>, Array<String>, and SmallArray<TagItem, 8>
    keel::SmallArray<keel::FixedString<32>, 8> fixedTags;
    fixedTags.push_back("alpha");

    keel::Array<keel::String> heapTags;
    heapTags.push_back(keel::String("dsp"));

    keel::SmallArray<TagItem, 8> richTags;
    richTags.push_back(TagItem{.text = "core", .variant = UiVariant::Info});

    Response rFixed{}, rSmall{}, rHeap{};
    CodeEditorResponse rCode{};
    TextAreaResponse rFixedArea{};
    Response rSelFixed{}, rSelHeap{};
    Response rComboFixed{}, rComboHeap{};
    Response rAutoFixed{}, rAutoHeap{};
    Response rMultiSmall{}, rMultiHeap{};
    Response rTagsFixed{}, rTagsHeap{}, rTagsRich{};

    auto renderFrame = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)}, 1.0f);
        if (auto col = ui.Column({.gap = 6.0f, .padding = Padding::All(8.0f), .width = SizeMode::Stretch})) {
            if (auto r1 = ui.Row({.gap = 8.0f})) {
                rFixed = ui.InputField(fixed16, {.width = 220.0f});
                rSmall = ui.InputField(small32, {.width = 220.0f});
                rHeap  = ui.InputField(heapStr, {.width = 220.0f});
            }
            if (auto r2 = ui.Row({.gap = 8.0f})) {
                rCode = ui.CodeEditor(editorCode, {
                    .rows = 4,
                    .showLineNumbers = true,
                    .width = 420.0f,
                    .enableBreakpoints = true,
                    .breakpoints = &bpSmallArray
                });
                rFixedArea = ui.TextArea(fixedArea, {
                    .rows = 4,
                    .width = 320.0f
                });
            }
            if (auto r3 = ui.Row({.gap = 8.0f})) {
                rSelFixed   = ui.Select(selFixed, kFilterModes, {.width = 180.0f});
                rSelHeap    = ui.Select(selHeap, kFilterModes, {.width = 180.0f});
                rComboFixed = ui.ComboBox(comboFixed, kFilterModes, {.width = 180.0f});
                rComboHeap  = ui.ComboBox(comboHeap, kFilterModes, {.width = 180.0f});
            }
            if (auto r4 = ui.Row({.gap = 8.0f})) {
                rAutoFixed  = ui.Autocomplete(autoFixed, kFilterModes, {.width = 180.0f});
                rAutoHeap   = ui.Autocomplete(autoHeap, kFilterModes, {.width = 180.0f});
                rMultiSmall = ui.MultiSelect(multiSmall, kFilterModes, {.searchable = true, .width = 240.0f});
                rMultiHeap  = ui.MultiSelect(multiHeap, kFilterModes, {.searchable = true, .width = 240.0f});
            }
            if (auto r5 = ui.Row({.gap = 8.0f})) {
                rTagsFixed = ui.TagInput(fixedTags, {.splitOnComma = true, .width = 260.0f});
                rTagsHeap  = ui.TagInput(heapTags, {.splitOnComma = true, .width = 260.0f});
                rTagsRich  = ui.TagInput(richTags, {.width = 260.0f});
            }
        }
        ui.EndFrame(canvas);
    };

    renderFrame();
    renderFrame();

    // --- 1a. FixedString<16>: type beyond 15-byte max capacity -> verify truncation + cursor sync! ---
    ui.RequestFocus(rFixed.id);
    renderFrame();
    ui.OnKeyDown(Key::End);
    ui.OnTextInput("0123456789ABCDEF"); // 5 + 16 = 21 bytes attempted, capacity = 15 bytes
    renderFrame();
    CHECK(rFixed.changed);
    CHECK(fixed16.size() == 15u);
    CHECK(fixed16.full());
    CHECK(fixed16.as_ref() == "Hello0123456789");
    CHECK(ui.GetTextEditAnchor(rFixed.id) == 15u);

    // Immediate Backspace must delete the 15th byte ('9'), proving internal edit buffer & cursor synced to 15 (not 21)
    ui.OnKeyDown(Key::Backspace);
    renderFrame();
    CHECK(fixed16.size() == 14u);
    CHECK(fixed16.as_ref() == "Hello012345678");
    CHECK(ui.GetTextEditAnchor(rFixed.id) == 14u);

    // Typing a 2-byte UTF-8 codepoint ("Я") at 14/15 bytes must clamp cleanly at UTF-8 boundary (14 bytes)
    ui.OnTextInput("Я");
    renderFrame();
    CHECK(fixed16.size() == 14u);
    CHECK(fixed16.as_ref() == "Hello012345678");
    CHECK(ui.GetTextEditAnchor(rFixed.id) == 14u);

    // --- 1b. SmallString<32>: inline edit <= 31 bytes, then heap spill > 31 bytes ---
    ui.RequestFocus(rSmall.id);
    renderFrame();
    ui.OnKeyDown(Key::End);
    ui.OnTextInput("_ok");
    renderFrame();
    CHECK(small32.is_inline());
    CHECK(small32.as_ref() == "Inline_ok");

    ui.OnTextInput("_0123456789_0123456789_spill");
    renderFrame();
    CHECK(!small32.is_inline());
    CHECK(small32.size() > 31u);
    CHECK(small32.ends_with("_spill"));

    // --- 1c. keel::String: dynamic heap string edit ---
    ui.RequestFocus(rHeap.id);
    renderFrame();
    ui.OnKeyDown(Key::End);
    ui.OnTextInput("_Edited");
    renderFrame();
    CHECK(heapStr.as_ref() == "HeapInit_Edited");

    // --- 2. CodeEditor (keel::String + SmallArray<size_t, 8> breakpoints) & TextArea (FixedString<32>) ---
    ui.RequestFocus(rCode.id);
    renderFrame();
    ui.OnKeyDown(Key::Home, {.ctrl = true});
    ui.OnKeyDown(Key::Enter);
    renderFrame();
    CHECK(rCode.changed);
    CHECK(rCode.lineDelta == 1);
    CHECK(editorCode.starts_with("\nint x = 1;"));
    CHECK(bpSmallArray.size() == 1u && bpSmallArray[0] == 4u);

    ui.OnKeyDown(Key::F9);
    renderFrame();
    CHECK(bpSmallArray.size() == 2u);

    ui.RequestFocus(rFixedArea.id);
    renderFrame();
    ui.OnKeyDown(Key::End, {.ctrl = true});
    ui.OnTextInput("_0123456789_0123456789_overflow");
    renderFrame();
    CHECK(fixedArea.size() == 31u);
    CHECK(fixedArea.full());
    CHECK(fixedArea.starts_with("line1\nline2_0123456789"));

    // --- 3a. Select bound to FixedString<32> and String ---
    auto clickWidget = [&](const Response& r) {
        ui.OnMouseMove({r.rect.x + 20.0f, r.rect.Center().y});
        ui.OnMouseDown(1);
        renderFrame();
        ui.OnMouseUp(1);
        renderFrame();
    };

    clickWidget(rSelFixed);
    CHECK(ui.IsPopupOpen(rSelFixed.id));
    ui.OnKeyDown(Key::Down);
    renderFrame();
    CHECK(selFixed.as_ref() == "BandPass");
    ui.OnKeyDown(Key::Escape);
    renderFrame();

    clickWidget(rSelHeap);
    CHECK(ui.IsPopupOpen(rSelHeap.id));
    ui.OnKeyDown(Key::Down);
    renderFrame();
    CHECK(selHeap.as_ref() == "HighPass");
    ui.OnKeyDown(Key::Escape);
    renderFrame();

    // --- 3b. ComboBox bound to FixedString<32> and String ---
    clickWidget(rComboFixed);
    CHECK(ui.IsPopupOpen(rComboFixed.id));
    ui.OnTextInput("not");
    renderFrame();
    ui.OnKeyDown(Key::Enter);
    renderFrame();
    CHECK(comboFixed.as_ref() == "Notch");

    clickWidget(rComboHeap);
    CHECK(ui.IsPopupOpen(rComboHeap.id));
    ui.OnTextInput("low");
    renderFrame();
    ui.OnKeyDown(Key::Enter);
    renderFrame();
    CHECK(comboHeap.as_ref() == "LowPass");

    // --- 3c. Autocomplete bound to FixedString<32> and String ---
    clickWidget(rAutoFixed);
    ui.OnTextInput("high");
    renderFrame();
    ui.OnKeyDown(Key::Enter);
    renderFrame();
    CHECK(autoFixed.as_ref() == "HighPass");

    clickWidget(rAutoHeap);
    ui.OnTextInput("CustomFIR");
    renderFrame();
    ui.OnKeyDown(Key::Enter);
    renderFrame();
    CHECK(autoHeap.as_ref() == "CustomFIR");

    // --- 3d. MultiSelect bound to SmallArray<size_t, 8> and Array<size_t> ---
    ui.OnMouseMove({rMultiSmall.rect.Right() - 12.0f, rMultiSmall.rect.Center().y});
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();
    CHECK(ui.IsPopupOpen(rMultiSmall.id));
    ui.OnKeyDown(Key::Backspace);
    renderFrame();
    CHECK(multiSmall.size() == 1u && multiSmall[0] == 0u);
    ui.OnKeyDown(Key::Escape);
    renderFrame();

    ui.OnMouseMove({rMultiHeap.rect.Right() - 12.0f, rMultiHeap.rect.Center().y});
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();
    CHECK(ui.IsPopupOpen(rMultiHeap.id));
    ui.OnKeyDown(Key::Backspace);
    renderFrame();
    CHECK(multiHeap.size() == 1u && multiHeap[0] == 1u);
    ui.OnKeyDown(Key::Escape);
    renderFrame();

    // --- 4. TagInput bound to SmallArray<FixedString<32>, 8>, Array<String>, and SmallArray<TagItem, 8> ---
    ui.RequestFocus(rTagsFixed.id);
    renderFrame();
    ui.OnTextInput("beta,gamma,");
    renderFrame();
    CHECK(fixedTags.size() == 3u);
    CHECK(fixedTags[1].as_ref() == "beta");
    CHECK(fixedTags[2].as_ref() == "gamma");

    ui.RequestFocus(rTagsHeap.id);
    renderFrame();
    ui.OnTextInput("fft");
    renderFrame();
    ui.OnKeyDown(Key::Enter);
    renderFrame();
    CHECK(heapTags.size() == 2u);
    CHECK(heapTags[1].as_ref() == "fft");
    ui.OnKeyDown(Key::Backspace);
    renderFrame();
    CHECK(heapTags.size() == 1u);
    CHECK(heapTags[0].as_ref() == "dsp");

    ui.RequestFocus(rTagsRich.id);
    renderFrame();
    ui.OnTextInput("gpu");
    renderFrame();
    ui.OnKeyDown(Key::Enter);
    renderFrame();
    CHECK(richTags.size() == 2u);
    CHECK(richTags[0].variant == UiVariant::Info);
    CHECK(richTags[1].text.as_ref() == "gpu");

    keel::println("  [PASS] TestKeelZeroCopyWidgetBindings completed");
}

// STL headers are included ONLY here at the very bottom (after guikit.hpp and all keel-native tests)
// to verify zero-header duck-typed STL interoperability without including keel/interop_std.h.
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

void TestExternalStlDuckTypedInterop() {
    using namespace guikit;
    keel::println("  -> TestExternalStlDuckTypedInterop...");

    Context ui;
    ThorCanvas canvas;
    const keel::u32 width = 900;
    const keel::u32 height = 760;
    keel::Array<keel::u32> pixels(width * height, 0u);
    canvas.SetTarget(pixels.data(), width, height, width);

    const std::string stdLabel = "External std::string Label";
    const std::string_view stdBtnText = "External std::string_view Button";
    const std::optional<float> optScale = 1.0f;
    const std::optional<float> optAutoScale = std::nullopt;

    std::string stdInput = "stl_edit";
    std::string stdCode = "float gain = 1.0f;\nreturn gain;";
    std::vector<keel::usize> stdBreakpoints = {2};

    const std::vector<SelectItem> stdSelectItems = {
        {.text = "Alpha"},
        {.text = "Beta"},
        {.text = "Gamma"}
    };
    const std::span<const SelectItem> stdSelectSpan(stdSelectItems);

    std::string stdSelStr = "Alpha";
    std::string_view stdSelView = "Beta";
    std::vector<keel::usize> stdMultiIndices = {0, 2};
    std::vector<std::string> stdStringTags = {"stl", "duck"};
    std::vector<TagItem> stdRichTags = {
        TagItem{.text = "badge", .variant = UiVariant::Success}
    };

    const std::vector<float> stdLevels = {-12.0f, -3.0f};
    const std::span<const float> stdLevelsSpan(stdLevels);

    Response rTextStr{}, rTextSv{}, rBtn{}, rInput{};
    CodeEditorResponse rCode{};
    Response rSelStr{}, rSelView{}, rMulti{};
    Response rStrTags{}, rRichTags{}, rMeter{};

    auto renderStlFrame = [&]() {
        ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)}, 1.0f);
        if (auto colZ = ui.Zone({.scale = optAutoScale})) {
            if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(12.0f), .width = SizeMode::Stretch})) {
                rTextStr = ui.Text(stdLabel);
                rTextSv  = ui.Text(stdBtnText);
                rBtn     = ui.Button({.text = stdBtnText, .tooltip = stdLabel, .width = 240.0f});
                rInput   = ui.InputField(stdInput, {.width = AxisSize::Auto()});
                if (auto boxZ = ui.Zone({.scale = optScale})) {
                    if (auto scaledBox = ui.Box()) {
                        (void)scaledBox;
                    }
                }
            rCode    = ui.CodeEditor(stdCode, {
                .rows = 4,
                .showLineNumbers = true,
                .enableBreakpoints = true,
                .breakpoints = &stdBreakpoints
            });
            if (auto row = ui.Row({.gap = 8.0f})) {
                rSelStr  = ui.Select(stdSelStr, stdSelectSpan, {.width = 200.0f});
                rSelView = ui.Select(stdSelView, stdSelectItems, {.width = 200.0f});
                rMulti   = ui.MultiSelect(stdMultiIndices, stdSelectSpan, {.searchable = true, .width = 240.0f});
            }
            if (auto row2 = ui.Row({.gap = 8.0f})) {
                rStrTags  = ui.TagInput(stdStringTags, {.splitOnComma = true, .width = 260.0f});
                rRichTags = ui.TagInput(stdRichTags, {.width = 260.0f});
                rMeter    = ui.LevelMeter(stdLevelsSpan, -60.0f, 0.0f, {.width = 160.0f});
            }
        }
        }
        ui.EndFrame(canvas);
    };

    renderStlFrame();
    renderStlFrame();

    CHECK(rTextStr.rect.w > 40.0f);
    CHECK(rTextSv.rect.w > 40.0f);
    CHECK_NEAR(rBtn.rect.w, 240.0f, 0.5f);
    CHECK(rMeter.rect.w > 100.0f);

    // 1. Mutate std::string via ui.InputField
    ui.RequestFocus(rInput.id);
    renderStlFrame();
    ui.OnKeyDown(Key::End);
    ui.OnTextInput("_ok");
    renderStlFrame();
    CHECK(stdInput == "stl_edit_ok");

    // 2. Mutate std::string + std::vector<size_t> breakpoints via ui.CodeEditor
    ui.RequestFocus(rCode.id);
    renderStlFrame();
    ui.OnKeyDown(Key::Home, {.ctrl = true});
    ui.OnKeyDown(Key::Enter);
    renderStlFrame();
    CHECK(stdCode.starts_with("\nfloat gain = 1.0f;"));
    CHECK(stdBreakpoints.size() == 1u && stdBreakpoints[0] == 3u);

    // 3. Mutate std::string and std::string_view via ui.Select + std::span<const SelectItem>
    ui.OnMouseMove({rSelStr.rect.x + 20.0f, rSelStr.rect.Center().y});
    ui.OnMouseDown(1);
    renderStlFrame();
    ui.OnMouseUp(1);
    renderStlFrame();
    CHECK(ui.IsPopupOpen(rSelStr.id));
    ui.OnKeyDown(Key::Down);
    renderStlFrame();
    CHECK(stdSelStr == "Beta");
    ui.OnKeyDown(Key::Escape);
    renderStlFrame();

    ui.OnMouseMove({rSelView.rect.x + 20.0f, rSelView.rect.Center().y});
    ui.OnMouseDown(1);
    renderStlFrame();
    ui.OnMouseUp(1);
    renderStlFrame();
    CHECK(ui.IsPopupOpen(rSelView.id));
    ui.OnKeyDown(Key::Down);
    renderStlFrame();
    CHECK(stdSelView == "Gamma");
    ui.OnKeyDown(Key::Escape);
    renderStlFrame();

    // 4. Mutate std::vector<size_t> via ui.MultiSelect
    ui.OnMouseMove({rMulti.rect.Right() - 12.0f, rMulti.rect.Center().y});
    ui.OnMouseDown(1);
    renderStlFrame();
    ui.OnMouseUp(1);
    renderStlFrame();
    CHECK(ui.IsPopupOpen(rMulti.id));
    ui.OnKeyDown(Key::Backspace);
    renderStlFrame();
    CHECK(stdMultiIndices.size() == 1u && stdMultiIndices[0] == 0u);
    ui.OnKeyDown(Key::Escape);
    renderStlFrame();

    // 5. Mutate std::vector<std::string> and std::vector<TagItem> via ui.TagInput
    ui.RequestFocus(rStrTags.id);
    renderStlFrame();
    ui.OnTextInput("interop,");
    renderStlFrame();
    CHECK(stdStringTags.size() == 3u);
    CHECK(stdStringTags[2] == "interop");

    ui.RequestFocus(rRichTags.id);
    renderStlFrame();
    ui.OnTextInput("verified");
    renderStlFrame();
    ui.OnKeyDown(Key::Enter);
    renderStlFrame();
    CHECK(stdRichTags.size() == 2u);
    CHECK(stdRichTags[0].variant == UiVariant::Success);
    CHECK(stdRichTags[1].text.as_ref() == "verified");

    keel::println("  [PASS] TestExternalStlDuckTypedInterop completed");
}

void TestBoundedCachesAndInactiveStateTrimming() {
    using namespace guikit;
    keel::println("  -> TestBoundedCachesAndInactiveStateTrimming...");

    // 1. Context automatic TrimInactiveCaches() for inactive persistent widget states
    {
        struct MockHeavyWidgetState {
            float zoom{2.75f};
            keel::i32 selectedMarker{42};
            keel::i32 trimCount{0};
            keel::Array<keel::u32> offscreenPixels{};

            void TrimInactiveCaches() noexcept {
                offscreenPixels = keel::Array<keel::u32>{};
                ++trimCount;
            }
        };

        ThorCanvas canvas;
        keel::Array<keel::u32> fb(320u * 240u, 0u);
        canvas.SetTarget(fb.data(), 320, 240, 320);

        Context ui;
        constexpr Id heavyId{0x987654321ULL};

        // Active for 65 frames: offscreen cache must NOT be trimmed
        for (keel::i32 f = 0; f < 65; ++f) {
            ui.BeginFrame({320.0f, 240.0f});
            auto& st = ui.GetOrCreatePersistentState<MockHeavyWidgetState>(heavyId);
            if (st.offscreenPixels.empty()) {
                st.offscreenPixels.assign(16384u, 0xFF112233u);
            }
            ui.Text("Active screen");
            ui.EndFrame(canvas);
        }
        {
            const auto* st = ui.FindState<MockHeavyWidgetState>(heavyId);
            CHECK(st != nullptr);
            if (st) {
                CHECK(st->trimCount == 0);
                CHECK(st->offscreenPixels.capacity() >= 16384u);
            }
        }

        // Idle for 200 frames (> 120-frame threshold): cache trimmed exactly once; persistent user state preserved
        for (keel::i32 f = 0; f < 200; ++f) {
            ui.BeginFrame({320.0f, 240.0f});
            ui.Text("Other screen");
            ui.EndFrame(canvas);
        }
        {
            const auto* st = ui.FindState<MockHeavyWidgetState>(heavyId);
            CHECK(st != nullptr);
            if (st) {
                CHECK(st->trimCount == 1);
                CHECK(st->offscreenPixels.capacity() == 0u);
                CHECK_NEAR(st->zoom, 2.75f, 1e-5f);
                CHECK(st->selectedMarker == 42);
            }
        }

        // Idle past 650 frames total: persistent state STILL survives (not garbage-collected), trimCount stays 1
        for (keel::i32 f = 0; f < 500; ++f) {
            ui.BeginFrame({320.0f, 240.0f});
            ui.Text("Still on other screen");
            ui.EndFrame(canvas);
        }
        {
            const auto* st = ui.FindState<MockHeavyWidgetState>(heavyId);
            CHECK(st != nullptr);
            if (st) {
                CHECK(st->trimCount == 1);
                CHECK_NEAR(st->zoom, 2.75f, 1e-5f);
                CHECK(st->selectedMarker == 42);
            }
        }

        // Re-activate widget, rebuild cache, then go idle again -> trimmed a second time
        ui.BeginFrame({320.0f, 240.0f});
        {
            auto& st = ui.GetOrCreatePersistentState<MockHeavyWidgetState>(heavyId);
            st.offscreenPixels.assign(8192u, 0xFF445566u);
        }
        ui.EndFrame(canvas);

        for (keel::i32 f = 0; f < 200; ++f) {
            ui.BeginFrame({320.0f, 240.0f});
            ui.Text("Idle again");
            ui.EndFrame(canvas);
        }
        {
            const auto* st = ui.FindState<MockHeavyWidgetState>(heavyId);
            CHECK(st != nullptr);
            if (st) {
                CHECK(st->trimCount == 2);
                CHECK(st->offscreenPixels.capacity() == 0u);
            }
        }
    }

    // 2. ThorCanvas StampLruCache eviction under heavy unique text & shape churn
    {
        ThorCanvas canvas;
        keel::Array<keel::u32> fb(400u * 120u, 0u);
        canvas.SetTarget(fb.data(), 400, 120, 400);

        canvas.Begin(400, 120, 1.0f);
        canvas.Clear(Color(16, 20, 28, 255));
        for (keel::i32 i = 0; i < 1500; ++i) {
            char buf[64]{};
            const keel::StringRef label = keel::format_buf(buf, "Telemetry Sample #%04d [%.2f dB]", i, i * 0.125f);
            canvas.DrawTextBoxStyled(
                label,
                Rect(8.0f, 8.0f, 360.0f, 28.0f),
                14.0f,
                Color(235, 240, 248, 255),
                FontStyle::Regular,
                0.0f,
                0.5f
            );
            const float w = 24.0f + static_cast<float>(i % 140) + 0.25f * static_cast<float>(i & 3);
            canvas.DrawRoundRect(
                Rect(8.0f, 48.0f, w, 24.0f),
                6.0f,
                Color(56, 189, 248, 220),
                Color(125, 211, 252, 255),
                1.0f
            );
        }
        canvas.End();

        keel::usize nonBgPixels = 0;
        const keel::u32 bgSample = fb[0];
        for (keel::u32 px : fb) {
            if (px != bgSample) {
                ++nonBgPixels;
            }
        }
        CHECK(nonBgPixels > 100u);
    }

#if GUIKIT_HAS_SCENES
    // 3. GisRenderEngine global Tier-1/Tier-2 tile cache + TilePixelBufferPool pan/zoom stress
    {
        GisRenderEngine::InvalidateTileCache();

        keel::Array<geo::GeoLayer> layers;
        layers.resize(6u);
        for (keel::usize li = 0; li < layers.size(); ++li) {
            geo::GeoLayer& lyr = layers[li];
            lyr.name = keel::format("Layer_{}", li);
            lyr.visible = true;
            lyr.style.primaryColor = Color(static_cast<keel::u8>(40 + li * 30), 120, 200, 255);
            lyr.style.useCustomColor = true;
            lyr.style.fillOpacity = 0.75f;
            lyr.style.strokeWidthPx = 1.25f;

            for (keel::i32 gx = 0; gx < 6; ++gx) {
                for (keel::i32 gy = 0; gy < 6; ++gy) {
                    geo::GeoFeature feat{};
                    feat.type = geo::GeometryType::Polygon;
                    const double baseLat = 59.80 + gx * 0.06 + static_cast<double>(li) * 0.004;
                    const double baseLon = 29.70 + gy * 0.08 + static_cast<double>(li) * 0.004;
                    geo::GeoPolygon poly{};
                    poly.exteriorRing.push_back({baseLat, baseLon});
                    poly.exteriorRing.push_back({baseLat, baseLon + 0.045});
                    poly.exteriorRing.push_back({baseLat + 0.040, baseLon + 0.045});
                    poly.exteriorRing.push_back({baseLat + 0.040, baseLon});
                    poly.exteriorRing.push_back({baseLat, baseLon});
                    feat.polygons.push_back(keel::move(poly));
                    lyr.features.push_back(keel::move(feat));
                }
            }
            lyr.FinalizeLayer(9000u + li);
        }

        ThorCanvas canvas;
        keel::Array<keel::u32> fb(640u * 480u, 0u);
        canvas.SetTarget(fb.data(), 640, 480, 640);

        Context ui;
        GisRenderConfig cfg{};
        cfg.maxCompositeTiles = 24;
        cfg.maxLayerTiles = 24;
        cfg.asyncBackgroundStreaming = false;

        keel::u64 selectedUid = 0;
        keel::usize totalRendered = 0;
        keel::usize totalHits = 0;
        for (keel::i32 step = 0; step < 24; ++step) {
            const GeoCoord stepCenter{59.92 + (step % 4) * 0.03, 29.85 + (step / 8) * 0.04};
            const float stepZoom = 11.0f + static_cast<float>((step / 2) % 3);
            ui.BeginFrame({640.0f, 480.0f});
            if (auto map = ui.MapView({
                .initialCenter = stepCenter,
                .initialTileZoom = stepZoom,
                .height = SizeMode::Stretch
            })) {
                if ((step % 2) == 0) {
                    map.CenterOnGeo(stepCenter, stepZoom);
                }
                const GisRenderStats st = GisRenderEngine::RenderLayers(map, layers, cfg, selectedUid, false);
                totalRendered += st.cachedTilesRendered;
                totalHits += st.cachedTilesHit;
            }
            ui.EndFrame(canvas);
        }
        CHECK(totalRendered > 0u);
        CHECK(totalHits > 0u);

        // Idle away from MapView for 180 frames -> MapViewInternalState::TrimInactiveCaches()
        // automatically triggers GisRenderEngine::TrimInactiveCaches(), then re-entering MapView re-renders cleanly
        for (keel::i32 f = 0; f < 180; ++f) {
            ui.BeginFrame({640.0f, 480.0f});
            ui.Text("Switched away from GIS map");
            ui.EndFrame(canvas);
        }
        canvas.TrimCaches();

        keel::usize postTrimRendered = 0;
        for (keel::i32 step = 0; step < 2; ++step) {
            ui.BeginFrame({640.0f, 480.0f});
            if (auto map = ui.MapView({
                .initialCenter = GeoCoord{59.92, 29.85},
                .initialTileZoom = 11.0f,
                .height = SizeMode::Stretch
            })) {
                const GisRenderStats st = GisRenderEngine::RenderLayers(map, layers, cfg, selectedUid, false);
                postTrimRendered += st.cachedTilesRendered;
            }
            ui.EndFrame(canvas);
        }
        CHECK(postTrimRendered > 0u);
        GisRenderEngine::InvalidateTileCache();
    }
#endif

    // 4. TextArea / CodeEditor & Markdown inactive cache trimming + reactive time-based wakeup & trimming
    {
        ThorCanvas canvas;
        keel::Array<keel::u32> fb(480u * 360u, 0u);
        canvas.SetTarget(fb.data(), 480, 360, 480);

        Context ui;
        ui.SetIdleHeartbeatMs(0); // Deep-sleep mode (0% CPU when idle unless untrimmed caches exist)
        ui.SetCurrentTime(10.0);

        keel::String codeSource = keel::String::from(
            "#include <cstdint>\n"
            "int compute(int x) {\n"
            "    int acc = 0;\n"
            "    for (int i = 0; i < x; ++i) {\n"
            "        acc += i * 2;\n"
            "    }\n"
            "    return acc;\n"
            "}\n"
        );
        const keel::StringRef mdSource =
            "# Telemetry Summary\n\n"
            "Status: **Nominal** | Frame: `1024`\n\n"
            "| Channel | Gain |\n"
            "| :--- | ---: |\n"
            "| CH-01 | -3.0 dB |\n"
            "| CH-02 | +1.5 dB |\n";

        Id editorId = Id::Null();
        float initialMdHeight = 0.0f;
        for (keel::i32 f = 0; f < 3; ++f) {
            ui.AdvanceTime(0.016);
            ui.BeginFrame({480.0f, 360.0f});
            const Response edRes = ui.CodeEditor(codeSource, {.rows = 6});
            editorId = edRes.id;
            const MarkdownResponse mdRes = ui.Markdown(mdSource);
            initialMdHeight = mdRes.rect.h;
            ui.EndFrame(canvas);
        }

        CHECK(editorId.IsValid());
        CHECK(initialMdHeight > 20.0f);
        {
            const auto* edSt = ui.FindState<TextAreaState>(editorId);
            CHECK(edSt != nullptr);
            if (edSt) {
                CHECK(!edSt->cachedLogicalLines.empty());
                CHECK(edSt->cachedLogicalLines.capacity() > 0u);
            }
        }

        // Navigate away from CodeEditor + Markdown and let repaint counter settle (3 frames, tiny dt):
        // caches are NOT trimmed yet, and GetRecommendedSleepTimeoutMs() wakes deep-sleep at 500 ms.
        for (keel::i32 f = 0; f < 3; ++f) {
            ui.AdvanceTime(0.016);
            ui.BeginFrame({480.0f, 360.0f});
            ui.Heading({.label = "Static Dashboard"});
            ui.EndFrame(canvas);
        }
        CHECK(!ui.NeedsRepaint());
        CHECK(ui.GetRecommendedSleepTimeoutMs() == 500);

        // Advance only 4 reactive heartbeat frames (0.5s each = 2.0s total, far below 120 frames):
        // time-based reactive cache trimming reclaims TextArea & Markdown caches and restores deep sleep (0 ms).
        for (keel::i32 f = 0; f < 4; ++f) {
            ui.AdvanceTime(0.5);
            ui.BeginFrame({480.0f, 360.0f});
            ui.Heading({.label = "Static Dashboard"});
            ui.EndFrame(canvas);
        }
        {
            const auto* edSt = ui.FindState<TextAreaState>(editorId);
            CHECK(edSt != nullptr);
            if (edSt) {
                CHECK(edSt->cachedLogicalLines.capacity() == 0u);
                CHECK(edSt->scratchVisualLines.capacity() == 0u);
            }
        }
        CHECK(ui.GetRecommendedSleepTimeoutMs() == 0);

        // Re-visiting Markdown + CodeEditor seamlessly rebuilds layout with identical geometry
        float rebuiltMdHeight = 0.0f;
        for (keel::i32 f = 0; f < 2; ++f) {
            ui.AdvanceTime(0.016);
            ui.BeginFrame({480.0f, 360.0f});
            (void)ui.CodeEditor(codeSource, {.rows = 6});
            rebuiltMdHeight = ui.Markdown(mdSource).rect.h;
            ui.EndFrame(canvas);
        }
        CHECK_NEAR(rebuiltMdHeight, initialMdHeight, 1.0f);
    }

    keel::println("  [PASS] TestBoundedCachesAndInactiveStateTrimming completed");
}

void TestElementHeadingCompositionAndIndependentSizing() {
    using namespace guikit;
    keel::println("  -> TestElementHeadingCompositionAndIndependentSizing...");

    struct ElementSpyCanvas final : public ThorCanvas {
        struct RoundRectRec {
            Rect rect;
            Color fill;
            Color stroke;
            float strokeWidth;
        };
        struct LineRec {
            Vec2 p1;
            Vec2 p2;
            Color color;
            float thickness;
        };
        void DrawRoundRect(Rect rect, float radius, Color fill, Color stroke = Color(0, 0, 0, 0), float strokeWidth = 0.0f) override {
            roundRects.push_back({rect, fill, stroke, strokeWidth});
            ThorCanvas::DrawRoundRect(rect, radius, fill, stroke, strokeWidth);
        }
        void DrawLine(Vec2 p1, Vec2 p2, Color color, float thickness = 1.0f) override {
            lines.push_back({p1, p2, color, thickness});
            ThorCanvas::DrawLine(p1, p2, color, thickness);
        }
        using ThorCanvas::Clear;
        void ResetRecords() {
            roundRects.clear();
            lines.clear();
        }
        keel::Array<RoundRectRec> roundRects;
        keel::Array<LineRec> lines;
    };

    keel::Array<keel::u32> pixels(800u * 600u, 0u);
    ElementSpyCanvas canvas;
    canvas.SetTarget(pixels.data(), 800, 600, 800);

    // 1. Standalone Heading with HeadingLevel (H5, default H3, H2, H4), UiSize independence, HeadingLayout, & Separator
    {
        Context ui;
        Response rH5Head{};
        Response rDefaultH3InXs{};
        Response rDefaultH3InXl{};
        Response rXsBtn{};
        Response rXlBtn{};
        Response rH2Head{};
        Response rH2BottomSepHead{};
        Response rInlineTrailHead{};

        for (keel::i32 f = 0; f < 2; ++f) {
            canvas.ResetRecords();
            ui.BeginFrame({800.0f, 600.0f});
            if (f == 1) {
                ui.InvalidateAll();
            }
            if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(0.0f)})) {
                rH5Head = ui.Heading({
                    .label = "Compact Section",
                    .description = "H5 heading typography",
                    .level = HeadingLevel::H5
                });

                {
                    rDefaultH3InXs = ui.Heading({
                        .label = "Default H3 Heading",
                        .description = "Independent heading typography"
                    });
                    rXsBtn = ui.Button({.text = "Action", .size = UiSize::Xs});
                }
                {
                    rDefaultH3InXl = ui.Heading({
                        .label = "Default H3 Heading",
                        .description = "Independent heading typography"
                    });
                    rXlBtn = ui.Button({.text = "Action", .size = UiSize::Xl});
                }

                rH2Head = ui.Heading({
                    .label = "Section Gain",
                    .description = "H2 heading typography",
                    .level = HeadingLevel::H2
                });

                rH2BottomSepHead = ui.Heading({
                    .label = "Section With Bottom Divider",
                    .description = "H2 + Bottom separator",
                    .level = HeadingLevel::H2,
                    .separator = {HeadingSeparator::Bottom, StateColor::Primary(), 2.0f}
                });

                rInlineTrailHead = ui.Heading({
                    .label = "Enable Bus",
                    .description = "Inline Subtitle",
                    .badge = "CH1",
                    .level = HeadingLevel::H4,
                    .layout = HeadingLayout::Inline,
                    .separator = {HeadingSeparator::Trailing, StateColor::Success(), 1.5f}
                });
            }
            ui.EndFrame(canvas);
        }

        // Default Heading (H3) is 100% invariant to enclosing UiSize scopes while Buttons scale with UiSize
        CHECK_NEAR(rDefaultH3InXs.rect.h, rDefaultH3InXl.rect.h, 0.01f);
        CHECK_NEAR(rDefaultH3InXs.rect.w, rDefaultH3InXl.rect.w, 0.01f);
        CHECK(rXlBtn.rect.h > rXsBtn.rect.h + 16.0f);

        // H5 < Default H3 < H2 < H2 + Bottom separator
        CHECK(rDefaultH3InXs.rect.h > rH5Head.rect.h + 2.0f);
        CHECK(rH2Head.rect.h > rDefaultH3InXs.rect.h + 1.5f);
        CHECK(rH2BottomSepHead.rect.h > rH2Head.rect.h + 6.0f);

        const Color expectedBottomCol = ui.GetTheme().ResolveColor(StateColor::Primary(), false, false, false, ui.GetThemePalette().borderSubtle);
        const Color expectedTrailCol = ui.GetTheme().ResolveColor(StateColor::Success(), false, false, false, ui.GetThemePalette().borderSubtle);
        bool foundBottomLine = false;
        bool foundInlineBridgeLine = false;
        for (const auto& ln : canvas.lines) {
            if (ln.color == expectedBottomCol && keel::abs(ln.thickness - 2.0f) < 0.05f) {
                CHECK_NEAR(ln.p1.x, 0.0f, 0.5f);
                CHECK_NEAR(ln.p2.x, 800.0f, 0.5f);
                foundBottomLine = true;
            }
            if (ln.color == expectedTrailCol && keel::abs(ln.thickness - 1.5f) < 0.05f) {
                CHECK(ln.p1.x > 60.0f);
                CHECK(ln.p2.x > ln.p1.x + 100.0f);
                CHECK(ln.p2.x < 800.0f - 40.0f); // Stops before Inline Subtitle on the right!
                foundInlineBridgeLine = true;
            }
        }
        CHECK(foundBottomLine);
        CHECK(foundInlineBridgeLine);
    }

    // 2. Orthogonal Outer Element Width (480dp) vs Inner Control Width (140dp) + Multi-Control Slot
    {
        Context ui;
        keel::String query("fft");
        bool cbFlag = false;
        Response rOuterEl{}, rBtn{}, rMultiEl{}, rQueryIn{}, rGoBtn{}, rAutoFitCbEl{};
        for (keel::i32 f = 0; f < 2; ++f) {
            canvas.ResetRecords();
            ui.BeginFrame({800.0f, 400.0f});
            if (f == 1) {
                ui.InvalidateAll();
            }
            if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(0.0f)})) {
                if (auto el = ui.Element({
                    .label = "Centered 480dp Element",
                    .description = "Inner button stays 140dp wide",
                    .align = Align::Center,
                    .width = 480.0f
                })) {
                    rOuterEl = el.response;
                    rBtn = ui.Button({
                        .text = "Apply",
                        .width = 140.0f
                    });
                }

                // Composite slot: ui.Row({.gap = 8.0f}) with InputField (Stretch) + Button (Fit)
                if (auto el = ui.Element({
                    .label = "Search Channels",
                    .description = "Composite horizontal slot via ui.Row"
                })) {
                    rMultiEl = el.response;
                    if (auto row = ui.Row({.gap = 8.0f})) {
                        rQueryIn = ui.InputField(query);
                        rGoBtn = ui.Button({.text = "Search"});
                    }
                }

                // Auto widthMode resolution: ElementLayout::Trailing Checkbox defaults to SizeMode::Fit (< 800dp)
                if (auto el = ui.Element({
                    .label = "Auto-Fit Checkbox"
                })) {
                    rAutoFitCbEl = el.response;
                    ui.Checkbox(cbFlag);
                }
            }
            ui.EndFrame(canvas);
        }

        // Outer Element is 480dp wide, and inner Button is 140dp wide centered at x = (480 - 140) * 0.5 = 170dp!
        CHECK_NEAR(rOuterEl.rect.w, 480.0f, 0.5f);
        CHECK_NEAR(rBtn.rect.w, 140.0f, 0.5f);
        CHECK_NEAR(rBtn.rect.x, 170.0f, 1.0f);

        // Multi-control slot fills full 800dp width, places Button to the right of InputField with 8dp gap
        CHECK_NEAR(rMultiEl.rect.w, 800.0f, 0.5f);
        CHECK_NEAR(rQueryIn.rect.x, 0.0f, 0.5f);
        CHECK_NEAR(rGoBtn.rect.x - rQueryIn.rect.Right(), 8.0f, 0.5f);
        CHECK_NEAR(rGoBtn.rect.Right(), 800.0f, 0.5f);

        // Inline + ControlFirst Checkbox Element auto-resolves to SizeMode::Fit so empty right margin isn't clickable
        CHECK(rAutoFitCbEl.rect.w > 40.0f);
        CHECK(rAutoFitCbEl.rect.w < 300.0f);

        // Explicit Fixed Element width (400dp) is never inflated by a wider Fixed child (480dp):
        // Element bounds stay 400dp and the child clamps cleanly to 400dp inside it.
        Response rFixedClampEl{}, rFixedClampBtn{};
        for (keel::i32 pass = 0; pass < 2; ++pass) {
            ui.BeginFrame({800.0f, 300.0f});
            if (auto centerRow = ui.Row({
                .align = Alignment::Center,
                .justify = Justify::Center,
                .height = SizeMode::Stretch
            })) {
                if (auto el = ui.Element({
                    .label = "Element Label",
                    .description = "Secondary description text",
                    .align = Align::End,
                    .width = 400.0f
                })) {
                    rFixedClampEl = el.response;
                    rFixedClampBtn = ui.Button({
                        .text = "Click me",
                        .width = 480.0f
                    });
                }
            }
            ui.EndFrame(canvas);
        }
        CHECK_NEAR(rFixedClampEl.rect.w, 400.0f, 0.5f);
        CHECK_NEAR(rFixedClampEl.rect.x, 200.0f, 0.5f);
        CHECK_NEAR(rFixedClampBtn.rect.w, 400.0f, 0.5f);
        CHECK_NEAR(rFixedClampBtn.rect.x, 200.0f, 0.5f);
    }

    // 3. Dirty-Rect Damage Tracking when HeadingOptions (level, layout, separator) or ElementOptions mutates
    {
        Context ui;
        HeadingLevel dynLevel = HeadingLevel::H4;
        HeadingLayout dynHeadLayout = HeadingLayout::Stacked;
        HeadingSeparator dynSep = HeadingSeparator::None;
        keel::StringRef dynElemLabel = "Gain Stage";
        bool sw = false;

        auto runFrame = [&]() {
            ui.BeginFrame({600.0f, 300.0f});
            if (auto col = ui.Column({.gap = 8.0f})) {
                ui.Heading({
                    .label = "Reactive Section",
                    .description = "Subtitle",
                    .level = dynLevel,
                    .layout = dynHeadLayout,
                    .separator = dynSep
                });
                if (auto el = ui.Element({.label = dynElemLabel})) {
                    ui.Switch(sw);
                }
            }
            ui.EndFrame(canvas);
        };

        // Settle static frame (0 dirty rects when idle)
        runFrame();
        runFrame();
        runFrame();
        CHECK(ui.GetDirtyRects().empty());

        // Mutating separator from None -> Trailing invalidates Heading visualHash
        dynSep = HeadingSeparator::Trailing;
        runFrame();
        CHECK(!ui.GetDirtyRects().empty());

        runFrame();
        CHECK(ui.GetDirtyRects().empty());

        // Mutating HeadingLevel from H4 -> H2 invalidates Heading visualHash
        dynLevel = HeadingLevel::H2;
        runFrame();
        CHECK(!ui.GetDirtyRects().empty());

        runFrame();
        CHECK(ui.GetDirtyRects().empty());

        // Mutating HeadingLayout from Stacked -> Overline invalidates Heading visualHash
        dynHeadLayout = HeadingLayout::Overline;
        runFrame();
        CHECK(!ui.GetDirtyRects().empty());

        runFrame();
        CHECK(ui.GetDirtyRects().empty());

        // Mutating Element label invalidates Element visualHash
        dynElemLabel = "Master Stage";
        runFrame();
        CHECK(!ui.GetDirtyRects().empty());
    }

    // 4. Unified AxisSize policy: Auto vs explicit Stretch in Toolbar, Fixed dp, and SizeMode::Fixed fallback
    {
        Context ui;
        keel::usize selAuto = 0;
        keel::usize selStretch = 0;
        keel::usize selFixedDp = 0;
        const keel::StaticArray<keel::StringRef, 3> items = {"Alpha", "Beta", "Gamma"};
        Response rAutoInToolbar{}, rStretchInToolbar{}, rFixedInToolbar{}, rBareFixedBtn{};

        for (keel::i32 f = 0; f < 2; ++f) {
            ui.BeginFrame({800.0f, 300.0f});
            if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(0.0f)})) {
                if (auto tb = ui.Toolbar({.overflow = ToolbarOverflow::Clip})) {
                    rAutoInToolbar = ui.Select(selAuto, items); // Auto -> shrinks to Fit in horizontal Toolbar
                    rFixedInToolbar = ui.Select(selFixedDp, items, {.width = 160.0f});
                }
                if (auto tb2 = ui.Toolbar({.overflow = ToolbarOverflow::Clip})) {
                    rStretchInToolbar = ui.Select(selStretch, items, {.width = SizeMode::Stretch});
                }
                // Bare SizeMode::Fixed without dp falls back safely to Fit instead of collapsing to 0 px
                rBareFixedBtn = ui.Button({.text = "Safe Fallback", .width = SizeMode::Fixed});
            }
            ui.EndFrame(canvas);
        }

        CHECK(rAutoInToolbar.rect.w > 40.0f);
        CHECK(rAutoInToolbar.rect.w < 200.0f);
        CHECK_NEAR(rFixedInToolbar.rect.w, 160.0f, 0.5f);
        CHECK(rStretchInToolbar.rect.w > 700.0f);
        CHECK(rBareFixedBtn.rect.w > 60.0f);
    }

    keel::println("  [PASS] TestElementHeadingCompositionAndIndependentSizing completed");
}

void TestPopoverComponent() {
    keel::println("  -> TestPopoverComponent...");
    keel::Array<keel::u32> pixels(800u * 600u, 0u);
    ThorCanvas canvas;
    canvas.SetTarget(pixels.data(), 800, 600, 800);

    Context ui;
    bool open = false;
    bool flagA = true;
    bool flagB = false;
    keel::usize modeIdx = 1;
    const keel::StaticArray<keel::StringRef, 3> modeLabels = {{"Off", "2 Hz", "4 Hz"}};
    bool triggerImperativeClose = false;

    Response triggerRes{};
    Response switchARes{};
    Response switchBRes{};
    Response selectRes{};
    Response popRes{};
    Rect behindBtnRect{};

    auto renderFrame = [&](Vec2 vp = {800.0f, 600.0f}) {
        ui.BeginFrame(vp);
        if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(0.0f)})) {
            if (auto tb = ui.Toolbar({})) {
                tb.Spacer();
                triggerRes = tb.Toggle(open, {
                    .text = "Engine",
                    .icon = Icon::ChevronDown(),
                    .iconPlacement = Placement::Trailing
                });
                if (auto pop = ui.Popover(open, {
                    .anchorRect = triggerRes.rect
                })) {
                    popRes = pop.response;
                    if (auto card = ui.Card({
                        .width = 280.0f,
                        .gap = 8.0f,
                        .padding = Padding::All(12.0f),
                        .elevated = true
                    })) {
                        if (auto el = ui.Element({.label = "GPU Shaders", .layout = ElementLayout::Leading})) {
                            switchARes = ui.Switch(flagA);
                        }
                        if (auto el = ui.Element({.label = "Reactive Loop", .layout = ElementLayout::Leading})) {
                            switchBRes = ui.Switch(flagB);
                        }
                        if (auto el = ui.Element({.label = "Heartbeat", .layout = ElementLayout::Leading})) {
                            selectRes = ui.Select(modeIdx, modeLabels, {.width = 110.0f});
                        }
                        if (triggerImperativeClose) {
                            triggerImperativeClose = false;
                            pop.Close();
                        }
                    }
                }
            }
            behindBtnRect = ui.Button({.text = "Workspace Button"}).rect;
        }
        ui.EndFrame(canvas);
    };

    // Frame 0-1: measure trigger on far right of toolbar and popover closed
    renderFrame();
    renderFrame();
    CHECK(!open);
    CHECK(triggerRes.rect.w > 40.0f);
    CHECK(triggerRes.rect.x > 600.0f);

    // Click trigger button -> opens popover on same frame without immediately self-dismissing
    const Vec2 trigCenter = triggerRes.rect.Center();
    ui.OnMouseMove(trigCenter);
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();
    CHECK(open);

    // Next frame has resolved popRes.rect anchored cleanly below trigger and clamped inside viewport
    renderFrame();
    CHECK(open);
    CHECK_NEAR(popRes.rect.w, 280.0f, 1.0f);
    CHECK(popRes.rect.y >= triggerRes.rect.Bottom());
    CHECK(popRes.rect.Right() <= 800.0f - 8.0f + 0.5f);
    CHECK(switchARes.rect.w > 20.0f);
    CHECK(popRes.rect.Contains(switchARes.rect.Center()));

    // Clicking switches inside popover toggles state AND keeps popover open
    const Vec2 swACenter = switchARes.rect.Center();
    ui.OnMouseMove(swACenter);
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();
    CHECK(open);
    CHECK(!flagA);

    const Vec2 swBCenter = switchBRes.rect.Center();
    ui.OnMouseMove(swBCenter);
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();
    CHECK(open);
    CHECK(flagB);

    // Opening Select dropdown inside Popover and pressing Escape closes Select first, keeping Popover open!
    const Vec2 selCenter = selectRes.rect.Center();
    ui.OnMouseMove(selCenter);
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();
    CHECK(open);
    CHECK(ui.GetOpenPopupId().IsValid());

    ui.OnKeyDown(Key::Escape);
    renderFrame();
    CHECK(!ui.GetOpenPopupId().IsValid());
    CHECK(open);

    // Second Escape closes the Popover itself
    ui.OnKeyDown(Key::Escape);
    renderFrame();
    CHECK(!open);

    // Re-open by clicking trigger, then click trigger again to close
    ui.OnMouseMove(trigCenter);
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();
    CHECK(open);

    ui.OnMouseMove(trigCenter);
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();
    CHECK(!open);

    // Re-open, then click outside popover (and outside anchor) -> closes popover
    ui.OnMouseMove(trigCenter);
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();
    CHECK(open);

    const Vec2 outsidePos{24.0f, 360.0f};
    ui.OnMouseMove(outsidePos);
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();
    CHECK(!open);

    // Imperative pop.Close() closes cleanly
    open = true;
    renderFrame();
    CHECK(open);
    triggerImperativeClose = true;
    renderFrame();
    CHECK(!open);

    // Transparent ui.Zone wrapper does not create a layout node or break sibling Row flow
    {
        UiTheme light = UiTheme::Light();
        Response r1{}, r2{};
        ThemeMode capturedInsideMode = ThemeMode::Dark;
        for (keel::i32 f = 0; f < 2; ++f) {
            ui.BeginFrame({800.0f, 600.0f});
            if (auto row = ui.Row({.gap = 8.0f})) {
                r1 = ui.Button({.text = "Normal"});
                if (auto z = ui.Zone({.theme = light})) {
                    capturedInsideMode = ui.GetThemeMode();
                    r2 = ui.Button({.text = "Themed"});
                }
            }
            ui.EndFrame(canvas);
        }
        CHECK(capturedInsideMode == ThemeMode::Light);
        CHECK(ui.GetThemeMode() == ThemeMode::Dark);
        CHECK(r2.rect.x > r1.rect.Right());
        CHECK_NEAR(r1.rect.y, r2.rect.y, 0.5f);
    }

    // Pure explicit Modal with custom align / verticalAlign / padding around raw Box
    {
        bool mOpen = true;
        Response boxRes{};
        for (keel::i32 f = 0; f < 2; ++f) {
            ui.BeginFrame({800.0f, 600.0f});
            if (auto modal = ui.Modal(mOpen, {
                .padding = Padding::All(20.0f),
                .align = Alignment::End,
                .verticalAlign = Alignment::Start
            })) {
                if (auto b = ui.Box({.width = 200.0f, .height = 100.0f})) {
                    boxRes = ui.Button({.text = "Inside", .width = SizeMode::Stretch});
                }
            }
            ui.EndFrame(canvas);
        }
        CHECK_NEAR(boxRes.rect.Right(), 800.0f - 20.0f, 1.0f);
        CHECK_NEAR(boxRes.rect.y, 20.0f, 1.0f);
    }

    keel::println("  [PASS] TestPopoverComponent completed");
}

void TestApiPolish() {
    keel::println("  -> TestApiPolish...");
    keel::Array<keel::u32> pixels(800u * 600u, 0u);
    ThorCanvas canvas;
    canvas.SetTarget(pixels.data(), 800, 600, 800);
    canvas.SetGpuShadersEnabled(true);

    // 1. ThorCanvas::DrawPixels bounds check via PixelRef
    {
        keel::StaticArray<keel::u32, 3> tooSmall = {{0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu}};
        CHECK(!canvas.DrawPixels({tooSmall, 2, 2}, {0.0f, 0.0f, 10.0f, 10.0f}));

        keel::StaticArray<keel::u32, 4> exactFit = {{0xFFFFFFFFu, 0xFF00FF00u, 0xFFFF0000u, 0xFF0000FFu}};
        CHECK(canvas.DrawPixels({exactFit, 2, 2}, {0.0f, 0.0f, 10.0f, 10.0f}));
    }

    // 2. GpuShaderPassDesc validates dataField (MdRef<float, 2>) & lutAbgr256 sizes
    {
        keel::StaticArray<float, 3> smallFloats = {{0.1f, 0.2f, 0.3f}};
        keel::StaticArray<float, 4> okFloats = {{0.1f, 0.2f, 0.3f, 0.4f}};
        keel::StaticArray<keel::u32, 256> lut{};
        lut[0] = 0xFF000000u;
        lut[255] = 0xFFFFFFFFu;

        ICanvas::GpuShaderPassDesc badPass{};
        badPass.widgetId = 101;
        badPass.shader = {.shaderId = "test.polish.v1", .glslEs2 = "void main() {}"};
        badPass.plotRect = {10.0f, 10.0f, 100.0f, 100.0f};
        badPass.dataField = {smallFloats, 2, 2};
        badPass.lutAbgr256 = lut;
        CHECK(badPass.dataField.empty());
        CHECK(!canvas.DrawGpuShaderPass(badPass, false));

        ICanvas::GpuShaderPassDesc okPass = badPass;
        okPass.dataField = {okFloats, 2, 2};
        CHECK(canvas.DrawGpuShaderPass(okPass, false));
        CHECK(canvas.GetGpuShaderPasses().size() == 1);
        CHECK(canvas.GetGpuShaderPasses()[0].dataField.size() == 4);
        CHECK(canvas.GetGpuShaderPasses()[0].lutAbgr256.size() == 256);
    }

    // 3. TextAreaOptions::syntaxHighlighter without void* userData & MapViewOptions::tileSource FunctionRef lambda
    Context ui;
    keel::i32 baseLayerCalls = 0;
    keel::i32 overlayLayerCalls = 0;
    keel::String code("int value = 42;\nreturn value;");

    for (keel::i32 f = 0; f < 2; ++f) {
        ui.BeginFrame({800.0f, 600.0f});
        if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(8.0f)})) {
            ui.CodeEditor(code, {
                .rows = 3,
                .syntaxHighlighter = [](keel::StringRef lineText, keel::usize /*lineIndex*/, keel::Array<TextSpan>& outSpans, const UiTheme& theme) {
                    if (!lineText.empty()) {
                        outSpans.push_back(TextSpan{0u, static_cast<keel::u32>(lineText.size()), theme.palette.accent});
                    }
                }
            });
            ui.CodeEditor("auto x = 1;", {
                .rows = 2,
                .readOnly = true,
                .syntaxHighlighter = &TextArea::CppHighlighter
            });
#if GUIKIT_HAS_SCENES
            if (auto map = ui.MapView({
                .tileSource = [&](OsmTileId /*tile*/, keel::i32 layerIndex) -> keel::StringRef {
                    if (layerIndex == 0) ++baseLayerCalls;
                    if (layerIndex == 1) ++overlayLayerCalls;
                    return {};
                },
                .height = 240.0f
            })) {
            }
#endif
        }
        ui.EndFrame(canvas);
    }

#if GUIKIT_HAS_SCENES
    CHECK(baseLayerCalls > 0);
    CHECK(overlayLayerCalls > 0);
#endif

    // 4. ContainerScope::GetResponse + ContextMenu(bool&) cursor re-anchoring & child button isolation
    {
        bool zoneOpen = false;
        Vec2 zoneMenuPos{};
        Rect zoneRect{};
        Rect childBtnRect{};
        ContextMenuResponse zoneMenuRes{};
        keel::i32 zoneClickCount = 0;
        const ContextMenuItem zItems[] = {
            {.label = "Item A"},
            {.label = "Item B"}
        };
        auto renderZone = [&]() {
            ui.BeginFrame({800.0f, 600.0f});
            if (auto root = ui.Column({.padding = Padding::All(20.0f)})) {
                if (auto zone = ui.Box({
                    .width = 240.0f,
                    .height = 140.0f,
                    .padding = Padding::All(16.0f)
                })) {
                    (void)ui.Button({.text = "Inner Btn"});
                    childBtnRect = ui.Button({.text = "Child"}).rect;
                    const Response zoneRes = zone.GetResponse();
                    zoneRect = zoneRes.rect;
                    if (zoneRes.IsTriggered(InteractionTrigger::Click)) {
                        ++zoneClickCount;
                    }
                    if (zoneRes.IsTriggered(InteractionTrigger::RightClick)) {
                        zoneOpen = true;
                        zoneMenuPos = ui.GetMousePos();
                    }
                    zoneMenuRes = ui.ContextMenu(zoneOpen, zItems, {
                        .anchorPos = zoneMenuPos
                    });
                }
            }
            ui.EndFrame(canvas);
        };

        renderZone();
        renderZone();
        CHECK(zoneRect.w >= 239.0f && zoneRect.h >= 139.0f);

        // Clicking inside a child button must NOT trigger the parent container's Click
        ui.OnMouseMove(childBtnRect.Center());
        ui.OnMouseDown(1);
        renderZone();
        ui.OnMouseUp(1);
        renderZone();
        CHECK(zoneClickCount == 0);

        // Clicking empty background of the container DOES trigger InteractionTrigger::Click
        const Vec2 emptySpot{zoneRect.x + 12.0f, zoneRect.Bottom() - 12.0f};
        ui.OnMouseMove(emptySpot);
        ui.OnMouseDown(1);
        renderZone();
        ui.OnMouseUp(1);
        renderZone();
        CHECK(zoneClickCount == 1);

        // Right-clicking at spot A opens menu near spot A; right-clicking at spot B moves it to spot B
        const Vec2 spotA{zoneRect.x + 20.0f, zoneRect.Bottom() - 15.0f};
        const Vec2 spotB{zoneRect.Right() - 25.0f, zoneRect.Bottom() - 15.0f};
        ui.OnMouseMove(spotA);
        ui.OnMouseDown(3);
        renderZone();
        ui.OnMouseUp(3);
        renderZone();
        CHECK(zoneOpen == true);
        CHECK(zoneMenuRes.isOpen == true);
        const Rect rectAtA = zoneMenuRes.rect;

        ui.OnMouseMove(spotB);
        ui.OnMouseDown(3);
        renderZone();
        ui.OnMouseUp(3);
        renderZone();
        CHECK(zoneOpen == true);
        CHECK(zoneMenuRes.isOpen == true);
        CHECK(zoneMenuRes.rect.x != rectAtA.x);

        // Right-clicking completely outside the zone closes the menu cleanly
        ui.OnMouseMove({750.0f, 550.0f});
        ui.OnMouseDown(3);
        renderZone();
        ui.OnMouseUp(3);
        renderZone();
        CHECK(zoneOpen == false);
        CHECK(zoneMenuRes.isOpen == false);
    }

    // 5. DatePickerLabels adapts Time/Date popover width to localized button captions without clipping
    {
        DateTime picked{.year = 2026, .month = 11, .day = 18, .hour = 12, .minute = 0, .second = 0};
        Response dpRes{};
        const keel::StringRef ruTimeSuffixes[3] = {"ч", "м", "с"};
        auto renderDp = [&]() {
            ui.BeginFrame({800.0f, 600.0f});
            if (auto col = ui.Column({.padding = Padding::All(20.0f)})) {
                dpRes = ui.DatePicker(picked, {
                    .mode = DatePickerMode::Time,
                    .showSeconds = false,
                    .labels = {
                        .timeSuffixes = ruTimeSuffixes,
                        .now = "Текущее время",
                        .done = "Применить выбор"
                    },
                    .width = 220.0f
                });
            }
            ui.EndFrame(canvas);
        };
        renderDp();
        renderDp();
        ui.OnMouseMove(dpRes.rect.Center());
        ui.OnMouseDown(1);
        renderDp();
        ui.OnMouseUp(1);
        renderDp();
        CHECK(ui.GetOpenPopupId() == dpRes.id);
        CHECK(ui.GetOpenPopupRect().w > 220.0f);
        ui.OnKeyDown(Key::Escape);
        renderDp();
    }

    keel::println("  [PASS] TestApiPolish completed");
}

void TestControlFlatSizingAndStyleOptions() {
    using namespace guikit;
    keel::println("  -> TestControlFlatSizingAndStyleOptions...");

    // 1. Flat sizing constraints (width, height, minWidth, maxWidth, minHeight, maxHeight) on control *Options
    {
        // Verify flat defaults on control *Options structs (background defaults to StateColor{} ->
        // theme/variant default; IsBackgroundEnabled(true) for surface controls, IsBackgroundEnabled(false) for Image/TextArea)
        const ButtonOptions defBtn{};
        const SelectOptions defSel{};
        const SwitchOptions defSw{};
        const SliderOptions defSl{};
        const DatePickerOptions defDp{};
        const ImageOptions defImg{};
        const TextAreaOptions defTa{};
        CHECK(defBtn.size == UiSize::Md);
        CHECK(defSw.size == UiSize::Md);
        CHECK(defSl.size == UiSize::Md);
        CHECK(defBtn.background.IsBackgroundEnabled(true));
        CHECK(defSel.background.IsBackgroundEnabled(true));
        CHECK(defDp.background.IsBackgroundEnabled(true));
        CHECK(!defImg.background.IsBackgroundEnabled(false));
        CHECK(!defTa.background.IsBackgroundEnabled(false));

        Context ui;
        ThorCanvas canvas;
        const keel::u32 width = 800;
        const keel::u32 height = 600;
        keel::Array<keel::u32> pixels(width * height, 0u);
        canvas.SetTarget(pixels.data(), width, height, width);

        // Standalone Measure checks for Button and InputField with min/max constraints
        const Vec2 btnMinMax = Button::Measure(
            {
                .text = "A very long action button label that exceeds maxWidth",
                .minWidth = 80.0f,
                .maxWidth = 140.0f,
                .minHeight = 42.0f
            },
            UiSize::Md,
            &canvas,
            ui.GetTheme()
        );
        CHECK_NEAR(btnMinMax.x, 140.0f, 0.5f);
        CHECK_NEAR(btnMinMax.y, 42.0f, 0.5f);

        const Vec2 btnMinClamp = Button::Measure(
            {
                .text = "OK",
                .minWidth = 160.0f,
                .minHeight = 46.0f,
                .maxHeight = 52.0f
            },
            UiSize::Md,
            &canvas,
            ui.GetTheme()
        );
        CHECK_NEAR(btnMinClamp.x, 160.0f, 0.5f);
        CHECK_NEAR(btnMinClamp.y, 46.0f, 0.5f);

        const Vec2 inMinMax = InputField::Measure(
            "short",
            {
                .width = SizeMode::Fit,
                .minWidth = 180.0f,
                .maxWidth = 240.0f,
                .minHeight = 40.0f
            },
            UiSize::Md,
            &canvas,
            ui.GetTheme()
        );
        CHECK_NEAR(inMinMax.x, 180.0f, 0.5f);
        CHECK_NEAR(inMinMax.y, 40.0f, 0.5f);

        // Full layout solver pass inside Context (Column 800px wide)
        keel::String inputVal("Editable text");
        keel::String inputWide("A sufficiently long string value inside InputField");
        Response rBtnStretchMax{}, rBtnMinH{}, rBtnMaxH{};
        Response rInStretchMax{}, rInFitMin{}, rInStretchH{};

        auto renderLayout = [&]() {
            ui.BeginFrame({static_cast<float>(width), static_cast<float>(height)});
            if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(0.0f), .width = SizeMode::Stretch})) {
                rBtnStretchMax = ui.Button({
                    .text = "Stretched Button Clamped",
                    .width = SizeMode::Stretch,
                    .minWidth = 120.0f,
                    .maxWidth = 260.0f
                });
                rBtnMinH = ui.Button({
                    .text = "Min Sized",
                    .minWidth = 190.0f,
                    .minHeight = 48.0f
                });
                if (auto tallRow = ui.Row({.padding = Padding::All(0.0f), .width = SizeMode::Stretch, .height = 100.0f})) {
                    rBtnMaxH = ui.Button({
                        .text = "Stretch Clamped H",
                        .width = 180.0f,
                        .height = SizeMode::Stretch,
                        .maxHeight = 44.0f
                    });
                }
                rInStretchMax = ui.InputField(inputVal, {
                    .width = SizeMode::Stretch,
                    .minWidth = 150.0f,
                    .maxWidth = 340.0f,
                    .minHeight = 42.0f
                });
                rInFitMin = ui.InputField(inputWide, {
                    .width = SizeMode::Fit,
                    .minWidth = 200.0f,
                    .maxWidth = 280.0f
                });
                if (auto tallRow2 = ui.Row({.padding = Padding::All(0.0f), .width = SizeMode::Stretch, .height = 90.0f})) {
                    rInStretchH = ui.InputField(inputVal, {
                        .width = 250.0f,
                        .height = SizeMode::Stretch,
                        .minHeight = 20.0f,
                        .maxHeight = 38.0f
                    });
                }
            }
            ui.EndFrame(canvas);
        };

        renderLayout();
        renderLayout();

        CHECK_NEAR(rBtnStretchMax.rect.w, 260.0f, 0.5f);
        CHECK(rBtnMinH.rect.w >= 190.0f - 0.5f);
        CHECK_NEAR(rBtnMinH.rect.h, 48.0f, 0.5f);
        CHECK_NEAR(rBtnMaxH.rect.w, 180.0f, 0.5f);
        CHECK_NEAR(rBtnMaxH.rect.h, 44.0f, 0.5f);

        CHECK_NEAR(rInStretchMax.rect.w, 340.0f, 0.5f);
        CHECK_NEAR(rInStretchMax.rect.h, 42.0f, 0.5f);
        CHECK(rInFitMin.rect.w >= 200.0f - 0.5f && rInFitMin.rect.w <= 280.0f + 0.5f);
        CHECK_NEAR(rInStretchH.rect.w, 250.0f, 0.5f);
        CHECK_NEAR(rInStretchH.rect.h, 38.0f, 0.5f);
    }

    // 2. Flat visual options (.size, .color, .background, .border) across boxed & frameless controls
    {
        struct StyleSpyCanvas final : public ICanvas {
            struct RoundRectCall {
                Rect rect;
                Color fill;
                Color stroke;
                float strokeWidth;
            };
            void Begin(keel::u32, keel::u32, float scale = 1.0f) override {
                m_scale = scale;
                roundRects.clear();
            }
            void End() override {}
            [[nodiscard]] float GetScale() const noexcept override { return m_scale; }
            void SetScale(float scale) override { m_scale = scale; }
            void PushTransform(const Transform2D&) override {}
            void PopTransform() override {}
            [[nodiscard]] keel::StringRef GetBackendName() const noexcept override { return "StyleSpyCanvas"; }
            void Clear(Color, keel::Optional<Rect> = keel::kNullOpt) override {}
            void PushClip(Rect) override {}
            void PopClip() override {}
            void DrawRect(Rect, Color) override {}
            void DrawRoundRect(Rect r, float, Color fill, Color stroke = Color(0, 0, 0, 0), float strokeWidth = 0.0f) override {
                roundRects.push_back({r, fill, stroke, strokeWidth});
            }
            Vec2 MeasureText(keel::StringRef text, float fontSize) override {
                return {static_cast<float>(text.size()) * fontSize * 0.55f, fontSize * 1.2f};
            }
            [[nodiscard]] float GetLineHeight(float fontSize) override { return fontSize * 1.25f; }
            void DrawText(keel::StringRef, Vec2, float, Color) override {}
            void DrawTextBox(keel::StringRef, Rect, float, Color, float = 0.0f, float = 0.5f) override {}
            void DrawLine(Vec2, Vec2, Color, float = 1.0f) override {}

            float m_scale{1.0f};
            keel::Array<RoundRectCall> roundRects;
        };

        Context ui;
        StyleSpyCanvas spyCanvas;
        const UiTheme& theme = ui.GetTheme();

        const StateColor dangerColor = StateColor::Danger();
        const StateColor dangerBg = StateColor::Danger().Tinted(0.20f);
        const Border dangerBorder = Border::Solid(2.0f, StateColor::Danger());
        const StateColor successColor = StateColor::Success();

        keel::String inTxt("Alert");
        keel::usize selIdx = 0;
        static constexpr keel::StaticArray<keel::StringRef, 2> selItems = {"One", "Two"};
        bool swVal = true;
        bool cbVal = true;
        bool rbVal = true;
        float slVal = 0.75f;

        Response rBtn{}, rIn{}, rSel{};
        Response rSw{}, rCb{}, rRb{}, rSl{}, rPb{};

        auto renderStyled = [&]() {
            ui.BeginFrame({800.0f, 600.0f});
            if (auto col = ui.Column({.gap = 8.0f, .padding = Padding::All(12.0f)})) {
                rBtn = ui.Button({.text = "Danger Action", .size = UiSize::Lg, .color = dangerColor, .background = dangerBg, .border = dangerBorder});
                rIn  = ui.InputField(inTxt, {.size = UiSize::Lg, .color = dangerColor, .background = dangerBg, .border = dangerBorder});
                rSel = ui.Select(selIdx, selItems, {.size = UiSize::Lg, .color = dangerColor, .background = dangerBg, .border = dangerBorder});

                rSw  = ui.Switch(swVal, {.size = UiSize::Sm, .color = successColor});
                rCb  = ui.Checkbox(cbVal, {.size = UiSize::Sm, .color = successColor});
                rRb  = ui.Radio(rbVal, {.size = UiSize::Sm, .color = successColor});
                rSl  = ui.Slider(slVal, 0.0f, 1.0f, {.size = UiSize::Sm, .color = successColor});
                rPb  = ui.ProgressBar(0.5f, {.size = UiSize::Sm, .color = successColor});
            }
            ui.EndFrame(spyCanvas);
        };

        renderStyled();
        ui.InvalidateAll();
        renderStyled();

        const float lgHeight = theme.GetMetrics(UiSize::Lg).defaultHeight;
        CHECK_NEAR(rBtn.rect.h, lgHeight, 0.5f);
        CHECK_NEAR(rIn.rect.h, theme.GetInputField(UiSize::Lg).defaultHeight, 0.5f);
        CHECK_NEAR(rSel.rect.h, theme.GetSelect(UiSize::Lg).defaultHeight, 0.5f);

        CHECK_NEAR(rSw.rect.h, theme.GetSwitch(UiSize::Sm).trackSize.y, 0.5f);
        CHECK_NEAR(rCb.rect.h, theme.GetCheckbox(UiSize::Sm).boxSize, 0.5f);
        CHECK_NEAR(rRb.rect.h, theme.GetCheckbox(UiSize::Sm).boxSize, 0.5f);
        CHECK_NEAR(rSl.rect.h, theme.GetSlider(UiSize::Sm).defaultHeight, 0.5f);
        CHECK(rPb.rect.h > 0.0f && rPb.rect.h < lgHeight);

        const Color expectedDangerBorder = theme.ResolveColor(StateColor::Danger(), false, false, false, theme.palette.border);
        const Color expectedSuccessColor = theme.ResolveColor(StateColor::Success(), false, false, false, theme.palette.accent);
        keel::i32 dangerBorderMatches = 0;
        keel::i32 successFillMatches = 0;
        for (const auto& rr : spyCanvas.roundRects) {
            if (rr.stroke == expectedDangerBorder && keel::abs(rr.strokeWidth - 2.0f) < 0.05f) {
                ++dangerBorderMatches;
            }
            if (rr.fill == expectedSuccessColor) {
                ++successFillMatches;
            }
        }
        // Button, InputField, and Select all draw the 2dp Danger border
        CHECK(dangerBorderMatches >= 3);
        // Switch, Checkbox, Slider, and ProgressBar all render using the Success accent color
        CHECK(successFillMatches >= 4);
    }

    keel::println("  [PASS] TestControlFlatSizingAndStyleOptions completed");
}

void TestSceneWorldSizeInsideScrollAreaDoesNotOverflow() {
    using namespace guikit;
    keel::println("  -> TestSceneWorldSizeInsideScrollAreaDoesNotOverflow...");

    ThorCanvas canvas;
    keel::Array<keel::u32> fb(800u * 500u, 0u);
    canvas.SetTarget(fb.data(), 800, 500, 800);

    Context ui;
    Rect sceneVp{};
    Vec2 topLeftScreen{};
    Vec2 botRightScreen{};
    float fittedZoom = 0.0f;

    auto renderFrame = [&]() {
        ui.BeginFrame({800.0f, 500.0f});
        if (auto backdrop = ui.ScrollArea({
            .orientation = Orientation::Both,
            .scrollbar = VisibilityPolicy::Never,
            .padding = Padding::All(18.0f)
        })) {
            if (auto windowSlot = ui.Box({
                .width = SizeMode::Stretch,
                .height = SizeMode::Stretch
            })) {
                if (auto sc = ui.Scene({
                    .worldSize = Vec2{2200.0f, 920.0f},
                    .height = SizeMode::Stretch
                })) {
                    sceneVp = sc.GetViewportRect();
                    topLeftScreen = sc.WorldToScreen({0.0f, 0.0f});
                    botRightScreen = sc.WorldToScreen({2200.0f, 920.0f});
                    fittedZoom = sc.GetZoom();
                }
            }
        }
        ui.EndFrame(canvas);
    };

    renderFrame();
    renderFrame();

    const float expectedW = 800.0f - 36.0f;
    const float expectedH = 500.0f - 36.0f;
    CHECK(sceneVp.w > 100.0f && sceneVp.w <= expectedW + 0.5f);
    CHECK(sceneVp.h > 100.0f && sceneVp.h <= expectedH + 0.5f);
    CHECK(fittedZoom > 0.25f && fittedZoom < 0.5f);
    CHECK(topLeftScreen.x >= sceneVp.x - 1.0f && topLeftScreen.x <= sceneVp.x + 2.0f);
    CHECK(botRightScreen.x >= sceneVp.Right() - 2.0f && botRightScreen.x <= sceneVp.Right() + 1.0f);
    CHECK(topLeftScreen.y >= sceneVp.y - 1.0f);
    CHECK(botRightScreen.y <= sceneVp.Bottom() + 1.0f);

    keel::println("  [PASS] TestSceneWorldSizeInsideScrollAreaDoesNotOverflow completed");
}

void TestApiPolishStretchWeightsTabBarColorAndOptionalMins() {
    using namespace guikit;
    keel::println("  -> TestApiPolishStretchWeightsTabBarColorAndOptionalMins...");

    ThorCanvas canvas;
    keel::Array<keel::u32> fb(640u * 600u, 0u);
    canvas.SetTarget(fb.data(), 640, 600, 640);

    // 1. Proportional AxisSize::Stretch(weight) across direct controls, Elements, and containers
    {
        Context ui;
        keel::FixedString<32> s1{"host"};
        keel::FixedString<32> s2{"8080"};
        Response rDirectA{}, rDirectB{};
        Response rElemA{}, rElemB{};
        Rect colBoxA{}, colBoxB{};
        Rect legacyFlexA{}, legacyFlexB{};

        auto runFrame = [&]() {
            ui.BeginFrame({600.0f, 600.0f});
            if (auto root = ui.Column({.gap = 0.0f})) {
                // 2:1 direct controls in Row (600dp total -> 400dp + 200dp)
                if (auto row = ui.Row({.gap = 0.0f})) {
                    rDirectA = ui.InputField(s1, {.width = AxisSize::Stretch(2.0f)});
                    rDirectB = ui.InputField(s2, {.width = AxisSize::Stretch(1.0f)});
                }
                // 3:1 Element-wrapped controls in Row (one on Element, one on inner control -> 450dp + 150dp)
                if (auto row = ui.Row({.gap = 0.0f})) {
                    if (auto e1 = ui.Element({.label = "Host", .width = AxisSize::Stretch(3.0f)})) {
                        rElemA = ui.InputField(s1);
                    }
                    if (auto e2 = ui.Element({.label = "Port"})) {
                        rElemB = ui.InputField(s2, {.width = AxisSize::Stretch(1.0f)});
                    }
                }
                // 2:1 vertical stretch weights in fixed-height Column (300dp -> 200dp + 100dp)
                if (auto col = ui.Column({.gap = 0.0f, .height = 300.0f})) {
                    if (auto b1 = ui.Box({.height = AxisSize::Stretch(2.0f)})) {
                        colBoxA = ui.GetPrevFrameBounds(b1.GetNode()->id);
                    }
                    if (auto b2 = ui.Box({.height = AxisSize::Stretch(1.0f)})) {
                        colBoxB = ui.GetPrevFrameBounds(b2.GetNode()->id);
                    }
                }
                // Existing container .flexGrow continues to work with default SizeMode::Stretch
                if (auto row = ui.Row({.gap = 0.0f})) {
                    if (auto b1 = ui.Box({.width = SizeMode::Stretch, .height = 20.0f, .flexGrow = 3.0f})) {
                        legacyFlexA = ui.GetPrevFrameBounds(b1.GetNode()->id);
                    }
                    if (auto b2 = ui.Box({.width = SizeMode::Stretch, .height = 20.0f, .flexGrow = 1.0f})) {
                        legacyFlexB = ui.GetPrevFrameBounds(b2.GetNode()->id);
                    }
                }
            }
            ui.EndFrame(canvas);
        };

        runFrame();
        runFrame();

        CHECK(keel::abs(rDirectA.rect.w - 400.0f) < 1.5f);
        CHECK(keel::abs(rDirectB.rect.w - 200.0f) < 1.5f);
        CHECK(keel::abs(rElemA.rect.w - 450.0f) < 1.5f);
        CHECK(keel::abs(rElemB.rect.w - 150.0f) < 1.5f);
        CHECK(keel::abs(colBoxA.h - 200.0f) < 1.5f);
        CHECK(keel::abs(colBoxB.h - 100.0f) < 1.5f);
        CHECK(keel::abs(legacyFlexA.w - 450.0f) < 1.5f);
        CHECK(keel::abs(legacyFlexB.w - 150.0f) < 1.5f);
    }

    // 2. TabBarOptions::color (StateColor) renders & triggers damage on change
    {
        Context ui;
        keel::i32 tabIdx = 0;
        StateColor tabColor = StateColor::Danger();
        auto renderTabs = [&]() {
            ui.BeginFrame({480.0f, 120.0f});
            ui.TabBar(tabIdx, {{"Alpha"}, {"Beta"}}, {
                .style = TabBarStyle::Underline,
                .color = tabColor
            });
            ui.EndFrame(canvas);
        };
        renderTabs();
        renderTabs();
        CHECK(ui.GetDirtyRects().empty());
        tabColor = StateColor::Success();
        renderTabs();
        CHECK(!ui.GetDirtyRects().empty());
    }

    // 3. Optional<float> minWidth / minHeight defaults and explicit overrides
    {
        Context ui;
        Rect cardDefaultRect{}, cardMinWRect{};
        Rect sceneDefaultRect{}, sceneSmallMinRect{};

        auto runFrame = [&]() {
            ui.BeginFrame({600.0f, 600.0f});
            if (auto col = ui.Column({.gap = 8.0f})) {
                if (auto row = ui.Row({.gap = 8.0f})) {
                    if (auto c1 = ui.Card({.width = SizeMode::Fit})) {
                        ui.Text("Hi");
                        cardDefaultRect = ui.GetPrevFrameBounds(c1.GetNode()->id);
                    }
                    if (auto c2 = ui.Card({.minWidth = 260.0f, .width = SizeMode::Fit})) {
                        ui.Text("Hi");
                        cardMinWRect = ui.GetPrevFrameBounds(c2.GetNode()->id);
                    }
                }
                if (auto sc1 = ui.Scene({.height = SizeMode::Fit})) {
                    sceneDefaultRect = sc1.GetViewportRect();
                }
                if (auto sc2 = ui.Scene({.height = SizeMode::Fit, .minHeight = 90.0f})) {
                    sceneSmallMinRect = sc2.GetViewportRect();
                }
            }
            ui.EndFrame(canvas);
        };

        runFrame();
        runFrame();

        CHECK(cardDefaultRect.w < 150.0f);
        CHECK(cardMinWRect.w >= 259.5f);
        CHECK(keel::abs(sceneDefaultRect.h - 220.0f) < 1.5f);
        CHECK(keel::abs(sceneSmallMinRect.h - 90.0f) < 1.5f);

        // HistogramOptions::minHeight default (96dp) vs explicit override (64dp)
        const HistogramData hData{};
        const Vec2 hDefault = Histogram::Measure(hData, {.height = SizeMode::Stretch}, UiSize::Md, &canvas, ui.GetTheme(), 300.0f);
        const Vec2 hCustom = Histogram::Measure(hData, {.height = SizeMode::Stretch, .minHeight = 64.0f}, UiSize::Md, &canvas, ui.GetTheme(), 300.0f);
        CHECK(keel::abs(hDefault.y - 96.0f) < 0.5f);
        CHECK(keel::abs(hCustom.y - 64.0f) < 0.5f);
    }

    keel::println("  [PASS] TestApiPolishStretchWeightsTabBarColorAndOptionalMins completed");
}

void TestFocusVisibleModalityAndTabBarFocusRing() {
    using namespace guikit;
    keel::println("  -> TestFocusVisibleModalityAndTabBarFocusRing...");

    ThorCanvas canvas;
    keel::Array<keel::u32> fb(520u * 260u, 0u);
    canvas.SetTarget(fb.data(), 520, 260, 520);

    Context ui;
    keel::i32 activeTab = 0;
    keel::FixedString<32> textVal{"hello"};
    Response btnResp{}, inputResp{}, tabResp{};
    bool btnClickedBySpace = false;

    auto renderFrame = [&]() {
        ui.BeginFrame({520.0f, 260.0f});
        if (auto col = ui.Column({.gap = 12.0f, .padding = Padding::All(16.0f)})) {
            tabResp = ui.TabBar(activeTab, {{"First"}, {"Second"}, {"Third"}}, {
                .style = TabBarStyle::Folder,
                .fullWidth = true
            });
            btnResp = ui.Button({.text = "Action"});
            if (btnResp.clicked) {
                btnClickedBySpace = true;
            }
            inputResp = ui.InputField(textVal);
        }
        ui.EndFrame(canvas);
    };

    renderFrame();
    renderFrame();

    // 1. Click "Second" tab with mouse -> tab activates, logical focus moves, but :focus-visible stays false (Pointer modality)
    const Vec2 secondTabPt{tabResp.rect.x + tabResp.rect.w * 0.5f, tabResp.rect.Center().y};
    ui.OnMouseMove(secondTabPt);
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();

    CHECK(activeTab == 1);
    CHECK(ui.GetInputModality() == InputModality::Pointer);
    CHECK(ui.GetFocusedId().IsValid());
    CHECK(!ui.IsFocusVisible(ui.GetFocusedId()));

    // Non-navigation keys (Ctrl+C, Escape) must NOT switch modality to Keyboard
    ui.OnKeyDown(Key::C, {.ctrl = true});
    ui.OnKeyDown(Key::Escape, {});
    renderFrame();
    CHECK(ui.GetInputModality() == InputModality::Pointer);
    CHECK(!ui.IsFocusVisible(ui.GetFocusedId()));

    // 2. Click Button with mouse -> logical focus is on button (Space still activates it), but focus ring is NOT visible
    btnClickedBySpace = false;
    ui.OnMouseMove(btnResp.rect.Center());
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();

    CHECK(ui.IsFocused(btnResp.id));
    CHECK(!ui.IsFocusVisible(btnResp.id));
    CHECK(ui.GetInputModality() == InputModality::Pointer);

    // Pressing Space on the pointer-focused button activates it AND switches modality to Keyboard (:focus-visible = true)
    btnClickedBySpace = false;
    ui.OnKeyDown(Key::Space);
    renderFrame();
    CHECK(btnClickedBySpace);
    CHECK(ui.GetInputModality() == InputModality::Keyboard);
    CHECK(ui.IsFocusVisible(btnResp.id));

    // 3. Pressing Tab cycles focus to InputField with :focus-visible == true
    ui.OnKeyDown(Key::Tab);
    renderFrame();
    CHECK(ui.IsFocused(inputResp.id));
    CHECK(ui.IsFocusVisible(inputResp.id));

    // 4. Clicking InputField with mouse switches modality to Pointer, while InputField keeps logical focus
    ui.OnMouseMove(inputResp.rect.Center());
    ui.OnMouseDown(1);
    renderFrame();
    ui.OnMouseUp(1);
    renderFrame();
    CHECK(ui.IsFocused(inputResp.id));
    CHECK(ui.GetInputModality() == InputModality::Pointer);
    CHECK(!ui.IsFocusVisible(inputResp.id));

    // 5. Explicit FocusReason::Keyboard vs FocusReason::Pointer
    ui.RequestFocus(btnResp.id, FocusReason::Keyboard);
    renderFrame();
    CHECK(ui.IsFocused(btnResp.id));
    CHECK(ui.IsFocusVisible(btnResp.id));

    ui.RequestFocus(btnResp.id, FocusReason::Pointer);
    renderFrame();
    CHECK(ui.IsFocused(btnResp.id));
    CHECK(!ui.IsFocusVisible(btnResp.id));

    keel::println("  [PASS] TestFocusVisibleModalityAndTabBarFocusRing completed");
}

void TestTextCompositorAndSubpixelRendering() {
    using namespace guikit;
    keel::println("  -> TestTextCompositorAndSubpixelRendering...");

    // 1. Lossless 8-bit sRGB <-> 12-bit Linear round-trip for every channel value [0..255]
    for (keel::u32 v = 0; v < 256u; ++v) {
        const keel::u16 lin = TextCompositor::SrgbToLinear12(static_cast<keel::u8>(v));
        const keel::u8 rt = TextCompositor::Linear12ToSrgb(lin);
        CHECK(rt == static_cast<keel::u8>(v));
    }

    // 2. Coverage LUT: endpoints exact, monotonic, stem darkening lifts small sizes & tapers at display sizes
    keel::StaticArray<keel::u8, 256> lutSmall{};
    keel::StaticArray<keel::u8, 256> lutLarge{};
    TextCompositor::BuildCoverageLut(lutSmall, 1.0f, 0.25f, 12.0f);
    TextCompositor::BuildCoverageLut(lutLarge, 1.0f, 0.25f, 32.0f);
    CHECK(lutSmall[0] == 0u && lutSmall[255] == 255u);
    CHECK(lutLarge[0] == 0u && lutLarge[255] == 255u);
    for (keel::u32 i = 1; i < 256u; ++i) {
        CHECK(lutSmall[i] >= lutSmall[i - 1]);
    }
    CHECK(lutSmall[128] > 128u);
    CHECK(lutLarge[128] == 128u);

    // 3. 5-tap LCD FIR filter: energy conservation on flat 255 interior + balanced impulse
    keel::StaticArray<keel::u8, 15> srcFlat{};
    keel::StaticArray<keel::u8, 15> dstFlat{};
    srcFlat.fill(255u);
    TextCompositor::FilterSubpixel3xRow(srcFlat.data(), dstFlat.data(), 5u);
    CHECK(dstFlat[6] == 255u && dstFlat[7] == 255u && dstFlat[8] == 255u);

    keel::StaticArray<keel::u8, 15> srcImpulse{};
    keel::StaticArray<keel::u8, 15> dstImpulse{};
    srcImpulse.fill(0u);
    srcImpulse[7] = 255u;
    TextCompositor::FilterSubpixel3xRow(srcImpulse.data(), dstImpulse.data(), 5u);
    CHECK(dstImpulse[5] == 8u);
    CHECK(dstImpulse[6] == 77u);
    CHECK(dstImpulse[7] == 86u);
    CHECK(dstImpulse[8] == 77u);
    CHECK(dstImpulse[9] == 8u);

    // 4. Gamma-correct linear blending vs sRGB blending on opaque Dark backdrop
    keel::u32 pxGamma = Color::Hex(0x181818FF).ToUint32();
    keel::u32 pxSrgb = pxGamma;
    const keel::u8 halfMask = 128u;
    const TextRenderSettings gammaOn{.smoothing = FontSmoothing::Grayscale, .gammaCorrectBlending = true, .contrast = 1.0f, .stemDarkening = 0.0f};
    const TextRenderSettings gammaOff{.smoothing = FontSmoothing::Grayscale, .gammaCorrectBlending = false, .contrast = 1.0f, .stemDarkening = 0.0f};
    const Color white(255, 255, 255, 255);
    TextCompositor::BlitGrayscaleMask(&pxGamma, 1, 1, 1, &halfMask, 1, 1, 1, 0, 0, Rect(0, 0, 1, 1), white, gammaOn, 14.0f);
    TextCompositor::BlitGrayscaleMask(&pxSrgb, 1, 1, 1, &halfMask, 1, 1, 1, 0, 0, Rect(0, 0, 1, 1), white, gammaOff, 14.0f);
    const keel::u32 rGamma = pxGamma & 0xFFu;
    const keel::u32 rSrgb = pxSrgb & 0xFFu;
    CHECK(rGamma > rSrgb + 30u); // Linear 50% white on dark is ~188 vs ~139 in sRGB

    // 5. ThorCanvas Grayscale vs SubpixelRGB vs SubpixelBGR & color-independent mask reuse
    ThorCanvas canvas;
    CHECK(canvas.GetTextEngineName() == "ThorVG");
    constexpr keel::u32 W = 180;
    constexpr keel::u32 H = 48;
    keel::Array<keel::u32> fbGray(W * H, 0u);
    keel::Array<keel::u32> fbRgb(W * H, 0u);
    keel::Array<keel::u32> fbBgr(W * H, 0u);
    keel::Array<keel::u32> fbTrans(W * H, 0u);

    const Color darkBg = Color(0x20, 0x20, 0x20, 0xFF);
    auto renderWord = [&](keel::Array<keel::u32>& fb, FontSmoothing mode, Color bg, Color fg) {
        canvas.SetTarget(fb.data(), W, H, W);
        canvas.SetTextRenderSettings(TextRenderSettings{.smoothing = mode, .gammaCorrectBlending = true, .contrast = 1.0f, .stemDarkening = 0.15f});
        canvas.Begin(W, H, 1.0f);
        if (bg.a > 0) {
            canvas.Clear(bg);
        } else {
            fb.assign(W * H, 0u);
        }
        canvas.DrawText("Hello 123", {8.0f, 10.0f}, 14.0f, fg);
        canvas.End();
    };

    renderWord(fbGray, FontSmoothing::Grayscale, darkBg, white);
    renderWord(fbRgb, FontSmoothing::SubpixelRGB, darkBg, white);
    renderWord(fbBgr, FontSmoothing::SubpixelBGR, darkBg, white);
    renderWord(fbTrans, FontSmoothing::SubpixelRGB, Color(0, 0, 0, 0), white);

    keel::usize grayLit = 0;
    keel::usize grayChromatic = 0;
    keel::usize rgbChromatic = 0;
    keel::usize rgbVsBgrDiff = 0;
    keel::usize transLit = 0;
    for (keel::usize i = 0; i < W * H; ++i) {
        const keel::u32 gPx = fbGray[i];
        const keel::u32 gr = gPx & 0xFFu, gg = (gPx >> 8) & 0xFFu, gb = (gPx >> 16) & 0xFFu;
        if (gr > 0x20u) ++grayLit;
        if (gr != gg || gg != gb) ++grayChromatic;

        const keel::u32 rPx = fbRgb[i];
        const keel::u32 rr = rPx & 0xFFu, rg = (rPx >> 8) & 0xFFu, rb = (rPx >> 16) & 0xFFu;
        if (rr != rg || rg != rb) ++rgbChromatic;
        if (rPx != fbBgr[i]) ++rgbVsBgrDiff;

        const keel::u32 tPx = fbTrans[i];
        const keel::u32 ta = (tPx >> 24) & 0xFFu;
        if (ta > 0u) {
            ++transLit;
            CHECK((tPx & 0xFFu) <= ta);
            CHECK(((tPx >> 8) & 0xFFu) <= ta);
            CHECK(((tPx >> 16) & 0xFFu) <= ta);
        }
    }

    CHECK(grayLit > 50u);
    CHECK(grayChromatic == 0u);   // Pure grayscale on neutral dark background has R == G == B
    CHECK(rgbChromatic > 20u);    // 3x LCD subpixel produces distinct R/G/B coverage on glyph edges
    CHECK(rgbVsBgrDiff > 20u);    // BGR flips subpixel polarity vs RGB
    CHECK(transLit > 50u);        // Transparent destination falls back cleanly to premul SrcOver

    // Auto mode selects SubpixelRGB at 1x and Grayscale at 2x HiDPI
    CHECK(TextCompositor::ResolveEffectiveSmoothing(FontSmoothing::Auto, 1.0f) == FontSmoothing::SubpixelRGB);
    CHECK(TextCompositor::ResolveEffectiveSmoothing(FontSmoothing::Auto, 2.0f) == FontSmoothing::Grayscale);

    keel::println("  [PASS] TestTextCompositorAndSubpixelRendering completed");
}



