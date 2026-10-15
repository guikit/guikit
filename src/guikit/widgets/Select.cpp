#include "guikit/widgets/Select.hpp"
#include <algorithm>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Element.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

Vec2 MeasureSelectBox(const SelectOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const SelectMetrics& m = theme.GetSelect(size);
    float boxW = (opts.stretch != StretchMode::Fill && opts.width.has_value()) ? theme.Scale(*opts.width) : m.defaultWidth;
    if (opts.minWidth.has_value()) {
        boxW = std::max(theme.Scale(*opts.minWidth), boxW);
    } else {
        boxW = std::max(m.minWidth, boxW);
    }
    return {boxW, m.defaultHeight};
}

ElementSpec MakeSelectElementSpec(
    const SelectOptions& opts,
    UiSize size,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const SelectMetrics& m = theme.GetSelect(size);
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .icon = opts.icon,
        .badge = opts.badge,
        .variant = opts.variant,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = opts.width,
        .widgetSize = size,
        .labelGap = m.labelGap,
        .descGap = m.descGap,
        .inlineGap = m.paddingX,
        .divider = opts.divider,
        .dividerStyle = opts.dividerStyle,
        .stretchableControl = true,
        .stretchHeightWhenStandalone = false,
        .disabled = disabled
    };
}

} // namespace

Rect Select::GetBoxRect(
    Rect bounds,
    const SelectOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const Vec2 boxDims = MeasureSelectBox(opts, size, theme);
    const ElementSpec spec = MakeSelectElementSpec(opts, size, opts.disabled, theme);
    return Element::Resolve(bounds, boxDims, spec, canvas, theme).controlRect;
}

Vec2 Select::Measure(
    const SelectOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) {
    const Vec2 boxDims = MeasureSelectBox(opts, size, theme);
    const ElementSpec spec = MakeSelectElementSpec(opts, size, opts.disabled, theme);
    return Element::Measure(boxDims, spec, canvas, theme, availableWidth);
}

Vec2 Select::Measure(
    const SelectOptions& opts,
    UiSize size,
    ICanvas* canvas,
    float availableWidth
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme, availableWidth);
}

Response Select::Run(
    Context& ctx,
    size_t* selectedIndex,
    int* selectedIntIndex,
    std::string* selectedString,
    std::string_view* selectedStringView,
    std::span<const std::string_view> items,
    const SelectOptions& opts,
    std::source_location loc
) {
    std::vector<SelectItem> richItems;
    richItems.reserve(items.size());
    for (std::string_view sv : items) {
        richItems.push_back(SelectItem{.text = sv});
    }
    return Run(ctx, selectedIndex, selectedIntIndex, selectedString, selectedStringView, richItems, opts, loc);
}

