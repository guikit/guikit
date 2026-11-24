#include "text_area_internal.hpp"

namespace guikit {

namespace detail::ta_impl {

bool ContainsSizeT(keel::ArrayRef<keel::usize> list, keel::usize val) noexcept {
    for (keel::usize item : list) {
        if (item == val) return true;
    }
    return false;
}

void SortSizeT(keel::MutArrayRef<keel::usize> arr) noexcept {
    const keel::usize n = arr.size();
    for (keel::usize gap = n / 2; gap > 0; gap /= 2) {
        for (keel::usize i = gap; i < n; ++i) {
            const keel::usize temp = arr[i];
            keel::usize j = i;
            while (j >= gap && arr[j - gap] > temp) {
                arr[j] = arr[j - gap];
                j -= gap;
            }
            arr[j] = temp;
        }
    }
}

void StringEraseSpan(keel::String& s, keel::usize pos, keel::usize len) {
    if (pos >= s.size() || len == 0) return;
    const keel::usize rem = keel::min(len, s.size() - pos);
    const keel::usize tail = s.size() - (pos + rem);
    if (tail > 0) {
        keel::mem_move(s.data() + pos, s.data() + pos + rem, tail);
    }
    s.resize(s.size() - rem);
}

void StringInsertSpan(keel::String& s, keel::usize pos, keel::StringRef ins) {
    if (ins.empty()) return;
    const keel::usize oldSize = s.size();
    const keel::usize clampedPos = keel::min(pos, oldSize);
    s.resize(oldSize + ins.size());
    const keel::usize tail = oldSize - clampedPos;
    if (tail > 0) {
        keel::mem_move(s.data() + clampedPos + ins.size(), s.data() + clampedPos, tail);
    }
    keel::mem_copy(s.data() + clampedPos, ins.data(), ins.size());
}

keel::f32 MeasureChunkNoTabs(
    ICanvas* canvas,
    keel::StringRef chunk,
    keel::f32 fontSize,
    FontStyle style
) noexcept {
    if (chunk.empty() || fontSize <= 0.0f) return 0.0f;
    if (canvas) {
        return canvas->MeasureTextStyled(chunk, fontSize, style).x;
    }
    const keel::f32 factor = (style == FontStyle::Monospace) ? 0.60f : 0.56f;
    return keel::as<keel::f32>(chunk.size()) * fontSize * factor;
}

keel::f32 MeasureSegmentWidth(
    ICanvas* canvas,
    keel::StringRef seg,
    keel::f32 fontSize,
    FontStyle style,
    keel::u8 tabSize
) noexcept {
    if (seg.empty() || fontSize <= 0.0f) return 0.0f;
    const keel::usize firstTab = seg.find('\t');
    if (firstTab == keel::StringRef::npos) {
        return MeasureChunkNoTabs(canvas, seg, fontSize, style);
    }

    const keel::f32 tabCols = keel::as<keel::f32>(keel::clamp<keel::i32>(tabSize, 1, 16));
    const keel::f32 tabW = MeasureChunkNoTabs(canvas, " ", fontSize, style) * tabCols;
    keel::f32 totalW = 0.0f;
    keel::usize pos = 0;
    while (pos < seg.size()) {
        if (seg[pos] == '\t') {
            keel::usize tabEnd = pos + 1;
            while (tabEnd < seg.size() && seg[tabEnd] == '\t') {
                ++tabEnd;
            }
            totalW += keel::as<keel::f32>(tabEnd - pos) * tabW;
            pos = tabEnd;
        } else {
            keel::usize nextTab = seg.find('\t', pos);
            if (nextTab == keel::StringRef::npos) {
                nextTab = seg.size();
            }
            totalW += MeasureChunkNoTabs(canvas, seg.substr(pos, nextTab - pos), fontSize, style);
            pos = nextTab;
        }
    }
    return totalW;
}

keel::usize HitTestColumnInLine(
    ICanvas* canvas,
    keel::StringRef lineSub,
    keel::f32 localX,
    keel::f32 fontSize,
    FontStyle style,
    keel::u8 tabSize
) noexcept {
    if (lineSub.empty() || localX <= 0.0f) return 0;
    keel::usize pos = 0;
    keel::f32 prevW = 0.0f;
    while (pos < lineSub.size()) {
        const keel::usize nextPos = TextUtil::NextCharBoundary(lineSub, pos);
        const keel::f32 nextW = MeasureSegmentWidth(canvas, lineSub.substr(0, nextPos), fontSize, style, tabSize);
        const keel::f32 mid = (prevW + nextW) * 0.5f;
        if (localX < mid) {
            return pos;
        }
        prevW = nextW;
        pos = nextPos;
    }
    return lineSub.size();
}


void SortFoldRegions(keel::MutArrayRef<FoldRegion> arr) noexcept {
    const keel::usize n = arr.size();
    for (keel::usize gap = n / 2; gap > 0; gap /= 2) {
        for (keel::usize i = gap; i < n; ++i) {
            const FoldRegion temp = arr[i];
            keel::usize j = i;
            while (j >= gap) {
                const FoldRegion& prev = arr[j - gap];
                const bool tempBeforePrev =
                    temp.startLine < prev.startLine ||
                    (temp.startLine == prev.startLine && temp.endLine > prev.endLine);
                if (!tempBeforePrev) break;
                arr[j] = prev;
                j -= gap;
            }
            arr[j] = temp;
        }
    }
}

void SplitLogicalLines(keel::StringRef text, keel::Array<LogicalLine>& outLines) {
    outLines.clear();
    keel::usize start = 0;
    while (start <= text.size()) {
        const keel::usize nl = text.find('\n', start);
        if (nl == keel::StringRef::npos) {
            keel::usize len = text.size() - start;
            if (len > 0 && text[start + len - 1] == '\r') {
                --len;
            }
            outLines.push_back({start, len});
            break;
        }
        keel::usize len = nl - start;
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

void ComputeFoldRegions(
    keel::StringRef text,
    keel::ArrayRef<LogicalLine> logicalLines,
    const SyntaxConfig* synCfg,
    keel::Array<FoldRegion>& outRegions
) {
    outRegions.clear();
    if (logicalLines.size() < 2) return;

    struct OpenEntry {
        keel::usize lineIdx{0};
        bool isKeyword{false};
    };
    keel::SmallArray<OpenEntry, 16> stack;

    const keel::StringRef lineComment = synCfg ? synCfg->lineComment : "//";
    const auto openKw = synCfg ? synCfg->foldOpenKeywords : keel::ArrayRef<keel::StringRef>{};
    const auto closeKw = synCfg ? synCfg->foldCloseKeywords : keel::ArrayRef<keel::StringRef>{};

    for (keel::usize i = 0; i < logicalLines.size(); ++i) {
        const keel::StringRef raw = text.substr(logicalLines[i].startOffset, logicalLines[i].length);
        keel::usize p = 0;
        while (p < raw.size() && (raw[p] == ' ' || raw[p] == '\t')) ++p;
        if (p >= raw.size()) continue;
        if (!lineComment.empty() && raw.substr(p).starts_with(lineComment)) continue;

        // Check leading keyword opener/closer (e.g. MATLAB function/if/for ... end)
        if (!openKw.empty() || !closeKw.empty()) {
            if (IsIdentStart(raw[p])) {
                keel::usize wEnd = p + 1;
                while (wEnd < raw.size() && IsIdentChar(raw[wEnd])) ++wEnd;
                const keel::StringRef firstWord = raw.substr(p, wEnd - p);
                if (ContainsToken(openKw, firstWord)) {
                    // Check that the same line doesn't already end with 'end'
                    bool closedOnSameLine = false;
                    keel::usize scan = wEnd;
                    while (scan < raw.size()) {
                        if (!lineComment.empty() && raw.substr(scan).starts_with(lineComment)) break;
                        if (IsIdentStart(raw[scan])) {
                            keel::usize e2 = scan + 1;
                            while (e2 < raw.size() && IsIdentChar(raw[e2])) ++e2;
                            if (ContainsToken(closeKw, raw.substr(scan, e2 - scan))) {
                                closedOnSameLine = true;
                            }
                            scan = e2;
                        } else {
                            ++scan;
                        }
                    }
                    if (!closedOnSameLine) {
                        stack.push_back({i, true});
                    }
                } else if (ContainsToken(closeKw, firstWord)) {
                    for (keel::usize sIdx = stack.size(); sIdx > 0; --sIdx) {
                        const keel::usize k = sIdx - 1;
                        if (stack[k].isKeyword) {
                            if (i > stack[k].lineIdx) {
                                outRegions.push_back({stack[k].lineIdx, i});
                            }
                            stack.remove_at(k);
                            break;
                        }
                    }
                }
            }
        }

        // Check brace '{' and '}' blocks
        bool inStr = false;
        char quoteC = '\0';
        for (keel::usize cIdx = p; cIdx < raw.size(); ++cIdx) {
            const char c = raw[cIdx];
            if (!inStr && !lineComment.empty() && raw.substr(cIdx).starts_with(lineComment)) {
                break;
            }
            if (c == '"' || c == '\'') {
                if (!inStr) {
                    if (c == '\'' && synCfg && synCfg->transposeApostrophe && cIdx > 0) {
                        const char prev = raw[cIdx - 1];
                        if (IsIdentChar(prev) || prev == ')' || prev == ']' || prev == '}' || prev == '.') {
                            continue;
                        }
                    }
                    inStr = true;
                    quoteC = c;
                } else if (quoteC == c && (cIdx == 0 || raw[cIdx - 1] != '\\')) {
                    inStr = false;
                }
                continue;
            }
            if (inStr) continue;

            if (c == '{') {
                stack.push_back({i, false});
            } else if (c == '}') {
                for (keel::usize sIdx = stack.size(); sIdx > 0; --sIdx) {
                    const keel::usize k = sIdx - 1;
                    if (!stack[k].isKeyword) {
                        if (i > stack[k].lineIdx) {
                            outRegions.push_back({stack[k].lineIdx, i});
                        }
                        stack.remove_at(k);
                        break;
                    }
                }
            }
        }
    }

    SortFoldRegions(outRegions);
}

const FoldRegion* FindFoldRegionStartingAt(keel::ArrayRef<FoldRegion> regions, keel::usize lineIdx) noexcept {
    for (const FoldRegion& r : regions) {
        if (r.startLine == lineIdx) return &r;
        if (r.startLine > lineIdx) break;
    }
    return nullptr;
}

bool IsLineFoldedStart(keel::ArrayRef<keel::usize> foldedLines, keel::usize lineIdx) noexcept {
    for (keel::usize fl : foldedLines) {
        if (fl == lineIdx) return true;
    }
    return false;
}

bool IsLineHiddenByFold(
    keel::ArrayRef<FoldRegion> regions,
    keel::ArrayRef<keel::usize> foldedLines,
    keel::usize lineIdx
) noexcept {
    if (foldedLines.empty() || regions.empty()) return false;
    for (keel::usize fl : foldedLines) {
        if (lineIdx <= fl) continue;
        if (const FoldRegion* r = FindFoldRegionStartingAt(regions, fl)) {
            if (lineIdx > r->startLine && lineIdx <= r->endLine) {
                return true;
            }
        }
    }
    return false;
}

void BuildVisualLines(
    keel::StringRef text,
    keel::ArrayRef<LogicalLine> logicalLines,
    bool wordWrap,
    keel::f32 wrapWidth,
    keel::f32 fontSize,
    FontStyle style,
    ICanvas* canvas,
    keel::Array<VisualLine>& outVisual,
    keel::ArrayRef<FoldRegion> foldRegions = {},
    keel::ArrayRef<keel::usize> foldedLines = {},
    keel::u8 tabSize
) {
    outVisual.clear();
    outVisual.reserve(logicalLines.size());

    for (keel::usize i = 0; i < logicalLines.size(); ++i) {
        if (IsLineHiddenByFold(foldRegions, foldedLines, i)) {
            continue;
        }
        const LogicalLine& ll = logicalLines[i];
        const keel::StringRef lineStr = text.substr(ll.startOffset, ll.length);
        if (!wordWrap || wrapWidth <= 24.0f || lineStr.empty()) {
            outVisual.push_back({i, ll.startOffset, 0, ll.length, true});
            continue;
        }

        const keel::f32 fullW = MeasureSegmentWidth(canvas, lineStr, fontSize, style, tabSize);
        if (fullW <= wrapWidth + 0.5f) {
            outVisual.push_back({i, ll.startOffset, 0, ll.length, true});
            continue;
        }

        keel::usize subStart = 0;
        bool first = true;
        while (subStart < ll.length) {
            keel::usize cur = subStart;
            keel::usize lastBreak = subStart;
            while (cur < ll.length) {
                const keel::usize next = TextUtil::NextCharBoundary(lineStr, cur);
                const keel::f32 w = MeasureSegmentWidth(canvas, lineStr.substr(subStart, next - subStart), fontSize, style, tabSize);
                if (w > wrapWidth && cur > subStart) {
                    break;
                }
                cur = next;
                if (cur < ll.length && (lineStr[cur - 1] == ' ' || lineStr[cur - 1] == '\t' || lineStr[cur - 1] == ',' || lineStr[cur - 1] == ';')) {
                    lastBreak = cur;
                }
            }
            const keel::usize takeEnd = (lastBreak > subStart && cur < ll.length) ? lastBreak : cur;
            const keel::usize subLen = keel::max<keel::usize>(1, takeEnd - subStart);
            outVisual.push_back({i, ll.startOffset + subStart, subStart, subLen, first});
            first = false;
            subStart += subLen;
        }
    }
    if (outVisual.empty()) {
        outVisual.push_back({0, 0, 0, 0, true});
    }
}

void FindAllSearchMatches(
    keel::StringRef text,
    keel::ArrayRef<LogicalLine> logicalLines,
    keel::StringRef query,
    bool matchCase,
    bool wholeWord,
    keel::Array<SearchMatch>& outMatches
) {
    outMatches.clear();
    if (query.empty() || text.empty()) return;

    auto isWordCodepoint = [](keel::u32 cp) noexcept -> bool {
        if (cp < 0x80u) {
            return IsIdentChar(keel::as<char>(cp));
        }
        return (cp >= 0x00C0u && cp <= 0x024Fu && cp != 0x00D7u && cp != 0x00F7u) ||
               (cp >= 0x0400u && cp <= 0x04FFu);
    };

    const keel::usize qLen = query.size();
    for (keel::usize lineIdx = 0; lineIdx < logicalLines.size(); ++lineIdx) {
        const LogicalLine& ll = logicalLines[lineIdx];
        if (matchCase && ll.length < qLen) continue;
        const keel::StringRef lineStr = text.substr(ll.startOffset, ll.length);

        keel::usize col = 0;
        while (col < ll.length) {
            keel::usize matchStart = keel::StringRef::npos;
            keel::usize matchLen = 0;
            if (matchCase) {
                matchStart = lineStr.find(query, col);
                matchLen = qLen;
            } else {
                keel::usize relPos = 0;
                if (TextUtil::FindCaseInsensitive(lineStr.substr(col), query, relPos, matchLen)) {
                    matchStart = col + relPos;
                }
            }
            if (matchStart == keel::StringRef::npos || matchLen == 0) {
                break;
            }
            const keel::usize matchEnd = matchStart + matchLen;

            bool matched = true;
            if (wholeWord) {
                if (matchStart > 0) {
                    keel::usize prevOff = keel::utf8_prev_boundary(lineStr, matchStart);
                    if (isWordCodepoint(keel::utf8_decode_next(lineStr, prevOff))) {
                        matched = false;
                    }
                }
                if (matched && matchEnd < ll.length) {
                    keel::usize nextOff = matchEnd;
                    if (isWordCodepoint(keel::utf8_decode_next(lineStr, nextOff))) {
                        matched = false;
                    }
                }
            }

            if (matched) {
                outMatches.push_back({
                    .startOffset = ll.startOffset + matchStart,
                    .endOffset = ll.startOffset + matchEnd,
                    .logicalLine = lineIdx,
                    .startCol = matchStart,
                    .endCol = matchEnd
                });
                col = keel::max(matchEnd, keel::utf8_next_boundary(lineStr, matchStart));
            } else {
                col = keel::utf8_next_boundary(lineStr, matchStart);
            }
        }
    }
}

bool FindMatchingBracketPair(
    keel::StringRef text,
    keel::usize cursorOffset,
    keel::usize& outOpenOffset,
    keel::usize& outCloseOffset
) noexcept {
    if (text.empty()) return false;
    const keel::usize safeCur = keel::min(cursorOffset, text.size());

    auto isBracket = [](char c) noexcept {
        return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}';
    };

    keel::usize brPos = keel::StringRef::npos;
    if (safeCur < text.size() && isBracket(text[safeCur])) {
        brPos = safeCur;
    } else if (safeCur > 0 && isBracket(text[safeCur - 1])) {
        brPos = safeCur - 1;
    }
    if (brPos == keel::StringRef::npos) return false;

    const char c = text[brPos];
    const bool forward = (c == '(' || c == '[' || c == '{');
    const char target = (c == '(') ? ')' : (c == ')') ? '('
                      : (c == '[') ? ']' : (c == ']') ? '['
                      : (c == '{') ? '}' : '{';

    keel::i32 depth = 0;
    if (forward) {
        const keel::usize maxScan = keel::min(text.size(), brPos + 16384);
        for (keel::usize i = brPos; i < maxScan; ++i) {
            if (text[i] == c) ++depth;
            else if (text[i] == target) {
                --depth;
                if (depth == 0) {
                    outOpenOffset = brPos;
                    outCloseOffset = i;
                    return true;
                }
            }
        }
    } else {
        const keel::usize minScan = (brPos > 16384) ? (brPos - 16384) : 0;
        for (keel::usize i = brPos + 1; i > minScan; --i) {
            const keel::usize idx = i - 1;
            if (text[idx] == c) ++depth;
            else if (text[idx] == target) {
                --depth;
                if (depth == 0) {
                    outOpenOffset = idx;
                    outCloseOffset = brPos;
                    return true;
                }
            }
        }
    }
    return false;
}

keel::StringRef ExtractWordAtOffset(keel::StringRef text, keel::usize offset, keel::usize* outStartOffset) noexcept {
    if (text.empty()) return {};
    keel::usize pos = keel::min(offset, text.size());
    if (pos == text.size() || !IsIdentChar(text[pos])) {
        if (pos > 0 && IsIdentChar(text[pos - 1])) {
            --pos;
        } else {
            return {};
        }
    }
    keel::usize s = pos;
    while (s > 0 && IsIdentChar(text[s - 1])) --s;
    if (!IsIdentStart(text[s])) return {};
    keel::usize e = pos + 1;
    while (e < text.size() && IsIdentChar(text[e])) ++e;
    if (outStartOffset) *outStartOffset = s;
    return text.substr(s, e - s);
}

keel::StringRef ExtractCodeWithoutLineComment(
    keel::StringRef lineText,
    keel::StringRef commentPrefix
) noexcept {
    keel::usize start = 0;
    while (start < lineText.size() && (lineText[start] == ' ' || lineText[start] == '\t' || lineText[start] == '\r')) {
        ++start;
    }
    if (start >= lineText.size()) {
        return {};
    }
    const keel::StringRef sub = lineText.substr(start);
    if (sub.starts_with("/*") || sub.starts_with("*/") || sub.starts_with("%{") || sub.starts_with("%}")) {
        return {};
    }

    bool inQuotes = false;
    char quoteChar = '\0';
    keel::usize codeLen = sub.size();
    for (keel::usize i = 0; i < sub.size(); ++i) {
        const char c = sub[i];
        if (inQuotes) {
            if (c == '\\' && i + 1 < sub.size()) {
                ++i;
                continue;
            }
            if (c == quoteChar) {
                inQuotes = false;
            }
            continue;
        }
        if (c == '"' || c == '\'') {
            inQuotes = true;
            quoteChar = c;
            continue;
        }
        if (!commentPrefix.empty() && sub.substr(i).starts_with(commentPrefix)) {
            codeLen = i;
            break;
        }
        if (commentPrefix.empty() && sub.substr(i).starts_with("//")) {
            codeLen = i;
            break;
        }
    }

    keel::StringRef code = sub.substr(0, codeLen);
    while (!code.empty() && (code.back() == ' ' || code.back() == '\t' || code.back() == '\r')) {
        code.remove_suffix(1);
    }
    if (code == "{" || code == "}" || code == "};" || code == "}," || code == ";" || code == ")" || code == "]") {
        return {};
    }
    return code;
}

bool HasCodeBeforeOffsetOnLine(
    keel::StringRef docText,
    const LogicalLine& ll,
    keel::usize cursorOffset
) noexcept {
    if (cursorOffset <= ll.startOffset) {
        return false;
    }
    const keel::usize len = keel::min(cursorOffset - ll.startOffset, ll.length);
    const keel::StringRef prefix = docText.substr(ll.startOffset, len);
    for (char c : prefix) {
        if (c != ' ' && c != '\t' && c != '\r') {
            return true;
        }
    }
    return false;
}

keel::StringRef ExtractLeadingIdentifierOrKey(keel::StringRef code) noexcept {
    keel::usize i = 0;
    while (i < code.size() && (code[i] == ' ' || code[i] == '\t' || code[i] == '(' || code[i] == '[' || code[i] == '*' || code[i] == '&')) {
        ++i;
    }
    if (i >= code.size()) {
        return {};
    }
    if (code[i] == '"') {
        const keel::usize start = i++;
        while (i < code.size() && code[i] != '"') {
            if (code[i] == '\\' && i + 1 < code.size()) ++i;
            ++i;
        }
        if (i < code.size()) ++i;
        return code.substr(start, i - start);
    }
    if (IsIdentStart(code[i])) {
        const keel::usize start = i++;
        while (i < code.size() && IsIdentChar(code[i])) {
            ++i;
        }
        return code.substr(start, i - start);
    }
    return {};
}

keel::StringRef ExtractAssignedTargetIdentifier(keel::StringRef code) noexcept {
    const keel::StringRef lead = ExtractLeadingIdentifierOrKey(code);
    if (lead == "for" || lead == "while" || lead == "if" || lead == "elif" ||
        lead == "else" || lead == "switch" || lead == "return" || lead == "function" ||
        lead == "def" || lead == "class" || lead == "rule" || lead == "when" ||
        lead == "emit" || lead == "alert") {
        return {};
    }

    keel::i32 parenDepth = 0;
    keel::usize eqPos = keel::StringRef::npos;
    for (keel::usize i = 0; i < code.size(); ++i) {
        const char c = code[i];
        if (c == '(' || c == '[') {
            ++parenDepth;
        } else if (c == ')' || c == ']') {
            if (parenDepth > 0) --parenDepth;
        } else if (c == '=' && parenDepth == 0) {
            const char prev = (i > 0) ? code[i - 1] : '\0';
            const char next = (i + 1 < code.size()) ? code[i + 1] : '\0';
            if (prev != '=' && prev != '!' && prev != '<' && prev != '>' && next != '=') {
                eqPos = i;
                break;
            }
        }
    }
    if (eqPos == keel::StringRef::npos || eqPos == 0) {
        return {};
    }

    keel::usize p = eqPos;
    if (p > 0 && (code[p - 1] == '+' || code[p - 1] == '-' || code[p - 1] == '*' ||
                  code[p - 1] == '/' || code[p - 1] == '|' || code[p - 1] == '&' || code[p - 1] == '^')) {
        --p;
    }
    while (p > 0 && (code[p - 1] == ' ' || code[p - 1] == '\t')) {
        --p;
    }
    if (p > 0 && (code[p - 1] == ')' || code[p - 1] == ']')) {
        const char closeBr = code[p - 1];
        const char openBr = (closeBr == ')') ? '(' : '[';
        keel::i32 depth = 0;
        while (p > 0) {
            --p;
            if (code[p] == closeBr) ++depth;
            else if (code[p] == openBr) {
                --depth;
                if (depth == 0) break;
            }
        }
        while (p > 0 && (code[p - 1] == ' ' || code[p - 1] == '\t')) {
            --p;
        }
    }
    const keel::usize endIdent = p;
    while (p > 0 && IsIdentChar(code[p - 1])) {
        --p;
    }
    if (endIdent > p && IsIdentStart(code[p])) {
        return code.substr(p, endIdent - p);
    }
    return {};
}

bool DoesLineMatchBaselineSignature(
    keel::StringRef currentCode,
    keel::StringRef baselineCode
) noexcept {
    if (currentCode.empty()) {
        return false;
    }
    if (baselineCode.empty()) {
        return true;
    }
    if (currentCode == baselineCode) {
        return true;
    }
    const keel::StringRef baseTarget = ExtractAssignedTargetIdentifier(baselineCode);
    if (!baseTarget.empty()) {
        const keel::StringRef curTarget = ExtractAssignedTargetIdentifier(currentCode);
        if (curTarget == baseTarget) {
            return true;
        }
        return false;
    }
    const keel::StringRef baseLead = ExtractLeadingIdentifierOrKey(baselineCode);
    const keel::StringRef curLead = ExtractLeadingIdentifierOrKey(currentCode);
    return !baseLead.empty() && (curLead == baseLead);
}

keel::usize ResolveTrackedCodeLine(
    keel::StringRef docText,
    keel::ArrayRef<LogicalLine> logicalLines,
    keel::usize requestedLine1,
    keel::StringRef explicitAnchor,
    keel::ArrayRef<keel::usize> baselineToCurrentLine,
    keel::ArrayRef<keel::String> baselineSignatures,
    keel::StringRef commentPrefix
) noexcept {
    if (logicalLines.empty() || requestedLine1 == 0) {
        return 0;
    }
    auto getLineCode = [&](keel::usize line1) noexcept -> keel::StringRef {
        if (line1 < 1 || line1 > logicalLines.size()) {
            return {};
        }
        const LogicalLine& ll = logicalLines[line1 - 1];
        return ExtractCodeWithoutLineComment(docText.substr(ll.startOffset, ll.length), commentPrefix);
    };

    const keel::usize mappedLine1 = (requestedLine1 < baselineToCurrentLine.size() && baselineToCurrentLine[requestedLine1] > 0)
        ? baselineToCurrentLine[requestedLine1]
        : requestedLine1;
    const keel::StringRef baseSig = (requestedLine1 < baselineSignatures.size())
        ? baselineSignatures[requestedLine1].as_ref()
        : keel::StringRef{};

    if (!explicitAnchor.empty()) {
        const keel::StringRef mappedCode = getLineCode(mappedLine1);
        if (!mappedCode.empty() && (mappedCode.find(explicitAnchor) != keel::StringRef::npos || DoesLineMatchBaselineSignature(mappedCode, baseSig))) {
            return mappedLine1;
        }
        for (keel::usize delta = 1; delta <= logicalLines.size(); ++delta) {
            if (mappedLine1 + delta <= logicalLines.size()) {
                const keel::StringRef c = getLineCode(mappedLine1 + delta);
                if (!c.empty() && c.find(explicitAnchor) != keel::StringRef::npos) {
                    return mappedLine1 + delta;
                }
            }
            if (mappedLine1 > delta) {
                const keel::StringRef c = getLineCode(mappedLine1 - delta);
                if (!c.empty() && c.find(explicitAnchor) != keel::StringRef::npos) {
                    return mappedLine1 - delta;
                }
            }
        }
        return 0;
    }

    const keel::StringRef mappedCode = getLineCode(mappedLine1);
    if (DoesLineMatchBaselineSignature(mappedCode, baseSig)) {
        return mappedLine1;
    }
    if (!baseSig.empty()) {
        for (keel::usize delta = 1; delta <= 2; ++delta) {
            if (mappedLine1 + delta <= logicalLines.size() && getLineCode(mappedLine1 + delta) == baseSig) {
                return mappedLine1 + delta;
            }
            if (mappedLine1 > delta && getLineCode(mappedLine1 - delta) == baseSig) {
                return mappedLine1 - delta;
            }
        }
    }
    return 0;
}


EditorFontMetrics ResolveEditorFontMetrics(
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 baseFontSize,
    keel::f32 effZoom,
    FontStyle style,
    keel::u8 tabSize
) noexcept {
    EditorFontMetrics m{};
    const keel::f32 clampedZoom = keel::clamp(effZoom, 0.5f, 3.0f);
    m.fontSize = baseFontSize * clampedZoom;

    if (canvas) {
        const Vec2 sample = canvas->MeasureTextStyled("Mg", m.fontSize, style);
        m.glyphBoxH = (sample.y > 0.0f) ? sample.y : theme.GetLineHeight(m.fontSize);
        const keel::f32 spW = canvas->MeasureTextStyled(" ", m.fontSize, style).x;
        m.spaceAdvance = (spW > 0.0f) ? spW : (m.fontSize * 0.60f);
        const keel::f32 digW = canvas->MeasureTextStyled("8", m.fontSize, FontStyle::Monospace).x;
        m.digitAdvance = (digW > 0.0f) ? digW : m.spaceAdvance;
    } else {
        m.glyphBoxH = theme.GetLineHeight(m.fontSize);
        const keel::f32 factor = (style == FontStyle::Monospace) ? 0.60f : 0.56f;
        m.spaceAdvance = m.fontSize * factor;
        m.digitAdvance = m.fontSize * 0.60f;
    }

    m.lineHeight = keel::max(theme.GetLineHeight(m.fontSize), m.glyphBoxH * 1.14f);
    m.textOffsetY = (m.lineHeight - m.glyphBoxH) * 0.5f;
    m.tabAdvance = m.spaceAdvance * keel::as<keel::f32>(keel::clamp<keel::i32>(tabSize, 1, 16));
    return m;
}

Vec2 MeasureEditorBox(
    keel::StringRef text,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 effZoom,
    keel::f32 availableWidth
) noexcept {
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const FontStyle textStyle = opts.monospace ? FontStyle::Monospace : FontStyle::Regular;
    const keel::f32 baseFontSize = opts.monospace
        ? theme.GetTypography(TextRole::CodeMedium).fontSize
        : theme.ResolveFontSize(TextRole::BodyMedium, size);
    const EditorFontMetrics fm = ResolveEditorFontMetrics(canvas, theme, baseFontSize, effZoom, textStyle);
    const keel::f32 fontSize = fm.fontSize;
    const keel::f32 lineHeight = fm.lineHeight;
    const keel::f32 padY = keel::max(8.0f, cm.paddingY);
    const keel::f32 padX = keel::max(10.0f, cm.paddingX);
    const keel::f32 statusH = opts.showStatusBar ? 22.0f : 0.0f;

    keel::usize lineCount = 1;
    keel::usize maxLineChars = 0;
    keel::usize curChars = 0;
    for (char c : text) {
        if (c == '\n') {
            ++lineCount;
            maxLineChars = keel::max(maxLineChars, curChars);
            curChars = 0;
        } else if (c != '\r') {
            ++curChars;
        }
    }
    maxLineChars = keel::max(maxLineChars, curChars);

    keel::f32 boxH = 0.0f;
    if (opts.height.IsFixed()) {
        boxH = opts.height.Dp();
    } else if (opts.autoHeight) {
        boxH = keel::as<keel::f32>(lineCount) * lineHeight + padY * 2.0f + statusH;
    } else if (opts.rows.has_value()) {
        const keel::u32 r = keel::max(1u, *opts.rows);
        boxH = keel::as<keel::f32>(r) * lineHeight + padY * 2.0f + statusH;
    } else {
        boxH = 4.0f * lineHeight + padY * 2.0f + statusH;
    }

    keel::usize digits = 2;
    keel::usize n = lineCount;
    while (n >= 100) {
        ++digits;
        n /= 10;
    }
    const bool hasBpGutter = opts.breakpoints.has_target() || opts.enableBreakpoints ||
        (opts.executionLine > 0) || (opts.executionLinePtr != nullptr && *opts.executionLinePtr > 0);
    const keel::f32 bpWidth = hasBpGutter ? keel::max(28.0f * effZoom, fm.spaceAdvance * 3.4f) : 0.0f;
    const keel::f32 foldWidth = opts.codeFolding ? keel::max(14.0f * effZoom, fm.spaceAdvance * 1.85f) : 0.0f;
    const keel::f32 gutterPadX = fm.spaceAdvance * 1.25f;
    const keel::f32 gutterWidth = opts.showLineNumbers
        ? (bpWidth + keel::as<keel::f32>(digits) * fm.digitAdvance + gutterPadX * 2.0f + foldWidth)
        : (bpWidth + foldWidth);

    keel::f32 boxW = 280.0f;
    if (opts.width.IsFixed()) {
        boxW = opts.width.Dp();
    } else if (opts.width.ResolveMode(SizeMode::Stretch) == SizeMode::Stretch && availableWidth > 0.0f) {
        boxW = availableWidth;
    } else {
        const keel::f32 charAdv = MeasureChunkNoTabs(canvas, "M", fontSize, textStyle);
        const keel::f32 estimatedW = gutterWidth + padX * 2.0f + keel::as<keel::f32>(keel::min<keel::usize>(maxLineChars, 80)) * charAdv + 24.0f;
        boxW = keel::max(boxW, estimatedW);
    }

    return Control::Measure({boxW, boxH}, Control::FromOptions(opts, SizeMode::Stretch, SizeMode::Fit), theme);
}


EditorLayoutRects ComputeEditorLayout(
    Rect bounds,
    keel::StringRef text,
    keel::ArrayRef<LogicalLine> logicalLines,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 effZoom,
    keel::f32 scrollX,
    keel::f32 scrollY,
    bool copyFeedbackActive,
    bool searchOpen,
    bool replaceOpen,
    bool gotoOpen,
    keel::ArrayRef<FoldRegion> foldRegions,
    keel::ArrayRef<keel::usize> foldedLines,
    keel::Array<VisualLine>& outVisualLines,
    keel::f32 debugToolbarRatio
) {
    EditorLayoutRects L{};
    L.toolbarSize = opts.toolbarSize.value_or(size);
    const ComponentMetrics& cm = theme.GetMetrics(size);
    const ComponentMetrics& tbCm = theme.GetMetrics(L.toolbarSize);
    const keel::f32 tbGap = theme.GetGap(L.toolbarSize);
    const ScrollbarMetrics& sm = theme.GetScrollbar(size);
    const FontStyle textStyle = opts.monospace ? FontStyle::Monospace : FontStyle::Regular;

    const keel::f32 baseFontSize = opts.monospace
        ? theme.GetTypography(TextRole::CodeMedium).fontSize
        : theme.ResolveFontSize(TextRole::BodyMedium, size);
    L.metrics = ResolveEditorFontMetrics(canvas, theme, baseFontSize, effZoom, textStyle, opts.tabSize);
    L.fontSize = L.metrics.fontSize;
    L.glyphBoxH = L.metrics.glyphBoxH;
    L.lineHeight = L.metrics.lineHeight;
    L.editorBox = bounds;

    const keel::f32 padY = keel::max(8.0f, cm.paddingY);
    const keel::f32 padX = keel::max(10.0f, cm.paddingX);

    keel::usize digits = 2;
    keel::usize n = keel::max<keel::usize>(1, logicalLines.size());
    while (n >= 100) {
        ++digits;
        n /= 10;
    }

    const bool hasBpGutter = opts.breakpoints.has_target() || opts.enableBreakpoints ||
        (opts.executionLine > 0) || (opts.executionLinePtr != nullptr && *opts.executionLinePtr > 0);
    L.bpColWidth = hasBpGutter ? keel::max(28.0f * effZoom, L.metrics.spaceAdvance * 3.4f) : 0.0f;
    L.foldColWidth = opts.codeFolding ? keel::max(14.0f * effZoom, L.metrics.spaceAdvance * 1.85f) : 0.0f;
    const keel::f32 gutterPadX = L.metrics.spaceAdvance * 1.25f;
    L.gutterWidth = opts.showLineNumbers
        ? (L.bpColWidth + keel::as<keel::f32>(digits) * L.metrics.digitAdvance + gutterPadX * 2.0f + L.foldColWidth)
        : (L.bpColWidth + L.foldColWidth);

    // Unzoomed (100%) gutter width for zoom-invariant floating toolbar track bounds
    const EditorFontMetrics baseMetrics = ResolveEditorFontMetrics(canvas, theme, baseFontSize, 1.0f, textStyle, opts.tabSize);
    const keel::f32 baseBpColW = hasBpGutter ? keel::max(28.0f, baseMetrics.spaceAdvance * 3.4f) : 0.0f;
    const keel::f32 baseFoldColW = opts.codeFolding ? keel::max(14.0f, baseMetrics.spaceAdvance * 1.85f) : 0.0f;
    const keel::f32 baseGutterPadX = baseMetrics.spaceAdvance * 1.25f;
    const keel::f32 baseGutterW = opts.showLineNumbers
        ? (baseBpColW + keel::as<keel::f32>(digits) * baseMetrics.digitAdvance + baseGutterPadX * 2.0f + baseFoldColW)
        : (baseBpColW + baseFoldColW);

    const Rect fullInner(
        L.editorBox.x + 1.0f,
        L.editorBox.y + 1.0f,
        keel::max(0.0f, L.editorBox.w - 2.0f),
        keel::max(0.0f, L.editorBox.h - 2.0f)
    );

    L.statusBarHeight = (opts.showStatusBar && fullInner.h > 60.0f) ? 22.0f : 0.0f;
    L.innerBounds = Rect(
        fullInner.x,
        fullInner.y,
        fullInner.w,
        keel::max(0.0f, fullInner.h - L.statusBarHeight)
    );
    if (L.statusBarHeight > 0.0f) {
        L.statusBarRect = Rect(
            fullInner.x,
            fullInner.Bottom() - L.statusBarHeight,
            fullInner.w,
            L.statusBarHeight
        );
    }

    if (L.gutterWidth > 0.0f) {
        L.gutterRect = Rect(
            L.innerBounds.x,
            L.innerBounds.y,
            keel::min(L.gutterWidth, L.innerBounds.w),
            L.innerBounds.h
        );
    }

    L.hasMinimap = opts.showMinimap && (L.innerBounds.w > 280.0f) && (L.innerBounds.h > 60.0f);
    L.minimapWidth = L.hasMinimap ? 64.0f : 0.0f;

    keel::f32 prelimTextW = keel::max(0.0f, L.innerBounds.w - L.gutterWidth - L.minimapWidth - padX * 2.0f);
    BuildVisualLines(text, logicalLines, opts.wordWrap, prelimTextW, L.fontSize, textStyle, canvas, outVisualLines, foldRegions, foldedLines, opts.tabSize);

    L.totalContentH = keel::as<keel::f32>(outVisualLines.size()) * L.lineHeight;
    const keel::f32 availTextH = keel::max(0.0f, L.innerBounds.h - padY * 2.0f);
    L.needVScroll = !opts.autoHeight && (L.totalContentH > availTextH + 1.0f) && sm.trackWidth > 0.0f;

    const keel::f32 vBarReserve = L.needVScroll ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;
    if (L.needVScroll && opts.wordWrap) {
        prelimTextW = keel::max(0.0f, L.innerBounds.w - L.gutterWidth - L.minimapWidth - padX * 2.0f - vBarReserve);
        BuildVisualLines(text, logicalLines, opts.wordWrap, prelimTextW, L.fontSize, textStyle, canvas, outVisualLines, foldRegions, foldedLines, opts.tabSize);
        L.totalContentH = keel::as<keel::f32>(outVisualLines.size()) * L.lineHeight;
    }

    L.maxLineW = 0.0f;
    if (!opts.wordWrap) {
        for (keel::usize li = 0; li < logicalLines.size(); ++li) {
            const LogicalLine& ll = logicalLines[li];
            if (ll.length == 0) continue;
            const keel::StringRef ls = text.substr(ll.startOffset, ll.length);
            keel::f32 lineW = MeasureSegmentWidth(canvas, ls, L.fontSize, textStyle, opts.tabSize);
            if (!opts.inlineValues.empty()) {
                for (const CodeInlineValue& iv : opts.inlineValues) {
                    if (iv.line == li + 1 && !iv.text.empty()) {
                        const keel::f32 ivFs = L.fontSize * 0.88f;
                        lineW += L.metrics.spaceAdvance * 2.2f + MeasureChunkNoTabs(canvas, iv.text, ivFs, FontStyle::Monospace) + 12.0f;
                    }
                }
            }
            L.maxLineW = keel::max(L.maxLineW, lineW);
        }
    }

    const keel::f32 availTextW = keel::max(0.0f, L.innerBounds.w - L.gutterWidth - L.minimapWidth - padX * 2.0f - vBarReserve);
    L.needHScroll = !opts.wordWrap && (L.maxLineW > availTextW + 2.0f) && sm.trackWidth > 0.0f;
    const keel::f32 hBarReserve = L.needHScroll ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;

    const keel::f32 finalViewportH = keel::max(0.0f, L.innerBounds.h - padY * 2.0f - hBarReserve);
    L.needVScroll = !opts.autoHeight && (L.totalContentH > finalViewportH + 1.0f) && sm.trackWidth > 0.0f;
    const keel::f32 finalVBarReserve = L.needVScroll ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;

    if (L.hasMinimap) {
        L.minimapRect = Rect(
            L.innerBounds.Right() - finalVBarReserve - L.minimapWidth,
            L.innerBounds.y,
            L.minimapWidth,
            L.innerBounds.h - hBarReserve
        );
    }

    L.textViewport = Rect(
        L.innerBounds.x + L.gutterWidth + padX,
        L.innerBounds.y + padY,
        keel::max(0.0f, L.innerBounds.w - L.gutterWidth - L.minimapWidth - padX * 2.0f - finalVBarReserve),
        finalViewportH
    );

    L.maxScrollY = keel::max(0.0f, L.totalContentH - L.textViewport.h);
    L.maxScrollX = opts.wordWrap ? 0.0f : keel::max(0.0f, L.maxLineW + 24.0f - L.textViewport.w);

    const keel::f32 clampedScrollY = keel::clamp(scrollY, 0.0f, L.maxScrollY);
    const keel::f32 clampedScrollX = keel::clamp(scrollX, 0.0f, L.maxScrollX);

    const ScrollbarStyleTokens& sbStyle = theme.scrollbarStyle;
    const keel::f32 sbBtnLen = (sbStyle.buttonSize > 0.0f) ? sbStyle.buttonSize : sm.trackWidth;

    if (L.needVScroll) {
        const keel::f32 barX = keel::floor(L.innerBounds.Right() - sm.trackWidth - sm.margin);
        const keel::f32 barY = keel::floor(L.innerBounds.y + sm.margin);
        const keel::f32 barH = keel::max(0.0f, L.innerBounds.h - sm.margin * 2.0f - hBarReserve);
        L.vHasButtons = sbStyle.showButtons && (barH >= sbBtnLen * 2.0f + 12.0f);
        if (L.vHasButtons) {
            L.vStartBtnRect = Rect(barX, barY, sm.trackWidth, sbBtnLen);
            L.vEndBtnRect   = Rect(barX, barY + barH - sbBtnLen, sm.trackWidth, sbBtnLen);
        }
        const keel::f32 trackY = barY + (L.vHasButtons ? sbBtnLen : 0.0f);
        const keel::f32 trackH = keel::max(0.0f, barH - (L.vHasButtons ? sbBtnLen * 2.0f : 0.0f));
        L.vTrackRect = Rect(barX, trackY, sm.trackWidth, trackH);
        if (trackH > 0.0f && L.totalContentH > 0.0f) {
            const keel::f32 visibleRatio = keel::clamp(L.textViewport.h / L.totalContentH, 0.05f, 1.0f);
            const keel::f32 thumbH = keel::min(trackH, keel::max(sm.minThumbLength, trackH * visibleRatio));
            const keel::f32 maxTravel = keel::max(0.0f, trackH - thumbH);
            const keel::f32 thumbY = trackY + ((L.maxScrollY > 0.0f) ? (clampedScrollY / L.maxScrollY) * maxTravel : 0.0f);
            L.vThumbRect = Rect(barX, thumbY, sm.trackWidth, thumbH);
        }
    }

    if (L.needHScroll) {
        const keel::f32 barX = keel::floor(L.innerBounds.x + L.gutterWidth + sm.margin);
        const keel::f32 barY = keel::floor(L.innerBounds.Bottom() - sm.trackWidth - sm.margin);
        const keel::f32 barW = keel::max(0.0f, L.innerBounds.w - L.gutterWidth - L.minimapWidth - sm.margin * 2.0f - (L.needVScroll ? (sm.trackWidth + sm.margin) : 0.0f));
        L.hHasButtons = sbStyle.showButtons && (barW >= sbBtnLen * 2.0f + 12.0f);
        if (L.hHasButtons) {
            L.hStartBtnRect = Rect(barX, barY, sbBtnLen, sm.trackWidth);
            L.hEndBtnRect   = Rect(barX + barW - sbBtnLen, barY, sbBtnLen, sm.trackWidth);
        }
        const keel::f32 trackX = barX + (L.hHasButtons ? sbBtnLen : 0.0f);
        const keel::f32 trackW = keel::max(0.0f, barW - (L.hHasButtons ? sbBtnLen * 2.0f : 0.0f));
        L.hTrackRect = Rect(trackX, barY, trackW, sm.trackWidth);
        const keel::f32 totalW = L.maxLineW + 24.0f;
        if (trackW > 0.0f && totalW > 0.0f) {
            const keel::f32 visibleRatio = keel::clamp(L.textViewport.w / totalW, 0.05f, 1.0f);
            const keel::f32 thumbW = keel::min(trackW, keel::max(sm.minThumbLength, trackW * visibleRatio));
            const keel::f32 maxTravel = keel::max(0.0f, trackW - thumbW);
            const keel::f32 thumbX = trackX + ((L.maxScrollX > 0.0f) ? (clampedScrollX / L.maxScrollX) * maxTravel : 0.0f);
            L.hThumbRect = Rect(thumbX, barY, thumbW, sm.trackWidth);
        }
    }

    // Top-right quick buttons (Copy, Search, Zoom %) when Search overlay is closed â€” sized via UiSize & Button::Measure
    const keel::f32 overlayMargin = keel::max(4.0f, tbGap * 0.75f);
    const keel::f32 overlayRight = L.innerBounds.Right() - finalVBarReserve - L.minimapWidth - overlayMargin;
    const keel::f32 overlayTop = L.innerBounds.y + overlayMargin;
    keel::f32 rightCursor = overlayRight;

    keel::f32 stableCopyReserve = 0.0f;
    if (opts.showCopyButton && L.innerBounds.w > tbCm.defaultWidth * 1.5f) {
        ButtonOptions stableCopyOpts{};
        stableCopyOpts.text = "Copy";
        stableCopyOpts.icon = Icon::Copy();
        stableCopyReserve = detail::Button::Measure(stableCopyOpts, L.toolbarSize, canvas, theme).x + tbGap * 0.75f;
    }

    if (!searchOpen && !gotoOpen) {
        if (opts.showCopyButton && L.innerBounds.w > tbCm.defaultWidth * 1.5f) {
            ButtonOptions copyOpts{};
            copyOpts.text = copyFeedbackActive ? "Copied" : "Copy";
            copyOpts.icon = copyFeedbackActive ? Icon::Check() : Icon::Copy();
            const Vec2 copySz = detail::Button::Measure(copyOpts, L.toolbarSize, canvas, theme);
            L.copyBtnRect = Rect(rightCursor - copySz.x, overlayTop, copySz.x, copySz.y);
            rightCursor -= (copySz.x + tbGap * 0.75f);
        }

        if (opts.allowZoom && keel::abs(effZoom - 1.0f) > 0.01f && L.innerBounds.w > tbCm.defaultWidth * 1.75f) {
            char zBuf[16]{};
            ButtonOptions zoomOpts{};
            zoomOpts.text = keel::format_buf(zBuf, "{}%", keel::round_i32(effZoom * 100.0f));
            const Vec2 zSz = detail::Button::Measure(zoomOpts, L.toolbarSize, canvas, theme);
            L.zoomBtnRect = Rect(rightCursor - zSz.x, overlayTop, zSz.x, zSz.y);
            rightCursor -= (zSz.x + tbGap * 0.75f);
        }
    }

    // Search & Replace / GoToLine floating overlay geometry â€” sized via UiSize & Button::Measure
    if ((searchOpen || gotoOpen) && L.innerBounds.w > tbCm.defaultHeight * 6.0f) {
        ButtonOptions sqOpts{};
        sqOpts.icon = Icon::Close();
        sqOpts.square = true;
        const Vec2 sqSz = detail::Button::Measure(sqOpts, L.toolbarSize, canvas, theme);
        const keel::f32 rowH = sqSz.y;
        const keel::f32 sqBtn = sqSz.x;
        const keel::f32 pad = keel::max(4.0f, tbGap * 0.65f);
        const keel::f32 gap = keel::max(3.0f, tbGap * 0.50f);
        const bool showRep = searchOpen && replaceOpen && !opts.readOnly;
        const keel::f32 countW = tbCm.defaultHeight * 2.0f;
        const keel::f32 maxPanelW = tbCm.defaultHeight * 13.5f;
        const keel::f32 panelW = keel::min(maxPanelW, keel::max(tbCm.defaultHeight * 6.5f, L.textViewport.w - tbGap));
        const keel::f32 panelH = showRep ? (rowH * 2.0f + pad * 2.0f + gap) : (rowH + pad * 2.0f);
        const keel::f32 panelX = overlayRight - panelW;
        const keel::f32 panelY = overlayTop;

        SearchOverlayRects& S = L.searchOverlay;
        S.panelRect = Rect(panelX, panelY, panelW, panelH);

        const keel::f32 y1 = panelY + pad;
        keel::f32 x = panelX + pad;

        if (searchOpen && !opts.readOnly) {
            S.toggleReplaceBtn = Rect(x, y1, sqBtn, rowH);
            x += sqBtn + gap;
        }

        const keel::f32 rightControlsW = searchOpen
            ? (sqBtn * 3.0f + countW + gap * 4.0f)
            : (sqBtn + gap);
        const keel::f32 findTotalW = keel::max(tbCm.defaultHeight * 2.0f, (panelX + panelW - pad) - x - rightControlsW);
        S.findInputRect = Rect(x, y1, findTotalW, rowH);

        if (searchOpen) {
            const keel::f32 inset = keel::max(2.0f, pad * 0.45f);
            const keel::f32 toggleSz = keel::max(16.0f, rowH - inset * 2.0f);
            S.wordBtnRect = Rect(S.findInputRect.Right() - toggleSz - inset, y1 + inset, toggleSz, toggleSz);
            S.caseBtnRect = Rect(S.wordBtnRect.x - toggleSz - inset * 0.75f, y1 + inset, toggleSz, toggleSz);

            keel::f32 rx = S.findInputRect.Right() + gap;
            S.countRect = Rect(rx, y1, countW, rowH);
            rx += countW + gap;
            S.prevBtnRect = Rect(rx, y1, sqBtn, rowH);
            rx += sqBtn + gap;
            S.nextBtnRect = Rect(rx, y1, sqBtn, rowH);
            rx += sqBtn + gap;
            S.closeBtnRect = Rect(rx, y1, sqBtn, rowH);
        } else {
            // Go to Line mode
            S.closeBtnRect = Rect(S.findInputRect.Right() + gap, y1, sqBtn, rowH);
        }

        if (showRep) {
            const keel::f32 y2 = y1 + rowH + gap;
            const keel::f32 repX = S.findInputRect.x;
            ButtonOptions rep1Opts{};
            rep1Opts.text = "1";
            rep1Opts.icon = Icon::Replace();
            const keel::f32 w1 = detail::Button::Measure(rep1Opts, L.toolbarSize, canvas, theme).x;

            ButtonOptions repAllOpts{};
            repAllOpts.text = "All";
            repAllOpts.icon = Icon::ReplaceAll();
            const keel::f32 w2 = detail::Button::Measure(repAllOpts, L.toolbarSize, canvas, theme).x;

            S.replaceInputRect = Rect(repX, y2, S.findInputRect.w, rowH);
            keel::f32 rx = S.replaceInputRect.Right() + gap;
            S.replaceOneBtnRect = Rect(rx, y2, w1, rowH);
            rx += w1 + gap;
            S.replaceAllBtnRect = Rect(rx, y2, w2, rowH);
        }
    }

    // Floating Debug Toolbar at top of editor â€” zoom-invariant horizontal track with Left/Center/Right & grip drag
    if (opts.showDebugToolbar && L.innerBounds.w > tbCm.defaultHeight * 6.5f) {
        ButtonOptions dbgBtnOpts{};
        dbgBtnOpts.icon = Icon::DebugContinue();
        dbgBtnOpts.square = true;
        dbgBtnOpts.style = ButtonStyle::Ghost;
        const Vec2 btnSz = detail::Button::Measure(dbgBtnOpts, L.toolbarSize, canvas, theme);
        const keel::f32 btnW = btnSz.x;
        const keel::f32 btnH2 = btnSz.y;
        const keel::f32 gap = keel::max(2.0f, tbGap * 0.50f);
        const keel::f32 pad = keel::max(4.0f, tbGap * 0.65f);
        const keel::f32 gripW = tbCm.iconSize * 1.15f;
        const keel::f32 sepExtra = tbGap; // Group separator slot before Restart & Stop
        const keel::f32 barW = gripW + 6.0f * btnW + 5.0f * gap + sepExtra + pad * 2.0f;

        // Zoom-invariant horizontal track bounds [minBarX, maxBarX]
        const keel::f32 fixedVBarReserve = (!opts.autoHeight && sm.trackWidth > 0.0f) ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;
        const keel::f32 fixedRightLimit = L.innerBounds.Right() - fixedVBarReserve - L.minimapWidth - overlayMargin - stableCopyReserve;
        const keel::f32 minBarX = L.innerBounds.x + baseGutterW + padX;
        const keel::f32 maxBarX = keel::max(minBarX, fixedRightLimit - barW);
        L.debugToolbar.minBarX = minBarX;
        L.debugToolbar.maxBarX = maxBarX;

        keel::f32 barX = minBarX;
        if (debugToolbarRatio >= 0.0f) {
            barX = minBarX + keel::clamp(debugToolbarRatio, 0.0f, 1.0f) * keel::max(0.0f, maxBarX - minBarX);
        } else if (opts.debugToolbarAlign == Align::Start) {
            barX = minBarX;
        } else if (opts.debugToolbarAlign == Align::End) {
            barX = maxBarX;
        } else {
            // Align::Center â€” centered within zoom-invariant track [minBarX, maxBarX]
            barX = keel::clamp(L.innerBounds.x + (L.innerBounds.w - barW) * 0.5f, minBarX, maxBarX);
        }

        if (L.searchOverlay.panelRect.w > 0.0f && barX + barW > L.searchOverlay.panelRect.x - tbGap) {
            barX = keel::max(minBarX, L.searchOverlay.panelRect.x - barW - tbGap);
        }
        L.debugToolbar.panelRect = Rect(barX, overlayTop, barW, btnH2 + pad * 2.0f);
        L.debugToolbar.gripRect = Rect(barX, overlayTop, pad + gripW, btnH2 + pad * 2.0f);

        // If the zoom % badge is visible and overlaps the debug toolbar, shift the zoom badge to the left of the debug toolbar
        if (L.zoomBtnRect.w > 0.0f) {
            const keel::f32 overlapPad = tbGap * 0.75f;
            if (L.debugToolbar.panelRect.Right() + overlapPad > L.zoomBtnRect.x &&
                L.debugToolbar.panelRect.x < L.zoomBtnRect.Right() + overlapPad) {
                L.zoomBtnRect.x = L.debugToolbar.panelRect.x - overlapPad - L.zoomBtnRect.w;
            }
        }

        keel::f32 bx = barX + pad + gripW;
        for (keel::usize i = 0; i < 6; ++i) {
            if (i == 4) {
                bx += sepExtra; // Group separator before Restart & Stop
            }
            L.debugToolbar.btnRects[i] = Rect(bx, overlayTop + pad, btnW, btnH2);
            bx += btnW + gap;
        }
    }

    return L;
}

keel::usize FindVisualLineForOffset(keel::ArrayRef<VisualLine> visualLines, keel::usize offset) noexcept {
    if (visualLines.empty()) return 0;
    for (keel::usize i = 0; i < visualLines.size(); ++i) {
        const VisualLine& vl = visualLines[i];
        const keel::usize vlEnd = vl.startOffset + vl.length;
        if (offset >= vl.startOffset && offset <= vlEnd) {
            if (offset == vlEnd && i + 1 < visualLines.size() && visualLines[i + 1].logicalLine == vl.logicalLine) {
                continue;
            }
            return i;
        }
    }
    return visualLines.size() - 1;
}

keel::usize FindLogicalLineForOffset(keel::ArrayRef<LogicalLine> logicalLines, keel::usize offset) noexcept {
    if (logicalLines.empty()) return 0;
    for (keel::usize i = 0; i < logicalLines.size(); ++i) {
        const LogicalLine& ll = logicalLines[i];
        if (offset >= ll.startOffset && offset <= ll.startOffset + ll.length) {
            return i;
        }
    }
    return logicalLines.size() - 1;
}


} // namespace detail::ta_impl

using namespace detail::ta_impl;

TextAreaResponse detail::TextArea::Run(
    Context& ctx,
    keel::Optional<keel::StringSink> stringValue,
    keel::StringRef displayFallback,
    const TextAreaOptions& opts,
    keel::SourceLocation loc
) {

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool wasFocused = ctx.IsFocused(id) && !effectiveDisabled;

    ctx.RegisterFocusable(id, effectiveDisabled);

    TextAreaResponse res{};
    static_cast<Response&>(res) = ctx.Interact(id);
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
    if (optsCopy.width.IsAuto()) {
        optsCopy.width = Control::ResolveWidthMode(ctx, opts.width, SizeMode::Stretch);
    }
    optsCopy.placeholder = ctx.AllocText(opts.placeholder);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    if (optsCopy.syntax.has_value()) {
        SyntaxConfig& syn = *optsCopy.syntax;
        syn.languageName = ctx.AllocString(syn.languageName);
        syn.lineComment = ctx.AllocString(syn.lineComment);
        syn.sectionPrefix = ctx.AllocString(syn.sectionPrefix);
        syn.blockCommentStart = ctx.AllocString(syn.blockCommentStart);
        syn.blockCommentEnd = ctx.AllocString(syn.blockCommentEnd);
        auto copyStrArray = [&](keel::ArrayRef<keel::StringRef> src) -> keel::ArrayRef<keel::StringRef> {
            if (src.empty()) return {};
            keel::MutArrayRef<keel::StringRef> dst = ctx.GetArena().create_slice<keel::StringRef>(src.size());
            for (keel::usize i = 0; i < src.size(); ++i) {
                dst[i] = ctx.AllocString(src[i]);
            }
            return dst;
        };
        syn.keywords = copyStrArray(syn.keywords);
        syn.types = copyStrArray(syn.types);
        syn.builtins = copyStrArray(syn.builtins);
        syn.foldOpenKeywords = copyStrArray(syn.foldOpenKeywords);
        syn.foldCloseKeywords = copyStrArray(syn.foldCloseKeywords);
    }
    if (!opts.debugVariables.empty()) {
        keel::MutArrayRef<CodeDebugVar> vars = ctx.GetArena().create_slice<CodeDebugVar>(opts.debugVariables.size());
        for (keel::usize i = 0; i < opts.debugVariables.size(); ++i) {
            vars[i].name = ctx.AllocString(opts.debugVariables[i].name);
            vars[i].value = ctx.AllocString(opts.debugVariables[i].value);
            vars[i].type = ctx.AllocString(opts.debugVariables[i].type);
        }
        optsCopy.debugVariables = vars;
    }

    const keel::StringRef callerText = stringValue.has_value() ? stringValue->as_ref() : displayFallback;
    keel::StringRef valStr = callerText;
    const keel::StringRef lineCmtPrefix = (opts.syntax.has_value() && !opts.syntax->lineComment.empty())
        ? opts.syntax->lineComment
        : "//";

    auto& state = ctx.GetOrCreatePersistentState<TextAreaState>(id);
    if (!isFocused) {
        state.overlayFocus = 0;
        ctx.SetSuspendDirectEditInput(false);
    }

    // Initialize or reset baseline line-tracking table when document is first loaded or externally replaced
    if (!state.baselineInitialized || callerText != state.prevDocumentText) {
        if (state.baselineInitialized && ctx.HasActiveTextEdit(id)) {
            ctx.SetTextEditBuffer(id, callerText);
        }
        keel::Array<LogicalLine> initLines;
        SplitLogicalLines(callerText, initLines);
        state.baselineToCurrentLine.assign(initLines.size() + 1, 0);
        state.baselineSignatures.clear();
        state.baselineSignatures.resize(initLines.size() + 1);
        for (keel::usize i = 0; i < initLines.size(); ++i) {
            state.baselineToCurrentLine[i + 1] = i + 1;
            const keel::StringRef rawLine = callerText.substr(initLines[i].startOffset, initLines[i].length);
            state.baselineSignatures[i + 1].assign(ExtractCodeWithoutLineComment(rawLine, lineCmtPrefix));
        }
        state.prevDocumentText.assign(callerText);
        state.prevCursor = keel::min(state.cursor, callerText.size());
        state.prevTextSize = callerText.size();
        state.prevLineCount = initLines.size();
        state.baselineInitialized = true;
    }

    const keel::f32 reqZoom = keel::clamp(opts.zoom, 0.5f, 3.0f);
    if (keel::abs(state.lastOptsZoom - reqZoom) > 0.001f) {
        state.zoom = reqZoom;
        state.lastOptsZoom = reqZoom;
    }
    state.zoom = keel::clamp(state.zoom, 0.5f, 3.0f);

    if (opts.debugToolbarAlign != state.lastDebugToolbarAlign) {
        state.lastDebugToolbarAlign = opts.debugToolbarAlign;
        state.debugToolbarRatio = -1.0f;
    }

    if (opts.allowZoom && !effectiveDisabled && (isFocused || res.hovered)) {
        if (ctx.ConsumeAction(KeyAction::ZoomIn)) {
            state.zoom = keel::clamp(state.zoom + 0.10f, 0.5f, 3.0f);
        } else if (ctx.ConsumeAction(KeyAction::ZoomOut)) {
            state.zoom = keel::clamp(state.zoom - 0.10f, 0.5f, 3.0f);
        } else if (ctx.ConsumeAction(KeyAction::ZoomReset)) {
            state.zoom = 1.0f;
        }
    }

    auto applyZoomAnchor = [&](keel::f32 newZoom) {
        const keel::f32 clampedNew = keel::clamp(newZoom, 0.5f, 3.0f);
        if (state.prevAppliedZoom > 0.001f && keel::abs(clampedNew - state.prevAppliedZoom) > 0.0001f) {
            const keel::f32 ratio = clampedNew / state.prevAppliedZoom;
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
        state.draggingMinimap = false;
        state.draggingDebugToolbar = false;
    }

    const keel::f64 now = ctx.GetCurrentTime();
    const bool copyFeedbackActive = (now < state.copyFeedbackUntil);
    if (copyFeedbackActive) {
        ctx.RequestRepaint(2);
    }

    const UiTheme& theme = ctx.GetTheme();
    ICanvas* canvas = ctx.GetCanvas();
    const FontStyle textStyle = optsCopy.monospace ? FontStyle::Monospace : FontStyle::Regular;

    const Rect refBounds = (res.rect.w > 0.0f && res.rect.h > 0.0f)
        ? res.rect
        : Rect({0.0f, 0.0f}, detail::TextArea::Measure(valStr, optsCopy, activeSize, canvas, theme));

    // Synchronize active edit buffer for both editable and readOnly
    if (isFocused || ctx.HasActiveTextEdit(id)) {
        valStr = ctx.EnsureTextEdit(id, valStr, /*multiline=*/true, opts.readOnly);
        ctx.SetTextEditAutoClose(id, opts.autoCloseBrackets);
        ctx.SetTextEditIndentation(id, opts.tabSize, opts.insertSpaces);
    }

    keel::Array<LogicalLine>& logicalLines = state.cachedLogicalLines;
    keel::Array<FoldRegion>& foldRegions = state.cachedFoldRegions;
    keel::Array<VisualLine>& visualLines = state.scratchVisualLines;

    const keel::u64 docHash = detail::HashArgs(valStr);
    const keel::u64 foldSyntaxHash = opts.syntax.has_value()
        ? detail::HashArgs(
            opts.syntax->lineComment,
            opts.syntax->foldOpenKeywords.size(),
            opts.syntax->foldCloseKeywords.size(),
            opts.syntax->transposeApostrophe
        )
        : 0u;
    const bool docChangedForCache =
        (logicalLines.empty() || state.cachedDocSize != valStr.size() || state.cachedDocHash != docHash);
    if (docChangedForCache) {
        SplitLogicalLines(valStr, logicalLines);
        state.cachedDocHash = docHash;
        state.cachedDocSize = valStr.size();
    }

    auto resolveDebugAndDiagnosticMarkers = [&](keel::StringRef curDoc) {
        if (opts.executionLinePtr != nullptr) {
            optsCopy.executionLine = *opts.executionLinePtr;
        } else if (opts.executionLine > 0 && opts.executionLine < state.baselineToCurrentLine.size() && state.baselineToCurrentLine[opts.executionLine] > 0) {
            optsCopy.executionLine = state.baselineToCurrentLine[opts.executionLine];
        } else {
            optsCopy.executionLine = opts.executionLine;
        }
        if (optsCopy.executionLine > logicalLines.size()) {
            optsCopy.executionLine = logicalLines.empty() ? 0 : logicalLines.size();
        }

        state.resolvedInlineValues.clear();
        state.resolvedInlineValues.reserve(opts.inlineValues.size());
        for (const CodeInlineValue& iv : opts.inlineValues) {
            if (iv.text.empty()) continue;
            const keel::usize resolvedLine = ResolveTrackedCodeLine(
                curDoc, logicalLines, iv.line, iv.anchor,
                state.baselineToCurrentLine, state.baselineSignatures, lineCmtPrefix
            );
            if (resolvedLine >= 1 && resolvedLine <= logicalLines.size()) {
                state.resolvedInlineValues.push_back(CodeInlineValue{
                    .line = resolvedLine,
                    .text = ctx.AllocString(iv.text),
                    .variant = iv.variant,
                    .anchor = ctx.AllocString(iv.anchor)
                });
            }
        }
        optsCopy.inlineValues = state.resolvedInlineValues;

        state.resolvedDiagnostics.clear();
        state.resolvedDiagnostics.reserve(opts.diagnostics.size());
        for (const CodeDiagnostic& diag : opts.diagnostics) {
            const keel::usize resolvedLine = ResolveTrackedCodeLine(
                curDoc, logicalLines, diag.line, diag.anchor,
                state.baselineToCurrentLine, state.baselineSignatures, lineCmtPrefix
            );
            if (resolvedLine >= 1 && resolvedLine <= logicalLines.size()) {
                CodeDiagnostic d = diag;
                d.line = resolvedLine;
                d.message = ctx.AllocString(diag.message);
                d.code = ctx.AllocString(diag.code);
                d.anchor = ctx.AllocString(diag.anchor);
                if (!diag.anchor.empty()) {
                    const LogicalLine& ll = logicalLines[resolvedLine - 1];
                    const keel::StringRef fullLine = curDoc.substr(ll.startOffset, ll.length);
                    if (const keel::usize anchorPos = fullLine.find(diag.anchor); anchorPos != keel::StringRef::npos) {
                        d.startCol = keel::as<keel::u32>(anchorPos);
                        d.endCol = keel::as<keel::u32>(anchorPos + diag.anchor.size());
                    }
                }
                state.resolvedDiagnostics.push_back(d);
            }
        }
        optsCopy.diagnostics = state.resolvedDiagnostics;
    };

    resolveDebugAndDiagnosticMarkers(valStr);

    if (opts.codeFolding) {
        if (docChangedForCache || !state.cachedFoldEnabled || state.cachedFoldSyntaxHash != foldSyntaxHash) {
            ComputeFoldRegions(valStr, logicalLines, opts.syntax.has_value() ? &*opts.syntax : nullptr, foldRegions);
            state.cachedFoldEnabled = true;
            state.cachedFoldSyntaxHash = foldSyntaxHash;
        }
        // Prune any stale foldedLines that no longer start a valid fold region
        keel::usize writeIdx = 0;
        for (keel::usize i = 0; i < state.foldedLines.size(); ++i) {
            const keel::usize fl = state.foldedLines[i];
            if (FindFoldRegionStartingAt(foldRegions, fl) != nullptr) {
                state.foldedLines[writeIdx++] = fl;
            }
        }
        state.foldedLines.resize(writeIdx);
    } else {
        foldRegions.clear();
        state.cachedFoldEnabled = false;
        state.foldedLines.clear();
    }

    IndexListSink activeBreakpoints = opts.breakpoints.has_target()
        ? opts.breakpoints
        : IndexListSink(state.internalBreakpoints);

    auto toggleBreakpointAtLine = [&](keel::usize line1Based) {
        if (line1Based == 0 || line1Based > logicalLines.size()) return;
        const keel::ArrayRef<keel::usize> bpView = activeBreakpoints.as_ref();
        keel::usize foundIdx = bpView.size();
        for (keel::usize i = 0; i < bpView.size(); ++i) {
            if (bpView[i] == line1Based) {
                foundIdx = i;
                break;
            }
        }
        if (foundIdx < bpView.size()) {
            activeBreakpoints.erase_at(foundIdx);
        } else {
            activeBreakpoints.push_back(line1Based);
            SortSizeT(activeBreakpoints.as_mut_ref());
        }
        res.debugAction = CodeDebugAction::ToggleBreakpoint;
        res.toggledBreakpointLine = line1Based;
    };

    auto ensureLineUnfolded = [&](keel::usize line0Based) {
        if (state.foldedLines.empty()) return;
        keel::usize writeIdx = 0;
        for (keel::usize i = 0; i < state.foldedLines.size(); ++i) {
            const keel::usize fl = state.foldedLines[i];
            bool shouldUnfold = false;
            if (const FoldRegion* r = FindFoldRegionStartingAt(foldRegions, fl)) {
                shouldUnfold = (line0Based > r->startLine && line0Based <= r->endLine);
            }
            if (!shouldUnfold) {
                state.foldedLines[writeIdx++] = fl;
            }
        }
        state.foldedLines.resize(writeIdx);
    };

    // Unfold if debugger executionLine stepped into a folded block
    if (optsCopy.executionLine > 0 && optsCopy.executionLine != state.prevExecutionLine) {
        ensureLineUnfolded(optsCopy.executionLine - 1);
    }

    EditorLayoutRects L = ComputeEditorLayout(
        refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
        state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
        state.searchOpen, state.replaceOpen, state.gotoOpen,
        foldRegions, state.foldedLines, visualLines,
        state.debugToolbarRatio
    );

    auto scrollLogicalLineIntoView = [&](keel::usize line0Based) {
        ensureLineUnfolded(line0Based);
        BuildVisualLines(valStr, logicalLines, opts.wordWrap, L.textViewport.w, L.fontSize, textStyle, canvas, visualLines, foldRegions, state.foldedLines, opts.tabSize);
        L.totalContentH = keel::as<keel::f32>(visualLines.size()) * L.lineHeight;
        L.maxScrollY = keel::max(0.0f, L.totalContentH - L.textViewport.h);
        keel::usize targetVis = 0;
        for (keel::usize v = 0; v < visualLines.size(); ++v) {
            if (visualLines[v].logicalLine >= line0Based) {
                targetVis = v;
                break;
            }
        }
        const keel::f32 lineTop = keel::as<keel::f32>(targetVis) * L.lineHeight;
        const keel::f32 lineBot = lineTop + L.lineHeight;
        if (lineTop < state.scrollY || lineBot > state.scrollY + L.textViewport.h) {
            state.scrollY = keel::clamp(lineTop - L.textViewport.h * 0.35f, 0.0f, L.maxScrollY);
        }
    };

    if (optsCopy.executionLine > 0 && optsCopy.executionLine != state.prevExecutionLine) {
        scrollLogicalLineIntoView(optsCopy.executionLine - 1);
        state.prevExecutionLine = optsCopy.executionLine;
    } else if (optsCopy.executionLine == 0) {
        state.prevExecutionLine = 0;
    }

    // --- Handle IDE & Debugger Keyboard Shortcuts when focused ---
    bool modifiedByIdeCommand = false;
    if (isFocused && !effectiveDisabled) {
        // KeyAction::Find (default Ctrl+F)
        if (opts.enableSearch && ctx.ConsumeAction(KeyAction::Find)) {
            state.searchOpen = true;
            state.gotoOpen = false;
            state.overlayFocus = 1;
            keel::usize s0 = 0, s1 = 0;
            if (ctx.GetTextEditSelection(id, s0, s1) && s1 > s0 && (s1 - s0) <= 64) {
                const keel::StringRef sel = valStr.substr(s0, s1 - s0);
                if (sel.find('\n') == keel::StringRef::npos) {
                    state.searchQuery.assign(sel);
                }
            }
        }
        // KeyAction::Replace (default Ctrl+H)
        if (opts.enableSearch && !opts.readOnly && ctx.ConsumeAction(KeyAction::Replace)) {
            state.searchOpen = true;
            state.replaceOpen = true;
            state.gotoOpen = false;
            state.overlayFocus = state.searchQuery.empty() ? 1 : 2;
            keel::usize s0 = 0, s1 = 0;
            if (state.searchQuery.empty() && ctx.GetTextEditSelection(id, s0, s1) && s1 > s0 && (s1 - s0) <= 64) {
                const keel::StringRef sel = valStr.substr(s0, s1 - s0);
                if (sel.find('\n') == keel::StringRef::npos) {
                    state.searchQuery.assign(sel);
                }
            }
        }
        // KeyAction::GoToLine (default Ctrl+G)
        if (opts.enableSearch && ctx.ConsumeAction(KeyAction::GoToLine)) {
            state.gotoOpen = true;
            state.searchOpen = false;
            state.overlayFocus = 3;
            state.gotoQuery.clear();
        }
        // KeyAction::ToggleComment (default Ctrl+/)
        if (!opts.readOnly && ctx.ConsumeAction(KeyAction::ToggleComment)) {
            const keel::StringRef cmtPrefix = (opts.syntax.has_value() && !opts.syntax->lineComment.empty())
                ? opts.syntax->lineComment
                : "//";
            keel::usize selS = state.selectionStart;
            keel::usize selE = state.selectionEnd;
            if (selE > selS && selE <= valStr.size() && valStr[selE - 1] == '\n') {
                --selE;
            }
            const keel::usize firstLine = FindLogicalLineForOffset(logicalLines, selS);
            const keel::usize lastLine = FindLogicalLineForOffset(logicalLines, selE);

            bool allCommented = true;
            for (keel::usize li = firstLine; li <= lastLine; ++li) {
                const keel::StringRef ls = valStr.substr(logicalLines[li].startOffset, logicalLines[li].length);
                keel::usize p = 0;
                while (p < ls.size() && (ls[p] == ' ' || ls[p] == '\t')) ++p;
                if (p >= ls.size()) continue;
                if (!ls.substr(p).starts_with(cmtPrefix)) {
                    allCommented = false;
                    break;
                }
            }

            keel::String newDoc = keel::String::from(valStr);
            keel::usize newCur = state.cursor;
            keel::usize newAnc = (state.cursor == state.selectionStart) ? state.selectionEnd : state.selectionStart;
            keel::isize cumulativeShift = 0;

            for (keel::usize li = firstLine; li <= lastLine; ++li) {
                const keel::usize lineStart = keel::as<keel::usize>(keel::as<keel::isize>(logicalLines[li].startOffset) + cumulativeShift);
                const keel::usize lineLen = logicalLines[li].length;
                const keel::StringRef ls = newDoc.as_ref().substr(lineStart, lineLen);
                keel::usize p = 0;
                while (p < ls.size() && (ls[p] == ' ' || ls[p] == '\t')) ++p;
                if (p >= ls.size() && firstLine != lastLine) continue;

                const keel::usize editPos = lineStart + p;
                if (allCommented && ls.substr(p).starts_with(cmtPrefix)) {
                    keel::usize remLen = cmtPrefix.size();
                    if (p + remLen < ls.size() && ls[p + remLen] == ' ') {
                        ++remLen;
                    }
                    StringEraseSpan(newDoc, editPos, remLen);
                    if (newCur > editPos) newCur = (newCur >= editPos + remLen) ? (newCur - remLen) : editPos;
                    if (newAnc > editPos) newAnc = (newAnc >= editPos + remLen) ? (newAnc - remLen) : editPos;
                    cumulativeShift -= keel::as<keel::isize>(remLen);
                } else if (!allCommented) {
                    keel::SmallString<32> ins;
                    ins.append(cmtPrefix);
                    ins.push_back(' ');
                    StringInsertSpan(newDoc, editPos, ins);
                    if (newCur >= editPos) newCur += ins.size();
                    if (newAnc >= editPos) newAnc += ins.size();
                    cumulativeShift += keel::as<keel::isize>(ins.size());
                }
            }

            ctx.ApplyTextEditChange(id, newDoc, newCur, newAnc);
            valStr = ctx.EnsureTextEdit(id, newDoc, true, opts.readOnly);
            SplitLogicalLines(valStr, logicalLines);
            modifiedByIdeCommand = true;
        }

        // Debugger actions: ToggleBreakpoint (F9), DebugContinue (F5), DebugStop (Shift+F5), DebugStepOver (F10), DebugStepInto (F11), DebugStepOut (Shift+F11)
        if ((opts.breakpoints.has_target() || opts.enableBreakpoints) && ctx.ConsumeAction(KeyAction::ToggleBreakpoint)) {
            const keel::usize curLine0 = FindLogicalLineForOffset(logicalLines, state.cursor);
            toggleBreakpointAtLine(curLine0 + 1);
        }
        if (ctx.ConsumeAction(KeyAction::DebugContinue)) {
            res.debugAction = CodeDebugAction::Continue;
        } else if (ctx.ConsumeAction(KeyAction::DebugStop)) {
            res.debugAction = CodeDebugAction::Stop;
        }
        if (ctx.ConsumeAction(KeyAction::DebugStepOver)) {
            res.debugAction = CodeDebugAction::StepOver;
        }
        if (ctx.ConsumeAction(KeyAction::DebugStepInto)) {
            res.debugAction = CodeDebugAction::StepInto;
        } else if (ctx.ConsumeAction(KeyAction::DebugStepOut)) {
            res.debugAction = CodeDebugAction::StepOut;
        }
    }

    // Compute search matches if search is open
    keel::Array<SearchMatch> searchMatches;
    if (state.searchOpen && !state.searchQuery.empty()) {
        FindAllSearchMatches(valStr, logicalLines, state.searchQuery, state.matchCase, state.wholeWord, searchMatches);
    }
    state.searchMatchCount = searchMatches.size();
    if (state.activeMatchIdx >= searchMatches.size()) {
        state.activeMatchIdx = 0;
    }

    auto jumpToSearchMatch = [&](keel::usize matchIdx) {
        if (searchMatches.empty()) return;
        state.activeMatchIdx = matchIdx % searchMatches.size();
        const SearchMatch& m = searchMatches[state.activeMatchIdx];
        ctx.SetTextEditCursor(id, m.endOffset, m.startOffset);
        state.cursor = m.endOffset;
        state.selectionStart = m.startOffset;
        state.selectionEnd = m.endOffset;
        scrollLogicalLineIntoView(m.logicalLine);
    };

    auto performReplaceOne = [&]() {
        if (opts.readOnly || searchMatches.empty()) return;
        if (state.activeMatchIdx >= searchMatches.size()) state.activeMatchIdx = 0;
        const SearchMatch m = searchMatches[state.activeMatchIdx];
        keel::String newDoc;
        newDoc.reserve(valStr.size() - (m.endOffset - m.startOffset) + state.replaceQuery.size());
        newDoc.append(valStr.substr(0, m.startOffset));
        newDoc.append(state.replaceQuery);
        newDoc.append(valStr.substr(m.endOffset));
        const keel::usize newCur = m.startOffset + state.replaceQuery.size();
        ctx.ApplyTextEditChange(id, newDoc, newCur, newCur);
        valStr = ctx.EnsureTextEdit(id, newDoc, true, opts.readOnly);
        SplitLogicalLines(valStr, logicalLines);
        FindAllSearchMatches(valStr, logicalLines, state.searchQuery, state.matchCase, state.wholeWord, searchMatches);
        state.searchMatchCount = searchMatches.size();
        if (!searchMatches.empty()) {
            if (state.activeMatchIdx >= searchMatches.size()) state.activeMatchIdx = 0;
            jumpToSearchMatch(state.activeMatchIdx);
        }
        modifiedByIdeCommand = true;
    };

    auto performReplaceAll = [&]() {
        if (opts.readOnly || searchMatches.empty() || state.searchQuery.empty()) return;
        keel::String newDoc;
        newDoc.reserve(valStr.size());
        keel::usize lastPos = 0;
        for (const SearchMatch& m : searchMatches) {
            newDoc.append(valStr.substr(lastPos, m.startOffset - lastPos));
            newDoc.append(state.replaceQuery);
            lastPos = m.endOffset;
        }
        newDoc.append(valStr.substr(lastPos));
        const keel::usize newCur = keel::min(state.cursor, newDoc.size());
        ctx.ApplyTextEditChange(id, newDoc, newCur, newCur);
        valStr = ctx.EnsureTextEdit(id, newDoc, true, opts.readOnly);
        SplitLogicalLines(valStr, logicalLines);
        searchMatches.clear();
        state.searchMatchCount = 0;
        state.activeMatchIdx = 0;
        modifiedByIdeCommand = true;
    };

    // FindNext / FindPrev (default F3 / Shift+F3): Find Next / Previous even when focus is in code editor
    if (isFocused && !effectiveDisabled) {
        const bool findNext = ctx.ConsumeAction(KeyAction::FindNext);
        const bool findPrev = !findNext && ctx.ConsumeAction(KeyAction::FindPrev);
        if (findNext || findPrev) {
            if (!state.searchOpen) {
                state.searchOpen = true;
            }
            if (!searchMatches.empty()) {
                const keel::usize n = searchMatches.size();
                const keel::usize nextIdx = findPrev ? ((state.activeMatchIdx + n - 1) % n) : ((state.activeMatchIdx + 1) % n);
                jumpToSearchMatch(nextIdx);
            }
        }
    }

    // Handle keyboard input inside Find / Replace / GoToLine overlay when overlayFocus != 0
    if (isFocused && !effectiveDisabled && state.overlayFocus != 0) {
        keel::String* targetStr = (state.overlayFocus == 1) ? &state.searchQuery
                                : (state.overlayFocus == 2) ? &state.replaceQuery
                                : &state.gotoQuery;

        const auto typedList = ctx.ConsumePendingTextInput();
        bool queryEdited = false;
        for (const keel::String& t : typedList) {
            for (char c : t) {
                if (c >= 32 && c != 127) {
                    if (state.overlayFocus == 3 && !keel::is_ascii_digit(c)) {
                        continue;
                    }
                    targetStr->push_back(c);
                    queryEdited = true;
                }
            }
        }

        while (ctx.ConsumeAction(KeyAction::DeletePrevWord)) {
            if (!targetStr->empty()) {
                targetStr->clear();
                queryEdited = true;
            }
        }
        while (ctx.ConsumeAction(KeyAction::DeletePrevChar)) {
            if (!targetStr->empty()) {
                targetStr->pop_back();
                queryEdited = true;
            }
        }
        while (ctx.ConsumeAction(KeyAction::DeleteNextChar) || ctx.ConsumeAction(KeyAction::DeleteNextWord)) {
            targetStr->clear();
            queryEdited = true;
        }
        if (ctx.ConsumeAction(KeyAction::Indent) || ctx.ConsumeAction(KeyAction::Unindent)) {
            if (state.searchOpen) {
                if (!state.replaceOpen && !opts.readOnly) {
                    state.replaceOpen = true;
                }
                if (state.replaceOpen && !opts.readOnly) {
                    state.overlayFocus = (state.overlayFocus == 1) ? 2 : 1;
                }
            }
        }
        if (ctx.ConsumeAction(KeyAction::Cancel)) {
            state.searchOpen = false;
            state.gotoOpen = false;
            state.overlayFocus = 0;
        }
        KeyModifiers kMods{};
        if (ctx.ConsumeAction(KeyAction::Confirm) || ctx.ConsumeKey(Key::Enter, kMods)) {
            if (state.overlayFocus == 1) {
                if (!searchMatches.empty()) {
                    const keel::usize n = searchMatches.size();
                    const keel::usize nextIdx = kMods.shift ? ((state.activeMatchIdx + n - 1) % n) : ((state.activeMatchIdx + 1) % n);
                    jumpToSearchMatch(nextIdx);
                }
            } else if (state.overlayFocus == 2) {
                if (kMods.ctrl && kMods.alt) {
                    performReplaceAll();
                } else {
                    performReplaceOne();
                }
            } else if (state.overlayFocus == 3) {
                const auto lineRes = keel::parse_u64(state.gotoQuery);
                if (lineRes.is_ok() && lineRes.value() >= 1) {
                    const keel::usize targetLine1 = keel::as<keel::usize>(lineRes.value());
                    const keel::usize line0 = keel::min(targetLine1 - 1, logicalLines.size() - 1);
                    const keel::usize destOffset = logicalLines[line0].startOffset;
                    ctx.SetTextEditCursor(id, destOffset, destOffset);
                    state.cursor = destOffset;
                    state.selectionStart = destOffset;
                    state.selectionEnd = destOffset;
                    scrollLogicalLineIntoView(line0);
                }
                state.gotoOpen = false;
                state.overlayFocus = 0;
            }
        }

        if (queryEdited && state.overlayFocus == 1) {
            FindAllSearchMatches(valStr, logicalLines, state.searchQuery, state.matchCase, state.wholeWord, searchMatches);
            state.searchMatchCount = searchMatches.size();
            state.activeMatchIdx = 0;
            for (keel::usize mi = 0; mi < searchMatches.size(); ++mi) {
                if (searchMatches[mi].startOffset >= state.cursor) {
                    state.activeMatchIdx = mi;
                    break;
                }
            }
            if (!searchMatches.empty()) {
                scrollLogicalLineIntoView(searchMatches[state.activeMatchIdx].logicalLine);
            }
        }
    } else if (isFocused && !effectiveDisabled && (state.searchOpen || state.gotoOpen)) {
        // Pressing Cancel (Escape) while in editor with search/goto bar open closes the bar without losing focus
        if (ctx.ConsumeAction(KeyAction::Cancel)) {
            state.searchOpen = false;
            state.gotoOpen = false;
            state.overlayFocus = 0;
        }
    }

    // Recompute layout if overlay state changed
    if (opts.codeFolding) {
        ComputeFoldRegions(valStr, logicalLines, opts.syntax.has_value() ? &*opts.syntax : nullptr, foldRegions);
    }
    L = ComputeEditorLayout(
        refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
        state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
        state.searchOpen, state.replaceOpen, state.gotoOpen,
        foldRegions, state.foldedLines, visualLines,
        state.debugToolbarRatio
    );

    const Vec2 mousePos = ctx.GetMousePos();
    const bool copyHovered = opts.showCopyButton && res.hovered && L.copyBtnRect.w > 0.0f && L.copyBtnRect.Contains(mousePos);
    const bool zoomHovered = opts.allowZoom && res.hovered && L.zoomBtnRect.w > 0.0f && L.zoomBtnRect.Contains(mousePos);
    const bool vStartBtnHovered = L.needVScroll && L.vHasButtons && res.hovered && L.vStartBtnRect.Contains(mousePos);
    const bool vEndBtnHovered   = L.needVScroll && L.vHasButtons && res.hovered && L.vEndBtnRect.Contains(mousePos);
    const bool hStartBtnHovered = L.needHScroll && L.hHasButtons && res.hovered && L.hStartBtnRect.Contains(mousePos);
    const bool hEndBtnHovered   = L.needHScroll && L.hHasButtons && res.hovered && L.hEndBtnRect.Contains(mousePos);
    const bool vThumbHovered = L.needVScroll && res.hovered && L.vThumbRect.Contains(mousePos);
    const bool hThumbHovered = L.needHScroll && res.hovered && L.hThumbRect.Contains(mousePos);
    const bool vTrackHovered = L.needVScroll && res.hovered && L.vTrackRect.Contains(mousePos);
    const bool hTrackHovered = L.needHScroll && res.hovered && L.hTrackRect.Contains(mousePos);
    const bool minimapHovered = L.hasMinimap && res.hovered && L.minimapRect.Contains(mousePos);

    // Hit-test Search/Replace overlay buttons
    keel::i32 hoveredOverlayBtn = -1;
    if ((state.searchOpen || state.gotoOpen) && res.hovered && L.searchOverlay.panelRect.Contains(mousePos)) {
        const auto& S = L.searchOverlay;
        if (S.toggleReplaceBtn.w > 0.0f && S.toggleReplaceBtn.Contains(mousePos)) hoveredOverlayBtn = 0;
        else if (S.caseBtnRect.w > 0.0f && S.caseBtnRect.Contains(mousePos)) hoveredOverlayBtn = 1;
        else if (S.wordBtnRect.w > 0.0f && S.wordBtnRect.Contains(mousePos)) hoveredOverlayBtn = 2;
        else if (S.prevBtnRect.w > 0.0f && S.prevBtnRect.Contains(mousePos)) hoveredOverlayBtn = 3;
        else if (S.nextBtnRect.w > 0.0f && S.nextBtnRect.Contains(mousePos)) hoveredOverlayBtn = 4;
        else if (S.closeBtnRect.w > 0.0f && S.closeBtnRect.Contains(mousePos)) hoveredOverlayBtn = 5;
        else if (S.replaceOneBtnRect.w > 0.0f && S.replaceOneBtnRect.Contains(mousePos)) hoveredOverlayBtn = 6;
        else if (S.replaceAllBtnRect.w > 0.0f && S.replaceAllBtnRect.Contains(mousePos)) hoveredOverlayBtn = 7;
        else if (S.findInputRect.Contains(mousePos)) hoveredOverlayBtn = 10;
        else if (S.replaceInputRect.w > 0.0f && S.replaceInputRect.Contains(mousePos)) hoveredOverlayBtn = 11;
        else hoveredOverlayBtn = 99;
    }

    // Hit-test Debug Toolbar buttons & 6-dot drag grip handle
    keel::i32 hoveredDebugBtn = -1;
    if (opts.showDebugToolbar && res.hovered && L.debugToolbar.panelRect.w > 0.0f && L.debugToolbar.panelRect.Contains(mousePos)) {
        for (keel::i32 i = 0; i < 6; ++i) {
            if (L.debugToolbar.btnRects[keel::as<keel::usize>(i)].Contains(mousePos)) {
                hoveredDebugBtn = i;
                break;
            }
        }
        if (hoveredDebugBtn < 0) {
            hoveredDebugBtn = L.debugToolbar.gripRect.Contains(mousePos) ? 98 : 99;
        }
    }

    // Hit-test Gutter (Breakpoints & Code Folding chevrons)
    keel::i32 hoveredGutterLine = -1;
    keel::i32 hoveredFoldLine = -1;
    if (res.hovered && L.gutterRect.w > 0.0f && L.gutterRect.Contains(mousePos)) {
        const keel::f32 relY = (mousePos.y - L.textViewport.y) + state.scrollY;
        const keel::i32 rawVisIdx = keel::floor_i32(relY / keel::max(1.0f, L.lineHeight));
        if (rawVisIdx >= 0 && rawVisIdx < keel::as<keel::i32>(visualLines.size())) {
            const VisualLine& vl = visualLines[keel::as<keel::usize>(rawVisIdx)];
            if (vl.isFirstSubLine) {
                if (opts.codeFolding && mousePos.x >= L.gutterRect.Right() - L.foldColWidth - 2.0f) {
                    if (FindFoldRegionStartingAt(foldRegions, vl.logicalLine) != nullptr) {
                        hoveredFoldLine = keel::as<keel::i32>(vl.logicalLine);
                    }
                } else if (opts.breakpoints.has_target() || opts.enableBreakpoints) {
                    hoveredGutterLine = keel::as<keel::i32>(vl.logicalLine);
                }
            }
        }
    }

    // Handle mouse wheel
    if (res.hovered && !effectiveDisabled) {
        const Vec2 wheel = ctx.GetMouseWheel();
        if (keel::abs(wheel.y) > 0.001f || keel::abs(wheel.x) > 0.001f) {
            if (opts.allowZoom && ctx.GetKeyModifiers().ctrl && keel::abs(wheel.y) > 0.001f) {
                const keel::f32 prevZoom = state.zoom;
                applyZoomAnchor(state.zoom + (wheel.y > 0.0f ? 0.10f : -0.10f));
                if (keel::abs(state.zoom - prevZoom) > 0.001f) {
                    L = ComputeEditorLayout(
                        refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
                        state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                        state.searchOpen, state.replaceOpen, state.gotoOpen,
                        foldRegions, state.foldedLines, visualLines,
                        state.debugToolbarRatio
                    );
                }
                ctx.ConsumeMouseWheel();
            } else if (!opts.wordWrap && (ctx.GetKeyModifiers().shift || keel::abs(wheel.x) > keel::abs(wheel.y)) && L.maxScrollX > 0.0f) {
                const keel::f32 delta = (keel::abs(wheel.x) > 0.001f) ? wheel.x : wheel.y;
                state.scrollX = keel::clamp(state.scrollX - delta * L.lineHeight * 2.5f, 0.0f, L.maxScrollX);
                ctx.ConsumeMouseWheel();
            } else if (!opts.autoHeight && L.maxScrollY > 0.0f && keel::abs(wheel.y) > 0.001f) {
                state.scrollY = keel::clamp(state.scrollY - wheel.y * L.lineHeight * 2.5f, 0.0f, L.maxScrollY);
                ctx.ConsumeMouseWheel();
            }
        }
    }

    // Handle LMB press on overlay buttons, debug toolbar, gutter, minimap, scrollbars, or text viewport
    bool consumedByChrome = false;
    if (mouseJustPressed) {
        if (hoveredOverlayBtn >= 0) {
            consumedByChrome = true;
            if (hoveredOverlayBtn == 0) {
                state.replaceOpen = !state.replaceOpen;
            } else if (hoveredOverlayBtn == 1) {
                state.matchCase = !state.matchCase;
                FindAllSearchMatches(valStr, logicalLines, state.searchQuery, state.matchCase, state.wholeWord, searchMatches);
                state.searchMatchCount = searchMatches.size();
            } else if (hoveredOverlayBtn == 2) {
                state.wholeWord = !state.wholeWord;
                FindAllSearchMatches(valStr, logicalLines, state.searchQuery, state.matchCase, state.wholeWord, searchMatches);
                state.searchMatchCount = searchMatches.size();
            } else if (hoveredOverlayBtn == 3 && !searchMatches.empty()) {
                const keel::usize n = searchMatches.size();
                jumpToSearchMatch((state.activeMatchIdx + n - 1) % n);
            } else if (hoveredOverlayBtn == 4 && !searchMatches.empty()) {
                jumpToSearchMatch((state.activeMatchIdx + 1) % searchMatches.size());
            } else if (hoveredOverlayBtn == 5) {
                state.searchOpen = false;
                state.gotoOpen = false;
                state.overlayFocus = 0;
            } else if (hoveredOverlayBtn == 6) {
                performReplaceOne();
            } else if (hoveredOverlayBtn == 7) {
                performReplaceAll();
            } else if (hoveredOverlayBtn == 10) {
                state.overlayFocus = state.gotoOpen ? 3 : 1;
            } else if (hoveredOverlayBtn == 11) {
                state.overlayFocus = 2;
            }
        } else if (hoveredDebugBtn >= 0) {
            consumedByChrome = true;
            if (hoveredDebugBtn == 0) res.debugAction = CodeDebugAction::Continue;
            else if (hoveredDebugBtn == 1) res.debugAction = CodeDebugAction::StepOver;
            else if (hoveredDebugBtn == 2) res.debugAction = CodeDebugAction::StepInto;
            else if (hoveredDebugBtn == 3) res.debugAction = CodeDebugAction::StepOut;
            else if (hoveredDebugBtn == 4) res.debugAction = CodeDebugAction::Restart;
            else if (hoveredDebugBtn == 5) res.debugAction = CodeDebugAction::Stop;
            else if (hoveredDebugBtn == 98 || hoveredDebugBtn == 99) {
                const keel::f32 dist = (mousePos - state.lastClickPos).Length();
                if ((now - state.lastClickTime) <= 0.35 && dist <= 6.0f) {
                    state.debugToolbarRatio = -1.0f;
                    state.draggingDebugToolbar = false;
                    L = ComputeEditorLayout(
                        refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
                        state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                        state.searchOpen, state.replaceOpen, state.gotoOpen,
                        foldRegions, state.foldedLines, visualLines,
                        state.debugToolbarRatio
                    );
                } else {
                    state.draggingDebugToolbar = true;
                    state.debugToolbarGrabOffset = mousePos.x - L.debugToolbar.panelRect.x;
                }
                state.lastClickTime = now;
                state.lastClickPos = mousePos;
            }
        } else if (hoveredFoldLine >= 0) {
            consumedByChrome = true;
            const keel::usize fl = keel::as<keel::usize>(hoveredFoldLine);
            keel::usize foundIdx = state.foldedLines.size();
            for (keel::usize fi = 0; fi < state.foldedLines.size(); ++fi) {
                if (state.foldedLines[fi] == fl) {
                    foundIdx = fi;
                    break;
                }
            }
            if (foundIdx < state.foldedLines.size()) {
                state.foldedLines.remove_at(foundIdx);
            } else {
                state.foldedLines.push_back(fl);
            }
            L = ComputeEditorLayout(
                refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
                state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                state.searchOpen, state.replaceOpen, state.gotoOpen,
                foldRegions, state.foldedLines, visualLines,
                state.debugToolbarRatio
            );
        } else if (hoveredGutterLine >= 0) {
            consumedByChrome = true;
            toggleBreakpointAtLine(keel::as<keel::usize>(hoveredGutterLine) + 1);
        } else if (copyHovered) {
            consumedByChrome = true;
            keel::StringRef toCopy = valStr;
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
                state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                state.searchOpen, state.replaceOpen, state.gotoOpen,
                foldRegions, state.foldedLines, visualLines,
                state.debugToolbarRatio
            );
        } else if (minimapHovered) {
            consumedByChrome = true;
            state.draggingMinimap = true;
        } else if (vStartBtnHovered) {
            consumedByChrome = true;
            state.scrollY = keel::max(0.0f, state.scrollY - theme.GetScrollbar(activeSize).scrollStep);
        } else if (vEndBtnHovered) {
            consumedByChrome = true;
            state.scrollY = keel::min(L.maxScrollY, state.scrollY + theme.GetScrollbar(activeSize).scrollStep);
        } else if (hStartBtnHovered) {
            consumedByChrome = true;
            state.scrollX = keel::max(0.0f, state.scrollX - theme.GetScrollbar(activeSize).scrollStep);
        } else if (hEndBtnHovered) {
            consumedByChrome = true;
            state.scrollX = keel::min(L.maxScrollX, state.scrollX + theme.GetScrollbar(activeSize).scrollStep);
        } else if (vThumbHovered) {
            consumedByChrome = true;
            state.draggingVThumb = true;
            state.dragGrabOffset = mousePos.y - L.vThumbRect.y;
        } else if (vTrackHovered) {
            consumedByChrome = true;
            const keel::f32 halfThumb = L.vThumbRect.h * 0.5f;
            const keel::f32 maxTravel = keel::max(1.0f, L.vTrackRect.h - L.vThumbRect.h);
            const keel::f32 t = keel::clamp((mousePos.y - L.vTrackRect.y - halfThumb) / maxTravel, 0.0f, 1.0f);
            state.scrollY = t * L.maxScrollY;
            state.draggingVThumb = true;
            state.dragGrabOffset = halfThumb;
        } else if (hThumbHovered) {
            consumedByChrome = true;
            state.draggingHThumb = true;
            state.dragGrabOffset = mousePos.x - L.hThumbRect.x;
        } else if (hTrackHovered) {
            consumedByChrome = true;
            const keel::f32 halfThumb = L.hThumbRect.w * 0.5f;
            const keel::f32 maxTravel = keel::max(1.0f, L.hTrackRect.w - L.hThumbRect.w);
            const keel::f32 t = keel::clamp((mousePos.x - L.hTrackRect.x - halfThumb) / maxTravel, 0.0f, 1.0f);
            state.scrollX = t * L.maxScrollX;
            state.draggingHThumb = true;
            state.dragGrabOffset = halfThumb;
        } else if (L.editorBox.Contains(mousePos)) {
            state.overlayFocus = 0;
            state.draggingText = true;
        }
    }

    if (state.draggingDebugToolbar && ctx.IsMouseDown() && opts.showDebugToolbar && L.debugToolbar.panelRect.w > 0.0f) {
        hoveredDebugBtn = 98;
        const keel::f32 travel = L.debugToolbar.maxBarX - L.debugToolbar.minBarX;
        if (travel > 0.5f) {
            const keel::f32 targetX = mousePos.x - state.debugToolbarGrabOffset;
            state.debugToolbarRatio = keel::clamp((targetX - L.debugToolbar.minBarX) / travel, 0.0f, 1.0f);
            L = ComputeEditorLayout(
                refBounds, valStr, logicalLines, optsCopy, activeSize, canvas, theme,
                state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                state.searchOpen, state.replaceOpen, state.gotoOpen,
                foldRegions, state.foldedLines, visualLines,
                state.debugToolbarRatio
            );
        }
    }
    if (state.draggingMinimap && ctx.IsMouseDown() && L.hasMinimap && L.minimapRect.h > 0.0f) {
        const keel::f32 microStep = 3.0f;
        const keel::f32 totalMiniH = keel::max(1.0f, keel::as<keel::f32>(visualLines.size()) * microStep);
        const keel::f32 effMiniH = keel::min(L.minimapRect.h, totalMiniH);
        const keel::f32 t = keel::clamp((mousePos.y - L.minimapRect.y) / keel::max(1.0f, effMiniH), 0.0f, 1.0f);
        state.scrollY = t * L.maxScrollY;
    }
    if (state.draggingVThumb && ctx.IsMouseDown() && L.needVScroll) {
        const keel::f32 maxTravel = keel::max(1.0f, L.vTrackRect.h - L.vThumbRect.h);
        const keel::f32 t = keel::clamp((mousePos.y - L.vTrackRect.y - state.dragGrabOffset) / maxTravel, 0.0f, 1.0f);
        state.scrollY = t * L.maxScrollY;
    }
    if (state.draggingHThumb && ctx.IsMouseDown() && L.needHScroll) {
        const keel::f32 maxTravel = keel::max(1.0f, L.hTrackRect.w - L.hThumbRect.w);
        const keel::f32 t = keel::clamp((mousePos.x - L.hTrackRect.x - state.dragGrabOffset) / maxTravel, 0.0f, 1.0f);
        state.scrollX = t * L.maxScrollX;
    }

    state.scrollY = keel::clamp(state.scrollY, 0.0f, L.maxScrollY);
    state.scrollX = keel::clamp(state.scrollX, 0.0f, L.maxScrollX);

    keel::usize cursor = state.cursor;
    keel::usize selAnchor = state.cursor;
    keel::StringRef displayText = valStr;

    if (isFocused || ctx.HasActiveTextEdit(id)) {
        // 2D Mouse hit-testing for cursor & selection
        if (!consumedByChrome && (mouseJustPressed || (state.draggingText && ctx.IsMouseDown()))) {
            const keel::f32 relY = (mousePos.y - L.textViewport.y) + state.scrollY;
            const keel::f32 relX = (mousePos.x - L.textViewport.x) + state.scrollX;

            const keel::i32 rawVisIdx = keel::floor_i32(relY / keel::max(1.0f, L.lineHeight));
            const keel::usize visIdx = keel::as<keel::usize>(keel::clamp(rawVisIdx, 0, keel::as<keel::i32>(visualLines.size()) - 1));
            const VisualLine& vl = visualLines[visIdx];
            const keel::StringRef subStr = valStr.substr(vl.startOffset, vl.length);
            const keel::usize colInSub = HitTestColumnInLine(canvas, subStr, relX, L.fontSize, textStyle, opts.tabSize);
            const keel::usize hitIdx = vl.startOffset + colInSub;

            if (mouseJustPressed) {
                const keel::f32 dist = (mousePos - state.lastClickPos).Length();
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
                        const keel::usize anchor = ctx.GetTextEditAnchor(id);
                        ctx.SetTextEditCursor(id, hitIdx, anchor);
                    } else {
                        ctx.SetTextEditCursor(id, hitIdx, hitIdx);
                    }
                } else if (state.clickCount == 2) {
                    keel::usize wStart = 0;
                    keel::usize wEnd = 0;
                    TextUtil::SelectWordAt(valStr, hitIdx, wStart, wEnd);
                    state.draggingWord = true;
                    state.wordDragStart = wStart;
                    state.wordDragEnd = wEnd;
                    ctx.SetTextEditCursor(id, wEnd, wStart);
                } else {
                    state.draggingWord = false;
                    const LogicalLine& ll = logicalLines[vl.logicalLine];
                    ctx.SetTextEditCursor(id, ll.startOffset + ll.length, ll.startOffset);
                }
            } else if (state.draggingText) {
                if (mousePos.y < L.textViewport.y && state.scrollY > 0.0f) {
                    state.scrollY = keel::max(0.0f, state.scrollY - L.lineHeight * 0.35f);
                } else if (mousePos.y > L.textViewport.Bottom() && state.scrollY < L.maxScrollY) {
                    state.scrollY = keel::min(L.maxScrollY, state.scrollY + L.lineHeight * 0.35f);
                }
                if (!opts.wordWrap) {
                    if (mousePos.x < L.textViewport.x && state.scrollX > 0.0f) {
                        state.scrollX = keel::max(0.0f, state.scrollX - L.lineHeight * 0.5f);
                    } else if (mousePos.x > L.textViewport.Right() && state.scrollX < L.maxScrollX) {
                        state.scrollX = keel::min(L.maxScrollX, state.scrollX + L.lineHeight * 0.5f);
                    }
                }

                if (state.clickCount >= 3) {
                    const LogicalLine& ll = logicalLines[vl.logicalLine];
                    ctx.SetTextEditCursor(id, ll.startOffset + ll.length, ll.startOffset);
                } else if (state.draggingWord) {
                    keel::usize wStart = 0;
                    keel::usize wEnd = 0;
                    TextUtil::SelectWordAt(valStr, hitIdx, wStart, wEnd);
                    if (hitIdx < state.wordDragStart) {
                        ctx.SetTextEditCursor(id, wStart, state.wordDragEnd);
                    } else {
                        ctx.SetTextEditCursor(id, wEnd, state.wordDragStart);
                    }
                } else {
                    const keel::usize anchor = ctx.GetTextEditAnchor(id);
                    ctx.SetTextEditCursor(id, hitIdx, anchor);
                }
            }
        }

