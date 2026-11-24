#include <cstring>
#include "guikit/core.hpp"
#include "guikit/core/scrollbar.hpp"
#include "guikit/core/style_painter.hpp"
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

static void SleepMicroseconds(keel::u64 us) noexcept {
    keel::sleep_us(us);
}

void TestGeometryAndTypes() {
    using namespace guikit;
    using namespace guikit::literals;

    // Vec2 tests
    Vec2 v1{10.0f, 20.0f};
    Vec2 v2{5.0f, 15.0f};
    CHECK(v1 + v2 == Vec2(15.0f, 35.0f));
    CHECK(v1 - v2 == Vec2(5.0f, 5.0f));
    CHECK(v1 * 2.0f == Vec2(20.0f, 40.0f));
    CHECK(v1 / 2.0f == Vec2(5.0f, 10.0f));

    // Rect tests
    Rect r{10.0f, 10.0f, 100.0f, 50.0f};
    CHECK(r.Contains({10.0f, 10.0f}));
    CHECK(r.Contains({60.0f, 35.0f}));
    CHECK(r.Contains({110.0f, 60.0f}));
    CHECK(!r.Contains({5.0f, 10.0f}));
    CHECK(!r.Contains({115.0f, 35.0f}));
    CHECK(r.Center() == Vec2(60.0f, 35.0f));

    Rect inset = r.Inset(5.0f);
    CHECK(inset.x == 15.0f && inset.y == 15.0f && inset.w == 90.0f && inset.h == 40.0f);

    // Color tests
    Color cHex = Color::Hex(0xFF8000FF);
    CHECK(cHex.r == 0xFF && cHex.g == 0x80 && cHex.b == 0x00 && cHex.a == 0xFF);

    Color cLerp = Color::Lerp(Color(0, 0, 0, 0), Color(100, 200, 50, 100), 0.5f);
    CHECK(cLerp.r == 50 && cLerp.g == 100 && cLerp.b == 25 && cLerp.a == 50);

    // Spacing and DP literals
    const auto theme = UiTheme::Dark();
    CHECK(theme.GetGap(UiSize::Sm) == 6.0f);
    CHECK(theme.GetGap(UiSize::Lg) == 12.0f);
    CHECK(theme.GetRadius(UiSize::Md) == 4.5f);
    Dp dpVal = 16.0_dp;
    CHECK(dpVal.ToPixels(2.0f) == 32.0f);

    // Padding token and resolution tests
    static_assert(sizeof(Padding) == 16, "Padding must remain 16 bytes for Node packing");
    Padding pUniform{12.0f};
    CHECK(pUniform.left == 12.0f && pUniform.top == 12.0f && pUniform.right == 12.0f && pUniform.bottom == 12.0f);
    CHECK(!pUniform.IsToken());

    Padding pToken{UiSize::Sm};
    CHECK(pToken.IsToken());
    CHECK(pToken.GetToken() == UiSize::Sm);

    Padding pResolved = theme.ResolvePadding(pToken);
    CHECK(pResolved == theme.GetPadding(UiSize::Sm));

    Padding pEmptyResolved = theme.ResolvePadding(keel::kNullOpt);
    CHECK((pEmptyResolved == Padding{0.0f, 0.0f, 0.0f, 0.0f}));

    Padding pCardResolved = theme.ResolvePadding(keel::kNullOpt, UiSize::Md);
    CHECK(pCardResolved == theme.GetPadding(UiSize::Md));

    // Typography line height test
    CHECK(theme.GetLineHeight(14.0f) == 14.0f * kDefaultLineHeightMultiplier);
}

void TestIdGeneration() {
    using namespace guikit;

    // Distinct child indices at same site must yield distinct IDs
    Id id0 = MakeId(0);
    Id id1 = MakeId(1);
    Id id2 = MakeId(2);

    CHECK(id0.IsValid());
    CHECK(id1.IsValid());
    CHECK(id0 != id1);
    CHECK(id1 != id2);

    // Distinct parents must yield distinct IDs
    Id parentA{100};
    Id parentB{200};
    Id childA = MakeId(0, keel::SourceLocation::current(), parentA);
    Id childB = MakeId(0, keel::SourceLocation::current(), parentB);
    CHECK(childA != childB);

    // Hash set compatibility
    keel::HashSet<Id> idSet;
    idSet.insert(id0);
    idSet.insert(id1);
    idSet.insert(id2);
    CHECK(idSet.size() == 3);
    CHECK(idSet.contains(id1));
}

void TestBumpAllocator() {
    using namespace guikit;

    keel::Arena allocator(1024); // 1 KB chunks for testing

    struct SampleData {
        keel::i32 a;
        keel::f64 b;
        keel::StringRef tag;
    };

    SampleData* obj1 = allocator.create<SampleData>(10, 3.14, "hello");
    CHECK(obj1 != nullptr);
    CHECK(obj1->a == 10);
    CHECK(reinterpret_cast<keel::usize>(obj1) % alignof(SampleData) == 0);

    SampleData* obj2 = allocator.create<SampleData>(20, 6.28, "world");
    CHECK(obj2 != nullptr);
    CHECK(obj2->a == 20);
    CHECK(obj1 != obj2);

    // Allocate enough objects to cross chunk boundary
    for (keel::i32 i = 0; i < 100; ++i) {
        SampleData* obj = allocator.create<SampleData>(i, keel::as<keel::f64>(i), "loop");
        CHECK(obj != nullptr);
        CHECK(obj->a == i);
    }

    CHECK(allocator.used() > 1024);

    // Reset should zero out offset without crashing or leaking
    allocator.reset();
    CHECK(allocator.used() == 0);

    // Reallocation after reset
    SampleData* objAfterReset = allocator.create<SampleData>(99, 1.0, "reset");
    CHECK(objAfterReset != nullptr);
    CHECK(objAfterReset->a == 99);

    // Oversized allocation when advancing into a previously-allocated small chunk must resize safely
    allocator.reset();
    void* fillChunk0 = allocator.alloc(900, 8);
    CHECK(fillChunk0 != nullptr);
    void* oversizedInChunk1 = allocator.alloc(4096, 16);
    CHECK(oversizedInChunk1 != nullptr);
    keel::mem_set(oversizedInChunk1, 0xAB, 4096);

    // Non-trivially destructible objects & arrays must invoke ~T() in LIFO order on reset() and ~Arena()
    keel::Array<keel::i32> dtorOrder;
    struct TrackedNonTrivial {
        keel::Array<keel::i32>* log{nullptr};
        keel::i32 id{0};
        keel::SmallArray<keel::i32, 4> ownedHeap{};

        TrackedNonTrivial() {
            ownedHeap.push_back(1);
            ownedHeap.push_back(2);
            ownedHeap.push_back(3);
            ownedHeap.push_back(4);
        }
        TrackedNonTrivial(keel::Array<keel::i32>* l, keel::i32 i) : TrackedNonTrivial() {
            log = l;
            id = i;
        }
        ~TrackedNonTrivial() noexcept {
            if (log) {
                log->push_back(id);
            }
        }
    };
    static_assert(!keel::IsTriviallyDestructible<TrackedNonTrivial>);

    {
        keel::Arena scopedAlloc(512);
        auto* n1 = scopedAlloc.create<TrackedNonTrivial>(&dtorOrder, 1);
        auto* n2 = scopedAlloc.create<TrackedNonTrivial>(&dtorOrder, 2);
        auto* arr = scopedAlloc.create_array<TrackedNonTrivial>(3);
        for (keel::i32 i = 0; i < 3; ++i) {
            arr[i].log = &dtorOrder;
            arr[i].id = 10 + i;
        }
        CHECK(n1->ownedHeap.size() == 4);
        CHECK(n2->ownedHeap.size() == 4);
        CHECK(dtorOrder.empty());

        scopedAlloc.reset();
        // LIFO destruction: arr[2] (12), arr[1] (11), arr[0] (10), then n2 (2), then n1 (1)
        static constexpr keel::StaticArray<keel::i32, 5> expectedAfterReset = {{12, 11, 10, 2, 1}};
        CHECK(dtorOrder.as_ref() == expectedAfterReset.as_ref());

        dtorOrder.clear();
        auto* n3 = scopedAlloc.create<TrackedNonTrivial>(&dtorOrder, 42);
        CHECK(n3 != nullptr);
    }
    // ~Arena() destroys remaining objects
    CHECK(dtorOrder.size() == 1 && dtorOrder[0] == 42);
}

void TestWidgetResponse() {
    using namespace guikit;

    WidgetResponse res;
    res.id = MakeId(0);
    res.rect = {10.0f, 20.0f, 80.0f, 30.0f};

    CHECK(!res); // clicked is false
    res.clicked = true;
    CHECK(bool(res)); // implicit bool operator true when clicked
}

void TestFrameMetrics() {
    using namespace guikit;

    FrameMetrics metrics;
    // Initial status string
    CHECK(!metrics.GetStatusString().empty());

    // Simulate 5 frames
    for (keel::i32 i = 0; i < 5; ++i) {
        metrics.BeginFrame();
        // Simulate small CPU work
        SleepMicroseconds(2000);
        metrics.MarkCpuDone();

        // Simulate small GPU work
        metrics.MarkGpuStart();
        SleepMicroseconds(500);
        metrics.MarkGpuDone();

        metrics.EndFrame(60.0f);
    }

    CHECK(metrics.GetFps() >= 0.0f);
    CHECK(metrics.GetCpuPercent() >= 0.0f && metrics.GetCpuPercent() <= 100.0f);
    CHECK(metrics.GetCpuTimeMs() >= 0.0f);
    CHECK(metrics.GetGpuTimeMs() >= 0.0f);
    CHECK(metrics.GetGpuBudgetPercent() >= 0.0f && metrics.GetGpuBudgetPercent() <= 100.0f);
    CHECK(metrics.GetRamUsageBytes() > 0);
    CHECK(metrics.GetRamUsageMb() > 0.0f);

    keel::StringRef status = metrics.GetStatusString();
    CHECK(!status.empty());
    CHECK(status.find("FPS") != keel::StringRef::npos);
    CHECK(status.find("CPU:") != keel::StringRef::npos);
    CHECK(status.find("GPU:") != keel::StringRef::npos);
    CHECK(status.find("budget") != keel::StringRef::npos);
    CHECK(status.find("RAM:") != keel::StringRef::npos);

    // Simulate active presented frames followed by >250ms reactive sleep (idle heartbeat)
    for (keel::i32 i = 0; i < 4; ++i) {
        metrics.BeginFrame();
        SleepMicroseconds(1000);
        metrics.MarkCpuDone();
        metrics.MarkGpuStart();
        metrics.MarkGpuDone();
        metrics.MarkFramePresented();
        metrics.EndFrame(60.0f);
    }
    CHECK(!metrics.IsIdle());

    // Sleep >250ms to simulate reactive idle heartbeat wait
    SleepMicroseconds(260000);
    metrics.BeginFrame();
    // Immediately at BeginFrame() (before UI StatusBar rendering), metrics must report Idle
    CHECK(metrics.IsIdle());
    CHECK(metrics.GetFpsString() == "0 FPS (Idle)");
    CHECK(metrics.GetCpuTimeMs() == 0.0f);
    CHECK(metrics.GetGpuTimeMs() == 0.0f);

    // Even if StatusBar repaints "0 FPS (Idle)" on this heartbeat frame and calls MarkFramePresented(),
    // isolated heartbeat presentation must NOT kick FrameMetrics out of idle
    metrics.MarkCpuDone();
    metrics.MarkGpuStart();
    metrics.MarkGpuDone();
    metrics.MarkFramePresented();
    metrics.EndFrame(60.0f);
    CHECK(metrics.IsIdle());
    CHECK(metrics.GetFpsString() == "0 FPS (Idle)");
    CHECK(metrics.GetCpuTimeMs() == 0.0f);
}

void TestNodeFootprint() {
    using namespace guikit;

    // Verify Node has been decoupled from widget options and its size is compact
    keel::println("[INFO] sizeof(Node) = {} bytes", sizeof(Node));
    static_assert(sizeof(Node) <= 256); // Compact <=256 bytes with 4 polymorphic delegates + aspect-ratio/minHeight/2D scroll state

    // Verify generic payload casting
    Node n;
    keel::i32 dummyPayload = 42;
    n.payload = &dummyPayload;
    CHECK(*n.As<keel::i32>() == 42);

    const Node& constN = n;
    CHECK(*constN.As<keel::i32>() == 42);
}

