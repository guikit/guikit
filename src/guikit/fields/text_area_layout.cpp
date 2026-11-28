#include "text_area_internal.hpp"

namespace guikit::detail::ta_impl {

namespace {

[[nodiscard]] keel::usize LowerBoundLogicalLineForOffset(
    keel::ArrayRef<LogicalLine> lines,
    keel::usize byteOffset
) noexcept {
    if (lines.empty()) return 0u;
    keel::usize lo = 0u;
    keel::usize hi = lines.size();
    while (lo + 1u < hi) {
        const keel::usize mid = lo + (hi - lo) / 2u;
        if (lines[mid].startOffset <= byteOffset) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

[[nodiscard]] keel::usize LowerBoundPrefixUsize(
    keel::ArrayRef<keel::usize> prefix,
    keel::usize target
) noexcept {
    // Returns largest index i in [0, prefix.size() - 2] such that prefix[i] <= target.
    if (prefix.size() <= 2u) return 0u;
    keel::usize lo = 0u;
    keel::usize hi = prefix.size() - 1u;
    while (lo + 1u < hi) {
        const keel::usize mid = lo + (hi - lo) / 2u;
        if (prefix[mid] <= target) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return lo;
}

} // namespace

// =============================================================================
// Basic Utilities & TextSourceView
// =============================================================================

keel::u64 HashFoldedLines(keel::ArrayRef<keel::usize> foldedLines) noexcept {
    keel::u64 h = detail::HashArgs(foldedLines.size());
    for (keel::usize fl : foldedLines) {
        h = detail::HashArgs(h, fl);
    }
    return h;
}

bool ContainsSizeT(keel::ArrayRef<keel::usize> list, keel::usize val) noexcept {
    for (keel::usize item : list) {
        if (item == val) return true;
    }
    return false;
}

void SortSizeT(keel::MutArrayRef<keel::usize> arr) noexcept {
    const keel::usize n = arr.size();
    for (keel::usize gap = n / 2u; gap > 0u; gap /= 2u) {
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
    if (pos >= s.size() || len == 0u) return;
    const keel::usize rem = keel::min(len, s.size() - pos);
    const keel::usize tail = s.size() - (pos + rem);
    if (tail > 0u) {
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
    if (tail > 0u) {
        keel::mem_move(s.data() + clampedPos + ins.size(), s.data() + clampedPos, tail);
    }
    keel::mem_copy(s.data() + clampedPos, ins.data(), ins.size());
}

void SplitLogicalLines(
    keel::StringRef text,
    keel::Array<LogicalLine>& outLines,
    keel::usize* outMaxLineBytes,
    bool* outPureAsciiNoTabs
) {
    outLines.clear();
    keel::usize maxLineBytes = 0u;
    bool pureAsciiNoTabs = true;

    const char* p = text.data();
    const keel::usize n = text.size();
    keel::usize start = 0u;
    for (keel::usize i = 0u; i < n; ++i) {
        const keel::u8 b = keel::as<keel::u8>(p[i]);
        if (b == '\n') {
            keel::usize len = i - start;
            if (len > 0u && p[start + len - 1u] == '\r') {
                --len;
            }
            outLines.push_back({start, len});
            if (len > maxLineBytes) maxLineBytes = len;
            start = i + 1u;
        } else if (pureAsciiNoTabs) {
            if (b >= 0x80u || (b < 0x20u && !(b == '\r' && i + 1u < n && p[i + 1u] == '\n')) || b == 0x7Fu) {
                pureAsciiNoTabs = false;
            }
        }
    }

    // Final line after last '\n' (or entire empty text).
    keel::usize tailLen = n - start;
    if (tailLen > 0u && p[start + tailLen - 1u] == '\r') {
        --tailLen;
    }
    outLines.push_back({start, tailLen});
    if (tailLen > maxLineBytes) maxLineBytes = tailLen;

    if (outMaxLineBytes) *outMaxLineBytes = maxLineBytes;
    if (outPureAsciiNoTabs) *outPureAsciiNoTabs = pureAsciiNoTabs;
}

TextLineRange TextSourceView::LineByteRange(keel::usize line) const noexcept {
    if (doc) {
        return doc->LineByteRange(line);
    }
    if (flatLines.empty()) {
        return TextLineRange{0u, flatText.size(), flatText.size()};
    }
    const keel::usize clamped = keel::min(line, flatLines.size() - 1u);
    const LogicalLine& ll = flatLines[clamped];
    const keel::usize nextStart = (clamped + 1u < flatLines.size())
        ? flatLines[clamped + 1u].startOffset
        : flatText.size();
    return TextLineRange{ll.startOffset, ll.length, nextStart - ll.startOffset};
}

TextLineCol TextSourceView::OffsetToLineCol(keel::usize byteOffset) const noexcept {
    if (doc) {
        return doc->OffsetToLineCol(byteOffset);
    }
    const keel::usize clamped = keel::min(byteOffset, flatText.size());
    if (flatLines.empty()) {
        return TextLineCol{0u, clamped};
    }
    const keel::usize line = LowerBoundLogicalLineForOffset(flatLines, clamped);
    const LogicalLine& ll = flatLines[line];
    const keel::usize col = (clamped >= ll.startOffset)
        ? keel::min(clamped - ll.startOffset, ll.length)
        : 0u;
    return TextLineCol{line, col};
}

keel::usize TextSourceView::LineColToOffset(keel::usize line, keel::usize colBytes) const noexcept {
    if (doc) {
        return doc->LineColToOffset(line, colBytes);
    }
    const TextLineRange r = LineByteRange(line);
    return r.startByte + keel::min(colBytes, r.contentBytes);
}

char TextSourceView::ByteAt(keel::usize byteOffset) const noexcept {
    if (doc) {
        return doc->ByteAt(byteOffset);
    }
    return (byteOffset < flatText.size()) ? flatText[byteOffset] : '\0';
}

bool TextSourceView::RangeIsPureAsciiNoTabs(keel::usize byteOffset, keel::usize byteLen) const noexcept {
    if (byteLen == 0u) return true;
    if (doc) {
        return doc->RangeIsPureAsciiNoTabs(byteOffset, byteLen);
    }
    if (flatPureAsciiNoTabs) return true;
    const keel::usize clampedEnd = keel::min(flatText.size(), byteOffset + byteLen);
    for (keel::usize i = keel::min(byteOffset, clampedEnd); i < clampedEnd; ++i) {
        const keel::u8 b = keel::as<keel::u8>(flatText[i]);
        if (b >= 0x80u || (b < 0x20u && b != '\n' && b != '\r') || b == 0x7Fu) {
            return false;
        }
    }
    return true;
}

keel::usize TextSourceView::SnapToUtf8Boundary(keel::usize byteOffset) const noexcept {
    if (doc) {
        return doc->SnapToUtf8Boundary(byteOffset);
    }
    const keel::usize n = flatText.size();
    if (byteOffset >= n) return n;
    keel::usize steps = 0u;
    while (byteOffset > 0u && steps < 3u && !keel::utf8_is_lead_byte(keel::as<keel::u8>(flatText[byteOffset]))) {
        --byteOffset;
        ++steps;
    }
    return byteOffset;
}

keel::StringRef TextSourceView::ReadSliceRef(
    keel::usize byteOffset,
    keel::usize maxBytes,
    keel::String& scratch
) const {
    if (maxBytes == 0u) return {};
    if (doc) {
        doc->ReadSlice(byteOffset, maxBytes, scratch);
        return scratch.as_ref();
    }
    if (byteOffset >= flatText.size()) return {};
    return flatText.substr(byteOffset, keel::min(maxBytes, flatText.size() - byteOffset));
}

keel::StringRef TextSourceView::ReadLineSliceRef(
    keel::usize line,
    keel::usize colByteStart,
    keel::usize maxBytes,
    keel::String& scratch
) const {
    if (maxBytes == 0u) return {};
    const TextLineRange r = LineByteRange(line);
    if (colByteStart >= r.contentBytes) return {};
    const keel::usize len = keel::min(maxBytes, r.contentBytes - colByteStart);
    return ReadSliceRef(r.startByte + colByteStart, len, scratch);
}

void TextSourceView::ForEachChunkInRange(
    keel::usize byteOffset,
    keel::usize byteLen,
    keel::FunctionRef<bool(keel::StringRef)> visitor
) const {
    if (byteLen == 0u || !visitor) return;
    if (doc) {
        doc->ForEachChunkInRange(byteOffset, byteLen, visitor);
        return;
    }
    if (byteOffset >= flatText.size()) return;
    visitor(flatText.substr(byteOffset, keel::min(byteLen, flatText.size() - byteOffset)));
}

// =============================================================================
// Font Metrics & Segment Measurement
// =============================================================================

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
    if (lineSub.empty() || localX <= 0.0f) return 0u;
    keel::usize pos = 0u;
    keel::f32 prevW = 0.0f;
    while (pos < lineSub.size()) {
        const keel::usize nextPos = TextUtil::NextCharBoundary(lineSub, pos);
        const keel::f32 nextW = MeasureSegmentWidth(canvas, lineSub.substr(0u, nextPos), fontSize, style, tabSize);
        const keel::f32 mid = (prevW + nextW) * 0.5f;
        if (localX < mid) {
            return pos;
        }
        prevW = nextW;
        pos = nextPos;
    }
    return lineSub.size();
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
        const keel::f32 mono10 = canvas->MeasureTextStyled("0123456789", m.fontSize, FontStyle::Monospace).x;
        m.monoCharAdvance = (mono10 > 0.0f) ? (mono10 * 0.1f) : m.digitAdvance;
    } else {
        m.glyphBoxH = theme.GetLineHeight(m.fontSize);
        const keel::f32 factor = (style == FontStyle::Monospace) ? 0.60f : 0.56f;
        m.spaceAdvance = m.fontSize * factor;
        m.digitAdvance = m.fontSize * 0.60f;
        m.monoCharAdvance = m.fontSize * 0.60f;
    }

    m.lineHeight = keel::max(theme.GetLineHeight(m.fontSize), m.glyphBoxH * 1.14f);
    m.textOffsetY = (m.lineHeight - m.glyphBoxH) * 0.5f;
    m.tabAdvance = m.spaceAdvance * keel::as<keel::f32>(keel::clamp<keel::i32>(tabSize, 1, 16));
    return m;
}

// =============================================================================
// Folding Helpers
// =============================================================================

void SortFoldRegions(keel::MutArrayRef<FoldRegion> arr) noexcept {
    const keel::usize n = arr.size();
    for (keel::usize gap = n / 2u; gap > 0u; gap /= 2u) {
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

void ComputeFoldRegions(
    const TextSourceView& source,
    const SyntaxConfig* synCfg,
    keel::Array<FoldRegion>& outRegions,
    keel::usize startLine,
    keel::usize endLineExclusive,
    keel::ArrayRef<FoldRegion> preservedRegions
) {
    outRegions.clear();
    const keel::usize lineCount = source.LineCount();
    if (lineCount < 2u) return;

    const keel::usize scanStart = keel::min(startLine, lineCount);
    const keel::usize scanEnd = keel::min(endLineExclusive, lineCount);

    struct OpenEntry {
        keel::usize lineIdx{0};
        bool isKeyword{false};
    };
    keel::SmallArray<OpenEntry, 16> stack;

    const keel::StringRef lineComment = synCfg ? synCfg->lineComment : "//";
    const auto openKw = synCfg ? synCfg->foldOpenKeywords : keel::ArrayRef<keel::StringRef>{};
    const auto closeKw = synCfg ? synCfg->foldCloseKeywords : keel::ArrayRef<keel::StringRef>{};
    const bool foldCaseInsensitive = synCfg ? synCfg->caseInsensitiveKeywords : false;

    keel::String lineScratch;
    for (keel::usize i = scanStart; i < scanEnd; ++i) {
        const TextLineRange lr = source.LineByteRange(i);
        if (lr.contentBytes == 0u || lr.contentBytes > kMaxSyntaxLineBytes) continue;
        const keel::StringRef raw = source.ReadSliceRef(lr.startByte, lr.contentBytes, lineScratch);
        keel::usize p = 0u;
        while (p < raw.size() && (raw[p] == ' ' || raw[p] == '\t')) ++p;
        if (p >= raw.size()) continue;
        if (!lineComment.empty() && raw.substr(p).starts_with(lineComment)) continue;

        if (!openKw.empty() || !closeKw.empty()) {
            if (IsIdentStart(raw[p])) {
                keel::usize wEnd = p + 1u;
                while (wEnd < raw.size() && IsIdentChar(raw[wEnd])) ++wEnd;
                const keel::StringRef firstWord = raw.substr(p, wEnd - p);
                if (ContainsToken(openKw, firstWord, foldCaseInsensitive)) {
                    bool closedOnSameLine = false;
                    keel::usize scan = wEnd;
                    while (scan < raw.size()) {
                        if (!lineComment.empty() && raw.substr(scan).starts_with(lineComment)) break;
                        if (IsIdentStart(raw[scan])) {
                            keel::usize e2 = scan + 1u;
                            while (e2 < raw.size() && IsIdentChar(raw[e2])) ++e2;
                            if (ContainsToken(closeKw, raw.substr(scan, e2 - scan), foldCaseInsensitive)) {
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
                } else if (ContainsToken(closeKw, firstWord, foldCaseInsensitive)) {
                    for (keel::usize sIdx = stack.size(); sIdx > 0u; --sIdx) {
                        const keel::usize k = sIdx - 1u;
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

        bool inStr = false;
        char quoteC = '\0';
        for (keel::usize cIdx = p; cIdx < raw.size(); ++cIdx) {
            const char c = raw[cIdx];
            if (!inStr && !lineComment.empty() && raw.substr(cIdx).starts_with(lineComment)) {
                break;
            }
            if (c == '"' || c == '\'') {
                if (!inStr) {
                    if (c == '\'' && synCfg && synCfg->transposeApostrophe && cIdx > 0u) {
                        const char prev = raw[cIdx - 1u];
                        if (IsIdentChar(prev) || prev == ')' || prev == ']' || prev == '}' || prev == '.') {
                            continue;
                        }
                    }
                    inStr = true;
                    quoteC = c;
                } else if (quoteC == c && (cIdx == 0u || raw[cIdx - 1u] != '\\')) {
                    inStr = false;
                }
                continue;
            }
            if (inStr) continue;

            if (c == '{') {
                stack.push_back({i, false});
            } else if (c == '}') {
                for (keel::usize sIdx = stack.size(); sIdx > 0u; --sIdx) {
                    const keel::usize k = sIdx - 1u;
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

    // Merge any preserved active folded regions located outside the current viewport window
    for (const FoldRegion& pr : preservedRegions) {
        if (pr.startLine < lineCount && pr.endLine < lineCount && pr.endLine > pr.startLine) {
            bool duplicate = false;
            for (const FoldRegion& existing : outRegions) {
                if (existing.startLine == pr.startLine) {
                    duplicate = true;
                    break;
                }
            }
            if (!duplicate) {
                outRegions.push_back(pr);
            }
        }
    }

    SortFoldRegions(outRegions);
}

const FoldRegion* FindFoldRegionStartingAt(keel::ArrayRef<FoldRegion> regions, keel::usize lineIdx) noexcept {
    if (regions.empty()) return nullptr;
    keel::usize lo = 0u;
    keel::usize hi = regions.size();
    while (lo < hi) {
        const keel::usize mid = lo + (hi - lo) / 2u;
        if (regions[mid].startLine < lineIdx) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    if (lo < regions.size() && regions[lo].startLine == lineIdx) {
        return &regions[lo];
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

// =============================================================================
// 2D TextViewProjection (Vertical + Horizontal)
// =============================================================================

void TextViewProjection::BuildHiddenSpans(
    keel::ArrayRef<FoldRegion> foldRegions,
    keel::ArrayRef<keel::usize> foldedLines
) noexcept {
    hiddenSpans.clear();
    if (foldedLines.empty() || foldRegions.empty()) return;

    keel::SmallArray<HiddenFoldSpan, 16> raw;
    for (keel::usize fl : foldedLines) {
        if (const FoldRegion* r = FindFoldRegionStartingAt(foldRegions, fl)) {
            if (r->endLine > r->startLine) {
                raw.push_back(HiddenFoldSpan{r->startLine + 1u, r->endLine, 0u});
            }
        }
    }
    if (raw.empty()) return;

    // Insertion sort by hideStart (F is small, typically <= 64).
    for (keel::usize i = 1u; i < raw.size(); ++i) {
        const HiddenFoldSpan key = raw[i];
        keel::usize j = i;
        while (j > 0u && raw[j - 1u].hideStart > key.hideStart) {
            raw[j] = raw[j - 1u];
            --j;
        }
        raw[j] = key;
    }

    // Merge overlapping / adjacent hidden ranges and accumulate hiddenBefore prefix sums.
    keel::usize accHidden = 0u;
    for (const HiddenFoldSpan& sp : raw) {
        if (!hiddenSpans.empty() && sp.hideStart <= hiddenSpans.back().hideEnd + 1u) {
            if (sp.hideEnd > hiddenSpans.back().hideEnd) {
                accHidden += (sp.hideEnd - hiddenSpans.back().hideEnd);
                hiddenSpans.back().hideEnd = sp.hideEnd;
            }
        } else {
            hiddenSpans.push_back(HiddenFoldSpan{sp.hideStart, sp.hideEnd, accHidden});
            accHidden += (sp.hideEnd - sp.hideStart + 1u);
        }
    }
}

bool TextViewProjection::IsLineHidden(keel::usize logicalLine) const noexcept {
    if (hiddenSpans.empty()) return false;
    keel::usize lo = 0u;
    keel::usize hi = hiddenSpans.size();
    while (lo < hi) {
        const keel::usize mid = lo + (hi - lo) / 2u;
        if (hiddenSpans[mid].hideEnd < logicalLine) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return (lo < hiddenSpans.size() &&
            logicalLine >= hiddenSpans[lo].hideStart &&
            logicalLine <= hiddenSpans[lo].hideEnd);
}

keel::usize TextViewProjection::LineWrapRowCount(keel::usize logicalLine) const noexcept {
    if (IsLineHidden(logicalLine)) return 0u;
    const keel::usize cols = keel::max<keel::usize>(1u, wrapCols);
    const TextLineRange r = source.LineByteRange(logicalLine);
    if (r.contentBytes <= cols) return 1u;
    return (r.contentBytes + cols - 1u) / cols;
}

VisualLine TextViewProjection::VisualRowAt(keel::usize visualRowIdx) const noexcept {
    if (totalVisualRows == 0u) {
        return VisualLine{0u, 0u, 0u, 0u, true};
    }
    if (visualRowIdx >= totalVisualRows) {
        visualRowIdx = totalVisualRows - 1u;
    }

    if (mode == Mode::Identity) {
        const TextLineRange r = source.LineByteRange(visualRowIdx);
        return VisualLine{visualRowIdx, r.startByte, 0u, r.contentBytes, true};
    }

    if (mode == Mode::Folded) {
        keel::usize logicalLine = visualRowIdx;
        if (!hiddenSpans.empty()) {
            // Find last span where (hideStart - hiddenBefore) <= visualRowIdx.
            keel::usize lo = 0u;
            keel::usize hi = hiddenSpans.size();
            while (lo < hi) {
                const keel::usize mid = lo + (hi - lo) / 2u;
                const keel::usize visBefore = hiddenSpans[mid].hideStart - hiddenSpans[mid].hiddenBefore;
                if (visBefore <= visualRowIdx) {
                    lo = mid + 1u;
                } else {
                    hi = mid;
                }
            }
            if (lo > 0u) {
                const HiddenFoldSpan& sp = hiddenSpans[lo - 1u];
                const keel::usize totalHiddenUpTo = sp.hiddenBefore + (sp.hideEnd - sp.hideStart + 1u);
                logicalLine = visualRowIdx + totalHiddenUpTo;
            }
        }
        const TextLineRange r = source.LineByteRange(logicalLine);
        return VisualLine{logicalLine, r.startByte, 0u, r.contentBytes, true};
    }

    if (mode == Mode::ExactWrap) {
        if (exactVisualLines.empty()) {
            return VisualLine{0u, 0u, 0u, 0u, true};
        }
        return exactVisualLines[keel::min(visualRowIdx, exactVisualLines.size() - 1u)];
    }

    // Analytic wrap modes (AnalyticSmallWrap or AnalyticChunkWrap).
    keel::usize logicalLine = 0u;
    keel::usize subIdx = 0u;
    if (mode == Mode::AnalyticSmallWrap && smallLinePrefix.size() >= 2u) {
        logicalLine = LowerBoundPrefixUsize(smallLinePrefix, visualRowIdx);
        subIdx = visualRowIdx - smallLinePrefix[logicalLine];
    } else if (mode == Mode::AnalyticChunkWrap && chunkPrefix.size() >= 2u) {
        const keel::usize chunkIdx = LowerBoundPrefixUsize(chunkPrefix, visualRowIdx);
        keel::usize acc = chunkPrefix[chunkIdx];
        const keel::usize lineCount = source.LineCount();
        const keel::usize lineStart = chunkIdx * kAnalyticWrapChunkLines;
        const keel::usize lineEnd = keel::min(lineCount, lineStart + kAnalyticWrapChunkLines);
        logicalLine = lineStart;
        for (keel::usize li = lineStart; li < lineEnd; ++li) {
            const keel::usize rows = LineWrapRowCount(li);
            if (visualRowIdx < acc + rows) {
                logicalLine = li;
                subIdx = visualRowIdx - acc;
                break;
            }
            acc += rows;
            logicalLine = li;
        }
    }

    const TextLineRange r = source.LineByteRange(logicalLine);
    const keel::usize cols = keel::max<keel::usize>(1u, wrapCols);
    keel::usize colStart = keel::min(subIdx * cols, r.contentBytes);
    keel::usize colEnd = keel::min(colStart + cols, r.contentBytes);
    if (!source.IsPureAsciiNoTabs()) {
        if (colStart > 0u && colStart < r.contentBytes) {
            colStart = source.SnapToUtf8Boundary(r.startByte + colStart) - r.startByte;
        }
        if (colEnd > colStart && colEnd < r.contentBytes) {
            const keel::usize snapped = source.SnapToUtf8Boundary(r.startByte + colEnd) - r.startByte;
            if (snapped > colStart) {
                colEnd = snapped;
            }
        }
    }
    return VisualLine{
        logicalLine,
        r.startByte + colStart,
        colStart,
        colEnd - colStart,
        subIdx == 0u
    };
}

keel::usize TextViewProjection::LogicalLineToVisualRow(keel::usize logicalLine) const noexcept {
    const keel::usize lineCount = source.LineCount();
    if (lineCount == 0u || totalVisualRows == 0u) return 0u;
    if (logicalLine >= lineCount) logicalLine = lineCount - 1u;

    if (mode == Mode::Identity) {
        return logicalLine;
    }

    if (mode == Mode::Folded) {
        if (hiddenSpans.empty()) return logicalLine;
        keel::usize lo = 0u;
        keel::usize hi = hiddenSpans.size();
        while (lo < hi) {
            const keel::usize mid = lo + (hi - lo) / 2u;
            if (hiddenSpans[mid].hideStart <= logicalLine) {
                lo = mid + 1u;
            } else {
                hi = mid;
            }
        }
        if (lo == 0u) return logicalLine;
        const HiddenFoldSpan& sp = hiddenSpans[lo - 1u];
        if (logicalLine <= sp.hideEnd) {
            return (sp.hideStart - 1u) - sp.hiddenBefore;
        }
        const keel::usize hiddenUpTo = sp.hiddenBefore + (sp.hideEnd - sp.hideStart + 1u);
        return logicalLine - hiddenUpTo;
    }

    if (mode == Mode::ExactWrap) {
        if (exactVisualLines.empty()) return 0u;
        keel::usize lo = 0u;
        keel::usize hi = exactVisualLines.size();
        while (lo < hi) {
            const keel::usize mid = lo + (hi - lo) / 2u;
            if (exactVisualLines[mid].logicalLine < logicalLine) {
                lo = mid + 1u;
            } else {
                hi = mid;
            }
        }
        return keel::min(lo, exactVisualLines.size() - 1u);
    }

    if (mode == Mode::AnalyticSmallWrap && logicalLine < smallLinePrefix.size()) {
        return keel::min(smallLinePrefix[logicalLine], totalVisualRows - 1u);
    }

    if (mode == Mode::AnalyticChunkWrap && !chunkPrefix.empty()) {
        const keel::usize chunkIdx = keel::min(logicalLine / kAnalyticWrapChunkLines, chunkPrefix.size() - 2u);
        keel::usize acc = chunkPrefix[chunkIdx];
        const keel::usize startLi = chunkIdx * kAnalyticWrapChunkLines;
        for (keel::usize li = startLi; li < logicalLine; ++li) {
            acc += LineWrapRowCount(li);
        }
        return keel::min(acc, totalVisualRows - 1u);
    }

    return 0u;
}

keel::usize TextViewProjection::OffsetToVisualRow(keel::usize byteOffset) const noexcept {
    if (totalVisualRows == 0u) return 0u;
    const keel::usize clamped = keel::min(byteOffset, source.ByteLength());

    if (mode == Mode::Identity) {
        return source.OffsetToLineCol(clamped).line;
    }

    if (mode == Mode::Folded) {
        return LogicalLineToVisualRow(source.OffsetToLineCol(clamped).line);
    }

    if (mode == Mode::ExactWrap) {
        if (exactVisualLines.empty()) return 0u;
        // Binary search by startOffset.
        keel::usize lo = 0u;
        keel::usize hi = exactVisualLines.size();
        while (lo + 1u < hi) {
            const keel::usize mid = lo + (hi - lo) / 2u;
            if (exactVisualLines[mid].startOffset <= clamped) {
                lo = mid;
            } else {
                hi = mid;
            }
        }
        // Prefer next sub-line of same logical line when cursor sits at exact wrap boundary.
        if (lo + 1u < exactVisualLines.size() &&
            exactVisualLines[lo + 1u].logicalLine == exactVisualLines[lo].logicalLine &&
            clamped == exactVisualLines[lo].startOffset + exactVisualLines[lo].length) {
            return lo + 1u;
        }
        return lo;
    }

    // Analytic wrap modes.
    const TextLineCol lc = source.OffsetToLineCol(clamped);
    const keel::usize baseRow = LogicalLineToVisualRow(lc.line);
    if (IsLineHidden(lc.line)) return baseRow;
    const keel::usize rows = LineWrapRowCount(lc.line);
    const keel::usize cols = keel::max<keel::usize>(1u, wrapCols);
    const keel::usize sub = (rows > 1u) ? keel::min(lc.colBytes / cols, rows - 1u) : 0u;
    return keel::min(baseRow + sub, totalVisualRows - 1u);
}

void BuildVisualLines(
    const TextSourceView& source,
    bool wordWrap,
    keel::f32 wrapWidth,
    keel::f32 fontSize,
    FontStyle style,
    ICanvas* canvas,
    keel::Array<VisualLine>& outVisual,
    keel::ArrayRef<FoldRegion> foldRegions,
    keel::ArrayRef<keel::usize> foldedLines,
    keel::u8 tabSize
) {
    outVisual.clear();
    const keel::usize lineCount = source.LineCount();
    outVisual.reserve(lineCount);

    TextViewProjection foldHelper{};
    foldHelper.BuildHiddenSpans(foldRegions, foldedLines);

    keel::String lineScratch;
    for (keel::usize i = 0u; i < lineCount; ++i) {
        if (foldHelper.IsLineHidden(i)) {
            continue;
        }
        const TextLineRange lr = source.LineByteRange(i);
        if (!wordWrap || wrapWidth <= 24.0f || lr.contentBytes == 0u) {
            outVisual.push_back({i, lr.startByte, 0u, lr.contentBytes, true});
            continue;
        }

        const keel::StringRef lineStr = source.ReadSliceRef(lr.startByte, lr.contentBytes, lineScratch);
        const keel::f32 fullW = MeasureSegmentWidth(canvas, lineStr, fontSize, style, tabSize);
        if (fullW <= wrapWidth + 0.5f) {
            outVisual.push_back({i, lr.startByte, 0u, lr.contentBytes, true});
            continue;
        }

        keel::usize subStart = 0u;
        bool first = true;
        while (subStart < lineStr.size()) {
            // Binary search maximum fitting byte count in [subStart, lineStr.size()] -> O(log L) instead of O(L^2)!
            keel::usize lo = TextUtil::NextCharBoundary(lineStr, subStart);
            keel::usize hi = lineStr.size();
            while (lo < hi) {
                keel::usize mid = lo + (hi - lo + 1u) / 2u;
                while (mid > lo && !keel::utf8_is_lead_byte(keel::as<keel::u8>(lineStr[mid]))) {
                    --mid;
                }
                if (mid <= lo) {
                    const keel::usize nextLo = TextUtil::NextCharBoundary(lineStr, lo);
                    if (nextLo <= hi && MeasureSegmentWidth(canvas, lineStr.substr(subStart, nextLo - subStart), fontSize, style, tabSize) <= wrapWidth) {
                        lo = nextLo;
                    }
                    break;
                }
                const keel::f32 w = MeasureSegmentWidth(canvas, lineStr.substr(subStart, mid - subStart), fontSize, style, tabSize);
                if (w <= wrapWidth) {
                    lo = mid;
                } else {
                    hi = mid - 1u;
                    while (hi > lo && !keel::utf8_is_lead_byte(keel::as<keel::u8>(lineStr[hi]))) {
                        --hi;
                    }
                }
            }

            keel::usize cur = lo;
            keel::usize lastBreak = subStart;
            if (cur < lineStr.size()) {
                for (keel::usize scan = cur; scan > subStart; --scan) {
                    const char ch = lineStr[scan - 1u];
                    if (ch == ' ' || ch == '\t' || ch == ',' || ch == ';') {
                        lastBreak = scan;
                        break;
                    }
                }
            }
            const keel::usize takeEnd = (lastBreak > subStart && cur < lineStr.size()) ? lastBreak : cur;
            const keel::usize subLen = keel::max<keel::usize>(1u, takeEnd - subStart);
            outVisual.push_back({i, lr.startByte + subStart, subStart, subLen, first});
            first = false;
            subStart += subLen;
        }
    }
    if (outVisual.empty()) {
        outVisual.push_back({0u, 0u, 0u, 0u, true});
    }
}

void PrepareTextViewProjection(
    const TextSourceView& source,
    bool wordWrap,
    keel::f32 wrapWidth,
    const EditorFontMetrics& metrics,
    FontStyle textStyle,
    ICanvas* canvas,
    keel::ArrayRef<FoldRegion> foldRegions,
    keel::ArrayRef<keel::usize> foldedLines,
    keel::u8 tabSize,
    keel::u64 docCacheKey,
    TextAreaState* state,
    TextViewProjection& outProj,
    keel::Array<VisualLine>* fallbackExactBuf,
    keel::Array<keel::usize>* fallbackSmallPrefix,
    keel::Array<keel::usize>* fallbackChunkPrefix
) {
    outProj = TextViewProjection{};
    outProj.source = source;
    outProj.BuildHiddenSpans(foldRegions, foldedLines);

    const keel::usize lineCount = source.LineCount();
    const keel::usize maxLineBytes = source.MaxLineByteLength();

    // 1. Non-wrapping modes: Identity (0 bytes) or Folded (O(log F), 0 bytes per line).
    if (!wordWrap || wrapWidth <= 24.0f) {
        if (outProj.hiddenSpans.empty()) {
            outProj.mode = TextViewProjection::Mode::Identity;
            outProj.totalVisualRows = lineCount;
        } else {
            outProj.mode = TextViewProjection::Mode::Folded;
            const HiddenFoldSpan& last = outProj.hiddenSpans.back();
            const keel::usize totalHidden = last.hiddenBefore + (last.hideEnd - last.hideStart + 1u);
            outProj.totalVisualRows = (lineCount > totalHidden) ? (lineCount - totalHidden) : 1u;
        }
        return;
    }

    const keel::u64 foldedHash = HashFoldedLines(foldedLines);

    // 2. Normal documents with wordWrap -> Exact word-boundary wrap (cached in state).
    if (lineCount <= kExactWrapMaxLines && maxLineBytes <= kExactWrapMaxLineBytes) {
        outProj.mode = TextViewProjection::Mode::ExactWrap;
        const keel::i32 wrapWQuant = keel::round_i32(wrapWidth * 2.0f);
        const keel::u64 exactKey = detail::HashArgs(
            docCacheKey, wrapWQuant, keel::round_i32(metrics.fontSize * 10.0f),
            keel::as<keel::u8>(textStyle), tabSize, foldedHash, foldRegions.size()
        );
        if (state) {
            if (state->cachedExactWrapKey != exactKey || state->scratchVisualLines.empty()) {
                BuildVisualLines(source, true, wrapWidth, metrics.fontSize, textStyle, canvas,
                                 state->scratchVisualLines, foldRegions, foldedLines, tabSize);
                state->cachedExactWrapKey = exactKey;
            }
            outProj.exactVisualLines = state->scratchVisualLines;
            outProj.totalVisualRows = state->scratchVisualLines.size();
        } else if (fallbackExactBuf) {
            BuildVisualLines(source, true, wrapWidth, metrics.fontSize, textStyle, canvas,
                             *fallbackExactBuf, foldRegions, foldedLines, tabSize);
            outProj.exactVisualLines = *fallbackExactBuf;
            outProj.totalVisualRows = fallbackExactBuf->size();
        }
        return;
    }

    // 3. Scalable analytic wrap (1M+ lines OR 10MB single lines with wordWrap == true).
    const keel::f32 charAdv = (textStyle == FontStyle::Monospace)
        ? keel::max(1.0f, metrics.monoCharAdvance)
        : keel::max(1.0f, metrics.spaceAdvance * 1.15f);
    const keel::usize wrapCols = keel::max<keel::usize>(1u, keel::as<keel::usize>(keel::floor(wrapWidth / charAdv)));
    outProj.wrapCols = wrapCols;

    // O(1) SumTree monoid fast-path: if even the longest line in the entire document fits within wrapCols,
    // no line can wrap -> Identity or Folded mode in 0 bytes and 0 ms!
    if (maxLineBytes <= wrapCols) {
        if (outProj.hiddenSpans.empty()) {
            outProj.mode = TextViewProjection::Mode::Identity;
            outProj.totalVisualRows = lineCount;
        } else {
            outProj.mode = TextViewProjection::Mode::Folded;
            const HiddenFoldSpan& last = outProj.hiddenSpans.back();
            const keel::usize totalHidden = last.hiddenBefore + (last.hideEnd - last.hideStart + 1u);
            outProj.totalVisualRows = (lineCount > totalHidden) ? (lineCount - totalHidden) : 1u;
        }
        return;
    }

    const keel::u64 analyticKey = detail::HashArgs(docCacheKey, wrapCols, foldedHash, foldRegions.size());

    if (lineCount <= kExactWrapMaxLines) {
        outProj.mode = TextViewProjection::Mode::AnalyticSmallWrap;
        keel::Array<keel::usize>* buf = state ? &state->cachedWrapSmallLinePrefix : fallbackSmallPrefix;
        if (buf) {
            const bool needBuild = !state || state->cachedAnalyticWrapKey != analyticKey || buf->size() != lineCount + 1u;
            if (needBuild) {
                buf->clear();
                buf->reserve(lineCount + 1u);
                keel::usize acc = 0u;
                for (keel::usize li = 0u; li < lineCount; ++li) {
                    buf->push_back(acc);
                    acc += outProj.LineWrapRowCount(li);
                }
                buf->push_back(acc);
                if (state) {
                    state->cachedAnalyticWrapKey = analyticKey;
                    state->cachedAnalyticWrapCols = wrapCols;
                    state->cachedAnalyticTotalRows = keel::max<keel::usize>(1u, acc);
                    state->cachedWrapChunkPrefix.clear();
                }
            }
            outProj.smallLinePrefix = *buf;
            outProj.totalVisualRows = keel::max<keel::usize>(1u, buf->back());
        }
    } else {
        outProj.mode = TextViewProjection::Mode::AnalyticChunkWrap;
        keel::Array<keel::usize>* buf = state ? &state->cachedWrapChunkPrefix : fallbackChunkPrefix;
        if (buf) {
            const keel::usize chunkCount = (lineCount + kAnalyticWrapChunkLines - 1u) / kAnalyticWrapChunkLines;
            const bool needBuild = !state || state->cachedAnalyticWrapKey != analyticKey || buf->size() != chunkCount + 1u;
            if (needBuild) {
                buf->clear();
                buf->reserve(chunkCount + 1u);
                // Single linear streaming pass over piece buffers (~5 ms for 1M lines, 10x faster than 1M tree descents!).
                keel::usize acc = 0u;
                keel::usize curLine = 0u;
                keel::usize curLineLen = 0u;
                keel::usize linesInChunk = 0u;
                bool prevWasCr = false;
                buf->push_back(0u);

                auto finishLine = [&]() {
                    const keel::usize contentLen = prevWasCr && curLineLen > 0u ? (curLineLen - 1u) : curLineLen;
                    if (!outProj.IsLineHidden(curLine)) {
                        const keel::usize rows = (contentLen <= wrapCols) ? 1u : ((contentLen + wrapCols - 1u) / wrapCols);
                        acc += rows;
                    }
                    ++curLine;
                    ++linesInChunk;
                    if (linesInChunk == kAnalyticWrapChunkLines) {
                        buf->push_back(acc);
                        linesInChunk = 0u;
                    }
                    curLineLen = 0u;
                    prevWasCr = false;
                };

                source.ForEachChunkInRange(0u, source.ByteLength(), [&](keel::StringRef chunk) -> bool {
                    const char* p = chunk.data();
                    const keel::usize n = chunk.size();
                    keel::usize i = 0u;
                    while (i < n) {
                        const keel::usize nl = chunk.find('\n', i);
                        if (nl == keel::StringRef::npos) {
                            curLineLen += (n - i);
                            prevWasCr = (p[n - 1u] == '\r');
                            break;
                        }
                        if (nl > i) {
                            curLineLen += (nl - i);
                            prevWasCr = (p[nl - 1u] == '\r');
                        }
                        finishLine();
                        i = nl + 1u;
                    }
                    return true;
                });

                // Final line after last '\n'
                if (curLine < lineCount) {
                    if (!outProj.IsLineHidden(curLine)) {
                        const keel::usize rows = (curLineLen <= wrapCols) ? 1u : ((curLineLen + wrapCols - 1u) / wrapCols);
                        acc += rows;
                    }
                    ++curLine;
                    ++linesInChunk;
                }
                if (linesInChunk > 0u || buf->size() < chunkCount + 1u) {
                    buf->push_back(acc);
                }

                if (state) {
                    state->cachedAnalyticWrapKey = analyticKey;
                    state->cachedAnalyticWrapCols = wrapCols;
                    state->cachedAnalyticTotalRows = keel::max<keel::usize>(1u, acc);
                    state->cachedWrapSmallLinePrefix.clear();
                }
            }
            outProj.chunkPrefix = *buf;
            outProj.totalVisualRows = keel::max<keel::usize>(1u, buf->back());
        }
    }
}

void AttachRenderProjection(
    const TextSourceView& source,
    const TextAreaOptions& opts,
    keel::f32 wrapWidth,
    const EditorFontMetrics& metrics,
    FontStyle textStyle,
    ICanvas* canvas,
    keel::ArrayRef<FoldRegion> foldRegions,
    keel::ArrayRef<keel::usize> foldedLines,
    const TextAreaData* extData,
    TextViewProjection& outProj,
    keel::Array<VisualLine>& fallbackExactBuf,
    keel::Array<keel::usize>& fallbackSmallPrefix,
    keel::Array<keel::usize>& fallbackChunkPrefix
) {
    if (extData) {
        if (!opts.wordWrap || wrapWidth <= 24.0f) {
            outProj = TextViewProjection{};
            outProj.source = source;
            outProj.BuildHiddenSpans(foldRegions, foldedLines);
            if (outProj.hiddenSpans.empty()) {
                outProj.mode = TextViewProjection::Mode::Identity;
                outProj.totalVisualRows = source.LineCount();
            } else {
                outProj.mode = TextViewProjection::Mode::Folded;
                const HiddenFoldSpan& last = outProj.hiddenSpans.back();
                const keel::usize totalHidden = last.hiddenBefore + (last.hideEnd - last.hideStart + 1u);
                outProj.totalVisualRows = (source.LineCount() > totalHidden) ? (source.LineCount() - totalHidden) : 1u;
            }
            return;
        }
        if (!extData->precomputedVisualLines.empty()) {
            outProj = TextViewProjection{};
            outProj.source = source;
            outProj.BuildHiddenSpans(foldRegions, foldedLines);
            outProj.mode = TextViewProjection::Mode::ExactWrap;
            outProj.exactVisualLines = extData->precomputedVisualLines;
            outProj.totalVisualRows = extData->precomputedVisualLines.size();
            return;
        }
        if (!extData->precomputedWrapSmallLinePrefix.empty() && extData->precomputedWrapCols > 0u) {
            outProj = TextViewProjection{};
            outProj.source = source;
            outProj.BuildHiddenSpans(foldRegions, foldedLines);
            outProj.mode = TextViewProjection::Mode::AnalyticSmallWrap;
            outProj.wrapCols = extData->precomputedWrapCols;
            outProj.smallLinePrefix = extData->precomputedWrapSmallLinePrefix;
            outProj.totalVisualRows = keel::max<keel::usize>(1u, extData->precomputedTotalVisualRows);
            return;
        }
        if (!extData->precomputedWrapChunkPrefix.empty() && extData->precomputedWrapCols > 0u) {
            outProj = TextViewProjection{};
            outProj.source = source;
            outProj.BuildHiddenSpans(foldRegions, foldedLines);
            outProj.mode = TextViewProjection::Mode::AnalyticChunkWrap;
            outProj.wrapCols = extData->precomputedWrapCols;
            outProj.chunkPrefix = extData->precomputedWrapChunkPrefix;
            outProj.totalVisualRows = keel::max<keel::usize>(1u, extData->precomputedTotalVisualRows);
            return;
        }
    }

    PrepareTextViewProjection(
        source, opts.wordWrap, wrapWidth, metrics, textStyle, canvas,
        foldRegions, foldedLines, opts.tabSize, 0u, nullptr,
        outProj, &fallbackExactBuf, &fallbackSmallPrefix, &fallbackChunkPrefix
    );
}

// =============================================================================
// 2D Horizontal Windowing & Fast Column Measurement / Hit-Testing
// =============================================================================

HorizontalWindow ComputeHorizontalWindow(
    const TextSourceView& source,
    const VisualLine& vl,
    const EditorFontMetrics& metrics,
    bool monospace,
    keel::f64 scrollX,
    keel::f32 viewportW
) noexcept {
    HorizontalWindow w{};
    if (vl.length == 0u) {
        return w;
    }

    // Short/normal lines: render the entire visual sub-line directly for 100% kerning/tab fidelity.
    if (vl.length <= kLongLineFastThresholdBytes) {
        w.colByteStart = 0u;
        w.colByteLen = vl.length;
        w.startXOffset = 0.0;
        w.fastMonospaceAscii = false;
        return w;
    }

    const keel::f64 charAdv = keel::max(1.0, keel::as<keel::f64>(monospace ? metrics.monoCharAdvance : metrics.spaceAdvance));
    const bool pureAscii = monospace && source.RangeIsPureAsciiNoTabs(vl.startOffset, vl.length);

    if (pureAscii) {
        const keel::f64 safeScroll = keel::max(0.0, scrollX);
        const keel::usize firstCol = (safeScroll > charAdv * 2.0)
            ? keel::as<keel::usize>(keel::floor(safeScroll / charAdv) - 2.0)
            : 0u;
        const keel::usize visCols = keel::as<keel::usize>(keel::ceil(keel::as<keel::f64>(keel::max(viewportW, 64.0f)) / charAdv)) + 8u;
        const keel::usize startCol = keel::min(firstCol, vl.length);
        const keel::usize len = keel::min(visCols, vl.length - startCol);
        w.colByteStart = startCol;
        w.colByteLen = len;
        w.startXOffset = keel::as<keel::f64>(startCol) * charAdv;
        w.fastMonospaceAscii = true;
        return w;
    }

    // Huge line (> 1 KB) containing tabs or UTF-8 multibyte: bound window cleanly so we never copy 10 MB!
    const keel::f64 safeScroll = keel::max(0.0, scrollX);
    const keel::usize estCol = (safeScroll > charAdv * 4.0)
        ? keel::as<keel::usize>(keel::floor(safeScroll / charAdv) - 4.0)
        : 0u;
    keel::usize startByte = keel::min(estCol, vl.length);
    if (startByte > 0u && startByte < vl.length) {
        startByte = source.SnapToUtf8Boundary(vl.startOffset + startByte) - vl.startOffset;
    }
    const keel::usize maxWinBytes = keel::max<keel::usize>(
        2048u,
        keel::as<keel::usize>(keel::ceil(keel::as<keel::f64>(keel::max(viewportW, 64.0f)) / charAdv)) * 4u
    );
    keel::usize endByte = keel::min(vl.length, startByte + maxWinBytes);
    if (endByte > startByte && endByte < vl.length) {
        const keel::usize snapped = source.SnapToUtf8Boundary(vl.startOffset + endByte) - vl.startOffset;
        if (snapped > startByte) endByte = snapped;
    }
    w.colByteStart = startByte;
    w.colByteLen = endByte - startByte;
    w.startXOffset = keel::as<keel::f64>(startByte) * charAdv;
    w.fastMonospaceAscii = false;
    return w;
}

keel::f64 MeasureColumnXInVisualRow(
    const TextSourceView& source,
    const VisualLine& vl,
    keel::usize colInSub,
    const EditorFontMetrics& metrics,
    FontStyle style,
    ICanvas* canvas,
    keel::u8 tabSize,
    keel::String& scratch
) noexcept {
    const keel::usize clampedCol = keel::min(colInSub, vl.length);
    if (clampedCol == 0u) return 0.0;

    if (vl.length > kLongLineFastThresholdBytes && style == FontStyle::Monospace) {
        if (source.RangeIsPureAsciiNoTabs(vl.startOffset, vl.length)) {
            return keel::as<keel::f64>(clampedCol) * keel::as<keel::f64>(metrics.monoCharAdvance);
        }
        // Bounded approximation for non-ASCII > 1 KB lines when far into the line.
        if (clampedCol > 4096u) {
            return keel::as<keel::f64>(clampedCol) * keel::as<keel::f64>(metrics.monoCharAdvance);
        }
    }

    const keel::StringRef prefix = source.ReadSliceRef(vl.startOffset, clampedCol, scratch);
    return keel::as<keel::f64>(MeasureSegmentWidth(canvas, prefix, metrics.fontSize, style, tabSize));
}

keel::usize HitTestVisualRowX(
    const TextSourceView& source,
    const VisualLine& vl,
    keel::f64 localX,
    const EditorFontMetrics& metrics,
    FontStyle style,
    ICanvas* canvas,
    keel::u8 tabSize,
    keel::String& scratch
) noexcept {
    if (vl.length == 0u || localX <= 0.0) return 0u;

    if (vl.length > kLongLineFastThresholdBytes) {
        const keel::f64 charAdv = keel::max(1.0, keel::as<keel::f64>(style == FontStyle::Monospace ? metrics.monoCharAdvance : metrics.spaceAdvance));
        if (style == FontStyle::Monospace && source.RangeIsPureAsciiNoTabs(vl.startOffset, vl.length)) {
            const keel::usize col = keel::as<keel::usize>(keel::floor((localX / charAdv) + 0.5));
            return keel::min(col, vl.length);
        }
        const HorizontalWindow win = ComputeHorizontalWindow(
            source, vl, metrics, style == FontStyle::Monospace, keel::max(0.0, localX - 256.0), 512.0f
        );
        const keel::StringRef sub = source.ReadSliceRef(vl.startOffset + win.colByteStart, win.colByteLen, scratch);
        const keel::f32 relLocalX = keel::as<keel::f32>(keel::max(0.0, localX - win.startXOffset));
        return keel::min(vl.length, win.colByteStart + HitTestColumnInLine(canvas, sub, relLocalX, metrics.fontSize, style, tabSize));
    }

    const keel::StringRef sub = source.ReadSliceRef(vl.startOffset, vl.length, scratch);
    return HitTestColumnInLine(canvas, sub, keel::as<keel::f32>(localX), metrics.fontSize, style, tabSize);
}

// =============================================================================
// Box & Editor Layout Measurement (O(1) / O(viewport))
// =============================================================================

Vec2 MeasureEditorBoxFromStats(
    keel::usize lineCount,
    keel::usize maxLineBytes,
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
    const EditorFontMetrics fm = ResolveEditorFontMetrics(canvas, theme, baseFontSize, effZoom, textStyle, opts.tabSize);
    const keel::f32 fontSize = fm.fontSize;
    const keel::f32 lineHeight = fm.lineHeight;
    const keel::f32 padY = keel::max(8.0f, cm.paddingY);
    const keel::f32 padX = keel::max(10.0f, cm.paddingX);
    const keel::f32 statusH = opts.showStatusBar ? 22.0f : 0.0f;

    const keel::usize safeLines = keel::max<keel::usize>(1u, lineCount);
    keel::f32 boxH = 0.0f;
    if (opts.height.IsFixed()) {
        boxH = opts.height.Dp();
    } else if (opts.autoHeight) {
        // Clamp autoHeight on absurdly huge files to avoid float overflow in layout solver.
        const keel::f64 rawH = keel::as<keel::f64>(safeLines) * keel::as<keel::f64>(lineHeight) + padY * 2.0 + statusH;
        boxH = keel::as<keel::f32>(keel::min(rawH, 1000000.0));
    } else if (opts.rows.has_value()) {
        const keel::u32 r = keel::max(1u, *opts.rows);
        boxH = keel::as<keel::f32>(r) * lineHeight + padY * 2.0f + statusH;
    } else {
        boxH = 4.0f * lineHeight + padY * 2.0f + statusH;
    }

    keel::usize digits = 2u;
    keel::usize n = safeLines;
    while (n >= 100u) {
        ++digits;
        n /= 10u;
    }
    const bool hasBpGutter = opts.breakpoints.has_target() || opts.enableBreakpoints ||
        (opts.executionLine > 0u) || (opts.executionLinePtr != nullptr && *opts.executionLinePtr > 0u);
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
        const keel::f32 estimatedW = gutterWidth + padX * 2.0f + keel::as<keel::f32>(keel::min<keel::usize>(maxLineBytes, 80u)) * charAdv + 24.0f;
        boxW = keel::max(boxW, estimatedW);
    }

    return Control::Measure({boxW, boxH}, Control::FromOptions(opts, SizeMode::Stretch, SizeMode::Fit), theme);
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
    keel::usize lineCount = 1u;
    keel::usize maxLineChars = 0u;
    keel::usize curChars = 0u;
    for (char c : text) {
        if (c == '\n') {
            ++lineCount;
            maxLineChars = keel::max(maxLineChars, curChars);
            curChars = 0u;
        } else if (c != '\r') {
            ++curChars;
        }
    }
    maxLineChars = keel::max(maxLineChars, curChars);
    return MeasureEditorBoxFromStats(lineCount, maxLineChars, opts, size, canvas, theme, effZoom, availableWidth);
}

EditorLayoutRects ComputeEditorLayout(
    Rect bounds,
    const TextSourceView& source,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 effZoom,
    keel::f64 scrollX,
    keel::f64 scrollY,
    bool copyFeedbackActive,
    bool searchOpen,
    bool replaceOpen,
    bool gotoOpen,
    keel::ArrayRef<FoldRegion> foldRegions,
    keel::ArrayRef<keel::usize> foldedLines,
    keel::u64 docCacheKey,
    TextAreaState* state,
    const TextAreaData* renderData,
    TextViewProjection& outProj,
    keel::Array<VisualLine>& fallbackExactBuf,
    keel::Array<keel::usize>& fallbackSmallPrefix,
    keel::Array<keel::usize>& fallbackChunkPrefix,
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

    const keel::usize logicalLineCount = source.LineCount();
    keel::usize digits = 2u;
    keel::usize n = keel::max<keel::usize>(1u, logicalLineCount);
    while (n >= 100u) {
        ++digits;
        n /= 10u;
    }

    const bool hasBpGutter = opts.breakpoints.has_target() || opts.enableBreakpoints ||
        (opts.executionLine > 0u) || (opts.executionLinePtr != nullptr && *opts.executionLinePtr > 0u);
    L.bpColWidth = hasBpGutter ? keel::max(28.0f * effZoom, L.metrics.spaceAdvance * 3.4f) : 0.0f;
    L.foldColWidth = opts.codeFolding ? keel::max(14.0f * effZoom, L.metrics.spaceAdvance * 1.85f) : 0.0f;
    const keel::f32 gutterPadX = L.metrics.spaceAdvance * 1.25f;
    L.gutterWidth = opts.showLineNumbers
        ? (L.bpColWidth + keel::as<keel::f32>(digits) * L.metrics.digitAdvance + gutterPadX * 2.0f + L.foldColWidth)
        : (L.bpColWidth + L.foldColWidth);

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

    const keel::f64 availTextH = keel::as<keel::f64>(keel::max(0.0f, L.innerBounds.h - padY * 2.0f));
    // Predict guaranteed vertical scrollbar upfront in O(1) so wordWrap doesn't ping-pong wrapWidth across two passes!
    const bool guaranteedVScroll = !opts.autoHeight && sm.trackWidth > 0.0f && foldedLines.empty() &&
        (keel::as<keel::f64>(logicalLineCount) * keel::as<keel::f64>(L.lineHeight) > availTextH + 1.0);
    const keel::f32 initialVBarReserve = guaranteedVScroll ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;
    keel::f32 prelimTextW = keel::max(0.0f, L.innerBounds.w - L.gutterWidth - L.minimapWidth - padX * 2.0f - initialVBarReserve);

    if (renderData != nullptr && state == nullptr) {
        AttachRenderProjection(
            source, opts, prelimTextW, L.metrics, textStyle, canvas,
            foldRegions, foldedLines, renderData,
            outProj, fallbackExactBuf, fallbackSmallPrefix, fallbackChunkPrefix
        );
    } else {
        PrepareTextViewProjection(
            source, opts.wordWrap, prelimTextW, L.metrics, textStyle, canvas,
            foldRegions, foldedLines, opts.tabSize, docCacheKey, state,
            outProj, &fallbackExactBuf, &fallbackSmallPrefix, &fallbackChunkPrefix
        );
    }

    L.totalVisualRows = keel::max<keel::usize>(1u, outProj.totalVisualRows);
    L.totalContentH = keel::as<keel::f64>(L.totalVisualRows) * keel::as<keel::f64>(L.lineHeight);
    L.needVScroll = !opts.autoHeight && (L.totalContentH > availTextH + 1.0) && sm.trackWidth > 0.0f;

    const keel::f32 vBarReserve = L.needVScroll ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;
    if (L.needVScroll && !guaranteedVScroll && opts.wordWrap) {
        prelimTextW = keel::max(0.0f, L.innerBounds.w - L.gutterWidth - L.minimapWidth - padX * 2.0f - vBarReserve);
        if (renderData != nullptr && state == nullptr) {
            AttachRenderProjection(
                source, opts, prelimTextW, L.metrics, textStyle, canvas,
                foldRegions, foldedLines, renderData,
                outProj, fallbackExactBuf, fallbackSmallPrefix, fallbackChunkPrefix
            );
        } else {
            PrepareTextViewProjection(
                source, opts.wordWrap, prelimTextW, L.metrics, textStyle, canvas,
                foldRegions, foldedLines, opts.tabSize, docCacheKey, state,
                outProj, &fallbackExactBuf, &fallbackSmallPrefix, &fallbackChunkPrefix
            );
        }
        L.totalVisualRows = keel::max<keel::usize>(1u, outProj.totalVisualRows);
        L.totalContentH = keel::as<keel::f64>(L.totalVisualRows) * keel::as<keel::f64>(L.lineHeight);
    }

    // Compute maxLineW in O(1) for huge docs / pure ASCII, or cached exact measurement for small docs.
    L.maxLineW = 0.0;
    if (!opts.wordWrap) {
        if (renderData != nullptr && renderData->hasPrecomputedMaxLineW) {
            L.maxLineW = renderData->precomputedMaxLineW;
        } else {
            const keel::u64 maxWKey = detail::HashArgs(
                docCacheKey,
                keel::round_i32(L.fontSize * 10.0f),
                keel::as<keel::u8>(textStyle),
                opts.tabSize,
                opts.inlineValues.size()
            );
            if (state != nullptr && state->cachedMaxLineWKey == maxWKey && maxWKey != 0u) {
                L.maxLineW = state->cachedMaxLineW;
            } else {
                const keel::usize maxBytes = source.MaxLineByteLength();
                if (logicalLineCount > 512u || maxBytes > 4096u || (opts.monospace && source.IsPureAsciiNoTabs() && opts.inlineValues.empty())) {
                    // O(1) exact monospace width via SumTree maxLineBytes!
                    const keel::f64 charAdv = keel::as<keel::f64>(opts.monospace ? L.metrics.monoCharAdvance : L.metrics.spaceAdvance * 1.15f);
                    L.maxLineW = keel::as<keel::f64>(maxBytes) * charAdv;
                    // Also check inline values if present (usually <= 16 entries, O(inlineValues)).
                    if (!opts.inlineValues.empty()) {
                        keel::String scratch;
                        const keel::f32 ivFs = L.fontSize * 0.88f;
                        for (const CodeInlineValue& iv : opts.inlineValues) {
                            if (iv.line >= 1u && iv.line <= logicalLineCount && !iv.text.empty()) {
                                const TextLineRange lr = source.LineByteRange(iv.line - 1u);
                                const keel::f64 baseW = (lr.contentBytes <= 4096u)
                                    ? keel::as<keel::f64>(MeasureSegmentWidth(canvas, source.ReadSliceRef(lr.startByte, lr.contentBytes, scratch), L.fontSize, textStyle, opts.tabSize))
                                    : (keel::as<keel::f64>(lr.contentBytes) * charAdv);
                                const keel::f64 extra = keel::as<keel::f64>(L.metrics.spaceAdvance * 2.2f + MeasureChunkNoTabs(canvas, iv.text, ivFs, FontStyle::Monospace) + 12.0f);
                                L.maxLineW = keel::max(L.maxLineW, baseW + extra);
                            }
                        }
                    }
                } else {
                    // Small document (<= 512 lines, <= 4 KB lines): exact width measurement, cached across frames!
                    keel::String scratch;
                    for (keel::usize li = 0u; li < logicalLineCount; ++li) {
                        const TextLineRange lr = source.LineByteRange(li);
                        if (lr.contentBytes == 0u) continue;
                        const keel::StringRef ls = source.ReadSliceRef(lr.startByte, lr.contentBytes, scratch);
                        keel::f64 lineW = keel::as<keel::f64>(MeasureSegmentWidth(canvas, ls, L.fontSize, textStyle, opts.tabSize));
                        if (!opts.inlineValues.empty()) {
                            for (const CodeInlineValue& iv : opts.inlineValues) {
                                if (iv.line == li + 1u && !iv.text.empty()) {
                                    const keel::f32 ivFs = L.fontSize * 0.88f;
                                    lineW += keel::as<keel::f64>(L.metrics.spaceAdvance * 2.2f + MeasureChunkNoTabs(canvas, iv.text, ivFs, FontStyle::Monospace) + 12.0f);
                                }
                            }
                        }
                        L.maxLineW = keel::max(L.maxLineW, lineW);
                    }
                }
                if (state != nullptr && docCacheKey != 0u) {
                    state->cachedMaxLineW = L.maxLineW;
                    state->cachedMaxLineWKey = maxWKey;
                }
            }
        }
    }

    const keel::f64 availTextW = keel::as<keel::f64>(keel::max(0.0f, L.innerBounds.w - L.gutterWidth - L.minimapWidth - padX * 2.0f - vBarReserve));
    L.needHScroll = !opts.wordWrap && (L.maxLineW > availTextW + 2.0) && sm.trackWidth > 0.0f;
    const keel::f32 hBarReserve = L.needHScroll ? (sm.trackWidth + sm.margin * 2.0f) : 0.0f;

    const keel::f32 finalViewportH = keel::max(0.0f, L.innerBounds.h - padY * 2.0f - hBarReserve);
    L.needVScroll = !opts.autoHeight && (L.totalContentH > keel::as<keel::f64>(finalViewportH) + 1.0) && sm.trackWidth > 0.0f;
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

    L.maxScrollY = keel::max(0.0, L.totalContentH - keel::as<keel::f64>(L.textViewport.h));
    L.maxScrollX = opts.wordWrap ? 0.0 : keel::max(0.0, L.maxLineW + 24.0 - keel::as<keel::f64>(L.textViewport.w));

    const keel::f64 clampedScrollY = keel::clamp(scrollY, 0.0, L.maxScrollY);
    const keel::f64 clampedScrollX = keel::clamp(scrollX, 0.0, L.maxScrollX);

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
        if (trackH > 0.0f && L.totalContentH > 0.0) {
            const keel::f32 visibleRatio = keel::as<keel::f32>(keel::clamp(keel::as<keel::f64>(L.textViewport.h) / L.totalContentH, 0.02, 1.0));
            const keel::f32 thumbH = keel::min(trackH, keel::max(sm.minThumbLength, trackH * visibleRatio));
            const keel::f32 maxTravel = keel::max(0.0f, trackH - thumbH);
            const keel::f64 scrollFrac = (L.maxScrollY > 0.0) ? (clampedScrollY / L.maxScrollY) : 0.0;
            const keel::f32 thumbY = trackY + keel::as<keel::f32>(scrollFrac * keel::as<keel::f64>(maxTravel));
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
        const keel::f64 totalW = L.maxLineW + 24.0;
        if (trackW > 0.0f && totalW > 0.0) {
            const keel::f32 visibleRatio = keel::as<keel::f32>(keel::clamp(keel::as<keel::f64>(L.textViewport.w) / totalW, 0.02, 1.0));
            const keel::f32 thumbW = keel::min(trackW, keel::max(sm.minThumbLength, trackW * visibleRatio));
            const keel::f32 maxTravel = keel::max(0.0f, trackW - thumbW);
            const keel::f64 scrollFrac = (L.maxScrollX > 0.0) ? (clampedScrollX / L.maxScrollX) : 0.0;
            const keel::f32 thumbX = trackX + keel::as<keel::f32>(scrollFrac * keel::as<keel::f64>(maxTravel));
            L.hThumbRect = Rect(thumbX, barY, thumbW, sm.trackWidth);
        }
    }

    // Top-right quick buttons (Copy, Zoom %) when Search overlay is closed
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

    // Search & Replace / GoToLine floating overlay geometry
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
        const keel::f32 countW = tbCm.defaultHeight * 2.4f;
        const keel::f32 maxPanelW = tbCm.defaultHeight * 14.0f;
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

    // Floating Debug Toolbar
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
        const keel::f32 sepExtra = tbGap;
        const keel::f32 barW = gripW + 6.0f * btnW + 5.0f * gap + sepExtra + pad * 2.0f;

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
            barX = keel::clamp(L.innerBounds.x + (L.innerBounds.w - barW) * 0.5f, minBarX, maxBarX);
        }

        if (L.searchOverlay.panelRect.w > 0.0f && barX + barW > L.searchOverlay.panelRect.x - tbGap) {
            barX = keel::max(minBarX, L.searchOverlay.panelRect.x - barW - tbGap);
        }
        L.debugToolbar.panelRect = Rect(barX, overlayTop, barW, btnH2 + pad * 2.0f);
        L.debugToolbar.gripRect = Rect(barX, overlayTop, pad + gripW, btnH2 + pad * 2.0f);

        if (L.zoomBtnRect.w > 0.0f) {
            const keel::f32 overlapPad = tbGap * 0.75f;
            if (L.debugToolbar.panelRect.Right() + overlapPad > L.zoomBtnRect.x &&
                L.debugToolbar.panelRect.x < L.zoomBtnRect.Right() + overlapPad) {
                L.zoomBtnRect.x = L.debugToolbar.panelRect.x - overlapPad - L.zoomBtnRect.w;
            }
        }

        keel::f32 bx = barX + pad + gripW;
        for (keel::usize i = 0u; i < 6u; ++i) {
            if (i == 4u) {
                bx += sepExtra;
            }
            L.debugToolbar.btnRects[i] = Rect(bx, overlayTop + pad, btnW, btnH2);
            bx += btnW + gap;
        }
    }

    return L;
}

} // namespace guikit::detail::ta_impl
