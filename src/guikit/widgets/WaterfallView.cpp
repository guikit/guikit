#include "guikit/widgets/WaterfallView.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <string>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Element.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/graphics/Colormap.hpp"

namespace guikit {

namespace {

[[nodiscard]] ElementSpec MakeWaterfallViewElementSpec(
    const WaterfallViewOptions& opts,
    UiSize size,
    const UiTheme& theme,
    bool disabled
) noexcept {
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .icon = opts.icon,
        .badge = opts.badge,
        .variant = opts.variant,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = opts.width,
        .widgetSize = size,
        .labelGap = theme.Scale(6.0f),
        .descGap = theme.Scale(4.0f),
        .inlineGap = theme.Scale(12.0f),
        .divider = opts.divider,
        .dividerStyle = opts.dividerStyle,
        .stretchableControl = true,
        .stretchHeightWhenStandalone = (opts.heightMode == SizeMode::Stretch),
        .disabled = disabled
    };
}

struct WaterfallViewState {
    uint32_t numBins{0};
    uint32_t historyRows{0};
    uint32_t headRow{0};
    uint32_t validRows{0};

    std::vector<float> ringBufferDb{};
    std::vector<float> liveSliceDb{};
    std::vector<float> peakHoldDb{};
    std::vector<float> averageDb{};

    uint64_t fullGeneration{1};
    uint64_t sliceGeneration{1};
    uint64_t lastSliceSequence{0};
    double lastUpdateTimeSec{0.0};
    bool initialized{false};

    // Interactive viewport & operator state
    float viewMinX{0.0f};
    float viewMaxX{24000.0f};
    float lastDomainMinX{0.0f};
    float lastDomainMaxX{24000.0f};

    bool hasSelectionBand{false};
    float selectionStartX{0.0f};
    float selectionEndX{0.0f};
    bool draggingSelection{false};
    float dragAnchorX{0.0f};

    bool draggingPan{false};
    float panGrabMouseX{0.0f};
    float panGrabMinX{0.0f};
    float panGrabMaxX{0.0f};

    bool draggingThreshold{false};
    float thresholdGateDb{-62.0f};
    float harmonicFundamentalX{1200.0f};
    int selectedTrackId{-1};

    // CPU fallback raster buffer (only allocated when GPU shaders are disabled)
    std::vector<uint32_t> cpuFallbackPixels{};
    uint32_t cpuFallbackW{0};
    uint32_t cpuFallbackH{0};
    uint64_t cpuFallbackKey{0};

    // Two-Layer Retained Base Cache (Pass A: Rulers, Grids, Colorbar, Transparent GPU Cutout)
    uint64_t baseLayerKey{0};
    uint32_t baseLayerW{0};
    uint32_t baseLayerH{0};
    std::vector<uint32_t> baseLayerPixels{};
    bool baseLayerValid{false};
};

struct WaterfallViewPayload {
    WaterfallViewOptions opts{};
    WaterfallViewResponse resp{};
    WaterfallViewState* state{nullptr};
};

struct WaterfallGeometry {
    Rect outerRect{};
    Rect traceRect{};
    Rect wfRect{};
    Rect xRulerRect{};
    Rect colorbarRect{};
    float leftMargin{52.0f};
    float rightMargin{52.0f};
    float topHeaderH{0.0f};
    float bottomRulerH{24.0f};
    float splitGap{6.0f};

    float domainMinX{0.0f};
    float domainMaxX{24000.0f};
    float viewMinX{0.0f};
    float viewMaxX{24000.0f};
    float dbFloor{-110.0f};
    float dbCeil{0.0f};
    float historyDurationSec{30.0f};
    bool invertTimeAxis{false};
    bool hasTrace{true};
    bool hasWaterfall{true};

    [[nodiscard]] float XToScreen(float xVal) const noexcept {
        const float span = std::max(1e-5f, viewMaxX - viewMinX);
        const float u = (xVal - viewMinX) / span;
        const Rect& ref = hasWaterfall ? wfRect : traceRect;
        return ref.x + u * ref.w;
    }

    [[nodiscard]] float ScreenToX(float px) const noexcept {
        const Rect& ref = hasWaterfall ? wfRect : traceRect;
        const float u = std::clamp((px - ref.x) / std::max(1.0f, ref.w), 0.0f, 1.0f);
        return viewMinX + u * (viewMaxX - viewMinX);
    }

    [[nodiscard]] float DbToTraceY(float db) const noexcept {
        const float span = std::max(1.0f, dbCeil - dbFloor);
        const float norm = std::clamp((db - dbFloor) / span, 0.0f, 1.0f);
        return traceRect.Bottom() - norm * traceRect.h;
    }

    [[nodiscard]] float TraceYToDb(float py) const noexcept {
        const float norm = std::clamp((traceRect.Bottom() - py) / std::max(1.0f, traceRect.h), 0.0f, 1.0f);
        return dbFloor + norm * (dbCeil - dbFloor);
    }

    [[nodiscard]] float TimeAgoToWaterfallY(float timeAgoSec) const noexcept {
        const float norm = std::clamp(timeAgoSec / std::max(0.1f, historyDurationSec), 0.0f, 1.0f);
        const float v = invertTimeAxis ? (1.0f - norm) : norm;
        return wfRect.y + v * wfRect.h;
    }

