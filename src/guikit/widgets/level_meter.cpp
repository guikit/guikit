#include "guikit/widgets/level_meter.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/format.h"
#include "keel/math.h"
#include "keel/static_array.h"

namespace guikit {

namespace {

constexpr keel::usize kMaxLevelMeterChannels = 8;
constexpr double kPeakHoldDurationSec = 0.80;

struct LevelMeterPersistState {
    keel::StaticArray<float, kMaxLevelMeterChannels> peakRatio{};
    keel::StaticArray<double, kMaxLevelMeterChannels> lastPeakUpdateTimeSec{};
    keel::StaticArray<bool, kMaxLevelMeterChannels> clipped{};
    double prevFrameTimeSec{-1.0};
    bool initialized{false};
};

struct LevelMeterChannelData {
    double value{0.0};
    float ratio{0.0f};
    float peakRatio{0.0f};
    bool clipped{false};
    keel::StringRef label{};
    keel::StringRef formattedValue{};
};

struct LevelMeterData {
    LevelMeterOptions opts;
    double min{-60.0};
    double max{0.0};
    keel::usize channelCount{1};
    keel::StaticArray<LevelMeterChannelData, kMaxLevelMeterChannels> channels{};
};

void FormatMeterValue(
    char* valBuf,
    keel::usize bufSize,
    double value,
    keel::StringRef format
) noexcept {
    keel::format_buf(valBuf, bufSize, format.empty() ? "{:.1f} dB" : format, value);
}

void FormatScaleTickLabel(
    char* buf,
    keel::usize bufSize,
    double val,
    double span
) noexcept {
    if (keel::abs(val) < 1e-4) {
        val = 0.0;
    }
    const keel::StringRef fmt = (span >= 10.0 && keel::abs(val - keel::round(val)) < 0.05) ? "{:.0f}" : "{:.1f}";
    keel::format_buf(buf, bufSize, fmt, val);
}

float GetDefaultBarThickness(UiSize size, const UiTheme& theme) noexcept {
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs: return 8.0f;
        case UiSize::Preset::Sm: return 10.0f;
        case UiSize::Preset::Lg: return 16.0f;
        case UiSize::Preset::Xl: return 20.0f;
        case UiSize::Preset::Md: return 12.0f;
        default:                 return keel::round(12.0f * theme.SizeFactor(size));
    }
}

[[nodiscard]] constexpr bool IsVerticalMeter(Orientation orientation) noexcept {
    return orientation == Orientation::Vertical;
}

ControlSpec EffectiveLevelMeterControl(
    const LevelMeterOptions& opts,
    UiSize size,
    const UiTheme& theme
) noexcept {
    const SliderMetrics& m = theme.GetSlider(size);
    const bool isVert = IsVerticalMeter(opts.orientation);
    return Control::FromOptions(
        opts,
        isVert ? SizeMode::Fit : SizeMode::Stretch,
        isVert ? SizeMode::Stretch : SizeMode::Fit,
        ElementLayout::Stacked,
        m.labelGap,
        m.descGap,
        12.0f
    );
}

Color ResolveZoneColor(
    float normPos,
    const LevelMeterOptions& opts,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const auto& pal = theme.palette;
    const float warnT = keel::clamp(opts.warningRatio, 0.05f, 0.98f);
    const float dangerT = keel::clamp(opts.dangerRatio, warnT + 0.01f, 0.995f);

    Color col;
    const StateColor resolvedColor = theme.ResolveStateColor(opts.color);
    if (!opts.multiZoneColor) {
        const VariantColors& primary = theme.GetVariant(UiVariant::Primary);
        if (!opts.color.HasAny()) {
            col = primary.bg;
        } else if (resolvedColor.normal.has_value() && resolvedColor.normal->a == 0) {
            col = pal.borderHover;
        } else {
            col = resolvedColor.normal.value_or(primary.bg);
        }
    } else {
        if (normPos >= dangerT) {
            col = theme.GetVariant(UiVariant::Danger).bg;
        } else if (normPos >= warnT) {
            col = theme.GetVariant(UiVariant::Warning).bg;
        } else {
            const Color defaultSafe = theme.GetVariant(UiVariant::Success).bg;
            if (!opts.color.HasAny() || opts.color == StateColor::Primary()) {
                col = defaultSafe;
            } else if (resolvedColor.normal.has_value() && resolvedColor.normal->a == 0) {
                col = defaultSafe;
            } else {
                col = resolvedColor.normal.value_or(defaultSafe);
            }
        }
    }
    return disabled ? col.WithAlphaF(0.40f) : col;
}

struct LevelMeterMetrics {
    float barThick{12.0f};
    float channelGap{4.0f};
    float labelFont{11.0f};
    float labelReserveW{0.0f};
    float labelLineH{14.0f};
    float valFont{12.0f};
    float valReserveW{0.0f};
    float valLineH{15.0f};
    float scaleFont{10.0f};
    float scaleLineH{12.0f};
    float scaleTickLen{4.0f};
    float scaleRegionH{0.0f}; // Used in Horizontal mode
    float scaleRegionW{0.0f}; // Used in Vertical mode
    float clipLedLen{8.0f};
    float clipGap{3.0f};
    Vec2 intrinsicSize{};
};

LevelMeterMetrics ComputeLevelMeterMetrics(
    keel::usize channelCount,
    double min,
    double max,
    const LevelMeterOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const SliderMetrics& sm = theme.GetSlider(size);
    const keel::usize numCh = keel::clamp<keel::usize>(channelCount, 1, kMaxLevelMeterChannels);
    const double rangeMin = keel::min(min, max);
    const double rangeMax = keel::max(min, max);
    const double span = keel::max(1e-6, rangeMax - rangeMin);

    LevelMeterMetrics mm{};
    mm.barThick = opts.barThickness.has_value()
        ? keel::max(4.0f, *opts.barThickness)
        : GetDefaultBarThickness(size, theme);
    mm.channelGap = (numCh > 1) ? 4.0f : 0.0f;

    mm.labelFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
    mm.labelLineH = canvas ? canvas->GetLineHeight(mm.labelFont) : theme.GetLineHeight(mm.labelFont);

    if (!opts.channelLabels.empty()) {
        for (keel::usize i = 0; i < keel::min(numCh, opts.channelLabels.size()); ++i) {
            if (!opts.channelLabels[i].empty()) {
                mm.labelReserveW = keel::max(
                    mm.labelReserveW,
                    TextUtil::MeasureWidth(opts.channelLabels[i], mm.labelFont, canvas)
                );
            }
        }
        if (mm.labelReserveW > 0.0f) {
            mm.labelReserveW = keel::max(mm.labelReserveW + 2.0f, 16.0f);
        }
    }

    const keel::StringRef fmt = opts.valueFormat.text.empty() ? "%.1f dB" : opts.valueFormat.text;
    if (opts.showValue) {
        mm.valFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelMedium, size);
        mm.valLineH = canvas ? canvas->GetLineHeight(mm.valFont) : theme.GetLineHeight(mm.valFont);

        auto measureSample = [&](double v) {
            char buf[64];
            FormatMeterValue(buf, sizeof(buf), v, fmt);
            mm.valReserveW = keel::max(mm.valReserveW, TextUtil::MeasureWidth(keel::StringRef(buf), mm.valFont, canvas));
        };
        measureSample(rangeMin);
        measureSample(rangeMax);
        measureSample(rangeMin + span * 0.777);
        for (keel::StringRef var : opts.valueFormat.variants) {
            if (!var.empty()) {
                mm.valReserveW = keel::max(mm.valReserveW, TextUtil::MeasureWidth(var, mm.valFont, canvas));
            }
        }
        mm.valReserveW += 4.0f;
    }

