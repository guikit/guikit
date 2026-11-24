#include "guikit/controls/radio.hpp"
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/style/style_painter.hpp"

namespace guikit {

namespace {

ControlSpec EffectiveRadioControl(UiSize size, const UiTheme& theme) noexcept {
    const CheckboxMetrics& m = theme.GetCheckbox(size);
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

Response detail::Radio::Run(
    Context& ctx,
    bool* value,
    bool selectedState,
    const RadioOptions& opts,
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

    bool currentSelected = (value != nullptr) ? *value : selectedState;

    if (res.clicked && !effectiveDisabled) {
        currentSelected = !currentSelected;
        if (value != nullptr) {
            *value = currentSelected;
        }
        res.changed = true;
    }

    RadioOptions optsCopy = opts;
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);

    const ControlSpec ctrl = EffectiveRadioControl(activeSize, ctx.GetTheme());
    Node* node = ctx.CreateNode(NodeType::Radio, id);
    node->SetPayload(ctx.AllocPayload<RadioOptions>(optsCopy));
    node->visualHash = detail::HashArgs(
        optsCopy.color,
        currentSelected
    );
    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* opts = n->As<RadioOptions>();
        return detail::Radio::Measure(opts ? *opts : RadioOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* opts = n->As<RadioOptions>();
        detail::Radio::Render(
            canvas,
            n->computedRect,
            n->checked,
            opts ? *opts : RadioOptions{},
            n->uiSize,
            n->hovered,
            n->pressed,
            n->disabled,
            theme,
            n->focused
        );
    };
    node->checked = currentSelected;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->focused = ctx.IsFocusVisible(id);

    Control::SetupNode(ctx, node, ctrl);

    return res;
}

Vec2 detail::Radio::Measure(
    const RadioOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 /*availableWidth*/
) {
    (void)opts;
    (void)canvas;
    const CheckboxMetrics& m = theme.GetCheckbox(size);
    const ControlSpec ctrl = EffectiveRadioControl(size, theme);
    return Control::Measure({m.boxSize, m.boxSize}, ctrl, theme);
}

Vec2 detail::Radio::Measure(
    const RadioOptions& opts,
    UiSize size,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme, availableWidth);
}

void detail::Radio::Render(
    ICanvas& canvas,
    Rect bounds,
    bool selected,
    const RadioOptions& opts,
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

    const CheckboxMetrics& m = theme.GetCheckbox(size);
    const Rect ctrlRect = bounds;
    const Rect boxRect(
        ctrlRect.x + (ctrlRect.w - m.boxSize) * 0.5f,
        ctrlRect.y + (ctrlRect.h - m.boxSize) * 0.5f,
        m.boxSize,
        m.boxSize
    );

    const auto& pal = theme.palette;
    Color ringFill;
    Color ringStroke;
    keel::f32 strokeWidth = 1.5f;
    Color dotColor = pal.controlKnobActive;

    if (selected) {
        const VariantColors& v = theme.GetVariant(UiVariant::Primary);
        if (!opts.color.HasAny()) {
            ringFill = (hovered && !disabled) ? v.bgHover : v.bg;
            ringStroke = (v.borderWidth > 0.0f) ? v.border : ringFill;
            strokeWidth = (v.borderWidth > 0.0f) ? v.borderWidth : 1.5f;
            dotColor = pal.controlKnobActive;
        } else if (opts.color.token == StateColorToken::Ghost || (opts.color.normal.has_value() && opts.color.normal->a == 0)) {
            const StateColor resolved = theme.ResolveStateColor(opts.color);
            ringFill = (hovered && !disabled) ? resolved.hover.value_or(theme.GetVariant(UiVariant::Ghost).bgHover) : Color::Transparent();
            ringStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
            strokeWidth = 1.5f;
            dotColor = pal.textPrimary;
        } else {
            ringFill = theme.ResolveColor(opts.color, hovered && !disabled, pressed && !disabled, false, v.bg, v.bgHover, v.bgActive, v.bg);
            ringStroke = ringFill;
            strokeWidth = 1.5f;
            dotColor = pal.controlKnobActive;
        }
    } else {
        ringFill = (hovered && !disabled) ? pal.controlTrackHover : pal.controlTrack;
        ringStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
    }

    if (disabled) {
        ringFill = ringFill.WithAlphaF(theme.disabledOpacity);
        ringStroke = ringStroke.WithAlphaF(theme.disabledOpacity);
        dotColor = dotColor.WithAlphaF(theme.disabledOpacity);
    } else if (focused) {
        ringStroke = pal.focusRing;
        strokeWidth = keel::max(strokeWidth, theme.focusRingWidth);
    }

    const keel::f32 outerRadius = boxRect.w * 0.5f;
    const Color resolvedDotColor = theme.Painter().DrawRadioCircle(
        canvas, theme, boxRect.Center(), outerRadius,
        ringFill, ringStroke, strokeWidth, dotColor,
        selected, pressed, focused, disabled
    );

    if (selected) {
        // Choose dot diameter so (boxRect.w - dotDia) * 0.5 lands on a clean 0.25-dp grid (eliminating relative subpixel drift)
        const keel::f32 rawGap = boxRect.w * 0.56f;
        const keel::f32 snappedGap = keel::round(rawGap * 2.0f) * 0.5f;
        const keel::f32 dotDia = keel::max(4.0f, boxRect.w - snappedGap);
        const Rect dotRect(
            boxRect.x + (boxRect.w - dotDia) * 0.5f,
            boxRect.y + (boxRect.h - dotDia) * 0.5f,
            dotDia,
            dotDia
        );
        canvas.DrawRoundRect(dotRect, dotDia * 0.5f, resolvedDotColor);
    }
}

} // namespace guikit
