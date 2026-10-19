#include "guikit/widgets/ImageView.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Control.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/layout/Column.hpp"
#include "guikit/layout/Grid.hpp"
#include "guikit/layout/Row.hpp"
#include "guikit/panels/Box.hpp"
#include "guikit/panels/Card.hpp"
#include "guikit/panels/ScrollArea.hpp"
#include "guikit/widgets/Button.hpp"
#include "guikit/widgets/Checkbox.hpp"
#include "guikit/widgets/Select.hpp"
#include "guikit/widgets/Slider.hpp"
#include "guikit/widgets/Text.hpp"

namespace guikit {

namespace {

constexpr size_t kHistBins = 256;
constexpr size_t kToneLutSize = 4096;

[[nodiscard]] float ReadSampleAsFloat(const ImageViewData& data, size_t index) noexcept {
    if (!data.rawPixels || index >= data.sampleCount) {
        return 0.0f;
    }
    switch (data.format) {
        case ImageSampleFormat::Float32:
            return static_cast<const float*>(data.rawPixels)[index];
        case ImageSampleFormat::UInt16:
            return static_cast<float>(static_cast<const uint16_t*>(data.rawPixels)[index]);
        case ImageSampleFormat::UInt8:
            return static_cast<float>(static_cast<const uint8_t*>(data.rawPixels)[index]);
    }
    return 0.0f;
}

[[nodiscard]] std::array<float, 4> ReadPixelChannels(const ImageViewData& data, uint32_t sx, uint32_t sy) noexcept {
    std::array<float, 4> out{0.0f, 0.0f, 0.0f, 1.0f};
    if (data.Empty() || sx >= data.width || sy >= data.height) {
        return out;
    }
    const size_t base = (static_cast<size_t>(sy) * data.width + sx) * data.channels;
    if (data.channels == 1) {
        const float v = ReadSampleAsFloat(data, base);
        out[0] = v;
        out[1] = v;
        out[2] = v;
    } else {
        out[0] = ReadSampleAsFloat(data, base + 0);
        out[1] = (data.channels >= 2) ? ReadSampleAsFloat(data, base + 1) : out[0];
        out[2] = (data.channels >= 3) ? ReadSampleAsFloat(data, base + 2) : out[0];
        out[3] = (data.channels >= 4) ? ReadSampleAsFloat(data, base + 3) : 1.0f;
    }
    return out;
}

[[nodiscard]] constexpr float ComputeLuminance(float r, float g, float b) noexcept {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

[[nodiscard]] uint64_t ComputeImageFingerprint(const ImageViewData& data) noexcept {
    if (data.Empty()) {
        return 0;
    }
    if (data.dataGeneration != 0) {
        return detail::HashArgs(data.dataGeneration, data.width, data.height, data.channels, static_cast<uint8_t>(data.format));
    }
    uint64_t h = detail::HashArgs(data.width, data.height, data.channels, static_cast<uint8_t>(data.format), data.sampleCount);
    const size_t probes = std::min<size_t>(64, data.sampleCount);
    for (size_t i = 0; i < probes; ++i) {
        const size_t idx = (i * (data.sampleCount - 1)) / std::max<size_t>(1, probes - 1);
        const float v = ReadSampleAsFloat(data, idx);
        uint32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(float));
        h = detail::HashArgs(h, bits);
    }
    return h;
}

/// Forward and inverse coordinate mapping between Source Image (srcW x srcH) and Oriented Display Image (dispW x dispH).
struct OrientedDimensions {
    uint32_t srcW{0};
    uint32_t srcH{0};
    uint32_t dispW{0};
    uint32_t dispH{0};
    ImageRotation rotation{ImageRotation::deg0};
    bool flipH{false};
    bool flipV{false};

    static constexpr OrientedDimensions Compute(
        uint32_t w,
        uint32_t h,
        ImageRotation rot,
        bool fh,
        bool fv
    ) noexcept {
        const bool swapped = (rot == ImageRotation::deg90 || rot == ImageRotation::deg270);
        return OrientedDimensions{
            .srcW = w,
            .srcH = h,
            .dispW = swapped ? h : w,
            .dispH = swapped ? w : h,
            .rotation = rot,
            .flipH = fh,
            .flipV = fv
        };
    }

    /// Maps integer display pixel (dx, dy) in [0..dispW-1]x[0..dispH-1] to source pixel (sx, sy) in [0..srcW-1]x[0..srcH-1].
    [[nodiscard]] constexpr void DisplayToSourcePx(uint32_t dx, uint32_t dy, uint32_t& outSx, uint32_t& outSy) const noexcept {
        const uint32_t rx = flipH ? (dispW - 1 - dx) : dx;
        const uint32_t ry = flipV ? (dispH - 1 - dy) : dy;
        switch (rotation) {
            case ImageRotation::deg0:
                outSx = rx;
                outSy = ry;
                break;
            case ImageRotation::deg90:
                outSx = ry;
                outSy = srcH - 1 - rx;
                break;
            case ImageRotation::deg180:
                outSx = srcW - 1 - rx;
                outSy = srcH - 1 - ry;
                break;
            case ImageRotation::deg270:
                outSx = srcW - 1 - ry;
                outSy = rx;
                break;
        }
    }

    /// Maps continuous display coordinates (dx, dy) in [0..dispW]x[0..dispH] to source coordinates (sx, sy).
    [[nodiscard]] constexpr Vec2 DisplayToSourceContinuous(Vec2 d) const noexcept {
        const float rx = flipH ? (static_cast<float>(dispW) - d.x) : d.x;
        const float ry = flipV ? (static_cast<float>(dispH) - d.y) : d.y;
        switch (rotation) {
            case ImageRotation::deg0:
                return {rx, ry};
            case ImageRotation::deg90:
                return {ry, static_cast<float>(srcH) - rx};
            case ImageRotation::deg180:
                return {static_cast<float>(srcW) - rx, static_cast<float>(srcH) - ry};
            case ImageRotation::deg270:
                return {static_cast<float>(srcW) - ry, rx};
        }
        return {rx, ry};
    }

    /// Maps continuous source coordinates (sx, sy) in [0..srcW]x[0..srcH] to display coordinates (dx, dy).
    [[nodiscard]] constexpr Vec2 SourceToDisplayContinuous(Vec2 s) const noexcept {
        float rx = s.x;
        float ry = s.y;
        switch (rotation) {
            case ImageRotation::deg0:
                rx = s.x;
                ry = s.y;
                break;
            case ImageRotation::deg90:
                rx = static_cast<float>(srcH) - s.y;
                ry = s.x;
                break;
            case ImageRotation::deg180:
                rx = static_cast<float>(srcW) - s.x;
                ry = static_cast<float>(srcH) - s.y;
                break;
            case ImageRotation::deg270:
                rx = s.y;
                ry = static_cast<float>(srcW) - s.x;
                break;
        }
        const float dx = flipH ? (static_cast<float>(dispW) - rx) : rx;
        const float dy = flipV ? (static_cast<float>(dispH) - ry) : ry;
        return {dx, dy};
    }
};

struct ProfileCrossSection {
    bool active{false};
    bool dragging{false};
    Vec2 startSrc{0.0f, 0.0f};
    Vec2 endSrc{0.0f, 0.0f};
    std::vector<float> samplesLum{};
    std::vector<float> samplesR{};
    std::vector<float> samplesG{};
    std::vector<float> samplesB{};
    float minVal{0.0f};
    float maxVal{0.0f};
    float meanVal{0.0f};

    void Recompute(const ImageViewData& data) {
        samplesLum.clear();
        samplesR.clear();
        samplesG.clear();
        samplesB.clear();
        minVal = 0.0f;
        maxVal = 0.0f;
        meanVal = 0.0f;
        if (!active || data.Empty()) {
            return;
        }

        const float dx = endSrc.x - startSrc.x;
        const float dy = endSrc.y - startSrc.y;
        const float dist = std::hypot(dx, dy);
        const int n = std::clamp(static_cast<int>(std::round(dist)) + 1, 2, 2048);

        samplesLum.resize(static_cast<size_t>(n));
        if (data.channels >= 3) {
            samplesR.resize(static_cast<size_t>(n));
            samplesG.resize(static_cast<size_t>(n));
            samplesB.resize(static_cast<size_t>(n));
        }

        float mn = 1e30f;
        float mx = -1e30f;
        double sum = 0.0;

        for (int i = 0; i < n; ++i) {
            const float t = (n > 1) ? (static_cast<float>(i) / static_cast<float>(n - 1)) : 0.0f;
            const uint32_t sx = std::min(data.width - 1, static_cast<uint32_t>(std::max(0.0f, std::floor(startSrc.x + t * dx))));
            const uint32_t sy = std::min(data.height - 1, static_cast<uint32_t>(std::max(0.0f, std::floor(startSrc.y + t * dy))));
            const auto ch = ReadPixelChannels(data, sx, sy);

            if (data.channels == 1) {
                const float v = ch[0];
                samplesLum[static_cast<size_t>(i)] = v;
                if (v < mn) mn = v;
                if (v > mx) mx = v;
                sum += static_cast<double>(v);
            } else {
                const float lum = ComputeLuminance(ch[0], ch[1], ch[2]);
                samplesLum[static_cast<size_t>(i)] = lum;
                samplesR[static_cast<size_t>(i)] = ch[0];
                samplesG[static_cast<size_t>(i)] = ch[1];
                samplesB[static_cast<size_t>(i)] = ch[2];
                mn = std::min({mn, ch[0], ch[1], ch[2]});
                mx = std::max({mx, ch[0], ch[1], ch[2]});
                sum += static_cast<double>(lum);
            }
        }

        minVal = (mn <= mx) ? mn : 0.0f;
        maxVal = (mn <= mx) ? mx : 0.0f;
        meanVal = static_cast<float>(sum / static_cast<double>(n));
    }
};

struct ImageViewInternalState {
    uint64_t cachedDataFp{0};
    float dataMin{0.0f};
    float dataMax{65535.0f};
    float dataMean{0.0f};
    std::array<uint32_t, kHistBins> binsLum{};
    std::array<uint32_t, kHistBins> binsR{};
    std::array<uint32_t, kHistBins> binsG{};
    std::array<uint32_t, kHistBins> binsB{};

