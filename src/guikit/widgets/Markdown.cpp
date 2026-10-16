#include "guikit/widgets/Markdown.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Element.hpp"
#include "guikit/core/Node.hpp"
#include "guikit/core/TextUtil.hpp"
#include "guikit/graphics/Icon.hpp"
#include "guikit/widgets/Image.hpp"
#include "guikit/widgets/TextArea.hpp"

namespace guikit {

namespace {

enum class InlineKind : uint8_t {
    Text = 0,
    Code,
    Link
};

struct InlineSpan {
    InlineKind kind{InlineKind::Text};
    FontStyle style{FontStyle::Regular};
    bool strikethrough{false};
    std::string text{};
    std::string url{};
};

enum class AlertKind : uint8_t {
    None = 0,
    Note,
    Tip,
    Important,
    Warning,
    Caution
};

enum class BlockKind : uint8_t {
    Heading = 0,
    Paragraph,
    Blockquote,
    Alert,
    CodeBlock,
    ListItem,
    ImageBlock,
    Table,
    Divider
};

struct TableCell {
    std::vector<InlineSpan> spans{};
};

struct MarkdownBlock {
    BlockKind kind{BlockKind::Paragraph};
    uint8_t headingLevel{1};            // 1..6
    uint8_t indentLevel{0};             // 0..3 for nested lists
    bool isOrderedList{false};
    uint16_t listNumber{1};
    int8_t taskState{-1};               // -1 = normal bullet, 0 = [ ], 1 = [x]
    AlertKind alertKind{AlertKind::None};
    std::string alertTitle{};
    std::string codeLang{};
    std::string rawText{};              // Code block content or Image path
    std::string imageAlt{};             // Image caption / alt
    std::vector<InlineSpan> spans{};
    std::vector<Align> tableAligns{};
    std::vector<std::vector<TableCell>> tableRows{}; // row 0 = header
    int codeBlockIndex{-1};
};

struct ParsedMarkdownDoc {
    uint64_t sourceHash{0};
    std::vector<MarkdownBlock> blocks{};
};

std::string_view TrimView(std::string_view sv) noexcept {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r')) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r')) {
        sv.remove_suffix(1);
    }
    return sv;
}

std::vector<InlineSpan> ParseInlineSpans(std::string_view line) {
    std::vector<InlineSpan> out;
    size_t i = 0;
    const size_t n = line.size();

    std::string cur;
    auto flushRegular = [&](FontStyle st = FontStyle::Regular, bool strike = false) {
        if (!cur.empty()) {
            out.push_back(InlineSpan{
                .kind = InlineKind::Text,
                .style = st,
                .strikethrough = strike,
                .text = std::move(cur),
                .url = {}
            });
            cur.clear();
        }
    };

    while (i < n) {
        // Inline code: `code`
        if (line[i] == '`') {
            const size_t endTick = line.find('`', i + 1);
            if (endTick != std::string_view::npos && endTick > i + 1) {
                flushRegular();
                out.push_back(InlineSpan{
                    .kind = InlineKind::Code,
                    .style = FontStyle::Monospace,
                    .strikethrough = false,
                    .text = std::string(line.substr(i + 1, endTick - i - 1)),
                    .url = {}
                });
                i = endTick + 1;
                continue;
            }
        }

        // Link: [label](url)
        if (line[i] == '[') {
            const size_t closeBr = line.find(']', i + 1);
            if (closeBr != std::string_view::npos && closeBr + 1 < n && line[closeBr + 1] == '(') {
                const size_t closePar = line.find(')', closeBr + 2);
                if (closePar != std::string_view::npos) {
                    flushRegular();
                    out.push_back(InlineSpan{
                        .kind = InlineKind::Link,
                        .style = FontStyle::Regular,
                        .strikethrough = false,
                        .text = std::string(line.substr(i + 1, closeBr - i - 1)),
                        .url = std::string(TrimView(line.substr(closeBr + 2, closePar - closeBr - 2)))
                    });
                    i = closePar + 1;
                    continue;
                }
            }
        }

        // Bold + Italic: ***text***
        if (i + 2 < n && line.substr(i, 3) == "***") {
            const size_t closePos = line.find("***", i + 3);
            if (closePos != std::string_view::npos && closePos > i + 3) {
                flushRegular();
                cur = std::string(line.substr(i + 3, closePos - i - 3));
                flushRegular(FontStyle::BoldItalic, false);
                i = closePos + 3;
                continue;
            }
        }

        // Bold: **text**
        if (i + 1 < n && line.substr(i, 2) == "**") {
            const size_t closePos = line.find("**", i + 2);
            if (closePos != std::string_view::npos && closePos > i + 2) {
                flushRegular();
                cur = std::string(line.substr(i + 2, closePos - i - 2));
                flushRegular(FontStyle::Bold, false);
                i = closePos + 2;
                continue;
            }
        }

        // Strikethrough: ~~text~~
        if (i + 1 < n && line.substr(i, 2) == "~~") {
            const size_t closePos = line.find("~~", i + 2);
            if (closePos != std::string_view::npos && closePos > i + 2) {
                flushRegular();
                cur = std::string(line.substr(i + 2, closePos - i - 2));
                flushRegular(FontStyle::Regular, true);
                i = closePos + 2;
                continue;
            }
        }

        // Italic: *text*
        if (line[i] == '*') {
            const size_t closePos = line.find('*', i + 1);
            if (closePos != std::string_view::npos && closePos > i + 1) {
                flushRegular();
                cur = std::string(line.substr(i + 1, closePos - i - 1));
                flushRegular(FontStyle::Italic, false);
                i = closePos + 1;
                continue;
            }
        }

        cur.push_back(line[i]);
        ++i;
    }

    flushRegular();
    return out;
}

std::vector<std::string_view> SplitTableCells(std::string_view rowLine) {
    std::string_view s = TrimView(rowLine);
    if (!s.empty() && s.front() == '|') s.remove_prefix(1);
    if (!s.empty() && s.back() == '|') s.remove_suffix(1);

    std::vector<std::string_view> cells;
    size_t pos = 0;
    while (pos <= s.size()) {
        const size_t pipe = s.find('|', pos);
        if (pipe == std::string_view::npos) {
            cells.push_back(TrimView(s.substr(pos)));
            break;
        }
        cells.push_back(TrimView(s.substr(pos, pipe - pos)));
        pos = pipe + 1;
    }
    return cells;
}

