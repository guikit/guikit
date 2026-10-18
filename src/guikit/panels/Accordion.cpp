#include "guikit/panels/Accordion.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

AccordionScope Accordion::Open(
    Context& ctx,
    bool* externalOpen,
    const AccordionOptions& opts,
    std::source_location loc
) {
    const Id idOuter = ctx.MakeChildId(loc);
    const Id idHeader = Id{detail::HashArgs(idOuter, 0xACC0'4EAD'0001ULL)};
    const Id idBody = Id{detail::HashArgs(idOuter, 0xACC0'B0D1'0002ULL)};

    const UiSize parentSize = ctx.GetCurrentSize();
    const UiSize headerSize = parentSize;
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const UiTheme& theme = ctx.GetTheme();

    bool isOpen = opts.defaultOpen;
    if (externalOpen != nullptr) {
        isOpen = *externalOpen;
    } else {
        AccordionState& st = ctx.GetOrCreateState<AccordionState>(idOuter, AccordionState{opts.defaultOpen});
        isOpen = st.open;
    }

    Response res = ctx.Interact(idHeader);
    if (activeDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.doubleClicked = false;
        res.rightClicked = false;
    } else if (res.clicked) {
        isOpen = !isOpen;
        res.changed = true;
        if (externalOpen != nullptr) {
            *externalOpen = isOpen;
        } else {
            ctx.GetOrCreateState<AccordionState>(idOuter, AccordionState{opts.defaultOpen}).open = isOpen;
        }
        ctx.InvalidateAll();
    }

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    Node* outerNode = ctx.CreateNode(NodeType::Accordion, idOuter);
    outerNode->direction = FlexDirection::Column;
    outerNode->widthMode = SizeMode::Stretch;
    outerNode->heightMode = SizeMode::Fit;
    outerNode->gap = 0.0f;
    outerNode->uiSize = headerSize;
    outerNode->disabled = activeDisabled;
    outerNode->debugBoundsOverride = opts.debugBounds;
    outerNode->inspectModeOverride = opts.inspectMode;

    ctx.PushScale(activeScale);
    ctx.PushSize(headerSize);
    ctx.PushDisabled(activeDisabled);
    ctx.PushContainer(outerNode);

    Node* headerNode = ctx.CreateNode(NodeType::AccordionHeader, idHeader);
    auto* hData = ctx.AllocPayload<AccordionHeaderData>();
    hData->opts = opts;
    hData->opts.label = ctx.AllocText(opts.label);
    hData->opts.description = ctx.AllocText(opts.description);
    hData->opts.badge = ctx.AllocText(opts.badge);
    hData->opts.tooltip = ctx.AllocText(opts.tooltip);
    if (!opts.icon.GetPath().empty()) {
        hData->opts.icon = Icon::File(ctx.AllocString(opts.icon.GetPath()));
    }
    hData->open = isOpen;

    headerNode->text = hData->opts.label.text;
    headerNode->SetPayload(hData);
    headerNode->uiSize = headerSize;
    headerNode->disabled = activeDisabled;
    headerNode->hovered = res.hovered;
    headerNode->pressed = res.pressed;
    headerNode->checked = isOpen;
    headerNode->widthMode = SizeMode::Stretch;
    headerNode->heightMode = SizeMode::Fit;
    headerNode->visualHash = detail::HashArgs(
        hData->opts.label,
        hData->opts.description,
        hData->opts.badge,
        static_cast<uint8_t>(opts.chevronPlacement),
        opts.background,
        opts.border,
        isOpen,
        activeDisabled,
        static_cast<uint8_t>(opts.icon.GetType()),
        static_cast<uint8_t>(opts.icon.GetId()),
        hData->opts.icon.GetPath()
    );
    headerNode->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* canvas, const UiTheme& th) noexcept -> Vec2 {
        const auto* d = n->As<AccordionHeaderData>();
        return Accordion::MeasureHeader(d ? d->opts : AccordionOptions{}, n->uiSize, canvas, th);
    };
    headerNode->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& th) noexcept {
        const auto* d = n->As<AccordionHeaderData>();
        Accordion::RenderHeader(
            canvas,
            n->computedRect,
            d ? d->opts : AccordionOptions{},
            n->uiSize,
            d ? d->open : n->checked,
            n->hovered,
            n->pressed,
            n->disabled,
            th
        );
    };

    if (isOpen) {
        Node* bodyNode = ctx.CreateNode(NodeType::Column, idBody);
        ctx.PopContainer(outerNode);

        bodyNode->theme = pushedTheme;
        bodyNode->direction = FlexDirection::Column;
        bodyNode->widthMode = SizeMode::Stretch;
        bodyNode->heightMode = SizeMode::Fit;
        bodyNode->gap = theme.ResolveGap(opts.gap, parentSize);
        bodyNode->padding = opts.padding.has_value()
            ? theme.ResolvePadding(*opts.padding, parentSize)
            : Padding(0.0f, theme.Scale(8.0f), 0.0f, theme.Scale(6.0f));
        bodyNode->uiSize = parentSize;
        bodyNode->disabled = activeDisabled;
        bodyNode->debugBoundsOverride = opts.debugBounds;
        bodyNode->inspectModeOverride = opts.inspectMode;

        ctx.PushScale(activeScale);
        ctx.PushSize(parentSize);
        ctx.PushDisabled(activeDisabled);
        ctx.PushContainer(bodyNode);
        return AccordionScope(&ctx, bodyNode, res, true);
    }

    outerNode->theme = pushedTheme;
    ctx.PopContainer(outerNode);
    return AccordionScope(nullptr, nullptr, res, false);
}