    // Cached tone-mapped & oriented ABGR8888 display buffer (rebuilt ONLY when data or tonal/transform params change)
    uint64_t cachedRenderKey{0};
    uint32_t displayW{0};
    uint32_t displayH{0};
    std::vector<uint32_t> displayAbgr{};

    // Viewport camera state
    float zoom{1.0f};
    Vec2 pan{0.0f, 0.0f};
    bool fitPending{true};
    bool panning{false};
    Vec2 panGrabMouse{0.0f, 0.0f};
    Vec2 panGrabStart{0.0f, 0.0f};

    // Direct Ctrl+LMB Window/Level drag state
    bool wlDragging{false};
    Vec2 wlGrabMouse{0.0f, 0.0f};
    float wlGrabWindow{1.0f};
    float wlGrabLevel{0.5f};

    // Double-click detector
    double lastClickTimeSec{-10.0};
    Vec2 lastClickPos{0.0f, 0.0f};

    // RMB Line Profile state
    ProfileCrossSection profile{};

    // Fallback internal params when caller calls the overload without ImageViewParams&
    ImageViewParams internalParams{};

    void EnsureStatsAndHistogram(const ImageViewData& data, ImageViewParams& params) {
        if (data.Empty()) {
            cachedDataFp = 0;
            dataMin = 0.0f;
            dataMax = 1.0f;
            binsLum.fill(0);
            binsR.fill(0);
            binsG.fill(0);
            binsB.fill(0);
            return;
        }

        const uint64_t fp = ComputeImageFingerprint(data);
        if (fp == cachedDataFp && params.initialized) {
            return;
        }

        const bool firstLoad = (cachedDataFp == 0) || !params.initialized;
        cachedDataFp = fp;

        float mn = 1e30f;
        float mx = -1e30f;
        double sum = 0.0;
        const size_t numPixels = static_cast<size_t>(data.width) * data.height;

        if (data.channels == 1) {
            for (size_t i = 0; i < numPixels; ++i) {
                const float v = ReadSampleAsFloat(data, i);
                if (std::isfinite(v)) {
                    if (v < mn) mn = v;
                    if (v > mx) mx = v;
                    sum += static_cast<double>(v);
                }
            }
        } else {
            for (size_t i = 0; i < numPixels; ++i) {
                const size_t base = i * data.channels;
                const float r = ReadSampleAsFloat(data, base + 0);
                const float g = ReadSampleAsFloat(data, base + 1);
                const float b = ReadSampleAsFloat(data, base + 2);
                if (std::isfinite(r) && std::isfinite(g) && std::isfinite(b)) {
                    mn = std::min({mn, r, g, b});
                    mx = std::max({mx, r, g, b});
                    sum += static_cast<double>(ComputeLuminance(r, g, b));
                }
            }
        }

        if (mn > mx) {
            mn = 0.0f;
            mx = (data.format == ImageSampleFormat::UInt16) ? 65535.0f : 255.0f;
        } else if (mx - mn < 1e-5f) {
            mx = mn + 1.0f;
        }

        dataMin = mn;
        dataMax = mx;
        dataMean = static_cast<float>(sum / static_cast<double>(std::max<size_t>(1, numPixels)));

        binsLum.fill(0);
        binsR.fill(0);
        binsG.fill(0);
        binsB.fill(0);

        const float scale = static_cast<float>(kHistBins - 1) / (dataMax - dataMin);
        if (data.channels == 1) {
            for (size_t i = 0; i < numPixels; ++i) {
                const float v = ReadSampleAsFloat(data, i);
                if (std::isfinite(v)) {
                    const size_t bin = static_cast<size_t>(std::clamp((v - dataMin) * scale, 0.0f, static_cast<float>(kHistBins - 1)));
                    binsLum[bin]++;
                }
            }
        } else {
            for (size_t i = 0; i < numPixels; ++i) {
                const size_t base = i * data.channels;
                const float r = ReadSampleAsFloat(data, base + 0);
                const float g = ReadSampleAsFloat(data, base + 1);
                const float b = ReadSampleAsFloat(data, base + 2);
                const float lum = ComputeLuminance(r, g, b);
                const size_t bL = static_cast<size_t>(std::clamp((lum - dataMin) * scale, 0.0f, static_cast<float>(kHistBins - 1)));
                const size_t bR = static_cast<size_t>(std::clamp((r - dataMin) * scale, 0.0f, static_cast<float>(kHistBins - 1)));
                const size_t bG = static_cast<size_t>(std::clamp((g - dataMin) * scale, 0.0f, static_cast<float>(kHistBins - 1)));
                const size_t bB = static_cast<size_t>(std::clamp((b - dataMin) * scale, 0.0f, static_cast<float>(kHistBins - 1)));
                binsLum[bL]++;
                binsR[bR]++;
                binsG[bG]++;
                binsB[bB]++;
            }
        }

        if (firstLoad) {
            for (auto& ch : params.channels) {
                ch.ResetToRange(dataMin, dataMax);
            }
            params.initialized = true;
            fitPending = true;
        }
        if (profile.active) {
            profile.Recompute(data);
        }
    }