void TestCarouselMetricsTokens() {
    using namespace guikit;

    UiTheme theme = UiTheme::Dark();

    // 1. Standard metrics baseline checks
    const auto& md = theme.GetCarousel(UiSize::Md);
    CHECK(md.arrowButtonSize == 19.5f);
    CHECK(md.iconSize == 10.5f);
    CHECK(md.cornerRadius == 4.5f);
    CHECK(md.margin == 3.0f);
    CHECK(md.gap == 4.5f);
    CHECK(md.gutter == 27.0f);
    CHECK(md.gutter == md.margin + md.arrowButtonSize + md.gap);
    CHECK(md.scrollStep == 82.5f);

    const auto& xs = theme.GetCarousel(UiSize::Xs);
    CHECK(xs.arrowButtonSize == 13.5f);
    CHECK(xs.iconSize == 7.5f);
    CHECK(xs.gutter == 18.0f);
    CHECK(xs.gutter == xs.margin + xs.arrowButtonSize + xs.gap);

    const auto& sm = theme.GetCarousel(UiSize::Sm);
    CHECK(sm.arrowButtonSize == 16.5f);
    CHECK(sm.iconSize == 9.0f);
    CHECK(sm.gutter == 22.5f);

    const auto& lg = theme.GetCarousel(UiSize::Lg);
    CHECK(lg.arrowButtonSize == 24.0f);
    CHECK(lg.iconSize == 12.0f);
    CHECK(lg.gutter == 33.75f);

    const auto& xl = theme.GetCarousel(UiSize::Xl);
    CHECK(xl.arrowButtonSize == 30.0f);
    CHECK(xl.iconSize == 15.0f);
    CHECK(xl.gutter == 42.0f);

    // Fallback to Md on None
    const auto& noneSize = theme.GetCarousel(UiSize::None);
    CHECK(noneSize.arrowButtonSize == md.arrowButtonSize);

    // 2. Mutable accessor check
    theme.MutableCarousel(UiSize::Md).scrollStep = 150.0f;
    CHECK(theme.GetCarousel(UiSize::Md).scrollStep == 150.0f);

    // 3. Custom UiSize (Dp & Factor) scaling for Carousel metrics
    const CarouselMetrics mdAfter = theme.GetCarousel(UiSize::Md);
    const CarouselMetrics customDp = theme.GetCarousel(UiSize::Dp(45.0f));
    CHECK(customDp.arrowButtonSize > mdAfter.arrowButtonSize);
    CHECK(customDp.scrollStep > mdAfter.scrollStep);
    const CarouselMetrics customFactor = theme.GetCarousel(UiSize::Factor(2.0f));
    CHECK(keel::abs(customFactor.arrowButtonSize - mdAfter.arrowButtonSize * 2.0f) < 0.01f);
    CHECK(keel::abs(customFactor.scrollStep - mdAfter.scrollStep * 2.0f) < 0.01f);
}

void TestSliderMetricsTokens() {
    using namespace guikit;

    UiTheme theme = UiTheme::Dark();

    // 1. Standard metrics baseline checks
    const auto& md = theme.GetSlider(UiSize::Md);
    CHECK(md.defaultHeight == 19.5f);
    CHECK(md.trackHeight == 3.75f);
    CHECK(md.thumbSize == 13.5f);
    CHECK(md.cornerRadius == 6.75f);
    CHECK(md.tickSize == 3.0f);

    const auto& xs = theme.GetSlider(UiSize::Xs);
    CHECK(xs.defaultHeight == 13.5f);
    CHECK(xs.thumbSize == 9.0f);

    // 2. Mutable accessor check
    theme.MutableSlider(UiSize::Md).trackHeight = 7.0f;
    CHECK(theme.GetSlider(UiSize::Md).trackHeight == 7.0f);

    // 3. Custom UiSize (Dp & Factor) scaling for Slider metrics
    static_assert(sizeof(UiSize) == 4, "UiSize must stay 4 bytes");
    const UiSize implicitDp = 45.0f;
    CHECK(implicitDp.IsCustom());
    const SliderMetrics mdAfter = theme.GetSlider(UiSize::Md);
    const SliderMetrics factor2 = theme.GetSlider(UiSize::Factor(2.0f));
    CHECK(keel::abs(factor2.defaultHeight - mdAfter.defaultHeight * 2.0f) < 0.01f);
    CHECK(keel::abs(factor2.thumbSize - mdAfter.thumbSize * 2.0f) < 0.01f);
    CHECK(keel::abs(factor2.trackHeight - mdAfter.trackHeight * 2.0f) < 0.01f);
}

void TestHeadingLevelMetricsTokens() {
    using namespace guikit;

    UiTheme theme = UiTheme::Dark();

    // 1. Standard metrics across H1..H6 must monotonically decrease
    const auto& h1 = theme.GetHeading(HeadingLevel::H1);
    const auto& h2 = theme.GetHeading(HeadingLevel::H2);
    const auto& h3 = theme.GetHeading(HeadingLevel::H3);
    const auto& h4 = theme.GetHeading(HeadingLevel::H4);
    const auto& h5 = theme.GetHeading(HeadingLevel::H5);
    const auto& h6 = theme.GetHeading(HeadingLevel::H6);

    CHECK(h1.titleFontSize > h2.titleFontSize);
    CHECK(h2.titleFontSize > h3.titleFontSize);
    CHECK(h3.titleFontSize > h4.titleFontSize);
    CHECK(h4.titleFontSize > h5.titleFontSize);
    CHECK(h5.titleFontSize > h6.titleFontSize);

    CHECK(h1.subtitleFontSize > h6.subtitleFontSize);
    CHECK(h1.badgeFontSize > h6.badgeFontSize);
    CHECK(h1.iconSize > h6.iconSize);
    CHECK(h1.gap > h6.gap);

    // Default HeadingOptions::level is H3, and ComponentMetrics carries UiSize badge/gap tokens
    constexpr HeadingOptions defaultHeadOpts{};
    static_assert(defaultHeadOpts.level == HeadingLevel::H3);
    CHECK(theme.GetMetrics(UiSize::Xl).badgeFontSize > theme.GetMetrics(UiSize::Xs).badgeFontSize);
    CHECK(theme.GetMetrics(UiSize::Xl).itemGap > theme.GetMetrics(UiSize::Xs).itemGap);

    // 2. Mutable accessor check
    theme.GetHeading(HeadingLevel::H1).titleFontSize = 30.0f;
    CHECK(theme.GetHeading(HeadingLevel::H1).titleFontSize == 30.0f);

    // 3. HeadingDivider value-type semantics & deterministic hashing
    constexpr HeadingDivider divNone{};
    constexpr HeadingDivider divBottom = HeadingSeparator::Bottom;
    constexpr HeadingDivider divCustom = HeadingDivider::Bottom(StateColor::Primary(), 2.0f);
    constexpr HeadingDivider divTrailing = HeadingDivider::Trailing(StateColor::Warning(), 1.5f);
    static_assert(!divNone.IsEnabled());
    static_assert(divBottom.IsEnabled());
    static_assert(divBottom == HeadingSeparator::Bottom);
    static_assert(divCustom.IsEnabled());
    static_assert(divCustom != divBottom);
    CHECK(detail::HashVal(divNone) != detail::HashVal(divBottom));
    CHECK(detail::HashVal(divBottom) != detail::HashVal(divCustom));
    CHECK(detail::HashVal(divCustom) != detail::HashVal(divTrailing));
}

void TestComputeBoxFit() {
    using namespace guikit;

    const Rect box{10.0f, 20.0f, 400.0f, 200.0f}; // 2:1 container
    const Vec2 srcWide{800.0f, 200.0f};           // 4:1 source

    // 1. Contain: scales by min(400/800=0.5, 200/200=1.0) = 0.5 -> 400x100 centered vertically
    const Rect rContain = ComputeBoxFit(ObjectFit::Contain, srcWide, box);
    CHECK(rContain.x == 10.0f && rContain.y == 70.0f);
    CHECK(rContain.w == 400.0f && rContain.h == 100.0f);

    // 2. Cover: scales by max(0.5, 1.0) = 1.0 -> 800x200 centered horizontally
    const Rect rCover = ComputeBoxFit(ObjectFit::Cover, srcWide, box);
    CHECK(rCover.x == -190.0f && rCover.y == 20.0f);
    CHECK(rCover.w == 800.0f && rCover.h == 200.0f);

    // 3. Fill: stretches directly to box
    const Rect rFill = ComputeBoxFit(ObjectFit::Fill, srcWide, box);
    CHECK(rFill == box);

    // 4. ScaleDown with smaller source (100x50): keeps 1:1 natural size (never upscales)
    const Vec2 srcSmall{100.0f, 50.0f};
    const Rect rScaleDown = ComputeBoxFit(ObjectFit::ScaleDown, srcSmall, box);
    CHECK(rScaleDown.w == 100.0f && rScaleDown.h == 50.0f);
    CHECK(rScaleDown.x == 160.0f && rScaleDown.y == 95.0f);

    // 5. Original with End/End alignment (1.0f, 1.0f)
    const Rect rOrigEnd = ComputeBoxFit(ObjectFit::Original, srcSmall, box, 1.0f, 1.0f);
    CHECK(rOrigEnd.x == 310.0f && rOrigEnd.y == 170.0f);
    CHECK(rOrigEnd.w == 100.0f && rOrigEnd.h == 50.0f);
}

void TestTextWrapForceSplitLastWord() {
    using namespace guikit;

    // 1. Single long word at end of string wider than maxWidth
    {
        const keel::StringRef text = "SizeMode::Stretch.";
        const auto lines = TextUtil::WrapText(text, 60.0f, 14.0f, nullptr);
        CHECK(lines.size() >= 2);
        keel::String reconstructed;
        for (keel::StringRef ln : lines) {
            reconstructed.append(ln);
        }
        CHECK(reconstructed == text);
    }

    // 2. Multi-word string where the final word requires force-splitting across narrow widths
    {
        const keel::StringRef subtitle =
            "Each button automatically receives equal available width via widthMode = SizeMode::Stretch.";
        for (keel::f32 maxW = 4.0f; maxW <= 250.0f; maxW += 7.0f) {
            const auto lines = TextUtil::WrapText(subtitle, maxW, 13.0f, nullptr);
            CHECK(!lines.empty());
        }
    }

    // 3. UTF-8 multi-byte string force-split at the end of string
    {
        const keel::StringRef utf8Text = "Короткий текст СверхдлинноеСловоВКонцеСтроки";
        for (keel::f32 maxW = 5.0f; maxW <= 200.0f; maxW += 11.0f) {
            const auto lines = TextUtil::WrapText(utf8Text, maxW, 14.0f, nullptr);
            CHECK(!lines.empty());
        }
    }
}

void TestStandaloneCoreModuleZeroDependencies() {
    using namespace guikit;

    class NullCanvas final : public ICanvas {
    public:
        void Begin(keel::u32, keel::u32, keel::f32 scale = 1.0f) override { m_scale = scale; }
        void End() override {}
        [[nodiscard]] keel::f32 GetScale() const noexcept override { return m_scale; }
        void SetScale(keel::f32 scale) override { m_scale = scale; }
        void PushTransform(const Transform2D&) override {}
        void PopTransform() override {}
        [[nodiscard]] keel::StringRef GetBackendName() const noexcept override { return "NullCanvas (Zero-Dep)"; }
        void Clear(Color, keel::Optional<Rect> = keel::kNullOpt) override {}
        void PushClip(Rect) override {}
        void PopClip() override {}
        void DrawRect(Rect, Color) override { ++drawCount; }
        void DrawRoundRect(Rect, keel::f32, Color, Color = Color(0, 0, 0, 0), keel::f32 = 0.0f) override { ++drawCount; }
        Vec2 MeasureText(keel::StringRef text, keel::f32 fontSize) override {
            return {keel::as<keel::f32>(text.size()) * fontSize * 0.55f, fontSize * 1.2f};
        }
        [[nodiscard]] keel::f32 GetLineHeight(keel::f32 fontSize) override { return fontSize * 1.25f; }
        void DrawText(keel::StringRef, Vec2, keel::f32, Color) override { ++drawCount; }
        void DrawTextBox(keel::StringRef, Rect, keel::f32, Color, keel::f32 = 0.0f, keel::f32 = 0.5f) override { ++drawCount; }
        void DrawLine(Vec2, Vec2, Color, keel::f32 = 1.0f) override { ++drawCount; }

        keel::f32 m_scale{1.0f};
        keel::usize drawCount{0};
    };

    Context ui;
    NullCanvas canvas;

    keel::f32 sliderVal = 0.5f;
    bool switchVal = true;
    keel::i32 selectedRow = 0;

    static constexpr keel::StaticArray<TableColumn, 2> cols = {{
        {.title = "Key",   .width = 120.0f},
        {.title = "Value", .flex = 1.0f}
    }};

    ui.BeginFrame({640.0f, 480.0f});
    if (auto card = ui.Card({.padding = Padding::All(12.0f)})) {
        ui.Heading({.label = "Standalone Core Module"});
        ui.Button({.text = "Action"});
        ui.Slider(sliderVal, 0.0f, 1.0f);
        if (auto el = ui.Element({.label = "Toggle"})) {
            ui.Switch(switchVal);
        }
        if (auto table = ui.Table(cols, {.totalRows = 10, .selectedRow = &selectedRow, .height = 180.0f})) {
            for (keel::usize r = table.FirstVisibleRow(); r < table.EndVisibleRow(); ++r) {
                if (auto row = table.Row(r)) {
                    ui.Text(ui.Format("K-{:02}", r));
                    ui.Element({.badge = {"OK", UiVariant::Success}, .width = SizeMode::Fit});
                }
            }
        }
    }
    ui.EndFrame(canvas);

    CHECK(canvas.drawCount > 0);
}

void TestEmbeddedAndRuntimeAssets() {
    using namespace guikit;

    // 1. Verify build-time embedded assets (guikit-icon-inter850-1024.png + Roboto-Regular.ttf)
    CHECK(Assets::Exists("assets/guikit-icon-inter850-1024.png"));
    CHECK(Assets::Exists("../assets/guikit-icon-inter850-1024.png"));
    CHECK(Assets::Exists("guikit-icon-inter850-1024.png"));
    CHECK(Assets::Exists("assets\\guikit-icon-inter850-1024.png"));
    CHECK(Assets::Exists("assets/fonts/Roboto-Regular.ttf"));
    CHECK(!Assets::Exists("assets/images/architecture.svg"));
    CHECK(!Assets::Exists("assets/images/circuit.svg"));
    CHECK(!Assets::ListPaths().empty());

    const auto iconPng = Assets::Find("assets/guikit-icon-inter850-1024.png");
    CHECK(iconPng.has_value());
    if (iconPng.has_value()) {
        CHECK(!iconPng->Empty());
        CHECK(iconPng->mimeType == "png");
        CHECK(iconPng->size > 1000u);
        // PNG magic signature: 89 50 4E 47
        CHECK(iconPng->data[0] == 0x89);
        CHECK(iconPng->data[1] == 0x50);
        CHECK(iconPng->data[2] == 0x4E);
        CHECK(iconPng->data[3] == 0x47);
    }

    // 2. Verify runtime asset registration & override cleanup
    keel::SmallArray<keel::u8, 8> customSvg;
    for (char ch : keel::StringRef("<svg/>")) {
        customSvg.push_back(keel::as<keel::u8>(ch));
    }
    Assets::RegisterOwned("assets/runtime/custom.svg", customSvg);
    CHECK(Assets::Exists("assets/runtime/custom.svg"));
    const auto foundCustom = Assets::Find("runtime/custom.svg");
    CHECK(foundCustom.has_value());
    if (foundCustom.has_value()) {
        CHECK(foundCustom->mimeType == "svg");
        CHECK(foundCustom->AsStringView() == "<svg/>");
    }
    CHECK(Assets::Unregister("assets/runtime/custom.svg"));
    CHECK(!Assets::Exists("assets/runtime/custom.svg"));
}

