#include "guikit/widgets/Heading.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/layout/Node.hpp"

namespace guikit {

namespace {

struct HeadingMetrics {
    float titleFontSize;
    float subtitleFontSize;
    float badgeFontSize;
    float iconSize;
    float gap;
    float badgeGap;
    float lineGap;
};

[[nodiscard]] constexpr HeadingMetrics GetHeadingMetrics(UiSize size) noexcept {
    switch (size) {
        case UiSize::None:
        case UiSize::Md: return { 16.0f, 13.0f, 11.0f, 16.0f, 6.0f, 8.0f, 2.0f };
        case UiSize::Xs: return { 13.0f, 10.0f,  9.0f, 12.0f, 4.0f, 6.0f, 1.5f };
        case UiSize::Sm: return { 14.0f, 11.0f, 10.0f, 14.0f, 5.0f, 6.0f, 2.0f };
        case UiSize::Lg: return { 18.0f, 14.0f, 12.0f, 18.0f, 8.0f, 8.0f, 3.0f };
        case UiSize::Xl: return { 22.0f, 15.0f, 13.0f, 22.0f, 8.0f, 10.0f, 4.0f };
    }
    return { 16.0f, 13.0f, 11.0f, 16.0f, 6.0f, 8.0f, 2.0f };
}

} // namespace

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
    node->text = ctx.InternString(opts.title);
    node->headingOpts = opts;
    node->headingOpts.title = node->text;
    if (!opts.subtitle.empty()) {
        node->headingOpts.subtitle = ctx.InternString(opts.subtitle);
    }
    if (!opts.badge.empty()) {
        node->headingOpts.badge = ctx.InternString(opts.badge);
    }

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 Heading::Measure(const HeadingOptions& opts, UiSize size, ICanvas* canvas) {
    const HeadingMetrics m = GetHeadingMetrics(size);
    const std::string_view sub = opts.subtitle;

    float titleLineW = 0.0f;
    float titleLineH = 0.0f;

    if (opts.icon.IsValid()) {
        titleLineW += m.iconSize;
        titleLineH = std::max(titleLineH, m.iconSize);
    }

    if (!opts.title.empty()) {
        if (titleLineW > 0.0f) titleLineW += m.gap;
        float textW = 0.0f;
        if (canvas) {
            textW = canvas->MeasureText(opts.title, m.titleFontSize).x;
        } else {
            textW = static_cast<float>(opts.title.length()) * (m.titleFontSize * 0.58f);
        }
        titleLineW += textW;
        titleLineH = std::max(titleLineH, m.titleFontSize * 1.25f);
    }

    if (!opts.badge.empty()) {
        if (titleLineW > 0.0f) titleLineW += m.badgeGap;
        float badgeTextW = 0.0f;
        if (canvas) {
            badgeTextW = canvas->MeasureText(opts.badge, m.badgeFontSize).x;
        } else {
            badgeTextW = static_cast<float>(opts.badge.length()) * (m.badgeFontSize * 0.55f);
        }
        const float badgeW = badgeTextW + 12.0f;
        titleLineW += badgeW;
        titleLineH = std::max(titleLineH, m.badgeFontSize * 1.4f);
    }

    float subW = 0.0f;
    float subH = 0.0f;
    if (!sub.empty()) {
        if (canvas) {
            subW = canvas->MeasureText(sub, m.subtitleFontSize).x;
        } else {
            subW = static_cast<float>(sub.length()) * (m.subtitleFontSize * 0.58f);
        }
        subH = m.subtitleFontSize * 1.25f;
    }

    const float totalW = std::max(titleLineW, subW);
    const float totalH = (subH > 0.0f && titleLineH > 0.0f)
        ? (titleLineH + m.lineGap + subH)
        : (titleLineH + subH);

    return {totalW, totalH};
}

void Heading::Render(
    ICanvas& canvas,
    Rect bounds,
    const HeadingOptions& opts,
    UiSize size,
    ThemeMode theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const HeadingMetrics m = GetHeadingMetrics(size);
    const auto pal = GetThemePalette(theme);
    const std::string_view sub = opts.subtitle;

    const Color titleColor = (opts.variant != UiVariant::Default)
        ? GetVariantColors(opts.variant, theme).text
        : pal.textPrimary;
    const Color subtitleColor = pal.textSecondary;

    float titleLineH = 0.0f;
    if (opts.icon.IsValid()) titleLineH = std::max(titleLineH, m.iconSize);
    if (!opts.title.empty()) titleLineH = std::max(titleLineH, m.titleFontSize * 1.25f);
    if (!opts.badge.empty()) titleLineH = std::max(titleLineH, m.badgeFontSize * 1.4f);

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
        float titleW = canvas.MeasureText(opts.title, m.titleFontSize).x;
        const Rect titleR(curX, topY, titleW, titleLineH);
        canvas.DrawTextBox(opts.title, titleR, m.titleFontSize, titleColor, 0.0f, 0.5f);
        curX += titleW;
    }

    // 3. Draw Badge
    if (!opts.badge.empty()) {
        curX += m.badgeGap;
        float badgeTextW = canvas.MeasureText(opts.badge, m.badgeFontSize).x;
        const float badgePadH = 6.0f;
        const float badgeW = badgeTextW + badgePadH * 2.0f;
        const float badgeH = m.badgeFontSize + 4.0f;
        const float badgeY = topY + (titleLineH - badgeH) * 0.5f;

        const Color badgeBg = (opts.variant != UiVariant::Default)
            ? GetVariantColors(opts.variant, theme).bgActive
            : (theme == ThemeMode::Dark ? Color::Hex(0x33415580) : Color::Hex(0xE2E8F0FF));
        const Color badgeTextCol = (opts.variant != UiVariant::Default)
            ? GetVariantColors(opts.variant, theme).text
            : pal.textPrimary;

        canvas.DrawRoundRect(Rect(curX, badgeY, badgeW, badgeH), badgeH * 0.5f, badgeBg);
        canvas.DrawTextBox(opts.badge, Rect(curX, badgeY, badgeW, badgeH), m.badgeFontSize, badgeTextCol, 0.5f, 0.5f);
    }

    // 4. Draw Subtitle
    if (!sub.empty()) {
        const float subY = topY + titleLineH + m.lineGap;
        const float subH = m.subtitleFontSize * 1.25f;
        const Rect subR(bounds.x, subY, bounds.w, subH);
        canvas.DrawTextBox(sub, subR, m.subtitleFontSize, subtitleColor, 0.0f, 0.5f);
    }
}

} // namespace guikit
