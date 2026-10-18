#include "guikit/widgets/Button.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Element.hpp"
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
    bOpts->badge = ctx.AllocText(opts.badge);
    bOpts->tooltip = ctx.AllocText(opts.tooltip);
    node->text = bOpts->label.text;
    node->SetPayload(bOpts);
    node->visualHash = detail::HashArgs(
        opts.color,
        opts.iconPlacement,
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        opts.controlAlign,
        opts.active,
        opts.square,
        opts.iconColor.value_or(Color::Transparent()),
        static_cast<uint8_t>(opts.stretch),
        opts.width.value_or(0.0f),
        opts.height.value_or(0.0f),
        opts.minWidth.value_or(0.0f),
        opts.size.value_or(activeSize),
        opts.background,
        opts.borderWidth,
        opts.borderRadius.value_or(-1.0f),
        opts.borderColor,
        opts.bgColor,
        opts.control.width.value_or(0.0f),
        opts.control.height.value_or(0.0f),
        opts.control.minWidth.value_or(0.0f),
        static_cast<uint8_t>(opts.control.align),
        opts.control.HasBackground(true),
        opts.control.borderWidth,
        opts.control.borderRadius.value_or(-1.0f),
        opts.control.borderColor,
        opts.control.bgColor,
        static_cast<uint8_t>(opts.icon.GetType()),
        static_cast<uint8_t>(opts.icon.GetId()),
        opts.icon.GetPath(),
        static_cast<uint8_t>(opts.labelIcon.GetType()),
        static_cast<uint8_t>(opts.labelIcon.GetId()),
        opts.labelIcon.GetPath(),
        bOpts->label,
        bOpts->text,
        bOpts->description,
        bOpts->badge
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

ControlSpec EffectiveButtonControl(const ButtonOptions& opts) noexcept {
    ControlSpec c = opts.control;
    if (!c.width.has_value() && opts.width.has_value()) c.width = opts.width;
    if (!c.height.has_value() && opts.height.has_value()) c.height = opts.height;
    if (!c.minWidth.has_value() && opts.minWidth.has_value()) c.minWidth = opts.minWidth;
    if (c.align == Align::Center && opts.controlAlign != Align::Center) c.align = opts.controlAlign;
    c.stretchable = true;
    c.stretchHeightWhenStandalone = (!opts.background && opts.borderWidth <= 0.0f);
    c.hasIntrinsicBox = true;
    return c;
}

Vec2 MeasureButtonCapsule(
    const ButtonOptions& opts,
    const ControlSpec& ctrl,
    const UiText& effectiveCapsuleText,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const float capFontSize = theme.ResolveFontSize(effectiveCapsuleText, TextRole::LabelLarge, size);
    const bool hasIcon = opts.icon.IsValid();
    const bool hasCapsuleText = !effectiveCapsuleText.empty();
    const float baseH = (ctrl.height.has_value() && *ctrl.height > 0.0f) ? theme.Scale(*ctrl.height) : m.defaultHeight;

    // Explicit square button or icon-only button with no text variants defaults to square
    if (opts.square || (hasIcon && !hasCapsuleText && effectiveCapsuleText.variants.empty())) {
        const float w = (opts.stretch != StretchMode::Fill && ctrl.width.has_value()) ? theme.Scale(*ctrl.width) : baseH;
        return {w, baseH};
    }

    const float maxTextW = TextUtil::MeasureWidth(effectiveCapsuleText, capFontSize, canvas);

    float contentW = 0.0f;
    float contentH = baseH;
    if (hasIcon && maxTextW > 0.0f) {
        if (opts.iconPlacement == Placement::Top || opts.iconPlacement == Placement::Bottom) {
            contentW = std::max(m.iconSize, maxTextW);
            contentH = std::max(baseH, m.iconSize + theme.Scale(4.0f) + capFontSize + m.paddingY * 2.0f);
        } else {
            const float gap = std::max(theme.Scale(4.0f), m.paddingX * 0.55f);
            contentW = m.iconSize + gap + maxTextW;
        }
    } else if (maxTextW > 0.0f) {
        contentW = maxTextW;
    }

    float totalW = contentW + m.paddingX * 2.0f;
    if (opts.stretch != StretchMode::Fill && ctrl.width.has_value()) {
        totalW = theme.Scale(*ctrl.width);
    } else if (ctrl.minWidth.has_value()) {
        totalW = std::max({m.defaultHeight, theme.Scale(*ctrl.minWidth), totalW});
    } else {
        totalW = std::max(m.defaultHeight, totalW);
    }
    return {totalW, contentH};
}

ElementSpec MakeButtonElementSpec(
    const ButtonOptions& opts,
    const ControlSpec& ctrl,
    const UiText& effectiveExternalLabel,
    UiSize size,
    bool disabled,
    const UiTheme& theme,
    bool hovered = false,
    bool active = false
) noexcept {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const auto& inputMetrics = theme.GetInputField(size);
    return ElementSpec{
        .label = effectiveExternalLabel,
        .description = opts.description,
        .icon = opts.labelIcon,
        .badge = opts.badge,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = ctrl.width,
        .height = ctrl.height,
        .control = ctrl,
        .background = opts.background,
        .borderWidth = opts.borderWidth,
        .borderRadius = opts.borderRadius,
        .borderColor = opts.borderColor,
        .bgColor = opts.bgColor,
        .widgetSize = size,
        .labelGap = inputMetrics.labelGap,
        .descGap = inputMetrics.descGap,
        .inlineGap = m.paddingX,
        .stretchableControl = true,
        .stretchHeightWhenStandalone = ctrl.stretchHeightWhenStandalone,
        .disabled = disabled,
        .hovered = hovered,
        .active = active
    };
}

} // namespace

