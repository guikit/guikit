#include "guikit/surfaces/activity_bar.hpp"
#include "guikit/display/separator.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "keel/math.h"

namespace guikit {

void ActivityBarScope::InvalidateContext() noexcept {
    if (m_ctx) {
        m_ctx->InvalidateAll();
    }
}

Response ActivityBarScope::Item(
    const ActivityBarItem& item,
    keel::SourceLocation loc
) {
    if (!m_ctx) return Response{};
    return detail::ActivityBar::Item(*m_ctx, item, m_opts, loc);
}

void ActivityBarScope::Separator(keel::SourceLocation loc) {
    if (!m_ctx || !m_node) return;
    guikit::detail::Separator::Run(*m_ctx, 0.0f, Color::Transparent(), loc);
}

void ActivityBarScope::Spacer(keel::f32 flex, keel::SourceLocation loc) {
    if (!m_ctx) return;
    const Id id = m_ctx->MakeChildId(loc);
    Node* spacer = m_ctx->CreateNode(NodeType::Spacer, id);
    spacer->widthMode = SizeMode::Stretch;
    spacer->heightMode = SizeMode::Stretch;
    spacer->isSpacer = true;
    spacer->flexGrow = flex;
    spacer->measureFn = [](const Node*, keel::f32 /*availableWidth*/, ICanvas*, const UiTheme&) noexcept -> Vec2 {
        return {0.0f, 0.0f};
    };
}

ActivityBarScope detail::ActivityBar::Open(
    Context& ctx,
    const ActivityBarOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    Response res{};
    res.id = id;
    res.rect = ctx.GetPrevUnclippedBounds(id);
    res.ctx = &ctx;

    Node* node = ctx.CreateNode(NodeType::Sidebar, id, opts.disabled);
    const UiTheme& theme = node->GetTheme(ctx.GetTheme());
    const auto& pal = theme.palette;
    const keel::f32 density = node->GetDensity();
    const bool activeDisabled = node->disabled;
    node->direction = FlexDirection::Column;
    node->align = Alignment::Center;
    node->justify = Justify::Start;
    const bool effectiveFlush = opts.flushWithSidebar.value_or(theme.shellStyle == ShellStyle::Docked);
    node->dockFlags =
        ((opts.side == SidebarSide::Right) ? dock_flags::kRight : dock_flags::kNone) |
        dock_flags::kCompact |
        (effectiveFlush ? dock_flags::kFlush : dock_flags::kNone);
    node->gap = (opts.gap.has_value() ? *opts.gap : 4.0f) * density;
    node->heightMode = SizeMode::Stretch;

    if (opts.width.has_value() && *opts.width > 0.0f) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = *opts.width;
    } else {
        node->widthMode = SizeMode::Fit;
        node->fixedWidth = 0.0f;
    }
    node->minWidth = 0.0f;
    node->padding = opts.padding.has_value()
        ? theme.ResolvePadding(opts.padding, activeSize, density)
        : theme.GetPadding(UiSize::Sm).ScaledBy(density);
    node->backgroundColor = opts.background.IsBackgroundEnabled(true)
        ? theme.ResolveColor(opts.background, false, false, activeDisabled, pal.surface)
        : Color::Transparent();
    node->SetSurfaceRole(SurfaceRole::DockEdge);
    node->SetDockSeam((opts.side == SidebarSide::Right) ? DockSeam::Left : DockSeam::Right);
    node->borderWidth = (opts.border.width == 1.0f) ? theme.shell.panelBorderWidth : opts.border.width;
    node->borderColor = (opts.border.width > 0.0f)
        ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, pal.border)
        : Color::Transparent();
    node->cornerRadius = opts.border.radius.has_value()
        ? *opts.border.radius
        : (effectiveFlush ? 0.0f : theme.shell.panelRadius);
    node->clipChildren = true;
    node->uiSize = activeSize;

    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(opts.side),
        keel::as<keel::u8>(opts.style),
        opts.color,
        opts.flushWithSidebar,
        opts.background,
        opts.border
    );

    ctx.FinishContainer(node);

    return ActivityBarScope(&ctx, node, res, opts);
}

Response detail::ActivityBar::Run(
    Context& ctx,
    keel::i32& activeIndex,
    keel::ArrayRef<ActivityBarItem> items,
    const ActivityBarOptions& opts,
    keel::SourceLocation loc
) {
    Response barRes{};
    if (auto scope = Open(ctx, opts, loc)) {
        barRes = scope.response;
        for (keel::usize i = 0; i < items.size(); ++i) {
            Response itemRes = scope.Item(activeIndex, keel::as<keel::i32>(i), items[i], loc);
            if (itemRes.clicked) {
                barRes.clicked = true;
            }
            if (itemRes.changed) {
                barRes.changed = true;
            }
            if (itemRes.hovered) {
                barRes.hovered = true;
            }
        }
    }
    return barRes;
}