void TestUiVectorAndArenaVector() {
    using namespace guikit;

    static_assert(keel::IsTriviallyDestructible<keel::ArenaArray<keel::i32>>);
    static_assert(keel::IsTriviallyDestructible<keel::ArenaArray<Rect>>);
    static_assert(!keel::IsTriviallyDestructible<keel::ArenaArray<keel::String>>);
    static_assert(keel::IsTriviallyDestructible<ScenePayload>);

    // 1. keel::SmallArray inline SBO storage & no self-referential pointer on clone/move
    {
        keel::SmallArray<keel::i32, 4> vec;
        CHECK(vec.empty());
        CHECK(vec.is_inline());
        CHECK(vec.capacity() == 4);

        vec.push_back(10);
        vec.push_back(20);
        vec.push_back(30);
        vec.push_back(40);
        CHECK(vec.size() == 4);
        CHECK(vec.is_inline());

        // Clone while inline must have its own independent inline storage
        keel::SmallArray<keel::i32, 4> copyVec = vec.clone();
        CHECK(copyVec.is_inline());
        CHECK(copyVec.data() != vec.data());
        copyVec[0] = 99;
        CHECK(vec[0] == 10);

        // Self-aliasing push_back across inline -> heap boundary
        vec.push_back(vec[0]);
        CHECK(vec.size() == 5);
        CHECK(!vec.is_inline());
        CHECK(vec[0] == 10);
        CHECK(vec[4] == 10);

        // Move from heap-allocated SmallArray steals heap buffer and resets source to inline
        const keel::i32* heapPtr = vec.data();
        keel::SmallArray<keel::i32, 4> movedVec = keel::move(vec);
        CHECK(!movedVec.is_inline());
        CHECK(movedVec.data() == heapPtr);
        CHECK(movedVec.size() == 5);
        CHECK(vec.empty());
        CHECK(vec.is_inline());

        // ArrayRef view
        auto sumSpan = [](keel::ArrayRef<keel::i32> sp) {
            keel::i32 s = 0;
            for (keel::i32 v : sp) s += v;
            return s;
        };
        CHECK(sumSpan(movedVec) == (10 + 20 + 30 + 40 + 10));
    }

    // 2. Non-trivial element lifetime in keel::SmallArray
    {
        keel::i32 liveCount = 0;
        struct TrackedItem {
            keel::i32* counter{nullptr};
            keel::i32 value{0};
            TrackedItem() = default;
            TrackedItem(keel::i32* c, keel::i32 v) : counter(c), value(v) {
                if (counter) ++(*counter);
            }
            TrackedItem(const TrackedItem& o) : counter(o.counter), value(o.value) {
                if (counter) ++(*counter);
            }
            TrackedItem(TrackedItem&& o) noexcept : counter(o.counter), value(o.value) {
                o.counter = nullptr;
            }
            TrackedItem& operator=(const TrackedItem& o) {
                if (this != &o) {
                    if (counter) --(*counter);
                    counter = o.counter;
                    value = o.value;
                    if (counter) ++(*counter);
                }
                return *this;
            }
            TrackedItem& operator=(TrackedItem&& o) noexcept {
                if (this != &o) {
                    if (counter) --(*counter);
                    counter = o.counter;
                    value = o.value;
                    o.counter = nullptr;
                }
                return *this;
            }
            ~TrackedItem() noexcept {
                if (counter) --(*counter);
            }
        };

        {
            keel::SmallArray<TrackedItem, 2> items;
            items.emplace_back(&liveCount, 1);
            items.emplace_back(&liveCount, 2);
            CHECK(items.is_inline());
            CHECK(liveCount == 2);

            // Spill to heap + self-aliasing copy
            items.push_back(items[0]);
            CHECK(!items.is_inline());
            CHECK(liveCount == 3);
            CHECK(items[2].value == 1);

            items.remove_at(1);
            CHECK(items.size() == 2);
            CHECK(liveCount == 2);
        }
        CHECK(liveCount == 0);
    }

    // 3. keel::ArenaArray in-place growth via Arena::try_extend
    {
        keel::Arena arena(4096);
        keel::ArenaArray<keel::i32> arenaVec(arena, 8);
        const keel::i32* initialPtr = arenaVec.data();
        CHECK(initialPtr != nullptr);
        CHECK(arenaVec.capacity() == 8);

        // Push 64 elements while nothing else allocates from `arena`:
        // try_extend extends the buffer in-place without moving `data()`!
        for (keel::i32 i = 0; i < 64; ++i) {
            arenaVec.push_back(i * 3);
        }
        CHECK(arenaVec.size() == 64);
        CHECK(arenaVec.data() == initialPtr);
        keel::ArrayRef<keel::i32> arenaRef = arenaVec;
        CHECK(arenaRef.size() == 64);
        for (keel::i32 i = 0; i < 64; ++i) {
            CHECK(arenaVec[i] == i * 3);
        }

        // Allocate something else on the arena so tip moves, then grow arenaVec again
        keel::i32* separator = arena.create<keel::i32>(777);
        CHECK(*separator == 777);
        for (keel::i32 i = 64; i < 150; ++i) {
            arenaVec.push_back(i * 3);
        }
        CHECK(arenaVec.size() == 150);
        CHECK(arenaVec.data() != initialPtr);
        CHECK(*separator == 777);
        CHECK(arenaVec[0] == 0);
        CHECK(arenaVec[63] == 63 * 3);
        CHECK(arenaVec[149] == 149 * 3);
    }
}