bool IsTableSeparatorRow(std::string_view line, std::vector<Align>& outAligns) {
    const std::string_view t = TrimView(line);
    if (t.find('|') == std::string_view::npos || t.find('-') == std::string_view::npos) {
        return false;
    }
    const auto cells = SplitTableCells(t);
    if (cells.empty()) return false;

    std::vector<Align> aligns;
    aligns.reserve(cells.size());
    for (std::string_view c : cells) {
        if (c.empty()) return false;
        bool hasDash = false;
        for (char ch : c) {
            if (ch == '-') hasDash = true;
            else if (ch != ':' && ch != ' ') return false;
        }
        if (!hasDash) return false;
        const bool leftColon = (c.front() == ':');
        const bool rightColon = (c.back() == ':');
        if (leftColon && rightColon) aligns.push_back(Align::Center);
        else if (rightColon) aligns.push_back(Align::End);
        else aligns.push_back(Align::Start);
    }
    outAligns = std::move(aligns);
    return true;
}

const ParsedMarkdownDoc& GetOrParseMarkdown(std::string_view source) {
    static std::unordered_map<uint64_t, ParsedMarkdownDoc> s_cache;
    const uint64_t h = detail::HashArgs(source);
    auto it = s_cache.find(h);
    if (it != s_cache.end()) {
        return it->second;
    }
    if (s_cache.size() > 64) {
        s_cache.clear();
    }

    ParsedMarkdownDoc doc{};
    doc.sourceHash = h;

    std::vector<std::string_view> lines;
    size_t pos = 0;
    while (pos <= source.size()) {
        const size_t nl = source.find('\n', pos);
        if (nl == std::string_view::npos) {
            lines.push_back(source.substr(pos));
            break;
        }
        std::string_view l = source.substr(pos, nl - pos);
        if (!l.empty() && l.back() == '\r') l.remove_suffix(1);
        lines.push_back(l);
        pos = nl + 1;
    }

    int nextCodeBlockIdx = 0;
    size_t i = 0;
    while (i < lines.size()) {
        const std::string_view rawLine = lines[i];
        const std::string_view trimmed = TrimView(rawLine);

        if (trimmed.empty()) {
            ++i;
            continue;
        }

        // 1. Fenced Code Block: ```lang
        if (trimmed.starts_with("```")) {
            MarkdownBlock blk{};
            blk.kind = BlockKind::CodeBlock;
            blk.codeLang = std::string(TrimView(trimmed.substr(3)));
            blk.codeBlockIndex = nextCodeBlockIdx++;
            ++i;
            std::string code;
            while (i < lines.size()) {
                if (TrimView(lines[i]).starts_with("```")) {
                    ++i;
                    break;
                }
                if (!code.empty()) code.push_back('\n');
                code.append(lines[i]);
                ++i;
            }
            blk.rawText = std::move(code);
            doc.blocks.push_back(std::move(blk));
            continue;
        }

        // 2. Horizontal Divider: --- or ***
        if ((trimmed.starts_with("---") || trimmed.starts_with("***")) && trimmed.size() >= 3) {
            bool allSame = true;
            for (char ch : trimmed) {
                if (ch != '-' && ch != '*' && ch != ' ') {
                    allSame = false;
                    break;
                }
            }
            if (allSame) {
                MarkdownBlock blk{};
                blk.kind = BlockKind::Divider;
                doc.blocks.push_back(std::move(blk));
                ++i;
                continue;
            }
        }

        // 3. Heading: # .. ######
        if (trimmed.front() == '#') {
            uint8_t lvl = 0;
            while (lvl < trimmed.size() && lvl < 6 && trimmed[lvl] == '#') {
                ++lvl;
            }
            if (lvl < trimmed.size() && trimmed[lvl] == ' ') {
                MarkdownBlock blk{};
                blk.kind = BlockKind::Heading;
                blk.headingLevel = lvl;
                blk.spans = ParseInlineSpans(TrimView(trimmed.substr(lvl + 1)));
                doc.blocks.push_back(std::move(blk));
                ++i;
                continue;
            }
        }

        // 4. Standalone Image: ![Alt](path)
        if (trimmed.starts_with("![")) {
            const size_t closeBr = trimmed.find("](");
            const size_t closePar = (closeBr != std::string_view::npos) ? trimmed.find(')', closeBr + 2) : std::string_view::npos;
            if (closeBr != std::string_view::npos && closePar != std::string_view::npos) {
                MarkdownBlock blk{};
                blk.kind = BlockKind::ImageBlock;
                blk.imageAlt = std::string(trimmed.substr(2, closeBr - 2));
                blk.rawText = std::string(TrimView(trimmed.substr(closeBr + 2, closePar - closeBr - 2)));
                doc.blocks.push_back(std::move(blk));
                ++i;
                continue;
            }
        }

        // 5. Blockquote or GitHub Alert: > [!NOTE] / > text
        if (trimmed.starts_with(">")) {
            std::string_view firstContent = TrimView(trimmed.substr(1));
            AlertKind alert = AlertKind::None;
            std::string alertTitle;
            if (firstContent.starts_with("[!NOTE]"))          { alert = AlertKind::Note;      alertTitle = "Note"; }
            else if (firstContent.starts_with("[!TIP]"))      { alert = AlertKind::Tip;       alertTitle = "Tip"; }
            else if (firstContent.starts_with("[!IMPORTANT]")){ alert = AlertKind::Important; alertTitle = "Important"; }
            else if (firstContent.starts_with("[!WARNING]"))  { alert = AlertKind::Warning;   alertTitle = "Warning"; }
            else if (firstContent.starts_with("[!CAUTION]"))  { alert = AlertKind::Caution;   alertTitle = "Caution"; }

            std::string combined;
            if (alert != AlertKind::None) {
                const size_t closeBr = firstContent.find(']');
                std::string_view rest = (closeBr != std::string_view::npos) ? TrimView(firstContent.substr(closeBr + 1)) : std::string_view{};
                if (!rest.empty()) combined.append(rest);
                ++i;
            }

            while (i < lines.size()) {
                const std::string_view ql = TrimView(lines[i]);
                if (!ql.starts_with(">")) break;
                const std::string_view sub = TrimView(ql.substr(1));
                if (!sub.empty()) {
                    if (!combined.empty()) combined.push_back(' ');
                    combined.append(sub);
                }
                ++i;
            }

            MarkdownBlock blk{};
            blk.kind = (alert != AlertKind::None) ? BlockKind::Alert : BlockKind::Blockquote;
            blk.alertKind = alert;
            blk.alertTitle = std::move(alertTitle);
            blk.spans = ParseInlineSpans(combined);
            doc.blocks.push_back(std::move(blk));
            continue;
        }

        // 6. Table: | Header | ... followed by | --- | --- |
        std::vector<Align> tableAligns;
        if (trimmed.find('|') != std::string_view::npos && i + 1 < lines.size() &&
            IsTableSeparatorRow(lines[i + 1], tableAligns)) {
            MarkdownBlock blk{};
            blk.kind = BlockKind::Table;
            blk.tableAligns = std::move(tableAligns);

            auto parseRow = [](std::string_view rowLine) {
                const auto rawCells = SplitTableCells(rowLine);
                std::vector<TableCell> row;
                row.reserve(rawCells.size());
                for (std::string_view c : rawCells) {
                    row.push_back(TableCell{ParseInlineSpans(c)});
                }
                return row;
            };

            blk.tableRows.push_back(parseRow(lines[i]));
            i += 2; // Skip header and separator row
            while (i < lines.size()) {
                const std::string_view tl = TrimView(lines[i]);
                if (tl.empty() || tl.find('|') == std::string_view::npos) break;
                blk.tableRows.push_back(parseRow(tl));
                ++i;
            }
            doc.blocks.push_back(std::move(blk));
            continue;
        }

        // 7. Unordered / Task List Item (- item, * item, - [x] task) or Ordered List Item (1. item)
        size_t leadingSpaces = 0;
        while (leadingSpaces < rawLine.size() && rawLine[leadingSpaces] == ' ') {
            ++leadingSpaces;
        }
        const uint8_t indentLvl = static_cast<uint8_t>(std::min<size_t>(3, leadingSpaces / 2));

        if ((trimmed.starts_with("- ") || trimmed.starts_with("* ")) && trimmed.size() > 2) {
            MarkdownBlock blk{};
            blk.kind = BlockKind::ListItem;
            blk.indentLevel = indentLvl;
            std::string_view itemText = TrimView(trimmed.substr(2));
            if (itemText.starts_with("[ ] ")) {
                blk.taskState = 0;
                itemText = TrimView(itemText.substr(4));
            } else if (itemText.starts_with("[x] ") || itemText.starts_with("[X] ")) {
                blk.taskState = 1;
                itemText = TrimView(itemText.substr(4));
            }
            blk.spans = ParseInlineSpans(itemText);
            doc.blocks.push_back(std::move(blk));
            ++i;
            continue;
        }

        size_t digitEnd = 0;
        while (digitEnd < trimmed.size() && trimmed[digitEnd] >= '0' && trimmed[digitEnd] <= '9') {
            ++digitEnd;
        }
        if (digitEnd > 0 && digitEnd + 1 < trimmed.size() && trimmed[digitEnd] == '.' && trimmed[digitEnd + 1] == ' ') {
            MarkdownBlock blk{};
            blk.kind = BlockKind::ListItem;
            blk.indentLevel = indentLvl;
            blk.isOrderedList = true;
            uint16_t num = 0;
            for (size_t d = 0; d < digitEnd; ++d) {
                num = static_cast<uint16_t>(num * 10 + (trimmed[d] - '0'));
            }
            blk.listNumber = std::max<uint16_t>(1, num);
            blk.spans = ParseInlineSpans(TrimView(trimmed.substr(digitEnd + 2)));
            doc.blocks.push_back(std::move(blk));
            ++i;
            continue;
        }

        // 8. Standard Paragraph (merge consecutive plain lines)
        std::string paraText(trimmed);
        ++i;
        while (i < lines.size()) {
            const std::string_view nextT = TrimView(lines[i]);
            if (nextT.empty() || nextT.starts_with("#") || nextT.starts_with("```") ||
                nextT.starts_with(">") || nextT.starts_with("- ") || nextT.starts_with("* ") ||
                nextT.starts_with("![") || nextT.starts_with("---") || nextT.starts_with("|")) {
                break;
            }
            paraText.push_back(' ');
            paraText.append(nextT);
            ++i;
        }

        MarkdownBlock blk{};
        blk.kind = BlockKind::Paragraph;
        blk.spans = ParseInlineSpans(paraText);
        doc.blocks.push_back(std::move(blk));
    }

    auto [insIt, _] = s_cache.emplace(h, std::move(doc));
    return insIt->second;
}

