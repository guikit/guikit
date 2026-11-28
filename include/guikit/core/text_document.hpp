#pragma once

#include "keel/array.h"
#include "keel/core.h"
#include "keel/function_ref.h"
#include "keel/ref.h"
#include "keel/static_array.h"
#include "keel/string.h"

namespace guikit {

/// 0-based line and byte-column coordinate within a TextDocument.
struct TextLineCol {
    keel::usize line{0};     ///< 0-based logical line index
    keel::usize colBytes{0}; ///< 0-based byte offset from start of the line

    [[nodiscard]] constexpr bool operator==(const TextLineCol&) const noexcept = default;
};

/// Byte span of a logical line inside a TextDocument.
struct TextLineRange {
    keel::usize startByte{0};    ///< Global byte offset where the line starts
    keel::usize contentBytes{0}; ///< Byte length excluding trailing '\n' (and '\r' of "\r\n")
    keel::usize totalBytes{0};   ///< Byte length including line break ('\n' or "\r\n"), or contentBytes on last line

    [[nodiscard]] constexpr bool operator==(const TextLineRange&) const noexcept = default;
};

/// Atomic document edit delta recorded in TextDocument's recent delta log.
/// Consumers (syntax checkpoints, folding, breakpoints, diagnostics, WYSIWYG AST)
/// replay these deltas in O(deltas) instead of re-hashing or diffing the whole document.
struct TextEditDelta {
    keel::u64 revision{0};       ///< Document revision produced by this edit
    keel::usize startByte{0};    ///< Global byte offset where replacement began
    keel::usize oldByteLen{0};   ///< Number of bytes removed
    keel::usize newByteLen{0};   ///< Number of bytes inserted
    keel::usize startLine{0};    ///< 0-based line where edit started
    keel::usize oldLineCount{0}; ///< Number of '\n' removed
    keel::usize newLineCount{0}; ///< Number of '\n' inserted
    keel::isize lineDelta{0};    ///< Net line count change (newLineCount - oldLineCount)
};

/// Direction bias when projecting a byte offset located directly at an edit boundary.
enum class TextOffsetBias : keel::u8 {
    Left = 0,  ///< Stay before newly inserted text (e.g. range start / sticky-left anchor)
    Right = 1  ///< Advance past newly inserted text (e.g. caret / sticky-right anchor)
};

/// Projects a global byte offset across a single TextEditDelta in O(1).
[[nodiscard]] constexpr keel::usize MapOffsetThroughEdit(
    keel::usize offset,
    const TextEditDelta& delta,
    TextOffsetBias bias = TextOffsetBias::Right
) noexcept {
    const keel::usize oldEnd = delta.startByte + delta.oldByteLen;
    if (offset < delta.startByte) {
        return offset;
    }
    if (offset == delta.startByte && delta.oldByteLen == 0) {
        return (bias == TextOffsetBias::Right) ? (delta.startByte + delta.newByteLen) : delta.startByte;
    }
    if (offset >= oldEnd) {
        return offset - delta.oldByteLen + delta.newByteLen;
    }
    return (bias == TextOffsetBias::Right) ? (delta.startByte + delta.newByteLen) : delta.startByte;
}

/// Projects a 0-based line index across a single TextEditDelta in O(1).
[[nodiscard]] constexpr keel::usize MapLineThroughEdit(
    keel::usize line,
    const TextEditDelta& delta
) noexcept {
    if (line <= delta.startLine) {
        return line;
    }
    const keel::usize oldEndLine = delta.startLine + delta.oldLineCount;
    if (line <= oldEndLine) {
        return delta.startLine;
    }
    return keel::as<keel::usize>(keel::as<keel::isize>(line) + delta.lineDelta);
}

/// Result returned by TextDocument::Undo() and TextDocument::Redo().
struct TextUndoResult {
    bool changed{false};
    keel::usize cursor{0};
    keel::usize selectionAnchor{0};
};

/// High-performance, domain-agnostic UTF-8 text document backed by a 64 KB-chunked
/// Red-Black Piece Tree with a SumTree monoid for O(1) line/max-line/ASCII metadata
/// and O(log P) edits, line lookups, and bounded-slice reads.
///
/// Designed for both 1M+ line files and multi-megabyte single-line files, suitable
/// for CodeEditor, log viewers, and source-backed Markdown WYSIWYG editors.
class TextDocument {
public:
    static constexpr keel::u32 kChunkTargetBytes = 65536u;
    static constexpr keel::usize kDeltaHistoryCapacity = 64u;
    static constexpr keel::usize kMaxUndoGroups = 512u;
    static constexpr keel::usize kMaxUndoBytes = 64u * 1024u * 1024u;

