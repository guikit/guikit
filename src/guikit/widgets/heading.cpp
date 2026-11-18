#include "guikit/widgets/heading.hpp"
#include <cmath>
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/layout/separator.hpp"

namespace guikit {

namespace {

float ResolveTitleFontSize(const HeadingOptions& opts, const HeadingMetrics& m, const UiTheme& theme) noexcept {
    if (opts.label.size.px > 0.0f) {
        return opts.label.size.px;
    }
    if (opts.label.size.role.has_value()) {
        return theme.GetTypography(*opts.label.size.role).fontSize;
    }
    if (opts.label.size.preset != UiSize::None) {
        return theme.GetMetrics(opts.label.size.preset).fontSize;
    }
    return m.titleFontSize;
}

float ResolveSubtitleFontSize(const HeadingOptions& opts, const HeadingMetrics& m, const UiTheme& theme) noexcept {
    if (opts.description.size.px > 0.0f) {
        return opts.description.size.px;
    }
    if (opts.description.size.role.has_value()) {
        return theme.GetTypography(*opts.description.size.role).fontSize;
    }
    if (opts.description.size.preset != UiSize::None) {
        return theme.GetMetrics(opts.description.size.preset).descFontSize;
    }
    return m.subtitleFontSize;
}

float ResolveBadgeFontSize(const HeadingOptions& opts, const HeadingMetrics& m, const UiTheme& theme) noexcept {
    if (opts.badge.size.px > 0.0f) {
        return opts.badge.size.px;
    }
    if (opts.badge.size.role.has_value()) {
        return theme.GetTypography(*opts.badge.size.role).fontSize;
    }
    if (opts.badge.size.preset != UiSize::None) {
        return theme.GetMetrics(opts.badge.size.preset).badgeFontSize;
    }
    return m.badgeFontSize;
}

Vec2 MeasureTitleBar(
    const HeadingOptions& opts,
    const HeadingMetrics& m,
    float titleFontSize,
    float badgeFontSize,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    float barW = 0.0f;
    float barH = 0.0f;

    if (opts.icon.IsValid()) {
        barW += m.iconSize;
        barH = keel::max(barH, m.iconSize);
    }

    if (opts.label.HasContentOrReserve()) {
        if (barW > 0.0f) barW += m.gap;
        barW += TextUtil::MeasureUiTextWidth(opts.label, titleFontSize, canvas);
        const float lineH = canvas ? canvas->GetLineHeight(titleFontSize) : theme.GetLineHeight(titleFontSize);
        barH = keel::max(barH, lineH);
    }

    if (opts.badge.HasContentOrReserve()) {
        if (barW > 0.0f) barW += m.badgeGap;
        const float badgeTextW = TextUtil::MeasureWidth(opts.badge, badgeFontSize, canvas);
        barW += badgeTextW + 12.0f;
        barH = keel::max(barH, badgeFontSize * 1.4f);
    }

    return {barW, barH};
}

struct HeadingLayoutGeometry {
    Rect bounds{};
    Rect titleBarRect{};
    Rect descRect{};
    float rawTitleBarW{0.0f};
    float titleFontSize{16.0f};
    float descFontSize{12.0f};
    float badgeFontSize{10.0f};
    bool hasTitleBar{false};
    bool hasDesc{false};
};

HeadingLayoutGeometry ResolveHeadingGeometry(
    Rect bounds,
    const HeadingOptions& opts,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    HeadingLayoutGeometry g{};
    const HeadingMetrics& m = theme.GetHeading(opts.level);
    g.titleFontSize = ResolveTitleFontSize(opts, m, theme);
    g.descFontSize = ResolveSubtitleFontSize(opts, m, theme);
    g.badgeFontSize = ResolveBadgeFontSize(opts, m, theme);
    g.hasTitleBar = opts.icon.IsValid() || opts.label.HasContentOrReserve() || opts.badge.HasContentOrReserve();
    g.hasDesc = opts.description.HasContentOrReserve();

    const Vec2 rawBar = g.hasTitleBar
        ? MeasureTitleBar(opts, m, g.titleFontSize, g.badgeFontSize, canvas, theme)
        : Vec2{0.0f, 0.0f};
    g.rawTitleBarW = rawBar.x;

    const float rawDescW = g.hasDesc ? TextUtil::MeasureUiTextWidth(opts.description, g.descFontSize, canvas) : 0.0f;
    const float inlineGap = m.gap * 1.5f;
    const float alignFactor = (opts.align == Align::End) ? 1.0f : ((opts.align == Align::Center) ? 0.5f : 0.0f);
    const float natW = (opts.layout == HeadingLayout::Inline)
        ? (rawBar.x + (g.hasTitleBar && g.hasDesc && rawDescW > 0.0f ? inlineGap + rawDescW : rawDescW))
        : keel::max(rawBar.x, rawDescW);
    const float minW = (g.hasDesc && !opts.description.wrap.IsWordWrap()) ? natW : rawBar.x;

    if (opts.width.IsFixed()) {
        const float effW = keel::min(bounds.w, keel::max(minW, opts.width.Dp()));
        bounds.x += (bounds.w - effW) * alignFactor;
        bounds.w = effW;
    } else if (opts.width.IsFit()) {
        const float effW = keel::min(bounds.w, natW);
        bounds.x += (bounds.w - effW) * alignFactor;
        bounds.w = effW;
    }

    g.bounds = bounds;
    const float bottomSep = Heading::ComputeBottomSeparatorSpace(opts, theme);
    Rect contentBounds = bounds;
    if (bottomSep > 0.0f) {
        contentBounds.h = keel::max(0.0f, contentBounds.h - bottomSep);
    }

    if (!g.hasDesc) {
        const float barH = keel::min(rawBar.y, contentBounds.h);
        const float barY = contentBounds.y + (contentBounds.h - barH) * 0.5f;
        g.titleBarRect = Rect(contentBounds.x, barY, contentBounds.w, barH);
        return g;
    }

    const float descLineH = canvas ? canvas->GetLineHeight(g.descFontSize) : theme.GetLineHeight(g.descFontSize);
    if (!g.hasTitleBar) {
        const float descH = TextUtil::MeasureUiTextHeight(opts.description, contentBounds.w, g.descFontSize, descLineH, canvas, &theme);
        g.descRect = Rect(contentBounds.x, contentBounds.y, contentBounds.w, keel::min(descH, contentBounds.h));
        return g;
    }

    if (opts.layout == HeadingLayout::Inline) {
        const float barW = keel::min(contentBounds.w, rawBar.x);
        const float availDescW = keel::max(0.0f, contentBounds.w - barW - inlineGap);
        const float descBlockW = keel::min(rawDescW, availDescW);
        const float descH = TextUtil::MeasureUiTextHeight(opts.description, descBlockW, g.descFontSize, descLineH, canvas, &theme);
        const float barH = keel::min(rawBar.y, contentBounds.h);
        const float clampedDescH = keel::min(descH, contentBounds.h);
        if (opts.width.IsFit()) {
            const float totalInlineW = barW + (descBlockW > 0.0f ? inlineGap + descBlockW : 0.0f);
            const float groupX = contentBounds.x + keel::max(0.0f, contentBounds.w - totalInlineW) * alignFactor;
            g.titleBarRect = Rect(groupX, contentBounds.y + (contentBounds.h - barH) * 0.5f, barW, barH);
            g.descRect = Rect(groupX + barW + inlineGap, contentBounds.y + (contentBounds.h - clampedDescH) * 0.5f, descBlockW, clampedDescH);
        } else {
            g.titleBarRect = Rect(contentBounds.x, contentBounds.y + (contentBounds.h - barH) * 0.5f, barW, barH);
            g.descRect = Rect(contentBounds.Right() - descBlockW, contentBounds.y + (contentBounds.h - clampedDescH) * 0.5f, descBlockW, clampedDescH);
        }
        return g;
    }

    const float descH = TextUtil::MeasureUiTextHeight(opts.description, contentBounds.w, g.descFontSize, descLineH, canvas, &theme);
    if (opts.layout == HeadingLayout::Overline) {
        g.descRect = Rect(contentBounds.x, contentBounds.y, contentBounds.w, descH);
        const float barY = contentBounds.y + descH + m.lineGap;
        g.titleBarRect = Rect(contentBounds.x, barY, contentBounds.w, rawBar.y);
    } else { // HeadingLayout::Stacked
        g.titleBarRect = Rect(contentBounds.x, contentBounds.y, contentBounds.w, rawBar.y);
        const float descY = contentBounds.y + rawBar.y + m.lineGap;
        g.descRect = Rect(contentBounds.x, descY, contentBounds.w, descH);
    }
    return g;
}

} // namespace

float Heading::ComputeBottomSeparatorSpace(
    const HeadingOptions& opts,
    const UiTheme& theme
) noexcept {
    if (opts.separator.IsEnabled() && opts.separator.placement == HeadingSeparator::Bottom) {
        const HeadingMetrics& m = theme.GetHeading(opts.level);
        const float divPad = keel::max(4.0f, m.gap * 0.75f);
        const float strokeW = opts.separator.width;
        return divPad + strokeW + divPad * 0.5f;
    }
    return 0.0f;
}

Response Heading::Run(
    Context& ctx,
    const HeadingOptions& opts,
    keel::SourceLocation loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();

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
    node->uiSize = UiSize::None;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;

    if (opts.width.IsFixed()) {
        node->widthMode = SizeMode::Fixed;
        node->fixedWidth = opts.width.Dp();
    } else {
        node->widthMode = Control::ResolveWidthMode(ctx, opts.width, SizeMode::Stretch);
    }
    node->heightMode = SizeMode::Fit;

    node->visualHash = detail::HashArgs(
        static_cast<keel::u8>(opts.level),
        static_cast<keel::u8>(opts.layout),
        static_cast<keel::u8>(opts.align),
        static_cast<keel::u8>(node->widthMode),
        opts.width,
        hOpts->label,
        hOpts->description,
        hOpts->badge,
        opts.separator,
        effectiveDisabled,
        static_cast<keel::u8>(opts.icon.GetType()),
        static_cast<keel::u8>(opts.icon.GetId()),
        hOpts->icon.GetPath()
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* o = n->As<HeadingOptions>();
        return Heading::Measure(o ? *o : HeadingOptions{}, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* o = n->As<HeadingOptions>();
        Heading::Render(
            canvas,
            n->computedRect,
            o ? *o : HeadingOptions{},
            theme,
            n->disabled,
            n->hovered,
            n->pressed || n->focused
        );
    };


    return res;
}

Vec2 Heading::Measure(
    const HeadingOptions& opts,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) noexcept {
    const HeadingMetrics& m = theme.GetHeading(opts.level);
    const float titleFs = ResolveTitleFontSize(opts, m, theme);
    const float descFs = ResolveSubtitleFontSize(opts, m, theme);
    const float badgeFs = ResolveBadgeFontSize(opts, m, theme);
    const bool hasTitleBar = opts.icon.IsValid() || opts.label.HasContentOrReserve() || opts.badge.HasContentOrReserve();
    const bool hasDesc = opts.description.HasContentOrReserve();
    const float bottomSep = ComputeBottomSeparatorSpace(opts, theme);

    const Vec2 rawBar = hasTitleBar
        ? MeasureTitleBar(opts, m, titleFs, badgeFs, canvas, theme)
        : Vec2{0.0f, 0.0f};
    const float rawDescW = hasDesc ? TextUtil::MeasureUiTextWidth(opts.description, descFs, canvas) : 0.0f;
    const float inlineGap = m.gap * 1.5f;

    const bool hasExplicitDpW = opts.width.IsFixed();
    const float fixedDpW = opts.width.Dp();
    if (hasExplicitDpW) {
        const float natW = (opts.layout == HeadingLayout::Inline)
            ? (rawBar.x + (hasTitleBar && hasDesc && rawDescW > 0.0f ? inlineGap + rawDescW : rawDescW))
            : keel::max(rawBar.x, rawDescW);
        const float minW = (hasDesc && !opts.description.wrap.IsWordWrap()) ? natW : rawBar.x;
        const float targetW = keel::max(minW, fixedDpW);
        availableWidth = (availableWidth > 0.0f) ? keel::min(availableWidth, targetW) : targetW;
    }

    if (!hasDesc) {
        float w = rawBar.x;
        if (hasExplicitDpW) w = keel::max(w, fixedDpW);
        return {w, rawBar.y + bottomSep};
    }

    const float descLineH = canvas ? canvas->GetLineHeight(descFs) : theme.GetLineHeight(descFs);
    if (!hasTitleBar) {
        const float descH = TextUtil::MeasureUiTextHeight(opts.description, availableWidth, descFs, descLineH, canvas, &theme);
        float w = (hasExplicitDpW && opts.description.wrap.IsWordWrap())
            ? keel::min(rawDescW, fixedDpW)
            : rawDescW;
        if (hasExplicitDpW) w = keel::max(w, fixedDpW);
        return {w, descH + bottomSep};
    }

    if (opts.layout == HeadingLayout::Inline) {
        const float descWrapW = (availableWidth > 0.0f)
            ? keel::max(0.0f, availableWidth - keel::min(availableWidth, rawBar.x) - inlineGap)
            : 0.0f;
        const float descH = TextUtil::MeasureUiTextHeight(opts.description, descWrapW, descFs, descLineH, canvas, &theme);
        const float effDescW = (hasExplicitDpW && opts.description.wrap.IsWordWrap())
            ? keel::min(rawDescW, descWrapW)
            : rawDescW;
        float totalW = rawBar.x + (effDescW > 0.0f ? inlineGap + effDescW : 0.0f);
        if (hasExplicitDpW) totalW = keel::max(totalW, fixedDpW);
        return {totalW, keel::max(rawBar.y, descH) + bottomSep};
    }

    const float descH = TextUtil::MeasureUiTextHeight(opts.description, availableWidth, descFs, descLineH, canvas, &theme);
    const float effDescW = (hasExplicitDpW && opts.description.wrap.IsWordWrap())
        ? keel::min(rawDescW, availableWidth)
        : rawDescW;
    float totalW = keel::max(rawBar.x, effDescW);
    if (hasExplicitDpW) totalW = keel::max(totalW, fixedDpW);
    return {totalW, rawBar.y + m.lineGap + descH + bottomSep};
}

void Heading::Render(
    ICanvas& canvas,
    Rect bounds,
    const HeadingOptions& opts,
    const UiTheme& theme,
    bool disabled,
    bool hovered,
    bool active
) noexcept {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const bool effDisabled = disabled || opts.disabled;
    const HeadingLayoutGeometry g = ResolveHeadingGeometry(bounds, opts, &canvas, theme);
    const HeadingMetrics& m = theme.GetHeading(opts.level);
    const auto& pal = theme.palette;
    const float alignFactor = (opts.align == Align::End) ? 1.0f : ((opts.align == Align::Center) ? 0.5f : 0.0f);

    const Color titleCol = theme.ResolveTextColor(opts.label, UiVariant::Default, effDisabled, hovered, active);
    const Color descCol = theme.ResolveTextColor(opts.description, UiVariant::Secondary, effDisabled, hovered, active);

    float barLeftX = g.titleBarRect.x;
    float barRightX = g.titleBarRect.x;

    if (g.hasTitleBar && g.titleBarRect.w > 0.0f && g.titleBarRect.h > 0.0f) {
        const float barW = keel::min(g.titleBarRect.w, g.rawTitleBarW);
        const float barAlign = (opts.layout == HeadingLayout::Inline && g.hasDesc) ? 0.0f : alignFactor;
        float curX = g.titleBarRect.x + keel::max(0.0f, g.titleBarRect.w - barW) * barAlign;
        barLeftX = curX;
        barRightX = curX + barW;

        const float topY = g.titleBarRect.y;
        const float titleLineH = g.titleBarRect.h;

        if (opts.icon.IsValid()) {
            const float iconY = topY + (titleLineH - m.iconSize) * 0.5f;
            opts.icon.Draw(canvas, Rect(curX, iconY, m.iconSize, m.iconSize), titleCol);
            curX += m.iconSize + ((opts.label.HasContentOrReserve() || !opts.badge.empty()) ? m.gap : 0.0f);
        }

        if (!opts.label.empty()) {
            const float badgeReserveW = opts.badge.empty()
                ? 0.0f
                : (m.badgeGap + TextUtil::MeasureWidth(opts.badge, g.badgeFontSize, &canvas) + 12.0f);
            const float titleAvailW = keel::max(0.0f, barRightX - curX - badgeReserveW);
            const Rect titleR(curX, topY, titleAvailW, titleLineH);
            if (opts.label.wrap.rich) {
                const float rawTitleW = TextUtil::MeasureUiTextWidth(opts.label, g.titleFontSize, &canvas);
                UiText singleLineRich = opts.label;
                singleLineRich.wrap.maxLines = 1;
                TextUtil::DrawUiText(canvas, singleLineRich, titleR, g.titleFontSize, titleLineH, titleCol, 0.0f, 0.5f, &theme);
                curX += keel::min(titleAvailW, rawTitleW);
            } else {
                float titleW = canvas.MeasureText(opts.label.text, g.titleFontSize).x;
                keel::FixedString<256> truncatedTitle;
                keel::StringRef displayTitle = opts.label.text;
                if (titleW > titleAvailW + kTextSubpixelTolerance && titleAvailW > 0.0f) {
                    truncatedTitle = TextUtil::TruncateWithEllipsis(opts.label.text, titleAvailW, g.titleFontSize, &canvas);
                    displayTitle = truncatedTitle;
                    titleW = canvas.MeasureText(displayTitle, g.titleFontSize).x;
                }
                canvas.DrawTextBox(displayTitle, titleR, g.titleFontSize, titleCol, 0.0f, 0.5f);
                curX += titleW;
            }
        }

        if (!opts.badge.empty()) {
            if (!opts.label.empty()) {
                curX += m.badgeGap;
            }
            const float badgeTextW = TextUtil::MeasureWidth(opts.badge, g.badgeFontSize, &canvas);
            const float badgePadH = 6.0f;
            const float badgeW = badgeTextW + badgePadH * 2.0f;
            const float badgeH = g.badgeFontSize + 4.0f;
            const float badgeY = topY + (titleLineH - badgeH) * 0.5f;

            Color badgeBg = pal.surfaceHover;
            Color badgeTextCol = pal.textPrimary;
            Color badgeBorderCol = Color::Transparent();
            float badgeBorderW = 0.0f;
            if (opts.badge.color.HasAny()) {
                const Color resolvedBadge = theme.ResolveColor(opts.badge.color, hovered, active, effDisabled, pal.textPrimary);
                badgeBg = resolvedBadge.WithAlphaF(effDisabled ? 0.12f : 0.22f);
                badgeTextCol = resolvedBadge;
                badgeBorderCol = resolvedBadge.WithAlphaF(effDisabled ? 0.25f : 0.50f);
                badgeBorderW = 1.0f;
            } else if (effDisabled) {
                badgeBg = badgeBg.WithAlphaF(0.40f);
                badgeTextCol = badgeTextCol.WithAlphaF(0.40f);
            }

            canvas.DrawRoundRect(Rect(curX, badgeY, badgeW, badgeH), badgeH * 0.5f, badgeBg, badgeBorderCol, badgeBorderW);
            canvas.DrawTextBox(opts.badge.text, Rect(curX, badgeY, badgeW, badgeH), g.badgeFontSize, badgeTextCol, 0.5f, 0.5f);
            curX += badgeW;
        }
    }

    if (g.hasDesc && !opts.description.empty() && g.descRect.w > 0.0f && g.descRect.h > 0.0f) {
        const float lineH = canvas.GetLineHeight(g.descFontSize);
        const bool compactInline = opts.width.IsFit();
        const float descAlign = (opts.layout == HeadingLayout::Inline && g.hasTitleBar)
            ? (compactInline ? 0.0f : 1.0f)
            : alignFactor;
        TextUtil::DrawUiText(canvas, opts.description, g.descRect, g.descFontSize, lineH, descCol, descAlign, 0.5f, &theme);
    }

    if (!opts.separator.IsEnabled()) {
        return;
    }

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
    const float sepStrokeW = opts.separator.width;

    if (opts.separator.placement == HeadingSeparator::Bottom) {
        const float sepPad = keel::max(4.0f, m.gap * 0.75f);
        float contentBottom = g.bounds.y;
        if (g.hasTitleBar) contentBottom = keel::max(contentBottom, g.titleBarRect.Bottom());
        if (g.hasDesc)     contentBottom = keel::max(contentBottom, g.descRect.Bottom());
        const float sepY = std::floor(contentBottom + sepPad) + 0.5f;
        Separator::RenderLine(canvas, Vec2(g.bounds.x, sepY), Vec2(g.bounds.Right(), sepY), sepColor, sepStrokeW);
    } else if (opts.separator.placement == HeadingSeparator::Trailing && g.hasTitleBar && g.titleBarRect.h > 0.0f) {
        const float lineY = std::floor(g.titleBarRect.y + g.titleBarRect.h * 0.5f) + 0.5f;
        if (opts.layout == HeadingLayout::Inline && g.hasDesc && g.descRect.w > 0.0f) {
            const float bridgeStart = barRightX + m.gap;
            const float bridgeEnd = g.descRect.x - m.gap;
            if (bridgeEnd > bridgeStart + 4.0f) {
                Separator::RenderLine(canvas, Vec2(bridgeStart, lineY), Vec2(bridgeEnd, lineY), sepColor, sepStrokeW);
            }
        } else {
            const float leftEnd = barLeftX - m.gap;
            const float rightStart = barRightX + m.gap;

            if (leftEnd > g.bounds.x + 4.0f) {
                Separator::RenderLine(canvas, Vec2(g.bounds.x, lineY), Vec2(leftEnd, lineY), sepColor, sepStrokeW);
            }
            if (g.bounds.Right() > rightStart + 4.0f) {
                Separator::RenderLine(canvas, Vec2(rightStart, lineY), Vec2(g.bounds.Right(), lineY), sepColor, sepStrokeW);
            }
        }
    }
}

} // namespace guikit
