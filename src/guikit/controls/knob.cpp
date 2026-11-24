#include "guikit/controls/knob.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/format.h"
#include "keel/math.h"

namespace guikit {

namespace {

constexpr keel::f32 kMinAngleRad = -1.25f * keel::kPiF; // -225 deg (7:30 o'clock)
constexpr keel::f32 kMaxAngleRad =  0.25f * keel::kPiF; //  +45 deg (4:30 o'clock)
constexpr keel::f32 kSweepRad    = kMaxAngleRad - kMinAngleRad; // 270 deg (1.5 * pi)

keel::f32 GetDefaultKnobDiameter(UiSize size, const UiTheme& theme) noexcept {
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs: return 38.0f;
        case UiSize::Preset::Sm: return 46.0f;
        case UiSize::Preset::Lg: return 66.0f;
        case UiSize::Preset::Xl: return 78.0f;
        case UiSize::Preset::Md: return 54.0f;
        default:                 return keel::round(54.0f * theme.SizeFactor(size));
    }
}

keel::f32 ResolveKnobDiameter(const KnobOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    if (opts.diameter.has_value() && *opts.diameter > 0.0f) {
        return *opts.diameter;
    }
    return GetDefaultKnobDiameter(size, theme);
}

ControlSpec EffectiveKnobControl(UiSize size, const UiTheme& theme) noexcept {
    const SliderMetrics& m = theme.GetSlider(size);
    ControlSpec c{};
    c.width = AxisSize::Fit();
    c.height = AxisSize::Fit();
    c.defaultWidthMode = SizeMode::Fit;
    c.defaultHeightMode = SizeMode::Fit;
    c.defaultLayout = ElementLayout::Leading;
    c.labelGap = m.labelGap;
    c.descGap = m.descGap;
    c.inlineGap = m.labelGap;
    return c;
}

keel::f64 QuantizeAndClamp(keel::f64 val, keel::f64 minVal, keel::f64 maxVal, keel::Optional<keel::f64> step) noexcept {
    if (maxVal < minVal) keel::swap(minVal, maxVal);
    val = keel::clamp(val, minVal, maxVal);
    if (step.has_value() && *step > 0.0) {
        const keel::f64 steps = keel::round((val - minVal) / *step);
        val = keel::clamp(minVal + steps * (*step), minVal, maxVal);
    }
    return val;
}

void FormatKnobValue(
    char* buf,
    keel::usize bufSize,
    keel::f64 val,
    bool isInt,
    const KnobOptions& opts
) noexcept {
    if (!opts.valueFormat.empty()) {
        if (isInt) {
            keel::format_buf(buf, bufSize, opts.valueFormat.text, keel::round_i64(val));
        } else {
            keel::format_buf(buf, bufSize, opts.valueFormat.text, val);
        }
    } else if (isInt) {
        keel::format_buf(buf, bufSize, "{}", keel::round_i64(val));
    } else {
        const keel::f64 step = opts.step.value_or(0.0);
        const keel::StringRef fmt = (step >= 1.0) ? "{:.0f}" : ((step >= 0.1) ? "{:.1f}" : "{:.2f}");
        keel::format_buf(buf, bufSize, fmt, val);
    }
}

void DrawAnnularArc(
    ICanvas& canvas,
    Vec2 center,
    keel::f32 radius,
    keel::f32 thickness,
    keel::f32 startAngleRad,
    keel::f32 endAngleRad,
    Color color
) {
    canvas.DrawArc(center, radius, startAngleRad, endAngleRad, color, thickness, /*roundCaps=*/true);
}

} // namespace