    /// RAII helper for grouping multiple edits into a single atomic Undo/Redo transaction.
    class EditGroupScope {
    public:
        explicit EditGroupScope(TextDocument& doc) noexcept : m_doc(&doc) {
            m_doc->BeginEditGroup();
        }
        ~EditGroupScope() noexcept {
            if (m_doc) m_doc->EndEditGroup();
        }
        EditGroupScope(const EditGroupScope&) = delete;
        EditGroupScope& operator=(const EditGroupScope&) = delete;
        EditGroupScope(EditGroupScope&& other) noexcept : m_doc(other.m_doc) {
            other.m_doc = nullptr;
        }
        EditGroupScope& operator=(EditGroupScope&&) = delete;

    private:
        TextDocument* m_doc{nullptr};
    };

    TextDocument();
    explicit TextDocument(keel::StringRef initialText);
    ~TextDocument() noexcept = default;

    TextDocument(const TextDocument&) = delete;
    TextDocument& operator=(const TextDocument&) = delete;
    TextDocument(TextDocument&&) noexcept = default;
    TextDocument& operator=(TextDocument&&) noexcept = default;

    /// Creates an explicit deep copy of the document's current text (history starts fresh).
    [[nodiscard]] TextDocument Clone() const;

    // --- Bulk Load ---

    /// Clears the document text and undo/redo history, bumping Revision().
    void Clear();

    /// Replaces the entire document with `text` in O(N) using 64 KB chunking and
    /// bottom-up balanced Red-Black Tree construction. Clears undo/redo history.
    void SetText(keel::StringRef text);

    // --- O(1) Metadata Queries ---

    [[nodiscard]] bool Empty() const noexcept { return ByteLength() == 0; }
    [[nodiscard]] keel::usize ByteLength() const noexcept;
    [[nodiscard]] keel::usize LineCount() const noexcept;

    /// Upper bound on content byte length of the longest line in the entire document (O(1)).
    /// Exact for LF documents; within +1 byte for CRLF documents.
    [[nodiscard]] keel::usize MaxLineByteLength() const noexcept;

    /// Returns true in O(1) if every byte in the document is printable ASCII (0x20..0x7E)
    /// or a valid LF/CRLF line break (no tabs, control bytes, stray CR, or UTF-8 multibyte).
    [[nodiscard]] bool IsPureAsciiNoTabs() const noexcept;

    /// Monotonically increasing revision counter bumped on every document mutation.
    [[nodiscard]] keel::u64 Revision() const noexcept { return m_revision; }

    /// Number of active pieces (nodes) in the Piece Tree.
    [[nodiscard]] keel::usize PieceCount() const noexcept { return m_activeNodeCount; }

    // --- O(log P) Coordinate & Slice Queries ---

    /// Returns the starting global byte offset of 0-based `line` (clamped to valid lines).
    [[nodiscard]] keel::usize LineStartByte(keel::usize line) const noexcept;

    /// Returns the byte range of 0-based `line` (clamped to valid lines).
    [[nodiscard]] TextLineRange LineByteRange(keel::usize line) const noexcept;

    /// Converts a global byte offset to 0-based (line, colBytes).
    [[nodiscard]] TextLineCol OffsetToLineCol(keel::usize byteOffset) const noexcept;

    /// Converts 0-based (line, colBytes) to a global byte offset clamped to the line content.
    [[nodiscard]] keel::usize LineColToOffset(keel::usize line, keel::usize colBytes) const noexcept;

    /// Returns the single byte at `byteOffset` (or '\0' if out of bounds).
    [[nodiscard]] char ByteAt(keel::usize byteOffset) const noexcept;

