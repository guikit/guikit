#include "guikit/core/text_document.hpp"

#include "keel/memory.h"
#include "keel/utf8.h"

namespace guikit {

namespace {

constexpr keel::u8 kFlagNonAscii = 1u << 0;
constexpr keel::u8 kFlagTabOrCtrl = 1u << 1;

[[nodiscard]] keel::u8 ScanSliceFlags(
    const char* data,
    keel::u32 start,
    keel::u32 length
) noexcept {
    keel::u8 flags = 0;
    const keel::u32 end = start + length;
    for (keel::u32 i = start; i < end; ++i) {
        const keel::u8 b = keel::as<keel::u8>(data[i]);
        if (b >= 0x80u) {
            flags |= kFlagNonAscii;
        } else if (b < 0x20u || b == 0x7Fu) {
            if (b == '\n') {
                continue;
            }
            if (b == '\r' && i + 1u < end && data[i + 1u] == '\n') {
                continue;
            }
            flags |= kFlagTabOrCtrl;
        }
        if (flags == (kFlagNonAscii | kFlagTabOrCtrl)) {
            break;
        }
    }
    return flags;
}

[[nodiscard]] keel::u32 LowerBoundU32(keel::ArrayRef<keel::u32> arr, keel::u32 lo, keel::u32 hi, keel::u32 target) noexcept {
    while (lo < hi) {
        const keel::u32 mid = lo + (hi - lo) / 2u;
        if (arr[mid] < target) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    return lo;
}

[[nodiscard]] keel::usize CountLineFeeds(keel::StringRef text) noexcept {
    keel::usize count = 0;
    const char* p = text.data();
    const keel::usize n = text.size();
    for (keel::usize i = 0; i < n; ++i) {
        if (p[i] == '\n') {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] keel::u32 FloorLog2(keel::usize n) noexcept {
    keel::u32 r = 0;
    while (n > 1u) {
        n >>= 1u;
        ++r;
    }
    return r;
}

[[nodiscard]] keel::usize ChooseChunkEnd(keel::StringRef text, keel::usize pos, keel::usize targetBytes) noexcept {
    const keel::usize total = text.size();
    if (pos + targetBytes >= total) {
        return total;
    }
    keel::usize end = pos + targetBytes;

    // Avoid splitting a multi-byte UTF-8 sequence across chunks (back off at most 3 bytes).
    keel::usize back = 0;
    while (back < 3u && end > pos + 1u && !keel::utf8_is_lead_byte(keel::as<keel::u8>(text[end]))) {
        --end;
        ++back;
    }

    // Avoid splitting a "\r\n" pair across chunks.
    if (end > pos + 1u && end < total && text[end - 1u] == '\r' && text[end] == '\n') {
        --end;
    }
    return end;
}

} // namespace

// =============================================================================
// Construction / Lifecycle
// =============================================================================

TextDocument::TextDocument() {
    InitSentinel();
}

TextDocument::TextDocument(keel::StringRef initialText) {
    InitSentinel();
    if (!initialText.empty()) {
        SetText(initialText);
    }
}

TextDocument TextDocument::Clone() const {
    keel::String full;
    ExtractText(full);
    return TextDocument(full.as_ref());
}

void TextDocument::InitSentinel() {
    m_nodes.clear();
    m_freeNodes.clear();
    TreeNode sentinel{};
    sentinel.color = kColorBlack;
    m_nodes.push_back(sentinel);
    m_root = kNil;
    m_activeNodeCount = 0;
}

void TextDocument::ResetTreePool() {
    m_chunks.clear();
    m_activeAddChunk = 0xFFFFFFFFu;
    InitSentinel();
}

void TextDocument::Clear() {
    ResetTreePool();
    ClearHistory();
    ++m_revision;
    m_lastResetRevision = m_revision;
    m_deltaWritePos = 0;
    m_deltaCount = 0;
}

void TextDocument::SetText(keel::StringRef text) {
    ResetTreePool();
    ClearHistory();

    if (!text.empty()) {
        const keel::usize estimatedChunks = (text.size() + kChunkTargetBytes - 1u) / kChunkTargetBytes;
        m_chunks.reserve(estimatedChunks + 1u);

        keel::Array<Piece> initialPieces;
        initialPieces.reserve(estimatedChunks + 1u);

        keel::usize pos = 0;
        while (pos < text.size()) {
            const keel::usize end = ChooseChunkEnd(text, pos, kChunkTargetBytes);
            const keel::u32 chunkLen = keel::as<keel::u32>(end - pos);

            TextChunk chunk{};
            chunk.immutable = true;
            chunk.data.reserve(chunkLen);
            chunk.data.append(text.substr(pos, chunkLen));

            const char* raw = chunk.data.data();
            keel::u8 flags = 0;
            keel::u32 prevLineStart = 0;
            keel::u32 maxLine = 0;
            for (keel::u32 i = 0; i < chunkLen; ++i) {
                const keel::u8 b = keel::as<keel::u8>(raw[i]);
                if (b == '\n') {
                    chunk.lfOffsets.push_back(i);
                    const keel::u32 segLen = i - prevLineStart;
                    if (segLen > maxLine) maxLine = segLen;
                    prevLineStart = i + 1u;
                } else if (b >= 0x80u) {
                    flags |= kFlagNonAscii;
                } else if (b < 0x20u || b == 0x7Fu) {
                    if (b == '\r' && i + 1u < chunkLen && raw[i + 1u] == '\n') {
                        continue;
                    }
                    flags |= kFlagTabOrCtrl;
                }
            }
            const keel::u32 tailLen = chunkLen - prevLineStart;
            if (tailLen > maxLine) maxLine = tailLen;

            const keel::u32 chunkIdx = keel::as<keel::u32>(m_chunks.size());
            const keel::u32 lfCount = keel::as<keel::u32>(chunk.lfOffsets.size());
            Piece p{};
            p.chunkIdx = chunkIdx;
            p.byteStart = 0;
            p.byteLength = chunkLen;
            p.lfStart = 0;
            p.lfCount = lfCount;
            p.ownPrefixBytes = (lfCount == 0) ? chunkLen : chunk.lfOffsets[0];
            p.ownSuffixBytes = (lfCount == 0) ? chunkLen : tailLen;
            p.ownMaxLineBytes = maxLine;
            p.ownFlags = flags;

            m_chunks.push_back(keel::move(chunk));
            initialPieces.push_back(p);
            pos = end;
        }

        m_nodes.reserve(initialPieces.size() + 2u);
        const keel::u32 maxDepth = FloorLog2(initialPieces.size());
        m_root = BuildBalancedSubtree(initialPieces.as_ref(), 0, initialPieces.size(), 0, maxDepth, kNil);
        if (m_root != kNil) {
            m_nodes[m_root].color = kColorBlack;
        }
    }

    ++m_revision;
    m_lastResetRevision = m_revision;
    m_deltaWritePos = 0;
    m_deltaCount = 0;
}

// =============================================================================
// O(1) Metadata Queries
// =============================================================================

keel::usize TextDocument::ByteLength() const noexcept {
    return (m_root == kNil) ? 0u : m_nodes[m_root].subtree.bytes;
}

keel::usize TextDocument::LineCount() const noexcept {
    return (m_root == kNil) ? 1u : (m_nodes[m_root].subtree.lfCount + 1u);
}

keel::usize TextDocument::MaxLineByteLength() const noexcept {
    return (m_root == kNil) ? 0u : m_nodes[m_root].subtree.maxLineBytes;
}

bool TextDocument::IsPureAsciiNoTabs() const noexcept {
    return (m_root == kNil) ? true : (m_nodes[m_root].subtree.flags == 0u);
}

// =============================================================================
// Piece & Tree Allocation Helpers
// =============================================================================

keel::u32 TextDocument::AllocNode(const Piece& piece) {
    keel::u32 idx = kNil;
    if (!m_freeNodes.empty()) {
        idx = m_freeNodes.back();
        m_freeNodes.pop_back();
    } else {
        idx = keel::as<keel::u32>(m_nodes.size());
        m_nodes.push_back(TreeNode{});
    }
    TreeNode& n = m_nodes[idx];
    n.piece = piece;
    n.subtree = PieceSummary(piece);
    n.left = kNil;
    n.right = kNil;
    n.parent = kNil;
    n.color = kColorRed;
    ++m_activeNodeCount;
    return idx;
}

void TextDocument::FreeNode(keel::u32 idx) noexcept {
    if (idx == kNil) return;
    TreeNode& n = m_nodes[idx];
    n = TreeNode{};
    m_freeNodes.push_back(idx);
    --m_activeNodeCount;
}

TextDocument::Piece TextDocument::MakePiece(
    keel::u32 chunkIdx,
    keel::u32 byteStart,
    keel::u32 byteLength
) const noexcept {
    Piece p{};
    p.chunkIdx = chunkIdx;
    p.byteStart = byteStart;
    p.byteLength = byteLength;
    ComputePieceMetrics(p);
    return p;
}

void TextDocument::ComputePieceMetrics(Piece& p) const noexcept {
    const TextChunk& chunk = m_chunks[p.chunkIdx];
    const keel::u32 byteEnd = p.byteStart + p.byteLength;
    const keel::u32 totalLfs = keel::as<keel::u32>(chunk.lfOffsets.size());

    p.lfStart = LowerBoundU32(chunk.lfOffsets.as_ref(), 0, totalLfs, p.byteStart);
    const keel::u32 lfEnd = LowerBoundU32(chunk.lfOffsets.as_ref(), p.lfStart, totalLfs, byteEnd);
    p.lfCount = lfEnd - p.lfStart;

    if (p.lfCount == 0u) {
        p.ownPrefixBytes = p.byteLength;
        p.ownSuffixBytes = p.byteLength;
        p.ownMaxLineBytes = p.byteLength;
    } else {
        const keel::u32 firstLf = chunk.lfOffsets[p.lfStart];
        const keel::u32 lastLf = chunk.lfOffsets[lfEnd - 1u];
        p.ownPrefixBytes = firstLf - p.byteStart;
        p.ownSuffixBytes = byteEnd - (lastLf + 1u);
        keel::u32 maxLine = keel::max(p.ownPrefixBytes, p.ownSuffixBytes);
        for (keel::u32 k = p.lfStart + 1u; k < lfEnd; ++k) {
            const keel::u32 segLen = chunk.lfOffsets[k] - (chunk.lfOffsets[k - 1u] + 1u);
            if (segLen > maxLine) maxLine = segLen;
        }
        p.ownMaxLineBytes = maxLine;
    }

    p.ownFlags = ScanSliceFlags(chunk.data.data(), p.byteStart, p.byteLength);
}

void TextDocument::AppendTextToChunks(keel::StringRef text, keel::Array<Piece>& outPieces) {
    outPieces.clear();
    if (text.empty()) return;

    keel::usize pos = 0;
    while (pos < text.size()) {
        // Check if current writable add-chunk has space for at least this slice.
        keel::u32 chunkIdx = m_activeAddChunk;
        keel::usize avail = 0;
        if (chunkIdx != 0xFFFFFFFFu && chunkIdx < m_chunks.size() && !m_chunks[chunkIdx].immutable) {
            const keel::usize used = m_chunks[chunkIdx].data.size();
            if (used < kChunkTargetBytes) {
                avail = kChunkTargetBytes - used;
            }
        }

        // If active add-chunk is full (or remaining text is larger than a tiny leftover), open a fresh chunk.
        if (avail == 0 || (text.size() - pos > avail && avail < 1024u)) {
            TextChunk fresh{};
            fresh.immutable = false;
            const keel::usize reserveBytes = keel::min(
                keel::as<keel::usize>(kChunkTargetBytes),
                keel::max(text.size() - pos, keel::as<keel::usize>(4096u))
            );
            fresh.data.reserve(reserveBytes);
            chunkIdx = keel::as<keel::u32>(m_chunks.size());
            m_chunks.push_back(keel::move(fresh));
            m_activeAddChunk = chunkIdx;
            avail = kChunkTargetBytes;
        }

        const keel::usize end = ChooseChunkEnd(text, pos, avail);
        const keel::u32 sliceLen = keel::as<keel::u32>(end - pos);
        TextChunk& chunk = m_chunks[chunkIdx];
        const keel::u32 startOff = keel::as<keel::u32>(chunk.data.size());
        chunk.data.append(text.substr(pos, sliceLen));

        const char* raw = chunk.data.data();
        const keel::u32 lfStart = keel::as<keel::u32>(chunk.lfOffsets.size());
        keel::u32 prevLineStart = startOff;
        keel::u32 maxLine = 0;
        keel::u8 flags = 0;
        const keel::u32 endOff = startOff + sliceLen;
        for (keel::u32 i = startOff; i < endOff; ++i) {
            const keel::u8 b = keel::as<keel::u8>(raw[i]);
            if (b == '\n') {
                chunk.lfOffsets.push_back(i);
                const keel::u32 segLen = i - prevLineStart;
                if (segLen > maxLine) maxLine = segLen;
                prevLineStart = i + 1u;
            } else if (b >= 0x80u) {
                flags |= kFlagNonAscii;
            } else if (b < 0x20u || b == 0x7Fu) {
                if (b == '\r' && i + 1u < endOff && raw[i + 1u] == '\n') {
                    continue;
                }
                flags |= kFlagTabOrCtrl;
            }
        }
        const keel::u32 tailLen = endOff - prevLineStart;
        if (tailLen > maxLine) maxLine = tailLen;

        const keel::u32 lfCount = keel::as<keel::u32>(chunk.lfOffsets.size()) - lfStart;
        Piece p{};
        p.chunkIdx = chunkIdx;
        p.byteStart = startOff;
        p.byteLength = sliceLen;
        p.lfStart = lfStart;
        p.lfCount = lfCount;
        p.ownPrefixBytes = (lfCount == 0u) ? sliceLen : (chunk.lfOffsets[lfStart] - startOff);
        p.ownSuffixBytes = (lfCount == 0u) ? sliceLen : tailLen;
        p.ownMaxLineBytes = maxLine;
        p.ownFlags = flags;
        outPieces.push_back(p);

        pos = end;
    }
}

// =============================================================================
// Red-Black Tree Maintenance
// =============================================================================

void TextDocument::UpdateNode(keel::u32 idx) noexcept {
    if (idx == kNil) return;
    TreeNode& n = m_nodes[idx];
    const NodeSummary leftSum = (n.left != kNil) ? m_nodes[n.left].subtree : NodeSummary{};
    const NodeSummary ownSum = PieceSummary(n.piece);
    const NodeSummary rightSum = (n.right != kNil) ? m_nodes[n.right].subtree : NodeSummary{};
    n.subtree = CombineSummary(CombineSummary(leftSum, ownSum), rightSum);
}

void TextDocument::UpdateUpwards(keel::u32 idx) noexcept {
    while (idx != kNil) {
        UpdateNode(idx);
        idx = m_nodes[idx].parent;
    }
}

void TextDocument::RotateLeft(keel::u32 x) noexcept {
    const keel::u32 y = m_nodes[x].right;
    if (y == kNil) return;

    m_nodes[x].right = m_nodes[y].left;
    if (m_nodes[y].left != kNil) {
        m_nodes[m_nodes[y].left].parent = x;
    }
    m_nodes[y].parent = m_nodes[x].parent;
    if (m_nodes[x].parent == kNil) {
        m_root = y;
    } else if (x == m_nodes[m_nodes[x].parent].left) {
        m_nodes[m_nodes[x].parent].left = y;
    } else {
        m_nodes[m_nodes[x].parent].right = y;
    }
    m_nodes[y].left = x;
    m_nodes[x].parent = y;

    UpdateNode(x);
    UpdateNode(y);
}

void TextDocument::RotateRight(keel::u32 y) noexcept {
    const keel::u32 x = m_nodes[y].left;
    if (x == kNil) return;

    m_nodes[y].left = m_nodes[x].right;
    if (m_nodes[x].right != kNil) {
        m_nodes[m_nodes[x].right].parent = y;
    }
    m_nodes[x].parent = m_nodes[y].parent;
    if (m_nodes[y].parent == kNil) {
        m_root = x;
    } else if (y == m_nodes[m_nodes[y].parent].right) {
        m_nodes[m_nodes[y].parent].right = x;
    } else {
        m_nodes[m_nodes[y].parent].left = x;
    }
    m_nodes[x].right = y;
    m_nodes[y].parent = x;

    UpdateNode(y);
    UpdateNode(x);
}

void TextDocument::InsertFixup(keel::u32 z) noexcept {
    while (z != m_root && m_nodes[m_nodes[z].parent].color == kColorRed) {
        const keel::u32 p = m_nodes[z].parent;
        const keel::u32 g = m_nodes[p].parent;
        if (g == kNil) break;

        if (p == m_nodes[g].left) {
            const keel::u32 y = m_nodes[g].right;
            if (y != kNil && m_nodes[y].color == kColorRed) {
                m_nodes[p].color = kColorBlack;
                m_nodes[y].color = kColorBlack;
                m_nodes[g].color = kColorRed;
                z = g;
            } else {
                if (z == m_nodes[p].right) {
                    z = p;
                    RotateLeft(z);
                }
                const keel::u32 p2 = m_nodes[z].parent;
                const keel::u32 g2 = m_nodes[p2].parent;
                m_nodes[p2].color = kColorBlack;
                m_nodes[g2].color = kColorRed;
                RotateRight(g2);
            }
        } else {
            const keel::u32 y = m_nodes[g].left;
            if (y != kNil && m_nodes[y].color == kColorRed) {
                m_nodes[p].color = kColorBlack;
                m_nodes[y].color = kColorBlack;
                m_nodes[g].color = kColorRed;
                z = g;
            } else {
                if (z == m_nodes[p].left) {
                    z = p;
                    RotateRight(z);
                }
                const keel::u32 p2 = m_nodes[z].parent;
                const keel::u32 g2 = m_nodes[p2].parent;
                m_nodes[p2].color = kColorBlack;
                m_nodes[g2].color = kColorRed;
                RotateLeft(g2);
            }
        }
    }
    if (m_root != kNil) {
        m_nodes[m_root].color = kColorBlack;
    }
}

void TextDocument::Transplant(keel::u32 u, keel::u32 v) noexcept {
    const keel::u32 uParent = m_nodes[u].parent;
    if (uParent == kNil) {
        m_root = v;
    } else if (u == m_nodes[uParent].left) {
        m_nodes[uParent].left = v;
    } else {
        m_nodes[uParent].right = v;
    }
    if (v != kNil) {
        m_nodes[v].parent = uParent;
    }
}

void TextDocument::DeleteFixup(keel::u32 x, keel::u32 xParent) noexcept {
    while (x != m_root && (x == kNil || m_nodes[x].color == kColorBlack)) {
        if (xParent == kNil) break;
        if (x == m_nodes[xParent].left) {
            keel::u32 w = m_nodes[xParent].right;
            if (w != kNil && m_nodes[w].color == kColorRed) {
                m_nodes[w].color = kColorBlack;
                m_nodes[xParent].color = kColorRed;
                RotateLeft(xParent);
                w = m_nodes[xParent].right;
            }
            if (w == kNil) {
                x = xParent;
                xParent = m_nodes[x].parent;
                continue;
            }
            const bool leftBlack = (m_nodes[w].left == kNil || m_nodes[m_nodes[w].left].color == kColorBlack);
            const bool rightBlack = (m_nodes[w].right == kNil || m_nodes[m_nodes[w].right].color == kColorBlack);
            if (leftBlack && rightBlack) {
                m_nodes[w].color = kColorRed;
                x = xParent;
                xParent = m_nodes[x].parent;
            } else {
                if (rightBlack) {
                    if (m_nodes[w].left != kNil) {
                        m_nodes[m_nodes[w].left].color = kColorBlack;
                    }
                    m_nodes[w].color = kColorRed;
                    RotateRight(w);
                    w = m_nodes[xParent].right;
                }
                m_nodes[w].color = m_nodes[xParent].color;
                m_nodes[xParent].color = kColorBlack;
                if (m_nodes[w].right != kNil) {
                    m_nodes[m_nodes[w].right].color = kColorBlack;
                }
                RotateLeft(xParent);
                x = m_root;
                xParent = kNil;
            }
        } else {
            keel::u32 w = m_nodes[xParent].left;
            if (w != kNil && m_nodes[w].color == kColorRed) {
                m_nodes[w].color = kColorBlack;
                m_nodes[xParent].color = kColorRed;
                RotateRight(xParent);
                w = m_nodes[xParent].left;
            }
            if (w == kNil) {
                x = xParent;
                xParent = m_nodes[x].parent;
                continue;
            }
            const bool rightBlack = (m_nodes[w].right == kNil || m_nodes[m_nodes[w].right].color == kColorBlack);
            const bool leftBlack = (m_nodes[w].left == kNil || m_nodes[m_nodes[w].left].color == kColorBlack);
            if (leftBlack && rightBlack) {
                m_nodes[w].color = kColorRed;
                x = xParent;
                xParent = m_nodes[x].parent;
            } else {
                if (leftBlack) {
                    if (m_nodes[w].right != kNil) {
                        m_nodes[m_nodes[w].right].color = kColorBlack;
                    }
                    m_nodes[w].color = kColorRed;
                    RotateLeft(w);
                    w = m_nodes[xParent].left;
                }
                m_nodes[w].color = m_nodes[xParent].color;
                m_nodes[xParent].color = kColorBlack;
                if (m_nodes[w].left != kNil) {
                    m_nodes[m_nodes[w].left].color = kColorBlack;
                }
                RotateRight(xParent);
                x = m_root;
                xParent = kNil;
            }
        }
    }
    if (x != kNil) {
        m_nodes[x].color = kColorBlack;
    }
}

void TextDocument::DeleteNode(keel::u32 z) noexcept {
    if (z == kNil) return;

    keel::u32 y = z;
    keel::u8 yOriginalColor = m_nodes[y].color;
    keel::u32 x = kNil;
    keel::u32 xParent = kNil;
    keel::u32 updateFrom = kNil;

    if (m_nodes[z].left == kNil) {
        x = m_nodes[z].right;
        xParent = m_nodes[z].parent;
        updateFrom = xParent;
        Transplant(z, m_nodes[z].right);
    } else if (m_nodes[z].right == kNil) {
        x = m_nodes[z].left;
        xParent = m_nodes[z].parent;
        updateFrom = xParent;
        Transplant(z, m_nodes[z].left);
    } else {
        y = MinNode(m_nodes[z].right);
        yOriginalColor = m_nodes[y].color;
        x = m_nodes[y].right;
        if (m_nodes[y].parent == z) {
            xParent = y;
            updateFrom = y;
        } else {
            xParent = m_nodes[y].parent;
            updateFrom = xParent;
            Transplant(y, m_nodes[y].right);
            m_nodes[y].right = m_nodes[z].right;
            m_nodes[m_nodes[y].right].parent = y;
        }
        Transplant(z, y);
        m_nodes[y].left = m_nodes[z].left;
        m_nodes[m_nodes[y].left].parent = y;
        m_nodes[y].color = m_nodes[z].color;
    }

    FreeNode(z);
    UpdateUpwards(updateFrom);

    if (yOriginalColor == kColorBlack) {
        DeleteFixup(x, xParent);
    }
}

void TextDocument::InsertNodeAtRoot(keel::u32 z) noexcept {
    m_root = z;
    m_nodes[z].parent = kNil;
    m_nodes[z].left = kNil;
    m_nodes[z].right = kNil;
    m_nodes[z].color = kColorBlack;
    UpdateNode(z);
}

void TextDocument::InsertNodeBefore(keel::u32 target, keel::u32 z) noexcept {
    if (m_nodes[target].left == kNil) {
        m_nodes[target].left = z;
        m_nodes[z].parent = target;
    } else {
        const keel::u32 pred = MaxNode(m_nodes[target].left);
        m_nodes[pred].right = z;
        m_nodes[z].parent = pred;
    }
    m_nodes[z].color = kColorRed;
    UpdateUpwards(z);
    InsertFixup(z);
}

void TextDocument::InsertNodeAfter(keel::u32 target, keel::u32 z) noexcept {
    if (m_nodes[target].right == kNil) {
        m_nodes[target].right = z;
        m_nodes[z].parent = target;
    } else {
        const keel::u32 succ = MinNode(m_nodes[target].right);
        m_nodes[succ].left = z;
        m_nodes[z].parent = succ;
    }
    m_nodes[z].color = kColorRed;
    UpdateUpwards(z);
    InsertFixup(z);
}

keel::u32 TextDocument::BuildBalancedSubtree(
    keel::ArrayRef<Piece> pieces,
    keel::usize lo,
    keel::usize hi,
    keel::u32 depth,
    keel::u32 maxDepth,
    keel::u32 parent
) {
    if (lo >= hi) return kNil;
    const keel::usize mid = lo + (hi - lo) / 2u;
    const keel::u32 nodeIdx = AllocNode(pieces[mid]);
    m_nodes[nodeIdx].parent = parent;
    m_nodes[nodeIdx].color = (depth == maxDepth && depth > 0u) ? kColorRed : kColorBlack;

    const keel::u32 leftIdx = BuildBalancedSubtree(pieces, lo, mid, depth + 1u, maxDepth, nodeIdx);
    const keel::u32 rightIdx = BuildBalancedSubtree(pieces, mid + 1u, hi, depth + 1u, maxDepth, nodeIdx);
    m_nodes[nodeIdx].left = leftIdx;
    m_nodes[nodeIdx].right = rightIdx;
    UpdateNode(nodeIdx);
    return nodeIdx;
}

keel::u32 TextDocument::MinNode(keel::u32 idx) const noexcept {
    if (idx == kNil) return kNil;
    while (m_nodes[idx].left != kNil) {
        idx = m_nodes[idx].left;
    }
    return idx;
}

keel::u32 TextDocument::MaxNode(keel::u32 idx) const noexcept {
    if (idx == kNil) return kNil;
    while (m_nodes[idx].right != kNil) {
        idx = m_nodes[idx].right;
    }
    return idx;
}

keel::u32 TextDocument::NextNode(keel::u32 idx) const noexcept {
    if (idx == kNil) return kNil;
    if (m_nodes[idx].right != kNil) {
        return MinNode(m_nodes[idx].right);
    }
    keel::u32 p = m_nodes[idx].parent;
    while (p != kNil && idx == m_nodes[p].right) {
        idx = p;
        p = m_nodes[p].parent;
    }
    return p;
}

keel::u32 TextDocument::PrevNode(keel::u32 idx) const noexcept {
    if (idx == kNil) return kNil;
    if (m_nodes[idx].left != kNil) {
        return MaxNode(m_nodes[idx].left);
    }
    keel::u32 p = m_nodes[idx].parent;
    while (p != kNil && idx == m_nodes[p].left) {
        idx = p;
        p = m_nodes[p].parent;
    }
    return p;
}

TextDocument::NodeLocation TextDocument::FindNodeAtOffset(keel::usize byteOffset) const noexcept {
    if (m_root == kNil) {
        return NodeLocation{kNil, 0u, 0u};
    }
    keel::u32 cur = m_root;
    keel::usize accBytes = 0;
    while (cur != kNil) {
        const TreeNode& n = m_nodes[cur];
        const keel::usize leftBytes = (n.left != kNil) ? m_nodes[n.left].subtree.bytes : 0u;
        if (byteOffset < accBytes + leftBytes) {
            cur = n.left;
        } else {
            const keel::usize nodeStart = accBytes + leftBytes;
            const keel::usize nodeEnd = nodeStart + n.piece.byteLength;
            if (byteOffset < nodeEnd || (byteOffset == nodeEnd && n.right == kNil)) {
                return NodeLocation{cur, nodeStart, byteOffset - nodeStart};
            }
            accBytes = nodeEnd;
            cur = n.right;
        }
    }
    return NodeLocation{kNil, 0u, 0u};
}

// =============================================================================
// Coordinate & Slice Queries
// =============================================================================

keel::usize TextDocument::LineStartByte(keel::usize line) const noexcept {
    if (line == 0u || m_root == kNil) {
        return 0u;
    }
    const keel::usize totalLfs = m_nodes[m_root].subtree.lfCount;
    if (line > totalLfs) {
        line = totalLfs;
    }
    if (line == 0u) {
        return 0u;
    }

    // Find byte offset immediately after the (line - 1)-th '\n'.
    keel::usize remLf = line - 1u;
    keel::usize accBytes = 0;
    keel::u32 cur = m_root;
    while (cur != kNil) {
        const TreeNode& n = m_nodes[cur];
        const keel::usize leftLfs = (n.left != kNil) ? m_nodes[n.left].subtree.lfCount : 0u;
        const keel::usize leftBytes = (n.left != kNil) ? m_nodes[n.left].subtree.bytes : 0u;
        if (remLf < leftLfs) {
            cur = n.left;
        } else if (remLf < leftLfs + n.piece.lfCount) {
            const keel::u32 localLfIdx = keel::as<keel::u32>(remLf - leftLfs);
            const TextChunk& chunk = m_chunks[n.piece.chunkIdx];
            const keel::u32 lfPosInChunk = chunk.lfOffsets[n.piece.lfStart + localLfIdx];
            const keel::usize relAfterLf = keel::as<keel::usize>((lfPosInChunk - n.piece.byteStart) + 1u);
            return accBytes + leftBytes + relAfterLf;
        } else {
            remLf -= leftLfs + n.piece.lfCount;
            accBytes += leftBytes + n.piece.byteLength;
            cur = n.right;
        }
    }
    return ByteLength();
}

TextLineRange TextDocument::LineByteRange(keel::usize line) const noexcept {
    const keel::usize totalLines = LineCount();
    if (line >= totalLines) {
        line = totalLines - 1u;
    }
    const keel::usize startByte = LineStartByte(line);
    if (line + 1u < totalLines) {
        const keel::usize nextStart = LineStartByte(line + 1u);
        const keel::usize totalBytes = nextStart - startByte;
        keel::usize contentBytes = (totalBytes > 0u) ? (totalBytes - 1u) : 0u;
        if (contentBytes > 0u && ByteAt(startByte + contentBytes - 1u) == '\r') {
            --contentBytes;
        }
        return TextLineRange{startByte, contentBytes, totalBytes};
    }

    const keel::usize totalBytes = ByteLength() - startByte;
    return TextLineRange{startByte, totalBytes, totalBytes};
}

TextLineCol TextDocument::OffsetToLineCol(keel::usize byteOffset) const noexcept {
    const keel::usize total = ByteLength();
    if (byteOffset > total) {
        byteOffset = total;
    }
    if (byteOffset == 0u || m_root == kNil) {
        return TextLineCol{0u, 0u};
    }
    if (byteOffset == total) {
        const keel::usize lastLine = m_nodes[m_root].subtree.lfCount;
        const keel::usize lineStart = LineStartByte(lastLine);
        return TextLineCol{lastLine, total - lineStart};
    }

    keel::u32 cur = m_root;
    keel::usize accBytes = 0;
    keel::usize accLfs = 0;
    while (cur != kNil) {
        const TreeNode& n = m_nodes[cur];
        const keel::usize leftBytes = (n.left != kNil) ? m_nodes[n.left].subtree.bytes : 0u;
        const keel::usize leftLfs = (n.left != kNil) ? m_nodes[n.left].subtree.lfCount : 0u;
        if (byteOffset < accBytes + leftBytes) {
            cur = n.left;
        } else {
            const keel::usize nodeStart = accBytes + leftBytes;
            const keel::usize nodeEnd = nodeStart + n.piece.byteLength;
            if (byteOffset < nodeEnd) {
                const keel::u32 relOffset = keel::as<keel::u32>(byteOffset - nodeStart);
                const keel::u32 chunkTarget = n.piece.byteStart + relOffset;
                const TextChunk& chunk = m_chunks[n.piece.chunkIdx];
                const keel::u32 lfEnd = n.piece.lfStart + n.piece.lfCount;
                // Count '\n' strictly before byteOffset.
                const keel::u32 firstGe = LowerBoundU32(chunk.lfOffsets.as_ref(), n.piece.lfStart, lfEnd, chunkTarget);
                const keel::u32 localLfsBefore = firstGe - n.piece.lfStart;
                const keel::usize line = accLfs + leftLfs + localLfsBefore;
                if (localLfsBefore > 0u) {
                    const keel::u32 prevLfChunk = chunk.lfOffsets[firstGe - 1u];
                    const keel::u32 prevLineStartRel = (prevLfChunk - n.piece.byteStart) + 1u;
                    return TextLineCol{line, keel::as<keel::usize>(relOffset - prevLineStartRel)};
                }
                if (line == 0u) {
                    return TextLineCol{0u, byteOffset};
                }
                const keel::usize lineStart = LineStartByte(line);
                return TextLineCol{line, byteOffset - lineStart};
            }
            accBytes = nodeEnd;
            accLfs += leftLfs + n.piece.lfCount;
            cur = n.right;
        }
    }
    return TextLineCol{0u, 0u};
}

keel::usize TextDocument::LineColToOffset(keel::usize line, keel::usize colBytes) const noexcept {
    const TextLineRange r = LineByteRange(line);
    return r.startByte + keel::min(colBytes, r.contentBytes);
}

char TextDocument::ByteAt(keel::usize byteOffset) const noexcept {
    if (byteOffset >= ByteLength()) {
        return '\0';
    }
    const NodeLocation loc = FindNodeAtOffset(byteOffset);
    if (loc.node == kNil) {
        return '\0';
    }
    const Piece& p = m_nodes[loc.node].piece;
    return m_chunks[p.chunkIdx].data[p.byteStart + keel::as<keel::u32>(loc.relOffset)];
}

bool TextDocument::RangeIsPureAsciiNoTabs(keel::usize byteOffset, keel::usize byteLen) const noexcept {
    if (byteLen == 0u || m_root == kNil) {
        return true;
    }
    if (m_nodes[m_root].subtree.flags == 0u) {
        return true;
    }
    const keel::usize total = ByteLength();
    if (byteOffset >= total) {
        return true;
    }
    const keel::usize clampedLen = keel::min(byteLen, total - byteOffset);
    NodeLocation loc = FindNodeAtOffset(byteOffset);
    keel::u32 cur = loc.node;
    keel::usize rel = loc.relOffset;
    keel::usize rem = clampedLen;

    while (cur != kNil && rem > 0u) {
        const Piece& p = m_nodes[cur].piece;
        const keel::usize take = keel::min(rem, keel::as<keel::usize>(p.byteLength) - rel);
        if (p.ownFlags != 0u) {
            if (rel == 0u && take == p.byteLength) {
                return false;
            }
            const TextChunk& chunk = m_chunks[p.chunkIdx];
            const keel::u8 sliceFlags = ScanSliceFlags(
                chunk.data.data(),
                p.byteStart + keel::as<keel::u32>(rel),
                keel::as<keel::u32>(take)
            );
            if (sliceFlags != 0u) {
                return false;
            }
        }
        rem -= take;
        cur = NextNode(cur);
        rel = 0u;
    }
    return true;
}

void TextDocument::ForEachChunkInRange(
    keel::usize byteOffset,
    keel::usize byteLen,
    keel::FunctionRef<bool(keel::StringRef)> visitor
) const {
    if (byteLen == 0u || m_root == kNil || !visitor) {
        return;
    }
    const keel::usize total = ByteLength();
    if (byteOffset >= total) {
        return;
    }
    keel::usize rem = keel::min(byteLen, total - byteOffset);
    const NodeLocation loc = FindNodeAtOffset(byteOffset);
    keel::u32 cur = loc.node;
    keel::usize rel = loc.relOffset;

    while (cur != kNil && rem > 0u) {
        const Piece& p = m_nodes[cur].piece;
        const keel::usize take = keel::min(rem, keel::as<keel::usize>(p.byteLength) - rel);
        const char* ptr = m_chunks[p.chunkIdx].data.data() + p.byteStart + rel;
        if (!visitor(keel::StringRef(ptr, take))) {
            return;
        }
        rem -= take;
        cur = NextNode(cur);
        rel = 0u;
    }
}

void TextDocument::ReadSlice(keel::usize byteOffset, keel::usize byteLen, keel::String& out) const {
    out.clear();
    const keel::usize total = ByteLength();
    if (byteOffset >= total || byteLen == 0u) {
        return;
    }
    const keel::usize actualLen = keel::min(byteLen, total - byteOffset);
    out.reserve(actualLen);
    ForEachChunkInRange(byteOffset, actualLen, [&out](keel::StringRef chunk) -> bool {
        out.append(chunk);
        return true;
    });
}

void TextDocument::ReadLineSlice(
    keel::usize line,
    keel::usize colByteStart,
    keel::usize maxBytes,
    keel::String& out
) const {
    out.clear();
    const TextLineRange r = LineByteRange(line);
    if (colByteStart >= r.contentBytes || maxBytes == 0u) {
        return;
    }
    const keel::usize len = keel::min(maxBytes, r.contentBytes - colByteStart);
    ReadSlice(r.startByte + colByteStart, len, out);
}

void TextDocument::ReadLine(keel::usize line, keel::String& out) const {
    const TextLineRange r = LineByteRange(line);
    ReadSlice(r.startByte, r.contentBytes, out);
}

void TextDocument::ExtractText(keel::String& out) const {
    ReadSlice(0u, ByteLength(), out);
}

keel::String TextDocument::ToString() const {
    keel::String out;
    ExtractText(out);
    return out;
}

// =============================================================================
// Bounded-Window UTF-8 & Word Boundary Navigation
// =============================================================================

keel::usize TextDocument::ReadRawWindow(keel::usize byteOffset, char* dst, keel::usize maxBytes) const noexcept {
    const keel::usize total = ByteLength();
    if (byteOffset >= total || maxBytes == 0u) {
        return 0u;
    }
    keel::usize rem = keel::min(maxBytes, total - byteOffset);
    keel::usize written = 0u;
    const NodeLocation loc = FindNodeAtOffset(byteOffset);
    keel::u32 cur = loc.node;
    keel::usize rel = loc.relOffset;
    while (cur != kNil && rem > 0u) {
        const Piece& p = m_nodes[cur].piece;
        const keel::usize take = keel::min(rem, keel::as<keel::usize>(p.byteLength) - rel);
        keel::mem_copy(dst + written, m_chunks[p.chunkIdx].data.data() + p.byteStart + rel, take);
        written += take;
        rem -= take;
        cur = NextNode(cur);
        rel = 0u;
    }
    return written;
}

keel::usize TextDocument::SnapToUtf8Boundary(keel::usize byteOffset) const noexcept {
    const keel::usize total = ByteLength();
    if (byteOffset >= total) {
        return total;
    }
    if (byteOffset == 0u) {
        return 0u;
    }
    const keel::usize winStart = (byteOffset >= 4u) ? (byteOffset - 4u) : 0u;
    char buf[8];
    const keel::usize readLen = ReadRawWindow(winStart, buf, (byteOffset - winStart) + 1u);
    keel::usize local = byteOffset - winStart;
    if (local >= readLen) {
        return byteOffset;
    }
    keel::usize steps = 0;
    while (local > 0u && steps < 3u && !keel::utf8_is_lead_byte(keel::as<keel::u8>(buf[local]))) {
        --local;
        ++steps;
    }
    return winStart + local;
}

keel::usize TextDocument::PrevCharOffset(keel::usize byteOffset) const noexcept {
    const keel::usize total = ByteLength();
    if (byteOffset > total) byteOffset = total;
    if (byteOffset == 0u) return 0u;

    const keel::usize winStart = (byteOffset >= 8u) ? (byteOffset - 8u) : 0u;
    char buf[8];
    const keel::usize readLen = ReadRawWindow(winStart, buf, byteOffset - winStart);
    const keel::usize localPrev = keel::utf8_prev_boundary(keel::StringRef(buf, readLen), readLen);
    return winStart + localPrev;
}

keel::usize TextDocument::NextCharOffset(keel::usize byteOffset) const noexcept {
    const keel::usize total = ByteLength();
    if (byteOffset >= total) return total;

    char buf[8];
    const keel::usize readLen = ReadRawWindow(byteOffset, buf, 8u);
    const keel::usize localNext = keel::utf8_next_boundary(keel::StringRef(buf, readLen), 0u);
    return byteOffset + localNext;
}

keel::usize TextDocument::PrevWordOffset(keel::usize byteOffset) const noexcept {
    const keel::usize total = ByteLength();
    if (byteOffset > total) byteOffset = total;
    if (byteOffset == 0u) return 0u;

    constexpr keel::usize kWinBytes = 512u;
    keel::usize winStart = (byteOffset > kWinBytes) ? (byteOffset - kWinBytes) : 0u;
    if (winStart > 0u) {
        winStart = SnapToUtf8Boundary(winStart);
    }
    char buf[kWinBytes + 8u];
    const keel::usize readLen = ReadRawWindow(winStart, buf, byteOffset - winStart);
    const keel::usize localPrev = keel::utf8_prev_word_boundary(keel::StringRef(buf, readLen), readLen);
    return winStart + localPrev;
}

keel::usize TextDocument::NextWordOffset(keel::usize byteOffset) const noexcept {
    const keel::usize total = ByteLength();
    if (byteOffset >= total) return total;

    constexpr keel::usize kWinBytes = 512u;
    char buf[kWinBytes + 8u];
    const keel::usize readLen = ReadRawWindow(byteOffset, buf, kWinBytes);
    const keel::usize localNext = keel::utf8_next_word_boundary(keel::StringRef(buf, readLen), 0u);
    return byteOffset + localNext;
}

void TextDocument::WordBoundsAt(
    keel::usize byteOffset,
    keel::usize& outStart,
    keel::usize& outEnd
) const noexcept {
    const keel::usize total = ByteLength();
    if (total == 0u) {
        outStart = 0u;
        outEnd = 0u;
        return;
    }
    if (byteOffset > total) byteOffset = total;

    constexpr keel::usize kHalfWin = 256u;
    keel::usize winStart = (byteOffset > kHalfWin) ? (byteOffset - kHalfWin) : 0u;
    if (winStart > 0u) {
        winStart = SnapToUtf8Boundary(winStart);
    }
    const keel::usize maxRead = (byteOffset - winStart) + kHalfWin;
    char buf[kHalfWin * 2u + 16u];
    const keel::usize readLen = ReadRawWindow(winStart, buf, keel::min(maxRead, sizeof(buf)));
    const keel::usize localIdx = keel::min(byteOffset - winStart, readLen);

    keel::usize localStart = 0;
    keel::usize localEnd = 0;
    keel::utf8_word_bounds_at(keel::StringRef(buf, readLen), localIdx, localStart, localEnd);
    outStart = winStart + localStart;
    outEnd = winStart + localEnd;
}

// =============================================================================
// Piece Tree Mutations
// =============================================================================

bool TextDocument::TryCoalesceInsertIntoNode(keel::u32 nodeIdx, keel::StringRef text) {
    if (nodeIdx == kNil || text.empty() || text.size() > kChunkTargetBytes) {
        return false;
    }
    Piece& p = m_nodes[nodeIdx].piece;
    if (p.chunkIdx != m_activeAddChunk || p.chunkIdx >= m_chunks.size()) {
        return false;
    }
    TextChunk& chunk = m_chunks[p.chunkIdx];
    if (chunk.immutable) {
        return false;
    }
    const keel::u32 oldChunkSize = keel::as<keel::u32>(chunk.data.size());
    if (p.byteStart + p.byteLength != oldChunkSize) {
        return false;
    }
    if (oldChunkSize + text.size() > kChunkTargetBytes) {
        return false;
    }

    // Check if the piece previously ended with '\r' and the new text starts with '\n'
    // (a stray-CR flag might become a valid CRLF, or vice versa).
    const bool hadTrailingCr = (p.byteLength > 0u && chunk.data[oldChunkSize - 1u] == '\r');
    const bool startsWithLf = (text[0] == '\n');

    chunk.data.append(text);
    const keel::u32 newEnd = keel::as<keel::u32>(chunk.data.size());
    const char* raw = chunk.data.data();

    keel::u32 addedLfs = 0;
    keel::u32 prevLineStart = oldChunkSize;
    keel::u32 curSuffix = p.ownSuffixBytes;
    keel::u32 maxLine = p.ownMaxLineBytes;
    keel::u8 addedFlags = 0;

    for (keel::u32 i = oldChunkSize; i < newEnd; ++i) {
        const keel::u8 b = keel::as<keel::u8>(raw[i]);
        if (b == '\n') {
            chunk.lfOffsets.push_back(i);
            const keel::u32 segLen = (addedLfs == 0u) ? (curSuffix + (i - prevLineStart)) : (i - prevLineStart);
            if (p.lfCount == 0u && addedLfs == 0u) {
                p.ownPrefixBytes = segLen;
            }
            if (segLen > maxLine) maxLine = segLen;
            prevLineStart = i + 1u;
            ++addedLfs;
        } else if (b >= 0x80u) {
            addedFlags |= kFlagNonAscii;
        } else if (b < 0x20u || b == 0x7Fu) {
            if (b == '\r' && i + 1u < newEnd && raw[i + 1u] == '\n') {
                continue;
            }
            addedFlags |= kFlagTabOrCtrl;
        }
    }

    p.byteLength += keel::as<keel::u32>(text.size());
    p.lfCount += addedLfs;

    if (addedLfs == 0u) {
        p.ownSuffixBytes += keel::as<keel::u32>(text.size());
        if (p.lfCount == 0u) {
            p.ownPrefixBytes = p.byteLength;
            p.ownMaxLineBytes = p.byteLength;
        } else if (p.ownSuffixBytes > maxLine) {
            maxLine = p.ownSuffixBytes;
            p.ownMaxLineBytes = maxLine;
        }
    } else {
        p.ownSuffixBytes = newEnd - prevLineStart;
        if (p.ownSuffixBytes > maxLine) maxLine = p.ownSuffixBytes;
        p.ownMaxLineBytes = maxLine;
    }

    if (hadTrailingCr && startsWithLf) {
        // Re-scan flags accurately because the previous trailing '\r' is now part of "\r\n".
        p.ownFlags = ScanSliceFlags(chunk.data.data(), p.byteStart, p.byteLength);
    } else {
        p.ownFlags = keel::as<keel::u8>(p.ownFlags | addedFlags);
    }

    UpdateUpwards(nodeIdx);
    return true;
}

void TextDocument::RawInsert(keel::usize byteOffset, keel::StringRef text) {
    if (text.empty()) return;

    const keel::usize total = ByteLength();
    if (byteOffset > total) byteOffset = total;

    // Empty tree case.
    if (m_root == kNil) {
        keel::Array<Piece> pieces;
        AppendTextToChunks(text, pieces);
        const keel::u32 firstNode = AllocNode(pieces[0]);
        InsertNodeAtRoot(firstNode);
        keel::u32 prev = firstNode;
        for (keel::usize i = 1u; i < pieces.size(); ++i) {
            const keel::u32 next = AllocNode(pieces[i]);
            InsertNodeAfter(prev, next);
            prev = next;
        }
        return;
    }

    // Append at end of document.
    if (byteOffset == total) {
        const keel::u32 tail = MaxNode(m_root);
        if (TryCoalesceInsertIntoNode(tail, text)) {
            return;
        }
        keel::Array<Piece> pieces;
        AppendTextToChunks(text, pieces);
        keel::u32 prev = tail;
        for (keel::usize i = 0u; i < pieces.size(); ++i) {
            const keel::u32 n = AllocNode(pieces[i]);
            InsertNodeAfter(prev, n);
            prev = n;
        }
        return;
    }

    // Insert at beginning of document.
    if (byteOffset == 0u) {
        const keel::u32 head = MinNode(m_root);
        keel::Array<Piece> pieces;
        AppendTextToChunks(text, pieces);
        const keel::u32 first = AllocNode(pieces[0]);
        InsertNodeBefore(head, first);
        keel::u32 prev = first;
        for (keel::usize i = 1u; i < pieces.size(); ++i) {
            const keel::u32 n = AllocNode(pieces[i]);
            InsertNodeAfter(prev, n);
            prev = n;
        }
        return;
    }

    // Interior insert.
    const NodeLocation loc = FindNodeAtOffset(byteOffset);
    if (loc.relOffset == 0u) {
        const keel::u32 prev = PrevNode(loc.node);
        if (TryCoalesceInsertIntoNode(prev, text)) {
            return;
        }
        keel::Array<Piece> pieces;
        AppendTextToChunks(text, pieces);
        const keel::u32 first = AllocNode(pieces[0]);
        InsertNodeBefore(loc.node, first);
        keel::u32 cursor = first;
        for (keel::usize i = 1u; i < pieces.size(); ++i) {
            const keel::u32 n = AllocNode(pieces[i]);
            InsertNodeAfter(cursor, n);
            cursor = n;
        }
        return;
    }

    // Split target piece at relOffset into [0, relOffset) and [relOffset, oldLen).
    const Piece orig = m_nodes[loc.node].piece;
    const keel::u32 leftLen = keel::as<keel::u32>(loc.relOffset);
    const keel::u32 rightLen = orig.byteLength - leftLen;

    m_nodes[loc.node].piece = MakePiece(orig.chunkIdx, orig.byteStart, leftLen);
    UpdateUpwards(loc.node);

    const Piece rightPiece = MakePiece(orig.chunkIdx, orig.byteStart + leftLen, rightLen);

    keel::Array<Piece> insertedPieces;
    AppendTextToChunks(text, insertedPieces);

    keel::u32 cursor = loc.node;
    for (keel::usize i = 0u; i < insertedPieces.size(); ++i) {
        const keel::u32 n = AllocNode(insertedPieces[i]);
        InsertNodeAfter(cursor, n);
        cursor = n;
    }
    const keel::u32 rightNode = AllocNode(rightPiece);
    InsertNodeAfter(cursor, rightNode);
}

void TextDocument::RawErase(keel::usize byteOffset, keel::usize eraseBytes) {
    const keel::usize total = ByteLength();
    if (eraseBytes == 0u || byteOffset >= total || m_root == kNil) {
        return;
    }
    if (eraseBytes > total - byteOffset) {
        eraseBytes = total - byteOffset;
    }

    // Fast path: erase entire document.
    if (byteOffset == 0u && eraseBytes == total) {
        InitSentinel();
        return;
    }

    const keel::usize eraseEnd = byteOffset + eraseBytes;
    const NodeLocation startLoc = FindNodeAtOffset(byteOffset);
    const NodeLocation endLoc = (eraseEnd < total)
        ? FindNodeAtOffset(eraseEnd)
        : NodeLocation{kNil, total, 0u};

    // Case A: Erase is strictly within a single piece.
    if (startLoc.node == endLoc.node && endLoc.node != kNil) {
        const Piece orig = m_nodes[startLoc.node].piece;
        const keel::u32 s = keel::as<keel::u32>(startLoc.relOffset);
        const keel::u32 e = keel::as<keel::u32>(endLoc.relOffset);

        if (s == 0u && e == orig.byteLength) {
            DeleteNode(startLoc.node);
        } else if (s == 0u) {
            m_nodes[startLoc.node].piece = MakePiece(orig.chunkIdx, orig.byteStart + e, orig.byteLength - e);
            UpdateUpwards(startLoc.node);
        } else if (e == orig.byteLength) {
            m_nodes[startLoc.node].piece = MakePiece(orig.chunkIdx, orig.byteStart, s);
            UpdateUpwards(startLoc.node);
        } else {
            // Split into left [0, s) and right [e, orig.byteLength).
            m_nodes[startLoc.node].piece = MakePiece(orig.chunkIdx, orig.byteStart, s);
            UpdateUpwards(startLoc.node);
            const Piece rightPiece = MakePiece(orig.chunkIdx, orig.byteStart + e, orig.byteLength - e);
            const keel::u32 rightNode = AllocNode(rightPiece);
            InsertNodeAfter(startLoc.node, rightNode);
        }
        return;
    }

    // Case B: Erase spans across multiple pieces (or reaches EOF).
    keel::u32 cur = startLoc.node;
    if (startLoc.relOffset > 0u) {
        const Piece orig = m_nodes[cur].piece;
        m_nodes[cur].piece = MakePiece(orig.chunkIdx, orig.byteStart, keel::as<keel::u32>(startLoc.relOffset));
        UpdateUpwards(cur);
        cur = NextNode(cur);
    }

    while (cur != endLoc.node && cur != kNil) {
        const keel::u32 next = NextNode(cur);
        DeleteNode(cur);
        cur = next;
    }

    if (endLoc.node != kNil && endLoc.relOffset > 0u) {
        const Piece orig = m_nodes[endLoc.node].piece;
        const keel::u32 e = keel::as<keel::u32>(endLoc.relOffset);
        if (e >= orig.byteLength) {
            DeleteNode(endLoc.node);
        } else {
            m_nodes[endLoc.node].piece = MakePiece(orig.chunkIdx, orig.byteStart + e, orig.byteLength - e);
            UpdateUpwards(endLoc.node);
        }
    }
}

void TextDocument::PushDelta(const TextEditDelta& delta) noexcept {
    m_deltaRing[m_deltaWritePos] = delta;
    m_deltaWritePos = (m_deltaWritePos + 1u) % kDeltaHistoryCapacity;
    if (m_deltaCount < kDeltaHistoryCapacity) {
        ++m_deltaCount;
    }
}

TextEditDelta TextDocument::ApplyRawReplace(
    keel::usize byteOffset,
    keel::usize eraseBytes,
    keel::StringRef insertUtf8
) {
    const keel::usize total = ByteLength();
    if (byteOffset > total) byteOffset = total;
    if (eraseBytes > total - byteOffset) eraseBytes = total - byteOffset;

    const keel::usize startLine = OffsetToLineCol(byteOffset).line;
    const keel::usize oldTotalLines = LineCount();
    const keel::usize newLfs = CountLineFeeds(insertUtf8);

    if (eraseBytes > 0u) {
        RawErase(byteOffset, eraseBytes);
    }
    if (!insertUtf8.empty()) {
        RawInsert(byteOffset, insertUtf8);
    }

    const keel::usize newTotalLines = LineCount();
    const keel::isize lineDelta = keel::as<keel::isize>(newTotalLines) - keel::as<keel::isize>(oldTotalLines);
    const keel::usize oldLfs = keel::as<keel::usize>(keel::as<keel::isize>(newLfs) - lineDelta);

    ++m_revision;
    TextEditDelta delta{};
    delta.revision = m_revision;
    delta.startByte = byteOffset;
    delta.oldByteLen = eraseBytes;
    delta.newByteLen = insertUtf8.size();
    delta.startLine = startLine;
    delta.oldLineCount = oldLfs;
    delta.newLineCount = newLfs;
    delta.lineDelta = lineDelta;
    PushDelta(delta);
    return delta;
}

// =============================================================================
// Public Mutations, Grouping & Undo/Redo
// =============================================================================

void TextDocument::BeginEditGroup() noexcept {
    if (m_editGroupDepth == 0u) {
        m_activeGroupId = m_nextGroupId++;
        m_canCoalesceUndo = false;
    }
    ++m_editGroupDepth;
}

void TextDocument::EndEditGroup() noexcept {
    if (m_editGroupDepth > 0u) {
        --m_editGroupDepth;
        if (m_editGroupDepth == 0u) {
            m_activeGroupId = 0u;
            m_canCoalesceUndo = false;
        }
    }
}

void TextDocument::ClearHistory() noexcept {
    m_undoOps.clear();
    m_redoOps.clear();
    m_undoStoredBytes = 0;
    m_canCoalesceUndo = false;
}

void TextDocument::TrimUndoHistory() noexcept {
    if (m_undoOps.size() <= 1u) {
        return;
    }

    // Count distinct groups on the undo stack.
    keel::usize groupCount = 1u;
    for (keel::usize i = 1u; i < m_undoOps.size(); ++i) {
        if (m_undoOps[i].groupId != m_undoOps[i - 1u].groupId) {
            ++groupCount;
        }
    }

    keel::usize dropCount = 0;
    while (groupCount > 1u && (groupCount > kMaxUndoGroups || m_undoStoredBytes > kMaxUndoBytes)) {
        const keel::u64 oldestGroup = m_undoOps[dropCount].groupId;
        while (dropCount < m_undoOps.size() && m_undoOps[dropCount].groupId == oldestGroup) {
            const UndoEntry& e = m_undoOps[dropCount];
            m_undoStoredBytes -= (e.deletedText.size() + e.insertedText.size());
            ++dropCount;
        }
        --groupCount;
    }

    if (dropCount > 0u) {
        const keel::usize remain = m_undoOps.size() - dropCount;
        for (keel::usize i = 0u; i < remain; ++i) {
            m_undoOps[i] = keel::move(m_undoOps[i + dropCount]);
        }
        m_undoOps.resize(remain);
    }
}

TextEditDelta TextDocument::ReplaceRange(
    keel::usize byteOffset,
    keel::usize eraseBytes,
    keel::StringRef insertUtf8,
    keel::usize beforeCursor,
    keel::usize beforeAnchor,
    keel::usize afterCursor,
    keel::usize afterAnchor,
    bool recordUndo,
    bool allowCoalesce
) {
    const keel::usize total = ByteLength();
    if (byteOffset > total) byteOffset = total;
    if (eraseBytes > total - byteOffset) eraseBytes = total - byteOffset;

    if (eraseBytes == 0u && insertUtf8.empty()) {
        TextEditDelta noop{};
        noop.revision = m_revision;
        noop.startByte = byteOffset;
        noop.startLine = OffsetToLineCol(byteOffset).line;
        return noop;
    }

    keel::String deleted;
    if (recordUndo && eraseBytes > 0u) {
        ReadSlice(byteOffset, eraseBytes, deleted);
    }

    const TextEditDelta delta = ApplyRawReplace(byteOffset, eraseBytes, insertUtf8);

    if (recordUndo) {
        m_redoOps.clear();

        // Try coalescing sequential single-character typing into the previous undo op.
        const bool isPureInsert = (eraseBytes == 0u && !insertUtf8.empty() && insertUtf8.size() <= 4u);
        const bool hasBreakChar = isPureInsert && (insertUtf8[0] == '\n' || insertUtf8[0] == '\r' || insertUtf8[0] == ' ' || insertUtf8[0] == '\t');
        if (allowCoalesce && m_canCoalesceUndo && m_editGroupDepth == 0u &&
            isPureInsert && !hasBreakChar && !m_undoOps.empty()) {
            UndoEntry& last = m_undoOps.back();
            if (last.deletedText.empty() && last.offset + last.insertedText.size() == byteOffset) {
                last.insertedText.append(insertUtf8);
                last.afterCursor = afterCursor;
                last.afterAnchor = afterAnchor;
                m_undoStoredBytes += insertUtf8.size();
                TrimUndoHistory();
                return delta;
            }
        }

        UndoEntry entry{};
        entry.offset = byteOffset;
        entry.deletedText = keel::move(deleted);
        entry.insertedText.assign(insertUtf8);
        entry.beforeCursor = beforeCursor;
        entry.beforeAnchor = beforeAnchor;
        entry.afterCursor = afterCursor;
        entry.afterAnchor = afterAnchor;
        entry.groupId = (m_editGroupDepth > 0u) ? m_activeGroupId : m_nextGroupId++;

        m_undoStoredBytes += entry.deletedText.size() + entry.insertedText.size();
        m_undoOps.push_back(keel::move(entry));
        m_canCoalesceUndo = allowCoalesce && isPureInsert && !hasBreakChar && (m_editGroupDepth == 0u);
        TrimUndoHistory();
    } else {
        m_canCoalesceUndo = false;
    }

    return delta;
}

TextEditDelta TextDocument::Insert(keel::usize byteOffset, keel::StringRef insertUtf8, bool recordUndo) {
    const keel::usize clamped = keel::min(byteOffset, ByteLength());
    const keel::usize afterPos = clamped + insertUtf8.size();
    return ReplaceRange(clamped, 0u, insertUtf8, clamped, clamped, afterPos, afterPos, recordUndo, false);
}

TextEditDelta TextDocument::Erase(keel::usize byteOffset, keel::usize eraseBytes, bool recordUndo) {
    const keel::usize total = ByteLength();
    const keel::usize clamped = keel::min(byteOffset, total);
    const keel::usize clampedErase = keel::min(eraseBytes, total - clamped);
    return ReplaceRange(clamped, clampedErase, keel::StringRef{}, clamped + clampedErase, clamped, clamped, clamped, recordUndo, false);
}

TextUndoResult TextDocument::Undo() {
    if (m_undoOps.empty()) {
        return TextUndoResult{false, 0u, 0u};
    }

    m_canCoalesceUndo = false;
    const keel::u64 targetGroup = m_undoOps.back().groupId;
    keel::usize restoredCursor = 0;
    keel::usize restoredAnchor = 0;

    while (!m_undoOps.empty() && m_undoOps.back().groupId == targetGroup) {
        UndoEntry op = keel::move(m_undoOps.back());
        m_undoOps.pop_back();
        m_undoStoredBytes -= (op.deletedText.size() + op.insertedText.size());

        ApplyRawReplace(op.offset, op.insertedText.size(), op.deletedText.as_ref());
        restoredCursor = op.beforeCursor;
        restoredAnchor = op.beforeAnchor;
        m_redoOps.push_back(keel::move(op));
    }

    return TextUndoResult{true, restoredCursor, restoredAnchor};
}

TextUndoResult TextDocument::Redo() {
    if (m_redoOps.empty()) {
        return TextUndoResult{false, 0u, 0u};
    }

    m_canCoalesceUndo = false;
    const keel::u64 targetGroup = m_redoOps.back().groupId;
    keel::usize restoredCursor = 0;
    keel::usize restoredAnchor = 0;

    while (!m_redoOps.empty() && m_redoOps.back().groupId == targetGroup) {
        UndoEntry op = keel::move(m_redoOps.back());
        m_redoOps.pop_back();

        ApplyRawReplace(op.offset, op.deletedText.size(), op.insertedText.as_ref());
        restoredCursor = op.afterCursor;
        restoredAnchor = op.afterAnchor;
        m_undoStoredBytes += (op.deletedText.size() + op.insertedText.size());
        m_undoOps.push_back(keel::move(op));
    }

    TrimUndoHistory();
    return TextUndoResult{true, restoredCursor, restoredAnchor};
}

// =============================================================================
// Incremental Change Log
// =============================================================================

bool TextDocument::GetDeltasSince(
    keel::u64 sinceRevision,
    keel::Array<TextEditDelta>& outDeltas
) const {
    outDeltas.clear();
    if (sinceRevision == m_revision) {
        return true;
    }
    if (sinceRevision > m_revision || sinceRevision < m_lastResetRevision) {
        return false;
    }
    const keel::u64 needed = m_revision - sinceRevision;
    if (needed > m_deltaCount) {
        return false;
    }

    outDeltas.reserve(keel::as<keel::usize>(needed));
    const keel::usize oldestPos = (m_deltaWritePos + kDeltaHistoryCapacity - m_deltaCount) % kDeltaHistoryCapacity;
    const keel::usize skip = m_deltaCount - keel::as<keel::usize>(needed);
    for (keel::usize i = 0u; i < keel::as<keel::usize>(needed); ++i) {
        const keel::usize idx = (oldestPos + skip + i) % kDeltaHistoryCapacity;
        const TextEditDelta& d = m_deltaRing[idx];
        if (d.revision != sinceRevision + i + 1u) {
            outDeltas.clear();
            return false;
        }
        outDeltas.push_back(d);
    }
    return true;
}

// =============================================================================
// Self-Check / Invariant Verification
// =============================================================================

bool TextDocument::VerifyInvariants() const noexcept {
    if (m_nodes.empty()) return false;
    if (m_nodes[kNil].color != kColorBlack) return false;
    if (m_nodes[kNil].subtree.bytes != 0u || m_nodes[kNil].subtree.lfCount != 0u) return false;
    if (m_activeNodeCount + m_freeNodes.size() + 1u != m_nodes.size()) return false;

    if (m_root == kNil) {
        return m_activeNodeCount == 0u;
    }
    if (m_nodes[m_root].color != kColorBlack || m_nodes[m_root].parent != kNil) {
        return false;
    }

    keel::usize visitedCount = 0;
    keel::i32 expectedBlackHeight = -1;

    struct Frame {
        keel::u32 node;
        keel::i32 blackSoFar;
    };
    keel::Array<Frame> stack;
    stack.push_back(Frame{m_root, 0});

    while (!stack.empty()) {
        const Frame f = stack.back();
        stack.pop_back();
        const keel::u32 idx = f.node;
        const TreeNode& n = m_nodes[idx];
        ++visitedCount;

        if (n.piece.byteLength == 0u) return false;
        if (n.piece.chunkIdx >= m_chunks.size()) return false;
        const TextChunk& ch = m_chunks[n.piece.chunkIdx];
        if (n.piece.byteStart + n.piece.byteLength > ch.data.size()) return false;

        // Verify piece metrics match ground truth.
        const Piece recomputed = MakePiece(n.piece.chunkIdx, n.piece.byteStart, n.piece.byteLength);
        if (recomputed.lfStart != n.piece.lfStart ||
            recomputed.lfCount != n.piece.lfCount ||
            recomputed.ownPrefixBytes != n.piece.ownPrefixBytes ||
            recomputed.ownSuffixBytes != n.piece.ownSuffixBytes ||
            recomputed.ownMaxLineBytes != n.piece.ownMaxLineBytes ||
            recomputed.ownFlags != n.piece.ownFlags) {
            return false;
        }

        // Verify Red-Red rule.
        if (n.color == kColorRed) {
            if (n.left != kNil && m_nodes[n.left].color == kColorRed) return false;
            if (n.right != kNil && m_nodes[n.right].color == kColorRed) return false;
        }

        // Verify parent links and subtree summary.
        if (n.left != kNil && m_nodes[n.left].parent != idx) return false;
        if (n.right != kNil && m_nodes[n.right].parent != idx) return false;

        const NodeSummary leftSum = (n.left != kNil) ? m_nodes[n.left].subtree : NodeSummary{};
        const NodeSummary rightSum = (n.right != kNil) ? m_nodes[n.right].subtree : NodeSummary{};
        const NodeSummary expectedSum = CombineSummary(CombineSummary(leftSum, PieceSummary(n.piece)), rightSum);
        if (n.subtree.bytes != expectedSum.bytes ||
            n.subtree.lfCount != expectedSum.lfCount ||
            n.subtree.prefixBytes != expectedSum.prefixBytes ||
            n.subtree.suffixBytes != expectedSum.suffixBytes ||
            n.subtree.maxLineBytes != expectedSum.maxLineBytes ||
            n.subtree.flags != expectedSum.flags) {
            return false;
        }

        const keel::i32 nextBlack = f.blackSoFar + ((n.color == kColorBlack) ? 1 : 0);
        if (n.left == kNil) {
            if (expectedBlackHeight < 0) expectedBlackHeight = nextBlack;
            else if (expectedBlackHeight != nextBlack) return false;
        } else {
            stack.push_back(Frame{n.left, nextBlack});
        }
        if (n.right == kNil) {
            if (expectedBlackHeight < 0) expectedBlackHeight = nextBlack;
            else if (expectedBlackHeight != nextBlack) return false;
        } else {
            stack.push_back(Frame{n.right, nextBlack});
        }
    }

    return visitedCount == m_activeNodeCount;
}

} // namespace guikit
