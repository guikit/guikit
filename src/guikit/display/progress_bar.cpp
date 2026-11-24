#include "guikit/display/progress_bar.hpp"
#include "../controls/slider_internal.hpp"
#include <cstring>

namespace guikit {

using namespace detail::slider_impl;

namespace {

struct ProgressBarData {
    ProgressBarOptions opts;
    keel::f64 value{0.0};
    keel::f64 min{0.0};
    keel::f64 max{100.0};
    keel::f32 animPhase{0.0f};
};


} // namespace

// ============================================================================
// ProgressBar (Continuous Progress Bar & Segmented LED / VU LevelMeter)
// ============================================================================

Response detail::ProgressBar::Run(
    Context& ctx,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
    const ProgressBarOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme& activeTheme = ctx.GetTheme();

    const ProgressBarOptions& optsCopy = opts;

    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);
    const keel::f64 clampedVal = keel::clamp(value, rangeMin, rangeMax);

    Response res = ctx.Interact(id);
    res.clicked = false;
    res.pressed = false;
    if (effectiveDisabled) {
        res.hovered = false;
    } else if (res.hovered && !optsCopy.tooltip.empty()) {
        ctx.SetTooltip(optsCopy.tooltip, ctx.GetPrevFrameBounds(id));
    }

    keel::f32 animPhase = 0.0f;
    if (optsCopy.indeterminate && !effectiveDisabled) {
        const keel::f32 animScale = keel::max(0.0f, activeTheme.animationScale);
        if (animScale > 0.001f) {
            animPhase = keel::as<keel::f32>(keel::fmod(keel::max(0.0, ctx.GetCurrentTime()) * 0.85 * keel::as<keel::f64>(animScale), 1.0));
            ctx.RequestRepaint(2);
        } else {
            animPhase = 0.35f;
        }
    }

    const ControlSpec ctrl = EffectiveSliderControl(optsCopy, activeSize, activeTheme);
    Node* node = ctx.CreateNode(NodeType::ProgressBar, id);
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;

    auto* data = ctx.AllocPayload<ProgressBarData>();
    data->opts = optsCopy;
    data->opts.textLeft = ctx.AllocText(optsCopy.textLeft);
    data->opts.textRight = ctx.AllocText(optsCopy.textRight);
    data->opts.tooltip = ctx.AllocText(optsCopy.tooltip);
    data->opts.valueFormat = ctx.AllocText(optsCopy.valueFormat);
    data->value = clampedVal;
    data->min = rangeMin;
    data->max = rangeMax;
    data->animPhase = animPhase;
    node->SetPayload(data);

    const keel::u64 valBits = keel::bit_cast<keel::u64>(clampedVal);
    const keel::f64 peakVal = data->opts.peakHoldValue.value_or(-1e9);
    const keel::u64 peakBits = keel::bit_cast<keel::u64>(peakVal);

    node->visualHash = detail::HashArgs(
        valBits,
        peakBits,
        keel::as<keel::u8>(data->opts.orientation),
        data->opts.indeterminate,
        animPhase,
        data->opts.segments,
        data->opts.multiZoneColor,
        data->opts.warningRatio,
        data->opts.dangerRatio,
        data->opts.trackHeight.value_or(0.0f),
        data->opts.color,
        keel::as<keel::u8>(data->opts.valuePlacement),
        data->opts.width,
        data->opts.height,
        data->opts.minWidth.value_or(0.0f),
        data->opts.maxWidth.value_or(0.0f),
        data->opts.minHeight.value_or(0.0f),
        data->opts.maxHeight.value_or(0.0f),
        data->opts.showValue,
        data->opts.textLeft,
        data->opts.textRight,
        data->opts.valueFormat
    );

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<ProgressBarData>();
        return detail::ProgressBar::Measure(
            d ? d->opts : ProgressBarOptions{},
            d ? d->value : 0.0,
            d ? d->min : 0.0,
            d ? d->max : 100.0,
            n->uiSize, canvas, theme, availableWidth
        );
    };

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<ProgressBarData>();
        if (!d) return;
        detail::ProgressBar::Render(
            canvas, n->computedRect, d->value, d->min, d->max,
            d->opts, n->uiSize, n->hovered, n->disabled, theme, d->animPhase
        );
    };

    Control::SetupNode(ctx, node, ctrl);
    return res;
}