Response detail::Knob::Run(
    Context& ctx,
    keel::f64* doubleVal,
    keel::f32* floatVal,
    keel::i64* intVal,
    keel::f64 minVal,
    keel::f64 maxVal,
    const KnobOptions& opts,
    keel::SourceLocation loc
) {

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool isInt = (intVal != nullptr);

    if (maxVal < minVal) {
        keel::swap(minVal, maxVal);
    }
    const keel::f64 span = keel::max(1e-9, maxVal - minVal);
    const keel::Optional<keel::f64> effectiveStep = opts.step.has_value()
        ? opts.step
        : (isInt ? keel::Optional<keel::f64>(1.0) : keel::kNullOpt);

    auto readVal = [&]() noexcept -> keel::f64 {
        if (doubleVal != nullptr) return *doubleVal;
        if (floatVal != nullptr)  return keel::as<keel::f64>(*floatVal);
        if (intVal != nullptr)    return keel::as<keel::f64>(*intVal);
        return minVal;
    };

    auto writeVal = [&](keel::f64 v) noexcept {
        const keel::f64 q = QuantizeAndClamp(v, minVal, maxVal, effectiveStep);
        if (doubleVal != nullptr) *doubleVal = q;
        if (floatVal != nullptr)  *floatVal = keel::as<keel::f32>(q);
        if (intVal != nullptr)    *intVal = keel::round_i64(q);
    };

    keel::f64 currentVal = QuantizeAndClamp(readVal(), minVal, maxVal, effectiveStep);
    writeVal(currentVal);

    ctx.RegisterFocusable(id, effectiveDisabled);
    Response res = ctx.Interact(id);
    Control::ForwardParentElementInteraction(ctx, effectiveDisabled, res);

    auto& state = ctx.GetOrCreateState<KnobState>(id);

    if (effectiveDisabled) {
        state.dragging = false;
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
    } else {
        const bool mouseJustPressed = res.hovered && ctx.IsMouseClicked();
        if (mouseJustPressed || res.clicked) {
            ctx.RequestFocus(id);
        }

        // Double-click resets to defaultValue (or center if bipolar)
        if (res.doubleClicked && (opts.defaultValue.has_value() || opts.bipolar)) {
            const keel::f64 targetVal = opts.defaultValue.has_value()
                ? *opts.defaultValue
                : ((minVal < 0.0 && maxVal > 0.0) ? 0.0 : (minVal + maxVal) * 0.5);
            const keel::f64 prev = currentVal;
            currentVal = QuantizeAndClamp(targetVal, minVal, maxVal, effectiveStep);
            writeVal(currentVal);
            state.dragging = false;
            if (keel::abs(currentVal - prev) > 1e-7) {
                res.changed = true;
            }
        } else if (mouseJustPressed) {
            state.dragging = true;
            state.dragStartVal = currentVal;
            state.dragStartMouse = ctx.GetMousePos();
            ctx.SetActiveId(id);
        }

        if (state.dragging) {
            if (ctx.IsMouseDown()) {
                ctx.SetActiveId(id);
                const Vec2 mousePos = ctx.GetMousePos();
                const keel::f32 dy = state.dragStartMouse.y - mousePos.y;
                const keel::f32 dx = mousePos.x - state.dragStartMouse.x;
                const keel::f32 netDragPx = dy + dx * 0.35f;
                const keel::f32 sensitivityPx = ctx.GetKeyModifiers().shift ? 600.0f : 150.0f;
                const keel::f64 deltaNorm = keel::as<keel::f64>(netDragPx / sensitivityPx);
                const keel::f64 prev = currentVal;
                currentVal = QuantizeAndClamp(state.dragStartVal + deltaNorm * span, minVal, maxVal, effectiveStep);
                writeVal(currentVal);
                res.pressed = true;
                if (keel::abs(currentVal - prev) > 1e-7) {
                    res.changed = true;
                }
            } else {
                state.dragging = false;
                if (ctx.GetActiveId() == id) {
                    ctx.SetActiveId(Id::Null());
                }
            }
        }

        // Mouse wheel and keyboard navigation
        const keel::f64 keyStep = effectiveStep.value_or(span * 0.02);
        keel::f64 stepDelta = 0.0;
        if (res.hovered && keel::abs(ctx.GetMouseWheel().y) > 0.001f) {
            stepDelta += (ctx.GetMouseWheel().y > 0.0f) ? keyStep : -keyStep;
            ctx.ConsumeMouseWheel();
        }
        if (ctx.IsFocused(id)) {
            if (ctx.ConsumeKey(Key::Up) || ctx.ConsumeKey(Key::Right)) {
                stepDelta += keyStep;
            }
            if (ctx.ConsumeKey(Key::Down) || ctx.ConsumeKey(Key::Left)) {
                stepDelta -= keyStep;
            }
            if (ctx.ConsumeKey(Key::Home)) {
                stepDelta = minVal - currentVal;
            }
            if (ctx.ConsumeKey(Key::End)) {
                stepDelta = maxVal - currentVal;
            }
        }
        if (stepDelta != 0.0) {
            const keel::f64 prev = currentVal;
            currentVal = QuantizeAndClamp(currentVal + stepDelta, minVal, maxVal, effectiveStep);
            writeVal(currentVal);
            if (keel::abs(currentVal - prev) > 1e-7) {
                res.changed = true;
            }
        }
    }

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    KnobOptions optsCopy = opts;
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    optsCopy.valueFormat = ctx.AllocText(opts.valueFormat);

    char valBuf[64]{};
    if (optsCopy.showValue) {
        FormatKnobValue(valBuf, sizeof(valBuf), currentVal, isInt, optsCopy);
        if (optsCopy.valueFormat.variants.empty()) {
            char minBuf[64]{};
            char maxBuf[64]{};
            char midBuf[64]{};
            FormatKnobValue(minBuf, sizeof(minBuf), minVal, isInt, optsCopy);
            FormatKnobValue(maxBuf, sizeof(maxBuf), maxVal, isInt, optsCopy);
            FormatKnobValue(midBuf, sizeof(midBuf), minVal + span * 0.888, isInt, optsCopy);

            auto* varArr = ctx.GetArena().alloc_array<keel::StringRef>(3);
            varArr[0] = ctx.AllocString(minBuf);
            varArr[1] = ctx.AllocString(maxBuf);
            varArr[2] = ctx.AllocString(midBuf);
            optsCopy.valueFormat.variants = keel::ArrayRef<keel::StringRef>(varArr, 3);
        }
    }

    const keel::f32 normVal = keel::as<keel::f32>(keel::clamp((currentVal - minVal) / span, 0.0, 1.0));
    keel::f32 normOrigin = 0.0f;
    if (optsCopy.bipolar) {
        if (minVal < 0.0 && maxVal > 0.0) {
            normOrigin = keel::as<keel::f32>(keel::clamp((0.0 - minVal) / span, 0.0, 1.0));
        } else {
            normOrigin = 0.5f;
        }
    }

    const UiTheme& theme = ctx.GetTheme();
    const ControlSpec ctrl = EffectiveKnobControl(activeSize, theme);
    Node* node = ctx.CreateNode(NodeType::Knob, id);

    auto* data = ctx.AllocPayload<KnobData>();
    data->opts = optsCopy;
    data->normalizedValue = normVal;
    data->normalizedOrigin = normOrigin;
    data->valueText = ctx.AllocString(valBuf);
    data->active = state.dragging || res.pressed;
    node->SetPayload(data);
    node->text = data->valueText;
    node->visualHash = detail::HashArgs(
        optsCopy.color,
        optsCopy.showValue,
        keel::as<keel::u8>(optsCopy.valuePlacement),
        optsCopy.bipolar,
        optsCopy.showTicks,
        optsCopy.tickCount,
        optsCopy.showArc,
        optsCopy.diameter.has_value(),
        optsCopy.diameter.value_or(0.0f),
        data->normalizedValue,
        data->normalizedOrigin,
        data->valueText,
        data->active
    );
    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<KnobData>();
        return detail::Knob::Measure(
            d ? d->opts : KnobOptions{},
            d ? d->valueText : keel::StringRef{},
            n->uiSize,
            canvas,
            theme,
            availableWidth
        );
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<KnobData>();
        detail::Knob::Render(
            canvas,
            n->computedRect,
            d ? d->normalizedValue : 0.0f,
            d ? d->normalizedOrigin : 0.0f,
            d ? d->valueText : keel::StringRef{},
            d ? d->opts : KnobOptions{},
            n->uiSize,
            n->hovered,
            d ? d->active : n->pressed,
            n->disabled,
            theme,
            n->focused
        );
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = data->active;
    node->focused = ctx.IsFocusVisible(id);

    Control::SetupNode(ctx, node, ctrl);

    return res;
}