Response detail::ActivityBar::Item(
    Context& ctx,
    const ActivityBarItem& item,
    const ActivityBarOptions& barOpts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = barOpts.size;
    const bool effectiveDisabled = item.disabled || barOpts.disabled || ctx.IsDisabled();

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

    if (res.hovered) {
        const UiText effTooltip = !item.tooltip.empty() ? item.tooltip : item.label;
        if (!effTooltip.empty()) {
            ctx.SetTooltip(effTooltip, res.rect);
        }
    }

    const ComponentMetrics& m = ctx.GetTheme().GetMetrics(activeSize);
    Node* node = ctx.CreateNode(NodeType::SidebarItem, id);
    auto* data = ctx.AllocPayload<ActivityBarItemData>();
    data->item = item;
    data->item.label = ctx.AllocText(item.label);
    data->item.tooltip = ctx.AllocText(item.tooltip);
    data->item.badge = ctx.AllocText(item.badge);
    data->item.icon = ctx.AllocIcon(item.icon);
    data->barOpts = barOpts;

    node->text = data->item.label.text;
    node->SetPayload(data);
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->active = item.active;
    node->focused = ctx.IsFocusVisible(id);
    node->widthMode = SizeMode::Fixed;
    node->heightMode = SizeMode::Fixed;
    node->fixedWidth = m.defaultHeight;
    node->fixedHeight = m.defaultHeight;

    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(barOpts.side),
        keel::as<keel::u8>(barOpts.style),
        item.color.HasAny() ? item.color : barOpts.color,
        item.active,
        effectiveDisabled,
        keel::as<keel::u8>(data->item.icon.GetType()),
        keel::as<keel::u8>(data->item.icon.GetId()),
        data->item.icon.GetPath(),
        data->item.label,
        data->item.badge
    );

    node->measureFn = [](const Node* n, keel::f32 /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<ActivityBarItemData>();
        if (!d) return {0.0f, 0.0f};
        return detail::ActivityBar::MeasureItem(d->item, d->barOpts, n->uiSize, canvas, theme);
    };

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<ActivityBarItemData>();
        if (!d) return;
        detail::ActivityBar::RenderItem(canvas, n->computedRect, d->item, d->barOpts, n->uiSize, n->hovered, n->pressed, n->disabled, theme, n->focused);
    };

    return res;
}

Vec2 detail::ActivityBar::MeasureItem(
    const ActivityBarItem& /*item*/,
    const ActivityBarOptions& /*opts*/,
    UiSize size,
    ICanvas* /*canvas*/,
    const UiTheme& theme
) {
    const ComponentMetrics& m = theme.GetMetrics(size);
    return {m.defaultHeight, m.defaultHeight};
}