    mm.scaleFont = keel::max(8.5f, theme.ResolveFontSize(TextRole::LabelSmall, size) * 0.90f);
    mm.scaleLineH = canvas ? canvas->GetLineHeight(mm.scaleFont) : theme.GetLineHeight(mm.scaleFont);
    mm.scaleTickLen = 4.0f;

    if (opts.showClipLed) {
        mm.clipLedLen = keel::clamp(mm.barThick * 0.62f, 6.0f, 12.0f);
        mm.clipGap = 3.0f;
    } else {
        mm.clipLedLen = 0.0f;
        mm.clipGap = 0.0f;
    }

    if (opts.orientation == Orientation::Horizontal) {
        if (opts.showScale) {
            mm.scaleRegionH = 3.0f + mm.scaleTickLen + 1.0f + mm.scaleLineH;
        }
        const float rowH = (numCh == 1 && opts.showValue)
            ? keel::max(mm.barThick, mm.valLineH)
            : (opts.showValue ? keel::max(mm.barThick, mm.valLineH * 0.85f) : mm.barThick);
        const float barsTotalH = static_cast<float>(numCh) * rowH + static_cast<float>(numCh - 1) * mm.channelGap;

        const float defaultTrackW = opts.width.IsFixed()
            ? opts.width.Dp()
            : sm.defaultWidth;
        const float leftW = (mm.labelReserveW > 0.0f) ? (mm.labelReserveW + 6.0f) : 0.0f;
        const float clipW = opts.showClipLed ? (mm.clipGap + mm.clipLedLen) : 0.0f;
        const float rightW = opts.showValue ? (8.0f + mm.valReserveW) : 0.0f;

        mm.intrinsicSize = {
            leftW + defaultTrackW + clipW + rightW,
            barsTotalH + mm.scaleRegionH
        };
    } else {
        // Vertical orientation
        if (opts.showScale) {
            float maxScaleLblW = 0.0f;
            for (keel::i32 i = 0; i <= 5; ++i) {
                const double t = static_cast<double>(i) / 5.0;
                char sbuf[32];
                FormatScaleTickLabel(sbuf, sizeof(sbuf), rangeMin + t * span, span);
                maxScaleLblW = keel::max(maxScaleLblW, TextUtil::MeasureWidth(keel::StringRef(sbuf), mm.scaleFont, canvas));
            }
            mm.scaleRegionW = maxScaleLblW + 4.0f + mm.scaleTickLen + 4.0f;
        }

        const float barsW = static_cast<float>(numCh) * mm.barThick + static_cast<float>(numCh - 1) * mm.channelGap;
        const float topClipH = opts.showClipLed ? (mm.clipLedLen + mm.clipGap) : 0.0f;
        const float bottomLabelH = (mm.labelReserveW > 0.0f) ? (4.0f + mm.labelLineH) : 0.0f;
        const float bottomValH = opts.showValue ? (4.0f + mm.valLineH) : 0.0f;
        const float defaultTrackH = keel::round(sm.defaultWidth);

        const float totalW = keel::max(mm.scaleRegionW + barsW, opts.showValue ? mm.valReserveW : 0.0f) + 8.0f;
        const float totalH = topClipH + defaultTrackH + bottomLabelH + bottomValH;
        mm.intrinsicSize = {totalW, totalH};
    }

    return mm;
}

