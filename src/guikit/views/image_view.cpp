#include "guikit/views/image_view.hpp"
#include "guikit/core/element.hpp"
#include "guikit/layout/spacer.hpp"
#include "guikit/surfaces/sidebar.hpp"
#include "guikit/display/histogram.hpp"
#include "guikit/controls/switch.hpp"
#include <cstdio>
#include <cstring>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"
#include "guikit/layout/column.hpp"
#include "guikit/layout/grid.hpp"
#include "guikit/layout/row.hpp"
#include "guikit/surfaces/box.hpp"
#include "guikit/surfaces/card.hpp"
#include "guikit/layout/scroll_area.hpp"
#include "guikit/actions/button.hpp"
#include "guikit/controls/checkbox.hpp"
#include "guikit/fields/select.hpp"
#include "guikit/controls/slider.hpp"
#include "guikit/display/text.hpp"
#include "keel/array.h"
#include "keel/bits.h"
#include "keel/core.h"
#include "keel/math.h"
#include "keel/static_array.h"
#include "keel/string.h"

namespace guikit {

namespace {

constexpr keel::usize kHistBins = 256;
constexpr keel::usize kToneLutSize = 4096;
constexpr keel::f32 kDefaultImageViewHeightDp = 620.0f;
constexpr keel::f32 kDefaultImageViewMinHeightDp = 360.0f;

[[nodiscard]] constexpr keel::f32 ResolveEffectiveMinHeightDp(const ImageViewOptions& opts) noexcept {
    const keel::f32 minH = opts.minHeight.value_or(kDefaultImageViewMinHeightDp);
    if (opts.height.IsStretch()) {
        return minH;
    }
    if (opts.height.IsFixed()) {
        return keel::max(minH, opts.height.Dp());
    }
    return keel::max(minH, kDefaultImageViewHeightDp);
}

[[nodiscard]] keel::f32 ReadSampleAsFloat(const ImageViewData& data, keel::usize index) noexcept {
    if (!data.rawPixels || index >= data.sampleCount) {
        return 0.0f;
    }
    switch (data.format) {
        case ImageSampleFormat::Float32:
            return static_cast<const keel::f32*>(data.rawPixels)[index];
        case ImageSampleFormat::UInt16:
            return keel::as<keel::f32>(static_cast<const keel::u16*>(data.rawPixels)[index]);
        case ImageSampleFormat::UInt8:
            return keel::as<keel::f32>(static_cast<const keel::u8*>(data.rawPixels)[index]);
    }
    return 0.0f;
}

[[nodiscard]] keel::StaticArray<keel::f32, 4> ReadPixelChannels(const ImageViewData& data, keel::u32 sx, keel::u32 sy) noexcept {
    keel::StaticArray<keel::f32, 4> out{0.0f, 0.0f, 0.0f, 1.0f};
    if (data.Empty() || sx >= data.width || sy >= data.height) {
        return out;
    }
    const keel::usize base = (keel::as<keel::usize>(sy) * data.width + sx) * data.channels;
    if (data.channels == 1) {
        const keel::f32 v = ReadSampleAsFloat(data, base);
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

[[nodiscard]] constexpr keel::f32 ComputeLuminance(keel::f32 r, keel::f32 g, keel::f32 b) noexcept {
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

[[nodiscard]] keel::u64 ComputeImageFingerprint(const ImageViewData& data) noexcept {
    if (data.Empty()) {
        return 0;
    }
    if (data.dataGeneration != 0) {
        return detail::HashArgs(data.dataGeneration, data.width, data.height, data.channels, keel::as<keel::u8>(data.format));
    }
    keel::u64 h = detail::HashArgs(data.width, data.height, data.channels, keel::as<keel::u8>(data.format), data.sampleCount);
    const keel::usize probes = keel::min<keel::usize>(64, data.sampleCount);
    for (keel::usize i = 0; i < probes; ++i) {
        const keel::usize idx = (i * (data.sampleCount - 1)) / keel::max<keel::usize>(1, probes - 1);
        const keel::f32 v = ReadSampleAsFloat(data, idx);
        const keel::u32 bits = keel::bit_cast<keel::u32>(v);
        h = detail::HashArgs(h, bits);
    }
    return h;
}

/// Forward and inverse coordinate mapping between Source Image (srcW x srcH) and Oriented Display Image (dispW x dispH).
struct OrientedDimensions {
    keel::u32 srcW{0};
    keel::u32 srcH{0};
    keel::u32 dispW{0};
    keel::u32 dispH{0};
    ImageRotation rotation{ImageRotation::Deg0};
    bool flipH{false};
    bool flipV{false};

    static constexpr OrientedDimensions Compute(
        keel::u32 w,
        keel::u32 h,
        ImageRotation rot,
        bool fh,
        bool fv
    ) noexcept {
        const bool swapped = (rot == ImageRotation::Deg90 || rot == ImageRotation::Deg270);
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
    constexpr void DisplayToSourcePx(keel::u32 dx, keel::u32 dy, keel::u32& outSx, keel::u32& outSy) const noexcept {
        const keel::u32 rx = flipH ? (dispW - 1 - dx) : dx;
        const keel::u32 ry = flipV ? (dispH - 1 - dy) : dy;
        switch (rotation) {
            case ImageRotation::Deg0:
                outSx = rx;
                outSy = ry;
                break;
            case ImageRotation::Deg90:
                outSx = ry;
                outSy = srcH - 1 - rx;
                break;
            case ImageRotation::Deg180:
                outSx = srcW - 1 - rx;
                outSy = srcH - 1 - ry;
                break;
            case ImageRotation::Deg270:
                outSx = srcW - 1 - ry;
                outSy = rx;
                break;
        }
    }

    /// Maps continuous display coordinates (dx, dy) in [0..dispW]x[0..dispH] to source coordinates (sx, sy).
    [[nodiscard]] constexpr Vec2 DisplayToSourceContinuous(Vec2 d) const noexcept {
        const keel::f32 rx = flipH ? (keel::as<keel::f32>(dispW) - d.x) : d.x;
        const keel::f32 ry = flipV ? (keel::as<keel::f32>(dispH) - d.y) : d.y;
        switch (rotation) {
            case ImageRotation::Deg0:
                return {rx, ry};
            case ImageRotation::Deg90:
                return {ry, keel::as<keel::f32>(srcH) - rx};
            case ImageRotation::Deg180:
                return {keel::as<keel::f32>(srcW) - rx, keel::as<keel::f32>(srcH) - ry};
            case ImageRotation::Deg270:
                return {keel::as<keel::f32>(srcW) - ry, rx};
        }
        return {rx, ry};
    }

    /// Maps continuous source coordinates (sx, sy) in [0..srcW]x[0..srcH] to display coordinates (dx, dy).
    [[nodiscard]] constexpr Vec2 SourceToDisplayContinuous(Vec2 s) const noexcept {
        keel::f32 rx = s.x;
        keel::f32 ry = s.y;
        switch (rotation) {
            case ImageRotation::Deg0:
                rx = s.x;
                ry = s.y;
                break;
            case ImageRotation::Deg90:
                rx = keel::as<keel::f32>(srcH) - s.y;
                ry = s.x;
                break;
            case ImageRotation::Deg180:
                rx = keel::as<keel::f32>(srcW) - s.x;
                ry = keel::as<keel::f32>(srcH) - s.y;
                break;
            case ImageRotation::Deg270:
                rx = s.y;
                ry = keel::as<keel::f32>(srcW) - s.x;
                break;
        }
        const keel::f32 dx = flipH ? (keel::as<keel::f32>(dispW) - rx) : rx;
        const keel::f32 dy = flipV ? (keel::as<keel::f32>(dispH) - ry) : ry;
        return {dx, dy};
    }
};

struct ProfileCrossSection {
    bool active{false};
    bool dragging{false};
    Vec2 startSrc{0.0f, 0.0f};
    Vec2 endSrc{0.0f, 0.0f};
    keel::Array<keel::f32> samplesLum{};
    keel::Array<keel::f32> samplesR{};
    keel::Array<keel::f32> samplesG{};
    keel::Array<keel::f32> samplesB{};
    keel::f32 minVal{0.0f};
    keel::f32 maxVal{0.0f};
    keel::f32 meanVal{0.0f};

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

        const keel::f32 dx = endSrc.x - startSrc.x;
        const keel::f32 dy = endSrc.y - startSrc.y;
        const keel::f32 dist = keel::hypot(dx, dy);
        const keel::i32 n = keel::clamp(keel::round_i32(dist) + 1, 2, 2048);

        samplesLum.resize(keel::as<keel::usize>(n));
        if (data.channels >= 3) {
            samplesR.resize(keel::as<keel::usize>(n));
            samplesG.resize(keel::as<keel::usize>(n));
            samplesB.resize(keel::as<keel::usize>(n));
        }

        keel::f32 mn = 1e30f;
        keel::f32 mx = -1e30f;
        keel::f64 sum = 0.0;

        for (keel::i32 i = 0; i < n; ++i) {
            const keel::f32 t = (n > 1) ? (keel::as<keel::f32>(i) / keel::as<keel::f32>(n - 1)) : 0.0f;
            const keel::u32 sx = keel::min(data.width - 1, keel::as<keel::u32>(keel::max(0.0f, keel::floor(startSrc.x + t * dx))));
            const keel::u32 sy = keel::min(data.height - 1, keel::as<keel::u32>(keel::max(0.0f, keel::floor(startSrc.y + t * dy))));
            const auto ch = ReadPixelChannels(data, sx, sy);

            if (data.channels == 1) {
                const keel::f32 v = ch[0];
                samplesLum[keel::as<keel::usize>(i)] = v;
                if (v < mn) mn = v;
                if (v > mx) mx = v;
                sum += keel::as<keel::f64>(v);
            } else {
                const keel::f32 lum = ComputeLuminance(ch[0], ch[1], ch[2]);
                samplesLum[keel::as<keel::usize>(i)] = lum;
                samplesR[keel::as<keel::usize>(i)] = ch[0];
                samplesG[keel::as<keel::usize>(i)] = ch[1];
                samplesB[keel::as<keel::usize>(i)] = ch[2];
                mn = keel::min(keel::min(mn, ch[0]), keel::min(ch[1], ch[2]));
                mx = keel::max(keel::max(mx, ch[0]), keel::max(ch[1], ch[2]));
                sum += keel::as<keel::f64>(lum);
            }
        }

        minVal = (mn <= mx) ? mn : 0.0f;
        maxVal = (mn <= mx) ? mx : 0.0f;
        meanVal = keel::as<keel::f32>(sum / keel::as<keel::f64>(n));
    }
};

struct ImageViewInternalState {
    keel::u64 cachedDataFp{0};
    keel::f32 dataMin{0.0f};
    keel::f32 dataMax{65535.0f};
    keel::f32 dataMean{0.0f};
    keel::StaticArray<keel::u32, kHistBins> binsLum{};
    keel::StaticArray<keel::u32, kHistBins> binsR{};
    keel::StaticArray<keel::u32, kHistBins> binsG{};
    keel::StaticArray<keel::u32, kHistBins> binsB{};

    // Cached tone-mapped & oriented ABGR8888 display buffer (rebuilt ONLY when data or tonal/transform params change)
    keel::u64 cachedRenderKey{0};
    keel::u32 displayW{0};
    keel::u32 displayH{0};
    keel::Array<keel::u32> displayAbgr{};

    void TrimInactiveCaches() noexcept {
        displayAbgr = keel::Array<keel::u32>{};
        cachedRenderKey = 0;
        displayW = 0;
        displayH = 0;
    }

    // Viewport camera state
    keel::f32 zoom{1.0f};
    Vec2 pan{0.0f, 0.0f};
    bool fitPending{true};
    keel::f32 lastCanvasW{0.0f};
    keel::f32 lastCanvasH{0.0f};
    bool panning{false};
    Vec2 panGrabMouse{0.0f, 0.0f};
    Vec2 panGrabStart{0.0f, 0.0f};

    // Direct Ctrl+LMB Window/Level drag state
    bool wlDragging{false};
    Vec2 wlGrabMouse{0.0f, 0.0f};
    keel::f32 wlGrabWindow{1.0f};
    keel::f32 wlGrabLevel{0.5f};

    // Double-click detector
    keel::f64 lastClickTimeSec{-10.0};
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

        const keel::u64 fp = ComputeImageFingerprint(data);
        if (fp == cachedDataFp && params.initialized) {
            return;
        }

        const bool firstLoad = (cachedDataFp == 0) || !params.initialized;
        cachedDataFp = fp;

        keel::f32 mn = 1e30f;
        keel::f32 mx = -1e30f;
        keel::f64 sum = 0.0;
        const keel::usize numPixels = keel::as<keel::usize>(data.width) * data.height;

        if (data.channels == 1) {
            for (keel::usize i = 0; i < numPixels; ++i) {
                const keel::f32 v = ReadSampleAsFloat(data, i);
                if (keel::is_finite(v)) {
                    if (v < mn) mn = v;
                    if (v > mx) mx = v;
                    sum += keel::as<keel::f64>(v);
                }
            }
        } else {
            for (keel::usize i = 0; i < numPixels; ++i) {
                const keel::usize base = i * data.channels;
                const keel::f32 r = ReadSampleAsFloat(data, base + 0);
                const keel::f32 g = ReadSampleAsFloat(data, base + 1);
                const keel::f32 b = ReadSampleAsFloat(data, base + 2);
                if (keel::is_finite(r) && keel::is_finite(g) && keel::is_finite(b)) {
                    mn = keel::min(keel::min(mn, r), keel::min(g, b));
                    mx = keel::max(keel::max(mx, r), keel::max(g, b));
                    sum += keel::as<keel::f64>(ComputeLuminance(r, g, b));
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
        dataMean = keel::as<keel::f32>(sum / keel::as<keel::f64>(keel::max<keel::usize>(1, numPixels)));

        binsLum.fill(0);
        binsR.fill(0);
        binsG.fill(0);
        binsB.fill(0);

        const keel::f32 scale = keel::as<keel::f32>(kHistBins - 1) / (dataMax - dataMin);
        if (data.channels == 1) {
            for (keel::usize i = 0; i < numPixels; ++i) {
                const keel::f32 v = ReadSampleAsFloat(data, i);
                if (keel::is_finite(v)) {
                    const keel::usize bin = keel::as<keel::usize>(keel::clamp((v - dataMin) * scale, 0.0f, keel::as<keel::f32>(kHistBins - 1)));
                    binsLum[bin]++;
                }
            }
        } else {
            for (keel::usize i = 0; i < numPixels; ++i) {
                const keel::usize base = i * data.channels;
                const keel::f32 r = ReadSampleAsFloat(data, base + 0);
                const keel::f32 g = ReadSampleAsFloat(data, base + 1);
                const keel::f32 b = ReadSampleAsFloat(data, base + 2);
                const keel::f32 lum = ComputeLuminance(r, g, b);
                const keel::usize bL = keel::as<keel::usize>(keel::clamp((lum - dataMin) * scale, 0.0f, keel::as<keel::f32>(kHistBins - 1)));
                const keel::usize bR = keel::as<keel::usize>(keel::clamp((r - dataMin) * scale, 0.0f, keel::as<keel::f32>(kHistBins - 1)));
                const keel::usize bG = keel::as<keel::usize>(keel::clamp((g - dataMin) * scale, 0.0f, keel::as<keel::f32>(kHistBins - 1)));
                const keel::usize bB = keel::as<keel::usize>(keel::clamp((b - dataMin) * scale, 0.0f, keel::as<keel::f32>(kHistBins - 1)));
                binsLum[bL]++;
                binsR[bR]++;
                binsG[bG]++;
                binsB[bB]++;
            }
        }

        if (firstLoad) {
            const keel::f32 initMin = (data.format == ImageSampleFormat::UInt8 && params.autoStrength == AutoWindowStrength::Off) ? 0.0f : dataMin;
            const keel::f32 initMax = (data.format == ImageSampleFormat::UInt8 && params.autoStrength == AutoWindowStrength::Off) ? 255.0f : dataMax;
            for (auto& ch : params.channels) {
                ch.ResetToRange(initMin, initMax);
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
        keel::usize pixelCount = 0;
        if (!keel::checked_mul(keel::as<keel::usize>(od.dispW), keel::as<keel::usize>(od.dispH), pixelCount)) {
            displayW = 0;
            displayH = 0;
            displayAbgr.clear();
            cachedRenderKey = 0;
            return;
        }

        keel::u64 rKey = detail::HashArgs(
            cachedDataFp,
            od.dispW,
            od.dispH,
            keel::as<keel::u8>(params.rotation),
            params.flipHorizontal,
            params.flipVertical,
            keel::as<keel::u8>(params.displayChannel),
            keel::as<keel::u8>(params.colormap)
        );
        for (const auto& ch : params.channels) {
            rKey = detail::HashArgs(rKey, ch.window, ch.level, ch.gamma, ch.invert);
        }

        if (rKey == cachedRenderKey && displayW == od.dispW && displayH == od.dispH &&
            displayAbgr.size() == pixelCount) {
            return;
        }

        if (displayW != od.dispW || displayH != od.dispH) {
            fitPending = true;
        }
        cachedRenderKey = rKey;
        displayW = od.dispW;
        displayH = od.dispH;
        displayAbgr.resize(pixelCount);

        const keel::f32 minV = dataMin;
        const keel::f32 spanV = keel::max(1e-5f, dataMax - dataMin);
        const keel::f32 invSpanLut = keel::as<keel::f32>(kToneLutSize - 1) / spanV;

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
            keel::StaticArray<keel::u32, kToneLutSize> toneAbgrLut{};
            for (keel::usize i = 0; i < kToneLutSize; ++i) {
                const keel::f32 val = minV + (keel::as<keel::f32>(i) / keel::as<keel::f32>(kToneLutSize - 1)) * spanV;
                keel::f32 t = activeTp->Evaluate(val);
                if (data.channels >= 3 && params.displayChannel != ImageDisplayChannel::CompositeRgb &&
                    params.displayChannel != ImageDisplayChannel::LuminanceOnly) {
                    // Also apply Master curve if an individual R/G/B channel is isolated
                    const keel::f32 masterVal = minV + t * spanV;
                    t = params.channels[0].Evaluate(masterVal);
                }
                const keel::usize cIdx = keel::as<keel::usize>(keel::clamp(keel::round_i32(t * 255.0f), 0, 255));
                toneAbgrLut[i] = cmapLut[cIdx];
            }

            for (keel::u32 dy = 0; dy < displayH; ++dy) {
                keel::u32* rowOut = displayAbgr.data() + keel::as<keel::usize>(dy) * displayW;
                for (keel::u32 dx = 0; dx < displayW; ++dx) {
                    keel::u32 sx = 0, sy = 0;
                    od.DisplayToSourcePx(dx, dy, sx, sy);
                    keel::f32 sample = 0.0f;
                    if (data.channels == 1) {
                        sample = ReadSampleAsFloat(data, keel::as<keel::usize>(sy) * data.width + sx);
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
                    const keel::usize lutIdx = keel::as<keel::usize>(keel::clamp((sample - minV) * invSpanLut, 0.0f, keel::as<keel::f32>(kToneLutSize - 1)));
                    rowOut[dx] = toneAbgrLut[lutIdx];
                }
            }
        } else {
            // Full Multi-Channel RGB Composite Mode: build 4096-entry LUTs for R, G, B combining Channel + Master curves
            keel::StaticArray<keel::u8, kToneLutSize> lutR{};
            keel::StaticArray<keel::u8, kToneLutSize> lutG{};
            keel::StaticArray<keel::u8, kToneLutSize> lutB{};

            for (keel::usize i = 0; i < kToneLutSize; ++i) {
                const keel::f32 val = minV + (keel::as<keel::f32>(i) / keel::as<keel::f32>(kToneLutSize - 1)) * spanV;
                const keel::f32 tr = params.channels[0].Evaluate(minV + params.channels[1].Evaluate(val) * spanV);
                const keel::f32 tg = params.channels[0].Evaluate(minV + params.channels[2].Evaluate(val) * spanV);
                const keel::f32 tb = params.channels[0].Evaluate(minV + params.channels[3].Evaluate(val) * spanV);
                lutR[i] = keel::as<keel::u8>(keel::clamp(keel::round_i32(tr * 255.0f), 0, 255));
                lutG[i] = keel::as<keel::u8>(keel::clamp(keel::round_i32(tg * 255.0f), 0, 255));
                lutB[i] = keel::as<keel::u8>(keel::clamp(keel::round_i32(tb * 255.0f), 0, 255));
            }

            const keel::f32 maxIdxF = keel::as<keel::f32>(kToneLutSize - 1);
            for (keel::u32 dy = 0; dy < displayH; ++dy) {
                keel::u32* rowOut = displayAbgr.data() + keel::as<keel::usize>(dy) * displayW;
                for (keel::u32 dx = 0; dx < displayW; ++dx) {
                    keel::u32 sx = 0, sy = 0;
                    od.DisplayToSourcePx(dx, dy, sx, sy);
                    const auto ch = ReadPixelChannels(data, sx, sy);
                    const keel::usize ir = keel::as<keel::usize>(keel::clamp((ch[0] - minV) * invSpanLut, 0.0f, maxIdxF));
                    const keel::usize ig = keel::as<keel::usize>(keel::clamp((ch[1] - minV) * invSpanLut, 0.0f, maxIdxF));
                    const keel::usize ib = keel::as<keel::usize>(keel::clamp((ch[2] - minV) * invSpanLut, 0.0f, maxIdxF));
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
    keel::StaticArray<Rect, 8> toolBtnRects{};
    UiSize toolbarSize{UiSize::Sm};
    bool hasTopBar{false};
    bool hasStatusBar{false};

    static constexpr keel::StaticArray<keel::StringRef, 8> kToolLabels = {
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
        constexpr keel::f32 scale = 1.0f;
        const ComponentMetrics& cm = theme.GetMetrics(g.toolbarSize);
        const keel::f32 gap = theme.GetGap(g.toolbarSize);

        g.hasTopBar = opts.showTopToolbar && (box.h >= 120.0f * scale);
        g.hasStatusBar = opts.showStatusBar && (box.h >= 90.0f * scale);

        const keel::f32 bh = cm.defaultHeight;
        const keel::f32 padY = keel::max(4.0f * scale, gap * 0.65f);
        const keel::f32 topH = g.hasTopBar ? (bh + padY * 2.0f) : 0.0f;
        const keel::f32 botH = g.hasStatusBar ? (26.0f * scale) : 0.0f;

        if (g.hasTopBar) {
            g.topBarRect = Rect(box.x + 1.0f, box.y + 1.0f, keel::max(0.0f, box.w - 2.0f), topH);
            keel::f32 bx = g.topBarRect.x + gap;
            const keel::f32 by = g.topBarRect.y + padY;
            for (keel::usize i = 0; i < kToolLabels.size(); ++i) {
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
                const Vec2 bSz = detail::Button::Measure(bOpts, g.toolbarSize, canvas, theme);
                g.toolBtnRects[i] = Rect(bx, by, bSz.x, bSz.y);
                bx += bSz.x + gap * 0.75f;
            }
        }
        if (g.hasStatusBar) {
            g.statusRect = Rect(box.x + 1.0f, box.Bottom() - botH - 1.0f, keel::max(0.0f, box.w - 2.0f), botH);
        }

        const keel::f32 canvasY = box.y + 1.0f + topH;
        const keel::f32 canvasH = keel::max(16.0f, box.h - 2.0f - topH - botH);
        g.canvasRect = Rect(box.x + 1.0f, canvasY, keel::max(16.0f, box.w - 2.0f), canvasH);
        return g;
    }
};

void FitImageToCanvas(ImageViewInternalState& st, const Rect& canvasRect) noexcept {
    if (st.displayW == 0 || st.displayH == 0 || canvasRect.w <= 4.0f || canvasRect.h <= 4.0f) {
        return;
    }
    const keel::f32 pad = 16.0f;
    const keel::f32 availW = keel::max(16.0f, canvasRect.w - pad * 2.0f);
    const keel::f32 availH = keel::max(16.0f, canvasRect.h - pad * 2.0f);
    const keel::f32 zx = availW / keel::as<keel::f32>(st.displayW);
    const keel::f32 zy = availH / keel::as<keel::f32>(st.displayH);
    st.zoom = keel::clamp(keel::min(zx, zy), 0.05f, 64.0f);
    st.pan.x = (canvasRect.w - keel::as<keel::f32>(st.displayW) * st.zoom) * 0.5f;
    st.pan.y = (canvasRect.h - keel::as<keel::f32>(st.displayH) * st.zoom) * 0.5f;
    st.fitPending = false;
}

void ZoomAroundPoint(ImageViewInternalState& st, const Rect& canvasRect, Vec2 focusScreenPt, keel::f32 newZoom) noexcept {
    newZoom = keel::clamp(newZoom, 0.05f, 64.0f);
    const keel::f32 relX = focusScreenPt.x - canvasRect.x;
    const keel::f32 relY = focusScreenPt.y - canvasRect.y;
    const keel::f32 imgX = (relX - st.pan.x) / keel::max(1e-4f, st.zoom);
    const keel::f32 imgY = (relY - st.pan.y) / keel::max(1e-4f, st.zoom);
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
    keel::i32 hoveredToolBtn{-1};
};

ImageViewResponse RunViewportNode(
    Context& ctx,
    Id id,
    const ImageViewData& data,
    ImageViewParams& params,
    ImageViewInternalState& st,
    const ImageViewOptions& opts,
    bool standaloneElement
) {
    const UiTheme& theme = ctx.GetTheme();
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    constexpr keel::f32 scale = 1.0f;

    Response baseRes = ctx.Interact(id);
    ImageViewResponse res{};
    static_cast<Response&>(res) = baseRes;

    const Rect boxRect = baseRes.rect;

    const ViewportLayoutGeom vgeom = ViewportLayoutGeom::Compute(boxRect, opts, activeSize, ctx.GetCanvas(), theme);
    const Rect cr = vgeom.canvasRect;

    if (keel::abs(cr.w - st.lastCanvasW) > 2.0f || keel::abs(cr.h - st.lastCanvasH) > 2.0f) {
        st.fitPending = true;
        st.lastCanvasW = cr.w;
        st.lastCanvasH = cr.h;
    }

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
    keel::i32 hoveredToolBtn = -1;
    bool clickedToolbar = false;
    if (!effectiveDisabled && vgeom.hasTopBar && baseRes.hovered) {
        for (keel::usize i = 0; i < vgeom.toolBtnRects.size(); ++i) {
            if (vgeom.toolBtnRects[i].Contains(mouse)) {
                hoveredToolBtn = keel::as<keel::i32>(i);
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
                            params.rotation = keel::from_underlying<ImageRotation>((keel::as<keel::u8>(params.rotation) + 1) % 4);
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
        if (baseRes.hovered && inCanvas && keel::abs(wheel.y) > 1e-3f) {
            const keel::f32 factor = keel::pow(1.15f, wheel.y);
            ZoomAroundPoint(st, cr, mouse, st.zoom * factor);
            ctx.ConsumeMouseWheel();
            res.changed = true;
        }

        // LMB / MMB Press: Double-click Fit/100%, Ctrl+LMB W/L drag, or Pan
        if (baseRes.hovered && inCanvas && ctx.IsMouseClicked()) {
            const keel::f64 nowSec = ctx.GetCurrentTime();
            const keel::f32 clickDist = keel::hypot(mouse.x - st.lastClickPos.x, mouse.y - st.lastClickPos.y);
            if (baseRes.doubleClicked || ((nowSec - st.lastClickTimeSec) < 0.32 && clickDist < 8.0f * scale)) {
                if (keel::abs(st.zoom - 1.0f) < 0.05f) {
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
                    const keel::usize chIdx = keel::as<keel::usize>(params.activeHistChannel);
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
                const keel::usize chIdx = keel::as<keel::usize>(params.activeHistChannel);
                const keel::f32 domainSpan = keel::max(1e-4f, st.dataMax - st.dataMin);
                const keel::f32 sensitivity = (mods.shift ? 0.25f : 1.0f) * (domainSpan / 320.0f);
                const keel::f32 dx = mouse.x - st.wlGrabMouse.x;
                const keel::f32 dy = mouse.y - st.wlGrabMouse.y;
                params.channels[chIdx].window = keel::clamp(st.wlGrabWindow + dx * sensitivity, domainSpan * 0.002f, domainSpan * 2.0f);
                params.channels[chIdx].level  = keel::clamp(st.wlGrabLevel  - dy * sensitivity, st.dataMin, st.dataMax);
                params.autoStrength = AutoWindowStrength::Off;
                res.paramsChanged = true;
                res.changed = true;
                res.pressed = true;
            } else {
                st.wlDragging = false;
                if (ctx.GetActiveId() == id) {
                    ctx.SetActiveId(Id::Null());
                }
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
                if (ctx.GetActiveId() == id) {
                    ctx.SetActiveId(Id::Null());
                }
            }
        }

        // RMB Line Profile Tool
        if (opts.enableLineProfile) {
            auto screenToSourceClamped = [&](Vec2 scrPt) noexcept -> Vec2 {
                const keel::f32 dispX = keel::clamp((scrPt.x - cr.x - st.pan.x) / keel::max(1e-4f, st.zoom), 0.0f, keel::as<keel::f32>(od.dispW));
                const keel::f32 dispY = keel::clamp((scrPt.y - cr.y - st.pan.y) / keel::max(1e-4f, st.zoom), 0.0f, keel::as<keel::f32>(od.dispH));
                const Vec2 srcPt = od.DisplayToSourceContinuous({dispX, dispY});
                return {
                    keel::clamp(srcPt.x, 0.0f, keel::as<keel::f32>( keel::max(1u, data.width) - 1)),
                    keel::clamp(srcPt.y, 0.0f, keel::as<keel::f32>( keel::max(1u, data.height) - 1))
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
                    if (ctx.GetActiveId() == id) {
                        ctx.SetActiveId(Id::Null());
                    }
                    if (keel::hypot(st.profile.endSrc.x - st.profile.startSrc.x,
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
        const keel::f32 dispX = (mouse.x - cr.x - st.pan.x) / keel::max(1e-4f, st.zoom);
        const keel::f32 dispY = (mouse.y - cr.y - st.pan.y) / keel::max(1e-4f, st.zoom);
        if (dispX >= 0.0f && dispY >= 0.0f &&
            dispX < keel::as<keel::f32>(st.displayW) && dispY < keel::as<keel::f32>(st.displayH)) {
            keel::u32 sx = 0, sy = 0;
            od.DisplayToSourcePx(keel::as<keel::u32>(dispX), keel::as<keel::u32>(dispY), sx, sy);
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
    res.profileSampleCount = keel::as<keel::u32>(st.profile.samplesLum.size());
    res.profileMin = st.profile.minVal;
    res.profileMax = st.profile.maxVal;
    res.profileMean = st.profile.meanVal;

    if (baseRes.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, baseRes.rect);
    }

    // 4. Create Scene Graph Node for the Viewport
    Node* node = ctx.CreateNode(NodeType::ImageView, id);
    auto* payload = ctx.AllocPayload<ImageViewViewportPayload>();
    payload->data = data;
    payload->params = params;
    payload->opts = opts;
    payload->opts.tooltip = ctx.AllocText(opts.tooltip);
    payload->resp = res;
    payload->state = &st;
    payload->hoveredToolBtn = hoveredToolBtn;
    node->SetPayload(payload);

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = baseRes.hovered;
    node->active = res.pressed;
    node->focused = baseRes.focused;

    const keel::f32 effectiveMinHDp = ResolveEffectiveMinHeightDp(opts);
    const keel::f32 effectiveMinH = effectiveMinHDp;

    if (standaloneElement) {
        ControlSpec ctrlSpec{};
        ctrlSpec.width = opts.width;
        ctrlSpec.defaultWidthMode = SizeMode::Stretch;
        ctrlSpec.defaultHeightMode = SizeMode::Fit;
        if (opts.height.IsFixed()) {
            ctrlSpec.height = effectiveMinHDp;
        } else if (opts.height.IsStretch()) {
            ctrlSpec.height = opts.height;
        } else {
            ctrlSpec.height = SizeMode::Fit;
        }
        ctrlSpec.minHeight = effectiveMinHDp;
        Control::SetupNode(ctx, node, ctrlSpec);
    } else {
        node->widthMode = SizeMode::Stretch;
        node->flexGrow = 1.0f;
        node->heightMode = SizeMode::Stretch;
        node->minHeight = effectiveMinH;
        ctx.AdaptChildToContainer(node);
    }

    node->visualHash = detail::HashArgs(
        node->type,
        st.cachedRenderKey,
        st.zoom,
        keel::round(st.pan.x * 2.0f),
        keel::round(st.pan.y * 2.0f),
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

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* c, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<ImageViewViewportPayload>();
        if (!p) return {0.0f, 0.0f};
        ImageViewOptions effOpts = p->opts;
        if (n->widthMode == SizeMode::Stretch && !effOpts.width.IsFixed()) {
            effOpts.width = SizeMode::Stretch;
        }
        return detail::ImageView::Measure(p->data, effOpts, n->uiSize, c, th, availableWidth);
    };

    node->renderFn = [](const Node* n, ICanvas& c, const UiTheme& th) noexcept {
        const auto* p = n->As<ImageViewViewportPayload>();
        if (!p || !p->state) return;

        const ImageViewData& d = p->data;
        const ImageViewParams& prm = p->params;
        const ImageViewOptions& o = p->opts;
        const ImageViewResponse& resp = p->resp;
        ImageViewInternalState& s = *p->state;
        constexpr keel::f32 scale = 1.0f;

        const Rect box = n->computedRect;
        if (box.w <= 8.0f || box.h <= 8.0f) return;

        const bool isDark = (th.mode == ThemeMode::Dark);
        const keel::f32 rad = o.border.radius.has_value() ? *o.border.radius : th.GetRadius(n->uiSize);
        const Color defaultBgOuter = isDark ? Color(12, 16, 24, 255) : Color(241, 245, 249, 255);
        const Color bgOuter   = th.ResolveColor(o.background, n->hovered, n->active, n->disabled, defaultBgOuter);
        const Color bgCanvas  = isDark ? Color(8, 10, 16, 255)  : Color(226, 232, 240, 255);
        const Color barBg     = isDark ? Color(17, 24, 39, 255) : Color(248, 250, 252, 255);
        const Color borderCol = th.ResolveColor(o.border.color, n->hovered, n->active, n->disabled, th.palette.borderSubtle);
        const keel::f32 borderW   = (o.border.width > 0.0f) ? o.border.width : 1.0f;
        const keel::f32 smallFs = th.GetFontSize(TextRole::LabelSmall) * 0.92f;

        c.DrawRoundRect(box, rad, bgOuter, borderCol, borderW);

        const ViewportLayoutGeom vg = ViewportLayoutGeom::Compute(box, o, n->uiSize, &c, th);
        const ComponentMetrics& tbCm = th.GetMetrics(vg.toolbarSize);
        const Rect cr = vg.canvasRect;

        if (keel::abs(cr.w - s.lastCanvasW) > 2.0f || keel::abs(cr.h - s.lastCanvasH) > 2.0f) {
            s.fitPending = true;
            s.lastCanvasW = cr.w;
            s.lastCanvasH = cr.h;
        }

        if (s.fitPending && cr.w > 32.0f && cr.h > 32.0f && s.displayW > 0 && s.displayH > 0) {
            FitImageToCanvas(s, cr);
        }

        // 1. Top Quick Toolbar (sized purely by UiSize / ComponentMetrics)
        if (vg.hasTopBar) {
            c.PushClip(vg.topBarRect);
            c.DrawRect(vg.topBarRect, barBg);
            c.DrawLine({vg.topBarRect.x, vg.topBarRect.Bottom()}, {vg.topBarRect.Right(), vg.topBarRect.Bottom()}, borderCol, 1.0f);

            for (keel::usize i = 0; i < ViewportLayoutGeom::kToolLabels.size(); ++i) {
                const bool isHovered = (p->hoveredToolBtn == keel::as<keel::i32>(i));
                const bool activeToggle =
                    (i == 5 && prm.flipHorizontal) ||
                    (i == 6 && prm.flipVertical) ||
                    (i == 4 && prm.rotation != ImageRotation::Deg0);
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
                bOpts.variant = activeToggle ? UiVariant::Primary : UiVariant::Secondary;
                bOpts.active = activeToggle;
                bOpts.width = SizeMode::Stretch;
                detail::Button::Render(c, br, bOpts, vg.toolbarSize, isHovered, false, n->disabled, th, false);
            }

            // Right side of top bar: active palette / mode badge
            char modeBuf[64]{};
            if (d.channels == 1 || prm.displayChannel != ImageDisplayChannel::CompositeRgb) {
                std::snprintf(modeBuf, sizeof(modeBuf), "%.*s%s | %.0f%%",
                              keel::as<keel::i32>(GetColormapName(prm.colormap).size()),
                              GetColormapName(prm.colormap).data(),
                              prm.channels[0].invert ? " (Inv)" : "",
                              s.zoom * 100.0f);
            } else {
                std::snprintf(modeBuf, sizeof(modeBuf), "RGB Composite | %.0f%%", s.zoom * 100.0f);
            }
            const keel::f32 rightX = vg.toolBtnRects[ViewportLayoutGeom::kToolLabels.size() - 1].Right() + 10.0f * scale;
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
                keel::as<keel::f32>(s.displayW) * s.zoom,
                keel::as<keel::f32>(s.displayH) * s.zoom
            );

            c.DrawPixels({s.displayAbgr, s.displayW, s.displayH}, destRect);
            c.DrawRect(Rect(destRect.x - 1.0f, destRect.y - 1.0f, destRect.w + 2.0f, 1.0f), Color(100, 116, 139, 130));
            c.DrawRect(Rect(destRect.x - 1.0f, destRect.Bottom(), destRect.w + 2.0f, 1.0f), Color(100, 116, 139, 130));
            c.DrawRect(Rect(destRect.x - 1.0f, destRect.y, 1.0f, destRect.h), Color(100, 116, 139, 130));
            c.DrawRect(Rect(destRect.Right(), destRect.y, 1.0f, destRect.h), Color(100, 116, 139, 130));

            // High-zoom pixel grid (>= 12x zoom)
            if (o.showPixelGridAtHighZoom && s.zoom >= 12.0f) {
                const Color gridCol(255, 255, 255, keel::as<keel::u8>(keel::clamp((s.zoom - 10.0f) * 4.0f, 18.0f, 75.0f)));
                const keel::u32 x0 = keel::as<keel::u32>(keel::max(0.0f, keel::floor((cr.x - destRect.x) / s.zoom)));
                const keel::u32 x1 = keel::min(s.displayW, keel::as<keel::u32>(keel::ceil((cr.Right() - destRect.x) / s.zoom)));
                const keel::u32 y0 = keel::as<keel::u32>(keel::max(0.0f, keel::floor((cr.y - destRect.y) / s.zoom)));
                const keel::u32 y1 = keel::min(s.displayH, keel::as<keel::u32>(keel::ceil((cr.Bottom() - destRect.y) / s.zoom)));

                for (keel::u32 gx = x0; gx <= x1; ++gx) {
                    const keel::f32 sx = destRect.x + keel::as<keel::f32>(gx) * s.zoom;
                    c.DrawLine({sx, keel::max(cr.y, destRect.y)}, {sx, keel::min(cr.Bottom(), destRect.Bottom())}, gridCol, 1.0f);
                }
                for (keel::u32 gy = y0; gy <= y1; ++gy) {
                    const keel::f32 sy = destRect.y + keel::as<keel::f32>(gy) * s.zoom;
                    c.DrawLine({keel::max(cr.x, destRect.x), sy}, {keel::min(cr.Right(), destRect.Right()), sy}, gridCol, 1.0f);
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
                const keel::f32 endR = 4.0f * scale;
                c.DrawRoundRect(Rect(p0.x - endR, p0.y - endR, endR * 2.0f, endR * 2.0f), endR, Color(255, 90, 90, 255), Color(15, 23, 42, 255), 1.0f);
                c.DrawRoundRect(Rect(p1.x - endR, p1.y - endR, endR * 2.0f, endR * 2.0f), endR, Color(56, 189, 248, 255), Color(15, 23, 42, 255), 1.0f);

                // Floating Line Profile Graph Panel at bottom of canvas
                const keel::f32 profH = keel::min(124.0f * scale, cr.h * 0.36f);
                const Rect profBox(
                    cr.x + 10.0f * scale,
                    cr.Bottom() - profH - 10.0f * scale,
                    keel::max(80.0f, cr.w - 20.0f * scale),
                    profH
                );
                c.DrawRoundRect(profBox, 6.0f * scale, Color(12, 16, 26, 225), Color(100, 160, 255, 160), 1.0f);

                const Rect profPlot(
                    profBox.x + 8.0f * scale,
                    profBox.y + 22.0f * scale,
                    keel::max(20.0f, profBox.w - 16.0f * scale),
                    keel::max(16.0f, profBox.h - 28.0f * scale)
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

                const keel::f32 pMin = s.profile.minVal;
                const keel::f32 pSpan = keel::max(1e-4f, s.profile.maxVal - pMin);

                auto drawProfileTrace = [&](keel::ArrayRef<keel::f32> vec, Color col, keel::f32 strokeW) {
                    if (vec.size() < 2) return;
                    const keel::usize nPts = vec.size();
                    Vec2 prev{};
                    for (keel::usize i = 0; i < nPts; ++i) {
                        const keel::f32 u = keel::as<keel::f32>(i) / keel::as<keel::f32>(nPts - 1);
                        const keel::f32 vNorm = keel::clamp((vec[i] - pMin) / pSpan, 0.0f, 1.0f);
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

            const auto& masterTp = prm.channels[keel::as<keel::usize>(prm.activeHistChannel)];
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

void detail::ImageView::ApplyQuantileAutoWindow(
    keel::ArrayRef<keel::u32> bins,
    keel::f32 minVal,
    keel::f32 maxVal,
    AutoWindowTarget target,
    AutoWindowStrength strength,
    ChannelTransferParams& ioTransfer
) noexcept {
    const keel::f32 span = keel::max(1e-4f, maxVal - minVal);
    if (bins.empty() || strength == AutoWindowStrength::Off) {
        ioTransfer.SetLowHigh(minVal, maxVal);
        return;
    }

    keel::u64 total = 0;
    for (keel::u32 c : bins) {
        total += c;
    }
    if (total == 0) {
        ioTransfer.SetLowHigh(minVal, maxVal);
        return;
    }

    const keel::f32 tailClip = (strength == AutoWindowStrength::Medium) ? 0.01f : 0.05f;
    const keel::f32 qLow  = (target == AutoWindowTarget::Black || target == AutoWindowTarget::All) ? tailClip : 0.0f;
    const keel::f32 qHigh = (target == AutoWindowTarget::White || target == AutoWindowTarget::All) ? (1.0f - tailClip) : 1.0f;

    auto findQuantileValue = [&](keel::f32 q) noexcept -> keel::f32 {
        if (q <= 0.0f) return minVal;
        if (q >= 1.0f) return maxVal;
        const keel::f64 targetCount = keel::as<keel::f64>(q) * keel::as<keel::f64>(total);
        keel::u64 accum = 0;
        for (keel::usize i = 0; i < bins.size(); ++i) {
            const keel::u64 nextAccum = accum + bins[i];
            if (keel::as<keel::f64>(nextAccum) >= targetCount) {
                const keel::f64 binFraction = (bins[i] > 0)
                    ? ((targetCount - keel::as<keel::f64>(accum)) / keel::as<keel::f64>(bins[i]))
                    : 0.0;
                const keel::f32 norm = (keel::as<keel::f32>(i) + keel::clamp(keel::as<keel::f32>(binFraction), 0.0f, 1.0f)) /
                                   keel::as<keel::f32>(bins.size());
                return minVal + norm * span;
            }
            accum = nextAccum;
        }
        return maxVal;
    };

    const keel::f32 lo = findQuantileValue(qLow);
    const keel::f32 hi = keel::max(lo + span * 0.005f, findQuantileValue(qHigh));
    ioTransfer.SetLowHigh(lo, hi, span * 0.002f);
}

Vec2 detail::ImageView::Measure(
    const ImageViewData& /*data*/,
    const ImageViewOptions& opts,
    UiSize /*size*/,
    ICanvas* /*canvas*/,
    const UiTheme& /*theme*/,
    keel::f32 availableWidth
) noexcept {
    keel::f32 w = 780.0f;
    if (opts.width.IsFixed()) {
        w = opts.width.Dp();
    } else if (opts.width.ResolveMode(SizeMode::Stretch) == SizeMode::Stretch && availableWidth > 0.0f) {
        w = availableWidth;
    }
    const keel::f32 h = ResolveEffectiveMinHeightDp(opts);
    return {w, h};
}

namespace {

ImageViewResponse RunImageViewImpl(
    Context& ctx,
    const ImageViewData& data,
    ImageViewParams* externalParams,
    const ImageViewOptions& opts,
    keel::SourceLocation loc
) {

    const Id rootId = ctx.MakeChildId(loc);
    auto& st = ctx.GetOrCreatePersistentState<ImageViewInternalState>(rootId);
    ImageViewParams& params = externalParams ? *externalParams : st.internalParams;

    ImageViewOptions optsCopy = opts;

    // 1. Update cached statistics, 256-bin histograms, and display pixel buffer
    st.EnsureStatsAndHistogram(data, params);
    st.EnsureDisplayPixels(data, params);

    if (!optsCopy.showSidebar) {
        ImageViewResponse vpRes = RunViewportNode(ctx, rootId, data, params, st, optsCopy, true);
        if (Node* cNode = ctx.GetCurrentContainer()) {
            for (Node* ch = cNode->firstChild; ch; ch = ch->nextSibling) {
                if (ch->id == rootId) {
                    break;
                }
            }
        }
        return vpRes;
    }

    // 2. Integrated Workstation Layout (Sidebar + Interactive Viewport)
    bool sidebarChangedParams = false;
    ImageViewResponse viewportRes{};

    auto renderSidebar = [&]() {
        if (auto sidePanel = guikit::Sidebar(ctx, {
            .side = optsCopy.sidebarOnRight ? SidebarSide::Right : SidebarSide::Left,
            .width = optsCopy.sidebarWidth,
            .scrollable = true,
            .gap = 8.0f,
            .padding = Padding::All(10.0f)
        })) {
            // --- Section 1: Histogram & Interactive Window/Level ---
            guikit::Text(ctx, "Histogram & Window/Level", {.role = TextRole::LabelLarge});

            const HistogramData histData = (data.channels >= 3)
                ? HistogramData::Rgb(st.binsLum, st.binsR, st.binsG, st.binsB, st.dataMin, st.dataMax)
                : HistogramData::Mono(st.binsLum, st.dataMin, st.dataMax);

            const keel::usize activeChIdx = (data.channels >= 3)
                ? keel::as<keel::usize>(params.activeHistChannel)
                : 0u;
            ChannelTransferParams& activeTp = params.channels[activeChIdx];

            HistogramOptions histOpts{};
            histOpts.height = 142.0f;
            histOpts.scaleMode = params.histScaleMode;
            histOpts.activeChannel = (data.channels >= 3) ? params.activeHistChannel : HistogramChannel::Master;
            histOpts.colormap = params.colormap;
            histOpts.showChannelPills = (data.channels >= 3);
            histOpts.multiChannelParams = params.channels;
            histOpts.size = UiSize::Sm;
            histOpts.tooltip = "Drag left/right markers for black/white points, center band for level, double-click to reset";

            const HistogramResponse hRes = guikit::Histogram(ctx, histData, activeTp, histOpts);
            if (hRes.channelChanged) {
                params.activeHistChannel = hRes.activeChannel;
            }
            if (hRes.windowLevelChanged) {
                params.autoStrength = AutoWindowStrength::Off;
                sidebarChangedParams = true;
            }

            // Multi-channel isolation selector when image has >= 3 channels
            if (data.channels >= 3) {
                static constexpr keel::StaticArray<keel::StringRef, 5> kDispChNames = {
                    "Full RGB Composite",
                    "Isolate Red Channel",
                    "Isolate Green Channel",
                    "Isolate Blue Channel",
                    "Isolate Luminance"
                };
                keel::i32 dispChIdx = keel::as<keel::i32>(params.displayChannel);
                if (auto el = guikit::Element(ctx, {.label = "View Mode", .size = UiSize::Sm})) {
                    if (guikit::Select(ctx, dispChIdx, kDispChNames, {.size = UiSize::Sm}).changed) {
                        params.displayChannel = keel::from_underlying<ImageDisplayChannel>(dispChIdx);
                        if (params.displayChannel == ImageDisplayChannel::RedOnly) params.activeHistChannel = HistogramChannel::Red;
                        else if (params.displayChannel == ImageDisplayChannel::GreenOnly) params.activeHistChannel = HistogramChannel::Green;
                        else if (params.displayChannel == ImageDisplayChannel::BlueOnly) params.activeHistChannel = HistogramChannel::Blue;
                        else params.activeHistChannel = HistogramChannel::Master;
                        sidebarChangedParams = true;
                    }
                }
            }

            // Window / Level / Gamma sliders for the active channel
            const keel::f32 span = keel::max(1.0f, st.dataMax - st.dataMin);
            const keel::f32 minW = keel::max(0.01f, span * 0.002f);
            ChannelTransferParams& curTp = params.channels[keel::as<keel::usize>((data.channels >= 3) ? params.activeHistChannel : HistogramChannel::Master)];
            const keel::StringRef wlFmt = (span >= 50.0f) ? "%.0f" : "%.2f";

            if (auto el = guikit::Element(ctx, {.label = "W (Window)", .size = UiSize::Sm})) {
                if (guikit::Slider(ctx, curTp.window, minW, span * 1.5f, {
                    .valueFormat = wlFmt,
                    .showValue = true,
                    .size = UiSize::Sm
                }).changed) {
                    params.autoStrength = AutoWindowStrength::Off;
                    sidebarChangedParams = true;
                }
            }

            if (auto el = guikit::Element(ctx, {.label = "L (Level)", .size = UiSize::Sm})) {
                if (guikit::Slider(ctx, curTp.level, st.dataMin, st.dataMax, {
                    .valueFormat = wlFmt,
                    .showValue = true,
                    .size = UiSize::Sm
                }).changed) {
                    params.autoStrength = AutoWindowStrength::Off;
                    sidebarChangedParams = true;
                }
            }

            if (auto el = guikit::Element(ctx, {.label = "Gamma", .size = UiSize::Sm})) {
                if (guikit::Slider(ctx, curTp.gamma, 0.15f, 4.0f, {
                    .valueFormat = "%.2f",
                    .step = 0.05,
                    .showValue = true,
                    .size = UiSize::Sm
                }).changed) {
                    sidebarChangedParams = true;
                }
            }

            if (auto invRow = guikit::Row(ctx, {.gap = 8.0f, .align = Alignment::Center})) {
                if (auto el = guikit::Element(ctx, {.label = "Invert", .size = UiSize::Sm})) {
                    if (guikit::Checkbox(ctx, curTp.invert, {.size = UiSize::Sm}).changed) {
                        sidebarChangedParams = true;
                    }
                }
                guikit::Spacer(ctx);
                if (guikit::Button(ctx, {.text = "Reset W/L", .size = UiSize::Sm, .variant = UiVariant::Secondary}).clicked) {
                    for (auto& ch : params.channels) {
                        ch.ResetToRange(st.dataMin, st.dataMax);
                    }
                    params.autoStrength = AutoWindowStrength::Off;
                    sidebarChangedParams = true;
                }
            }

            // --- Section 2: Colormap (for 1-channel or isolated channel) ---
            const bool canUseColormap = (data.channels == 1) || (params.displayChannel != ImageDisplayChannel::CompositeRgb);
            {
                keel::i32 cmapIdx = keel::as<keel::i32>(params.colormap);
                if (auto el = guikit::Element(ctx, {
                    .label = "Colormap",
                    .description = canUseColormap ? keel::StringRef{} : "Select an isolated channel to apply Colormap",
                    .size = UiSize::Sm,
                    .disabled = !canUseColormap
                })) {
                    if (guikit::Select(ctx, cmapIdx, GetAllColormapNames(), {.size = UiSize::Sm}).changed) {
                        params.colormap = keel::from_underlying<Colormap>(cmapIdx);
                        sidebarChangedParams = true;
                    }
                }
            }

            // --- Section 3: Auto Window (Quantile Clipping) ---
            guikit::Text(ctx, "Auto Window", {.role = TextRole::LabelLarge});
            static constexpr keel::StaticArray<keel::StringRef, 3> kAutoTargets = {"Black", "White", "All"};
            keel::i32 targetIdx = keel::as<keel::i32>(params.autoTarget);
            if (guikit::StepSlider(ctx, targetIdx, kAutoTargets, {.showValue = false, .size = UiSize::Sm}).changed) {
                params.autoTarget = keel::from_underlying<AutoWindowTarget>(targetIdx);
                if (params.autoStrength != AutoWindowStrength::Off) {
                    keel::ArrayRef<keel::u32> targetBins = st.binsLum;
                    if (data.channels >= 3) {
                        if (params.activeHistChannel == HistogramChannel::Red) targetBins = st.binsR;
                        else if (params.activeHistChannel == HistogramChannel::Green) targetBins = st.binsG;
                        else if (params.activeHistChannel == HistogramChannel::Blue) targetBins = st.binsB;
                    }
                    detail::ImageView::ApplyQuantileAutoWindow(targetBins, st.dataMin, st.dataMax, params.autoTarget, params.autoStrength, curTp);
                    sidebarChangedParams = true;
                }
            }

            if (auto strRow = guikit::Row(ctx, {.gap = 6.0f})) {
                static constexpr keel::StaticArray<keel::StringRef, 3> kStrengthNames = {"Off", "Medium", "Strong"};
                for (keel::usize i = 0; i < 3; ++i) {
                    const auto sMode = keel::from_underlying<AutoWindowStrength>(i);
                    const bool isSel = (params.autoStrength == sMode);
                    if (guikit::Button(ctx, {
                        .text = kStrengthNames[i],
                        .size = UiSize::Sm,
                        .width = SizeMode::Stretch,
                        .variant = isSel ? UiVariant::Primary : UiVariant::Secondary
                    }).clicked) {
                        params.autoStrength = sMode;
                        keel::ArrayRef<keel::u32> targetBins = st.binsLum;
                        if (data.channels >= 3) {
                            if (params.activeHistChannel == HistogramChannel::Red) targetBins = st.binsR;
                            else if (params.activeHistChannel == HistogramChannel::Green) targetBins = st.binsG;
                            else if (params.activeHistChannel == HistogramChannel::Blue) targetBins = st.binsB;
                        }
                        detail::ImageView::ApplyQuantileAutoWindow(targetBins, st.dataMin, st.dataMax, params.autoTarget, params.autoStrength, curTp);
                        sidebarChangedParams = true;
                    }
                }
            }

            if (data.channels >= 3) {
                if (guikit::Button(ctx, {
                    .text = "Auto White Balance (Per-Channel R/G/B)",
                    .size = UiSize::Sm,
                    .width = SizeMode::Stretch,
                    .variant = UiVariant::Secondary
                }).clicked) {
                    params.channels[0].ResetToRange(st.dataMin, st.dataMax);
                    const AutoWindowStrength effStr = (params.autoStrength == AutoWindowStrength::Off)
                        ? AutoWindowStrength::Medium
                        : params.autoStrength;
                    detail::ImageView::ApplyQuantileAutoWindow(st.binsR, st.dataMin, st.dataMax, AutoWindowTarget::All, effStr, params.channels[1]);
                    detail::ImageView::ApplyQuantileAutoWindow(st.binsG, st.dataMin, st.dataMax, AutoWindowTarget::All, effStr, params.channels[2]);
                    detail::ImageView::ApplyQuantileAutoWindow(st.binsB, st.dataMin, st.dataMax, AutoWindowTarget::All, effStr, params.channels[3]);
                    sidebarChangedParams = true;
                }
            }

            // --- Section 4: Transform & View ---
            guikit::Text(ctx, "Transform & View", {.role = TextRole::LabelLarge});
            if (auto zRow = guikit::Row(ctx, {.gap = 6.0f, .align = Alignment::Center})) {
                if (guikit::Button(ctx, {.text = "Fit", .size = UiSize::Sm, .width = SizeMode::Stretch, .variant = UiVariant::Secondary}).clicked) {
                    st.fitPending = true;
                }
                if (guikit::Button(ctx, {.text = "100%", .size = UiSize::Sm, .width = SizeMode::Stretch, .variant = UiVariant::Secondary}).clicked) {
                    st.zoom = 1.0f;
                    st.fitPending = false;
                }
            }

            static constexpr keel::StaticArray<keel::StringRef, 4> kRotLabels = {"0\xC2\xB0", "90\xC2\xB0", "180\xC2\xB0", "270\xC2\xB0"};
            keel::i32 rotIdx = keel::as<keel::i32>(params.rotation);
            if (auto el = guikit::Element(ctx, {.label = "Rotate", .size = UiSize::Sm})) {
                if (guikit::StepSlider(ctx, rotIdx, kRotLabels, {.showValue = false, .size = UiSize::Sm}).changed) {
                    params.rotation = keel::from_underlying<ImageRotation>(rotIdx);
                    st.fitPending = true;
                    sidebarChangedParams = true;
                }
            }

            if (auto flipRow = guikit::Row(ctx, {.gap = 6.0f})) {
                if (guikit::Button(ctx, {
                    .text = "Flip H",
                    .size = UiSize::Sm,
                    .width = SizeMode::Stretch,
                    .variant = params.flipHorizontal ? UiVariant::Primary : UiVariant::Secondary
                }).clicked) {
                    params.flipHorizontal = !params.flipHorizontal;
                    sidebarChangedParams = true;
                }
                if (guikit::Button(ctx, {
                    .text = "Flip V",
                    .size = UiSize::Sm,
                    .width = SizeMode::Stretch,
                    .variant = params.flipVertical ? UiVariant::Primary : UiVariant::Secondary
                }).clicked) {
                    params.flipVertical = !params.flipVertical;
                    sidebarChangedParams = true;
                }
            }
        }
    };

    const keel::f32 effectiveMinHDp = ResolveEffectiveMinHeightDp(optsCopy);
    const SizeMode resolvedOuterW = Control::ResolveWidthMode(ctx, optsCopy.width, SizeMode::Stretch);
    const AxisSize outerWidth = optsCopy.width.IsAuto() ? AxisSize(resolvedOuterW) : optsCopy.width;
    const AxisSize outerHeight = optsCopy.height.IsFixed()
        ? AxisSize::Fixed(effectiveMinHDp)
        : (optsCopy.height.IsStretch() ? optsCopy.height : AxisSize::Fit());

    if (auto outerRow = guikit::Row(ctx, {
        .gap = 8.0f,
        .align = Alignment::Stretch,
        .width = outerWidth,
        .height = outerHeight,
        .minHeight = effectiveMinHDp
    })) {
        if (Node* rNode = outerRow.GetNode()) {
            if (rNode->parent && rNode->parent->type == NodeType::Element) {
                rNode->widthMode = SizeMode::Stretch;
                rNode->heightMode = SizeMode::Stretch;
                rNode->flexGrow = Control::ResolveFlexGrow(rNode->parent, outerWidth, outerHeight, 1.0f);
            } else {
                if (optsCopy.width.IsFixed()) {
                    rNode->widthMode = SizeMode::Fixed;
                    rNode->fixedWidth = optsCopy.width.Dp();
                } else {
                    rNode->widthMode = resolvedOuterW;
                    rNode->fixedWidth = 0.0f;
                }
                const keel::f32 scaledMinH = effectiveMinHDp;
                rNode->minHeight = scaledMinH;
                if (optsCopy.height.IsFixed()) {
                    rNode->heightMode = SizeMode::Fixed;
                    rNode->fixedHeight = scaledMinH;
                } else if (optsCopy.height.IsStretch()) {
                    rNode->heightMode = SizeMode::Stretch;
                    rNode->fixedHeight = 0.0f;
                } else {
                    rNode->heightMode = SizeMode::Fit;
                    rNode->fixedHeight = 0.0f;
                }
                rNode->flexGrow = Control::ResolveFlexGrow(rNode->parent, outerWidth, outerHeight, 1.0f);
            }
        }

        const Id vpId = Id{detail::HashArgs(rootId, 0x494D'4756'5001ULL)};
        if (!optsCopy.sidebarOnRight) {
            renderSidebar();
            if (sidebarChangedParams) {
                st.EnsureDisplayPixels(data, params);
            }
            viewportRes = RunViewportNode(ctx, vpId, data, params, st, optsCopy, false);
        } else {
            viewportRes = RunViewportNode(ctx, vpId, data, params, st, optsCopy, false);
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

} // namespace

ImageViewResponse detail::ImageView::Run(
    Context& ctx,
    const ImageViewData& data,
    const ImageViewOptions& opts,
    keel::SourceLocation loc
) {
    return RunImageViewImpl(ctx, data, nullptr, opts, loc);
}

ImageViewResponse detail::ImageView::Run(
    Context& ctx,
    const ImageViewData& data,
    ImageViewParams& params,
    const ImageViewOptions& opts,
    keel::SourceLocation loc
) {
    return RunImageViewImpl(ctx, data, &params, opts, loc);
}

} // namespace guikit
