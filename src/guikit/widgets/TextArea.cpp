#include "guikit/widgets/TextArea.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Element.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"

namespace guikit {

namespace {

static constexpr std::array<std::string_view, 67> kCppKeywords = {
    "alignas", "alignof", "auto", "break", "case", "catch", "class", "concept",
    "const", "consteval", "constexpr", "constinit", "continue", "co_await",
    "co_return", "co_yield", "decltype", "default", "delete", "do", "else",
    "enum", "explicit", "export", "extern", "false", "final", "for", "friend",
    "goto", "if", "inline", "mutable", "namespace", "new", "noexcept", "nullptr",
    "operator", "override", "private", "protected", "public", "requires",
    "return", "sizeof", "static", "static_assert", "static_cast", "dynamic_cast",
    "reinterpret_cast", "const_cast", "struct", "switch", "template", "this",
    "throw", "true", "try", "typedef", "typeid", "typename", "union", "using",
    "virtual", "volatile", "while", "import"
};

static constexpr std::array<std::string_view, 86> kCppTypes = {
    "bool", "char", "char8_t", "char16_t", "char32_t", "wchar_t", "short", "int",
    "long", "signed", "unsigned", "float", "double", "void", "size_t", "ptrdiff_t",
    "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t",
    "uint64_t", "std", "string", "string_view", "vector", "array", "span",
    "optional", "pair", "tuple", "unique_ptr", "shared_ptr", "function",
    "guikit", "Context", "Response", "WidgetResponse", "ContainerScope",
    "UiTheme", "UiStyle", "UiSize", "UiVariant", "UiText", "TextRole", "TextWrap",
    "TextLines", "TextVariants", "FontStyle", "Density", "ThemeMode", "Color",
    "Rect", "Vec2", "Padding", "Icon", "IconId", "Align", "Alignment", "Justify",
    "SizeMode", "StretchMode", "Orientation", "ElementLayout", "InlineOrder",
    "HeadingDivider", "CursorMode", "TabBarStyle", "SyntaxConfig", "SyntaxPalette",
    "TextSpan", "Element", "Button", "Slider", "StepSlider", "InputField",
    "Select", "Switch", "Checkbox", "Heading", "Text", "TextArea", "TabBar",
    "Toolbar"
};

static constexpr std::array<std::string_view, 3> kJsonKeywords = {
    "true", "false", "null"
};

bool IsIdentStart(char c) noexcept {
    const unsigned char u = static_cast<unsigned char>(c);
    return std::isalpha(u) || c == '_';
}

bool IsIdentChar(char c) noexcept {
    const unsigned char u = static_cast<unsigned char>(c);
    return std::isalnum(u) || c == '_';
}

bool ContainsToken(std::span<const std::string_view> list, std::string_view token) noexcept {
    for (std::string_view item : list) {
        if (item == token) return true;
    }
    return false;
}

float MeasureChunkNoTabs(
    ICanvas* canvas,
    std::string_view chunk,
    float fontSize,
    FontStyle style
) noexcept {
    if (chunk.empty() || fontSize <= 0.0f) return 0.0f;
    if (canvas) {
        return canvas->MeasureTextStyled(chunk, fontSize, style).x;
    }
    const float factor = (style == FontStyle::Monospace) ? 0.60f : 0.56f;
    return static_cast<float>(chunk.size()) * fontSize * factor;
}

float MeasureSegmentWidth(
    ICanvas* canvas,
    std::string_view seg,
    float fontSize,
    FontStyle style
) noexcept {
    if (seg.empty() || fontSize <= 0.0f) return 0.0f;
    const size_t firstTab = seg.find('\t');
    if (firstTab == std::string_view::npos) {
        return MeasureChunkNoTabs(canvas, seg, fontSize, style);
    }

    const float tabW = MeasureChunkNoTabs(canvas, " ", fontSize, style) * 4.0f;
    float totalW = 0.0f;
    size_t pos = 0;
    while (pos < seg.size()) {
        if (seg[pos] == '\t') {
            size_t tabEnd = pos + 1;
            while (tabEnd < seg.size() && seg[tabEnd] == '\t') {
                ++tabEnd;
            }
            totalW += static_cast<float>(tabEnd - pos) * tabW;
            pos = tabEnd;
        } else {
            size_t nextTab = seg.find('\t', pos);
            if (nextTab == std::string_view::npos) {
                nextTab = seg.size();
            }
            totalW += MeasureChunkNoTabs(canvas, seg.substr(pos, nextTab - pos), fontSize, style);
            pos = nextTab;
        }
    }
    return totalW;
}

size_t HitTestColumnInLine(
    ICanvas* canvas,
    std::string_view lineSub,
    float localX,
    float fontSize,
    FontStyle style
) noexcept {
    if (lineSub.empty() || localX <= 0.0f) return 0;
    size_t pos = 0;
    float prevW = 0.0f;
    while (pos < lineSub.size()) {
        const size_t nextPos = TextUtil::NextCharBoundary(lineSub, pos);
        const float nextW = MeasureSegmentWidth(canvas, lineSub.substr(0, nextPos), fontSize, style);
        const float mid = (prevW + nextW) * 0.5f;
        if (localX < mid) {
            return pos;
        }
        prevW = nextW;
        pos = nextPos;
    }
    return lineSub.size();
}

struct LogicalLine {
    size_t startOffset{0};
    size_t length{0};
};

struct VisualLine {
    size_t logicalLine{0};
    size_t startOffset{0}; ///< Global byte offset in text
    size_t startCol{0};    ///< Column byte offset within logical line
    size_t length{0};      ///< Byte length of this visual sub-line
    bool isFirstSubLine{true};
};

void SplitLogicalLines(std::string_view text, std::vector<LogicalLine>& outLines) {
    outLines.clear();
    size_t start = 0;
    while (start <= text.size()) {
        const size_t nl = text.find('\n', start);
        if (nl == std::string_view::npos) {
            size_t len = text.size() - start;
            if (len > 0 && text[start + len - 1] == '\r') {
                --len;
            }
            outLines.push_back({start, len});
            break;
        }
        size_t len = nl - start;
        if (len > 0 && text[start + len - 1] == '\r') {
            --len;
        }
        outLines.push_back({start, len});
        start = nl + 1;
    }
    if (outLines.empty()) {
        outLines.push_back({0, 0});
    }
}

void BuildVisualLines(
    std::string_view text,
    const std::vector<LogicalLine>& logicalLines,
    bool wordWrap,
    float wrapWidth,
    float fontSize,
    FontStyle style,
    ICanvas* canvas,
    std::vector<VisualLine>& outVisual
) {
    outVisual.clear();
    outVisual.reserve(logicalLines.size());

    for (size_t i = 0; i < logicalLines.size(); ++i) {
        const LogicalLine& ll = logicalLines[i];
        const std::string_view lineStr = text.substr(ll.startOffset, ll.length);
        if (!wordWrap || wrapWidth <= 24.0f || lineStr.empty()) {
            outVisual.push_back({i, ll.startOffset, 0, ll.length, true});
            continue;
        }

        const float fullW = MeasureSegmentWidth(canvas, lineStr, fontSize, style);
        if (fullW <= wrapWidth + 0.5f) {
            outVisual.push_back({i, ll.startOffset, 0, ll.length, true});
            continue;
        }

        size_t subStart = 0;
        bool first = true;
        while (subStart < ll.length) {
            size_t cur = subStart;
            size_t lastBreak = subStart;
            while (cur < ll.length) {
                const size_t next = TextUtil::NextCharBoundary(lineStr, cur);
                const float w = MeasureSegmentWidth(canvas, lineStr.substr(subStart, next - subStart), fontSize, style);
                if (w > wrapWidth && cur > subStart) {
                    break;
                }
                cur = next;
                if (cur < ll.length && (lineStr[cur - 1] == ' ' || lineStr[cur - 1] == '\t' || lineStr[cur - 1] == ',' || lineStr[cur - 1] == ';')) {
                    lastBreak = cur;
                }
            }
            const size_t takeEnd = (lastBreak > subStart && cur < ll.length) ? lastBreak : cur;
            const size_t subLen = std::max<size_t>(1, takeEnd - subStart);
            outVisual.push_back({i, ll.startOffset + subStart, subStart, subLen, first});
            first = false;
            subStart += subLen;
        }
    }
    if (outVisual.empty()) {
        outVisual.push_back({0, 0, 0, 0, true});
    }
}

ElementSpec MakeTextAreaElementSpec(
    const TextAreaOptions& opts,
    UiSize size,
    bool disabled,
    const UiTheme& theme
) noexcept {
    const InputFieldMetrics& im = theme.GetInputField(size);
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
        .labelGap = im.labelGap,
        .descGap = im.descGap,
        .inlineGap = im.paddingX,
        .divider = opts.divider,
        .dividerStyle = opts.dividerStyle,
        .stretchableControl = true,
        .stretchHeightWhenStandalone = !opts.autoHeight,
        .disabled = disabled
    };
}

struct EditorFontMetrics {
    float fontSize{13.0f};
    float glyphBoxH{18.0f};
    float lineHeight{21.0f};
    float textOffsetY{1.5f};
    float spaceAdvance{7.8f};
    float tabAdvance{31.2f};
    float digitAdvance{7.8f};
};

EditorFontMetrics ResolveEditorFontMetrics(
    ICanvas* canvas,
    const UiTheme& theme,
    float baseFontSize,
    float effZoom,
    FontStyle style
) noexcept {
    EditorFontMetrics m{};
    const float clampedZoom = std::clamp(effZoom, 0.5f, 3.0f);
    m.fontSize = baseFontSize * clampedZoom;

    if (canvas) {
        const Vec2 sample = canvas->MeasureTextStyled("Mg", m.fontSize, style);
        m.glyphBoxH = (sample.y > 0.0f) ? sample.y : theme.GetLineHeight(m.fontSize);
        const float spW = canvas->MeasureTextStyled(" ", m.fontSize, style).x;
        m.spaceAdvance = (spW > 0.0f) ? spW : (m.fontSize * 0.60f);
        const float digW = canvas->MeasureTextStyled("8", m.fontSize, FontStyle::Monospace).x;
        m.digitAdvance = (digW > 0.0f) ? digW : m.spaceAdvance;
    } else {
        m.glyphBoxH = theme.GetLineHeight(m.fontSize);
        const float factor = (style == FontStyle::Monospace) ? 0.60f : 0.56f;
        m.spaceAdvance = m.fontSize * factor;
        m.digitAdvance = m.fontSize * 0.60f;
    }

    // Strictly linear floating-point line height without integer std::ceil quantization:
    // ensures X and Y scale with the exact same derivative at every fractional zoom step.
    m.lineHeight = std::max(theme.GetLineHeight(m.fontSize), m.glyphBoxH * 1.14f);
    m.textOffsetY = (m.lineHeight - m.glyphBoxH) * 0.5f;
    m.tabAdvance = m.spaceAdvance * 4.0f;
    return m;
}

Vec2 MeasureEditorBox(
    std::string_view text,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float effZoom
) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const FontStyle textStyle = opts.monospace ? FontStyle::Monospace : FontStyle::Regular;
    const float baseFontSize = opts.monospace
        ? theme.GetTypography(TextRole::CodeMedium).fontSize
        : theme.ResolveFontSize(TextRole::BodyMedium, size);
    const EditorFontMetrics fm = ResolveEditorFontMetrics(canvas, theme, baseFontSize, effZoom, textStyle);
    const float fontSize = fm.fontSize;
    const float lineHeight = fm.lineHeight;
    const float padY = std::max(theme.Scale(8.0f), cm.paddingY);
    const float padX = std::max(theme.Scale(10.0f), cm.paddingX);

