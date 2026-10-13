#include "guikit/widgets/InputField.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/ElementComposer.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

namespace {

Vec2 MeasureInputBox(const InputFieldOptions& opts, UiSize size, const UiTheme& theme) noexcept {
    const InputFieldMetrics& m = theme.GetInputField(size);
    float boxW = (opts.stretch != StretchMode::Fill && opts.width.has_value()) ? *opts.width : m.defaultWidth;
    if (opts.minWidth.has_value()) {
        boxW = std::max(*opts.minWidth, boxW);
    } else {
        boxW = std::max(m.minWidth, boxW);
    }
    return {boxW, m.defaultHeight};
}

ElementSpec MakeInputFieldElementSpec(
    const InputFieldOptions& opts,
    UiSize size,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const InputFieldMetrics& m = theme.GetInputField(size);
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = opts.width,
        .descriptionVariant = opts.descriptionVariant,
        .labelFontSize = m.labelFontSize,
        .descFontSize = m.descFontSize,
        .labelGap = m.labelGap,
        .descGap = m.descGap,
        .inlineGap = m.paddingX,
        .stretchableControl = true,
        .stretchHeightWhenStandalone = false,
        .disabled = disabled
    };
}

} // namespace

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
        opts.descriptionVariant,
        static_cast<uint8_t>(opts.layout),
        static_cast<uint8_t>(opts.inlineOrder),
        static_cast<uint8_t>(opts.elementAlign),
        static_cast<uint8_t>(opts.stretch),
        opts.width.value_or(0.0f),
        opts.minWidth.value_or(0.0f),
        opts.readOnly,
        opts.radius.value_or(-1.0f),
        data->displayText,
        optsCopy.prefix,
        optsCopy.suffix,
        optsCopy.placeholder,
        optsCopy.description,
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
    std::string_view /*displayText*/,
    const InputFieldOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const Vec2 boxDims = MeasureInputBox(opts, size, theme);
    const ElementSpec spec = MakeInputFieldElementSpec(opts, size, opts.disabled, theme);
    return ElementComposer::Measure(boxDims, spec, canvas, theme);
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
    const Vec2 boxDims = MeasureInputBox(opts, size, theme);
    const ElementSpec spec = MakeInputFieldElementSpec(opts, size, disabled, theme);
    const ElementGeometry geom = ElementComposer::Resolve(bounds, boxDims, spec, &canvas, theme);
    ElementComposer::RenderText(canvas, geom, spec, theme);
    const Rect boxRect = geom.controlRect;

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

    if (disabled) {
        boxBg = boxBg.WithAlphaF(0.40f);
        boxBorder = boxBorder.WithAlphaF(0.40f);
        textCol = textCol.WithAlphaF(0.40f);
        placeholderCol = placeholderCol.WithAlphaF(0.40f);
        unitCol = unitCol.WithAlphaF(0.40f);
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
}

} // namespace guikit