void DrawChannelBarHorizontal(
    ICanvas& canvas,
    Rect trackRect,
    const LevelMeterChannelData& ch,
    const LevelMeterOptions& opts,
    const LevelMeterMetrics& mm,
    bool disabled,
    const UiTheme& theme
) noexcept {
    if (trackRect.w <= 1.0f || trackRect.h <= 1.0f) return;

    trackRect.x = keel::round(trackRect.x);
    trackRect.y = keel::round(trackRect.y);
    trackRect.w = keel::round(trackRect.w);
    trackRect.h = keel::max(2.0f, keel::round(trackRect.h));

    const auto& pal = theme.palette;
    const float warnT = keel::clamp(opts.warningRatio, 0.05f, 0.98f);
    const float dangerT = keel::clamp(opts.dangerRatio, warnT + 0.01f, 0.995f);

    keel::u32 segCount = 0;
    float gap = 2.0f;
    float avgSegW = 2.0f;
    if (opts.segments > 0) {
        segCount = keel::clamp<keel::u32>(opts.segments, 2u, 64u);
        gap = (segCount > 28) ? 1.0f : 2.0f;
        const float totalGaps = gap * static_cast<float>(segCount - 1);
        avgSegW = keel::max(1.0f, (trackRect.w - totalGaps) / static_cast<float>(segCount));
    }

    // Recessed housing backdrop
    const float housingPad = 2.0f;
    const Rect housingRect(
        trackRect.x - housingPad,
        trackRect.y - housingPad,
        trackRect.w + housingPad * 2.0f,
        trackRect.h + housingPad * 2.0f
    );
    const Color housingBg = disabled ? pal.surface.WithAlphaF(0.4f) : pal.surface;
    const Color housingBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
    canvas.DrawRoundRect(housingRect, keel::min(3.0f, housingRect.h * 0.22f), housingBg, housingBorder, 1.0f);

    if (opts.segments > 0) {
        keel::i32 peakSegIdx = -1;
        if (ch.peakRatio > 0.005f) {
            peakSegIdx = keel::clamp(
                keel::ceil_i32(ch.peakRatio * static_cast<float>(segCount)) - 1,
                0,
                static_cast<keel::i32>(segCount) - 1
            );
        }

        const bool useRound = (keel::min(avgSegW, trackRect.h) >= 8.0f);
        const float stepW = (trackRect.w + gap) / static_cast<float>(segCount);
        for (keel::u32 i = 0; i < segCount; ++i) {
            const float segLeft = keel::round(trackRect.x + static_cast<float>(i) * stepW);
            const float segRight = (i + 1 == segCount)
                ? trackRect.Right()
                : keel::round(trackRect.x + static_cast<float>(i + 1) * stepW - gap);
            const float segW = keel::max(1.0f, segRight - segLeft);
            const float segThreshold = static_cast<float>(i + 1) / static_cast<float>(segCount);
            const float segMidRatio = (static_cast<float>(i) + 0.5f) / static_cast<float>(segCount);
            const bool isLit = (ch.ratio >= segThreshold - (0.5f / static_cast<float>(segCount))) && (ch.ratio > 0.001f);
            const bool isPeakSeg = (static_cast<keel::i32>(i) == peakSegIdx);

            const Color zoneCol = ResolveZoneColor(segMidRatio, opts, disabled, theme);
            const Rect segRect(segLeft, trackRect.y, segW, trackRect.h);
            const Color fillCol = (isLit || isPeakSeg) ? zoneCol : zoneCol.WithAlphaF(disabled ? 0.08f : 0.16f);

            if (useRound) {
                canvas.DrawRoundRect(segRect, 1.0f, fillCol);
            } else {
                canvas.DrawRect(segRect, fillCol);
            }
        }
    } else {
        // Smooth continuous bar
        const float radius = keel::min(3.0f, trackRect.h * 0.3f);
        if (opts.multiZoneColor) {
            // Unlit 3-zone dim backdrop
            const float wSafe = warnT * trackRect.w;
            const float wWarn = (dangerT - warnT) * trackRect.w;
            const float wDang = (1.0f - dangerT) * trackRect.w;

            const Color cSafe = ResolveZoneColor(0.1f, opts, disabled, theme);
            const Color cWarn = ResolveZoneColor((warnT + dangerT) * 0.5f, opts, disabled, theme);
            const Color cDang = ResolveZoneColor(0.95f, opts, disabled, theme);

            canvas.DrawRect(Rect(trackRect.x, trackRect.y, wSafe, trackRect.h), cSafe.WithAlphaF(disabled ? 0.08f : 0.16f));
            canvas.DrawRect(Rect(trackRect.x + wSafe, trackRect.y, wWarn, trackRect.h), cWarn.WithAlphaF(disabled ? 0.08f : 0.16f));
            canvas.DrawRect(Rect(trackRect.x + wSafe + wWarn, trackRect.y, wDang, trackRect.h), cDang.WithAlphaF(disabled ? 0.08f : 0.16f));

            // Lit 3-zone segments
            if (ch.ratio > 0.001f) {
                const float rSafe = keel::min(ch.ratio, warnT);
                if (rSafe > 0.0f) {
                    canvas.DrawRect(Rect(trackRect.x, trackRect.y, rSafe * trackRect.w, trackRect.h), cSafe);
                }
                if (ch.ratio > warnT) {
                    const float rWarn = keel::min(ch.ratio, dangerT) - warnT;
                    canvas.DrawRect(Rect(trackRect.x + warnT * trackRect.w, trackRect.y, rWarn * trackRect.w, trackRect.h), cWarn);
                }
                if (ch.ratio > dangerT) {
                    const float rDang = ch.ratio - dangerT;
                    canvas.DrawRect(Rect(trackRect.x + dangerT * trackRect.w, trackRect.y, rDang * trackRect.w, trackRect.h), cDang);
                }
            }
        } else {
            const Color baseCol = ResolveZoneColor(ch.ratio, opts, disabled, theme);
            canvas.DrawRoundRect(trackRect, radius, baseCol.WithAlphaF(disabled ? 0.08f : 0.16f));
            if (ch.ratio > 0.001f) {
                canvas.DrawRoundRect(Rect(trackRect.x, trackRect.y, ch.ratio * trackRect.w, trackRect.h), radius, baseCol);
            }
        }

        // Peak-hold marker bar
        if (ch.peakRatio > 0.005f) {
            const float peakX = keel::clamp(
                trackRect.x + ch.peakRatio * trackRect.w - 1.25f,
                trackRect.x,
                keel::max(trackRect.x, trackRect.Right() - 2.5f)
            );
            const Color peakCol = ResolveZoneColor(ch.peakRatio, opts, disabled, theme);
            canvas.DrawRoundRect(Rect(peakX, trackRect.y - 1.0f, 2.5f, trackRect.h + 2.0f), 1.0f, peakCol);
        }
    }

    // Dedicated Clip LED cap at right end
    if (opts.showClipLed && mm.clipLedLen > 0.0f) {
        const Rect clipRect(
            keel::round(trackRect.Right() + keel::max(3.0f, keel::round(mm.clipGap))),
            trackRect.y,
            keel::round(mm.clipLedLen),
            trackRect.h
        );
        const bool isClip = ch.clipped || (ch.ratio >= 0.995f);
        const Color dangerBg = theme.GetVariant(UiVariant::Danger).bg;
        const Color clipFill = isClip
            ? (disabled ? dangerBg.WithAlphaF(0.45f) : dangerBg)
            : dangerBg.WithAlphaF(disabled ? 0.08f : 0.18f);
        const Color clipBorder = isClip
            ? (disabled ? dangerBg.WithAlphaF(0.4f) : dangerBg)
            : (disabled ? pal.border.WithAlphaF(0.25f) : pal.border.WithAlphaF(0.65f));
        canvas.DrawRoundRect(clipRect, keel::min(2.5f, clipRect.w * 0.3f), clipFill, clipBorder, 1.0f);
    }
}