    size_t lineCount = 1;
    size_t maxLineChars = 0;
    size_t curChars = 0;
    for (char c : text) {
        if (c == '\n') {
            ++lineCount;
            maxLineChars = std::max(maxLineChars, curChars);
            curChars = 0;
        } else if (c != '\r') {
            ++curChars;
        }
    }
    maxLineChars = std::max(maxLineChars, curChars);

    float boxH = 0.0f;
    if (opts.fixedHeight.has_value()) {
        boxH = theme.Scale(*opts.fixedHeight);
    } else if (opts.autoHeight) {
        boxH = static_cast<float>(lineCount) * lineHeight + padY * 2.0f;
    } else if (opts.rows.has_value()) {
        const uint32_t r = std::max(1u, *opts.rows);
        boxH = static_cast<float>(r) * lineHeight + padY * 2.0f;
    } else {
        boxH = 4.0f * lineHeight + padY * 2.0f;
    }

    size_t digits = 2;
    size_t n = lineCount;
    while (n >= 100) {
        ++digits;
        n /= 10;
    }
    const float gutterPadX = fm.spaceAdvance * 1.25f;
    const float gutterWidth = opts.showLineNumbers
        ? (static_cast<float>(digits) * fm.digitAdvance + gutterPadX * 2.0f)
        : 0.0f;

    float boxW = theme.Scale(280.0f);
    if (opts.stretch != StretchMode::Fill && opts.width.has_value()) {
        boxW = theme.Scale(*opts.width);
    } else {
        const float charAdv = MeasureChunkNoTabs(canvas, "M", fontSize, textStyle);
        const float estimatedW = gutterWidth + padX * 2.0f + static_cast<float>(std::min<size_t>(maxLineChars, 80)) * charAdv + theme.Scale(24.0f);
        boxW = std::max(boxW, estimatedW);
    }

    return {boxW, boxH};
}

struct EditorLayoutRects {
    ElementGeometry elemGeom{};
    Rect editorBox{};
    Rect innerBounds{};
    Rect gutterRect{};
    Rect textViewport{};
    Rect vTrackRect{};
    Rect vThumbRect{};
    Rect hTrackRect{};
    Rect hThumbRect{};
    Rect copyBtnRect{};
    Rect zoomBtnRect{};
    EditorFontMetrics metrics{};
    float fontSize{13.0f};
    float glyphBoxH{18.0f};
    float lineHeight{18.0f};
    float gutterWidth{0.0f};
    float maxScrollY{0.0f};
    float maxScrollX{0.0f};
    float totalContentH{0.0f};
    float maxLineW{0.0f};
    bool needVScroll{false};
    bool needHScroll{false};
};

