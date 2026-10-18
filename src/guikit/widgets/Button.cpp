#include "guikit/widgets/Button.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

ControlSpec EffectiveButtonControl(const ButtonOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const auto& inputMetrics = theme.GetInputField(size);
    ControlSpec c{};
    c.width = opts.width;
    c.height = opts.height;
    c.minWidth = opts.minWidth;
    c.align = opts.align;
    c.background = opts.background;
    c.border = opts.border;
    c.defaultLayout = ElementLayout::Stacked;
    c.defaultInlineOrder = InlineOrder::LabelFirst;
    c.labelGap = inputMetrics.labelGap;
    c.descGap = inputMetrics.descGap;
    c.inlineGap = m.paddingX;
    c.stretchable = true;
    c.stretchHeightWhenStandalone = true;
    c.hasIntrinsicBox = true;
    return c;
}

Vec2 MeasureButtonCapsule(
    const ButtonOptions& opts,
    const ControlSpec& ctrl,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const float capFontSize = theme.ResolveFontSize(opts.text, TextRole::LabelLarge, size);
    const bool hasIcon = opts.icon.IsValid();
    const bool hasCapsuleText = !opts.text.empty();
    const float baseH = (ctrl.height.has_value() && *ctrl.height > 0.0f) ? theme.Scale(*ctrl.height) : m.defaultHeight;

    // Explicit square button or icon-only button with no text variants defaults to square
    if (opts.square || (hasIcon && !hasCapsuleText && opts.text.variants.empty())) {
        const float w = (opts.stretch != StretchMode::Fill && ctrl.width.has_value()) ? theme.Scale(*ctrl.width) : baseH;
        return {w, baseH};
    }

    const float maxTextW = TextUtil::MeasureWidth(opts.text, capFontSize, canvas);

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

} // namespace

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

    ButtonOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);
    optsCopy.text = ctx.AllocText(opts.text);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    if (!opts.icon.GetPath().empty()) {
        optsCopy.icon = Icon::File(ctx.AllocString(opts.icon.GetPath()));
    }

    Node* node = ctx.CreateNode(NodeType::Button, id);
    auto* bOpts = ctx.AllocPayload<ButtonOptions>(optsCopy);
    node->text = bOpts->text.text;
    node->SetPayload(bOpts);

    const ControlSpec ctrl = EffectiveButtonControl(*bOpts, activeSize, ctx.GetTheme());
    node->visualHash = detail::HashArgs(
        bOpts->color,
        bOpts->iconPlacement,
        static_cast<uint8_t>(bOpts->align),
        bOpts->active,
        bOpts->square,
        bOpts->iconColor.value_or(Color::Transparent()),
        static_cast<uint8_t>(bOpts->stretch),
        bOpts->width.value_or(0.0f),
        bOpts->height.value_or(0.0f),
        bOpts->minWidth.value_or(0.0f),
        bOpts->size.value_or(activeSize),
        bOpts->background,
        bOpts->border,
        static_cast<uint8_t>(bOpts->icon.GetType()),
        static_cast<uint8_t>(bOpts->icon.GetId()),
        bOpts->icon.GetPath(),
        bOpts->text
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
    node->widthMode = (bOpts->stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;
    node->minWidth = ctx.GetTheme().GetMetrics(activeSize).defaultHeight;

    Control::SetupNode(ctx, node, ctrl, bOpts->stretch, Align::Start);
    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Button::Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas, const UiTheme& theme, float /*availableWidth*/) {
    const ControlSpec ctrl = EffectiveButtonControl(opts, size, theme);
    const Vec2 capsuleDims = MeasureButtonCapsule(opts, ctrl, size, canvas, theme);
    return Control::Measure(capsuleDims, ctrl, opts.stretch, size, theme);
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
    const UiText& effectiveCapsuleText = opts.text;
    const float capFontSize = theme.ResolveFontSize(effectiveCapsuleText, TextRole::LabelLarge, size);

    const ControlSpec ctrl = EffectiveButtonControl(opts, size, theme);
    const Rect capsuleRect = bounds;

    const bool hasCtrlBg = ctrl.HasBackground(true);
    const StateColor& effColor = (ctrl.background.HasAny() && !ctrl.background.IsSurface() && !ctrl.background.IsNone())
        ? ctrl.background
        : opts.color;

    Color bg = Color::Transparent();
    Color defaultText = theme.palette.textPrimary;
    Color borderCol = Color::Transparent();
    float borderW = (ctrl.border.width > 0.0f) ? theme.Scale(ctrl.border.width) : 0.0f;

    if (!effColor.HasAny() || effColor == StateColor::Secondary()) {
        const VariantColors& colors = theme.GetVariant(UiVariant::Secondary);
        defaultText = colors.text;
        borderCol = colors.border;
        if (ctrl.border.width <= 0.0f) borderW = colors.borderWidth;
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
        borderCol = (ctrl.border.width > 0.0f) ? theme.palette.border : colors.border;
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
        if (ctrl.border.width > 0.0f) {
            borderCol = defaultText.WithAlphaF(0.65f);
            if (disabled) borderCol = borderCol.WithAlphaF(0.40f);
        }
    }

    Color textCol = effectiveCapsuleText.color.HasAny()
        ? effectiveCapsuleText.color.Resolve(hovered, pressed || opts.active, disabled, defaultText)
        : (disabled ? defaultText.WithAlphaF(0.40f) : defaultText);

    if (ctrl.border.color.HasAny()) {
        borderCol = ctrl.border.color.Resolve(hovered, pressed || opts.active || focused, disabled, borderCol);
    }

    if (focused && !disabled) {
        if (!ctrl.border.color.active.has_value()) {
            borderCol = theme.palette.focusRing;
        }
        borderW = std::max(borderW, 2.0f);
    }

    // Clip all button capsule content strictly to capsuleRect to prevent any bleeding
    canvas.PushClip(capsuleRect);

    // Background & border
    const float radius = ctrl.border.radius.has_value() ? theme.Scale(*ctrl.border.radius) : m.cornerRadius;
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