    void EnsureDisplayPixels(const ImageViewData& data, const ImageViewParams& params) {
        if (data.Empty()) {
            displayW = 0;
            displayH = 0;
            displayAbgr.clear();
            cachedRenderKey = 0;
            return;
        }

        const OrientedDimensions od = OrientedDimensions::Compute(
            data.width, data.height, params.rotation, params.flipHorizontal, params.flipVertical
        );

        uint64_t rKey = detail::HashArgs(
            cachedDataFp,
            od.dispW,
            od.dispH,
            static_cast<uint8_t>(params.rotation),
            params.flipHorizontal,
            params.flipVertical,
            static_cast<uint8_t>(params.displayChannel),
            static_cast<uint8_t>(params.colormap)
        );
        for (const auto& ch : params.channels) {
            rKey = detail::HashArgs(rKey, ch.window, ch.level, ch.gamma, ch.invert);
        }

        if (rKey == cachedRenderKey && displayW == od.dispW && displayH == od.dispH &&
            displayAbgr.size() == static_cast<size_t>(od.dispW) * od.dispH) {
            return;
        }

        cachedRenderKey = rKey;
        displayW = od.dispW;
        displayH = od.dispH;
        displayAbgr.resize(static_cast<size_t>(displayW) * displayH);

        const float minV = dataMin;
        const float spanV = std::max(1e-5f, dataMax - dataMin);
        const float invSpanLut = static_cast<float>(kToneLutSize - 1) / spanV;

        const bool isScalarMode = (data.channels == 1) || (params.displayChannel != ImageDisplayChannel::CompositeRgb);

        if (isScalarMode) {
            // Determine which transfer curve governs the scalar view
            const ChannelTransferParams* activeTp = &params.channels[0];
            if (data.channels >= 3) {
                if (params.displayChannel == ImageDisplayChannel::RedOnly) activeTp = &params.channels[1];
                else if (params.displayChannel == ImageDisplayChannel::GreenOnly) activeTp = &params.channels[2];
                else if (params.displayChannel == ImageDisplayChannel::BlueOnly) activeTp = &params.channels[3];
            }

            // Build 4096-entry ABGR8888 LUT combining Window/Level/Gamma/Invert + active Colormap
            const auto& cmapLut = GetColormapLutAbgr(params.colormap, false);
            std::array<uint32_t, kToneLutSize> toneAbgrLut{};
            for (size_t i = 0; i < kToneLutSize; ++i) {
                const float val = minV + (static_cast<float>(i) / static_cast<float>(kToneLutSize - 1)) * spanV;
                float t = activeTp->Evaluate(val);
                if (data.channels >= 3 && params.displayChannel != ImageDisplayChannel::CompositeRgb &&
                    params.displayChannel != ImageDisplayChannel::LuminanceOnly) {
                    // Also apply Master curve if an individual R/G/B channel is isolated
                    const float masterVal = minV + t * spanV;
                    t = params.channels[0].Evaluate(masterVal);
                }
                const size_t cIdx = static_cast<size_t>(std::clamp(std::round(t * 255.0f), 0.0f, 255.0f));
                toneAbgrLut[i] = cmapLut[cIdx];
            }

            for (uint32_t dy = 0; dy < displayH; ++dy) {
                uint32_t* rowOut = displayAbgr.data() + static_cast<size_t>(dy) * displayW;
                for (uint32_t dx = 0; dx < displayW; ++dx) {
                    uint32_t sx = 0, sy = 0;
                    od.DisplayToSourcePx(dx, dy, sx, sy);
                    float sample = 0.0f;
                    if (data.channels == 1) {
                        sample = ReadSampleAsFloat(data, static_cast<size_t>(sy) * data.width + sx);
                    } else {
                        const auto ch = ReadPixelChannels(data, sx, sy);
                        switch (params.displayChannel) {
                            case ImageDisplayChannel::RedOnly:       sample = ch[0]; break;
                            case ImageDisplayChannel::GreenOnly:     sample = ch[1]; break;
                            case ImageDisplayChannel::BlueOnly:      sample = ch[2]; break;
                            case ImageDisplayChannel::LuminanceOnly:
                            case ImageDisplayChannel::CompositeRgb:  sample = ComputeLuminance(ch[0], ch[1], ch[2]); break;
                        }
                    }
                    const size_t lutIdx = static_cast<size_t>(std::clamp((sample - minV) * invSpanLut, 0.0f, static_cast<float>(kToneLutSize - 1)));
                    rowOut[dx] = toneAbgrLut[lutIdx];
                }
            }
        } else {
            // Full Multi-Channel RGB Composite Mode: build 4096-entry LUTs for R, G, B combining Channel + Master curves
            std::array<uint8_t, kToneLutSize> lutR{};
            std::array<uint8_t, kToneLutSize> lutG{};
            std::array<uint8_t, kToneLutSize> lutB{};

            for (size_t i = 0; i < kToneLutSize; ++i) {
                const float val = minV + (static_cast<float>(i) / static_cast<float>(kToneLutSize - 1)) * spanV;
                const float tr = params.channels[0].Evaluate(minV + params.channels[1].Evaluate(val) * spanV);
                const float tg = params.channels[0].Evaluate(minV + params.channels[2].Evaluate(val) * spanV);
                const float tb = params.channels[0].Evaluate(minV + params.channels[3].Evaluate(val) * spanV);
                lutR[i] = static_cast<uint8_t>(std::clamp(std::round(tr * 255.0f), 0.0f, 255.0f));
                lutG[i] = static_cast<uint8_t>(std::clamp(std::round(tg * 255.0f), 0.0f, 255.0f));
                lutB[i] = static_cast<uint8_t>(std::clamp(std::round(tb * 255.0f), 0.0f, 255.0f));
            }

            const float maxIdxF = static_cast<float>(kToneLutSize - 1);
            for (uint32_t dy = 0; dy < displayH; ++dy) {
                uint32_t* rowOut = displayAbgr.data() + static_cast<size_t>(dy) * displayW;
                for (uint32_t dx = 0; dx < displayW; ++dx) {
                    uint32_t sx = 0, sy = 0;
                    od.DisplayToSourcePx(dx, dy, sx, sy);
                    const auto ch = ReadPixelChannels(data, sx, sy);
                    const size_t ir = static_cast<size_t>(std::clamp((ch[0] - minV) * invSpanLut, 0.0f, maxIdxF));
                    const size_t ig = static_cast<size_t>(std::clamp((ch[1] - minV) * invSpanLut, 0.0f, maxIdxF));
                    const size_t ib = static_cast<size_t>(std::clamp((ch[2] - minV) * invSpanLut, 0.0f, maxIdxF));
                    rowOut[dx] = PackAbgr(Color(lutR[ir], lutG[ig], lutB[ib], 255));
                }
            }
        }
    }
};

struct ViewportLayoutGeom {
    Rect outerRect{};
    Rect topBarRect{};
    Rect canvasRect{};
    Rect statusRect{};
    std::array<Rect, 8> toolBtnRects{};
    UiSize toolbarSize{UiSize::Sm};
    bool hasTopBar{false};
    bool hasStatusBar{false};

    static constexpr std::array<std::string_view, 8> kToolLabels = {
        "Fit", "100%", "-", "+", "90\xC2\xB0", "Flip H", "Flip V", "Reset"
    };

