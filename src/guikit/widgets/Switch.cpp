#include "guikit/widgets/Switch.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
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

ControlSpec EffectiveSwitchControl(const SwitchOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SwitchMetrics& m = theme.GetSwitch(size);
    ControlSpec ctrl{};
    ctrl.width = opts.width;
    ctrl.height = opts.height;
    ctrl.align = opts.align;
    ctrl.background = opts.background;
    ctrl.border = opts.border;
    ctrl.defaultLayout = ElementLayout::Inline;
    ctrl.defaultInlineOrder = InlineOrder::ControlFirst;
    ctrl.labelGap = m.descGap;
    ctrl.descGap = m.descGap;
    ctrl.inlineGap = m.gap;
    ctrl.stretchable = false;
    ctrl.stretchHeightWhenStandalone = false;
    ctrl.hasIntrinsicBox = false;
    return ctrl;
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
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();

    ctx.RegisterFocusable(id, effectiveDisabled);

    Response res = ctx.Interact(id);
    Control::ForwardParentElementInteraction(ctx, effectiveDisabled, res);
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

    SwitchOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);
    optsCopy.textLeft = ctx.AllocText(opts.textLeft);
    optsCopy.textRight = ctx.AllocText(opts.textRight);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);

    const ControlSpec ctrl = EffectiveSwitchControl(optsCopy, activeSize, ctx.GetTheme());
    Node* node = ctx.CreateNode(NodeType::Switch, id);
    node->text = !optsCopy.textRight.empty() ? optsCopy.textRight.text : optsCopy.textLeft.text;
    node->SetPayload(ctx.AllocPayload<SwitchOptions>(optsCopy));
    node->visualHash = detail::HashArgs(
        optsCopy.color,
        static_cast<uint8_t>(optsCopy.align),
        optsCopy.symmetric,
        static_cast<uint8_t>(optsCopy.stretch),
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.background,
        optsCopy.border,
        currentChecked,
        optsCopy.textLeft,
        optsCopy.textRight
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
    node->widthMode = (optsCopy.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);
    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Switch::Measure(
    const SwitchOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const Vec2 ctrlDims = MeasureSwitchControl(opts, size, canvas, theme);
    const ControlSpec ctrl = EffectiveSwitchControl(opts, size, theme);
    return Control::Measure(ctrlDims, ctrl, opts.stretch, size, theme);
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
    const ControlSpec ctrl = EffectiveSwitchControl(opts, size, theme);
    Control::RenderFrame(canvas, bounds, ctrl, size, disabled, theme, hovered, pressed || focused);

    const Rect ctrlRect = Control::ResolveInnerRect(bounds, ctrl, size, theme);
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
