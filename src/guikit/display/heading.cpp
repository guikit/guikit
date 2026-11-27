#include "guikit/display/heading.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/style/style_painter.hpp"
#include "guikit/core/control.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/display/separator.hpp"
#include "keel/math.h"

namespace guikit {

namespace {

keel::f32 ResolveTitleFontSize(const HeadingOptions& opts, const HeadingMetrics& m, const UiTheme& theme) noexcept {
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

keel::f32 ResolveSubtitleFontSize(const HeadingOptions& opts, const HeadingMetrics& m, const UiTheme& theme) noexcept {
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

keel::f32 ResolveBadgeFontSize(const HeadingOptions& opts, const HeadingMetrics& m, const UiTheme& theme) noexcept {
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
    keel::f32 titleFontSize,
    keel::f32 badgeFontSize,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    keel::f32 barW = 0.0f;
    keel::f32 barH = 0.0f;

    if (opts.icon.IsValid()) {
        barW += m.iconSize;
        barH = keel::max(barH, m.iconSize);
    }

    if (opts.label.HasContentOrReserve()) {
        if (barW > 0.0f) barW += m.gap;
        barW += TextUtil::MeasureUiTextWidth(opts.label, titleFontSize, canvas);
        const keel::f32 lineH = canvas ? canvas->GetLineHeight(titleFontSize) : theme.GetLineHeight(titleFontSize);
        barH = keel::max(barH, lineH);
    }

    if (opts.badge.HasContentOrReserve()) {
        if (barW > 0.0f) barW += m.badgeGap;
        const keel::f32 badgeTextW = TextUtil::MeasureWidth(opts.badge, badgeFontSize, canvas);
        barW += badgeTextW + 12.0f;
        barH = keel::max(barH, badgeFontSize * 1.4f);
    }

    return {barW, barH};
}

struct HeadingLayoutGeometry {
    Rect bounds{};
    Rect titleBarRect{};
    Rect descRect{};
    keel::f32 rawTitleBarW{0.0f};
    keel::f32 titleFontSize{16.0f};
    keel::f32 descFontSize{12.0f};
    keel::f32 badgeFontSize{10.0f};
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

    const keel::f32 rawDescW = g.hasDesc ? TextUtil::MeasureUiTextWidth(opts.description, g.descFontSize, canvas) : 0.0f;
    const keel::f32 inlineGap = m.gap * 1.5f;
    const keel::f32 alignFactor = (opts.align == Align::End) ? 1.0f : ((opts.align == Align::Center) ? 0.5f : 0.0f);
    const keel::f32 natW = (opts.layout == HeadingLayout::Inline)
        ? (rawBar.x + (g.hasTitleBar && g.hasDesc && rawDescW > 0.0f ? inlineGap + rawDescW : rawDescW))
        : keel::max(rawBar.x, rawDescW);
    const keel::f32 minW = (g.hasDesc && !opts.description.wrap.IsWordWrap()) ? natW : rawBar.x;

    if (opts.width.IsFixed()) {
        const keel::f32 effW = keel::min(bounds.w, keel::max(minW, opts.width.Dp()));
        bounds.x += (bounds.w - effW) * alignFactor;
        bounds.w = effW;
    } else if (opts.width.IsFit()) {
        const keel::f32 effW = keel::min(bounds.w, natW);
        bounds.x += (bounds.w - effW) * alignFactor;
        bounds.w = effW;
    }

    g.bounds = bounds;
    const keel::f32 bottomSep = detail::Heading::ComputeBottomSeparatorSpace(opts, theme);
    Rect contentBounds = bounds;
    if (bottomSep > 0.0f) {
        contentBounds.h = keel::max(0.0f, contentBounds.h - bottomSep);
    }

    if (!g.hasDesc) {
        const keel::f32 barH = keel::min(rawBar.y, contentBounds.h);
        const keel::f32 barY = contentBounds.y + (contentBounds.h - barH) * 0.5f;
        g.titleBarRect = Rect(contentBounds.x, barY, contentBounds.w, barH);
        return g;
    }

    const keel::f32 descLineH = canvas ? canvas->GetLineHeight(g.descFontSize) : theme.GetLineHeight(g.descFontSize);
    if (!g.hasTitleBar) {
        const keel::f32 descH = TextUtil::MeasureUiTextHeight(opts.description, contentBounds.w, g.descFontSize, descLineH, canvas, &theme);
        g.descRect = Rect(contentBounds.x, contentBounds.y, contentBounds.w, keel::min(descH, contentBounds.h));
        return g;
    }

    if (opts.layout == HeadingLayout::Inline) {
        const keel::f32 barW = keel::min(contentBounds.w, rawBar.x);
        const keel::f32 availDescW = keel::max(0.0f, contentBounds.w - barW - inlineGap);
        const keel::f32 descBlockW = keel::min(rawDescW, availDescW);
        const keel::f32 descH = TextUtil::MeasureUiTextHeight(opts.description, descBlockW, g.descFontSize, descLineH, canvas, &theme);
        const keel::f32 barH = keel::min(rawBar.y, contentBounds.h);
        const keel::f32 clampedDescH = keel::min(descH, contentBounds.h);
        if (opts.width.IsFit()) {
            const keel::f32 totalInlineW = barW + (descBlockW > 0.0f ? inlineGap + descBlockW : 0.0f);
            const keel::f32 groupX = contentBounds.x + keel::max(0.0f, contentBounds.w - totalInlineW) * alignFactor;
            g.titleBarRect = Rect(groupX, contentBounds.y + (contentBounds.h - barH) * 0.5f, barW, barH);
            g.descRect = Rect(groupX + barW + inlineGap, contentBounds.y + (contentBounds.h - clampedDescH) * 0.5f, descBlockW, clampedDescH);
        } else {
            g.titleBarRect = Rect(contentBounds.x, contentBounds.y + (contentBounds.h - barH) * 0.5f, barW, barH);
            g.descRect = Rect(contentBounds.Right() - descBlockW, contentBounds.y + (contentBounds.h - clampedDescH) * 0.5f, descBlockW, clampedDescH);
        }
        return g;
    }

    const keel::f32 descH = TextUtil::MeasureUiTextHeight(opts.description, contentBounds.w, g.descFontSize, descLineH, canvas, &theme);
    if (opts.layout == HeadingLayout::Overline) {
        g.descRect = Rect(contentBounds.x, contentBounds.y, contentBounds.w, descH);
        const keel::f32 barY = contentBounds.y + descH + m.lineGap;
        g.titleBarRect = Rect(contentBounds.x, barY, contentBounds.w, rawBar.y);
    } else { // HeadingLayout::Stacked
        g.titleBarRect = Rect(contentBounds.x, contentBounds.y, contentBounds.w, rawBar.y);
        const keel::f32 descY = contentBounds.y + rawBar.y + m.lineGap;
        g.descRect = Rect(contentBounds.x, descY, contentBounds.w, descH);
    }
    return g;
}

} // namespace

keel::f32 detail::Heading::ComputeBottomSeparatorSpace(
    const HeadingOptions& opts,
    const UiTheme& theme
) noexcept {
    if (opts.separator.IsEnabled() && opts.separator.placement == HeadingSeparator::Bottom) {
        const HeadingMetrics& m = theme.GetHeading(opts.level);
        const keel::f32 divPad = keel::max(4.0f, m.gap * 0.75f);
        const keel::f32 strokeW = opts.separator.width;
        return divPad + strokeW + divPad * 0.5f;
    }
    return 0.0f;
}

Response detail::Heading::Run(
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
    node->flexGrow = Control::ResolveFlexGrow(node->parent, opts.width, AxisSize::Fit(), 1.0f);

    node->visualHash = detail::HashArgs(
        keel::as<keel::u8>(opts.level),
        keel::as<keel::u8>(opts.layout),
        keel::as<keel::u8>(opts.align),
        keel::as<keel::u8>(node->widthMode),
        opts.width,
        hOpts->label,
        hOpts->description,
        hOpts->badge,
        opts.separator,
        effectiveDisabled,
        keel::as<keel::u8>(opts.icon.GetType()),
        keel::as<keel::u8>(opts.icon.GetId()),
        hOpts->icon.GetPath()
    );

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* o = n->As<HeadingOptions>();
        return detail::Heading::Measure(o ? *o : HeadingOptions{}, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* o = n->As<HeadingOptions>();
        HeadingOptions effOpts = o ? *o : HeadingOptions{};
        const Node* p = n->parent;
        const bool onActiveTreeRow =
            (p && p->type == NodeType::TreeItem && p->active) ||
            (p && p->parent && p->parent->type == NodeType::TreeItem && p->parent->active);
        const bool onSolidSel = onActiveTreeRow && theme.Painter().GetChromeMetrics(theme).solidSelectionHighlight;
        if (onSolidSel) {
            const auto& pal = theme.palette;
            if (!effOpts.label.color.HasAny()) {
                effOpts.label.color = StateColor{pal.accentText};
            }
            if (!effOpts.description.color.HasAny()) {
                effOpts.description.color = StateColor{pal.accentText.WithAlphaF(0.86f)};
            }
            // Mark active=true so badge switches to solid-accent contrast when onSolidSel
            if (!effOpts.badge.empty()) {
                effOpts.badge.color = StateColor{pal.accentText};
            }
        }
        detail::Heading::Render(
            canvas,
            n->computedRect,
            effOpts,
            theme,
            n->disabled,
            n->hovered,
            n->pressed || n->focused || onSolidSel
        );
    };


    return res;
}

Vec2 detail::Heading::Measure(
    const HeadingOptions& opts,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 availableWidth
) noexcept {
    const HeadingMetrics& m = theme.GetHeading(opts.level);
    const keel::f32 titleFs = ResolveTitleFontSize(opts, m, theme);
    const keel::f32 descFs = ResolveSubtitleFontSize(opts, m, theme);
    const keel::f32 badgeFs = ResolveBadgeFontSize(opts, m, theme);
    const bool hasTitleBar = opts.icon.IsValid() || opts.label.HasContentOrReserve() || opts.badge.HasContentOrReserve();
    const bool hasDesc = opts.description.HasContentOrReserve();
    const keel::f32 bottomSep = ComputeBottomSeparatorSpace(opts, theme);

    const Vec2 rawBar = hasTitleBar
        ? MeasureTitleBar(opts, m, titleFs, badgeFs, canvas, theme)
        : Vec2{0.0f, 0.0f};
    const keel::f32 rawDescW = hasDesc ? TextUtil::MeasureUiTextWidth(opts.description, descFs, canvas) : 0.0f;
    const keel::f32 inlineGap = m.gap * 1.5f;

    const bool hasExplicitDpW = opts.width.IsFixed();
    const keel::f32 fixedDpW = opts.width.Dp();
    if (hasExplicitDpW) {
        const keel::f32 natW = (opts.layout == HeadingLayout::Inline)
            ? (rawBar.x + (hasTitleBar && hasDesc && rawDescW > 0.0f ? inlineGap + rawDescW : rawDescW))
            : keel::max(rawBar.x, rawDescW);
        const keel::f32 minW = (hasDesc && !opts.description.wrap.IsWordWrap()) ? natW : rawBar.x;
        const keel::f32 targetW = keel::max(minW, fixedDpW);
        availableWidth = (availableWidth > 0.0f) ? keel::min(availableWidth, targetW) : targetW;
    }

    if (!hasDesc) {
        keel::f32 w = rawBar.x;
        if (hasExplicitDpW) w = keel::max(w, fixedDpW);
        return {w, rawBar.y + bottomSep};
    }

    const keel::f32 descLineH = canvas ? canvas->GetLineHeight(descFs) : theme.GetLineHeight(descFs);
    if (!hasTitleBar) {
        const keel::f32 descH = TextUtil::MeasureUiTextHeight(opts.description, availableWidth, descFs, descLineH, canvas, &theme);
        keel::f32 w = (hasExplicitDpW && opts.description.wrap.IsWordWrap())
            ? keel::min(rawDescW, fixedDpW)
            : rawDescW;
        if (hasExplicitDpW) w = keel::max(w, fixedDpW);
        return {w, descH + bottomSep};
    }

    if (opts.layout == HeadingLayout::Inline) {
        const keel::f32 descWrapW = (availableWidth > 0.0f)
            ? keel::max(0.0f, availableWidth - keel::min(availableWidth, rawBar.x) - inlineGap)
            : 0.0f;
        const keel::f32 descH = TextUtil::MeasureUiTextHeight(opts.description, descWrapW, descFs, descLineH, canvas, &theme);
        const keel::f32 effDescW = (hasExplicitDpW && opts.description.wrap.IsWordWrap())
            ? keel::min(rawDescW, descWrapW)
            : rawDescW;
        keel::f32 totalW = rawBar.x + (effDescW > 0.0f ? inlineGap + effDescW : 0.0f);
        if (hasExplicitDpW) totalW = keel::max(totalW, fixedDpW);
        return {totalW, keel::max(rawBar.y, descH) + bottomSep};
    }

    const keel::f32 descH = TextUtil::MeasureUiTextHeight(opts.description, availableWidth, descFs, descLineH, canvas, &theme);
    const keel::f32 effDescW = (hasExplicitDpW && opts.description.wrap.IsWordWrap())
        ? keel::min(rawDescW, availableWidth)
        : rawDescW;
    keel::f32 totalW = keel::max(rawBar.x, effDescW);
    if (hasExplicitDpW) totalW = keel::max(totalW, fixedDpW);
    return {totalW, rawBar.y + m.lineGap + descH + bottomSep};
}

void detail::Heading::Render(
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
    const keel::f32 alignFactor = (opts.align == Align::End) ? 1.0f : ((opts.align == Align::Center) ? 0.5f : 0.0f);

    const Color titleCol = theme.ResolveTextColor(opts.label, UiVariant::Default, effDisabled, hovered, active);
    const Color descCol = theme.ResolveTextColor(opts.description, UiVariant::Secondary, effDisabled, hovered, active);

    keel::f32 barLeftX = g.titleBarRect.x;
    keel::f32 barRightX = g.titleBarRect.x;

    if (g.hasTitleBar && g.titleBarRect.w > 0.0f && g.titleBarRect.h > 0.0f) {
        const keel::f32 barW = keel::min(g.titleBarRect.w, g.rawTitleBarW);
        const keel::f32 barAlign = (opts.layout == HeadingLayout::Inline && g.hasDesc) ? 0.0f : alignFactor;
        keel::f32 curX = g.titleBarRect.x + keel::max(0.0f, g.titleBarRect.w - barW) * barAlign;
        barLeftX = curX;
        barRightX = curX + barW;

        const keel::f32 topY = g.titleBarRect.y;
        const keel::f32 titleLineH = g.titleBarRect.h;

        if (opts.icon.IsValid()) {
            const keel::f32 iconY = topY + (titleLineH - m.iconSize) * 0.5f;
            opts.icon.Draw(canvas, Rect(curX, iconY, m.iconSize, m.iconSize), titleCol);
            curX += m.iconSize + ((opts.label.HasContentOrReserve() || !opts.badge.empty()) ? m.gap : 0.0f);
        }

        if (!opts.label.empty()) {
            const keel::f32 badgeReserveW = opts.badge.empty()
                ? 0.0f
                : (m.badgeGap + TextUtil::MeasureWidth(opts.badge, g.badgeFontSize, &canvas) + 12.0f);
            const keel::f32 titleAvailW = keel::max(0.0f, barRightX - curX - badgeReserveW);
            const Rect titleR(curX, topY, titleAvailW, titleLineH);
            if (opts.label.wrap.rich) {
                const keel::f32 rawTitleW = TextUtil::MeasureUiTextWidth(opts.label, g.titleFontSize, &canvas);
                UiText singleLineRich = opts.label;
                singleLineRich.wrap.maxLines = 1;
                TextUtil::DrawUiText(canvas, singleLineRich, titleR, g.titleFontSize, titleLineH, titleCol, 0.0f, 0.5f, &theme);
                curX += keel::min(titleAvailW, rawTitleW);
            } else {
                keel::f32 titleW = canvas.MeasureText(opts.label.text, g.titleFontSize).x;
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
            const keel::f32 badgeTextW = TextUtil::MeasureWidth(opts.badge, g.badgeFontSize, &canvas);
            const keel::f32 badgePadH = 6.0f;
            const keel::f32 badgeW = badgeTextW + badgePadH * 2.0f;
            const keel::f32 badgeH = g.badgeFontSize + 4.0f;
            const keel::f32 badgeY = topY + (titleLineH - badgeH) * 0.5f;

            Color badgeBg = pal.surfaceHover;
            Color badgeTextCol = pal.textPrimary;
            Color badgeBorderCol = Color::Transparent();
            keel::f32 badgeBorderW = 0.0f;
            if (opts.badge.color.HasAny()) {
                const Color resolvedBadge = theme.ResolveColor(opts.badge.color, hovered, active, effDisabled, pal.textPrimary);
                badgeBg = resolvedBadge.WithAlphaF(effDisabled ? 0.12f : 0.22f);
                const Color refBg = (active && theme.Painter().GetChromeMetrics(theme).solidSelectionHighlight)
                    ? pal.accent
                    : pal.surface;
                badgeTextCol = effDisabled ? resolvedBadge.WithAlphaF(theme.disabledOpacity) : theme.EnsureReadableInk(resolvedBadge, refBg);
                badgeBorderCol = resolvedBadge.WithAlphaF(effDisabled ? 0.25f : 0.50f);
                badgeBorderW = 1.0f;
            } else if (effDisabled) {
                badgeBg = badgeBg.WithAlphaF(theme.disabledOpacity);
                badgeTextCol = badgeTextCol.WithAlphaF(theme.disabledOpacity);
            }

            const Rect badgeR(curX, badgeY, badgeW, badgeH);
            theme.Painter().DrawBadge(canvas, theme, badgeR, badgeBg, badgeBorderCol, badgeBorderW, badgeH * 0.5f);
            canvas.DrawTextBox(opts.badge.text, badgeR, g.badgeFontSize, badgeTextCol, 0.5f, 0.5f);
            curX += badgeW;
        }
    }

    if (g.hasDesc && !opts.description.empty() && g.descRect.w > 0.0f && g.descRect.h > 0.0f) {
        const keel::f32 lineH = canvas.GetLineHeight(g.descFontSize);
        const bool compactInline = opts.width.IsFit();
        const keel::f32 descAlign = (opts.layout == HeadingLayout::Inline && g.hasTitleBar)
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
        pal.borderSubtle.WithAlphaF(theme.disabledOpacity)
    );
    const keel::f32 sepStrokeW = opts.separator.width;

    if (opts.separator.placement == HeadingSeparator::Bottom) {
        const keel::f32 sepPad = keel::max(4.0f, m.gap * 0.75f);
        keel::f32 contentBottom = g.bounds.y;
        if (g.hasTitleBar) contentBottom = keel::max(contentBottom, g.titleBarRect.Bottom());
        if (g.hasDesc)     contentBottom = keel::max(contentBottom, g.descRect.Bottom());
        const keel::f32 sepY = contentBottom + sepPad;
        theme.Painter().DrawDivider(canvas, theme, Vec2(g.bounds.x, sepY), Vec2(g.bounds.Right(), sepY), sepColor, sepStrokeW);
    } else if (opts.separator.placement == HeadingSeparator::Trailing && g.hasTitleBar && g.titleBarRect.h > 0.0f) {
        const keel::f32 lineY = g.titleBarRect.y + g.titleBarRect.h * 0.5f;
        if (opts.layout == HeadingLayout::Inline && g.hasDesc && g.descRect.w > 0.0f) {
            const keel::f32 bridgeStart = barRightX + m.gap;
            const keel::f32 bridgeEnd = g.descRect.x - m.gap;
            if (bridgeEnd > bridgeStart + 4.0f) {
                detail::Separator::RenderLine(canvas, Vec2(bridgeStart, lineY), Vec2(bridgeEnd, lineY), sepColor, sepStrokeW);
            }
        } else {
            const keel::f32 leftEnd = barLeftX - m.gap;
            const keel::f32 rightStart = barRightX + m.gap;

            if (leftEnd > g.bounds.x + 4.0f) {
                detail::Separator::RenderLine(canvas, Vec2(g.bounds.x, lineY), Vec2(leftEnd, lineY), sepColor, sepStrokeW);
            }
            if (g.bounds.Right() > rightStart + 4.0f) {
                detail::Separator::RenderLine(canvas, Vec2(rightStart, lineY), Vec2(g.bounds.Right(), lineY), sepColor, sepStrokeW);
            }
        }
    }
}

} // namespace guikit
