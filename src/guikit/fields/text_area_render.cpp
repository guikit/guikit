#include "text_area_internal.hpp"

namespace guikit {

namespace detail::ta_impl {

void DrawWavyUnderline(ICanvas& canvas, keel::f32 x0, keel::f32 x1, keel::f32 y, Color col, keel::f32 scale) {
    const keel::f32 step = keel::max(2.5f, 3.0f * scale);
    const keel::f32 amp = keel::max(1.2f, 1.5f * scale);
    keel::f32 x = x0;
    bool up = true;
    while (x < x1) {
        const keel::f32 nx = keel::min(x1, x + step);
        const keel::f32 yA = up ? (y - amp) : (y + amp);
        const keel::f32 yB = up ? (y + amp) : (y - amp);
        canvas.DrawLine({x, yA}, {nx, yB}, col, keel::max(1.0f, 1.25f * scale));
        x = nx;
        up = !up;
    }
}

} // namespace detail::ta_impl

using namespace detail::ta_impl;

Vec2 detail::TextArea::Measure(
    keel::StringRef text,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 availableWidth
) {
    return MeasureEditorBox(text, opts, size, canvas, theme, opts.zoom, availableWidth);
}

Vec2 detail::TextArea::Measure(
    keel::StringRef text,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas
) {
    return Measure(text, opts, size, canvas, UiTheme::Dark(), 0.0f);
}

Vec2 detail::TextArea::Measure(
    const TextDocument& doc,
    const TextAreaOptions& opts,
    UiSize size,
    ICanvas* canvas,
    const UiTheme& theme,
    keel::f32 availableWidth
) {
    return MeasureEditorBoxFromStats(
        doc.LineCount(),
        doc.MaxLineByteLength(),
        opts,
        size,
        canvas,
        theme,
        opts.zoom,
        availableWidth
    );
}

void detail::TextArea::Render(
    ICanvas& canvas,
    Rect bounds,
    keel::StringRef text,
    const TextAreaOptions& opts,
    UiSize size,
    bool hovered,
    bool focused,
    keel::usize cursor,
    bool disabled,
    bool cursorVisible,
    keel::f32 scrollY,
    const UiTheme& theme,
    const TextAreaData* extData
) {
    if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;

    const ThemePalette& pal = theme.palette;
    const SyntaxPalette& synPal = opts.syntaxPalette.has_value()
        ? *opts.syntaxPalette
        : ((opts.syntax.has_value() && opts.syntax->palette.has_value()) ? *opts.syntax->palette : theme.syntax);
    const bool hasCustomEditorSurface = (synPal.editorBg.a > 0);
    const Color editorBgCol = hasCustomEditorSurface ? synPal.editorBg : pal.inputBackground;
    const bool isEditorDark = (0.299f * editorBgCol.r + 0.587f * editorBgCol.g + 0.114f * editorBgCol.b) < 128.0f;
    const Color editorGutterBg = (synPal.gutterBg.a > 0)
        ? synPal.gutterBg
        : (isEditorDark ? Color(15, 23, 42, 190) : Color(241, 245, 249, 210));
    const Color editorBorderSubtle = hasCustomEditorSurface
        ? (isEditorDark ? Color(255, 255, 255, 26) : Color(15, 23, 42, 28))
        : pal.borderSubtle;
    const Color editorTextPrimary = hasCustomEditorSurface ? synPal.identifier : pal.textPrimary;
    const Color editorTextSecondary = hasCustomEditorSurface ? synPal.punctuation.WithAlphaF(0.85f) : pal.textSecondary;
    const Color editorTextMuted = hasCustomEditorSurface ? synPal.comment.WithAlphaF(0.82f) : pal.textMuted;
    const Color editorCaret = hasCustomEditorSurface
        ? (isEditorDark ? Color(248, 250, 252) : Color(15, 23, 42))
        : pal.caret;
    const ScrollbarMetrics& sm = theme.GetScrollbar(size);
    const FontStyle textStyle = opts.monospace ? FontStyle::Monospace : FontStyle::Regular;

    const keel::f32 effZoom = keel::clamp(extData ? extData->zoom : opts.zoom, 0.5f, 3.0f);
    const keel::f64 effScrollX = extData ? extData->scrollX : 0.0;
    const keel::f64 effScrollY = extData ? extData->scrollY : keel::as<keel::f64>(scrollY);
    const keel::usize selStart = extData ? extData->selectionStart : cursor;
    const keel::usize selEnd = extData ? extData->selectionEnd : cursor;
    const bool hasSelection = (selEnd > selStart);
    const bool searchOpen = extData && extData->searchOpen;
    const bool replaceOpen = extData && extData->replaceOpen;
    const bool gotoOpen = extData && extData->gotoOpen;
    const keel::ArrayRef<keel::usize> foldedLines = extData ? extData->foldedLines : keel::ArrayRef<keel::usize>{};
    const keel::ArrayRef<keel::usize> breakpoints = extData ? extData->breakpoints : opts.breakpoints.as_ref();

    // Build unified TextSourceView (zero-copy for TextDocument, zero-alloc for precomputed flat lines!)
    keel::Array<LogicalLine> fallbackLogicalLines;
    keel::usize fallbackMaxLineBytes = 0u;
    bool fallbackPureAscii = true;
    TextSourceView source{};
    if (extData && extData->document != nullptr) {
        source = TextSourceView::FromDocument(*extData->document);
    } else if (extData && !extData->precomputedLogicalLines.empty()) {
        source = TextSourceView::FromFlat(
            text, extData->precomputedLogicalLines, extData->cachedMaxLineBytes, extData->cachedPureAsciiNoTabs);
    } else {
        SplitLogicalLines(text, fallbackLogicalLines, &fallbackMaxLineBytes, &fallbackPureAscii);
        source = TextSourceView::FromFlat(text, fallbackLogicalLines, fallbackMaxLineBytes, fallbackPureAscii);
    }

    const keel::usize totalLogicalLines = source.LineCount();

    keel::Array<FoldRegion> fallbackFoldRegions;
    keel::ArrayRef<FoldRegion> foldRegions =
        (extData && opts.codeFolding) ? extData->precomputedFoldRegions : keel::ArrayRef<FoldRegion>{};
    if (opts.codeFolding && !extData) {
        ComputeFoldRegions(source, opts.syntax.has_value() ? &*opts.syntax : nullptr, fallbackFoldRegions);
        foldRegions = fallbackFoldRegions;
    }

    TextViewProjection proj{};
    keel::Array<VisualLine> fallbackExactBuf;
    keel::Array<keel::usize> fallbackSmallPrefix;
    keel::Array<keel::usize> fallbackChunkPrefix;
    const EditorLayoutRects L = ComputeEditorLayout(
        bounds, source, opts, size, &canvas, theme,
        effZoom, effScrollX, effScrollY,
        extData ? extData->copyFeedbackActive : false,
        searchOpen, replaceOpen, gotoOpen,
        foldRegions, foldedLines, 0u, nullptr, extData,
        proj, fallbackExactBuf, fallbackSmallPrefix, fallbackChunkPrefix,
        extData ? extData->debugToolbarRatio : -1.0f
    );

    const Rect editorBox = L.editorBox;
    if (editorBox.w <= 0.0f || editorBox.h <= 0.0f) return;

    // 1. Editor frame via FieldFrame
    const keel::f32 r = theme.ResolveRadius(opts.border.radius, size);
    FieldFrame::Draw(canvas, theme, FieldFrameParams{
        .rect = editorBox,
        .style = opts.style,
        .status = opts.status,
        .background = opts.background,
        .border = opts.border,
        .defaultRadius = r,
        .defaultStrokeWidth = 1.0f,
        .defaultBgOverride = editorBgCol,
        .hovered = hovered,
        .focused = focused,
        .disabled = disabled
    });

    const Rect innerBounds = L.innerBounds;
    if (innerBounds.w <= 2.0f || innerBounds.h <= 2.0f) return;

    // 2. Reuse precomputed search matches & overview ruler buckets (0 work in Render!)
    keel::Array<SearchMatch> fallbackSearchMatches;
    keel::StaticArray<keel::u8, TextAreaState::kSearchRulerBuckets> fallbackRulerBuckets{};
    bool searchCapped = extData ? extData->searchMatchesCapped : false;
    keel::ArrayRef<SearchMatch> searchMatches{};
    keel::ArrayRef<keel::u8> searchRulerBuckets{};
    if (searchOpen && extData && !extData->searchQuery.empty()) {
        if (!extData->precomputedSearchMatches.empty() || extData->searchMatchCount == 0u) {
            searchMatches = extData->precomputedSearchMatches;
            searchRulerBuckets = extData->searchRulerBuckets;
        } else {
            FindAllSearchMatches(
                source, extData->searchQuery, extData->matchCase, extData->wholeWord,
                fallbackSearchMatches, fallbackRulerBuckets.as_mut_ref(), searchCapped, kMaxStoredSearchMatches);
            searchMatches = fallbackSearchMatches;
            searchRulerBuckets = fallbackRulerBuckets.as_ref();
        }
    }

    const keel::usize safeCursor = keel::min(cursor, source.ByteLength());
    keel::String activeWordScratch;
    keel::StringRef activeWord{};
    if (opts.highlightOccurrences && focused && !disabled && !searchOpen) {
        if (hasSelection && (selEnd - selStart) <= 48u) {
            const keel::StringRef s = source.ReadSliceRef(selStart, selEnd - selStart, activeWordScratch);
            bool validIdent = !s.empty() && IsIdentStart(s[0]);
            for (char ch : s) {
                if (!IsIdentChar(ch)) { validIdent = false; break; }
            }
            if (validIdent) activeWord = s;
        } else if (!hasSelection) {
            activeWord = ExtractWordAtOffset(source, safeCursor, activeWordScratch);
        }
    }

    keel::usize matchBracketOpen = keel::StringRef::npos;
    keel::usize matchBracketClose = keel::StringRef::npos;
    const bool hasBracketPair = opts.matchBrackets && focused && !disabled &&
        FindMatchingBracketPair(source, safeCursor, matchBracketOpen, matchBracketClose);

    canvas.PushClip(innerBounds);

    // 3. Gutter background
    if (L.gutterWidth > 0.0f) {
        canvas.DrawRect(L.gutterRect, editorGutterBg);
        canvas.DrawLine(
            Vec2{L.gutterRect.Right(), L.gutterRect.y},
            Vec2{L.gutterRect.Right(), L.gutterRect.Bottom()},
            editorBorderSubtle,
            1.0f
        );
    }

    const Rect textViewport = L.textViewport;
    const keel::f64 clampedScrollY = keel::clamp(effScrollY, 0.0, L.maxScrollY);
    const keel::f64 clampedScrollX = keel::clamp(effScrollX, 0.0, L.maxScrollX);

    const keel::usize activeVisRow = proj.OffsetToVisualRow(safeCursor);
    const TextLineCol activeLc = source.OffsetToLineCol(safeCursor);
    const keel::usize activeLogicalLine = activeLc.line;

    // 4. Placeholder when empty
    if (source.Empty() && !opts.placeholder.empty() && !focused) {
        TextUtil::DrawUiText(
            canvas,
            opts.placeholder,
            Rect(textViewport.x, textViewport.y, textViewport.w, L.lineHeight),
            L.fontSize,
            L.lineHeight,
            editorTextMuted,
            0.0f,
            0.5f,
            &theme
        );
    }

    // 5. 2D Virtualized visible rows × visible columns loop
    const keel::usize totalVisLines = L.totalVisualRows;
    const keel::f64 lineHD = keel::max(1.0, keel::as<keel::f64>(L.lineHeight));
    const keel::usize firstVisLine = keel::as<keel::usize>(keel::max(0.0, keel::floor(clampedScrollY / lineHD)));
    const keel::usize maxVisCount = keel::as<keel::usize>(keel::ceil(keel::as<keel::f64>(textViewport.h) / lineHD)) + 2u;
    const keel::usize lastVisLine = keel::min(totalVisLines, firstVisLine + maxVisCount);

    keel::Array<TextSpan> syntaxSpans;
    syntaxSpans.reserve(32u);
    keel::usize cachedSyntaxLine = keel::as<keel::usize>(-1);

    const bool useCallbackSyntax = (opts.syntaxHighlighter != nullptr);
    const bool useConfigSyntax = (!useCallbackSyntax && opts.syntax.has_value() && opts.syntax->enabled);
    const bool anySyntax = (useCallbackSyntax || useConfigSyntax) && !disabled;

    // Use pre-resolved checkpointed multiline block comment state (O(1) on 1M+ lines!)
    bool inBlockComment = false;
    if (useConfigSyntax && !opts.syntax->blockCommentStart.empty() && !opts.syntax->blockCommentEnd.empty() && firstVisLine < totalVisLines) {
        const keel::usize targetLog = proj.VisualRowAt(firstVisLine).logicalLine;
        if (extData && extData->hasInitialBlockComment && extData->initialBlockCommentLine == targetLog) {
            inBlockComment = extData->initialInBlockComment;
        } else {
            inBlockComment = ResolveBlockCommentStateAtLine(source, *opts.syntax, targetLog, 0u, nullptr);
        }
    }

    Rect hoveredWordScreenRect{};
    keel::String visSliceScratch;
    keel::String fullLineScratch;
    keel::String leadScratch;
    keel::String colScratch;

    for (keel::usize vIdx = firstVisLine; vIdx < lastVisLine; ++vIdx) {
        const VisualLine vl = proj.VisualRowAt(vIdx);
        // f64 subtraction BEFORE f32 cast guarantees sub-pixel accuracy even at line 10,000,000!
        const keel::f32 lineY = textViewport.y + keel::as<keel::f32>(keel::as<keel::f64>(vIdx) * lineHD - clampedScrollY);
        if (lineY + L.lineHeight < innerBounds.y || lineY > innerBounds.Bottom()) {
            continue;
        }

        const bool isExecLine = (opts.executionLine == vl.logicalLine + 1u);

        if (isExecLine) {
            canvas.DrawRect(
                Rect(innerBounds.x + L.gutterWidth, lineY, innerBounds.w - L.gutterWidth - L.minimapWidth, L.lineHeight),
                Color(245, 158, 11, 42)
            );
            canvas.DrawRect(
                Rect(innerBounds.x + L.gutterWidth, lineY, 2.5f, L.lineHeight),
                Color(245, 158, 11, 230)
            );
        } else if (focused && !disabled && vIdx == activeVisRow && !hasSelection) {
            canvas.DrawRect(
                Rect(innerBounds.x + L.gutterWidth, lineY, innerBounds.w - L.gutterWidth - L.minimapWidth, L.lineHeight),
                synPal.lineHighlight
            );
        }

        const keel::f32 textY = lineY + L.metrics.textOffsetY;

        // Gutter contents: Breakpoint circle, Execution arrow, Line number, Folding chevron
        if (L.gutterWidth > 0.0f && vl.isFirstSubLine) {
            const keel::f32 centerY = lineY + L.lineHeight * 0.5f;

            const bool hasBp = ContainsSizeT(breakpoints, vl.logicalLine + 1u);
            const bool hovBp = extData && (extData->hoveredGutterLine == keel::as<keel::i32>(vl.logicalLine));
            if (hasBp || hovBp) {
                const keel::f32 bpR = keel::min(L.lineHeight * 0.28f, 5.0f * effZoom);
                const keel::f32 bpX = innerBounds.x + keel::max(bpR + 3.5f, L.bpColWidth * 0.25f);
                const Color bpCol = hasBp ? Color(239, 68, 68, 245) : Color(239, 68, 68, 105);
                canvas.DrawRoundRect(Rect(bpX - bpR, centerY - bpR, bpR * 2.0f, bpR * 2.0f), bpR, bpCol, hasBp ? Color(254, 202, 202, 180) : Color(0, 0, 0, 0), 1.0f);
            }

            if (isExecLine) {
                const keel::f32 ax = innerBounds.x + keel::max(14.5f * effZoom, L.bpColWidth * 0.56f);
                const keel::f32 aw = 7.5f * effZoom;
                const keel::f32 ah = 5.0f * effZoom;
                canvas.DrawTriangle(
                    {ax, centerY - ah},
                    {ax, centerY + ah},
                    {ax + aw, centerY},
                    Color(251, 191, 36, 255),
                    Color(180, 83, 9, 255),
                    1.0f
                );
            }

            if (opts.showLineNumbers) {
                char numBuf[24]{};
                const keel::i32 nLen = std::snprintf(numBuf, sizeof(numBuf), "%zu", vl.logicalLine + 1u);
                if (nLen > 0) {
                    const keel::StringRef numStr(numBuf, keel::as<keel::usize>(nLen));
                    const bool isCurrentLine = focused && !disabled && (vl.logicalLine == activeLogicalLine);
                    const Color numCol = disabled
                        ? pal.textDisabled
                        : (isExecLine ? Color(251, 191, 36) : (isCurrentLine ? editorTextPrimary : editorTextMuted));
                    const keel::f32 numW = MeasureChunkNoTabs(&canvas, numStr, L.fontSize, FontStyle::Monospace);
                    const keel::f32 gutterPadRight = L.metrics.spaceAdvance * 1.15f + L.foldColWidth;
                    const keel::f32 numX = innerBounds.x + keel::max(2.0f, L.gutterWidth - gutterPadRight - numW);
                    canvas.DrawTextStyled(numStr, Vec2{numX, textY}, L.fontSize, numCol, FontStyle::Monospace);
                }
            }

            if (opts.codeFolding) {
                if (FindFoldRegionStartingAt(foldRegions, vl.logicalLine) != nullptr) {
                    const bool isFolded = IsLineFoldedStart(foldedLines, vl.logicalLine);
                    const bool hovFold = extData && (extData->hoveredFoldLine == keel::as<keel::i32>(vl.logicalLine));
                    const Color chevCol = hovFold ? editorTextPrimary : editorTextSecondary;
                    const keel::f32 cx = L.gutterRect.Right() - L.foldColWidth * 0.5f;
                    const keel::f32 icoSz = keel::min(L.foldColWidth * 0.85f, 11.0f);
                    const Rect chevRect(cx - icoSz * 0.5f, centerY - icoSz * 0.5f, icoSz, icoSz);
                    if (hovFold) {
                        canvas.DrawRoundRect(chevRect, 2.5f, editorBorderSubtle);
                    }
                    const Icon chevIco = isFolded ? Icon::ChevronRight() : Icon::ChevronDown();
                    chevIco.Draw(canvas, chevRect, chevCol);
                }
            }
        }

        // Clip and render code/text within textViewport
        canvas.PushClip(Rect(textViewport.x, innerBounds.y, textViewport.w, innerBounds.h));

        const TextLineRange lr = source.LineByteRange(vl.logicalLine);
        const keel::usize vlEndOffset = vl.startOffset + vl.length;
        const HorizontalWindow hw = ComputeHorizontalWindow(
            source, vl, L.metrics, opts.monospace, clampedScrollX, textViewport.w);
        const keel::usize winGlobalStart = vl.startOffset + hw.colByteStart;
        const keel::usize winGlobalEnd = winGlobalStart + hw.colByteLen;
        // f64 horizontal subtraction before f32 cast guarantees exact rendering at col 5,000,000!
        const keel::f32 winStartX = textViewport.x + keel::as<keel::f32>(hw.startXOffset - clampedScrollX);
        const keel::StringRef visSlice = source.ReadSliceRef(winGlobalStart, hw.colByteLen, visSliceScratch);

        auto colInSubToScreenX = [&](keel::usize colInSub) noexcept -> keel::f32 {
            if (hw.fastMonospaceAscii) {
                const keel::f64 xD = keel::as<keel::f64>(colInSub) * keel::as<keel::f64>(L.metrics.monoCharAdvance);
                return textViewport.x + keel::as<keel::f32>(xD - clampedScrollX);
            }
            if (colInSub >= hw.colByteStart && colInSub <= hw.colByteStart + hw.colByteLen) {
                return winStartX + MeasureSegmentWidth(&canvas, visSlice.substr(0u, colInSub - hw.colByteStart), L.fontSize, textStyle, opts.tabSize);
            }
            const keel::f64 xD = MeasureColumnXInVisualRow(source, vl, colInSub, L.metrics, textStyle, &canvas, opts.tabSize, colScratch);
            return textViewport.x + keel::as<keel::f32>(xD - clampedScrollX);
        };

        // Indent guides (bounded read of first <= 256 bytes, never reads 10 MB lines!)
        const keel::usize tabStep = keel::clamp<keel::usize>(opts.tabSize, 1u, 16u);
        if (opts.showIndentGuides && vl.isFirstSubLine && lr.contentBytes > 0u && clampedScrollX < 2048.0) {
            const keel::StringRef lead = source.ReadSliceRef(lr.startByte, keel::min<keel::usize>(lr.contentBytes, 256u), leadScratch);
            keel::usize leadSpaces = 0u;
            for (char ch : lead) {
                if (ch == ' ') ++leadSpaces;
                else if (ch == '\t') leadSpaces += tabStep;
                else break;
            }
            const Color guideCol = hasCustomEditorSurface ? editorBorderSubtle : pal.borderSubtle.WithAlphaF(0.42f);
            for (keel::usize g = tabStep; g < leadSpaces; g += tabStep) {
                const keel::f32 gx = textViewport.x + keel::as<keel::f32>(keel::as<keel::f64>(g) * keel::as<keel::f64>(L.metrics.spaceAdvance) - clampedScrollX);
                if (gx >= textViewport.x && gx <= textViewport.Right()) {
                    canvas.DrawLine({gx, lineY}, {gx, lineY + L.lineHeight}, guideCol, 1.0f);
                }
            }
        }

        // Word occurrence highlights inside visible 2D slice only (O(visibleCols)!)
        if (!activeWord.empty() && visSlice.size() >= activeWord.size()) {
            const keel::usize wLen = activeWord.size();
            for (keel::usize col = 0u; col + wLen <= visSlice.size(); ) {
                if (visSlice.substr(col, wLen) == activeWord) {
                    const keel::usize matchGlobal = winGlobalStart + col;
                    const bool leftOk = (matchGlobal == vl.startOffset) || !IsIdentChar(source.ByteAt(matchGlobal - 1u));
                    const bool rightOk = (matchGlobal + wLen == vlEndOffset) || !IsIdentChar(source.ByteAt(matchGlobal + wLen));
                    if (leftOk && rightOk) {
                        const keel::f32 ox = colInSubToScreenX(hw.colByteStart + col);
                        const keel::f32 ow = hw.fastMonospaceAscii
                            ? (keel::as<keel::f32>(wLen) * L.metrics.monoCharAdvance)
                            : MeasureSegmentWidth(&canvas, activeWord, L.fontSize, textStyle, opts.tabSize);
                        canvas.DrawRoundRect(
                            Rect(ox - 1.0f, lineY + 1.0f, ow + 2.0f, L.lineHeight - 2.0f),
                            2.5f,
                            pal.focusRing.WithAlphaF(0.14f),
                            pal.focusRing.WithAlphaF(0.38f),
                            1.0f
                        );
                        col += wLen;
                        continue;
                    }
                }
                ++col;
            }
        }

        // Search match highlights on this visible window via O(log M) binary search!
        if (!searchMatches.empty() && winGlobalEnd > winGlobalStart) {
            const keel::usize firstM = FindFirstSearchMatchAtOrAfter(searchMatches, winGlobalStart);
            for (keel::usize mi = firstM; mi < searchMatches.size(); ++mi) {
                const SearchMatch& m = searchMatches[mi];
                if (m.startOffset >= winGlobalEnd) break;
                if (m.endOffset <= winGlobalStart) continue;
                const keel::usize s0 = keel::max(m.startOffset, winGlobalStart) - vl.startOffset;
                const keel::usize s1 = keel::min(m.endOffset, winGlobalEnd) - vl.startOffset;
                const keel::f32 mx = colInSubToScreenX(s0);
                const keel::f32 mxEnd = colInSubToScreenX(s1);
                const keel::f32 mw = keel::max(2.0f, mxEnd - mx);
                const bool isActiveMatch = (extData && mi == extData->activeMatchIdx);
                const Color mFill = isActiveMatch ? Color(245, 158, 11, 120) : Color(245, 158, 11, 58);
                const Color mStroke = isActiveMatch ? Color(251, 191, 36, 240) : Color(245, 158, 11, 140);
                canvas.DrawRoundRect(Rect(mx, lineY + 1.0f, mw, L.lineHeight - 2.0f), 2.5f, mFill, mStroke, 1.0f);
            }
        }

        // Selection highlight rectangle clipped to visible horizontal window
        if (hasSelection && focused && !disabled) {
            const keel::usize s0 = keel::max(selStart, winGlobalStart);
            const keel::usize s1 = keel::min(selEnd, winGlobalEnd);
            const bool fullRowSelectedPastEnd = (selStart <= vl.startOffset && selEnd > vlEndOffset && winGlobalEnd == vlEndOffset);
            if (s1 > s0 || fullRowSelectedPastEnd) {
                const keel::f32 selX0 = colInSubToScreenX(s0 - vl.startOffset);
                keel::f32 selX1 = (s1 > s0) ? colInSubToScreenX(s1 - vl.startOffset) : selX0;
                if (selEnd > vlEndOffset && winGlobalEnd == vlEndOffset) {
                    selX1 += L.metrics.spaceAdvance;
                }
                const keel::f32 selW = selX1 - selX0;
                if (selW > 0.0f) {
                    canvas.DrawRect(Rect(selX0, lineY, selW, L.lineHeight), synPal.selectionBg);
                }
            }
        }

        // Bracket matching boxes
        if (hasBracketPair) {
            const keel::usize bracketOffsets[2] = {matchBracketOpen, matchBracketClose};
            for (keel::usize bOff : bracketOffsets) {
                if (bOff >= winGlobalStart && bOff < winGlobalEnd) {
                    const keel::f32 bx = colInSubToScreenX(bOff - vl.startOffset);
                    const keel::f32 bw = opts.monospace ? L.metrics.monoCharAdvance : L.metrics.spaceAdvance;
                    canvas.DrawRoundRect(
                        Rect(bx - 1.0f, lineY + 1.5f, bw + 2.0f, L.lineHeight - 3.0f),
                        2.0f,
                        pal.focusRing.WithAlphaF(0.18f),
                        pal.focusRing.WithAlphaF(0.75f),
                        1.0f
                    );
                }
            }
        }

        // Track screen position of hoveredWord for DataTip overlay
        if (extData && !extData->hoveredWord.empty() && vl.logicalLine == extData->hoveredWordLine && vl.isFirstSubLine) {
            if (extData->hoveredWordCol >= hw.colByteStart && extData->hoveredWordCol <= hw.colByteStart + hw.colByteLen) {
                const keel::f32 wx = colInSubToScreenX(extData->hoveredWordCol);
                const keel::f32 ww = MeasureSegmentWidth(&canvas, extData->hoveredWord, L.fontSize, textStyle, opts.tabSize);
                hoveredWordScreenRect = Rect(wx, lineY, ww, L.lineHeight);
            }
        }

        // Render text or syntax spans across visible slice
        const Color wsCol = editorTextMuted.WithAlphaF(0.55f);
        const keel::f32 wsCenterY = lineY + L.lineHeight * 0.52f;
        const keel::f32 wsDotR = keel::max(1.1f, 1.35f * effZoom);
        auto drawSpaceDots = [&](keel::f32 baseX, keel::usize count) {
            if (!opts.showWhitespace || count == 0u) return;
            for (keel::usize i = 0u; i < count; ++i) {
                const keel::f32 dx = baseX + (keel::as<keel::f32>(i) + 0.5f) * L.metrics.spaceAdvance;
                if (dx >= textViewport.x - 2.0f && dx <= textViewport.Right() + 2.0f) {
                    canvas.DrawRoundRect(Rect(dx - wsDotR, wsCenterY - wsDotR, wsDotR * 2.0f, wsDotR * 2.0f), wsDotR, wsCol);
                }
            }
        };
        auto drawTabArrows = [&](keel::f32 baseX, keel::usize count) {
            if (!opts.showWhitespace || count == 0u) return;
            const keel::f32 strokeW = keel::max(1.0f, 1.15f * effZoom);
            const keel::f32 headSz = keel::min(L.lineHeight * 0.20f, L.metrics.spaceAdvance * 0.45f);
            for (keel::usize i = 0u; i < count; ++i) {
                const keel::f32 tx0 = baseX + keel::as<keel::f32>(i) * L.metrics.tabAdvance + L.metrics.spaceAdvance * 0.35f;
                const keel::f32 tx1 = baseX + keel::as<keel::f32>(i + 1u) * L.metrics.tabAdvance - L.metrics.spaceAdvance * 0.35f;
                if (tx1 >= textViewport.x - 2.0f && tx0 <= textViewport.Right() + 2.0f && tx1 > tx0) {
                    canvas.DrawLine({tx0, wsCenterY}, {tx1, wsCenterY}, wsCol, strokeW);
                    canvas.DrawLine({tx1 - headSz, wsCenterY - headSz}, {tx1, wsCenterY}, wsCol, strokeW);
                    canvas.DrawLine({tx1 - headSz, wsCenterY + headSz}, {tx1, wsCenterY}, wsCol, strokeW);
                }
            }
        };

        keel::f32 curX = winStartX;
        if (!visSlice.empty()) {
            auto drawPiece = [&](keel::StringRef piece, Color col) {
                keel::usize p = 0u;
                while (p < piece.size()) {
                    if (piece[p] == '\t') {
                        keel::usize tabEnd = p + 1u;
                        while (tabEnd < piece.size() && piece[tabEnd] == '\t') ++tabEnd;
                        const keel::usize tabCount = tabEnd - p;
                        drawTabArrows(curX, tabCount);
                        curX += keel::as<keel::f32>(tabCount) * L.metrics.tabAdvance;
                        p = tabEnd;
                    } else {
                        keel::usize nextTab = piece.find('\t', p);
                        if (nextTab == keel::StringRef::npos) nextTab = piece.size();
                        const keel::StringRef seg = piece.substr(p, nextTab - p);

                        keel::usize firstNonSpace = 0u;
                        while (firstNonSpace < seg.size() && seg[firstNonSpace] == ' ') ++firstNonSpace;
                        if (firstNonSpace > 0u) {
                            drawSpaceDots(curX, firstNonSpace);
                            curX += keel::as<keel::f32>(firstNonSpace) * L.metrics.spaceAdvance;
                        }

                        if (firstNonSpace < seg.size()) {
                            keel::usize lastNonSpace = seg.size();
                            while (lastNonSpace > firstNonSpace && seg[lastNonSpace - 1u] == ' ') --lastNonSpace;
                            const keel::StringRef body = seg.substr(firstNonSpace, lastNonSpace - firstNonSpace);
                            const keel::f32 bodyW = hw.fastMonospaceAscii
                                ? (keel::as<keel::f32>(body.size()) * L.metrics.monoCharAdvance)
                                : MeasureChunkNoTabs(&canvas, body, L.fontSize, textStyle);
                            if (curX + bodyW >= textViewport.x - 4.0f && curX <= textViewport.Right() + 4.0f) {
                                canvas.DrawTextStyled(body, Vec2{curX, textY}, L.fontSize, col, textStyle);
                                if (opts.showWhitespace) {
                                    for (keel::usize bi = 0u; bi < body.size(); ++bi) {
                                        if (body[bi] == ' ') {
                                            const keel::f32 spX = curX + (hw.fastMonospaceAscii
                                                ? (keel::as<keel::f32>(bi) * L.metrics.monoCharAdvance)
                                                : MeasureChunkNoTabs(&canvas, body.substr(0u, bi), L.fontSize, textStyle))
                                                + L.metrics.spaceAdvance * 0.5f;
                                            canvas.DrawRoundRect(Rect(spX - wsDotR, wsCenterY - wsDotR, wsDotR * 2.0f, wsDotR * 2.0f), wsDotR, wsCol);
                                        }
                                    }
                                }
                            }
                            curX += bodyW;

                            const keel::usize trailingSpaces = seg.size() - lastNonSpace;
                            if (trailingSpaces > 0u) {
                                drawSpaceDots(curX, trailingSpaces);
                                curX += keel::as<keel::f32>(trailingSpaces) * L.metrics.spaceAdvance;
                            }
                        }
                        p = nextTab;
                    }
                }
            };

            // Guard against full-line tokenization on very long lines (> 20 KB, matching VS Code)
            const bool allowSyntaxOnLine = anySyntax && (lr.contentBytes <= kMaxSyntaxLineBytes);
            if (allowSyntaxOnLine) {
                const keel::StringRef fullLineStr = source.ReadSliceRef(lr.startByte, lr.contentBytes, fullLineScratch);
                if (cachedSyntaxLine != vl.logicalLine) {
                    syntaxSpans.clear();
                    if (useCallbackSyntax) {
                        opts.syntaxHighlighter(fullLineStr, vl.logicalLine, syntaxSpans, theme);
                    } else if (useConfigSyntax) {
                        HighlightLine(
                            fullLineStr,
                            vl.logicalLine,
                            *opts.syntax,
                            syntaxSpans,
                            theme,
                            opts.syntaxPalette.has_value() ? &*opts.syntaxPalette : nullptr,
                            &inBlockComment
                        );
                    }
                    cachedSyntaxLine = vl.logicalLine;
                }

                const keel::usize visStartInLine = vl.startCol + hw.colByteStart;
                const keel::usize visEndInLine = visStartInLine + hw.colByteLen;
                keel::usize curCol = visStartInLine;

                for (const TextSpan& sp : syntaxSpans) {
                    const keel::usize spStart = sp.startCol;
                    const keel::usize spEnd = sp.startCol + sp.length;
                    if (spEnd <= visStartInLine) continue;
                    if (spStart >= visEndInLine) break;

                    const keel::usize clipStart = keel::max(spStart, visStartInLine);
                    const keel::usize clipEnd = keel::min(spEnd, visEndInLine);

                    if (clipStart > curCol) {
                        drawPiece(fullLineStr.substr(curCol, clipStart - curCol), synPal.identifier);
                    }
                    if (clipEnd > clipStart) {
                        drawPiece(fullLineStr.substr(clipStart, clipEnd - clipStart), sp.color);
                        curCol = clipEnd;
                    }
                }
                if (curCol < visEndInLine) {
                    drawPiece(fullLineStr.substr(curCol, visEndInLine - curCol), synPal.identifier);
                }
            } else {
                const Color textCol = disabled ? pal.textDisabled : editorTextPrimary;
                drawPiece(visSlice, textCol);
            }
        }

        // Visible EOL return marker (↵)
        const bool winAtLineEnd = (vl.startCol + hw.colByteStart + hw.colByteLen >= lr.contentBytes);
        if (opts.showWhitespace && winAtLineEnd && (vl.logicalLine + 1u < totalLogicalLines)) {
            const keel::f32 eolX = curX + L.metrics.spaceAdvance * 0.25f;
            const keel::f32 eolW = L.metrics.spaceAdvance * 0.62f;
            const keel::f32 eolTop = wsCenterY - L.lineHeight * 0.18f;
            const keel::f32 eolBot = wsCenterY + L.lineHeight * 0.12f;
            const keel::f32 headSz = keel::min(L.lineHeight * 0.14f, L.metrics.spaceAdvance * 0.30f);
            const keel::f32 eolStrokeW = keel::max(1.0f, 1.1f * effZoom);
            if (eolX + eolW >= textViewport.x - 2.0f && eolX <= textViewport.Right() + 2.0f) {
                canvas.DrawLine({eolX + eolW, eolTop}, {eolX + eolW, eolBot}, wsCol, eolStrokeW);
                canvas.DrawLine({eolX + eolW, eolBot}, {eolX, eolBot}, wsCol, eolStrokeW);
                canvas.DrawLine({eolX + headSz, eolBot - headSz}, {eolX, eolBot}, wsCol, eolStrokeW);
                canvas.DrawLine({eolX + headSz, eolBot + headSz}, {eolX, eolBot}, wsCol, eolStrokeW);
            }
        }

        // Folded block summary pill "[...]"
        const bool isFoldedStart = opts.codeFolding && vl.isFirstSubLine && IsLineFoldedStart(foldedLines, vl.logicalLine);
        if (isFoldedStart) {
            const keel::f32 pillX = curX + L.metrics.spaceAdvance * 0.8f;
            const keel::f32 pillW = L.metrics.spaceAdvance * 4.2f;
            const keel::f32 pillH = L.lineHeight - 4.0f;
            const Rect pillRect(pillX, lineY + 2.0f, pillW, pillH);
            const keel::f32 foldRad = theme.Painter().GetBadgeRadius(theme, pillH, 3.0f, false);
            canvas.DrawRoundRect(pillRect, foldRad, editorGutterBg, editorBorderSubtle, 1.0f);
            canvas.DrawTextBox("...", pillRect, L.fontSize * 0.9f, editorTextSecondary, 0.5f, 0.45f);
            curX = pillRect.Right();
        }

        // Inline Debugger Variable Readouts
        const bool isLastSubLine = (vl.startCol + vl.length >= lr.contentBytes) || isFoldedStart;
        if (isLastSubLine && winAtLineEnd && !opts.inlineValues.empty() && lr.contentBytes <= kMaxSyntaxLineBytes) {
            const keel::StringRef cmtPrefix = (opts.syntax.has_value() && !opts.syntax->lineComment.empty())
                ? opts.syntax->lineComment
                : "//";
            const keel::StringRef fullLineStr = source.ReadSliceRef(lr.startByte, lr.contentBytes, fullLineScratch);
            if (!ExtractCodeWithoutLineComment(fullLineStr, cmtPrefix).empty()) {
                for (const CodeInlineValue& iv : opts.inlineValues) {
                    if (iv.line == vl.logicalLine + 1u && !iv.text.empty()) {
                        const keel::f32 ivFs = L.fontSize * 0.88f;
                        const keel::f32 ivW = MeasureChunkNoTabs(&canvas, iv.text, ivFs, FontStyle::Monospace) + 12.0f;
                        keel::f32 ivX = curX + L.metrics.spaceAdvance * 2.2f;
                        bool clampedRight = false;
                        if (opts.wordWrap && ivX + ivW > textViewport.Right() - 4.0f) {
                            ivX = keel::max(textViewport.x + 4.0f, textViewport.Right() - 4.0f - ivW);
                            clampedRight = true;
                        }
                        const keel::f32 ivH = L.lineHeight - 3.0f;
                        const Rect ivRect(ivX, lineY + 1.5f, ivW, ivH);
                        const Color accent = theme.GetTextColor(iv.variant);
                        const keel::f32 ivRad = theme.Painter().GetBadgeRadius(theme, ivH, 3.5f, false);
                        if (clampedRight) {
                            canvas.DrawRoundRect(ivRect, ivRad, editorBgCol.WithAlphaF(0.92f), Color::Transparent(), 0.0f);
                        }
                        canvas.DrawRoundRect(ivRect, ivRad, accent.WithAlphaF(0.14f), accent.WithAlphaF(0.34f), 1.0f);
                        canvas.DrawTextStyled(iv.text, Vec2{ivX + 6.0f, textY + 0.8f}, ivFs, accent.WithAlphaF(0.94f), FontStyle::Monospace);
                        curX = ivRect.Right();
                    }
                }
            }
        }

        // Diagnostic wavy underline squiggles (~~~)
        if (!opts.diagnostics.empty()) {
            for (const CodeDiagnostic& diag : opts.diagnostics) {
                if (diag.line != vl.logicalLine + 1u) continue;
                keel::usize dStart = diag.startCol;
                keel::usize dEnd = (diag.endCol > diag.startCol) ? keel::min<keel::usize>(diag.endCol, lr.contentBytes) : lr.contentBytes;
                if (dStart == 0u && diag.endCol == 0u && lr.contentBytes > 0u) {
                    const keel::StringRef lead = source.ReadSliceRef(lr.startByte, keel::min<keel::usize>(lr.contentBytes, 256u), leadScratch);
                    while (dStart < lead.size() && (lead[dStart] == ' ' || lead[dStart] == '\t')) ++dStart;
                }
                const keel::usize winLineStart = vl.startCol + hw.colByteStart;
                const keel::usize winLineEnd = winLineStart + hw.colByteLen;
                if (dEnd <= winLineStart || dStart >= winLineEnd) continue;
                const keel::usize s0 = keel::max(dStart, winLineStart) - vl.startCol;
                const keel::usize s1 = keel::min(dEnd, winLineEnd) - vl.startCol;
                const keel::f32 x0 = colInSubToScreenX(s0);
                const keel::f32 x1 = colInSubToScreenX(s1);
                const Color sqCol = (diag.severity == DiagnosticSeverity::Error) ? Color(239, 68, 68, 235)
                                  : (diag.severity == DiagnosticSeverity::Warning) ? Color(245, 158, 11, 235)
                                  : Color(56, 189, 248, 220);
                DrawWavyUnderline(canvas, x0, keel::max(x0 + L.metrics.spaceAdvance, x1), lineY + L.lineHeight - 2.0f, sqCol, effZoom);
            }
        }

        // Active cursor caret
        if (focused && cursorVisible && !disabled && vIdx == activeVisRow && (!extData || extData->overlayFocus == 0)) {
            const keel::usize colInSub = (safeCursor >= vl.startOffset)
                ? keel::min(safeCursor - vl.startOffset, vl.length)
                : 0u;
            const keel::f32 caretX = colInSubToScreenX(colInSub);
            if (caretX >= textViewport.x - 2.0f && caretX <= textViewport.Right() + 2.0f) {
                const keel::f32 caretH = keel::max(L.glyphBoxH, L.lineHeight - 2.0f);
                const keel::f32 caretY = lineY + (L.lineHeight - caretH) * 0.5f;
                const keel::f32 reqCaretW = extData ? extData->cursorWidth : 1.8f;
                const keel::f32 caretW = keel::max(1.0f, reqCaretW);
                canvas.DrawRect(Rect(caretX, caretY, caretW, caretH), editorCaret);
            }
        }

        canvas.PopClip();
    }

    // 6. Right-hand Code Minimap (strictly reads <= 88 bytes per visible micro-line!)
    if (L.hasMinimap && L.minimapRect.w > 0.0f) {
        const Color miniBg = (synPal.gutterBg.a > 0)
            ? synPal.gutterBg.WithAlphaF(0.92f)
            : (isEditorDark ? Color(11, 17, 30, 180) : Color(241, 245, 249, 195));
        canvas.DrawRect(L.minimapRect, miniBg);
        canvas.DrawLine(
            {L.minimapRect.x, L.minimapRect.y},
            {L.minimapRect.x, L.minimapRect.Bottom()},
            editorBorderSubtle,
            1.0f
        );

        const keel::f32 microStep = 3.0f;
        const keel::f32 microH = 1.6f;
        const keel::usize maxMiniLines = keel::as<keel::usize>(keel::max(1.0f, L.minimapRect.h / microStep));
        const keel::usize startVis = (totalVisLines > maxMiniLines && L.maxScrollY > 0.0)
            ? keel::as<keel::usize>((clampedScrollY / L.maxScrollY) * keel::as<keel::f64>(totalVisLines - maxMiniLines))
            : 0u;
        const keel::usize endVis = keel::min(totalVisLines, startVis + maxMiniLines);

        const keel::f32 visCountF = keel::as<keel::f32>(keel::max<keel::usize>(1u, lastVisLine - firstVisLine));
        const keel::f32 vpTopY = L.minimapRect.y + keel::as<keel::f32>(firstVisLine > startVis ? firstVisLine - startVis : 0u) * microStep;
        const keel::f32 vpH = keel::min(L.minimapRect.Bottom() - vpTopY, visCountF * microStep);
        const bool miniActive = extData && (extData->minimapHovered || extData->minimapDragging);
        canvas.DrawRect(
            Rect(L.minimapRect.x + 1.0f, vpTopY, L.minimapRect.w - 1.0f, keel::max(10.0f, vpH)),
            pal.focusRing.WithAlphaF(miniActive ? 0.22f : 0.12f)
        );

        const keel::f32 charScale = (L.minimapRect.w - 8.0f) / 90.0f;
        keel::String miniScratch;
        for (keel::usize mv = startVis; mv < endVis; ++mv) {
            const VisualLine vl = proj.VisualRowAt(mv);
            const keel::f32 my = L.minimapRect.y + keel::as<keel::f32>(mv - startVis) * microStep + 2.0f;
            const keel::StringRef ls = source.ReadSliceRef(vl.startOffset, keel::min<keel::usize>(vl.length, 88u), miniScratch);

            if (opts.executionLine == vl.logicalLine + 1u) {
                canvas.DrawRect(Rect(L.minimapRect.x + 1.0f, my - 0.5f, L.minimapRect.w - 2.0f, microStep), Color(251, 191, 36, 160));
            }
            if (ContainsSizeT(breakpoints, vl.logicalLine + 1u)) {
                canvas.DrawRect(Rect(L.minimapRect.x + 2.0f, my, 3.5f, microStep), Color(239, 68, 68, 230));
            }

            keel::usize cIdx = 0u;
            while (cIdx < ls.size()) {
                while (cIdx < ls.size() && (ls[cIdx] == ' ' || ls[cIdx] == '\t')) ++cIdx;
                if (cIdx >= ls.size()) break;
                const keel::usize tokStart = cIdx;
                while (cIdx < ls.size() && ls[cIdx] != ' ' && ls[cIdx] != '\t') ++cIdx;
                const keel::StringRef tok = ls.substr(tokStart, cIdx - tokStart);
                Color mCol = synPal.identifier.WithAlphaF(0.55f);
                if (useConfigSyntax) {
                    if (!opts.syntax->lineComment.empty() && tok.starts_with(opts.syntax->lineComment)) {
                        mCol = synPal.comment.WithAlphaF(0.65f);
                    } else if (ContainsToken(opts.syntax->keywords, tok)) {
                        mCol = synPal.keyword.WithAlphaF(0.75f);
                    } else if (ContainsToken(opts.syntax->types, tok)) {
                        mCol = synPal.type.WithAlphaF(0.75f);
                    } else if (tok.front() == '"' || tok.front() == '\'') {
                        mCol = synPal.string.WithAlphaF(0.70f);
                    }
                }
                const keel::f32 mx = L.minimapRect.x + 4.0f + keel::as<keel::f32>(tokStart) * charScale;
                const keel::f32 mw = keel::max(1.5f, keel::as<keel::f32>(cIdx - tokStart) * charScale);
                canvas.DrawRect(Rect(mx, my, mw, microH), mCol);
            }
        }
    }

    // 7. Vertical & horizontal scrollbars + BUCKETED overview ruler (<= 512 rects, never 100k DrawRects!)
    if (L.needVScroll && L.vTrackRect.h > 0.0f) {
        const bool vHov = extData && extData->vThumbHovered;
        const bool vDrag = extData && extData->vThumbDragging;
        const Rect vBarRect = L.vHasButtons
            ? Rect(L.vStartBtnRect.x, L.vStartBtnRect.y, L.vTrackRect.w, L.vEndBtnRect.Bottom() - L.vStartBtnRect.y)
            : L.vTrackRect;
        ScrollbarPaintParams vp{};
        vp.barRect = vBarRect;
        vp.trackRect = L.vTrackRect;
        vp.thumbRect = L.vThumbRect;
        vp.startButtonRect = L.vStartBtnRect;
        vp.endButtonRect = L.vEndBtnRect;
        vp.horizontal = false;
        vp.hasButtons = L.vHasButtons;
        vp.hovered = vHov || vDrag;
        vp.thumbHovered = vHov;
        vp.dragging = vDrag;
        vp.cornerRadius = sm.cornerRadius;
        theme.Painter().DrawScrollbar(canvas, theme, vp);

        if (!searchRulerBuckets.empty()) {
            const keel::f64 bCountD = keel::as<keel::f64>(searchRulerBuckets.size());
            const keel::f64 trackH = keel::as<keel::f64>(L.vTrackRect.h);
            for (keel::usize bi = 0u; bi < searchRulerBuckets.size(); ++bi) {
                if (searchRulerBuckets[bi] != 0u) {
                    const keel::f32 ty = L.vTrackRect.y + keel::as<keel::f32>((keel::as<keel::f64>(bi) / bCountD) * trackH);
                    canvas.DrawRect(Rect(L.vTrackRect.x, ty, L.vTrackRect.w, 2.0f), Color(245, 158, 11, 210));
                }
            }
        }

        const keel::f64 totalLinesD = keel::as<keel::f64>(keel::max<keel::usize>(1u, totalLogicalLines));
        for (keel::usize bp : breakpoints) {
            if (bp >= 1u && bp <= totalLogicalLines) {
                const keel::f32 ty = L.vTrackRect.y + keel::as<keel::f32>((keel::as<keel::f64>(bp - 1u) / totalLinesD) * keel::as<keel::f64>(L.vTrackRect.h));
                canvas.DrawRect(Rect(L.vTrackRect.x, ty, L.vTrackRect.w, 2.5f), Color(239, 68, 68, 230));
            }
        }
        for (const CodeDiagnostic& d : opts.diagnostics) {
            if (d.line >= 1u && d.line <= totalLogicalLines) {
                const keel::f32 ty = L.vTrackRect.y + keel::as<keel::f32>((keel::as<keel::f64>(d.line - 1u) / totalLinesD) * keel::as<keel::f64>(L.vTrackRect.h));
                const Color dc = (d.severity == DiagnosticSeverity::Error) ? Color(239, 68, 68, 230) : Color(245, 158, 11, 220);
                canvas.DrawRect(Rect(L.vTrackRect.x, ty, L.vTrackRect.w, 2.5f), dc);
            }
        }
    }

    if (L.needHScroll && L.hTrackRect.w > 0.0f) {
        const bool hHov = extData && extData->hThumbHovered;
        const bool hDrag = extData && extData->hThumbDragging;
        const Rect hBarRect = L.hHasButtons
            ? Rect(L.hStartBtnRect.x, L.hStartBtnRect.y, L.hEndBtnRect.Right() - L.hStartBtnRect.x, L.hTrackRect.h)
            : L.hTrackRect;
        ScrollbarPaintParams hp{};
        hp.barRect = hBarRect;
        hp.trackRect = L.hTrackRect;
        hp.thumbRect = L.hThumbRect;
        hp.startButtonRect = L.hStartBtnRect;
        hp.endButtonRect = L.hEndBtnRect;
        hp.horizontal = true;
        hp.hasButtons = L.hHasButtons;
        hp.hovered = hHov || hDrag;
        hp.thumbHovered = hHov;
        hp.dragging = hDrag;
        hp.cornerRadius = sm.cornerRadius;
        theme.Painter().DrawScrollbar(canvas, theme, hp);
    }

    if (L.needVScroll && L.needHScroll) {
        const Rect cornerRect(
            keel::floor(L.innerBounds.Right() - sm.trackWidth - sm.margin),
            keel::floor(L.innerBounds.Bottom() - sm.trackWidth - sm.margin),
            sm.trackWidth,
            sm.trackWidth
        );
        theme.Painter().DrawScrollbarCorner(canvas, theme, cornerRect);
    }

    const UiSize tbSize = L.toolbarSize;
    const ComponentMetrics& tbCm = theme.GetMetrics(tbSize);
    const keel::f32 badgeFs = tbCm.companionFontSize;
    const keel::f32 btnRad = tbCm.cornerRadius;
    const keel::f32 overlayRad = (btnRad > 0.01f) ? (btnRad + 2.0f) : 0.0f;

    // 8. Debug Hover DataTip Card
    if (extData && !extData->hoveredWord.empty() && hoveredWordScreenRect.w > 0.0f && !opts.debugVariables.empty()) {
        for (const CodeDebugVar& dv : opts.debugVariables) {
            if (dv.name == extData->hoveredWord) {
                const keel::f32 tipFs = theme.GetFontSize(TextRole::LabelSmall, size);
                char headerBuf[256];
                const keel::StringRef headerStr = dv.type.empty()
                    ? dv.name
                    : keel::format_buf(headerBuf, "%s : %s", dv.name, dv.type);
                const keel::f32 w1 = MeasureChunkNoTabs(&canvas, headerStr, tipFs, FontStyle::Monospace);
                const keel::f32 w2 = MeasureChunkNoTabs(&canvas, dv.value, tipFs, FontStyle::Monospace);
                const keel::f32 cardW = keel::max(w1, w2) + 18.0f;
                const keel::f32 cardH = 38.0f;
                keel::f32 cardX = keel::clamp(hoveredWordScreenRect.x, textViewport.x + 4.0f, keel::max(textViewport.x + 4.0f, textViewport.Right() - cardW - 4.0f));
                keel::f32 cardY = hoveredWordScreenRect.y - cardH - 4.0f;
                if (cardY < innerBounds.y + 4.0f) {
                    cardY = hoveredWordScreenRect.Bottom() + 4.0f;
                }
                const Rect cardRect(cardX, cardY, cardW, cardH);
                theme.Painter().DrawSurface(canvas, theme, SurfacePaintParams{
                    .rect = cardRect,
                    .role = SurfaceRole::Popup,
                    .radius = (overlayRad > 0.01f) ? keel::min(5.0f, overlayRad) : 0.0f,
                    .bg = pal.surfaceElevated,
                    .border = pal.focusRing,
                    .borderWidth = 1.0f,
                    .elevation = Elevation::Sm
                });
                canvas.DrawTextStyled(headerStr, {cardX + 8.0f, cardY + 4.0f}, tipFs, synPal.type, FontStyle::Monospace);
                canvas.DrawTextStyled(dv.value, {cardX + 8.0f, cardY + 20.0f}, tipFs, pal.textPrimary, FontStyle::Monospace);
                break;
            }
        }
    }

    // 9. Floating Debug Toolbar
    if (opts.showDebugToolbar && L.debugToolbar.panelRect.w > 0.0f) {
        const auto& DT = L.debugToolbar;
        const bool gripDragging = extData && extData->debugToolbarDragging;
        const bool gripHovered = extData && (extData->hoveredDebugBtn == 98 || gripDragging);
        const Color dtBorderCol = gripDragging ? pal.focusRing : (gripHovered ? pal.textSecondary : pal.borderHover);
        theme.Painter().DrawSurface(canvas, theme, SurfacePaintParams{
            .rect = DT.panelRect,
            .role = SurfaceRole::Popup,
            .radius = overlayRad,
            .bg = pal.surfaceElevated,
            .border = dtBorderCol,
            .borderWidth = gripDragging ? 1.5f : 1.0f,
            .explicitBorder = gripDragging,
            .elevation = Elevation::Sm
        });

        const keel::f32 gripSlotW = DT.btnRects[0].x - DT.panelRect.x;
        const keel::f32 dotSz = keel::max(2.0f, tbCm.iconSize * 0.14f);
        const keel::f32 dotRad = (btnRad > 0.01f) ? (dotSz * 0.5f) : 0.0f;
        const keel::f32 dotStepY = tbCm.iconSize * 0.32f;
        const keel::f32 dotStepX = tbCm.iconSize * 0.28f;
        const keel::f32 gripX = DT.panelRect.x + (gripSlotW - dotStepX - dotSz) * 0.5f;
        const keel::f32 gripCy = DT.panelRect.y + DT.panelRect.h * 0.5f;
        const Color gripCol = gripDragging ? pal.focusRing : (gripHovered ? pal.textPrimary : pal.textMuted.WithAlphaF(0.75f));
        for (keel::i32 rowIdx = -1; rowIdx <= 1; ++rowIdx) {
            const keel::f32 dy = gripCy + keel::as<keel::f32>(rowIdx) * dotStepY - dotSz * 0.5f;
            canvas.DrawRoundRect(Rect(gripX, dy, dotSz, dotSz), dotRad, gripCol);
            canvas.DrawRoundRect(Rect(gripX + dotStepX, dy, dotSz, dotSz), dotRad, gripCol);
        }

        const Rect sepRect(
            DT.btnRects[3].Right(),
            DT.panelRect.y,
            keel::max(1.0f, DT.btnRects[4].x - DT.btnRects[3].Right()),
            DT.panelRect.h
        );
        detail::Toolbar::RenderSeparator(canvas, sepRect, pal.borderSubtle, theme);

        static constexpr keel::StaticArray<IconId, 6> kDbgIcons = {
            IconId::DebugContinue, IconId::StepOver, IconId::StepInto,
            IconId::StepOut, IconId::Refresh, IconId::Stop
        };
        const keel::StaticArray<Color, 6> dbgColors = {
            Color(34, 197, 94), Color(56, 189, 248), Color(56, 189, 248),
            Color(56, 189, 248), Color(34, 197, 94), Color(239, 68, 68)
        };
        for (keel::i32 i = 0; i < 6; ++i) {
            const Rect br = DT.btnRects[keel::as<keel::usize>(i)];
            const bool bHov = extData && (extData->hoveredDebugBtn == i);
            const Color baseCol = dbgColors[keel::as<keel::usize>(i)];

            ButtonOptions bOpts{};
            bOpts.icon = Icon(kDbgIcons[keel::as<keel::usize>(i)]);
            bOpts.square = true;
            bOpts.style = ButtonStyle::Ghost;
            bOpts.iconColor = bHov ? baseCol : baseCol.WithAlphaF(0.95f);
            bOpts.width = SizeMode::Stretch;
            detail::Button::Render(canvas, br, bOpts, tbSize, bHov, false, disabled, theme, false);
        }
    }

    // 10. Floating Find & Replace / Go to Line Overlay
    if ((searchOpen || gotoOpen) && L.searchOverlay.panelRect.w > 0.0f && extData) {
        const auto& S = L.searchOverlay;
        theme.Painter().DrawSurface(canvas, theme, SurfacePaintParams{
            .rect = S.panelRect,
            .role = SurfaceRole::Popup,
            .radius = overlayRad,
            .bg = pal.surfaceElevated,
            .border = pal.borderHover,
            .borderWidth = 1.0f,
            .elevation = Elevation::Sm
        });

        auto drawOverlayInput = [&](Rect rInput, bool isFocused) {
            FieldFrame::Draw(canvas, theme, FieldFrameParams{
                .rect = rInput,
                .defaultRadius = btnRad,
                .defaultStrokeWidth = 1.0f,
                .focused = isFocused,
                .disabled = disabled
            });
        };

        const keel::i32 hovBtn = extData->hoveredOverlayBtn;
        auto renderSquareToolBtn = [&](Rect rBtn, IconId iconId, bool activeToggle, keel::i32 btnIdx) {
            if (rBtn.w <= 0.0f) return;
            ButtonOptions bOpts{};
            bOpts.icon = Icon(iconId);
            bOpts.square = true;
            bOpts.style = activeToggle ? ButtonStyle::Solid : ButtonStyle::Ghost;
            bOpts.variant = activeToggle ? UiVariant::Primary : UiVariant::Default;
            bOpts.active = activeToggle;
            bOpts.width = SizeMode::Stretch;
            detail::Button::Render(canvas, rBtn, bOpts, tbSize, hovBtn == btnIdx, false, disabled, theme, false);
        };

        const keel::f32 inputPadX = keel::max(6.0f, tbCm.paddingX * 0.55f);

        if (searchOpen) {
            renderSquareToolBtn(S.toggleReplaceBtn, replaceOpen ? IconId::ChevronDown : IconId::ChevronRight, replaceOpen, 0);

            const bool findFocused = (extData->overlayFocus == 1);
            drawOverlayInput(S.findInputRect, findFocused);
            const Rect findTextRect(S.findInputRect.x + inputPadX, S.findInputRect.y, keel::max(10.0f, S.caseBtnRect.x - S.findInputRect.x - inputPadX * 1.3f), S.findInputRect.h);
            if (extData->searchQuery.empty() && !findFocused) {
                canvas.DrawTextBox("Find...", findTextRect, badgeFs, pal.textMuted, 0.0f, 0.5f);
            } else {
                canvas.DrawTextBox(extData->searchQuery, findTextRect, badgeFs, pal.textPrimary, 0.0f, 0.5f);
                if (findFocused && cursorVisible) {
                    const keel::f32 qW = TextUtil::MeasureWidth(extData->searchQuery, badgeFs, &canvas);
                    const keel::f32 cx = keel::min(findTextRect.Right() - 2.0f, findTextRect.x + qW + 1.0f);
                    const keel::f32 caretPad = keel::max(3.0f, S.findInputRect.h * 0.16f);
                    canvas.DrawRect(Rect(cx, S.findInputRect.y + caretPad, 1.5f, S.findInputRect.h - caretPad * 2.0f), pal.caret);
                }
            }

            {
                ButtonOptions caseOpts{};
                caseOpts.text = "Aa";
                caseOpts.square = true;
                caseOpts.style = extData->matchCase ? ButtonStyle::Solid : ButtonStyle::Ghost;
                caseOpts.variant = extData->matchCase ? UiVariant::Primary : UiVariant::Default;
                caseOpts.active = extData->matchCase;
                caseOpts.width = SizeMode::Stretch;
                detail::Button::Render(canvas, S.caseBtnRect, caseOpts, tbSize, hovBtn == 1, false, disabled, theme, false);
            }
            {
                ButtonOptions wordOpts{};
                wordOpts.text = "ab";
                wordOpts.square = true;
                wordOpts.style = extData->wholeWord ? ButtonStyle::Solid : ButtonStyle::Ghost;
                wordOpts.variant = extData->wholeWord ? UiVariant::Primary : UiVariant::Default;
                wordOpts.active = extData->wholeWord;
                wordOpts.width = SizeMode::Stretch;
                detail::Button::Render(canvas, S.wordBtnRect, wordOpts, tbSize, hovBtn == 2, false, disabled, theme, false);

                const Color fg = extData->wholeWord ? theme.GetVariant(UiVariant::Primary).text : ((hovBtn == 2) ? pal.textPrimary : pal.textSecondary);
                const keel::f32 bx1 = S.wordBtnRect.x + S.wordBtnRect.w * 0.20f;
                const keel::f32 bx2 = S.wordBtnRect.Right() - S.wordBtnRect.w * 0.20f;
                const keel::f32 by = S.wordBtnRect.Bottom() - S.wordBtnRect.h * 0.14f;
                const keel::f32 bh = keel::max(1.8f, S.wordBtnRect.h * 0.10f);
                canvas.DrawLine({bx1, by - bh}, {bx1, by}, fg, 1.2f);
                canvas.DrawLine({bx1, by}, {bx2, by}, fg, 1.2f);
                canvas.DrawLine({bx2, by - bh}, {bx2, by}, fg, 1.2f);
            }

            char cntBuf[32]{};
            if (extData->searchQuery.empty()) {
                std::snprintf(cntBuf, sizeof(cntBuf), "0/0");
            } else if (searchMatches.empty()) {
                std::snprintf(cntBuf, sizeof(cntBuf), "0 res");
            } else if (searchCapped) {
                std::snprintf(cntBuf, sizeof(cntBuf), "%zu/%zu+", extData->activeMatchIdx + 1u, searchMatches.size());
            } else {
                std::snprintf(cntBuf, sizeof(cntBuf), "%zu/%zu", extData->activeMatchIdx + 1u, searchMatches.size());
            }
            const Color cntCol = (!extData->searchQuery.empty() && searchMatches.empty())
                ? theme.GetTextColor(UiVariant::Danger)
                : pal.textSecondary;
            canvas.DrawTextBox(cntBuf, S.countRect, badgeFs, cntCol, 0.5f, 0.5f);

            renderSquareToolBtn(S.prevBtnRect, IconId::ArrowUp, false, 3);
            renderSquareToolBtn(S.nextBtnRect, IconId::ArrowDown, false, 4);
            renderSquareToolBtn(S.closeBtnRect, IconId::Close, false, 5);

            if (replaceOpen && S.replaceInputRect.w > 0.0f) {
                const bool repFocused = (extData->overlayFocus == 2);
                drawOverlayInput(S.replaceInputRect, repFocused);
                const Rect repTextRect(S.replaceInputRect.x + inputPadX, S.replaceInputRect.y, S.replaceInputRect.w - inputPadX * 2.0f, S.replaceInputRect.h);
                if (extData->replaceQuery.empty() && !repFocused) {
                    canvas.DrawTextBox("Replace...", repTextRect, badgeFs, pal.textMuted, 0.0f, 0.5f);
                } else {
                    canvas.DrawTextBox(extData->replaceQuery, repTextRect, badgeFs, pal.textPrimary, 0.0f, 0.5f);
                    if (repFocused && cursorVisible) {
                        const keel::f32 qW = TextUtil::MeasureWidth(extData->replaceQuery, badgeFs, &canvas);
                        const keel::f32 cx = keel::min(repTextRect.Right() - 2.0f, repTextRect.x + qW + 1.0f);
                        const keel::f32 caretPad = keel::max(3.0f, S.replaceInputRect.h * 0.16f);
                        canvas.DrawRect(Rect(cx, S.replaceInputRect.y + caretPad, 1.5f, S.replaceInputRect.h - caretPad * 2.0f), pal.caret);
                    }
                }
                {
                    ButtonOptions rep1Opts{};
                    rep1Opts.text = "1";
                    rep1Opts.icon = Icon::Replace();
                    rep1Opts.style = ButtonStyle::Ghost;
                    rep1Opts.width = SizeMode::Stretch;
                    detail::Button::Render(canvas, S.replaceOneBtnRect, rep1Opts, tbSize, hovBtn == 6, false, disabled, theme, false);
                }
                {
                    ButtonOptions repAllOpts{};
                    repAllOpts.text = "All";
                    repAllOpts.icon = Icon::ReplaceAll();
                    repAllOpts.style = ButtonStyle::Ghost;
                    repAllOpts.width = SizeMode::Stretch;
                    detail::Button::Render(canvas, S.replaceAllBtnRect, repAllOpts, tbSize, hovBtn == 7, false, disabled, theme, false);
                }
            }
        } else if (gotoOpen) {
            const bool gotoFocused = (extData->overlayFocus == 3);
            drawOverlayInput(S.findInputRect, gotoFocused);
            const Rect gTextRect(S.findInputRect.x + inputPadX, S.findInputRect.y, S.findInputRect.w - inputPadX * 2.0f, S.findInputRect.h);
            if (extData->gotoQuery.empty()) {
                char ph[48]{};
                std::snprintf(ph, sizeof(ph), "Go to Line (1..%zu)...", totalLogicalLines);
                canvas.DrawTextBox(ph, gTextRect, badgeFs, pal.textMuted, 0.0f, 0.5f);
            } else {
                canvas.DrawTextBox(extData->gotoQuery, gTextRect, badgeFs, pal.textPrimary, 0.0f, 0.5f);
            }
            renderSquareToolBtn(S.closeBtnRect, IconId::Close, false, 5);
        }
    }

    // 11. Top-right quick overlays: Copy & Zoom %
    if (L.copyBtnRect.w > 0.0f) {
        const bool copyHov = extData && extData->copyHovered;
        const bool copied = extData && extData->copyFeedbackActive;
        ButtonOptions copyOpts{};
        copyOpts.text = copied ? "Copied" : "Copy";
        copyOpts.icon = copied ? Icon::Check() : Icon::Copy();
        copyOpts.variant = copied ? UiVariant::Success : UiVariant::Secondary;
        copyOpts.active = copied;
        copyOpts.width = SizeMode::Stretch;
        detail::Button::Render(canvas, L.copyBtnRect, copyOpts, tbSize, copyHov, false, disabled, theme, false);
    }

    if (L.zoomBtnRect.w > 0.0f) {
        const bool zHov = extData && extData->zoomBadgeHovered;
        char zBuf[16]{};
        ButtonOptions zoomOpts{};
        zoomOpts.text = keel::format_buf(zBuf, "{}%", keel::round_i32(effZoom * 100.0f));
        zoomOpts.variant = UiVariant::Secondary;
        zoomOpts.width = SizeMode::Stretch;
        detail::Button::Render(canvas, L.zoomBtnRect, zoomOpts, tbSize, zHov, false, disabled, theme, false);
    }

    canvas.PopClip();

    // 12. IDE Bottom Status Bar
    if (L.statusBarHeight > 0.0f && L.statusBarRect.w > 0.0f) {
        canvas.PushClip(L.statusBarRect);
        const Color sbBg = (synPal.gutterBg.a > 0)
            ? synPal.gutterBg
            : (isEditorDark ? Color(12, 18, 32, 235) : Color(226, 232, 240, 240));
        canvas.DrawRect(L.statusBarRect, sbBg);
        canvas.DrawLine(
            {L.statusBarRect.x, L.statusBarRect.y},
            {L.statusBarRect.Right(), L.statusBarRect.y},
            editorBorderSubtle,
            1.0f
        );

        const keel::f32 sbFs = theme.GetFontSize(TextRole::LabelSmall, size);
        const keel::usize curLine1 = activeLogicalLine + 1u;
        const keel::usize curCol1 = activeLc.colBytes + 1u;

        keel::usize errCnt = 0u;
        keel::usize warnCnt = 0u;
        for (const CodeDiagnostic& d : opts.diagnostics) {
            if (d.severity == DiagnosticSeverity::Error) ++errCnt;
            else if (d.severity == DiagnosticSeverity::Warning) ++warnCnt;
        }

        keel::f32 sbX = L.statusBarRect.x + 8.0f;
        const keel::f32 sbCy = L.statusBarRect.y + L.statusBarRect.h * 0.5f;
        const keel::f32 sbIcoSz = 11.0f;

        if (opts.executionLine > 0u) {
            const Color pauseCol(251, 191, 36);
            Icon::Pause().Draw(canvas, Rect(sbX, sbCy - sbIcoSz * 0.5f, sbIcoSz, sbIcoSz), pauseCol);
            sbX += sbIcoSz + 4.0f;
            char dbgBuf[48]{};
            std::snprintf(dbgBuf, sizeof(dbgBuf), "PAUSED Ln %zu  |", opts.executionLine);
            const keel::f32 dw = TextUtil::MeasureWidth(dbgBuf, sbFs, &canvas);
            canvas.DrawTextBox(dbgBuf, Rect(sbX, L.statusBarRect.y, dw + 6.0f, L.statusBarRect.h), sbFs, pauseCol, 0.0f, 0.5f);
            sbX += dw + 8.0f;
        }

        const Color errCol = (errCnt > 0u) ? Color(239, 68, 68) : editorTextSecondary;
        Icon::Error().Draw(canvas, Rect(sbX, sbCy - sbIcoSz * 0.5f, sbIcoSz, sbIcoSz), errCol);
        sbX += sbIcoSz + 3.0f;
        char errBuf[16]{};
        std::snprintf(errBuf, sizeof(errBuf), "%zu", errCnt);
        const keel::f32 ew = TextUtil::MeasureWidth(errBuf, sbFs, &canvas);
        canvas.DrawTextBox(errBuf, Rect(sbX, L.statusBarRect.y, ew + 4.0f, L.statusBarRect.h), sbFs, errCol, 0.0f, 0.5f);
        sbX += ew + 8.0f;

        const Color warnCol = (warnCnt > 0u) ? Color(245, 158, 11) : editorTextSecondary;
        Icon::Warning().Draw(canvas, Rect(sbX, sbCy - sbIcoSz * 0.5f, sbIcoSz, sbIcoSz), warnCol);
        sbX += sbIcoSz + 3.0f;
        char warnBuf[16]{};
        std::snprintf(warnBuf, sizeof(warnBuf), "%zu", warnCnt);
        const keel::f32 ww = TextUtil::MeasureWidth(warnBuf, sbFs, &canvas);
        canvas.DrawTextBox(warnBuf, Rect(sbX, L.statusBarRect.y, ww + 4.0f, L.statusBarRect.h), sbFs, warnCol, 0.0f, 0.5f);
        sbX += ww + 10.0f;

        char leftStatus[128]{};
        if (hasSelection) {
            std::snprintf(leftStatus, sizeof(leftStatus), "|  Ln %zu, Col %zu (%zu sel)  |  %zu lines  |  %zu BPs",
                          curLine1, curCol1, selEnd - selStart, totalLogicalLines, breakpoints.size());
        } else {
            std::snprintf(leftStatus, sizeof(leftStatus), "|  Ln %zu, Col %zu  |  %zu lines  |  %zu BPs",
                          curLine1, curCol1, totalLogicalLines, breakpoints.size());
        }
        canvas.DrawTextBox(
            leftStatus,
            Rect(sbX, L.statusBarRect.y, keel::max(10.0f, L.statusBarRect.w * 0.65f - (sbX - L.statusBarRect.x)), L.statusBarRect.h),
            sbFs,
            editorTextSecondary,
            0.0f,
            0.5f
        );

        const keel::StringRef langName = (opts.syntax.has_value() && opts.syntax->enabled)
            ? opts.syntax->languageName
            : "Plain Text";
        char rightStatus[96]{};
        std::snprintf(rightStatus, sizeof(rightStatus), "Spaces: %u  |  UTF-8  |  %d%%  |  %.*s",
                      keel::as<keel::u32>(keel::clamp<keel::i32>(opts.tabSize, 1, 16)),
                      keel::round_i32(effZoom * 100.0f),
                      keel::as<keel::i32>(langName.size()), langName.data());
        canvas.DrawTextBox(
            rightStatus,
            Rect(L.statusBarRect.x + L.statusBarRect.w * 0.45f, L.statusBarRect.y, L.statusBarRect.w * 0.55f - 10.0f, L.statusBarRect.h),
            sbFs,
            editorTextSecondary,
            1.0f,
            0.5f
        );
        canvas.PopClip();
    }
}

} // namespace guikit
