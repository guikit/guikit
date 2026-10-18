#include "guikit/widgets/Switch.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Element.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

Vec2 MeasureSwitchControl(
    const SwitchOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const SwitchMetrics& m = theme.GetSwitch(size);
    float ctrlW = m.trackSize.x;
    float ctrlH = m.trackSize.y;

    if (!opts.textLeft.empty() || !opts.textLeft.variants.empty()) {
        const float leftFont = theme.ResolveFontSize(opts.textLeft, TextRole::LabelMedium, size);
        const float leftW = TextUtil::MeasureWidth(opts.textLeft, leftFont, canvas);
        const float leftH = canvas ? canvas->GetLineHeight(leftFont) : theme.GetLineHeight(leftFont);
        ctrlW += leftW + m.gap;
        ctrlH = std::max(ctrlH, leftH);
    }

    if (!opts.textRight.empty() || !opts.textRight.variants.empty()) {
        const float rightFont = theme.ResolveFontSize(opts.textRight, TextRole::LabelMedium, size);
        const float rightW = TextUtil::MeasureWidth(opts.textRight, rightFont, canvas);
        const float rightH = canvas ? canvas->GetLineHeight(rightFont) : theme.GetLineHeight(rightFont);
        ctrlW += m.gap + rightW;
        ctrlH = std::max(ctrlH, rightH);
    }

    return {ctrlW, ctrlH};
}

ControlSpec EffectiveSwitchControl(const SwitchOptions& opts) noexcept {
    ControlSpec ctrl = opts.control;
    if (!ctrl.width.has_value() && opts.width.has_value()) {
        ctrl.width = opts.width;
    }
    if (!ctrl.height.has_value() && opts.height.has_value()) {
        ctrl.height = opts.height;
    }
    if (!ctrl.background.has_value()) {
        ctrl.background = false;
    }
    ctrl.stretchable = false;
    ctrl.stretchHeightWhenStandalone = false;
    ctrl.hasIntrinsicBox = false;
    return ctrl;
}

ElementSpec MakeSwitchElementSpec(
    const SwitchOptions& opts,
    UiSize size,
    bool disabled,
    const UiTheme& theme,
    bool hovered = false,
    bool active = false
) noexcept {
    const SwitchMetrics& m = theme.GetSwitch(size);
    const ControlSpec ctrl = EffectiveSwitchControl(opts);
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .icon = opts.icon,
        .badge = opts.badge,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = ctrl.width,
        .height = ctrl.height,
        .control = ctrl,
        .background = opts.background,
        .borderWidth = opts.borderWidth,
        .borderRadius = opts.borderRadius,
        .borderColor = opts.borderColor,
        .bgColor = opts.bgColor,
        .widgetSize = size,
        .labelGap = m.descGap,
        .descGap = m.descGap,
        .inlineGap = m.gap,
        .stretchableControl = false,
        .stretchHeightWhenStandalone = false,
        .disabled = disabled,
        .hovered = hovered,
        .active = active
    };
}

} // namespace

