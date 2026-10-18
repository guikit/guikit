#include "guikit/widgets/Histogram.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Element.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

namespace {

enum class HistDragMode : uint8_t {
    None = 0,
    DragLow,
    DragHigh,
    DragLevel
};

struct HistogramInternalState {
    HistDragMode dragMode{HistDragMode::None};
    float grabMouseX{0.0f};
    float grabLow{0.0f};
    float grabHigh{0.0f};
    float grabLevel{0.0f};
    float grabWindow{1.0f};
    double lastClickTimeSec{-10.0};
    Vec2 lastClickPos{0.0f, 0.0f};
};

struct HistogramGeometry {
    Rect outerRect{};
    Rect headerRect{};
    Rect plotRect{};
    Rect colorbarRect{};
    std::array<Rect, 4> pillRects{};
    bool hasHeader{false};
    bool hasPills{false};
    bool hasColorbar{false};
};

[[nodiscard]] ElementSpec MakeHistogramElementSpec(
    const HistogramOptions& opts,
    UiSize size,
    const UiTheme& theme,
    bool disabled
) noexcept {
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .icon = opts.icon,
        .badge = opts.badge,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .align = opts.align,
        .stretch = opts.stretch,
        .width = opts.width,
        .widgetSize = size,
        .labelGap = theme.Scale(6.0f),
        .descGap = theme.Scale(4.0f),
        .inlineGap = theme.Scale(12.0f),
        .stretchableControl = true,
        .stretchHeightWhenStandalone = (opts.heightMode == SizeMode::Stretch),
        .disabled = disabled
    };
}

[[nodiscard]] HistogramGeometry ComputeHistogramGeometry(
    const Rect& box,
    const HistogramData& data,
    const HistogramOptions& opts,
    float scale
) noexcept {
    HistogramGeometry g{};
    g.outerRect = box;
    const float pad = 6.0f * scale;
    g.hasPills = (data.channelCount >= 3) && opts.showChannelPills;
    g.hasHeader = g.hasPills || opts.showRangeReadout;
    g.hasColorbar = opts.showColorbar && (box.h >= 64.0f * scale);

    const float headerH = g.hasHeader ? (20.0f * scale) : 0.0f;
    const float colorbarH = g.hasColorbar ? (8.0f * scale) : 0.0f;
    const float colorbarGap = g.hasColorbar ? (4.0f * scale) : 0.0f;

    if (g.hasHeader) {
        g.headerRect = Rect(box.x + pad, box.y + 4.0f * scale, std::max(0.0f, box.w - pad * 2.0f), headerH);
        if (g.hasPills) {
            const float pillW = 28.0f * scale;
            const float pillGap = 3.0f * scale;
            const float totalPillsW = pillW * 4.0f + pillGap * 3.0f;
            float px = g.headerRect.Right() - totalPillsW;
            const float py = g.headerRect.y + 1.0f * scale;
            const float ph = std::max(12.0f, g.headerRect.h - 2.0f * scale);
            for (size_t i = 0; i < 4; ++i) {
                g.pillRects[i] = Rect(px, py, pillW, ph);
                px += pillW + pillGap;
            }
        }
    }

    const float plotTop = box.y + (g.hasHeader ? (4.0f * scale + headerH + 2.0f * scale) : pad);
    const float plotBottom = box.Bottom() - pad - colorbarH - colorbarGap;
    g.plotRect = Rect(
        box.x + pad,
        plotTop,
        std::max(16.0f, box.w - pad * 2.0f),
        std::max(16.0f, plotBottom - plotTop)
    );

    if (g.hasColorbar) {
        g.colorbarRect = Rect(
            g.plotRect.x,
            g.plotRect.Bottom() + colorbarGap,
            g.plotRect.w,
            colorbarH
        );
    }
    return g;
}

