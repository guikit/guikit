#include "guikit/widgets/tab_bar.hpp"
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/style_painter.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/panels/carousel.hpp"

namespace guikit {

namespace {

Rect ComputeTabCloseButtonRect(Rect bounds, const ComponentMetrics& m) noexcept {
    const keel::f32 btnSz = keel::max(12.0f, m.iconSize);
    const keel::f32 padX = keel::min(m.paddingX, bounds.w * 0.12f);
    const keel::f32 bx = bounds.Right() - keel::max(4.0f, padX * 0.65f) - btnSz;
    const keel::f32 by = bounds.y + (bounds.h - btnSz) * 0.5f;
    return Rect(bx, by, btnSz, btnSz);
}

} // namespace

TabBarResponse TabBar::Run(
    Context& ctx,
    keel::i32& activeIndex,
    keel::ArrayRef<TabItem> tabs,
    const TabBarOptions& opts,
    keel::SourceLocation loc
) {
    TabBarResponse res;
    if (tabs.empty()) return res;

    const UiSize activeSize = opts.size;
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();

    keel::Optional<ContainerScope> carouselScope;
    Node* barNode = nullptr;

    if (opts.scrollable && !opts.fullWidth) {
        CarouselOptions cOpts;
        cOpts.scrollWheel = true;
        cOpts.size = activeSize;
        cOpts.disabled = activeDisabled;

        carouselScope.emplace(ctx.Carousel(cOpts, loc));
        barNode = carouselScope->GetNode();
    } else {
        const Id barId = ctx.MakeChildId(loc);
        barNode = ctx.CreateNode(NodeType::Container, barId);
        barNode->direction = FlexDirection::Row;
        barNode->align = Alignment::Center;
        barNode->uiSize = activeSize;
        barNode->disabled = activeDisabled;
        barNode->widthMode = opts.fullWidth ? SizeMode::Stretch : SizeMode::Fit;

        ctx.PushContainer(barNode);
    }

    const UiTheme& theme = ctx.GetTheme();
    const ComponentMetrics& m = theme.GetMetrics(activeSize);
    const keel::f32 barDensity = barNode->GetDensity();
    const keel::f32 defaultTabGap = (theme.controlStyle.tabGapOverride >= 0.0f)
        ? theme.controlStyle.tabGapOverride
        : ((opts.style == TabBarStyle::Card || opts.style == TabBarStyle::Folder)
            ? 2.0f
            : ((theme.SizeFactor(activeSize) < 0.90f) ? theme.GetGap(UiSize::Xs) : theme.GetGap(UiSize::Sm)));
    const keel::f32 activeTabGap = (opts.gap.has_value() ? *opts.gap : defaultTabGap) * barDensity;

    const Color resolvedBg = opts.background.IsBackgroundEnabled(false)
        ? (opts.background.IsSurface()
            ? theme.palette.surfaceElevated.ScaleAlpha(opts.background.alphaScale)
            : theme.ResolveColor(opts.background, false, false, activeDisabled, theme.palette.surfaceElevated))
        : Color::Transparent();
    const Color resolvedBorderCol = (opts.border.width > 0.0f || opts.border.color.HasAny())
        ? theme.ResolveColor(opts.border.color, false, false, activeDisabled, theme.palette.borderSubtle)
        : Color::Transparent();
    const bool hasCustomBgOrBorder = (resolvedBg.a > 0 || resolvedBorderCol.a > 0 || opts.border.width > 0.0f);

    Padding resolvedPadding{};
    if (opts.padding.has_value()) {
        resolvedPadding = theme.ResolvePadding(opts.padding, UiSize::Md, barDensity);
    } else if (opts.style == TabBarStyle::Pill && hasCustomBgOrBorder) {
        const keel::f32 defaultPad = ((theme.SizeFactor(activeSize) < 0.90f) ? 2.0f : 3.0f) * barDensity;
        resolvedPadding = Padding{defaultPad, defaultPad, defaultPad, defaultPad};
    }

    keel::f32 resolvedRadius = 0.0f;
    if (opts.border.radius.has_value()) {
        resolvedRadius = *opts.border.radius;
    } else if (opts.style == TabBarStyle::Pill && hasCustomBgOrBorder) {
        resolvedRadius = m.cornerRadius + 2.0f;
    }

    const keel::f32 effectiveBorderWidth = (opts.border.width > 0.0f)
        ? opts.border.width
        : (resolvedBorderCol.a > 0 ? 1.0f : 0.0f);

    barNode->gap = activeTabGap;
    barNode->padding = resolvedPadding;
    barNode->heightMode = SizeMode::Fixed;
    barNode->fixedHeight = m.defaultHeight + resolvedPadding.Vertical();
    barNode->backgroundColor = resolvedBg;
    barNode->borderColor = resolvedBorderCol;
    barNode->borderWidth = effectiveBorderWidth;
    barNode->cornerRadius = resolvedRadius;
    if (resolvedRadius > 0.0f) {
        barNode->clipChildren = true;
    }
    barNode->visualHash = detail::HashArgs(
        opts.style,
        opts.showSeparator,
        resolvedBg.ToUint32(),
        resolvedBorderCol.ToUint32(),
        effectiveBorderWidth,
        resolvedRadius,
        resolvedPadding.top,
        resolvedPadding.left
    );
    const bool hasBaselineSeparator = opts.showSeparator &&
        (opts.style == TabBarStyle::Underline || opts.style == TabBarStyle::Card || opts.style == TabBarStyle::Folder);
    if (hasBaselineSeparator) {
        barNode->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& th) noexcept {
            th.Painter().DrawTabBarBaseline(canvas, th, n->computedRect);
        };
    }