void DrawChannelBarVertical(
    ICanvas& canvas,
    Rect trackRect,
    const LevelMeterChannelData& ch,
    const LevelMeterOptions& opts,
    const LevelMeterMetrics& mm,
    bool disabled,
    const UiTheme& theme
) noexcept {
    if (trackRect.w <= 1.0f || trackRect.h <= 1.0f) return;

    trackRect.x = keel::round(trackRect.x);
    trackRect.y = keel::round(trackRect.y);
    trackRect.w = keel::max(2.0f, keel::round(trackRect.w));
    trackRect.h = keel::round(trackRect.h);

    const auto& pal = theme.palette;
    const float warnT = keel::clamp(opts.warningRatio, 0.05f, 0.98f);
    const float dangerT = keel::clamp(opts.dangerRatio, warnT + 0.01f, 0.995f);

    keel::u32 segCount = 0;
    float gap = 2.0f;
    float avgSegH = 2.0f;
    if (opts.segments > 0) {
        segCount = keel::clamp<keel::u32>(opts.segments, 2u, 64u);
        gap = (segCount > 28) ? 1.0f : 2.0f;
        const float totalGaps = gap * static_cast<float>(segCount - 1);
        avgSegH = keel::max(1.0f, (trackRect.h - totalGaps) / static_cast<float>(segCount));
    }

    // Recessed housing backdrop
    const float housingPad = 2.0f;
    const Rect housingRect(
        trackRect.x - housingPad,
        trackRect.y - housingPad,
        trackRect.w + housingPad * 2.0f,
        trackRect.h + housingPad * 2.0f
    );
    const Color housingBg = disabled ? pal.surface.WithAlphaF(0.4f) : pal.surface;
    const Color housingBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
    canvas.DrawRoundRect(housingRect, keel::min(3.0f, housingRect.w * 0.22f), housingBg, housingBorder, 1.0f);

    if (opts.segments > 0) {
        keel::i32 peakSegIdx = -1;
        if (ch.peakRatio > 0.005f) {
            peakSegIdx = keel::clamp(
                keel::ceil_i32(ch.peakRatio * static_cast<float>(segCount)) - 1,
                0,
                static_cast<keel::i32>(segCount) - 1
            );
        }

        const bool useRound = (keel::min(trackRect.w, avgSegH) >= 8.0f);
        const float stepH = (trackRect.h + gap) / static_cast<float>(segCount);
        for (keel::u32 i = 0; i < segCount; ++i) {
            // Segment 0 is at the bottom, segment segCount-1 is at the top (all integer pixel coordinates)
            const float segBottom = keel::round(trackRect.Bottom() - static_cast<float>(i) * stepH);
            const float segTop = (i + 1 == segCount)
                ? trackRect.y
                : keel::round(trackRect.Bottom() - (static_cast<float>(i + 1) * stepH - gap));
            const float segH = keel::max(1.0f, segBottom - segTop);
            const float segThreshold = static_cast<float>(i + 1) / static_cast<float>(segCount);
            const float segMidRatio = (static_cast<float>(i) + 0.5f) / static_cast<float>(segCount);
            const bool isLit = (ch.ratio >= segThreshold - (0.5f / static_cast<float>(segCount))) && (ch.ratio > 0.001f);
            const bool isPeakSeg = (static_cast<keel::i32>(i) == peakSegIdx);

            const Color zoneCol = ResolveZoneColor(segMidRatio, opts, disabled, theme);
            const Rect segRect(trackRect.x, segTop, trackRect.w, segH);
            const Color fillCol = (isLit || isPeakSeg) ? zoneCol : zoneCol.WithAlphaF(disabled ? 0.08f : 0.16f);

            if (useRound) {
                canvas.DrawRoundRect(segRect, 1.0f, fillCol);
            } else {
                canvas.DrawRect(segRect, fillCol);
            }
        }
    } else {
        // Smooth continuous vertical bar (fills bottom-to-top)
        const float radius = keel::min(3.0f, trackRect.w * 0.3f);
        if (opts.multiZoneColor) {
            const float hSafe = warnT * trackRect.h;
            const float hWarn = (dangerT - warnT) * trackRect.h;
            const float hDang = (1.0f - dangerT) * trackRect.h;

            const Color cSafe = ResolveZoneColor(0.1f, opts, disabled, theme);
            const Color cWarn = ResolveZoneColor((warnT + dangerT) * 0.5f, opts, disabled, theme);
            const Color cDang = ResolveZoneColor(0.95f, opts, disabled, theme);

            canvas.DrawRect(Rect(trackRect.x, trackRect.Bottom() - hSafe, trackRect.w, hSafe), cSafe.WithAlphaF(disabled ? 0.08f : 0.16f));
            canvas.DrawRect(Rect(trackRect.x, trackRect.Bottom() - hSafe - hWarn, trackRect.w, hWarn), cWarn.WithAlphaF(disabled ? 0.08f : 0.16f));
            canvas.DrawRect(Rect(trackRect.x, trackRect.y, trackRect.w, hDang), cDang.WithAlphaF(disabled ? 0.08f : 0.16f));

            if (ch.ratio > 0.001f) {
                const float rSafe = keel::min(ch.ratio, warnT);
                if (rSafe > 0.0f) {
                    const float hs = rSafe * trackRect.h;
                    canvas.DrawRect(Rect(trackRect.x, trackRect.Bottom() - hs, trackRect.w, hs), cSafe);
                }
                if (ch.ratio > warnT) {
                    const float rWarn = keel::min(ch.ratio, dangerT) - warnT;
                    const float hw = rWarn * trackRect.h;
                    canvas.DrawRect(Rect(trackRect.x, trackRect.Bottom() - warnT * trackRect.h - hw, trackRect.w, hw), cWarn);
                }
                if (ch.ratio > dangerT) {
                    const float rDang = ch.ratio - dangerT;
                    const float hd = rDang * trackRect.h;
                    canvas.DrawRect(Rect(trackRect.x, trackRect.Bottom() - dangerT * trackRect.h - hd, trackRect.w, hd), cDang);
                }
            }
        } else {
            const Color baseCol = ResolveZoneColor(ch.ratio, opts, disabled, theme);
            canvas.DrawRoundRect(trackRect, radius, baseCol.WithAlphaF(disabled ? 0.08f : 0.16f));
            if (ch.ratio > 0.001f) {
                const float fillH = ch.ratio * trackRect.h;
                canvas.DrawRoundRect(Rect(trackRect.x, trackRect.Bottom() - fillH, trackRect.w, fillH), radius, baseCol);
            }
        }

        // Peak-hold marker bar
        if (ch.peakRatio > 0.005f) {
            const float peakY = keel::clamp(
                trackRect.Bottom() - ch.peakRatio * trackRect.h - 1.25f,
                trackRect.y,
                keel::max(trackRect.y, trackRect.Bottom() - 2.5f)
            );
            const Color peakCol = ResolveZoneColor(ch.peakRatio, opts, disabled, theme);
            canvas.DrawRoundRect(Rect(trackRect.x - 1.0f, peakY, trackRect.w + 2.0f, 2.5f), 1.0f, peakCol);
        }
    }

    // Dedicated Clip LED cap at top end
    if (opts.showClipLed && mm.clipLedLen > 0.0f) {
        const float clipH = keel::round(mm.clipLedLen);
        const float clipGap = keel::max(3.0f, keel::round(mm.clipGap));
        const Rect clipRect(
            trackRect.x,
            keel::round(trackRect.y - clipGap - clipH),
            trackRect.w,
            clipH
        );
        const bool isClip = ch.clipped || (ch.ratio >= 0.995f);
        const Color dangerBg = theme.GetVariant(UiVariant::Danger).bg;
        const Color clipFill = isClip
            ? (disabled ? dangerBg.WithAlphaF(0.45f) : dangerBg)
            : dangerBg.WithAlphaF(disabled ? 0.08f : 0.18f);
        const Color clipBorder = isClip
            ? (disabled ? dangerBg.WithAlphaF(0.4f) : dangerBg)
            : (disabled ? pal.border.WithAlphaF(0.25f) : pal.border.WithAlphaF(0.65f));
        canvas.DrawRoundRect(clipRect, keel::min(2.5f, clipRect.h * 0.3f), clipFill, clipBorder, 1.0f);
    }
}

