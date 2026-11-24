#include "guikit/core/text_buffer.hpp"
#include "guikit/context.hpp"
#include "guikit/core/text_util.hpp"
#include "keel/math.h"
#include "keel/memory.h"
#include "keel/utf8.h"

namespace guikit {

namespace {

[[nodiscard]] keel::usize StrRfindBefore(keel::StringRef s, char c, keel::usize maxExclusivePos) noexcept {
    if (maxExclusivePos == 0 || s.empty()) {
        return keel::StringRef::npos;
    }
    return s.substr(0, keel::min(maxExclusivePos, s.size())).rfind(c);
}

void StrEraseRange(keel::String& s, keel::usize pos, keel::usize count) noexcept {
    if (pos >= s.size() || count == 0) {
        return;
    }
    const keel::usize actual = keel::min(count, s.size() - pos);
    const keel::usize tail = s.size() - (pos + actual);
    if (tail > 0) {
        keel::mem_move(s.data() + pos, s.data() + pos + actual, tail);
    }
    s.resize(s.size() - actual);
}

void StrInsert(keel::String& s, keel::usize pos, keel::StringRef ins) {
    if (ins.empty()) {
        return;
    }
    pos = keel::min(pos, s.size());
    if (pos == s.size()) {
        s.append(ins);
        return;
    }
    if (s.data() != nullptr && ins.data() >= s.data() && ins.data() <= s.data() + s.size()) {
        keel::String copy = keel::String::from(ins);
        StrInsert(s, pos, copy.as_ref());
        return;
    }
    const keel::usize oldLen = s.size();
    const keel::usize insLen = ins.size();
    s.resize(oldLen + insLen);
    keel::mem_move(s.data() + pos + insLen, s.data() + pos, oldLen - pos);
    keel::mem_copy(s.data() + pos, ins.data(), insLen);
}

void StrReplace(keel::String& s, keel::usize pos, keel::usize count, keel::StringRef rep) {
    pos = keel::min(pos, s.size());
    if (s.data() != nullptr && rep.data() >= s.data() && rep.data() <= s.data() + s.size()) {
        keel::String copy = keel::String::from(rep);
        StrReplace(s, pos, count, copy.as_ref());
        return;
    }
    StrEraseRange(s, pos, count);
    StrInsert(s, pos, rep);
}

} // namespace

void TextBuffer::ReplaceRange(
    keel::usize pos,
    keel::usize eraseCount,
    keel::StringRef inserted,
    keel::usize newCursor,
    keel::usize newSelectionAnchor,
    bool recordUndo
) {
    pos = keel::min(pos, text.size());
    const keel::usize actualErase = keel::min(eraseCount, text.size() - pos);
    const keel::StringRef deletedSlice = text.substr(pos, actualErase);

    if (deletedSlice == inserted) {
        cursor = keel::min(newCursor, text.size());
        selectionAnchor = keel::min(newSelectionAnchor, text.size());
        return;
    }

    // Copy `inserted` upfront if it aliases `text`
    keel::String safeInserted;
    keel::StringRef insRef = inserted;
    if (text.data() != nullptr && inserted.data() >= text.data() && inserted.data() <= text.data() + text.size()) {
        safeInserted = keel::String::from(inserted);
        insRef = safeInserted.as_ref();
    }

    if (recordUndo) {
        TextEditOp op{};
        op.pos = pos;
        op.deletedText = keel::String::from(deletedSlice);
        op.insertedText = keel::String::from(insRef);
        op.beforeCursor = cursor;
        op.beforeAnchor = selectionAnchor;
        op.afterCursor = newCursor;
        op.afterAnchor = newSelectionAnchor;
        if (undoOps.size() >= kMaxUndoHistory) {
            undoOps.remove_at(0);
        }
        undoOps.push_back(keel::move(op));
        redoOps.clear();
    }

    StrReplace(text, pos, actualErase, insRef);
    cursor = keel::min(newCursor, text.size());
    selectionAnchor = keel::min(newSelectionAnchor, text.size());
    if (recordUndo && !undoOps.empty()) {
        undoOps.back().afterCursor = cursor;
        undoOps.back().afterAnchor = selectionAnchor;
    }
    preferredColumn.reset();
    changed = true;
}

void TextBuffer::ApplyDiff(keel::StringRef newText, keel::usize newCursor, keel::usize newSelectionAnchor) {
    const keel::StringRef oldRef = text.as_ref();
    if (oldRef == newText) {
        cursor = keel::min(newCursor, text.size());
        selectionAnchor = keel::min(newSelectionAnchor, text.size());
        preferredColumn.reset();
        return;
    }

    const keel::usize minLen = keel::min(oldRef.size(), newText.size());
    keel::usize prefix = 0;
    while (prefix < minLen && oldRef[prefix] == newText[prefix]) {
        ++prefix;
    }
    keel::usize suffix = 0;
    while (suffix < (minLen - prefix) &&
           oldRef[oldRef.size() - 1 - suffix] == newText[newText.size() - 1 - suffix]) {
        ++suffix;
    }

    const keel::usize eraseCount = oldRef.size() - prefix - suffix;
    const keel::StringRef inserted = newText.substr(prefix, newText.size() - prefix - suffix);
    ReplaceRange(prefix, eraseCount, inserted, newCursor, newSelectionAnchor, true);
}

bool TextBuffer::Undo() {
    while (!undoOps.empty()) {
        TextEditOp op = keel::move(undoOps.back());
        undoOps.pop_back();
        const keel::usize pos = keel::min(op.pos, text.size());
        const keel::usize eraseLen = keel::min(op.insertedText.size(), text.size() - pos);
        StrReplace(text, pos, eraseLen, op.deletedText.as_ref());
        cursor = keel::min(op.beforeCursor, text.size());
        selectionAnchor = keel::min(op.beforeAnchor, text.size());
        preferredColumn.reset();
        changed = true;
        if (redoOps.size() >= kMaxUndoHistory) {
            redoOps.remove_at(0);
        }
        redoOps.push_back(keel::move(op));
        return true;
    }
    return false;
}

bool TextBuffer::Redo() {
    while (!redoOps.empty()) {
        TextEditOp op = keel::move(redoOps.back());
        redoOps.pop_back();
        const keel::usize pos = keel::min(op.pos, text.size());
        const keel::usize eraseLen = keel::min(op.deletedText.size(), text.size() - pos);
        StrReplace(text, pos, eraseLen, op.insertedText.as_ref());
        cursor = keel::min(op.afterCursor, text.size());
        selectionAnchor = keel::min(op.afterAnchor, text.size());
        preferredColumn.reset();
        changed = true;
        if (undoOps.size() >= kMaxUndoHistory) {
            undoOps.remove_at(0);
        }
        undoOps.push_back(keel::move(op));
        return true;
    }
    return false;
}

bool TextBuffer::DeleteSelection() {
    if (!HasSelection() || readOnly) {
        return false;
    }
    preferredColumn.reset();
    const keel::usize s = keel::min(SelectionStart(), text.size());
    const keel::usize e = keel::min(SelectionEnd(), text.size());
    if (e > s) {
        ReplaceRange(s, e - s, keel::StringRef{}, s, s, true);
        return true;
    }
    selectionAnchor = cursor;
    return false;
}

void TextBuffer::Insert(keel::StringRef utf8Text) {
    if (readOnly || utf8Text.empty()) {
        return;
    }

    keel::String sanitized = keel::String::with_capacity(utf8Text.size());
    for (char c : utf8Text) {
        const keel::u8 u = keel::as<keel::u8>(c);
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            if (multiline) {
                sanitized.push_back('\n');
            }
            continue;
        }
        if (u < 32 && c != '\t') {
            continue;
        }
        sanitized.push_back(c);
    }