    [[nodiscard]] float WaterfallYToTimeAgo(float py) const noexcept {
        const float v = std::clamp((py - wfRect.y) / std::max(1.0f, wfRect.h), 0.0f, 1.0f);
        const float norm = invertTimeAxis ? (1.0f - v) : v;
        return norm * historyDurationSec;
    }
};

[[nodiscard]] WaterfallGeometry ComputeWaterfallGeometry(const Rect& bounds, const WaterfallViewOptions& opts, float viewMinX, float viewMaxX) noexcept {
    WaterfallGeometry g{};
    g.outerRect = bounds;
    g.leftMargin = 54.0f;
    g.rightMargin = opts.showColorbar ? 52.0f : 12.0f;
    g.bottomRulerH = 24.0f;
    g.topHeaderH = 8.0f;
    g.splitGap = 6.0f;

    g.domainMinX = opts.minX;
    g.domainMaxX = std::max(opts.minX + 1e-3f, opts.maxX);
    g.viewMinX = viewMinX;
    g.viewMaxX = std::max(viewMinX + 1e-3f, viewMaxX);
    g.dbFloor = opts.dbFloor;
    g.dbCeil = std::max(opts.dbFloor + 5.0f, opts.dbCeil);
    g.historyDurationSec = std::max(1.0f, opts.historyDurationSec);
    g.invertTimeAxis = opts.invertTimeAxis;

    const float plotX = bounds.x + g.leftMargin;
    const float plotW = std::max(40.0f, bounds.w - g.leftMargin - g.rightMargin);
    const float availY = bounds.y + g.topHeaderH;
    const float availH = std::max(40.0f, bounds.h - g.topHeaderH - g.bottomRulerH);

    if (opts.viewLayout == WaterfallLayout::SplitTraceAndWaterfall) {
        g.hasTrace = true;
        g.hasWaterfall = true;
        const float ratio = std::clamp(opts.topTraceHeightRatio, 0.18f, 0.55f);
        const float traceH = std::max(54.0f, std::floor((availH - g.splitGap) * ratio));
        const float wfH = std::max(60.0f, availH - traceH - g.splitGap);
        g.traceRect = Rect(plotX, availY, plotW, traceH);
        g.wfRect = Rect(plotX, availY + traceH + g.splitGap, plotW, wfH);
    } else if (opts.viewLayout == WaterfallLayout::WaterfallOnly) {
        g.hasTrace = false;
        g.hasWaterfall = true;
        g.traceRect = Rect(plotX, availY, plotW, 0.0f);
        g.wfRect = Rect(plotX, availY, plotW, availH);
    } else {
        g.hasTrace = true;
        g.hasWaterfall = false;
        g.traceRect = Rect(plotX, availY, plotW, availH);
        g.wfRect = Rect(plotX, availY + availH, plotW, 0.0f);
    }

    const float bottomY = g.hasWaterfall ? g.wfRect.Bottom() : g.traceRect.Bottom();
    g.xRulerRect = Rect(plotX, bottomY, plotW, g.bottomRulerH);
    g.colorbarRect = opts.showColorbar
        ? Rect(plotX + plotW + 4.0f, availY, std::max(0.0f, g.rightMargin - 8.0f), availH)
        : Rect(plotX + plotW, availY, 0.0f, 0.0f);
    return g;
}

void FormatXValue(char* buf, size_t bufSize, float xVal, WaterfallMode mode, float visibleSpan) noexcept {
    if (mode == WaterfallMode::DoaBtr) {
        if (visibleSpan < 25.0f) {
            std::snprintf(buf, bufSize, "%+06.1f°", xVal);
        } else {
            std::snprintf(buf, bufSize, "%+04.0f°", xVal);
        }
    } else {
        const float absX = std::abs(xVal);
        if (absX >= 1000000.0f) {
            std::snprintf(buf, bufSize, "%.3f MHz", xVal / 1000000.0f);
        } else if (absX >= 1000.0f || visibleSpan >= 2000.0f) {
            if (visibleSpan < 1500.0f) {
                std::snprintf(buf, bufSize, "%.2f kHz", xVal / 1000.0f);
            } else {
                std::snprintf(buf, bufSize, "%.1f kHz", xVal / 1000.0f);
            }
        } else {
            std::snprintf(buf, bufSize, "%.0f Hz", xVal);
        }
    }
}

[[nodiscard]] float SampleRingBufferDb(
    const WaterfallViewState& state,
    const WaterfallViewOptions& opts,
    float xVal,
    float timeAgoSec
) noexcept {
    if (state.ringBufferDb.empty() || state.numBins == 0 || state.historyRows == 0 || state.validRows == 0) {
        return opts.dbFloor;
    }
    const float domainSpan = std::max(1e-5f, opts.maxX - opts.minX);
    const float uNorm = std::clamp((xVal - opts.minX) / domainSpan, 0.0f, 1.0f);
    const uint32_t binIdx = std::min(state.numBins - 1, static_cast<uint32_t>( std::lround(uNorm * static_cast<float>(state.numBins - 1)) ));

    const float ageNorm = std::clamp(timeAgoSec / std::max(0.1f, opts.historyDurationSec), 0.0f, 1.0f);
    const uint32_t ageRows = std::min(state.validRows - 1, static_cast<uint32_t>(std::lround(ageNorm * static_cast<float>(state.historyRows - 1))));
    const uint32_t rowIdx = (state.headRow + state.historyRows - (ageRows % state.historyRows)) % state.historyRows;

    return state.ringBufferDb[static_cast<size_t>(rowIdx) * state.numBins + binIdx] + opts.gainDb;
}

[[nodiscard]] float SampleTraceVectorDb(
    const std::vector<float>& vec,
    const WaterfallViewOptions& opts,
    float xVal
) noexcept {
    if (vec.empty()) return opts.dbFloor;
    const float domainSpan = std::max(1e-5f, opts.maxX - opts.minX);
    const float uNorm = std::clamp((xVal - opts.minX) / domainSpan, 0.0f, 1.0f);
    const float fIdx = uNorm * static_cast<float>(vec.size() - 1);
    const size_t i0 = static_cast<size_t>(std::floor(fIdx));
    const size_t i1 = std::min(vec.size() - 1, i0 + 1);
    const float frac = fIdx - static_cast<float>(i0);
    return (vec[i0] + frac * (vec[i1] - vec[i0])) + opts.gainDb;
}

void DrawWaterfallViewNode(const Node* node, ICanvas& canvas, const UiTheme& theme) {
    const auto* payload = node->As<WaterfallViewPayload>();
    if (!payload || !payload->state) return;

    const WaterfallViewOptions& opts = payload->opts;
    const WaterfallViewResponse& resp = payload->resp;
    WaterfallViewState& state = *payload->state;

    const ElementSpec spec = MakeWaterfallViewElementSpec(opts, node->uiSize, theme, node->disabled);
    const Vec2 ctrlSize = {node->computedRect.w, node->computedRect.h};
    const ElementGeometry elemGeom = Element::Resolve(node->computedRect, ctrlSize, spec, &canvas, theme);
    Element::RenderText(canvas, elemGeom, spec, theme);

    const Rect r = elemGeom.controlRect;
    if (r.w <= 20.0f || r.h <= 20.0f) return;

    const WaterfallGeometry geom = ComputeWaterfallGeometry(r, opts, resp.visibleMinX, resp.visibleMaxX);
    const float radius = opts.borderRadius.has_value()
        ? theme.Scale(*opts.borderRadius)
        : theme.GetRadius(node->uiSize);

    const Color bgOuter{7, 10, 19, 255};
    const Color bgPanel{5, 8, 16, 255};
    const Color borderCol{30, 41, 59, 255};
    const Color gridLineCol{56, 189, 248, 34};
    const Color gridMajorCol{56, 189, 248, 72};
    const Color labelTextCol{148, 163, 184, 225};
    const Color accentTextCol{56, 189, 248, 245};

    const auto packFloat = [](float v) noexcept -> uint64_t {
        uint32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(float));
        return static_cast<uint64_t>(bits);
    };

    uint64_t staticKey = 14695981039346656037ULL;
    auto mixKey = [&](uint64_t v) noexcept {
        staticKey ^= v;
        staticKey *= 1099511628211ULL;
    };
    mixKey(packFloat(r.x));
    mixKey(packFloat(r.y));
    mixKey(packFloat(r.w));
    mixKey(packFloat(r.h));
    mixKey(packFloat(canvas.GetScale()));
    mixKey(static_cast<uint64_t>(opts.mode));
    mixKey(static_cast<uint64_t>(opts.viewLayout));
    mixKey(static_cast<uint64_t>(opts.colormap));
    mixKey(packFloat(geom.viewMinX));
    mixKey(packFloat(geom.viewMaxX));
    mixKey(packFloat(geom.dbFloor));
    mixKey(packFloat(geom.dbCeil));
    mixKey(packFloat(geom.historyDurationSec));
    mixKey(packFloat(opts.topTraceHeightRatio));
    mixKey(opts.invertTimeAxis ? 1ULL : 0ULL);
    mixKey(opts.showColorbar ? 1ULL : 0ULL);
    mixKey(opts.showGrid ? 1ULL : 0ULL);
    mixKey(static_cast<uint64_t>(theme.mode));

    const float domainSpan = std::max(1e-5f, geom.domainMaxX - geom.domainMinX);
    const float xMinNorm = std::clamp((geom.viewMinX - geom.domainMinX) / domainSpan, 0.0f, 1.0f);
    const float xMaxNorm = std::clamp((geom.viewMaxX - geom.domainMinX) / domainSpan, 0.0f, 1.0f);

    auto buildGpuDesc = [&]() noexcept -> ICanvas::WaterfallGpuFieldDesc {
        ICanvas::WaterfallGpuFieldDesc desc{};
        desc.widgetId = static_cast<uint64_t>(node->id.value);
        desc.fullGeneration = state.fullGeneration;
        desc.sliceGeneration = state.sliceGeneration;
        desc.ringBufferDb = state.ringBufferDb.data();
        desc.texWidth = state.numBins;
        desc.texHeight = state.historyRows;
        desc.headRow = state.headRow;
        desc.validRows = state.validRows;
        desc.colormapId = static_cast<uint8_t>(opts.colormap);
        CopyColormapLutAbgr(opts.colormap, desc.colormapAbgr);
        desc.plotRect = geom.wfRect;
        desc.clipRect = geom.wfRect;
        desc.xMinNorm = xMinNorm;
        desc.xMaxNorm = xMaxNorm;
        desc.dbFloor = geom.dbFloor;
        desc.dbCeil = geom.dbCeil;
        desc.gainDb = opts.gainDb;
        desc.thresholdGateDb = resp.cursorInside ? state.thresholdGateDb : state.thresholdGateDb;
        desc.gateSuppression = opts.gateSuppression;
        desc.gamma = opts.gamma;
        desc.invertTimeAxis = opts.invertTimeAxis;
        desc.bgColor = bgPanel;
        return desc;
    };

    bool restoredBase = false;
    if (canvas.SupportsGpuShaders() && state.baseLayerValid && state.baseLayerKey == staticKey) {
        if (canvas.RestoreRegion(r, state.baseLayerPixels, state.baseLayerW, state.baseLayerH)) {
            if (geom.hasWaterfall && !state.ringBufferDb.empty()) {
                const auto gpuDesc = buildGpuDesc();
                (void)canvas.DrawWaterfallGpuField(gpuDesc, false);
            }
            restoredBase = true;
        }
    }