// --- Inline Rich-Text Span Flow Engine (Measure + Render + Link Hit-Testing) ---

struct FlowResult {
    float width{0.0f};
    float height{0.0f};
    std::string_view hoveredLinkUrl{};
    Rect hoveredLinkRect{};
};

FlowResult LayoutOrRenderSpans(
    ICanvas* canvas,
    bool doRender,
    const std::vector<InlineSpan>& spans,
    Rect bounds,
    float baseFontSize,
    Color baseColor,
    const UiTheme& theme,
    Vec2 mousePos = {-1.0f, -1.0f},
    FontStyle overrideStyle = FontStyle::Regular
) {
    FlowResult res{};
    if (spans.empty()) return res;

    const float s = theme.Scale(1.0f);
    const float lineH = canvas ? canvas->GetLineHeight(baseFontSize) : theme.GetLineHeight(baseFontSize);
    const float maxW = (bounds.w > 0.0f) ? bounds.w : 10000.0f;

    float curX = 0.0f;
    float curY = 0.0f;
    float maxLineW = 0.0f;

    auto measureToken = [&](std::string_view tok, FontStyle st, bool isCode) -> float {
        const float fs = isCode ? (baseFontSize * 0.92f) : baseFontSize;
        float w = 0.0f;
        if (canvas) {
            w = canvas->MeasureTextStyled(tok, fs, st).x;
        } else {
            w = static_cast<float>(tok.size()) * fs * 0.56f;
        }
        if (isCode) {
            w += 10.0f * s; // Horizontal pill padding
        }
        return w;
    };

    for (const InlineSpan& span : spans) {
        const FontStyle effStyle = (overrideStyle != FontStyle::Regular && span.style == FontStyle::Regular)
            ? overrideStyle
            : span.style;

        if (span.kind == InlineKind::Code) {
            const float tokW = measureToken(span.text, FontStyle::Monospace, true);
            if (curX > 0.0f && curX + tokW > maxW) {
                maxLineW = std::max(maxLineW, curX);
                curX = 0.0f;
                curY += lineH;
            }
            if (doRender && canvas) {
                const float codeFs = baseFontSize * 0.92f;
                const Rect pillRect(
                    bounds.x + curX,
                    bounds.y + curY + 1.0f * s,
                    tokW,
                     std::max(codeFs + 4.0f * s, lineH - 2.0f * s)
                );
                canvas->DrawRoundRect(
                    pillRect,
                    4.0f * s,
                    theme.palette.surfaceElevated,
                    theme.palette.borderSubtle,
                    1.0f
                );
                canvas->DrawTextBox(
                    span.text,
                    pillRect,
                    codeFs,
                    theme.palette.accent,
                    0.5f,
                    0.5f
                );
            }
            curX += tokW;
            continue;
        }

        // Tokenize by words while preserving spaces
        std::string_view txt = span.text;
        size_t pos = 0;
        while (pos < txt.size()) {
            size_t nextPos = pos;
            if (txt[pos] == ' ') {
                while (nextPos < txt.size() && txt[nextPos] == ' ') ++nextPos;
            } else {
                while (nextPos < txt.size() && txt[nextPos] != ' ') ++nextPos;
            }

            std::string_view word = txt.substr(pos, nextPos - pos);
            pos = nextPos;

            if (word.front() == ' ' && curX == 0.0f) {
                continue; // Skip leading spaces at start of wrapped line
            }

            const float wordW = measureToken(word, effStyle, false);
            if (curX > 0.0f && curX + wordW > maxW && word.front() != ' ') {
                maxLineW = std::max(maxLineW, curX);
                curX = 0.0f;
                curY += lineH;
            }

            const Rect wordRect(bounds.x + curX, bounds.y + curY, wordW, lineH);
            const bool isLink = (span.kind == InlineKind::Link);
            const bool wordHovered = isLink && wordRect.Contains(mousePos);
            if (wordHovered) {
                res.hoveredLinkUrl = span.url;
                res.hoveredLinkRect = wordRect;
            }

            if (doRender && canvas) {
                const Color drawCol = isLink ? theme.palette.accent : baseColor;
                const float glyphH = canvas->MeasureTextStyled("Mg", baseFontSize, effStyle).y;
                const float textY = bounds.y + curY + (lineH - ((glyphH > 0.0f) ? glyphH : baseFontSize)) * 0.5f;
                canvas->DrawTextStyled(word, {bounds.x + curX, textY}, baseFontSize, drawCol, effStyle);

                if (isLink) {
                    const float uy = bounds.y + curY + lineH - 2.0f * s;
                    const Color uCol = wordHovered ? theme.palette.accent : theme.palette.accent.WithAlpha(140);
                    canvas->DrawLine({bounds.x + curX, uy}, {bounds.x + curX + wordW, uy}, uCol, wordHovered ? 1.5f : 1.0f);
                }
                if (span.strikethrough) {
                    const float sy = bounds.y + curY + lineH * 0.52f;
                    canvas->DrawLine({bounds.x + curX, sy}, {bounds.x + curX + wordW, sy}, drawCol.WithAlpha(180), 1.2f);
                }
            }

            curX += wordW;
        }
    }

    maxLineW = std::max(maxLineW, curX);
    res.width = maxLineW;
    res.height = curY + lineH;
    return res;
}

