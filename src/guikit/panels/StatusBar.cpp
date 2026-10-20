#include "guikit/panels/StatusBar.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/panels/Toolbar.hpp"

namespace guikit {

namespace {

struct StatusSegmentPayload {
    StatusItemOptions opts{};
    StatusBarStyle barStyle{StatusBarStyle::Surface};
    bool interactive{false};
};

float ResolveStatusBarHeight(const UiTheme& /*theme*/, UiSize size, float scale) noexcept {
    switch (size) {
        case UiSize::Xs: return 20.0f * scale;
        case UiSize::Sm: return 22.0f * scale;
        case UiSize::Lg: return 30.0f * scale;
        case UiSize::Xl: return 34.0f * scale;
        case UiSize::Md:
        default:         return 26.0f * scale;
    }
}

Color ResolveSegmentTextColor(
    const StatusItemOptions& opts,
    StatusBarStyle barStyle,
    const UiTheme& theme,
    bool hovered
) noexcept {
    const auto& p = theme.palette;
    const Color fallback = (barStyle == StatusBarStyle::Accent)
        ? p.accentText
        : (hovered ? p.textPrimary : p.textSecondary);
    if (opts.color.HasAny()) {
        return opts.color.Resolve(hovered, opts.active, opts.disabled, fallback);
    }
    if (opts.text.color.HasAny()) {
        return opts.text.color.Resolve(hovered, opts.active, opts.disabled, fallback);
    }
    return fallback;
}

Vec2 MeasureStatusSegment(
    const Node* node,
    float /*availableWidth*/,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const auto* payload = node->As<StatusSegmentPayload>();
    if (!payload) return {0.0f, 0.0f};
    const StatusItemOptions& o = payload->opts;
    const float scale = theme.Scale(1.0f);
    const float fontSize = theme.ResolveFontSize(o.text, TextRole::LabelSmall, node->uiSize);
    const float padX = (payload->interactive ? 6.0f : 4.0f) * scale;
    const float itemGap = 5.0f * scale;

    float w = padX * 2.0f;
    int parts = 0;

    if (o.dotColor.a > 0) {
        w += 6.0f * scale;
        ++parts;
    }
    if (o.icon.IsValid()) {
        w += 12.0f * scale;
        ++parts;
    }
    if (!o.text.empty()) {
        const float textW = canvas
            ? canvas->MeasureText(o.text.text, fontSize).x
            : (static_cast<float>(o.text.text.size()) * fontSize * 0.56f);
        w += textW;
        ++parts;
    }
    if (!o.badge.empty()) {
        const float badgeFont = std::max(9.0f * scale, fontSize * 0.85f);
        const float badgeTextW = canvas
            ? canvas->MeasureText(o.badge.text, badgeFont).x
            : (static_cast<float>(o.badge.text.size()) * badgeFont * 0.58f);
        w += std::max(14.0f * scale, badgeTextW + 8.0f * scale);
        ++parts;
    }
    if (parts > 1) {
        w += static_cast<float>(parts - 1) * itemGap;
    }

    const float h = ResolveStatusBarHeight(theme, node->uiSize, scale) - 4.0f * scale;
    return {std::ceil(w), std::max(16.0f * scale, h)};
}

void RenderStatusSegment(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) noexcept {
    const auto* payload = node->As<StatusSegmentPayload>();
    if (!payload) return;
    const StatusItemOptions& o = payload->opts;
    const Rect& r = node->computedRect;
    if (r.w <= 0.5f || r.h <= 0.5f) return;

    const float scale = theme.Scale(1.0f);
    const auto& p = theme.palette;
    const float alphaMul = node->disabled ? 0.45f : 1.0f;

    // 1. Interactive hover / active background pill
    if (payload->interactive || o.active) {
        Color bg = Color::Transparent();
        if (payload->barStyle == StatusBarStyle::Accent) {
            if (node->pressed || o.active) {
                bg = p.accentText.WithAlphaF(0.22f * alphaMul);
            } else if (node->hovered) {
                bg = p.accentText.WithAlphaF(0.14f * alphaMul);
            }
        } else {
            if (node->pressed) {
                bg = p.surfaceActive.WithAlphaF(alphaMul);
            } else if (o.active) {
                bg = p.accent.WithAlphaF(0.16f * alphaMul);
            } else if (node->hovered) {
                bg = p.surfaceHover.WithAlphaF(alphaMul);
            }
        }
        if (bg.a > 0) {
            canvas.DrawRoundRect(r, 4.0f * scale, bg);
        }
    }

    const float fontSize = theme.ResolveFontSize(o.text, TextRole::LabelSmall, node->uiSize);
    const float padX = (payload->interactive ? 6.0f : 4.0f) * scale;
    const float itemGap = 5.0f * scale;
    float curX = r.x + padX;
    const float centerY = r.Center().y;

    Color fg = ResolveSegmentTextColor(o, payload->barStyle, theme, node->hovered || o.active);
    if (o.active && payload->barStyle != StatusBarStyle::Accent && !o.color.HasAny() && !o.text.color.HasAny()) {
        fg = p.accent;
    }
    fg = fg.WithAlphaF(alphaMul);

    // 2. Optional status LED dot
    if (o.dotColor.a > 0) {
        const float dotSz = 6.0f * scale;
        const Rect dotRect(curX, centerY - dotSz * 0.5f, dotSz, dotSz);
        canvas.DrawRoundRect(dotRect, dotSz * 0.5f, o.dotColor.WithAlphaF(alphaMul));
        curX += dotSz + itemGap;
    }

    // 3. Optional leading icon
    if (o.icon.IsValid()) {
        const float iconSz = 12.0f * scale;
        const Rect iconRect(curX, centerY - iconSz * 0.5f, iconSz, iconSz);
        o.icon.Draw(canvas, iconRect, fg);
        curX += iconSz + itemGap;
    }

    // 4. Text label
    if (!o.text.empty()) {
        const Vec2 textSz = canvas.MeasureText(o.text.text, fontSize);
        const float textY = centerY - textSz.y * 0.5f;
        canvas.DrawText(o.text.text, {curX, textY}, fontSize, fg);
        curX += textSz.x + itemGap;
    }

    // 5. Optional trailing badge pill
    if (!o.badge.empty()) {
        const float badgeFont = std::max(9.0f * scale, fontSize * 0.85f);
        const Vec2 bSz = canvas.MeasureText(o.badge.text, badgeFont);
        const float pillH = std::min(r.h - 2.0f * scale, std::max(13.0f * scale, bSz.y + 2.0f * scale));
        const float pillW = std::max(pillH, bSz.x + 8.0f * scale);
        const Rect pillRect(curX, centerY - pillH * 0.5f, pillW, pillH);

        const Color pillBg = (payload->barStyle == StatusBarStyle::Accent)
            ? p.accentText.WithAlphaF(0.22f * alphaMul)
            : p.accent.WithAlphaF(0.18f * alphaMul);
        const Color pillFg = (payload->barStyle == StatusBarStyle::Accent)
            ? p.accentText.WithAlphaF(alphaMul)
            : p.accent.WithAlphaF(alphaMul);

        canvas.DrawRoundRect(pillRect, pillH * 0.5f, pillBg);
        canvas.DrawTextBox(o.badge.text, pillRect, badgeFont, pillFg, 0.5f, 0.5f);
    }
}

} // namespace

void StatusBarScope::Item(
    const StatusItemOptions& opts,
    std::source_location loc
) {
    if (!m_ctx) return;
    StatusBar::RenderSegment(*m_ctx, opts, m_opts.style, false, loc);
}

Response StatusBarScope::Action(
    const StatusItemOptions& opts,
    std::source_location loc
) {
    if (!m_ctx) return Response{};
    return StatusBar::RenderSegment(*m_ctx, opts, m_opts.style, true, loc);
}

void StatusBarScope::Separator(float heightRatio, std::source_location loc) {
    if (!m_ctx || !m_node) return;
    const Id id = m_ctx->MakeChildId(loc);
    const float scale = m_ctx->GetCurrentScale();

    Node* sep = m_ctx->CreateNode(NodeType::Separator, id);
    sep->widthMode = SizeMode::Fixed;
    sep->fixedWidth = 8.0f * scale;
    sep->heightMode = SizeMode::Fixed;
    const float parentH = (m_node->fixedHeight > 0.0f) ? m_node->fixedHeight : (26.0f * scale);
    sep->fixedHeight = parentH * std::clamp(heightRatio, 0.2f, 0.9f);
    sep->borderColor = (m_opts.style == StatusBarStyle::Accent)
        ? m_ctx->GetTheme().palette.accentText.WithAlphaF(0.3f)
        : m_node->borderColor;
    sep->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* /*canvas*/, const UiTheme& /*theme*/) noexcept -> Vec2 {
        return {
            n->fixedWidth > 0.0f ? n->fixedWidth : 8.0f,
            n->fixedHeight > 0.0f ? n->fixedHeight : 14.0f
        };
    };
    sep->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        Toolbar::RenderSeparator(canvas, n->computedRect, n->borderColor, theme, Orientation::Horizontal);
    };
}

