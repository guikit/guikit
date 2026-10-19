#include "guikit/widgets/Segmented.hpp"
#include <algorithm>
#include <cmath>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

ControlSpec EffectiveSegmentedControl(const SegmentedOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& sm = theme.GetSelect(size);
    ControlSpec c{};
    c.width = opts.width;
    c.height = opts.height;
    c.minWidth = opts.minWidth;
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
    c.hasIntrinsicBox = true;
    return c;
}

float MeasureSegmentNaturalWidth(
    const SegmentedItem& item,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const float fontSize = theme.ResolveFontSize(item.text, TextRole::LabelMedium, size);
    const float textW = item.text.HasContentOrReserve()
        ? TextUtil::MeasureUiTextWidth(item.text, fontSize, canvas)
        : 0.0f;
    const bool hasIcon = item.icon.IsValid();
    const bool hasText = (textW > 0.0f);
    const float padX = std::max(8.0f, cm.paddingX * 0.78f);
    const float iconGap = 6.0f;

    if (hasIcon && hasText) {
        if (item.iconPlacement == Placement::Top || item.iconPlacement == Placement::Bottom) {
            return std::max(textW, cm.iconSize) + padX * 2.0f;
        }
        return textW + cm.iconSize + iconGap + padX * 2.0f;
    }
    if (hasIcon) {
        return cm.iconSize + padX * 1.6f;
    }
    return textW + padX * 2.0f;
}

Vec2 MeasureSegmentedBox(
    std::span<const SegmentedItem> items,
    const SegmentedOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const float inset = (size <= UiSize::Sm) ? 2.0f : 3.0f;

    bool hasStackedIconText = false;
    float maxSegW = 28.0f;
    float sumSegW = 0.0f;
    for (const SegmentedItem& it : items) {
        const float sw = MeasureSegmentNaturalWidth(it, size, canvas, theme);
        maxSegW = std::max(maxSegW, sw);
        sumSegW += sw;
        if (it.icon.IsValid() && it.text.HasContentOrReserve() &&
            (it.iconPlacement == Placement::Top || it.iconPlacement == Placement::Bottom)) {
            hasStackedIconText = true;
        }
    }

    const size_t count = std::max<size_t>(1, items.size());
    float totalW = opts.equalWidth
        ? (maxSegW * static_cast<float>(count) + inset * 2.0f)
        : (sumSegW + inset * 2.0f);

    if (opts.minWidth.has_value()) {
        totalW = std::max(theme.Scale(*opts.minWidth), totalW);
    }
    if (opts.stretch != StretchMode::Fill && opts.width.has_value()) {
        totalW = theme.Scale(*opts.width);
    }

    float defaultH = cm.defaultHeight;
    if (hasStackedIconText) {
        const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
        defaultH = std::max(defaultH, cm.iconSize + fontSize + 14.0f);
    }
    const float boxH = opts.height.has_value() ? theme.Scale(*opts.height) : defaultH;
    return {totalW, boxH};
}

void ComputeSegmentRects(
    Rect trackRect,
    std::span<const SegmentedItem> items,
    const SegmentedOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    std::vector<Rect>& outRects
) {
    outRects.clear();
    if (items.empty()) return;
    outRects.reserve(items.size());

    const float inset = (size <= UiSize::Sm) ? 2.0f : 3.0f;
    const Rect inner = trackRect.Inset(inset);
    if (inner.w <= 0.0f || inner.h <= 0.0f) {
        for (size_t i = 0; i < items.size(); ++i) {
            outRects.emplace_back(trackRect.x, trackRect.y, 0.0f, 0.0f);
        }
        return;
    }

    if (opts.equalWidth || items.size() == 1) {
        const float segW = inner.w / static_cast<float>(items.size());
        for (size_t i = 0; i < items.size(); ++i) {
            outRects.emplace_back(inner.x + static_cast<float>(i) * segW, inner.y, segW, inner.h);
        }
    } else {
        std::vector<float> natW(items.size(), 1.0f);
        float sumNat = 0.0f;
        for (size_t i = 0; i < items.size(); ++i) {
            natW[i] = std::max(1.0f, MeasureSegmentNaturalWidth(items[i], size, canvas, theme));
            sumNat += natW[i];
        }
        float curX = inner.x;
        for (size_t i = 0; i < items.size(); ++i) {
            const float w = (i + 1 == items.size())
                ? std::max(0.0f, inner.Right() - curX)
                : (inner.w * (natW[i] / std::max(1.0f, sumNat)));
            outRects.emplace_back(curX, inner.y, w, inner.h);
            curX += w;
        }
    }
}

