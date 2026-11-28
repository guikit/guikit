#include "text_area_internal.hpp"

namespace guikit::detail::ta_impl {

namespace {

[[nodiscard]] keel::usize Utf8OffsetAtCodepointColumn(
    const TextDocument& doc,
    const TextLineRange& lineRange,
    keel::usize targetCodepointCol
) noexcept {
    if (targetCodepointCol == 0u || lineRange.contentBytes == 0u) {
        return lineRange.startByte;
    }
    if (doc.RangeIsPureAsciiNoTabs(lineRange.startByte, lineRange.contentBytes)) {
        return lineRange.startByte + keel::min(targetCodepointCol, lineRange.contentBytes);
    }
    // Stream through line chunks up to targetCodepointCol (never copies huge lines!).
    keel::usize remainingCols = targetCodepointCol;
    keel::usize byteOff = 0u;
    doc.ForEachChunkInRange(lineRange.startByte, lineRange.contentBytes, [&](keel::StringRef chunk) -> bool {
        keel::usize pos = 0u;
        while (pos < chunk.size() && remainingCols > 0u) {
            pos = keel::utf8_next_boundary(chunk, pos);
            --remainingCols;
        }
        byteOff += pos;
        return remainingCols > 0u;
    });
    return lineRange.startByte + keel::min(byteOff, lineRange.contentBytes);
}

[[nodiscard]] keel::usize CountCodepointsInLinePrefix(
    const TextDocument& doc,
    const TextLineRange& lineRange,
    keel::usize colBytes
) noexcept {
    const keel::usize clamped = keel::min(colBytes, lineRange.contentBytes);
    if (clamped == 0u) return 0u;
    if (doc.RangeIsPureAsciiNoTabs(lineRange.startByte, clamped)) {
        return clamped;
    }
    keel::usize count = 0u;
    doc.ForEachChunkInRange(lineRange.startByte, clamped, [&](keel::StringRef chunk) -> bool {
        count += keel::utf8_codepoint_count(chunk);
        return true;
    });
    return count;
}

/// Maps a logical line to its 0-based index among *visible* (non-folded) logical lines in O(log F).
[[nodiscard]] keel::usize LogicalLineToVisibleLogicalIndex(
    const TextViewProjection& proj,
    keel::usize logicalLine
) noexcept {
    if (proj.hiddenSpans.empty()) return logicalLine;
    keel::usize lo = 0u;
    keel::usize hi = proj.hiddenSpans.size();
    while (lo < hi) {
        const keel::usize mid = lo + (hi - lo) / 2u;
        if (proj.hiddenSpans[mid].hideStart <= logicalLine) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    if (lo == 0u) return logicalLine;
    const HiddenFoldSpan& sp = proj.hiddenSpans[lo - 1u];
    if (logicalLine <= sp.hideEnd) {
        return (sp.hideStart - 1u) - sp.hiddenBefore;
    }
    const keel::usize hiddenUpTo = sp.hiddenBefore + (sp.hideEnd - sp.hideStart + 1u);
    return logicalLine - hiddenUpTo;
}

/// Inverse of LogicalLineToVisibleLogicalIndex: maps a visible logical line index back to a logical line in O(log F).
[[nodiscard]] keel::usize VisibleLogicalIndexToLogicalLine(
    const TextViewProjection& proj,
    keel::usize visLogIdx,
    keel::usize totalLines
) noexcept {
    if (totalLines == 0u) return 0u;
    if (proj.hiddenSpans.empty()) return keel::min(visLogIdx, totalLines - 1u);
    keel::usize lo = 0u;
    keel::usize hi = proj.hiddenSpans.size();
    while (lo < hi) {
        const keel::usize mid = lo + (hi - lo) / 2u;
        const keel::usize visBefore = proj.hiddenSpans[mid].hideStart - proj.hiddenSpans[mid].hiddenBefore;
        if (visBefore <= visLogIdx) {
            lo = mid + 1u;
        } else {
            hi = mid;
        }
    }
    keel::usize logicalLine = visLogIdx;
    if (lo > 0u) {
        const HiddenFoldSpan& sp = proj.hiddenSpans[lo - 1u];
        const keel::usize hiddenUpTo = sp.hiddenBefore + (sp.hideEnd - sp.hideStart + 1u);
        logicalLine = visLogIdx + hiddenUpTo;
    }
    return keel::min(logicalLine, totalLines - 1u);
}

[[nodiscard]] keel::usize TotalVisibleLogicalLines(
    const TextViewProjection& proj,
    keel::usize totalLines
) noexcept {
    if (proj.hiddenSpans.empty()) return totalLines;
    const HiddenFoldSpan& last = proj.hiddenSpans.back();
    const keel::usize totalHidden = last.hiddenBefore + (last.hideEnd - last.hideStart + 1u);
    return (totalLines > totalHidden) ? (totalLines - totalHidden) : 1u;
}

} // namespace

// =============================================================================
// Direct O(log P) TextDocument Editing & Keyboard Command Router
// =============================================================================

bool HandleTextDocumentInput(
    Context& ctx,
    Id /*id*/,
    TextDocument& doc,
    TextAreaState& state,
    const TextAreaOptions& opts,
    const TextViewProjection& proj,
    bool& outLostFocus
) {
    outLostFocus = false;
    bool mutated = false;

    const bool readOnly = opts.readOnly;
    const bool autoClose = opts.autoCloseBrackets;
    const keel::u8 tabSize = keel::as<keel::u8>(keel::clamp<keel::i32>(opts.tabSize, 1, 16));
    const bool insertSpaces = opts.insertSpaces;

    auto hasSel = [&]() noexcept -> bool { return state.cursor != state.anchor; };
    auto selStart = [&]() noexcept -> keel::usize { return keel::min(state.cursor, state.anchor); };
    auto selEnd = [&]() noexcept -> keel::usize { return keel::max(state.cursor, state.anchor); };
    auto clampCaret = [&]() noexcept {
        const keel::usize n = doc.ByteLength();
        state.cursor = keel::min(state.cursor, n);
        state.anchor = keel::min(state.anchor, n);
        state.selectionStart = keel::min(state.cursor, state.anchor);
        state.selectionEnd = keel::max(state.cursor, state.anchor);
    };
    clampCaret();

    auto deleteSelection = [&]() -> bool {
        if (!hasSel() || readOnly) return false;
        state.preferredColumn.reset();
        const keel::usize s = selStart();
        const keel::usize e = selEnd();
        if (e > s) {
            doc.ReplaceRange(s, e - s, keel::StringRef{}, state.cursor, state.anchor, s, s, true, false);
            state.cursor = s;
            state.anchor = s;
            clampCaret();
            mutated = true;
            return true;
        }
        state.anchor = state.cursor;
        clampCaret();
        return false;
    };

    auto insertText = [&](keel::StringRef utf8Text, bool allowCoalesce) {
        if (readOnly || utf8Text.empty()) return;

        keel::String sanitized = keel::String::with_capacity(utf8Text.size());
        for (char c : utf8Text) {
            const keel::u8 u = keel::as<keel::u8>(c);
            if (c == '\r') continue;
            if (c == '\n') {
                sanitized.push_back('\n');
                continue;
            }
            if (u < 32u && c != '\t') continue;
            sanitized.push_back(c);
        }
        if (sanitized.empty() && !hasSel()) return;

        state.preferredColumn.reset();

        if (autoClose && sanitized.size() == 1u) {
            const char c = sanitized[0];
            const keel::usize safeCur = keel::min(state.cursor, doc.ByteLength());

            // Overtype closing bracket/quote if cursor sits right before it
            if (!hasSel() && safeCur < doc.ByteLength() &&
                (c == ')' || c == ']' || c == '}' || c == '"' || c == '\'') &&
                doc.ByteAt(safeCur) == c) {
                state.cursor = safeCur + 1u;
                state.anchor = state.cursor;
                clampCaret();
                doc.BreakUndoCoalesce();
                return;
            }

            char closeChar = 0;
            if (c == '(') closeChar = ')';
            else if (c == '[') closeChar = ']';
            else if (c == '{') closeChar = '}';
            else if (c == '"') closeChar = '"';
            else if (c == '\'') closeChar = '\'';

            if (closeChar != 0) {
                if (hasSel()) {
                    const keel::usize s = selStart();
                    const keel::usize e = selEnd();
                    TextDocument::EditGroupScope grp(doc);
                    const char closeBuf[1] = {closeChar};
                    const char openBuf[1] = {c};
                    doc.ReplaceRange(e, 0u, keel::StringRef(closeBuf, 1u), state.cursor, state.anchor, e + 1u, s + 1u, true, false);
                    doc.ReplaceRange(s, 0u, keel::StringRef(openBuf, 1u), state.cursor, state.anchor, e + 1u, s + 1u, true, false);
                    state.anchor = s + 1u;
                    state.cursor = e + 1u;
                    clampCaret();
                    mutated = true;
                    return;
                } else {
                    bool allowAuto = true;
                    if (c == '\'' && safeCur > 0u) {
                        const char prev = doc.ByteAt(safeCur - 1u);
                        if (IsIdentChar(prev) || prev == ')' || prev == ']' || prev == '}' || prev == '.') {
                            allowAuto = false;
                        }
                    }
                    if ((c == '"' || c == '\'') && safeCur > 0u) {
                        const char prev = doc.ByteAt(safeCur - 1u);
                        if (prev == '\\' || prev == c) allowAuto = false;
                    }
                    const bool nextOk = (safeCur == doc.ByteLength()) || [&]() {
                        const char nxt = doc.ByteAt(safeCur);
                        return nxt == '\n' || nxt == '\r' || nxt == ' ' || nxt == '\t' ||
                               nxt == ')' || nxt == ']' || nxt == '}' || nxt == ',' || nxt == ';';
                    }();
                    if (allowAuto && nextOk) {
                        const char pairStr[2] = {c, closeChar};
                        doc.ReplaceRange(safeCur, 0u, keel::StringRef(pairStr, 2u), safeCur, safeCur, safeCur + 1u, safeCur + 1u, true, false);
                        state.cursor = safeCur + 1u;
                        state.anchor = state.cursor;
                        clampCaret();
                        mutated = true;
                        return;
                    }
                }
            }
        }

        const keel::usize s = hasSel() ? selStart() : state.cursor;
        const keel::usize e = hasSel() ? selEnd() : s;
        const keel::usize newCur = s + sanitized.size();
        doc.ReplaceRange(s, e - s, sanitized.as_ref(), state.cursor, state.anchor, newCur, newCur, true, allowCoalesce && !hasSel());
        state.cursor = newCur;
        state.anchor = newCur;
        clampCaret();
        mutated = true;
    };

    // 1. Consume pending UTF-8 text input
    const auto typedList = ctx.ConsumePendingTextInput();
    for (const keel::String& txt : typedList) {
        insertText(txt.as_ref(), true);
    }

    // 2. Process queued key events in FIFO order (leaving unhandled keys untouched!)
    // Vertical cursor movement steps across *visible* logical lines in O(log F),
    // smoothly jumping over collapsed code folds instead of unintentionally unfolding them.
    auto moveLinesVertically = [&](keel::isize lineDelta, bool extendSel) -> bool {
        const TextLineCol curLc = doc.OffsetToLineCol(state.cursor);
        const TextLineRange curLr = doc.LineByteRange(curLc.line);
        if (!state.preferredColumn.has_value()) {
            state.preferredColumn = CountCodepointsInLinePrefix(doc, curLr, curLc.colBytes);
        }
        const keel::usize targetCol = *state.preferredColumn;
        const keel::usize totalLines = doc.LineCount();
        const keel::usize curVisLog = LogicalLineToVisibleLogicalIndex(proj, curLc.line);
        const keel::usize totalVisLog = TotalVisibleLogicalLines(proj, totalLines);

        if (lineDelta < 0) {
            const keel::usize steps = keel::as<keel::usize>(-lineDelta);
            if (curVisLog == 0u) {
                state.cursor = 0u;
                if (!extendSel) state.anchor = state.cursor;
                clampCaret();
                return true;
            }
            const keel::usize targetVisLog = (curVisLog > steps) ? (curVisLog - steps) : 0u;
            const keel::usize targetLine = VisibleLogicalIndexToLogicalLine(proj, targetVisLog, totalLines);
            const TextLineRange dstLr = doc.LineByteRange(targetLine);
            state.cursor = Utf8OffsetAtCodepointColumn(doc, dstLr, targetCol);
        } else if (lineDelta > 0) {
            const keel::usize steps = keel::as<keel::usize>(lineDelta);
            if (curVisLog + 1u >= totalVisLog) {
                state.cursor = doc.ByteLength();
                if (!extendSel) state.anchor = state.cursor;
                clampCaret();
                return true;
            }
            const keel::usize targetVisLog = keel::min(totalVisLog - 1u, curVisLog + steps);
            const keel::usize targetLine = VisibleLogicalIndexToLogicalLine(proj, targetVisLog, totalLines);
            const TextLineRange dstLr = doc.LineByteRange(targetLine);
            state.cursor = Utf8OffsetAtCodepointColumn(doc, dstLr, targetCol);
        }

        if (!extendSel) state.anchor = state.cursor;
        clampCaret();
        doc.BreakUndoCoalesce();
        return true;
    };

    ctx.ProcessPendingKeys([&](Key key, KeyModifiers mods) -> bool {
        const KeyAction action = ctx.ResolveAction(key, mods);
        const bool isVert =
            action == KeyAction::MoveUp || action == KeyAction::SelectUp ||
            action == KeyAction::MoveDown || action == KeyAction::SelectDown ||
            action == KeyAction::MovePageUp || action == KeyAction::SelectPageUp ||
            action == KeyAction::MovePageDown || action == KeyAction::SelectPageDown;
        if (!isVert) {
            state.preferredColumn.reset();
        }

        switch (action) {
            case KeyAction::SelectAll:
                state.anchor = 0u;
                state.cursor = doc.ByteLength();
                clampCaret();
                doc.BreakUndoCoalesce();
                return true;

            case KeyAction::Copy:
                if (hasSel()) {
                    keel::String selText;
                    doc.ReadSlice(selStart(), selEnd() - selStart(), selText);
                    ctx.SetClipboardText(selText.as_ref());
                }
                return true;

            case KeyAction::Cut:
                if (hasSel()) {
                    keel::String selText;
                    doc.ReadSlice(selStart(), selEnd() - selStart(), selText);
                    ctx.SetClipboardText(selText.as_ref());
                    if (!readOnly) {
                        deleteSelection();
                    }
                }
                return true;

            case KeyAction::Paste:
                if (!readOnly) {
                    const keel::String clip = ctx.GetClipboardText();
                    if (!clip.empty()) {
                        insertText(clip.as_ref(), false);
                    }
                }
                return true;

            case KeyAction::Undo:
                if (!readOnly) {
                    const TextUndoResult u = doc.Undo();
                    if (u.changed) {
                        state.cursor = u.cursor;
                        state.anchor = u.selectionAnchor;
                        clampCaret();
                        mutated = true;
                    }
                }
                return true;

            case KeyAction::Redo:
                if (!readOnly) {
                    const TextUndoResult r = doc.Redo();
                    if (r.changed) {
                        state.cursor = r.cursor;
                        state.anchor = r.selectionAnchor;
                        clampCaret();
                        mutated = true;
                    }
                }
                return true;

            case KeyAction::MoveLineUp:
            case KeyAction::MoveLineDown: {
                if (readOnly) return false;
                const TextLineCol lc = doc.OffsetToLineCol(state.cursor);
                const TextLineRange curR = doc.LineByteRange(lc.line);
                if (action == KeyAction::MoveLineUp && lc.line > 0u) {
                    const TextLineRange prevR = doc.LineByteRange(lc.line - 1u);
                    keel::String curLine;
                    keel::String prevLine;
                    doc.ReadSlice(curR.startByte, curR.contentBytes, curLine);
                    doc.ReadSlice(prevR.startByte, prevR.contentBytes, prevLine);
                    keel::String swapped = keel::String::with_capacity(curLine.size() + 1u + prevLine.size());
                    swapped.append(curLine.as_ref());
                    swapped.push_back('\n');
                    swapped.append(prevLine.as_ref());
                    const keel::usize eraseLen = (curR.startByte + curR.contentBytes) - prevR.startByte;
                    const keel::usize newCur = prevR.startByte + keel::min(lc.colBytes, curLine.size());
                    doc.ReplaceRange(prevR.startByte, eraseLen, swapped.as_ref(), state.cursor, state.anchor, newCur, newCur, true, false);
                    state.cursor = newCur;
                    state.anchor = newCur;
                    clampCaret();
                    mutated = true;
                } else if (action == KeyAction::MoveLineDown && lc.line + 1u < doc.LineCount()) {
                    const TextLineRange nextR = doc.LineByteRange(lc.line + 1u);
                    keel::String curLine;
                    keel::String nextLine;
                    doc.ReadSlice(curR.startByte, curR.contentBytes, curLine);
                    doc.ReadSlice(nextR.startByte, nextR.contentBytes, nextLine);
                    keel::String swapped = keel::String::with_capacity(nextLine.size() + 1u + curLine.size());
                    swapped.append(nextLine.as_ref());
                    swapped.push_back('\n');
                    swapped.append(curLine.as_ref());
                    const keel::usize eraseLen = (nextR.startByte + nextR.contentBytes) - curR.startByte;
                    const keel::usize newCur = curR.startByte + nextLine.size() + 1u + keel::min(lc.colBytes, curLine.size());
                    doc.ReplaceRange(curR.startByte, eraseLen, swapped.as_ref(), state.cursor, state.anchor, newCur, newCur, true, false);
                    state.cursor = newCur;
                    state.anchor = newCur;
                    clampCaret();
                    mutated = true;
                }
                return true;
            }

            case KeyAction::DuplicateLine: {
                if (readOnly) return false;
                const TextLineCol lc = doc.OffsetToLineCol(state.cursor);
                const TextLineRange curR = doc.LineByteRange(lc.line);
                keel::String lineStr;
                doc.ReadSlice(curR.startByte, curR.contentBytes, lineStr);
                keel::String dup = keel::String::with_capacity(1u + lineStr.size());
                dup.push_back('\n');
                dup.append(lineStr.as_ref());
                const keel::usize insAt = curR.startByte + curR.contentBytes;
                const keel::usize newCur = insAt + 1u + keel::min(lc.colBytes, lineStr.size());
                doc.ReplaceRange(insAt, 0u, dup.as_ref(), state.cursor, state.anchor, newCur, newCur, true, false);
                state.cursor = newCur;
                state.anchor = newCur;
                clampCaret();
                mutated = true;
                return true;
            }

            case KeyAction::DeleteLine: {
                if (readOnly) return false;
                const TextLineCol lc = doc.OffsetToLineCol(state.cursor);
                const TextLineRange curR = doc.LineByteRange(lc.line);
                if (lc.line + 1u < doc.LineCount()) {
                    doc.ReplaceRange(curR.startByte, curR.totalBytes, keel::StringRef{}, state.cursor, state.anchor, curR.startByte, curR.startByte, true, false);
                    state.cursor = curR.startByte;
                    state.anchor = curR.startByte;
                    clampCaret();
                    mutated = true;
                } else if (curR.startByte > 0u) {
                    const keel::usize prevBreakStart = (curR.startByte >= 2u && doc.ByteAt(curR.startByte - 2u) == '\r')
                        ? (curR.startByte - 2u)
                        : (curR.startByte - 1u);
                    const keel::usize eraseLen = doc.ByteLength() - prevBreakStart;
                    doc.ReplaceRange(prevBreakStart, eraseLen, keel::StringRef{}, state.cursor, state.anchor, prevBreakStart, prevBreakStart, true, false);
                    state.cursor = prevBreakStart;
                    state.anchor = prevBreakStart;
                    clampCaret();
                    mutated = true;
                } else if (!doc.Empty()) {
                    doc.ReplaceRange(0u, doc.ByteLength(), keel::StringRef{}, state.cursor, state.anchor, 0u, 0u, true, false);
                    state.cursor = 0u;
                    state.anchor = 0u;
                    clampCaret();
                    mutated = true;
                }
                return true;
            }

            case KeyAction::DeletePrevChar:
            case KeyAction::DeletePrevWord: {
                if (readOnly) return true;
                const bool byWord = (action == KeyAction::DeletePrevWord);
                if (hasSel()) {
                    deleteSelection();
                } else if (state.cursor > 0u && !doc.Empty()) {
                    const keel::usize safeCur = state.cursor;
                    if (autoClose && !byWord && safeCur < doc.ByteLength()) {
                        const char prevC = doc.ByteAt(safeCur - 1u);
                        const char nextC = doc.ByteAt(safeCur);
                        if ((prevC == '(' && nextC == ')') ||
                            (prevC == '[' && nextC == ']') ||
                            (prevC == '{' && nextC == '}') ||
                            (prevC == '"' && nextC == '"') ||
                            (prevC == '\'' && nextC == '\'')) {
                            doc.ReplaceRange(safeCur - 1u, 2u, keel::StringRef{}, safeCur, safeCur, safeCur - 1u, safeCur - 1u, true, false);
                            state.cursor = safeCur - 1u;
                            state.anchor = state.cursor;
                            clampCaret();
                            mutated = true;
                            return true;
                        }
                    }
                    const keel::usize prev = byWord ? doc.PrevWordOffset(safeCur) : doc.PrevCharOffset(safeCur);
                    if (safeCur > prev) {
                        doc.ReplaceRange(prev, safeCur - prev, keel::StringRef{}, safeCur, safeCur, prev, prev, true, false);
                        state.cursor = prev;
                        state.anchor = prev;
                        clampCaret();
                        mutated = true;
                    }
                }
                return true;
            }

            case KeyAction::DeleteNextChar:
            case KeyAction::DeleteNextWord: {
                if (readOnly) return true;
                const bool byWord = (action == KeyAction::DeleteNextWord);
                if (hasSel()) {
                    deleteSelection();
                } else if (state.cursor < doc.ByteLength()) {
                    const keel::usize next = byWord ? doc.NextWordOffset(state.cursor) : doc.NextCharOffset(state.cursor);
                    if (next > state.cursor) {
                        doc.ReplaceRange(state.cursor, next - state.cursor, keel::StringRef{}, state.cursor, state.cursor, state.cursor, state.cursor, true, false);
                        clampCaret();
                        mutated = true;
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
                if (!extendSel && hasSel()) {
                    state.cursor = byWord ? doc.PrevWordOffset(selStart()) : selStart();
                    state.anchor = state.cursor;
                } else {
                    state.cursor = byWord ? doc.PrevWordOffset(state.cursor) : doc.PrevCharOffset(state.cursor);
                    if (!extendSel) state.anchor = state.cursor;
                }
                clampCaret();
                doc.BreakUndoCoalesce();
                return true;
            }

            case KeyAction::MoveRight:
            case KeyAction::SelectRight:
            case KeyAction::MoveWordRight:
            case KeyAction::SelectWordRight: {
                const bool extendSel = (action == KeyAction::SelectRight || action == KeyAction::SelectWordRight);
                const bool byWord = (action == KeyAction::MoveWordRight || action == KeyAction::SelectWordRight);
                if (!extendSel && hasSel()) {
                    state.cursor = byWord ? doc.NextWordOffset(selEnd()) : selEnd();
                    state.anchor = state.cursor;
                } else {
                    state.cursor = byWord ? doc.NextWordOffset(state.cursor) : doc.NextCharOffset(state.cursor);
                    if (!extendSel) state.anchor = state.cursor;
                }
                clampCaret();
                doc.BreakUndoCoalesce();
                return true;
            }

            case KeyAction::MoveUp: return moveLinesVertically(-1, false);
            case KeyAction::SelectUp: return moveLinesVertically(-1, true);
            case KeyAction::MoveDown: return moveLinesVertically(1, false);
            case KeyAction::SelectDown: return moveLinesVertically(1, true);
            case KeyAction::MovePageUp: return moveLinesVertically(-20, false);
            case KeyAction::SelectPageUp: return moveLinesVertically(-20, true);
            case KeyAction::MovePageDown: return moveLinesVertically(20, false);
            case KeyAction::SelectPageDown: return moveLinesVertically(20, true);

            case KeyAction::MoveLineStart:
            case KeyAction::SelectLineStart:
            case KeyAction::MoveDocStart:
            case KeyAction::SelectDocStart: {
                const bool extendSel = (action == KeyAction::SelectLineStart || action == KeyAction::SelectDocStart);
                const bool docStart = (action == KeyAction::MoveDocStart || action == KeyAction::SelectDocStart);
                if (!docStart) {
                    const TextLineCol lc = doc.OffsetToLineCol(state.cursor);
                    const TextLineRange lr = doc.LineByteRange(lc.line);
                    keel::String leadScratch;
                    doc.ReadSlice(lr.startByte, keel::min<keel::usize>(lr.contentBytes, 256u), leadScratch);
                    keel::usize indentLen = 0u;
                    while (indentLen < leadScratch.size() && (leadScratch[indentLen] == ' ' || leadScratch[indentLen] == '\t')) {
                        ++indentLen;
                    }
                    const keel::usize firstNonWs = lr.startByte + indentLen;
                    if (autoClose && indentLen > 0u && (state.cursor == lr.startByte || state.cursor > firstNonWs)) {
                        state.cursor = firstNonWs;
                    } else {
                        state.cursor = lr.startByte;
                    }
                } else {
                    state.cursor = 0u;
                }
                if (!extendSel) state.anchor = state.cursor;
                clampCaret();
                doc.BreakUndoCoalesce();
                return true;
            }

            case KeyAction::MoveLineEnd:
            case KeyAction::SelectLineEnd:
            case KeyAction::MoveDocEnd:
            case KeyAction::SelectDocEnd: {
                const bool extendSel = (action == KeyAction::SelectLineEnd || action == KeyAction::SelectDocEnd);
                const bool docEnd = (action == KeyAction::MoveDocEnd || action == KeyAction::SelectDocEnd);
                if (!docEnd) {
                    const TextLineCol lc = doc.OffsetToLineCol(state.cursor);
                    const TextLineRange lr = doc.LineByteRange(lc.line);
                    state.cursor = lr.startByte + lr.contentBytes;
                } else {
                    state.cursor = doc.ByteLength();
                }
                if (!extendSel) state.anchor = state.cursor;
                clampCaret();
                doc.BreakUndoCoalesce();
                return true;
            }

            case KeyAction::Indent:
            case KeyAction::Unindent: {
                if (readOnly) return false;
                keel::FixedString<20> indentUnit;
                if (insertSpaces) {
                    for (keel::u8 i = 0u; i < tabSize; ++i) indentUnit.push_back(' ');
                } else {
                    indentUnit.push_back('\t');
                }
                const bool unindent = (action == KeyAction::Unindent);
                const TextLineCol sLc = doc.OffsetToLineCol(selStart());
                keel::usize eOff = selEnd();
                if (eOff > selStart() && eOff > 0u && doc.ByteAt(eOff - 1u) == '\n') {
                    --eOff;
                }
                const TextLineCol eLc = doc.OffsetToLineCol(eOff);
                const bool multiLineSel = hasSel() && (eLc.line > sLc.line);

                if (unindent || multiLineSel) {
                    const keel::usize origCur = state.cursor;
                    const keel::usize origAnc = state.anchor;
                    keel::usize newCur = origCur;
                    keel::usize newAnc = origAnc;

                    TextDocument::EditGroupScope grp(doc);
                    // Apply from bottom line upward so earlier line byte offsets never shift!
                    for (keel::usize li = eLc.line + 1u; li > sLc.line; --li) {
                        const keel::usize lineIdx = li - 1u;
                        const TextLineRange lr = doc.LineByteRange(lineIdx);
                        if (!unindent) {
                            doc.ReplaceRange(lr.startByte, 0u, indentUnit.as_ref(), origCur, origAnc, newCur, newAnc, true, false);
                            if (newCur >= lr.startByte) newCur += indentUnit.size();
                            if (newAnc >= lr.startByte) newAnc += indentUnit.size();
                            mutated = true;
                        } else if (lr.contentBytes > 0u) {
                            keel::String head;
                            doc.ReadSlice(lr.startByte, keel::min<keel::usize>(lr.contentBytes, tabSize), head);
                            keel::usize rem = 0u;
                            if (!head.empty() && head[0] == '\t') {
                                rem = 1u;
                            } else {
                                while (rem < head.size() && head[rem] == ' ') ++rem;
                            }
                            if (rem > 0u) {
                                doc.ReplaceRange(lr.startByte, rem, keel::StringRef{}, origCur, origAnc, newCur, newAnc, true, false);
                                if (newCur > lr.startByte) newCur = (newCur >= lr.startByte + rem) ? (newCur - rem) : lr.startByte;
                                if (newAnc > lr.startByte) newAnc = (newAnc >= lr.startByte + rem) ? (newAnc - rem) : lr.startByte;
                                mutated = true;
                            }
                        }
                    }
                    state.cursor = newCur;
                    state.anchor = newAnc;
                    clampCaret();
                    return true;
                }

                insertText(indentUnit.as_ref(), false);
                return true;
            }

            case KeyAction::Confirm: {
                if (readOnly) return true;
                keel::FixedString<20> indentUnit;
                if (insertSpaces) {
                    for (keel::u8 i = 0u; i < tabSize; ++i) indentUnit.push_back(' ');
                } else {
                    indentUnit.push_back('\t');
                }
                const keel::usize safeCur = state.cursor;
                const TextLineCol lc = doc.OffsetToLineCol(safeCur);
                const TextLineRange lr = doc.LineByteRange(lc.line);
                keel::String leadBuf;
                doc.ReadSlice(lr.startByte, keel::min<keel::usize>(lc.colBytes, 256u), leadBuf);
                keel::String indent;
                for (char ch : leadBuf) {
                    if (ch == ' ' || ch == '\t') indent.push_back(ch);
                    else break;
                }

                if (autoClose && !hasSel() && safeCur > 0u && safeCur < doc.ByteLength()) {
                    const char prevC = doc.ByteAt(safeCur - 1u);
                    const char nextC = doc.ByteAt(safeCur);
                    if ((prevC == '{' && nextC == '}') ||
                        (prevC == '[' && nextC == ']') ||
                        (prevC == '(' && nextC == ')')) {
                        keel::String mid = keel::String::with_capacity(1u + indent.size() + indentUnit.size());
                        mid.push_back('\n');
                        mid.append(indent.as_ref());
                        mid.append(indentUnit.as_ref());
                        keel::String full = keel::String::with_capacity(mid.size() + 1u + indent.size());
                        full.append(mid.as_ref());
                        full.push_back('\n');
                        full.append(indent.as_ref());
                        const keel::usize newCur = safeCur + mid.size();
                        doc.ReplaceRange(safeCur, 0u, full.as_ref(), safeCur, safeCur, newCur, newCur, true, false);
                        state.cursor = newCur;
                        state.anchor = newCur;
                        clampCaret();
                        mutated = true;
                        return true;
                    }
                }
                if (autoClose && !hasSel() && safeCur > lr.startByte) {
                    keel::usize checkPos = safeCur;
                    keel::usize backLimit = keel::min<keel::usize>(safeCur - lr.startByte, 64u);
                    while (backLimit > 0u) {
                        const char ch = doc.ByteAt(checkPos - 1u);
                        if (ch == ' ' || ch == '\t') {
                            --checkPos;
                            --backLimit;
                        } else {
                            break;
                        }
                    }
                    if (checkPos > lr.startByte) {
                        const char openC = doc.ByteAt(checkPos - 1u);
                        if (openC == '{' || openC == '[' || openC == '(') {
                            indent.append(indentUnit.as_ref());
                        }
                    }
                }
                keel::String nlIndent = keel::String::with_capacity(1u + indent.size());
                nlIndent.push_back('\n');
                nlIndent.append(indent.as_ref());
                insertText(nlIndent.as_ref(), false);
                return true;
            }

            case KeyAction::Cancel:
                if (hasSel()) {
                    state.anchor = state.cursor;
                    clampCaret();
                    return true;
                }
                outLostFocus = true;
                return true;

            default:
                return false;
        }
    });

    return mutated;
}

} // namespace guikit::detail::ta_impl
