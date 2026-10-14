#include "guikit/widgets/TextArea.hpp"
#include <algorithm>
#include <cctype>
#include <string>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Node.hpp"

namespace guikit {

namespace {

struct LineSpan {
    size_t start{0};
    size_t length{0};
};

static size_t CountLines(std::string_view text) noexcept {
    if (text.empty()) return 1;
    size_t count = 1;
    for (char c : text) {
        if (c == '\n') ++count;
    }
    return count;
}

static void BuildLineOffsets(std::string_view text, std::vector<LineSpan>& outLines) {
    outLines.clear();
    if (text.empty()) {
        outLines.push_back({0, 0});
        return;
    }
    size_t start = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') {
            size_t len = i - start;
            if (len > 0 && text[i - 1] == '\r') {
                --len; // Exclude \r from line content
            }
            outLines.push_back({start, len});
            start = i + 1;
        }
    }
    if (start <= text.size()) {
        size_t len = text.size() - start;
        if (len > 0 && text.back() == '\r') {
            --len;
        }
        outLines.push_back({start, len});
    }
}

static bool IsCppKeyword(std::string_view word) noexcept {
    static constexpr std::string_view keywords[] = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor",
        "bool", "break", "case", "catch", "char", "char8_t", "char16_t", "char32_t",
        "class", "compl", "concept", "const", "consteval", "constexpr", "constinit",
        "const_cast", "continue", "co_await", "co_return", "co_yield", "decltype",
        "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
        "explicit", "export", "extern", "false", "float", "for", "friend", "goto",
        "if", "inline", "int", "long", "mutable", "namespace", "new", "noexcept",
        "not", "not_eq", "nullptr", "operator", "or", "or_eq", "private",
        "protected", "public", "register", "reinterpret_cast", "requires",
        "return", "short", "signed", "sizeof", "static", "static_assert",
        "static_cast", "struct", "switch", "template", "this", "thread_local",
        "throw", "true", "try", "typedef", "typeid", "typename", "union",
        "unsigned", "using", "virtual", "void", "volatile", "wchar_t", "while",
        "xor", "xor_eq"
    };
    for (std::string_view kw : keywords) {
        if (word == kw) return true;
    }
    return false;
}

static bool IsCppTypeOrStd(std::string_view word) noexcept {
    static constexpr std::string_view types[] = {
        "std", "string", "string_view", "vector", "array", "optional", "span",
        "size_t", "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t",
        "uint32_t", "uint64_t", "UiVariant", "UiSize", "Icon", "TextRole",
        "Placement", "Color", "Rect", "Vec2", "Response", "Context", "UiTheme",
        "ThemeMode", "Density", "NodeType", "HeadingOptions", "ButtonOptions",
        "SliderOptions", "TextAreaOptions", "CardOptions", "BoxOptions"
    };
    for (std::string_view ty : types) {
        if (word == ty) return true;
    }
    return false;
}

} // anonymous namespace

