#include "guikit/widgets/Knob.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kMinAngleRad = -1.25f * kPi; // -225 deg (7:30 o'clock)
constexpr float kMaxAngleRad =  0.25f * kPi; //  +45 deg (4:30 o'clock)
constexpr float kSweepRad    = kMaxAngleRad - kMinAngleRad; // 270 deg (1.5 * pi)

float GetDefaultKnobDiameter(UiSize size, const UiTheme& theme) noexcept {
    switch (size) {
        case UiSize::Xs: return theme.Scale(38.0f);
        case UiSize::Sm: return theme.Scale(46.0f);
        case UiSize::Lg: return theme.Scale(66.0f);
        case UiSize::Xl: return theme.Scale(78.0f);
        case UiSize::Md:
        default:         return theme.Scale(54.0f);
    }
}

ControlSpec EffectiveKnobControl(const KnobOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SliderMetrics& m = theme.GetSlider(size);
    ControlSpec c{};
    c.width = opts.width;
    c.height = opts.height;
    c.align = opts.align;
    c.background = opts.background;
    c.border = opts.border;
    c.defaultLayout = ElementLayout::Inline;
    c.defaultInlineOrder = InlineOrder::LabelFirst;
    c.labelGap = m.labelGap;
    c.descGap = m.descGap;
    c.inlineGap = m.labelGap;
    c.stretchable = false;
    c.stretchHeightWhenStandalone = false;
    c.hasIntrinsicBox = false;
    return c;
}

double QuantizeAndClamp(double val, double minVal, double maxVal, std::optional<double> step) noexcept {
    if (maxVal < minVal) std::swap(minVal, maxVal);
    val = std::clamp(val, minVal, maxVal);
    if (step.has_value() && *step > 0.0) {
        const double steps = std::round((val - minVal) / *step);
        val = std::clamp(minVal + steps * (*step), minVal, maxVal);
    }
    return val;
}

void FormatKnobValue(
    char* buf,
    size_t bufSize,
    double val,
    bool isInt,
    const KnobOptions& opts
) noexcept {
    if (!opts.valueFormat.empty()) {
        if (opts.valueFormat.text.find('%') == std::string_view::npos) {
            std::snprintf(buf, bufSize, "%.*s",
                          static_cast<int>(opts.valueFormat.text.size()),
                          opts.valueFormat.text.data());
        } else if (isInt && opts.valueFormat.text.find('f') == std::string_view::npos) {
            std::snprintf(buf, bufSize, opts.valueFormat.text.data(), static_cast<int>(std::llround(val)));
        } else {
            std::snprintf(buf, bufSize, opts.valueFormat.text.data(), val);
        }
    } else if (isInt) {
        std::snprintf(buf, bufSize, "%d", static_cast<int>(std::llround(val)));
    } else {
        const double step = opts.step.value_or(0.0);
        if (step >= 1.0) {
            std::snprintf(buf, bufSize, "%.0f", val);
        } else if (step >= 0.1) {
            std::snprintf(buf, bufSize, "%.1f", val);
        } else {
            std::snprintf(buf, bufSize, "%.2f", val);
        }
    }
}

void DrawAnnularArc(
    ICanvas& canvas,
    Vec2 center,
    float radius,
    float thickness,
    float startAngleRad,
    float endAngleRad,
    Color color
) {
    if (color.a == 0 || radius <= 0.5f || thickness <= 0.2f) return;
    if (std::abs(endAngleRad - startAngleRad) < 1e-4f) return;

    if (endAngleRad < startAngleRad) {
        std::swap(startAngleRad, endAngleRad);
    }

    const float halfT = thickness * 0.5f;
    const float rOuter = radius + halfT;
    const float rInner = std::max(0.5f, radius - halfT);
    const float span = endAngleRad - startAngleRad;
    const int segs = std::clamp(static_cast<int>(std::ceil(span / (kPi / 24.0f))), 6, 64);

    VectorPath path;
    path.Reserve(static_cast<size_t>(segs * 2 + 4), static_cast<size_t>(segs * 2 + 4));

    for (int i = 0; i <= segs; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segs);
        const float a = startAngleRad + span * t;
        const Vec2 pt(center.x + std::cos(a) * rOuter, center.y + std::sin(a) * rOuter);
        if (i == 0) path.MoveTo(pt);
        else        path.LineTo(pt);
    }
    for (int i = segs; i >= 0; --i) {
        const float t = static_cast<float>(i) / static_cast<float>(segs);
        const float a = startAngleRad + span * t;
        const Vec2 pt(center.x + std::cos(a) * rInner, center.y + std::sin(a) * rInner);
        path.LineTo(pt);
    }
    path.Close();

    PathPaint paint{};
    paint.fillColor = color;
    paint.renderFill = true;
    canvas.DrawPath(path, paint);

    // Draw rounded caps at start and end of the arc
    const Vec2 capStart(center.x + std::cos(startAngleRad) * radius, center.y + std::sin(startAngleRad) * radius);
    const Vec2 capEnd(center.x + std::cos(endAngleRad) * radius, center.y + std::sin(endAngleRad) * radius);
    canvas.DrawRoundRect(Rect(capStart.x - halfT, capStart.y - halfT, thickness, thickness), halfT, color);
    canvas.DrawRoundRect(Rect(capEnd.x - halfT, capEnd.y - halfT, thickness, thickness), halfT, color);
}

} // namespace