        bool changed = false;
        bool submitted = false;
        displayText = ctx.GetOrStartTextEdit(id, valStr, cursor, selAnchor, changed, submitted, /*multiline=*/true, opts.readOnly);
        changed = changed || modifiedByIdeCommand || (displayText != state.prevDocumentText);

        if (changed && !opts.readOnly) {
            if (stringValue.has_value()) {
                const keel::StringRef prevCallerVal = ctx.AllocString(stringValue->as_ref());
                stringValue->assign(displayText);
                const keel::StringRef stored = stringValue->as_ref();
                if (stored != displayText) {
                    cursor = keel::min(cursor, stored.size());
                    selAnchor = keel::min(selAnchor, stored.size());
                    ctx.SetTextEditBuffer(id, stored);
                    ctx.SetTextEditCursor(id, cursor, selAnchor);
                    displayText = stored;
                }
                res.changed = (displayText != prevCallerVal) || (displayText != state.prevDocumentText);
            } else {
                res.changed = true;
            }
            keel::Array<LogicalLine> oldLogicalLines;
            SplitLogicalLines(state.prevDocumentText, oldLogicalLines);
            SplitLogicalLines(displayText, logicalLines);
            const keel::usize oldLineCount = oldLogicalLines.size();
            const keel::usize newLineCount = logicalLines.size();

            // Automatically shift breakpoints, executionLine, and baseline line mapping when lines are inserted/deleted
            if (newLineCount != oldLineCount) {
                const keel::isize diff = keel::as<keel::isize>(newLineCount) - keel::as<keel::isize>(oldLineCount);
                const keel::StringRef oldDoc = state.prevDocumentText.as_ref();
                keel::usize prefixLen = 0;
                const keel::usize maxCommon = keel::min(oldDoc.size(), displayText.size());
                while (prefixLen < maxCommon && oldDoc[prefixLen] == displayText[prefixLen]) {
                    ++prefixLen;
                }
                keel::usize rawSuffixLen = 0;
                while (rawSuffixLen < maxCommon &&
                       oldDoc[oldDoc.size() - 1 - rawSuffixLen] == displayText[displayText.size() - 1 - rawSuffixLen]) {
                    ++rawSuffixLen;
                }
                const keel::usize suffixBound = oldDoc.size() - rawSuffixLen;
                const keel::usize editLo = keel::min(prefixLen, suffixBound);
                const keel::usize editHi = keel::max(prefixLen, suffixBound);
                const keel::usize safePreCur = keel::clamp(
                    keel::min(state.prevCursor, oldDoc.size()),
                    editLo,
                    editHi
                );
                const keel::usize oldEditLine0 = FindLogicalLineForOffset(oldLogicalLines, safePreCur);
                const keel::usize oldEditLine1 = oldEditLine0 + 1;
                const keel::usize safePostCur = keel::min(cursor, displayText.size());
                const keel::usize newEditLine0 = FindLogicalLineForOffset(logicalLines, safePostCur);
                const keel::usize newEditLine1 = newEditLine0 + 1;

                keel::usize shiftStartLine1 = oldEditLine1;
                if (diff > 0) {
                    const bool hasCodeBefore = HasCodeBeforeOffsetOnLine(
                        state.prevDocumentText,
                        oldLogicalLines[oldEditLine0],
                        safePreCur
                    );
                    shiftStartLine1 = hasCodeBefore ? (oldEditLine1 + 1) : oldEditLine1;
                } else {
                    shiftStartLine1 = newEditLine1 + 1;
                }

                const keel::isize minTargetLine = (diff < 0) ? keel::as<keel::isize>(newEditLine1) : 1;
                auto shiftLineNum = [&](keel::usize& lineNum) {
                    if (lineNum == 0) return;
                    if (lineNum >= shiftStartLine1) {
                        const keel::isize shifted = keel::max<keel::isize>(minTargetLine, keel::as<keel::isize>(lineNum) + diff);
                        lineNum = keel::min<keel::usize>(keel::as<keel::usize>(shifted), newLineCount);
                    } else if (lineNum > newLineCount) {
                        lineNum = newLineCount;
                    }
                };

                keel::MutArrayRef<keel::usize> bpMut = activeBreakpoints.as_mut_ref();
                for (keel::usize& bp : bpMut) {
                    shiftLineNum(bp);
                }
                if (bpMut.size() > 1) {
                    SortSizeT(bpMut);
                    for (keel::usize bi = bpMut.size() - 1; bi > 0; --bi) {
                        if (bpMut[bi] == bpMut[bi - 1]) {
                            activeBreakpoints.erase_at(bi);
                        }
                    }
                }

                for (keel::usize b = 1; b < state.baselineToCurrentLine.size(); ++b) {
                    shiftLineNum(state.baselineToCurrentLine[b]);
                }

                if (opts.executionLinePtr != nullptr && *opts.executionLinePtr > 0) {
                    shiftLineNum(*opts.executionLinePtr);
                    optsCopy.executionLine = *opts.executionLinePtr;
                    state.prevExecutionLine = *opts.executionLinePtr;
                }

                res.lineDelta = keel::as<keel::i32>(diff);
                res.editLine = (diff > 0) ? oldEditLine1 : newEditLine1;
            }

            state.prevDocumentText.assign(displayText);
            state.prevLineCount = newLineCount;
            state.cachedDocHash = detail::HashArgs(displayText);
            state.cachedDocSize = displayText.size();
            resolveDebugAndDiagnosticMarkers(displayText);

            if (opts.codeFolding) {
                ComputeFoldRegions(displayText, logicalLines, opts.syntax.has_value() ? &*opts.syntax : nullptr, foldRegions);
                state.cachedFoldEnabled = true;
                state.cachedFoldSyntaxHash = foldSyntaxHash;
            } else {
                foldRegions.clear();
                state.cachedFoldEnabled = false;
            }
            L = ComputeEditorLayout(
                refBounds, displayText, logicalLines, optsCopy, activeSize, canvas, theme,
                state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                state.searchOpen, state.replaceOpen, state.gotoOpen,
                foldRegions, state.foldedLines, visualLines,
                state.debugToolbarRatio
            );
        }

