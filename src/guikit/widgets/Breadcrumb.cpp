#include "guikit/widgets/Breadcrumb.hpp"
#include <algorithm>
#include <cmath>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

struct BreadcrumbPersistentState {
    bool expanded{false};
    uint32_t lastMaxVisible{0};
    size_t lastItemCount{0};
};

struct BreadcrumbToken {
    size_t itemIndex{0};
    bool isEllipsis{false};
};

ControlSpec EffectiveBreadcrumbControl(const BreadcrumbOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& sm = theme.GetSelect(size);
    ControlSpec c{};
    c.width = opts.width;
    c.height = opts.height;
    c.align = opts.align;
    c.background = opts.background;
    c.border = opts.border;
    c.defaultLayout = ElementLayout::Stacked;
    c.defaultInlineOrder = InlineOrder::LabelFirst;
    c.labelGap = sm.labelGap;
    c.descGap = sm.descGap;
    c.inlineGap = sm.paddingX;
    c.stretchable = true;
    c.stretchHeightWhenStandalone = false;
    c.hasIntrinsicBox = false;
    return c;
}

void BuildBreadcrumbTokens(
    std::span<const BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    bool expanded,
    std::vector<BreadcrumbToken>& outTokens
) {
    outTokens.clear();
    if (items.empty()) return;

    const bool shouldCollapse = !expanded &&
                                (opts.maxVisibleItems >= 2) &&
                                (items.size() > static_cast<size_t>(opts.maxVisibleItems));

    if (!shouldCollapse) {
        outTokens.reserve(items.size());
        for (size_t i = 0; i < items.size(); ++i) {
            outTokens.push_back(BreadcrumbToken{.itemIndex = i, .isEllipsis = false});
        }
        return;
    }

    const size_t trailingCount = static_cast<size_t>(opts.maxVisibleItems - 1);
    outTokens.reserve(1 + 1 + trailingCount);
    outTokens.push_back(BreadcrumbToken{.itemIndex = 0, .isEllipsis = false});
    outTokens.push_back(BreadcrumbToken{.itemIndex = 0, .isEllipsis = true});

    const size_t startTrailing = items.size() - trailingCount;
    for (size_t i = startTrailing; i < items.size(); ++i) {
        outTokens.push_back(BreadcrumbToken{.itemIndex = i, .isEllipsis = false});
    }
}

float MeasureTokenNaturalWidth(
    std::span<const BreadcrumbItem> items,
    const BreadcrumbToken& token,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const float padX = std::max(theme.Scale(6.0f), cm.paddingX * 0.65f);

    if (token.isEllipsis) {
        const float dotsW = TextUtil::MeasureWidth("...", fontSize, canvas);
        return std::max(theme.Scale(26.0f), dotsW + padX * 1.8f);
    }

    if (token.itemIndex >= items.size()) {
        return padX * 2.0f;
    }

    const BreadcrumbItem& item = items[token.itemIndex];
    const float iconGap = theme.Scale(5.0f);
    const float badgeGap = theme.Scale(5.0f);
    const float badgeFontSize = fontSize * 0.82f;

    float contentW = 0.0f;
    if (item.icon.IsValid()) {
        contentW += cm.iconSize;
    }
    if (!item.label.empty()) {
        if (contentW > 0.0f) contentW += iconGap;
        contentW += TextUtil::MeasureWidth(item.label.View(), fontSize, canvas);
    }
    if (!item.badge.empty()) {
        if (contentW > 0.0f) contentW += badgeGap;
        contentW += TextUtil::MeasureWidth(item.badge.View(), badgeFontSize, canvas) + theme.Scale(10.0f);
    }

    return std::max(cm.defaultHeight * 0.75f, contentW + padX * 2.0f);
}

float MeasureSeparatorSlotWidth(UiSize size, const UiTheme& theme) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    return std::max(theme.Scale(14.0f), cm.iconSize * 1.05f);
}

Vec2 MeasureBreadcrumbNaturalBox(
    std::span<const BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    bool expanded,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    std::vector<BreadcrumbToken> tokens;
    BuildBreadcrumbTokens(items, opts, expanded, tokens);

    const float sepW = MeasureSeparatorSlotWidth(size, theme);
    float totalW = 0.0f;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) {
            totalW += sepW;
        }
        totalW += MeasureTokenNaturalWidth(items, tokens[i], size, canvas, theme);
    }

    if (opts.stretch != StretchMode::Fill && opts.width.has_value() && *opts.width > 0.0f) {
        totalW = theme.Scale(*opts.width);
    }

    const float boxH = (opts.height.has_value() && *opts.height > 0.0f)
        ? theme.Scale(*opts.height)
        : cm.defaultHeight;

    return {std::max(theme.Scale(24.0f), totalW), boxH};
}