    static ViewportLayoutGeom Compute(
        const Rect& box,
        const ImageViewOptions& opts,
        UiSize widgetSize,
        ICanvas* canvas,
        const UiTheme& theme
    ) noexcept {
        ViewportLayoutGeom g{};
        g.outerRect = box;
        g.toolbarSize = opts.toolbarSize.value_or(widgetSize);
        const float scale = theme.Scale(1.0f);
        const ComponentMetrics& cm = theme.GetMetrics(g.toolbarSize);
        const float gap = theme.GetGap(g.toolbarSize);

        g.hasTopBar = opts.showTopToolbar && (box.h >= 120.0f * scale);
        g.hasStatusBar = opts.showStatusBar && (box.h >= 90.0f * scale);

        const float bh = cm.defaultHeight;
        const float padY = std::max(4.0f * scale, gap * 0.65f);
        const float topH = g.hasTopBar ? (bh + padY * 2.0f) : 0.0f;
        const float botH = g.hasStatusBar ? (26.0f * scale) : 0.0f;

        if (g.hasTopBar) {
            g.topBarRect = Rect(box.x + 1.0f, box.y + 1.0f, std::max(0.0f, box.w - 2.0f), topH);
            float bx = g.topBarRect.x + gap;
            const float by = g.topBarRect.y + padY;
            for (size_t i = 0; i < kToolLabels.size(); ++i) {
                ButtonOptions bOpts{};
                if (i == 2) {
                    bOpts.icon = Icon::Minus();
                    bOpts.square = true;
                } else if (i == 3) {
                    bOpts.icon = Icon::Plus();
                    bOpts.square = true;
                } else {
                    bOpts.text = kToolLabels[i];
                }
                const Vec2 bSz = Button::Measure(bOpts, g.toolbarSize, canvas, theme);
                g.toolBtnRects[i] = Rect(bx, by, bSz.x, bSz.y);
                bx += bSz.x + gap * 0.75f;
            }
        }
        if (g.hasStatusBar) {
            g.statusRect = Rect(box.x + 1.0f, box.Bottom() - botH - 1.0f, std::max(0.0f, box.w - 2.0f), botH);
        }

        const float canvasY = box.y + 1.0f + topH;
        const float canvasH = std::max(16.0f, box.h - 2.0f - topH - botH);
        g.canvasRect = Rect(box.x + 1.0f, canvasY, std::max(16.0f, box.w - 2.0f), canvasH);
        return g;
    }
};

void FitImageToCanvas(ImageViewInternalState& st, const Rect& canvasRect) noexcept {
    if (st.displayW == 0 || st.displayH == 0 || canvasRect.w <= 4.0f || canvasRect.h <= 4.0f) {
        return;
    }
    const float pad = 16.0f;
    const float availW = std::max(16.0f, canvasRect.w - pad * 2.0f);
    const float availH = std::max(16.0f, canvasRect.h - pad * 2.0f);
    const float zx = availW / static_cast<float>(st.displayW);
    const float zy = availH / static_cast<float>(st.displayH);
    st.zoom = std::clamp(std::min(zx, zy), 0.05f, 64.0f);
    st.pan.x = (canvasRect.w - static_cast<float>(st.displayW) * st.zoom) * 0.5f;
    st.pan.y = (canvasRect.h - static_cast<float>(st.displayH) * st.zoom) * 0.5f;
    st.fitPending = false;
}

void ZoomAroundPoint(ImageViewInternalState& st, const Rect& canvasRect, Vec2 focusScreenPt, float newZoom) noexcept {
    newZoom = std::clamp(newZoom, 0.05f, 64.0f);
    const float relX = focusScreenPt.x - canvasRect.x;
    const float relY = focusScreenPt.y - canvasRect.y;
    const float imgX = (relX - st.pan.x) / std::max(1e-4f, st.zoom);
    const float imgY = (relY - st.pan.y) / std::max(1e-4f, st.zoom);
    st.zoom = newZoom;
    st.pan.x = relX - imgX * st.zoom;
    st.pan.y = relY - imgY * st.zoom;
    st.fitPending = false;
}

struct ImageViewViewportPayload {
    ImageViewData data{};
    ImageViewParams params{};
    ImageViewOptions opts{};
    ImageViewResponse resp{};
    ImageViewInternalState* state{nullptr};
    int hoveredToolBtn{-1};
};

ImageViewResponse RunViewportNode(
    Context& ctx,
    Id id,
    const ImageViewData& data,
    ImageViewParams& params,
    ImageViewInternalState& st,
    const ImageViewOptions& opts,
    bool standaloneElement,
    std::optional<float> inheritedH = std::nullopt
) {
    const UiTheme& theme = ctx.GetTheme();
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const float scale = theme.Scale(1.0f);

    Response baseRes = ctx.Interact(id);
    ImageViewResponse res{};
    static_cast<Response&>(res) = baseRes;

    const Rect boxRect = baseRes.rect;

    const ViewportLayoutGeom vgeom = ViewportLayoutGeom::Compute(boxRect, opts, activeSize, ctx.GetCanvas(), theme);
    const Rect cr = vgeom.canvasRect;

    if (st.fitPending && cr.w > 32.0f && cr.h > 32.0f && st.displayW > 0 && st.displayH > 0) {
        FitImageToCanvas(st, cr);
    }

    const Vec2 mouse = ctx.GetMousePos();
    const Vec2 wheel = ctx.GetMouseWheel();
    const KeyModifiers mods = ctx.GetKeyModifiers();
    const OrientedDimensions od = OrientedDimensions::Compute(
        data.width, data.height, params.rotation, params.flipHorizontal, params.flipVertical
    );

    // 1. Top Toolbar quick buttons
    int hoveredToolBtn = -1;
    bool clickedToolbar = false;
    if (!effectiveDisabled && vgeom.hasTopBar && baseRes.hovered) {
        for (size_t i = 0; i < vgeom.toolBtnRects.size(); ++i) {
            if (vgeom.toolBtnRects[i].Contains(mouse)) {
                hoveredToolBtn = static_cast<int>(i);
                if (ctx.IsMouseClicked()) {
                    clickedToolbar = true;
                    ctx.SetActiveId(Id::Null());
                    const Vec2 centerPt{cr.x + cr.w * 0.5f, cr.y + cr.h * 0.5f};
                    switch (i) {
                        case 0: // Fit
                            FitImageToCanvas(st, cr);
                            break;
                        case 1: // 100%
                            ZoomAroundPoint(st, cr, centerPt, 1.0f);
                            break;
                        case 2: // -
                            ZoomAroundPoint(st, cr, centerPt, st.zoom / 1.25f);
                            break;
                        case 3: // +
                            ZoomAroundPoint(st, cr, centerPt, st.zoom * 1.25f);
                            break;
                        case 4: // 90 deg rotate CW
                            params.rotation = static_cast<ImageRotation>((static_cast<uint8_t>(params.rotation) + 1) % 4);
                            st.fitPending = true;
                            res.paramsChanged = true;
                            break;
                        case 5: // Flip H
                            params.flipHorizontal = !params.flipHorizontal;
                            res.paramsChanged = true;
                            break;
                        case 6: // Flip V
                            params.flipVertical = !params.flipVertical;
                            res.paramsChanged = true;
                            break;
                        case 7: // Reset W/L
                            for (auto& ch : params.channels) {
                                ch.ResetToRange(st.dataMin, st.dataMax);
                            }
                            params.autoStrength = AutoWindowStrength::Off;
                            res.paramsChanged = true;
                            break;
                    }
                    res.changed = true;
                }
                break;
            }
        }
    }

    // 2. Canvas Viewport Interactions (Wheel Zoom, Pan, Ctrl+LMB W/L Drag, Double-Click Fit/100%, RMB Line Profile)
    const bool inCanvas = cr.Contains(mouse);
    if (!effectiveDisabled && !clickedToolbar && !data.Empty()) {
        // Cursor-centric Mouse Wheel Zoom
        if (baseRes.hovered && inCanvas && std::abs(wheel.y) > 1e-3f) {
            const float factor = std::pow(1.15f, wheel.y);
            ZoomAroundPoint(st, cr, mouse, st.zoom * factor);
            ctx.ConsumeMouseWheel();
            res.changed = true;
        }

        // LMB / MMB Press: Double-click Fit/100%, Ctrl+LMB W/L drag, or Pan
        if (baseRes.hovered && inCanvas && ctx.IsMouseClicked()) {
            const double nowSec = ctx.GetCurrentTime();
            const float clickDist = std::hypot(mouse.x - st.lastClickPos.x, mouse.y - st.lastClickPos.y);
            if (baseRes.doubleClicked || ((nowSec - st.lastClickTimeSec) < 0.32 && clickDist < 8.0f * scale)) {
                if (std::abs(st.zoom - 1.0f) < 0.05f) {
                    FitImageToCanvas(st, cr);
                } else {
                    ZoomAroundPoint(st, cr, mouse, 1.0f);
                }
                st.lastClickTimeSec = -10.0;
                st.panning = false;
                st.wlDragging = false;
                res.changed = true;
            } else {
                st.lastClickTimeSec = nowSec;
                st.lastClickPos = mouse;

                if (opts.enableCtrlWlDrag && mods.ctrl) {
                    const size_t chIdx = static_cast<size_t>(params.activeHistChannel);
                    st.wlDragging = true;
                    st.wlGrabMouse = mouse;
                    st.wlGrabWindow = params.channels[chIdx].window;
                    st.wlGrabLevel = params.channels[chIdx].level;
                } else {
                    st.panning = true;
                    st.panGrabMouse = mouse;
                    st.panGrabStart = st.pan;
                }
            }
        }

        if (baseRes.hovered && inCanvas && ctx.IsMiddleMouseClicked()) {
            st.panning = true;
            st.panGrabMouse = mouse;
            st.panGrabStart = st.pan;
        }

        if (st.wlDragging) {
            if (ctx.IsMouseDown()) {
                ctx.SetActiveId(id);
                const size_t chIdx = static_cast<size_t>(params.activeHistChannel);
                const float domainSpan = std::max(1e-4f, st.dataMax - st.dataMin);
                const float sensitivity = (mods.shift ? 0.25f : 1.0f) * (domainSpan / 320.0f);
                const float dx = mouse.x - st.wlGrabMouse.x;
                const float dy = mouse.y - st.wlGrabMouse.y;
                params.channels[chIdx].window = std::clamp(st.wlGrabWindow + dx * sensitivity, domainSpan * 0.002f, domainSpan * 2.0f);
                params.channels[chIdx].level  = std::clamp(st.wlGrabLevel  - dy * sensitivity, st.dataMin, st.dataMax);
                params.autoStrength = AutoWindowStrength::Off;
                res.paramsChanged = true;
                res.changed = true;
                res.pressed = true;
            } else {
                st.wlDragging = false;
            }
        }

        if (st.panning) {
            if (ctx.IsMouseDown() || ctx.IsMiddleMouseDown()) {
                ctx.SetActiveId(id);
                st.pan.x = st.panGrabStart.x + (mouse.x - st.panGrabMouse.x);
                st.pan.y = st.panGrabStart.y + (mouse.y - st.panGrabMouse.y);
                st.fitPending = false;
                res.pressed = true;
            } else {
                st.panning = false;
            }
        }

        // RMB Line Profile Tool
        if (opts.enableLineProfile) {
            auto screenToSourceClamped = [&](Vec2 scrPt) noexcept -> Vec2 {
                const float dispX = std::clamp((scrPt.x - cr.x - st.pan.x) / std::max(1e-4f, st.zoom), 0.0f, static_cast<float>(od.dispW));
                const float dispY = std::clamp((scrPt.y - cr.y - st.pan.y) / std::max(1e-4f, st.zoom), 0.0f, static_cast<float>(od.dispH));
                const Vec2 srcPt = od.DisplayToSourceContinuous({dispX, dispY});
                return {
                    std::clamp(srcPt.x, 0.0f, static_cast<float>( std::max(1u, data.width) - 1)),
                    std::clamp(srcPt.y, 0.0f, static_cast<float>( std::max(1u, data.height) - 1))
                };
            };

            if (baseRes.hovered && inCanvas && ctx.IsRightMouseClicked()) {
                st.profile.active = true;
                st.profile.dragging = true;
                st.profile.startSrc = screenToSourceClamped(mouse);
                st.profile.endSrc = st.profile.startSrc;
                st.profile.Recompute(data);
                res.changed = true;
            }

            if (st.profile.dragging) {
                if (ctx.IsRightMouseDown()) {
                    ctx.SetActiveId(id);
                    st.profile.endSrc = screenToSourceClamped(mouse);
                    st.profile.Recompute(data);
                    res.pressed = true;
                } else {
                    st.profile.dragging = false;
                    if (std::hypot(st.profile.endSrc.x - st.profile.startSrc.x,
                                   st.profile.endSrc.y - st.profile.startSrc.y) < 1.5f) {
                        // Simple right-click clears the profile overlay
                        st.profile.active = false;
                    }
                }
            }
        }
    } else {
        if (!ctx.IsMouseDown() && !ctx.IsMiddleMouseDown()) {
            st.panning = false;
            st.wlDragging = false;
        }
        if (!ctx.IsRightMouseDown()) {
            st.profile.dragging = false;
        }
    }

    // Rebuild display pixels if params changed during toolbar/Ctrl+LMB interaction
    if (res.paramsChanged) {
        st.EnsureDisplayPixels(data, params);
    }

    // 3. Populate Cursor Pixel Readout & Telemetry
    if (baseRes.hovered && inCanvas && !data.Empty() && st.displayW > 0 && st.displayH > 0) {
        const float dispX = (mouse.x - cr.x - st.pan.x) / std::max(1e-4f, st.zoom);
        const float dispY = (mouse.y - cr.y - st.pan.y) / std::max(1e-4f, st.zoom);
        if (dispX >= 0.0f && dispY >= 0.0f &&
            dispX < static_cast<float>(st.displayW) && dispY < static_cast<float>(st.displayH)) {
            uint32_t sx = 0, sy = 0;
            od.DisplayToSourcePx(static_cast<uint32_t>(dispX), static_cast<uint32_t>(dispY), sx, sy);
            res.cursorInsideImage = true;
            res.cursorImageX = sx;
            res.cursorImageY = sy;
            res.cursorSample = ReadPixelChannels(data, sx, sy);
            res.cursorLuminance = (data.channels == 1)
                ? res.cursorSample[0]
                : ComputeLuminance(res.cursorSample[0], res.cursorSample[1], res.cursorSample[2]);
        }
    }

    res.zoom = st.zoom;
    res.panOffset = st.pan;
    res.dataMin = st.dataMin;
    res.dataMax = st.dataMax;
    res.profileActive = st.profile.active;
    res.profileStartPx = st.profile.startSrc;
    res.profileEndPx = st.profile.endSrc;
    res.profileSampleCount = static_cast<uint32_t>(st.profile.samplesLum.size());
    res.profileMin = st.profile.minVal;
    res.profileMax = st.profile.maxVal;
    res.profileMean = st.profile.meanVal;

    // 4. Create Scene Graph Node for the Viewport
    Node* node = ctx.CreateNode(NodeType::ImageView, id);
    auto* payload = ctx.AllocPayload<ImageViewViewportPayload>();
    payload->data = data;
    payload->params = params;
    payload->opts = opts;
    payload->resp = res;
    payload->state = &st;
    payload->hoveredToolBtn = hoveredToolBtn;
    node->SetPayload(payload);

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = baseRes.hovered;
    node->active = res.pressed;
    node->focused = baseRes.focused;

    if (standaloneElement) {
        node->heightMode = opts.heightMode;
        node->minHeight = std::max(theme.Scale(opts.minHeight), theme.Scale(opts.height));
        ControlSpec ctrlSpec{};
        ctrlSpec.width = opts.width;
        ctrlSpec.height = inheritedH;
        Control::SetupNode(ctx, node, ctrlSpec, opts.stretch, opts.align);
        if (!node->parent || node->parent->type != NodeType::Element) {
            node->heightMode = opts.heightMode;
            if (inheritedH.has_value() && *inheritedH > 0.0f) {
                node->heightMode = SizeMode::Fixed;
                node->fixedHeight = theme.Scale(*inheritedH);
            }
            node->minHeight = std::max(theme.Scale(opts.minHeight), theme.Scale(opts.height));
            ctx.AdaptChildToContainer(node);
            if (opts.heightMode == SizeMode::Fit && !inheritedH.has_value() && node->heightMode == SizeMode::Stretch) {
                node->heightMode = SizeMode::Fit;
            }
        }
    } else {
        node->widthMode = SizeMode::Stretch;
        node->flexGrow = 1.0f;
        node->heightMode = SizeMode::Stretch;
        node->minHeight = std::max(theme.Scale(opts.minHeight), theme.Scale(opts.height));
        ctx.AdaptChildToContainer(node);
    }

    node->visualHash = detail::HashArgs(
        node->type,
        st.cachedRenderKey,
        st.zoom,
        std::round(st.pan.x * 2.0f),
        std::round(st.pan.y * 2.0f),
        res.cursorInsideImage,
        res.cursorImageX,
        res.cursorImageY,
        st.profile.active,
        st.profile.startSrc.x,
        st.profile.startSrc.y,
        st.profile.endSrc.x,
        st.profile.endSrc.y,
        hoveredToolBtn,
        vgeom.toolbarSize,
        opts.showTopToolbar,
        opts.showStatusBar,
        opts.background,
        opts.border,
        activeSize,
        effectiveDisabled
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* c, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<ImageViewViewportPayload>();
        if (!p) return {0.0f, 0.0f};
        return ImageView::Measure(p->data, p->opts, n->uiSize, c, th, availableWidth);
    };

    node->renderFn = [](const Node* n, ICanvas& c, const UiTheme& th) noexcept {
        const auto* p = n->As<ImageViewViewportPayload>();
        if (!p || !p->state) return;

        const ImageViewData& d = p->data;
        const ImageViewParams& prm = p->params;
        const ImageViewOptions& o = p->opts;
        const ImageViewResponse& resp = p->resp;
        ImageViewInternalState& s = *p->state;
        const float scale = th.Scale(1.0f);

        const Rect box = n->computedRect;
        if (box.w <= 8.0f || box.h <= 8.0f) return;

        const bool isDark = (th.mode == ThemeMode::Dark);
        const float rad = o.border.radius.has_value() ? th.Scale(*o.border.radius) : th.GetRadius(n->uiSize);
        const Color defaultBgOuter = isDark ? Color(12, 16, 24, 255) : Color(241, 245, 249, 255);
        const Color bgOuter   = o.background.Resolve(n->hovered, n->active, n->disabled, defaultBgOuter);
        const Color bgCanvas  = isDark ? Color(8, 10, 16, 255)  : Color(226, 232, 240, 255);
        const Color barBg     = isDark ? Color(17, 24, 39, 255) : Color(248, 250, 252, 255);
        const Color borderCol = o.border.color.Resolve(n->hovered, n->active, n->disabled, th.palette.borderSubtle);
        const float borderW   = (o.border.width > 0.0f) ? th.Scale(o.border.width) : 1.0f;
        const float smallFs = th.GetFontSize(TextRole::LabelSmall) * 0.92f;

        c.DrawRoundRect(box, rad, bgOuter, borderCol, borderW);

        const ViewportLayoutGeom vg = ViewportLayoutGeom::Compute(box, o, n->uiSize, &c, th);
        const ComponentMetrics& tbCm = th.GetMetrics(vg.toolbarSize);
        const Rect cr = vg.canvasRect;

        if (s.fitPending && cr.w > 32.0f && cr.h > 32.0f && s.displayW > 0 && s.displayH > 0) {
            FitImageToCanvas(s, cr);
        }

        // 1. Top Quick Toolbar (sized purely by UiSize / ComponentMetrics)
        if (vg.hasTopBar) {
            c.PushClip(vg.topBarRect);
            c.DrawRect(vg.topBarRect, barBg);
            c.DrawLine({vg.topBarRect.x, vg.topBarRect.Bottom()}, {vg.topBarRect.Right(), vg.topBarRect.Bottom()}, borderCol, 1.0f);

            for (size_t i = 0; i < ViewportLayoutGeom::kToolLabels.size(); ++i) {
                const bool isHovered = (p->hoveredToolBtn == static_cast<int>(i));
                const bool activeToggle =
                    (i == 5 && prm.flipHorizontal) ||
                    (i == 6 && prm.flipVertical) ||
                    (i == 4 && prm.rotation != ImageRotation::deg0);
                const Rect& br = vg.toolBtnRects[i];

                ButtonOptions bOpts{};
                if (i == 2) {
                    bOpts.icon = Icon::Minus();
                    bOpts.square = true;
                } else if (i == 3) {
                    bOpts.icon = Icon::Plus();
                    bOpts.square = true;
                } else {
                    bOpts.text = ViewportLayoutGeom::kToolLabels[i];
                }
                bOpts.color = activeToggle ? StateColor::Primary() : StateColor::Secondary();
                bOpts.active = activeToggle;
                bOpts.stretch = StretchMode::Fill;
                Button::Render(c, br, bOpts, vg.toolbarSize, isHovered, false, n->disabled, th, false);
            }

            // Right side of top bar: active palette / mode badge
            char modeBuf[64]{};
            if (d.channels == 1 || prm.displayChannel != ImageDisplayChannel::CompositeRgb) {
                std::snprintf(modeBuf, sizeof(modeBuf), "%.*s%s | %.0f%%",
                              static_cast<int>(GetColormapName(prm.colormap).size()),
                              GetColormapName(prm.colormap).data(),
                              prm.channels[0].invert ? " (Inv)" : "",
                              s.zoom * 100.0f);
            } else {
                std::snprintf(modeBuf, sizeof(modeBuf), "RGB Composite | %.0f%%", s.zoom * 100.0f);
            }
            const float rightX = vg.toolBtnRects.back().Right() + 10.0f * scale;
            if (vg.topBarRect.Right() - rightX > 40.0f) {
                c.DrawTextBox(
                    modeBuf,
                    Rect(rightX, vg.topBarRect.y, vg.topBarRect.Right() - rightX - 10.0f * scale, vg.topBarRect.h),
                    tbCm.companionFontSize,
                    th.palette.textSecondary,
                    1.0f,
                    0.5f
                );
            }
            c.PopClip();
        }

        // 2. Main Image Viewport Area
        c.PushClip(cr);
        c.DrawRect(cr, bgCanvas);

        if (!s.displayAbgr.empty() && s.displayW > 0 && s.displayH > 0) {
            const Rect destRect(
                cr.x + s.pan.x,
                cr.y + s.pan.y,
                static_cast<float>(s.displayW) * s.zoom,
                static_cast<float>(s.displayH) * s.zoom
            );

            c.DrawPixels(s.displayAbgr.data(), s.displayW, s.displayH, destRect);
            c.DrawRect(Rect(destRect.x - 1.0f, destRect.y - 1.0f, destRect.w + 2.0f, 1.0f), Color(100, 116, 139, 130));
            c.DrawRect(Rect(destRect.x - 1.0f, destRect.Bottom(), destRect.w + 2.0f, 1.0f), Color(100, 116, 139, 130));
            c.DrawRect(Rect(destRect.x - 1.0f, destRect.y, 1.0f, destRect.h), Color(100, 116, 139, 130));
            c.DrawRect(Rect(destRect.Right(), destRect.y, 1.0f, destRect.h), Color(100, 116, 139, 130));

            // High-zoom pixel grid (>= 12x zoom)
            if (o.showPixelGridAtHighZoom && s.zoom >= 12.0f) {
                const Color gridCol(255, 255, 255, static_cast<uint8_t>(std::clamp((s.zoom - 10.0f) * 4.0f, 18.0f, 75.0f)));
                const uint32_t x0 = static_cast<uint32_t>(std::max(0.0f, std::floor((cr.x - destRect.x) / s.zoom)));
                const uint32_t x1 = std::min(s.displayW, static_cast<uint32_t>(std::ceil((cr.Right() - destRect.x) / s.zoom)));
                const uint32_t y0 = static_cast<uint32_t>(std::max(0.0f, std::floor((cr.y - destRect.y) / s.zoom)));
                const uint32_t y1 = std::min(s.displayH, static_cast<uint32_t>(std::ceil((cr.Bottom() - destRect.y) / s.zoom)));

                for (uint32_t gx = x0; gx <= x1; ++gx) {
                    const float sx = destRect.x + static_cast<float>(gx) * s.zoom;
                    c.DrawLine({sx, std::max(cr.y, destRect.y)}, {sx, std::min(cr.Bottom(), destRect.Bottom())}, gridCol, 1.0f);
                }
                for (uint32_t gy = y0; gy <= y1; ++gy) {
                    const float sy = destRect.y + static_cast<float>(gy) * s.zoom;
                    c.DrawLine({std::max(cr.x, destRect.x), sy}, {std::min(cr.Right(), destRect.Right()), sy}, gridCol, 1.0f);
                }
            }

            // 3. RMB Cross-Section Line Profile Vector Overlay & Floating Plot
            if (s.profile.active && s.profile.samplesLum.size() >= 2) {
                const OrientedDimensions od = OrientedDimensions::Compute(
                    d.width, d.height, prm.rotation, prm.flipHorizontal, prm.flipVertical
                );
                const Vec2 d0 = od.SourceToDisplayContinuous({s.profile.startSrc.x + 0.5f, s.profile.startSrc.y + 0.5f});
                const Vec2 d1 = od.SourceToDisplayContinuous({s.profile.endSrc.x + 0.5f, s.profile.endSrc.y + 0.5f});
                const Vec2 p0{destRect.x + d0.x * s.zoom, destRect.y + d0.y * s.zoom};
                const Vec2 p1{destRect.x + d1.x * s.zoom, destRect.y + d1.y * s.zoom};

                c.DrawLine(p0, p1, Color(0, 0, 0, 170), 3.5f);
                c.DrawLine(p0, p1, Color(255, 75, 75, 240), 2.0f);
                const float endR = 4.0f * scale;
                c.DrawRoundRect(Rect(p0.x - endR, p0.y - endR, endR * 2.0f, endR * 2.0f), endR, Color(255, 90, 90, 255), Color(15, 23, 42, 255), 1.0f);
                c.DrawRoundRect(Rect(p1.x - endR, p1.y - endR, endR * 2.0f, endR * 2.0f), endR, Color(56, 189, 248, 255), Color(15, 23, 42, 255), 1.0f);

                // Floating Line Profile Graph Panel at bottom of canvas
                const float profH = std::min(124.0f * scale, cr.h * 0.36f);
                const Rect profBox(
                    cr.x + 10.0f * scale,
                    cr.Bottom() - profH - 10.0f * scale,
                    std::max(80.0f, cr.w - 20.0f * scale),
                    profH
                );
                c.DrawRoundRect(profBox, 6.0f * scale, Color(12, 16, 26, 225), Color(100, 160, 255, 160), 1.0f);

                const Rect profPlot(
                    profBox.x + 8.0f * scale,
                    profBox.y + 22.0f * scale,
                    std::max(20.0f, profBox.w - 16.0f * scale),
                    std::max(16.0f, profBox.h - 28.0f * scale)
                );

                // Header stats inside Profile Box
                char profHdr[128]{};
                std::snprintf(
                    profHdr,
                    sizeof(profHdr),
                    "Line Profile: %zu px  |  Min: %.1f  |  Max: %.1f  |  Mean: %.1f  |  \xCE\x94: %.1f",
                    s.profile.samplesLum.size(),
                    s.profile.minVal,
                    s.profile.maxVal,
                    s.profile.meanVal,
                    s.profile.maxVal - s.profile.minVal
                );
                c.DrawText(profHdr, {profBox.x + 10.0f * scale, profBox.y + 5.0f * scale}, smallFs * 0.92f, Color(226, 232, 240, 240));

                // Grid lines inside Profile Plot
                c.DrawLine({profPlot.x, profPlot.y + profPlot.h * 0.5f}, {profPlot.Right(), profPlot.y + profPlot.h * 0.5f}, Color(71, 85, 105, 90), 1.0f);

                const float pMin = s.profile.minVal;
                const float pSpan = std::max(1e-4f, s.profile.maxVal - pMin);

                auto drawProfileTrace = [&](const std::vector<float>& vec, Color col, float strokeW) {
                    if (vec.size() < 2) return;
                    const size_t nPts = vec.size();
                    Vec2 prev{};
                    for (size_t i = 0; i < nPts; ++i) {
                        const float u = static_cast<float>(i) / static_cast<float>(nPts - 1);
                        const float vNorm = std::clamp((vec[i] - pMin) / pSpan, 0.0f, 1.0f);
                        const Vec2 pt{
                            profPlot.x + u * profPlot.w,
                            profPlot.Bottom() - vNorm * profPlot.h
                        };
                        if (i > 0) {
                            c.DrawLine(prev, pt, col, strokeW);
                        }
                        prev = pt;
                    }
                };

                if (d.channels >= 3 && prm.displayChannel == ImageDisplayChannel::CompositeRgb) {
                    drawProfileTrace(s.profile.samplesR, Color(248, 113, 113, 215), 1.35f);
                    drawProfileTrace(s.profile.samplesG, Color(74, 222, 128, 215), 1.35f);
                    drawProfileTrace(s.profile.samplesB, Color(96, 165, 250, 215), 1.35f);
                    drawProfileTrace(s.profile.samplesLum, Color(241, 245, 249, 235), 1.6f);
                } else {
                    drawProfileTrace(s.profile.samplesLum, Color(120, 200, 255, 245), 1.8f);
                }
            }
        } else {
            c.DrawTextBox("No image loaded", cr, th.GetFontSize(TextRole::BodyMedium), th.palette.textMuted, 0.5f, 0.5f);
        }

        c.PopClip();

        // 4. Bottom Telemetry Status Bar
        if (vg.hasStatusBar) {
            c.PushClip(vg.statusRect);
            c.DrawRect(vg.statusRect, barBg);
            c.DrawLine({vg.statusRect.x, vg.statusRect.y}, {vg.statusRect.Right(), vg.statusRect.y}, borderCol, 1.0f);

            const auto& masterTp = prm.channels[static_cast<size_t>(prm.activeHistChannel)];
            char statusLeft[160]{};
            std::snprintf(
                statusLeft,
                sizeof(statusLeft),
                "%ux%u (%uch)  |  W: %.0f  L: %.0f  \xCE\xB3: %.2f",
                d.width,
                d.height,
                d.channels,
                masterTp.window,
                masterTp.level,
                masterTp.gamma
            );
            c.DrawTextBox(
                statusLeft,
                Rect(vg.statusRect.x + 8.0f * scale, vg.statusRect.y, vg.statusRect.w * 0.55f, vg.statusRect.h),
                smallFs * 0.90f,
                th.palette.textSecondary,
                0.0f,
                0.5f
            );

            char statusRight[160]{};
            if (resp.cursorInsideImage) {
                if (d.channels == 1) {
                    std::snprintf(
                        statusRight,
                        sizeof(statusRight),
                        "X: %u  Y: %u  |  Val: %.1f",
                        resp.cursorImageX,
                        resp.cursorImageY,
                        resp.cursorSample[0]
                    );
                } else {
                    std::snprintf(
                        statusRight,
                        sizeof(statusRight),
                        "X: %u  Y: %u  |  R:%.0f G:%.0f B:%.0f  Lum:%.0f",
                        resp.cursorImageX,
                        resp.cursorImageY,
                        resp.cursorSample[0],
                        resp.cursorSample[1],
                        resp.cursorSample[2],
                        resp.cursorLuminance
                    );
                }
            } else {
                std::snprintf(statusRight, sizeof(statusRight), "LMB: Pan  |  Ctrl+LMB: W/L  |  RMB: Line Profile");
            }
            c.DrawTextBox(
                statusRight,
                Rect(vg.statusRect.x + vg.statusRect.w * 0.42f, vg.statusRect.y, vg.statusRect.w * 0.58f - 8.0f * scale, vg.statusRect.h),
                smallFs * 0.90f,
                resp.cursorInsideImage ? th.palette.textPrimary : th.palette.textMuted,
                1.0f,
                0.5f
            );
            c.PopClip();
        }
    };

    return res;
}

} // namespace

void ImageView::ApplyQuantileAutoWindow(
    std::span<const uint32_t> bins,
    float minVal,
    float maxVal,
    AutoWindowTarget target,
    AutoWindowStrength strength,
    ChannelTransferParams& ioTransfer
) noexcept {
    const float span = std::max(1e-4f, maxVal - minVal);
    if (bins.empty() || strength == AutoWindowStrength::Off) {
        ioTransfer.SetLowHigh(minVal, maxVal);
        return;
    }

    uint64_t total = 0;
    for (uint32_t c : bins) {
        total += c;
    }
    if (total == 0) {
        ioTransfer.SetLowHigh(minVal, maxVal);
        return;
    }

    const float tailClip = (strength == AutoWindowStrength::Medium) ? 0.01f : 0.05f;
    const float qLow  = (target == AutoWindowTarget::Black || target == AutoWindowTarget::All) ? tailClip : 0.0f;
    const float qHigh = (target == AutoWindowTarget::White || target == AutoWindowTarget::All) ? (1.0f - tailClip) : 1.0f;

    auto findQuantileValue = [&](float q) noexcept -> float {
        if (q <= 0.0f) return minVal;
        if (q >= 1.0f) return maxVal;
        const double targetCount = static_cast<double>(q) * static_cast<double>(total);
        uint64_t accum = 0;
        for (size_t i = 0; i < bins.size(); ++i) {
            const uint64_t nextAccum = accum + bins[i];
            if (static_cast<double>(nextAccum) >= targetCount) {
                const double binFraction = (bins[i] > 0)
                    ? ((targetCount - static_cast<double>(accum)) / static_cast<double>(bins[i]))
                    : 0.0;
                const float norm = (static_cast<float>(i) + std::clamp(static_cast<float>(binFraction), 0.0f, 1.0f)) /
                                   static_cast<float>(bins.size());
                return minVal + norm * span;
            }
            accum = nextAccum;
        }
        return maxVal;
    };

    const float lo = findQuantileValue(qLow);
    const float hi = std::max(lo + span * 0.005f, findQuantileValue(qHigh));
    ioTransfer.SetLowHigh(lo, hi, span * 0.002f);
}

Vec2 ImageView::Measure(
    const ImageViewData& /*data*/,
    const ImageViewOptions& opts,
    UiSize /*size*/,
    ICanvas* /*canvas*/,
    const UiTheme& theme,
    float availableWidth
) noexcept {
    float w = 780.0f * theme.Scale(1.0f);
    if (opts.stretch == StretchMode::Fill && availableWidth > 0.0f) {
        const float ratio = opts.width.has_value() ? std::clamp(*opts.width, 0.1f, 1.0f) : 1.0f;
        w = availableWidth * ratio;
    } else if (opts.width.has_value() && *opts.width > 0.0f) {
        w = theme.Scale(*opts.width);
    }
    const float h = std::max(theme.Scale(opts.minHeight), theme.Scale(opts.height));
    return {w, h};
}

ImageViewResponse ImageView::Run(
    Context& ctx,
    const ImageViewData& data,
    const ImageViewOptions& opts,
    std::source_location loc
) {
    const Id baseId = ctx.MakeChildId(loc);
    auto& st = ctx.GetOrCreateState<ImageViewInternalState>(baseId);
    return Run(ctx, data, st.internalParams, opts, loc);
}

ImageViewResponse ImageView::Run(
    Context& ctx,
    const ImageViewData& data,
    ImageViewParams& params,
    const ImageViewOptions& opts,
    std::source_location loc
) {
    const Id rootId = ctx.MakeChildId(loc);
    auto& st = ctx.GetOrCreateState<ImageViewInternalState>(rootId);

    ImageViewOptions optsCopy = opts;
    std::optional<float> inheritedH{};
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, inheritedH);
    if (inheritedH.has_value() && *inheritedH > 0.0f) {
        optsCopy.height = *inheritedH;
    }