[[nodiscard]] float ScaleBinValue(uint32_t count, uint32_t maxCount, HistogramScaleMode mode) noexcept {
    if (count == 0 || maxCount == 0) {
        return 0.0f;
    }
    switch (mode) {
        case HistogramScaleMode::Sqrt:
            return std::sqrt(static_cast<float>(count)) / std::sqrt(static_cast<float>(maxCount));
        case HistogramScaleMode::Logarithmic:
            return std::log1p(static_cast<float>(count)) / std::log1p(static_cast<float>(maxCount));
        case HistogramScaleMode::Linear:
            return static_cast<float>(count) / static_cast<float>(maxCount);
    }
    return 0.0f;
}

[[nodiscard]] uint32_t MaxBinOf(std::span<const uint32_t> bins) noexcept {
    uint32_t m = 1;
    for (uint32_t v : bins) {
        if (v > m) m = v;
    }
    return m;
}

void FormatHistValue(char* buf, size_t bufSize, float v, float span) noexcept {
    if (span >= 50.0f) {
        std::snprintf(buf, bufSize, "%.0f", v);
    } else if (span >= 2.0f) {
        std::snprintf(buf, bufSize, "%.1f", v);
    } else {
        std::snprintf(buf, bufSize, "%.3f", v);
    }
}

[[nodiscard]] Color GetChannelAccentColor(HistogramChannel ch, bool isDark) noexcept {
    switch (ch) {
        case HistogramChannel::Master:
            return isDark ? Color(96, 175, 255, 240) : Color(37, 99, 235, 240);
        case HistogramChannel::Red:
            return Color(248, 82, 82, 240);
        case HistogramChannel::Green:
            return Color(52, 211, 102, 240);
        case HistogramChannel::Blue:
            return Color(76, 154, 255, 240);
    }
    return Color(96, 175, 255, 240);
}

void DrawChannelDistribution(
    ICanvas& c,
    const Rect& plotRect,
    std::span<const uint32_t> bins,
    uint32_t maxCount,
    HistogramScaleMode scaleMode,
    Color fillCol,
    Color strokeCol
) noexcept {
    if (bins.empty() || plotRect.w <= 2.0f || plotRect.h <= 2.0f) {
        return;
    }
    const size_t n = bins.size();
    const float barW = plotRect.w / static_cast<float>(n);
    const float usableH = std::max(2.0f, plotRect.h - 3.0f);

    Vec2 prevTop{};
    bool hasPrev = false;
    for (size_t i = 0; i < n; ++i) {
        const float normH = std::clamp(ScaleBinValue(bins[i], maxCount, scaleMode), 0.0f, 1.0f);
        const float bh = normH * usableH;
        const float bx = plotRect.x + static_cast<float>(i) * barW;
        const float by = plotRect.Bottom() - bh;
        if (bh >= 0.5f && fillCol.a > 0) {
            c.DrawRect(Rect(bx, by, std::max(1.0f, barW + 0.35f), bh), fillCol);
        }
        const Vec2 curTop{bx + barW * 0.5f, by};
        if (hasPrev && strokeCol.a > 0) {
            c.DrawLine(prevTop, curTop, strokeCol, 1.25f);
        }
        prevTop = curTop;
        hasPrev = true;
    }
}

void DrawTransferCurveLine(
    ICanvas& c,
    const Rect& plotRect,
    float minVal,
    float maxVal,
    const ChannelTransferParams& tp,
    Color curveColor,
    float strokeWidth
) noexcept {
    const float domainSpan = std::max(1e-5f, maxVal - minVal);
    const float xlo = plotRect.x + std::clamp((tp.Low() - minVal) / domainSpan, 0.0f, 1.0f) * plotRect.w;
    const float xhi = plotRect.x + std::clamp((tp.High() - minVal) / domainSpan, 0.0f, 1.0f) * plotRect.w;

    constexpr int kSegments = 24;
    Vec2 prevPt{};
    for (int i = 0; i <= kSegments; ++i) {
        const float u = static_cast<float>(i) / static_cast<float>(kSegments);
        const float val = tp.Low() + u * tp.window;
        const float outY = tp.Evaluate(val);
        const float sx = std::clamp(xlo + u * (xhi - xlo), plotRect.x, plotRect.Right());
        const float sy = plotRect.Bottom() - outY * plotRect.h;
        const Vec2 pt{sx, sy};
        if (i > 0) {
            c.DrawLine(prevPt, pt, curveColor, strokeWidth);
        }
        prevPt = pt;
    }
}

