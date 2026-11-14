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
    const HeadingMetrics& m = theme.GetHeading(activeSize);
    UiText resolvedLabel = opts.label;
    bool useHeadingMetrics = true;
    if (opts.level != HeadingLevel::Auto && resolvedLabel.size.IsAuto()) {
        useHeadingMetrics = false;
        switch (opts.level) {
            case HeadingLevel::H1: resolvedLabel.size = TextRole::HeadlineLarge; break;
            case HeadingLevel::H2: resolvedLabel.size = TextRole::HeadlineSmall; break;
            case HeadingLevel::H3: resolvedLabel.size = TextRole::TitleLarge; break;
            case HeadingLevel::H4: resolvedLabel.size = TextRole::TitleMedium; break;
            case HeadingLevel::Auto: break;
        }
    }
    return ElementSpec{
        .label = resolvedLabel,
        .description = opts.description,
        .icon = opts.icon,
        .badge = opts.badge,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .align = opts.align,
        .stretch = opts.stretch,
        .width = opts.width,
        .widthRatio = opts.widthRatio,
        .height = opts.height,
        .background = opts.background,
        .border = opts.border,
        .widgetSize = activeSize,
        .labelGap = m.lineGap,
        .descGap = m.lineGap,
        .inlineGap = m.gap * 1.5f,
        .hasControl = false,
        .headingTypography = useHeadingMetrics,
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
    if (opts.separator == HeadingSeparator::Bottom) {
        const HeadingMetrics& m = theme.GetHeading(size);
        const float divPad = std::max(4.0f, m.gap * 0.75f);
        return divPad + 1.0f + divPad * 0.5f;
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
        opts.height.value_or(0.0f),
        opts.background,
        opts.border,
        hOpts->label,
        hOpts->description,
        hOpts->badge,
        static_cast<uint8_t>(opts.separator),
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
    geom.outerBounds = bounds;
    Element::RenderText(canvas, geom, spec, theme);

    if (opts.separator == HeadingSeparator::None) {
        return;
    }

    const auto& pal = theme.palette;
    const HeadingMetrics& m = theme.GetHeading(size);
    const float alignFactor = (opts.align == Align::End) ? 1.0f : ((opts.align == Align::Center) ? 0.5f : 0.0f);

    const Color sepColor = theme.ResolveColor(
        opts.border.color,
        hovered,
        active,
        effDisabled,
        pal.borderSubtle,
        pal.borderHover,
        pal.focusRing,
        pal.borderSubtle.WithAlphaF(0.40f)
    );

    const float outerPad = ComputeOuterPadding(spec, theme);
    const Rect outer = bounds.Inset(outerPad);
    float spanX = outer.x;
    float spanW = outer.w;
    if (const auto fillRatio = Control::ResolveFillRatio(opts.stretch, opts.width, opts.widthRatio)) {
        const float maxElemW = std::max({
            geom.controlRect.w,
            geom.hasLabel ? geom.labelRect.w : 0.0f,
            geom.hasDesc ? geom.descRect.w : 0.0f
        });
        spanW = std::min(outer.w, std::max(outer.w * (*fillRatio), maxElemW));
        spanX = outer.x + (outer.w - spanW) * alignFactor;
    } else if (opts.stretch == StretchMode::Compact || Control::HasExplicitDpWidth(opts.stretch, opts.width, opts.widthRatio)) {
        const float maxElemW = std::max({
            geom.controlRect.w,
            geom.hasLabel ? geom.labelRect.w : 0.0f,
            geom.hasDesc ? geom.descRect.w : 0.0f
        });
        const float natSpanW = Control::HasExplicitDpWidth(opts.stretch, opts.width, opts.widthRatio)
            ? std::min(outer.w, theme.Scale(*opts.width))
            : std::min(outer.w, maxElemW);
        spanW = natSpanW;
        spanX = outer.x + (outer.w - spanW) * alignFactor;
    }

    if (opts.separator == HeadingSeparator::Bottom) {
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
            1.0f
        );
    } else if (opts.separator == HeadingSeparator::Trailing && geom.controlRect.h > 0.0f) {
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
            Separator::RenderLine(canvas, Vec2(spanX, lineY), Vec2(leftEnd, lineY), sepColor, 1.0f);
        }
        if (spanRight > rightStart + 4.0f) {
            Separator::RenderLine(canvas, Vec2(rightStart, lineY), Vec2(spanRight, lineY), sepColor, 1.0f);
        }
    }
}

} // namespace guikit
