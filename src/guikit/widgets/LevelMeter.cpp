#include "guikit/widgets/LevelMeter.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

constexpr size_t kMaxLevelMeterChannels = 8;
constexpr double kPeakHoldDurationSec = 0.80;

struct LevelMeterPersistState {
    std::array<float, kMaxLevelMeterChannels> peakRatio{};
    std::array<double, kMaxLevelMeterChannels> lastPeakUpdateTimeSec{};
    std::array<bool, kMaxLevelMeterChannels> clipped{};
    double prevFrameTimeSec{-1.0};
    bool initialized{false};
};

struct LevelMeterChannelData {
    double value{0.0};
    float ratio{0.0f};
    float peakRatio{0.0f};
    bool clipped{false};
    std::string_view label{};
    std::string_view formattedValue{};
};

struct LevelMeterData {
    LevelMeterOptions opts;
    double min{-60.0};
    double max{0.0};
    size_t channelCount{1};
    std::array<LevelMeterChannelData, kMaxLevelMeterChannels> channels{};
};

void FormatMeterValue(
    char* valBuf,
    size_t bufSize,
    double value,
    std::string_view format
) noexcept {
    if (!format.empty()) {
        char convSpec = '\0';
        bool isLongLong = false;
        bool isLong = false;
        for (size_t i = 0; i < format.size(); ++i) {
            if (format[i] == '%') {
                if (i + 1 < format.size() && format[i + 1] == '%') {
                    ++i;
                    continue;
                }
                for (size_t j = i + 1; j < format.size(); ++j) {
                    const char c = format[j];
                    if (c == 'l') {
                        if (isLong) isLongLong = true;
                        else isLong = true;
                    } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                        convSpec = c;
                        break;
                    }
                }
                break;
            }
        }

        std::string fmt(format);
        if (convSpec == 'd' || convSpec == 'i' || convSpec == 'u' ||
            convSpec == 'x' || convSpec == 'X' || convSpec == 'o') {
            const long long intVal = static_cast<long long>(std::round(value));
            if (isLongLong) {
                std::snprintf(valBuf, bufSize, fmt.c_str(), intVal);
            } else if (isLong) {
                std::snprintf(valBuf, bufSize, fmt.c_str(), static_cast<long>(intVal));
            } else {
                std::snprintf(valBuf, bufSize, fmt.c_str(), static_cast<int>(intVal));
            }
        } else if (convSpec != '\0') {
            std::snprintf(valBuf, bufSize, fmt.c_str(), value);
        } else {
            std::snprintf(valBuf, bufSize, "%s", fmt.c_str());
        }
    } else {
        std::snprintf(valBuf, bufSize, "%.1f dB", value);
    }
}

void FormatScaleTickLabel(
    char* buf,
    size_t bufSize,
    double val,
    double span
) noexcept {
    if (std::abs(val) < 1e-4) {
        val = 0.0;
    }
    if (span >= 10.0 && std::abs(val - std::round(val)) < 0.05) {
        std::snprintf(buf, bufSize, "%.0f", val);
    } else {
        std::snprintf(buf, bufSize, "%.1f", val);
    }
}

float GetDefaultBarThickness(UiSize size, const UiTheme& theme) noexcept {
    switch (size) {
        case UiSize::Xs: return theme.Scale(8.0f);
        case UiSize::Sm: return theme.Scale(10.0f);
        case UiSize::Lg: return theme.Scale(16.0f);
        case UiSize::Xl: return theme.Scale(20.0f);
        case UiSize::Md:
        default:         return theme.Scale(12.0f);
    }
}

ControlSpec EffectiveLevelMeterControl(
    const LevelMeterOptions& opts,
    UiSize size,
    const UiTheme& theme
) noexcept {
    const SliderMetrics& m = theme.GetSlider(size);
    ControlSpec c{};
    c.width = opts.width;
    c.height = opts.height;
    c.align = opts.align;
    c.background = opts.background;
    c.border = opts.border;
    c.defaultLayout = ElementLayout::Stacked;
    c.defaultInlineOrder = InlineOrder::LabelFirst;
    c.labelGap = m.labelGap;
    c.descGap = m.descGap;
    c.inlineGap = 12.0f;
    c.stretchable = true;
    c.stretchHeightWhenStandalone = false;
    c.hasIntrinsicBox = false;
    return c;
}

