#include "guikit/panels/Toolbar.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

namespace {
struct ToolButtonPayload {
    ButtonOptions btn{};
    UiText badge{};
    UiVariant variant{UiVariant::Ghost};
};
} // namespace

void ToolbarScope::InvalidateContext() noexcept {
    if (m_ctx) {
        m_ctx->InvalidateAll();
    }
}

void ToolbarScope::Separator(float heightRatio, std::source_location loc) {
    if (!m_ctx || !m_node) return;
    const Id id = m_ctx->MakeChildId(loc);
    const float scale = m_ctx->GetCurrentScale();
    const bool isVerticalBar = (m_opts.orientation == Orientation::Vertical);

    Node* sep = m_ctx->CreateNode(NodeType::Separator, id);
    if (isVerticalBar) {
        sep->widthMode = SizeMode::Stretch;
        sep->heightMode = SizeMode::Fixed;
        sep->fixedHeight = 8.0f * scale;
        sep->borderColor = m_node->borderColor;
        sep->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* /*canvas*/, const UiTheme& /*theme*/) noexcept -> Vec2 {
            return {20.0f, n->fixedHeight > 0.0f ? n->fixedHeight : 8.0f};
        };
        sep->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
            Toolbar::RenderSeparator(canvas, n->computedRect, n->borderColor, theme, Orientation::Vertical);
        };
    } else {
        sep->widthMode = SizeMode::Fixed;
        sep->fixedWidth = 10.0f * scale;
        sep->heightMode = SizeMode::Fixed;
        const float parentH = (m_node->fixedHeight > 0.0f) ? m_node->fixedHeight : (32.0f * scale);
        sep->fixedHeight = parentH * std::clamp(heightRatio, 0.1f, 1.0f);
        sep->borderColor = m_node->borderColor;
        sep->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* /*canvas*/, const UiTheme& /*theme*/) noexcept -> Vec2 {
            return {
                n->fixedWidth > 0.0f ? n->fixedWidth : 10.0f,
                n->fixedHeight > 0.0f ? n->fixedHeight : 20.0f
            };
        };
        sep->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
            Toolbar::RenderSeparator(canvas, n->computedRect, n->borderColor, theme, Orientation::Horizontal);
        };
    }
}

void ToolbarScope::Spacer(std::source_location loc) {
    if (!m_ctx) return;
    const Id id = m_ctx->MakeChildId(loc);
    Node* spacer = m_ctx->CreateNode(NodeType::Spacer, id);
    if (m_opts.orientation == Orientation::Vertical) {
        spacer->widthMode = SizeMode::Fit;
        spacer->heightMode = SizeMode::Stretch;
    } else {
        spacer->widthMode = SizeMode::Stretch;
    }
    spacer->isSpacer = true;
    spacer->flexGrow = 1.0f;
    spacer->measureFn = [](const Node*, float /*availableWidth*/, ICanvas*, const UiTheme&) noexcept -> Vec2 {
        return {0.0f, 0.0f};
    };
}

Response ToolbarScope::ToolButton(
    ToolButtonOptions opts,
    std::source_location loc
) {
    if (!m_ctx) return Response{};
    return Toolbar::ToolButton(*m_ctx, opts, loc);
}

Response ToolbarScope::Toggle(
    bool& active,
    ToolButtonOptions opts,
    std::source_location loc
) {
    if (!m_ctx) return Response{};
    opts.active = active;
    if (active && opts.variant == UiVariant::Ghost) {
        opts.variant = UiVariant::Primary;
    } else if (!active) {
        opts.variant = UiVariant::Ghost;
    }
    Response res = Toolbar::ToolButton(*m_ctx, opts, loc);
    if (res.clicked) {
        active = !active;
        res.changed = true;
        m_ctx->InvalidateAll();
    }
    return res;
}

ContextMenuResponse ToolbarScope::Dropdown(
    ToolButtonOptions opts,
    std::span<const ContextMenuItem> menuItems,
    ContextMenuOptions menuOpts,
    std::source_location loc
) {
    if (!m_ctx) return ContextMenuResponse{};
    if (!opts.icon.IsValid() && !opts.square) {
        opts.icon = Icon::ChevronDown();
        opts.iconPlacement = Placement::Trailing;
    }
    const Response btnRes = Toolbar::ToolButton(*m_ctx, opts, loc);
    menuOpts.trigger = ContextMenuTrigger::LeftClick;
    return ContextMenu::Run(*m_ctx, nullptr, std::nullopt, btnRes.rect, menuItems, menuOpts, loc);
}