    if (!restoredBase) {
        // =================================================================
        // PASS A: Static Instrument Base Layer (Rulers, Grids, Colorbar, Cutout)
        // =================================================================
        canvas.DrawRoundRect(r, radius, bgOuter, borderCol, 1.2f);

        // 1. Upper Trace Panel Static Background & Graticule
        if (geom.hasTrace && geom.traceRect.h > 10.0f) {
            canvas.DrawRect(geom.traceRect, bgPanel);
            canvas.DrawRoundRect(geom.traceRect, 0.0f, Color::Transparent(), borderCol, 1.0f);

            if (opts.showGrid) {
                constexpr int kDbRows = 4;
                for (int i = 0; i <= kDbRows; ++i) {
                    const float frac = static_cast<float>(i) / static_cast<float>(kDbRows);
                    const float y = geom.traceRect.y + frac * geom.traceRect.h;
                    const float dbVal = geom.dbCeil - frac * (geom.dbCeil - geom.dbFloor);
                    if (i > 0 && i < kDbRows) {
                        canvas.DrawLine({geom.traceRect.x, y}, {geom.traceRect.Right(), y}, gridLineCol, 1.0f);
                    }
                    char dbBuf[24];
                    std::snprintf(dbBuf, sizeof(dbBuf), "%+.0fdB", dbVal);
                    canvas.DrawText(dbBuf, {r.x + 6.0f, y - 5.5f}, 9.5f, labelTextCol);
                }

                constexpr int kXCols = 8;
                for (int c = 1; c < kXCols; ++c) {
                    const float u = static_cast<float>(c) / static_cast<float>(kXCols);
                    const float x = geom.traceRect.x + u * geom.traceRect.w;
                    const bool isCenter = (c == kXCols / 2);
                    canvas.DrawLine({x, geom.traceRect.y}, {x, geom.traceRect.Bottom()},
                                    isCenter ? gridMajorCol : gridLineCol, isCenter ? 1.2f : 1.0f);
                }
            }
        }

        // 2. Lower Waterfall Panel GPU Underlay Cutout & Graticule
        if (geom.hasWaterfall && geom.wfRect.h > 10.0f) {
            bool gpuHandled = false;
            if (canvas.SupportsGpuShaders() && !state.ringBufferDb.empty()) {
                const auto gpuDesc = buildGpuDesc();
                gpuHandled = canvas.DrawWaterfallGpuField(gpuDesc, true);
            }

            if (!gpuHandled) {
                canvas.DrawRect(geom.wfRect, bgPanel);
                // CPU Software Fallback when GPU switch is disabled
                if (!state.ringBufferDb.empty() && state.numBins > 0 && state.historyRows > 0) {
                    const auto& lutAbgr = GetColormapLutAbgr(opts.colormap);
                    const uint32_t fbW = std::clamp(static_cast<uint32_t>(geom.wfRect.w * 0.5f), 64u, 320u);
                    const uint32_t fbH = std::clamp(static_cast<uint32_t>(geom.wfRect.h * 0.5f), 48u, 200u);
                    state.cpuFallbackPixels.resize(static_cast<size_t>(fbW) * fbH);
                    const float dbSpan = std::max(1.0f, geom.dbCeil - geom.dbFloor);
                    const float gamma = std::max(0.25f, opts.gamma);
                    for (uint32_t py = 0; py < fbH; ++py) {
                        const float v = static_cast<float>(py) / static_cast<float>(std::max(1u, fbH - 1));
                        const float timeUv = opts.invertTimeAxis ? (1.0f - v) : v;
                        const uint32_t ageRow = static_cast<uint32_t>(std::lround(timeUv * static_cast<float>(state.historyRows - 1)));
                        if (ageRow >= state.validRows) {
                            std::fill_n(state.cpuFallbackPixels.data() + static_cast<size_t>(py) * fbW, fbW, PackAbgr(bgPanel));
                            continue;
                        }
                        const uint32_t ringRow = (state.headRow + state.historyRows - (ageRow % state.historyRows)) % state.historyRows;
                        const float* rowPtr = state.ringBufferDb.data() + static_cast<size_t>(ringRow) * state.numBins;
                        uint32_t* dstRow = state.cpuFallbackPixels.data() + static_cast<size_t>(py) * fbW;
                        for (uint32_t px = 0; px < fbW; ++px) {
                            const float u = xMinNorm + (static_cast<float>(px) / static_cast<float>(std::max(1u, fbW - 1))) * (xMaxNorm - xMinNorm);
                            const uint32_t bin = std::min(state.numBins - 1, static_cast<uint32_t>(std::lround(u * static_cast<float>(state.numBins - 1))));
                            const float dbVal = rowPtr[bin] + opts.gainDb;
                            float norm = std::clamp((dbVal - geom.dbFloor) / dbSpan, 0.0f, 1.0f);
                            if (opts.gateSuppression > 0.001f && dbVal < state.thresholdGateDb) {
                                const float soft = std::clamp((dbVal - (state.thresholdGateDb - 10.0f)) / 10.0f, 0.0f, 1.0f);
                                norm *= (1.0f - opts.gateSuppression) + opts.gateSuppression * (soft * 0.25f);
                            }
                            norm = std::pow(norm, gamma);
                            const size_t lutIdx = static_cast<size_t>(std::clamp(norm * 255.0f, 0.0f, 255.0f));
                            dstRow[px] = lutAbgr[lutIdx];
                        }
                    }
                    canvas.DrawPixels(state.cpuFallbackPixels.data(), fbW, fbH, geom.wfRect);
                }
            }

            canvas.DrawRoundRect(geom.wfRect, 0.0f, Color::Transparent(), borderCol, 1.0f);

            // Time-axis Y labels & waterfall graticule
            constexpr int kTimeTicks = 6;
            for (int t = 0; t <= kTimeTicks; ++t) {
                const float frac = static_cast<float>(t) / static_cast<float>(kTimeTicks);
                const float timeAgo = frac * geom.historyDurationSec;
                const float y = geom.TimeAgoToWaterfallY(timeAgo);
                if (opts.showGrid && t > 0 && t < kTimeTicks) {
                    canvas.DrawLine({geom.wfRect.x, y}, {geom.wfRect.Right(), y}, gridLineCol, 1.0f);
                }
                char tBuf[24];
                if (t == 0) {
                    std::snprintf(tBuf, sizeof(tBuf), "0.0s");
                } else {
                    std::snprintf(tBuf, sizeof(tBuf), "-%.0fs", timeAgo);
                }
                canvas.DrawText(tBuf, {r.x + 8.0f, y - 5.5f}, 9.5f, (t == 0) ? accentTextCol : labelTextCol);
            }

            if (opts.showGrid) {
                constexpr int kXCols = 8;
                for (int c = 1; c < kXCols; ++c) {
                    const float u = static_cast<float>(c) / static_cast<float>(kXCols);
                    const float x = geom.wfRect.x + u * geom.wfRect.w;
                    const bool isCenter = (c == kXCols / 2);
                    canvas.DrawLine({x, geom.wfRect.y}, {x, geom.wfRect.Bottom()},
                                    isCenter ? gridMajorCol : gridLineCol, isCenter ? 1.2f : 1.0f);
                }
            }
        }

        // 3. Bottom X-Axis Ruler (Frequency Hz/kHz/MHz or Azimuth Bearing deg)
        {
            const float visSpan = geom.viewMaxX - geom.viewMinX;
            constexpr int kXCols = 8;
            for (int c = 0; c <= kXCols; ++c) {
                const float u = static_cast<float>(c) / static_cast<float>(kXCols);
                const float x = geom.xRulerRect.x + u * geom.xRulerRect.w;
                const float xVal = geom.viewMinX + u * visSpan;
                canvas.DrawLine({x, geom.xRulerRect.y}, {x, geom.xRulerRect.y + 4.5f}, labelTextCol, 1.1f);

                char xBuf[32];
                FormatXValue(xBuf, sizeof(xBuf), xVal, opts.mode, visSpan);
                const float textOff = (c == 0) ? 0.0f : ((c == kXCols) ? -42.0f : -20.0f);
                canvas.DrawText(xBuf, {x + textOff, geom.xRulerRect.y + 6.0f}, 10.0f,
                                (c == kXCols / 2) ? accentTextCol : labelTextCol);
            }
        }

        // 4. Right-Side dB Intensity Colorbar
        if (opts.showColorbar && geom.colorbarRect.w >= 18.0f) {
            canvas.PushClip(geom.colorbarRect);
            canvas.DrawRect(geom.colorbarRect, Color(9, 13, 24, 255));
            canvas.DrawLine({geom.colorbarRect.x, geom.colorbarRect.y},
                            {geom.colorbarRect.x, geom.colorbarRect.Bottom()}, borderCol, 1.0f);

            const Rect barStrip(geom.colorbarRect.x + 7.0f, geom.colorbarRect.y + 20.0f,
                                11.0f, std::max(24.0f, geom.colorbarRect.h - 32.0f));
            DrawColormapBar(canvas, barStrip, opts.colormap, true, false, 2.0f, Color(148, 163, 184, 120));

            canvas.DrawText("dB", {geom.colorbarRect.x + 7.0f, geom.colorbarRect.y + 4.0f}, 9.5f, accentTextCol);
            for (int i = 0; i <= 4; ++i) {
                const float frac = static_cast<float>(i) / 4.0f;
                const float y = barStrip.y + frac * barStrip.h;
                const float dbVal = geom.dbCeil - frac * (geom.dbCeil - geom.dbFloor);
                char dbBuf[16];
                std::snprintf(dbBuf, sizeof(dbBuf), "%.0f", dbVal);
                canvas.DrawText(dbBuf, {barStrip.Right() + 3.0f, y - 5.0f}, 9.0f, labelTextCol);
            }
            canvas.PopClip();
        }

        if (canvas.SupportsGpuShaders()) {
            if (canvas.CaptureRegion(r, state.baseLayerPixels, state.baseLayerW, state.baseLayerH)) {
                state.baseLayerKey = staticKey;
                state.baseLayerValid = true;
            }
        }
    }

