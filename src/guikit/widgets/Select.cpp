#include "guikit/widgets/Select.hpp"
#include <algorithm>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

Rect Select::GetBoxRect(
    Rect bounds,
    const SelectOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const SelectMetrics& m = theme.GetSelect(size);
    const std::string_view effectiveLabel = opts.label;
    const bool hasLabel = !effectiveLabel.empty();
    const bool hasDesc = !opts.description.empty();

    const float labelH = hasLabel ? (canvas ? canvas->GetLineHeight(m.labelFontSize) : theme.GetLineHeight(m.labelFontSize)) : 0.0f;
    const float descH = hasDesc ? (canvas ? canvas->GetLineHeight(m.descFontSize) : theme.GetLineHeight(m.descFontSize)) : 0.0f;

    float naturalBoxW = opts.width.value_or(m.defaultWidth);
    if (opts.minWidth.has_value()) {
        naturalBoxW = std::max(*opts.minWidth, naturalBoxW);
    } else {
        naturalBoxW = std::max(m.minWidth, naturalBoxW);
    }

    if (opts.labelPlacement == Placement::Stacked) {
        float curY = bounds.y;
        if (hasLabel) {
            curY += labelH + m.labelGap;
        }
        const float boxW = (opts.stretch == StretchMode::Fill && !opts.width.has_value()) ? bounds.w : std::min(bounds.w, naturalBoxW);
        const float boxX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - boxW) : bounds.x;
        return Rect(boxX, curY, boxW, m.defaultHeight);
    }
    
    if (opts.labelPlacement == Placement::Leading) {
        float labelW = 0.0f;
        if (hasLabel) {
            if (canvas) {
                labelW = canvas->MeasureText(effectiveLabel, m.labelFontSize).x;
            } else {
                labelW = static_cast<float>(effectiveLabel.length()) * (m.labelFontSize * kDefaultCharAdvanceRatio);
            }
        }
        if (opts.stretch == StretchMode::Between) {
            const float boxW = std::min(naturalBoxW, std::max(0.0f, bounds.w - labelW - (hasLabel ? m.paddingX : 0.0f)));
            const float boxX = bounds.x + bounds.w - boxW;
            return Rect(boxX, bounds.y + (bounds.h - m.defaultHeight) * 0.5f, boxW, m.defaultHeight);
        } else if (opts.stretch == StretchMode::Fill) {
            const float boxX = bounds.x + labelW + (hasLabel ? m.paddingX : 0.0f);
            const float boxW = opts.width.has_value() ? std::min(bounds.w - (labelW + (hasLabel ? m.paddingX : 0.0f)), naturalBoxW) : std::max(0.0f, bounds.w - labelW - (hasLabel ? m.paddingX : 0.0f));
            return Rect(boxX, bounds.y + (bounds.h - m.defaultHeight) * 0.5f, boxW, m.defaultHeight);
        } else { // Compact
            const float boxX = bounds.x + labelW + (hasLabel ? m.paddingX : 0.0f);
            const float boxW = std::min(naturalBoxW, std::max(0.0f, bounds.w - labelW - (hasLabel ? m.paddingX : 0.0f)));
            return Rect(boxX, bounds.y + (bounds.h - m.defaultHeight) * 0.5f, boxW, m.defaultHeight);
        }
    }
    
    if (opts.labelPlacement == Placement::Trailing) {
        float labelW = 0.0f;
        if (hasLabel) {
            if (canvas) {
                labelW = canvas->MeasureText(effectiveLabel, m.labelFontSize).x;
            } else {
                labelW = static_cast<float>(effectiveLabel.length()) * (m.labelFontSize * kDefaultCharAdvanceRatio);
            }
        }
        const float boxW = (opts.stretch == StretchMode::Fill && !opts.width.has_value())
            ? std::max(0.0f, bounds.w - (hasLabel ? (labelW + m.paddingX) : 0.0f))
            : std::min(naturalBoxW, std::max(0.0f, bounds.w - (hasLabel ? (labelW + m.paddingX) : 0.0f)));
        return Rect(bounds.x, bounds.y + (bounds.h - m.defaultHeight) * 0.5f, boxW, m.defaultHeight);
    }
    
    if (opts.labelPlacement == Placement::Top) {
        float curY = bounds.y;
        if (hasLabel) curY += labelH + (hasDesc ? m.descGap : 0.0f);
        if (hasDesc) curY += descH;
        curY += m.labelGap;
        const float boxW = (opts.stretch == StretchMode::Fill && !opts.width.has_value()) ? bounds.w : std::min(bounds.w, naturalBoxW);
        const float boxX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - boxW) : bounds.x;
        return Rect(boxX, curY, boxW, m.defaultHeight);
    }
    
    // Placement::Bottom
    const float boxW = (opts.stretch == StretchMode::Fill && !opts.width.has_value()) ? bounds.w : std::min(bounds.w, naturalBoxW);
    const float boxX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - boxW) : bounds.x;
    return Rect(boxX, bounds.y, boxW, m.defaultHeight);
}