void TestMainWindowHeadlessBackend() {
    using namespace guikit;

    class HeadlessCanvas final : public ICanvas {
    public:
        void Begin(keel::u32, keel::u32, keel::f32 scale = 1.0f) override {
            m_scale = scale;
            m_passes.clear();
        }
        void End() override {}
        [[nodiscard]] keel::f32 GetScale() const noexcept override { return m_scale; }
        void SetScale(keel::f32 scale) override { m_scale = scale; }
        void PushTransform(const Transform2D&) override {}
        void PopTransform() override {}
        [[nodiscard]] keel::StringRef GetBackendName() const noexcept override { return "HeadlessCanvas"; }
        void Clear(Color, keel::Optional<Rect> = keel::kNullOpt) override {}
        void PushClip(Rect) override {}
        void PopClip() override {}
        void DrawRect(Rect, Color) override { ++drawCount; }
        void DrawRoundRect(Rect, keel::f32, Color, Color = Color(0, 0, 0, 0), keel::f32 = 0.0f) override { ++drawCount; }
        Vec2 MeasureText(keel::StringRef text, keel::f32 fontSize) override {
            return {keel::as<keel::f32>(text.size()) * fontSize * 0.55f, fontSize * 1.2f};
        }
        [[nodiscard]] keel::f32 GetLineHeight(keel::f32 fontSize) override { return fontSize * 1.25f; }
        void DrawText(keel::StringRef, Vec2, keel::f32, Color) override { ++drawCount; }
        void DrawTextBox(keel::StringRef, Rect, keel::f32, Color, keel::f32 = 0.0f, keel::f32 = 0.5f) override { ++drawCount; }
        void DrawLine(Vec2, Vec2, Color, keel::f32 = 1.0f) override { ++drawCount; }
        bool DrawGpuShaderPass(const GpuShaderPassDesc& desc, bool = true) override {
            m_passes.push_back(desc);
            ++drawCount;
            return true;
        }
        [[nodiscard]] keel::ArrayRef<GpuShaderPassDesc> GetGpuShaderPasses() const noexcept override {
            return m_passes;
        }

        keel::f32 m_scale{1.0f};
        keel::usize drawCount{0};
        keel::Array<GpuShaderPassDesc> m_passes;
    };

    class MockWindowBackend final : public IWindowBackend {
    public:
        bool Initialize(const MainWindowConfig& cfg, Context&, WindowHostCallbacks) override {
            m_cfg = cfg;
            initialized = true;
            return true;
        }
        void Shutdown() noexcept override {
            shutdownCalled = true;
        }
        bool PumpEvents(Context&, bool, keel::i32) override {
            return true;
        }
        ICanvas& GetCanvas() noexcept override { return m_canvas; }
        const ICanvas& GetCanvas() const noexcept override { return m_canvas; }
        void PrepareFrameTarget(Context&) override {}
        void PresentFrame(const Context&, FrameMetrics& metrics, bool) override {
            metrics.MarkGpuStart();
            metrics.MarkGpuDone();
            ++framesPresented;
        }
        keel::i32 GetWidth() const noexcept override { return m_cfg.width; }
        keel::i32 GetHeight() const noexcept override { return m_cfg.height; }
        keel::f32 GetDpiScale() const noexcept override { return m_dpi; }
        keel::f32 GetDisplayPixelsPerMm() const noexcept override { return 4.0f; }
        bool IsMinimized() const noexcept override { return false; }
        bool IsLiveResizeFrame() const noexcept override { return false; }
        bool IsVsyncEnabled() const noexcept override { return m_vsync; }
        void SetVsyncEnabled(bool enabled) noexcept override { m_vsync = enabled; }
        bool IsGpuShadersEnabled() const noexcept override { return m_gpuShaders; }
        bool IsGpuShadersAvailable() const noexcept override { return true; }
        void SetGpuShadersEnabled(bool enabled) noexcept override { m_gpuShaders = enabled; }
        void UpdateSystemCursor(CursorType cursor) noexcept override { lastCursor = cursor; }
        void OpenUrl(keel::StringRef) const noexcept override {}

        MainWindowConfig m_cfg{};
        HeadlessCanvas m_canvas{};
        keel::f32 m_dpi{1.0f};
        bool m_vsync{true};
        bool m_gpuShaders{true};
        CursorType lastCursor{CursorType::Default};
        bool initialized{false};
        bool shutdownCalled{false};
        keel::i32 framesPresented{0};
    };

    class TestAppWindow final : public MainWindow {
    public:
        using MainWindow::MainWindow;

        bool initCalled{false};
        bool shutdownCalled{false};
        keel::i32 guiCount{0};
        keel::f32 framePixelsPerDp{0.0f};
        keel::f32 frameDisplayScale{0.0f};
        keel::f32 frameRootZoom{0.0f};
        keel::f32 rootDensity{0.0f};
        Vec2 logicalViewport{};

    protected:
        void OnInit() override {
            initCalled = true;
        }
        void OnRender(Context& ui) override {
            ++guiCount;
            framePixelsPerDp = ui.GetPixelsPerDp();
            frameDisplayScale = ui.GetDisplayScale();
            frameRootZoom = ui.GetRootZoom();
            rootDensity = ui.GetDensity();
            logicalViewport = ui.GetLogicalViewport();
            if (auto tb = ui.Toolbar()) {
                RenderThemeControls(tb);
                RenderDebugControls(tb);
                RenderEngineControls(tb);
            }
            static constexpr keel::StaticArray<CommandPaletteItem, 1> kCmds = {{
                {.label = "Test Cmd", .category = "Test", .shortcut = "Ctrl+T"}
            }};
            if (auto sb = ui.StatusBar()) {
                (void)RenderCommandPalette(sb, kCmds);
                RenderTelemetryStatus(sb, true);
            }

            // Emit a universal GPU shader pass descriptor
            ICanvas::GpuShaderPassDesc pass{};
            pass.widgetId = 42;
            pass.plotRect = {10.0f, 10.0f, 200.0f, 100.0f};
            pass.shader.shaderId = "test.custom_shader";
            pass.shader.glslEs2 = "void main() {}";
            pass.params[0] = {1.0f, 2.0f, 3.0f, 4.0f};
            (void)GetCanvas().DrawGpuShaderPass(pass);

            if (guiCount >= 3) {
                Close();
            }
        }
        void OnShutdown() override {
            shutdownCalled = true;
        }
    };

    auto backend = keel::make_unique<MockWindowBackend>();
    MockWindowBackend* rawBackend = backend.get();

    TestAppWindow app(
        {.title = "Headless Test", .width = 1000, .height = 800, .reactiveMode = false},
        keel::move(backend)
    );
    const keel::i32 exitCode = app.Run();
    CHECK(exitCode == 0);
    CHECK(app.initCalled);
    CHECK(app.shutdownCalled);
    CHECK(app.guiCount == 3);
    CHECK(rawBackend->initialized);
    CHECK(rawBackend->shutdownCalled);
    CHECK(rawBackend->framesPresented == 3);
    CHECK(rawBackend->m_canvas.drawCount > 0);
    CHECK(rawBackend->m_canvas.GetGpuShaderPasses().size() == 1);
    CHECK(rawBackend->m_canvas.GetGpuShaderPasses()[0].shader.shaderId == "test.custom_shader");

    // Default density: MainWindow starts at density 1.0f unless overridden in the config
    CHECK(keel::abs(app.rootDensity - 1.0f) < 1e-6f);
    {
        TestAppWindow touchApp({.reactiveMode = false, .initialDensity = 1.35f}, keel::make_unique<MockWindowBackend>());
        (void)touchApp.Run();
        CHECK(keel::abs(touchApp.rootDensity - 1.35f) < 1e-6f);
    }

    // Shell scale is always a toolbar preset, so the selector label matches the applied scale
    {
        TestAppWindow scaleApp({}, keel::make_unique<MockWindowBackend>());
        auto label = [&]() { return MainWindow::kScaleLabels[scaleApp.GetShellScaleIndex()]; };
        auto scale = [&]() { return scaleApp.GetShellScale(); };

        CHECK(keel::abs(scale() - 1.0f) < 1e-6f);
        CHECK(label() == "100%");

        scaleApp.StepShellScale(-1);
        CHECK(keel::abs(scale() - 0.9f) < 1e-6f);
        CHECK(label() == "90%");
        scaleApp.StepShellScale(-1);
        CHECK(keel::abs(scale() - 0.75f) < 1e-6f);
        CHECK(label() == "75%");
        scaleApp.StepShellScale(-1); // clamped at the smallest preset (Ctrl+Minus cannot go below the selector)
        CHECK(keel::abs(scale() - 0.75f) < 1e-6f);
        CHECK(label() == "75%");

        scaleApp.StepShellScale(+100); // clamped at the largest preset
        CHECK(keel::abs(scale() - 2.0f) < 1e-6f);
        CHECK(label() == "200%");

        // Programmatic values snap to the nearest preset instead of drifting out of the selector's range
        scaleApp.SetShellScale(0.5f);
        CHECK(keel::abs(scale() - 0.75f) < 1e-6f);
        CHECK(label() == "75%");
        scaleApp.SetShellScale(1.3f);
        CHECK(keel::abs(scale() - 1.25f) < 1e-6f);
        CHECK(label() == "125%");
        scaleApp.SetShellScale(3.0f);
        CHECK(keel::abs(scale() - 2.0f) < 1e-6f);
        CHECK(label() == "200%");

        TestAppWindow initApp({.initialScale = 0.8f}, keel::make_unique<MockWindowBackend>());
        CHECK(keel::abs(initApp.GetShellScale() - 0.75f) < 1e-6f);
    }

    // Shell zoom is a frame-wide factor: pixelsPerDp = OS display scale x shell zoom, applied once by the
    // canvas. The widget tree itself stays at scale 1, so nothing can be left unscaled by a missed multiply.
    {
        auto zoomBackend = keel::make_unique<MockWindowBackend>();
        MockWindowBackend* rawZoomBackend = zoomBackend.get();
        rawZoomBackend->m_dpi = 1.5f;
        TestAppWindow zoomApp(
            {.width = 1500, .height = 900, .reactiveMode = false, .initialScale = 1.25f},
            keel::move(zoomBackend)
        );
        CHECK(zoomApp.Run() == 0);
        CHECK(keel::abs(zoomApp.framePixelsPerDp - 1.875f) < 1e-5f);
        CHECK(keel::abs(rawZoomBackend->m_canvas.GetScale() - 1.875f) < 1e-5f);
        CHECK(keel::abs(zoomApp.frameDisplayScale - 1.5f) < 1e-6f);
        CHECK(keel::abs(zoomApp.frameRootZoom - 1.25f) < 1e-6f);
        CHECK(keel::abs(zoomApp.logicalViewport.x - 800.0f) < 0.01f);
        CHECK(keel::abs(zoomApp.logicalViewport.y - 480.0f) < 0.01f);
    }

    // Engine toolbar button opens Popover with working Switches & Heartbeat Select while staying open
    {
        struct EnginePumpBackend final : public IWindowBackend {
            bool Initialize(const MainWindowConfig& cfg, Context&, WindowHostCallbacks) override {
                m_cfg = cfg;
                return true;
            }
            void Shutdown() noexcept override {}
            bool PumpEvents(Context& ui, bool, keel::i32) override {
                switch (step) {
                    case 0:
                        if (hasClickTarget) {
                            ui.OnMouseMove(nextClickPos);
                            ui.OnMouseDown(1);
                            step = 1;
                        }
                        break;
                    case 1:
                        ui.OnMouseMove(nextClickPos);
                        ui.OnMouseUp(1);
                        step = 2;
                        break;
                    case 2:
                        if (hasSwitchTarget) {
                            ui.OnMouseMove(nextSwitchPos);
                            ui.OnMouseDown(1);
                            step = 3;
                        }
                        break;
                    case 3:
                        ui.OnMouseMove(nextSwitchPos);
                        ui.OnMouseUp(1);
                        step = 4;
                        break;
                    default:
                        break;
                }
                return true;
            }
            ICanvas& GetCanvas() noexcept override { return m_canvas; }
            const ICanvas& GetCanvas() const noexcept override { return m_canvas; }
            void PrepareFrameTarget(Context&) override {}
            void PresentFrame(const Context&, FrameMetrics&, bool) override {}
            keel::i32 GetWidth() const noexcept override { return m_cfg.width; }
            keel::i32 GetHeight() const noexcept override { return m_cfg.height; }
            keel::f32 GetDpiScale() const noexcept override { return 1.0f; }
            keel::f32 GetDisplayPixelsPerMm() const noexcept override { return 4.0f; }
            bool IsMinimized() const noexcept override { return false; }
            bool IsLiveResizeFrame() const noexcept override { return false; }
            bool IsVsyncEnabled() const noexcept override { return vsync; }
            void SetVsyncEnabled(bool enabled) noexcept override { vsync = enabled; }
            bool IsGpuShadersEnabled() const noexcept override { return gpu; }
            bool IsGpuShadersAvailable() const noexcept override { return true; }
            void SetGpuShadersEnabled(bool enabled) noexcept override { gpu = enabled; }
            void UpdateSystemCursor(CursorType) noexcept override {}
            void OpenUrl(keel::StringRef) const noexcept override {}

            MainWindowConfig m_cfg{};
            HeadlessCanvas m_canvas{};
            keel::i32 step{0};
            bool vsync{true};
            bool gpu{true};
            bool hasClickTarget{false};
            Vec2 nextClickPos{};
            bool hasSwitchTarget{false};
            Vec2 nextSwitchPos{};
        };

        class EnginePopoverApp final : public MainWindow {
        public:
            EnginePopoverApp(EnginePumpBackend* b, keel::UniquePtr<IWindowBackend> owned)
                : MainWindow({.width = 1000, .height = 700, .reactiveMode = false}, keel::move(owned))
                , m_b(b) {}

            bool sawPopover{false};
            keel::i32 switchCount{0};

        protected:
            void OnRender(Context& ui) override {
                if (auto tb = ui.Toolbar({})) {
                    tb.Spacer();
                    RenderEngineControls(tb);
                    if (Node* tbNode = tb.GetNode()) {
                        for (Node* c = tbNode->firstChild; c != nullptr; c = c->nextSibling) {
                            if (c->type == NodeType::Button && c->text == "Engine") {
                                const Rect r = ui.GetPrevFrameBounds(c->id);
                                if (r.w > 1.0f && !m_b->hasClickTarget) {
                                    m_b->nextClickPos = r.Center();
                                    m_b->hasClickTarget = true;
                                }
                            } else if (c->type == NodeType::Popover) {
                                sawPopover = true;
                                Node* contentRoot = (c->firstChild && c->firstChild->type == NodeType::Card)
                                    ? c->firstChild
                                    : c;
                                keel::i32 idx = 0;
                                for (Node* pChild = contentRoot->firstChild; pChild != nullptr; pChild = pChild->nextSibling) {
                                    if (pChild->type == NodeType::Element && pChild->firstChild &&
                                        pChild->firstChild->type == NodeType::Switch) {
                                        if (idx == 1) { // VSync switch
                                            const Rect sr = ui.GetPrevUnclippedBounds(pChild->firstChild->id);
                                            if (sr.w > 1.0f && !m_b->hasSwitchTarget) {
                                                m_b->nextSwitchPos = sr.Center();
                                                m_b->hasSwitchTarget = true;
                                            }
                                        }
                                        ++idx;
                                    }
                                }
                                switchCount = idx;
                            }
                        }
                    }
                }
                if ((m_b->step >= 4 && postDoneFrames++ >= 1) || safetyFrames++ >= 20) {
                    Close();
                }
            }

            EnginePumpBackend* m_b{nullptr};
            keel::i32 postDoneFrames{0};
            keel::i32 safetyFrames{0};
        };

        auto engBackend = keel::make_unique<EnginePumpBackend>();
        EnginePumpBackend* rawB = engBackend.get();
        EnginePopoverApp engApp(rawB, keel::move(engBackend));
        CHECK(engApp.Run() == 0);
        CHECK(engApp.sawPopover);
        CHECK(engApp.switchCount == 6); // 5 pipeline switches + Gamma Blending
        CHECK(!rawB->vsync); // VSync toggled off via Engine popover Switch
    }
}

void TestKeelIntegration() {
    using namespace guikit;

    // 1. keel::Hash<guikit::Id> & keel::HashMap<Id, Rect>
    keel::HashMap<Id, Rect> boundsMap;
    const Id id1 = MakeId(1);
    const Id id2 = MakeId(2);
    boundsMap.put(id1, Rect{10.0f, 20.0f, 100.0f, 40.0f});
    boundsMap.put(id2, Rect{50.0f, 60.0f, 200.0f, 80.0f});
    CHECK(boundsMap.size() == 2);
    const Rect* r1 = boundsMap.get(id1);
    CHECK(r1 != nullptr && *r1 == Rect(10.0f, 20.0f, 100.0f, 40.0f));

    // 1b. keel::Optional<T&> non-owning reference specialization & ZoneOptions::theme safety
    {
        static_assert(sizeof(keel::Optional<keel::i32&>) == sizeof(keel::i32*));
        static_assert(sizeof(keel::Optional<const UiTheme&>) == sizeof(const UiTheme*));
        static_assert(keel::IsTriviallyCopyable<keel::Optional<const UiTheme&>>);
        static_assert(!keel::IsConstructible<keel::Optional<const UiTheme&>, UiTheme&&>);

        keel::i32 a = 10;
        keel::i32 b = 20;
        keel::Optional<keel::i32&> refOpt;
        CHECK(!refOpt.has_value());
        CHECK(refOpt.value_or(77) == 77);
        refOpt = a;
        CHECK(refOpt.has_value() && &(*refOpt) == &a);
        *refOpt = 15;
        CHECK(a == 15);
        refOpt = b; // rebinds reference without overwriting a
        CHECK(a == 15 && &(*refOpt) == &b);
        keel::Optional<const keel::i32&> crefOpt = refOpt;
        CHECK(crefOpt.has_value() && &(*crefOpt) == &b);
        crefOpt.reset();
        CHECK(!crefOpt.has_value());
    }

    // 2. keel::Arena::allocator() arena bridge
    keel::Arena frameArena(4096);
    keel::Array<keel::i32> arenaArray(frameArena.allocator());
    for (keel::i32 i = 0; i < 32; ++i) {
        arenaArray.push_back(i * 5);
    }
    CHECK(arenaArray.size() == 32);
    CHECK(arenaArray[10] == 50);
    CHECK(frameArena.used() > 0);

    // 3. keel::FixedString <-> keel::StringRef & keel::SmallArray <-> keel::ArrayRef
    keel::FixedString<> label(keel::StringRef("Keel Bridge"));
    keel::StringRef ref = label.as_ref();
    CHECK(ref == "Keel Bridge");

    const keel::i32 rawVals[4] = {1, 2, 3, 4};
    keel::SmallArray<keel::i32, 4> uvec(keel::ArrayRef<keel::i32>(rawVals, 4));
    keel::ArrayRef<keel::i32> aref = uvec;
    CHECK(aref.size() == 4);
    CHECK(aref[2] == 3);

    // 4. IndexListSink with keel::Array<size_t> and keel::SmallArray<size_t, 8>
    {
        IndexListSink nullSink = nullptr;
        CHECK(!nullSink.has_target());
        CHECK(!bool(nullSink));
        CHECK(nullSink == nullptr);
        CHECK(nullSink.empty());
        CHECK(nullSink.size() == 0);
        nullSink.push_back(99);
        nullSink.erase_at(0);
        nullSink.clear();
        CHECK(nullSink.empty());

        keel::Array<keel::usize> dynIndices;
        IndexListSink arrSink(&dynIndices);
        CHECK(arrSink.has_target() && arrSink != nullptr);
        arrSink.push_back(10);
        arrSink.push_back(20);
        arrSink.push_back(30);
        CHECK(arrSink.size() == 3 && dynIndices.size() == 3);
        CHECK(arrSink[0] == 10 && arrSink[1] == 20 && arrSink[2] == 30);
        arrSink[1] = 25;
        CHECK(dynIndices[1] == 25);
        arrSink.erase_at(0);
        CHECK(arrSink.size() == 2 && arrSink[0] == 25 && arrSink[1] == 30);
        arrSink.erase_at(99);
        CHECK(arrSink.size() == 2);
        arrSink.clear();
        CHECK(arrSink.empty() && dynIndices.empty());

        keel::SmallArray<keel::usize, 8> smallIndices;
        IndexListSink smallSink(&smallIndices);
        CHECK(smallSink.has_target());
        smallSink.push_back(3);
        smallSink.push_back(7);
        smallSink.push_back(11);
        smallSink.push_back(15);
        CHECK(smallIndices.is_inline());
        CHECK(smallSink.size() == 4);
        keel::usize sum = 0;
        for (keel::usize v : smallSink) {
            sum += v;
        }
        CHECK(sum == (3 + 7 + 11 + 15));
        smallSink.erase_at(2);
        static constexpr keel::StaticArray<keel::usize, 3> expectedSmall = {{3, 7, 15}};
        CHECK(smallSink.as_ref() == expectedSmall.as_ref());
        smallSink.clear();
        CHECK(smallSink.empty() && smallIndices.empty());
    }

    // 5. UiText + TextVariants with keel::StaticArray<keel::StringRef, N>
    {
        static constexpr keel::StaticArray<keel::StringRef, 3> kModeVariants = {{
            "Idle",
            "Acquiring Telemetry",
            "Calibrating Sensor"
        }};

        const TextVariants tv(kModeVariants);
        CHECK(!tv.empty());
        CHECK(tv.size() == 3);
        CHECK(tv.span()[0] == "Idle");
        CHECK(tv.span()[1] == "Acquiring Telemetry");
        CHECK(tv.span()[2] == "Calibrating Sensor");

        const UiText txtWithVars("Idle", kModeVariants);
        CHECK(txtWithVars == "Idle");
        CHECK(txtWithVars.variants.size() == 3);
        CHECK(txtWithVars.HasContentOrReserve());

        const keel::f32 wShort = TextUtil::MeasureWidth("Idle", 14.0f, nullptr);
        const keel::f32 wLongest = TextUtil::MeasureWidth("Acquiring Telemetry", 14.0f, nullptr);
        const keel::f32 wUiText = TextUtil::MeasureWidth(txtWithVars, 14.0f, nullptr);
        CHECK(wUiText > wShort);
        CHECK(keel::abs(wUiText - wLongest) < 1e-4f);

        const UiText styledVars("Idle", TextRole::LabelMedium, UiVariant::Success, kModeVariants);
        CHECK(styledVars == "Idle");
        CHECK(styledVars.size.role == TextRole::LabelMedium);
        CHECK(styledVars.variants == tv);

        const UiText reserveOnly(kModeVariants);
        CHECK(reserveOnly.empty());
        CHECK(reserveOnly.HasContentOrReserve());
        CHECK(keel::abs(TextUtil::MeasureWidth(reserveOnly, 14.0f, nullptr) - wLongest) < 1e-4f);
    }

    // 6. keel/math.h constants, numeric limits, and UB-free integer rounding
    {
        static_assert(keel::round_i32(2.5f) == 3);
        static_assert(keel::round_i32(-2.5f) == -3);
        static_assert(keel::round_i32(keel::Limits<keel::f32>::quiet_nan()) == 0);
        static_assert(keel::round_i32(keel::Limits<keel::f32>::infinity()) == keel::Limits<keel::i32>::kMax);
        static_assert(keel::floor_i32(-1.2f) == -2);
        static_assert(keel::ceil_i32(1.2f) == 2);
        static_assert(keel::approx_eq(keel::deg_to_rad(180.0f), keel::kPiF));
        CHECK(keel::is_nan(keel::Limits<keel::f32>::quiet_nan()));
        CHECK(keel::is_inf(keel::Limits<keel::f64>::infinity()));
        CHECK(keel::round_u32(254.6f) == 255u);
    }
}