    // =====================================================================
    // PASS B: Dynamic Tactical & Spectral Overlay (Traces, Tracks, Cursors, HUD)
    // =====================================================================

    // Dynamic Threshold Marker on Colorbar
    if (opts.showColorbar && geom.colorbarRect.w >= 18.0f) {
        const Rect barStrip(geom.colorbarRect.x + 7.0f, geom.colorbarRect.y + 20.0f,
                            11.0f, std::max(24.0f, geom.colorbarRect.h - 32.0f));
        const float gateNorm = std::clamp((state.thresholdGateDb - geom.dbFloor) / std::max(1.0f, geom.dbCeil - geom.dbFloor), 0.0f, 1.0f);
        const float gateY = barStrip.Bottom() - gateNorm * barStrip.h;
        canvas.DrawLine({barStrip.x - 3.0f, gateY}, {barStrip.Right() + 3.0f, gateY}, Color(244, 63, 94, 240), 2.0f);
    }

    // 1. Upper Instantaneous Trace Panel (Average, Peak Hold, Live Slice, Threshold Gate, Auto Peaks)
    if (geom.hasTrace && geom.traceRect.h > 10.0f && !state.liveSliceDb.empty()) {
        canvas.PushClip(geom.traceRect);

        // Selected Band / Sector Highlight in Trace Panel
        if (resp.hasSelectionBand) {
            const float sx0 = std::clamp(geom.XToScreen(resp.selectionStartX), geom.traceRect.x, geom.traceRect.Right());
            const float sx1 = std::clamp(geom.XToScreen(resp.selectionEndX), geom.traceRect.x, geom.traceRect.Right());
            const float left = std::min(sx0, sx1);
            const float width = std::abs(sx1 - sx0);
            if (width > 1.0f) {
                canvas.DrawRect(Rect(left, geom.traceRect.y, width, geom.traceRect.h), Color(56, 189, 248, 36));
                canvas.DrawLine({left, geom.traceRect.y}, {left, geom.traceRect.Bottom()}, Color(56, 189, 248, 210), 1.3f);
                canvas.DrawLine({left + width, geom.traceRect.y}, {left + width, geom.traceRect.Bottom()}, Color(56, 189, 248, 210), 1.3f);
            }
        }

        // Harmonic Comb Lines in Trace Panel
        if (opts.showHarmonicComb && state.harmonicFundamentalX > 0.0f) {
            for (int h = 1; h <= 6; ++h) {
                const float hx = state.harmonicFundamentalX * static_cast<float>(h);
                if (hx >= geom.viewMinX && hx <= geom.viewMaxX) {
                    const float px = geom.XToScreen(hx);
                    const Color hc = (h == 1) ? Color(250, 204, 21, 210) : Color(250, 204, 21, 120);
                    canvas.DrawLine({px, geom.traceRect.y}, {px, geom.traceRect.Bottom()}, hc, (h == 1) ? 1.5f : 1.0f);
                    char hBuf[12];
                    std::snprintf(hBuf, sizeof(hBuf), "H%d", h);
                    canvas.DrawText(hBuf, {px + 3.0f, geom.traceRect.y + 4.0f}, 9.0f, hc);
                }
            }
        }

        const int numSteps = std::clamp(static_cast<int>(geom.traceRect.w / 2.5f), 64, 360);
        auto drawTraceCurve = [&](const std::vector<float>& vec, Color strokeCol, float strokeW, bool fillBars) {
            if (vec.empty()) return;
            Vec2 prevPt{};
            for (int s = 0; s <= numSteps; ++s) {
                const float u = static_cast<float>(s) / static_cast<float>(numSteps);
                const float xVal = geom.viewMinX + u * (geom.viewMaxX - geom.viewMinX);
                const float dbVal = SampleTraceVectorDb(vec, opts, xVal);
                const Vec2 pt{geom.traceRect.x + u * geom.traceRect.w, geom.DbToTraceY(dbVal)};
                if (fillBars && (s % 2 == 0)) {
                    const Color barCol(strokeCol.r, strokeCol.g, strokeCol.b, 28);
                    canvas.DrawLine({pt.x, geom.traceRect.Bottom()}, pt, barCol, 1.5f);
                }
                if (s > 0) {
                    canvas.DrawLine(prevPt, pt, strokeCol, strokeW);
                }
                prevPt = pt;
            }
        };

        if (opts.showAverageTrace) {
            drawTraceCurve(state.averageDb, Color(251, 191, 36, 185), 1.3f, false);
        }
        if (opts.showPeakHold) {
            drawTraceCurve(state.peakHoldDb, Color(192, 132, 252, 200), 1.2f, false);
        }
        if (opts.showLiveTrace) {
            const Color liveCol = (opts.mode == WaterfallMode::DoaBtr)
                ? Color(52, 211, 153, 245)
                : Color(56, 189, 248, 245);
            drawTraceCurve(state.liveSliceDb, liveCol, 1.7f, true);
        }

        // Interactive Detection Threshold Gate Line
        if (opts.showThresholdGate) {
            const float gateY = geom.DbToTraceY(state.thresholdGateDb);
            const Color gateCol = state.draggingThreshold ? Color(250, 204, 21, 245) : Color(244, 63, 94, 205);
            canvas.DrawLine({geom.traceRect.x, gateY}, {geom.traceRect.Right(), gateY}, gateCol, 1.4f);

            char gateTag[32];
            std::snprintf(gateTag, sizeof(gateTag), "GATE %+.0f dB", state.thresholdGateDb);
            const Rect pill(geom.traceRect.Right() - 78.0f, std::clamp(gateY - 9.0f, geom.traceRect.y + 2.0f, geom.traceRect.Bottom() - 18.0f), 74.0f, 16.0f);
            canvas.DrawRoundRect(pill, 4.0f, Color(15, 23, 42, 225), gateCol, 1.0f);
            canvas.DrawText(gateTag, {pill.x + 6.0f, pill.y + 2.5f}, 9.0f, gateCol);
        }

        // Automatic Peak / DOA Lobe Detection & Callout Reticles
        if (opts.showPeakMarkers && state.liveSliceDb.size() >= 8) {
            struct PeakCandidate {
                float xVal;
                float dbVal;
            };
            std::vector<PeakCandidate> peaks;
            const size_t n = state.liveSliceDb.size();
            const size_t minSepBins = std::max<size_t>(4, n / 28);
            for (size_t i = 2; i + 2 < n; ++i) {
                const float db = state.liveSliceDb[i] + opts.gainDb;
                if (db < state.thresholdGateDb) continue;
                if (db >= state.liveSliceDb[i - 1] + opts.gainDb &&
                    db >= state.liveSliceDb[i + 1] + opts.gainDb &&
                    db > state.liveSliceDb[i - 2] + opts.gainDb &&
                    db > state.liveSliceDb[i + 2] + opts.gainDb) {
                    const float xVal = geom.domainMinX + (static_cast<float>(i) / static_cast<float>(n - 1)) * (geom.domainMaxX - geom.domainMinX);
                    if (xVal < geom.viewMinX || xVal > geom.viewMaxX) continue;
                    bool merged = false;
                    for (auto& existing : peaks) {
                        const float existBin = ((existing.xVal - geom.domainMinX) / domainSpan) * static_cast<float>(n - 1);
                        if (std::abs(static_cast<float>(i) - existBin) < static_cast<float>(minSepBins)) {
                            if (db > existing.dbVal) {
                                existing.xVal = xVal;
                                existing.dbVal = db;
                            }
                            merged = true;
                            break;
                        }
                    }
                    if (!merged) {
                        peaks.push_back({xVal, db});
                    }
                }
            }
            std::sort(peaks.begin(), peaks.end(), [](const PeakCandidate& a, const PeakCandidate& b) {
                return a.dbVal > b.dbVal;
            });
            const size_t maxPeaks = std::min<size_t>(4, peaks.size());
            for (size_t k = 0; k < maxPeaks; ++k) {
                const Vec2 pkPt{geom.XToScreen(peaks[k].xVal), geom.DbToTraceY(peaks[k].dbVal)};
                const Color pkCol = (k == 0) ? Color(250, 204, 21, 255) : Color(56, 189, 248, 235);
                canvas.DrawTriangle({pkPt.x, pkPt.y - 5.0f}, {pkPt.x - 4.0f, pkPt.y}, {pkPt.x + 4.0f, pkPt.y}, pkCol);
                canvas.DrawTriangle({pkPt.x, pkPt.y + 5.0f}, {pkPt.x - 4.0f, pkPt.y}, {pkPt.x + 4.0f, pkPt.y}, pkCol);

                char xStr[24];
                FormatXValue(xStr, sizeof(xStr), peaks[k].xVal, opts.mode, geom.viewMaxX - geom.viewMinX);
                char pkLabel[48];
                std::snprintf(pkLabel, sizeof(pkLabel), "%s (%.0fdB)", xStr, peaks[k].dbVal);
                const float lx = std::clamp(pkPt.x - 24.0f, geom.traceRect.x + 4.0f, geom.traceRect.Right() - 96.0f);
                const float ly = std::max(geom.traceRect.y + 3.0f, pkPt.y - 17.0f);
                canvas.DrawText(pkLabel, {lx, ly}, 9.2f, pkCol);
            }
        }

        // Synchronized Cursor Hairline in Trace Panel
        if (resp.cursorInside) {
            const float cx = geom.XToScreen(resp.cursorX);
            canvas.DrawLine({cx, geom.traceRect.y}, {cx, geom.traceRect.Bottom()}, Color(255, 255, 255, 165), 1.0f);
            const float cy = geom.DbToTraceY(resp.cursorLiveDb);
            canvas.DrawRoundRect(Rect(cx - 3.5f, cy - 3.5f, 7.0f, 7.0f), 3.5f, Color(56, 189, 248, 255), Color(255, 255, 255, 240), 1.2f);
        }

        canvas.PopClip();
    }

