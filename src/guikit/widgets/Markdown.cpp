#include "guikit/widgets/Markdown.hpp"
#include <algorithm>
#include <cctype>
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
    const UiTheme& theme
) {
    if (!overlay) return;

    // 1. Search match highlights
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
                    Rect(x0, tok.rect.y, x1 - x0, tok.rect.h),
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
                Rect(x0, tok.rect.y, x1 - x0, tok.rect.h),
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
    const UiTheme& theme
) {
    if (!overlay || !overlay->showCaret) return;
    if (overlay->caretOffset >= tok.charStart && overlay->caretOffset <= tok.charEnd) {
        const float cx = SubRangePixelBounds(&canvas, tok, fullPlainText, overlay->caretOffset, overlay->caretOffset).first;
        canvas.DrawLine(
            {cx, tok.rect.y + 1.0f},
            {cx, tok.rect.Bottom() - 1.0f},
            theme.palette.accent,
            1.5f
        );
    }
}

// --- Inline Rich-Text Span Flow Engine (Measure + Render + Link Hit-Testing + Token Recording) ---

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
    float codeBaseFontSize,
    float lineHeightScale,
    float firstLineIndent,
    Color baseColor,
    const UiTheme& theme,
    float zoom,
    Vec2 mousePos = {-1.0f, -1.0f},
    FontStyle overrideStyle = FontStyle::Regular,
    DocumentTextMap* textMap = nullptr,
    const RenderOverlayState* overlay = nullptr
) {
    FlowResult res{};
    if (spans.empty()) return res;

    const float s = theme.Scale(1.0f) * zoom;
    const float rawLineH = canvas ? canvas->GetLineHeight(baseFontSize) : theme.GetLineHeight(baseFontSize);
    const float lineH = rawLineH * std::clamp(lineHeightScale, 0.8f, 2.5f);
    const float maxW = (bounds.w > 0.0f) ? bounds.w : 10000.0f;

    float curX = std::max(0.0f, firstLineIndent);
    float curY = 0.0f;
    float maxLineW = 0.0f;

    auto measureToken = [&](std::string_view tok, FontStyle st, bool isCode) -> float {
        const float fs = isCode ? codeBaseFontSize : baseFontSize;
        float w = MeasureStyledSlice(canvas, tok, fs, st);
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
                if (textMap) textMap->AppendNewline();
            }
            const float codeFs = codeBaseFontSize;
            const Rect pillRect(
                bounds.x + curX,
                bounds.y + curY + 1.0f * s,
                tokW,
                std::max(codeFs + 4.0f * s, lineH - 2.0f * s)
            );
            const float padX = 5.0f * s;
            if (textMap) {
                textMap->AddToken(pillRect, span.text, codeFs, FontStyle::Monospace, padX);
            }
            if (doRender && canvas) {
                canvas->DrawRoundRect(
                    pillRect,
                    4.0f * s,
                    theme.palette.surfaceElevated,
                    theme.palette.borderSubtle,
                    1.0f
                );
                if (textMap && !textMap->tokens.empty()) {
                    DrawTokenSelectionAndSearchHighlights(*canvas, textMap->tokens.back(), textMap->plainText, overlay, theme);
                }
                canvas->DrawTextBox(
                    span.text,
                    pillRect,
                    codeFs,
                    theme.palette.accent,
                    0.5f,
                    0.5f
                );
                if (textMap && !textMap->tokens.empty()) {
                    DrawTokenCaretIfPresent(*canvas, textMap->tokens.back(), textMap->plainText, overlay, theme);
                }
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
                if (textMap) textMap->AppendNewline();
            }

            const Rect wordRect(bounds.x + curX, bounds.y + curY, wordW, lineH);
            const bool isLink = (span.kind == InlineKind::Link);
            const bool wordHovered = isLink && wordRect.Contains(mousePos);
            if (wordHovered) {
                res.hoveredLinkUrl = span.url;
                res.hoveredLinkRect = wordRect;
            }

            if (textMap) {
                textMap->AddToken(wordRect, word, baseFontSize, effStyle, 0.0f);
            }

            if (doRender && canvas) {
                if (textMap && !textMap->tokens.empty()) {
                    DrawTokenSelectionAndSearchHighlights(*canvas, textMap->tokens.back(), textMap->plainText, overlay, theme);
                }

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
                if (textMap && !textMap->tokens.empty()) {
                    DrawTokenCaretIfPresent(*canvas, textMap->tokens.back(), textMap->plainText, overlay, theme);
                }
            }

            curX += wordW;
        }
    }

    if (textMap) textMap->AppendNewline();
    maxLineW = std::max(maxLineW, curX);
    res.width = maxLineW;
    res.height = curY + lineH;
    return res;
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