Response Switch::Run(
    Context& ctx,
    bool* value,
    bool checkedState,
    const SwitchOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();

    ctx.RegisterFocusable(id, effectiveDisabled);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.rightClicked = false;
        res.doubleClicked = false;
    } else {
        if (res.clicked) {
            ctx.RequestFocus(id);
        }
        if (ctx.IsFocused(id) && (ctx.ConsumeKey(Key::Space) || ctx.ConsumeKey(Key::Enter))) {
            res.clicked = true;
        }
    }

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    bool currentChecked = (value != nullptr) ? *value : checkedState;

    if (res.clicked && !effectiveDisabled) {
        currentChecked = !currentChecked;
        if (value != nullptr) {
            *value = currentChecked;
        }
        res.changed = true;
    }

    const ControlSpec ctrl = EffectiveSwitchControl(opts);
    Node* node = ctx.CreateNode(NodeType::Switch, id);
    SwitchOptions optsCopy = opts;
    optsCopy.label = ctx.AllocText(opts.label);
    optsCopy.textLeft = ctx.AllocText(opts.textLeft);
    optsCopy.textRight = ctx.AllocText(opts.textRight);
    optsCopy.description = ctx.AllocText(opts.description);
    optsCopy.badge = ctx.AllocText(opts.badge);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    node->text = optsCopy.label.text;
    node->SetPayload(ctx.AllocPayload<SwitchOptions>(optsCopy));
    node->visualHash = detail::HashArgs(
        opts.color,
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        opts.symmetric,
        static_cast<uint8_t>(opts.stretch),
        ctrl.width.value_or(0.0f),
        ctrl.height.value_or(0.0f),
        static_cast<uint8_t>(ctrl.align),
        opts.background,
        opts.borderWidth,
        opts.borderRadius.value_or(-1.0f),
        opts.borderColor,
        opts.bgColor,
        ctrl.HasBackground(false),
        ctrl.borderWidth,
        ctrl.borderRadius.value_or(-1.0f),
        ctrl.borderColor,
        ctrl.bgColor,
        static_cast<uint8_t>(opts.icon.GetType()),
        static_cast<uint8_t>(opts.icon.GetId()),
        opts.icon.GetPath(),
        currentChecked,
        optsCopy.label,
        optsCopy.textLeft,
        optsCopy.textRight,
        optsCopy.description,
        optsCopy.badge
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* opts = n->As<SwitchOptions>();
        return Switch::Measure(opts ? *opts : SwitchOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* opts = n->As<SwitchOptions>();
        Switch::Render(
            canvas,
            n->computedRect,
            n->checked,
            opts ? *opts : SwitchOptions{},
            n->uiSize,
            n->hovered,
            n->pressed,
            n->disabled,
            theme,
            n->focused
        );
    };
    node->checked = currentChecked;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->focused = ctx.IsFocused(id);
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Switch::Measure(
    const SwitchOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) {
    const Vec2 ctrlDims = MeasureSwitchControl(opts, size, canvas, theme);
    const ElementSpec spec = MakeSwitchElementSpec(opts, size, opts.disabled, theme);
    return Element::Measure(ctrlDims, spec, canvas, theme, availableWidth);
}

Vec2 Switch::Measure(
    const SwitchOptions& opts,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme, availableWidth);
}