Response TextArea::Run(
    Context& ctx,
    std::string* stringValue,
    std::string_view displayFallback,
    const TextAreaOptions& opts,
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

    std::string_view valStr = (stringValue != nullptr) ? *stringValue : displayFallback;
    size_t cursor = 0;
    std::string_view displayText = valStr;

    if ((isFocused || ctx.HasActiveTextEdit(id)) && !opts.readOnly) {
        bool changed = false;
        bool submitted = false;
        displayText = ctx.GetOrStartTextEdit(id, valStr, cursor, changed, submitted, /*multiline=*/true);

        if (changed) {
            if (stringValue != nullptr) {
                *stringValue = displayText;
            }
            res.changed = true;
        }
        if (submitted) {
            res.submitted = true;
        }
    }

    auto& state = ctx.GetOrCreateState<TextAreaState>(id);

    // Mouse wheel scrolling for scrollable (non-autoHeight) TextArea
    if (!opts.autoHeight && !effectiveDisabled) {
        Rect prevBounds = ctx.GetPrevFrameBounds(id);
        if (prevBounds.w > 0.0f && prevBounds.h > 0.0f && prevBounds.Contains(ctx.GetMousePos())) {
            Vec2 wheel = ctx.GetMouseWheel();
            if (wheel.y != 0.0f) {
                const TypographyMetrics& tm = ctx.GetTheme().GetTypography(opts.monospace ? TextRole::CodeMedium : TextRole::BodyMedium);
                state.scrollY -= wheel.y * tm.lineHeight * 3.0f;
                if (state.scrollY < 0.0f) state.scrollY = 0.0f;
            }
        }
    }

    Node* node = ctx.CreateNode(NodeType::TextArea, id);
    node->text = ctx.AllocString(opts.label);

    TextAreaOptions optsCopy = opts;
    optsCopy.label = node->text;
    if (!opts.placeholder.empty()) optsCopy.placeholder = ctx.AllocString(opts.placeholder);

    auto* data = ctx.AllocPayload<TextAreaData>();
    data->opts = optsCopy;
    data->text = ctx.AllocString(displayText);
    data->cursor = cursor;
    data->cursorVisible = isFocused && ctx.IsCursorVisible();
    data->scrollY = state.scrollY;
    node->payload = data;

    node->visualHash = detail::HashArgs(
        opts.variant,
        opts.readOnly,
        opts.showLineNumbers,
        opts.autoHeight,
        opts.rows.value_or(0),
        data->text,
        node->text,
        data->scrollY,
        isFocused,
        data->cursorVisible
    );

    node->measureFn = [](const Node* n, float /*availableWidth*/, ICanvas* canvas, const UiTheme& theme) noexcept -> Vec2 {
        const auto* d = n->As<TextAreaData>();
        return TextArea::Measure(
            d ? d->text : std::string_view{},
            d ? d->opts : TextAreaOptions{},
            n->uiSize,
            canvas,
            theme
        );
    };

    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& theme) noexcept {
        const auto* d = n->As<TextAreaData>();
        TextArea::Render(
            canvas,
            n->computedRect,
            d ? d->text : std::string_view{},
            d ? d->opts : TextAreaOptions{},
            n->uiSize,
            n->hovered,
            n->focused,
            d ? d->cursor : 0,
            n->disabled,
            d ? d->cursorVisible : false,
            d ? d->scrollY : 0.0f,
            theme
        );
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->focused = isFocused;

    node->widthMode = SizeMode::Stretch;
    node->heightMode = opts.heightMode;

    ctx.AdaptChildToContainer(node);

    return res;
}

Vec2 TextArea::Measure(
    std::string_view text,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme
) {
    const TypographyMetrics& tm = theme.GetTypography(opts.monospace ? TextRole::CodeMedium : TextRole::BodyMedium);
    const float lineHeight = (canvas && canvas->GetLineHeight(tm.fontSize) > 0.0f)
        ? canvas->GetLineHeight(tm.fontSize)
        : tm.lineHeight;

    const float padY = theme.GetMetrics(size).paddingY * 1.25f;
    const size_t totalLines = CountLines(text);

    float h = 0.0f;
    if (opts.fixedHeight.has_value() && *opts.fixedHeight > 0.0f) {
        h = theme.Scale(*opts.fixedHeight);
    } else if (opts.autoHeight) {
        h = padY * 2.0f + static_cast<float>(totalLines) * lineHeight;
    } else if (opts.rows.has_value() && *opts.rows > 0) {
        h = padY * 2.0f + static_cast<float>(*opts.rows) * lineHeight;
    } else {
        constexpr uint32_t kDefaultRows = 6;
        h = padY * 2.0f + static_cast<float>(std::min(totalLines, static_cast<size_t>(kDefaultRows))) * lineHeight;
    }

    if (!opts.label.empty()) {
        const float labelH = canvas ? canvas->GetLineHeight(theme.GetTypography(TextRole::LabelSmall).fontSize)
                                    : theme.GetTypography(TextRole::LabelSmall).lineHeight;
        h += labelH + 4.0f;
    }

    return {0.0f, h};
}