Vec2 Accordion::MeasureHeader(
    const AccordionOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const HeadingMetrics& hm = theme.GetHeading(size);

    const float titleFont = theme.ResolveFontSize(opts.label, TextRole::LabelLarge, size);
    const float descFont = theme.ResolveFontSize(opts.description, TextRole::LabelSmall, size);
    const float badgeFont = theme.ResolveFontSize(opts.badge, TextRole::LabelSmall, size);
    const float padX = std::max(theme.Scale(8.0f), m.paddingX * 0.70f);
    const float padY = std::max(theme.Scale(5.0f), m.paddingY * 0.75f);
    const float itemGap = std::max(theme.Scale(6.0f), hm.gap);
    const float chevronSize = std::max(theme.Scale(11.0f), titleFont * 0.85f);
    const float iconSize = std::max(theme.Scale(13.0f), titleFont);

    float rowW = padX * 2.0f + chevronSize + itemGap;
    if (opts.icon.IsValid()) {
        rowW += iconSize + itemGap;
    }
    if (opts.label.HasContentOrReserve()) {
        rowW += TextUtil::MeasureWidth(opts.label, titleFont, canvas);
    }
    if (opts.badge.HasContentOrReserve()) {
        const float badgePadX = std::max(theme.Scale(5.0f), badgeFont * 0.45f);
        rowW += itemGap + TextUtil::MeasureWidth(opts.badge, badgeFont, canvas) + badgePadX * 2.0f;
    }

    float titleRowH = std::max(titleFont * 1.30f, chevronSize);
    if (opts.icon.IsValid()) {
        titleRowH = std::max(titleRowH, iconSize);
    }

    float totalH = padY * 2.0f + titleRowH;
    if (opts.description.HasContentOrReserve()) {
        const float descW = padX * 2.0f + chevronSize + itemGap + TextUtil::MeasureWidth(opts.description, descFont, canvas);
        rowW = std::max(rowW, descW);
        totalH += hm.lineGap + descFont * 1.25f;
    }

    return Vec2{rowW, totalH};
}

