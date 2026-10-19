#include "guikit/widgets/Radio.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

namespace {

ControlSpec EffectiveRadioControl(const RadioOptions& opts, UiSize size, const UiTheme& theme) noexcept {
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

Response Radio::Run(
    Context& ctx,
    bool* value,
    bool selectedState,
    const RadioOptions& opts,
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

    bool currentSelected = (value != nullptr) ? *value : selectedState;

    if (res.clicked && !effectiveDisabled) {
        currentSelected = !currentSelected;
        if (value != nullptr) {
            *value = currentSelected;
        }
        res.changed = true;
    }

    RadioOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);

    const ControlSpec ctrl = EffectiveRadioControl(optsCopy, activeSize, ctx.GetTheme());
    Node* node = ctx.CreateNode(NodeType::Radio, id);
    node->SetPayload(ctx.AllocPayload<RadioOptions>(optsCopy));
    node->visualHash = detail::HashArgs(
        optsCopy.color,
        static_cast<uint8_t>(optsCopy.align),
        static_cast<uint8_t>(optsCopy.stretch),
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.background,
        optsCopy.border,
        currentSelected
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* opts = n->As<RadioOptions>();
        return Radio::Measure(opts ? *opts : RadioOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* opts = n->As<RadioOptions>();
        Radio::Render(
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
    node->focused = ctx.IsFocused(id);
    node->widthMode = (optsCopy.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);
    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Radio::Measure(
    const RadioOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    (void)canvas;
    const CheckboxMetrics& m = theme.GetCheckbox(size);
    const ControlSpec ctrl = EffectiveRadioControl(opts, size, theme);
    return Control::Measure({m.boxSize, m.boxSize}, ctrl, opts.stretch, size, theme);
}

Vec2 Radio::Measure(
    const RadioOptions& opts,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme, availableWidth);
}

void Radio::Render(
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
    const ControlSpec ctrl = EffectiveRadioControl(opts, size, theme);
    Control::RenderFrame(canvas, bounds, ctrl, size, disabled, theme, hovered, pressed || focused);
    const Rect ctrlRect = Control::ResolveInnerRect(bounds, ctrl, size, theme);
    const Rect boxRect(
        ctrlRect.x + (ctrlRect.w - m.boxSize) * 0.5f,
        ctrlRect.y + (ctrlRect.h - m.boxSize) * 0.5f,
        m.boxSize,
        m.boxSize
    );

    const auto& pal = theme.palette;
    Color ringFill;
    Color ringStroke;
    float strokeWidth = 1.5f;
    Color dotColor = pal.controlKnobActive;

    if (selected) {
        const VariantColors& v = theme.GetVariant(UiVariant::Primary);
        if (!opts.color.HasAny()) {
            ringFill = (hovered && !disabled) ? v.bgHover : v.bg;
            ringStroke = (v.borderWidth > 0.0f) ? v.border : ringFill;
            strokeWidth = (v.borderWidth > 0.0f) ? v.borderWidth : 1.5f;
            dotColor = pal.controlKnobActive;
        } else if (opts.color.normal.has_value() && opts.color.normal->a == 0) {
            ringFill = (hovered && !disabled) ? opts.color.hover.value_or(theme.GetVariant(UiVariant::Ghost).bgHover) : Color::Transparent();
            ringStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
            strokeWidth = 1.5f;
            dotColor = pal.textPrimary;
        } else {
            ringFill = opts.color.Resolve(hovered && !disabled, pressed && !disabled, false, v.bg, v.bgHover, v.bgActive, v.bg);
            ringStroke = ringFill;
            strokeWidth = 1.5f;
            dotColor = pal.controlKnobActive;
        }
    } else {
        ringFill = (hovered && !disabled) ? pal.controlTrackHover : pal.controlTrack;
        ringStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
    }

    if (disabled) {
        ringFill = ringFill.WithAlphaF(0.40f);
        ringStroke = ringStroke.WithAlphaF(0.40f);
        dotColor = dotColor.WithAlphaF(0.40f);
    } else if (focused) {
        ringStroke = pal.focusRing;
        strokeWidth = std::max(strokeWidth, 2.0f);
    }

    const float outerRadius = boxRect.w * 0.5f;
    canvas.DrawRoundRect(boxRect, outerRadius, ringFill, ringStroke, strokeWidth);

    if (selected) {
        const float dotDia = std::max(4.0f, boxRect.w * 0.44f);
        const Rect dotRect(
            boxRect.x + (boxRect.w - dotDia) * 0.5f,
            boxRect.y + (boxRect.h - dotDia) * 0.5f,
            dotDia,
            dotDia
        );
        canvas.DrawRoundRect(dotRect, dotDia * 0.5f, dotColor);
    }
}

} // namespace guikit
