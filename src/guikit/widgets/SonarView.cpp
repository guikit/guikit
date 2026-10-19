#include "guikit/widgets/SonarView.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Control.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/graphics/Colormap.hpp"

namespace guikit {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

struct SonarViewState {
    uint32_t numBeams{0};
    uint32_t numRangeBins{0};
    std::vector<float> persistentGrid{};
    std::vector<uint32_t> abgrPixels{};
    uint32_t cachedImgW{0};
    uint32_t cachedImgH{0};
    uint64_t gridGeneration{1};
    uint64_t lastInputHash{0};

    float rangeZoom{1.0f};
    float eblBearingDeg{35.0f};
    float vrmRangeMeters{220.0f};
    bool eblVrmDragging{false};
    int selectedTargetId{-1};
    float prevSweepAngleDeg{0.0f};
    bool initialized{false};

    uint64_t baseLayerKey{0};
    uint32_t baseLayerW{0};
    uint32_t baseLayerH{0};
    std::vector<uint32_t> baseLayerPixels{};
    bool baseLayerValid{false};
};

struct SonarViewPayload {
    SonarViewOptions opts{};
    SonarViewResponse resp{};
    SonarViewState* state{nullptr};
    uint32_t imgW{0};
    uint32_t imgH{0};
    const uint32_t* abgrData{nullptr};
};

/// Normalizes an angle in degrees to [0, 360).
[[nodiscard]] inline float NormalizeBearingDeg(float deg) noexcept {
    float r = std::fmod(deg, 360.0f);
    if (r < 0.0f) r += 360.0f;
    return r;
}

/// Computes signed shortest angular difference (a - b) in degrees within [-180, +180].
[[nodiscard]] inline float SignedAngleDiffDeg(float a, float b) noexcept {
    return std::fmod(a - b + 540.0f, 360.0f) - 180.0f;
}

/// Returns true if `bearingDeg` lies inside the clockwise sector from `startDeg` to `endDeg`.
[[nodiscard]] inline bool IsBearingInSector(float bearingDeg, float startDeg, float endDeg) noexcept {
    const float b = NormalizeBearingDeg(bearingDeg);
    const float s = NormalizeBearingDeg(startDeg);
    const float e = NormalizeBearingDeg(endDeg);
    if (std::abs(endDeg - startDeg) >= 359.9f) return true;
    if (s <= e) {
        return b >= s && b <= e;
    }
    return b >= s || b <= e;
}

/// Geometry helper for mapping between polar (rangeMeters, bearingDeg) and screen (x, y).
struct SonarPlotGeometry {
    Rect plotRect{};
    SonarDisplayMode mode{SonarDisplayMode::PPI_360};
    float visibleMaxRange{500.0f};
    float minRange{0.0f};
    float headingDeg{0.0f};
    float sectorApertureDeg{120.0f};
    float startBearingDeg{0.0f};
    float endBearingDeg{360.0f};

    Vec2 origin{};          ///< Screen pixel origin of the transducer / radar center
    float maxRadiusPx{1.0f};///< Pixel radius corresponding to `visibleMaxRange`

    static SonarPlotGeometry Compute(const Rect& rect, const SonarViewOptions& opts, float rangeZoom) noexcept {
        SonarPlotGeometry g;
        g.plotRect = rect;
        g.mode = opts.displayMode;
        g.visibleMaxRange = std::max(10.0f, opts.maxRangeMeters * std::clamp(rangeZoom, 0.2f, 1.0f));
        g.minRange = std::clamp(opts.minRangeMeters, 0.0f, g.visibleMaxRange * 0.5f);
        g.headingDeg = opts.headingDeg;
        g.sectorApertureDeg = std::clamp(opts.sectorApertureDeg, 30.0f, 240.0f);
        g.startBearingDeg = opts.matrixStartBearingDeg;
        g.endBearingDeg = opts.matrixEndBearingDeg;

        if (g.mode == SonarDisplayMode::PPI_360) {
            const float pad = 28.0f;
            g.origin = {rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f};
            g.maxRadiusPx = std::max(24.0f, std::min(rect.w, rect.h) * 0.5f - pad);
        } else if (g.mode == SonarDisplayMode::SectorWedge) {
            const float topPad = 32.0f;
            const float bottomPad = 26.0f;
            const float sidePad = 32.0f;
            const float halfAngleRad = (g.sectorApertureDeg * 0.5f) * kDegToRad;
            const float sinHalf = std::max(0.25f, std::sin(std::min(halfAngleRad, kPi * 0.5f)));
            const float cosHalf = std::min(0.0f, std::cos(halfAngleRad)); // negative if > 180 deg

            const float availH = std::max(40.0f, rect.h - topPad - bottomPad);
            const float availW = std::max(40.0f, rect.w - sidePad * 2.0f);
            const float rByH = availH / (1.0f - cosHalf);
            const float rByW = (availW * 0.5f) / sinHalf;
            g.maxRadiusPx = std::max(24.0f, std::min(rByH, rByW));

            const float totalFanH = g.maxRadiusPx * (1.0f - cosHalf);
            const float topY = rect.y + (rect.h - totalFanH) * 0.5f + 4.0f;
            g.origin = {rect.x + rect.w * 0.5f, topY + g.maxRadiusPx};
        } else {
            g.origin = {rect.x + 48.0f, rect.Bottom() - 28.0f};
            g.maxRadiusPx = std::max(24.0f, rect.h - 48.0f);
        }
        return g;
    }

    [[nodiscard]] Rect GetBScanInnerRect() const noexcept {
        return Rect(
            plotRect.x + 50.0f,
            plotRect.y + 18.0f,
            std::max(20.0f, plotRect.w - 66.0f),
            std::max(20.0f, plotRect.h - 46.0f)
        );
    }

    [[nodiscard]] Vec2 PolarToScreen(float rangeMeters, float bearingDeg) const noexcept {
        const float rNorm = std::clamp(rangeMeters / std::max(1.0f, visibleMaxRange), 0.0f, 1.5f);
        if (mode == SonarDisplayMode::PPI_360) {
            const float rad = bearingDeg * kDegToRad;
            const float rPx = rNorm * maxRadiusPx;
            return {origin.x + std::sin(rad) * rPx, origin.y - std::cos(rad) * rPx};
        } else if (mode == SonarDisplayMode::SectorWedge) {
            const float relDeg = SignedAngleDiffDeg(bearingDeg, headingDeg);
            const float rad = relDeg * kDegToRad;
            const float rPx = rNorm * maxRadiusPx;
            return {origin.x + std::sin(rad) * rPx, origin.y - std::cos(rad) * rPx};
        } else {
            const Rect inner = GetBScanInnerRect();
            const float halfAp = sectorApertureDeg * 0.5f;
            const float relDeg = SignedAngleDiffDeg(bearingDeg, headingDeg);
            const float u = std::clamp((relDeg + halfAp) / std::max(1.0f, sectorApertureDeg), 0.0f, 1.0f);
            const float x = inner.x + u * inner.w;
            const float y = inner.Bottom() - std::clamp(rNorm, 0.0f, 1.0f) * inner.h;
            return {x, y};
        }
    }

