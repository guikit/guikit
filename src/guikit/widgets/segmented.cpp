#include "guikit/widgets/segmented.hpp"
#include <cmath>
#include "guikit/context.hpp"
#include "guikit/backend/i_canvas.hpp"
#include "guikit/core/node.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/small_array.h"

namespace guikit {

namespace {

ControlSpec EffectiveSegmentedControl(const SegmentedOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& sm = theme.GetSelect(size);
    ControlSpec c{};
    c.width = opts.width;
    c.height = opts.height;
    c.defaultWidthMode = SizeMode::Stretch;
    c.defaultHeightMode = SizeMode::Fit;
    c.minWidth = opts.minWidth;
    c.defaultLayout = ElementLayout::Stacked;
    c.labelGap = sm.labelGap;
    c.descGap = sm.descGap;
    c.inlineGap = sm.paddingX;
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
    const float padX = keel::max(8.0f, cm.paddingX * 0.78f);
    const float iconGap = 6.0f;

    if (hasIcon && hasText) {
        if (item.iconPlacement == Placement::Top || item.iconPlacement == Placement::Bottom) {
            return keel::max(textW, cm.iconSize) + padX * 2.0f;
        }
        return textW + cm.iconSize + iconGap + padX * 2.0f;
    }
    if (hasIcon) {
        return cm.iconSize + padX * 1.6f;
    }
    return textW + padX * 2.0f;
}

Vec2 MeasureSegmentedBox(
    keel::ArrayRef<SegmentedItem> items,
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
        maxSegW = keel::max(maxSegW, sw);
        sumSegW += sw;
        if (it.icon.IsValid() && it.text.HasContentOrReserve() &&
            (it.iconPlacement == Placement::Top || it.iconPlacement == Placement::Bottom)) {
            hasStackedIconText = true;
        }
    }

    const keel::usize count = keel::max<keel::usize>(1, items.size());
    float totalW = opts.equalWidth
        ? (maxSegW * static_cast<float>(count) + inset * 2.0f)
        : (sumSegW + inset * 2.0f);

    if (opts.minWidth.has_value()) {
        totalW = keel::max(theme.Scale(*opts.minWidth), totalW);
    }
    if (opts.width.IsFixed()) {
        totalW = theme.Scale(opts.width.Dp());
    }