        state.cursor = cursor;
        state.selectionStart = keel::min(cursor, selAnchor);
        state.selectionEnd = keel::max(cursor, selAnchor);

        // If cursor moved into a folded region via keyboard navigation, unfold it
        const keel::usize curLogical = FindLogicalLineForOffset(logicalLines, cursor);
        if (IsLineHiddenByFold(foldRegions, state.foldedLines, curLogical)) {
            ensureLineUnfolded(curLogical);
            L = ComputeEditorLayout(
                refBounds, displayText, logicalLines, optsCopy, activeSize, canvas, theme,
                state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
                state.searchOpen, state.replaceOpen, state.gotoOpen,
                foldRegions, state.foldedLines, visualLines,
                state.debugToolbarRatio
            );
        }

        // Auto-scroll to keep active cursor visible when cursor or text changes
        if (cursor != state.prevCursor || displayText.size() != state.prevTextSize) {
            const keel::usize curVisRow = FindVisualLineForOffset(visualLines, cursor);
            const VisualLine& cvl = visualLines[curVisRow];
            const keel::f32 curTop = keel::as<keel::f32>(curVisRow) * L.lineHeight;
            const keel::f32 curBot = curTop + L.lineHeight;

            if (curTop < state.scrollY) {
                state.scrollY = curTop;
            } else if (curBot > state.scrollY + L.textViewport.h && L.textViewport.h > 0.0f) {
                state.scrollY = curBot - L.textViewport.h;
            }

            if (!opts.wordWrap && L.textViewport.w > 0.0f) {
                const keel::usize colOffset = (cursor >= cvl.startOffset) ? keel::min(cursor - cvl.startOffset, cvl.length) : 0;
                const keel::StringRef prefix = displayText.substr(cvl.startOffset, colOffset);
                const keel::f32 curX = MeasureSegmentWidth(canvas, prefix, L.fontSize, textStyle, opts.tabSize);
                const keel::f32 margin = 16.0f;
                if (curX < state.scrollX + margin) {
                    state.scrollX = keel::max(0.0f, curX - margin);
                } else if (curX > state.scrollX + L.textViewport.w - margin) {
                    state.scrollX = keel::min(L.maxScrollX, curX - L.textViewport.w + margin);
                }
            }

            state.scrollY = keel::clamp(state.scrollY, 0.0f, L.maxScrollY);
            state.scrollX = keel::clamp(state.scrollX, 0.0f, L.maxScrollX);
            state.prevCursor = cursor;
            state.prevTextSize = displayText.size();
        }