EditorLayoutRects ComputeEditorLayout(
    Rect bounds,
    std::string_view text,
    const std::vector<LogicalLine>& logicalLines,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float effZoom,
    float scrollX,
    float scrollY,
    bool copyFeedbackActive,
    std::vector<VisualLine>& outVisualLines
) {
    EditorLayoutRects L{};
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const ScrollbarMetrics& sm = theme.GetScrollbar(size);
    const FontStyle textStyle = opts.monospace ? FontStyle::Monospace : FontStyle::Regular;

    const float baseFontSize = opts.monospace
        ? theme.GetTypography(TextRole::CodeMedium).fontSize
        : theme.ResolveFontSize(TextRole::BodyMedium, size);
    L.metrics = ResolveEditorFontMetrics(canvas, theme, baseFontSize, effZoom, textStyle);
    L.fontSize = L.metrics.fontSize;
    L.glyphBoxH = L.metrics.glyphBoxH;
    L.lineHeight = L.metrics.lineHeight;

    const Vec2 intrinsicBox = MeasureEditorBox(text, opts, size, canvas, theme, effZoom);
    const ElementSpec spec = MakeTextAreaElementSpec(opts, size, opts.disabled, theme);
    L.elemGeom = Element::Resolve(bounds, intrinsicBox, spec, canvas, theme);
    L.editorBox = L.elemGeom.controlRect;

    const float padY = std::max(theme.Scale(8.0f), cm.paddingY);
    const float padX = std::max(theme.Scale(10.0f), cm.paddingX);

    size_t digits = 2;
    size_t n = std::max<size_t>(1, logicalLines.size());
    while (n >= 100) {
        ++digits;
        n /= 10;
    }
    const float gutterPadX = L.metrics.spaceAdvance * 1.25f;
    L.gutterWidth = opts.showLineNumbers
        ? (static_cast<float>(digits) * L.metrics.digitAdvance + gutterPadX * 2.0f)
        : 0.0f;

    L.innerBounds = Rect(
        L.editorBox.x + 1.0f,
        L.editorBox.y + 1.0f,
        std::max(0.0f, L.editorBox.w - 2.0f),
        std::max(0.0f, L.editorBox.h - 2.0f)
    );

    if (opts.showLineNumbers && L.gutterWidth > 0.0f) {
        L.gutterRect = Rect(
            L.innerBounds.x,
            L.innerBounds.y,
            std::min(L.gutterWidth, L.innerBounds.w),
            L.innerBounds.h
        );
    }

    // Preliminary viewport width before scrollbars
    float prelimTextW = std::max(0.0f, L.innerBounds.w - L.gutterWidth - padX * 2.0f);
    BuildVisualLines(text, logicalLines, opts.wordWrap, prelimTextW, L.fontSize, textStyle, canvas, outVisualLines);

    L.totalContentH = static_cast<float>(outVisualLines.size()) * L.lineHeight;
    const float availTextH = std::max(0.0f, L.innerBounds.h - padY * 2.0f);
    L.needVScroll = !opts.autoHeight && (L.totalContentH > availTextH + 1.0f) && sm.trackWidth > 0.0f;

    const float vBarReserve = L.needVScroll ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;
    if (L.needVScroll && opts.wordWrap) {
        prelimTextW = std::max(0.0f, L.innerBounds.w - L.gutterWidth - padX * 2.0f - vBarReserve);
        BuildVisualLines(text, logicalLines, opts.wordWrap, prelimTextW, L.fontSize, textStyle, canvas, outVisualLines);
        L.totalContentH = static_cast<float>(outVisualLines.size()) * L.lineHeight;
    }

    // Measure max line width for horizontal scrolling when !wordWrap
    L.maxLineW = 0.0f;
    if (!opts.wordWrap) {
        for (const LogicalLine& ll : logicalLines) {
            if (ll.length == 0) continue;
            const std::string_view ls = text.substr(ll.startOffset, ll.length);
            L.maxLineW = std::max(L.maxLineW, MeasureSegmentWidth(canvas, ls, L.fontSize, textStyle));
        }
    }

    const float availTextW = std::max(0.0f, L.innerBounds.w - L.gutterWidth - padX * 2.0f - vBarReserve);
    L.needHScroll = !opts.wordWrap && (L.maxLineW > availTextW + 2.0f) && sm.trackWidth > 0.0f;
    const float hBarReserve = L.needHScroll ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;

    const float finalViewportH = std::max(0.0f, L.innerBounds.h - padY * 2.0f - hBarReserve);
    L.needVScroll = !opts.autoHeight && (L.totalContentH > finalViewportH + 1.0f) && sm.trackWidth > 0.0f;

    L.textViewport = Rect(
        L.innerBounds.x + L.gutterWidth + padX,
        L.innerBounds.y + padY,
        std::max(0.0f, L.innerBounds.w - L.gutterWidth - padX * 2.0f - (L.needVScroll ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f)),
        finalViewportH
    );

    L.maxScrollY = std::max(0.0f, L.totalContentH - L.textViewport.h);
    L.maxScrollX = opts.wordWrap ? 0.0f : std::max(0.0f, L.maxLineW + theme.Scale(16.0f) - L.textViewport.w);

    const float clampedScrollY = std::clamp(scrollY, 0.0f, L.maxScrollY);
    const float clampedScrollX = std::clamp(scrollX, 0.0f, L.maxScrollX);

    if (L.needVScroll) {
        const float trackX = L.innerBounds.Right() - sm.trackWidth - sm.margin;
        const float trackY = L.innerBounds.y + sm.margin;
        const float trackH = std::max(0.0f, L.innerBounds.h - sm.margin * 2.0f - hBarReserve);
        L.vTrackRect = Rect(trackX, trackY, sm.trackWidth, trackH);
        if (trackH > 0.0f && L.totalContentH > 0.0f) {
            const float visibleRatio = std::clamp(L.textViewport.h / L.totalContentH, 0.05f, 1.0f);
            const float thumbH = std::min(trackH, std::max(sm.minThumbLength, trackH * visibleRatio));
            const float maxTravel = std::max(0.0f, trackH - thumbH);
            const float thumbY = trackY + ((L.maxScrollY > 0.0f) ? (clampedScrollY / L.maxScrollY) * maxTravel : 0.0f);
            L.vThumbRect = Rect(trackX, thumbY, sm.trackWidth, thumbH);
        }
    }

    if (L.needHScroll) {
        const float trackX = L.innerBounds.x + L.gutterWidth + sm.margin;
        const float trackY = L.innerBounds.Bottom() - sm.trackWidth - sm.margin;
        const float trackW = std::max(0.0f, L.innerBounds.w - L.gutterWidth - sm.margin * 2.0f - (L.needVScroll ? (sm.trackWidth + sm.margin) : 0.0f));
        L.hTrackRect = Rect(trackX, trackY, trackW, sm.trackWidth);
        const float totalW = L.maxLineW + theme.Scale(16.0f);
        if (trackW > 0.0f && totalW > 0.0f) {
            const float visibleRatio = std::clamp(L.textViewport.w / totalW, 0.05f, 1.0f);
            const float thumbW = std::min(trackW, std::max(sm.minThumbLength, trackW * visibleRatio));
            const float maxTravel = std::max(0.0f, trackW - thumbW);
            const float thumbX = trackX + ((L.maxScrollX > 0.0f) ? (clampedScrollX / L.maxScrollX) * maxTravel : 0.0f);
            L.hThumbRect = Rect(thumbX, trackY, thumbW, sm.trackWidth);
        }
    }

    // Top-right overlay buttons (Copy & Zoom %)
    const float overlayRight = L.innerBounds.Right() - (L.needVScroll ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f) - theme.Scale(6.0f);
    const float overlayTop = L.innerBounds.y + theme.Scale(6.0f);
    const float badgeFs = theme.GetFontSize(TextRole::LabelSmall, size);
    const float btnH = std::max(theme.Scale(22.0f), badgeFs + theme.Scale(8.0f));
    float rightCursor = overlayRight;

    if (opts.showCopyButton && L.innerBounds.w > theme.Scale(120.0f)) {
        const std::string_view copyLbl = copyFeedbackActive ? "Copied" : "Copy";
        const float lblW = TextUtil::MeasureWidth(copyLbl, badgeFs, canvas);
        const float iconSz = theme.Scale(11.0f);
        const float btnW = lblW + iconSz + theme.Scale(16.0f);
        L.copyBtnRect = Rect(rightCursor - btnW, overlayTop, btnW, btnH);
        rightCursor -= (btnW + theme.Scale(6.0f));
    }

    if (opts.allowZoom && std::abs(effZoom - 1.0f) > 0.01f && L.innerBounds.w > theme.Scale(140.0f)) {
        char zBuf[16]{};
        std::snprintf(zBuf, sizeof(zBuf), "%d%%", static_cast<int>(std::lround(effZoom * 100.0f)));
        const float zW = TextUtil::MeasureWidth(zBuf, badgeFs, canvas) + theme.Scale(12.0f);
        L.zoomBtnRect = Rect(rightCursor - zW, overlayTop, zW, btnH);
    }

    return L;
}

size_t FindVisualLineForOffset(const std::vector<VisualLine>& visualLines, size_t offset) noexcept {
    if (visualLines.empty()) return 0;
    for (size_t i = 0; i < visualLines.size(); ++i) {
        const VisualLine& vl = visualLines[i];
        const size_t vlEnd = vl.startOffset + vl.length;
        if (offset >= vl.startOffset && offset <= vlEnd) {
            if (offset == vlEnd && i + 1 < visualLines.size() && visualLines[i + 1].logicalLine == vl.logicalLine) {
                continue;
            }
            return i;
        }
    }
    return visualLines.size() - 1;
}

} // namespace

SyntaxConfig SyntaxConfig::Cpp() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .keywords = kCppKeywords,
        .types = kCppTypes,
        .lineComment = "//",
        .blockCommentStart = "/*",
        .blockCommentEnd = "*/",
        .preprocessorPrefix = '#',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = true,
        .highlightPunctuation = true
    };
}

SyntaxConfig SyntaxConfig::Json() noexcept {
    return SyntaxConfig{
        .enabled = true,
        .keywords = kJsonKeywords,
        .types = {},
        .lineComment = "",
        .blockCommentStart = "",
        .blockCommentEnd = "",
        .preprocessorPrefix = '\0',
        .highlightStrings = true,
        .highlightNumbers = true,
        .highlightFunctions = false,
        .highlightPunctuation = true
    };
}

