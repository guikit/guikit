#include "guikit/panels/accordion.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/context.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"

namespace guikit {

AccordionScope detail::Accordion::Open(
    Context& ctx,
    bool* externalOpen,
    const AccordionOptions& opts,
    keel::SourceLocation loc
) {
    const Id idOuter = ctx.MakeChildId(loc);
    const Id idHeader = Id{detail::HashArgs(idOuter, 0xACC0'4EAD'0001ULL)};
    const Id idBody = Id{detail::HashArgs(idOuter, 0xACC0'B0D1'0002ULL)};

    const UiSize headerSize = opts.size;
    Node* outerNode = ctx.CreateNode(NodeType::Accordion, idOuter, opts.disabled);
    const UiTheme& theme = outerNode->GetTheme(ctx.GetTheme());
    const bool activeDisabled = outerNode->disabled;

    bool isOpen = opts.defaultOpen;
    if (externalOpen != nullptr) {
        isOpen = *externalOpen;
    } else {
        AccordionState& st = ctx.GetOrCreatePersistentState<AccordionState>(idOuter, AccordionState{opts.defaultOpen});
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
            ctx.GetOrCreatePersistentState<AccordionState>(idOuter, AccordionState{opts.defaultOpen}).open = isOpen;
        }
        ctx.InvalidateAll();
    }

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    outerNode->direction = FlexDirection::Column;
    outerNode->widthMode = SizeMode::Stretch;
    outerNode->heightMode = SizeMode::Fit;
    outerNode->gap = 0.0f;
    outerNode->uiSize = headerSize;

    ctx.FinishContainer(outerNode);

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
        keel::as<keel::u8>(opts.chevronPlacement),
        opts.background,
        opts.border,
        isOpen,
        activeDisabled,
        keel::as<keel::u8>(opts.icon.GetType()),
        keel::as<keel::u8>(opts.icon.GetId()),
        hData->opts.icon.GetPath()
    );
    headerNode->measureFn = [](const Node* n, keel::f32 /*availableWidth*/, ICanvas* canvas, const UiTheme& th) noexcept -> Vec2 {
        const auto* d = n->As<AccordionHeaderData>();
        return detail::Accordion::MeasureHeader(d ? d->opts : AccordionOptions{}, n->uiSize, canvas, th);
    };
    headerNode->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& th) noexcept {
        const auto* d = n->As<AccordionHeaderData>();
        detail::Accordion::RenderHeader(
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

        const keel::f32 bodyDensity = bodyNode->GetDensity();
        bodyNode->direction = FlexDirection::Column;
        bodyNode->widthMode = SizeMode::Stretch;
        bodyNode->heightMode = SizeMode::Fit;
        bodyNode->gap = theme.ResolveGap(opts.gap, UiSize::Md, bodyDensity);
        bodyNode->padding = opts.padding.has_value()
            ? theme.ResolvePadding(*opts.padding, UiSize::Md, bodyDensity)
            : Padding(0.0f, 8.0f * bodyDensity, 0.0f, 6.0f * bodyDensity);
        bodyNode->uiSize = UiSize::Md;
        bodyNode->disabled = activeDisabled;

        ctx.PushContainer(bodyNode);
        return AccordionScope(&ctx, bodyNode, res, true);
    }

    ctx.PopContainer(outerNode);
    return AccordionScope(nullptr, nullptr, res, false);
}