void ComputeBreadcrumbLayout(
    Rect innerRect,
    std::span<const BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    bool expanded,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    std::vector<BreadcrumbToken>& outTokens,
    std::vector<Rect>& outTokenRects,
    std::vector<Rect>& outSepRects
) {
    BuildBreadcrumbTokens(items, opts, expanded, outTokens);
    outTokenRects.clear();
    outSepRects.clear();
    if (outTokens.empty()) return;

    outTokenRects.reserve(outTokens.size());
    outSepRects.reserve(outTokens.size() > 1 ? outTokens.size() - 1 : 0);

    const float sepW = MeasureSeparatorSlotWidth(size, theme);
    std::vector<float> widths(outTokens.size(), 0.0f);
    float totalW = 0.0f;
    for (size_t i = 0; i < outTokens.size(); ++i) {
        if (i > 0) totalW += sepW;
        widths[i] = MeasureTokenNaturalWidth(items, outTokens[i], size, canvas, theme);
        totalW += widths[i];
    }

    float startX = innerRect.x;
    if (opts.stretch == StretchMode::Fill && innerRect.w > totalW) {
        if (opts.align == Align::Center) {
            startX += (innerRect.w - totalW) * 0.5f;
        } else if (opts.align == Align::End) {
            startX += (innerRect.w - totalW);
        }
    }

    float curX = startX;
    for (size_t i = 0; i < outTokens.size(); ++i) {
        if (i > 0) {
            outSepRects.emplace_back(curX, innerRect.y, sepW, innerRect.h);
            curX += sepW;
        }
        outTokenRects.emplace_back(curX, innerRect.y, widths[i], innerRect.h);
        curX += widths[i];
    }
}

void DrawSeparatorGlyph(
    ICanvas& canvas,
    Rect sepRect,
    BreadcrumbSeparator separator,
    UiSize size,
    const UiTheme& theme,
    Color color
) {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const float iconSz = std::round(cm.iconSize * 0.78f);
    const Vec2 center = sepRect.Center();

    switch (separator) {
        case BreadcrumbSeparator::Chevron: {
            const Rect iconR(center.x - iconSz * 0.5f, center.y - iconSz * 0.5f, iconSz, iconSz);
            Icon::ChevronRight().Draw(canvas, iconR, color);
            break;
        }
        case BreadcrumbSeparator::Arrow: {
            const Rect iconR(center.x - iconSz * 0.5f, center.y - iconSz * 0.5f, iconSz, iconSz);
            Icon::ArrowRight().Draw(canvas, iconR, color);
            break;
        }
        case BreadcrumbSeparator::Slash: {
            canvas.DrawTextBox("/", sepRect, fontSize, color, 0.5f, 0.5f);
            break;
        }
        case BreadcrumbSeparator::Dot: {
            const float r = std::max(1.8f, theme.Scale(2.2f));
            canvas.DrawRoundRect(Rect(center.x - r, center.y - r, r * 2.0f, r * 2.0f), r, color);
            break;
        }
    }
}

} // namespace

Vec2 Breadcrumb::Measure(
    std::span<const BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const Vec2 boxDims = MeasureBreadcrumbNaturalBox(items, opts, false, size, canvas, theme);
    const ControlSpec ctrl = EffectiveBreadcrumbControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, opts.stretch, size, theme);
}

Response Breadcrumb::Run(
    Context& ctx,
    size_t& activeIndex,
    std::span<const std::string_view> items,
    const BreadcrumbOptions& opts,
    std::source_location loc
) {
    std::vector<BreadcrumbItem> richItems;
    richItems.reserve(items.size());
    for (std::string_view sv : items) {
        richItems.push_back(BreadcrumbItem{.label = UiString(sv)});
    }
    return Run(ctx, activeIndex, std::span<const BreadcrumbItem>(richItems), opts, loc);
}

Response Breadcrumb::Run(
    Context& ctx,
    int& activeIndex,
    std::span<const std::string_view> items,
    const BreadcrumbOptions& opts,
    std::source_location loc
) {
    size_t idx = (activeIndex >= 0) ? static_cast<size_t>(activeIndex) : 0;
    Response res = Run(ctx, idx, items, opts, loc);
    if (res.changed) {
        activeIndex = static_cast<int>(idx);
    }
    return res;
}