    [[nodiscard]] bool ScreenToPolar(Vec2 pt, float& outRangeMeters, float& outBearingDeg) const noexcept {
        if (!plotRect.Contains(pt)) return false;
        if (mode == SonarDisplayMode::PPI_360) {
            const float dx = pt.x - origin.x;
            const float dy = origin.y - pt.y;
            const float distPx = std::sqrt(dx * dx + dy * dy);
            if (distPx > maxRadiusPx) return false;
            outRangeMeters = (distPx / maxRadiusPx) * visibleMaxRange;
            outBearingDeg = NormalizeBearingDeg(std::atan2(dx, dy) * kRadToDeg);
            return outRangeMeters >= minRange;
        } else if (mode == SonarDisplayMode::SectorWedge) {
            const float dx = pt.x - origin.x;
            const float dy = origin.y - pt.y;
            const float distPx = std::sqrt(dx * dx + dy * dy);
            if (distPx > maxRadiusPx) return false;
            const float relDeg = std::atan2(dx, dy) * kRadToDeg;
            if (std::abs(relDeg) > sectorApertureDeg * 0.5f) return false;
            outRangeMeters = (distPx / maxRadiusPx) * visibleMaxRange;
            outBearingDeg = NormalizeBearingDeg(headingDeg + relDeg);
            return outRangeMeters >= minRange;
        } else {
            const Rect inner = GetBScanInnerRect();
            if (!inner.Contains(pt)) return false;
            const float u = (pt.x - inner.x) / std::max(1.0f, inner.w);
            const float v = (inner.Bottom() - pt.y) / std::max(1.0f, inner.h);
            outRangeMeters = v * visibleMaxRange;
            const float relDeg = (u - 0.5f) * sectorApertureDeg;
            outBearingDeg = NormalizeBearingDeg(headingDeg + relDeg);
            return outRangeMeters >= minRange;
        }
    }
};

/// Samples `persistentGrid` with bilinear interpolation in `(beamIdx, rangeBin)` and applies Gain, TVG, and Threshold.
[[nodiscard]] float SampleProcessedEcho(
    const SonarViewState& state,
    const SonarViewOptions& opts,
    float rangeMeters,
    float bearingDeg
) noexcept {
    if (state.persistentGrid.empty() || state.numBeams == 0 || state.numRangeBins == 0) {
        return 0.0f;
    }
    if (rangeMeters < opts.minRangeMeters || rangeMeters > opts.maxRangeMeters) {
        return 0.0f;
    }

    const float rNorm = std::clamp(rangeMeters / std::max(1.0f, opts.maxRangeMeters), 0.0f, 1.0f);
    const float spanDeg = opts.matrixEndBearingDeg - opts.matrixStartBearingDeg;
    float beamNorm = 0.0f;
    if (std::abs(spanDeg) >= 359.5f) {
        beamNorm = NormalizeBearingDeg(bearingDeg - opts.matrixStartBearingDeg) / 360.0f;
    } else {
        const float rel = SignedAngleDiffDeg(bearingDeg, opts.headingDeg);
        const float halfSpan = std::max(1.0f, std::abs(spanDeg) * 0.5f);
        if (std::abs(rel) > halfSpan) return 0.0f;
        beamNorm = (rel + halfSpan) / (2.0f * halfSpan);
    }

    const float bFloat = std::clamp(beamNorm, 0.0f, 1.0f) * static_cast<float>(state.numBeams - 1);
    const float rFloat = rNorm * static_cast<float>(state.numRangeBins - 1);

    const uint32_t b0 = static_cast<uint32_t>(bFloat);
    const uint32_t b1 = std::min(state.numBeams - 1, b0 + 1);
    const float fb = bFloat - static_cast<float>(b0);

    const uint32_t r0 = static_cast<uint32_t>(rFloat);
    const uint32_t r1 = std::min(state.numRangeBins - 1, r0 + 1);
    const float fr = rFloat - static_cast<float>(r0);

    const float v00 = state.persistentGrid[b0 * state.numRangeBins + r0];
    const float v01 = state.persistentGrid[b0 * state.numRangeBins + r1];
    const float v10 = state.persistentGrid[b1 * state.numRangeBins + r0];
    const float v11 = state.persistentGrid[b1 * state.numRangeBins + r1];

    const float v0 = v00 + fr * (v01 - v00);
    const float v1 = v10 + fr * (v11 - v10);
    float raw = v0 + fb * (v1 - v0);

    // Apply Time-Varied Gain (TVG): compensates spherical spreading + absorption with distance
    const float tvgBoost = 1.0f + opts.tvgAmount * (1.35f * rNorm + 0.55f * std::sqrt(rNorm));
    // Apply Receiver Gain (dB -> linear amplitude multiplier)
    const float gainLinear = std::pow(10.0f, opts.gainDb / 28.0f);
    float val = raw * tvgBoost * gainLinear;

    // Clutter rejection threshold
    const float thresh = std::clamp(opts.clutterThreshold, 0.0f, 0.85f);
    if (val <= thresh) {
        return 0.0f;
    }
    val = (val - thresh) / std::max(0.05f, 1.0f - thresh);
    return std::clamp(val, 0.0f, 1.0f);
}

void DrawCircleOutline(
    ICanvas& canvas,
    Vec2 center,
    float radiusPx,
    Color fill,
    Color stroke = Color::Transparent(),
    float strokeWidth = 0.0f
) {
    if (radiusPx <= 0.5f) return;
    canvas.DrawRoundRect(
        Rect(center.x - radiusPx, center.y - radiusPx, radiusPx * 2.0f, radiusPx * 2.0f),
        radiusPx,
        fill,
        stroke,
        strokeWidth
    );
}

void DrawClosedPolygon(
    ICanvas& canvas,
    std::span<const Vec2> pts,
    Color fill,
    Color stroke,
    float strokeWidth
) {
    if (pts.size() < 3) return;
    if (fill.a > 0) {
        for (size_t i = 1; i + 1 < pts.size(); ++i) {
            canvas.DrawTriangle(pts[0], pts[i], pts[i + 1], fill);
        }
    }
    if (stroke.a > 0 && strokeWidth > 0.0f) {
        for (size_t i = 0; i < pts.size(); ++i) {
            canvas.DrawLine(pts[i], pts[(i + 1) % pts.size()], stroke, strokeWidth);
        }
    }
}

void DrawArcPolyline(
    ICanvas& canvas,
    Vec2 center,
    float radiusPx,
    float startScreenDeg,
    float endScreenDeg,
    Color color,
    float thickness,
    int segments = 48
) {
    if (radiusPx <= 1.0f || segments < 2) return;
    Vec2 prev{};
    for (int i = 0; i <= segments; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segments);
        const float deg = startScreenDeg + t * (endScreenDeg - startScreenDeg);
        const float rad = deg * kDegToRad;
        const Vec2 pt{center.x + std::sin(rad) * radiusPx, center.y - std::cos(rad) * radiusPx};
        if (i > 0) {
            canvas.DrawLine(prev, pt, color, thickness);
        }
        prev = pt;
    }
}

