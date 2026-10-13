#include "guikit/widgets/Switch.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/ElementComposer.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

namespace {

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
    node->text = ctx.AllocString(opts.label);
    SwitchOptions optsCopy = opts;
    optsCopy.label = node->text;
    if (!opts.trailingLabel.empty()) {
        optsCopy.trailingLabel = ctx.AllocString(opts.trailingLabel);
    }
    if (!opts.description.empty()) {
        optsCopy.description = ctx.AllocString(opts.description);
    }
    node->payload = ctx.AllocPayload<SwitchOptions>(optsCopy);
    node->visualHash = detail::HashArgs(
        opts.variant,
        opts.descriptionVariant,
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        opts.symmetric,
        static_cast<uint8_t>(opts.stretch),
        currentChecked,
        node->text,
        optsCopy.trailingLabel,
        optsCopy.description
    );
    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* opts = n->As<SwitchOptions>();
        return Switch::Measure(opts ? *opts : SwitchOptions{}, n->uiSize, canvas, theme);
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
    const UiTheme& theme
) {
    const SwitchMetrics& m = theme.GetSwitch(size);
    const bool isBoth = (opts.layout == ElementLayout::Inline && !opts.trailingLabel.empty());

    if (!isBoth) {
        const ElementSpec spec = MakeSwitchElementSpec(opts, size, opts.disabled, theme);
        return ElementComposer::Measure(m.trackSize, spec, canvas, theme);
    }

    const bool hasLabel = !opts.label.empty();
    const bool hasTrailingLabel = !opts.trailingLabel.empty();
    const bool hasDesc = !opts.description.empty();

    Vec2 labelDims = {0.0f, 0.0f};
    if (hasLabel) {
        if (canvas) {
            labelDims = canvas->MeasureText(opts.label, m.fontSize);
        } else {
            const float estW = static_cast<float>(opts.label.length()) * (m.fontSize * kDefaultCharAdvanceRatio);
            labelDims = {estW, theme.GetLineHeight(m.fontSize)};
        }
    }

    Vec2 descDims = {0.0f, 0.0f};
    if (hasDesc) {
        if (canvas) {
            descDims = canvas->MeasureText(opts.description, m.descFontSize);
        } else {
            const float estW = static_cast<float>(opts.description.length()) * (m.descFontSize * kDefaultCharAdvanceRatio);
            descDims = {estW, theme.GetLineHeight(m.descFontSize)};
        }
    }

    Vec2 trailingDims = {0.0f, 0.0f};
    if (hasTrailingLabel) {
        if (canvas) {
            trailingDims = canvas->MeasureText(opts.trailingLabel, m.fontSize);
        } else {
            const float estW = static_cast<float>(opts.trailingLabel.length()) * (m.fontSize * kDefaultCharAdvanceRatio);
            trailingDims = {estW, theme.GetLineHeight(m.fontSize)};
        }
    }

    float topRowW = m.trackSize.x;
    if (hasLabel) {
        topRowW += labelDims.x + m.gap;
    }
    if (hasTrailingLabel) {
        topRowW += m.gap + trailingDims.x;
    }
    const float topRowH = std::max({m.trackSize.y, labelDims.y, trailingDims.y});
    const float totalW = std::max(topRowW, descDims.x);
    const float totalH = topRowH + (hasDesc ? (m.descGap + descDims.y) : 0.0f);
    return {totalW, totalH};
}

