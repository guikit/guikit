#include "guikit/surfaces/status_bar.hpp"
#include "guikit/display/separator.hpp"
#include "guikit/layout/spacer.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/surfaces/toolbar.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

struct StatusSegmentPayload {
    StatusItemOptions opts{};
    StatusBarStyle barStyle{StatusBarStyle::Surface};
    bool interactive{false};
};

keel::f32 ResolveStatusBarHeight(const UiTheme& theme, UiSize size, keel::f32 scale) noexcept {
    switch (size.GetPreset()) {
        case UiSize::Preset::Xs: return 20.0f * scale;
        case UiSize::Preset::Sm: return 22.0f * scale;
        case UiSize::Preset::Lg: return 30.0f * scale;
        case UiSize::Preset::Xl: return 34.0f * scale;
        case UiSize::Preset::Md:
        case UiSize::Preset::None:
            return 26.0f * scale;
        default:
            return 26.0f * theme.SizeFactor(size) * scale;
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
        return theme.ResolveColor(opts.color, hovered, opts.active, opts.disabled, fallback);
    }
    if (opts.text.color.HasAny()) {
        return theme.ResolveTextStateColor(opts.text.color).Resolve(hovered, opts.active, opts.disabled, fallback);
    }
    return fallback;
}

Vec2 MeasureStatusSegment(
    const Node* node,
    keel::f32 /*availableWidth*/,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const auto* payload = node->As<StatusSegmentPayload>();
    if (!payload) return {0.0f, 0.0f};
    const StatusItemOptions& o = payload->opts;
    const keel::f32 scale = 1.0f;
    const keel::f32 fontSize = theme.ResolveFontSize(o.text, TextRole::LabelSmall, node->uiSize);
    const keel::f32 padX = (payload->interactive ? 6.0f : 4.0f) * scale;
    const keel::f32 itemGap = 5.0f * scale;

    keel::f32 w = padX * 2.0f;
    keel::i32 parts = 0;

    if (o.dotColor.a > 0) {
        w += 6.0f * scale;
        ++parts;
    }
    if (o.icon.IsValid()) {
        w += 12.0f * scale;
        ++parts;
    }
    if (!o.text.empty() || o.text.HasContentOrReserve()) {
        const keel::f32 textW = TextUtil::MeasureWidth(o.text, fontSize, canvas);
        w += textW;
        ++parts;
    }
    if (!o.badge.empty() || o.badge.HasContentOrReserve()) {
        const keel::f32 badgeFont = keel::max(9.0f * scale, fontSize * 0.85f);
        const keel::f32 badgeTextW = TextUtil::MeasureWidth(o.badge, badgeFont, canvas);
        w += keel::max(14.0f * scale, badgeTextW + 8.0f * scale);
        ++parts;
    }
    if (parts > 1) {
        w += keel::as<keel::f32>(parts - 1) * itemGap;
    }
    if (o.minWidth.has_value() && *o.minWidth > 0.0f) {
        w = keel::max(w, *o.minWidth * scale);
    }

    const keel::f32 h = ResolveStatusBarHeight(theme, node->uiSize, scale) - 4.0f * scale;
    return {keel::ceil(w), keel::max(16.0f * scale, h)};
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

    const keel::f32 scale = 1.0f;
    const auto& p = theme.palette;
    const keel::f32 alphaMul = node->disabled ? 0.45f : 1.0f;

    // 1. Interactive hover / active background frame via active IStylePainter
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
        if (bg.a > 0 || (!node->disabled && (node->hovered || node->pressed || o.active))) {
            theme.Painter().DrawFrame(canvas, theme, FrameKind::ToolbarButton, FramePaintParams{
                .rect = r,
                .radius = keel::min(4.0f * scale, theme.GetRadius(UiSize::Sm)),
                .bg = bg,
                .hovered = node->hovered,
                .pressed = node->pressed,
                .active = o.active,
                .disabled = node->disabled,
                .accent = p.accent
            });
        }
    }

    const keel::f32 pressShift = payload->interactive
        ? theme.Painter().GetButtonContentOffset(theme, node->pressed, o.active, node->disabled)
        : 0.0f;
    const keel::f32 fontSize = theme.ResolveFontSize(o.text, TextRole::LabelSmall, node->uiSize);
    const keel::f32 padX = (payload->interactive ? 6.0f : 4.0f) * scale;
    const keel::f32 itemGap = 5.0f * scale;
    keel::f32 curX = r.x + padX + pressShift;
    const keel::f32 centerY = r.Center().y + pressShift;

    Color fg = ResolveSegmentTextColor(o, payload->barStyle, theme, node->hovered || o.active);
    if (o.active && payload->barStyle != StatusBarStyle::Accent && !o.color.HasAny() && !o.text.color.HasAny()) {
        fg = p.accent;
    }
    fg = fg.WithAlphaF(alphaMul);

    // 2. Optional status LED dot
    if (o.dotColor.a > 0) {
        const keel::f32 dotSz = 6.0f * scale;
        const Rect dotRect(curX, centerY - dotSz * 0.5f, dotSz, dotSz);
        canvas.DrawRoundRect(dotRect, dotSz * 0.5f, o.dotColor.WithAlphaF(alphaMul));
        curX += dotSz + itemGap;
    }

    // 3. Optional leading icon
    if (o.icon.IsValid()) {
        const keel::f32 iconSz = 12.0f * scale;
        const Rect iconRect(curX, centerY - iconSz * 0.5f, iconSz, iconSz);
        o.icon.Draw(canvas, iconRect, fg);
        curX += iconSz + itemGap;
    }

    // 4. Text label
    if (!o.text.empty()) {
        const Vec2 textSz = canvas.MeasureText(o.text.text, fontSize);
        const keel::f32 reservedW = keel::max(textSz.x, TextUtil::MeasureWidth(o.text, fontSize, &canvas));
        const keel::f32 textY = keel::round(centerY - textSz.y * 0.5f - 1.5f * scale);
        canvas.DrawText(o.text.text, {curX, textY}, fontSize, fg);
        curX += reservedW + itemGap;
    }

    // 5. Optional trailing badge pill
    if (!o.badge.empty()) {
        const keel::f32 badgeFont = keel::max(9.0f * scale, fontSize * 0.85f);
        const Vec2 bSz = canvas.MeasureText(o.badge.text, badgeFont);
        const keel::f32 pillH = keel::min(r.h - 2.0f * scale, keel::max(13.0f * scale, bSz.y + 2.0f * scale));
        const keel::f32 pillW = keel::max(pillH, bSz.x + 8.0f * scale);
        const Rect pillRect(curX, centerY - pillH * 0.5f, pillW, pillH);

        const Color pillBg = (payload->barStyle == StatusBarStyle::Accent)
            ? p.accentText.WithAlphaF(0.22f * alphaMul)
            : p.accent.WithAlphaF(0.18f * alphaMul);
        const Color pillFg = (payload->barStyle == StatusBarStyle::Accent)
            ? p.accentText.WithAlphaF(alphaMul)
            : theme.EnsureReadableInk(p.accent, p.surface).WithAlphaF(alphaMul);

        theme.Painter().DrawBadge(canvas, theme, pillRect, pillBg, Color::Transparent(), 0.0f, pillH * 0.5f);
        canvas.DrawTextBox(o.badge.text, pillRect, badgeFont, pillFg, 0.5f, 0.5f);
    }
}

} // namespace

