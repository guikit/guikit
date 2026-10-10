#include "guikit/widgets/InputField.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/layout/Node.hpp"

namespace guikit {

Response InputField::Run(
    Context& ctx,
    std::string_view label,
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
    const std::string_view effectiveLabel = !label.empty() ? label : opts.label;
    node->text = ctx.InternString(effectiveLabel);
    node->displayText = ctx.InternString(displayText);

    InputFieldOptions optsCopy = opts;
    optsCopy.label = node->text;
    if (!opts.description.empty()) optsCopy.description = ctx.InternString(opts.description);
    if (!opts.placeholder.empty()) optsCopy.placeholder = ctx.InternString(opts.placeholder);
    if (!opts.prefix.empty()) optsCopy.prefix = ctx.InternString(opts.prefix);
    if (!opts.suffix.empty()) optsCopy.suffix = ctx.InternString(opts.suffix);

    node->inputOpts = optsCopy;
    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->focused = isFocused;
    node->cursor = cursor;
    node->cursorVisible = isFocused && ctx.IsCursorVisible(opts.cursorMode);
    node->cursorWidth = ctx.GetCursorConfig().width;

    node->widthMode = SizeMode::Stretch;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 InputField::Measure(
    std::string_view label,
    std::string_view displayText,
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    const InputFieldMetrics m = GetInputFieldMetrics(size);
    const std::string_view effectiveLabel = !label.empty() ? label : opts.label;

    float boxW = 120.0f;
    switch (size) {
        case UiSize::None:
        case UiSize::Md: boxW = 120.0f; break;
        case UiSize::Xs: boxW = 80.0f; break;
        case UiSize::Sm: boxW = 100.0f; break;
        case UiSize::Lg: boxW = 160.0f; break;
        case UiSize::Xl: boxW = 200.0f; break;
    }

    Vec2 labelDims = {0.0f, 0.0f};
    if (!effectiveLabel.empty()) {
        if (canvas) {
            labelDims = canvas->MeasureText(effectiveLabel, m.labelFontSize);
        } else {
            labelDims = {static_cast<float>(effectiveLabel.length()) * (m.labelFontSize * 0.58f), m.labelFontSize * 1.25f};
        }
    }

    Vec2 descDims = {0.0f, 0.0f};
    if (!opts.description.empty()) {
        if (canvas) {
            descDims = canvas->MeasureText(opts.description, m.descFontSize);
        } else {
            descDims = {static_cast<float>(opts.description.length()) * (m.descFontSize * 0.58f), m.descFontSize * 1.25f};
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

void InputField::Render(
    ICanvas& canvas,
    Rect bounds,
    std::string_view label,
    std::string_view displayText,
    const InputFieldOptions& opts,
    UiSize size,
    bool hovered,
    bool focused,
    size_t cursor,
    bool disabled,
    bool cursorVisible,
    float cursorWidth,
    ThemeMode theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
        return;
    }

    const InputFieldMetrics m = GetInputFieldMetrics(size);
    const std::string_view effectiveLabel = !label.empty() ? label : opts.label;
    const bool hasLabel = !effectiveLabel.empty();
    const bool hasDesc = !opts.description.empty();

    Rect boxRect;
    Rect labelRect;
    Rect descRect;

    const float labelH = hasLabel ? (m.labelFontSize * 1.25f) : 0.0f;
    const float descH = hasDesc ? (m.descFontSize * 1.25f) : 0.0f;

    if (opts.labelPlacement == Placement::Stacked) {
        float curY = bounds.y;
        if (hasLabel) {
            labelRect = Rect(bounds.x, curY, bounds.w, labelH);
            curY += labelH + m.labelGap;
        }
        boxRect = Rect(bounds.x, curY, bounds.w, m.defaultHeight);
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
        const float boxX = bounds.x + labelW + (hasLabel ? m.paddingX : 0.0f);
        const float boxW = std::max(0.0f, bounds.w - labelW - (hasLabel ? m.paddingX : 0.0f));
        boxRect = Rect(boxX, bounds.y + (bounds.h - m.defaultHeight) * 0.5f, boxW, m.defaultHeight);
    } else if (opts.labelPlacement == Placement::Trailing) {
        float labelW = 0.0f;
        if (hasLabel) {
            labelW = canvas.MeasureText(effectiveLabel, m.labelFontSize).x;
        }
        const float boxW = std::max(0.0f, bounds.w - (hasLabel ? (labelW + m.paddingX) : 0.0f));
        boxRect = Rect(bounds.x, bounds.y + (bounds.h - m.defaultHeight) * 0.5f, boxW, m.defaultHeight);
        if (hasLabel) {
            labelRect = Rect(bounds.x + boxW + m.paddingX, bounds.y + (bounds.h - labelH) * 0.5f, bounds.w - boxW - m.paddingX, labelH);
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
        boxRect = Rect(bounds.x, curY, bounds.w, m.defaultHeight);
    } else { // Placement::Bottom
        float curY = bounds.y;
        boxRect = Rect(bounds.x, curY, bounds.w, m.defaultHeight);
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
    const auto pal = GetThemePalette(theme);
    Color boxBg = hovered && !disabled
        ? (theme == ThemeMode::Dark ? Color::Hex(0x1E293BFF) : Color::Hex(0xFFFFFFFF))
        : (theme == ThemeMode::Dark ? Color::Hex(0x0F172AFF) : Color::Hex(0xF8FAFCFF));
    Color boxBorder;
    float strokeW = m.strokeWidth;

    if (focused && !disabled) {
        boxBorder = (opts.variant == UiVariant::Default) ? pal.accent : GetVariantColors(opts.variant, theme).bg;
        strokeW += 0.5f;
    } else if (hovered && !disabled) {
        boxBorder = theme == ThemeMode::Dark ? Color::Hex(0x64748BFF) : Color::Hex(0x94A3B8FF);
    } else {
        boxBorder = (opts.variant == UiVariant::Default) ? pal.inputBorder : GetVariantColors(opts.variant, theme).border;
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
    canvas.DrawRoundRect(boxRect, m.cornerRadius, boxBg, boxBorder, strokeW);

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
        canvas.DrawLine({caretX, caretY}, {caretX, caretY + caretH}, Color::Hex(0x38BDF8FF), cursorWidth);
    }
    canvas.PopClip();

    // Draw external label & description
    if (hasLabel && labelRect.w > 0.0f && labelRect.h > 0.0f) {
        canvas.PushClip(labelRect);
        canvas.DrawTextBox(effectiveLabel, labelRect, m.labelFontSize, labelCol, 0.0f, 0.5f);
        canvas.PopClip();
    }
    if (hasDesc && descRect.w > 0.0f && descRect.h > 0.0f) {
        canvas.PushClip(descRect);
        canvas.DrawTextBox(opts.description, descRect, m.descFontSize, descCol, 0.0f, 0.5f);
        canvas.PopClip();
    }
}

} // namespace guikit
