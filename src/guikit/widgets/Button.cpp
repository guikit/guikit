#include "guikit/widgets/Button.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

Response Button::Run(Context& ctx, const ButtonOptions& opts, std::source_location loc) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.rightClicked = false;
        res.doubleClicked = false;
    }

    Node* node = ctx.CreateNode(NodeType::Button, id);
    node->text = ctx.AllocString(opts.label);
    auto* bOpts = ctx.AllocPayload<ButtonOptions>(opts);
    bOpts->label = node->text;
    if (!opts.text.empty()) {
        bOpts->text = ctx.AllocString(opts.text);
    }
    if (!opts.description.empty()) {
        bOpts->description = ctx.AllocString(opts.description);
    }
    node->payload = bOpts;
    node->visualHash = detail::HashArgs(
        opts.variant,
        opts.descriptionVariant,
        opts.iconPlacement,
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        opts.controlAlign,
        opts.active,
        static_cast<uint8_t>(opts.stretch),
        opts.width.value_or(0.0f),
        opts.minWidth.value_or(0.0f),
        opts.size.value_or(activeSize),
        opts.radius.value_or(-1.0f),
        static_cast<uint8_t>(opts.icon.GetType()),
        static_cast<uint8_t>(opts.icon.GetId()),
        opts.icon.GetPath(),
        node->text,
        bOpts->text,
        bOpts->description
    );
    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* b = n->As<ButtonOptions>();
        return Button::Measure(b ? *b : ButtonOptions{}, n->uiSize, canvas, theme);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* b = n->As<ButtonOptions>();
        Button::Render(canvas, n->computedRect, b ? *b : ButtonOptions{}, n->uiSize, n->hovered, n->pressed, n->disabled, theme);
    };
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;
    node->minWidth = ctx.GetTheme().GetMetrics(activeSize).defaultHeight;

    ctx.AdaptChildToContainer(node);

    return res;
}

namespace {

Vec2 MeasureButtonCapsule(
    const ButtonOptions& opts,
    std::string_view effectiveCapsuleText,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const bool hasIcon = opts.icon.IsValid();
    const bool hasCapsuleText = !effectiveCapsuleText.empty();

    // Icon-only button with no text variants defaults to square
    if (hasIcon && !hasCapsuleText && opts.textVariants.empty()) {
        const float w = (opts.stretch != StretchMode::Fill && opts.width.has_value()) ? *opts.width : m.defaultHeight;
        return {w, m.defaultHeight};
    }

    float maxTextW = 0.0f;
    if (hasCapsuleText) {
        if (canvas) {
            maxTextW = canvas->MeasureText(effectiveCapsuleText, m.fontSize).x;
        } else {
            maxTextW = static_cast<float>(effectiveCapsuleText.length()) * (m.fontSize * kDefaultCharAdvanceRatio);
        }
    }

    // Evaluate all candidate text variants to ensure stable sizing across state changes
    for (std::string_view var : opts.textVariants) {
        if (var.empty()) continue;
        float varW = 0.0f;
        if (canvas) {
            varW = canvas->MeasureText(var, m.fontSize).x;
        } else {
            varW = static_cast<float>(var.length()) * (m.fontSize * kDefaultCharAdvanceRatio);
        }
        maxTextW = std::max(maxTextW, varW);
    }

    float contentW = 0.0f;
    float contentH = m.defaultHeight;
    if (hasIcon && maxTextW > 0.0f) {
        if (opts.iconPlacement == Placement::Top || opts.iconPlacement == Placement::Bottom) {
            contentW = std::max(m.iconSize, maxTextW);
            contentH = std::max(m.defaultHeight, m.iconSize + 4.0f + m.fontSize + m.paddingY * 2.0f);
        } else {
            const float gap = 8.0f;
            contentW = m.iconSize + gap + maxTextW;
        }
    } else if (maxTextW > 0.0f) {
        contentW = maxTextW;
    }

    float totalW = contentW + m.paddingX * 2.0f;
    if (opts.stretch != StretchMode::Fill && opts.width.has_value()) {
        totalW = *opts.width;
    } else if (opts.minWidth.has_value()) {
        totalW = std::max({m.defaultHeight, *opts.minWidth, totalW});
    } else {
        totalW = std::max(m.defaultHeight, totalW);
    }
    return {totalW, contentH};
}

} // namespace

