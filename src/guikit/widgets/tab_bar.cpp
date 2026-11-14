#include "guikit/widgets/tab_bar.hpp"
#include <algorithm>
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "guikit/panels/carousel.hpp"

namespace guikit {

namespace {

Rect ComputeTabCloseButtonRect(Rect bounds, const ComponentMetrics& m) noexcept {
    const float btnSz = std::max(12.0f, m.iconSize);
    const float padX = std::min(m.paddingX, bounds.w * 0.12f);
    const float bx = bounds.Right() - std::max(4.0f, padX * 0.65f) - btnSz;
    const float by = bounds.y + (bounds.h - btnSz) * 0.5f;
    return Rect(bx, by, btnSz, btnSz);
}

} // namespace

TabBarResponse TabBar::Run(
    Context& ctx,
    int& activeIndex,
    std::span<const TabItem> tabs,
    const TabBarOptions& opts,
    std::source_location loc
) {
    TabBarResponse res;
    if (tabs.empty()) return res;

    const UiSize activeSize = ctx.GetCurrentSize();
    const bool activeDisabled = opts.disabled || ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());

    std::optional<ContainerScope> carouselScope;
    Node* barNode = nullptr;

    if (opts.scrollable && !opts.fullWidth) {
        CarouselOptions cOpts;
        cOpts.scrollWheel = true;
        cOpts.disabled = activeDisabled;
        cOpts.theme = opts.theme;
        cOpts.scale = opts.scale;

        carouselScope.emplace(ctx.Carousel(cOpts, loc));
        barNode = carouselScope->GetNode();
    } else {
        UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
        const Id barId = ctx.MakeChildId(loc);
        barNode = ctx.CreateNode(NodeType::Container, barId);
        barNode->theme = pushedTheme;
        barNode->direction = FlexDirection::Row;
        barNode->align = Alignment::Center;
        barNode->uiSize = activeSize;
        barNode->disabled = activeDisabled;
        barNode->widthMode = opts.fullWidth ? SizeMode::Stretch : SizeMode::Fit;

        ctx.PushScale(activeScale);
        ctx.PushSize(activeSize);
        ctx.PushDisabled(activeDisabled);
        ctx.PushContainer(barNode);
    }