void TestThemeTokensPaddingAndDateTime() {
    using namespace guikit;

    // 1. Theme-aware StateColor token resolution in Light vs Dark themes
    const UiTheme dark = UiTheme::Dark();
    const UiTheme light = UiTheme::Light();

    CHECK(light.ResolveColor(StateColor::Surface(), false, false, false, Color()) == light.palette.surface);
    CHECK(dark.ResolveColor(StateColor::Surface(), false, false, false, Color()) == dark.palette.surface);
    CHECK(light.palette.surface != dark.palette.surface);

    CHECK(light.ResolveColor(StateColor::Primary(), false, false, false, Color()) == light.GetVariant(UiVariant::Primary).bg);
    CHECK(dark.ResolveColor(StateColor::Primary(), false, false, false, Color()) == dark.GetVariant(UiVariant::Primary).bg);
    CHECK(light.ResolveColor(StateColor::Secondary(), false, false, false, Color()) == light.GetVariant(UiVariant::Secondary).bg);
    CHECK(dark.ResolveColor(StateColor::Secondary(), false, false, false, Color()) == dark.GetVariant(UiVariant::Secondary).bg);
    CHECK(light.GetVariant(UiVariant::Secondary).bg != dark.GetVariant(UiVariant::Secondary).bg);

    const StateColor lightTxt = light.ResolveTextStateColor(StateColor::TextPrimary());
    const StateColor darkTxt = dark.ResolveTextStateColor(StateColor::TextPrimary());
    CHECK(lightTxt.normal == light.palette.textPrimary);
    CHECK(darkTxt.normal == dark.palette.textPrimary);
    CHECK(lightTxt.normal != darkTxt.normal);

    const Color customRgb(17, 89, 203, 255);
    const StateColor customSc = StateColor::FromColor(customRgb);
    CHECK(customSc.token == StateColorToken::Custom);
    CHECK(light.ResolveColor(customSc, false, false, false, Color()) == customRgb);
    CHECK(dark.ResolveColor(customSc, false, false, false, Color()) == customRgb);

    // 1b. Tonal surface tint (`Tinted`) & theme-reactive opacity (`WithAlphaF` / `WithAlpha`)
    static_assert(Color::Mix255(Color(10, 20, 30, 255), Color(200, 100, 50, 255), 0) == Color(10, 20, 30, 255));
    static_assert(Color::Mix255(Color(10, 20, 30, 255), Color(200, 100, 50, 255), 255) == Color(200, 100, 50, 255));
    static_assert(Color(200, 100, 50, 255).ScaleAlpha(128).a == 128);

    const StateColor dgrPure = StateColor::Danger();
    const StateColor dgrTint0 = StateColor::Danger().Tinted(0.0f);
    const StateColor dgrTint15 = StateColor::Danger().Tinted(0.15f);
    const StateColor dgrTint100 = StateColor::Danger().Tinted(1.0f);

    CHECK(dgrTint15.IsTinted());
    CHECK(!dgrPure.IsTinted());
    CHECK(!dgrTint100.IsTinted());

    // Tinted(0.0f) -> pure opaque theme surface; Tinted(1.0f) -> pure semantic variant bg
    CHECK(dark.ResolveColor(dgrTint0, false, false, false, Color()) == dark.palette.surface);
    CHECK(light.ResolveColor(dgrTint0, false, false, false, Color()) == light.palette.surface);
    CHECK(dark.ResolveColor(dgrTint100, false, false, false, Color()) == dark.GetVariant(UiVariant::Danger).bg);
    CHECK(light.ResolveColor(dgrTint100, false, false, false, Color()) == light.GetVariant(UiVariant::Danger).bg);

    // Tinted(0.15f) -> 100% opaque tonal surface in normal, hover, and active states across Dark & Light
    const StateColor darkTinted = dark.ResolveStateColor(dgrTint15);
    const StateColor lightTinted = light.ResolveStateColor(dgrTint15);
    CHECK(darkTinted.normal.has_value() && darkTinted.normal->a == 255);
    CHECK(darkTinted.hover.has_value() && darkTinted.hover->a == 255);
    CHECK(darkTinted.active.has_value() && darkTinted.active->a == 255);
    CHECK(darkTinted.normal->r > dark.palette.surface.r);
    CHECK(darkTinted.normal->r < dark.GetVariant(UiVariant::Danger).bg.r);

    CHECK(lightTinted.normal.has_value() && lightTinted.normal->a == 255);
    CHECK(lightTinted.hover.has_value() && lightTinted.hover->a == 255);
    CHECK(lightTinted.active.has_value() && lightTinted.active->a == 255);
    CHECK(lightTinted.normal->g < light.palette.surface.g);
    CHECK(lightTinted.normal->g > light.GetVariant(UiVariant::Danger).bg.g);

    // Post-resolve alpha scaling (`WithAlphaF`) preserves theme reactivity without double-scaling
    const StateColor surfHalf = StateColor::Surface().WithAlpha(128);
    CHECK(surfHalf.IsSurface());
    CHECK(surfHalf.HasAlphaScale());
    const Color darkSurfHalf = dark.ResolveColor(surfHalf, false, false, false, Color());
    const Color lightSurfHalf = light.ResolveColor(surfHalf, false, false, false, Color());
    CHECK(darkSurfHalf == dark.palette.surface.ScaleAlpha(128));
    CHECK(lightSurfHalf == light.palette.surface.ScaleAlpha(128));

    const Color customHalf = StateColor::FromColor(Color(100, 150, 200, 255)).WithAlpha(128).Resolve(false, false, false, Color());
    CHECK(customHalf == Color(100, 150, 200, 128));

    // Dirty-rect hash sensitivity for `surfaceTint` and `alphaScale`
    CHECK(detail::HashArgs(dgrPure) != detail::HashArgs(dgrTint15));
    CHECK(detail::HashArgs(dgrTint15) != detail::HashArgs(dgrTint15.WithAlphaF(0.5f)));
    CHECK(detail::HashArgs(StateColor::Surface()) != detail::HashArgs(surfHalf));

    // 2. Negative Padding must never collide with Padding::IsToken()
    const Padding neg1 = Padding::All(-1.0f);
    const Padding neg2 = Padding::All(-2.0f);
    const Padding neg4 = Padding::All(-4.0f);
    const Padding asymNeg(-1.0f, 6.0f, -3.0f, 2.0f);
    CHECK(!neg1.IsToken());
    CHECK(!neg2.IsToken());
    CHECK(!neg4.IsToken());
    CHECK(!asymNeg.IsToken());
    CHECK(neg1.GetToken() == UiSize::None);
    CHECK(neg1.Horizontal() == -2.0f);
    CHECK(neg2.Vertical() == -4.0f);
    CHECK(asymNeg.Horizontal() == -4.0f);
    CHECK(asymNeg.Vertical() == 8.0f);

    const Padding tokSm(UiSize::Sm);
    const Padding tokXl(UiSize::Xl);
    CHECK(tokSm.IsToken());
    CHECK(tokXl.IsToken());
    CHECK(tokSm.GetToken() == UiSize::Sm);
    CHECK(tokXl.GetToken() == UiSize::Xl);
    CHECK(tokSm != neg1);
    CHECK(tokSm != tokXl);

    // 3. DateTime::Today() sanity bounds
    const DateTime today = DateTime::Today();
    CHECK(today.year >= 2024 && today.year <= 2200);
    CHECK(today.month >= 1 && today.month <= 12);
    CHECK(today.day >= 1 && today.day <= 31);
    CHECK(today.hour >= 0 && today.hour <= 23);
    CHECK(today.minute >= 0 && today.minute <= 59);
    CHECK(today.second >= 0 && today.second <= 60);

    // 4. Node::GetTypeName() completeness for newer widget/panel types
    Node n{};
    const NodeType checkTypes[] = {
        NodeType::Breadcrumb,
        NodeType::DatePicker,
        NodeType::LevelMeter,
        NodeType::Pagination,
        NodeType::TagInput,
        NodeType::SplitView,
        NodeType::CommandPalette
    };
    for (NodeType nt : checkTypes) {
        n.type = nt;
        CHECK(n.GetTypeName() != "Unknown");
        CHECK(!n.GetTypeName().empty());
    }

#if KEEL_HAS_EXCEPTIONS
    // Verify zero-heap keel::Exception (`std::exception` compatible) works cleanly under guikit_enforce_guidelines
    bool caughtStdException = false;
    try {
        throw keel::Exception::with_error(
            keel::Error{keel::ErrorCode::InvalidArgument, 0}, "Invalid gain: {:.1f} dB", 24.5f
        );
    } catch (const std::exception& e) {
        caughtStdException = (keel::StringRef(e.what()) == "Invalid gain: 24.5 dB");
    }
    CHECK(caughtStdException);
#endif
}