Vec2 Select::Measure(
    const SelectOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const SelectMetrics& m = theme.GetSelect(size);
    const std::string_view effectiveLabel = opts.label;

    float boxW = opts.width.value_or(m.defaultWidth);
    if (opts.minWidth.has_value()) {
        boxW = std::max(*opts.minWidth, boxW);
    } else {
        boxW = std::max(m.minWidth, boxW);
    }

    Vec2 labelDims = {0.0f, 0.0f};
    if (!effectiveLabel.empty()) {
        if (canvas) {
            labelDims = canvas->MeasureText(effectiveLabel, m.labelFontSize);
        } else {
            labelDims = {static_cast<float>(effectiveLabel.length()) * (m.labelFontSize * kDefaultCharAdvanceRatio), theme.GetLineHeight(m.labelFontSize)};
        }
    }

    Vec2 descDims = {0.0f, 0.0f};
    if (!opts.description.empty()) {
        if (canvas) {
            descDims = canvas->MeasureText(opts.description, m.descFontSize);
        } else {
            descDims = {static_cast<float>(opts.description.length()) * (m.descFontSize * kDefaultCharAdvanceRatio), theme.GetLineHeight(m.descFontSize)};
        }
    }

    const bool hasLabel = !effectiveLabel.empty();
    const bool hasDesc = !opts.description.empty();

    if (opts.labelPlacement == Placement::Stacked) {
        float totalH = m.defaultHeight;
        if (hasLabel) totalH += labelDims.y + m.labelGap;
        if (hasDesc) totalH += m.descGap + descDims.y;
        return {std::max({boxW, labelDims.x, descDims.x}), totalH};
    }

    if (opts.labelPlacement == Placement::Leading || opts.labelPlacement == Placement::Trailing) {
        const float labelW = hasLabel ? labelDims.x + m.paddingX : 0.0f;
        return {boxW + labelW, std::max(m.defaultHeight, labelDims.y)};
    }

    if (opts.labelPlacement == Placement::Top) {
        float textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc) textH += (hasLabel ? m.descGap : 0.0f) + descDims.y;
        return {std::max({boxW, labelDims.x, descDims.x}), textH + m.labelGap + m.defaultHeight};
    }

    if (opts.labelPlacement == Placement::Bottom) {
        float textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc) textH += (hasLabel ? m.descGap : 0.0f) + descDims.y;
        return {std::max({boxW, labelDims.x, descDims.x}), m.defaultHeight + m.descGap + textH};
    }

    return {boxW, m.defaultHeight};
}

