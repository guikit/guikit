#include "guikit/controls/checkbox.hpp"
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/core/node.hpp"

namespace guikit {

namespace {

ControlSpec EffectiveCheckboxControl(UiSize size, const UiTheme& theme) noexcept {
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

Response detail::Checkbox::Run(
    Context& ctx,
    bool* value,
    bool checkedState,
    const CheckboxOptions& opts,
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

    CheckboxOptions optsCopy = opts;
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);

    const ControlSpec ctrl = EffectiveCheckboxControl(activeSize, ctx.GetTheme());
    Node* node = ctx.CreateNode(NodeType::Checkbox, id);
    node->SetPayload(ctx.AllocPayload<CheckboxOptions>(optsCopy));
    node->visualHash = detail::HashArgs(
        optsCopy.color,
        optsCopy.indeterminate,
        currentChecked
    );
    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* opts = n->As<CheckboxOptions>();
        return detail::Checkbox::Measure(opts ? *opts : CheckboxOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* opts = n->As<CheckboxOptions>();
        detail::Checkbox::Render(
            canvas,
            n->computedRect,
            n->checked,
            opts ? *opts : CheckboxOptions{},
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

Vec2 detail::Checkbox::Measure(
    const CheckboxOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 /*availableWidth*/
) {
    (void)opts;
    (void)canvas;
    const CheckboxMetrics& m = theme.GetCheckbox(size);
    const ControlSpec ctrl = EffectiveCheckboxControl(size, theme);
    return Control::Measure({m.boxSize, m.boxSize}, ctrl, theme);
}

Vec2 detail::Checkbox::Measure(
    const CheckboxOptions& opts,
    UiSize size,
    ICanvas* canvas,
    keel::f32 availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme, availableWidth);
}

void detail::Checkbox::Render(
    ICanvas& canvas,
    Rect bounds,
    bool checked,
    const CheckboxOptions& opts,
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

    // Determine colors
    const auto& pal = theme.palette;
    Color boxFill;
    Color boxStroke;
    keel::f32 strokeWidth = 1.5f;
    Color markColor = pal.controlKnobActive;

    const bool isActive = checked || opts.indeterminate;

    if (isActive) {
        const VariantColors& v = theme.GetVariant(UiVariant::Primary);
        if (!opts.color.HasAny()) {
            boxFill = (hovered && !disabled) ? v.bgHover : v.bg;
            boxStroke = (v.borderWidth > 0.0f) ? v.border : boxFill;
            strokeWidth = (v.borderWidth > 0.0f) ? v.borderWidth : 1.5f;
            markColor = (v.bg.a == 0u || theme.fillStyle == FillStyle::Soft) ? v.text : pal.controlKnobActive;
        } else if (opts.color.token == StateColorToken::Ghost || (opts.color.normal.has_value() && opts.color.normal->a == 0)) {
            const StateColor resolved = theme.ResolveStateColor(opts.color);
            boxFill = (hovered && !disabled) ? resolved.hover.value_or(theme.GetVariant(UiVariant::Ghost).bgHover) : Color::Transparent();
            boxStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
            strokeWidth = 1.5f;
            markColor = pal.textPrimary;
        } else {
            boxFill = theme.ResolveColor(opts.color, hovered && !disabled, pressed && !disabled, false, v.bg, v.bgHover, v.bgActive, v.bg);
            const Color solidAccent = theme.ResolveSemanticAccent(opts.color, UiVariant::Primary, hovered && !disabled, pressed && !disabled, false);
            boxStroke = (boxFill.a == 0u || theme.fillStyle == FillStyle::Soft) ? solidAccent : boxFill;
            strokeWidth = 1.5f;
            markColor = (boxFill.a == 0u || theme.fillStyle == FillStyle::Soft) ? solidAccent : pal.controlKnobActive;
        }
    } else {
        boxFill = (hovered && !disabled) ? pal.controlTrackHover : pal.controlTrack;
        boxStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
    }

    if (disabled) {
        boxFill = boxFill.WithAlphaF(theme.disabledOpacity);
        boxStroke = boxStroke.WithAlphaF(theme.disabledOpacity);
        markColor = markColor.WithAlphaF(theme.disabledOpacity);
    } else if (focused) {
        boxStroke = pal.focusRing;
        strokeWidth = keel::max(strokeWidth, theme.focusRingWidth);
    }

    // Draw box via active style painter
    markColor = theme.Painter().DrawCheckboxBox(
        canvas,
        theme,
        boxRect,
        m.cornerRadius,
        boxFill,
        boxStroke,
        strokeWidth,
        markColor,
        pressed,
        focused,
        disabled
    );

    // Draw checkmark or indeterminate dash
    if (checked) {
        const Vec2 p1(boxRect.x + boxRect.w * 0.22f, boxRect.y + boxRect.h * 0.52f);
        const Vec2 p2(boxRect.x + boxRect.w * 0.42f, boxRect.y + boxRect.h * 0.74f);
        const Vec2 p3(boxRect.x + boxRect.w * 0.78f, boxRect.y + boxRect.h * 0.26f);

        canvas.DrawLine(p1, p2, markColor, m.strokeWidth);
        canvas.DrawLine(p2, p3, markColor, m.strokeWidth);
    } else if (opts.indeterminate) {
        const Vec2 p1(boxRect.x + boxRect.w * 0.25f, boxRect.y + boxRect.h * 0.50f);
        const Vec2 p2(boxRect.x + boxRect.w * 0.75f, boxRect.y + boxRect.h * 0.50f);

        canvas.DrawLine(p1, p2, markColor, m.strokeWidth);
    }
}

} // namespace guikit
