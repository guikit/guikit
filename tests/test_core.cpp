#include <iostream>
#include <unordered_set>
#include "guikit/core/Allocator.hpp"
#include "guikit/core/FrameMetrics.hpp"
#include "guikit/core/Id.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"
#include "guikit/geo/MapGrid.hpp"

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

    BumpAllocator allocator(1024); // 1 KB chunks for testing

    struct SampleData {
        int a;
        double b;
        std::string_view tag;
    };

    SampleData* obj1 = allocator.Allocate<SampleData>(10, 3.14, "hello");
    CHECK(obj1 != nullptr);
    CHECK(obj1->a == 10);
    CHECK(reinterpret_cast<uintptr_t>(obj1) % alignof(SampleData) == 0);

    SampleData* obj2 = allocator.Allocate<SampleData>(20, 6.28, "world");
    CHECK(obj2 != nullptr);
    CHECK(obj2->a == 20);
    CHECK(obj1 != obj2);

    // Allocate enough objects to cross chunk boundary
    for (int i = 0; i < 100; ++i) {
        SampleData* obj = allocator.Allocate<SampleData>(i, static_cast<double>(i), "loop");
        CHECK(obj != nullptr);
        CHECK(obj->a == i);
    }

    CHECK(allocator.GetTotalAllocatedBytes() > 1024);

    // Reset should zero out offset without crashing or leaking
    allocator.Reset();
    CHECK(allocator.GetTotalAllocatedBytes() == 0);

    // Reallocation after reset
    SampleData* objAfterReset = allocator.Allocate<SampleData>(99, 1.0, "reset");
    CHECK(objAfterReset != nullptr);
    CHECK(objAfterReset->a == 99);
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

    std::string_view status = metrics.GetStatusString();
    CHECK(!status.empty());
    CHECK(status.find("FPS") != std::string_view::npos);
    CHECK(status.find("CPU:") != std::string_view::npos);
    CHECK(status.find("GPU:") != std::string_view::npos);
    CHECK(status.find("budget") != std::string_view::npos);
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
    const std::string dmmStr = MapGrid::FormatCoord(helgoland, GeoCoordFormat::NauticalDMM);
    CHECK(dmmStr.find("54") != std::string::npos);
    CHECK(dmmStr.find("007") != std::string::npos);
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
    TestComputeBoxFit();
    TestMapGridAndProjections();
    TestTextWrapForceSplitLastWord();

    if (g_failures == 0) {
        std::cout << "[PASS] All GuiKit Core tests passed successfully!\n";
        return 0;
    } else {
        std::cerr << "[FAIL] Total failures: " << g_failures << "\n";
        return 1;
    }
}
