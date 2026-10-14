#include "guikit/widgets/Switch.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/ElementComposer.hpp"
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

ElementSpec MakeSwitchElementSpec(
    const SwitchOptions& opts,
    UiSize size,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const SwitchMetrics& m = theme.GetSwitch(size);
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .widgetSize = size,
        .labelGap = m.descGap,
        .descGap = m.descGap,
        .inlineGap = m.gap,
        .stretchableControl = false,
        .stretchHeightWhenStandalone = false,
        .disabled = disabled
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

    Node* node = ctx.CreateNode(NodeType::Switch, id);
    SwitchOptions optsCopy = opts;
    optsCopy.label = ctx.AllocText(opts.label);
    optsCopy.textLeft = ctx.AllocText(opts.textLeft);
    optsCopy.textRight = ctx.AllocText(opts.textRight);
    optsCopy.description = ctx.AllocText(opts.description);
    node->text = optsCopy.label.text;
    node->payload = ctx.AllocPayload<SwitchOptions>(optsCopy);
    node->visualHash = detail::HashArgs(
        opts.variant,
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        opts.symmetric,
        static_cast<uint8_t>(opts.stretch),
        currentChecked,
        optsCopy.label,
        optsCopy.textLeft,
        optsCopy.textRight,
        optsCopy.description
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

Vec2 Switch::Measure(
    const SwitchOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) {
    const Vec2 ctrlDims = MeasureSwitchControl(opts, size, canvas, theme);
    const ElementSpec spec = MakeSwitchElementSpec(opts, size, opts.disabled, theme);
    return ElementComposer::Measure(ctrlDims, spec, canvas, theme, availableWidth);
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
    bool /*pressed*/,
    bool disabled,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const SwitchMetrics& m = theme.GetSwitch(size);
    const auto& pal = theme.palette;

    const Vec2 ctrlDims = MeasureSwitchControl(opts, size, &canvas, theme);
    const ElementSpec spec = MakeSwitchElementSpec(opts, size, disabled, theme);
    const ElementGeometry geom = ElementComposer::Resolve(bounds, ctrlDims, spec, &canvas, theme);
    ElementComposer::RenderText(canvas, geom, spec, theme);

    float curX = geom.controlRect.x;
    if (!opts.textLeft.empty() || !opts.textLeft.variants.empty()) {
        const float leftFont = theme.ResolveFontSize(opts.textLeft, TextRole::LabelMedium, size);
        const float leftW = TextUtil::MeasureWidth(opts.textLeft, leftFont, &canvas);
        if (!opts.textLeft.empty()) {
            const UiVariant defaultLeftVar = checked ? UiVariant::Secondary : UiVariant::Default;
            const Color leftCol = theme.ResolveTextColor(opts.textLeft, defaultLeftVar, disabled);
            canvas.DrawTextBox(
                opts.textLeft.text,
                Rect(curX, geom.controlRect.y, leftW, geom.controlRect.h),
                leftFont,
                leftCol,
                1.0f,
                0.5f
            );
        }
        curX += leftW + m.gap;
    }

    const float trackY = geom.controlRect.y + (geom.controlRect.h - m.trackSize.y) * 0.5f;
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
                Rect(curX, geom.controlRect.y, rightW, geom.controlRect.h),
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
        const VariantColors& v = theme.GetVariant(opts.variant);
        if (opts.variant == UiVariant::Ghost) {
            trackFill = (hovered && !disabled) ? v.bgHover : Color::Transparent();
            trackStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
            strokeWidth = 1.0f;
            knobFill = pal.controlKnobActive;
        } else {
            trackFill = (hovered && !disabled) ? v.bgHover : v.bg;
            trackStroke = (v.borderWidth > 0.0f) ? v.border : Color::Transparent();
            strokeWidth = v.borderWidth;
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