Response Knob::Run(
    Context& ctx,
    double* doubleVal,
    float* floatVal,
    int* intVal,
    double minVal,
    double maxVal,
    const KnobOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool isInt = (intVal != nullptr);

    if (maxVal < minVal) {
        std::swap(minVal, maxVal);
    }
    const double span = std::max(1e-9, maxVal - minVal);
    const std::optional<double> effectiveStep = opts.step.has_value()
        ? opts.step
        : (isInt ? std::optional<double>(1.0) : std::nullopt);

    auto readVal = [&]() noexcept -> double {
        if (doubleVal != nullptr) return *doubleVal;
        if (floatVal != nullptr)  return static_cast<double>(*floatVal);
        if (intVal != nullptr)    return static_cast<double>(*intVal);
        return minVal;
    };

    auto writeVal = [&](double v) noexcept {
        const double q = QuantizeAndClamp(v, minVal, maxVal, effectiveStep);
        if (doubleVal != nullptr) *doubleVal = q;
        if (floatVal != nullptr)  *floatVal = static_cast<float>(q);
        if (intVal != nullptr)    *intVal = static_cast<int>(std::llround(q));
    };

    double currentVal = QuantizeAndClamp(readVal(), minVal, maxVal, effectiveStep);
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
            const double targetVal = opts.defaultValue.has_value()
                ? *opts.defaultValue
                : ((minVal < 0.0 && maxVal > 0.0) ? 0.0 : (minVal + maxVal) * 0.5);
            const double prev = currentVal;
            currentVal = QuantizeAndClamp(targetVal, minVal, maxVal, effectiveStep);
            writeVal(currentVal);
            state.dragging = false;
            if (std::abs(currentVal - prev) > 1e-7) {
                res.changed = true;
            }
        } else if (mouseJustPressed) {
            state.dragging = true;
            state.dragStartVal = currentVal;
            state.dragStartMouse = ctx.GetMousePos();
        }

        if (state.dragging) {
            if (ctx.IsMouseDown()) {
                const Vec2 mousePos = ctx.GetMousePos();
                const float dy = state.dragStartMouse.y - mousePos.y;
                const float dx = mousePos.x - state.dragStartMouse.x;
                const float netDragPx = dy + dx * 0.35f;
                const float sensitivityPx = ctx.GetKeyModifiers().shift ? 600.0f : 150.0f;
                const double deltaNorm = static_cast<double>(netDragPx / sensitivityPx);
                const double prev = currentVal;
                currentVal = QuantizeAndClamp(state.dragStartVal + deltaNorm * span, minVal, maxVal, effectiveStep);
                writeVal(currentVal);
                res.pressed = true;
                if (std::abs(currentVal - prev) > 1e-7) {
                    res.changed = true;
                }
            } else {
                state.dragging = false;
            }
        }

        // Mouse wheel and keyboard navigation
        const double keyStep = effectiveStep.value_or(span * 0.02);
        double stepDelta = 0.0;
        if (res.hovered && std::abs(ctx.GetMouseWheel().y) > 0.001f) {
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
            const double prev = currentVal;
            currentVal = QuantizeAndClamp(currentVal + stepDelta, minVal, maxVal, effectiveStep);
            writeVal(currentVal);
            if (std::abs(currentVal - prev) > 1e-7) {
                res.changed = true;
            }
        }
    }

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    KnobOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    optsCopy.valueFormat = ctx.AllocText(opts.valueFormat);

    char valBuf[64]{};
    if (optsCopy.showValue) {
        FormatKnobValue(valBuf, sizeof(valBuf), currentVal, isInt, optsCopy);
    }

    const float normVal = static_cast<float>(std::clamp((currentVal - minVal) / span, 0.0, 1.0));
    float normOrigin = 0.0f;
    if (optsCopy.bipolar) {
        if (minVal < 0.0 && maxVal > 0.0) {
            normOrigin = static_cast<float>(std::clamp((0.0 - minVal) / span, 0.0, 1.0));
        } else {
            normOrigin = 0.5f;
        }
    }

    const UiTheme& theme = ctx.GetTheme();
    const ControlSpec ctrl = EffectiveKnobControl(optsCopy, activeSize, theme);
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
        static_cast<uint8_t>(optsCopy.align),
        static_cast<uint8_t>(optsCopy.stretch),
        optsCopy.showValue,
        static_cast<uint8_t>(optsCopy.valuePlacement),
        optsCopy.bipolar,
        optsCopy.showTicks,
        optsCopy.tickCount,
        optsCopy.showArc,
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.background,
        optsCopy.border,
        data->normalizedValue,
        data->normalizedOrigin,
        data->valueText,
        data->active
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<KnobData>();
        return Knob::Measure(
            d ? d->opts : KnobOptions{},
            d ? d->valueText : std::string_view{},
            n->uiSize,
            canvas,
            theme,
            availableWidth
        );
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<KnobData>();
        Knob::Render(
            canvas,
            n->computedRect,
            d ? d->normalizedValue : 0.0f,
            d ? d->normalizedOrigin : 0.0f,
            d ? d->valueText : std::string_view{},
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
    node->focused = ctx.IsFocused(id);
    node->widthMode = (optsCopy.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);
    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Knob::Measure(
    const KnobOptions& opts,
    std::string_view valueText,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const float dialDia = GetDefaultKnobDiameter(size, theme);
    const ControlSpec ctrl = EffectiveKnobControl(opts, size, theme);

    float w = dialDia;
    float h = dialDia;

    if (opts.showValue && (!valueText.empty() || !opts.valueFormat.variants.empty())) {
        const float valFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelSmall, size);
        float textW = valueText.empty() ? 0.0f : TextUtil::MeasureWidth(valueText, valFont, canvas);
        for (std::string_view v : opts.valueFormat.variants) {
            if (!v.empty()) {
                textW = std::max(textW, TextUtil::MeasureWidth(v, valFont, canvas));
            }
        }
        textW = std::max(textW + theme.Scale(4.0f), theme.Scale(32.0f));
        const float textH = valFont * 1.25f;
        const float gap = theme.Scale(4.0f);

        switch (opts.valuePlacement) {
            case Placement::Top:
            case Placement::Bottom:
                w = std::max(w, textW);
                h += gap + textH;
                break;
            case Placement::Leading:
            case Placement::Trailing:
                w += gap + textW;
                h = std::max(h, textH);
                break;
            case Placement::Thumb:
                break;
        }
    }

    return Control::Measure({w, h}, ctrl, opts.stretch, size, theme);
}