Response Select::Run(
    Context& ctx,
    size_t* selectedIndex,
    int* selectedIntIndex,
    std::string* selectedString,
    std::string_view* selectedStringView,
    std::span<const SelectItem> items,
    const SelectOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();

    const bool wasOpen = ctx.IsPopupOpen(id);

    ctx.RegisterFocusable(id, effectiveDisabled);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    } else {
        if (res.clicked) {
            ctx.RequestFocus(id);
        }
        if (!wasOpen && ctx.IsFocused(id) && (ctx.ConsumeKey(Key::Space) || ctx.ConsumeKey(Key::Enter) || ctx.ConsumeKey(Key::Down))) {
            res.clicked = true;
        }
    }

    // Determine currently selected index from pointers
    size_t currIndex = size_t(-1);
    if (selectedIndex != nullptr) {
        if (*selectedIndex < items.size()) currIndex = *selectedIndex;
    } else if (selectedIntIndex != nullptr) {
        if (*selectedIntIndex >= 0 && static_cast<size_t>(*selectedIntIndex) < items.size()) {
            currIndex = static_cast<size_t>(*selectedIntIndex);
        }
    } else if (selectedString != nullptr) {
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i].text.text == *selectedString) {
                currIndex = i;
                break;
            }
        }
    } else if (selectedStringView != nullptr) {
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i].text.text == *selectedStringView) {
                currIndex = i;
                break;
            }
        }
    }

    const UiTheme theme = ctx.GetTheme();
    const SelectMetrics& m = theme.GetSelect(activeSize);
    const ScrollbarMetrics& sm = theme.GetScrollbar(activeSize);
    ICanvas* canvas = ctx.GetCanvas();

    auto measureRowContentW = [&](const UiText& mainText, const UiAffix& pfx, const UiAffix& sfx) -> float {
        const float mainFont = theme.ResolveFontSize(mainText, TextRole::LabelMedium, activeSize);
        float w = TextUtil::MeasureUiTextWidth(mainText, mainFont, canvas);
        if (pfx.HasContentOrReserve()) {
            const float pFont = theme.ResolveFontSize(pfx.text, TextRole::LabelMedium, activeSize);
            w += TextUtil::MeasureAffixWidth(pfx, pFont, m.iconSize, 4.0f, canvas) + 6.0f;
        }
        if (sfx.HasContentOrReserve()) {
            const float sFont = theme.ResolveFontSize(sfx.text, TextRole::LabelMedium, activeSize);
            w += TextUtil::MeasureAffixWidth(sfx, sFont, m.iconSize, 4.0f, canvas) + 6.0f;
        }
        return w;
    };

    float maxRowW = 0.0f;
    for (const SelectItem& it : items) {
        const UiAffix& effPfx = it.prefix.HasContentOrReserve() ? it.prefix : opts.prefix;
        const UiAffix& effSfx = it.suffix.HasContentOrReserve() ? it.suffix : opts.suffix;
        maxRowW = std::max(maxRowW, measureRowContentW(it.text, effPfx, effSfx));
        // Also ensure popup row width (with item's own prefix & suffix) fits
        maxRowW = std::max(maxRowW, measureRowContentW(it.text, it.prefix, it.suffix));
    }
    if (opts.placeholder.HasContentOrReserve()) {
        maxRowW = std::max(maxRowW, measureRowContentW(opts.placeholder, opts.prefix, opts.suffix));
    }

    const bool exceedsVisibleItems = opts.maxVisibleItems.has_value()
        && *opts.maxVisibleItems > 0
        && items.size() > static_cast<size_t>(*opts.maxVisibleItems);

    float intrinsicW = maxRowW + m.paddingX * 2.0f + m.iconSize + 8.0f + (exceedsVisibleItems ? sm.gutter : 0.0f);
    if (opts.minWidth.has_value()) {
        intrinsicW = std::max(theme.Scale(*opts.minWidth), intrinsicW);
    } else {
        intrinsicW = std::max(m.minWidth, intrinsicW);
    }
    if (opts.stretch != StretchMode::Fill && opts.width.has_value()) {
        intrinsicW = theme.Scale(*opts.width);
    }

    const float unscaledIntrinsicW = (theme.scale > 0.0f) ? (intrinsicW / theme.scale) : intrinsicW;
    SelectOptions runOpts = opts;
    if (opts.stretch != StretchMode::Fill && !runOpts.width.has_value()) {
        runOpts.width = unscaledIntrinsicW;
    }

    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    const Rect boxRect = GetBoxRect(prevBounds, runOpts, activeSize, nullptr, theme);

    struct SelectPopupState {
        float scrollY{0.0f};
        bool draggingScrollbar{false};
        float dragStartMouseY{0.0f};
        float dragStartScrollY{0.0f};
        bool scrollbarHovered{false};
        bool scrollbarActive{false};
    };
    auto& popupState = ctx.GetOrCreateState<SelectPopupState>(id);

    const float totalContentH = static_cast<float>(items.size()) * m.itemHeight;
    auto scrollItemIntoView = [&](size_t idx, float viewportH, float maxScrollY) {
        if (idx >= items.size() || maxScrollY <= 0.0f || viewportH <= 0.0f) {
            return;
        }
        const float itemTop = static_cast<float>(idx) * m.itemHeight;
        const float itemBottom = itemTop + m.itemHeight;
        if (itemTop < popupState.scrollY) {
            popupState.scrollY = itemTop;
        } else if (itemBottom > popupState.scrollY + viewportH) {
            popupState.scrollY = itemBottom - viewportH;
        }
        popupState.scrollY = std::clamp(popupState.scrollY, 0.0f, maxScrollY);
    };

    bool isOpen = wasOpen;

    // Handle clicking the select box to toggle popup open/close
    if (res.clicked && !effectiveDisabled) {
        if (wasOpen) {
            ctx.ClosePopup();
            isOpen = false;
            popupState.draggingScrollbar = false;
            popupState.scrollbarHovered = false;
            popupState.scrollbarActive = false;
        } else if (!items.empty()) {
            const float popupW = boxRect.w > 0.0f ? boxRect.w : intrinsicW;
            const size_t visibleCount = (opts.maxVisibleItems.has_value() && *opts.maxVisibleItems > 0)
                ? std::min(items.size(), static_cast<size_t>(*opts.maxVisibleItems))
                : items.size();
            const float contentH = static_cast<float>(visibleCount) * m.itemHeight + 8.0f;
            float popupH = contentH;
            float popupX = boxRect.x;
            float popupY = boxRect.y + boxRect.h + 4.0f;

            // Viewport boundary check: flip upward if overflowing bottom
            const Vec2 vp = ctx.GetViewportSize();
            if (vp.y > 0.0f) {
                const float availDown = vp.y - popupY - 8.0f;
                const float availUp = boxRect.y - 4.0f - 8.0f;
                if (popupY + popupH > vp.y && availUp > availDown) {
                    popupY = std::max(8.0f, boxRect.y - 4.0f - popupH);
                    popupH = std::min(popupH, boxRect.y - 4.0f - popupY);
                } else if (popupY + popupH > vp.y) {
                    popupH = std::max(m.itemHeight + 8.0f, availDown);
                }
            }
            if (vp.x > 0.0f) {
                popupX = std::clamp(popupX, 4.0f, std::max(4.0f, vp.x - popupW - 4.0f));
            }

            const Rect popupRect(popupX, popupY, popupW, popupH);
            ctx.OpenPopup(id, popupRect);
            isOpen = true;

            const float viewportH = std::max(0.0f, popupRect.h - 8.0f);
            const float maxScrollY = std::max(0.0f, totalContentH - viewportH);
            popupState.draggingScrollbar = false;
            popupState.scrollbarHovered = false;
            popupState.scrollbarActive = false;
            popupState.scrollY = std::clamp(popupState.scrollY, 0.0f, maxScrollY);
            if (currIndex < items.size()) {
                scrollItemIntoView(currIndex, viewportH, maxScrollY);
            } else {
                popupState.scrollY = 0.0f;
            }
        }
    }

    auto applySelection = [&](size_t newIdx) {
        if (currIndex != newIdx && newIdx < items.size()) {
            res.changed = true;
            currIndex = newIdx;
            if (selectedIndex != nullptr) *selectedIndex = newIdx;
            if (selectedIntIndex != nullptr) *selectedIntIndex = static_cast<int>(newIdx);
            if (selectedString != nullptr) *selectedString = std::string(items[newIdx].text.text);
            if (selectedStringView != nullptr) *selectedStringView = items[newIdx].text.text;
        }
    };

    // Process open popup interaction, scrolling, and keyboard navigation
    int hoveredItem = -1;
    if (isOpen && wasOpen && !effectiveDisabled) {
        const Rect popupRect = ctx.GetOpenPopupRect();
        const Vec2 mouse = ctx.GetMousePos();
        const float viewportH = std::max(0.0f, popupRect.h - 8.0f);
        const float maxScrollY = std::max(0.0f, totalContentH - viewportH);
        const bool hasScrollbar = (maxScrollY > 0.5f);

        popupState.scrollY = std::clamp(popupState.scrollY, 0.0f, maxScrollY);

        // 1. Mouse wheel scrolling inside popup
        if (hasScrollbar && popupRect.Contains(mouse)) {
            const Vec2 wheel = ctx.GetPopupMouseWheel(id);
            if (wheel.y != 0.0f || wheel.x != 0.0f) {
                popupState.scrollY = std::clamp(popupState.scrollY - wheel.y * sm.scrollStep, 0.0f, maxScrollY);
                ctx.ConsumeMouseWheel();
            }
        }

        // 2. Scrollbar thumb drag & track page-jump interaction
        popupState.scrollbarHovered = false;
        if (hasScrollbar && popupRect.h > 10.0f) {
            const float trackX = popupRect.Right() - sm.trackWidth - sm.margin;
            const float trackY = popupRect.y + sm.margin;
            const float trackH = std::max(0.0f, popupRect.h - 2.0f * sm.margin);
            const float thumbH = std::clamp((viewportH / std::max(1.0f, totalContentH)) * trackH, sm.minThumbLength, trackH);
            const float travelH = std::max(0.0f, trackH - thumbH);
            const float scrollRatio = (maxScrollY > 0.0f) ? std::clamp(popupState.scrollY / maxScrollY, 0.0f, 1.0f) : 0.0f;
            const float thumbY = trackY + scrollRatio * travelH;

            const Rect trackRect(trackX, trackY, sm.trackWidth, trackH);
            const Rect thumbRect(trackX, thumbY, sm.trackWidth, thumbH);
            const Rect gutterRect(popupRect.Right() - sm.gutter, popupRect.y, sm.gutter, popupRect.h);

            const bool overThumb = thumbRect.Contains(mouse);
            const bool overTrack = trackRect.Contains(mouse) || gutterRect.Contains(mouse);
            popupState.scrollbarHovered = overThumb || overTrack;

            if (ctx.IsMouseClicked() && overTrack) {
                if (overThumb) {
                    popupState.draggingScrollbar = true;
                    popupState.dragStartMouseY = mouse.y;
                    popupState.dragStartScrollY = popupState.scrollY;
                    ctx.SetActiveId(id);
                } else {
                    if (mouse.y < thumbRect.y) {
                        popupState.scrollY = std::max(0.0f, popupState.scrollY - viewportH * 0.8f);
                    } else if (mouse.y > thumbRect.Bottom()) {
                        popupState.scrollY = std::min(maxScrollY, popupState.scrollY + viewportH * 0.8f);
                    }
                }
                ctx.ConsumeMouseClick();
            }

            if (popupState.draggingScrollbar) {
                if (ctx.IsMouseDown()) {
                    if (travelH > 0.0f) {
                        const float deltaY = mouse.y - popupState.dragStartMouseY;
                        popupState.scrollY = std::clamp(
                            popupState.dragStartScrollY + (deltaY / travelH) * maxScrollY,
                            0.0f,
                            maxScrollY
                        );
                    }
                } else {
                    popupState.draggingScrollbar = false;
                    if (ctx.GetActiveId() == id) {
                        ctx.SetActiveId(Id::Null());
                    }
                    ctx.ConsumeMouseClick();
                }
            }
        } else {
            popupState.draggingScrollbar = false;
        }
        popupState.scrollbarActive = popupState.draggingScrollbar;

        // 3. Check hover and click on dropdown items (suppressed when dragging or hovering scrollbar)
        const float itemW = std::max(0.0f, popupRect.w - 8.0f - (hasScrollbar ? sm.gutter : 0.0f));
        const Rect itemsViewport(popupRect.x + 4.0f, popupRect.y + 4.0f, itemW, viewportH);

        if (!popupState.draggingScrollbar && !popupState.scrollbarHovered && itemsViewport.Contains(mouse)) {
            for (size_t i = 0; i < items.size(); ++i) {
                const Rect itemRect(
                    popupRect.x + 4.0f,
                    popupRect.y + 4.0f + static_cast<float>(i) * m.itemHeight - popupState.scrollY,
                    itemW,
                    m.itemHeight
                );
                if (itemRect.Bottom() <= itemsViewport.y || itemRect.y >= itemsViewport.Bottom()) {
                    continue;
                }
                if (itemRect.Contains(mouse)) {
                    if (!items[i].disabled) {
                        hoveredItem = static_cast<int>(i);
                    }
                    if (ctx.IsMouseReleased()) {
                        if (!items[i].disabled) {
                            applySelection(i);
                            ctx.ClosePopup();
                            isOpen = false;
                        }
                        ctx.ConsumeMouseClick();
                        break;
                    }
                }
            }
        }

        // 4. Keyboard navigation when popup is open (skipping disabled items + auto scroll-into-view)
        if (isOpen) {
            if (ctx.ConsumeKey(Key::Escape) || ctx.ConsumeKey(Key::Enter)) {
                ctx.ClosePopup();
                isOpen = false;
                popupState.draggingScrollbar = false;
            } else if (ctx.ConsumeKey(Key::Down)) {
                if (!items.empty()) {
                    const size_t startIdx = (currIndex == size_t(-1)) ? 0 : (currIndex + 1);
                    for (size_t k = startIdx; k < items.size(); ++k) {
                        if (!items[k].disabled) {
                            applySelection(k);
                            scrollItemIntoView(k, viewportH, maxScrollY);
                            break;
                        }
                    }
                }
            } else if (ctx.ConsumeKey(Key::Up)) {
                if (!items.empty() && currIndex != size_t(-1) && currIndex > 0) {
                    for (size_t k = currIndex; k > 0; --k) {
                        const size_t candidate = k - 1;
                        if (!items[candidate].disabled) {
                            applySelection(candidate);
                            scrollItemIntoView(candidate, viewportH, maxScrollY);
                            break;
                        }
                    }
                }
            }
        }
    } else if (!isOpen) {
        popupState.draggingScrollbar = false;
        popupState.scrollbarHovered = false;
        popupState.scrollbarActive = false;
    }

    // Determine current display text and active affixes for closed select box
    UiText currentDisplayText{};
    UiAffix activePrefix = opts.prefix;
    UiAffix activeSuffix = opts.suffix;

    if (currIndex < items.size()) {
        currentDisplayText = items[currIndex].text;
        if (items[currIndex].prefix.HasContentOrReserve()) {
            activePrefix = items[currIndex].prefix;
        }
        if (items[currIndex].suffix.HasContentOrReserve()) {
            activeSuffix = items[currIndex].suffix;
        }
    } else if (selectedString != nullptr && !selectedString->empty()) {
        currentDisplayText = *selectedString;
    } else if (selectedStringView != nullptr && !selectedStringView->empty()) {
        currentDisplayText = *selectedStringView;
    } else {
        currentDisplayText = opts.placeholder;
    }

    // Register overlay render callback if popup is open
    if (isOpen) {
        const Rect currentPopupRect = ctx.GetOpenPopupRect();
        const float popupScrollY = popupState.scrollY;
        const bool sbHovered = popupState.scrollbarHovered;
        const bool sbActive = popupState.scrollbarActive;
        std::vector<SelectItem> itemsVec;
        itemsVec.reserve(items.size());
        for (const SelectItem& it : items) {
            SelectItem copy = it;
            copy.text = ctx.AllocText(it.text);
            copy.prefix = ctx.AllocAffix(it.prefix);
            copy.suffix = ctx.AllocAffix(it.suffix);
            itemsVec.push_back(copy);
        }
        ctx.RegisterOverlay([currentPopupRect, itemsVec = std::move(itemsVec), currIndex, hoveredItem, opts, activeSize, theme, popupScrollY, sbHovered, sbActive](ICanvas& canvas) {
            Select::RenderPopup(canvas, currentPopupRect, std::span<const SelectItem>(itemsVec), currIndex, hoveredItem, opts, activeSize, theme, popupScrollY, sbHovered, sbActive);
        });
    }

    if (res.hovered && !isOpen && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, prevBounds);
    }

    Node* node = ctx.CreateNode(NodeType::Select, id);

    SelectOptions optsCopy = opts;
    optsCopy.label = ctx.AllocText(opts.label);
    optsCopy.badge = ctx.AllocText(opts.badge);
    optsCopy.description = ctx.AllocText(opts.description);
    optsCopy.placeholder = ctx.AllocText(opts.placeholder);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    optsCopy.prefix = ctx.AllocAffix(activePrefix);
    optsCopy.suffix = ctx.AllocAffix(activeSuffix);
    node->text = optsCopy.label.text;
    if (opts.stretch != StretchMode::Fill && !optsCopy.width.has_value()) optsCopy.width = unscaledIntrinsicW;

    auto* data = ctx.AllocPayload<SelectData>();
    data->opts = optsCopy;
    data->displayText = ctx.AllocText(currentDisplayText);
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        opts.variant,
        static_cast<uint8_t>(opts.stretch),
        optsCopy.width.value_or(0.0f),
        opts.minWidth.value_or(0.0f),
        optsCopy.label,
        optsCopy.badge,
        optsCopy.icon,
        data->displayText,
        optsCopy.description,
        optsCopy.placeholder,
        optsCopy.prefix,
        optsCopy.suffix,
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        static_cast<uint8_t>(opts.divider),
        static_cast<uint8_t>(opts.dividerStyle),
        opts.radius.value_or(-1.0f)
    );
    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<SelectData>();
        return Select::Measure(d ? d->opts : SelectOptions{}, n->uiSize, canvas, theme, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<SelectData>();
        Select::Render(
            canvas,
            n->computedRect,
            d ? d->displayText : UiText{},
            d ? d->opts : SelectOptions{},
            n->uiSize,
            n->hovered,
            n->active,
            n->disabled,
            theme,
            n->focused
        );
    };
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->active = isOpen;
    node->focused = isOpen || ctx.IsFocused(id);

    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

