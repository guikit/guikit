#include "guikit/widgets/heading.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/layout/separator.hpp"

namespace guikit {

ElementSpec Heading::MakeSpec(
    const HeadingOptions& opts,
    UiSize activeSize,
    bool isDisabled,
    const UiTheme& theme,
    bool isHovered,
    bool isActive
) noexcept {
    const HeadingMetrics& m = theme.GetHeading(opts.level, activeSize);
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .icon = opts.icon,
        .badge = opts.badge,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .align = opts.align,
        .stretch = opts.stretch,
        .width = opts.width,
        .widthRatio = opts.widthRatio,
        .background = opts.background,
        .border = opts.border,
        .widgetSize = activeSize,
        .headingLevel = opts.level,
        .labelGap = m.lineGap,
        .descGap = m.lineGap,
        .inlineGap = m.gap * 1.5f,
        .hasControl = false,
        .headingTypography = true,
        .stretchableControl = false,
        .stretchHeightWhenStandalone = false,
        .disabled = isDisabled,
        .hovered = isHovered,
        .active = isActive
    };
}

float Heading::ComputeBottomSeparatorSpace(
    const HeadingOptions& opts,
    UiSize size,
    const UiTheme& theme
) noexcept {
    if (opts.separator.IsEnabled() && opts.separator.placement == HeadingSeparator::Bottom) {
        const HeadingMetrics& m = theme.GetHeading(opts.level, size);
        const float divPad = std::max(4.0f, m.gap * 0.75f);
        const float strokeW = theme.Scale(opts.separator.width);
        return divPad + strokeW + divPad * 0.5f;
    }
    return 0.0f;
}

Response Heading::Run(
    Context& ctx,
    const HeadingOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.doubleClicked = false;
        res.rightClicked = false;
    }

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    Node* node = ctx.CreateNode(NodeType::Heading, id);
    node->theme = pushedTheme;
    auto* hOpts = ctx.AllocPayload<HeadingOptions>(opts);
    hOpts->label = ctx.AllocText(opts.label);
    hOpts->description = ctx.AllocText(opts.description);
    hOpts->badge = ctx.AllocText(opts.badge);
    hOpts->tooltip = ctx.AllocText(opts.tooltip);
    if (!opts.icon.GetPath().empty()) {
        hOpts->icon = Icon::File(ctx.AllocString(opts.icon.GetPath()));
    }
    node->text = hOpts->label.text;
    node->SetPayload(hOpts);
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;
    node->visualHash = detail::HashArgs(
        static_cast<uint8_t>(opts.level),
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.align),
        static_cast<uint8_t>(opts.stretch),
        opts.width.value_or(0.0f),
        opts.widthRatio.value_or(-1.0f),
        opts.background,
        opts.border,
        hOpts->label,
        hOpts->description,
        hOpts->badge,
        opts.separator,
        effectiveDisabled,
        static_cast<uint8_t>(opts.icon.GetType()),
        static_cast<uint8_t>(opts.icon.GetId()),
        hOpts->icon.GetPath()
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* o = n->As<HeadingOptions>();
        return Heading::Measure(o ? *o : HeadingOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* o = n->As<HeadingOptions>();
        Heading::Render(
            canvas,
            n->computedRect,
            o ? *o : HeadingOptions{},
            n->uiSize,
            theme,
            n->disabled,
            n->hovered,
            n->pressed || n->focused
        );
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);
    if (pushedTheme) {
        ctx.PopTheme();
    }

    return res;
}

Vec2 Heading::Measure(
    const HeadingOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) noexcept {
    const ElementSpec spec = MakeSpec(opts, size, opts.disabled, theme);
    Vec2 result = Element::Measure({0.0f, 0.0f}, spec, canvas, theme, availableWidth);
    result.y += ComputeBottomSeparatorSpace(opts, size, theme);
    return result;
}

void Heading::Render(
    ICanvas& canvas,
    Rect bounds,
    const HeadingOptions& opts,
    UiSize size,
    const UiTheme& theme,
    bool disabled,
    bool hovered,
    bool active
) noexcept {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const bool effDisabled = disabled || opts.disabled;
    const ElementSpec spec = MakeSpec(opts, size, effDisabled, theme, hovered, active);
    const float bottomSepSpace = ComputeBottomSeparatorSpace(opts, size, theme);

    Rect resolveBounds = bounds;
    if (bottomSepSpace > 0.0f) {
        resolveBounds.h = std::max(0.0f, resolveBounds.h - bottomSepSpace);
    }

    ElementGeometry geom = Element::Resolve(resolveBounds, {0.0f, 0.0f}, spec, &canvas, theme);
    geom.outerBounds.y = bounds.y;
    geom.outerBounds.h = bounds.h;
    Element::RenderText(canvas, geom, spec, theme);

    if (!opts.separator.IsEnabled()) {
        return;
    }

    const auto& pal = theme.palette;
    const HeadingMetrics& m = theme.GetHeading(opts.level, size);
    const float alignFactor = (opts.align == Align::End) ? 1.0f : ((opts.align == Align::Center) ? 0.5f : 0.0f);

    const Color sepColor = theme.ResolveColor(
        opts.separator.color,
        hovered,
        active,
        effDisabled,
        pal.borderSubtle,
        pal.borderHover,
        pal.focusRing,
        pal.borderSubtle.WithAlphaF(0.40f)
    );
    const float strokeW = theme.Scale(opts.separator.width);

    const float outerPad = ComputeOuterPadding(spec, theme);
    const Rect inner = (outerPad > 0.0f) ? geom.outerBounds.Inset(outerPad) : geom.outerBounds;
    const float spanX = inner.x;
    const float spanW = inner.w;

    if (opts.separator.placement == HeadingSeparator::Bottom) {
        const float sepPad = std::max(4.0f, m.gap * 0.75f);
        float contentBottom = geom.controlRect.Bottom();
        if (geom.hasLabel) contentBottom = std::max(contentBottom, geom.labelRect.Bottom());
        if (geom.hasDesc)  contentBottom = std::max(contentBottom, geom.descRect.Bottom());
        const float sepY = std::floor(contentBottom + sepPad) + 0.5f;
        Separator::RenderLine(
            canvas,
            Vec2(spanX, sepY),
            Vec2(spanX + spanW, sepY),
            sepColor,
            strokeW
        );
    } else if (opts.separator.placement == HeadingSeparator::Trailing && geom.controlRect.h > 0.0f) {
        const Rect& labelBarRect = geom.controlRect;
        const Vec2 rawBar = MeasureLabelBar(spec, geom.labelFontSize, &canvas, theme);
        const float barW = std::min(labelBarRect.w, rawBar.x);
        const float barLeftX = labelBarRect.x + std::max(0.0f, labelBarRect.w - barW) * alignFactor;
        const float barRightX = barLeftX + barW;

        const float lineY = std::floor(labelBarRect.y + labelBarRect.h * 0.5f) + 0.5f;
        const float leftEnd = barLeftX - m.gap;
        const float rightStart = barRightX + m.gap;
        const float spanRight = spanX + spanW;

        if (leftEnd > spanX + 4.0f) {
            Separator::RenderLine(canvas, Vec2(spanX, lineY), Vec2(leftEnd, lineY), sepColor, strokeW);
        }
        if (spanRight > rightStart + 4.0f) {
            Separator::RenderLine(canvas, Vec2(rightStart, lineY), Vec2(spanRight, lineY), sepColor, strokeW);
        }
    }
}

} // namespace guikit