float HeadingFontSize(uint8_t level, const UiTheme& theme) noexcept {
    switch (level) {
        case 1: return theme.GetFontSize(TextRole::HeadlineLarge);
        case 2: return theme.GetFontSize(TextRole::HeadlineSmall);
        case 3: return theme.GetFontSize(TextRole::TitleLarge);
        case 4: return theme.GetFontSize(TextRole::TitleMedium);
        case 5: return theme.GetFontSize(TextRole::LabelLarge);
        default: return theme.GetFontSize(TextRole::LabelMedium);
    }
}

Color AlertAccentColor(AlertKind kind, const UiTheme& theme) noexcept {
    switch (kind) {
        case AlertKind::Note:      return theme.GetVariant(UiVariant::Info).bg;
        case AlertKind::Tip:       return theme.GetVariant(UiVariant::Success).bg;
        case AlertKind::Important: return theme.GetVariant(UiVariant::Primary).bg;
        case AlertKind::Warning:   return theme.GetVariant(UiVariant::Warning).bg;
        case AlertKind::Caution:   return theme.GetVariant(UiVariant::Danger).bg;
        default:                   return theme.palette.accent;
    }
}

Icon AlertIcon(AlertKind kind) noexcept {
    switch (kind) {
        case AlertKind::Note:      return Icon::Info();
        case AlertKind::Tip:       return Icon::Check();
        case AlertKind::Important: return Icon::Flash();
        case AlertKind::Warning:   return Icon::Warning();
        case AlertKind::Caution:   return Icon::Error();
        default:                   return Icon::Info();
    }
}

struct MarkdownPassResult {
    float totalWidth{0.0f};
    float totalHeight{0.0f};
    std::string_view hoveredLink{};
    Rect hoveredLinkRect{};
    int hoveredCopyBlock{-1};
    std::string_view hoveredCopyCode{};
};

