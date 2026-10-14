#include "guikit/widgets/Button.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/ElementComposer.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

Response Button::Run(Context& ctx, const ButtonOptions& opts, std::source_location loc) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());

    ctx.RegisterFocusable(id, effectiveDisabled);

    Response res = ctx.Interact(id);
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

    Node* node = ctx.CreateNode(NodeType::Button, id);
    auto* bOpts = ctx.AllocPayload<ButtonOptions>(opts);
    bOpts->label = ctx.AllocText(opts.label);
    bOpts->text = ctx.AllocText(opts.text);
    bOpts->description = ctx.AllocText(opts.description);
    bOpts->tooltip = ctx.AllocText(opts.tooltip);
    node->text = bOpts->label.text;
    node->SetPayload(bOpts);
    node->visualHash = detail::HashArgs(
        opts.variant,
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
        bOpts->label,
        bOpts->text,
        bOpts->description
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* b = n->As<ButtonOptions>();
        return Button::Measure(b ? *b : ButtonOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* b = n->As<ButtonOptions>();
        Button::Render(canvas, n->computedRect, b ? *b : ButtonOptions{}, n->uiSize, n->hovered, n->pressed, n->disabled, theme, n->focused);
    };
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->focused = ctx.IsFocused(id);
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;
    node->minWidth = ctx.GetTheme().GetMetrics(activeSize).defaultHeight;

    ctx.AdaptChildToContainer(node);

    return res;
}

namespace {

Vec2 MeasureButtonCapsule(
    const ButtonOptions& opts,
    const UiText& effectiveCapsuleText,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const float capFontSize = theme.ResolveFontSize(effectiveCapsuleText, TextRole::LabelLarge, size);
    const bool hasIcon = opts.icon.IsValid();
    const bool hasCapsuleText = !effectiveCapsuleText.empty();

    // Icon-only button with no text variants defaults to square
    if (hasIcon && !hasCapsuleText && effectiveCapsuleText.variants.empty()) {
        const float w = (opts.stretch != StretchMode::Fill && opts.width.has_value()) ? theme.Scale(*opts.width) : m.defaultHeight;
        return {w, m.defaultHeight};
    }

    const float maxTextW = TextUtil::MeasureWidth(effectiveCapsuleText, capFontSize, canvas);

    float contentW = 0.0f;
    float contentH = m.defaultHeight;
    if (hasIcon && maxTextW > 0.0f) {
        if (opts.iconPlacement == Placement::Top || opts.iconPlacement == Placement::Bottom) {
            contentW = std::max(m.iconSize, maxTextW);
            contentH = std::max(m.defaultHeight, m.iconSize + 4.0f + capFontSize + m.paddingY * 2.0f);
        } else {
            const float gap = 8.0f;
            contentW = m.iconSize + gap + maxTextW;
        }
    } else if (maxTextW > 0.0f) {
        contentW = maxTextW;
    }

    float totalW = contentW + m.paddingX * 2.0f;
    if (opts.stretch != StretchMode::Fill && opts.width.has_value()) {
        totalW = theme.Scale(*opts.width);
    } else if (opts.minWidth.has_value()) {
        totalW = std::max({m.defaultHeight, theme.Scale(*opts.minWidth), totalW});
    } else {
        totalW = std::max(m.defaultHeight, totalW);
    }
    return {totalW, contentH};
}

ElementSpec MakeButtonElementSpec(
    const ButtonOptions& opts,
    const UiText& effectiveExternalLabel,
    UiSize size,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const auto& inputMetrics = theme.GetInputField(size);
    return ElementSpec{
        .label = effectiveExternalLabel,
        .description = opts.description,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = opts.width,
        .widgetSize = size,
        .labelGap = inputMetrics.labelGap,
        .descGap = inputMetrics.descGap,
        .inlineGap = m.paddingX,
        .stretchableControl = true,
        .stretchHeightWhenStandalone = true,
        .disabled = disabled
    };
}

} // namespace

Vec2 Button::Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas, const UiTheme& theme, float availableWidth) {
    const bool hasExplicitText = !opts.text.empty() || !opts.text.variants.empty();
    const bool hasExplicitDesc = opts.description.HasContentOrReserve();
    const UiText effectiveCapsuleText = hasExplicitText ? opts.text : (!hasExplicitDesc ? opts.label : UiText{});
    const UiText effectiveExternalLabel = hasExplicitText ? opts.label : (hasExplicitDesc ? opts.label : UiText{});

    const Vec2 capsuleDims = MeasureButtonCapsule(opts, effectiveCapsuleText, size, canvas, theme);
    const ElementSpec spec = MakeButtonElementSpec(opts, effectiveExternalLabel, size, opts.disabled, theme);
    return ElementComposer::Measure(capsuleDims, spec, canvas, theme, availableWidth);
}

Vec2 Button::Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas, float availableWidth) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme, availableWidth);
}