        if (isFocused && !opts.readOnly && state.overlayFocus == 0 && !visualLines.empty() && L.textViewport.w > 0.0f) {
            const keel::usize curVisRow = FindVisualLineForOffset(visualLines, cursor);
            const VisualLine& cvl = visualLines[curVisRow];
            const keel::usize colOffset = (cursor >= cvl.startOffset)
                ? keel::min(cursor - cvl.startOffset, cvl.length)
                : 0u;
            const keel::StringRef prefix = displayText.substr(cvl.startOffset, colOffset);
            const keel::f32 caretX = L.textViewport.x - state.scrollX +
                MeasureSegmentWidth(canvas, prefix, L.fontSize, textStyle, opts.tabSize);
            const keel::f32 caretY = L.textViewport.y - state.scrollY +
                keel::as<keel::f32>(curVisRow) * L.lineHeight;
            ctx.SetImeTargetRect(Rect(caretX, caretY, 2.0f, L.lineHeight));
        }
    } else {
        state.selectionStart = state.cursor;
        state.selectionEnd = state.cursor;
    }

    // Set suspend direct edit input for the next frame's pre-BeginFrame events if overlay input is focused
    ctx.SetSuspendDirectEditInput(isFocused && state.overlayFocus != 0);

    // Tooltips for Debug Toolbar & Search/Replace overlay buttons
    if (hoveredDebugBtn >= 0 && hoveredDebugBtn < 6) {
        static constexpr keel::StaticArray<keel::StringRef, 6> kDbgTips = {
            "Continue (F5)",
            "Step Over (F10)",
            "Step Into (F11)",
            "Step Out (Shift+F11)",
            "Restart Debugger",
            "Stop Debugging (Shift+F5)"
        };
        ctx.SetTooltip(kDbgTips[keel::as<keel::usize>(hoveredDebugBtn)], L.debugToolbar.btnRects[keel::as<keel::usize>(hoveredDebugBtn)]);
    } else if (hoveredDebugBtn == 98 && !state.draggingDebugToolbar) {
        ctx.SetTooltip("Drag to move Debug Toolbar (Double-click to reset)", L.debugToolbar.gripRect);
    } else if (hoveredOverlayBtn >= 0) {
        static constexpr keel::StaticArray<keel::StringRef, 8> kOverlayTips = {
            "Toggle Replace (Ctrl+H)",
            "Match Case",
            "Match Whole Word",
            "Previous Match (Shift+Enter)",
            "Next Match (Enter / F3)",
            "Close (Escape)",
            "Replace Next Match",
            "Replace All Matches"
        };
        if (hoveredOverlayBtn < 8) {
            ctx.SetTooltip(kOverlayTips[keel::as<keel::usize>(hoveredOverlayBtn)], Rect(mousePos.x - 4.0f, mousePos.y - 4.0f, 8.0f, 8.0f));
        }
    }

