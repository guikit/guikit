#include "guikit/panels/Toolbar.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

void ToolbarScope::Separator(float heightRatio, std::source_location loc) {
    if (!m_ctx || !m_node) return;
    const Id id = m_ctx->MakeChildId(loc);
    const float scale = m_ctx->GetCurrentScale();
    Node* sep = m_ctx->CreateNode(NodeType::Separator, id);
    sep->widthMode = SizeMode::Fixed;
    sep->fixedWidth = 10.0f * scale; // 10px slot with centered 1px line
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
        Toolbar::RenderSeparator(canvas, n->computedRect, n->borderColor, theme);
    };
}

void ToolbarScope::Spacer(std::source_location loc) {
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

Response ToolbarScope::ToolButton(
    ToolButtonOptions opts,
    std::source_location loc
) {
    if (!m_ctx) return Response{};
    return Toolbar::ToolButton(*m_ctx, opts, loc);
}

ToolbarScope Toolbar::Open(
    Context& ctx,
    ToolbarOptions opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const UiTheme& theme = ctx.GetTheme();
    const ComponentMetrics& m = theme.GetMetrics(activeSize);
    const float barH = m.defaultHeight + 8.0f * activeScale;

    Node* node = ctx.CreateNode(NodeType::Container, id);
    node->theme = pushedTheme;
    node->direction = FlexDirection::Row;
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
                toolbarPadding = Padding::Symmetric(g, g * 0.5f);
            }
        } else {
            toolbarPadding = theme.ResolvePadding(opts.padding);
        }
    } else {
        const float g = theme.GetGap(activeSize);
        toolbarPadding = Padding::Symmetric(g, g * 0.5f);
    }
    node->padding = toolbarPadding;

    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fixed;
    node->fixedHeight = barH;
    const auto& pal = theme.palette;
    node->backgroundColor = opts.backgroundColor.a > 0 ? opts.backgroundColor : pal.surface;
    node->borderColor = opts.borderColor.a > 0 ? opts.borderColor : pal.border;
    node->borderWidth = (opts.borderColor.a > 0) ? theme.Scale(1.0f) : 0.0f;
    node->cornerRadius = theme.ResolveRadius(opts.radius, activeSize);
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;

    ctx.PushScale(activeScale);
    ctx.PushSize(activeSize);
    ctx.PushDisabled(effectiveDisabled);
    ctx.PushContainer(node);

    return ToolbarScope(&ctx, node);
}

Response Toolbar::ToolButton(
    Context& ctx,
    const ToolButtonOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
    }

    Node* node = ctx.CreateNode(NodeType::Button, id);

    ButtonOptions bOpts;
    bOpts.label = ctx.AllocText(opts.label);
    node->text = bOpts.label.text;
    bOpts.icon = opts.icon;
    bOpts.variant = opts.variant;
    bOpts.iconPlacement = opts.iconPlacement;
    bOpts.controlAlign = opts.controlAlign;
    bOpts.stretch = opts.stretch;
    bOpts.width = opts.width;
    bOpts.minWidth = opts.minWidth;
    bOpts.disabled = effectiveDisabled;
    bOpts.active = opts.active;
    bOpts.size = activeSize;

    node->payload = ctx.AllocPayload<ButtonOptions>(bOpts);
    node->visualHash = detail::HashArgs(
        opts.variant,
        opts.iconPlacement,
        opts.active,
        static_cast<uint8_t>(opts.stretch),
        opts.size.value_or(activeSize),
        static_cast<uint8_t>(opts.icon.GetType()),
        static_cast<uint8_t>(opts.icon.GetId()),
        opts.icon.GetPath(),
        bOpts.label
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* b = n->As<ButtonOptions>();
        return Button::Measure(b ? *b : ButtonOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* b = n->As<ButtonOptions>();
        Button::Render(canvas, n->computedRect, b ? *b : ButtonOptions{}, n->uiSize, n->hovered, n->pressed, n->disabled, theme);
    };
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

void Toolbar::RenderSeparator(
    ICanvas& canvas,
    Rect bounds,
    Color color,
    const UiTheme& theme
) {
    const Color stroke = color.a > 0 ? color : theme.palette.borderSubtle;
    const float midX = bounds.x + bounds.w * 0.5f;
    const float padY = std::max(2.0f, bounds.h * 0.20f);
    canvas.DrawLine(
        Vec2(midX, bounds.y + padY),
        Vec2(midX, bounds.Bottom() - padY),
        stroke,
        1.0f
    );
}

} // namespace guikit