Color ResolveContrastingTextOnFill(Color fill, const ThemePalette& pal) noexcept {
    const float lum = (0.299f * static_cast<float>(fill.r) +
                       0.587f * static_cast<float>(fill.g) +
                       0.114f * static_cast<float>(fill.b)) / 255.0f;
    return (lum > 0.62f) ? Color(15, 23, 42, 255) : pal.controlKnobActive;
}

} // namespace

Vec2 Segmented::Measure(
    std::span<const SegmentedItem> items,
    const SegmentedOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const Vec2 boxDims = MeasureSegmentedBox(items, opts, size, canvas, theme);
    const ControlSpec ctrl = EffectiveSegmentedControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, opts.stretch, size, theme);
}

Vec2 Segmented::Measure(
    std::span<const SegmentedItem> items,
    const SegmentedOptions& opts,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(items, opts, size, canvas, defaultTheme, availableWidth);
}

Response Segmented::Run(
    Context& ctx,
    size_t* selectedIndex,
    int* selectedIntIndex,
    uint32_t* selectedMask,
    std::span<const std::string_view> items,
    const SegmentedOptions& opts,
    std::source_location loc
) {
    std::vector<SegmentedItem> richItems;
    richItems.reserve(items.size());
    for (std::string_view sv : items) {
        richItems.push_back(SegmentedItem{.text = sv});
    }
    return Run(ctx, selectedIndex, selectedIntIndex, selectedMask, richItems, opts, loc);
}