Response RunLevelMeterImpl(
    Context& ctx,
    const double* values,
    keel::usize channelCount,
    double min,
    double max,
    const LevelMeterOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme& activeTheme = ctx.GetTheme();

    LevelMeterOptions optsCopy = opts;

    const double rangeMin = keel::min(min, max);
    const double rangeMax = keel::max(min, max);
    const double span = keel::max(1e-9, rangeMax - rangeMin);
    const keel::usize numCh = keel::clamp<keel::usize>(channelCount, 1, kMaxLevelMeterChannels);

    auto& state = ctx.GetOrCreatePersistentState<LevelMeterPersistState>(id);
    const double nowSec = ctx.GetCurrentTime();
    const float dt = (state.prevFrameTimeSec >= 0.0 && nowSec > state.prevFrameTimeSec)
        ? static_cast<float>(keel::min(0.25, nowSec - state.prevFrameTimeSec))
        : (1.0f / 60.0f);
    state.prevFrameTimeSec = nowSec;

    Response res = ctx.Interact(id);
    Control::ForwardParentElementInteraction(ctx, effectiveDisabled, res);

    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
    } else if (res.hovered && !optsCopy.tooltip.empty()) {
        ctx.SetTooltip(optsCopy.tooltip, ctx.GetPrevFrameBounds(id));
    }

    auto* data = ctx.AllocPayload<LevelMeterData>();
    data->opts = optsCopy;
    data->opts.tooltip = ctx.AllocText(optsCopy.tooltip);
    data->opts.valueFormat = ctx.AllocText(optsCopy.valueFormat);
    data->min = rangeMin;
    data->max = rangeMax;
    data->channelCount = numCh;

    const keel::StringRef fmt = optsCopy.valueFormat.text.empty() ? "%.1f dB" : optsCopy.valueFormat.text;
    bool needsDecayRepaint = false;

    for (keel::usize ch = 0; ch < numCh; ++ch) {
        const double rawVal = (values != nullptr) ? values[ch] : rangeMin;
        const double clampedVal = keel::clamp(rawVal, rangeMin, rangeMax);
        const float ratio = static_cast<float>(keel::clamp((clampedVal - rangeMin) / span, 0.0, 1.0));

        if (!state.initialized) {
            state.peakRatio[ch] = ratio;
            state.lastPeakUpdateTimeSec[ch] = nowSec;
        }

        if (ratio >= 0.995f) {
            state.clipped[ch] = true;
        }

        if (optsCopy.peakHoldValue.has_value()) {
            state.peakRatio[ch] = static_cast<float>(keel::clamp((*optsCopy.peakHoldValue - rangeMin) / span, 0.0, 1.0));
        } else if (optsCopy.autoPeakHold) {
            if (ratio >= state.peakRatio[ch]) {
                state.peakRatio[ch] = ratio;
                state.lastPeakUpdateTimeSec[ch] = nowSec;
            } else if ((nowSec - state.lastPeakUpdateTimeSec[ch]) > kPeakHoldDurationSec) {
                const float decayRate = keel::max(0.01f, optsCopy.peakHoldDecayPerSec);
                state.peakRatio[ch] = keel::max(ratio, state.peakRatio[ch] - decayRate * dt);
                if (state.peakRatio[ch] > ratio + 0.001f) {
                    needsDecayRepaint = true;
                }
            } else if (state.peakRatio[ch] > ratio + 0.001f) {
                needsDecayRepaint = true;
            }
        } else {
            state.peakRatio[ch] = 0.0f;
        }

        if (!effectiveDisabled && res.clicked) {
            state.peakRatio[ch] = ratio;
            state.lastPeakUpdateTimeSec[ch] = nowSec;
            state.clipped[ch] = (ratio >= 0.995f);
        }

        char valBuf[64]{};
        FormatMeterValue(valBuf, sizeof(valBuf), clampedVal, fmt);

        LevelMeterChannelData& cd = data->channels[ch];
        cd.value = clampedVal;
        cd.ratio = ratio;
        cd.peakRatio = state.peakRatio[ch];
        cd.clipped = state.clipped[ch];
        cd.label = (ch < optsCopy.channelLabels.size() && !optsCopy.channelLabels[ch].empty())
            ? ctx.AllocString(optsCopy.channelLabels[ch])
            : keel::StringRef{};
        cd.formattedValue = ctx.AllocString(valBuf);
    }
    state.initialized = true;

    if (!effectiveDisabled && res.clicked) {
        res.changed = true;
        ctx.RequestRepaint(3);
    } else if (needsDecayRepaint) {
        ctx.RequestRepaint(2);
    }

    const ControlSpec ctrl = EffectiveLevelMeterControl(optsCopy, activeSize, activeTheme);
    Node* node = ctx.CreateNode(NodeType::LevelMeter, id);
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->text = data->channels[0].formattedValue;
    node->SetPayload(data);

    keel::u64 chHash = 0;
    for (keel::usize ch = 0; ch < numCh; ++ch) {
        const auto& cd = data->channels[ch];
        chHash = detail::HashArgs(
            chHash,
            cd.ratio,
            cd.peakRatio,
            cd.clipped,
            cd.label,
            cd.formattedValue
        );
    }

    node->visualHash = detail::HashArgs(
        chHash,
        data->min,
        data->max,
        data->channelCount,
        static_cast<keel::u8>(data->opts.orientation),
        data->opts.segments,
        data->opts.multiZoneColor,
        data->opts.warningRatio,
        data->opts.dangerRatio,
        data->opts.autoPeakHold,
        data->opts.showClipLed,
        data->opts.showScale,
        data->opts.showValue,
        data->opts.barThickness.value_or(0.0f),
        data->opts.color,
        data->opts.width,
        data->opts.height,
        data->opts.minWidth.value_or(0.0f),
        data->opts.maxWidth.value_or(0.0f),
        data->opts.minHeight.value_or(0.0f),
        data->opts.maxHeight.value_or(0.0f),
        data->opts.valueFormat
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<LevelMeterData>();
        if (!d) {
            return LevelMeter::Measure(1, -60.0, 0.0, LevelMeterOptions{}, n->uiSize, canvas, theme, availableWidth);
        }
        return LevelMeter::Measure(
            d->channelCount,
            d->min,
            d->max,
            d->opts,
            n->uiSize,
            canvas,
            theme,
            availableWidth
        );
    };

    node->renderFn = &LevelMeter::Render;

    Control::SetupNode(ctx, node, ctrl);
    return res;
}

} // namespace

