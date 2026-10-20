#include "guikit/widgets/SidebarMenu.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"
#include "guikit/widgets/Heading.hpp"

namespace guikit {

void SidebarMenuScope::InvalidateContext() noexcept {
    if (m_ctx) {
        m_ctx->InvalidateAll();
    }
}

void SidebarMenuScope::Section(
    UiText title,
    bool divider,
    std::source_location loc
) {
    if (!m_ctx || !m_node) return;

    const float scale = m_ctx->GetCurrentScale();
    if (m_node->childCount > 0 && m_opts.sectionGap > 0.0f) {
        const Id gapId = m_ctx->MakeChildId(loc);
        Node* gapNode = m_ctx->CreateNode(NodeType::Box, gapId);
        gapNode->widthMode = SizeMode::Stretch;
        gapNode->heightMode = SizeMode::Fixed;
        gapNode->fixedHeight = m_opts.sectionGap * scale;
    }

    if (m_opts.collapsed) {
        if (divider) {
            Separator(loc);
        }
        return;
    }

    m_ctx->PushSize(UiSize::Xs);
    Heading::Run(*m_ctx, HeadingOptions{
        .label = title,
        .divider = divider
    }, loc);
    m_ctx->PopSize();
}

Response SidebarMenuScope::Item(
    const SidebarMenuItem& item,
    std::source_location loc
) {
    if (!m_ctx) return Response{};
    return SidebarMenu::Item(*m_ctx, item, m_opts, loc);
}

void SidebarMenuScope::Separator(std::source_location loc) {
    if (!m_ctx || !m_node) return;
    const Id id = m_ctx->MakeChildId(loc);
    const float scale = m_ctx->GetCurrentScale();
    Node* sep = m_ctx->CreateNode(NodeType::Separator, id);
    sep->widthMode = SizeMode::Stretch;
    sep->heightMode = SizeMode::Fixed;
    sep->fixedHeight = 8.0f * scale;
    sep->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* /*canvas*/, const UiTheme& /*theme*/) noexcept -> Vec2 {
        return {0.0f, n->fixedHeight > 0.0f ? n->fixedHeight : 8.0f};
    };
    sep->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const float y = std::floor(n->computedRect.Center().y) + 0.5f;
        canvas.DrawLine(
            {n->computedRect.x + 2.0f, y},
            {n->computedRect.Right() - 2.0f, y},
            theme.palette.borderSubtle,
            1.0f
        );
    };
}

void SidebarMenuScope::Spacer(float flex, std::source_location loc) {
    if (!m_ctx) return;
    const Id id = m_ctx->MakeChildId(loc);
    Node* spacer = m_ctx->CreateNode(NodeType::Spacer, id);
    spacer->widthMode = SizeMode::Stretch;
    spacer->heightMode = SizeMode::Stretch;
    spacer->isSpacer = true;
    spacer->flexGrow = flex;
    spacer->measureFn = [](const Node*, float /*availableWidth*/, ICanvas*, const UiTheme&) noexcept -> Vec2 {
        return {0.0f, 0.0f};
    };
}