void TextArea::HighlightLine(
    std::string_view lineText,
    size_t /*lineIndex*/,
    const SyntaxConfig& config,
    std::vector<TextSpan>& outSpans,
    const UiTheme& theme,
    const SyntaxPalette* paletteOverride
) {
    outSpans.clear();
    if (lineText.empty()) return;

    const SyntaxPalette& pal = paletteOverride
        ? *paletteOverride
        : (config.palette.has_value() ? *config.palette : theme.syntax);

    if (!config.enabled) {
        outSpans.push_back({0, static_cast<uint32_t>(lineText.size()), pal.identifier});
        return;
    }

    const size_t len = lineText.size();
    size_t i = 0;

    // Skip leading whitespace
    while (i < len && (lineText[i] == ' ' || lineText[i] == '\t')) {
        ++i;
    }
    if (i > 0) {
        outSpans.push_back({0, static_cast<uint32_t>(i), pal.identifier});
    }

    // Preprocessor directive check
    if (config.preprocessorPrefix != '\0' && i < len && lineText[i] == config.preprocessorPrefix) {
        // Highlight directive up to quotes or '<', and string/header path if present
        size_t qPos = lineText.find_first_of("\"<", i + 1);
        if (qPos != std::string_view::npos && config.highlightStrings) {
            outSpans.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(qPos - i), pal.preprocessor});
            outSpans.push_back({static_cast<uint32_t>(qPos), static_cast<uint32_t>(len - qPos), pal.string});
        } else {
            outSpans.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(len - i), pal.preprocessor});
        }
        return;
    }

    while (i < len) {
        // 1. Whitespace run
        if (lineText[i] == ' ' || lineText[i] == '\t') {
            const size_t start = i;
            while (i < len && (lineText[i] == ' ' || lineText[i] == '\t')) {
                ++i;
            }
            outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), pal.identifier});
            continue;
        }

        // 2. Line comment
        if (!config.lineComment.empty() &&
            i + config.lineComment.size() <= len &&
            lineText.substr(i, config.lineComment.size()) == config.lineComment) {
            outSpans.push_back({static_cast<uint32_t>(i), static_cast<uint32_t>(len - i), pal.comment});
            break;
        }

        // 3. Block comment
        if (!config.blockCommentStart.empty() &&
            i + config.blockCommentStart.size() <= len &&
            lineText.substr(i, config.blockCommentStart.size()) == config.blockCommentStart) {
            const size_t start = i;
            i += config.blockCommentStart.size();
            if (!config.blockCommentEnd.empty()) {
                const size_t endPos = lineText.find(config.blockCommentEnd, i);
                if (endPos != std::string_view::npos) {
                    i = endPos + config.blockCommentEnd.size();
                } else {
                    i = len;
                }
            } else {
                i = len;
            }
            outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), pal.comment});
            continue;
        }

        // 4. String / char literal
        if (config.highlightStrings && (lineText[i] == '"' || lineText[i] == '\'')) {
            const char quote = lineText[i];
            const size_t start = i++;
            while (i < len) {
                if (lineText[i] == '\\' && i + 1 < len) {
                    i += 2;
                    continue;
                }
                if (lineText[i] == quote) {
                    ++i;
                    break;
                }
                ++i;
            }
            outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), pal.string});
            continue;
        }

        // 5. Numeric literal
        if (config.highlightNumbers &&
            (std::isdigit(static_cast<unsigned char>(lineText[i])) ||
             (lineText[i] == '.' && i + 1 < len && std::isdigit(static_cast<unsigned char>(lineText[i + 1]))))) {
            const size_t start = i++;
            while (i < len && (std::isalnum(static_cast<unsigned char>(lineText[i])) || lineText[i] == '.' || lineText[i] == '\'')) {
                ++i;
            }
            outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), pal.number});
            continue;
        }

        // 6. Identifier / keyword / type / function call
        if (IsIdentStart(lineText[i])) {
            const size_t start = i++;
            while (i < len && IsIdentChar(lineText[i])) {
                ++i;
            }
            const std::string_view word = lineText.substr(start, i - start);
            Color tokenColor = pal.identifier;
            if (ContainsToken(config.keywords, word)) {
                tokenColor = pal.keyword;
            } else if (ContainsToken(config.types, word)) {
                tokenColor = pal.type;
            } else if (config.highlightFunctions) {
                size_t look = i;
                while (look < len && (lineText[look] == ' ' || lineText[look] == '\t')) {
                    ++look;
                }
                if (look < len && lineText[look] == '(') {
                    tokenColor = pal.function;
                }
            }
            outSpans.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(i - start), tokenColor});
            continue;
        }

        // 7. Punctuation / operators
        const size_t start = i++;
        while (i < len &&
               lineText[i] != ' ' && lineText[i] != '\t' &&
               lineText[i] != '"' && lineText[i] != '\'' &&
               !IsIdentStart(lineText[i]) &&
               !std::isdigit(static_cast<unsigned char>(lineText[i]))) {
            if (!config.lineComment.empty() &&
                i + config.lineComment.size() <= len &&
                lineText.substr(i, config.lineComment.size()) == config.lineComment) {
                break;
            }
            if (!config.blockCommentStart.empty() &&
                i + config.blockCommentStart.size() <= len &&
                lineText.substr(i, config.blockCommentStart.size()) == config.blockCommentStart) {
                break;
            }
            ++i;
        }
        outSpans.push_back({
            static_cast<uint32_t>(start),
            static_cast<uint32_t>(i - start),
            config.highlightPunctuation ? pal.punctuation : pal.identifier
        });
    }
}

void TextArea::CppHighlighter(
    std::string_view lineText,
    size_t lineIndex,
    std::vector<TextSpan>& outSpans,
    const UiTheme& theme,
    void* /*userData*/
) {
    HighlightLine(lineText, lineIndex, SyntaxConfig::Cpp(), outSpans, theme, nullptr);
}