Response LevelMeter::Run(
    Context& ctx,
    double value,
    double min,
    double max,
    const LevelMeterOptions& opts,
    keel::SourceLocation loc
) {
    return RunLevelMeterImpl(ctx, &value, 1, min, max, opts, loc);
}

Response LevelMeter::Run(
    Context& ctx,
    keel::ArrayRef<float> channelValues,
    float min,
    float max,
    const LevelMeterOptions& opts,
    keel::SourceLocation loc
) {
    keel::StaticArray<double, kMaxLevelMeterChannels> vals{};
    const keel::usize count = channelValues.empty()
        ? 1
        : keel::min(channelValues.size(), kMaxLevelMeterChannels);
    if (channelValues.empty()) {
        vals[0] = static_cast<double>(min);
    } else {
        for (keel::usize i = 0; i < count; ++i) {
            vals[i] = static_cast<double>(channelValues[i]);
        }
    }
    return RunLevelMeterImpl(ctx, vals.data(), count, static_cast<double>(min), static_cast<double>(max), opts, loc);
}

Vec2 LevelMeter::Measure(
    keel::usize channelCount,
    double min,
    double max,
    const LevelMeterOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const ControlSpec ctrl = EffectiveLevelMeterControl(opts, size, theme);
    const LevelMeterMetrics mm = ComputeLevelMeterMetrics(channelCount, min, max, opts, size, canvas, theme);
    return Control::Measure(mm.intrinsicSize, ctrl, theme);
}