Vec2 Button::Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas, const UiTheme& theme) {
    const std::string_view effectiveCapsuleText = !opts.text.empty() ? opts.text : (opts.description.empty() ? opts.label : std::string_view{});
    const std::string_view effectiveExternalLabel = !opts.text.empty() ? opts.label : (!opts.description.empty() ? opts.label : std::string_view{});
    const std::string_view effectiveDescription = opts.description;
    const bool hasExternalLabel = !effectiveExternalLabel.empty();
    const bool hasExternalDesc = !effectiveDescription.empty();
    const bool hasExternal = hasExternalLabel || hasExternalDesc;

    const Vec2 capsuleDims = MeasureButtonCapsule(opts, effectiveCapsuleText, size, canvas, theme);

    if (!hasExternal) {
        return capsuleDims;
    }

    const ComponentMetrics& m = theme.GetMetrics(size);
    const auto& inputMetrics = theme.GetInputField(size);

    Vec2 labelDims = {0.0f, 0.0f};
    if (hasExternalLabel) {
        if (canvas) {
            labelDims = canvas->MeasureText(effectiveExternalLabel, inputMetrics.labelFontSize);
        } else {
            labelDims = {
                static_cast<float>(effectiveExternalLabel.length()) * (inputMetrics.labelFontSize * kDefaultCharAdvanceRatio),
                theme.GetLineHeight(inputMetrics.labelFontSize)
            };
        }
    }

    Vec2 descDims = {0.0f, 0.0f};
    if (hasExternalDesc) {
        if (canvas) {
            descDims = canvas->MeasureText(effectiveDescription, inputMetrics.descFontSize);
        } else {
            descDims = {
                static_cast<float>(effectiveDescription.length()) * (inputMetrics.descFontSize * kDefaultCharAdvanceRatio),
                theme.GetLineHeight(inputMetrics.descFontSize)
            };
        }
    }

    if (opts.layout == ElementLayout::Stacked) {
        const float totalW = std::max({capsuleDims.x, labelDims.x, descDims.x});
        float totalH = capsuleDims.y;
        if (hasExternalLabel) totalH += labelDims.y + inputMetrics.labelGap;
        if (hasExternalDesc)  totalH += inputMetrics.descGap + descDims.y;
        return {totalW, totalH};
    }

    if (opts.layout == ElementLayout::Top) {
        const float totalW = std::max({capsuleDims.x, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasExternalLabel) textH += labelDims.y;
        if (hasExternalDesc)  textH += (hasExternalLabel ? inputMetrics.descGap : 0.0f) + descDims.y;
        return {totalW, textH + inputMetrics.labelGap + capsuleDims.y};
    }

    if (opts.layout == ElementLayout::Bottom) {
        const float totalW = std::max({capsuleDims.x, labelDims.x, descDims.x});
        float textH = 0.0f;
        if (hasExternalLabel) textH += labelDims.y;
        if (hasExternalDesc)  textH += (hasExternalLabel ? inputMetrics.descGap : 0.0f) + descDims.y;
        return {totalW, capsuleDims.y + inputMetrics.descGap + textH};
    }

    // ElementLayout::Inline
    const float textW = std::max(labelDims.x, descDims.x);
    float textH = 0.0f;
    if (hasExternalLabel && hasExternalDesc) {
        textH = labelDims.y + inputMetrics.descGap + descDims.y;
    } else if (hasExternalLabel) {
        textH = labelDims.y;
    } else if (hasExternalDesc) {
        textH = descDims.y;
    }
    const float gap = m.paddingX;
    const float totalW = capsuleDims.x + (textW > 0.0f ? gap + textW : 0.0f);
    const float totalH = std::max(capsuleDims.y, textH);
    return {totalW, totalH};
}

Vec2 Button::Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme);
}