Vec2 Switch::Measure(
    const SwitchOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme);
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
    const bool isBoth = (opts.layout == ElementLayout::Inline && !opts.trailingLabel.empty());
    const bool isSymmetric = opts.symmetric || isBoth;

    Rect trackRect;
    Rect labelRect;
    Rect trailingLabelRect;
    Rect descRect;

    if (isBoth) {
        const bool hasLabel = !opts.label.empty();
        const bool hasTrailingLabel = !opts.trailingLabel.empty();
        const bool hasDesc = !opts.description.empty();

        const float labelH = hasLabel ? canvas.GetLineHeight(m.fontSize) : 0.0f;
        const float trailingLabelH = hasTrailingLabel ? canvas.GetLineHeight(m.fontSize) : 0.0f;
        const float descH = hasDesc ? canvas.GetLineHeight(m.descFontSize) : 0.0f;

        const Vec2 leftDims = hasLabel ? canvas.MeasureText(opts.label, m.fontSize) : Vec2{0.0f, 0.0f};
        const Vec2 rightDims = hasTrailingLabel ? canvas.MeasureText(opts.trailingLabel, m.fontSize) : Vec2{0.0f, 0.0f};

        const float topRowH = std::max({m.trackSize.y, labelH, trailingLabelH});
        const float totalContentH = topRowH + (hasDesc ? (m.descGap + descH) : 0.0f);
        const float startY = bounds.y + std::max(0.0f, (bounds.h - totalContentH) * 0.5f);
        const float trackY = startY + (topRowH - m.trackSize.y) * 0.5f;
        const float textY = startY + (topRowH - std::max(labelH, trailingLabelH)) * 0.5f;

        if (opts.stretch != StretchMode::Compact && bounds.w > (leftDims.x + m.trackSize.x + rightDims.x + m.gap * 2.0f)) {
            if (hasLabel) {
                labelRect = Rect(bounds.x, textY, leftDims.x, labelH);
            }
            const float trackX = bounds.x + (bounds.w - m.trackSize.x) * 0.5f;
            trackRect = Rect(trackX, trackY, m.trackSize.x, m.trackSize.y);
            if (hasTrailingLabel) {
                trailingLabelRect = Rect(bounds.x + bounds.w - rightDims.x, textY, rightDims.x, trailingLabelH);
            }
        } else {
            float curX = bounds.x;
            if (hasLabel) {
                labelRect = Rect(curX, textY, leftDims.x, labelH);
                curX += leftDims.x + m.gap;
            }
            trackRect = Rect(curX, trackY, m.trackSize.x, m.trackSize.y);
            curX += m.trackSize.x;
            if (hasTrailingLabel) {
                curX += m.gap;
                const float remW = std::max(rightDims.x, bounds.w - (curX - bounds.x));
                trailingLabelRect = Rect(curX, textY, remW, trailingLabelH);
            }
        }
        if (hasDesc) {
            descRect = Rect(bounds.x, startY + topRowH + m.descGap, bounds.w, descH);
        }

        if (hasLabel && labelRect.w > 0.0f && labelRect.h > 0.0f) {
            const Color leftCol = disabled ? pal.textDisabled : (checked ? pal.textMuted : pal.textPrimary);
            canvas.DrawTextBox(opts.label, labelRect, m.fontSize, leftCol, 1.0f, 0.5f);
        }
        if (hasTrailingLabel && trailingLabelRect.w > 0.0f && trailingLabelRect.h > 0.0f) {
            const Color rightCol = disabled ? pal.textDisabled : (checked ? pal.textPrimary : pal.textMuted);
            canvas.DrawTextBox(opts.trailingLabel, trailingLabelRect, m.fontSize, rightCol, 0.0f, 0.5f);
        }
        if (hasDesc && descRect.w > 0.0f && descRect.h > 0.0f) {
            Color descCol = theme.GetTextColor(opts.descriptionVariant);
            if (disabled) descCol = descCol.WithAlphaF(0.40f);
            const float alignFactor = (opts.elementAlign == Align::End) ? 1.0f : ((opts.elementAlign == Align::Center) ? 0.5f : 0.0f);
            canvas.DrawTextBox(opts.description, descRect, m.descFontSize, descCol, alignFactor, 0.5f);
        }
    } else {
        const ElementSpec spec = MakeSwitchElementSpec(opts, size, disabled, theme);
        const ElementGeometry geom = ElementComposer::Resolve(bounds, m.trackSize, spec, &canvas, theme);
        ElementComposer::RenderText(canvas, geom, spec, theme);
        trackRect = geom.controlRect;
    }

    // Determine colors
    Color trackFill;
    Color trackStroke{0, 0, 0, 0};
    float strokeWidth = 0.0f;
    Color knobFill;

    if (isSymmetric) {
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