Response TextArea::Run(
    Context& ctx,
    std::string* stringValue,
    std::string_view displayFallback,
    const TextAreaOptions& opts,
    std::source_location loc
) {
    const auto scopedTheme = ctx.PushScopedTheme(opts.theme, opts.scale);

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool wasFocused = ctx.IsFocused(id) && !effectiveDisabled;

    ctx.RegisterFocusable(id, effectiveDisabled);

    Response res = ctx.Interact(id);
    if (effectiveDisabled) {
        res.clicked = false;
        res.hovered = false;
        res.pressed = false;
        res.focused = false;
    }

    const bool mouseJustPressed = res.hovered && ctx.IsMouseClicked() && !effectiveDisabled;
    if ((mouseJustPressed || res.clicked) && !effectiveDisabled) {
        ctx.RequestFocus(id);
    }

    const bool isFocused = ctx.IsFocused(id) && !effectiveDisabled;
    res.focused = isFocused;

    if (res.hovered && !opts.tooltip.empty()) {
        ctx.SetTooltip(opts.tooltip, res.rect);
    }

    TextAreaOptions optsCopy = opts;
    optsCopy.label = ctx.AllocText(opts.label);
    optsCopy.description = ctx.AllocText(opts.description);
    optsCopy.badge = ctx.AllocText(opts.badge);
    optsCopy.placeholder = ctx.AllocString(opts.placeholder);
    optsCopy.tooltip = ctx.AllocString(opts.tooltip);

    std::string_view valStr = (stringValue != nullptr) ? std::string_view(*stringValue) : displayFallback;

    auto& state = ctx.GetOrCreateState<TextAreaState>(id);
    const float reqZoom = std::clamp(opts.zoom, 0.5f, 3.0f);
    if (std::abs(state.lastOptsZoom - reqZoom) > 0.001f) {
        state.zoom = reqZoom;
        state.lastOptsZoom = reqZoom;
    }
    state.zoom = std::clamp(state.zoom, 0.5f, 3.0f);

    if (opts.allowZoom && !effectiveDisabled && (isFocused || res.hovered) && ctx.GetKeyModifiers().ctrl) {
        if (ctx.ConsumeKey(Key::Plus)) {
            state.zoom = std::clamp(state.zoom + 0.10f, 0.5f, 3.0f);
        } else if (ctx.ConsumeKey(Key::Minus)) {
            state.zoom = std::clamp(state.zoom - 0.10f, 0.5f, 3.0f);
        } else if (ctx.ConsumeKey(Key::Zero)) {
            state.zoom = 1.0f;
        }
    }

    auto applyZoomAnchor = [&](float newZoom) {
        const float clampedNew = std::clamp(newZoom, 0.5f, 3.0f);
        if (state.prevAppliedZoom > 0.001f && std::abs(clampedNew - state.prevAppliedZoom) > 0.0001f) {
            const float ratio = clampedNew / state.prevAppliedZoom;
            state.scrollY *= ratio;
            state.scrollX *= ratio;
        }
        state.zoom = clampedNew;
        state.prevAppliedZoom = clampedNew;
    };
    applyZoomAnchor(state.zoom);

    if (!ctx.IsMouseDown()) {
        state.draggingText = false;
        state.draggingWord = false;
        state.draggingVThumb = false;
        state.draggingHThumb = false;
    }

    const double now = ctx.GetCurrentTime();
    const bool copyFeedbackActive = (now < state.copyFeedbackUntil);
    if (copyFeedbackActive) {
        ctx.RequestRepaint(2);
    }

    const UiTheme& theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();
    const FontStyle textStyle = optsCopy.monospace ? FontStyle::Monospace : FontStyle::Regular;

    const Rect refBounds = (res.rect.w > 0.0f && res.rect.h > 0.0f)
        ? res.rect
        : Rect({0.0f, 0.0f}, TextArea::Measure(valStr, optsCopy, activeSize, canvas, theme));

    // Synchronize active edit buffer for both editable and readOnly (for selection & Ctrl+A/C)
    if (isFocused || ctx.HasActiveTextEdit(id)) {
        valStr = ctx.EnsureTextEdit(id, valStr, /*multiline=*/true, opts.readOnly);
    }

    std::vector<LogicalLine> logicalLines;
    SplitLogicalLines(valStr, logicalLines);
    std::vector<VisualLine> visualLines;
    EditorLayoutRects L = ComputeEditorLayout(
        refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
        state.zoom, state.scrollX, state.scrollY, copyFeedbackActive, visualLines
    );

    const Vec2 mousePos = ctx.GetMousePos();
    const bool copyHovered = opts.showCopyButton && res.hovered && L.copyBtnRect.w > 0.0f && L.copyBtnRect.Contains(mousePos);
    const bool zoomHovered = opts.allowZoom && res.hovered && L.zoomBtnRect.w > 0.0f && L.zoomBtnRect.Contains(mousePos);
    const bool vThumbHovered = L.needVScroll && res.hovered && L.vThumbRect.Contains(mousePos);
    const bool hThumbHovered = L.needHScroll && res.hovered && L.hThumbRect.Contains(mousePos);
    const bool vTrackHovered = L.needVScroll && res.hovered && L.vTrackRect.Contains(mousePos);
    const bool hTrackHovered = L.needHScroll && res.hovered && L.hTrackRect.Contains(mousePos);

    // Handle mouse wheel: Ctrl+Wheel -> Zoom, Shift+Wheel / wheel.x -> Horizontal scroll, Wheel -> Vertical scroll
    if (res.hovered && !effectiveDisabled) {
        const Vec2 wheel = ctx.GetMouseWheel();
        if (std::abs(wheel.y) > 0.001f || std::abs(wheel.x) > 0.001f) {
            if (opts.allowZoom && ctx.GetKeyModifiers().ctrl && std::abs(wheel.y) > 0.001f) {
                const float prevZoom = state.zoom;
                applyZoomAnchor(state.zoom + (wheel.y > 0.0f ? 0.10f : -0.10f));
                if (std::abs(state.zoom - prevZoom) > 0.001f) {
                    L = ComputeEditorLayout(
                        refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
                        state.zoom, state.scrollX, state.scrollY, copyFeedbackActive, visualLines
                    );
                }
                ctx.ConsumeMouseWheel();
            } else if (!opts.wordWrap && (ctx.GetKeyModifiers().shift || std::abs(wheel.x) > std::abs(wheel.y)) && L.maxScrollX > 0.0f) {
                const float delta = (std::abs(wheel.x) > 0.001f) ? wheel.x : wheel.y;
                state.scrollX = std::clamp(state.scrollX - delta * L.lineHeight * 2.5f, 0.0f, L.maxScrollX);
                ctx.ConsumeMouseWheel();
            } else if (!opts.autoHeight && L.maxScrollY > 0.0f && std::abs(wheel.y) > 0.001f) {
                state.scrollY = std::clamp(state.scrollY - wheel.y * L.lineHeight * 2.5f, 0.0f, L.maxScrollY);
                ctx.ConsumeMouseWheel();
            }
        }
    }

    // Handle LMB press on overlay buttons, scrollbars, or text viewport
    bool consumedByChrome = false;
    if (mouseJustPressed) {
        if (copyHovered) {
            consumedByChrome = true;
            std::string_view toCopy = valStr;
            if (state.selectionEnd > state.selectionStart && state.selectionEnd <= valStr.size()) {
                toCopy = valStr.substr(state.selectionStart, state.selectionEnd - state.selectionStart);
            }
            ctx.SetClipboardText(toCopy);
            state.copyFeedbackUntil = now + 1.5;
            ctx.RequestRepaint(3);
        } else if (zoomHovered) {
            consumedByChrome = true;
            applyZoomAnchor(1.0f);
            L = ComputeEditorLayout(
                refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
                state.zoom, state.scrollX, state.scrollY, copyFeedbackActive, visualLines
            );
        } else if (vThumbHovered) {
            consumedByChrome = true;
            state.draggingVThumb = true;
            state.dragGrabOffset = mousePos.y - L.vThumbRect.y;
        } else if (vTrackHovered) {
            consumedByChrome = true;
            const float halfThumb = L.vThumbRect.h * 0.5f;
            const float maxTravel = std::max(1.0f, L.vTrackRect.h - L.vThumbRect.h);
            const float t = std::clamp((mousePos.y - L.vTrackRect.y - halfThumb) / maxTravel, 0.0f, 1.0f);
            state.scrollY = t * L.maxScrollY;
            state.draggingVThumb = true;
            state.dragGrabOffset = halfThumb;
        } else if (hThumbHovered) {
            consumedByChrome = true;
            state.draggingHThumb = true;
            state.dragGrabOffset = mousePos.x - L.hThumbRect.x;
        } else if (hTrackHovered) {
            consumedByChrome = true;
            const float halfThumb = L.hThumbRect.w * 0.5f;
            const float maxTravel = std::max(1.0f, L.hTrackRect.w - L.hThumbRect.w);
            const float t = std::clamp((mousePos.x - L.hTrackRect.x - halfThumb) / maxTravel, 0.0f, 1.0f);
            state.scrollX = t * L.maxScrollX;
            state.draggingHThumb = true;
            state.dragGrabOffset = halfThumb;
        } else if (L.editorBox.Contains(mousePos)) {
            state.draggingText = true;
        }
    }

    if (state.draggingVThumb && ctx.IsMouseDown() && L.needVScroll) {
        const float maxTravel = std::max(1.0f, L.vTrackRect.h - L.vThumbRect.h);
        const float t = std::clamp((mousePos.y - L.vTrackRect.y - state.dragGrabOffset) / maxTravel, 0.0f, 1.0f);
        state.scrollY = t * L.maxScrollY;
    }
    if (state.draggingHThumb && ctx.IsMouseDown() && L.needHScroll) {
        const float maxTravel = std::max(1.0f, L.hTrackRect.w - L.hThumbRect.w);
        const float t = std::clamp((mousePos.x - L.hTrackRect.x - state.dragGrabOffset) / maxTravel, 0.0f, 1.0f);
        state.scrollX = t * L.maxScrollX;
    }

    state.scrollY = std::clamp(state.scrollY, 0.0f, L.maxScrollY);
    state.scrollX = std::clamp(state.scrollX, 0.0f, L.maxScrollX);

    size_t cursor = state.cursor;
    size_t selAnchor = state.cursor;
    std::string_view displayText = valStr;

    if (isFocused || ctx.HasActiveTextEdit(id)) {
        // 2D Mouse hit-testing for cursor & selection
        if (!consumedByChrome && (mouseJustPressed || (state.draggingText && ctx.IsMouseDown()))) {
            const float relY = (mousePos.y - L.textViewport.y) + state.scrollY;
            const float relX = (mousePos.x - L.textViewport.x) + state.scrollX;

            const int rawVisIdx = static_cast<int>(std::floor(relY / std::max(1.0f, L.lineHeight)));
            const size_t visIdx = static_cast<size_t>(std::clamp(rawVisIdx, 0, static_cast<int>(visualLines.size()) - 1));
            const VisualLine& vl = visualLines[visIdx];
            const std::string_view subStr = valStr.substr(vl.startOffset, vl.length);
            const size_t colInSub = HitTestColumnInLine(canvas, subStr, relX, L.fontSize, textStyle);
            const size_t hitIdx = vl.startOffset + colInSub;

            if (mouseJustPressed) {
                const float dist = (mousePos - state.lastClickPos).Length();
                if (wasFocused && (now - state.lastClickTime) <= 0.35 && dist <= 6.0f) {
                    state.clickCount += 1;
                } else {
                    state.clickCount = 1;
                }
                state.lastClickTime = now;
                state.lastClickPos = mousePos;

                if (state.clickCount == 1) {
                    state.draggingWord = false;
                    if (ctx.GetKeyModifiers().shift && wasFocused) {
                        const size_t anchor = ctx.GetTextEditAnchor(id);
                        ctx.SetTextEditCursor(id, hitIdx, anchor);
                    } else {
                        ctx.SetTextEditCursor(id, hitIdx, hitIdx);
                    }
                } else if (state.clickCount == 2) {
                    size_t wStart = 0;
                    size_t wEnd = 0;
                    TextUtil::SelectWordAt(valStr, hitIdx, wStart, wEnd);
                    state.draggingWord = true;
                    state.wordDragStart = wStart;
                    state.wordDragEnd = wEnd;
                    ctx.SetTextEditCursor(id, wEnd, wStart);
                } else {
                    // Triple-click selects the entire logical line
                    state.draggingWord = false;
                    const LogicalLine& ll = logicalLines[vl.logicalLine];
                    ctx.SetTextEditCursor(id, ll.startOffset + ll.length, ll.startOffset);
                }
            } else if (state.draggingText) {
                // Auto-scroll when dragging near viewport top/bottom/left/right edges
                if (mousePos.y < L.textViewport.y && state.scrollY > 0.0f) {
                    state.scrollY = std::max(0.0f, state.scrollY - L.lineHeight * 0.35f);
                } else if (mousePos.y > L.textViewport.Bottom() && state.scrollY < L.maxScrollY) {
                    state.scrollY = std::min(L.maxScrollY, state.scrollY + L.lineHeight * 0.35f);
                }
                if (!opts.wordWrap) {
                    if (mousePos.x < L.textViewport.x && state.scrollX > 0.0f) {
                        state.scrollX = std::max(0.0f, state.scrollX - L.lineHeight * 0.5f);
                    } else if (mousePos.x > L.textViewport.Right() && state.scrollX < L.maxScrollX) {
                        state.scrollX = std::min(L.maxScrollX, state.scrollX + L.lineHeight * 0.5f);
                    }
                }

                if (state.clickCount >= 3) {
                    const LogicalLine& ll = logicalLines[vl.logicalLine];
                    ctx.SetTextEditCursor(id, ll.startOffset + ll.length, ll.startOffset);
                } else if (state.draggingWord) {
                    size_t wStart = 0;
                    size_t wEnd = 0;
                    TextUtil::SelectWordAt(valStr, hitIdx, wStart, wEnd);
                    if (hitIdx < state.wordDragStart) {
                        ctx.SetTextEditCursor(id, wStart, state.wordDragEnd);
                    } else {
                        ctx.SetTextEditCursor(id, wEnd, state.wordDragStart);
                    }
                } else {
                    const size_t anchor = ctx.GetTextEditAnchor(id);
                    ctx.SetTextEditCursor(id, hitIdx, anchor);
                }
            }
        }

        bool changed = false;
        bool submitted = false;
        displayText = ctx.GetOrStartTextEdit(id, valStr, cursor, selAnchor, changed, submitted, /*multiline=*/true, opts.readOnly);

        if (changed && !opts.readOnly) {
            if (stringValue != nullptr) {
                *stringValue = displayText;
            }
            res.changed = true;
            SplitLogicalLines(displayText, logicalLines);
            L = ComputeEditorLayout(
                refBounds, displayText, logicalLines, optsCopy, activeSize, canvas, theme,
                state.zoom, state.scrollX, state.scrollY, copyFeedbackActive, visualLines
            );
        }

        state.cursor = cursor;
        state.selectionStart = std::min(cursor, selAnchor);
        state.selectionEnd = std::max(cursor, selAnchor);

        // Auto-scroll to keep active cursor visible when cursor or text changes
        if (cursor != state.prevCursor || displayText.size() != state.prevTextSize) {
            const size_t curVisRow = FindVisualLineForOffset(visualLines, cursor);
            const VisualLine& cvl = visualLines[curVisRow];
            const float curTop = static_cast<float>(curVisRow) * L.lineHeight;
            const float curBot = curTop + L.lineHeight;

            if (curTop < state.scrollY) {
                state.scrollY = curTop;
            } else if (curBot > state.scrollY + L.textViewport.h && L.textViewport.h > 0.0f) {
                state.scrollY = curBot - L.textViewport.h;
            }

            if (!opts.wordWrap && L.textViewport.w > 0.0f) {
                const size_t colOffset = (cursor >= cvl.startOffset) ? std::min(cursor - cvl.startOffset, cvl.length) : 0;
                const std::string_view prefix = displayText.substr(cvl.startOffset, colOffset);
                const float curX = MeasureSegmentWidth(canvas, prefix, L.fontSize, textStyle);
                const float margin = theme.Scale(16.0f);
                if (curX < state.scrollX + margin) {
                    state.scrollX = std::max(0.0f, curX - margin);
                } else if (curX > state.scrollX + L.textViewport.w - margin) {
                    state.scrollX = std::min(L.maxScrollX, curX - L.textViewport.w + margin);
                }
            }

            state.scrollY = std::clamp(state.scrollY, 0.0f, L.maxScrollY);
            state.scrollX = std::clamp(state.scrollX, 0.0f, L.maxScrollX);
            state.prevCursor = cursor;
            state.prevTextSize = displayText.size();
        }
    } else {
        state.selectionStart = state.cursor;
        state.selectionEnd = state.cursor;
    }

    const bool cursorVisible = isFocused && ctx.IsCursorVisible(opts.cursorMode);

    Node* node = ctx.CreateNode(NodeType::TextArea, id);
    auto* data = ctx.AllocPayload<TextAreaData>();
    if (data) {
        data->opts = optsCopy;
        data->text = ctx.AllocString(displayText);
        data->cursor = cursor;
        data->selectionStart = state.selectionStart;
        data->selectionEnd = state.selectionEnd;
        data->cursorVisible = cursorVisible;
        data->scrollX = state.scrollX;
        data->scrollY = state.scrollY;
        data->zoom = state.zoom;
        data->copyHovered = copyHovered;
        data->copyFeedbackActive = copyFeedbackActive;
        data->zoomBadgeHovered = zoomHovered;
        data->vThumbHovered = vThumbHovered;
        data->vThumbDragging = state.draggingVThumb;
        data->hThumbHovered = hThumbHovered;
        data->hThumbDragging = state.draggingHThumb;
    }
    node->SetPayload(data);
    node->visualHash = detail::HashArgs(
        data ? data->text : displayText,
        optsCopy.label.text,
        optsCopy.description.text,
        optsCopy.badge.text,
        optsCopy.placeholder,
        static_cast<uint8_t>(optsCopy.variant),
        static_cast<uint8_t>(optsCopy.layout),
        static_cast<uint8_t>(optsCopy.inlineOrder),
        static_cast<uint8_t>(optsCopy.elementAlign),
        static_cast<uint8_t>(optsCopy.stretch),
        optsCopy.width.value_or(-1.0f),
        optsCopy.divider,
        static_cast<uint8_t>(optsCopy.dividerStyle),
        optsCopy.readOnly,
        optsCopy.showLineNumbers,
        optsCopy.showCopyButton,
        optsCopy.monospace,
        optsCopy.wordWrap,
        optsCopy.autoHeight,
        optsCopy.rows.value_or(0u),
        optsCopy.fixedHeight.value_or(-1.0f),
        optsCopy.borderRadius.value_or(-1.0f),
        optsCopy.syntax.has_value() ? optsCopy.syntax->enabled : false,
        cursor,
        state.selectionStart,
        state.selectionEnd,
        cursorVisible,
        state.scrollX,
        state.scrollY,
        state.zoom,
        copyHovered,
        copyFeedbackActive,
        zoomHovered,
        vThumbHovered,
        state.draggingVThumb,
        hThumbHovered,
        state.draggingHThumb,
        res.hovered,
        res.pressed,
        isFocused,
        effectiveDisabled
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* c, const UiTheme& t) -> Vec2 {
        const auto* d = n->As<TextAreaData>();
        if (!d) return {0.0f, 0.0f};
        TextAreaOptions mOpts = d->opts;
        mOpts.zoom = d->zoom;
        return TextArea::Measure(d->text, mOpts, n->uiSize, c, t, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& c, const UiTheme& t) {
        const auto* d = n->As<TextAreaData>();
        if (d) {
            TextArea::Render(
                c,
                n->computedRect,
                d->text,
                d->opts,
                n->uiSize,
                n->hovered,
                n->focused,
                d->cursor,
                n->disabled,
                d->cursorVisible,
                d->scrollY,
                t,
                d
            );
        }
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = res.hovered;
    node->pressed = res.pressed;
    node->focused = isFocused;
    node->theme = scopedTheme;
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = opts.heightMode;
    if (opts.fixedHeight.has_value() && !opts.label.HasContentOrReserve() && !opts.description.HasContentOrReserve() && !opts.divider) {
        node->heightMode = SizeMode::Fixed;
        node->fixedHeight = theme.Scale(*opts.fixedHeight);
    }

    ctx.AdaptChildToContainer(node);
    if (scopedTheme != nullptr) {
        ctx.PopTheme();
    }

    return res;
}

Vec2 TextArea::Measure(
    std::string_view text,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) {
    const Vec2 boxDims = MeasureEditorBox(text, opts, size, canvas, theme, opts.zoom);
    const ElementSpec spec = MakeTextAreaElementSpec(opts, size, opts.disabled, theme);
    return Element::Measure(boxDims, spec, canvas, theme, availableWidth);
}

Vec2 TextArea::Measure(
    std::string_view text,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    return Measure(text, opts, size, canvas, UiTheme::Dark(), 0.0f);
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
    const UiTheme& theme,
    const TextAreaData* extData
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const ThemePalette& pal = theme.palette;
    const SyntaxPalette& synPal = opts.syntaxPalette.has_value()
        ? *opts.syntaxPalette
        : ((opts.syntax.has_value() && opts.syntax->palette.has_value()) ? *opts.syntax->palette : theme.syntax);
    const ScrollbarMetrics& sm = theme.GetScrollbar(size);
    const FontStyle textStyle = opts.monospace ? FontStyle::Monospace : FontStyle::Regular;

    const float effZoom = std::clamp(extData ? extData->zoom : opts.zoom, 0.5f, 3.0f);
    const float effScrollX = extData ? extData->scrollX : 0.0f;
    const float effScrollY = extData ? extData->scrollY : scrollY;
    const size_t selStart = extData ? extData->selectionStart : cursor;
    const size_t selEnd = extData ? extData->selectionEnd : cursor;
    const bool hasSelection = (selEnd > selStart);

    std::vector<LogicalLine> logicalLines;
    SplitLogicalLines(text, logicalLines);
    std::vector<VisualLine> visualLines;
    const EditorLayoutRects L = ComputeEditorLayout(
        bounds, text, logicalLines, opts, size, &canvas, theme,
        effZoom, effScrollX, effScrollY, extData ? extData->copyFeedbackActive : false, visualLines
    );

    // 1. Render external Element header, icon, badge, description, and divider
    const ElementSpec spec = MakeTextAreaElementSpec(opts, size, disabled, theme);
    Element::RenderText(canvas, L.elemGeom, spec, theme);

    const Rect editorBox = L.editorBox;
    if (editorBox.w <= 0.0f || editorBox.h <= 0.0f) return;

    // 2. Render editor container frame
    const float r = theme.ResolveRadius(opts.borderRadius, size);
    const Color bgCol = disabled ? pal.inputBackground.WithAlphaF(0.45f) : pal.inputBackground;
    Color borderCol = pal.inputBorder;
    float strokeW = theme.Scale(1.0f);
    if (!disabled) {
        if (focused) {
            borderCol = (opts.variant == UiVariant::Default) ? pal.focusRing : theme.GetVariant(opts.variant).bg;
            strokeW = theme.Scale(1.5f);
        } else if (hovered) {
            borderCol = pal.borderHover;
        }
    } else {
        borderCol = borderCol.WithAlphaF(0.45f);
    }

    canvas.DrawRoundRect(editorBox, r, bgCol, borderCol, strokeW);

    const Rect innerBounds = L.innerBounds;
    if (innerBounds.w <= 2.0f || innerBounds.h <= 2.0f) return;

    canvas.PushClip(innerBounds);

    // 3. Line numbers gutter background & separator
    if (opts.showLineNumbers && L.gutterWidth > 0.0f) {
        const Color gutterBg = (theme.mode == ThemeMode::Dark)
            ? Color(15, 23, 42, 190)
            : Color(241, 245, 249, 210);
        canvas.DrawRect(L.gutterRect, gutterBg);
        canvas.DrawLine(
            Vec2{L.gutterRect.Right(), L.gutterRect.y},
            Vec2{L.gutterRect.Right(), L.gutterRect.Bottom()},
            pal.borderSubtle,
            1.0f
        );
    }

    const Rect textViewport = L.textViewport;
    const float clampedScrollY = std::clamp(effScrollY, 0.0f, L.maxScrollY);
    const float clampedScrollX = std::clamp(effScrollX, 0.0f, L.maxScrollX);

    // Determine active cursor visual row & logical line
    const size_t safeCursor = std::min(cursor, text.size());
    const size_t activeVisRow = FindVisualLineForOffset(visualLines, safeCursor);
    const size_t activeLogicalLine = visualLines[activeVisRow].logicalLine;

    // 4. Placeholder when empty
    if (text.empty() && !opts.placeholder.empty() && !focused) {
        canvas.DrawTextBox(
            opts.placeholder,
            Rect(textViewport.x, textViewport.y, textViewport.w, L.lineHeight),
            L.fontSize,
            pal.textMuted,
            0.0f,
            0.5f
        );
    }

    // 5. Virtualized visible lines loop
    const size_t totalVisLines = visualLines.size();
    const size_t firstVisLine = (L.lineHeight > 0.0f)
        ? static_cast<size_t>(std::max(0.0f, std::floor(clampedScrollY / L.lineHeight)))
        : 0;
    const size_t maxVisCount = (L.lineHeight > 0.0f)
        ? (static_cast<size_t>(std::ceil(textViewport.h / L.lineHeight)) + 2)
        : totalVisLines;
    const size_t lastVisLine = std::min(totalVisLines, firstVisLine + maxVisCount);

    std::vector<TextSpan> syntaxSpans;
    syntaxSpans.reserve(32);
    size_t cachedLogicalLine = static_cast<size_t>(-1);

    const bool useCallbackSyntax = (opts.syntaxHighlighter != nullptr);
    const bool useConfigSyntax = (!useCallbackSyntax && opts.syntax.has_value() && opts.syntax->enabled);
    const bool anySyntax = (useCallbackSyntax || useConfigSyntax) && !disabled;

    for (size_t vIdx = firstVisLine; vIdx < lastVisLine; ++vIdx) {
        const VisualLine& vl = visualLines[vIdx];
        const float lineY = textViewport.y + static_cast<float>(vIdx) * L.lineHeight - clampedScrollY;
        if (lineY + L.lineHeight < innerBounds.y || lineY > innerBounds.Bottom()) {
            continue;
        }

        // Active line subtle highlight when focused and no multi-char selection
        if (focused && !disabled && vIdx == activeVisRow && !hasSelection) {
            canvas.DrawRect(
                Rect(innerBounds.x + L.gutterWidth, lineY, innerBounds.w - L.gutterWidth, L.lineHeight),
                synPal.lineHighlight
            );
        }

        const float textY = lineY + L.metrics.textOffsetY;

        // Draw line number in gutter locked to the exact same baseline (textY) and font style as code
        if (opts.showLineNumbers && L.gutterWidth > 0.0f && vl.isFirstSubLine) {
            char numBuf[16]{};
            const int nLen = std::snprintf(numBuf, sizeof(numBuf), "%zu", vl.logicalLine + 1);
            if (nLen > 0) {
                const std::string_view numStr(numBuf, static_cast<size_t>(nLen));
                const bool isCurrentLine = focused && !disabled && (vl.logicalLine == activeLogicalLine);
                const Color numCol = disabled
                    ? pal.textDisabled
                    : (isCurrentLine ? pal.textPrimary : pal.textMuted);
                const float numW = MeasureChunkNoTabs(&canvas, numStr, L.fontSize, FontStyle::Monospace);
                const float gutterPadRight = L.metrics.spaceAdvance * 1.15f;
                const float numX = innerBounds.x + std::max(2.0f, L.gutterWidth - gutterPadRight - numW);
                canvas.DrawTextStyled(numStr, Vec2{numX, textY}, L.fontSize, numCol, FontStyle::Monospace);
            }
        }

        // Clip and render code/text within textViewport
        canvas.PushClip(Rect(textViewport.x, innerBounds.y, textViewport.w, innerBounds.h));

        const float startX = textViewport.x - clampedScrollX;
        const size_t vlEndOffset = vl.startOffset + vl.length;

        // Selection highlight rectangle for this visual line (seamless full lineHeight)
        if (hasSelection && focused && !disabled) {
            const size_t s0 = std::max(selStart, vl.startOffset);
            const size_t s1 = std::min(selEnd, vlEndOffset);
            if (s1 > s0 || (selStart <= vl.startOffset && selEnd > vlEndOffset)) {
                const std::string_view preSel = text.substr(vl.startOffset, s0 - vl.startOffset);
                const std::string_view midSel = text.substr(s0, s1 - s0);
                const float selX0 = startX + MeasureSegmentWidth(&canvas, preSel, L.fontSize, textStyle);
                float selW = MeasureSegmentWidth(&canvas, midSel, L.fontSize, textStyle);
                if (selEnd > vlEndOffset) {
                    selW += L.metrics.spaceAdvance;
                }
                if (selW > 0.0f) {
                    canvas.DrawRect(Rect(selX0, lineY, selW, L.lineHeight), synPal.selectionBg);
                }
            }
        }

        // Render text or syntax spans as contiguous tokens (splitting only on '\t' tabs)
        if (vl.length > 0) {
            const LogicalLine& ll = logicalLines[vl.logicalLine];
            const std::string_view fullLineStr = text.substr(ll.startOffset, ll.length);
            const std::string_view subLineStr = text.substr(vl.startOffset, vl.length);

            float curX = startX;
            auto drawPiece = [&](std::string_view piece, Color col) {
                size_t p = 0;
                while (p < piece.size()) {
                    if (piece[p] == '\t') {
                        size_t tabEnd = p + 1;
                        while (tabEnd < piece.size() && piece[tabEnd] == '\t') {
                            ++tabEnd;
                        }
                        curX += static_cast<float>(tabEnd - p) * L.metrics.tabAdvance;
                        p = tabEnd;
                    } else {
                        size_t nextTab = piece.find('\t', p);
                        if (nextTab == std::string_view::npos) {
                            nextTab = piece.size();
                        }
                        const std::string_view seg = piece.substr(p, nextTab - p);

                        // Advance past leading spaces so first glyph in DrawTextStyled is always visible
                        size_t firstNonSpace = 0;
                        while (firstNonSpace < seg.size() && seg[firstNonSpace] == ' ') {
                            ++firstNonSpace;
                        }
                        if (firstNonSpace > 0) {
                            curX += static_cast<float>(firstNonSpace) * L.metrics.spaceAdvance;
                        }

                        if (firstNonSpace < seg.size()) {
                            size_t lastNonSpace = seg.size();
                            while (lastNonSpace > firstNonSpace && seg[lastNonSpace - 1] == ' ') {
                                --lastNonSpace;
                            }
                            const std::string_view body = seg.substr(firstNonSpace, lastNonSpace - firstNonSpace);
                            const float bodyW = MeasureChunkNoTabs(&canvas, body, L.fontSize, textStyle);
                            if (curX + bodyW >= textViewport.x - 4.0f && curX <= textViewport.Right() + 4.0f) {
                                canvas.DrawTextStyled(body, Vec2{curX, textY}, L.fontSize, col, textStyle);
                            }
                            curX += bodyW;

                            const size_t trailingSpaces = seg.size() - lastNonSpace;
                            if (trailingSpaces > 0) {
                                curX += static_cast<float>(trailingSpaces) * L.metrics.spaceAdvance;
                            }
                        }
                        p = nextTab;
                    }
                }
            };

            if (anySyntax) {
                if (cachedLogicalLine != vl.logicalLine) {
                    syntaxSpans.clear();
                    if (useCallbackSyntax) {
                        opts.syntaxHighlighter(fullLineStr, vl.logicalLine, syntaxSpans, theme, opts.highlighterUserData);
                    } else if (useConfigSyntax) {
                        HighlightLine(
                            fullLineStr,
                            vl.logicalLine,
                            *opts.syntax,
                            syntaxSpans,
                            theme,
                            opts.syntaxPalette.has_value() ? &*opts.syntaxPalette : nullptr
                        );
                    }
                    cachedLogicalLine = vl.logicalLine;
                }

                const size_t subStartCol = vl.startCol;
                const size_t subEndCol = vl.startCol + vl.length;
                size_t curCol = subStartCol;

                for (const TextSpan& sp : syntaxSpans) {
                    const size_t spStart = sp.startCol;
                    const size_t spEnd = sp.startCol + sp.length;
                    if (spEnd <= subStartCol) continue;
                    if (spStart >= subEndCol) break;

                    const size_t clipStart = std::max(spStart, subStartCol);
                    const size_t clipEnd = std::min(spEnd, subEndCol);

                    if (clipStart > curCol) {
                        drawPiece(fullLineStr.substr(curCol, clipStart - curCol), synPal.identifier);
                    }
                    if (clipEnd > clipStart) {
                        drawPiece(fullLineStr.substr(clipStart, clipEnd - clipStart), sp.color);
                        curCol = clipEnd;
                    }
                }
                if (curCol < subEndCol) {
                    drawPiece(fullLineStr.substr(curCol, subEndCol - curCol), synPal.identifier);
                }
            } else {
                const Color textCol = disabled ? pal.textDisabled : pal.textPrimary;
                drawPiece(subLineStr, textCol);
            }
        }

        // Render active cursor caret centered vertically on the line
        if (focused && cursorVisible && !disabled && vIdx == activeVisRow) {
            const size_t colInSub = (safeCursor >= vl.startOffset)
                ? std::min(safeCursor - vl.startOffset, vl.length)
                : 0;
            const std::string_view beforeCursor = text.substr(vl.startOffset, colInSub);
            const float caretX = startX + MeasureSegmentWidth(&canvas, beforeCursor, L.fontSize, textStyle);
            if (caretX >= textViewport.x - 2.0f && caretX <= textViewport.Right() + 2.0f) {
                const float caretH = std::max(L.glyphBoxH, L.lineHeight - theme.Scale(2.0f));
                const float caretY = lineY + (L.lineHeight - caretH) * 0.5f;
                canvas.DrawRect(
                    Rect(caretX, caretY, std::max(1.5f, theme.Scale(1.8f)), caretH),
                    pal.caret
                );
            }
        }

        canvas.PopClip();
    }

    // 6. Render vertical & horizontal scrollbars
    if (L.needVScroll && L.vTrackRect.h > 0.0f) {
        canvas.DrawRoundRect(L.vTrackRect, sm.cornerRadius, pal.surfaceHover.WithAlphaF(0.25f));
        const bool vActive = extData && (extData->vThumbHovered || extData->vThumbDragging);
        const Color thumbCol = vActive ? pal.controlKnobHover : pal.controlKnob.WithAlphaF(0.65f);
        canvas.DrawRoundRect(L.vThumbRect, sm.cornerRadius, thumbCol);
    }

    if (L.needHScroll && L.hTrackRect.w > 0.0f) {
        canvas.DrawRoundRect(L.hTrackRect, sm.cornerRadius, pal.surfaceHover.WithAlphaF(0.25f));
        const bool hActive = extData && (extData->hThumbHovered || extData->hThumbDragging);
        const Color thumbCol = hActive ? pal.controlKnobHover : pal.controlKnob.WithAlphaF(0.65f);
        canvas.DrawRoundRect(L.hThumbRect, sm.cornerRadius, thumbCol);
    }

    // 7. Render top-right quick overlays: Copy button & Zoom % reset badge
    const float badgeFs = theme.GetFontSize(TextRole::LabelSmall, size);
    const float btnRad = theme.GetRadius(UiSize::Sm);

    if (L.copyBtnRect.w > 0.0f) {
        const bool copyHov = extData && extData->copyHovered;
        const bool copied = extData && extData->copyFeedbackActive;
        const Color btnBg = copied
            ? theme.GetVariant(UiVariant::Success).bg.WithAlphaF(0.22f)
            : (copyHov ? pal.surfaceActive : pal.surface.WithAlphaF(0.88f));
        const Color btnBorder = copied
            ? theme.GetVariant(UiVariant::Success).bg
            : (copyHov ? pal.borderHover : pal.borderSubtle);
        const Color btnFg = copied
            ? theme.GetVariant(UiVariant::Success).bg
            : (copyHov ? pal.textPrimary : pal.textSecondary);

        canvas.DrawRoundRect(L.copyBtnRect, btnRad, btnBg, btnBorder, 1.0f);
        const float iconSz = theme.Scale(11.0f);
        const Rect iconRect(
            L.copyBtnRect.x + theme.Scale(6.0f),
            L.copyBtnRect.y + (L.copyBtnRect.h - iconSz) * 0.5f,
            iconSz,
            iconSz
        );
        Icon(copied ? IconId::Check : IconId::Copy).Draw(canvas, iconRect, btnFg);
        const std::string_view copyLbl = copied ? "Copied" : "Copy";
        canvas.DrawTextBox(
            copyLbl,
            Rect(iconRect.Right() + theme.Scale(4.0f), L.copyBtnRect.y, L.copyBtnRect.Right() - iconRect.Right() - theme.Scale(8.0f), L.copyBtnRect.h),
            badgeFs,
            btnFg,
            0.5f,
            0.5f
        );
    }

    if (L.zoomBtnRect.w > 0.0f) {
        const bool zHov = extData && extData->zoomBadgeHovered;
        const Color zBg = zHov ? pal.surfaceActive : pal.surface.WithAlphaF(0.88f);
        const Color zBorder = zHov ? pal.accent : pal.borderSubtle;
        const Color zFg = zHov ? pal.accent : pal.textSecondary;
        canvas.DrawRoundRect(L.zoomBtnRect, btnRad, zBg, zBorder, 1.0f);
        char zBuf[16]{};
        std::snprintf(zBuf, sizeof(zBuf), "%d%%", static_cast<int>(std::lround(effZoom * 100.0f)));
        canvas.DrawTextBox(zBuf, L.zoomBtnRect, badgeFs, zFg, 0.5f, 0.5f);
    }

    canvas.PopClip();
}

} // namespace guikit
