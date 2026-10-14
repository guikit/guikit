#include "guikit/widgets/Heading.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

Response Heading::Run(
    Context& ctx,
    const HeadingOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = ctx.IsDisabled();

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
    }

    Node* node = ctx.CreateNode(NodeType::Heading, id);
    auto* hOpts = ctx.AllocPayload<HeadingOptions>(opts);
    hOpts->title = ctx.AllocText(opts.title);
    hOpts->subtitle = ctx.AllocText(opts.subtitle);
    hOpts->badge = ctx.AllocText(opts.badge);
    node->text = hOpts->title.text;
    node->SetPayload(hOpts);
    node->visualHash = detail::HashArgs(
        opts.variant,
        hOpts->title,
        hOpts->subtitle,
        hOpts->badge,
        opts.divider,
        static_cast<uint8_t>(opts.dividerStyle),
        opts.wordWrap,
        static_cast<uint8_t>(opts.icon.GetType()),
        static_cast<uint8_t>(opts.icon.GetId()),
        opts.icon.GetPath()
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* opts = n->As<HeadingOptions>();
        return Heading::Measure(opts ? *opts : HeadingOptions{}, n->uiSize, availableWidth, canvas, theme);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* opts = n->As<HeadingOptions>();
        Heading::Render(
            canvas,
            n->computedRect,
            opts ? *opts : HeadingOptions{},
            n->uiSize,
            theme
        );
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Heading::Measure(const HeadingOptions& opts, UiSize size, float availableWidth, ICanvas* canvas, const UiTheme& theme) {
    const HeadingMetrics& m = theme.GetHeading(size);
    const float titleFont = opts.title.role.has_value() ? theme.GetTypography(*opts.title.role).fontSize : m.titleFontSize;
    const float subFont = opts.subtitle.role.has_value() ? theme.GetTypography(*opts.subtitle.role).fontSize : m.subtitleFontSize;
    const float badgeFont = opts.badge.role.has_value() ? theme.GetTypography(*opts.badge.role).fontSize : m.badgeFontSize;

    float titleLineW = 0.0f;
    float titleLineH = 0.0f;

    if (opts.icon.IsValid()) {
        titleLineW += m.iconSize;
        titleLineH = std::max(titleLineH, m.iconSize);
    }

    if (opts.title.HasContentOrReserve()) {
        if (titleLineW > 0.0f) titleLineW += m.gap;
        const float textW = TextUtil::MeasureWidth(opts.title, titleFont, canvas);
        titleLineW += textW;
        const float titleH = canvas ? canvas->GetLineHeight(titleFont) : theme.GetLineHeight(titleFont);
        titleLineH = std::max(titleLineH, titleH);
    }

    if (opts.badge.HasContentOrReserve()) {
        if (titleLineW > 0.0f) titleLineW += m.badgeGap;
        const float badgeTextW = TextUtil::MeasureWidth(opts.badge, badgeFont, canvas);
        const float badgeW = badgeTextW + 12.0f;
        titleLineW += badgeW;
        titleLineH = std::max(titleLineH, badgeFont * 1.4f);
    }

    float subW = 0.0f;
    float subH = 0.0f;
    if (opts.subtitle.HasContentOrReserve()) {
        const float subLineH = canvas ? canvas->GetLineHeight(subFont) : theme.GetLineHeight(subFont);
        UiText subSpec = opts.subtitle;
        if (opts.wordWrap) {
            subSpec.wrap = TextWrap::Word;
        }
        subW = TextUtil::MeasureWidth(subSpec, subFont, canvas);
        if (subSpec.wrap == TextWrap::Word && availableWidth > 0.0f) {
            subW = std::min(subW, availableWidth);
        }
        subH = TextUtil::MeasureUiTextHeight(subSpec, availableWidth, subFont, subLineH, canvas);
    }

    const float totalW = std::max(titleLineW, subW);
    float totalH = (subH > 0.0f && titleLineH > 0.0f)
        ? (titleLineH + m.lineGap + subH)
        : (titleLineH + subH);

    if (opts.divider && opts.dividerStyle == HeadingDivider::Bottom) {
        const float divPad = std::max(4.0f, m.gap * 0.75f);
        totalH += divPad + 1.0f + divPad * 0.5f;
    }

    return {totalW, totalH};
}

Vec2 Heading::Measure(const HeadingOptions& opts, UiSize size, float availableWidth, ICanvas* canvas) {
    return Measure(opts, size, availableWidth, canvas, UiTheme::Dark());
}

float Heading::ResolveHeight(
    const HeadingOptions& opts,
    UiSize size,
    float availableWidth,
    ICanvas* canvas,
    const UiTheme& theme
) {
    return Measure(opts, size, availableWidth, canvas, theme).y;
}

void Heading::Render(
    ICanvas& canvas,
    Rect bounds,
    const HeadingOptions& opts,
    UiSize size,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const HeadingMetrics& m = theme.GetHeading(size);
    const auto& pal = theme.palette;
    const float titleFont = opts.title.role.has_value() ? theme.GetTypography(*opts.title.role).fontSize : m.titleFontSize;
    const float subFont = opts.subtitle.role.has_value() ? theme.GetTypography(*opts.subtitle.role).fontSize : m.subtitleFontSize;
    const float badgeFont = opts.badge.role.has_value() ? theme.GetTypography(*opts.badge.role).fontSize : m.badgeFontSize;

    const UiVariant defaultTitleVar = (opts.variant != UiVariant::Default) ? opts.variant : UiVariant::Default;
    const Color titleColor = theme.ResolveTextColor(opts.title, defaultTitleVar, false);
    const Color subtitleColor = theme.ResolveTextColor(opts.subtitle, UiVariant::Secondary, false);

    float titleLineH = 0.0f;
    if (opts.icon.IsValid()) titleLineH = std::max(titleLineH, m.iconSize);
    if (opts.title.HasContentOrReserve()) titleLineH = std::max(titleLineH, canvas.GetLineHeight(titleFont));
    if (opts.badge.HasContentOrReserve()) titleLineH = std::max(titleLineH, badgeFont * 1.4f);

    float curX = bounds.x;
    const float topY = bounds.y;

    // 1. Draw Icon
    if (opts.icon.IsValid()) {
        const float iconY = topY + (titleLineH - m.iconSize) * 0.5f;
        opts.icon.Draw(canvas, Rect(curX, iconY, m.iconSize, m.iconSize), titleColor);
        curX += m.iconSize + m.gap;
    }

    // 2. Draw Title
    if (!opts.title.empty()) {
        const float badgeReserveW = opts.badge.empty() ? 0.0f : (m.badgeGap + TextUtil::MeasureWidth(opts.badge, badgeFont, &canvas) + 12.0f);
        const float titleAvailW = std::max(0.0f, bounds.Right() - curX - badgeReserveW);
        float titleW = canvas.MeasureText(opts.title.text, titleFont).x;
        std::string displayTitle(opts.title.text);
        if (titleW > titleAvailW + kTextSubpixelTolerance && titleAvailW > 0.0f) {
            displayTitle = TextUtil::TruncateWithEllipsis(opts.title.text, titleAvailW, titleFont, &canvas);
            titleW = canvas.MeasureText(displayTitle, titleFont).x;
        }
        const Rect titleR(curX, topY, titleAvailW, titleLineH);
        canvas.DrawTextBox(displayTitle, titleR, titleFont, titleColor, 0.0f, 0.5f);
        curX += titleW;
    }

    // 3. Draw Badge
    if (!opts.badge.empty()) {
        curX += m.badgeGap;
        const float badgeTextW = TextUtil::MeasureWidth(opts.badge, badgeFont, &canvas);
        const float badgePadH = 6.0f;
        const float badgeW = badgeTextW + badgePadH * 2.0f;
        const float badgeH = badgeFont + 4.0f;
        const float badgeY = topY + (titleLineH - badgeH) * 0.5f;

        const UiVariant badgeVar = opts.badge.variant.value_or(opts.variant);
        const Color badgeBg = (badgeVar != UiVariant::Default)
            ? theme.GetVariant(badgeVar).bgActive
            : pal.surfaceHover;
        const Color badgeTextCol = (badgeVar != UiVariant::Default)
            ? theme.GetVariant(badgeVar).text
            : pal.textPrimary;

        canvas.DrawRoundRect(Rect(curX, badgeY, badgeW, badgeH), badgeH * 0.5f, badgeBg);
        canvas.DrawTextBox(opts.badge.text, Rect(curX, badgeY, badgeW, badgeH), badgeFont, badgeTextCol, 0.5f, 0.5f);
    }

    // 4. Draw Subtitle
    float subTotalH = 0.0f;
    if (opts.subtitle.HasContentOrReserve()) {
        const float subY = topY + titleLineH + m.lineGap;
        const float subLineH = canvas.GetLineHeight(subFont);
        UiText subSpec = opts.subtitle;
        if (opts.wordWrap) {
            subSpec.wrap = TextWrap::Word;
        }
        subTotalH = TextUtil::MeasureUiTextHeight(subSpec, bounds.w, subFont, subLineH, &canvas);
        if (!subSpec.empty()) {
            const Rect subR(bounds.x, subY, bounds.w, subTotalH);
            TextUtil::DrawUiText(canvas, subSpec, subR, subFont, subLineH, subtitleColor, 0.0f, 0.5f);
        }
    }

    // 5. Draw Divider
    if (opts.divider) {
        const Color divColor = (opts.variant != UiVariant::Default)
            ? theme.GetVariant(opts.variant).border
            : pal.borderSubtle;

        if (opts.dividerStyle == HeadingDivider::Bottom) {
            const float divPad = std::max(4.0f, m.gap * 0.75f);
            const float textH = (subTotalH <= 0.0f) ? titleLineH : (titleLineH + m.lineGap + subTotalH);
            const float divY = std::floor(topY + textH + divPad) + 0.5f;
            canvas.DrawLine(
                Vec2(bounds.x, divY),
                Vec2(bounds.Right(), divY),
                divColor,
                1.0f
            );
        } else if (opts.dividerStyle == HeadingDivider::Trailing) {
            const float lineStartX = curX + m.gap;
            const float lineEndX = bounds.Right();
            if (lineEndX > lineStartX + 4.0f) {
                const float lineY = std::floor(topY + titleLineH * 0.5f) + 0.5f;
                canvas.DrawLine(
                    Vec2(lineStartX, lineY),
                    Vec2(lineEndX, lineY),
                    divColor,
                    1.0f
                );
            }
        }
    }
}

} // namespace guikit