    // Auto-scroll to active tab when activeIndex changes or on initial display
    keel::i32& lastActive = ctx.GetOrCreatePersistentState<keel::i32>(barNode->id, -1);
    if (lastActive != activeIndex) {
        lastActive = activeIndex;
        if (barNode->type == NodeType::Carousel) {
            if (auto* cData = barNode->As<CarouselData>()) {
                cData->scrollToActiveTab = true;
            }
        }
    }

    for (keel::usize i = 0; i < tabs.size(); ++i) {
        const TabItem& item = tabs[i];
        const Id tabId = MakeId(keel::as<keel::u32>(i), loc, barNode->id);
        const bool effectiveDisabled = activeDisabled || item.disabled;

        ctx.RegisterFocusable(tabId, effectiveDisabled);

        Response tabRes = ctx.Interact(tabId);
        bool closeHovered = false;
        bool closeClicked = false;
        if (effectiveDisabled) {
            tabRes.clicked = false;
            tabRes.hovered = false;
            tabRes.pressed = false;
        } else {
            if (item.closable && tabRes.rect.w > 0.0f && (tabRes.hovered || tabRes.clicked || tabRes.pressed)) {
                const Rect closeRect = ComputeTabCloseButtonRect(tabRes.rect, m);
                if (closeRect.Contains(ctx.GetMousePos())) {
                    closeHovered = true;
                    if (tabRes.clicked) {
                        closeClicked = true;
                        tabRes.clicked = false;
                    }
                }
            }
            if (tabRes.clicked) {
                ctx.RequestFocus(tabId);
            }
            if (ctx.IsFocused(tabId) && (ctx.ConsumeKey(Key::Space) || ctx.ConsumeKey(Key::Enter))) {
                tabRes.clicked = true;
            }
        }

        if (closeClicked) {
            res.closedIndex = i;
            ctx.RequestRepaint(2);
        }

        if (tabRes.hovered && !closeHovered && !item.tooltip.empty()) {
            ctx.SetTooltip(item.tooltip, tabRes.rect);
        }

        if (tabRes.clicked && !effectiveDisabled && activeIndex != keel::as<keel::i32>(i)) {
            activeIndex = keel::as<keel::i32>(i);
            res.changed = true;
            res.clicked = true;
            ctx.GetOrCreatePersistentState<keel::i32>(barNode->id) = activeIndex;
            if (barNode->type == NodeType::Carousel) {
                if (auto* cData = barNode->As<CarouselData>()) {
                    cData->scrollToActiveTab = true;
                }
            }
            ctx.RequestRepaint(3);
        }

        Node* tabNode = ctx.CreateNode(NodeType::TabItem, tabId);
        auto* tData = ctx.AllocPayload<TabItemData>();
        tData->item = item;
        tData->item.label = ctx.AllocText(item.label);
        tData->item.badge = ctx.AllocText(item.badge);
        tData->item.tooltip = ctx.AllocText(item.tooltip);
        tData->item.icon = ctx.AllocIcon(item.icon);
        tData->barOpts = opts;
        tData->closeHovered = closeHovered;
        tabNode->text = tData->item.label.text;
        tabNode->SetPayload(tData);
        tabNode->visualHash = detail::HashArgs(
            opts.style,
            opts.showSeparator,
            opts.color,
            opts.border.radius.value_or(-1.0f),
            item.variant,
            item.closable,
            closeHovered,
            tData->item.badge,
            tData->item.label,
            keel::as<keel::u8>(tData->item.icon.GetType()),
            keel::as<keel::u8>(tData->item.icon.GetId()),
            tData->item.icon.GetPath()
        );
        tabNode->measureFn = [](const Node* n, keel::f32 /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
            const auto* d = n->As<TabItemData>();
            return TabBar::MeasureTab(
                d ? d->item : TabItem{},
                d ? d->barOpts : TabBarOptions{},
                n->uiSize,
                canvas,
                theme
            );
        };
        tabNode->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
            const auto* d = n->As<TabItemData>();
            TabBar::RenderTab(
                canvas,
                n->computedRect,
                d ? d->item : TabItem{},
                d ? d->barOpts : TabBarOptions{},
                n->uiSize,
                n->active,
                n->hovered,
                n->pressed,
                n->disabled,
                theme,
                n->focused,
                d ? d->closeHovered : false
            );
        };
        tabNode->active = (activeIndex == keel::as<keel::i32>(i));
        tabNode->uiSize = activeSize;
        tabNode->disabled = effectiveDisabled;
        tabNode->hovered = tabRes.hovered;
        tabNode->pressed = tabRes.pressed;
        tabNode->focused = ctx.IsFocusVisible(tabId);
        tabNode->widthMode = opts.fullWidth ? SizeMode::Stretch : SizeMode::Fit;
        tabNode->heightMode = SizeMode::Stretch;
    }

    if (!carouselScope) {
        ctx.PopContainer(barNode);
        ctx.AdaptChildToContainer(barNode);
    }

    res.rect = ctx.GetPrevFrameBounds(barNode->id);
    return res;
}