Vec2 TextArea::Measure(
    std::string_view text,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    static const UiTheme defaultTheme = UiTheme::Dark();
    return Measure(text, opts, size, canvas, defaultTheme);
}

void TextArea::Render(
    ICanvas& canvas,
    Rect bounds,
    std::string_view text,
    const TextAreaOptions& opts,
    UiSize size,
    bool hovered,
    bool focused,
    size_t cursor,
    bool disabled,
    bool cursorVisible,
    float scrollY,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    Rect boxBounds = bounds;

    // Optional Stacked Label above the text area
    if (!opts.label.empty()) {
        const TypographyMetrics& lm = theme.GetTypography(TextRole::LabelSmall);
        const float labelH = canvas.GetLineHeight(lm.fontSize);
        const Rect labelRect(bounds.x, bounds.y, bounds.w, labelH);
        canvas.DrawTextBox(opts.label, labelRect, lm.fontSize, theme.palette.textSecondary, 0.0f, 0.5f);

        const float gap = 4.0f;
        boxBounds.y += labelH + gap;
        boxBounds.h = std::max(0.0f, boxBounds.h - (labelH + gap));
    }

    if (boxBounds.w <= 0.0f || boxBounds.h <= 0.0f) return;

    // Background and border styling
    const auto& pal = theme.palette;
    Color bg = pal.surfaceElevated;
    Color borderCol = pal.borderSubtle;
    float borderW = 1.0f;

    if (disabled) {
        bg = bg.WithAlphaF(0.40f);
        borderCol = pal.borderSubtle.WithAlphaF(0.30f);
    } else if (focused) {
        borderCol = pal.focusRing;
        borderW = 2.0f;
    } else if (hovered && !opts.readOnly) {
        borderCol = theme.GetVariant(opts.variant).bgHover;
    }

    const float radius = theme.GetRadius(size);
    canvas.DrawRoundRect(boxBounds, radius, bg, borderCol, borderW);

    // Font metrics
    const TypographyMetrics& tm = theme.GetTypography(opts.monospace ? TextRole::CodeMedium : TextRole::BodyMedium);
    const float fontSize = tm.fontSize;
    const float lineHeight = (canvas.GetLineHeight(fontSize) > 0.0f) ? canvas.GetLineHeight(fontSize) : tm.lineHeight;
    const float charAdvance = fontSize * (tm.isMonospace ? kMonospaceCharAdvanceRatio : kDefaultCharAdvanceRatio);

    // Line offset table
    static thread_local std::vector<LineSpan> s_lines;
    BuildLineOffsets(text, s_lines);
    const size_t totalLines = s_lines.size();

    // Gutter / Line numbers column calculation
    float gutterWidth = 0.0f;
    if (opts.showLineNumbers) {
        int digits = 2;
        size_t n = totalLines;
        while (n >= 100) {
            digits++;
            n /= 10;
        }
        const float gutterPadX = 8.0f;
        gutterWidth = gutterPadX + static_cast<float>(digits) * charAdvance + gutterPadX;

        // Subtle vertical separator line between gutter and code
        const float divX = boxBounds.x + gutterWidth;
        canvas.DrawLine({divX, boxBounds.y + 1.0f}, {divX, boxBounds.Bottom() - 1.0f}, pal.borderSubtle.WithAlphaF(0.40f), 1.0f);
    }

    const float padX = 10.0f;
    const float padY = theme.GetMetrics(size).paddingY * 1.25f;

    const Rect textViewport(
        boxBounds.x + gutterWidth + padX,
        boxBounds.y + padY,
        std::max(0.0f, boxBounds.w - gutterWidth - padX * 2.0f),
        std::max(0.0f, boxBounds.h - padY * 2.0f)
    );

    if (textViewport.w <= 0.0f || textViewport.h <= 0.0f) return;

    // Visible lines virtual window (Viewport Culling)
    const float totalContentH = static_cast<float>(totalLines) * lineHeight;
    const float maxScrollY = std::max(0.0f, totalContentH - textViewport.h);
    const float activeScrollY = opts.autoHeight ? 0.0f : std::clamp(scrollY, 0.0f, maxScrollY);

    int firstLine = std::clamp(static_cast<int>(activeScrollY / lineHeight), 0, static_cast<int>(totalLines - 1));
    int lastLine = std::clamp(static_cast<int>((activeScrollY + textViewport.h) / lineHeight) + 1, 0, static_cast<int>(totalLines - 1));

    // Render Gutter Line Numbers
    if (opts.showLineNumbers) {
        const float gutterPadX = 8.0f;
        for (int i = firstLine; i <= lastLine; ++i) {
            const float lineY = textViewport.y + static_cast<float>(i) * lineHeight - activeScrollY;
            std::string numStr = std::to_string(i + 1);
            const Rect numRect(boxBounds.x, lineY, gutterWidth - gutterPadX, lineHeight);
            canvas.DrawTextBox(numStr, numRect, fontSize, pal.textMuted.WithAlphaF(0.60f), 1.0f, 0.0f);
        }
    }

    // Render Text Lines (clipped to viewport)
    canvas.PushClip(textViewport);

    static thread_local std::vector<TextSpan> s_spans;
    const Color defaultTextColor = theme.GetVariant(opts.variant).text;

    // Track cursor coordinate
    float cursorX = textViewport.x;
    float cursorY = textViewport.y;
    bool cursorFound = false;

    for (int i = firstLine; i <= lastLine; ++i) {
        const LineSpan& span = s_lines[i];
        std::string_view lineStr = (span.length > 0) ? text.substr(span.start, span.length) : std::string_view{};
        const float lineY = textViewport.y + static_cast<float>(i) * lineHeight - activeScrollY;

        // Check if cursor is on this line
        if (focused && !opts.readOnly && !cursorFound) {
            if (cursor >= span.start && cursor <= span.start + span.length) {
                const size_t col = cursor - span.start;
                cursorX = textViewport.x + static_cast<float>(col) * charAdvance;
                cursorY = lineY;
                cursorFound = true;
            }
        }

        if (lineStr.empty()) {
            continue;
        }

        if (opts.syntaxHighlighter != nullptr) {
            s_spans.clear();
            opts.syntaxHighlighter(lineStr, static_cast<size_t>(i), s_spans, theme, opts.highlighterUserData);
            if (s_spans.empty()) {
                canvas.DrawText(lineStr, {textViewport.x, lineY}, fontSize, defaultTextColor);
            } else {
                uint32_t currentCol = 0;
                for (const auto& s : s_spans) {
                    if (s.startCol > currentCol) {
                        std::string_view gapText = lineStr.substr(currentCol, s.startCol - currentCol);
                        float gapX = textViewport.x + static_cast<float>(currentCol) * charAdvance;
                        canvas.DrawText(gapText, {gapX, lineY}, fontSize, defaultTextColor);
                    }
                    if (s.length > 0 && s.startCol < lineStr.size()) {
                        size_t actualLen = std::min(static_cast<size_t>(s.length), lineStr.size() - s.startCol);
                        std::string_view spanText = lineStr.substr(s.startCol, actualLen);
                        float spanX = textViewport.x + static_cast<float>(s.startCol) * charAdvance;
                        canvas.DrawText(spanText, {spanX, lineY}, fontSize, s.color);
                        currentCol = s.startCol + static_cast<uint32_t>(actualLen);
                    }
                }
                if (currentCol < lineStr.size()) {
                    std::string_view trailing = lineStr.substr(currentCol);
                    float trailX = textViewport.x + static_cast<float>(currentCol) * charAdvance;
                    canvas.DrawText(trailing, {trailX, lineY}, fontSize, defaultTextColor);
                }
            }
        } else {
            canvas.DrawText(lineStr, {textViewport.x, lineY}, fontSize, defaultTextColor);
        }
    }

    // Render Blinking Cursor in editable mode
    if (focused && !opts.readOnly && cursorVisible && cursorFound) {
        const Rect curRect(cursorX, cursorY, 1.5f, lineHeight);
        canvas.DrawRect(curRect, pal.caret);
    }

    canvas.PopClip();
}

