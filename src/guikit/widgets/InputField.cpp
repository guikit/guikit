#include "guikit/widgets/InputField.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

Response InputField::Run(
    Context& ctx,
    std::string* stringValue,
    float* floatValue,
    double* doubleValue,
    int* intValue,
    std::string_view displayFallback,
    const InputFieldOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    }

    if (res.clicked && !effectiveDisabled && !opts.readOnly) {
        ctx.RequestFocus(id);
    }

    const bool isFocused = ctx.IsFocused(id) && !effectiveDisabled;
    res.focused = isFocused;

    // Convert underlying data to formatted text
    char valBuf[64];
    std::string_view valStr;

    if (stringValue != nullptr) {
        valStr = *stringValue;
    } else if (floatValue != nullptr) {
        const char* fmt = opts.format.empty() ? "%.2f" : opts.format.data();
        std::snprintf(valBuf, sizeof(valBuf), fmt, *floatValue);
        valStr = valBuf;
    } else if (doubleValue != nullptr) {
        const char* fmt = opts.format.empty() ? "%.2f" : opts.format.data();
        std::snprintf(valBuf, sizeof(valBuf), fmt, *doubleValue);
        valStr = valBuf;
    } else if (intValue != nullptr) {
        std::snprintf(valBuf, sizeof(valBuf), "%d", *intValue);
        valStr = valBuf;
    } else {
        valStr = displayFallback;
    }

    // Active edit state integration with Context
    size_t cursor = 0;
    std::string_view displayText = valStr;

    if ((isFocused || ctx.HasActiveTextEdit(id)) && !opts.readOnly) {
        bool changed = false;
        bool submitted = false;
        displayText = ctx.GetOrStartTextEdit(id, valStr, cursor, changed, submitted);

        if (changed) {
            if (stringValue != nullptr) {
                *stringValue = displayText;
            } else if (floatValue != nullptr) {
                char* end = nullptr;
                std::string s(displayText);
                const float f = std::strtof(s.c_str(), &end);
                if (end != s.c_str()) {
                    *floatValue = f;
                }
            } else if (doubleValue != nullptr) {
                char* end = nullptr;
                std::string s(displayText);
                const double d = std::strtod(s.c_str(), &end);
                if (end != s.c_str()) {
                    *doubleValue = d;
                }
            } else if (intValue != nullptr) {
                char* end = nullptr;
                std::string s(displayText);
                const long i = std::strtol(s.c_str(), &end, 10);
                if (end != s.c_str()) {
                    *intValue = static_cast<int>(i);
                }
            }
            res.changed = true;
        }

        if (submitted) {
            res.submitted = true;
        }
    }

    Node* node = ctx.CreateNode(NodeType::InputField, id);
    node->text = ctx.AllocString(opts.label);

    InputFieldOptions optsCopy = opts;
    optsCopy.label = node->text;
    if (!opts.description.empty()) optsCopy.description = ctx.AllocString(opts.description);
    if (!opts.placeholder.empty()) optsCopy.placeholder = ctx.AllocString(opts.placeholder);
    if (!opts.prefix.empty()) optsCopy.prefix = ctx.AllocString(opts.prefix);
    if (!opts.suffix.empty()) optsCopy.suffix = ctx.AllocString(opts.suffix);

    auto* data = ctx.AllocPayload<InputFieldData>();
    data->opts = optsCopy;
    data->displayText = ctx.AllocString(displayText);
    data->cursor = cursor;
    data->cursorVisible = isFocused && ctx.IsCursorVisible(opts.cursorMode);
    data->cursorWidth = ctx.GetCursorConfig().width;
    node->payload = data;
    node->visualHash = detail::HashArgs(
        opts.variant,
        static_cast<uint8_t>(opts.stretch),
        opts.width.value_or(0.0f),
        opts.minWidth.value_or(0.0f),
        opts.readOnly,
        opts.radius.value_or(-1.0f),
        data->displayText,
        optsCopy.prefix,
        optsCopy.suffix,
        optsCopy.placeholder,
        node->text,
        static_cast<uint8_t>(opts.prefixIcon.GetType()),
        static_cast<uint8_t>(opts.prefixIcon.GetId()),
        static_cast<uint8_t>(opts.suffixIcon.GetType()),
        static_cast<uint8_t>(opts.suffixIcon.GetId())
    );
    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<InputFieldData>();
        return InputField::Measure(
            d ? d->displayText : std::string_view{},
            d ? d->opts : InputFieldOptions{},
            n->uiSize,
            canvas,
            theme
        );
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<InputFieldData>();
        InputField::Render(
            canvas,
            n->computedRect,
            d ? d->displayText : std::string_view{},
            d ? d->opts : InputFieldOptions{},
            n->uiSize,
            n->hovered,
            n->focused,
            d ? d->cursor : 0,
            n->disabled,
            d ? d->cursorVisible : false,
            d ? d->cursorWidth : 1.5f,
            theme
        );
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->focused = isFocused;

    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 InputField::Measure(
    std::string_view displayText,
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const InputFieldMetrics& m = theme.GetInputField(size);
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

    if (opts.layout == ElementLayout::Stacked) {
        float totalH = m.defaultHeight;
        if (hasLabel) totalH += labelDims.y + m.labelGap;
        if (hasDesc) totalH += m.descGap + descDims.y;
        return {std::max({boxW, labelDims.x, descDims.x}), totalH};
    }

    if (opts.layout == ElementLayout::Inline) {
        const float textW = std::max(labelDims.x, descDims.x);
        float textH = 0.0f;
        if (hasLabel && hasDesc) {
            textH = labelDims.y + m.descGap + descDims.y;
        } else if (hasLabel) {
            textH = labelDims.y;
        } else if (hasDesc) {
            textH = descDims.y;
        }
        const float gap = (hasLabel || hasDesc) ? m.paddingX : 0.0f;
        return {boxW + (textW > 0.0f ? gap + textW : 0.0f), std::max(m.defaultHeight, textH)};
    }

    if (opts.layout == ElementLayout::Top) {
        float textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc) textH += (hasLabel ? m.descGap : 0.0f) + descDims.y;
        return {std::max({boxW, labelDims.x, descDims.x}), textH + m.labelGap + m.defaultHeight};
    }

    if (opts.layout == ElementLayout::Bottom) {
        float textH = 0.0f;
        if (hasLabel) textH += labelDims.y;
        if (hasDesc) textH += (hasLabel ? m.descGap : 0.0f) + descDims.y;
        return {std::max({boxW, labelDims.x, descDims.x}), m.defaultHeight + m.descGap + textH};
    }

    return {boxW, m.defaultHeight};
}