    float defaultH = cm.defaultHeight;
    if (hasStackedIconText) {
        const float fontSize = theme.GetFontSize(TextRole::LabelMedium, size);
        defaultH = keel::max(defaultH, cm.iconSize + fontSize + 14.0f);
    }
    const float boxH = opts.height.IsFixed() ? theme.Scale(opts.height.Dp()) : defaultH;
    return {totalW, boxH};
}

void ComputeSegmentRects(
    Rect trackRect,
    keel::ArrayRef<SegmentedItem> items,
    const SegmentedOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::SmallArray<Rect, 16>& outRects
) {
    outRects.clear();
    if (items.empty()) return;
    outRects.reserve(items.size());

    const float inset = (size <= UiSize::Sm) ? 2.0f : 3.0f;
    const Rect inner = trackRect.Inset(inset);
    if (inner.w <= 0.0f || inner.h <= 0.0f) {
        for (keel::usize i = 0; i < items.size(); ++i) {
            outRects.emplace_back(trackRect.x, trackRect.y, 0.0f, 0.0f);
        }
        return;
    }

    if (opts.equalWidth || items.size() == 1) {
        const float segW = inner.w / static_cast<float>(items.size());
        for (keel::usize i = 0; i < items.size(); ++i) {
            outRects.emplace_back(inner.x + static_cast<float>(i) * segW, inner.y, segW, inner.h);
        }
    } else {
        keel::SmallArray<float, 16> natW(items.size(), 1.0f);
        float sumNat = 0.0f;
        for (keel::usize i = 0; i < items.size(); ++i) {
            natW[i] = keel::max(1.0f, MeasureSegmentNaturalWidth(items[i], size, canvas, theme));
            sumNat += natW[i];
        }
        float curX = inner.x;
        for (keel::usize i = 0; i < items.size(); ++i) {
            const float w = (i + 1 == items.size())
                ? keel::max(0.0f, inner.Right() - curX)
                : (inner.w * (natW[i] / keel::max(1.0f, sumNat)));
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
    keel::ArrayRef<SegmentedItem> items,
    const SegmentedOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float /*availableWidth*/
) {
    const Vec2 boxDims = MeasureSegmentedBox(items, opts, size, canvas, theme);
    const ControlSpec ctrl = EffectiveSegmentedControl(opts, size, theme);
    return Control::Measure(boxDims, ctrl, theme);
}

Vec2 Segmented::Measure(
    keel::ArrayRef<SegmentedItem> items,
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
    keel::usize* selectedIndex,
    keel::i32* selectedIntIndex,
    keel::u32* selectedMask,
    keel::ArrayRef<keel::StringRef> items,
    const SegmentedOptions& opts,
    keel::SourceLocation loc
) {
    keel::SmallArray<SegmentedItem, 16> richItems;
    richItems.reserve(items.size());
    for (keel::StringRef sv : items) {
        richItems.push_back(SegmentedItem{.text = sv});
    }
    return Run(ctx, selectedIndex, selectedIntIndex, selectedMask, richItems.as_ref(), opts, loc);
}

Response Segmented::Run(
    Context& ctx,
    keel::usize* selectedIndex,
    keel::i32* selectedIntIndex,
    keel::u32* selectedMask,
    keel::ArrayRef<SegmentedItem> items,
    const SegmentedOptions& opts,
    keel::SourceLocation loc
) {
    const ZoneScope zoneScope = ctx.Zone({.theme = opts.theme, .scale = opts.scale});

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = ctx.GetCurrentSize();
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiTheme theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();

    SegmentedOptions optsCopy = opts;
    const SizeMode effW = Control::ResolveWidthMode(ctx, opts.width, SizeMode::Stretch);
    if (!optsCopy.width.IsFixed()) {
        optsCopy.width = effW;
    }

    ctx.RegisterFocusable(id, effectiveDisabled);
    Response res = ctx.Interact(id);

    const bool multiSelect = (selectedMask != nullptr);
    keel::usize currIndex = keel::usize(-1);
    keel::u32 currMask = 0;

    if (multiSelect) {
        currMask = *selectedMask;
    } else if (selectedIndex != nullptr) {
        if (*selectedIndex < items.size()) currIndex = *selectedIndex;
    } else if (selectedIntIndex != nullptr) {
        if (*selectedIntIndex >= 0 && static_cast<keel::usize>(*selectedIntIndex) < items.size()) {
            currIndex = static_cast<keel::usize>(*selectedIntIndex);
        }
    }

    auto applySingleSelection = [&](keel::usize newIdx) {
        if (newIdx < items.size() && !items[newIdx].disabled && currIndex != newIdx) {
            currIndex = newIdx;
            if (selectedIndex != nullptr) *selectedIndex = newIdx;
            if (selectedIntIndex != nullptr) *selectedIntIndex = static_cast<keel::i32>(newIdx);
            res.changed = true;
        }
    };

    auto toggleMaskBit = [&](keel::usize bitIdx) {
        if (bitIdx < items.size() && bitIdx < 32 && !items[bitIdx].disabled) {
            currMask ^= (1u << static_cast<keel::u32>(bitIdx));
            if (selectedMask != nullptr) *selectedMask = currMask;
            res.changed = true;
        }
    };

    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    keel::SmallArray<Rect, 16> segRects;
    ComputeSegmentRects(prevBounds, items, optsCopy, activeSize, canvas, theme, segRects);

    keel::i32 hoveredSegment = -1;
    keel::i32 pressedSegment = -1;

    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    } else {
        const Vec2 mouse = ctx.GetMousePos();
        if (res.hovered || res.pressed || res.clicked) {
            for (keel::usize i = 0; i < segRects.size(); ++i) {
                if (segRects[i].Contains(mouse)) {
                    if (!items[i].disabled) {
                        hoveredSegment = static_cast<keel::i32>(i);
                        if (res.pressed) {
                            pressedSegment = static_cast<keel::i32>(i);
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
                    toggleMaskBit(static_cast<keel::usize>(hoveredSegment));
                } else {
                    applySingleSelection(static_cast<keel::usize>(hoveredSegment));
                }
            }
        }

        // Keyboard navigation when focused in single-select mode
        if (ctx.IsFocused(id) && !multiSelect && !items.empty()) {
            if (ctx.ConsumeKey(Key::Right) || ctx.ConsumeKey(Key::Down)) {
                const keel::usize start = (currIndex == keel::usize(-1)) ? 0 : (currIndex + 1);
                for (keel::usize i = start; i < items.size(); ++i) {
                    if (!items[i].disabled) {
                        applySingleSelection(i);
                        break;
                    }
                }
            } else if (ctx.ConsumeKey(Key::Left) || ctx.ConsumeKey(Key::Up)) {
                if (currIndex != keel::usize(-1) && currIndex > 0) {
                    for (keel::usize k = currIndex; k > 0; --k) {
                        if (!items[k - 1].disabled) {
                            applySingleSelection(k - 1);
                            break;
                        }
                    }
                }
            } else if (ctx.ConsumeKey(Key::Home)) {
                for (keel::usize i = 0; i < items.size(); ++i) {
                    if (!items[i].disabled) {
                        applySingleSelection(i);
                        break;
                    }
                }
            } else if (ctx.ConsumeKey(Key::End)) {
                for (keel::usize k = items.size(); k > 0; --k) {
                    if (!items[k - 1].disabled) {
                        applySingleSelection(k - 1);
                        break;
                    }
                }
            }
        }
    }

    if (res.hovered) {
        if (hoveredSegment >= 0) {
            const keel::usize hIdx = static_cast<keel::usize>(hoveredSegment);
            const Rect hRect = segRects[hIdx];
            if (!items[hIdx].tooltip.empty()) {
                ctx.SetTooltip(items[hIdx].tooltip, hRect);
            } else if (!items[hIdx].text.empty()) {
                const float natW = MeasureSegmentNaturalWidth(items[hIdx], activeSize, nullptr, theme);
                if (hRect.w + 1.0f < natW) {
                    ctx.SetTooltip(items[hIdx].text, hRect);
                } else if (!optsCopy.tooltip.empty()) {
                    ctx.SetTooltip(optsCopy.tooltip, prevBounds);
                }
            } else if (!optsCopy.tooltip.empty()) {
                ctx.SetTooltip(optsCopy.tooltip, prevBounds);
            }
        } else if (!optsCopy.tooltip.empty()) {
            ctx.SetTooltip(optsCopy.tooltip, prevBounds);
        }
    }

    Node* node = ctx.CreateNode(NodeType::Segmented, id);

    optsCopy.tooltip = ctx.AllocText(optsCopy.tooltip);
    SegmentedItem* arenaItems = ctx.GetArena().create_array<SegmentedItem>(items.size());
    keel::u64 itemsHash = 0;
    for (keel::usize i = 0; i < items.size(); ++i) {
        arenaItems[i] = items[i];
        arenaItems[i].text = ctx.AllocText(items[i].text);
        arenaItems[i].tooltip = ctx.AllocText(items[i].tooltip);
        arenaItems[i].icon = ctx.AllocIcon(items[i].icon);
        itemsHash = detail::HashArgs(
            itemsHash,
            arenaItems[i].text,
            arenaItems[i].icon,
            static_cast<keel::u8>(arenaItems[i].iconPlacement),
            arenaItems[i].disabled
        );
    }

    auto* data = ctx.AllocPayload<SegmentedData>();
    data->opts = optsCopy;
    data->items = keel::ArrayRef<SegmentedItem>(arenaItems, items.size());
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
        optsCopy.width,
        optsCopy.height,
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
    Control::SetupNode(ctx, node, ctrl);

    return res;
}

void Segmented::Render(
    ICanvas& canvas,
    Rect bounds,
    keel::ArrayRef<SegmentedItem> items,
    keel::usize selectedIndex,
    keel::u32 selectedMask,
    bool multiSelect,
    keel::i32 hoveredSegment,
    keel::i32 pressedSegment,
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
    const Rect trackRect = bounds;
    const float inset = (size <= UiSize::Sm) ? 2.0f : 3.0f;
    const float trackRadius = opts.border.radius.has_value()
        ? theme.Scale(*opts.border.radius)
        : cm.cornerRadius;

    // 1. Render outer segmented track well
    const bool hasBg = opts.background.IsBackgroundEnabled(true);
    Color trackBg = hasBg ? pal.inputBackground : Color::Transparent();
    if (hasBg && opts.background.HasAny()) {
        if (!opts.background.IsSurface()) {
            trackBg = theme.ResolveColor(opts.background, hoveredSegment >= 0, pressedSegment >= 0, disabled, trackBg);
        } else if (opts.background.HasAlphaScale()) {
            trackBg = trackBg.ScaleAlpha(opts.background.alphaScale);
        }
    }
    Color trackBorder = focused ? pal.focusRing : pal.inputBorder;
    if (opts.border.color.HasAny()) {
        trackBorder = theme.ResolveColor(opts.border.color, hoveredSegment >= 0, focused, disabled, trackBorder);
    }
    float trackStrokeW = (opts.border.width > 0.0f) ? theme.Scale(opts.border.width) : sm.strokeWidth;
    if (focused && !disabled && trackStrokeW > 0.0f) trackStrokeW += 0.5f;
    if (disabled) {
        trackBg = trackBg.WithAlphaF(0.40f);
        trackBorder = trackBorder.WithAlphaF(0.40f);
    }
    canvas.DrawRoundRect(trackRect, trackRadius, trackBg, trackBorder, trackStrokeW);

    if (items.empty()) return;

    keel::SmallArray<Rect, 16> segRects;
    ComputeSegmentRects(trackRect, items, opts, size, &canvas, theme, segRects);
    const float pillRadius = keel::max(2.0f, trackRadius - inset);

    auto isSegmentSelected = [&](keel::usize idx) noexcept -> bool {
        if (multiSelect) {
            return idx < 32 && ((selectedMask & (1u << static_cast<keel::u32>(idx))) != 0u);
        }
        return idx == selectedIndex;
    };

    // 2. Render subtle vertical dividers between adjacent unselected segments
    if (opts.showDividers && items.size() > 1) {
        const Color divCol = pal.borderSubtle.WithAlpha(disabled ? 55 : 120);
        for (keel::usize i = 0; i + 1 < items.size(); ++i) {
            const bool curSel = isSegmentSelected(i);
            const bool nextSel = isSegmentSelected(i + 1);
            const bool curHov = (hoveredSegment == static_cast<keel::i32>(i));
            const bool nextHov = (hoveredSegment == static_cast<keel::i32>(i + 1));
            if (!curSel && !nextSel && !curHov && !nextHov) {
                const Rect& r = segRects[i];
                const float x = std::round(r.Right());
                const float padY = keel::max(4.0f, r.h * 0.22f);
                canvas.DrawLine({x, r.y + padY}, {x, r.Bottom() - padY}, divCol, 1.0f);
            }
        }
    }

    // 3. Render segments (active pill, hover highlight, icon, and text)
    for (keel::usize i = 0; i < items.size(); ++i) {
        const SegmentedItem& item = items[i];
        const Rect sr = segRects[i];
        if (sr.w <= 0.0f || sr.h <= 0.0f) continue;

        const bool segDisabled = disabled || item.disabled;
        const bool selected = isSegmentSelected(i);
        const bool hovered = !segDisabled && (hoveredSegment == static_cast<keel::i32>(i));
        const bool pressed = !segDisabled && (pressedSegment == static_cast<keel::i32>(i));

        Color fgColor = segDisabled ? pal.textMuted : (hovered ? pal.textPrimary : pal.textSecondary);

        if (selected) {
            Color pillFill{};
            Color pillStroke{0, 0, 0, 0};
            float pillStrokeW = 0.0f;

            if (opts.color.HasAny()) {
                pillFill = theme.ResolveColor(opts.color, hovered, pressed, segDisabled, pal.accent);
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

        // Draw icon and/or text inside segment rect (strictly clipped to segment bounds)
        canvas.PushClip(sr);

        const bool hasIcon = item.icon.IsValid();
        const bool hasText = !item.text.empty();
        const float fontSize = theme.ResolveFontSize(item.text, TextRole::LabelMedium, size);
        const float iconSz = keel::min(cm.iconSize, keel::max(8.0f, keel::min(sr.w, sr.h) - 4.0f));

        if (hasIcon && hasText) {
            if (item.iconPlacement == Placement::Top || item.iconPlacement == Placement::Bottom) {
                const float padX = keel::min(theme.Scale(3.0f), sr.w * 0.10f);
                const float availW = keel::max(0.0f, sr.w - padX * 2.0f);
                const float gapY = 2.0f;
                const float totalH = iconSz + gapY + fontSize;
                if (availW < fontSize * 1.15f || sr.h < iconSz + fontSize * 0.85f) {
                    const Rect iconRect(sr.x + (sr.w - iconSz) * 0.5f, sr.y + (sr.h - iconSz) * 0.5f, iconSz, iconSz);
                    item.icon.Draw(canvas, iconRect, fgColor);
                } else {
                    const float startY = sr.y + (sr.h - totalH) * 0.5f;
                    if (item.iconPlacement == Placement::Top) {
                        const Rect iconRect(sr.x + (sr.w - iconSz) * 0.5f, startY, iconSz, iconSz);
                        item.icon.Draw(canvas, iconRect, fgColor);
                        const Rect textRect(sr.x + padX, startY + iconSz + gapY, availW, fontSize + 2.0f);
                        canvas.DrawTextBox(item.text.text, textRect, fontSize, fgColor, 0.5f, 0.5f);
                    } else {
                        const Rect textRect(sr.x + padX, startY, availW, fontSize + 2.0f);
                        canvas.DrawTextBox(item.text.text, textRect, fontSize, fgColor, 0.5f, 0.5f);
                        const Rect iconRect(sr.x + (sr.w - iconSz) * 0.5f, startY + fontSize + gapY, iconSz, iconSz);
                        item.icon.Draw(canvas, iconRect, fgColor);
                    }
                }
            } else {
                const float padX = keel::min(theme.Scale(4.0f), sr.w * 0.12f);
                const float gapX = keel::min(theme.Scale(5.0f), keel::max(2.0f, sr.w * 0.08f));
                const float availW = keel::max(0.0f, sr.w - padX * 2.0f);
                const float availTextW = availW - iconSz - gapX;

                // When segment is too narrow to show meaningful text beside the icon, auto-collapse to centered icon
                if (availTextW < fontSize * 1.45f) {
                    const Rect iconRect(sr.x + (sr.w - iconSz) * 0.5f, sr.y + (sr.h - iconSz) * 0.5f, iconSz, iconSz);
                    item.icon.Draw(canvas, iconRect, fgColor);
                } else {
                    keel::FixedString<256> truncatedLabel;
                    keel::StringRef displayLabel = item.text.text;
                    float textW = canvas.MeasureText(displayLabel, fontSize).x;
                    if (textW > availTextW + kTextSubpixelTolerance) {
                        truncatedLabel = TextUtil::TruncateWithEllipsis(item.text.text, availTextW, fontSize, &canvas);
                        displayLabel = truncatedLabel;
                        textW = keel::min(availTextW, canvas.MeasureText(displayLabel, fontSize).x);
                    }
                    const float totalW = iconSz + gapX + textW;
                    const float startX = sr.x + keel::max(padX, (sr.w - totalW) * 0.5f);
                    const float iconY = sr.y + (sr.h - iconSz) * 0.5f;
                    if (item.iconPlacement == Placement::Trailing) {
                        const Rect textRect(startX, sr.y, availTextW, sr.h);
                        canvas.DrawTextBox(displayLabel, textRect, fontSize, fgColor, 0.0f, 0.5f);
                        const Rect iconRect(startX + textW + gapX, iconY, iconSz, iconSz);
                        item.icon.Draw(canvas, iconRect, fgColor);
                    } else {
                        const Rect iconRect(startX, iconY, iconSz, iconSz);
                        item.icon.Draw(canvas, iconRect, fgColor);
                        const Rect textRect(startX + iconSz + gapX, sr.y, availTextW, sr.h);
                        canvas.DrawTextBox(displayLabel, textRect, fontSize, fgColor, 0.0f, 0.5f);
                    }
                }
            }
        } else if (hasIcon) {
            const Rect iconRect(sr.x + (sr.w - iconSz) * 0.5f, sr.y + (sr.h - iconSz) * 0.5f, iconSz, iconSz);
            item.icon.Draw(canvas, iconRect, fgColor);
        } else if (hasText) {
            const float padX = keel::min(theme.Scale(4.0f), sr.w * 0.12f);
            canvas.DrawTextBox(item.text.text, sr.Inset(padX, 0.0f), fontSize, fgColor, 0.5f, 0.5f);
        }

        canvas.PopClip();
    }
}

} // namespace guikit
