#include "guikit/controls/switch.hpp"
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"

namespace guikit {

namespace {

Vec2 MeasureSwitchControl(
    const SwitchOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const SwitchMetrics& m = theme.GetSwitch(size);
    keel::f32 ctrlW = m.trackSize.x;
    keel::f32 ctrlH = m.trackSize.y;

    if (!opts.textLeft.empty() || !opts.textLeft.variants.empty()) {
        const keel::f32 leftFont = theme.ResolveFontSize(opts.textLeft, TextRole::LabelMedium, size);
        const keel::f32 leftW = TextUtil::MeasureWidth(opts.textLeft, leftFont, canvas);
        const keel::f32 leftH = canvas ? canvas->GetLineHeight(leftFont) : theme.GetLineHeight(leftFont);
        ctrlW += leftW + m.gap;
        ctrlH = keel::max(ctrlH, leftH);
    }

    if (!opts.textRight.empty() || !opts.textRight.variants.empty()) {
        const keel::f32 rightFont = theme.ResolveFontSize(opts.textRight, TextRole::LabelMedium, size);
        const keel::f32 rightW = TextUtil::MeasureWidth(opts.textRight, rightFont, canvas);
        const keel::f32 rightH = canvas ? canvas->GetLineHeight(rightFont) : theme.GetLineHeight(rightFont);
        ctrlW += m.gap + rightW;
        ctrlH = keel::max(ctrlH, rightH);
    }

    return {ctrlW, ctrlH};
}

ControlSpec EffectiveSwitchControl(UiSize size, const UiTheme& theme) noexcept {
    const SwitchMetrics& m = theme.GetSwitch(size);
    ControlSpec ctrl{};
    ctrl.width = AxisSize::Fit();
    ctrl.height = AxisSize::Fit();
    ctrl.defaultWidthMode = SizeMode::Fit;
    ctrl.defaultHeightMode = SizeMode::Fit;
    ctrl.defaultLayout = ElementLayout::Trailing;
    ctrl.labelGap = m.labelGap;
    ctrl.descGap = m.descGap;
    ctrl.inlineGap = m.gap;
    return ctrl;
}

} // namespace

Response detail::Switch::Run(
    Context& ctx,
    bool* value,
    bool checkedState,
    const SwitchOptions& opts,
    keel::SourceLocation loc
) {

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
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
    optsCopy.textLeft = ctx.AllocText(opts.textLeft);
    optsCopy.textRight = ctx.AllocText(opts.textRight);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);

    const ControlSpec ctrl = EffectiveSwitchControl(activeSize, ctx.GetTheme());
    Node* node = ctx.CreateNode(NodeType::Switch, id);
    node->text = !optsCopy.textRight.empty() ? optsCopy.textRight.text : optsCopy.textLeft.text;
    node->SetPayload(ctx.AllocPayload<SwitchOptions>(optsCopy));
    node->visualHash = detail::HashArgs(
        optsCopy.color,
        optsCopy.symmetric,
        currentChecked,
        optsCopy.textLeft,
        optsCopy.textRight
    );
    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* opts = n->As<SwitchOptions>();
        return detail::Switch::Measure(opts ? *opts : SwitchOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* opts = n->As<SwitchOptions>();
        detail::Switch::Render(
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
    node->focused = ctx.IsFocusVisible(id);

    Control::SetupNode(ctx, node, ctrl);

    return res;
}

Vec2 detail::Switch::Measure(
    const SwitchOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 /*availableWidth*/
) {
    const Vec2 ctrlDims = MeasureSwitchControl(opts, size, canvas, theme);
    const ControlSpec ctrl = EffectiveSwitchControl(size, theme);
    return Control::Measure(ctrlDims, ctrl, theme);
}

Vec2 detail::Switch::Measure(
    const SwitchOptions& opts,
    UiSize size,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme, availableWidth);
}

void detail::Switch::Render(
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
    const Rect ctrlRect = bounds;
    keel::f32 curX = ctrlRect.x + keel::max(0.0f, (ctrlRect.w - ctrlDims.x) * 0.5f);
    if (!opts.textLeft.empty() || !opts.textLeft.variants.empty()) {
        const keel::f32 leftFont = theme.ResolveFontSize(opts.textLeft, TextRole::LabelMedium, size);
        const keel::f32 leftW = TextUtil::MeasureWidth(opts.textLeft, leftFont, &canvas);
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

    const keel::f32 trackY = ctrlRect.y + (ctrlRect.h - m.trackSize.y) * 0.5f;
    const Rect trackRect(curX, trackY, m.trackSize.x, m.trackSize.y);
    curX += m.trackSize.x;

    if (!opts.textRight.empty() || !opts.textRight.variants.empty()) {
        curX += m.gap;
        const keel::f32 rightFont = theme.ResolveFontSize(opts.textRight, TextRole::LabelMedium, size);
        const keel::f32 rightW = TextUtil::MeasureWidth(opts.textRight, rightFont, &canvas);
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
    keel::f32 strokeWidth = 0.0f;
    Color knobFill;

    if (opts.symmetric) {
        trackFill = (hovered && !disabled) ? pal.controlTrackHover : pal.controlTrack;
        trackStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
        strokeWidth = 1.0f;
        knobFill = pal.controlKnobActive;
    } else if (checked) {
        const VariantColors& v = theme.GetVariant(UiVariant::Primary);
        if (!opts.color.HasAny()) {
            const bool isOutlineTrack = (v.bg.a == 0u);
            trackFill = isOutlineTrack
                ? ((hovered && !disabled) ? pal.accentHover : pal.accent)
                : ((hovered && !disabled) ? v.bgHover : v.bg);
            trackStroke = (v.borderWidth > 0.0f) ? v.border : Color::Transparent();
            strokeWidth = v.borderWidth;
            knobFill = isOutlineTrack ? pal.accentText : ((v.bg.a > 0u && theme.surfaceStyle == SurfaceStyle::Soft) ? v.text : pal.controlKnobActive);
        } else if (opts.color.token == StateColorToken::Ghost || (opts.color.normal.has_value() && opts.color.normal->a == 0)) {
            const StateColor resolved = theme.ResolveStateColor(opts.color);
            trackFill = (hovered && !disabled) ? resolved.hover.value_or(theme.GetVariant(UiVariant::Ghost).bgHover) : Color::Transparent();
            trackStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
            strokeWidth = 1.0f;
            knobFill = pal.controlKnobActive;
        } else {
            trackFill = theme.ResolveColor(opts.color, hovered && !disabled, pressed && !disabled, false, v.bg, v.bgHover, v.bgActive, v.bg);
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
        trackFill = trackFill.WithAlphaF(theme.disabledOpacity);
        trackStroke = trackStroke.WithAlphaF(theme.disabledOpacity);
        knobFill = knobFill.WithAlphaF(theme.disabledOpacity);
    } else if (focused) {
        trackStroke = pal.focusRing;
        strokeWidth = keel::max(strokeWidth, theme.focusRingWidth);
    }

    // Draw track & knob via active style painter
    theme.Painter().DrawSwitch(
        canvas,
        theme,
        trackRect,
        m.knobDiameter,
        m.knobPadding,
        trackFill,
        trackStroke,
        strokeWidth,
        knobFill,
        checked,
        focused,
        disabled,
        theme.GetRadius(size)
    );
}

} // namespace guikit