void Accordion::RenderHeader(
    ICanvas& canvas,
    Rect bounds,
    const AccordionOptions& opts,
    UiSize size,
    bool open,
    bool hovered,
    bool pressed,
    bool disabled,
    const UiTheme& theme
) noexcept {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const auto& pal = theme.palette;
    const ComponentMetrics& m = theme.GetMetrics(size);
    const HeadingMetrics& hm = theme.GetHeading(size);

    const float titleFont = theme.ResolveFontSize(opts.label, TextRole::LabelLarge, size);
    const float descFont = theme.ResolveFontSize(opts.description, TextRole::LabelSmall, size);
    const float badgeFont = theme.ResolveFontSize(opts.badge, TextRole::LabelSmall, size);
    const float padX = std::max(theme.Scale(8.0f), m.paddingX * 0.70f);
    const float padY = std::max(theme.Scale(5.0f), m.paddingY * 0.75f);
    const float itemGap = std::max(theme.Scale(6.0f), hm.gap);
    const float chevronSize = std::max(theme.Scale(11.0f), titleFont * 0.85f);
    const float iconSize = std::max(theme.Scale(13.0f), titleFont);
    const float radius = opts.border.radius.has_value()
        ? theme.Scale(*opts.border.radius)
        : std::max(theme.Scale(4.0f), theme.GetRadius(size) * 0.75f);

    // 1. Header background & border
    Color bgColor = Color::Transparent();
    if (opts.background.IsNone()) {
        if (!disabled && (hovered || pressed)) {
            bgColor = pressed ? pal.surfaceActive.WithAlphaF(0.65f) : pal.surfaceHover.WithAlphaF(0.45f);
        }
    } else if (!opts.background.HasAny() || opts.background.IsSurface()) {
        if (disabled) {
            bgColor = pal.surfaceHover.WithAlphaF(0.28f);
        } else if (pressed) {
            bgColor = pal.surfaceActive;
        } else if (hovered) {
            bgColor = pal.surfaceHover;
        } else {
            bgColor = pal.surfaceHover.WithAlphaF(0.58f);
        }
    } else {
        bgColor = opts.background.Resolve(
            hovered,
            pressed,
            disabled,
            pal.surface,
            pal.surfaceHover,
            pal.surfaceActive,
            pal.surface.WithAlphaF(0.45f)
        );
    }

    const float strokeW = opts.border.HasStroke() ? theme.Scale(opts.border.width) : 0.0f;
    Color borderCol = Color::Transparent();
    if (strokeW > 0.0f) {
        borderCol = opts.border.color.Resolve(
            hovered,
            pressed,
            disabled,
            pal.borderSubtle,
            pal.borderHover,
            pal.focusRing,
            pal.borderSubtle.WithAlphaF(0.40f)
        );
    }

    if (bgColor.a > 0 || strokeW > 0.0f) {
        canvas.DrawRoundRect(bounds, radius, bgColor, borderCol, strokeW);
    }

    // 2. Colors for text and icons
    const Color titleColor = theme.ResolveTextColor(opts.label, UiVariant::Default, disabled, hovered, pressed);
    const Color chevronColor = disabled ? pal.textDisabled : (hovered ? pal.textPrimary : pal.textSecondary);

    float titleRowH = std::max(titleFont * 1.30f, chevronSize);
    if (opts.icon.IsValid()) {
        titleRowH = std::max(titleRowH, iconSize);
    }
    const float titleRowY = opts.description.empty()
        ? (bounds.y + (bounds.h - titleRowH) * 0.5f)
        : (bounds.y + padY);
    const float titleCenterY = titleRowY + titleRowH * 0.5f;

    float leftX = bounds.x + padX;
    float rightX = bounds.Right() - padX;

    const Icon chevronIcon = open ? Icon::ChevronDown() : Icon::ChevronRight();
    if (opts.chevronPlacement == Placement::Leading) {
        chevronIcon.Draw(
            canvas,
            Rect{leftX, titleCenterY - chevronSize * 0.5f, chevronSize, chevronSize},
            chevronColor
        );
        leftX += chevronSize + itemGap;
    } else {
        rightX -= chevronSize;
        chevronIcon.Draw(
            canvas,
            Rect{rightX, titleCenterY - chevronSize * 0.5f, chevronSize, chevronSize},
            chevronColor
        );
        rightX -= itemGap;
    }

    if (opts.icon.IsValid()) {
        opts.icon.Draw(
            canvas,
            Rect{leftX, titleCenterY - iconSize * 0.5f, iconSize, iconSize},
            titleColor
        );
        leftX += iconSize + itemGap;
    }

    // 3. Optional trailing pill badge
    if (!opts.badge.empty() && rightX > leftX + theme.Scale(20.0f)) {
        const float badgePadX = std::max(theme.Scale(5.0f), badgeFont * 0.45f);
        const float badgeTextW = TextUtil::MeasureWidth(opts.badge, badgeFont, &canvas);
        const float badgeW = badgeTextW + badgePadX * 2.0f;
        const float badgeH = std::min(titleRowH, badgeFont + theme.Scale(5.0f));
        const Rect badgeRect{
            rightX - badgeW,
            titleCenterY - badgeH * 0.5f,
            badgeW,
            badgeH
        };

        Color badgeBg = pal.surfaceHover;
        Color badgeFg = pal.textPrimary;
        if (opts.badge.color.HasAny()) {
            const Color resolvedBadgeCol = opts.badge.color.Resolve(hovered, pressed, disabled, pal.textPrimary);
            badgeBg = resolvedBadgeCol.WithAlphaF(disabled ? 0.12f : 0.22f);
            badgeFg = disabled ? pal.textDisabled : resolvedBadgeCol;
        } else if (disabled) {
            badgeBg = badgeBg.WithAlphaF(0.40f);
            badgeFg = pal.textDisabled;
        }

        canvas.DrawRoundRect(badgeRect, badgeH * 0.35f, badgeBg);
        canvas.DrawTextBox(opts.badge.text, badgeRect, badgeFont, badgeFg, 0.5f, 0.5f);
        rightX = badgeRect.x - itemGap;
    }

    // 4. Title & optional subtitle
    const float availTitleW = std::max(0.0f, rightX - leftX);
    if (!opts.label.empty() && availTitleW > 1.0f) {
        canvas.DrawTextBox(
            opts.label.text,
            Rect{leftX, titleRowY, availTitleW, titleRowH},
            titleFont,
            titleColor,
            0.0f,
            0.5f
        );
    }

    if (!opts.description.empty() && availTitleW > 1.0f) {
        const Color descColor = theme.ResolveTextColor(opts.description, UiVariant::Muted, disabled, hovered, pressed);
        const float descH = descFont * 1.25f;
        const float descY = titleRowY + titleRowH + hm.lineGap;
        canvas.DrawTextBox(
            opts.description.text,
            Rect{leftX, descY, std::max(0.0f, bounds.Right() - padX - leftX), descH},
            descFont,
            descColor,
            0.0f,
            0.5f
        );
    }
}

} // namespace guikit