    if (sanitized.empty() && !HasSelection()) {
        return;
    }

    preferredColumn.reset();

    if (autoCloseBrackets && multiline && !readOnly && sanitized.size() == 1) {
        const char c = sanitized[0];
        const keel::usize safeCur = keel::min(cursor, text.size());

        // Overtype closing bracket/quote if cursor is right before it
        if (!HasSelection() && safeCur < text.size() &&
            (c == ')' || c == ']' || c == '}' || c == '"' || c == '\'') &&
            text[safeCur] == c) {
            cursor = safeCur + 1;
            selectionAnchor = cursor;
            return;
        }

        char closeChar = 0;
        if (c == '(') closeChar = ')';
        else if (c == '[') closeChar = ']';
        else if (c == '{') closeChar = '}';
        else if (c == '"') closeChar = '"';
        else if (c == '\'') closeChar = '\'';

        if (closeChar != 0) {
            if (HasSelection()) {
                const keel::usize s = keel::min(SelectionStart(), text.size());
                const keel::usize e = keel::min(SelectionEnd(), text.size());
                keel::String wrapped = keel::String::with_capacity((e - s) + 2);
                wrapped.push_back(c);
                wrapped.append(text.substr(s, e - s));
                wrapped.push_back(closeChar);
                ReplaceRange(s, e - s, wrapped.as_ref(), e + 1, s + 1, true);
                return;
            } else {
                bool allowAutoClose = true;
                if (c == '\'' && safeCur > 0) {
                    const char prev = text[safeCur - 1];
                    const bool prevIdentOrClose =
                        (prev >= 'a' && prev <= 'z') ||
                        (prev >= 'A' && prev <= 'Z') ||
                        (prev >= '0' && prev <= '9') ||
                        prev == '_' || prev == ')' || prev == ']' || prev == '}' || prev == '.';
                    if (prevIdentOrClose) {
                        allowAutoClose = false; // e.g. MATLAB matrix transpose A' or A.'
                    }
                }
                if ((c == '"' || c == '\'') && safeCur > 0 && (text[safeCur - 1] == '\\' || text[safeCur - 1] == c)) {
                    allowAutoClose = false;
                }
                const bool nextOk = (safeCur == text.size()) ||
                                    text[safeCur] == '\n' || text[safeCur] == ' ' || text[safeCur] == '\t' ||
                                    text[safeCur] == ')' || text[safeCur] == ']' || text[safeCur] == '}' ||
                                    text[safeCur] == ',' || text[safeCur] == ';';
                if (allowAutoClose && nextOk) {
                    const char pairStr[2] = {c, closeChar};
                    ReplaceRange(safeCur, 0, keel::StringRef(pairStr, 2), safeCur + 1, safeCur + 1, true);
                    return;
                }
            }
        }
    }