void DrawTargetSymbol(
    ICanvas& canvas,
    Vec2 pos,
    const SonarTarget& target,
    bool isSelected,
    bool isHovered,
    bool inGuardZone,
    const SonarPlotGeometry& geom
) {
    Color symColor{56, 189, 248, 240};
    switch (target.classification) {
        case SonarTargetClass::Diver:
            symColor = inGuardZone ? Color(244, 63, 94, 255) : Color(251, 146, 60, 255);
            break;
        case SonarTargetClass::UUV:
            symColor = Color(192, 132, 252, 255);
            break;
        case SonarTargetClass::SurfaceCraft:
            symColor = Color(52, 211, 153, 255);
            break;
        case SonarTargetClass::Biologic:
            symColor = Color(56, 189, 248, 220);
            break;
        case SonarTargetClass::StaticAnomaly:
            symColor = Color(250, 204, 21, 235);
            break;
    }

    // 1. Snail trail (historical positions)
    if (target.trail.size() >= 2) {
        for (size_t i = 0; i < target.trail.size(); ++i) {
            const float alphaFrac = static_cast<float>(i + 1) / static_cast<float>(target.trail.size() + 1);
            const Vec2 tp = geom.PolarToScreen(target.trail[i].rangeMeters, target.trail[i].bearingDeg);
            if (geom.plotRect.Contains(tp)) {
                Color tc = symColor;
                tc.a = static_cast<uint8_t>(alphaFrac * 150.0f);
                DrawCircleOutline(canvas, tp, 2.2f, tc);
            }
        }
    }

    // 2. Velocity vector (Course Over Ground / Speed leader line)
    if (target.speedMetersPerSec > 0.05f && geom.mode != SonarDisplayMode::BScan) {
        const float leaderLenPx = std::clamp(target.speedMetersPerSec * 14.0f, 12.0f, 38.0f);
        float courseScreenRad = target.courseDeg * kDegToRad;
        if (geom.mode == SonarDisplayMode::SectorWedge) {
            courseScreenRad = SignedAngleDiffDeg(target.courseDeg, geom.headingDeg) * kDegToRad;
        }
        const Vec2 tip{
            pos.x + std::sin(courseScreenRad) * leaderLenPx,
            pos.y - std::cos(courseScreenRad) * leaderLenPx
        };
        canvas.DrawLine(pos, tip, symColor, 1.6f);
    }

    // 3. Tactical symbol geometry
    const float r = isSelected ? 7.5f : (isHovered ? 6.8f : 5.8f);
    if (target.classification == SonarTargetClass::Diver) {
        const std::array<Vec2, 4> diamond = {{
            {pos.x, pos.y - r},
            {pos.x + r, pos.y},
            {pos.x, pos.y + r},
            {pos.x - r, pos.y}
        }};
        Color fill = symColor;
        fill.a = inGuardZone ? 95 : 55;
        DrawClosedPolygon(canvas, diamond, fill, symColor, 1.8f);
        DrawCircleOutline(canvas, pos, 2.0f, symColor);
    } else if (target.classification == SonarTargetClass::UUV) {
        std::array<Vec2, 6> hex{};
        for (int i = 0; i < 6; ++i) {
            const float a = (static_cast<float>(i) * 60.0f - 30.0f) * kDegToRad;
            hex[i] = {pos.x + std::cos(a) * r, pos.y + std::sin(a) * r};
        }
        Color fill = symColor;
        fill.a = 55;
        DrawClosedPolygon(canvas, hex, fill, symColor, 1.7f);
        DrawCircleOutline(canvas, pos, 2.0f, symColor);
    } else if (target.classification == SonarTargetClass::StaticAnomaly) {
        Color fill = symColor;
        fill.a = 55;
        canvas.DrawTriangle(
            {pos.x, pos.y - r},
            {pos.x + r * 0.9f, pos.y + r * 0.75f},
            {pos.x - r * 0.9f, pos.y + r * 0.75f},
            fill,
            symColor,
            1.6f
        );
    } else {
        Color fill = symColor;
        fill.a = 50;
        DrawCircleOutline(canvas, pos, r * 0.85f, fill, symColor, 1.6f);
        DrawCircleOutline(canvas, pos, 1.8f, symColor);
    }

    // 4. Selection / lock-on corner brackets
    if (isSelected || isHovered) {
        const float b = r + 4.5f;
        const float leg = 4.5f;
        const Color lockCol = isSelected ? Color(250, 204, 21, 255) : Color(226, 232, 240, 210);
        canvas.DrawLine({pos.x - b, pos.y - b}, {pos.x - b + leg, pos.y - b}, lockCol, 1.6f);
        canvas.DrawLine({pos.x - b, pos.y - b}, {pos.x - b, pos.y - b + leg}, lockCol, 1.6f);
        canvas.DrawLine({pos.x + b, pos.y - b}, {pos.x + b - leg, pos.y - b}, lockCol, 1.6f);
        canvas.DrawLine({pos.x + b, pos.y - b}, {pos.x + b, pos.y - b + leg}, lockCol, 1.6f);
        canvas.DrawLine({pos.x - b, pos.y + b}, {pos.x - b + leg, pos.y + b}, lockCol, 1.6f);
        canvas.DrawLine({pos.x - b, pos.y + b}, {pos.x - b, pos.y + b - leg}, lockCol, 1.6f);
        canvas.DrawLine({pos.x + b, pos.y + b}, {pos.x + b - leg, pos.y + b}, lockCol, 1.6f);
        canvas.DrawLine({pos.x + b, pos.y + b}, {pos.x + b, pos.y + b - leg}, lockCol, 1.6f);
    }
}

