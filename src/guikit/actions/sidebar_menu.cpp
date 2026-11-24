#include "guikit/actions/sidebar_menu.hpp"
#include "guikit/display/separator.hpp"
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/display/heading.hpp"

namespace guikit {

void SidebarMenuScope::InvalidateContext() noexcept {
    if (m_ctx) {
        m_ctx->InvalidateAll();
    }
}

void SidebarMenuScope::Section(
    UiText title,
    bool separator,
    keel::SourceLocation loc
) {
    if (!m_ctx || !m_node) return;

    if (m_node->childCount > 0 && m_opts.sectionGap > 0.0f) {
        const Id gapId = m_ctx->MakeChildId(loc);
        Node* gapNode = m_ctx->CreateNode(NodeType::Box, gapId);
        gapNode->widthMode = SizeMode::Stretch;
        gapNode->heightMode = SizeMode::Fixed;
        gapNode->fixedHeight = m_opts.sectionGap * m_node->GetDensity();
    }

    if (m_opts.collapsed) {
        if (separator) {
            Separator(loc);
        }
        return;
    }

    detail::Heading::Run(*m_ctx, HeadingOptions{
        .label = title,
        .level = HeadingLevel::H6,
        .separator = separator ? HeadingSeparator::Bottom : HeadingSeparator::None
    }, loc);
}

Response SidebarMenuScope::Item(
    const SidebarMenuItem& item,
    keel::SourceLocation loc
) {
    if (!m_ctx) return Response{};
    return detail::SidebarMenu::Item(*m_ctx, item, m_opts, loc);
}

void SidebarMenuScope::Separator(keel::SourceLocation loc) {
    if (!m_ctx || !m_node) return;
    guikit::detail::Separator::Run(*m_ctx, 0.0f, Color::Transparent(), loc);
}

void SidebarMenuScope::Spacer(keel::f32 flex, keel::SourceLocation loc) {
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

SidebarMenuScope detail::SidebarMenu::Open(
    Context& ctx,
    const SidebarMenuOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();

    Node* node = ctx.CreateNode(NodeType::Column, id);
    node->direction = FlexDirection::Column;
    node->align = Alignment::Stretch;
    node->justify = Justify::Start;
    node->gap = (opts.gap.has_value() ? *opts.gap : 4.0f) * node->GetDensity();

    SidebarMenuOptions effectiveOpts = opts;
    for (Node* p = node->parent; p != nullptr; p = p->parent) {
        if (p->type == NodeType::Sidebar) {
            if (p->IsCompactDock()) {
                effectiveOpts.collapsed = true;
            }
            break;
        }
    }

    if (effectiveOpts.collapsed) {
        node->widthMode = SizeMode::Fit;
        if (node->parent && node->parent->type == NodeType::Sidebar) {
            node->parent->widthMode = SizeMode::Fit;
            node->parent->fixedWidth = 0.0f;
            node->parent->minWidth = 0.0f;
        }
    } else if (effectiveOpts.width.has_value()) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = *effectiveOpts.width;
    } else {
        node->widthMode = SizeMode::Stretch;
    }
    node->heightMode = SizeMode::Fit;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    ctx.PushContainer(node);

    return SidebarMenuScope(&ctx, node, effectiveOpts);
}

Response detail::SidebarMenu::Run(
    Context& ctx,
    keel::i32& activeIndex,
    keel::ArrayRef<SidebarMenuItem> items,
    const SidebarMenuOptions& opts,
    keel::SourceLocation loc
) {
    Response barRes{};
    if (auto scope = Open(ctx, opts, loc)) {
        barRes.id = scope.GetNode() ? scope.GetNode()->id : Id::Null();
        for (keel::usize i = 0; i < items.size(); ++i) {
            if (!items[i].section.empty()) {
                scope.Section(items[i].section, true, loc);
            }
            SidebarMenuItem it = items[i];
            it.active = (keel::as<keel::i32>(i) == activeIndex);
            Response itemRes = scope.Item(it, loc);
            if (itemRes.clicked && activeIndex != keel::as<keel::i32>(i)) {
                activeIndex = keel::as<keel::i32>(i);
                barRes.clicked = true;
                barRes.changed = true;
                ctx.InvalidateAll();
            }
            if (itemRes.hovered) {
                barRes.hovered = true;
            }
        }
    }
    return barRes;
}

Response detail::SidebarMenu::Item(
    Context& ctx,
    const SidebarMenuItem& item,
    const SidebarMenuOptions& menuOpts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = menuOpts.size;
    const bool effectiveDisabled = item.disabled || menuOpts.disabled || ctx.IsDisabled();

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
        const UiText effTooltip = !item.tooltip.empty() ? item.tooltip : (menuOpts.collapsed ? item.text : UiText{});
        if (!effTooltip.empty()) {
            ctx.SetTooltip(effTooltip, res.rect);
        }
    }