    const UiTheme& theme = ctx.GetTheme();
    const ComponentMetrics& m = theme.GetMetrics(activeSize);
    const float defaultTabGap = (opts.style == TabBarStyle::Card || opts.style == TabBarStyle::Folder)
        ? theme.Scale(2.0f)
        : ((activeSize <= UiSize::Sm) ? theme.GetGap(UiSize::Xs) : theme.GetGap(UiSize::Sm));
    const float activeTabGap = opts.gap.has_value() ? theme.Scale(*opts.gap) : defaultTabGap;

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
        resolvedPadding = theme.ResolvePadding(opts.padding);
    } else if (opts.style == TabBarStyle::Pill && hasCustomBgOrBorder) {
        const float defaultPad = ((activeSize <= UiSize::Sm) ? 2.0f : 3.0f) * activeScale;
        resolvedPadding = Padding{defaultPad, defaultPad, defaultPad, defaultPad};
    }

    float resolvedRadius = 0.0f;
    if (opts.border.radius.has_value()) {
        resolvedRadius = theme.Scale(*opts.border.radius);
    } else if (opts.style == TabBarStyle::Pill && hasCustomBgOrBorder) {
        resolvedRadius = m.cornerRadius + 2.0f * activeScale;
    }

    const float effectiveBorderWidth = (opts.border.width > 0.0f)
        ? theme.Scale(opts.border.width)
        : (resolvedBorderCol.a > 0 ? theme.Scale(1.0f) : 0.0f);

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
            const float sepY = n->computedRect.Bottom() - 0.5f;
            canvas.DrawLine(
                Vec2(n->computedRect.x, sepY),
                Vec2(n->computedRect.Right(), sepY),
                th.palette.borderSubtle,
                1.0f
            );
        };
    }

    // Auto-scroll to active tab when activeIndex changes or on initial display
    int& lastActive = ctx.GetOrCreatePersistentState<int>(barNode->id, -1);
    if (lastActive != activeIndex) {
        lastActive = activeIndex;
        if (barNode->type == NodeType::Carousel) {
            if (auto* cData = barNode->As<CarouselData>()) {
                cData->scrollToActiveTab = true;
            }
        }
    }

    for (size_t i = 0; i < tabs.size(); ++i) {
        const TabItem& item = tabs[i];
        const Id tabId = MakeId(static_cast<uint32_t>(i), loc, barNode->id);
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

        if (tabRes.clicked && !effectiveDisabled && activeIndex != static_cast<int>(i)) {
            activeIndex = static_cast<int>(i);
            res.changed = true;
            res.clicked = true;
            ctx.GetOrCreatePersistentState<int>(barNode->id) = activeIndex;
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
            opts.activeColor.ToUint32(),
            opts.border.radius.value_or(-1.0f),
            item.variant,
            item.closable,
            closeHovered,
            tData->item.badge,
            tData->item.label,
            static_cast<uint8_t>(tData->item.icon.GetType()),
            static_cast<uint8_t>(tData->item.icon.GetId()),
            tData->item.icon.GetPath()
        );
        tabNode->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
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
        tabNode->active = (activeIndex == static_cast<int>(i));
        tabNode->uiSize = activeSize;
        tabNode->disabled = effectiveDisabled;
        tabNode->hovered = tabRes.hovered;
        tabNode->pressed = tabRes.pressed;
        tabNode->focused = ctx.IsFocused(tabId);
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
    const float labelFont = theme.ResolveFontSize(item.label, TextRole::LabelLarge, size);
    const float badgeFontSize = theme.ResolveFontSize(item.badge, m.fontSize * 0.78f);
    float contentW = 0.0f;

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
        const float badgeTextW = TextUtil::MeasureWidth(item.badge, badgeFontSize, canvas);
        contentW += badgeTextW + 12.0f; // Badge padding
    }

    if (item.closable) {
        const float closeBtnSz = std::max(12.0f, m.iconSize);
        contentW += (contentW > 0.0f ? 6.0f : 0.0f) + closeBtnSz;
    }

    const float padX = m.paddingX + (opts.style == TabBarStyle::Folder ? theme.Scale(4.0f) : 0.0f);
    const float totalW = contentW + padX * 2.0f;

    return {std::max(m.defaultHeight, totalW), m.defaultHeight};
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
    const float labelFont = theme.ResolveFontSize(item.label, TextRole::LabelLarge, size);
    const float badgeFontSize = theme.ResolveFontSize(item.badge, m.fontSize * 0.78f);

    canvas.PushClip(bounds);

    const Color accentCol = (opts.activeColor.a > 0)
        ? opts.activeColor
        : (item.variant != UiVariant::Default ? theme.GetVariant(item.variant).bg : pal.accent);

    if (opts.style == TabBarStyle::Underline) {
        if (opts.showSeparator) {
            canvas.DrawLine(
                Vec2(bounds.x, bounds.Bottom() - 0.5f),
                Vec2(bounds.Right(), bounds.Bottom() - 0.5f),
                pal.borderSubtle,
                1.0f
            );
        }

        if (hovered && !active && !disabled) {
            const Rect hoverR(bounds.x, bounds.y + 4.0f, bounds.w, bounds.h - 8.0f);
            canvas.DrawRoundRect(hoverR, m.cornerRadius, pal.surfaceHover);
        }

        if (active) {
            const float lineH = 3.0f;
            const float linePad = 2.0f;
            const Rect lineR(bounds.x + linePad, bounds.Bottom() - lineH, std::max(0.0f, bounds.w - linePad * 2.0f), lineH);
            canvas.DrawRoundRect(lineR, 1.5f, accentCol);
        }
        if (focused && !active && !disabled) {
            canvas.DrawRoundRect(Rect(bounds.x + 1.0f, bounds.y + 2.0f, std::max(0.0f, bounds.w - 2.0f), std::max(0.0f, bounds.h - 4.0f)), m.cornerRadius, Color::Transparent(), pal.focusRing, 1.5f);
        }
    } else if (opts.style == TabBarStyle::Card) {
        const float r = opts.border.radius.has_value() ? theme.Scale(*opts.border.radius) : m.cornerRadius;
        const Rect cardR(bounds.x, bounds.y + 2.0f, bounds.w, bounds.h + r);
        if (active) {
            canvas.DrawRoundRect(cardR, r, pal.surface, pal.border, 1.0f);
            // Mask baseline seam under active card tab
            canvas.DrawRect(Rect(bounds.x + 1.0f, bounds.Bottom() - 1.5f, std::max(0.0f, bounds.w - 2.0f), 2.0f), pal.surface);
            canvas.DrawRoundRect(Rect(bounds.x + 1.0f, bounds.y + 2.0f, std::max(0.0f, bounds.w - 2.0f), 2.5f), 1.0f, accentCol);
        } else {
            if (opts.showSeparator) {
                canvas.DrawLine(Vec2(bounds.x, bounds.Bottom() - 0.5f), Vec2(bounds.Right(), bounds.Bottom() - 0.5f), pal.borderSubtle, 1.0f);
            }
            if (hovered && !disabled) {
                canvas.DrawRoundRect(cardR, r, pal.surfaceHover, pal.borderSubtle, 1.0f);
            }
        }
        if (focused && !disabled) {
            canvas.DrawRoundRect(Rect(bounds.x + 1.0f, bounds.y + 3.0f, std::max(0.0f, bounds.w - 2.0f), std::max(0.0f, bounds.h - 4.0f)), std::max(0.0f, r - 1.0f), Color::Transparent(), pal.focusRing, 1.5f);
        }
    } else if (opts.style == TabBarStyle::Folder) {
        const float slope = std::min(theme.Scale(6.0f), bounds.w * 0.14f);
        const float topY = bounds.y + (active ? 1.5f : 4.0f);
        const float botY = bounds.Bottom();
        VectorPath folderPath;
        folderPath.MoveTo({bounds.x, botY});
        folderPath.LineTo({bounds.x + slope, topY});
        folderPath.LineTo({bounds.Right() - slope, topY});
        folderPath.LineTo({bounds.Right(), botY});
        folderPath.Close();

        if (active || (hovered && !disabled)) {
            PathPaint p{};
            p.fillColor = active ? pal.surfaceElevated : pal.surfaceHover;
            p.renderFill = true;
            p.strokeColor = active ? pal.border : pal.borderSubtle;
            p.strokeWidthPx = 1.0f;
            p.renderStroke = true;
            canvas.DrawPath(folderPath, p);
        }
        if (active) {
            canvas.DrawRect(Rect(bounds.x + 1.0f, botY - 1.5f, std::max(0.0f, bounds.w - 2.0f), 2.0f), pal.surfaceElevated);
            canvas.DrawLine(Vec2(bounds.x + slope, topY + 0.5f), Vec2(bounds.Right() - slope, topY + 0.5f), accentCol, 2.0f);
        } else if (opts.showSeparator) {
            canvas.DrawLine(Vec2(bounds.x, botY - 0.5f), Vec2(bounds.Right(), botY - 0.5f), pal.borderSubtle, 1.0f);
        }
        if (focused && !disabled) {
            canvas.DrawRoundRect(Rect(bounds.x + slope, topY + 1.0f, std::max(0.0f, bounds.w - slope * 2.0f), std::max(0.0f, botY - topY - 2.0f)), 2.0f, Color::Transparent(), pal.focusRing, 1.5f);
        }
    } else {
        // Pill / Segmented style
        const bool hasCustomBgOrBorder = (opts.background.IsBackgroundEnabled(false) || opts.border.color.HasAny() || opts.border.width > 0.0f);
        float padTop = 0.0f;
        if (opts.padding.has_value()) {
            padTop = opts.padding->top;
        } else if (opts.style == TabBarStyle::Pill && hasCustomBgOrBorder) {
            padTop = (size <= UiSize::Sm) ? 2.0f : 3.0f;
        }

        float pillRadius = m.cornerRadius;
        if (opts.border.radius.has_value()) {
            if (*opts.border.radius == 0.0f) {
                pillRadius = 0.0f;
            } else {
                pillRadius = std::max(0.0f, *opts.border.radius - padTop);
            }
        }

        const Color focusBorder = (focused && !disabled) ? pal.focusRing : Color::Transparent();
        const float focusBorderW = (focused && !disabled) ? 1.5f : 0.0f;

        if (active) {
            const Color pillCol = (opts.activeColor.a > 0)
                ? opts.activeColor
                : (item.variant != UiVariant::Default ? theme.GetVariant(item.variant).bgActive : pal.surfaceActive);
            canvas.DrawRoundRect(bounds, pillRadius, pillCol, focusBorder, focusBorderW);
        } else if (hovered && !disabled) {
            canvas.DrawRoundRect(bounds, pillRadius, pal.surfaceHover, focusBorder, focusBorderW);
        } else if (focused && !disabled) {
            canvas.DrawRoundRect(bounds, pillRadius, Color::Transparent(), focusBorder, focusBorderW);
        }
    }

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
    const float closeReserve = item.closable ? (closeRect.w + 6.0f) : 0.0f;

    // Measure components to center content within tab bounds (gracefully adapting at narrow tab widths)
    const float padX = std::min(m.paddingX, bounds.w * 0.12f);
    const float innerW = std::max(0.0f, bounds.w - padX * 2.0f - closeReserve);
    const bool hasIcon = item.icon.IsValid();
    const bool hasLabel = item.label.HasContentOrReserve();
    float badgeW = 0.0f;
    if (item.badge.HasContentOrReserve()) {
        badgeW = TextUtil::MeasureWidth(item.badge, badgeFontSize, &canvas) + 12.0f;
    }
    const bool showBadge = !item.badge.empty() &&
        (innerW >= (hasIcon ? m.iconSize + 6.0f : 0.0f) + badgeW + (hasLabel ? labelFont * 2.2f + 6.0f : 0.0f));

    const float iconAndBadgeW = (hasIcon ? m.iconSize : 0.0f) + (showBadge ? badgeW + 6.0f : 0.0f);
    const float availLabelW = std::max(0.0f, innerW - iconAndBadgeW - ((hasIcon && hasLabel) ? 6.0f : 0.0f));
    const bool showLabel = !item.label.empty() && (!hasIcon || availLabelW >= labelFont * 1.35f);

    keel::FixedString<256> truncatedLabel;
    std::string_view displayLabel = item.label.text;
    float labelW = 0.0f;
    if (showLabel) {
        labelW = canvas.MeasureText(displayLabel, labelFont).x;
        if (labelW > availLabelW + kTextSubpixelTolerance && availLabelW > 0.0f) {
            truncatedLabel = TextUtil::TruncateWithEllipsis(item.label.text, availLabelW, labelFont, &canvas);
            displayLabel = truncatedLabel;
            labelW = std::min(availLabelW, canvas.MeasureText(displayLabel, labelFont).x);
        }
    }

    float contentW = 0.0f;
    if (hasIcon) contentW += m.iconSize;
    if (showLabel) {
        if (contentW > 0.0f) contentW += 6.0f;
        contentW += labelW;
    }
    if (showBadge) {
        if (contentW > 0.0f) contentW += 6.0f;
        contentW += badgeW;
    }

    const float contentAreaW = std::max(0.0f, bounds.w - closeReserve);
    float curX = bounds.x + std::max(padX, (contentAreaW - contentW) * 0.5f);
    const float centerY = bounds.y + bounds.h * 0.5f;

    // Draw Icon
    if (hasIcon) {
        const Rect iconR(curX, centerY - m.iconSize * 0.5f, m.iconSize, m.iconSize);
        item.icon.Draw(canvas, iconR, fgCol);
        curX += m.iconSize + (showLabel ? 6.0f : 0.0f);
    }

    // Draw Label
    if (showLabel) {
        const Rect textR(curX, bounds.y, availLabelW, bounds.h);
        canvas.DrawTextBox(displayLabel, textR, labelFont, fgCol, 0.0f, 0.5f);
        curX += labelW;
    }

    // Draw Badge
    if (showBadge) {
        if (hasIcon || showLabel) curX += 6.0f;
        const float badgeH = badgeFontSize + 6.0f;
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

        canvas.DrawRoundRect(badgeR, 4.0f, badgeBg);
        canvas.DrawTextBox(item.badge.text, badgeR, badgeFontSize, badgeText, 0.5f, 0.5f);
    }

    // Draw trailing Close ('X') button if closable
    if (item.closable && closeRect.w > 4.0f && bounds.w > closeRect.w + 8.0f) {
        if (closeHovered && !disabled) {
            canvas.DrawRoundRect(closeRect, 3.5f, pal.surfaceHover);
        }
        const float icSz = std::max(8.0f, closeRect.w * 0.68f);
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