Vec2 detail::Accordion::MeasureHeader(
    const AccordionOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const ComponentMetrics& m = theme.GetMetrics(size);

    const keel::f32 titleFont = theme.ResolveFontSize(opts.label, TextRole::LabelLarge, size);
    const keel::f32 descFont = theme.ResolveFontSize(opts.description, TextRole::LabelSmall, size);
    const keel::f32 badgeFont = theme.ResolveFontSize(opts.badge, TextRole::LabelSmall, size);
    const keel::f32 padX = keel::max(8.0f, m.paddingX * 0.70f);
    const keel::f32 padY = keel::max(5.0f, m.paddingY * 0.75f);
    const keel::f32 itemGap = keel::max(6.0f, m.itemGap);
    const keel::f32 chevronSize = keel::max(11.0f, titleFont * 0.85f);
    const keel::f32 iconSize = keel::max(13.0f, titleFont);

    keel::f32 rowW = padX * 2.0f + chevronSize + itemGap;
    if (opts.icon.IsValid()) {
        rowW += iconSize + itemGap;
    }
    if (opts.label.HasContentOrReserve()) {
        rowW += TextUtil::MeasureWidth(opts.label, titleFont, canvas);
    }
    if (opts.badge.HasContentOrReserve()) {
        const keel::f32 badgePadX = keel::max(5.0f, badgeFont * 0.45f);
        rowW += itemGap + TextUtil::MeasureWidth(opts.badge, badgeFont, canvas) + badgePadX * 2.0f;
    }

    keel::f32 titleRowH = keel::max(titleFont * 1.30f, chevronSize);
    if (opts.icon.IsValid()) {
        titleRowH = keel::max(titleRowH, iconSize);
    }

    keel::f32 totalH = padY * 2.0f + titleRowH;
    if (opts.description.HasContentOrReserve()) {
        const keel::f32 descW = padX * 2.0f + chevronSize + itemGap + TextUtil::MeasureWidth(opts.description, descFont, canvas);
        rowW = keel::max(rowW, descW);
        totalH += m.lineGap + descFont * 1.25f;
    }

    return Vec2{rowW, totalH};
}

void detail::Accordion::RenderHeader(
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

    const keel::f32 titleFont = theme.ResolveFontSize(opts.label, TextRole::LabelLarge, size);
    const keel::f32 descFont = theme.ResolveFontSize(opts.description, TextRole::LabelSmall, size);
    const keel::f32 badgeFont = theme.ResolveFontSize(opts.badge, TextRole::LabelSmall, size);
    const keel::f32 padX = keel::max(8.0f, m.paddingX * 0.70f);
    const keel::f32 padY = keel::max(5.0f, m.paddingY * 0.75f);
    const keel::f32 itemGap = keel::max(6.0f, m.itemGap);
    const keel::f32 chevronSize = keel::max(11.0f, titleFont * 0.85f);
    const keel::f32 iconSize = keel::max(13.0f, titleFont);
    const keel::f32 radius = opts.border.radius.has_value()
        ? *opts.border.radius
        : keel::max(4.0f, theme.GetRadius(size) * 0.75f);

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
        if (opts.background.IsSurface() && opts.background.HasAlphaScale()) {
            bgColor = bgColor.ScaleAlpha(opts.background.alphaScale);
        }
    } else {
        bgColor = theme.ResolveColor(
            opts.background,
            hovered,
            pressed,
            disabled,
            pal.surface,
            pal.surfaceHover,
            pal.surfaceActive,
            pal.surface.WithAlphaF(0.45f)
        );
    }

    const keel::f32 strokeW = opts.border.HasStroke() ? opts.border.width : 0.0f;
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

    keel::f32 titleRowH = keel::max(titleFont * 1.30f, chevronSize);
    if (opts.icon.IsValid()) {
        titleRowH = keel::max(titleRowH, iconSize);
    }
    const keel::f32 titleRowY = opts.description.empty()
        ? (bounds.y + (bounds.h - titleRowH) * 0.5f)
        : (bounds.y + padY);
    const keel::f32 titleCenterY = titleRowY + titleRowH * 0.5f;

    keel::f32 leftX = bounds.x + padX;
    keel::f32 rightX = bounds.Right() - padX;

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
    if (!opts.badge.empty() && rightX > leftX + 20.0f) {
        const keel::f32 badgePadX = keel::max(5.0f, badgeFont * 0.45f);
        const keel::f32 badgeTextW = TextUtil::MeasureWidth(opts.badge, badgeFont, &canvas);
        const keel::f32 badgeW = badgeTextW + badgePadX * 2.0f;
        const keel::f32 badgeH = keel::min(titleRowH, badgeFont + 5.0f);
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
    const keel::f32 availTitleW = keel::max(0.0f, rightX - leftX);
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
        const keel::f32 descH = descFont * 1.25f;
        const keel::f32 descY = titleRowY + titleRowH + m.lineGap;
        canvas.DrawTextBox(
            opts.description.text,
            Rect{leftX, descY, keel::max(0.0f, bounds.Right() - padX - leftX), descH},
            descFont,
            descColor,
            0.0f,
            0.5f
        );
    }
}

} // namespace guikit