    /// Returns true if all pieces overlapping `[byteOffset, byteOffset + byteLen)` contain
    /// only printable ASCII and LF/CRLF line breaks (zero tabs / non-ASCII).
    [[nodiscard]] bool RangeIsPureAsciiNoTabs(keel::usize byteOffset, keel::usize byteLen) const noexcept;

    /// Reads `[byteOffset, byteOffset + byteLen)` into `out` in O(log P + byteLen).
    void ReadSlice(keel::usize byteOffset, keel::usize byteLen, keel::String& out) const;

    /// Reads a horizontal slice `[colByteStart, colByteStart + maxBytes)` from 0-based `line`
    /// (never materializing the rest of a multi-megabyte line).
    void ReadLineSlice(
        keel::usize line,
        keel::usize colByteStart,
        keel::usize maxBytes,
        keel::String& out
    ) const;

    /// Reads the entire content of 0-based `line` (excluding '\r' / '\n') into `out`.
    /// Prefer ReadLineSlice() in viewport hot paths to stay O(visibleCols) on huge lines.
    void ReadLine(keel::usize line, keel::String& out) const;

    /// Zero-copy streaming iteration over piece slices overlapping `[byteOffset, byteOffset + byteLen)`.
    /// Return false from `visitor` to stop early.
    void ForEachChunkInRange(
        keel::usize byteOffset,
        keel::usize byteLen,
        keel::FunctionRef<bool(keel::StringRef)> visitor
    ) const;

    /// Extracts the complete document into `out`.
    void ExtractText(keel::String& out) const;

    /// Returns a contiguous copy of the complete document.
    [[nodiscard]] keel::String ToString() const;

    // --- Bounded-Window UTF-8 & Word Navigation (O(log P), safe on 10 MB lines) ---

    /// Snaps `byteOffset` backward (at most 3 bytes) to a valid UTF-8 codepoint boundary.
    [[nodiscard]] keel::usize SnapToUtf8Boundary(keel::usize byteOffset) const noexcept;

    /// Returns the byte offset of the previous UTF-8 codepoint before `byteOffset`.
    [[nodiscard]] keel::usize PrevCharOffset(keel::usize byteOffset) const noexcept;

    /// Returns the byte offset of the next UTF-8 codepoint after `byteOffset`.
    [[nodiscard]] keel::usize NextCharOffset(keel::usize byteOffset) const noexcept;

    /// Returns the byte offset of the previous word boundary before `byteOffset`.
    [[nodiscard]] keel::usize PrevWordOffset(keel::usize byteOffset) const noexcept;

    /// Returns the byte offset of the next word boundary after `byteOffset`.
    [[nodiscard]] keel::usize NextWordOffset(keel::usize byteOffset) const noexcept;

    /// Computes the `[outStart, outEnd)` byte range of the word/token at `byteOffset`.
    void WordBoundsAt(
        keel::usize byteOffset,
        keel::usize& outStart,
        keel::usize& outEnd
    ) const noexcept;

    // --- O(log P) Mutations & Transactions ---

    /// Replaces `[byteOffset, byteOffset + eraseBytes)` with `insertUtf8`.
    /// Records an O(delta) undo entry when `recordUndo == true`.
    TextEditDelta ReplaceRange(
        keel::usize byteOffset,
        keel::usize eraseBytes,
        keel::StringRef insertUtf8,
        keel::usize beforeCursor = 0,
        keel::usize beforeAnchor = 0,
        keel::usize afterCursor = 0,
        keel::usize afterAnchor = 0,
        bool recordUndo = true,
        bool allowCoalesce = false
    );

    /// Inserts `insertUtf8` at `byteOffset`.
    TextEditDelta Insert(
        keel::usize byteOffset,
        keel::StringRef insertUtf8,
        bool recordUndo = true
    );

    /// Erases `eraseBytes` starting at `byteOffset`.
    TextEditDelta Erase(
        keel::usize byteOffset,
        keel::usize eraseBytes,
        bool recordUndo = true
    );

    /// Begins a nestable atomic Undo/Redo edit group (all edits until matching EndEditGroup undo together).
    void BeginEditGroup() noexcept;

    /// Ends the current atomic Undo/Redo edit group.
    void EndEditGroup() noexcept;