Vec2 InputField::Measure(
    std::string_view displayText,
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(displayText, opts, size, canvas, defaultTheme);
}

void InputField::Render(
    ICanvas& canvas,
    Rect bounds,
    std::string_view displayText,
    const InputFieldOptions& opts,
    UiSize size,
    bool hovered,
    bool focused,
    size_t cursor,
    bool disabled,
    bool cursorVisible,
    float cursorWidth,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const InputFieldMetrics& m = theme.GetInputField(size);
    const std::string_view effectiveLabel = opts.label;
    const bool hasLabel = !effectiveLabel.empty();
    const bool hasDesc = !opts.description.empty();

    Rect boxRect;
    Rect labelRect;
    Rect descRect;

    const float labelH = hasLabel ? canvas.GetLineHeight(m.labelFontSize) : 0.0f;
    const float descH = hasDesc ? canvas.GetLineHeight(m.descFontSize) : 0.0f;
    const float measuredLabelW = hasLabel ? canvas.MeasureText(effectiveLabel, m.labelFontSize).x : 0.0f;
    const float measuredDescW = hasDesc ? canvas.MeasureText(opts.description, m.descFontSize).x : 0.0f;
    const float measuredTextW = std::max(measuredLabelW, measuredDescW);

    float naturalBoxW = opts.width.value_or(m.defaultWidth);
    if (opts.minWidth.has_value()) {
        naturalBoxW = std::max(*opts.minWidth, naturalBoxW);
    } else {
        naturalBoxW = std::max(m.minWidth, naturalBoxW);
    }

    if (opts.layout == ElementLayout::Stacked) {
        const float boxW = (opts.stretch == StretchMode::Fill) ? bounds.w : std::min(bounds.w, naturalBoxW);
        const float blockW = (opts.stretch == StretchMode::Fill) ? bounds.w : std::min(bounds.w, std::max(boxW, measuredTextW));
        const float blockX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - blockW) : bounds.x;
        const float boxX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - boxW) : bounds.x;

        float curY = bounds.y;
        if (hasLabel) {
            labelRect = Rect(blockX, curY, blockW, labelH);
            curY += labelH + m.labelGap;
        }
        boxRect = Rect(boxX, curY, boxW, m.defaultHeight);
        curY += m.defaultHeight + m.descGap;
        if (hasDesc) {
            descRect = Rect(blockX, curY, blockW, descH);
        }
    } else if (opts.layout == ElementLayout::Top) {
        const float boxW = (opts.stretch == StretchMode::Fill) ? bounds.w : std::min(bounds.w, naturalBoxW);
        const float blockW = (opts.stretch == StretchMode::Fill) ? bounds.w : std::min(bounds.w, std::max(boxW, measuredTextW));
        const float blockX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - blockW) : bounds.x;
        const float boxX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - boxW) : bounds.x;

        float curY = bounds.y;
        if (hasLabel) {
            labelRect = Rect(blockX, curY, blockW, labelH);
            curY += labelH + (hasDesc ? m.descGap : 0.0f);
        }
        if (hasDesc) {
            descRect = Rect(blockX, curY, blockW, descH);
            curY += descH;
        }
        curY += m.labelGap;
        boxRect = Rect(boxX, curY, boxW, m.defaultHeight);
    } else if (opts.layout == ElementLayout::Bottom) {
        const float boxW = (opts.stretch == StretchMode::Fill) ? bounds.w : std::min(bounds.w, naturalBoxW);
        const float blockW = (opts.stretch == StretchMode::Fill) ? bounds.w : std::min(bounds.w, std::max(boxW, measuredTextW));
        const float blockX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - blockW) : bounds.x;
        const float boxX = (opts.stretch == StretchMode::Between) ? (bounds.x + bounds.w - boxW) : bounds.x;

        float curY = bounds.y;
        boxRect = Rect(boxX, curY, boxW, m.defaultHeight);
        curY += m.defaultHeight + m.descGap;
        if (hasLabel) {
            labelRect = Rect(blockX, curY, blockW, labelH);
            curY += labelH + (hasDesc ? m.descGap : 0.0f);
        }
        if (hasDesc) {
            descRect = Rect(blockX, curY, blockW, descH);
        }
    } else { // ElementLayout::Inline
        const float gap = (hasLabel || hasDesc) ? m.paddingX : 0.0f;
        const float totalTextH = (hasLabel && hasDesc) ? (labelH + m.descGap + descH) : (hasLabel ? labelH : descH);
        const float startY = bounds.y + (bounds.h - totalTextH) * 0.5f;

        float boxW = naturalBoxW;
        if (opts.stretch == StretchMode::Fill) {
            boxW = std::max(m.minWidth, bounds.w - measuredTextW - gap);
        } else {
            boxW = std::min(bounds.w, naturalBoxW);
        }

        if (opts.inlineOrder == InlineOrder::LabelFirst) {
            // Label first (left), box second (right)
            float boxX = bounds.x + bounds.w - boxW;
            if (opts.stretch == StretchMode::Compact) {
                boxX = bounds.x + measuredTextW + gap;
            }
            boxRect = Rect(boxX, bounds.y + (bounds.h - m.defaultHeight) * 0.5f, boxW, m.defaultHeight);

            float textY = startY;
            if (hasLabel) {
                labelRect = Rect(bounds.x, textY, measuredTextW, labelH);
                textY += labelH + m.descGap;
            }
            if (hasDesc) {
                descRect = Rect(bounds.x, textY, measuredTextW, descH);
            }
        } else {
            // InlineOrder::ControlFirst: Box first (left), text second (right)
            boxRect = Rect(bounds.x, bounds.y + (bounds.h - m.defaultHeight) * 0.5f, boxW, m.defaultHeight);

            float textX = bounds.x + boxW + gap;
            if (opts.stretch == StretchMode::Between) {
                textX = std::max(bounds.x + boxW + gap, bounds.x + bounds.w - measuredTextW);
            }
            float textY = startY;
            if (hasLabel) {
                labelRect = Rect(textX, textY, measuredTextW, labelH);
                textY += labelH + m.descGap;
            }
            if (hasDesc) {
                descRect = Rect(textX, textY, measuredTextW, descH);
            }
        }
    }

    // Determine colors
    const auto& pal = theme.palette;
    Color boxBg = (hovered && !disabled) ? pal.surface : pal.inputBackground;
    Color boxBorder;
    float strokeW = m.strokeWidth;

    const VariantColors& v = theme.GetVariant(opts.variant);
    if (focused && !disabled) {
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
    Color unitCol = pal.textSecondary;
    Color labelCol = pal.textPrimary;
    Color descCol = pal.textSecondary;

    if (disabled) {
        boxBg = boxBg.WithAlphaF(0.40f);
        boxBorder = boxBorder.WithAlphaF(0.40f);
        textCol = textCol.WithAlphaF(0.40f);
        placeholderCol = placeholderCol.WithAlphaF(0.40f);
        unitCol = unitCol.WithAlphaF(0.40f);
        labelCol = labelCol.WithAlphaF(0.40f);
        descCol = descCol.WithAlphaF(0.40f);
    }

    // Draw box background and border
    const float radius = opts.radius.value_or(m.cornerRadius);
    canvas.DrawRoundRect(boxRect, radius, boxBg, boxBorder, strokeW);

    // Left elements (prefix icon, prefix text)
    float leftX = boxRect.x + m.paddingX;
    if (opts.prefixIcon.IsValid()) {
        const Rect iconRect(leftX, boxRect.y + (boxRect.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
        opts.prefixIcon.Draw(canvas, iconRect, unitCol);
        leftX += m.iconSize + 6.0f;
    }
    if (!opts.prefix.empty()) {
        const Vec2 pSize = canvas.MeasureText(opts.prefix, m.fontSize);
        const Rect pRect(leftX, boxRect.y, pSize.x, boxRect.h);
        canvas.DrawTextBox(opts.prefix, pRect, m.fontSize, unitCol, 0.0f, 0.5f);
        leftX += pSize.x + 4.0f;
    }

    // Right elements (suffix icon, suffix/unit text)
    float rightX = boxRect.x + boxRect.w - m.paddingX;
    if (opts.suffixIcon.IsValid()) {
        rightX -= m.iconSize;
        const Rect iconRect(rightX, boxRect.y + (boxRect.h - m.iconSize) * 0.5f, m.iconSize, m.iconSize);
        opts.suffixIcon.Draw(canvas, iconRect, unitCol);
        rightX -= 6.0f;
    }
    if (!opts.suffix.empty()) {
        const Vec2 sSize = canvas.MeasureText(opts.suffix, m.fontSize);
        rightX -= sSize.x;
        const Rect sRect(rightX, boxRect.y, sSize.x, boxRect.h);
        canvas.DrawTextBox(opts.suffix, sRect, m.fontSize, unitCol, 0.0f, 0.5f);
        rightX -= 6.0f;
    }

    // Text / placeholder / cursor in middle
    const float availW = std::max(0.0f, rightX - leftX);
    const Rect textRect(leftX, boxRect.y, availW, boxRect.h);
    canvas.PushClip(textRect);

    if (displayText.empty() && !opts.placeholder.empty() && !focused) {
        canvas.DrawTextBox(opts.placeholder, textRect, m.fontSize, placeholderCol, 0.0f, 0.5f);
    } else if (!displayText.empty()) {
        canvas.DrawTextBox(displayText, textRect, m.fontSize, textCol, 0.0f, 0.5f);
    }

    if (focused && !disabled && cursorVisible) {
        const size_t safeCursor = std::min(cursor, displayText.size());
        const std::string_view sub = displayText.substr(0, safeCursor);
        const float subW = canvas.MeasureText(sub, m.fontSize).x;
        const float caretX = textRect.x + subW;
        const float caretH = m.fontSize * 1.15f;
        const float caretY = boxRect.y + (boxRect.h - caretH) * 0.5f;
        canvas.DrawLine({caretX, caretY}, {caretX, caretY + caretH}, pal.caret, cursorWidth);
    }
    canvas.PopClip();

    // Draw external label & description with labelAlign
    const float alignFactor = (opts.labelAlign == Align::End) ? 1.0f : ((opts.labelAlign == Align::Center) ? 0.5f : 0.0f);
    if (hasLabel && labelRect.w > 0.0f && labelRect.h > 0.0f) {
        canvas.DrawTextBox(effectiveLabel, labelRect, m.labelFontSize, labelCol, alignFactor, 0.5f);
    }
    if (hasDesc && descRect.w > 0.0f && descRect.h > 0.0f) {
        canvas.DrawTextBox(opts.description, descRect, m.descFontSize, descCol, alignFactor, 0.5f);
    }
}

} // namespace guikit