MarkdownPassResult ExecuteMarkdownLayoutAndRender(
    ICanvas* canvas,
    bool doRender,
    Rect contentBounds,
    const ParsedMarkdownDoc& doc,
    const MarkdownOptions& opts,
    const UiTheme& theme,
    Vec2 mousePos,
    int hoveredCopyBlock,
    int activeCopiedBlock,
    bool disabled
) {
    MarkdownPassResult out{};
    const float s = theme.Scale(1.0f);
    const float availW = std::max(120.0f, contentBounds.w);
    const float bodyFs = theme.GetFontSize(TextRole::BodyMedium);
    const float codeFs = theme.GetFontSize(TextRole::CodeMedium);
    const float codeLineH = canvas ? canvas->GetLineHeight(codeFs) : theme.GetLineHeight(codeFs);
    const float gap = theme.Scale(opts.blockGap);

    float curY = contentBounds.y;
    bool firstBlock = true;

    for (const MarkdownBlock& blk : doc.blocks) {
        if (!firstBlock) {
            curY += (blk.kind == BlockKind::Heading) ? (gap * 1.25f) : gap;
        }
        firstBlock = false;

        switch (blk.kind) {
            case BlockKind::Divider: {
                const float h = 8.0f * s;
                if (doRender && canvas) {
                    const float ly = curY + h * 0.5f;
                    canvas->DrawLine({contentBounds.x, ly}, {contentBounds.x + availW, ly}, theme.palette.borderSubtle, 1.0f);
                }
                curY += h;
                out.totalWidth = std::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::Heading: {
                const float hFs = HeadingFontSize(blk.headingLevel, theme);
                const FlowResult fr = LayoutOrRenderSpans(
                    canvas,
                    doRender,
                    blk.spans,
                    Rect(contentBounds.x, curY, availW, 0.0f),
                    hFs,
                    theme.palette.textPrimary,
                    theme,
                    mousePos,
                    FontStyle::Bold
                );
                if (!fr.hoveredLinkUrl.empty()) {
                    out.hoveredLink = fr.hoveredLinkUrl;
                    out.hoveredLinkRect = fr.hoveredLinkRect;
                }
                curY += fr.height;
                if (blk.headingLevel <= 2) {
                    curY += 6.0f * s;
                    if (doRender && canvas) {
                        canvas->DrawLine({contentBounds.x, curY}, {contentBounds.x + availW, curY}, theme.palette.borderSubtle, 1.0f);
                    }
                    curY += 2.0f * s;
                }
                out.totalWidth = std::max(out.totalWidth, fr.width);
                break;
            }

            case BlockKind::Paragraph: {
                const FlowResult fr = LayoutOrRenderSpans(
                    canvas,
                    doRender,
                    blk.spans,
                    Rect(contentBounds.x, curY, availW, 0.0f),
                    bodyFs,
                    theme.palette.textPrimary,
                    theme,
                    mousePos
                );
                if (!fr.hoveredLinkUrl.empty()) {
                    out.hoveredLink = fr.hoveredLinkUrl;
                    out.hoveredLinkRect = fr.hoveredLinkRect;
                }
                curY += fr.height;
                out.totalWidth = std::max(out.totalWidth, fr.width);
                break;
            }

            case BlockKind::Blockquote: {
                const float padX = 14.0f * s;
                const float padY = 8.0f * s;
                const float innerW = std::max(40.0f, availW - padX * 2.0f);
                const FlowResult measureFr = LayoutOrRenderSpans(
                    canvas,
                    false,
                    blk.spans,
                    Rect(contentBounds.x + padX, curY + padY, innerW, 0.0f),
                    bodyFs,
                    theme.palette.textSecondary,
                    theme,
                    mousePos,
                    FontStyle::Italic
                );
                const float boxH = measureFr.height + padY * 2.0f;
                const Rect boxRect(contentBounds.x, curY, availW, boxH);

                if (doRender && canvas) {
                    canvas->DrawRoundRect(boxRect, 6.0f * s, theme.palette.surfaceElevated.WithAlpha(140));
                    canvas->DrawRoundRect(Rect(boxRect.x, boxRect.y, 4.0f * s, boxRect.h), 2.0f * s, theme.palette.borderHover);
                    LayoutOrRenderSpans(
                        canvas,
                        true,
                        blk.spans,
                        Rect(contentBounds.x + padX, curY + padY, innerW, 0.0f),
                        bodyFs,
                        theme.palette.textSecondary,
                        theme,
                        mousePos,
                        FontStyle::Italic
                    );
                }
                if (!measureFr.hoveredLinkUrl.empty()) {
                    out.hoveredLink = measureFr.hoveredLinkUrl;
                    out.hoveredLinkRect = measureFr.hoveredLinkRect;
                }
                curY += boxH;
                out.totalWidth = std::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::Alert: {
                const Color accent = AlertAccentColor(blk.alertKind, theme);
                const float padX = 16.0f * s;
                const float padY = 10.0f * s;
                const float headerH = 20.0f * s;
                const float innerW = std::max(40.0f, availW - padX * 2.0f);

                const FlowResult measureFr = LayoutOrRenderSpans(
                    canvas,
                    false,
                    blk.spans,
                    Rect(contentBounds.x + padX, curY + padY + headerH + 4.0f * s, innerW, 0.0f),
                    bodyFs,
                    theme.palette.textPrimary,
                    theme,
                    mousePos
                );
                const float boxH = padY * 2.0f + headerH + 4.0f * s + measureFr.height;
                const Rect boxRect(contentBounds.x, curY, availW, boxH);

                if (doRender && canvas) {
                    canvas->DrawRoundRect(boxRect, 6.0f * s, accent.WithAlpha(24), accent.WithAlpha(90), 1.0f);
                    canvas->DrawRoundRect(Rect(boxRect.x, boxRect.y, 4.0f * s, boxRect.h), 2.0f * s, accent);

                    const Icon ic = AlertIcon(blk.alertKind);
                    const float icSz = 15.0f * s;
                    ic.Render(*canvas, Rect(contentBounds.x + padX, curY + padY + (headerH - icSz) * 0.5f, icSz, icSz), accent);

                    const float titleFs = theme.GetFontSize(TextRole::LabelLarge);
                    canvas->DrawTextStyled(
                        blk.alertTitle,
                        {contentBounds.x + padX + icSz + 8.0f * s, curY + padY + (headerH - titleFs) * 0.5f},
                        titleFs,
                        accent,
                        FontStyle::Bold
                    );

                    LayoutOrRenderSpans(
                        canvas,
                        true,
                        blk.spans,
                        Rect(contentBounds.x + padX, curY + padY + headerH + 4.0f * s, innerW, 0.0f),
                        bodyFs,
                        theme.palette.textPrimary,
                        theme,
                        mousePos
                    );
                }
                if (!measureFr.hoveredLinkUrl.empty()) {
                    out.hoveredLink = measureFr.hoveredLinkUrl;
                    out.hoveredLinkRect = measureFr.hoveredLinkRect;
                }
                curY += boxH;
                out.totalWidth = std::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::ListItem: {
                const float indent = static_cast<float>(blk.indentLevel) * 18.0f * s + 22.0f * s;
                const float itemX = contentBounds.x + indent;
                const float itemW = std::max(40.0f, availW - indent);
                const float lineH = canvas ? canvas->GetLineHeight(bodyFs) : theme.GetLineHeight(bodyFs);

                if (doRender && canvas) {
                    const float markerX = itemX - 18.0f * s;
                    if (blk.taskState >= 0) {
                        const float cbSz = 14.0f * s;
                        const Rect cbRect(markerX, curY + (lineH - cbSz) * 0.5f, cbSz, cbSz);
                        if (blk.taskState == 1) {
                            canvas->DrawRoundRect(cbRect, 3.0f * s, theme.palette.accent);
                            Icon::Check().Render(*canvas, cbRect.Inset(2.0f * s), Color(255, 255, 255, 255));
                        } else {
                            canvas->DrawRoundRect(cbRect, 3.0f * s, theme.palette.surfaceElevated, theme.palette.borderHover, 1.2f);
                        }
                    } else if (blk.isOrderedList) {
                        char numBuf[16];
                        std::snprintf(numBuf, sizeof(numBuf), "%u.", static_cast<unsigned>(blk.listNumber));
                        canvas->DrawTextStyled(
                            numBuf,
                            {markerX - 2.0f * s, curY + (lineH - bodyFs) * 0.5f},
                            bodyFs,
                            theme.palette.accent,
                            FontStyle::Bold
                        );
                    } else {
                        const float dotSz = 5.0f * s;
                        const Rect dotRect(markerX + 4.0f * s, curY + (lineH - dotSz) * 0.5f, dotSz, dotSz);
                        canvas->DrawRoundRect(dotRect, dotSz * 0.5f, theme.palette.accent);
                    }
                }

                const FlowResult fr = LayoutOrRenderSpans(
                    canvas,
                    doRender,
                    blk.spans,
                    Rect(itemX, curY, itemW, 0.0f),
                    bodyFs,
                    (blk.taskState == 1) ? theme.palette.textSecondary : theme.palette.textPrimary,
                    theme,
                    mousePos
                );
                if (!fr.hoveredLinkUrl.empty()) {
                    out.hoveredLink = fr.hoveredLinkUrl;
                    out.hoveredLinkRect = fr.hoveredLinkRect;
                }
                curY += fr.height;
                out.totalWidth = std::max(out.totalWidth, indent + fr.width);
                break;
            }

            case BlockKind::CodeBlock: {
                // Count lines
                std::vector<std::string_view> codeLines;
                size_t p = 0;
                std::string_view raw = blk.rawText;
                while (p <= raw.size()) {
                    const size_t nl = raw.find('\n', p);
                    if (nl == std::string_view::npos) {
                        codeLines.push_back(raw.substr(p));
                        break;
                    }
                    codeLines.push_back(raw.substr(p, nl - p));
                    p = nl + 1;
                }
                if (codeLines.empty()) codeLines.push_back("");

                const float headerH = 28.0f * s;
                const float padY = 10.0f * s;
                const float padX = 12.0f * s;
                const float gutterW = opts.codeLineNumbers ? (34.0f * s) : 0.0f;
                const float bodyH = static_cast<float>(codeLines.size()) * codeLineH + padY * 2.0f;
                const float totalBoxH = headerH + bodyH;
                const Rect boxRect(contentBounds.x, curY, availW, totalBoxH);

                // Copy button geometry in header
                const float copyBtnW = (activeCopiedBlock == blk.codeBlockIndex) ? (72.0f * s) : (56.0f * s);
                const float copyBtnH = 20.0f * s;
                const Rect copyBtnRect(
                    boxRect.Right() - copyBtnW - 6.0f * s,
                    boxRect.y + (headerH - copyBtnH) * 0.5f,
                    copyBtnW,
                    copyBtnH
                );
                if (opts.showCopyButton && !disabled && copyBtnRect.Contains(mousePos)) {
                    out.hoveredCopyBlock = blk.codeBlockIndex;
                    out.hoveredCopyCode = blk.rawText;
                }

                if (doRender && canvas) {
                    const float rad = 6.0f * s;
                    canvas->DrawRoundRect(boxRect, rad, theme.palette.surfaceElevated, theme.palette.borderSubtle, 1.0f);
                    // Header divider
                    canvas->DrawLine(
                        {boxRect.x, boxRect.y + headerH},
                        {boxRect.Right(), boxRect.y + headerH},
                        theme.palette.borderSubtle,
                        1.0f
                    );

                    const float lblFs = theme.GetFontSize(TextRole::LabelSmall);
                    const std::string_view langLabel = blk.codeLang.empty() ? "code" : std::string_view(blk.codeLang);
                    canvas->DrawTextStyled(
                        langLabel,
                        {boxRect.x + padX, boxRect.y + (headerH - lblFs) * 0.5f},
                        lblFs,
                        theme.palette.textMuted,
                        FontStyle::Bold
                    );

                    if (opts.showCopyButton) {
                        const bool isHov = (hoveredCopyBlock == blk.codeBlockIndex);
                        const bool isCopied = (activeCopiedBlock == blk.codeBlockIndex);
                        const Color btnBg = isCopied
                            ? theme.GetVariant(UiVariant::Success).bg.WithAlpha(40)
                            : (isHov ? theme.palette.accent.WithAlpha(35) : theme.palette.surface);
                        const Color btnBorder = isCopied
                            ? theme.GetVariant(UiVariant::Success).bg
                            : (isHov ? theme.palette.accent : theme.palette.borderSubtle);
                        const Color btnText = isCopied
                            ? theme.GetVariant(UiVariant::Success).bg
                            : (isHov ? theme.palette.accent : theme.palette.textSecondary);
                        canvas->DrawRoundRect(copyBtnRect, 4.0f * s, btnBg, btnBorder, 1.0f);
                        canvas->DrawTextBox(
                            isCopied ? "Copied" : "Copy",
                            copyBtnRect,
                            lblFs,
                            btnText,
                            0.5f,
                            0.5f
                        );
                    }

                    // Render code lines with optional gutter and syntax highlighting
                    canvas->PushClip(Rect(boxRect.x + 1.0f, boxRect.y + headerH + 1.0f, boxRect.w - 2.0f, bodyH - 2.0f));
                    if (opts.codeLineNumbers) {
                        canvas->DrawLine(
                            {boxRect.x + gutterW, boxRect.y + headerH},
                            {boxRect.x + gutterW, boxRect.Bottom()},
                            theme.palette.borderSubtle,
                            1.0f
                        );
                    }

                    const float codeGlyphH = canvas->MeasureTextStyled("Mg", codeFs, FontStyle::Monospace).y;
                    std::vector<TextSpan> syntaxSpans;
                    for (size_t ln = 0; ln < codeLines.size(); ++ln) {
                        const float ly = boxRect.y + headerH + padY + static_cast<float>(ln) * codeLineH;
                        const float textY = ly + (codeLineH - ((codeGlyphH > 0.0f) ? codeGlyphH : codeFs)) * 0.5f;

                        if (opts.codeLineNumbers) {
                            char lnBuf[16];
                            std::snprintf(lnBuf, sizeof(lnBuf), "%zu", ln + 1);
                            canvas->DrawTextBox(
                                lnBuf,
                                Rect(boxRect.x + 4.0f * s, ly, gutterW - 10.0f * s, codeLineH),
                                theme.GetFontSize(TextRole::CodeSmall),
                                theme.palette.textMuted,
                                1.0f,
                                0.5f
                            );
                        }

                        const float codeStartX = boxRect.x + gutterW + padX;
                        const std::string_view lineStr = codeLines[ln];
                        if (opts.syntaxHighlight && !lineStr.empty()) {
                            syntaxSpans.clear();
                            TextArea::CppHighlighter(lineStr, ln, syntaxSpans, theme, nullptr);
                            if (syntaxSpans.empty()) {
                                canvas->DrawTextStyled(lineStr, {codeStartX, textY}, codeFs, theme.palette.textPrimary, FontStyle::Monospace);
                            } else {
                                float spanX = codeStartX;
                                for (const TextSpan& sp : syntaxSpans) {
                                    if (sp.startCol < lineStr.size() && sp.length > 0) {
                                        const std::string_view tok = lineStr.substr(sp.startCol, sp.length);
                                        bool allSpaces = true;
                                        for (char ch : tok) {
                                            if (ch != ' ' && ch != '\t') {
                                                allSpaces = false;
                                                break;
                                            }
                                        }
                                        if (!allSpaces) {
                                            canvas->DrawTextStyled(tok, {spanX, textY}, codeFs, sp.color, FontStyle::Monospace);
                                        }
                                        spanX += canvas->MeasureTextStyled(tok, codeFs, FontStyle::Monospace).x;
                                    }
                                }
                            }
                        } else if (!lineStr.empty()) {
                            canvas->DrawTextStyled(lineStr, {codeStartX, textY}, codeFs, theme.palette.textPrimary, FontStyle::Monospace);
                        }
                    }
                    canvas->PopClip();
                }

                curY += totalBoxH;
                out.totalWidth = std::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::ImageBlock: {
                Vec2 nat = canvas ? canvas->GetImageSize(blk.rawText) : Vec2{0.0f, 0.0f};
                if (nat.x <= 0.0f || nat.y <= 0.0f) {
                    nat = {320.0f, 160.0f};
                }
                const Vec2 scaledNat = { theme.Scale(nat.x), theme.Scale(nat.y) };
                const float maxH = theme.Scale(opts.imageMaxHeight);
                const float scaleFactor = std::min({1.0f, availW / scaledNat.x, maxH / scaledNat.y});
                const float imgW = std::max(40.0f, scaledNat.x * scaleFactor);
                const float imgH = std::max(30.0f, scaledNat.y * scaleFactor);
                const float imgX = contentBounds.x + (availW - imgW) * 0.5f;
                const Rect imgRect(imgX, curY, imgW, imgH);
                const float rad = theme.Scale(opts.imageRadius);

                if (doRender && canvas) {
                    if (opts.imageCheckerboard) {
                        Image::DrawCheckerboard(*canvas, imgRect, rad, theme, 10.0f);
                    } else {
                        canvas->DrawRoundRect(imgRect, rad, theme.palette.surfaceElevated);
                    }
                    if (!canvas->DrawImage(blk.rawText, imgRect, rad, 255)) {
                        canvas->DrawTextBox(
                            blk.imageAlt.empty() ? blk.rawText : std::string_view(blk.imageAlt),
                            imgRect.Inset(8.0f),
                            theme.GetFontSize(TextRole::LabelMedium),
                            theme.palette.textMuted,
                            0.5f,
                            0.5f
                        );
                    }
                    canvas->DrawRoundRect(imgRect, rad, Color::Transparent(), theme.palette.borderSubtle, 1.0f);
                }
                curY += imgH;

                if (!blk.imageAlt.empty()) {
                    const float capFs = theme.GetFontSize(TextRole::LabelSmall);
                    const float capH = (canvas ? canvas->GetLineHeight(capFs) : theme.GetLineHeight(capFs)) + 4.0f * s;
                    if (doRender && canvas) {
                        const float tw = canvas->MeasureTextStyled(blk.imageAlt, capFs, FontStyle::Italic).x;
                        const float tx = contentBounds.x + std::max(0.0f, (availW - tw) * 0.5f);
                        canvas->DrawTextStyled(
                            blk.imageAlt,
                            {tx, curY + 4.0f * s},
                            capFs,
                            theme.palette.textMuted,
                            FontStyle::Italic
                        );
                    }
                    curY += capH;
                }
                out.totalWidth = std::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::Table: {
                if (blk.tableRows.empty()) break;
                const size_t numCols = std::max<size_t>(1, blk.tableAligns.size());
                const float colW = availW / static_cast<float>(numCols);
                const float cellPadX = 10.0f * s;
                const float cellPadY = 7.0f * s;
                const float rowH = (canvas ? canvas->GetLineHeight(bodyFs) : theme.GetLineHeight(bodyFs)) + cellPadY * 2.0f;
                const float tableH = static_cast<float>(blk.tableRows.size()) * rowH;
                const Rect tableRect(contentBounds.x, curY, availW, tableH);

                if (doRender && canvas) {
                    canvas->DrawRoundRect(tableRect, 6.0f * s, theme.palette.surface, theme.palette.borderSubtle, 1.0f);

                    for (size_t r = 0; r < blk.tableRows.size(); ++r) {
                        const float ry = curY + static_cast<float>(r) * rowH;
                        const Rect rowRect(contentBounds.x, ry, availW, rowH);
                        if (r == 0) {
                            canvas->DrawRoundRect(rowRect, 6.0f * s, theme.palette.surfaceElevated);
                        } else if (r % 2 == 1) {
                            canvas->DrawRect(rowRect.Inset(1.0f, 0.0f), theme.palette.surfaceElevated.WithAlpha(90));
                        }
                        if (r > 0) {
                            canvas->DrawLine({rowRect.x, ry}, {rowRect.Right(), ry}, theme.palette.borderSubtle, 1.0f);
                        }

                        const auto& rowCells = blk.tableRows[r];
                        for (size_t c = 0; c < numCols; ++c) {
                            const float cx = contentBounds.x + static_cast<float>(c) * colW;
                            if (c > 0) {
                                canvas->DrawLine({cx, ry}, {cx, ry + rowH}, theme.palette.borderSubtle, 1.0f);
                            }
                            if (c < rowCells.size()) {
                                const Align colAlign = (c < blk.tableAligns.size()) ? blk.tableAligns[c] : Align::Start;
                                const FontStyle cellStyle = (r == 0) ? FontStyle::Bold : FontStyle::Regular;
                                const Rect cellInner(cx + cellPadX, ry + cellPadY, std::max(10.0f, colW - cellPadX * 2.0f), rowH - cellPadY * 2.0f);
                                const FlowResult cfr = LayoutOrRenderSpans(
                                    canvas,
                                    false,
                                    rowCells[c].spans,
                                    cellInner,
                                    bodyFs,
                                    theme.palette.textPrimary,
                                    theme,
                                    mousePos,
                                    cellStyle
                                );
                                float offsetX = 0.0f;
                                if (colAlign == Align::Center) offsetX = std::max(0.0f, (cellInner.w - cfr.width) * 0.5f);
                                else if (colAlign == Align::End) offsetX = std::max(0.0f, cellInner.w - cfr.width);

                                LayoutOrRenderSpans(
                                    canvas,
                                    true,
                                    rowCells[c].spans,
                                    Rect(cellInner.x + offsetX, cellInner.y, cellInner.w - offsetX, cellInner.h),
                                    bodyFs,
                                    theme.palette.textPrimary,
                                    theme,
                                    mousePos,
                                    cellStyle
                                );
                            }
                        }
                    }
                }
                curY += tableH;
                out.totalWidth = std::max(out.totalWidth, availW);
                break;
            }
        }
    }

    out.totalHeight = std::max(0.0f, curY - contentBounds.y);
    return out;
}

struct MarkdownState {
    int copiedBlockIndex{-1};
};

struct MarkdownPayload {
    std::string_view text{};
    MarkdownOptions opts{};
    int hoveredCopyBlock{-1};
    int activeCopiedBlock{-1};
};

ElementSpec MakeMarkdownSpec(const MarkdownOptions& opts, UiSize size, const UiTheme& theme, bool disabled) noexcept {
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .elementAlign = opts.elementAlign,
        .stretch = opts.stretch,
        .width = opts.width,
        .widgetSize = size,
        .labelGap = theme.Scale(6.0f),
        .descGap = theme.Scale(4.0f),
        .inlineGap = theme.Scale(12.0f),
        .stretchableControl = true,
        .stretchHeightWhenStandalone = false,
        .disabled = disabled
    };
}

} // namespace

MarkdownResponse Markdown::Run(
    Context& ctx,
    std::string_view markdownText,
    const MarkdownOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize activeSize = opts.size.value_or(ctx.GetCurrentSize());
    const UiTheme& theme = ctx.GetTheme();

    auto& state = ctx.GetOrCreateState<MarkdownState>(id);

    Response baseRes = ctx.Interact(id);
    MarkdownResponse res{};
    static_cast<Response&>(res) = baseRes;

    const ParsedMarkdownDoc& doc = GetOrParseMarkdown(markdownText);
    int hoveredCopyBlock = -1;

    if (baseRes.hovered && !effectiveDisabled && baseRes.rect.w > 0.0f) {
        ICanvas* hitCanvas = ctx.GetCanvas();
        const Rect hitBounds = ctx.GetPrevUnclippedBounds(id);
        const ElementSpec spec = MakeMarkdownSpec(opts, activeSize, theme, effectiveDisabled);
        const Vec2 ctrlSize = { hitBounds.w, hitBounds.h };
        const Rect ctrlRect = ElementComposer::Resolve(hitBounds, ctrlSize, spec, hitCanvas, theme).controlRect;

        const MarkdownPassResult hit = ExecuteMarkdownLayoutAndRender(
            hitCanvas,
            false,
            ctrlRect,
            doc,
            opts,
            theme,
            ctx.GetMousePos(),
            -1,
            state.copiedBlockIndex,
            effectiveDisabled
        );

        hoveredCopyBlock = hit.hoveredCopyBlock;
        if (!hit.hoveredLink.empty()) {
            ctx.SetTooltip(hit.hoveredLink, hit.hoveredLinkRect);
            if (baseRes.clicked) {
                res.clickedLink = hit.hoveredLink;
                res.changed = true;
                if (opts.onLinkClick) {
                    opts.onLinkClick(hit.hoveredLink);
                }
            }
        } else if (hit.hoveredCopyBlock >= 0 && baseRes.clicked) {
            state.copiedBlockIndex = hit.hoveredCopyBlock;
            res.copiedBlockIndex = hit.hoveredCopyBlock;
            res.changed = true;
            ctx.SetClipboardText(hit.hoveredCopyCode);
        }
    }

    Node* node = ctx.CreateNode(NodeType::Markdown, id);
    auto* payload = ctx.AllocPayload<MarkdownPayload>();
    payload->text = ctx.AllocString(markdownText);
    payload->opts = opts;
    payload->opts.label = ctx.AllocText(opts.label);
    payload->opts.description = ctx.AllocText(opts.description);
    payload->hoveredCopyBlock = hoveredCopyBlock;
    payload->activeCopiedBlock = state.copiedBlockIndex;
    node->SetPayload(payload);

    node->visualHash = detail::HashArgs(
        doc.sourceHash,
        payload->opts.label,
        payload->opts.description,
        opts.codeLineNumbers,
        opts.syntaxHighlight,
        opts.showCopyButton,
        opts.imageCheckerboard,
        opts.imageMaxHeight,
        opts.imageRadius,
        opts.blockGap,
        hoveredCopyBlock,
        state.copiedBlockIndex,
        activeSize,
        effectiveDisabled
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<MarkdownPayload>();
        if (!p) return {0.0f, 0.0f};
        return Markdown::Measure(p->text, p->opts, n->uiSize, canvas, th, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& th) noexcept {
        const auto* p = n->As<MarkdownPayload>();
        if (!p) return;
        Markdown::Render(
            canvas,
            n->computedRect,
            p->text,
            p->opts,
            n->uiSize,
            {-1.0f, -1.0f},
            p->hoveredCopyBlock,
            p->activeCopiedBlock,
            n->disabled,
            th
        );
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = baseRes.hovered;
    node->pressed = baseRes.pressed;
    node->widthMode = (opts.stretch != StretchMode::Compact) ? SizeMode::Stretch : SizeMode::Fit;
    node->heightMode = SizeMode::Fit;

    ctx.AdaptChildToContainer(node);
    if (node->heightMode == SizeMode::Stretch) {
        node->heightMode = SizeMode::Fit;
    }

    return res;
}

Vec2 Markdown::Measure(
    std::string_view markdownText,
    const MarkdownOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    float availableWidth
) {
    const ParsedMarkdownDoc& doc = GetOrParseMarkdown(markdownText);
    const ElementSpec spec = MakeMarkdownSpec(opts, size, theme, opts.disabled);

    float ctrlW = (availableWidth > 0.0f) ? availableWidth : theme.Scale(520.0f);
    if (opts.stretch == StretchMode::Fill && availableWidth > 0.0f && opts.width.has_value()) {
        const float pct = std::clamp(*opts.width, 10.0f, 100.0f);
        ctrlW = availableWidth * (pct * 0.01f);
    } else if (opts.width.has_value() && *opts.width > 0.0f) {
        ctrlW = theme.Scale(*opts.width);
    }

    const MarkdownPassResult res = ExecuteMarkdownLayoutAndRender(
        canvas,
        false,
        Rect(0.0f, 0.0f, ctrlW, 0.0f),
        doc,
        opts,
        theme,
        {-1.0f, -1.0f},
        -1,
        -1,
        opts.disabled
    );

    const Vec2 ctrlSize = { std::max(ctrlW, res.totalWidth), res.totalHeight };
    return ElementComposer::Measure(ctrlSize, spec, canvas, theme, availableWidth);
}

void Markdown::Render(
    ICanvas& canvas,
    Rect bounds,
    std::string_view markdownText,
    const MarkdownOptions& opts,
    UiSize size,
    Vec2 mousePos,
    int hoveredCopyBlock,
    int activeCopiedBlock,
    bool disabled,
    const UiTheme& theme
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const ParsedMarkdownDoc& doc = GetOrParseMarkdown(markdownText);
    const ElementSpec spec = MakeMarkdownSpec(opts, size, theme, disabled);

    const MarkdownPassResult mRes = ExecuteMarkdownLayoutAndRender(
        &canvas,
        false,
        Rect(0.0f, 0.0f, bounds.w, 0.0f),
        doc,
        opts,
        theme,
        mousePos,
        hoveredCopyBlock,
        activeCopiedBlock,
        disabled
    );

    const ElementGeometry geom = ElementComposer::Resolve(
        bounds,
        {bounds.w, mRes.totalHeight},
        spec,
        &canvas,
        theme
    );
    ElementComposer::RenderText(canvas, geom, spec, theme);

    ExecuteMarkdownLayoutAndRender(
        &canvas,
        true,
        geom.controlRect,
        doc,
        opts,
        theme,
        mousePos,
        hoveredCopyBlock,
        activeCopiedBlock,
        disabled
    );
}

} // namespace guikit