void TextArea::CppHighlighter(
    std::string_view lineText,
    size_t /*lineIndex*/,
    std::vector<TextSpan>& outSpans,
    const UiTheme& theme,
    void* /*userData*/
) {
    if (lineText.empty()) return;

    const bool dark = (theme.mode == ThemeMode::Dark);
    const Color kwColor     = dark ? Color::Hex(0xF472B6FF) : Color::Hex(0xDB2777FF); // Pink/Rose Keyword
    const Color typeColor   = dark ? Color::Hex(0x38BDF8FF) : Color::Hex(0x0284C7FF); // Sky Blue Type
    const Color strColor    = dark ? Color::Hex(0x34D399FF) : Color::Hex(0x059669FF); // Emerald String
    const Color numColor    = dark ? Color::Hex(0xFBBF24FF) : Color::Hex(0xD97706FF); // Amber Number
    const Color commentCol  = Color::Hex(0x64748BFF);                                  // Slate Muted Comment
    const Color punctColor  = dark ? Color::Hex(0x94A3B8FF) : Color::Hex(0x64748BFF); // Slate Subdued Symbols

    size_t i = 0;
    const size_t n = lineText.size();

    while (i < n) {
        // Skip whitespace
        if (std::isspace(static_cast<unsigned char>(lineText[i]))) {
            ++i;
            continue;
        }

        // Line comment: // ...
        if (i + 1 < n && lineText[i] == '/' && lineText[i + 1] == '/') {
            outSpans.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(n - i), commentCol});
            break;
        }

        // String literal: "..."
        if (lineText[i] == '"') {
            size_t start = i++;
            while (i < n && lineText[i] != '"') {
                if (lineText[i] == '\\' && i + 1 < n) ++i; // escape sequence
                ++i;
            }
            if (i < n && lineText[i] == '"') ++i;
            outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), strColor});
            continue;
        }

        // Character literal: '...'
        if (lineText[i] == '\'') {
            size_t start = i++;
            while (i < n && lineText[i] != '\'') {
                if (lineText[i] == '\\' && i + 1 < n) ++i;
                ++i;
            }
            if (i < n && lineText[i] == '\'') ++i;
            outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), strColor});
            continue;
        }

        // Numbers: 123, 0.0f, 0x1A
        if (std::isdigit(static_cast<unsigned char>(lineText[i]))) {
            size_t start = i++;
            while (i < n && (std::isalnum(static_cast<unsigned char>(lineText[i])) || lineText[i] == '.' || lineText[i] == 'f')) {
                ++i;
            }
            outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), numColor});
            continue;
        }

        // Identifiers and Keywords
        if (std::isalpha(static_cast<unsigned char>(lineText[i])) || lineText[i] == '_') {
            size_t start = i++;
            while (i < n && (std::isalnum(static_cast<unsigned char>(lineText[i])) || lineText[i] == '_')) {
                ++i;
            }
            std::string_view word = lineText.substr(start, i - start);
            if (IsCppKeyword(word)) {
                outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), kwColor});
            } else if (IsCppTypeOrStd(word)) {
                outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), typeColor});
            }
            continue;
        }

        // Punctuation and operators: ::, ->, {, }, (, ), ;, etc.
        if (lineText[i] == '{' || lineText[i] == '}' || lineText[i] == '(' || lineText[i] == ')' ||
            lineText[i] == ';' || lineText[i] == ',' || lineText[i] == '.' || lineText[i] == ':') {
            outSpans.push_back({static_cast<uint32_t>(i), 1, punctColor});
            ++i;
            continue;
        }

        ++i;
    }
}

} // namespace guikit