void RenderSonarViewNode(const Node* node, ICanvas& canvas, const UiTheme& theme) noexcept {
    const auto* payload = node->As<SonarViewPayload>();
    if (!payload) return;

    const SonarViewOptions& opts = payload->opts;
    const SonarViewResponse& resp = payload->resp;

    const Rect r = node->computedRect;
    if (r.w <= 8.0f || r.h <= 8.0f) return;

    const float radius = opts.border.radius.has_value()
        ? theme.Scale(*opts.border.radius)
        : theme.GetRadius(node->uiSize);
    const Color bgConsole = opts.background.Resolve(node->hovered, node->pressed, node->disabled, Color{6, 10, 18, 255});
    const Color borderCol = opts.border.color.Resolve(node->hovered, node->pressed, node->disabled, theme.palette.borderSubtle);
    const float borderW = (opts.border.width > 0.0f) ? theme.Scale(opts.border.width) : 1.0f;
    const Color gridLineCol{56, 189, 248, 48};
    const Color gridMajorCol{56, 189, 248, 95};
    const Color labelTextCol{148, 163, 184, 225};
    const Color accentTextCol{56, 189, 248, 235};

    const float colorbarW = opts.showColorbar ? 52.0f : 0.0f;
    const Rect plotRect(
        r.x + 2.0f,
        r.y + 2.0f,
        std::max(16.0f, r.w - colorbarW - 4.0f),
        std::max(16.0f, r.h - 4.0f)
    );
    const Rect colorbarRect(
        plotRect.Right() + 1.0f,
        r.y + 2.0f,
        std::max(0.0f, colorbarW - 3.0f),
        std::max(16.0f, r.h - 4.0f)
    );

    const SonarPlotGeometry geom = SonarPlotGeometry::Compute(plotRect, opts, resp.rangeZoom);

    auto makeSonarGpuDesc = [&](SonarViewState& state) {
        ICanvas::SonarGpuFieldDesc gpuDesc{};
        gpuDesc.widgetId = node->id.value;
        gpuDesc.dataGeneration = state.gridGeneration;
        gpuDesc.polarData = state.persistentGrid.data();
        gpuDesc.numBeams = state.numBeams;
        gpuDesc.numRangeBins = state.numRangeBins;
        gpuDesc.colormapId = static_cast<uint8_t>(opts.colormap);
        CopyColormapLutAbgr(opts.colormap, gpuDesc.colormapAbgr);
        gpuDesc.plotRect = plotRect;
        gpuDesc.origin = geom.origin;
        gpuDesc.maxRadiusPx = geom.maxRadiusPx;
        gpuDesc.minRangeMeters = geom.minRange;
        gpuDesc.visibleMaxRangeMeters = geom.visibleMaxRange;
        gpuDesc.matrixMaxRangeMeters = opts.maxRangeMeters;
        gpuDesc.matrixStartBearingDeg = opts.matrixStartBearingDeg;
        gpuDesc.matrixEndBearingDeg = opts.matrixEndBearingDeg;
        gpuDesc.displayMode = static_cast<uint8_t>(geom.mode);
        gpuDesc.bscanInnerRect = geom.GetBScanInnerRect();
        gpuDesc.headingDeg = geom.headingDeg;
        gpuDesc.sectorApertureDeg = geom.sectorApertureDeg;
        gpuDesc.gainDb = opts.gainDb;
        gpuDesc.tvgAmount = opts.tvgAmount;
        gpuDesc.clutterThreshold = opts.clutterThreshold;
        gpuDesc.bgColor = bgConsole;
        return gpuDesc;
    };

    const uint64_t staticKey = detail::HashArgs(
        std::round(r.x * 2.0f),
        std::round(r.y * 2.0f),
        std::round(r.w * 2.0f),
        std::round(r.h * 2.0f),
        canvas.GetScale(),
        canvas.SupportsGpuShaders(),
        static_cast<uint8_t>(opts.displayMode),
        static_cast<uint8_t>(opts.colormap),
        opts.maxRangeMeters,
        opts.minRangeMeters,
        opts.headingDeg,
        opts.sectorApertureDeg,
        opts.rangeRingCount,
        opts.showRangeRings,
        opts.showBearingDial,
        opts.showColorbar,
        opts.clutterThreshold,
        opts.guardZone.enabled,
        opts.guardZone.minRangeMeters,
        opts.guardZone.maxRangeMeters,
        opts.guardZone.startBearingDeg,
        opts.guardZone.endBearingDeg,
        resp.guardZoneAlarm,
        resp.rangeZoom,
        bgConsole.ToUint32(),
        borderCol.ToUint32(),
        borderW,
        radius
    );

    bool restoredFromCache = false;
    if (payload->state && canvas.SupportsGpuShaders()) {
        SonarViewState& state = *payload->state;
        if (state.baseLayerValid && state.baseLayerKey == staticKey) {
            if (canvas.RestoreRegion(r, state.baseLayerPixels, state.baseLayerW, state.baseLayerH)) {
                restoredFromCache = true;
                canvas.PushClip(plotRect);
                const auto gpuDesc = makeSonarGpuDesc(state);
                canvas.DrawSonarGpuField(gpuDesc, false);
                canvas.PopClip();
            }
        }
    }

    if (!restoredFromCache) {
        // =====================================================================
        // PASS A: Static Base Layer (Console frame, GPU hole, Guard Zone, Range Rings, Dial, Colorbar)
        // =====================================================================
        canvas.DrawRoundRect(r, radius, bgConsole, borderCol, borderW);
        canvas.PushClip(plotRect);

        if (payload->state) {
            SonarViewState& state = *payload->state;
            const auto gpuDesc = makeSonarGpuDesc(state);
            if (!canvas.DrawSonarGpuField(gpuDesc, true)) {
                const auto& lutAbgr = GetColormapLutAbgr(opts.colormap);
                const float resScale = std::min({
                    0.80f,
                    560.0f / std::max(1.0f, plotRect.w),
                    460.0f / std::max(1.0f, plotRect.h)
                });
                const uint32_t imgW = static_cast<uint32_t>(std::max(64.0f, std::round(plotRect.w * resScale)));
                const uint32_t imgH = static_cast<uint32_t>(std::max(64.0f, std::round(plotRect.h * resScale)));
                state.cachedImgW = imgW;
                state.cachedImgH = imgH;
                state.abgrPixels.resize(static_cast<size_t>(imgW) * imgH);

                const uint32_t bgAbgr = PackAbgr(bgConsole);
                const float stepX = plotRect.w / static_cast<float>(imgW);
                const float stepY = plotRect.h / static_cast<float>(imgH);

                for (uint32_t py = 0; py < imgH; ++py) {
                    uint32_t* rowPtr = state.abgrPixels.data() + static_cast<size_t>(py) * imgW;
                    const float sy = plotRect.y + (static_cast<float>(py) + 0.5f) * stepY;
                    for (uint32_t px = 0; px < imgW; ++px) {
                        const float sx = plotRect.x + (static_cast<float>(px) + 0.5f) * stepX;
                        float rMeters = 0.0f;
                        float brgDeg = 0.0f;
                        if (geom.ScreenToPolar({sx, sy}, rMeters, brgDeg)) {
                            const float echo = SampleProcessedEcho(state, opts, rMeters, brgDeg);
                            const size_t idx = static_cast<size_t>(std::clamp(echo * 255.0f, 0.0f, 255.0f));
                            rowPtr[px] = lutAbgr[idx];
                        } else {
                            rowPtr[px] = bgAbgr;
                        }
                    }
                }

                canvas.DrawPixels(
                    state.abgrPixels.data(),
                    imgW,
                    imgH,
                    plotRect
                );
            }
        }

        // 2. Guard Zone tactical sector overlay
        if (opts.guardZone.enabled) {
            const float gzMinR = std::clamp(opts.guardZone.minRangeMeters, 0.0f, geom.visibleMaxRange);
            const float gzMaxR = std::clamp(opts.guardZone.maxRangeMeters, gzMinR, geom.visibleMaxRange);
            if (gzMaxR > gzMinR + 1.0f) {
                const Color gzStroke = resp.guardZoneAlarm
                    ? Color(244, 63, 94, 225)
                    : Color(34, 211, 238, 145);
                const Color gzFill = resp.guardZoneAlarm
                    ? Color(244, 63, 94, 38)
                    : Color(34, 211, 238, 20);

                if (geom.mode == SonarDisplayMode::PPI_360 || geom.mode == SonarDisplayMode::SectorWedge) {
                    float span = std::fmod(opts.guardZone.endBearingDeg - opts.guardZone.startBearingDeg + 360.0f, 360.0f);
                    if (span <= 0.01f) span = 360.0f;

                    constexpr int kArcSteps = 28;
                    for (int i = 0; i < kArcSteps; ++i) {
                        const float b0 = opts.guardZone.startBearingDeg + (static_cast<float>(i) / kArcSteps) * span;
                        const float b1 = opts.guardZone.startBearingDeg + (static_cast<float>(i + 1) / kArcSteps) * span;
                        const Vec2 outer0 = geom.PolarToScreen(gzMaxR, b0);
                        const Vec2 outer1 = geom.PolarToScreen(gzMaxR, b1);
                        const Vec2 inner0 = geom.PolarToScreen(gzMinR, b0);
                        const Vec2 inner1 = geom.PolarToScreen(gzMinR, b1);
                        canvas.DrawTriangle(inner0, outer0, outer1, gzFill);
                        canvas.DrawTriangle(inner0, outer1, inner1, gzFill);
                        canvas.DrawLine(outer0, outer1, gzStroke, resp.guardZoneAlarm ? 1.8f : 1.2f);
                        canvas.DrawLine(inner0, inner1, gzStroke, resp.guardZoneAlarm ? 1.8f : 1.2f);
                    }
                    canvas.DrawLine(
                        geom.PolarToScreen(gzMinR, opts.guardZone.startBearingDeg),
                        geom.PolarToScreen(gzMaxR, opts.guardZone.startBearingDeg),
                        gzStroke,
                        resp.guardZoneAlarm ? 1.8f : 1.2f
                    );
                    canvas.DrawLine(
                        geom.PolarToScreen(gzMinR, opts.guardZone.endBearingDeg),
                        geom.PolarToScreen(gzMaxR, opts.guardZone.endBearingDeg),
                        gzStroke,
                        resp.guardZoneAlarm ? 1.8f : 1.2f
                    );
                } else {
                    const Vec2 p0 = geom.PolarToScreen(gzMaxR, opts.guardZone.startBearingDeg);
                    const Vec2 p1 = geom.PolarToScreen(gzMinR, opts.guardZone.endBearingDeg);
                    const Rect gzRect(
                        std::min(p0.x, p1.x),
                        std::min(p0.y, p1.y),
                        std::abs(p1.x - p0.x),
                        std::abs(p1.y - p0.y)
                    );
                    canvas.DrawRoundRect(gzRect, 2.0f, gzFill, gzStroke, 1.4f);
                }
            }
        }

        // 3. Range Rings & Azimuth Spokes / Bearing Dial
        const uint32_t ringCount = std::clamp(opts.rangeRingCount, 2u, 10u);
        if (geom.mode == SonarDisplayMode::PPI_360) {
            if (opts.showRangeRings) {
                for (uint32_t i = 1; i <= ringCount; ++i) {
                    const float frac = static_cast<float>(i) / static_cast<float>(ringCount);
                    const float rPx = frac * geom.maxRadiusPx;
                    const Color c = (i == ringCount) ? gridMajorCol : gridLineCol;
                    DrawCircleOutline(canvas, geom.origin, rPx, Color::Transparent(), c, (i == ringCount) ? 1.5f : 1.0f);

                    char rBuf[32];
                    std::snprintf(rBuf, sizeof(rBuf), "%.0fm", frac * geom.visibleMaxRange);
                    canvas.DrawText(rBuf, {geom.origin.x + 5.0f, geom.origin.y - rPx + 2.0f}, 10.0f, labelTextCol);
                }
            }

            if (opts.showBearingDial) {
                for (int deg = 0; deg < 360; deg += 30) {
                    const float rad = static_cast<float>(deg) * kDegToRad;
                    const Vec2 inner{geom.origin.x + std::sin(rad) * (geom.maxRadiusPx * 0.12f),
                                     geom.origin.y - std::cos(rad) * (geom.maxRadiusPx * 0.12f)};
                    const Vec2 outer{geom.origin.x + std::sin(rad) * geom.maxRadiusPx,
                                     geom.origin.y - std::cos(rad) * geom.maxRadiusPx};
                    const bool cardinal = (deg % 90 == 0);
                    canvas.DrawLine(inner, outer, cardinal ? gridMajorCol : gridLineCol, cardinal ? 1.2f : 0.9f);
                }

                for (int deg = 0; deg < 360; deg += 5) {
                    const float rad = static_cast<float>(deg) * kDegToRad;
                    const bool major = (deg % 30 == 0);
                    const bool med = (deg % 10 == 0);
                    const float tickLen = major ? 7.0f : (med ? 4.5f : 2.5f);
                    const Vec2 p0{geom.origin.x + std::sin(rad) * geom.maxRadiusPx,
                                  geom.origin.y - std::cos(rad) * geom.maxRadiusPx};
                    const Vec2 p1{geom.origin.x + std::sin(rad) * (geom.maxRadiusPx + tickLen),
                                  geom.origin.y - std::cos(rad) * (geom.maxRadiusPx + tickLen)};
                    canvas.DrawLine(p0, p1, major ? accentTextCol : labelTextCol, major ? 1.4f : 1.0f);

                    if (major) {
                        char bBuf[16];
                        if (deg == 0) std::snprintf(bBuf, sizeof(bBuf), "000 N");
                        else if (deg == 90) std::snprintf(bBuf, sizeof(bBuf), "090 E");
                        else if (deg == 180) std::snprintf(bBuf, sizeof(bBuf), "180 S");
                        else if (deg == 270) std::snprintf(bBuf, sizeof(bBuf), "270 W");
                        else std::snprintf(bBuf, sizeof(bBuf), "%03d", deg);

                        const float labelR = geom.maxRadiusPx + 15.0f;
                        const Vec2 lp{geom.origin.x + std::sin(rad) * labelR - 14.0f,
                                      geom.origin.y - std::cos(rad) * labelR - 6.0f};
                        canvas.DrawText(bBuf, lp, 10.5f, (deg % 90 == 0) ? accentTextCol : labelTextCol);
                    }
                }
            }
        } else if (geom.mode == SonarDisplayMode::SectorWedge) {
            const float halfAp = geom.sectorApertureDeg * 0.5f;
            if (opts.showRangeRings) {
                for (uint32_t i = 1; i <= ringCount; ++i) {
                    const float frac = static_cast<float>(i) / static_cast<float>(ringCount);
                    const float rPx = frac * geom.maxRadiusPx;
                    const bool outer = (i == ringCount);
                    DrawArcPolyline(
                        canvas,
                        geom.origin,
                        rPx,
                        -halfAp,
                        +halfAp,
                        outer ? gridMajorCol : gridLineCol,
                        outer ? 1.6f : 1.0f,
                        48
                    );

                    const float rightRad = halfAp * kDegToRad;
                    const Vec2 rp{
                        geom.origin.x + std::sin(rightRad) * rPx + 6.0f,
                        geom.origin.y - std::cos(rightRad) * rPx - 6.0f
                    };
                    char rBuf[32];
                    std::snprintf(rBuf, sizeof(rBuf), "%.0fm", frac * geom.visibleMaxRange);
                    canvas.DrawText(rBuf, rp, 10.0f, labelTextCol);
                }
            }

            const Vec2 leftEdge = geom.PolarToScreen(geom.visibleMaxRange, geom.headingDeg - halfAp);
            const Vec2 rightEdge = geom.PolarToScreen(geom.visibleMaxRange, geom.headingDeg + halfAp);
            canvas.DrawLine(geom.origin, leftEdge, gridMajorCol, 1.5f);
            canvas.DrawLine(geom.origin, rightEdge, gridMajorCol, 1.5f);

            if (opts.showBearingDial) {
                const int stepDeg = (geom.sectorApertureDeg <= 90.0f) ? 15 : 30;
                for (float rel = -halfAp; rel <= halfAp + 0.1f; rel += static_cast<float>(stepDeg)) {
                    const float brg = NormalizeBearingDeg(geom.headingDeg + rel);
                    const Vec2 pOuter = geom.PolarToScreen(geom.visibleMaxRange, brg);
                    const bool centerRay = (std::abs(rel) < 0.5f);
                    canvas.DrawLine(geom.origin, pOuter, centerRay ? gridMajorCol : gridLineCol, centerRay ? 1.3f : 0.9f);

                    const float rad = rel * kDegToRad;
                    const Vec2 lp{
                        geom.origin.x + std::sin(rad) * (geom.maxRadiusPx + 14.0f) - 12.0f,
                        geom.origin.y - std::cos(rad) * (geom.maxRadiusPx + 14.0f) - 7.0f
                    };
                    char bBuf[16];
                    std::snprintf(bBuf, sizeof(bBuf), "%03.0f°", brg);
                    canvas.DrawText(bBuf, lp, 10.0f, centerRay ? accentTextCol : labelTextCol);
                }
            }
        } else {
            const Rect inner = geom.GetBScanInnerRect();
            canvas.DrawRoundRect(inner, 0.0f, Color::Transparent(), gridMajorCol, 1.2f);
            if (opts.showRangeRings) {
                for (uint32_t i = 1; i <= ringCount; ++i) {
                    const float frac = static_cast<float>(i) / static_cast<float>(ringCount);
                    const float y = inner.Bottom() - frac * inner.h;
                    canvas.DrawLine({inner.x, y}, {inner.Right(), y}, gridLineCol, 1.0f);
                    char rBuf[32];
                    std::snprintf(rBuf, sizeof(rBuf), "%.0fm", frac * geom.visibleMaxRange);
                    canvas.DrawText(rBuf, {plotRect.x + 6.0f, y - 6.0f}, 10.0f, labelTextCol);
                }
            }
            if (opts.showBearingDial) {
                const float halfAp = geom.sectorApertureDeg * 0.5f;
                for (int i = 0; i <= 6; ++i) {
                    const float u = static_cast<float>(i) / 6.0f;
                    const float x = inner.x + u * inner.w;
                    canvas.DrawLine({x, inner.y}, {x, inner.Bottom()}, gridLineCol, 1.0f);
                    const float brg = NormalizeBearingDeg(geom.headingDeg + (u - 0.5f) * 2.0f * halfAp);
                    char bBuf[16];
                    std::snprintf(bBuf, sizeof(bBuf), "%03.0f°", brg);
                    canvas.DrawText(bBuf, {x - 12.0f, inner.Bottom() + 4.0f}, 10.0f, labelTextCol);
                }
            }
        }

        canvas.PopClip();

        // Right-hand Acoustic Backscatter dB Colorbar
        if (opts.showColorbar && colorbarRect.w >= 20.0f) {
            canvas.PushClip(colorbarRect);
            canvas.DrawRect(colorbarRect, Color(9, 13, 24, 255));
            canvas.DrawLine({colorbarRect.x, colorbarRect.y}, {colorbarRect.x, colorbarRect.Bottom()}, borderCol, 1.0f);

            const Rect barStrip(colorbarRect.x + 8.0f, colorbarRect.y + 24.0f, 12.0f, std::max(24.0f, colorbarRect.h - 48.0f));
            DrawColormapBar(canvas, barStrip, opts.colormap, true, false, 2.0f, Color(148, 163, 184, 120));

            const float threshY = barStrip.Bottom() - std::clamp(opts.clutterThreshold, 0.0f, 0.85f) * barStrip.h;
            canvas.DrawLine({barStrip.x - 3.0f, threshY}, {barStrip.Right() + 3.0f, threshY}, Color(244, 63, 94, 230), 1.8f);

            canvas.DrawText("dB", {colorbarRect.x + 8.0f, colorbarRect.y + 6.0f}, 10.0f, accentTextCol);
            for (int i = 0; i <= 4; ++i) {
                const float frac = static_cast<float>(i) / 4.0f;
                const float y = barStrip.y + frac * barStrip.h;
                const int dbVal = -i * 15;
                char dbBuf[16];
                std::snprintf(dbBuf, sizeof(dbBuf), "%d", dbVal);
                canvas.DrawText(dbBuf, {barStrip.Right() + 4.0f, y - 5.0f}, 9.5f, labelTextCol);
            }
            canvas.PopClip();
        }

        if (payload->state && canvas.SupportsGpuShaders()) {
            SonarViewState& state = *payload->state;
            if (canvas.CaptureRegion(r, state.baseLayerPixels, state.baseLayerW, state.baseLayerH)) {
                state.baseLayerKey = staticKey;
                state.baseLayerValid = true;
            }
        }
    }

    // =====================================================================
    // PASS B: Dynamic Tactical Overlay (Sweep Line, EBL/VRM, Contacts, Crosshair, HUD)
    // =====================================================================
    canvas.PushClip(plotRect);

    // 4. Active Rotating Sweep Line
    if (opts.scanMode == SonarScanMode::RotatingSweep) {
        const Vec2 sweepTip = geom.PolarToScreen(geom.visibleMaxRange, opts.sweepAngleDeg);
        const Color sweepCol = (opts.colormap == Colormap::Green)
            ? Color(74, 222, 128, 235)
            : Color(56, 189, 248, 230);
        if (geom.mode != SonarDisplayMode::BScan) {
            canvas.DrawLine(geom.origin, sweepTip, sweepCol, 2.0f);
        } else {
            const Rect inner = geom.GetBScanInnerRect();
            canvas.DrawLine({sweepTip.x, inner.y}, {sweepTip.x, inner.Bottom()}, sweepCol, 2.0f);
        }
    }

    // 5. Electronic Bearing Line (EBL) & Variable Range Marker (VRM)
    if (opts.showEblVrm) {
        const Color eblCol{250, 204, 21, 185};
        const Color vrmCol{56, 189, 248, 185};
        const float vrmR = std::clamp(resp.vrmRangeMeters, 0.0f, geom.visibleMaxRange);

        if (geom.mode == SonarDisplayMode::PPI_360) {
            const Vec2 eblEnd = geom.PolarToScreen(geom.visibleMaxRange, resp.eblBearingDeg);
            canvas.DrawLine(geom.origin, eblEnd, eblCol, 1.3f);
            const float vrmPx = (vrmR / geom.visibleMaxRange) * geom.maxRadiusPx;
            DrawCircleOutline(canvas, geom.origin, vrmPx, Color::Transparent(), vrmCol, 1.3f);
        } else if (geom.mode == SonarDisplayMode::SectorWedge) {
            const float halfAp = geom.sectorApertureDeg * 0.5f;
            if (std::abs(SignedAngleDiffDeg(resp.eblBearingDeg, geom.headingDeg)) <= halfAp) {
                const Vec2 eblEnd = geom.PolarToScreen(geom.visibleMaxRange, resp.eblBearingDeg);
                canvas.DrawLine(geom.origin, eblEnd, eblCol, 1.3f);
            }
            const float vrmPx = (vrmR / geom.visibleMaxRange) * geom.maxRadiusPx;
            DrawArcPolyline(canvas, geom.origin, vrmPx, -halfAp, +halfAp, vrmCol, 1.3f, 40);
        } else {
            const Rect inner = geom.GetBScanInnerRect();
            const Vec2 isect = geom.PolarToScreen(vrmR, resp.eblBearingDeg);
            canvas.DrawLine({isect.x, inner.y}, {isect.x, inner.Bottom()}, eblCol, 1.3f);
            canvas.DrawLine({inner.x, isect.y}, {inner.Right(), isect.y}, vrmCol, 1.3f);
        }

        const Vec2 markerPt = geom.PolarToScreen(vrmR, resp.eblBearingDeg);
        if (plotRect.Contains(markerPt)) {
            DrawCircleOutline(canvas, markerPt, 4.0f, Color(250, 204, 21, 230));
        }
    }

    // 6. Tracked Contacts / Diver Targets
    if (opts.showTargets) {
        for (const auto& t : opts.targets) {
            if (t.rangeMeters > geom.visibleMaxRange) continue;
            if (geom.mode != SonarDisplayMode::PPI_360) {
                if (std::abs(SignedAngleDiffDeg(t.bearingDeg, geom.headingDeg)) > geom.sectorApertureDeg * 0.5f) {
                    continue;
                }
            }
            const Vec2 pos = geom.PolarToScreen(t.rangeMeters, t.bearingDeg);
            if (!plotRect.Contains(pos)) continue;

            const bool isSel = (t.id == resp.selectedTargetId);
            const bool isHov = (t.id == resp.hoveredTargetId);
            const bool inGz = opts.guardZone.enabled &&
                              t.rangeMeters >= opts.guardZone.minRangeMeters &&
                              t.rangeMeters <= opts.guardZone.maxRangeMeters &&
                              IsBearingInSector(t.bearingDeg, opts.guardZone.startBearingDeg, opts.guardZone.endBearingDeg);

            DrawTargetSymbol(canvas, pos, t, isSel, isHov, inGz, geom);

            if (opts.showTargetLabels) {
                char tagBuf[64];
                std::snprintf(tagBuf, sizeof(tagBuf), "%s  %.0fm %03.0f°",
                              t.callsign.c_str(), t.rangeMeters, NormalizeBearingDeg(t.bearingDeg));
                const Color tagTextCol = isSel ? Color(250, 204, 21, 255) : Color(226, 232, 240, 235);
                canvas.DrawText(tagBuf, {pos.x + 10.0f, pos.y - 6.0f}, 10.5f, tagTextCol);
            }
        }
    }

    // 7. Live Cursor Crosshair
    if (resp.cursorInsidePlot) {
        const Vec2 cp = geom.PolarToScreen(resp.cursorRangeMeters, resp.cursorBearingDeg);
        DrawCircleOutline(canvas, cp, 4.5f, Color::Transparent(), Color(255, 255, 255, 190), 1.2f);
    }

    // 8. Corner Tactical HUD Overlays
    if (opts.showHudOverlay) {
        const Rect tlCard(plotRect.x + 10.0f, plotRect.y + 10.0f, 204.0f, 58.0f);
        canvas.DrawRoundRect(tlCard, 6.0f, Color(11, 17, 30, 215), Color(56, 189, 248, 80), 1.0f);

        const char* modeTitle = (opts.displayMode == SonarDisplayMode::PPI_360)
            ? "PPI 360° OMNI SONAR"
            : (opts.displayMode == SonarDisplayMode::SectorWedge ? "DDS MULTIBEAM SECTOR" : "B-SCAN AZIMUTH MATRIX");
        canvas.DrawText(modeTitle, {tlCard.x + 10.0f, tlCard.y + 6.0f}, 10.5f, accentTextCol);

        char rngLine[64];
        std::snprintf(rngLine, sizeof(rngLine), "SCALE: %.0f m   BINS: %ux%u",
                      geom.visibleMaxRange, opts.numBeams, opts.numRangeBins);
        canvas.DrawText(rngLine, {tlCard.x + 10.0f, tlCard.y + 23.0f}, 10.0f, Color(226, 232, 240, 230));

        char dspLine[64];
        std::snprintf(dspLine, sizeof(dspLine), "GAIN: %+.1f dB   TVG: %.0f%%",
                      opts.gainDb, opts.tvgAmount * 100.0f);
        canvas.DrawText(dspLine, {tlCard.x + 10.0f, tlCard.y + 39.0f}, 10.0f, labelTextCol);

        const Rect trCard(plotRect.Right() - 228.0f, plotRect.y + 10.0f, 218.0f, 76.0f);
        const Color trBorder = resp.guardZoneAlarm ? Color(244, 63, 94, 215) : Color(56, 189, 248, 80);
        canvas.DrawRoundRect(trCard, 6.0f, Color(11, 17, 30, 220), trBorder, 1.2f);

        if (resp.guardZoneAlarm) {
            char alarmBuf[64];
            std::snprintf(alarmBuf, sizeof(alarmBuf), "ALARM: PERIMETER BREACH (%u)", resp.guardZoneIntruderCount);
            canvas.DrawText(alarmBuf, {trCard.x + 10.0f, trCard.y + 6.0f}, 10.5f, Color(244, 63, 94, 255));
        } else {
            canvas.DrawText("GUARD ZONE: SECURE", {trCard.x + 10.0f, trCard.y + 6.0f}, 10.5f, Color(52, 211, 153, 240));
        }

        const SonarTarget* selTarget = nullptr;
        for (const auto& t : opts.targets) {
            if (t.id == resp.selectedTargetId) {
                selTarget = &t;
                break;
            }
        }
        if (selTarget) {
            char tLine1[64];
            std::snprintf(tLine1, sizeof(tLine1), "LOCK: %s  (SNR +%.1fdB)",
                          selTarget->callsign.c_str(), selTarget->snrDb);
            canvas.DrawText(tLine1, {trCard.x + 10.0f, trCard.y + 24.0f}, 10.0f, Color(250, 204, 21, 255));

            char tLine2[64];
            std::snprintf(tLine2, sizeof(tLine2), "RNG %.1fm | BRG %05.1f°",
                          selTarget->rangeMeters, NormalizeBearingDeg(selTarget->bearingDeg));
            canvas.DrawText(tLine2, {trCard.x + 10.0f, trCard.y + 40.0f}, 10.0f, Color(226, 232, 240, 235));

            char tLine3[64];
            std::snprintf(tLine3, sizeof(tLine3), "SPD %.2fm/s | COG %03.0f° | Z -%.1fm",
                          selTarget->speedMetersPerSec, selTarget->courseDeg, selTarget->depthMeters);
            canvas.DrawText(tLine3, {trCard.x + 10.0f, trCard.y + 56.0f}, 9.5f, labelTextCol);
        } else {
            canvas.DrawText("NO TARGET LOCKED", {trCard.x + 10.0f, trCard.y + 28.0f}, 10.0f, labelTextCol);
            canvas.DrawText("Click contact reticle to track", {trCard.x + 10.0f, trCard.y + 46.0f}, 9.5f, labelTextCol);
        }

        const Rect blCard(plotRect.x + 10.0f, plotRect.Bottom() - 44.0f, 244.0f, 36.0f);
        canvas.DrawRoundRect(blCard, 6.0f, Color(11, 17, 30, 215), Color(56, 189, 248, 70), 1.0f);
        char eblLine[80];
        std::snprintf(eblLine, sizeof(eblLine), "EBL: %05.1f°   VRM: %5.1f m",
                      NormalizeBearingDeg(resp.eblBearingDeg), resp.vrmRangeMeters);
        canvas.DrawText(eblLine, {blCard.x + 10.0f, blCard.y + 4.0f}, 10.0f, Color(250, 204, 21, 240));

        char curLine[80];
        if (resp.cursorInsidePlot) {
            std::snprintf(curLine, sizeof(curLine), "CUR: %5.1f m @ %05.1f°  (%+.1f dB)",
                          resp.cursorRangeMeters, NormalizeBearingDeg(resp.cursorBearingDeg), resp.cursorEchoDb);
        } else {
            std::snprintf(curLine, sizeof(curLine), "CUR: ---.- m @ ---.-°  (Shift+Drag EBL)");
        }
        canvas.DrawText(curLine, {blCard.x + 10.0f, blCard.y + 19.0f}, 9.5f, labelTextCol);
    }

    canvas.PopClip();
}

} // namespace