    /// Prevents subsequent typing edits from coalescing into the previous undo operation.
    void BreakUndoCoalesce() noexcept { m_canCoalesceUndo = false; }

    // --- Undo / Redo ---

    [[nodiscard]] bool CanUndo() const noexcept { return !m_undoOps.empty(); }
    [[nodiscard]] bool CanRedo() const noexcept { return !m_redoOps.empty(); }
    [[nodiscard]] keel::usize UndoCount() const noexcept { return m_undoOps.size(); }
    [[nodiscard]] keel::usize RedoCount() const noexcept { return m_redoOps.size(); }
    void ClearHistory() noexcept;

    /// Reverts the most recent edit (or edit group) in O(delta).
    TextUndoResult Undo();

    /// Re-applies the most recently undone edit (or edit group) in O(delta).
    TextUndoResult Redo();

    // --- Incremental Change Log ---

    /// Populates `outDeltas` with all edit deltas strictly after `sinceRevision` in chronological order.
    /// Returns true if the full contiguous delta chain is available; returns false if a bulk reset
    /// occurred or more than `kDeltaHistoryCapacity` edits elapsed (caller should rebuild its cache).
    [[nodiscard]] bool GetDeltasSince(
        keel::u64 sinceRevision,
        keel::Array<TextEditDelta>& outDeltas
    ) const;

    // --- Diagnostics & Self-Check ---

    /// Verifies all Red-Black Tree, SumTree monoid, and chunk LF index invariants.
    [[nodiscard]] bool VerifyInvariants() const noexcept;

private:
    static constexpr keel::u32 kNil = 0u;
    static constexpr keel::u8 kColorBlack = 0u;
    static constexpr keel::u8 kColorRed = 1u;
    static constexpr keel::u8 kFlagHasNonAscii = 1u << 0;
    static constexpr keel::u8 kFlagHasTabOrCtrl = 1u << 1;

    struct NodeSummary {
        keel::usize bytes{0};
        keel::usize lfCount{0};
        keel::usize prefixBytes{0};
        keel::usize suffixBytes{0};
        keel::usize maxLineBytes{0};
        keel::u8 flags{0};
    };

    struct TextChunk {
        keel::String data;
        keel::Array<keel::u32> lfOffsets;
        bool immutable{true};
    };

    struct Piece {
        keel::u32 chunkIdx{0};
        keel::u32 byteStart{0};
        keel::u32 byteLength{0};
        keel::u32 lfStart{0};
        keel::u32 lfCount{0};
        keel::u32 ownPrefixBytes{0};
        keel::u32 ownSuffixBytes{0};
        keel::u32 ownMaxLineBytes{0};
        keel::u8 ownFlags{0};
    };

    struct TreeNode {
        Piece piece{};
        NodeSummary subtree{};
        keel::u32 left{0};
        keel::u32 right{0};
        keel::u32 parent{0};
        keel::u8 color{kColorBlack};
    };

    struct NodeLocation {
        keel::u32 node{0};
        keel::usize nodeStartByte{0};
        keel::usize relOffset{0};
    };

    struct UndoEntry {
        keel::usize offset{0};
        keel::String deletedText;
        keel::String insertedText;
        keel::usize beforeCursor{0};
        keel::usize beforeAnchor{0};
        keel::usize afterCursor{0};
        keel::usize afterAnchor{0};
        keel::u64 groupId{0};
    };

    [[nodiscard]] static constexpr NodeSummary CombineSummary(
        const NodeSummary& a,
        const NodeSummary& b
    ) noexcept {
        if (a.bytes == 0) return b;
        if (b.bytes == 0) return a;
        NodeSummary out{};
        out.bytes = a.bytes + b.bytes;
        out.lfCount = a.lfCount + b.lfCount;
        out.prefixBytes = (a.lfCount == 0) ? (a.bytes + b.prefixBytes) : a.prefixBytes;
        out.suffixBytes = (b.lfCount == 0) ? (a.suffixBytes + b.bytes) : b.suffixBytes;
        const keel::usize joined = a.suffixBytes + b.prefixBytes;
        out.maxLineBytes = keel::max(keel::max(a.maxLineBytes, b.maxLineBytes), joined);
        out.flags = keel::as<keel::u8>(a.flags | b.flags);
        return out;
    }