    // 1. Update cached statistics, 256-bin histograms, and display pixel buffer
    st.EnsureStatsAndHistogram(data, params);
    st.EnsureDisplayPixels(data, params);

    if (!optsCopy.showSidebar) {
        return RunViewportNode(ctx, rootId, data, params, st, optsCopy, true, inheritedH);
    }

    // 2. Integrated Workstation Layout (Sidebar + Interactive Viewport)
    bool sidebarChangedParams = false;
    ImageViewResponse viewportRes{};

    auto renderSidebar = [&]() {
        if (auto sidePanel = ctx.Sidebar({
            .side = optsCopy.sidebarOnRight ? SidebarSide::Right : SidebarSide::Left,
            .width = optsCopy.sidebarWidth,
            .scrollable = true,
            .gap = 8.0f,
            .padding = Padding::All(10.0f),
            .border = {.radius = 6.0f}
        })) {
            ctx.PushSize(UiSize::Sm);

            // --- Section 1: Histogram & Interactive Window/Level ---
            ctx.Text("Histogram & Window/Level", {.role = TextRole::LabelLarge});

            const HistogramData histData = (data.channels >= 3)
                ? HistogramData::Rgb(st.binsLum, st.binsR, st.binsG, st.binsB, st.dataMin, st.dataMax)
                : HistogramData::Mono(st.binsLum, st.dataMin, st.dataMax);

            const size_t activeChIdx = (data.channels >= 3)
                ? static_cast<size_t>(params.activeHistChannel)
                : 0u;
            ChannelTransferParams& activeTp = params.channels[activeChIdx];

            HistogramOptions histOpts{};
            histOpts.height = 142.0f;
            histOpts.scaleMode = params.histScaleMode;
            histOpts.activeChannel = (data.channels >= 3) ? params.activeHistChannel : HistogramChannel::Master;
            histOpts.colormap = params.colormap;
            histOpts.showChannelPills = (data.channels >= 3);
            histOpts.multiChannelParams = params.channels;
            histOpts.tooltip = "Drag left/right markers for black/white points, center band for level, double-click to reset";

            const HistogramResponse hRes = ctx.Run<Histogram>(histData, activeTp, histOpts);
            if (hRes.channelChanged) {
                params.activeHistChannel = hRes.activeChannel;
            }
            if (hRes.windowLevelChanged) {
                params.autoStrength = AutoWindowStrength::Off;
                sidebarChangedParams = true;
            }

            // Multi-channel isolation selector when image has >= 3 channels
            if (data.channels >= 3) {
                static constexpr std::array<std::string_view, 5> kDispChNames = {
                    "Full RGB Composite",
                    "Isolate Red Channel",
                    "Isolate Green Channel",
                    "Isolate Blue Channel",
                    "Isolate Luminance"
                };
                int dispChIdx = static_cast<int>(params.displayChannel);
                if (auto el = ctx.Element({.label = "View Mode"})) {
                    if (ctx.Select(dispChIdx, kDispChNames).changed) {
                        params.displayChannel = static_cast<ImageDisplayChannel>(dispChIdx);
                        if (params.displayChannel == ImageDisplayChannel::RedOnly) params.activeHistChannel = HistogramChannel::Red;
                        else if (params.displayChannel == ImageDisplayChannel::GreenOnly) params.activeHistChannel = HistogramChannel::Green;
                        else if (params.displayChannel == ImageDisplayChannel::BlueOnly) params.activeHistChannel = HistogramChannel::Blue;
                        else params.activeHistChannel = HistogramChannel::Master;
                        sidebarChangedParams = true;
                    }
                }
            }

            // Window / Level / Gamma sliders for the active channel
            const float span = std::max(1.0f, st.dataMax - st.dataMin);
            const float minW = std::max(0.01f, span * 0.002f);
            ChannelTransferParams& curTp = params.channels[static_cast<size_t>((data.channels >= 3) ? params.activeHistChannel : HistogramChannel::Master)];
            const std::string_view wlFmt = (span >= 50.0f) ? "%.0f" : "%.2f";

            if (auto el = ctx.Element({.label = "W (Window)"})) {
                if (ctx.Slider(curTp.window, minW, span * 1.5f, {
                    .valueFormat = wlFmt,
                    .showValue = true
                }).changed) {
                    params.autoStrength = AutoWindowStrength::Off;
                    sidebarChangedParams = true;
                }
            }

            if (auto el = ctx.Element({.label = "L (Level)"})) {
                if (ctx.Slider(curTp.level, st.dataMin, st.dataMax, {
                    .valueFormat = wlFmt,
                    .showValue = true
                }).changed) {
                    params.autoStrength = AutoWindowStrength::Off;
                    sidebarChangedParams = true;
                }
            }

            if (auto el = ctx.Element({.label = "Gamma"})) {
                if (ctx.Slider(curTp.gamma, 0.15f, 4.0f, {
                    .valueFormat = "%.2f",
                    .step = 0.05,
                    .showValue = true
                }).changed) {
                    sidebarChangedParams = true;
                }
            }

            if (auto invRow = ctx.Row({.gap = 8.0f, .align = Alignment::Center})) {
                if (auto el = ctx.Element({.label = "Invert"})) {
                    if (ctx.Checkbox(curTp.invert).changed) {
                        sidebarChangedParams = true;
                    }
                }
                ctx.Spacer();
                if (ctx.Button({.text = "Reset W/L", .color = StateColor::Secondary()}).clicked) {
                    for (auto& ch : params.channels) {
                        ch.ResetToRange(st.dataMin, st.dataMax);
                    }
                    params.autoStrength = AutoWindowStrength::Off;
                    sidebarChangedParams = true;
                }
            }

            // --- Section 2: Colormap (for 1-channel or isolated channel) ---
            const bool canUseColormap = (data.channels == 1) || (params.displayChannel != ImageDisplayChannel::CompositeRgb);
            ctx.Disabled(!canUseColormap, [&]() {
                int cmapIdx = static_cast<int>(params.colormap);
                if (auto el = ctx.Element({
                    .label = "Colormap",
                    .description = canUseColormap ? std::string_view{} : "Select an isolated channel to apply Colormap"
                })) {
                    if (ctx.Select(cmapIdx, GetAllColormapNames()).changed) {
                        params.colormap = static_cast<Colormap>(cmapIdx);
                        sidebarChangedParams = true;
                    }
                }
            });

            // --- Section 3: Auto Window (Quantile Clipping) ---
            ctx.Text("Auto Window", {.role = TextRole::LabelLarge});
            static constexpr std::array<std::string_view, 3> kAutoTargets = {"Black", "White", "All"};
            int targetIdx = static_cast<int>(params.autoTarget);
            if (ctx.StepSlider(targetIdx, kAutoTargets, {.showValue = false}).changed) {
                params.autoTarget = static_cast<AutoWindowTarget>(targetIdx);
                if (params.autoStrength != AutoWindowStrength::Off) {
                    std::span<const uint32_t> targetBins = st.binsLum;
                    if (data.channels >= 3) {
                        if (params.activeHistChannel == HistogramChannel::Red) targetBins = st.binsR;
                        else if (params.activeHistChannel == HistogramChannel::Green) targetBins = st.binsG;
                        else if (params.activeHistChannel == HistogramChannel::Blue) targetBins = st.binsB;
                    }
                    ApplyQuantileAutoWindow(targetBins, st.dataMin, st.dataMax, params.autoTarget, params.autoStrength, curTp);
                    sidebarChangedParams = true;
                }
            }

            if (auto strRow = ctx.Row({.gap = 6.0f})) {
                static constexpr std::array<std::string_view, 3> kStrengthNames = {"Off", "Medium", "Strong"};
                for (size_t i = 0; i < 3; ++i) {
                    const auto sMode = static_cast<AutoWindowStrength>(i);
                    const bool isSel = (params.autoStrength == sMode);
                    if (ctx.Button({
                        .text = kStrengthNames[i],
                        .color = isSel ? StateColor::Primary() : StateColor::Secondary(),
                        .stretch = StretchMode::Fill
                    }).clicked) {
                        params.autoStrength = sMode;
                        std::span<const uint32_t> targetBins = st.binsLum;
                        if (data.channels >= 3) {
                            if (params.activeHistChannel == HistogramChannel::Red) targetBins = st.binsR;
                            else if (params.activeHistChannel == HistogramChannel::Green) targetBins = st.binsG;
                            else if (params.activeHistChannel == HistogramChannel::Blue) targetBins = st.binsB;
                        }
                        ApplyQuantileAutoWindow(targetBins, st.dataMin, st.dataMax, params.autoTarget, params.autoStrength, curTp);
                        sidebarChangedParams = true;
                    }
                }
            }

            if (data.channels >= 3) {
                if (ctx.Button({
                    .text = "Auto White Balance (Per-Channel R/G/B)",
                    .color = StateColor::Secondary(),
                    .stretch = StretchMode::Fill
                }).clicked) {
                    params.channels[0].ResetToRange(st.dataMin, st.dataMax);
                    const AutoWindowStrength effStr = (params.autoStrength == AutoWindowStrength::Off)
                        ? AutoWindowStrength::Medium
                        : params.autoStrength;
                    ApplyQuantileAutoWindow(st.binsR, st.dataMin, st.dataMax, AutoWindowTarget::All, effStr, params.channels[1]);
                    ApplyQuantileAutoWindow(st.binsG, st.dataMin, st.dataMax, AutoWindowTarget::All, effStr, params.channels[2]);
                    ApplyQuantileAutoWindow(st.binsB, st.dataMin, st.dataMax, AutoWindowTarget::All, effStr, params.channels[3]);
                    sidebarChangedParams = true;
                }
            }

            // --- Section 4: Transform & View ---
            ctx.Text("Transform & View", {.role = TextRole::LabelLarge});
            if (auto zRow = ctx.Row({.gap = 6.0f, .align = Alignment::Center})) {
                if (ctx.Button({.text = "Fit", .color = StateColor::Secondary(), .stretch = StretchMode::Fill}).clicked) {
                    st.fitPending = true;
                }
                if (ctx.Button({.text = "100%", .color = StateColor::Secondary(), .stretch = StretchMode::Fill}).clicked) {
                    st.zoom = 1.0f;
                    st.fitPending = false;
                }
            }

            static constexpr std::array<std::string_view, 4> kRotLabels = {"0\xC2\xB0", "90\xC2\xB0", "180\xC2\xB0", "270\xC2\xB0"};
            int rotIdx = static_cast<int>(params.rotation);
            if (auto el = ctx.Element({.label = "Rotate"})) {
                if (ctx.StepSlider(rotIdx, kRotLabels, {.showValue = false}).changed) {
                    params.rotation = static_cast<ImageRotation>(rotIdx);
                    st.fitPending = true;
                    sidebarChangedParams = true;
                }
            }

            if (auto flipRow = ctx.Row({.gap = 6.0f})) {
                if (ctx.Button({
                    .text = "Flip H",
                    .color = params.flipHorizontal ? StateColor::Primary() : StateColor::Secondary(),
                    .stretch = StretchMode::Fill
                }).clicked) {
                    params.flipHorizontal = !params.flipHorizontal;
                    sidebarChangedParams = true;
                }
                if (ctx.Button({
                    .text = "Flip V",
                    .color = params.flipVertical ? StateColor::Primary() : StateColor::Secondary(),
                    .stretch = StretchMode::Fill
                }).clicked) {
                    params.flipVertical = !params.flipVertical;
                    sidebarChangedParams = true;
                }
            }

            ctx.PopSize();
        }
    };