void StatusBarScope::Spacer(std::source_location loc) {
    if (!m_ctx) return;
    const Id id = m_ctx->MakeChildId(loc);
    Node* spacer = m_ctx->CreateNode(NodeType::Spacer, id);
    spacer->widthMode = SizeMode::Stretch;
    spacer->isSpacer = true;
    spacer->flexGrow = 1.0f;
    spacer->measureFn = [](const Node*, float /*availableWidth*/, ICanvas*, const UiTheme&) noexcept -> Vec2 {
        return {0.0f, 0.0f};
    };
}

StatusBarScope StatusBar::Open(
    Context& ctx,
    const StatusBarOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const UiTheme& theme = ctx.GetTheme();
    const float barH = opts.height.has_value()
        ? theme.Scale(*opts.height)
        : ResolveStatusBarHeight(theme, activeSize, activeScale);

    Response res{};
    res.id = id;
    res.rect = ctx.GetPrevUnclippedBounds(id);
    res.ctx = &ctx;

    Node* node = ctx.CreateNode(NodeType::StatusBar, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = FlexDirection::Row;
    node->align = Alignment::Center;
    node->gap = opts.gap.has_value()
        ? theme.ResolveGap(opts.gap, activeSize)
        : (6.0f * activeScale);

    if (opts.padding.has_value()) {
        node->padding = theme.ResolvePadding(opts.padding, activeSize);
    } else {
        node->padding = Padding::Symmetric(8.0f * activeScale, 2.0f * activeScale);
    }

    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fixed;
    node->fixedHeight = barH;

    const auto& pal = theme.palette;
    const Color styleBg = (opts.style == StatusBarStyle::Accent)
        ? pal.accent
        : ((opts.style == StatusBarStyle::Header) ? pal.surfaceElevated : pal.surface);
    if (!opts.background.IsBackgroundEnabled(true)) {
        node->backgroundColor = Color::Transparent();
    } else if (opts.background.HasAny() && !opts.background.IsSurface()) {
        node->backgroundColor = opts.background.Resolve(false, false, effectiveDisabled, styleBg);
    } else {
        node->backgroundColor = styleBg;
    }

    node->borderColor = opts.border.color.Resolve(false, false, effectiveDisabled, pal.border);
    node->borderWidth = (opts.style == StatusBarStyle::Accent && !opts.border.color.HasAny())
        ? 0.0f
        : theme.Scale(opts.border.width);
    node->cornerRadius = opts.border.radius.has_value()
        ? theme.Scale(*opts.border.radius)
        : 0.0f;

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->visualHash = detail::HashArgs(
        static_cast<uint8_t>(opts.style),
        node->fixedHeight,
        opts.background,
        opts.border
    );

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(effectiveDisabled);
    ctx.PushContainer(node);

    return StatusBarScope(&ctx, node, res, opts);
}

Response StatusBar::RenderSegment(
    Context& ctx,
    const StatusItemOptions& opts,
    StatusBarStyle barStyle,
    bool interactive,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize activeSize = ctx.GetCurrentSize();

    if (interactive) {
        ctx.RegisterFocusable(id, effectiveDisabled);
    }

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
    } else if (interactive) {
        if (res.clicked) {
            ctx.RequestFocus(id);
        }
        if (ctx.IsFocused(id) && (ctx.ConsumeKey(Key::Space) || ctx.ConsumeKey(Key::Enter))) {
            res.clicked = true;
        }
    } else {
        res.clicked = false;
        res.pressed = false;
    }

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    Node* node = ctx.CreateNode(interactive ? NodeType::Button : NodeType::Text, id);
    node->widthMode = SizeMode::Fit;
    node->heightMode = SizeMode::Fit;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = interactive && res.hovered;
    node->pressed = interactive && res.pressed;
    node->focused = interactive && ctx.IsFocused(id);
    node->active = opts.active;

    auto* payload = ctx.AllocPayload<StatusSegmentPayload>();
    payload->opts = opts;
    payload->opts.text = ctx.AllocText(opts.text);
    payload->opts.badge = ctx.AllocText(opts.badge);
    payload->opts.tooltip = ctx.AllocText(opts.tooltip);
    payload->barStyle = barStyle;
    payload->interactive = interactive;
    node->SetPayload(payload);
    node->text = payload->opts.text.text;

    node->visualHash = detail::HashArgs(
        payload->opts.text,
        payload->opts.badge,
        static_cast<uint8_t>(payload->opts.icon.GetType()),
        static_cast<uint8_t>(payload->opts.icon.GetId()),
        payload->opts.icon.GetPath(),
        payload->opts.dotColor,
        payload->opts.color,
        static_cast<uint8_t>(barStyle),
        interactive,
        opts.active,
        node->hovered,
        node->pressed,
        node->disabled
    );

    node->measureFn = &MeasureStatusSegment;
    node->renderFn = &RenderStatusSegment;

    return res;
}

} // namespace guikit
