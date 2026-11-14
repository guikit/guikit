#include <cstring>
#include <iostream>
#include <thread>
#include <unordered_set>
#include "guikit/core.hpp"
#include "guikit/geo/map_grid.hpp"
#include "keel/keel.h"

static int g_failures = 0;

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            std::cerr << "[FAIL] " << #cond << " (" << __FILE__ << ":" << __LINE__   \
                      << ")\n";                                                      \
            ++g_failures;                                                            \
        }                                                                            \
    } while (0)

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
    CHECK(theme.GetGap(UiSize::Sm) == 8.0f);
    CHECK(theme.GetGap(UiSize::Lg) == 16.0f);
    CHECK(theme.GetRadius(UiSize::Md) == 6.0f);
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

    Padding pEmptyResolved = theme.ResolvePadding(std::nullopt);
    CHECK((pEmptyResolved == Padding{0.0f, 0.0f, 0.0f, 0.0f}));

    Padding pCardResolved = theme.ResolvePadding(std::nullopt, UiSize::Md);
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
    Id childA = MakeId(0, std::source_location::current(), parentA);
    Id childB = MakeId(0, std::source_location::current(), parentB);
    CHECK(childA != childB);

    // Hash map compatibility
    std::unordered_set<Id> idSet;
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
        int a;
        double b;
        std::string_view tag;
    };

    SampleData* obj1 = allocator.create<SampleData>(10, 3.14, "hello");
    CHECK(obj1 != nullptr);
    CHECK(obj1->a == 10);
    CHECK(reinterpret_cast<uintptr_t>(obj1) % alignof(SampleData) == 0);

    SampleData* obj2 = allocator.create<SampleData>(20, 6.28, "world");
    CHECK(obj2 != nullptr);
    CHECK(obj2->a == 20);
    CHECK(obj1 != obj2);

    // Allocate enough objects to cross chunk boundary
    for (int i = 0; i < 100; ++i) {
        SampleData* obj = allocator.create<SampleData>(i, static_cast<double>(i), "loop");
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
    std::memset(oversizedInChunk1, 0xAB, 4096);

    // Non-trivially destructible objects & arrays must invoke ~T() in LIFO order on reset() and ~Arena()
    std::vector<int> dtorOrder;
    struct TrackedNonTrivial {
        std::vector<int>* log{nullptr};
        int id{0};
        std::vector<int> ownedHeap{1, 2, 3, 4};

        TrackedNonTrivial() = default;
        TrackedNonTrivial(std::vector<int>* l, int i) : log(l), id(i) {}
        ~TrackedNonTrivial() {
            if (log) {
                log->push_back(id);
            }
        }
    };
    static_assert(!std::is_trivially_destructible_v<TrackedNonTrivial>);

    {
        keel::Arena scopedAlloc(512);
        auto* n1 = scopedAlloc.create<TrackedNonTrivial>(&dtorOrder, 1);
        auto* n2 = scopedAlloc.create<TrackedNonTrivial>(&dtorOrder, 2);
        auto* arr = scopedAlloc.create_array<TrackedNonTrivial>(3);
        for (int i = 0; i < 3; ++i) {
            arr[i].log = &dtorOrder;
            arr[i].id = 10 + i;
        }
        CHECK(n1->ownedHeap.size() == 4);
        CHECK(n2->ownedHeap.size() == 4);
        CHECK(dtorOrder.empty());

        scopedAlloc.reset();
        // LIFO destruction: arr[2] (12), arr[1] (11), arr[0] (10), then n2 (2), then n1 (1)
        const std::vector<int> expectedAfterReset = {12, 11, 10, 2, 1};
        CHECK(dtorOrder == expectedAfterReset);

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
    for (int i = 0; i < 5; ++i) {
        metrics.BeginFrame();
        // Simulate small CPU work
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        metrics.MarkCpuDone();

        // Simulate small GPU work
        metrics.MarkGpuStart();
        std::this_thread::sleep_for(std::chrono::microseconds(500));
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

    std::string_view status = metrics.GetStatusString();
    CHECK(!status.empty());
    CHECK(status.find("FPS") != std::string_view::npos);
    CHECK(status.find("CPU:") != std::string_view::npos);
    CHECK(status.find("GPU:") != std::string_view::npos);
    CHECK(status.find("budget") != std::string_view::npos);
    CHECK(status.find("RAM:") != std::string_view::npos);

    // Simulate active presented frames followed by >250ms reactive sleep (idle heartbeat)
    for (int i = 0; i < 4; ++i) {
        metrics.BeginFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        metrics.MarkCpuDone();
        metrics.MarkGpuStart();
        metrics.MarkGpuDone();
        metrics.MarkFramePresented();
        metrics.EndFrame(60.0f);
    }
    CHECK(!metrics.IsIdle());

    // Sleep >250ms to simulate reactive idle heartbeat wait
    std::this_thread::sleep_for(std::chrono::milliseconds(260));
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
    std::cout << "[INFO] sizeof(Node) = " << sizeof(Node) << " bytes\n";
    CHECK(sizeof(Node) <= 256); // Compact <=256 bytes with 4 polymorphic delegates + aspect-ratio/minHeight/2D scroll state

    // Verify generic payload casting
    Node n;
    int dummyPayload = 42;
    n.payload = &dummyPayload;
    CHECK(*n.As<int>() == 42);

    const Node& constN = n;
    CHECK(*constN.As<int>() == 42);
}

void TestCarouselMetricsTokens() {
    using namespace guikit;

    UiTheme theme = UiTheme::Dark();

    // 1. Standard metrics baseline checks
    const auto& md = theme.GetCarousel(UiSize::Md);
    CHECK(md.arrowButtonSize == 26.0f);
    CHECK(md.iconSize == 14.0f);
    CHECK(md.cornerRadius == 6.0f);
    CHECK(md.margin == 4.0f);
    CHECK(md.gap == 6.0f);
    CHECK(md.gutter == 36.0f);
    CHECK(md.gutter == md.margin + md.arrowButtonSize + md.gap);
    CHECK(md.scrollStep == 110.0f);

    const auto& xs = theme.GetCarousel(UiSize::Xs);
    CHECK(xs.arrowButtonSize == 18.0f);
    CHECK(xs.iconSize == 10.0f);
    CHECK(xs.gutter == 24.0f);
    CHECK(xs.gutter == xs.margin + xs.arrowButtonSize + xs.gap);

    const auto& sm = theme.GetCarousel(UiSize::Sm);
    CHECK(sm.arrowButtonSize == 22.0f);
    CHECK(sm.iconSize == 12.0f);
    CHECK(sm.gutter == 30.0f);

    const auto& lg = theme.GetCarousel(UiSize::Lg);
    CHECK(lg.arrowButtonSize == 32.0f);
    CHECK(lg.iconSize == 16.0f);
    CHECK(lg.gutter == 45.0f);

    const auto& xl = theme.GetCarousel(UiSize::Xl);
    CHECK(xl.arrowButtonSize == 40.0f);
    CHECK(xl.iconSize == 20.0f);
    CHECK(xl.gutter == 56.0f);

    // Fallback to Md on None
    const auto& noneSize = theme.GetCarousel(UiSize::None);
    CHECK(noneSize.arrowButtonSize == md.arrowButtonSize);

    // 2. Mutable accessor check
    theme.GetCarousel(UiSize::Md).scrollStep = 150.0f;
    CHECK(theme.GetCarousel(UiSize::Md).scrollStep == 150.0f);

    // 3. Density presets
    UiTheme compactTheme = UiTheme::Dark(Density::Compact);
    CHECK(compactTheme.GetCarousel(UiSize::Md).arrowButtonSize == 24.0f);
    CHECK(compactTheme.GetCarousel(UiSize::Md).iconSize == 13.0f);

    UiTheme touchTheme = UiTheme::Dark(Density::Touch);
    CHECK(touchTheme.GetCarousel(UiSize::Md).arrowButtonSize == 36.0f);
    CHECK(touchTheme.GetCarousel(UiSize::Md).iconSize == 18.0f);

    // 4. Scaling
    UiTheme scaledTheme = UiTheme::Dark().Scaled(2.0f);
    const auto& scaledMd = scaledTheme.GetCarousel(UiSize::Md);
    CHECK(scaledMd.arrowButtonSize == 52.0f);
    CHECK(scaledMd.iconSize == 28.0f);
    CHECK(scaledMd.cornerRadius == 12.0f);
    CHECK(scaledMd.margin == 8.0f);
    CHECK(scaledMd.gap == 12.0f);
    CHECK(scaledMd.gutter == 72.0f);
    CHECK(scaledMd.scrollStep == 220.0f);
}

void TestSliderMetricsTokens() {
    using namespace guikit;

    UiTheme theme = UiTheme::Dark();

    // 1. Standard metrics baseline checks
    const auto& md = theme.GetSlider(UiSize::Md);
    CHECK(md.defaultHeight == 26.0f);
    CHECK(md.trackHeight == 5.0f);
    CHECK(md.thumbSize == 18.0f);
    CHECK(md.cornerRadius == 9.0f);
    CHECK(md.tickSize == 4.0f);

    const auto& xs = theme.GetSlider(UiSize::Xs);
    CHECK(xs.defaultHeight == 18.0f);
    CHECK(xs.thumbSize == 12.0f);

    // 2. Mutable accessor check
    theme.GetSlider(UiSize::Md).trackHeight = 7.0f;
    CHECK(theme.GetSlider(UiSize::Md).trackHeight == 7.0f);

    // 3. Density presets
    UiTheme compactTheme = UiTheme::Dark(Density::Compact);
    CHECK(compactTheme.GetSlider(UiSize::Md).thumbSize == 16.0f);

    UiTheme touchTheme = UiTheme::Dark(Density::Touch);
    CHECK(touchTheme.GetSlider(UiSize::Md).thumbSize == 26.0f);

    // 4. Scaling
    UiTheme scaledTheme = UiTheme::Dark().Scaled(2.0f);
    const auto& scaledMd = scaledTheme.GetSlider(UiSize::Md);
    CHECK(scaledMd.defaultHeight == 52.0f);
    CHECK(scaledMd.thumbSize == 36.0f);
    CHECK(scaledMd.trackHeight == 10.0f);
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

    // Auto level delegates to active UiSize
    CHECK(theme.GetHeading(HeadingLevel::Auto, UiSize::Lg).titleFontSize == theme.GetHeading(UiSize::Lg).titleFontSize);
    CHECK(theme.GetHeading(HeadingLevel::Auto, UiSize::Xs).titleFontSize == theme.GetHeading(UiSize::Xs).titleFontSize);

    // 2. Mutable accessor check
    theme.GetHeading(HeadingLevel::H1).titleFontSize = 30.0f;
    CHECK(theme.GetHeading(HeadingLevel::H1).titleFontSize == 30.0f);

    // 3. Density presets (Compact < Standard < Touch)
    const UiTheme compactTheme = UiTheme::Dark(Density::Compact);
    const UiTheme stdTheme = UiTheme::Dark(Density::Standard);
    const UiTheme touchTheme = UiTheme::Dark(Density::Touch);
    CHECK(compactTheme.GetHeading(HeadingLevel::H1).titleFontSize < stdTheme.GetHeading(HeadingLevel::H1).titleFontSize);
    CHECK(stdTheme.GetHeading(HeadingLevel::H1).titleFontSize < touchTheme.GetHeading(HeadingLevel::H1).titleFontSize);
    CHECK(compactTheme.GetHeading(HeadingLevel::H6).titleFontSize < stdTheme.GetHeading(HeadingLevel::H6).titleFontSize);
    CHECK(stdTheme.GetHeading(HeadingLevel::H6).titleFontSize < touchTheme.GetHeading(HeadingLevel::H6).titleFontSize);

    // 4. Scaling
    const UiTheme scaledTheme = stdTheme.Scaled(2.0f);
    CHECK(scaledTheme.GetHeading(HeadingLevel::H1).titleFontSize == stdTheme.GetHeading(HeadingLevel::H1).titleFontSize * 2.0f);
    CHECK(scaledTheme.GetHeading(HeadingLevel::H6).titleFontSize == stdTheme.GetHeading(HeadingLevel::H6).titleFontSize * 2.0f);
    CHECK(scaledTheme.GetHeading(HeadingLevel::H1).iconSize == stdTheme.GetHeading(HeadingLevel::H1).iconSize * 2.0f);

    // 5. HeadingDivider value-type semantics & deterministic hashing
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

void TestMapGridAndProjections() {
    using namespace guikit;

    // 1. Round-trip Geographic <-> Normalized (u, v) across all 3 CRS projections
    const GeoCoord helgoland{54.1833, 7.8833};
    for (MapCrs crs : {MapCrs::EPSG3857_WebMercator,
                       MapCrs::EPSG3395_WorldMercator,
                       MapCrs::EPSG4326_PlateCarree}) {
        const Vec2 uv = MapGrid::GeoToNormalized(helgoland, crs);
        CHECK(uv.x > 0.5f && uv.x < 0.6f);
        CHECK(uv.y > 0.1f && uv.y < 0.45f);
        const GeoCoord back = MapGrid::NormalizedToGeo(uv, crs);
        CHECK(std::abs(back.lat - helgoland.lat) < 1e-3);
        CHECK(std::abs(back.lon - helgoland.lon) < 1e-3);
    }

    // 2. Noticeable difference between Spherical (EPSG:3857) and WGS84 Ellipsoidal (EPSG:3395) Mercator Y
    const auto [u3857, v3857] = MapGrid::GeoToNormalized(helgoland, MapCrs::EPSG3857_WebMercator);
    const auto [u3395, v3395] = MapGrid::GeoToNormalized(helgoland, MapCrs::EPSG3395_WorldMercator);
    CHECK(std::abs(u3857 - u3395) < 1e-9);
    CHECK(std::abs(v3857 - v3395) > 5e-4);

    // 3. Tile ID & View State calculation
    MapGridConfig cfg{
        .crs = MapCrs::EPSG3857_WebMercator,
        .worldSize = 2048.0f,
        .baseZoom = 3,
        .minTileZoom = 1,
        .maxTileZoom = 18
    };
    const Vec2 helgolandWorld = MapGrid::GeoToWorld(helgoland, cfg.worldSize, cfg.crs);
    const float sceneZoomZ7 = MapGrid::TileZoomToSceneZoom(7.0f, cfg.baseZoom);
    CHECK(std::abs(sceneZoomZ7 - 16.0f) < 1e-4f);

    const Rect visibleWorld{
        helgolandWorld.x - 20.0f,
        helgolandWorld.y - 15.0f,
        40.0f,
        30.0f
    };
    const Rect viewportRect{0.0f, 0.0f, 640.0f, 480.0f};
    const MapViewState view = MapGrid::ComputeView(
        visibleWorld, viewportRect, sceneZoomZ7, helgolandWorld, cfg
    );
    CHECK(view.tileZoom == 7);
    CHECK(!view.visibleTiles.empty());
    CHECK(!view.meridians.empty());
    CHECK(!view.parallels.empty());
    CHECK(std::abs(view.cursor.lat - helgoland.lat) < 1e-2);
    CHECK(std::abs(view.cursor.lon - helgoland.lon) < 1e-2);
    CHECK(view.metricScale.screenPixels > 20.0f);
    CHECK(view.nauticalScale.screenPixels > 20.0f);

    // 4. Nautical DMM formatting check
    const auto dmmStr = MapGrid::FormatCoord(helgoland, GeoCoordFormat::NauticalDMM);
    CHECK(std::string_view(dmmStr).find("54") != std::string_view::npos);
    CHECK(std::string_view(dmmStr).find("007") != std::string_view::npos);
}

void TestTextWrapForceSplitLastWord() {
    using namespace guikit;

    // 1. Single long word at end of string wider than maxWidth
    {
        const std::string_view text = "StretchMode::Fill.";
        const auto lines = TextUtil::WrapText(text, 60.0f, 14.0f, nullptr);
        CHECK(lines.size() >= 2);
        std::string reconstructed;
        for (std::string_view ln : lines) {
            reconstructed.append(ln);
        }
        CHECK(reconstructed == text);
    }

    // 2. Multi-word string where the final word requires force-splitting across narrow widths
    {
        const std::string_view subtitle =
            "Each button automatically receives exactly 33.3% available width via stretch = StretchMode::Fill.";
        for (float maxW = 4.0f; maxW <= 250.0f; maxW += 7.0f) {
            const auto lines = TextUtil::WrapText(subtitle, maxW, 13.0f, nullptr);
            CHECK(!lines.empty());
        }
    }

    // 3. UTF-8 multi-byte string force-split at the end of string
    {
        const std::string_view utf8Text = "Короткий текст СверхдлинноеСловоВКонцеСтроки";
        for (float maxW = 5.0f; maxW <= 200.0f; maxW += 11.0f) {
            const auto lines = TextUtil::WrapText(utf8Text, maxW, 14.0f, nullptr);
            CHECK(!lines.empty());
        }
    }
}

void TestStandaloneCoreModuleZeroDependencies() {
    using namespace guikit;

    class NullCanvas final : public ICanvas {
    public:
        void Begin(uint32_t, uint32_t, float scale = 1.0f) override { m_scale = scale; }
        void End() override {}
        [[nodiscard]] float GetScale() const noexcept override { return m_scale; }
        void SetScale(float scale) override { m_scale = scale; }
        [[nodiscard]] std::string_view GetBackendName() const noexcept override { return "NullCanvas (Zero-Dep)"; }
        void Clear(Color, std::optional<Rect> = std::nullopt) override {}
        void PushClip(Rect) override {}
        void PopClip() override {}
        void DrawRect(Rect, Color) override { ++drawCount; }
        void DrawRoundRect(Rect, float, Color, Color = Color(0, 0, 0, 0), float = 0.0f) override { ++drawCount; }
        Vec2 MeasureText(std::string_view text, float fontSize) override {
            return {static_cast<float>(text.size()) * fontSize * 0.55f, fontSize * 1.2f};
        }
        [[nodiscard]] float GetLineHeight(float fontSize) override { return fontSize * 1.25f; }
        void DrawText(std::string_view, Vec2, float, Color) override { ++drawCount; }
        void DrawTextBox(std::string_view, Rect, float, Color, float = 0.0f, float = 0.5f) override { ++drawCount; }
        void DrawLine(Vec2, Vec2, Color, float = 1.0f) override { ++drawCount; }

        float m_scale{1.0f};
        size_t drawCount{0};
    };

    Context ui;
    NullCanvas canvas;

    float sliderVal = 0.5f;
    bool switchVal = true;
    int selectedRow = 0;

    static constexpr std::array<TableColumn, 2> cols = {{
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
            for (size_t r = table.FirstVisibleRow(); r < table.EndVisibleRow(); ++r) {
                if (auto row = table.Row(r)) {
                    ui.Text(ui.Format("K-{:02}", r));
                    ui.Element({.badge = {"OK", UiVariant::Success}, .stretch = StretchMode::Compact});
                }
            }
        }
    }
    ui.EndFrame(canvas);

    CHECK(canvas.drawCount > 0);
}

void TestEmbeddedAndRuntimeAssets() {
    using namespace guikit;

    // 1. Verify build-time embedded assets (lena.png + Roboto-Regular.ttf)
    CHECK(Assets::Exists("assets/images/lena.png"));
    CHECK(Assets::Exists("../assets/images/lena.png"));
    CHECK(Assets::Exists("images/lena.png"));
    CHECK(Assets::Exists("assets\\images\\lena.png"));
    CHECK(Assets::Exists("assets/fonts/Roboto-Regular.ttf"));
    CHECK(Assets::Exists("assets/geo/demo_bay.geojson"));
    CHECK(!Assets::Exists("assets/images/architecture.svg"));
    CHECK(!Assets::Exists("assets/images/circuit.svg"));

    const auto demoBay = Assets::Find("assets/geo/demo_bay.geojson");
    CHECK(demoBay.has_value());
    if (demoBay.has_value()) {
        CHECK(!demoBay->Empty());
        CHECK(demoBay->size > 300000u);
    }

    const auto lena = Assets::Find("assets/images/lena.png");
    CHECK(lena.has_value());
    if (lena.has_value()) {
        CHECK(!lena->Empty());
        CHECK(lena->mimeType == "png");
        CHECK(lena->size > 1000u);
        // PNG magic signature: 89 50 4E 47
        CHECK(lena->data[0] == 0x89);
        CHECK(lena->data[1] == 0x50);
        CHECK(lena->data[2] == 0x4E);
        CHECK(lena->data[3] == 0x47);
    }

    // 2. Verify runtime asset registration & override cleanup
    std::vector<uint8_t> customSvg = {'<', 's', 'v', 'g', '/', '>'};
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

    static_assert(std::is_trivially_destructible_v<keel::ArenaArray<int>>);
    static_assert(std::is_trivially_destructible_v<keel::ArenaArray<Rect>>);
    static_assert(!std::is_trivially_destructible_v<keel::ArenaArray<std::string>>);
    static_assert(std::is_trivially_destructible_v<ScenePayload>);

    // 1. keel::SmallArray inline SBO storage & no self-referential pointer on clone/move
    {
        keel::SmallArray<int, 4> vec;
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
        keel::SmallArray<int, 4> copyVec = vec.clone();
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
        const int* heapPtr = vec.data();
        keel::SmallArray<int, 4> movedVec = std::move(vec);
        CHECK(!movedVec.is_inline());
        CHECK(movedVec.data() == heapPtr);
        CHECK(movedVec.size() == 5);
        CHECK(vec.empty());
        CHECK(vec.is_inline());

        // Span view
        auto sumSpan = [](std::span<const int> sp) {
            int s = 0;
            for (int v : sp) s += v;
            return s;
        };
        CHECK(sumSpan(movedVec) == (10 + 20 + 30 + 40 + 10));
    }

    // 2. Non-trivial element lifetime in keel::SmallArray
    {
        int liveCount = 0;
        struct TrackedItem {
            int* counter{nullptr};
            int value{0};
            TrackedItem() = default;
            TrackedItem(int* c, int v) : counter(c), value(v) {
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
        keel::ArenaArray<int> arenaVec(arena, 8);
        const int* initialPtr = arenaVec.data();
        CHECK(initialPtr != nullptr);
        CHECK(arenaVec.capacity() == 8);

        // Push 64 elements while nothing else allocates from `arena`:
        // try_extend extends the buffer in-place without moving `data()`!
        for (int i = 0; i < 64; ++i) {
            arenaVec.push_back(i * 3);
        }
        CHECK(arenaVec.size() == 64);
        CHECK(arenaVec.data() == initialPtr);
        keel::ArrayRef<int> arenaRef = arenaVec;
        CHECK(arenaRef.size() == 64);
        for (int i = 0; i < 64; ++i) {
            CHECK(arenaVec[i] == i * 3);
        }

        // Allocate something else on the arena so tip moves, then grow arenaVec again
        int* separator = arena.create<int>(777);
        CHECK(*separator == 777);
        for (int i = 64; i < 150; ++i) {
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
        void Begin(uint32_t, uint32_t, float scale = 1.0f) override {
            m_scale = scale;
            m_passes.clear();
        }
        void End() override {}
        [[nodiscard]] float GetScale() const noexcept override { return m_scale; }
        void SetScale(float scale) override { m_scale = scale; }
        [[nodiscard]] std::string_view GetBackendName() const noexcept override { return "HeadlessCanvas"; }
        void Clear(Color, std::optional<Rect> = std::nullopt) override {}
        void PushClip(Rect) override {}
        void PopClip() override {}
        void DrawRect(Rect, Color) override { ++drawCount; }
        void DrawRoundRect(Rect, float, Color, Color = Color(0, 0, 0, 0), float = 0.0f) override { ++drawCount; }
        Vec2 MeasureText(std::string_view text, float fontSize) override {
            return {static_cast<float>(text.size()) * fontSize * 0.55f, fontSize * 1.2f};
        }
        [[nodiscard]] float GetLineHeight(float fontSize) override { return fontSize * 1.25f; }
        void DrawText(std::string_view, Vec2, float, Color) override { ++drawCount; }
        void DrawTextBox(std::string_view, Rect, float, Color, float = 0.0f, float = 0.5f) override { ++drawCount; }
        void DrawLine(Vec2, Vec2, Color, float = 1.0f) override { ++drawCount; }
        bool DrawGpuShaderPass(const GpuShaderPassDesc& desc, bool = true) override {
            m_passes.push_back(desc);
            ++drawCount;
            return true;
        }
        [[nodiscard]] const std::vector<GpuShaderPassDesc>& GetGpuShaderPasses() const noexcept override {
            return m_passes;
        }

        float m_scale{1.0f};
        size_t drawCount{0};
        std::vector<GpuShaderPassDesc> m_passes;
    };

    class MockWindowBackend final : public IWindowBackend {
    public:
        bool Initialize(const MainWindowConfig& cfg, Context&, const WindowHostCallbacks&) override {
            m_cfg = cfg;
            initialized = true;
            return true;
        }
        void Shutdown() noexcept override {
            shutdownCalled = true;
        }
        bool PumpEvents(Context&, bool, int) override {
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
        int GetWidth() const noexcept override { return m_cfg.width; }
        int GetHeight() const noexcept override { return m_cfg.height; }
        float GetDisplayPixelsPerMm() const noexcept override { return 4.0f; }
        bool IsMinimized() const noexcept override { return false; }
        bool IsLiveResizeFrame() const noexcept override { return false; }
        bool IsVsyncEnabled() const noexcept override { return m_vsync; }
        void SetVsyncEnabled(bool enabled) noexcept override { m_vsync = enabled; }
        bool IsGpuShadersEnabled() const noexcept override { return m_gpuShaders; }
        bool IsGpuShadersAvailable() const noexcept override { return true; }
        void SetGpuShadersEnabled(bool enabled) noexcept override { m_gpuShaders = enabled; }
        void UpdateSystemCursor(CursorType cursor) noexcept override { lastCursor = cursor; }
        void OpenUrl(std::string_view) const noexcept override {}

        MainWindowConfig m_cfg{};
        HeadlessCanvas m_canvas{};
        bool m_vsync{true};
        bool m_gpuShaders{true};
        CursorType lastCursor{CursorType::Default};
        bool initialized{false};
        bool shutdownCalled{false};
        int framesPresented{0};
    };

    class TestAppWindow final : public MainWindow {
    public:
        using MainWindow::MainWindow;

        bool initCalled{false};
        bool shutdownCalled{false};
        int guiCount{0};

    protected:
        void OnInit() override {
            initCalled = true;
        }
        void OnRender(Context& ui) override {
            ++guiCount;
            if (auto tb = ui.Toolbar()) {
                RenderThemeControls(tb);
                RenderDebugControls(tb);
                RenderEngineControls(tb);
            }
            static constexpr std::array<CommandPaletteItem, 1> kCmds = {{
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

    auto backend = std::make_unique<MockWindowBackend>();
    MockWindowBackend* rawBackend = backend.get();

    TestAppWindow app({.title = "Headless Test", .width = 1000, .height = 800, .reactiveMode = false}, std::move(backend));
    const int exitCode = app.Run();
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

    // 2. keel::Arena::allocator() arena bridge
    keel::Arena frameArena(4096);
    keel::Array<int> arenaArray(frameArena.allocator());
    for (int i = 0; i < 32; ++i) {
        arenaArray.push_back(i * 5);
    }
    CHECK(arenaArray.size() == 32);
    CHECK(arenaArray[10] == 50);
    CHECK(frameArena.used() > 0);

    // 3. keel::FixedString <-> keel::StringRef & keel::SmallArray <-> keel::ArrayRef
    keel::FixedString<> label(keel::StringRef("Keel Bridge"));
    keel::StringRef ref = label.as_ref();
    CHECK(ref == "Keel Bridge");

    const int rawVals[4] = {1, 2, 3, 4};
    keel::SmallArray<int, 4> uvec(keel::ArrayRef<int>(rawVals, 4));
    keel::ArrayRef<int> aref = uvec;
    CHECK(aref.size() == 4);
    CHECK(aref[2] == 3);
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
}

int main() {
    std::cout << "[RUN] Testing GuiKit Core Foundation...\n";

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
    TestMapGridAndProjections();
    TestTextWrapForceSplitLastWord();
    TestStandaloneCoreModuleZeroDependencies();
    TestEmbeddedAndRuntimeAssets();
    TestUiVectorAndArenaVector();
    TestMainWindowHeadlessBackend();
    TestKeelIntegration();
    TestThemeTokensPaddingAndDateTime();

    if (g_failures == 0) {
        std::cout << "[PASS] All GuiKit Core tests passed successfully!\n";
        return 0;
    } else {
        std::cerr << "[FAIL] Total failures: " << g_failures << "\n";
        return 1;
    }
}