    if (auto outerRow = ctx.Row({
        .gap = 8.0f,
        .align = Alignment::Stretch,
        .heightMode = optsCopy.heightMode
    }, loc)) {
        if (Node* rNode = outerRow.GetNode()) {
            const UiTheme& th = ctx.GetTheme();
            if (rNode->parent && rNode->parent->type == NodeType::Element) {
                rNode->widthMode = SizeMode::Stretch;
                rNode->heightMode = SizeMode::Stretch;
            } else {
                if (optsCopy.width.has_value() && *optsCopy.width > 0.0f) {
                    rNode->widthMode = SizeMode::Fixed;
                    rNode->fixedWidth = th.Scale(*optsCopy.width);
                }
                rNode->minHeight = std::max(th.Scale(optsCopy.minHeight), th.Scale(optsCopy.height));
                if (optsCopy.heightMode == SizeMode::Fixed || inheritedH.has_value()) {
                    rNode->heightMode = SizeMode::Fixed;
                    rNode->fixedHeight = std::max(th.Scale(optsCopy.minHeight), th.Scale(optsCopy.height));
                }
            }
        }

        const Id vpId = ctx.MakeChildId(loc);
        if (!optsCopy.sidebarOnRight) {
            renderSidebar();
            if (sidebarChangedParams) {
                st.EnsureDisplayPixels(data, params);
            }
            viewportRes = RunViewportNode(ctx, vpId, data, params, st, optsCopy, false, inheritedH);
        } else {
            viewportRes = RunViewportNode(ctx, vpId, data, params, st, optsCopy, false, inheritedH);
            renderSidebar();
            if (sidebarChangedParams) {
                st.EnsureDisplayPixels(data, params);
            }
        }
    }

    if (sidebarChangedParams) {
        viewportRes.paramsChanged = true;
        viewportRes.changed = true;
    }
    return viewportRes;
}

} // namespace guikit