void TestElementHeadingSpecAndSeparatorSpace() {
    keel::println("  -> TestElementHeadingSpecAndSeparatorSpace...");
    using namespace guikit;

    // Compile-time purity checks: ElementOptions and HeadingOptions have zero panel surface/border fields,
    // and ElementOptions has zero heading-hierarchy or divider fields.
    static_assert(![]<typename T = guikit::ElementOptions>() { return requires(T o) { o.background; }; }());
    static_assert(![]<typename T = guikit::ElementOptions>() { return requires(T o) { o.border; }; }());
    static_assert(![]<typename T = guikit::ElementOptions>() { return requires(T o) { o.level; }; }());
    static_assert(![]<typename T = guikit::ElementOptions>() { return requires(T o) { o.separator; }; }());
    static_assert(![]<typename T = guikit::HeadingOptions>() { return requires(T o) { o.background; }; }());
    static_assert(![]<typename T = guikit::HeadingOptions>() { return requires(T o) { o.border; }; }());

    const UiTheme theme = UiTheme::Dark();

    // 0. Compile-time & constexpr invariants for unified AxisSize policy value type
    static_assert(sizeof(AxisSize) == 8);
    static_assert(!keel::IsConstructible<AxisSize, bool>);
    static_assert(!keel::IsConstructible<AxisSize, char>);
    static_assert(AxisSize{}.IsAuto());
    static_assert(AxisSize(SizeMode::Fit).IsFit());
    static_assert(AxisSize(SizeMode::Stretch).IsStretch());
    static_assert(AxisSize(SizeMode::Stretch) == AxisSize::Stretch());
    static_assert(AxisSize::Stretch().StretchWeight() == 1.0f && !AxisSize::Stretch().HasCustomStretchWeight());
    static_assert(AxisSize::Stretch(2.5f).IsStretch() && AxisSize::Stretch(2.5f) == SizeMode::Stretch);
    static_assert(AxisSize::Stretch(2.5f).StretchWeight() == 2.5f && AxisSize::Stretch(2.5f).HasCustomStretchWeight());
    static_assert(AxisSize::Stretch(2.5f).Dp() == 0.0f);
    static_assert(AxisSize::Stretch(0.0f).StretchWeight() == 1.0f && AxisSize::Stretch(-3.0f).StretchWeight() == 1.0f);
    static_assert(AxisSize::Stretch(2.0f) != AxisSize::Stretch(1.0f));
    static_assert(AxisSize(SizeMode::Fixed).IsFit()); // Bare Fixed without dp safely falls back to Fit
    static_assert(AxisSize(180.0f).IsFixed() && AxisSize(180.0f).Dp() == 180.0f);
    static_assert(AxisSize(200).IsFixed() && AxisSize(200).Dp() == 200.0f);
    static_assert(AxisSize(0.0f).IsAuto() && AxisSize(-5.0f).IsAuto());
    static_assert(AxisSize::Auto().ResolveMode(SizeMode::Stretch) == SizeMode::Stretch);
    static_assert(AxisSize::Fit().ResolveMode(SizeMode::Stretch) == SizeMode::Fit);
    static_assert(AxisSize::Fixed(120.0f).ResolveMode(SizeMode::Fit) == SizeMode::Fixed);

    // 1. Control-less ElementOptions uses compact form-field typography and AxisSize::Fixed when explicit dp width > 0
    const ElementOptions elemOpts{
        .label = "Channel Gain",
        .description = "Preamp stage",
        .width = 240.0f
    };
    const ElementSpec elemSpec = Element::MakeSpec(elemOpts, UiSize::Md, false, theme);
    CHECK(!elemSpec.hasControl);
    CHECK(elemSpec.width.IsFixed());
    CHECK(elemSpec.width == SizeMode::Fixed);
    CHECK(keel::abs(elemSpec.width.Dp() - 240.0f) < 0.001f);

    const ElementSpec fitSpec = Element::MakeSpec(
        ElementOptions{.label = "Compact Tag", .width = SizeMode::Fit}, UiSize::Md, false, theme);
    CHECK(fitSpec.width.IsFit());
    CHECK(fitSpec.width == SizeMode::Fit);

    // 2. Standalone Heading::Measure & ComputeBottomSeparatorSpace across HeadingLevel and HeadingLayout
    const HeadingOptions h2BottomOpts{
        .label = "Audio Bus",
        .description = "Master routing",
        .level = HeadingLevel::H2,
        .layout = HeadingLayout::Stacked,
        .separator = {HeadingSeparator::Bottom, StateColor::Primary(), 2.0f}
    };
    const HeadingOptions h2NoneOpts{
        .label = "Audio Bus",
        .description = "Master routing",
        .level = HeadingLevel::H2,
        .layout = HeadingLayout::Stacked,
        .separator = HeadingSeparator::None
    };
    const keel::f32 sepSpace = Heading::ComputeBottomSeparatorSpace(h2BottomOpts, theme);
    CHECK(sepSpace > 6.0f);
    CHECK(keel::abs(Heading::ComputeBottomSeparatorSpace(h2NoneOpts, theme)) < 0.001f);

    const Vec2 mBottom = Heading::Measure(h2BottomOpts, nullptr, theme, 600.0f);
    const Vec2 mNone = Heading::Measure(h2NoneOpts, nullptr, theme, 600.0f);
    CHECK(keel::abs((mBottom.y - mNone.y) - sepSpace) < 0.05f);

    // 3. Overline matches Stacked intrinsic height, while Inline places title + subtitle on one horizontal row
    const HeadingOptions overlineOpts{
        .label = "Audio Bus",
        .description = "Master routing",
        .level = HeadingLevel::H3,
        .layout = HeadingLayout::Overline
    };
    const HeadingOptions stackedH3Opts{
        .label = "Audio Bus",
        .description = "Master routing",
        .level = HeadingLevel::H3,
        .layout = HeadingLayout::Stacked
    };
    const HeadingOptions inlineH3Opts{
        .label = "Audio Bus",
        .description = "Master routing",
        .level = HeadingLevel::H3,
        .layout = HeadingLayout::Inline
    };
    const Vec2 mOver = Heading::Measure(overlineOpts, nullptr, theme, 600.0f);
    const Vec2 mStack = Heading::Measure(stackedH3Opts, nullptr, theme, 600.0f);
    const Vec2 mInline = Heading::Measure(inlineH3Opts, nullptr, theme, 600.0f);
    CHECK(keel::abs(mOver.y - mStack.y) < 0.05f);
    CHECK(mInline.x > mStack.x);
    CHECK(mInline.y < mStack.y);

    // 4. Trailing separator adds zero vertical bottom space
    const HeadingOptions trailingOpts{
        .label = "Inline Section",
        .level = HeadingLevel::H4,
        .separator = HeadingSeparator::Trailing
    };
    CHECK(keel::abs(Heading::ComputeBottomSeparatorSpace(trailingOpts, theme)) < 0.001f);
}

void TestMdRefAndPixelRef() {
    using namespace guikit;
    keel::println("  -> TestMdRefAndPixelRef...");

    // 1. Valid 2D MutMdRef / MdRef indexing, row slicing, outer slice, and reshape
    keel::StaticArray<keel::f32, 6> storage = {{1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}};
    keel::MutMdRef<keel::f32, 2> mutMat(storage, 2, 3);
    CHECK(!mutMat.empty());
    CHECK(mutMat.rank() == 2);
    CHECK(mutMat.extent(0) == 2);
    CHECK(mutMat.extent(1) == 3);
    CHECK(mutMat.size() == 6);
    CHECK(mutMat.stride(0) == 3);
    CHECK(mutMat.stride(1) == 1);
    CHECK(mutMat(1, 2) == 6.0f);
    mutMat(1, 2) = 42.0f;

    const keel::MdRef<keel::f32, 2> mat = mutMat;
    CHECK(mat(1, 2) == 42.0f);
    CHECK(mat.row(0).size() == 3);
    CHECK(mat.row(1)[2] == 42.0f);
    CHECK(mat.subview(1)(2) == 42.0f);

    const keel::MdRef<keel::f32, 2> tailRow = mat.slice(1, 1);
    CHECK(tailRow.extent(0) == 1);
    CHECK(tailRow.extent(1) == 3);
    CHECK(tailRow(0, 2) == 42.0f);

    const keel::MdRef<keel::f32, 3> vol = mat.reshape(1, 3, 2);
    CHECK(!vol.empty());
    CHECK(vol(0, 2, 1) == 42.0f);
    CHECK(mat.reshape(2, 2).empty()); // Mismatched total element count rejected

    // 2. Undersized, zero-extent, negative-signed, and overflowed shapes normalize to empty()
    CHECK((keel::MdRef<keel::f32, 2>(storage, 3, 3).empty()));
    CHECK((keel::MdRef<keel::f32, 2>(storage, 0, 3).empty()));
    CHECK((keel::MdRef<keel::f32, 2>(storage, -2, 3).empty()));
    CHECK((keel::MdRef<keel::f32, 2>(storage, keel::kInvalidIndex / 2, 4).empty()));

    // 3. Graphics PixelRef & MutPixelRef preserve (width, height) contract over MdRef<u32, 2>
    keel::StaticArray<keel::u32, 6> pxBuf{};
    MutPixelRef mutPx(pxBuf, 3, 2);
    CHECK(!mutPx.Empty());
    CHECK(mutPx.Width() == 3);
    CHECK(mutPx.Height() == 2);
    mutPx.Fill(0xFF112233u);
    mutPx.At(2, 1) = 0xFFAABBCCu;

    const PixelRef px = mutPx;
    CHECK(px.At(0, 0) == 0xFF112233u);
    CHECK(px.At(2, 1) == 0xFFAABBCCu);
    CHECK(px.Row(1)[2] == 0xFFAABBCCu);
    CHECK(PixelRef(pxBuf, 4, 2).Empty()); // undersized rejected

    // 4. StridedMdRef / MutStridedMdRef: column slicing, decimation, transpose, RGB channel extraction & PixelRef ROI
    {
        // 4a. Spectrogram-like 2D matrix (4 time frames x 3 freq bins): extract column (bin 1) & decimate frames
        keel::StaticArray<keel::f32, 12> specBuf = {{
            1.0f,  2.0f,  3.0f,
            4.0f,  5.0f,  6.0f,
            7.0f,  8.0f,  9.0f,
            10.0f, 11.0f, 12.0f
        }};
        keel::MutMdRef<keel::f32, 2> specMut(specBuf, 4, 3);
        const keel::StridedMdRef<keel::f32, 2> spec = specMut.as_ref();
        CHECK(spec.is_contiguous());
        CHECK(!spec.as_contiguous().empty());

        const keel::StridedMdRef<keel::f32, 1> bin1 = spec.col(1);
        CHECK(bin1.extent(0) == 4);
        CHECK(bin1.stride(0) == 3);
        CHECK(!bin1.is_contiguous());
        CHECK(bin1.as_contiguous().empty());
        CHECK(bin1(0) == 2.0f);
        CHECK(bin1(3) == 11.0f);

        const keel::StridedMdRef<keel::f32, 1> decimated = bin1.slice_axis(0, 1, keel::kInvalidIndex, 2);
        CHECK(decimated.extent(0) == 2);
        CHECK(decimated.stride(0) == 6);
        CHECK(decimated(0) == 5.0f);
        CHECK(decimated(1) == 11.0f);

        const keel::StridedMdRef<keel::f32, 2> specT = spec.transpose();
        CHECK(specT.extent(0) == 3 && specT.extent(1) == 4);
        CHECK(specT(2, 3) == 12.0f);

        // 4b. Interleaved 3D RGB image (2 x 2 x 3) -> 2D Green channel slice & mutable ROI fill
        keel::StaticArray<keel::u8, 12> rgbBuf = {{
            10, 20, 30,   40, 50, 60,
            70, 80, 90,   100, 110, 120
        }};
        const keel::MdRef<keel::u8, 3> rgb(rgbBuf, 2, 2, 3);
        const keel::StridedMdRef<keel::u8, 2> greenPlane = rgb.strided().subview(2, 1);
        CHECK(greenPlane.extent(0) == 2 && greenPlane.extent(1) == 2);
        CHECK(greenPlane(0, 0) == 20 && greenPlane(0, 1) == 50);
        CHECK(greenPlane(1, 0) == 80 && greenPlane(1, 1) == 110);

        // 4c. StridedPixelRef / MutStridedPixelRef ROI crop keeps (x, y) & (Width, Height) semantics
        mutPx.Fill(0x00000000u);
        MutStridedPixelRef roiMut = mutPx.Subrect(1, 0, 2, 2);
        CHECK(!roiMut.Empty());
        CHECK(roiMut.Width() == 2);
        CHECK(roiMut.Height() == 2);
        CHECK(roiMut.RowStride() == 3);
        CHECK(!roiMut.IsContiguous());
        roiMut.Fill(0xFFDEADBEu);
        roiMut.At(1, 1) = 0xFFCAFE00u;

        const StridedPixelRef roi = px.Subrect(1, 0, 2, 2);
        CHECK(roi.At(0, 0) == 0xFFDEADBEu);
        CHECK(roi.At(1, 1) == 0xFFCAFE00u);
        CHECK(roi.Row(1)[0] == 0xFFDEADBEu);
        CHECK(roi.Subrect(1, 1, 1, 1).At(0, 0) == 0xFFCAFE00u);
        CHECK(px.At(0, 0) == 0x00000000u);
        CHECK(px.At(0, 1) == 0x00000000u);
        CHECK(px.At(1, 0) == 0xFFDEADBEu);
        CHECK(px.At(2, 1) == 0xFFCAFE00u);
        CHECK(StridedPixelRef(pxBuf, 3, 2, 2).Empty()); // rowStride < width rejected
        CHECK(StridedPixelRef(px).IsContiguous());
        CHECK(StridedPixelRef(px).AsContiguous().Width() == 3);

        // 4d. Out-of-bounds strided buffer & overflow rejection
        CHECK((keel::StridedMdRef<keel::f32, 2>(specBuf, {4, 3}, {4, 1}).empty())); // (3*4 + 2*1 = 14) >= 12
        CHECK((keel::StridedMdRef<keel::f32, 2>(specBuf, {2, 2}, {keel::kInvalidIndex, 1}).empty()));
        CHECK(spec.slice_axis(0, 4, 1).empty());
    }
}