void StatusBarScope::Item(
    const StatusItemOptions& opts,
    keel::SourceLocation loc
) {
    if (!m_ctx) return;
    detail::StatusBar::RenderSegment(*m_ctx, opts, m_opts.style, false, loc);
}

Response StatusBarScope::Action(
    const StatusItemOptions& opts,
    keel::SourceLocation loc
) {
    if (!m_ctx) return Response{};
    return detail::StatusBar::RenderSegment(*m_ctx, opts, m_opts.style, true, loc);
}

void StatusBarScope::Separator(keel::f32 heightRatio, keel::SourceLocation loc) {
    if (!m_ctx || !m_node) return;
    const Color col = (m_opts.style == StatusBarStyle::Accent)
        ? m_ctx->GetTheme().palette.accentText.WithAlphaF(0.3f)
        : m_node->borderColor;
    guikit::detail::Separator::Run(*m_ctx, heightRatio, col, loc);
}

void StatusBarScope::Spacer(keel::SourceLocation loc) {
    if (!m_ctx) return;
    const Id id = m_ctx->MakeChildId(loc);
    Node* spacer = m_ctx->CreateNode(NodeType::Spacer, id);
    spacer->widthMode = SizeMode::Stretch;
    spacer->isSpacer = true;
    spacer->flexGrow = 1.0f;
    spacer->measureFn = [](const Node*, keel::f32 /*availableWidth*/, ICanvas*, const UiTheme&) noexcept -> Vec2 {
        return {0.0f, 0.0f};
    };
}