ToolbarScope Toolbar::Open(
    Context& ctx,
    ToolbarOptions opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const UiTheme& theme = ctx.GetTheme();
    const ComponentMetrics& m = theme.GetMetrics(activeSize);
    const float barExtent = m.defaultHeight + 8.0f * activeScale;
    const bool isVertical = (opts.orientation == Orientation::Vertical);

    Response res{};
    res.id = id;
    res.rect = ctx.GetPrevUnclippedBounds(id);
    res.ctx = &ctx;

    Node* node = ctx.CreateNode(NodeType::Toolbar, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->direction = isVertical ? FlexDirection::Column : FlexDirection::Row;
    node->align = Alignment::Center;
    node->gap = theme.ResolveGap(opts.gap, activeSize);

    Padding toolbarPadding;
    if (opts.padding.has_value()) {
        if (opts.padding->IsToken()) {
            const UiSize t = opts.padding->GetToken();
            if (t == UiSize::None) {
                toolbarPadding = Padding{};
            } else {
                const float g = theme.GetGap(t);
                toolbarPadding = isVertical
                    ? Padding::Symmetric(g * 0.5f, g)
                    : Padding::Symmetric(g, g * 0.5f);
            }
        } else {
            toolbarPadding = theme.ResolvePadding(opts.padding);
        }
    } else {
        const float g = theme.GetGap(activeSize);
        toolbarPadding = isVertical
            ? Padding::Symmetric(g * 0.5f, g)
            : Padding::Symmetric(g, g * 0.5f);
    }
    node->padding = toolbarPadding;

    if (isVertical) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = barExtent;
        node->heightMode = (opts.style == ToolbarStyle::Floating) ? SizeMode::Fit : SizeMode::Stretch;
    } else {
        node->widthMode = (opts.style == ToolbarStyle::Floating) ? SizeMode::Fit : SizeMode::Stretch;
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = barExtent;
    }

    const auto& pal = theme.palette;
    if (opts.backgroundColor.a > 0) {
        node->backgroundColor = opts.backgroundColor;
    } else if (opts.style == ToolbarStyle::Floating) {
        node->backgroundColor = pal.surfaceElevated;
    } else {
        node->backgroundColor = pal.surface;
    }

    if (opts.borderColor.a > 0) {
        node->borderColor = opts.borderColor;
        node->borderWidth = theme.Scale(1.0f);
    } else if (opts.style == ToolbarStyle::Floating || opts.style == ToolbarStyle::Header) {
        node->borderColor = pal.border;
        node->borderWidth = theme.Scale(1.0f);
    } else {
        node->borderColor = pal.border;
        node->borderWidth = 0.0f;
    }

    if (opts.borderRadius.has_value()) {
        node->cornerRadius = theme.Scale(*opts.borderRadius);
    } else if (opts.style == ToolbarStyle::Header) {
        node->cornerRadius = 0.0f;
    } else if (opts.style == ToolbarStyle::Floating) {
        node->cornerRadius = theme.ResolveRadius(std::nullopt, activeSize) + 2.0f * activeScale;
    } else {
        node->cornerRadius = theme.ResolveRadius(std::nullopt, activeSize);
    }

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->visualHash = detail::HashArgs(
        static_cast<uint8_t>(opts.style),
        static_cast<uint8_t>(opts.orientation),
        node->cornerRadius,
        node->borderWidth
    );

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(effectiveDisabled);
    ctx.PushContainer(node);

    return ToolbarScope(&ctx, node, res, opts);
}

