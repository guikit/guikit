#include "guikit/actions/button.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

ControlSpec EffectiveButtonControl(const ButtonOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const auto& inputMetrics = theme.GetInputField(size);
    ControlSpec c = Control::FromOptions(
        opts,
        SizeMode::Fit,
        SizeMode::Fit,
        ElementLayout::Stacked,
        inputMetrics.labelGap,
        inputMetrics.descGap,
        m.paddingX
    );
    if (opts.square) {
        c.width = AxisSize::Fit();
        c.minWidth = keel::kNullOpt;
        c.maxWidth = keel::kNullOpt;
    }
    return c;
}

void DrawButtonSpinner(ICanvas& canvas, Rect r, Color col, keel::f32 phase) {
    if (r.w <= 1.0f || r.h <= 1.0f || col.a == 0) return;
    const keel::f32 diameter = keel::min(r.w, r.h);
    const keel::f32 strokeW = keel::clamp(diameter * 0.14f, 1.5f, 3.0f);
    const keel::f32 radius = keel::max(1.0f, (diameter - strokeW) * 0.5f);
    const Vec2 center(r.x + r.w * 0.5f, r.y + r.h * 0.5f);

    // Concentric background ring + 250-degree rotating arc (single-quad SDF on GPU canvases)
    canvas.DrawArc(center, radius, 0.0f, keel::kTauF, col.WithAlphaF(0.22f), strokeW, /*roundCaps=*/false);

    const keel::f32 startA = phase * keel::kTauF;
    const keel::f32 sweep = 1.38f * keel::kPiF;
    canvas.DrawArc(center, radius, startA, startA + sweep, col, strokeW, /*roundCaps=*/true);
}

void DrawButtonGlyph(ICanvas& canvas, const ButtonOptions& opts, Rect r, Color col, keel::f32 phase) {
    if (opts.loading) {
        DrawButtonSpinner(canvas, r, col, phase);
    } else if (opts.icon.IsValid()) {
        opts.icon.Draw(canvas, r, col);
    }
}

Vec2 MeasureButtonCapsule(
    const ButtonOptions& opts,
    const ControlSpec& ctrl,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const keel::f32 capFontSize = theme.ResolveFontSize(opts.text, TextRole::LabelLarge, size);
    const keel::f32 badgeFontSize = theme.ResolveFontSize(opts.badge, m.fontSize * 0.78f);
    const bool hasIcon = opts.loading || opts.icon.IsValid();
    const bool hasCapsuleText = !opts.text.empty();
    const bool hasBadge = opts.badge.HasContentOrReserve();
    const keel::f32 baseH = ctrl.height.IsFixed() ? ctrl.height.Dp() : m.defaultHeight;

    // Explicit square button or icon-only button with no text variants or badge defaults to square
    if (opts.square) {
        return {baseH, baseH};
    }
    if (hasIcon && !hasCapsuleText && opts.text.variants.empty() && !hasBadge) {
        const keel::f32 w = ctrl.width.IsFixed() ? ctrl.width.Dp() : baseH;
        return {w, baseH};
    }

    const keel::f32 maxTextW = TextUtil::MeasureWidth(opts.text, capFontSize, canvas);

    keel::f32 contentW = 0.0f;
    keel::f32 contentH = baseH;
    if (hasIcon && maxTextW > 0.0f) {
        if (opts.iconPlacement == Placement::Top || opts.iconPlacement == Placement::Bottom) {
            contentW = keel::max(m.iconSize, maxTextW);
            contentH = keel::max(baseH, m.iconSize + 4.0f + capFontSize + m.paddingY * 2.0f);
        } else {
            const keel::f32 gap = keel::max(4.0f, m.paddingX * 0.55f);
            contentW = m.iconSize + gap + maxTextW;
        }
    } else if (hasIcon) {
        contentW = m.iconSize;
    } else if (maxTextW > 0.0f) {
        contentW = maxTextW;
    }

    if (hasBadge) {
        const keel::f32 badgeTextW = TextUtil::MeasureWidth(opts.badge, badgeFontSize, canvas);
        const keel::f32 badgeW = badgeTextW + 10.0f;
        if (contentW > 0.0f) {
            contentW += 6.0f;
        }
        contentW += badgeW;
    }

    keel::f32 totalW = contentW + m.paddingX * 2.0f;
    if (ctrl.width.IsFixed()) {
        totalW = ctrl.width.Dp();
    } else if (ctrl.minWidth.has_value()) {
        totalW = keel::max(m.defaultHeight, keel::max(*ctrl.minWidth, totalW));
    } else {
        totalW = keel::max(m.defaultHeight, totalW);
    }
    return {totalW, contentH};
}

} // namespace