MarkdownPassResult ExecuteMarkdownLayoutAndRender(
    ICanvas* canvas,
    bool doRender,
    Rect outerBounds,
    const ParsedMarkdownDoc& doc,
    const MarkdownOptions& opts,
    const UiTheme& theme,
    float zoom,
    Vec2 mousePos,
    int hoveredCopyBlock,
    int activeCopiedBlock,
    bool disabled,
    DocumentTextMap* textMap = nullptr,
    const RenderOverlayState* overlay = nullptr
) {
    MarkdownPassResult out{};
    if (textMap) {
        textMap->Clear();
    }

    const float effZoom = std::clamp(zoom, 0.5f, 3.0f);
    const float s = theme.Scale(1.0f) * effZoom;
    const float docPad = std::max(0.0f, opts.documentPadding) * s;

    float availW = std::max(120.0f, outerBounds.w - docPad * 2.0f);
    float startX = outerBounds.x + docPad;
    if (opts.maxContentWidth.has_value() && *opts.maxContentWidth > 0.0f) {
        const float maxColW = *opts.maxContentWidth * s;
        if (availW > maxColW) {
            startX += (availW - maxColW) * 0.5f;
            availW = maxColW;
        }
    }

    const Rect contentBounds(startX, outerBounds.y + docPad, availW, outerBounds.h);
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
                const float hFs = HeadingFontSize(blk.headingLevel, opts, theme, effZoom);
                const float headingInlineCodeFs = opts.inlineCodeFontSize.has_value()
                    ? (inlineCodeFs * (hFs / std::max(1.0f, bodyFs)))
                    : (hFs * 0.88f);
                const FlowResult fr = LayoutOrRenderSpans(
                    canvas,
                    doRender,
                    blk.spans,
                    Rect(contentBounds.x, curY, availW, 0.0f),
                    hFs,
                    headingInlineCodeFs,
                    lhScale,
                    0.0f,
                    theme.palette.textPrimary,
                    theme,
                    effZoom,
                    mousePos,
                    FontStyle::Bold,
                    textMap,
                    overlay
                );
                if (!fr.hoveredLinkUrl.empty()) {
                    out.hoveredLink = fr.hoveredLinkUrl;
                    out.hoveredLinkRect = fr.hoveredLinkRect;
                }
                curY += fr.height;
                if (opts.headingDividers && blk.headingLevel <= 2) {
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
                    inlineCodeFs,
                    lhScale,
                    paraIndent,
                    theme.palette.textPrimary,
                    theme,
                    effZoom,
                    mousePos,
                    FontStyle::Regular,
                    textMap,
                    overlay
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
                const float padX = blkPad + 2.0f * s;
                const float padY = blkPad * 0.7f;
                const float innerW = std::max(40.0f, availW - padX * 2.0f);
                const FlowResult measureFr = LayoutOrRenderSpans(
                    canvas,
                    false,
                    blk.spans,
                    Rect(contentBounds.x + padX, curY + padY, innerW, 0.0f),
                    bodyFs,
                    inlineCodeFs,
                    lhScale,
                    0.0f,
                    theme.palette.textSecondary,
                    theme,
                    effZoom,
                    mousePos,
                    FontStyle::Italic,
                    doRender ? nullptr : textMap,
                    nullptr
                );
                const float boxH = measureFr.height + padY * 2.0f;
                const Rect boxRect(contentBounds.x, curY, availW, boxH);

                if (doRender && canvas) {
                    canvas->DrawRoundRect(boxRect, blkRad, theme.palette.surfaceElevated.WithAlpha(140));
                    canvas->DrawRoundRect(Rect(boxRect.x, boxRect.y, 4.0f * s, boxRect.h), 2.0f * s, theme.palette.borderHover);
                    LayoutOrRenderSpans(
                        canvas,
                        true,
                        blk.spans,
                        Rect(contentBounds.x + padX, curY + padY, innerW, 0.0f),
                        bodyFs,
                        inlineCodeFs,
                        lhScale,
                        0.0f,
                        theme.palette.textSecondary,
                        theme,
                        effZoom,
                        mousePos,
                        FontStyle::Italic,
                        textMap,
                        overlay
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
                const float padX = blkPad + 4.0f * s;
                const float padY = blkPad * 0.85f;
                const float headerH = 20.0f * s;
                const float innerW = std::max(40.0f, availW - padX * 2.0f);

                const FlowResult measureFr = LayoutOrRenderSpans(
                    canvas,
                    false,
                    blk.spans,
                    Rect(contentBounds.x + padX, curY + padY + headerH + 4.0f * s, innerW, 0.0f),
                    bodyFs,
                    inlineCodeFs,
                    lhScale,
                    0.0f,
                    theme.palette.textPrimary,
                    theme,
                    effZoom,
                    mousePos,
                    FontStyle::Regular,
                    doRender ? nullptr : textMap,
                    nullptr
                );
                const float boxH = padY * 2.0f + headerH + 4.0f * s + measureFr.height;
                const Rect boxRect(contentBounds.x, curY, availW, boxH);

                if (doRender && canvas) {
                    canvas->DrawRoundRect(boxRect, blkRad, accent.WithAlpha(24), accent.WithAlpha(90), 1.0f);
                    canvas->DrawRoundRect(Rect(boxRect.x, boxRect.y, 4.0f * s, boxRect.h), 2.0f * s, accent);

                    const Icon ic = AlertIcon(blk.alertKind);
                    const float icSz = 15.0f * s;
                    ic.Render(*canvas, Rect(contentBounds.x + padX, curY + padY + (headerH - icSz) * 0.5f, icSz, icSz), accent);

                    const float titleFs = theme.GetFontSize(TextRole::LabelLarge) * effZoom;
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
                        inlineCodeFs,
                        lhScale,
                        0.0f,
                        theme.palette.textPrimary,
                        theme,
                        effZoom,
                        mousePos,
                        FontStyle::Regular,
                        textMap,
                        overlay
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
                const float indent = static_cast<float>(blk.indentLevel) * listStep + (listStep + 4.0f * s);
                const float itemX = contentBounds.x + indent;
                const float itemW = std::max(40.0f, availW - indent);
                const float lineH = (canvas ? canvas->GetLineHeight(bodyFs) : theme.GetLineHeight(bodyFs)) * lhScale;

                bool isTaskChecked = (blk.taskState == 1);
                if (blk.taskIndex >= 0 && overlay && overlay->taskOverrides) {
                    auto it = overlay->taskOverrides->find(blk.taskIndex);
                    if (it != overlay->taskOverrides->end()) {
                        isTaskChecked = it->second;
                    }
                }

                const float markerX = itemX - 18.0f * s;
                if (blk.taskState >= 0) {
                    const float cbSz = 14.0f * s;
                    const Rect cbRect(markerX, curY + (lineH - cbSz) * 0.5f, cbSz, cbSz);
                    if (opts.interactiveTasks && !disabled && cbRect.Inset(-3.0f * s).Contains(mousePos)) {
                        out.hoveredTaskIndex = blk.taskIndex;
                        out.hoveredTaskCurrentState = isTaskChecked;
                    }
                    if (doRender && canvas) {
                        const bool cbHovered = (out.hoveredTaskIndex == blk.taskIndex && blk.taskIndex >= 0);
                        if (isTaskChecked) {
                            canvas->DrawRoundRect(cbRect, 3.0f * s, theme.palette.accent);
                            Icon::Check().Render(*canvas, cbRect.Inset(2.0f * s), Color(255, 255, 255, 255));
                        } else {
                            canvas->DrawRoundRect(
                                cbRect,
                                3.0f * s,
                                theme.palette.surfaceElevated,
                                cbHovered ? theme.palette.accent : theme.palette.borderHover,
                                cbHovered ? 1.5f : 1.2f
                            );
                        }
                    }
                } else if (doRender && canvas) {
                    if (blk.isOrderedList) {
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
                    inlineCodeFs,
                    lhScale,
                    0.0f,
                    isTaskChecked ? theme.palette.textSecondary : theme.palette.textPrimary,
                    theme,
                    effZoom,
                    mousePos,
                    FontStyle::Regular,
                    textMap,
                    overlay
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
                // Split lines
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

                const float codeStartX = boxRect.x + gutterW + padX;
                const float spaceW = MeasureStyledSlice(canvas, " ", codeFs, FontStyle::Monospace);

                // Record code lines into DocumentTextMap for selection, cursor movement, and search
                std::vector<size_t> codeLineTokenIndices;
                if (textMap) {
                    codeLineTokenIndices.reserve(codeLines.size());
                    for (size_t ln = 0; ln < codeLines.size(); ++ln) {
                        const float ly = boxRect.y + headerH + padY + static_cast<float>(ln) * codeLineH;
                        const std::string_view lineStr = codeLines[ln];
                        const float lineW = std::max(spaceW, MeasureStyledSlice(canvas, lineStr, codeFs, FontStyle::Monospace));
                        const Rect lineRect(codeStartX, ly, lineW, codeLineH);
                        codeLineTokenIndices.push_back(textMap->tokens.size());
                        if (!lineStr.empty()) {
                            textMap->AddToken(lineRect, lineStr, codeFs, FontStyle::Monospace, 0.0f);
                        } else {
                            textMap->AddToken(lineRect, " ", codeFs, FontStyle::Monospace, 0.0f);
                        }
                        textMap->AppendNewline();
                    }
                }

                if (doRender && canvas) {
                    // Resolve language-specific syntax rules + optional custom SyntaxPalette from TextArea!
                    SyntaxConfig synCfg = SyntaxConfig::ForLanguage(blk.codeLang);
                    if (opts.codePalette.has_value()) {
                        synCfg.palette = *opts.codePalette;
                    }
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

                    canvas->DrawRoundRect(boxRect, blkRad, codeBg, theme.palette.borderSubtle, 1.0f);
                    // Header background & divider
                    canvas->DrawRoundRect(Rect(boxRect.x, boxRect.y, boxRect.w, headerH), blkRad, gutterBg);
                    canvas->DrawLine(
                        {boxRect.x, boxRect.y + headerH},
                        {boxRect.Right(), boxRect.y + headerH},
                        theme.palette.borderSubtle,
                        1.0f
                    );

                    const float lblFs = theme.GetFontSize(TextRole::LabelSmall) * effZoom;
                    const std::string_view langLabel = blk.codeLang.empty() ? "code" : std::string_view(blk.codeLang);
                    canvas->DrawTextStyled(
                        langLabel,
                        {boxRect.x + padX, boxRect.y + (headerH - lblFs) * 0.5f},
                        lblFs,
                        lineNumCol,
                        FontStyle::Bold
                    );

                    if (opts.showCopyButton) {
                        const bool isHov = (hoveredCopyBlock == blk.codeBlockIndex);
                        const bool isCopied = (activeCopiedBlock == blk.codeBlockIndex);
                        const Color btnBg = isCopied
                            ? theme.GetVariant(UiVariant::Success).bg.WithAlpha(40)
                            : (isHov ? theme.palette.accent.WithAlpha(35) : codeBg);
                        const Color btnBorder = isCopied
                            ? theme.GetVariant(UiVariant::Success).bg
                            : (isHov ? theme.palette.accent : theme.palette.borderSubtle);
                        const Color btnText = isCopied
                            ? theme.GetVariant(UiVariant::Success).bg
                            : (isHov ? theme.palette.accent : defaultCodeCol);
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

                    // Render code lines with gutter, indent guides, whitespace markers, selection & syntax highlighting
                    canvas->PushClip(Rect(boxRect.x + 1.0f, boxRect.y + headerH + 1.0f, boxRect.w - 2.0f, bodyH - 2.0f));
                    if (opts.codeLineNumbers) {
                        canvas->DrawRect(Rect(boxRect.x, boxRect.y + headerH, gutterW, bodyH), gutterBg.WithAlpha(180));
                        canvas->DrawLine(
                            {boxRect.x + gutterW, boxRect.y + headerH},
                            {boxRect.x + gutterW, boxRect.Bottom()},
                            theme.palette.borderSubtle,
                            1.0f
                        );
                    }

                    const float codeGlyphH = canvas->MeasureTextStyled("Mg", codeFs, FontStyle::Monospace).y;
                    std::vector<TextSpan> syntaxSpans;
                    bool inBlockComment = false;

                    for (size_t ln = 0; ln < codeLines.size(); ++ln) {
                        const float ly = boxRect.y + headerH + padY + static_cast<float>(ln) * codeLineH;
                        const float textY = ly + (codeLineH - ((codeGlyphH > 0.0f) ? codeGlyphH : codeFs)) * 0.5f;

                        if (opts.codeLineNumbers) {
                            char lnBuf[16];
                            std::snprintf(lnBuf, sizeof(lnBuf), "%zu", ln + 1);
                            canvas->DrawTextBox(
                                lnBuf,
                                Rect(boxRect.x + 4.0f * s, ly, gutterW - 10.0f * s, codeLineH),
                                theme.GetFontSize(TextRole::CodeSmall) * effZoom,
                                lineNumCol,
                                1.0f,
                                0.5f
                            );
                        }

                        if (textMap && ln < codeLineTokenIndices.size()) {
                            const PositionedToken& tok = textMap->tokens[codeLineTokenIndices[ln]];
                            DrawTokenSelectionAndSearchHighlights(*canvas, tok, textMap->plainText, overlay, theme);
                        }

                        const std::string_view lineStr = codeLines[ln];

                        // Indent guides (every 4 leading spaces)
                        if (opts.codeIndentGuides && !lineStr.empty() && spaceW > 0.0f) {
                            size_t leadSpaces = 0;
                            while (leadSpaces < lineStr.size() && lineStr[leadSpaces] == ' ') ++leadSpaces;
                            for (size_t col = 4; col <= leadSpaces; col += 4) {
                                const float gx = std::round(codeStartX + static_cast<float>(col) * spaceW);
                                canvas->DrawLine({gx, ly}, {gx, ly + codeLineH}, theme.palette.borderSubtle.WithAlpha(90), 1.0f);
                            }
                        }

                        // Visible whitespace markers (space dots and tab arrows)
                        if (opts.codeShowWhitespace && !lineStr.empty() && spaceW > 0.0f) {
                            const float dotR = std::max(1.2f, 1.4f * s);
                            const float cy = ly + codeLineH * 0.5f;
                            for (size_t c = 0; c < lineStr.size(); ++c) {
                                if (lineStr[c] == ' ') {
                                    const float cx = codeStartX + (static_cast<float>(c) + 0.5f) * spaceW;
                                    canvas->DrawRoundRect(Rect(cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f), dotR, wsColor);
                                } else if (lineStr[c] == '\t') {
                                    const float x0 = codeStartX + static_cast<float>(c) * spaceW + 2.0f * s;
                                    const float x1 = codeStartX + static_cast<float>(c + 1) * spaceW - 2.0f * s;
                                    if (x1 > x0) {
                                        canvas->DrawLine({x0, cy}, {x1, cy}, wsColor, 1.2f);
                                        canvas->DrawLine({x1 - 3.0f * s, cy - 2.5f * s}, {x1, cy}, wsColor, 1.2f);
                                        canvas->DrawLine({x1 - 3.0f * s, cy + 2.5f * s}, {x1, cy}, wsColor, 1.2f);
                                    }
                                }
                            }
                        }

                        if (opts.syntaxHighlight && !lineStr.empty()) {
                            syntaxSpans.clear();
                            TextArea::HighlightLine(
                                lineStr,
                                ln,
                                synCfg,
                                syntaxSpans,
                                theme,
                                opts.codePalette.has_value() ? &(*opts.codePalette) : nullptr,
                                &inBlockComment
                            );
                            if (syntaxSpans.empty()) {
                                canvas->DrawTextStyled(lineStr, {codeStartX, textY}, codeFs, defaultCodeCol, FontStyle::Monospace);
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
                            canvas->DrawTextStyled(lineStr, {codeStartX, textY}, codeFs, defaultCodeCol, FontStyle::Monospace);
                        }

                        if (textMap && ln < codeLineTokenIndices.size()) {
                            const PositionedToken& tok = textMap->tokens[codeLineTokenIndices[ln]];
                            DrawTokenCaretIfPresent(*canvas, tok, textMap->plainText, overlay, theme);
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
                const Vec2 scaledNat = { nat.x * s, nat.y * s };
                const float maxH = opts.imageMaxHeight * s;
                const float scaleFactor = std::min({1.0f, availW / scaledNat.x, maxH / scaledNat.y});
                const float imgW = std::max(40.0f, scaledNat.x * scaleFactor);
                const float imgH = std::max(30.0f, scaledNat.y * scaleFactor);
                const float imgX = contentBounds.x + (availW - imgW) * 0.5f;
                const Rect imgRect(imgX, curY, imgW, imgH);
                const float rad = opts.imageRadius * s;

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
                            theme.GetFontSize(TextRole::LabelMedium) * effZoom,
                            theme.palette.textMuted,
                            0.5f,
                            0.5f
                        );
                    }
                    canvas->DrawRoundRect(imgRect, rad, Color::Transparent(), theme.palette.borderSubtle, 1.0f);
                }
                curY += imgH;

                if (!blk.imageAlt.empty()) {
                    const float capFs = theme.GetFontSize(TextRole::LabelSmall) * effZoom;
                    const float capH = (canvas ? canvas->GetLineHeight(capFs) : theme.GetLineHeight(capFs)) * lhScale + 4.0f * s;
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
                const float rowH = (canvas ? canvas->GetLineHeight(bodyFs) : theme.GetLineHeight(bodyFs)) * lhScale + cellPadY * 2.0f;
                const float tableH = static_cast<float>(blk.tableRows.size()) * rowH;
                const Rect tableRect(contentBounds.x, curY, availW, tableH);

                if (doRender && canvas) {
                    canvas->DrawRoundRect(tableRect, blkRad, theme.palette.surface, theme.palette.borderSubtle, 1.0f);
                }

                for (size_t r = 0; r < blk.tableRows.size(); ++r) {
                    const float ry = curY + static_cast<float>(r) * rowH;
                    const Rect rowRect(contentBounds.x, ry, availW, rowH);
                    if (doRender && canvas) {
                        if (r == 0) {
                            canvas->DrawRoundRect(rowRect, blkRad, theme.palette.surfaceElevated);
                        } else if (r % 2 == 1) {
                            canvas->DrawRect(rowRect.Inset(1.0f, 0.0f), theme.palette.surfaceElevated.WithAlpha(90));
                        }
                        if (r > 0) {
                            canvas->DrawLine({rowRect.x, ry}, {rowRect.Right(), ry}, theme.palette.borderSubtle, 1.0f);
                        }
                    }

                    const auto& rowCells = blk.tableRows[r];
                    for (size_t c = 0; c < numCols; ++c) {
                        const float cx = contentBounds.x + static_cast<float>(c) * colW;
                        if (doRender && canvas && c > 0) {
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
                                inlineCodeFs,
                                lhScale,
                                0.0f,
                                theme.palette.textPrimary,
                                theme,
                                effZoom,
                                mousePos,
                                cellStyle,
                                nullptr,
                                nullptr
                            );
                            float offsetX = 0.0f;
                            if (colAlign == Align::Center) offsetX = std::max(0.0f, (cellInner.w - cfr.width) * 0.5f);
                            else if (colAlign == Align::End) offsetX = std::max(0.0f, cellInner.w - cfr.width);

                            const FlowResult rfr = LayoutOrRenderSpans(
                                canvas,
                                doRender,
                                rowCells[c].spans,
                                Rect(cellInner.x + offsetX, cellInner.y, cellInner.w - offsetX, cellInner.h),
                                bodyFs,
                                inlineCodeFs,
                                lhScale,
                                0.0f,
                                theme.palette.textPrimary,
                                theme,
                                effZoom,
                                mousePos,
                                cellStyle,
                                textMap,
                                overlay
                            );
                            if (!rfr.hoveredLinkUrl.empty()) {
                                out.hoveredLink = rfr.hoveredLinkUrl;
                                out.hoveredLinkRect = rfr.hoveredLinkRect;
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

    out.totalHeight = std::max(0.0f, (curY - outerBounds.y) + docPad);
    out.totalWidth += docPad * 2.0f;
    return out;
}

struct MarkdownState {
    uint64_t docSourceHash{0};
    int copiedBlockIndex{-1};
    float zoom{1.0f};

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
};

ElementSpec MakeMarkdownSpec(const MarkdownOptions& opts, UiSize size, const UiTheme& theme, bool disabled) noexcept {
    return ElementSpec{
        .label = opts.label,
        .description = opts.description,
        .layout = opts.layout,
        .inlineOrder = opts.inlineOrder,
        .align = opts.align,
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
    const ParsedMarkdownDoc& doc = GetOrParseMarkdown(markdownText);
    if (state.docSourceHash != doc.sourceHash) {
        state.docSourceHash = doc.sourceHash;
        state.cursorOffset = 0;
        state.selAnchorOffset = 0;
        state.preferredCursorX = -1.0f;
        state.taskOverrides.clear();
    }

    if (opts.zoom != nullptr) {
        state.zoom = std::clamp(*opts.zoom, 0.5f, 3.0f);
    }

    Response baseRes = ctx.Interact(id);
    MarkdownResponse res{};
    static_cast<Response&>(res) = baseRes;

    const bool isFocused = (ctx.GetFocusedId() == id);

    // 1. Handle Smooth Zoom (Ctrl+Wheel & KeyAction::ZoomIn/ZoomOut/ZoomReset)
    if (opts.allowZoom && !effectiveDisabled) {
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
            if (opts.zoom != nullptr) {
                *opts.zoom = state.zoom;
            }
            res.changed = true;
            ctx.RequestRepaint(2);
        }
    }
    res.currentZoom = state.zoom;

    // 2. Build Layout & DocumentTextMap on previous unclipped bounds for hit-testing, selection, cursor, and search
    ICanvas* hitCanvas = ctx.GetCanvas();
    Rect hitBounds = ctx.GetPrevUnclippedBounds(id);
    if (hitBounds.w <= 0.0f) {
        hitBounds = Rect(0.0f, 0.0f, theme.Scale(520.0f), theme.Scale(400.0f));
    }
    const ElementSpec spec = MakeMarkdownSpec(opts, activeSize, theme, effectiveDisabled);
    const Rect ctrlRect = Element::Resolve(hitBounds, {hitBounds.w, hitBounds.h}, spec, hitCanvas, theme).controlRect;

    DocumentTextMap textMap{};
    RenderOverlayState hitOverlay{};
    hitOverlay.taskOverrides = &state.taskOverrides;

    const MarkdownPassResult hit = ExecuteMarkdownLayoutAndRender(
        hitCanvas,
        false,
        ctrlRect,
        doc,
        opts,
        theme,
        state.zoom,
        ctx.GetMousePos(),
        -1,
        state.copiedBlockIndex,
        effectiveDisabled,
        &textMap,
        &hitOverlay
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
                    if (opts.onLinkClick) {
                        opts.onLinkClick(hit.hoveredLink);
                    }
                }
            } else if (hit.hoveredCopyBlock >= 0) {
                if (baseRes.clicked) {
                    state.copiedBlockIndex = hit.hoveredCopyBlock;
                    res.copiedBlockIndex = hit.hoveredCopyBlock;
                    res.changed = true;
                    ctx.SetClipboardText(hit.hoveredCopyCode);
                }
            } else if (hit.hoveredTaskIndex >= 0 && opts.interactiveTasks) {
                if (baseRes.clicked) {
                    const bool nextChecked = !hit.hoveredTaskCurrentState;
                    state.taskOverrides[hit.hoveredTaskIndex] = nextChecked;
                    res.toggledTaskIndex = hit.hoveredTaskIndex;
                    res.toggledTaskChecked = nextChecked;
                    res.changed = true;
                    if (opts.onTaskToggle) {
                        opts.onTaskToggle(hit.hoveredTaskIndex, nextChecked);
                    }
                }
            }
        }

        // Mouse click / double-click / drag selection
        const bool clickedInteractiveSubElement =
            !hit.hoveredLink.empty() || hit.hoveredCopyBlock >= 0 || (hit.hoveredTaskIndex >= 0 && opts.interactiveTasks);

        if (opts.enableSelection && baseRes.hovered && ctx.IsMouseClicked() && !clickedInteractiveSubElement) {
            ctx.RequestFocus(id);
            const size_t hitOff = HitTestDocumentCharOffset(textMap, hitCanvas, ctx.GetMousePos());
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
                state.cursorOffset = HitTestDocumentCharOffset(textMap, hitCanvas, ctx.GetMousePos());
                state.preferredCursorX = -1.0f;
            } else {
                state.isDraggingSelection = false;
            }
        }
    }

    // 4. Handle Keyboard Actions via centralized KeyAction system (Search, Copy, SelectAll, Cursor & Selection Navigation)
    if ((isFocused || baseRes.hovered) && !effectiveDisabled) {
        if (opts.enableSearch && ctx.ConsumeAction(KeyAction::Find)) {
            state.searchBarOpen = true;
            ctx.RequestFocus(id);
            ctx.RequestRepaint(2);
        }
    }

    if (isFocused && !effectiveDisabled) {
        if (state.searchBarOpen && opts.enableSearch) {
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

        if (opts.enableSelection) {
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
    const std::string_view effectiveQuery = !opts.searchQuery.empty()
        ? opts.searchQuery
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
    payload->opts = opts;
    payload->opts.label = ctx.AllocText(opts.label);
    payload->opts.description = ctx.AllocText(opts.description);
    payload->zoom = state.zoom;
    payload->hoveredCopyBlock = hoveredCopyBlock;
    payload->activeCopiedBlock = state.copiedBlockIndex;
    payload->hasSelection = opts.enableSelection && state.HasSelection();
    payload->selMin = state.SelMin();
    payload->selMax = state.SelMax();
    payload->showCaret = opts.enableSelection && isFocused;
    payload->caretOffset = state.cursorOffset;
    payload->searchBarOpen = state.searchBarOpen && opts.enableSearch;
    payload->activeSearchQuery = ctx.AllocString(effectiveQuery);
    payload->activeSearchMatch = res.activeSearchMatch;
    payload->searchMatchCount = res.searchMatchCount;
    payload->taskOverrides = &state.taskOverrides;
    node->SetPayload(payload);

    node->visualHash = detail::HashArgs(
        doc.sourceHash,
        payload->opts.label,
        payload->opts.description,
        state.zoom,
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
        opts.showCopyButton,
        opts.codeShowWhitespace,
        opts.codeIndentGuides,
        opts.codePalette.has_value() ? opts.codePalette->editorBg.ToUint32() : 0u,
        opts.codePalette.has_value() ? opts.codePalette->keyword.ToUint32() : 0u,
        opts.imageCheckerboard,
        opts.imageMaxHeight,
        opts.imageRadius,
        hoveredCopyBlock,
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
        MarkdownOptions mOpts = p->opts;
        float z = p->zoom;
        mOpts.zoom = &z;
        return Markdown::Measure(p->text, mOpts, n->uiSize, canvas, th, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& canvas, const UiTheme& th) noexcept {
        const auto* p = n->As<MarkdownPayload>();
        if (!p) return;

        const ParsedMarkdownDoc& parsedDoc = GetOrParseMarkdown(p->text);
        const ElementSpec elSpec = MakeMarkdownSpec(p->opts, n->uiSize, th, n->disabled);

        const MarkdownPassResult mRes = ExecuteMarkdownLayoutAndRender(
            &canvas,
            false,
            Rect(0.0f, 0.0f, n->computedRect.w, 0.0f),
            parsedDoc,
            p->opts,
            th,
            p->zoom,
            {-1.0f, -1.0f},
            p->hoveredCopyBlock,
            p->activeCopiedBlock,
            n->disabled,
            nullptr,
            nullptr
        );

        const ElementGeometry geom = Element::Resolve(
            n->computedRect,
            {n->computedRect.w, mRes.totalHeight},
            elSpec,
            &canvas,
            th
        );
        Element::RenderText(canvas, geom, elSpec, th);

        DocumentTextMap renderMap{};
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
            // Pre-pass to collect plainText so search match ranges are ready before drawing tokens
            ExecuteMarkdownLayoutAndRender(
                &canvas,
                false,
                geom.controlRect,
                parsedDoc,
                p->opts,
                th,
                p->zoom,
                {-1.0f, -1.0f},
                p->hoveredCopyBlock,
                p->activeCopiedBlock,
                n->disabled,
                &renderMap,
                nullptr
            );
            matches = FindSearchMatches(renderMap.plainText, p->activeSearchQuery);
            overlay.searchMatches = &matches;
        }

        ExecuteMarkdownLayoutAndRender(
            &canvas,
            true,
            geom.controlRect,
            parsedDoc,
            p->opts,
            th,
            p->zoom,
            {-1.0f, -1.0f},
            p->hoveredCopyBlock,
            p->activeCopiedBlock,
            n->disabled,
            &renderMap,
            &overlay
        );

        // Render floating in-document Search bar if open
        if (p->searchBarOpen) {
            const float s = th.Scale(1.0f);
            const float barW = 240.0f * s;
            const float barH = 30.0f * s;
            const Rect barRect(
                geom.controlRect.Right() - barW - 10.0f * s,
                geom.controlRect.y + 8.0f * s,
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
    const float effZoom = (opts.zoom != nullptr) ? std::clamp(*opts.zoom, 0.5f, 3.0f) : 1.0f;

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
        effZoom,
        {-1.0f, -1.0f},
        -1,
        -1,
        opts.disabled
    );

    const Vec2 ctrlSize = { std::max(ctrlW, res.totalWidth), res.totalHeight };
    return Element::Measure(ctrlSize, spec, canvas, theme, availableWidth);
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
    const float effZoom = (opts.zoom != nullptr) ? std::clamp(*opts.zoom, 0.5f, 3.0f) : 1.0f;

    const MarkdownPassResult mRes = ExecuteMarkdownLayoutAndRender(
        &canvas,
        false,
        Rect(0.0f, 0.0f, bounds.w, 0.0f),
        doc,
        opts,
        theme,
        effZoom,
        mousePos,
        hoveredCopyBlock,
        activeCopiedBlock,
        disabled
    );

    const ElementGeometry geom = Element::Resolve(
        bounds,
        {bounds.w, mRes.totalHeight},
        spec,
        &canvas,
        theme
    );
    Element::RenderText(canvas, geom, spec, theme);

    ExecuteMarkdownLayoutAndRender(
        &canvas,
        true,
        geom.controlRect,
        doc,
        opts,
        theme,
        effZoom,
        mousePos,
        hoveredCopyBlock,
        activeCopiedBlock,
        disabled
    );
}

} // namespace guikit
