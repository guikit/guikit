#pragma once

#include "guikit/core/id.hpp"
#include "guikit/core/types.hpp"
#include "keel/array.h"
#include "keel/core.h"
#include "keel/optional.h"
#include "keel/string.h"

namespace guikit {

class Context;

/// Single reversible delta edit operation stored in `TextBuffer`'s undo/redo history.
/// Stores only the deleted and inserted slices rather than full document clones.
struct TextEditOp {
    keel::usize pos{0};
    keel::String deletedText;
    keel::String insertedText;
    keel::usize beforeCursor{0};
    keel::usize beforeAnchor{0};
    keel::usize afterCursor{0};
    keel::usize afterAnchor{0};
};

/// Active IME (Input Method Editor) preedit composition state (CJK, dead keys, accented input).
struct ImeCompositionState {
    keel::String text;
    keel::usize cursor{0};
    keel::usize selectionLength{0};
    bool active{false};

    void Clear() noexcept {
        text.clear();
        cursor = 0;
        selectionLength = 0;
        active = false;
    }
};

/// Persistent per-widget diff-based undo/redo history preserved across focus switches.
struct TextEditHistory {
    keel::String lastText;
    keel::Array<TextEditOp> undoOps;
    keel::Array<TextEditOp> redoOps;
};

/// Autonomous UTF-8 text editing buffer with O(delta) operational undo/redo,
/// selection, bracket auto-closing, smart indentation, and keyboard command routing.
class TextBuffer {
public:
    static constexpr keel::usize kMaxUndoHistory = 512u;

    Id id{Id::Null()};
    keel::String text;
    keel::usize cursor{0};
    keel::usize selectionAnchor{0};
    keel::Array<TextEditOp> undoOps;
    keel::Array<TextEditOp> redoOps;
    keel::Optional<keel::usize> preferredColumn;
    bool changed{false};
    bool submitted{false};
    bool multiline{false};
    bool readOnly{false};
    bool autoCloseBrackets{false};
    keel::u8 tabSize{4};
    bool insertSpaces{true};

    [[nodiscard]] bool HasSelection() const noexcept { return cursor != selectionAnchor; }
    [[nodiscard]] keel::usize SelectionStart() const noexcept { return keel::min(cursor, selectionAnchor); }
    [[nodiscard]] keel::usize SelectionEnd() const noexcept { return keel::max(cursor, selectionAnchor); }

    [[nodiscard]] keel::StringRef SelectedText() const noexcept {
        if (!HasSelection()) return {};
        const keel::usize s = keel::min(SelectionStart(), text.size());
        const keel::usize e = keel::min(SelectionEnd(), text.size());
        return text.substr(s, e - s);
    }

    void SelectAll() noexcept {
        selectionAnchor = 0;
        cursor = text.size();
        preferredColumn.reset();
    }

    /// Replaces `[pos .. pos + eraseCount)` with `inserted` and records an O(delta) `TextEditOp`.
    void ReplaceRange(
        keel::usize pos,
        keel::usize eraseCount,
        keel::StringRef inserted,
        keel::usize newCursor,
        keel::usize newSelectionAnchor,
        bool recordUndo = true
    );

    /// Replaces the entire text with `newText`, storing only the minimal prefix/suffix diff in `undoOps`.
    void ApplyDiff(keel::StringRef newText, keel::usize newCursor, keel::usize newSelectionAnchor);

    /// Reverts the last recorded edit operation in O(delta). Returns true if text changed.
    bool Undo();

    /// Re-applies the last undone edit operation in O(delta). Returns true if text changed.
    bool Redo();

    /// Deletes the active selection (recording an O(delta) op). Returns true if text changed.
    bool DeleteSelection();

    /// Inserts sanitized UTF-8 text at the cursor (replacing any selection and handling auto-close brackets).
    void Insert(keel::StringRef utf8Text);

    /// Processes a key chord via `ctx.ResolveAction(key, mods)`. Returns true if consumed.
    bool HandleKey(Context& ctx, Key key, KeyModifiers mods, bool& outLostFocus);

    /// Releases excess heap capacity when no text widget is focused.
    bool TrimIdleMemory() noexcept;

    [[nodiscard]] keel::usize UndoCount() const noexcept { return undoOps.size(); }
    [[nodiscard]] keel::usize RedoCount() const noexcept { return redoOps.size(); }
};

} // namespace guikit