Response Breadcrumb::Run(
    Context& ctx,
    int& activeIndex,
    std::span<const BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    std::source_location loc
) {
    size_t idx = (activeIndex >= 0) ? static_cast<size_t>(activeIndex) : 0;
    Response res = Run(ctx, idx, items, opts, loc);
    if (res.changed) {
        activeIndex = static_cast<int>(idx);
    }
    return res;
}

Response Breadcrumb::Run(
    Context& ctx,
    size_t& activeIndex,
    std::span<const BreadcrumbItem> items,
    const BreadcrumbOptions& opts,
    std::source_location loc
) {
    UiTheme* pushedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();

    auto& state = ctx.GetOrCreateState<BreadcrumbPersistentState>(id);
    if (state.lastMaxVisible != opts.maxVisibleItems || state.lastItemCount != items.size()) {
        state.expanded = false;
        state.lastMaxVisible = opts.maxVisibleItems;
        state.lastItemCount = items.size();
    }

    BreadcrumbOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    const Vec2 naturalDims = MeasureBreadcrumbNaturalBox(items, optsCopy, state.expanded, activeSize, canvas, theme);
    const float unscaledW = (theme.scale > 0.0f) ? (naturalDims.x / theme.scale) : naturalDims.x;
    if (optsCopy.stretch != StretchMode::Fill && !optsCopy.width.has_value()) {
        optsCopy.width = unscaledW;
    }

    ctx.RegisterFocusable(id, effectiveDisabled);
    Response res = ctx.Interact(id);

    size_t currIndex = items.empty() ? 0 : std::min(activeIndex, items.size() - 1);

    const ControlSpec ctrl = EffectiveBreadcrumbControl(optsCopy, activeSize, theme);
    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    const Rect innerBounds = Control::ResolveInnerRect(prevBounds, ctrl, activeSize, theme);

    std::vector<BreadcrumbToken> tokens;
    std::vector<Rect> tokenRects;
    std::vector<Rect> sepRects;
    ComputeBreadcrumbLayout(innerBounds, items, optsCopy, state.expanded, activeSize, canvas, theme, tokens, tokenRects, sepRects);

    int hoveredToken = -1;
    int pressedToken = -1;

    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    } else {
        const Vec2 mouse = ctx.GetMousePos();
        if (res.hovered || res.pressed || res.clicked) {
            for (size_t i = 0; i < tokenRects.size(); ++i) {
                if (tokenRects[i].Contains(mouse)) {
                    hoveredToken = static_cast<int>(i);
                    if (res.pressed) {
                        pressedToken = static_cast<int>(i);
                    }
                    break;
                }
            }
        }

        if (res.clicked) {
            ctx.RequestFocus(id);
            if (hoveredToken >= 0 && static_cast<size_t>(hoveredToken) < tokens.size()) {
                const BreadcrumbToken& tok = tokens[static_cast<size_t>(hoveredToken)];
                if (tok.isEllipsis) {
                    state.expanded = true;
                    ctx.InvalidateAll();
                    ctx.RequestRepaint(3);
                } else if (tok.itemIndex < items.size()) {
                    if (currIndex != tok.itemIndex) {
                        currIndex = tok.itemIndex;
                        activeIndex = currIndex;
                        res.changed = true;
                    }
                    ctx.RequestRepaint(3);
                }
            }
        }

        // Keyboard navigation when focused
        if (ctx.IsFocused(id) && !items.empty()) {
            if (ctx.ConsumeKey(Key::Right) || ctx.ConsumeKey(Key::Down)) {
                if (currIndex + 1 < items.size()) {
                    ++currIndex;
                    activeIndex = currIndex;
                    res.changed = true;
                }
            } else if (ctx.ConsumeKey(Key::Left) || ctx.ConsumeKey(Key::Up)) {
                if (currIndex > 0) {
                    --currIndex;
                    activeIndex = currIndex;
                    res.changed = true;
                }
            } else if (ctx.ConsumeKey(Key::Home)) {
                if (currIndex != 0) {
                    currIndex = 0;
                    activeIndex = 0;
                    res.changed = true;
                }
            } else if (ctx.ConsumeKey(Key::End)) {
                if (currIndex != items.size() - 1) {
                    currIndex = items.size() - 1;
                    activeIndex = currIndex;
                    res.changed = true;
                }
            }
        }
    }

    if (res.hovered && !optsCopy.tooltip.empty()) {
        ctx.SetTooltip(optsCopy.tooltip.View(), prevBounds);
    }

    Node* node = ctx.CreateNode(NodeType::Breadcrumb, id);
    node->theme = pushedTheme;
    node->debugBoundsOverride = opts.debugBounds;
    node->inspectModeOverride = opts.inspectMode;

    optsCopy.tooltip = ctx.String(optsCopy.tooltip.View());
    BreadcrumbItem* arenaItems = items.empty()
        ? nullptr
        : static_cast<BreadcrumbItem*>(ctx.GetArena().AllocateRaw(sizeof(BreadcrumbItem) * items.size(), alignof(BreadcrumbItem)));
    uint64_t itemsHash = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        ::new (&arenaItems[i]) BreadcrumbItem(items[i]);
        arenaItems[i].label = ctx.String(items[i].label.View());
        arenaItems[i].badge = ctx.String(items[i].badge.View());
        if (!arenaItems[i].icon.GetPath().empty()) {
            arenaItems[i].icon = Icon::File(ctx.AllocString(arenaItems[i].icon.GetPath()));
        }
        itemsHash = detail::HashArgs(
            itemsHash,
            arenaItems[i].label.View(),
            arenaItems[i].icon,
            arenaItems[i].badge.View()
        );
    }

    auto* data = ctx.AllocPayload<BreadcrumbData>();
    data->opts = optsCopy;
    data->items = std::span<const BreadcrumbItem>(arenaItems, items.size());
    data->activeIndex = currIndex;
    data->expanded = state.expanded;
    data->hoveredToken = hoveredToken;
    data->pressedToken = pressedToken;

    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        itemsHash,
        currIndex,
        state.expanded,
        hoveredToken,
        pressedToken,
        static_cast<uint8_t>(optsCopy.separator),
        optsCopy.maxVisibleItems,
        static_cast<uint8_t>(optsCopy.stretch),
        static_cast<uint8_t>(optsCopy.align),
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.color,
        optsCopy.background,
        optsCopy.border
    );

    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* c, const UiTheme& t) noexcept -> Vec2 {
        const auto* d = n->As<BreadcrumbData>();
        if (!d) return {0.0f, 0.0f};
        BreadcrumbOptions measureOpts = d->opts;
        if (d->expanded) {
            measureOpts.maxVisibleItems = 0;
        }
        return Breadcrumb::Measure(d->items, measureOpts, n->uiSize, c, t);
    };
    node->renderFn = &Breadcrumb::Render;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->active = res.pressed;
    node->focused = ctx.IsFocused(id);

    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);

    if (pushedTheme) {
        ctx.PopTheme();
    }

    return res;
}