struct HistogramPayload {
    HistogramData data{};
    ChannelTransferParams transfer{};
    HistogramOptions opts{};
    HistogramResponse resp{};
    HistogramInternalState* state{nullptr};
};

} // namespace

Vec2 Histogram::Measure(
    const HistogramData& /*data*/,
    const HistogramOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) noexcept {
    const float w = (opts.width.has_value() && *opts.width > 0.0f) ? theme.Scale(*opts.width) : 240.0f * theme.Scale(1.0f);
    const float h = std::max(theme.Scale(opts.minHeight), theme.Scale(opts.height));
    const ElementSpec spec = MakeHistogramElementSpec(opts, size, theme, opts.disabled);
    return Element::Measure({w, h}, spec, canvas, theme, availableWidth);
}

HistogramResponse Histogram::Run(
    Context& ctx,
    const HistogramData& data,
    ChannelTransferParams& transfer,
    const HistogramOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiTheme& theme = ctx.GetTheme();
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const float scale = theme.Scale(1.0f);

    auto& st = ctx.GetOrCreateState<HistogramInternalState>(id);
    Response baseRes = ctx.Interact(id);

    HistogramResponse res{};
    static_cast<Response&>(res) = baseRes;
    res.activeChannel = opts.activeChannel;

    const ElementSpec elemSpec = MakeHistogramElementSpec(opts, activeSize, theme, effectiveDisabled);
    const Vec2 fallbackCtrl = {
        (opts.width.has_value() && *opts.width > 0.0f) ? theme.Scale(*opts.width) : 240.0f * scale,
        std::max(theme.Scale(opts.minHeight), theme.Scale(opts.height))
    };
    const ElementGeometry prevElem = Element::Resolve(baseRes.rect, fallbackCtrl, elemSpec, ctx.GetCanvas(), theme);
    const Rect ctrlRect = prevElem.controlRect.w > 4.0f ? prevElem.controlRect : baseRes.rect;
    const HistogramGeometry geom = ComputeHistogramGeometry(ctrlRect, data, opts, scale);

    const float minVal = data.minValue;
    const float maxVal = std::max(minVal + 1e-4f, data.maxValue);
    const float domainSpan = maxVal - minVal;

    const Vec2 mouse = ctx.GetMousePos();
    const KeyModifiers mods = ctx.GetKeyModifiers();

    // 1. Check interactive channel pills [RGB | R | G | B]
    bool clickedPill = false;
    if (!effectiveDisabled && geom.hasPills && baseRes.hovered && ctx.IsMouseClicked()) {
        for (size_t i = 0; i < 4; ++i) {
            if (geom.pillRects[i].Contains(mouse)) {
                const auto targetCh = static_cast<HistogramChannel>(i);
                if (targetCh != res.activeChannel) {
                    res.activeChannel = targetCh;
                    res.channelChanged = true;
                    res.changed = true;
                }
                clickedPill = true;
                ctx.SetActiveId(Id::Null());
                break;
            }
        }
    }

    // 2. Interactive W/L handle & band dragging on plotRect
    if (!effectiveDisabled && opts.interactive && !clickedPill && geom.plotRect.w > 8.0f) {
        const float xlo = geom.plotRect.x + std::clamp((transfer.Low() - minVal) / domainSpan, 0.0f, 1.0f) * geom.plotRect.w;
        const float xhi = geom.plotRect.x + std::clamp((transfer.High() - minVal) / domainSpan, 0.0f, 1.0f) * geom.plotRect.w;
        const float grabRadius = 7.0f * scale;

        const Rect expandedPlot(
            geom.plotRect.x - grabRadius,
            geom.plotRect.y - 4.0f * scale,
            geom.plotRect.w + grabRadius * 2.0f,
            geom.plotRect.h + 8.0f * scale
        );
        const bool inPlotOrBar = expandedPlot.Contains(mouse);

        if (baseRes.hovered && inPlotOrBar && ctx.IsMouseClicked()) {
            const double nowSec = ctx.GetCurrentTime();
            const float clickDist = std::hypot(mouse.x - st.lastClickPos.x, mouse.y - st.lastClickPos.y);
            if (baseRes.doubleClicked || ((nowSec - st.lastClickTimeSec) < 0.35 && clickDist < 8.0f * scale)) {
                // Double-click resets active channel window/level to full data range
                transfer.SetLowHigh(minVal, maxVal);
                res.windowLevelChanged = true;
                res.changed = true;
                st.dragMode = HistDragMode::None;
                st.lastClickTimeSec = -10.0;
            } else {
                st.lastClickTimeSec = nowSec;
                st.lastClickPos = mouse;

                const float distLo = std::abs(mouse.x - xlo);
                const float distHi = std::abs(mouse.x - xhi);
                if (distLo <= grabRadius && distLo <= distHi) {
                    st.dragMode = HistDragMode::DragLow;
                } else if (distHi <= grabRadius) {
                    st.dragMode = HistDragMode::DragHigh;
                } else {
                    st.dragMode = HistDragMode::DragLevel;
                }
                st.grabMouseX = mouse.x;
                st.grabLow = transfer.Low();
                st.grabHigh = transfer.High();
                st.grabLevel = transfer.level;
                st.grabWindow = transfer.window;
            }
        }

        if (st.dragMode != HistDragMode::None) {
            if (ctx.IsMouseDown()) {
                ctx.SetActiveId(id);
                const float fineFactor = mods.shift ? 0.2f : 1.0f;
                const float dxNorm = ((mouse.x - st.grabMouseX) / std::max(1.0f, geom.plotRect.w)) * fineFactor;
                const float dVal = dxNorm * domainSpan;
                const float minWin = std::max(1e-4f, domainSpan * 0.002f);

                if (st.dragMode == HistDragMode::DragLow) {
                    const float newLo = std::clamp(st.grabLow + dVal, minVal, st.grabHigh - minWin);
                    transfer.SetLowHigh(newLo, st.grabHigh, minWin);
                } else if (st.dragMode == HistDragMode::DragHigh) {
                    const float newHi = std::clamp(st.grabHigh + dVal, st.grabLow + minWin, maxVal);
                    transfer.SetLowHigh(st.grabLow, newHi, minWin);
                } else if (st.dragMode == HistDragMode::DragLevel) {
                    const float newLevel = std::clamp(st.grabLevel + dVal, minVal, maxVal);
                    transfer.level = newLevel;
                    transfer.window = std::max(minWin, st.grabWindow);
                }
                res.windowLevelChanged = true;
                res.changed = true;
                res.pressed = true;
            } else {
                st.dragMode = HistDragMode::None;
            }
        }
    } else if (!ctx.IsMouseDown()) {
        st.dragMode = HistDragMode::None;
    }

    // 3. Hover telemetry
    if (baseRes.hovered && geom.plotRect.Contains(mouse)) {
        const float u = std::clamp((mouse.x - geom.plotRect.x) / std::max(1.0f, geom.plotRect.w), 0.0f, 1.0f);
        res.hoveredValue = minVal + u * domainSpan;
        std::span<const uint32_t> activeBins = data.binsLum;
        if (data.channelCount >= 3) {
            if (res.activeChannel == HistogramChannel::Red && !data.binsR.empty()) activeBins = data.binsR;
            else if (res.activeChannel == HistogramChannel::Green && !data.binsG.empty()) activeBins = data.binsG;
            else if (res.activeChannel == HistogramChannel::Blue && !data.binsB.empty()) activeBins = data.binsB;
        }
        if (!activeBins.empty()) {
            const size_t idx = std::min(activeBins.size() - 1, static_cast<size_t>(u * static_cast<float>(activeBins.size())));
            res.hoveredBin = static_cast<int>(idx);
            res.hoveredCount = activeBins[idx];
        }
    }

    res.window = transfer.window;
    res.level = transfer.level;
    res.lowValue = transfer.Low();
    res.highValue = transfer.High();

    if (baseRes.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, baseRes.rect);
    }

    // Create Node & Payload
    Node* node = ctx.CreateNode(NodeType::Histogram, id);
    auto* payload = ctx.AllocPayload<HistogramPayload>();
    payload->data = data;
    payload->transfer = transfer;
    payload->opts = opts;
    payload->opts.activeChannel = res.activeChannel;
    payload->opts.label = ctx.AllocText(opts.label);
    payload->opts.description = ctx.AllocText(opts.description);
    payload->opts.badge = ctx.AllocText(opts.badge);
    payload->resp = res;
    payload->state = &st;
    node->SetPayload(payload);

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = baseRes.hovered;
    node->active = res.pressed;
    node->focused = baseRes.focused;

    if (opts.width.has_value() && *opts.width > 0.0f) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = theme.Scale(*opts.width);
    } else {
        node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    }
    node->heightMode = opts.heightMode;
    node->minHeight = std::max(theme.Scale(opts.minHeight), theme.Scale(opts.height));

    // Visual hash for damage tracking
    uint64_t binsFp = data.binsLum.size();
    auto mixBins = [&](std::span<const uint32_t> b) {
        if (b.empty()) return;
        const size_t step = std::max<size_t>(1, b.size() / 16);
        for (size_t i = 0; i < b.size(); i += step) {
            binsFp = detail::HashArgs(binsFp, b[i]);
        }
    };
    mixBins(data.binsLum);
    if (data.channelCount >= 3) {
        mixBins(data.binsR);
        mixBins(data.binsG);
        mixBins(data.binsB);
    }

    node->visualHash = detail::HashArgs(
        node->type,
        binsFp,
        data.channelCount,
        data.minValue,
        data.maxValue,
        transfer.window,
        transfer.level,
        transfer.gamma,
        transfer.invert,
        static_cast<uint8_t>(opts.scaleMode),
        static_cast<uint8_t>(res.activeChannel),
        static_cast<uint8_t>(opts.colormap),
        opts.showColorbar,
        opts.showTransferCurve,
        opts.showChannelPills,
        opts.showRangeReadout,
        res.hoveredBin,
        static_cast<uint8_t>(st.dragMode),
        activeSize,
        effectiveDisabled
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* c, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<HistogramPayload>();
        if (!p) return {0.0f, 0.0f};
        return Histogram::Measure(p->data, p->opts, n->uiSize, c, th, availableWidth);
    };

    node->renderFn = [](const Node* n, ICanvas& c, const UiTheme& th) noexcept {
        const auto* p = n->As<HistogramPayload>();
        if (!p) return;

        const HistogramData& d = p->data;
        const ChannelTransferParams& tp = p->transfer;
        const HistogramOptions& o = p->opts;
        const HistogramResponse& resp = p->resp;
        const float scale = th.Scale(1.0f);

        const ElementSpec spec = MakeHistogramElementSpec(o, n->uiSize, th, n->disabled);
        const Vec2 ctrlSize = {n->computedRect.w, n->computedRect.h};
        const ElementGeometry elemGeom = Element::Resolve(n->computedRect, ctrlSize, spec, &c, th);
        Element::RenderText(c, elemGeom, spec, th);

        const Rect box = elemGeom.controlRect;
        if (box.w <= 8.0f || box.h <= 8.0f) return;

        const bool isDark = (th.mode == ThemeMode::Dark);
        const float rad = o.borderRadius.has_value() ? th.Scale(*o.borderRadius) : th.GetRadius(n->uiSize);
        const Color bgSurface = isDark ? Color(15, 20, 30, 255) : Color(245, 248, 252, 255);
        const Color plotBg    = isDark ? Color(10, 14, 22, 255) : Color(255, 255, 255, 255);
        const Color borderCol = th.palette.borderSubtle;
        const Color gridCol   = isDark ? Color(51, 65, 85, 70) : Color(203, 213, 225, 110);

        c.DrawRoundRect(box, rad, bgSurface, borderCol, 1.0f);

        const HistogramGeometry g = ComputeHistogramGeometry(box, d, o, scale);
        const float minVal = d.minValue;
        const float maxVal = std::max(minVal + 1e-4f, d.maxValue);
        const float domainSpan = maxVal - minVal;
        const float smallFs = th.GetFontSize(TextRole::LabelSmall) * 0.86f;

        // 1. Header bar: Range Readout & Multi-Channel Pills [RGB | R | G | B]
        if (g.hasHeader) {
            if (o.showRangeReadout) {
                char loBuf[24]{};
                char hiBuf[24]{};
                FormatHistValue(loBuf, sizeof(loBuf), tp.Low(), domainSpan);
                FormatHistValue(hiBuf, sizeof(hiBuf), tp.High(), domainSpan);
                char rangeBuf[64]{};
                std::snprintf(rangeBuf, sizeof(rangeBuf), "[%s .. %s]", loBuf, hiBuf);
                const float maxLabelW = g.hasPills
                    ? std::max(20.0f, g.pillRects[0].x - g.headerRect.x - 4.0f * scale)
                    : g.headerRect.w;
                c.DrawTextBox(
                    rangeBuf,
                    Rect(g.headerRect.x, g.headerRect.y, maxLabelW, g.headerRect.h),
                    smallFs,
                    th.palette.textSecondary,
                    0.0f,
                    0.5f
                );
            }

            if (g.hasPills) {
                static constexpr std::array<std::string_view, 4> kPillLabels = {"RGB", "R", "G", "B"};
                for (size_t i = 0; i < 4; ++i) {
                    const auto ch = static_cast<HistogramChannel>(i);
                    const bool isSelected = (o.activeChannel == ch);
                    const Color chAccent = GetChannelAccentColor(ch, isDark);
                    const Color pillFill = isSelected
                        ? chAccent.WithAlpha(isDark ? 65 : 45)
                        : (isDark ? Color(24, 32, 46, 200) : Color(230, 236, 245, 220));
                    const Color pillStroke = isSelected ? chAccent : borderCol;
                    const Color pillText = isSelected ? (isDark ? Color(248, 250, 252, 255) : chAccent) : th.palette.textMuted;

                    c.DrawRoundRect(g.pillRects[i], 3.5f * scale, pillFill, pillStroke, 1.0f);
                    c.DrawTextBox(kPillLabels[i], g.pillRects[i], smallFs * 0.92f, pillText, 0.5f, 0.5f);
                }
            }
        }

        // 2. Main Histogram Plot Area
        const Rect pr = g.plotRect;
        c.DrawRoundRect(pr, 3.0f * scale, plotBg, borderCol, 1.0f);
        c.PushClip(pr.Inset(1.0f));

        // Subtle 25%, 50%, 75% grid lines
        for (int k = 1; k <= 3; ++k) {
            const float u = static_cast<float>(k) * 0.25f;
            const float gx = pr.x + u * pr.w;
            const float gy = pr.y + u * pr.h;
            c.DrawLine({gx, pr.y}, {gx, pr.Bottom()}, gridCol, 1.0f);
            c.DrawLine({pr.x, gy}, {pr.Right(), gy}, gridCol, 1.0f);
        }

        // Render Single-Channel or Multi-Channel Bins
        if (d.channelCount < 3) {
            const uint32_t maxB = MaxBinOf(d.binsLum);
            const Color barFill = isDark ? Color(120, 165, 225, 165) : Color(70, 130, 210, 165);
            const Color barStroke = isDark ? Color(165, 210, 255, 230) : Color(37, 99, 235, 230);
            DrawChannelDistribution(c, pr, d.binsLum, maxB, o.scaleMode, barFill, barStroke);
        } else {
            uint32_t maxAll = std::max({
                MaxBinOf(d.binsLum),
                MaxBinOf(d.binsR),
                MaxBinOf(d.binsG),
                MaxBinOf(d.binsB)
            });

            if (o.activeChannel == HistogramChannel::Master) {
                DrawChannelDistribution(c, pr, d.binsR, maxAll, o.scaleMode, Color(239, 68, 68, 70),  Color(248, 113, 113, 210));
                DrawChannelDistribution(c, pr, d.binsG, maxAll, o.scaleMode, Color(34, 197, 94, 70),  Color(74, 222, 128, 210));
                DrawChannelDistribution(c, pr, d.binsB, maxAll, o.scaleMode, Color(59, 130, 246, 70), Color(96, 165, 250, 210));
                if (!d.binsLum.empty()) {
                    DrawChannelDistribution(c, pr, d.binsLum, maxAll, o.scaleMode, Color::Transparent(),
                                            isDark ? Color(226, 232, 240, 190) : Color(51, 65, 85, 190));
                }
            } else {
                // Draw non-selected channels dimmed in background, active channel highlighted in foreground
                if (o.activeChannel != HistogramChannel::Red) {
                    DrawChannelDistribution(c, pr, d.binsR, maxAll, o.scaleMode, Color(239, 68, 68, 22), Color(239, 68, 68, 55));
                }
                if (o.activeChannel != HistogramChannel::Green) {
                    DrawChannelDistribution(c, pr, d.binsG, maxAll, o.scaleMode, Color(34, 197, 94, 22), Color(34, 197, 94, 55));
                }
                if (o.activeChannel != HistogramChannel::Blue) {
                    DrawChannelDistribution(c, pr, d.binsB, maxAll, o.scaleMode, Color(59, 130, 246, 22), Color(59, 130, 246, 55));
                }

                if (o.activeChannel == HistogramChannel::Red) {
                    DrawChannelDistribution(c, pr, d.binsR, MaxBinOf(d.binsR), o.scaleMode, Color(239, 68, 68, 135), Color(252, 129, 129, 245));
                } else if (o.activeChannel == HistogramChannel::Green) {
                    DrawChannelDistribution(c, pr, d.binsG, MaxBinOf(d.binsG), o.scaleMode, Color(34, 197, 94, 135), Color(74, 222, 128, 245));
                } else if (o.activeChannel == HistogramChannel::Blue) {
                    DrawChannelDistribution(c, pr, d.binsB, MaxBinOf(d.binsB), o.scaleMode, Color(59, 130, 246, 135), Color(118, 185, 255, 245));
                }
            }
        }

        // 3. Window / Level [xlo .. xhi] Overlay Band, Transfer Curve & Grab Handles
        const float xlo = pr.x + std::clamp((tp.Low() - minVal) / domainSpan, 0.0f, 1.0f) * pr.w;
        const float xhi = pr.x + std::clamp((tp.High() - minVal) / domainSpan, 0.0f, 1.0f) * pr.w;
        const float xmid = 0.5f * (xlo + xhi);
        const Color activeAccent = GetChannelAccentColor(o.activeChannel, isDark);

        if (xhi > xlo) {
            c.DrawRect(Rect(xlo, pr.y, xhi - xlo, pr.h), activeAccent.WithAlpha(isDark ? 38 : 28));
        }

        // Center level dashed/subtle vertical marker
        c.DrawLine({xmid, pr.y}, {xmid, pr.Bottom()}, activeAccent.WithAlpha(95), 1.0f);

        // Transfer curve(s)
        if (o.showTransferCurve) {
            if (d.channelCount >= 3 && o.activeChannel == HistogramChannel::Master && o.multiChannelParams.has_value()) {
                const auto& mcp = *o.multiChannelParams;
                DrawTransferCurveLine(c, pr, minVal, maxVal, mcp[1], Color(248, 113, 113, 145), 1.15f);
                DrawTransferCurveLine(c, pr, minVal, maxVal, mcp[2], Color(74, 222, 128, 145), 1.15f);
                DrawTransferCurveLine(c, pr, minVal, maxVal, mcp[3], Color(96, 165, 250, 145), 1.15f);
            }
            const Color curveCol = isDark ? Color(241, 245, 249, 215) : activeAccent.WithAlpha(220);
            DrawTransferCurveLine(c, pr, minVal, maxVal, tp, curveCol, 1.5f);
        }

        // Left & Right boundary lines + triangular grab handles
        const Color lineCol = activeAccent;
        c.DrawLine({xlo, pr.y}, {xlo, pr.Bottom()}, lineCol, 2.0f);
        c.DrawLine({xhi, pr.y}, {xhi, pr.Bottom()}, lineCol, 2.0f);

        const float triW = 5.0f * scale;
        const float triH = 7.0f * scale;
        c.DrawTriangle({xlo - triW, pr.y}, {xlo + triW, pr.y}, {xlo, pr.y + triH}, lineCol);
        c.DrawTriangle({xhi - triW, pr.y}, {xhi + triW, pr.y}, {xhi, pr.y + triH}, lineCol);

        c.PopClip();

        // 4. Bottom Colorbar / Channel Gradient Strip
        if (g.hasColorbar) {
            if (d.channelCount < 3 || o.activeChannel == HistogramChannel::Master) {
                DrawColormapBar(c, g.colorbarRect, o.colormap, false, tp.invert, 2.0f * scale, borderCol);
            } else {
                std::array<uint32_t, 256> chLut{};
                for (size_t i = 0; i < 256; ++i) {
                    const uint8_t v = static_cast<uint8_t>(tp.invert ? (255 - i) : i);
                    if (o.activeChannel == HistogramChannel::Red) {
                        chLut[i] = PackAbgr(Color(v, static_cast<uint8_t>(v / 8), static_cast<uint8_t>(v / 8), 255));
                    } else if (o.activeChannel == HistogramChannel::Green) {
                        chLut[i] = PackAbgr(Color(static_cast<uint8_t>(v / 8), v, static_cast<uint8_t>(v / 8), 255));
                    } else {
                        chLut[i] = PackAbgr(Color(static_cast<uint8_t>(v / 8), static_cast<uint8_t>(v / 6), v, 255));
                    }
                }
                c.DrawPixels(chLut.data(), 256, 1, g.colorbarRect, 2.0f * scale, 255);
                c.DrawRoundRect(g.colorbarRect, 2.0f * scale, Color::Transparent(), borderCol, 1.0f);
            }
        }

        // 5. Hovered bin readout pill
        if (resp.hoveredBin >= 0 && pr.w > 80.0f) {
            char hovBuf[48]{};
            char valStr[24]{};
            FormatHistValue(valStr, sizeof(valStr), resp.hoveredValue, domainSpan);
            std::snprintf(hovBuf, sizeof(hovBuf), "v:%s (%u)", valStr, resp.hoveredCount);
            const Vec2 tw = c.MeasureText(hovBuf, smallFs * 0.88f);
            const Rect badgeR(
                pr.Right() - tw.x - 10.0f * scale,
                pr.y + 4.0f * scale,
                tw.x + 6.0f * scale,
                14.0f * scale
            );
            c.DrawRoundRect(badgeR, 3.0f * scale, isDark ? Color(15, 23, 42, 210) : Color(255, 255, 255, 225), borderCol, 1.0f);
            c.DrawTextBox(hovBuf, badgeR, smallFs * 0.88f, th.palette.textPrimary, 0.5f, 0.5f);
        }
    };

    ctx.AdaptChildToContainer(node);
    return res;
}

} // namespace guikit