void detail::ActivityBar::RenderItem(
    ICanvas& canvas,
    Rect bounds,
    const ActivityBarItem& item,
    const ActivityBarOptions& opts,
    UiSize size,
    bool hovered,
    bool pressed,
    bool disabled,
    const UiTheme& theme,
    bool focused
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const ComponentMetrics& m = theme.GetMetrics(size);
    const auto& pal = theme.palette;
    const StateColor& effColor = item.color.HasAny() ? item.color : opts.color;
    const VariantColors& vDefault = theme.GetVariant(UiVariant::Primary);
    VariantColors v = vDefault;
    if (effColor.HasAny()) {
        v.bg = effColor.Resolve(false, false, false, vDefault.bg);
        v.bgHover = effColor.hover.has_value() ? *effColor.hover : v.bg.WithAlphaF(0.85f);
        v.bgActive = effColor.active.has_value() ? *effColor.active : v.bg.WithAlphaF(0.75f);
    }
    const keel::f32 radius = m.cornerRadius;

    Color bg = Color::Transparent();
    Color borderCol = Color::Transparent();
    keel::f32 borderW = 0.0f;
    Color textCol = item.label.color.HasAny()
        ? item.label.color.Resolve(hovered, pressed || item.active, disabled, pal.textPrimary)
        : pal.textPrimary;
    Color iconCol = pal.textSecondary;

    if (opts.style == ActivityBarStyle::Pill) {
        if (item.active) {
            bg = pressed ? v.bgActive : (hovered ? v.bgHover : v.bg);
            textCol = item.label.color.HasAny()
                ? item.label.color.Resolve(hovered, true, disabled, v.text)
                : v.text;
            iconCol = textCol;
        } else if (pressed) {
            bg = pal.surfaceActive;
            iconCol = pal.textPrimary;
        } else if (hovered) {
            bg = pal.surfaceHover;
            iconCol = pal.textPrimary;
        }
    } else { // ActivityBarStyle::Indicator
        if (item.active) {
            bg = pressed ? v.bg.WithAlphaF(0.28f) : (hovered ? v.bg.WithAlphaF(0.22f) : v.bg.WithAlphaF(0.16f));
            iconCol = v.bg;
        } else if (pressed) {
            bg = pal.surfaceActive;
            iconCol = pal.textPrimary;
        } else if (hovered) {
            bg = pal.surfaceHover;
            iconCol = pal.textPrimary;
        }
    }

    if (disabled) {
        bg = bg.WithAlphaF(theme.disabledOpacity);
        textCol = textCol.WithAlphaF(theme.disabledOpacity);
        iconCol = iconCol.WithAlphaF(theme.disabledOpacity);
    } else if (focused) {
        borderCol = pal.focusRing;
        borderW = 1.5f;
    }

    canvas.PushClip(bounds);

    if (bg.a > 0 || borderW > 0.0f) {
        canvas.DrawRoundRect(bounds, radius, bg, borderCol, borderW);
    }

    // 1. Active edge indicator bar (drawn on Left edge when docked Left, Right edge when docked Right)
    if (opts.style == ActivityBarStyle::Indicator && item.active) {
        const keel::f32 barW = 3.0f;
        const keel::f32 barPadY = keel::min(6.0f, bounds.h * 0.20f);
        const keel::f32 barX = (opts.side == SidebarSide::Right)
            ? (bounds.Right() - barW - 1.0f)
            : (bounds.x + 1.0f);
        const Rect barRect(barX, bounds.y + barPadY, barW, keel::max(4.0f, bounds.h - barPadY * 2.0f));
        const Color barCol = disabled ? v.bg.WithAlphaF(theme.disabledOpacity) : v.bg;
        canvas.DrawRoundRect(barRect, barW * 0.5f, barCol);
    }

    // 2. Centered Icon (or fallback monogram letter)
    const keel::f32 iconSz = keel::min(bounds.w * 0.56f, keel::round(m.iconSize * 1.12f));
    if (item.icon.IsValid()) {
        const Rect iconRect(
            bounds.x + (bounds.w - iconSz) * 0.5f,
            bounds.y + (bounds.h - iconSz) * 0.5f,
            iconSz,
            iconSz
        );
        item.icon.Draw(canvas, iconRect, iconCol);
    } else if (!item.label.empty()) {
        const keel::f32 fontSize = theme.ResolveFontSize(item.label, TextRole::LabelLarge, size);
        canvas.DrawTextBox(item.label.text.substr(0, 1), bounds, fontSize, iconCol, 0.5f, 0.5f);
    }

    // 3. Corner Badge (compact counter pill in the bottom-right corner with surface cutout border)
    if (!item.badge.empty()) {
        const keel::f32 badgeFont = keel::max(9.0f, theme.ResolveFontSize(item.badge, TextRole::LabelSmall, size) - 1.5f);
        const Vec2 badgeDims = canvas.MeasureText(item.badge.text, badgeFont);
        const keel::f32 badgeH = keel::min(bounds.h * 0.48f, keel::max(13.0f, badgeFont + 3.0f));
        const keel::f32 badgeW = keel::min(bounds.w - 4.0f, keel::max(badgeH, badgeDims.x + 6.0f));
        const Rect badgeRect(
            bounds.Right() - badgeW - 2.0f,
            bounds.Bottom() - badgeH - 2.0f,
            badgeW,
            badgeH
        );
        const Color badgeBg = disabled ? v.bg.WithAlphaF(theme.disabledOpacity) : v.bg;
        const Color badgeTextCol = item.badge.color.HasAny()
            ? item.badge.color.Resolve(hovered, item.active, disabled, v.text)
            : (disabled ? v.text.WithAlphaF(theme.disabledOpacity) : v.text);
        const Color cutoutCol = (opts.background.HasAny() && !opts.background.IsSurface())
            ? opts.background.Resolve(false, false, disabled, pal.surface)
            : pal.surface;
        canvas.DrawRoundRect(badgeRect, badgeH * 0.5f, badgeBg, cutoutCol, 1.5f);
        canvas.DrawTextBox(item.badge.text, badgeRect, badgeFont, badgeTextCol, 0.5f, 0.5f);
    }

    canvas.PopClip();
}

} // namespace guikit