Vec2 Button::Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas, const UiTheme& theme, float availableWidth) {
    const bool hasExplicitText = !opts.text.empty() || !opts.text.variants.empty();
    const bool hasExplicitDesc = opts.description.HasContentOrReserve() || opts.labelIcon.IsValid() || opts.badge.HasContentOrReserve();
    const UiText effectiveCapsuleText = hasExplicitText ? opts.text : (!hasExplicitDesc ? opts.label : UiText{});
    const UiText effectiveExternalLabel = hasExplicitText ? opts.label : (hasExplicitDesc ? opts.label : UiText{});

    const ControlSpec ctrl = EffectiveButtonControl(opts);
    const Vec2 capsuleDims = MeasureButtonCapsule(opts, ctrl, effectiveCapsuleText, size, canvas, theme);
    const ElementSpec spec = MakeButtonElementSpec(opts, ctrl, effectiveExternalLabel, size, opts.disabled, theme);
    return Element::Measure(capsuleDims, spec, canvas, theme, availableWidth);
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
    const bool hasExplicitDesc = opts.description.HasContentOrReserve() || opts.labelIcon.IsValid() || opts.badge.HasContentOrReserve();
    const UiText effectiveCapsuleText = hasExplicitText ? opts.text : (!hasExplicitDesc ? opts.label : UiText{});
    const UiText effectiveExternalLabel = hasExplicitText ? opts.label : (hasExplicitDesc ? opts.label : UiText{});
    const float capFontSize = theme.ResolveFontSize(effectiveCapsuleText, TextRole::LabelLarge, size);

    const ControlSpec ctrl = EffectiveButtonControl(opts);
    const Vec2 capSize = MeasureButtonCapsule(opts, ctrl, effectiveCapsuleText, size, &canvas, theme);
    const ElementSpec spec = MakeButtonElementSpec(
        opts, ctrl, effectiveExternalLabel, size, disabled, theme, hovered, pressed || opts.active || focused
    );
    const ElementGeometry geom = Element::Resolve(bounds, capSize, spec, &canvas, theme);
    Element::RenderText(canvas, geom, spec, theme);
    const Rect capsuleRect = geom.controlRect;

    const bool hasCtrlBg = ctrl.HasBackground(true);
    const StateColor& effColor = ctrl.bgColor.HasAny() ? ctrl.bgColor : opts.color;

    Color bg = Color::Transparent();
    Color defaultText = theme.palette.textPrimary;
    Color borderCol = Color::Transparent();
    float borderW = (ctrl.borderWidth > 0.0f) ? theme.Scale(ctrl.borderWidth) : 0.0f;

    if (!effColor.HasAny() || effColor == StateColor::Secondary()) {
        const VariantColors& colors = theme.GetVariant(UiVariant::Secondary);
        defaultText = colors.text;
        borderCol = colors.border;
        if (ctrl.borderWidth <= 0.0f) borderW = colors.borderWidth;
        if (hasCtrlBg) {
            if (pressed) bg = colors.bgActive;
            else if (opts.active) bg = hovered ? colors.bgHover : colors.bgActive;
            else if (hovered) bg = colors.bgHover;
            else bg = colors.bg;
        }
        if (disabled) {
            bg = bg.WithAlphaF(0.40f);
            borderCol = borderCol.WithAlphaF(0.40f);
        }
    } else if (effColor.normal.has_value() && effColor.normal->a == 0) {
        const VariantColors& colors = theme.GetVariant(UiVariant::Ghost);
        defaultText = colors.text;
        borderCol = (ctrl.borderWidth > 0.0f) ? theme.palette.border : colors.border;
        if (hasCtrlBg) {
            if (pressed) bg = effColor.active.value_or(colors.bgActive);
            else if (opts.active) bg = hovered ? effColor.hover.value_or(colors.bgHover) : effColor.active.value_or(colors.bgActive);
            else if (hovered) bg = effColor.hover.value_or(colors.bgHover);
            else bg = Color::Transparent();
        }
        if (!disabled && opts.active) {
            borderCol = colors.text.WithAlphaF(0.35f);
            borderW = std::max(borderW, 1.0f);
        }
        if (disabled) {
            bg = bg.WithAlphaF(0.40f);
            borderCol = borderCol.WithAlphaF(0.40f);
        }
    } else {
        const VariantColors& fallback = theme.GetVariant(UiVariant::Primary);
        defaultText = (effColor == StateColor::Warning() && theme.mode == ThemeMode::Dark)
            ? Color::Hex(0x0F172AFF)
            : Color::Hex(0xFFFFFFFF);
        if (hasCtrlBg) {
            if (opts.active && !pressed && hovered && !disabled) {
                bg = effColor.hover.value_or(effColor.normal.value_or(fallback.bgHover));
            } else {
                bg = effColor.Resolve(hovered, pressed || opts.active, disabled, fallback.bg, fallback.bgHover, fallback.bgActive, fallback.bg.WithAlphaF(0.40f));
            }
        }
        if (ctrl.borderWidth > 0.0f) {
            borderCol = defaultText.WithAlphaF(0.65f);
            if (disabled) borderCol = borderCol.WithAlphaF(0.40f);
        }
    }

    Color textCol = effectiveCapsuleText.color.HasAny()
        ? effectiveCapsuleText.color.Resolve(hovered, pressed || opts.active, disabled, defaultText)
        : (disabled ? defaultText.WithAlphaF(0.40f) : defaultText);

    if (ctrl.borderColor.HasAny()) {
        borderCol = ctrl.borderColor.Resolve(hovered, pressed || opts.active || focused, disabled, borderCol);
    }

    if (focused && !disabled) {
        if (!ctrl.borderColor.active.has_value()) {
            borderCol = theme.palette.focusRing;
        }
        borderW = std::max(borderW, 2.0f);
    }

    // Clip all button capsule content strictly to capsuleRect to prevent any bleeding
    canvas.PushClip(capsuleRect);

    // Background & border
    const float radius = ctrl.borderRadius.has_value() ? theme.Scale(*ctrl.borderRadius) : m.cornerRadius;
    canvas.DrawRoundRect(capsuleRect, radius, bg, borderCol, borderW);

    const bool hasIcon = opts.icon.IsValid();
    const bool hasCapsuleText = !effectiveCapsuleText.empty();
    const Color icoCol = opts.iconColor.has_value()
        ? (disabled ? opts.iconColor->WithAlphaF(0.40f) : *opts.iconColor)
        : textCol;
    const float padX = opts.square ? std::min(m.paddingX, theme.Scale(2.0f)) : m.paddingX;

    if (hasIcon && !hasCapsuleText) {
        // Icon-only
        const float isz = m.iconSize;
        float iconX = capsuleRect.x + (capsuleRect.w - isz) * 0.5f;
        if (ctrl.align == Align::Start) {
            iconX = capsuleRect.x + padX;
        } else if (ctrl.align == Align::End) {
            iconX = capsuleRect.x + capsuleRect.w - padX - isz;
        }
        const Rect iconBounds(
            iconX,
            capsuleRect.y + (capsuleRect.h - isz) * 0.5f,
            isz, isz
        );
        opts.icon.Draw(canvas, iconBounds, icoCol);
    } else if (!hasIcon && hasCapsuleText) {
        // Label-only with automatic ellipsis
        const float availW = std::max(0.0f, capsuleRect.w - padX * 2.0f);
        const Rect textRect(capsuleRect.x + padX, capsuleRect.y, availW, capsuleRect.h);
        float alignX = 0.5f;
        if (ctrl.align == Align::Start) {
            alignX = 0.0f;
        } else if (ctrl.align == Align::End) {
            alignX = 1.0f;
        }
        canvas.DrawTextBox(effectiveCapsuleText.text, textRect, capFontSize, textCol, alignX, 0.5f);
    } else if (hasIcon && hasCapsuleText) {
        // Icon + Label
        const float gap = std::max(theme.Scale(4.0f), m.paddingX * 0.55f);
        if (opts.iconPlacement == Placement::Top) {
            const float vGap = theme.Scale(4.0f);
            const float textH = canvas.GetLineHeight(capFontSize);
            const float totalContentH = m.iconSize + vGap + textH;
            const float startY = capsuleRect.y + (capsuleRect.h - totalContentH) * 0.5f;
            float iconX = capsuleRect.x + (capsuleRect.w - m.iconSize) * 0.5f;
            float alignX = 0.5f;
            if (ctrl.align == Align::Start) {
                iconX = capsuleRect.x + padX;
                alignX = 0.0f;
            } else if (ctrl.align == Align::End) {
                iconX = capsuleRect.x + capsuleRect.w - padX - m.iconSize;
                alignX = 1.0f;
            }
            opts.icon.Draw(canvas, Rect(iconX, startY, m.iconSize, m.iconSize), icoCol);
            const Rect textRect(capsuleRect.x + padX, startY + m.iconSize + vGap, std::max(0.0f, capsuleRect.w - padX * 2.0f), textH);
            canvas.DrawTextBox(effectiveCapsuleText.text, textRect, capFontSize, textCol, alignX, 0.5f);
        } else if (opts.iconPlacement == Placement::Bottom) {
            const float vGap = theme.Scale(4.0f);
            const float textH = canvas.GetLineHeight(capFontSize);
            const float totalContentH = m.iconSize + vGap + textH;
            const float startY = capsuleRect.y + (capsuleRect.h - totalContentH) * 0.5f;
            float iconX = capsuleRect.x + (capsuleRect.w - m.iconSize) * 0.5f;
            float alignX = 0.5f;
            if (ctrl.align == Align::Start) {
                iconX = capsuleRect.x + padX;
                alignX = 0.0f;
            } else if (ctrl.align == Align::End) {
                iconX = capsuleRect.x + capsuleRect.w - padX - m.iconSize;
                alignX = 1.0f;
            }
            const Rect textRect(capsuleRect.x + padX, startY, std::max(0.0f, capsuleRect.w - padX * 2.0f), textH);
            canvas.DrawTextBox(effectiveCapsuleText.text, textRect, capFontSize, textCol, alignX, 0.5f);
            opts.icon.Draw(canvas, Rect(iconX, startY + textH + vGap, m.iconSize, m.iconSize), icoCol);
        } else if (opts.iconPlacement == Placement::Trailing) {
            const float availTextW = std::max(0.0f, capsuleRect.w - padX * 2.0f - m.iconSize - gap);
            float textW = canvas.MeasureText(effectiveCapsuleText.text, capFontSize).x;
            std::string displayLabel(effectiveCapsuleText.text);
            if (textW > availTextW + kTextSubpixelTolerance && availTextW > 0.0f) {
                displayLabel = TextUtil::TruncateWithEllipsis(effectiveCapsuleText.text, availTextW, capFontSize, &canvas);
                textW = canvas.MeasureText(displayLabel, capFontSize).x;
            }
            const float totalContentW = m.iconSize + gap + textW;
            const float centeredStartX = capsuleRect.x + (capsuleRect.w - totalContentW) * 0.5f;
            const float minStartX = capsuleRect.x + padX;
            const float maxStartX = capsuleRect.x + capsuleRect.w - padX - totalContentW;
            float startX = centeredStartX;
            if (ctrl.align == Align::Start) {
                startX = minStartX;
            } else if (ctrl.align == Align::End) {
                startX = maxStartX;
            } else if (!effectiveCapsuleText.variants.empty() || capsuleRect.w < totalContentW + padX * 2.0f) {
                startX = minStartX;
            }
            const float iconY = capsuleRect.y + (capsuleRect.h - m.iconSize) * 0.5f;
            const Rect textRect(startX, capsuleRect.y, textW, capsuleRect.h);
            canvas.DrawTextBox(displayLabel, textRect, capFontSize, textCol, 0.0f, 0.5f);
            opts.icon.Draw(canvas, Rect(startX + textW + gap, iconY, m.iconSize, m.iconSize), icoCol);
        } else { // Leading / default
            const float availTextW = std::max(0.0f, capsuleRect.w - padX * 2.0f - m.iconSize - gap);
            float textW = canvas.MeasureText(effectiveCapsuleText.text, capFontSize).x;
            std::string displayLabel(effectiveCapsuleText.text);
            if (textW > availTextW + kTextSubpixelTolerance && availTextW > 0.0f) {
                displayLabel = TextUtil::TruncateWithEllipsis(effectiveCapsuleText.text, availTextW, capFontSize, &canvas);
                textW = canvas.MeasureText(displayLabel, capFontSize).x;
            }
            const float totalContentW = m.iconSize + gap + textW;
            const float centeredStartX = capsuleRect.x + (capsuleRect.w - totalContentW) * 0.5f;
            const float minStartX = capsuleRect.x + padX;
            const float maxStartX = capsuleRect.x + capsuleRect.w - padX - totalContentW;
            float startX = centeredStartX;
            if (ctrl.align == Align::Start) {
                startX = minStartX;
            } else if (ctrl.align == Align::End) {
                startX = maxStartX;
            } else if (!effectiveCapsuleText.variants.empty() || capsuleRect.w < totalContentW + padX * 2.0f) {
                startX = minStartX;
            }
            const float iconY = capsuleRect.y + (capsuleRect.h - m.iconSize) * 0.5f;
            opts.icon.Draw(canvas, Rect(startX, iconY, m.iconSize, m.iconSize), icoCol);
            const float remainingW = std::max(0.0f, (capsuleRect.x + capsuleRect.w - padX) - (startX + m.iconSize + gap));
            const Rect textRect(startX + m.iconSize + gap, capsuleRect.y, remainingW, capsuleRect.h);
            canvas.DrawTextBox(displayLabel, textRect, capFontSize, textCol, 0.0f, 0.5f);
        }
    }

    canvas.PopClip();
}

} // namespace guikit