Vec2 detail::Knob::Measure(
    const KnobOptions& opts,
    keel::StringRef valueText,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 /*availableWidth*/
) {
    const keel::f32 dialDia = ResolveKnobDiameter(opts, size, theme);
    const ControlSpec ctrl = EffectiveKnobControl(size, theme);

    keel::f32 w = dialDia;
    keel::f32 h = dialDia;

    if (opts.showValue && (!valueText.empty() || !opts.valueFormat.variants.empty())) {
        const keel::f32 valFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelSmall, size);
        keel::f32 textW = valueText.empty() ? 0.0f : TextUtil::MeasureWidth(valueText, valFont, canvas);
        for (keel::StringRef v : opts.valueFormat.variants) {
            if (!v.empty()) {
                textW = keel::max(textW, TextUtil::MeasureWidth(v, valFont, canvas));
            }
        }
        textW = keel::max(textW + 4.0f, 32.0f);
        const keel::f32 textH = valFont * 1.25f;
        const keel::f32 gap = 4.0f;

        switch (opts.valuePlacement) {
            case Placement::Top:
            case Placement::Bottom:
                w = keel::max(w, textW);
                h += gap + textH;
                break;
            case Placement::Leading:
            case Placement::Trailing:
                w += gap + textW;
                h = keel::max(h, textH);
                break;
            case Placement::Thumb:
                break;
        }
    }

    return Control::Measure({w, h}, ctrl, theme);
}