    // 2. Lower Waterfall Panel Overlays (Selection Band, Harmonic Comb, DOA Target Tracks, Crosshair, HUD)
    if (geom.hasWaterfall && geom.wfRect.h > 10.0f) {
        canvas.PushClip(geom.wfRect);

        // Selected Band / Sector Highlight in Waterfall
        if (resp.hasSelectionBand) {
            const float sx0 = std::clamp(geom.XToScreen(resp.selectionStartX), geom.wfRect.x, geom.wfRect.Right());
            const float sx1 = std::clamp(geom.XToScreen(resp.selectionEndX), geom.wfRect.x, geom.wfRect.Right());
            const float left = std::min(sx0, sx1);
            const float width = std::abs(sx1 - sx0);
            if (width > 1.0f) {
                canvas.DrawRect(Rect(left, geom.wfRect.y, width, geom.wfRect.h), Color(56, 189, 248, 28));
                canvas.DrawLine({left, geom.wfRect.y}, {left, geom.wfRect.Bottom()}, Color(56, 189, 248, 210), 1.3f);
                canvas.DrawLine({left + width, geom.wfRect.y}, {left + width, geom.wfRect.Bottom()}, Color(56, 189, 248, 210), 1.3f);
            }
        }

        // Harmonic Comb Lines in Waterfall
        if (opts.showHarmonicComb && state.harmonicFundamentalX > 0.0f) {
            for (int h = 1; h <= 6; ++h) {
                const float hx = state.harmonicFundamentalX * static_cast<float>(h);
                if (hx >= geom.viewMinX && hx <= geom.viewMaxX) {
                    const float px = geom.XToScreen(hx);
                    const Color hc = (h == 1) ? Color(250, 204, 21, 175) : Color(250, 204, 21, 95);
                    canvas.DrawLine({px, geom.wfRect.y}, {px, geom.wfRect.Bottom()}, hc, 1.0f);
                }
            }
        }

        // DOA / Emitter Target Track Trajectories & Gate Corridors
        if (opts.showTracks && !opts.tracks.empty()) {
            for (const auto& trk : opts.tracks) {
                const bool isSel = (trk.id == resp.selectedTrackId);
                const bool isHov = (trk.id == resp.hoveredTrackId);

                Color trkCol{52, 211, 153, 235};
                if (trk.status == WaterfallTrackStatus::Alarm) {
                    trkCol = Color(244, 63, 94, 245);
                } else if (trk.status == WaterfallTrackStatus::Coasting) {
                    trkCol = Color(251, 191, 36, 220);
                }
                if (isSel) {
                    trkCol = Color(250, 204, 21, 255);
                }

                // Draw historical trajectory polyline & gate corridor
                if (trk.history.size() >= 2) {
                    const float gateHalfPx = (trk.gateWidth * 0.5f / std::max(1e-5f, geom.viewMaxX - geom.viewMinX)) * geom.wfRect.w;
                    for (size_t i = 0; i + 1 < trk.history.size(); ++i) {
                        const auto& p0 = trk.history[i];
                        const auto& p1 = trk.history[i + 1];
                        if (p0.timeAgoSec > geom.historyDurationSec && p1.timeAgoSec > geom.historyDurationSec) continue;

                        const Vec2 s0{geom.XToScreen(p0.xValue), geom.TimeAgoToWaterfallY(p0.timeAgoSec)};
                        const Vec2 s1{geom.XToScreen(p1.xValue), geom.TimeAgoToWaterfallY(p1.timeAgoSec)};
                        canvas.DrawLine(s0, s1, trkCol, isSel ? 2.4f : 1.8f);

                        if (opts.showTrackGates && gateHalfPx >= 3.0f && (i % 2 == 0)) {
                            const Color gateCol(trkCol.r, trkCol.g, trkCol.b, isSel ? 115 : 68);
                            canvas.DrawLine({s0.x - gateHalfPx, s0.y}, {s1.x - gateHalfPx, s1.y}, gateCol, 1.0f);
                            canvas.DrawLine({s0.x + gateHalfPx, s0.y}, {s1.x + gateHalfPx, s1.y}, gateCol, 1.0f);
                        }
                    }
                }

                // Draw current track head reticle at t = 0s
                if (trk.currentX >= geom.viewMinX && trk.currentX <= geom.viewMaxX) {
                    const Vec2 headPt{geom.XToScreen(trk.currentX), geom.TimeAgoToWaterfallY(0.0f) + (opts.invertTimeAxis ? -8.0f : 8.0f)};
                    const float sz = (isSel || isHov) ? 6.5f : 5.2f;
                    canvas.DrawTriangle({headPt.x, headPt.y - sz}, {headPt.x - sz, headPt.y}, {headPt.x + sz, headPt.y}, trkCol);
                    canvas.DrawTriangle({headPt.x, headPt.y + sz}, {headPt.x - sz, headPt.y}, {headPt.x + sz, headPt.y}, trkCol);

                    // Drift rate prediction vector (5 seconds projection)
                    const float projX = trk.currentX - trk.driftRatePerSec * 4.0f;
                    const Vec2 vecEnd{geom.XToScreen(projX), headPt.y + (opts.invertTimeAxis ? -22.0f : 22.0f)};
                    canvas.DrawLine(headPt, vecEnd, Color(255, 255, 255, 210), 1.4f);

                    // Track Callsign & Bearing/Drift Telemetry Badge
                    char xStr[24];
                    FormatXValue(xStr, sizeof(xStr), trk.currentX, opts.mode, geom.viewMaxX - geom.viewMinX);
                    char tagBuf[64];
                    if (opts.mode == WaterfallMode::DoaBtr) {
                        std::snprintf(tagBuf, sizeof(tagBuf), "%s %s (%+.1f°/s)",
                                      trk.callsign.c_str(), xStr, trk.driftRatePerSec);
                    } else {
                        std::snprintf(tagBuf, sizeof(tagBuf), "%s %s (%.0fdB)",
                                      trk.callsign.c_str(), xStr, trk.levelDb);
                    }
                    const float tagW = 148.0f;
                    const float tagX = std::clamp(headPt.x + 9.0f, geom.wfRect.x + 4.0f, geom.wfRect.Right() - tagW - 4.0f);
                    const float tagY = std::clamp(headPt.y - 7.0f, geom.wfRect.y + 4.0f, geom.wfRect.Bottom() - 20.0f);
                    canvas.DrawRoundRect(Rect(tagX, tagY, tagW, 17.0f), 4.0f, Color(9, 14, 26, 220), trkCol, isSel ? 1.4f : 1.0f);
                    canvas.DrawText(tagBuf, {tagX + 5.0f, tagY + 2.5f}, 9.2f, trkCol);
                }
            }
        }

        // Synchronized Crosshair & Live Readout Tooltip in Waterfall
        if (resp.cursorInside) {
            const float cx = geom.XToScreen(resp.cursorX);
            canvas.DrawLine({cx, geom.wfRect.y}, {cx, geom.wfRect.Bottom()}, Color(255, 255, 255, 165), 1.0f);

            if (resp.cursorInWaterfall) {
                const float cy = geom.TimeAgoToWaterfallY(resp.cursorTimeAgoSec);
                canvas.DrawLine({geom.wfRect.x, cy}, {geom.wfRect.Right(), cy}, Color(255, 255, 255, 130), 1.0f);
                canvas.DrawRoundRect(Rect(cx - 4.0f, cy - 4.0f, 8.0f, 8.0f), 4.0f, Color::Transparent(), Color(250, 204, 21, 245), 1.4f);

                char xStr[24];
                FormatXValue(xStr, sizeof(xStr), resp.cursorX, opts.mode, geom.viewMaxX - geom.viewMinX);
                char tipBuf[80];
                std::snprintf(tipBuf, sizeof(tipBuf), "%s  |  -%.1fs  |  WF: %+.1f dB  (Live: %+.1f dB)",
                              xStr, resp.cursorTimeAgoSec, resp.cursorWaterfallDb, resp.cursorLiveDb);
                const float boxW = 248.0f;
                const float boxH = 20.0f;
                const float bx = (cx + 12.0f + boxW <= geom.wfRect.Right()) ? (cx + 12.0f) : (cx - boxW - 12.0f);
                const float by = std::clamp(cy - 24.0f, geom.wfRect.y + 6.0f, geom.wfRect.Bottom() - boxH - 6.0f);
                canvas.DrawRoundRect(Rect(bx, by, boxW, boxH), 4.0f, Color(9, 13, 24, 232), Color(56, 189, 248, 180), 1.0f);
                canvas.DrawText(tipBuf, {bx + 7.0f, by + 4.0f}, 9.5f, Color(241, 245, 249, 250));
            }
        }

        // Corner Operator Telemetry HUD Cards
        if (opts.showHudOverlay) {
            const Rect tlCard(geom.wfRect.x + 10.0f, geom.wfRect.y + 10.0f, 226.0f, 56.0f);
            canvas.DrawRoundRect(tlCard, 6.0f, Color(9, 14, 26, 215), Color(56, 189, 248, 85), 1.0f);

            const char* modeTitle = (opts.mode == WaterfallMode::DoaBtr)
                ? "DOA / BTR AZIMUTH WATERFALL"
                : "RTSA SPECTRAL WATERFALL";
            canvas.DrawText(modeTitle, {tlCard.x + 9.0f, tlCard.y + 5.0f}, 10.0f, accentTextCol);

            char info1[64];
            std::snprintf(info1, sizeof(info1), "RING: %ux%u  HIST: %.0fs  ZOOM: %.1fx",
                          opts.numBins, opts.historyRows, geom.historyDurationSec, resp.zoomFactor);
            canvas.DrawText(info1, {tlCard.x + 9.0f, tlCard.y + 22.0f}, 9.5f, Color(226, 232, 240, 230));

            char peakXStr[24];
            FormatXValue(peakXStr, sizeof(peakXStr), resp.dominantPeakX, opts.mode, geom.viewMaxX - geom.viewMinX);
            char info2[64];
            std::snprintf(info2, sizeof(info2), "PEAK: %s (%+.1f dB)", peakXStr, resp.dominantPeakDb);
            canvas.DrawText(info2, {tlCard.x + 9.0f, tlCard.y + 38.0f}, 9.5f, Color(250, 204, 21, 235));

            if (resp.hasSelectionBand) {
                const Rect trCard(geom.wfRect.Right() - 228.0f, geom.wfRect.y + 10.0f, 218.0f, 42.0f);
                canvas.DrawRoundRect(trCard, 6.0f, Color(9, 14, 26, 222), Color(56, 189, 248, 150), 1.0f);
                char cStr[24], sStr[24];
                FormatXValue(cStr, sizeof(cStr), resp.selectionCenterX, opts.mode, geom.viewMaxX - geom.viewMinX);
                FormatXValue(sStr, sizeof(sStr), resp.selectionSpanX, opts.mode, geom.viewMaxX - geom.viewMinX);
                char bLine1[64], bLine2[64];
                std::snprintf(bLine1, sizeof(bLine1), "%s: %s  SPAN: %s",
                              (opts.mode == WaterfallMode::DoaBtr) ? "SECTOR CTR" : "BAND Fc", cStr, sStr);
                std::snprintf(bLine2, sizeof(bLine2), "INTEGRATED POWER: %+.1f dB", resp.selectionBandPowerDb);
                canvas.DrawText(bLine1, {trCard.x + 8.0f, trCard.y + 6.0f}, 9.5f, accentTextCol);
                canvas.DrawText(bLine2, {trCard.x + 8.0f, trCard.y + 22.0f}, 9.5f, Color(52, 211, 153, 245));
            }
        }

        canvas.PopClip();
    }
}

} // namespace