Color ResolveZoneColor(
    float normPos,
    const LevelMeterOptions& opts,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const auto& pal = theme.palette;
    const float warnT = std::clamp(opts.warningRatio, 0.05f, 0.98f);
    const float dangerT = std::clamp(opts.dangerRatio, warnT + 0.01f, 0.995f);

    Color col;
    if (!opts.multiZoneColor) {
        const VariantColors& primary = theme.GetVariant(UiVariant::Primary);
        if (!opts.color.HasAny()) {
            col = primary.bg;
        } else if (opts.color.normal.has_value() && opts.color.normal->a == 0) {
            col = pal.borderHover;
        } else {
            col = opts.color.normal.value_or(primary.bg);
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
            } else if (opts.color.normal.has_value() && opts.color.normal->a == 0) {
                col = defaultSafe;
            } else {
                col = opts.color.normal.value_or(defaultSafe);
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
    size_t channelCount,
    double min,
    double max,
    const LevelMeterOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const SliderMetrics& sm = theme.GetSlider(size);
    const size_t numCh = std::clamp<size_t>(channelCount, 1, kMaxLevelMeterChannels);
    const double rangeMin = std::min(min, max);
    const double rangeMax = std::max(min, max);
    const double span = std::max(1e-6, rangeMax - rangeMin);

    LevelMeterMetrics mm{};
    mm.barThick = opts.barThickness.has_value()
        ? theme.Scale(std::max(4.0f, *opts.barThickness))
        : GetDefaultBarThickness(size, theme);
    mm.channelGap = (numCh > 1) ? theme.Scale(4.0f) : 0.0f;

    mm.labelFont = theme.ResolveFontSize(TextRole::LabelSmall, size);
    mm.labelLineH = canvas ? canvas->GetLineHeight(mm.labelFont) : theme.GetLineHeight(mm.labelFont);

    if (!opts.channelLabels.empty()) {
        for (size_t i = 0; i < std::min(numCh, opts.channelLabels.size()); ++i) {
            if (!opts.channelLabels[i].empty()) {
                mm.labelReserveW = std::max(
                    mm.labelReserveW,
                    TextUtil::MeasureWidth(opts.channelLabels[i], mm.labelFont, canvas)
                );
            }
        }
        if (mm.labelReserveW > 0.0f) {
            mm.labelReserveW = std::max(mm.labelReserveW + theme.Scale(2.0f), theme.Scale(16.0f));
        }
    }

    const std::string_view fmt = opts.valueFormat.text.empty() ? "%.1f dB" : opts.valueFormat.text;
    if (opts.showValue) {
        mm.valFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelMedium, size);
        mm.valLineH = canvas ? canvas->GetLineHeight(mm.valFont) : theme.GetLineHeight(mm.valFont);

        auto measureSample = [&](double v) {
            char buf[64];
            FormatMeterValue(buf, sizeof(buf), v, fmt);
            mm.valReserveW = std::max(mm.valReserveW, TextUtil::MeasureWidth(std::string_view(buf), mm.valFont, canvas));
        };
        measureSample(rangeMin);
        measureSample(rangeMax);
        measureSample(rangeMin + span * 0.777);
        for (std::string_view var : opts.valueFormat.variants) {
            if (!var.empty()) {
                mm.valReserveW = std::max(mm.valReserveW, TextUtil::MeasureWidth(var, mm.valFont, canvas));
            }
        }
        mm.valReserveW += theme.Scale(4.0f);
    }

    mm.scaleFont = std::max(8.5f, theme.ResolveFontSize(TextRole::LabelSmall, size) * 0.90f);
    mm.scaleLineH = canvas ? canvas->GetLineHeight(mm.scaleFont) : theme.GetLineHeight(mm.scaleFont);
    mm.scaleTickLen = theme.Scale(4.0f);

    if (opts.showClipLed) {
        mm.clipLedLen = std::clamp(mm.barThick * 0.62f, theme.Scale(6.0f), theme.Scale(12.0f));
        mm.clipGap = theme.Scale(3.0f);
    } else {
        mm.clipLedLen = 0.0f;
        mm.clipGap = 0.0f;
    }

    if (opts.orientation == Orientation::Horizontal) {
        if (opts.showScale) {
            mm.scaleRegionH = theme.Scale(3.0f) + mm.scaleTickLen + theme.Scale(1.0f) + mm.scaleLineH;
        }
        const float rowH = (numCh == 1 && opts.showValue)
            ? std::max(mm.barThick, mm.valLineH)
            : (opts.showValue ? std::max(mm.barThick, mm.valLineH * 0.85f) : mm.barThick);
        const float barsTotalH = static_cast<float>(numCh) * rowH + static_cast<float>(numCh - 1) * mm.channelGap;

        const float defaultTrackW = (opts.stretch != StretchMode::Fill && opts.width.has_value())
            ? theme.Scale(*opts.width)
            : sm.defaultWidth;
        const float leftW = (mm.labelReserveW > 0.0f) ? (mm.labelReserveW + theme.Scale(6.0f)) : 0.0f;
        const float clipW = opts.showClipLed ? (mm.clipGap + mm.clipLedLen) : 0.0f;
        const float rightW = opts.showValue ? (theme.Scale(8.0f) + mm.valReserveW) : 0.0f;

        mm.intrinsicSize = {
            leftW + defaultTrackW + clipW + rightW,
            barsTotalH + mm.scaleRegionH
        };
    } else {
        // Vertical orientation
        if (opts.showScale) {
            float maxScaleLblW = 0.0f;
            for (int i = 0; i <= 5; ++i) {
                const double t = static_cast<double>(i) / 5.0;
                char sbuf[32];
                FormatScaleTickLabel(sbuf, sizeof(sbuf), rangeMin + t * span, span);
                maxScaleLblW = std::max(maxScaleLblW, TextUtil::MeasureWidth(std::string_view(sbuf), mm.scaleFont, canvas));
            }
            mm.scaleRegionW = maxScaleLblW + theme.Scale(4.0f) + mm.scaleTickLen + theme.Scale(4.0f);
        }

        const float barsW = static_cast<float>(numCh) * mm.barThick + static_cast<float>(numCh - 1) * mm.channelGap;
        const float topClipH = opts.showClipLed ? (mm.clipLedLen + mm.clipGap) : 0.0f;
        const float bottomLabelH = (mm.labelReserveW > 0.0f) ? (theme.Scale(4.0f) + mm.labelLineH) : 0.0f;
        const float bottomValH = opts.showValue ? (theme.Scale(4.0f) + mm.valLineH) : 0.0f;
        float defaultTrackH = std::round(theme.Scale(160.0f));
        if (opts.segments > 0) {
            const uint32_t segCount = std::clamp<uint32_t>(opts.segments, 2u, 64u);
            const float gap = (segCount > 28) ? 1.0f : 2.0f;
            const float totalGaps = gap * static_cast<float>(segCount - 1);
            const float segH = std::max(2.0f, std::round((defaultTrackH - totalGaps) / static_cast<float>(segCount)));
            defaultTrackH = static_cast<float>(segCount) * segH + totalGaps;
        }

        const float totalW = std::max(mm.scaleRegionW + barsW, opts.showValue ? mm.valReserveW : 0.0f) + theme.Scale(8.0f);
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

    trackRect.x = std::round(trackRect.x);
    trackRect.y = std::round(trackRect.y);
    trackRect.w = std::round(trackRect.w);
    trackRect.h = std::max(2.0f, std::round(trackRect.h));

    const auto& pal = theme.palette;
    const float warnT = std::clamp(opts.warningRatio, 0.05f, 0.98f);
    const float dangerT = std::clamp(opts.dangerRatio, warnT + 0.01f, 0.995f);

    uint32_t segCount = 0;
    float gap = 2.0f;
    float segW = 2.0f;
    if (opts.segments > 0) {
        segCount = std::clamp<uint32_t>(opts.segments, 2u, 64u);
        gap = (segCount > 28) ? 1.0f : 2.0f;
        const float totalGaps = gap * static_cast<float>(segCount - 1);
        segW = std::max(1.0f, std::floor((trackRect.w - totalGaps) / static_cast<float>(segCount)));
        trackRect.w = static_cast<float>(segCount) * segW + totalGaps;
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
    canvas.DrawRoundRect(housingRect, std::min(3.0f, housingRect.h * 0.22f), housingBg, housingBorder, 1.0f);

    if (opts.segments > 0) {
        int peakSegIdx = -1;
        if (ch.peakRatio > 0.005f) {
            peakSegIdx = std::clamp(
                static_cast<int>(std::ceil(ch.peakRatio * static_cast<float>(segCount))) - 1,
                0,
                static_cast<int>(segCount) - 1
            );
        }

        const bool useRound = (std::min(segW, trackRect.h) >= 8.0f);
        for (uint32_t i = 0; i < segCount; ++i) {
            const float segX = trackRect.x + static_cast<float>(i) * (segW + gap);
            const float segThreshold = static_cast<float>(i + 1) / static_cast<float>(segCount);
            const float segMidRatio = (static_cast<float>(i) + 0.5f) / static_cast<float>(segCount);
            const bool isLit = (ch.ratio >= segThreshold - (0.5f / static_cast<float>(segCount))) && (ch.ratio > 0.001f);
            const bool isPeakSeg = (static_cast<int>(i) == peakSegIdx);

            const Color zoneCol = ResolveZoneColor(segMidRatio, opts, disabled, theme);
            const Rect segRect(segX, trackRect.y, segW, trackRect.h);
            const Color fillCol = (isLit || isPeakSeg) ? zoneCol : zoneCol.WithAlphaF(disabled ? 0.08f : 0.16f);

            if (useRound) {
                canvas.DrawRoundRect(segRect, 1.0f, fillCol);
            } else {
                canvas.DrawRect(segRect, fillCol);
            }
        }
    } else {
        // Smooth continuous bar
        const float radius = std::min(3.0f, trackRect.h * 0.3f);
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
                const float rSafe = std::min(ch.ratio, warnT);
                if (rSafe > 0.0f) {
                    canvas.DrawRect(Rect(trackRect.x, trackRect.y, rSafe * trackRect.w, trackRect.h), cSafe);
                }
                if (ch.ratio > warnT) {
                    const float rWarn = std::min(ch.ratio, dangerT) - warnT;
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
            const float peakX = std::clamp(
                trackRect.x + ch.peakRatio * trackRect.w - 1.25f,
                trackRect.x,
                std::max(trackRect.x, trackRect.Right() - 2.5f)
            );
            const Color peakCol = ResolveZoneColor(ch.peakRatio, opts, disabled, theme);
            canvas.DrawRoundRect(Rect(peakX, trackRect.y - 1.0f, 2.5f, trackRect.h + 2.0f), 1.0f, peakCol);
        }
    }

    // Dedicated Clip LED cap at right end
    if (opts.showClipLed && mm.clipLedLen > 0.0f) {
        const Rect clipRect(
            std::round(trackRect.Right() + std::max(3.0f, std::round(mm.clipGap))),
            trackRect.y,
            std::round(mm.clipLedLen),
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
        canvas.DrawRoundRect(clipRect, std::min(2.5f, clipRect.w * 0.3f), clipFill, clipBorder, 1.0f);
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

    trackRect.x = std::round(trackRect.x);
    trackRect.y = std::round(trackRect.y);
    trackRect.w = std::max(2.0f, std::round(trackRect.w));
    trackRect.h = std::round(trackRect.h);

    const auto& pal = theme.palette;
    const float warnT = std::clamp(opts.warningRatio, 0.05f, 0.98f);
    const float dangerT = std::clamp(opts.dangerRatio, warnT + 0.01f, 0.995f);

    uint32_t segCount = 0;
    float gap = 2.0f;
    float segH = 2.0f;
    if (opts.segments > 0) {
        segCount = std::clamp<uint32_t>(opts.segments, 2u, 64u);
        gap = (segCount > 28) ? 1.0f : 2.0f;
        const float totalGaps = gap * static_cast<float>(segCount - 1);
        segH = std::max(1.0f, std::floor((trackRect.h - totalGaps) / static_cast<float>(segCount)));
        const float exactH = static_cast<float>(segCount) * segH + totalGaps;
        trackRect.y = std::round(trackRect.Bottom() - exactH);
        trackRect.h = exactH;
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
    canvas.DrawRoundRect(housingRect, std::min(3.0f, housingRect.w * 0.22f), housingBg, housingBorder, 1.0f);

    if (opts.segments > 0) {
        int peakSegIdx = -1;
        if (ch.peakRatio > 0.005f) {
            peakSegIdx = std::clamp(
                static_cast<int>(std::ceil(ch.peakRatio * static_cast<float>(segCount))) - 1,
                0,
                static_cast<int>(segCount) - 1
            );
        }

        const bool useRound = (std::min(trackRect.w, segH) >= 8.0f);
        for (uint32_t i = 0; i < segCount; ++i) {
            // Segment 0 is at the bottom, segment segCount-1 is at the top (all integer pixel coordinates)
            const float segY = trackRect.Bottom() - static_cast<float>(i + 1) * segH - static_cast<float>(i) * gap;
            const float segThreshold = static_cast<float>(i + 1) / static_cast<float>(segCount);
            const float segMidRatio = (static_cast<float>(i) + 0.5f) / static_cast<float>(segCount);
            const bool isLit = (ch.ratio >= segThreshold - (0.5f / static_cast<float>(segCount))) && (ch.ratio > 0.001f);
            const bool isPeakSeg = (static_cast<int>(i) == peakSegIdx);

            const Color zoneCol = ResolveZoneColor(segMidRatio, opts, disabled, theme);
            const Rect segRect(trackRect.x, segY, trackRect.w, segH);
            const Color fillCol = (isLit || isPeakSeg) ? zoneCol : zoneCol.WithAlphaF(disabled ? 0.08f : 0.16f);

            if (useRound) {
                canvas.DrawRoundRect(segRect, 1.0f, fillCol);
            } else {
                canvas.DrawRect(segRect, fillCol);
            }
        }
    } else {
        // Smooth continuous vertical bar (fills bottom-to-top)
        const float radius = std::min(3.0f, trackRect.w * 0.3f);
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
                const float rSafe = std::min(ch.ratio, warnT);
                if (rSafe > 0.0f) {
                    const float hs = rSafe * trackRect.h;
                    canvas.DrawRect(Rect(trackRect.x, trackRect.Bottom() - hs, trackRect.w, hs), cSafe);
                }
                if (ch.ratio > warnT) {
                    const float rWarn = std::min(ch.ratio, dangerT) - warnT;
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
            const float peakY = std::clamp(
                trackRect.Bottom() - ch.peakRatio * trackRect.h - 1.25f,
                trackRect.y,
                std::max(trackRect.y, trackRect.Bottom() - 2.5f)
            );
            const Color peakCol = ResolveZoneColor(ch.peakRatio, opts, disabled, theme);
            canvas.DrawRoundRect(Rect(trackRect.x - 1.0f, peakY, trackRect.w + 2.0f, 2.5f), 1.0f, peakCol);
        }
    }

    // Dedicated Clip LED cap at top end
    if (opts.showClipLed && mm.clipLedLen > 0.0f) {
        const float clipH = std::round(mm.clipLedLen);
        const float clipGap = std::max(3.0f, std::round(mm.clipGap));
        const Rect clipRect(
            trackRect.x,
            std::round(trackRect.y - clipGap - clipH),
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
        canvas.DrawRoundRect(clipRect, std::min(2.5f, clipRect.h * 0.3f), clipFill, clipBorder, 1.0f);
    }
}

Response RunLevelMeterImpl(
    Context& ctx,
    const double* values,
    size_t channelCount,
    double min,
    double max,
    const LevelMeterOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const UiTheme& activeTheme = ctx.GetTheme();

    LevelMeterOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    const double rangeMin = std::min(min, max);
    const double rangeMax = std::max(min, max);
    const double span = std::max(1e-9, rangeMax - rangeMin);
    const size_t numCh = std::clamp<size_t>(channelCount, 1, kMaxLevelMeterChannels);

    auto& state = ctx.GetOrCreateState<LevelMeterPersistState>(id);
    const double nowSec = ctx.GetCurrentTime();
    const float dt = (state.prevFrameTimeSec >= 0.0 && nowSec > state.prevFrameTimeSec)
        ? static_cast<float>(std::min(0.25, nowSec - state.prevFrameTimeSec))
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

    const std::string_view fmt = optsCopy.valueFormat.text.empty() ? "%.1f dB" : optsCopy.valueFormat.text;
    bool needsDecayRepaint = false;

    for (size_t ch = 0; ch < numCh; ++ch) {
        const double rawVal = (values != nullptr) ? values[ch] : rangeMin;
        const double clampedVal = std::clamp(rawVal, rangeMin, rangeMax);
        const float ratio = static_cast<float>(std::clamp((clampedVal - rangeMin) / span, 0.0, 1.0));

        if (!state.initialized) {
            state.peakRatio[ch] = ratio;
            state.lastPeakUpdateTimeSec[ch] = nowSec;
        }

        if (ratio >= 0.995f) {
            state.clipped[ch] = true;
        }

        if (optsCopy.peakHoldValue.has_value()) {
            state.peakRatio[ch] = static_cast<float>(std::clamp((*optsCopy.peakHoldValue - rangeMin) / span, 0.0, 1.0));
        } else if (optsCopy.autoPeakHold) {
            if (ratio >= state.peakRatio[ch]) {
                state.peakRatio[ch] = ratio;
                state.lastPeakUpdateTimeSec[ch] = nowSec;
            } else if ((nowSec - state.lastPeakUpdateTimeSec[ch]) > kPeakHoldDurationSec) {
                const float decayRate = std::max(0.01f, optsCopy.peakHoldDecayPerSec);
                state.peakRatio[ch] = std::max(ratio, state.peakRatio[ch] - decayRate * dt);
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
            : std::string_view{};
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
    node->theme = pushedTheme;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->debugBoundsOverride = optsCopy.debugBounds;
    node->inspectModeOverride = optsCopy.inspectMode;
    node->widthMode = (optsCopy.stretch != StretchMode::Compact && optsCopy.orientation == Orientation::Horizontal)
        ? SizeMode::Stretch
        : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;
    node->text = data->channels[0].formattedValue;
    node->SetPayload(data);

    uint64_t chHash = 0;
    for (size_t ch = 0; ch < numCh; ++ch) {
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
        static_cast<uint8_t>(data->opts.orientation),
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
        static_cast<uint8_t>(data->opts.stretch),
        static_cast<uint8_t>(data->opts.align),
        data->opts.background,
        data->opts.border,
        data->opts.width.value_or(0.0f),
        data->opts.height.value_or(0.0f),
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

    Control::SetupNode(ctx, node, ctrl, data->opts.stretch, data->opts.align);
    ctx.AdaptChildToContainer(node);
    if (pushedTheme) {
        ctx.PopTheme();
    }
    return res;
}

} // namespace

Response LevelMeter::Run(
    Context& ctx,
    double value,
    double min,
    double max,
    const LevelMeterOptions& opts,
    std::source_location loc
) {
    return RunLevelMeterImpl(ctx, &value, 1, min, max, opts, loc);
}

Response LevelMeter::Run(
    Context& ctx,
    std::span<const float> channelValues,
    float min,
    float max,
    const LevelMeterOptions& opts,
    std::source_location loc
) {
    std::array<double, kMaxLevelMeterChannels> vals{};
    const size_t count = channelValues.empty()
        ? 1
        : std::min(channelValues.size(), kMaxLevelMeterChannels);
    if (channelValues.empty()) {
        vals[0] = static_cast<double>(min);
    } else {
        for (size_t i = 0; i < count; ++i) {
            vals[i] = static_cast<double>(channelValues[i]);
        }
    }
    return RunLevelMeterImpl(ctx, vals.data(), count, static_cast<double>(min), static_cast<double>(max), opts, loc);
}

Vec2 LevelMeter::Measure(
    size_t channelCount,
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
    return Control::Measure(mm.intrinsicSize, ctrl, opts.stretch, size, theme);
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
    const bool hovered = node->hovered;
    const size_t numCh = std::clamp<size_t>(d->channelCount, 1, kMaxLevelMeterChannels);

    const ControlSpec ctrl = EffectiveLevelMeterControl(opts, size, theme);
    Control::RenderFrame(canvas, bounds, ctrl, size, disabled, theme, hovered, node->pressed);
    const Rect ctrlRect = Control::ResolveInnerRect(bounds, ctrl, size, theme);
    if (ctrlRect.w <= 2.0f || ctrlRect.h <= 2.0f) return;

    const auto& pal = theme.palette;
    const LevelMeterMetrics mm = ComputeLevelMeterMetrics(numCh, d->min, d->max, opts, size, &canvas, theme);
    const double span = std::max(1e-6, d->max - d->min);

    const Color labelCol = disabled ? pal.textSecondary.WithAlphaF(0.4f) : pal.textSecondary;
    const Color valCol = theme.ResolveTextColor(opts.valueFormat, UiVariant::Secondary, disabled);
    const Color scaleTickCol = disabled ? pal.border.WithAlphaF(0.35f) : pal.borderHover;
    const Color scaleTextCol = disabled ? pal.textMuted.WithAlphaF(0.35f) : pal.textMuted;

    if (opts.orientation == Orientation::Horizontal) {
        const float leftW = (mm.labelReserveW > 0.0f) ? (mm.labelReserveW + theme.Scale(6.0f)) : 0.0f;
        const float clipTotalW = opts.showClipLed ? (mm.clipGap + mm.clipLedLen) : 0.0f;
        const float rightW = opts.showValue ? (theme.Scale(8.0f) + mm.valReserveW) : 0.0f;
        const float trackX = std::round(ctrlRect.x + leftW);
        float trackW = std::max(16.0f, std::round(ctrlRect.w - leftW - clipTotalW - rightW));
        if (opts.segments > 0) {
            const uint32_t segCount = std::clamp<uint32_t>(opts.segments, 2u, 64u);
            const float gap = (segCount > 28) ? 1.0f : 2.0f;
            const float totalGaps = gap * static_cast<float>(segCount - 1);
            const float segW = std::max(1.0f, std::floor((trackW - totalGaps) / static_cast<float>(segCount)));
            trackW = static_cast<float>(segCount) * segW + totalGaps;
        }

        const float rowH = (numCh == 1 && opts.showValue)
            ? std::max(mm.barThick, mm.valLineH)
            : (opts.showValue ? std::max(mm.barThick, mm.valLineH * 0.85f) : mm.barThick);
        const float barsTotalH = static_cast<float>(numCh) * rowH + static_cast<float>(numCh - 1) * mm.channelGap;
        const float totalContentH = barsTotalH + mm.scaleRegionH;
        const float startY = ctrlRect.y + std::max(0.0f, (ctrlRect.h - totalContentH) * 0.5f);

        for (size_t ch = 0; ch < numCh; ++ch) {
            const LevelMeterChannelData& cd = d->channels[ch];
            const float rowY = startY + static_cast<float>(ch) * (rowH + mm.channelGap);
            const float barY = std::round(rowY + (rowH - std::round(mm.barThick)) * 0.5f);

            // 1. Channel label on the left
            if (mm.labelReserveW > 0.0f && !cd.label.empty()) {
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
                Rect(trackX, barY, trackW, std::round(mm.barThick)),
                cd,
                opts,
                mm,
                disabled,
                theme
            );

            // 3. Value readout on the right
            if (opts.showValue && !cd.formattedValue.empty()) {
                const float valX = trackX + trackW + clipTotalW + theme.Scale(8.0f);
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

        // 4. Calibrated horizontal scale ticks & labels below bars
        if (opts.showScale && trackW >= 32.0f) {
            const float tickTopY = std::round(startY + barsTotalH + theme.Scale(3.0f));
            const float labelTopY = tickTopY + mm.scaleTickLen + theme.Scale(1.0f);
            constexpr int kMajorTicks = 5; // 6 positions: 0%, 20%, 40%, 60%, 80%, 100%

            for (int i = 0; i <= kMajorTicks * 2; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(kMajorTicks * 2);
                const float tx = std::round(trackX + t * trackW);
                const bool isMajor = (i % 2 == 0);
                const float tLen = isMajor ? mm.scaleTickLen : (mm.scaleTickLen * 0.6f);
                canvas.DrawLine(Vec2(tx, tickTopY), Vec2(tx, tickTopY + tLen), scaleTickCol, 1.0f);

                if (isMajor) {
                    char sbuf[32];
                    FormatScaleTickLabel(sbuf, sizeof(sbuf), d->min + static_cast<double>(t) * span, span);
                    const float lblW = theme.Scale(36.0f);
                    float lblX = tx - lblW * 0.5f;
                    float alignX = 0.5f;
                    if (i == 0) {
                        lblX = tx;
                        alignX = 0.0f;
                    } else if (i == kMajorTicks * 2) {
                        lblX = tx - lblW;
                        alignX = 1.0f;
                    }
                    canvas.DrawTextBox(
                        sbuf,
                        Rect(lblX, labelTopY, lblW, mm.scaleLineH),
                        mm.scaleFont,
                        scaleTextCol,
                        alignX,
                        0.5f
                    );
                }
            }
        }
    } else {
        // Vertical orientation
        const float barThickInt = std::max(2.0f, std::round(mm.barThick));
        const float chGapInt = std::max(2.0f, std::round(mm.channelGap));
        const float barsW = static_cast<float>(numCh) * barThickInt + static_cast<float>(numCh - 1) * chGapInt;
        const float assemblyW = mm.scaleRegionW + barsW;
        float assemblyStartX = ctrlRect.x;
        if (opts.align == Align::Center) {
            assemblyStartX = ctrlRect.x + std::max(0.0f, (ctrlRect.w - assemblyW) * 0.5f);
        } else if (opts.align == Align::End) {
            assemblyStartX = ctrlRect.x + std::max(0.0f, ctrlRect.w - assemblyW);
        }

        const float topClipH = opts.showClipLed ? (mm.clipLedLen + mm.clipGap) : 0.0f;
        const float bottomLabelH = (mm.labelReserveW > 0.0f) ? (theme.Scale(4.0f) + mm.labelLineH) : 0.0f;
        const float bottomValH = opts.showValue ? (theme.Scale(4.0f) + mm.valLineH) : 0.0f;
        float trackY = std::round(ctrlRect.y + topClipH);
        float trackH = std::max(24.0f, std::round(ctrlRect.h - topClipH - bottomLabelH - bottomValH));
        if (opts.segments > 0) {
            const uint32_t segCount = std::clamp<uint32_t>(opts.segments, 2u, 64u);
            const float gap = (segCount > 28) ? 1.0f : 2.0f;
            const float totalGaps = gap * static_cast<float>(segCount - 1);
            const float segH = std::max(1.0f, std::floor((trackH - totalGaps) / static_cast<float>(segCount)));
            const float exactH = static_cast<float>(segCount) * segH + totalGaps;
            trackY = std::round((trackY + trackH) - exactH);
            trackH = exactH;
        }
        const float barsStartX = std::round(assemblyStartX + mm.scaleRegionW);

        // 1. Calibrated vertical scale on the left of the bars
        if (opts.showScale && trackH >= 32.0f) {
            const float tickRightX = std::round(barsStartX - theme.Scale(4.0f));
            const float labelRightX = tickRightX - mm.scaleTickLen - theme.Scale(3.0f);
            const float labelW = std::max(8.0f, labelRightX - assemblyStartX);
            constexpr int kMajorTicks = 5;

            for (int i = 0; i <= kMajorTicks * 2; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(kMajorTicks * 2);
                const float ty = std::round(trackY + trackH - t * trackH);
                const bool isMajor = (i % 2 == 0);
                const float tLen = isMajor ? mm.scaleTickLen : (mm.scaleTickLen * 0.6f);
                canvas.DrawLine(Vec2(tickRightX - tLen, ty), Vec2(tickRightX, ty), scaleTickCol, 1.0f);

                if (isMajor) {
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
                }
            }
        }

        // 2. Vertical channel bars & channel labels below each bar
        size_t maxChannelIdx = 0;
        for (size_t ch = 0; ch < numCh; ++ch) {
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
                const float lblY = trackY + trackH + theme.Scale(4.0f);
                const float lblBoxW = std::max(barThickInt, mm.labelReserveW);
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
            const std::string_view valStr = d->channels[maxChannelIdx].formattedValue;
            if (!valStr.empty()) {
                const float valY = trackY + trackH + bottomLabelH + theme.Scale(4.0f);
                const float valBoxW = std::max(assemblyW, mm.valReserveW);
                const float valBoxX = assemblyStartX + (assemblyW - valBoxW) * 0.5f;
                canvas.DrawTextBox(
                    valStr,
                    Rect(valBoxX, valY, valBoxW, mm.valLineH),
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