    // Detect hovered word & diagnostic tooltip in textViewport
    keel::StringRef hoveredWord{};
    keel::usize hoveredWordLine = 0;
    keel::usize hoveredWordCol = 0;
    if (res.hovered && L.textViewport.Contains(mousePos) && hoveredOverlayBtn < 0 && hoveredDebugBtn < 0) {
        const keel::f32 relY = (mousePos.y - L.textViewport.y) + state.scrollY;
        const keel::f32 relX = (mousePos.x - L.textViewport.x) + state.scrollX;
        const keel::i32 rawVisIdx = keel::floor_i32(relY / keel::max(1.0f, L.lineHeight));
        if (rawVisIdx >= 0 && rawVisIdx < keel::as<keel::i32>(visualLines.size())) {
            const VisualLine& vl = visualLines[keel::as<keel::usize>(rawVisIdx)];
            const keel::StringRef subStr = displayText.substr(vl.startOffset, vl.length);
            const keel::usize colInSub = HitTestColumnInLine(canvas, subStr, relX, L.fontSize, textStyle, opts.tabSize);
            const keel::usize hitOffset = vl.startOffset + colInSub;
            keel::usize wordStartOff = 0;
            hoveredWord = ExtractWordAtOffset(displayText, hitOffset, &wordStartOff);
            if (!hoveredWord.empty()) {
                hoveredWordLine = vl.logicalLine;
                hoveredWordCol = wordStartOff - logicalLines[vl.logicalLine].startOffset;
            }

            // Check if hovering over a diagnostic on this line
            for (const CodeDiagnostic& diag : optsCopy.diagnostics) {
                if (diag.line == vl.logicalLine + 1 && !diag.message.empty()) {
                    const keel::usize col0 = vl.startCol + colInSub;
                    const keel::usize dStart = diag.startCol;
                    const keel::usize dEnd = (diag.endCol > diag.startCol) ? diag.endCol : logicalLines[vl.logicalLine].length;
                    if (col0 >= dStart && col0 <= dEnd) {
                        const auto tip = diag.code.empty()
                            ? ctx.String(diag.message)
                            : ctx.Format("[%s] %s", diag.code, diag.message);
                        ctx.SetTooltip(tip, Rect(mousePos.x - 4.0f, mousePos.y - 4.0f, 8.0f, 8.0f));
                        break;
                    }
                }
            }
        }
    }