void Breadcrumb::Render(
    const Node* node,
    ICanvas& canvas,
    const UiTheme& theme
) {
    if (!node) return;
    const auto* d = node->As<BreadcrumbData>();
    if (!d) return;

    const Rect bounds = node->computedRect;
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const UiSize size = node->uiSize;
    const bool disabled = node->disabled;
    const bool focused = node->focused;
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const auto& pal = theme.palette;

    const ControlSpec ctrl = EffectiveBreadcrumbControl(d->opts, size, theme);
    Control::RenderFrame(canvas, bounds, ctrl, size, disabled, theme, d->hoveredToken >= 0, d->pressedToken >= 0 || focused);
    const Rect innerRect = Control::ResolveInnerRect(bounds, ctrl, size, theme);

    if (d->items.empty()) return;

    std::vector<BreadcrumbToken> tokens;
    std::vector<Rect> tokenRects;
    std::vector<Rect> sepRects;
    ComputeBreadcrumbLayout(innerRect, d->items, d->opts, d->expanded, size, &canvas, theme, tokens, tokenRects, sepRects);

    canvas.PushClip(bounds);

    // 1. Render separators between tokens
    const Color sepColor = disabled ? pal.textDisabled : pal.textMuted;
    for (const Rect& sr : sepRects) {
        DrawSeparatorGlyph(canvas, sr, d->opts.separator, size, theme, sepColor);
    }

    // 2. Render tokens (items & interactive "..." collapse pill)
    const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
    const float badgeFontSize = fontSize * 0.82f;
    const float pillRadius = std::max(theme.Scale(4.0f), cm.cornerRadius * 0.85f);
    const float pillInsetY = std::max(theme.Scale(2.0f), innerRect.h * 0.10f);

    for (size_t i = 0; i < tokens.size(); ++i) {
        const BreadcrumbToken& tok = tokens[i];
        const Rect tr = tokenRects[i];
        const Rect pillRect(tr.x, tr.y + pillInsetY, tr.w, std::max(0.0f, tr.h - pillInsetY * 2.0f));

        const bool isHovered = !disabled && (d->hoveredToken == static_cast<int>(i));
        const bool isPressed = !disabled && (d->pressedToken == static_cast<int>(i));
        const bool isActiveItem = !tok.isEllipsis && (tok.itemIndex == d->activeIndex);

        const Color accentColor = d->opts.color.HasAny()
            ? d->opts.color.Resolve(isHovered, isPressed, disabled, pal.accent)
            : pal.accent;

        if (tok.isEllipsis) {
            const Color pillBg = isPressed
                ? pal.surfaceActive
                : (isHovered ? pal.surfaceHover : pal.surfaceHover.WithAlphaF(0.55f));
            const Color pillBorder = isHovered ? pal.borderHover : pal.borderSubtle;
            canvas.DrawRoundRect(pillRect, pillRadius, pillBg, pillBorder, 1.0f);

            const Color dotsCol = disabled ? pal.textDisabled : (isHovered ? pal.textPrimary : pal.textSecondary);
            canvas.DrawTextBox("...", pillRect, fontSize, dotsCol, 0.5f, 0.5f);
            continue;
        }

        if (tok.itemIndex >= d->items.size()) continue;
        const BreadcrumbItem& item = d->items[tok.itemIndex];

        // Pill background for active or hovered breadcrumb item
        if (isActiveItem) {
            const Color activeFill = d->opts.color.HasAny()
                ? accentColor.WithAlphaF(disabled ? 0.10f : (isHovered ? 0.24f : 0.16f))
                : pal.accent.WithAlphaF(disabled ? 0.10f : (isHovered ? 0.22f : 0.14f));
            const Color activeStroke = (focused && !disabled)
                ? pal.focusRing
                : accentColor.WithAlphaF(disabled ? 0.20f : 0.45f);
            canvas.DrawRoundRect(pillRect, pillRadius, activeFill, activeStroke, 1.0f);
        } else if (isHovered || isPressed) {
            const Color hovFill = isPressed ? pal.surfaceActive : pal.surfaceHover;
            canvas.DrawRoundRect(pillRect, pillRadius, hovFill);
        }

        Color fgColor = disabled
            ? pal.textDisabled
            : (isActiveItem
                ? (d->opts.color.HasAny() ? accentColor : pal.textPrimary)
                : (isHovered ? pal.textPrimary : pal.textSecondary));

        const float iconGap = theme.Scale(5.0f);
        const float badgeGap = theme.Scale(5.0f);

        float contentW = 0.0f;
        if (item.icon.IsValid()) {
            contentW += cm.iconSize;
        }
        float labelW = 0.0f;
        if (!item.label.empty()) {
            if (contentW > 0.0f) contentW += iconGap;
            labelW = canvas.MeasureText(item.label.View(), fontSize).x;
            contentW += labelW;
        }
        float badgeW = 0.0f;
        if (!item.badge.empty()) {
            if (contentW > 0.0f) contentW += badgeGap;
            badgeW = canvas.MeasureText(item.badge.View(), badgeFontSize).x + theme.Scale(10.0f);
            contentW += badgeW;
        }

        float curX = tr.x + std::max(0.0f, (tr.w - contentW) * 0.5f);
        const float centerY = tr.y + tr.h * 0.5f;

        if (item.icon.IsValid()) {
            const Rect iconR(curX, centerY - cm.iconSize * 0.5f, cm.iconSize, cm.iconSize);
            item.icon.Draw(canvas, iconR, isActiveItem ? accentColor : fgColor);
            curX += cm.iconSize + iconGap;
        }

        if (!item.label.empty()) {
            const Rect textR(curX, tr.y, labelW + 2.0f, tr.h);
            canvas.DrawTextBox(item.label.View(), textR, fontSize, fgColor, 0.0f, 0.5f);
            curX += labelW + badgeGap;
        }

        if (!item.badge.empty()) {
            const float badgeH = badgeFontSize + theme.Scale(5.0f);
            const Rect badgeR(curX, centerY - badgeH * 0.5f, badgeW, badgeH);
            const Color badgeBg = isActiveItem ? accentColor.WithAlphaF(0.25f) : pal.surfaceHover;
            const Color badgeFg = isActiveItem ? accentColor : pal.textSecondary;
            canvas.DrawRoundRect(badgeR, theme.Scale(4.0f), badgeBg);
            canvas.DrawTextBox(item.badge.View(), badgeR, badgeFontSize, badgeFg, 0.5f, 0.5f);
        }
    }

    canvas.PopClip();
}

} // namespace guikit