void TestBuiltInImageDecoding() {
    using namespace guikit;

    // Pure guikit::core must inspect & decode embedded raster assets via built-in keel::imagefile
    // without requiring native or sdl backends to register a custom ImageDecoderFn.
    const auto iconAsset = Assets::Find("assets/guikit-icon-inter850-1024.png");
    CHECK(iconAsset.has_value() && !iconAsset->Empty());
    if (iconAsset && !iconAsset->Empty()) {
        keel::u32 peekW = 0u;
        keel::u32 peekH = 0u;
        CHECK(InspectImageFromMemory(iconAsset->Bytes(), peekW, peekH));
        CHECK(peekW == 1024u && peekH == 1024u);

        keel::u32 decW = 0u;
        keel::u32 decH = 0u;
        keel::Array<keel::u32> rgba;
        CHECK(DecodeImageFromMemory(iconAsset->Bytes(), decW, decH, rgba));
        CHECK(decW == 1024u && decH == 1024u);
        CHECK(rgba.size() == 1024u * 1024u);

        keel::u32 opaqueCount = 0u;
        for (keel::usize i = 0u; i < rgba.size(); i += 257u) {
            if ((rgba[i] >> 24) > 0u) {
                ++opaqueCount;
            }
        }
        CHECK(opaqueCount > 100u);
    }

    // Minimal valid 2x1 24-bit uncompressed TGA verifies multi-format decoding beyond PNG
    const keel::u8 tinyTga[24] = {
        0u, 0u, 2u, 0u, 0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u, 2u, 0u, 1u, 0u, 24u, 0x20u,
        10u, 20u, 30u, // BGR -> RGB(30, 20, 10)
        40u, 50u, 60u  // BGR -> RGB(60, 50, 40)
    };
    const keel::ArrayRef<keel::u8> tgaRef(tinyTga, sizeof(tinyTga));
    keel::u32 tw = 0u;
    keel::u32 th = 0u;
    keel::Array<keel::u32> tgaRgba;
    CHECK(InspectImageFromMemory(tgaRef, tw, th));
    CHECK(tw == 2u && th == 1u);
    CHECK(DecodeImageFromMemory(tgaRef, tw, th, tgaRgba));
    CHECK(tw == 2u && th == 1u && tgaRgba.size() == 2u);
    CHECK(tgaRgba[0] == (30u | (20u << 8) | (10u << 16) | (255u << 24)));
    CHECK(tgaRgba[1] == (60u | (50u << 8) | (40u << 16) | (255u << 24)));

    // Garbage / truncated input must fail cleanly without touching outputs
    const keel::u8 garbage[8] = {0x00u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u};
    keel::u32 gw = 99u;
    keel::u32 gh = 99u;
    keel::Array<keel::u32> gRgba;
    CHECK(!InspectImageFromMemory(keel::ArrayRef<keel::u8>(garbage, sizeof(garbage)), gw, gh));
    CHECK(gw == 0u && gh == 0u);
    CHECK(!DecodeImageFromMemory(keel::ArrayRef<keel::u8>(garbage, sizeof(garbage)), gw, gh, gRgba));
    CHECK(gw == 0u && gh == 0u);
}

void TestThemeSystemColorSciencePresetsSpecJsonAndLerp() {
    using namespace guikit;

    // 1. WCAG luminance, contrast ratio & auto-contrast BestTextColor
    const Color black = Color::Hex(0x000000FF);
    const Color white = Color::Hex(0xFFFFFFFF);
    CHECK(black.RelativeLuminance() < 0.001f);
    CHECK(white.RelativeLuminance() > 0.999f);
    CHECK(Color::ContrastRatio(black, white) > 20.5f);
    CHECK(Color::ContrastRatio(white, white) < 1.01f);

    const Color brightYellow = Color::Hex(0xFDE047FF);
    const Color deepIndigo   = Color::Hex(0x1E1B4BFF);
    const Color textOnYellow = Color::BestTextColor(brightYellow);
    const Color textOnIndigo = Color::BestTextColor(deepIndigo);
    CHECK(textOnYellow.RelativeLuminance() < 0.1f);
    CHECK(textOnIndigo.RelativeLuminance() > 0.9f);
    CHECK(Color::ContrastRatio(textOnYellow, brightYellow) >= 4.5f);
    CHECK(Color::ContrastRatio(textOnIndigo, deepIndigo) >= 4.5f);

    // 2. HSL round-trip & Lighten/Darken monotonicity
    const Color teal = Color::Hex(0x14B8A6FF);
    keel::f32 h = 0.0f;
    keel::f32 s = 0.0f;
    keel::f32 l = 0.0f;
    teal.ToHsl(h, s, l);
    const Color roundTripped = Color::FromHsl(h, s, l, teal.a);
    CHECK(keel::abs(keel::as<keel::i32>(roundTripped.r) - keel::as<keel::i32>(teal.r)) <= 2);
    CHECK(keel::abs(keel::as<keel::i32>(roundTripped.g) - keel::as<keel::i32>(teal.g)) <= 2);
    CHECK(keel::abs(keel::as<keel::i32>(roundTripped.b) - keel::as<keel::i32>(teal.b)) <= 2);
    CHECK(teal.Lighten(0.15f).RelativeLuminance() > teal.RelativeLuminance());
    CHECK(teal.Darken(0.15f).RelativeLuminance() < teal.RelativeLuminance());

    // 3. All curated ThemePresets meet WCAG AA contrast (and HighContrast meets >= 12:1)
    for (keel::usize i = 0u; i < kThemePresetCount; ++i) {
        const auto preset = keel::from_underlying<ThemePreset>(i);
        CHECK(!ThemePresetName(preset).empty());
        const UiTheme t = UiTheme::FromPreset(preset);
        CHECK(t.preset == preset);
        const keel::f32 textBg = Color::ContrastRatio(t.palette.textPrimary, t.palette.background);
        const keel::f32 accentContrast = Color::ContrastRatio(t.palette.accentText, t.palette.accent);
        CHECK(textBg >= 4.5f);
        CHECK(accentContrast >= 4.5f);
        if (preset == ThemePreset::HighContrastDark || preset == ThemePreset::HighContrastLight) {
            CHECK(textBg >= 12.0f);
            CHECK(!t.shadowsEnabled);
            CHECK(t.focusRingWidth >= 3.0f);
            CHECK(!t.GetShadow(Elevation::Lg).IsVisible());
        } else if (preset == ThemePreset::Windows98) {
            CHECK(t.mode == ThemeMode::Light);
            CHECK(t.IsBevel());
            CHECK(!t.shadowsEnabled);
            CHECK(!t.GetShadow(Elevation::Lg).IsVisible());
            CHECK(t.GetMetrics(UiSize::Md).cornerRadius == 0.0f);
            CHECK(t.GetRadius(UiSize::Md) == 0.0f);
        } else {
            CHECK(t.shadowsEnabled);
            CHECK(t.GetShadow(Elevation::Lg).blur > t.GetShadow(Elevation::Sm).blur);
            CHECK(!t.GetShadow(Elevation::None).IsVisible());
        }
    }

    // 3b. Windows98 preset factory, Optional<SurfaceStyle> override, custom IStylePainter & JSON round-trip
    {
        const UiTheme w98 = UiTheme::Windows98();
        CHECK(w98.preset == ThemePreset::Windows98);
        CHECK(w98.surfaceStyle == SurfaceStyle::Bevel);
        CHECK(w98.controlStyle.frameChrome == FrameChrome::Bevel3D);
        CHECK(w98.controlStyle.sliderThumb == SliderThumbShape::Pointer3D);
        CHECK(w98.controlStyle.progressStyle == ProgressFillStyle::Segmented3D);
        CHECK(w98.controlStyle.tabChrome == TabBarChrome::Classic3D);
        CHECK(w98.controlStyle.tabGapOverride == 0.0f);
        CHECK(!w98.controlStyle.pillBadgesAllowed);
        CHECK(w98.GetMetrics(UiSize::Md).cornerRadius == 0.0f);

        // Default ThemeSpec with Windows98 preserves preset's Bevel style
        ThemeSpec w98Spec{};
        w98Spec.base = ThemePreset::Windows98;
        const UiTheme w98Built = UiTheme::Build(w98Spec);
        CHECK(w98Built.surfaceStyle == SurfaceStyle::Bevel);
        CHECK(w98Built.IsBevel());

        // Explicit Solid override on Windows98 switches cleanly to Flat chrome
        ThemeSpec w98SolidSpec{};
        w98SolidSpec.base = ThemePreset::Windows98;
        w98SolidSpec.surfaceStyle = SurfaceStyle::Solid;
        const UiTheme w98Solid = UiTheme::Build(w98SolidSpec);
        CHECK(w98Solid.surfaceStyle == SurfaceStyle::Solid);
        CHECK(!w98Solid.IsBevel());
        CHECK(w98Solid.controlStyle.frameChrome == FrameChrome::Flat);

        const auto w98Parsed = ThemeSpec::FromJson(w98Spec.ToJson());
        CHECK(w98Parsed.has_value() && w98Parsed->base == ThemePreset::Windows98);

        // Custom IStylePainter hook dispatch
        struct CountingPainter final : public DefaultStylePainter {
            mutable keel::i32 frameCalls{0};
            void DrawFrame(ICanvas& c, const UiTheme& t, FrameKind k, const FramePaintParams& p) const override {
                ++frameCalls;
                DefaultStylePainter::DrawFrame(c, t, k, p);
            }
        };
        CountingPainter customPainter{};
        UiTheme customPainted = UiTheme::Windows98();
        customPainted.SetStylePainter(&customPainter);
        CHECK(&customPainted.Painter() == &customPainter);
        NullCanvas nullCanvas{};
        customPainted.Painter().DrawFrame(nullCanvas, customPainted, FrameKind::Button, FramePaintParams{
            .rect = Rect(0.0f, 0.0f, 80.0f, 28.0f),
            .bg = customPainted.palette.surface
        });
        CHECK(customPainter.frameCalls == 1);
    }

    // 4. Declarative ThemeSpec builder, SurfaceStyle, SurfaceContrast, SyntaxPreset & mutators
    ThemeSpec spec{};
    spec.base = ThemePreset::Nord;
    spec.surfaceStyle = SurfaceStyle::Soft;
    spec.syntaxPreset = SyntaxPreset::Monokai;
    spec.accent = Color::Hex(0xF59E0BFF);
    spec.neutral = Color::Hex(0x161922FF);
    spec.background = Color::Hex(0x10131AFF);
    spec.surface = Color::Hex(0x1B202BFF);
    spec.border = Color::Hex(0x334155FF);
    spec.textPrimary = Color::Hex(0xF8FAFCFF);
    spec.success = Color::Hex(0x22C55EFF);
    spec.surfaceContrast = 1.35f;
    spec.radiusScale = 0.0f;
    spec.spacingScale = 1.2f;
    spec.fontScale = 1.25f;
    spec.lineHeightMultiplier = 1.55f;
    spec.borderScale = 2.0f;
    spec.shadowIntensity = 0.0f;
    spec.focusRingWidth = 3.5f;
    spec.disabledOpacity = 0.35f;
    spec.animationScale = 0.0f;
    spec.shellStyle = ShellStyle::Floating;

    const UiTheme nordBase = UiTheme::Nord();
    const UiTheme custom = UiTheme::Build(spec);
    CHECK(custom.surfaceStyle == SurfaceStyle::Soft);
    CHECK(custom.syntaxPreset == SyntaxPreset::Monokai);
    CHECK(custom.syntax.editorBg == SyntaxPalette::Monokai().editorBg);
    CHECK(custom.palette.accent == *spec.accent);
    CHECK(custom.palette.background == *spec.background);
    CHECK(custom.palette.surface == *spec.surface);
    CHECK(custom.palette.border == *spec.border);
    CHECK(custom.palette.textPrimary == *spec.textPrimary);
    CHECK(Color::ContrastRatio(custom.palette.accentText, custom.palette.accent) >= 4.5f);
    CHECK(custom.GetVariant(UiVariant::Primary).borderWidth > 0.0f);
    CHECK(Color::ContrastRatio(custom.GetVariant(UiVariant::Primary).text, custom.GetVariant(UiVariant::Primary).bg) >= 4.5f);
    CHECK(custom.GetMetrics(UiSize::Md).cornerRadius == 0.0f);
    CHECK(!custom.shadowsEnabled);
    CHECK(!custom.GetShadow(Elevation::Lg).IsVisible());
    CHECK(custom.borderScale == 2.0f);
    CHECK(keel::abs(custom.focusRingWidth - 3.5f) < 0.01f);
    CHECK(keel::abs(custom.disabledOpacity - 0.35f) < 0.01f);
    CHECK(custom.animationScale == 0.0f);
    CHECK(keel::abs(custom.lineHeightMultiplier - 1.55f) < 0.01f);
    CHECK(custom.shellStyle == ShellStyle::Floating);
    CHECK(custom.GetTypography(TextRole::BodyMedium).fontSize > nordBase.GetTypography(TextRole::BodyMedium).fontSize * 1.2f);
    CHECK(custom.Spacing().md > nordBase.Spacing().md * 1.15f);

    // 4b. Outline & Brutalist SurfaceStyles + SurfaceContrast separation
    ThemeSpec outlineSpec{};
    outlineSpec.base = ThemePreset::Dark;
    outlineSpec.surfaceStyle = SurfaceStyle::Outline;
    const UiTheme outlined = UiTheme::Build(outlineSpec);
    CHECK(outlined.GetVariant(UiVariant::Primary).bg.a == 0u);
    CHECK(outlined.GetVariant(UiVariant::Primary).borderWidth > 1.0f);
    CHECK(outlined.GetSemanticAccent(UiVariant::Primary).a == 255u);
    CHECK(Color::ContrastRatio(outlined.GetVariant(UiVariant::Primary).text, outlined.palette.surface) >= 4.5f);

    ThemeSpec brutalSpec{};
    brutalSpec.base = ThemePreset::Light;
    brutalSpec.surfaceStyle = SurfaceStyle::Brutalist;
    const UiTheme brutal = UiTheme::Build(brutalSpec);
    CHECK(!brutal.shadowsEnabled);
    CHECK(brutal.GetVariant(UiVariant::Primary).borderWidth >= 1.5f);

    ThemeSpec lowC{};
    lowC.surfaceContrast = 0.6f;
    ThemeSpec highC{};
    highC.surfaceContrast = 1.6f;
    const UiTheme lowCT = UiTheme::Build(lowC);
    const UiTheme highCT = UiTheme::Build(highC);
    const keel::f32 lowDelta = keel::abs(lowCT.palette.surface.RelativeLuminance() - lowCT.palette.background.RelativeLuminance());
    const keel::f32 highDelta = keel::abs(highCT.palette.surface.RelativeLuminance() - highCT.palette.background.RelativeLuminance());
    CHECK(highDelta > lowDelta);

    // 5. ThemeSpec JSON serialization round-trip & malformed input rejection
    const keel::String json = spec.ToJson();
    CHECK(!json.empty());
    const keel::Optional<ThemeSpec> parsedOpt = ThemeSpec::FromJson(json);
    CHECK(parsedOpt.has_value());
    if (parsedOpt.has_value()) {
        const ThemeSpec& parsed = *parsedOpt;
        CHECK(parsed.base == ThemePreset::Nord);
        CHECK(parsed.surfaceStyle.has_value() && *parsed.surfaceStyle == SurfaceStyle::Soft);
        CHECK(parsed.syntaxPreset == SyntaxPreset::Monokai);
        CHECK(parsed.accent.has_value() && *parsed.accent == *spec.accent);
        CHECK(parsed.neutral.has_value() && *parsed.neutral == *spec.neutral);
        CHECK(parsed.background.has_value() && *parsed.background == *spec.background);
        CHECK(parsed.surface.has_value() && *parsed.surface == *spec.surface);
        CHECK(parsed.border.has_value() && *parsed.border == *spec.border);
        CHECK(parsed.textPrimary.has_value() && *parsed.textPrimary == *spec.textPrimary);
        CHECK(parsed.success.has_value() && *parsed.success == *spec.success);
        CHECK(keel::abs(parsed.surfaceContrast - 1.35f) < 0.01f);
        CHECK(keel::abs(parsed.radiusScale - 0.0f) < 0.01f);
        CHECK(keel::abs(parsed.spacingScale - 1.2f) < 0.01f);
        CHECK(keel::abs(parsed.fontScale - 1.25f) < 0.01f);
        CHECK(keel::abs(parsed.lineHeightMultiplier - 1.55f) < 0.01f);
        CHECK(keel::abs(parsed.borderScale - 2.0f) < 0.01f);
        CHECK(keel::abs(parsed.shadowIntensity - 0.0f) < 0.01f);
        CHECK(keel::abs(parsed.focusRingWidth - 3.5f) < 0.01f);
        CHECK(keel::abs(parsed.disabledOpacity - 0.35f) < 0.01f);
        CHECK(keel::abs(parsed.animationScale - 0.0f) < 0.01f);
        CHECK(parsed.shellStyle == ShellStyle::Floating);
    }
    CHECK(!ThemeSpec::FromJson("").has_value());
    CHECK(!ThemeSpec::FromJson("not-json").has_value());
    CHECK(!ThemeSpec::FromJson("{\"base\": ").has_value());

    // 6. Smooth UiTheme::Lerp interpolation
    const UiTheme dark = UiTheme::Dark();
    const UiTheme light = UiTheme::Light();
    const UiTheme mid = UiTheme::Lerp(dark, light, 0.5f);
    CHECK(mid.palette.background.RelativeLuminance() > dark.palette.background.RelativeLuminance());
    CHECK(mid.palette.background.RelativeLuminance() < light.palette.background.RelativeLuminance());
    CHECK(UiTheme::Lerp(dark, light, 0.0f).palette.background == dark.palette.background);
    CHECK(UiTheme::Lerp(dark, light, 1.0f).palette.background == light.palette.background);

    // 7. Shared OS system theme query helper (QueryOsSystemTheme)
    {
        const keel::Optional<ThemeMode> probed = QueryOsSystemTheme();
        (void)probed;
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
        const keel::Optional<keel::String> prevGtkTheme = keel::env_get("GTK_THEME");
        (void)keel::env_set("GTK_THEME", "Adwaita:dark");
        CHECK(QueryOsSystemTheme().has_value() && *QueryOsSystemTheme() == ThemeMode::Dark);
        (void)keel::env_set("GTK_THEME", "Adwaita:light");
        CHECK(QueryOsSystemTheme().has_value() && *QueryOsSystemTheme() == ThemeMode::Light);
        if (prevGtkTheme.has_value()) {
            (void)keel::env_set("GTK_THEME", *prevGtkTheme);
        } else {
            (void)keel::env_unset("GTK_THEME");
        }
#endif
    }
}

