#include "guikit/widgets/Checkbox.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

namespace {

ControlSpec EffectiveCheckboxControl(const CheckboxOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const CheckboxMetrics& m = theme.GetCheckbox(size);
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

Response Checkbox::Run(
    Context& ctx,
    bool* value,
    bool checkedState,
    const CheckboxOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
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
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);

    const ControlSpec ctrl = EffectiveCheckboxControl(optsCopy, activeSize, ctx.GetTheme());
    Node* node = ctx.CreateNode(NodeType::Checkbox, id);
    node->SetPayload(ctx.AllocPayload<CheckboxOptions>(optsCopy));
    node->visualHash = detail::HashArgs(
        optsCopy.color,
        static_cast<uint8_t>(optsCopy.align),
        optsCopy.indeterminate,
        optsCopy.radio,
        static_cast<uint8_t>(optsCopy.stretch),
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.background,
        optsCopy.border,
        currentChecked
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* opts = n->As<CheckboxOptions>();
        return Checkbox::Measure(opts ? *opts : CheckboxOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* opts = n->As<CheckboxOptions>();
        Checkbox::Render(
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
    node->focused = ctx.IsFocused(id);
    node->widthMode = (optsCopy.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);
    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Checkbox::Measure(
    const CheckboxOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    (void)canvas;
    const CheckboxMetrics& m = theme.GetCheckbox(size);
    const ControlSpec ctrl = EffectiveCheckboxControl(opts, size, theme);
    return Control::Measure({m.boxSize, m.boxSize}, ctrl, opts.stretch, size, theme);
}

Vec2 Checkbox::Measure(
    const CheckboxOptions& opts,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme, availableWidth);
}

void Checkbox::Render(
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
    const ControlSpec ctrl = EffectiveCheckboxControl(opts, size, theme);
    Control::RenderFrame(canvas, bounds, ctrl, size, disabled, theme, hovered, pressed || focused);
    const Rect ctrlRect = Control::ResolveInnerRect(bounds, ctrl, size, theme);
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
    float strokeWidth = 1.5f;
    Color markColor = pal.controlKnobActive;

    const bool isActive = checked || opts.indeterminate;

    if (isActive) {
        const VariantColors& v = theme.GetVariant(UiVariant::Primary);
        if (!opts.color.HasAny()) {
            boxFill = (hovered && !disabled) ? v.bgHover : v.bg;
            boxStroke = (v.borderWidth > 0.0f) ? v.border : boxFill;
            strokeWidth = (v.borderWidth > 0.0f) ? v.borderWidth : 1.5f;
            markColor = pal.controlKnobActive;
        } else if (opts.color.normal.has_value() && opts.color.normal->a == 0) {
            boxFill = (hovered && !disabled) ? opts.color.hover.value_or(theme.GetVariant(UiVariant::Ghost).bgHover) : Color::Transparent();
            boxStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
            strokeWidth = 1.5f;
            markColor = pal.textPrimary;
        } else {
            boxFill = opts.color.Resolve(hovered && !disabled, pressed && !disabled, false, v.bg, v.bgHover, v.bgActive, v.bg);
            boxStroke = boxFill;
            strokeWidth = 1.5f;
            markColor = pal.controlKnobActive;
        }
    } else {
        boxFill = (hovered && !disabled) ? pal.controlTrackHover : pal.controlTrack;
        boxStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
    }

    if (disabled) {
        boxFill = boxFill.WithAlphaF(0.40f);
        boxStroke = boxStroke.WithAlphaF(0.40f);
        markColor = markColor.WithAlphaF(0.40f);
    } else if (focused) {
        boxStroke = pal.focusRing;
        strokeWidth = std::max(strokeWidth, 2.0f);
    }

    // Draw box or radio circle
    const float outerRadius = opts.radio ? (boxRect.w * 0.5f) : m.cornerRadius;
    canvas.DrawRoundRect(boxRect, outerRadius, boxFill, boxStroke, strokeWidth);

    // Draw checkmark, radio inner dot, or indeterminate dash
    if (opts.radio) {
        if (checked) {
            const float dotDia = std::max(4.0f, boxRect.w * 0.44f);
            const Rect dotRect(
                boxRect.x + (boxRect.w - dotDia) * 0.5f,
                boxRect.y + (boxRect.h - dotDia) * 0.5f,
                dotDia,
                dotDia
            );
            canvas.DrawRoundRect(dotRect, dotDia * 0.5f, markColor);
        } else if (opts.indeterminate) {
            const Vec2 p1(boxRect.x + boxRect.w * 0.28f, boxRect.y + boxRect.h * 0.50f);
            const Vec2 p2(boxRect.x + boxRect.w * 0.72f, boxRect.y + boxRect.h * 0.50f);
            canvas.DrawLine(p1, p2, markColor, m.strokeWidth);
        }
    } else if (checked) {
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