void Button::Render(
    ICanvas& canvas,
    Rect bounds,
    const ButtonOptions& opts,
    UiSize size,
    bool hovered,
    bool pressed,
    bool disabled,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const ComponentMetrics& m = theme.GetMetrics(size);
    const auto& inputMetrics = theme.GetInputField(size);
    const auto& pal = theme.palette;

    const std::string_view effectiveCapsuleText = !opts.text.empty() ? opts.text : (opts.description.empty() ? opts.label : std::string_view{});
    const std::string_view effectiveExternalLabel = !opts.text.empty() ? opts.label : (!opts.description.empty() ? opts.label : std::string_view{});
    const std::string_view effectiveDescription = opts.description;
    const bool hasExternalLabel = !effectiveExternalLabel.empty();
    const bool hasExternalDesc = !effectiveDescription.empty();
    const bool hasExternal = hasExternalLabel || hasExternalDesc;

    Rect capsuleRect = bounds;
    Rect labelRect;
    Rect descRect;

    const Vec2 capSize = MeasureButtonCapsule(opts, effectiveCapsuleText, size, &canvas, theme);
    const float alignFactor = (opts.elementAlign == Align::End) ? 1.0f : ((opts.elementAlign == Align::Center) ? 0.5f : 0.0f);

    const auto resolveCapsuleWidth = [&](float intrinsicW) -> float {
        if (opts.stretch == StretchMode::Fill) {
            const float pct = std::clamp(opts.width.value_or(100.0f), 0.0f, 100.0f);
            return bounds.w * (pct * 0.01f);
        }
        return std::min(bounds.w, opts.width.value_or(intrinsicW));
    };

    if (!hasExternal) {
        const float capW = resolveCapsuleWidth(capSize.x);
        const float capX = bounds.x + (bounds.w - capW) * alignFactor;
        capsuleRect = Rect(capX, bounds.y, capW, bounds.h);
    } else {
        const float labelH = hasExternalLabel ? canvas.GetLineHeight(inputMetrics.labelFontSize) : 0.0f;
        const float descH = hasExternalDesc ? canvas.GetLineHeight(inputMetrics.descFontSize) : 0.0f;

        const float measuredLabelW = hasExternalLabel ? canvas.MeasureText(effectiveExternalLabel, inputMetrics.labelFontSize).x : 0.0f;
        const float measuredDescW = hasExternalDesc ? canvas.MeasureText(effectiveDescription, inputMetrics.descFontSize).x : 0.0f;
        const float measuredTextW = std::max(measuredLabelW, measuredDescW);
        const float capW = resolveCapsuleWidth(capSize.x);

        if (opts.layout == ElementLayout::Stacked) {
            const float blockW = (opts.stretch == StretchMode::Compact) ? std::min(bounds.w, std::max(capW, measuredTextW)) : bounds.w;
            const float blockX = bounds.x;
            const float capX = blockX + (blockW - capW) * alignFactor;

            float curY = bounds.y;
            if (hasExternalLabel) {
                labelRect = Rect(blockX, curY, blockW, labelH);
                curY += labelH + inputMetrics.labelGap;
            }
            capsuleRect = Rect(capX, curY, capW, capSize.y);
            curY += capsuleRect.h + inputMetrics.descGap;
            if (hasExternalDesc) {
                descRect = Rect(blockX, curY, blockW, descH);
            }
        } else if (opts.layout == ElementLayout::Top) {
            const float blockW = (opts.stretch == StretchMode::Compact) ? std::min(bounds.w, std::max(capW, measuredTextW)) : bounds.w;
            const float blockX = bounds.x;
            const float capX = blockX + (blockW - capW) * alignFactor;

            float curY = bounds.y;
            if (hasExternalLabel) {
                labelRect = Rect(blockX, curY, blockW, labelH);
                curY += labelH + (hasExternalDesc ? inputMetrics.descGap : 0.0f);
            }
            if (hasExternalDesc) {
                descRect = Rect(blockX, curY, blockW, descH);
                curY += descH;
            }
            curY += inputMetrics.labelGap;
            capsuleRect = Rect(capX, curY, capW, capSize.y);
        } else if (opts.layout == ElementLayout::Bottom) {
            const float blockW = (opts.stretch == StretchMode::Compact) ? std::min(bounds.w, std::max(capW, measuredTextW)) : bounds.w;
            const float blockX = bounds.x;
            const float capX = blockX + (blockW - capW) * alignFactor;

            float curY = bounds.y;
            capsuleRect = Rect(capX, curY, capW, capSize.y);
            curY += capsuleRect.h + inputMetrics.descGap;
            if (hasExternalLabel) {
                labelRect = Rect(blockX, curY, blockW, labelH);
                curY += labelH + (hasExternalDesc ? inputMetrics.descGap : 0.0f);
            }
            if (hasExternalDesc) {
                descRect = Rect(blockX, curY, blockW, descH);
            }
        } else { // ElementLayout::Inline
            const float gap = m.paddingX;
            const float availTextW = std::max(0.0f, bounds.w - capW - gap);
            const float textBlockW = std::min(measuredTextW, availTextW);

            const float totalTextH = (hasExternalLabel && hasExternalDesc) ? (labelH + inputMetrics.descGap + descH) : (hasExternalLabel ? labelH : descH);
            const float startY = bounds.y + (bounds.h - totalTextH) * 0.5f;

            if (opts.inlineOrder == InlineOrder::LabelFirst) {
                // Label first (left), capsule second (right)
                float capX = bounds.x + bounds.w - capW;
                if (opts.stretch == StretchMode::Compact) {
                    capX = bounds.x + (textBlockW > 0.0f ? textBlockW + gap : 0.0f);
                }
                capsuleRect = Rect(capX, bounds.y + (bounds.h - capSize.y) * 0.5f, capW, capSize.y);

                float textY = startY;
                if (hasExternalLabel) {
                    labelRect = Rect(bounds.x, textY, textBlockW, labelH);
                    textY += labelH + inputMetrics.descGap;
                }
                if (hasExternalDesc) {
                    descRect = Rect(bounds.x, textY, textBlockW, descH);
                }
            } else {
                // InlineOrder::ControlFirst: Capsule first (left), text second (right)
                capsuleRect = Rect(bounds.x, bounds.y + (bounds.h - capSize.y) * 0.5f, capW, capSize.y);

                float textX = bounds.x + capW + gap;
                if (opts.stretch != StretchMode::Compact) {
                    textX = std::max(bounds.x + capW + gap, bounds.x + bounds.w - textBlockW);
                }
                float textY = startY;
                if (hasExternalLabel) {
                    labelRect = Rect(textX, textY, textBlockW, labelH);
                    textY += labelH + inputMetrics.descGap;
                }
                if (hasExternalDesc) {
                    descRect = Rect(textX, textY, textBlockW, descH);
                }
            }
        }

        // Draw external label and description with elementAlign and automatic truncation
        Color labelCol = pal.textPrimary;
        Color descCol = theme.GetTextColor(opts.descriptionVariant);
        if (disabled) {
            labelCol = labelCol.WithAlphaF(0.40f);
            descCol = descCol.WithAlphaF(0.40f);
        }

        if (hasExternalLabel && labelRect.w > 0.0f && labelRect.h > 0.0f) {
            std::string truncLabel;
            std::string_view drawLabel = effectiveExternalLabel;
            if (measuredLabelW > labelRect.w + kTextSubpixelTolerance) {
                truncLabel = TextUtil::TruncateWithEllipsis(effectiveExternalLabel, labelRect.w, inputMetrics.labelFontSize, &canvas);
                drawLabel = truncLabel;
            }
            canvas.DrawTextBox(drawLabel, labelRect, inputMetrics.labelFontSize, labelCol, alignFactor, 0.5f);
        }
        if (hasExternalDesc && descRect.w > 0.0f && descRect.h > 0.0f) {
            std::string truncDesc;
            std::string_view drawDesc = effectiveDescription;
            if (measuredDescW > descRect.w + kTextSubpixelTolerance) {
                truncDesc = TextUtil::TruncateWithEllipsis(effectiveDescription, descRect.w, inputMetrics.descFontSize, &canvas);
                drawDesc = truncDesc;
            }
            canvas.DrawTextBox(drawDesc, descRect, inputMetrics.descFontSize, descCol, alignFactor, 0.5f);
        }
    }

    const VariantColors& colors = theme.GetVariant(opts.variant);

    Color bg = colors.bg;
    Color textCol = colors.text;
    Color borderCol = colors.border;
    float borderW = colors.borderWidth;

    if (disabled) {
        bg = bg.WithAlphaF(0.40f);
        textCol = textCol.WithAlphaF(0.40f);
        borderCol = borderCol.WithAlphaF(0.40f);
    } else if (pressed) {
        bg = colors.bgActive;
    } else if (opts.active) {
        bg = hovered ? colors.bgHover : colors.bgActive;
        if (opts.variant == UiVariant::Ghost) {
            borderCol = colors.text.WithAlphaF(0.35f);
            borderW = std::max(borderW, 1.0f);
        }
    } else if (hovered) {
        bg = colors.bgHover;
    }

    // Clip all button capsule content strictly to capsuleRect to prevent any bleeding
    canvas.PushClip(capsuleRect);

    // Background & border
    const float radius = opts.radius.value_or(m.cornerRadius);
    canvas.DrawRoundRect(capsuleRect, radius, bg, borderCol, borderW);

    const bool hasIcon = opts.icon.IsValid();
    const bool hasCapsuleText = !effectiveCapsuleText.empty();

    if (hasIcon && !hasCapsuleText) {
        // Icon-only
        const float isz = m.iconSize;
        float iconX = capsuleRect.x + (capsuleRect.w - isz) * 0.5f;
        if (opts.controlAlign == Align::Start) {
            iconX = capsuleRect.x + m.paddingX;
        } else if (opts.controlAlign == Align::End) {
            iconX = capsuleRect.x + capsuleRect.w - m.paddingX - isz;
        }
        const Rect iconBounds(
            iconX,
            capsuleRect.y + (capsuleRect.h - isz) * 0.5f,
            isz, isz
        );
        opts.icon.Draw(canvas, iconBounds, textCol);
    } else if (!hasIcon && hasCapsuleText) {
        // Label-only with automatic ellipsis
        const float availW = std::max(0.0f, capsuleRect.w - m.paddingX * 2.0f);
        const Rect textRect(capsuleRect.x + m.paddingX, capsuleRect.y, availW, capsuleRect.h);
        float alignX = 0.5f;
        if (opts.controlAlign == Align::Start) {
            alignX = 0.0f;
        } else if (opts.controlAlign == Align::End) {
            alignX = 1.0f;
        }
        canvas.DrawTextBox(effectiveCapsuleText, textRect, m.fontSize, textCol, alignX, 0.5f);
    } else if (hasIcon && hasCapsuleText) {
        // Icon + Label
        if (opts.iconPlacement == Placement::Top) {
            const float gap = 4.0f;
            const float textH = canvas.GetLineHeight(m.fontSize);
            const float totalContentH = m.iconSize + gap + textH;
            const float startY = capsuleRect.y + (capsuleRect.h - totalContentH) * 0.5f;
            float iconX = capsuleRect.x + (capsuleRect.w - m.iconSize) * 0.5f;
            float alignX = 0.5f;
            if (opts.controlAlign == Align::Start) {
                iconX = capsuleRect.x + m.paddingX;
                alignX = 0.0f;
            } else if (opts.controlAlign == Align::End) {
                iconX = capsuleRect.x + capsuleRect.w - m.paddingX - m.iconSize;
                alignX = 1.0f;
            }
            opts.icon.Draw(canvas, Rect(iconX, startY, m.iconSize, m.iconSize), textCol);
            const Rect textRect(capsuleRect.x + m.paddingX, startY + m.iconSize + gap, std::max(0.0f, capsuleRect.w - m.paddingX * 2.0f), textH);
            canvas.DrawTextBox(effectiveCapsuleText, textRect, m.fontSize, textCol, alignX, 0.5f);
        } else if (opts.iconPlacement == Placement::Bottom) {
            const float gap = 4.0f;
            const float textH = canvas.GetLineHeight(m.fontSize);
            const float totalContentH = m.iconSize + gap + textH;
            const float startY = capsuleRect.y + (capsuleRect.h - totalContentH) * 0.5f;
            float iconX = capsuleRect.x + (capsuleRect.w - m.iconSize) * 0.5f;
            float alignX = 0.5f;
            if (opts.controlAlign == Align::Start) {
                iconX = capsuleRect.x + m.paddingX;
                alignX = 0.0f;
            } else if (opts.controlAlign == Align::End) {
                iconX = capsuleRect.x + capsuleRect.w - m.paddingX - m.iconSize;
                alignX = 1.0f;
            }
            const Rect textRect(capsuleRect.x + m.paddingX, startY, std::max(0.0f, capsuleRect.w - m.paddingX * 2.0f), textH);
            canvas.DrawTextBox(effectiveCapsuleText, textRect, m.fontSize, textCol, alignX, 0.5f);
            opts.icon.Draw(canvas, Rect(iconX, startY + textH + gap, m.iconSize, m.iconSize), textCol);
        } else if (opts.iconPlacement == Placement::Trailing) {
            const float gap = 8.0f;
            const float availTextW = std::max(0.0f, capsuleRect.w - m.paddingX * 2.0f - m.iconSize - gap);
            float textW = canvas.MeasureText(effectiveCapsuleText, m.fontSize).x;
            std::string displayLabel(effectiveCapsuleText);
            if (textW > availTextW + kTextSubpixelTolerance && availTextW > 0.0f) {
                displayLabel = TextUtil::TruncateWithEllipsis(effectiveCapsuleText, availTextW, m.fontSize, &canvas);
                textW = canvas.MeasureText(displayLabel, m.fontSize).x;
            }
            const float totalContentW = m.iconSize + gap + textW;
            const float centeredStartX = capsuleRect.x + (capsuleRect.w - totalContentW) * 0.5f;
            const float minStartX = capsuleRect.x + m.paddingX;
            const float maxStartX = capsuleRect.x + capsuleRect.w - m.paddingX - totalContentW;
            float startX = centeredStartX;
            if (opts.controlAlign == Align::Start) {
                startX = minStartX;
            } else if (opts.controlAlign == Align::End) {
                startX = maxStartX;
            } else if (!opts.textVariants.empty() || capsuleRect.w < totalContentW + m.paddingX * 2.0f) {
                startX = minStartX;
            }
            const float iconY = capsuleRect.y + (capsuleRect.h - m.iconSize) * 0.5f;
            const Rect textRect(startX, capsuleRect.y, textW, capsuleRect.h);
            canvas.DrawTextBox(displayLabel, textRect, m.fontSize, textCol, 0.0f, 0.5f);
            opts.icon.Draw(canvas, Rect(startX + textW + gap, iconY, m.iconSize, m.iconSize), textCol);
        } else { // Leading / default
            const float gap = 8.0f;
            const float availTextW = std::max(0.0f, capsuleRect.w - m.paddingX * 2.0f - m.iconSize - gap);
            float textW = canvas.MeasureText(effectiveCapsuleText, m.fontSize).x;
            std::string displayLabel(effectiveCapsuleText);
            if (textW > availTextW + kTextSubpixelTolerance && availTextW > 0.0f) {
                displayLabel = TextUtil::TruncateWithEllipsis(effectiveCapsuleText, availTextW, m.fontSize, &canvas);
                textW = canvas.MeasureText(displayLabel, m.fontSize).x;
            }
            const float totalContentW = m.iconSize + gap + textW;
            const float centeredStartX = capsuleRect.x + (capsuleRect.w - totalContentW) * 0.5f;
            const float minStartX = capsuleRect.x + m.paddingX;
            const float maxStartX = capsuleRect.x + capsuleRect.w - m.paddingX - totalContentW;
            float startX = centeredStartX;
            if (opts.controlAlign == Align::Start) {
                startX = minStartX;
            } else if (opts.controlAlign == Align::End) {
                startX = maxStartX;
            } else if (!opts.textVariants.empty() || capsuleRect.w < totalContentW + m.paddingX * 2.0f) {
                startX = minStartX;
            }
            const float iconY = capsuleRect.y + (capsuleRect.h - m.iconSize) * 0.5f;
            opts.icon.Draw(canvas, Rect(startX, iconY, m.iconSize, m.iconSize), textCol);
            const float remainingW = std::max(0.0f, (capsuleRect.x + capsuleRect.w - m.paddingX) - (startX + m.iconSize + gap));
            const Rect textRect(startX + m.iconSize + gap, capsuleRect.y, remainingW, capsuleRect.h);
            canvas.DrawTextBox(displayLabel, textRect, m.fontSize, textCol, 0.0f, 0.5f);
        }
    }

    canvas.PopClip();
}

} // namespace guikit