Vec2 TabBar::MeasureTab(
    const TabItem& item,
    const TabBarOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const keel::f32 labelFont = theme.ResolveFontSize(item.label, TextRole::LabelLarge, size);
    const keel::f32 badgeFontSize = theme.ResolveFontSize(item.badge, m.fontSize * 0.78f);
    keel::f32 contentW = 0.0f;

    if (item.icon.IsValid()) {
        contentW += m.iconSize;
    }

    if (item.label.HasContentOrReserve()) {
        if (contentW > 0.0f) {
            contentW += 6.0f; // Gap between icon and text
        }
        contentW += TextUtil::MeasureWidth(item.label, labelFont, canvas);
    }

    if (item.badge.HasContentOrReserve()) {
        contentW += 6.0f; // Gap between text and badge
        const keel::f32 badgeTextW = TextUtil::MeasureWidth(item.badge, badgeFontSize, canvas);
        contentW += badgeTextW + 12.0f; // Badge padding
    }

    if (item.closable) {
        const keel::f32 closeBtnSz = keel::max(12.0f, m.iconSize);
        contentW += (contentW > 0.0f ? 6.0f : 0.0f) + closeBtnSz;
    }

    const keel::f32 padX = m.paddingX + (opts.style == TabBarStyle::Folder ? 4.0f : 0.0f);
    const keel::f32 totalW = contentW + padX * 2.0f;

    return {keel::max(m.defaultHeight, totalW), m.defaultHeight};
}

Vec2 TabBar::MeasureTab(
    const TabItem& item,
    const TabBarOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    return MeasureTab(item, opts, size, canvas, UiTheme::Dark());
}