Vec2 detail::Knob::Measure(
    const KnobOptions& opts,
    keel::StringRef valueText,
    UiSize size,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, valueText, size, canvas, defaultTheme, availableWidth);
}

void detail::Knob::Render(
    ICanvas& canvas,
    Rect bounds,
    keel::f32 normalizedValue,
    keel::f32 normalizedOrigin,
    keel::StringRef valueText,
    const KnobOptions& opts,
    UiSize size,
    bool hovered,
    bool active,
    bool disabled,
    const UiTheme& theme,
    bool focused
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const Rect innerRect = bounds;
    if (innerRect.w <= 2.0f || innerRect.h <= 2.0f) {
        return;
    }

    const auto& pal = theme.palette;
    const keel::f32 valFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelSmall, size);
    const bool hasValText = opts.showValue && !valueText.empty();

    Rect dialArea = innerRect;
    Rect valRect{};

    if (hasValText && opts.valuePlacement != Placement::Thumb) {
        const keel::f32 textH = valFont * 1.25f;
        const keel::f32 gap = 4.0f;
        if (opts.valuePlacement == Placement::Bottom) {
            const keel::f32 dialH = keel::max(16.0f, innerRect.h - textH - gap);
            dialArea = Rect(innerRect.x, innerRect.y, innerRect.w, dialH);
            valRect = Rect(innerRect.x, innerRect.y + dialH + gap, innerRect.w, textH);
        } else if (opts.valuePlacement == Placement::Top) {
            const keel::f32 dialH = keel::max(16.0f, innerRect.h - textH - gap);
            valRect = Rect(innerRect.x, innerRect.y, innerRect.w, textH);
            dialArea = Rect(innerRect.x, innerRect.y + textH + gap, innerRect.w, dialH);
        } else if (opts.valuePlacement == Placement::Leading) {
            keel::f32 textW = TextUtil::MeasureWidth(valueText, valFont, &canvas) + 4.0f;
            for (keel::StringRef v : opts.valueFormat.variants) {
                if (!v.empty()) textW = keel::max(textW, TextUtil::MeasureWidth(v, valFont, &canvas) + 4.0f);
            }
            textW = keel::max(textW, 32.0f);
            const keel::f32 dialW = keel::max(16.0f, innerRect.w - textW - gap);
            valRect = Rect(innerRect.x, innerRect.y, textW, innerRect.h);
            dialArea = Rect(innerRect.x + textW + gap, innerRect.y, dialW, innerRect.h);
        } else if (opts.valuePlacement == Placement::Trailing) {
            keel::f32 textW = TextUtil::MeasureWidth(valueText, valFont, &canvas) + 4.0f;
            for (keel::StringRef v : opts.valueFormat.variants) {
                if (!v.empty()) textW = keel::max(textW, TextUtil::MeasureWidth(v, valFont, &canvas) + 4.0f);
            }
            textW = keel::max(textW, 32.0f);
            const keel::f32 dialW = keel::max(16.0f, innerRect.w - textW - gap);
            dialArea = Rect(innerRect.x, innerRect.y, dialW, innerRect.h);
            valRect = Rect(innerRect.x + dialW + gap, innerRect.y, textW, innerRect.h);
        }
    }

    const keel::f32 targetDia = ResolveKnobDiameter(opts, size, theme);
    const keel::f32 dialDiameter = keel::min(keel::min(dialArea.w, dialArea.h), targetDia);
    const Vec2 center(
        dialArea.x + dialArea.w * 0.5f,
        dialArea.y + dialArea.h * 0.5f
    );
    const keel::f32 outerR = keel::max(6.0f, dialDiameter * 0.5f - 1.5f);

    // Resolve colors
    const VariantColors& primaryVar = theme.GetVariant(UiVariant::Primary);
    Color accentCol;
    if (!opts.color.HasAny()) {
        accentCol = active ? primaryVar.bgActive : (hovered ? primaryVar.bgHover : primaryVar.bg);
    } else {
        accentCol = theme.ResolveColor(opts.color, hovered && !disabled, active && !disabled, disabled, primaryVar.bg, primaryVar.bgHover, primaryVar.bgActive, primaryVar.bg);
    }

    Color trackCol = (hovered && !disabled) ? pal.controlTrackHover : pal.controlTrack;
    Color rotorFill = (active && !disabled) ? pal.surfaceHover : ((hovered && !disabled) ? pal.surfaceElevated : pal.surface);
    Color rotorBorder = focused ? pal.focusRing : ((hovered || active) && !disabled ? pal.borderHover : pal.border);
    Color tickMutedCol = pal.borderHover;
    Color textCol = opts.valueFormat.color.HasAny()
        ? theme.ResolveTextColor(opts.valueFormat, UiVariant::Default, disabled, hovered, active)
        : (active && !disabled ? accentCol : pal.textSecondary);

    if (disabled) {
        const keel::f32 da = theme.disabledOpacity;
        accentCol = accentCol.WithAlphaF(da);
        trackCol = trackCol.WithAlphaF(da);
        rotorFill = rotorFill.WithAlphaF(da);
        rotorBorder = rotorBorder.WithAlphaF(da);
        tickMutedCol = tickMutedCol.WithAlphaF(da);
        textCol = textCol.WithAlphaF(da);
    }

    const keel::f32 normVal = keel::clamp(normalizedValue, 0.0f, 1.0f);
    const keel::f32 normOrig = keel::clamp(normalizedOrigin, 0.0f, 1.0f);
    const keel::f32 valAngle = kMinAngleRad + kSweepRad * normVal;
    const keel::f32 origAngle = kMinAngleRad + kSweepRad * normOrig;

    const keel::f32 tickLen = opts.showTicks ? keel::max(2.5f, outerR * 0.13f) : 0.0f;
    const keel::f32 tickGap = opts.showTicks ? 2.0f : 0.0f;
    const keel::f32 arcThickness = keel::clamp(outerR * 0.14f, 2.5f, 5.5f);
    const keel::f32 arcRadius = outerR - tickLen - tickGap - arcThickness * 0.5f;

    // 1. Radial scale ticks around outer perimeter
    if (opts.showTicks && outerR >= 12.0f) {
        const keel::u32 numTicks = keel::clamp(opts.tickCount == 0 ? 11u : opts.tickCount, 2u, 31u);
        const keel::f32 rTickOuter = outerR;
        const keel::f32 rTickInner = outerR - tickLen;
        const keel::f32 loT = keel::min(normOrig, normVal);
        const keel::f32 hiT = keel::max(normOrig, normVal);

        for (keel::u32 i = 0; i < numTicks; ++i) {
            const keel::f32 t = keel::as<keel::f32>(i) / keel::as<keel::f32>(numTicks - 1);
            const keel::f32 a = kMinAngleRad + kSweepRad * t;
            const bool isMajor = (i == 0) || (i + 1 == numTicks) || (numTicks % 2 == 1 && i == numTicks / 2);
            const keel::f32 innerR = isMajor ? (rTickInner - 1.0f) : rTickInner;
            const bool isTickLit = opts.bipolar
                ? (t >= loT - 0.005f && t <= hiT + 0.005f)
                : (t <= normVal + 0.005f);
            const Color tCol = isTickLit ? accentCol : tickMutedCol;
            const keel::f32 tStroke = isMajor ? 1.6f : 1.1f;

            const Vec2 p0(center.x + keel::cos(a) * innerR, center.y + keel::sin(a) * innerR);
            const Vec2 p1(center.x + keel::cos(a) * rTickOuter, center.y + keel::sin(a) * rTickOuter);
            canvas.DrawLine(p0, p1, tCol, tStroke);
        }
    }

    // 2. Outer track arc and active value arc
    if (opts.showArc && arcRadius > 4.0f) {
        DrawAnnularArc(canvas, center, arcRadius, arcThickness, kMinAngleRad, kMaxAngleRad, trackCol);

        if (opts.bipolar) {
            if (keel::abs(valAngle - origAngle) > 0.01f) {
                DrawAnnularArc(canvas, center, arcRadius, arcThickness, origAngle, valAngle, accentCol);
            }
            // Center zero marker dot on arc
            const Vec2 zeroPt(center.x + keel::cos(origAngle) * arcRadius, center.y + keel::sin(origAngle) * arcRadius);
            const keel::f32 dotR = arcThickness * 0.55f;
            canvas.DrawRoundRect(Rect(zeroPt.x - dotR, zeroPt.y - dotR, dotR * 2.0f, dotR * 2.0f), dotR, accentCol);
        } else if (normVal > 0.003f) {
            DrawAnnularArc(canvas, center, arcRadius, arcThickness, kMinAngleRad, valAngle, accentCol);
        }
    }

    // 3. Inner circular rotor cap
    const keel::f32 rotorGap = opts.showArc ? (arcThickness * 0.5f + 3.5f) : 2.0f;
    const keel::f32 rotorR = keel::max(5.0f, (opts.showArc ? arcRadius : (outerR - tickLen - tickGap)) - rotorGap);
    const Rect rotorRect(center.x - rotorR, center.y - rotorR, rotorR * 2.0f, rotorR * 2.0f);
    const keel::f32 rotorStrokeW = focused ? 2.0f : 1.4f;
    canvas.DrawRoundRect(rotorRect, rotorR, rotorFill, rotorBorder, rotorStrokeW);

    // Subtle inner metallic rim
    if (rotorR > 9.0f) {
        const keel::f32 rimInset = 2.2f;
        const keel::f32 rimR = rotorR - rimInset;
        canvas.DrawRoundRect(
            Rect(center.x - rimR, center.y - rimR, rimR * 2.0f, rimR * 2.0f),
            rimR,
            Color::Transparent(),
            pal.borderSubtle.WithAlphaF(disabled ? 0.25f : 0.55f),
            1.0f
        );
    }

    // 4. Rotor pointer line & indicator dot
    const bool centerValueMode = hasValText && (opts.valuePlacement == Placement::Thumb);
    const keel::f32 ptrOuterR = keel::max(2.0f, rotorR - 3.0f);
    const keel::f32 ptrInnerR = centerValueMode
        ? keel::max(1.5f, rotorR * 0.68f)
        : keel::max(1.5f, rotorR * 0.28f);
    const Vec2 ptrStart(center.x + keel::cos(valAngle) * ptrInnerR, center.y + keel::sin(valAngle) * ptrInnerR);
    const Vec2 ptrEnd(center.x + keel::cos(valAngle) * ptrOuterR, center.y + keel::sin(valAngle) * ptrOuterR);
    const keel::f32 ptrWidth = 2.2f;
    canvas.DrawLine(ptrStart, ptrEnd, accentCol, ptrWidth);

    const keel::f32 tipDotR = ptrWidth * 0.65f;
    canvas.DrawRoundRect(Rect(ptrEnd.x - tipDotR, ptrEnd.y - tipDotR, tipDotR * 2.0f, tipDotR * 2.0f), tipDotR, accentCol);

    // 5. Value readout text
    if (hasValText) {
        if (centerValueMode) {
            const keel::f32 centerFont = keel::min(valFont, keel::max(8.0f, rotorR * 0.62f));
            canvas.DrawTextBox(valueText, rotorRect, centerFont, textCol, 0.5f, 0.5f);
        } else if (valRect.w > 1.0f && valRect.h > 1.0f) {
            const keel::f32 alignX = (opts.valuePlacement == Placement::Leading) ? 1.0f
                               : (opts.valuePlacement == Placement::Trailing) ? 0.0f
                               : 0.5f;
            canvas.DrawTextBox(valueText, valRect, valFont, textCol, alignX, 0.5f);
        }
    }
}

} // namespace guikit