Vec2 Knob::Measure(
    const KnobOptions& opts,
    std::string_view valueText,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, valueText, size, canvas, defaultTheme, availableWidth);
}

void Knob::Render(
    ICanvas& canvas,
    Rect bounds,
    float normalizedValue,
    float normalizedOrigin,
    std::string_view valueText,
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

    const ControlSpec ctrl = EffectiveKnobControl(opts, size, theme);
    Control::RenderFrame(canvas, bounds, ctrl, size, disabled, theme, hovered, active || focused);
    const Rect innerRect = Control::ResolveInnerRect(bounds, ctrl, size, theme);
    if (innerRect.w <= 2.0f || innerRect.h <= 2.0f) {
        return;
    }

    const auto& pal = theme.palette;
    const float valFont = theme.ResolveFontSize(opts.valueFormat, TextRole::LabelSmall, size);
    const bool hasValText = opts.showValue && !valueText.empty();

    Rect dialArea = innerRect;
    Rect valRect{};

    if (hasValText && opts.valuePlacement != Placement::Thumb) {
        const float textH = valFont * 1.25f;
        const float gap = theme.Scale(3.0f);
        if (opts.valuePlacement == Placement::Bottom) {
            const float dialH = std::max(16.0f, innerRect.h - textH - gap);
            dialArea = Rect(innerRect.x, innerRect.y, innerRect.w, dialH);
            valRect = Rect(innerRect.x, innerRect.y + dialH + gap, innerRect.w, textH);
        } else if (opts.valuePlacement == Placement::Top) {
            const float dialH = std::max(16.0f, innerRect.h - textH - gap);
            valRect = Rect(innerRect.x, innerRect.y, innerRect.w, textH);
            dialArea = Rect(innerRect.x, innerRect.y + textH + gap, innerRect.w, dialH);
        } else if (opts.valuePlacement == Placement::Leading) {
            float textW = TextUtil::MeasureWidth(valueText, valFont, &canvas) + theme.Scale(4.0f);
            for (std::string_view v : opts.valueFormat.variants) {
                if (!v.empty()) textW = std::max(textW, TextUtil::MeasureWidth(v, valFont, &canvas) + theme.Scale(4.0f));
            }
            const float dialW = std::max(16.0f, innerRect.w - textW - gap);
            valRect = Rect(innerRect.x, innerRect.y, textW, innerRect.h);
            dialArea = Rect(innerRect.x + textW + gap, innerRect.y, dialW, innerRect.h);
        } else if (opts.valuePlacement == Placement::Trailing) {
            float textW = TextUtil::MeasureWidth(valueText, valFont, &canvas) + theme.Scale(4.0f);
            for (std::string_view v : opts.valueFormat.variants) {
                if (!v.empty()) textW = std::max(textW, TextUtil::MeasureWidth(v, valFont, &canvas) + theme.Scale(4.0f));
            }
            const float dialW = std::max(16.0f, innerRect.w - textW - gap);
            dialArea = Rect(innerRect.x, innerRect.y, dialW, innerRect.h);
            valRect = Rect(innerRect.x + dialW + gap, innerRect.y, textW, innerRect.h);
        }
    }

    const float dialDiameter = std::min(dialArea.w, dialArea.h);
    const Vec2 center(
        dialArea.x + dialArea.w * 0.5f,
        dialArea.y + dialArea.h * 0.5f
    );
    const float outerR = std::max(6.0f, dialDiameter * 0.5f - theme.Scale(1.5f));

    // Resolve colors
    const VariantColors& primaryVar = theme.GetVariant(UiVariant::Primary);
    Color accentCol;
    if (!opts.color.HasAny()) {
        accentCol = active ? primaryVar.bgActive : (hovered ? primaryVar.bgHover : primaryVar.bg);
    } else {
        accentCol = opts.color.Resolve(hovered && !disabled, active && !disabled, disabled, primaryVar.bg, primaryVar.bgHover, primaryVar.bgActive, primaryVar.bg);
    }

    Color trackCol = (hovered && !disabled) ? pal.controlTrackHover : pal.controlTrack;
    Color rotorFill = (active && !disabled) ? pal.surfaceHover : ((hovered && !disabled) ? pal.surfaceElevated : pal.surface);
    Color rotorBorder = focused ? pal.focusRing : ((hovered || active) && !disabled ? pal.borderHover : pal.border);
    Color tickMutedCol = pal.borderHover;
    Color textCol = opts.valueFormat.color.HasAny()
        ? theme.ResolveTextColor(opts.valueFormat, UiVariant::Default, disabled, hovered, active)
        : (active && !disabled ? accentCol : pal.textSecondary);

    if (disabled) {
        accentCol = accentCol.WithAlphaF(0.40f);
        trackCol = trackCol.WithAlphaF(0.40f);
        rotorFill = rotorFill.WithAlphaF(0.40f);
        rotorBorder = rotorBorder.WithAlphaF(0.40f);
        tickMutedCol = tickMutedCol.WithAlphaF(0.35f);
        textCol = textCol.WithAlphaF(0.40f);
    }

    const float normVal = std::clamp(normalizedValue, 0.0f, 1.0f);
    const float normOrig = std::clamp(normalizedOrigin, 0.0f, 1.0f);
    const float valAngle = kMinAngleRad + kSweepRad * normVal;
    const float origAngle = kMinAngleRad + kSweepRad * normOrig;

    const float tickLen = opts.showTicks ? std::max(2.5f, outerR * 0.13f) : 0.0f;
    const float tickGap = opts.showTicks ? theme.Scale(2.0f) : 0.0f;
    const float arcThickness = std::clamp(outerR * 0.14f, theme.Scale(2.5f), theme.Scale(5.5f));
    const float arcRadius = outerR - tickLen - tickGap - arcThickness * 0.5f;

    // 1. Radial scale ticks around outer perimeter
    if (opts.showTicks && outerR >= 12.0f) {
        const uint32_t numTicks = std::clamp(opts.tickCount == 0 ? 11u : opts.tickCount, 2u, 31u);
        const float rTickOuter = outerR;
        const float rTickInner = outerR - tickLen;
        const float loT = std::min(normOrig, normVal);
        const float hiT = std::max(normOrig, normVal);

        for (uint32_t i = 0; i < numTicks; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(numTicks - 1);
            const float a = kMinAngleRad + kSweepRad * t;
            const bool isMajor = (i == 0) || (i + 1 == numTicks) || (numTicks % 2 == 1 && i == numTicks / 2);
            const float innerR = isMajor ? (rTickInner - theme.Scale(1.0f)) : rTickInner;
            const bool isTickLit = opts.bipolar
                ? (t >= loT - 0.005f && t <= hiT + 0.005f)
                : (t <= normVal + 0.005f);
            const Color tCol = isTickLit ? accentCol : tickMutedCol;
            const float tStroke = isMajor ? theme.Scale(1.6f) : theme.Scale(1.1f);

            const Vec2 p0(center.x + std::cos(a) * innerR, center.y + std::sin(a) * innerR);
            const Vec2 p1(center.x + std::cos(a) * rTickOuter, center.y + std::sin(a) * rTickOuter);
            canvas.DrawLine(p0, p1, tCol, tStroke);
        }
    }

    // 2. Outer track arc and active value arc
    if (opts.showArc && arcRadius > 4.0f) {
        DrawAnnularArc(canvas, center, arcRadius, arcThickness, kMinAngleRad, kMaxAngleRad, trackCol);

        if (opts.bipolar) {
            if (std::abs(valAngle - origAngle) > 0.01f) {
                DrawAnnularArc(canvas, center, arcRadius, arcThickness, origAngle, valAngle, accentCol);
            }
            // Center zero marker dot on arc
            const Vec2 zeroPt(center.x + std::cos(origAngle) * arcRadius, center.y + std::sin(origAngle) * arcRadius);
            const float dotR = arcThickness * 0.55f;
            canvas.DrawRoundRect(Rect(zeroPt.x - dotR, zeroPt.y - dotR, dotR * 2.0f, dotR * 2.0f), dotR, accentCol);
        } else if (normVal > 0.003f) {
            DrawAnnularArc(canvas, center, arcRadius, arcThickness, kMinAngleRad, valAngle, accentCol);
        }
    }

    // 3. Inner circular rotor cap
    const float rotorGap = opts.showArc ? (arcThickness * 0.5f + theme.Scale(3.5f)) : theme.Scale(2.0f);
    const float rotorR = std::max(5.0f, (opts.showArc ? arcRadius : (outerR - tickLen - tickGap)) - rotorGap);
    const Rect rotorRect(center.x - rotorR, center.y - rotorR, rotorR * 2.0f, rotorR * 2.0f);
    const float rotorStrokeW = focused ? theme.Scale(2.0f) : theme.Scale(1.4f);
    canvas.DrawRoundRect(rotorRect, rotorR, rotorFill, rotorBorder, rotorStrokeW);

    // Subtle inner metallic rim
    if (rotorR > 9.0f) {
        const float rimInset = theme.Scale(2.2f);
        const float rimR = rotorR - rimInset;
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
    const float ptrOuterR = std::max(2.0f, rotorR - theme.Scale(3.0f));
    const float ptrInnerR = centerValueMode
        ? std::max(1.5f, rotorR * 0.68f)
        : std::max(1.5f, rotorR * 0.28f);
    const Vec2 ptrStart(center.x + std::cos(valAngle) * ptrInnerR, center.y + std::sin(valAngle) * ptrInnerR);
    const Vec2 ptrEnd(center.x + std::cos(valAngle) * ptrOuterR, center.y + std::sin(valAngle) * ptrOuterR);
    const float ptrWidth = std::max(1.8f, theme.Scale(2.2f));
    canvas.DrawLine(ptrStart, ptrEnd, accentCol, ptrWidth);

    const float tipDotR = ptrWidth * 0.65f;
    canvas.DrawRoundRect(Rect(ptrEnd.x - tipDotR, ptrEnd.y - tipDotR, tipDotR * 2.0f, tipDotR * 2.0f), tipDotR, accentCol);

    // 5. Value readout text
    if (hasValText) {
        if (centerValueMode) {
            const float centerFont = std::min(valFont, std::max(8.0f, rotorR * 0.62f));
            canvas.DrawTextBox(valueText, rotorRect, centerFont, textCol, 0.5f, 0.5f);
        } else if (valRect.w > 1.0f && valRect.h > 1.0f) {
            const float alignX = (opts.valuePlacement == Placement::Leading) ? 1.0f
                               : (opts.valuePlacement == Placement::Trailing) ? 0.0f
                               : 0.5f;
            canvas.DrawTextBox(valueText, valRect, valFont, textCol, alignX, 0.5f);
        }
    }
}

} // namespace guikit
