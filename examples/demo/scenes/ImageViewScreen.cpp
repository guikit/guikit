#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>
#include "../DemoContext.hpp"

namespace guikit::demo {

namespace {

constexpr uint32_t kDemoImgW = 1024;
constexpr uint32_t kDemoImgH = 768;

[[nodiscard]] inline float HashNoise2D(uint32_t x, uint32_t y, uint32_t seed) noexcept {
    uint32_t n = x * 374761393u + y * 668265263u + seed * 1442695041u;
    n = (n ^ (n >> 13u)) * 1274126177u;
    n = n ^ (n >> 16u);
    return static_cast<float>(n & 0xFFFFu) / 65535.0f;
}

struct ImageViewDemoState {
    bool initialized{false};
    int datasetIndex{0}; // 0 = 16-bit Radiographic/NDT Phantom (1ch), 1 = Multi-Channel Fluorescence/Spectral (3ch RGB)

    std::vector<uint16_t> mono16Pixels{};
    std::vector<float> rgbFloatPixels{};
    uint64_t generationCounter{1};

    ImageViewParams paramsMono{};
    ImageViewParams paramsRgb{};
    ImageViewOptions viewOpts{
        .heightMode = SizeMode::Stretch,
        .showSidebar = true,
        .sidebarOnRight = false,
        .sidebarWidth = 300.0f,
        .showTopToolbar = true,
        .showStatusBar = true,
        .showPixelGridAtHighZoom = true,
        .enableLineProfile = true,
        .enableCtrlWlDrag = true
    };
    ImageViewResponse lastResp{};

    void EnsureGenerated() {
        if (initialized) {
            return;
        }
        initialized = true;
        GenerateMono16Phantom();
        GenerateMultispectralRgb();
    }

    void GenerateMono16Phantom() {
        mono16Pixels.resize(static_cast<size_t>(kDemoImgW) * kDemoImgH);
        const float cx = kDemoImgW * 0.5f;
        const float cy = kDemoImgH * 0.54f;

        for (uint32_t y = 0; y < kDemoImgH; ++y) {
            const float ny = static_cast<float>(y) / static_cast<float>(kDemoImgH - 1);
            for (uint32_t x = 0; x < kDemoImgW; ++x) {
                const float nx = static_cast<float>(x) / static_cast<float>(kDemoImgW - 1);

                // 1. Smooth background gradient (12% .. 82% of 16-bit range)
                float v = 8000.0f + 22000.0f * (0.55f * nx + 0.45f * ny);

                // 2. Top 10-step optical density calibration wedge
                if (y >= 28 && y <= 104 && x >= 64 && x < 960) {
                    const int step = static_cast<int>((x - 64) * 10 / (960 - 64));
                    v = 4500.0f + static_cast<float>(step) * 5800.0f;
                    // Subtle internal calibration pinholes inside each step
                    const float stepCenterX = 64.0f + (static_cast<float>(step) + 0.5f) * (896.0f / 10.0f);
                    const float dHole = std::hypot(static_cast<float>(x) - stepCenterX, static_cast<float>(y) - 66.0f);
                    if (dHole < 10.0f) {
                        v += 1800.0f * (1.0f - dHole / 10.0f);
                    }
                }

                // 3. Central radiographic assembly housing & concentric attenuation rings
                const float dx = static_cast<float>(x) - cx;
                const float dy = static_cast<float>(y) - cy;
                const float r = std::hypot(dx, dy);
                if (r < 265.0f) {
                    v += 14000.0f * std::cos((r / 265.0f) * 1.570796f);
                    // Annular machined grooves
                    if (r > 195.0f && r < 225.0f) {
                        v -= 6500.0f;
                    } else if (r > 110.0f && r < 135.0f) {
                        v += 7200.0f;
                    }
                    // Radial spokes
                    const float ang = std::atan2(dy, dx);
                    const float spoke = std::cos(ang * 8.0f);
                    if (r > 48.0f && r < 185.0f && spoke > 0.86f) {
                        v += 4200.0f * (spoke - 0.86f) / 0.14f;
                    }
                }

                // 4. High-frequency resolution line-pair bar targets (bottom-left)
                if (x >= 72 && x <= 320 && y >= 540 && y <= 700) {
                    const float freq = 0.15f + 0.85f * (static_cast<float>(x - 72) / 248.0f);
                    const float wave = std::sin(static_cast<float>(x - 72) * freq * 1.3f);
                    v = 26000.0f + (wave >= 0.0f ? 11500.0f : -11500.0f);
                }

                // 5. Low-contrast subtle inclusions array (bottom-right) for W/L inspection
                if (x >= 690 && x <= 955 && y >= 520 && y <= 715) {
                    v = 44000.0f;
                    for (int row = 0; row < 3; ++row) {
                        for (int col = 0; col < 4; ++col) {
                            const float icx = 730.0f + static_cast<float>(col) * 62.0f;
                            const float icy = 555.0f + static_cast<float>(row) * 60.0f;
                            const float rad = 18.0f - static_cast<float>(row) * 4.0f;
                            const float d = std::hypot(static_cast<float>(x) - icx, static_cast<float>(y) - icy);
                            if (d < rad) {
                                const float contrast = (800.0f + static_cast<float>(col) * 1400.0f);
                                v += contrast * (1.0f - (d * d) / (rad * rad));
                            }
                        }
                    }
                }

                // 6. Sensor quantum noise
                const float noise = (HashNoise2D(x, y, 1337u) + HashNoise2D(x, y, 7919u) - 1.0f) * 480.0f;
                const float clamped = std::clamp(std::round(v + noise), 0.0f, 65535.0f);
                mono16Pixels[static_cast<size_t>(y) * kDemoImgW + x] = static_cast<uint16_t>(clamped);
            }
        }
    }