Response detail::Button::Run(Context& ctx, const ButtonOptions& opts, keel::SourceLocation loc) {

    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool interactionBlocked = effectiveDisabled || opts.loading;
    const UiSize activeSize = opts.size;

    ctx.RegisterFocusable(id, interactionBlocked);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.rightClicked = false;
        res.doubleClicked = false;
    } else if (opts.loading) {
        res.clicked = false;
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

    if (res.hovered) {
        const UiText& effTooltip = !opts.tooltip.empty()
            ? opts.tooltip
            : ((opts.square && !opts.text.empty()) ? opts.text : UiText{});
        if (!effTooltip.empty()) {
            ctx.SetTooltip(effTooltip, res.rect);
        }
    }

    keel::f32 loadingPhase = 0.0f;
    if (opts.loading && !effectiveDisabled) {
        loadingPhase = keel::as<keel::f32>(keel::fmod(keel::max(0.0, ctx.GetCurrentTime()) * 1.35, 1.0));
        ctx.RequestRepaint(2);
    }

    ButtonOptions optsCopy = opts;
    optsCopy.text = ctx.AllocText(opts.text);
    optsCopy.badge = ctx.AllocText(opts.badge);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    if (!opts.icon.GetPath().empty()) {
        optsCopy.icon = Icon::File(ctx.AllocString(opts.icon.GetPath()));
    }

    struct ButtonPayload {
        ButtonOptions opts;
        keel::f32 loadingPhase{0.0f};
    };

    Node* node = ctx.CreateNode(NodeType::Button, id);
    auto* payload = ctx.AllocPayload<ButtonPayload>(ButtonPayload{optsCopy, loadingPhase});
    const ButtonOptions* bOpts = &payload->opts;
    node->text = bOpts->text.text;
    node->SetPayload(payload);

    const ControlSpec ctrl = EffectiveButtonControl(*bOpts, activeSize, ctx.GetTheme());
    node->visualHash = detail::HashArgs(
        bOpts->color,
        bOpts->iconPlacement,
        keel::as<keel::u8>(bOpts->align),
        bOpts->active,
        bOpts->loading,
        loadingPhase,
        bOpts->square,
        bOpts->iconColor.value_or(Color::Transparent()),
        bOpts->width,
        bOpts->height,
        bOpts->minWidth.value_or(0.0f),
        bOpts->maxWidth.value_or(0.0f),
        bOpts->minHeight.value_or(0.0f),
        bOpts->maxHeight.value_or(0.0f),
        activeSize,
        bOpts->background,
        bOpts->border,
        keel::as<keel::u8>(bOpts->icon.GetType()),
        keel::as<keel::u8>(bOpts->icon.GetId()),
        bOpts->icon.GetPath(),
        bOpts->text,
        bOpts->badge
    );
    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* p = n->As<ButtonPayload>();
        return detail::Button::Measure(p ? p->opts : ButtonOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* p = n->As<ButtonPayload>();
        detail::Button::Render(
            canvas,
            n->computedRect,
            p ? p->opts : ButtonOptions{},
            n->uiSize,
            n->hovered,
            n->pressed,
            n->disabled,
            theme,
            n->focused,
            p ? p->loadingPhase : 0.0f
        );
    };
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->focused = ctx.IsFocusVisible(id);

    Control::SetupNode(ctx, node, ctrl);
    if (!ctrl.width.IsFixed()) {
        const keel::f32 defaultMinW = (bOpts->square && bOpts->height.IsFixed())
            ? bOpts->height.Dp()
            : ctx.GetTheme().GetMetrics(activeSize).defaultHeight;
        node->minWidth = keel::max(node->minWidth, defaultMinW);
    }
    if (bOpts->square && (!node->parent || node->parent->type != NodeType::Element) &&
        !bOpts->height.IsFixed() && !bOpts->height.IsStretch()) {
        node->heightMode = SizeMode::Fit;
    }

    return res;
}