void StatusBarScope::Gap(keel::f32 sizeDp, keel::SourceLocation loc) {
    if (!m_ctx) return;
    guikit::detail::Spacer::Gap(*m_ctx, sizeDp, loc);
}

StatusBarScope detail::StatusBar::Open(
    Context& ctx,
    const StatusBarOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    Node* node = ctx.CreateNode(NodeType::StatusBar, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const keel::f32 density = node->GetDensity();
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = node->disabled;
    const keel::f32 barH = opts.height.has_value()
        ? *opts.height
        : ResolveStatusBarHeight(theme, activeSize, 1.0f);

    Response res{};
    res.id = id;
    res.rect = ctx.GetPrevUnclippedBounds(id);
    res.ctx = &ctx;

    node->direction = FlexDirection::Row;
    node->align = Alignment::Center;
    node->gap = opts.gap.has_value()
        ? theme.ResolveGap(opts.gap, activeSize, density)
        : (6.0f * density);

    if (opts.padding.has_value()) {
        node->padding = theme.ResolvePadding(opts.padding, activeSize, density);
    } else {
        node->padding = Padding::Symmetric(8.0f * density, 2.0f * density);
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
        node->backgroundColor = theme.ResolveColor(opts.background, false, false, effectiveDisabled, styleBg);
    } else {
        node->backgroundColor = opts.background.IsSurface() ? styleBg.ScaleAlpha(opts.background.alphaScale) : styleBg;
    }

    node->SetSurfaceRole(SurfaceRole::DockEdge);
    node->SetDockSeam((theme.shellStyle == ShellStyle::Floating) ? DockSeam::None : DockSeam::Top);
    node->borderColor = theme.ResolveColor(opts.border.color, false, false, effectiveDisabled, pal.border);
    node->borderWidth = (opts.style == StatusBarStyle::Accent && !opts.border.color.HasAny())
        ? 0.0f
        : ((opts.border.width == 1.0f) ? theme.shell.panelBorderWidth : opts.border.width);
    ctx.ApplySurfaceGradients(
        node,
        theme,
        opts.background.IsBackgroundEnabled(true) ? opts.background : StateColor{},
        (node->borderWidth > 0.0f) ? opts.border.color : StateColor{},
        false,
        false,
        effectiveDisabled
    );
    node->explicitBorder = opts.border.HasExplicitStroke() ? 1 : 0;
    node->explicitRadius = opts.border.radius.has_value() ? 1 : 0;
    node->cornerRadius = opts.border.radius.has_value()
        ? *opts.border.radius
        : theme.shell.panelRadius;

    node->uiSize = activeSize;
    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(theme.shellStyle),
        keel::as<keel::u8>(opts.style),
        node->fixedHeight,
        opts.background,
        opts.border
    );

    ctx.FinishContainer(node);

    return StatusBarScope(&ctx, node, res, opts);
}

Response detail::StatusBar::RenderSegment(
    Context& ctx,
    const StatusItemOptions& opts,
    StatusBarStyle barStyle,
    bool interactive,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const Node* parent = ctx.GetCurrentContainer();
    const UiSize activeSize = (parent != nullptr) ? parent->uiSize : UiSize::Md;

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
    node->focused = interactive && ctx.IsFocusVisible(id);
    node->active = opts.active;

    auto* payload = ctx.AllocPayload<StatusSegmentPayload>();
    payload->opts = opts;
    payload->opts.text = ctx.AllocText(opts.text);
    payload->opts.badge = ctx.AllocText(opts.badge);
    payload->opts.tooltip = ctx.AllocText(opts.tooltip);
    payload->opts.icon = ctx.AllocIcon(opts.icon);
    payload->barStyle = barStyle;
    payload->interactive = interactive;
    node->SetPayload(payload);
    node->text = payload->opts.text.text;

    node->visualHash = detail::HashArgs(
        payload->opts.text,
        payload->opts.badge,
        keel::as<keel::u8>(payload->opts.icon.GetType()),
        keel::as<keel::u8>(payload->opts.icon.GetId()),
        payload->opts.icon.GetPath(),
        payload->opts.dotColor,
        payload->opts.color,
        keel::as<keel::u8>(barStyle),
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
