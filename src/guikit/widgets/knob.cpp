#include "guikit/widgets/knob.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"

namespace guikit {

namespace {

constexpr float kPi = 3.14159265358979323846f;
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
    c.widthRatio = opts.widthRatio;
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

double QuantizeAndClamp(double val, double minVal, double maxVal, keel::Optional<double> step) noexcept {
    if (maxVal < minVal) keel::swap(minVal, maxVal);
    val = keel::clamp(val, minVal, maxVal);
    if (step.has_value() && *step > 0.0) {
        const double steps = std::round((val - minVal) / *step);
        val = keel::clamp(minVal + steps * (*step), minVal, maxVal);
    }
    return val;
}

void FormatKnobValue(
    char* buf,
    keel::usize bufSize,
    double val,
    bool isInt,
    const KnobOptions& opts
) noexcept {
    if (!opts.valueFormat.empty()) {
        if (opts.valueFormat.text.find('%') == keel::StringRef::npos) {
            std::snprintf(buf, bufSize, "%.*s",
                          static_cast<keel::i32>(opts.valueFormat.text.size()),
                          opts.valueFormat.text.data());
        } else {
            char fmtBuf[128]{};
            const keel::usize copyLen = keel::min(opts.valueFormat.text.size(), sizeof(fmtBuf) - 1);
            std::memcpy(fmtBuf, opts.valueFormat.text.data(), copyLen);
            fmtBuf[copyLen] = '\0';
            if (isInt && opts.valueFormat.text.find('f') == keel::StringRef::npos) {
                if (opts.valueFormat.text.find("ll") != keel::StringRef::npos ||
                    opts.valueFormat.text.find('z') != keel::StringRef::npos) {
                    std::snprintf(buf, bufSize, fmtBuf, static_cast<long long>(std::llround(val)));
                } else {
                    std::snprintf(buf, bufSize, fmtBuf, static_cast<int>(std::llround(val)));
                }
            } else {
                std::snprintf(buf, bufSize, fmtBuf, val);
            }
        }
    } else if (isInt) {
        std::snprintf(buf, bufSize, "%lld", static_cast<long long>(std::llround(val)));
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
        keel::swap(startAngleRad, endAngleRad);
    }

    const float halfT = thickness * 0.5f;
    const float rOuter = radius + halfT;
    const float rInner = keel::max(0.5f, radius - halfT);
    const float span = endAngleRad - startAngleRad;
    const keel::i32 segs = keel::clamp(static_cast<keel::i32>(std::ceil(span / (kPi / 24.0f))), 6, 64);

    VectorPath path;
    path.Reserve(static_cast<keel::usize>(segs * 2 + 4), static_cast<keel::usize>(segs * 2 + 4));

    for (keel::i32 i = 0; i <= segs; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(segs);
        const float a = startAngleRad + span * t;
        const Vec2 pt(center.x + std::cos(a) * rOuter, center.y + std::sin(a) * rOuter);
        if (i == 0) path.MoveTo(pt);
        else        path.LineTo(pt);
    }
    for (keel::i32 i = segs; i >= 0; --i) {
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
    keel::i64* intVal,
    double minVal,
    double maxVal,
    const KnobOptions& opts,
    keel::SourceLocation loc
) {
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    struct ThemePopGuard {
        Context& c;
        UiTheme* t;
        ~ThemePopGuard() noexcept { if (t) c.PopTheme(); }
    } themeGuard{ctx, pushedTheme};

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool isInt = (intVal != nullptr);

    if (maxVal < minVal) {
        keel::swap(minVal, maxVal);
    }
    const double span = keel::max(1e-9, maxVal - minVal);
    const keel::Optional<double> effectiveStep = opts.step.has_value()
        ? opts.step
        : (isInt ? keel::Optional<double>(1.0) : keel::kNullOpt);

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
        if (intVal != nullptr)    *intVal = static_cast<keel::i64>(std::llround(q));
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
            ctx.SetActiveId(id);
        }

        if (state.dragging) {
            if (ctx.IsMouseDown()) {
                ctx.SetActiveId(id);
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
                if (ctx.GetActiveId() == id) {
                    ctx.SetActiveId(Id::Null());
                }
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
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.widthRatio);
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

    const float normVal = static_cast<float>(keel::clamp((currentVal - minVal) / span, 0.0, 1.0));
    float normOrigin = 0.0f;
    if (optsCopy.bipolar) {
        if (minVal < 0.0 && maxVal > 0.0) {
            normOrigin = static_cast<float>(keel::clamp((0.0 - minVal) / span, 0.0, 1.0));
        } else {
            normOrigin = 0.5f;
        }
    }

    const UiTheme& theme = ctx.GetTheme();
    const ControlSpec ctrl = EffectiveKnobControl(optsCopy, activeSize, theme);
    Node* node = ctx.CreateNode(NodeType::Knob, id);
    node->theme = pushedTheme;

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
        static_cast<keel::u8>(optsCopy.align),
        static_cast<keel::u8>(optsCopy.stretch),
        optsCopy.showValue,
        static_cast<keel::u8>(optsCopy.valuePlacement),
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
            d ? d->valueText : keel::StringRef{},
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
    node->focused = ctx.IsFocused(id);
    node->widthMode = (optsCopy.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);
    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Knob::Measure(
    const KnobOptions& opts,
    keel::StringRef valueText,
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
        for (keel::StringRef v : opts.valueFormat.variants) {
            if (!v.empty()) {
                textW = keel::max(textW, TextUtil::MeasureWidth(v, valFont, canvas));
            }
        }
        textW = keel::max(textW + theme.Scale(4.0f), theme.Scale(32.0f));
        const float textH = valFont * 1.25f;
        const float gap = theme.Scale(4.0f);

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

    return Control::Measure({w, h}, ctrl, opts.stretch, size, theme);
}

Vec2 Knob::Measure(
    const KnobOptions& opts,
    keel::StringRef valueText,
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
        const float gap = theme.Scale(4.0f);
        if (opts.valuePlacement == Placement::Bottom) {
            const float dialH = keel::max(16.0f, innerRect.h - textH - gap);
            dialArea = Rect(innerRect.x, innerRect.y, innerRect.w, dialH);
            valRect = Rect(innerRect.x, innerRect.y + dialH + gap, innerRect.w, textH);
        } else if (opts.valuePlacement == Placement::Top) {
            const float dialH = keel::max(16.0f, innerRect.h - textH - gap);
            valRect = Rect(innerRect.x, innerRect.y, innerRect.w, textH);
            dialArea = Rect(innerRect.x, innerRect.y + textH + gap, innerRect.w, dialH);
        } else if (opts.valuePlacement == Placement::Leading) {
            float textW = TextUtil::MeasureWidth(valueText, valFont, &canvas) + theme.Scale(4.0f);
            for (keel::StringRef v : opts.valueFormat.variants) {
                if (!v.empty()) textW = keel::max(textW, TextUtil::MeasureWidth(v, valFont, &canvas) + theme.Scale(4.0f));
            }
            textW = keel::max(textW, theme.Scale(32.0f));
            const float dialW = keel::max(16.0f, innerRect.w - textW - gap);
            valRect = Rect(innerRect.x, innerRect.y, textW, innerRect.h);
            dialArea = Rect(innerRect.x + textW + gap, innerRect.y, dialW, innerRect.h);
        } else if (opts.valuePlacement == Placement::Trailing) {
            float textW = TextUtil::MeasureWidth(valueText, valFont, &canvas) + theme.Scale(4.0f);
            for (keel::StringRef v : opts.valueFormat.variants) {
                if (!v.empty()) textW = keel::max(textW, TextUtil::MeasureWidth(v, valFont, &canvas) + theme.Scale(4.0f));
            }
            textW = keel::max(textW, theme.Scale(32.0f));
            const float dialW = keel::max(16.0f, innerRect.w - textW - gap);
            dialArea = Rect(innerRect.x, innerRect.y, dialW, innerRect.h);
            valRect = Rect(innerRect.x + dialW + gap, innerRect.y, textW, innerRect.h);
        }
    }

    const float defaultDia = GetDefaultKnobDiameter(size, theme);
    const float dialDiameter = (opts.width.has_value() || opts.height.has_value())
        ? keel::min(dialArea.w, dialArea.h)
        : keel::min(keel::min(dialArea.w, dialArea.h), defaultDia);
    const Vec2 center(
        dialArea.x + dialArea.w * 0.5f,
        dialArea.y + dialArea.h * 0.5f
    );
    const float outerR = keel::max(6.0f, dialDiameter * 0.5f - theme.Scale(1.5f));

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
        accentCol = accentCol.WithAlphaF(0.40f);
        trackCol = trackCol.WithAlphaF(0.40f);
        rotorFill = rotorFill.WithAlphaF(0.40f);
        rotorBorder = rotorBorder.WithAlphaF(0.40f);
        tickMutedCol = tickMutedCol.WithAlphaF(0.35f);
        textCol = textCol.WithAlphaF(0.40f);
    }

    const float normVal = keel::clamp(normalizedValue, 0.0f, 1.0f);
    const float normOrig = keel::clamp(normalizedOrigin, 0.0f, 1.0f);
    const float valAngle = kMinAngleRad + kSweepRad * normVal;
    const float origAngle = kMinAngleRad + kSweepRad * normOrig;

    const float tickLen = opts.showTicks ? keel::max(2.5f, outerR * 0.13f) : 0.0f;
    const float tickGap = opts.showTicks ? theme.Scale(2.0f) : 0.0f;
    const float arcThickness = keel::clamp(outerR * 0.14f, theme.Scale(2.5f), theme.Scale(5.5f));
    const float arcRadius = outerR - tickLen - tickGap - arcThickness * 0.5f;

    // 1. Radial scale ticks around outer perimeter
    if (opts.showTicks && outerR >= 12.0f) {
        const keel::u32 numTicks = keel::clamp(opts.tickCount == 0 ? 11u : opts.tickCount, 2u, 31u);
        const float rTickOuter = outerR;
        const float rTickInner = outerR - tickLen;
        const float loT = keel::min(normOrig, normVal);
        const float hiT = keel::max(normOrig, normVal);

        for (keel::u32 i = 0; i < numTicks; ++i) {
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
    const float rotorR = keel::max(5.0f, (opts.showArc ? arcRadius : (outerR - tickLen - tickGap)) - rotorGap);
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
    const float ptrOuterR = keel::max(2.0f, rotorR - theme.Scale(3.0f));
    const float ptrInnerR = centerValueMode
        ? keel::max(1.5f, rotorR * 0.68f)
        : keel::max(1.5f, rotorR * 0.28f);
    const Vec2 ptrStart(center.x + std::cos(valAngle) * ptrInnerR, center.y + std::sin(valAngle) * ptrInnerR);
    const Vec2 ptrEnd(center.x + std::cos(valAngle) * ptrOuterR, center.y + std::sin(valAngle) * ptrOuterR);
    const float ptrWidth = keel::max(1.8f, theme.Scale(2.2f));
    canvas.DrawLine(ptrStart, ptrEnd, accentCol, ptrWidth);

    const float tipDotR = ptrWidth * 0.65f;
    canvas.DrawRoundRect(Rect(ptrEnd.x - tipDotR, ptrEnd.y - tipDotR, tipDotR * 2.0f, tipDotR * 2.0f), tipDotR, accentCol);

    // 5. Value readout text
    if (hasValText) {
        if (centerValueMode) {
            const float centerFont = keel::min(valFont, keel::max(8.0f, rotorR * 0.62f));
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