void Select::Render(
    ICanvas& canvas,
    Rect bounds,
    UiText currentText,
    const SelectOptions& opts,
    UiSize size,
    bool hovered,
    bool open,
    bool disabled,
    const UiTheme& theme,
    bool focused
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const SelectMetrics& m = theme.GetSelect(size);
    const Vec2 boxDims = MeasureSelectBox(opts, size, theme);
    const ElementSpec spec = MakeSelectElementSpec(opts, size, disabled, theme);
    const ElementGeometry geom = Element::Resolve(bounds, boxDims, spec, &canvas, theme);
    Element::RenderText(canvas, geom, spec, theme);
    const Rect boxRect = geom.controlRect;

    // Determine colors
    const auto& pal = theme.palette;
    Color boxBg = (hovered && !disabled) ? pal.surface : pal.inputBackground;
    Color boxBorder;
    float strokeW = m.strokeWidth;

    const VariantColors& v = theme.GetVariant(opts.variant);
    if ((open || focused) && !disabled) {
        boxBorder = (opts.variant == UiVariant::Default || opts.variant == UiVariant::Ghost) ? pal.focusRing : v.bg;
        strokeW += 0.5f;
    } else if (hovered && !disabled) {
        boxBorder = (opts.variant == UiVariant::Default || opts.variant == UiVariant::Ghost) ? pal.borderHover : v.bgHover;
    } else {
        if (opts.variant == UiVariant::Default) {
            boxBorder = pal.inputBorder;
        } else if (opts.variant == UiVariant::Ghost) {
            boxBorder = pal.border;
        } else {
            boxBorder = (v.border.a > 0) ? v.border : v.bg;
        }
    }

    Color placeholderCol = theme.ResolveTextColor(opts.placeholder, UiVariant::Secondary, disabled);
    Color chevronCol = open ? pal.accent : pal.textSecondary;
    Color unitCol = pal.textSecondary;

    if (disabled) {
        boxBg = boxBg.WithAlphaF(0.40f);
        boxBorder = boxBorder.WithAlphaF(0.40f);
        chevronCol = chevronCol.WithAlphaF(0.40f);
        unitCol = unitCol.WithAlphaF(0.40f);
    }

    // Draw box background and border
    const float radius = opts.radius.has_value() ? theme.Scale(*opts.radius) : m.cornerRadius;
    canvas.DrawRoundRect(boxRect, radius, boxBg, boxBorder, strokeW);

    // Left elements (prefix icon and/or text)
    float leftX = boxRect.x + m.paddingX;
    if (opts.prefix.HasContentOrReserve()) {
        const float pFont = theme.ResolveFontSize(opts.prefix.text, TextRole::LabelMedium, size);
        const Color pTextCol = theme.ResolveTextColor(opts.prefix.text, UiVariant::Secondary, disabled);
        const Color pIconCol = opts.prefix.text.variant.has_value() ? pTextCol : chevronCol;
        const float pW = TextUtil::MeasureAffixWidth(opts.prefix, pFont, m.iconSize, 4.0f, &canvas);
        TextUtil::DrawAffix(canvas, opts.prefix, Rect(leftX, boxRect.y, pW, boxRect.h), pFont, m.iconSize, pTextCol, pIconCol, 4.0f);
        leftX += pW + 6.0f;
    }

    // Right elements (chevron indicator + optional suffix before chevron)
    float rightX = boxRect.x + boxRect.w - m.paddingX - m.iconSize;
    const Rect chevronRect(rightX, boxRect.y + (boxRect.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
    const Icon chevron = open ? Icon::ChevronUp() : Icon::ChevronDown();
    chevron.Draw(canvas, chevronRect, chevronCol);
    rightX -= 6.0f;

    if (opts.suffix.HasContentOrReserve()) {
        const float sFont = theme.ResolveFontSize(opts.suffix.text, TextRole::LabelMedium, size);
        const Color sTextCol = theme.ResolveTextColor(opts.suffix.text, UiVariant::Muted, disabled);
        const Color sIconCol = opts.suffix.text.variant.has_value() ? sTextCol : unitCol;
        const float sW = TextUtil::MeasureAffixWidth(opts.suffix, sFont, m.iconSize, 4.0f, &canvas);
        rightX -= sW;
        TextUtil::DrawAffix(canvas, opts.suffix, Rect(rightX, boxRect.y, sW, boxRect.h), sFont, m.iconSize, sTextCol, sIconCol, 4.0f);
        rightX -= 6.0f;
    }

    // Center display text / placeholder
    const float availW = std::max(0.0f, rightX - leftX);
    const Rect textRect(leftX, boxRect.y, availW, boxRect.h);
    canvas.PushClip(textRect);

    const bool isPlaceholder = currentText.empty() || (!currentText.variant.has_value() && currentText.text == opts.placeholder.text);
    const float selFont = theme.ResolveFontSize(currentText, TextRole::LabelMedium, size);
    const Color mainCol = isPlaceholder
        ? placeholderCol
        : theme.ResolveTextColor(currentText, UiVariant::Default, disabled);
    TextUtil::DrawUiText(canvas, currentText, textRect, selFont, boxRect.h, mainCol, 0.0f, 0.5f);

    canvas.PopClip();
}

void Select::RenderPopup(
    ICanvas& canvas,
    Rect popupRect,
    std::span<const std::string_view> items,
    size_t selectedIndex,
    int hoveredIndex,
    const SelectOptions& opts,
    UiSize size,
    const UiTheme& theme,
    float scrollY,
    bool scrollbarHovered,
    bool scrollbarActive
) {
    std::vector<SelectItem> richItems;
    richItems.reserve(items.size());
    for (std::string_view sv : items) {
        richItems.push_back(SelectItem{.text = sv});
    }
    RenderPopup(canvas, popupRect, std::span<const SelectItem>(richItems), selectedIndex, hoveredIndex, opts, size, theme, scrollY, scrollbarHovered, scrollbarActive);
}

void Select::RenderPopup(
    ICanvas& canvas,
    Rect popupRect,
    std::span<const SelectItem> items,
    size_t selectedIndex,
    int hoveredIndex,
    const SelectOptions& /*opts*/,
    UiSize size,
    const UiTheme& theme,
    float scrollY,
    bool scrollbarHovered,
    bool scrollbarActive
) {
    if (popupRect.w <= 0.0f || popupRect.h <= 0.0f || items.empty()) {
        return;
    }

    const SelectMetrics& m = theme.GetSelect(size);
    const ScrollbarMetrics& sm = theme.GetScrollbar(size);
    const auto& pal = theme.palette;

    // Dropdown container styling: card with border and subtle shadow
    const Color popupBg = pal.surfaceElevated;
    const Color popupBorder = pal.border;
    const Color shadowCol = Color(0, 0, 0, 50);

    // Outer subtle drop shadow
    canvas.DrawRoundRect(Rect(popupRect.x, popupRect.y + 2.0f, popupRect.w, popupRect.h), m.cornerRadius + 1.0f, shadowCol);

    // Popup background and border
    canvas.DrawRoundRect(popupRect, m.cornerRadius, popupBg, popupBorder, 1.0f);

    const float viewportH = std::max(0.0f, popupRect.h - 8.0f);
    const float totalContentH = static_cast<float>(items.size()) * m.itemHeight;
    const float maxScrollY = std::max(0.0f, totalContentH - viewportH);
    const bool hasScrollbar = (maxScrollY > 0.5f);
    const float clampedScrollY = std::clamp(scrollY, 0.0f, maxScrollY);
    const float itemW = std::max(0.0f, popupRect.w - 8.0f - (hasScrollbar ? sm.gutter : 0.0f));
    const Rect itemsClipRect(popupRect.x + 1.0f, popupRect.y + 4.0f, std::max(0.0f, popupRect.w - 2.0f), viewportH);

    canvas.PushClip(itemsClipRect);

    for (size_t i = 0; i < items.size(); ++i) {
        const SelectItem& item = items[i];
        const Rect itemRect(
            popupRect.x + 4.0f,
            popupRect.y + 4.0f + static_cast<float>(i) * m.itemHeight - clampedScrollY,
            itemW,
            m.itemHeight
        );

        // Cull items completely outside the visible scroll viewport
        if (itemRect.Bottom() <= itemsClipRect.y || itemRect.y >= itemsClipRect.Bottom()) {
            continue;
        }

        const bool itemDisabled = item.disabled;
        const bool isSelected = (i == selectedIndex);
        const bool isHovered = (static_cast<int>(i) == hoveredIndex) && !itemDisabled;

        if (isSelected) {
            const Color itemBg = isHovered ? pal.surfaceActive : pal.surfaceHover;
            canvas.DrawRoundRect(itemRect, 4.0f, itemBg);
        } else if (isHovered) {
            canvas.DrawRoundRect(itemRect, 4.0f, pal.surfaceHover);
        }

        // Colors for item text and checkmark
        Color itemTextCol = item.text.variant.has_value()
            ? theme.ResolveTextColor(item.text, UiVariant::Default, itemDisabled)
            : (isSelected ? (itemDisabled ? pal.accent.WithAlphaF(0.40f) : pal.accent)
                          : (itemDisabled ? pal.textPrimary.WithAlphaF(0.40f) : pal.textPrimary));

        // Right side: checkmark (if selected) + item suffix
        float textRightX = itemRect.x + itemRect.w - m.paddingX;
        if (isSelected) {
            const float checkX = textRightX - m.iconSize;
            const float checkY = itemRect.y + (itemRect.h - m.iconSize) * 0.5f;
            const Rect checkRect(checkX, checkY, m.iconSize, m.iconSize);
            Icon::Check().Draw(canvas, checkRect, itemTextCol);
            textRightX = checkX - 6.0f;
        }

        if (item.suffix.HasContentOrReserve()) {
            const float sFont = theme.ResolveFontSize(item.suffix.text, TextRole::LabelMedium, size);
            const Color sTextCol = theme.ResolveTextColor(item.suffix.text, UiVariant::Muted, itemDisabled);
            const float sW = TextUtil::MeasureAffixWidth(item.suffix, sFont, m.iconSize, 4.0f, &canvas);
            textRightX -= sW;
            TextUtil::DrawAffix(canvas, item.suffix, Rect(textRightX, itemRect.y, sW, itemRect.h), sFont, m.iconSize, sTextCol, sTextCol, 4.0f);
            textRightX -= 6.0f;
        }

        // Left side: item prefix + main text
        float textLeftX = itemRect.x + m.paddingX;
        if (item.prefix.HasContentOrReserve()) {
            const float pFont = theme.ResolveFontSize(item.prefix.text, TextRole::LabelMedium, size);
            const Color pTextCol = theme.ResolveTextColor(item.prefix.text, UiVariant::Secondary, itemDisabled);
            const float pW = TextUtil::MeasureAffixWidth(item.prefix, pFont, m.iconSize, 4.0f, &canvas);
            TextUtil::DrawAffix(canvas, item.prefix, Rect(textLeftX, itemRect.y, pW, itemRect.h), pFont, m.iconSize, pTextCol, pTextCol, 4.0f);
            textLeftX += pW + 6.0f;
        }

        const float selFont = theme.ResolveFontSize(item.text, TextRole::LabelMedium, size);
        const Rect itemTextRect(textLeftX, itemRect.y, std::max(0.0f, textRightX - textLeftX), itemRect.h);
        TextUtil::DrawUiText(canvas, item.text, itemTextRect, selFont, itemRect.h, itemTextCol, 0.0f, 0.5f);
    }

    canvas.PopClip();

    // Render vertical scrollbar when items exceed visible popup height
    if (hasScrollbar && popupRect.h > 10.0f) {
        canvas.PushClip(popupRect);
        const float trackX = popupRect.Right() - sm.trackWidth - sm.margin;
        const float trackY = popupRect.y + sm.margin;
        const float trackH = std::max(0.0f, popupRect.h - 2.0f * sm.margin);
        const float thumbH = std::clamp((viewportH / std::max(1.0f, totalContentH)) * trackH, sm.minThumbLength, trackH);
        const float travelH = std::max(0.0f, trackH - thumbH);
        const float scrollRatio = (maxScrollY > 0.0f) ? (clampedScrollY / maxScrollY) : 0.0f;
        const float thumbY = trackY + scrollRatio * travelH;

        const Rect trackRect(trackX, trackY, sm.trackWidth, trackH);
        const Rect thumbRect(trackX, thumbY, sm.trackWidth, thumbH);

        canvas.DrawRoundRect(trackRect, sm.cornerRadius, pal.borderSubtle);

        Color thumbCol = pal.textMuted;
        if (scrollbarActive) {
            thumbCol = pal.accent;
        } else if (scrollbarHovered) {
            thumbCol = pal.textSecondary;
        }
        canvas.DrawRoundRect(thumbRect, sm.cornerRadius, thumbCol);
        canvas.PopClip();
    }
}

} // namespace guikit