Vec2 detail::Button::Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas, const UiTheme& theme, keel::f32 /*availableWidth*/) {
    const ControlSpec ctrl = EffectiveButtonControl(opts, size, theme);
    const Vec2 capsuleDims = MeasureButtonCapsule(opts, ctrl, size, canvas, theme);
    return Control::Measure(capsuleDims, ctrl, theme);
}

Vec2 detail::Button::Measure(const ButtonOptions& opts, UiSize size, ICanvas* canvas, keel::f32 availableWidth) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme, availableWidth);
}

void detail::Button::Render(
    ICanvas& canvas,
    Rect bounds,
    const ButtonOptions& opts,
    UiSize size,
    bool hovered,
    bool pressed,
    bool disabled,
    const UiTheme& theme,
    bool focused,
    keel::f32 loadingPhase
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const ComponentMetrics& m = theme.GetMetrics(size);
    const UiText& effectiveCapsuleText = opts.text;
    const keel::f32 capFontSize = theme.ResolveFontSize(effectiveCapsuleText, TextRole::LabelLarge, size);

    const Rect capsuleRect = bounds;

    const bool hasCtrlBg = opts.background.IsBackgroundEnabled(true);
    const bool overrideBg = opts.background.HasAny() && !opts.background.IsSurface() && !opts.background.IsNone();
    StateColor rawEffColor = overrideBg ? opts.background : opts.color;
    if (!overrideBg && opts.background.IsSurface() && opts.background.HasAlphaScale()) {
        rawEffColor = rawEffColor.WithAlpha(opts.background.alphaScale);
    }
    const StateColor effColor = theme.ResolveStateColor(rawEffColor);

    Color bg = Color::Transparent();
    Color defaultText = theme.palette.textPrimary;
    Color borderCol = Color::Transparent();
    keel::f32 borderW = (opts.border.width > 0.0f) ? opts.border.width : 0.0f;

    if (!effColor.HasAny() || effColor.token == StateColorToken::Secondary) {
        const VariantColors& colors = theme.GetVariant(UiVariant::Secondary);
        defaultText = colors.text;
        borderCol = colors.border;
        if (opts.border.width <= 0.0f) borderW = colors.borderWidth;
        if (hasCtrlBg) {
            if (disabled) {
                bg = effColor.disabled.value_or(colors.bg.WithAlphaF(theme.disabledOpacity));
            } else if (pressed) {
                bg = effColor.active.value_or(colors.bgActive);
            } else if (opts.active) {
                bg = hovered ? effColor.hover.value_or(colors.bgHover) : effColor.active.value_or(colors.bgActive);
            } else if (hovered) {
                bg = effColor.hover.value_or(colors.bgHover);
            } else {
                bg = effColor.normal.value_or(colors.bg);
            }
            if (!effColor.HasAny() && rawEffColor.HasAlphaScale()) {
                bg = bg.ScaleAlpha(rawEffColor.alphaScale);
            }
        }
        if (disabled) {
            borderCol = borderCol.WithAlphaF(theme.disabledOpacity);
        }
    } else if (effColor.token == StateColorToken::Ghost || (effColor.normal.has_value() && effColor.normal->a == 0)) {
        const VariantColors& colors = theme.GetVariant(UiVariant::Ghost);
        defaultText = colors.text;
        borderCol = (opts.border.width > 0.0f) ? theme.palette.border : colors.border;
        if (hasCtrlBg) {
            if (pressed) bg = effColor.active.value_or(colors.bgActive);
            else if (opts.active) bg = hovered ? effColor.hover.value_or(colors.bgHover) : effColor.active.value_or(colors.bgActive);
            else if (hovered) bg = effColor.hover.value_or(colors.bgHover);
            else bg = Color::Transparent();
        }
        if (!disabled && opts.active) {
            borderCol = colors.text.WithAlphaF(0.35f);
            borderW = keel::max(borderW, 1.0f);
        }
        if (disabled) {
            bg = bg.WithAlphaF(theme.disabledOpacity);
            borderCol = borderCol.WithAlphaF(theme.disabledOpacity);
        }
    } else {
        UiVariant matchedVar = UiVariant::Primary;
        bool hasMatchedVar = false;
        switch (effColor.token) {
            case StateColorToken::Primary: matchedVar = UiVariant::Primary; hasMatchedVar = true; break;
            case StateColorToken::Success: matchedVar = UiVariant::Success; hasMatchedVar = true; break;
            case StateColorToken::Warning: matchedVar = UiVariant::Warning; hasMatchedVar = true; break;
            case StateColorToken::Danger:  matchedVar = UiVariant::Danger;  hasMatchedVar = true; break;
            case StateColorToken::Info:    matchedVar = UiVariant::Info;    hasMatchedVar = true; break;
            default: break;
        }
        const VariantColors& fallback = theme.GetVariant(matchedVar);
        if (rawEffColor.IsTinted() && rawEffColor.GetTint() < 0.55f) {
            defaultText = hasMatchedVar ? theme.GetTextColor(matchedVar) : theme.palette.textPrimary;
        } else {
            defaultText = hasMatchedVar ? fallback.text : Color::Hex(0xFFFFFFFF);
        }
        if (hasCtrlBg) {
            if (opts.active && !pressed && hovered && !disabled) {
                bg = effColor.hover.value_or(effColor.normal.value_or(fallback.bgHover));
            } else {
                bg = effColor.Resolve(hovered, pressed || opts.active, disabled, fallback.bg, fallback.bgHover, fallback.bgActive, fallback.bg.WithAlphaF(theme.disabledOpacity));
            }
        }
        if (opts.border.width > 0.0f) {
            borderCol = defaultText.WithAlphaF(0.65f);
            if (disabled) borderCol = borderCol.WithAlphaF(theme.disabledOpacity);
        } else if (hasMatchedVar && fallback.borderWidth > 0.0f) {
            borderCol = disabled ? fallback.border.WithAlphaF(theme.disabledOpacity) : fallback.border;
            borderW = fallback.borderWidth;
        }
    }

    Color textCol = effectiveCapsuleText.color.HasAny()
        ? theme.ResolveTextStateColor(effectiveCapsuleText.color).Resolve(hovered, pressed || opts.active, disabled, defaultText)
        : (disabled ? defaultText.WithAlphaF(theme.disabledOpacity) : defaultText);

    if (opts.border.color.HasAny()) {
        borderCol = theme.ResolveColor(opts.border.color, hovered, pressed || opts.active || focused, disabled, borderCol);
    }

    if (focused && !disabled) {
        if (!opts.border.color.active.has_value()) {
            borderCol = theme.palette.focusRing;
        }
        borderW = keel::max(borderW, theme.focusRingWidth);
    }

    // Clip all button capsule content strictly to capsuleRect to prevent any bleeding
    canvas.PushClip(capsuleRect);

    // Background & border via active style painter
    const bool isGhostBtn = (effColor.token == StateColorToken::Ghost) ||
                            (effColor.normal.has_value() && effColor.normal->a == 0u);
    const keel::f32 radius = opts.border.radius.has_value() ? *opts.border.radius : m.cornerRadius;

    theme.Painter().DrawFrame(
        canvas,
        theme,
        isGhostBtn ? FrameKind::ToolbarButton : FrameKind::Button,
        FramePaintParams{
            .rect = capsuleRect,
            .radius = radius,
            .explicitRadius = opts.border.radius.has_value(),
            .bg = hasCtrlBg ? bg : Color::Transparent(),
            .border = borderCol,
            .borderWidth = borderW,
            .hovered = hovered,
            .pressed = pressed,
            .active = opts.active,
            .focused = focused,
            .disabled = disabled
        }
    );

    const bool hasIcon = opts.loading || opts.icon.IsValid();
    const bool hasCapsuleText = !effectiveCapsuleText.empty();
    const Color icoCol = opts.iconColor.has_value()
        ? (disabled ? opts.iconColor->WithAlphaF(theme.disabledOpacity) : *opts.iconColor)
        : textCol;
    const keel::f32 padX = opts.square ? keel::min(m.paddingX, 2.0f) : m.paddingX;

    // Reserve optional trailing badge inside capsule
    const keel::f32 badgeFontSize = theme.ResolveFontSize(opts.badge, m.fontSize * 0.78f);
    keel::f32 badgeW = 0.0f;
    keel::f32 badgeGap = 0.0f;
    bool drawBadge = false;
    if (!opts.badge.empty() && !opts.square) {
        badgeW = TextUtil::MeasureWidth(opts.badge, badgeFontSize, &canvas) + 10.0f;
        badgeGap = (hasIcon || hasCapsuleText) ? 6.0f : 0.0f;
        if (capsuleRect.w >= padX * 2.0f + badgeW + badgeGap + (hasCapsuleText ? capFontSize * 1.5f : (hasIcon ? m.iconSize : 0.0f))) {
            drawBadge = true;
        }
    }
    const keel::f32 pressShift = (!disabled && (pressed || opts.active)) ? theme.chrome.buttonPressOffset : 0.0f;
    const Rect contentBounds(
        capsuleRect.x + pressShift,
        capsuleRect.y + pressShift,
        keel::max(0.0f, capsuleRect.w - (drawBadge ? (badgeW + badgeGap) : 0.0f)),
        capsuleRect.h
    );
    if (drawBadge) {
        const keel::f32 badgeH = keel::min(capsuleRect.h - 4.0f, badgeFontSize + 5.0f);
        const Rect badgeRect(
            capsuleRect.Right() - padX - badgeW,
            capsuleRect.y + (capsuleRect.h - badgeH) * 0.5f,
            badgeW,
            badgeH
        );
        Color badgeBg = textCol.WithAlphaF(0.18f);
        Color badgeFg = textCol;
        if (opts.badge.color.HasAny()) {
            badgeFg = theme.ResolveTextStateColor(opts.badge.color).Resolve(hovered, pressed || opts.active, disabled, textCol);
            badgeBg = badgeFg.WithAlphaF(0.20f);
        }
        theme.Painter().DrawBadge(canvas, theme, badgeRect, badgeBg, Color::Transparent(), 0.0f, badgeH * 0.5f);
        canvas.DrawTextBox(opts.badge.text, badgeRect, badgeFontSize, badgeFg, 0.5f, 0.5f);
    }

    if (hasIcon && (!hasCapsuleText || opts.square)) {
        // Icon-only or Square ToolButton with icon
        const keel::f32 isz = m.iconSize;
        keel::f32 iconX = contentBounds.x + (contentBounds.w - isz) * 0.5f;
        if (!opts.square) {
            if (opts.align == Align::Start) {
                iconX = contentBounds.x + padX;
            } else if (opts.align == Align::End) {
                iconX = contentBounds.x + contentBounds.w - padX - isz;
            }
        }
        const Rect iconBounds(
            iconX,
            contentBounds.y + (contentBounds.h - isz) * 0.5f,
            isz, isz
        );
        DrawButtonGlyph(canvas, opts, iconBounds, icoCol, loadingPhase);
    } else if (!hasIcon && hasCapsuleText) {
        // Label-only with automatic ellipsis
        const keel::f32 rightPad = drawBadge ? 0.0f : padX;
        const keel::f32 availW = keel::max(0.0f, contentBounds.w - padX - rightPad);
        const Rect textRect(contentBounds.x + padX, contentBounds.y, availW, contentBounds.h);
        keel::f32 alignX = 0.5f;
        if (!opts.square) {
            if (opts.align == Align::Start) {
                alignX = 0.0f;
            } else if (opts.align == Align::End) {
                alignX = 1.0f;
            }
        }
        theme.Painter().DrawControlText(canvas, theme, effectiveCapsuleText.text, textRect, capFontSize, textCol, disabled, alignX, 0.5f);
    } else if (hasIcon && hasCapsuleText) {
        // Icon + Label
        const keel::f32 rightPad = drawBadge ? 0.0f : padX;
        const keel::f32 gap = keel::max(4.0f, m.paddingX * 0.55f);
        if (opts.iconPlacement == Placement::Top) {
            const keel::f32 vGap = 4.0f;
            const keel::f32 textH = canvas.GetLineHeight(capFontSize);
            const keel::f32 totalContentH = m.iconSize + vGap + textH;
            const keel::f32 startY = contentBounds.y + (contentBounds.h - totalContentH) * 0.5f;
            keel::f32 iconX = contentBounds.x + (contentBounds.w - m.iconSize) * 0.5f;
            keel::f32 alignX = 0.5f;
            if (opts.align == Align::Start) {
                iconX = contentBounds.x + padX;
                alignX = 0.0f;
            } else if (opts.align == Align::End) {
                iconX = contentBounds.x + contentBounds.w - rightPad - m.iconSize;
                alignX = 1.0f;
            }
            DrawButtonGlyph(canvas, opts, Rect(iconX, startY, m.iconSize, m.iconSize), icoCol, loadingPhase);
            const Rect textRect(contentBounds.x + padX, startY + m.iconSize + vGap, keel::max(0.0f, contentBounds.w - padX - rightPad), textH);
            theme.Painter().DrawControlText(canvas, theme, effectiveCapsuleText.text, textRect, capFontSize, textCol, disabled, alignX, 0.5f);
        } else if (opts.iconPlacement == Placement::Bottom) {
            const keel::f32 vGap = 4.0f;
            const keel::f32 textH = canvas.GetLineHeight(capFontSize);
            const keel::f32 totalContentH = m.iconSize + vGap + textH;
            const keel::f32 startY = contentBounds.y + (contentBounds.h - totalContentH) * 0.5f;
            keel::f32 iconX = contentBounds.x + (contentBounds.w - m.iconSize) * 0.5f;
            keel::f32 alignX = 0.5f;
            if (opts.align == Align::Start) {
                iconX = contentBounds.x + padX;
                alignX = 0.0f;
            } else if (opts.align == Align::End) {
                iconX = contentBounds.x + contentBounds.w - rightPad - m.iconSize;
                alignX = 1.0f;
            }
            const Rect textRect(contentBounds.x + padX, startY, keel::max(0.0f, contentBounds.w - padX - rightPad), textH);
            theme.Painter().DrawControlText(canvas, theme, effectiveCapsuleText.text, textRect, capFontSize, textCol, disabled, alignX, 0.5f);
            DrawButtonGlyph(canvas, opts, Rect(iconX, startY + textH + vGap, m.iconSize, m.iconSize), icoCol, loadingPhase);
        } else if (opts.iconPlacement == Placement::Trailing) {
            const keel::f32 availTextW = keel::max(0.0f, contentBounds.w - padX - rightPad - m.iconSize - gap);
            const keel::f32 iconY = contentBounds.y + (contentBounds.h - m.iconSize) * 0.5f;
            if (availTextW < capFontSize * 1.25f) {
                DrawButtonGlyph(canvas, opts, Rect(contentBounds.x + (contentBounds.w - m.iconSize) * 0.5f, iconY, m.iconSize, m.iconSize), icoCol, loadingPhase);
            } else {
                keel::f32 textW = canvas.MeasureText(effectiveCapsuleText.text, capFontSize).x;
                keel::FixedString<256> truncatedLabel;
                keel::StringRef displayLabel = effectiveCapsuleText.text;
                if (textW > availTextW + kTextSubpixelTolerance && availTextW > 0.0f) {
                    truncatedLabel = TextUtil::TruncateWithEllipsis(effectiveCapsuleText.text, availTextW, capFontSize, &canvas);
                    displayLabel = truncatedLabel;
                    textW = canvas.MeasureText(displayLabel, capFontSize).x;
                }
                const keel::f32 totalContentW = m.iconSize + gap + textW;
                const keel::f32 centeredStartX = contentBounds.x + (contentBounds.w - totalContentW) * 0.5f;
                const keel::f32 minStartX = contentBounds.x + padX;
                const keel::f32 maxStartX = contentBounds.x + contentBounds.w - rightPad - totalContentW;
                keel::f32 startX = centeredStartX;
                if (opts.align == Align::Start) {
                    startX = minStartX;
                } else if (opts.align == Align::End) {
                    startX = maxStartX;
                } else if (!effectiveCapsuleText.variants.empty() || contentBounds.w < totalContentW + padX + rightPad) {
                    startX = minStartX;
                }
                const Rect textRect(startX, contentBounds.y, textW, contentBounds.h);
                canvas.DrawTextBox(displayLabel, textRect, capFontSize, textCol, 0.0f, 0.5f);
                DrawButtonGlyph(canvas, opts, Rect(startX + textW + gap, iconY, m.iconSize, m.iconSize), icoCol, loadingPhase);
            }
        } else { // Leading / default
            const keel::f32 availTextW = keel::max(0.0f, contentBounds.w - padX - rightPad - m.iconSize - gap);
            const keel::f32 iconY = contentBounds.y + (contentBounds.h - m.iconSize) * 0.5f;
            if (availTextW < capFontSize * 1.25f) {
                DrawButtonGlyph(canvas, opts, Rect(contentBounds.x + (contentBounds.w - m.iconSize) * 0.5f, iconY, m.iconSize, m.iconSize), icoCol, loadingPhase);
            } else {
                keel::f32 textW = canvas.MeasureText(effectiveCapsuleText.text, capFontSize).x;
                keel::FixedString<256> truncatedLabel;
                keel::StringRef displayLabel = effectiveCapsuleText.text;
                if (textW > availTextW + kTextSubpixelTolerance && availTextW > 0.0f) {
                    truncatedLabel = TextUtil::TruncateWithEllipsis(effectiveCapsuleText.text, availTextW, capFontSize, &canvas);
                    displayLabel = truncatedLabel;
                    textW = canvas.MeasureText(displayLabel, capFontSize).x;
                }
                const keel::f32 totalContentW = m.iconSize + gap + textW;
                const keel::f32 centeredStartX = contentBounds.x + (contentBounds.w - totalContentW) * 0.5f;
                const keel::f32 minStartX = contentBounds.x + padX;
                const keel::f32 maxStartX = contentBounds.x + contentBounds.w - rightPad - totalContentW;
                keel::f32 startX = centeredStartX;
                if (opts.align == Align::Start) {
                    startX = minStartX;
                } else if (opts.align == Align::End) {
                    startX = maxStartX;
                } else if (!effectiveCapsuleText.variants.empty() || contentBounds.w < totalContentW + padX + rightPad) {
                    startX = minStartX;
                }
                DrawButtonGlyph(canvas, opts, Rect(startX, iconY, m.iconSize, m.iconSize), icoCol, loadingPhase);
                const keel::f32 remainingW = keel::max(0.0f, (contentBounds.x + contentBounds.w - rightPad) - (startX + m.iconSize + gap));
                const Rect textRect(startX + m.iconSize + gap, contentBounds.y, remainingW, contentBounds.h);
                canvas.DrawTextBox(displayLabel, textRect, capFontSize, textCol, 0.0f, 0.5f);
            }
        }
    }

    canvas.PopClip();
}

} // namespace guikit
