#include <iostream>
#include <unordered_set>
#include "guikit/core/BumpAllocator.hpp"
#include "guikit/core/FrameMetrics.hpp"
#include "guikit/core/Id.hpp"
#include "guikit/core/Response.hpp"
#include "guikit/core/Theme.hpp"
#include "guikit/core/Types.hpp"
#include "guikit/layout/Node.hpp"

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
    CHECK(sizeof(Node) <= 248); // Compact 248 bytes with 4 polymorphic delegates (reduced from ~500 bytes)

    // Verify generic payload casting
    Node n;
    int dummyPayload = 42;
    n.payload = &dummyPayload;
    CHECK(*n.As<int>() == 42);

    const Node& constN = n;
    CHECK(*constN.As<int>() == 42);
}

int main() {
    std::cout << "[RUN] Testing GuiKit Core Foundation...\n";

    TestGeometryAndTypes();
    TestIdGeneration();
    TestBumpAllocator();
    TestWidgetResponse();
    TestFrameMetrics();
    TestNodeFootprint();

    if (g_failures == 0) {
        std::cout << "[PASS] All GuiKit Core tests passed successfully!\n";
        return 0;
    } else {
        std::cerr << "[FAIL] Total failures: " << g_failures << "\n";
        return 1;
    }
}