Response Segmented::Run(
    Context& ctx,
    size_t* selectedIndex,
    int* selectedIntIndex,
    uint32_t* selectedMask,
    std::span<const SegmentedItem> items,
    const SegmentedOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();

    SegmentedOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    const Vec2 naturalDims = MeasureSegmentedBox(items, optsCopy, activeSize, canvas, theme);
    const float unscaledW = (theme.scale > 0.0f) ? (naturalDims.x / theme.scale) : naturalDims.x;
    if (optsCopy.stretch != StretchMode::Fill && !optsCopy.width.has_value()) {
        optsCopy.width = unscaledW;
    }

    ctx.RegisterFocusable(id, effectiveDisabled);
    Response res = ctx.Interact(id);

    const bool multiSelect = (selectedMask != nullptr);
    size_t currIndex = size_t(-1);
    uint32_t currMask = 0;

    if (multiSelect) {
        currMask = *selectedMask;
    } else if (selectedIndex != nullptr) {
        if (*selectedIndex < items.size()) currIndex = *selectedIndex;
    } else if (selectedIntIndex != nullptr) {
        if (*selectedIntIndex >= 0 && static_cast<size_t>(*selectedIntIndex) < items.size()) {
            currIndex = static_cast<size_t>(*selectedIntIndex);
        }
    }

    auto applySingleSelection = [&](size_t newIdx) {
        if (newIdx < items.size() && !items[newIdx].disabled && currIndex != newIdx) {
            currIndex = newIdx;
            if (selectedIndex != nullptr) *selectedIndex = newIdx;
            if (selectedIntIndex != nullptr) *selectedIntIndex = static_cast<int>(newIdx);
            res.changed = true;
        }
    };

    auto toggleMaskBit = [&](size_t bitIdx) {
        if (bitIdx < items.size() && bitIdx < 32 && !items[bitIdx].disabled) {
            currMask ^= (1u << static_cast<uint32_t>(bitIdx));
            if (selectedMask != nullptr) *selectedMask = currMask;
            res.changed = true;
        }
    };

    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    std::vector<Rect> segRects;
    ComputeSegmentRects(prevBounds, items, optsCopy, activeSize, canvas, theme, segRects);

    int hoveredSegment = -1;
    int pressedSegment = -1;

    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    } else {
        const Vec2 mouse = ctx.GetMousePos();
        if (res.hovered || res.pressed || res.clicked) {
            for (size_t i = 0; i < segRects.size(); ++i) {
                if (segRects[i].Contains(mouse)) {
                    if (!items[i].disabled) {
                        hoveredSegment = static_cast<int>(i);
                        if (res.pressed) {
                            pressedSegment = static_cast<int>(i);
                        }
                    }
                    break;
                }
            }
        }

        if (res.clicked) {
            ctx.RequestFocus(id);
            if (hoveredSegment >= 0) {
                if (multiSelect) {
                    toggleMaskBit(static_cast<size_t>(hoveredSegment));
                } else {
                    applySingleSelection(static_cast<size_t>(hoveredSegment));
                }
            }
        }

        // Keyboard navigation when focused in single-select mode
        if (ctx.IsFocused(id) && !multiSelect && !items.empty()) {
            if (ctx.ConsumeKey(Key::Right) || ctx.ConsumeKey(Key::Down)) {
                const size_t start = (currIndex == size_t(-1)) ? 0 : (currIndex + 1);
                for (size_t i = start; i < items.size(); ++i) {
                    if (!items[i].disabled) {
                        applySingleSelection(i);
                        break;
                    }
                }
            } else if (ctx.ConsumeKey(Key::Left) || ctx.ConsumeKey(Key::Up)) {
                if (currIndex != size_t(-1) && currIndex > 0) {
                    for (size_t k = currIndex; k > 0; --k) {
                        if (!items[k - 1].disabled) {
                            applySingleSelection(k - 1);
                            break;
                        }
                    }
                }
            } else if (ctx.ConsumeKey(Key::Home)) {
                for (size_t i = 0; i < items.size(); ++i) {
                    if (!items[i].disabled) {
                        applySingleSelection(i);
                        break;
                    }
                }
            } else if (ctx.ConsumeKey(Key::End)) {
                for (size_t k = items.size(); k > 0; --k) {
                    if (!items[k - 1].disabled) {
                        applySingleSelection(k - 1);
                        break;
                    }
                }
            }
        }
    }

    if (res.hovered) {
        if (hoveredSegment >= 0 && !items[static_cast<size_t>(hoveredSegment)].tooltip.empty()) {
            ctx.SetTooltip(items[static_cast<size_t>(hoveredSegment)].tooltip, segRects[static_cast<size_t>(hoveredSegment)]);
        } else if (!optsCopy.tooltip.empty()) {
            ctx.SetTooltip(optsCopy.tooltip, prevBounds);
        }
    }

    Node* node = ctx.CreateNode(NodeType::Segmented, id);

    optsCopy.tooltip = ctx.AllocText(optsCopy.tooltip);
    SegmentedItem* arenaItems = items.empty()
        ? nullptr
        : static_cast<SegmentedItem*>(ctx.GetArena().AllocateRaw(sizeof(SegmentedItem) * items.size(), alignof(SegmentedItem)));
    uint64_t itemsHash = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        ::new (&arenaItems[i]) SegmentedItem(items[i]);
        arenaItems[i].text = ctx.AllocText(items[i].text);
        arenaItems[i].tooltip = ctx.AllocText(items[i].tooltip);
        itemsHash = detail::HashArgs(
            itemsHash,
            arenaItems[i].text,
            arenaItems[i].icon,
            static_cast<uint8_t>(arenaItems[i].iconPlacement),
            arenaItems[i].disabled
        );
    }

    auto* data = ctx.AllocPayload<SegmentedData>();
    data->opts = optsCopy;
    data->items = std::span<const SegmentedItem>(arenaItems, items.size());
    data->selectedIndex = currIndex;
    data->selectedMask = currMask;
    data->multiSelect = multiSelect;
    data->hoveredSegment = hoveredSegment;
    data->pressedSegment = pressedSegment;

    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        itemsHash,
        currIndex,
        currMask,
        multiSelect,
        hoveredSegment,
        pressedSegment,
        static_cast<uint8_t>(optsCopy.stretch),
        static_cast<uint8_t>(optsCopy.align),
        optsCopy.width.value_or(0.0f),
        optsCopy.height.value_or(0.0f),
        optsCopy.minWidth.value_or(0.0f),
        optsCopy.color,
        optsCopy.background,
        optsCopy.border,
        optsCopy.equalWidth,
        optsCopy.showDividers
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* c, const UiTheme& t) noexcept -> Vec2 {
        const auto* d = n->As<SegmentedData>();
        if (!d) return {0.0f, 0.0f};
        return Segmented::Measure(d->items, d->opts, n->uiSize, c, t, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& c, const UiTheme& t) noexcept {
        const auto* d = n->As<SegmentedData>();
        if (!d) return;
        Segmented::Render(
            c,
            n->computedRect,
            d->items,
            d->selectedIndex,
            d->selectedMask,
            d->multiSelect,
            d->hoveredSegment,
            d->pressedSegment,
            d->opts,
            n->uiSize,
            n->disabled,
            t,
            n->focused
        );
    };
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->active = res.pressed;
    node->focused = ctx.IsFocused(id);

    const ControlSpec ctrl = EffectiveSegmentedControl(optsCopy, activeSize, theme);
    Control::SetupNode(ctx, node, ctrl, optsCopy.stretch, optsCopy.align);
    ctx.AdaptChildToContainer(node);

    return res;
}

