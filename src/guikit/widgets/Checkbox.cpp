#include "guikit/widgets/Checkbox.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

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
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.labelAlign),
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
    const std::string_view effectiveLabel = opts.label;
    const bool hasLabel = !effectiveLabel.empty();
    const bool hasDesc = !opts.description.empty();

    if (!hasLabel && !hasDesc) {
        return {m.boxSize, m.boxSize};
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

    if (opts.layout == ElementLayout::Stacked) {
        // Label on top, checkbox in middle, description on bottom
        const float maxW = std::max({m.boxSize, labelDims.x, descDims.x});
        float totalH = m.boxSize;
        if (hasLabel) {
            totalH += labelDims.y + m.descGap;
        }
        if (hasDesc) {
            totalH += m.descGap + descDims.y;
        }
        return {maxW, totalH};
    }

    if (opts.layout == ElementLayout::Top) {
        // Both label and description above checkbox
        const float maxW = std::max({m.boxSize, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasLabel && hasDesc) {
            textH = labelDims.y + m.descGap + descDims.y;
        } else if (hasLabel) {
            textH = labelDims.y;
        } else if (hasDesc) {
            textH = descDims.y;
        }
        return {maxW, textH + m.descGap + m.boxSize};
    }

    if (opts.layout == ElementLayout::Bottom) {
        // Checkbox above, both label and description below checkbox
        const float maxW = std::max({m.boxSize, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasLabel && hasDesc) {
            textH = labelDims.y + m.descGap + descDims.y;
        } else if (hasLabel) {
            textH = labelDims.y;
        } else if (hasDesc) {
            textH = descDims.y;
        }
        return {maxW, m.boxSize + m.descGap + textH};
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

    const float totalW = m.boxSize + m.gap + textW;
    const float totalH = std::max(m.boxSize, textH);
    return {totalW, totalH};
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
    const std::string_view effectiveLabel = opts.label;
    const bool hasLabel = !effectiveLabel.empty();
    const bool hasDesc = !opts.description.empty();
    const bool hasText = hasLabel || hasDesc;

    Rect boxRect;
    Rect labelRect;
    Rect descRect;
    float alignX = 0.0f;

    const float labelH = hasLabel ? canvas.GetLineHeight(m.fontSize) : 0.0f;
    const float descH = hasDesc ? canvas.GetLineHeight(m.descFontSize) : 0.0f;

    if (!hasText) {
        const float boxY = bounds.y + (bounds.h - m.boxSize) * 0.5f;
        boxRect = Rect(bounds.x, boxY, m.boxSize, m.boxSize);
    } else if (opts.layout == ElementLayout::Stacked) {
        // Label on top, checkbox in middle, description on bottom
        const float naturalTextW = std::max(
            hasLabel ? canvas.MeasureText(opts.label, m.fontSize).x : 0.0f,
            hasDesc ? canvas.MeasureText(opts.description, m.descFontSize).x : 0.0f
        );
        const float blockW = (opts.stretch == StretchMode::Fill) ? bounds.w : std::min(bounds.w, std::max(m.boxSize, naturalTextW));
        const float blockX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - blockW) : bounds.x;
        const float boxX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - m.boxSize) : bounds.x;

        float curY = bounds.y;
        if (hasLabel) {
            labelRect = Rect(blockX, curY, blockW, labelH);
            curY += labelH + m.descGap;
        }
        boxRect = Rect(boxX, curY, m.boxSize, m.boxSize);
        curY += m.boxSize + m.descGap;
        if (hasDesc) {
            descRect = Rect(blockX, curY, blockW, descH);
        }
        alignX = 0.0f;
    } else if (opts.layout == ElementLayout::Top) {
        // Both label and description above checkbox
        const float naturalTextW = std::max(
            hasLabel ? canvas.MeasureText(opts.label, m.fontSize).x : 0.0f,
            hasDesc ? canvas.MeasureText(opts.description, m.descFontSize).x : 0.0f
        );
        const float blockW = (opts.stretch == StretchMode::Fill) ? bounds.w : std::min(bounds.w, std::max(m.boxSize, naturalTextW));
        const float blockX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - blockW) : bounds.x;
        const float boxX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - m.boxSize) : bounds.x;

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
        boxRect = Rect(boxX, curY, m.boxSize, m.boxSize);
        alignX = 0.0f;
    } else if (opts.layout == ElementLayout::Bottom) {
        // Checkbox on top, both label and description below checkbox
        const float naturalTextW = std::max(
            hasLabel ? canvas.MeasureText(opts.label, m.fontSize).x : 0.0f,
            hasDesc ? canvas.MeasureText(opts.description, m.descFontSize).x : 0.0f
        );
        const float blockW = (opts.stretch == StretchMode::Fill) ? bounds.w : std::min(bounds.w, std::max(m.boxSize, naturalTextW));
        const float blockX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - blockW) : bounds.x;
        const float boxX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - m.boxSize) : bounds.x;

        float curY = bounds.y;
        boxRect = Rect(boxX, curY, m.boxSize, m.boxSize);
        curY += m.boxSize + m.descGap;
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
        const float boxY = bounds.y + (bounds.h - m.boxSize) * 0.5f;
        const float totalTextH = (hasLabel && hasDesc) ? (labelH + m.descGap + descH) : (hasLabel ? labelH : descH);
        const float startY = bounds.y + (bounds.h - totalTextH) * 0.5f;

        if (opts.inlineOrder == InlineOrder::LabelFirst) {
            // Label first (left), checkbox second (right)
            float boxX = bounds.x + bounds.w - m.boxSize;
            if (opts.stretch == StretchMode::Compact) {
                boxX = bounds.x + naturalTextW + m.gap;
            }
            boxRect = Rect(boxX, boxY, m.boxSize, m.boxSize);

            if (hasLabel && hasDesc) {
                labelRect = Rect(bounds.x, startY, naturalTextW, labelH);
                descRect = Rect(bounds.x, startY + labelH + m.descGap, naturalTextW, descH);
            } else if (hasLabel) {
                labelRect = Rect(bounds.x, startY, naturalTextW, labelH);
            } else if (hasDesc) {
                descRect = Rect(bounds.x, startY, naturalTextW, descH);
            }
        } else {
            // InlineOrder::ControlFirst: Checkbox first (left), text second (right)
            boxRect = Rect(bounds.x, boxY, m.boxSize, m.boxSize);

            float textX = bounds.x + m.boxSize + m.gap;
            if (opts.stretch == StretchMode::Between) {
                textX = std::max(bounds.x + m.boxSize + m.gap, bounds.x + bounds.w - naturalTextW);
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

    Color textCol = pal.textPrimary;
    Color descCol = pal.textSecondary;

    if (disabled) {
        boxFill = boxFill.WithAlphaF(0.40f);
        boxStroke = boxStroke.WithAlphaF(0.40f);
        markColor = markColor.WithAlphaF(0.40f);
        textCol = pal.textDisabled;
        descCol = pal.textDisabled;
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

    // Draw text elements with labelAlign
    const float alignFactor = (opts.labelAlign == Align::End) ? 1.0f : ((opts.labelAlign == Align::Center) ? 0.5f : 0.0f);
    if (hasLabel && labelRect.w > 0.0f && labelRect.h > 0.0f) {
        canvas.DrawTextBox(effectiveLabel, labelRect, m.fontSize, textCol, alignFactor, 0.5f);
    }
    if (hasDesc && descRect.w > 0.0f && descRect.h > 0.0f) {
        canvas.DrawTextBox(opts.description, descRect, m.descFontSize, descCol, alignFactor, 0.5f);
    }
}

} // namespace guikit