Vec2 detail::ProgressBar::Measure(
    const ProgressBarOptions& opts,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 /*availableWidth*/
) {
    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);
    const bool hasStaticLabel = !opts.valueFormat.text.empty() && opts.valueFormat.text.find('%') == keel::StringRef::npos;
    const bool effShowVal = opts.showValue && (!opts.indeterminate || hasStaticLabel);
    const keel::StringRef fmt = opts.valueFormat.text.empty() ? "%.0f %%" : opts.valueFormat.text;
    char valBuf[64]{};
    if (effShowVal) {
        FormatSliderValue(valBuf, sizeof(valBuf), value, false, fmt);
    }

    UiText fmtText = opts.valueFormat;
    if (fmtText.text.empty()) {
        fmtText.text = fmt;
    }

    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, fmtText, valBuf,
        effShowVal, opts.valuePlacement, ctrl.width,
        0.0f, size, canvas, theme,
        {}, !opts.indeterminate, rangeMin, rangeMax, false, opts.orientation
    );
    if (opts.trackHeight.has_value()) {
        const keel::f32 minThick = *opts.trackHeight + 4.0f;
        if (IsVerticalSlider(opts.orientation)) {
            if (minThick > cm.crossW) {
                cm.intrinsicControlSize.x += (minThick - cm.crossW);
                cm.crossW = minThick;
            }
        } else {
            if (minThick > cm.trackRowH) {
                cm.intrinsicControlSize.y += (minThick - cm.trackRowH);
                cm.trackRowH = minThick;
            }
        }
    }
    return Control::Measure(cm.intrinsicControlSize, ctrl, theme);
}

Vec2 detail::ProgressBar::Measure(
    const ProgressBarOptions& opts,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
    UiSize size,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, value, min, max, size, canvas, defaultTheme, availableWidth);
}