void LevelMeter::Render(const Node* node, ICanvas& canvas, const UiTheme& theme) {
    if (!node) return;
    const auto* d = node->As<LevelMeterData>();
    if (!d) return;

    const Rect bounds = node->computedRect;
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const LevelMeterOptions& opts = d->opts;
    const UiSize size = node->uiSize;
    const bool disabled = node->disabled;
    const keel::usize numCh = keel::clamp<keel::usize>(d->channelCount, 1, kMaxLevelMeterChannels);

    const Rect ctrlRect = bounds;
    if (ctrlRect.w <= 2.0f || ctrlRect.h <= 2.0f) return;

    const auto& pal = theme.palette;
    const LevelMeterMetrics mm = ComputeLevelMeterMetrics(numCh, d->min, d->max, opts, size, &canvas, theme);
    const double span = keel::max(1e-6, d->max - d->min);

    const Color labelCol = disabled ? pal.textSecondary.WithAlphaF(0.4f) : pal.textSecondary;
    const Color valCol = theme.ResolveTextColor(opts.valueFormat, UiVariant::Secondary, disabled);
    const Color scaleTickCol = disabled ? pal.border.WithAlphaF(0.35f) : pal.borderHover;
    const Color scaleTextCol = disabled ? pal.textMuted.WithAlphaF(0.35f) : pal.textMuted;

    if (opts.orientation == Orientation::Horizontal) {
        float leftW = (mm.labelReserveW > 0.0f) ? (mm.labelReserveW + 6.0f) : 0.0f;
        const float clipTotalW = opts.showClipLed ? (mm.clipGap + mm.clipLedLen) : 0.0f;
        float rightW = opts.showValue ? (8.0f + mm.valReserveW) : 0.0f;
        const bool showRightValue = opts.showValue && (ctrlRect.w >= leftW + clipTotalW + rightW + 42.0f);
        if (!showRightValue) {
            rightW = 0.0f;
        }
        const bool showLeftLabel = (mm.labelReserveW > 0.0f) && (ctrlRect.w >= leftW + clipTotalW + 32.0f);
        if (!showLeftLabel) {
            leftW = 0.0f;
        }
        const float trackX = keel::round(ctrlRect.x + leftW);
        const float trackW = keel::max(12.0f, keel::round(ctrlRect.w - leftW - clipTotalW - rightW));

        const float rowH = (numCh == 1 && showRightValue)
            ? keel::max(mm.barThick, mm.valLineH)
            : (showRightValue ? keel::max(mm.barThick, mm.valLineH * 0.85f) : mm.barThick);
        const float barsTotalH = static_cast<float>(numCh) * rowH + static_cast<float>(numCh - 1) * mm.channelGap;
        const float totalContentH = barsTotalH + mm.scaleRegionH;
        const float startY = ctrlRect.y + keel::max(0.0f, (ctrlRect.h - totalContentH) * 0.5f);

        for (keel::usize ch = 0; ch < numCh; ++ch) {
            const LevelMeterChannelData& cd = d->channels[ch];
            const float rowY = startY + static_cast<float>(ch) * (rowH + mm.channelGap);
            const float barY = keel::round(rowY + (rowH - keel::round(mm.barThick)) * 0.5f);

            // 1. Channel label on the left
            if (showLeftLabel && !cd.label.empty()) {
                canvas.DrawTextBox(
                    cd.label,
                    Rect(ctrlRect.x, rowY, mm.labelReserveW, rowH),
                    mm.labelFont,
                    labelCol,
                    0.0f,
                    0.5f
                );
            }

            // 2. Meter bar & Clip LED cap
            DrawChannelBarHorizontal(
                canvas,
                Rect(trackX, barY, trackW, keel::round(mm.barThick)),
                cd,
                opts,
                mm,
                disabled,
                theme
            );

            // 3. Value readout on the right
            if (showRightValue && !cd.formattedValue.empty()) {
                const float valX = trackX + trackW + clipTotalW + 8.0f;
                canvas.DrawTextBox(
                    cd.formattedValue,
                    Rect(valX, rowY, mm.valReserveW, rowH),
                    mm.valFont,
                    valCol,
                    1.0f,
                    0.5f
                );
            }
        }

        // 4. Calibrated horizontal scale ticks & labels below bars (with collision culling)
        if (opts.showScale && trackW >= 28.0f) {
            const float tickTopY = keel::round(startY + barsTotalH + 3.0f);
            const float labelTopY = tickTopY + mm.scaleTickLen + 1.0f;
            constexpr keel::i32 kMajorTicks = 5; // 6 major positions: 0%, 20%, 40%, 60%, 80%, 100%
            const bool showMinorTicks = (trackW >= 80.0f);

            for (keel::i32 i = 0; i <= kMajorTicks * 2; ++i) {
                const bool isMajor = (i % 2 == 0);
                if (!isMajor && !showMinorTicks) continue;
                const float t = static_cast<float>(i) / static_cast<float>(kMajorTicks * 2);
                const float tx = keel::round(trackX + t * trackW);
                const float tLen = isMajor ? mm.scaleTickLen : (mm.scaleTickLen * 0.6f);
                canvas.DrawLine(Vec2(tx, tickTopY), Vec2(tx, tickTopY + tLen), scaleTickCol, 1.0f);
            }

            struct MajorTickLabel {
                char text[32]{};
                float tx{0.0f};
                float tw{0.0f};
                float leftX{0.0f};
                float rightX{0.0f};
                float alignX{0.5f};
                bool visible{false};
            };
            keel::StaticArray<MajorTickLabel, kMajorTicks + 1> majorLabels{};
            for (keel::i32 m = 0; m <= kMajorTicks; ++m) {
                const float t = static_cast<float>(m) / static_cast<float>(kMajorTicks);
                const float tx = keel::round(trackX + t * trackW);
                auto& ml = majorLabels[static_cast<keel::usize>(m)];
                ml.tx = tx;
                FormatScaleTickLabel(ml.text, sizeof(ml.text), d->min + static_cast<double>(t) * span, span);
                ml.tw = keel::max(10.0f, TextUtil::MeasureWidth(keel::StringRef(ml.text), mm.scaleFont, &canvas));
                if (m == 0) {
                    ml.leftX = tx;
                    ml.rightX = tx + ml.tw;
                    ml.alignX = 0.0f;
                } else if (m == kMajorTicks) {
                    ml.leftX = tx - ml.tw;
                    ml.rightX = tx;
                    ml.alignX = 1.0f;
                } else {
                    ml.leftX = tx - ml.tw * 0.5f;
                    ml.rightX = tx + ml.tw * 0.5f;
                    ml.alignX = 0.5f;
                }
            }

            const float minGap = 6.0f;
            bool allFit = true;
            for (keel::i32 m = 0; m < kMajorTicks; ++m) {
                if (majorLabels[static_cast<keel::usize>(m)].rightX + minGap > majorLabels[static_cast<keel::usize>(m + 1)].leftX) {
                    allFit = false;
                    break;
                }
            }

            if (allFit) {
                for (auto& ml : majorLabels) ml.visible = true;
            } else {
                // Always display the 0 dB / max reference endpoint; add min & midpoints if they fit without overlap
                majorLabels[kMajorTicks].visible = true;
                if (majorLabels[0].rightX + minGap <= majorLabels[kMajorTicks].leftX) {
                    majorLabels[0].visible = true;
                    if (majorLabels[2].leftX >= majorLabels[0].rightX + minGap &&
                        majorLabels[2].rightX + minGap <= majorLabels[kMajorTicks].leftX) {
                        majorLabels[2].visible = true;
                    }
                }
            }

            for (keel::i32 m = 0; m <= kMajorTicks; ++m) {
                const auto& ml = majorLabels[static_cast<keel::usize>(m)];
                if (!ml.visible) continue;
                const float boxW = keel::max(ml.tw + 4.0f, 28.0f);
                float boxX = ml.tx - boxW * 0.5f;
                if (m == 0) boxX = ml.tx;
                else if (m == kMajorTicks) boxX = ml.tx - boxW;
                canvas.DrawTextBox(
                    ml.text,
                    Rect(boxX, labelTopY, boxW, mm.scaleLineH),
                    mm.scaleFont,
                    scaleTextCol,
                    ml.alignX,
                    0.5f
                );
            }
        }
    } else {
        // Vertical orientation
        const float barThickInt = keel::max(2.0f, keel::round(mm.barThick));
        const float chGapInt = keel::max(2.0f, keel::round(mm.channelGap));
        const float barsW = static_cast<float>(numCh) * barThickInt + static_cast<float>(numCh - 1) * chGapInt;
        const float assemblyW = mm.scaleRegionW + barsW;
        const float assemblyStartX = ctrlRect.x + keel::max(0.0f, (ctrlRect.w - assemblyW) * 0.5f);

        const float topClipH = opts.showClipLed ? (mm.clipLedLen + mm.clipGap) : 0.0f;
        const float bottomLabelH = (mm.labelReserveW > 0.0f) ? (4.0f + mm.labelLineH) : 0.0f;
        const float bottomValH = opts.showValue ? (4.0f + mm.valLineH) : 0.0f;
        const float trackY = keel::round(ctrlRect.y + topClipH);
        const float trackH = keel::max(24.0f, keel::round(ctrlRect.h - topClipH - bottomLabelH - bottomValH));
        const float barsStartX = keel::round(assemblyStartX + mm.scaleRegionW);

        // 1. Calibrated vertical scale on the left of the bars (with vertical collision culling)
        if (opts.showScale && trackH >= 28.0f) {
            const float tickRightX = keel::round(barsStartX - 4.0f);
            const float labelRightX = tickRightX - mm.scaleTickLen - 3.0f;
            const float labelW = keel::max(8.0f, labelRightX - assemblyStartX);
            constexpr keel::i32 kMajorTicks = 5;
            const bool showMinorTicks = (trackH >= 70.0f);
            float lastDrawnY = 1e9f;
            const float minGapY = mm.scaleLineH * 0.95f;
            const float topMostY = trackY;

            for (keel::i32 i = 0; i <= kMajorTicks * 2; ++i) {
                const bool isMajor = (i % 2 == 0);
                if (!isMajor && !showMinorTicks) continue;
                const float t = static_cast<float>(i) / static_cast<float>(kMajorTicks * 2);
                const float ty = keel::round(trackY + trackH - t * trackH);
                const float tLen = isMajor ? mm.scaleTickLen : (mm.scaleTickLen * 0.6f);
                canvas.DrawLine(Vec2(tickRightX - tLen, ty), Vec2(tickRightX, ty), scaleTickCol, 1.0f);

                if (isMajor) {
                    const bool isTopEndpoint = (i == kMajorTicks * 2);
                    const bool fitsBelowTop = (ty - topMostY >= minGapY);
                    const bool fitsAbovePrev = (lastDrawnY - ty >= minGapY);
                    if (isTopEndpoint || (fitsBelowTop && fitsAbovePrev)) {
                        char sbuf[32];
                        FormatScaleTickLabel(sbuf, sizeof(sbuf), d->min + static_cast<double>(t) * span, span);
                        canvas.DrawTextBox(
                            sbuf,
                            Rect(assemblyStartX, ty - mm.scaleLineH * 0.5f, labelW, mm.scaleLineH),
                            mm.scaleFont,
                            scaleTextCol,
                            1.0f,
                            0.5f
                        );
                        lastDrawnY = ty;
                    }
                }
            }
        }

        // 2. Vertical channel bars & channel labels below each bar
        keel::usize maxChannelIdx = 0;
        for (keel::usize ch = 0; ch < numCh; ++ch) {
            const LevelMeterChannelData& cd = d->channels[ch];
            if (cd.value > d->channels[maxChannelIdx].value) {
                maxChannelIdx = ch;
            }
            const float barX = barsStartX + static_cast<float>(ch) * (barThickInt + chGapInt);
            DrawChannelBarVertical(
                canvas,
                Rect(barX, trackY, barThickInt, trackH),
                cd,
                opts,
                mm,
                disabled,
                theme
            );

            if (mm.labelReserveW > 0.0f && !cd.label.empty()) {
                const float lblY = trackY + trackH + 4.0f;
                const float lblBoxW = keel::max(barThickInt, mm.labelReserveW);
                canvas.DrawTextBox(
                    cd.label,
                    Rect(barX + (barThickInt - lblBoxW) * 0.5f, lblY, lblBoxW, mm.labelLineH),
                    mm.labelFont,
                    labelCol,
                    0.5f,
                    0.5f
                );
            }
        }

        // 3. Value readout at the bottom
        if (opts.showValue) {
            const keel::StringRef valStr = d->channels[maxChannelIdx].formattedValue;
            if (!valStr.empty()) {
                const float valY = trackY + trackH + bottomLabelH + 4.0f;
                canvas.DrawTextBox(
                    valStr,
                    Rect(ctrlRect.x, valY, ctrlRect.w, mm.valLineH),
                    mm.valFont,
                    valCol,
                    0.5f,
                    0.5f
                );
            }
        }
    }
}

} // namespace guikit