    [[nodiscard]] static constexpr NodeSummary PieceSummary(const Piece& p) noexcept {
        NodeSummary s{};
        s.bytes = p.byteLength;
        s.lfCount = p.lfCount;
        s.prefixBytes = p.ownPrefixBytes;
        s.suffixBytes = p.ownSuffixBytes;
        s.maxLineBytes = p.ownMaxLineBytes;
        s.flags = p.ownFlags;
        return s;
    }

    void InitSentinel();
    void ResetTreePool();
    [[nodiscard]] keel::u32 AllocNode(const Piece& piece);
    void FreeNode(keel::u32 idx) noexcept;

    [[nodiscard]] Piece MakePiece(keel::u32 chunkIdx, keel::u32 byteStart, keel::u32 byteLength) const noexcept;
    void ComputePieceMetrics(Piece& p) const noexcept;
    void AppendTextToChunks(keel::StringRef text, keel::Array<Piece>& outPieces);

    void UpdateNode(keel::u32 idx) noexcept;
    void UpdateUpwards(keel::u32 idx) noexcept;
    void RotateLeft(keel::u32 x) noexcept;
    void RotateRight(keel::u32 y) noexcept;
    void InsertFixup(keel::u32 z) noexcept;
    void Transplant(keel::u32 u, keel::u32 v) noexcept;
    void DeleteFixup(keel::u32 x, keel::u32 xParent) noexcept;
    void DeleteNode(keel::u32 z) noexcept;

    void InsertNodeAtRoot(keel::u32 z) noexcept;
    void InsertNodeBefore(keel::u32 target, keel::u32 z) noexcept;
    void InsertNodeAfter(keel::u32 target, keel::u32 z) noexcept;

    [[nodiscard]] keel::u32 BuildBalancedSubtree(
        keel::ArrayRef<Piece> pieces,
        keel::usize lo,
        keel::usize hi,
        keel::u32 depth,
        keel::u32 maxDepth,
        keel::u32 parent
    );

    [[nodiscard]] keel::u32 MinNode(keel::u32 idx) const noexcept;
    [[nodiscard]] keel::u32 MaxNode(keel::u32 idx) const noexcept;
    [[nodiscard]] keel::u32 NextNode(keel::u32 idx) const noexcept;
    [[nodiscard]] keel::u32 PrevNode(keel::u32 idx) const noexcept;
    [[nodiscard]] NodeLocation FindNodeAtOffset(keel::usize byteOffset) const noexcept;

    [[nodiscard]] bool TryCoalesceInsertIntoNode(keel::u32 nodeIdx, keel::StringRef text);
    void RawInsert(keel::usize byteOffset, keel::StringRef text);
    void RawErase(keel::usize byteOffset, keel::usize eraseBytes);
    TextEditDelta ApplyRawReplace(keel::usize byteOffset, keel::usize eraseBytes, keel::StringRef insertUtf8);
    void PushDelta(const TextEditDelta& delta) noexcept;
    void TrimUndoHistory() noexcept;
    [[nodiscard]] keel::usize ReadRawWindow(keel::usize byteOffset, char* dst, keel::usize maxBytes) const noexcept;

    keel::Array<TextChunk> m_chunks;
    keel::Array<TreeNode> m_nodes;
    keel::Array<keel::u32> m_freeNodes;
    keel::u32 m_root{kNil};
    keel::u32 m_activeAddChunk{0xFFFFFFFFu};
    keel::usize m_activeNodeCount{0};

    keel::u64 m_revision{0};
    keel::u64 m_lastResetRevision{0};
    keel::StaticArray<TextEditDelta, kDeltaHistoryCapacity> m_deltaRing{};
    keel::usize m_deltaWritePos{0};
    keel::usize m_deltaCount{0};

    keel::Array<UndoEntry> m_undoOps;
    keel::Array<UndoEntry> m_redoOps;
    keel::usize m_undoStoredBytes{0};
    keel::u64 m_nextGroupId{1};
    keel::u64 m_activeGroupId{0};
    keel::u32 m_editGroupDepth{0};
    bool m_canCoalesceUndo{false};
};

} // namespace guikit
