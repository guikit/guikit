#include "guikit/widgets/button.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"

namespace guikit {

namespace {

ControlSpec EffectiveButtonControl(const ButtonOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const auto& inputMetrics = theme.GetInputField(size);
    ControlSpec c{};
    c.width = opts.width;
    c.widthRatio = opts.widthRatio;
    c.height = opts.height;
    c.minWidth = opts.square ? std::nullopt : opts.minWidth;
    c.align = opts.align;
    c.background = opts.background;
    c.border = opts.border;
    c.defaultLayout = ElementLayout::Stacked;
    c.defaultInlineOrder = InlineOrder::LabelFirst;
    c.labelGap = inputMetrics.labelGap;
    c.descGap = inputMetrics.descGap;
    c.inlineGap = m.paddingX;
    c.stretchable = !opts.square;
    c.stretchHeightWhenStandalone = !opts.square;
    c.hasIntrinsicBox = true;
    return c;
}

void DrawButtonSpinner(ICanvas& canvas, Rect r, Color col, float phase) {
    if (r.w <= 1.0f || r.h <= 1.0f || col.a == 0) return;
    const float diameter = std::min(r.w, r.h);
    const float strokeW = std::clamp(diameter * 0.14f, 1.5f, 3.0f);
    const float radius = std::max(1.0f, (diameter - strokeW) * 0.5f);
    const Vec2 center(r.x + r.w * 0.5f, r.y + r.h * 0.5f);

    // Subtle background ring
    canvas.DrawRoundRect(
        Rect(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f),
        radius,
        Color::Transparent(),
        col.WithAlphaF(0.22f),
        strokeW
    );

    // 250-degree rotating arc
    constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
    const float startA = phase * kTwoPi;
    const float sweep = 1.38f * std::numbers::pi_v<float>;
    constexpr int kSegs = 18;
    VectorPath path;
    path.Reserve(kSegs + 1, kSegs + 1);
    for (int i = 0; i <= kSegs; ++i) {
        const float a = startA + sweep * (static_cast<float>(i) / static_cast<float>(kSegs));
        const Vec2 pt(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius);
        if (i == 0) path.MoveTo(pt);
        else path.LineTo(pt);
    }
    PathPaint paint{};
    paint.strokeColor = col;
    paint.strokeWidthPx = strokeW;
    paint.renderStroke = true;
    paint.renderFill = false;
    canvas.DrawPath(path, paint);
}

void DrawButtonGlyph(ICanvas& canvas, const ButtonOptions& opts, Rect r, Color col, float phase) {
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
    const float capFontSize = theme.ResolveFontSize(opts.text, TextRole::LabelLarge, size);
    const float badgeFontSize = theme.ResolveFontSize(opts.badge, m.fontSize * 0.78f);
    const bool hasIcon = opts.loading || opts.icon.IsValid();
    const bool hasCapsuleText = !opts.text.empty();
    const bool hasBadge = opts.badge.HasContentOrReserve();
    const float baseH = (ctrl.height.has_value() && *ctrl.height > 0.0f) ? theme.Scale(*ctrl.height) : m.defaultHeight;

    // Explicit square button or icon-only button with no text variants or badge defaults to square
    if (opts.square) {
        return {baseH, baseH};
    }
    if (hasIcon && !hasCapsuleText && opts.text.variants.empty() && !hasBadge) {
        const float w = Control::HasExplicitDpWidth(opts.stretch, ctrl.width, ctrl.widthRatio) ? theme.Scale(*ctrl.width) : baseH;
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
    } else if (hasIcon) {
        contentW = m.iconSize;
    } else if (maxTextW > 0.0f) {
        contentW = maxTextW;
    }

    if (hasBadge) {
        const float badgeTextW = TextUtil::MeasureWidth(opts.badge, badgeFontSize, canvas);
        const float badgeW = badgeTextW + theme.Scale(10.0f);
        if (contentW > 0.0f) {
            contentW += theme.Scale(6.0f);
        }
        contentW += badgeW;
    }

    float totalW = contentW + m.paddingX * 2.0f;
    if (Control::HasExplicitDpWidth(opts.stretch, ctrl.width, ctrl.widthRatio)) {
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
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    struct ThemePopGuard {
        Context& c;
        UiTheme* t;
        ~ThemePopGuard() noexcept { if (t) c.PopTheme(); }
    } themePop{ctx, pushedTheme};

    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool interactionBlocked = effectiveDisabled || opts.loading;
    const UiSize activeSize = ctx.GetCurrentSize();

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

    float loadingPhase = 0.0f;
    if (opts.loading && !effectiveDisabled) {
        loadingPhase = static_cast<float>(std::fmod(std::max(0.0, ctx.GetCurrentTime()) * 1.35, 1.0));
        ctx.RequestRepaint(2);
    }

    ButtonOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.widthRatio, optsCopy.height);
    optsCopy.text = ctx.AllocText(opts.text);
    optsCopy.badge = ctx.AllocText(opts.badge);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    if (!opts.icon.GetPath().empty()) {
        optsCopy.icon = Icon::File(ctx.AllocString(opts.icon.GetPath()));
    }

    Node* node = ctx.CreateNode(NodeType::Button, id);
    node->theme = pushedTheme;
    node->scrollOffset.x = loadingPhase;
    auto* bOpts = ctx.AllocPayload<ButtonOptions>(optsCopy);
    node->text = bOpts->text.text;
    node->SetPayload(bOpts);

    const ControlSpec ctrl = EffectiveButtonControl(*bOpts, activeSize, ctx.GetTheme());
    node->visualHash = detail::HashArgs(
        bOpts->color,
        bOpts->iconPlacement,
        static_cast<uint8_t>(bOpts->align),
        bOpts->active,
        bOpts->loading,
        loadingPhase,
        bOpts->square,
        bOpts->iconColor.value_or(Color::Transparent()),
        static_cast<uint8_t>(bOpts->stretch),
        bOpts->width.value_or(0.0f),
        bOpts->widthRatio.value_or(-1.0f),
        bOpts->height.value_or(0.0f),
        bOpts->minWidth.value_or(0.0f),
        activeSize,
        bOpts->background,
        bOpts->border,
        static_cast<uint8_t>(bOpts->icon.GetType()),
        static_cast<uint8_t>(bOpts->icon.GetId()),
        bOpts->icon.GetPath(),
        bOpts->text,
        bOpts->badge
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* b = n->As<ButtonOptions>();
        return Button::Measure(b ? *b : ButtonOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* b = n->As<ButtonOptions>();
        Button::Render(
            canvas,
            n->computedRect,
            b ? *b : ButtonOptions{},
            n->uiSize,
            n->hovered,
            n->pressed,
            n->disabled,
            theme,
            n->focused,
            n->scrollOffset.x
        );
    };
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->focused = ctx.IsFocused(id);
    node->widthMode = (bOpts->stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;
    node->minWidth = (bOpts->square && bOpts->height.has_value() && *bOpts->height > 0.0f)
        ? ctx.GetTheme().Scale(*bOpts->height)
        : ctx.GetTheme().GetMetrics(activeSize).defaultHeight;

    Control::SetupNode(ctx, node, ctrl, bOpts->stretch, bOpts->square ? bOpts->align : Align::Start);
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
    bool focused,
    float loadingPhase
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
    const StateColor& rawEffColor = (ctrl.background.HasAny() && !ctrl.background.IsSurface() && !ctrl.background.IsNone())
        ? ctrl.background
        : opts.color;
    const StateColor effColor = theme.ResolveStateColor(rawEffColor);

    Color bg = Color::Transparent();
    Color defaultText = theme.palette.textPrimary;
    Color borderCol = Color::Transparent();
    float borderW = (ctrl.border.width > 0.0f) ? theme.Scale(ctrl.border.width) : 0.0f;

    if (!effColor.HasAny() || effColor.token == StateColorToken::Secondary) {
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
    } else if (effColor.token == StateColorToken::Ghost || (effColor.normal.has_value() && effColor.normal->a == 0)) {
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
        defaultText = hasMatchedVar ? fallback.text : Color::Hex(0xFFFFFFFF);
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
        ? theme.ResolveTextStateColor(effectiveCapsuleText.color).Resolve(hovered, pressed || opts.active, disabled, defaultText)
        : (disabled ? defaultText.WithAlphaF(0.40f) : defaultText);

    if (ctrl.border.color.HasAny()) {
        borderCol = theme.ResolveColor(ctrl.border.color, hovered, pressed || opts.active || focused, disabled, borderCol);
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

    const bool hasIcon = opts.loading || opts.icon.IsValid();
    const bool hasCapsuleText = !effectiveCapsuleText.empty();
    const Color icoCol = opts.iconColor.has_value()
        ? (disabled ? opts.iconColor->WithAlphaF(0.40f) : *opts.iconColor)
        : textCol;
    const float padX = opts.square ? std::min(m.paddingX, theme.Scale(2.0f)) : m.paddingX;

    // Reserve optional trailing badge inside capsule
    const float badgeFontSize = theme.ResolveFontSize(opts.badge, m.fontSize * 0.78f);
    float badgeW = 0.0f;
    float badgeGap = 0.0f;
    bool drawBadge = false;
    if (!opts.badge.empty() && !opts.square) {
        badgeW = TextUtil::MeasureWidth(opts.badge, badgeFontSize, &canvas) + theme.Scale(10.0f);
        badgeGap = (hasIcon || hasCapsuleText) ? theme.Scale(6.0f) : 0.0f;
        if (capsuleRect.w >= padX * 2.0f + badgeW + badgeGap + (hasCapsuleText ? capFontSize * 1.5f : (hasIcon ? m.iconSize : 0.0f))) {
            drawBadge = true;
        }
    }
    const Rect contentBounds(
        capsuleRect.x,
        capsuleRect.y,
        std::max(0.0f, capsuleRect.w - (drawBadge ? (badgeW + badgeGap) : 0.0f)),
        capsuleRect.h
    );
    if (drawBadge) {
        const float badgeH = std::min(capsuleRect.h - theme.Scale(4.0f), badgeFontSize + theme.Scale(5.0f));
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
        canvas.DrawRoundRect(badgeRect, badgeH * 0.5f, badgeBg);
        canvas.DrawTextBox(opts.badge.text, badgeRect, badgeFontSize, badgeFg, 0.5f, 0.5f);
    }

    if (hasIcon && (!hasCapsuleText || opts.square)) {
        // Icon-only or Square ToolButton with icon
        const float isz = m.iconSize;
        float iconX = contentBounds.x + (contentBounds.w - isz) * 0.5f;
        if (!opts.square) {
            if (ctrl.align == Align::Start) {
                iconX = contentBounds.x + padX;
            } else if (ctrl.align == Align::End) {
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
        const float rightPad = drawBadge ? 0.0f : padX;
        const float availW = std::max(0.0f, contentBounds.w - padX - rightPad);
        const Rect textRect(contentBounds.x + padX, contentBounds.y, availW, contentBounds.h);
        float alignX = 0.5f;
        if (!opts.square) {
            if (ctrl.align == Align::Start) {
                alignX = 0.0f;
            } else if (ctrl.align == Align::End) {
                alignX = 1.0f;
            }
        }
        canvas.DrawTextBox(effectiveCapsuleText.text, textRect, capFontSize, textCol, alignX, 0.5f);
    } else if (hasIcon && hasCapsuleText) {
        // Icon + Label
        const float rightPad = drawBadge ? 0.0f : padX;
        const float gap = std::max(theme.Scale(4.0f), m.paddingX * 0.55f);
        if (opts.iconPlacement == Placement::Top) {
            const float vGap = theme.Scale(4.0f);
            const float textH = canvas.GetLineHeight(capFontSize);
            const float totalContentH = m.iconSize + vGap + textH;
            const float startY = contentBounds.y + (contentBounds.h - totalContentH) * 0.5f;
            float iconX = contentBounds.x + (contentBounds.w - m.iconSize) * 0.5f;
            float alignX = 0.5f;
            if (ctrl.align == Align::Start) {
                iconX = contentBounds.x + padX;
                alignX = 0.0f;
            } else if (ctrl.align == Align::End) {
                iconX = contentBounds.x + contentBounds.w - rightPad - m.iconSize;
                alignX = 1.0f;
            }
            DrawButtonGlyph(canvas, opts, Rect(iconX, startY, m.iconSize, m.iconSize), icoCol, loadingPhase);
            const Rect textRect(contentBounds.x + padX, startY + m.iconSize + vGap, std::max(0.0f, contentBounds.w - padX - rightPad), textH);
            canvas.DrawTextBox(effectiveCapsuleText.text, textRect, capFontSize, textCol, alignX, 0.5f);
        } else if (opts.iconPlacement == Placement::Bottom) {
            const float vGap = theme.Scale(4.0f);
            const float textH = canvas.GetLineHeight(capFontSize);
            const float totalContentH = m.iconSize + vGap + textH;
            const float startY = contentBounds.y + (contentBounds.h - totalContentH) * 0.5f;
            float iconX = contentBounds.x + (contentBounds.w - m.iconSize) * 0.5f;
            float alignX = 0.5f;
            if (ctrl.align == Align::Start) {
                iconX = contentBounds.x + padX;
                alignX = 0.0f;
            } else if (ctrl.align == Align::End) {
                iconX = contentBounds.x + contentBounds.w - rightPad - m.iconSize;
                alignX = 1.0f;
            }
            const Rect textRect(contentBounds.x + padX, startY, std::max(0.0f, contentBounds.w - padX - rightPad), textH);
            canvas.DrawTextBox(effectiveCapsuleText.text, textRect, capFontSize, textCol, alignX, 0.5f);
            DrawButtonGlyph(canvas, opts, Rect(iconX, startY + textH + vGap, m.iconSize, m.iconSize), icoCol, loadingPhase);
        } else if (opts.iconPlacement == Placement::Trailing) {
            const float availTextW = std::max(0.0f, contentBounds.w - padX - rightPad - m.iconSize - gap);
            const float iconY = contentBounds.y + (contentBounds.h - m.iconSize) * 0.5f;
            if (availTextW < capFontSize * 1.25f) {
                DrawButtonGlyph(canvas, opts, Rect(contentBounds.x + (contentBounds.w - m.iconSize) * 0.5f, iconY, m.iconSize, m.iconSize), icoCol, loadingPhase);
            } else {
                float textW = canvas.MeasureText(effectiveCapsuleText.text, capFontSize).x;
                keel::FixedString<256> truncatedLabel;
                std::string_view displayLabel = effectiveCapsuleText.text;
                if (textW > availTextW + kTextSubpixelTolerance && availTextW > 0.0f) {
                    truncatedLabel = TextUtil::TruncateWithEllipsis(effectiveCapsuleText.text, availTextW, capFontSize, &canvas);
                    displayLabel = truncatedLabel;
                    textW = canvas.MeasureText(displayLabel, capFontSize).x;
                }
                const float totalContentW = m.iconSize + gap + textW;
                const float centeredStartX = contentBounds.x + (contentBounds.w - totalContentW) * 0.5f;
                const float minStartX = contentBounds.x + padX;
                const float maxStartX = contentBounds.x + contentBounds.w - rightPad - totalContentW;
                float startX = centeredStartX;
                if (ctrl.align == Align::Start) {
                    startX = minStartX;
                } else if (ctrl.align == Align::End) {
                    startX = maxStartX;
                } else if (!effectiveCapsuleText.variants.empty() || contentBounds.w < totalContentW + padX + rightPad) {
                    startX = minStartX;
                }
                const Rect textRect(startX, contentBounds.y, textW, contentBounds.h);
                canvas.DrawTextBox(displayLabel, textRect, capFontSize, textCol, 0.0f, 0.5f);
                DrawButtonGlyph(canvas, opts, Rect(startX + textW + gap, iconY, m.iconSize, m.iconSize), icoCol, loadingPhase);
            }
        } else { // Leading / default
            const float availTextW = std::max(0.0f, contentBounds.w - padX - rightPad - m.iconSize - gap);
            const float iconY = contentBounds.y + (contentBounds.h - m.iconSize) * 0.5f;
            if (availTextW < capFontSize * 1.25f) {
                DrawButtonGlyph(canvas, opts, Rect(contentBounds.x + (contentBounds.w - m.iconSize) * 0.5f, iconY, m.iconSize, m.iconSize), icoCol, loadingPhase);
            } else {
                float textW = canvas.MeasureText(effectiveCapsuleText.text, capFontSize).x;
                keel::FixedString<256> truncatedLabel;
                std::string_view displayLabel = effectiveCapsuleText.text;
                if (textW > availTextW + kTextSubpixelTolerance && availTextW > 0.0f) {
                    truncatedLabel = TextUtil::TruncateWithEllipsis(effectiveCapsuleText.text, availTextW, capFontSize, &canvas);
                    displayLabel = truncatedLabel;
                    textW = canvas.MeasureText(displayLabel, capFontSize).x;
                }
                const float totalContentW = m.iconSize + gap + textW;
                const float centeredStartX = contentBounds.x + (contentBounds.w - totalContentW) * 0.5f;
                const float minStartX = contentBounds.x + padX;
                const float maxStartX = contentBounds.x + contentBounds.w - rightPad - totalContentW;
                float startX = centeredStartX;
                if (ctrl.align == Align::Start) {
                    startX = minStartX;
                } else if (ctrl.align == Align::End) {
                    startX = maxStartX;
                } else if (!effectiveCapsuleText.variants.empty() || contentBounds.w < totalContentW + padX + rightPad) {
                    startX = minStartX;
                }
                DrawButtonGlyph(canvas, opts, Rect(startX, iconY, m.iconSize, m.iconSize), icoCol, loadingPhase);
                const float remainingW = std::max(0.0f, (contentBounds.x + contentBounds.w - rightPad) - (startX + m.iconSize + gap));
                const Rect textRect(startX + m.iconSize + gap, contentBounds.y, remainingW, contentBounds.h);
                canvas.DrawTextBox(displayLabel, textRect, capFontSize, textCol, 0.0f, 0.5f);
            }
        }
    }

    canvas.PopClip();
}

} // namespace guikit