Vec2 Select::Measure(
    const SelectOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(opts, size, canvas, defaultTheme);
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
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();

    const bool wasOpen = ctx.IsPopupOpen(id);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
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
            if (items[i] == *selectedString) {
                currIndex = i;
                break;
            }
        }
    } else if (selectedStringView != nullptr) {
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i] == *selectedStringView) {
                currIndex = i;
                break;
            }
        }
    }

    // Determine current display text for closed select box
    std::string_view currentDisplayText;
    if (currIndex < items.size()) {
        currentDisplayText = items[currIndex];
    } else if (selectedString != nullptr && !selectedString->empty()) {
        currentDisplayText = *selectedString;
    } else if (selectedStringView != nullptr && !selectedStringView->empty()) {
        currentDisplayText = *selectedStringView;
    } else {
        currentDisplayText = opts.placeholder;
    }

    const UiTheme theme = ctx.GetTheme();
    const SelectMetrics& m = theme.GetSelect(activeSize);

    float maxItemTextW = 0.0f;
    for (std::string_view it : items) {
        const float itemW = static_cast<float>(it.length()) * (m.fontSize * kDefaultCharAdvanceRatio);
        maxItemTextW = std::max(maxItemTextW, itemW);
    }
    if (!opts.placeholder.empty()) {
        const float phW = static_cast<float>(opts.placeholder.length()) * (m.fontSize * kDefaultCharAdvanceRatio);
        maxItemTextW = std::max(maxItemTextW, phW);
    }
    float intrinsicW = maxItemTextW + m.paddingX * 2.0f + m.iconSize + 8.0f;
    if (opts.prefixIcon.IsValid()) {
        intrinsicW += m.iconSize + 6.0f;
    }
    if (opts.minWidth.has_value()) {
        intrinsicW = std::max(*opts.minWidth, intrinsicW);
    } else {
        intrinsicW = std::max(m.minWidth, intrinsicW);
    }
    if (opts.width.has_value()) {
        intrinsicW = *opts.width;
    }

    SelectOptions runOpts = opts;
    if (opts.stretch != StretchMode::Fill && !runOpts.width.has_value()) {
        runOpts.width = intrinsicW;
    }

    const Rect prevBounds = ctx.GetPrevFrameBounds(id);
    const Rect boxRect = GetBoxRect(prevBounds, runOpts, activeSize, nullptr, theme);

    bool isOpen = wasOpen;

    // Handle clicking the select box to toggle popup open/close
    if (res.clicked && !effectiveDisabled) {
        if (wasOpen) {
            ctx.ClosePopup();
            isOpen = false;
        } else if (!items.empty()) {
            const float popupW = boxRect.w > 0.0f ? boxRect.w : intrinsicW;
            const size_t visibleCount = opts.maxVisibleItems.has_value()
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
        }
    }

    // Process open popup interaction and keyboard navigation
    int hoveredItem = -1;
    if (isOpen && wasOpen && !effectiveDisabled) {
        const Rect popupRect = ctx.GetOpenPopupRect();
        const Vec2 mouse = ctx.GetMousePos();

        // Check hover and click on dropdown items
        for (size_t i = 0; i < items.size(); ++i) {
            const Rect itemRect(
                popupRect.x + 4.0f,
                popupRect.y + 4.0f + static_cast<float>(i) * m.itemHeight,
                std::max(0.0f, popupRect.w - 8.0f),
                m.itemHeight
            );
            if (itemRect.Contains(mouse)) {
                hoveredItem = static_cast<int>(i);
                if (ctx.IsMouseReleased()) {
                    if (currIndex != i) {
                        res.changed = true;
                        currIndex = i;
                        if (selectedIndex != nullptr) *selectedIndex = i;
                        if (selectedIntIndex != nullptr) *selectedIntIndex = static_cast<int>(i);
                        if (selectedString != nullptr) *selectedString = std::string(items[i]);
                        if (selectedStringView != nullptr) *selectedStringView = items[i];
                        currentDisplayText = items[i];
                    }
                    ctx.ClosePopup();
                    ctx.ConsumeMouseClick();
                    isOpen = false;
                    break;
                }
            }
        }

        // Keyboard navigation when popup is open
        if (isOpen) {
            if (ctx.ConsumeKey(Key::Escape)) {
                ctx.ClosePopup();
                isOpen = false;
            } else if (ctx.ConsumeKey(Key::Enter)) {
                ctx.ClosePopup();
                isOpen = false;
            } else if (ctx.ConsumeKey(Key::Down)) {
                if (!items.empty()) {
                    const size_t nextIdx = (currIndex == size_t(-1)) ? 0 : std::min(currIndex + 1, items.size() - 1);
                    if (currIndex != nextIdx) {
                        currIndex = nextIdx;
                        res.changed = true;
                        if (selectedIndex != nullptr) *selectedIndex = currIndex;
                        if (selectedIntIndex != nullptr) *selectedIntIndex = static_cast<int>(currIndex);
                        if (selectedString != nullptr) *selectedString = std::string(items[currIndex]);
                        if (selectedStringView != nullptr) *selectedStringView = items[currIndex];
                        currentDisplayText = items[currIndex];
                    }
                }
            } else if (ctx.ConsumeKey(Key::Up)) {
                if (!items.empty() && currIndex > 0 && currIndex != size_t(-1)) {
                    currIndex--;
                    res.changed = true;
                    if (selectedIndex != nullptr) *selectedIndex = currIndex;
                    if (selectedIntIndex != nullptr) *selectedIntIndex = static_cast<int>(currIndex);
                    if (selectedString != nullptr) *selectedString = std::string(items[currIndex]);
                    if (selectedStringView != nullptr) *selectedStringView = items[currIndex];
                    currentDisplayText = items[currIndex];
                }
            }
        }
    }

    // Register overlay render callback if popup is open
    if (isOpen) {
        const Rect currentPopupRect = ctx.GetOpenPopupRect();
        std::vector<std::string_view> itemsVec(items.begin(), items.end());
        ctx.RegisterOverlay([currentPopupRect, itemsVec = std::move(itemsVec), currIndex, hoveredItem, opts, activeSize, theme](ICanvas& canvas) {
            Select::RenderPopup(canvas, currentPopupRect, itemsVec, currIndex, hoveredItem, opts, activeSize, theme);
        });
    }

    Node* node = ctx.CreateNode(NodeType::Select, id);
    node->text = ctx.AllocString(opts.label);

    SelectOptions optsCopy = opts;
    optsCopy.label = node->text;
    if (opts.stretch != StretchMode::Fill && !optsCopy.width.has_value()) optsCopy.width = intrinsicW;
    if (!opts.description.empty()) optsCopy.description = ctx.AllocString(opts.description);
    if (!opts.placeholder.empty()) optsCopy.placeholder = ctx.AllocString(opts.placeholder);

    auto* data = ctx.AllocPayload<SelectData>();
    data->opts = optsCopy;
    data->displayText = ctx.AllocString(currentDisplayText);
    node->payload = data;
    node->visualHash = detail::HashArgs(
        opts.variant,
        static_cast<uint8_t>(opts.stretch),
        optsCopy.width.value_or(0.0f),
        opts.minWidth.value_or(0.0f),
        node->text,
        data->displayText,
        optsCopy.description,
        optsCopy.placeholder,
        opts.labelPlacement,
        opts.radius.value_or(-1.0f),
        static_cast<uint8_t>(opts.prefixIcon.GetType()),
        static_cast<uint8_t>(opts.prefixIcon.GetId())
    );
    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<SelectData>();
        return Select::Measure(d ? d->opts : SelectOptions{}, n->uiSize, canvas, theme);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<SelectData>();
        Select::Render(
            canvas,
            n->computedRect,
            d ? d->displayText : std::string_view{},
            d ? d->opts : SelectOptions{},
            n->uiSize,
            n->hovered,
            n->focused,
            n->disabled,
            theme
        );
    };
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->focused = isOpen;

    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