void Switch::Render(
    ICanvas& canvas,
    Rect bounds,
    bool checked,
    const SwitchOptions& opts,
    UiSize size,
    bool hovered,
    bool pressed,
    bool disabled,
    const UiTheme& theme,
    bool focused
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const SwitchMetrics& m = theme.GetSwitch(size);
    const auto& pal = theme.palette;

    const Vec2 ctrlDims = MeasureSwitchControl(opts, size, &canvas, theme);
    const ElementSpec spec = MakeSwitchElementSpec(opts, size, disabled, theme, hovered, pressed || focused);
    const ElementGeometry geom = Element::Resolve(bounds, ctrlDims, spec, &canvas, theme);
    Element::RenderText(canvas, geom, spec, theme);

    const Rect& ctrlRect = geom.innerControlRect;
    float curX = ctrlRect.x + std::max(0.0f, (ctrlRect.w - ctrlDims.x) * 0.5f);
    if (!opts.textLeft.empty() || !opts.textLeft.variants.empty()) {
        const float leftFont = theme.ResolveFontSize(opts.textLeft, TextRole::LabelMedium, size);
        const float leftW = TextUtil::MeasureWidth(opts.textLeft, leftFont, &canvas);
        if (!opts.textLeft.empty()) {
            const UiVariant defaultLeftVar = checked ? UiVariant::Secondary : UiVariant::Default;
            const Color leftCol = theme.ResolveTextColor(opts.textLeft, defaultLeftVar, disabled);
            canvas.DrawTextBox(
                opts.textLeft.text,
                Rect(curX, ctrlRect.y, leftW, ctrlRect.h),
                leftFont,
                leftCol,
                1.0f,
                0.5f
            );
        }
        curX += leftW + m.gap;
    }

    const float trackY = ctrlRect.y + (ctrlRect.h - m.trackSize.y) * 0.5f;
    const Rect trackRect(curX, trackY, m.trackSize.x, m.trackSize.y);
    curX += m.trackSize.x;

    if (!opts.textRight.empty() || !opts.textRight.variants.empty()) {
        curX += m.gap;
        const float rightFont = theme.ResolveFontSize(opts.textRight, TextRole::LabelMedium, size);
        const float rightW = TextUtil::MeasureWidth(opts.textRight, rightFont, &canvas);
        if (!opts.textRight.empty()) {
            const UiVariant defaultRightVar = checked ? UiVariant::Default : UiVariant::Secondary;
            const Color rightCol = theme.ResolveTextColor(opts.textRight, defaultRightVar, disabled);
            canvas.DrawTextBox(
                opts.textRight.text,
                Rect(curX, ctrlRect.y, rightW, ctrlRect.h),
                rightFont,
                rightCol,
                0.0f,
                0.5f
            );
        }
    }

    // Determine track & knob colors
    Color trackFill;
    Color trackStroke{0, 0, 0, 0};
    float strokeWidth = 0.0f;
    Color knobFill;

    if (opts.symmetric) {
        trackFill = (hovered && !disabled) ? pal.controlTrackHover : pal.controlTrack;
        trackStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
        strokeWidth = 1.0f;
        knobFill = pal.controlKnobActive;
    } else if (checked) {
        const VariantColors& v = theme.GetVariant(UiVariant::Primary);
        if (!opts.color.HasAny()) {
            trackFill = (hovered && !disabled) ? v.bgHover : v.bg;
            trackStroke = (v.borderWidth > 0.0f) ? v.border : Color::Transparent();
            strokeWidth = v.borderWidth;
            knobFill = pal.controlKnobActive;
        } else if (opts.color.normal.has_value() && opts.color.normal->a == 0) {
            trackFill = (hovered && !disabled) ? opts.color.hover.value_or(theme.GetVariant(UiVariant::Ghost).bgHover) : Color::Transparent();
            trackStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
            strokeWidth = 1.0f;
            knobFill = pal.controlKnobActive;
        } else {
            trackFill = opts.color.Resolve(hovered && !disabled, pressed && !disabled, false, v.bg, v.bgHover, v.bgActive, v.bg);
            trackStroke = Color::Transparent();
            strokeWidth = 0.0f;
            knobFill = pal.controlKnobActive;
        }
    } else {
        trackFill = (hovered && !disabled) ? pal.controlTrackHover : pal.controlTrack;
        trackStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
        strokeWidth = 1.0f;
        knobFill = (hovered && !disabled) ? pal.controlKnobHover : pal.controlKnob;
    }

    if (disabled) {
        trackFill = trackFill.WithAlphaF(0.40f);
        trackStroke = trackStroke.WithAlphaF(0.40f);
        knobFill = knobFill.WithAlphaF(0.40f);
    } else if (focused) {
        trackStroke = pal.focusRing;
        strokeWidth = std::max(strokeWidth, 2.0f);
    }

    // Draw track
    const float trackRadius = m.trackSize.y * 0.5f;
    canvas.DrawRoundRect(trackRect, trackRadius, trackFill, trackStroke, strokeWidth);

    // Compute knob position
    const float knobY = trackRect.y + m.knobPadding;
    float knobX = 0.0f;
    if (checked) {
        knobX = trackRect.x + trackRect.w - m.knobDiameter - m.knobPadding;
    } else {
        knobX = trackRect.x + m.knobPadding;
    }
    const Rect knobRect(knobX, knobY, m.knobDiameter, m.knobDiameter);
    canvas.DrawRoundRect(knobRect, m.knobDiameter * 0.5f, knobFill);
}

} // namespace guikit