void Button::Render(
    ICanvas& canvas,
    Rect bounds,
    const ButtonOptions& opts,
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

    const ComponentMetrics& m = theme.GetMetrics(size);

    const bool hasExplicitText = !opts.text.empty() || !opts.text.variants.empty();
    const bool hasExplicitDesc = opts.description.HasContentOrReserve();
    const UiText effectiveCapsuleText = hasExplicitText ? opts.text : (!hasExplicitDesc ? opts.label : UiText{});
    const UiText effectiveExternalLabel = hasExplicitText ? opts.label : (hasExplicitDesc ? opts.label : UiText{});
    const float capFontSize = theme.ResolveFontSize(effectiveCapsuleText, TextRole::LabelLarge, size);

    const Vec2 capSize = MeasureButtonCapsule(opts, effectiveCapsuleText, size, &canvas, theme);
    const ElementSpec spec = MakeButtonElementSpec(opts, effectiveExternalLabel, size, disabled, theme);
    const ElementGeometry geom = ElementComposer::Resolve(bounds, capSize, spec, &canvas, theme);
    ElementComposer::RenderText(canvas, geom, spec, theme);
    const Rect capsuleRect = geom.controlRect;

    const VariantColors& colors = theme.GetVariant(opts.variant);

    Color bg = colors.bg;
    Color textCol = effectiveCapsuleText.variant.has_value()
        ? theme.GetTextColor(*effectiveCapsuleText.variant)
        : colors.text;
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

    if (focused && !disabled) {
        borderCol = theme.palette.focusRing;
        borderW = std::max(borderW, 2.0f);
    }

    // Clip all button capsule content strictly to capsuleRect to prevent any bleeding
    canvas.PushClip(capsuleRect);

    // Background & border
    const float radius = opts.radius.has_value() ? theme.Scale(*opts.radius) : m.cornerRadius;
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
        canvas.DrawTextBox(effectiveCapsuleText.text, textRect, capFontSize, textCol, alignX, 0.5f);
    } else if (hasIcon && hasCapsuleText) {
        // Icon + Label
        if (opts.iconPlacement == Placement::Top) {
            const float gap = 4.0f;
            const float textH = canvas.GetLineHeight(capFontSize);
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
            canvas.DrawTextBox(effectiveCapsuleText.text, textRect, capFontSize, textCol, alignX, 0.5f);
        } else if (opts.iconPlacement == Placement::Bottom) {
            const float gap = 4.0f;
            const float textH = canvas.GetLineHeight(capFontSize);
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
            canvas.DrawTextBox(effectiveCapsuleText.text, textRect, capFontSize, textCol, alignX, 0.5f);
            opts.icon.Draw(canvas, Rect(iconX, startY + textH + gap, m.iconSize, m.iconSize), textCol);
        } else if (opts.iconPlacement == Placement::Trailing) {
            const float gap = 8.0f;
            const float availTextW = std::max(0.0f, capsuleRect.w - m.paddingX * 2.0f - m.iconSize - gap);
            float textW = canvas.MeasureText(effectiveCapsuleText.text, capFontSize).x;
            std::string displayLabel(effectiveCapsuleText.text);
            if (textW > availTextW + kTextSubpixelTolerance && availTextW > 0.0f) {
                displayLabel = TextUtil::TruncateWithEllipsis(effectiveCapsuleText.text, availTextW, capFontSize, &canvas);
                textW = canvas.MeasureText(displayLabel, capFontSize).x;
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
            } else if (!effectiveCapsuleText.variants.empty() || capsuleRect.w < totalContentW + m.paddingX * 2.0f) {
                startX = minStartX;
            }
            const float iconY = capsuleRect.y + (capsuleRect.h - m.iconSize) * 0.5f;
            const Rect textRect(startX, capsuleRect.y, textW, capsuleRect.h);
            canvas.DrawTextBox(displayLabel, textRect, capFontSize, textCol, 0.0f, 0.5f);
            opts.icon.Draw(canvas, Rect(startX + textW + gap, iconY, m.iconSize, m.iconSize), textCol);
        } else { // Leading / default
            const float gap = 8.0f;
            const float availTextW = std::max(0.0f, capsuleRect.w - m.paddingX * 2.0f - m.iconSize - gap);
            float textW = canvas.MeasureText(effectiveCapsuleText.text, capFontSize).x;
            std::string displayLabel(effectiveCapsuleText.text);
            if (textW > availTextW + kTextSubpixelTolerance && availTextW > 0.0f) {
                displayLabel = TextUtil::TruncateWithEllipsis(effectiveCapsuleText.text, availTextW, capFontSize, &canvas);
                textW = canvas.MeasureText(displayLabel, capFontSize).x;
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
            } else if (!effectiveCapsuleText.variants.empty() || capsuleRect.w < totalContentW + m.paddingX * 2.0f) {
                startX = minStartX;
            }
            const float iconY = capsuleRect.y + (capsuleRect.h - m.iconSize) * 0.5f;
            opts.icon.Draw(canvas, Rect(startX, iconY, m.iconSize, m.iconSize), textCol);
            const float remainingW = std::max(0.0f, (capsuleRect.x + capsuleRect.w - m.paddingX) - (startX + m.iconSize + gap));
            const Rect textRect(startX + m.iconSize + gap, capsuleRect.y, remainingW, capsuleRect.h);
            canvas.DrawTextBox(displayLabel, textRect, capFontSize, textCol, 0.0f, 0.5f);
        }
    }

    canvas.PopClip();
}

} // namespace guikit