Vec2 SonarView::Measure(
    std::span<const float> /*polarMatrix*/,
    const SonarViewOptions& opts,
    UiSize /*size*/,
    ICanvas* /*canvas*/,
    const UiTheme& theme,
    float availableWidth
) {
    float defaultW = availableWidth > 0.0f ? availableWidth : theme.Scale(520.0f);
    if (opts.stretch == StretchMode::Fill && availableWidth > 0.0f && opts.width.has_value()) {
        defaultW = availableWidth * std::clamp(*opts.width, 0.1f, 1.0f);
    } else if (opts.width.has_value() && *opts.width > 0.0f) {
        defaultW = theme.Scale(*opts.width);
    }
    const float defaultH = (opts.height.has_value() && *opts.height > 0.0f)
        ? theme.Scale(*opts.height)
        : theme.Scale(420.0f);
    return {defaultW, defaultH};
}

SonarViewResponse SonarView::Run(
    Context& ctx,
    std::span<const float> polarMatrix,
    const SonarViewOptions& optsIn,
    std::source_location loc
) {
    SonarViewOptions opts = optsIn;
    Control::InheritElementSizing(ctx, opts.stretch, opts.width, opts.height);

    const Id id = ctx.MakeChildId(loc);
    const bool isDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize effectiveSize = ctx.GetCurrentSize();
    const UiTheme& theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();

    Response baseRes = ctx.Interact(id);
    SonarViewResponse resp{};
    static_cast<Response&>(resp) = baseRes;

    Rect prevBounds = baseRes.rect;
    if (prevBounds.w <= 1.0f || prevBounds.h <= 1.0f) {
        prevBounds = ctx.GetParentPrevFrameBounds();
    }
    const Rect activeRect = prevBounds;

    auto& state = ctx.GetOrCreateState<SonarViewState>(id);

    const uint32_t numBeams = std::max(8u, opts.numBeams);
    const uint32_t numBins = std::max(8u, opts.numRangeBins);
    const size_t totalCells = static_cast<size_t>(numBeams) * numBins;

    if (!state.initialized || state.numBeams != numBeams || state.numRangeBins != numBins || opts.requestClearPersistence) {
        state.numBeams = numBeams;
        state.numRangeBins = numBins;
        state.persistentGrid.assign(totalCells, 0.0f);
        state.prevSweepAngleDeg = opts.sweepAngleDeg;
        state.gridGeneration++;
        if (!state.initialized) {
            state.rangeZoom = opts.viewRangeZoom ? std::clamp(*opts.viewRangeZoom, 0.2f, 1.0f) : 1.0f;
            state.eblBearingDeg = opts.eblBearingDeg ? *opts.eblBearingDeg : 35.0f;
            state.vrmRangeMeters = opts.vrmRangeMeters ? *opts.vrmRangeMeters : (opts.maxRangeMeters * 0.48f);
            state.selectedTargetId = opts.selectedTargetId ? *opts.selectedTargetId : -1;
            state.initialized = true;
        }
    }

    if (opts.requestResetView) {
        state.rangeZoom = 1.0f;
        if (opts.viewRangeZoom) *opts.viewRangeZoom = 1.0f;
    }
    if (opts.viewRangeZoom) {
        state.rangeZoom = std::clamp(*opts.viewRangeZoom, 0.2f, 1.0f);
    }
    if (opts.eblBearingDeg) {
        state.eblBearingDeg = *opts.eblBearingDeg;
    }
    if (opts.vrmRangeMeters) {
        state.vrmRangeMeters = *opts.vrmRangeMeters;
    }
    if (opts.selectedTargetId) {
        state.selectedTargetId = *opts.selectedTargetId;
    }

    // 1. Update Polar Persistence Buffer from `polarMatrix`
    const float persist = std::clamp(opts.persistence, 0.0f, 0.97f);
    if (polarMatrix.size() >= totalCells) {
        uint64_t inHash = detail::HashArgs(opts.sweepAngleDeg, static_cast<uint8_t>(opts.scanMode), persist);
        const size_t step = std::max<size_t>(1, totalCells / 64);
        for (size_t i = 0; i < totalCells; i += step) {
            inHash = detail::HashArgs(inHash, polarMatrix[i]);
        }
        if (inHash != state.lastInputHash || state.gridGeneration <= 1) {
            state.lastInputHash = inHash;
            state.gridGeneration++;
            if (opts.scanMode == SonarScanMode::MultibeamContinuous) {
                for (size_t i = 0; i < totalCells; ++i) {
                    const float incoming = std::clamp(polarMatrix[i], 0.0f, 1.0f);
                    const float decayed = state.persistentGrid[i] * persist;
                    state.persistentGrid[i] = std::max(incoming, decayed * 0.92f + incoming * 0.08f);
                }
            } else {
                const float frameDecay = 0.965f + 0.032f * persist;
                const float spanDeg = opts.matrixEndBearingDeg - opts.matrixStartBearingDeg;
                const float halfSweep = std::max(3.0f, opts.sweepBeamWidthDeg * 0.5f);
                for (uint32_t b = 0; b < numBeams; ++b) {
                    const float beamNorm = static_cast<float>(b) / static_cast<float>(std::max(1u, numBeams - 1));
                    float beamBrg = 0.0f;
                    if (std::abs(spanDeg) >= 359.5f) {
                        beamBrg = NormalizeBearingDeg(opts.matrixStartBearingDeg + beamNorm * 360.0f);
                    } else {
                        beamBrg = NormalizeBearingDeg(opts.headingDeg + (beamNorm - 0.5f) * spanDeg);
                    }
                    const float diff = std::abs(SignedAngleDiffDeg(beamBrg, opts.sweepAngleDeg));
                    const bool inBeam = (diff <= halfSweep);
                    float* row = state.persistentGrid.data() + static_cast<size_t>(b) * numBins;
                    const float* srcRow = polarMatrix.data() + static_cast<size_t>(b) * numBins;
                    for (uint32_t r = 0; r < numBins; ++r) {
                        float v = row[r] * frameDecay;
                        if (inBeam) {
                            const float beamWeight = 1.0f - (diff / (halfSweep + 0.1f)) * 0.35f;
                            v = std::max(v, std::clamp(srcRow[r] * beamWeight, 0.0f, 1.0f));
                        }
                        row[r] = v;
                    }
                }
            }
        }
    }
    state.prevSweepAngleDeg = opts.sweepAngleDeg;

    // 2. Interactive Input Handling
    const float colorbarW = opts.showColorbar ? 52.0f : 0.0f;
    const Rect plotRect(
        activeRect.x + 2.0f,
        activeRect.y + 2.0f,
        std::max(16.0f, activeRect.w - colorbarW - 4.0f),
        std::max(16.0f, activeRect.h - 4.0f)
    );
    SonarPlotGeometry geom = SonarPlotGeometry::Compute(plotRect, opts, state.rangeZoom);

    const Vec2 mousePos = ctx.GetMousePos();
    const bool hovered = !isDisabled && activeRect.Contains(mousePos);
    resp.hovered = hovered;

    if (hovered && plotRect.Contains(mousePos)) {
        const Vec2 wheel = ctx.GetMouseWheel();
        if (std::abs(wheel.y) > 0.01f) {
            const float factor = (wheel.y > 0.0f) ? 0.88f : 1.14f;
            state.rangeZoom = std::clamp(state.rangeZoom * factor, 0.2f, 1.0f);
            if (opts.viewRangeZoom) *opts.viewRangeZoom = state.rangeZoom;
            geom = SonarPlotGeometry::Compute(plotRect, opts, state.rangeZoom);
            ctx.ConsumeMouseWheel();
            ctx.RequestRepaint(2);
            resp.changed = true;
        }

        float curR = 0.0f;
        float curBrg = 0.0f;
        if (geom.ScreenToPolar(mousePos, curR, curBrg)) {
            resp.cursorInsidePlot = true;
            resp.cursorRangeMeters = curR;
            resp.cursorBearingDeg = curBrg;
            const float echoNorm = SampleProcessedEcho(state, opts, curR, curBrg);
            resp.cursorEchoDb = -60.0f + echoNorm * 60.0f;
        }

        float bestDistSq = 18.0f * 18.0f;
        for (const auto& t : opts.targets) {
            if (t.rangeMeters > geom.visibleMaxRange) continue;
            const Vec2 tp = geom.PolarToScreen(t.rangeMeters, t.bearingDeg);
            const float dx = mousePos.x - tp.x;
            const float dy = mousePos.y - tp.y;
            const float d2 = dx * dx + dy * dy;
            if (d2 <= bestDistSq) {
                bestDistSq = d2;
                resp.hoveredTargetId = t.id;
            }
        }

        const Vec2 eblMarkerPt = geom.PolarToScreen(state.vrmRangeMeters, state.eblBearingDeg);
        const float eblDistSq = (mousePos.x - eblMarkerPt.x) * (mousePos.x - eblMarkerPt.x) +
                                (mousePos.y - eblMarkerPt.y) * (mousePos.y - eblMarkerPt.y);

        if (ctx.IsMouseClicked()) {
            if (resp.hoveredTargetId != -1) {
                state.selectedTargetId = resp.hoveredTargetId;
                if (opts.selectedTargetId) *opts.selectedTargetId = state.selectedTargetId;
                resp.targetSelectionChanged = true;
                resp.clicked = true;
                resp.changed = true;
            } else if (opts.showEblVrm && (ctx.GetKeyModifiers().shift || eblDistSq <= 16.0f * 16.0f)) {
                state.eblVrmDragging = true;
                ctx.SetActiveId(id);
            }
        }
    }

    if (state.eblVrmDragging) {
        if (ctx.IsMouseDown()) {
            float curR = 0.0f;
            float curBrg = 0.0f;
            if (geom.ScreenToPolar(mousePos, curR, curBrg)) {
                state.vrmRangeMeters = std::clamp(curR, 5.0f, geom.visibleMaxRange);
                state.eblBearingDeg = curBrg;
                if (opts.vrmRangeMeters) *opts.vrmRangeMeters = state.vrmRangeMeters;
                if (opts.eblBearingDeg) *opts.eblBearingDeg = state.eblBearingDeg;
                resp.changed = true;
                ctx.RequestRepaint(2);
            }
        } else {
            state.eblVrmDragging = false;
            if (ctx.GetActiveId() == id) ctx.SetActiveId(Id::Null());
        }
    }

    // 3. Evaluate Guard Zone Intrusion Alarm
    if (opts.guardZone.enabled) {
        for (const auto& t : opts.targets) {
            if (t.classification == SonarTargetClass::Diver || t.classification == SonarTargetClass::UUV) {
                if (t.rangeMeters >= opts.guardZone.minRangeMeters &&
                    t.rangeMeters <= opts.guardZone.maxRangeMeters &&
                    IsBearingInSector(t.bearingDeg, opts.guardZone.startBearingDeg, opts.guardZone.endBearingDeg)) {
                    resp.guardZoneAlarm = true;
                    resp.guardZoneIntruderCount++;
                }
            }
        }
    }

    resp.displayMode = opts.displayMode;
    resp.visibleMaxRangeMeters = geom.visibleMaxRange;
    resp.rangeZoom = state.rangeZoom;
    resp.eblVrmActive = opts.showEblVrm;
    resp.eblBearingDeg = state.eblBearingDeg;
    resp.vrmRangeMeters = state.vrmRangeMeters;
    resp.selectedTargetId = state.selectedTargetId;

    if (hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, baseRes.rect);
    }

    Node* node = ctx.CreateNode(NodeType::SonarView, id);
    auto* payload = ctx.AllocPayload<SonarViewPayload>();
    payload->opts = opts;
    payload->opts.tooltip = ctx.AllocString(opts.tooltip);
    payload->resp = resp;
    payload->state = &state;

    node->text = "SonarView";
    node->SetPayload(payload);

    uint64_t gridFp = 0;
    if (!state.persistentGrid.empty()) {
        const size_t stride = std::max<size_t>(1, state.persistentGrid.size() / 64);
        for (size_t i = 0; i < state.persistentGrid.size(); i += stride) {
            gridFp = detail::HashArgs(gridFp, state.persistentGrid[i]);
        }
    }

    node->visualHash = detail::HashArgs(
        gridFp,
        static_cast<uint8_t>(opts.displayMode),
        static_cast<uint8_t>(opts.scanMode),
        static_cast<uint8_t>(opts.colormap),
        opts.gainDb,
        opts.tvgAmount,
        opts.clutterThreshold,
        opts.persistence,
        opts.sweepAngleDeg,
        opts.sectorApertureDeg,
        state.rangeZoom,
        state.eblBearingDeg,
        state.vrmRangeMeters,
        state.selectedTargetId,
        resp.cursorInsidePlot ? resp.cursorRangeMeters : -1.0f,
        resp.cursorInsidePlot ? resp.cursorBearingDeg : -1.0f,
        resp.guardZoneAlarm,
        resp.guardZoneIntruderCount,
        plotRect.w,
        plotRect.h,
        effectiveSize,
        isDisabled,
        canvas ? canvas->SupportsGpuShaders() : false,
        opts.background,
        opts.border
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* c, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<SonarViewPayload>();
        if (!p) return {0.0f, 0.0f};
        return SonarView::Measure({}, p->opts, n->uiSize, c, th, availableWidth);
    };

    node->renderFn = &RenderSonarViewNode;

    node->uiSize = effectiveSize;
    node->disabled = isDisabled;
    node->hovered = hovered;
    node->pressed = baseRes.pressed;
    node->heightMode = opts.heightMode;

    ControlSpec ctrlSpec{};
    ctrlSpec.width = opts.width;
    ctrlSpec.height = opts.height;
    Control::SetupNode(ctx, node, ctrlSpec, opts.stretch, opts.align);
    if (!node->parent || node->parent->type != NodeType::Element) {
        node->heightMode = opts.heightMode;
        if (opts.height.has_value() && *opts.height > 0.0f) {
            node->heightMode = SizeMode::Fixed;
            node->fixedHeight = theme.Scale(*opts.height);
        }
        node->minHeight = (opts.height.has_value() && *opts.height > 0.0f) ? theme.Scale(*opts.height) : theme.Scale(260.0f);
        ctx.AdaptChildToContainer(node);
        if (opts.heightMode == SizeMode::Fit && !opts.height.has_value() && node->heightMode == SizeMode::Stretch) {
            node->heightMode = SizeMode::Fit;
        }
    }

    return resp;
}

} // namespace guikit