SidebarMenuScope SidebarMenu::Open(
    Context& ctx,
    const SidebarMenuOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const UiTheme& theme = ctx.GetTheme();
    Node* node = ctx.CreateNode(NodeType::Column, id);
    node->theme = pushedTheme;
    node->direction = FlexDirection::Column;
    node->align = Alignment::Stretch;
    node->justify = Justify::Start;
    node->gap = opts.gap.has_value() ? theme.Scale(*opts.gap) : (4.0f * activeScale);

    SidebarMenuOptions effectiveOpts = opts;
    for (Node* p = node->parent; p != nullptr; p = p->parent) {
        if (p->type == NodeType::Sidebar) {
            if (p->sidebarCompact) {
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
        node->fixedWidth = theme.Scale(*effectiveOpts.width);
    } else {
        node->widthMode = SizeMode::Stretch;
    }
    node->heightMode = SizeMode::Fit;
    node->uiSize = activeSize;
    node->disabled = activeDisabled;

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(node);

    return SidebarMenuScope(&ctx, node, effectiveOpts);
}

Response SidebarMenu::Run(
    Context& ctx,
    int& activeIndex,
    std::span<const SidebarMenuItem> items,
    const SidebarMenuOptions& opts,
    std::source_location loc
) {
    Response barRes{};
    if (auto scope = Open(ctx, opts, loc)) {
        barRes.id = scope.GetNode() ? scope.GetNode()->id : Id::Null();
        for (size_t i = 0; i < items.size(); ++i) {
            if (!items[i].section.empty()) {
                scope.Section(items[i].section, true, loc);
            }
            SidebarMenuItem it = items[i];
            it.active = (static_cast<int>(i) == activeIndex);
            Response itemRes = scope.Item(it, loc);
            if (itemRes.clicked && activeIndex != static_cast<int>(i)) {
                activeIndex = static_cast<int>(i);
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

Response SidebarMenu::Item(
    Context& ctx,
    const SidebarMenuItem& item,
    const SidebarMenuOptions& menuOpts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
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
    data->menuOpts = menuOpts;

    node->text = data->item.text.text;
    node->SetPayload(data);
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->active = item.active;
    node->focused = ctx.IsFocused(id);
    node->widthMode = menuOpts.collapsed ? SizeMode::Fixed : SizeMode::Stretch;
    node->heightMode = SizeMode::Fit;
    if (menuOpts.collapsed) {
        node->fixedWidth = ctx.GetTheme().GetMetrics(activeSize).defaultHeight;
    }

    node->visualHash = detail::HashArgs(
        static_cast<uint8_t>(menuOpts.style),
        menuOpts.collapsed,
        menuOpts.border,
        item.color.HasAny() ? item.color : menuOpts.color,
        item.active,
        effectiveDisabled,
        static_cast<uint8_t>(item.icon.GetType()),
        static_cast<uint8_t>(item.icon.GetId()),
        item.icon.GetPath(),
        data->item.text,
        data->item.badge
    );

    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<SidebarItemData>();
        if (!d) return {0.0f, 0.0f};
        return SidebarMenu::MeasureItem(d->item, d->menuOpts, n->uiSize, canvas, theme);
    };

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<SidebarItemData>();
        if (!d) return;
        SidebarMenu::RenderItem(canvas, n->computedRect, d->item, d->menuOpts, n->uiSize, n->hovered, n->pressed, n->disabled, theme, n->focused);
    };

    return res;
}

Vec2 SidebarMenu::MeasureItem(
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

    const float fontSize = theme.ResolveFontSize(item.text, TextRole::LabelLarge, size);
    const float indicatorPad = (opts.style == SidebarMenuStyle::Indicator) ? 4.0f : 0.0f;

    float contentW = indicatorPad;
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
        const float badgeFont = theme.ResolveFontSize(item.badge, TextRole::LabelSmall, size);
        const float badgeTextW = TextUtil::MeasureWidth(item.badge, badgeFont, canvas);
        contentW += 8.0f + badgeTextW + 10.0f;
    }

    return {contentW + m.paddingX * 2.0f, m.defaultHeight};
}

Vec2 SidebarMenu::MeasureItem(
    const SidebarMenuItem& item,
    const SidebarMenuOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return MeasureItem(item, opts, size, canvas, defaultTheme);
}

void SidebarMenu::RenderItem(
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
    const StateColor& effColor = item.color.HasAny() ? item.color : opts.color;
    const VariantColors& vDefault = theme.GetVariant(UiVariant::Primary);
    VariantColors v = vDefault;
    if (effColor.HasAny()) {
        v.bg = effColor.Resolve(false, false, false, vDefault.bg);
        v.bgHover = effColor.hover.has_value() ? *effColor.hover : v.bg.WithAlphaF(0.85f);
        v.bgActive = effColor.active.has_value() ? *effColor.active : v.bg.WithAlphaF(0.75f);
    }
    const float radius = opts.border.radius.value_or(m.cornerRadius);

    Color bg = Color::Transparent();
    Color borderCol = Color::Transparent();
    float borderW = 0.0f;
    Color textCol = item.text.color.HasAny()
        ? item.text.color.Resolve(hovered, pressed || item.active, disabled, pal.textPrimary)
        : pal.textPrimary;
    Color iconCol = pal.textSecondary;

    if (opts.style == SidebarMenuStyle::Pill) {
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
    } else { // SidebarMenuStyle::Indicator
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
    }

    if (disabled) {
        bg = bg.WithAlphaF(0.40f);
        if (!item.text.color.HasAny()) {
            textCol = textCol.WithAlphaF(0.40f);
        }
        iconCol = iconCol.WithAlphaF(0.40f);
    } else if (focused) {
        borderCol = pal.focusRing;
        borderW = 1.5f;
    }

    canvas.PushClip(bounds);

    if (bg.a > 0 || borderW > 0.0f) {
        canvas.DrawRoundRect(bounds, radius, bg, borderCol, borderW);
    }

    if (opts.style == SidebarMenuStyle::Indicator && item.active) {
        const float barW = 3.0f;
        const float barPadY = std::min(6.0f, bounds.h * 0.22f);
        const Rect barRect(bounds.x + 2.0f, bounds.y + barPadY, barW, std::max(4.0f, bounds.h - barPadY * 2.0f));
        Color barCol = disabled ? v.bg.WithAlphaF(0.40f) : v.bg;
        canvas.DrawRoundRect(barRect, barW * 0.5f, barCol);
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
            const float fontSize = theme.ResolveFontSize(item.text, TextRole::LabelLarge, size);
            canvas.DrawTextBox(item.text.text.substr(0, 1), bounds, fontSize, textCol, 0.5f, 0.5f);
        }
        if (!item.badge.empty()) {
            const float badgeFont = std::max(9.0f, theme.ResolveFontSize(item.badge, TextRole::LabelSmall, size) - 1.5f);
            const Vec2 badgeDims = canvas.MeasureText(item.badge.text, badgeFont);
            const float badgeH = std::min(bounds.h * 0.48f, std::max(13.0f, badgeFont + 3.0f));
            const float badgeW = std::min(bounds.w - 4.0f, std::max(badgeH, badgeDims.x + 6.0f));
            const Rect badgeRect(
                bounds.Right() - badgeW - 2.0f,
                bounds.Bottom() - badgeH - 2.0f,
                badgeW,
                badgeH
            );
            Color badgeBg = disabled ? v.bg.WithAlphaF(0.45f) : v.bg;
            Color badgeTextCol = item.badge.color.HasAny()
                ? item.badge.color.Resolve(hovered, item.active, disabled, v.text)
                : (disabled ? v.text.WithAlphaF(0.55f) : v.text);
            canvas.DrawRoundRect(badgeRect, badgeH * 0.5f, badgeBg, pal.surface, 1.5f);
            canvas.DrawTextBox(item.badge.text, badgeRect, badgeFont, badgeTextCol, 0.5f, 0.5f);
        }
        canvas.PopClip();
        return;
    }

    const float indicatorPad = (opts.style == SidebarMenuStyle::Indicator) ? 4.0f : 0.0f;
    float leftX = bounds.x + m.paddingX + indicatorPad;
    float rightX = bounds.Right() - m.paddingX;

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
        const float badgeFont = theme.ResolveFontSize(item.badge, TextRole::LabelSmall, size);
        const Vec2 badgeTextDims = canvas.MeasureText(item.badge.text, badgeFont);
        const float badgeW = badgeTextDims.x + 10.0f;
        const float badgeH = std::min(bounds.h - 6.0f, badgeFont + 6.0f);
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
            : (disabled ? defaultBadgeTextCol.WithAlphaF(0.40f) : defaultBadgeTextCol);
        if (disabled) {
            badgeBg = badgeBg.WithAlphaF(0.40f);
        }
        canvas.DrawRoundRect(badgeRect, badgeH * 0.5f, badgeBg);
        canvas.DrawTextBox(item.badge.text, badgeRect, badgeFont, badgeTextCol, 0.5f, 0.5f);
        rightX = badgeRect.x - 6.0f;
    }

    // 3. Item Text
    if (!item.text.empty() && rightX > leftX) {
        const float fontSize = theme.ResolveFontSize(item.text, TextRole::LabelLarge, size);
        const Rect textRect(leftX, bounds.y, rightX - leftX, bounds.h);
        canvas.PushClip(textRect);
        canvas.DrawTextBox(item.text.text, textRect, fontSize, textCol, 0.0f, 0.5f);
        canvas.PopClip();
    }

    canvas.PopClip();
}

} // namespace guikit