    // Populate TextAreaResponse telemetry
    const keel::usize safeCur = keel::min(cursor, displayText.size());
    const keel::usize curLogIdx = FindLogicalLineForOffset(logicalLines, safeCur);
    res.cursorLine = curLogIdx + 1;
    res.cursorColumn = (safeCur - logicalLines[curLogIdx].startOffset) + 1;
    res.totalLines = logicalLines.size();
    res.searchMatchCount = state.searchMatchCount;
    res.activeSearchMatch = (state.searchMatchCount > 0) ? (state.activeMatchIdx + 1) : 0;
    const keel::StringRef arenaHoveredWord = ctx.AllocString(hoveredWord);
    res.hoveredWord = arenaHoveredWord;

    const bool cursorVisible = isFocused && ctx.IsCursorVisible(opts.cursorMode);
    const keel::f32 cursorWidth = ctx.GetCursorConfig().width;

    const keel::ArrayRef<keel::usize> bpView = activeBreakpoints.as_ref();
    keel::MutArrayRef<keel::usize> arenaBps{};
    if (!bpView.empty()) {
        arenaBps = ctx.GetArena().create_slice<keel::usize>(bpView.size());
        for (keel::usize i = 0; i < bpView.size(); ++i) {
            arenaBps[i] = bpView[i];
        }
    }
    keel::MutArrayRef<keel::usize> arenaFolded{};
    if (!state.foldedLines.empty()) {
        arenaFolded = ctx.GetArena().create_slice<keel::usize>(state.foldedLines.size());
        for (keel::usize i = 0; i < state.foldedLines.size(); ++i) {
            arenaFolded[i] = state.foldedLines[i];
        }
    }