    const keel::usize s = HasSelection() ? keel::min(SelectionStart(), text.size()) : keel::min(cursor, text.size());
    const keel::usize e = HasSelection() ? keel::min(SelectionEnd(), text.size()) : s;
    const keel::usize newCur = s + sanitized.size();
    ReplaceRange(s, e - s, sanitized.as_ref(), newCur, newCur, true);
}

bool TextBuffer::HandleKey(Context& ctx, Key key, KeyModifiers mods, bool& outLostFocus) {
    const KeyAction action = ctx.ResolveAction(key, mods);
    const bool isVerticalMove =
        action == KeyAction::MoveUp || action == KeyAction::SelectUp ||
        action == KeyAction::MoveDown || action == KeyAction::SelectDown ||
        action == KeyAction::MovePageUp || action == KeyAction::SelectPageUp ||
        action == KeyAction::MovePageDown || action == KeyAction::SelectPageDown;
    if (!isVerticalMove) {
        preferredColumn.reset();
    }

    auto utf8OffsetAtColumn = [](keel::StringRef lineSv, keel::usize targetCodepointCol) noexcept -> keel::usize {
        keel::usize byteOff = 0;
        for (keel::usize colIdx = 0; colIdx < targetCodepointCol && byteOff < lineSv.size(); ++colIdx) {
            byteOff = keel::utf8_next_boundary(lineSv, byteOff);
        }
        return keel::min(byteOff, lineSv.size());
    };

    auto moveLinesVertically = [&](keel::i32 lineDelta, bool extendSel) -> bool {
        if (!multiline) return false;
        const keel::usize safeCur = keel::min(cursor, text.size());
        const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', safeCur);
        keel::usize curLineStart = (prevNl == keel::StringRef::npos) ? 0 : (prevNl + 1);
        const keel::usize col = keel::utf8_codepoint_count(
            text.substr(curLineStart, safeCur - curLineStart)
        );
        if (!preferredColumn.has_value()) {
            preferredColumn = col;
        }
        const keel::usize targetCol = *preferredColumn;

        if (lineDelta < 0) {
            const keel::i32 steps = -lineDelta;
            for (keel::i32 s = 0; s < steps; ++s) {
                if (curLineStart == 0) {
                    if (s == 0) {
                        cursor = 0;
                        if (!extendSel) selectionAnchor = cursor;
                        return true;
                    }
                    break;
                }
                const keel::usize prevLineEnd = curLineStart - 1;
                const keel::usize prevPrevNl = StrRfindBefore(text.as_ref(), '\n', prevLineEnd);
                curLineStart = (prevPrevNl == keel::StringRef::npos) ? 0 : (prevPrevNl + 1);
            }
            keel::usize lineEnd = text.find('\n', curLineStart);
            if (lineEnd == keel::StringRef::npos) lineEnd = text.size();
            const keel::StringRef targetLine = text.substr(curLineStart, lineEnd - curLineStart);
            cursor = curLineStart + utf8OffsetAtColumn(targetLine, targetCol);
        } else if (lineDelta > 0) {
            const keel::i32 steps = lineDelta;
            keel::usize curLineEnd = text.find('\n', curLineStart);
            for (keel::i32 s = 0; s < steps; ++s) {
                if (curLineEnd == keel::StringRef::npos) {
                    if (s == 0) {
                        cursor = text.size();
                        if (!extendSel) selectionAnchor = cursor;
                        return true;
                    }
                    break;
                }
                curLineStart = curLineEnd + 1;
                curLineEnd = text.find('\n', curLineStart);
            }
            const keel::usize lineEnd = (curLineEnd == keel::StringRef::npos) ? text.size() : curLineEnd;
            const keel::StringRef targetLine = text.substr(curLineStart, lineEnd - curLineStart);
            cursor = curLineStart + utf8OffsetAtColumn(targetLine, targetCol);
        }

        if (!extendSel) {
            selectionAnchor = cursor;
        }
        return true;
    };

    switch (action) {
        case KeyAction::MoveLineUp:
        case KeyAction::MoveLineDown: {
            if (!multiline || readOnly) return false;
            const keel::usize safeCur = keel::min(cursor, text.size());
            const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', safeCur);
            const keel::usize curLineStart = (prevNl == keel::StringRef::npos) ? 0 : (prevNl + 1);
            keel::usize curLineEnd = text.find('\n', safeCur);
            if (curLineEnd == keel::StringRef::npos) curLineEnd = text.size();
            const keel::usize col = safeCur - curLineStart;

            if (action == KeyAction::MoveLineUp) {
                if (curLineStart > 0) {
                    const keel::usize prevLineEnd = curLineStart - 1;
                    const keel::usize prevPrevNl = StrRfindBefore(text.as_ref(), '\n', prevLineEnd);
                    const keel::usize prevLineStart = (prevPrevNl == keel::StringRef::npos) ? 0 : (prevPrevNl + 1);
                    keel::String curLine = keel::String::from(text.substr(curLineStart, curLineEnd - curLineStart));
                    keel::String prevLine = keel::String::from(text.substr(prevLineStart, prevLineEnd - prevLineStart));
                    keel::String swapped = keel::String::with_capacity(curLine.size() + 1 + prevLine.size());
                    swapped.append(curLine.as_ref());
                    swapped.push_back('\n');
                    swapped.append(prevLine.as_ref());
                    const keel::usize newCur = prevLineStart + keel::min(col, curLine.size());
                    ReplaceRange(prevLineStart, curLineEnd - prevLineStart, swapped.as_ref(), newCur, newCur, true);
                }
            } else {
                if (curLineEnd < text.size()) {
                    const keel::usize nextLineStart = curLineEnd + 1;
                    keel::usize nextLineEnd = text.find('\n', nextLineStart);
                    if (nextLineEnd == keel::StringRef::npos) nextLineEnd = text.size();
                    keel::String curLine = keel::String::from(text.substr(curLineStart, curLineEnd - curLineStart));
                    keel::String nextLine = keel::String::from(text.substr(nextLineStart, nextLineEnd - nextLineStart));
                    keel::String swapped = keel::String::with_capacity(nextLine.size() + 1 + curLine.size());
                    swapped.append(nextLine.as_ref());
                    swapped.push_back('\n');
                    swapped.append(curLine.as_ref());
                    const keel::usize newCur = curLineStart + nextLine.size() + 1 + keel::min(col, curLine.size());
                    ReplaceRange(curLineStart, nextLineEnd - curLineStart, swapped.as_ref(), newCur, newCur, true);
                }
            }
            return true;
        }

        case KeyAction::SelectAll:
            SelectAll();
            return true;

        case KeyAction::Copy:
            if (HasSelection()) {
                ctx.SetClipboardText(SelectedText());
            }
            return true;

        case KeyAction::Cut:
            if (HasSelection()) {
                ctx.SetClipboardText(SelectedText());
                if (!readOnly) {
                    DeleteSelection();
                }
            }
            return true;

        case KeyAction::Paste:
            if (!readOnly) {
                const keel::String clip = ctx.GetClipboardText();
                if (!clip.empty()) {
                    Insert(clip.as_ref());
                }
            }
            return true;

        case KeyAction::Undo:
            if (!readOnly) {
                Undo();
            }
            return true;

        case KeyAction::Redo:
            if (!readOnly) {
                Redo();
            }
            return true;

        case KeyAction::DuplicateLine: {
            if (!multiline || readOnly) return false;
            if (HasSelection()) {
                const keel::String sel = keel::String::from(SelectedText());
                if (!sel.empty()) {
                    keel::usize nextPos = text.find(sel.as_ref(), SelectionEnd());
                    if (nextPos == keel::StringRef::npos) {
                        nextPos = text.find(sel.as_ref(), 0);
                    }
                    if (nextPos != keel::StringRef::npos && nextPos != SelectionStart()) {
                        selectionAnchor = nextPos;
                        cursor = nextPos + sel.size();
                        return true;
                    }
                }
            }
            const keel::usize safeCur = keel::min(cursor, text.size());
            const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', safeCur);
            const keel::usize curLineStart = (prevNl == keel::StringRef::npos) ? 0 : (prevNl + 1);
            keel::usize curLineEnd = text.find('\n', safeCur);
            if (curLineEnd == keel::StringRef::npos) curLineEnd = text.size();
            const keel::usize col = safeCur - curLineStart;
            keel::String dup = keel::String::with_capacity(1 + (curLineEnd - curLineStart));
            dup.push_back('\n');
            dup.append(text.substr(curLineStart, curLineEnd - curLineStart));
            const keel::usize newCur = curLineEnd + 1 + col;
            ReplaceRange(curLineEnd, 0, dup.as_ref(), newCur, newCur, true);
            return true;
        }

        case KeyAction::DeleteLine: {
            if (!multiline || readOnly) return false;
            const keel::usize safeCur = keel::min(cursor, text.size());
            const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', safeCur);
            const keel::usize curLineStart = (prevNl == keel::StringRef::npos) ? 0 : (prevNl + 1);
            const keel::usize curLineEnd = text.find('\n', safeCur);
            if (curLineEnd != keel::StringRef::npos) {
                ReplaceRange(curLineStart, curLineEnd - curLineStart + 1, keel::StringRef{}, curLineStart, curLineStart, true);
            } else if (curLineStart > 0) {
                ReplaceRange(curLineStart - 1, text.size() - (curLineStart - 1), keel::StringRef{}, curLineStart - 1, curLineStart - 1, true);
            } else if (!text.empty()) {
                ReplaceRange(0, text.size(), keel::StringRef{}, 0, 0, true);
            }
            return true;
        }

        case KeyAction::DeletePrevChar:
        case KeyAction::DeletePrevWord: {
            if (readOnly) return true;
            const bool byWord = (action == KeyAction::DeletePrevWord);
            if (HasSelection()) {
                DeleteSelection();
            } else if (cursor > 0 && !text.empty()) {
                const keel::usize safeCur = keel::min(cursor, text.size());
                if (autoCloseBrackets && multiline && !byWord && safeCur < text.size()) {
                    const char prevC = text[safeCur - 1];
                    const char nextC = text[safeCur];
                    if ((prevC == '(' && nextC == ')') ||
                        (prevC == '[' && nextC == ']') ||
                        (prevC == '{' && nextC == '}') ||
                        (prevC == '"' && nextC == '"') ||
                        (prevC == '\'' && nextC == '\'')) {
                        ReplaceRange(safeCur - 1, 2, keel::StringRef{}, safeCur - 1, safeCur - 1, true);
                        return true;
                    }
                }
                const keel::usize prev = byWord
                    ? TextUtil::PrevWordBoundary(text.as_ref(), safeCur)
                    : TextUtil::PrevCharBoundary(text.as_ref(), safeCur);
                if (safeCur > prev) {
                    ReplaceRange(prev, safeCur - prev, keel::StringRef{}, prev, prev, true);
                }
            } else {
                return false;
            }
            return true;
        }

        case KeyAction::DeleteNextChar:
        case KeyAction::DeleteNextWord: {
            if (readOnly) return true;
            const bool byWord = (action == KeyAction::DeleteNextWord);
            if (HasSelection()) {
                DeleteSelection();
            } else if (cursor < text.size()) {
                const keel::usize next = byWord
                    ? TextUtil::NextWordBoundary(text.as_ref(), cursor)
                    : TextUtil::NextCharBoundary(text.as_ref(), cursor);
                if (next > cursor) {
                    ReplaceRange(cursor, next - cursor, keel::StringRef{}, cursor, cursor, true);
                }
            }
            return true;
        }

        case KeyAction::MoveLeft:
        case KeyAction::SelectLeft:
        case KeyAction::MoveWordLeft:
        case KeyAction::SelectWordLeft: {
            const bool extendSel = (action == KeyAction::SelectLeft || action == KeyAction::SelectWordLeft);
            const bool byWord = (action == KeyAction::MoveWordLeft || action == KeyAction::SelectWordLeft);
            if (!extendSel && HasSelection()) {
                cursor = byWord
                    ? TextUtil::PrevWordBoundary(text.as_ref(), SelectionStart())
                    : SelectionStart();
                selectionAnchor = cursor;
            } else {
                cursor = byWord
                    ? TextUtil::PrevWordBoundary(text.as_ref(), cursor)
                    : TextUtil::PrevCharBoundary(text.as_ref(), cursor);
                if (!extendSel) {
                    selectionAnchor = cursor;
                }
            }
            return true;
        }

        case KeyAction::MoveRight:
        case KeyAction::SelectRight:
        case KeyAction::MoveWordRight:
        case KeyAction::SelectWordRight: {
            const bool extendSel = (action == KeyAction::SelectRight || action == KeyAction::SelectWordRight);
            const bool byWord = (action == KeyAction::MoveWordRight || action == KeyAction::SelectWordRight);
            if (!extendSel && HasSelection()) {
                cursor = byWord
                    ? TextUtil::NextWordBoundary(text.as_ref(), SelectionEnd())
                    : SelectionEnd();
                selectionAnchor = cursor;
            } else {
                cursor = byWord
                    ? TextUtil::NextWordBoundary(text.as_ref(), cursor)
                    : TextUtil::NextCharBoundary(text.as_ref(), cursor);
                if (!extendSel) {
                    selectionAnchor = cursor;
                }
            }
            return true;
        }

        case KeyAction::MoveUp:
            return moveLinesVertically(-1, false);
        case KeyAction::SelectUp:
            return moveLinesVertically(-1, true);
        case KeyAction::MoveDown:
            return moveLinesVertically(1, false);
        case KeyAction::SelectDown:
            return moveLinesVertically(1, true);
        case KeyAction::MovePageUp:
            return moveLinesVertically(-20, false);
        case KeyAction::SelectPageUp:
            return moveLinesVertically(-20, true);
        case KeyAction::MovePageDown:
            return moveLinesVertically(20, false);
        case KeyAction::SelectPageDown:
            return moveLinesVertically(20, true);

        case KeyAction::MoveLineStart:
        case KeyAction::SelectLineStart:
        case KeyAction::MoveDocStart:
        case KeyAction::SelectDocStart: {
            const bool extendSel = (action == KeyAction::SelectLineStart || action == KeyAction::SelectDocStart);
            const bool docStart = (action == KeyAction::MoveDocStart || action == KeyAction::SelectDocStart);
            if (multiline && !docStart) {
                const keel::usize safeCur = keel::min(cursor, text.size());
                const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', safeCur);
                const keel::usize lineStart = (prevNl == keel::StringRef::npos) ? 0 : prevNl + 1;
                keel::usize firstNonWs = lineStart;
                while (firstNonWs < text.size() && (text[firstNonWs] == ' ' || text[firstNonWs] == '\t')) {
                    ++firstNonWs;
                }
                if (safeCur == lineStart && firstNonWs > lineStart && autoCloseBrackets) {
                    cursor = firstNonWs;
                } else if (safeCur > firstNonWs && firstNonWs > lineStart && autoCloseBrackets) {
                    cursor = firstNonWs;
                } else {
                    cursor = lineStart;
                }
            } else {
                cursor = 0;
            }
            if (!extendSel) {
                selectionAnchor = cursor;
            }
            return true;
        }

        case KeyAction::MoveLineEnd:
        case KeyAction::SelectLineEnd:
        case KeyAction::MoveDocEnd:
        case KeyAction::SelectDocEnd: {
            const bool extendSel = (action == KeyAction::SelectLineEnd || action == KeyAction::SelectDocEnd);
            const bool docEnd = (action == KeyAction::MoveDocEnd || action == KeyAction::SelectDocEnd);
            if (multiline && !docEnd) {
                const keel::usize nextNl = text.find('\n', cursor);
                cursor = (nextNl == keel::StringRef::npos) ? text.size() : nextNl;
            } else {
                cursor = text.size();
            }
            if (!extendSel) {
                selectionAnchor = cursor;
            }
            return true;
        }

        case KeyAction::Indent:
        case KeyAction::Unindent: {
            if (multiline && !readOnly) {
                const keel::usize stepCols = keel::clamp<keel::usize>(tabSize, 1, 16);
                keel::FixedString<20> indentUnit;
                if (insertSpaces) {
                    for (keel::usize i = 0; i < stepCols; ++i) indentUnit.push_back(' ');
                } else {
                    indentUnit.push_back('\t');
                }
                const keel::usize unitLen = indentUnit.size();
                const bool unindent = (action == KeyAction::Unindent);
                const bool multiLineSel = HasSelection() && (SelectedText().find('\n') != keel::StringRef::npos);
                if (unindent || multiLineSel) {
                    const keel::usize selS = keel::min(SelectionStart(), text.size());
                    keel::usize selE = keel::min(SelectionEnd(), text.size());
                    if (selE > selS && text[selE - 1] == '\n') {
                        --selE;
                    }
                    const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', selS);
                    const keel::usize blockStart = (prevNl == keel::StringRef::npos) ? 0 : (prevNl + 1);
                    keel::usize blockEnd = text.find('\n', selE);
                    if (blockEnd == keel::StringRef::npos) blockEnd = text.size();

                    // Build modified block slice and commit as a single atomic O(block) undo op
                    keel::String block = keel::String::from(text.substr(blockStart, blockEnd - blockStart));
                    keel::usize newCur = cursor;
                    keel::usize newAnc = selectionAnchor;
                    keel::usize relPos = 0;
                    bool anyModified = false;
                    while (relPos <= block.size()) {
                        keel::usize relLineEnd = block.find('\n', relPos);
                        if (relLineEnd == keel::StringRef::npos) relLineEnd = block.size();
                        const keel::usize absPos = blockStart + relPos;

                        if (!unindent) {
                            StrInsert(block, relPos, indentUnit.as_ref());
                            if (newCur >= absPos) newCur += unitLen;
                            if (newAnc >= absPos) newAnc += unitLen;
                            relLineEnd += unitLen;
                            anyModified = true;
                        } else {
                            keel::usize rem = 0;
                            if (relPos < relLineEnd && block[relPos] == '\t') {
                                rem = 1;
                            } else {
                                while (rem < stepCols && (relPos + rem) < relLineEnd && block[relPos + rem] == ' ') {
                                    ++rem;
                                }
                            }
                            if (rem > 0) {
                                StrEraseRange(block, relPos, rem);
                                if (newCur > absPos) {
                                    newCur = (newCur >= absPos + rem) ? (newCur - rem) : absPos;
                                }
                                if (newAnc > absPos) {
                                    newAnc = (newAnc >= absPos + rem) ? (newAnc - rem) : absPos;
                                }
                                relLineEnd -= rem;
                                anyModified = true;
                            }
                        }

                        if (relLineEnd >= block.size()) {
                            break;
                        }
                        relPos = relLineEnd + 1;
                    }
                    if (anyModified) {
                        ReplaceRange(blockStart, blockEnd - blockStart, block.as_ref(), newCur, newAnc, true);
                    }
                    return true;
                }
                Insert(indentUnit.as_ref());
                return true;
            }
            return false;
        }

        case KeyAction::Confirm:
            if (multiline) {
                if (!readOnly) {
                    const keel::usize stepCols = keel::clamp<keel::usize>(tabSize, 1, 16);
                    keel::FixedString<20> indentUnit;
                    if (insertSpaces) {
                        for (keel::usize i = 0; i < stepCols; ++i) indentUnit.push_back(' ');
                    } else {
                        indentUnit.push_back('\t');
                    }
                    const keel::usize safeCur = keel::min(cursor, text.size());
                    const keel::usize prevNl = StrRfindBefore(text.as_ref(), '\n', safeCur);
                    const keel::usize curLineStart = (prevNl == keel::StringRef::npos) ? 0 : (prevNl + 1);
                    keel::String indent;
                    for (keel::usize i = curLineStart; i < safeCur && (text[i] == ' ' || text[i] == '\t'); ++i) {
                        indent.push_back(text[i]);
                    }
                    if (autoCloseBrackets && !HasSelection() && safeCur > 0 && safeCur < text.size()) {
                        const char prevC = text[safeCur - 1];
                        const char nextC = text[safeCur];
                        if ((prevC == '{' && nextC == '}') ||
                            (prevC == '[' && nextC == ']') ||
                            (prevC == '(' && nextC == ')')) {
                            keel::String mid = keel::String::with_capacity(1 + indent.size() + indentUnit.size());
                            mid.push_back('\n');
                            mid.append(indent.as_ref());
                            mid.append(indentUnit.as_ref());
                            keel::String full = keel::String::with_capacity(mid.size() + 1 + indent.size());
                            full.append(mid.as_ref());
                            full.push_back('\n');
                            full.append(indent.as_ref());
                            const keel::usize newCur = safeCur + mid.size();
                            ReplaceRange(safeCur, 0, full.as_ref(), newCur, newCur, true);
                            return true;
                        }
                    }
                    if (autoCloseBrackets && !HasSelection() && safeCur > curLineStart) {
                        keel::usize checkPos = safeCur;
                        while (checkPos > curLineStart && (text[checkPos - 1] == ' ' || text[checkPos - 1] == '\t')) {
                            --checkPos;
                        }
                        if (checkPos > curLineStart && (text[checkPos - 1] == '{' || text[checkPos - 1] == '[' || text[checkPos - 1] == '(')) {
                            indent.append(indentUnit.as_ref());
                        }
                    }
                    keel::String nlIndent = keel::String::with_capacity(1 + indent.size());
                    nlIndent.push_back('\n');
                    nlIndent.append(indent.as_ref());
                    Insert(nlIndent.as_ref());
                }
                return true;
            }
            submitted = true;
            outLostFocus = true;
            return true;

        case KeyAction::Cancel:
            if (HasSelection()) {
                selectionAnchor = cursor;
                return true;
            }
            ctx.FlushActiveEditHistory();
            outLostFocus = true;
            id = Id::Null();
            return true;

        default:
            return false;
    }
}

bool TextBuffer::TrimIdleMemory() noexcept {
    if (text.capacity() > 4096 || !undoOps.empty() || !redoOps.empty()) {
        text = {};
        undoOps = {};
        redoOps = {};
        return true;
    }
    return false;
}

} // namespace guikit