void TestScrollbarCustomizationPresetsAndGeometry() {
    using namespace guikit;

    // 1. Default theme uses Modern scrollbar; Windows98 auto-selects ClassicBevel 16dp with stepper buttons
    const UiTheme dark = UiTheme::Dark();
    CHECK(dark.scrollbarPreset == ScrollbarPreset::Auto);
    CHECK(dark.scrollbarStyle.chrome == ScrollbarChromeStyle::Modern);
    CHECK(!dark.scrollbarStyle.showButtons);
    CHECK(keel::abs(dark.GetScrollbar(UiSize::Md).trackWidth - 6.0f) < 0.01f);

    const UiTheme win98 = UiTheme::Windows98();
    CHECK(win98.scrollbarStyle.chrome == ScrollbarChromeStyle::ClassicBevel);
    CHECK(win98.scrollbarStyle.showButtons);
    CHECK(win98.scrollbarStyle.arrowStyle == ScrollbarArrowStyle::Triangle);
    CHECK(win98.scrollbarStyle.showCornerBox);
    CHECK(keel::abs(win98.GetScrollbar(UiSize::Md).trackWidth - 16.0f) < 0.01f);
    CHECK(keel::abs(win98.GetScrollbar(UiSize::Md).margin - 0.0f) < 0.01f);

    // 2. All Windows-era presets produce valid distinct tokens & metrics
    UiTheme t = UiTheme::Light();
    for (keel::usize i = 0; i < kScrollbarPresetCount; ++i) {
        const auto preset = keel::from_underlying<ScrollbarPreset>(i);
        CHECK(!ScrollbarPresetName(preset).empty());
        t.SetScrollbarPreset(preset);
        CHECK(t.scrollbarPreset == preset);
        if (preset == ScrollbarPreset::Classic1988) {
            CHECK(t.scrollbarStyle.chrome == ScrollbarChromeStyle::Outlined);
            CHECK(t.scrollbarStyle.arrowStyle == ScrollbarArrowStyle::Arrow);
            CHECK(t.scrollbarStyle.showButtons);
        } else if (preset == ScrollbarPreset::WindowsXp) {
            CHECK(t.scrollbarStyle.gripStyle == ScrollbarGripStyle::Lines);
            CHECK(t.scrollbarStyle.arrowStyle == ScrollbarArrowStyle::Chevron);
            CHECK(t.scrollbarStyle.showButtons);
        } else if (preset == ScrollbarPreset::Windows7) {
            CHECK(t.scrollbarStyle.gripStyle == ScrollbarGripStyle::Lines);
            CHECK(t.scrollbarStyle.arrowStyle == ScrollbarArrowStyle::Triangle);
            CHECK(t.scrollbarStyle.showButtons);
        } else if (preset == ScrollbarPreset::WindowsFlat) {
            CHECK(t.scrollbarStyle.chrome == ScrollbarChromeStyle::Flat);
            CHECK(t.scrollbarStyle.arrowStyle == ScrollbarArrowStyle::Chevron);
            CHECK(t.scrollbarStyle.showButtons);
        }
    }

    // 3. Explicit scrollbar width scaling
    t.SetScrollbarPreset(ScrollbarPreset::Windows98);
    t.SetScrollbarWidth(20.0f);
    CHECK(keel::abs(t.GetScrollbar(UiSize::Md).trackWidth - 20.0f) < 0.01f);
    CHECK(t.GetScrollbar(UiSize::Sm).trackWidth < t.GetScrollbar(UiSize::Md).trackWidth);
    CHECK(t.GetScrollbar(UiSize::Lg).trackWidth > t.GetScrollbar(UiSize::Md).trackWidth);

    // 4. ThemeSpec JSON round-trip for scrollbarPreset and scrollbarWidth
    {
        ThemeSpec spec{};
        spec.base = ThemePreset::Light;
        spec.scrollbarPreset = ScrollbarPreset::WindowsXp;
        spec.scrollbarWidth = 18.0f;

        const keel::String json = spec.ToJson();
        const auto parsed = ThemeSpec::FromJson(json);
        CHECK(parsed.has_value());
        if (parsed.has_value()) {
            CHECK(parsed->scrollbarPreset.has_value() && *parsed->scrollbarPreset == ScrollbarPreset::WindowsXp);
            CHECK(parsed->scrollbarWidth.has_value() && keel::abs(*parsed->scrollbarWidth - 18.0f) < 0.01f);
        }

        const UiTheme built = UiTheme::Build(spec);
        CHECK(built.scrollbarPreset == ScrollbarPreset::WindowsXp);
        CHECK(built.scrollbarStyle.gripStyle == ScrollbarGripStyle::Lines);
        CHECK(keel::abs(built.GetScrollbar(UiSize::Md).trackWidth - 18.0f) < 0.01f);
    }

    // 5. Scrollbar::ComputeAxis vertical & horizontal with stepper buttons & cornerReserve
    {
        const Rect vp(0.0f, 0.0f, 300.0f, 200.0f);
        const ScrollbarMetrics& m = win98.GetScrollbar(UiSize::Md);
        const ScrollbarGeometry vGeom = Scrollbar::ComputeAxis(
            vp, 800.0f, 150.0f, false, m, win98.scrollbarStyle, 16.0f
        );
        CHECK(vGeom.visible);
        CHECK(vGeom.hasButtons);
        CHECK(keel::abs(vGeom.barRect.h - 184.0f) < 0.01f);
        CHECK(keel::abs(vGeom.startButtonRect.h - 16.0f) < 0.01f);
        CHECK(keel::abs(vGeom.endButtonRect.h - 16.0f) < 0.01f);
        CHECK(keel::abs(vGeom.trackRect.y - vGeom.startButtonRect.Bottom()) < 0.01f);
        CHECK(keel::abs(vGeom.trackRect.Bottom() - vGeom.endButtonRect.y) < 0.01f);
        CHECK(vGeom.thumbRect.y >= vGeom.trackRect.y);
        CHECK(vGeom.thumbRect.Bottom() <= vGeom.trackRect.Bottom());

        const ScrollbarGeometry hGeom = Scrollbar::ComputeAxis(
            vp, 900.0f, 100.0f, true, m, win98.scrollbarStyle, 16.0f
        );
        CHECK(hGeom.visible);
        CHECK(hGeom.hasButtons);
        CHECK(keel::abs(hGeom.barRect.w - 284.0f) < 0.01f);
        CHECK(keel::abs(hGeom.trackRect.x - hGeom.startButtonRect.Right()) < 0.01f);
        CHECK(keel::abs(hGeom.trackRect.Right() - hGeom.endButtonRect.x) < 0.01f);
        CHECK(hGeom.thumbRect.x >= hGeom.trackRect.x);
        CHECK(hGeom.thumbRect.Right() <= hGeom.trackRect.Right());
    }

    // 6. Custom IStylePainter override receives DrawScrollbar & DrawScrollbarCorner
    {
        struct SpyScrollbarPainter final : public DefaultStylePainter {
            mutable keel::i32 scrollbarCalls{0};
            mutable keel::i32 cornerCalls{0};
            void DrawScrollbar(ICanvas& c, const UiTheme& th, const ScrollbarPaintParams& p) const override {
                ++scrollbarCalls;
                DefaultStylePainter::DrawScrollbar(c, th, p);
            }
            void DrawScrollbarCorner(ICanvas& c, const UiTheme& th, Rect r) const override {
                ++cornerCalls;
                DefaultStylePainter::DrawScrollbarCorner(c, th, r);
            }
        };

        SpyScrollbarPainter spy{};
        UiTheme customTheme = UiTheme::Windows98();
        customTheme.stylePainter = &spy;
        NullCanvas nullCanvas{};
        const ScrollbarGeometry g = Scrollbar::ComputeAxis(
            Rect(0.0f, 0.0f, 200.0f, 200.0f), 600.0f, 50.0f, false,
            customTheme.GetScrollbar(UiSize::Md), customTheme.scrollbarStyle
        );
        Scrollbar::DrawAxis(
            nullCanvas, g, false, true, false,
            ScrollbarPart::StartButton, ScrollbarPart::None,
            customTheme.GetScrollbar(UiSize::Md), customTheme
        );
        customTheme.Painter().DrawScrollbarCorner(nullCanvas, customTheme, Rect(184.0f, 184.0f, 16.0f, 16.0f));
        CHECK(spy.scrollbarCalls == 1);
        CHECK(spy.cornerCalls == 1);
    }
}

int main() {
    keel::println("[RUN] Testing GuiKit Core Foundation...");

    TestGeometryAndTypes();
    TestIdGeneration();
    TestBumpAllocator();
    TestWidgetResponse();
    TestFrameMetrics();
    TestNodeFootprint();
    TestCarouselMetricsTokens();
    TestSliderMetricsTokens();
    TestHeadingLevelMetricsTokens();
    TestComputeBoxFit();
    TestTextWrapForceSplitLastWord();
    TestStandaloneCoreModuleZeroDependencies();
    TestEmbeddedAndRuntimeAssets();
    TestUiVectorAndArenaVector();
    TestMainWindowHeadlessBackend();
    TestKeelIntegration();
    TestThemeTokensPaddingAndDateTime();
    TestElementHeadingSpecAndSeparatorSpace();
    TestMdRefAndPixelRef();
    TestBuiltInImageDecoding();
    TestThemeSystemColorSciencePresetsSpecJsonAndLerp();
    TestScrollbarCustomizationPresetsAndGeometry();

    if (g_failures == 0) {
        keel::println("[PASS] All GuiKit Core tests passed successfully!");
        return 0;
    } else {
        keel::eprintln("[FAIL] Total failures: {}", g_failures);
        return 1;
    }
}
