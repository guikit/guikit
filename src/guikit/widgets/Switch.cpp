#include "guikit/widgets/Switch.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

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
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.labelAlign),
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
    const std::string_view effectiveLabel = opts.label;
    const bool hasLabel = !effectiveLabel.empty();
    const bool hasTrailingLabel = isBoth && !opts.trailingLabel.empty();
    const bool hasDesc = !opts.description.empty();

    if (!hasLabel && !hasTrailingLabel && !hasDesc) {
        return m.trackSize;
    }

    Vec2 labelDims = {0.0f, 0.0f};
    if (hasLabel) {
        if (canvas) {
            labelDims = canvas->MeasureText(effectiveLabel, m.fontSize);
        } else {
            const float estW = static_cast<float>(effectiveLabel.length()) * (m.fontSize * kDefaultCharAdvanceRatio);
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

    if (isBoth) {
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

    if (opts.layout == ElementLayout::Stacked) {
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

    if (opts.layout == ElementLayout::Top) {
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

    if (opts.layout == ElementLayout::Bottom) {
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

    // ElementLayout::Inline
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
    const bool isBoth = (opts.layout == ElementLayout::Inline && !opts.trailingLabel.empty());
    const bool isSymmetric = opts.symmetric || isBoth;
    const std::string_view effectiveLabel = opts.label;
    const bool hasLabel = !effectiveLabel.empty();
    const bool hasTrailingLabel = isBoth && !opts.trailingLabel.empty();
    const bool hasDesc = !opts.description.empty();
    const bool hasText = hasLabel || hasTrailingLabel || hasDesc;

    Rect trackRect;
    Rect labelRect;
    Rect trailingLabelRect;
    Rect descRect;
    float alignX = 0.0f;

    const float labelH = hasLabel ? canvas.GetLineHeight(m.fontSize) : 0.0f;
    const float trailingLabelH = hasTrailingLabel ? canvas.GetLineHeight(m.fontSize) : 0.0f;
    const float descH = hasDesc ? canvas.GetLineHeight(m.descFontSize) : 0.0f;

    if (!hasText) {
        const float trackY = bounds.y + (bounds.h - m.trackSize.y) * 0.5f;
        trackRect = Rect(bounds.x, trackY, m.trackSize.x, m.trackSize.y);
    } else if (isBoth) {
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
    } else if (opts.layout == ElementLayout::Stacked) {
        // Label on top, track in middle, description on bottom
        const float naturalTextW = std::max(
            hasLabel ? canvas.MeasureText(opts.label, m.fontSize).x : 0.0f,
            hasDesc ? canvas.MeasureText(opts.description, m.descFontSize).x : 0.0f
        );
        const float blockW = (opts.stretch == StretchMode::Fill) ? bounds.w : std::min(bounds.w, std::max(m.trackSize.x, naturalTextW));
        const float blockX = (opts.stretch == StretchMode::Space) ? (bounds.x + bounds.w - blockW) : bounds.x;
        const float trackX = (opts.stretch == StretchMode::Space) ? (bounds.x + bounds.w - m.trackSize.x) : bounds.x;

        float curY = bounds.y;
        if (hasLabel) {
            labelRect = Rect(blockX, curY, blockW, labelH);
            curY += labelH + m.descGap;
        }
        trackRect = Rect(trackX, curY, m.trackSize.x, m.trackSize.y);
        curY += m.trackSize.y + m.descGap;
        if (hasDesc) {
            descRect = Rect(blockX, curY, blockW, descH);
        }
        alignX = 0.0f;
    } else if (opts.layout == ElementLayout::Top) {
        // Both label and description above track
        const float naturalTextW = std::max(
            hasLabel ? canvas.MeasureText(opts.label, m.fontSize).x : 0.0f,
            hasDesc ? canvas.MeasureText(opts.description, m.descFontSize).x : 0.0f
        );
        const float blockW = (opts.stretch == StretchMode::Fill) ? bounds.w : std::min(bounds.w, std::max(m.trackSize.x, naturalTextW));
        const float blockX = (opts.stretch == StretchMode::Space) ? (bounds.x + bounds.w - blockW) : bounds.x;
        const float trackX = (opts.stretch == StretchMode::Space) ? (bounds.x + bounds.w - m.trackSize.x) : bounds.x;

        float curY = bounds.y;
        if (hasLabel) {
            labelRect = Rect(blockX, curY, blockW, labelH);
            curY += labelH + (hasDesc ? m.descGap : 0.0f);
        }
        if (hasDesc) {
            descRect = Rect(blockX, curY, blockW, descH);
            curY += descH;
        }
        curY += m.descGap;
        trackRect = Rect(trackX, curY, m.trackSize.x, m.trackSize.y);
        alignX = 0.0f;
    } else if (opts.layout == ElementLayout::Bottom) {
        // Track on top, both label and description below track
        const float naturalTextW = std::max(
            hasLabel ? canvas.MeasureText(opts.label, m.fontSize).x : 0.0f,
            hasDesc ? canvas.MeasureText(opts.description, m.descFontSize).x : 0.0f
        );
        const float blockW = (opts.stretch == StretchMode::Fill) ? bounds.w : std::min(bounds.w, std::max(m.trackSize.x, naturalTextW));
        const float blockX = (opts.stretch == StretchMode::Space) ? (bounds.x + bounds.w - blockW) : bounds.x;
        const float trackX = (opts.stretch == StretchMode::Space) ? (bounds.x + bounds.w - m.trackSize.x) : bounds.x;

        float curY = bounds.y;
        trackRect = Rect(trackX, curY, m.trackSize.x, m.trackSize.y);
        curY += m.trackSize.y + m.descGap;
        if (hasLabel) {
            labelRect = Rect(blockX, curY, blockW, labelH);
            curY += labelH + (hasDesc ? m.descGap : 0.0f);
        }
        if (hasDesc) {
            descRect = Rect(blockX, curY, blockW, descH);
        }
        alignX = 0.0f;
    } else { // ElementLayout::Inline
        const float naturalTextW = std::max(
            hasLabel ? canvas.MeasureText(opts.label, m.fontSize).x : 0.0f,
            hasDesc ? canvas.MeasureText(opts.description, m.descFontSize).x : 0.0f
        );
        const float trackY = bounds.y + (bounds.h - m.trackSize.y) * 0.5f;
        const float totalTextH = (hasLabel && hasDesc) ? (labelH + m.descGap + descH) : (hasLabel ? labelH : descH);
        const float startY = bounds.y + (bounds.h - totalTextH) * 0.5f;

        if (opts.inlineOrder == InlineOrder::LabelFirst) {
            // Label first (left), track second (right)
            float trackX = bounds.x + bounds.w - m.trackSize.x;
            if (opts.stretch == StretchMode::Compact) {
                trackX = bounds.x + naturalTextW + m.gap;
            }
            trackRect = Rect(trackX, trackY, m.trackSize.x, m.trackSize.y);

            if (hasLabel && hasDesc) {
                labelRect = Rect(bounds.x, startY, naturalTextW, labelH);
                descRect = Rect(bounds.x, startY + labelH + m.descGap, naturalTextW, descH);
            } else if (hasLabel) {
                labelRect = Rect(bounds.x, startY, naturalTextW, labelH);
            } else if (hasDesc) {
                descRect = Rect(bounds.x, startY, naturalTextW, descH);
            }
        } else {
            // InlineOrder::ControlFirst: Track first (left), text second (right)
            trackRect = Rect(bounds.x, trackY, m.trackSize.x, m.trackSize.y);

            float textX = bounds.x + m.trackSize.x + m.gap;
            if (opts.stretch == StretchMode::Space) {
                textX = std::max(bounds.x + m.trackSize.x + m.gap, bounds.x + bounds.w - naturalTextW);
            }

            if (hasLabel && hasDesc) {
                labelRect = Rect(textX, startY, naturalTextW, labelH);
                descRect = Rect(textX, startY + labelH + m.descGap, naturalTextW, descH);
            } else if (hasLabel) {
                labelRect = Rect(textX, startY, naturalTextW, labelH);
            } else if (hasDesc) {
                descRect = Rect(textX, startY, naturalTextW, descH);
            }
        }
    }

    // Determine colors
    const auto& pal = theme.palette;
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
    if (isBoth) {
        if (hasLabel && labelRect.w > 0.0f && labelRect.h > 0.0f) {
            const Color leftCol = disabled ? pal.textDisabled : (checked ? pal.textMuted : pal.textPrimary);
            canvas.DrawTextBox(opts.label, labelRect, m.fontSize, leftCol, 1.0f, 0.5f);
        }
        if (hasTrailingLabel && trailingLabelRect.w > 0.0f && trailingLabelRect.h > 0.0f) {
            const Color rightCol = disabled ? pal.textDisabled : (checked ? pal.textPrimary : pal.textMuted);
            canvas.DrawTextBox(opts.trailingLabel, trailingLabelRect, m.fontSize, rightCol, 0.0f, 0.5f);
        }
        if (hasDesc && descRect.w > 0.0f && descRect.h > 0.0f) {
            canvas.DrawTextBox(opts.description, descRect, m.descFontSize, descCol, 0.0f, 0.5f);
        }
    } else {
        const float alignFactor = (opts.labelAlign == Align::End) ? 1.0f : ((opts.labelAlign == Align::Center) ? 0.5f : 0.0f);
        if (hasLabel && labelRect.w > 0.0f && labelRect.h > 0.0f) {
            canvas.DrawTextBox(effectiveLabel, labelRect, m.fontSize, textCol, alignFactor, 0.5f);
        }
        if (hasDesc && descRect.w > 0.0f && descRect.h > 0.0f) {
            canvas.DrawTextBox(opts.description, descRect, m.descFontSize, descCol, alignFactor, 0.5f);
        }
    }
}

} // namespace guikit