Response Toolbar::ToolButton(
    Context& ctx,
    const ToolButtonOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize activeSize = ctx.GetCurrentSize();

    ctx.RegisterFocusable(id, effectiveDisabled);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
    } else {
        if (res.clicked) {
            ctx.RequestFocus(id);
        }
        if (ctx.IsFocused(id) && (ctx.ConsumeKey(Key::Space) || ctx.ConsumeKey(Key::Enter))) {
            res.clicked = true;
        }
    }

    if (res.hovered) {
        const UiText effTooltip = !opts.tooltip.empty()
            ? opts.tooltip
            : (opts.square && !opts.label.empty() ? opts.label : UiText{});
        if (!effTooltip.empty()) {
            ctx.SetTooltip(effTooltip, res.rect);
        }
    }

    Node* node = ctx.CreateNode(NodeType::Button, id);

    auto* payload = ctx.AllocPayload<ToolButtonPayload>();
    payload->btn.text = opts.square ? UiText{} : ctx.AllocText(opts.label);
    payload->btn.tooltip = ctx.AllocText(opts.tooltip);
    node->text = payload->btn.text.text;
    payload->btn.icon = opts.icon;
    payload->btn.color = StateColor::FromVariant(opts.variant);
    payload->btn.iconPlacement = opts.iconPlacement;
    payload->btn.align = opts.controlAlign;
    payload->btn.stretch = opts.stretch;
    payload->btn.width = opts.width;
    payload->btn.minWidth = opts.minWidth;
    payload->btn.disabled = effectiveDisabled;
    payload->btn.active = opts.active;
    payload->btn.square = opts.square;
    payload->btn.iconColor = opts.iconColor;
    payload->badge = ctx.AllocText(opts.badge);
    payload->variant = (opts.variant == UiVariant::Ghost) ? UiVariant::Primary : opts.variant;

    node->SetPayload(payload);
    node->visualHash = detail::HashArgs(
        opts.variant,
        opts.iconPlacement,
        opts.active,
        opts.square,
        opts.iconColor.value_or(Color::Transparent()),
        static_cast<uint8_t>(opts.stretch),
        activeSize,
        static_cast<uint8_t>(opts.icon.GetType()),
        static_cast<uint8_t>(opts.icon.GetId()),
        opts.icon.GetPath(),
        payload->btn.text,
        payload->badge
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* p = n->As<ToolButtonPayload>();
        return Button::Measure(p ? p->btn : ButtonOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* p = n->As<ToolButtonPayload>();
        Button::Render(canvas, n->computedRect, p ? p->btn : ButtonOptions{}, n->uiSize, n->hovered, n->pressed, n->disabled, theme, n->focused);
        if (p && !p->badge.empty()) {
            const auto& pal = theme.palette;
            const VariantColors& v = theme.GetVariant(p->variant);
            const float badgeFont = std::max(9.0f, theme.ResolveFontSize(p->badge, TextRole::LabelSmall, n->uiSize) - 1.5f);
            const Vec2 badgeDims = canvas.MeasureText(p->badge.text, badgeFont);
            const float badgeH = std::min(n->computedRect.h * 0.48f, std::max(13.0f, badgeFont + 3.0f));
            const float badgeW = std::min(n->computedRect.w - 4.0f, std::max(badgeH, badgeDims.x + 6.0f));
            const Rect badgeRect(
                n->computedRect.Right() - badgeW - 2.0f,
                n->computedRect.y + 2.0f,
                badgeW,
                badgeH
            );
            const Color badgeBg = n->disabled ? v.bg.WithAlphaF(0.45f) : v.bg;
            const Color badgeTextCol = n->disabled ? v.text.WithAlphaF(0.55f) : v.text;
            canvas.DrawRoundRect(badgeRect, badgeH * 0.5f, badgeBg, pal.surface, 1.5f);
            canvas.DrawTextBox(p->badge.text, badgeRect, badgeFont, badgeTextCol, 0.5f, 0.5f);
        }
    };
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->focused = ctx.IsFocused(id);
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

void Toolbar::RenderSeparator(
    ICanvas& canvas,
    Rect bounds,
    Color color,
    const UiTheme& theme,
    Orientation orientation
) {
    const Color stroke = color.a > 0 ? color : theme.palette.borderSubtle;
    if (orientation == Orientation::Vertical) {
        const float midY = std::floor(bounds.Center().y) + 0.5f;
        const float padX = std::max(2.0f, bounds.w * 0.18f);
        canvas.DrawLine(
            Vec2(bounds.x + padX, midY),
            Vec2(bounds.Right() - padX, midY),
            stroke,
            1.0f
        );
    } else {
        const float midX = std::floor(bounds.Center().x) + 0.5f;
        const float padY = std::max(2.0f, bounds.h * 0.20f);
        canvas.DrawLine(
            Vec2(midX, bounds.y + padY),
            Vec2(midX, bounds.Bottom() - padY),
            stroke,
            1.0f
        );
    }
}

} // namespace guikit
