#include "guikit/core.hpp"
#include "guikit/core/text_document.hpp"
#include "keel/keel.h"
#include "keel/time.h"
#include "keel/utf8.h"

namespace {

keel::i32 g_failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            keel::eprintln("[FAIL] {} ({}:{})", #cond, __FILE__, __LINE__); \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

struct Rng64 {
    keel::u64 state{0x9E3779B97F4A7C15ULL};

    [[nodiscard]] keel::u64 Next() noexcept {
        keel::u64 x = state;
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        state = x;
        return x;
    }

    [[nodiscard]] keel::usize Range(keel::usize bound) noexcept {
        if (bound == 0u) return 0u;
        return keel::as<keel::usize>(Next() % bound);
    }
};

void OracleReplace(keel::String& s, keel::usize offset, keel::usize eraseBytes, keel::StringRef insertText) {
    keel::String next;
    next.reserve(s.size() - eraseBytes + insertText.size());
    next.append(s.as_ref().substr(0u, offset));
    next.append(insertText);
    next.append(s.as_ref().substr(offset + eraseBytes));
    s = keel::move(next);
}

[[nodiscard]] keel::usize OracleLineCount(keel::StringRef s) noexcept {
    keel::usize lines = 1u;
    for (char c : s) {
        if (c == '\n') ++lines;
    }
    return lines;
}

[[nodiscard]] keel::usize OracleMaxSegmentBytes(keel::StringRef s) noexcept {
    keel::usize best = 0u;
    keel::usize cur = 0u;
    for (char c : s) {
        if (c == '\n') {
            if (cur > best) best = cur;
            cur = 0u;
        } else {
            ++cur;
        }
    }
    if (cur > best) best = cur;
    return best;
}

[[nodiscard]] guikit::TextLineRange OracleLineRange(keel::StringRef s, keel::usize targetLine) noexcept {
    keel::usize curLine = 0u;
    keel::usize lineStart = 0u;
    for (keel::usize i = 0u; i < s.size(); ++i) {
        if (s[i] == '\n') {
            if (curLine == targetLine) {
                const keel::usize total = (i + 1u) - lineStart;
                keel::usize content = i - lineStart;
                if (content > 0u && s[lineStart + content - 1u] == '\r') {
                    --content;
                }
                return guikit::TextLineRange{lineStart, content, total};
            }
            ++curLine;
            lineStart = i + 1u;
        }
    }
    const keel::usize rem = s.size() - lineStart;
    return guikit::TextLineRange{lineStart, rem, rem};
}

[[nodiscard]] guikit::TextLineCol OracleOffsetToLineCol(keel::StringRef s, keel::usize offset) noexcept {
    if (offset > s.size()) offset = s.size();
    keel::usize line = 0u;
    keel::usize col = 0u;
    for (keel::usize i = 0u; i < offset; ++i) {
        if (s[i] == '\n') {
            ++line;
            col = 0u;
        } else {
            ++col;
        }
    }
    return guikit::TextLineCol{line, col};
}

void TestEmptyAndBasicOperations() {
    using namespace guikit;

    TextDocument doc;
    CHECK(doc.Empty());
    CHECK(doc.ByteLength() == 0u);
    CHECK(doc.LineCount() == 1u);
    CHECK(doc.MaxLineByteLength() == 0u);
    CHECK(doc.IsPureAsciiNoTabs());
    CHECK(doc.PieceCount() == 0u);
    CHECK(doc.VerifyInvariants());
    CHECK(doc.LineByteRange(0u) == (TextLineRange{0u, 0u, 0u}));
    CHECK(doc.OffsetToLineCol(0u) == (TextLineCol{0u, 0u}));
    CHECK(doc.LineColToOffset(0u, 10u) == 0u);
    CHECK(doc.ByteAt(0u) == '\0');
    CHECK(doc.ToString().empty());

    doc.SetText("alpha\nbeta gamma\nomega");
    CHECK(!doc.Empty());
    CHECK(doc.ByteLength() == 22u);
    CHECK(doc.LineCount() == 3u);
    CHECK(doc.MaxLineByteLength() == 10u);
    CHECK(doc.IsPureAsciiNoTabs());
    CHECK(doc.VerifyInvariants());

    CHECK(doc.LineByteRange(0u) == (TextLineRange{0u, 5u, 6u}));
    CHECK(doc.LineByteRange(1u) == (TextLineRange{6u, 10u, 11u}));
    CHECK(doc.LineByteRange(2u) == (TextLineRange{17u, 5u, 5u}));
    CHECK(doc.OffsetToLineCol(0u) == (TextLineCol{0u, 0u}));
    CHECK(doc.OffsetToLineCol(5u) == (TextLineCol{0u, 5u}));
    CHECK(doc.OffsetToLineCol(6u) == (TextLineCol{1u, 0u}));
    CHECK(doc.OffsetToLineCol(11u) == (TextLineCol{1u, 5u}));
    CHECK(doc.OffsetToLineCol(22u) == (TextLineCol{2u, 5u}));
    CHECK(doc.LineColToOffset(1u, 5u) == 11u);
    CHECK(doc.LineColToOffset(1u, 999u) == 16u);

    keel::String slice;
    doc.ReadLineSlice(1u, 5u, 5u, slice);
    CHECK(slice == "gamma");
    doc.ReadLine(2u, slice);
    CHECK(slice == "omega");

    TextDocument cloned = doc.Clone();
    CHECK(cloned.ToString() == doc.ToString());
    CHECK(cloned.VerifyInvariants());

    doc.Clear();
    CHECK(doc.Empty());
    CHECK(doc.LineCount() == 1u);
    CHECK(doc.VerifyInvariants());
}

void TestBalancedBuildAcrossChunkCounts() {
    using namespace guikit;

    // Test exact bottom-up balanced Red-Black tree invariants for chunk counts 1..25.
    keel::String line = keel::String::from("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcde\n"); // 64 bytes
    keel::String payload;
    for (keel::usize chunkCount = 1u; chunkCount <= 25u; ++chunkCount) {
        const keel::usize targetBytes = chunkCount * TextDocument::kChunkTargetBytes;
        payload.reserve(targetBytes);
        while (payload.size() < targetBytes) {
            payload.append(line.as_ref());
        }
        TextDocument doc(payload.as_ref());
        CHECK(doc.VerifyInvariants());
        CHECK(doc.ByteLength() == payload.size());
        CHECK(doc.LineCount() == payload.size() / 64u + 1u);
        CHECK(doc.MaxLineByteLength() == 63u);
        CHECK(doc.IsPureAsciiNoTabs());
        CHECK(doc.ToString() == payload);
    }
}

void TestCrlfUtf8AndChunkBoundaries() {
    using namespace guikit;

    // 1. Place "\r\n" right across the 64 KB chunk boundary.
    keel::String crlfText;
    crlfText.reserve(TextDocument::kChunkTargetBytes + 128u);
    for (keel::usize i = 0u; i + 1u < TextDocument::kChunkTargetBytes; ++i) {
        crlfText.push_back('a');
    }
    crlfText.append("\r\nsecond_line\r\n");

    TextDocument crlfDoc(crlfText.as_ref());
    CHECK(crlfDoc.VerifyInvariants());
    CHECK(crlfDoc.LineCount() == 3u);
    CHECK(crlfDoc.IsPureAsciiNoTabs());
    const TextLineRange r0 = crlfDoc.LineByteRange(0u);
    CHECK(r0.startByte == 0u);
    CHECK(r0.contentBytes == TextDocument::kChunkTargetBytes - 1u);
    CHECK(r0.totalBytes == TextDocument::kChunkTargetBytes + 1u);
    keel::String line1;
    crlfDoc.ReadLine(1u, line1);
    CHECK(line1 == "second_line");

    // 2. Force cross-piece "\r" | "\n" via edits and verify CRLF stripping still works.
    TextDocument splitCrlf("hello\r");
    CHECK(splitCrlf.LineCount() == 1u);
    CHECK(!splitCrlf.IsPureAsciiNoTabs()); // trailing stray '\r'
    splitCrlf.Insert(6u, "\nworld");
    CHECK(splitCrlf.VerifyInvariants());
    CHECK(splitCrlf.LineCount() == 2u);
    CHECK(splitCrlf.LineByteRange(0u) == (TextLineRange{0u, 5u, 7u}));
    CHECK(splitCrlf.LineByteRange(1u) == (TextLineRange{7u, 5u, 5u}));
    keel::String l0;
    splitCrlf.ReadLine(0u, l0);
    CHECK(l0 == "hello");

    // 3. Multi-byte UTF-8 (Cyrillic 2B, CJK 3B, Emoji 4B) right at 64 KB chunk boundary.
    keel::String utf8Text;
    utf8Text.reserve(TextDocument::kChunkTargetBytes + 64u);
    for (keel::usize i = 0u; i < TextDocument::kChunkTargetBytes - 2u; ++i) {
        utf8Text.push_back('x');
    }
    utf8Text.append("\xF0\x9F\x9A\x80"); // 🚀 (4 bytes) straddling 64 KB mark
    utf8Text.append("\n\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82 \xE4\xB8\x96\xE7\x95\x8C\nplain_ascii_line");

    TextDocument utf8Doc(utf8Text.as_ref());
    CHECK(utf8Doc.VerifyInvariants());
    CHECK(!utf8Doc.IsPureAsciiNoTabs());
    CHECK(utf8Doc.ToString() == utf8Text);
    CHECK(utf8Doc.LineCount() == 3u);

    // Line 2 is pure ASCII even though lines 0-1 contain UTF-8.
    const TextLineRange r2 = utf8Doc.LineByteRange(2u);
    CHECK(utf8Doc.RangeIsPureAsciiNoTabs(r2.startByte, r2.contentBytes));

    // UTF-8 boundary snapping & navigation around the 4-byte emoji.
    const keel::usize emojiStart = TextDocument::kChunkTargetBytes - 2u;
    CHECK(utf8Doc.SnapToUtf8Boundary(emojiStart) == emojiStart);
    CHECK(utf8Doc.SnapToUtf8Boundary(emojiStart + 1u) == emojiStart);
    CHECK(utf8Doc.SnapToUtf8Boundary(emojiStart + 2u) == emojiStart);
    CHECK(utf8Doc.SnapToUtf8Boundary(emojiStart + 3u) == emojiStart);
    CHECK(utf8Doc.SnapToUtf8Boundary(emojiStart + 4u) == emojiStart + 4u);
    CHECK(utf8Doc.NextCharOffset(emojiStart) == emojiStart + 4u);
    CHECK(utf8Doc.PrevCharOffset(emojiStart + 4u) == emojiStart);

    // Word bounds on Cyrillic + CJK line.
    const TextLineRange r1 = utf8Doc.LineByteRange(1u);
    keel::usize ws = 0;
    keel::usize we = 0;
    utf8Doc.WordBoundsAt(r1.startByte + 2u, ws, we);
    keel::String word;
    utf8Doc.ReadSlice(ws, we - ws, word);
    CHECK(word == "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82");
}

void TestUndoRedoEditGroupsAndCoalescing() {
    using namespace guikit;

    TextDocument doc("hello world");
    // 1. Typing coalescing: typing "!!!" merges into one undo step.
    doc.ReplaceRange(11u, 0u, "!", 11u, 11u, 12u, 12u, true, true);
    doc.ReplaceRange(12u, 0u, "!", 12u, 12u, 13u, 13u, true, true);
    doc.ReplaceRange(13u, 0u, "!", 13u, 13u, 14u, 14u, true, true);
    CHECK(doc.ToString() == "hello world!!!");
    CHECK(doc.UndoCount() == 1u);

    auto u1 = doc.Undo();
    CHECK(u1.changed && u1.cursor == 11u);
    CHECK(doc.ToString() == "hello world");

    auto r1 = doc.Redo();
    CHECK(r1.changed && r1.cursor == 14u);
    CHECK(doc.ToString() == "hello world!!!");

    // 2. Atomic EditGroupScope (Markdown WYSIWYG bold toggle: wrap "world" in "**").
    {
        TextDocument::EditGroupScope group(doc);
        doc.ReplaceRange(11u, 0u, "**", 6u, 11u, 6u, 13u);
        doc.ReplaceRange(6u, 0u, "**", 6u, 13u, 8u, 13u);
    }
    CHECK(doc.ToString() == "hello **world**!!!");
    CHECK(doc.VerifyInvariants());

    // Single Undo reverts BOTH "**" insertions!
    auto u2 = doc.Undo();
    CHECK(u2.changed);
    CHECK(doc.ToString() == "hello world!!!");
    CHECK(doc.VerifyInvariants());

    // Single Redo reapplies BOTH "**" insertions!
    auto r2 = doc.Redo();
    CHECK(r2.changed);
    CHECK(doc.ToString() == "hello **world**!!!");
    CHECK(doc.VerifyInvariants());

    // New edit clears redo stack.
    doc.Undo();
    CHECK(doc.CanRedo());
    doc.Insert(0u, "prefix ");
    CHECK(!doc.CanRedo());
    CHECK(doc.ToString() == "prefix hello world!!!");
}

void TestDeltaLogAndOffsetMapping() {
    using namespace guikit;

    TextDocument doc("line0\nline1\nline2\nline3");
    const keel::u64 rev0 = doc.Revision();

    // Insert 2 new lines inside line1.
    const TextEditDelta d1 = doc.Insert(11u, "\ninsertedA\ninsertedB"); // 20 bytes
    CHECK(d1.revision == rev0 + 1u);
    CHECK(d1.startByte == 11u);
    CHECK(d1.oldByteLen == 0u);
    CHECK(d1.newByteLen == 20u);
    CHECK(d1.startLine == 1u);
    CHECK(d1.lineDelta == 2);

    // Offset projection checks (Left vs Right bias at boundary).
    CHECK(MapOffsetThroughEdit(5u, d1, TextOffsetBias::Right) == 5u);
    CHECK(MapOffsetThroughEdit(11u, d1, TextOffsetBias::Left) == 11u);
    CHECK(MapOffsetThroughEdit(11u, d1, TextOffsetBias::Right) == 31u);
    CHECK(MapOffsetThroughEdit(15u, d1, TextOffsetBias::Left) == 35u);

    // Line projection checks.
    CHECK(MapLineThroughEdit(0u, d1) == 0u);
    CHECK(MapLineThroughEdit(1u, d1) == 1u);
    CHECK(MapLineThroughEdit(2u, d1) == 4u);
    CHECK(MapLineThroughEdit(3u, d1) == 5u);

    // Delta replay including Undo/Redo.
    doc.Erase(0u, 6u); // removes "line0\n"
    doc.Undo();        // restores "line0\n"

    keel::Array<TextEditDelta> deltas;
    CHECK(doc.GetDeltasSince(rev0, deltas));
    CHECK(deltas.size() == 3u);
    CHECK(deltas[0].lineDelta == 2);
    CHECK(deltas[1].lineDelta == -1);
    CHECK(deltas[2].lineDelta == 1);

    // Overflowing delta ring buffer (> 64 edits) returns false.
    const keel::u64 revBeforeBurst = doc.Revision();
    for (keel::usize i = 0u; i < TextDocument::kDeltaHistoryCapacity + 5u; ++i) {
        doc.Insert(0u, "a", false);
    }
    CHECK(!doc.GetDeltasSince(revBeforeBurst, deltas));

    // SetText resets delta chain validity.
    const keel::u64 revBeforeReset = doc.Revision();
    doc.SetText("fresh");
    CHECK(!doc.GetDeltasSince(revBeforeReset, deltas));
    CHECK(doc.GetDeltasSince(doc.Revision(), deltas) && deltas.empty());
}

void TestRandomizedFuzzAgainstOracle() {
    using namespace guikit;

    Rng64 rng{0x123456789ABCDEF0ULL};
    keel::String oracle("initial\nline_one\r\nline_two\twith_tab\n\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82\n");
    TextDocument doc(oracle.as_ref());

    keel::Array<keel::String> undoOracle;
    keel::Array<keel::String> redoOracle;

    const char* snippets[] = {
        "a",
        "hello",
        "\n",
        "\r\n",
        "foo\nbar\nbaz",
        "\tindented\n",
        "\xD0\xA2\xD0\xB5\xD1\x81\xD1\x82", // Тест
        "\xF0\x9F\x9A\x80",                 // 🚀
        "lineA\r\nlineB\r\n",
        ""
    };

    const keel::i32 failuresBeforeFuzz = g_failures;
    for (keel::usize step = 0u; step < 1200u; ++step) {
        const keel::usize action = rng.Range(100u);

        if (action < 45u) {
            // Insert
            const keel::usize pos = rng.Range(oracle.size() + 1u);
            keel::String ins;
            if (action == 0u) {
                // Occasional large multi-chunk insert (> 64 KB) to stress chunk splits!
                ins.reserve(70000u);
                for (keel::usize k = 0u; k < 1000u; ++k) {
                    ins.append("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456\n");
                }
            } else {
                ins.assign(snippets[rng.Range(9u)]);
            }
            undoOracle.push_back(oracle.clone());
            redoOracle.clear();
            OracleReplace(oracle, pos, 0u, ins.as_ref());
            doc.Insert(pos, ins.as_ref());
        } else if (action < 75u && !oracle.empty()) {
            // Erase
            const keel::usize pos = rng.Range(oracle.size());
            const keel::usize maxErase = oracle.size() - pos;
            const keel::usize len = 1u + rng.Range(keel::min(maxErase, keel::as<keel::usize>(512u)));
            undoOracle.push_back(oracle.clone());
            redoOracle.clear();
            OracleReplace(oracle, pos, len, keel::StringRef{});
            doc.Erase(pos, len);
        } else if (action < 88u && !oracle.empty()) {
            // Replace range
            const keel::usize pos = rng.Range(oracle.size());
            const keel::usize maxErase = oracle.size() - pos;
            const keel::usize len = rng.Range(keel::min(maxErase, keel::as<keel::usize>(128u)) + 1u);
            const keel::StringRef ins = snippets[rng.Range(9u)];
            if (len > 0u || !ins.empty()) {
                undoOracle.push_back(oracle.clone());
                redoOracle.clear();
                OracleReplace(oracle, pos, len, ins);
                doc.ReplaceRange(pos, len, ins);
            }
        } else if (action < 95u && !undoOracle.empty() && doc.CanUndo()) {
            // Undo (note: history capped at 512 groups, so keep oracle synced only when undo succeeds)
            redoOracle.push_back(oracle.clone());
            oracle = keel::move(undoOracle.back());
            undoOracle.pop_back();
            CHECK(doc.Undo().changed);
        } else if (!redoOracle.empty() && doc.CanRedo()) {
            undoOracle.push_back(oracle.clone());
            oracle = keel::move(redoOracle.back());
            redoOracle.pop_back();
            CHECK(doc.Redo().changed);
        }

        // Keep oracle history length within TextDocument::kMaxUndoGroups to avoid cap divergence.
        while (undoOracle.size() > TextDocument::kMaxUndoGroups) {
            undoOracle.remove_at(0u);
        }

        CHECK(doc.VerifyInvariants());
        CHECK(doc.ByteLength() == oracle.size());
        CHECK(doc.LineCount() == OracleLineCount(oracle.as_ref()));
        CHECK(doc.MaxLineByteLength() == OracleMaxSegmentBytes(oracle.as_ref()));

        if (step % 25u == 0u || step == 1199u) {
            CHECK(doc.ToString() == oracle);
        }

        // Spot-check random line ranges, offset<->line/col round-trips, and slices.
        const keel::usize lines = doc.LineCount();
        const keel::usize probeLine = rng.Range(lines);
        const TextLineRange expectedRange = OracleLineRange(oracle.as_ref(), probeLine);
        const TextLineRange actualRange = doc.LineByteRange(probeLine);
        CHECK(actualRange == expectedRange);

        const keel::usize probeOff = rng.Range(oracle.size() + 1u);
        const TextLineCol expectedLc = OracleOffsetToLineCol(oracle.as_ref(), probeOff);
        const TextLineCol actualLc = doc.OffsetToLineCol(probeOff);
        CHECK(actualLc == expectedLc);

        if (expectedRange.contentBytes > 0u) {
            const keel::usize colStart = rng.Range(expectedRange.contentBytes);
            const keel::usize sliceLen = 1u + rng.Range(keel::min(expectedRange.contentBytes - colStart, keel::as<keel::usize>(64u)));
            keel::String actualSlice;
            doc.ReadLineSlice(probeLine, colStart, sliceLen, actualSlice);
            CHECK(actualSlice.as_ref() == oracle.as_ref().substr(expectedRange.startByte + colStart, sliceLen));
        }

        if (g_failures > failuresBeforeFuzz) {
            keel::eprintln("[ABORT] Fuzz diverged at step {}", step);
            return;
        }
    }
}

void TestOneMillionLinesStressAndBenchmark() {
    using namespace guikit;

    constexpr keel::usize kLineCount = 1000000u;
    keel::String big;
    big.reserve(kLineCount * 46u);
    for (keel::usize i = 0u; i < kLineCount; ++i) {
        big.append("const keel::i32 value_");
        char numBuf[16];
        keel::usize n = i;
        keel::usize digits = 0u;
        do {
            numBuf[digits++] = keel::as<char>('0' + (n % 10u));
            n /= 10u;
        } while (n > 0u);
        while (digits > 0u) {
            big.push_back(numBuf[--digits]);
        }
        big.append(" = 42; // ok");
        if (i + 1u < kLineCount) {
            big.push_back('\n');
        }
    }

    const keel::u64 t0 = keel::monotonic_ns();
    TextDocument doc(big.as_ref());
    const keel::u64 tLoadNs = keel::monotonic_ns() - t0;

    CHECK(doc.LineCount() == kLineCount);
    CHECK(doc.ByteLength() == big.size());
    CHECK(doc.IsPureAsciiNoTabs());
    CHECK(doc.VerifyInvariants());

    // Benchmark 300 edits (start, middle, end) + Undo/Redo.
    const keel::u64 tEditStart = keel::monotonic_ns();
    const keel::usize midLineStart = doc.LineStartByte(500000u);
    for (keel::usize i = 0u; i < 100u; ++i) {
        doc.Insert(0u, "// header\n");
        doc.Insert(midLineStart, "  // mid\n");
        doc.Insert(doc.ByteLength(), "\n// footer");
    }
    const keel::u64 tEditNs = keel::monotonic_ns() - tEditStart;

    CHECK(doc.LineCount() == kLineCount + 300u);
    CHECK(doc.VerifyInvariants());

    // Revert all 300 edits via Undo.
    const keel::u64 tUndoStart = keel::monotonic_ns();
    for (keel::usize i = 0u; i < 300u; ++i) {
        CHECK(doc.Undo().changed);
    }
    const keel::u64 tUndoNs = keel::monotonic_ns() - tUndoStart;
    CHECK(doc.LineCount() == kLineCount);
    CHECK(doc.ByteLength() == big.size());
    CHECK(doc.VerifyInvariants());

    // Benchmark 1,000 random line lookups + slice reads near line 999,999.
    const keel::u64 tQueryStart = keel::monotonic_ns();
    keel::String lineScratch;
    for (keel::usize i = 0u; i < 1000u; ++i) {
        const keel::usize lineIdx = (i * 997u) % kLineCount;
        doc.ReadLineSlice(lineIdx, 0u, 64u, lineScratch);
        const TextLineRange r = doc.LineByteRange(lineIdx);
        const TextLineCol lc = doc.OffsetToLineCol(r.startByte + 5u);
        CHECK(lc.line == lineIdx && lc.colBytes == 5u);
    }
    const keel::u64 tQueryNs = keel::monotonic_ns() - tQueryStart;

    doc.ReadLine(999999u, lineScratch);
    CHECK(lineScratch == "const keel::i32 value_999999 = 42; // ok");

    const keel::f64 loadMs = keel::as<keel::f64>(tLoadNs) * 1e-6;
    const keel::f64 avgEditUs = keel::as<keel::f64>(tEditNs) / (300.0 * 1000.0);
    const keel::f64 avgUndoUs = keel::as<keel::f64>(tUndoNs) / (300.0 * 1000.0);
    const keel::f64 avgQueryUs = keel::as<keel::f64>(tQueryNs) / (1000.0 * 1000.0);
    keel::println(
        "[BENCH 1M lines ({} MB)] load={:.2f} ms | pieces={} | edit={:.2f} us/op | undo={:.2f} us/op | query={:.2f} us/op",
        big.size() / (1024u * 1024u),
        loadMs,
        doc.PieceCount(),
        avgEditUs,
        avgUndoUs,
        avgQueryUs
    );

    // Generous CI safety assertions (order of magnitude above expected <20 us).
    CHECK(avgEditUs < 1000.0);
    CHECK(avgQueryUs < 1000.0);
}

void TestTenMegabyteSingleLineStressAndBenchmark() {
    using namespace guikit;

    constexpr keel::usize kTotalBytes = 10u * 1024u * 1024u;
    keel::String hugeLine;
    hugeLine.reserve(kTotalBytes);
    const keel::StringRef pattern = "{\"id\":12345,\"key\":\"value_abcdef\",\"ok\":true},"; // 44 bytes
    while (hugeLine.size() + pattern.size() <= kTotalBytes) {
        hugeLine.append(pattern);
    }
    while (hugeLine.size() < kTotalBytes) {
        hugeLine.push_back('x');
    }

    const keel::u64 tLoad0 = keel::monotonic_ns();
    TextDocument doc(hugeLine.as_ref());
    const keel::u64 tLoadNs = keel::monotonic_ns() - tLoad0;

    CHECK(doc.LineCount() == 1u);
    CHECK(doc.ByteLength() == kTotalBytes);
    CHECK(doc.MaxLineByteLength() == kTotalBytes);
    CHECK(doc.IsPureAsciiNoTabs());
    CHECK(doc.VerifyInvariants());

    // Read 256-byte horizontal viewport slices at start, middle (5 MB), and tail (10 MB).
    const keel::u64 tSlice0 = keel::monotonic_ns();
    keel::String slice;
    for (keel::usize i = 0u; i < 1000u; ++i) {
        const keel::usize col = (i * 9973u) % (kTotalBytes - 256u);
        doc.ReadLineSlice(0u, col, 256u, slice);
    }
    const keel::u64 tSliceNs = keel::monotonic_ns() - tSlice0;

    doc.ReadLineSlice(0u, 5000000u, 128u, slice);
    CHECK(slice.as_ref() == hugeLine.as_ref().substr(5000000u, 128u));

    // Bounded-window navigation at byte 5,000,000 must stay O(log P) and exact.
    CHECK(doc.NextCharOffset(5000000u) == 5000001u);
    CHECK(doc.PrevCharOffset(5000000u) == 4999999u);
    keel::usize ws = 0;
    keel::usize we = 0;
    doc.WordBoundsAt(5000000u, ws, we);
    CHECK(ws <= 5000000u && we >= 5000000u && (we - ws) <= 64u);

    // Insert & erase in middle of 10 MB line -> MaxLineByteLength updates in O(log P) & reads in O(1)!
    const keel::u64 tMidEdit0 = keel::monotonic_ns();
    doc.Insert(5000000u, "INSERTED_TOKEN");
    const keel::u64 tMidEditNs = keel::monotonic_ns() - tMidEdit0;
    CHECK(doc.LineCount() == 1u);
    CHECK(doc.MaxLineByteLength() == kTotalBytes + 14u);
    CHECK(doc.VerifyInvariants());

    doc.ReadLineSlice(0u, 5000000u, 14u, slice);
    CHECK(slice == "INSERTED_TOKEN");

    // Split the 10 MB line into two 5 MB lines via a single '\n' insert -> MaxLineByteLength drops immediately!
    doc.Insert(5000000u, "\n");
    CHECK(doc.LineCount() == 2u);
    CHECK(doc.MaxLineByteLength() == (kTotalBytes - 5000000u) + 14u);
    CHECK(doc.VerifyInvariants());

    // Undo both edits -> restores original 10 MB single line!
    CHECK(doc.Undo().changed);
    CHECK(doc.LineCount() == 1u);
    CHECK(doc.MaxLineByteLength() == kTotalBytes + 14u);
    CHECK(doc.Undo().changed);
    CHECK(doc.MaxLineByteLength() == kTotalBytes);
    CHECK(doc.VerifyInvariants());

    const keel::f64 loadMs = keel::as<keel::f64>(tLoadNs) * 1e-6;
    const keel::f64 avgSliceUs = keel::as<keel::f64>(tSliceNs) / (1000.0 * 1000.0);
    const keel::f64 midEditUs = keel::as<keel::f64>(tMidEditNs) * 1e-3;
    keel::println(
        "[BENCH 10 MB single line] load={:.2f} ms | pieces={} | 256B slice={:.2f} us/op | mid-insert={:.2f} us",
        loadMs,
        doc.PieceCount(),
        avgSliceUs,
        midEditUs
    );

    CHECK(avgSliceUs < 500.0);
    CHECK(midEditUs < 1000.0);
}

void TestCodeEditorDocumentInteractionAndMassiveRender() {
    using namespace guikit;

    // 1. Functional UI test: TextArea(ui, TextDocument&) editing, auto-close, Tab, Ctrl+/, Undo/Redo, Find & Replace All
    {
        Context ui;
        NullCanvas canvas;
        TextDocument doc("int main() {\n    return 0;\n}");
        TextAreaResponse resp{};

        auto runFrame = [&]() {
            ui.BeginFrame({960.0f, 640.0f});
            resp = guikit::CodeEditor(ui, doc, {
                .syntax = SyntaxConfig::Cpp(),
                .height = 420.0f,
                .showMinimap = true,
                .showStatusBar = true,
                .codeFolding = true,
                .enableSearch = true
            });
            ui.EndFrame(canvas);
        };

        runFrame();
        runFrame(); // Frame 2 populates previous-frame hit bounds in immediate mode
        CHECK(resp.totalLines == 3u);
        CHECK(resp.rect.w > 200.0f && resp.rect.h > 200.0f);

        // Focus by clicking inside editor, then move to start and type
        ui.OnMouseMove(resp.rect.Center());
        ui.OnMouseDown(1);
        runFrame();
        ui.OnMouseUp(1);
        CHECK(resp.focused);
        CHECK(ui.WantsTextInput());

        // Move to doc start and type "// top\n"
        ui.OnKeyDown(Key::Home, {.ctrl = true});
        runFrame();
        ui.OnTextInput("// top");
        runFrame();
        ui.OnKeyDown(Key::Enter, {});
        runFrame();
        CHECK(resp.changed);
        CHECK(doc.LineCount() == 4u);
        keel::String line0;
        doc.ReadLine(0u, line0);
        CHECK(line0 == "// top");

        // Undo reverts Enter, then reverts "// top"
        ui.OnKeyDown(Key::Z, {.ctrl = true});
        runFrame();
        CHECK(doc.LineCount() == 3u);
        ui.OnKeyDown(Key::Z, {.ctrl = true});
        runFrame();
        CHECK(doc.ToString() == "int main() {\n    return 0;\n}");

        // Find + Replace All via Ctrl+H
        ui.OnKeyDown(Key::H, {.ctrl = true});
        runFrame();
        ui.OnTextInput("return");
        runFrame();
        CHECK(resp.searchMatchCount == 1u);
        // Switch focus to Replace input via Tab, type "yield_", Ctrl+Alt+Enter to Replace All
        ui.OnKeyDown(Key::Tab, {});
        runFrame();
        ui.OnTextInput("yield_");
        ui.OnKeyDown(Key::Enter, {.ctrl = true, .alt = true});
        runFrame();
        CHECK(doc.ToString() == "int main() {\n    yield_ 0;\n}");

        // Escape closes overlay, then Ctrl+Z atomically undoes Replace All!
        ui.OnKeyDown(Key::Escape, {});
        runFrame();
        ui.OnKeyDown(Key::Z, {.ctrl = true});
        runFrame();
        CHECK(doc.ToString() == "int main() {\n    return 0;\n}");
    }

    // 2. End-to-end 1,000,000-line full UI layout + scroll to line 999,990 + NullCanvas render benchmark!
    {
        constexpr keel::usize kLineCount = 1000000u;
        keel::String big;
        big.reserve(kLineCount * 42u);
        for (keel::usize i = 0u; i < kLineCount; ++i) {
            big.append("const keel::i32 row_");
            char numBuf[16];
            keel::usize n = i;
            keel::usize digits = 0u;
            do {
                numBuf[digits++] = keel::as<char>('0' + (n % 10u));
                n /= 10u;
            } while (n > 0u);
            while (digits > 0u) big.push_back(numBuf[--digits]);
            big.append(" = 123;");
            if (i + 1u < kLineCount) big.push_back('\n');
        }

        TextDocument doc(big.as_ref());
        Context ui;
        NullCanvas canvas;
        TextAreaResponse resp{};
        const TextAreaOptions opts{
            .syntax = SyntaxConfig::Cpp(),
            .height = 600.0f,
            .showMinimap = true,
            .showStatusBar = true,
            .enableSearch = true
        };

        auto renderFrame = [&]() {
            ui.BeginFrame({1280.0f, 720.0f});
            resp = guikit::CodeEditor(ui, doc, opts);
            ui.EndFrame(canvas);
        };

        renderFrame();
        CHECK(resp.totalLines == kLineCount);

        // Jump scrollY to line ~999,980 (f64 precision > 20,000,000 dp!) and benchmark 20 scroll+render frames!
        auto& st = ui.GetOrCreatePersistentState<TextAreaState>(resp.id);
        st.scrollY = 999980.0 * 20.0;
        renderFrame();
        CHECK(st.scrollY > 15000000.0); // Proves f64 scroll handles > 2^24 dp cleanly!

        const keel::u64 t0 = keel::monotonic_ns();
        for (keel::i32 f = 0; f < 20; ++f) {
            st.scrollY -= 21.0;
            renderFrame();
        }
        const keel::u64 elapsedNs = keel::monotonic_ns() - t0;
        const keel::f64 msPerFrame = (keel::as<keel::f64>(elapsedNs) * 1e-6) / 20.0;
        keel::println("[BENCH UI 1M lines @ line 999,980] {:.3f} ms/frame (scroll + layout + syntax + minimap + NullCanvas)", msPerFrame);
        CHECK(msPerFrame < 15.0);

        // Also test 1M lines with wordWrap = true in a NARROW 260dp viewport (forces REAL 2M+ wrapped visual rows!)
        TextAreaOptions wrapOpts = opts;
        wrapOpts.wordWrap = true;
        wrapOpts.width = 260.0f;
        for (keel::i32 w = 0; w < 2; ++w) {
            ui.BeginFrame({1280.0f, 720.0f});
            resp = guikit::CodeEditor(ui, doc, wrapOpts);
            ui.EndFrame(canvas);
        }
        const keel::u64 tw0 = keel::monotonic_ns();
        for (keel::i32 f = 0; f < 10; ++f) {
            st.scrollY -= 21.0;
            ui.BeginFrame({1280.0f, 720.0f});
            resp = guikit::CodeEditor(ui, doc, wrapOpts);
            ui.EndFrame(canvas);
        }
        const keel::f64 wrapMs = (keel::as<keel::f64>(keel::monotonic_ns() - tw0) * 1e-6) / 10.0;
        keel::println("[BENCH UI 1M lines wordWrap=ON (narrow 260dp -> ~2M visual rows)] {:.3f} ms/frame", wrapMs);
        CHECK(wrapMs < 15.0);
    }

    // 3. End-to-end 10 MB single-line UI render & horizontal scroll (col ~5,000,000!) + wordWrap=ON!
    {
        constexpr keel::usize kTotalBytes = 10u * 1024u * 1024u;
        keel::String hugeLine;
        hugeLine.reserve(kTotalBytes);
        const keel::StringRef pat = "{\"id\":12345,\"name\":\"guikit_editor\",\"ok\":true},";
        while (hugeLine.size() + pat.size() <= kTotalBytes) hugeLine.append(pat);
        while (hugeLine.size() < kTotalBytes) hugeLine.push_back('z');

        TextDocument doc(hugeLine.as_ref());
        Context ui;
        NullCanvas canvas;
        TextAreaResponse resp{};
        const TextAreaOptions opts{
            .wordWrap = false,
            .syntax = SyntaxConfig::Json(),
            .height = 500.0f,
            .showMinimap = true,
            .showStatusBar = true
        };

        ui.BeginFrame({1280.0f, 720.0f});
        resp = guikit::CodeEditor(ui, doc, opts);
        ui.EndFrame(canvas);
        CHECK(resp.totalLines == 1u);

        auto& st = ui.GetOrCreatePersistentState<TextAreaState>(resp.id);
        st.scrollX = 5000000.0 * 7.8; // Scroll horizontally to column 5,000,000 (> 39,000,000 dp!)
        const keel::u64 t0 = keel::monotonic_ns();
        for (keel::i32 f = 0; f < 20; ++f) {
            st.scrollX += 16.0;
            ui.BeginFrame({1280.0f, 720.0f});
            resp = guikit::CodeEditor(ui, doc, opts);
            ui.EndFrame(canvas);
        }
        const keel::f64 hMs = (keel::as<keel::f64>(keel::monotonic_ns() - t0) * 1e-6) / 20.0;
        CHECK(st.scrollX > 30000000.0); // Proves f64 scrollX works past 2^24 dp!
        keel::println("[BENCH UI 10 MB single line @ col 5M (wordWrap=OFF)] {:.3f} ms/frame", hMs);
        CHECK(hMs < 15.0);

        // Switch to wordWrap = true on the 10 MB single line -> AnalyticSmallWrap (~80,000+ visual rows, 16 bytes RAM)!
        TextAreaOptions wrapOpts = opts;
        wrapOpts.wordWrap = true;
        st.scrollX = 0.0;
        st.scrollY = 50000.0 * 20.0;
        const keel::u64 tw0 = keel::monotonic_ns();
        for (keel::i32 f = 0; f < 20; ++f) {
            st.scrollY += 21.0;
            ui.BeginFrame({1280.0f, 720.0f});
            resp = guikit::CodeEditor(ui, doc, wrapOpts);
            ui.EndFrame(canvas);
        }
        const keel::f64 wMs = (keel::as<keel::f64>(keel::monotonic_ns() - tw0) * 1e-6) / 20.0;
        keel::println("[BENCH UI 10 MB single line (wordWrap=ON @ row 50k)] {:.3f} ms/frame", wMs);
        CHECK(wMs < 15.0);
    }
}

void TestCheckpointedLexerAndWindowedFolding() {
    using namespace guikit;

    // Build a 100,000-line C++ document with:
    //  - a 2,000-line multiline /* ... */ comment from line 10,000 to line 12,000 (0-based)
    //  - foldable functions around line 80,000
    constexpr keel::usize kLines = 100000u;
    keel::String src;
    src.reserve(kLines * 36u);
    for (keel::usize i = 0u; i < kLines; ++i) {
        if (i == 10000u) {
            src.append("/* start huge block comment\n");
        } else if (i == 12000u) {
            src.append("   end huge block comment */\n");
        } else if (i == 11500u) {
            src.append("int TARGET_INSIDE_COMMENT = 42;\n");
        } else if (i == 12100u) {
            src.append("int TARGET_AFTER_COMMENT = 7;\n");
        } else if (i == 80000u) {
            src.append("void HugeFileFunction() {\n");
        } else if (i == 80005u) {
            src.append("}\n");
        } else {
            src.append("auto x = 1; // regular line\n");
        }
    }

    TextDocument doc(src.as_ref());
    CHECK(doc.LineCount() == kLines + 1u);

    class ColorSpyCanvas : public NullCanvas {
    public:
        keel::String targetToken;
        Color observedColor{};
        bool foundTarget{false};

        void ResetProbe(keel::StringRef tok) {
            targetToken.assign(tok);
            observedColor = Color::Transparent();
            foundTarget = false;
        }

        void DrawTextStyled(keel::StringRef text, Vec2 pos, keel::f32 fontSize, Color color, FontStyle style, keel::f32 angleDeg = 0.0f) override {
            if (!foundTarget && text == targetToken) {
                observedColor = color;
                foundTarget = true;
            }
            NullCanvas::DrawTextStyled(text, pos, fontSize, color, style, angleDeg);
        }
    };

    Context ui;
    ColorSpyCanvas spy;
    const UiTheme theme = UiTheme::Dark();
    ui.SetTheme(theme);

    const TextAreaOptions opts{
        .syntax = SyntaxConfig::Cpp(),
        .height = 500.0f,
        .codeFolding = true
    };
    TextAreaResponse resp{};

    // Single call-site guarantees identical SourceLocation -> stable widget Id across frames.
    auto drawFrame = [&]() {
        ui.BeginFrame({1024.0f, 640.0f});
        resp = guikit::CodeEditor(ui, doc, opts);
        ui.EndFrame(spy);
    };
    drawFrame();
    drawFrame();

    const keel::f32 baseFs = theme.GetTypography(TextRole::CodeMedium).fontSize;
    const keel::f32 lh = keel::max(theme.GetLineHeight(baseFs), spy.MeasureTextStyled("Mg", baseFs, FontStyle::Monospace).y * 1.14f);

    auto renderAtLine = [&](keel::usize line0, keel::StringRef probe) {
        auto& st = ui.GetOrCreatePersistentState<TextAreaState>(resp.id);
        st.scrollY = keel::as<keel::f64>(line0 > 2u ? line0 - 2u : 0u) * keel::as<keel::f64>(lh);
        spy.ResetProbe(probe);
        drawFrame();
    };

    // 1. Line 11,500 is 1,500 lines deep inside /* ... */ -> MUST be colored as comment (proves checkpoints!)
    renderAtLine(11500u, "int TARGET_INSIDE_COMMENT = 42;");
    CHECK(spy.foundTarget);
    CHECK(spy.observedColor == theme.syntax.comment);

    // 2. Line 12,100 is AFTER */ -> "int" MUST be colored as type (NOT comment!)
    renderAtLine(12100u, "int");
    CHECK(spy.foundTarget);
    CHECK(spy.observedColor == theme.syntax.type);

    // 3. Remove "/*" at line 10,000 -> incremental checkpoint invalidation turns line 11,500 into code!
    const TextLineRange openLine = doc.LineByteRange(10000u);
    doc.ReplaceRange(openLine.startByte, 2u, "//");
    renderAtLine(11500u, "TARGET_INSIDE_COMMENT");
    CHECK(spy.foundTarget);
    CHECK(spy.observedColor == theme.syntax.identifier);

    // 4. Undo restores "/*" -> line 11,500 immediately turns back into comment!
    CHECK(doc.Undo().changed);
    renderAtLine(11500u, "int TARGET_INSIDE_COMMENT = 42;");
    CHECK(spy.foundTarget);
    CHECK(spy.observedColor == theme.syntax.comment);

    // 5. Windowed Code Folding at line 80,000 on a 100,000-line file!
    renderAtLine(80000u, "HugeFileFunction");
    CHECK(spy.foundTarget);
    CHECK(spy.observedColor == theme.syntax.function);
    auto* st = ui.FindState<TextAreaState>(resp.id);
    CHECK(st != nullptr);
    bool foundFoldAt80000 = false;
    if (st) {
        for (const TextAreaFoldRegion& fr : st->cachedFoldRegions) {
            if (fr.startLine == 80000u && fr.endLine == 80005u) {
                foundFoldAt80000 = true;
                break;
            }
        }
    }
    CHECK(foundFoldAt80000);

    // 6. Collapse fold at line 80,000 and scroll far away -> collapsed fold MUST remain preserved!
    {
        auto& mutSt = ui.GetOrCreatePersistentState<TextAreaState>(resp.id);
        mutSt.foldedLines.push_back(80000u);
        mutSt.preservedFoldedRegions.push_back(TextAreaFoldRegion{80000u, 80005u});
    }
    renderAtLine(1000u, "auto");
    const auto* stAfter = ui.FindState<TextAreaState>(resp.id);
    bool preservedStillActive = false;
    if (stAfter) {
        for (keel::usize fl : stAfter->foldedLines) {
            if (fl == 80000u) {
                preservedStillActive = true;
                break;
            }
        }
    }
    CHECK(preservedStillActive);
}

void TestBuiltInLanguagePresetsAndHighlighting() {
    using namespace guikit;
    const UiTheme theme = UiTheme::Dark();
    const SyntaxPalette& pal = theme.syntax;

    auto findSpanColor = [&](keel::StringRef line, const SyntaxConfig& cfg, keel::StringRef needle, bool* inOutBlock = nullptr) -> Color {
        keel::Array<TextSpan> spans;
        detail::TextArea::HighlightLine(line, 0u, cfg, spans, theme, nullptr, inOutBlock);
        for (const TextSpan& sp : spans) {
            if (sp.startCol + sp.length <= line.size() && line.substr(sp.startCol, sp.length) == needle) {
                return sp.color;
            }
        }
        return Color::Transparent();
    };

    // 1. Verify ForLanguage & ForFilename resolution across all 22 built-in presets
    CHECK(SyntaxConfig::ForLanguage("cpp").languageName == "C++20");
    CHECK(SyntaxConfig::ForLanguage("matlab").languageName == "MATLAB");
    CHECK(SyntaxConfig::ForLanguage("json").languageName == "JSON");
    CHECK(SyntaxConfig::ForLanguage("py").languageName == "Python");
    CHECK(SyntaxConfig::ForLanguage("rs").languageName == "Rust");
    CHECK(SyntaxConfig::ForLanguage("tsx").languageName == "TypeScript");
    CHECK(SyntaxConfig::ForLanguage("js").languageName == "JavaScript");
    CHECK(SyntaxConfig::ForLanguage("frag").languageName == "GLSL");
    CHECK(SyntaxConfig::ForLanguage("go").languageName == "Go");
    CHECK(SyntaxConfig::ForLanguage("csharp").languageName == "C#");
    CHECK(SyntaxConfig::ForLanguage("java").languageName == "Java");
    CHECK(SyntaxConfig::ForLanguage("kt").languageName == "Kotlin");
    CHECK(SyntaxConfig::ForLanguage("swift").languageName == "Swift");
    CHECK(SyntaxConfig::ForLanguage("sv").languageName == "SystemVerilog");
    CHECK(SyntaxConfig::ForLanguage("vhdl").languageName == "VHDL");
    CHECK(SyntaxConfig::ForLanguage("lua").languageName == "Lua");
    CHECK(SyntaxConfig::ForLanguage("cmake").languageName == "CMake");
    CHECK(SyntaxConfig::ForLanguage("bash").languageName == "Shell");
    CHECK(SyntaxConfig::ForLanguage("pwsh").languageName == "PowerShell");
    CHECK(SyntaxConfig::ForLanguage("cmd").languageName == "Batch");
    CHECK(SyntaxConfig::ForLanguage("postgres").languageName == "SQL");
    CHECK(SyntaxConfig::ForLanguage("yml").languageName == "YAML");
    CHECK(SyntaxConfig::ForLanguage("scss").languageName == "CSS");
    CHECK(SyntaxConfig::ForLanguage("bpmn").languageName == "XML");
    CHECK(!SyntaxConfig::ForLanguage("plain").enabled);

    CHECK(SyntaxConfig::ForFilename("/src/rtl/alu.sv").languageName == "SystemVerilog");
    CHECK(SyntaxConfig::ForFilename("C:\\fpga\\fifo.vhd").languageName == "VHDL");
    CHECK(SyntaxConfig::ForFilename("app/MainActivity.kt").languageName == "Kotlin");
    CHECK(SyntaxConfig::ForFilename("Sources/App.swift").languageName == "Swift");
    CHECK(SyntaxConfig::ForFilename("web/styles.css").languageName == "CSS");
    CHECK(SyntaxConfig::ForFilename("diagrams/workflow.bpmn").languageName == "XML");
    CHECK(SyntaxConfig::ForFilename("project/CMakeLists.txt").languageName == "CMake");
    CHECK(SyntaxConfig::ForFilename("shaders/sonar.frag").languageName == "GLSL");
    CHECK(SyntaxConfig::ForFilename("scripts/deploy.ps1").languageName == "PowerShell");
    CHECK(SyntaxConfig::ForFilename("scripts\\windows\\native-run.cmd").languageName == "Batch");
    CHECK(SyntaxConfig::ForFilename("build.bat").languageName == "Batch");
    CHECK(!SyntaxConfig::ForFilename("README.unknown").enabled);

    // 2. Rust: lifetimes ('a, 'static) vs char literals ('x', '\n')
    {
        const SyntaxConfig rs = SyntaxConfig::Rust();
        const keel::StringRef line = "pub fn pick<'a>(s: &'a str, ch: char) -> char { 'x' }";
        CHECK(findSpanColor(line, rs, "pub") == pal.keyword);
        CHECK(findSpanColor(line, rs, "fn") == pal.keyword);
        CHECK(findSpanColor(line, rs, "'a") == pal.type);
        CHECK(findSpanColor(line, rs, "str") == pal.type);
        CHECK(findSpanColor(line, rs, "'x'") == pal.string);
    }

    // 3. Verilog / SystemVerilog: `define, always_ff, logic, 8'hFF, $display
    {
        const SyntaxConfig sv = SyntaxConfig::Verilog();
        CHECK(findSpanColor("`define WIDTH 8", sv, "`define WIDTH 8") == pal.preprocessor);
        const keel::StringRef line = "always_ff @(posedge clk) logic [7:0] d = 8'hFF; $display(\"ok\");";
        CHECK(findSpanColor(line, sv, "always_ff") == pal.keyword);
        CHECK(findSpanColor(line, sv, "posedge") == pal.keyword);
        CHECK(findSpanColor(line, sv, "logic") == pal.type);
        CHECK(findSpanColor(line, sv, "8'hFF") == pal.number);
        CHECK(findSpanColor(line, sv, "$display") == pal.function);
    }

    // 4. VHDL: case-insensitive keywords/types + clk'event apostrophe disambiguation + '1' bit literal
    {
        const SyntaxConfig vhd = SyntaxConfig::Vhdl();
        const keel::StringRef line = "IF rising_edge(clk) AND clk'event THEN q <= STD_LOGIC_VECTOR('1'); END IF;";
        CHECK(findSpanColor(line, vhd, "IF") == pal.keyword);
        CHECK(findSpanColor(line, vhd, "AND") == pal.keyword);
        CHECK(findSpanColor(line, vhd, "END") == pal.keyword);
        CHECK(findSpanColor(line, vhd, "rising_edge") == pal.function);
        CHECK(findSpanColor(line, vhd, "event") == pal.function);
        CHECK(findSpanColor(line, vhd, "STD_LOGIC_VECTOR") == pal.type);
        CHECK(findSpanColor(line, vhd, "'1'") == pal.string);
    }

    // 5. Kotlin: @Composable, suspend fun, StateFlow, remember
    {
        const SyntaxConfig kt = SyntaxConfig::Kotlin();
        CHECK(findSpanColor("@Composable", kt, "@Composable") == pal.preprocessor);
        const keel::StringRef line = "suspend fun render(s: StateFlow<Int>) = when (val v = remember()) { else -> v }";
        CHECK(findSpanColor(line, kt, "suspend") == pal.keyword);
        CHECK(findSpanColor(line, kt, "fun") == pal.keyword);
        CHECK(findSpanColor(line, kt, "when") == pal.keyword);
        CHECK(findSpanColor(line, kt, "StateFlow") == pal.type);
        CHECK(findSpanColor(line, kt, "remember") == pal.function);
    }

    // 6. Swift: #if, guard let, actor, async, Optional
    {
        const SyntaxConfig sw = SyntaxConfig::Swift();
        CHECK(findSpanColor("#if os(macOS)", sw, "#if os(macOS)") == pal.preprocessor);
        const keel::StringRef line = "actor Loader { func fetch() async -> Optional<URL> { guard let u = nil else { return nil } } }";
        CHECK(findSpanColor(line, sw, "actor") == pal.keyword);
        CHECK(findSpanColor(line, sw, "guard") == pal.keyword);
        CHECK(findSpanColor(line, sw, "async") == pal.keyword);
        CHECK(findSpanColor(line, sw, "Optional") == pal.type);
        CHECK(findSpanColor(line, sw, "URL") == pal.type);
    }

    // 7. CSS: @media, hyphenated properties, #hex colors, calc(), and pseudo-class safety (.btn:hover)
    {
        const SyntaxConfig css = SyntaxConfig::Css();
        CHECK(findSpanColor("@media (min-width: 768px) {", css, "@media (min-width: 768px) {") == pal.preprocessor);
        const keel::StringRef propLine = "  background-color: #1e293b; display: flex; width: calc(100% - 16px);";
        CHECK(findSpanColor(propLine, css, "background-color") == pal.type);
        CHECK(findSpanColor(propLine, css, "#1e293b") == pal.number);
        CHECK(findSpanColor(propLine, css, "flex") == pal.keyword);
        CHECK(findSpanColor(propLine, css, "calc") == pal.function);
        // .btn:hover must NOT highlight "btn" as a property key!
        CHECK(findSpanColor(".btn:hover { custom-prop: 12px; }", css, "btn") == pal.identifier);
        CHECK(findSpanColor(".btn:hover { custom-prop: 12px; }", css, "custom-prop") == pal.type);
    }

    // 8. XML / HTML / SVG / BPMN: tags, attributes, values, entities, CDATA, multiline <!-- ... -->
    {
        const SyntaxConfig xml = SyntaxConfig::Xml();
        const keel::StringRef prolog = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
        CHECK(findSpanColor(prolog, xml, "xml") == pal.preprocessor);
        CHECK(findSpanColor(prolog, xml, "version") == pal.type);
        CHECK(findSpanColor(prolog, xml, "\"1.0\"") == pal.string);

        const keel::StringRef elem = "<bpmn:startEvent id=\"s1\" data-testid=\"ev-1\">Start &amp; Go</bpmn:startEvent>";
        CHECK(findSpanColor(elem, xml, "bpmn:startEvent") == pal.keyword);
        CHECK(findSpanColor(elem, xml, "data-testid") == pal.type);
        CHECK(findSpanColor(elem, xml, "\"ev-1\"") == pal.string);
        CHECK(findSpanColor(elem, xml, "&amp;") == pal.number);

        bool inComment = false;
        CHECK(findSpanColor("<!-- open multiline comment", xml, "<!-- open multiline comment", &inComment) == pal.comment);
        CHECK(inComment == true);
        CHECK(findSpanColor("  <stillCommented/>", xml, "  <stillCommented/>", &inComment) == pal.comment);
        CHECK(inComment == true);
        CHECK(findSpanColor("close --> <realTag/>", xml, "close -->", &inComment) == pal.comment);
        CHECK(inComment == false);
        CHECK(findSpanColor("close --> <realTag/>", xml, "realTag") == pal.keyword);
    }

    // 9. SQL & CMake case-insensitive + TypeScript backtick strings + YAML unquoted hyphen keys
    {
        const SyntaxConfig sql = SyntaxConfig::Sql();
        const keel::StringRef q = "select COALESCE(name, 'x') from users where id = 1;";
        CHECK(findSpanColor(q, sql, "select") == pal.keyword);
        CHECK(findSpanColor(q, sql, "COALESCE") == pal.function);
        CHECK(findSpanColor(q, sql, "from") == pal.keyword);

        const SyntaxConfig cm = SyntaxConfig::Cmake();
        CHECK(findSpanColor("TARGET_LINK_LIBRARIES(app PRIVATE guikit::core)", cm, "TARGET_LINK_LIBRARIES") == pal.function);
        CHECK(findSpanColor("TARGET_LINK_LIBRARIES(app PRIVATE guikit::core)", cm, "PRIVATE") == pal.type);

        const SyntaxConfig ts = SyntaxConfig::TypeScript();
        bool inBlk = false;
        CHECK(findSpanColor("const s: string = `/* not a comment */`;", ts, "`/* not a comment */`", &inBlk) == pal.string);
        CHECK(inBlk == false);

        const SyntaxConfig yml = SyntaxConfig::Yaml();
        CHECK(findSpanColor("max-retries: 5", yml, "max-retries") == pal.type);
        CHECK(findSpanColor("enabled: true", yml, "true") == pal.keyword);
    }

    // 10. PowerShell: case-insensitive cmdlets (Verb-Noun), $PSScriptRoot, <# ... #> multiline comments
    {
        const SyntaxConfig ps = SyntaxConfig::PowerShell();
        const keel::StringRef line = "Function Build-App { param([string]$Root = $PSScriptRoot) Write-Host \"OK\" }";
        CHECK(findSpanColor(line, ps, "Function") == pal.keyword);
        CHECK(findSpanColor(line, ps, "param") == pal.keyword);
        CHECK(findSpanColor(line, ps, "string") == pal.type);
        CHECK(findSpanColor(line, ps, "$PSScriptRoot") == pal.function);
        CHECK(findSpanColor(line, ps, "Write-Host") == pal.function);

        bool inPsBlk = false;
        CHECK(findSpanColor("<# multiline ps comment", ps, "<# multiline ps comment", &inPsBlk) == pal.comment);
        CHECK(inPsBlk == true);
        keel::Array<TextSpan> psSpans;
        const keel::StringRef closePsLine = "still commented #> Write-Output $true";
        detail::TextArea::HighlightLine(closePsLine, 0u, ps, psSpans, theme, nullptr, &inPsBlk);
        CHECK(inPsBlk == false);
        auto findInPs = [&](keel::StringRef needle) -> Color {
            for (const TextSpan& sp : psSpans) {
                if (sp.startCol + sp.length <= closePsLine.size() && closePsLine.substr(sp.startCol, sp.length) == needle) {
                    return sp.color;
                }
            }
            return Color::Transparent();
        };
        CHECK(findInPs("still commented #>") == pal.comment);
        CHECK(findInPs("Write-Output") == pal.function);
        CHECK(findInPs("$true") == pal.function);
    }

    // 11. Windows Batch / CMD: @echo off, :: comment, REM full-line comment, setlocal/EnableDelayedExpansion
    {
        const SyntaxConfig bat = SyntaxConfig::Batch();
        CHECK(findSpanColor("@echo off", bat, "@echo off") == pal.preprocessor);
        CHECK(findSpanColor(":: double-colon comment", bat, ":: double-colon comment") == pal.comment);
        CHECK(findSpanColor("  REM if not exist set goto", bat, "REM if not exist set goto") == pal.comment);
        const keel::StringRef line = "setlocal EnableDelayedExpansion & if not exist \"out\" mkdir out";
        CHECK(findSpanColor(line, bat, "setlocal") == pal.keyword);
        CHECK(findSpanColor(line, bat, "EnableDelayedExpansion") == pal.type);
        CHECK(findSpanColor(line, bat, "if") == pal.keyword);
        CHECK(findSpanColor(line, bat, "not") == pal.keyword);
        CHECK(findSpanColor(line, bat, "exist") == pal.keyword);
        CHECK(findSpanColor(line, bat, "mkdir") == pal.function);
    }
}

void TestCodeFoldingWithCursorInsideBlock() {
    using namespace guikit;

    // Function block spanning lines 0..4 (0-based):
    //   0: void Foo() {
    //   1:     int a = 1;
    //   2:     int b = 2;
    //   3:     int c = a + b;
    //   4: }
    //   5: int after = 0;
    TextDocument doc(
        "void Foo() {\n"
        "    int a = 1;\n"
        "    int b = 2;\n"
        "    int c = a + b;\n"
        "}\n"
        "int after = 0;\n"
    );

    Context ui;
    NullCanvas canvas;
    const UiTheme theme = UiTheme::Dark();
    ui.SetTheme(theme);

    const TextAreaOptions opts{
        .showLineNumbers = true,
        .syntax = SyntaxConfig::Cpp(),
        .height = 300.0f,
        .codeFolding = true
    };
    TextAreaResponse resp{};

    auto drawFrame = [&]() {
        ui.BeginFrame({800.0f, 400.0f});
        resp = guikit::CodeEditor(ui, doc, opts);
        ui.EndFrame(canvas);
    };

    // Warm up layout
    drawFrame();
    drawFrame();

    // Place focus and cursor INSIDE the foldable block (on line 2: "    int b = 2;")
    const keel::usize insideOffset = doc.LineColToOffset(2u, 4u);
    {
        auto& st = ui.GetOrCreatePersistentState<TextAreaState>(resp.id);
        st.cursor = insideOffset;
        st.anchor = insideOffset;
        st.selectionStart = insideOffset;
        st.selectionEnd = insideOffset;
        st.prevCursor = insideOffset;
    }
    ui.RequestFocus(resp.id);
    drawFrame();
    CHECK(resp.focused);
    CHECK(resp.cursorLine == 3u); // 1-based line 3 == 0-based line 2

    // Compute exact fold chevron hit coordinates on line 0
    const keel::f32 baseFs = theme.GetTypography(TextRole::CodeMedium).fontSize;
    const keel::f32 spW = canvas.MeasureTextStyled(" ", baseFs, FontStyle::Monospace).x;
    const keel::f32 digW = canvas.MeasureTextStyled("8", baseFs, FontStyle::Monospace).x;
    const keel::f32 foldColW = keel::max(14.0f, spW * 1.85f);
    const keel::f32 gutterW = 2.0f * digW + spW * 2.5f + foldColW;
    const keel::f32 padY = keel::max(8.0f, theme.GetMetrics(UiSize::Md).paddingY);
    const keel::f32 lh = keel::max(theme.GetLineHeight(baseFs), canvas.MeasureTextStyled("Mg", baseFs, FontStyle::Monospace).y * 1.14f);
    const Vec2 chevronPos{
        resp.rect.x + 1.0f + gutterW - foldColW * 0.5f,
        resp.rect.y + 1.0f + padY + lh * 0.5f
    };

    // Click fold chevron while cursor is INSIDE lines 1..4 -> MUST collapse AND relocate cursor to line 0!
    ui.OnMouseMove(chevronPos);
    ui.OnMouseDown(1);
    drawFrame();
    ui.OnMouseUp(1);
    drawFrame();

    const auto* st = ui.FindState<TextAreaState>(resp.id);
    CHECK(st != nullptr);
    CHECK(st != nullptr && st->foldedLines.size() == 1u && st->foldedLines[0] == 0u);
    CHECK(resp.cursorLine == 1u); // Relocated cleanly to header line 1 (0-based line 0)

    // Render another idle frame while focused -> MUST STAY FOLDED!
    drawFrame();
    CHECK(st != nullptr && st->foldedLines.size() == 1u && st->foldedLines[0] == 0u);

    // Pressing Down arrow from folded line 0 MUST jump cleanly over hidden lines 1..4 to line 5 (1-based line 6)
    // WITHOUT unfolding the block!
    ui.OnKeyDown(Key::Down);
    drawFrame();
    CHECK(resp.cursorLine == 6u);
    CHECK(st != nullptr && st->foldedLines.size() == 1u && st->foldedLines[0] == 0u);

    // Pressing Up arrow from line 5 MUST jump cleanly back to line 0 (1-based line 1) WITHOUT unfolding!
    ui.OnKeyDown(Key::Up);
    drawFrame();
    CHECK(resp.cursorLine == 1u);
    CHECK(st != nullptr && st->foldedLines.size() == 1u && st->foldedLines[0] == 0u);

    // Click chevron again -> MUST unfold cleanly!
    ui.OnMouseMove(chevronPos);
    ui.OnMouseDown(1);
    drawFrame();
    ui.OnMouseUp(1);
    drawFrame();
    CHECK(st != nullptr && st->foldedLines.empty());
}

} // namespace

int main() {
    TestEmptyAndBasicOperations();
    TestBalancedBuildAcrossChunkCounts();
    TestCrlfUtf8AndChunkBoundaries();
    TestUndoRedoEditGroupsAndCoalescing();
    TestDeltaLogAndOffsetMapping();
    TestRandomizedFuzzAgainstOracle();
    TestOneMillionLinesStressAndBenchmark();
    TestTenMegabyteSingleLineStressAndBenchmark();
    TestCodeEditorDocumentInteractionAndMassiveRender();
    TestCheckpointedLexerAndWindowedFolding();
    TestBuiltInLanguagePresetsAndHighlighting();
    TestCodeFoldingWithCursorInsideBlock();

    if (g_failures == 0) {
        keel::println("[PASS] All TextDocument tests passed (0 failures).");
        return 0;
    }
    keel::eprintln("[FAIL] TextDocument tests failed with {} failure(s).", g_failures);
    return 1;
}