void detail::ProgressBar::Render(
    ICanvas& canvas,
    Rect bounds,
    keel::f64 value,
    keel::f64 min,
    keel::f64 max,
    const ProgressBarOptions& opts,
    UiSize size,
    bool hovered,
    bool disabled,
    const UiTheme& theme,
    keel::f32 animPhase
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const SliderMetrics& m = theme.GetSlider(size);
    const auto& pal = theme.palette;
    const keel::f64 rangeMin = keel::min(min, max);
    const keel::f64 rangeMax = keel::max(min, max);
    const bool vertical = IsVerticalSlider(opts.orientation);
    const bool hasStaticLabel = !opts.valueFormat.text.empty() && opts.valueFormat.text.find('%') == keel::StringRef::npos;
    const bool effShowVal = opts.showValue && (!opts.indeterminate || hasStaticLabel);
    const keel::StringRef fmt = opts.valueFormat.text.empty() ? "%.0f %%" : opts.valueFormat.text;

    char valBuf[64]{};
    if (effShowVal) {
        FormatSliderValue(valBuf, sizeof(valBuf), value, false, fmt);
    }

    UiText fmtText = opts.valueFormat;
    if (fmtText.text.empty()) {
        fmtText.text = fmt;
    }

    (void)hovered;
    const ControlSpec ctrl = EffectiveSliderControl(opts, size, theme);
    SliderControlMetrics cm = ComputeSliderControlMetrics(
        opts.textLeft, opts.textRight, fmtText, valBuf,
        effShowVal, opts.valuePlacement, ctrl.width,
        0.0f, size, &canvas, theme,
        {}, !opts.indeterminate, rangeMin, rangeMax, false, opts.orientation
    );
    const keel::f32 barThick = opts.trackHeight.has_value()
        ? *opts.trackHeight
        : keel::max(m.trackHeight * 1.55f, 8.0f);
    if (!vertical && barThick + 4.0f > cm.trackRowH) {
        cm.intrinsicControlSize.y += ((barThick + 4.0f) - cm.trackRowH);
        cm.trackRowH = barThick + 4.0f;
    } else if (vertical && barThick + 4.0f > cm.crossW) {
        cm.intrinsicControlSize.x += ((barThick + 4.0f) - cm.crossW);
        cm.crossW = barThick + 4.0f;
    }

    const Rect ctrlRect = bounds;

    const keel::f64 range = rangeMax - rangeMin;
    const keel::f32 ratio = opts.indeterminate
        ? 0.0f
        : ((range > 0.0) ? keel::as<keel::f32>(keel::clamp((value - rangeMin) / range, 0.0, 1.0)) : 0.0f);

    auto resolveZoneColor = [&](keel::f32 normPos) -> Color {
        if (!opts.multiZoneColor) {
            return ResolveSliderTrackColor(opts.color, disabled, theme);
        }
        Color col;
        if (normPos >= opts.dangerRatio) {
            col = theme.GetSemanticAccent(UiVariant::Danger);
        } else if (normPos >= opts.warningRatio) {
            col = theme.GetSemanticAccent(UiVariant::Warning);
        } else if (!opts.color.HasAny() || opts.color.token == StateColorToken::Primary) {
            col = theme.GetSemanticAccent(UiVariant::Success);
        } else if (opts.color.token == StateColorToken::Ghost || (opts.color.normal.has_value() && opts.color.normal->a == 0)) {
            col = pal.borderHover;
        } else {
            const StateColor resolved = theme.ResolveStateColor(opts.color);
            col = resolved.normal.value_or(theme.GetSemanticAccent(UiVariant::Success));
        }
        return disabled ? col.WithAlphaF(theme.disabledOpacity) : col;
    };

    if (vertical) {
        SliderTrackLayout layout = ComputeSliderTrackLayout(ctrlRect, cm, m, true);
        const keel::f32 trackTop = layout.region.y;
        const keel::f32 trackH = keel::max(12.0f, layout.region.h);
        const keel::f32 barWInt = keel::max(2.0f, keel::round(barThick));
        const keel::f32 barX = keel::round(layout.cross - barWInt * 0.5f);
        const keel::f32 fillH = ratio * trackH;
        const Vec2 tipCenter(layout.cross, trackTop + trackH - fillH);

        RenderSliderCompanionTextsVertical(
            canvas, ctrlRect, layout, tipCenter, barWInt,
            cm, opts.textLeft, opts.textRight, fmtText, valBuf,
            effShowVal, opts.valuePlacement, disabled, theme
        );

        if (opts.indeterminate) {
            const Rect trackBgRect(barX, trackTop, barWInt, trackH);
            const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
            const Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
            canvas.DrawRoundRect(trackBgRect, barWInt * 0.5f, trackBg, trackBorder, 1.0f);

            const keel::f32 shuttleH = keel::max(barWInt, trackH * 0.32f);
            const keel::f32 pingPong = 1.0f - keel::abs(keel::fmod(animPhase * 2.0f, 2.0f) - 1.0f);
            const keel::f32 smoothT = pingPong * pingPong * (3.0f - 2.0f * pingPong);
            const keel::f32 shuttleY = trackTop + (1.0f - smoothT) * keel::max(0.0f, trackH - shuttleH);
            canvas.DrawRoundRect(Rect(barX, shuttleY, barWInt, shuttleH), barWInt * 0.5f, ResolveSliderTrackColor(opts.color, disabled, theme));
            return;
        }

        if (opts.segments > 1) {
            const keel::u32 segCount = keel::min<keel::u32>(opts.segments, 64u);
            const keel::f32 gap = (segCount > 28) ? 1.0f : 2.0f;
            const keel::f32 totalGaps = gap * keel::as<keel::f32>(segCount - 1);
            const keel::f32 segH = keel::max(2.0f, keel::floor((keel::round(trackH) - totalGaps) / keel::as<keel::f32>(segCount)));
            const keel::f32 bottomY = keel::round(trackTop + trackH);
            const bool useRound = (keel::min(barWInt, segH) >= 8.0f);
            const Color unlitBg = disabled ? pal.surfaceActive.WithAlphaF(0.35f) : pal.surfaceActive;
            const Color unlitBorder = disabled ? pal.border.WithAlphaF(0.25f) : pal.border.WithAlphaF(0.65f);

            keel::i32 peakSegIdx = -1;
            if (opts.peakHoldValue.has_value() && range > 0.0) {
                const keel::f32 peakRatio = keel::as<keel::f32>(keel::clamp((*opts.peakHoldValue - rangeMin) / range, 0.0, 1.0));
                if (peakRatio > 0.001f) {
                    peakSegIdx = keel::clamp(
                        keel::ceil_i32(peakRatio * keel::as<keel::f32>(segCount)) - 1,
                        0,
                        keel::as<keel::i32>(segCount) - 1
                    );
                }
            }

            for (keel::u32 i = 0; i < segCount; ++i) {
                const keel::f32 segY = bottomY - keel::as<keel::f32>(i + 1) * segH - keel::as<keel::f32>(i) * gap;
                const keel::f32 segThreshold = keel::as<keel::f32>(i + 1) / keel::as<keel::f32>(segCount);
                const keel::f32 segMidRatio = (keel::as<keel::f32>(i) + 0.5f) / keel::as<keel::f32>(segCount);
                const bool isLit = (ratio >= segThreshold - (0.5f / keel::as<keel::f32>(segCount))) && (ratio > 0.001f);
                const bool isPeakSeg = (keel::as<keel::i32>(i) == peakSegIdx);
                const Rect segRect(barX, segY, barWInt, segH);
                if (isLit || isPeakSeg) {
                    const Color segCol = resolveZoneColor(segMidRatio);
                    if (useRound) canvas.DrawRoundRect(segRect, 1.0f, segCol);
                    else canvas.DrawRect(segRect, segCol);
                } else {
                    if (useRound) canvas.DrawRoundRect(segRect, 1.0f, unlitBg, unlitBorder, 1.0f);
                    else canvas.DrawRect(segRect, unlitBg);
                }
            }
        } else {
            const Rect trackBgRect(barX, trackTop, barWInt, trackH);
            const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
            const Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
            canvas.DrawRoundRect(trackBgRect, barWInt * 0.5f, trackBg, trackBorder, 1.0f);

            if (fillH > 0.5f) {
                const Rect fillRect(barX, trackTop + trackH - fillH, barWInt, fillH);
                canvas.DrawRoundRect(fillRect, barWInt * 0.5f, resolveZoneColor(ratio));
            }
            if (opts.peakHoldValue.has_value() && range > 0.0) {
                const keel::f32 peakRatio = keel::as<keel::f32>(keel::clamp((*opts.peakHoldValue - rangeMin) / range, 0.0, 1.0));
                if (peakRatio > 0.001f) {
                    const keel::f32 peakY = trackTop + trackH - peakRatio * trackH;
                    canvas.DrawRoundRect(Rect(barX - 2.0f, peakY - 1.5f, barWInt + 4.0f, 3.0f), 1.5f, resolveZoneColor(peakRatio));
                }
            }
        }
        return;
    }

    const keel::f32 trackRegionX = ctrlRect.x + cm.leftGutterW;
    const keel::f32 trackRegionW = keel::max(12.0f, ctrlRect.w - cm.leftGutterW - cm.rightGutterW);
    const keel::f32 contentOffsetY = keel::max(0.0f, (ctrlRect.h - cm.intrinsicControlSize.y) * 0.5f);
    const keel::f32 trackRowY = ctrlRect.y + contentOffsetY + cm.topValH;
    const keel::f32 centerY = trackRowY + cm.trackRowH * 0.5f;

    const keel::f32 fillW = ratio * trackRegionW;
    const keel::f32 tipX = trackRegionX + fillW;

    // 0. Draw companion textLeft / textRight and value readout
    RenderSliderCompanionTexts(
        canvas, ctrlRect, trackRegionX, trackRegionW, trackRowY, tipX,
        cm, opts.textLeft, opts.textRight, fmtText, valBuf,
        effShowVal, opts.valuePlacement, disabled, theme
    );

    const bool isSeg3D = (theme.chrome.progressFill == ProgressFillStyle::Segmented);
    const bool sharpRect = isSeg3D || (m.cornerRadius <= 0.01f);
    const keel::f32 effBarThick = isSeg3D ? keel::max(barThick + 4.0f, 14.0f) : barThick;
    const keel::f32 barY = keel::round(centerY - effBarThick * 0.5f);
    const keel::f32 barHInt = keel::max(2.0f, keel::round(effBarThick));

    if (opts.indeterminate) {
        const Rect trackBgRect(trackRegionX, barY, trackRegionW, effBarThick);
        const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
        const Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
        theme.Painter().DrawProgressBar(
            canvas, theme, trackBgRect, 0.0f, false, true, animPhase,
            trackBg, trackBorder, ResolveSliderTrackColor(opts.color, disabled, theme), m.cornerRadius, disabled
        );
        return;
    }

    if (opts.segments > 1) {
        // Segmented LED / VU Level Meter (integer pixel-snapped cells & gaps)
        const keel::u32 segCount = keel::min<keel::u32>(opts.segments, 64u);
        const keel::f32 gap = (segCount > 28) ? 1.0f : 2.0f;
        const keel::f32 totalGaps = gap * keel::as<keel::f32>(segCount - 1);
        const keel::f32 segW = keel::max(2.0f, keel::floor((keel::round(trackRegionW) - totalGaps) / keel::as<keel::f32>(segCount)));
        const keel::f32 startX = keel::round(trackRegionX);
        const bool useRound = !sharpRect && (keel::min(segW, barHInt) >= 8.0f);
        const Color unlitBg = disabled ? pal.surfaceActive.WithAlphaF(0.35f) : pal.surfaceActive;
        const Color unlitBorder = disabled ? pal.border.WithAlphaF(0.25f) : pal.border.WithAlphaF(0.65f);

        keel::i32 peakSegIdx = -1;
        if (opts.peakHoldValue.has_value() && range > 0.0) {
            const keel::f32 peakRatio = keel::as<keel::f32>(keel::clamp((*opts.peakHoldValue - rangeMin) / range, 0.0, 1.0));
            if (peakRatio > 0.001f) {
                peakSegIdx = keel::clamp(
                    keel::ceil_i32(peakRatio * keel::as<keel::f32>(segCount)) - 1,
                    0,
                    keel::as<keel::i32>(segCount) - 1
                );
            }
        }

        for (keel::u32 i = 0; i < segCount; ++i) {
            const keel::f32 segX = startX + keel::as<keel::f32>(i) * (segW + gap);
            const keel::f32 segThreshold = keel::as<keel::f32>(i + 1) / keel::as<keel::f32>(segCount);
            const keel::f32 segMidRatio = (keel::as<keel::f32>(i) + 0.5f) / keel::as<keel::f32>(segCount);
            const bool isLit = (ratio >= segThreshold - (0.5f / keel::as<keel::f32>(segCount))) && (ratio > 0.001f);
            const bool isPeakSeg = (keel::as<keel::i32>(i) == peakSegIdx);

            const Rect segRect(segX, barY, segW, barHInt);
            if (isLit || isPeakSeg) {
                const Color segCol = resolveZoneColor(segMidRatio);
                if (useRound) {
                    canvas.DrawRoundRect(segRect, 1.0f, segCol);
                } else {
                    canvas.DrawRect(segRect, segCol);
                }
            } else {
                if (useRound) {
                    canvas.DrawRoundRect(segRect, 1.0f, unlitBg, unlitBorder, 1.0f);
                } else {
                    canvas.DrawRect(segRect, unlitBg);
                }
            }
        }
    } else {
        const Rect trackBgRect(trackRegionX, barY, trackRegionW, effBarThick);
        const Color trackBg = disabled ? pal.surfaceActive.WithAlphaF(0.4f) : pal.surfaceActive;
        const Color trackBorder = disabled ? pal.border.WithAlphaF(0.3f) : pal.border;
        theme.Painter().DrawProgressBar(
            canvas, theme, trackBgRect, ratio, false, false, animPhase,
            trackBg, trackBorder, resolveZoneColor(ratio), m.cornerRadius, disabled
        );

        // Optional peak-hold marker tick on smooth bar
        if (opts.peakHoldValue.has_value() && range > 0.0) {
            const keel::f32 peakRatio = keel::as<keel::f32>(keel::clamp((*opts.peakHoldValue - rangeMin) / range, 0.0, 1.0));
            if (peakRatio > 0.001f) {
                const keel::f32 peakX = trackRegionX + peakRatio * trackRegionW;
                const Color peakCol = resolveZoneColor(peakRatio);
                canvas.DrawRoundRect(Rect(peakX - 1.5f, barY - 2.0f, 3.0f, effBarThick + 4.0f), sharpRect ? 0.0f : 1.5f, peakCol);
            }
        }
    }
}


} // namespace guikit
