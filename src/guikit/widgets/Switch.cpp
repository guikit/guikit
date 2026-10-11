#include "guikit/widgets/Switch.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/layout/Node.hpp"

namespace guikit {

Response Switch::Run(
    Context& ctx,
    std::string_view label,
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
    const std::string_view effectiveLabel = !label.empty() ? label : opts.label;
    node->text = ctx.InternString(effectiveLabel);
    SwitchOptions optsCopy = opts;
    optsCopy.label = node->text;
    if (!opts.description.empty()) {
        optsCopy.description = ctx.InternString(opts.description);
    }
    node->payload = ctx.AllocPayload<SwitchOptions>(optsCopy);
    node->measureFn = [](const Node* n, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* opts = n->As<SwitchOptions>();
        return Switch::Measure(n->text, opts ? *opts : SwitchOptions{}, n->uiSize, canvas, theme);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* opts = n->As<SwitchOptions>();
        Switch::Render(
            canvas,
            n->computedRect,
            n->text,
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
    node->widthMode = SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Switch::Measure(
    std::string_view label,
    const SwitchOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const SwitchMetrics& m = theme.GetSwitch(size);
    const std::string_view effectiveLabel = !label.empty() ? label : opts.label;
    const bool hasLabel = !effectiveLabel.empty();
    const bool hasDesc = !opts.description.empty();

    if (!hasLabel && !hasDesc) {
        return m.trackSize;
    }

    Vec2 labelDims = {0.0f, 0.0f};
    if (hasLabel) {
        if (canvas) {
            labelDims = canvas->MeasureText(effectiveLabel, m.fontSize);
        } else {
            const float estW = static_cast<float>(effectiveLabel.length()) * (m.fontSize * 0.58f);
            labelDims = {estW, m.fontSize * 1.35f};
        }
    }

    Vec2 descDims = {0.0f, 0.0f};
    if (hasDesc) {
        if (canvas) {
            descDims = canvas->MeasureText(opts.description, m.descFontSize);
        } else {
            const float estW = static_cast<float>(opts.description.length()) * (m.descFontSize * 0.58f);
            descDims = {estW, m.descFontSize * 1.35f};
        }
    }

    if (opts.labelPlacement == Placement::Stacked) {
        // Label on top, track in middle, description on bottom
        const float maxW = std::max({m.trackSize.x, labelDims.x, descDims.x});
        float totalH = m.trackSize.y;
        if (hasLabel) {
            totalH += labelDims.y + m.descGap;
        }
        if (hasDesc) {
            totalH += m.descGap + descDims.y;
        }
        return {maxW, totalH};
    }

    if (opts.labelPlacement == Placement::Top) {
        // Both label and description above track
        const float maxW = std::max({m.trackSize.x, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasLabel && hasDesc) {
            textH = labelDims.y + m.descGap + descDims.y;
        } else if (hasLabel) {
            textH = labelDims.y;
        } else if (hasDesc) {
            textH = descDims.y;
        }
        return {maxW, textH + m.descGap + m.trackSize.y};
    }

    if (opts.labelPlacement == Placement::Bottom) {
        // Track above, both label and description below track
        const float maxW = std::max({m.trackSize.x, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasLabel && hasDesc) {
            textH = labelDims.y + m.descGap + descDims.y;
        } else if (hasLabel) {
            textH = labelDims.y;
        } else if (hasDesc) {
            textH = descDims.y;
        }
        return {maxW, m.trackSize.y + m.descGap + textH};
    }

    // Horizontal: Leading or Trailing
    const float textW = std::max(labelDims.x, descDims.x);
    float textH = 0.0f;
    if (hasLabel && hasDesc) {
        textH = labelDims.y + m.descGap + descDims.y;
    } else if (hasLabel) {
        textH = labelDims.y;
    } else if (hasDesc) {
        textH = descDims.y;
    }

    const float totalW = m.trackSize.x + m.gap + textW;
    const float totalH = std::max(m.trackSize.y, textH);
    return {totalW, totalH};
}

Vec2 Switch::Measure(
    std::string_view label,
    const SwitchOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(label, opts, size, canvas, defaultTheme);
}

void Switch::Render(
    ICanvas& canvas,
    Rect bounds,
    std::string_view label,
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
    const std::string_view effectiveLabel = !label.empty() ? label : opts.label;
    const bool hasLabel = !effectiveLabel.empty();
    const bool hasDesc = !opts.description.empty();
    const bool hasText = hasLabel || hasDesc;

    Rect trackRect;
    Rect labelRect;
    Rect descRect;
    float alignX = 0.0f;

    const float labelH = hasLabel ? std::max(m.fontSize * 1.35f, canvas.MeasureText(effectiveLabel, m.fontSize).y) : 0.0f;
    const float descH = hasDesc ? std::max(m.descFontSize * 1.35f, canvas.MeasureText(opts.description, m.descFontSize).y) : 0.0f;

    if (!hasText) {
        const float trackY = bounds.y + (bounds.h - m.trackSize.y) * 0.5f;
        trackRect = Rect(bounds.x, trackY, m.trackSize.x, m.trackSize.y);
    } else if (opts.labelPlacement == Placement::Stacked) {
        // Label on top, track in middle, description on bottom
        float curY = bounds.y;
        if (hasLabel) {
            labelRect = Rect(bounds.x, curY, bounds.w, labelH);
            curY += labelH + m.descGap;
        }
        trackRect = Rect(bounds.x, curY, m.trackSize.x, m.trackSize.y);
        curY += m.trackSize.y + m.descGap;
        if (hasDesc) {
            descRect = Rect(bounds.x, curY, bounds.w, descH);
        }
        alignX = 0.0f;
    } else if (opts.labelPlacement == Placement::Top) {
        // Both label and description above track
        float curY = bounds.y;
        if (hasLabel) {
            labelRect = Rect(bounds.x, curY, bounds.w, labelH);
            curY += labelH + (hasDesc ? m.descGap : 0.0f);
        }
        if (hasDesc) {
            descRect = Rect(bounds.x, curY, bounds.w, descH);
            curY += descH;
        }
        curY += m.descGap;
        trackRect = Rect(bounds.x, curY, m.trackSize.x, m.trackSize.y);
        alignX = 0.0f;
    } else if (opts.labelPlacement == Placement::Bottom) {
        // Track on top, both label and description below track
        float curY = bounds.y;
        trackRect = Rect(bounds.x, curY, m.trackSize.x, m.trackSize.y);
        curY += m.trackSize.y + m.descGap;
        if (hasLabel) {
            labelRect = Rect(bounds.x, curY, bounds.w, labelH);
            curY += labelH + (hasDesc ? m.descGap : 0.0f);
        }
        if (hasDesc) {
            descRect = Rect(bounds.x, curY, bounds.w, descH);
        }
        alignX = 0.0f;
    } else if (opts.labelPlacement == Placement::Leading) {
        // Horizontal: Text on left, track on right
        const float trackY = bounds.y + (bounds.h - m.trackSize.y) * 0.5f;
        const float textW = std::max(0.0f, bounds.w - m.trackSize.x - m.gap);
        const float totalTextH = (hasLabel && hasDesc) ? (labelH + m.descGap + descH) : (hasLabel ? labelH : descH);
        const float startY = bounds.y + (bounds.h - totalTextH) * 0.5f;

        if (hasLabel && hasDesc) {
            labelRect = Rect(bounds.x, startY, textW, labelH);
            descRect = Rect(bounds.x, startY + labelH + m.descGap, textW, descH);
        } else if (hasLabel) {
            labelRect = Rect(bounds.x, startY, textW, labelH);
        } else if (hasDesc) {
            descRect = Rect(bounds.x, startY, textW, descH);
        }

        trackRect = Rect(bounds.x + bounds.w - m.trackSize.x, trackY, m.trackSize.x, m.trackSize.y);
        alignX = 1.0f;
    } else { // Trailing (default)
        // Horizontal: Track on left, text on right
        const float trackY = bounds.y + (bounds.h - m.trackSize.y) * 0.5f;
        trackRect = Rect(bounds.x, trackY, m.trackSize.x, m.trackSize.y);

        const float textX = bounds.x + m.trackSize.x + m.gap;
        const float textW = std::max(0.0f, bounds.w - m.trackSize.x - m.gap);
        const float totalTextH = (hasLabel && hasDesc) ? (labelH + m.descGap + descH) : (hasLabel ? labelH : descH);
        const float startY = bounds.y + (bounds.h - totalTextH) * 0.5f;

        if (hasLabel && hasDesc) {
            labelRect = Rect(textX, startY, textW, labelH);
            descRect = Rect(textX, startY + labelH + m.descGap, textW, descH);
        } else if (hasLabel) {
            labelRect = Rect(textX, startY, textW, labelH);
        } else if (hasDesc) {
            descRect = Rect(textX, startY, textW, descH);
        }
        alignX = 0.0f;
    }

    // Determine colors
    const auto& pal = theme.palette;
    Color trackFill;
    Color trackStroke{0, 0, 0, 0};
    float strokeWidth = 0.0f;
    Color knobFill;

    if (checked) {
        const VariantColors& v = theme.GetVariant(opts.variant);
        trackFill = (hovered && !disabled) ? v.bgHover : v.bg;
        trackStroke = Color::Transparent();
        strokeWidth = 0.0f;
        knobFill = pal.controlKnobActive;
    } else {
        trackFill = (hovered && !disabled) ? pal.controlTrackHover : pal.controlTrack;
        trackStroke = (hovered && !disabled) ? pal.borderHover : pal.border;
        strokeWidth = 1.0f;
        knobFill = (hovered && !disabled) ? pal.controlKnobHover : pal.controlKnob;
    }

    Color textCol = pal.textPrimary;
    Color descCol = pal.textSecondary;

    if (disabled) {
        trackFill = trackFill.WithAlphaF(0.40f);
        trackStroke = trackStroke.WithAlphaF(0.40f);
        knobFill = knobFill.WithAlphaF(0.40f);
        textCol = pal.textDisabled;
        descCol = pal.textDisabled;
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

    // Draw text elements
    if (hasLabel && labelRect.w > 0.0f && labelRect.h > 0.0f) {
        canvas.DrawTextBox(effectiveLabel, labelRect, m.fontSize, textCol, alignX, 0.5f);
    }
    if (hasDesc && descRect.w > 0.0f && descRect.h > 0.0f) {
        canvas.DrawTextBox(opts.description, descRect, m.descFontSize, descCol, alignX, 0.5f);
    }
}

} // namespace guikit