void Select::Render(
    ICanvas& canvas,
    Rect bounds,
    std::string_view currentText,
    const SelectOptions& opts,
    UiSize size,
    bool hovered,
    bool open,
    bool disabled,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const SelectMetrics& m = theme.GetSelect(size);
    const std::string_view effectiveLabel = opts.label;
    const bool hasLabel = !effectiveLabel.empty();
    const bool hasDesc = !opts.description.empty();

    Rect boxRect;
    Rect labelRect;
    Rect descRect;

    const float labelH = hasLabel ? canvas.GetLineHeight(m.labelFontSize) : 0.0f;
    const float descH = hasDesc ? canvas.GetLineHeight(m.descFontSize) : 0.0f;

    float naturalBoxW = opts.width.value_or(m.defaultWidth);
    if (opts.minWidth.has_value()) {
        naturalBoxW = std::max(*opts.minWidth, naturalBoxW);
    } else {
        naturalBoxW = std::max(m.minWidth, naturalBoxW);
    }

    if (opts.labelPlacement == Placement::Stacked) {
        float curY = bounds.y;
        if (hasLabel) {
            labelRect = Rect(bounds.x, curY, bounds.w, labelH);
            curY += labelH + m.labelGap;
        }
        const float boxW = (opts.stretch == StretchMode::Fill && !opts.width.has_value()) ? bounds.w : std::min(bounds.w, naturalBoxW);
        boxRect = Rect(bounds.x, curY, boxW, m.defaultHeight);
        curY += m.defaultHeight + m.descGap;
        if (hasDesc) {
            descRect = Rect(bounds.x, curY, bounds.w, descH);
        }
    } else if (opts.labelPlacement == Placement::Leading) {
        float labelW = 0.0f;
        if (hasLabel) {
            labelW = canvas.MeasureText(effectiveLabel, m.labelFontSize).x;
            labelRect = Rect(bounds.x, bounds.y + (bounds.h - labelH) * 0.5f, labelW, labelH);
        }
        if (opts.stretch == StretchMode::Between) {
            const float boxW = std::min(naturalBoxW, std::max(0.0f, bounds.w - labelW - (hasLabel ? m.paddingX : 0.0f)));
            const float boxX = bounds.x + bounds.w - boxW;
            boxRect = Rect(boxX, bounds.y + (bounds.h - m.defaultHeight) * 0.5f, boxW, m.defaultHeight);
        } else if (opts.stretch == StretchMode::Fill) {
            const float boxX = bounds.x + labelW + (hasLabel ? m.paddingX : 0.0f);
            const float boxW = opts.width.has_value() ? std::min(bounds.w - (labelW + (hasLabel ? m.paddingX : 0.0f)), naturalBoxW) : std::max(0.0f, bounds.w - labelW - (hasLabel ? m.paddingX : 0.0f));
            boxRect = Rect(boxX, bounds.y + (bounds.h - m.defaultHeight) * 0.5f, boxW, m.defaultHeight);
        } else { // Compact
            const float boxX = bounds.x + labelW + (hasLabel ? m.paddingX : 0.0f);
            const float boxW = std::min(naturalBoxW, std::max(0.0f, bounds.w - labelW - (hasLabel ? m.paddingX : 0.0f)));
            boxRect = Rect(boxX, bounds.y + (bounds.h - m.defaultHeight) * 0.5f, boxW, m.defaultHeight);
        }
    } else if (opts.labelPlacement == Placement::Trailing) {
        float labelW = 0.0f;
        if (hasLabel) {
            labelW = canvas.MeasureText(effectiveLabel, m.labelFontSize).x;
        }
        const float boxW = (opts.stretch == StretchMode::Fill && !opts.width.has_value())
            ? std::max(0.0f, bounds.w - (hasLabel ? (labelW + m.paddingX) : 0.0f))
            : std::min(naturalBoxW, std::max(0.0f, bounds.w - (hasLabel ? (labelW + m.paddingX) : 0.0f)));
        boxRect = Rect(bounds.x, bounds.y + (bounds.h - m.defaultHeight) * 0.5f, boxW, m.defaultHeight);
        if (hasLabel) {
            float labelX = bounds.x + boxW + m.paddingX;
            if (opts.stretch == StretchMode::Between || (opts.stretch == StretchMode::Fill && opts.width.has_value())) {
                labelX = std::max(labelX, bounds.x + bounds.w - labelW);
            }
            labelRect = Rect(labelX, bounds.y + (bounds.h - labelH) * 0.5f, labelW, labelH);
        }
    } else if (opts.labelPlacement == Placement::Top) {
        float curY = bounds.y;
        if (hasLabel) {
            labelRect = Rect(bounds.x, curY, bounds.w, labelH);
            curY += labelH + (hasDesc ? m.descGap : 0.0f);
        }
        if (hasDesc) {
            descRect = Rect(bounds.x, curY, bounds.w, descH);
            curY += descH;
        }
        curY += m.labelGap;
        const float boxW = (opts.stretch == StretchMode::Fill && !opts.width.has_value()) ? bounds.w : std::min(bounds.w, naturalBoxW);
        boxRect = Rect(bounds.x, curY, boxW, m.defaultHeight);
    } else { // Placement::Bottom
        float curY = bounds.y;
        const float boxW = (opts.stretch == StretchMode::Fill && !opts.width.has_value()) ? bounds.w : std::min(bounds.w, naturalBoxW);
        boxRect = Rect(bounds.x, curY, boxW, m.defaultHeight);
        curY += m.defaultHeight + m.descGap;
        if (hasLabel) {
            labelRect = Rect(bounds.x, curY, bounds.w, labelH);
            curY += labelH + (hasDesc ? m.descGap : 0.0f);
        }
        if (hasDesc) {
            descRect = Rect(bounds.x, curY, bounds.w, descH);
        }
    }

    // Determine colors
    const auto& pal = theme.palette;
    Color boxBg = (hovered && !disabled) ? pal.surface : pal.inputBackground;
    Color boxBorder;
    float strokeW = m.strokeWidth;

    const VariantColors& v = theme.GetVariant(opts.variant);
    if (open && !disabled) {
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

    Color textCol = pal.textPrimary;
    Color placeholderCol = pal.textSecondary;
    Color chevronCol = open ? pal.accent : pal.textSecondary;
    Color labelCol = pal.textPrimary;
    Color descCol = pal.textSecondary;

    if (disabled) {
        boxBg = boxBg.WithAlphaF(0.40f);
        boxBorder = boxBorder.WithAlphaF(0.40f);
        textCol = textCol.WithAlphaF(0.40f);
        placeholderCol = placeholderCol.WithAlphaF(0.40f);
        chevronCol = chevronCol.WithAlphaF(0.40f);
        labelCol = labelCol.WithAlphaF(0.40f);
        descCol = descCol.WithAlphaF(0.40f);
    }

    // Draw box background and border
    const float radius = opts.radius.value_or(m.cornerRadius);
    canvas.DrawRoundRect(boxRect, radius, boxBg, boxBorder, strokeW);

    // Left elements (prefix icon)
    float leftX = boxRect.x + m.paddingX;
    if (opts.prefixIcon.IsValid()) {
        const Rect iconRect(leftX, boxRect.y + (boxRect.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
        opts.prefixIcon.Draw(canvas, iconRect, chevronCol);
        leftX += m.iconSize + 6.0f;
    }

    // Right elements (chevron indicator)
    const float rightX = boxRect.x + boxRect.w - m.paddingX - m.iconSize;
    const Rect chevronRect(rightX, boxRect.y + (boxRect.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
    const Icon chevron = open ? Icon::ChevronUp() : Icon::ChevronDown();
    chevron.Draw(canvas, chevronRect, chevronCol);

    // Center display text / placeholder
    const float availW = std::max(0.0f, rightX - 6.0f - leftX);
    const Rect textRect(leftX, boxRect.y, availW, boxRect.h);
    canvas.PushClip(textRect);

    const bool isPlaceholder = currentText.empty() || currentText == opts.placeholder;
    canvas.DrawTextBox(currentText, textRect, m.fontSize, isPlaceholder ? placeholderCol : textCol, 0.0f, 0.5f);

    canvas.PopClip();

    // Draw external label & description
    if (hasLabel && labelRect.w > 0.0f && labelRect.h > 0.0f) {
        canvas.DrawTextBox(effectiveLabel, labelRect, m.labelFontSize, labelCol, 0.0f, 0.5f);
    }
    if (hasDesc && descRect.w > 0.0f && descRect.h > 0.0f) {
        canvas.DrawTextBox(opts.description, descRect, m.descFontSize, descCol, 0.0f, 0.5f);
    }
}

void Select::RenderPopup(
    ICanvas& canvas,
    Rect popupRect,
    std::span<const std::string_view> items,
    size_t selectedIndex,
    int hoveredIndex,
    const SelectOptions& opts,
    UiSize size,
    const UiTheme& theme
) {
    if (popupRect.w <= 0.0f || popupRect.h <= 0.0f || items.empty()) {
        return;
    }

    const SelectMetrics& m = theme.GetSelect(size);
    const auto& pal = theme.palette;

    // Dropdown container styling: card with border and subtle shadow
    const Color popupBg = pal.surfaceElevated;
    const Color popupBorder = pal.border;
    const Color shadowCol = Color(0, 0, 0, 50);

    // Outer subtle drop shadow
    canvas.DrawRoundRect(Rect(popupRect.x, popupRect.y + 2.0f, popupRect.w, popupRect.h), m.cornerRadius + 1.0f, shadowCol);

    // Popup background and border
    canvas.DrawRoundRect(popupRect, m.cornerRadius, popupBg, popupBorder, 1.0f);

    canvas.PushClip(popupRect);

    for (size_t i = 0; i < items.size(); ++i) {
        const Rect itemRect(
            popupRect.x + 4.0f,
            popupRect.y + 4.0f + static_cast<float>(i) * m.itemHeight,
            std::max(0.0f, popupRect.w - 8.0f),
            m.itemHeight
        );

        const bool isSelected = (i == selectedIndex);
        const bool isHovered = (static_cast<int>(i) == hoveredIndex);

        if (isSelected) {
            const Color itemBg = isHovered ? pal.surfaceActive : pal.surfaceHover;
            canvas.DrawRoundRect(itemRect, 4.0f, itemBg);
        } else if (isHovered) {
            canvas.DrawRoundRect(itemRect, 4.0f, pal.surfaceHover);
        }

        // Colors for item text and checkmark
        Color itemTextCol = pal.textPrimary;
        if (isSelected) {
            itemTextCol = pal.accent;
        } else if (isHovered) {
            itemTextCol = pal.textPrimary;
        }

        // Draw checkmark on right side if selected
        float textRightX = itemRect.x + itemRect.w - m.paddingX;
        if (isSelected) {
            const float checkX = itemRect.x + itemRect.w - m.paddingX - m.iconSize;
            const float checkY = itemRect.y + (itemRect.h - m.iconSize) * 0.5f;
            const Rect checkRect(checkX, checkY, m.iconSize, m.iconSize);
            Icon::Check().Draw(canvas, checkRect, itemTextCol);
            textRightX = checkX - 4.0f;
        }

        const float textLeftX = itemRect.x + m.paddingX;
        const Rect itemTextRect(textLeftX, itemRect.y, std::max(0.0f, textRightX - textLeftX), itemRect.h);
        canvas.DrawTextBox(items[i], itemTextRect, m.fontSize, itemTextCol, 0.0f, 0.5f);
    }

    canvas.PopClip();
}

} // namespace guikit