    void GenerateMultispectralRgb() {
        rgbFloatPixels.resize(static_cast<size_t>(kDemoImgW) * kDemoImgH * 3u);
        const float cx = kDemoImgW * 0.5f;
        const float cy = kDemoImgH * 0.52f;

        for (uint32_t y = 0; y < kDemoImgH; ++y) {
            const float ny = static_cast<float>(y) / static_cast<float>(kDemoImgH - 1);
            for (uint32_t x = 0; x < kDemoImgW; ++x) {
                const float nx = static_cast<float>(x) / static_cast<float>(kDemoImgW - 1);
                const float dx = static_cast<float>(x) - cx;
                const float dy = static_cast<float>(y) - cy;
                const float r = std::hypot(dx, dy);
                const float ang = std::atan2(dy, dx);

                // Base background with slight channel offset (ideal for testing per-channel histogram & Auto Balance)
                float red   = 180.0f + 420.0f * nx;
                float green = 140.0f + 380.0f * ny;
                float blue  = 260.0f + 320.0f * (1.0f - 0.5f * (nx + ny));

                // Red Channel: outer filament ring & vascular radial web
                const float ringEnv = std::exp(-((r - 220.0f) * (r - 220.0f)) / (2.0f * 42.0f * 42.0f));
                const float web = 0.5f + 0.5f * std::sin(ang * 12.0f + r * 0.045f);
                red += 2850.0f * ringEnv * (0.35f + 0.65f * web);

                // Green Channel: cellular organelle clusters & top calibration bars
                for (int k = 0; k < 6; ++k) {
                    const float a = static_cast<float>(k) * (2.0f * std::numbers::pi_v<float> / 6.0f);
                    const float ox = cx + std::cos(a) * 135.0f;
                    const float oy = cy + std::sin(a) * 135.0f;
                    const float od = std::hypot(static_cast<float>(x) - ox, static_cast<float>(y) - oy);
                    green += 2700.0f * std::exp(-(od * od) / (2.0f * 36.0f * 36.0f));
                }

                // Blue Channel: bright central core & outer calibration beacons
                blue += 3300.0f * std::exp(-(r * r) / (2.0f * 68.0f * 68.0f));

                // Top RGB calibration step wedge
                if (y >= 24 && y <= 84 && x >= 64 && x < 960) {
                    const float u = static_cast<float>(x - 64) / 896.0f;
                    if (y < 44) {
                        red = u * 3800.0f; green = 120.0f; blue = 120.0f;
                    } else if (y < 64) {
                        red = 120.0f; green = u * 3800.0f; blue = 120.0f;
                    } else {
                        red = 120.0f; green = 120.0f; blue = u * 3800.0f;
                    }
                }

                const float nR = (HashNoise2D(x, y, 101u) - 0.5f) * 75.0f;
                const float nG = (HashNoise2D(x, y, 202u) - 0.5f) * 75.0f;
                const float nB = (HashNoise2D(x, y, 303u) - 0.5f) * 75.0f;

                const size_t base = (static_cast<size_t>(y) * kDemoImgW + x) * 3u;
                rgbFloatPixels[base + 0] = std::clamp(red + nR,   0.0f, 4095.0f);
                rgbFloatPixels[base + 1] = std::clamp(green + nG, 0.0f, 4095.0f);
                rgbFloatPixels[base + 2] = std::clamp(blue + nB,  0.0f, 4095.0f);
            }
        }
    }
};

ImageViewDemoState s_imgDemo{};

} // namespace

void RenderImageViewInspector(guikit::Context& ui, DemoContext& /*ctx*/) {
    s_imgDemo.EnsureGenerated();

    ui.Heading({.title = "DATASET & CHANNELS", .size = UiSize::Xs, .divider = true});
    static constexpr std::array<std::string_view, 2> kDatasetNames = {
        "16-bit Radiograph Phantom (1ch)",
        "Multispectral Fluorescence (3ch RGB)"
    };
    if (ui.Select(s_imgDemo.datasetIndex, kDatasetNames, {.label = "Synthetic Dataset"}).changed) {
        ui.InvalidateAll();
    }

    ImageViewParams& activeParams = (s_imgDemo.datasetIndex == 0) ? s_imgDemo.paramsMono : s_imgDemo.paramsRgb;

    static constexpr std::array<std::string_view, 3> kHistScales = {"Sqrt", "Log", "Linear"};
    int scaleIdx = static_cast<int>(activeParams.histScaleMode);
    if (ui.StepSlider(scaleIdx, kHistScales, {.label = "Histogram Scale", .showValue = false}).changed) {
        activeParams.histScaleMode = static_cast<HistogramScaleMode>(scaleIdx);
        ui.InvalidateAll();
    }

    ui.Heading({.title = "WORKSTATION LAYOUT", .size = UiSize::Xs, .divider = true});
    if (ui.Switch(s_imgDemo.viewOpts.showSidebar, {.label = "Show Built-in Sidebar"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_imgDemo.viewOpts.sidebarOnRight, {
        .label = "Sidebar on Right",
        .disabled = !s_imgDemo.viewOpts.showSidebar
    }).changed) {
        ui.InvalidateAll();
    }
    if (ui.Slider(s_imgDemo.viewOpts.sidebarWidth, 240.0f, 380.0f, {
        .label = "Sidebar Width",
        .valueFormat = "%.0f px",
        .disabled = !s_imgDemo.viewOpts.showSidebar
    }).changed) {
        ui.InvalidateAll();
    }
    if (ui.Switch(s_imgDemo.viewOpts.showTopToolbar, {.label = "Show Top Quick Toolbar"}).changed) ui.InvalidateAll();
    UiSize tbSize = s_imgDemo.viewOpts.toolbarSize.value_or(UiSize::Md);
    if (ui.StepSlider(tbSize, {
        .label = "Toolbar Size",
        .disabled = !s_imgDemo.viewOpts.showTopToolbar
    }).changed) {
        s_imgDemo.viewOpts.toolbarSize = tbSize;
        ui.InvalidateAll();
    }
    if (ui.Switch(s_imgDemo.viewOpts.showStatusBar, {.label = "Show Bottom Status Bar"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_imgDemo.viewOpts.showPixelGridAtHighZoom, {.label = "High-Zoom Pixel Grid (>=12x)"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_imgDemo.viewOpts.enableLineProfile, {.label = "Enable RMB Line Profile"}).changed) ui.InvalidateAll();
    if (ui.Switch(s_imgDemo.viewOpts.enableCtrlWlDrag, {.label = "Enable Ctrl+LMB W/L Drag"}).changed) ui.InvalidateAll();
}

void RenderImageViewStage(guikit::Context& ui, DemoContext& /*ctx*/) {
    s_imgDemo.EnsureGenerated();

    if (s_imgDemo.datasetIndex == 0) {
        const ImageViewData data = ImageViewData::FromU16(
            s_imgDemo.mono16Pixels,
            kDemoImgW,
            kDemoImgH,
            1,
            1001ull
        );
        s_imgDemo.lastResp = ui.ImageView(data, s_imgDemo.paramsMono, s_imgDemo.viewOpts);
    } else {
        const ImageViewData data = ImageViewData::FromFloat(
            s_imgDemo.rgbFloatPixels,
            kDemoImgW,
            kDemoImgH,
            3,
            2002ull
        );
        s_imgDemo.lastResp = ui.ImageView(data, s_imgDemo.paramsRgb, s_imgDemo.viewOpts);
    }
}

std::string GenerateImageViewCode(const DemoContext& /*ctx*/) {
    const ImageViewParams& prm = (s_imgDemo.datasetIndex == 0) ? s_imgDemo.paramsMono : s_imgDemo.paramsRgb;
    const auto& ch = prm.channels[static_cast<size_t>(prm.activeHistChannel)];

    char buf[1024]{};
    std::snprintf(
        buf,
        sizeof(buf),
        "// Single-Channel (16-bit/float) & Multi-Channel (RGB) Scientific ImageView\n"
        "guikit::ImageViewData img = %s;\n\n"
        "params.colormap = guikit::Colormap::%.*s;\n"
        "params.channels[0].window = %.1ff;\n"
        "params.channels[0].level  = %.1ff;\n"
        "params.channels[0].gamma  = %.2ff;\n"
        "params.channels[0].invert = %s;\n\n"
        "auto resp = ui.ImageView(img, params, {\n"
        "    .showSidebar = %s,\n"
        "    .sidebarOnRight = %s,\n"
        "    .sidebarWidth = %.0ff,\n"
        "    .showTopToolbar = %s,\n"
        "    .showStatusBar = %s,\n"
        "    .enableLineProfile = %s\n"
        "});",
        (s_imgDemo.datasetIndex == 0)
            ? "guikit::ImageViewData::FromU16(pixels16, 1024, 768, 1)"
            : "guikit::ImageViewData::FromFloat(pixelsRgb, 1024, 768, 3)",
        static_cast<int>(GetColormapName(prm.colormap).size()),
        GetColormapName(prm.colormap).data(),
        ch.window,
        ch.level,
        ch.gamma,
        ch.invert ? "true" : "false",
        s_imgDemo.viewOpts.showSidebar ? "true" : "false",
        s_imgDemo.viewOpts.sidebarOnRight ? "true" : "false",
        s_imgDemo.viewOpts.sidebarWidth,
        s_imgDemo.viewOpts.showTopToolbar ? "true" : "false",
        s_imgDemo.viewOpts.showStatusBar ? "true" : "false",
        s_imgDemo.viewOpts.enableLineProfile ? "true" : "false"
    );
    return std::string(buf);
}

} // namespace guikit::demo