Vec2 WaterfallView::Measure(
    std::span<const float> /*latestSliceDb*/,
    const WaterfallViewOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) {
    const float defaultW = opts.width.has_value()
        ? theme.Scale(*opts.width)
        : (availableWidth > 0.0f ? availableWidth : theme.Scale(520.0f));
    const float defaultH = opts.height.has_value()
        ? theme.Scale(*opts.height)
        : theme.Scale(420.0f);
    const ElementSpec spec = MakeWaterfallViewElementSpec(opts, size, theme, opts.disabled);
    return Element::Measure({defaultW, defaultH}, spec, canvas, theme, availableWidth);
}

WaterfallViewResponse WaterfallView::Run(
    Context& ctx,
    std::span<const float> latestSliceDb,
    const WaterfallViewOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiTheme& theme = ctx.GetTheme();
    const UiSize size = opts.size.value_or(ctx.GetCurrentSize());
    const bool effDisabled = opts.disabled || ctx.IsDisabled();

    Node* node = ctx.CreateNode(NodeType::WaterfallView, id);
    auto* payload = ctx.AllocPayload<WaterfallViewPayload>();
    payload->opts = opts;
    payload->opts.label = ctx.AllocText(opts.label);
    payload->opts.description = ctx.AllocText(opts.description);
    payload->opts.badge = ctx.AllocText(opts.badge);
    if (!opts.icon.GetPath().empty()) {
        payload->opts.icon = Icon::File(ctx.AllocString(opts.icon.GetPath()));
    }

    auto& state = ctx.GetOrCreateState<WaterfallViewState>(id);
    payload->state = &state;

    const uint32_t numBins = std::max(16u, opts.numBins);
    const uint32_t historyRows = std::max(16u, opts.historyRows);
    const double nowSec = ctx.GetCurrentTime();
    const float dtSec = (state.lastUpdateTimeSec > 0.0)
        ? static_cast<float>(std::clamp(nowSec - state.lastUpdateTimeSec, 0.0, 0.25))
        : 0.016f;
    state.lastUpdateTimeSec = nowSec;

    // 1. Initialize or resize Circular Ring Buffer
    const bool dimsChanged = (!state.initialized || state.numBins != numBins || state.historyRows != historyRows);
    const bool domainChanged = (std::abs(state.lastDomainMinX - opts.minX) > 1e-3f ||
                                std::abs(state.lastDomainMaxX - opts.maxX) > 1e-3f);
    if (dimsChanged || opts.requestClearHistory) {
        state.numBins = numBins;
        state.historyRows = historyRows;
        state.ringBufferDb.assign(static_cast<size_t>(historyRows) * numBins, opts.dbFloor);
        state.liveSliceDb.assign(numBins, opts.dbFloor);
        state.peakHoldDb.assign(numBins, opts.dbFloor);
        state.averageDb.assign(numBins, opts.dbFloor);
        state.headRow = historyRows - 1;
        state.validRows = 0;

        if (opts.initialHistoryMatrix.size() >= static_cast<size_t>(historyRows) * numBins) {
            // Bulk preload: initialHistoryMatrix row 0 = newest (t = 0s), row (historyRows - 1) = oldest
            for (uint32_t age = 0; age < historyRows; ++age) {
                const uint32_t ringRow = (state.headRow + historyRows - age) % historyRows;
                const float* srcRow = opts.initialHistoryMatrix.data() + static_cast<size_t>(age) * numBins;
                float* dstRow = state.ringBufferDb.data() + static_cast<size_t>(ringRow) * numBins;
                std::memcpy(dstRow, srcRow, static_cast<size_t>(numBins) * sizeof(float));
            }
            state.validRows = historyRows;
            std::memcpy(state.liveSliceDb.data(), opts.initialHistoryMatrix.data(), static_cast<size_t>(numBins) * sizeof(float));
            std::memcpy(state.peakHoldDb.data(), state.liveSliceDb.data(), static_cast<size_t>(numBins) * sizeof(float));
            std::memcpy(state.averageDb.data(), state.liveSliceDb.data(), static_cast<size_t>(numBins) * sizeof(float));

            const uint32_t scanRows = std::min(historyRows, 48u);
            for (uint32_t age = 1; age < scanRows; ++age) {
                const float* srcRow = opts.initialHistoryMatrix.data() + static_cast<size_t>(age) * numBins;
                for (uint32_t b = 0; b < numBins; ++b) {
                    state.peakHoldDb[b] = std::max(state.peakHoldDb[b], srcRow[b]);
                    state.averageDb[b] = state.averageDb[b] * 0.9f + srcRow[b] * 0.1f;
                }
            }
        } else if (latestSliceDb.size() >= numBins) {
            std::memcpy(state.ringBufferDb.data() + static_cast<size_t>(state.headRow) * numBins,
                        latestSliceDb.data(), static_cast<size_t>(numBins) * sizeof(float));
            std::memcpy(state.liveSliceDb.data(), latestSliceDb.data(), static_cast<size_t>(numBins) * sizeof(float));
            std::memcpy(state.peakHoldDb.data(), latestSliceDb.data(), static_cast<size_t>(numBins) * sizeof(float));
            std::memcpy(state.averageDb.data(), latestSliceDb.data(), static_cast<size_t>(numBins) * sizeof(float));
            state.validRows = 1;
        }

        state.lastSliceSequence = opts.sliceSequence;
        state.fullGeneration++;
        state.sliceGeneration++;
        state.initialized = true;
    } else if (opts.sliceSequence != state.lastSliceSequence && latestSliceDb.size() >= numBins) {
        // Streaming O(1) Ring-Buffer Advance: write only the single new row at headRow!
        state.headRow = (state.headRow + 1) % historyRows;
        state.validRows = std::min(historyRows, state.validRows + 1);
        float* dstRow = state.ringBufferDb.data() + static_cast<size_t>(state.headRow) * numBins;
        std::memcpy(dstRow, latestSliceDb.data(), static_cast<size_t>(numBins) * sizeof(float));
        std::memcpy(state.liveSliceDb.data(), latestSliceDb.data(), static_cast<size_t>(numBins) * sizeof(float));

        const float decayStep = std::max(0.0f, opts.peakDecayDbPerSec) * dtSec;
        const float alpha = std::clamp(opts.averageAlpha, 0.01f, 1.0f);
        for (uint32_t b = 0; b < numBins; ++b) {
            const float v = latestSliceDb[b];
            state.peakHoldDb[b] = std::max(state.peakHoldDb[b] - decayStep, v);
            state.averageDb[b] += alpha * (v - state.averageDb[b]);
        }

        state.lastSliceSequence = opts.sliceSequence;
        state.sliceGeneration++;
    }

    if (opts.requestResetPeakHold && !state.liveSliceDb.empty()) {
        state.peakHoldDb = state.liveSliceDb;
    }

    if (domainChanged || opts.requestResetZoom || (state.viewMaxX <= state.viewMinX)) {
        state.viewMinX = opts.minX;
        state.viewMaxX = opts.maxX;
        state.lastDomainMinX = opts.minX;
        state.lastDomainMaxX = opts.maxX;
        if (opts.requestResetZoom) {
            state.hasSelectionBand = false;
        }
    }

    // Two-way bindings sync in
    if (opts.viewMinX && opts.viewMaxX && *opts.viewMaxX > *opts.viewMinX) {
        state.viewMinX = std::clamp(*opts.viewMinX, opts.minX, opts.maxX);
        state.viewMaxX = std::clamp(*opts.viewMaxX, state.viewMinX + 1e-3f, opts.maxX);
    }
    if (!state.draggingThreshold) {
        state.thresholdGateDb = opts.thresholdGateDbPtr ? *opts.thresholdGateDbPtr : opts.thresholdGateDb;
    }
    if (opts.harmonicFundamentalX && *opts.harmonicFundamentalX > 0.0f) {
        state.harmonicFundamentalX = *opts.harmonicFundamentalX;
    }
    if (opts.selectedTrackId) {
        state.selectedTrackId = *opts.selectedTrackId;
    }

    Response baseResp = ctx.Interact(id);
    WaterfallViewResponse resp{};
    static_cast<Response&>(resp) = baseResp;
    resp.mode = opts.mode;

    Rect prevBounds = baseResp.rect;
    if (prevBounds.w <= 1.0f || prevBounds.h <= 1.0f) {
        prevBounds = ctx.GetParentPrevFrameBounds();
    }
    const ElementSpec spec = MakeWaterfallViewElementSpec(payload->opts, size, theme, effDisabled);
    const ElementGeometry prevElemGeom = Element::Resolve(prevBounds, {prevBounds.w, prevBounds.h}, spec, ctx.GetCanvas(), theme);
    const Rect ctrlBounds = (prevElemGeom.controlRect.w > 1.0f && prevElemGeom.controlRect.h > 1.0f)
        ? prevElemGeom.controlRect
        : prevBounds;
    const WaterfallGeometry geom = ComputeWaterfallGeometry(ctrlBounds, payload->opts, state.viewMinX, state.viewMaxX);

    const Vec2 mousePos = ctx.GetMousePos();
    const bool inTrace = geom.hasTrace && geom.traceRect.Contains(mousePos);
    const bool inWf = geom.hasWaterfall && geom.wfRect.Contains(mousePos);
    const bool inPlot = (inTrace || inWf) && !effDisabled;

    if (inPlot) {
        // 1. Mouse Wheel X-Axis Zoom centered on cursorX
        const Vec2 wheel = ctx.GetMouseWheel();
        if (std::abs(wheel.y) > 0.01f) {
            const float cursorX = geom.ScreenToX(mousePos.x);
            const float domainSpan = std::max(1e-3f, opts.maxX - opts.minX);
            const float curSpan = std::max(1e-3f, state.viewMaxX - state.viewMinX);
            const float zoomFactor = (wheel.y > 0.0f) ? 0.80f : 1.25f;
            const float newSpan = std::clamp(curSpan * zoomFactor, domainSpan / 32.0f, domainSpan);
            const float rel = (cursorX - state.viewMinX) / curSpan;
            float newMin = cursorX - rel * newSpan;
            float newMax = newMin + newSpan;
            if (newMin < opts.minX) {
                newMin = opts.minX;
                newMax = newMin + newSpan;
            }
            if (newMax > opts.maxX) {
                newMax = opts.maxX;
                newMin = std::max(opts.minX, newMax - newSpan);
            }
            state.viewMinX = newMin;
            state.viewMaxX = newMax;
            resp.changed = true;
            ctx.ConsumeMouseWheel();
            ctx.RequestRepaint(2);
        }

        // 2. Hit-test DOA / Emitter Tracks
        for (const auto& trk : opts.tracks) {
            const float headX = geom.XToScreen(trk.currentX);
            const float headY = geom.TimeAgoToWaterfallY(0.0f) + (opts.invertTimeAxis ? -8.0f : 8.0f);
            if (std::abs(mousePos.x - headX) <= 14.0f && std::abs(mousePos.y - headY) <= 14.0f) {
                resp.hoveredTrackId = trk.id;
                break;
            }
            for (const auto& pt : trk.history) {
                if (pt.timeAgoSec > geom.historyDurationSec) continue;
                const float px = geom.XToScreen(pt.xValue);
                const float py = geom.TimeAgoToWaterfallY(pt.timeAgoSec);
                if (std::abs(mousePos.x - px) <= 10.0f && std::abs(mousePos.y - py) <= 10.0f) {
                    resp.hoveredTrackId = trk.id;
                    break;
                }
            }
            if (resp.hoveredTrackId != -1) break;
        }

        // 3. Mouse Click / Drag Interactions
        if (ctx.IsMouseClicked()) {
            const KeyModifiers mods = ctx.GetKeyModifiers();
            const float gateY = geom.DbToTraceY(state.thresholdGateDb);
            if (inTrace && opts.showThresholdGate && std::abs(mousePos.y - gateY) <= 7.0f) {
                state.draggingThreshold = true;
            } else if (mods.shift) {
                state.draggingPan = true;
                state.panGrabMouseX = mousePos.x;
                state.panGrabMinX = state.viewMinX;
                state.panGrabMaxX = state.viewMaxX;
            } else if (resp.hoveredTrackId != -1) {
                if (state.selectedTrackId != resp.hoveredTrackId) {
                    state.selectedTrackId = resp.hoveredTrackId;
                    resp.trackSelectionChanged = true;
                    resp.changed = true;
                }
            } else {
                state.draggingSelection = true;
                state.dragAnchorX = geom.ScreenToX(mousePos.x);
                state.selectionStartX = state.dragAnchorX;
                state.selectionEndX = state.dragAnchorX;
            }
        }
    }

    if (ctx.IsMouseDown()) {
        if (state.draggingThreshold && geom.hasTrace) {
            state.thresholdGateDb = std::clamp(geom.TraceYToDb(mousePos.y), opts.dbFloor + 2.0f, opts.dbCeil - 2.0f);
            if (opts.thresholdGateDbPtr) {
                *opts.thresholdGateDbPtr = state.thresholdGateDb;
            }
            resp.changed = true;
            ctx.RequestRepaint(2);
        } else if (state.draggingPan) {
            const float span = state.panGrabMaxX - state.panGrabMinX;
            const float refW = std::max(1.0f, geom.hasWaterfall ? geom.wfRect.w : geom.traceRect.w);
            const float dxVal = ((mousePos.x - state.panGrabMouseX) / refW) * span;
            float newMin = state.panGrabMinX - dxVal;
            float newMax = state.panGrabMaxX - dxVal;
            if (newMin < opts.minX) {
                newMin = opts.minX;
                newMax = newMin + span;
            }
            if (newMax > opts.maxX) {
                newMax = opts.maxX;
                newMin = std::max(opts.minX, newMax - span);
            }
            state.viewMinX = newMin;
            state.viewMaxX = newMax;
            resp.changed = true;
            ctx.RequestRepaint(2);
        } else if (state.draggingSelection) {
            const float curX = geom.ScreenToX(mousePos.x);
            state.selectionStartX = std::min(state.dragAnchorX, curX);
            state.selectionEndX = std::max(state.dragAnchorX, curX);
            const float minPxSpan = ((state.viewMaxX - state.viewMinX) / std::max(1.0f, geom.wfRect.w)) * 4.0f;
            state.hasSelectionBand = (state.selectionEndX - state.selectionStartX) > minPxSpan;
            ctx.RequestRepaint(2);
        }
    } else {
        if (state.draggingSelection) {
            const float minPxSpan = ((state.viewMaxX - state.viewMinX) / std::max(1.0f, geom.wfRect.w)) * 4.0f;
            if ((state.selectionEndX - state.selectionStartX) <= minPxSpan) {
                state.hasSelectionBand = false;
                if (opts.showHarmonicComb && state.dragAnchorX > 0.0f) {
                    state.harmonicFundamentalX = state.dragAnchorX;
                    if (opts.harmonicFundamentalX) {
                        *opts.harmonicFundamentalX = state.harmonicFundamentalX;
                    }
                }
            }
        }
        state.draggingThreshold = false;
        state.draggingPan = false;
        state.draggingSelection = false;
    }

    // Write back two-way bindings
    if (opts.viewMinX) *opts.viewMinX = state.viewMinX;
    if (opts.viewMaxX) *opts.viewMaxX = state.viewMaxX;
    if (opts.selectionStartX) *opts.selectionStartX = state.selectionStartX;
    if (opts.selectionEndX) *opts.selectionEndX = state.selectionEndX;
    if (opts.selectedTrackId) *opts.selectedTrackId = state.selectedTrackId;

    // Populate Telemetry Response
    resp.visibleMinX = state.viewMinX;
    resp.visibleMaxX = state.viewMaxX;
    resp.zoomFactor = std::max(1.0f, (opts.maxX - opts.minX) / std::max(1e-3f, state.viewMaxX - state.viewMinX));
    resp.selectedTrackId = state.selectedTrackId;
    resp.populatedRows = state.validRows;

    if (inPlot) {
        resp.cursorInside = true;
        resp.cursorInTrace = inTrace;
        resp.cursorInWaterfall = inWf;
        resp.cursorX = geom.ScreenToX(mousePos.x);
        resp.cursorTimeAgoSec = inWf ? geom.WaterfallYToTimeAgo(mousePos.y) : 0.0f;
        resp.cursorLiveDb = SampleTraceVectorDb(state.liveSliceDb, opts, resp.cursorX);
        resp.cursorPeakDb = SampleTraceVectorDb(state.peakHoldDb, opts, resp.cursorX);
        resp.cursorAvgDb = SampleTraceVectorDb(state.averageDb, opts, resp.cursorX);
        resp.cursorWaterfallDb = inWf
            ? SampleRingBufferDb(state, opts, resp.cursorX, resp.cursorTimeAgoSec)
            : resp.cursorLiveDb;
    }

    if (state.hasSelectionBand) {
        resp.hasSelectionBand = true;
        resp.selectionStartX = state.selectionStartX;
        resp.selectionEndX = state.selectionEndX;
        resp.selectionCenterX = 0.5f * (state.selectionStartX + state.selectionEndX);
        resp.selectionSpanX = state.selectionEndX - state.selectionStartX;

        if (!state.liveSliceDb.empty()) {
            const float domainSpan = std::max(1e-5f, opts.maxX - opts.minX);
            const size_t b0 = static_cast<size_t>(std::clamp(((resp.selectionStartX - opts.minX) / domainSpan) * static_cast<float>(numBins - 1), 0.0f, static_cast<float>(numBins - 1)));
            const size_t b1 = static_cast<size_t>(std::clamp(((resp.selectionEndX - opts.minX) / domainSpan) * static_cast<float>(numBins - 1), 0.0f, static_cast<float>(numBins - 1)));
            float maxBandDb = opts.dbFloor;
            double linSum = 0.0;
            size_t count = 0;
            for (size_t b = std::min(b0, b1); b <= std::max(b0, b1); ++b) {
                const float db = state.liveSliceDb[b] + opts.gainDb;
                maxBandDb = std::max(maxBandDb, db);
                linSum += std::pow(10.0, static_cast<double>(db) / 10.0);
                count++;
            }
            resp.selectionBandPowerDb = (count > 0)
                ? static_cast<float>(10.0 * std::log10(std::max(1e-14, linSum / static_cast<double>(count))))
                : maxBandDb;
        }
    }

    // Dominant Peak across visible range
    if (!state.liveSliceDb.empty()) {
        float bestDb = -200.0f;
        float bestX = state.viewMinX;
        const float domainSpan = std::max(1e-5f, opts.maxX - opts.minX);
        for (uint32_t b = 0; b < numBins; ++b) {
            const float xVal = opts.minX + (static_cast<float>(b) / static_cast<float>(numBins - 1)) * domainSpan;
            if (xVal < state.viewMinX || xVal > state.viewMaxX) continue;
            const float db = state.liveSliceDb[b] + opts.gainDb;
            if (db > bestDb) {
                bestDb = db;
                bestX = xVal;
            }
        }
        resp.dominantPeakX = bestX;
        resp.dominantPeakDb = bestDb;
    }

    payload->resp = resp;

    // Compute visual hash for frame-to-frame damage tracking
    const auto packF = [](float v) noexcept -> uint64_t {
        uint32_t b = 0;
        std::memcpy(&b, &v, sizeof(float));
        return static_cast<uint64_t>(b);
    };
    uint64_t h = 14695981039346656037ULL;
    auto mix = [&](uint64_t v) noexcept {
        h ^= v;
        h *= 1099511628211ULL;
    };
    mix(state.fullGeneration);
    mix(state.sliceGeneration);
    mix(static_cast<uint64_t>(opts.mode));
    mix(static_cast<uint64_t>(opts.viewLayout));
    mix(static_cast<uint64_t>(opts.colormap));
    mix(packF(resp.visibleMinX));
    mix(packF(resp.visibleMaxX));
    mix(packF(opts.dbFloor));
    mix(packF(opts.dbCeil));
    mix(packF(opts.gainDb));
    mix(packF(state.thresholdGateDb));
    mix(packF(opts.gateSuppression));
    mix(packF(opts.gamma));
    mix(resp.cursorInside ? 1ULL : 0ULL);
    if (resp.cursorInside) {
        mix(packF(resp.cursorX));
        mix(packF(resp.cursorTimeAgoSec));
    }
    mix(resp.hasSelectionBand ? 1ULL : 0ULL);
    if (resp.hasSelectionBand) {
        mix(packF(resp.selectionStartX));
        mix(packF(resp.selectionEndX));
    }
    mix(static_cast<uint64_t>(resp.selectedTrackId + 2));
    mix(static_cast<uint64_t>(resp.hoveredTrackId + 2));
    node->visualHash = h;

    node->text = payload->opts.label.text.empty() ? "WaterfallView" : payload->opts.label.text;
    node->SetPayload(payload);
    node->measureFn = [](const Node* n, float availW, ICanvas* c, const UiTheme& t) noexcept -> Vec2 {
        const auto* p = n->As<WaterfallViewPayload>();
        return p ? WaterfallView::Measure({}, p->opts, n->uiSize, c, t, availW) : Vec2{520.0f, 420.0f};
    };
    node->renderFn = &DrawWaterfallViewNode;

    node->uiSize = size;
    node->disabled = effDisabled;
    node->hovered = baseResp.hovered;
    node->pressed = baseResp.pressed;
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = opts.heightMode;
    if (opts.height.has_value() && *opts.height > 0.0f &&
        !opts.label.HasContentOrReserve() && !opts.description.HasContentOrReserve()) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(*opts.height);
    }
    node->minHeight = opts.height.has_value() ? theme.Scale(*opts.height) : theme.Scale(280.0f);

    ctx.AdaptChildToContainer(node);
    if (opts.heightMode == SizeMode::Fit && node->heightMode == SizeMode::Stretch) {
        node->heightMode = SizeMode::Fit;
    }
    if (!opts.tooltip.empty() && baseResp.hovered) {
        ctx.SetTooltip(opts.tooltip, prevBounds);
    }

    return resp;
}

} // namespace guikit
