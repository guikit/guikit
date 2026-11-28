#include "text_area_internal.hpp"

namespace guikit {

namespace detail::ta_impl {

keel::StringRef ExtractCodeWithoutLineComment(
    keel::StringRef lineText,
    keel::StringRef commentPrefix
) noexcept {
    keel::usize start = 0u;
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
    for (keel::usize i = 0u; i < sub.size(); ++i) {
        const char c = sub[i];
        if (inQuotes) {
            if (c == '\\' && i + 1u < sub.size()) {
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

    keel::StringRef code = sub.substr(0u, codeLen);
    while (!code.empty() && (code.back() == ' ' || code.back() == '\t' || code.back() == '\r')) {
        code.remove_suffix(1u);
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

namespace {

keel::StringRef ExtractLeadingIdentifierOrKey(keel::StringRef code) noexcept {
    keel::usize i = 0u;
    while (i < code.size() && (code[i] == ' ' || code[i] == '\t' || code[i] == '(' || code[i] == '[' || code[i] == '*' || code[i] == '&')) {
        ++i;
    }
    if (i >= code.size()) return {};
    if (code[i] == '"') {
        const keel::usize start = i++;
        while (i < code.size() && code[i] != '"') {
            if (code[i] == '\\' && i + 1u < code.size()) ++i;
            ++i;
        }
        if (i < code.size()) ++i;
        return code.substr(start, i - start);
    }
    if (IsIdentStart(code[i])) {
        const keel::usize start = i++;
        while (i < code.size() && IsIdentChar(code[i])) ++i;
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
    for (keel::usize i = 0u; i < code.size(); ++i) {
        const char c = code[i];
        if (c == '(' || c == '[') ++parenDepth;
        else if (c == ')' || c == ']') { if (parenDepth > 0) --parenDepth; }
        else if (c == '=' && parenDepth == 0) {
            const char prev = (i > 0u) ? code[i - 1u] : '\0';
            const char next = (i + 1u < code.size()) ? code[i + 1u] : '\0';
            if (prev != '=' && prev != '!' && prev != '<' && prev != '>' && next != '=') {
                eqPos = i;
                break;
            }
        }
    }
    if (eqPos == keel::StringRef::npos || eqPos == 0u) return {};

    keel::usize p = eqPos;
    if (p > 0u && (code[p - 1u] == '+' || code[p - 1u] == '-' || code[p - 1u] == '*' ||
                   code[p - 1u] == '/' || code[p - 1u] == '|' || code[p - 1u] == '&' || code[p - 1u] == '^')) {
        --p;
    }
    while (p > 0u && (code[p - 1u] == ' ' || code[p - 1u] == '\t')) --p;
    if (p > 0u && (code[p - 1u] == ')' || code[p - 1u] == ']')) {
        const char closeBr = code[p - 1u];
        const char openBr = (closeBr == ')') ? '(' : '[';
        keel::i32 depth = 0;
        while (p > 0u) {
            --p;
            if (code[p] == closeBr) ++depth;
            else if (code[p] == openBr) {
                --depth;
                if (depth == 0) break;
            }
        }
        while (p > 0u && (code[p - 1u] == ' ' || code[p - 1u] == '\t')) --p;
    }
    const keel::usize endIdent = p;
    while (p > 0u && IsIdentChar(code[p - 1u])) --p;
    if (endIdent > p && IsIdentStart(code[p])) {
        return code.substr(p, endIdent - p);
    }
    return {};
}

bool DoesLineMatchBaselineSignature(keel::StringRef currentCode, keel::StringRef baselineCode) noexcept {
    if (currentCode.empty()) return false;
    if (baselineCode.empty()) return true;
    if (currentCode == baselineCode) return true;
    const keel::StringRef baseTarget = ExtractAssignedTargetIdentifier(baselineCode);
    if (!baseTarget.empty()) {
        return ExtractAssignedTargetIdentifier(currentCode) == baseTarget;
    }
    const keel::StringRef baseLead = ExtractLeadingIdentifierOrKey(baselineCode);
    const keel::StringRef curLead = ExtractLeadingIdentifierOrKey(currentCode);
    return !baseLead.empty() && (curLead == baseLead);
}

} // namespace

keel::usize ResolveTrackedCodeLine(
    keel::StringRef docText,
    keel::ArrayRef<LogicalLine> logicalLines,
    keel::usize requestedLine1,
    keel::StringRef explicitAnchor,
    keel::ArrayRef<keel::usize> baselineToCurrentLine,
    keel::ArrayRef<keel::String> baselineSignatures,
    keel::StringRef commentPrefix
) noexcept {
    if (logicalLines.empty() || requestedLine1 == 0u) return 0u;
    auto getLineCode = [&](keel::usize line1) noexcept -> keel::StringRef {
        if (line1 < 1u || line1 > logicalLines.size()) return {};
        const LogicalLine& ll = logicalLines[line1 - 1u];
        return ExtractCodeWithoutLineComment(docText.substr(ll.startOffset, ll.length), commentPrefix);
    };

    const keel::usize mappedLine1 = (requestedLine1 < baselineToCurrentLine.size() && baselineToCurrentLine[requestedLine1] > 0u)
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
        const keel::usize maxSearch = keel::min<keel::usize>(logicalLines.size(), 4096u);
        for (keel::usize delta = 1u; delta <= maxSearch; ++delta) {
            if (mappedLine1 + delta <= logicalLines.size()) {
                const keel::StringRef c = getLineCode(mappedLine1 + delta);
                if (!c.empty() && c.find(explicitAnchor) != keel::StringRef::npos) return mappedLine1 + delta;
            }
            if (mappedLine1 > delta) {
                const keel::StringRef c = getLineCode(mappedLine1 - delta);
                if (!c.empty() && c.find(explicitAnchor) != keel::StringRef::npos) return mappedLine1 - delta;
            }
        }
        return 0u;
    }

    const keel::StringRef mappedCode = getLineCode(mappedLine1);
    if (DoesLineMatchBaselineSignature(mappedCode, baseSig)) {
        return mappedLine1;
    }
    if (!baseSig.empty()) {
        for (keel::usize delta = 1u; delta <= 2u; ++delta) {
            if (mappedLine1 + delta <= logicalLines.size() && getLineCode(mappedLine1 + delta) == baseSig) return mappedLine1 + delta;
            if (mappedLine1 > delta && getLineCode(mappedLine1 - delta) == baseSig) return mappedLine1 - delta;
        }
    }
    return 0u;
}

} // namespace detail::ta_impl

using namespace detail::ta_impl;

namespace {

TextAreaResponse RunTextAreaCore(
    Context& ctx,
    TextDocument* mutDoc,
    const TextDocument* constDoc,
    keel::Optional<keel::StringSink> stringValue,
    keel::StringRef displayFallback,
    const TextAreaOptions& opts,
    keel::SourceLocation loc
) {
    const TextDocument* docPtr = mutDoc ? mutDoc : constDoc;
    const bool isDocMode = (docPtr != nullptr);

    const Id id = ctx.MakeChildId(loc);
    const UiSize activeSize = opts.size;
    const bool effectiveDisabled = opts.disabled || ctx.IsDisabled();
    const bool effectiveReadOnly = opts.readOnly || (isDocMode && mutDoc == nullptr);
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
    optsCopy.readOnly = effectiveReadOnly;
    if (optsCopy.width.IsAuto()) {
        optsCopy.width = Control::ResolveWidthMode(ctx, opts.width, SizeMode::Stretch);
    }
    optsCopy.placeholder = ctx.AllocText(opts.placeholder);
    optsCopy.tooltip = ctx.AllocText(opts.tooltip);
    if (optsCopy.syntax.has_value()) {
        SyntaxConfig& syn = *optsCopy.syntax;
        syn.languageName = ctx.AllocString(syn.languageName);
        syn.lineComment = ctx.AllocString(syn.lineComment);
        syn.lineCommentKeyword = ctx.AllocString(syn.lineCommentKeyword);
        syn.sectionPrefix = ctx.AllocString(syn.sectionPrefix);
        syn.blockCommentStart = ctx.AllocString(syn.blockCommentStart);
        syn.blockCommentEnd = ctx.AllocString(syn.blockCommentEnd);
        auto copyStrArray = [&](keel::ArrayRef<keel::StringRef> src) -> keel::ArrayRef<keel::StringRef> {
            if (src.empty()) return {};
            // Built-in preset tables live in static constexpr storage and are pre-sorted for O(log K) binary search.
            if (IsBuiltInSortedTokenList(src)) return src;
            keel::MutArrayRef<keel::StringRef> dst = ctx.GetArena().create_slice<keel::StringRef>(src.size());
            for (keel::usize i = 0u; i < src.size(); ++i) {
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
        for (keel::usize i = 0u; i < opts.debugVariables.size(); ++i) {
            vars[i].name = ctx.AllocString(opts.debugVariables[i].name);
            vars[i].value = ctx.AllocString(opts.debugVariables[i].value);
            vars[i].type = ctx.AllocString(opts.debugVariables[i].type);
        }
        optsCopy.debugVariables = vars;
    }

    const keel::StringRef lineCmtPrefix = (opts.syntax.has_value() && !opts.syntax->lineComment.empty())
        ? opts.syntax->lineComment
        : "//";

    auto& state = ctx.GetOrCreatePersistentState<TextAreaState>(id);
    if (!isFocused) {
        state.overlayFocus = 0;
        ctx.SetSuspendDirectEditInput(false);
    }

    // Active breakpoints sink (external or internal)
    IndexListSink activeBreakpoints = opts.breakpoints.has_target()
        ? opts.breakpoints
        : IndexListSink(state.internalBreakpoints);

    const bool hasAnyCodeMarkers =
        !opts.diagnostics.empty() || !opts.inlineValues.empty() ||
        opts.breakpoints.has_target() || opts.enableBreakpoints ||
        opts.executionLine > 0u || (opts.executionLinePtr != nullptr && *opts.executionLinePtr > 0u);

    const keel::StringRef callerText = (!isDocMode)
        ? (stringValue.has_value() ? stringValue->as_ref() : displayFallback)
        : keel::StringRef{};
    keel::StringRef valStr = callerText;

    // --- State Sync: TextDocument mode (zero-copy O(deltas)) vs Legacy StringRef mode ---
    bool docMutatedByExternal = false;
    if (isDocMode) {
        if (state.lastDocPtr != docPtr) {
            state.lastDocPtr = docPtr;
            state.lastDocRevision = docPtr->Revision();
            state.cursor = keel::min(state.cursor, docPtr->ByteLength());
            state.anchor = state.cursor;
            state.selectionStart = state.cursor;
            state.selectionEnd = state.cursor;
            state.prevCursor = state.cursor;
            state.prevTextSize = docPtr->ByteLength();
            state.prevLineCount = docPtr->LineCount();
            state.cachedFoldEnabled = false;
            state.cachedFoldWindowBucket = keel::as<keel::usize>(-1);
            state.preservedFoldedRegions.clear();
            state.foldedLines.clear();
            state.cachedExactWrapKey = 0u;
            state.cachedAnalyticWrapKey = 0u;
            state.cachedMaxLineWKey = 0u;
            state.cachedSearchKey = 0u;
            state.cachedLexerConfigKey = 0u;
            state.cachedLexerDocKey = 0u;
            state.lexerCheckpoints.clear();
            state.lexerValidCheckpoints = 0u;
        }
    } else {
        const bool trackLegacyBaseline = hasAnyCodeMarkers || (callerText.size() <= 65536u);
        if (trackLegacyBaseline && (!state.baselineInitialized || callerText != state.prevDocumentText)) {
            if (state.baselineInitialized && ctx.HasActiveTextEdit(id)) {
                ctx.SetTextEditBuffer(id, callerText);
            }
            keel::Array<LogicalLine> initLines;
            SplitLogicalLines(callerText, initLines);
            state.baselineToCurrentLine.assign(initLines.size() + 1u, 0u);
            state.baselineSignatures.clear();
            state.baselineSignatures.resize(initLines.size() + 1u);
            for (keel::usize i = 0u; i < initLines.size(); ++i) {
                state.baselineToCurrentLine[i + 1u] = i + 1u;
                const keel::StringRef rawLine = callerText.substr(initLines[i].startOffset, initLines[i].length);
                state.baselineSignatures[i + 1u].assign(ExtractCodeWithoutLineComment(rawLine, lineCmtPrefix));
            }
            state.prevDocumentText.assign(callerText);
            state.prevCursor = keel::min(state.cursor, callerText.size());
            state.prevTextSize = callerText.size();
            state.prevLineCount = initLines.size();
            state.baselineInitialized = true;
        }
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
            const keel::f64 ratio = keel::as<keel::f64>(clampedNew) / keel::as<keel::f64>(state.prevAppliedZoom);
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

    // Synchronize active edit backend
    if (isDocMode) {
        if (isFocused) {
            ctx.RegisterCustomTextEdit(id, /*multiline=*/true, effectiveReadOnly);
        }
    } else if (isFocused || ctx.HasActiveTextEdit(id)) {
        valStr = ctx.EnsureTextEdit(id, valStr, /*multiline=*/true, effectiveReadOnly);
        ctx.SetTextEditAutoClose(id, opts.autoCloseBrackets);
        ctx.SetTextEditIndentation(id, opts.tabSize, opts.insertSpaces);
    }

    keel::Array<LogicalLine>& logicalLines = state.cachedLogicalLines;
    keel::Array<FoldRegion>& foldRegions = state.cachedFoldRegions;

    keel::u64 docCacheKey = 0u;
    bool docChangedForCache = false;
    if (isDocMode) {
        docCacheKey = detail::HashArgs(reinterpret_cast<keel::usize>(docPtr), docPtr->Revision());
        docChangedForCache = (docPtr->Revision() != state.lastDocRevision);
        docMutatedByExternal = docChangedForCache;
    } else {
        const keel::u64 docHash = detail::HashArgs(valStr);
        docCacheKey = detail::HashArgs(docHash, valStr.size());
        docChangedForCache = (logicalLines.empty() || state.cachedDocSize != valStr.size() || state.cachedDocHash != docHash);
        if (docChangedForCache) {
            SplitLogicalLines(valStr, logicalLines, &state.cachedMaxLineBytes, &state.cachedPureAsciiNoTabs);
            state.cachedDocHash = docHash;
            state.cachedDocSize = valStr.size();
        }
    }

    auto makeSourceView = [&]() noexcept -> TextSourceView {
        if (isDocMode) {
            return TextSourceView::FromDocument(*docPtr);
        }
        return TextSourceView::FromFlat(valStr, logicalLines, state.cachedMaxLineBytes, state.cachedPureAsciiNoTabs);
    };

    TextSourceView source = makeSourceView();

    const Rect refBounds = (res.rect.w > 0.0f && res.rect.h > 0.0f)
        ? res.rect
        : Rect({0.0f, 0.0f}, MeasureEditorBoxFromStats(
            source.LineCount(), source.MaxLineByteLength(), optsCopy, activeSize, canvas, theme, state.zoom));

    // Apply incremental line shifts from TextDocument deltas (O(deltas * markers), zero full-doc diff!)
    auto syncDocumentDeltas = [&]() {
        if (!isDocMode || docPtr->Revision() == state.lastDocRevision) return;
        keel::Array<TextEditDelta> deltas;
        const bool contiguous = docPtr->GetDeltasSince(state.lastDocRevision, deltas);
        const keel::usize newLineCount = docPtr->LineCount();
        if (contiguous && !deltas.empty()) {
            keel::isize netDiff = 0;
            keel::usize firstEditLine1 = 0u;
            for (const TextEditDelta& d : deltas) {
                InvalidateLexerCheckpointsFromLine(state, d.startLine);
                if (d.lineDelta != 0) {
                    if (firstEditLine1 == 0u) firstEditLine1 = d.startLine + 1u;
                    netDiff += d.lineDelta;
                    auto mapLine1 = [&](keel::usize& line1) {
                        if (line1 == 0u) return;
                        const keel::usize mapped0 = MapLineThroughEdit(line1 - 1u, d);
                        line1 = keel::min(mapped0 + 1u, newLineCount);
                    };
                    keel::MutArrayRef<keel::usize> bpMut = activeBreakpoints.as_mut_ref();
                    for (keel::usize& bp : bpMut) mapLine1(bp);
                    if (opts.executionLinePtr != nullptr && *opts.executionLinePtr > 0u) {
                        mapLine1(*opts.executionLinePtr);
                    }
                    for (keel::usize& fl : state.foldedLines) {
                        fl = keel::min(MapLineThroughEdit(fl, d), newLineCount - 1u);
                    }
                    for (FoldRegion& pr : state.preservedFoldedRegions) {
                        pr.startLine = keel::min(MapLineThroughEdit(pr.startLine, d), newLineCount - 1u);
                        pr.endLine = keel::min(MapLineThroughEdit(pr.endLine, d), newLineCount - 1u);
                    }
                }
            }
            keel::MutArrayRef<keel::usize> bpMut = activeBreakpoints.as_mut_ref();
            if (bpMut.size() > 1u) {
                SortSizeT(bpMut);
                for (keel::usize bi = bpMut.size() - 1u; bi > 0u; --bi) {
                    if (bpMut[bi] == bpMut[bi - 1u]) activeBreakpoints.erase_at(bi);
                }
            }
            if (netDiff != 0) {
                res.lineDelta += keel::as<keel::i32>(netDiff);
                if (res.editLine == 0u) res.editLine = firstEditLine1;
            }
        } else if (!contiguous) {
            state.lexerValidCheckpoints = state.lexerCheckpoints.empty() ? 0u : 1u;
            state.preservedFoldedRegions.clear();
            state.foldedLines.clear();
        }
        state.lastDocRevision = docPtr->Revision();
        docCacheKey = detail::HashArgs(reinterpret_cast<keel::usize>(docPtr), docPtr->Revision());
    };

    if (docMutatedByExternal) {
        syncDocumentDeltas();
    }

    auto resolveDebugAndDiagnosticMarkers = [&](const TextSourceView& src) {
        const keel::usize lineCnt = src.LineCount();
        if (opts.executionLinePtr != nullptr) {
            optsCopy.executionLine = *opts.executionLinePtr;
        } else if (!isDocMode && opts.executionLine > 0u &&
                   opts.executionLine < state.baselineToCurrentLine.size() &&
                   state.baselineToCurrentLine[opts.executionLine] > 0u) {
            optsCopy.executionLine = state.baselineToCurrentLine[opts.executionLine];
        } else {
            optsCopy.executionLine = opts.executionLine;
        }
        if (optsCopy.executionLine > lineCnt) {
            optsCopy.executionLine = lineCnt;
        }

        state.resolvedInlineValues.clear();
        if (!opts.inlineValues.empty()) {
            state.resolvedInlineValues.reserve(opts.inlineValues.size());
            for (const CodeInlineValue& iv : opts.inlineValues) {
                if (iv.text.empty()) continue;
                const keel::usize resolvedLine = (!isDocMode && state.baselineInitialized)
                    ? ResolveTrackedCodeLine(
                        src.flatText, logicalLines, iv.line, iv.anchor,
                        state.baselineToCurrentLine, state.baselineSignatures, lineCmtPrefix)
                    : iv.line;
                if (resolvedLine >= 1u && resolvedLine <= lineCnt) {
                    state.resolvedInlineValues.push_back(CodeInlineValue{
                        .line = resolvedLine,
                        .text = ctx.AllocString(iv.text),
                        .variant = iv.variant,
                        .anchor = ctx.AllocString(iv.anchor)
                    });
                }
            }
        }
        optsCopy.inlineValues = state.resolvedInlineValues;

        state.resolvedDiagnostics.clear();
        if (!opts.diagnostics.empty()) {
            state.resolvedDiagnostics.reserve(opts.diagnostics.size());
            keel::String diagLineScratch;
            for (const CodeDiagnostic& diag : opts.diagnostics) {
                const keel::usize resolvedLine = (!isDocMode && state.baselineInitialized)
                    ? ResolveTrackedCodeLine(
                        src.flatText, logicalLines, diag.line, diag.anchor,
                        state.baselineToCurrentLine, state.baselineSignatures, lineCmtPrefix)
                    : diag.line;
                if (resolvedLine >= 1u && resolvedLine <= lineCnt) {
                    CodeDiagnostic d = diag;
                    d.line = resolvedLine;
                    d.message = ctx.AllocString(diag.message);
                    d.code = ctx.AllocString(diag.code);
                    d.anchor = ctx.AllocString(diag.anchor);
                    if (!diag.anchor.empty()) {
                        const TextLineRange lr = src.LineByteRange(resolvedLine - 1u);
                        if (lr.contentBytes <= kMaxSyntaxLineBytes) {
                            const keel::StringRef fullLine = src.ReadSliceRef(lr.startByte, lr.contentBytes, diagLineScratch);
                            if (const keel::usize anchorPos = fullLine.find(diag.anchor); anchorPos != keel::StringRef::npos) {
                                d.startCol = keel::as<keel::u32>(anchorPos);
                                d.endCol = keel::as<keel::u32>(anchorPos + diag.anchor.size());
                            }
                        }
                    }
                    state.resolvedDiagnostics.push_back(d);
                }
            }
        }
        optsCopy.diagnostics = state.resolvedDiagnostics;
    };

    resolveDebugAndDiagnosticMarkers(source);

    const keel::u64 foldSyntaxHash = opts.syntax.has_value()
        ? detail::HashArgs(
            opts.syntax->lineComment,
            opts.syntax->foldOpenKeywords.size(),
            opts.syntax->foldCloseKeywords.size(),
            opts.syntax->transposeApostrophe,
            opts.syntax->caseInsensitiveKeywords)
        : 0u;

    auto refreshFoldRegions = [&](bool forceRebuild, keel::Optional<keel::usize> centerLineOverride = {}) {
        if (opts.codeFolding) {
            const keel::usize totalLines = source.LineCount();
            if (totalLines <= kWindowedFoldThresholdLines) {
                if (forceRebuild || !state.cachedFoldEnabled || state.cachedFoldSyntaxHash != foldSyntaxHash ||
                    state.cachedFoldWindowBucket != keel::as<keel::usize>(-1)) {
                    ComputeFoldRegions(source, opts.syntax.has_value() ? &*opts.syntax : nullptr, foldRegions);
                    state.cachedFoldEnabled = true;
                    state.cachedFoldSyntaxHash = foldSyntaxHash;
                    state.cachedFoldWindowBucket = keel::as<keel::usize>(-1);
                }
            } else {
                // Viewport-windowed code folding for 50k..1M+ line files (scans <= 1024 lines, cached per 256-line bucket!)
                keel::usize approxCenterLine = 0u;
                if (centerLineOverride.has_value()) {
                    approxCenterLine = keel::min(totalLines - 1u, *centerLineOverride);
                } else {
                    const keel::f32 baseFs = opts.monospace
                        ? theme.GetTypography(TextRole::CodeMedium).fontSize
                        : theme.ResolveFontSize(TextRole::BodyMedium, activeSize);
                    const EditorFontMetrics foldFm = ResolveEditorFontMetrics(
                        canvas, theme, baseFs, state.zoom, textStyle, opts.tabSize);
                    const keel::f64 estLineH = keel::max(1.0, keel::as<keel::f64>(foldFm.lineHeight));
                    approxCenterLine = keel::min(
                        totalLines - 1u,
                        keel::as<keel::usize>(keel::max(0.0, keel::floor(state.scrollY / estLineH)))
                    );
                }
                const keel::usize winBucket = approxCenterLine / kWindowedFoldBucketLines;
                if (forceRebuild || !state.cachedFoldEnabled || state.cachedFoldSyntaxHash != foldSyntaxHash ||
                    state.cachedFoldWindowBucket != winBucket) {
                    const keel::usize winStart = (approxCenterLine > kWindowedFoldRadiusLines)
                        ? (approxCenterLine - kWindowedFoldRadiusLines) : 0u;
                    const keel::usize winEnd = keel::min(totalLines, approxCenterLine + kWindowedFoldRadiusLines);
                    ComputeFoldRegions(
                        source, opts.syntax.has_value() ? &*opts.syntax : nullptr, foldRegions,
                        winStart, winEnd, state.preservedFoldedRegions
                    );
                    state.cachedFoldEnabled = true;
                    state.cachedFoldSyntaxHash = foldSyntaxHash;
                    state.cachedFoldWindowBucket = winBucket;
                }
            }
            keel::usize writeIdx = 0u;
            for (keel::usize i = 0u; i < state.foldedLines.size(); ++i) {
                const keel::usize fl = state.foldedLines[i];
                if (FindFoldRegionStartingAt(foldRegions, fl) != nullptr) {
                    state.foldedLines[writeIdx++] = fl;
                }
            }
            state.foldedLines.resize(writeIdx);
        } else {
            foldRegions.clear();
            state.cachedFoldEnabled = false;
            state.cachedFoldWindowBucket = keel::as<keel::usize>(-1);
            state.foldedLines.clear();
            state.preservedFoldedRegions.clear();
        }
    };
    refreshFoldRegions(docChangedForCache);

    auto toggleBreakpointAtLine = [&](keel::usize line1Based) {
        if (line1Based == 0u || line1Based > source.LineCount()) return;
        const keel::ArrayRef<keel::usize> bpView = activeBreakpoints.as_ref();
        keel::usize foundIdx = bpView.size();
        for (keel::usize i = 0u; i < bpView.size(); ++i) {
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

    auto ensureLineUnfolded = [&](keel::usize line0Based) -> bool {
        if (state.foldedLines.empty()) return false;
        keel::usize writeIdx = 0u;
        bool changedAny = false;
        for (keel::usize i = 0u; i < state.foldedLines.size(); ++i) {
            const keel::usize fl = state.foldedLines[i];
            bool shouldUnfold = false;
            if (const FoldRegion* r = FindFoldRegionStartingAt(foldRegions, fl)) {
                shouldUnfold = (line0Based > r->startLine && line0Based <= r->endLine);
            }
            if (!shouldUnfold) {
                state.foldedLines[writeIdx++] = fl;
            } else {
                changedAny = true;
                for (keel::usize pi = 0u; pi < state.preservedFoldedRegions.size(); ++pi) {
                    if (state.preservedFoldedRegions[pi].startLine == fl) {
                        state.preservedFoldedRegions.remove_at(pi);
                        break;
                    }
                }
            }
        }
        state.foldedLines.resize(writeIdx);
        return changedAny;
    };

    if (optsCopy.executionLine > 0u && optsCopy.executionLine != state.prevExecutionLine) {
        ensureLineUnfolded(optsCopy.executionLine - 1u);
    }

    // Projection & single-pass layout helper
    TextViewProjection proj{};
    keel::Array<VisualLine> unusedExactBuf;
    keel::Array<keel::usize> unusedSmallPref;
    keel::Array<keel::usize> unusedChunkPref;

    auto computeLayout = [&]() -> EditorLayoutRects {
        return ComputeEditorLayout(
            refBounds, source, optsCopy, activeSize, canvas, theme,
            state.zoom, state.scrollX, state.scrollY, copyFeedbackActive,
            state.searchOpen, state.replaceOpen, state.gotoOpen,
            foldRegions, state.foldedLines, docCacheKey, &state, nullptr,
            proj, unusedExactBuf, unusedSmallPref, unusedChunkPref,
            state.debugToolbarRatio
        );
    };

    EditorLayoutRects L = computeLayout();
    bool layoutDirty = false;

    auto scrollLogicalLineIntoView = [&](keel::usize line0Based) {
        if (ensureLineUnfolded(line0Based)) {
            L = computeLayout();
        }
        const keel::usize targetVis = proj.LogicalLineToVisualRow(line0Based);
        const keel::f64 lineTop = keel::as<keel::f64>(targetVis) * keel::as<keel::f64>(L.lineHeight);
        const keel::f64 lineBot = lineTop + keel::as<keel::f64>(L.lineHeight);
        const keel::f64 vpH = keel::as<keel::f64>(L.textViewport.h);
        if (lineTop < state.scrollY || lineBot > state.scrollY + vpH) {
            state.scrollY = keel::clamp(lineTop - vpH * 0.35, 0.0, L.maxScrollY);
        }
    };

    if (optsCopy.executionLine > 0u && optsCopy.executionLine != state.prevExecutionLine) {
        scrollLogicalLineIntoView(optsCopy.executionLine - 1u);
        state.prevExecutionLine = optsCopy.executionLine;
    } else if (optsCopy.executionLine == 0u) {
        state.prevExecutionLine = 0u;
    }

    auto setCursorAndAnchor = [&](keel::usize newCur, keel::usize newAnc) {
        const keel::usize maxOff = source.ByteLength();
        newCur = keel::min(newCur, maxOff);
        newAnc = keel::min(newAnc, maxOff);
        state.cursor = newCur;
        state.anchor = newAnc;
        state.selectionStart = keel::min(newCur, newAnc);
        state.selectionEnd = keel::max(newCur, newAnc);
        if (!isDocMode) {
            ctx.SetTextEditCursor(id, newCur, newAnc);
        } else if (mutDoc) {
            mutDoc->BreakUndoCoalesce();
        }
    };

    // Cached search helper (only scans when docCacheKey / query / matchCase / wholeWord changes!)
    auto ensureSearchCache = [&]() {
        if (!state.searchOpen || state.searchQuery.empty()) {
            state.cachedSearchMatches.clear();
            state.searchMatchCount = 0u;
            state.searchMatchesCapped = false;
            state.activeMatchIdx = 0u;
            state.cachedSearchKey = 0u;
            for (keel::u8& b : state.cachedSearchRulerBuckets) b = 0u;
            return;
        }
        const keel::u64 searchKey = detail::HashArgs(
            docCacheKey, state.searchQuery.as_ref(), state.matchCase, state.wholeWord);
        if (state.cachedSearchKey != searchKey) {
            FindAllSearchMatches(
                source, state.searchQuery.as_ref(), state.matchCase, state.wholeWord,
                state.cachedSearchMatches, state.cachedSearchRulerBuckets.as_mut_ref(),
                state.searchMatchesCapped, kMaxStoredSearchMatches
            );
            state.cachedSearchKey = searchKey;
        }
        state.searchMatchCount = state.cachedSearchMatches.size();
        if (state.activeMatchIdx >= state.cachedSearchMatches.size()) {
            state.activeMatchIdx = 0u;
        }
    };

    // --- Handle IDE & Debugger Keyboard Shortcuts when focused ---
    bool modifiedByIdeCommand = false;
    if (isFocused && !effectiveDisabled) {
        // KeyAction::Find (Ctrl+F)
        if (opts.enableSearch && ctx.ConsumeAction(KeyAction::Find)) {
            state.searchOpen = true;
            state.gotoOpen = false;
            state.overlayFocus = 1;
            layoutDirty = true;
            keel::usize s0 = 0u, s1 = 0u;
            bool hasSelForFind = false;
            if (isDocMode) {
                s0 = state.selectionStart;
                s1 = state.selectionEnd;
                hasSelForFind = (s1 > s0);
            } else {
                hasSelForFind = ctx.GetTextEditSelection(id, s0, s1) && s1 > s0;
            }
            if (hasSelForFind && (s1 - s0) <= 64u) {
                keel::String selScratch;
                const keel::StringRef sel = source.ReadSliceRef(s0, s1 - s0, selScratch);
                if (sel.find('\n') == keel::StringRef::npos) {
                    state.searchQuery.assign(sel);
                }
            }
        }
        // KeyAction::Replace (Ctrl+H)
        if (opts.enableSearch && !effectiveReadOnly && ctx.ConsumeAction(KeyAction::Replace)) {
            state.searchOpen = true;
            state.replaceOpen = true;
            state.gotoOpen = false;
            state.overlayFocus = state.searchQuery.empty() ? 1 : 2;
            layoutDirty = true;
            keel::usize s0 = 0u, s1 = 0u;
            bool hasSelForFind = false;
            if (state.searchQuery.empty()) {
                if (isDocMode) {
                    s0 = state.selectionStart;
                    s1 = state.selectionEnd;
                    hasSelForFind = (s1 > s0);
                } else {
                    hasSelForFind = ctx.GetTextEditSelection(id, s0, s1) && s1 > s0;
                }
                if (hasSelForFind && (s1 - s0) <= 64u) {
                    keel::String selScratch;
                    const keel::StringRef sel = source.ReadSliceRef(s0, s1 - s0, selScratch);
                    if (sel.find('\n') == keel::StringRef::npos) {
                        state.searchQuery.assign(sel);
                    }
                }
            }
        }
        // KeyAction::GoToLine (Ctrl+G)
        if (opts.enableSearch && ctx.ConsumeAction(KeyAction::GoToLine)) {
            state.gotoOpen = true;
            state.searchOpen = false;
            state.overlayFocus = 3;
            state.gotoQuery.clear();
            layoutDirty = true;
        }
        // KeyAction::ToggleComment (Ctrl+/)
        if (!effectiveReadOnly && ctx.ConsumeAction(KeyAction::ToggleComment)) {
            const keel::StringRef cmtPrefix = lineCmtPrefix;
            keel::usize selS = state.selectionStart;
            keel::usize selE = state.selectionEnd;
            if (selE > selS && selE <= source.ByteLength() && source.ByteAt(selE - 1u) == '\n') {
                --selE;
            }
            const keel::usize firstLine = source.OffsetToLineCol(selS).line;
            const keel::usize lastLine = source.OffsetToLineCol(selE).line;

            bool allCommented = true;
            keel::String lineScratch;
            for (keel::usize li = firstLine; li <= lastLine; ++li) {
                const TextLineRange lr = source.LineByteRange(li);
                const keel::StringRef ls = source.ReadSliceRef(lr.startByte, keel::min<keel::usize>(lr.contentBytes, 256u), lineScratch);
                keel::usize p = 0u;
                while (p < ls.size() && (ls[p] == ' ' || ls[p] == '\t')) ++p;
                if (p >= ls.size()) continue;
                if (!ls.substr(p).starts_with(cmtPrefix)) {
                    allCommented = false;
                    break;
                }
            }

            if (mutDoc) {
                const keel::usize origCur = state.cursor;
                const keel::usize origAnc = state.anchor;
                keel::usize newCur = origCur;
                keel::usize newAnc = origAnc;
                TextDocument::EditGroupScope grp(*mutDoc);
                for (keel::usize li = lastLine + 1u; li > firstLine; --li) {
                    const keel::usize lineIdx = li - 1u;
                    const TextLineRange lr = mutDoc->LineByteRange(lineIdx);
                    mutDoc->ReadSlice(lr.startByte, keel::min<keel::usize>(lr.contentBytes, 256u), lineScratch);
                    const keel::StringRef ls = lineScratch.as_ref();
                    keel::usize p = 0u;
                    while (p < ls.size() && (ls[p] == ' ' || ls[p] == '\t')) ++p;
                    if (p >= ls.size() && firstLine != lastLine) continue;

                    const keel::usize editPos = lr.startByte + p;
                    if (allCommented && ls.substr(p).starts_with(cmtPrefix)) {
                        keel::usize remLen = cmtPrefix.size();
                        if (p + remLen < ls.size() && ls[p + remLen] == ' ') ++remLen;
                        mutDoc->ReplaceRange(editPos, remLen, keel::StringRef{}, origCur, origAnc, newCur, newAnc, true, false);
                        if (newCur > editPos) newCur = (newCur >= editPos + remLen) ? (newCur - remLen) : editPos;
                        if (newAnc > editPos) newAnc = (newAnc >= editPos + remLen) ? (newAnc - remLen) : editPos;
                    } else if (!allCommented) {
                        keel::SmallString<32> ins;
                        ins.append(cmtPrefix);
                        ins.push_back(' ');
                        mutDoc->ReplaceRange(editPos, 0u, ins.as_ref(), origCur, origAnc, newCur, newAnc, true, false);
                        if (newCur >= editPos) newCur += ins.size();
                        if (newAnc >= editPos) newAnc += ins.size();
                    }
                }
                source = makeSourceView();
                syncDocumentDeltas();
                setCursorAndAnchor(newCur, newAnc);
                modifiedByIdeCommand = true;
                layoutDirty = true;
            } else if (!isDocMode) {
                keel::String newDoc = keel::String::from(valStr);
                keel::usize newCur = state.cursor;
                keel::usize newAnc = (state.cursor == state.selectionStart) ? state.selectionEnd : state.selectionStart;
                keel::isize cumulativeShift = 0;

                for (keel::usize li = firstLine; li <= lastLine; ++li) {
                    const keel::usize lineStart = keel::as<keel::usize>(keel::as<keel::isize>(logicalLines[li].startOffset) + cumulativeShift);
                    const keel::usize lineLen = logicalLines[li].length;
                    const keel::StringRef ls = newDoc.as_ref().substr(lineStart, lineLen);
                    keel::usize p = 0u;
                    while (p < ls.size() && (ls[p] == ' ' || ls[p] == '\t')) ++p;
                    if (p >= ls.size() && firstLine != lastLine) continue;

                    const keel::usize editPos = lineStart + p;
                    if (allCommented && ls.substr(p).starts_with(cmtPrefix)) {
                        keel::usize remLen = cmtPrefix.size();
                        if (p + remLen < ls.size() && ls[p + remLen] == ' ') ++remLen;
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
                valStr = ctx.EnsureTextEdit(id, newDoc, true, effectiveReadOnly);
                SplitLogicalLines(valStr, logicalLines, &state.cachedMaxLineBytes, &state.cachedPureAsciiNoTabs);
                state.cachedDocHash = detail::HashArgs(valStr);
                state.cachedDocSize = valStr.size();
                docCacheKey = detail::HashArgs(state.cachedDocHash, valStr.size());
                source = makeSourceView();
                modifiedByIdeCommand = true;
                layoutDirty = true;
            }
        }

        // Debugger actions (F9, F5, Shift+F5, F10, F11, Shift+F11)
        if ((opts.breakpoints.has_target() || opts.enableBreakpoints) && ctx.ConsumeAction(KeyAction::ToggleBreakpoint)) {
            const keel::usize curLine0 = source.OffsetToLineCol(state.cursor).line;
            toggleBreakpointAtLine(curLine0 + 1u);
        }
        if (ctx.ConsumeAction(KeyAction::DebugContinue)) res.debugAction = CodeDebugAction::Continue;
        else if (ctx.ConsumeAction(KeyAction::DebugStop)) res.debugAction = CodeDebugAction::Stop;
        if (ctx.ConsumeAction(KeyAction::DebugStepOver)) res.debugAction = CodeDebugAction::StepOver;
        if (ctx.ConsumeAction(KeyAction::DebugStepInto)) res.debugAction = CodeDebugAction::StepInto;
        else if (ctx.ConsumeAction(KeyAction::DebugStepOut)) res.debugAction = CodeDebugAction::StepOut;
    }

    ensureSearchCache();
    keel::Array<SearchMatch>& searchMatches = state.cachedSearchMatches;

    auto jumpToSearchMatch = [&](keel::usize matchIdx) {
        if (searchMatches.empty()) return;
        state.activeMatchIdx = matchIdx % searchMatches.size();
        const SearchMatch& m = searchMatches[state.activeMatchIdx];
        setCursorAndAnchor(m.endOffset, m.startOffset);
        scrollLogicalLineIntoView(m.logicalLine);
    };

    auto performReplaceOne = [&]() {
        if (effectiveReadOnly || searchMatches.empty()) return;
        if (state.activeMatchIdx >= searchMatches.size()) state.activeMatchIdx = 0u;
        const SearchMatch m = searchMatches[state.activeMatchIdx];
        const keel::usize newCur = m.startOffset + state.replaceQuery.size();
        if (mutDoc) {
            mutDoc->ReplaceRange(m.startOffset, m.endOffset - m.startOffset, state.replaceQuery.as_ref(),
                                 state.cursor, state.anchor, newCur, newCur, true, false);
            source = makeSourceView();
            syncDocumentDeltas();
        } else if (!isDocMode) {
            keel::String newDoc;
            newDoc.reserve(valStr.size() - (m.endOffset - m.startOffset) + state.replaceQuery.size());
            newDoc.append(valStr.substr(0u, m.startOffset));
            newDoc.append(state.replaceQuery);
            newDoc.append(valStr.substr(m.endOffset));
            ctx.ApplyTextEditChange(id, newDoc, newCur, newCur);
            valStr = ctx.EnsureTextEdit(id, newDoc, true, effectiveReadOnly);
            SplitLogicalLines(valStr, logicalLines, &state.cachedMaxLineBytes, &state.cachedPureAsciiNoTabs);
            state.cachedDocHash = detail::HashArgs(valStr);
            state.cachedDocSize = valStr.size();
            docCacheKey = detail::HashArgs(state.cachedDocHash, valStr.size());
            source = makeSourceView();
        }
        ensureSearchCache();
        if (!searchMatches.empty()) {
            if (state.activeMatchIdx >= searchMatches.size()) state.activeMatchIdx = 0u;
            jumpToSearchMatch(state.activeMatchIdx);
        } else {
            setCursorAndAnchor(newCur, newCur);
        }
        modifiedByIdeCommand = true;
        layoutDirty = true;
    };

    auto performReplaceAll = [&]() {
        if (effectiveReadOnly || searchMatches.empty() || state.searchQuery.empty()) return;
        if (mutDoc) {
            const keel::usize origCur = state.cursor;
            const keel::usize origAnc = state.anchor;
            {
                TextDocument::EditGroupScope grp(*mutDoc);
                for (keel::usize i = searchMatches.size(); i > 0u; --i) {
                    const SearchMatch& m = searchMatches[i - 1u];
                    mutDoc->ReplaceRange(m.startOffset, m.endOffset - m.startOffset, state.replaceQuery.as_ref(),
                                         origCur, origAnc, origCur, origAnc, true, false);
                }
            }
            source = makeSourceView();
            syncDocumentDeltas();
            const keel::usize clampedCur = keel::min(origCur, mutDoc->ByteLength());
            setCursorAndAnchor(clampedCur, clampedCur);
        } else if (!isDocMode) {
            keel::String newDoc;
            newDoc.reserve(valStr.size());
            keel::usize lastPos = 0u;
            for (const SearchMatch& m : searchMatches) {
                newDoc.append(valStr.substr(lastPos, m.startOffset - lastPos));
                newDoc.append(state.replaceQuery);
                lastPos = m.endOffset;
            }
            newDoc.append(valStr.substr(lastPos));
            const keel::usize newCur = keel::min(state.cursor, newDoc.size());
            ctx.ApplyTextEditChange(id, newDoc, newCur, newCur);
            valStr = ctx.EnsureTextEdit(id, newDoc, true, effectiveReadOnly);
            SplitLogicalLines(valStr, logicalLines, &state.cachedMaxLineBytes, &state.cachedPureAsciiNoTabs);
            state.cachedDocHash = detail::HashArgs(valStr);
            state.cachedDocSize = valStr.size();
            docCacheKey = detail::HashArgs(state.cachedDocHash, valStr.size());
            source = makeSourceView();
        }
        ensureSearchCache();
        modifiedByIdeCommand = true;
        layoutDirty = true;
    };

    // FindNext / FindPrev (F3 / Shift+F3)
    if (isFocused && !effectiveDisabled) {
        const bool findNext = ctx.ConsumeAction(KeyAction::FindNext);
        const bool findPrev = !findNext && ctx.ConsumeAction(KeyAction::FindPrev);
        if (findNext || findPrev) {
            if (!state.searchOpen) {
                state.searchOpen = true;
                layoutDirty = true;
                ensureSearchCache();
            }
            if (!searchMatches.empty()) {
                const keel::usize n = searchMatches.size();
                const keel::usize nextIdx = findPrev ? ((state.activeMatchIdx + n - 1u) % n) : ((state.activeMatchIdx + 1u) % n);
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
                if (keel::as<keel::u8>(c) >= 32u && c != 127) {
                    if (state.overlayFocus == 3 && !keel::is_ascii_digit(c)) continue;
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
                if (!state.replaceOpen && !effectiveReadOnly) {
                    state.replaceOpen = true;
                    layoutDirty = true;
                }
                if (state.replaceOpen && !effectiveReadOnly) {
                    state.overlayFocus = (state.overlayFocus == 1) ? 2 : 1;
                }
            }
        }
        if (ctx.ConsumeAction(KeyAction::Cancel)) {
            state.searchOpen = false;
            state.gotoOpen = false;
            state.overlayFocus = 0;
            layoutDirty = true;
        }
        KeyModifiers kMods{};
        if (ctx.ConsumeAction(KeyAction::Confirm) || ctx.ConsumeKey(Key::Enter, kMods)) {
            if (state.overlayFocus == 1) {
                if (!searchMatches.empty()) {
                    const keel::usize n = searchMatches.size();
                    const keel::usize nextIdx = kMods.shift ? ((state.activeMatchIdx + n - 1u) % n) : ((state.activeMatchIdx + 1u) % n);
                    jumpToSearchMatch(nextIdx);
                }
            } else if (state.overlayFocus == 2) {
                if (kMods.ctrl && kMods.alt) performReplaceAll();
                else performReplaceOne();
            } else if (state.overlayFocus == 3) {
                const auto lineRes = keel::parse_u64(state.gotoQuery);
                if (lineRes.is_ok() && lineRes.value() >= 1u) {
                    const keel::usize targetLine1 = keel::as<keel::usize>(lineRes.value());
                    const keel::usize line0 = keel::min(targetLine1 - 1u, source.LineCount() - 1u);
                    const keel::usize destOffset = source.LineByteRange(line0).startByte;
                    setCursorAndAnchor(destOffset, destOffset);
                    scrollLogicalLineIntoView(line0);
                }
                state.gotoOpen = false;
                state.overlayFocus = 0;
                layoutDirty = true;
            }
        }

        if (queryEdited && state.overlayFocus == 1) {
            ensureSearchCache();
            state.activeMatchIdx = 0u;
            if (!searchMatches.empty()) {
                const keel::usize idx = FindFirstSearchMatchAtOrAfter(searchMatches, state.cursor);
                state.activeMatchIdx = (idx < searchMatches.size()) ? idx : 0u;
                scrollLogicalLineIntoView(searchMatches[state.activeMatchIdx].logicalLine);
            }
        }
    } else if (isFocused && !effectiveDisabled && (state.searchOpen || state.gotoOpen)) {
        if (ctx.ConsumeAction(KeyAction::Cancel)) {
            state.searchOpen = false;
            state.gotoOpen = false;
            state.overlayFocus = 0;
            layoutDirty = true;
        }
    }

    if (layoutDirty) {
        if (modifiedByIdeCommand) {
            refreshFoldRegions(true);
        }
        L = computeLayout();
        layoutDirty = false;
    }

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

    // Hit-test Gutter (Breakpoints & Code Folding chevrons) using f64 scroll
    keel::i32 hoveredGutterLine = -1;
    keel::i32 hoveredFoldLine = -1;
    if (res.hovered && L.gutterRect.w > 0.0f && L.gutterRect.Contains(mousePos)) {
        const keel::f64 relY = keel::as<keel::f64>(mousePos.y - L.textViewport.y) + state.scrollY;
        const keel::f64 rawIdxF = keel::floor(relY / keel::max(1.0, keel::as<keel::f64>(L.lineHeight)));
        if (rawIdxF >= 0.0 && rawIdxF < keel::as<keel::f64>(L.totalVisualRows)) {
            const VisualLine vl = proj.VisualRowAt(keel::as<keel::usize>(rawIdxF));
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

    // Handle mouse wheel (f64 precision)
    if (res.hovered && !effectiveDisabled) {
        const Vec2 wheel = ctx.GetMouseWheel();
        if (keel::abs(wheel.y) > 0.001f || keel::abs(wheel.x) > 0.001f) {
            if (opts.allowZoom && ctx.GetKeyModifiers().ctrl && keel::abs(wheel.y) > 0.001f) {
                const keel::f32 prevZoom = state.zoom;
                applyZoomAnchor(state.zoom + (wheel.y > 0.0f ? 0.10f : -0.10f));
                if (keel::abs(state.zoom - prevZoom) > 0.001f) {
                    L = computeLayout();
                }
                ctx.ConsumeMouseWheel();
            } else if (!opts.wordWrap && (ctx.GetKeyModifiers().shift || keel::abs(wheel.x) > keel::abs(wheel.y)) && L.maxScrollX > 0.0) {
                const keel::f64 delta = keel::as<keel::f64>((keel::abs(wheel.x) > 0.001f) ? wheel.x : wheel.y);
                state.scrollX = keel::clamp(state.scrollX - delta * keel::as<keel::f64>(L.lineHeight) * 2.5, 0.0, L.maxScrollX);
                ctx.ConsumeMouseWheel();
            } else if (!opts.autoHeight && L.maxScrollY > 0.0 && keel::abs(wheel.y) > 0.001f) {
                state.scrollY = keel::clamp(state.scrollY - keel::as<keel::f64>(wheel.y) * keel::as<keel::f64>(L.lineHeight) * 2.5, 0.0, L.maxScrollY);
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
                layoutDirty = true;
            } else if (hoveredOverlayBtn == 1) {
                state.matchCase = !state.matchCase;
                ensureSearchCache();
            } else if (hoveredOverlayBtn == 2) {
                state.wholeWord = !state.wholeWord;
                ensureSearchCache();
            } else if (hoveredOverlayBtn == 3 && !searchMatches.empty()) {
                const keel::usize n = searchMatches.size();
                jumpToSearchMatch((state.activeMatchIdx + n - 1u) % n);
            } else if (hoveredOverlayBtn == 4 && !searchMatches.empty()) {
                jumpToSearchMatch((state.activeMatchIdx + 1u) % searchMatches.size());
            } else if (hoveredOverlayBtn == 5) {
                state.searchOpen = false;
                state.gotoOpen = false;
                state.overlayFocus = 0;
                layoutDirty = true;
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
                    layoutDirty = true;
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
            for (keel::usize fi = 0u; fi < state.foldedLines.size(); ++fi) {
                if (state.foldedLines[fi] == fl) {
                    foundIdx = fi;
                    break;
                }
            }
            if (foundIdx < state.foldedLines.size()) {
                state.foldedLines.remove_at(foundIdx);
                for (keel::usize pi = 0u; pi < state.preservedFoldedRegions.size(); ++pi) {
                    if (state.preservedFoldedRegions[pi].startLine == fl) {
                        state.preservedFoldedRegions.remove_at(pi);
                        break;
                    }
                }
            } else {
                state.foldedLines.push_back(fl);
                if (const FoldRegion* fr = FindFoldRegionStartingAt(foldRegions, fl)) {
                    state.preservedFoldedRegions.push_back(*fr);
                }
            }
            layoutDirty = true;
        } else if (hoveredGutterLine >= 0) {
            consumedByChrome = true;
            toggleBreakpointAtLine(keel::as<keel::usize>(hoveredGutterLine) + 1u);
        } else if (copyHovered) {
            consumedByChrome = true;
            if (state.selectionEnd > state.selectionStart && state.selectionEnd <= source.ByteLength()) {
                keel::String selCopy;
                const keel::StringRef slice = source.ReadSliceRef(state.selectionStart, state.selectionEnd - state.selectionStart, selCopy);
                ctx.SetClipboardText(slice);
            } else if (isDocMode) {
                keel::String fullCopy;
                docPtr->ExtractText(fullCopy);
                ctx.SetClipboardText(fullCopy.as_ref());
            } else {
                ctx.SetClipboardText(valStr);
            }
            state.copyFeedbackUntil = now + 1.5;
            ctx.RequestRepaint(3);
        } else if (zoomHovered) {
            consumedByChrome = true;
            applyZoomAnchor(1.0f);
            layoutDirty = true;
        } else if (minimapHovered) {
            consumedByChrome = true;
            state.draggingMinimap = true;
        } else if (vStartBtnHovered) {
            consumedByChrome = true;
            state.scrollY = keel::max(0.0, state.scrollY - keel::as<keel::f64>(theme.GetScrollbar(activeSize).scrollStep));
        } else if (vEndBtnHovered) {
            consumedByChrome = true;
            state.scrollY = keel::min(L.maxScrollY, state.scrollY + keel::as<keel::f64>(theme.GetScrollbar(activeSize).scrollStep));
        } else if (hStartBtnHovered) {
            consumedByChrome = true;
            state.scrollX = keel::max(0.0, state.scrollX - keel::as<keel::f64>(theme.GetScrollbar(activeSize).scrollStep));
        } else if (hEndBtnHovered) {
            consumedByChrome = true;
            state.scrollX = keel::min(L.maxScrollX, state.scrollX + keel::as<keel::f64>(theme.GetScrollbar(activeSize).scrollStep));
        } else if (vThumbHovered) {
            consumedByChrome = true;
            state.draggingVThumb = true;
            state.dragGrabOffset = mousePos.y - L.vThumbRect.y;
        } else if (vTrackHovered) {
            consumedByChrome = true;
            const keel::f32 halfThumb = L.vThumbRect.h * 0.5f;
            const keel::f32 maxTravel = keel::max(1.0f, L.vTrackRect.h - L.vThumbRect.h);
            const keel::f64 t = keel::clamp(keel::as<keel::f64>(mousePos.y - L.vTrackRect.y - halfThumb) / keel::as<keel::f64>(maxTravel), 0.0, 1.0);
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
            const keel::f64 t = keel::clamp(keel::as<keel::f64>(mousePos.x - L.hTrackRect.x - halfThumb) / keel::as<keel::f64>(maxTravel), 0.0, 1.0);
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
            layoutDirty = true;
        }
    }
    if (layoutDirty) {
        L = computeLayout();
        layoutDirty = false;
    }

    if (state.draggingMinimap && ctx.IsMouseDown() && L.hasMinimap && L.minimapRect.h > 0.0f) {
        const keel::f64 microStep = 3.0;
        const keel::f64 totalMiniH = keel::max(1.0, keel::as<keel::f64>(L.totalVisualRows) * microStep);
        const keel::f64 effMiniH = keel::min(keel::as<keel::f64>(L.minimapRect.h), totalMiniH);
        const keel::f64 t = keel::clamp(keel::as<keel::f64>(mousePos.y - L.minimapRect.y) / keel::max(1.0, effMiniH), 0.0, 1.0);
        state.scrollY = t * L.maxScrollY;
    }
    if (state.draggingVThumb && ctx.IsMouseDown() && L.needVScroll) {
        const keel::f32 maxTravel = keel::max(1.0f, L.vTrackRect.h - L.vThumbRect.h);
        const keel::f64 t = keel::clamp(keel::as<keel::f64>(mousePos.y - L.vTrackRect.y - state.dragGrabOffset) / keel::as<keel::f64>(maxTravel), 0.0, 1.0);
        state.scrollY = t * L.maxScrollY;
    }
    if (state.draggingHThumb && ctx.IsMouseDown() && L.needHScroll) {
        const keel::f32 maxTravel = keel::max(1.0f, L.hTrackRect.w - L.hThumbRect.w);
        const keel::f64 t = keel::clamp(keel::as<keel::f64>(mousePos.x - L.hTrackRect.x - state.dragGrabOffset) / keel::as<keel::f64>(maxTravel), 0.0, 1.0);
        state.scrollX = t * L.maxScrollX;
    }

    state.scrollY = keel::clamp(state.scrollY, 0.0, L.maxScrollY);
    state.scrollX = keel::clamp(state.scrollX, 0.0, L.maxScrollX);

    keel::usize cursor = state.cursor;
    keel::usize selAnchor = state.anchor;
    keel::StringRef displayText = valStr;
    keel::String hitScratch;

    const bool activeInputSession = isDocMode ? isFocused : (isFocused || ctx.HasActiveTextEdit(id));
    if (activeInputSession) {
        // 2D Mouse hit-testing for cursor & selection (f64 + O(visibleCols) on huge lines!)
        if (!consumedByChrome && (mouseJustPressed || (state.draggingText && ctx.IsMouseDown()))) {
            const keel::f64 relY = keel::as<keel::f64>(mousePos.y - L.textViewport.y) + state.scrollY;
            const keel::f64 relX = keel::as<keel::f64>(mousePos.x - L.textViewport.x) + state.scrollX;

            const keel::f64 rawVisF = keel::floor(relY / keel::max(1.0, keel::as<keel::f64>(L.lineHeight)));
            const keel::usize visIdx = (rawVisF <= 0.0)
                ? 0u
                : keel::min(keel::as<keel::usize>(rawVisF), L.totalVisualRows - 1u);
            const VisualLine vl = proj.VisualRowAt(visIdx);
            const keel::usize colInSub = HitTestVisualRowX(source, vl, relX, L.metrics, textStyle, canvas, opts.tabSize, hitScratch);
            const keel::usize hitIdx = vl.startOffset + colInSub;

            auto selectWordAtHit = [&](keel::usize off, keel::usize& outS, keel::usize& outE) {
                if (isDocMode) {
                    docPtr->WordBoundsAt(off, outS, outE);
                } else {
                    TextUtil::SelectWordAt(valStr, off, outS, outE);
                }
            };

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
                        const keel::usize curAnc = isDocMode ? state.anchor : ctx.GetTextEditAnchor(id);
                        setCursorAndAnchor(hitIdx, curAnc);
                    } else {
                        setCursorAndAnchor(hitIdx, hitIdx);
                    }
                } else if (state.clickCount == 2) {
                    keel::usize wStart = 0u, wEnd = 0u;
                    selectWordAtHit(hitIdx, wStart, wEnd);
                    state.draggingWord = true;
                    state.wordDragStart = wStart;
                    state.wordDragEnd = wEnd;
                    setCursorAndAnchor(wEnd, wStart);
                } else {
                    state.draggingWord = false;
                    const TextLineRange lr = source.LineByteRange(vl.logicalLine);
                    setCursorAndAnchor(lr.startByte + lr.contentBytes, lr.startByte);
                }
            } else if (state.draggingText) {
                if (mousePos.y < L.textViewport.y && state.scrollY > 0.0) {
                    state.scrollY = keel::max(0.0, state.scrollY - keel::as<keel::f64>(L.lineHeight) * 0.35);
                } else if (mousePos.y > L.textViewport.Bottom() && state.scrollY < L.maxScrollY) {
                    state.scrollY = keel::min(L.maxScrollY, state.scrollY + keel::as<keel::f64>(L.lineHeight) * 0.35);
                }
                if (!opts.wordWrap) {
                    if (mousePos.x < L.textViewport.x && state.scrollX > 0.0) {
                        state.scrollX = keel::max(0.0, state.scrollX - keel::as<keel::f64>(L.lineHeight) * 0.5);
                    } else if (mousePos.x > L.textViewport.Right() && state.scrollX < L.maxScrollX) {
                        state.scrollX = keel::min(L.maxScrollX, state.scrollX + keel::as<keel::f64>(L.lineHeight) * 0.5);
                    }
                }

                if (state.clickCount >= 3) {
                    const TextLineRange lr = source.LineByteRange(vl.logicalLine);
                    setCursorAndAnchor(lr.startByte + lr.contentBytes, lr.startByte);
                } else if (state.draggingWord) {
                    keel::usize wStart = 0u, wEnd = 0u;
                    selectWordAtHit(hitIdx, wStart, wEnd);
                    if (hitIdx < state.wordDragStart) {
                        setCursorAndAnchor(wStart, state.wordDragEnd);
                    } else {
                        setCursorAndAnchor(wEnd, state.wordDragStart);
                    }
                } else {
                    const keel::usize curAnc = isDocMode ? state.anchor : ctx.GetTextEditAnchor(id);
                    setCursorAndAnchor(hitIdx, curAnc);
                }
            }
        }

        if (isDocMode) {
            bool docChangedThisFrame = modifiedByIdeCommand;
            if (isFocused && state.overlayFocus == 0) {
                bool lostFocus = false;
                if (mutDoc) {
                    if (HandleTextDocumentInput(ctx, id, *mutDoc, state, optsCopy, lostFocus)) {
                        docChangedThisFrame = true;
                    }
                } else {
                    // Read-only TextDocument: allow navigation, SelectAll, and Copy via a lightweight const handler.
                    // Consume navigation/copy actions without mutating the document.
                    ctx.ProcessPendingKeys([&](Key key, KeyModifiers mods) -> bool {
                        const KeyAction action = ctx.ResolveAction(key, mods);
                        switch (action) {
                            case KeyAction::SelectAll:
                                setCursorAndAnchor(docPtr->ByteLength(), 0u);
                                return true;
                            case KeyAction::Copy:
                                if (state.selectionEnd > state.selectionStart) {
                                    keel::String selText;
                                    docPtr->ReadSlice(state.selectionStart, state.selectionEnd - state.selectionStart, selText);
                                    ctx.SetClipboardText(selText.as_ref());
                                }
                                return true;
                            case KeyAction::MoveLeft:
                            case KeyAction::SelectLeft: {
                                const keel::usize next = docPtr->PrevCharOffset(state.cursor);
                                setCursorAndAnchor(next, (action == KeyAction::SelectLeft) ? state.anchor : next);
                                return true;
                            }
                            case KeyAction::MoveRight:
                            case KeyAction::SelectRight: {
                                const keel::usize next = docPtr->NextCharOffset(state.cursor);
                                setCursorAndAnchor(next, (action == KeyAction::SelectRight) ? state.anchor : next);
                                return true;
                            }
                            case KeyAction::MoveDocStart:
                            case KeyAction::SelectDocStart:
                                setCursorAndAnchor(0u, (action == KeyAction::SelectDocStart) ? state.anchor : 0u);
                                return true;
                            case KeyAction::MoveDocEnd:
                            case KeyAction::SelectDocEnd:
                                setCursorAndAnchor(docPtr->ByteLength(), (action == KeyAction::SelectDocEnd) ? state.anchor : docPtr->ByteLength());
                                return true;
                            case KeyAction::Cancel:
                                if (state.cursor != state.anchor) {
                                    setCursorAndAnchor(state.cursor, state.cursor);
                                } else {
                                    lostFocus = true;
                                }
                                return true;
                            default:
                                return false;
                        }
                    });
                }
                if (lostFocus) {
                    ctx.ClearFocus();
                }
            }
            if (docChangedThisFrame) {
                res.changed = true;
                source = makeSourceView();
                syncDocumentDeltas();
                resolveDebugAndDiagnosticMarkers(source);
                refreshFoldRegions(true);
                ensureSearchCache();
                L = computeLayout();
            }
            cursor = state.cursor;
            selAnchor = state.anchor;
        } else {
            // Legacy StringSink / StringRef TextBuffer flow
            bool changed = false;
            bool submitted = false;
            displayText = ctx.GetOrStartTextEdit(id, valStr, cursor, selAnchor, changed, submitted, /*multiline=*/true, effectiveReadOnly);
            changed = changed || modifiedByIdeCommand || (state.baselineInitialized && displayText != state.prevDocumentText);

            if (changed && !effectiveReadOnly) {
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
                    res.changed = (displayText != prevCallerVal) || (state.baselineInitialized && displayText != state.prevDocumentText);
                } else {
                    res.changed = true;
                }
                valStr = displayText;
                keel::Array<LogicalLine> oldLogicalLines;
                if (state.baselineInitialized) {
                    SplitLogicalLines(state.prevDocumentText, oldLogicalLines);
                }
                SplitLogicalLines(displayText, logicalLines, &state.cachedMaxLineBytes, &state.cachedPureAsciiNoTabs);
                const keel::usize oldLineCount = state.baselineInitialized ? oldLogicalLines.size() : state.prevLineCount;
                const keel::usize newLineCount = logicalLines.size();

                if (state.baselineInitialized && newLineCount != oldLineCount) {
                    const keel::isize diff = keel::as<keel::isize>(newLineCount) - keel::as<keel::isize>(oldLineCount);
                    const keel::StringRef oldDoc = state.prevDocumentText.as_ref();
                    keel::usize prefixLen = 0u;
                    const keel::usize maxCommon = keel::min(oldDoc.size(), displayText.size());
                    while (prefixLen < maxCommon && oldDoc[prefixLen] == displayText[prefixLen]) ++prefixLen;
                    keel::usize rawSuffixLen = 0u;
                    while (rawSuffixLen < maxCommon &&
                           oldDoc[oldDoc.size() - 1u - rawSuffixLen] == displayText[displayText.size() - 1u - rawSuffixLen]) {
                        ++rawSuffixLen;
                    }
                    const keel::usize suffixBound = oldDoc.size() - rawSuffixLen;
                    const keel::usize editLo = keel::min(prefixLen, suffixBound);
                    const keel::usize editHi = keel::max(prefixLen, suffixBound);
                    const keel::usize safePreCur = keel::clamp(keel::min(state.prevCursor, oldDoc.size()), editLo, editHi);
                    const TextSourceView oldSrc = TextSourceView::FromFlat(oldDoc, oldLogicalLines, 0u, false);
                    const TextSourceView newSrc = TextSourceView::FromFlat(displayText, logicalLines, state.cachedMaxLineBytes, state.cachedPureAsciiNoTabs);
                    const keel::usize oldEditLine0 = oldSrc.OffsetToLineCol(safePreCur).line;
                    const keel::usize oldEditLine1 = oldEditLine0 + 1u;
                    const keel::usize safePostCur = keel::min(cursor, displayText.size());
                    const keel::usize newEditLine0 = newSrc.OffsetToLineCol(safePostCur).line;
                    const keel::usize newEditLine1 = newEditLine0 + 1u;

                    keel::usize shiftStartLine1 = 1u;
                    if (diff > 0) {
                        const bool hasCodeBefore = HasCodeBeforeOffsetOnLine(oldDoc, oldLogicalLines[oldEditLine0], safePreCur);
                        shiftStartLine1 = hasCodeBefore ? (oldEditLine1 + 1u) : oldEditLine1;
                    } else {
                        shiftStartLine1 = newEditLine1 + 1u;
                    }

                    const keel::isize minTargetLine = (diff < 0) ? keel::as<keel::isize>(newEditLine1) : 1;
                    auto shiftLineNum = [&](keel::usize& lineNum) {
                        if (lineNum == 0u) return;
                        if (lineNum >= shiftStartLine1) {
                            const keel::isize shifted = keel::max<keel::isize>(minTargetLine, keel::as<keel::isize>(lineNum) + diff);
                            lineNum = keel::min<keel::usize>(keel::as<keel::usize>(shifted), newLineCount);
                        } else if (lineNum > newLineCount) {
                            lineNum = newLineCount;
                        }
                    };

                    keel::MutArrayRef<keel::usize> bpMut = activeBreakpoints.as_mut_ref();
                    for (keel::usize& bp : bpMut) shiftLineNum(bp);
                    if (bpMut.size() > 1u) {
                        SortSizeT(bpMut);
                        for (keel::usize bi = bpMut.size() - 1u; bi > 0u; --bi) {
                            if (bpMut[bi] == bpMut[bi - 1u]) activeBreakpoints.erase_at(bi);
                        }
                    }
                    for (keel::usize b = 1u; b < state.baselineToCurrentLine.size(); ++b) {
                        shiftLineNum(state.baselineToCurrentLine[b]);
                    }
                    if (opts.executionLinePtr != nullptr && *opts.executionLinePtr > 0u) {
                        shiftLineNum(*opts.executionLinePtr);
                        optsCopy.executionLine = *opts.executionLinePtr;
                        state.prevExecutionLine = *opts.executionLinePtr;
                    }
                    res.lineDelta = keel::as<keel::i32>(diff);
                    res.editLine = (diff > 0) ? oldEditLine1 : newEditLine1;
                }

                if (state.baselineInitialized) {
                    state.prevDocumentText.assign(displayText);
                }
                state.prevLineCount = newLineCount;
                state.cachedDocHash = detail::HashArgs(displayText);
                state.cachedDocSize = displayText.size();
                docCacheKey = detail::HashArgs(state.cachedDocHash, displayText.size());
                source = makeSourceView();
                resolveDebugAndDiagnosticMarkers(source);
                refreshFoldRegions(true);
                ensureSearchCache();
                L = computeLayout();
            }

            state.cursor = cursor;
            state.anchor = selAnchor;
            state.selectionStart = keel::min(cursor, selAnchor);
            state.selectionEnd = keel::max(cursor, selAnchor);
        }

        // Unfold if cursor moved into a folded block
        const keel::usize curLogical = source.OffsetToLineCol(cursor).line;
        if (proj.IsLineHidden(curLogical)) {
            if (ensureLineUnfolded(curLogical)) {
                L = computeLayout();
            }
        }

        // Auto-scroll to keep active cursor visible (f64 precision, O(log P))
        const keel::usize curTotalBytes = source.ByteLength();
        if (cursor != state.prevCursor || curTotalBytes != state.prevTextSize) {
            const keel::usize curVisRow = proj.OffsetToVisualRow(cursor);
            const VisualLine cvl = proj.VisualRowAt(curVisRow);
            const keel::f64 curTop = keel::as<keel::f64>(curVisRow) * keel::as<keel::f64>(L.lineHeight);
            const keel::f64 curBot = curTop + keel::as<keel::f64>(L.lineHeight);
            const keel::f64 vpH = keel::as<keel::f64>(L.textViewport.h);

            if (curTop < state.scrollY) {
                state.scrollY = curTop;
            } else if (curBot > state.scrollY + vpH && vpH > 0.0) {
                state.scrollY = curBot - vpH;
            }

            if (!opts.wordWrap && L.textViewport.w > 0.0f) {
                const keel::usize colOffset = (cursor >= cvl.startOffset) ? keel::min(cursor - cvl.startOffset, cvl.length) : 0u;
                const keel::f64 curX = MeasureColumnXInVisualRow(source, cvl, colOffset, L.metrics, textStyle, canvas, opts.tabSize, hitScratch);
                const keel::f64 margin = 16.0;
                const keel::f64 vpW = keel::as<keel::f64>(L.textViewport.w);
                if (curX < state.scrollX + margin) {
                    state.scrollX = keel::max(0.0, curX - margin);
                } else if (curX > state.scrollX + vpW - margin) {
                    state.scrollX = keel::min(L.maxScrollX, curX - vpW + margin);
                }
            }

            state.scrollY = keel::clamp(state.scrollY, 0.0, L.maxScrollY);
            state.scrollX = keel::clamp(state.scrollX, 0.0, L.maxScrollX);
            state.prevCursor = cursor;
            state.prevTextSize = curTotalBytes;
        }

        if (isFocused && !effectiveReadOnly && state.overlayFocus == 0 && L.totalVisualRows > 0u && L.textViewport.w > 0.0f) {
            const keel::usize curVisRow = proj.OffsetToVisualRow(cursor);
            const VisualLine cvl = proj.VisualRowAt(curVisRow);
            const keel::usize colOffset = (cursor >= cvl.startOffset) ? keel::min(cursor - cvl.startOffset, cvl.length) : 0u;
            const keel::f64 curX = MeasureColumnXInVisualRow(source, cvl, colOffset, L.metrics, textStyle, canvas, opts.tabSize, hitScratch);
            const keel::f32 caretX = L.textViewport.x + keel::as<keel::f32>(curX - state.scrollX);
            const keel::f32 caretY = L.textViewport.y + keel::as<keel::f32>(keel::as<keel::f64>(curVisRow) * keel::as<keel::f64>(L.lineHeight) - state.scrollY);
            ctx.SetImeTargetRect(Rect(caretX, caretY, 2.0f, L.lineHeight));
        }
    } else {
        state.anchor = state.cursor;
        state.selectionStart = state.cursor;
        state.selectionEnd = state.cursor;
    }

    ctx.SetSuspendDirectEditInput(!isDocMode && isFocused && state.overlayFocus != 0);

    // Tooltips for Debug Toolbar & Search/Replace overlay buttons
    if (hoveredDebugBtn >= 0 && hoveredDebugBtn < 6) {
        static constexpr keel::StaticArray<keel::StringRef, 6> kDbgTips = {
            "Continue (F5)", "Step Over (F10)", "Step Into (F11)",
            "Step Out (Shift+F11)", "Restart Debugger", "Stop Debugging (Shift+F5)"
        };
        ctx.SetTooltip(kDbgTips[keel::as<keel::usize>(hoveredDebugBtn)], L.debugToolbar.btnRects[keel::as<keel::usize>(hoveredDebugBtn)]);
    } else if (hoveredDebugBtn == 98 && !state.draggingDebugToolbar) {
        ctx.SetTooltip("Drag to move Debug Toolbar (Double-click to reset)", L.debugToolbar.gripRect);
    } else if (hoveredOverlayBtn >= 0 && hoveredOverlayBtn < 8) {
        static constexpr keel::StaticArray<keel::StringRef, 8> kOverlayTips = {
            "Toggle Replace (Ctrl+H)", "Match Case", "Match Whole Word",
            "Previous Match (Shift+Enter)", "Next Match (Enter / F3)",
            "Close (Escape)", "Replace Next Match", "Replace All Matches"
        };
        ctx.SetTooltip(kOverlayTips[keel::as<keel::usize>(hoveredOverlayBtn)], Rect(mousePos.x - 4.0f, mousePos.y - 4.0f, 8.0f, 8.0f));
    }

    // Detect hovered word & diagnostic tooltip in textViewport (bounded O(visibleCols)!)
    keel::String hoveredWordScratch;
    keel::StringRef hoveredWord{};
    keel::usize hoveredWordLine = 0u;
    keel::usize hoveredWordCol = 0u;
    if (res.hovered && L.textViewport.Contains(mousePos) && hoveredOverlayBtn < 0 && hoveredDebugBtn < 0) {
        const keel::f64 relY = keel::as<keel::f64>(mousePos.y - L.textViewport.y) + state.scrollY;
        const keel::f64 relX = keel::as<keel::f64>(mousePos.x - L.textViewport.x) + state.scrollX;
        const keel::f64 rawIdxF = keel::floor(relY / keel::max(1.0, keel::as<keel::f64>(L.lineHeight)));
        if (rawIdxF >= 0.0 && rawIdxF < keel::as<keel::f64>(L.totalVisualRows)) {
            const VisualLine vl = proj.VisualRowAt(keel::as<keel::usize>(rawIdxF));
            const keel::usize colInSub = HitTestVisualRowX(source, vl, relX, L.metrics, textStyle, canvas, opts.tabSize, hitScratch);
            const keel::usize hitOffset = vl.startOffset + colInSub;
            keel::usize wordStartOff = 0u;
            hoveredWord = ExtractWordAtOffset(source, hitOffset, hoveredWordScratch, &wordStartOff);
            if (!hoveredWord.empty()) {
                const TextLineRange lr = source.LineByteRange(vl.logicalLine);
                hoveredWordLine = vl.logicalLine;
                hoveredWordCol = (wordStartOff >= lr.startByte) ? (wordStartOff - lr.startByte) : 0u;
            }

            if (!optsCopy.diagnostics.empty()) {
                const TextLineRange lr = source.LineByteRange(vl.logicalLine);
                for (const CodeDiagnostic& diag : optsCopy.diagnostics) {
                    if (diag.line == vl.logicalLine + 1u && !diag.message.empty()) {
                        const keel::usize col0 = vl.startCol + colInSub;
                        const keel::usize dStart = diag.startCol;
                        const keel::usize dEnd = (diag.endCol > diag.startCol) ? diag.endCol : lr.contentBytes;
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
    }

    // Populate TextAreaResponse telemetry in O(log P)
    const keel::usize safeCur = keel::min(cursor, source.ByteLength());
    const TextLineCol curLc = source.OffsetToLineCol(safeCur);
    res.cursorLine = curLc.line + 1u;
    res.cursorColumn = curLc.colBytes + 1u;
    res.totalLines = source.LineCount();
    res.searchMatchCount = state.searchMatchCount;
    res.activeSearchMatch = (state.searchMatchCount > 0u) ? (state.activeMatchIdx + 1u) : 0u;
    const keel::StringRef arenaHoveredWord = ctx.AllocString(hoveredWord);
    res.hoveredWord = arenaHoveredWord;

    const bool cursorVisible = isFocused && ctx.IsCursorVisible(opts.cursorMode);
    const keel::f32 cursorWidth = ctx.GetCursorConfig().width;

    const keel::ArrayRef<keel::usize> bpView = activeBreakpoints.as_ref();
    keel::MutArrayRef<keel::usize> arenaBps{};
    if (!bpView.empty()) {
        arenaBps = ctx.GetArena().create_slice<keel::usize>(bpView.size());
        for (keel::usize i = 0u; i < bpView.size(); ++i) arenaBps[i] = bpView[i];
    }
    keel::MutArrayRef<keel::usize> arenaFolded{};
    if (!state.foldedLines.empty()) {
        arenaFolded = ctx.GetArena().create_slice<keel::usize>(state.foldedLines.size());
        for (keel::usize i = 0u; i < state.foldedLines.size(); ++i) arenaFolded[i] = state.foldedLines[i];
    }

    // Pre-resolve exact first visible logical line for windowed folding & checkpointed block comments!
    keel::usize firstVisLogical = 0u;
    if (L.totalVisualRows > 0u) {
        const keel::f64 lineHD = keel::max(1.0, keel::as<keel::f64>(L.lineHeight));
        const keel::usize firstVisRow = keel::min(
            L.totalVisualRows - 1u,
            keel::as<keel::usize>(keel::max(0.0, keel::floor(state.scrollY / lineHD)))
        );
        firstVisLogical = proj.VisualRowAt(firstVisRow).logicalLine;
    }
    if (opts.codeFolding && source.LineCount() > kWindowedFoldThresholdLines) {
        refreshFoldRegions(false, firstVisLogical);
    }
    bool resolvedInBlockComment = false;
    bool hasResolvedBlockComment = false;
    keel::usize resolvedBlockCommentLine = 0u;
    if (!effectiveDisabled && optsCopy.syntaxHighlighter == nullptr &&
        optsCopy.syntax.has_value() && optsCopy.syntax->enabled &&
        !optsCopy.syntax->blockCommentStart.empty() && !optsCopy.syntax->blockCommentEnd.empty() &&
        L.totalVisualRows > 0u) {
        resolvedBlockCommentLine = firstVisLogical;
        resolvedInBlockComment = ResolveBlockCommentStateAtLine(
            source, *optsCopy.syntax, resolvedBlockCommentLine, docCacheKey, &state);
        hasResolvedBlockComment = true;
    }

    Node* node = ctx.CreateNode(NodeType::TextArea, id);
    auto* data = ctx.AllocPayload<TextAreaData>();
    if (data) {
        data->opts = optsCopy;
        data->document = docPtr; // ZERO-COPY when backed by TextDocument!
        data->text = isDocMode ? keel::StringRef{} : ctx.AllocString(displayText);
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
        data->searchMatchesCapped = state.searchMatchesCapped;
        data->hoveredOverlayBtn = hoveredOverlayBtn;
        data->foldedLines = arenaFolded;
        data->breakpoints = arenaBps;
        data->precomputedLogicalLines = isDocMode ? keel::ArrayRef<LogicalLine>{} : keel::ArrayRef<LogicalLine>(logicalLines);
        data->precomputedFoldRegions = foldRegions;
        data->precomputedVisualLines = (proj.mode == TextViewProjection::Mode::ExactWrap) ? proj.exactVisualLines : keel::ArrayRef<VisualLine>{};
        data->precomputedSearchMatches = state.cachedSearchMatches;
        data->searchRulerBuckets = state.cachedSearchRulerBuckets.as_ref();
        data->precomputedWrapSmallLinePrefix = (proj.mode == TextViewProjection::Mode::AnalyticSmallWrap) ? proj.smallLinePrefix : keel::ArrayRef<keel::usize>{};
        data->precomputedWrapChunkPrefix = (proj.mode == TextViewProjection::Mode::AnalyticChunkWrap) ? proj.chunkPrefix : keel::ArrayRef<keel::usize>{};
        data->precomputedWrapCols = proj.wrapCols;
        data->precomputedTotalVisualRows = proj.totalVisualRows;
        data->cachedMaxLineBytes = source.MaxLineByteLength();
        data->cachedPureAsciiNoTabs = source.IsPureAsciiNoTabs();
        data->precomputedMaxLineW = L.maxLineW;
        data->hasPrecomputedMaxLineW = !optsCopy.wordWrap;
        data->initialInBlockComment = resolvedInBlockComment;
        data->hasInitialBlockComment = hasResolvedBlockComment;
        data->initialBlockCommentLine = resolvedBlockCommentLine;
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

    // O(1) visual hash using docCacheKey (TextDocument::Revision() or already-computed cachedDocHash)!
    node->visualHash = detail::HashArgs(
        docCacheKey,
        optsCopy.placeholder,
        keel::as<keel::u8>(optsCopy.style),
        keel::as<keel::u8>(optsCopy.status),
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
        state.searchMatchCount,
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
        if (d->document != nullptr) {
            return detail::TextArea::Measure(*d->document, mOpts, n->uiSize, c, t, availableWidth);
        }
        if (!d->precomputedLogicalLines.empty()) {
            return MeasureEditorBoxFromStats(
                d->precomputedLogicalLines.size(), d->cachedMaxLineBytes, mOpts, n->uiSize, c, t, d->zoom, availableWidth);
        }
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
                keel::as<keel::f32>(d->scrollY),
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

} // namespace

TextAreaResponse detail::TextArea::Run(
    Context& ctx,
    keel::Optional<keel::StringSink> stringValue,
    keel::StringRef displayFallback,
    const TextAreaOptions& opts,
    keel::SourceLocation loc
) {
    return RunTextAreaCore(ctx, nullptr, nullptr, keel::move(stringValue), displayFallback, opts, loc);
}

TextAreaResponse detail::TextArea::RunDocument(
    Context& ctx,
    TextDocument& doc,
    const TextAreaOptions& opts,
    keel::SourceLocation loc
) {
    return RunTextAreaCore(ctx, &doc, &doc, keel::kNullOpt, keel::StringRef{}, opts, loc);
}

TextAreaResponse detail::TextArea::RunDocumentReadOnly(
    Context& ctx,
    const TextDocument& doc,
    const TextAreaOptions& opts,
    keel::SourceLocation loc
) {
    return RunTextAreaCore(ctx, nullptr, &doc, keel::kNullOpt, keel::StringRef{}, opts, loc);
}

} // namespace guikit