    Node* node = ctx.CreateNode(NodeType::SidebarItem, id);
    auto* data = ctx.AllocPayload<SidebarItemData>();
    data->item = item;
    data->item.text = ctx.AllocText(item.text);
    data->item.badge = ctx.AllocText(item.badge);
    data->item.section = ctx.AllocText(item.section);
    data->item.tooltip = ctx.AllocText(item.tooltip);
    data->item.icon = ctx.AllocIcon(item.icon);
    data->menuOpts = menuOpts;

    node->text = data->item.text.text;
    node->SetPayload(data);
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->active = item.active;
    node->focused = ctx.IsFocusVisible(id);
    node->widthMode = menuOpts.collapsed ? SizeMode::Fixed : SizeMode::Stretch;
    node->heightMode = SizeMode::Fit;
    if (menuOpts.collapsed) {
        node->fixedWidth = ctx.GetTheme().GetMetrics(activeSize).defaultHeight;
    }

    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(menuOpts.style),
        menuOpts.collapsed,
        menuOpts.border,
        keel::as<keel::u8>(item.variant != UiVariant::Default ? item.variant : menuOpts.variant),
        item.color.HasAny() ? item.color : menuOpts.color,
        item.active,
        effectiveDisabled,
        keel::as<keel::u8>(data->item.icon.GetType()),
        keel::as<keel::u8>(data->item.icon.GetId()),
        data->item.icon.GetPath(),
        data->item.text,
        data->item.badge
    );

    node->measureFn = [](const Node* n, keel::f32 /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<SidebarItemData>();
        if (!d) return {0.0f, 0.0f};
        return detail::SidebarMenu::MeasureItem(d->item, d->menuOpts, n->uiSize, canvas, theme);
    };

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<SidebarItemData>();
        if (!d) return;
        detail::SidebarMenu::RenderItem(canvas, n->computedRect, d->item, d->menuOpts, n->uiSize, n->hovered, n->pressed, n->disabled, theme, n->focused);
    };

    return res;
}

Vec2 detail::SidebarMenu::MeasureItem(
    const SidebarMenuItem& item,
    const SidebarMenuOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const ComponentMetrics& m = theme.GetMetrics(size);
    if (opts.collapsed) {
        return {m.defaultHeight, m.defaultHeight};
    }

    const keel::f32 fontSize = theme.ResolveFontSize(item.text, TextRole::LabelLarge, size);
    const keel::f32 indicatorPad = (opts.style == SidebarMenuStyle::Indicator) ? 4.0f : 0.0f;

    keel::f32 contentW = indicatorPad;
    if (item.icon.IsValid()) {
        contentW += m.iconSize;
        if (!item.text.empty()) {
            contentW += 8.0f;
        }
    }

    if (!item.text.empty() || !item.text.variants.empty()) {
        contentW += TextUtil::MeasureWidth(item.text, fontSize, canvas);
    }

    if (!item.badge.empty() || !item.badge.variants.empty()) {
        const keel::f32 badgeFont = theme.ResolveFontSize(item.badge, TextRole::LabelSmall, size);
        const keel::f32 badgeTextW = TextUtil::MeasureWidth(item.badge, badgeFont, canvas);
        contentW += 8.0f + badgeTextW + 10.0f;
    }

    return {contentW + m.paddingX * 2.0f, m.defaultHeight};
}

