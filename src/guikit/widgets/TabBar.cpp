#include "guikit/widgets/TabBar.hpp"
#include <algorithm>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
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
    const ComponentMetrics& m = ctx.GetTheme().GetMetrics(activeSize);

    std::optional<ContainerScope> carouselScope;
    Node* barNode = nullptr;

    if (opts.scrollable && !opts.fullWidth) {
        CarouselOptions cOpts;
        cOpts.size = activeSize;
        cOpts.gap = (activeSize <= UiSize::Sm) ? UiSize::Xs : UiSize::Sm;
        cOpts.scrollWheel = true;
        cOpts.disabled = ctx.IsDisabled();

        carouselScope.emplace(ctx.Carousel(cOpts, loc));
        barNode = carouselScope->GetNode();
        barNode->heightMode = SizeMode::Fixed;
        barNode->fixedHeight = m.defaultHeight;
        if (auto* cData = barNode->As<CarouselData>()) {
            cData->showBaselineSeparator = (opts.showSeparator && opts.style == TabBarStyle::Underline);
        }
    } else {
        const Id barId = ctx.MakeChildId(loc);
        barNode = ctx.CreateNode(NodeType::Container, barId);
        barNode->direction = FlexDirection::Row;
        barNode->align = Alignment::Center;
        barNode->gap = (activeSize <= UiSize::Sm) ? ctx.GetTheme().GetGap(UiSize::Xs) : ctx.GetTheme().GetGap(UiSize::Sm);
        barNode->uiSize = activeSize;
        barNode->widthMode = opts.fullWidth ? SizeMode::Stretch : SizeMode::Fit;
        barNode->heightMode = SizeMode::Fixed;
        barNode->fixedHeight = m.defaultHeight;
        barNode->payload = ctx.AllocPayload<TabBarOptions>(opts);
        ctx.PushContainer(barNode);
    }

    // Auto-scroll to active tab when activeIndex changes or on initial display
    int& lastActive = ctx.GetOrCreateState<int>(barNode->id, -1);
    if (lastActive != activeIndex) {
        lastActive = activeIndex;
        if (auto* cData = barNode->As<CarouselData>()) {
            cData->scrollToActiveTab = true;
        }
    }

    for (size_t i = 0; i < tabs.size(); ++i) {
        const TabItem& item = tabs[i];
        const Id tabId = MakeId(static_cast<uint32_t>(i), loc, barNode->id);
        const bool effectiveDisabled = item.disabled || ctx.IsDisabled();

        Response tabRes = ctx.Interact(tabId);
        if (effectiveDisabled) {
            tabRes.clicked = false;
            tabRes.hovered = false;
            tabRes.pressed = false;
        }

        if (tabRes.clicked && !effectiveDisabled && activeIndex != static_cast<int>(i)) {
            activeIndex = static_cast<int>(i);
            res.changed = true;
            res.clicked = true;
            ctx.GetOrCreateState<int>(barNode->id) = activeIndex;
            if (auto* cData = barNode->As<CarouselData>()) {
                cData->scrollToActiveTab = true;
            }
            ctx.RequestRepaint(3);
        }

        Node* tabNode = ctx.CreateNode(NodeType::TabItem, tabId);
        tabNode->text = ctx.InternString(item.label);
        auto* tData = ctx.AllocPayload<TabItemData>();
        tData->item = item;
        tData->item.label = tabNode->text;
        tData->barOpts = opts;
        tabNode->payload = tData;
        tabNode->measureFn = [](const Node* n, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
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
                theme
            );
        };
        tabNode->active = (activeIndex == static_cast<int>(i));
        tabNode->uiSize = activeSize;
        tabNode->disabled = effectiveDisabled;
        tabNode->hovered = tabRes.hovered;
        tabNode->pressed = tabRes.pressed;
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
    const ComponentMetrics& m = theme.GetMetrics(size);
    float contentW = 0.0f;

    if (item.icon.IsValid()) {
        contentW += m.iconSize;
    }

    if (!item.label.empty()) {
        if (contentW > 0.0f) {
            contentW += 6.0f; // Gap between icon and text
        }
        float textW = 0.0f;
        if (canvas) {
            textW = canvas->MeasureText(item.label, m.fontSize).x;
        } else {
            textW = static_cast<float>(item.label.length()) * (m.fontSize * kDefaultCharAdvanceRatio);
        }
        contentW += textW;
    }

    if (!item.badge.empty()) {
        contentW += 6.0f; // Gap between text and badge
        float badgeTextW = 0.0f;
        const float badgeFontSize = m.fontSize * 0.78f;
        if (canvas) {
            badgeTextW = canvas->MeasureText(item.badge, badgeFontSize).x;
        } else {
            badgeTextW = static_cast<float>(item.badge.length()) * (badgeFontSize * 0.55f);
        }
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
    const UiTheme& theme
) {
    const ComponentMetrics& m = theme.GetMetrics(size);
    const auto& pal = theme.palette;

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
    } else {
        // Pill / Segmented style
        if (active) {
            const Color pillCol = (opts.activeColor.a > 0)
                ? opts.activeColor
                : (item.variant != UiVariant::Default ? theme.GetVariant(item.variant).bgActive : pal.surfaceActive);
            canvas.DrawRoundRect(bounds, m.cornerRadius, pillCol);
        } else if (hovered && !disabled) {
            canvas.DrawRoundRect(bounds, m.cornerRadius, pal.surfaceHover);
        }
    }

    // Determine foreground colors for text and icon
    Color fgCol;
    if (disabled) {
        fgCol = pal.textDisabled;
    } else if (active) {
        fgCol = pal.textPrimary;
    } else if (hovered) {
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
    if (!item.label.empty()) {
        if (contentW > 0.0f) contentW += 6.0f;
        textW = canvas.MeasureText(item.label, m.fontSize).x;
        contentW += textW;
    }
    float badgeW = 0.0f;
    const float badgeFontSize = m.fontSize * 0.78f;
    if (!item.badge.empty()) {
        contentW += 6.0f;
        badgeW = canvas.MeasureText(item.badge, badgeFontSize).x + 12.0f;
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
        const Rect textR(curX, bounds.y, textW, bounds.h);
        canvas.DrawTextBox(item.label, textR, m.fontSize, fgCol, 0.0f, 0.5f);
        curX += textW;
    }

    // Draw Badge
    if (!item.badge.empty()) {
        curX += 6.0f;
        const float badgeH = badgeFontSize + 6.0f;
        const Rect badgeR(curX, centerY - badgeH * 0.5f, badgeW, badgeH);

        Color badgeBg = active ? pal.accent.WithAlphaF(0.25f) : pal.surfaceHover;
        Color badgeText = active ? pal.accent : pal.textSecondary;

        // Highlight "Live" badges in emerald green
        if (item.badge == "Live") {
            const VariantColors success = theme.GetVariant(UiVariant::Success);
            badgeBg = success.bg.WithAlphaF(0.5f);
            badgeText = success.text;
        }

        canvas.DrawRoundRect(badgeR, 4.0f, badgeBg);
        canvas.DrawTextBox(item.badge, badgeR, badgeFontSize, badgeText, 0.5f, 0.5f);
    }
}

} // namespace guikit