void Segmented::Render(
    ICanvas& canvas,
    Rect bounds,
    std::span<const SegmentedItem> items,
    size_t selectedIndex,
    uint32_t selectedMask,
    bool multiSelect,
    int hoveredSegment,
    int pressedSegment,
    const SegmentedOptions& opts,
    UiSize size,
    bool disabled,
    const UiTheme& theme,
    bool focused
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const ComponentMetrics& cm = theme.GetMetrics(size);
    const SelectMetrics& sm = theme.GetSelect(size);
    const auto& pal = theme.palette;
    const ControlSpec ctrl = EffectiveSegmentedControl(opts, size, theme);
    const Rect trackRect = bounds;
    const float inset = (size <= UiSize::Sm) ? 2.0f : 3.0f;
    const float trackRadius = ctrl.border.radius.has_value()
        ? theme.Scale(*ctrl.border.radius)
        : cm.cornerRadius;

    // 1. Render outer segmented track well
    Color trackBg = ctrl.HasBackground(true) ? pal.inputBackground : Color::Transparent();
    if (ctrl.HasBackground(true) && ctrl.background.HasAny() && !ctrl.background.IsSurface()) {
        trackBg = ctrl.background.Resolve(hoveredSegment >= 0, pressedSegment >= 0, disabled, trackBg);
    }
    Color trackBorder = focused ? pal.focusRing : pal.inputBorder;
    if (ctrl.border.color.HasAny()) {
        trackBorder = ctrl.border.color.Resolve(hoveredSegment >= 0, focused, disabled, trackBorder);
    }
    float trackStrokeW = (ctrl.border.width > 0.0f) ? theme.Scale(ctrl.border.width) : sm.strokeWidth;
    if (focused && !disabled && trackStrokeW > 0.0f) trackStrokeW += 0.5f;
    if (disabled) {
        trackBg = trackBg.WithAlphaF(0.40f);
        trackBorder = trackBorder.WithAlphaF(0.40f);
    }
    canvas.DrawRoundRect(trackRect, trackRadius, trackBg, trackBorder, trackStrokeW);

    if (items.empty()) return;

    std::vector<Rect> segRects;
    ComputeSegmentRects(trackRect, items, opts, size, &canvas, theme, segRects);
    const float pillRadius = std::max(2.0f, trackRadius - inset);

    auto isSegmentSelected = [&](size_t idx) noexcept -> bool {
        if (multiSelect) {
            return idx < 32 && ((selectedMask & (1u << static_cast<uint32_t>(idx))) != 0u);
        }
        return idx == selectedIndex;
    };

    // 2. Render subtle vertical dividers between adjacent unselected segments
    if (opts.showDividers && items.size() > 1) {
        const Color divCol = pal.borderSubtle.WithAlpha(disabled ? 55 : 120);
        for (size_t i = 0; i + 1 < items.size(); ++i) {
            const bool curSel = isSegmentSelected(i);
            const bool nextSel = isSegmentSelected(i + 1);
            const bool curHov = (hoveredSegment == static_cast<int>(i));
            const bool nextHov = (hoveredSegment == static_cast<int>(i + 1));
            if (!curSel && !nextSel && !curHov && !nextHov) {
                const float x = std::round(segRects[i].Right());
                const float padY = std::max(4.0f, segRects[i].h * 0.22f);
                canvas.DrawLine({x, segRects[i].y + padY}, {x, segRects[i].Bottom() - padY}, divCol, 1.0f);
            }
        }
    }

    // 3. Render segments (active pill, hover highlight, icon, and text)
    for (size_t i = 0; i < items.size(); ++i) {
        const SegmentedItem& item = items[i];
        const Rect sr = segRects[i];
        if (sr.w <= 0.0f || sr.h <= 0.0f) continue;

        const bool segDisabled = disabled || item.disabled;
        const bool selected = isSegmentSelected(i);
        const bool hovered = !segDisabled && (hoveredSegment == static_cast<int>(i));
        const bool pressed = !segDisabled && (pressedSegment == static_cast<int>(i));

        Color fgColor = segDisabled ? pal.textMuted : (hovered ? pal.textPrimary : pal.textSecondary);

        if (selected) {
            Color pillFill{};
            Color pillStroke{0, 0, 0, 0};
            float pillStrokeW = 0.0f;

            if (opts.color.HasAny()) {
                pillFill = opts.color.Resolve(hovered, pressed, segDisabled, pal.accent);
                fgColor = segDisabled ? pal.textMuted : ResolveContrastingTextOnFill(pillFill, pal);
            } else {
                // Default elevated segment pill with subtle primary accent tint & crisp border
                const Color basePill = Color::Lerp(pal.surfaceHover, pal.accent, 0.24f);
                pillFill = segDisabled
                    ? pal.surface.WithAlphaF(0.45f)
                    : (pressed ? StateColor::DarkenColor(basePill)
                               : (hovered ? StateColor::BrightenColor(basePill) : basePill));
                pillStroke = segDisabled ? pal.borderSubtle : pal.accent.WithAlpha(165);
                pillStrokeW = 1.0f;
                fgColor = segDisabled ? pal.textMuted : pal.textPrimary;
            }
            canvas.DrawRoundRect(sr, pillRadius, pillFill, pillStroke, pillStrokeW);
        } else if (hovered || pressed) {
            const Color hovFill = pressed
                ? pal.surfaceActive.WithAlpha(160)
                : pal.surfaceHover.WithAlpha(115);
            canvas.DrawRoundRect(sr, pillRadius, hovFill);
        }

        // Draw icon and/or text inside segment rect
        const bool hasIcon = item.icon.IsValid();
        const bool hasText = !item.text.empty();
        const float fontSize = theme.ResolveFontSize(item.text, TextRole::LabelMedium, size);
        const float iconSz = cm.iconSize;

        if (hasIcon && hasText) {
            const float textW = canvas.MeasureText(item.text.text, fontSize).x;
            if (item.iconPlacement == Placement::Top || item.iconPlacement == Placement::Bottom) {
                const float gapY = 2.0f;
                const float totalH = iconSz + gapY + fontSize;
                const float startY = sr.y + (sr.h - totalH) * 0.5f;
                if (item.iconPlacement == Placement::Top) {
                    const Rect iconRect(sr.x + (sr.w - iconSz) * 0.5f, startY, iconSz, iconSz);
                    item.icon.Draw(canvas, iconRect, fgColor);
                    const Rect textRect(sr.x, startY + iconSz + gapY, sr.w, fontSize + 2.0f);
                    canvas.DrawTextBox(item.text.text, textRect, fontSize, fgColor, 0.5f, 0.5f);
                } else {
                    const Rect textRect(sr.x, startY, sr.w, fontSize + 2.0f);
                    canvas.DrawTextBox(item.text.text, textRect, fontSize, fgColor, 0.5f, 0.5f);
                    const Rect iconRect(sr.x + (sr.w - iconSz) * 0.5f, startY + fontSize + gapY, iconSz, iconSz);
                    item.icon.Draw(canvas, iconRect, fgColor);
                }
            } else {
                const float gapX = 5.0f;
                const float totalW = iconSz + gapX + textW;
                const float startX = sr.x + (sr.w - totalW) * 0.5f;
                const float iconY = sr.y + (sr.h - iconSz) * 0.5f;
                if (item.iconPlacement == Placement::Trailing) {
                    const Rect textRect(startX, sr.y, textW + 2.0f, sr.h);
                    canvas.DrawTextBox(item.text.text, textRect, fontSize, fgColor, 0.0f, 0.5f);
                    const Rect iconRect(startX + textW + gapX, iconY, iconSz, iconSz);
                    item.icon.Draw(canvas, iconRect, fgColor);
                } else {
                    const Rect iconRect(startX, iconY, iconSz, iconSz);
                    item.icon.Draw(canvas, iconRect, fgColor);
                    const Rect textRect(startX + iconSz + gapX, sr.y, textW + 2.0f, sr.h);
                    canvas.DrawTextBox(item.text.text, textRect, fontSize, fgColor, 0.0f, 0.5f);
                }
            }
        } else if (hasIcon) {
            const Rect iconRect(sr.x + (sr.w - iconSz) * 0.5f, sr.y + (sr.h - iconSz) * 0.5f, iconSz, iconSz);
            item.icon.Draw(canvas, iconRect, fgColor);
        } else if (hasText) {
            canvas.DrawTextBox(item.text.text, sr.Inset(4.0f, 0.0f), fontSize, fgColor, 0.5f, 0.5f);
        }
    }
}

} // namespace guikit