    Node* node = ctx.CreateNode(NodeType::TextArea, id);
    auto* data = ctx.AllocPayload<TextAreaData>();
    if (data) {
        data->opts = optsCopy;
        data->text = ctx.AllocString(displayText);
        data->cursor = cursor;
        data->selectionStart = state.selectionStart;
        data->selectionEnd = state.selectionEnd;
        data->cursorVisible = cursorVisible;
        data->cursorWidth = cursorWidth;
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
        data->minimapHovered = minimapHovered;
        data->minimapDragging = state.draggingMinimap;
        data->debugToolbarDragging = state.draggingDebugToolbar;
        data->debugToolbarRatio = state.debugToolbarRatio;
        data->searchOpen = state.searchOpen;
        data->replaceOpen = state.replaceOpen;
        data->gotoOpen = state.gotoOpen;
        data->overlayFocus = state.overlayFocus;
        data->searchQuery = ctx.AllocString(state.searchQuery);
        data->replaceQuery = ctx.AllocString(state.replaceQuery);
        data->gotoQuery = ctx.AllocString(state.gotoQuery);
        data->matchCase = state.matchCase;
        data->wholeWord = state.wholeWord;
        data->activeMatchIdx = state.activeMatchIdx;
        data->searchMatchCount = state.searchMatchCount;
        data->hoveredOverlayBtn = hoveredOverlayBtn;
        data->foldedLines = arenaFolded;
        data->breakpoints = arenaBps;
        data->precomputedLogicalLines = logicalLines;
        data->precomputedFoldRegions = foldRegions;
        data->hoveredGutterLine = hoveredGutterLine;
        data->hoveredFoldLine = hoveredFoldLine;
        data->hoveredDebugBtn = hoveredDebugBtn;
        data->hoveredWord = arenaHoveredWord;
        data->hoveredWordLine = hoveredWordLine;
        data->hoveredWordCol = hoveredWordCol;
    }
    node->SetPayload(data);

