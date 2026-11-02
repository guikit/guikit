#include "guikit/widgets/Markdown.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "guikit/Context.hpp"
#include "guikit/backend/ICanvas.hpp"
#include "guikit/core/Control.hpp"
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
    int taskIndex{-1};                  // 0-based index across task items in document
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

FontStyle CombineFontStyles(FontStyle a, FontStyle b) noexcept {
    if (a == FontStyle::Monospace || b == FontStyle::Monospace) {
        return FontStyle::Monospace;
    }
    const bool bold = (a == FontStyle::Bold || a == FontStyle::BoldItalic ||
                       b == FontStyle::Bold || b == FontStyle::BoldItalic);
    const bool italic = (a == FontStyle::Italic || a == FontStyle::BoldItalic ||
                         b == FontStyle::Italic || b == FontStyle::BoldItalic);
    if (bold && italic) return FontStyle::BoldItalic;
    if (bold) return FontStyle::Bold;
    if (italic) return FontStyle::Italic;
    return FontStyle::Regular;
}

std::vector<InlineSpan> ParseInlineSpans(
    std::string_view line,
    FontStyle inheritedStyle = FontStyle::Regular,
    bool inheritedStrike = false
) {
    std::vector<InlineSpan> out;
    size_t i = 0;
    const size_t n = line.size();

    std::string cur;
    auto flushCurrent = [&]() {
        if (!cur.empty()) {
            out.push_back(InlineSpan{
                .kind = InlineKind::Text,
                .style = inheritedStyle,
                .strikethrough = inheritedStrike,
                .text = std::move(cur),
                .url = {}
            });
            cur.clear();
        }
    };

    auto appendNested = [&](std::string_view inner, FontStyle addedStyle, bool addedStrike) {
        auto nested = ParseInlineSpans(
            inner,
            CombineFontStyles(inheritedStyle, addedStyle),
            inheritedStrike || addedStrike
        );
        for (auto& sp : nested) {
            out.push_back(std::move(sp));
        }
    };

    while (i < n) {
        // Inline code: `code`
        if (line[i] == '`') {
            const size_t endTick = line.find('`', i + 1);
            if (endTick != std::string_view::npos && endTick > i + 1) {
                flushCurrent();
                out.push_back(InlineSpan{
                    .kind = InlineKind::Code,
                    .style = FontStyle::Monospace,
                    .strikethrough = inheritedStrike,
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
                    flushCurrent();
                    std::string_view labelView = line.substr(i + 1, closeBr - i - 1);
                    if (labelView.size() >= 2 && labelView.front() == '`' && labelView.back() == '`') {
                        labelView = labelView.substr(1, labelView.size() - 2);
                    }
                    out.push_back(InlineSpan{
                        .kind = InlineKind::Link,
                        .style = inheritedStyle,
                        .strikethrough = inheritedStrike,
                        .text = std::string(labelView),
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
                flushCurrent();
                appendNested(line.substr(i + 3, closePos - i - 3), FontStyle::BoldItalic, false);
                i = closePos + 3;
                continue;
            }
        }

        // Bold: **text**
        if (i + 1 < n && line.substr(i, 2) == "**") {
            const size_t closePos = line.find("**", i + 2);
            if (closePos != std::string_view::npos && closePos > i + 2) {
                flushCurrent();
                appendNested(line.substr(i + 2, closePos - i - 2), FontStyle::Bold, false);
                i = closePos + 2;
                continue;
            }
        }

        // Strikethrough: ~~text~~
        if (i + 1 < n && line.substr(i, 2) == "~~") {
            const size_t closePos = line.find("~~", i + 2);
            if (closePos != std::string_view::npos && closePos > i + 2) {
                flushCurrent();
                appendNested(line.substr(i + 2, closePos - i - 2), FontStyle::Regular, true);
                i = closePos + 2;
                continue;
            }
        }

        // Italic: *text*
        if (line[i] == '*') {
            const size_t closePos = line.find('*', i + 1);
            if (closePos != std::string_view::npos && closePos > i + 1) {
                flushCurrent();
                appendNested(line.substr(i + 1, closePos - i - 1), FontStyle::Italic, false);
                i = closePos + 1;
                continue;
            }
        }

        cur.push_back(line[i]);
        ++i;
    }

    flushCurrent();
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
    int nextTaskIdx = 0;
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
                if (!blk.imageAlt.empty()) {
                    blk.spans = ParseInlineSpans(blk.imageAlt, FontStyle::Italic, false);
                }
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
                blk.taskIndex = nextTaskIdx++;
                itemText = TrimView(itemText.substr(4));
            } else if (itemText.starts_with("[x] ") || itemText.starts_with("[X] ")) {
                blk.taskState = 1;
                blk.taskIndex = nextTaskIdx++;
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

// --- Positioned Token / Line Map for Selection, Cursor Navigation & Search ---

struct PositionedToken {
    Rect rect{};
    size_t charStart{0};
    size_t charEnd{0};
    size_t lineIndex{0};
    float fontSize{13.0f};
    FontStyle fontStyle{FontStyle::Regular};
    float textPadX{0.0f};
};

struct PositionedLine {
    float top{0.0f};
    float bottom{0.0f};
    size_t firstToken{0};
    size_t tokenCount{0};
    size_t charStart{0};
    size_t charEnd{0};
};

struct SearchMatchRange {
    size_t start{0};
    size_t end{0};
};

struct DocumentTextMap {
    std::string plainText{};
    std::vector<PositionedToken> tokens{};
    std::vector<PositionedLine> lines{};

    void Clear() {
        plainText.clear();
        tokens.clear();
        lines.clear();
    }

    void AppendNewline() {
        if (!plainText.empty() && plainText.back() != '\n') {
            plainText.push_back('\n');
        }
    }

    void AddToken(Rect r, std::string_view text, float fs, FontStyle style, float padX = 0.0f) {
        if (text.empty()) return;
        const size_t cStart = plainText.size();
        plainText.append(text);
        const size_t cEnd = plainText.size();

        // Determine if this token belongs to the current line or starts a new visual line
        bool newLine = lines.empty();
        if (!newLine) {
            const PositionedLine& lastLine = lines.back();
            const float lastCy = (lastLine.top + lastLine.bottom) * 0.5f;
            const float curCy = r.y + r.h * 0.5f;
            if (std::abs(curCy - lastCy) > std::max(r.h, lastLine.bottom - lastLine.top) * 0.45f) {
                newLine = true;
            }
        }

        if (newLine) {
            lines.push_back(PositionedLine{
                .top = r.y,
                .bottom = r.Bottom(),
                .firstToken = tokens.size(),
                .tokenCount = 1,
                .charStart = cStart,
                .charEnd = cEnd
            });
        } else {
            PositionedLine& curLine = lines.back();
            curLine.top = std::min(curLine.top, r.y);
            curLine.bottom = std::max(curLine.bottom, r.Bottom());
            curLine.tokenCount += 1;
            curLine.charEnd = cEnd;
        }

        tokens.push_back(PositionedToken{
            .rect = r,
            .charStart = cStart,
            .charEnd = cEnd,
            .lineIndex = lines.size() - 1,
            .fontSize = fs,
            .fontStyle = style,
            .textPadX = padX
        });
    }
};

float MeasureStyledSlice(ICanvas* canvas, std::string_view s, float fs, FontStyle style) noexcept {
    if (s.empty()) return 0.0f;
    if (canvas) {
        return canvas->MeasureTextStyled(s, fs, style).x;
    }
    return static_cast<float>(s.size()) * fs * 0.56f;
}

std::pair<float, float> SubRangePixelBounds(
    ICanvas* canvas,
    const PositionedToken& tok,
    std::string_view fullPlainText,
    size_t subStart,
    size_t subEnd
) noexcept {
    const size_t s0 = std::clamp(subStart, tok.charStart, tok.charEnd);
    const size_t s1 = std::clamp(subEnd, tok.charStart, tok.charEnd);
    const std::string_view tokStr = fullPlainText.substr(tok.charStart, tok.charEnd - tok.charStart);
    const float baseX = tok.rect.x + tok.textPadX;
    const float x0 = std::clamp(
        baseX + MeasureStyledSlice(canvas, tokStr.substr(0, s0 - tok.charStart), tok.fontSize, tok.fontStyle),
        tok.rect.x,
        tok.rect.Right()
    );
    const float x1 = std::clamp(
        baseX + MeasureStyledSlice(canvas, tokStr.substr(0, s1 - tok.charStart), tok.fontSize, tok.fontStyle),
        tok.rect.x,
        tok.rect.Right()
    );
    return {x0, std::max(x0, x1)};
}

size_t HitTestTokenCharOffset(
    ICanvas* canvas,
    const PositionedToken& tok,
    std::string_view fullPlainText,
    float mouseX
) noexcept {
    const std::string_view tokStr = fullPlainText.substr(tok.charStart, tok.charEnd - tok.charStart);
    const float relX = mouseX - (tok.rect.x + tok.textPadX);
    if (relX <= 0.0f) return tok.charStart;
    if (mouseX >= tok.rect.Right()) return tok.charEnd;

    size_t cur = 0;
    float prevW = 0.0f;
    while (cur < tokStr.size()) {
        const size_t next = TextUtil::NextCharBoundary(tokStr, cur);
        const float nextW = MeasureStyledSlice(canvas, tokStr.substr(0, next), tok.fontSize, tok.fontStyle);
        if (relX < (prevW + nextW) * 0.5f) {
            return tok.charStart + cur;
        }
        prevW = nextW;
        cur = next;
    }
    return tok.charEnd;
}

size_t HitTestDocumentCharOffset(
    const DocumentTextMap& map,
    ICanvas* canvas,
    Vec2 mousePos
) noexcept {
    if (map.lines.empty() || map.tokens.empty()) return 0;

    // 1. Find target visual line
    if (mousePos.y < map.lines.front().top) {
        return 0;
    }
    if (mousePos.y > map.lines.back().bottom) {
        return map.plainText.size();
    }

    size_t bestLineIdx = 0;
    float bestVertDist = 1e9f;
    for (size_t li = 0; li < map.lines.size(); ++li) {
        const PositionedLine& ln = map.lines[li];
        if (mousePos.y >= ln.top && mousePos.y <= ln.bottom) {
            bestLineIdx = li;
            break;
        }
        const float dist = (mousePos.y < ln.top) ? (ln.top - mousePos.y) : (mousePos.y - ln.bottom);
        if (dist < bestVertDist) {
            bestVertDist = dist;
            bestLineIdx = li;
        }
    }

    const PositionedLine& ln = map.lines[bestLineIdx];
    if (ln.tokenCount == 0) return ln.charStart;

    const PositionedToken& firstTok = map.tokens[ln.firstToken];
    const PositionedToken& lastTok = map.tokens[ln.firstToken + ln.tokenCount - 1];
    if (mousePos.x <= firstTok.rect.x) return firstTok.charStart;
    if (mousePos.x >= lastTok.rect.Right()) return lastTok.charEnd;

    for (size_t ti = 0; ti < ln.tokenCount; ++ti) {
        const PositionedToken& tok = map.tokens[ln.firstToken + ti];
        if (mousePos.x >= tok.rect.x && mousePos.x <= tok.rect.Right()) {
            return HitTestTokenCharOffset(canvas, tok, map.plainText, mousePos.x);
        }
        if (ti + 1 < ln.tokenCount) {
            const PositionedToken& nextTok = map.tokens[ln.firstToken + ti + 1];
            if (mousePos.x > tok.rect.Right() && mousePos.x < nextTok.rect.x) {
                const float mid = (tok.rect.Right() + nextTok.rect.x) * 0.5f;
                return (mousePos.x < mid) ? tok.charEnd : nextTok.charStart;
            }
        }
    }
    return lastTok.charEnd;
}

size_t FindLineForCharOffset(const DocumentTextMap& map, size_t offset) noexcept {
    if (map.lines.empty()) return 0;
    for (size_t li = 0; li < map.lines.size(); ++li) {
        if (offset >= map.lines[li].charStart && offset <= map.lines[li].charEnd) {
            return li;
        }
        if (offset < map.lines[li].charStart) {
            return (li > 0) ? (li - 1) : 0;
        }
    }
    return map.lines.size() - 1;
}

float GetCursorXForOffset(const DocumentTextMap& map, ICanvas* canvas, size_t offset) noexcept {
    for (const PositionedToken& tok : map.tokens) {
        if (offset >= tok.charStart && offset <= tok.charEnd) {
            return SubRangePixelBounds(canvas, tok, map.plainText, offset, offset).first;
        }
    }
    if (!map.tokens.empty()) {
        if (offset == 0) return map.tokens.front().rect.x;
        return map.tokens.back().rect.Right();
    }
    return 0.0f;
}

bool CharEqualsIgnoreCase(char a, char b) noexcept {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

std::vector<SearchMatchRange> FindSearchMatches(std::string_view text, std::string_view query) {
    std::vector<SearchMatchRange> matches;
    if (query.empty() || text.size() < query.size()) return matches;

    for (size_t i = 0; i + query.size() <= text.size(); ++i) {
        bool ok = true;
        for (size_t j = 0; j < query.size(); ++j) {
            if (!CharEqualsIgnoreCase(text[i + j], query[j])) {
                ok = false;
                break;
            }
        }
        if (ok) {
            matches.push_back(SearchMatchRange{i, i + query.size()});
            i += query.size() - 1;
        }
    }
    return matches;
}

struct RenderOverlayState {
    bool hasSelection{false};
    size_t selMin{0};
    size_t selMax{0};
    bool showCaret{false};
    size_t caretOffset{0};
    const std::vector<SearchMatchRange>* searchMatches{nullptr};
    int activeSearchMatch{-1};
    const std::unordered_map<int, bool>* taskOverrides{nullptr};
};

void DrawTokenSelectionAndSearchHighlights(
    ICanvas& canvas,
    const PositionedToken& tok,
    std::string_view fullPlainText,
    const RenderOverlayState* overlay,
    const UiTheme& theme,
    Vec2 docOrigin
) {
    if (!overlay) return;

    // 1. Search match highlights (tok.rect is in document-local coordinates)
    if (overlay->searchMatches && !overlay->searchMatches->empty()) {
        const auto& matches = *overlay->searchMatches;
        for (size_t mi = 0; mi < matches.size(); ++mi) {
            const SearchMatchRange& m = matches[mi];
            if (m.end <= tok.charStart || m.start >= tok.charEnd) continue;
            const auto [x0, x1] = SubRangePixelBounds(&canvas, tok, fullPlainText, m.start, m.end);
            if (x1 > x0) {
                const bool isActive = (static_cast<int>(mi) == overlay->activeSearchMatch);
                const Color hlBg = isActive
                    ? theme.GetVariant(UiVariant::Warning).bg.WithAlpha(140)
                    : theme.GetVariant(UiVariant::Warning).bg.WithAlpha(65);
                const Color hlBorder = isActive
                    ? theme.GetVariant(UiVariant::Warning).bg
                    : Color::Transparent();
                canvas.DrawRoundRect(
                    Rect(docOrigin.x + x0, docOrigin.y + tok.rect.y, x1 - x0, tok.rect.h),
                    2.0f,
                    hlBg,
                    hlBorder,
                    isActive ? 1.0f : 0.0f
                );
            }
        }
    }

    // 2. Text selection highlight
    if (overlay->hasSelection && overlay->selMax > tok.charStart && overlay->selMin < tok.charEnd) {
        const auto [x0, x1] = SubRangePixelBounds(&canvas, tok, fullPlainText, overlay->selMin, overlay->selMax);
        if (x1 > x0) {
            canvas.DrawRect(
                Rect(docOrigin.x + x0, docOrigin.y + tok.rect.y, x1 - x0, tok.rect.h),
                theme.palette.accent.WithAlpha(85)
            );
        }
    }
}

void DrawTokenCaretIfPresent(
    ICanvas& canvas,
    const PositionedToken& tok,
    std::string_view fullPlainText,
    const RenderOverlayState* overlay,
    const UiTheme& theme,
    Vec2 docOrigin
) {
    if (!overlay || !overlay->showCaret) return;
    if (overlay->caretOffset >= tok.charStart && overlay->caretOffset <= tok.charEnd) {
        const float cx = SubRangePixelBounds(&canvas, tok, fullPlainText, overlay->caretOffset, overlay->caretOffset).first;
        canvas.DrawLine(
            {docOrigin.x + cx, docOrigin.y + tok.rect.y + 1.0f},
            {docOrigin.x + cx, docOrigin.y + tok.rect.Bottom() - 1.0f},
            theme.palette.accent,
            1.5f
        );
    }
}

// --- Phase 2A: Pure Inline Rich-Text Span Flow Layout Engine (in local coordinates) ---

struct InlineAtom {
    InlineKind kind{InlineKind::Text};
    FontStyle style{FontStyle::Regular};
    bool strikethrough{false};
    bool isWhitespace{false};
    std::string_view text{};
    std::string_view url{};
    float width{0.0f};
    float fontSize{13.0f};
    float padX{0.0f};
};

struct FlowLine {
    size_t firstAtom{0};
    size_t atomCount{0};
    float indent{0.0f};
    float contentWidth{0.0f};
};

struct PositionedAtom {
    InlineAtom atom{};
    Rect localRect{};            // Relative to the InlineFlowLayout's origin (0, 0)
    float textLocalY{0.0f};      // Relative to the InlineFlowLayout's origin (0, 0)
    size_t lineIdx{0};
    size_t tokenIndex{SIZE_MAX}; // Index into MarkdownDocumentLayout::textMap.tokens
};

struct InlineFlowLayout {
    float width{0.0f};
    float height{0.0f};
    float lineH{0.0f};
    float scale{1.0f};
    size_t lineCount{0};
    std::vector<PositionedAtom> items{};
};

struct LinkHitRegion {
    Rect localRect{};
    std::string_view url{};
};

InlineFlowLayout BuildInlineFlowLayout(
    ICanvas* canvas,
    const std::vector<InlineSpan>& spans,
    float maxWidth,
    float baseFontSize,
    float codeBaseFontSize,
    float lineHeightScale,
    float firstLineIndent,
    const UiTheme& theme,
    float zoom,
    FontStyle overrideStyle = FontStyle::Regular,
    Align horizontalAlign = Align::Start
) {
    InlineFlowLayout res{};
    if (spans.empty()) return res;

    const float s = theme.Scale(1.0f) * zoom;
    const float rawLineH = canvas ? canvas->GetLineHeight(baseFontSize) : theme.GetLineHeight(baseFontSize);
    const float lineH = rawLineH * std::clamp(lineHeightScale, 0.8f, 2.5f);
    const float maxW = (maxWidth > 0.0f) ? maxWidth : 10000.0f;
    const float codePadX = 4.5f * s;

    res.lineH = lineH;
    res.scale = s;

    // 1. Flatten InlineSpans into initial atomic runs (words, whitespace, code pills)
    std::vector<InlineAtom> rawAtoms;
    rawAtoms.reserve(spans.size() * 3);

    for (const InlineSpan& span : spans) {
        if (span.kind == InlineKind::Code) {
            if (span.text.empty()) continue;
            const float textW = MeasureStyledSlice(canvas, span.text, codeBaseFontSize, FontStyle::Monospace);
            rawAtoms.push_back(InlineAtom{
                .kind = InlineKind::Code,
                .style = FontStyle::Monospace,
                .strikethrough = span.strikethrough,
                .isWhitespace = false,
                .text = span.text,
                .url = {},
                .width = textW + codePadX * 2.0f,
                .fontSize = codeBaseFontSize,
                .padX = codePadX
            });
            continue;
        }

        const FontStyle effStyle = CombineFontStyles(overrideStyle, span.style);
        std::string_view txt = span.text;
        size_t pos = 0;
        while (pos < txt.size()) {
            size_t nextPos = pos;
            const bool isWs = (txt[pos] == ' ');
            if (isWs) {
                while (nextPos < txt.size() && txt[nextPos] == ' ') ++nextPos;
            } else {
                while (nextPos < txt.size() && txt[nextPos] != ' ') ++nextPos;
            }
            const std::string_view slice = txt.substr(pos, nextPos - pos);
            pos = nextPos;

            const float w = MeasureStyledSlice(canvas, slice, baseFontSize, effStyle);
            rawAtoms.push_back(InlineAtom{
                .kind = span.kind,
                .style = effStyle,
                .strikethrough = span.strikethrough,
                .isWhitespace = isWs,
                .text = slice,
                .url = span.url,
                .width = w,
                .fontSize = baseFontSize,
                .padX = 0.0f
            });
        }
    }

    if (rawAtoms.empty()) return res;

    // 2. Pack atoms into visual lines, keeping contiguous non-whitespace clusters (e.g. `(` + `code` + `)`) together
    //    and splitting oversized tokens/code pills at delimiter boundaries so nothing overflows maxW or orphans `)`.
    std::vector<InlineAtom> atoms;
    atoms.reserve(rawAtoms.size() + 4);
    std::vector<FlowLine> flowLines;
    FlowLine curLine{};
    curLine.firstAtom = 0;
    curLine.indent = (horizontalAlign == Align::Start) ? std::max(0.0f, firstLineIndent) : 0.0f;
    float curX = curLine.indent;

    auto measureAtomSlice = [&](const InlineAtom& proto, std::string_view slice) -> float {
        if (proto.kind == InlineKind::Code) {
            return MeasureStyledSlice(canvas, slice, proto.fontSize, FontStyle::Monospace) + proto.padX * 2.0f;
        }
        return MeasureStyledSlice(canvas, slice, proto.fontSize, proto.style);
    };

    auto splitAtomForWidth = [&](const InlineAtom& atom, float availW) -> std::pair<InlineAtom, InlineAtom> {
        std::string_view sv = atom.text;
        size_t fitEnd = 1;
        while (fitEnd < sv.size() && (static_cast<unsigned char>(sv[fitEnd]) & 0xC0) == 0x80) {
            ++fitEnd;
        }
        while (fitEnd < sv.size()) {
            size_t nextCp = fitEnd + 1;
            while (nextCp < sv.size() && (static_cast<unsigned char>(sv[nextCp]) & 0xC0) == 0x80) {
                ++nextCp;
            }
            if (nextCp < sv.size() && measureAtomSlice(atom, sv.substr(0, nextCp)) <= availW + 0.5f) {
                fitEnd = nextCp;
            } else {
                break;
            }
        }

        size_t splitPos = fitEnd;
        if (fitEnd >= 4) {
            const size_t minBreak = std::max<size_t>(1, (fitEnd * 45) / 100);
            for (size_t b = fitEnd; b > minBreak; --b) {
                const char ch = sv[b - 1];
                if (ch == '/' || ch == ':' || ch == '.' || ch == '-' || ch == '_') {
                    splitPos = b;
                    break;
                }
            }
        }

        InlineAtom head = atom;
        head.text = sv.substr(0, splitPos);
        head.width = measureAtomSlice(atom, head.text);

        InlineAtom tail = atom;
        tail.text = sv.substr(splitPos);
        tail.width = measureAtomSlice(atom, tail.text);
        return {head, tail};
    };

    auto finalizeCurrentLine = [&]() {
        while (curLine.atomCount > 0 && atoms[curLine.firstAtom + curLine.atomCount - 1].isWhitespace) {
            curLine.atomCount -= 1;
        }
        float w = 0.0f;
        for (size_t k = 0; k < curLine.atomCount; ++k) {
            w += atoms[curLine.firstAtom + k].width;
        }
        curLine.contentWidth = w;
        if (curLine.atomCount > 0) {
            flowLines.push_back(curLine);
        }
        curLine = FlowLine{
            .firstAtom = atoms.size(),
            .atomCount = 0,
            .indent = 0.0f,
            .contentWidth = 0.0f
        };
        curX = 0.0f;
    };

    auto appendAtomToCurrentLine = [&](const InlineAtom& a) {
        if (curLine.atomCount == 0) {
            curLine.firstAtom = atoms.size();
        }
        atoms.push_back(a);
        curLine.atomCount += 1;
        curX += a.width;
        curLine.contentWidth = curX - curLine.indent;
    };

    size_t idx = 0;
    while (idx < rawAtoms.size()) {
        if (rawAtoms[idx].isWhitespace) {
            if (curLine.atomCount == 0) {
                ++idx;
                continue;
            }
            appendAtomToCurrentLine(rawAtoms[idx]);
            ++idx;
            continue;
        }

        size_t clusterEnd = idx + 1;
        float clusterW = rawAtoms[idx].width;
        while (clusterEnd < rawAtoms.size() && !rawAtoms[clusterEnd].isWhitespace) {
            clusterW += rawAtoms[clusterEnd].width;
            ++clusterEnd;
        }

        if (curLine.atomCount > 0 && curX + clusterW > maxW + 0.5f) {
            finalizeCurrentLine();
        }

        for (size_t k = idx; k < clusterEnd; ++k) {
            InlineAtom curAtom = rawAtoms[k];
            const bool hasTrailingPunct =
                (k + 1 == clusterEnd - 1) &&
                (rawAtoms[k + 1].kind == InlineKind::Text) &&
                (rawAtoms[k + 1].text.size() <= 2) &&
                (rawAtoms[k + 1].width <= maxW * 0.25f);
            const float trailingSuffixW = hasTrailingPunct ? rawAtoms[k + 1].width : 0.0f;

            while (curX + curAtom.width + trailingSuffixW > maxW + 0.5f) {
                const bool onlyOpeningPrefixOnLine =
                    (curLine.atomCount == 1) &&
                    (atoms[curLine.firstAtom].kind == InlineKind::Text) &&
                    (atoms[curLine.firstAtom].text.size() <= 2) &&
                    (atoms[curLine.firstAtom].width <= maxW * 0.25f);

                if (curLine.atomCount > 0 && !onlyOpeningPrefixOnLine) {
                    finalizeCurrentLine();
                    continue;
                }

                if (curAtom.text.size() > 1) {
                    const float availForHead = std::max(8.0f, maxW - curX);
                    auto [head, tail] = splitAtomForWidth(curAtom, availForHead);
                    appendAtomToCurrentLine(head);
                    finalizeCurrentLine();
                    curAtom = tail;
                } else {
                    if (curLine.atomCount > 0) {
                        finalizeCurrentLine();
                    }
                    break;
                }
            }

            appendAtomToCurrentLine(curAtom);
        }

        idx = clusterEnd;
    }

    if (curLine.atomCount > 0) {
        finalizeCurrentLine();
    }
    if (flowLines.empty()) return res;

    // 3. Materialize PositionedAtoms in flow-local coordinates (0, 0) with pre-measured glyph heights
    const float codeGlyphH = canvas ? canvas->MeasureTextStyled("Mg", codeBaseFontSize, FontStyle::Monospace).y : codeBaseFontSize;
    std::array<float, 5> cachedGlyphH{};
    for (uint8_t st = 0; st < 5; ++st) {
        cachedGlyphH[st] = canvas
            ? canvas->MeasureTextStyled("Mg", baseFontSize, static_cast<FontStyle>(st)).y
            : baseFontSize;
    }

    float maxLineW = 0.0f;
    res.items.reserve(atoms.size());

    for (size_t li = 0; li < flowLines.size(); ++li) {
        const FlowLine& fl = flowLines[li];
        const float lineY = static_cast<float>(li) * lineH;
        maxLineW = std::max(maxLineW, fl.indent + fl.contentWidth);

        float drawX = fl.indent;
        if (horizontalAlign == Align::Center && maxWidth > 0.0f) {
            drawX = std::max(0.0f, (maxWidth - fl.contentWidth) * 0.5f);
        } else if (horizontalAlign == Align::End && maxWidth > 0.0f) {
            drawX = std::max(0.0f, maxWidth - fl.contentWidth);
        }

        for (size_t k = 0; k < fl.atomCount; ++k) {
            const InlineAtom& atom = atoms[fl.firstAtom + k];
            if (atom.kind == InlineKind::Code) {
                const float pillH = std::max(atom.fontSize + 4.0f * s, lineH - 2.0f * s);
                const Rect pillRect(
                    drawX,
                    lineY + (lineH - pillH) * 0.5f,
                    atom.width,
                    pillH
                );
                const float codeTextY = pillRect.y + (pillRect.h - ((codeGlyphH > 0.0f) ? codeGlyphH : atom.fontSize)) * 0.5f;
                res.items.push_back(PositionedAtom{
                    .atom = atom,
                    .localRect = pillRect,
                    .textLocalY = codeTextY,
                    .lineIdx = li,
                    .tokenIndex = SIZE_MAX
                });
                drawX += atom.width;
                continue;
            }

            const Rect wordRect(drawX, lineY, atom.width, lineH);
            const uint8_t stIdx = std::min<uint8_t>(4, static_cast<uint8_t>(atom.style));
            const float glyphH = cachedGlyphH[stIdx];
            const float textY = lineY + (lineH - ((glyphH > 0.0f) ? glyphH : atom.fontSize)) * 0.5f;

            res.items.push_back(PositionedAtom{
                .atom = atom,
                .localRect = wordRect,
                .textLocalY = textY,
                .lineIdx = li,
                .tokenIndex = SIZE_MAX
            });
            drawX += atom.width;
        }
    }

    res.width = maxLineW;
    res.height = static_cast<float>(flowLines.size()) * lineH;
    res.lineCount = flowLines.size();
    return res;
}

constexpr Rect OffsetRect(Rect r, Vec2 delta) noexcept {
    return Rect(r.x + delta.x, r.y + delta.y, r.w, r.h);
}

void CommitInlineFlowToDocument(
    InlineFlowLayout& flow,
    Vec2 flowDocOrigin,
    DocumentTextMap& textMap,
    std::vector<LinkHitRegion>& linkRegions
) {
    if (flow.items.empty()) return;
    size_t currentLine = flow.items.front().lineIdx;
    for (PositionedAtom& item : flow.items) {
        if (item.lineIdx != currentLine) {
            textMap.AppendNewline();
            currentLine = item.lineIdx;
        }
        const Rect docRect = OffsetRect(item.localRect, flowDocOrigin);
        item.tokenIndex = textMap.tokens.size();
        if (item.atom.kind == InlineKind::Code) {
            textMap.AddToken(docRect, item.atom.text, item.atom.fontSize, FontStyle::Monospace, item.atom.padX);
        } else {
            textMap.AddToken(docRect, item.atom.text, item.atom.fontSize, item.atom.style, 0.0f);
            if (item.atom.kind == InlineKind::Link) {
                linkRegions.push_back(LinkHitRegion{
                    .localRect = docRect,
                    .url = item.atom.url
                });
            }
        }
    }
    textMap.AppendNewline();
}

void RenderInlineFlowLayout(
    ICanvas& canvas,
    const InlineFlowLayout& flow,
    Vec2 flowScreenOrigin,
    Vec2 docScreenOrigin,
    Color baseColor,
    const DocumentTextMap& textMap,
    const RenderOverlayState* overlay,
    const UiTheme& theme,
    Vec2 localMousePos
) {
    const float s = flow.scale;
    for (const PositionedAtom& item : flow.items) {
        const InlineAtom& atom = item.atom;
        const Rect screenRect = OffsetRect(item.localRect, flowScreenOrigin);
        const float screenTextY = flowScreenOrigin.y + item.textLocalY;
        const PositionedToken* tok = (item.tokenIndex < textMap.tokens.size())
            ? &textMap.tokens[item.tokenIndex]
            : nullptr;

        if (atom.kind == InlineKind::Code) {
            canvas.DrawRoundRect(
                screenRect,
                4.0f * s,
                theme.palette.surfaceElevated,
                theme.palette.borderSubtle,
                1.0f
            );
            if (tok) {
                DrawTokenSelectionAndSearchHighlights(canvas, *tok, textMap.plainText, overlay, theme, docScreenOrigin);
            }
            canvas.DrawTextStyled(
                atom.text,
                {screenRect.x + atom.padX, screenTextY},
                atom.fontSize,
                theme.palette.accent,
                FontStyle::Monospace
            );
            if (atom.strikethrough) {
                const float sy = screenRect.y + screenRect.h * 0.52f;
                canvas.DrawLine(
                    {screenRect.x + atom.padX, sy},
                    {screenRect.Right() - atom.padX, sy},
                    theme.palette.accent.WithAlpha(180),
                    1.2f
                );
            }
            if (tok) {
                DrawTokenCaretIfPresent(canvas, *tok, textMap.plainText, overlay, theme, docScreenOrigin);
            }
            continue;
        }

        if (tok) {
            DrawTokenSelectionAndSearchHighlights(canvas, *tok, textMap.plainText, overlay, theme, docScreenOrigin);
        }

        const bool isLink = (atom.kind == InlineKind::Link);
        const bool wordHovered = isLink && tok && tok->rect.Contains(localMousePos);
        const Color drawCol = isLink ? theme.palette.accent : baseColor;

        if (!atom.isWhitespace) {
            canvas.DrawTextStyled(atom.text, {screenRect.x, screenTextY}, atom.fontSize, drawCol, atom.style);
        }
        if (isLink) {
            const float uy = screenRect.Bottom() - 2.0f * s;
            const Color uCol = wordHovered ? theme.palette.accent : theme.palette.accent.WithAlpha(140);
            canvas.DrawLine({screenRect.x, uy}, {screenRect.Right(), uy}, uCol, wordHovered ? 1.5f : 1.0f);
        }
        if (atom.strikethrough) {
            const float sy = screenRect.y + screenRect.h * 0.52f;
            canvas.DrawLine({screenRect.x, sy}, {screenRect.Right(), sy}, drawCol.WithAlpha(180), 1.2f);
        }
        if (tok) {
            DrawTokenCaretIfPresent(canvas, *tok, textMap.plainText, overlay, theme, docScreenOrigin);
        }
    }
}

float HeadingFontSize(uint8_t level, const MarkdownOptions& opts, const UiTheme& theme, float zoom) noexcept {
    float base = 0.0f;
    switch (level) {
        case 1: base = theme.GetFontSize(TextRole::HeadlineLarge); break;
        case 2: base = theme.GetFontSize(TextRole::HeadlineSmall); break;
        case 3: base = theme.GetFontSize(TextRole::TitleLarge); break;
        case 4: base = theme.GetFontSize(TextRole::TitleMedium); break;
        case 5: base = theme.GetFontSize(TextRole::LabelLarge); break;
        default: base = theme.GetFontSize(TextRole::LabelMedium); break;
    }
    return base * std::clamp(opts.headingScale, 0.5f, 2.5f) * zoom;
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

// --- Phase 2B: Retained Document Layout (in document-local coordinates (0, 0)) ---

struct CopyBtnHitRegion {
    Rect boxLocalRect{};
    float headerH{0.0f};
    int codeBlockIndex{-1};
    std::string_view rawCode{};
};

struct TaskHitRegion {
    Rect localHitRect{};
    int taskIndex{-1};
    bool defaultChecked{false};
};

struct CodeRenderSpan {
    std::string_view text{};
    Color color{};
    float localOffsetX{0.0f};
};

struct CodeVisualLineLayout {
    size_t logicalLine{0};
    bool isFirstSlice{true};
    bool isLastSlice{true};
    std::string_view sliceStr{};
    float localY{0.0f};
    float textLocalY{0.0f};
    size_t tokenIndex{SIZE_MAX};
    std::vector<CodeRenderSpan> renderSpans{};
};

struct BlockLayout {
    BlockKind kind{BlockKind::Paragraph};
    size_t astBlockIndex{0};
    Rect localBounds{}; // Bounding box in document-local coordinates (0, 0), used for O(1) frustum culling

    InlineFlowLayout flow{};
    Vec2 flowLocalOrigin{};
    bool hasHeadingDivider{false};
    float dividerLocalY{0.0f};

    float boxRadius{0.0f};
    float padX{0.0f};
    float padY{0.0f};
    float headerH{0.0f};
    Rect markerLocalRect{};

    std::vector<CodeVisualLineLayout> codeVisualLines{};
    float gutterW{0.0f};
    float codeStartX{0.0f};
    float spaceW{0.0f};
    float codeFs{13.0f};
    float codeLineH{18.0f};
    float bodyH{0.0f};

    Rect imgLocalRect{};
    float imgRadius{0.0f};

    std::vector<float> colWidths{};
    std::vector<float> colX{};
    std::vector<float> rowHeights{};
    std::vector<std::vector<InlineFlowLayout>> cellFlows{};
    std::vector<std::vector<Vec2>> cellFlowOrigins{};
};

struct MarkdownDocumentLayout {
    uint64_t layoutKey{0};
    float totalWidth{0.0f};
    float totalHeight{0.0f};
    float effZoom{1.0f};
    float scale{1.0f};
    Rect contentLocalBounds{};
    DocumentTextMap textMap{};
    std::vector<BlockLayout> blocks{};
    std::vector<LinkHitRegion> linkRegions{};
    std::vector<CopyBtnHitRegion> copyRegions{};
    std::vector<TaskHitRegion> taskRegions{};
};

struct MarkdownPassResult {
    float totalWidth{0.0f};
    float totalHeight{0.0f};
    std::string_view hoveredLink{};
    Rect hoveredLinkRect{};
    int hoveredCopyBlock{-1};
    std::string_view hoveredCopyCode{};
    int hoveredTaskIndex{-1};
    bool hoveredTaskCurrentState{false};
};

// Fast single-pass intrinsic cell width measurement (avoids building full FlowLines just for column sizing)
std::pair<float, float> MeasureCellIntrinsicWidths(
    ICanvas* canvas,
    const std::vector<InlineSpan>& spans,
    float bodyFs,
    float inlineCodeFs,
    FontStyle rowBaseStyle,
    float scale
) noexcept {
    float minTokenW = 0.0f;
    float unwrappedW = 0.0f;
    const float codePadW = 9.0f * scale;

    for (const auto& sp : spans) {
        if (sp.kind == InlineKind::Code) {
            if (sp.text.empty()) continue;
            const float w = MeasureStyledSlice(canvas, sp.text, inlineCodeFs, FontStyle::Monospace) + codePadW;
            minTokenW = std::max(minTokenW, w);
            unwrappedW += w;
        } else {
            const FontStyle effStyle = CombineFontStyles(rowBaseStyle, sp.style);
            unwrappedW += MeasureStyledSlice(canvas, sp.text, bodyFs, effStyle);
            std::string_view sv = sp.text;
            size_t i = 0;
            while (i < sv.size()) {
                while (i < sv.size() && (sv[i] == ' ' || sv[i] == '\t' || sv[i] == '\n')) ++i;
                size_t start = i;
                while (i < sv.size() && sv[i] != ' ' && sv[i] != '\t' && sv[i] != '\n') ++i;
                if (i > start) {
                    minTokenW = std::max(minTokenW, MeasureStyledSlice(canvas, sv.substr(start, i - start), bodyFs, effStyle));
                }
            }
        }
    }
    return {minTokenW, unwrappedW};
}

uint64_t ComputeMarkdownLayoutKey(
    uint64_t docSourceHash,
    float outerWidth,
    float effZoom,
    const MarkdownOptions& opts,
    const UiTheme& theme,
    bool hasCanvasMetrics
) noexcept {
    const int32_t qWidth = static_cast<int32_t>(std::round(outerWidth * 2.0f));
    const int32_t qZoom = static_cast<int32_t>(std::round(effZoom * 1000.0f));
    return detail::HashArgs(
        docSourceHash,
        qWidth,
        qZoom,
        hasCanvasMetrics,
        theme.mode,
        theme.density,
        theme.Scale(100.0f),
        theme.palette.textPrimary.ToUint32(),
        theme.palette.accent.ToUint32(),
        opts.bodyFontSize.value_or(-1.0f),
        opts.headingScale,
        opts.inlineCodeFontSize.value_or(-1.0f),
        opts.codeFontSize.value_or(-1.0f),
        opts.lineHeightScale,
        opts.documentPadding,
        opts.maxContentWidth.value_or(-1.0f),
        opts.blockGap,
        opts.paragraphIndent,
        opts.listIndent,
        opts.blockPadding,
        opts.blockRadius,
        opts.headingDividers,
        opts.codeLineNumbers,
        opts.syntaxHighlight,
        opts.codePalette.has_value() ? opts.codePalette->editorBg.ToUint32() : 0u,
        opts.codePalette.has_value() ? opts.codePalette->keyword.ToUint32() : 0u,
        opts.imageMaxHeight,
        opts.imageRadius
    );
}

std::shared_ptr<const MarkdownDocumentLayout> BuildMarkdownDocumentLayout(
    ICanvas* canvas,
    float outerWidth,
    const ParsedMarkdownDoc& doc,
    const MarkdownOptions& opts,
    const UiTheme& theme,
    float zoom,
    uint64_t layoutKey
) {
    auto layoutPtr = std::make_shared<MarkdownDocumentLayout>();
    MarkdownDocumentLayout& out = *layoutPtr;
    out.layoutKey = layoutKey;

    const float effZoom = std::clamp(zoom, 0.5f, 3.0f);
    const float s = theme.Scale(1.0f) * effZoom;
    const float docPad = std::max(0.0f, opts.documentPadding) * s;

    out.effZoom = effZoom;
    out.scale = s;

    float availW = std::max(120.0f, outerWidth - docPad * 2.0f);
    float startX = docPad;
    if (opts.maxContentWidth.has_value() && *opts.maxContentWidth > 0.0f) {
        const float maxColW = *opts.maxContentWidth * s;
        if (availW > maxColW) {
            startX += (availW - maxColW) * 0.5f;
            availW = maxColW;
        }
    }

    const Rect contentBounds(startX, docPad, availW, 0.0f);
    out.contentLocalBounds = contentBounds;

    const float bodyFs = (opts.bodyFontSize.has_value() ? theme.Scale(*opts.bodyFontSize) : theme.GetFontSize(TextRole::BodyMedium)) * effZoom;
    const float codeFs = (opts.codeFontSize.has_value() ? theme.Scale(*opts.codeFontSize) : theme.GetFontSize(TextRole::CodeMedium)) * effZoom;
    const float inlineCodeFs = opts.inlineCodeFontSize.has_value()
        ? (theme.Scale(*opts.inlineCodeFontSize) * effZoom)
        : (bodyFs * 0.92f);
    const float lhScale = std::clamp(opts.lineHeightScale, 0.8f, 2.5f);
    const float codeLineH = (canvas ? canvas->GetLineHeight(codeFs) : theme.GetLineHeight(codeFs)) * lhScale;
    const float gap = opts.blockGap * s;
    const float blkPad = std::max(4.0f, opts.blockPadding) * s;
    const float blkRad = std::max(0.0f, opts.blockRadius) * s;
    const float paraIndent = std::max(0.0f, opts.paragraphIndent) * s;
    const float listStep = std::max(8.0f, opts.listIndent) * s;

    float curY = contentBounds.y;
    bool firstBlock = true;
    out.blocks.reserve(doc.blocks.size());

    for (size_t bIdx = 0; bIdx < doc.blocks.size(); ++bIdx) {
        const MarkdownBlock& blk = doc.blocks[bIdx];
        if (!firstBlock) {
            curY += (blk.kind == BlockKind::Heading) ? (gap * 1.25f) : gap;
        }
        firstBlock = false;

        BlockLayout bl{};
        bl.kind = blk.kind;
        bl.astBlockIndex = bIdx;

        switch (blk.kind) {
            case BlockKind::Divider: {
                const float h = 8.0f * s;
                bl.localBounds = Rect(contentBounds.x, curY, availW, h);
                curY += h;
                out.totalWidth = std::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::Heading: {
                const float hFs = HeadingFontSize(blk.headingLevel, opts, theme, effZoom);
                const float headingInlineCodeFs = opts.inlineCodeFontSize.has_value()
                    ? (inlineCodeFs * (hFs / std::max(1.0f, bodyFs)) * 0.85f)
                    : (hFs * 0.75f);
                const float startY = curY;
                bl.flowLocalOrigin = {contentBounds.x, curY};
                bl.flow = BuildInlineFlowLayout(
                    canvas,
                    blk.spans,
                    availW,
                    hFs,
                    headingInlineCodeFs,
                    lhScale,
                    0.0f,
                    theme,
                    effZoom,
                    FontStyle::Bold
                );
                CommitInlineFlowToDocument(bl.flow, bl.flowLocalOrigin, out.textMap, out.linkRegions);
                curY += bl.flow.height;
                if (opts.headingDividers && blk.headingLevel <= 2) {
                    curY += 6.0f * s;
                    bl.hasHeadingDivider = true;
                    bl.dividerLocalY = curY;
                    curY += 2.0f * s;
                }
                bl.localBounds = Rect(contentBounds.x, startY, availW, curY - startY);
                out.totalWidth = std::max(out.totalWidth, bl.flow.width);
                break;
            }

            case BlockKind::Paragraph: {
                const float startY = curY;
                bl.flowLocalOrigin = {contentBounds.x, curY};
                bl.flow = BuildInlineFlowLayout(
                    canvas,
                    blk.spans,
                    availW,
                    bodyFs,
                    inlineCodeFs,
                    lhScale,
                    paraIndent,
                    theme,
                    effZoom,
                    FontStyle::Regular
                );
                CommitInlineFlowToDocument(bl.flow, bl.flowLocalOrigin, out.textMap, out.linkRegions);
                curY += bl.flow.height;
                bl.localBounds = Rect(contentBounds.x, startY, availW, bl.flow.height);
                out.totalWidth = std::max(out.totalWidth, bl.flow.width);
                break;
            }

            case BlockKind::Blockquote: {
                const float padX = blkPad + 2.0f * s;
                const float padY = blkPad * 0.7f;
                const float innerW = std::max(40.0f, availW - padX * 2.0f);
                bl.flowLocalOrigin = {contentBounds.x + padX, curY + padY};
                bl.flow = BuildInlineFlowLayout(
                    canvas,
                    blk.spans,
                    innerW,
                    bodyFs,
                    inlineCodeFs,
                    lhScale,
                    0.0f,
                    theme,
                    effZoom,
                    FontStyle::Italic
                );
                CommitInlineFlowToDocument(bl.flow, bl.flowLocalOrigin, out.textMap, out.linkRegions);
                const float boxH = bl.flow.height + padY * 2.0f;
                bl.localBounds = Rect(contentBounds.x, curY, availW, boxH);
                bl.boxRadius = blkRad;
                curY += boxH;
                out.totalWidth = std::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::Alert: {
                const float padX = blkPad + 4.0f * s;
                const float padY = blkPad * 0.85f;
                const float headerH = 20.0f * s;
                const float innerW = std::max(40.0f, availW - padX * 2.0f);
                bl.padX = padX;
                bl.padY = padY;
                bl.headerH = headerH;
                bl.boxRadius = blkRad;
                bl.flowLocalOrigin = {contentBounds.x + padX, curY + padY + headerH + 4.0f * s};
                bl.flow = BuildInlineFlowLayout(
                    canvas,
                    blk.spans,
                    innerW,
                    bodyFs,
                    inlineCodeFs,
                    lhScale,
                    0.0f,
                    theme,
                    effZoom,
                    FontStyle::Regular
                );
                CommitInlineFlowToDocument(bl.flow, bl.flowLocalOrigin, out.textMap, out.linkRegions);
                const float boxH = padY * 2.0f + headerH + 4.0f * s + bl.flow.height;
                bl.localBounds = Rect(contentBounds.x, curY, availW, boxH);
                curY += boxH;
                out.totalWidth = std::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::ListItem: {
                const float indent = static_cast<float>(blk.indentLevel) * listStep + (listStep + 4.0f * s);
                const float itemX = contentBounds.x + indent;
                const float itemW = std::max(40.0f, availW - indent);
                const float lineH = (canvas ? canvas->GetLineHeight(bodyFs) : theme.GetLineHeight(bodyFs)) * lhScale;
                const float markerX = itemX - 18.0f * s;

                if (blk.taskState >= 0) {
                    const float cbSz = 14.0f * s;
                    bl.markerLocalRect = Rect(markerX, curY + (lineH - cbSz) * 0.5f, cbSz, cbSz);
                    out.taskRegions.push_back(TaskHitRegion{
                        .localHitRect = bl.markerLocalRect.Inset(-3.0f * s),
                        .taskIndex = blk.taskIndex,
                        .defaultChecked = (blk.taskState == 1)
                    });
                } else if (blk.isOrderedList) {
                    bl.markerLocalRect = Rect(markerX - 2.0f * s, curY + (lineH - bodyFs) * 0.5f, 16.0f * s, bodyFs);
                } else {
                    const float dotSz = 5.0f * s;
                    bl.markerLocalRect = Rect(markerX + 4.0f * s, curY + (lineH - dotSz) * 0.5f, dotSz, dotSz);
                }

                bl.flowLocalOrigin = {itemX, curY};
                bl.flow = BuildInlineFlowLayout(
                    canvas,
                    blk.spans,
                    itemW,
                    bodyFs,
                    inlineCodeFs,
                    lhScale,
                    0.0f,
                    theme,
                    effZoom,
                    FontStyle::Regular
                );
                CommitInlineFlowToDocument(bl.flow, bl.flowLocalOrigin, out.textMap, out.linkRegions);
                bl.localBounds = Rect(contentBounds.x, curY, availW, std::max(lineH, bl.flow.height));
                curY += bl.flow.height;
                out.totalWidth = std::max(out.totalWidth, indent + bl.flow.width);
                break;
            }

            case BlockKind::CodeBlock: {
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
                const float padY = blkPad * 0.85f;
                const float padX = blkPad;
                const float gutterW = opts.codeLineNumbers ? (36.0f * s) : 0.0f;
                const float codeStartX = contentBounds.x + gutterW + padX;
                const float codeAvailW = std::max(48.0f * s, availW - gutterW - padX * 2.0f);
                const float spaceW = std::max(1.0f, MeasureStyledSlice(canvas, " ", codeFs, FontStyle::Monospace));
                const float codeGlyphH = canvas ? canvas->MeasureTextStyled("Mg", codeFs, FontStyle::Monospace).y : codeFs;

                SyntaxConfig synCfg = SyntaxConfig::ForLanguage(blk.codeLang);
                if (opts.codePalette.has_value()) {
                    synCfg.palette = *opts.codePalette;
                }
                const Color defaultCodeCol = opts.codePalette.has_value()
                    ? opts.codePalette->identifier
                    : theme.palette.textPrimary;

                std::vector<TextSpan> syntaxSpans;
                bool inBlockComment = false;

                bl.codeVisualLines.reserve(codeLines.size());

                for (size_t ln = 0; ln < codeLines.size(); ++ln) {
                    const std::string_view lineStr = codeLines[ln];
                    syntaxSpans.clear();
                    if (opts.syntaxHighlight && !lineStr.empty()) {
                        TextArea::HighlightLine(
                            lineStr,
                            ln,
                            synCfg,
                            syntaxSpans,
                            theme,
                            opts.codePalette.has_value() ? &(*opts.codePalette) : nullptr,
                            &inBlockComment
                        );
                    }

                    auto emitSlice = [&](size_t startCol, size_t len, bool isFirst, bool isLast) {
                        const size_t vIdx = bl.codeVisualLines.size();
                        const float ly = curY + headerH + padY + static_cast<float>(vIdx) * codeLineH;
                        const float textY = ly + (codeLineH - ((codeGlyphH > 0.0f) ? codeGlyphH : codeFs)) * 0.5f;
                        const std::string_view sliceStr = lineStr.substr(startCol, len);
                        const float lineW = std::max(spaceW, MeasureStyledSlice(canvas, sliceStr, codeFs, FontStyle::Monospace));
                        const Rect lineRect(codeStartX, ly, lineW, codeLineH);

                        const size_t tokIdx = out.textMap.tokens.size();
                        if (!sliceStr.empty()) {
                            out.textMap.AddToken(lineRect, sliceStr, codeFs, FontStyle::Monospace, 0.0f);
                        } else {
                            out.textMap.AddToken(lineRect, " ", codeFs, FontStyle::Monospace, 0.0f);
                        }
                        if (isLast) {
                            out.textMap.AppendNewline();
                        }

                        CodeVisualLineLayout vline{};
                        vline.logicalLine = ln;
                        vline.isFirstSlice = isFirst;
                        vline.isLastSlice = isLast;
                        vline.sliceStr = sliceStr;
                        vline.localY = ly;
                        vline.textLocalY = textY;
                        vline.tokenIndex = tokIdx;

                        if (!sliceStr.empty()) {
                            if (syntaxSpans.empty()) {
                                vline.renderSpans.push_back(CodeRenderSpan{
                                    .text = sliceStr,
                                    .color = defaultCodeCol,
                                    .localOffsetX = 0.0f
                                });
                            } else {
                                const size_t sliceEnd = startCol + len;
                                float spanOffX = 0.0f;
                                for (const TextSpan& sp : syntaxSpans) {
                                    const size_t spStart = static_cast<size_t>(sp.startCol);
                                    const size_t spEnd = spStart + static_cast<size_t>(sp.length);
                                    const size_t segStart = std::max(spStart, startCol);
                                    const size_t segEnd = std::min({spEnd, sliceEnd, lineStr.size()});
                                    if (segEnd > segStart) {
                                        const std::string_view tok = lineStr.substr(segStart, segEnd - segStart);
                                        bool allSpaces = true;
                                        for (char ch : tok) {
                                            if (ch != ' ' && ch != '\t') {
                                                allSpaces = false;
                                                break;
                                            }
                                        }
                                        if (!allSpaces) {
                                            vline.renderSpans.push_back(CodeRenderSpan{
                                                .text = tok,
                                                .color = sp.color,
                                                .localOffsetX = spanOffX
                                            });
                                        }
                                        spanOffX += MeasureStyledSlice(canvas, tok, codeFs, FontStyle::Monospace);
                                    }
                                }
                            }
                        }

                        bl.codeVisualLines.push_back(std::move(vline));
                    };

                    if (lineStr.empty()) {
                        emitSlice(0, 0, true, true);
                        continue;
                    }
                    const float fullW = MeasureStyledSlice(canvas, lineStr, codeFs, FontStyle::Monospace);
                    if (fullW <= codeAvailW + 0.5f) {
                        emitSlice(0, lineStr.size(), true, true);
                        continue;
                    }

                    size_t pos = 0;
                    while (pos < lineStr.size()) {
                        const size_t approxChars = std::max<size_t>(1, static_cast<size_t>(std::floor(codeAvailW / spaceW)));
                        size_t fitEnd = std::min(lineStr.size(), pos + approxChars);
                        while (fitEnd < lineStr.size() && (static_cast<unsigned char>(lineStr[fitEnd]) & 0xC0) == 0x80) {
                            ++fitEnd;
                        }
                        while (fitEnd < lineStr.size()) {
                            size_t nextCp = fitEnd + 1;
                            while (nextCp < lineStr.size() && (static_cast<unsigned char>(lineStr[nextCp]) & 0xC0) == 0x80) {
                                ++nextCp;
                            }
                            if (MeasureStyledSlice(canvas, lineStr.substr(pos, nextCp - pos), codeFs, FontStyle::Monospace) <= codeAvailW + 0.5f) {
                                fitEnd = nextCp;
                            } else {
                                break;
                            }
                        }
                        while (fitEnd > pos + 1 && MeasureStyledSlice(canvas, lineStr.substr(pos, fitEnd - pos), codeFs, FontStyle::Monospace) > codeAvailW + 0.5f) {
                            --fitEnd;
                            while (fitEnd > pos + 1 && (static_cast<unsigned char>(lineStr[fitEnd]) & 0xC0) == 0x80) {
                                --fitEnd;
                            }
                        }

                        size_t splitEnd = fitEnd;
                        if (fitEnd < lineStr.size() && (fitEnd - pos) >= 10) {
                            const size_t minBreak = pos + ((fitEnd - pos) * 60) / 100;
                            for (size_t k = fitEnd; k > minBreak; --k) {
                                const char ch = lineStr[k - 1];
                                if (ch == ' ' || ch == ',' || ch == ';' || ch == '(' || ch == '{' || ch == '[') {
                                    splitEnd = k;
                                    break;
                                }
                            }
                        }
                        if (splitEnd <= pos) {
                            splitEnd = std::min(lineStr.size(), pos + 1);
                            while (splitEnd < lineStr.size() && (static_cast<unsigned char>(lineStr[splitEnd]) & 0xC0) == 0x80) {
                                ++splitEnd;
                            }
                        }

                        emitSlice(pos, splitEnd - pos, (pos == 0), (splitEnd >= lineStr.size()));
                        pos = splitEnd;
                    }
                }

                const float bodyH = static_cast<float>(bl.codeVisualLines.size()) * codeLineH + padY * 2.0f;
                const float totalBoxH = headerH + bodyH;
                bl.localBounds = Rect(contentBounds.x, curY, availW, totalBoxH);
                bl.headerH = headerH;
                bl.padX = padX;
                bl.padY = padY;
                bl.boxRadius = blkRad;
                bl.gutterW = gutterW;
                bl.codeStartX = codeStartX;
                bl.spaceW = spaceW;
                bl.codeFs = codeFs;
                bl.codeLineH = codeLineH;
                bl.bodyH = bodyH;

                out.copyRegions.push_back(CopyBtnHitRegion{
                    .boxLocalRect = bl.localBounds,
                    .headerH = headerH,
                    .codeBlockIndex = blk.codeBlockIndex,
                    .rawCode = blk.rawText
                });

                curY += totalBoxH;
                out.totalWidth = std::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::ImageBlock: {
                const float startY = curY;
                Vec2 nat = canvas ? canvas->GetImageSize(blk.rawText) : Vec2{0.0f, 0.0f};
                if (nat.x <= 0.0f || nat.y <= 0.0f) {
                    nat = {320.0f, 160.0f};
                }
                const Vec2 scaledNat = { nat.x * s, nat.y * s };
                const float maxH = opts.imageMaxHeight * s;
                const float scaleFactor = std::min({1.0f, availW / scaledNat.x, maxH / scaledNat.y});
                const float imgW = std::max(40.0f, scaledNat.x * scaleFactor);
                const float imgH = std::max(30.0f, scaledNat.y * scaleFactor);
                const float imgX = contentBounds.x + (availW - imgW) * 0.5f;
                bl.imgLocalRect = Rect(imgX, curY, imgW, imgH);
                bl.imgRadius = opts.imageRadius * s;
                curY += imgH;

                if (!blk.spans.empty()) {
                    const float capFs = theme.GetFontSize(TextRole::LabelSmall) * effZoom;
                    const float capCodeFs = capFs * 0.92f;
                    curY += 4.0f * s;
                    bl.flowLocalOrigin = {contentBounds.x, curY};
                    bl.flow = BuildInlineFlowLayout(
                        canvas,
                        blk.spans,
                        availW,
                        capFs,
                        capCodeFs,
                        lhScale,
                        0.0f,
                        theme,
                        effZoom,
                        FontStyle::Italic,
                        Align::Center
                    );
                    CommitInlineFlowToDocument(bl.flow, bl.flowLocalOrigin, out.textMap, out.linkRegions);
                    curY += bl.flow.height;
                }
                bl.localBounds = Rect(contentBounds.x, startY, availW, curY - startY);
                out.totalWidth = std::max(out.totalWidth, availW);
                break;
            }

            case BlockKind::Table: {
                if (blk.tableRows.empty()) break;
                size_t numCols = std::max<size_t>(1, blk.tableAligns.size());
                for (const auto& r : blk.tableRows) {
                    numCols = std::max(numCols, r.size());
                }

                const float cellPadX = std::clamp(availW / (static_cast<float>(numCols) * 14.0f), 4.5f * s, 10.0f * s);
                const float cellPadY = std::clamp(cellPadX * 0.7f, 4.5f * s, 7.0f * s);

                // 1. Single-pass intrinsic width measurement per cell
                std::vector<float> minTextW(numCols, 24.0f * s);
                std::vector<float> naturalTextW(numCols, 24.0f * s);

                for (size_t r = 0; r < blk.tableRows.size(); ++r) {
                    const FontStyle rowBaseStyle = (r == 0) ? FontStyle::Bold : FontStyle::Regular;
                    const auto& rowCells = blk.tableRows[r];
                    for (size_t c = 0; c < numCols && c < rowCells.size(); ++c) {
                        const auto [cellMinTokW, cellNatW] = MeasureCellIntrinsicWidths(
                            canvas,
                            rowCells[c].spans,
                            bodyFs,
                            inlineCodeFs,
                            rowBaseStyle,
                            s
                        );
                        minTextW[c] = std::max(minTextW[c], cellMinTokW);
                        naturalTextW[c] = std::max(naturalTextW[c], cellNatW);
                    }
                }

                float sumMinTextW = 0.0f;
                for (size_t c = 0; c < numCols; ++c) {
                    minTextW[c] = std::min(minTextW[c], naturalTextW[c]);
                    sumMinTextW += minTextW[c];
                }

                const float totalPadW = static_cast<float>(numCols) * cellPadX * 2.0f;
                const float availTextW = std::max(32.0f * s, availW - totalPadW);
                const float tableFontScale = (availTextW < sumMinTextW * 1.03f && sumMinTextW > 0.0f)
                    ? std::clamp(availTextW / (sumMinTextW * 1.03f), 0.68f, 1.0f)
                    : 1.0f;
                const float tableBodyFs = bodyFs * tableFontScale;
                const float tableCodeFs = inlineCodeFs * tableFontScale;
                const float tableEffZoom = effZoom * tableFontScale;
                const float minRowH = (canvas ? canvas->GetLineHeight(tableBodyFs) : theme.GetLineHeight(tableBodyFs)) * lhScale + cellPadY * 2.0f;

                std::vector<float> minColW(numCols, 0.0f);
                std::vector<float> naturalColW(numCols, 0.0f);
                float sumMinW = 0.0f;
                float sumNatW = 0.0f;
                for (size_t c = 0; c < numCols; ++c) {
                    minColW[c] = minTextW[c] * tableFontScale + cellPadX * 2.0f;
                    naturalColW[c] = naturalTextW[c] * tableFontScale + cellPadX * 2.0f;
                    sumMinW += minColW[c];
                    sumNatW += naturalColW[c];
                }

                bl.colWidths.assign(numCols, availW / static_cast<float>(numCols));
                if (availW >= sumNatW && sumNatW > 0.0f) {
                    const float extraW = availW - sumNatW;
                    for (size_t c = 0; c < numCols; ++c) {
                        bl.colWidths[c] = naturalColW[c] + extraW * (naturalColW[c] / sumNatW);
                    }
                } else if (availW >= sumMinW && sumNatW > sumMinW) {
                    const float flexAvail = availW - sumMinW;
                    const float flexTotal = sumNatW - sumMinW;
                    for (size_t c = 0; c < numCols; ++c) {
                        bl.colWidths[c] = minColW[c] + flexAvail * ((naturalColW[c] - minColW[c]) / flexTotal);
                    }
                } else if (sumMinW > 0.0f) {
                    for (size_t c = 0; c < numCols; ++c) {
                        bl.colWidths[c] = availW * (minColW[c] / sumMinW);
                    }
                }

                bl.colX.resize(numCols, contentBounds.x);
                {
                    float accumX = contentBounds.x;
                    for (size_t c = 0; c < numCols; ++c) {
                        bl.colX[c] = accumX;
                        accumX += bl.colWidths[c];
                    }
                }

                // 2. Build each cell's InlineFlowLayout ONCE, compute rowHeights, and commit tokens at centered Y
                const size_t numRows = blk.tableRows.size();
                bl.cellFlows.resize(numRows, std::vector<InlineFlowLayout>(numCols));
                bl.cellFlowOrigins.resize(numRows, std::vector<Vec2>(numCols));
                bl.rowHeights.assign(numRows, minRowH);
                float tableH = 0.0f;

                for (size_t r = 0; r < numRows; ++r) {
                    const FontStyle cellStyle = (r == 0) ? FontStyle::Bold : FontStyle::Regular;
                    const auto& rowCells = blk.tableRows[r];
                    float maxCellH = 0.0f;
                    for (size_t c = 0; c < numCols && c < rowCells.size(); ++c) {
                        const Align colAlign = (c < blk.tableAligns.size()) ? blk.tableAligns[c] : Align::Start;
                        const float cellInnerW = std::max(8.0f, bl.colWidths[c] - cellPadX * 2.0f);
                        bl.cellFlows[r][c] = BuildInlineFlowLayout(
                            canvas,
                            rowCells[c].spans,
                            cellInnerW,
                            tableBodyFs,
                            tableCodeFs,
                            lhScale,
                            0.0f,
                            theme,
                            tableEffZoom,
                            cellStyle,
                            colAlign
                        );
                        maxCellH = std::max(maxCellH, bl.cellFlows[r][c].height);
                    }
                    bl.rowHeights[r] = std::max(minRowH, maxCellH + cellPadY * 2.0f);
                    tableH += bl.rowHeights[r];
                }

                float ry = curY;
                for (size_t r = 0; r < numRows; ++r) {
                    const float rowH = bl.rowHeights[r];
                    const auto& rowCells = blk.tableRows[r];
                    for (size_t c = 0; c < numCols && c < rowCells.size(); ++c) {
                        const float cellContentH = bl.cellFlows[r][c].height;
                        const float cellContentY = ry + std::max(cellPadY, (rowH - cellContentH) * 0.5f);
                        const Vec2 cellOrigin{bl.colX[c] + cellPadX, cellContentY};
                        bl.cellFlowOrigins[r][c] = cellOrigin;
                        CommitInlineFlowToDocument(bl.cellFlows[r][c], cellOrigin, out.textMap, out.linkRegions);
                    }
                    ry += rowH;
                }

                bl.localBounds = Rect(contentBounds.x, curY, availW, tableH);
                bl.boxRadius = blkRad;
                bl.padX = cellPadX;
                bl.padY = cellPadY;
                curY += tableH;
                out.totalWidth = std::max(out.totalWidth, availW);
                break;
            }
        }

        out.blocks.push_back(std::move(bl));
    }

    out.totalHeight = std::max(0.0f, curY + docPad);
    out.totalWidth += docPad * 2.0f;
    return layoutPtr;
}

std::shared_ptr<const MarkdownDocumentLayout> GetOrBuildMarkdownLayout(
    ICanvas* canvas,
    float outerWidth,
    const ParsedMarkdownDoc& doc,
    const MarkdownOptions& opts,
    const UiTheme& theme,
    float zoom
) {
    static std::unordered_map<uint64_t, std::shared_ptr<const MarkdownDocumentLayout>> s_layoutCache;
    const float effZoom = std::clamp(zoom, 0.5f, 3.0f);
    const uint64_t key = ComputeMarkdownLayoutKey(doc.sourceHash, outerWidth, effZoom, opts, theme, canvas != nullptr);

    auto it = s_layoutCache.find(key);
    if (it != s_layoutCache.end()) {
        return it->second;
    }
    if (s_layoutCache.size() > 32) {
        s_layoutCache.clear();
    }

    auto built = BuildMarkdownDocumentLayout(canvas, outerWidth, doc, opts, theme, effZoom, key);
    s_layoutCache.emplace(key, built);
    return built;
}

// --- Phase 3A: O(1) / O(K) Document Hit-Testing (Zero Layout, Zero Font Measurement) ---

MarkdownPassResult HitTestMarkdownLayout(
    const MarkdownDocumentLayout& layout,
    Rect outerBounds,
    Vec2 screenMousePos,
    int activeCopiedBlock,
    bool interactiveTasks,
    bool showCopyButton,
    bool disabled,
    const std::unordered_map<int, bool>* taskOverrides
) noexcept {
    MarkdownPassResult out{};
    out.totalWidth = layout.totalWidth;
    out.totalHeight = layout.totalHeight;
    if (disabled || !outerBounds.Contains(screenMousePos)) {
        return out;
    }

    const Vec2 docOrigin = outerBounds.Pos();
    const Vec2 localMouse = screenMousePos - docOrigin;
    const float s = layout.scale;

    for (const LinkHitRegion& lr : layout.linkRegions) {
        if (lr.localRect.Contains(localMouse)) {
            out.hoveredLink = lr.url;
            out.hoveredLinkRect = OffsetRect(lr.localRect, docOrigin);
            break;
        }
    }

    if (showCopyButton) {
        for (const CopyBtnHitRegion& cr : layout.copyRegions) {
            const float copyBtnW = (activeCopiedBlock == cr.codeBlockIndex) ? (72.0f * s) : (56.0f * s);
            const float copyBtnH = 20.0f * s;
            const Rect btnLocalRect(
                cr.boxLocalRect.Right() - copyBtnW - 6.0f * s,
                cr.boxLocalRect.y + (cr.headerH - copyBtnH) * 0.5f,
                copyBtnW,
                copyBtnH
            );
            if (btnLocalRect.Contains(localMouse)) {
                out.hoveredCopyBlock = cr.codeBlockIndex;
                out.hoveredCopyCode = cr.rawCode;
                break;
            }
        }
    }

    if (interactiveTasks) {
        for (const TaskHitRegion& tr : layout.taskRegions) {
            if (tr.localHitRect.Contains(localMouse)) {
                bool isChecked = tr.defaultChecked;
                if (taskOverrides) {
                    auto it = taskOverrides->find(tr.taskIndex);
                    if (it != taskOverrides->end()) {
                        isChecked = it->second;
                    }
                }
                out.hoveredTaskIndex = tr.taskIndex;
                out.hoveredTaskCurrentState = isChecked;
                break;
            }
        }
    }

    return out;
}

// --- Phase 3B: Zero-Layout Document Rendering with Viewport Frustum Culling ---

void RenderMarkdownDocumentLayout(
    ICanvas& canvas,
    const ParsedMarkdownDoc& doc,
    const MarkdownDocumentLayout& layout,
    Rect outerBounds,
    Rect visibleViewportRect,
    const MarkdownOptions& opts,
    const UiTheme& theme,
    Vec2 screenMousePos,
    int hoveredCopyBlock,
    int activeCopiedBlock,
    int hoveredTaskIndex,
    const RenderOverlayState* overlay
) {
    const Vec2 docOrigin = outerBounds.Pos();
    const Vec2 localMouse = screenMousePos - docOrigin;
    const float effZoom = layout.effZoom;
    const float s = layout.scale;
    const float bodyFs = (opts.bodyFontSize.has_value() ? theme.Scale(*opts.bodyFontSize) : theme.GetFontSize(TextRole::BodyMedium)) * effZoom;

    for (const BlockLayout& bl : layout.blocks) {
        if (bl.astBlockIndex >= doc.blocks.size()) continue;
        const MarkdownBlock& blk = doc.blocks[bl.astBlockIndex];
        const Rect screenBlk = OffsetRect(bl.localBounds, docOrigin);

        // O(1) Viewport Frustum Culling: skip any block completely outside the visible scroll viewport
        if (visibleViewportRect.h > 0.0f &&
            (screenBlk.Bottom() < visibleViewportRect.y - 2.0f || screenBlk.y > visibleViewportRect.Bottom() + 2.0f)) {
            continue;
        }

        switch (bl.kind) {
            case BlockKind::Divider: {
                const float ly = screenBlk.y + screenBlk.h * 0.5f;
                canvas.DrawLine({screenBlk.x, ly}, {screenBlk.Right(), ly}, theme.palette.borderSubtle, 1.0f);
                break;
            }

            case BlockKind::Heading: {
                RenderInlineFlowLayout(
                    canvas,
                    bl.flow,
                    docOrigin + bl.flowLocalOrigin,
                    docOrigin,
                    theme.palette.textPrimary,
                    layout.textMap,
                    overlay,
                    theme,
                    localMouse
                );
                if (bl.hasHeadingDivider) {
                    const float dy = docOrigin.y + bl.dividerLocalY;
                    canvas.DrawLine({screenBlk.x, dy}, {screenBlk.Right(), dy}, theme.palette.borderSubtle, 1.0f);
                }
                break;
            }

            case BlockKind::Paragraph: {
                RenderInlineFlowLayout(
                    canvas,
                    bl.flow,
                    docOrigin + bl.flowLocalOrigin,
                    docOrigin,
                    theme.palette.textPrimary,
                    layout.textMap,
                    overlay,
                    theme,
                    localMouse
                );
                break;
            }

            case BlockKind::Blockquote: {
                canvas.DrawRoundRect(screenBlk, bl.boxRadius, theme.palette.surfaceElevated.WithAlpha(140));
                canvas.DrawRoundRect(Rect(screenBlk.x, screenBlk.y, 4.0f * s, screenBlk.h), 2.0f * s, theme.palette.borderHover);
                RenderInlineFlowLayout(
                    canvas,
                    bl.flow,
                    docOrigin + bl.flowLocalOrigin,
                    docOrigin,
                    theme.palette.textSecondary,
                    layout.textMap,
                    overlay,
                    theme,
                    localMouse
                );
                break;
            }

            case BlockKind::Alert: {
                const Color accent = AlertAccentColor(blk.alertKind, theme);
                canvas.DrawRoundRect(screenBlk, bl.boxRadius, accent.WithAlpha(24), accent.WithAlpha(90), 1.0f);
                canvas.DrawRoundRect(Rect(screenBlk.x, screenBlk.y, 4.0f * s, screenBlk.h), 2.0f * s, accent);

                const Icon ic = AlertIcon(blk.alertKind);
                const float icSz = 15.0f * s;
                ic.Render(canvas, Rect(screenBlk.x + bl.padX, screenBlk.y + bl.padY + (bl.headerH - icSz) * 0.5f, icSz, icSz), accent);

                const float titleFs = theme.GetFontSize(TextRole::LabelLarge) * effZoom;
                canvas.DrawTextStyled(
                    blk.alertTitle,
                    {screenBlk.x + bl.padX + icSz + 8.0f * s, screenBlk.y + bl.padY + (bl.headerH - titleFs) * 0.5f},
                    titleFs,
                    accent,
                    FontStyle::Bold
                );

                RenderInlineFlowLayout(
                    canvas,
                    bl.flow,
                    docOrigin + bl.flowLocalOrigin,
                    docOrigin,
                    theme.palette.textPrimary,
                    layout.textMap,
                    overlay,
                    theme,
                    localMouse
                );
                break;
            }

            case BlockKind::ListItem: {
                bool isTaskChecked = (blk.taskState == 1);
                if (blk.taskIndex >= 0 && overlay && overlay->taskOverrides) {
                    auto it = overlay->taskOverrides->find(blk.taskIndex);
                    if (it != overlay->taskOverrides->end()) {
                        isTaskChecked = it->second;
                    }
                }

                const Rect markerRect = OffsetRect(bl.markerLocalRect, docOrigin);
                if (blk.taskState >= 0) {
                    const bool cbHovered = (hoveredTaskIndex == blk.taskIndex && blk.taskIndex >= 0);
                    if (isTaskChecked) {
                        canvas.DrawRoundRect(markerRect, 3.0f * s, theme.palette.accent);
                        Icon::Check().Render(canvas, markerRect.Inset(2.0f * s), Color(255, 255, 255, 255));
                    } else {
                        canvas.DrawRoundRect(
                            markerRect,
                            3.0f * s,
                            theme.palette.surfaceElevated,
                            cbHovered ? theme.palette.accent : theme.palette.borderHover,
                            cbHovered ? 1.5f : 1.2f
                        );
                    }
                } else if (blk.isOrderedList) {
                    char numBuf[16];
                    std::snprintf(numBuf, sizeof(numBuf), "%u.", static_cast<unsigned>(blk.listNumber));
                    canvas.DrawTextStyled(
                        numBuf,
                        markerRect.Pos(),
                        bodyFs,
                        theme.palette.accent,
                        FontStyle::Bold
                    );
                } else {
                    canvas.DrawRoundRect(markerRect, markerRect.w * 0.5f, theme.palette.accent);
                }

                RenderInlineFlowLayout(
                    canvas,
                    bl.flow,
                    docOrigin + bl.flowLocalOrigin,
                    docOrigin,
                    isTaskChecked ? theme.palette.textSecondary : theme.palette.textPrimary,
                    layout.textMap,
                    overlay,
                    theme,
                    localMouse
                );
                break;
            }

            case BlockKind::CodeBlock: {
                const bool hasCustomBg = opts.codePalette.has_value() && opts.codePalette->editorBg.a > 0;
                const Color codeBg = hasCustomBg ? opts.codePalette->editorBg : theme.palette.surfaceElevated;
                const Color gutterBg = (opts.codePalette.has_value() && opts.codePalette->gutterBg.a > 0)
                    ? opts.codePalette->gutterBg
                    : theme.palette.surface;
                const Color defaultCodeCol = opts.codePalette.has_value()
                    ? opts.codePalette->identifier
                    : theme.palette.textPrimary;
                const Color lineNumCol = opts.codePalette.has_value()
                    ? opts.codePalette->comment
                    : theme.palette.textMuted;
                const Color wsColor = opts.codePalette.has_value()
                    ? opts.codePalette->comment.WithAlpha(110)
                    : theme.palette.textMuted.WithAlpha(110);

                canvas.DrawRoundRect(screenBlk, bl.boxRadius, codeBg, theme.palette.borderSubtle, 1.0f);
                canvas.DrawRoundRect(Rect(screenBlk.x, screenBlk.y, screenBlk.w, bl.headerH), bl.boxRadius, gutterBg);
                canvas.DrawLine(
                    {screenBlk.x, screenBlk.y + bl.headerH},
                    {screenBlk.Right(), screenBlk.y + bl.headerH},
                    theme.palette.borderSubtle,
                    1.0f
                );

                const float lblFs = theme.GetFontSize(TextRole::LabelSmall) * effZoom;
                const std::string_view langLabel = blk.codeLang.empty() ? "code" : std::string_view(blk.codeLang);
                canvas.DrawTextStyled(
                    langLabel,
                    {screenBlk.x + bl.padX, screenBlk.y + (bl.headerH - lblFs) * 0.5f},
                    lblFs,
                    lineNumCol,
                    FontStyle::Bold
                );

                if (opts.showCopyButton) {
                    const bool isHov = (hoveredCopyBlock == blk.codeBlockIndex);
                    const bool isCopied = (activeCopiedBlock == blk.codeBlockIndex);
                    const float copyBtnW = isCopied ? (72.0f * s) : (56.0f * s);
                    const float copyBtnH = 20.0f * s;
                    const Rect copyBtnRect(
                        screenBlk.Right() - copyBtnW - 6.0f * s,
                        screenBlk.y + (bl.headerH - copyBtnH) * 0.5f,
                        copyBtnW,
                        copyBtnH
                    );
                    const Color btnBg = isCopied
                        ? theme.GetVariant(UiVariant::Success).bg.WithAlpha(40)
                        : (isHov ? theme.palette.accent.WithAlpha(35) : codeBg);
                    const Color btnBorder = isCopied
                        ? theme.GetVariant(UiVariant::Success).bg
                        : (isHov ? theme.palette.accent : theme.palette.borderSubtle);
                    const Color btnText = isCopied
                        ? theme.GetVariant(UiVariant::Success).bg
                        : (isHov ? theme.palette.accent : defaultCodeCol);
                    canvas.DrawRoundRect(copyBtnRect, 4.0f * s, btnBg, btnBorder, 1.0f);
                    canvas.DrawTextBox(
                        isCopied ? "Copied" : "Copy",
                        copyBtnRect,
                        lblFs,
                        btnText,
                        0.5f,
                        0.5f
                    );
                }

                canvas.PushClip(Rect(screenBlk.x + 1.0f, screenBlk.y + bl.headerH + 1.0f, screenBlk.w - 2.0f, bl.bodyH - 2.0f));
                if (opts.codeLineNumbers) {
                    canvas.DrawRect(Rect(screenBlk.x, screenBlk.y + bl.headerH, bl.gutterW, bl.bodyH), gutterBg.WithAlpha(180));
                    canvas.DrawLine(
                        {screenBlk.x + bl.gutterW, screenBlk.y + bl.headerH},
                        {screenBlk.x + bl.gutterW, screenBlk.Bottom()},
                        theme.palette.borderSubtle,
                        1.0f
                    );
                }

                const float screenCodeStartX = docOrigin.x + bl.codeStartX;
                const float lineNumFs = theme.GetFontSize(TextRole::CodeSmall) * effZoom;

                for (const CodeVisualLineLayout& vline : bl.codeVisualLines) {
                    const float screenLy = docOrigin.y + vline.localY;
                    const float screenTextY = docOrigin.y + vline.textLocalY;

                    if (opts.codeLineNumbers && vline.isFirstSlice) {
                        char lnBuf[16];
                        std::snprintf(lnBuf, sizeof(lnBuf), "%zu", vline.logicalLine + 1);
                        canvas.DrawTextBox(
                            lnBuf,
                            Rect(screenBlk.x + 4.0f * s, screenLy, bl.gutterW - 10.0f * s, bl.codeLineH),
                            lineNumFs,
                            lineNumCol,
                            1.0f,
                            0.5f
                        );
                    }

                    const PositionedToken* tok = (vline.tokenIndex < layout.textMap.tokens.size())
                        ? &layout.textMap.tokens[vline.tokenIndex]
                        : nullptr;
                    if (tok) {
                        DrawTokenSelectionAndSearchHighlights(canvas, *tok, layout.textMap.plainText, overlay, theme, docOrigin);
                    }

                    if (opts.codeIndentGuides && vline.isFirstSlice && !vline.sliceStr.empty() && bl.spaceW > 0.0f) {
                        size_t leadSpaces = 0;
                        while (leadSpaces < vline.sliceStr.size() && vline.sliceStr[leadSpaces] == ' ') ++leadSpaces;
                        for (size_t col = 4; col <= leadSpaces; col += 4) {
                            const float gx = std::round(screenCodeStartX + static_cast<float>(col) * bl.spaceW);
                            canvas.DrawLine({gx, screenLy}, {gx, screenLy + bl.codeLineH}, theme.palette.borderSubtle.WithAlpha(90), 1.0f);
                        }
                    }

                    if (opts.codeShowWhitespace && !vline.sliceStr.empty() && bl.spaceW > 0.0f) {
                        const float dotR = std::max(1.2f, 1.4f * s);
                        const float cy = screenLy + bl.codeLineH * 0.5f;
                        for (size_t c = 0; c < vline.sliceStr.size(); ++c) {
                            if (vline.sliceStr[c] == ' ') {
                                const float cx = screenCodeStartX + (static_cast<float>(c) + 0.5f) * bl.spaceW;
                                canvas.DrawRoundRect(Rect(cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f), dotR, wsColor);
                            } else if (vline.sliceStr[c] == '\t') {
                                const float x0 = screenCodeStartX + static_cast<float>(c) * bl.spaceW + 2.0f * s;
                                const float x1 = screenCodeStartX + static_cast<float>(c + 1) * bl.spaceW - 2.0f * s;
                                if (x1 > x0) {
                                    canvas.DrawLine({x0, cy}, {x1, cy}, wsColor, 1.2f);
                                    canvas.DrawLine({x1 - 3.0f * s, cy - 2.5f * s}, {x1, cy}, wsColor, 1.2f);
                                    canvas.DrawLine({x1 - 3.0f * s, cy + 2.5f * s}, {x1, cy}, wsColor, 1.2f);
                                }
                            }
                        }
                    }

                    for (const CodeRenderSpan& sp : vline.renderSpans) {
                        canvas.DrawTextStyled(
                            sp.text,
                            {screenCodeStartX + sp.localOffsetX, screenTextY},
                            bl.codeFs,
                            sp.color,
                            FontStyle::Monospace
                        );
                    }

                    if (tok) {
                        DrawTokenCaretIfPresent(canvas, *tok, layout.textMap.plainText, overlay, theme, docOrigin);
                    }
                }
                canvas.PopClip();
                break;
            }

            case BlockKind::ImageBlock: {
                const Rect imgRect = OffsetRect(bl.imgLocalRect, docOrigin);
                if (opts.imageCheckerboard) {
                    Image::DrawCheckerboard(canvas, imgRect, bl.imgRadius, theme, 10.0f);
                } else {
                    canvas.DrawRoundRect(imgRect, bl.imgRadius, theme.palette.surfaceElevated);
                }
                if (!canvas.DrawImage(blk.rawText, imgRect, bl.imgRadius, 255)) {
                    canvas.DrawTextBox(
                        blk.imageAlt.empty() ? blk.rawText : std::string_view(blk.imageAlt),
                        imgRect.Inset(8.0f),
                        theme.GetFontSize(TextRole::LabelMedium) * effZoom,
                        theme.palette.textMuted,
                        0.5f,
                        0.5f
                    );
                }
                canvas.DrawRoundRect(imgRect, bl.imgRadius, Color::Transparent(), theme.palette.borderSubtle, 1.0f);

                if (!bl.flow.items.empty()) {
                    RenderInlineFlowLayout(
                        canvas,
                        bl.flow,
                        docOrigin + bl.flowLocalOrigin,
                        docOrigin,
                        theme.palette.textMuted,
                        layout.textMap,
                        overlay,
                        theme,
                        localMouse
                    );
                }
                break;
            }

            case BlockKind::Table: {
                if (bl.rowHeights.empty() || bl.colWidths.empty()) break;
                canvas.DrawRoundRect(screenBlk, bl.boxRadius, theme.palette.surface, theme.palette.borderSubtle, 1.0f);

                const size_t numRows = bl.rowHeights.size();
                const size_t numCols = bl.colWidths.size();
                float ry = screenBlk.y;

                for (size_t r = 0; r < numRows; ++r) {
                    const float rowH = bl.rowHeights[r];
                    const Rect rowRect(screenBlk.x, ry, screenBlk.w, rowH);
                    if (r == 0) {
                        canvas.DrawRoundRect(rowRect, bl.boxRadius, theme.palette.surfaceElevated);
                    } else if (r % 2 == 1) {
                        canvas.DrawRect(rowRect.Inset(1.0f, 0.0f), theme.palette.surfaceElevated.WithAlpha(90));
                    }
                    if (r > 0) {
                        canvas.DrawLine({rowRect.x, ry}, {rowRect.Right(), ry}, theme.palette.borderSubtle, 1.0f);
                    }

                    for (size_t c = 0; c < numCols; ++c) {
                        const float cx = docOrigin.x + bl.colX[c];
                        const float cw = bl.colWidths[c];
                        if (c > 0) {
                            canvas.DrawLine({cx, ry}, {cx, ry + rowH}, theme.palette.borderSubtle, 1.0f);
                        }
                        if (r < bl.cellFlows.size() && c < bl.cellFlows[r].size() && !bl.cellFlows[r][c].items.empty()) {
                            canvas.PushClip(Rect(cx + 1.0f, ry + 1.0f, std::max(1.0f, cw - 2.0f), std::max(1.0f, rowH - 2.0f)));
                            RenderInlineFlowLayout(
                                canvas,
                                bl.cellFlows[r][c],
                                docOrigin + bl.cellFlowOrigins[r][c],
                                docOrigin,
                                theme.palette.textPrimary,
                                layout.textMap,
                                overlay,
                                theme,
                                localMouse
                            );
                            canvas.PopClip();
                        }
                    }
                    ry += rowH;
                }
                break;
            }
        }
    }
}

struct MarkdownState {
    uint64_t docSourceHash{0};
    int copiedBlockIndex{-1};
    float zoom{1.0f};
    std::shared_ptr<const MarkdownDocumentLayout> cachedLayout{};

    // Selection & Cursor state
    size_t cursorOffset{0};
    size_t selAnchorOffset{0};
    bool isDraggingSelection{false};
    float preferredCursorX{-1.0f};
    std::string cachedSelectedText{};

    // Interactive Task List overrides
    std::unordered_map<int, bool> taskOverrides{};

    // In-document Search state
    bool searchBarOpen{false};
    std::string searchQuery{};
    int activeSearchMatch{0};

    bool HasSelection() const noexcept {
        return cursorOffset != selAnchorOffset;
    }
    size_t SelMin() const noexcept {
        return std::min(cursorOffset, selAnchorOffset);
    }
    size_t SelMax() const noexcept {
        return std::max(cursorOffset, selAnchorOffset);
    }
};

struct MarkdownPayload {
    std::string_view text{};
    MarkdownOptions opts{};
    float zoom{1.0f};
    int hoveredCopyBlock{-1};
    int activeCopiedBlock{-1};
    int hoveredTaskIndex{-1};
    bool hasSelection{false};
    size_t selMin{0};
    size_t selMax{0};
    bool showCaret{false};
    size_t caretOffset{0};
    bool searchBarOpen{false};
    std::string_view activeSearchQuery{};
    int activeSearchMatch{-1};
    int searchMatchCount{0};
    const std::unordered_map<int, bool>* taskOverrides{nullptr};
    const MarkdownDocumentLayout* layout{nullptr};
    mutable ICanvas* lastCanvas{nullptr};
};

} // namespace

MarkdownResponse Markdown::Run(
    Context& ctx,
    std::string_view markdownText,
    const MarkdownOptions& opts,
    std::source_location loc
) {
    const Id id = ctx.MakeChildId(loc);
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const UiSize activeSize = ctx.GetCurrentSize();
    const UiTheme& theme = ctx.GetTheme();

    MarkdownOptions optsCopy = opts;
    Control::InheritElementSizing(ctx, optsCopy.stretch, optsCopy.width, optsCopy.height);

    auto& state = ctx.GetOrCreateState<MarkdownState>(id);
    const ParsedMarkdownDoc& doc = GetOrParseMarkdown(markdownText);
    if (state.docSourceHash != doc.sourceHash) {
        state.docSourceHash = doc.sourceHash;
        state.cursorOffset = 0;
        state.selAnchorOffset = 0;
        state.preferredCursorX = -1.0f;
        state.taskOverrides.clear();
    }

    if (optsCopy.zoom != nullptr) {
        state.zoom = std::clamp(*optsCopy.zoom, 0.5f, 3.0f);
    }

    Response baseRes = ctx.Interact(id);
    MarkdownResponse res{};
    static_cast<Response&>(res) = baseRes;

    const bool isFocused = (ctx.GetFocusedId() == id);

    // 1. Handle Smooth Zoom (Ctrl+Wheel & KeyAction::ZoomIn/ZoomOut/ZoomReset)
    if (optsCopy.allowZoom && !effectiveDisabled) {
        bool zoomChanged = false;
        if (baseRes.hovered && ctx.GetKeyModifiers().ctrl && std::abs(ctx.GetMouseWheel().y) > 0.01f) {
            const float step = (ctx.GetMouseWheel().y > 0.0f) ? 0.1f : -0.1f;
            const float nextZoom = std::clamp( std::round((state.zoom + step) * 10.0f) / 10.0f, 0.5f, 3.0f );
            if (std::abs(nextZoom - state.zoom) > 0.001f) {
                state.zoom = nextZoom;
                zoomChanged = true;
            }
        }
        if (isFocused || baseRes.hovered) {
            if (ctx.ConsumeAction(KeyAction::ZoomIn)) {
                state.zoom = std::clamp(std::round((state.zoom + 0.1f) * 10.0f) / 10.0f, 0.5f, 3.0f);
                zoomChanged = true;
            } else if (ctx.ConsumeAction(KeyAction::ZoomOut)) {
                state.zoom = std::clamp(std::round((state.zoom - 0.1f) * 10.0f) / 10.0f, 0.5f, 3.0f);
                zoomChanged = true;
            } else if (ctx.ConsumeAction(KeyAction::ZoomReset)) {
                state.zoom = 1.0f;
                zoomChanged = true;
            }
        }
        if (zoomChanged) {
            if (optsCopy.zoom != nullptr) {
                *optsCopy.zoom = state.zoom;
            }
            res.changed = true;
            ctx.RequestRepaint(2);
        }
    }
    res.currentZoom = state.zoom;

    // 2. Resolve cached Document Layout & DocumentTextMap on previous unclipped bounds (O(1) on cache hit)
    ICanvas* hitCanvas = ctx.GetCanvas();
    Rect hitBounds = ctx.GetPrevUnclippedBounds(id);
    if (hitBounds.w <= 0.0f) {
        hitBounds = Rect(0.0f, 0.0f, theme.Scale(520.0f), theme.Scale(400.0f));
    }
    const Rect ctrlRect = hitBounds;

    state.cachedLayout = GetOrBuildMarkdownLayout(hitCanvas, ctrlRect.w, doc, optsCopy, theme, state.zoom);
    const MarkdownDocumentLayout& layout = *state.cachedLayout;
    const DocumentTextMap& textMap = layout.textMap;

    const MarkdownPassResult hit = HitTestMarkdownLayout(
        layout,
        ctrlRect,
        baseRes.hovered ? ctx.GetMousePos() : Vec2{-100000.0f, -100000.0f},
        state.copiedBlockIndex,
        optsCopy.interactiveTasks,
        optsCopy.showCopyButton,
        effectiveDisabled,
        &state.taskOverrides
    );

    const size_t maxOffset = textMap.plainText.size();
    state.cursorOffset = std::min(state.cursorOffset, maxOffset);
    state.selAnchorOffset = std::min(state.selAnchorOffset, maxOffset);

    int hoveredCopyBlock = hit.hoveredCopyBlock;

    // 3. Handle Links, Copy Button, Task Checkboxes, and Mouse Text Selection
    if (!effectiveDisabled) {
        if (baseRes.hovered) {
            if (!hit.hoveredLink.empty()) {
                ctx.SetTooltip(hit.hoveredLink, hit.hoveredLinkRect);
                if (baseRes.clicked) {
                    res.clickedLink = hit.hoveredLink;
                    res.changed = true;
                    if (optsCopy.onLinkClick) {
                        optsCopy.onLinkClick(hit.hoveredLink);
                    }
                }
            } else if (hit.hoveredCopyBlock >= 0) {
                if (baseRes.clicked) {
                    state.copiedBlockIndex = hit.hoveredCopyBlock;
                    res.copiedBlockIndex = hit.hoveredCopyBlock;
                    res.changed = true;
                    ctx.SetClipboardText(hit.hoveredCopyCode);
                }
            } else if (hit.hoveredTaskIndex >= 0 && optsCopy.interactiveTasks) {
                if (baseRes.clicked) {
                    const bool nextChecked = !hit.hoveredTaskCurrentState;
                    state.taskOverrides[hit.hoveredTaskIndex] = nextChecked;
                    res.toggledTaskIndex = hit.hoveredTaskIndex;
                    res.toggledTaskChecked = nextChecked;
                    res.changed = true;
                    if (optsCopy.onTaskToggle) {
                        optsCopy.onTaskToggle(hit.hoveredTaskIndex, nextChecked);
                    }
                }
            }
        }

        // Mouse click / double-click / drag selection (using document-local mouse coordinates)
        const bool clickedInteractiveSubElement =
            !hit.hoveredLink.empty() || hit.hoveredCopyBlock >= 0 || (hit.hoveredTaskIndex >= 0 && optsCopy.interactiveTasks);

        if (optsCopy.enableSelection && baseRes.hovered && ctx.IsMouseClicked() && !clickedInteractiveSubElement) {
            ctx.RequestFocus(id);
            const Vec2 localMouse = ctx.GetMousePos() - ctrlRect.Pos();
            const size_t hitOff = HitTestDocumentCharOffset(textMap, hitCanvas, localMouse);
            state.preferredCursorX = -1.0f;
            if (baseRes.doubleClicked) {
                TextUtil::SelectWordAt(textMap.plainText, hitOff, state.selAnchorOffset, state.cursorOffset);
                state.isDraggingSelection = false;
            } else {
                state.cursorOffset = hitOff;
                if (!ctx.GetKeyModifiers().shift) {
                    state.selAnchorOffset = hitOff;
                }
                state.isDraggingSelection = true;
            }
        } else if (state.isDraggingSelection) {
            if (ctx.IsMouseDown()) {
                const Vec2 localMouse = ctx.GetMousePos() - ctrlRect.Pos();
                state.cursorOffset = HitTestDocumentCharOffset(textMap, hitCanvas, localMouse);
                state.preferredCursorX = -1.0f;
            } else {
                state.isDraggingSelection = false;
            }
        }
    }

    // 4. Handle Keyboard Actions via centralized KeyAction system (Search, Copy, SelectAll, Cursor & Selection Navigation)
    if ((isFocused || baseRes.hovered) && !effectiveDisabled) {
        if (optsCopy.enableSearch && ctx.ConsumeAction(KeyAction::Find)) {
            state.searchBarOpen = true;
            ctx.RequestFocus(id);
            ctx.RequestRepaint(2);
        }
    }

    if (isFocused && !effectiveDisabled) {
        if (state.searchBarOpen && optsCopy.enableSearch) {
            if (ctx.ConsumeAction(KeyAction::Cancel)) {
                state.searchBarOpen = false;
                ctx.RequestRepaint(2);
            } else if (ctx.ConsumeAction(KeyAction::FindNext) || ctx.ConsumeAction(KeyAction::Confirm)) {
                state.activeSearchMatch += 1;
            } else if (ctx.ConsumeAction(KeyAction::FindPrev)) {
                state.activeSearchMatch -= 1;
            } else if (ctx.ConsumeAction(KeyAction::DeletePrevChar)) {
                if (!state.searchQuery.empty()) {
                    const size_t prev = TextUtil::PrevCharBoundary(state.searchQuery, state.searchQuery.size());
                    state.searchQuery.erase(prev);
                    state.activeSearchMatch = 0;
                }
            } else if (ctx.ConsumeAction(KeyAction::DeletePrevWord)) {
                if (!state.searchQuery.empty()) {
                    const size_t prev = TextUtil::PrevWordBoundary(state.searchQuery, state.searchQuery.size());
                    state.searchQuery.erase(prev);
                    state.activeSearchMatch = 0;
                }
            } else {
                const auto typedChunks = ctx.ConsumePendingTextInput();
                if (!typedChunks.empty()) {
                    for (const std::string& chunk : typedChunks) {
                        for (char ch : chunk) {
                            if (static_cast<unsigned char>(ch) >= 32) {
                                state.searchQuery.push_back(ch);
                            }
                        }
                    }
                    state.activeSearchMatch = 0;
                }
            }
        }

        if (optsCopy.enableSelection) {
            auto moveCursorHoriz = [&](size_t newOff, bool extendSelection) {
                state.cursorOffset = std::min(newOff, maxOffset);
                if (!extendSelection) {
                    state.selAnchorOffset = state.cursorOffset;
                }
                state.preferredCursorX = -1.0f;
                ctx.RequestRepaint(2);
            };

            auto moveCursorVert = [&](int lineDelta, bool extendSelection) {
                if (textMap.lines.empty()) return;
                const size_t curLine = FindLineForCharOffset(textMap, state.cursorOffset);
                if (state.preferredCursorX < 0.0f) {
                    state.preferredCursorX = GetCursorXForOffset(textMap, hitCanvas, state.cursorOffset);
                }
                const int targetLine = std::clamp(
                    static_cast<int>(curLine) + lineDelta,
                    0,
                    static_cast<int>(textMap.lines.size()) - 1
                );
                const PositionedLine& ln = textMap.lines[static_cast<size_t>(targetLine)];
                const float targetY = (ln.top + ln.bottom) * 0.5f;
                state.cursorOffset = HitTestDocumentCharOffset(textMap, hitCanvas, {state.preferredCursorX, targetY});
                if (!extendSelection) {
                    state.selAnchorOffset = state.cursorOffset;
                }
                ctx.RequestRepaint(2);
            };

            if (ctx.ConsumeAction(KeyAction::Copy)) {
                if (state.HasSelection()) {
                    const size_t s0 = state.SelMin();
                    const size_t s1 = state.SelMax();
                    ctx.SetClipboardText(std::string_view(textMap.plainText).substr(s0, s1 - s0));
                }
            } else if (ctx.ConsumeAction(KeyAction::SelectAll)) {
                state.selAnchorOffset = 0;
                state.cursorOffset = maxOffset;
                state.preferredCursorX = -1.0f;
            } else if (ctx.ConsumeAction(KeyAction::Cancel)) {
                state.selAnchorOffset = state.cursorOffset;
            } else if (ctx.ConsumeAction(KeyAction::MoveLeft)) {
                if (state.HasSelection()) {
                    moveCursorHoriz(state.SelMin(), false);
                } else {
                    moveCursorHoriz(TextUtil::PrevCharBoundary(textMap.plainText, state.cursorOffset), false);
                }
            } else if (ctx.ConsumeAction(KeyAction::MoveRight)) {
                if (state.HasSelection()) {
                    moveCursorHoriz(state.SelMax(), false);
                } else {
                    moveCursorHoriz(TextUtil::NextCharBoundary(textMap.plainText, state.cursorOffset), false);
                }
            } else if (ctx.ConsumeAction(KeyAction::SelectLeft)) {
                moveCursorHoriz(TextUtil::PrevCharBoundary(textMap.plainText, state.cursorOffset), true);
            } else if (ctx.ConsumeAction(KeyAction::SelectRight)) {
                moveCursorHoriz(TextUtil::NextCharBoundary(textMap.plainText, state.cursorOffset), true);
            } else if (ctx.ConsumeAction(KeyAction::MoveWordLeft)) {
                moveCursorHoriz(TextUtil::PrevWordBoundary(textMap.plainText, state.cursorOffset), false);
            } else if (ctx.ConsumeAction(KeyAction::MoveWordRight)) {
                moveCursorHoriz(TextUtil::NextWordBoundary(textMap.plainText, state.cursorOffset), false);
            } else if (ctx.ConsumeAction(KeyAction::SelectWordLeft)) {
                moveCursorHoriz(TextUtil::PrevWordBoundary(textMap.plainText, state.cursorOffset), true);
            } else if (ctx.ConsumeAction(KeyAction::SelectWordRight)) {
                moveCursorHoriz(TextUtil::NextWordBoundary(textMap.plainText, state.cursorOffset), true);
            } else if (ctx.ConsumeAction(KeyAction::MoveUp)) {
                moveCursorVert(-1, false);
            } else if (ctx.ConsumeAction(KeyAction::MoveDown)) {
                moveCursorVert(1, false);
            } else if (ctx.ConsumeAction(KeyAction::SelectUp)) {
                moveCursorVert(-1, true);
            } else if (ctx.ConsumeAction(KeyAction::SelectDown)) {
                moveCursorVert(1, true);
            } else if (ctx.ConsumeAction(KeyAction::MovePageUp)) {
                moveCursorVert(-10, false);
            } else if (ctx.ConsumeAction(KeyAction::MovePageDown)) {
                moveCursorVert(10, false);
            } else if (ctx.ConsumeAction(KeyAction::SelectPageUp)) {
                moveCursorVert(-10, true);
            } else if (ctx.ConsumeAction(KeyAction::SelectPageDown)) {
                moveCursorVert(10, true);
            } else if (ctx.ConsumeAction(KeyAction::MoveLineStart)) {
                if (!textMap.lines.empty()) {
                    const size_t li = FindLineForCharOffset(textMap, state.cursorOffset);
                    moveCursorHoriz(textMap.lines[li].charStart, false);
                }
            } else if (ctx.ConsumeAction(KeyAction::MoveLineEnd)) {
                if (!textMap.lines.empty()) {
                    const size_t li = FindLineForCharOffset(textMap, state.cursorOffset);
                    moveCursorHoriz(textMap.lines[li].charEnd, false);
                }
            } else if (ctx.ConsumeAction(KeyAction::SelectLineStart)) {
                if (!textMap.lines.empty()) {
                    const size_t li = FindLineForCharOffset(textMap, state.cursorOffset);
                    moveCursorHoriz(textMap.lines[li].charStart, true);
                }
            } else if (ctx.ConsumeAction(KeyAction::SelectLineEnd)) {
                if (!textMap.lines.empty()) {
                    const size_t li = FindLineForCharOffset(textMap, state.cursorOffset);
                    moveCursorHoriz(textMap.lines[li].charEnd, true);
                }
            } else if (ctx.ConsumeAction(KeyAction::MoveDocStart)) {
                moveCursorHoriz(0, false);
            } else if (ctx.ConsumeAction(KeyAction::MoveDocEnd)) {
                moveCursorHoriz(maxOffset, false);
            } else if (ctx.ConsumeAction(KeyAction::SelectDocStart)) {
                moveCursorHoriz(0, true);
            } else if (ctx.ConsumeAction(KeyAction::SelectDocEnd)) {
                moveCursorHoriz(maxOffset, true);
            }
        }
    }

    // 5. Compute Search Matches & Selected Text for Response
    const std::string_view effectiveQuery = !optsCopy.searchQuery.empty()
        ? optsCopy.searchQuery
        : std::string_view(state.searchQuery);
    const auto matches = FindSearchMatches(textMap.plainText, effectiveQuery);
    res.searchMatchCount = static_cast<int>(matches.size());
    if (!matches.empty()) {
        if (state.activeSearchMatch < 0) {
            state.activeSearchMatch = static_cast<int>(matches.size()) - 1;
        } else {
            state.activeSearchMatch = state.activeSearchMatch % static_cast<int>(matches.size());
        }
        res.activeSearchMatch = state.activeSearchMatch;
    } else {
        res.activeSearchMatch = -1;
    }

    if (state.HasSelection() && state.SelMax() <= textMap.plainText.size()) {
        state.cachedSelectedText = textMap.plainText.substr(state.SelMin(), state.SelMax() - state.SelMin());
        res.selectedText = state.cachedSelectedText;
    }

    // 6. Create Node & Payload
    Node* node = ctx.CreateNode(NodeType::Markdown, id);
    auto* payload = ctx.AllocPayload<MarkdownPayload>();
    payload->text = ctx.AllocString(markdownText);
    payload->opts = optsCopy;
    payload->zoom = state.zoom;
    payload->hoveredCopyBlock = hoveredCopyBlock;
    payload->activeCopiedBlock = state.copiedBlockIndex;
    payload->hoveredTaskIndex = hit.hoveredTaskIndex;
    payload->hasSelection = optsCopy.enableSelection && state.HasSelection();
    payload->selMin = state.SelMin();
    payload->selMax = state.SelMax();
    payload->showCaret = optsCopy.enableSelection && isFocused;
    payload->caretOffset = state.cursorOffset;
    payload->searchBarOpen = state.searchBarOpen && optsCopy.enableSearch;
    payload->activeSearchQuery = ctx.AllocString(effectiveQuery);
    payload->activeSearchMatch = res.activeSearchMatch;
    payload->searchMatchCount = res.searchMatchCount;
    payload->taskOverrides = &state.taskOverrides;
    payload->layout = state.cachedLayout.get();
    payload->lastCanvas = ctx.GetCanvas();
    node->SetPayload(payload);

    node->visualHash = detail::HashArgs(
        doc.sourceHash,
        state.zoom,
        optsCopy.width.value_or(-1.0f),
        optsCopy.height.value_or(-1.0f),
        optsCopy.background,
        optsCopy.border,
        optsCopy.bodyFontSize.value_or(-1.0f),
        optsCopy.headingScale,
        optsCopy.inlineCodeFontSize.value_or(-1.0f),
        optsCopy.codeFontSize.value_or(-1.0f),
        optsCopy.lineHeightScale,
        optsCopy.documentPadding,
        optsCopy.maxContentWidth.value_or(-1.0f),
        optsCopy.blockGap,
        optsCopy.paragraphIndent,
        optsCopy.listIndent,
        optsCopy.blockPadding,
        optsCopy.blockRadius,
        optsCopy.headingDividers,
        optsCopy.codeLineNumbers,
        optsCopy.syntaxHighlight,
        optsCopy.showCopyButton,
        optsCopy.codeShowWhitespace,
        optsCopy.codeIndentGuides,
        optsCopy.codePalette.has_value() ? optsCopy.codePalette->editorBg.ToUint32() : 0u,
        optsCopy.codePalette.has_value() ? optsCopy.codePalette->keyword.ToUint32() : 0u,
        optsCopy.imageCheckerboard,
        optsCopy.imageMaxHeight,
        optsCopy.imageRadius,
        hoveredCopyBlock,
        hit.hoveredTaskIndex,
        state.copiedBlockIndex,
        payload->hasSelection,
        payload->selMin,
        payload->selMax,
        payload->showCaret,
        payload->caretOffset,
        payload->searchBarOpen,
        payload->activeSearchQuery,
        payload->activeSearchMatch,
        state.taskOverrides.size(),
        activeSize,
        effectiveDisabled
    );

    node->measureFn = [](const Node* n, float availableWidth, ICanvas* canvas, const UiTheme& th) noexcept -> Vec2 {
        const auto* p = n->As<MarkdownPayload>();
        if (!p) return {0.0f, 0.0f};
        if (canvas) {
            p->lastCanvas = canvas;
        }
        MarkdownOptions mOpts = p->opts;
        float z = p->zoom;
        mOpts.zoom = &z;
        return Markdown::Measure(p->text, mOpts, n->uiSize, canvas ? canvas : p->lastCanvas, th, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& th) noexcept {
        const auto* p = n->As<MarkdownPayload>();
        if (!p) return;

        if (p->opts.background.IsBackgroundEnabled(false) || p->opts.border.color.HasAny() || p->opts.border.width > 0.0f) {
            const Color bgCol = p->opts.background.IsBackgroundEnabled(false)
                ? (p->opts.background.IsSurface() ? th.palette.surface : p->opts.background.Resolve(n->hovered, n->pressed, n->disabled, th.palette.surface))
                : Color::Transparent();
            const Color bCol = p->opts.border.color.Resolve(n->hovered, n->pressed, n->disabled, th.palette.border);
            const float bW = (p->opts.border.width > 0.0f) ? th.Scale(p->opts.border.width) : (p->opts.border.color.HasAny() ? th.Scale(1.0f) : 0.0f);
            const float bR = p->opts.border.radius.has_value() ? th.Scale(*p->opts.border.radius) : 0.0f;
            canvas.DrawRoundRect(n->computedRect, bR, bgCol, bCol, bW);
        }

        const ParsedMarkdownDoc& parsedDoc = GetOrParseMarkdown(p->text);
        const Rect ctrlRect = n->computedRect;

        // Compute visible viewport rect by intersecting with clipping ancestors (for O(1) block frustum culling)
        Rect visibleRect = ctrlRect;
        for (const Node* anc = n->parent; anc != nullptr; anc = anc->parent) {
            if (anc->clipChildren) {
                visibleRect = visibleRect.Intersect(anc->computedRect);
            }
        }

        std::shared_ptr<const MarkdownDocumentLayout> layoutHolder;
        const MarkdownDocumentLayout& layout =
            (p->layout != nullptr && std::abs(p->layout->totalWidth - ctrlRect.w) < 1.0f)
                ? *p->layout
                : *(layoutHolder = GetOrBuildMarkdownLayout(&canvas, ctrlRect.w, parsedDoc, p->opts, th, p->zoom));

        RenderOverlayState overlay{};
        overlay.hasSelection = p->hasSelection;
        overlay.selMin = p->selMin;
        overlay.selMax = p->selMax;
        overlay.showCaret = p->showCaret;
        overlay.caretOffset = p->caretOffset;
        overlay.activeSearchMatch = p->activeSearchMatch;
        overlay.taskOverrides = p->taskOverrides;

        std::vector<SearchMatchRange> matches;
        if (!p->activeSearchQuery.empty()) {
            matches = FindSearchMatches(layout.textMap.plainText, p->activeSearchQuery);
            overlay.searchMatches = &matches;
        }

        RenderMarkdownDocumentLayout(
            canvas,
            parsedDoc,
            layout,
            ctrlRect,
            visibleRect,
            p->opts,
            th,
            {-100000.0f, -100000.0f},
            p->hoveredCopyBlock,
            p->activeCopiedBlock,
            p->hoveredTaskIndex,
            &overlay
        );

        // Render floating in-document Search bar if open (pinned to visible viewport top-right)
        if (p->searchBarOpen) {
            const float s = th.Scale(1.0f);
            const float barW = 240.0f * s;
            const float barH = 30.0f * s;
            const float anchorTop = (visibleRect.h > 0.0f) ? visibleRect.y : ctrlRect.y;
            const float anchorRight = (visibleRect.w > 0.0f) ? visibleRect.Right() : ctrlRect.Right();
            const Rect barRect(
                anchorRight - barW - 10.0f * s,
                anchorTop + 8.0f * s,
                barW,
                barH
            );
            canvas.DrawRoundRect(barRect, 6.0f * s, th.palette.surfaceElevated, th.palette.accent, 1.2f);
            const float fs = th.GetFontSize(TextRole::BodySmall);
            const std::string disp = p->activeSearchQuery.empty()
                ? "Find in document..."
                : std::string(p->activeSearchQuery);
            canvas.DrawTextBox(
                disp,
                Rect(barRect.x + 10.0f * s, barRect.y, barW - 76.0f * s, barH),
                fs,
                p->activeSearchQuery.empty() ? th.palette.textMuted : th.palette.textPrimary,
                0.0f,
                0.5f
            );
            char countBuf[32];
            if (p->searchMatchCount > 0) {
                std::snprintf(countBuf, sizeof(countBuf), "%d/%d", p->activeSearchMatch + 1, p->searchMatchCount);
            } else {
                std::snprintf(countBuf, sizeof(countBuf), "0/0");
            }
            canvas.DrawTextBox(
                countBuf,
                Rect(barRect.Right() - 64.0f * s, barRect.y, 56.0f * s, barH),
                th.GetFontSize(TextRole::LabelSmall),
                th.palette.textMuted,
                1.0f,
                0.5f
            );
        }
    };

    node->uiSize = activeSize;
    node->disabled = effectiveDisabled;
    node->hovered = baseRes.hovered;
    node->pressed = baseRes.pressed;
    node->focused = isFocused;
    node->heightMode = SizeMode::Fit;

    ControlSpec ctrlSpec{};
    ctrlSpec.width = optsCopy.width;
    ctrlSpec.height = optsCopy.height;
    Control::SetupNode(ctx, node, ctrlSpec, optsCopy.stretch, optsCopy.align);
    if (!node->parent || node->parent->type != NodeType::Element) {
        if (optsCopy.height.has_value() && *optsCopy.height > 0.0f) {
            node->heightMode = SizeMode::Fixed;
            node->fixedHeight = theme.Scale(*optsCopy.height);
        } else {
            node->heightMode = SizeMode::Fit;
        }
        ctx.AdaptChildToContainer(node);
        if (!optsCopy.height.has_value() && node->heightMode == SizeMode::Stretch) {
            node->heightMode = SizeMode::Fit;
        }
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
    (void)size;
    const ParsedMarkdownDoc& doc = GetOrParseMarkdown(markdownText);
    const float effZoom = (opts.zoom != nullptr) ? std::clamp(*opts.zoom, 0.5f, 3.0f) : 1.0f;

    float ctrlW = (availableWidth > 0.0f) ? availableWidth : theme.Scale(520.0f);
    if (opts.stretch == StretchMode::Fill && availableWidth > 0.0f && opts.width.has_value()) {
        const float ratio = std::clamp(*opts.width, 0.1f, 1.0f);
        ctrlW = availableWidth * ratio;
    } else if (opts.width.has_value() && *opts.width > 0.0f) {
        ctrlW = theme.Scale(*opts.width);
    }

    const auto layout = GetOrBuildMarkdownLayout(canvas, ctrlW, doc, opts, theme, effZoom);

    const float finalH = (opts.height.has_value() && *opts.height > 0.0f)
        ? theme.Scale(*opts.height)
        : layout->totalHeight;
    return { std::max(ctrlW, layout->totalWidth), finalH };
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
    (void)size;
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    if (opts.background.IsBackgroundEnabled(false) || opts.border.color.HasAny() || opts.border.width > 0.0f) {
        const Color bgCol = opts.background.IsBackgroundEnabled(false)
            ? (opts.background.IsSurface() ? theme.palette.surface : opts.background.Resolve(false, false, disabled, theme.palette.surface))
            : Color::Transparent();
        const Color bCol = opts.border.color.Resolve(false, false, disabled, theme.palette.border);
        const float bW = (opts.border.width > 0.0f) ? theme.Scale(opts.border.width) : (opts.border.color.HasAny() ? theme.Scale(1.0f) : 0.0f);
        const float bR = opts.border.radius.has_value() ? theme.Scale(*opts.border.radius) : 0.0f;
        canvas.DrawRoundRect(bounds, bR, bgCol, bCol, bW);
    }

    const ParsedMarkdownDoc& doc = GetOrParseMarkdown(markdownText);
    const float effZoom = (opts.zoom != nullptr) ? std::clamp(*opts.zoom, 0.5f, 3.0f) : 1.0f;

    const auto layout = GetOrBuildMarkdownLayout(&canvas, bounds.w, doc, opts, theme, effZoom);
    const MarkdownPassResult hit = HitTestMarkdownLayout(
        *layout,
        bounds,
        mousePos,
        activeCopiedBlock,
        opts.interactiveTasks,
        opts.showCopyButton,
        disabled,
        nullptr
    );

    RenderMarkdownDocumentLayout(
        canvas,
        doc,
        *layout,
        bounds,
        bounds,
        opts,
        theme,
        mousePos,
        (hoveredCopyBlock >= 0) ? hoveredCopyBlock : hit.hoveredCopyBlock,
        activeCopiedBlock,
        hit.hoveredTaskIndex,
        nullptr
    );
}

} // namespace guikit