void TabBar::RenderTab(
    ICanvas& canvas,
    Rect bounds,
    const TabItem& item,
    const TabBarOptions& opts,
    UiSize size,
    bool active,
    bool hovered,
    bool pressed,
    bool disabled,
    const UiTheme& theme,
    bool focused,
    bool closeHovered
) {
    (void)pressed;
    const ComponentMetrics& m = theme.GetMetrics(size);
    const auto& pal = theme.palette;
    const keel::f32 labelFont = theme.ResolveFontSize(item.label, TextRole::LabelLarge, size);
    const keel::f32 badgeFontSize = theme.ResolveFontSize(item.badge, m.fontSize * 0.78f);

    canvas.PushClip(bounds);

    const Color defaultAccent = (item.variant != UiVariant::Default)
        ? theme.GetVariant(item.variant).bg
        : pal.accent;
    const Color accentCol = opts.color.HasAny()
        ? theme.ResolveColor(opts.color, hovered, active, disabled, defaultAccent)
        : defaultAccent;

    TabChromeStyle chromeStyle = TabChromeStyle::Pill;
    if (opts.style == TabBarStyle::Underline) chromeStyle = TabChromeStyle::Underline;
    else if (opts.style == TabBarStyle::Card) chromeStyle = TabChromeStyle::Card;
    else if (opts.style == TabBarStyle::Folder) chromeStyle = TabChromeStyle::Folder;

    keel::f32 tabRadius = opts.border.radius.has_value() ? *opts.border.radius : m.cornerRadius;
    if (opts.style == TabBarStyle::Pill) {
        const bool hasCustomBgOrBorder = (opts.background.IsBackgroundEnabled(false) || opts.border.color.HasAny() || opts.border.width > 0.0f);
        keel::f32 padTop = 0.0f;
        if (opts.padding.has_value()) {
            padTop = opts.padding->top;
        } else if (hasCustomBgOrBorder) {
            padTop = (theme.SizeFactor(size) < 0.90f) ? 2.0f : 3.0f;
        }
        if (opts.border.radius.has_value()) {
            tabRadius = (*opts.border.radius == 0.0f) ? 0.0f : keel::max(0.0f, *opts.border.radius - padTop);
        }
    }
    const Color defaultPillCol = (item.variant != UiVariant::Default)
        ? theme.GetVariant(item.variant).bgActive
        : pal.surfaceActive;
    const Color pillActiveCol = opts.color.HasAny()
        ? theme.ResolveColor(opts.color, hovered, active, disabled, defaultPillCol)
        : defaultPillCol;

    const keel::f32 chromeYShift = theme.Painter().DrawTabChrome(
        canvas,
        theme,
        bounds,
        chromeStyle,
        active,
        hovered,
        focused,
        disabled,
        opts.showSeparator,
        accentCol,
        pillActiveCol,
        tabRadius
    );

    // Determine foreground colors for text and icon
    Color fgCol;
    if (item.label.color.HasAny()) {
        fgCol = theme.ResolveTextStateColor(item.label.color).Resolve(hovered, active, disabled, pal.textSecondary, pal.textPrimary, pal.textPrimary, pal.textDisabled);
    } else if (disabled) {
        fgCol = pal.textDisabled;
    } else if (active || hovered) {
        fgCol = pal.textPrimary;
    } else {
        fgCol = pal.textSecondary;
    }

    const Rect closeRect = item.closable ? ComputeTabCloseButtonRect(bounds, m) : Rect{};
    const keel::f32 closeReserve = item.closable ? (closeRect.w + 6.0f) : 0.0f;

    // Measure components to center content within tab bounds (gracefully adapting at narrow tab widths)
    const keel::f32 padX = keel::min(m.paddingX, bounds.w * 0.12f);
    const keel::f32 innerW = keel::max(0.0f, bounds.w - padX * 2.0f - closeReserve);
    const bool hasIcon = item.icon.IsValid();
    const bool hasLabel = item.label.HasContentOrReserve();
    keel::f32 badgeW = 0.0f;
    if (item.badge.HasContentOrReserve()) {
        badgeW = TextUtil::MeasureWidth(item.badge, badgeFontSize, &canvas) + 12.0f;
    }
    const bool showBadge = !item.badge.empty() &&
        (innerW >= (hasIcon ? m.iconSize + 6.0f : 0.0f) + badgeW + (hasLabel ? labelFont * 2.2f + 6.0f : 0.0f));

    const keel::f32 iconAndBadgeW = (hasIcon ? m.iconSize : 0.0f) + (showBadge ? badgeW + 6.0f : 0.0f);
    const keel::f32 availLabelW = keel::max(0.0f, innerW - iconAndBadgeW - ((hasIcon && hasLabel) ? 6.0f : 0.0f));
    const bool showLabel = !item.label.empty() && (!hasIcon || availLabelW >= labelFont * 1.35f);

    keel::FixedString<256> truncatedLabel;
    keel::StringRef displayLabel = item.label.text;
    keel::f32 labelW = 0.0f;
    if (showLabel) {
        labelW = canvas.MeasureText(displayLabel, labelFont).x;
        if (labelW > availLabelW + kTextSubpixelTolerance && availLabelW > 0.0f) {
            truncatedLabel = TextUtil::TruncateWithEllipsis(item.label.text, availLabelW, labelFont, &canvas);
            displayLabel = truncatedLabel;
            labelW = keel::min(availLabelW, canvas.MeasureText(displayLabel, labelFont).x);
        }
    }

    keel::f32 contentW = 0.0f;
    if (hasIcon) contentW += m.iconSize;
    if (showLabel) {
        if (contentW > 0.0f) contentW += 6.0f;
        contentW += labelW;
    }
    if (showBadge) {
        if (contentW > 0.0f) contentW += 6.0f;
        contentW += badgeW;
    }

    const keel::f32 contentAreaW = keel::max(0.0f, bounds.w - closeReserve);
    keel::f32 curX = bounds.x + keel::max(padX, (contentAreaW - contentW) * 0.5f);
    const keel::f32 centerY = bounds.y + bounds.h * 0.5f + chromeYShift;

    // Draw Icon
    if (hasIcon) {
        const Rect iconR(curX, centerY - m.iconSize * 0.5f, m.iconSize, m.iconSize);
        item.icon.Draw(canvas, iconR, fgCol);
        curX += m.iconSize + (showLabel ? 6.0f : 0.0f);
    }

    // Draw Label
    if (showLabel) {
        const Rect textR(curX, bounds.y + chromeYShift, availLabelW, bounds.h);
        theme.Painter().DrawControlText(canvas, theme, displayLabel, textR, labelFont, fgCol, disabled, 0.0f, 0.5f);
        curX += labelW;
    }

    // Draw Badge
    if (showBadge) {
        if (hasIcon || showLabel) curX += 6.0f;
        const keel::f32 badgeH = badgeFontSize + 6.0f;
        const Rect badgeR(curX, centerY - badgeH * 0.5f, badgeW, badgeH);

        Color badgeBg = active ? pal.accent.WithAlphaF(0.25f) : pal.surfaceHover;
        Color badgeText = active ? pal.accent : pal.textSecondary;

        if (item.badge.color.HasAny()) {
            const Color resolved = theme.ResolveColor(item.badge.color, hovered, active, disabled, badgeText);
            badgeBg = resolved.WithAlphaF(0.25f);
            badgeText = resolved;
        } else if (item.badge.text == "Live") {
            const VariantColors success = theme.GetVariant(UiVariant::Success);
            badgeBg = success.bg.WithAlphaF(0.5f);
            badgeText = success.text;
        }

        theme.Painter().DrawBadge(canvas, theme, badgeR, badgeBg, Color::Transparent(), 0.0f, 4.0f);
        canvas.DrawTextBox(item.badge.text, badgeR, badgeFontSize, badgeText, 0.5f, 0.5f);
    }

    // Draw trailing Close ('X') button if closable
    if (item.closable && closeRect.w > 4.0f && bounds.w > closeRect.w + 8.0f) {
        if (closeHovered && !disabled) {
            canvas.DrawRoundRect(closeRect, keel::min(3.5f, m.cornerRadius), pal.surfaceHover);
        }
        const keel::f32 icSz = keel::max(8.0f, closeRect.w * 0.68f);
        const Rect icRect(
            closeRect.x + (closeRect.w - icSz) * 0.5f,
            closeRect.y + (closeRect.h - icSz) * 0.5f,
            icSz,
            icSz
        );
        const Color closeCol = disabled ? pal.textDisabled : (closeHovered ? pal.textPrimary : pal.textSecondary);
        Icon::Close().Draw(canvas, icRect, closeCol);
    }

    canvas.PopClip();
}

} // namespace guikit