    keel::u64 markersHash = detail::HashArgs(optsCopy.diagnostics.size(), optsCopy.inlineValues.size(), bpView.size());
    for (const CodeInlineValue& iv : optsCopy.inlineValues) {
        markersHash = detail::HashArgs(markersHash, iv.line, iv.text, keel::as<keel::u8>(iv.variant));
    }
    for (const CodeDiagnostic& d : optsCopy.diagnostics) {
        markersHash = detail::HashArgs(markersHash, d.line, d.startCol, d.endCol, keel::as<keel::u8>(d.severity), d.message);
    }
    for (keel::usize bp : bpView) {
        markersHash = detail::HashArgs(markersHash, bp);
    }

    const SyntaxPalette& effSynPal = optsCopy.syntaxPalette.has_value()
        ? *optsCopy.syntaxPalette
        : ((optsCopy.syntax.has_value() && optsCopy.syntax->palette.has_value()) ? *optsCopy.syntax->palette : theme.syntax);

    node->visualHash = detail::HashArgs(
        data ? data->text : displayText,
        optsCopy.placeholder,
        optsCopy.color,
        optsCopy.width,
        optsCopy.height,
        optsCopy.minWidth.value_or(0.0f),
        optsCopy.maxWidth.value_or(0.0f),
        optsCopy.minHeight.value_or(0.0f),
        optsCopy.maxHeight.value_or(0.0f),
        optsCopy.readOnly,
        optsCopy.showLineNumbers,
        optsCopy.showCopyButton,
        optsCopy.monospace,
        optsCopy.wordWrap,
        optsCopy.tabSize,
        optsCopy.insertSpaces,
        optsCopy.autoHeight,
        optsCopy.rows.value_or(0u),
        optsCopy.background,
        optsCopy.border,
        optsCopy.syntax.has_value() ? optsCopy.syntax->enabled : false,
        optsCopy.syntax.has_value() ? optsCopy.syntax->languageName : keel::StringRef{},
        effSynPal.editorBg.ToUint32(),
        effSynPal.keyword.ToUint32(),
        effSynPal.string.ToUint32(),
        effSynPal.comment.ToUint32(),
        optsCopy.showMinimap,
        optsCopy.showStatusBar,
        optsCopy.showIndentGuides,
        optsCopy.codeFolding,
        optsCopy.executionLine,
        optsCopy.showDebugToolbar,
        keel::as<keel::u8>(optsCopy.debugToolbarAlign),
        state.debugToolbarRatio,
        state.draggingDebugToolbar,
        optsCopy.toolbarSize.value_or(activeSize),
        markersHash,
        state.foldedLines.size(),
        state.searchOpen,
        state.replaceOpen,
        state.gotoOpen,
        state.overlayFocus,
        data ? data->searchQuery : keel::StringRef{},
        data ? data->replaceQuery : keel::StringRef{},
        data ? data->gotoQuery : keel::StringRef{},
        state.matchCase,
        state.wholeWord,
        state.activeMatchIdx,
        hoveredOverlayBtn,
        hoveredDebugBtn,
        hoveredGutterLine,
        hoveredFoldLine,
        data ? data->hoveredWord : keel::StringRef{},
        cursor,
        state.selectionStart,
        state.selectionEnd,
        cursorVisible,
        cursorWidth,
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
        minimapHovered,
        state.draggingMinimap,
        res.hovered,
        res.pressed,
        isFocused,
        effectiveDisabled
    );

    node->measureFn = [](const Node* n, keel::f32 availableWidth, ICanvas* c, const UiTheme& t) -> Vec2 {
        const auto* d = n->As<TextAreaData>();
        if (!d) return {0.0f, 0.0f};
        TextAreaOptions mOpts = d->opts;
        if (n->widthMode == SizeMode::Stretch && !mOpts.width.IsFixed()) {
            mOpts.width = SizeMode::Stretch;
        }
        mOpts.zoom = d->zoom;
        return detail::TextArea::Measure(d->text, mOpts, n->uiSize, c, t, availableWidth);
    };
    node->renderFn = [](const Node* n, ICanvas& c, const UiTheme& t) {
        const auto* d = n->As<TextAreaData>();
        if (d) {
            detail::TextArea::Render(
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

    ControlSpec ctrlSpec = Control::FromOptions(optsCopy, SizeMode::Stretch, SizeMode::Fit);
    if (optsCopy.autoHeight && !optsCopy.height.IsFixed()) {
        ctrlSpec.height = AxisSize(SizeMode::Fit);
    }
    Control::SetupNode(ctx, node, ctrlSpec);
    if ((!node->parent || node->parent->type != NodeType::Element) &&
        ctrlSpec.height.ResolveMode(SizeMode::Fit) == SizeMode::Fit &&
        node->heightMode == SizeMode::Stretch) {
        node->heightMode = SizeMode::Fit;
    }
    return res;
}


} // namespace guikit
