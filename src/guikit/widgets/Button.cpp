#include "guikit/widgets/Button.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

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
    node->text = ctx.InternString(opts.label);
    auto* bOpts = ctx.AllocPayload<ButtonOptions>(opts);
    bOpts->label = node->text;
    node->payload = bOpts;
    node->measureFn = [](const Node* n, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
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
    node->widthMode = opts.stretch ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Button::Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas, const UiTheme& theme) {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const bool hasIcon = opts.icon.IsValid();
    const bool hasLabel = !opts.label.empty();

    // Icon-only button with no label variants defaults to square
    if (hasIcon && !hasLabel && opts.labelVariants.empty()) {
        return {m.defaultHeight, m.defaultHeight};
    }

    float maxTextW = 0.0f;
    if (hasLabel) {
        if (canvas) {
            maxTextW = canvas->MeasureText(opts.label, m.fontSize).x;
        } else {
            maxTextW = static_cast<float>(opts.label.length()) * (m.fontSize * kDefaultCharAdvanceRatio);
        }
    }

    // Evaluate all candidate label variants to ensure stable sizing across state changes
    for (std::string_view var : opts.labelVariants) {
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

    const float totalW = contentW + m.paddingX * 2.0f;
    return {std::max(m.defaultHeight, totalW), contentH};
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
    const ComponentMetrics& m = theme.GetMetrics(size);
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

    // Background & border
    canvas.DrawRoundRect(bounds, m.cornerRadius, bg, borderCol, borderW);

    const bool hasIcon = opts.icon.IsValid();
    const bool hasLabel = !opts.label.empty();

    if (hasIcon && !hasLabel) {
        // Centered icon
        const float isz = m.iconSize;
        const Rect iconBounds(
            bounds.x + (bounds.w - isz) * 0.5f,
            bounds.y + (bounds.h - isz) * 0.5f,
            isz, isz
        );
        opts.icon.Draw(canvas, iconBounds, textCol);
    } else if (!hasIcon && hasLabel) {
        // Centered label
        canvas.DrawTextBox(opts.label, bounds, m.fontSize, textCol, 0.5f, 0.5f);
    } else if (hasIcon && hasLabel) {
        // Icon + Label
        if (opts.iconPlacement == Placement::Top) {
            const float gap = 4.0f;
            const float textH = canvas.GetLineHeight(m.fontSize);
            const float totalContentH = m.iconSize + gap + textH;
            const float startY = bounds.y + (bounds.h - totalContentH) * 0.5f;
            const float iconX = bounds.x + (bounds.w - m.iconSize) * 0.5f;
            opts.icon.Draw(canvas, Rect(iconX, startY, m.iconSize, m.iconSize), textCol);
            const Rect textRect(bounds.x, startY + m.iconSize + gap, bounds.w, textH);
            canvas.DrawTextBox(opts.label, textRect, m.fontSize, textCol, 0.5f, 0.5f);
        } else if (opts.iconPlacement == Placement::Bottom) {
            const float gap = 4.0f;
            const float textH = canvas.GetLineHeight(m.fontSize);
            const float totalContentH = m.iconSize + gap + textH;
            const float startY = bounds.y + (bounds.h - totalContentH) * 0.5f;
            const Rect textRect(bounds.x, startY, bounds.w, textH);
            canvas.DrawTextBox(opts.label, textRect, m.fontSize, textCol, 0.5f, 0.5f);
            const float iconX = bounds.x + (bounds.w - m.iconSize) * 0.5f;
            opts.icon.Draw(canvas, Rect(iconX, startY + textH + gap, m.iconSize, m.iconSize), textCol);
        } else if (opts.iconPlacement == Placement::Trailing) {
            const float gap = 8.0f;
            float textW = canvas.MeasureText(opts.label, m.fontSize).x;
            const float totalContentW = m.iconSize + gap + textW;
            const float centeredStartX = bounds.x + (bounds.w - totalContentW) * 0.5f;
            const float minStartX = bounds.x + m.paddingX;
            const float startX = (!opts.labelVariants.empty() || bounds.w < totalContentW + m.paddingX * 2.0f)
                ? minStartX
                : centeredStartX;
            const float iconY = bounds.y + (bounds.h - m.iconSize) * 0.5f;
            const Rect textRect(startX, bounds.y, textW, bounds.h);
            canvas.DrawTextBox(opts.label, textRect, m.fontSize, textCol, 0.0f, 0.5f);
            opts.icon.Draw(canvas, Rect(startX + textW + gap, iconY, m.iconSize, m.iconSize), textCol);
        } else { // Leading / default
            const float gap = 8.0f;
            float textW = canvas.MeasureText(opts.label, m.fontSize).x;
            const float totalContentW = m.iconSize + gap + textW;
            const float centeredStartX = bounds.x + (bounds.w - totalContentW) * 0.5f;
            const float minStartX = bounds.x + m.paddingX;
            const float startX = (!opts.labelVariants.empty() || bounds.w < totalContentW + m.paddingX * 2.0f)
                ? minStartX
                : centeredStartX;
            const float iconY = bounds.y + (bounds.h - m.iconSize) * 0.5f;
            opts.icon.Draw(canvas, Rect(startX, iconY, m.iconSize, m.iconSize), textCol);
            const Rect textRect(startX + m.iconSize + gap, bounds.y, textW, bounds.h);
            canvas.DrawTextBox(opts.label, textRect, m.fontSize, textCol, 0.0f, 0.5f);
        }
    }
}

} // namespace guikit