Vec2 detail::SidebarMenu::MeasureItem(
    const SidebarMenuItem& item,
    const SidebarMenuOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return MeasureItem(item, opts, size, canvas, defaultTheme);
}

void detail::SidebarMenu::RenderItem(
    ICanvas& canvas,
    Rect bounds,
    const SidebarMenuItem& item,
    const SidebarMenuOptions& opts,
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
    const UiVariant effVariant = (item.variant != UiVariant::Default) ? item.variant : opts.variant;
    const StateColor& effColor = item.color.HasAny() ? item.color : opts.color;
    const VariantColors& vDefault = theme.GetVariant(effVariant != UiVariant::Default ? effVariant : UiVariant::Primary);
    VariantColors v = vDefault;
    if (effVariant != UiVariant::Default) {
        const Color semSeed = theme.GetSemanticAccent(effVariant);
        v.bg = semSeed;
        v.bgHover = StateColor::BrightenColor(semSeed);
        v.bgActive = StateColor::DarkenColor(semSeed);
        v.text = Color::BestTextColor(semSeed, Color::Hex(0x0F172AFF), Color::Hex(0xFFFFFFFF));
    }
    if (effColor.HasAny()) {
        v.bg = theme.ResolveColor(effColor, false, false, false, v.bg);
        v.bgHover = theme.ResolveColor(effColor, true, false, false, StateColor::BrightenColor(v.bg));
        v.bgActive = theme.ResolveColor(effColor, false, true, false, StateColor::DarkenColor(v.bg));
        v.text = Color::BestTextColor(v.bg, Color::Hex(0x0F172AFF), Color::Hex(0xFFFFFFFF));
    }
    const keel::f32 radius = opts.border.radius.value_or(m.cornerRadius);
    const bool useSelectionPainter = (opts.style == SidebarMenuStyle::Indicator && !effColor.HasAny() && effVariant == UiVariant::Default);

    Color bg = Color::Transparent();
    Color borderCol = Color::Transparent();
    keel::f32 borderW = 0.0f;
    Color textCol = item.text.color.HasAny()
        ? theme.ResolveTextStateColor(item.text.color).Resolve(hovered, pressed || item.active, disabled, pal.textPrimary)
        : pal.textPrimary;
    Color iconCol = pal.textSecondary;

    canvas.PushClip(bounds);

    if (useSelectionPainter) {
        const bool solidHighlight = theme.Painter().DrawSelectionHighlight(
            canvas, theme, bounds, radius,
            hovered || pressed, item.active, false, false, disabled, Color::Transparent()
        );
        if (solidHighlight) {
            if (!item.text.color.HasAny()) textCol = pal.accentText;
            iconCol = pal.accentText;
        } else if (item.active) {
            iconCol = pal.accent;
        } else if (hovered || pressed) {
            iconCol = pal.textPrimary;
        }
        if (focused && !disabled) {
            theme.Painter().DrawFrame(canvas, theme, FrameKind::ToolbarButton, FramePaintParams{
                .rect = bounds,
                .radius = radius,
                .explicitRadius = opts.border.radius.has_value(),
                .bg = Color::Transparent(),
                .border = pal.focusRing,
                .borderWidth = 1.5f,
                .focused = true
            });
        }
    } else if (opts.style == SidebarMenuStyle::Pill) {
        if (item.active) {
            bg = pressed ? v.bgActive : (hovered ? v.bgHover : v.bg);
            textCol = item.text.color.HasAny()
                ? item.text.color.Resolve(hovered, true, disabled, v.text)
                : v.text;
            iconCol = textCol;
        } else if (pressed) {
            bg = pal.surfaceActive;
            iconCol = pal.textPrimary;
        } else if (hovered) {
            bg = pal.surfaceHover;
            iconCol = pal.textPrimary;
        }
        if (disabled) {
            bg = bg.WithAlphaF(theme.disabledOpacity);
        }
        if (item.active) {
            theme.Painter().DrawFrame(canvas, theme, FrameKind::Button, FramePaintParams{
                .rect = bounds,
                .radius = radius,
                .explicitRadius = opts.border.radius.has_value(),
                .bg = bg,
                .border = Color::Transparent(),
                .borderWidth = 0.0f,
                .hovered = hovered,
                .pressed = pressed,
                .active = true,
                .focused = focused && !disabled,
                .disabled = disabled,
                .accent = v.bg
            });
        } else if (bg.a > 0 || (focused && !disabled)) {
            theme.Painter().DrawFrame(canvas, theme, FrameKind::ToolbarButton, FramePaintParams{
                .rect = bounds,
                .radius = radius,
                .explicitRadius = opts.border.radius.has_value(),
                .bg = bg,
                .border = borderCol,
                .borderWidth = borderW,
                .hovered = hovered,
                .pressed = pressed,
                .focused = focused && !disabled,
                .disabled = disabled,
                .accent = v.bg
            });
        }
    } else { // SidebarMenuStyle::Indicator with custom effColor
        if (item.active) {
            bg = pressed ? v.bg.WithAlphaF(0.28f) : (hovered ? v.bg.WithAlphaF(0.22f) : v.bg.WithAlphaF(0.16f));
            textCol = item.text.color.HasAny()
                ? item.text.color.Resolve(hovered, true, disabled, pal.textPrimary)
                : pal.textPrimary;
            iconCol = v.bg;
        } else if (pressed) {
            bg = pal.surfaceActive;
            iconCol = pal.textPrimary;
        } else if (hovered) {
            bg = pal.surfaceHover;
            iconCol = pal.textPrimary;
        }
        if (disabled) {
            bg = bg.WithAlphaF(theme.disabledOpacity);
        }
        if (bg.a > 0 || (focused && !disabled)) {
            theme.Painter().DrawFrame(canvas, theme, FrameKind::ToolbarButton, FramePaintParams{
                .rect = bounds,
                .radius = radius,
                .explicitRadius = opts.border.radius.has_value(),
                .bg = bg,
                .border = borderCol,
                .borderWidth = borderW,
                .hovered = hovered,
                .pressed = pressed,
                .focused = focused && !disabled,
                .disabled = disabled,
                .accent = v.bg
            });
        }
        if (item.active) {
            const keel::f32 barW = 3.0f;
            const keel::f32 barPadY = keel::min(6.0f, bounds.h * 0.22f);
            const Rect barRect(bounds.x + 2.0f, bounds.y + barPadY, barW, keel::max(4.0f, bounds.h - barPadY * 2.0f));
            Color barCol = disabled ? v.bg.WithAlphaF(theme.disabledOpacity) : v.bg;
            canvas.DrawRoundRect(barRect, barW * 0.5f, barCol);
        }
    }

    if (disabled) {
        if (!item.text.color.HasAny()) {
            textCol = textCol.WithAlphaF(theme.disabledOpacity);
        }
        iconCol = iconCol.WithAlphaF(theme.disabledOpacity);
    }

    if (opts.collapsed) {
        if (item.icon.IsValid()) {
            const Rect iconRect(
                bounds.x + (bounds.w - m.iconSize) * 0.5f,
                bounds.y + (bounds.h - m.iconSize) * 0.5f,
                m.iconSize,
                m.iconSize
            );
            item.icon.Draw(canvas, iconRect, iconCol);
        } else if (!item.text.empty()) {
            const keel::f32 fontSize = theme.ResolveFontSize(item.text, TextRole::LabelLarge, size);
            canvas.DrawTextBox(item.text.text.substr(0, 1), bounds, fontSize, textCol, 0.5f, 0.5f);
        }
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
            Color badgeBg = disabled ? v.bg.WithAlphaF(theme.disabledOpacity) : v.bg;
            Color badgeTextCol = item.badge.color.HasAny()
                ? item.badge.color.Resolve(hovered, item.active, disabled, v.text)
                : (disabled ? v.text.WithAlphaF(theme.disabledOpacity) : v.text);
            theme.Painter().DrawBadge(canvas, theme, badgeRect, badgeBg, pal.surface, 1.5f, badgeH * 0.5f);
            canvas.DrawTextBox(item.badge.text, badgeRect, badgeFont, badgeTextCol, 0.5f, 0.5f);
        }
        canvas.PopClip();
        return;
    }

    const keel::f32 indicatorPad = (opts.style == SidebarMenuStyle::Indicator) ? 4.0f : 0.0f;
    keel::f32 leftX = bounds.x + m.paddingX + indicatorPad;
    keel::f32 rightX = bounds.Right() - m.paddingX;

    // 1. Leading Icon
    if (item.icon.IsValid()) {
        const Rect iconRect(
            leftX,
            bounds.y + (bounds.h - m.iconSize) * 0.5f,
            m.iconSize,
            m.iconSize
        );
        item.icon.Draw(canvas, iconRect, iconCol);
        leftX += m.iconSize + 8.0f;
    }

    // 2. Trailing Badge
    if (!item.badge.empty()) {
        const keel::f32 badgeFont = theme.ResolveFontSize(item.badge, TextRole::LabelSmall, size);
        const Vec2 badgeTextDims = canvas.MeasureText(item.badge.text, badgeFont);
        const keel::f32 badgeW = badgeTextDims.x + 10.0f;
        const keel::f32 badgeH = keel::min(bounds.h - 6.0f, badgeFont + 6.0f);
        const Rect badgeRect(
            rightX - badgeW,
            bounds.y + (bounds.h - badgeH) * 0.5f,
            badgeW,
            badgeH
        );
        Color badgeBg = item.active && opts.style == SidebarMenuStyle::Pill
            ? Color(255, 255, 255, 45)
            : pal.surfaceActive;
        const Color defaultBadgeTextCol = (item.active && opts.style == SidebarMenuStyle::Pill) ? textCol : pal.textSecondary;
        Color badgeTextCol = item.badge.color.HasAny()
            ? item.badge.color.Resolve(hovered, item.active, disabled, defaultBadgeTextCol)
            : (disabled ? defaultBadgeTextCol.WithAlphaF(theme.disabledOpacity) : defaultBadgeTextCol);
        if (disabled) {
            badgeBg = badgeBg.WithAlphaF(theme.disabledOpacity);
        }
        theme.Painter().DrawBadge(canvas, theme, badgeRect, badgeBg, Color::Transparent(), 0.0f, badgeH * 0.5f);
        canvas.DrawTextBox(item.badge.text, badgeRect, badgeFont, badgeTextCol, 0.5f, 0.5f);
        rightX = badgeRect.x - 6.0f;
    }

    // 3. Item Text
    if (!item.text.empty() && rightX > leftX) {
        const keel::f32 fontSize = theme.ResolveFontSize(item.text, TextRole::LabelLarge, size);
        const Rect textRect(leftX, bounds.y, rightX - leftX, bounds.h);
        canvas.PushClip(textRect);
        theme.Painter().DrawControlText(canvas, theme, item.text.text, textRect, fontSize, textCol, disabled, 0.0f, 0.5f);
        canvas.PopClip();
    }

    canvas.PopClip();
}

} // namespace guikit
