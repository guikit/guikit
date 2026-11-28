#include "text_area_internal.hpp"

namespace guikit::detail::ta_impl {

namespace {

[[nodiscard]] bool IsWordCodepoint(keel::u32 cp) noexcept {
    if (cp < 0x80u) {
        return IsIdentChar(keel::as<char>(cp));
    }
    return (cp >= 0x00C0u && cp <= 0x024Fu && cp != 0x00D7u && cp != 0x00F7u) ||
           (cp >= 0x0400u && cp <= 0x04FFu);
}

[[nodiscard]] bool PassesWholeWordBoundary(
    const TextSourceView& source,
    keel::usize matchStart,
    keel::usize matchEnd,
    keel::String& scratch
) noexcept {
    if (matchStart > 0u) {
        const keel::usize winStart = (matchStart >= 4u) ? (matchStart - 4u) : 0u;
        const keel::StringRef left = source.ReadSliceRef(winStart, matchStart - winStart, scratch);
        if (!left.empty()) {
            keel::usize prevOff = keel::utf8_prev_boundary(left, left.size());
            if (IsWordCodepoint(keel::utf8_decode_next(left, prevOff))) {
                return false;
            }
        }
    }
    const keel::usize total = source.ByteLength();
    if (matchEnd < total) {
        const keel::StringRef right = source.ReadSliceRef(matchEnd, keel::min<keel::usize>(4u, total - matchEnd), scratch);
        if (!right.empty()) {
            keel::usize nextOff = 0u;
            if (IsWordCodepoint(keel::utf8_decode_next(right, nextOff))) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

// =============================================================================
// Piece-Streaming Search & Bucketed Overview Ruler
// =============================================================================

void FindAllSearchMatches(
    const TextSourceView& source,
    keel::StringRef query,
    bool matchCase,
    bool wholeWord,
    keel::Array<SearchMatch>& outMatches,
    keel::MutArrayRef<keel::u8> outRulerBuckets,
    bool& outCapped,
    keel::usize maxMatches
) {
    outMatches.clear();
    outCapped = false;
    for (keel::u8& b : outRulerBuckets) {
        b = 0u;
    }
    if (query.empty() || source.Empty()) return;

    const keel::usize qLen = query.size();
    const keel::usize totalBytes = source.ByteLength();
    const keel::usize totalLines = keel::max<keel::usize>(1u, source.LineCount());
    const keel::usize bucketCount = outRulerBuckets.size();

    // Case-insensitive UTF-8 match length can differ slightly from query length (rare, e.g. 2-byte folding);
    // keep a comfortable overlap across 64 KB piece boundaries.
    const keel::usize overlapBytes = qLen + 8u;
    keel::String carry;
    keel::String bridge;
    keel::String boundaryScratch;

    keel::usize chunkGlobalStart = 0u;
    keel::usize minNextGlobalStart = 0u;

    auto recordMatch = [&](keel::usize matchStart, keel::usize matchLen) -> bool {
        const keel::usize matchEnd = matchStart + matchLen;
        if (wholeWord && !PassesWholeWordBoundary(source, matchStart, matchEnd, boundaryScratch)) {
            return true;
        }
        if (outMatches.size() >= maxMatches) {
            outCapped = true;
            return false;
        }
        const TextLineCol lc = source.OffsetToLineCol(matchStart);
        outMatches.push_back(SearchMatch{
            .startOffset = matchStart,
            .endOffset = matchEnd,
            .logicalLine = lc.line,
            .startCol = lc.colBytes,
            .endCol = lc.colBytes + matchLen
        });
        if (bucketCount > 0u) {
            const keel::usize bucket = keel::min(bucketCount - 1u, (lc.line * bucketCount) / totalLines);
            outRulerBuckets[bucket] = 1u;
        }
        return true;
    };

    auto scanBuffer = [&](keel::StringRef buf, keel::usize bufGlobalStart, keel::usize maxMatchStartExclusive) -> bool {
        keel::usize pos = (minNextGlobalStart > bufGlobalStart) ? (minNextGlobalStart - bufGlobalStart) : 0u;
        while (pos < buf.size() && (bufGlobalStart + pos) < maxMatchStartExclusive) {
            keel::usize relStart = keel::StringRef::npos;
            keel::usize matchLen = 0u;
            if (matchCase) {
                relStart = buf.find(query, pos);
                matchLen = qLen;
            } else {
                keel::usize foundRel = 0u;
                if (TextUtil::FindCaseInsensitive(buf.substr(pos), query, foundRel, matchLen)) {
                    relStart = pos + foundRel;
                }
            }
            if (relStart == keel::StringRef::npos || matchLen == 0u) {
                break;
            }
            const keel::usize globalStart = bufGlobalStart + relStart;
            if (globalStart >= maxMatchStartExclusive) {
                break;
            }

            if (!recordMatch(globalStart, matchLen)) {
                return false;
            }
            const keel::usize acceptedEnd = globalStart + matchLen;
            const keel::usize nextCharGlobal = bufGlobalStart + keel::utf8_next_boundary(buf, relStart);
            minNextGlobalStart = keel::max(acceptedEnd, nextCharGlobal);
            pos = minNextGlobalStart - bufGlobalStart;
        }
        return true;
    };

    source.ForEachChunkInRange(0u, totalBytes, [&](keel::StringRef chunk) -> bool {
        if (chunk.empty()) return true;

        // 1. Scan cross-boundary bridge [carry + head of chunk] if carry is non-empty.
        if (!carry.empty()) {
            const keel::usize headTake = keel::min(chunk.size(), overlapBytes);
            bridge.clear();
            bridge.reserve(carry.size() + headTake);
            bridge.append(carry.as_ref());
            bridge.append(chunk.substr(0u, headTake));
            const keel::usize bridgeGlobalStart = chunkGlobalStart - carry.size();
            if (!scanBuffer(bridge.as_ref(), bridgeGlobalStart, chunkGlobalStart)) {
                return false;
            }
        }

        // 2. Zero-copy scan inside this chunk itself!
        if (!scanBuffer(chunk, chunkGlobalStart, chunkGlobalStart + chunk.size())) {
            return false;
        }

        // 3. Save tail overlap for the next chunk boundary.
        const keel::usize keep = keel::min(chunk.size(), overlapBytes);
        carry.assign(chunk.substr(chunk.size() - keep, keep));
        chunkGlobalStart += chunk.size();
        return !outCapped;
    });
}

keel::usize FindFirstSearchMatchAtOrAfter(
    keel::ArrayRef<SearchMatch> matches,
    keel::usize byteOffset
) noexcept {
    keel::usize lo = 0u;
    keel::usize hi = matches.size();
    while (lo < hi) {
        const keel::usize mid = lo + (hi - lo) / 2u;
        if (matches[mid].endOffset <= byteOffset) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return lo;
}

// =============================================================================
// Bounded-Window Bracket Matching & Word Extraction (Safe on 10 MB Lines!)
// =============================================================================

bool FindMatchingBracketPair(
    const TextSourceView& source,
    keel::usize cursorOffset,
    keel::usize& outOpenOffset,
    keel::usize& outCloseOffset
) noexcept {
    const keel::usize total = source.ByteLength();
    if (total == 0u) return false;
    const keel::usize safeCur = keel::min(cursorOffset, total);

    auto isBracket = [](char c) noexcept {
        return c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}';
    };

    keel::usize brPos = keel::StringRef::npos;
    if (safeCur < total && isBracket(source.ByteAt(safeCur))) {
        brPos = safeCur;
    } else if (safeCur > 0u && isBracket(source.ByteAt(safeCur - 1u))) {
        brPos = safeCur - 1u;
    }
    if (brPos == keel::StringRef::npos) return false;

    const char c = source.ByteAt(brPos);
    const bool forward = (c == '(' || c == '[' || c == '{');
    const char target = (c == '(') ? ')' : (c == ')') ? '('
                      : (c == '[') ? ']' : (c == ']') ? '['
                      : (c == '{') ? '}' : '{';

    constexpr keel::usize kMaxBracketScan = 16384u;
    keel::String winScratch;
    keel::i32 depth = 0;

    if (forward) {
        const keel::usize scanLen = keel::min(total - brPos, kMaxBracketScan);
        const keel::StringRef win = source.ReadSliceRef(brPos, scanLen, winScratch);
        for (keel::usize i = 0u; i < win.size(); ++i) {
            if (win[i] == c) ++depth;
            else if (win[i] == target) {
                --depth;
                if (depth == 0) {
                    outOpenOffset = brPos;
                    outCloseOffset = brPos + i;
                    return true;
                }
            }
        }
    } else {
        const keel::usize winStart = (brPos >= kMaxBracketScan) ? (brPos - kMaxBracketScan + 1u) : 0u;
        const keel::usize scanLen = (brPos - winStart) + 1u;
        const keel::StringRef win = source.ReadSliceRef(winStart, scanLen, winScratch);
        for (keel::usize i = win.size(); i > 0u; --i) {
            const keel::usize idx = i - 1u;
            if (win[idx] == c) ++depth;
            else if (win[idx] == target) {
                --depth;
                if (depth == 0) {
                    outOpenOffset = winStart + idx;
                    outCloseOffset = brPos;
                    return true;
                }
            }
        }
    }
    return false;
}

keel::StringRef ExtractWordAtOffset(
    const TextSourceView& source,
    keel::usize offset,
    keel::String& scratch,
    keel::usize* outStartOffset
) noexcept {
    const keel::usize total = source.ByteLength();
    if (total == 0u) return {};

    constexpr keel::usize kHalfWin = 128u;
    const keel::usize clamped = keel::min(offset, total);
    const keel::usize winStart = (clamped > kHalfWin) ? (clamped - kHalfWin) : 0u;
    const keel::usize winLen = keel::min(total - winStart, (clamped - winStart) + kHalfWin);
    const keel::StringRef win = source.ReadSliceRef(winStart, winLen, scratch);
    if (win.empty()) return {};

    keel::usize pos = keel::min(clamped - winStart, win.size());
    if (pos == win.size() || !IsIdentChar(win[pos])) {
        if (pos > 0u && IsIdentChar(win[pos - 1u])) {
            --pos;
        } else {
            return {};
        }
    }
    keel::usize s = pos;
    while (s > 0u && IsIdentChar(win[s - 1u])) --s;
    if (!IsIdentStart(win[s])) return {};
    keel::usize e = pos + 1u;
    while (e < win.size() && IsIdentChar(win[e])) ++e;
    if (outStartOffset) *outStartOffset = winStart + s;
    return win.substr(s, e - s);
}

} // namespace guikit::detail::ta_impl
