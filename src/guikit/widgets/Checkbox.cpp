#include "guikit/widgets/Checkbox.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/ElementComposer.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

namespace {

ElementSpec MakeCheckboxElementSpec(
    const CheckboxOptions& opts,
    UiSize size,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const CheckboxMetrics& m = theme.GetCheckbox(size);
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .descriptionVariant = opts.descriptionVariant,
        .labelFontSize = m.fontSize,
        .descFontSize = m.descFontSize,
        .labelGap = m.descGap,
        .descGap = m.descGap,
        .inlineGap = m.gap,
        .stretchableControl = false,
        .stretchHeightWhenStandalone = false,
        .disabled = disabled
    };
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

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.rightClicked = false;
        res.doubleClicked = false;
    }

    bool currentChecked = (value != nullptr) ? *value : checkedState;

    if (res.clicked && !effectiveDisabled) {
        currentChecked = !currentChecked;
        if (value != nullptr) {
            *value = currentChecked;
        }
        res.changed = true;
    }

    Node* node = ctx.CreateNode(NodeType::Checkbox, id);
    node->text = ctx.AllocString(opts.label);
    CheckboxOptions optsCopy = opts;
    optsCopy.label = node->text;
    if (!opts.description.empty()) {
        optsCopy.description = ctx.AllocString(opts.description);
    }
    node->payload = ctx.AllocPayload<CheckboxOptions>(optsCopy);
    node->visualHash = detail::HashArgs(
        opts.variant,
        opts.descriptionVariant,
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        opts.indeterminate,
        static_cast<uint8_t>(opts.stretch),
        currentChecked,
        node->text,
        optsCopy.description
    );
    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* opts = n->As<CheckboxOptions>();
        return Checkbox::Measure(opts ? *opts : CheckboxOptions{}, n->uiSize, canvas, theme);
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
            theme
        );
    };
    node->checked = currentChecked;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Checkbox::Measure(
    const CheckboxOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const CheckboxMetrics& m = theme.GetCheckbox(size);
    const ElementSpec spec = MakeCheckboxElementSpec(opts, size, opts.disabled, theme);
    return ElementComposer::Measure({m.boxSize, m.boxSize}, spec, canvas, theme);
}

Vec2 Checkbox::Measure(
    const CheckboxOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme);
}

void Checkbox::Render(
    ICanvas& canvas,
    Rect bounds,
    bool checked,
    const CheckboxOptions& opts,
    UiSize size,
    bool hovered,
    bool /*pressed*/,
    bool disabled,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const CheckboxMetrics& m = theme.GetCheckbox(size);
    const ElementSpec spec = MakeCheckboxElementSpec(opts, size, disabled, theme);
    const ElementGeometry geom = ElementComposer::Resolve(bounds, {m.boxSize, m.boxSize}, spec, &canvas, theme);
    ElementComposer::RenderText(canvas, geom, spec, theme);
    const Rect boxRect = geom.controlRect;

    // Determine colors
    const auto& pal = theme.palette;
    Color boxFill;
    Color boxStroke;
    float strokeWidth = 1.5f;
    Color markColor = pal.controlKnobActive;

    const bool isActive = checked || opts.indeterminate;

    if (isActive) {
        const VariantColors& v = theme.GetVariant(opts.variant);
        if (opts.variant == UiVariant::Ghost) {
            boxFill = (hovered && !disabled) ? v.bgHover : Color::Transparent();
            boxStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
            strokeWidth = 1.5f;
            markColor = pal.textPrimary;
        } else {
            boxFill = (hovered && !disabled) ? v.bgHover : v.bg;
            boxStroke = (v.borderWidth > 0.0f) ? v.border : boxFill;
            strokeWidth = (v.borderWidth > 0.0f) ? v.borderWidth : 1.5f;
            markColor = (opts.variant == UiVariant::Default) ? pal.textPrimary : pal.controlKnobActive;
        }
    } else {
        boxFill = (hovered && !disabled) ? pal.controlTrackHover : pal.controlTrack;
        boxStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
    }

    if (disabled) {
        boxFill = boxFill.WithAlphaF(0.40f);
        boxStroke = boxStroke.WithAlphaF(0.40f);
        markColor = markColor.WithAlphaF(0.40f);
    }

    // Draw box
    canvas.DrawRoundRect(boxRect, m.cornerRadius, boxFill, boxStroke, strokeWidth);

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
