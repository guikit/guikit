#include "guikit/widgets/TabBar.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"
#include "guikit/panels/Carousel.hpp"

namespace guikit {

Response TabBar::Run(
    Context& ctx,
    int& activeIndex,
    std::span<const TabItem> tabs,
    const TabBarOptions& opts,
    std::source_location loc
) {
    Response res;
    if (tabs.empty()) return res;

    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool activeDisabled = ctx.IsDisabled();
    const float activeScale = opts.scale.value_or(ctx.GetCurrentScale());

    std::optional<ContainerScope> carouselScope;
    Node* barNode = nullptr;

    if (opts.scrollable && !opts.fullWidth) {
        CarouselOptions cOpts;
        cOpts.size = activeSize;
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
    const float defaultTabGap = (activeSize <= UiSize::Sm) ? theme.GetGap(UiSize::Xs) : theme.GetGap(UiSize::Sm);
    const float activeTabGap = opts.gap.has_value() ? theme.Scale(*opts.gap) : defaultTabGap;

    const bool hasCustomBgOrBorder = (opts.backgroundColor.a > 0 || opts.borderColor.a > 0);

    Padding resolvedPadding{};
    if (opts.padding.has_value()) {
        resolvedPadding = theme.ResolvePadding(opts.padding);
    } else if (opts.style == TabBarStyle::Pill && hasCustomBgOrBorder) {
        const float defaultPad = ((activeSize <= UiSize::Sm) ? 2.0f : 3.0f) * activeScale;
        resolvedPadding = Padding{defaultPad, defaultPad, defaultPad, defaultPad};
    }

    float resolvedRadius = 0.0f;
    if (opts.borderRadius.has_value()) {
        resolvedRadius = theme.Scale(*opts.borderRadius);
    } else if (opts.style == TabBarStyle::Pill && hasCustomBgOrBorder) {
        resolvedRadius = m.cornerRadius + 2.0f * activeScale;
    }

    const float effectiveBorderWidth = (opts.borderWidth > 0.0f) ? theme.Scale(opts.borderWidth)
        : (opts.borderColor.a > 0 ? theme.Scale(1.0f) : 0.0f);

    barNode->gap = activeTabGap;
    barNode->padding = resolvedPadding;
    barNode->heightMode = SizeMode::Fixed;
    barNode->fixedHeight = m.defaultHeight + resolvedPadding.Vertical();
    barNode->backgroundColor = opts.backgroundColor;
    barNode->borderColor = opts.borderColor;
    barNode->borderWidth = effectiveBorderWidth;
    barNode->cornerRadius = resolvedRadius;
    if (resolvedRadius > 0.0f) {
        barNode->clipChildren = true;
    }
    barNode->visualHash = detail::HashArgs(
        opts.backgroundColor.ToUint32(),
        opts.borderColor.ToUint32(),
        effectiveBorderWidth,
        resolvedRadius,
        resolvedPadding.top,
        resolvedPadding.left
    );
    if (barNode->type == NodeType::Carousel) {
        if (auto* cData = barNode->As<CarouselData>()) {
            cData->showBaselineSeparator = (opts.showSeparator && opts.style == TabBarStyle::Underline);
        }
    } else if (opts.showSeparator && opts.style == TabBarStyle::Underline) {
        barNode->postRenderFn = [](const Node* n, ICanvas& canvas, const UiTheme& th) noexcept {
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
    int& lastActive = ctx.GetOrCreateState<int>(barNode->id, -1);
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
        const bool effectiveDisabled = item.disabled || ctx.IsDisabled();

        ctx.RegisterFocusable(tabId, effectiveDisabled);

        Response tabRes = ctx.Interact(tabId);
        if (effectiveDisabled) {
            tabRes.clicked = false;
            tabRes.hovered = false;
            tabRes.pressed = false;
        } else {
            if (tabRes.clicked) {
                ctx.RequestFocus(tabId);
            }
            if (ctx.IsFocused(tabId) && (ctx.ConsumeKey(Key::Space) || ctx.ConsumeKey(Key::Enter))) {
                tabRes.clicked = true;
            }
        }

        if (tabRes.hovered && !item.tooltip.empty()) {
            ctx.SetTooltip(item.tooltip, tabRes.rect);
        }

        if (tabRes.clicked && !effectiveDisabled && activeIndex != static_cast<int>(i)) {
            activeIndex = static_cast<int>(i);
            res.changed = true;
            res.clicked = true;
            ctx.GetOrCreateState<int>(barNode->id) = activeIndex;
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
        tData->barOpts = opts;
        tabNode->text = tData->item.label.text;
        tabNode->SetPayload(tData);
        tabNode->visualHash = detail::HashArgs(
            opts.style,
            opts.showSeparator,
            opts.activeColor.ToUint32(),
            opts.borderRadius.value_or(-1.0f),
            item.variant,
            tData->item.badge,
            tData->item.label,
            static_cast<uint8_t>(item.icon.GetType()),
            static_cast<uint8_t>(item.icon.GetId()),
            item.icon.GetPath()
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
                n->focused
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

    return res;
}

Vec2 TabBar::MeasureTab(
    const TabItem& item,
    const TabBarOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    (void)opts;
    const ComponentMetrics& m = theme.GetMetrics(size);
    const float labelFont = theme.ResolveFontSize(item.label, TextRole::LabelMedium, size);
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

    const float padX = m.paddingX;
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
    bool focused
) {
    (void)pressed;
    const ComponentMetrics& m = theme.GetMetrics(size);
    const auto& pal = theme.palette;
    const float labelFont = theme.ResolveFontSize(item.label, TextRole::LabelMedium, size);
    const float badgeFontSize = theme.ResolveFontSize(item.badge, m.fontSize * 0.78f);

    canvas.PushClip(bounds);

    if (opts.style == TabBarStyle::Underline) {
        // Horizontal baseline separator spanning across tabs
        if (opts.showSeparator) {
            canvas.DrawLine(
                Vec2(bounds.x, bounds.Bottom() - 0.5f),
                Vec2(bounds.Right(), bounds.Bottom() - 0.5f),
                pal.borderSubtle,
                1.0f
            );
        }

        // Hover backdrop highlight
        if (hovered && !active && !disabled) {
            const Rect hoverR(bounds.x, bounds.y + 4.0f, bounds.w, bounds.h - 8.0f);
            canvas.DrawRoundRect(hoverR, m.cornerRadius, pal.surfaceHover);
        }

        // Bottom active underline accent
        if (active) {
            const float lineH = 3.0f;
            const float linePad = 2.0f;
            const Color lineCol = (opts.activeColor.a > 0)
                ? opts.activeColor
                : (item.variant != UiVariant::Default ? theme.GetVariant(item.variant).bg : pal.accent);
            const Rect lineR(bounds.x + linePad, bounds.Bottom() - lineH, std::max(0.0f, bounds.w - linePad * 2.0f), lineH);
            canvas.DrawRoundRect(lineR, 1.5f, lineCol);
        }
        if (focused && !disabled) {
            canvas.DrawRoundRect(Rect(bounds.x + 1.0f, bounds.y + 2.0f, std::max(0.0f, bounds.w - 2.0f), std::max(0.0f, bounds.h - 4.0f)), m.cornerRadius, Color::Transparent(), pal.focusRing, 1.5f);
        }
    } else {
        // Pill / Segmented style
        const bool hasCustomBgOrBorder = (opts.backgroundColor.a > 0 || opts.borderColor.a > 0);
        float padTop = 0.0f;
        if (opts.padding.has_value()) {
            padTop = opts.padding->top;
        } else if (opts.style == TabBarStyle::Pill && hasCustomBgOrBorder) {
            padTop = (size <= UiSize::Sm) ? 2.0f : 3.0f;
        }

        float pillRadius = m.cornerRadius;
        if (opts.borderRadius.has_value()) {
            if (*opts.borderRadius == 0.0f) {
                pillRadius = 0.0f;
            } else {
                pillRadius = std::max(0.0f, *opts.borderRadius - padTop);
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
        fgCol = item.label.color.Resolve(hovered, active, disabled, pal.textSecondary, pal.textPrimary, pal.textPrimary, pal.textDisabled);
    } else if (disabled) {
        fgCol = pal.textDisabled;
    } else if (active || hovered) {
        fgCol = pal.textPrimary;
    } else {
        fgCol = pal.textSecondary;
    }

    // Measure components to center content within tab bounds
    float contentW = 0.0f;
    if (item.icon.IsValid()) {
        contentW += m.iconSize;
    }
    float textW = 0.0f;
    if (item.label.HasContentOrReserve()) {
        if (contentW > 0.0f) contentW += 6.0f;
        textW = TextUtil::MeasureWidth(item.label, labelFont, &canvas);
        contentW += textW;
    }
    float badgeW = 0.0f;
    if (item.badge.HasContentOrReserve()) {
        contentW += 6.0f;
        badgeW = TextUtil::MeasureWidth(item.badge, badgeFontSize, &canvas) + 12.0f;
        contentW += badgeW;
    }

    float curX = bounds.x + std::max(0.0f, (bounds.w - contentW) * 0.5f);
    const float centerY = bounds.y + bounds.h * 0.5f;

    // Draw Icon
    if (item.icon.IsValid()) {
        const Rect iconR(curX, centerY - m.iconSize * 0.5f, m.iconSize, m.iconSize);
        item.icon.Draw(canvas, iconR, fgCol);
        curX += m.iconSize + 6.0f;
    }

    // Draw Label
    if (!item.label.empty()) {
        const float maxLabelW = std::max(0.0f, bounds.Right() - m.paddingX - curX - (item.badge.empty() ? 0.0f : (badgeW + 6.0f)));
        float labelW = canvas.MeasureText(item.label.text, labelFont).x;
        std::string displayLabel(item.label.text);
        if (labelW > maxLabelW + kTextSubpixelTolerance && maxLabelW > 0.0f) {
            displayLabel = TextUtil::TruncateWithEllipsis(item.label.text, maxLabelW, labelFont, &canvas);
            labelW = canvas.MeasureText(displayLabel, labelFont).x;
        }
        const Rect textR(curX, bounds.y, maxLabelW, bounds.h);
        canvas.DrawTextBox(displayLabel, textR, labelFont, fgCol, 0.0f, 0.5f);
        curX += labelW;
    }

    // Draw Badge
    if (!item.badge.empty()) {
        curX += 6.0f;
        const float badgeH = badgeFontSize + 6.0f;
        const Rect badgeR(curX, centerY - badgeH * 0.5f, badgeW, badgeH);

        Color badgeBg = active ? pal.accent.WithAlphaF(0.25f) : pal.surfaceHover;
        Color badgeText = active ? pal.accent : pal.textSecondary;

        if (item.badge.color.HasAny()) {
            const Color resolved = item.badge.color.Resolve(hovered, active, disabled, badgeText);
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

    canvas.PopClip();
}

} // namespace guikit
